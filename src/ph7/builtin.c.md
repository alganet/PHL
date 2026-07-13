# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3839/4460 lines (86.08%)

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
|  33742 |  303 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  304 | `{` |
|  33747 |  305 | `	int res = 1; /* Assume empty by default */` |
|  33747 |  306 | `	if( nArg > 0 ){` |
|  33745 |  307 | `		res = ph7_value_is_empty(apArg[0]);` |
|  16870 |  308 | `	}` |
|  33747 |  309 | `	ph7_result_bool(pCtx,res);` |
|  33747 |  310 | `	return PH7_OK;` |
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
| 221760 |  353 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  354 | `{` |
|      - |  355 | `	const char *zSource,*zOfft;` |
|      - |  356 | `	int nOfft,nLen,nSrcLen;` |
| 221765 |  357 | `	if( nArg < 2 ){` |
|      - |  358 | `		/* return FALSE */` |
|    ! 0 |  359 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  360 | `		return PH7_OK;` |
|      - |  361 | `	}` |
|      - |  362 | `	/* Extract the target string */` |
| 221765 |  363 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 221765 |  364 | `	if( nSrcLen < 1 ){` |
|      - |  365 | `		/* Empty string,return FALSE */` |
|  12059 |  366 | `		ph7_result_bool(pCtx,0);` |
|  12059 |  367 | `		return PH7_OK;` |
|      - |  368 | `	}` |
| 209711 |  369 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  370 | `	/* Extract the offset */` |
| 209711 |  371 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 209711 |  372 | `	if( nOfft < 0 ){` |
|  32541 |  373 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  32541 |  374 | `		if( zOfft < zSource ){` |
|      - |  375 | `			/* Invalid offset */` |
|      5 |  376 | `			ph7_result_bool(pCtx,0);` |
|      5 |  377 | `			return PH7_OK;` |
|      - |  378 | `		}` |
|  32537 |  379 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  32537 |  380 | `		nOfft = (int)(zOfft-zSource);` |
| 193441 |  381 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  382 | `		/* Invalid offset */` |
|    217 |  383 | `		ph7_result_bool(pCtx,0);` |
|    217 |  384 | `		return PH7_OK;` |
|    ! 0 |  385 | `	}else{` |
| 176963 |  386 | `		zOfft = &zSource[nOfft];` |
| 176963 |  387 | `		nLen = nSrcLen - nOfft;` |
|      - |  388 | `	}` |
| 209495 |  389 | `	if( nArg > 2 ){` |
|      - |  390 | `		/* Extract the length */` |
| 172567 |  391 | `		nLen = ph7_value_to_int(apArg[2]);` |
| 172567 |  392 | `		if( nLen == 0 ){` |
|      - |  393 | `			/* Invalid length,return an empty string */` |
|      5 |  394 | `			ph7_result_string(pCtx,"",0);` |
|      5 |  395 | `			return PH7_OK;` |
| 172563 |  396 | `		}else if( nLen < 0 ){` |
|  32529 |  397 | `			nLen = nSrcLen + nLen - nOfft;` |
|  32529 |  398 | `			if( nLen < 1 ){` |
|      - |  399 | `				/* Invalid  length */` |
|      3 |  400 | `				nLen = nSrcLen - nOfft;` |
|      1 |  401 | `			}` |
|  16262 |  402 | `		}` |
| 172563 |  403 | `		if( nLen + nOfft > nSrcLen ){` |
|      - |  404 | `			/* Invalid length */` |
|   5501 |  405 | `			nLen = nSrcLen - nOfft;` |
|   2748 |  406 | `		}` |
|  86279 |  407 | `	}` |
|      - |  408 | `	/* Return the substring */` |
| 209491 |  409 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 209491 |  410 | `	return PH7_OK;` |
| 110885 |  411 | `}` |
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
|  11568 | 1178 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1179 | `{` |
|  11573 | 1180 | `	int iLen = 0;` |
|  11573 | 1181 | `	if( nArg > 0 ){` |
|  11573 | 1182 | `		ph7_value_to_string(apArg[0],&iLen);` |
|   5784 | 1183 | `	}` |
|      - | 1184 | `	/* String length */` |
|  11573 | 1185 | `	ph7_result_int(pCtx,iLen);` |
|  11573 | 1186 | `	return PH7_OK;` |
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
|     18 | 1226 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1227 | `{` |
|      - | 1228 | `	const char *z1,*z2;` |
|      - | 1229 | `	int res;` |
|      - | 1230 | `	int n;` |
|     19 | 1231 | `	if( nArg < 3 ){` |
|      - | 1232 | `		/* Perform a standard comparison */` |
|    ! 0 | 1233 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 1234 | `	}` |
|      - | 1235 | `	/* Desired comparison length */` |
|     19 | 1236 | `	n  = ph7_value_to_int(apArg[2]);` |
|     19 | 1237 | `	if( n < 0 ){` |
|      - | 1238 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 1239 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1240 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 1241 | `			ph7_function_name(pCtx));` |
|      - | 1242 | `	}` |
|      - | 1243 | `	/* Perform the comparison */` |
|     17 | 1244 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     17 | 1245 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     17 | 1246 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 1247 | `	/* Comparison result */` |
|     17 | 1248 | `	ph7_result_int(pCtx,res);` |
|     17 | 1249 | `	return PH7_OK;` |
|     10 | 1250 | `}` |
|      - | 1251 | `/*` |
|      - | 1252 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 1253 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 1254 | ` * Parameter` |
|      - | 1255 | ` *  str1: The first string` |
|      - | 1256 | ` *  str2: The second string` |
|      - | 1257 | ` * Return` |
|      - | 1258 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1259 | ` *  than str2, and 0 if they are equal.` |
|      - | 1260 | ` */` |
|     14 | 1261 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1262 | `{` |
|      - | 1263 | `	const char *z1,*z2;` |
|      - | 1264 | `	int n1,n2;` |
|      - | 1265 | `	int res;` |
|     15 | 1266 | `	if( nArg < 2 ){` |
|    ! 0 | 1267 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 1268 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 1269 | `		return PH7_OK;` |
|      - | 1270 | `	}` |
|      - | 1271 | `	/* Perform the comparison */` |
|     15 | 1272 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     15 | 1273 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     15 | 1274 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1275 | `	/* Comparison result */` |
|     15 | 1276 | `	ph7_result_int(pCtx,res);` |
|     15 | 1277 | `	return PH7_OK;` |
|      8 | 1278 | `}` |
|      - | 1279 | `/*` |
|      - | 1280 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 1281 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 1282 | ` * Parameter` |
|      - | 1283 | ` *  $str1: The first string` |
|      - | 1284 | ` *  $str2: The second string` |
|      - | 1285 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 1286 | ` * Return` |
|      - | 1287 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1288 | ` *  than str2, and 0 if they are equal.` |
|      - | 1289 | ` */` |
|      8 | 1290 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1291 | `{` |
|      - | 1292 | `	const char *z1,*z2;` |
|      - | 1293 | `	int res;` |
|      - | 1294 | `	int n;` |
|      9 | 1295 | `	if( nArg < 3 ){` |
|      - | 1296 | `		/* Perform a standard comparison */` |
|    ! 0 | 1297 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 1298 | `	}` |
|      - | 1299 | `	/* Desired comparison length */` |
|      9 | 1300 | `	n  = ph7_value_to_int(apArg[2]);` |
|      9 | 1301 | `	if( n < 0 ){` |
|      - | 1302 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 1303 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1304 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 1305 | `			ph7_function_name(pCtx));` |
|      - | 1306 | `	}` |
|      - | 1307 | `	/* Perform the comparison */` |
|      7 | 1308 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|      7 | 1309 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|      7 | 1310 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 1311 | `	/* Comparison result */` |
|      7 | 1312 | `	ph7_result_int(pCtx,res);` |
|      7 | 1313 | `	return PH7_OK;` |
|      5 | 1314 | `}` |
|      - | 1315 | `/*` |
|      - | 1316 | ` * Implode context [i.e: it's private data].` |
|      - | 1317 | ` * A pointer to the following structure is forwarded` |
|      - | 1318 | ` * verbatim to the array walker callback defined below.` |
|      - | 1319 | ` */` |
|      - | 1320 | `struct implode_data {` |
|      - | 1321 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 1322 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 1323 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 1324 | `	int nSeplen;          /* Separator length */` |
|      - | 1325 | `	int bFirst;           /* TRUE if first call */` |
|      - | 1326 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 1327 | `	sxi32 rc;             /* Captured allocation rc; SXERR_MEM => the builtin raises an OOM fatal */` |
|      - | 1328 | `};` |
|      - | 1329 | `/*` |
|      - | 1330 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 1331 | ` * The following routine is invoked for each array entry passed` |
|      - | 1332 | ` * to the implode() function.` |
|      - | 1333 | ` */` |
| 140444 | 1334 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 1335 | `{` |
|  70222 | 1336 | `	SXUNUSED(pKey);` |
| 140449 | 1337 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 1338 | `	const char *zData;` |
|      - | 1339 | `	int nLen;` |
| 140449 | 1340 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 1341 | `		if( pData->nSeplen > 0 ){` |
|      3 | 1342 | `			if( !pData->bFirst ){` |
|      - | 1343 | `				/* append the separator first */` |
|      3 | 1344 | `				if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 1345 | `					pData->rc = SXERR_MEM;` |
|    ! 0 | 1346 | `					return PH7_ABORT;` |
|      - | 1347 | `				}` |
|      2 | 1348 | `			}else{` |
|    ! 0 | 1349 | `				pData->bFirst = 0;` |
|      - | 1350 | `			}` |
|      1 | 1351 | `		}` |
|      - | 1352 | `		/* Recurse */` |
|      3 | 1353 | `		pData->bFirst = 1;` |
|      3 | 1354 | `		pData->nRecCount++;` |
|      3 | 1355 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 1356 | `		pData->nRecCount--;` |
|      - | 1357 | `		/* Propagate an allocation failure surfaced deeper in the recursion. */` |
|      3 | 1358 | `		if( pData->rc != SXRET_OK ){` |
|    ! 0 | 1359 | `			return PH7_ABORT;` |
|      - | 1360 | `		}` |
|      3 | 1361 | `		return PH7_OK;` |
|      - | 1362 | `	}` |
|      - | 1363 | `	/* Extract the string representation of the entry value */` |
| 140447 | 1364 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 1365 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 140447 | 1366 | `	if( pData->bFirst ){` |
|  32949 | 1367 | `		pData->bFirst = 0;` |
| 123975 | 1368 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 1369 | `		/* append the separator first */` |
| 107491 | 1370 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 1371 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1372 | `			return PH7_ABORT;` |
|      - | 1373 | `		}` |
|  53743 | 1374 | `	}` |
|      - | 1375 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 140447 | 1376 | `	if( nLen > 0 ){` |
| 128393 | 1377 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1378 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1379 | `			return PH7_ABORT;` |
|      - | 1380 | `		}` |
|  64194 | 1381 | `	}` |
| 140447 | 1382 | `	return PH7_OK;` |
|  70227 | 1383 | `}` |
|      - | 1384 | `/*` |
|      - | 1385 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 1386 | ` * string implode(array $pieces,...)` |
|      - | 1387 | ` *  Join array elements with a string.` |
|      - | 1388 | ` * $glue` |
|      - | 1389 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 1390 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 1391 | ` * $pieces` |
|      - | 1392 | ` *   The array of strings to implode.` |
|      - | 1393 | ` * Return` |
|      - | 1394 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 1395 | ` *  order, with the glue string between each element.` |
|      - | 1396 | ` */` |
|  32970 | 1397 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1398 | `{` |
|      - | 1399 | `	struct implode_data imp_data;` |
|  32975 | 1400 | `	int i = 1;` |
|  32975 | 1401 | `	if( nArg < 1 ){` |
|      - | 1402 | `		/* Missing argument,return NULL */` |
|    ! 0 | 1403 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1404 | `		return PH7_OK;` |
|      - | 1405 | `	}` |
|      - | 1406 | `	/* Prepare the implode context */` |
|  32975 | 1407 | `	imp_data.pCtx = pCtx;` |
|  32975 | 1408 | `	imp_data.bRecursive = 0;` |
|  32975 | 1409 | `	imp_data.bFirst = 1;` |
|  32975 | 1410 | `	imp_data.nRecCount = 0;` |
|  32975 | 1411 | `	imp_data.rc = SXRET_OK;` |
|  32975 | 1412 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  32973 | 1413 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  16489 | 1414 | `	}else{` |
|      3 | 1415 | `		imp_data.zSep = 0;` |
|      3 | 1416 | `		imp_data.nSeplen = 0;` |
|      3 | 1417 | `		i = 0;` |
|      - | 1418 | `	}` |
|  32975 | 1419 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 1420 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1421 | `	}` |
|      - | 1422 | `	/* Start the 'join' process */` |
|  65945 | 1423 | `	while( i < nArg ){` |
|  32975 | 1424 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1425 | `			/* Iterate throw array entries */` |
|  32975 | 1426 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 1427 | `			/* Surface a callback allocation failure as a fatal */` |
|  32975 | 1428 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 1429 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1430 | `			}` |
|  16490 | 1431 | `		}else{` |
|      - | 1432 | `			const char *zData;` |
|      - | 1433 | `			int nLen;` |
|      - | 1434 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 1435 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1436 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 1437 | `			if( imp_data.bFirst ){` |
|    ! 0 | 1438 | `				imp_data.bFirst = 0;` |
|    ! 0 | 1439 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1440 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 1441 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1442 | `				}` |
|    ! 0 | 1443 | `			}` |
|      - | 1444 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 1445 | `			if( nLen > 0 ){` |
|    ! 0 | 1446 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1447 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1448 | `				}` |
|    ! 0 | 1449 | `			}` |
|      - | 1450 | `		}` |
|  32975 | 1451 | `		i++;` |
|      5 | 1452 | `	}` |
|  32975 | 1453 | `	return PH7_OK;` |
|  16490 | 1454 | `}` |
|      - | 1455 | `/*` |
|      - | 1456 | ` * Symisc eXtension:` |
|      - | 1457 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 1458 | ` * Purpose` |
|      - | 1459 | ` *  Same as implode() but recurse on arrays.` |
|      - | 1460 | ` * Example:` |
|      - | 1461 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 1462 | ` *   echo implode_recursive("/",$a);` |
|      - | 1463 | ` *   Will output` |
|      - | 1464 | ` *     usr/home/dean.` |
|      - | 1465 | ` *   While the standard implode would produce.` |
|      - | 1466 | ` *    usr/Array.` |
|      - | 1467 | ` * Parameter` |
|      - | 1468 | ` *  Refer to implode().` |
|      - | 1469 | ` * Return` |
|      - | 1470 | ` *  Refer to implode().` |
|      - | 1471 | ` */` |
|     12 | 1472 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1473 | `{` |
|      - | 1474 | `	struct implode_data imp_data;` |
|     13 | 1475 | `	int i = 1;` |
|     13 | 1476 | `	if( nArg < 1 ){` |
|      - | 1477 | `		/* Missing argument,return NULL */` |
|      3 | 1478 | `		ph7_result_null(pCtx);` |
|      3 | 1479 | `		return PH7_OK;` |
|      - | 1480 | `	}` |
|      - | 1481 | `	/* Prepare the implode context */` |
|     11 | 1482 | `	imp_data.pCtx = pCtx;` |
|     11 | 1483 | `	imp_data.bRecursive = 1;` |
|     11 | 1484 | `	imp_data.bFirst = 1;` |
|     11 | 1485 | `	imp_data.nRecCount = 0;` |
|     11 | 1486 | `	imp_data.rc = SXRET_OK;` |
|     11 | 1487 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 1488 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 1489 | `	}else{` |
|    ! 0 | 1490 | `		imp_data.zSep = 0;` |
|    ! 0 | 1491 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 1492 | `		i = 0;` |
|      - | 1493 | `	}` |
|     11 | 1494 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 1495 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1496 | `	}` |
|      - | 1497 | `	/* Start the 'join' process */` |
|     21 | 1498 | `	while( i < nArg ){` |
|     11 | 1499 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1500 | `			/* Iterate throw array entries */` |
|      3 | 1501 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 1502 | `			/* Surface a callback allocation failure as a fatal */` |
|      3 | 1503 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 1504 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1505 | `			}` |
|      2 | 1506 | `		}else{` |
|      - | 1507 | `			const char *zData;` |
|      - | 1508 | `			int nLen;` |
|      - | 1509 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 1510 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1511 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 1512 | `			if( imp_data.bFirst ){` |
|      9 | 1513 | `				imp_data.bFirst = 0;` |
|      4 | 1514 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1515 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 1516 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1517 | `				}` |
|    ! 0 | 1518 | `			}` |
|      - | 1519 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 1520 | `			if( nLen > 0 ){` |
|      9 | 1521 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1522 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1523 | `				}` |
|      4 | 1524 | `			}` |
|      - | 1525 | `		}` |
|     11 | 1526 | `		i++;` |
|      1 | 1527 | `	}` |
|     11 | 1528 | `	return PH7_OK;` |
|      7 | 1529 | `}` |
|      - | 1530 | `/*` |
|      - | 1531 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 1532 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 1533 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 1534 | ` * Parameters` |
|      - | 1535 | ` *  $delimiter` |
|      - | 1536 | ` *   The boundary string.` |
|      - | 1537 | ` * $string` |
|      - | 1538 | ` *   The input string.` |
|      - | 1539 | ` * $limit` |
|      - | 1540 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 1541 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 1542 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 1543 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 1544 | ` * Returns` |
|      - | 1545 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 1546 | ` *  on boundaries formed by the delimiter.` |
|      - | 1547 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 1548 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 1549 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 1550 | ` *  will be returned.` |
|      - | 1551 | ` * NOTE:` |
|      - | 1552 | ` *  Negative limit is not supported.` |
|      - | 1553 | ` */` |
|   6416 | 1554 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1555 | `{` |
|      - | 1556 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 1557 | `	int nDelim,nStrlen,iLimit;` |
|      - | 1558 | `	ph7_value *pArray;` |
|      - | 1559 | `	ph7_value *pValue;` |
|      - | 1560 | `	sxu32 nOfft;` |
|      - | 1561 | `	sxi32 rc;` |
|   6421 | 1562 | `	if( nArg < 2 ){` |
|      - | 1563 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 1564 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1565 | `		return PH7_OK;` |
|      - | 1566 | `	}` |
|      - | 1567 | `	/* Extract the delimiter */` |
|   6421 | 1568 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   6421 | 1569 | `	if( nDelim < 1 ){` |
|      - | 1570 | `		/* Empty delimiter: PHP 8 throws a catchable ValueError. */` |
|      5 | 1571 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1572 | `			"explode(): Argument #1 ($separator) must not be empty");` |
|      - | 1573 | `	}` |
|      - | 1574 | `	/* Extract the string */` |
|   6417 | 1575 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   6417 | 1576 | `	if( nStrlen < 1 ){` |
|      - | 1577 | `		/* Empty string: normally an array with a single empty element (PHP behavior).` |
|      - | 1578 | `		 * A negative limit drops the last -limit components, so the sole empty` |
|      - | 1579 | `		 * component is dropped and the result is an empty array. */` |
|      7 | 1580 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      7 | 1581 | `		if( pArrayTmp == 0 ){` |
|      - | 1582 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 1583 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 1584 | `			return PH7_OK;` |
|      - | 1585 | `		}` |
|      7 | 1586 | `		if( !(nArg > 2 && ph7_value_to_int(apArg[2]) < 0) ){` |
|      5 | 1587 | `			ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      5 | 1588 | `			if( pValueTmp == 0 ){` |
|      - | 1589 | `				/* Out of memory,return FALSE */` |
|    ! 0 | 1590 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 1591 | `				return PH7_OK;` |
|      - | 1592 | `			}` |
|      5 | 1593 | `			ph7_value_string(pValueTmp, "", 0);` |
|      5 | 1594 | `			if( ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp) != SXRET_OK ){` |
|    ! 0 | 1595 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1596 | `			}` |
|      2 | 1597 | `		}` |
|      7 | 1598 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      7 | 1599 | `		return PH7_OK;` |
|      - | 1600 | `	}` |
|      - | 1601 | `	/* Point to the end of the string */` |
|   6411 | 1602 | `	zEnd = &zString[nStrlen];` |
|      - | 1603 | `	/* Create the array */` |
|   6411 | 1604 | `	pArray =  ph7_context_new_array(pCtx);` |
|   6411 | 1605 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   6411 | 1606 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 1607 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 1608 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1609 | `		return PH7_OK;` |
|      - | 1610 | `	}` |
|      - | 1611 | `	/* Set a defualt limit */` |
|   6411 | 1612 | `	iLimit = SXI32_HIGH;` |
|   6411 | 1613 | `	if( nArg > 2 ){` |
|     38 | 1614 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     38 | 1615 | `		if( iLimit < 0 ){` |
|      - | 1616 | `			/* Negative limit: keep all components except the last -iLimit (PHP).` |
|      - | 1617 | `			 * Pre-count the components (delimiters + 1), then emit only the first` |
|      - | 1618 | `			 * nKeep CLEAN components — no trailing-remainder merge (the difference` |
|      - | 1619 | `			 * from the positive path). nKeep <= 0 drops everything -> empty array. */` |
|     17 | 1620 | `			int nTotal = 1,nKeep;` |
|     17 | 1621 | `			const char *zScan = zString;` |
|      - | 1622 | `			sxu32 nScanOfft;` |
|     57 | 1623 | `			while( SyBlobSearch(zScan,(sxu32)(zEnd - zScan),zDelim,nDelim,&nScanOfft) == SXRET_OK ){` |
|     41 | 1624 | `				nTotal++;` |
|     41 | 1625 | `				zScan = &zScan[nScanOfft + nDelim];` |
|      1 | 1626 | `			}` |
|     17 | 1627 | `			nKeep = nTotal + iLimit; /* iLimit < 0, so this is nTotal - (-iLimit) */` |
|     49 | 1628 | `			while( nKeep > (int)ph7_array_count(pArray)` |
|     39 | 1629 | `				&& SyBlobSearch(zString,(sxu32)(zEnd - zString),zDelim,nDelim,&nOfft) == SXRET_OK ){` |
|      - | 1630 | `				/* Emit the next clean component */` |
|     23 | 1631 | `				zCur = &zString[nOfft];` |
|     23 | 1632 | `				ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|     23 | 1633 | `				if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 1634 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1635 | `				}` |
|     23 | 1636 | `				zString = &zCur[nDelim];` |
|     23 | 1637 | `				ph7_value_reset_string_cursor(pValue);` |
|      1 | 1638 | `			}` |
|     17 | 1639 | `			ph7_result_value(pCtx,pArray);` |
|     17 | 1640 | `			return PH7_OK;` |
|      - | 1641 | `		}` |
|     22 | 1642 | `		if( iLimit == 0 ){` |
|      5 | 1643 | `			iLimit = 1;` |
|      2 | 1644 | `		}` |
|     22 | 1645 | `		iLimit--;` |
|      9 | 1646 | `	}` |
|      - | 1647 | `	/* Start exploding */` |
|  75981 | 1648 | `	for(;;){` |
| 151967 | 1649 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 151967 | 1650 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 1651 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   6395 | 1652 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   6395 | 1653 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 1654 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1655 | `			}` |
|   6395 | 1656 | `			break;` |
|      - | 1657 | `		}` |
|      - | 1658 | `		/* Point to the desired offset */` |
| 145577 | 1659 | `		zCur = &zString[nOfft];` |
|      - | 1660 | `		/* Perform the store operation (may be empty) */` |
| 145577 | 1661 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 145577 | 1662 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 1663 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 1664 | `		}` |
|      - | 1665 | `		/* Point beyond the delimiter */` |
| 145577 | 1666 | `		zString = &zCur[nDelim];` |
|      - | 1667 | `		/* Reset the cursor */` |
| 145577 | 1668 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 1669 | `	}` |
|      - | 1670 | `	/* Return the freshly created array */` |
|   6395 | 1671 | `	ph7_result_value(pCtx,pArray);` |
|      - | 1672 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 1673 | `	 * released as soon we return from this foregin function.` |
|      - | 1674 | `	 */` |
|   6395 | 1675 | `	return PH7_OK;` |
|   3213 | 1676 | `}` |
|      - | 1677 | `/*` |
|      - | 1678 | ` * string trim(string $str[,string $charlist ])` |
|      - | 1679 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 1680 | ` * Parameters` |
|      - | 1681 | ` *  $str` |
|      - | 1682 | ` *   The string that will be trimmed.` |
|      - | 1683 | ` * $charlist` |
|      - | 1684 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1685 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1686 | ` *   With .. you can specify a range of characters.` |
|      - | 1687 | ` * Returns.` |
|      - | 1688 | ` *  Thr processed string.` |
|      - | 1689 | ` * NOTE:` |
|      - | 1690 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 1691 | ` */` |
|  14486 | 1692 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1693 | `{` |
|      - | 1694 | `	const char *zString;` |
|      - | 1695 | `	int nLen;` |
|  14491 | 1696 | `	if( nArg < 1 ){` |
|      - | 1697 | `		/* Missing arguments,return null */` |
|    ! 0 | 1698 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1699 | `		return PH7_OK;` |
|      - | 1700 | `	}` |
|      - | 1701 | `	/* Extract the target string */` |
|  14491 | 1702 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  14491 | 1703 | `	if( nLen < 1 ){` |
|      - | 1704 | `		/* Empty string,return */` |
|   1305 | 1705 | `		ph7_result_string(pCtx,"",0);` |
|   1305 | 1706 | `		return PH7_OK;` |
|      - | 1707 | `	}` |
|      - | 1708 | `	/* Start the trim process */` |
|  13191 | 1709 | `	if( nArg < 2 ){` |
|      - | 1710 | `		SyString sStr;` |
|      - | 1711 | `		/* Remove white spaces and NUL bytes */` |
|  13161 | 1712 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  33017 | 1713 | `		SyStringFullTrimSafe(&sStr);` |
|  13161 | 1714 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   6583 | 1715 | `	}else{` |
|      - | 1716 | `		/* Char list */` |
|      - | 1717 | `		const char *zList;` |
|      - | 1718 | `		int nListlen;` |
|     33 | 1719 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     33 | 1720 | `		if( nListlen < 1 ){` |
|      - | 1721 | `			/* Return the string unchanged */` |
|      6 | 1722 | `			ph7_result_string(pCtx,zString,nLen);` |
|      4 | 1723 | `		}else{` |
|      - | 1724 | `			char aMask[256];` |
|     29 | 1725 | `			const char *zEnd = &zString[nLen];` |
|     29 | 1726 | `			const char *zCur = zString;` |
|     29 | 1727 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 1728 | `			/* Left trim */` |
|     79 | 1729 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     53 | 1730 | `				zCur++;` |
|      3 | 1731 | `			}` |
|      - | 1732 | `			/* Right trim */` |
|     79 | 1733 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     53 | 1734 | `				zEnd--;` |
|      3 | 1735 | `			}` |
|     29 | 1736 | `			if( zCur >= zEnd ){` |
|      - | 1737 | `				/* Return the empty string */` |
|    ! 0 | 1738 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1739 | `			}else{` |
|     29 | 1740 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 1741 | `			}` |
|      - | 1742 | `		}` |
|      - | 1743 | `	}` |
|  13191 | 1744 | `	return PH7_OK;` |
|   7248 | 1745 | `}` |
|      - | 1746 | `/*` |
|      - | 1747 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 1748 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 1749 | ` * Parameters` |
|      - | 1750 | ` *  $str` |
|      - | 1751 | ` *   The string that will be trimmed.` |
|      - | 1752 | ` * $charlist` |
|      - | 1753 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1754 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1755 | ` *   With .. you can specify a range of characters.` |
|      - | 1756 | ` * Returns.` |
|      - | 1757 | ` *  Thr processed string.` |
|      - | 1758 | ` * NOTE:` |
|      - | 1759 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 1760 | ` */` |
|     28 | 1761 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1762 | `{` |
|      - | 1763 | `	const char *zString;` |
|      - | 1764 | `	int nLen;` |
|     31 | 1765 | `	if( nArg < 1 ){` |
|      - | 1766 | `		/* Missing arguments,return null */` |
|    ! 0 | 1767 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1768 | `		return PH7_OK;` |
|      - | 1769 | `	}` |
|      - | 1770 | `	/* Extract the target string */` |
|     31 | 1771 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 1772 | `	if( nLen < 1 ){` |
|      - | 1773 | `		/* Empty string,return */` |
|      5 | 1774 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1775 | `		return PH7_OK;` |
|      - | 1776 | `	}` |
|      - | 1777 | `	/* Start the trim process */` |
|     27 | 1778 | `	if( nArg < 2 ){` |
|      - | 1779 | `		SyString sStr;` |
|      - | 1780 | `		/* Remove white spaces and NUL bytes*/` |
|     17 | 1781 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     41 | 1782 | `		SyStringRightTrimSafe(&sStr);` |
|     17 | 1783 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      9 | 1784 | `	}else{` |
|      - | 1785 | `		/* Char list */` |
|      - | 1786 | `		const char *zList;` |
|      - | 1787 | `		int nListlen;` |
|     11 | 1788 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     11 | 1789 | `		if( nListlen < 1 ){` |
|      - | 1790 | `			/* Return the string unchanged */` |
|    ! 0 | 1791 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 1792 | `		}else{` |
|      - | 1793 | `			char aMask[256];` |
|     11 | 1794 | `			const char *zEnd = &zString[nLen];` |
|     11 | 1795 | `			const char *zCur = zString;` |
|     11 | 1796 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 1797 | `			/* Right trim */` |
|     29 | 1798 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     20 | 1799 | `				zEnd--;` |
|      2 | 1800 | `			}` |
|     11 | 1801 | `			if( zEnd <= zCur ){` |
|      - | 1802 | `				/* Return the empty string */` |
|    ! 0 | 1803 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1804 | `			}else{` |
|     11 | 1805 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 1806 | `			}` |
|      - | 1807 | `		}` |
|      - | 1808 | `	}` |
|     27 | 1809 | `	return PH7_OK;` |
|     17 | 1810 | `}` |
|      - | 1811 | `/*` |
|      - | 1812 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 1813 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 1814 | ` * Parameters` |
|      - | 1815 | ` *  $str` |
|      - | 1816 | ` *   The string that will be trimmed.` |
|      - | 1817 | ` * $charlist` |
|      - | 1818 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1819 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1820 | ` *   With .. you can specify a range of characters.` |
|      - | 1821 | ` * Returns.` |
|      - | 1822 | ` *  Thr processed string.` |
|      - | 1823 | ` * NOTE:` |
|      - | 1824 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 1825 | ` */` |
|     12 | 1826 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1827 | `{` |
|      - | 1828 | `	const char *zString;` |
|      - | 1829 | `	int nLen;` |
|     14 | 1830 | `	if( nArg < 1 ){` |
|      - | 1831 | `		/* Missing arguments,return null */` |
|    ! 0 | 1832 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1833 | `		return PH7_OK;` |
|      - | 1834 | `	}` |
|      - | 1835 | `	/* Extract the target string */` |
|     14 | 1836 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     14 | 1837 | `	if( nLen < 1 ){` |
|      - | 1838 | `		/* Empty string,return */` |
|    ! 0 | 1839 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1840 | `		return PH7_OK;` |
|      - | 1841 | `	}` |
|      - | 1842 | `	/* Start the trim process */` |
|     14 | 1843 | `	if( nArg < 2 ){` |
|      - | 1844 | `		SyString sStr;` |
|      - | 1845 | `		/* Remove white spaces and NUL byte */` |
|      3 | 1846 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|      8 | 1847 | `		SyStringLeftTrimSafe(&sStr);` |
|      3 | 1848 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      2 | 1849 | `	}else{` |
|      - | 1850 | `		/* Char list */` |
|      - | 1851 | `		const char *zList;` |
|      - | 1852 | `		int nListlen;` |
|     12 | 1853 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     12 | 1854 | `		if( nListlen < 1 ){` |
|      - | 1855 | `			/* Return the string unchanged */` |
|      3 | 1856 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 1857 | `		}else{` |
|      - | 1858 | `			char aMask[256];` |
|     10 | 1859 | `			const char *zEnd = &zString[nLen];` |
|     10 | 1860 | `			const char *zCur = zString;` |
|     10 | 1861 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 1862 | `			/* Left trim */` |
|     28 | 1863 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     20 | 1864 | `				zCur++;` |
|      2 | 1865 | `			}` |
|     10 | 1866 | `			if( zCur >= zEnd ){` |
|      - | 1867 | `				/* Return the empty string */` |
|    ! 0 | 1868 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1869 | `			}else{` |
|     10 | 1870 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 1871 | `			}` |
|      - | 1872 | `		}` |
|      - | 1873 | `	}` |
|     14 | 1874 | `	return PH7_OK;` |
|      8 | 1875 | `}` |
|      - | 1876 | `/*` |
|      - | 1877 | ` * string strtolower(string $str)` |
|      - | 1878 | ` *  Make a string lowercase.` |
|      - | 1879 | ` * Parameters` |
|      - | 1880 | ` *  $str` |
|      - | 1881 | ` *   The input string.` |
|      - | 1882 | ` * Returns.` |
|      - | 1883 | ` *  The lowercased string.` |
|      - | 1884 | ` */` |
|  32954 | 1885 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1886 | `{` |
|      - | 1887 | `	const char *zString,*zCur,*zEnd;` |
|      - | 1888 | `	int nLen;` |
|  32959 | 1889 | `	if( nArg < 1 ){` |
|      - | 1890 | `		/* Missing arguments,return null */` |
|    ! 0 | 1891 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1892 | `		return PH7_OK;` |
|      - | 1893 | `	}` |
|      - | 1894 | `	/* Extract the target string */` |
|  32959 | 1895 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  32959 | 1896 | `	if( nLen < 1 ){` |
|      - | 1897 | `		/* Empty string,return */` |
|      3 | 1898 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1899 | `		return PH7_OK;` |
|      - | 1900 | `	}` |
|      - | 1901 | `	/* Perform the requested operation */` |
|  32957 | 1902 | `	zEnd = &zString[nLen];` |
| 104041 | 1903 | `	for(;;){` |
| 208087 | 1904 | `		if( zString >= zEnd ){` |
|      - | 1905 | `			/* No more input,break immediately */` |
|  32957 | 1906 | `			break;` |
|      - | 1907 | `		}` |
| 175135 | 1908 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 1909 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 1910 | `			zCur = zString;` |
|    ! 0 | 1911 | `			zString++;` |
|    ! 0 | 1912 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 1913 | `				zString++;` |
|    ! 0 | 1914 | `			}` |
|      - | 1915 | `			/* Append UTF-8 stream */` |
|    ! 0 | 1916 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 1917 | `		}else{` |
| 175135 | 1918 | `			int c = zString[0];` |
| 175135 | 1919 | `			if( SyisUpper(c) ){` |
| 172789 | 1920 | `				c = SyToLower(zString[0]);` |
|  86392 | 1921 | `			}` |
|      - | 1922 | `			/* Append character */` |
| 175135 | 1923 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 1924 | `			/* Advance the cursor */` |
| 175135 | 1925 | `			zString++;` |
|      - | 1926 | `		}` |
|      5 | 1927 | `	}` |
|  32957 | 1928 | `	return PH7_OK;` |
|  16482 | 1929 | `}` |
|      - | 1930 | `/*` |
|      - | 1931 | ` * string strtolower(string $str)` |
|      - | 1932 | ` *  Make a string uppercase.` |
|      - | 1933 | ` * Parameters` |
|      - | 1934 | ` *  $str` |
|      - | 1935 | ` *   The input string.` |
|      - | 1936 | ` * Returns.` |
|      - | 1937 | ` *  The uppercased string.` |
|      - | 1938 | ` */` |
|     48 | 1939 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1940 | `{` |
|      - | 1941 | `	const char *zString,*zCur,*zEnd;` |
|      - | 1942 | `	int nLen;` |
|     52 | 1943 | `	if( nArg < 1 ){` |
|      - | 1944 | `		/* Missing arguments,return null */` |
|    ! 0 | 1945 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1946 | `		return PH7_OK;` |
|      - | 1947 | `	}` |
|      - | 1948 | `	/* Extract the target string */` |
|     52 | 1949 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     52 | 1950 | `	if( nLen < 1 ){` |
|      - | 1951 | `		/* Empty string,return */` |
|      3 | 1952 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1953 | `		return PH7_OK;` |
|      - | 1954 | `	}` |
|      - | 1955 | `	/* Perform the requested operation */` |
|     50 | 1956 | `	zEnd = &zString[nLen];` |
|    111 | 1957 | `	for(;;){` |
|    226 | 1958 | `		if( zString >= zEnd ){` |
|      - | 1959 | `			/* No more input,break immediately */` |
|     50 | 1960 | `			break;` |
|      - | 1961 | `		}` |
|    180 | 1962 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 1963 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 1964 | `			zCur = zString;` |
|    ! 0 | 1965 | `			zString++;` |
|    ! 0 | 1966 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 1967 | `				zString++;` |
|    ! 0 | 1968 | `			}` |
|      - | 1969 | `			/* Append UTF-8 stream */` |
|    ! 0 | 1970 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 1971 | `		}else{` |
|    180 | 1972 | `			int c = zString[0];` |
|    180 | 1973 | `			if( SyisLower(c) ){` |
|    174 | 1974 | `				c = SyToUpper(zString[0]);` |
|     85 | 1975 | `			}` |
|      - | 1976 | `			/* Append character */` |
|    180 | 1977 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 1978 | `			/* Advance the cursor */` |
|    180 | 1979 | `			zString++;` |
|      - | 1980 | `		}` |
|      4 | 1981 | `	}` |
|     50 | 1982 | `	return PH7_OK;` |
|     28 | 1983 | `}` |
|      - | 1984 | `/*` |
|      - | 1985 | ` * string ucfirst(string $str)` |
|      - | 1986 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 1987 | ` *  character is alphabetic.` |
|      - | 1988 | ` * Parameters` |
|      - | 1989 | ` *  $str` |
|      - | 1990 | ` *   The input string.` |
|      - | 1991 | ` * Returns.` |
|      - | 1992 | ` *  The processed string.` |
|      - | 1993 | ` */` |
|      4 | 1994 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1995 | `{` |
|      - | 1996 | `	const char *zString,*zEnd;` |
|      - | 1997 | `	int nLen,c;` |
|      5 | 1998 | `	if( nArg < 1 ){` |
|      - | 1999 | `		/* Missing arguments,return null */` |
|    ! 0 | 2000 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2001 | `		return PH7_OK;` |
|      - | 2002 | `	}` |
|      - | 2003 | `	/* Extract the target string */` |
|      5 | 2004 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2005 | `	if( nLen < 1 ){` |
|      - | 2006 | `		/* Empty string,return */` |
|      3 | 2007 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2008 | `		return PH7_OK;` |
|      - | 2009 | `	}` |
|      - | 2010 | `	/* Perform the requested operation */` |
|      3 | 2011 | `	zEnd = &zString[nLen];` |
|      3 | 2012 | `	c = zString[0];` |
|      3 | 2013 | `	if( SyisLower(c) ){` |
|      3 | 2014 | `		c = SyToUpper(c);` |
|      1 | 2015 | `	}` |
|      - | 2016 | `	/* Append the first character */` |
|      3 | 2017 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2018 | `	zString++;` |
|      3 | 2019 | `	if( zString < zEnd ){` |
|      - | 2020 | `		/* Append the rest of the input verbatim */` |
|      3 | 2021 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2022 | `	}` |
|      3 | 2023 | `	return PH7_OK;` |
|      3 | 2024 | `}` |
|      - | 2025 | `/*` |
|      - | 2026 | ` * string lcfirst(string $str)` |
|      - | 2027 | ` *  Make a string's first character lowercase.` |
|      - | 2028 | ` * Parameters` |
|      - | 2029 | ` *  $str` |
|      - | 2030 | ` *   The input string.` |
|      - | 2031 | ` * Returns.` |
|      - | 2032 | ` *  The processed string.` |
|      - | 2033 | ` */` |
|      4 | 2034 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2035 | `{` |
|      - | 2036 | `	const char *zString,*zEnd;` |
|      - | 2037 | `	int nLen,c;` |
|      5 | 2038 | `	if( nArg < 1 ){` |
|      - | 2039 | `		/* Missing arguments,return null */` |
|    ! 0 | 2040 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2041 | `		return PH7_OK;` |
|      - | 2042 | `	}` |
|      - | 2043 | `	/* Extract the target string */` |
|      5 | 2044 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2045 | `	if( nLen < 1 ){` |
|      - | 2046 | `		/* Empty string,return */` |
|      3 | 2047 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2048 | `		return PH7_OK;` |
|      - | 2049 | `	}` |
|      - | 2050 | `	/* Perform the requested operation */` |
|      3 | 2051 | `	zEnd = &zString[nLen];` |
|      3 | 2052 | `	c = zString[0];` |
|      3 | 2053 | `	if( SyisUpper(c) ){` |
|      3 | 2054 | `		c = SyToLower(c);` |
|      1 | 2055 | `	}` |
|      - | 2056 | `	/* Append the first character */` |
|      3 | 2057 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2058 | `	zString++;` |
|      3 | 2059 | `	if( zString < zEnd ){` |
|      - | 2060 | `		/* Append the rest of the input verbatim */` |
|      3 | 2061 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2062 | `	}` |
|      3 | 2063 | `	return PH7_OK;` |
|      3 | 2064 | `}` |
|      - | 2065 | `/*` |
|      - | 2066 | ` * int ord(string $string)` |
|      - | 2067 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 2068 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 2069 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 2070 | ` * Parameters` |
|      - | 2071 | ` *  $string` |
|      - | 2072 | ` *   The input string.` |
|      - | 2073 | ` * Returns` |
|      - | 2074 | ` *  The ASCII value as an integer.` |
|      - | 2075 | ` */` |
|     56 | 2076 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2077 | `{` |
|      - | 2078 | `	const char *zString;` |
|      - | 2079 | `	int nLen,c;` |
|      - | 2080 | `	/* PHP requires exactly one argument. */` |
|     59 | 2081 | `	if( nArg != 1 ){` |
|      8 | 2082 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2083 | `			"ArgumentCountError",` |
|      - | 2084 | `			"ord() expects exactly 1 argument, %d given",` |
|      2 | 2085 | `			nArg` |
|      - | 2086 | `			);` |
|      - | 2087 | `	}` |
|      - | 2088 | `	/* Passing null is deprecated (E_DEPRECATED).  PHP emits this before` |
|      - | 2089 | `	 * the empty-string deprecation, so we check null first. */` |
|     53 | 2090 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 2091 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2092 | `			"ord(): Passing null to parameter #1 ($character) "` |
|      - | 2093 | `			"of type string is deprecated"` |
|      - | 2094 | `			);` |
|      1 | 2095 | `	}` |
|      - | 2096 | `	/* Extract the target string */` |
|     53 | 2097 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     53 | 2098 | `	if( nLen < 1 ){` |
|      - | 2099 | `		/* Empty string is deprecated (E_DEPRECATED). */` |
|      5 | 2100 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2101 | `			"ord(): Providing an empty string is deprecated"` |
|      - | 2102 | `			);` |
|      5 | 2103 | `		ph7_result_int(pCtx,0);` |
|      5 | 2104 | `		return PH7_OK;` |
|      - | 2105 | `	}` |
|      - | 2106 | `	/* A string longer than one byte is deprecated (E_DEPRECATED). */` |
|     49 | 2107 | `	if( nLen > 1 ){` |
|      7 | 2108 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2109 | `			"ord(): Providing a string that is not one byte long "` |
|      - | 2110 | `			"is deprecated. Use ord($str[0]) instead"` |
|      - | 2111 | `			);` |
|      3 | 2112 | `	}` |
|      - | 2113 | `	/* Extract the ASCII value of the first character */` |
|     49 | 2114 | `	c = (unsigned char)zString[0];` |
|      - | 2115 | `	/* Return that value */` |
|     49 | 2116 | `	ph7_result_int(pCtx,c);` |
|     49 | 2117 | `	return PH7_OK;` |
|     31 | 2118 | `}` |
|      - | 2119 | `/*` |
|      - | 2120 | ` * string chr(int $codepoint)` |
|      - | 2121 | ` *  Returns a one-character string containing the character specified` |
|      - | 2122 | ` *  by the given codepoint.  Any integer is accepted; values outside` |
|      - | 2123 | ` *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.` |
|      - | 2124 | ` * Parameters` |
|      - | 2125 | ` *  $codepoint` |
|      - | 2126 | ` *   An integer codepoint.  Values outside 0-255 are deprecated and` |
|      - | 2127 | ` *   will be constrained to a single byte.` |
|      - | 2128 | ` * Returns` |
|      - | 2129 | ` *  A single-character string.` |
|      - | 2130 | ` */` |
|   6486 | 2131 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2132 | `{` |
|      - | 2133 | `	int c;` |
|      - | 2134 | `	unsigned char ch;` |
|      - | 2135 | `	/* PHP requires exactly one argument. */` |
|   6489 | 2136 | `	if( nArg != 1 ){` |
|      8 | 2137 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2138 | `			"ArgumentCountError",` |
|      - | 2139 | `			"chr() expects exactly 1 argument, %d given",` |
|      2 | 2140 | `			nArg` |
|      - | 2141 | `			);` |
|      - | 2142 | `	}` |
|      - | 2143 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 2144 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 2145 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 2146 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|   6483 | 2147 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      - | 2148 | `		char zBuf[120];` |
|      4 | 2149 | `		SyBufferFormat(zBuf,sizeof(zBuf),` |
|      - | 2150 | `			"Implicit conversion from float %g to int loses precision",` |
|      1 | 2151 | `			ph7_value_to_double(apArg[0])` |
|      - | 2152 | `			);` |
|      3 | 2153 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zBuf);` |
|      1 | 2154 | `	}` |
|      - | 2155 | `	/* Extract the codepoint. */` |
|   6483 | 2156 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 2157 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 2158 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 2159 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 2160 | `	 * name to avoid the API double-prefixing it. */` |
|   6483 | 2161 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 2162 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 2163 | `			E_DEPRECATED,` |
|      - | 2164 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 2165 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 2166 | `			"The value used will be constrained using % 256"` |
|      - | 2167 | `			);` |
|      2 | 2168 | `	}` |
|      - | 2169 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 2170 | `	 * when taking the address of a wider int. */` |
|   6483 | 2171 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 2172 | `	/* Return the specified character */` |
|   6483 | 2173 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|   6483 | 2174 | `	return PH7_OK;` |
|   3246 | 2175 | `}` |
|      - | 2176 | `/*` |
|      - | 2177 | ` * Binary to hex consumer callback.` |
|      - | 2178 | ` * This callback is the default consumer used by the hash functions` |
|      - | 2179 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 2180 | ` */` |
|   3118 | 2181 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 2182 | `{` |
|      - | 2183 | `	/* Append hex chunk verbatim */` |
|   3120 | 2184 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   3120 | 2185 | `	return SXRET_OK;` |
|      2 | 2186 | `}` |
|      - | 2187 |  |
|      - | 2188 | `/*` |
|      - | 2189 | ` * string bin2hex(string $str)` |
|      - | 2190 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 2191 | ` * Parameters` |
|      - | 2192 | ` *  $str` |
|      - | 2193 | ` *   The input string.` |
|      - | 2194 | ` * Returns.` |
|      - | 2195 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 2196 | ` */` |
|    138 | 2197 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2198 | `{` |
|      - | 2199 | `	const char *zString;` |
|      - | 2200 | `	int nLen;` |
|      - | 2201 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|    143 | 2202 | `	if( nArg != 1 ){` |
|      8 | 2203 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2204 | `			"ArgumentCountError",` |
|      - | 2205 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|      2 | 2206 | `			nArg` |
|      - | 2207 | `			);` |
|      - | 2208 | `	}` |
|      - | 2209 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 2210 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 2211 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 2212 | `	 */` |
|    204 | 2213 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|    130 | 2214 | `		( ph7_value_is_object(apArg[0]) &&` |
|      3 | 2215 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|      2 | 2216 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|      1 | 2217 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 2218 | `		)` |
|      - | 2219 | `	){` |
|      9 | 2220 | `		const char *zType = ph7_type_name(apArg[0]);` |
|      9 | 2221 | `		if( ph7_value_is_object(apArg[0]) ){` |
|      3 | 2222 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      3 | 2223 | `			if( pInst && pInst->pClass ){` |
|      3 | 2224 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      1 | 2225 | `			}` |
|      1 | 2226 | `		}` |
|     12 | 2227 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2228 | `			"TypeError",` |
|      - | 2229 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|      3 | 2230 | `			zType` |
|      - | 2231 | `			);` |
|      - | 2232 | `	}` |
|      - | 2233 | `	/* Extract the target string */` |
|    130 | 2234 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    130 | 2235 | `	if( nLen < 1 ){` |
|      - | 2236 | `		/* Empty string,return */` |
|     13 | 2237 | `		ph7_result_string(pCtx,"",0);` |
|     13 | 2238 | `		return PH7_OK;` |
|      - | 2239 | `	}` |
|      - | 2240 | `	/* Perform the requested operation */` |
|    118 | 2241 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|    118 | 2242 | `	return PH7_OK;` |
|     74 | 2243 | `}` |
|      - | 2244 |  |
|      - | 2245 | `/* Search callback signature */` |
|      - | 2246 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 2247 | `/*` |
|      - | 2248 | ` * Case-insensitive pattern match.` |
|      - | 2249 | ` * Brute force is the default search method used here.` |
|      - | 2250 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 2251 | ` * well for short/medium texts on modern hardware.` |
|      - | 2252 | ` */` |
|    276 | 2253 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      1 | 2254 | `{` |
|    277 | 2255 | `	const char *zpIn = (const char *)pPattern;` |
|    277 | 2256 | `	const char *zIn = (const char *)pText;` |
|    277 | 2257 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    277 | 2258 | `	const char *zEnd = &zIn[nLen];` |
|      - | 2259 | `	const char *zPtr,*zPtr2;` |
|      - | 2260 | `	int c,d;` |
|    277 | 2261 | `	if( iPatLen > nLen ){` |
|      - | 2262 | `		/* Don't bother processing */` |
|     67 | 2263 | `		return SXERR_NOTFOUND;` |
|      - | 2264 | `	}` |
|    783 | 2265 | `	for(;;){` |
|   1567 | 2266 | `		if( zIn >= zEnd ){` |
|    171 | 2267 | `			break;` |
|      - | 2268 | `		}` |
|   1397 | 2269 | `		c = SyToLower(zIn[0]);` |
|   1397 | 2270 | `		d = SyToLower(zpIn[0]);` |
|   1397 | 2271 | `		if( c == d ){` |
|    159 | 2272 | `			zPtr   = &zIn[1];` |
|    159 | 2273 | `			zPtr2  = &zpIn[1];` |
|    130 | 2274 | `			for(;;){` |
|    261 | 2275 | `				if( zPtr2 >= zpEnd ){` |
|      - | 2276 | `					/* Pattern found */` |
|     41 | 2277 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 2278 | `					return SXRET_OK;` |
|      - | 2279 | `				}` |
|    221 | 2280 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 2281 | `					break;` |
|      - | 2282 | `				}` |
|    221 | 2283 | `				c = SyToLower(zPtr[0]);` |
|    221 | 2284 | `				d = SyToLower(zPtr2[0]);` |
|    221 | 2285 | `				if( c != d ){` |
|    119 | 2286 | `					break;` |
|      - | 2287 | `				}` |
|    103 | 2288 | `				zPtr++; zPtr2++;` |
|      1 | 2289 | `			}` |
|     59 | 2290 | `		}` |
|   1357 | 2291 | `		zIn++;` |
|      1 | 2292 | `	}` |
|      - | 2293 | `	/* Pattern not found */` |
|    171 | 2294 | `	return SXERR_NOTFOUND;` |
|    139 | 2295 | `}` |
|      - | 2296 | `/*` |
|      - | 2297 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2298 | ` *  Find the first occurrence of a string.` |
|      - | 2299 | ` * Parameters` |
|      - | 2300 | ` *  $haystack` |
|      - | 2301 | ` *   The input string.` |
|      - | 2302 | ` * $needle` |
|      - | 2303 | ` *   Search pattern (must be a string).` |
|      - | 2304 | ` * $before_needle` |
|      - | 2305 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2306 | ` *   of the needle (excluding the needle).` |
|      - | 2307 | ` * Return` |
|      - | 2308 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2309 | ` */` |
|      6 | 2310 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2311 | `{` |
|      7 | 2312 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2313 | `	const char *zBlob,*zPattern;` |
|      - | 2314 | `	int nLen,nPatLen;` |
|      - | 2315 | `	sxu32 nOfft;` |
|      - | 2316 | `	sxi32 rc;` |
|      7 | 2317 | `	if( nArg < 2 ){` |
|      - | 2318 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2319 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2320 | `		return PH7_OK;` |
|      - | 2321 | `	}` |
|      - | 2322 | `	/* Extract the needle and the haystack */` |
|      7 | 2323 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 2324 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 2325 | `	nOfft = 0; /* cc warning */` |
|      9 | 2326 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 2327 | `		int before = 0;` |
|      - | 2328 | `		/* Perform the lookup */` |
|      5 | 2329 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2330 | `		if( rc != SXRET_OK ){` |
|      - | 2331 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2332 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2333 | `			return PH7_OK;` |
|      - | 2334 | `		}` |
|      - | 2335 | `		/* Return the portion of the string */` |
|      5 | 2336 | `		if( nArg > 2 ){` |
|      3 | 2337 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2338 | `		}` |
|      5 | 2339 | `		if( before ){` |
|      3 | 2340 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2341 | `		}else{` |
|      3 | 2342 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2343 | `		}` |
|      3 | 2344 | `	}else{` |
|      3 | 2345 | `		ph7_result_bool(pCtx,0);` |
|      - | 2346 | `	}` |
|      7 | 2347 | `	return PH7_OK;` |
|      4 | 2348 | `}` |
|      - | 2349 | `/*` |
|      - | 2350 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2351 | ` *  Case-insensitive strstr().` |
|      - | 2352 | ` * Parameters` |
|      - | 2353 | ` *  $haystack` |
|      - | 2354 | ` *   The input string.` |
|      - | 2355 | ` * $needle` |
|      - | 2356 | ` *   Search pattern (must be a string).` |
|      - | 2357 | ` * $before_needle` |
|      - | 2358 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2359 | ` *   of the needle (excluding the needle).` |
|      - | 2360 | ` * Return` |
|      - | 2361 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2362 | ` */` |
|      4 | 2363 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2364 | `{` |
|      5 | 2365 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2366 | `	const char *zBlob,*zPattern;` |
|      - | 2367 | `	int nLen,nPatLen;` |
|      - | 2368 | `	sxu32 nOfft;` |
|      - | 2369 | `	sxi32 rc;` |
|      5 | 2370 | `	if( nArg < 2 ){` |
|      - | 2371 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2372 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2373 | `		return PH7_OK;` |
|      - | 2374 | `	}` |
|      - | 2375 | `	/* Extract the needle and the haystack */` |
|      5 | 2376 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2377 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 2378 | `	nOfft = 0; /* cc warning */` |
|      7 | 2379 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 2380 | `		int before = 0;` |
|      - | 2381 | `		/* Perform the lookup */` |
|      5 | 2382 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2383 | `		if( rc != SXRET_OK ){` |
|      - | 2384 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2385 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2386 | `			return PH7_OK;` |
|      - | 2387 | `		}` |
|      - | 2388 | `		/* Return the portion of the string */` |
|      5 | 2389 | `		if( nArg > 2 ){` |
|      3 | 2390 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2391 | `		}` |
|      5 | 2392 | `		if( before ){` |
|      3 | 2393 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2394 | `		}else{` |
|      3 | 2395 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2396 | `		}` |
|      3 | 2397 | `	}else{` |
|    ! 0 | 2398 | `		ph7_result_bool(pCtx,0);` |
|      - | 2399 | `	}` |
|      5 | 2400 | `	return PH7_OK;` |
|      3 | 2401 | `}` |
|      - | 2402 | `/*` |
|      - | 2403 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2404 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 2405 | ` * Parameters` |
|      - | 2406 | ` *  $haystack` |
|      - | 2407 | ` *   The input string.` |
|      - | 2408 | ` * $needle` |
|      - | 2409 | ` *   Search pattern (must be a string).` |
|      - | 2410 | ` * $offset` |
|      - | 2411 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 2412 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 2413 | ` *   of haystack.` |
|      - | 2414 | ` * Return` |
|      - | 2415 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 2416 | ` */` |
|   1334 | 2417 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2418 | `{` |
|   1339 | 2419 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2420 | `	const char *zBlob,*zPattern;` |
|      - | 2421 | `	int nLen,nPatLen,nStart;` |
|      - | 2422 | `	sxu32 nOfft;` |
|      - | 2423 | `	sxi32 rc;` |
|   1339 | 2424 | `	if( nArg < 2 ){` |
|      - | 2425 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2426 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2427 | `		return PH7_OK;` |
|      - | 2428 | `	}` |
|      - | 2429 | `	/* Extract the needle and the haystack */` |
|   1339 | 2430 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|   1339 | 2431 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|   1339 | 2432 | `	nOfft = 0; /* cc warning */` |
|   1339 | 2433 | `	nStart = 0;` |
|      - | 2434 | `	/* Peek the starting offset if available */` |
|   1339 | 2435 | `	if( nArg > 2 ){` |
|    ! 0 | 2436 | `		nStart = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 2437 | `		if( nStart < 0 ){` |
|    ! 0 | 2438 | `			nStart = -nStart;` |
|    ! 0 | 2439 | `		}` |
|    ! 0 | 2440 | `		if( nStart >= nLen ){` |
|      - | 2441 | `			/* Invalid offset */` |
|    ! 0 | 2442 | `			nStart = 0;` |
|    ! 0 | 2443 | `		}else{` |
|    ! 0 | 2444 | `			zBlob += nStart;` |
|    ! 0 | 2445 | `			nLen -= nStart;` |
|      - | 2446 | `		}` |
|    ! 0 | 2447 | `	}` |
|   1339 | 2448 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2449 | `		/* Perform the lookup */` |
|   1337 | 2450 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|   1337 | 2451 | `		if( rc != SXRET_OK ){` |
|      - | 2452 | `			/* Pattern not found,return FALSE */` |
|    715 | 2453 | `			ph7_result_bool(pCtx,0);` |
|    715 | 2454 | `			return PH7_OK;` |
|      - | 2455 | `		}` |
|      - | 2456 | `		/* Return the pattern position */` |
|    626 | 2457 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|    315 | 2458 | `	}else{` |
|      3 | 2459 | `		ph7_result_bool(pCtx,0);` |
|      - | 2460 | `	}` |
|    628 | 2461 | `	return PH7_OK;` |
|    672 | 2462 | `}` |
|      - | 2463 | `/*` |
|      - | 2464 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|      - | 2465 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|      - | 2466 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|      - | 2467 | ` * TypeError for arrays, resources, and objects without __toString.` |
|      - | 2468 | ` *` |
|      - | 2469 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|      - | 2470 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|      - | 2471 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|      - | 2472 | ` *` |
|      - | 2473 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|      - | 2474 | ` * is valid until pTmp is released or pArg is mutated.` |
|      - | 2475 | ` */` |
|    426 | 2476 | `static sxi32 StrPredicateResolveArg(` |
|      - | 2477 | `	ph7_context *pCtx,` |
|      - | 2478 | `	ph7_value *pArg,` |
|      - | 2479 | `	const char *zFunc,` |
|      - | 2480 | `	int iArgNum,` |
|      - | 2481 | `	const char *zParamName,` |
|      - | 2482 | `	const char *zNullMsg,` |
|      - | 2483 | `	ph7_value *pTmp,` |
|      - | 2484 | `	const char **pzOut,` |
|      - | 2485 | `	int *pnOut` |
|      4 | 2486 | `){` |
|    430 | 2487 | `	if( ph7_value_is_null(pArg) ){` |
|     13 | 2488 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zNullMsg);` |
|     13 | 2489 | `		*pzOut = "";` |
|     13 | 2490 | `		*pnOut = 0;` |
|     13 | 2491 | `		return PH7_OK;` |
|      - | 2492 | `	}` |
|    640 | 2493 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|    396 | 2494 | `	    ( ph7_value_is_object(pArg) &&` |
|     72 | 2495 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|     48 | 2496 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|     24 | 2497 | `	        "__toString",sizeof("__toString")-1) == 0` |
|      - | 2498 | `	    )` |
|      - | 2499 | `	){` |
|     34 | 2500 | `		const char *zType = ph7_type_name(pArg);` |
|     34 | 2501 | `		if( ph7_value_is_object(pArg) ){` |
|     13 | 2502 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     13 | 2503 | `			if( pInst && pInst->pClass ){` |
|     13 | 2504 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      6 | 2505 | `			}` |
|      6 | 2506 | `		}` |
|     49 | 2507 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2508 | `			"TypeError",` |
|      - | 2509 | `			"%s(): Argument #%d (%s) must be of type string, %s given",` |
|     15 | 2510 | `			zFunc, iArgNum, zParamName, zType` |
|      - | 2511 | `			);` |
|      - | 2512 | `	}` |
|    385 | 2513 | `	if( ph7_value_is_object(pArg) ){` |
|     37 | 2514 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     37 | 2515 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 2516 | `			"__toString",sizeof("__toString")-1);` |
|     37 | 2517 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|     37 | 2518 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|     37 | 2519 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|     37 | 2520 | `		return PH7_OK;` |
|      - | 2521 | `	}` |
|    349 | 2522 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|    349 | 2523 | `	return PH7_OK;` |
|    217 | 2524 | `}` |
|      - | 2525 | `/*` |
|      - | 2526 | ` * bool str_contains(string $haystack, string $needle)` |
|      - | 2527 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|      - | 2528 | ` * Return` |
|      - | 2529 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|      - | 2530 | ` */` |
|     96 | 2531 | `static int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2532 | `{` |
|      - | 2533 | `	const char *zHaystack,*zNeedle;` |
|      - | 2534 | `	int nHayLen,nNeedleLen;` |
|      - | 2535 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2536 | `	sxi32 rc;` |
|    100 | 2537 | `	if( nArg != 2 ){` |
|     18 | 2538 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2539 | `			"ArgumentCountError",` |
|      - | 2540 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|      5 | 2541 | `			nArg` |
|      - | 2542 | `			);` |
|      - | 2543 | `	}` |
|     88 | 2544 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     88 | 2545 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     88 | 2546 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack",` |
|      - | 2547 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|      - | 2548 | `		"of type string is deprecated",` |
|      - | 2549 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     88 | 2550 | `	if( rc != PH7_OK ) goto out;` |
|     81 | 2551 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle",` |
|      - | 2552 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|      - | 2553 | `		"of type string is deprecated",` |
|      - | 2554 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     81 | 2555 | `	if( rc != PH7_OK ) goto out;` |
|     77 | 2556 | `	if( nNeedleLen < 1 ){` |
|     13 | 2557 | `		ph7_result_bool(pCtx,1);` |
|     71 | 2558 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2559 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2560 | `	}else{` |
|     85 | 2561 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|     28 | 2562 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|     57 | 2563 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|      - | 2564 | `	}` |
|     77 | 2565 | `	rc = PH7_OK;` |
|     43 | 2566 | `out:` |
|     88 | 2567 | `	PH7_MemObjRelease(&sHayTmp);` |
|     88 | 2568 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     88 | 2569 | `	return rc;` |
|     52 | 2570 | `}` |
|      - | 2571 | `/*` |
|      - | 2572 | ` * bool str_starts_with(string $haystack, string $needle)` |
|      - | 2573 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|      - | 2574 | ` * Return` |
|      - | 2575 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|      - | 2576 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 2577 | ` */` |
|     78 | 2578 | `static int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2579 | `{` |
|      - | 2580 | `	const char *zHaystack,*zNeedle;` |
|      - | 2581 | `	int nHayLen,nNeedleLen;` |
|      - | 2582 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2583 | `	sxi32 rc;` |
|     82 | 2584 | `	if( nArg != 2 ){` |
|     18 | 2585 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2586 | `			"ArgumentCountError",` |
|      - | 2587 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|      5 | 2588 | `			nArg` |
|      - | 2589 | `			);` |
|      - | 2590 | `	}` |
|     70 | 2591 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 2592 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 2593 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack",` |
|      - | 2594 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 2595 | `		"of type string is deprecated",` |
|      - | 2596 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 2597 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 2598 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle",` |
|      - | 2599 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|      - | 2600 | `		"of type string is deprecated",` |
|      - | 2601 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 2602 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 2603 | `	if( nNeedleLen < 1 ){` |
|     13 | 2604 | `		ph7_result_bool(pCtx,1);` |
|     53 | 2605 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2606 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2607 | `	}else{` |
|     58 | 2608 | `		ph7_result_bool(pCtx,` |
|     38 | 2609 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 2610 | `	}` |
|     59 | 2611 | `	rc = PH7_OK;` |
|     34 | 2612 | `out:` |
|     70 | 2613 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 2614 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 2615 | `	return rc;` |
|     43 | 2616 | `}` |
|      - | 2617 | `/*` |
|      - | 2618 | ` * bool str_ends_with(string $haystack, string $needle)` |
|      - | 2619 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|      - | 2620 | ` * Return` |
|      - | 2621 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|      - | 2622 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 2623 | ` */` |
|     78 | 2624 | `static int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2625 | `{` |
|      - | 2626 | `	const char *zHaystack,*zNeedle;` |
|      - | 2627 | `	int nHayLen,nNeedleLen;` |
|      - | 2628 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2629 | `	sxi32 rc;` |
|     82 | 2630 | `	if( nArg != 2 ){` |
|     18 | 2631 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2632 | `			"ArgumentCountError",` |
|      - | 2633 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|      5 | 2634 | `			nArg` |
|      - | 2635 | `			);` |
|      - | 2636 | `	}` |
|     70 | 2637 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 2638 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 2639 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack",` |
|      - | 2640 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 2641 | `		"of type string is deprecated",` |
|      - | 2642 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 2643 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 2644 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle",` |
|      - | 2645 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|      - | 2646 | `		"of type string is deprecated",` |
|      - | 2647 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 2648 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 2649 | `	if( nNeedleLen < 1 ){` |
|     13 | 2650 | `		ph7_result_bool(pCtx,1);` |
|     53 | 2651 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2652 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2653 | `	}else{` |
|     58 | 2654 | `		ph7_result_bool(pCtx,` |
|     38 | 2655 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 2656 | `	}` |
|     59 | 2657 | `	rc = PH7_OK;` |
|     34 | 2658 | `out:` |
|     70 | 2659 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 2660 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 2661 | `	return rc;` |
|     43 | 2662 | `}` |
|      - | 2663 | `/*` |
|      - | 2664 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2665 | ` *  Case-insensitive strpos.` |
|      - | 2666 | ` * Parameters` |
|      - | 2667 | ` *  $haystack` |
|      - | 2668 | ` *   The input string.` |
|      - | 2669 | ` * $needle` |
|      - | 2670 | ` *   Search pattern (must be a string).` |
|      - | 2671 | ` * $offset` |
|      - | 2672 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 2673 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 2674 | ` *   of haystack.` |
|      - | 2675 | ` * Return` |
|      - | 2676 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 2677 | ` */` |
|    174 | 2678 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2679 | `{` |
|    175 | 2680 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2681 | `	const char *zBlob,*zPattern;` |
|      - | 2682 | `	int nLen,nPatLen,nStart;` |
|      - | 2683 | `	sxu32 nOfft;` |
|      - | 2684 | `	sxi32 rc;` |
|    175 | 2685 | `	if( nArg < 2 ){` |
|      - | 2686 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2687 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2688 | `		return PH7_OK;` |
|      - | 2689 | `	}` |
|      - | 2690 | `	/* Extract the needle and the haystack */` |
|    175 | 2691 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    175 | 2692 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    175 | 2693 | `	nOfft = 0; /* cc warning */` |
|    175 | 2694 | `	nStart = 0;` |
|      - | 2695 | `	/* Peek the starting offset if available */` |
|    175 | 2696 | `	if( nArg > 2 ){` |
|      5 | 2697 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 2698 | `		if( nStart < 0 ){` |
|      3 | 2699 | `			nStart = -nStart;` |
|      1 | 2700 | `		}` |
|      5 | 2701 | `		if( nStart >= nLen ){` |
|      - | 2702 | `			/* Invalid offset */` |
|    ! 0 | 2703 | `			nStart = 0;` |
|    ! 0 | 2704 | `		}else{` |
|      5 | 2705 | `			zBlob += nStart;` |
|      5 | 2706 | `			nLen -= nStart;` |
|      - | 2707 | `		}` |
|      2 | 2708 | `	}` |
|    175 | 2709 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2710 | `		/* Perform the lookup */` |
|    175 | 2711 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    175 | 2712 | `		if( rc != SXRET_OK ){` |
|      - | 2713 | `			/* Pattern not found,return FALSE */` |
|    161 | 2714 | `			ph7_result_bool(pCtx,0);` |
|    161 | 2715 | `			return PH7_OK;` |
|      - | 2716 | `		}` |
|      - | 2717 | `		/* Return the pattern position */` |
|     15 | 2718 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 2719 | `	}else{` |
|    ! 0 | 2720 | `		ph7_result_bool(pCtx,0);` |
|      - | 2721 | `	}` |
|     15 | 2722 | `	return PH7_OK;` |
|     88 | 2723 | `}` |
|      - | 2724 | `/*` |
|      - | 2725 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2726 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 2727 | ` * Parameters` |
|      - | 2728 | ` *  $haystack` |
|      - | 2729 | ` *   The input string.` |
|      - | 2730 | ` * $needle` |
|      - | 2731 | ` *   Search pattern (must be a string).` |
|      - | 2732 | ` * $offset` |
|      - | 2733 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 2734 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 2735 | ` *   characters from the end of the string, searching backwards.` |
|      - | 2736 | ` * Return` |
|      - | 2737 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 2738 | ` */` |
|     40 | 2739 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2740 | `{` |
|      - | 2741 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     41 | 2742 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2743 | `	int nLen,nPatLen;` |
|      - | 2744 | `	sxu32 nOfft;` |
|      - | 2745 | `	sxi32 rc;` |
|     41 | 2746 | `	if( nArg < 2 ){` |
|      - | 2747 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2748 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2749 | `		return PH7_OK;` |
|      - | 2750 | `	}` |
|      - | 2751 | `	/* Extract the needle and the haystack */` |
|     41 | 2752 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 2753 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 2754 | `	/* Point to the end of the pattern */` |
|     41 | 2755 | `	zPtr = &zBlob[nLen - 1];` |
|     41 | 2756 | `	zEnd = &zBlob[nLen];` |
|      - | 2757 | `	/* Save the starting posistion */` |
|     41 | 2758 | `	zStart = zBlob;` |
|     41 | 2759 | `	nOfft = 0; /* cc warning */` |
|      - | 2760 | `	/* Peek the starting offset if available */` |
|     41 | 2761 | `	if( nArg > 2 ){` |
|      - | 2762 | `		int nStart;` |
|     21 | 2763 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 2764 | `		if( nStart < 0 ){` |
|     11 | 2765 | `			nStart = -nStart;` |
|     11 | 2766 | `			if( nStart >= nLen ){` |
|      - | 2767 | `				/* Invalid offset */` |
|      3 | 2768 | `				ph7_result_bool(pCtx,0);` |
|      3 | 2769 | `				return PH7_OK;` |
|    ! 0 | 2770 | `			}else{` |
|      9 | 2771 | `				nLen -= nStart;` |
|      9 | 2772 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 2773 | `				zEnd = &zBlob[nLen];` |
|      - | 2774 | `			}` |
|      5 | 2775 | `		}else{` |
|     11 | 2776 | `			if( nStart >= nLen ){` |
|      - | 2777 | `				/* Invalid offset */` |
|      5 | 2778 | `				ph7_result_bool(pCtx,0);` |
|      5 | 2779 | `				return PH7_OK;` |
|    ! 0 | 2780 | `			}else{` |
|      7 | 2781 | `				zBlob += nStart;` |
|      7 | 2782 | `				nLen -= nStart;` |
|      - | 2783 | `			}` |
|      - | 2784 | `		}` |
|      7 | 2785 | `	}` |
|     35 | 2786 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2787 | `		/* Perform the lookup */` |
|    121 | 2788 | `		for(;;){` |
|    243 | 2789 | `			if( zBlob >= zPtr ){` |
|     21 | 2790 | `				break;` |
|      - | 2791 | `			}` |
|    223 | 2792 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    223 | 2793 | `			if( rc == SXRET_OK ){` |
|      - | 2794 | `				/* Pattern found,return it's position */` |
|     13 | 2795 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 2796 | `				return PH7_OK;` |
|      - | 2797 | `			}` |
|    211 | 2798 | `			zPtr--;` |
|      1 | 2799 | `		}` |
|      - | 2800 | `		/* Pattern not found,return FALSE */` |
|     21 | 2801 | `		ph7_result_bool(pCtx,0);` |
|     11 | 2802 | `	}else{` |
|      3 | 2803 | `		ph7_result_bool(pCtx,0);` |
|      - | 2804 | `	}` |
|     23 | 2805 | `	return PH7_OK;` |
|     21 | 2806 | `}` |
|      - | 2807 | `/*` |
|      - | 2808 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2809 | ` *  Case-insensitive strrpos.` |
|      - | 2810 | ` * Parameters` |
|      - | 2811 | ` *  $haystack` |
|      - | 2812 | ` *   The input string.` |
|      - | 2813 | ` * $needle` |
|      - | 2814 | ` *   Search pattern (must be a string).` |
|      - | 2815 | ` * $offset` |
|      - | 2816 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 2817 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 2818 | ` *   characters from the end of the string, searching backwards.` |
|      - | 2819 | ` * Return` |
|      - | 2820 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 2821 | ` */` |
|     26 | 2822 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2823 | `{` |
|      - | 2824 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     27 | 2825 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2826 | `	int nLen,nPatLen;` |
|      - | 2827 | `	sxu32 nOfft;` |
|      - | 2828 | `	sxi32 rc;` |
|     27 | 2829 | `	if( nArg < 2 ){` |
|      - | 2830 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2831 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2832 | `		return PH7_OK;` |
|      - | 2833 | `	}` |
|      - | 2834 | `	/* Extract the needle and the haystack */` |
|     27 | 2835 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 2836 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 2837 | `	/* Point to the end of the pattern */` |
|     27 | 2838 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 2839 | `	zEnd = &zBlob[nLen];` |
|      - | 2840 | `	/* Save the starting posistion */` |
|     27 | 2841 | `	zStart = zBlob;` |
|     27 | 2842 | `	nOfft = 0; /* cc warning */` |
|      - | 2843 | `	/* Peek the starting offset if available */` |
|     27 | 2844 | `	if( nArg > 2 ){` |
|      - | 2845 | `		int nStart;` |
|     15 | 2846 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 2847 | `		if( nStart < 0 ){` |
|      7 | 2848 | `			nStart = -nStart;` |
|      7 | 2849 | `			if( nStart >= nLen ){` |
|      - | 2850 | `				/* Invalid offset */` |
|      3 | 2851 | `				ph7_result_bool(pCtx,0);` |
|      3 | 2852 | `				return PH7_OK;` |
|    ! 0 | 2853 | `			}else{` |
|      5 | 2854 | `				nLen -= nStart;` |
|      5 | 2855 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 2856 | `				zEnd = &zBlob[nLen];` |
|      - | 2857 | `			}` |
|      3 | 2858 | `		}else{` |
|      9 | 2859 | `			if( nStart >= nLen ){` |
|      - | 2860 | `				/* Invalid offset */` |
|      5 | 2861 | `				ph7_result_bool(pCtx,0);` |
|      5 | 2862 | `				return PH7_OK;` |
|    ! 0 | 2863 | `			}else{` |
|      5 | 2864 | `				zBlob += nStart;` |
|      5 | 2865 | `				nLen -= nStart;` |
|      - | 2866 | `			}` |
|      - | 2867 | `		}` |
|      4 | 2868 | `	}` |
|     21 | 2869 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2870 | `		/* Perform the lookup */` |
|     44 | 2871 | `		for(;;){` |
|     89 | 2872 | `			if( zBlob >= zPtr ){` |
|      9 | 2873 | `				break;` |
|      - | 2874 | `			}` |
|     81 | 2875 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 2876 | `			if( rc == SXRET_OK ){` |
|      - | 2877 | `				/* Pattern found,return it's position */` |
|     11 | 2878 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 2879 | `				return PH7_OK;` |
|      - | 2880 | `			}` |
|     71 | 2881 | `			zPtr--;` |
|      1 | 2882 | `		}` |
|      - | 2883 | `		/* Pattern not found,return FALSE */` |
|      9 | 2884 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2885 | `	}else{` |
|      3 | 2886 | `		ph7_result_bool(pCtx,0);` |
|      - | 2887 | `	}` |
|     11 | 2888 | `	return PH7_OK;` |
|     14 | 2889 | `}` |
|      - | 2890 | `/*` |
|      - | 2891 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 2892 | ` *  Find the last occurrence of a character in a string.` |
|      - | 2893 | ` * Parameters` |
|      - | 2894 | ` *  $haystack` |
|      - | 2895 | ` *   The input string.` |
|      - | 2896 | ` * $needle` |
|      - | 2897 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 2898 | ` *  This behavior is different from that of strstr().` |
|      - | 2899 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 2900 | ` *  as the ordinal value of a character.` |
|      - | 2901 | ` * Return` |
|      - | 2902 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 2903 | ` */` |
|     22 | 2904 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2905 | `{` |
|      - | 2906 | `	const char *zBlob;` |
|      - | 2907 | `	int nLen,c;` |
|     23 | 2908 | `	if( nArg < 2 ){` |
|      - | 2909 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2910 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2911 | `		return PH7_OK;` |
|      - | 2912 | `	}` |
|      - | 2913 | `	/* Extract the haystack */` |
|     23 | 2914 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 2915 | `	c = 0; /* cc warning */` |
|     23 | 2916 | `	if( nLen > 0 ){` |
|      - | 2917 | `		sxu32 nOfft;` |
|      - | 2918 | `		sxi32 rc;` |
|     21 | 2919 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 2920 | `			const char *zPattern;` |
|     11 | 2921 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 2922 | `														 * for NULL pointer.` |
|      - | 2923 | `														 */` |
|     11 | 2924 | `			c = zPattern[0];` |
|      6 | 2925 | `		}else{` |
|      - | 2926 | `			/* Int cast */` |
|     11 | 2927 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 2928 | `		}` |
|      - | 2929 | `		/* Perform the lookup */` |
|     21 | 2930 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 2931 | `		if( rc != SXRET_OK ){` |
|      - | 2932 | `			/* No such entry,return FALSE */` |
|      7 | 2933 | `			ph7_result_bool(pCtx,0);` |
|      7 | 2934 | `			return PH7_OK;` |
|      - | 2935 | `		}` |
|      - | 2936 | `		/* Return the string portion */` |
|     15 | 2937 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 2938 | `	}else{` |
|      3 | 2939 | `		ph7_result_bool(pCtx,0);` |
|      - | 2940 | `	}` |
|     17 | 2941 | `	return PH7_OK;` |
|     12 | 2942 | `}` |
|      - | 2943 | `/*` |
|      - | 2944 | ` * string strrev(string $string)` |
|      - | 2945 | ` *  Reverse a string.` |
|      - | 2946 | ` * Parameters` |
|      - | 2947 | ` *  $string` |
|      - | 2948 | ` *   String to be reversed.` |
|      - | 2949 | ` * Return` |
|      - | 2950 | ` *  The reversed string.` |
|      - | 2951 | ` */` |
|      2 | 2952 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2953 | `{` |
|      - | 2954 | `	const char *zIn,*zEnd;` |
|      - | 2955 | `	int nLen,c;` |
|      3 | 2956 | `	if( nArg < 1 ){` |
|      - | 2957 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 2958 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2959 | `		return PH7_OK;` |
|      - | 2960 | `	}` |
|      - | 2961 | `	/* Extract the target string */` |
|      3 | 2962 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 2963 | `	if( nLen < 1 ){` |
|      - | 2964 | `		/* Empty string Return null */` |
|    ! 0 | 2965 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2966 | `		return PH7_OK;` |
|      - | 2967 | `	}` |
|      - | 2968 | `	/* Perform the requested operation */` |
|      3 | 2969 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 2970 | `	for(;;){` |
|      9 | 2971 | `		if( zEnd < zIn ){` |
|      - | 2972 | `			/* No more input to process */` |
|      3 | 2973 | `			break;` |
|      - | 2974 | `		}` |
|      - | 2975 | `		/* Append current character */` |
|      7 | 2976 | `		c = zEnd[0];` |
|      7 | 2977 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 2978 | `		zEnd--;` |
|      1 | 2979 | `	}` |
|      3 | 2980 | `	return PH7_OK;` |
|      2 | 2981 | `}` |
|      - | 2982 | `/*` |
|      - | 2983 | ` * string ucwords(string $string [, string $separators = " \t\r\n\f\v"])` |
|      - | 2984 | ` *  Uppercase the first character of each word in a string.` |
|      - | 2985 | ` *  A word begins at the start of the string and after any character present in` |
|      - | 2986 | ` *  $separators. The default separators are the whitespace characters (space,` |
|      - | 2987 | ` *  horizontal tab, carriage return, newline, form-feed and vertical tab); an` |
|      - | 2988 | ` *  explicit $separators argument REPLACES them (an empty string leaves only the` |
|      - | 2989 | ` *  very first character upper-cased). Like PHP, this is byte-based: only ASCII` |
|      - | 2990 | ` *  bytes are upper-cased and a byte is a separator only if it appears in the set.` |
|      - | 2991 | ` * Parameters` |
|      - | 2992 | ` *  $string` |
|      - | 2993 | ` *   The input string.` |
|      - | 2994 | ` *  $separators` |
|      - | 2995 | ` *   The optional word-boundary characters.` |
|      - | 2996 | ` * Return` |
|      - | 2997 | ` *  The modified string.` |
|      - | 2998 | ` */` |
|     22 | 2999 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3000 | `{` |
|      - | 3001 | `	const char *zIn;` |
|      - | 3002 | `	int nLen,i,iStart;` |
|      - | 3003 | `	char aDelim[256];` |
|     23 | 3004 | `	if( nArg < 1 ){` |
|      - | 3005 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3006 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3007 | `		return PH7_OK;` |
|      - | 3008 | `	}` |
|      - | 3009 | `	/* Build the separator membership table: an explicit $separators argument` |
|      - | 3010 | `	 * replaces the default whitespace set (an empty string clears it). */` |
|     23 | 3011 | `	SyZero(aDelim,(sxu32)sizeof(aDelim));` |
|     23 | 3012 | `	if( nArg > 1 ){` |
|      - | 3013 | `		int nDelim;` |
|      9 | 3014 | `		const char *zDelim = ph7_value_to_string(apArg[1],&nDelim);` |
|     17 | 3015 | `		for( i = 0 ; i < nDelim ; i++ ){` |
|      9 | 3016 | `			aDelim[(unsigned char)zDelim[i]] = 1;` |
|      5 | 3017 | `		}` |
|      5 | 3018 | `	}else{` |
|     15 | 3019 | `		aDelim[(unsigned char)' ']  = 1;` |
|     15 | 3020 | `		aDelim[(unsigned char)'\t'] = 1;` |
|     15 | 3021 | `		aDelim[(unsigned char)'\r'] = 1;` |
|     15 | 3022 | `		aDelim[(unsigned char)'\n'] = 1;` |
|     15 | 3023 | `		aDelim[(unsigned char)'\f'] = 1;` |
|     15 | 3024 | `		aDelim[(unsigned char)'\v'] = 1;` |
|      - | 3025 | `	}` |
|      - | 3026 | `	/* Extract the target string */` |
|     23 | 3027 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3028 | `	if( nLen < 1 ){` |
|      - | 3029 | `		/* Empty string – match PHP semantics */` |
|      3 | 3030 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3031 | `		return PH7_OK;` |
|      - | 3032 | `	}` |
|      - | 3033 | `	/* Upper-case the first byte of each word (the leading byte, or any byte that` |
|      - | 3034 | `	 * follows a separator), appending the untouched runs in between verbatim. */` |
|     21 | 3035 | `	iStart = 0;` |
|    309 | 3036 | `	for( i = 0 ; i < nLen ; i++ ){` |
|    289 | 3037 | `		int c = (unsigned char)zIn[i];` |
|    289 | 3038 | `		if( (i == 0 \|\| aDelim[(unsigned char)zIn[i-1]]) && c < 0x80 && SyisLower(c) ){` |
|     53 | 3039 | `			char up = (char)SyToUpper(c);` |
|     53 | 3040 | `			if( i > iStart ){` |
|     35 | 3041 | `				ph7_result_string(pCtx,&zIn[iStart],i - iStart);` |
|     17 | 3042 | `			}` |
|     53 | 3043 | `			ph7_result_string(pCtx,&up,1);` |
|     53 | 3044 | `			iStart = i + 1;` |
|     26 | 3045 | `		}` |
|    145 | 3046 | `	}` |
|     21 | 3047 | `	if( nLen > iStart ){` |
|     21 | 3048 | `		ph7_result_string(pCtx,&zIn[iStart],nLen - iStart);` |
|     10 | 3049 | `	}` |
|     21 | 3050 | `	return PH7_OK;` |
|     12 | 3051 | `}` |
|      - | 3052 | `/*` |
|      - | 3053 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3054 | ` *  Returns input repeated multiplier times.` |
|      - | 3055 | ` * Parameters` |
|      - | 3056 | ` *  $string` |
|      - | 3057 | ` *   String to be repeated.` |
|      - | 3058 | ` * $multiplier` |
|      - | 3059 | ` *  Number of time the input string should be repeated.` |
|      - | 3060 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3061 | ` *  to 0, the function will return an empty string.` |
|      - | 3062 | ` * Return` |
|      - | 3063 | ` *  The repeated string.` |
|      - | 3064 | ` */` |
|  20430 | 3065 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3066 | `{` |
|      - | 3067 | `	const char *zIn;` |
|      - | 3068 | `	int nLen;` |
|      - | 3069 | `	ph7_int64 nMul;` |
|      - | 3070 | `	int rc;` |
|  20432 | 3071 | `	if( nArg < 2 ){` |
|      - | 3072 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3073 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3074 | `		return PH7_OK;` |
|      - | 3075 | `	}` |
|      - | 3076 | `	/* Extract the target string */` |
|  20432 | 3077 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3078 | `	/* Extract the multiplier as a 64-bit value (a 32-bit read would wrap a large` |
|      - | 3079 | `	 * positive $times into a negative one and trip a spurious ValueError). PHP` |
|      - | 3080 | `	 * validates $times regardless of the string contents: a negative count throws` |
|      - | 3081 | `	 * a catchable ValueError. */` |
|  20432 | 3082 | `	nMul = ph7_value_to_int64(apArg[1]);` |
|  20432 | 3083 | `	if( nMul < 0 ){` |
|      3 | 3084 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 3085 | `			"str_repeat(): Argument #2 ($times) must be greater than or equal to 0");` |
|      - | 3086 | `	}` |
|  20430 | 3087 | `	if( nLen < 1 \|\| nMul < 1 ){` |
|      - | 3088 | `		/* Empty input or a zero multiplier yields the empty string (PHP). */` |
|    ! 0 | 3089 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3090 | `		return PH7_OK;` |
|      - | 3091 | `	}` |
|      - | 3092 | `	/* Perform the requested operation */` |
| 221628 | 3093 | `	for(;;){` |
| 443258 | 3094 | `		if( !nMul ){` |
|  20430 | 3095 | `			break;` |
|      - | 3096 | `		}` |
|      - | 3097 | `		/* Append the copy */` |
| 422830 | 3098 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 422830 | 3099 | `		if( rc != PH7_OK ){` |
|      - | 3100 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 3101 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 3102 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 3103 | `		}` |
| 422830 | 3104 | `		nMul--;` |
|      2 | 3105 | `	}` |
|  20430 | 3106 | `	return PH7_OK;` |
|  10217 | 3107 | `}` |
|      - | 3108 | `/*` |
|      - | 3109 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3110 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3111 | ` * Parameters` |
|      - | 3112 | ` *  $string` |
|      - | 3113 | ` *   The input string.` |
|      - | 3114 | ` * $is_xhtml` |
|      - | 3115 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3116 | ` * Return` |
|      - | 3117 | ` *  The processed string.` |
|      - | 3118 | ` */` |
|      4 | 3119 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3120 | `{` |
|      - | 3121 | `	const char *zIn,*zCur,*zEnd;` |
|      5 | 3122 | `	int is_xhtml = 1; /* Default to XHTML-style '<br />' like PHP */` |
|      - | 3123 | `	int nLen;` |
|      5 | 3124 | `	if( nArg < 1 ){` |
|      - | 3125 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 3126 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3127 | `		return PH7_OK;` |
|      - | 3128 | `	}` |
|      - | 3129 | `	/* Extract the target string */` |
|      5 | 3130 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3131 | `	if( nLen < 1 ){` |
|      - | 3132 | `		/* Empty string,return null */` |
|    ! 0 | 3133 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3134 | `		return PH7_OK;` |
|      - | 3135 | `	}` |
|      5 | 3136 | `	if( nArg > 1 ){` |
|      3 | 3137 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3138 | `	}` |
|      5 | 3139 | `	zEnd = &zIn[nLen];` |
|      - | 3140 | `	/* Perform the requested operation */` |
|      4 | 3141 | `	for(;;){` |
|      9 | 3142 | `		zCur = zIn;` |
|      - | 3143 | `		/* Delimit the string */` |
|     21 | 3144 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3145 | `			zIn++;` |
|      1 | 3146 | `		}` |
|      9 | 3147 | `		if( zCur < zIn ){` |
|      - | 3148 | `			/* Output chunk verbatim */` |
|      9 | 3149 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3150 | `		}` |
|      9 | 3151 | `		if( zIn >= zEnd ){` |
|      - | 3152 | `			/* No more input to process */` |
|      5 | 3153 | `			break;` |
|      - | 3154 | `		}` |
|      - | 3155 | `		/* Output the HTML line break */` |
|      - | 3156 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br />' (space before the slash), otherwise use '<br>' */` |
|      5 | 3157 | `		if( is_xhtml ){` |
|      3 | 3158 | `			ph7_result_string(pCtx,"<br />",(int)sizeof("<br />")-1);` |
|      2 | 3159 | `		}else{` |
|      3 | 3160 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3161 | `		}` |
|      5 | 3162 | `		zCur = zIn;` |
|      - | 3163 | `		/* Append trailing line */` |
|     11 | 3164 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3165 | `			zIn++;` |
|      1 | 3166 | `		}` |
|      5 | 3167 | `		if( zCur < zIn ){` |
|      - | 3168 | `			/* Output chunk verbatim */` |
|      5 | 3169 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 3170 | `		}` |
|      1 | 3171 | `	}` |
|      5 | 3172 | `	return PH7_OK;` |
|      3 | 3173 | `}` |
|      - | 3174 | `/*` |
|      - | 3175 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3176 | ` *  According to the PHP reference manual.` |
|      - | 3177 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3178 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3179 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 3180 | ` * This applies to both sprintf() and printf().` |
|      - | 3181 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 3182 | ` * or more of these elements, in order:` |
|      - | 3183 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 3184 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 3185 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 3186 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 3187 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 3188 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 3189 | ` *   it with a single quote ('). See the examples below.` |
|      - | 3190 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 3191 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 3192 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 3193 | ` *   should result in.` |
|      - | 3194 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 3195 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 3196 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 3197 | ` *   limit to the string.` |
|      - | 3198 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 3199 | ` *       % - a literal percent character. No argument is required.` |
|      - | 3200 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 3201 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 3202 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 3203 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 3204 | ` * 	     for the number of digits after the decimal point.` |
|      - | 3205 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 3206 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 3207 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 3208 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 3209 | ` *       g - shorter of %e and %f.` |
|      - | 3210 | ` *       G - shorter of %E and %f.` |
|      - | 3211 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 3212 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 3213 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 3214 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 3215 | ` */` |
|      - | 3216 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3217 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 3218 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 3219 | `/*` |
|      - | 3220 | `** Conversion types fall into various categories as defined by the` |
|      - | 3221 | `** following enumeration.` |
|      - | 3222 | `*/` |
|      - | 3223 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 3224 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 3225 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 3226 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 3227 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 3228 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 3229 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 3230 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 3231 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 3232 |  |
|      - | 3233 | `/*` |
|      - | 3234 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 3235 | `*/` |
|      - | 3236 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 3237 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 3238 | `/*` |
|      - | 3239 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 3240 | `** by an instance of the following structure` |
|      - | 3241 | `*/` |
|      - | 3242 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 3243 | `struct ph7_fmt_info` |
|      - | 3244 | `{` |
|      - | 3245 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 3246 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 3247 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 3248 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 3249 | `  char *charset; /* The character set for conversion */` |
|      - | 3250 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 3251 | `};` |
|      - | 3252 | `/* PH7_PhpFloatShape (php's float-shape post-processing) lives in memobj.c —` |
|      - | 3253 | ` * the default float->string cast needs it even when this whole formatting` |
|      - | 3254 | ` * region is compiled out by PH7_DISABLE_DISK_IO. */` |
|      - | 3255 | `/*` |
|      - | 3256 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 3257 | ` * used conversion types first.` |
|      - | 3258 | ` */` |
|      - | 3259 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 3260 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 3261 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 3262 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 3263 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 3264 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 3265 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 3266 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 3267 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 3268 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3269 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3270 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 3271 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 3272 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 3273 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 3274 | `  /* php's 'h'/'H' are the locale-independent twins of 'g'/'G'; PHL always` |
|      - | 3275 | `   * formats in the C locale, so they behave identically. */` |
|      - | 3276 | `  {  'h',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 3277 | `  {  'H',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 3278 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 3279 | `};` |
|      - | 3280 | `/*` |
|      - | 3281 | ` * PHP 8 raises a catchable ValueError for an unknown conversion specifier` |
|      - | 3282 | ` * (e.g. "%y", or the C-ism "%#x" — '#' is not a php flag). Because printf()` |
|      - | 3283 | ` * and fprintf() stream their output incrementally while sprintf() buffers it,` |
|      - | 3284 | ` * every format builtin calls PH7_FormatValidate (below) to check the whole` |
|      - | 3285 | ` * format string BEFORE formatting so the throw happens with no partial output` |
|      - | 3286 | ` * escaping (php buffers the entire result and only emits it on success). This` |
|      - | 3287 | ` * scan mirrors the specifier-locating logic of the main format loop below.` |
|      - | 3288 | ` * On the first unknown specifier, stores it in *pBad and returns TRUE; returns` |
|      - | 3289 | ` * FALSE when every specifier is known. (A found-flag rather than a sentinel` |
|      - | 3290 | ` * char, so a NUL specifier byte — "%\0" — is still reported, not mistaken for` |
|      - | 3291 | ` * "all valid".)` |
|      - | 3292 | ` */` |
|    334 | 3293 | `static int FormatUnknownSpec(const char *zIn,int nByte,int *pBad)` |
|      1 | 3294 | `{` |
|    335 | 3295 | `	const char *zEnd = &zIn[nByte];` |
|      - | 3296 | `	int c,idx;` |
|   3165 | 3297 | `	while( zIn < zEnd ){` |
|   2851 | 3298 | `		if( zIn[0] != '%' ){` |
|   2201 | 3299 | `			zIn++;` |
|   2201 | 3300 | `			continue;` |
|      - | 3301 | `		}` |
|    651 | 3302 | `		zIn++; /* jump the percent sign */` |
|      - | 3303 | `		/* php-supported flags: '-', '+', ' ', '0' and the "'<pad>'" custom-pad` |
|      - | 3304 | `		 * form. '#' is intentionally NOT treated as a flag so it surfaces as an` |
|      - | 3305 | `		 * unknown specifier, matching php. */` |
|    693 | 3306 | `		while( zIn < zEnd ){` |
|    691 | 3307 | `			c = zIn[0];` |
|    691 | 3308 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|     43 | 3309 | `				zIn++;` |
|     43 | 3310 | `				continue;` |
|      - | 3311 | `			}` |
|    649 | 3312 | `			if( c=='\'' ){` |
|    ! 0 | 3313 | `				zIn++;` |
|    ! 0 | 3314 | `				if( zIn < zEnd ){` |
|    ! 0 | 3315 | `					zIn++; /* the custom pad character */` |
|    ! 0 | 3316 | `				}` |
|    ! 0 | 3317 | `				continue;` |
|      - | 3318 | `			}` |
|    649 | 3319 | `			break;` |
|    ! 0 | 3320 | `		}` |
|      - | 3321 | `		/* field width */` |
|    725 | 3322 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|     75 | 3323 | `			zIn++;` |
|      1 | 3324 | `		}` |
|      - | 3325 | `		/* positional specifier ($) — php parses flags AFTER it (e.g. "%1$-10s"),` |
|      - | 3326 | `		 * so skip the full flag set and width again, mirroring the main loop. */` |
|    651 | 3327 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
|    ! 0 | 3328 | `			zIn++;` |
|    ! 0 | 3329 | `			while( zIn < zEnd ){` |
|    ! 0 | 3330 | `				c = zIn[0];` |
|    ! 0 | 3331 | `				if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|    ! 0 | 3332 | `					zIn++;` |
|    ! 0 | 3333 | `					continue;` |
|      - | 3334 | `				}` |
|    ! 0 | 3335 | `				if( c=='\'' ){` |
|    ! 0 | 3336 | `					zIn++;` |
|    ! 0 | 3337 | `					if( zIn < zEnd ){` |
|    ! 0 | 3338 | `						zIn++;` |
|    ! 0 | 3339 | `					}` |
|    ! 0 | 3340 | `					continue;` |
|      - | 3341 | `				}` |
|    ! 0 | 3342 | `				break;` |
|    ! 0 | 3343 | `			}` |
|    ! 0 | 3344 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    ! 0 | 3345 | `				zIn++;` |
|    ! 0 | 3346 | `			}` |
|    ! 0 | 3347 | `		}` |
|      - | 3348 | `		/* precision */` |
|    651 | 3349 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|     87 | 3350 | `			zIn++;` |
|    183 | 3351 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|     97 | 3352 | `				zIn++;` |
|      1 | 3353 | `			}` |
|     43 | 3354 | `		}` |
|      - | 3355 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|    651 | 3356 | `		if( zIn < zEnd && zIn[0]=='l' ){` |
|     11 | 3357 | `			zIn++;` |
|      5 | 3358 | `		}` |
|    651 | 3359 | `		if( zIn >= zEnd ){` |
|      - | 3360 | `			/* A dangling '%' with no specifier: PHL's legacy path silently` |
|      - | 3361 | `			 * truncates here (recorded residual); nothing to validate. */` |
|      3 | 3362 | `			break;` |
|      - | 3363 | `		}` |
|    649 | 3364 | `		c = zIn[0];` |
|    649 | 3365 | `		zIn++; /* jump the conversion specifier */` |
|   3191 | 3366 | `		for( idx = 0 ; idx < (int)SX_ARRAYSIZE(aFmt) ; idx++ ){` |
|   3173 | 3367 | `			if( c == aFmt[idx].fmttype ){` |
|    631 | 3368 | `				break;` |
|      - | 3369 | `			}` |
|   1272 | 3370 | `		}` |
|    649 | 3371 | `		if( idx >= (int)SX_ARRAYSIZE(aFmt) ){` |
|     19 | 3372 | `			*pBad = c; /* unknown specifier */` |
|     19 | 3373 | `			return TRUE;` |
|      - | 3374 | `		}` |
|      1 | 3375 | `	}` |
|    317 | 3376 | `	return FALSE;` |
|    168 | 3377 | `}` |
|      - | 3378 | `/*` |
|      - | 3379 | ` * Validate a printf-style format string. PHP 8 raises a catchable ValueError for` |
|      - | 3380 | ` * an unknown conversion specifier, thrown before any output is produced. Every` |
|      - | 3381 | ` * format builtin (sprintf/printf/vprintf/vsprintf/fprintf/vfprintf) calls this` |
|      - | 3382 | ` * up-front, then propagates the returned status verbatim (PH7_EXCEPTION when the` |
|      - | 3383 | ` * throw is caught in place, PH7_ABORT when it goes uncaught).` |
|      - | 3384 | ` * Returns PH7_OK when the format is valid.` |
|      - | 3385 | ` */` |
|    334 | 3386 | `PH7_PRIVATE sxi32 PH7_FormatValidate(ph7_context *pCtx,const char *zFormat,int nByte)` |
|      1 | 3387 | `{` |
|    335 | 3388 | `	int badSpec = 0;` |
|    335 | 3389 | `	if( FormatUnknownSpec(zFormat,nByte,&badSpec) ){` |
|     28 | 3390 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      9 | 3391 | `			"Unknown format specifier \"%c\"",badSpec);` |
|      - | 3392 | `	}` |
|    317 | 3393 | `	return PH7_OK;` |
|    168 | 3394 | `}` |
|      - | 3395 | `/*` |
|      - | 3396 | `` * PHP 8: a printf-family `$format` argument is a `string` parameter — scalars`` |
|      - | 3397 | ` * (int/float/bool) and null coerce to a string, but an array/object/resource` |
|      - | 3398 | ` * raises a catchable TypeError. iArg is the 1-based argument position ($format` |
|      - | 3399 | ` * is #1 for sprintf/printf/vprintf/vsprintf, #2 for fprintf/vfprintf). Returns` |
|      - | 3400 | ` * PH7_OK when the value is string-coercible (the caller then uses` |
|      - | 3401 | ` * ph7_value_to_string, which renders scalars/null verbatim).` |
|      - | 3402 | ` */` |
|    354 | 3403 | `PH7_PRIVATE sxi32 PH7_FormatCheckFormatArg(ph7_context *pCtx,ph7_value *pArg,int iArg)` |
|      1 | 3404 | `{` |
|    355 | 3405 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg) \|\| ph7_value_is_resource(pArg) ){` |
|      - | 3406 | `		char zBuf[64];` |
|     13 | 3407 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 3408 | `			"%s(): Argument #%d ($format) must be of type string, %s given",` |
|      4 | 3409 | `			ph7_function_name(pCtx),iArg,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|      - | 3410 | `	}` |
|    347 | 3411 | `	return PH7_OK;` |
|    178 | 3412 | `}` |
|      - | 3413 | `/*` |
|      - | 3414 | ` * Format a given string.` |
|      - | 3415 | ` * The root program.  All variations call this core.` |
|      - | 3416 | ` * INPUTS:` |
|      - | 3417 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 3418 | ` *            1. A pointer to the call context.` |
|      - | 3419 | ` *            2. A pointer to the list of characters to be output` |
|      - | 3420 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 3421 | ` *            3. An integer number of characters to be output.` |
|      - | 3422 | ` *               (Note: This number might be zero.)` |
|      - | 3423 | ` *            4. Upper layer private data.` |
|      - | 3424 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 3425 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 3426 | ` */` |
|    316 | 3427 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 3428 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 3429 | `	ph7_context *pCtx,  /* call context */` |
|      - | 3430 | `	const char *zIn,    /* Format string */` |
|      - | 3431 | `	int nByte,          /* Format string length */` |
|      - | 3432 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 3433 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 3434 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 3435 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 3436 | `	)` |
|      1 | 3437 | `{` |
|    317 | 3438 | `	char spaces[] = "                                                  ";` |
|      - | 3439 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    317 | 3440 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 3441 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 3442 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 3443 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 3444 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 3445 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 3446 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 3447 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 3448 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 3449 | `	ph7_int64 iVal;` |
|      - | 3450 | `	int precision;           /* Precision of the current field */` |
|      - | 3451 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 3452 | `	int c,rc,n;` |
|      - | 3453 | `	int length;              /* Length of the field */` |
|      - | 3454 | `	int prefix;` |
|      - | 3455 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 3456 | `	int width;               /* Width of the current field */` |
|      - | 3457 | `	int idx;` |
|    317 | 3458 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 3459 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 3460 | `	/* An unknown conversion specifier is rejected up-front by PH7_FormatValidate()` |
|      - | 3461 | `	 * (called by every format builtin before this routine), so the specifier set` |
|      - | 3462 | `	 * seen here is always valid. */` |
|      - | 3463 | `	/* Start the format process */` |
|    473 | 3464 | `	for(;;){` |
|    947 | 3465 | `		zCur = zIn;` |
|   3133 | 3466 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|   2187 | 3467 | `			zIn++;` |
|      1 | 3468 | `		}` |
|    947 | 3469 | `		if( zCur < zIn ){` |
|      - | 3470 | `			/* Consume chunk verbatim */` |
|    661 | 3471 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|    661 | 3472 | `			if( rc != SXRET_OK ){` |
|      - | 3473 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|    ! 0 | 3474 | `				break;` |
|      - | 3475 | `			}` |
|    330 | 3476 | `		}` |
|    947 | 3477 | `		if( zIn >= zEnd ){` |
|      - | 3478 | `			/* No more input to process,break immediately */` |
|    315 | 3479 | `			break;` |
|      - | 3480 | `		}` |
|      - | 3481 | `		/* Find out what flags are present */` |
|    633 | 3482 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    632 | 3483 | `			flag_alternateform = flag_zeropad = 0;` |
|    633 | 3484 | `		zIn++; /* Jump the precent sign */` |
|    316 | 3485 | `		do{` |
|    675 | 3486 | `			c = zIn[0];` |
|    675 | 3487 | `			switch( c ){` |
|     15 | 3488 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      7 | 3489 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 3490 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|     17 | 3491 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 3492 | `			case '\'':` |
|    ! 0 | 3493 | `				zIn++;` |
|    ! 0 | 3494 | `				if( zIn < zEnd ){` |
|      - | 3495 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 3496 | `					c = zIn[0];` |
|    ! 0 | 3497 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3498 | `						spaces[idx] = (char)c;` |
|    ! 0 | 3499 | `					}` |
|    ! 0 | 3500 | `					c = 0;` |
|    ! 0 | 3501 | `				}` |
|    ! 0 | 3502 | `				break;` |
|    632 | 3503 | `			default:                                       break;` |
|      - | 3504 | `			}` |
|    675 | 3505 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 3506 | `		/* Get the field width */` |
|    633 | 3507 | `		width = 0;` |
|   1023 | 3508 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     75 | 3509 | `			width = width*10 + (zIn[0] - '0');` |
|     75 | 3510 | `			zIn++;` |
|      1 | 3511 | `		}` |
|    633 | 3512 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 3513 | `			/* Position specifer */` |
|    ! 0 | 3514 | `			if( width > 0 ){` |
|    ! 0 | 3515 | `				n = width;` |
|    ! 0 | 3516 | `				if( vf && n > 0 ){` |
|    ! 0 | 3517 | `					n--;` |
|    ! 0 | 3518 | `				}` |
|    ! 0 | 3519 | `			}` |
|    ! 0 | 3520 | `			zIn++;` |
|    ! 0 | 3521 | `			width = 0;` |
|      - | 3522 | `			/* php's grammar is %argnum$<flags><width>: the flags come AFTER the` |
|      - | 3523 | `			 * positional, so re-parse the full flag set here (e.g. "%1$-10s"),` |
|      - | 3524 | `			 * not just zero-padding. */` |
|    ! 0 | 3525 | `			do{` |
|    ! 0 | 3526 | `				c = zIn[0];` |
|    ! 0 | 3527 | `				switch( c ){` |
|    ! 0 | 3528 | `				case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|    ! 0 | 3529 | `				case '+':   flag_plussign = 1;        c = 0;   break;` |
|    ! 0 | 3530 | `				case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|    ! 0 | 3531 | `				case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 3532 | `				case '\'':` |
|    ! 0 | 3533 | `					zIn++;` |
|    ! 0 | 3534 | `					if( zIn < zEnd ){` |
|    ! 0 | 3535 | `						c = zIn[0];` |
|    ! 0 | 3536 | `						for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3537 | `							spaces[idx] = (char)c;` |
|    ! 0 | 3538 | `						}` |
|    ! 0 | 3539 | `						c = 0;` |
|    ! 0 | 3540 | `					}` |
|    ! 0 | 3541 | `					break;` |
|    ! 0 | 3542 | `				default:                                       break;` |
|      - | 3543 | `				}` |
|    ! 0 | 3544 | `			}while( c==0 && (zIn++ < zEnd) );` |
|    ! 0 | 3545 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 3546 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 3547 | `				zIn++;` |
|    ! 0 | 3548 | `			}` |
|    ! 0 | 3549 | `		}` |
|    633 | 3550 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 3551 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 3552 | `		}` |
|      - | 3553 | `		/* Get the precision */` |
|    633 | 3554 | `		precision = -1;` |
|    633 | 3555 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     87 | 3556 | `			precision = 0;` |
|     87 | 3557 | `			zIn++;` |
|    226 | 3558 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     97 | 3559 | `				precision = precision*10 + (zIn[0] - '0');` |
|     97 | 3560 | `				zIn++;` |
|      1 | 3561 | `			}` |
|     43 | 3562 | `		}` |
|      - | 3563 | `		/* Consume a single 'l' length modifier (a C-ism php accepts and ignores,` |
|      - | 3564 | `		 * e.g. "%ld"); PH7_FormatValidate mirrors this. Exactly one is skipped:` |
|      - | 3565 | `		 * in "%lld" the second 'l' becomes the (unknown) specifier, just like php. */` |
|    633 | 3566 | `		if( zIn < zEnd && zIn[0] == 'l' ){` |
|      9 | 3567 | `			zIn++;` |
|      4 | 3568 | `		}` |
|    633 | 3569 | `		if( zIn >= zEnd ){` |
|      - | 3570 | `			/* No more input */` |
|      3 | 3571 | `			break;` |
|      - | 3572 | `		}` |
|      - | 3573 | `		/* Fetch the info entry for the field */` |
|    631 | 3574 | `		pInfo = 0;` |
|    631 | 3575 | `		xtype = PH7_FMT_ERROR;` |
|    631 | 3576 | `		c = zIn[0];` |
|    631 | 3577 | `		zIn++; /* Jump the format specifer */` |
|   2867 | 3578 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|   2867 | 3579 | `			if( c==aFmt[idx].fmttype ){` |
|    631 | 3580 | `				pInfo = &aFmt[idx];` |
|    631 | 3581 | `				xtype = pInfo->type;` |
|    631 | 3582 | `				break;` |
|      - | 3583 | `			}` |
|   1119 | 3584 | `		}` |
|    631 | 3585 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    631 | 3586 | `		length = 0;` |
|      - | 3587 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 3588 | `		 /*` |
|      - | 3589 | `		  ** At this point, variables are initialized as follows:` |
|      - | 3590 | `		  **` |
|      - | 3591 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 3592 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 3593 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 3594 | `		  **                               field width was negative.` |
|      - | 3595 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 3596 | `		  **                               the conversion character.` |
|      - | 3597 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 3598 | `		  **   width                       The specified field width.  This is` |
|      - | 3599 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 3600 | `		  **   precision                   The specified precision.  The default` |
|      - | 3601 | `		  **                               is -1.` |
|      - | 3602 | `		  */` |
|    631 | 3603 | `		switch(xtype){` |
|      3 | 3604 | `		case PH7_FMT_PERCENT:` |
|      - | 3605 | `			/* A literal percent character */` |
|      7 | 3606 | `			zWorker[0] = '%';` |
|      7 | 3607 | `			length = (int)sizeof(char);` |
|      7 | 3608 | `			break;` |
|      3 | 3609 | `		case PH7_FMT_CHARX:` |
|      - | 3610 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 3611 | `			 * with that ASCII value` |
|      - | 3612 | `			 */` |
|      7 | 3613 | `			pArg = NEXT_ARG;` |
|      7 | 3614 | `			if( pArg == 0 ){` |
|      3 | 3615 | `				c = 0;` |
|      2 | 3616 | `			}else{` |
|      5 | 3617 | `				c = ph7_value_to_int(pArg);` |
|      - | 3618 | `			}` |
|      - | 3619 | `			/* NUL byte is an acceptable value */` |
|      7 | 3620 | `			zWorker[0] = (char)c;` |
|      7 | 3621 | `			length = (int)sizeof(char);` |
|      7 | 3622 | `			break;` |
|    162 | 3623 | `		case PH7_FMT_STRING:` |
|      - | 3624 | `			/* the argument is treated as and presented as a string */` |
|    325 | 3625 | `			pArg = NEXT_ARG;` |
|    325 | 3626 | `			if( pArg == 0 ){` |
|    ! 0 | 3627 | `				length = 0;` |
|    ! 0 | 3628 | `			}else{` |
|    325 | 3629 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 3630 | `			}` |
|    325 | 3631 | `			if( length < 1 ){` |
|    ! 0 | 3632 | `				zBuf = " ";` |
|    ! 0 | 3633 | `				length = (int)sizeof(char);` |
|    ! 0 | 3634 | `			}` |
|    325 | 3635 | `			if( precision>=0 && precision<length ){` |
|      3 | 3636 | `				length = precision;` |
|      1 | 3637 | `			}` |
|    325 | 3638 | `			if( flag_zeropad ){` |
|      - | 3639 | `				/* zero-padding works on strings too */` |
|    ! 0 | 3640 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3641 | `					spaces[idx] = '0';` |
|    ! 0 | 3642 | `				}` |
|    ! 0 | 3643 | `			}` |
|    325 | 3644 | `			break;` |
|     59 | 3645 | `		case PH7_FMT_RADIX:` |
|    119 | 3646 | `			pArg = NEXT_ARG;` |
|    119 | 3647 | `			if( pArg == 0 ){` |
|    ! 0 | 3648 | `				iVal = 0;` |
|    ! 0 | 3649 | `			}else{` |
|    119 | 3650 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 3651 | `			}` |
|      - | 3652 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|    119 | 3653 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 3654 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 3655 | `			}` |
|      - | 3656 | `#if 1` |
|      - | 3657 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 3658 | `        ** I think this is stupid.*/` |
|    119 | 3659 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 3660 | `#else` |
|      - | 3661 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 3662 | `        ** but leave the prefix for hex.*/` |
|      - | 3663 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 3664 | `#endif` |
|    119 | 3665 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|     95 | 3666 | `          if( iVal<0 ){` |
|     25 | 3667 | `            iVal = -iVal;` |
|      - | 3668 | `			/* Ticket 1433-003 */` |
|     25 | 3669 | `			if( iVal < 0 ){` |
|      - | 3670 | `				/* Overflow */` |
|    ! 0 | 3671 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3672 | `			}` |
|     25 | 3673 | `            prefix = '-';` |
|     83 | 3674 | `          }else if( flag_plussign )  prefix = '+';` |
|     69 | 3675 | `          else if( flag_blanksign )  prefix = ' ';` |
|     67 | 3676 | `          else                       prefix = 0;` |
|     48 | 3677 | `        }else{` |
|     25 | 3678 | `			if( iVal<0 ){` |
|    ! 0 | 3679 | `				iVal = -iVal;` |
|      - | 3680 | `				/* Ticket 1433-003 */` |
|    ! 0 | 3681 | `				if( iVal < 0 ){` |
|      - | 3682 | `					/* Overflow */` |
|    ! 0 | 3683 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3684 | `				}` |
|    ! 0 | 3685 | `			}` |
|     25 | 3686 | `			prefix = 0;` |
|      - | 3687 | `		}` |
|    119 | 3688 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      7 | 3689 | `          precision = width-(prefix!=0);` |
|      3 | 3690 | `        }` |
|    119 | 3691 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 3692 | `        {` |
|      - | 3693 | `          register char *cset;      /* Use registers for speed */` |
|      - | 3694 | `          register int base;` |
|    119 | 3695 | `          cset = pInfo->charset;` |
|    119 | 3696 | `          base = pInfo->base;` |
|     59 | 3697 | `          do{                                           /* Convert to ascii */` |
|    185 | 3698 | `            *(--zBuf) = cset[iVal%base];` |
|    185 | 3699 | `            iVal = iVal/base;` |
|    185 | 3700 | `          }while( iVal>0 );` |
|      - | 3701 | `        }` |
|    119 | 3702 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    141 | 3703 | `        for(idx=precision-length; idx>0; idx--){` |
|     23 | 3704 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|     12 | 3705 | `        }` |
|    119 | 3706 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|    119 | 3707 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 3708 | `          char *pre, x;` |
|    ! 0 | 3709 | `          pre = pInfo->prefix;` |
|    ! 0 | 3710 | `          if( *zBuf!=pre[0] ){` |
|    ! 0 | 3711 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|    ! 0 | 3712 | `          }` |
|    ! 0 | 3713 | `        }` |
|    119 | 3714 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    119 | 3715 | `		break;` |
|     88 | 3716 | `		case PH7_FMT_FLOAT:` |
|      - | 3717 | `		case PH7_FMT_EXP:` |
|      - | 3718 | `		case PH7_FMT_GENERIC:{` |
|      - | 3719 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3720 | `		double realvalue;` |
|      - | 3721 | `		char zFmt[8];` |
|      - | 3722 | `		int nOut, nFmt;` |
|    177 | 3723 | `		pArg = NEXT_ARG;` |
|    177 | 3724 | `		if( pArg == 0 ){` |
|    ! 0 | 3725 | `			realvalue = 0;` |
|    ! 0 | 3726 | `		}else{` |
|    177 | 3727 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 3728 | `		}` |
|      - | 3729 | `		/* php prints the IEEE specials bare — NaN / INF / -INF with no width` |
|      - | 3730 | `		 * padding, precision, or sign flags (php_sprintf_appenddouble). */` |
|    177 | 3731 | `		if( PH7_IS_NAN(realvalue) ){` |
|     21 | 3732 | `			zBuf = "NaN";` |
|     21 | 3733 | `			length = 3;` |
|     21 | 3734 | `			width = 0;` |
|     21 | 3735 | `			break;` |
|      - | 3736 | `		}` |
|    157 | 3737 | `		if( PH7_IS_INF(realvalue) ){` |
|     37 | 3738 | `			if( realvalue < 0.0 ){` |
|     15 | 3739 | `				zBuf = "-INF";` |
|     15 | 3740 | `				length = 4;` |
|      8 | 3741 | `			}else{` |
|     23 | 3742 | `				zBuf = "INF";` |
|     23 | 3743 | `				length = 3;` |
|      - | 3744 | `			}` |
|     37 | 3745 | `			width = 0;` |
|     37 | 3746 | `			break;` |
|      - | 3747 | `		}` |
|    121 | 3748 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|    121 | 3749 | `		if( precision > 53 ){` |
|      - | 3750 | `			/* php's FORMAT_CONV_MAX_PRECISION cap, with the same E_NOTICE` |
|      - | 3751 | `			 * (message prefixed with the active function's name, like` |
|      - | 3752 | `			 * php_error_docref). */` |
|      - | 3753 | `			char zMsg[160];` |
|      4 | 3754 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 3755 | `				"%z(): Requested precision of %d digits was truncated to PHP maximum of %d digits",` |
|      2 | 3756 | `				&pCtx->pFunc->sName,precision,53);` |
|      3 | 3757 | `			PH7_VmThrowError(pCtx->pVm,0,E_NOTICE,zMsg);` |
|      3 | 3758 | `			precision = 53;` |
|      1 | 3759 | `		}` |
|      - | 3760 | ``		/* php's %f/%e extract the sign via `num < 0`, so negative zero prints`` |
|      - | 3761 | `		 * unsigned there — while %g (php_gcvt on the raw value) keeps "-0". */` |
|    121 | 3762 | `		if( xtype!=PH7_FMT_GENERIC && realvalue == 0.0 ){` |
|      9 | 3763 | `			realvalue = 0.0;` |
|      4 | 3764 | `		}` |
|      - | 3765 | `		/* php's float conversions are correctly rounded (zend_dtoa); use libc` |
|      - | 3766 | `		 * snprintf as the digit engine (the byte-exact-floats rule — the old` |
|      - | 3767 | `		 * hand-rolled vxGetdigit loop stopped at 16 significant digits, so` |
|      - | 3768 | `		 * e.g. %f of 1e308 printed zeros where php prints the exact binary64` |
|      - | 3769 | `		 * expansion), then post-process into php's exact shapes below. */` |
|    121 | 3770 | `		nFmt = 0;` |
|    121 | 3771 | `		zFmt[nFmt++] = '%';` |
|    121 | 3772 | `		if( flag_alternateform ) zFmt[nFmt++] = '#';` |
|      - | 3773 | `		/* php's ' ' flag selects space PADDING (its default), not C's` |
|      - | 3774 | `		 * space-for-positive-sign — so flag_blanksign is NOT forwarded. */` |
|    121 | 3775 | `		if( flag_plussign ) zFmt[nFmt++] = '+';` |
|    121 | 3776 | `		zFmt[nFmt++] = '.';` |
|    121 | 3777 | `		zFmt[nFmt++] = '*';` |
|    165 | 3778 | `		zFmt[nFmt++] = (char)(xtype==PH7_FMT_FLOAT ? 'f' :` |
|     30 | 3779 | `			(xtype==PH7_FMT_EXP ? ((pInfo->charset[0]=='E') ? 'E' : 'e')` |
|     28 | 3780 | `			                    : ((pInfo->charset[0]=='E') ? 'G' : 'g')));` |
|    121 | 3781 | `		zFmt[nFmt] = 0;` |
|    121 | 3782 | `		nOut = snprintf(zWorker,sizeof(zWorker),zFmt,precision,realvalue);` |
|    121 | 3783 | `		if( nOut < 0 \|\| nOut >= (int)sizeof(zWorker) ){` |
|      - | 3784 | `			/* Cannot happen with precision capped at 53 (%f of DBL_MAX is` |
|      - | 3785 | `			 * ~365 bytes); keep the truncated output rather than overrun. */` |
|    ! 0 | 3786 | `			nOut = (int)SyStrlen(zWorker);` |
|    ! 0 | 3787 | `		}` |
|    121 | 3788 | `		nOut = (int)PH7_PhpFloatShape(zWorker,(sxi32)nOut,xtype==PH7_FMT_GENERIC);` |
|    121 | 3789 | `		zBuf = zWorker;` |
|    121 | 3790 | `		length = nOut;` |
|      - | 3791 | `		/* Let the zero-pad block below insert zeros between the sign (written` |
|      - | 3792 | `		 * by snprintf) and the first digit, as before. */` |
|    121 | 3793 | `		prefix = (zWorker[0]=='-' \|\| zWorker[0]=='+' \|\| zWorker[0]==' ') ? zWorker[0] : 0;` |
|      - | 3794 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 3795 | `        ** set and we are not left justified */` |
|    121 | 3796 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 3797 | `          int i;` |
|      7 | 3798 | `          int nPad = width - length;` |
|     51 | 3799 | `          for(i=width; i>=nPad; i--){` |
|     45 | 3800 | `            zBuf[i] = zBuf[i-nPad];` |
|     23 | 3801 | `          }` |
|      7 | 3802 | `          i = prefix!=0;` |
|     29 | 3803 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      7 | 3804 | `          length = width;` |
|      3 | 3805 | `        }` |
|      - | 3806 | `#else` |
|      - | 3807 | `         zBuf = " ";` |
|      - | 3808 | `		 length = (int)sizeof(char);` |
|      - | 3809 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|    121 | 3810 | `		 break;` |
|      - | 3811 | `							 }` |
|    ! 0 | 3812 | `		default:` |
|      - | 3813 | `			/* Unreachable: PH7_FormatValidate() rejects unknown specifiers with a` |
|      - | 3814 | `			 * catchable ValueError before formatting begins. Kept as a defensive` |
|      - | 3815 | `			 * no-op that emits nothing. */` |
|    ! 0 | 3816 | `			length = 0;` |
|    ! 0 | 3817 | `			break;` |
|      - | 3818 | `		}` |
|      - | 3819 | `		 /*` |
|      - | 3820 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 3821 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 3822 | `		 ** the output.` |
|      - | 3823 | `		 */` |
|    631 | 3824 | `    if( !flag_leftjustify ){` |
|      - | 3825 | `      register int nspace;` |
|    617 | 3826 | `      nspace = width-length;` |
|    617 | 3827 | `      if( nspace>0 ){` |
|      7 | 3828 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3829 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3830 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3831 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3832 | `			}` |
|    ! 0 | 3833 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3834 | `        }` |
|      7 | 3835 | `        if( nspace>0 ){` |
|      7 | 3836 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      7 | 3837 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3838 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3839 | `			}` |
|      3 | 3840 | `		}` |
|      3 | 3841 | `      }` |
|    308 | 3842 | `    }` |
|    631 | 3843 | `    if( length>0 ){` |
|    631 | 3844 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    631 | 3845 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3846 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3847 | `		}` |
|    315 | 3848 | `    }` |
|    631 | 3849 | `    if( flag_leftjustify ){` |
|      - | 3850 | `      register int nspace;` |
|     15 | 3851 | `      nspace = width-length;` |
|     15 | 3852 | `      if( nspace>0 ){` |
|     11 | 3853 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3854 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3855 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3856 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3857 | `			}` |
|    ! 0 | 3858 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3859 | `        }` |
|     11 | 3860 | `        if( nspace>0 ){` |
|     11 | 3861 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|     11 | 3862 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3863 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3864 | `			}` |
|      5 | 3865 | `		}` |
|      5 | 3866 | `      }` |
|      7 | 3867 | `    }` |
|      1 | 3868 | ` }/* for(;;) */` |
|    317 | 3869 | `	return SXRET_OK;` |
|    159 | 3870 | `}` |
|      - | 3871 | `/*` |
|      - | 3872 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 3873 | ` */` |
|    146 | 3874 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 3875 | `{` |
|      - | 3876 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|      - | 3877 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|      - | 3878 | `	 * non-OK rc also stops the format loop. */` |
|    147 | 3879 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|    147 | 3880 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|    147 | 3881 | `	return *pRc;` |
|      1 | 3882 | `}` |
|      - | 3883 | `/*` |
|      - | 3884 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 3885 | ` *  Return a formatted string.` |
|      - | 3886 | ` * Parameters` |
|      - | 3887 | ` *  $format` |
|      - | 3888 | ` *    The format string (see block comment above)` |
|      - | 3889 | ` * Return` |
|      - | 3890 | ` *  A string produced according to the formatting string format.` |
|      - | 3891 | ` */` |
|    110 | 3892 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3893 | `{` |
|      - | 3894 | `	const char *zFormat;` |
|    111 | 3895 | `	sxi32 rc = SXRET_OK;` |
|      - | 3896 | `	int nLen;` |
|    111 | 3897 | `	if( nArg < 1 ){` |
|      - | 3898 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 3899 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3900 | `		return PH7_OK;` |
|      - | 3901 | `	}` |
|      - | 3902 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|    111 | 3903 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    111 | 3904 | `	if( rc != PH7_OK ){` |
|      5 | 3905 | `		return rc;` |
|      - | 3906 | `	}` |
|      - | 3907 | `	/* Extract the string format (scalars/null coerce). */` |
|    107 | 3908 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    107 | 3909 | `	if( nLen < 1 ){` |
|      - | 3910 | `		/* Empty string */` |
|    ! 0 | 3911 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3912 | `		return PH7_OK;` |
|      - | 3913 | `	}` |
|      - | 3914 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 3915 | `	 * output; propagate the throw status verbatim. */` |
|    107 | 3916 | `	rc = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    107 | 3917 | `	if( rc != PH7_OK ){` |
|     17 | 3918 | `		return rc;` |
|      - | 3919 | `	}` |
|      - | 3920 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|     91 | 3921 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|     91 | 3922 | `	if( rc != SXRET_OK ){` |
|      - | 3923 | `		/* The result append ran out of memory: raise a fatal rather than` |
|      - | 3924 | `		 * returning a silently-truncated string. */` |
|    ! 0 | 3925 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 3926 | `	}` |
|     91 | 3927 | `	return PH7_OK;` |
|     56 | 3928 | `}` |
|      - | 3929 | `/*` |
|      - | 3930 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 3931 | ` */` |
|   1130 | 3932 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 3933 | `{` |
|   1131 | 3934 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 3935 | `	/* Call the VM output consumer directly */` |
|   1131 | 3936 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 3937 | `	/* Increment counter */` |
|   1131 | 3938 | `	*pCounter += nLen;` |
|   1131 | 3939 | `	return PH7_OK;` |
|      1 | 3940 | `}` |
|      - | 3941 | `/*` |
|      - | 3942 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 3943 | ` *  Output a formatted string.` |
|      - | 3944 | ` * Parameters` |
|      - | 3945 | ` *  $format` |
|      - | 3946 | ` *   See sprintf() for a description of format.` |
|      - | 3947 | ` * Return` |
|      - | 3948 | ` *  The length of the outputted string.` |
|      - | 3949 | ` */` |
|    200 | 3950 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3951 | `{` |
|    201 | 3952 | `	ph7_int64 nCounter = 0;` |
|      - | 3953 | `	const char *zFormat;` |
|      - | 3954 | `	int nLen;` |
|    201 | 3955 | `	if( nArg < 1 ){` |
|      - | 3956 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 3957 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3958 | `		return PH7_OK;` |
|      - | 3959 | `	}` |
|      - | 3960 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|      - | 3961 | `	{` |
|    201 | 3962 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    201 | 3963 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 3964 | `			return rcf;` |
|      - | 3965 | `		}` |
|      - | 3966 | `	}` |
|      - | 3967 | `	/* Extract the string format (scalars/null coerce). */` |
|    201 | 3968 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    201 | 3969 | `	if( nLen < 1 ){` |
|      - | 3970 | `		/* Empty string */` |
|    ! 0 | 3971 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3972 | `		return PH7_OK;` |
|      - | 3973 | `	}` |
|      - | 3974 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 3975 | `	 * output; propagate the throw status verbatim. */` |
|      - | 3976 | `	{` |
|    201 | 3977 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    201 | 3978 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 3979 | `			return rcv;` |
|      - | 3980 | `		}` |
|      - | 3981 | `	}` |
|      - | 3982 | `	/* Format the string */` |
|    201 | 3983 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 3984 | `	/* Return the length of the outputted string */` |
|    201 | 3985 | `	ph7_result_int64(pCtx,nCounter);` |
|    201 | 3986 | `	return PH7_OK;` |
|    101 | 3987 | `}` |
|      - | 3988 | `/*` |
|      - | 3989 | ` * int vprintf(string $format,array $args)` |
|      - | 3990 | ` *  Output a formatted string.` |
|      - | 3991 | ` * Parameters` |
|      - | 3992 | ` *  $format` |
|      - | 3993 | ` *   See sprintf() for a description of format.` |
|      - | 3994 | ` * Return` |
|      - | 3995 | ` *  The length of the outputted string.` |
|      - | 3996 | ` */` |
|      4 | 3997 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3998 | `{` |
|      5 | 3999 | `	ph7_int64 nCounter = 0;` |
|      - | 4000 | `	const char *zFormat;` |
|      - | 4001 | `	ph7_hashmap *pMap;` |
|      - | 4002 | `	SySet sArg;` |
|      - | 4003 | `	int nLen,n;` |
|      - | 4004 | `	sxi32 rcFmt;` |
|      5 | 4005 | `	if( nArg < 2 ){` |
|      - | 4006 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 4007 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4008 | `		return PH7_OK;` |
|      - | 4009 | `	}` |
|      - | 4010 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|      5 | 4011 | `	rcFmt = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|      5 | 4012 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4013 | `		return rcFmt;` |
|      - | 4014 | `	}` |
|      5 | 4015 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 4016 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 4017 | `		char zBuf[64];` |
|      4 | 4018 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4019 | `			"vprintf(): Argument #2 ($values) must be of type array, %s given",` |
|      2 | 4020 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 4021 | `	}` |
|      - | 4022 | `	/* Extract the string format (scalars/null coerce). */` |
|      3 | 4023 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4024 | `	if( nLen < 1 ){` |
|      - | 4025 | `		/* Empty string */` |
|    ! 0 | 4026 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4027 | `		return PH7_OK;` |
|      - | 4028 | `	}` |
|      - | 4029 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4030 | `	 * output; propagate the throw status verbatim. */` |
|      3 | 4031 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      3 | 4032 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4033 | `		return rcFmt;` |
|      - | 4034 | `	}` |
|      - | 4035 | `	/* Point to the hashmap */` |
|      3 | 4036 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4037 | `	/* Extract arguments from the hashmap */` |
|      3 | 4038 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4039 | `	/* Format the string */` |
|      3 | 4040 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 4041 | `	/* Release the container */` |
|      3 | 4042 | `	SySetRelease(&sArg);` |
|      - | 4043 | `	/* Return the length of the outputted string */` |
|      3 | 4044 | `	ph7_result_int64(pCtx,nCounter);` |
|      3 | 4045 | `	return PH7_OK;` |
|      3 | 4046 | `}` |
|      - | 4047 | `/*` |
|      - | 4048 | ` * int vsprintf(string $format,array $args)` |
|      - | 4049 | ` *  Output a formatted string.` |
|      - | 4050 | ` * Parameters` |
|      - | 4051 | ` *  $format` |
|      - | 4052 | ` *   See sprintf() for a description of format.` |
|      - | 4053 | ` * Return` |
|      - | 4054 | ` *  A string produced according to the formatting string format.` |
|      - | 4055 | ` */` |
|     22 | 4056 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4057 | `{` |
|      - | 4058 | `	const char *zFormat;` |
|      - | 4059 | `	ph7_hashmap *pMap;` |
|      - | 4060 | `	SySet sArg;` |
|     23 | 4061 | `	sxi32 rc = SXRET_OK;` |
|      - | 4062 | `	sxi32 rcFmt;` |
|      - | 4063 | `	int nLen,n;` |
|     23 | 4064 | `	if( nArg < 2 ){` |
|      - | 4065 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4066 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4067 | `		return PH7_OK;` |
|      - | 4068 | `	}` |
|      - | 4069 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|     23 | 4070 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|     23 | 4071 | `	if( rc != PH7_OK ){` |
|      5 | 4072 | `		return rc;` |
|      - | 4073 | `	}` |
|     19 | 4074 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 4075 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 4076 | `		char zBuf[64];` |
|     16 | 4077 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4078 | `			"vsprintf(): Argument #2 ($values) must be of type array, %s given",` |
|     10 | 4079 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 4080 | `	}` |
|      - | 4081 | `	/* Extract the string format (scalars/null coerce). */` |
|      9 | 4082 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 4083 | `	if( nLen < 1 ){` |
|      - | 4084 | `		/* Empty string */` |
|    ! 0 | 4085 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4086 | `		return PH7_OK;` |
|      - | 4087 | `	}` |
|      - | 4088 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4089 | `	 * output; propagate the throw status verbatim. */` |
|      9 | 4090 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      9 | 4091 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4092 | `		return rcFmt;` |
|      - | 4093 | `	}` |
|      - | 4094 | `	/* Point to hashmap */` |
|      9 | 4095 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4096 | `	/* Extract arguments from the hashmap */` |
|      9 | 4097 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4098 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|      9 | 4099 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|      - | 4100 | `	/* Release the container */` |
|      9 | 4101 | `	SySetRelease(&sArg);` |
|      9 | 4102 | `	if( rc != SXRET_OK ){` |
|      - | 4103 | `		/* The result append ran out of memory: raise a fatal. */` |
|    ! 0 | 4104 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4105 | `	}` |
|      9 | 4106 | `	return PH7_OK;` |
|     12 | 4107 | `}` |
|      - | 4108 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 4109 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4110 | `/*` |
|      - | 4111 | ` * Symisc eXtension.` |
|      - | 4112 | ` * string size_format(int64 $size)` |
|      - | 4113 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 4114 | ` *  Example:` |
|      - | 4115 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 4116 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 4117 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 4118 | ` * Parameter` |
|      - | 4119 | ` *  $size` |
|      - | 4120 | ` *    Entity size in bytes.` |
|      - | 4121 | ` * Return` |
|      - | 4122 | ` *   Formatted string representation of the given size.` |
|      - | 4123 | ` */` |
|     24 | 4124 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4125 | `{` |
|      - | 4126 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 4127 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 4128 | `	sxi32 nRest,i_32;` |
|      - | 4129 | `	ph7_int64 iSize;` |
|     25 | 4130 | `	int c = -1; /* index in zUnit[] */` |
|      - | 4131 |  |
|     25 | 4132 | `	if( nArg < 1 ){` |
|      - | 4133 | `		/* Missing argument,return the empty string */` |
|      3 | 4134 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4135 | `		return PH7_OK;` |
|      - | 4136 | `	}` |
|      - | 4137 | `	/* Extract the given size */` |
|     23 | 4138 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 4139 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 4140 | `		/* Don't bother formatting,return immediately */` |
|      5 | 4141 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 4142 | `		return PH7_OK;` |
|      - | 4143 | `	}` |
|     19 | 4144 | `	for(;;){` |
|     39 | 4145 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 4146 | `		iSize >>= 10;` |
|     39 | 4147 | `		c++;` |
|     39 | 4148 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 4149 | `			break;` |
|      - | 4150 | `		}` |
|      1 | 4151 | `	}` |
|     19 | 4152 | `	nRest /= 100;` |
|     19 | 4153 | `	if( nRest > 9 ){` |
|    ! 0 | 4154 | `		nRest = 9;` |
|    ! 0 | 4155 | `	}` |
|     19 | 4156 | `	if( iSize > 999 ){` |
|    ! 0 | 4157 | `		c++;` |
|    ! 0 | 4158 | `		nRest = 9;` |
|    ! 0 | 4159 | `		iSize = 0;` |
|    ! 0 | 4160 | `	}` |
|     19 | 4161 | `	i_32 = (sxi32)iSize;` |
|      - | 4162 | `	/* Format */` |
|     19 | 4163 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 4164 | `	return PH7_OK;` |
|     13 | 4165 | `}` |
|      - | 4166 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4167 | `/*` |
|      - | 4168 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 4169 | ` *   Calculate the md5 hash of a string.` |
|      - | 4170 | ` * Parameter` |
|      - | 4171 | ` *  $str` |
|      - | 4172 | ` *   Input string` |
|      - | 4173 | ` * $raw_output` |
|      - | 4174 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4175 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4176 | ` * Return` |
|      - | 4177 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 4178 | ` */` |
|     12 | 4179 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4180 | `{` |
|      - | 4181 | `	unsigned char zDigest[16];` |
|     13 | 4182 | `	int raw_output = FALSE;` |
|      - | 4183 | `	const void *pIn;` |
|      - | 4184 | `	int nLen;` |
|     13 | 4185 | `	if( nArg < 1 ){` |
|      - | 4186 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4187 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4188 | `		return PH7_OK;` |
|      - | 4189 | `	}` |
|      - | 4190 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 4191 | `	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */` |
|     13 | 4192 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 4193 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 4194 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 4195 | `	}` |
|      - | 4196 | `	/* Compute the MD5 digest */` |
|     13 | 4197 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|     13 | 4198 | `	if( raw_output ){` |
|      - | 4199 | `		/* Output raw digest */` |
|      5 | 4200 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 4201 | `	}else{` |
|      - | 4202 | `		/* Perform a binary to hex conversion */` |
|      9 | 4203 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4204 | `	}` |
|     13 | 4205 | `	return PH7_OK;` |
|      7 | 4206 | `}` |
|      - | 4207 | `/*` |
|      - | 4208 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 4209 | ` *   Calculate the sha1 hash of a string.` |
|      - | 4210 | ` * Parameter` |
|      - | 4211 | ` *  $str` |
|      - | 4212 | ` *   Input string` |
|      - | 4213 | ` * $raw_output` |
|      - | 4214 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4215 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4216 | ` * Return` |
|      - | 4217 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 4218 | ` */` |
|     10 | 4219 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4220 | `{` |
|      - | 4221 | `	unsigned char zDigest[20];` |
|     11 | 4222 | `	int raw_output = FALSE;` |
|      - | 4223 | `	const void *pIn;` |
|      - | 4224 | `	int nLen;` |
|     11 | 4225 | `	if( nArg < 1 ){` |
|      - | 4226 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4227 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4228 | `		return PH7_OK;` |
|      - | 4229 | `	}` |
|      - | 4230 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 4231 | `	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */` |
|     11 | 4232 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 4233 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 4234 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 4235 | `	}` |
|      - | 4236 | `	/* Compute the SHA1 digest */` |
|     11 | 4237 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|     11 | 4238 | `	if( raw_output ){` |
|      - | 4239 | `		/* Output raw digest */` |
|      5 | 4240 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 4241 | `	}else{` |
|      - | 4242 | `		/* Perform a binary to hex conversion */` |
|      7 | 4243 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4244 | `	}` |
|     11 | 4245 | `	return PH7_OK;` |
|      6 | 4246 | `}` |
|      - | 4247 | `/*` |
|      - | 4248 | ` * int64 crc32(string $str)` |
|      - | 4249 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 4250 | ` * Parameter` |
|      - | 4251 | ` *  $str` |
|      - | 4252 | ` *   Input string` |
|      - | 4253 | ` * Return` |
|      - | 4254 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 4255 | ` */` |
|      2 | 4256 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4257 | `{` |
|      - | 4258 | `	const void *pIn;` |
|      - | 4259 | `	sxu32 nCRC;` |
|      - | 4260 | `	int nLen;` |
|      3 | 4261 | `	if( nArg < 1 ){` |
|      - | 4262 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 4263 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4264 | `		return PH7_OK;` |
|      - | 4265 | `	}` |
|      - | 4266 | `	/* Extract the input string */` |
|      3 | 4267 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4268 | `	if( nLen < 1 ){` |
|      - | 4269 | `		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike` |
|      - | 4270 | `		 * md5()/sha1(), whose empty-string digests are non-zero. */` |
|    ! 0 | 4271 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4272 | `		return PH7_OK;` |
|      - | 4273 | `	}` |
|      - | 4274 | `	/* Calculate the sum */` |
|      3 | 4275 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 4276 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 4277 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 4278 | `	return PH7_OK;` |
|      2 | 4279 | `}` |
|      - | 4280 | `/*` |
|      - | 4281 | ` * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is` |
|      - | 4282 | ` * described by a small record so one dispatch (and one generic HMAC) serves them` |
|      - | 4283 | ` * all. Thin adapters normalize the differing context types and the reversed` |
|      - | 4284 | ` * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.` |
|      - | 4285 | ` */` |
|     11 | 4286 | `static void HashMd5Init(HashCtx *c){ MD5Init(&c->md5); }` |
|     15 | 4287 | `static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }` |
|     11 | 4288 | `static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }` |
|     11 | 4289 | `static void HashSha1Init(HashCtx *c){ SHA1Init(&c->sha1); }` |
|     15 | 4290 | `static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }` |
|     11 | 4291 | `static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }` |
|      9 | 4292 | `static void HashSha224Init(HashCtx *c){ SHA224Init(&c->sha256); }` |
|     33 | 4293 | `static void HashSha256Init(HashCtx *c){ SHA256Init(&c->sha256); }` |
|     57 | 4294 | `static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }` |
|     41 | 4295 | `static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }` |
|      9 | 4296 | `static void HashSha384Init(HashCtx *c){ SHA384Init(&c->sha512); }` |
|     15 | 4297 | `static void HashSha512Init(HashCtx *c){ SHA512Init(&c->sha512); }` |
|     27 | 4298 | `static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }` |
|     23 | 4299 | `static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }` |
|      - | 4300 | `typedef struct HashAlgo HashAlgo;` |
|      - | 4301 | `struct HashAlgo {` |
|      - | 4302 | `	const char *zName;   /* lowercase canonical name */` |
|      - | 4303 | `	int nDigestLen;      /* output bytes: 16/20/28/32/48/64 */` |
|      - | 4304 | `	int nBlockLen;       /* internal block bytes (for HMAC): 64 or 128 */` |
|      - | 4305 | `	void (*xInit)(HashCtx *);` |
|      - | 4306 | `	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);` |
|      - | 4307 | `	void (*xFinal)(HashCtx *,unsigned char *);` |
|      - | 4308 | `};` |
|      - | 4309 | `static const HashAlgo aHashAlgo[] = {` |
|      - | 4310 | `	{ "md5",    16, 64,  HashMd5Init,    HashMd5Update,    HashMd5Final    },` |
|      - | 4311 | `	{ "sha1",   20, 64,  HashSha1Init,   HashSha1Update,   HashSha1Final   },` |
|      - | 4312 | `	{ "sha224", 28, 64,  HashSha224Init, HashSha256Update, HashSha256Final },` |
|      - | 4313 | `	{ "sha256", 32, 64,  HashSha256Init, HashSha256Update, HashSha256Final },` |
|      - | 4314 | `	{ "sha384", 48, 128, HashSha384Init, HashSha512Update, HashSha512Final },` |
|      - | 4315 | `	{ "sha512", 64, 128, HashSha512Init, HashSha512Update, HashSha512Final },` |
|      - | 4316 | `};` |
|      - | 4317 | `/* Case-insensitive algorithm lookup (PHP accepts 'SHA256' etc.). */` |
|     73 | 4318 | `static const HashAlgo * HashFindAlgo(const char *zName,int nLen){` |
|      - | 4319 | `	sxu32 i;` |
|    279 | 4320 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|    272 | 4321 | `		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen` |
|    211 | 4322 | `			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){` |
|     67 | 4323 | `			return &aHashAlgo[i];` |
|      - | 4324 | `		}` |
|    106 | 4325 | `	}` |
|      6 | 4326 | `	return 0;` |
|     38 | 4327 | `}` |
|      - | 4328 | `/*` |
|      - | 4329 | ` * string hash(string $algo,string $data[,bool $binary = false])` |
|      - | 4330 | ` *   Generate a hash value (message digest).` |
|      - | 4331 | ` */` |
|     54 | 4332 | `static int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4333 | `{` |
|      - | 4334 | `	const HashAlgo *pAlgo;` |
|      - | 4335 | `	const char *zAlgo,*zData;` |
|     56 | 4336 | `	int nAlgoLen,nDataLen,raw_output = FALSE;` |
|      - | 4337 | `	HashCtx sCtx;` |
|      - | 4338 | `	unsigned char zDigest[64];` |
|     56 | 4339 | `	if( nArg < 2 ){` |
|    ! 0 | 4340 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4341 | `			"hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4342 | `	}` |
|     56 | 4343 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     56 | 4344 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     56 | 4345 | `	if( pAlgo == 0 ){` |
|      3 | 4346 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4347 | `			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - | 4348 | `	}` |
|     53 | 4349 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     53 | 4350 | `	if( nArg > 2 ){` |
|      9 | 4351 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|      4 | 4352 | `	}` |
|     53 | 4353 | `	pAlgo->xInit(&sCtx);` |
|     53 | 4354 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     53 | 4355 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     53 | 4356 | `	if( raw_output ){` |
|      9 | 4357 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|      5 | 4358 | `	}else{` |
|     45 | 4359 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - | 4360 | `	}` |
|     53 | 4361 | `	return PH7_OK;` |
|     29 | 4362 | `}` |
|      - | 4363 | `/*` |
|      - | 4364 | ` * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])` |
|      - | 4365 | ` *   Generate a keyed hash value using the HMAC method (RFC 2104).` |
|      - | 4366 | ` */` |
|     16 | 4367 | `static int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4368 | `{` |
|      - | 4369 | `	const HashAlgo *pAlgo;` |
|      - | 4370 | `	const char *zAlgo,*zData,*zKey;` |
|     18 | 4371 | `	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;` |
|      - | 4372 | `	HashCtx sCtx;` |
|      - | 4373 | `	unsigned char zKeyBlock[128],zIpad[128],zOpad[128],zInner[64],zDigest[64];` |
|      - | 4374 | `	int i,nBlock,nDigest;` |
|     18 | 4375 | `	if( nArg < 3 ){` |
|    ! 0 | 4376 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4377 | `			"hash_hmac() expects at least 3 arguments, %d given",nArg);` |
|      - | 4378 | `	}` |
|     18 | 4379 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     18 | 4380 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     18 | 4381 | `	if( pAlgo == 0 ){` |
|      3 | 4382 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4383 | `			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 4384 | `	}` |
|     15 | 4385 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     15 | 4386 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|     15 | 4387 | `	if( nArg > 3 ){` |
|      3 | 4388 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|      1 | 4389 | `	}` |
|     15 | 4390 | `	nBlock = pAlgo->nBlockLen;` |
|     15 | 4391 | `	nDigest = pAlgo->nDigestLen;` |
|      - | 4392 | `	/* Reduce the key to a single block: hash it if longer than the block, then` |
|      - | 4393 | `	 * zero-pad (a short or empty key is just zero-padded). */` |
|     15 | 4394 | `	SyZero(zKeyBlock,sizeof(zKeyBlock));` |
|     15 | 4395 | `	if( nKeyLen > nBlock ){` |
|      3 | 4396 | `		pAlgo->xInit(&sCtx);` |
|      3 | 4397 | `		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);` |
|      3 | 4398 | `		pAlgo->xFinal(&sCtx,zKeyBlock);` |
|     14 | 4399 | `	}else if( nKeyLen > 0 ){` |
|     11 | 4400 | `		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);` |
|      5 | 4401 | `	}` |
|   1039 | 4402 | `	for( i = 0; i < nBlock; i++ ){` |
|   1025 | 4403 | `		zIpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);` |
|   1025 | 4404 | `		zOpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);` |
|    513 | 4405 | `	}` |
|      - | 4406 | `	/* inner = H((key ^ ipad) \|\| data) */` |
|     15 | 4407 | `	pAlgo->xInit(&sCtx);` |
|     15 | 4408 | `	pAlgo->xUpdate(&sCtx,zIpad,(unsigned int)nBlock);` |
|     15 | 4409 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     15 | 4410 | `	pAlgo->xFinal(&sCtx,zInner);` |
|      - | 4411 | `	/* out = H((key ^ opad) \|\| inner) */` |
|     15 | 4412 | `	pAlgo->xInit(&sCtx);` |
|     15 | 4413 | `	pAlgo->xUpdate(&sCtx,zOpad,(unsigned int)nBlock);` |
|     15 | 4414 | `	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)nDigest);` |
|     15 | 4415 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     15 | 4416 | `	if( raw_output ){` |
|      3 | 4417 | `		ph7_result_string(pCtx,(const char *)zDigest,nDigest);` |
|      2 | 4418 | `	}else{` |
|     13 | 4419 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);` |
|      - | 4420 | `	}` |
|     15 | 4421 | `	return PH7_OK;` |
|     10 | 4422 | `}` |
|      - | 4423 | `/*` |
|      - | 4424 | ` * bool hash_equals(string $known_string,string $user_string)` |
|      - | 4425 | ` *   Timing-attack-safe string comparison.` |
|      - | 4426 | ` */` |
|     14 | 4427 | `static int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4428 | `{` |
|      - | 4429 | `	const char *zKnown,*zUser;` |
|      - | 4430 | `	int nKnown,nUser,i;` |
|     17 | 4431 | `	volatile unsigned char vDiff = 0;` |
|     17 | 4432 | `	if( nArg < 2 ){` |
|    ! 0 | 4433 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4434 | `			"hash_equals() expects exactly 2 arguments, %d given",nArg);` |
|      - | 4435 | `	}` |
|     17 | 4436 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      4 | 4437 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4438 | `			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",` |
|      1 | 4439 | `			ph7_type_name(apArg[0]));` |
|      - | 4440 | `	}` |
|     14 | 4441 | `	if( !ph7_value_is_string(apArg[1]) ){` |
|      4 | 4442 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4443 | `			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",` |
|      2 | 4444 | `			ph7_type_name(apArg[1]));` |
|      - | 4445 | `	}` |
|     11 | 4446 | `	zKnown = ph7_value_to_string(apArg[0],&nKnown);` |
|     11 | 4447 | `	zUser = ph7_value_to_string(apArg[1],&nUser);` |
|     11 | 4448 | `	if( nKnown != nUser ){` |
|      5 | 4449 | `		ph7_result_bool(pCtx,0);` |
|      5 | 4450 | `		return PH7_OK;` |
|      - | 4451 | `	}` |
|      - | 4452 | `	/* Constant-time: read every byte, never short-circuit. */` |
|     19 | 4453 | `	for( i = 0; i < nKnown; i++ ){` |
|     13 | 4454 | `		vDiff \|= (unsigned char)(zKnown[i] ^ zUser[i]);` |
|      7 | 4455 | `	}` |
|      7 | 4456 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|      7 | 4457 | `	return PH7_OK;` |
|     10 | 4458 | `}` |
|      - | 4459 | `/*` |
|      - | 4460 | ` * array hash_algos(void)` |
|      - | 4461 | ` *   Return a list of the registered hashing algorithms.` |
|      - | 4462 | ` */` |
|      2 | 4463 | `static int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4464 | `{` |
|      - | 4465 | `	ph7_value *pArray,*pValue;` |
|      - | 4466 | `	sxu32 i;` |
|      1 | 4467 | `	SXUNUSED(nArg);` |
|      1 | 4468 | `	SXUNUSED(apArg);` |
|      3 | 4469 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4470 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 4471 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 4472 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4473 | `		return PH7_OK;` |
|      - | 4474 | `	}` |
|     15 | 4475 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|     13 | 4476 | `		ph7_value_string(pValue,aHashAlgo[i].zName,-1);` |
|     13 | 4477 | `		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);` |
|     13 | 4478 | `		ph7_value_reset_string_cursor(pValue);` |
|      7 | 4479 | `	}` |
|      3 | 4480 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4481 | `	return PH7_OK;` |
|      2 | 4482 | `}` |
|      - | 4483 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 4484 | `/*` |
|      - | 4485 | ` * password_* (bcrypt). These live in ext/standard in real PHP — outside the` |
|      - | 4486 | ` * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.` |
|      - | 4487 | ` */` |
|      - | 4488 | `/*` |
|      - | 4489 | ` * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a` |
|      - | 4490 | ` * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).` |
|      - | 4491 | ` */` |
|     40 | 4492 | `static int BcryptParseHash(const char *zHash,int nHash,int *piCost)` |
|      1 | 4493 | `{` |
|      - | 4494 | `	int iCost;` |
|     40 | 4495 | `	if( nHash != 60 \|\| zHash[0] != '$' \|\| zHash[1] != '2' \|\| zHash[3] != '$'` |
|     29 | 4496 | `		\|\| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){` |
|     13 | 4497 | `		return FALSE;` |
|      - | 4498 | `	}` |
|     29 | 4499 | `	if( zHash[4] < '0' \|\| zHash[4] > '9' \|\| zHash[5] < '0' \|\| zHash[5] > '9' \|\| zHash[6] != '$' ){` |
|    ! 0 | 4500 | `		return FALSE;` |
|      - | 4501 | `	}` |
|     29 | 4502 | `	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');` |
|     29 | 4503 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      3 | 4504 | `		return FALSE;` |
|      - | 4505 | `	}` |
|     27 | 4506 | `	if( piCost ){ *piCost = iCost; }` |
|     27 | 4507 | `	return TRUE;` |
|     21 | 4508 | `}` |
|      - | 4509 | `/*` |
|      - | 4510 | ` * TRUE if the $algo argument selects bcrypt: null (PASSWORD_DEFAULT) or the` |
|      - | 4511 | ` * "2y" id (PASSWORD_BCRYPT/PASSWORD_DEFAULT). bcrypt is the only supported algo.` |
|      - | 4512 | ` */` |
|     20 | 4513 | `static int BcryptIsBcryptAlgo(ph7_value *pAlgo)` |
|      3 | 4514 | `{` |
|     23 | 4515 | `	if( ph7_value_is_null(pAlgo) ){` |
|    ! 0 | 4516 | `		return TRUE;` |
|      - | 4517 | `	}` |
|     23 | 4518 | `	if( ph7_value_is_string(pAlgo) ){` |
|      - | 4519 | `		int nAlgo;` |
|     23 | 4520 | `		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);` |
|     23 | 4521 | `		return ( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' );` |
|      - | 4522 | `	}` |
|    ! 0 | 4523 | `	return FALSE;` |
|     13 | 4524 | `}` |
|      - | 4525 | `/*` |
|      - | 4526 | ` * bool\|string password_hash(string $password,string\|int\|null $algo[,array $options])` |
|      - | 4527 | ` *  Create a bcrypt hash of the password.` |
|      - | 4528 | ` */` |
|     16 | 4529 | `static int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4530 | `{` |
|      - | 4531 | `	const char *zPwd;` |
|     19 | 4532 | `	int nPwd,iCost = 12;` |
|      - | 4533 | `	unsigned char aSalt[16];` |
|      - | 4534 | `	char zHash[60];` |
|     19 | 4535 | `	if( nArg < 2 ){` |
|    ! 0 | 4536 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4537 | `			"password_hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4538 | `	}` |
|     19 | 4539 | `	if( !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      3 | 4540 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4541 | `			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");` |
|      - | 4542 | `	}` |
|      - | 4543 | `	/* cost from $options['cost'] (default 12). */` |
|     16 | 4544 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|     14 | 4545 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|     14 | 4546 | `		if( pCost ){ iCost = ph7_value_to_int(pCost); }` |
|      6 | 4547 | `	}` |
|     16 | 4548 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      4 | 4549 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 4550 | `			"Invalid bcrypt cost parameter specified: %d",iCost);` |
|      - | 4551 | `	}` |
|     13 | 4552 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     13 | 4553 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 4554 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - | 4555 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|      - | 4556 | `	}` |
|     13 | 4557 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){` |
|    ! 0 | 4558 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4559 | `		return PH7_OK;` |
|      - | 4560 | `	}` |
|     13 | 4561 | `	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));` |
|     13 | 4562 | `	return PH7_OK;` |
|     11 | 4563 | `}` |
|      - | 4564 | `/*` |
|      - | 4565 | ` * bool password_verify(string $password,string $hash)` |
|      - | 4566 | ` *  Verify a password against a bcrypt hash. Never throws on a malformed hash.` |
|      - | 4567 | ` */` |
|     28 | 4568 | `static int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4569 | `{` |
|      - | 4570 | `	const char *zPwd,*zHash;` |
|      - | 4571 | `	int nPwd,nHash,iCost,i;` |
|      - | 4572 | `	unsigned char aSalt[16];` |
|      - | 4573 | `	char zComputed[60];` |
|     29 | 4574 | `	volatile unsigned char vDiff = 0;` |
|     29 | 4575 | `	if( nArg < 2 ){` |
|    ! 0 | 4576 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4577 | `			"password_verify() expects exactly 2 arguments, %d given",nArg);` |
|      - | 4578 | `	}` |
|     29 | 4579 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     29 | 4580 | `	zHash = ph7_value_to_string(apArg[1],&nHash);` |
|     29 | 4581 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|     11 | 4582 | `		ph7_result_bool(pCtx,0);` |
|     11 | 4583 | `		return PH7_OK;` |
|      - | 4584 | `	}` |
|      - | 4585 | `	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */` |
|     19 | 4586 | `	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 4587 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4588 | `		return PH7_OK;` |
|      - | 4589 | `	}` |
|     19 | 4590 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){` |
|    ! 0 | 4591 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4592 | `		return PH7_OK;` |
|      - | 4593 | `	}` |
|      - | 4594 | `	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps` |
|      - | 4595 | `	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */` |
|    577 | 4596 | `	for( i = 29; i < 60; i++ ){` |
|    559 | 4597 | `		vDiff \|= (unsigned char)(zComputed[i] ^ zHash[i]);` |
|    280 | 4598 | `	}` |
|     19 | 4599 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|     19 | 4600 | `	return PH7_OK;` |
|     15 | 4601 | `}` |
|      - | 4602 | `/*` |
|      - | 4603 | ` * array password_get_info(string $hash)` |
|      - | 4604 | ` *  Return ["algo"=>id\|null, "algoName"=>name, "options"=>[...]].` |
|      - | 4605 | ` */` |
|      6 | 4606 | `static int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4607 | `{` |
|      7 | 4608 | `	const char *zHash = "";` |
|      7 | 4609 | `	int nHash,iCost = 0,bBcrypt = 0;` |
|      - | 4610 | `	ph7_value *pArray,*pOptions,*pVal;` |
|      7 | 4611 | `	if( nArg > 0 ){` |
|      7 | 4612 | `		zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 4613 | `		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);` |
|      3 | 4614 | `	}` |
|      7 | 4615 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 4616 | `	pOptions = ph7_context_new_array(pCtx);` |
|      7 | 4617 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      7 | 4618 | `	if( pArray == 0 \|\| pOptions == 0 \|\| pVal == 0 ){` |
|    ! 0 | 4619 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4620 | `		return PH7_OK;` |
|      - | 4621 | `	}` |
|      7 | 4622 | `	if( bBcrypt ){` |
|      5 | 4623 | `		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */` |
|      5 | 4624 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 4625 | `		ph7_value_reset_string_cursor(pVal);` |
|      5 | 4626 | `		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);` |
|      5 | 4627 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      5 | 4628 | `		ph7_value_int(pVal,iCost);` |
|      5 | 4629 | `		ph7_array_add_strkey_elem(pOptions,"cost",pVal);` |
|      3 | 4630 | `	}else{` |
|      3 | 4631 | `		ph7_value_null(pVal);                          /* algo => null */` |
|      3 | 4632 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      3 | 4633 | `		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);` |
|      3 | 4634 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      - | 4635 | `	}` |
|      7 | 4636 | `	ph7_array_add_strkey_elem(pArray,"options",pOptions);` |
|      7 | 4637 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 4638 | `	return PH7_OK;` |
|      4 | 4639 | `}` |
|      - | 4640 | `/*` |
|      - | 4641 | ` * bool password_needs_rehash(string $hash,string\|int\|null $algo[,array $options])` |
|      - | 4642 | ` *  True if the hash was not made with the given algo/options.` |
|      - | 4643 | ` */` |
|      6 | 4644 | `static int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4645 | `{` |
|      - | 4646 | `	const char *zHash;` |
|      7 | 4647 | `	int nHash,iCost = 0,iWantCost = 12;` |
|      7 | 4648 | `	if( nArg < 2 ){` |
|    ! 0 | 4649 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4650 | `			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4651 | `	}` |
|      7 | 4652 | `	zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 4653 | `	if( !BcryptParseHash(zHash,nHash,&iCost) \|\| !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      - | 4654 | `		/* A non-bcrypt hash, or a request for a different algo → needs rehash. */` |
|      3 | 4655 | `		ph7_result_bool(pCtx,1);` |
|      3 | 4656 | `		return PH7_OK;` |
|      - | 4657 | `	}` |
|      5 | 4658 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|      5 | 4659 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|      5 | 4660 | `		if( pCost ){ iWantCost = ph7_value_to_int(pCost); }` |
|      2 | 4661 | `	}` |
|      5 | 4662 | `	ph7_result_bool(pCtx,iCost != iWantCost);` |
|      5 | 4663 | `	return PH7_OK;` |
|      4 | 4664 | `}` |
|      - | 4665 | `/*` |
|      - | 4666 | ` * filter_var() — input validation and sanitization (the ext/filter API).` |
|      - | 4667 | ` *` |
|      - | 4668 | ` * Filter and flag identifiers (values match PHP 8.5; the constants themselves` |
|      - | 4669 | ` * are registered in constant.c). The validate filters are hand-rolled rather` |
|      - | 4670 | ` * than delegating to SyStrToInt64/SyStrToReal: the former silently skips leading` |
|      - | 4671 | ` * zeros and cannot signal overflow, and the latter treats ',' as a decimal point` |
|      - | 4672 | ` * unconditionally — neither matches PHP's filter semantics.` |
|      - | 4673 | ` */` |
|      - | 4674 | `#define FV_VALIDATE_INT     257` |
|      - | 4675 | `#define FV_VALIDATE_BOOLEAN 258` |
|      - | 4676 | `#define FV_VALIDATE_FLOAT   259` |
|      - | 4677 | `#define FV_VALIDATE_REGEXP  272` |
|      - | 4678 | `#define FV_VALIDATE_URL     273` |
|      - | 4679 | `#define FV_VALIDATE_EMAIL   274` |
|      - | 4680 | `#define FV_VALIDATE_IP      275` |
|      - | 4681 | `#define FV_VALIDATE_MAC     276` |
|      - | 4682 | `#define FV_VALIDATE_DOMAIN  277` |
|      - | 4683 | `#define FV_SANITIZE_SPECIAL_CHARS      515` |
|      - | 4684 | `#define FV_DEFAULT          516 /* == FILTER_UNSAFE_RAW: pass the value through */` |
|      - | 4685 | `#define FV_SANITIZE_EMAIL   517` |
|      - | 4686 | `#define FV_SANITIZE_URL     518` |
|      - | 4687 | `#define FV_SANITIZE_NUMBER_INT   519` |
|      - | 4688 | `#define FV_SANITIZE_NUMBER_FLOAT 520` |
|      - | 4689 | `#define FV_SANITIZE_FULL_SPECIAL_CHARS 522` |
|      - | 4690 | `#define FV_FLAG_ALLOW_OCTAL  1` |
|      - | 4691 | `#define FV_FLAG_ALLOW_HEX    2` |
|      - | 4692 | `#define FV_FLAG_STRIP_LOW    4` |
|      - | 4693 | `#define FV_FLAG_STRIP_HIGH   8` |
|      - | 4694 | `#define FV_FLAG_ENCODE_LOW   16` |
|      - | 4695 | `#define FV_FLAG_ENCODE_HIGH  32` |
|      - | 4696 | `#define FV_FLAG_ENCODE_AMP   64` |
|      - | 4697 | `#define FV_FLAG_NO_ENCODE_QUOTES 128` |
|      - | 4698 | `#define FV_FLAG_STRIP_BACKTICK   512` |
|      - | 4699 | `#define FV_FLAG_ALLOW_FRACTION   4096` |
|      - | 4700 | `#define FV_FLAG_ALLOW_THOUSAND   8192` |
|      - | 4701 | `#define FV_FLAG_ALLOW_SCIENTIFIC 16384` |
|      - | 4702 | `#define FV_FLAG_IPV4  1048576` |
|      - | 4703 | `#define FV_FLAG_IPV6  2097152` |
|      - | 4704 | `#define FV_NULL_ON_FAILURE 134217728` |
|      - | 4705 | `/* The subset of flags the UNSAFE_RAW/DEFAULT string filter (FvSanitizeString)` |
|      - | 4706 | ` * acts on: when none are set the filter is a verbatim pass-through, so FV_DEFAULT` |
|      - | 4707 | ` * can shortcut. Keep this in sync with FvSanitizeString's flag handling. */` |
|      - | 4708 | `#define FV_FLAG_STRING_MASK (FV_FLAG_STRIP_LOW\|FV_FLAG_STRIP_HIGH\|FV_FLAG_STRIP_BACKTICK \` |
|      - | 4709 | `                            \|FV_FLAG_ENCODE_LOW\|FV_FLAG_ENCODE_HIGH\|FV_FLAG_ENCODE_AMP)` |
|      - | 4710 |  |
|      - | 4711 | `/* Trim leading/trailing PHP whitespace, adjusting the (*pz,*pn) view in place.` |
|      - | 4712 | ` * SyisSpace (isspace) matches PHP's filter whitespace set " \t\n\r\v\f". */` |
|    153 | 4713 | `static void FvTrim(const char **pz,int *pn){` |
|    153 | 4714 | `	const char *z = *pz;` |
|    153 | 4715 | `	int n = *pn;` |
|    157 | 4716 | `	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }` |
|    161 | 4717 | `	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }` |
|    153 | 4718 | `	*pz = z; *pn = n;` |
|    153 | 4719 | `}` |
|      - | 4720 | `/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     57 | 4721 | `static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){` |
|     57 | 4722 | `	int neg = 0, i;` |
|     57 | 4723 | `	sxu64 u = 0;` |
|     57 | 4724 | `	FvTrim(&z,&n);` |
|     57 | 4725 | `	if( n==0 ){ return 0; }` |
|     51 | 4726 | `	if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }` |
|     51 | 4727 | `	if( n==0 ){ return 0; }` |
|     49 | 4728 | `	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'\|\|z[1]=='X') ){` |
|      3 | 4729 | `		z += 2; n -= 2;` |
|      3 | 4730 | `		if( n==0 ){ return 0; }` |
|      7 | 4731 | `		for( i=0; i<n; i++ ){` |
|      5 | 4732 | `			int h = SyHexToint((unsigned char)z[i]);` |
|      5 | 4733 | `			if( h<0 ){ return 0; }` |
|      5 | 4734 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }` |
|      5 | 4735 | `			u = u*16 + (sxu64)h;` |
|      3 | 4736 | `		}` |
|     48 | 4737 | `	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){` |
|      9 | 4738 | `		for( i=0; i<n; i++ ){` |
|      7 | 4739 | `			if( z[i]<'0' \|\| z[i]>'7' ){ return 0; }` |
|      7 | 4740 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }` |
|      7 | 4741 | `			u = u*8 + (sxu64)(z[i]-'0');` |
|      4 | 4742 | `		}` |
|      2 | 4743 | `	}else{` |
|     45 | 4744 | `		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */` |
|    201 | 4745 | `		for( i=0; i<n; i++ ){` |
|    173 | 4746 | `			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|    161 | 4747 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }` |
|    161 | 4748 | `			u = u*10 + (sxu64)(z[i]-'0');` |
|     81 | 4749 | `		}` |
|      - | 4750 | `	}` |
|     33 | 4751 | `	if( neg ){` |
|      5 | 4752 | `		if( u > 0x8000000000000000ULL ){ return 0; }` |
|      5 | 4753 | `		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */` |
|      3 | 4754 | `	}else{` |
|     29 | 4755 | `		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }` |
|     27 | 4756 | `		*pOut = (ph7_int64)u;` |
|      - | 4757 | `	}` |
|     31 | 4758 | `	return 1;` |
|     29 | 4759 | `}` |
|      - | 4760 | `/* FILTER_VALIDATE_FLOAT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     69 | 4761 | `static int FvValidateFloat(const char *z,int n,int flags,double *pOut){` |
|      - | 4762 | `	char zBuf[512];` |
|     69 | 4763 | `	int i, m = 0, seenDigit = 0;` |
|     69 | 4764 | `	const char *zv; int nv; double d = 0;` |
|     69 | 4765 | `	FvTrim(&z,&n);` |
|      - | 4766 | `	/* Bound the input: zBuf[512] holds the thousand-separator-stripped copy, and` |
|      - | 4767 | `	 * the cap also rejects the pathological 500+ digit floats PHP refuses. */` |
|     69 | 4768 | `	if( n==0 \|\| n>500 ){ return 0; }` |
|     69 | 4769 | `	if( flags & FV_FLAG_ALLOW_THOUSAND ){` |
|      - | 4770 | `		/* Commas are optional, but when present they must group the integer part` |
|      - | 4771 | `		 * into a leading run of 1..3 digits followed by groups of exactly 3` |
|      - | 4772 | `		 * ("1,000" ok, "1,5"/"1234,567" rejected). Strip them into zBuf and reject` |
|      - | 4773 | `		 * a comma anywhere in the fractional/exponent tail. */` |
|     25 | 4774 | `		int s = 0, intEnd, segStart, segIdx, hasComma = 0;` |
|     25 | 4775 | `		if( s<n && (z[s]=='+'\|\|z[s]=='-') ){ zBuf[m++] = z[s]; s++; }` |
|     25 | 4776 | `		intEnd = s;` |
|    167 | 4777 | `		while( intEnd<n && z[intEnd]!='.' && z[intEnd]!='e' && z[intEnd]!='E' ){` |
|    143 | 4778 | `			if( z[intEnd]==',' ){ hasComma = 1; }` |
|    143 | 4779 | `			intEnd++;` |
|      1 | 4780 | `		}` |
|     25 | 4781 | `		if( hasComma ){` |
|     25 | 4782 | `			segStart = s; segIdx = 0;` |
|    165 | 4783 | `			for( i=s; i<=intEnd; i++ ){` |
|    151 | 4784 | `				if( i==intEnd \|\| z[i]==',' ){` |
|     49 | 4785 | `					int segLen = i - segStart, k;` |
|     49 | 4786 | `					if( segIdx==0 ){ if( segLen<1 \|\| segLen>3 ){ return 0; } }` |
|     25 | 4787 | `					else if( segLen!=3 ){ return 0; }` |
|    119 | 4788 | `					for( k=segStart; k<i; k++ ){` |
|     81 | 4789 | `						if( !SyisDigit((unsigned char)z[k]) ){ return 0; }` |
|     81 | 4790 | `						zBuf[m++] = z[k];` |
|     41 | 4791 | `					}` |
|     39 | 4792 | `					segStart = i+1; segIdx++;` |
|     19 | 4793 | `				}` |
|     71 | 4794 | `			}` |
|      8 | 4795 | `		}else{` |
|    ! 0 | 4796 | `			for( i=s; i<intEnd; i++ ){ zBuf[m++] = z[i]; }` |
|      - | 4797 | `		}` |
|     27 | 4798 | `		for( i=intEnd; i<n; i++ ){` |
|     13 | 4799 | `			if( z[i]==',' ){ return 0; }` |
|     13 | 4800 | `			zBuf[m++] = z[i];` |
|      7 | 4801 | `		}` |
|     15 | 4802 | `		zv = zBuf; nv = m;` |
|      8 | 4803 | `	}else{` |
|     45 | 4804 | `		zv = z; nv = n;` |
|      - | 4805 | `	}` |
|     59 | 4806 | `	i = 0;` |
|     59 | 4807 | `	if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|    167 | 4808 | `	while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     59 | 4809 | `	if( i<nv && zv[i]=='.' ){` |
|     21 | 4810 | `		i++;` |
|     39 | 4811 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     10 | 4812 | `	}` |
|     59 | 4813 | `	if( !seenDigit ){ return 0; }` |
|     57 | 4814 | `	if( i<nv && (zv[i]=='e'\|\|zv[i]=='E') ){` |
|     29 | 4815 | `		i++;` |
|     29 | 4816 | `		if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|     29 | 4817 | `		if( i>=nv \|\| !SyisDigit((unsigned char)zv[i]) ){ return 0; }` |
|    105 | 4818 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; }` |
|     14 | 4819 | `	}` |
|     57 | 4820 | `	if( i!=nv ){ return 0; } /* trailing junk */` |
|      - | 4821 | `	/* The grammar above guarantees zv[0..nv) is a clean ASCII decimal float (no hex /` |
|      - | 4822 | `	 * inf / nan / trailing junk), so it is safe to hand to libc strtod, which — unlike` |
|      - | 4823 | `	 * SyStrToReal (15 sig-digits + exponent clamped to 308, so it silently saturates` |
|      - | 4824 | `	 * overflowing magnitudes to a finite value) — is overflow/underflow-aware and` |
|      - | 4825 | `	 * correctly rounded. strtod needs a NUL-terminated string: the ALLOW_THOUSAND path` |
|      - | 4826 | `	 * already built the span in zBuf (zv==zBuf); the plain path must copy it there (z is` |
|      - | 4827 | `	 * const + not NUL-terminated). nv <= n <= 500 < sizeof(zBuf) by the cap above.` |
|      - | 4828 | `	 * Matches PHP 8.5 byte-for-byte: reject overflow (-> +/-INF) and total underflow` |
|      - | 4829 | `	 * (-> 0.0), keep subnormals (nonzero, errno==ERANGE) and a genuine "0" (errno==0). */` |
|     53 | 4830 | `	if( zv != zBuf ){ SyMemcpy(zv,zBuf,(sxu32)nv); }` |
|     53 | 4831 | `	zBuf[nv] = 0;` |
|     53 | 4832 | `	errno = 0;` |
|     53 | 4833 | `	d = strtod(zBuf,0);` |
|     53 | 4834 | `	if( errno == ERANGE && (d == HUGE_VAL \|\| d == -HUGE_VAL \|\| d == 0.0) ){` |
|     15 | 4835 | `		return 0;` |
|      - | 4836 | `	}` |
|     39 | 4837 | `	*pOut = d;` |
|     39 | 4838 | `	return 1;` |
|     35 | 4839 | `}` |
|      - | 4840 | `/* FILTER_VALIDATE_BOOLEAN. Returns 1 if the string is recognized (sets *pBool),` |
|      - | 4841 | ` * 0 if it is unrecognized (the failure path). "0"/"false"/"" are recognized as` |
|      - | 4842 | ` * false, NOT failures. */` |
|     33 | 4843 | `static int FvValidateBool(const char *z,int n,int *pBool){` |
|     33 | 4844 | `	FvTrim(&z,&n);` |
|     32 | 4845 | `	if( (n==1 && z[0]=='1') \|\| (n==4 && SyStrnicmp(z,"true",4)==0)` |
|     25 | 4846 | `	    \|\| (n==2 && SyStrnicmp(z,"on",2)==0) \|\| (n==3 && SyStrnicmp(z,"yes",3)==0) ){` |
|     11 | 4847 | `		*pBool = 1; return 1;` |
|      - | 4848 | `	}` |
|     22 | 4849 | `	if( n==0 \|\| (n==1 && z[0]=='0') \|\| (n==5 && SyStrnicmp(z,"false",5)==0)` |
|     11 | 4850 | `	    \|\| (n==3 && SyStrnicmp(z,"off",3)==0) \|\| (n==2 && SyStrnicmp(z,"no",2)==0) ){` |
|     11 | 4851 | `		*pBool = 0; return 1;` |
|      - | 4852 | `	}` |
|      9 | 4853 | `	return 0;` |
|     15 | 4854 | `}` |
|      - | 4855 | `/* IPv4 dotted-quad: exactly 4 octets 0..255, no leading zeros. */` |
|     33 | 4856 | `static int FvValidateIp4(const char *z,int n){` |
|     33 | 4857 | `	int i = 0, parts = 0;` |
|     77 | 4858 | `	while( i<n ){` |
|     65 | 4859 | `		int val = 0, digits = 0, start = i;` |
|    143 | 4860 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){` |
|     85 | 4861 | `			val = val*10 + (z[i]-'0');` |
|     85 | 4862 | `			if( val>255 ){ return 0; }` |
|     79 | 4863 | `			digits++; i++;` |
|      1 | 4864 | `		}` |
|     59 | 4865 | `		if( digits==0 \|\| digits>3 ){ return 0; }` |
|     49 | 4866 | `		if( digits>1 && z[start]=='0' ){ return 0; } /* leading zero */` |
|     45 | 4867 | `		parts++;` |
|     45 | 4868 | `		if( parts>4 ){ return 0; }` |
|     45 | 4869 | `		if( i<n ){` |
|     33 | 4870 | `			if( z[i]!='.' ){ return 0; }` |
|     33 | 4871 | `			i++;` |
|     33 | 4872 | `			if( i>=n ){ return 0; } /* trailing dot */` |
|     16 | 4873 | `		}` |
|      1 | 4874 | `	}` |
|     13 | 4875 | `	return parts==4;` |
|     17 | 4876 | `}` |
|      - | 4877 | `/* A colon-separated run of IPv6 hextets with no "::" (n may be 0 -> 0 groups),` |
|      - | 4878 | ` * allowing a trailing embedded IPv4. Returns the 16-bit group count or -1. */` |
|     19 | 4879 | `static int FvIp6Hextets(const char *z,int n){` |
|     19 | 4880 | `	int i = 0, segStart = 0, groups = 0;` |
|     19 | 4881 | `	if( n==0 ){ return 0; }` |
|    145 | 4882 | `	while( i<=n ){` |
|    133 | 4883 | `		if( i==n \|\| z[i]==':' ){` |
|     23 | 4884 | `			int segLen = i - segStart, j, isV4 = 0;` |
|     23 | 4885 | `			if( segLen==0 ){ return -1; } /* an empty hextet (stray ':') */` |
|     77 | 4886 | `			for( j=segStart; j<i; j++ ){ if( z[j]=='.' ){ isV4 = 1; break; } }` |
|     23 | 4887 | `			if( isV4 ){` |
|     11 | 4888 | `				if( i!=n ){ return -1; } /* IPv4 only as the final token */` |
|     11 | 4889 | `				if( !FvValidateIp4(z+segStart,segLen) ){ return -1; }` |
|      5 | 4890 | `				groups += 2;` |
|      3 | 4891 | `			}else{` |
|     13 | 4892 | `				if( segLen>4 ){ return -1; }` |
|     47 | 4893 | `				for( j=segStart; j<i; j++ ){ if( SyHexToint((unsigned char)z[j])<0 ){ return -1; } }` |
|     13 | 4894 | `				groups++;` |
|      - | 4895 | `			}` |
|     17 | 4896 | `			segStart = i+1;` |
|      8 | 4897 | `		}` |
|    127 | 4898 | `		i++;` |
|      1 | 4899 | `	}` |
|     13 | 4900 | `	return groups;` |
|     10 | 4901 | `}` |
|      - | 4902 | `/* IPv6: at most one "::" zero-run; 8 groups exactly, or fewer when "::" present. */` |
|     19 | 4903 | `static int FvValidateIp6(const char *z,int n){` |
|     19 | 4904 | `	const char *zDbl = 0;` |
|      - | 4905 | `	int i, ga, gb;` |
|    139 | 4906 | `	for( i=0; i+1<n; i++ ){` |
|    123 | 4907 | `		if( z[i]==':' && z[i+1]==':' ){` |
|     13 | 4908 | `			if( zDbl ){ return 0; } /* a second "::" is invalid */` |
|     11 | 4909 | `			zDbl = z+i;` |
|      5 | 4910 | `		}` |
|     61 | 4911 | `	}` |
|     17 | 4912 | `	if( zDbl==0 ){` |
|      9 | 4913 | `		return FvIp6Hextets(z,n)==8;` |
|    ! 0 | 4914 | `	}else{` |
|      9 | 4915 | `		int lenA = (int)(zDbl - z);` |
|      9 | 4916 | `		int lenB = n - lenA - 2;` |
|      9 | 4917 | `		ga = (lenA==0) ? 0 : FvIp6Hextets(z,lenA);` |
|      9 | 4918 | `		gb = (lenB==0) ? 0 : FvIp6Hextets(zDbl+2,lenB);` |
|      9 | 4919 | `		if( ga<0 \|\| gb<0 ){ return 0; }` |
|      9 | 4920 | `		return (ga+gb)<=7; /* "::" stands for at least one zero group */` |
|      - | 4921 | `	}` |
|     10 | 4922 | `}` |
|     25 | 4923 | `static int FvValidateIp(const char *z,int n,int flags){` |
|     25 | 4924 | `	int v4 = (flags & FV_FLAG_IPV4), v6 = (flags & FV_FLAG_IPV6);` |
|     25 | 4925 | `	if( !v4 && !v6 ){ v4 = v6 = 1; } /* default accepts either family */` |
|     25 | 4926 | `	if( v4 && FvValidateIp4(z,n) ){ return 1; }` |
|     21 | 4927 | `	if( v6 && FvValidateIp6(z,n) ){ return 1; }` |
|     13 | 4928 | `	return 0;` |
|     13 | 4929 | `}` |
|      - | 4930 | `/* FILTER_VALIDATE_MAC: 17-char colon- or dash-separated hex (XX:XX:..:XX). */` |
|     11 | 4931 | `static int FvValidateMac(const char *z,int n){` |
|      - | 4932 | `	char sep;` |
|      - | 4933 | `	int i;` |
|     11 | 4934 | `	if( n!=17 ){ return 0; }` |
|      7 | 4935 | `	sep = z[2];` |
|      7 | 4936 | `	if( sep!=':' && sep!='-' ){ return 0; }` |
|    105 | 4937 | `	for( i=0; i<17; i++ ){` |
|    101 | 4938 | `		if( (i%3)==2 ){ if( z[i]!=sep ){ return 0; } }` |
|     71 | 4939 | `		else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }` |
|     50 | 4940 | `	}` |
|      5 | 4941 | `	return 1;` |
|      6 | 4942 | `}` |
|      - | 4943 | `/* FILTER_VALIDATE_EMAIL (best-effort: covers the common cases, not quoted local` |
|      - | 4944 | ` * parts or IP-literal domains). */` |
|     28 | 4945 | `static int FvValidateEmail(const char *z,int n){` |
|     28 | 4946 | `	int at = -1, i, localLen, domLen, labelStart, dotCount = 0;` |
|      - | 4947 | `	const char *zDom;` |
|     28 | 4948 | `	if( n==0 \|\| n>320 ){ return 0; }` |
|    201 | 4949 | `	for( i=0; i<n; i++ ){` |
|    181 | 4950 | `		if( z[i]=='@' ){ if( at>=0 ){ return 0; } at = i; }` |
|     91 | 4951 | `	}` |
|     21 | 4952 | `	if( at<=0 \|\| at==n-1 ){ return 0; } /* one '@', non-empty local and domain */` |
|     21 | 4953 | `	localLen = at;` |
|     21 | 4954 | `	zDom = z + at + 1;` |
|     21 | 4955 | `	domLen = n - at - 1;` |
|     21 | 4956 | `	if( z[0]=='.' \|\| z[at-1]=='.' ){ return 0; }` |
|     57 | 4957 | `	for( i=0; i<localLen; i++ ){` |
|     43 | 4958 | `		unsigned char c = (unsigned char)z[i];` |
|     43 | 4959 | `		if( c<=' ' ){ return 0; }` |
|     41 | 4960 | `		if( c=='.' && i+1<localLen && z[i+1]=='.' ){ return 0; }` |
|     20 | 4961 | `	}` |
|     15 | 4962 | `	if( zDom[0]=='.' \|\| zDom[domLen-1]=='.' ){ return 0; }` |
|     13 | 4963 | `	labelStart = 0;` |
|     85 | 4964 | `	for( i=0; i<=domLen; i++ ){` |
|     75 | 4965 | `		if( i==domLen \|\| zDom[i]=='.' ){` |
|     25 | 4966 | `			int ll = i - labelStart;` |
|     25 | 4967 | `			if( ll==0 ){ return 0; } /* consecutive dots */` |
|     23 | 4968 | `			if( zDom[labelStart]=='-' \|\| zDom[i-1]=='-' ){ return 0; }` |
|     23 | 4969 | `			if( i<domLen ){ dotCount++; }` |
|     23 | 4970 | `			labelStart = i+1;` |
|     12 | 4971 | `		}else{` |
|     51 | 4972 | `			unsigned char c = (unsigned char)zDom[i];` |
|     51 | 4973 | `			if( !((c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9')\|\|c=='-') ){ return 0; }` |
|      - | 4974 | `		}` |
|     37 | 4975 | `	}` |
|     11 | 4976 | `	if( dotCount<1 ){ return 0; } /* PHP requires a dot in the domain (any TLD length) */` |
|      9 | 4977 | `	return 1;` |
|     15 | 4978 | `}` |
|      - | 4979 | `/* FILTER_VALIDATE_DOMAIN (lenient, matching PHP without FILTER_FLAG_HOSTNAME). */` |
|     11 | 4980 | `static int FvValidateDomain(const char *z,int n){` |
|      - | 4981 | `	int i;` |
|     11 | 4982 | `	if( n<1 \|\| n>253 \|\| z[0]=='.' ){ return 0; }` |
|     81 | 4983 | `	for( i=0; i<n; i++ ){` |
|     75 | 4984 | `		unsigned char c = (unsigned char)z[i];` |
|     75 | 4985 | `		if( c<=' ' ){ return 0; }` |
|     75 | 4986 | `		if( c=='.' && i+1<n && z[i+1]=='.' ){ return 0; }` |
|     37 | 4987 | `	}` |
|      7 | 4988 | `	return 1;` |
|      6 | 4989 | `}` |
|      - | 4990 | `/* FILTER_VALIDATE_URL: require a scheme and a host (PHP's filter is itself` |
|      - | 4991 | ` * parse_url-based, so PH7_VmHttpSplitURI tracks it closely). */` |
|     15 | 4992 | `static int FvValidateUrl(const char *z,int n){` |
|      - | 4993 | `	SyhttpUri sUri;` |
|     15 | 4994 | `	if( n==0 ){ return 0; }` |
|     15 | 4995 | `	SyZero(&sUri,(sxu32)sizeof(sUri));` |
|     15 | 4996 | `	if( PH7_VmHttpSplitURI(&sUri,z,(sxu32)n)!=SXRET_OK ){ return 0; }` |
|     15 | 4997 | `	return sUri.sScheme.nByte!=0 && sUri.sHost.nByte!=0;` |
|      8 | 4998 | `}` |
|      - | 4999 | `/* The Fv sanitizers build their result by appending directly to the call` |
|      - | 5000 | ` * context (ph7_result_string accumulates, like htmlspecialchars), emitting each` |
|      - | 5001 | ` * kept run in one call and seeding "" so an all-stripped input yields "". */` |
|      - | 5002 | `/* SANITIZE_NUMBER_INT (isFloat=0) / SANITIZE_NUMBER_FLOAT (isFloat=1). */` |
|     37 | 5003 | `static void FvSanitizeNumber(ph7_context *pCtx,const char *z,int n,int isFloat,int flags){` |
|     37 | 5004 | `	int i, runStart = 0;` |
|     37 | 5005 | `	ph7_result_string(pCtx,"",0);` |
|     97 | 5006 | `	for( i=0; i<n; i++ ){` |
|     91 | 5007 | `		char c = z[i];` |
|     91 | 5008 | `		int keep = (c>='0'&&c<='9') \|\| c=='+' \|\| c=='-';` |
|     91 | 5009 | `		if( !keep && isFloat ){` |
|     38 | 5010 | `			keep = (c=='.' && (flags & FV_FLAG_ALLOW_FRACTION))` |
|     23 | 5011 | `			    \|\| (c==',' && (flags & FV_FLAG_ALLOW_THOUSAND))` |
|     36 | 5012 | `			    \|\| ((c=='e'\|\|c=='E') && (flags & FV_FLAG_ALLOW_SCIENTIFIC));` |
|     12 | 5013 | `		}` |
|     61 | 5014 | `		if( !keep ){` |
|     33 | 5015 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     33 | 5016 | `			runStart = i+1;` |
|     16 | 5017 | `		}` |
|     31 | 5018 | `	}` |
|      7 | 5019 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 | 5020 | `}` |
|      - | 5021 | `/* Return non-zero when byte c must be stripped under the STRIP_* flags. Shared` |
|      - | 5022 | ` * by the UNSAFE_RAW string filter and SANITIZE_SPECIAL_CHARS. STRIP_LOW drops` |
|      - | 5023 | `` * bytes <32, STRIP_HIGH drops bytes >=127 (incl. DEL), STRIP_BACKTICK drops '`'.`` |
|      - | 5024 | ` * Matches php_filter_strip(); verified byte-exact vs php 8.5.7. */` |
|    287 | 5025 | `static int FvStripByte(unsigned char c,int flags){` |
|    287 | 5026 | `	if( (flags & FV_FLAG_STRIP_LOW)      && c<32 )    { return 1; }` |
|    281 | 5027 | `	if( (flags & FV_FLAG_STRIP_HIGH)     && c>=127 )  { return 1; }` |
|    269 | 5028 | `	if( (flags & FV_FLAG_STRIP_BACKTICK) && c==0x60 ) { return 1; }` |
|    267 | 5029 | `	return 0;` |
|    144 | 5030 | `}` |
|      - | 5031 | `/* FILTER_UNSAFE_RAW / FILTER_DEFAULT with flags: no default transform, but the` |
|      - | 5032 | ` * STRIP/ENCODE flags apply. Precedence (per php_filter_unsafe_raw, verified` |
|      - | 5033 | ` * vs php 8.5.7): a byte is first tested for stripping; a surviving byte is then` |
|      - | 5034 | ` * encoded as a decimal numeric entity if ENCODE_LOW (<32) / ENCODE_HIGH (>=127)` |
|      - | 5035 | ` * is set, and '&' becomes "&#38;" under ENCODE_AMP. So STRIP_LOW\|ENCODE_LOW` |
|      - | 5036 | ` * strips (nothing left to encode). Bytes are treated individually — ENCODE_HIGH` |
|      - | 5037 | ` * numeric-encodes each byte of a multibyte sequence separately, not the codepoint. */` |
|     25 | 5038 | `static void FvSanitizeString(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 5039 | `	int i, runStart = 0;` |
|     25 | 5040 | `	ph7_result_string(pCtx,"",0);` |
|    193 | 5041 | `	for( i=0; i<n; i++ ){` |
|    179 | 5042 | `		unsigned char c = (unsigned char)z[i];` |
|    179 | 5043 | `		if( FvStripByte(c,flags) ){` |
|     13 | 5044 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     13 | 5045 | `			runStart = i+1;` |
|     13 | 5046 | `			continue;` |
|      - | 5047 | `		}` |
|    167 | 5048 | `		if( c=='&' && (flags & FV_FLAG_ENCODE_AMP) ){` |
|      3 | 5049 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      3 | 5050 | `			ph7_result_string(pCtx,"&#38;",-1);` |
|      3 | 5051 | `			runStart = i+1;` |
|    166 | 5052 | `		}else if( (c<32 && (flags & FV_FLAG_ENCODE_LOW))` |
|    164 | 5053 | `		       \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     37 | 5054 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 5055 | `			ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|      9 | 5056 | `			runStart = i+1;` |
|      4 | 5057 | `		}` |
|     79 | 5058 | `	}` |
|     15 | 5059 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     15 | 5060 | `}` |
|      - | 5061 | `/* FILTER_SANITIZE_SPECIAL_CHARS: encode <>&"' and every control byte <32 as a` |
|      - | 5062 | ` * decimal numeric entity (&#60; &#38; &#34; ...). The STRIP_* flags remove bytes` |
|      - | 5063 | ` * before encoding; ENCODE_HIGH numeric-encodes surviving bytes >=127. Bytes >=128` |
|      - | 5064 | ` * are otherwise passed through verbatim (this filter is NOT UTF-8-aware — only the` |
|      - | 5065 | ` * FULL variant is). Byte-exact vs php 8.5.7. */` |
|     13 | 5066 | `static void FvSanitizeSpecial(ph7_context *pCtx,const char *z,int n,int flags){` |
|     13 | 5067 | `	int i, runStart = 0;` |
|      - | 5068 | `	const char *zEnt;` |
|     13 | 5069 | `	ph7_result_string(pCtx,"",0);` |
|    131 | 5070 | `	for( i=0; i<n; i++ ){` |
|    119 | 5071 | `		unsigned char c = (unsigned char)z[i];` |
|    119 | 5072 | `		if( FvStripByte(c,flags) ){` |
|      9 | 5073 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 5074 | `			runStart = i+1;` |
|      9 | 5075 | `			continue;` |
|      - | 5076 | `		}` |
|    111 | 5077 | `		switch( c ){` |
|      3 | 5078 | `		case '<':  zEnt = "&#60;"; break;` |
|      3 | 5079 | `		case '>':  zEnt = "&#62;"; break;` |
|     11 | 5080 | `		case '&':  zEnt = "&#38;"; break;` |
|      3 | 5081 | `		case '"':  zEnt = "&#34;"; break;` |
|      3 | 5082 | `		case '\'': zEnt = "&#39;"; break;` |
|     46 | 5083 | `		default:` |
|      - | 5084 | `			/* Control bytes <32 are always numeric-encoded; bytes >=127 only when` |
|      - | 5085 | `			 * ENCODE_HIGH is set. Everything else stays in the current run. */` |
|     93 | 5086 | `			if( c<32 \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     17 | 5087 | `				if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     17 | 5088 | `				ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|     17 | 5089 | `				runStart = i+1;` |
|      8 | 5090 | `			}` |
|     93 | 5091 | `			continue; /* keep in the current run */` |
|      - | 5092 | `		}` |
|     19 | 5093 | `		if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     19 | 5094 | `		ph7_result_string(pCtx,zEnt,-1); /* -1: length from strlen */` |
|     19 | 5095 | `		runStart = i+1;` |
|     10 | 5096 | `	}` |
|     13 | 5097 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     13 | 5098 | `}` |
|      - | 5099 | `/* HTML 4.01 named-entity table (codepoint -> "&name;") used by the UTF-8-aware` |
|      - | 5100 | ` * FULL_SPECIAL_CHARS filter, sorted ascending by codepoint for binary search.` |
|      - | 5101 | ` * Generated from php 8.5.7 (the exact set php_escape_html_entities emits for the` |
|      - | 5102 | ` * default document type); the five inline specials <>&"' are handled separately,` |
|      - | 5103 | ` * so every entry here is a codepoint >=0xA0. 248 rows. */` |
|      - | 5104 | `static const struct { sxu32 cp; const char *zEnt; } aHtml401Ent[] = {` |
|      - | 5105 | `	{0x00A0,"&nbsp;"},{0x00A1,"&iexcl;"},{0x00A2,"&cent;"},{0x00A3,"&pound;"},` |
|      - | 5106 | `	{0x00A4,"&curren;"},{0x00A5,"&yen;"},{0x00A6,"&brvbar;"},{0x00A7,"&sect;"},` |
|      - | 5107 | `	{0x00A8,"&uml;"},{0x00A9,"&copy;"},{0x00AA,"&ordf;"},{0x00AB,"&laquo;"},` |
|      - | 5108 | `	{0x00AC,"&not;"},{0x00AD,"&shy;"},{0x00AE,"&reg;"},{0x00AF,"&macr;"},` |
|      - | 5109 | `	{0x00B0,"&deg;"},{0x00B1,"&plusmn;"},{0x00B2,"&sup2;"},{0x00B3,"&sup3;"},` |
|      - | 5110 | `	{0x00B4,"&acute;"},{0x00B5,"&micro;"},{0x00B6,"&para;"},{0x00B7,"&middot;"},` |
|      - | 5111 | `	{0x00B8,"&cedil;"},{0x00B9,"&sup1;"},{0x00BA,"&ordm;"},{0x00BB,"&raquo;"},` |
|      - | 5112 | `	{0x00BC,"&frac14;"},{0x00BD,"&frac12;"},{0x00BE,"&frac34;"},{0x00BF,"&iquest;"},` |
|      - | 5113 | `	{0x00C0,"&Agrave;"},{0x00C1,"&Aacute;"},{0x00C2,"&Acirc;"},{0x00C3,"&Atilde;"},` |
|      - | 5114 | `	{0x00C4,"&Auml;"},{0x00C5,"&Aring;"},{0x00C6,"&AElig;"},{0x00C7,"&Ccedil;"},` |
|      - | 5115 | `	{0x00C8,"&Egrave;"},{0x00C9,"&Eacute;"},{0x00CA,"&Ecirc;"},{0x00CB,"&Euml;"},` |
|      - | 5116 | `	{0x00CC,"&Igrave;"},{0x00CD,"&Iacute;"},{0x00CE,"&Icirc;"},{0x00CF,"&Iuml;"},` |
|      - | 5117 | `	{0x00D0,"&ETH;"},{0x00D1,"&Ntilde;"},{0x00D2,"&Ograve;"},{0x00D3,"&Oacute;"},` |
|      - | 5118 | `	{0x00D4,"&Ocirc;"},{0x00D5,"&Otilde;"},{0x00D6,"&Ouml;"},{0x00D7,"&times;"},` |
|      - | 5119 | `	{0x00D8,"&Oslash;"},{0x00D9,"&Ugrave;"},{0x00DA,"&Uacute;"},{0x00DB,"&Ucirc;"},` |
|      - | 5120 | `	{0x00DC,"&Uuml;"},{0x00DD,"&Yacute;"},{0x00DE,"&THORN;"},{0x00DF,"&szlig;"},` |
|      - | 5121 | `	{0x00E0,"&agrave;"},{0x00E1,"&aacute;"},{0x00E2,"&acirc;"},{0x00E3,"&atilde;"},` |
|      - | 5122 | `	{0x00E4,"&auml;"},{0x00E5,"&aring;"},{0x00E6,"&aelig;"},{0x00E7,"&ccedil;"},` |
|      - | 5123 | `	{0x00E8,"&egrave;"},{0x00E9,"&eacute;"},{0x00EA,"&ecirc;"},{0x00EB,"&euml;"},` |
|      - | 5124 | `	{0x00EC,"&igrave;"},{0x00ED,"&iacute;"},{0x00EE,"&icirc;"},{0x00EF,"&iuml;"},` |
|      - | 5125 | `	{0x00F0,"&eth;"},{0x00F1,"&ntilde;"},{0x00F2,"&ograve;"},{0x00F3,"&oacute;"},` |
|      - | 5126 | `	{0x00F4,"&ocirc;"},{0x00F5,"&otilde;"},{0x00F6,"&ouml;"},{0x00F7,"&divide;"},` |
|      - | 5127 | `	{0x00F8,"&oslash;"},{0x00F9,"&ugrave;"},{0x00FA,"&uacute;"},{0x00FB,"&ucirc;"},` |
|      - | 5128 | `	{0x00FC,"&uuml;"},{0x00FD,"&yacute;"},{0x00FE,"&thorn;"},{0x00FF,"&yuml;"},` |
|      - | 5129 | `	{0x0152,"&OElig;"},{0x0153,"&oelig;"},{0x0160,"&Scaron;"},{0x0161,"&scaron;"},` |
|      - | 5130 | `	{0x0178,"&Yuml;"},{0x0192,"&fnof;"},{0x02C6,"&circ;"},{0x02DC,"&tilde;"},` |
|      - | 5131 | `	{0x0391,"&Alpha;"},{0x0392,"&Beta;"},{0x0393,"&Gamma;"},{0x0394,"&Delta;"},` |
|      - | 5132 | `	{0x0395,"&Epsilon;"},{0x0396,"&Zeta;"},{0x0397,"&Eta;"},{0x0398,"&Theta;"},` |
|      - | 5133 | `	{0x0399,"&Iota;"},{0x039A,"&Kappa;"},{0x039B,"&Lambda;"},{0x039C,"&Mu;"},` |
|      - | 5134 | `	{0x039D,"&Nu;"},{0x039E,"&Xi;"},{0x039F,"&Omicron;"},{0x03A0,"&Pi;"},` |
|      - | 5135 | `	{0x03A1,"&Rho;"},{0x03A3,"&Sigma;"},{0x03A4,"&Tau;"},{0x03A5,"&Upsilon;"},` |
|      - | 5136 | `	{0x03A6,"&Phi;"},{0x03A7,"&Chi;"},{0x03A8,"&Psi;"},{0x03A9,"&Omega;"},` |
|      - | 5137 | `	{0x03B1,"&alpha;"},{0x03B2,"&beta;"},{0x03B3,"&gamma;"},{0x03B4,"&delta;"},` |
|      - | 5138 | `	{0x03B5,"&epsilon;"},{0x03B6,"&zeta;"},{0x03B7,"&eta;"},{0x03B8,"&theta;"},` |
|      - | 5139 | `	{0x03B9,"&iota;"},{0x03BA,"&kappa;"},{0x03BB,"&lambda;"},{0x03BC,"&mu;"},` |
|      - | 5140 | `	{0x03BD,"&nu;"},{0x03BE,"&xi;"},{0x03BF,"&omicron;"},{0x03C0,"&pi;"},` |
|      - | 5141 | `	{0x03C1,"&rho;"},{0x03C2,"&sigmaf;"},{0x03C3,"&sigma;"},{0x03C4,"&tau;"},` |
|      - | 5142 | `	{0x03C5,"&upsilon;"},{0x03C6,"&phi;"},{0x03C7,"&chi;"},{0x03C8,"&psi;"},` |
|      - | 5143 | `	{0x03C9,"&omega;"},{0x03D1,"&thetasym;"},{0x03D2,"&upsih;"},{0x03D6,"&piv;"},` |
|      - | 5144 | `	{0x2002,"&ensp;"},{0x2003,"&emsp;"},{0x2009,"&thinsp;"},{0x200C,"&zwnj;"},` |
|      - | 5145 | `	{0x200D,"&zwj;"},{0x200E,"&lrm;"},{0x200F,"&rlm;"},{0x2013,"&ndash;"},` |
|      - | 5146 | `	{0x2014,"&mdash;"},{0x2018,"&lsquo;"},{0x2019,"&rsquo;"},{0x201A,"&sbquo;"},` |
|      - | 5147 | `	{0x201C,"&ldquo;"},{0x201D,"&rdquo;"},{0x201E,"&bdquo;"},{0x2020,"&dagger;"},` |
|      - | 5148 | `	{0x2021,"&Dagger;"},{0x2022,"&bull;"},{0x2026,"&hellip;"},{0x2030,"&permil;"},` |
|      - | 5149 | `	{0x2032,"&prime;"},{0x2033,"&Prime;"},{0x2039,"&lsaquo;"},{0x203A,"&rsaquo;"},` |
|      - | 5150 | `	{0x203E,"&oline;"},{0x2044,"&frasl;"},{0x20AC,"&euro;"},{0x2111,"&image;"},` |
|      - | 5151 | `	{0x2118,"&weierp;"},{0x211C,"&real;"},{0x2122,"&trade;"},{0x2135,"&alefsym;"},` |
|      - | 5152 | `	{0x2190,"&larr;"},{0x2191,"&uarr;"},{0x2192,"&rarr;"},{0x2193,"&darr;"},` |
|      - | 5153 | `	{0x2194,"&harr;"},{0x21B5,"&crarr;"},{0x21D0,"&lArr;"},{0x21D1,"&uArr;"},` |
|      - | 5154 | `	{0x21D2,"&rArr;"},{0x21D3,"&dArr;"},{0x21D4,"&hArr;"},{0x2200,"&forall;"},` |
|      - | 5155 | `	{0x2202,"&part;"},{0x2203,"&exist;"},{0x2205,"&empty;"},{0x2207,"&nabla;"},` |
|      - | 5156 | `	{0x2208,"&isin;"},{0x2209,"&notin;"},{0x220B,"&ni;"},{0x220F,"&prod;"},` |
|      - | 5157 | `	{0x2211,"&sum;"},{0x2212,"&minus;"},{0x2217,"&lowast;"},{0x221A,"&radic;"},` |
|      - | 5158 | `	{0x221D,"&prop;"},{0x221E,"&infin;"},{0x2220,"&ang;"},{0x2227,"&and;"},` |
|      - | 5159 | `	{0x2228,"&or;"},{0x2229,"&cap;"},{0x222A,"&cup;"},{0x222B,"&int;"},` |
|      - | 5160 | `	{0x2234,"&there4;"},{0x223C,"&sim;"},{0x2245,"&cong;"},{0x2248,"&asymp;"},` |
|      - | 5161 | `	{0x2260,"&ne;"},{0x2261,"&equiv;"},{0x2264,"&le;"},{0x2265,"&ge;"},` |
|      - | 5162 | `	{0x2282,"&sub;"},{0x2283,"&sup;"},{0x2284,"&nsub;"},{0x2286,"&sube;"},` |
|      - | 5163 | `	{0x2287,"&supe;"},{0x2295,"&oplus;"},{0x2297,"&otimes;"},{0x22A5,"&perp;"},` |
|      - | 5164 | `	{0x22C5,"&sdot;"},{0x2308,"&lceil;"},{0x2309,"&rceil;"},{0x230A,"&lfloor;"},` |
|      - | 5165 | `	{0x230B,"&rfloor;"},{0x2329,"&lang;"},{0x232A,"&rang;"},{0x25CA,"&loz;"},` |
|      - | 5166 | `	{0x2660,"&spades;"},{0x2663,"&clubs;"},{0x2665,"&hearts;"},{0x2666,"&diams;"}` |
|      - | 5167 | `};` |
|      - | 5168 | `/* Binary-search aHtml401Ent[] for cp; return its "&name;" entity or 0. */` |
|     41 | 5169 | `static const char *FvHtml401Lookup(sxu32 cp){` |
|     41 | 5170 | `	int lo = 0, hi = (int)SX_ARRAYSIZE(aHtml401Ent) - 1;` |
|    323 | 5171 | `	while( lo <= hi ){` |
|    309 | 5172 | `		int mid = (lo + hi) / 2;` |
|    309 | 5173 | `		sxu32 c = aHtml401Ent[mid].cp;` |
|    309 | 5174 | `		if( c == cp ){ return aHtml401Ent[mid].zEnt; }` |
|    283 | 5175 | `		if( c < cp ){ lo = mid + 1; } else { hi = mid - 1; }` |
|      1 | 5176 | `	}` |
|     15 | 5177 | `	return 0;` |
|     21 | 5178 | `}` |
|      - | 5179 | `/* Decode one strict-UTF-8 sequence at p (< zEnd). On success returns its byte` |
|      - | 5180 | ` * length (1..4) and sets *pCp to the codepoint; on any malformed, overlong,` |
|      - | 5181 | ` * surrogate, truncated or out-of-range (>U+10FFFF) sequence returns 0. Matches` |
|      - | 5182 | ` * PHP's UTF-8 validation used by FULL_SPECIAL_CHARS (verified vs php 8.5.7). */` |
|    101 | 5183 | `static int FvUtf8Next(const unsigned char *p,const unsigned char *zEnd,sxu32 *pCp){` |
|    101 | 5184 | `	unsigned char c = p[0];` |
|    101 | 5185 | `	if( c < 0x80 ){ *pCp = c; return 1; }` |
|    101 | 5186 | `	if( c < 0xC2 ){ return 0; }              /* 0x80-0xBF stray cont / 0xC0-0xC1 overlong */` |
|     99 | 5187 | `	if( c < 0xE0 ){                          /* 2-byte: U+0080..U+07FF */` |
|     47 | 5188 | `		if( zEnd-p < 2 \|\| (p[1]&0xC0)!=0x80 ){ return 0; }` |
|     45 | 5189 | `		*pCp = ((sxu32)(c&0x1F)<<6) \| (p[1]&0x3F);` |
|     45 | 5190 | `		return 2;` |
|      - | 5191 | `	}` |
|     53 | 5192 | `	if( c < 0xF0 ){                          /* 3-byte: U+0800..U+FFFF minus surrogates */` |
|      - | 5193 | `		sxu32 cp;` |
|     47 | 5194 | `		if( zEnd-p < 3 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 ){ return 0; }` |
|     33 | 5195 | `		cp = ((sxu32)(c&0x0F)<<12) \| ((sxu32)(p[1]&0x3F)<<6) \| (p[2]&0x3F);` |
|     33 | 5196 | `		if( cp < 0x800 \|\| (cp>=0xD800 && cp<=0xDFFF) ){ return 0; }` |
|     29 | 5197 | `		*pCp = cp;` |
|     29 | 5198 | `		return 3;` |
|      - | 5199 | `	}` |
|      7 | 5200 | `	if( c < 0xF5 ){                          /* 4-byte: U+10000..U+10FFFF */` |
|      - | 5201 | `		sxu32 cp;` |
|      5 | 5202 | `		if( zEnd-p < 4 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 \|\| (p[3]&0xC0)!=0x80 ){ return 0; }` |
|      5 | 5203 | `		cp = ((sxu32)(c&0x07)<<18) \| ((sxu32)(p[1]&0x3F)<<12) \| ((sxu32)(p[2]&0x3F)<<6) \| (p[3]&0x3F);` |
|      5 | 5204 | `		if( cp < 0x10000 \|\| cp > 0x10FFFF ){ return 0; }` |
|      5 | 5205 | `		*pCp = cp;` |
|      5 | 5206 | `		return 4;` |
|      - | 5207 | `	}` |
|      3 | 5208 | `	return 0;                                /* 0xF5-0xFF */` |
|     51 | 5209 | `}` |
|      - | 5210 | `/* FILTER_SANITIZE_FULL_SPECIAL_CHARS: htmlentities-style, UTF-8-aware. Encodes` |
|      - | 5211 | ` * <>&"' as named entities ("'" -> &#039;; quotes suppressed under NO_ENCODE_QUOTES),` |
|      - | 5212 | ` * and every valid UTF-8 codepoint with an HTML 4.01 named entity as that entity;` |
|      - | 5213 | ` * valid codepoints without a named entity (and low control bytes) pass through` |
|      - | 5214 | ` * verbatim. If the input contains ANY invalid UTF-8 the whole result is "".` |
|      - | 5215 | ` * The STRIP/ENCODE flags do NOT apply to this filter (only NO_ENCODE_QUOTES).` |
|      - | 5216 | ` * php's filter does NOT re-encode valid pre-existing entities ("&amp;" stays,` |
|      - | 5217 | ` * "&bogus;" becomes "&amp;bogus;"), i.e. double_encode=false semantics —` |
|      - | 5218 | ` * exactly htmlentities(ENT_QUOTES\|ENT_HTML401, double_encode: false), so this` |
|      - | 5219 | ` * delegates to the shared encoder. Byte-exact vs php 8.5.7. */` |
|     25 | 5220 | `static void FvSanitizeFull(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 5221 | `	int iEntFlags = (flags & FV_FLAG_NO_ENCODE_QUOTES) ? 0 : PH7_ENT_QUOTES;` |
|     25 | 5222 | `	HtmlEscape(pCtx,z,n,iEntFlags,1/*bAll*/,0/*bDoubleEncode*/);` |
|     25 | 5223 | `}` |
|      - | 5224 | `/* ---------------------------------------------------------------------------` |
|      - | 5225 | ` * UTF-8-aware HTML entity core (htmlspecialchars/htmlentities family).` |
|      - | 5226 | ` * Prototyped next to the five builtins earlier in this file; lives here so it` |
|      - | 5227 | ` * can share aHtml401Ent[]/FvHtml401Lookup()/FvUtf8Next() with the filter_var` |
|      - | 5228 | ` * FULL_SPECIAL_CHARS filter above. Byte-exact vs php 8.5.7 (oracle-swept).` |
|      - | 5229 | ` * ------------------------------------------------------------------------ */` |
|      - | 5230 | `/* Encode cp as UTF-8 into zBuf (>= 4 bytes); return the byte length 1..4.` |
|      - | 5231 | ` * Thin wrapper over the engine-wide SX_WRITE_UTF8 (sxmacros.h). */` |
|    585 | 5232 | `static int HtmlCpUtf8(sxu32 cp,char *zBuf){` |
|    585 | 5233 | `	sxu8 *z = (sxu8 *)zBuf;` |
|    585 | 5234 | `	SX_WRITE_UTF8(z,cp);` |
|    585 | 5235 | `	return (int)(z - (sxu8 *)zBuf);` |
|      1 | 5236 | `}` |
|      - | 5237 | `/* Doctype-allowed codepoint test (php's unicode_cp_is_allowed) — gates what a` |
|      - | 5238 | ` * numeric reference may DECODE to. Oracle-pinned per doctype: HTML401` |
|      - | 5239 | ` * disallows C0 (except TAB/LF/CR) and DEL..U+009F; XML1 and XHTML share the` |
|      - | 5240 | ` * XML rules — DEL..U+009F allowed, U+FFFE/U+FFFF excluded; HTML5 swaps CR` |
|      - | 5241 | ` * for FF (0x0C) and excludes the noncharacters (U+FDD0..U+FDEF and every` |
|      - | 5242 | ` * U+xFFFE/U+xFFFF). Surrogates are disallowed everywhere. */` |
|     91 | 5243 | `static int HtmlCpAllowed(sxu32 cp,int iFlags){` |
|     91 | 5244 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|     91 | 5245 | `	if( cp==0x09 \|\| cp==0x0A ){ return 1; }` |
|     87 | 5246 | `	if( cp==0x0D ){ return iDoc != PH7_ENT_DOC_HTML5; }` |
|     85 | 5247 | `	if( cp==0x0C ){ return iDoc == PH7_ENT_DOC_HTML5; }` |
|     85 | 5248 | `	if( cp < 0x20 \|\| cp > 0x10FFFF ){ return 0; }` |
|     79 | 5249 | `	if( cp>=0xD800 && cp<=0xDFFF ){ return 0; }` |
|     77 | 5250 | `	if( cp>=0x7F && cp<=0x9F ){ return iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML; }` |
|     71 | 5251 | `	if( iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML ){` |
|    ! 0 | 5252 | `		return cp!=0xFFFE && cp!=0xFFFF;` |
|      - | 5253 | `	}` |
|     71 | 5254 | `	if( iDoc == PH7_ENT_DOC_HTML5 ){` |
|      9 | 5255 | `		if( cp>=0xFDD0 && cp<=0xFDEF ){ return 0; }` |
|      9 | 5256 | `		if( (cp & 0xFFFF) >= 0xFFFE ){ return 0; }` |
|      4 | 5257 | `	}` |
|     71 | 5258 | `	return 1;` |
|     46 | 5259 | `}` |
|      - | 5260 | `/* The ENT_DISALLOWED gate for RAW characters on the ENCODE side. Same as the` |
|      - | 5261 | ` * decode gate except CR under HTML5: php's encode-side unicode_cp_is_allowed` |
|      - | 5262 | ` * keeps a literal "\r" verbatim under ENT_HTML5\|ENT_DISALLOWED while the` |
|      - | 5263 | ` * decode side leaves "&#13;" un-decoded (oracle-pinned at flags 176). */` |
|      9 | 5264 | `static int HtmlCpAllowedEncode(sxu32 cp,int iFlags){` |
|      9 | 5265 | `	if( cp==0x0D && (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 ){ return 1; }` |
|      9 | 5266 | `	return HtmlCpAllowed(cp,iFlags);` |
|      5 | 5267 | `}` |
|      - | 5268 | `/* Numeric-reference validity for the double_encode=false "is this already a` |
|      - | 5269 | ` * valid entity" test — a MUCH looser predicate than the decode gate above:` |
|      - | 5270 | ` * any codepoint <= U+10FFFF is valid (controls and surrogates included, every` |
|      - | 5271 | ` * doctype). ENT_DISALLOWED re-tightens non-HTML401 doctypes to the decode` |
|      - | 5272 | ` * gate, except that HTML5 exempts surrogates. All oracle-pinned: &#0; and` |
|      - | 5273 | ` * &#xD800; stay verbatim at flags 11 and 139; flags -1 (HTML5+DISALLOWED)` |
|      - | 5274 | ` * re-encodes &#0; and &#x10FFFF; but still keeps &#xD800;; flags 144` |
|      - | 5275 | ` * (XML1+DISALLOWED) re-encodes &#xD800;. */` |
|      9 | 5276 | `static int HtmlNumericAllowed(sxu32 cp,int iFlags){` |
|      9 | 5277 | `	if( cp > 0x10FFFF ){ return 0; }` |
|      7 | 5278 | `	if( (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML401 ){ return 1; /* never tightened */ }` |
|    ! 0 | 5279 | `	if( (iFlags & PH7_ENT_DISALLOWED)` |
|    ! 0 | 5280 | `	 && !((iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 && cp>=0xD800 && cp<=0xDFFF)` |
|    ! 0 | 5281 | `	 && !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|    ! 0 | 5282 | `	return 1;` |
|      5 | 5283 | `}` |
|      - | 5284 | `/* How many bytes the malformed UTF-8 sequence at p consumes — php's` |
|      - | 5285 | ` * get_next_char failure step (one U+FFFD substitution / one ENT_IGNORE drop` |
|      - | 5286 | ` * per MAXIMAL invalid subpart, not per byte): a prefix-valid sequence eats` |
|      - | 5287 | ` * its continuation bytes ("\xE0\x80\xAF" is ONE unit) while a byte that could` |
|      - | 5288 | ` * start a new sequence is left for the next round. */` |
|      5 | 5289 | `static int HtmlUtf8Trail(unsigned char c){ return c>=0x80 && c<=0xBF; }` |
|     11 | 5290 | `static int HtmlUtf8Lead(unsigned char c){ return c<0x80 \|\| (c>=0xC2 && c<=0xF4); }` |
|     15 | 5291 | `static int HtmlUtf8FailAdvance(const unsigned char *p,const unsigned char *zEnd){` |
|     15 | 5292 | `	unsigned char c = p[0];` |
|     15 | 5293 | `	int nAvail = (int)(zEnd - p);` |
|     15 | 5294 | `	if( c < 0xC2 \|\| c > 0xF4 ){ return 1; } /* stray trail / C0-C1 / F5-FF */` |
|     13 | 5295 | `	if( c < 0xE0 ){` |
|      3 | 5296 | `		if( nAvail < 2 ){ return 1; }` |
|      3 | 5297 | `		return HtmlUtf8Lead(p[1]) ? 1 : 2;` |
|      - | 5298 | `	}` |
|     11 | 5299 | `	if( c < 0xF0 ){` |
|     11 | 5300 | `		if( nAvail >= 3 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) ){` |
|      3 | 5301 | `			return 3; /* complete but overlong/surrogate */` |
|      - | 5302 | `		}` |
|      9 | 5303 | `		if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 5304 | `		if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 5305 | `		return 3;` |
|      - | 5306 | `	}` |
|    ! 0 | 5307 | `	if( nAvail >= 4 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) && HtmlUtf8Trail(p[3]) ){` |
|    ! 0 | 5308 | `		return 4; /* complete but overlong / > U+10FFFF */` |
|      - | 5309 | `	}` |
|    ! 0 | 5310 | `	if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 5311 | `	if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 5312 | `	if( nAvail < 4 \|\| HtmlUtf8Lead(p[3]) ){ return 3; }` |
|    ! 0 | 5313 | `	return 4;` |
|      8 | 5314 | `}` |
|      - | 5315 | `/* The basic special entities, shared by named matching, the hsc_decode` |
|      - | 5316 | ` * numeric whitelist and the translation-table builder so the sets can never` |
|      - | 5317 | ` * drift apart. (&apos; is not an HTML 4.01 entity — doctype-gated below.) */` |
|      - | 5318 | `static const struct { const char *zEnt; int n; sxu32 cp; } aHtmlSpecEnt[] = {` |
|      - | 5319 | `	{"&amp;",5,38},{"&lt;",4,60},{"&gt;",4,62},{"&quot;",6,34},{"&apos;",6,39}` |
|      - | 5320 | `};` |
|      - | 5321 | `/* Does this doctype consult the named-entity table (aHtml401Ent)? XML 1.0 has` |
|      - | 5322 | ` * no named entities beyond the specials; XHTML/HTML5 are approximated by the` |
|      - | 5323 | ` * HTML 4.01 table (documented divergence, PLAN.md §3.9). */` |
|     63 | 5324 | `static int HtmlDocHasNamedTable(int iDoc){` |
|     63 | 5325 | `	return iDoc != PH7_ENT_DOC_XML1;` |
|      1 | 5326 | `}` |
|      - | 5327 | `/* The single-quote entity per doctype. Oracle-pinned asymmetry: for every` |
|      - | 5328 | ` * non-HTML401 doctype htmlspecialchars emits &apos; while htmlentities` |
|      - | 5329 | ` * (bEntities) keeps &#039; under XHTML too. The translation table mirrors` |
|      - | 5330 | ` * whichever function the requested table belongs to. */` |
|     29 | 5331 | `static const char *HtmlAposEntity(int iDoc,int bEntities){` |
|     29 | 5332 | `	if( iDoc == PH7_ENT_DOC_HTML401 \|\| (bEntities && iDoc == PH7_ENT_DOC_XHTML) ){` |
|     21 | 5333 | `		return "&#039;";` |
|      - | 5334 | `	}` |
|      9 | 5335 | `	return "&apos;";` |
|     15 | 5336 | `}` |
|      - | 5337 | `/* Try to parse one HTML entity at z (z[0]=='&', z < zEnd). bFull selects the` |
|      - | 5338 | ` * html_entity_decode set (doctype named table + any allowed numeric ref) vs` |
|      - | 5339 | ` * the htmlspecialchars_decode set (the basic specials + quote numerics only).` |
|      - | 5340 | ` * Named matching is case-SENSITIVE and the ';' is required (both PHP-exact);` |
|      - | 5341 | ` * numeric refs accept dec/hex (x or X) with any number of leading zeros but` |
|      - | 5342 | ` * reject out-of-range, surrogate and doctype-disallowed codepoints (the` |
|      - | 5343 | ` * caller then leaves the source verbatim). Quote-flag gating is NOT applied` |
|      - | 5344 | ` * here — the same routine doubles as the "is this a valid entity" test for` |
|      - | 5345 | ` * double_encode=false, which ignores the quote bits (oracle-pinned).` |
|      - | 5346 | ` * bEncodeCheck selects the looser HtmlNumericAllowed predicate used by that` |
|      - | 5347 | ` * double_encode test; decode callers pass 0 for the HtmlCpAllowed gate.` |
|      - | 5348 | ` * On success sets *pCp / *pnConsumed and returns 1. */` |
|    172 | 5349 | `static int HtmlParseEntity(const unsigned char *z,const unsigned char *zEnd,` |
|      1 | 5350 | `                           int iFlags,int bFull,int bEncodeCheck,sxu32 *pCp,int *pnConsumed){` |
|    173 | 5351 | `	int nAvail = (int)(zEnd - z);` |
|    173 | 5352 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 5353 | `	sxu32 n;` |
|    173 | 5354 | `	if( nAvail < 4 ){ return 0; } /* shortest entities: &lt; &#9; */` |
|    169 | 5355 | `	if( z[1] == '#' ){` |
|      - | 5356 | `		/* Numeric reference */` |
|     89 | 5357 | `		sxu32 cp = 0;` |
|     89 | 5358 | `		int i = 2, bHex = 0, nDig = 0;` |
|     89 | 5359 | `		if( z[i]=='x' \|\| z[i]=='X' ){ bHex = 1; i++; }` |
|    317 | 5360 | `		for( ; i < nAvail && z[i] != ';' ; i++ ){` |
|      - | 5361 | `			int v;` |
|    221 | 5362 | `			unsigned char c = z[i];` |
|    221 | 5363 | `			if( c>='0' && c<='9' ){ v = c - '0'; }` |
|     17 | 5364 | `			else if( bHex && c>='a' && c<='f' ){ v = c - 'a' + 10; }` |
|     17 | 5365 | `			else if( bHex && c>='A' && c<='F' ){ v = c - 'A' + 10; }` |
|    ! 0 | 5366 | `			else { return 0; }` |
|      - | 5367 | `			/* Stop accumulating once out of range (keeps validating the shape;` |
|      - | 5368 | `			 * max intermediate is 0x10FFFF*16+15, no sxu32 overflow). */` |
|    221 | 5369 | `			if( cp <= 0x10FFFF ){ cp = cp * (bHex ? 16 : 10) + (sxu32)v; }` |
|    221 | 5370 | `			nDig++;` |
|    111 | 5371 | `		}` |
|     97 | 5372 | `		if( nDig == 0 \|\| i >= nAvail ){ return 0; } /* no digits / no ';' */` |
|     97 | 5373 | `		if( bEncodeCheck ? !HtmlNumericAllowed(cp,iFlags) : !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|     83 | 5374 | `		if( !bFull ){` |
|      - | 5375 | `			/* hsc_decode: numeric refs to the five specials only. */` |
|     99 | 5376 | `			for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) && aHtmlSpecEnt[n].cp != cp ; n++ ){}` |
|     25 | 5377 | `			if( n >= SX_ARRAYSIZE(aHtmlSpecEnt) ){ return 0; }` |
|     11 | 5378 | `		}` |
|     75 | 5379 | `		*pCp = cp;` |
|     75 | 5380 | `		*pnConsumed = i + 1;` |
|     75 | 5381 | `		return 1;` |
|      - | 5382 | `	}` |
|      - | 5383 | `	/* Named reference — every entity name starts with a letter, so anything` |
|      - | 5384 | `	 * else can bail out before touching the tables. */` |
|     81 | 5385 | `	if( !((z[1]>='a' && z[1]<='z') \|\| (z[1]>='A' && z[1]<='Z')) ){ return 0; }` |
|    287 | 5386 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) ; n++ ){` |
|    265 | 5387 | `		if( aHtmlSpecEnt[n].cp == 39 && iDoc == PH7_ENT_DOC_HTML401 ){ continue; }` |
|    243 | 5388 | `		if( nAvail >= aHtmlSpecEnt[n].n && SyMemcmp(z,aHtmlSpecEnt[n].zEnt,(sxu32)aHtmlSpecEnt[n].n) == 0 ){` |
|     53 | 5389 | `			*pCp = aHtmlSpecEnt[n].cp;` |
|     53 | 5390 | `			*pnConsumed = aHtmlSpecEnt[n].n;` |
|     53 | 5391 | `			return 1;` |
|      - | 5392 | `		}` |
|     96 | 5393 | `	}` |
|     23 | 5394 | `	if( bFull && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 5395 | `		/* Linear scan of the 248-row table: runs only at '&'-then-letter` |
|      - | 5396 | `		 * positions and guarantees the decode set can never drift from the` |
|      - | 5397 | `		 * encode table. The first-letter guard skips the SyStrlen/SyMemcmp` |
|      - | 5398 | `		 * for ~96% of rows. */` |
|   3369 | 5399 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|      - | 5400 | `			sxu32 nEnt;` |
|   3357 | 5401 | `			if( z[1] != (unsigned char)aHtml401Ent[n].zEnt[1] ){ continue; }` |
|    121 | 5402 | `			nEnt = SyStrlen(aHtml401Ent[n].zEnt);` |
|    121 | 5403 | `			if( (sxu32)nAvail >= nEnt && SyMemcmp(z,aHtml401Ent[n].zEnt,nEnt) == 0 ){` |
|      7 | 5404 | `				*pCp = aHtml401Ent[n].cp;` |
|      7 | 5405 | `				*pnConsumed = (int)nEnt;` |
|      7 | 5406 | `				return 1;` |
|      - | 5407 | `			}` |
|     58 | 5408 | `		}` |
|      6 | 5409 | `	}` |
|     17 | 5410 | `	return 0;` |
|     88 | 5411 | `}` |
|      - | 5412 | `/* Shared encoder for htmlspecialchars (bAll=0) and htmlentities (bAll=1).` |
|      - | 5413 | ` * Invalid UTF-8 policy: ENT_IGNORE drops the byte (and wins over SUBSTITUTE),` |
|      - | 5414 | ` * ENT_SUBSTITUTE emits one U+FFFD per invalid byte, neither -> the whole` |
|      - | 5415 | ` * result is "" (pre-validated in a first pass: the accumulating result API` |
|      - | 5416 | ` * cannot roll back — same reason FvSanitizeFull is two-pass). */` |
|     94 | 5417 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 5418 | `                       int iFlags,int bAll,int bDoubleEncode){` |
|     95 | 5419 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     95 | 5420 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|      - | 5421 | `	const unsigned char *runStart;` |
|     95 | 5422 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 5423 | `	sxu32 cp;` |
|     95 | 5424 | `	if( (iFlags & (PH7_ENT_IGNORE\|PH7_ENT_SUBSTITUTE)) == 0 ){` |
|      - | 5425 | `		/* Pass 1: any malformed sequence rejects the entire input. ASCII` |
|      - | 5426 | `		 * bytes cannot be malformed, so skip them without the decoder. */` |
|    381 | 5427 | `		while( p < zEnd ){` |
|      - | 5428 | `			int len;` |
|    323 | 5429 | `			if( *p < 0x80 ){ p++; continue; }` |
|     37 | 5430 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     37 | 5431 | `			if( len == 0 ){ ph7_result_string(pCtx,"",0); return; }` |
|     27 | 5432 | `			p += len;` |
|      1 | 5433 | `		}` |
|     59 | 5434 | `		p = (const unsigned char *)zIn;` |
|     29 | 5435 | `	}` |
|     85 | 5436 | `	runStart = p;` |
|     85 | 5437 | `	ph7_result_string(pCtx,"",0);` |
|    455 | 5438 | `	while( p < zEnd ){` |
|    371 | 5439 | `		const char *zEnt = 0;` |
|      - | 5440 | `		int len;` |
|    371 | 5441 | `		if( *p < 0x80 ){` |
|    307 | 5442 | `			len = 1;` |
|    307 | 5443 | `			switch( *p ){` |
|     25 | 5444 | `			case '<': zEnt = "&lt;"; break;` |
|     25 | 5445 | `			case '>': zEnt = "&gt;"; break;` |
|     18 | 5446 | `			case '&':` |
|     37 | 5447 | `				zEnt = "&amp;";` |
|     37 | 5448 | `				if( !bDoubleEncode ){` |
|      - | 5449 | `					sxu32 eCp; int nEat;` |
|     25 | 5450 | `					if( HtmlParseEntity(p,zEnd,iFlags,1,1,&eCp,&nEat) ){` |
|      - | 5451 | `						/* A valid existing entity: keep it verbatim. */` |
|     13 | 5452 | `						zEnt = 0;` |
|     13 | 5453 | `						len = nEat;` |
|      6 | 5454 | `					}` |
|     12 | 5455 | `				}` |
|     37 | 5456 | `				break;` |
|     10 | 5457 | `			case '"':` |
|     21 | 5458 | `				if( iFlags & PH7_ENT_QUOTE_DOUBLE ){ zEnt = "&quot;"; }` |
|     21 | 5459 | `				break;` |
|     12 | 5460 | `			case '\'':` |
|     25 | 5461 | `				if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|     23 | 5462 | `					zEnt = HtmlAposEntity(iDoc,bAll);` |
|     11 | 5463 | `				}` |
|     25 | 5464 | `				break;` |
|     89 | 5465 | `			default:` |
|    179 | 5466 | `				if( (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode((sxu32)*p,iFlags) ){` |
|    ! 0 | 5467 | `					zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 5468 | `				}` |
|    178 | 5469 | `				break;` |
|      - | 5470 | `			}` |
|    154 | 5471 | `		}else{` |
|     65 | 5472 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     65 | 5473 | `			if( len == 0 ){` |
|      - | 5474 | `				/* Malformed subpart (IGNORE or SUBSTITUTE is set, else pass 1` |
|      - | 5475 | `				 * would have rejected): drop it or emit ONE U+FFFD for the` |
|      - | 5476 | `				 * whole unit (php substitutes per maximal invalid subpart). */` |
|     15 | 5477 | `				if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|     15 | 5478 | `				if( (iFlags & PH7_ENT_IGNORE) == 0 ){ ph7_result_string(pCtx,"\xEF\xBF\xBD",3); }` |
|     15 | 5479 | `				p += HtmlUtf8FailAdvance(p,zEnd);` |
|     15 | 5480 | `				runStart = p;` |
|     15 | 5481 | `				continue;` |
|      - | 5482 | `			}` |
|     51 | 5483 | `			if( bAll && HtmlDocHasNamedTable(iDoc) ){` |
|     41 | 5484 | `				zEnt = FvHtml401Lookup(cp);` |
|     20 | 5485 | `			}` |
|     51 | 5486 | `			if( zEnt == 0 && (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode(cp,iFlags) ){` |
|    ! 0 | 5487 | `				zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 5488 | `			}` |
|      - | 5489 | `		}` |
|    357 | 5490 | `		if( zEnt ){` |
|    135 | 5491 | `			if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|    135 | 5492 | `			ph7_result_string(pCtx,zEnt,-1);` |
|    135 | 5493 | `			runStart = p + len;` |
|     67 | 5494 | `		}` |
|    357 | 5495 | `		p += len;` |
|      1 | 5496 | `	}` |
|     85 | 5497 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     48 | 5498 | `}` |
|      - | 5499 | `/* Shared decoder for html_entity_decode (bFull=1) and htmlspecialchars_decode` |
|      - | 5500 | ` * (bFull=0). Quote refs (cp 34/39, named or numeric) are gated by the quote` |
|      - | 5501 | ` * bits and left verbatim when suppressed; an invalid entity leaves its '&'` |
|      - | 5502 | ` * verbatim and rescans right after it, which also yields PHP's no-double-` |
|      - | 5503 | ` * decode behavior ("&amp;lt;" -> "&lt;"). */` |
|     82 | 5504 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 5505 | `                         int iFlags,int bFull){` |
|     83 | 5506 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     83 | 5507 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|     83 | 5508 | `	const unsigned char *runStart = p;` |
|     83 | 5509 | `	ph7_result_string(pCtx,"",0);` |
|    557 | 5510 | `	while( p < zEnd ){` |
|      - | 5511 | `		sxu32 cp;` |
|      - | 5512 | `		int nEat;` |
|    510 | 5513 | `		if( *p != '&' ){ p++; continue; }` |
|    155 | 5514 | `		if( !HtmlParseEntity(p,zEnd,iFlags,bFull,0,&cp,&nEat) ){ p++; continue; }` |
|    124 | 5515 | `		if( (cp == 34 && (iFlags & PH7_ENT_QUOTE_DOUBLE) == 0)` |
|    117 | 5516 | `		 \|\| (cp == 39 && (iFlags & PH7_ENT_QUOTE_SINGLE) == 0) ){` |
|      - | 5517 | `			/* Suppressed quote: leave the entity source verbatim. */` |
|     37 | 5518 | `			p += nEat;` |
|     37 | 5519 | `			continue;` |
|      - | 5520 | `		}` |
|     89 | 5521 | `		if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|      - | 5522 | `		{` |
|      - | 5523 | `			char zBuf[4];` |
|     89 | 5524 | `			int n = HtmlCpUtf8(cp,zBuf);` |
|     89 | 5525 | `			ph7_result_string(pCtx,zBuf,n);` |
|      - | 5526 | `		}` |
|     89 | 5527 | `		p += nEat;` |
|     89 | 5528 | `		runStart = p;` |
|      1 | 5529 | `	}` |
|     79 | 5530 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     79 | 5531 | `}` |
|      - | 5532 | `/* Validate the optional charset argument at apArg[idx]: UTF-8 aliases (and` |
|      - | 5533 | ` * ""/NULL meaning the default) are accepted; anything else — including` |
|      - | 5534 | ` * php-supported single-byte charsets like ISO-8859-1, PHL is UTF-8-only per` |
|      - | 5535 | ` * PLAN.md §6 — raises PHP's unsupported-charset warning and is treated as` |
|      - | 5536 | ` * UTF-8 (ph7_context_throw_error_format prepends the function name). */` |
|    141 | 5537 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx){` |
|      - | 5538 | `	const char *zCs;` |
|      - | 5539 | `	int nCs;` |
|    148 | 5540 | `	if( nArg <= idx \|\| ph7_value_is_null(apArg[idx]) ){ return; }` |
|     15 | 5541 | `	zCs = ph7_value_to_string(apArg[idx],&nCs);` |
|     15 | 5542 | `	if( nCs == 0 ){ return; } /* "" selects the default charset (UTF-8) */` |
|     13 | 5543 | `	if( nCs == 5 && SyStrnicmp(zCs,"UTF-8",5) == 0 ){` |
|     13 | 5544 | `		return; /* php accepts only "UTF-8" (any case) silently — "UTF8" warns */` |
|      - | 5545 | `	}` |
|    ! 0 | 5546 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5547 | `		"Charset \"%.*s\" is not supported, assuming UTF-8",nCs,zCs);` |
|     71 | 5548 | `}` |
|      - | 5549 | `/* get_html_translation_table() worker: character (UTF-8 bytes) => entity.` |
|      - | 5550 | ` * The five specials come first in byte order, then — for HTML_ENTITIES with a` |
|      - | 5551 | ` * named-table doctype — the 248 aHtml401Ent rows ascending (oracle-pinned` |
|      - | 5552 | ` * ordering; 253 entries under the defaults). */` |
|    549 | 5553 | `static void HtmlTableAdd(ph7_value *pArray,ph7_value *pValue,const char *zKey,const char *zEnt){` |
|    549 | 5554 | `	ph7_value_string(pValue,zEnt,-1);` |
|    549 | 5555 | `	ph7_array_add_strkey_elem(pArray,zKey,pValue);` |
|    549 | 5556 | `	ph7_value_reset_string_cursor(pValue);` |
|    549 | 5557 | `}` |
|     13 | 5558 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags){` |
|      - | 5559 | `	ph7_value *pArray,*pValue;` |
|     13 | 5560 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 5561 | `	sxu32 n;` |
|     13 | 5562 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 5563 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 5564 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|    ! 0 | 5565 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5566 | `		return;` |
|      - | 5567 | `	}` |
|     13 | 5568 | `	if( iFlags & PH7_ENT_QUOTE_DOUBLE ){` |
|     11 | 5569 | `		HtmlTableAdd(pArray,pValue,"\"","&quot;");` |
|      5 | 5570 | `	}` |
|     13 | 5571 | `	HtmlTableAdd(pArray,pValue,"&","&amp;");` |
|     13 | 5572 | `	if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|      - | 5573 | `		/* The apostrophe row mirrors the function each table belongs to:` |
|      - | 5574 | `		 * SPECIALCHARS follows htmlspecialchars, ENTITIES follows` |
|      - | 5575 | `		 * htmlentities (oracle-pinned at flags 35). */` |
|      7 | 5576 | `		HtmlTableAdd(pArray,pValue,"'",HtmlAposEntity(iDoc,iTable != 0));` |
|      3 | 5577 | `	}` |
|     13 | 5578 | `	HtmlTableAdd(pArray,pValue,"<","&lt;");` |
|     13 | 5579 | `	HtmlTableAdd(pArray,pValue,">","&gt;");` |
|     13 | 5580 | `	if( iTable != 0 /*php: any non-HTML_SPECIALCHARS table => entities*/ && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 5581 | `		char zKey[8];` |
|    499 | 5582 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|    497 | 5583 | `			int nK = HtmlCpUtf8(aHtml401Ent[n].cp,zKey);` |
|    497 | 5584 | `			zKey[nK] = 0;` |
|    497 | 5585 | `			HtmlTableAdd(pArray,pValue,zKey,aHtml401Ent[n].zEnt);` |
|    249 | 5586 | `		}` |
|      1 | 5587 | `	}` |
|     13 | 5588 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 5589 | `}` |
|     25 | 5590 | `static int FvEmailAllowed(unsigned char c){` |
|     25 | 5591 | `	if( (c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9') ){ return 1; }` |
|     16 | 5592 | `	return c=='!'\|\|c=='#'\|\|c=='$'\|\|c=='%'\|\|c=='&'\|\|c=='\''\|\|c=='*'\|\|c=='+'` |
|     10 | 5593 | ``	    \|\| c=='-'\|\|c=='='\|\|c=='?'\|\|c=='^'\|\|c=='_'\|\|c=='`'\|\|c=='{'\|\|c=='\|'`` |
|     15 | 5594 | `	    \|\| c=='}'\|\|c=='~'\|\|c=='@'\|\|c=='.'\|\|c=='['\|\|c==']';` |
|     13 | 5595 | `}` |
|     23 | 5596 | `static int FvUrlAllowed(unsigned char c){` |
|     23 | 5597 | `	return c>=33 && c<=126; /* PHP keeps every printable ASCII byte except space */` |
|      1 | 5598 | `}` |
|      - | 5599 | `/* SANITIZE_EMAIL (isUrl=0) / SANITIZE_URL (isUrl=1): strip disallowed bytes. */` |
|      5 | 5600 | `static void FvSanitizeChars(ph7_context *pCtx,const char *z,int n,int isUrl){` |
|      5 | 5601 | `	int i, runStart = 0;` |
|      5 | 5602 | `	ph7_result_string(pCtx,"",0);` |
|     51 | 5603 | `	for( i=0; i<n; i++ ){` |
|     47 | 5604 | `		unsigned char c = (unsigned char)z[i];` |
|     47 | 5605 | `		if( !(isUrl ? FvUrlAllowed(c) : FvEmailAllowed(c)) ){` |
|     11 | 5606 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     11 | 5607 | `			runStart = i+1;` |
|      5 | 5608 | `		}` |
|     24 | 5609 | `	}` |
|      5 | 5610 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      5 | 5611 | `}` |
|      - | 5612 | `/*` |
|      - | 5613 | ` * Apply the selected filter to one already-resolved input value and write the` |
|      - | 5614 | ` * result into pCtx. Shared by filter_var() and filter_input(): the caller has` |
|      - | 5615 | ` * already parsed $filter/$flags/$options. On validation failure the 'default'` |
|      - | 5616 | ` * option (if any) is returned, else null when FILTER_NULL_ON_FAILURE is set,` |
|      - | 5617 | ` * else false. A validating filter that passes returns the (string) input` |
|      - | 5618 | ` * unchanged; a sanitizer writes its transformed output directly.` |
|      - | 5619 | ` */` |
|    316 | 5620 | `static int FvApplyFilter(ph7_context *pCtx,ph7_value *pInput,` |
|      - | 5621 | `                         int iFilter,int iFlags,ph7_value *pOpts,` |
|      - | 5622 | `                         ph7_value *pDefault)` |
|      3 | 5623 | `{` |
|    319 | 5624 | `	int bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|      - | 5625 | `	const char *zVal; int nVal;` |
|      - | 5626 | `	/* An array/object input fails every scalar filter. */` |
|    319 | 5627 | `	if( ph7_value_is_array(pInput) ){ goto fail; }` |
|    317 | 5628 | `	zVal = ph7_value_to_string(pInput,&nVal);` |
|    317 | 5629 | `	switch( iFilter ){` |
|     28 | 5630 | `	case FV_VALIDATE_INT: {` |
|      - | 5631 | `		ph7_int64 v;` |
|     58 | 5632 | `		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }` |
|     31 | 5633 | `		if( pOpts ){` |
|      7 | 5634 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|      7 | 5635 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|      7 | 5636 | `			if( pMin && v<ph7_value_to_int64(pMin) ){ goto fail; }` |
|      7 | 5637 | `			if( pMax && v>ph7_value_to_int64(pMax) ){ goto fail; }` |
|      2 | 5638 | `		}` |
|     29 | 5639 | `		ph7_result_int64(pCtx,v);` |
|     29 | 5640 | `		return PH7_OK;` |
|      - | 5641 | `	}` |
|     34 | 5642 | `	case FV_VALIDATE_FLOAT: {` |
|      - | 5643 | `		double d;` |
|     69 | 5644 | `		if( !FvValidateFloat(zVal,nVal,iFlags,&d) ){ goto fail; }` |
|     39 | 5645 | `		ph7_result_double(pCtx,d);` |
|     39 | 5646 | `		return PH7_OK;` |
|      - | 5647 | `	}` |
|     14 | 5648 | `	case FV_VALIDATE_BOOLEAN: {` |
|      - | 5649 | `		int b;` |
|     29 | 5650 | `		if( !FvValidateBool(zVal,nVal,&b) ){ goto fail; }` |
|     21 | 5651 | `		ph7_result_bool(pCtx,b);` |
|     21 | 5652 | `		return PH7_OK;` |
|      - | 5653 | `	}` |
|     25 | 5654 | `	case FV_VALIDATE_IP:     if( !FvValidateIp(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|     11 | 5655 | `	case FV_VALIDATE_MAC:    if( !FvValidateMac(zVal,nVal) ){ goto fail; }       goto pass;` |
|     28 | 5656 | `	case FV_VALIDATE_EMAIL:  if( !FvValidateEmail(zVal,nVal) ){ goto fail; }     goto pass;` |
|     11 | 5657 | `	case FV_VALIDATE_DOMAIN: if( !FvValidateDomain(zVal,nVal) ){ goto fail; }    goto pass;` |
|     15 | 5658 | `	case FV_VALIDATE_URL:    if( !FvValidateUrl(zVal,nVal) ){ goto fail; }       goto pass;` |
|      3 | 5659 | `	case FV_VALIDATE_REGEXP: {` |
|      - | 5660 | `#ifdef PH7_ENABLE_PCRE` |
|      8 | 5661 | `		ph7_value *pRe = pOpts ? ph7_array_fetch(pOpts,"regexp",(int)sizeof("regexp")-1) : 0;` |
|      8 | 5662 | `		const char *zRe; int nRe, matched = 0;` |
|      8 | 5663 | `		if( pRe==0 ){` |
|      3 | 5664 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5665 | `				"filter_var(): \"regexp\" option is missing");` |
|      - | 5666 | `		}` |
|      5 | 5667 | `		zRe = ph7_value_to_string(pRe,&nRe);` |
|      5 | 5668 | `		if( PH7_PcreMatchQuiet(pCtx,zRe,nRe,zVal,nVal,&matched)!=SXRET_OK \|\| !matched ){ goto fail; }` |
|      3 | 5669 | `		goto pass;` |
|      - | 5670 | `#else` |
|      - | 5671 | `		goto fail;` |
|      - | 5672 | `#endif` |
|      - | 5673 | `	}` |
|      3 | 5674 | `	case FV_SANITIZE_NUMBER_INT:   FvSanitizeNumber(pCtx,zVal,nVal,0,0);      return PH7_OK;` |
|      5 | 5675 | `	case FV_SANITIZE_NUMBER_FLOAT: FvSanitizeNumber(pCtx,zVal,nVal,1,iFlags); return PH7_OK;` |
|     13 | 5676 | `	case FV_SANITIZE_SPECIAL_CHARS:      FvSanitizeSpecial(pCtx,zVal,nVal,iFlags); return PH7_OK;` |
|     25 | 5677 | `	case FV_SANITIZE_FULL_SPECIAL_CHARS: FvSanitizeFull(pCtx,zVal,nVal,iFlags);    return PH7_OK;` |
|      3 | 5678 | `	case FV_SANITIZE_EMAIL: FvSanitizeChars(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 5679 | `	case FV_SANITIZE_URL:   FvSanitizeChars(pCtx,zVal,nVal,1); return PH7_OK;` |
|     13 | 5680 | `	case FV_DEFAULT:` |
|      - | 5681 | `		/* FILTER_UNSAFE_RAW / FILTER_DEFAULT: pass through unchanged unless a` |
|      - | 5682 | `		 * STRIP/ENCODE flag is set, in which case apply the string filter. */` |
|     28 | 5683 | `		if( iFlags & FV_FLAG_STRING_MASK ){` |
|     15 | 5684 | `			FvSanitizeString(pCtx,zVal,nVal,iFlags);` |
|     15 | 5685 | `			return PH7_OK;` |
|      - | 5686 | `		}` |
|     14 | 5687 | `		goto pass;` |
|    ! 0 | 5688 | `	default:` |
|    ! 0 | 5689 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5690 | `			"Unknown filter with ID %d",iFilter);` |
|    ! 0 | 5691 | `		break; /* unknown filter id -> fail */` |
|    ! 0 | 5692 | `	}` |
|     58 | 5693 | `fail:` |
|    118 | 5694 | `	if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|    114 | 5695 | `	else if( bNull ){ ph7_result_null(pCtx); }` |
|    108 | 5696 | `	else { ph7_result_bool(pCtx,0); }` |
|    118 | 5697 | `	return PH7_OK;` |
|     26 | 5698 | `pass: /* validation passed: return the (string) input unchanged */` |
|     54 | 5699 | `	ph7_result_string(pCtx,zVal,nVal);` |
|     54 | 5700 | `	return PH7_OK;` |
|    161 | 5701 | `}` |
|      - | 5702 | `/*` |
|      - | 5703 | ` * Parse the ($filter, $options) pair shared by filter_var()/filter_input() out` |
|      - | 5704 | ` * of apArg[iBase] ($filter) and apArg[iBase+1] ($options): $options is either a` |
|      - | 5705 | ` * plain flags int, or an array with 'flags' and an 'options' sub-array (whose` |
|      - | 5706 | ` * 'default' entry is the fallback value). Fills the four output pointers;` |
|      - | 5707 | ` * unset outputs keep the caller-provided defaults.` |
|      - | 5708 | ` */` |
|    328 | 5709 | `static void FvParseFilterArgs(int nArg,ph7_value **apArg,int iBase,` |
|      - | 5710 | `                              int *piFilter,int *piFlags,` |
|      - | 5711 | `                              ph7_value **ppOpts,ph7_value **ppDefault)` |
|      3 | 5712 | `{` |
|    331 | 5713 | `	if( nArg>iBase ){ *piFilter = ph7_value_to_int(apArg[iBase]); }` |
|    331 | 5714 | `	if( nArg>iBase+1 ){` |
|     88 | 5715 | `		if( ph7_value_is_array(apArg[iBase+1]) ){` |
|     42 | 5716 | `			ph7_value *pF = ph7_array_fetch(apArg[iBase+1],"flags",(int)sizeof("flags")-1);` |
|     42 | 5717 | `			if( pF ){ *piFlags = ph7_value_to_int(pF); }` |
|     42 | 5718 | `			*ppOpts = ph7_array_fetch(apArg[iBase+1],"options",(int)sizeof("options")-1);` |
|     42 | 5719 | `			if( *ppOpts && !ph7_value_is_array(*ppOpts) ){ *ppOpts = 0; }` |
|     42 | 5720 | `			if( *ppOpts ){ *ppDefault = ph7_array_fetch(*ppOpts,"default",(int)sizeof("default")-1); }` |
|     22 | 5721 | `		}else{` |
|     48 | 5722 | `			*piFlags = ph7_value_to_int(apArg[iBase+1]);` |
|      - | 5723 | `		}` |
|     43 | 5724 | `	}` |
|    331 | 5725 | `}` |
|      - | 5726 | `/*` |
|      - | 5727 | ` * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 5728 | ` *  Validate or sanitize a value; see FvApplyFilter for the failure semantics.` |
|      - | 5729 | ` */` |
|    306 | 5730 | `static int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5731 | `{` |
|    308 | 5732 | `	int iFilter = FV_DEFAULT, iFlags = 0;` |
|    308 | 5733 | `	ph7_value *pOpts = 0, *pDefault = 0;` |
|    308 | 5734 | `	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    308 | 5735 | `	FvParseFilterArgs(nArg,apArg,1,&iFilter,&iFlags,&pOpts,&pDefault);` |
|    308 | 5736 | `	return FvApplyFilter(pCtx,apArg[0],iFilter,iFlags,pOpts,pDefault);` |
|    155 | 5737 | `}` |
|      - | 5738 | `/*` |
|      - | 5739 | ` * filter_input($type, $var_name, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 5740 | ` *  Look up $var_name in the requested INPUT_* superglobal, then apply the` |
|      - | 5741 | ` *  filter. Semantics verified byte-for-byte against php 8.5:` |
|      - | 5742 | ` *   - variable NOT set: 'default' option wins, else false when` |
|      - | 5743 | ` *     FILTER_NULL_ON_FAILURE is set, else null. (Note the null/false roles are` |
|      - | 5744 | ` *     INVERTED relative to a present value that fails validation, which yields` |
|      - | 5745 | ` *     default > null-if-NULL_ON_FAILURE > false via FvApplyFilter.)` |
|      - | 5746 | ` *   - variable present: delegate to FvApplyFilter.` |
|      - | 5747 | ` *  Divergence: php reads a SAPI snapshot of the original request variables` |
|      - | 5748 | ` *  captured at startup; PHL reads the live superglobal. In CLI they match for` |
|      - | 5749 | ` *  the SAPI-registered keys (SCRIPT_NAME/PHP_SELF/DOCUMENT_ROOT); keys added` |
|      - | 5750 | ` *  only to the live $_SERVER (REQUEST_TIME/PWD/…) are visible here but not in` |
|      - | 5751 | ` *  php's snapshot.` |
|      - | 5752 | ` */` |
|     28 | 5753 | `static int PH7_builtin_filter_input(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5754 | `{` |
|     30 | 5755 | `	int iType, iFilter = FV_DEFAULT, iFlags = 0;` |
|     30 | 5756 | `	ph7_value *pOpts = 0, *pDefault = 0, *pSuper, *pElem;` |
|      - | 5757 | `	const char *zVar, *zSuper; int nVar; sxu32 nSuper;` |
|     30 | 5758 | `	if( nArg<2 ){` |
|      7 | 5759 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|      2 | 5760 | `			"filter_input() expects at least 2 arguments, %d given",nArg);` |
|      - | 5761 | `	}` |
|     26 | 5762 | `	iType = ph7_value_to_int(apArg[0]);` |
|     26 | 5763 | `	switch( iType ){` |
|      3 | 5764 | `	case 0: zSuper = "_POST";   nSuper = (sxu32)sizeof("_POST")-1;   break; /* INPUT_POST */` |
|      3 | 5765 | `	case 1: zSuper = "_GET";    nSuper = (sxu32)sizeof("_GET")-1;    break; /* INPUT_GET */` |
|    ! 0 | 5766 | `	case 2: zSuper = "_COOKIE"; nSuper = (sxu32)sizeof("_COOKIE")-1; break; /* INPUT_COOKIE */` |
|    ! 0 | 5767 | `	case 4: zSuper = "_ENV";    nSuper = (sxu32)sizeof("_ENV")-1;    break; /* INPUT_ENV */` |
|     19 | 5768 | `	case 5: zSuper = "_SERVER"; nSuper = (sxu32)sizeof("_SERVER")-1; break; /* INPUT_SERVER */` |
|      1 | 5769 | `	default:` |
|      3 | 5770 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5771 | `			"filter_input(): Argument #1 ($type) must be an INPUT_* constant");` |
|      - | 5772 | `	}` |
|     23 | 5773 | `	zVar = ph7_value_to_string(apArg[1],&nVar);` |
|     23 | 5774 | `	FvParseFilterArgs(nArg,apArg,2,&iFilter,&iFlags,&pOpts,&pDefault);` |
|      - | 5775 | `	/* Resolve the variable from the superglobal (missing/non-array -> not set). */` |
|     23 | 5776 | `	pSuper = PH7_VmExtractSuper(pCtx->pVm,zSuper,nSuper);` |
|     23 | 5777 | `	pElem = (pSuper && ph7_value_is_array(pSuper))` |
|     33 | 5778 | `		? ph7_array_fetch(pSuper,zVar,nVar) : 0;` |
|     23 | 5779 | `	if( pElem==0 ){` |
|      - | 5780 | `		/* Variable not set: default > false(if NULL_ON_FAILURE) > null. Note the` |
|      - | 5781 | `		 * false/null roles are inverted vs FvApplyFilter's present-but-fails path. */` |
|     13 | 5782 | `		if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|      9 | 5783 | `		else if( iFlags & FV_NULL_ON_FAILURE ){ ph7_result_bool(pCtx,0); }` |
|      7 | 5784 | `		else { ph7_result_null(pCtx); }` |
|     13 | 5785 | `		return PH7_OK;` |
|      - | 5786 | `	}` |
|     11 | 5787 | `	return FvApplyFilter(pCtx,pElem,iFilter,iFlags,pOpts,pDefault);` |
|     16 | 5788 | `}` |
|      - | 5789 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 5790 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 5791 | `/*` |
|      - | 5792 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 5793 |  |
|      - | 5794 | ` */` |
|      4 | 5795 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 5796 | `	const char *zInput, /* Raw input */` |
|      - | 5797 | `	int nByte,  /* Input length */` |
|      - | 5798 | `	int delim,  /* Delimiter */` |
|      - | 5799 | `	int encl,   /* Enclosure */` |
|      - | 5800 | `	int escape,  /* Escape character */` |
|      - | 5801 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 5802 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 5803 | `	)` |
|      1 | 5804 | `{` |
|      5 | 5805 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 5806 | `	const char *zIn = zInput;` |
|      - | 5807 | `	const char *zPtr;` |
|      - | 5808 | `	int isEnc;` |
|      - | 5809 | `	/* Start processing */` |
|      8 | 5810 | `	for(;;){` |
|     17 | 5811 | `		if( zIn >= zEnd ){` |
|      - | 5812 | `			/* No more input to process */` |
|      5 | 5813 | `			break;` |
|      - | 5814 | `		}` |
|     13 | 5815 | `		isEnc = 0;` |
|     13 | 5816 | `		zPtr = zIn;` |
|      - | 5817 | `		/* Find the first delimiter */` |
|     27 | 5818 | `		while( zIn < zEnd ){` |
|     23 | 5819 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 5820 | `				/* Delimiter found,break imediately */` |
|      5 | 5821 | `				break;` |
|     15 | 5822 | `			}else if( zIn[0] == encl ){` |
|      - | 5823 | `				/* Inside enclosure? */` |
|    ! 0 | 5824 | `				isEnc = !isEnc;` |
|     15 | 5825 | `			}else if( zIn[0] == escape ){` |
|      - | 5826 | `				/* Escape sequence */` |
|    ! 0 | 5827 | `				zIn++;` |
|    ! 0 | 5828 | `			}` |
|      - | 5829 | `			/* Advance the cursor */` |
|     15 | 5830 | `			zIn++;` |
|      1 | 5831 | `		}` |
|     13 | 5832 | `		if( zIn > zPtr ){` |
|     13 | 5833 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 5834 | `			sxi32 rc;` |
|      - | 5835 | `			/* Invoke the supllied callback */` |
|     13 | 5836 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 5837 | `				zPtr++;` |
|    ! 0 | 5838 | `				nByteChunk-=2;` |
|    ! 0 | 5839 | `			}` |
|     13 | 5840 | `			if( nByteChunk > 0 ){` |
|     13 | 5841 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 5842 | `				if( rc == SXERR_ABORT ){` |
|      - | 5843 | `					/* User callback request an operation abort */` |
|    ! 0 | 5844 | `					break;` |
|      - | 5845 | `				}` |
|      6 | 5846 | `			}` |
|      6 | 5847 | `		}` |
|      - | 5848 | `		/* Ignore trailing delimiter */` |
|     21 | 5849 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 5850 | `			zIn++;` |
|      1 | 5851 | `		}` |
|      1 | 5852 | `	}` |
|      5 | 5853 | `	return SXRET_OK;` |
|      1 | 5854 | `}` |
|      - | 5855 | `/*` |
|      - | 5856 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 5857 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 5858 | ` * argument to this callback.` |
|      - | 5859 | ` */` |
|     12 | 5860 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 5861 | `{` |
|     13 | 5862 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 5863 | `	ph7_value sEntry;` |
|      - | 5864 | `	SyString sToken;` |
|      - | 5865 | `	/* Insert the token in the given array */` |
|     13 | 5866 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 5867 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 5868 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 5869 | `	if( sToken.nByte < 1){` |
|    ! 0 | 5870 | `		return SXRET_OK;` |
|      - | 5871 | `	}` |
|     13 | 5872 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 5873 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 5874 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 5875 | `	return SXRET_OK;` |
|      7 | 5876 | `}` |
|      - | 5877 | `/*` |
|      - | 5878 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 5879 | ` *  Parse a CSV string into an array.` |
|      - | 5880 | ` * Parameters` |
|      - | 5881 | ` *  $input` |
|      - | 5882 | ` *   The string to parse.` |
|      - | 5883 | ` *  $delimiter` |
|      - | 5884 | ` *   Set the field delimiter (one character only).` |
|      - | 5885 | ` *  $enclosure` |
|      - | 5886 | ` *   Set the field enclosure character (one character only).` |
|      - | 5887 | ` *  $escape` |
|      - | 5888 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 5889 | ` * Return` |
|      - | 5890 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 5891 | ` */` |
|      2 | 5892 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5893 | `{` |
|      - | 5894 | `	const char *zInput,*zPtr;` |
|      - | 5895 | `	ph7_value *pArray;` |
|      3 | 5896 | `	int delim  = ',';   /* Delimiter */` |
|      3 | 5897 | `	int encl   = '"' ;  /* Enclosure */` |
|      3 | 5898 | `	int escape = '\\';  /* Escape character */` |
|      - | 5899 | `	int nLen;` |
|      3 | 5900 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5901 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 5902 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5903 | `		return PH7_OK;` |
|      - | 5904 | `	}` |
|      - | 5905 | `	/* Extract the raw input */` |
|      3 | 5906 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5907 | `	if( nArg > 1 ){` |
|      - | 5908 | `		int i;` |
|      3 | 5909 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 5910 | `			/* Extract the delimiter */` |
|      3 | 5911 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 5912 | `			if( i > 0 ){` |
|      3 | 5913 | `				delim = zPtr[0];` |
|      1 | 5914 | `			}` |
|      1 | 5915 | `		}` |
|      3 | 5916 | `		if( nArg > 2 ){` |
|      3 | 5917 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 5918 | `				/* Extract the enclosure */` |
|      3 | 5919 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 5920 | `				if( i > 0 ){` |
|      3 | 5921 | `					encl = zPtr[0];` |
|      1 | 5922 | `				}` |
|      1 | 5923 | `			}` |
|      3 | 5924 | `			if( nArg > 3 ){` |
|      3 | 5925 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 5926 | `					/* Extract the escape character */` |
|      3 | 5927 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 5928 | `					if( i > 0 ){` |
|      3 | 5929 | `						escape = zPtr[0];` |
|      1 | 5930 | `					}` |
|      1 | 5931 | `				}` |
|      1 | 5932 | `			}` |
|      1 | 5933 | `		}` |
|      1 | 5934 | `	}` |
|      - | 5935 | `	/* Create our array */` |
|      3 | 5936 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5937 | `	if( pArray == 0 ){` |
|      - | 5938 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|    ! 0 | 5939 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5940 | `	}` |
|      - | 5941 | `	/* Parse the raw input */` |
|      3 | 5942 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 5943 | `	/* Return the freshly created array */` |
|      3 | 5944 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5945 | `	return PH7_OK;` |
|      2 | 5946 | `}` |
|      - | 5947 | `/*` |
|      - | 5948 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 5949 | ` * container.` |
|      - | 5950 | ` * Refer to [strip_tags()].` |
|      - | 5951 | ` */` |
|     10 | 5952 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5953 | `{` |
|     11 | 5954 | `	const char *zEnd = &zTag[nByte];` |
|      - | 5955 | `	const char *zPtr;` |
|      - | 5956 | `	SyString sEntry;` |
|      - | 5957 | `	/* Strip tags */` |
|     10 | 5958 | `	for(;;){` |
|     45 | 5959 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 5960 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 5961 | `				zTag++;` |
|      1 | 5962 | `		}` |
|     21 | 5963 | `		if( zTag >= zEnd ){` |
|     11 | 5964 | `			break;` |
|      - | 5965 | `		}` |
|     11 | 5966 | `		zPtr = zTag;` |
|      - | 5967 | `		/* Delimit the tag */` |
|     25 | 5968 | `		while(zTag < zEnd ){` |
|     25 | 5969 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5970 | `				/* UTF-8 stream */` |
|      3 | 5971 | `				zTag++;` |
|      5 | 5972 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 5973 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 5974 | `				break;` |
|    ! 0 | 5975 | `			}else{` |
|     13 | 5976 | `				zTag++;` |
|      - | 5977 | `			}` |
|      1 | 5978 | `		}` |
|     11 | 5979 | `		if( zTag > zPtr ){` |
|      - | 5980 | `			/* Perform the insertion */` |
|     11 | 5981 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 5982 | `			SyStringFullTrim(&sEntry);` |
|     11 | 5983 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 5984 | `		}` |
|      - | 5985 | `		/* Jump the trailing '>' */` |
|     11 | 5986 | `		zTag++;` |
|      1 | 5987 | `	}` |
|     11 | 5988 | `	return SXRET_OK;` |
|      1 | 5989 | `}` |
|      - | 5990 | `/*` |
|      - | 5991 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 5992 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 5993 | ` * Refer to [strip_tags()].` |
|      - | 5994 | ` */` |
|     36 | 5995 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5996 | `{` |
|     37 | 5997 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 5998 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 5999 | `		SyString sTag;` |
|     85 | 6000 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 6001 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 6002 | `			zTag++;` |
|      1 | 6003 | `		}` |
|      - | 6004 | `		/* Delimit the tag */` |
|     25 | 6005 | `		zCur = zTag;` |
|     77 | 6006 | `		while(zTag < zEnd ){` |
|     77 | 6007 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 6008 | `				/* UTF-8 stream */` |
|      5 | 6009 | `				zTag++;` |
|      9 | 6010 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 6011 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 6012 | `				break;` |
|    ! 0 | 6013 | `			}else{` |
|     49 | 6014 | `				zTag++;` |
|      - | 6015 | `			}` |
|      1 | 6016 | `		}` |
|     25 | 6017 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 6018 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 6019 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 6020 | `		if( sTag.nByte > 0 ){` |
|      - | 6021 | `			SyString *aEntry,*pEntry;` |
|      - | 6022 | `			sxi32 rc;` |
|      - | 6023 | `			sxu32 n;` |
|      - | 6024 | `			/* Perform the lookup */` |
|     25 | 6025 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 6026 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 6027 | `				pEntry = &aEntry[n];` |
|      - | 6028 | `				/* Do the comparison */` |
|     25 | 6029 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 6030 | `				if( !rc ){` |
|     21 | 6031 | `					return SXRET_OK;` |
|      - | 6032 | `				}` |
|      3 | 6033 | `			}` |
|      2 | 6034 | `		}` |
|      2 | 6035 | `	}` |
|      - | 6036 | `	/* No such tag */` |
|     17 | 6037 | `	return SXERR_NOTFOUND;` |
|     19 | 6038 | `}` |
|      - | 6039 | `/*` |
|      - | 6040 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 6041 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 6042 | ` * Refer to [strip_tags()].` |
|      - | 6043 | ` */` |
|     16 | 6044 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 6045 | `{` |
|     17 | 6046 | `	const char *zEnd = &zIn[nByte];` |
|      - | 6047 | `	const char *zPtr,*zTag;` |
|      - | 6048 | `	SySet sSet;` |
|      - | 6049 | `	/* initialize the set of allowed tags */` |
|     17 | 6050 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 6051 | `	if( nTaglen > 0 ){` |
|      - | 6052 | `		/* Set of allowed tags */` |
|     11 | 6053 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 6054 | `	}` |
|      - | 6055 | `	/* Set the empty string */` |
|     17 | 6056 | `	ph7_result_string(pCtx,"",0);` |
|      - | 6057 | `	/* Start processing */` |
|     26 | 6058 | `	for(;;){` |
|     53 | 6059 | `		if(zIn >= zEnd){` |
|      - | 6060 | `			/* No more input to process */` |
|     15 | 6061 | `			break;` |
|      - | 6062 | `		}` |
|     39 | 6063 | `		zPtr = zIn;` |
|      - | 6064 | `		/* Find a tag */` |
|    133 | 6065 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 6066 | `			zIn++;` |
|      1 | 6067 | `		}` |
|     39 | 6068 | `		if( zIn > zPtr ){` |
|      - | 6069 | `			/* Consume raw input */` |
|     21 | 6070 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 6071 | `		}` |
|      - | 6072 | `		/* Ignore trailing null bytes */` |
|     39 | 6073 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 6074 | `			zIn++;` |
|    ! 0 | 6075 | `		}` |
|     39 | 6076 | `		if(zIn >= zEnd){` |
|      - | 6077 | `			/* No more input to process */` |
|      3 | 6078 | `			break;` |
|      - | 6079 | `		}` |
|     37 | 6080 | `		if( zIn[0] == '<' ){` |
|      - | 6081 | `			sxi32 rc;` |
|     37 | 6082 | `			zTag = zIn++;` |
|      - | 6083 | `			/* Delimit the tag */` |
|    127 | 6084 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 6085 | `				zIn++;` |
|      1 | 6086 | `			}` |
|     37 | 6087 | `			if( zIn < zEnd ){` |
|     37 | 6088 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 6089 | `			}` |
|      - | 6090 | `			/* Query the set */` |
|     37 | 6091 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 6092 | `			if( rc == SXRET_OK ){` |
|      - | 6093 | `				/* Keep the tag */` |
|     21 | 6094 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 6095 | `			}` |
|     18 | 6096 | `		}` |
|      1 | 6097 | `	}` |
|      - | 6098 | `	/* Cleanup */` |
|     17 | 6099 | `	SySetRelease(&sSet);` |
|     17 | 6100 | `	return SXRET_OK;` |
|      1 | 6101 | `}` |
|      - | 6102 | `/*` |
|      - | 6103 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 6104 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 6105 | ` * Parameters` |
|      - | 6106 | ` *  $str` |
|      - | 6107 | ` *  The input string.` |
|      - | 6108 | ` * $allowable_tags` |
|      - | 6109 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 6110 | ` * Return` |
|      - | 6111 | ` *  Returns the stripped string.` |
|      - | 6112 | ` */` |
|     14 | 6113 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6114 | `{` |
|     15 | 6115 | `	const char *zTaglist = 0;` |
|      - | 6116 | `	const char *zString;` |
|     15 | 6117 | `	int nTaglen = 0;` |
|      - | 6118 | `	int nLen;` |
|     15 | 6119 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6120 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 6121 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 6122 | `		return PH7_OK;` |
|      - | 6123 | `	}` |
|      - | 6124 | `	/* Point to the raw string */` |
|     15 | 6125 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 6126 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 6127 | `		/* Allowed tag */` |
|     11 | 6128 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 6129 | `	}` |
|      - | 6130 | `	/* Process input */` |
|     15 | 6131 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 6132 | `	return PH7_OK;` |
|      8 | 6133 | `}` |
|      - | 6134 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6135 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6136 | `/*` |
|      - | 6137 | ` * string str_shuffle(string $str)` |
|      - | 6138 |  |
|      - | 6139 | ` *  Randomly shuffles a string.` |
|      - | 6140 | ` * Parameters` |
|      - | 6141 | ` *  $str` |
|      - | 6142 | ` *   The input string.` |
|      - | 6143 | ` * Return` |
|      - | 6144 | ` *  Returns the shuffled string.` |
|      - | 6145 | ` */` |
|     10 | 6146 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6147 | `{` |
|      - | 6148 | `	const char *zString;` |
|      - | 6149 | `	int nLen,i,c;` |
|      - | 6150 | `	sxu32 iR;` |
|     11 | 6151 | `	if( nArg < 1 ){` |
|      - | 6152 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 6153 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 6154 | `		return PH7_OK;` |
|      - | 6155 | `	}` |
|      - | 6156 | `	/* Extract the target string */` |
|     11 | 6157 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 6158 | `	if( nLen < 1 ){` |
|      - | 6159 | `		/* Nothing to shuffle */` |
|      3 | 6160 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 6161 | `		return PH7_OK;` |
|      - | 6162 | `	}` |
|      - | 6163 | `	/* Shuffle the string */` |
|     43 | 6164 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 6165 | `		/* Generate a random number first */` |
|     35 | 6166 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 6167 | `		/* Extract a random offset */` |
|     35 | 6168 | `		c = zString[iR % nLen];` |
|      - | 6169 | `		/* Append it */` |
|     35 | 6170 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 6171 | `	}` |
|      9 | 6172 | `	return PH7_OK;` |
|      6 | 6173 | `}` |
|      - | 6174 | `/*` |
|      - | 6175 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 6176 | ` *  Convert a string to an array.` |
|      - | 6177 | ` * Parameters` |
|      - | 6178 | ` * $string` |
|      - | 6179 | ` *  The input string.` |
|      - | 6180 | ` * $split_length` |
|      - | 6181 | ` *  Maximum length of the chunk.` |
|      - | 6182 | ` * Return` |
|      - | 6183 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 6184 | ` *  except possibly the last one which may be shorter.` |
|      - | 6185 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 6186 | ` *  as the first (and only) array element.` |
|      - | 6187 | ` *  An empty string returns an empty array.` |
|      - | 6188 | ` * Errors` |
|      - | 6189 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 6190 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 6191 | ` *  ValueError if $split_length is less than 1.` |
|      - | 6192 | ` */` |
|     28 | 6193 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 6194 | `{` |
|      - | 6195 | `	const char *zString,*zEnd;` |
|      - | 6196 | `	ph7_value *pArray,*pValue;` |
|      - | 6197 | `	int split_len;` |
|      - | 6198 | `	int nLen;` |
|     33 | 6199 | `	if( nArg < 1 ){` |
|      4 | 6200 | `		return PH7_VmThrowException(pCtx,` |
|      - | 6201 | `			"ArgumentCountError",` |
|      - | 6202 | `			"str_split() expects at least 1 argument, %d given",` |
|      1 | 6203 | `			nArg` |
|      - | 6204 | `			);` |
|      - | 6205 | `	}` |
|      - | 6206 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     38 | 6207 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     40 | 6208 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 6209 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 6210 | `		return PH7_VmThrowException(pCtx,` |
|      - | 6211 | `			"TypeError",` |
|      - | 6212 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 6213 | `			ph7_type_name(apArg[0])` |
|      - | 6214 | `			);` |
|      - | 6215 | `	}` |
|      - | 6216 | `	/* Point to the target string */` |
|     27 | 6217 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 6218 | `	split_len = (int)sizeof(char);` |
|     27 | 6219 | `	if( nArg > 1 ){` |
|      - | 6220 | `		/* Split length */` |
|     17 | 6221 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 6222 | `		if( split_len < 1 ){` |
|      6 | 6223 | `			return PH7_VmThrowException(pCtx,` |
|      - | 6224 | `				"ValueError",` |
|      - | 6225 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 6226 | `				);` |
|      - | 6227 | `		}` |
|     11 | 6228 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 6229 | `			split_len = nLen;` |
|      1 | 6230 | `		}` |
|      5 | 6231 | `	}` |
|      - | 6232 | `	/* Create the array and the scalar value */` |
|     21 | 6233 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 6234 | `	/*Chunk value */` |
|     21 | 6235 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 6236 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 6237 | `		/* Return FALSE */` |
|    ! 0 | 6238 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6239 | `		return PH7_OK;` |
|      - | 6240 | `	}` |
|      - | 6241 | `	/* Point to the end of the string */` |
|     21 | 6242 | `	zEnd = &zString[nLen];` |
|      - | 6243 | `	/* Perform the requested operation */` |
|     48 | 6244 | `	for(;;){` |
|      - | 6245 | `		int nMax;` |
|     59 | 6246 | `		if( zString >= zEnd ){` |
|      - | 6247 | `			/* No more input to process */` |
|     21 | 6248 | `			break;` |
|      - | 6249 | `		}` |
|     39 | 6250 | `		nMax = (int)(zEnd-zString);` |
|     39 | 6251 | `		if( nMax < split_len ){` |
|      3 | 6252 | `			split_len = nMax;` |
|      1 | 6253 | `		}` |
|      - | 6254 | `		/* Copy the current chunk */` |
|     39 | 6255 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 6256 | `		/* Insert it */` |
|     39 | 6257 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|    ! 0 | 6258 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6259 | `		}` |
|      - | 6260 | `		/* reset the string cursor */` |
|     39 | 6261 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 6262 | `		/* Update position */` |
|     39 | 6263 | `		zString += split_len;` |
|      1 | 6264 | `	}` |
|      - | 6265 | `	/*` |
|      - | 6266 | `	 * Return the array.` |
|      - | 6267 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 6268 | `	 * upon we return from this function.` |
|      - | 6269 | `	 */` |
|     21 | 6270 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 6271 | `	return PH7_OK;` |
|     19 | 6272 | `}` |
|      - | 6273 | `/*` |
|      - | 6274 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 6275 | ` * Refer to [strspn()].` |
|      - | 6276 | ` */` |
|     28 | 6277 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 6278 | `{` |
|     29 | 6279 | `	const char *zIn = *pzIn;` |
|      - | 6280 | `	const char *zPtr;` |
|      - | 6281 | `	/* Ignore leading white spaces */` |
|     29 | 6282 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 6283 | `		zIn++;` |
|    ! 0 | 6284 | `	}` |
|     29 | 6285 | `	if( zIn >= zEnd ){` |
|      - | 6286 | `		/* End of input */` |
|    ! 0 | 6287 | `		return SXERR_EOF;` |
|      - | 6288 | `	}` |
|     29 | 6289 | `	zPtr = zIn;` |
|      - | 6290 | `	/* Extract the token */` |
|    201 | 6291 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 6292 | `		zIn++;` |
|      1 | 6293 | `	}` |
|     29 | 6294 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 6295 | `	/* Synchronize pointers */` |
|     29 | 6296 | `	*pzIn = zIn;` |
|      - | 6297 | `	/* Return to the caller */` |
|     29 | 6298 | `	return SXRET_OK;` |
|     15 | 6299 | `}` |
|      - | 6300 | `/*` |
|      - | 6301 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 6302 | ` * return the longest match.` |
|      - | 6303 | ` * Refer to [strspn()].` |
|      - | 6304 | ` */` |
|     18 | 6305 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 6306 | `{` |
|     19 | 6307 | `	const char *zEnd = &zString[nLen];` |
|     19 | 6308 | `	const char *zIn = zString;` |
|      - | 6309 | `	int i,c;` |
|     45 | 6310 | `	for(;;){` |
|     91 | 6311 | `		if( zString >= zEnd ){` |
|      7 | 6312 | `			break;` |
|      - | 6313 | `		}` |
|      - | 6314 | `		/* Extract current character */` |
|     85 | 6315 | `		c = zString[0];` |
|      - | 6316 | `		/* Perform the lookup */` |
|    383 | 6317 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 6318 | `			if( c == zMask[i] ){` |
|      - | 6319 | `				/* Character found */` |
|     73 | 6320 | `				break;` |
|      - | 6321 | `			}` |
|    150 | 6322 | `		}` |
|     85 | 6323 | `		if( i >= nMaskLen ){` |
|      - | 6324 | `			/* Character not in the current mask,break immediately */` |
|     13 | 6325 | `			break;` |
|      - | 6326 | `		}` |
|      - | 6327 | `		/* Advance cursor */` |
|     73 | 6328 | `		zString++;` |
|      1 | 6329 | `	}` |
|      - | 6330 | `	/* Longest match */` |
|     19 | 6331 | `	return (int)(zString-zIn);` |
|      1 | 6332 | `}` |
|      - | 6333 | `/*` |
|      - | 6334 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 6335 | ` * Refer to [strcspn()].` |
|      - | 6336 | ` */` |
|     10 | 6337 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 6338 | `{` |
|     11 | 6339 | `	const char *zEnd = &zString[nLen];` |
|     11 | 6340 | `	const char *zIn = zString;` |
|      - | 6341 | `	int i,c;` |
|     12 | 6342 | `	for(;;){` |
|     25 | 6343 | `		if( zString >= zEnd ){` |
|      3 | 6344 | `			break;` |
|      - | 6345 | `		}` |
|      - | 6346 | `		/* Extract current character */` |
|     23 | 6347 | `		c = zString[0];` |
|      - | 6348 | `		/* Perform the lookup */` |
|     51 | 6349 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 6350 | `			if( c == zMask[i] ){` |
|      9 | 6351 | `				break;` |
|      - | 6352 | `			}` |
|     15 | 6353 | `		}` |
|     23 | 6354 | `		if( i < nMaskLen ){` |
|      - | 6355 | `			/* Character in the current mask,break immediately */` |
|      9 | 6356 | `			break;` |
|      - | 6357 | `		}` |
|      - | 6358 | `		/* Advance cursor */` |
|     15 | 6359 | `		zString++;` |
|      1 | 6360 | `	}` |
|      - | 6361 | `	/* Longest match */` |
|     11 | 6362 | `	return (int)(zString-zIn);` |
|      1 | 6363 | `}` |
|      - | 6364 | `/*` |
|      - | 6365 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 6366 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 6367 | ` *  of characters contained within a given mask.` |
|      - | 6368 | ` * Parameters` |
|      - | 6369 | ` * $str` |
|      - | 6370 | ` *  The input string.` |
|      - | 6371 | ` * $mask` |
|      - | 6372 | ` *  The list of allowable characters.` |
|      - | 6373 | ` * $start` |
|      - | 6374 | ` *  The position in subject to start searching.` |
|      - | 6375 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 6376 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 6377 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 6378 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 6379 | ` *  start'th position from the end of subject.` |
|      - | 6380 | ` * $length` |
|      - | 6381 | ` *  The length of the segment from subject to examine.` |
|      - | 6382 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 6383 | ` *  characters after the starting position.` |
|      - | 6384 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 6385 | ` *  position up to length characters from the end of subject.` |
|      - | 6386 | ` * Return` |
|      - | 6387 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 6388 | ` * in mask.` |
|      - | 6389 | ` */` |
|     24 | 6390 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6391 | `{` |
|      - | 6392 | `	const char *zString,*zMask,*zEnd;` |
|      - | 6393 | `	int iMasklen,iLen;` |
|      - | 6394 | `	SyString sToken;` |
|     25 | 6395 | `	int iCount = 0;` |
|      - | 6396 | `	int rc;` |
|     25 | 6397 | `	if( nArg < 2 ){` |
|      - | 6398 | `		/* Missing agruments,return zero */` |
|    ! 0 | 6399 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 6400 | `		return PH7_OK;` |
|      - | 6401 | `	}` |
|      - | 6402 | `	/* Extract the target string */` |
|     25 | 6403 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6404 | `	/* Extract the mask */` |
|     25 | 6405 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 6406 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 6407 | `		/* Nothing to process,return zero */` |
|      7 | 6408 | `		ph7_result_int(pCtx,0);` |
|      7 | 6409 | `		return PH7_OK;` |
|      - | 6410 | `	}` |
|     19 | 6411 | `	if( nArg > 2 ){` |
|      - | 6412 | `		int nOfft;` |
|      - | 6413 | `		/* Extract the offset */` |
|      9 | 6414 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 6415 | `		if( nOfft < 0 ){` |
|    ! 0 | 6416 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 6417 | `			if( zBase > zString ){` |
|    ! 0 | 6418 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 6419 | `				zString = zBase;` |
|    ! 0 | 6420 | `			}else{` |
|      - | 6421 | `				/* Invalid offset */` |
|    ! 0 | 6422 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 6423 | `				return PH7_OK;` |
|      - | 6424 | `			}` |
|    ! 0 | 6425 | `		}else{` |
|      9 | 6426 | `			if( nOfft >= iLen ){` |
|      - | 6427 | `				/* Invalid offset */` |
|    ! 0 | 6428 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 6429 | `				return PH7_OK;` |
|    ! 0 | 6430 | `			}else{` |
|      - | 6431 | `				/* Update offset */` |
|      9 | 6432 | `				zString += nOfft;` |
|      9 | 6433 | `				iLen -= nOfft;` |
|      - | 6434 | `			}` |
|      - | 6435 | `		}` |
|      9 | 6436 | `		if( nArg > 3 ){` |
|      - | 6437 | `			int iUserlen;` |
|      - | 6438 | `			/* Extract the desired length */` |
|      9 | 6439 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 6440 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 6441 | `				iLen = iUserlen;` |
|      2 | 6442 | `			}` |
|      4 | 6443 | `		}` |
|      4 | 6444 | `	}` |
|      - | 6445 | `	/* Point to the end of the string */` |
|     19 | 6446 | `	zEnd = &zString[iLen];` |
|      - | 6447 | `	/* Extract the first non-space token */` |
|     19 | 6448 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 6449 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 6450 | `		/* Compare against the current mask */` |
|     19 | 6451 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 6452 | `	}` |
|      - | 6453 | `	/* Longest match */` |
|     19 | 6454 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 6455 | `	return PH7_OK;` |
|     13 | 6456 | `}` |
|      - | 6457 | `/*` |
|      - | 6458 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 6459 | ` *  Find length of initial segment not matching mask.` |
|      - | 6460 | ` * Parameters` |
|      - | 6461 | ` * $str` |
|      - | 6462 | ` *  The input string.` |
|      - | 6463 | ` * $mask` |
|      - | 6464 | ` *  The list of not allowed characters.` |
|      - | 6465 | ` * $start` |
|      - | 6466 | ` *  The position in subject to start searching.` |
|      - | 6467 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 6468 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 6469 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 6470 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 6471 | ` *  start'th position from the end of subject.` |
|      - | 6472 | ` * $length` |
|      - | 6473 | ` *  The length of the segment from subject to examine.` |
|      - | 6474 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 6475 | ` *  characters after the starting position.` |
|      - | 6476 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 6477 | ` *  position up to length characters from the end of subject.` |
|      - | 6478 | ` * Return` |
|      - | 6479 | ` *  Returns the length of the segment as an integer.` |
|      - | 6480 | ` */` |
|     14 | 6481 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6482 | `{` |
|      - | 6483 | `	const char *zString,*zMask,*zEnd;` |
|      - | 6484 | `	int iMasklen,iLen;` |
|      - | 6485 | `	SyString sToken;` |
|     15 | 6486 | `	int iCount = 0;` |
|      - | 6487 | `	int rc;` |
|     15 | 6488 | `	if( nArg < 2 ){` |
|      - | 6489 | `		/* Missing agruments,return zero */` |
|    ! 0 | 6490 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 6491 | `		return PH7_OK;` |
|      - | 6492 | `	}` |
|      - | 6493 | `	/* Extract the target string */` |
|     15 | 6494 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6495 | `	/* Extract the mask */` |
|     15 | 6496 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 6497 | `	if( iLen < 1 ){` |
|      - | 6498 | `		/* Nothing to process,return zero */` |
|    ! 0 | 6499 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 6500 | `		return PH7_OK;` |
|      - | 6501 | `	}` |
|     15 | 6502 | `	if( iMasklen < 1 ){` |
|      - | 6503 | `		/* No given mask,return the string length */` |
|      3 | 6504 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 6505 | `		return PH7_OK;` |
|      - | 6506 | `	}` |
|     13 | 6507 | `	if( nArg > 2 ){` |
|      - | 6508 | `		int nOfft;` |
|      - | 6509 | `		/* Extract the offset */` |
|     11 | 6510 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 6511 | `		if( nOfft < 0 ){` |
|    ! 0 | 6512 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 6513 | `			if( zBase > zString ){` |
|    ! 0 | 6514 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 6515 | `				zString = zBase;` |
|    ! 0 | 6516 | `			}else{` |
|      - | 6517 | `				/* Invalid offset */` |
|    ! 0 | 6518 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 6519 | `				return PH7_OK;` |
|      - | 6520 | `			}` |
|    ! 0 | 6521 | `		}else{` |
|     11 | 6522 | `			if( nOfft >= iLen ){` |
|      - | 6523 | `				/* Invalid offset */` |
|      3 | 6524 | `				ph7_result_int(pCtx,0);` |
|      3 | 6525 | `				return PH7_OK;` |
|    ! 0 | 6526 | `			}else{` |
|      - | 6527 | `				/* Update offset */` |
|      9 | 6528 | `				zString += nOfft;` |
|      9 | 6529 | `				iLen -= nOfft;` |
|      - | 6530 | `			}` |
|      - | 6531 | `		}` |
|      9 | 6532 | `		if( nArg > 3 ){` |
|      - | 6533 | `			int iUserlen;` |
|      - | 6534 | `			/* Extract the desired length */` |
|    ! 0 | 6535 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 6536 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 6537 | `				iLen = iUserlen;` |
|    ! 0 | 6538 | `			}` |
|    ! 0 | 6539 | `		}` |
|      4 | 6540 | `	}` |
|      - | 6541 | `	/* Point to the end of the string */` |
|     11 | 6542 | `	zEnd = &zString[iLen];` |
|      - | 6543 | `	/* Extract the first non-space token */` |
|     11 | 6544 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 6545 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 6546 | `		/* Compare against the current mask */` |
|     11 | 6547 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 6548 | `	}` |
|      - | 6549 | `	/* Longest match */` |
|     11 | 6550 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 6551 | `	return PH7_OK;` |
|      8 | 6552 | `}` |
|      - | 6553 | `/*` |
|      - | 6554 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 6555 | ` *  Search a string for any of a set of characters.` |
|      - | 6556 | ` * Parameters` |
|      - | 6557 | ` *  $haystack` |
|      - | 6558 | ` *   The string where char_list is looked for.` |
|      - | 6559 | ` *  $char_list` |
|      - | 6560 | ` *   This parameter is case sensitive.` |
|      - | 6561 | ` * Return` |
|      - | 6562 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 6563 | ` */` |
|      4 | 6564 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6565 | `{` |
|      - | 6566 | `	const char *zString,*zList,*zEnd;` |
|      - | 6567 | `	int iLen,iListLen,i,c;` |
|      - | 6568 | `	sxu32 nOfft,nMax;` |
|      - | 6569 | `	sxi32 rc;` |
|      5 | 6570 | `	if( nArg < 2 ){` |
|      - | 6571 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 6572 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6573 | `		return PH7_OK;` |
|      - | 6574 | `	}` |
|      - | 6575 | `	/* Extract the haystack and the char list */` |
|      5 | 6576 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 6577 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 6578 | `	if( iLen < 1 ){` |
|      - | 6579 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 6580 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6581 | `		return PH7_OK;` |
|      - | 6582 | `	}` |
|      - | 6583 | `	/* Point to the end of the string */` |
|      5 | 6584 | `	zEnd = &zString[iLen];` |
|      5 | 6585 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 6586 | `	/* perform the requested operation */` |
|     15 | 6587 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 6588 | `		c = zList[i];` |
|     11 | 6589 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 6590 | `		if( rc == SXRET_OK ){` |
|      5 | 6591 | `			if( nMax < nOfft ){` |
|      3 | 6592 | `				nOfft = nMax;` |
|      1 | 6593 | `			}` |
|      2 | 6594 | `		}` |
|      6 | 6595 | `	}` |
|      5 | 6596 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 6597 | `		/* No such substring,return FALSE */` |
|      3 | 6598 | `		ph7_result_bool(pCtx,0);` |
|      2 | 6599 | `	}else{` |
|      - | 6600 | `		/* Return the substring */` |
|      3 | 6601 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 6602 | `	}` |
|      5 | 6603 | `	return PH7_OK;` |
|      3 | 6604 | `}` |
|      - | 6605 | `/* SPDX-SnippetBegin */` |
|      - | 6606 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 6607 | `/* SPDX-License-Identifier: blessing */` |
|      - | 6608 | `/*` |
|      - | 6609 | ` * string soundex(string $str)` |
|      - | 6610 | ` *  Calculate the soundex key of a string.` |
|      - | 6611 | ` * Parameters` |
|      - | 6612 | ` *  $str` |
|      - | 6613 | ` *   The input string.` |
|      - | 6614 | ` * Return` |
|      - | 6615 | ` *  Returns the soundex key as a string.` |
|      - | 6616 | ` * Note:` |
|      - | 6617 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 6618 | ` * source tree.` |
|      - | 6619 | ` */` |
|     22 | 6620 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6621 | `{` |
|      - | 6622 | `	const unsigned char *zIn;` |
|      - | 6623 | `	char zResult[8];` |
|      - | 6624 | `	int i, j;` |
|      - | 6625 | `	static const unsigned char iCode[] = {` |
|      - | 6626 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 6627 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 6628 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 6629 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 6630 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 6631 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 6632 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 6633 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 6634 | `	};` |
|     23 | 6635 | `	if( nArg < 1 ){` |
|      - | 6636 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 6637 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 6638 | `		return PH7_OK;` |
|      - | 6639 | `	}` |
|     23 | 6640 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     35 | 6641 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     23 | 6642 | `	if( zIn[i] ){` |
|     17 | 6643 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 6644 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 6645 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 6646 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 6647 | `			if( code>0 ){` |
|     45 | 6648 | `				if( code!=prevcode ){` |
|     33 | 6649 | `					prevcode = (unsigned char)code;` |
|     33 | 6650 | `					zResult[j++] = (char)code + '0';` |
|     16 | 6651 | `				}` |
|     23 | 6652 | `			}else{` |
|     49 | 6653 | `				prevcode = 0;` |
|      - | 6654 | `			}` |
|     47 | 6655 | `		}` |
|     33 | 6656 | `		while( j<4 ){` |
|     17 | 6657 | `			zResult[j++] = '0';` |
|      1 | 6658 | `		}` |
|     17 | 6659 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 6660 | `	}else{` |
|      - | 6661 | `	  /* No alphabetic character: PHP returns "0000" (not the SQLite "?000"). */` |
|      7 | 6662 | `	  ph7_result_string(pCtx,"0000",4);` |
|      - | 6663 | `	}` |
|     23 | 6664 | `	return PH7_OK;` |
|     12 | 6665 | `}` |
|      - | 6666 | `/* SPDX-SnippetEnd */` |
|      - | 6667 | `/*` |
|      - | 6668 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 6669 | ` *  Wraps a string to a given number of characters.` |
|      - | 6670 | ` * Parameters` |
|      - | 6671 | ` *  $str` |
|      - | 6672 | ` *   The input string.` |
|      - | 6673 | ` * $width` |
|      - | 6674 | ` *  The column width.` |
|      - | 6675 | ` * $break` |
|      - | 6676 | ` *  The line is broken using the optional break parameter.` |
|      - | 6677 | ` * Return` |
|      - | 6678 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 6679 | ` */` |
|     26 | 6680 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6681 | `{` |
|      - | 6682 | `	const char *zIn,*zBreak;` |
|      - | 6683 | `	SyBlob sWorker;` |
|      - | 6684 | `	int iLen,iBreaklen,iWidth,iCut,iStart,iSpace,iCur;` |
|      - | 6685 | `	sxi32 rc;` |
|     27 | 6686 | `	if( nArg < 1 ){` |
|      - | 6687 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 6688 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 6689 | `		return PH7_OK;` |
|      - | 6690 | `	}` |
|      - | 6691 | `	/* Extract the input string */` |
|     27 | 6692 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6693 | `	/* Width (default 75; PHP allows 0/negative — break at every space). */` |
|     27 | 6694 | `	iWidth = 75;` |
|     27 | 6695 | `	if( nArg > 1 ){` |
|     27 | 6696 | `		iWidth = ph7_value_to_int(apArg[1]);` |
|     13 | 6697 | `	}` |
|      - | 6698 | `	/* Break string (default "\n"). */` |
|     27 | 6699 | `	zBreak = "\n";` |
|     27 | 6700 | `	iBreaklen = (int)sizeof(char);` |
|     27 | 6701 | `	if( nArg > 2 ){` |
|     13 | 6702 | `		zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      6 | 6703 | `	}` |
|      - | 6704 | `	/* Cut long words? (default false). */` |
|     27 | 6705 | `	iCut = 0;` |
|     27 | 6706 | `	if( nArg > 3 ){` |
|      7 | 6707 | `		iCut = ph7_value_to_bool(apArg[3]);` |
|      3 | 6708 | `	}` |
|     27 | 6709 | `	if( iLen < 1 ){` |
|      - | 6710 | `		/* PHP returns the empty string for empty input before validating the other args. */` |
|      5 | 6711 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 6712 | `		return PH7_OK;` |
|      - | 6713 | `	}` |
|      - | 6714 | `	/* PHP 8 domain errors (catchable ValueError). */` |
|     23 | 6715 | `	if( iBreaklen < 1 ){` |
|      3 | 6716 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6717 | `			"wordwrap(): Argument #3 ($break) must not be empty");` |
|      - | 6718 | `	}` |
|     21 | 6719 | `	if( iWidth == 0 && iCut ){` |
|      3 | 6720 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6721 | `			"wordwrap(): Argument #4 ($cut_long_words) cannot be true when argument #2 ($width) is 0");` |
|      - | 6722 | `	}` |
|      - | 6723 | `	/*` |
|      - | 6724 | `	 * PHP's algorithm: a single left-to-right pass tracking the start of the` |
|      - | 6725 | `	 * current line (iStart) and the position of the last space seen on it` |
|      - | 6726 | `	 * (iSpace). A break is emitted when the line reaches the width, at the last` |
|      - | 6727 | `	 * space if there was one, otherwise (only when cut is enabled) hard at the` |
|      - | 6728 | `	 * boundary. An existing break sequence in the input resets the line.` |
|      - | 6729 | `	 */` |
|     19 | 6730 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     19 | 6731 | `	iStart = iSpace = iCur = 0;` |
|     19 | 6732 | `	rc = SXRET_OK;` |
|    551 | 6733 | `	while( iCur < iLen ){` |
|    533 | 6734 | `		if( iBreaklen <= iLen - iCur && SyMemcmp(&zIn[iCur],zBreak,(sxu32)iBreaklen) == 0 ){` |
|      - | 6735 | `			/* Existing break sequence in the input: copy it verbatim and reset the line. */` |
|    ! 0 | 6736 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart + iBreaklen));` |
|    ! 0 | 6737 | `			if( rc != SXRET_OK ){ goto oom; }` |
|    ! 0 | 6738 | `			iCur += iBreaklen;` |
|    ! 0 | 6739 | `			iStart = iSpace = iCur;` |
|    ! 0 | 6740 | `			continue;` |
|    533 | 6741 | `		}else if( zIn[iCur] == ' ' ){` |
|     67 | 6742 | `			if( iCur - iStart >= iWidth ){` |
|      - | 6743 | `				/* The line already fills the width at this space: break here (the space is consumed). */` |
|     13 | 6744 | `				rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     13 | 6745 | `				if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     13 | 6746 | `				if( rc != SXRET_OK ){ goto oom; }` |
|     13 | 6747 | `				iStart = iCur + 1;` |
|      6 | 6748 | `			}` |
|     67 | 6749 | `			iSpace = iCur;` |
|    500 | 6750 | `		}else if( iCut && iCur - iStart >= iWidth && iStart >= iSpace ){` |
|      - | 6751 | `			/* A word longer than the width with no space to break at: hard-cut at the boundary. */` |
|      7 | 6752 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|      7 | 6753 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|      7 | 6754 | `			if( rc != SXRET_OK ){ goto oom; }` |
|      7 | 6755 | `			iStart = iSpace = iCur;` |
|    464 | 6756 | `		}else if( iCur - iStart >= iWidth && iStart < iSpace ){` |
|      - | 6757 | `			/* Past the width mid-word: wrap back to the last space (which is consumed). */` |
|     17 | 6758 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iSpace - iStart));` |
|     17 | 6759 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     17 | 6760 | `			if( rc != SXRET_OK ){ goto oom; }` |
|     17 | 6761 | `			iStart = iSpace = iSpace + 1;` |
|      8 | 6762 | `		}` |
|    533 | 6763 | `		iCur++;` |
|      1 | 6764 | `	}` |
|      - | 6765 | `	/* Emit the trailing chunk. */` |
|     19 | 6766 | `	if( iStart < iCur ){` |
|     19 | 6767 | `		rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     19 | 6768 | `		if( rc != SXRET_OK ){ goto oom; }` |
|      9 | 6769 | `	}` |
|     19 | 6770 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|     19 | 6771 | `	SyBlobRelease(&sWorker);` |
|     19 | 6772 | `	return PH7_OK;` |
|    ! 0 | 6773 | `oom:` |
|    ! 0 | 6774 | `	SyBlobRelease(&sWorker);` |
|    ! 0 | 6775 | `	return PH7_ContextMemoryError(pCtx);` |
|     14 | 6776 | `}` |
|      - | 6777 | `/*` |
|      - | 6778 | ` * Check if the given character is a member of the given mask.` |
|      - | 6779 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 6780 | ` * Refer to [strtok()].` |
|      - | 6781 | ` */` |
|     30 | 6782 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 6783 | `{` |
|      - | 6784 | `	int i;` |
|     57 | 6785 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 6786 | `		if( c == zMask[i] ){` |
|     13 | 6787 | `			if( pOfft ){` |
|      5 | 6788 | `				*pOfft = i;` |
|      2 | 6789 | `			}` |
|     13 | 6790 | `			return TRUE;` |
|      - | 6791 | `		}` |
|     14 | 6792 | `	}` |
|     19 | 6793 | `	return FALSE;` |
|     16 | 6794 | `}` |
|      - | 6795 | `/*` |
|      - | 6796 | ` * Extract a single token from the input stream.` |
|      - | 6797 | ` * Refer to [strtok()].` |
|      - | 6798 | ` */` |
|      6 | 6799 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 6800 | `{` |
|      7 | 6801 | `	const char *zIn = *pzIn;` |
|      - | 6802 | `	const char *zPtr;` |
|      - | 6803 | `	/* Ignore leading delimiter */` |
|     11 | 6804 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 6805 | `		zIn++;` |
|      1 | 6806 | `	}` |
|      7 | 6807 | `	if( zIn >= zEnd ){` |
|      - | 6808 | `		/* End of input */` |
|    ! 0 | 6809 | `		return SXERR_EOF;` |
|      - | 6810 | `	}` |
|      7 | 6811 | `	zPtr = zIn;` |
|      - | 6812 | `	/* Extract the token */` |
|     13 | 6813 | `	while( zIn < zEnd ){` |
|     11 | 6814 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 6815 | `			/* UTF-8 stream */` |
|    ! 0 | 6816 | `			zIn++;` |
|    ! 0 | 6817 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 6818 | `		}else{` |
|     11 | 6819 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 6820 | `				break;` |
|      - | 6821 | `			}` |
|      7 | 6822 | `			zIn++;` |
|      - | 6823 | `		}` |
|      1 | 6824 | `	}` |
|      7 | 6825 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 6826 | `	/* Update the cursor */` |
|      7 | 6827 | `	*pzIn = zIn;` |
|      - | 6828 | `	/* Return to the caller */` |
|      7 | 6829 | `	return SXRET_OK;` |
|      4 | 6830 | `}` |
|      - | 6831 | `/* strtok auxiliary private data */` |
|      - | 6832 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 6833 | `struct strtok_aux_data` |
|      - | 6834 | `{` |
|      - | 6835 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 6836 | `	const char *zIn;   /* Current input stream */` |
|      - | 6837 | `	const char *zEnd;  /* End of input */` |
|      - | 6838 | `};` |
|      - | 6839 | `/*` |
|      - | 6840 | ` * string strtok(string $str,string $token)` |
|      - | 6841 | ` * string strtok(string $token)` |
|      - | 6842 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 6843 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 6844 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 6845 | ` *  words by using the space character as the token.` |
|      - | 6846 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 6847 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 6848 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 6849 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 6850 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 6851 | ` *  the argument are found.` |
|      - | 6852 | ` * Parameters` |
|      - | 6853 | ` *  $str` |
|      - | 6854 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 6855 | ` * $token` |
|      - | 6856 | ` *  The delimiter used when splitting up str.` |
|      - | 6857 | ` * Return` |
|      - | 6858 | ` *   Current token or FALSE on EOF.` |
|      - | 6859 | ` */` |
|      6 | 6860 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6861 | `{` |
|      - | 6862 | `	strtok_aux_data *pAux;` |
|      - | 6863 | `	const char *zMask;` |
|      - | 6864 | `	SyString sToken;` |
|      - | 6865 | `	int nMasklen;` |
|      - | 6866 | `	sxi32 rc;` |
|      7 | 6867 | `	if( nArg < 2 ){` |
|      - | 6868 | `		/* Extract top aux data */` |
|      5 | 6869 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      5 | 6870 | `		if( pAux == 0 ){` |
|      - | 6871 | `			/* No aux data,return FALSE */` |
|    ! 0 | 6872 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6873 | `			return PH7_OK;` |
|      - | 6874 | `		}` |
|      5 | 6875 | `		nMasklen = 0;` |
|      5 | 6876 | `		zMask = ""; /* cc warning */` |
|      5 | 6877 | `		if( nArg > 0 ){` |
|      - | 6878 | `			/* Extract the mask */` |
|      5 | 6879 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 6880 | `		}` |
|      5 | 6881 | `		if( nMasklen < 1 ){` |
|      - | 6882 | `			/* Invalid mask,return FALSE */` |
|    ! 0 | 6883 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 6884 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6885 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 6886 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6887 | `			return PH7_OK;` |
|      - | 6888 | `		}` |
|      - | 6889 | `		/* Extract the token */` |
|      5 | 6890 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 6891 | `		if( rc != SXRET_OK ){` |
|      - | 6892 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 6893 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 6894 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6895 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 6896 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6897 | `		}else{` |
|      - | 6898 | `			/* Return the extracted token */` |
|      5 | 6899 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6900 | `		}` |
|      3 | 6901 | `	}else{` |
|      - | 6902 | `		const char *zInput,*zCur;` |
|      - | 6903 | `		char *zDup;` |
|      - | 6904 | `		int nLen;` |
|      - | 6905 | `		/* Extract the raw input */` |
|      3 | 6906 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 6907 | `		if( nLen < 1 ){` |
|      - | 6908 | `			/* Empty input,return FALSE */` |
|    ! 0 | 6909 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6910 | `			return PH7_OK;` |
|      - | 6911 | `		}` |
|      - | 6912 | `		/* Extract the mask */` |
|      3 | 6913 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 6914 | `		if( nMasklen < 1 ){` |
|      - | 6915 | `			/* Set a default mask */` |
|      - | 6916 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 6917 | `			zMask = TOK_MASK;` |
|    ! 0 | 6918 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 6919 | `#undef TOK_MASK` |
|    ! 0 | 6920 | `		}` |
|      - | 6921 | `		/* Extract a single token */` |
|      3 | 6922 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 6923 | `		if( rc != SXRET_OK ){` |
|      - | 6924 | `			/* Empty input */` |
|    ! 0 | 6925 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6926 | `			return PH7_OK;` |
|    ! 0 | 6927 | `		}else{` |
|      - | 6928 | `			/* Return the extracted token */` |
|      3 | 6929 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6930 | `		}` |
|      - | 6931 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 6932 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 6933 | `		if( pAux ){` |
|      3 | 6934 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 6935 | `			if( nLen < 1 ){` |
|    ! 0 | 6936 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6937 | `				return PH7_OK;` |
|      - | 6938 | `			}` |
|      - | 6939 | `			/* Duplicate input */` |
|      3 | 6940 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 6941 | `			if( zDup  ){` |
|      3 | 6942 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 6943 | `				/* Register the aux data */` |
|      3 | 6944 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 6945 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 6946 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 6947 | `			}` |
|      1 | 6948 | `		}` |
|      - | 6949 | `	}` |
|      7 | 6950 | `	return PH7_OK;` |
|      4 | 6951 | `}` |
|      - | 6952 | `/*` |
|      - | 6953 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 6954 | ` *  Pad a string to a certain length with another string` |
|      - | 6955 | ` * Parameters` |
|      - | 6956 | ` *  $input` |
|      - | 6957 | ` *   The input string.` |
|      - | 6958 | ` * $pad_length` |
|      - | 6959 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 6960 | ` *   string, no padding takes place.` |
|      - | 6961 | ` * $pad_string` |
|      - | 6962 | ` *   Note:` |
|      - | 6963 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 6964 | ` *    divided by the pad_string's length.` |
|      - | 6965 | ` * $pad_type` |
|      - | 6966 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 6967 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 6968 | ` * Return` |
|      - | 6969 | ` *  The padded string.` |
|      - | 6970 | ` */` |
|     10 | 6971 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6972 | `{` |
|      - | 6973 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 6974 | `	const char *zIn,*zPad;` |
|     11 | 6975 | `	if( nArg < 2 ){` |
|      - | 6976 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 6977 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 6978 | `		return PH7_OK;` |
|      - | 6979 | `	}` |
|      - | 6980 | `	/* Extract the target string */` |
|     11 | 6981 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6982 | `	/* Padding length */` |
|     11 | 6983 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|     11 | 6984 | `	if( iPadlen > 0 ){` |
|      9 | 6985 | `		iPadlen -= iLen;` |
|      4 | 6986 | `	}` |
|     11 | 6987 | `	if( iPadlen < 1  ){` |
|      - | 6988 | `		/* Return the string verbatim */` |
|      5 | 6989 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      5 | 6990 | `		return PH7_OK;` |
|      - | 6991 | `	}` |
|      7 | 6992 | `	zPad = " "; /* Whitespace padding */` |
|      7 | 6993 | `	iStrpad = (int)sizeof(char);` |
|      7 | 6994 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      7 | 6995 | `	if( nArg > 2 ){` |
|      - | 6996 | `		/* Padding string */` |
|      7 | 6997 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      7 | 6998 | `		if( iStrpad < 1 ){` |
|      - | 6999 | `			/* An empty pad string throws a catchable ValueError in PHP 8` |
|      - | 7000 | `			 * (only reached once padding is actually required). */` |
|      3 | 7001 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7002 | `				"str_pad(): Argument #3 ($pad_string) must not be empty");` |
|      - | 7003 | `		}` |
|      5 | 7004 | `		if( nArg > 3 ){` |
|      - | 7005 | `			/* Padd type */` |
|      5 | 7006 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 7007 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 7008 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 7009 | `			}` |
|      2 | 7010 | `		}` |
|      2 | 7011 | `	}` |
|      5 | 7012 | `	iDiv = 1;` |
|      5 | 7013 | `	if( iType == 2 ){` |
|    ! 0 | 7014 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 7015 | `	}` |
|      - | 7016 | `	/* Perform the requested operation */` |
|      5 | 7017 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 7018 | `		jPad = iStrpad;` |
|      5 | 7019 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 7020 | `			/* Padding */` |
|      5 | 7021 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 7022 | `				break;` |
|      - | 7023 | `			}` |
|      3 | 7024 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7025 | `		}` |
|      3 | 7026 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 7027 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 7028 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 7029 | `				if( jPad > iStrpad ){` |
|    ! 0 | 7030 | `					jPad = iStrpad;` |
|    ! 0 | 7031 | `				}` |
|      3 | 7032 | `				if( jPad < 1){` |
|    ! 0 | 7033 | `					break;` |
|      - | 7034 | `				}` |
|      3 | 7035 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 7036 | `			}` |
|      1 | 7037 | `		}` |
|      1 | 7038 | `	}` |
|      5 | 7039 | `	if( iLen > 0 ){` |
|      - | 7040 | `		/* Append the input string */` |
|      5 | 7041 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7042 | `	}` |
|      5 | 7043 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 7044 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 7045 | `			/* Padding */` |
|      5 | 7046 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 7047 | `				break;` |
|      - | 7048 | `			}` |
|      3 | 7049 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7050 | `		}` |
|      5 | 7051 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 7052 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 7053 | `			if( jPad > iStrpad ){` |
|    ! 0 | 7054 | `				jPad = iStrpad;` |
|    ! 0 | 7055 | `			}` |
|      3 | 7056 | `			if( jPad < 1){` |
|    ! 0 | 7057 | `				break;` |
|      - | 7058 | `			}` |
|      3 | 7059 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 7060 | `		}` |
|      1 | 7061 | `	}` |
|      5 | 7062 | `	return PH7_OK;` |
|      6 | 7063 | `}` |
|      - | 7064 | `/*` |
|      - | 7065 | ` * String replacement private data.` |
|      - | 7066 | ` */` |
|      - | 7067 | `typedef struct str_replace_data str_replace_data;` |
|      - | 7068 | `struct str_replace_data` |
|      - | 7069 | `{` |
|      - | 7070 | `	/* Used by the str_replace family to collect the search/replace arguments. */` |
|      - | 7071 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 7072 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 7073 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 7074 | `};` |
|      - | 7075 | `/*` |
|      - | 7076 | ` * Remove a substring.` |
|      - | 7077 | ` */` |
|      - | 7078 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 7079 | `	for(;;){\` |
|      - | 7080 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 7081 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 7082 | `		++OFFT;\` |
|      - | 7083 | `	}\` |
|      - | 7084 | `}` |
|      - | 7085 | `/*` |
|      - | 7086 | ` * Shift right and insert algorithm.` |
|      - | 7087 | ` */` |
|      - | 7088 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 7089 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 7090 | `		for(;;){\` |
|      - | 7091 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 7092 | `			if(INLEN < 1 ) { break; }\` |
|      - | 7093 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 7094 | `			--INLEN; \` |
|      - | 7095 | `		}\` |
|      - | 7096 | `		for(;;){\` |
|      - | 7097 | `				if(ELEN < 1) { break; }\` |
|      - | 7098 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 7099 | `				OFFT++;\` |
|      - | 7100 | `				ENTRY++;\` |
|      - | 7101 | `				--ELEN;\` |
|      - | 7102 | `		}\` |
|      - | 7103 | `}` |
|      - | 7104 | `/*` |
|      - | 7105 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 7106 | ` * replacement string [i.e: zReplace].` |
|      - | 7107 | ` */` |
|     46 | 7108 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 7109 | `{` |
|     47 | 7110 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 7111 | `	sxu32 n,m;` |
|     47 | 7112 | `	n = SyBlobLength(pWorker);` |
|     47 | 7113 | `	m = nOfft;` |
|      - | 7114 | `	/* Delete the old entry */` |
|   6573 | 7115 | `	STRDEL(zInput,n,m,nLen);` |
|     47 | 7116 | `	SyBlobLength(pWorker) -= nLen;` |
|     47 | 7117 | `	if( nReplen > 0 ){` |
|     41 | 7118 | `		sxi32 iRep = nReplen;` |
|      - | 7119 | `		sxi32 rc;` |
|      - | 7120 | `		/*` |
|      - | 7121 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 7122 | `		 * string.` |
|      - | 7123 | `		 */` |
|     41 | 7124 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     41 | 7125 | `		if( rc != SXRET_OK ){` |
|      - | 7126 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 7127 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 7128 | `			return rc;` |
|      - | 7129 | `		}` |
|      - | 7130 | `		/* Perform the insertion now */` |
|     41 | 7131 | `		zInput = (char *)SyBlobData(pWorker);` |
|     41 | 7132 | `		n = SyBlobLength(pWorker);` |
|   6357 | 7133 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     41 | 7134 | `		SyBlobLength(pWorker) += nReplen;` |
|     20 | 7135 | `	}` |
|     47 | 7136 | `	return SXRET_OK;` |
|     24 | 7137 | `}` |
|      - | 7138 | `/*` |
|      - | 7139 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 7140 | ` * to collect search/replace string.` |
|      - | 7141 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 7142 | ` */` |
|     26 | 7143 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 7144 | `{` |
|     27 | 7145 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 7146 | `	SyString sWorker;` |
|      - | 7147 | `	const char *zIn;` |
|      - | 7148 | `	int nByte;` |
|      - | 7149 | `	/* Extract a string representation of the given argument */` |
|     27 | 7150 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 7151 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 7152 | `	if( nByte > 0 ){` |
|      - | 7153 | `		char *zDup;` |
|      - | 7154 | `		/* Duplicate the chunk */` |
|     25 | 7155 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 7156 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 7157 | `			);` |
|     25 | 7158 | `		if( zDup == 0 ){` |
|      - | 7159 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 7160 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 7161 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 7162 | `			return SXERR_MEM;` |
|      - | 7163 | `		}` |
|     25 | 7164 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 7165 | `		/* Save the chunk */` |
|     25 | 7166 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 7167 | `	}` |
|      - | 7168 | `	/* Save for later processing */` |
|     27 | 7169 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 7170 | `	/* All done */` |
|     13 | 7171 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 7172 | `	return PH7_OK;` |
|     14 | 7173 | `}` |
|      - | 7174 | `/*` |
|      - | 7175 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 7176 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 7177 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 7178 | ` * Parameters` |
|      - | 7179 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 7180 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 7181 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 7182 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 7183 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 7184 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 7185 | ` * $search` |
|      - | 7186 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 7187 | ` *  to designate multiple needles.` |
|      - | 7188 | ` * $replace` |
|      - | 7189 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 7190 | ` *  to designate multiple replacements.` |
|      - | 7191 | ` * $subject` |
|      - | 7192 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 7193 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 7194 | ` *  of subject, and the return value is an array as well.` |
|      - | 7195 | ` * $count (Not used)` |
|      - | 7196 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 7197 | ` * Return` |
|      - | 7198 | ` * This function returns a string or an array with the replaced values.` |
|      - | 7199 | ` */` |
|  29176 | 7200 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7201 | `{` |
|      - | 7202 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 7203 | `	ProcStringMatch xMatch;` |
|      - | 7204 | `	const char *zIn,*zFunc;` |
|      - | 7205 | `	str_replace_data sRep;` |
|      - | 7206 | `	SyBlob sWorker;` |
|      - | 7207 | `	SySet sReplace;` |
|      - | 7208 | `	SySet sSearch;` |
|      - | 7209 | `	int rep_str;` |
|      - | 7210 | `	int nByte;` |
|      - | 7211 | `	sxi32 rc;` |
|  29181 | 7212 | `	if( nArg < 3 ){` |
|      - | 7213 | `		/* Missing/Invalid arguments,return null */` |
|    ! 0 | 7214 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7215 | `		return PH7_OK;` |
|      - | 7216 | `	}` |
|      - | 7217 | `	/* Initialize fields */` |
|  29181 | 7218 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  29181 | 7219 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  29181 | 7220 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  29181 | 7221 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  29181 | 7222 | `	sRep.pCtx = pCtx;` |
|  29181 | 7223 | `	sRep.pCollector = &sSearch;` |
|  29181 | 7224 | `	rep_str = 0;` |
|      - | 7225 | `	/* Extract the subject */` |
|  29181 | 7226 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  29181 | 7227 | `	if( nByte < 1 ){` |
|      - | 7228 | `		/* Nothing to replace,return the empty string */` |
|     29 | 7229 | `		ph7_result_string(pCtx,"",0);` |
|     29 | 7230 | `		return PH7_OK;` |
|      - | 7231 | `	}` |
|      - | 7232 | `	/* Copy the subject */` |
|  29153 | 7233 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 7234 | `	/* Search string */` |
|  29153 | 7235 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 7236 | `		/* Collect search string */` |
|      9 | 7237 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 7238 | `	}else{` |
|      - | 7239 | `		/* Single pattern */` |
|  29145 | 7240 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  29145 | 7241 | `		if( nByte < 1 ){` |
|      - | 7242 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 7243 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 7244 | `			return PH7_OK;` |
|      - | 7245 | `		}` |
|  29141 | 7246 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 7247 | `		/* Save for later processing */` |
|  29141 | 7248 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 7249 | `	}` |
|      - | 7250 | `	/* Replace string */` |
|  29149 | 7251 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 7252 | `		/* Collect replace string */` |
|      7 | 7253 | `		sRep.pCollector = &sReplace;` |
|      7 | 7254 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 7255 | `	}else{` |
|      - | 7256 | `		/* Single needle */` |
|  29143 | 7257 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  29143 | 7258 | `		rep_str = 1;` |
|  29143 | 7259 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 7260 | `		/* Save for later processing */` |
|  29143 | 7261 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 7262 | `	}` |
|      - | 7263 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  29149 | 7264 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 7265 | `		SySetRelease(&sSearch);` |
|    ! 0 | 7266 | `		SySetRelease(&sReplace);` |
|    ! 0 | 7267 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 7268 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7269 | `	}` |
|      - | 7270 | `	/* Reset loop cursors */` |
|  29149 | 7271 | `	SySetResetCursor(&sSearch);` |
|  29149 | 7272 | `	SySetResetCursor(&sReplace);` |
|  29149 | 7273 | `	pReplace = pSearch = 0; /* cc warning */` |
|  29149 | 7274 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 7275 | `	/* Extract function name */` |
|  29149 | 7276 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 7277 | `	/* Set the default pattern match routine */` |
|  29149 | 7278 | `	xMatch = SyBlobSearch;` |
|  29149 | 7279 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 7280 | `		/* Case insensitive pattern match */` |
|     11 | 7281 | `		xMatch = iPatternMatch;` |
|      5 | 7282 | `	}` |
|      - | 7283 | `	/* Start the replace process */` |
|  58301 | 7284 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 7285 | `		sxu32 nCount,nOfft;` |
|  29157 | 7286 | `		if( pSearch->nByte <  1 ){` |
|      - | 7287 | `			/* Empty string,ignore */` |
|      3 | 7288 | `			continue;` |
|      - | 7289 | `		}` |
|      - | 7290 | `		/* Extract the replace string */` |
|  29155 | 7291 | `		if( rep_str ){` |
|  29145 | 7292 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  14575 | 7293 | `		}else{` |
|     11 | 7294 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 7295 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 7296 | `				 * An empty string is used for the rest of replacement values` |
|      - | 7297 | `				 */` |
|      3 | 7298 | `				pReplace = 0;` |
|      1 | 7299 | `			}` |
|      - | 7300 | `		}` |
|  29155 | 7301 | `		if( pReplace == 0 ){` |
|      - | 7302 | `			/* Use an empty string instead */` |
|      3 | 7303 | `			pReplace = &sTemp;` |
|      1 | 7304 | `		}` |
|  29155 | 7305 | `		nOfft = nCount = 0;` |
|  14598 | 7306 | `		for(;;){` |
|  29201 | 7307 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 7308 | `				break;` |
|      - | 7309 | `			}` |
|      - | 7310 | `			/* Perform a pattern lookup */` |
|  43781 | 7311 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  29184 | 7312 | `				pSearch->nByte,&nOfft);` |
|  29189 | 7313 | `			if( rc != SXRET_OK ){` |
|      - | 7314 | `				/* Pattern not found */` |
|  29143 | 7315 | `				break;` |
|      - | 7316 | `			}` |
|      - | 7317 | `			/* Perform the replace operation */` |
|     47 | 7318 | `			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|     47 | 7319 | `			if( rc != SXRET_OK ){` |
|      - | 7320 | `				/* Allocation failure: surface a fatal instead of a partial result */` |
|    ! 0 | 7321 | `				SySetRelease(&sSearch);` |
|    ! 0 | 7322 | `				SySetRelease(&sReplace);` |
|    ! 0 | 7323 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 7324 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 7325 | `			}` |
|      - | 7326 | `			/* Increment offset counter */` |
|     47 | 7327 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 7328 | `		}` |
|      5 | 7329 | `	}` |
|      - | 7330 | `	/* All done,clean-up the mess left behind */` |
|  29149 | 7331 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  29149 | 7332 | `	SySetRelease(&sSearch);` |
|  29149 | 7333 | `	SySetRelease(&sReplace);` |
|  29149 | 7334 | `	SyBlobRelease(&sWorker);` |
|  29149 | 7335 | `	if( rc != PH7_OK ){` |
|    ! 0 | 7336 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7337 | `	}` |
|  29149 | 7338 | `	return PH7_OK;` |
|  14593 | 7339 | `}` |
|      - | 7340 | `/*` |
|      - | 7341 | ` * strtr() array form: a single (key,value) pair copied out of the replace_pairs` |
|      - | 7342 | ` * array. The bytes are owned by a persistent pool (see strtr_collect) rather than` |
|      - | 7343 | ` * the transient walker values, which HashmapWalk releases after each callback, so` |
|      - | 7344 | ` * we store byte offsets into that pool instead of raw pointers.` |
|      - | 7345 | ` */` |
|      - | 7346 | `typedef struct strtr_entry strtr_entry;` |
|      - | 7347 | `struct strtr_entry` |
|      - | 7348 | `{` |
|      - | 7349 | `	sxu32 nKeyOfft; /* Offset of the search key inside the pool */` |
|      - | 7350 | `	sxu32 nKeyLen;  /* Length of the search key */` |
|      - | 7351 | `	sxu32 nValOfft; /* Offset of the replacement inside the pool */` |
|      - | 7352 | `	sxu32 nValLen;  /* Length of the replacement */` |
|      - | 7353 | `};` |
|      - | 7354 | `typedef struct strtr_collect strtr_collect;` |
|      - | 7355 | `struct strtr_collect` |
|      - | 7356 | `{` |
|      - | 7357 | `	SyBlob *pPool;  /* Byte pool holding copied key + value bytes */` |
|      - | 7358 | `	SySet  *pTable; /* Set of strtr_entry (parallel offsets into pPool) */` |
|      - | 7359 | `	sxi32   rc;     /* Carries an allocation failure (SXERR_MEM) out of the walker */` |
|      - | 7360 | `};` |
|      - | 7361 | `/*` |
|      - | 7362 | ` * Collect one replace_pairs entry into the persistent pool/offset table.` |
|      - | 7363 | ` * PHP coerces both the key and the value to string (an integer key becomes its` |
|      - | 7364 | ` * decimal form) and ignores an empty-string key.` |
|      - | 7365 | ` */` |
|     20 | 7366 | `static int StrtrCollectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 7367 | `{` |
|     21 | 7368 | `	strtr_collect *pCol = (strtr_collect *)pUserData;` |
|      - | 7369 | `	const char *zKey,*zVal;` |
|      - | 7370 | `	strtr_entry sEnt;` |
|      - | 7371 | `	int nKey,nVal;` |
|     21 | 7372 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|     21 | 7373 | `	if( nKey < 1 ){` |
|      - | 7374 | `		/* PHP ignores an empty-string key (it also emits a warning we do not replicate). */` |
|    ! 0 | 7375 | `		return PH7_OK;` |
|      - | 7376 | `	}` |
|     21 | 7377 | `	zVal = ph7_value_to_string(pData,&nVal);` |
|     21 | 7378 | `	sEnt.nKeyOfft = SyBlobLength(pCol->pPool);` |
|     21 | 7379 | `	sEnt.nKeyLen  = (sxu32)nKey;` |
|     21 | 7380 | `	if( SyBlobAppend(pCol->pPool,(const void *)zKey,(sxu32)nKey) != SXRET_OK ){` |
|    ! 0 | 7381 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 7382 | `		return SXERR_ABORT;` |
|      - | 7383 | `	}` |
|     21 | 7384 | `	sEnt.nValOfft = SyBlobLength(pCol->pPool);` |
|     21 | 7385 | `	sEnt.nValLen  = (sxu32)nVal;` |
|     21 | 7386 | `	if( nVal > 0 && SyBlobAppend(pCol->pPool,(const void *)zVal,(sxu32)nVal) != SXRET_OK ){` |
|    ! 0 | 7387 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 7388 | `		return SXERR_ABORT;` |
|      - | 7389 | `	}` |
|     21 | 7390 | `	if( SySetPut(pCol->pTable,(const void *)&sEnt) != SXRET_OK ){` |
|    ! 0 | 7391 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 7392 | `		return SXERR_ABORT;` |
|      - | 7393 | `	}` |
|     21 | 7394 | `	return PH7_OK;` |
|     11 | 7395 | `}` |
|      - | 7396 | `/*` |
|      - | 7397 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 7398 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 7399 | ` *  Translate characters or replace substrings.` |
|      - | 7400 | ` * Parameters` |
|      - | 7401 | ` *  $str` |
|      - | 7402 | ` *  The string being translated.` |
|      - | 7403 | ` * $from` |
|      - | 7404 | ` *  The string being translated to to.` |
|      - | 7405 | ` * $to` |
|      - | 7406 | ` *  The string replacing from.` |
|      - | 7407 | ` * $replace_pairs` |
|      - | 7408 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 7409 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 7410 | ` * Return` |
|      - | 7411 | ` *  The translated string.` |
|      - | 7412 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 7413 | ` */` |
|     12 | 7414 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7415 | `{` |
|      - | 7416 | `	const char *zIn;` |
|      - | 7417 | `	int nLen;` |
|     13 | 7418 | `	if( nArg < 1 ){` |
|      - | 7419 | `		/* Nothing to replace,return FALSE */` |
|    ! 0 | 7420 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7421 | `		return PH7_OK;` |
|      - | 7422 | `	}` |
|     13 | 7423 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 7424 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 7425 | `		/* Invalid arguments */` |
|    ! 0 | 7426 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 7427 | `		return PH7_OK;` |
|      - | 7428 | `	}` |
|     18 | 7429 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 7430 | `		strtr_collect sCol;` |
|      - | 7431 | `		SyBlob sPool,sWorker;` |
|      - | 7432 | `		SySet sTable;` |
|      - | 7433 | `		const char *zPool;` |
|      - | 7434 | `		strtr_entry *pEnt;` |
|      - | 7435 | `		sxi32 rc;` |
|      - | 7436 | `		int i,iRun;` |
|      - | 7437 | `		/*` |
|      - | 7438 | `		 * PHP's array-form strtr is a single left-to-right pass over the subject:` |
|      - | 7439 | `		 * at every position it substitutes the LONGEST replace_pairs key that` |
|      - | 7440 | `		 * matches there, then advances past the key (replacements are never` |
|      - | 7441 | `		 * rescanned). It is not a sequential per-key global replace. First copy` |
|      - | 7442 | `		 * the pairs into a persistent pool, then run that scan.` |
|      - | 7443 | `		 */` |
|     11 | 7444 | `		SyBlobInit(&sPool,&pCtx->pVm->sAllocator);` |
|     11 | 7445 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     11 | 7446 | `		SySetInit(&sTable,&pCtx->pVm->sAllocator,sizeof(strtr_entry));` |
|     11 | 7447 | `		sCol.pPool  = &sPool;` |
|     11 | 7448 | `		sCol.pTable = &sTable;` |
|     11 | 7449 | `		sCol.rc     = SXRET_OK;` |
|     11 | 7450 | `		ph7_array_walk(apArg[1],StrtrCollectWalker,&sCol);` |
|     11 | 7451 | `		if( sCol.rc != SXRET_OK ){` |
|      - | 7452 | `			/* Allocation failure while collecting the pairs: surface a fatal */` |
|    ! 0 | 7453 | `			SyBlobRelease(&sPool);` |
|    ! 0 | 7454 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 7455 | `			SySetRelease(&sTable);` |
|    ! 0 | 7456 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 7457 | `		}` |
|      - | 7458 | `		/* The pool is now stable, so offsets can be resolved against its base. */` |
|     11 | 7459 | `		zPool = (const char *)SyBlobData(&sPool);` |
|     11 | 7460 | `		rc = SXRET_OK;` |
|     11 | 7461 | `		iRun = 0; /* Start of the pending run of unmatched bytes copied verbatim. */` |
|     43 | 7462 | `		for( i = 0 ; i < nLen ; ){` |
|     33 | 7463 | `			strtr_entry *pBest = 0;` |
|     33 | 7464 | `			sxu32 nBest = 0;` |
|      - | 7465 | `			/* Pick the longest key that matches at the current position. */` |
|     33 | 7466 | `			SySetResetCursor(&sTable);` |
|     97 | 7467 | `			while( SXRET_OK == SySetGetNextEntry(&sTable,(void **)&pEnt) ){` |
|     64 | 7468 | `				if( pEnt->nKeyLen > nBest` |
|     60 | 7469 | `					&& pEnt->nKeyLen <= (sxu32)(nLen - i)` |
|     56 | 7470 | `					&& SyMemcmp(zPool + pEnt->nKeyOfft,zIn + i,pEnt->nKeyLen) == 0 ){` |
|     29 | 7471 | `					nBest = pEnt->nKeyLen;` |
|     29 | 7472 | `					pBest = pEnt;` |
|     14 | 7473 | `				}` |
|      1 | 7474 | `			}` |
|     33 | 7475 | `			if( pBest == 0 ){` |
|      - | 7476 | `				/* No key here: extend the literal run and copy it in one shot later. */` |
|      9 | 7477 | `				i++;` |
|      9 | 7478 | `				continue;` |
|      - | 7479 | `			}` |
|      - | 7480 | `			/* Flush the pending literal run, then the replacement. */` |
|     25 | 7481 | `			if( i > iRun ){` |
|      5 | 7482 | `				rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(i - iRun));` |
|      2 | 7483 | `			}` |
|     25 | 7484 | `			if( rc == SXRET_OK && pBest->nValLen > 0 ){` |
|     25 | 7485 | `				rc = SyBlobAppend(&sWorker,zPool + pBest->nValOfft,pBest->nValLen);` |
|     12 | 7486 | `			}` |
|     25 | 7487 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 7488 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 7489 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 7490 | `				SySetRelease(&sTable);` |
|    ! 0 | 7491 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 7492 | `			}` |
|     25 | 7493 | `			i += (int)pBest->nKeyLen;` |
|     25 | 7494 | `			iRun = i;` |
|      1 | 7495 | `		}` |
|      - | 7496 | `		/* Flush the trailing literal run. */` |
|     11 | 7497 | `		if( nLen > iRun ){` |
|      3 | 7498 | `			rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(nLen - iRun));` |
|      3 | 7499 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 7500 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 7501 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 7502 | `				SySetRelease(&sTable);` |
|    ! 0 | 7503 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 7504 | `			}` |
|      1 | 7505 | `		}` |
|      - | 7506 | `		/* All done, return the result string */` |
|     16 | 7507 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|     10 | 7508 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 7509 | `		/* Clean-up */` |
|     11 | 7510 | `		SyBlobRelease(&sPool);` |
|     11 | 7511 | `		SyBlobRelease(&sWorker);` |
|     11 | 7512 | `		SySetRelease(&sTable);` |
|     11 | 7513 | `		if( rc != PH7_OK ){` |
|    ! 0 | 7514 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 7515 | `		}` |
|      6 | 7516 | `	}else{` |
|      - | 7517 | `		int i,flen,tlen,c,iOfft;` |
|      - | 7518 | `		const char *zFrom,*zTo;` |
|      3 | 7519 | `		if( nArg < 3 ){` |
|      - | 7520 | `			/* Nothing to replace */` |
|    ! 0 | 7521 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 7522 | `			return PH7_OK;` |
|      - | 7523 | `		}` |
|      - | 7524 | `		/* Extract given arguments */` |
|      3 | 7525 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 7526 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 7527 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 7528 | `			/* Nothing to replace */` |
|    ! 0 | 7529 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 7530 | `			return PH7_OK;` |
|      - | 7531 | `		}` |
|      - | 7532 | `		/* Start the replace process */` |
|     13 | 7533 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 7534 | `			c = zIn[i];` |
|     11 | 7535 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 7536 | `				if ( iOfft < tlen ){` |
|      5 | 7537 | `					c = zTo[iOfft];` |
|      2 | 7538 | `				}` |
|      2 | 7539 | `			}` |
|     11 | 7540 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 7541 |  |
|      6 | 7542 | `		}` |
|      - | 7543 | `	}` |
|     13 | 7544 | `	return PH7_OK;` |
|      7 | 7545 | `}` |
|      - | 7546 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7547 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7548 | `/*` |
|      - | 7549 | ` * Parse an INI string.` |
|      - | 7550 |  |
|      - | 7551 | ` * According to wikipedia` |
|      - | 7552 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 7553 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 7554 | ` *  Format` |
|      - | 7555 | `*    Properties` |
|      - | 7556 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 7557 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 7558 | `*     Example:` |
|      - | 7559 | `*      name=value` |
|      - | 7560 | `*    Sections` |
|      - | 7561 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 7562 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 7563 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 7564 | `*     or the end of the file. Sections may not be nested.` |
|      - | 7565 | `*     Example:` |
|      - | 7566 | `*      [section]` |
|      - | 7567 | `*   Comments` |
|      - | 7568 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 7569 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 7570 | `*/` |
|     12 | 7571 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 7572 | `{` |
|      - | 7573 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 7574 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 7575 | `	SyHashEntry *pEntry;` |
|      - | 7576 | `	SyString sEntry;` |
|      - | 7577 | `	SyHash sHash;` |
|      - | 7578 | `	int c;` |
|      - | 7579 | `	/* Create an empty array and worker variables */` |
|     13 | 7580 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 7581 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 7582 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 7583 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 7584 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|    ! 0 | 7585 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7586 | `	}` |
|     13 | 7587 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 7588 | `	pCur = pArray;` |
|      - | 7589 | `	/* Start the parse process */` |
|     21 | 7590 | `	for(;;){` |
|      - | 7591 | `		/* Ignore leading white spaces */` |
|     69 | 7592 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 7593 | `			zIn++;` |
|      1 | 7594 | `		}` |
|     43 | 7595 | `		if( zIn >= zEnd ){` |
|      - | 7596 | `			/* No more input to process */` |
|     13 | 7597 | `			break;` |
|      - | 7598 | `		}` |
|     31 | 7599 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 7600 | `			/* Comment til the end of line */` |
|    ! 0 | 7601 | `			zIn++;` |
|    ! 0 | 7602 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 7603 | `				zIn++;` |
|    ! 0 | 7604 | `			}` |
|    ! 0 | 7605 | `			continue;` |
|      - | 7606 | `		}` |
|      - | 7607 | `		/* Reset the string cursor of the working variable */` |
|     31 | 7608 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 7609 | `		if( zIn[0] == '[' ){` |
|      - | 7610 | `			/* Section: Extract the section name */` |
|      9 | 7611 | `			zIn++;` |
|      9 | 7612 | `			zCur = zIn;` |
|     73 | 7613 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 7614 | `				zIn++;` |
|      1 | 7615 | `			}` |
|      9 | 7616 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 7617 | `				/* Save the section name */` |
|      5 | 7618 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 7619 | `				SyStringFullTrim(&sEntry);` |
|      5 | 7620 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 7621 | `				if( sEntry.nByte > 0 ){` |
|      - | 7622 | `					/* Associate an array with the section */` |
|      5 | 7623 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 7624 | `					if( pSection ){` |
|      5 | 7625 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 7626 | `						pCur = pSection;` |
|      2 | 7627 | `					}` |
|      2 | 7628 | `				}` |
|      2 | 7629 | `			}` |
|      9 | 7630 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 7631 | `		}else{` |
|      - | 7632 | `			ph7_value *pOldCur;` |
|      - | 7633 | `			int is_array;` |
|      - | 7634 | `			int iLen;` |
|      - | 7635 | `			/* Properties */` |
|     23 | 7636 | `			is_array = 0;` |
|     23 | 7637 | `			zCur = zIn;` |
|     23 | 7638 | `			iLen = 0; /* cc warning */` |
|     23 | 7639 | `			pOldCur = pCur;` |
|    155 | 7640 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 7641 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 7642 | `					/* Array */` |
|    ! 0 | 7643 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 7644 | `					is_array = 1;` |
|    ! 0 | 7645 | `					if( iLen > 0 ){` |
|    ! 0 | 7646 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 7647 | `						/* Query the hashtable */` |
|    ! 0 | 7648 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 7649 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 7650 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 7651 | `						if( pEntry ){` |
|    ! 0 | 7652 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 7653 | `						}else{` |
|      - | 7654 | `							/* Create an empty array */` |
|    ! 0 | 7655 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 7656 | `							if( pvArr ){` |
|      - | 7657 | `								/* Save the entry */` |
|    ! 0 | 7658 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 7659 | `								/* Insert the entry */` |
|    ! 0 | 7660 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 7661 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 7662 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 7663 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 7664 | `							}` |
|      - | 7665 | `						}` |
|    ! 0 | 7666 | `						if( pvArr ){` |
|    ! 0 | 7667 | `							pCur = pvArr;` |
|    ! 0 | 7668 | `						}` |
|    ! 0 | 7669 | `					}` |
|    ! 0 | 7670 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 7671 | `						zIn++;` |
|    ! 0 | 7672 | `					}` |
|    ! 0 | 7673 | `				}` |
|    133 | 7674 | `				zIn++;` |
|      1 | 7675 | `			}` |
|     23 | 7676 | `			if( !is_array ){` |
|     23 | 7677 | `				iLen = (int)(zIn-zCur);` |
|     11 | 7678 | `			}` |
|      - | 7679 | `			/* Trim the key */` |
|     23 | 7680 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 7681 | `			SyStringFullTrim(&sEntry);` |
|     23 | 7682 | `			if( sEntry.nByte > 0 ){` |
|     23 | 7683 | `				if( !is_array ){` |
|      - | 7684 | `					/* Save the key name */` |
|     23 | 7685 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 7686 | `				}` |
|      - | 7687 | `				/* extract key value */` |
|     23 | 7688 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 7689 | `				zIn++; /* '=' */` |
|     39 | 7690 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 7691 | `					zIn++;` |
|      1 | 7692 | `				}` |
|     23 | 7693 | `				if( zIn < zEnd ){` |
|     21 | 7694 | `					zCur = zIn;` |
|     21 | 7695 | `					c = zIn[0];` |
|     21 | 7696 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 7697 | `						zIn++;` |
|      - | 7698 | `						/* Delimit the value */` |
|    ! 0 | 7699 | `						while( zIn < zEnd ){` |
|    ! 0 | 7700 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 7701 | `								break;` |
|      - | 7702 | `							}` |
|    ! 0 | 7703 | `							zIn++;` |
|    ! 0 | 7704 | `						}` |
|    ! 0 | 7705 | `						if( zIn < zEnd ){` |
|    ! 0 | 7706 | `							zIn++;` |
|    ! 0 | 7707 | `						}` |
|    ! 0 | 7708 | `					}else{` |
|    125 | 7709 | `						while( zIn < zEnd ){` |
|    123 | 7710 | `							if( zIn[0] == '\n' ){` |
|     19 | 7711 | `								if( zIn[-1] != '\\' ){` |
|     19 | 7712 | `									break;` |
|    ! 0 | 7713 | `								}` |
|    105 | 7714 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 7715 | `								/* Inline comments */` |
|    ! 0 | 7716 | `								break;` |
|      - | 7717 | `							}` |
|    105 | 7718 | `							zIn++;` |
|      1 | 7719 | `						}` |
|      - | 7720 | `					}` |
|      - | 7721 | `					/* Trim the value */` |
|     21 | 7722 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 7723 | `					SyStringFullTrim(&sEntry);` |
|     21 | 7724 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 7725 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 7726 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 7727 | `					}` |
|     21 | 7728 | `					if( sEntry.nByte > 0 ){` |
|     21 | 7729 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 7730 | `					}` |
|      - | 7731 | `					/* Insert the key and it's value */` |
|     21 | 7732 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 7733 | `				}` |
|     12 | 7734 | `			}else{` |
|    ! 0 | 7735 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 7736 | `					zIn++;` |
|    ! 0 | 7737 | `				}` |
|      - | 7738 | `			}` |
|     23 | 7739 | `			pCur = pOldCur;` |
|      - | 7740 | `		}` |
|      1 | 7741 | `	}` |
|     13 | 7742 | `	SyHashRelease(&sHash);` |
|      - | 7743 | `	/* Return the parse of the INI string */` |
|     13 | 7744 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 7745 | `	return SXRET_OK;` |
|      7 | 7746 | `}` |
|      - | 7747 | `/*` |
|      - | 7748 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 7749 | ` *  Parse a configuration string.` |
|      - | 7750 | ` * Parameters` |
|      - | 7751 | ` *  $ini` |
|      - | 7752 | ` *   The contents of the ini file being parsed.` |
|      - | 7753 | ` *  $process_sections` |
|      - | 7754 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 7755 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 7756 | ` *  $scanner_mode (Not used)` |
|      - | 7757 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 7758 | ` *   then option values will not be parsed.` |
|      - | 7759 | ` * Return` |
|      - | 7760 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 7761 | ` */` |
|     10 | 7762 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7763 | `{` |
|      - | 7764 | `	const char *zIni;` |
|      - | 7765 | `	int nByte;` |
|     11 | 7766 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7767 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 7768 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7769 | `		return PH7_OK;` |
|      - | 7770 | `	}` |
|      - | 7771 | `	/* Extract the raw INI buffer */` |
|     11 | 7772 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 7773 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|     11 | 7774 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      6 | 7775 | `}` |
|      - | 7776 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7777 |  |
|      - | 7778 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7779 |  |
|      - | 7780 | `/*` |
|      - | 7781 | ` * Ctype Functions.` |
|      - | 7782 | ` * Status:` |
|      - | 7783 | ` *    Stable.` |
|      - | 7784 | ` */` |
|      - | 7785 | `/*` |
|      - | 7786 | ` * bool ctype_alnum(string $text)` |
|      - | 7787 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 7788 | ` * Parameters` |
|      - | 7789 | ` *  $text` |
|      - | 7790 | ` *   The tested string.` |
|      - | 7791 | ` * Return` |
|      - | 7792 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 7793 | ` */` |
|     14 | 7794 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7795 | `{` |
|      - | 7796 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7797 | `	int nLen;` |
|     15 | 7798 | `	if( nArg < 1 ){` |
|      - | 7799 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7800 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7801 | `		return PH7_OK;` |
|      - | 7802 | `	}` |
|      - | 7803 | `	/* Extract the target string */` |
|     15 | 7804 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 7805 | `	zEnd = &zIn[nLen];` |
|     15 | 7806 | `	if( nLen < 1 ){` |
|      - | 7807 | `		/* Empty string,return FALSE */` |
|      3 | 7808 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7809 | `		return PH7_OK;` |
|      - | 7810 | `	}` |
|      - | 7811 | `	/* Perform the requested operation */` |
|     32 | 7812 | `	for(;;){` |
|     65 | 7813 | `		if( zIn >= zEnd ){` |
|      - | 7814 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7815 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7816 | `			return PH7_OK;` |
|      - | 7817 | `		}` |
|     57 | 7818 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 7819 | `			break;` |
|      - | 7820 | `		}` |
|      - | 7821 | `		/* Point to the next character */` |
|     53 | 7822 | `		zIn++;` |
|      1 | 7823 | `	}` |
|      - | 7824 | `	/* The test failed,return FALSE */` |
|      5 | 7825 | `	ph7_result_bool(pCtx,0);` |
|      5 | 7826 | `	return PH7_OK;` |
|      8 | 7827 | `}` |
|      - | 7828 | `/*` |
|      - | 7829 | ` * bool ctype_alpha(string $text)` |
|      - | 7830 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 7831 | ` * Parameters` |
|      - | 7832 | ` *  $text` |
|      - | 7833 | ` *   The tested string.` |
|      - | 7834 | ` * Return` |
|      - | 7835 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 7836 | ` */` |
|     16 | 7837 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7838 | `{` |
|      - | 7839 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7840 | `	int nLen;` |
|     17 | 7841 | `	if( nArg < 1 ){` |
|      - | 7842 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7843 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7844 | `		return PH7_OK;` |
|      - | 7845 | `	}` |
|      - | 7846 | `	/* Extract the target string */` |
|     17 | 7847 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7848 | `	zEnd = &zIn[nLen];` |
|     17 | 7849 | `	if( nLen < 1 ){` |
|      - | 7850 | `		/* Empty string,return FALSE */` |
|      3 | 7851 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7852 | `		return PH7_OK;` |
|      - | 7853 | `	}` |
|      - | 7854 | `	/* Perform the requested operation */` |
|     42 | 7855 | `	for(;;){` |
|     85 | 7856 | `		if( zIn >= zEnd ){` |
|      - | 7857 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7858 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7859 | `			return PH7_OK;` |
|      - | 7860 | `		}` |
|     77 | 7861 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 7862 | `			break;` |
|      - | 7863 | `		}` |
|      - | 7864 | `		/* Point to the next character */` |
|     71 | 7865 | `		zIn++;` |
|      1 | 7866 | `	}` |
|      - | 7867 | `	/* The test failed,return FALSE */` |
|      7 | 7868 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7869 | `	return PH7_OK;` |
|      9 | 7870 | `}` |
|      - | 7871 | `/*` |
|      - | 7872 | ` * bool ctype_cntrl(string $text)` |
|      - | 7873 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 7874 | ` * Parameters` |
|      - | 7875 | ` *  $text` |
|      - | 7876 | ` *   The tested string.` |
|      - | 7877 | ` * Return` |
|      - | 7878 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 7879 | ` */` |
|     16 | 7880 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7881 | `{` |
|      - | 7882 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7883 | `	int nLen;` |
|     17 | 7884 | `	if( nArg < 1 ){` |
|      - | 7885 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7886 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7887 | `		return PH7_OK;` |
|      - | 7888 | `	}` |
|      - | 7889 | `	/* Extract the target string */` |
|     17 | 7890 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7891 | `	zEnd = &zIn[nLen];` |
|     17 | 7892 | `	if( nLen < 1 ){` |
|      - | 7893 | `		/* Empty string,return FALSE */` |
|      3 | 7894 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7895 | `		return PH7_OK;` |
|      - | 7896 | `	}` |
|      - | 7897 | `	/* Perform the requested operation */` |
|     14 | 7898 | `	for(;;){` |
|     29 | 7899 | `		if( zIn >= zEnd ){` |
|      - | 7900 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7901 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7902 | `			return PH7_OK;` |
|      - | 7903 | `		}` |
|     21 | 7904 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7905 | `			/* UTF-8 stream  */` |
|    ! 0 | 7906 | `			break;` |
|      - | 7907 | `		}` |
|     21 | 7908 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 7909 | `			break;` |
|      - | 7910 | `		}` |
|      - | 7911 | `		/* Point to the next character */` |
|     15 | 7912 | `		zIn++;` |
|      1 | 7913 | `	}` |
|      - | 7914 | `	/* The test failed,return FALSE */` |
|      7 | 7915 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7916 | `	return PH7_OK;` |
|      9 | 7917 | `}` |
|      - | 7918 | `/*` |
|      - | 7919 | ` * bool ctype_digit(string $text)` |
|      - | 7920 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 7921 | ` * Parameters` |
|      - | 7922 | ` *  $text` |
|      - | 7923 | ` *   The tested string.` |
|      - | 7924 | ` * Return` |
|      - | 7925 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 7926 | ` */` |
|   1614 | 7927 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7928 | `{` |
|      - | 7929 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7930 | `	int nLen;` |
|   1619 | 7931 | `	if( nArg < 1 ){` |
|      - | 7932 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7933 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7934 | `		return PH7_OK;` |
|      - | 7935 | `	}` |
|      - | 7936 | `	/* Extract the target string */` |
|   1619 | 7937 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1619 | 7938 | `	zEnd = &zIn[nLen];` |
|   1619 | 7939 | `	if( nLen < 1 ){` |
|      - | 7940 | `		/* Empty string,return FALSE */` |
|      3 | 7941 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7942 | `		return PH7_OK;` |
|      - | 7943 | `	}` |
|      - | 7944 | `	/* Perform the requested operation */` |
|   1515 | 7945 | `	for(;;){` |
|   3035 | 7946 | `		if( zIn >= zEnd ){` |
|      - | 7947 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1373 | 7948 | `			ph7_result_bool(pCtx,1);` |
|   1373 | 7949 | `			return PH7_OK;` |
|      - | 7950 | `		}` |
|   1667 | 7951 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7952 | `			/* UTF-8 stream  */` |
|    ! 0 | 7953 | `			break;` |
|      - | 7954 | `		}` |
|   1667 | 7955 | `		if( !SyisDigit(zIn[0]) ){` |
|    249 | 7956 | `			break;` |
|      - | 7957 | `		}` |
|      - | 7958 | `		/* Point to the next character */` |
|   1423 | 7959 | `		zIn++;` |
|      5 | 7960 | `	}` |
|      - | 7961 | `	/* The test failed,return FALSE */` |
|    249 | 7962 | `	ph7_result_bool(pCtx,0);` |
|    249 | 7963 | `	return PH7_OK;` |
|    812 | 7964 | `}` |
|      - | 7965 | `/*` |
|      - | 7966 | ` * bool ctype_xdigit(string $text)` |
|      - | 7967 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 7968 | ` * Parameters` |
|      - | 7969 | ` *  $text` |
|      - | 7970 | ` *   The tested string.` |
|      - | 7971 | ` * Return` |
|      - | 7972 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 7973 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 7974 | ` */` |
|     18 | 7975 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7976 | `{` |
|      - | 7977 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7978 | `	int nLen;` |
|     19 | 7979 | `	if( nArg < 1 ){` |
|      - | 7980 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7981 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7982 | `		return PH7_OK;` |
|      - | 7983 | `	}` |
|      - | 7984 | `	/* Extract the target string */` |
|     19 | 7985 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 7986 | `	zEnd = &zIn[nLen];` |
|     19 | 7987 | `	if( nLen < 1 ){` |
|      - | 7988 | `		/* Empty string,return FALSE */` |
|      3 | 7989 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7990 | `		return PH7_OK;` |
|      - | 7991 | `	}` |
|      - | 7992 | `	/* Perform the requested operation */` |
|     46 | 7993 | `	for(;;){` |
|     93 | 7994 | `		if( zIn >= zEnd ){` |
|      - | 7995 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 7996 | `			ph7_result_bool(pCtx,1);` |
|     11 | 7997 | `			return PH7_OK;` |
|      - | 7998 | `		}` |
|     83 | 7999 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8000 | `			/* UTF-8 stream  */` |
|    ! 0 | 8001 | `			break;` |
|      - | 8002 | `		}` |
|     83 | 8003 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 8004 | `			break;` |
|      - | 8005 | `		}` |
|      - | 8006 | `		/* Point to the next character */` |
|     77 | 8007 | `		zIn++;` |
|      1 | 8008 | `	}` |
|      - | 8009 | `	/* The test failed,return FALSE */` |
|      7 | 8010 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8011 | `	return PH7_OK;` |
|     10 | 8012 | `}` |
|      - | 8013 | `/*` |
|      - | 8014 | ` * bool ctype_graph(string $text)` |
|      - | 8015 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 8016 | ` * Parameters` |
|      - | 8017 | ` *  $text` |
|      - | 8018 | ` *   The tested string.` |
|      - | 8019 | ` * Return` |
|      - | 8020 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 8021 | ` * (no white space), FALSE otherwise.` |
|      - | 8022 | ` */` |
|     16 | 8023 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8024 | `{` |
|      - | 8025 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8026 | `	int nLen;` |
|     17 | 8027 | `	if( nArg < 1 ){` |
|      - | 8028 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8029 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8030 | `		return PH7_OK;` |
|      - | 8031 | `	}` |
|      - | 8032 | `	/* Extract the target string */` |
|     17 | 8033 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8034 | `	zEnd = &zIn[nLen];` |
|     17 | 8035 | `	if( nLen < 1 ){` |
|      - | 8036 | `		/* Empty string,return FALSE */` |
|      3 | 8037 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8038 | `		return PH7_OK;` |
|      - | 8039 | `	}` |
|      - | 8040 | `	/* Perform the requested operation */` |
|     57 | 8041 | `	for(;;){` |
|    115 | 8042 | `		if( zIn >= zEnd ){` |
|      - | 8043 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8044 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8045 | `			return PH7_OK;` |
|      - | 8046 | `		}` |
|    107 | 8047 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8048 | `			/* UTF-8 stream  */` |
|    ! 0 | 8049 | `			break;` |
|      - | 8050 | `		}` |
|    107 | 8051 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 8052 | `			break;` |
|      - | 8053 | `		}` |
|      - | 8054 | `		/* Point to the next character */` |
|    101 | 8055 | `		zIn++;` |
|      1 | 8056 | `	}` |
|      - | 8057 | `	/* The test failed,return FALSE */` |
|      7 | 8058 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8059 | `	return PH7_OK;` |
|      9 | 8060 | `}` |
|      - | 8061 | `/*` |
|      - | 8062 | ` * bool ctype_print(string $text)` |
|      - | 8063 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 8064 | ` * Parameters` |
|      - | 8065 | ` *  $text` |
|      - | 8066 | ` *   The tested string.` |
|      - | 8067 | ` * Return` |
|      - | 8068 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 8069 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 8070 | ` *  or control function at all.` |
|      - | 8071 | ` */` |
|     16 | 8072 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8073 | `{` |
|      - | 8074 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8075 | `	int nLen;` |
|     17 | 8076 | `	if( nArg < 1 ){` |
|      - | 8077 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8078 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8079 | `		return PH7_OK;` |
|      - | 8080 | `	}` |
|      - | 8081 | `	/* Extract the target string */` |
|     17 | 8082 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8083 | `	zEnd = &zIn[nLen];` |
|     17 | 8084 | `	if( nLen < 1 ){` |
|      - | 8085 | `		/* Empty string,return FALSE */` |
|      3 | 8086 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8087 | `		return PH7_OK;` |
|      - | 8088 | `	}` |
|      - | 8089 | `	/* Perform the requested operation */` |
|     63 | 8090 | `	for(;;){` |
|    127 | 8091 | `		if( zIn >= zEnd ){` |
|      - | 8092 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8093 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8094 | `			return PH7_OK;` |
|      - | 8095 | `		}` |
|    119 | 8096 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8097 | `			/* UTF-8 stream  */` |
|    ! 0 | 8098 | `			break;` |
|      - | 8099 | `		}` |
|    119 | 8100 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 8101 | `			break;` |
|      - | 8102 | `		}` |
|      - | 8103 | `		/* Point to the next character */` |
|    113 | 8104 | `		zIn++;` |
|      1 | 8105 | `	}` |
|      - | 8106 | `	/* The test failed,return FALSE */` |
|      7 | 8107 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8108 | `	return PH7_OK;` |
|      9 | 8109 | `}` |
|      - | 8110 | `/*` |
|      - | 8111 | ` * bool ctype_punct(string $text)` |
|      - | 8112 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 8113 | ` * Parameters` |
|      - | 8114 | ` *  $text` |
|      - | 8115 | ` *   The tested string.` |
|      - | 8116 | ` * Return` |
|      - | 8117 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 8118 | ` *  digit or blank, FALSE otherwise.` |
|      - | 8119 | ` */` |
|     18 | 8120 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8121 | `{` |
|      - | 8122 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8123 | `	int nLen;` |
|     19 | 8124 | `	if( nArg < 1 ){` |
|      - | 8125 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8126 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8127 | `		return PH7_OK;` |
|      - | 8128 | `	}` |
|      - | 8129 | `	/* Extract the target string */` |
|     19 | 8130 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 8131 | `	zEnd = &zIn[nLen];` |
|     19 | 8132 | `	if( nLen < 1 ){` |
|      - | 8133 | `		/* Empty string,return FALSE */` |
|      3 | 8134 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8135 | `		return PH7_OK;` |
|      - | 8136 | `	}` |
|      - | 8137 | `	/* Perform the requested operation */` |
|     38 | 8138 | `	for(;;){` |
|     77 | 8139 | `		if( zIn >= zEnd ){` |
|      - | 8140 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8141 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8142 | `			return PH7_OK;` |
|      - | 8143 | `		}` |
|     69 | 8144 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8145 | `			/* UTF-8 stream  */` |
|    ! 0 | 8146 | `			break;` |
|      - | 8147 | `		}` |
|     69 | 8148 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 8149 | `			break;` |
|      - | 8150 | `		}` |
|      - | 8151 | `		/* Point to the next character */` |
|     61 | 8152 | `		zIn++;` |
|      1 | 8153 | `	}` |
|      - | 8154 | `	/* The test failed,return FALSE */` |
|      9 | 8155 | `	ph7_result_bool(pCtx,0);` |
|      9 | 8156 | `	return PH7_OK;` |
|     10 | 8157 | `}` |
|      - | 8158 | `/*` |
|      - | 8159 | ` * bool ctype_space(string $text)` |
|      - | 8160 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 8161 | ` * Parameters` |
|      - | 8162 | ` *  $text` |
|      - | 8163 | ` *   The tested string.` |
|      - | 8164 | ` * Return` |
|      - | 8165 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 8166 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 8167 | ` *  and form feed characters.` |
|      - | 8168 | ` */` |
|  61965 | 8169 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 8170 | `{` |
|      - | 8171 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8172 | `	int nLen;` |
|  61970 | 8173 | `	if( nArg < 1 ){` |
|      - | 8174 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8175 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8176 | `		return PH7_OK;` |
|      - | 8177 | `	}` |
|      - | 8178 | `	/* Extract the target string */` |
|  61970 | 8179 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  61970 | 8180 | `	zEnd = &zIn[nLen];` |
|  61970 | 8181 | `	if( nLen < 1 ){` |
|      - | 8182 | `		/* Empty string,return FALSE */` |
|      3 | 8183 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8184 | `		return PH7_OK;` |
|      - | 8185 | `	}` |
|      - | 8186 | `	/* Perform the requested operation */` |
|  32087 | 8187 | `	for(;;){` |
|  64094 | 8188 | `		if( zIn >= zEnd ){` |
|      - | 8189 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   2107 | 8190 | `			ph7_result_bool(pCtx,1);` |
|   2107 | 8191 | `			return PH7_OK;` |
|      - | 8192 | `		}` |
|  61992 | 8193 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8194 | `			/* UTF-8 stream  */` |
|    ! 0 | 8195 | `			break;` |
|      - | 8196 | `		}` |
|  61992 | 8197 | `		if( !SyisSpace(zIn[0]) ){` |
|  59866 | 8198 | `			break;` |
|      - | 8199 | `		}` |
|      - | 8200 | `		/* Point to the next character */` |
|   2131 | 8201 | `		zIn++;` |
|      5 | 8202 | `	}` |
|      - | 8203 | `	/* The test failed,return FALSE */` |
|  59866 | 8204 | `	ph7_result_bool(pCtx,0);` |
|  59866 | 8205 | `	return PH7_OK;` |
|  31030 | 8206 | `}` |
|      - | 8207 | `/*` |
|      - | 8208 | ` * bool ctype_lower(string $text)` |
|      - | 8209 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 8210 | ` * Parameters` |
|      - | 8211 | ` *  $text` |
|      - | 8212 | ` *   The tested string.` |
|      - | 8213 | ` * Return` |
|      - | 8214 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 8215 | ` */` |
|     16 | 8216 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8217 | `{` |
|      - | 8218 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8219 | `	int nLen;` |
|     17 | 8220 | `	if( nArg < 1 ){` |
|      - | 8221 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8222 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8223 | `		return PH7_OK;` |
|      - | 8224 | `	}` |
|      - | 8225 | `	/* Extract the target string */` |
|     17 | 8226 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8227 | `	zEnd = &zIn[nLen];` |
|     17 | 8228 | `	if( nLen < 1 ){` |
|      - | 8229 | `		/* Empty string,return FALSE */` |
|      3 | 8230 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8231 | `		return PH7_OK;` |
|      - | 8232 | `	}` |
|      - | 8233 | `	/* Perform the requested operation */` |
|     27 | 8234 | `	for(;;){` |
|     55 | 8235 | `		if( zIn >= zEnd ){` |
|      - | 8236 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 8237 | `			ph7_result_bool(pCtx,1);` |
|      5 | 8238 | `			return PH7_OK;` |
|      - | 8239 | `		}` |
|     51 | 8240 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 8241 | `			break;` |
|      - | 8242 | `		}` |
|      - | 8243 | `		/* Point to the next character */` |
|     41 | 8244 | `		zIn++;` |
|      1 | 8245 | `	}` |
|      - | 8246 | `	/* The test failed,return FALSE */` |
|     11 | 8247 | `	ph7_result_bool(pCtx,0);` |
|     11 | 8248 | `	return PH7_OK;` |
|      9 | 8249 | `}` |
|      - | 8250 | `/*` |
|      - | 8251 | ` * bool ctype_upper(string $text)` |
|      - | 8252 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 8253 | ` * Parameters` |
|      - | 8254 | ` *  $text` |
|      - | 8255 | ` *   The tested string.` |
|      - | 8256 | ` * Return` |
|      - | 8257 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 8258 | ` */` |
|     16 | 8259 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8260 | `{` |
|      - | 8261 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8262 | `	int nLen;` |
|     17 | 8263 | `	if( nArg < 1 ){` |
|      - | 8264 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8265 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8266 | `		return PH7_OK;` |
|      - | 8267 | `	}` |
|      - | 8268 | `	/* Extract the target string */` |
|     17 | 8269 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8270 | `	zEnd = &zIn[nLen];` |
|     17 | 8271 | `	if( nLen < 1 ){` |
|      - | 8272 | `		/* Empty string,return FALSE */` |
|      3 | 8273 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8274 | `		return PH7_OK;` |
|      - | 8275 | `	}` |
|      - | 8276 | `	/* Perform the requested operation */` |
|     28 | 8277 | `	for(;;){` |
|     57 | 8278 | `		if( zIn >= zEnd ){` |
|      - | 8279 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 8280 | `			ph7_result_bool(pCtx,1);` |
|      5 | 8281 | `			return PH7_OK;` |
|      - | 8282 | `		}` |
|     53 | 8283 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 8284 | `			break;` |
|      - | 8285 | `		}` |
|      - | 8286 | `		/* Point to the next character */` |
|     43 | 8287 | `		zIn++;` |
|      1 | 8288 | `	}` |
|      - | 8289 | `	/* The test failed,return FALSE */` |
|     11 | 8290 | `	ph7_result_bool(pCtx,0);` |
|     11 | 8291 | `	return PH7_OK;` |
|      9 | 8292 | `}` |
|      - | 8293 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 8294 | `/*` |
|      - | 8295 | ` * Section:` |
|      - | 8296 | ` *    URL handling Functions.` |
|      - | 8297 | ` * Status:` |
|      - | 8298 | ` *    Stable.` |
|      - | 8299 | ` */` |
|      - | 8300 | `/*` |
|      - | 8301 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 8302 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 8303 | ` */` |
|   1026 | 8304 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 8305 | `{` |
|      - | 8306 | `	/* Store in the call context result buffer */` |
|   1028 | 8307 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 8308 | `	return SXRET_OK;` |
|      2 | 8309 | `}` |
|      - | 8310 | `/*` |
|      - | 8311 | ` * string base64_encode(string $data)` |
|      - | 8312 | ` * string convert_uuencode(string $data)` |
|      - | 8313 | ` *  Encodes data with MIME base64` |
|      - | 8314 | ` * Parameter` |
|      - | 8315 | ` *  $data` |
|      - | 8316 | ` *    Data to encode` |
|      - | 8317 | ` * Return` |
|      - | 8318 | ` *  Encoded data or FALSE on failure.` |
|      - | 8319 | ` */` |
|      6 | 8320 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8321 | `{` |
|      - | 8322 | `	const char *zIn;` |
|      - | 8323 | `	int nLen;` |
|      7 | 8324 | `	if( nArg < 1 ){` |
|      - | 8325 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8326 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8327 | `		return PH7_OK;` |
|      - | 8328 | `	}` |
|      - | 8329 | `	/* Extract the input string */` |
|      7 | 8330 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8331 | `	if( nLen < 1 ){` |
|      - | 8332 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8333 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8334 | `		return PH7_OK;` |
|      - | 8335 | `	}` |
|      - | 8336 | `	/* Perform the BASE64 encoding */` |
|      7 | 8337 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 8338 | `	return PH7_OK;` |
|      4 | 8339 | `}` |
|      - | 8340 | `/*` |
|      - | 8341 | ` * string base64_decode(string $data)` |
|      - | 8342 | ` * string convert_uudecode(string $data)` |
|      - | 8343 | ` *  Decodes data encoded with MIME base64` |
|      - | 8344 | ` * Parameter` |
|      - | 8345 | ` *  $data` |
|      - | 8346 | ` *    Encoded data.` |
|      - | 8347 | ` * Return` |
|      - | 8348 | ` *  Returns the original data or FALSE on failure.` |
|      - | 8349 | ` */` |
|     34 | 8350 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 8351 | `{` |
|      - | 8352 | `	const char *zIn;` |
|      - | 8353 | `	int nLen;` |
|     36 | 8354 | `	if( nArg < 1 ){` |
|      - | 8355 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8356 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8357 | `		return PH7_OK;` |
|      - | 8358 | `	}` |
|      - | 8359 | `	/* Extract the input string */` |
|     36 | 8360 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 8361 | `	if( nLen < 1 ){` |
|      - | 8362 | `		/* Nothing to process,return FALSE */` |
|      3 | 8363 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8364 | `		return PH7_OK;` |
|      - | 8365 | `	}` |
|      - | 8366 | `	/* Perform the BASE64 decoding */` |
|     34 | 8367 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 8368 | `	return PH7_OK;` |
|     19 | 8369 | `}` |
|      - | 8370 | `/*` |
|      - | 8371 | ` * string urlencode(string $str)` |
|      - | 8372 | ` *  URL encoding` |
|      - | 8373 | ` * Parameter` |
|      - | 8374 | ` *  $data` |
|      - | 8375 | ` *   Input string.` |
|      - | 8376 | ` * Return` |
|      - | 8377 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 8378 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 8379 | ` *  encoded as plus (+) signs.` |
|      - | 8380 | ` */` |
|      4 | 8381 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8382 | `{` |
|      - | 8383 | `	const char *zIn;` |
|      - | 8384 | `	int nLen;` |
|      5 | 8385 | `	if( nArg < 1 ){` |
|      - | 8386 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8387 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8388 | `		return PH7_OK;` |
|      - | 8389 | `	}` |
|      - | 8390 | `	/* Extract the input string */` |
|      5 | 8391 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 8392 | `	if( nLen < 1 ){` |
|      - | 8393 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8394 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8395 | `		return PH7_OK;` |
|      - | 8396 | `	}` |
|      - | 8397 | `	/* Perform the URL encoding */` |
|      5 | 8398 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 8399 | `	return PH7_OK;` |
|      3 | 8400 | `}` |
|      - | 8401 | `/*` |
|      - | 8402 | ` * string urldecode(string $str)` |
|      - | 8403 | ` *  Decodes any %## encoding in the given string.` |
|      - | 8404 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 8405 | ` * Parameter` |
|      - | 8406 | ` *  $data` |
|      - | 8407 | ` *    Input string.` |
|      - | 8408 | ` * Return` |
|      - | 8409 | ` *  Decoded URL or FALSE on failure.` |
|      - | 8410 | ` */` |
|      6 | 8411 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8412 | `{` |
|      - | 8413 | `	const char *zIn;` |
|      - | 8414 | `	int nLen;` |
|      7 | 8415 | `	if( nArg < 1 ){` |
|      - | 8416 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8417 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8418 | `		return PH7_OK;` |
|      - | 8419 | `	}` |
|      - | 8420 | `	/* Extract the input string */` |
|      7 | 8421 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8422 | `	if( nLen < 1 ){` |
|      - | 8423 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8424 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8425 | `		return PH7_OK;` |
|      - | 8426 | `	}` |
|      - | 8427 | `	/* Perform the URL decoding */` |
|      7 | 8428 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 8429 | `	return PH7_OK;` |
|      4 | 8430 | `}` |
|      - | 8431 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8432 | `/* Table of the built-in functions */` |
|      - | 8433 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 8434 | `	   /* Variable handling functions */` |
|      - | 8435 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 8436 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 8437 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 8438 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 8439 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 8440 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 8441 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 8442 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 8443 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 8444 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 8445 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 8446 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 8447 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 8448 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 8449 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 8450 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 8451 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 8452 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 8453 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 8454 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 8455 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8456 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 8457 | `	   /* Math functions */` |
|      - | 8458 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 8459 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 8460 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 8461 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 8462 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 8463 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 8464 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 8465 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 8466 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 8467 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 8468 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 8469 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 8470 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 8471 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 8472 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 8473 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 8474 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 8475 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 8476 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 8477 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 8478 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 8479 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 8480 | `	{ "round",    PH7_builtin_round        },` |
|      - | 8481 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 8482 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 8483 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 8484 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 8485 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 8486 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 8487 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 8488 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 8489 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 8490 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8491 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8492 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 8493 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8494 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8495 | `	   /* String handling functions */` |
|      - | 8496 |  |
|      - | 8497 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 8498 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 8499 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 8500 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 8501 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 8502 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 8503 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 8504 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 8505 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 8506 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 8507 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 8508 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 8509 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 8510 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 8511 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 8512 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 8513 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 8514 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 8515 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 8516 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 8517 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 8518 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 8519 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 8520 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 8521 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 8522 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 8523 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 8524 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 8525 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 8526 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 8527 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 8528 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 8529 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 8530 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 8531 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 8532 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 8533 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 8534 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 8535 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 8536 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 8537 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 8538 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 8539 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 8540 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 8541 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 8542 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 8543 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 8544 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 8545 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - | 8546 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - | 8547 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - | 8548 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 8549 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8550 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8551 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 8552 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 8553 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 8554 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 8555 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8556 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8557 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 8558 |  |
|      - | 8559 |  |
|      - | 8560 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 8561 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 8562 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 8563 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 8564 | `	{ "hash",         PH7_builtin_hash      },` |
|      - | 8565 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|      - | 8566 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|      - | 8567 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|      - | 8568 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 8569 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|      - | 8570 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|      - | 8571 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|      - | 8572 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|      - | 8573 | `	{ "filter_var",            PH7_builtin_filter_var },` |
|      - | 8574 | `	{ "filter_input",          PH7_builtin_filter_input },` |
|      - | 8575 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8576 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8577 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 8578 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 8579 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8580 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8581 |  |
|      - | 8582 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 8583 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 8584 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 8585 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 8586 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 8587 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 8588 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 8589 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 8590 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 8591 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 8592 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 8593 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 8594 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8595 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8596 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 8597 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8598 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8599 |  |
|      - | 8600 | `	         /* Ctype functions */` |
|      - | 8601 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 8602 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 8603 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 8604 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 8605 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 8606 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 8607 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 8608 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 8609 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 8610 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 8611 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 8612 | `	         /* Time functions */` |
|      - | 8613 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 8614 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 8615 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 8616 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 8617 | `	{ "date",        PH7_builtin_date         },` |
|      - | 8618 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 8619 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 8620 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 8621 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 8622 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 8623 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 8624 | `	        /* URL functions */` |
|      - | 8625 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 8626 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 8627 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 8628 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 8629 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 8630 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 8631 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 8632 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 8633 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8634 | `};` |
|      - | 8635 | `/*` |
|      - | 8636 | ` * Register the built-in functions defined above,the array functions` |
|      - | 8637 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 8638 | ` */` |
|   3474 | 8639 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      5 | 8640 | `{` |
|      - | 8641 | `	sxu32 n;` |
| 583637 | 8642 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 580163 | 8643 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 290084 | 8644 | `	}` |
|      - | 8645 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   3479 | 8646 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 8647 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   3479 | 8648 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   3479 | 8649 | `}` |
|      - | 8650 |  |
