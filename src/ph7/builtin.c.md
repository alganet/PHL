# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3767/4423 lines (85.17%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `/* This file implement built-in 'foreign' functions for the PH7 engine */` |
|      - |    8 | `/*` |
|      - |    9 | ` * Section:` |
|      - |   10 | ` *    Variable handling Functions.` |
|      - |   11 | ` * Status:` |
|      - |   12 | ` *    Stable.` |
|      - |   13 | ` */` |
|      - |   14 | `/*` |
|      - |   15 | ` * bool is_bool($var)` |
|      - |   16 | ` *  Finds out whether a variable is a boolean.` |
|      - |   17 | ` * Parameters` |
|      - |   18 | ` *   $var: The variable being evaluated.` |
|      - |   19 | ` * Return` |
|      - |   20 | ` *  TRUE if var is a boolean. False otherwise.` |
|      - |   21 | ` */` |
|     32 |   22 | `static int PH7_builtin_is_bool(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   23 |  |
|     33 |   24 | `	int res = 0; /* Assume false by default */` |
|     33 |   25 | `	if( nArg > 0 ){` |
|     29 |   26 | `		res = ph7_value_is_bool(apArg[0]);` |
|     14 |   27 | `	}` |
|      - |   28 | `	/* Query result */` |
|     33 |   29 | `	ph7_result_bool(pCtx,res);` |
|     33 |   30 | `	return PH7_OK;` |
|      1 |   31 |  |
|      - |   32 | `/*` |
|      - |   33 | ` * bool is_float($var)` |
|      - |   34 | ` * bool is_real($var)` |
|      - |   35 | ` * bool is_double($var)` |
|      - |   36 | ` *  Finds out whether a variable is a float.` |
|      - |   37 | ` * Parameters` |
|      - |   38 | ` *   $var: The variable being evaluated.` |
|      - |   39 | ` * Return` |
|      - |   40 | ` *  TRUE if var is a float. False otherwise.` |
|      - |   41 | ` */` |
|     42 |   42 | `static int PH7_builtin_is_float(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   43 |  |
|     43 |   44 | `	int res = 0; /* Assume false by default */` |
|     43 |   45 | `	if( nArg > 0 ){` |
|     41 |   46 | `		res = ph7_value_is_float(apArg[0]);` |
|     20 |   47 | `	}` |
|      - |   48 | `	/* Query result */` |
|     43 |   49 | `	ph7_result_bool(pCtx,res);` |
|     43 |   50 | `	return PH7_OK;` |
|      1 |   51 |  |
|      - |   52 | `/*` |
|      - |   53 | ` * bool is_int($var)` |
|      - |   54 | ` * bool is_integer($var)` |
|      - |   55 | ` * bool is_long($var)` |
|      - |   56 | ` *  Finds out whether a variable is an integer.` |
|      - |   57 | ` * Parameters` |
|      - |   58 | ` *   $var: The variable being evaluated.` |
|      - |   59 | ` * Return` |
|      - |   60 | ` *  TRUE if var is an integer. False otherwise.` |
|      - |   61 | ` */` |
|     96 |   62 | `static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   63 |  |
|     97 |   64 | `	int res = 0; /* Assume false by default */` |
|     97 |   65 | `	if( nArg > 0 ){` |
|     95 |   66 | `		res = ph7_value_is_int(apArg[0]);` |
|     47 |   67 | `	}` |
|      - |   68 | `	/* Query result */` |
|     97 |   69 | `	ph7_result_bool(pCtx,res);` |
|     97 |   70 | `	return PH7_OK;` |
|      1 |   71 |  |
|      - |   72 | `/*` |
|      - |   73 | ` * bool is_string($var)` |
|      - |   74 | ` *  Finds out whether a variable is a string.` |
|      - |   75 | ` * Parameters` |
|      - |   76 | ` *   $var: The variable being evaluated.` |
|      - |   77 | ` * Return` |
|      - |   78 | ` *  TRUE if var is string. False otherwise.` |
|      - |   79 | ` */` |
|     56 |   80 | `static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   81 |  |
|     57 |   82 | `	int res = 0; /* Assume false by default */` |
|     57 |   83 | `	if( nArg > 0 ){` |
|     55 |   84 | `		res = ph7_value_is_string(apArg[0]);` |
|     27 |   85 | `	}` |
|      - |   86 | `	/* Query result */` |
|     57 |   87 | `	ph7_result_bool(pCtx,res);` |
|     57 |   88 | `	return PH7_OK;` |
|      1 |   89 |  |
|      - |   90 | `/*` |
|      - |   91 | ` * bool is_null($var)` |
|      - |   92 | ` *  Finds out whether a variable is NULL.` |
|      - |   93 | ` * Parameters` |
|      - |   94 | ` *   $var: The variable being evaluated.` |
|      - |   95 | ` * Return` |
|      - |   96 | ` *  TRUE if var is NULL. False otherwise.` |
|      - |   97 | ` */` |
|     22 |   98 | `static int PH7_builtin_is_null(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   99 |  |
|     23 |  100 | `	int res = 0; /* Assume false by default */` |
|     23 |  101 | `	if( nArg > 0 ){` |
|     21 |  102 | `		res = ph7_value_is_null(apArg[0]);` |
|     10 |  103 | `	}` |
|      - |  104 | `	/* Query result */` |
|     23 |  105 | `	ph7_result_bool(pCtx,res);` |
|     23 |  106 | `	return PH7_OK;` |
|      1 |  107 |  |
|      - |  108 | `/*` |
|      - |  109 | ` * bool is_numeric($var)` |
|      - |  110 | ` *  Find out whether a variable is NULL.` |
|      - |  111 | ` * Parameters` |
|      - |  112 | ` *  $var: The variable being evaluated.` |
|      - |  113 | ` * Return` |
|      - |  114 | ` *  True if var is numeric. False otherwise.` |
|      - |  115 | ` */` |
|     28 |  116 | `static int PH7_builtin_is_numeric(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  117 |  |
|     30 |  118 | `	int res = 0; /* Assume false by default */` |
|     30 |  119 | `	if( nArg > 0 ){` |
|     28 |  120 | `		res = ph7_value_is_numeric(apArg[0]);` |
|     13 |  121 | `	}` |
|      - |  122 | `	/* Query result */` |
|     30 |  123 | `	ph7_result_bool(pCtx,res);` |
|     30 |  124 | `	return PH7_OK;` |
|      2 |  125 |  |
|      - |  126 | `/*` |
|      - |  127 | ` * bool is_scalar($var)` |
|      - |  128 | ` *  Find out whether a variable is a scalar.` |
|      - |  129 | ` * Parameters` |
|      - |  130 | ` *  $var: The variable being evaluated.` |
|      - |  131 | ` * Return` |
|      - |  132 | ` *  True if var is scalar. False otherwise.` |
|      - |  133 | ` */` |
|     14 |  134 | `static int PH7_builtin_is_scalar(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  135 |  |
|     15 |  136 | `	int res = 0; /* Assume false by default */` |
|     15 |  137 | `	if( nArg > 0 ){` |
|     13 |  138 | `		res = ph7_value_is_scalar(apArg[0]);` |
|      6 |  139 | `	}` |
|      - |  140 | `	/* Query result */` |
|     15 |  141 | `	ph7_result_bool(pCtx,res);` |
|     15 |  142 | `	return PH7_OK;` |
|      1 |  143 |  |
|      - |  144 | `/*` |
|      - |  145 | ` * bool is_array($var)` |
|      - |  146 | ` *  Find out whether a variable is an array.` |
|      - |  147 | ` * Parameters` |
|      - |  148 | ` *  $var: The variable being evaluated.` |
|      - |  149 | ` * Return` |
|      - |  150 | ` *  True if var is an array. False otherwise.` |
|      - |  151 | ` */` |
|     90 |  152 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  153 |  |
|     92 |  154 | `	int res = 0; /* Assume false by default */` |
|     92 |  155 | `	if( nArg > 0 ){` |
|     90 |  156 | `		res = ph7_value_is_array(apArg[0]);` |
|     44 |  157 | `	}` |
|      - |  158 | `	/* Query result */` |
|     92 |  159 | `	ph7_result_bool(pCtx,res);` |
|     92 |  160 | `	return PH7_OK;` |
|      2 |  161 |  |
|      - |  162 | `/*` |
|      - |  163 | ` * bool is_object($var)` |
|      - |  164 | ` *  Find out whether a variable is an object.` |
|      - |  165 | ` * Parameters` |
|      - |  166 | ` *  $var: The variable being evaluated.` |
|      - |  167 | ` * Return` |
|      - |  168 | ` *  True if var is an object. False otherwise.` |
|      - |  169 | ` */` |
|     20 |  170 | `static int PH7_builtin_is_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  171 |  |
|     21 |  172 | `	int res = 0; /* Assume false by default */` |
|     21 |  173 | `	if( nArg > 0 ){` |
|     19 |  174 | `		res = ph7_value_is_object(apArg[0]);` |
|      9 |  175 | `	}` |
|      - |  176 | `	/* Query result */` |
|     21 |  177 | `	ph7_result_bool(pCtx,res);` |
|     21 |  178 | `	return PH7_OK;` |
|      1 |  179 |  |
|      - |  180 | `/*` |
|      - |  181 | ` * bool is_resource($var)` |
|      - |  182 | ` *  Find out whether a variable is a resource.` |
|      - |  183 | ` * Parameters` |
|      - |  184 | ` *  $var: The variable being evaluated.` |
|      - |  185 | ` * Return` |
|      - |  186 | ` *  True if a resource. False otherwise.` |
|      - |  187 | ` */` |
|     60 |  188 | `static int PH7_builtin_is_resource(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  189 |  |
|     62 |  190 | `	int res = 0; /* Assume false by default */` |
|     62 |  191 | `	if( nArg > 0 ){` |
|     60 |  192 | `		res = ph7_value_is_resource(apArg[0]);` |
|     29 |  193 | `	}` |
|     62 |  194 | `	ph7_result_bool(pCtx,res);` |
|     62 |  195 | `	return PH7_OK;` |
|      2 |  196 |  |
|      - |  197 | `/*` |
|      - |  198 | ` * float floatval($var)` |
|      - |  199 | ` *  Get float value of a variable.` |
|      - |  200 | ` * Parameter` |
|      - |  201 | ` *  $var: The variable being processed.` |
|      - |  202 | ` * Return` |
|      - |  203 | ` *  the float value of a variable.` |
|      - |  204 | ` */` |
|      6 |  205 | `static int PH7_builtin_floatval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  206 |  |
|      7 |  207 | `	if( nArg < 1 ){` |
|      - |  208 | `		/* return 0.0 */` |
|      3 |  209 | `		ph7_result_double(pCtx,0);` |
|      2 |  210 | `	}else{` |
|      - |  211 | `		double dval;` |
|      - |  212 | `		/* Perform the cast */` |
|      5 |  213 | `		dval = ph7_value_to_double(apArg[0]);` |
|      5 |  214 | `		ph7_result_double(pCtx,dval);` |
|      - |  215 | `	}` |
|      7 |  216 | `	return PH7_OK;` |
|      1 |  217 |  |
|      - |  218 | `/*` |
|      - |  219 | ` * int intval($var)` |
|      - |  220 | ` *  Get integer value of a variable.` |
|      - |  221 | ` * Parameter` |
|      - |  222 | ` *  $var: The variable being processed.` |
|      - |  223 | ` * Return` |
|      - |  224 | ` *  the int value of a variable.` |
|      - |  225 | ` */` |
|      6 |  226 | `static int PH7_builtin_intval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  227 |  |
|      7 |  228 | `	if( nArg < 1 ){` |
|      - |  229 | `		/* return 0 */` |
|      3 |  230 | `		ph7_result_int(pCtx,0);` |
|      2 |  231 | `	}else{` |
|      - |  232 | `		sxi64 iVal;` |
|      - |  233 | `		/* Perform the cast */` |
|      5 |  234 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|      5 |  235 | `		ph7_result_int64(pCtx,iVal);` |
|      - |  236 | `	}` |
|      7 |  237 | `	return PH7_OK;` |
|      1 |  238 |  |
|      - |  239 | `/*` |
|      - |  240 | ` * string strval($var)` |
|      - |  241 | ` *  Get the string representation of a variable.` |
|      - |  242 | ` * Parameter` |
|      - |  243 | ` *  $var: The variable being processed.` |
|      - |  244 | ` * Return` |
|      - |  245 | ` *  the string value of a variable.` |
|      - |  246 | ` */` |
|      4 |  247 | `static int PH7_builtin_strval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  248 |  |
|      5 |  249 | `	if( nArg < 1 ){` |
|      - |  250 | `		/* return NULL */` |
|      3 |  251 | `		ph7_result_null(pCtx);` |
|      2 |  252 | `	}else{` |
|      - |  253 | `		const char *zVal;` |
|      3 |  254 | `		int iLen = 0; /* cc -O6 warning */` |
|      - |  255 | `		/* Perform the cast */` |
|      3 |  256 | `		zVal = ph7_value_to_string(apArg[0],&iLen);` |
|      3 |  257 | `		ph7_result_string(pCtx,zVal,iLen);` |
|      - |  258 | `	}` |
|      5 |  259 | `	return PH7_OK;` |
|      1 |  260 |  |
|      - |  261 | `/*` |
|      - |  262 | ` * bool empty($var)` |
|      - |  263 | ` *  Determine whether a variable is empty.` |
|      - |  264 | ` * Parameters` |
|      - |  265 | ` *   $var: The variable being checked.` |
|      - |  266 | ` * Return` |
|      - |  267 | ` *  0 if var has a non-empty and non-zero value.1 otherwise.` |
|      - |  268 | ` */` |
|  16958 |  269 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  270 |  |
|  16960 |  271 | `	int res = 1; /* Assume empty by default */` |
|  16960 |  272 | `	if( nArg > 0 ){` |
|  16958 |  273 | `		res = ph7_value_is_empty(apArg[0]);` |
|   8478 |  274 | `	}` |
|  16960 |  275 | `	ph7_result_bool(pCtx,res);` |
|  16960 |  276 | `	return PH7_OK;` |
|      - |  277 |  |
|      2 |  278 |  |
|      - |  279 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |  280 | `#define PH7_NEED_BUILTIN_REG 1` |
|      - |  281 | `#endif` |
|      - |  282 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - |  283 | `#define PH7_NEED_FMT_AND_INI 1` |
|      - |  284 | `#endif` |
|      - |  285 |  |
|      - |  286 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - |  287 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - |  288 |  |
|      - |  289 | `/*` |
|      - |  290 | ` * Section:` |
|      - |  291 | ` *    Math Functions.` |
|      - |  292 |  |
|      - |  293 | ` * Status:` |
|      - |  294 | ` *    Stable.` |
|      - |  295 | ` */` |
|      - |  296 | `#include <stdlib.h> /* abs */` |
|      - |  297 | `#include <math.h>` |
|      - |  298 | `/*` |
|      - |  299 | ` * float sqrt(float $arg )` |
|      - |  300 | ` *  Square root of the given number.` |
|      - |  301 | ` * Parameter` |
|      - |  302 | ` *  The number to process.` |
|      - |  303 | ` * Return` |
|      - |  304 | ` *  The square root of arg or the special value Nan of failure.` |
|      - |  305 | ` */` |
|      6 |  306 | `static int PH7_builtin_sqrt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  307 |  |
|      - |  308 | `	double r,x;` |
|      7 |  309 | `	if( nArg < 1 ){` |
|      - |  310 | `		/* Missing argument,return 0 */` |
|      5 |  311 | `		ph7_result_int(pCtx,0);` |
|      5 |  312 | `		return PH7_OK;` |
|      - |  313 | `	}` |
|      3 |  314 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  315 | `	/* Perform the requested operation */` |
|      3 |  316 | `	r = sqrt(x);` |
|      - |  317 | `	/* store the result back */` |
|      3 |  318 | `	ph7_result_double(pCtx,r);` |
|      3 |  319 | `	return PH7_OK;` |
|      4 |  320 |  |
|      - |  321 | `/*` |
|      - |  322 | ` * float exp(float $arg )` |
|      - |  323 | ` *  Calculates the exponent of e.` |
|      - |  324 | ` * Parameter` |
|      - |  325 | ` *  The number to process.` |
|      - |  326 | ` * Return` |
|      - |  327 | ` *  'e' raised to the power of arg.` |
|      - |  328 | ` */` |
|     20 |  329 | `static int PH7_builtin_exp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  330 |  |
|      - |  331 | `	double r,x;` |
|     21 |  332 | `	if( nArg < 1 ){` |
|      - |  333 | `		/* Missing argument,return 0 */` |
|      3 |  334 | `		ph7_result_int(pCtx,0);` |
|      3 |  335 | `		return PH7_OK;` |
|      - |  336 | `	}` |
|     19 |  337 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  338 | `	/* Perform the requested operation */` |
|     19 |  339 | `	r = exp(x);` |
|      - |  340 | `	/* store the result back */` |
|     19 |  341 | `	ph7_result_double(pCtx,r);` |
|     19 |  342 | `	return PH7_OK;` |
|     11 |  343 |  |
|      - |  344 | `/*` |
|      - |  345 | ` * float floor(float $arg )` |
|      - |  346 | ` *  Round fractions down.` |
|      - |  347 | ` * Parameter` |
|      - |  348 | ` *  The number to process.` |
|      - |  349 | ` * Return` |
|      - |  350 | ` *  Returns the next lowest integer value by rounding down value if necessary.` |
|      - |  351 | ` */` |
|     14 |  352 | `static int PH7_builtin_floor(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  353 |  |
|      - |  354 | `	double r,x;` |
|      - |  355 | `	/* PHP requires exactly one argument. */` |
|     16 |  356 | `	if( nArg != 1 ){` |
|      7 |  357 | `		return PH7_VmThrowException(pCtx,` |
|      - |  358 | `			"ArgumentCountError",` |
|      - |  359 | `			"floor() expects exactly 1 argument, %d given",` |
|      2 |  360 | `			nArg` |
|      - |  361 | `			);` |
|      - |  362 | `	}` |
|      - |  363 | `	/*` |
|      - |  364 | `	 * Validate argument type. Only int/float (and numeric strings) are accepted.` |
|      - |  365 | `	 * Other types (including non-numeric strings) raise a TypeError just like` |
|      - |  366 | `	 * ceil() and other math functions.` |
|      - |  367 | `	 */` |
|     12 |  368 | `	if( ph7_value_is_int(apArg[0]) == 0 && ph7_value_is_float(apArg[0]) == 0 ){` |
|      6 |  369 | `		if( ph7_value_is_string(apArg[0]) ){` |
|      - |  370 | `			int len;` |
|      6 |  371 | `			sxu8 bReal = FALSE;` |
|      6 |  372 | `			const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|      - |  373 | `			sxi32 rcNum;` |
|      6 |  374 | `			rcNum = SyStrIsNumeric(zStr, len, &bReal, 0);` |
|      6 |  375 | `			if( rcNum != SXRET_OK ){` |
|      4 |  376 | `				return PH7_VmThrowException(pCtx,` |
|      - |  377 | `					"TypeError",` |
|      - |  378 | `					"floor(): Argument #1 ($num) must be of type int\|float, %s given",` |
|      1 |  379 | `					ph7_type_name(apArg[0])` |
|      - |  380 | `					);` |
|      - |  381 | `			}` |
|      2 |  382 | `		}else{` |
|      - |  383 | `			/* Disallow all other types (arrays, objects, resources, etc.) */` |
|    ! 0 |  384 | `			return PH7_VmThrowException(pCtx,` |
|      - |  385 | `				"TypeError",` |
|      - |  386 | `				"floor(): Argument #1 ($num) must be of type int\|float, %s given",` |
|    ! 0 |  387 | `				ph7_type_name(apArg[0])` |
|      - |  388 | `				);` |
|      - |  389 | `		}` |
|      1 |  390 | `	}` |
|      - |  391 |  |
|      9 |  392 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  393 | `	/* Perform the requested operation */` |
|      9 |  394 | `	r = floor(x);` |
|      - |  395 | `	/* store the result back */` |
|      9 |  396 | `	ph7_result_double(pCtx,r);` |
|      9 |  397 | `	return PH7_OK;` |
|      9 |  398 |  |
|      - |  399 | `/*` |
|      - |  400 | ` * float cos(float $arg )` |
|      - |  401 | ` *  Cosine.` |
|      - |  402 | ` * Parameter` |
|      - |  403 | ` *  The number to process.` |
|      - |  404 | ` * Return` |
|      - |  405 | ` *  The cosine of arg.` |
|      - |  406 | ` */` |
|      4 |  407 | `static int PH7_builtin_cos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  408 |  |
|      - |  409 | `	double r,x;` |
|      5 |  410 | `	if( nArg < 1 ){` |
|      - |  411 | `		/* Missing argument,return 0 */` |
|      3 |  412 | `		ph7_result_int(pCtx,0);` |
|      3 |  413 | `		return PH7_OK;` |
|      - |  414 | `	}` |
|      3 |  415 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  416 | `	/* Perform the requested operation */` |
|      3 |  417 | `	r = cos(x);` |
|      - |  418 | `	/* store the result back */` |
|      3 |  419 | `	ph7_result_double(pCtx,r);` |
|      3 |  420 | `	return PH7_OK;` |
|      3 |  421 |  |
|      - |  422 | `/*` |
|      - |  423 | ` * float acos(float $arg )` |
|      - |  424 | ` *  Arc cosine.` |
|      - |  425 | ` * Parameter` |
|      - |  426 | ` *  The number to process.` |
|      - |  427 | ` * Return` |
|      - |  428 | ` *  The arc cosine of arg.` |
|      - |  429 | ` */` |
|     22 |  430 | `static int PH7_builtin_acos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  431 |  |
|      - |  432 | `	double r, x;` |
|      - |  433 | `	/* PHP enforces exactly one argument and a floatable parameter. */` |
|     24 |  434 | `	if( nArg != 1 ){` |
|      4 |  435 | `		return PH7_VmThrowException(pCtx,` |
|      - |  436 | `			"ArgumentCountError",` |
|      - |  437 | `			"acos() expects exactly 1 argument, %d given",` |
|      1 |  438 | `			nArg` |
|      - |  439 | `			);` |
|      - |  440 | `	}` |
|      - |  441 | `	/* Type checking: reject non-numeric values (arrays, objects, resources, strings)` |
|      - |  442 | `	 * PHP8 reports a TypeError for wrong types.  Numeric strings are allowed but` |
|      - |  443 | `	 * the float conversion will handle them. */` |
|     22 |  444 | `	if( !ph7_value_is_numeric(apArg[0]) ){` |
|      7 |  445 | `		return PH7_VmThrowException(pCtx,` |
|      - |  446 | `			"TypeError",` |
|      - |  447 | `			"acos(): Argument #1 ($num) must be of type float, %s given",` |
|      2 |  448 | `			ph7_type_name(apArg[0])` |
|      - |  449 | `			);` |
|      - |  450 | `	}` |
|      - |  451 | `	/* Convert to double now that we know it's numeric. */` |
|     17 |  452 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  453 | `	/* Handle domain error ourselves.  PHP returns NaN for \|x\|>1. */` |
|     17 |  454 | `	if( x < -1.0 \|\| x > 1.0 ){` |
|      5 |  455 | `		r = PH7_NAN_VALUE();` |
|      3 |  456 | `	}else{` |
|     13 |  457 | `		r = acos(x);` |
|      - |  458 | `	}` |
|      - |  459 | `	/* store the result back */` |
|     17 |  460 | `	ph7_result_double(pCtx,r);` |
|     17 |  461 | `	return PH7_OK;` |
|     13 |  462 |  |
|      - |  463 | `/*` |
|      - |  464 | ` * float cosh(float $arg )` |
|      - |  465 | ` *  Hyperbolic cosine.` |
|      - |  466 | ` * Parameter` |
|      - |  467 | ` *  The number to process.` |
|      - |  468 | ` * Return` |
|      - |  469 | ` *  The hyperbolic cosine of arg.` |
|      - |  470 | ` */` |
|     18 |  471 | `static int PH7_builtin_cosh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  472 |  |
|      - |  473 | `	double r,x;` |
|     19 |  474 | `	if( nArg < 1 ){` |
|      - |  475 | `		/* Missing argument,return 0 */` |
|      3 |  476 | `		ph7_result_int(pCtx,0);` |
|      3 |  477 | `		return PH7_OK;` |
|      - |  478 | `	}` |
|     17 |  479 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  480 | `	/* Perform the requested operation */` |
|     17 |  481 | `	r = cosh(x);` |
|      - |  482 | `	/* store the result back */` |
|     17 |  483 | `	ph7_result_double(pCtx,r);` |
|     17 |  484 | `	return PH7_OK;` |
|     10 |  485 |  |
|      - |  486 | `/*` |
|      - |  487 | ` * float sin(float $arg )` |
|      - |  488 | ` *  Sine.` |
|      - |  489 | ` * Parameter` |
|      - |  490 | ` *  The number to process.` |
|      - |  491 | ` * Return` |
|      - |  492 | ` *  The sine of arg.` |
|      - |  493 | ` */` |
|      8 |  494 | `static int PH7_builtin_sin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  495 |  |
|      - |  496 | `	double r,x;` |
|      9 |  497 | `	if( nArg < 1 ){` |
|      - |  498 | `		/* Missing argument,return 0 */` |
|      7 |  499 | `		ph7_result_int(pCtx,0);` |
|      7 |  500 | `		return PH7_OK;` |
|      - |  501 | `	}` |
|      3 |  502 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  503 | `	/* Perform the requested operation */` |
|      3 |  504 | `	r = sin(x);` |
|      - |  505 | `	/* store the result back */` |
|      3 |  506 | `	ph7_result_double(pCtx,r);` |
|      3 |  507 | `	return PH7_OK;` |
|      5 |  508 |  |
|      - |  509 | `/*` |
|      - |  510 | ` * float asin(float $arg )` |
|      - |  511 | ` *  Arc sine.` |
|      - |  512 | ` * Parameter` |
|      - |  513 | ` *  The number to process.` |
|      - |  514 | ` * Return` |
|      - |  515 | ` *  The arc sine of arg.` |
|      - |  516 | ` */` |
|     14 |  517 | `static int PH7_builtin_asin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  518 |  |
|      - |  519 | `	double r,x;` |
|     15 |  520 | `	if( nArg < 1 ){` |
|      - |  521 | `		/* Missing argument,return 0 */` |
|      3 |  522 | `		ph7_result_int(pCtx,0);` |
|      3 |  523 | `		return PH7_OK;` |
|      - |  524 | `	}` |
|     13 |  525 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  526 | `	/* Perform the requested operation */` |
|     13 |  527 | `	r = asin(x);` |
|      - |  528 | `	/* store the result back */` |
|     13 |  529 | `	ph7_result_double(pCtx,r);` |
|     13 |  530 | `	return PH7_OK;` |
|      8 |  531 |  |
|      - |  532 | `/*` |
|      - |  533 | ` * float sinh(float $arg )` |
|      - |  534 | ` *  Hyperbolic sine.` |
|      - |  535 | ` * Parameter` |
|      - |  536 | ` *  The number to process.` |
|      - |  537 | ` * Return` |
|      - |  538 | ` *  The hyperbolic sine of arg.` |
|      - |  539 | ` */` |
|     20 |  540 | `static int PH7_builtin_sinh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  541 |  |
|      - |  542 | `	double r,x;` |
|     21 |  543 | `	if( nArg < 1 ){` |
|      - |  544 | `		/* Missing argument,return 0 */` |
|      3 |  545 | `		ph7_result_int(pCtx,0);` |
|      3 |  546 | `		return PH7_OK;` |
|      - |  547 | `	}` |
|     19 |  548 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  549 | `	/* Perform the requested operation */` |
|     19 |  550 | `	r = sinh(x);` |
|      - |  551 | `	/* store the result back */` |
|     19 |  552 | `	ph7_result_double(pCtx,r);` |
|     19 |  553 | `	return PH7_OK;` |
|     11 |  554 |  |
|      - |  555 | `/*` |
|      - |  556 | ` * float ceil(float $arg )` |
|      - |  557 | ` *  Round fractions up.` |
|      - |  558 | ` * Parameter` |
|      - |  559 | ` *  The number to process.` |
|      - |  560 | ` * Return` |
|      - |  561 | ` *  The next highest integer value by rounding up value if necessary.` |
|      - |  562 | ` */` |
|     14 |  563 | `static int PH7_builtin_ceil(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  564 |  |
|      - |  565 | `	double r,x;` |
|      - |  566 | `	/* PHP requires exactly one argument. */` |
|     16 |  567 | `	if( nArg != 1 ){` |
|      7 |  568 | `		return PH7_VmThrowException(pCtx,` |
|      - |  569 | `			"ArgumentCountError",` |
|      - |  570 | `			"ceil() expects exactly 1 argument, %d given",` |
|      2 |  571 | `			nArg` |
|      - |  572 | `			);` |
|      - |  573 | `	}` |
|      - |  574 | `	/*` |
|      - |  575 | `	 * PHP only accepts ints, floats or numeric strings.  Any other types` |
|      - |  576 | `	 * (in particular non-numeric strings) should raise a TypeError.  We` |
|      - |  577 | `	 * mimic the approach used by abs() and perform an explicit numeric` |
|      - |  578 | `	 * check on strings before converting to double.` |
|      - |  579 | `	 */` |
|     12 |  580 | `	if( !ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]) ){` |
|      6 |  581 | `		if( ph7_value_is_string(apArg[0]) ){` |
|      - |  582 | `			int len;` |
|      6 |  583 | `			sxu8 bReal = FALSE;` |
|      6 |  584 | `			const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|      - |  585 | `			sxi32 rcNum;` |
|      6 |  586 | `			rcNum = SyStrIsNumeric(zStr, len, &bReal, 0);` |
|      6 |  587 | `			if( rcNum != SXRET_OK ){` |
|      3 |  588 | `				return PH7_VmThrowException(pCtx,` |
|      - |  589 | `					"TypeError",` |
|      - |  590 | `					"ceil(): Argument #1 ($num) must be of type int\|float, string given"` |
|      - |  591 | `					);` |
|      - |  592 | `			}` |
|      2 |  593 | `		}else{` |
|      - |  594 | `			/* Reject arrays, objects, resources, booleans, NULL, etc. */` |
|    ! 0 |  595 | `			return PH7_VmThrowException(pCtx,` |
|      - |  596 | `				"TypeError",` |
|      - |  597 | `				"ceil(): Argument #1 ($num) must be of type int\|float"` |
|      - |  598 | `				);` |
|      - |  599 | `		}` |
|      1 |  600 | `	}` |
|      - |  601 |  |
|      9 |  602 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  603 | `	/* Perform the requested operation */` |
|      9 |  604 | `	r = ceil(x);` |
|      - |  605 | `	/* store the result back */` |
|      9 |  606 | `	ph7_result_double(pCtx,r);` |
|      9 |  607 | `	return PH7_OK;` |
|      9 |  608 |  |
|      - |  609 | `/*` |
|      - |  610 | ` * float tan(float $arg )` |
|      - |  611 | ` *  Tangent.` |
|      - |  612 | ` * Parameter` |
|      - |  613 | ` *  The number to process.` |
|      - |  614 | ` * Return` |
|      - |  615 | ` *  The tangent of arg.` |
|      - |  616 | ` */` |
|      6 |  617 | `static int PH7_builtin_tan(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  618 |  |
|      - |  619 | `	double r,x;` |
|      7 |  620 | `	if( nArg < 1 ){` |
|      - |  621 | `		/* Missing argument,return 0 */` |
|      3 |  622 | `		ph7_result_int(pCtx,0);` |
|      3 |  623 | `		return PH7_OK;` |
|      - |  624 | `	}` |
|      5 |  625 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  626 | `	/* Perform the requested operation */` |
|      5 |  627 | `	r = tan(x);` |
|      - |  628 | `	/* store the result back */` |
|      5 |  629 | `	ph7_result_double(pCtx,r);` |
|      5 |  630 | `	return PH7_OK;` |
|      4 |  631 |  |
|      - |  632 | `/*` |
|      - |  633 | ` * float atan(float $arg )` |
|      - |  634 | ` *  Arc tangent.` |
|      - |  635 | ` * Parameter` |
|      - |  636 | ` *  The number to process.` |
|      - |  637 | ` * Return` |
|      - |  638 | ` *  The arc tangent of arg.` |
|      - |  639 | ` */` |
|     16 |  640 | `static int PH7_builtin_atan(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  641 |  |
|      - |  642 | `	double r,x;` |
|     17 |  643 | `	if( nArg < 1 ){` |
|      - |  644 | `		/* Missing argument,return 0 */` |
|      5 |  645 | `		ph7_result_int(pCtx,0);` |
|      5 |  646 | `		return PH7_OK;` |
|      - |  647 | `	}` |
|     13 |  648 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  649 | `	/* Perform the requested operation */` |
|     13 |  650 | `	r = atan(x);` |
|      - |  651 | `	/* store the result back */` |
|     13 |  652 | `	ph7_result_double(pCtx,r);` |
|     13 |  653 | `	return PH7_OK;` |
|      9 |  654 |  |
|      - |  655 | `/*` |
|      - |  656 | ` * float tanh(float $arg )` |
|      - |  657 | ` *  Hyperbolic tangent.` |
|      - |  658 | ` * Parameter` |
|      - |  659 | ` *  The number to process.` |
|      - |  660 | ` * Return` |
|      - |  661 | ` *  The Hyperbolic tangent of arg.` |
|      - |  662 | ` */` |
|     20 |  663 | `static int PH7_builtin_tanh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  664 |  |
|      - |  665 | `	double r,x;` |
|     21 |  666 | `	if( nArg < 1 ){` |
|      - |  667 | `		/* Missing argument,return 0 */` |
|      3 |  668 | `		ph7_result_int(pCtx,0);` |
|      3 |  669 | `		return PH7_OK;` |
|      - |  670 | `	}` |
|     19 |  671 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  672 | `	/* Perform the requested operation */` |
|     19 |  673 | `	r = tanh(x);` |
|      - |  674 | `	/* store the result back */` |
|     19 |  675 | `	ph7_result_double(pCtx,r);` |
|     19 |  676 | `	return PH7_OK;` |
|     11 |  677 |  |
|      - |  678 | `/*` |
|      - |  679 | ` * float atan2(float $y,float $x)` |
|      - |  680 | ` *  Arc tangent of two variable.` |
|      - |  681 | ` * Parameter` |
|      - |  682 | ` *  $y = Dividend parameter.` |
|      - |  683 | ` *  $x = Divisor parameter.` |
|      - |  684 | ` * Return` |
|      - |  685 | ` *  The arc tangent of y/x in radian.` |
|      - |  686 | ` */` |
|     10 |  687 | `static int PH7_builtin_atan2(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  688 |  |
|      - |  689 | `	double r,x,y;` |
|     11 |  690 | `	if( nArg < 2 ){` |
|      - |  691 | `		/* Missing arguments,return 0 */` |
|      5 |  692 | `		ph7_result_int(pCtx,0);` |
|      5 |  693 | `		return PH7_OK;` |
|      - |  694 | `	}` |
|      7 |  695 | `	y = ph7_value_to_double(apArg[0]);` |
|      7 |  696 | `	x = ph7_value_to_double(apArg[1]);` |
|      - |  697 | `	/* Perform the requested operation */` |
|      7 |  698 | `	r = atan2(y,x);` |
|      - |  699 | `	/* store the result back */` |
|      7 |  700 | `	ph7_result_double(pCtx,r);` |
|      7 |  701 | `	return PH7_OK;` |
|      6 |  702 |  |
|      - |  703 | `/*` |
|      - |  704 | ` * float/int64 abs(float/int64 $arg )` |
|      - |  705 | ` *  Absolute value.` |
|      - |  706 | ` * Parameter` |
|      - |  707 | ` *  The number to process.` |
|      - |  708 | ` * Return` |
|      - |  709 | ` *  The absolute value of number.` |
|      - |  710 | ` */` |
|    122 |  711 | `static int PH7_builtin_abs(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  712 |  |
|      - |  713 | `	int is_float;` |
|      - |  714 | `	/* PHP requires exactly one argument. */` |
|    124 |  715 | `	if( nArg != 1 ){` |
|     11 |  716 | `		return PH7_VmThrowException(pCtx,` |
|      - |  717 | `			"ArgumentCountError",` |
|      - |  718 | `			"abs() expects exactly 1 argument, %d given",` |
|      3 |  719 | `			nArg` |
|      - |  720 | `			);` |
|      - |  721 | `	}` |
|      - |  722 |  |
|      - |  723 | `	/* Numeric strings with decimal/exponent are treated as real values. */` |
|    118 |  724 | `	is_float = ph7_value_is_float(apArg[0]);` |
|    118 |  725 | `	if( !is_float && ph7_value_is_string(apArg[0]) ){` |
|      - |  726 | `		int len;` |
|     10 |  727 | `		sxu8 bReal = FALSE;` |
|     10 |  728 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|      - |  729 | `		sxi32 rcNum;` |
|     10 |  730 | `		rcNum = SyStrIsNumeric(zStr, len, &bReal, 0);` |
|     10 |  731 | `		if( rcNum != SXRET_OK ){` |
|      3 |  732 | `			return PH7_VmThrowException(pCtx,` |
|      - |  733 | `				"TypeError",` |
|      - |  734 | `				"abs(): Argument #1 ($num) must be of type int\|float, string given"` |
|      - |  735 | `				);` |
|      - |  736 | `		}` |
|      7 |  737 | `		if( bReal ){` |
|      5 |  738 | `			is_float = 1;` |
|      2 |  739 | `		}` |
|      3 |  740 | `	}` |
|    116 |  741 | `	if( is_float ){` |
|      - |  742 | `		double r,x;` |
|     99 |  743 | `		x = ph7_value_to_double(apArg[0]);` |
|      - |  744 | `		/* Perform the requested operation */` |
|     99 |  745 | `		r = fabs(x);` |
|     99 |  746 | `		ph7_result_double(pCtx,r);` |
|     50 |  747 | `	}else{` |
|      - |  748 | `		int r,x;` |
|     18 |  749 | `		x = ph7_value_to_int(apArg[0]);` |
|      - |  750 | `		/* Perform the requested operation */` |
|     18 |  751 | `		r = abs(x);` |
|     18 |  752 | `		ph7_result_int(pCtx,r);` |
|      - |  753 | `	}` |
|    116 |  754 | `	return PH7_OK;` |
|     63 |  755 |  |
|      - |  756 | `/*` |
|      - |  757 | ` * float log(float $arg,[int/float $base])` |
|      - |  758 | ` *  Natural logarithm.` |
|      - |  759 | ` * Parameter` |
|      - |  760 | ` *  $arg: The number to process.` |
|      - |  761 | ` *  $base: The optional logarithmic base to use. (only base-10 is supported)` |
|      - |  762 | ` * Return` |
|      - |  763 | ` *  The logarithm of arg to base, if given, or the natural logarithm.` |
|      - |  764 | ` * Note:` |
|      - |  765 | ` *  only Natural log and base-10 log are supported.` |
|      - |  766 | ` */` |
|     14 |  767 | `static int PH7_builtin_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  768 |  |
|      - |  769 | `	double r,x;` |
|     15 |  770 | `	if( nArg < 1 ){` |
|      - |  771 | `		/* Missing argument,return 0 */` |
|      3 |  772 | `		ph7_result_int(pCtx,0);` |
|      3 |  773 | `		return PH7_OK;` |
|      - |  774 | `	}` |
|     13 |  775 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  776 | `	/* Perform the requested operation */` |
|     13 |  777 | `	if( nArg == 2 && ph7_value_is_numeric(apArg[1]) && ph7_value_to_int(apArg[1]) == 10 ){` |
|      - |  778 | `		/* Base-10 log */` |
|      5 |  779 | `		r = log10(x);` |
|      3 |  780 | `	}else{` |
|      9 |  781 | `		r = log(x);` |
|      - |  782 | `	}` |
|      - |  783 | `	/* store the result back */` |
|     13 |  784 | `	ph7_result_double(pCtx,r);` |
|     13 |  785 | `	return PH7_OK;` |
|      8 |  786 |  |
|      - |  787 | `/*` |
|      - |  788 | ` * float log10(float $arg )` |
|      - |  789 | ` *  Base-10 logarithm.` |
|      - |  790 | ` * Parameter` |
|      - |  791 | ` *  The number to process.` |
|      - |  792 | ` * Return` |
|      - |  793 | ` *  The Base-10 logarithm of the given number.` |
|      - |  794 | ` */` |
|     16 |  795 | `static int PH7_builtin_log10(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  796 |  |
|      - |  797 | `	double r,x;` |
|     17 |  798 | `	if( nArg < 1 ){` |
|      - |  799 | `		/* Missing argument,return 0 */` |
|      3 |  800 | `		ph7_result_int(pCtx,0);` |
|      3 |  801 | `		return PH7_OK;` |
|      - |  802 | `	}` |
|     15 |  803 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  804 | `	/* Perform the requested operation */` |
|     15 |  805 | `	r = log10(x);` |
|      - |  806 | `	/* store the result back */` |
|     15 |  807 | `	ph7_result_double(pCtx,r);` |
|     15 |  808 | `	return PH7_OK;` |
|      9 |  809 |  |
|      - |  810 | `/*` |
|      - |  811 | ` * number pow(number $base,number $exp)` |
|      - |  812 | ` *  Exponential expression.` |
|      - |  813 | ` * Parameter` |
|      - |  814 | ` *  base` |
|      - |  815 | ` *  The base to use.` |
|      - |  816 | ` * exp` |
|      - |  817 | ` *  The exponent.` |
|      - |  818 | ` * Return` |
|      - |  819 | ` *  base raised to the power of exp.` |
|      - |  820 | ` *  If the result can be represented as integer it will be returned` |
|      - |  821 | ` *  as type integer, else it will be returned as type float.` |
|      - |  822 | ` */` |
|      8 |  823 | `static int PH7_builtin_pow(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  824 |  |
|      - |  825 | `	double r,x,y;` |
|      9 |  826 | `	if( nArg < 1 ){` |
|      - |  827 | `		/* Missing argument,return 0 */` |
|      5 |  828 | `		ph7_result_int(pCtx,0);` |
|      5 |  829 | `		return PH7_OK;` |
|      - |  830 | `	}` |
|      5 |  831 | `	x = ph7_value_to_double(apArg[0]);` |
|      5 |  832 | `	y = ph7_value_to_double(apArg[1]);` |
|      - |  833 | `	/* Perform the requested operation */` |
|      5 |  834 | `	r = pow(x,y);` |
|      5 |  835 | `	ph7_result_double(pCtx,r);` |
|      5 |  836 | `	return PH7_OK;` |
|      5 |  837 |  |
|      - |  838 | `/*` |
|      - |  839 | ` * float pi(void)` |
|      - |  840 | ` *  Returns an approximation of pi.` |
|      - |  841 | ` * Note` |
|      - |  842 | ` *  you can use the M_PI constant which yields identical results to pi().` |
|      - |  843 | ` * Return` |
|      - |  844 | ` *  The value of pi as float.` |
|      - |  845 | ` */` |
|      2 |  846 | `static int PH7_builtin_pi(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  847 |  |
|      1 |  848 | `	SXUNUSED(nArg); /* cc warning */` |
|      1 |  849 | `	SXUNUSED(apArg);` |
|      3 |  850 | `	ph7_result_double(pCtx,PH7_PI);` |
|      3 |  851 | `	return PH7_OK;` |
|      1 |  852 |  |
|      - |  853 | `/*` |
|      - |  854 | ` * float fmod(float $x,float $y)` |
|      - |  855 | ` *  Returns the floating point remainder (modulo) of the division of the arguments.` |
|      - |  856 | ` * Parameters` |
|      - |  857 | ` * $x` |
|      - |  858 | ` *  The dividend` |
|      - |  859 | ` * $y` |
|      - |  860 | ` *  The divisor` |
|      - |  861 | ` * Return` |
|      - |  862 | ` *  The floating point remainder of x/y.` |
|      - |  863 | ` */` |
|      8 |  864 | `static int PH7_builtin_fmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  865 |  |
|      - |  866 | `	double x,y,r;` |
|      9 |  867 | `	if( nArg < 2 ){` |
|      - |  868 | `		/* Missing arguments */` |
|      7 |  869 | `		ph7_result_double(pCtx,0);` |
|      7 |  870 | `		return PH7_OK;` |
|      - |  871 | `	}` |
|      - |  872 | `	/* Extract given arguments */` |
|      3 |  873 | `	x = ph7_value_to_double(apArg[0]);` |
|      3 |  874 | `	y = ph7_value_to_double(apArg[1]);` |
|      - |  875 | `	/* Perform the requested operation */` |
|      3 |  876 | `	r = fmod(x,y);` |
|      - |  877 | `	/* Processing result */` |
|      3 |  878 | `	ph7_result_double(pCtx,r);` |
|      3 |  879 | `	return PH7_OK;` |
|      5 |  880 |  |
|      - |  881 | `/*` |
|      - |  882 | ` * float hypot(float $x,float $y)` |
|      - |  883 | ` *  Calculate the length of the hypotenuse of a right-angle triangle .` |
|      - |  884 | ` * Parameters` |
|      - |  885 | ` * $x` |
|      - |  886 | ` *  Length of first side` |
|      - |  887 | ` * $y` |
|      - |  888 | ` *  Length of first side` |
|      - |  889 | ` * Return` |
|      - |  890 | ` *  Calculated length of the hypotenuse.` |
|      - |  891 | ` */` |
|      6 |  892 | `static int PH7_builtin_hypot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  893 |  |
|      - |  894 | `	double x,y,r;` |
|      7 |  895 | `	if( nArg < 2 ){` |
|      - |  896 | `		/* Missing arguments */` |
|      5 |  897 | `		ph7_result_double(pCtx,0);` |
|      5 |  898 | `		return PH7_OK;` |
|      - |  899 | `	}` |
|      - |  900 | `	/* Extract given arguments */` |
|      3 |  901 | `	x = ph7_value_to_double(apArg[0]);` |
|      3 |  902 | `	y = ph7_value_to_double(apArg[1]);` |
|      - |  903 | `	/* Perform the requested operation */` |
|      3 |  904 | `	r = hypot(x,y);` |
|      - |  905 | `	/* Processing result */` |
|      3 |  906 | `	ph7_result_double(pCtx,r);` |
|      3 |  907 | `	return PH7_OK;` |
|      4 |  908 |  |
|      - |  909 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - |  910 | `/*` |
|      - |  911 | ` * float round ( float $val [, int $precision = 0 [, int $mode = PHP_ROUND_HALF_UP ]] )` |
|      - |  912 | ` *  Exponential expression.` |
|      - |  913 | ` * Parameter` |
|      - |  914 | ` *  $val` |
|      - |  915 | ` *   The value to round.` |
|      - |  916 | ` * $precision` |
|      - |  917 | ` *   The optional number of decimal digits to round to.` |
|      - |  918 | ` * $mode` |
|      - |  919 | ` *   One of PHP_ROUND_HALF_UP, PHP_ROUND_HALF_DOWN, PHP_ROUND_HALF_EVEN, or PHP_ROUND_HALF_ODD.` |
|      - |  920 | ` *   (not supported).` |
|      - |  921 | ` * Return` |
|      - |  922 | ` *  The rounded value.` |
|      - |  923 | ` */` |
|     20 |  924 | `static int PH7_builtin_round(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  925 |  |
|     21 |  926 | `	int n = 0;` |
|      - |  927 | `	double r;` |
|     21 |  928 | `	if( nArg < 1 ){` |
|      - |  929 | `		/* Missing argument,return 0 */` |
|      5 |  930 | `		ph7_result_int(pCtx,0);` |
|      5 |  931 | `		return PH7_OK;` |
|      - |  932 | `	}` |
|      - |  933 | `	/* Extract the precision if available */` |
|     17 |  934 | `	if( nArg > 1 ){` |
|      5 |  935 | `		n = ph7_value_to_int(apArg[1]);` |
|      5 |  936 | `		if( n>30 ){` |
|      3 |  937 | `			n = 30;` |
|      1 |  938 | `		}` |
|      5 |  939 | `		if( n<0 ){` |
|      3 |  940 | `			n = 0;` |
|      1 |  941 | `		}` |
|      2 |  942 | `	}` |
|     17 |  943 | `	r = ph7_value_to_double(apArg[0]);` |
|      - |  944 | `	/* If Y==0 and X will fit in a 64-bit int,` |
|      - |  945 | `     * handle the rounding directly.Otherwise` |
|      - |  946 | `	 * use our own cutsom printf [i.e:SyBufferFormat()].` |
|      - |  947 | `     */` |
|     17 |  948 | `	if( n==0 && r>=0 && r < (double)(LARGEST_INT64-1) ){` |
|     13 |  949 | `    r = (double)((ph7_int64)(r+0.5));` |
|     11 |  950 | `	}else if( n==0 && r<0 && (-r) < (double)(LARGEST_INT64-1) ){` |
|      3 |  951 | `    r = -(double)((ph7_int64)((-r)+0.5));` |
|      2 |  952 | `  }else{` |
|      - |  953 | `	  char zBuf[256];` |
|      - |  954 | `	  sxu32 nLen;` |
|      3 |  955 | `	  nLen = SyBufferFormat(zBuf,sizeof(zBuf),"%.*f",n,r);` |
|      - |  956 | `	  /* Convert the string to real number */` |
|      3 |  957 | `	  SyStrToReal(zBuf,nLen,(void *)&r,0);` |
|      - |  958 | `  }` |
|      - |  959 | `  /* Return thr rounded value */` |
|     17 |  960 | `  ph7_result_double(pCtx,r);` |
|     17 |  961 | `  return PH7_OK;` |
|     11 |  962 |  |
|      - |  963 | `/*` |
|      - |  964 | ` * string dechex(int $number)` |
|      - |  965 | ` *  Decimal to hexadecimal.` |
|      - |  966 | ` * Parameters` |
|      - |  967 | ` *  $number` |
|      - |  968 | ` *   Decimal value to convert` |
|      - |  969 | ` * Return` |
|      - |  970 | ` *  Hexadecimal string representation of number` |
|      - |  971 | ` */` |
|      6 |  972 | `static int PH7_builtin_dechex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  973 |  |
|      - |  974 | `	int iVal;` |
|      7 |  975 | `	if( nArg < 1 ){` |
|      - |  976 | `		/* Missing arguments,return null */` |
|      5 |  977 | `		ph7_result_null(pCtx);` |
|      5 |  978 | `		return PH7_OK;` |
|      - |  979 | `	}` |
|      - |  980 | `	/* Extract the given number */` |
|      3 |  981 | `	iVal = ph7_value_to_int(apArg[0]);` |
|      - |  982 | `	/* Format */` |
|      3 |  983 | `	ph7_result_string_format(pCtx,"%x",iVal);` |
|      3 |  984 | `	return PH7_OK;` |
|      4 |  985 |  |
|      - |  986 | `/*` |
|      - |  987 | ` * string decoct(int $number)` |
|      - |  988 | ` *  Decimal to Octal.` |
|      - |  989 | ` * Parameters` |
|      - |  990 | ` *  $number` |
|      - |  991 | ` *   Decimal value to convert` |
|      - |  992 | ` * Return` |
|      - |  993 | ` *  Octal string representation of number` |
|      - |  994 | ` */` |
|      8 |  995 | `static int PH7_builtin_decoct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  996 |  |
|      - |  997 | `	int iVal;` |
|      9 |  998 | `	if( nArg < 1 ){` |
|      - |  999 | `		/* Missing arguments,return null */` |
|      3 | 1000 | `		ph7_result_null(pCtx);` |
|      3 | 1001 | `		return PH7_OK;` |
|      - | 1002 | `	}` |
|      - | 1003 | `	/* Extract the given number */` |
|      7 | 1004 | `	iVal = ph7_value_to_int(apArg[0]);` |
|      - | 1005 | `	/* Format */` |
|      7 | 1006 | `	ph7_result_string_format(pCtx,"%o",iVal);` |
|      7 | 1007 | `	return PH7_OK;` |
|      5 | 1008 |  |
|      - | 1009 | `/*` |
|      - | 1010 | ` * string decbin(int $number)` |
|      - | 1011 | ` *  Decimal to binary.` |
|      - | 1012 | ` * Parameters` |
|      - | 1013 | ` *  $number` |
|      - | 1014 | ` *   Decimal value to convert` |
|      - | 1015 | ` * Return` |
|      - | 1016 | ` *  Binary string representation of number` |
|      - | 1017 | ` */` |
|      4 | 1018 | `static int PH7_builtin_decbin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1019 |  |
|      - | 1020 | `	int iVal;` |
|      5 | 1021 | `	if( nArg < 1 ){` |
|      - | 1022 | `		/* Missing arguments,return null */` |
|      3 | 1023 | `		ph7_result_null(pCtx);` |
|      3 | 1024 | `		return PH7_OK;` |
|      - | 1025 | `	}` |
|      - | 1026 | `	/* Extract the given number */` |
|      3 | 1027 | `	iVal = ph7_value_to_int(apArg[0]);` |
|      - | 1028 | `	/* Format */` |
|      3 | 1029 | `	ph7_result_string_format(pCtx,"%B",iVal);` |
|      3 | 1030 | `	return PH7_OK;` |
|      3 | 1031 |  |
|      - | 1032 | `/*` |
|      - | 1033 | ` * int64 hexdec(string $hex_string)` |
|      - | 1034 | ` *  Hexadecimal to decimal.` |
|      - | 1035 | ` * Parameters` |
|      - | 1036 | ` *  $hex_string` |
|      - | 1037 | ` *   The hexadecimal string to convert` |
|      - | 1038 | ` * Return` |
|      - | 1039 | ` *  The decimal representation of hex_string` |
|      - | 1040 | ` */` |
|     24 | 1041 | `static int PH7_builtin_hexdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1042 |  |
|      - | 1043 | `	const char *zString,*zEnd;` |
|      - | 1044 | `	ph7_int64 iVal;` |
|      - | 1045 | `	int nLen;` |
|     25 | 1046 | `	if( nArg < 1 ){` |
|      - | 1047 | `		/* Missing arguments,return -1 */` |
|      5 | 1048 | `		ph7_result_int(pCtx,-1);` |
|      5 | 1049 | `		return PH7_OK;` |
|      - | 1050 | `	}` |
|     21 | 1051 | `	iVal = 0;` |
|     21 | 1052 | `	if( ph7_value_is_string(apArg[0]) ){` |
|      - | 1053 | `		/* Extract the given string */` |
|     15 | 1054 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1055 | `		/* Delimit the string */` |
|     15 | 1056 | `		zEnd = &zString[nLen];` |
|      - | 1057 | `		/* Ignore non hex-stream */` |
|     21 | 1058 | `		while( zString < zEnd ){` |
|     21 | 1059 | `			if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 1060 | `				/* UTF-8 stream */` |
|      5 | 1061 | `				zString++;` |
|      9 | 1062 | `				while( zString < zEnd && (((unsigned char)zString[0] & 0xc0) == 0x80) ){` |
|      5 | 1063 | `					zString++;` |
|      1 | 1064 | `				}` |
|      3 | 1065 | `			}else{` |
|     17 | 1066 | `				if( SyisHex(zString[0]) ){` |
|     15 | 1067 | `					break;` |
|      - | 1068 | `				}` |
|      - | 1069 | `				/* Ignore */` |
|      3 | 1070 | `				zString++;` |
|      - | 1071 | `			}` |
|      1 | 1072 | `		}` |
|     15 | 1073 | `		if( zString < zEnd ){` |
|      - | 1074 | `			/* Cast */` |
|     15 | 1075 | `			SyHexStrToInt64(zString,(sxu32)(zEnd-zString),(void *)&iVal,0);` |
|      7 | 1076 | `		}` |
|      8 | 1077 | `	}else{` |
|      - | 1078 | `		/* Extract as a 64-bit integer */` |
|      7 | 1079 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|      - | 1080 | `	}` |
|      - | 1081 | `	/* Return the number */` |
|     21 | 1082 | `	ph7_result_int64(pCtx,iVal);` |
|     21 | 1083 | `	return PH7_OK;` |
|     13 | 1084 |  |
|      - | 1085 | `/*` |
|      - | 1086 | ` * int64 bindec(string $bin_string)` |
|      - | 1087 | ` *  Binary to decimal.` |
|      - | 1088 | ` * Parameters` |
|      - | 1089 | ` *  $bin_string` |
|      - | 1090 | ` *   The binary string to convert` |
|      - | 1091 | ` * Return` |
|      - | 1092 | ` *  Returns the decimal equivalent of the binary number represented by the binary_string argument.` |
|      - | 1093 | ` */` |
|     12 | 1094 | `static int PH7_builtin_bindec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1095 |  |
|      - | 1096 | `	const char *zString;` |
|      - | 1097 | `	ph7_int64 iVal;` |
|      - | 1098 | `	int nLen;` |
|     13 | 1099 | `	if( nArg < 1 ){` |
|      - | 1100 | `		/* Missing arguments,return -1 */` |
|      5 | 1101 | `		ph7_result_int(pCtx,-1);` |
|      5 | 1102 | `		return PH7_OK;` |
|      - | 1103 | `	}` |
|      9 | 1104 | `	iVal = 0;` |
|      9 | 1105 | `	if( ph7_value_is_string(apArg[0]) ){` |
|      - | 1106 | `		/* Extract the given string */` |
|      7 | 1107 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 1108 | `		if( nLen > 0 ){` |
|      - | 1109 | `			/* Perform a binary cast */` |
|      5 | 1110 | `			SyBinaryStrToInt64(zString,(sxu32)nLen,(void *)&iVal,0);` |
|      2 | 1111 | `		}` |
|      4 | 1112 | `	}else{` |
|      - | 1113 | `		/* Extract as a 64-bit integer */` |
|      3 | 1114 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|      - | 1115 | `	}` |
|      - | 1116 | `	/* Return the number */` |
|      9 | 1117 | `	ph7_result_int64(pCtx,iVal);` |
|      9 | 1118 | `	return PH7_OK;` |
|      7 | 1119 |  |
|      - | 1120 | `/*` |
|      - | 1121 | ` * int64 octdec(string $oct_string)` |
|      - | 1122 | ` *  Octal to decimal.` |
|      - | 1123 | ` * Parameters` |
|      - | 1124 | ` *  $oct_string` |
|      - | 1125 | ` *   The octal string to convert` |
|      - | 1126 | ` * Return` |
|      - | 1127 | ` *  Returns the decimal equivalent of the octal number represented by the octal_string argument.` |
|      - | 1128 | ` */` |
|      6 | 1129 | `static int PH7_builtin_octdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1130 |  |
|      - | 1131 | `	const char *zString;` |
|      - | 1132 | `	ph7_int64 iVal;` |
|      - | 1133 | `	int nLen;` |
|      7 | 1134 | `	if( nArg < 1 ){` |
|      - | 1135 | `		/* Missing arguments,return -1 */` |
|      3 | 1136 | `		ph7_result_int(pCtx,-1);` |
|      3 | 1137 | `		return PH7_OK;` |
|      - | 1138 | `	}` |
|      5 | 1139 | `	iVal = 0;` |
|      5 | 1140 | `	if( ph7_value_is_string(apArg[0]) ){` |
|      - | 1141 | `		/* Extract the given string */` |
|      3 | 1142 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 1143 | `		if( nLen > 0 ){` |
|      - | 1144 | `			/* Perform the cast */` |
|      3 | 1145 | `			SyOctalStrToInt64(zString,(sxu32)nLen,(void *)&iVal,0);` |
|      1 | 1146 | `		}` |
|      2 | 1147 | `	}else{` |
|      - | 1148 | `		/* Extract as a 64-bit integer */` |
|      3 | 1149 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|      - | 1150 | `	}` |
|      - | 1151 | `	/* Return the number */` |
|      5 | 1152 | `	ph7_result_int64(pCtx,iVal);` |
|      5 | 1153 | `	return PH7_OK;` |
|      4 | 1154 |  |
|      - | 1155 | `/*` |
|      - | 1156 | ` * srand([int $seed])` |
|      - | 1157 | ` * mt_srand([int $seed])` |
|      - | 1158 | ` *  Seed the random number generator.` |
|      - | 1159 | ` * Parameters` |
|      - | 1160 | ` * $seed` |
|      - | 1161 | ` *  Optional seed value` |
|      - | 1162 | ` * Return` |
|      - | 1163 | ` *  null.` |
|      - | 1164 | ` * Note:` |
|      - | 1165 | ` *  THIS FUNCTION IS A NO-OP.` |
|      - | 1166 | ` *  THE PH7 PRNG IS AUTOMATICALLY SEEDED WHEN THE VM IS CREATED.` |
|      - | 1167 | ` */` |
|     20 | 1168 | `static int PH7_builtin_srand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1169 |  |
|     10 | 1170 | `	SXUNUSED(nArg);` |
|     10 | 1171 | `	SXUNUSED(apArg);` |
|     21 | 1172 | `	ph7_result_null(pCtx);` |
|     21 | 1173 | `	return PH7_OK;` |
|      1 | 1174 |  |
|      - | 1175 | `/*` |
|      - | 1176 | ` * string base_convert(string $number,int $frombase,int $tobase)` |
|      - | 1177 | ` *  Convert a number between arbitrary bases.` |
|      - | 1178 | ` * Parameters` |
|      - | 1179 | ` * $number` |
|      - | 1180 | ` *  The number to convert` |
|      - | 1181 | ` * $frombase` |
|      - | 1182 | ` *  The base number is in` |
|      - | 1183 | ` * $tobase` |
|      - | 1184 | ` *  The base to convert number to` |
|      - | 1185 | ` * Return` |
|      - | 1186 | ` *  Number converted to base tobase` |
|      - | 1187 | ` */` |
|      - | 1188 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 1189 | `#ifdef PH7_NEED_FMT_AND_INI` |
|     48 | 1190 | `static int PH7_builtin_base_convert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      - | 1191 |  |
|      - | 1192 |  |
|      1 | 1193 |  |
|      - | 1194 | `	int nLen,iFbase,iTobase;` |
|      - | 1195 | `	const char *zNum;` |
|      - | 1196 | `	ph7_int64 iNum;` |
|     49 | 1197 | `	if( nArg < 3 ){` |
|      - | 1198 | `		/* Return the empty string*/` |
|     13 | 1199 | `		ph7_result_string(pCtx,"",0);` |
|     13 | 1200 | `		return PH7_OK;` |
|      - | 1201 | `	}` |
|      - | 1202 | `	/* Base numbers */` |
|     37 | 1203 | `	iFbase  = ph7_value_to_int(apArg[1]);` |
|     37 | 1204 | `	iTobase = ph7_value_to_int(apArg[2]);` |
|     37 | 1205 | `	if( ph7_value_is_string(apArg[0]) ){` |
|      - | 1206 | `		/* Extract the target number */` |
|     33 | 1207 | `		zNum = ph7_value_to_string(apArg[0],&nLen);` |
|     33 | 1208 | `		if( nLen < 1 ){` |
|      - | 1209 | `			/* Return the empty string*/` |
|      5 | 1210 | `			ph7_result_string(pCtx,"",0);` |
|      5 | 1211 | `			return PH7_OK;` |
|      - | 1212 | `		}` |
|      - | 1213 | `		/* Base conversion */` |
|     29 | 1214 | `		switch(iFbase){` |
|      5 | 1215 | `		case 16:` |
|      - | 1216 | `			/* Hex */` |
|     11 | 1217 | `			SyHexStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|     11 | 1218 | `			break;` |
|      3 | 1219 | `		case 8:` |
|      - | 1220 | `			/* Octal */` |
|      7 | 1221 | `			SyOctalStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|      7 | 1222 | `			break;` |
|      2 | 1223 | `		case 2:` |
|      - | 1224 | `			/* Binary */` |
|      5 | 1225 | `			SyBinaryStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|      5 | 1226 | `			break;` |
|      4 | 1227 | `		default:` |
|      - | 1228 | `			/* Decimal */` |
|      9 | 1229 | `			SyStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|      8 | 1230 | `			break;` |
|      - | 1231 | `		}` |
|     15 | 1232 | `	}else{` |
|      5 | 1233 | `		iNum = ph7_value_to_int64(apArg[0]);` |
|      - | 1234 | `	}` |
|     33 | 1235 | `	switch(iTobase){` |
|      3 | 1236 | `	case 16:` |
|      - | 1237 | `		/* Hex */` |
|      7 | 1238 | `		ph7_result_string_format(pCtx,"%qx",iNum); /* Quad hex */` |
|      7 | 1239 | `		break;` |
|      1 | 1240 | `	case 8:` |
|      - | 1241 | `		/* Octal */` |
|      3 | 1242 | `		ph7_result_string_format(pCtx,"%qo",iNum); /* Quad octal */` |
|      3 | 1243 | `		break;` |
|      1 | 1244 | `	case 2:` |
|      - | 1245 | `		/* Binary */` |
|      3 | 1246 | `		ph7_result_string_format(pCtx,"%qB",iNum); /* Quad binary */` |
|      3 | 1247 | `		break;` |
|     11 | 1248 | `	default:` |
|      - | 1249 | `		/* Decimal */` |
|     23 | 1250 | `		ph7_result_string_format(pCtx,"%qd",iNum); /* Quad decimal */` |
|     22 | 1251 | `		break;` |
|      - | 1252 | `	}` |
|     33 | 1253 | `	return PH7_OK;` |
|     25 | 1254 |  |
|      - | 1255 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 1256 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 1257 | `/*` |
|      - | 1258 | ` * Section:` |
|      - | 1259 | ` *    String handling Functions.` |
|      - | 1260 | ` * Status:` |
|      - | 1261 | ` *    Stable.` |
|      - | 1262 | ` */` |
|      - | 1263 | `/*` |
|      - | 1264 | ` * string substr(string $string,int $start[, int $length ])` |
|      - | 1265 | ` *  Return part of a string.` |
|      - | 1266 | ` * Parameters` |
|      - | 1267 | ` *  $string` |
|      - | 1268 | ` *   The input string. Must be one character or longer.` |
|      - | 1269 | ` * $start` |
|      - | 1270 | ` *   If start is non-negative, the returned string will start at the start'th position` |
|      - | 1271 | ` *   in string, counting from zero. For instance, in the string 'abcdef', the character` |
|      - | 1272 | ` *   at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 1273 | ` *   If start is negative, the returned string will start at the start'th character` |
|      - | 1274 | ` *   from the end of string.` |
|      - | 1275 | ` *   If string is less than or equal to start characters long, FALSE will be returned.` |
|      - | 1276 | ` * $length` |
|      - | 1277 | ` *   If length is given and is positive, the string returned will contain at most length` |
|      - | 1278 | ` *   characters beginning from start (depending on the length of string).` |
|      - | 1279 | ` *   If length is given and is negative, then that many characters will be omitted from` |
|      - | 1280 | ` *   the end of string (after the start position has been calculated when a start is negative).` |
|      - | 1281 | ` *   If start denotes the position of this truncation or beyond, false will be returned.` |
|      - | 1282 | ` *   If length is given and is 0, FALSE or NULL an empty string will be returned.` |
|      - | 1283 | ` *   If length is omitted, the substring starting from start until the end of the string` |
|      - | 1284 | ` *   will be returned.` |
|      - | 1285 | ` * Return` |
|      - | 1286 | ` *  Returns the extracted part of string, or FALSE on failure or an empty string.` |
|      - | 1287 | ` */` |
| 119396 | 1288 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1289 |  |
|      - | 1290 | `	const char *zSource,*zOfft;` |
|      - | 1291 | `	int nOfft,nLen,nSrcLen;` |
| 119398 | 1292 | `	if( nArg < 2 ){` |
|      - | 1293 | `		/* return FALSE */` |
|      5 | 1294 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1295 | `		return PH7_OK;` |
|      - | 1296 | `	}` |
|      - | 1297 | `	/* Extract the target string */` |
| 119394 | 1298 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 119394 | 1299 | `	if( nSrcLen < 1 ){` |
|      - | 1300 | `		/* Empty string,return FALSE */` |
|   7466 | 1301 | `		ph7_result_bool(pCtx,0);` |
|   7466 | 1302 | `		return PH7_OK;` |
|      - | 1303 | `	}` |
| 111930 | 1304 | `	nLen = nSrcLen; /* cc warning */` |
|      - | 1305 | `	/* Extract the offset */` |
| 111930 | 1306 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 111930 | 1307 | `	if( nOfft < 0 ){` |
|  18892 | 1308 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  18892 | 1309 | `		if( zOfft < zSource ){` |
|      - | 1310 | `			/* Invalid offset */` |
|      5 | 1311 | `			ph7_result_bool(pCtx,0);` |
|      5 | 1312 | `			return PH7_OK;` |
|      - | 1313 | `		}` |
|  18888 | 1314 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  18888 | 1315 | `		nOfft = (int)(zOfft-zSource);` |
| 102483 | 1316 | `	}else if( nOfft >= nSrcLen ){` |
|      - | 1317 | `		/* Invalid offset */` |
|      7 | 1318 | `		ph7_result_bool(pCtx,0);` |
|      7 | 1319 | `		return PH7_OK;` |
|    ! 0 | 1320 | `	}else{` |
|  93034 | 1321 | `		zOfft = &zSource[nOfft];` |
|  93034 | 1322 | `		nLen = nSrcLen - nOfft;` |
|      - | 1323 | `	}` |
| 111920 | 1324 | `	if( nArg > 2 ){` |
|      - | 1325 | `		/* Extract the length */` |
|  93032 | 1326 | `		nLen = ph7_value_to_int(apArg[2]);` |
|  93032 | 1327 | `		if( nLen == 0 ){` |
|      - | 1328 | `			/* Invalid length,return an empty string */` |
|      5 | 1329 | `			ph7_result_string(pCtx,"",0);` |
|      5 | 1330 | `			return PH7_OK;` |
|  93028 | 1331 | `		}else if( nLen < 0 ){` |
|  18890 | 1332 | `			nLen = nSrcLen + nLen - nOfft;` |
|  18890 | 1333 | `			if( nLen < 1 ){` |
|      - | 1334 | `				/* Invalid  length */` |
|      3 | 1335 | `				nLen = nSrcLen - nOfft;` |
|      1 | 1336 | `			}` |
|   9444 | 1337 | `		}` |
|  93028 | 1338 | `		if( nLen + nOfft > nSrcLen ){` |
|      - | 1339 | `			/* Invalid length */` |
|   2250 | 1340 | `			nLen = nSrcLen - nOfft;` |
|   1124 | 1341 | `		}` |
|  46513 | 1342 | `	}` |
|      - | 1343 | `	/* Return the substring */` |
| 111916 | 1344 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 111916 | 1345 | `	return PH7_OK;` |
|  59700 | 1346 |  |
|      - | 1347 | `/*` |
|      - | 1348 | ` * int substr_compare(string $main_str,string $str ,int $offset[,int $length[,bool $case_insensitivity = false ]])` |
|      - | 1349 | ` *  Binary safe comparison of two strings from an offset, up to length characters.` |
|      - | 1350 | ` * Parameters` |
|      - | 1351 | ` *  $main_str` |
|      - | 1352 | ` *  The main string being compared.` |
|      - | 1353 | ` *  $str` |
|      - | 1354 | ` *   The secondary string being compared.` |
|      - | 1355 | ` * $offset` |
|      - | 1356 | ` *  The start position for the comparison. If negative, it starts counting from` |
|      - | 1357 | ` *  the end of the string.` |
|      - | 1358 | ` * $length` |
|      - | 1359 | ` *  The length of the comparison. The default value is the largest of the length` |
|      - | 1360 | ` *  of the str compared to the length of main_str less the offset.` |
|      - | 1361 | ` * $case_insensitivity` |
|      - | 1362 | ` *  If case_insensitivity is TRUE, comparison is case insensitive.` |
|      - | 1363 | ` * Return` |
|      - | 1364 | ` *  Returns < 0 if main_str from position offset is less than str, > 0 if it is greater than` |
|      - | 1365 | ` *  str, and 0 if they are equal. If offset is equal to or greater than the length of main_str` |
|      - | 1366 | ` *  or length is set and is less than 1, substr_compare() prints a warning and returns FALSE.` |
|      - | 1367 | ` */` |
|     26 | 1368 | `static int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1369 |  |
|      - | 1370 | `	const char *zSource,*zOfft,*zSub;` |
|      - | 1371 | `	int nOfft,nLen,nSrcLen,nSublen;` |
|     27 | 1372 | `	int iCase = 0;` |
|      - | 1373 | `	int rc;` |
|     27 | 1374 | `	if( nArg < 3 ){` |
|      - | 1375 | `		/* Missing arguments,return FALSE */` |
|      5 | 1376 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1377 | `		return PH7_OK;` |
|      - | 1378 | `	}` |
|      - | 1379 | `	/* Extract the target string */` |
|     23 | 1380 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|     23 | 1381 | `	if( nSrcLen < 1 ){` |
|      - | 1382 | `		/* Empty string,return FALSE */` |
|      3 | 1383 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1384 | `		return PH7_OK;` |
|      - | 1385 | `	}` |
|     21 | 1386 | `	nLen = nSrcLen; /* cc warning */` |
|      - | 1387 | `	/* Extract the substring */` |
|     21 | 1388 | `	zSub = ph7_value_to_string(apArg[1],&nSublen);` |
|     21 | 1389 | `	if( nSublen < 1 \|\| nSublen > nSrcLen){` |
|      - | 1390 | `		/* Empty string,return FALSE */` |
|      3 | 1391 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1392 | `		return PH7_OK;` |
|      - | 1393 | `	}` |
|      - | 1394 | `	/* Extract the offset */` |
|     19 | 1395 | `	nOfft = ph7_value_to_int(apArg[2]);` |
|     19 | 1396 | `	if( nOfft < 0 ){` |
|      5 | 1397 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|      5 | 1398 | `		if( zOfft < zSource ){` |
|      - | 1399 | `			/* Invalid offset */` |
|      3 | 1400 | `			ph7_result_bool(pCtx,0);` |
|      3 | 1401 | `			return PH7_OK;` |
|      - | 1402 | `		}` |
|      3 | 1403 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|      3 | 1404 | `		nOfft = (int)(zOfft-zSource);` |
|     16 | 1405 | `	}else if( nOfft >= nSrcLen ){` |
|      - | 1406 | `		/* Invalid offset */` |
|      3 | 1407 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1408 | `		return PH7_OK;` |
|    ! 0 | 1409 | `	}else{` |
|     13 | 1410 | `		zOfft = &zSource[nOfft];` |
|     13 | 1411 | `		nLen = nSrcLen - nOfft;` |
|      - | 1412 | `	}` |
|     15 | 1413 | `	if( nArg > 3 ){` |
|      - | 1414 | `		/* Extract the length */` |
|     13 | 1415 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     13 | 1416 | `		if( nLen < 1 ){` |
|      - | 1417 | `			/* Invalid  length */` |
|      5 | 1418 | `			ph7_result_int(pCtx,1);` |
|      5 | 1419 | `			return PH7_OK;` |
|      9 | 1420 | `		}else if( nLen + nOfft > nSrcLen ){` |
|      - | 1421 | `			/* Invalid length */` |
|      3 | 1422 | `			nLen = nSrcLen - nOfft;` |
|      1 | 1423 | `		}` |
|      9 | 1424 | `		if( nArg > 4 ){` |
|      - | 1425 | `			/* Case-sensitive or not */` |
|      5 | 1426 | `			iCase = ph7_value_to_bool(apArg[4]);` |
|      2 | 1427 | `		}` |
|      4 | 1428 | `	}` |
|      - | 1429 | `	/* Perform the comparison */` |
|     11 | 1430 | `	if( iCase ){` |
|      3 | 1431 | `		rc = SyStrnicmp(zOfft,zSub,(sxu32)nLen);` |
|      2 | 1432 | `	}else{` |
|      9 | 1433 | `		rc = SyStrncmp(zOfft,zSub,(sxu32)nLen);` |
|      - | 1434 | `	}` |
|      - | 1435 | `	/* Comparison result */` |
|     11 | 1436 | `	ph7_result_int(pCtx,rc);` |
|     11 | 1437 | `	return PH7_OK;` |
|     14 | 1438 |  |
|      - | 1439 | `/*` |
|      - | 1440 | ` * int substr_count(string $haystack,string $needle[,int $offset = 0 [,int $length ]])` |
|      - | 1441 | ` *  Count the number of substring occurrences.` |
|      - | 1442 | ` * Parameters` |
|      - | 1443 | ` * $haystack` |
|      - | 1444 | ` *   The string to search in` |
|      - | 1445 | ` * $needle` |
|      - | 1446 | ` *   The substring to search for` |
|      - | 1447 | ` * $offset` |
|      - | 1448 | ` *  The offset where to start counting` |
|      - | 1449 | ` * $length (NOT USED)` |
|      - | 1450 | ` *  The maximum length after the specified offset to search for the substring.` |
|      - | 1451 | ` *  It outputs a warning if the offset plus the length is greater than the haystack length.` |
|      - | 1452 | ` * Return` |
|      - | 1453 | ` *  Toral number of substring occurrences.` |
|      - | 1454 | ` */` |
|     24 | 1455 | `static int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1456 |  |
|      - | 1457 | `	const char *zText,*zPattern,*zEnd;` |
|      - | 1458 | `	int nTextlen,nPatlen;` |
|     25 | 1459 | `	int iCount = 0;` |
|      - | 1460 | `	sxu32 nOfft;` |
|      - | 1461 | `	sxi32 rc;` |
|     25 | 1462 | `	if( nArg < 2 ){` |
|      - | 1463 | `		/* Missing arguments */` |
|      5 | 1464 | `		ph7_result_int(pCtx,0);` |
|      5 | 1465 | `		return PH7_OK;` |
|      - | 1466 | `	}` |
|      - | 1467 | `	/* Point to the haystack */` |
|     21 | 1468 | `	zText = ph7_value_to_string(apArg[0],&nTextlen);` |
|      - | 1469 | `	/* Point to the neddle */` |
|     21 | 1470 | `	zPattern = ph7_value_to_string(apArg[1],&nPatlen);` |
|     21 | 1471 | `	if( nTextlen < 1 \|\| nPatlen < 1 \|\| nPatlen > nTextlen ){` |
|      - | 1472 | `		/* NOOP,return zero */` |
|      3 | 1473 | `		ph7_result_int(pCtx,0);` |
|      3 | 1474 | `		return PH7_OK;` |
|      - | 1475 | `	}` |
|     19 | 1476 | `	if( nArg > 2 ){` |
|      - | 1477 | `		int iOfft;` |
|      - | 1478 | `		/* Extract the offset */` |
|     15 | 1479 | `		iOfft = ph7_value_to_int(apArg[2]);` |
|     15 | 1480 | `		if( iOfft < 0 \|\| iOfft > nTextlen ){` |
|      - | 1481 | `			/* Invalid offset,return zero */` |
|      3 | 1482 | `			ph7_result_int(pCtx,0);` |
|      3 | 1483 | `			return PH7_OK;` |
|      - | 1484 | `		}` |
|      - | 1485 | `		/* Point to the desired offset */` |
|     13 | 1486 | `		zText = &zText[iOfft];` |
|      - | 1487 | `		/* Adjust length */` |
|     13 | 1488 | `		nTextlen -= iOfft;` |
|      6 | 1489 | `	}` |
|      - | 1490 | `	/* Point to the end of the string */` |
|     17 | 1491 | `	zEnd = &zText[nTextlen];` |
|     17 | 1492 | `	if( nArg > 3 ){` |
|      - | 1493 | `		int nLen;` |
|      - | 1494 | `		/* Extract the length */` |
|     13 | 1495 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     13 | 1496 | `		if( nLen < 0 \|\| nLen > nTextlen ){` |
|      - | 1497 | `			/* Invalid length,return 0 */` |
|      7 | 1498 | `			ph7_result_int(pCtx,0);` |
|      7 | 1499 | `			return PH7_OK;` |
|      - | 1500 | `		}` |
|      - | 1501 | `		/* Adjust pointer */` |
|      7 | 1502 | `		nTextlen = nLen;` |
|      7 | 1503 | `		zEnd = &zText[nTextlen];` |
|      3 | 1504 | `	}` |
|      - | 1505 | `	/* Perform the search */` |
|     12 | 1506 | `	for(;;){` |
|     25 | 1507 | `		rc = SyBlobSearch((const void *)zText,(sxu32)(zEnd-zText),(const void *)zPattern,nPatlen,&nOfft);` |
|     25 | 1508 | `		if( rc != SXRET_OK ){` |
|      - | 1509 | `			/* Pattern not found,break immediately */` |
|      9 | 1510 | `			break;` |
|      - | 1511 | `		}` |
|      - | 1512 | `		/* Increment counter and update the offset */` |
|     17 | 1513 | `		iCount++;` |
|     17 | 1514 | `		zText += nOfft + nPatlen;` |
|     17 | 1515 | `		if( zText >= zEnd ){` |
|      3 | 1516 | `			break;` |
|      - | 1517 | `		}` |
|      1 | 1518 | `	}` |
|      - | 1519 | `	/* Pattern count */` |
|     11 | 1520 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 1521 | `	return PH7_OK;` |
|     13 | 1522 |  |
|      - | 1523 | `/*` |
|      - | 1524 | ` * string chunk_split(string $body[,int $chunklen = 76 [, string $end = "\r\n" ]])` |
|      - | 1525 | ` *   Split a string into smaller chunks.` |
|      - | 1526 | ` * Parameters` |
|      - | 1527 | ` *  $body` |
|      - | 1528 | ` *   The string to be chunked.` |
|      - | 1529 | ` * $chunklen` |
|      - | 1530 | ` *   The chunk length.` |
|      - | 1531 | ` * $end` |
|      - | 1532 | ` *   The line ending sequence.` |
|      - | 1533 | ` * Return` |
|      - | 1534 | ` *  The chunked string or NULL on failure.` |
|      - | 1535 | ` */` |
|     16 | 1536 | `static int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1537 |  |
|     17 | 1538 | `	const char *zIn,*zEnd,*zSep = "\r\n";` |
|      - | 1539 | `	int nSepLen,nChunkLen,nLen;` |
|     17 | 1540 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1541 | `		/* Nothing to split,return null */` |
|      5 | 1542 | `		ph7_result_null(pCtx);` |
|      5 | 1543 | `		return PH7_OK;` |
|      - | 1544 | `	}` |
|      - | 1545 | `	/* initialize/Extract arguments */` |
|     13 | 1546 | `	nSepLen = (int)sizeof("\r\n") - 1;` |
|     13 | 1547 | `	nChunkLen = 76;` |
|     13 | 1548 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 1549 | `	zEnd = &zIn[nLen];` |
|     13 | 1550 | `	if( nArg > 1 ){` |
|      - | 1551 | `		/* Chunk length */` |
|     13 | 1552 | `		nChunkLen = ph7_value_to_int(apArg[1]);` |
|     13 | 1553 | `		if( nChunkLen < 1 ){` |
|      - | 1554 | `			/* Switch back to the default length */` |
|      3 | 1555 | `			nChunkLen = 76;` |
|      1 | 1556 | `		}` |
|     13 | 1557 | `		if( nArg > 2 ){` |
|      - | 1558 | `			/* Separator */` |
|      9 | 1559 | `			zSep = ph7_value_to_string(apArg[2],&nSepLen);` |
|      9 | 1560 | `			if( nSepLen < 1 ){` |
|      - | 1561 | `				/* Switch back to the default separator */` |
|      3 | 1562 | `				zSep = "\r\n";` |
|      3 | 1563 | `				nSepLen = (int)sizeof("\r\n") - 1;` |
|      1 | 1564 | `			}` |
|      4 | 1565 | `		}` |
|      6 | 1566 | `	}` |
|      - | 1567 | `	/* Perform the requested operation */` |
|     13 | 1568 | `	if( nChunkLen > nLen ){` |
|      - | 1569 | `		/* Nothing to split,return the string and the separator */` |
|      9 | 1570 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);` |
|      9 | 1571 | `		return PH7_OK;` |
|      - | 1572 | `	}` |
|     17 | 1573 | `	while( zIn < zEnd ){` |
|     13 | 1574 | `		if( nChunkLen > (int)(zEnd-zIn) ){` |
|      3 | 1575 | `			nChunkLen = (int)(zEnd - zIn);` |
|      1 | 1576 | `		}` |
|      - | 1577 | `		/* Append the chunk and the separator */` |
|     13 | 1578 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);` |
|      - | 1579 | `		/* Point beyond the chunk */` |
|     13 | 1580 | `		zIn += nChunkLen;` |
|      1 | 1581 | `	}` |
|      5 | 1582 | `	return PH7_OK;` |
|      9 | 1583 |  |
|      - | 1584 | `/*` |
|      - | 1585 | ` * string addslashes(string $str)` |
|      - | 1586 | ` *  Quote string with slashes.` |
|      - | 1587 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1588 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1589 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1590 | ` * Parameter` |
|      - | 1591 | ` *  str: The string to be escaped.` |
|      - | 1592 | ` * Return` |
|      - | 1593 | ` *  Returns the escaped string` |
|      - | 1594 | ` */` |
|     24 | 1595 | `static int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1596 |  |
|      - | 1597 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1598 | `	int nLen;` |
|      - | 1599 | `	/* PHP enforces exactly one argument. */` |
|     26 | 1600 | `	if( nArg != 1 ){` |
|      7 | 1601 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1602 | `			"ArgumentCountError",` |
|      - | 1603 | `			"addslashes() expects exactly 1 argument, %d given",` |
|      2 | 1604 | `			nArg` |
|      - | 1605 | `			);` |
|      - | 1606 | `	}` |
|      - | 1607 | `	/* NULL is deprecated and treated as an empty string; other invalid` |
|      - | 1608 | `	 * types still produce a TypeError. */` |
|     22 | 1609 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 1610 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 1611 | `			E_DEPRECATED,` |
|      - | 1612 | `			"addslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - | 1613 | `			);` |
|      - | 1614 | `		/* fall through so conversion below yields empty string */` |
|      1 | 1615 | `	}` |
|      - | 1616 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     38 | 1617 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     29 | 1618 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     18 | 1619 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 1620 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1621 | `			"TypeError",` |
|      - | 1622 | `			"addslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 1623 | `			ph7_type_name(apArg[0])` |
|      - | 1624 | `			);` |
|      - | 1625 | `	}` |
|      - | 1626 | `	/* Convert to string representation first and obtain length. */` |
|     19 | 1627 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 1628 | `	if( nLen < 1 ){` |
|      - | 1629 | `		/* Return the empty string */` |
|      5 | 1630 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1631 | `		return PH7_OK;` |
|      - | 1632 | `	}` |
|     15 | 1633 | `	zEnd = &zIn[nLen];` |
|     15 | 1634 | `	zCur = 0; /* cc warning */` |
|     20 | 1635 | `	for(;;){` |
|     41 | 1636 | `		if( zIn >= zEnd ){` |
|      - | 1637 | `			/* No more input */` |
|     15 | 1638 | `			break;` |
|      - | 1639 | `		}` |
|     27 | 1640 | `		zCur = zIn;` |
|      - | 1641 | `		/* scan until a character that needs escaping (', ", \\, or NUL) */` |
|     89 | 1642 | `		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' && zIn[0] != '\0' ){` |
|     63 | 1643 | `			zIn++;` |
|      1 | 1644 | `		}` |
|     27 | 1645 | `		if( zIn > zCur ){` |
|      - | 1646 | `			/* Append raw contents */` |
|     23 | 1647 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 1648 | `		}` |
|     27 | 1649 | `		if( zIn < zEnd ){` |
|     17 | 1650 | `			int c = zIn[0];` |
|     17 | 1651 | `			if( c == '\0' ){` |
|      - | 1652 | `				/* PHP escapes NUL as "\\0" (two characters) */` |
|      3 | 1653 | `				ph7_result_string(pCtx,"\\0",2);` |
|      2 | 1654 | `			}else{` |
|     15 | 1655 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1656 | `			}` |
|      8 | 1657 | `		}` |
|     27 | 1658 | `		zIn++;` |
|      1 | 1659 | `	}` |
|     15 | 1660 | `	return PH7_OK;` |
|     14 | 1661 |  |
|      - | 1662 | `/*` |
|      - | 1663 | ` * Check if the given character is present in the given mask.` |
|      - | 1664 | ` * Return TRUE if present. FALSE otherwise.` |
|      - | 1665 | ` */` |
|    124 | 1666 | `static int cSlashCheckMask(int c,const char *zMask,int nLen)` |
|      1 | 1667 |  |
|    125 | 1668 | `	const char *zEnd = &zMask[nLen];` |
|    555 | 1669 | `	while( zMask < zEnd ){` |
|      - | 1670 | `		/* Support range syntax A..Z where A and Z are literal bytes.  The` |
|      - | 1671 | `		 * original PH7 implementation ignored ranges; tests rely on them so` |
|      - | 1672 | `		 * provide a simple on-the-fly check here. */` |
|    475 | 1673 | `		if( zMask + 3 < zEnd && zMask[1] == '.' && zMask[2] == '.' ){` |
|      3 | 1674 | `			int lo = (unsigned char)zMask[0];` |
|      3 | 1675 | `			int hi = (unsigned char)zMask[3];` |
|      3 | 1676 | `			if( lo > hi ){` |
|    ! 0 | 1677 | `				int tmp = lo; lo = hi; hi = tmp;` |
|    ! 0 | 1678 | `			}` |
|      3 | 1679 | `			if( c >= lo && c <= hi ){` |
|      3 | 1680 | `				return 1;` |
|      - | 1681 | `			}` |
|      - | 1682 | `			/* consume the range specifier */` |
|    ! 0 | 1683 | `			zMask += 4;` |
|    ! 0 | 1684 | `			continue;` |
|      - | 1685 | `		}` |
|    473 | 1686 | `		if( zMask[0] == c ){` |
|      - | 1687 | `			/* Character present,return TRUE */` |
|     43 | 1688 | `			return 1;` |
|      - | 1689 | `		}` |
|      - | 1690 | `		/* Advance the pointer */` |
|    431 | 1691 | `		zMask++;` |
|      1 | 1692 | `	}` |
|      - | 1693 | `	/* Not present */` |
|     81 | 1694 | `	return 0;` |
|     63 | 1695 |  |
|      - | 1696 | `/*` |
|      - | 1697 | ` * string addcslashes(string $str,string $charlist)` |
|      - | 1698 | ` *  Quote string with slashes in a C style.` |
|      - | 1699 | ` * Parameter` |
|      - | 1700 | ` *  $str:` |
|      - | 1701 | ` *    The string to be escaped.` |
|      - | 1702 | ` *  $charlist:` |
|      - | 1703 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|      - | 1704 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|      - | 1705 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|      - | 1706 | ` * Return` |
|      - | 1707 | ` *  Returns the escaped string.` |
|      - | 1708 | ` * Note:` |
|      - | 1709 | ` *  Range characters [i.e: 'A..Z'] is not implemented in the current release.` |
|      - | 1710 | ` */` |
|     34 | 1711 | `static int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1712 |  |
|      - | 1713 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - | 1714 | `	int nLen,nMask;` |
|      - | 1715 | `	/* PHP enforces exactly two arguments. */` |
|     36 | 1716 | `	if( nArg != 2 ){` |
|      7 | 1717 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1718 | `			"ArgumentCountError",` |
|      - | 1719 | `			"addcslashes() expects exactly 2 arguments, %d given",` |
|      2 | 1720 | `			nArg` |
|      - | 1721 | `			);` |
|      - | 1722 | `	}` |
|      - | 1723 | `	/* First argument must be a string-ish value.  NULL is deprecated and` |
|      - | 1724 | `	 * treated as the empty string (PHP 8.1). */` |
|     32 | 1725 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      - | 1726 | `		/* Emit deprecation only once, similar to PHP behaviour. */` |
|      3 | 1727 | `		PH7_VmThrowError(pCtx->pVm,0,/* iErr will be patched to 8192 below */` |
|      - | 1728 | `			E_DEPRECATED,` |
|      - | 1729 | `			"addcslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - | 1730 | `			);` |
|      - | 1731 | `		/* treat as empty string; fall through to conversion logic */` |
|     56 | 1732 | `	} else if( ph7_value_is_array(apArg[0]) \|\|` |
|     41 | 1733 | `	          ph7_value_is_object(apArg[0]) \|\|` |
|     26 | 1734 | `	          ph7_value_is_resource(apArg[0]) ){` |
|      4 | 1735 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1736 | `			"TypeError",` |
|      - | 1737 | `			"addcslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 1738 | `			ph7_type_name(apArg[0])` |
|      - | 1739 | `			);` |
|      - | 1740 | `	}` |
|      - | 1741 | `	/* Second argument must be a string.  NULL is deprecated and treated as` |
|      - | 1742 | `	 * an empty mask per PHP semantics.  Arrays/objects/resources still` |
|      - | 1743 | `	 * trigger a TypeError. */` |
|     30 | 1744 | `	if( ph7_value_is_null(apArg[1]) ){` |
|      3 | 1745 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 1746 | `			E_DEPRECATED,` |
|      - | 1747 | `			"addcslashes(): Passing null to parameter #2 ($characters) of type string is deprecated"` |
|      - | 1748 | `			);` |
|      - | 1749 | `		/* allow through so it becomes empty string below */` |
|     52 | 1750 | `	} else if( ph7_value_is_array(apArg[1]) \|\|` |
|     38 | 1751 | `	          ph7_value_is_object(apArg[1]) \|\|` |
|     24 | 1752 | `	          ph7_value_is_resource(apArg[1]) ){` |
|      4 | 1753 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1754 | `			"TypeError",` |
|      - | 1755 | `			"addcslashes(): Argument #2 ($characters) must be of type string, %s given",` |
|      2 | 1756 | `			ph7_type_name(apArg[1])` |
|      - | 1757 | `			);` |
|      - | 1758 | `	}` |
|      - | 1759 | `	/* Extract the string to process */` |
|     27 | 1760 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1761 | `	/* NULL would never reach here due to the check above. */` |
|     27 | 1762 | `	if( nLen < 1 ){` |
|      - | 1763 | `		/* Empty string returns itself. */` |
|      5 | 1764 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      5 | 1765 | `		return PH7_OK;` |
|      - | 1766 | `	}` |
|      - | 1767 | `	/* Extract the desired mask */` |
|     23 | 1768 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|     23 | 1769 | `	zEnd = &zIn[nLen];` |
|     23 | 1770 | `	zCur = 0; /* cc warning */` |
|     29 | 1771 | `	for(;;){` |
|     59 | 1772 | `		if( zIn >= zEnd ){` |
|      - | 1773 | `			/* No more input */` |
|     23 | 1774 | `			break;` |
|      - | 1775 | `		}` |
|     37 | 1776 | `		zCur = zIn;` |
|     91 | 1777 | `		while( zIn < zEnd && !cSlashCheckMask(zIn[0],zMask,nMask) ){` |
|     55 | 1778 | `			zIn++;` |
|      1 | 1779 | `		}` |
|     37 | 1780 | `		if( zIn > zCur ){` |
|      - | 1781 | `			/* Append raw contents */` |
|     33 | 1782 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 | 1783 | `		}` |
|     37 | 1784 | `		if( zIn < zEnd ){` |
|      - | 1785 | `			/* Make sure we treat the byte as unsigned to avoid negative values` |
|      - | 1786 | `			 * on platforms where char is signed. */` |
|     19 | 1787 | `			int c = (unsigned char)zIn[0];` |
|      - | 1788 | `			/* Handle special C-like escapes for common control characters first.` |
|      - | 1789 | `			 * PHP outputs "\n" "\r" "\t" "\v" "\f" when those chars are` |
|      - | 1790 | `			 * in the mask. NUL is left to the octal conversion below. */` |
|     19 | 1791 | `			if( c == '\n' ){` |
|      3 | 1792 | `				ph7_result_string(pCtx,"\\n",2);` |
|     18 | 1793 | `			}else if( c == '\r' ){` |
|      3 | 1794 | `				ph7_result_string(pCtx,"\\r",2);` |
|     16 | 1795 | `			}else if( c == '\t' ){` |
|      3 | 1796 | `				ph7_result_string(pCtx,"\\t",2);` |
|     14 | 1797 | `			}else if( c == '\v' ){` |
|      3 | 1798 | `				ph7_result_string(pCtx,"\\v",2);` |
|     12 | 1799 | `			}else if( c == '\f' ){` |
|      3 | 1800 | `				ph7_result_string(pCtx,"\\f",2);` |
|     10 | 1801 | `			}else if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|      - | 1802 | `				/* Convert to octal.  PHP always emits three-digit zero-padded` |
|      - | 1803 | `				 * octal escapes (\001 not \1). */` |
|      7 | 1804 | `				ph7_result_string_format(pCtx,"\\%03o",c);` |
|      4 | 1805 | `			}else{` |
|      3 | 1806 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1807 | `			}` |
|      9 | 1808 | `		}` |
|     37 | 1809 | `		zIn++;` |
|      1 | 1810 | `	}` |
|     23 | 1811 | `	return PH7_OK;` |
|     19 | 1812 |  |
|      - | 1813 | `/*` |
|      - | 1814 | ` * string quotemeta(string $str)` |
|      - | 1815 | ` *  Quote meta characters.` |
|      - | 1816 | ` * Parameter` |
|      - | 1817 | ` *  $str:` |
|      - | 1818 | ` *    The string to be escaped.` |
|      - | 1819 | ` * Return` |
|      - | 1820 | ` *  Returns the escaped string.` |
|      - | 1821 | `*/` |
|     10 | 1822 | `static int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1823 |  |
|      - | 1824 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1825 | `	int nLen;` |
|     11 | 1826 | `	if( nArg < 1 ){` |
|      - | 1827 | `		/* Nothing to process,retun NULL */` |
|      3 | 1828 | `		ph7_result_null(pCtx);` |
|      3 | 1829 | `		return PH7_OK;` |
|      - | 1830 | `	}` |
|      - | 1831 | `	/* Extract the string to process */` |
|      9 | 1832 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 1833 | `	if( nLen < 1 ){` |
|      - | 1834 | `		/* Return the empty string */` |
|      3 | 1835 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1836 | `		return PH7_OK;` |
|      - | 1837 | `	}` |
|      7 | 1838 | `	zEnd = &zIn[nLen];` |
|      7 | 1839 | `	zCur = 0; /* cc warning */` |
|     17 | 1840 | `	for(;;){` |
|     35 | 1841 | `		if( zIn >= zEnd ){` |
|      - | 1842 | `			/* No more input */` |
|      7 | 1843 | `			break;` |
|      - | 1844 | `		}` |
|     29 | 1845 | `		zCur = zIn;` |
|     55 | 1846 | `		while( zIn < zEnd && !cSlashCheckMask(zIn[0],".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1) ){` |
|     27 | 1847 | `			zIn++;` |
|      1 | 1848 | `		}` |
|     29 | 1849 | `		if( zIn > zCur ){` |
|      - | 1850 | `			/* Append raw contents */` |
|     11 | 1851 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      5 | 1852 | `		}` |
|     29 | 1853 | `		if( zIn < zEnd ){` |
|     27 | 1854 | `			int c = zIn[0];` |
|     27 | 1855 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|     13 | 1856 | `		}` |
|     29 | 1857 | `		zIn++;` |
|      1 | 1858 | `	}` |
|      7 | 1859 | `	return PH7_OK;` |
|      6 | 1860 |  |
|      - | 1861 | `/*` |
|      - | 1862 | ` * string stripslashes(string $str)` |
|      - | 1863 | ` *  Un-quotes a quoted string.` |
|      - | 1864 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1865 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1866 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1867 | ` * Parameter` |
|      - | 1868 | ` *  $str` |
|      - | 1869 | ` *   The input string.` |
|      - | 1870 | ` * Return` |
|      - | 1871 | ` *  Returns a string with backslashes stripped off.` |
|      - | 1872 | ` */` |
|      8 | 1873 | `static int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1874 |  |
|      - | 1875 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1876 | `	int nLen;` |
|      9 | 1877 | `	if( nArg < 1 ){` |
|      - | 1878 | `		/* Nothing to process,retun NULL */` |
|      3 | 1879 | `		ph7_result_null(pCtx);` |
|      3 | 1880 | `		return PH7_OK;` |
|      - | 1881 | `	}` |
|      - | 1882 | `	/* Extract the string to process */` |
|      7 | 1883 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 1884 | `	if( zIn == 0 ){` |
|    ! 0 | 1885 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1886 | `		return PH7_OK;` |
|      - | 1887 | `	}` |
|      7 | 1888 | `	zEnd = &zIn[nLen];` |
|      7 | 1889 | `	zCur = 0; /* cc warning */` |
|      - | 1890 | `	/* Encode the string */` |
|      4 | 1891 | `	for(;;){` |
|      9 | 1892 | `		if( zIn >= zEnd ){` |
|      - | 1893 | `			/* No more input */` |
|      5 | 1894 | `			break;` |
|      - | 1895 | `		}` |
|      5 | 1896 | `		zCur = zIn;` |
|     17 | 1897 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 | 1898 | `			zIn++;` |
|      1 | 1899 | `		}` |
|      5 | 1900 | `		if( zIn > zCur ){` |
|      - | 1901 | `			/* Append raw contents */` |
|      5 | 1902 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 1903 | `		}` |
|      5 | 1904 | `		if( &zIn[1] < zEnd ){` |
|      3 | 1905 | `			int c = zIn[1];` |
|      3 | 1906 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - | 1907 | `				/* Ignore the backslash */` |
|      3 | 1908 | `				zIn++;` |
|      1 | 1909 | `			}` |
|      2 | 1910 | `		}else{` |
|      3 | 1911 | `			break;` |
|      - | 1912 | `		}` |
|      1 | 1913 | `	}` |
|      7 | 1914 | `	return PH7_OK;` |
|      5 | 1915 |  |
|      - | 1916 | `/*` |
|      - | 1917 | ` * string htmlspecialchars(string $string [, int $flags = ENT_COMPAT \| ENT_HTML401 [, string $charset]])` |
|      - | 1918 | ` *  HTML escaping of special characters.` |
|      - | 1919 | ` *  The translations performed are:` |
|      - | 1920 | ` *   '&' (ampersand) ==> '&amp;'` |
|      - | 1921 | ` *   '"' (double quote) ==> '&quot;' when ENT_NOQUOTES is not set.` |
|      - | 1922 | ` *   "'" (single quote) ==> '&#039;' only when ENT_QUOTES is set.` |
|      - | 1923 | ` *   '<' (less than) ==> '&lt;'` |
|      - | 1924 | ` *   '>' (greater than) ==> '&gt;'` |
|      - | 1925 | ` * Parameters` |
|      - | 1926 | ` *  $string` |
|      - | 1927 | ` *   The string being converted.` |
|      - | 1928 | ` * $flags` |
|      - | 1929 | ` *   A bitmask of one or more of the following flags, which specify how to handle quotes.` |
|      - | 1930 | ` *   The default is ENT_COMPAT \| ENT_HTML401.` |
|      - | 1931 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone.` |
|      - | 1932 | ` *   ENT_QUOTES 	Will convert both double and single quotes.` |
|      - | 1933 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted.` |
|      - | 1934 | ` *   ENT_IGNORE 	Silently discard invalid code unit sequences instead of returning an empty string.` |
|      - | 1935 | ` * $charset` |
|      - | 1936 | ` *  Defines character set used in conversion. The default character set is ISO-8859-1. (Not used)` |
|      - | 1937 | ` * Return` |
|      - | 1938 | ` *  The escaped string or NULL on failure.` |
|      - | 1939 | ` */` |
|     20 | 1940 | `static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1941 |  |
|      - | 1942 | `	const char *zCur,*zIn,*zEnd;` |
|     21 | 1943 | `	int iFlags = 0x01\|0x40; /* ENT_COMPAT \| ENT_HTML401 */` |
|      - | 1944 | `	int nLen,c;` |
|     21 | 1945 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1946 | `		/* Missing/Invalid arguments,return NULL */` |
|      9 | 1947 | `		ph7_result_null(pCtx);` |
|      9 | 1948 | `		return PH7_OK;` |
|      - | 1949 | `	}` |
|      - | 1950 | `	/* Extract the target string */` |
|     13 | 1951 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1952 | `	/* Return early when the input is empty, mirroring PHP's behavior. */` |
|     13 | 1953 | `	if( nLen == 0 ){` |
|      3 | 1954 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1955 | `		return PH7_OK;` |
|      - | 1956 | `	}` |
|     11 | 1957 | `	zEnd = &zIn[nLen];` |
|      - | 1958 | `	/* Extract the flags if available */` |
|     11 | 1959 | `	if( nArg > 1 ){` |
|      9 | 1960 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 | 1961 | `		if( iFlags < 0 ){` |
|      3 | 1962 | `			iFlags = 0x01\|0x40;` |
|      1 | 1963 | `		}` |
|      4 | 1964 | `	}` |
|      - | 1965 | `	/* Perform the requested operation */` |
|     23 | 1966 | `	for(;;){` |
|     47 | 1967 | `		if( zIn >= zEnd ){` |
|      9 | 1968 | `			break;` |
|      - | 1969 | `		}` |
|     39 | 1970 | `		zCur = zIn;` |
|     83 | 1971 | `		while( zIn < zEnd && zIn[0] != '&' && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '<' && zIn[0] != '>' ){` |
|     45 | 1972 | `			zIn++;` |
|      1 | 1973 | `		}` |
|     39 | 1974 | `		if( zCur < zIn ){` |
|      - | 1975 | `			/* Append the raw string verbatim */` |
|     17 | 1976 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      8 | 1977 | `		}` |
|     39 | 1978 | `		if( zIn >= zEnd ){` |
|      3 | 1979 | `			break;` |
|      - | 1980 | `		}` |
|     37 | 1981 | `		c = zIn[0];` |
|     37 | 1982 | `		if( c == '&' ){` |
|      - | 1983 | `			/* Expand '&amp;' */` |
|      9 | 1984 | `			ph7_result_string(pCtx,"&amp;",(int)sizeof("&amp;")-1);` |
|     33 | 1985 | `		}else if( c == '<' ){` |
|      - | 1986 | `			/* Expand '&lt;' */` |
|      7 | 1987 | `			ph7_result_string(pCtx,"&lt;",(int)sizeof("&lt;")-1);` |
|     26 | 1988 | `		}else if( c == '>' ){` |
|      - | 1989 | `			/* Expand '&gt;' */` |
|      9 | 1990 | `			ph7_result_string(pCtx,"&gt;",(int)sizeof("&gt;")-1);` |
|     19 | 1991 | `		}else if( c == '\'' ){` |
|      5 | 1992 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 1993 | `				/* Expand '&#039;' */` |
|      5 | 1994 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      3 | 1995 | `			}else{` |
|      - | 1996 | `				/* Leave the single quote untouched */` |
|    ! 0 | 1997 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      1 | 1998 | `			}` |
|     13 | 1999 | `		}else if( c == '"' ){` |
|     11 | 2000 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      - | 2001 | `				/* Expand '&quot;' */` |
|      7 | 2002 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      4 | 2003 | `			}else{` |
|      - | 2004 | `				/* Leave the double quote untouched */` |
|      5 | 2005 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      - | 2006 | `			}` |
|      5 | 2007 | `		}` |
|      - | 2008 | `		/* Ignore the unsafe HTML character */` |
|     37 | 2009 | `		zIn++;` |
|      1 | 2010 | `	}` |
|     11 | 2011 | `	return PH7_OK;` |
|     11 | 2012 |  |
|      - | 2013 | `/*` |
|      - | 2014 | ` * string htmlspecialchars_decode(string $string[,int $quote_style = ENT_COMPAT ])` |
|      - | 2015 | ` *  Unescape HTML entities.` |
|      - | 2016 | ` * Parameters` |
|      - | 2017 | ` *  $string` |
|      - | 2018 | ` *   The string to decode` |
|      - | 2019 | ` *  $quote_style` |
|      - | 2020 | ` *    The quote style. One of the following constants:` |
|      - | 2021 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone (default)` |
|      - | 2022 | ` *   ENT_QUOTES 	Will convert both double and single quotes` |
|      - | 2023 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted` |
|      - | 2024 | ` * Return` |
|      - | 2025 | ` *  The unescaped string or NULL on failure.` |
|      - | 2026 | ` */` |
|     16 | 2027 | `static int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2028 |  |
|      - | 2029 | `	const char *zCur,*zIn,*zEnd;` |
|     17 | 2030 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 2031 | `	int nLen,nJump;` |
|     17 | 2032 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2033 | `		/* Missing/Invalid arguments,return NULL */` |
|      7 | 2034 | `		ph7_result_null(pCtx);` |
|      7 | 2035 | `		return PH7_OK;` |
|      - | 2036 | `	}` |
|      - | 2037 | `	/* Extract the target string */` |
|     11 | 2038 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 2039 | `	zEnd = &zIn[nLen];` |
|      - | 2040 | `	/* Extract the flags if available */` |
|     11 | 2041 | `	if( nArg > 1 ){` |
|      7 | 2042 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      7 | 2043 | `		if( iFlags < 0 ){` |
|      3 | 2044 | `			iFlags = 0x01;` |
|      1 | 2045 | `		}` |
|      3 | 2046 | `	}` |
|      - | 2047 | `	/* Perform the requested operation */` |
|     15 | 2048 | `	for(;;){` |
|     31 | 2049 | `		if( zIn >= zEnd ){` |
|     11 | 2050 | `			break;` |
|      - | 2051 | `		}` |
|     21 | 2052 | `		zCur = zIn;` |
|     51 | 2053 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|     31 | 2054 | `			zIn++;` |
|      1 | 2055 | `		}` |
|     21 | 2056 | `		if( zCur < zIn ){` |
|      - | 2057 | `			/* Append the raw string verbatim */` |
|      9 | 2058 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 2059 | `		}` |
|     21 | 2060 | `		nLen = (int)(zEnd-zIn);` |
|     21 | 2061 | `		nJump = (int)sizeof(char);` |
|     21 | 2062 | `		if( nLen >= (int)sizeof("&amp;")-1 && SyStrnicmp(zIn,"&amp;",sizeof("&amp;")-1) == 0 ){` |
|      - | 2063 | `			/* &amp; ==> '&' */` |
|      3 | 2064 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|      3 | 2065 | `			nJump = (int)sizeof("&amp;")-1;` |
|     20 | 2066 | `		}else if( nLen >= (int)sizeof("&lt;")-1 && SyStrnicmp(zIn,"&lt;",sizeof("&lt;")-1) == 0 ){` |
|      - | 2067 | `			/* &lt; ==> < */` |
|      3 | 2068 | `			ph7_result_string(pCtx,"<",(int)sizeof(char));` |
|      3 | 2069 | `			nJump = (int)sizeof("&lt;")-1;` |
|     18 | 2070 | `		}else if( nLen >= (int)sizeof("&gt;")-1 && SyStrnicmp(zIn,"&gt;",sizeof("&gt;")-1) == 0 ){` |
|      - | 2071 | `			/* &gt; ==> '>' */` |
|      3 | 2072 | `			ph7_result_string(pCtx,">",(int)sizeof(char));` |
|      3 | 2073 | `			nJump = (int)sizeof("&gt;")-1;` |
|     16 | 2074 | `		}else if( nLen >= (int)sizeof("&quot;")-1 && SyStrnicmp(zIn,"&quot;",sizeof("&quot;")-1) == 0 ){` |
|      - | 2075 | `			/* &quot; ==> '"' */` |
|     13 | 2076 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      9 | 2077 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      5 | 2078 | `			}else{` |
|      - | 2079 | `				/* Leave untouched */` |
|      5 | 2080 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      - | 2081 | `			}` |
|     13 | 2082 | `			nJump = (int)sizeof("&quot;")-1;` |
|      9 | 2083 | `		}else if( nLen >= (int)sizeof("&#039;")-1 && SyStrnicmp(zIn,"&#039;",sizeof("&#039;")-1) == 0 ){` |
|      - | 2084 | `			/* &#039; ==> ''' */` |
|      3 | 2085 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 2086 | `				/* Expand ''' */` |
|      3 | 2087 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      2 | 2088 | `			}else{` |
|      - | 2089 | `				/* Leave untouched */` |
|    ! 0 | 2090 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      - | 2091 | `			}` |
|      3 | 2092 | `			nJump = (int)sizeof("&#039;")-1;` |
|      1 | 2093 | `		}else if( nLen >= (int)sizeof(char) ){` |
|      - | 2094 | `			/* expand '&' */` |
|    ! 0 | 2095 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 2096 | `		}else{` |
|      - | 2097 | `			/* No more input to process */` |
|    ! 0 | 2098 | `			break;` |
|      - | 2099 | `		}` |
|     21 | 2100 | `		zIn += nJump;` |
|      1 | 2101 | `	}` |
|     11 | 2102 | `	return PH7_OK;` |
|      9 | 2103 |  |
|      - | 2104 | `/* HTML encoding/Decoding table` |
|      - | 2105 | ` * Source: Symisc RunTime API.[chm@symisc.net]` |
|      - | 2106 | ` */` |
|      - | 2107 | `static const char *azHtmlEscape[] = {` |
|      - | 2108 | ` 	"&lt;","<","&gt;",">","&amp;","&","&quot;","\"","&#39;","'",` |
|      - | 2109 | `	"&#33;","!","&#36;","$","&#35;","#","&#37;","%","&#40;","(",` |
|      - | 2110 | `	"&#41;",")","&#123;","{","&#125;","}","&#61;","=","&#43;","+",` |
|      - | 2111 | `	"&#63;","?","&#91;","[","&#93;","]","&#64;","@","&#44;",","` |
|      - | 2112 | ` };` |
|      - | 2113 | `/*` |
|      - | 2114 | ` * array get_html_translation_table(void)` |
|      - | 2115 | ` *  Returns the translation table used by htmlspecialchars() and htmlentities().` |
|      - | 2116 | ` * Parameters` |
|      - | 2117 | ` *  None` |
|      - | 2118 | ` * Return` |
|      - | 2119 | ` *  The translation table as an array or NULL on failure.` |
|      - | 2120 | ` */` |
|      4 | 2121 | `static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2122 |  |
|      - | 2123 | `	ph7_value *pArray,*pValue;` |
|      - | 2124 | `	sxu32 n;` |
|      - | 2125 | `	/* Element value */` |
|      5 | 2126 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      5 | 2127 | `	if( pValue == 0 ){` |
|    ! 0 | 2128 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2129 | `		SXUNUSED(apArg);` |
|      - | 2130 | `		/* Return NULL */` |
|    ! 0 | 2131 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2132 | `		return PH7_OK;` |
|      - | 2133 | `	}` |
|      - | 2134 | `	/* Create a new array */` |
|      5 | 2135 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 | 2136 | `	if( pArray == 0 ){` |
|      - | 2137 | `		/* Return NULL */` |
|    ! 0 | 2138 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2139 | `		return PH7_OK;` |
|      - | 2140 | `	}` |
|      - | 2141 | `	/* Make the table */` |
|     85 | 2142 | `	for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|      - | 2143 | `		/* Prepare the value */` |
|     81 | 2144 | `		ph7_value_string(pValue,azHtmlEscape[n],-1 /* Compute length automatically */);` |
|      - | 2145 | `		/* Insert the value */` |
|     81 | 2146 | `		ph7_array_add_strkey_elem(pArray,azHtmlEscape[n+1],pValue);` |
|      - | 2147 | `		/* Reset the string cursor */` |
|     81 | 2148 | `		ph7_value_reset_string_cursor(pValue);` |
|     41 | 2149 | `	}` |
|      - | 2150 | `	/*` |
|      - | 2151 | `	 * Return the array.` |
|      - | 2152 | `	 * Don't worry about freeing memory, everything will be automatically` |
|      - | 2153 | `	 * released upon we return from this function.` |
|      - | 2154 | `	 */` |
|      5 | 2155 | `	ph7_result_value(pCtx,pArray);` |
|      5 | 2156 | `	return PH7_OK;` |
|      3 | 2157 |  |
|      - | 2158 | `/*` |
|      - | 2159 | ` * string htmlentities( string $string [, int $flags = ENT_COMPAT \| ENT_HTML401]);` |
|      - | 2160 | ` *   Convert all applicable characters to HTML entities` |
|      - | 2161 | ` * Parameters` |
|      - | 2162 | ` * $string` |
|      - | 2163 | ` *   The input string.` |
|      - | 2164 | ` * $flags` |
|      - | 2165 | ` *  A bitmask of one or more of the flags (see block-comment on PH7_builtin_htmlspecialchars())` |
|      - | 2166 | ` * Return` |
|      - | 2167 | ` * The encoded string.` |
|      - | 2168 | ` */` |
|     10 | 2169 | `static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2170 |  |
|     11 | 2171 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 2172 | `	const char *zIn,*zEnd;` |
|      - | 2173 | `	int nLen,c;` |
|      - | 2174 | `	sxu32 n;` |
|     11 | 2175 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2176 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 2177 | `		ph7_result_null(pCtx);` |
|      5 | 2178 | `		return PH7_OK;` |
|      - | 2179 | `	}` |
|      - | 2180 | `	/* Extract the target string */` |
|      7 | 2181 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 2182 | `	/* Handle empty string up front */` |
|      7 | 2183 | `	if( nLen == 0 ){` |
|      3 | 2184 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2185 | `		return PH7_OK;` |
|      - | 2186 | `	}` |
|      5 | 2187 | `	zEnd = &zIn[nLen];` |
|      - | 2188 | `	/* Extract the flags if available */` |
|      5 | 2189 | `	if( nArg > 1 ){` |
|      3 | 2190 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      3 | 2191 | `		if( iFlags < 0 ){` |
|      3 | 2192 | `			iFlags = 0x01;` |
|      1 | 2193 | `		}` |
|      1 | 2194 | `	}` |
|      - | 2195 | `	/* Perform the requested operation */` |
|     11 | 2196 | `	for(;;){` |
|     23 | 2197 | `		if( zIn >= zEnd ){` |
|      - | 2198 | `			/* No more input to process */` |
|      5 | 2199 | `			break;` |
|      - | 2200 | `		}` |
|     19 | 2201 | `		c = zIn[0];` |
|      - | 2202 | `		/* Perform a linear lookup on the decoding table */` |
|    233 | 2203 | `		for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    223 | 2204 | `			if( azHtmlEscape[n+1][0] == c ){` |
|      - | 2205 | `				/* Got one */` |
|      9 | 2206 | `				break;` |
|      - | 2207 | `			}` |
|    108 | 2208 | `		}` |
|     19 | 2209 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|      - | 2210 | `			/* Output the safe sequence [i.e: '<' ==> '&lt;"] */` |
|      9 | 2211 | `			if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 2212 | `				/* Expand the double quote verbatim */` |
|    ! 0 | 2213 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      9 | 2214 | `			}else if(c == '\'' && ((iFlags & 0x02 /*ENT_QUOTES*/) == 0 \|\| (iFlags & 0x04) /*ENT_NOQUOTES*/) ){` |
|      - | 2215 | `				/* expand single quote verbatim */` |
|    ! 0 | 2216 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|    ! 0 | 2217 | `			}else{` |
|      9 | 2218 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1/*Compute length automatically */);` |
|      - | 2219 | `			}` |
|      5 | 2220 | `		}else{` |
|      - | 2221 | `			/* Output character verbatim */` |
|     11 | 2222 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2223 | `		}` |
|     19 | 2224 | `		zIn++;` |
|      1 | 2225 | `	}` |
|      5 | 2226 | `	return PH7_OK;` |
|      6 | 2227 |  |
|      - | 2228 | `/*` |
|      - | 2229 | ` * string html_entity_decode(string $string [, int $quote_style = ENT_COMPAT [, string $charset = 'UTF-8' ]])` |
|      - | 2230 | ` *   Perform the reverse operation of html_entity_decode().` |
|      - | 2231 | ` * Parameters` |
|      - | 2232 | ` * $string` |
|      - | 2233 | ` *   The input string.` |
|      - | 2234 | ` * $flags` |
|      - | 2235 | ` *  A bitmask of one or more of the flags (see comment on PH7_builtin_htmlspecialchars())` |
|      - | 2236 | ` * Return` |
|      - | 2237 | ` * The decoded string.` |
|      - | 2238 | ` */` |
|     28 | 2239 | `static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2240 |  |
|      - | 2241 | `	const char *zCur,*zIn,*zEnd;` |
|     29 | 2242 | `	int iFlags = 0x01; /* ENT_COMPAT  */` |
|      - | 2243 | `	int nLen;` |
|      - | 2244 | `	sxu32 n;` |
|     29 | 2245 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2246 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 2247 | `		ph7_result_null(pCtx);` |
|      5 | 2248 | `		return PH7_OK;` |
|      - | 2249 | `	}` |
|      - | 2250 | `	/* Extract the target string */` |
|     25 | 2251 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 2252 | `	zEnd = &zIn[nLen];` |
|      - | 2253 | `	/* Extract the flags if available */` |
|     25 | 2254 | `	if( nArg > 1 ){` |
|     15 | 2255 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     15 | 2256 | `		if( iFlags < 0 ){` |
|      3 | 2257 | `			iFlags = 0x01;` |
|      1 | 2258 | `		}` |
|      7 | 2259 | `	}` |
|      - | 2260 | `	/* Perform the requested operation */` |
|     27 | 2261 | `	for(;;){` |
|     55 | 2262 | `		if( zIn >= zEnd ){` |
|      - | 2263 | `			/* No more input to process */` |
|     13 | 2264 | `			break;` |
|      - | 2265 | `		}` |
|     43 | 2266 | `		zCur = zIn;` |
|    173 | 2267 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|    131 | 2268 | `			zIn++;` |
|      1 | 2269 | `		}` |
|     43 | 2270 | `		if( zCur < zIn ){` |
|      - | 2271 | `			/* Append raw string verbatim */` |
|     27 | 2272 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     13 | 2273 | `		}` |
|     43 | 2274 | `		if( zIn >= zEnd ){` |
|     13 | 2275 | `			break;` |
|      - | 2276 | `		}` |
|     31 | 2277 | `		nLen = (int)(zEnd-zIn);` |
|      - | 2278 | `		/* Find an encoded sequence */` |
|    113 | 2279 | `		for(n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    113 | 2280 | `			int iLen = (int)SyStrlen(azHtmlEscape[n]);` |
|    113 | 2281 | `			if( nLen >= iLen && SyStrnicmp(zIn,azHtmlEscape[n],(sxu32)iLen) == 0 ){` |
|      - | 2282 | `				/* Got one */` |
|     31 | 2283 | `				zIn += iLen;` |
|     31 | 2284 | `				break;` |
|      - | 2285 | `			}` |
|     42 | 2286 | `		}` |
|     31 | 2287 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|     31 | 2288 | `			int c = azHtmlEscape[n+1][0];` |
|      - | 2289 | `			/* Output the decoded character */` |
|     31 | 2290 | `			if( c == '\'' && ((iFlags & 0x02) == 0 /*ENT_QUOTES*/\|\| (iFlags & 0x04) /*ENT_NOQUOTES*/)  ){` |
|      - | 2291 | `				/* Do not process single quotes */` |
|      9 | 2292 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|     27 | 2293 | `			}else if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 2294 | `				/* Do not process double quotes */` |
|      5 | 2295 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|      3 | 2296 | `			}else{` |
|     19 | 2297 | `				ph7_result_string(pCtx,azHtmlEscape[n+1],-1); /* Compute length automatically */` |
|      - | 2298 | `			}` |
|     16 | 2299 | `		}else{` |
|      - | 2300 | `			/* Append '&' */` |
|    ! 0 | 2301 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 2302 | `			zIn++;` |
|      - | 2303 | `		}` |
|      1 | 2304 | `	}` |
|     25 | 2305 | `	return PH7_OK;` |
|     15 | 2306 |  |
|      - | 2307 | `/*` |
|      - | 2308 | ` * int strlen($string)` |
|      - | 2309 | ` *  return the length of the given string.` |
|      - | 2310 | ` * Parameter` |
|      - | 2311 | ` *  string: The string being measured for length.` |
|      - | 2312 | ` * Return` |
|      - | 2313 | ` *  length of the given string.` |
|      - | 2314 | ` */` |
|   1936 | 2315 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2316 |  |
|   1938 | 2317 | `	int iLen = 0;` |
|   1938 | 2318 | `	if( nArg > 0 ){` |
|   1936 | 2319 | `		ph7_value_to_string(apArg[0],&iLen);` |
|    967 | 2320 | `	}` |
|      - | 2321 | `	/* String length */` |
|   1938 | 2322 | `	ph7_result_int(pCtx,iLen);` |
|   1938 | 2323 | `	return PH7_OK;` |
|      2 | 2324 |  |
|      - | 2325 | `/*` |
|      - | 2326 | ` * int strcmp(string $str1,string $str2)` |
|      - | 2327 | ` *  Perform a binary safe string comparison.` |
|      - | 2328 | ` * Parameter` |
|      - | 2329 | ` *  str1: The first string` |
|      - | 2330 | ` *  str2: The second string` |
|      - | 2331 | ` * Return` |
|      - | 2332 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2333 | ` *  than str2, and 0 if they are equal.` |
|      - | 2334 | ` */` |
|     66 | 2335 | `static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2336 |  |
|      - | 2337 | `	const char *z1,*z2;` |
|      - | 2338 | `	int n1,n2;` |
|      - | 2339 | `	int res;` |
|     67 | 2340 | `	if( nArg < 2 ){` |
|      5 | 2341 | `		res = nArg == 0 ? 0 : 1;` |
|      5 | 2342 | `		ph7_result_int(pCtx,res);` |
|      5 | 2343 | `		return PH7_OK;` |
|      - | 2344 | `	}` |
|      - | 2345 | `	/* Perform the comparison */` |
|     63 | 2346 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     63 | 2347 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     63 | 2348 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2349 | `	/* Comparison result */` |
|     63 | 2350 | `	ph7_result_int(pCtx,res);` |
|     63 | 2351 | `	return PH7_OK;` |
|     34 | 2352 |  |
|      - | 2353 | `/*` |
|      - | 2354 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 2355 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 2356 | ` * Parameter` |
|      - | 2357 | ` *  str1: The first string` |
|      - | 2358 | ` *  str2: The second string` |
|      - | 2359 | ` * Return` |
|      - | 2360 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2361 | ` *  than str2, and 0 if they are equal.` |
|      - | 2362 | ` */` |
|     20 | 2363 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2364 |  |
|      - | 2365 | `	const char *z1,*z2;` |
|      - | 2366 | `	int res;` |
|      - | 2367 | `	int n;` |
|     21 | 2368 | `	if( nArg < 3 ){` |
|      - | 2369 | `		/* Perform a standard comparison */` |
|      5 | 2370 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 2371 | `	}` |
|      - | 2372 | `	/* Desired comparison length */` |
|     17 | 2373 | `	n  = ph7_value_to_int(apArg[2]);` |
|     17 | 2374 | `	if( n < 0 ){` |
|      - | 2375 | `		/* Invalid length */` |
|      3 | 2376 | `		ph7_result_int(pCtx,-1);` |
|      3 | 2377 | `		return PH7_OK;` |
|      - | 2378 | `	}` |
|      - | 2379 | `	/* Perform the comparison */` |
|     15 | 2380 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     15 | 2381 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     15 | 2382 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 2383 | `	/* Comparison result */` |
|     15 | 2384 | `	ph7_result_int(pCtx,res);` |
|     15 | 2385 | `	return PH7_OK;` |
|     11 | 2386 |  |
|      - | 2387 | `/*` |
|      - | 2388 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 2389 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 2390 | ` * Parameter` |
|      - | 2391 | ` *  str1: The first string` |
|      - | 2392 | ` *  str2: The second string` |
|      - | 2393 | ` * Return` |
|      - | 2394 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2395 | ` *  than str2, and 0 if they are equal.` |
|      - | 2396 | ` */` |
|     22 | 2397 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2398 |  |
|      - | 2399 | `	const char *z1,*z2;` |
|      - | 2400 | `	int n1,n2;` |
|      - | 2401 | `	int res;` |
|     23 | 2402 | `	if( nArg < 2 ){` |
|      9 | 2403 | `		res = nArg == 0 ? 0 : 1;` |
|      9 | 2404 | `		ph7_result_int(pCtx,res);` |
|      9 | 2405 | `		return PH7_OK;` |
|      - | 2406 | `	}` |
|      - | 2407 | `	/* Perform the comparison */` |
|     15 | 2408 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     15 | 2409 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     15 | 2410 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2411 | `	/* Comparison result */` |
|     15 | 2412 | `	ph7_result_int(pCtx,res);` |
|     15 | 2413 | `	return PH7_OK;` |
|     12 | 2414 |  |
|      - | 2415 | `/*` |
|      - | 2416 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 2417 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 2418 | ` * Parameter` |
|      - | 2419 | ` *  $str1: The first string` |
|      - | 2420 | ` *  $str2: The second string` |
|      - | 2421 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 2422 | ` * Return` |
|      - | 2423 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2424 | ` *  than str2, and 0 if they are equal.` |
|      - | 2425 | ` */` |
|      8 | 2426 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2427 |  |
|      - | 2428 | `	const char *z1,*z2;` |
|      - | 2429 | `	int res;` |
|      - | 2430 | `	int n;` |
|      9 | 2431 | `	if( nArg < 3 ){` |
|      - | 2432 | `		/* Perform a standard comparison */` |
|      5 | 2433 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 2434 | `	}` |
|      - | 2435 | `	/* Desired comparison length */` |
|      5 | 2436 | `	n  = ph7_value_to_int(apArg[2]);` |
|      5 | 2437 | `	if( n < 0 ){` |
|      - | 2438 | `		/* Invalid length */` |
|    ! 0 | 2439 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2440 | `		return PH7_OK;` |
|      - | 2441 | `	}` |
|      - | 2442 | `	/* Perform the comparison */` |
|      5 | 2443 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|      5 | 2444 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|      5 | 2445 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 2446 | `	/* Comparison result */` |
|      5 | 2447 | `	ph7_result_int(pCtx,res);` |
|      5 | 2448 | `	return PH7_OK;` |
|      5 | 2449 |  |
|      - | 2450 | `/*` |
|      - | 2451 | ` * Implode context [i.e: it's private data].` |
|      - | 2452 | ` * A pointer to the following structure is forwarded` |
|      - | 2453 | ` * verbatim to the array walker callback defined below.` |
|      - | 2454 | ` */` |
|      - | 2455 | `struct implode_data {` |
|      - | 2456 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 2457 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 2458 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 2459 | `	int nSeplen;          /* Separator length */` |
|      - | 2460 | `	int bFirst;           /* TRUE if first call */` |
|      - | 2461 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 2462 | `};` |
|      - | 2463 | `/*` |
|      - | 2464 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 2465 | ` * The following routine is invoked for each array entry passed` |
|      - | 2466 | ` * to the implode() function.` |
|      - | 2467 | ` */` |
|  83166 | 2468 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      2 | 2469 |  |
|  41583 | 2470 | `	SXUNUSED(pKey);` |
|  83168 | 2471 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 2472 | `	const char *zData;` |
|      - | 2473 | `	int nLen;` |
|  83168 | 2474 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 2475 | `		if( pData->nSeplen > 0 ){` |
|      3 | 2476 | `			if( !pData->bFirst ){` |
|      - | 2477 | `				/* append the separator first */` |
|      3 | 2478 | `				ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|      2 | 2479 | `			}else{` |
|    ! 0 | 2480 | `				pData->bFirst = 0;` |
|      - | 2481 | `			}` |
|      1 | 2482 | `		}` |
|      - | 2483 | `		/* Recurse */` |
|      3 | 2484 | `		pData->bFirst = 1;` |
|      3 | 2485 | `		pData->nRecCount++;` |
|      3 | 2486 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 2487 | `		pData->nRecCount--;` |
|      3 | 2488 | `		return PH7_OK;` |
|      - | 2489 | `	}` |
|      - | 2490 | `	/* Extract the string representation of the entry value */` |
|  83166 | 2491 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 2492 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
|  83166 | 2493 | `	if( pData->bFirst ){` |
|  19060 | 2494 | `		pData->bFirst = 0;` |
|  73637 | 2495 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 2496 | `		/* append the separator first */` |
|  64096 | 2497 | `		ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|  32047 | 2498 | `	}` |
|      - | 2499 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
|  83166 | 2500 | `	if( nLen > 0 ){` |
|  75702 | 2501 | `		ph7_result_string(pData->pCtx,zData,nLen);` |
|  37850 | 2502 | `	}` |
|  83166 | 2503 | `	return PH7_OK;` |
|  41585 | 2504 |  |
|      - | 2505 | `/*` |
|      - | 2506 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 2507 | ` * string implode(array $pieces,...)` |
|      - | 2508 | ` *  Join array elements with a string.` |
|      - | 2509 | ` * $glue` |
|      - | 2510 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 2511 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 2512 | ` * $pieces` |
|      - | 2513 | ` *   The array of strings to implode.` |
|      - | 2514 | ` * Return` |
|      - | 2515 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 2516 | ` *  order, with the glue string between each element.` |
|      - | 2517 | ` */` |
|  19086 | 2518 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2519 |  |
|      - | 2520 | `	struct implode_data imp_data;` |
|  19088 | 2521 | `	int i = 1;` |
|  19088 | 2522 | `	if( nArg < 1 ){` |
|      - | 2523 | `		/* Missing argument,return NULL */` |
|    ! 0 | 2524 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2525 | `		return PH7_OK;` |
|      - | 2526 | `	}` |
|      - | 2527 | `	/* Prepare the implode context */` |
|  19088 | 2528 | `	imp_data.pCtx = pCtx;` |
|  19088 | 2529 | `	imp_data.bRecursive = 0;` |
|  19088 | 2530 | `	imp_data.bFirst = 1;` |
|  19088 | 2531 | `	imp_data.nRecCount = 0;` |
|  19088 | 2532 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  19086 | 2533 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|   9544 | 2534 | `	}else{` |
|      3 | 2535 | `		imp_data.zSep = 0;` |
|      3 | 2536 | `		imp_data.nSeplen = 0;` |
|      3 | 2537 | `		i = 0;` |
|      - | 2538 | `	}` |
|  19088 | 2539 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 2540 | `	/* Start the 'join' process */` |
|  38174 | 2541 | `	while( i < nArg ){` |
|  19088 | 2542 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2543 | `			/* Iterate throw array entries */` |
|  19088 | 2544 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|   9545 | 2545 | `		}else{` |
|      - | 2546 | `			const char *zData;` |
|      - | 2547 | `			int nLen;` |
|      - | 2548 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 2549 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2550 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 2551 | `			if( imp_data.bFirst ){` |
|    ! 0 | 2552 | `				imp_data.bFirst = 0;` |
|    ! 0 | 2553 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2554 | `				ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen);` |
|    ! 0 | 2555 | `			}` |
|      - | 2556 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 2557 | `			if( nLen > 0 ){` |
|    ! 0 | 2558 | `				ph7_result_string(pCtx,zData,nLen);` |
|    ! 0 | 2559 | `			}` |
|      - | 2560 | `		}` |
|  19088 | 2561 | `		i++;` |
|      2 | 2562 | `	}` |
|  19088 | 2563 | `	return PH7_OK;` |
|   9545 | 2564 |  |
|      - | 2565 | `/*` |
|      - | 2566 | ` * Symisc eXtension:` |
|      - | 2567 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 2568 | ` * Purpose` |
|      - | 2569 | ` *  Same as implode() but recurse on arrays.` |
|      - | 2570 | ` * Example:` |
|      - | 2571 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 2572 | ` *   echo implode_recursive("/",$a);` |
|      - | 2573 | ` *   Will output` |
|      - | 2574 | ` *     usr/home/dean.` |
|      - | 2575 | ` *   While the standard implode would produce.` |
|      - | 2576 | ` *    usr/Array.` |
|      - | 2577 | ` * Parameter` |
|      - | 2578 | ` *  Refer to implode().` |
|      - | 2579 | ` * Return` |
|      - | 2580 | ` *  Refer to implode().` |
|      - | 2581 | ` */` |
|     12 | 2582 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2583 |  |
|      - | 2584 | `	struct implode_data imp_data;` |
|     13 | 2585 | `	int i = 1;` |
|     13 | 2586 | `	if( nArg < 1 ){` |
|      - | 2587 | `		/* Missing argument,return NULL */` |
|      3 | 2588 | `		ph7_result_null(pCtx);` |
|      3 | 2589 | `		return PH7_OK;` |
|      - | 2590 | `	}` |
|      - | 2591 | `	/* Prepare the implode context */` |
|     11 | 2592 | `	imp_data.pCtx = pCtx;` |
|     11 | 2593 | `	imp_data.bRecursive = 1;` |
|     11 | 2594 | `	imp_data.bFirst = 1;` |
|     11 | 2595 | `	imp_data.nRecCount = 0;` |
|     11 | 2596 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 2597 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 2598 | `	}else{` |
|    ! 0 | 2599 | `		imp_data.zSep = 0;` |
|    ! 0 | 2600 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 2601 | `		i = 0;` |
|      - | 2602 | `	}` |
|     11 | 2603 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 2604 | `	/* Start the 'join' process */` |
|     21 | 2605 | `	while( i < nArg ){` |
|     11 | 2606 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2607 | `			/* Iterate throw array entries */` |
|      3 | 2608 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      2 | 2609 | `		}else{` |
|      - | 2610 | `			const char *zData;` |
|      - | 2611 | `			int nLen;` |
|      - | 2612 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 2613 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2614 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 2615 | `			if( imp_data.bFirst ){` |
|      9 | 2616 | `				imp_data.bFirst = 0;` |
|      4 | 2617 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2618 | `				ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen);` |
|    ! 0 | 2619 | `			}` |
|      - | 2620 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 2621 | `			if( nLen > 0 ){` |
|      9 | 2622 | `				ph7_result_string(pCtx,zData,nLen);` |
|      4 | 2623 | `			}` |
|      - | 2624 | `		}` |
|     11 | 2625 | `		i++;` |
|      1 | 2626 | `	}` |
|     11 | 2627 | `	return PH7_OK;` |
|      7 | 2628 |  |
|      - | 2629 | `/*` |
|      - | 2630 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 2631 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 2632 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 2633 | ` * Parameters` |
|      - | 2634 | ` *  $delimiter` |
|      - | 2635 | ` *   The boundary string.` |
|      - | 2636 | ` * $string` |
|      - | 2637 | ` *   The input string.` |
|      - | 2638 | ` * $limit` |
|      - | 2639 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 2640 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 2641 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 2642 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 2643 | ` * Returns` |
|      - | 2644 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 2645 | ` *  on boundaries formed by the delimiter.` |
|      - | 2646 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 2647 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 2648 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 2649 | ` *  will be returned.` |
|      - | 2650 | ` * NOTE:` |
|      - | 2651 | ` *  Negative limit is not supported.` |
|      - | 2652 | ` */` |
|   3486 | 2653 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2654 |  |
|      - | 2655 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 2656 | `	int nDelim,nStrlen,iLimit;` |
|      - | 2657 | `	ph7_value *pArray;` |
|      - | 2658 | `	ph7_value *pValue;` |
|      - | 2659 | `	sxu32 nOfft;` |
|      - | 2660 | `	sxi32 rc;` |
|   3488 | 2661 | `	if( nArg < 2 ){` |
|      - | 2662 | `		/* Missing arguments,return FALSE */` |
|      9 | 2663 | `		ph7_result_bool(pCtx,0);` |
|      9 | 2664 | `		return PH7_OK;` |
|      - | 2665 | `	}` |
|      - | 2666 | `	/* Extract the delimiter */` |
|   3480 | 2667 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   3480 | 2668 | `	if( nDelim < 1 ){` |
|      - | 2669 | `		/* Empty delimiter,return FALSE */` |
|      3 | 2670 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2671 | `		return PH7_OK;` |
|      - | 2672 | `	}` |
|      - | 2673 | `	/* Extract the string */` |
|   3478 | 2674 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   3478 | 2675 | `	if( nStrlen < 1 ){` |
|      - | 2676 | `		/* Empty string: return an array with a single empty element (PHP behavior) */` |
|      3 | 2677 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      3 | 2678 | `		ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      3 | 2679 | `		if( pArrayTmp == 0 \|\| pValueTmp == 0 ){` |
|      - | 2680 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 2681 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2682 | `			return PH7_OK;` |
|      - | 2683 | `		}` |
|      3 | 2684 | `		ph7_value_string(pValueTmp, "", 0);` |
|      3 | 2685 | `		ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp);` |
|      3 | 2686 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      3 | 2687 | `		return PH7_OK;` |
|      - | 2688 | `	}` |
|      - | 2689 | `	/* Point to the end of the string */` |
|   3476 | 2690 | `	zEnd = &zString[nStrlen];` |
|      - | 2691 | `	/* Create the array */` |
|   3476 | 2692 | `	pArray =  ph7_context_new_array(pCtx);` |
|   3476 | 2693 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   3476 | 2694 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2695 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 2696 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2697 | `		return PH7_OK;` |
|      - | 2698 | `	}` |
|      - | 2699 | `	/* Set a defualt limit */` |
|   3476 | 2700 | `	iLimit = SXI32_HIGH;` |
|   3476 | 2701 | `	if( nArg > 2 ){` |
|      9 | 2702 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|      9 | 2703 | `		 if( iLimit < 0 ){` |
|      3 | 2704 | `			iLimit = -iLimit;` |
|      1 | 2705 | `		}` |
|      9 | 2706 | `		if( iLimit == 0 ){` |
|      3 | 2707 | `			iLimit = 1;` |
|      1 | 2708 | `		}` |
|      9 | 2709 | `		iLimit--;` |
|      4 | 2710 | `	}` |
|      - | 2711 | `	/* Start exploding */` |
|  40822 | 2712 | `	for(;;){` |
|  81646 | 2713 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
|  81646 | 2714 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 2715 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   3476 | 2716 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   3476 | 2717 | `			ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|   3476 | 2718 | `			break;` |
|      - | 2719 | `		}` |
|      - | 2720 | `		/* Point to the desired offset */` |
|  78172 | 2721 | `		zCur = &zString[nOfft];` |
|      - | 2722 | `		/* Perform the store operation (may be empty) */` |
|  78172 | 2723 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|  78172 | 2724 | `		ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|      - | 2725 | `		/* Point beyond the delimiter */` |
|  78172 | 2726 | `		zString = &zCur[nDelim];` |
|      - | 2727 | `		/* Reset the cursor */` |
|  78172 | 2728 | `		ph7_value_reset_string_cursor(pValue);` |
|      2 | 2729 | `	}` |
|      - | 2730 | `	/* Return the freshly created array */` |
|   3476 | 2731 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2732 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 2733 | `	 * released as soon we return from this foregin function.` |
|      - | 2734 | `	 */` |
|   3476 | 2735 | `	return PH7_OK;` |
|   1745 | 2736 |  |
|      - | 2737 | `/*` |
|      - | 2738 | ` * string trim(string $str[,string $charlist ])` |
|      - | 2739 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2740 | ` * Parameters` |
|      - | 2741 | ` *  $str` |
|      - | 2742 | ` *   The string that will be trimmed.` |
|      - | 2743 | ` * $charlist` |
|      - | 2744 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2745 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2746 | ` *   With .. you can specify a range of characters.` |
|      - | 2747 | ` * Returns.` |
|      - | 2748 | ` *  Thr processed string.` |
|      - | 2749 | ` * NOTE:` |
|      - | 2750 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 2751 | ` */` |
|   8526 | 2752 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2753 |  |
|      - | 2754 | `	const char *zString;` |
|      - | 2755 | `	int nLen;` |
|   8528 | 2756 | `	if( nArg < 1 ){` |
|      - | 2757 | `		/* Missing arguments,return null */` |
|      3 | 2758 | `		ph7_result_null(pCtx);` |
|      3 | 2759 | `		return PH7_OK;` |
|      - | 2760 | `	}` |
|      - | 2761 | `	/* Extract the target string */` |
|   8526 | 2762 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|   8526 | 2763 | `	if( nLen < 1 ){` |
|      - | 2764 | `		/* Empty string,return */` |
|   1612 | 2765 | `		ph7_result_string(pCtx,"",0);` |
|   1612 | 2766 | `		return PH7_OK;` |
|      - | 2767 | `	}` |
|      - | 2768 | `	/* Start the trim process */` |
|   6916 | 2769 | `	if( nArg < 2 ){` |
|      - | 2770 | `		SyString sStr;` |
|      - | 2771 | `		/* Remove white spaces and NUL bytes */` |
|   6912 | 2772 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  16730 | 2773 | `		SyStringFullTrimSafe(&sStr);` |
|   6912 | 2774 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   3457 | 2775 | `	}else{` |
|      - | 2776 | `		/* Char list */` |
|      - | 2777 | `		const char *zList;` |
|      - | 2778 | `		int nListlen;` |
|      5 | 2779 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 2780 | `		if( nListlen < 1 ){` |
|      - | 2781 | `			/* Return the string unchanged */` |
|      3 | 2782 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2783 | `		}else{` |
|      3 | 2784 | `			const char *zEnd = &zString[nLen];` |
|      3 | 2785 | `			const char *zCur = zString;` |
|      - | 2786 | `			const char *zPtr;` |
|      - | 2787 | `			int i;` |
|      - | 2788 | `			/* Left trim */` |
|      4 | 2789 | `			for(;;){` |
|      9 | 2790 | `				if( zCur >= zEnd ){` |
|    ! 0 | 2791 | `					break;` |
|      - | 2792 | `				}` |
|      9 | 2793 | `				zPtr = zCur;` |
|     17 | 2794 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 2795 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|      7 | 2796 | `						zCur++;` |
|      3 | 2797 | `					}` |
|      5 | 2798 | `				}` |
|      9 | 2799 | `				if( zCur == zPtr ){` |
|      - | 2800 | `					/* No match,break immediately */` |
|      3 | 2801 | `					break;` |
|      - | 2802 | `				}` |
|      1 | 2803 | `			}` |
|      - | 2804 | `			/* Right trim */` |
|      3 | 2805 | `			zEnd--;` |
|      4 | 2806 | `			for(;;){` |
|      9 | 2807 | `				if( zEnd <= zCur ){` |
|    ! 0 | 2808 | `					break;` |
|      - | 2809 | `				}` |
|      9 | 2810 | `				zPtr = zEnd;` |
|     17 | 2811 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 2812 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      7 | 2813 | `						zEnd--;` |
|      3 | 2814 | `					}` |
|      5 | 2815 | `				}` |
|      9 | 2816 | `				if( zEnd == zPtr ){` |
|      3 | 2817 | `					break;` |
|      - | 2818 | `				}` |
|      1 | 2819 | `			}` |
|      3 | 2820 | `			if( zCur >= zEnd ){` |
|      - | 2821 | `				/* Return the empty string */` |
|    ! 0 | 2822 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2823 | `			}else{` |
|      3 | 2824 | `				zEnd++;` |
|      3 | 2825 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2826 | `			}` |
|      - | 2827 | `		}` |
|      - | 2828 | `	}` |
|   6916 | 2829 | `	return PH7_OK;` |
|   4265 | 2830 |  |
|      - | 2831 | `/*` |
|      - | 2832 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 2833 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 2834 | ` * Parameters` |
|      - | 2835 | ` *  $str` |
|      - | 2836 | ` *   The string that will be trimmed.` |
|      - | 2837 | ` * $charlist` |
|      - | 2838 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2839 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2840 | ` *   With .. you can specify a range of characters.` |
|      - | 2841 | ` * Returns.` |
|      - | 2842 | ` *  Thr processed string.` |
|      - | 2843 | ` * NOTE:` |
|      - | 2844 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 2845 | ` */` |
|     26 | 2846 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2847 |  |
|      - | 2848 | `	const char *zString;` |
|      - | 2849 | `	int nLen;` |
|     27 | 2850 | `	if( nArg < 1 ){` |
|      - | 2851 | `		/* Missing arguments,return null */` |
|      3 | 2852 | `		ph7_result_null(pCtx);` |
|      3 | 2853 | `		return PH7_OK;` |
|      - | 2854 | `	}` |
|      - | 2855 | `	/* Extract the target string */` |
|     25 | 2856 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 2857 | `	if( nLen < 1 ){` |
|      - | 2858 | `		/* Empty string,return */` |
|      5 | 2859 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2860 | `		return PH7_OK;` |
|      - | 2861 | `	}` |
|      - | 2862 | `	/* Start the trim process */` |
|     21 | 2863 | `	if( nArg < 2 ){` |
|      - | 2864 | `		SyString sStr;` |
|      - | 2865 | `		/* Remove white spaces and NUL bytes*/` |
|     17 | 2866 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     41 | 2867 | `		SyStringRightTrimSafe(&sStr);` |
|     17 | 2868 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      9 | 2869 | `	}else{` |
|      - | 2870 | `		/* Char list */` |
|      - | 2871 | `		const char *zList;` |
|      - | 2872 | `		int nListlen;` |
|      5 | 2873 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 2874 | `		if( nListlen < 1 ){` |
|      - | 2875 | `			/* Return the string unchanged */` |
|    ! 0 | 2876 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 2877 | `		}else{` |
|      5 | 2878 | `			const char *zEnd = &zString[nLen - 1];` |
|      5 | 2879 | `			const char *zCur = zString;` |
|      - | 2880 | `			const char *zPtr;` |
|      - | 2881 | `			int i;` |
|      - | 2882 | `			/* Right trim */` |
|      6 | 2883 | `			for(;;){` |
|     13 | 2884 | `				if( zEnd <= zCur ){` |
|    ! 0 | 2885 | `					break;` |
|      - | 2886 | `				}` |
|     13 | 2887 | `				zPtr = zEnd;` |
|     25 | 2888 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     13 | 2889 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      9 | 2890 | `						zEnd--;` |
|      4 | 2891 | `					}` |
|      7 | 2892 | `				}` |
|     13 | 2893 | `				if( zEnd == zPtr ){` |
|      5 | 2894 | `					break;` |
|      - | 2895 | `				}` |
|      1 | 2896 | `			}` |
|      5 | 2897 | `			if( zEnd <= zCur ){` |
|      - | 2898 | `				/* Return the empty string */` |
|    ! 0 | 2899 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2900 | `			}else{` |
|      5 | 2901 | `				zEnd++;` |
|      5 | 2902 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2903 | `			}` |
|      - | 2904 | `		}` |
|      - | 2905 | `	}` |
|     21 | 2906 | `	return PH7_OK;` |
|     14 | 2907 |  |
|      - | 2908 | `/*` |
|      - | 2909 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 2910 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2911 | ` * Parameters` |
|      - | 2912 | ` *  $str` |
|      - | 2913 | ` *   The string that will be trimmed.` |
|      - | 2914 | ` * $charlist` |
|      - | 2915 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2916 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2917 | ` *   With .. you can specify a range of characters.` |
|      - | 2918 | ` * Returns.` |
|      - | 2919 | ` *  Thr processed string.` |
|      - | 2920 | ` * NOTE:` |
|      - | 2921 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 2922 | ` */` |
|     12 | 2923 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2924 |  |
|      - | 2925 | `	const char *zString;` |
|      - | 2926 | `	int nLen;` |
|     13 | 2927 | `	if( nArg < 1 ){` |
|      - | 2928 | `		/* Missing arguments,return null */` |
|      3 | 2929 | `		ph7_result_null(pCtx);` |
|      3 | 2930 | `		return PH7_OK;` |
|      - | 2931 | `	}` |
|      - | 2932 | `	/* Extract the target string */` |
|     11 | 2933 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 2934 | `	if( nLen < 1 ){` |
|      - | 2935 | `		/* Empty string,return */` |
|    ! 0 | 2936 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2937 | `		return PH7_OK;` |
|      - | 2938 | `	}` |
|      - | 2939 | `	/* Start the trim process */` |
|     11 | 2940 | `	if( nArg < 2 ){` |
|      - | 2941 | `		SyString sStr;` |
|      - | 2942 | `		/* Remove white spaces and NUL byte */` |
|      3 | 2943 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|      8 | 2944 | `		SyStringLeftTrimSafe(&sStr);` |
|      3 | 2945 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      2 | 2946 | `	}else{` |
|      - | 2947 | `		/* Char list */` |
|      - | 2948 | `		const char *zList;` |
|      - | 2949 | `		int nListlen;` |
|      9 | 2950 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      9 | 2951 | `		if( nListlen < 1 ){` |
|      - | 2952 | `			/* Return the string unchanged */` |
|      3 | 2953 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2954 | `		}else{` |
|      7 | 2955 | `			const char *zEnd = &zString[nLen];` |
|      7 | 2956 | `			const char *zCur = zString;` |
|      - | 2957 | `			const char *zPtr;` |
|      - | 2958 | `			int i;` |
|      - | 2959 | `			/* Left trim */` |
|      7 | 2960 | `			for(;;){` |
|     15 | 2961 | `				if( zCur >= zEnd ){` |
|    ! 0 | 2962 | `					break;` |
|      - | 2963 | `				}` |
|     15 | 2964 | `				zPtr = zCur;` |
|     41 | 2965 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     27 | 2966 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|     13 | 2967 | `						zCur++;` |
|      6 | 2968 | `					}` |
|     14 | 2969 | `				}` |
|     15 | 2970 | `				if( zCur == zPtr ){` |
|      - | 2971 | `					/* No match,break immediately */` |
|      7 | 2972 | `					break;` |
|      - | 2973 | `				}` |
|      1 | 2974 | `			}` |
|      7 | 2975 | `			if( zCur >= zEnd ){` |
|      - | 2976 | `				/* Return the empty string */` |
|    ! 0 | 2977 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2978 | `			}else{` |
|      7 | 2979 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2980 | `			}` |
|      - | 2981 | `		}` |
|      - | 2982 | `	}` |
|     11 | 2983 | `	return PH7_OK;` |
|      7 | 2984 |  |
|      - | 2985 | `/*` |
|      - | 2986 | ` * string strtolower(string $str)` |
|      - | 2987 | ` *  Make a string lowercase.` |
|      - | 2988 | ` * Parameters` |
|      - | 2989 | ` *  $str` |
|      - | 2990 | ` *   The input string.` |
|      - | 2991 | ` * Returns.` |
|      - | 2992 | ` *  The lowercased string.` |
|      - | 2993 | ` */` |
|  18890 | 2994 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2995 |  |
|      - | 2996 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2997 | `	int nLen;` |
|  18892 | 2998 | `	if( nArg < 1 ){` |
|      - | 2999 | `		/* Missing arguments,return null */` |
|      3 | 3000 | `		ph7_result_null(pCtx);` |
|      3 | 3001 | `		return PH7_OK;` |
|      - | 3002 | `	}` |
|      - | 3003 | `	/* Extract the target string */` |
|  18890 | 3004 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  18890 | 3005 | `	if( nLen < 1 ){` |
|      - | 3006 | `		/* Empty string,return */` |
|      3 | 3007 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3008 | `		return PH7_OK;` |
|      - | 3009 | `	}` |
|      - | 3010 | `	/* Perform the requested operation */` |
|  18888 | 3011 | `	zEnd = &zString[nLen];` |
|  59687 | 3012 | `	for(;;){` |
| 119376 | 3013 | `		if( zString >= zEnd ){` |
|      - | 3014 | `			/* No more input,break immediately */` |
|  18888 | 3015 | `			break;` |
|      - | 3016 | `		}` |
| 100490 | 3017 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 3018 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 3019 | `			zCur = zString;` |
|    ! 0 | 3020 | `			zString++;` |
|    ! 0 | 3021 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 3022 | `				zString++;` |
|    ! 0 | 3023 | `			}` |
|      - | 3024 | `			/* Append UTF-8 stream */` |
|    ! 0 | 3025 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 3026 | `		}else{` |
| 100490 | 3027 | `			int c = zString[0];` |
| 100490 | 3028 | `			if( SyisUpper(c) ){` |
| 100488 | 3029 | `				c = SyToLower(zString[0]);` |
|  50243 | 3030 | `			}` |
|      - | 3031 | `			/* Append character */` |
| 100490 | 3032 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 3033 | `			/* Advance the cursor */` |
| 100490 | 3034 | `			zString++;` |
|      - | 3035 | `		}` |
|      2 | 3036 | `	}` |
|  18888 | 3037 | `	return PH7_OK;` |
|   9447 | 3038 |  |
|      - | 3039 | `/*` |
|      - | 3040 | ` * string strtolower(string $str)` |
|      - | 3041 | ` *  Make a string uppercase.` |
|      - | 3042 | ` * Parameters` |
|      - | 3043 | ` *  $str` |
|      - | 3044 | ` *   The input string.` |
|      - | 3045 | ` * Returns.` |
|      - | 3046 | ` *  The uppercased string.` |
|      - | 3047 | ` */` |
|     10 | 3048 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3049 |  |
|      - | 3050 | `	const char *zString,*zCur,*zEnd;` |
|      - | 3051 | `	int nLen;` |
|     11 | 3052 | `	if( nArg < 1 ){` |
|      - | 3053 | `		/* Missing arguments,return null */` |
|      3 | 3054 | `		ph7_result_null(pCtx);` |
|      3 | 3055 | `		return PH7_OK;` |
|      - | 3056 | `	}` |
|      - | 3057 | `	/* Extract the target string */` |
|      9 | 3058 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 3059 | `	if( nLen < 1 ){` |
|      - | 3060 | `		/* Empty string,return */` |
|      3 | 3061 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3062 | `		return PH7_OK;` |
|      - | 3063 | `	}` |
|      - | 3064 | `	/* Perform the requested operation */` |
|      7 | 3065 | `	zEnd = &zString[nLen];` |
|     19 | 3066 | `	for(;;){` |
|     39 | 3067 | `		if( zString >= zEnd ){` |
|      - | 3068 | `			/* No more input,break immediately */` |
|      7 | 3069 | `			break;` |
|      - | 3070 | `		}` |
|     33 | 3071 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 3072 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 3073 | `			zCur = zString;` |
|    ! 0 | 3074 | `			zString++;` |
|    ! 0 | 3075 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 3076 | `				zString++;` |
|    ! 0 | 3077 | `			}` |
|      - | 3078 | `			/* Append UTF-8 stream */` |
|    ! 0 | 3079 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 3080 | `		}else{` |
|     33 | 3081 | `			int c = zString[0];` |
|     33 | 3082 | `			if( SyisLower(c) ){` |
|     27 | 3083 | `				c = SyToUpper(zString[0]);` |
|     13 | 3084 | `			}` |
|      - | 3085 | `			/* Append character */` |
|     33 | 3086 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 3087 | `			/* Advance the cursor */` |
|     33 | 3088 | `			zString++;` |
|      - | 3089 | `		}` |
|      1 | 3090 | `	}` |
|      7 | 3091 | `	return PH7_OK;` |
|      6 | 3092 |  |
|      - | 3093 | `/*` |
|      - | 3094 | ` * string ucfirst(string $str)` |
|      - | 3095 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 3096 | ` *  character is alphabetic.` |
|      - | 3097 | ` * Parameters` |
|      - | 3098 | ` *  $str` |
|      - | 3099 | ` *   The input string.` |
|      - | 3100 | ` * Returns.` |
|      - | 3101 | ` *  The processed string.` |
|      - | 3102 | ` */` |
|      6 | 3103 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3104 |  |
|      - | 3105 | `	const char *zString,*zEnd;` |
|      - | 3106 | `	int nLen,c;` |
|      7 | 3107 | `	if( nArg < 1 ){` |
|      - | 3108 | `		/* Missing arguments,return null */` |
|      3 | 3109 | `		ph7_result_null(pCtx);` |
|      3 | 3110 | `		return PH7_OK;` |
|      - | 3111 | `	}` |
|      - | 3112 | `	/* Extract the target string */` |
|      5 | 3113 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3114 | `	if( nLen < 1 ){` |
|      - | 3115 | `		/* Empty string,return */` |
|      3 | 3116 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3117 | `		return PH7_OK;` |
|      - | 3118 | `	}` |
|      - | 3119 | `	/* Perform the requested operation */` |
|      3 | 3120 | `	zEnd = &zString[nLen];` |
|      3 | 3121 | `	c = zString[0];` |
|      3 | 3122 | `	if( SyisLower(c) ){` |
|      3 | 3123 | `		c = SyToUpper(c);` |
|      1 | 3124 | `	}` |
|      - | 3125 | `	/* Append the first character */` |
|      3 | 3126 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 3127 | `	zString++;` |
|      3 | 3128 | `	if( zString < zEnd ){` |
|      - | 3129 | `		/* Append the rest of the input verbatim */` |
|      3 | 3130 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 3131 | `	}` |
|      3 | 3132 | `	return PH7_OK;` |
|      4 | 3133 |  |
|      - | 3134 | `/*` |
|      - | 3135 | ` * string lcfirst(string $str)` |
|      - | 3136 | ` *  Make a string's first character lowercase.` |
|      - | 3137 | ` * Parameters` |
|      - | 3138 | ` *  $str` |
|      - | 3139 | ` *   The input string.` |
|      - | 3140 | ` * Returns.` |
|      - | 3141 | ` *  The processed string.` |
|      - | 3142 | ` */` |
|      6 | 3143 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3144 |  |
|      - | 3145 | `	const char *zString,*zEnd;` |
|      - | 3146 | `	int nLen,c;` |
|      7 | 3147 | `	if( nArg < 1 ){` |
|      - | 3148 | `		/* Missing arguments,return null */` |
|      3 | 3149 | `		ph7_result_null(pCtx);` |
|      3 | 3150 | `		return PH7_OK;` |
|      - | 3151 | `	}` |
|      - | 3152 | `	/* Extract the target string */` |
|      5 | 3153 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3154 | `	if( nLen < 1 ){` |
|      - | 3155 | `		/* Empty string,return */` |
|      3 | 3156 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3157 | `		return PH7_OK;` |
|      - | 3158 | `	}` |
|      - | 3159 | `	/* Perform the requested operation */` |
|      3 | 3160 | `	zEnd = &zString[nLen];` |
|      3 | 3161 | `	c = zString[0];` |
|      3 | 3162 | `	if( SyisUpper(c) ){` |
|      3 | 3163 | `		c = SyToLower(c);` |
|      1 | 3164 | `	}` |
|      - | 3165 | `	/* Append the first character */` |
|      3 | 3166 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 3167 | `	zString++;` |
|      3 | 3168 | `	if( zString < zEnd ){` |
|      - | 3169 | `		/* Append the rest of the input verbatim */` |
|      3 | 3170 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 3171 | `	}` |
|      3 | 3172 | `	return PH7_OK;` |
|      4 | 3173 |  |
|      - | 3174 | `/*` |
|      - | 3175 | ` * int ord(string $string)` |
|      - | 3176 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 3177 | ` * Parameters` |
|      - | 3178 | ` *  $str` |
|      - | 3179 | ` *   The input string.` |
|      - | 3180 | ` * Returns.` |
|      - | 3181 | ` *  The ASCII value as an integer.` |
|      - | 3182 | ` */` |
|     32 | 3183 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3184 |  |
|      - | 3185 | `	const char *zString;` |
|      - | 3186 | `	int nLen,c;` |
|     33 | 3187 | `	if( nArg < 1 ){` |
|      - | 3188 | `		/* Missing arguments,return -1 */` |
|      3 | 3189 | `		ph7_result_int(pCtx,-1);` |
|      3 | 3190 | `		return PH7_OK;` |
|      - | 3191 | `	}` |
|      - | 3192 | `	/* Extract the target string */` |
|     31 | 3193 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 3194 | `	if( nLen < 1 ){` |
|      - | 3195 | `		/* Empty string,return -1 */` |
|      3 | 3196 | `		ph7_result_int(pCtx,-1);` |
|      3 | 3197 | `		return PH7_OK;` |
|      - | 3198 | `	}` |
|      - | 3199 | `	/* Extract the ASCII value of the first character */` |
|     29 | 3200 | `	c = zString[0];` |
|      - | 3201 | `	/* Return that value */` |
|     29 | 3202 | `	ph7_result_int(pCtx,c);` |
|     29 | 3203 | `	return PH7_OK;` |
|     17 | 3204 |  |
|      - | 3205 | `/*` |
|      - | 3206 | ` * string chr(int $ascii)` |
|      - | 3207 | ` *  Returns a one-character string containing the character specified by ascii.` |
|      - | 3208 | ` * Parameters` |
|      - | 3209 | ` *  $ascii` |
|      - | 3210 | ` *   The ascii code.` |
|      - | 3211 | ` * Returns.` |
|      - | 3212 | ` *  The specified character.` |
|      - | 3213 | ` */` |
|     28 | 3214 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3215 |  |
|      - | 3216 | `	int c;` |
|     29 | 3217 | `	if( nArg < 1 ){` |
|      - | 3218 | `		/* Missing arguments,return null */` |
|      3 | 3219 | `		ph7_result_null(pCtx);` |
|      3 | 3220 | `		return PH7_OK;` |
|      - | 3221 | `	}` |
|      - | 3222 | `	/* Extract the ASCII value */` |
|     27 | 3223 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 3224 | `	/* Return the specified character */` |
|     27 | 3225 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     27 | 3226 | `	return PH7_OK;` |
|     15 | 3227 |  |
|      - | 3228 | `/*` |
|      - | 3229 | ` * Binary to hex consumer callback.` |
|      - | 3230 | ` * This callback is the default consumer used by the hash functions` |
|      - | 3231 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 3232 | ` */` |
|    226 | 3233 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 3234 |  |
|      - | 3235 | `	/* Append hex chunk verbatim */` |
|    227 | 3236 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|    227 | 3237 | `	return SXRET_OK;` |
|      1 | 3238 |  |
|      - | 3239 |  |
|      - | 3240 | `/*` |
|      - | 3241 | ` * string bin2hex(string $str)` |
|      - | 3242 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 3243 | ` * Parameters` |
|      - | 3244 | ` *  $str` |
|      - | 3245 | ` *   The input string.` |
|      - | 3246 | ` * Returns.` |
|      - | 3247 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 3248 | ` */` |
|     12 | 3249 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3250 |  |
|      - | 3251 | `	const char *zString;` |
|      - | 3252 | `	int nLen;` |
|     13 | 3253 | `	if( nArg < 1 ){` |
|      - | 3254 | `		/* Missing arguments,return null */` |
|      3 | 3255 | `		ph7_result_null(pCtx);` |
|      3 | 3256 | `		return PH7_OK;` |
|      - | 3257 | `	}` |
|      - | 3258 | `	/* Extract the target string */` |
|     11 | 3259 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 3260 | `	if( nLen < 1 ){` |
|      - | 3261 | `		/* Empty string,return */` |
|      3 | 3262 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3263 | `		return PH7_OK;` |
|      - | 3264 | `	}` |
|      - | 3265 | `	/* Perform the requested operation */` |
|      9 | 3266 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|      9 | 3267 | `	return PH7_OK;` |
|      7 | 3268 |  |
|      - | 3269 |  |
|      - | 3270 | `/* Search callback signature */` |
|      - | 3271 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 3272 | `/*` |
|      - | 3273 | ` * Case-insensitive pattern match.` |
|      - | 3274 | ` * Brute force is the default search method used here.` |
|      - | 3275 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 3276 | ` * well for short/medium texts on modern hardware.` |
|      - | 3277 | ` */` |
|    118 | 3278 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      1 | 3279 |  |
|    119 | 3280 | `	const char *zpIn = (const char *)pPattern;` |
|    119 | 3281 | `	const char *zIn = (const char *)pText;` |
|    119 | 3282 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    119 | 3283 | `	const char *zEnd = &zIn[nLen];` |
|      - | 3284 | `	const char *zPtr,*zPtr2;` |
|      - | 3285 | `	int c,d;` |
|    119 | 3286 | `	if( iPatLen > nLen ){` |
|      - | 3287 | `		/* Don't bother processing */` |
|     33 | 3288 | `		return SXERR_NOTFOUND;` |
|      - | 3289 | `	}` |
|    244 | 3290 | `	for(;;){` |
|    489 | 3291 | `		if( zIn >= zEnd ){` |
|     47 | 3292 | `			break;` |
|      - | 3293 | `		}` |
|    443 | 3294 | `		c = SyToLower(zIn[0]);` |
|    443 | 3295 | `		d = SyToLower(zpIn[0]);` |
|    443 | 3296 | `		if( c == d ){` |
|     41 | 3297 | `			zPtr   = &zIn[1];` |
|     41 | 3298 | `			zPtr2  = &zpIn[1];` |
|     71 | 3299 | `			for(;;){` |
|    143 | 3300 | `				if( zPtr2 >= zpEnd ){` |
|      - | 3301 | `					/* Pattern found */` |
|     41 | 3302 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 3303 | `					return SXRET_OK;` |
|      - | 3304 | `				}` |
|    103 | 3305 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 3306 | `					break;` |
|      - | 3307 | `				}` |
|    103 | 3308 | `				c = SyToLower(zPtr[0]);` |
|    103 | 3309 | `				d = SyToLower(zPtr2[0]);` |
|    103 | 3310 | `				if( c != d ){` |
|    ! 0 | 3311 | `					break;` |
|      - | 3312 | `				}` |
|    103 | 3313 | `				zPtr++; zPtr2++;` |
|      1 | 3314 | `			}` |
|    ! 0 | 3315 | `		}` |
|    403 | 3316 | `		zIn++;` |
|      1 | 3317 | `	}` |
|      - | 3318 | `	/* Pattern not found */` |
|     47 | 3319 | `	return SXERR_NOTFOUND;` |
|     60 | 3320 |  |
|      - | 3321 | `/*` |
|      - | 3322 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3323 | ` *  Find the first occurrence of a string.` |
|      - | 3324 | ` * Parameters` |
|      - | 3325 | ` *  $haystack` |
|      - | 3326 | ` *   The input string.` |
|      - | 3327 | ` * $needle` |
|      - | 3328 | ` *   Search pattern (must be a string).` |
|      - | 3329 | ` * $before_needle` |
|      - | 3330 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3331 | ` *   of the needle (excluding the needle).` |
|      - | 3332 | ` * Return` |
|      - | 3333 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3334 | ` */` |
|     10 | 3335 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3336 |  |
|     11 | 3337 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3338 | `	const char *zBlob,*zPattern;` |
|      - | 3339 | `	int nLen,nPatLen;` |
|      - | 3340 | `	sxu32 nOfft;` |
|      - | 3341 | `	sxi32 rc;` |
|     11 | 3342 | `	if( nArg < 2 ){` |
|      - | 3343 | `		/* Missing arguments,return FALSE */` |
|      5 | 3344 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3345 | `		return PH7_OK;` |
|      - | 3346 | `	}` |
|      - | 3347 | `	/* Extract the needle and the haystack */` |
|      7 | 3348 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 3349 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 3350 | `	nOfft = 0; /* cc warning */` |
|      9 | 3351 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3352 | `		int before = 0;` |
|      - | 3353 | `		/* Perform the lookup */` |
|      5 | 3354 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3355 | `		if( rc != SXRET_OK ){` |
|      - | 3356 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3357 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3358 | `			return PH7_OK;` |
|      - | 3359 | `		}` |
|      - | 3360 | `		/* Return the portion of the string */` |
|      5 | 3361 | `		if( nArg > 2 ){` |
|      3 | 3362 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3363 | `		}` |
|      5 | 3364 | `		if( before ){` |
|      3 | 3365 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3366 | `		}else{` |
|      3 | 3367 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3368 | `		}` |
|      3 | 3369 | `	}else{` |
|      3 | 3370 | `		ph7_result_bool(pCtx,0);` |
|      - | 3371 | `	}` |
|      7 | 3372 | `	return PH7_OK;` |
|      6 | 3373 |  |
|      - | 3374 | `/*` |
|      - | 3375 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3376 | ` *  Case-insensitive strstr().` |
|      - | 3377 | ` * Parameters` |
|      - | 3378 | ` *  $haystack` |
|      - | 3379 | ` *   The input string.` |
|      - | 3380 | ` * $needle` |
|      - | 3381 | ` *   Search pattern (must be a string).` |
|      - | 3382 | ` * $before_needle` |
|      - | 3383 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3384 | ` *   of the needle (excluding the needle).` |
|      - | 3385 | ` * Return` |
|      - | 3386 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3387 | ` */` |
|      6 | 3388 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3389 |  |
|      7 | 3390 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3391 | `	const char *zBlob,*zPattern;` |
|      - | 3392 | `	int nLen,nPatLen;` |
|      - | 3393 | `	sxu32 nOfft;` |
|      - | 3394 | `	sxi32 rc;` |
|      7 | 3395 | `	if( nArg < 2 ){` |
|      - | 3396 | `		/* Missing arguments,return FALSE */` |
|      3 | 3397 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3398 | `		return PH7_OK;` |
|      - | 3399 | `	}` |
|      - | 3400 | `	/* Extract the needle and the haystack */` |
|      5 | 3401 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3402 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 3403 | `	nOfft = 0; /* cc warning */` |
|      7 | 3404 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3405 | `		int before = 0;` |
|      - | 3406 | `		/* Perform the lookup */` |
|      5 | 3407 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3408 | `		if( rc != SXRET_OK ){` |
|      - | 3409 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3410 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3411 | `			return PH7_OK;` |
|      - | 3412 | `		}` |
|      - | 3413 | `		/* Return the portion of the string */` |
|      5 | 3414 | `		if( nArg > 2 ){` |
|      3 | 3415 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3416 | `		}` |
|      5 | 3417 | `		if( before ){` |
|      3 | 3418 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3419 | `		}else{` |
|      3 | 3420 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3421 | `		}` |
|      3 | 3422 | `	}else{` |
|    ! 0 | 3423 | `		ph7_result_bool(pCtx,0);` |
|      - | 3424 | `	}` |
|      5 | 3425 | `	return PH7_OK;` |
|      4 | 3426 |  |
|      - | 3427 | `/*` |
|      - | 3428 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3429 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 3430 | ` * Parameters` |
|      - | 3431 | ` *  $haystack` |
|      - | 3432 | ` *   The input string.` |
|      - | 3433 | ` * $needle` |
|      - | 3434 | ` *   Search pattern (must be a string).` |
|      - | 3435 | ` * $offset` |
|      - | 3436 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3437 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3438 | ` *   of haystack.` |
|      - | 3439 | ` * Return` |
|      - | 3440 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3441 | ` */` |
|     80 | 3442 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3443 |  |
|     82 | 3444 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3445 | `	const char *zBlob,*zPattern;` |
|      - | 3446 | `	int nLen,nPatLen,nStart;` |
|      - | 3447 | `	sxu32 nOfft;` |
|      - | 3448 | `	sxi32 rc;` |
|     82 | 3449 | `	if( nArg < 2 ){` |
|      - | 3450 | `		/* Missing arguments,return FALSE */` |
|      7 | 3451 | `		ph7_result_bool(pCtx,0);` |
|      7 | 3452 | `		return PH7_OK;` |
|      - | 3453 | `	}` |
|      - | 3454 | `	/* Extract the needle and the haystack */` |
|     76 | 3455 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     76 | 3456 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     76 | 3457 | `	nOfft = 0; /* cc warning */` |
|     76 | 3458 | `	nStart = 0;` |
|      - | 3459 | `	/* Peek the starting offset if available */` |
|     76 | 3460 | `	if( nArg > 2 ){` |
|    ! 0 | 3461 | `		nStart = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 3462 | `		if( nStart < 0 ){` |
|    ! 0 | 3463 | `			nStart = -nStart;` |
|    ! 0 | 3464 | `		}` |
|    ! 0 | 3465 | `		if( nStart >= nLen ){` |
|      - | 3466 | `			/* Invalid offset */` |
|    ! 0 | 3467 | `			nStart = 0;` |
|    ! 0 | 3468 | `		}else{` |
|    ! 0 | 3469 | `			zBlob += nStart;` |
|    ! 0 | 3470 | `			nLen -= nStart;` |
|      - | 3471 | `		}` |
|    ! 0 | 3472 | `	}` |
|     76 | 3473 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3474 | `		/* Perform the lookup */` |
|     74 | 3475 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     74 | 3476 | `		if( rc != SXRET_OK ){` |
|      - | 3477 | `			/* Pattern not found,return FALSE */` |
|      5 | 3478 | `			ph7_result_bool(pCtx,0);` |
|      5 | 3479 | `			return PH7_OK;` |
|      - | 3480 | `		}` |
|      - | 3481 | `		/* Return the pattern position */` |
|     70 | 3482 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|     36 | 3483 | `	}else{` |
|      3 | 3484 | `		ph7_result_bool(pCtx,0);` |
|      - | 3485 | `	}` |
|     72 | 3486 | `	return PH7_OK;` |
|     42 | 3487 |  |
|      - | 3488 | `/*` |
|      - | 3489 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3490 | ` *  Case-insensitive strpos.` |
|      - | 3491 | ` * Parameters` |
|      - | 3492 | ` *  $haystack` |
|      - | 3493 | ` *   The input string.` |
|      - | 3494 | ` * $needle` |
|      - | 3495 | ` *   Search pattern (must be a string).` |
|      - | 3496 | ` * $offset` |
|      - | 3497 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3498 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3499 | ` *   of haystack.` |
|      - | 3500 | ` * Return` |
|      - | 3501 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3502 | ` */` |
|     18 | 3503 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3504 |  |
|     19 | 3505 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3506 | `	const char *zBlob,*zPattern;` |
|      - | 3507 | `	int nLen,nPatLen,nStart;` |
|      - | 3508 | `	sxu32 nOfft;` |
|      - | 3509 | `	sxi32 rc;` |
|     19 | 3510 | `	if( nArg < 2 ){` |
|      - | 3511 | `		/* Missing arguments,return FALSE */` |
|      3 | 3512 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3513 | `		return PH7_OK;` |
|      - | 3514 | `	}` |
|      - | 3515 | `	/* Extract the needle and the haystack */` |
|     17 | 3516 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 3517 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     17 | 3518 | `	nOfft = 0; /* cc warning */` |
|     17 | 3519 | `	nStart = 0;` |
|      - | 3520 | `	/* Peek the starting offset if available */` |
|     17 | 3521 | `	if( nArg > 2 ){` |
|      5 | 3522 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 3523 | `		if( nStart < 0 ){` |
|      3 | 3524 | `			nStart = -nStart;` |
|      1 | 3525 | `		}` |
|      5 | 3526 | `		if( nStart >= nLen ){` |
|      - | 3527 | `			/* Invalid offset */` |
|    ! 0 | 3528 | `			nStart = 0;` |
|    ! 0 | 3529 | `		}else{` |
|      5 | 3530 | `			zBlob += nStart;` |
|      5 | 3531 | `			nLen -= nStart;` |
|      - | 3532 | `		}` |
|      2 | 3533 | `	}` |
|     17 | 3534 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3535 | `		/* Perform the lookup */` |
|     17 | 3536 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     17 | 3537 | `		if( rc != SXRET_OK ){` |
|      - | 3538 | `			/* Pattern not found,return FALSE */` |
|      3 | 3539 | `			ph7_result_bool(pCtx,0);` |
|      3 | 3540 | `			return PH7_OK;` |
|      - | 3541 | `		}` |
|      - | 3542 | `		/* Return the pattern position */` |
|     15 | 3543 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 3544 | `	}else{` |
|    ! 0 | 3545 | `		ph7_result_bool(pCtx,0);` |
|      - | 3546 | `	}` |
|     15 | 3547 | `	return PH7_OK;` |
|     10 | 3548 |  |
|      - | 3549 | `/*` |
|      - | 3550 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3551 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 3552 | ` * Parameters` |
|      - | 3553 | ` *  $haystack` |
|      - | 3554 | ` *   The input string.` |
|      - | 3555 | ` * $needle` |
|      - | 3556 | ` *   Search pattern (must be a string).` |
|      - | 3557 | ` * $offset` |
|      - | 3558 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3559 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3560 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3561 | ` * Return` |
|      - | 3562 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3563 | ` */` |
|     32 | 3564 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3565 |  |
|      - | 3566 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     33 | 3567 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3568 | `	int nLen,nPatLen;` |
|      - | 3569 | `	sxu32 nOfft;` |
|      - | 3570 | `	sxi32 rc;` |
|     33 | 3571 | `	if( nArg < 2 ){` |
|      - | 3572 | `		/* Missing arguments,return FALSE */` |
|      3 | 3573 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3574 | `		return PH7_OK;` |
|      - | 3575 | `	}` |
|      - | 3576 | `	/* Extract the needle and the haystack */` |
|     31 | 3577 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 3578 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3579 | `	/* Point to the end of the pattern */` |
|     31 | 3580 | `	zPtr = &zBlob[nLen - 1];` |
|     31 | 3581 | `	zEnd = &zBlob[nLen];` |
|      - | 3582 | `	/* Save the starting posistion */` |
|     31 | 3583 | `	zStart = zBlob;` |
|     31 | 3584 | `	nOfft = 0; /* cc warning */` |
|      - | 3585 | `	/* Peek the starting offset if available */` |
|     31 | 3586 | `	if( nArg > 2 ){` |
|      - | 3587 | `		int nStart;` |
|     21 | 3588 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 3589 | `		if( nStart < 0 ){` |
|     11 | 3590 | `			nStart = -nStart;` |
|     11 | 3591 | `			if( nStart >= nLen ){` |
|      - | 3592 | `				/* Invalid offset */` |
|      3 | 3593 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3594 | `				return PH7_OK;` |
|    ! 0 | 3595 | `			}else{` |
|      9 | 3596 | `				nLen -= nStart;` |
|      9 | 3597 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 3598 | `				zEnd = &zBlob[nLen];` |
|      - | 3599 | `			}` |
|      5 | 3600 | `		}else{` |
|     11 | 3601 | `			if( nStart >= nLen ){` |
|      - | 3602 | `				/* Invalid offset */` |
|      5 | 3603 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3604 | `				return PH7_OK;` |
|    ! 0 | 3605 | `			}else{` |
|      7 | 3606 | `				zBlob += nStart;` |
|      7 | 3607 | `				nLen -= nStart;` |
|      - | 3608 | `			}` |
|      - | 3609 | `		}` |
|      7 | 3610 | `	}` |
|     25 | 3611 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3612 | `		/* Perform the lookup */` |
|     57 | 3613 | `		for(;;){` |
|    115 | 3614 | `			if( zBlob >= zPtr ){` |
|     11 | 3615 | `				break;` |
|      - | 3616 | `			}` |
|    105 | 3617 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    105 | 3618 | `			if( rc == SXRET_OK ){` |
|      - | 3619 | `				/* Pattern found,return it's position */` |
|     13 | 3620 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 3621 | `				return PH7_OK;` |
|      - | 3622 | `			}` |
|     93 | 3623 | `			zPtr--;` |
|      1 | 3624 | `		}` |
|      - | 3625 | `		/* Pattern not found,return FALSE */` |
|     11 | 3626 | `		ph7_result_bool(pCtx,0);` |
|      6 | 3627 | `	}else{` |
|      3 | 3628 | `		ph7_result_bool(pCtx,0);` |
|      - | 3629 | `	}` |
|     13 | 3630 | `	return PH7_OK;` |
|     17 | 3631 |  |
|      - | 3632 | `/*` |
|      - | 3633 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3634 | ` *  Case-insensitive strrpos.` |
|      - | 3635 | ` * Parameters` |
|      - | 3636 | ` *  $haystack` |
|      - | 3637 | ` *   The input string.` |
|      - | 3638 | ` * $needle` |
|      - | 3639 | ` *   Search pattern (must be a string).` |
|      - | 3640 | ` * $offset` |
|      - | 3641 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3642 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3643 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3644 | ` * Return` |
|      - | 3645 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3646 | ` */` |
|     28 | 3647 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3648 |  |
|      - | 3649 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     29 | 3650 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3651 | `	int nLen,nPatLen;` |
|      - | 3652 | `	sxu32 nOfft;` |
|      - | 3653 | `	sxi32 rc;` |
|     29 | 3654 | `	if( nArg < 2 ){` |
|      - | 3655 | `		/* Missing arguments,return FALSE */` |
|      3 | 3656 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3657 | `		return PH7_OK;` |
|      - | 3658 | `	}` |
|      - | 3659 | `	/* Extract the needle and the haystack */` |
|     27 | 3660 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 3661 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3662 | `	/* Point to the end of the pattern */` |
|     27 | 3663 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 3664 | `	zEnd = &zBlob[nLen];` |
|      - | 3665 | `	/* Save the starting posistion */` |
|     27 | 3666 | `	zStart = zBlob;` |
|     27 | 3667 | `	nOfft = 0; /* cc warning */` |
|      - | 3668 | `	/* Peek the starting offset if available */` |
|     27 | 3669 | `	if( nArg > 2 ){` |
|      - | 3670 | `		int nStart;` |
|     15 | 3671 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3672 | `		if( nStart < 0 ){` |
|      7 | 3673 | `			nStart = -nStart;` |
|      7 | 3674 | `			if( nStart >= nLen ){` |
|      - | 3675 | `				/* Invalid offset */` |
|      3 | 3676 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3677 | `				return PH7_OK;` |
|    ! 0 | 3678 | `			}else{` |
|      5 | 3679 | `				nLen -= nStart;` |
|      5 | 3680 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 3681 | `				zEnd = &zBlob[nLen];` |
|      - | 3682 | `			}` |
|      3 | 3683 | `		}else{` |
|      9 | 3684 | `			if( nStart >= nLen ){` |
|      - | 3685 | `				/* Invalid offset */` |
|      5 | 3686 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3687 | `				return PH7_OK;` |
|    ! 0 | 3688 | `			}else{` |
|      5 | 3689 | `				zBlob += nStart;` |
|      5 | 3690 | `				nLen -= nStart;` |
|      - | 3691 | `			}` |
|      - | 3692 | `		}` |
|      4 | 3693 | `	}` |
|     21 | 3694 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3695 | `		/* Perform the lookup */` |
|     44 | 3696 | `		for(;;){` |
|     89 | 3697 | `			if( zBlob >= zPtr ){` |
|      9 | 3698 | `				break;` |
|      - | 3699 | `			}` |
|     81 | 3700 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 3701 | `			if( rc == SXRET_OK ){` |
|      - | 3702 | `				/* Pattern found,return it's position */` |
|     11 | 3703 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 3704 | `				return PH7_OK;` |
|      - | 3705 | `			}` |
|     71 | 3706 | `			zPtr--;` |
|      1 | 3707 | `		}` |
|      - | 3708 | `		/* Pattern not found,return FALSE */` |
|      9 | 3709 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3710 | `	}else{` |
|      3 | 3711 | `		ph7_result_bool(pCtx,0);` |
|      - | 3712 | `	}` |
|     11 | 3713 | `	return PH7_OK;` |
|     15 | 3714 |  |
|      - | 3715 | `/*` |
|      - | 3716 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3717 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3718 | ` * Parameters` |
|      - | 3719 | ` *  $haystack` |
|      - | 3720 | ` *   The input string.` |
|      - | 3721 | ` * $needle` |
|      - | 3722 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3723 | ` *  This behavior is different from that of strstr().` |
|      - | 3724 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3725 | ` *  as the ordinal value of a character.` |
|      - | 3726 | ` * Return` |
|      - | 3727 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3728 | ` */` |
|     24 | 3729 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3730 |  |
|      - | 3731 | `	const char *zBlob;` |
|      - | 3732 | `	int nLen,c;` |
|     25 | 3733 | `	if( nArg < 2 ){` |
|      - | 3734 | `		/* Missing arguments,return FALSE */` |
|      3 | 3735 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3736 | `		return PH7_OK;` |
|      - | 3737 | `	}` |
|      - | 3738 | `	/* Extract the haystack */` |
|     23 | 3739 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3740 | `	c = 0; /* cc warning */` |
|     23 | 3741 | `	if( nLen > 0 ){` |
|      - | 3742 | `		sxu32 nOfft;` |
|      - | 3743 | `		sxi32 rc;` |
|     21 | 3744 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 3745 | `			const char *zPattern;` |
|     11 | 3746 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 3747 | `														 * for NULL pointer.` |
|      - | 3748 | `														 */` |
|     11 | 3749 | `			c = zPattern[0];` |
|      6 | 3750 | `		}else{` |
|      - | 3751 | `			/* Int cast */` |
|     11 | 3752 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 3753 | `		}` |
|      - | 3754 | `		/* Perform the lookup */` |
|     21 | 3755 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3756 | `		if( rc != SXRET_OK ){` |
|      - | 3757 | `			/* No such entry,return FALSE */` |
|      7 | 3758 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3759 | `			return PH7_OK;` |
|      - | 3760 | `		}` |
|      - | 3761 | `		/* Return the string portion */` |
|     15 | 3762 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 3763 | `	}else{` |
|      3 | 3764 | `		ph7_result_bool(pCtx,0);` |
|      - | 3765 | `	}` |
|     17 | 3766 | `	return PH7_OK;` |
|     13 | 3767 |  |
|      - | 3768 | `/*` |
|      - | 3769 | ` * string strrev(string $string)` |
|      - | 3770 | ` *  Reverse a string.` |
|      - | 3771 | ` * Parameters` |
|      - | 3772 | ` *  $string` |
|      - | 3773 | ` *   String to be reversed.` |
|      - | 3774 | ` * Return` |
|      - | 3775 | ` *  The reversed string.` |
|      - | 3776 | ` */` |
|      4 | 3777 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3778 |  |
|      - | 3779 | `	const char *zIn,*zEnd;` |
|      - | 3780 | `	int nLen,c;` |
|      5 | 3781 | `	if( nArg < 1 ){` |
|      - | 3782 | `		/* Missing arguments,return NULL */` |
|      3 | 3783 | `		ph7_result_null(pCtx);` |
|      3 | 3784 | `		return PH7_OK;` |
|      - | 3785 | `	}` |
|      - | 3786 | `	/* Extract the target string */` |
|      3 | 3787 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3788 | `	if( nLen < 1 ){` |
|      - | 3789 | `		/* Empty string Return null */` |
|    ! 0 | 3790 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3791 | `		return PH7_OK;` |
|      - | 3792 | `	}` |
|      - | 3793 | `	/* Perform the requested operation */` |
|      3 | 3794 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3795 | `	for(;;){` |
|      9 | 3796 | `		if( zEnd < zIn ){` |
|      - | 3797 | `			/* No more input to process */` |
|      3 | 3798 | `			break;` |
|      - | 3799 | `		}` |
|      - | 3800 | `		/* Append current character */` |
|      7 | 3801 | `		c = zEnd[0];` |
|      7 | 3802 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3803 | `		zEnd--;` |
|      1 | 3804 | `	}` |
|      3 | 3805 | `	return PH7_OK;` |
|      3 | 3806 |  |
|      - | 3807 | `/*` |
|      - | 3808 | ` * string ucwords(string $string)` |
|      - | 3809 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3810 | ` *  The definition of a word is any string of characters that is immediately after` |
|      - | 3811 | ` *  a whitespace (These are: space, form-feed, newline, carriage return, horizontal tab, and vertical tab).` |
|      - | 3812 | ` * Parameters` |
|      - | 3813 | ` *  $string` |
|      - | 3814 | ` *   The input string.` |
|      - | 3815 | ` * Return` |
|      - | 3816 | ` *  The modified string..` |
|      - | 3817 | ` */` |
|     14 | 3818 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3819 |  |
|      - | 3820 | `	const char *zIn,*zCur,*zEnd;` |
|      - | 3821 | `	int nLen,c;` |
|     15 | 3822 | `	if( nArg < 1 ){` |
|      - | 3823 | `		/* Missing arguments,return NULL */` |
|      3 | 3824 | `		ph7_result_null(pCtx);` |
|      3 | 3825 | `		return PH7_OK;` |
|      - | 3826 | `	}` |
|      - | 3827 | `	/* Extract the target string */` |
|     13 | 3828 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 3829 | `	if( nLen < 1 ){` |
|      - | 3830 | `		/* Empty string – match PHP semantics */` |
|      3 | 3831 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3832 | `		return PH7_OK;` |
|      - | 3833 | `	}` |
|      - | 3834 | `	/* Perform the requested operation */` |
|     11 | 3835 | `	zEnd = &zIn[nLen];` |
|     21 | 3836 | `	for(;;){` |
|      - | 3837 | `		/* Jump leading white spaces */` |
|     43 | 3838 | `		zCur = zIn;` |
|     65 | 3839 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0x80 && SyisSpace(zIn[0]) ){` |
|     23 | 3840 | `			zIn++;` |
|      1 | 3841 | `		}` |
|     43 | 3842 | `		if( zCur < zIn ){` |
|      - | 3843 | `			/* Append white space stream */` |
|     23 | 3844 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 3845 | `		}` |
|     43 | 3846 | `		if( zIn >= zEnd ){` |
|      - | 3847 | `			/* No more input to process */` |
|     11 | 3848 | `			break;` |
|      - | 3849 | `		}` |
|     33 | 3850 | `		c = zIn[0];` |
|     33 | 3851 | `		if( c < 0x80 && SyisLower(c) ){` |
|     29 | 3852 | `			c = SyToUpper(c);` |
|     14 | 3853 | `		}` |
|      - | 3854 | `		/* Append the upper-cased character */` |
|     33 | 3855 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     33 | 3856 | `		zIn++;` |
|     33 | 3857 | `		zCur = zIn;` |
|      - | 3858 | `		/* Append the word varbatim */` |
|    149 | 3859 | `		while( zIn < zEnd ){` |
|    139 | 3860 | `			if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 3861 | `				/* UTF-8 stream */` |
|    ! 0 | 3862 | `				zIn++;` |
|    ! 0 | 3863 | `				SX_JMP_UTF8(zIn,zEnd);` |
|    138 | 3864 | `			}else if( !SyisSpace(zIn[0]) ){` |
|    117 | 3865 | `				zIn++;` |
|     59 | 3866 | `			}else{` |
|     23 | 3867 | `				break;` |
|      - | 3868 | `			}` |
|      1 | 3869 | `		}` |
|     33 | 3870 | `		if( zCur < zIn ){` |
|     33 | 3871 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 | 3872 | `		}` |
|      1 | 3873 | `	}` |
|     11 | 3874 | `	return PH7_OK;` |
|      8 | 3875 |  |
|      - | 3876 | `/*` |
|      - | 3877 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3878 | ` *  Returns input repeated multiplier times.` |
|      - | 3879 | ` * Parameters` |
|      - | 3880 | ` *  $string` |
|      - | 3881 | ` *   String to be repeated.` |
|      - | 3882 | ` * $multiplier` |
|      - | 3883 | ` *  Number of time the input string should be repeated.` |
|      - | 3884 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3885 | ` *  to 0, the function will return an empty string.` |
|      - | 3886 | ` * Return` |
|      - | 3887 | ` *  The repeated string.` |
|      - | 3888 | ` */` |
|  20212 | 3889 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3890 |  |
|      - | 3891 | `	const char *zIn;` |
|      - | 3892 | `	int nLen,nMul;` |
|      - | 3893 | `	int rc;` |
|  20213 | 3894 | `	if( nArg < 2 ){` |
|      - | 3895 | `		/* Missing arguments,return NULL */` |
|      3 | 3896 | `		ph7_result_null(pCtx);` |
|      3 | 3897 | `		return PH7_OK;` |
|      - | 3898 | `	}` |
|      - | 3899 | `	/* Extract the target string */` |
|  20211 | 3900 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|  20211 | 3901 | `	if( nLen < 1 ){` |
|      - | 3902 | `		/* Empty string.Return null */` |
|    ! 0 | 3903 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3904 | `		return PH7_OK;` |
|      - | 3905 | `	}` |
|      - | 3906 | `	/* Extract the multiplier */` |
|  20211 | 3907 | `	nMul = ph7_value_to_int(apArg[1]);` |
|  20211 | 3908 | `	if( nMul < 1 ){` |
|      - | 3909 | `		/* Return the empty string */` |
|      3 | 3910 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3911 | `		return PH7_OK;` |
|      - | 3912 | `	}` |
|      - | 3913 | `	/* Perform the requested operation */` |
| 120220 | 3914 | `	for(;;){` |
| 240441 | 3915 | `		if( !nMul ){` |
|  20209 | 3916 | `			break;` |
|      - | 3917 | `		}` |
|      - | 3918 | `		/* Append the copy */` |
| 220233 | 3919 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 220233 | 3920 | `		if( rc != PH7_OK ){` |
|      - | 3921 | `			/* Out of memory,break immediately */` |
|    ! 0 | 3922 | `			break;` |
|      - | 3923 | `		}` |
| 220233 | 3924 | `		nMul--;` |
|      1 | 3925 | `	}` |
|  20209 | 3926 | `	return PH7_OK;` |
|  10107 | 3927 |  |
|      - | 3928 | `/*` |
|      - | 3929 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3930 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3931 | ` * Parameters` |
|      - | 3932 | ` *  $string` |
|      - | 3933 | ` *   The input string.` |
|      - | 3934 | ` * $is_xhtml` |
|      - | 3935 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3936 | ` * Return` |
|      - | 3937 | ` *  The processed string.` |
|      - | 3938 | ` */` |
|      6 | 3939 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3940 |  |
|      - | 3941 | `	const char *zIn,*zCur,*zEnd;` |
|      7 | 3942 | `	int is_xhtml = 1; /* Default to XHTML-style '<br/>' like PHP */` |
|      - | 3943 | `	int nLen;` |
|      7 | 3944 | `	if( nArg < 1 ){` |
|      - | 3945 | `		/* Missing arguments,return the empty string */` |
|      3 | 3946 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3947 | `		return PH7_OK;` |
|      - | 3948 | `	}` |
|      - | 3949 | `	/* Extract the target string */` |
|      5 | 3950 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3951 | `	if( nLen < 1 ){` |
|      - | 3952 | `		/* Empty string,return null */` |
|    ! 0 | 3953 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3954 | `		return PH7_OK;` |
|      - | 3955 | `	}` |
|      5 | 3956 | `	if( nArg > 1 ){` |
|      3 | 3957 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3958 | `	}` |
|      5 | 3959 | `	zEnd = &zIn[nLen];` |
|      - | 3960 | `	/* Perform the requested operation */` |
|      4 | 3961 | `	for(;;){` |
|      9 | 3962 | `		zCur = zIn;` |
|      - | 3963 | `		/* Delimit the string */` |
|     21 | 3964 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3965 | `			zIn++;` |
|      1 | 3966 | `		}` |
|      9 | 3967 | `		if( zCur < zIn ){` |
|      - | 3968 | `			/* Output chunk verbatim */` |
|      9 | 3969 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3970 | `		}` |
|      9 | 3971 | `		if( zIn >= zEnd ){` |
|      - | 3972 | `			/* No more input to process */` |
|      5 | 3973 | `			break;` |
|      - | 3974 | `		}` |
|      - | 3975 | `		/* Output the HTML line break */` |
|      - | 3976 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br/>' (legacy without space), otherwise use '<br>' */` |
|      5 | 3977 | `		if( is_xhtml ){` |
|      3 | 3978 | `			ph7_result_string(pCtx,"<br/>",(int)sizeof("<br/>")-1);` |
|      2 | 3979 | `		}else{` |
|      3 | 3980 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3981 | `		}` |
|      5 | 3982 | `		zCur = zIn;` |
|      - | 3983 | `		/* Append trailing line */` |
|     11 | 3984 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3985 | `			zIn++;` |
|      1 | 3986 | `		}` |
|      5 | 3987 | `		if( zCur < zIn ){` |
|      - | 3988 | `			/* Output chunk verbatim */` |
|      5 | 3989 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 3990 | `		}` |
|      1 | 3991 | `	}` |
|      5 | 3992 | `	return PH7_OK;` |
|      4 | 3993 |  |
|      - | 3994 | `/*` |
|      - | 3995 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3996 | ` *  According to the PHP reference manual.` |
|      - | 3997 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3998 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3999 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 4000 | ` * This applies to both sprintf() and printf().` |
|      - | 4001 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 4002 | ` * or more of these elements, in order:` |
|      - | 4003 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 4004 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 4005 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 4006 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 4007 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 4008 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 4009 | ` *   it with a single quote ('). See the examples below.` |
|      - | 4010 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 4011 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 4012 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 4013 | ` *   should result in.` |
|      - | 4014 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 4015 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 4016 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 4017 | ` *   limit to the string.` |
|      - | 4018 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 4019 | ` *       % - a literal percent character. No argument is required.` |
|      - | 4020 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 4021 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 4022 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 4023 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 4024 | ` * 	     for the number of digits after the decimal point.` |
|      - | 4025 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 4026 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 4027 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 4028 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 4029 | ` *       g - shorter of %e and %f.` |
|      - | 4030 | ` *       G - shorter of %E and %f.` |
|      - | 4031 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 4032 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 4033 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 4034 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 4035 | ` */` |
|      - | 4036 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4037 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 4038 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 4039 | `/*` |
|      - | 4040 | `** Conversion types fall into various categories as defined by the` |
|      - | 4041 | `** following enumeration.` |
|      - | 4042 | `*/` |
|      - | 4043 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 4044 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 4045 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 4046 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 4047 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 4048 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 4049 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 4050 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 4051 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 4052 |  |
|      - | 4053 | `/*` |
|      - | 4054 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 4055 | `*/` |
|      - | 4056 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 4057 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 4058 | `/*` |
|      - | 4059 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 4060 | `** by an instance of the following structure` |
|      - | 4061 | `*/` |
|      - | 4062 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 4063 | `struct ph7_fmt_info` |
|      - | 4064 |  |
|      - | 4065 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 4066 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 4067 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 4068 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 4069 | `  char *charset; /* The character set for conversion */` |
|      - | 4070 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 4071 | `};` |
|      - | 4072 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 4073 | `/*` |
|      - | 4074 | `** "*val" is a double such that 0.1 <= *val < 10.0` |
|      - | 4075 | `** Return the ascii code for the leading digit of *val, then` |
|      - | 4076 | `** multiply "*val" by 10.0 to renormalize.` |
|      - | 4077 | `**` |
|      - | 4078 | `** Example:` |
|      - | 4079 | `**     input:     *val = 3.14159` |
|      - | 4080 | `**     output:    *val = 1.4159    function return = '3'` |
|      - | 4081 | `**` |
|      - | 4082 | `** The counter *cnt is incremented each time.  After counter exceeds` |
|      - | 4083 | `** 16 (the number of significant digits in a 64-bit float) '0' is` |
|      - | 4084 | `** always returned.` |
|      - | 4085 | `*/` |
|    404 | 4086 | `static int vxGetdigit(sxlongreal *val,int *cnt)` |
|      1 | 4087 |  |
|      - | 4088 | `  sxlongreal d;` |
|      - | 4089 | `  int digit;` |
|      - | 4090 |  |
|    405 | 4091 | `  if( (*cnt)++ >= 16 ){` |
|    ! 0 | 4092 | `	  return '0';` |
|      - | 4093 | `  }` |
|    405 | 4094 | `  digit = (int)*val;` |
|    405 | 4095 | `  d = digit;` |
|    405 | 4096 | `   *val = (*val - d)*10.0;` |
|    405 | 4097 | `  return digit + '0' ;` |
|    203 | 4098 |  |
|      - | 4099 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      - | 4100 | `/*` |
|      - | 4101 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 4102 | ` * used conversion types first.` |
|      - | 4103 | ` */` |
|      - | 4104 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 4105 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 4106 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 4107 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 4108 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 4109 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 4110 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 4111 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 4112 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 4113 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4114 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4115 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 4116 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 4117 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 4118 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 4119 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 4120 | `};` |
|      - | 4121 | `/*` |
|      - | 4122 | ` * Format a given string.` |
|      - | 4123 | ` * The root program.  All variations call this core.` |
|      - | 4124 | ` * INPUTS:` |
|      - | 4125 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 4126 | ` *            1. A pointer to the call context.` |
|      - | 4127 | ` *            2. A pointer to the list of characters to be output` |
|      - | 4128 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 4129 | ` *            3. An integer number of characters to be output.` |
|      - | 4130 | ` *               (Note: This number might be zero.)` |
|      - | 4131 | ` *            4. Upper layer private data.` |
|      - | 4132 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 4133 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 4134 | ` */` |
|    120 | 4135 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 4136 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 4137 | `	ph7_context *pCtx,  /* call context */` |
|      - | 4138 | `	const char *zIn,    /* Format string */` |
|      - | 4139 | `	int nByte,          /* Format string length */` |
|      - | 4140 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 4141 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 4142 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 4143 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 4144 | `	)` |
|      1 | 4145 |  |
|    121 | 4146 | `	char spaces[] = "                                                  ";` |
|      - | 4147 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    121 | 4148 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 4149 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 4150 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 4151 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 4152 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 4153 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 4154 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 4155 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 4156 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 4157 | `	ph7_int64 iVal;` |
|      - | 4158 | `	int precision;           /* Precision of the current field */` |
|      - | 4159 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 4160 | `	int c,rc,n;` |
|      - | 4161 | `	int length;              /* Length of the field */` |
|      - | 4162 | `	int prefix;` |
|      - | 4163 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 4164 | `	int width;               /* Width of the current field */` |
|      - | 4165 | `	int idx;` |
|    121 | 4166 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 4167 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 4168 | `	/* Start the format process */` |
|    123 | 4169 | `	for(;;){` |
|    247 | 4170 | `		zCur = zIn;` |
|    697 | 4171 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|    451 | 4172 | `			zIn++;` |
|      1 | 4173 | `		}` |
|    247 | 4174 | `		if( zCur < zIn ){` |
|      - | 4175 | `			/* Consume chunk verbatim */` |
|     95 | 4176 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|     95 | 4177 | `			if( rc == SXERR_ABORT ){` |
|      - | 4178 | `				/* Callback request an operation abort */` |
|    ! 0 | 4179 | `				break;` |
|      - | 4180 | `			}` |
|     47 | 4181 | `		}` |
|    247 | 4182 | `		if( zIn >= zEnd ){` |
|      - | 4183 | `			/* No more input to process,break immediately */` |
|    119 | 4184 | `			break;` |
|      - | 4185 | `		}` |
|      - | 4186 | `		/* Find out what flags are present */` |
|    129 | 4187 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    128 | 4188 | `			flag_alternateform = flag_zeropad = 0;` |
|    129 | 4189 | `		zIn++; /* Jump the precent sign */` |
|     64 | 4190 | `		do{` |
|    157 | 4191 | `			c = zIn[0];` |
|    157 | 4192 | `			switch( c ){` |
|      9 | 4193 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      3 | 4194 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 4195 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|      9 | 4196 | `			case '#':   flag_alternateform = 1;   c = 0;   break;` |
|      5 | 4197 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 4198 | `			case '\'':` |
|    ! 0 | 4199 | `				zIn++;` |
|    ! 0 | 4200 | `				if( zIn < zEnd ){` |
|      - | 4201 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 4202 | `					c = zIn[0];` |
|    ! 0 | 4203 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4204 | `						spaces[idx] = (char)c;` |
|    ! 0 | 4205 | `					}` |
|    ! 0 | 4206 | `					c = 0;` |
|    ! 0 | 4207 | `				}` |
|    ! 0 | 4208 | `				break;` |
|    128 | 4209 | `			default:                                       break;` |
|      - | 4210 | `			}` |
|    157 | 4211 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 4212 | `		/* Get the field width */` |
|    129 | 4213 | `		width = 0;` |
|    223 | 4214 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     31 | 4215 | `			width = width*10 + (zIn[0] - '0');` |
|     31 | 4216 | `			zIn++;` |
|      1 | 4217 | `		}` |
|    129 | 4218 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 4219 | `			/* Position specifer */` |
|    ! 0 | 4220 | `			if( width > 0 ){` |
|    ! 0 | 4221 | `				n = width;` |
|    ! 0 | 4222 | `				if( vf && n > 0 ){` |
|    ! 0 | 4223 | `					n--;` |
|    ! 0 | 4224 | `				}` |
|    ! 0 | 4225 | `			}` |
|    ! 0 | 4226 | `			zIn++;` |
|    ! 0 | 4227 | `			width = 0;` |
|    ! 0 | 4228 | `			if( zIn < zEnd && zIn[0] == '0' ){` |
|    ! 0 | 4229 | `				flag_zeropad = 1;` |
|    ! 0 | 4230 | `				zIn++;` |
|    ! 0 | 4231 | `			}` |
|    ! 0 | 4232 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 4233 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 4234 | `				zIn++;` |
|    ! 0 | 4235 | `			}` |
|    ! 0 | 4236 | `		}` |
|    129 | 4237 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 4238 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 4239 | `		}` |
|      - | 4240 | `		/* Get the precision */` |
|    129 | 4241 | `		precision = -1;` |
|    129 | 4242 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     57 | 4243 | `			precision = 0;` |
|     57 | 4244 | `			zIn++;` |
|    145 | 4245 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     61 | 4246 | `				precision = precision*10 + (zIn[0] - '0');` |
|     61 | 4247 | `				zIn++;` |
|      1 | 4248 | `			}` |
|     28 | 4249 | `		}` |
|    129 | 4250 | `		if( zIn >= zEnd ){` |
|      - | 4251 | `			/* No more input */` |
|      3 | 4252 | `			break;` |
|      - | 4253 | `		}` |
|      - | 4254 | `		/* Fetch the info entry for the field */` |
|    127 | 4255 | `		pInfo = 0;` |
|    127 | 4256 | `		xtype = PH7_FMT_ERROR;` |
|    127 | 4257 | `		c = zIn[0];` |
|    127 | 4258 | `		zIn++; /* Jump the format specifer */` |
|    699 | 4259 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|    697 | 4260 | `			if( c==aFmt[idx].fmttype ){` |
|    125 | 4261 | `				pInfo = &aFmt[idx];` |
|    125 | 4262 | `				xtype = pInfo->type;` |
|    125 | 4263 | `				break;` |
|      - | 4264 | `			}` |
|    287 | 4265 | `		}` |
|    127 | 4266 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    127 | 4267 | `		length = 0;` |
|      - | 4268 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 4269 | `		 /*` |
|      - | 4270 | `		  ** At this point, variables are initialized as follows:` |
|      - | 4271 | `		  **` |
|      - | 4272 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 4273 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 4274 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 4275 | `		  **                               field width was negative.` |
|      - | 4276 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 4277 | `		  **                               the conversion character.` |
|      - | 4278 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 4279 | `		  **   width                       The specified field width.  This is` |
|      - | 4280 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 4281 | `		  **   precision                   The specified precision.  The default` |
|      - | 4282 | `		  **                               is -1.` |
|      - | 4283 | `		  */` |
|    127 | 4284 | `		switch(xtype){` |
|    ! 0 | 4285 | `		case PH7_FMT_PERCENT:` |
|      - | 4286 | `			/* A literal percent character */` |
|    ! 0 | 4287 | `			zWorker[0] = '%';` |
|    ! 0 | 4288 | `			length = (int)sizeof(char);` |
|    ! 0 | 4289 | `			break;` |
|      3 | 4290 | `		case PH7_FMT_CHARX:` |
|      - | 4291 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 4292 | `			 * with that ASCII value` |
|      - | 4293 | `			 */` |
|      7 | 4294 | `			pArg = NEXT_ARG;` |
|      7 | 4295 | `			if( pArg == 0 ){` |
|      3 | 4296 | `				c = 0;` |
|      2 | 4297 | `			}else{` |
|      5 | 4298 | `				c = ph7_value_to_int(pArg);` |
|      - | 4299 | `			}` |
|      - | 4300 | `			/* NUL byte is an acceptable value */` |
|      7 | 4301 | `			zWorker[0] = (char)c;` |
|      7 | 4302 | `			length = (int)sizeof(char);` |
|      7 | 4303 | `			break;` |
|     12 | 4304 | `		case PH7_FMT_STRING:` |
|      - | 4305 | `			/* the argument is treated as and presented as a string */` |
|     25 | 4306 | `			pArg = NEXT_ARG;` |
|     25 | 4307 | `			if( pArg == 0 ){` |
|    ! 0 | 4308 | `				length = 0;` |
|    ! 0 | 4309 | `			}else{` |
|     25 | 4310 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 4311 | `			}` |
|     25 | 4312 | `			if( length < 1 ){` |
|    ! 0 | 4313 | `				zBuf = " ";` |
|    ! 0 | 4314 | `				length = (int)sizeof(char);` |
|    ! 0 | 4315 | `			}` |
|     25 | 4316 | `			if( precision>=0 && precision<length ){` |
|      3 | 4317 | `				length = precision;` |
|      1 | 4318 | `			}` |
|     25 | 4319 | `			if( flag_zeropad ){` |
|      - | 4320 | `				/* zero-padding works on strings too */` |
|    ! 0 | 4321 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4322 | `					spaces[idx] = '0';` |
|    ! 0 | 4323 | `				}` |
|    ! 0 | 4324 | `			}` |
|     25 | 4325 | `			break;` |
|     20 | 4326 | `		case PH7_FMT_RADIX:` |
|     41 | 4327 | `			pArg = NEXT_ARG;` |
|     41 | 4328 | `			if( pArg == 0 ){` |
|    ! 0 | 4329 | `				iVal = 0;` |
|    ! 0 | 4330 | `			}else{` |
|     41 | 4331 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 4332 | `			}` |
|      - | 4333 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|     41 | 4334 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 4335 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 4336 | `			}` |
|      - | 4337 | `#if 1` |
|      - | 4338 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 4339 | `        ** I think this is stupid.*/` |
|     41 | 4340 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 4341 | `#else` |
|      - | 4342 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 4343 | `        ** but leave the prefix for hex.*/` |
|      - | 4344 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 4345 | `#endif` |
|     41 | 4346 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|     23 | 4347 | `          if( iVal<0 ){` |
|      3 | 4348 | `            iVal = -iVal;` |
|      - | 4349 | `			/* Ticket 1433-003 */` |
|      3 | 4350 | `			if( iVal < 0 ){` |
|      - | 4351 | `				/* Overflow */` |
|    ! 0 | 4352 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4353 | `			}` |
|      3 | 4354 | `            prefix = '-';` |
|     22 | 4355 | `          }else if( flag_plussign )  prefix = '+';` |
|     19 | 4356 | `          else if( flag_blanksign )  prefix = ' ';` |
|     17 | 4357 | `          else                       prefix = 0;` |
|     12 | 4358 | `        }else{` |
|     19 | 4359 | `			if( iVal<0 ){` |
|    ! 0 | 4360 | `				iVal = -iVal;` |
|      - | 4361 | `				/* Ticket 1433-003 */` |
|    ! 0 | 4362 | `				if( iVal < 0 ){` |
|      - | 4363 | `					/* Overflow */` |
|    ! 0 | 4364 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4365 | `				}` |
|    ! 0 | 4366 | `			}` |
|     19 | 4367 | `			prefix = 0;` |
|      - | 4368 | `		}` |
|     41 | 4369 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      3 | 4370 | `          precision = width-(prefix!=0);` |
|      1 | 4371 | `        }` |
|     41 | 4372 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 4373 | `        {` |
|      - | 4374 | `          register char *cset;      /* Use registers for speed */` |
|      - | 4375 | `          register int base;` |
|     41 | 4376 | `          cset = pInfo->charset;` |
|     41 | 4377 | `          base = pInfo->base;` |
|     20 | 4378 | `          do{                                           /* Convert to ascii */` |
|     79 | 4379 | `            *(--zBuf) = cset[iVal%base];` |
|     79 | 4380 | `            iVal = iVal/base;` |
|     79 | 4381 | `          }while( iVal>0 );` |
|      - | 4382 | `        }` |
|     41 | 4383 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     55 | 4384 | `        for(idx=precision-length; idx>0; idx--){` |
|     15 | 4385 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|      8 | 4386 | `        }` |
|     41 | 4387 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|     41 | 4388 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 4389 | `          char *pre, x;` |
|      9 | 4390 | `          pre = pInfo->prefix;` |
|      9 | 4391 | `          if( *zBuf!=pre[0] ){` |
|     23 | 4392 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|      4 | 4393 | `          }` |
|      4 | 4394 | `        }` |
|     41 | 4395 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     41 | 4396 | `		break;` |
|     27 | 4397 | `		case PH7_FMT_FLOAT:` |
|      - | 4398 | `		case PH7_FMT_EXP:` |
|      - | 4399 | `		case PH7_FMT_GENERIC:{` |
|      - | 4400 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 4401 | `		long double realvalue;` |
|      - | 4402 | `		int  exp;                /* exponent of real numbers */` |
|      - | 4403 | `		double rounder;          /* Used for rounding floating point values */` |
|      - | 4404 | `		int flag_dp;            /* True if decimal point should be shown */` |
|      - | 4405 | `		int flag_rtz;           /* True if trailing zeros should be removed */` |
|      - | 4406 | `		int flag_exp;           /* True to force display of the exponent */` |
|      - | 4407 | `		int nsd;                 /* Number of significant digits returned */` |
|     55 | 4408 | `		pArg = NEXT_ARG;` |
|     55 | 4409 | `		if( pArg == 0 ){` |
|    ! 0 | 4410 | `			realvalue = 0;` |
|    ! 0 | 4411 | `		}else{` |
|     55 | 4412 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 4413 | `		}` |
|      - | 4414 | `		/* Special-case NaN and infinities since the normal formatting logic` |
|      - | 4415 | `		 * below assumes a finite positive realvalue. */` |
|     55 | 4416 | `		if( PH7_IS_NAN(realvalue) ){` |
|    ! 0 | 4417 | `			zBuf = "NAN";` |
|    ! 0 | 4418 | `			length = 3;` |
|    ! 0 | 4419 | `			break;` |
|      - | 4420 | `		}` |
|     55 | 4421 | `		if( PH7_IS_INF(realvalue) ){` |
|      - | 4422 | `			/* Infinity prints as INF or -INF depending on sign. */` |
|    ! 0 | 4423 | `			if( realvalue < 0.0 ){` |
|    ! 0 | 4424 | `				zBuf = "-INF";` |
|    ! 0 | 4425 | `				length = 4;` |
|    ! 0 | 4426 | `			}else{` |
|    ! 0 | 4427 | `				zBuf = "INF";` |
|    ! 0 | 4428 | `				length = 3;` |
|      - | 4429 | `			}` |
|    ! 0 | 4430 | `			break;` |
|      - | 4431 | `		}` |
|     55 | 4432 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|     55 | 4433 | `		if( precision>PH7_FMT_BUFSIZ-40) precision = PH7_FMT_BUFSIZ-40;` |
|     55 | 4434 | `        if( realvalue<0.0 ){` |
|    ! 0 | 4435 | `          realvalue = -realvalue;` |
|    ! 0 | 4436 | `          prefix = '-';` |
|    ! 0 | 4437 | `        }else{` |
|     55 | 4438 | `          if( flag_plussign )          prefix = '+';` |
|     55 | 4439 | `          else if( flag_blanksign )    prefix = ' ';` |
|     55 | 4440 | `          else                         prefix = 0;` |
|      - | 4441 | `        }` |
|     55 | 4442 | `        if( pInfo->type==PH7_FMT_GENERIC && precision>0 ) precision--;` |
|     55 | 4443 | `        rounder = 0.0;` |
|      - | 4444 | `#if 0` |
|      - | 4445 | `        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */` |
|      - | 4446 | `        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);` |
|      - | 4447 | `#else` |
|      - | 4448 | `        /* It makes more sense to use 0.5 */` |
|    387 | 4449 | `        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);` |
|      - | 4450 | `#endif` |
|     55 | 4451 | `        if( pInfo->type==PH7_FMT_FLOAT ) realvalue += rounder;` |
|      - | 4452 | `        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */` |
|     55 | 4453 | `        exp = 0;` |
|     55 | 4454 | `        if( realvalue>0.0 ){` |
|     59 | 4455 | `          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }` |
|     87 | 4456 | `          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }` |
|     59 | 4457 | `          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }` |
|     69 | 4458 | `          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }` |
|     55 | 4459 | `          if( exp>350 \|\| exp<-350 ){` |
|    ! 0 | 4460 | `            zBuf = "NaN";` |
|    ! 0 | 4461 | `            length = 3;` |
|    ! 0 | 4462 | `            break;` |
|      - | 4463 | `          }` |
|     27 | 4464 | `        }` |
|     55 | 4465 | `        zBuf = zWorker;` |
|      - | 4466 | `        /*` |
|      - | 4467 | `        ** If the field type is etGENERIC, then convert to either etEXP` |
|      - | 4468 | `        ** or etFLOAT, as appropriate.` |
|      - | 4469 | `        */` |
|     55 | 4470 | `        flag_exp = xtype==PH7_FMT_EXP;` |
|     55 | 4471 | `        if( xtype!=PH7_FMT_FLOAT ){` |
|    ! 0 | 4472 | `          realvalue += rounder;` |
|    ! 0 | 4473 | `          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }` |
|    ! 0 | 4474 | `        }` |
|     55 | 4475 | `        if( xtype==PH7_FMT_GENERIC ){` |
|    ! 0 | 4476 | `          flag_rtz = !flag_alternateform;` |
|    ! 0 | 4477 | `          if( exp<-4 \|\| exp>precision ){` |
|    ! 0 | 4478 | `            xtype = PH7_FMT_EXP;` |
|    ! 0 | 4479 | `          }else{` |
|    ! 0 | 4480 | `            precision = precision - exp;` |
|    ! 0 | 4481 | `            xtype = PH7_FMT_FLOAT;` |
|      - | 4482 | `          }` |
|    ! 0 | 4483 | `        }else{` |
|     55 | 4484 | `          flag_rtz = 0;` |
|      - | 4485 | `        }` |
|      - | 4486 | `        /*` |
|      - | 4487 | `        ** The "exp+precision" test causes output to be of type etEXP if` |
|      - | 4488 | `        ** the precision is too large to fit in buf[].` |
|      - | 4489 | `        */` |
|     55 | 4490 | `        nsd = 0;` |
|     55 | 4491 | `        if( xtype==PH7_FMT_FLOAT && exp+precision<PH7_FMT_BUFSIZ-30 ){` |
|     55 | 4492 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|     55 | 4493 | `          if( prefix ) *(zBuf++) = (char)prefix;         /* Sign */` |
|     55 | 4494 | `          if( exp<0 )  *(zBuf++) = '0';            /* Digits before "." */` |
|    145 | 4495 | `          else for(; exp>=0; exp--) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     55 | 4496 | `          if( flag_dp ) *(zBuf++) = '.';           /* The decimal point */` |
|     87 | 4497 | `          for(exp++; exp<0 && precision>0; precision--, exp++){` |
|     33 | 4498 | `            *(zBuf++) = '0';` |
|     17 | 4499 | `          }` |
|    355 | 4500 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     55 | 4501 | `          *(zBuf--) = 0;                           /* Null terminate */` |
|     55 | 4502 | `          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */` |
|    ! 0 | 4503 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 4504 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 4505 | `          }` |
|     55 | 4506 | `          zBuf++;                            /* point to next free slot */` |
|     28 | 4507 | `        }else{    /* etEXP or etGENERIC */` |
|    ! 0 | 4508 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|    ! 0 | 4509 | `          if( prefix ) *(zBuf++) = (char)prefix;   /* Sign */` |
|    ! 0 | 4510 | `          *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);  /* First digit */` |
|    ! 0 | 4511 | `          if( flag_dp ) *(zBuf++) = '.';     /* Decimal point */` |
|    ! 0 | 4512 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|    ! 0 | 4513 | `          zBuf--;                            /* point to last digit */` |
|    ! 0 | 4514 | `          if( flag_rtz && flag_dp ){          /* Remove tail zeros */` |
|    ! 0 | 4515 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 4516 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 4517 | `          }` |
|    ! 0 | 4518 | `          zBuf++;                            /* point to next free slot */` |
|    ! 0 | 4519 | `          if( exp \|\| flag_exp ){` |
|    ! 0 | 4520 | `            *(zBuf++) = pInfo->charset[0];` |
|    ! 0 | 4521 | `            if( exp<0 ){ *(zBuf++) = '-'; exp = -exp; } /* sign of exp */` |
|    ! 0 | 4522 | `            else       { *(zBuf++) = '+'; }` |
|    ! 0 | 4523 | `            if( exp>=100 ){` |
|    ! 0 | 4524 | `              *(zBuf++) = (char)((exp/100)+'0');                /* 100's digit */` |
|    ! 0 | 4525 | `              exp %= 100;` |
|    ! 0 | 4526 | `            }` |
|    ! 0 | 4527 | `            *(zBuf++) = (char)(exp/10+'0');                     /* 10's digit */` |
|    ! 0 | 4528 | `            *(zBuf++) = (char)(exp%10+'0');                     /* 1's digit */` |
|    ! 0 | 4529 | `          }` |
|      - | 4530 | `        }` |
|      - | 4531 | `        /* The converted number is in buf[] and zero terminated.Output it.` |
|      - | 4532 | `        ** Note that the number is in the usual order, not reversed as with` |
|      - | 4533 | `        ** integer conversions.*/` |
|     55 | 4534 | `        length = (int)(zBuf-zWorker);` |
|     55 | 4535 | `        zBuf = zWorker;` |
|      - | 4536 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 4537 | `        ** set and we are not left justified */` |
|     55 | 4538 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 4539 | `          int i;` |
|      3 | 4540 | `          int nPad = width - length;` |
|     13 | 4541 | `          for(i=width; i>=nPad; i--){` |
|     11 | 4542 | `            zBuf[i] = zBuf[i-nPad];` |
|      6 | 4543 | `          }` |
|      3 | 4544 | `          i = prefix!=0;` |
|      5 | 4545 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      3 | 4546 | `          length = width;` |
|      1 | 4547 | `        }` |
|      - | 4548 | `#else` |
|      - | 4549 | `         zBuf = " ";` |
|      - | 4550 | `		 length = (int)sizeof(char);` |
|      - | 4551 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|     55 | 4552 | `		 break;` |
|      - | 4553 | `							 }` |
|      1 | 4554 | `		default:` |
|      - | 4555 | `			/* Invalid format specifer */` |
|      3 | 4556 | `			zWorker[0] = '?';` |
|      3 | 4557 | `			length = (int)sizeof(char);` |
|      2 | 4558 | `			break;` |
|      - | 4559 | `		}` |
|      - | 4560 | `		 /*` |
|      - | 4561 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 4562 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 4563 | `		 ** the output.` |
|      - | 4564 | `		 */` |
|    127 | 4565 | `    if( !flag_leftjustify ){` |
|      - | 4566 | `      register int nspace;` |
|    119 | 4567 | `      nspace = width-length;` |
|    119 | 4568 | `      if( nspace>0 ){` |
|      5 | 4569 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4570 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4571 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4572 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4573 | `			}` |
|    ! 0 | 4574 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4575 | `        }` |
|      5 | 4576 | `        if( nspace>0 ){` |
|      5 | 4577 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      5 | 4578 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4579 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4580 | `			}` |
|      2 | 4581 | `		}` |
|      2 | 4582 | `      }` |
|     59 | 4583 | `    }` |
|    127 | 4584 | `    if( length>0 ){` |
|    127 | 4585 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    127 | 4586 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4587 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4588 | `		}` |
|     63 | 4589 | `    }` |
|    127 | 4590 | `    if( flag_leftjustify ){` |
|      - | 4591 | `      register int nspace;` |
|      9 | 4592 | `      nspace = width-length;` |
|      9 | 4593 | `      if( nspace>0 ){` |
|      9 | 4594 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4595 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4596 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4597 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4598 | `			}` |
|    ! 0 | 4599 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4600 | `        }` |
|      9 | 4601 | `        if( nspace>0 ){` |
|      9 | 4602 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      9 | 4603 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4604 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4605 | `			}` |
|      4 | 4606 | `		}` |
|      4 | 4607 | `      }` |
|      4 | 4608 | `    }` |
|      1 | 4609 | ` }/* for(;;) */` |
|    121 | 4610 | `	return SXRET_OK;` |
|     61 | 4611 |  |
|      - | 4612 | `/*` |
|      - | 4613 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 4614 | ` */` |
|     84 | 4615 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4616 |  |
|      - | 4617 | `	/* Consume directly */` |
|     85 | 4618 | `	ph7_result_string(pCtx,zInput,nLen);` |
|     42 | 4619 | `	SXUNUSED(pUserData); /* cc warning */` |
|     85 | 4620 | `	return PH7_OK;` |
|      1 | 4621 |  |
|      - | 4622 | `/*` |
|      - | 4623 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 4624 | ` *  Return a formatted string.` |
|      - | 4625 | ` * Parameters` |
|      - | 4626 | ` *  $format` |
|      - | 4627 | ` *    The format string (see block comment above)` |
|      - | 4628 | ` * Return` |
|      - | 4629 | ` *  A string produced according to the formatting string format.` |
|      - | 4630 | ` */` |
|     56 | 4631 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4632 |  |
|      - | 4633 | `	const char *zFormat;` |
|      - | 4634 | `	int nLen;` |
|     57 | 4635 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4636 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 4637 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4638 | `		return PH7_OK;` |
|      - | 4639 | `	}` |
|      - | 4640 | `	/* Extract the string format */` |
|     55 | 4641 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     55 | 4642 | `	if( nLen < 1 ){` |
|      - | 4643 | `		/* Empty string */` |
|    ! 0 | 4644 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4645 | `		return PH7_OK;` |
|      - | 4646 | `	}` |
|      - | 4647 | `	/* Format the string */` |
|     55 | 4648 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,0,FALSE);` |
|     55 | 4649 | `	return PH7_OK;` |
|     29 | 4650 |  |
|      - | 4651 | `/*` |
|      - | 4652 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 4653 | ` */` |
|    110 | 4654 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4655 |  |
|    111 | 4656 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 4657 | `	/* Call the VM output consumer directly */` |
|    111 | 4658 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 4659 | `	/* Increment counter */` |
|    111 | 4660 | `	*pCounter += nLen;` |
|    111 | 4661 | `	return PH7_OK;` |
|      1 | 4662 |  |
|      - | 4663 | `/*` |
|      - | 4664 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 4665 | ` *  Output a formatted string.` |
|      - | 4666 | ` * Parameters` |
|      - | 4667 | ` *  $format` |
|      - | 4668 | ` *   See sprintf() for a description of format.` |
|      - | 4669 | ` * Return` |
|      - | 4670 | ` *  The length of the outputted string.` |
|      - | 4671 | ` */` |
|     42 | 4672 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4673 |  |
|     43 | 4674 | `	ph7_int64 nCounter = 0;` |
|      - | 4675 | `	const char *zFormat;` |
|      - | 4676 | `	int nLen;` |
|     43 | 4677 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4678 | `		/* Missing/Invalid arguments,return 0 */` |
|      3 | 4679 | `		ph7_result_int(pCtx,0);` |
|      3 | 4680 | `		return PH7_OK;` |
|      - | 4681 | `	}` |
|      - | 4682 | `	/* Extract the string format */` |
|     41 | 4683 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 4684 | `	if( nLen < 1 ){` |
|      - | 4685 | `		/* Empty string */` |
|    ! 0 | 4686 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4687 | `		return PH7_OK;` |
|      - | 4688 | `	}` |
|      - | 4689 | `	/* Format the string */` |
|     41 | 4690 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 4691 | `	/* Return the length of the outputted string */` |
|     41 | 4692 | `	ph7_result_int64(pCtx,nCounter);` |
|     41 | 4693 | `	return PH7_OK;` |
|     22 | 4694 |  |
|      - | 4695 | `/*` |
|      - | 4696 | ` * int vprintf(string $format,array $args)` |
|      - | 4697 | ` *  Output a formatted string.` |
|      - | 4698 | ` * Parameters` |
|      - | 4699 | ` *  $format` |
|      - | 4700 | ` *   See sprintf() for a description of format.` |
|      - | 4701 | ` * Return` |
|      - | 4702 | ` *  The length of the outputted string.` |
|      - | 4703 | ` */` |
|      2 | 4704 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4705 |  |
|      3 | 4706 | `	ph7_int64 nCounter = 0;` |
|      - | 4707 | `	const char *zFormat;` |
|      - | 4708 | `	ph7_hashmap *pMap;` |
|      - | 4709 | `	SySet sArg;` |
|      - | 4710 | `	int nLen,n;` |
|      3 | 4711 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4712 | `		/* Missing/Invalid arguments,return 0 */` |
|    ! 0 | 4713 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4714 | `		return PH7_OK;` |
|      - | 4715 | `	}` |
|      - | 4716 | `	/* Extract the string format */` |
|      3 | 4717 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4718 | `	if( nLen < 1 ){` |
|      - | 4719 | `		/* Empty string */` |
|    ! 0 | 4720 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4721 | `		return PH7_OK;` |
|      - | 4722 | `	}` |
|      - | 4723 | `	/* Point to the hashmap */` |
|      3 | 4724 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4725 | `	/* Extract arguments from the hashmap */` |
|      3 | 4726 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4727 | `	/* Format the string */` |
|      3 | 4728 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 4729 | `	/* Return the length of the outputted string */` |
|      3 | 4730 | `	ph7_result_int64(pCtx,nCounter);` |
|      - | 4731 | `	/* Release the container */` |
|      3 | 4732 | `	SySetRelease(&sArg);` |
|      3 | 4733 | `	return PH7_OK;` |
|      2 | 4734 |  |
|      - | 4735 | `/*` |
|      - | 4736 | ` * int vsprintf(string $format,array $args)` |
|      - | 4737 | ` *  Output a formatted string.` |
|      - | 4738 | ` * Parameters` |
|      - | 4739 | ` *  $format` |
|      - | 4740 | ` *   See sprintf() for a description of format.` |
|      - | 4741 | ` * Return` |
|      - | 4742 | ` *  A string produced according to the formatting string format.` |
|      - | 4743 | ` */` |
|     10 | 4744 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4745 |  |
|      - | 4746 | `	const char *zFormat;` |
|      - | 4747 | `	ph7_hashmap *pMap;` |
|      - | 4748 | `	SySet sArg;` |
|      - | 4749 | `	int nLen,n;` |
|     11 | 4750 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4751 | `		/* Missing/Invalid arguments,return the empty string */` |
|      5 | 4752 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 4753 | `		return PH7_OK;` |
|      - | 4754 | `	}` |
|      - | 4755 | `	/* Extract the string format */` |
|      7 | 4756 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4757 | `	if( nLen < 1 ){` |
|      - | 4758 | `		/* Empty string */` |
|    ! 0 | 4759 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4760 | `		return PH7_OK;` |
|      - | 4761 | `	}` |
|      - | 4762 | `	/* Point to hashmap */` |
|      7 | 4763 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4764 | `	/* Extract arguments from the hashmap */` |
|      7 | 4765 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4766 | `	/* Format the string */` |
|      7 | 4767 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),0,TRUE);` |
|      - | 4768 | `	/* Release the container */` |
|      7 | 4769 | `	SySetRelease(&sArg);` |
|      7 | 4770 | `	return PH7_OK;` |
|      6 | 4771 |  |
|      - | 4772 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 4773 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4774 | `/*` |
|      - | 4775 | ` * Symisc eXtension.` |
|      - | 4776 | ` * string size_format(int64 $size)` |
|      - | 4777 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 4778 | ` *  Example:` |
|      - | 4779 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 4780 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 4781 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 4782 | ` * Parameter` |
|      - | 4783 | ` *  $size` |
|      - | 4784 | ` *    Entity size in bytes.` |
|      - | 4785 | ` * Return` |
|      - | 4786 | ` *   Formatted string representation of the given size.` |
|      - | 4787 | ` */` |
|     24 | 4788 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4789 |  |
|      - | 4790 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 4791 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 4792 | `	sxi32 nRest,i_32;` |
|      - | 4793 | `	ph7_int64 iSize;` |
|     25 | 4794 | `	int c = -1; /* index in zUnit[] */` |
|      - | 4795 |  |
|     25 | 4796 | `	if( nArg < 1 ){` |
|      - | 4797 | `		/* Missing argument,return the empty string */` |
|      3 | 4798 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4799 | `		return PH7_OK;` |
|      - | 4800 | `	}` |
|      - | 4801 | `	/* Extract the given size */` |
|     23 | 4802 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 4803 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 4804 | `		/* Don't bother formatting,return immediately */` |
|      5 | 4805 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 4806 | `		return PH7_OK;` |
|      - | 4807 | `	}` |
|     19 | 4808 | `	for(;;){` |
|     39 | 4809 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 4810 | `		iSize >>= 10;` |
|     39 | 4811 | `		c++;` |
|     39 | 4812 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 4813 | `			break;` |
|      - | 4814 | `		}` |
|      1 | 4815 | `	}` |
|     19 | 4816 | `	nRest /= 100;` |
|     19 | 4817 | `	if( nRest > 9 ){` |
|    ! 0 | 4818 | `		nRest = 9;` |
|    ! 0 | 4819 | `	}` |
|     19 | 4820 | `	if( iSize > 999 ){` |
|    ! 0 | 4821 | `		c++;` |
|    ! 0 | 4822 | `		nRest = 9;` |
|    ! 0 | 4823 | `		iSize = 0;` |
|    ! 0 | 4824 | `	}` |
|     19 | 4825 | `	i_32 = (sxi32)iSize;` |
|      - | 4826 | `	/* Format */` |
|     19 | 4827 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 4828 | `	return PH7_OK;` |
|     13 | 4829 |  |
|      - | 4830 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4831 | `/*` |
|      - | 4832 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 4833 | ` *   Calculate the md5 hash of a string.` |
|      - | 4834 | ` * Parameter` |
|      - | 4835 | ` *  $str` |
|      - | 4836 | ` *   Input string` |
|      - | 4837 | ` * $raw_output` |
|      - | 4838 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4839 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4840 | ` * Return` |
|      - | 4841 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 4842 | ` */` |
|     10 | 4843 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4844 |  |
|      - | 4845 | `	unsigned char zDigest[16];` |
|     11 | 4846 | `	int raw_output = FALSE;` |
|      - | 4847 | `	const void *pIn;` |
|      - | 4848 | `	int nLen;` |
|     11 | 4849 | `	if( nArg < 1 ){` |
|      - | 4850 | `		/* Missing arguments,return the empty string */` |
|      3 | 4851 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4852 | `		return PH7_OK;` |
|      - | 4853 | `	}` |
|      - | 4854 | `	/* Extract the input string */` |
|      9 | 4855 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 4856 | `	if( nLen < 1 ){` |
|      - | 4857 | `		/* Empty string */` |
|    ! 0 | 4858 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4859 | `		return PH7_OK;` |
|      - | 4860 | `	}` |
|      9 | 4861 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      3 | 4862 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 | 4863 | `	}` |
|      - | 4864 | `	/* Compute the MD5 digest */` |
|      9 | 4865 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|      9 | 4866 | `	if( raw_output ){` |
|      - | 4867 | `		/* Output raw digest */` |
|      3 | 4868 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      2 | 4869 | `	}else{` |
|      - | 4870 | `		/* Perform a binary to hex conversion */` |
|      7 | 4871 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4872 | `	}` |
|      9 | 4873 | `	return PH7_OK;` |
|      6 | 4874 |  |
|      - | 4875 | `/*` |
|      - | 4876 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 4877 | ` *   Calculate the sha1 hash of a string.` |
|      - | 4878 | ` * Parameter` |
|      - | 4879 | ` *  $str` |
|      - | 4880 | ` *   Input string` |
|      - | 4881 | ` * $raw_output` |
|      - | 4882 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4883 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4884 | ` * Return` |
|      - | 4885 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 4886 | ` */` |
|      8 | 4887 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4888 |  |
|      - | 4889 | `	unsigned char zDigest[20];` |
|      9 | 4890 | `	int raw_output = FALSE;` |
|      - | 4891 | `	const void *pIn;` |
|      - | 4892 | `	int nLen;` |
|      9 | 4893 | `	if( nArg < 1 ){` |
|      - | 4894 | `		/* Missing arguments,return the empty string */` |
|      3 | 4895 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4896 | `		return PH7_OK;` |
|      - | 4897 | `	}` |
|      - | 4898 | `	/* Extract the input string */` |
|      7 | 4899 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4900 | `	if( nLen < 1 ){` |
|      - | 4901 | `		/* Empty string */` |
|    ! 0 | 4902 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4903 | `		return PH7_OK;` |
|      - | 4904 | `	}` |
|      7 | 4905 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      3 | 4906 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 | 4907 | `	}` |
|      - | 4908 | `	/* Compute the SHA1 digest */` |
|      7 | 4909 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|      7 | 4910 | `	if( raw_output ){` |
|      - | 4911 | `		/* Output raw digest */` |
|      3 | 4912 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      2 | 4913 | `	}else{` |
|      - | 4914 | `		/* Perform a binary to hex conversion */` |
|      5 | 4915 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4916 | `	}` |
|      7 | 4917 | `	return PH7_OK;` |
|      5 | 4918 |  |
|      - | 4919 | `/*` |
|      - | 4920 | ` * int64 crc32(string $str)` |
|      - | 4921 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 4922 | ` * Parameter` |
|      - | 4923 | ` *  $str` |
|      - | 4924 | ` *   Input string` |
|      - | 4925 | ` * Return` |
|      - | 4926 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 4927 | ` */` |
|      4 | 4928 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4929 |  |
|      - | 4930 | `	const void *pIn;` |
|      - | 4931 | `	sxu32 nCRC;` |
|      - | 4932 | `	int nLen;` |
|      5 | 4933 | `	if( nArg < 1 ){` |
|      - | 4934 | `		/* Missing arguments,return 0 */` |
|      3 | 4935 | `		ph7_result_int(pCtx,0);` |
|      3 | 4936 | `		return PH7_OK;` |
|      - | 4937 | `	}` |
|      - | 4938 | `	/* Extract the input string */` |
|      3 | 4939 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4940 | `	if( nLen < 1 ){` |
|      - | 4941 | `		/* Empty string */` |
|    ! 0 | 4942 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4943 | `		return PH7_OK;` |
|      - | 4944 | `	}` |
|      - | 4945 | `	/* Calculate the sum */` |
|      3 | 4946 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 4947 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 4948 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 4949 | `	return PH7_OK;` |
|      3 | 4950 |  |
|      - | 4951 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 4952 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4953 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 4954 | `/*` |
|      - | 4955 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 4956 |  |
|      - | 4957 | ` */` |
|      4 | 4958 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 4959 | `	const char *zInput, /* Raw input */` |
|      - | 4960 | `	int nByte,  /* Input length */` |
|      - | 4961 | `	int delim,  /* Delimiter */` |
|      - | 4962 | `	int encl,   /* Enclosure */` |
|      - | 4963 | `	int escape,  /* Escape character */` |
|      - | 4964 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 4965 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 4966 | `	)` |
|      1 | 4967 |  |
|      5 | 4968 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 4969 | `	const char *zIn = zInput;` |
|      - | 4970 | `	const char *zPtr;` |
|      - | 4971 | `	int isEnc;` |
|      - | 4972 | `	/* Start processing */` |
|      8 | 4973 | `	for(;;){` |
|     17 | 4974 | `		if( zIn >= zEnd ){` |
|      - | 4975 | `			/* No more input to process */` |
|      5 | 4976 | `			break;` |
|      - | 4977 | `		}` |
|     13 | 4978 | `		isEnc = 0;` |
|     13 | 4979 | `		zPtr = zIn;` |
|      - | 4980 | `		/* Find the first delimiter */` |
|     27 | 4981 | `		while( zIn < zEnd ){` |
|     23 | 4982 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 4983 | `				/* Delimiter found,break imediately */` |
|      5 | 4984 | `				break;` |
|     15 | 4985 | `			}else if( zIn[0] == encl ){` |
|      - | 4986 | `				/* Inside enclosure? */` |
|    ! 0 | 4987 | `				isEnc = !isEnc;` |
|     15 | 4988 | `			}else if( zIn[0] == escape ){` |
|      - | 4989 | `				/* Escape sequence */` |
|    ! 0 | 4990 | `				zIn++;` |
|    ! 0 | 4991 | `			}` |
|      - | 4992 | `			/* Advance the cursor */` |
|     15 | 4993 | `			zIn++;` |
|      1 | 4994 | `		}` |
|     13 | 4995 | `		if( zIn > zPtr ){` |
|     13 | 4996 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 4997 | `			sxi32 rc;` |
|      - | 4998 | `			/* Invoke the supllied callback */` |
|     13 | 4999 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 5000 | `				zPtr++;` |
|    ! 0 | 5001 | `				nByteChunk-=2;` |
|    ! 0 | 5002 | `			}` |
|     13 | 5003 | `			if( nByteChunk > 0 ){` |
|     13 | 5004 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 5005 | `				if( rc == SXERR_ABORT ){` |
|      - | 5006 | `					/* User callback request an operation abort */` |
|    ! 0 | 5007 | `					break;` |
|      - | 5008 | `				}` |
|      6 | 5009 | `			}` |
|      6 | 5010 | `		}` |
|      - | 5011 | `		/* Ignore trailing delimiter */` |
|     21 | 5012 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 5013 | `			zIn++;` |
|      1 | 5014 | `		}` |
|      1 | 5015 | `	}` |
|      5 | 5016 | `	return SXRET_OK;` |
|      1 | 5017 |  |
|      - | 5018 | `/*` |
|      - | 5019 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 5020 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 5021 | ` * argument to this callback.` |
|      - | 5022 | ` */` |
|     12 | 5023 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 5024 |  |
|     13 | 5025 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 5026 | `	ph7_value sEntry;` |
|      - | 5027 | `	SyString sToken;` |
|      - | 5028 | `	/* Insert the token in the given array */` |
|     13 | 5029 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 5030 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 5031 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 5032 | `	if( sToken.nByte < 1){` |
|    ! 0 | 5033 | `		return SXRET_OK;` |
|      - | 5034 | `	}` |
|     13 | 5035 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 5036 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 5037 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 5038 | `	return SXRET_OK;` |
|      7 | 5039 |  |
|      - | 5040 | `/*` |
|      - | 5041 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 5042 | ` *  Parse a CSV string into an array.` |
|      - | 5043 | ` * Parameters` |
|      - | 5044 | ` *  $input` |
|      - | 5045 | ` *   The string to parse.` |
|      - | 5046 | ` *  $delimiter` |
|      - | 5047 | ` *   Set the field delimiter (one character only).` |
|      - | 5048 | ` *  $enclosure` |
|      - | 5049 | ` *   Set the field enclosure character (one character only).` |
|      - | 5050 | ` *  $escape` |
|      - | 5051 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 5052 | ` * Return` |
|      - | 5053 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 5054 | ` */` |
|      4 | 5055 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5056 |  |
|      - | 5057 | `	const char *zInput,*zPtr;` |
|      - | 5058 | `	ph7_value *pArray;` |
|      5 | 5059 | `	int delim  = ',';   /* Delimiter */` |
|      5 | 5060 | `	int encl   = '"' ;  /* Enclosure */` |
|      5 | 5061 | `	int escape = '\\';  /* Escape character */` |
|      - | 5062 | `	int nLen;` |
|      5 | 5063 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5064 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 5065 | `		ph7_result_null(pCtx);` |
|      3 | 5066 | `		return PH7_OK;` |
|      - | 5067 | `	}` |
|      - | 5068 | `	/* Extract the raw input */` |
|      3 | 5069 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5070 | `	if( nArg > 1 ){` |
|      - | 5071 | `		int i;` |
|      3 | 5072 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 5073 | `			/* Extract the delimiter */` |
|      3 | 5074 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 5075 | `			if( i > 0 ){` |
|      3 | 5076 | `				delim = zPtr[0];` |
|      1 | 5077 | `			}` |
|      1 | 5078 | `		}` |
|      3 | 5079 | `		if( nArg > 2 ){` |
|      3 | 5080 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 5081 | `				/* Extract the enclosure */` |
|      3 | 5082 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 5083 | `				if( i > 0 ){` |
|      3 | 5084 | `					encl = zPtr[0];` |
|      1 | 5085 | `				}` |
|      1 | 5086 | `			}` |
|      3 | 5087 | `			if( nArg > 3 ){` |
|      3 | 5088 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 5089 | `					/* Extract the escape character */` |
|      3 | 5090 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 5091 | `					if( i > 0 ){` |
|      3 | 5092 | `						escape = zPtr[0];` |
|      1 | 5093 | `					}` |
|      1 | 5094 | `				}` |
|      1 | 5095 | `			}` |
|      1 | 5096 | `		}` |
|      1 | 5097 | `	}` |
|      - | 5098 | `	/* Create our array */` |
|      3 | 5099 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5100 | `	if( pArray == 0 ){` |
|    ! 0 | 5101 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 5102 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5103 | `		return PH7_OK;` |
|      - | 5104 | `	}` |
|      - | 5105 | `	/* Parse the raw input */` |
|      3 | 5106 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 5107 | `	/* Return the freshly created array */` |
|      3 | 5108 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5109 | `	return PH7_OK;` |
|      3 | 5110 |  |
|      - | 5111 | `/*` |
|      - | 5112 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 5113 | ` * container.` |
|      - | 5114 | ` * Refer to [strip_tags()].` |
|      - | 5115 | ` */` |
|     10 | 5116 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5117 |  |
|     11 | 5118 | `	const char *zEnd = &zTag[nByte];` |
|      - | 5119 | `	const char *zPtr;` |
|      - | 5120 | `	SyString sEntry;` |
|      - | 5121 | `	/* Strip tags */` |
|     10 | 5122 | `	for(;;){` |
|     45 | 5123 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 5124 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 5125 | `				zTag++;` |
|      1 | 5126 | `		}` |
|     21 | 5127 | `		if( zTag >= zEnd ){` |
|     11 | 5128 | `			break;` |
|      - | 5129 | `		}` |
|     11 | 5130 | `		zPtr = zTag;` |
|      - | 5131 | `		/* Delimit the tag */` |
|     25 | 5132 | `		while(zTag < zEnd ){` |
|     25 | 5133 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5134 | `				/* UTF-8 stream */` |
|      3 | 5135 | `				zTag++;` |
|      5 | 5136 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 5137 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 5138 | `				break;` |
|    ! 0 | 5139 | `			}else{` |
|     13 | 5140 | `				zTag++;` |
|      - | 5141 | `			}` |
|      1 | 5142 | `		}` |
|     11 | 5143 | `		if( zTag > zPtr ){` |
|      - | 5144 | `			/* Perform the insertion */` |
|     11 | 5145 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 5146 | `			SyStringFullTrim(&sEntry);` |
|     11 | 5147 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 5148 | `		}` |
|      - | 5149 | `		/* Jump the trailing '>' */` |
|     11 | 5150 | `		zTag++;` |
|      1 | 5151 | `	}` |
|     11 | 5152 | `	return SXRET_OK;` |
|      1 | 5153 |  |
|      - | 5154 | `/*` |
|      - | 5155 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 5156 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 5157 | ` * Refer to [strip_tags()].` |
|      - | 5158 | ` */` |
|     36 | 5159 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5160 |  |
|     37 | 5161 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 5162 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 5163 | `		SyString sTag;` |
|     85 | 5164 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 5165 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 5166 | `			zTag++;` |
|      1 | 5167 | `		}` |
|      - | 5168 | `		/* Delimit the tag */` |
|     25 | 5169 | `		zCur = zTag;` |
|     77 | 5170 | `		while(zTag < zEnd ){` |
|     77 | 5171 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5172 | `				/* UTF-8 stream */` |
|      5 | 5173 | `				zTag++;` |
|      9 | 5174 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 5175 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 5176 | `				break;` |
|    ! 0 | 5177 | `			}else{` |
|     49 | 5178 | `				zTag++;` |
|      - | 5179 | `			}` |
|      1 | 5180 | `		}` |
|     25 | 5181 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 5182 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 5183 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 5184 | `		if( sTag.nByte > 0 ){` |
|      - | 5185 | `			SyString *aEntry,*pEntry;` |
|      - | 5186 | `			sxi32 rc;` |
|      - | 5187 | `			sxu32 n;` |
|      - | 5188 | `			/* Perform the lookup */` |
|     25 | 5189 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 5190 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 5191 | `				pEntry = &aEntry[n];` |
|      - | 5192 | `				/* Do the comparison */` |
|     25 | 5193 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 5194 | `				if( !rc ){` |
|     21 | 5195 | `					return SXRET_OK;` |
|      - | 5196 | `				}` |
|      3 | 5197 | `			}` |
|      2 | 5198 | `		}` |
|      2 | 5199 | `	}` |
|      - | 5200 | `	/* No such tag */` |
|     17 | 5201 | `	return SXERR_NOTFOUND;` |
|     19 | 5202 |  |
|      - | 5203 | `/*` |
|      - | 5204 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 5205 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 5206 | ` * Refer to [strip_tags()].` |
|      - | 5207 | ` */` |
|     16 | 5208 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 5209 |  |
|     17 | 5210 | `	const char *zEnd = &zIn[nByte];` |
|      - | 5211 | `	const char *zPtr,*zTag;` |
|      - | 5212 | `	SySet sSet;` |
|      - | 5213 | `	/* initialize the set of allowed tags */` |
|     17 | 5214 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 5215 | `	if( nTaglen > 0 ){` |
|      - | 5216 | `		/* Set of allowed tags */` |
|     11 | 5217 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 5218 | `	}` |
|      - | 5219 | `	/* Set the empty string */` |
|     17 | 5220 | `	ph7_result_string(pCtx,"",0);` |
|      - | 5221 | `	/* Start processing */` |
|     26 | 5222 | `	for(;;){` |
|     53 | 5223 | `		if(zIn >= zEnd){` |
|      - | 5224 | `			/* No more input to process */` |
|     15 | 5225 | `			break;` |
|      - | 5226 | `		}` |
|     39 | 5227 | `		zPtr = zIn;` |
|      - | 5228 | `		/* Find a tag */` |
|    133 | 5229 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 5230 | `			zIn++;` |
|      1 | 5231 | `		}` |
|     39 | 5232 | `		if( zIn > zPtr ){` |
|      - | 5233 | `			/* Consume raw input */` |
|     21 | 5234 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 5235 | `		}` |
|      - | 5236 | `		/* Ignore trailing null bytes */` |
|     39 | 5237 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 5238 | `			zIn++;` |
|    ! 0 | 5239 | `		}` |
|     39 | 5240 | `		if(zIn >= zEnd){` |
|      - | 5241 | `			/* No more input to process */` |
|      3 | 5242 | `			break;` |
|      - | 5243 | `		}` |
|     37 | 5244 | `		if( zIn[0] == '<' ){` |
|      - | 5245 | `			sxi32 rc;` |
|     37 | 5246 | `			zTag = zIn++;` |
|      - | 5247 | `			/* Delimit the tag */` |
|    127 | 5248 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 5249 | `				zIn++;` |
|      1 | 5250 | `			}` |
|     37 | 5251 | `			if( zIn < zEnd ){` |
|     37 | 5252 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 5253 | `			}` |
|      - | 5254 | `			/* Query the set */` |
|     37 | 5255 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 5256 | `			if( rc == SXRET_OK ){` |
|      - | 5257 | `				/* Keep the tag */` |
|     21 | 5258 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 5259 | `			}` |
|     18 | 5260 | `		}` |
|      1 | 5261 | `	}` |
|      - | 5262 | `	/* Cleanup */` |
|     17 | 5263 | `	SySetRelease(&sSet);` |
|     17 | 5264 | `	return SXRET_OK;` |
|      1 | 5265 |  |
|      - | 5266 | `/*` |
|      - | 5267 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 5268 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 5269 | ` * Parameters` |
|      - | 5270 | ` *  $str` |
|      - | 5271 | ` *  The input string.` |
|      - | 5272 | ` * $allowable_tags` |
|      - | 5273 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 5274 | ` * Return` |
|      - | 5275 | ` *  Returns the stripped string.` |
|      - | 5276 | ` */` |
|     16 | 5277 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5278 |  |
|     17 | 5279 | `	const char *zTaglist = 0;` |
|      - | 5280 | `	const char *zString;` |
|     17 | 5281 | `	int nTaglen = 0;` |
|      - | 5282 | `	int nLen;` |
|     17 | 5283 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5284 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 5285 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5286 | `		return PH7_OK;` |
|      - | 5287 | `	}` |
|      - | 5288 | `	/* Point to the raw string */` |
|     15 | 5289 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 5290 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 5291 | `		/* Allowed tag */` |
|     11 | 5292 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 5293 | `	}` |
|      - | 5294 | `	/* Process input */` |
|     15 | 5295 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 5296 | `	return PH7_OK;` |
|      9 | 5297 |  |
|      - | 5298 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5299 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5300 | `/*` |
|      - | 5301 | ` * string str_shuffle(string $str)` |
|      - | 5302 |  |
|      - | 5303 | ` *  Randomly shuffles a string.` |
|      - | 5304 | ` * Parameters` |
|      - | 5305 | ` *  $str` |
|      - | 5306 | ` *   The input string.` |
|      - | 5307 | ` * Return` |
|      - | 5308 | ` *  Returns the shuffled string.` |
|      - | 5309 | ` */` |
|     12 | 5310 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5311 |  |
|      - | 5312 | `	const char *zString;` |
|      - | 5313 | `	int nLen,i,c;` |
|      - | 5314 | `	sxu32 iR;` |
|     13 | 5315 | `	if( nArg < 1 ){` |
|      - | 5316 | `		/* Missing arguments,return the empty string */` |
|      3 | 5317 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5318 | `		return PH7_OK;` |
|      - | 5319 | `	}` |
|      - | 5320 | `	/* Extract the target string */` |
|     11 | 5321 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5322 | `	if( nLen < 1 ){` |
|      - | 5323 | `		/* Nothing to shuffle */` |
|      3 | 5324 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5325 | `		return PH7_OK;` |
|      - | 5326 | `	}` |
|      - | 5327 | `	/* Shuffle the string */` |
|     43 | 5328 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 5329 | `		/* Generate a random number first */` |
|     35 | 5330 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 5331 | `		/* Extract a random offset */` |
|     35 | 5332 | `		c = zString[iR % nLen];` |
|      - | 5333 | `		/* Append it */` |
|     35 | 5334 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 5335 | `	}` |
|      9 | 5336 | `	return PH7_OK;` |
|      7 | 5337 |  |
|      - | 5338 | `/*` |
|      - | 5339 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 5340 | ` *  Convert a string to an array.` |
|      - | 5341 | ` * Parameters` |
|      - | 5342 | ` * $str` |
|      - | 5343 | ` *  The input string.` |
|      - | 5344 | ` * $split_length` |
|      - | 5345 | ` *  Maximum length of the chunk.` |
|      - | 5346 | ` * Return` |
|      - | 5347 | ` *  If the optional split_length parameter is specified, the returned array` |
|      - | 5348 | ` *  will be broken down into chunks with each being split_length in length, otherwise` |
|      - | 5349 | ` *  each chunk will be one character in length. FALSE is returned if split_length is less than 1.` |
|      - | 5350 | ` *  If the split_length length exceeds the length of string, the entire string is returned` |
|      - | 5351 | ` *  as the first (and only) array element.` |
|      - | 5352 | ` */` |
|      8 | 5353 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5354 |  |
|      - | 5355 | `	const char *zString,*zEnd;` |
|      - | 5356 | `	ph7_value *pArray,*pValue;` |
|      - | 5357 | `	int split_len;` |
|      - | 5358 | `	int nLen;` |
|      9 | 5359 | `	if( nArg < 1 ){` |
|      - | 5360 | `		/* Missing arguments,return FALSE */` |
|      5 | 5361 | `		ph7_result_bool(pCtx,0);` |
|      5 | 5362 | `		return PH7_OK;` |
|      - | 5363 | `	}` |
|      - | 5364 | `	/* Point to the target string */` |
|      5 | 5365 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 5366 | `	if( nLen < 1 ){` |
|      - | 5367 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 5368 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5369 | `		return PH7_OK;` |
|      - | 5370 | `	}` |
|      5 | 5371 | `	split_len = (int)sizeof(char);` |
|      5 | 5372 | `	if( nArg > 1 ){` |
|      - | 5373 | `		/* Split length */` |
|      5 | 5374 | `		split_len = ph7_value_to_int(apArg[1]);` |
|      5 | 5375 | `		if( split_len < 1 ){` |
|      - | 5376 | `			/* Invalid length,return FALSE */` |
|      3 | 5377 | `			ph7_result_bool(pCtx,0);` |
|      3 | 5378 | `			return PH7_OK;` |
|      - | 5379 | `		}` |
|      3 | 5380 | `		if( split_len > nLen ){` |
|    ! 0 | 5381 | `			split_len = nLen;` |
|    ! 0 | 5382 | `		}` |
|      1 | 5383 | `	}` |
|      - | 5384 | `	/* Create the array and the scalar value */` |
|      3 | 5385 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 5386 | `	/*Chunk value */` |
|      3 | 5387 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 5388 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 5389 | `		/* Return FALSE */` |
|    ! 0 | 5390 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5391 | `		return PH7_OK;` |
|      - | 5392 | `	}` |
|      - | 5393 | `	/* Point to the end of the string */` |
|      3 | 5394 | `	zEnd = &zString[nLen];` |
|      - | 5395 | `	/* Perform the requested operation */` |
|      7 | 5396 | `	for(;;){` |
|      - | 5397 | `		int nMax;` |
|      9 | 5398 | `		if( zString >= zEnd ){` |
|      - | 5399 | `			/* No more input to process */` |
|      3 | 5400 | `			break;` |
|      - | 5401 | `		}` |
|      7 | 5402 | `		nMax = (int)(zEnd-zString);` |
|      7 | 5403 | `		if( nMax < split_len ){` |
|    ! 0 | 5404 | `			split_len = nMax;` |
|    ! 0 | 5405 | `		}` |
|      - | 5406 | `		/* Copy the current chunk */` |
|      7 | 5407 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 5408 | `		/* Insert it */` |
|      7 | 5409 | `		ph7_array_add_elem(pArray,0,pValue); /* Will make it's own copy */` |
|      - | 5410 | `		/* reset the string cursor */` |
|      7 | 5411 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 5412 | `		/* Update position */` |
|      7 | 5413 | `		zString += split_len;` |
|      1 | 5414 | `	}` |
|      - | 5415 | `	/*` |
|      - | 5416 | `	 * Return the array.` |
|      - | 5417 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 5418 | `	 * upon we return from this function.` |
|      - | 5419 | `	 */` |
|      3 | 5420 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5421 | `	return PH7_OK;` |
|      5 | 5422 |  |
|      - | 5423 | `/*` |
|      - | 5424 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 5425 | ` * Refer to [strspn()].` |
|      - | 5426 | ` */` |
|     28 | 5427 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 5428 |  |
|     29 | 5429 | `	const char *zIn = *pzIn;` |
|      - | 5430 | `	const char *zPtr;` |
|      - | 5431 | `	/* Ignore leading white spaces */` |
|     29 | 5432 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 5433 | `		zIn++;` |
|    ! 0 | 5434 | `	}` |
|     29 | 5435 | `	if( zIn >= zEnd ){` |
|      - | 5436 | `		/* End of input */` |
|    ! 0 | 5437 | `		return SXERR_EOF;` |
|      - | 5438 | `	}` |
|     29 | 5439 | `	zPtr = zIn;` |
|      - | 5440 | `	/* Extract the token */` |
|    201 | 5441 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 5442 | `		zIn++;` |
|      1 | 5443 | `	}` |
|     29 | 5444 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5445 | `	/* Synchronize pointers */` |
|     29 | 5446 | `	*pzIn = zIn;` |
|      - | 5447 | `	/* Return to the caller */` |
|     29 | 5448 | `	return SXRET_OK;` |
|     15 | 5449 |  |
|      - | 5450 | `/*` |
|      - | 5451 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 5452 | ` * return the longest match.` |
|      - | 5453 | ` * Refer to [strspn()].` |
|      - | 5454 | ` */` |
|     18 | 5455 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5456 |  |
|     19 | 5457 | `	const char *zEnd = &zString[nLen];` |
|     19 | 5458 | `	const char *zIn = zString;` |
|      - | 5459 | `	int i,c;` |
|     45 | 5460 | `	for(;;){` |
|     91 | 5461 | `		if( zString >= zEnd ){` |
|      7 | 5462 | `			break;` |
|      - | 5463 | `		}` |
|      - | 5464 | `		/* Extract current character */` |
|     85 | 5465 | `		c = zString[0];` |
|      - | 5466 | `		/* Perform the lookup */` |
|    383 | 5467 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 5468 | `			if( c == zMask[i] ){` |
|      - | 5469 | `				/* Character found */` |
|     73 | 5470 | `				break;` |
|      - | 5471 | `			}` |
|    150 | 5472 | `		}` |
|     85 | 5473 | `		if( i >= nMaskLen ){` |
|      - | 5474 | `			/* Character not in the current mask,break immediately */` |
|     13 | 5475 | `			break;` |
|      - | 5476 | `		}` |
|      - | 5477 | `		/* Advance cursor */` |
|     73 | 5478 | `		zString++;` |
|      1 | 5479 | `	}` |
|      - | 5480 | `	/* Longest match */` |
|     19 | 5481 | `	return (int)(zString-zIn);` |
|      1 | 5482 |  |
|      - | 5483 | `/*` |
|      - | 5484 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 5485 | ` * Refer to [strcspn()].` |
|      - | 5486 | ` */` |
|     10 | 5487 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5488 |  |
|     11 | 5489 | `	const char *zEnd = &zString[nLen];` |
|     11 | 5490 | `	const char *zIn = zString;` |
|      - | 5491 | `	int i,c;` |
|     12 | 5492 | `	for(;;){` |
|     25 | 5493 | `		if( zString >= zEnd ){` |
|      3 | 5494 | `			break;` |
|      - | 5495 | `		}` |
|      - | 5496 | `		/* Extract current character */` |
|     23 | 5497 | `		c = zString[0];` |
|      - | 5498 | `		/* Perform the lookup */` |
|     51 | 5499 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 5500 | `			if( c == zMask[i] ){` |
|      9 | 5501 | `				break;` |
|      - | 5502 | `			}` |
|     15 | 5503 | `		}` |
|     23 | 5504 | `		if( i < nMaskLen ){` |
|      - | 5505 | `			/* Character in the current mask,break immediately */` |
|      9 | 5506 | `			break;` |
|      - | 5507 | `		}` |
|      - | 5508 | `		/* Advance cursor */` |
|     15 | 5509 | `		zString++;` |
|      1 | 5510 | `	}` |
|      - | 5511 | `	/* Longest match */` |
|     11 | 5512 | `	return (int)(zString-zIn);` |
|      1 | 5513 |  |
|      - | 5514 | `/*` |
|      - | 5515 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5516 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 5517 | ` *  of characters contained within a given mask.` |
|      - | 5518 | ` * Parameters` |
|      - | 5519 | ` * $str` |
|      - | 5520 | ` *  The input string.` |
|      - | 5521 | ` * $mask` |
|      - | 5522 | ` *  The list of allowable characters.` |
|      - | 5523 | ` * $start` |
|      - | 5524 | ` *  The position in subject to start searching.` |
|      - | 5525 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5526 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5527 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5528 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5529 | ` *  start'th position from the end of subject.` |
|      - | 5530 | ` * $length` |
|      - | 5531 | ` *  The length of the segment from subject to examine.` |
|      - | 5532 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5533 | ` *  characters after the starting position.` |
|      - | 5534 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5535 | ` *  position up to length characters from the end of subject.` |
|      - | 5536 | ` * Return` |
|      - | 5537 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 5538 | ` * in mask.` |
|      - | 5539 | ` */` |
|     26 | 5540 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5541 |  |
|      - | 5542 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5543 | `	int iMasklen,iLen;` |
|      - | 5544 | `	SyString sToken;` |
|     27 | 5545 | `	int iCount = 0;` |
|      - | 5546 | `	int rc;` |
|     27 | 5547 | `	if( nArg < 2 ){` |
|      - | 5548 | `		/* Missing agruments,return zero */` |
|      3 | 5549 | `		ph7_result_int(pCtx,0);` |
|      3 | 5550 | `		return PH7_OK;` |
|      - | 5551 | `	}` |
|      - | 5552 | `	/* Extract the target string */` |
|     25 | 5553 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5554 | `	/* Extract the mask */` |
|     25 | 5555 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 5556 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 5557 | `		/* Nothing to process,return zero */` |
|      7 | 5558 | `		ph7_result_int(pCtx,0);` |
|      7 | 5559 | `		return PH7_OK;` |
|      - | 5560 | `	}` |
|     19 | 5561 | `	if( nArg > 2 ){` |
|      - | 5562 | `		int nOfft;` |
|      - | 5563 | `		/* Extract the offset */` |
|      9 | 5564 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 5565 | `		if( nOfft < 0 ){` |
|    ! 0 | 5566 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5567 | `			if( zBase > zString ){` |
|    ! 0 | 5568 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5569 | `				zString = zBase;` |
|    ! 0 | 5570 | `			}else{` |
|      - | 5571 | `				/* Invalid offset */` |
|    ! 0 | 5572 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5573 | `				return PH7_OK;` |
|      - | 5574 | `			}` |
|    ! 0 | 5575 | `		}else{` |
|      9 | 5576 | `			if( nOfft >= iLen ){` |
|      - | 5577 | `				/* Invalid offset */` |
|    ! 0 | 5578 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5579 | `				return PH7_OK;` |
|    ! 0 | 5580 | `			}else{` |
|      - | 5581 | `				/* Update offset */` |
|      9 | 5582 | `				zString += nOfft;` |
|      9 | 5583 | `				iLen -= nOfft;` |
|      - | 5584 | `			}` |
|      - | 5585 | `		}` |
|      9 | 5586 | `		if( nArg > 3 ){` |
|      - | 5587 | `			int iUserlen;` |
|      - | 5588 | `			/* Extract the desired length */` |
|      9 | 5589 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 5590 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 5591 | `				iLen = iUserlen;` |
|      2 | 5592 | `			}` |
|      4 | 5593 | `		}` |
|      4 | 5594 | `	}` |
|      - | 5595 | `	/* Point to the end of the string */` |
|     19 | 5596 | `	zEnd = &zString[iLen];` |
|      - | 5597 | `	/* Extract the first non-space token */` |
|     19 | 5598 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 5599 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5600 | `		/* Compare against the current mask */` |
|     19 | 5601 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 5602 | `	}` |
|      - | 5603 | `	/* Longest match */` |
|     19 | 5604 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 5605 | `	return PH7_OK;` |
|     14 | 5606 |  |
|      - | 5607 | `/*` |
|      - | 5608 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5609 | ` *  Find length of initial segment not matching mask.` |
|      - | 5610 | ` * Parameters` |
|      - | 5611 | ` * $str` |
|      - | 5612 | ` *  The input string.` |
|      - | 5613 | ` * $mask` |
|      - | 5614 | ` *  The list of not allowed characters.` |
|      - | 5615 | ` * $start` |
|      - | 5616 | ` *  The position in subject to start searching.` |
|      - | 5617 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5618 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5619 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5620 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5621 | ` *  start'th position from the end of subject.` |
|      - | 5622 | ` * $length` |
|      - | 5623 | ` *  The length of the segment from subject to examine.` |
|      - | 5624 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5625 | ` *  characters after the starting position.` |
|      - | 5626 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5627 | ` *  position up to length characters from the end of subject.` |
|      - | 5628 | ` * Return` |
|      - | 5629 | ` *  Returns the length of the segment as an integer.` |
|      - | 5630 | ` */` |
|     16 | 5631 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5632 |  |
|      - | 5633 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5634 | `	int iMasklen,iLen;` |
|      - | 5635 | `	SyString sToken;` |
|     17 | 5636 | `	int iCount = 0;` |
|      - | 5637 | `	int rc;` |
|     17 | 5638 | `	if( nArg < 2 ){` |
|      - | 5639 | `		/* Missing agruments,return zero */` |
|      3 | 5640 | `		ph7_result_int(pCtx,0);` |
|      3 | 5641 | `		return PH7_OK;` |
|      - | 5642 | `	}` |
|      - | 5643 | `	/* Extract the target string */` |
|     15 | 5644 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5645 | `	/* Extract the mask */` |
|     15 | 5646 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 5647 | `	if( iLen < 1 ){` |
|      - | 5648 | `		/* Nothing to process,return zero */` |
|    ! 0 | 5649 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5650 | `		return PH7_OK;` |
|      - | 5651 | `	}` |
|     15 | 5652 | `	if( iMasklen < 1 ){` |
|      - | 5653 | `		/* No given mask,return the string length */` |
|      3 | 5654 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 5655 | `		return PH7_OK;` |
|      - | 5656 | `	}` |
|     13 | 5657 | `	if( nArg > 2 ){` |
|      - | 5658 | `		int nOfft;` |
|      - | 5659 | `		/* Extract the offset */` |
|     11 | 5660 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 5661 | `		if( nOfft < 0 ){` |
|    ! 0 | 5662 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5663 | `			if( zBase > zString ){` |
|    ! 0 | 5664 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5665 | `				zString = zBase;` |
|    ! 0 | 5666 | `			}else{` |
|      - | 5667 | `				/* Invalid offset */` |
|    ! 0 | 5668 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5669 | `				return PH7_OK;` |
|      - | 5670 | `			}` |
|    ! 0 | 5671 | `		}else{` |
|     11 | 5672 | `			if( nOfft >= iLen ){` |
|      - | 5673 | `				/* Invalid offset */` |
|      3 | 5674 | `				ph7_result_int(pCtx,0);` |
|      3 | 5675 | `				return PH7_OK;` |
|    ! 0 | 5676 | `			}else{` |
|      - | 5677 | `				/* Update offset */` |
|      9 | 5678 | `				zString += nOfft;` |
|      9 | 5679 | `				iLen -= nOfft;` |
|      - | 5680 | `			}` |
|      - | 5681 | `		}` |
|      9 | 5682 | `		if( nArg > 3 ){` |
|      - | 5683 | `			int iUserlen;` |
|      - | 5684 | `			/* Extract the desired length */` |
|    ! 0 | 5685 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 5686 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 5687 | `				iLen = iUserlen;` |
|    ! 0 | 5688 | `			}` |
|    ! 0 | 5689 | `		}` |
|      4 | 5690 | `	}` |
|      - | 5691 | `	/* Point to the end of the string */` |
|     11 | 5692 | `	zEnd = &zString[iLen];` |
|      - | 5693 | `	/* Extract the first non-space token */` |
|     11 | 5694 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 5695 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5696 | `		/* Compare against the current mask */` |
|     11 | 5697 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 5698 | `	}` |
|      - | 5699 | `	/* Longest match */` |
|     11 | 5700 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 5701 | `	return PH7_OK;` |
|      9 | 5702 |  |
|      - | 5703 | `/*` |
|      - | 5704 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 5705 | ` *  Search a string for any of a set of characters.` |
|      - | 5706 | ` * Parameters` |
|      - | 5707 | ` *  $haystack` |
|      - | 5708 | ` *   The string where char_list is looked for.` |
|      - | 5709 | ` *  $char_list` |
|      - | 5710 | ` *   This parameter is case sensitive.` |
|      - | 5711 | ` * Return` |
|      - | 5712 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 5713 | ` */` |
|      6 | 5714 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5715 |  |
|      - | 5716 | `	const char *zString,*zList,*zEnd;` |
|      - | 5717 | `	int iLen,iListLen,i,c;` |
|      - | 5718 | `	sxu32 nOfft,nMax;` |
|      - | 5719 | `	sxi32 rc;` |
|      7 | 5720 | `	if( nArg < 2 ){` |
|      - | 5721 | `		/* Missing arguments,return FALSE */` |
|      3 | 5722 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5723 | `		return PH7_OK;` |
|      - | 5724 | `	}` |
|      - | 5725 | `	/* Extract the haystack and the char list */` |
|      5 | 5726 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 5727 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 5728 | `	if( iLen < 1 ){` |
|      - | 5729 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 5730 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5731 | `		return PH7_OK;` |
|      - | 5732 | `	}` |
|      - | 5733 | `	/* Point to the end of the string */` |
|      5 | 5734 | `	zEnd = &zString[iLen];` |
|      5 | 5735 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 5736 | `	/* perform the requested operation */` |
|     15 | 5737 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 5738 | `		c = zList[i];` |
|     11 | 5739 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 5740 | `		if( rc == SXRET_OK ){` |
|      5 | 5741 | `			if( nMax < nOfft ){` |
|      3 | 5742 | `				nOfft = nMax;` |
|      1 | 5743 | `			}` |
|      2 | 5744 | `		}` |
|      6 | 5745 | `	}` |
|      5 | 5746 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 5747 | `		/* No such substring,return FALSE */` |
|      3 | 5748 | `		ph7_result_bool(pCtx,0);` |
|      2 | 5749 | `	}else{` |
|      - | 5750 | `		/* Return the substring */` |
|      3 | 5751 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 5752 | `	}` |
|      5 | 5753 | `	return PH7_OK;` |
|      4 | 5754 |  |
|      - | 5755 | `/*` |
|      - | 5756 | ` * string soundex(string $str)` |
|      - | 5757 | ` *  Calculate the soundex key of a string.` |
|      - | 5758 | ` * Parameters` |
|      - | 5759 | ` *  $str` |
|      - | 5760 | ` *   The input string.` |
|      - | 5761 | ` * Return` |
|      - | 5762 | ` *  Returns the soundex key as a string.` |
|      - | 5763 | ` * Note:` |
|      - | 5764 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 5765 | ` * source tree.` |
|      - | 5766 | ` */` |
|     20 | 5767 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5768 |  |
|      - | 5769 | `	const unsigned char *zIn;` |
|      - | 5770 | `	char zResult[8];` |
|      - | 5771 | `	int i, j;` |
|      - | 5772 | `	static const unsigned char iCode[] = {` |
|      - | 5773 |  |
|      - | 5774 |  |
|      - | 5775 |  |
|      - | 5776 |  |
|      - | 5777 |  |
|      - | 5778 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5779 |  |
|      - | 5780 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5781 | `	};` |
|     21 | 5782 | `	if( nArg < 1 ){` |
|      - | 5783 | `		/* Missing arguments,return the empty string */` |
|      3 | 5784 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5785 | `		return PH7_OK;` |
|      - | 5786 | `	}` |
|     19 | 5787 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     19 | 5788 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     19 | 5789 | `	if( zIn[i] ){` |
|     17 | 5790 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 5791 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 5792 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 5793 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 5794 | `			if( code>0 ){` |
|     45 | 5795 | `				if( code!=prevcode ){` |
|     33 | 5796 | `					prevcode = (unsigned char)code;` |
|     33 | 5797 | `					zResult[j++] = (char)code + '0';` |
|     16 | 5798 | `				}` |
|     23 | 5799 | `			}else{` |
|     49 | 5800 | `				prevcode = 0;` |
|      - | 5801 | `			}` |
|     47 | 5802 | `		}` |
|     33 | 5803 | `		while( j<4 ){` |
|     17 | 5804 | `			zResult[j++] = '0';` |
|      1 | 5805 | `		}` |
|     17 | 5806 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 5807 | `	}else{` |
|      3 | 5808 | `	  ph7_result_string(pCtx,"?000",4);` |
|      - | 5809 | `	}` |
|     19 | 5810 | `	return PH7_OK;` |
|     11 | 5811 |  |
|      - | 5812 | `/*` |
|      - | 5813 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 5814 | ` *  Wraps a string to a given number of characters.` |
|      - | 5815 | ` * Parameters` |
|      - | 5816 | ` *  $str` |
|      - | 5817 | ` *   The input string.` |
|      - | 5818 | ` * $width` |
|      - | 5819 | ` *  The column width.` |
|      - | 5820 | ` * $break` |
|      - | 5821 | ` *  The line is broken using the optional break parameter.` |
|      - | 5822 | ` * Return` |
|      - | 5823 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 5824 | ` */` |
|     14 | 5825 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5826 |  |
|      - | 5827 | `	const char *zIn,*zEnd,*zBreak;` |
|      - | 5828 | `	int iLen,iBreaklen,iChunk;` |
|     15 | 5829 | `	if( nArg < 1 ){` |
|      - | 5830 | `		/* Missing arguments,return the empty string */` |
|      3 | 5831 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5832 | `		return PH7_OK;` |
|      - | 5833 | `	}` |
|      - | 5834 | `	/* Extract the input string */` |
|     13 | 5835 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|     13 | 5836 | `	if( iLen < 1 ){` |
|      - | 5837 | `		/* Nothing to process,return the empty string */` |
|      3 | 5838 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5839 | `		return PH7_OK;` |
|      - | 5840 | `	}` |
|      - | 5841 | `	/* Chunk length */` |
|     11 | 5842 | `	iChunk = 75;` |
|     11 | 5843 | `	iBreaklen = 0;` |
|     11 | 5844 | `	zBreak = ""; /* cc warning */` |
|     11 | 5845 | `	if( nArg > 1 ){` |
|     11 | 5846 | `		iChunk = ph7_value_to_int(apArg[1]);` |
|     11 | 5847 | `		if( iChunk < 1 ){` |
|    ! 0 | 5848 | `			iChunk = 75;` |
|    ! 0 | 5849 | `		}` |
|     11 | 5850 | `		if( nArg > 2 ){` |
|      3 | 5851 | `			zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      1 | 5852 | `		}` |
|      5 | 5853 | `	}` |
|     11 | 5854 | `	if( iBreaklen < 1 ){` |
|      - | 5855 | `		/* Set a default column break */` |
|      - | 5856 | `#ifdef __WINNT__` |
|      1 | 5857 | `		zBreak = "\r\n";` |
|      1 | 5858 | `		iBreaklen = (int)sizeof("\r\n")-1;` |
|      - | 5859 | `#else` |
|      8 | 5860 | `		zBreak = "\n";` |
|      8 | 5861 | `		iBreaklen = (int)sizeof(char);` |
|      - | 5862 | `#endif` |
|      4 | 5863 | `	}` |
|      - | 5864 | `	/* Perform the requested operation */` |
|     11 | 5865 | `	zEnd = &zIn[iLen];` |
|     41 | 5866 | `	for(;;){` |
|      - | 5867 | `		int nMax;` |
|     47 | 5868 | `		if( zIn >= zEnd ){` |
|      - | 5869 | `			/* No more input to process */` |
|     11 | 5870 | `			break;` |
|      - | 5871 | `		}` |
|     37 | 5872 | `		nMax = (int)(zEnd-zIn);` |
|     37 | 5873 | `		if( iChunk > nMax ){` |
|     11 | 5874 | `			iChunk = nMax;` |
|      5 | 5875 | `		}` |
|      - | 5876 | `		/* Append the column first */` |
|     37 | 5877 | `		ph7_result_string(pCtx,zIn,iChunk); /* Will make it's own copy */` |
|      - | 5878 | `		/* Advance the cursor */` |
|     37 | 5879 | `		zIn += iChunk;` |
|     37 | 5880 | `		if( zIn < zEnd ){` |
|      - | 5881 | `			/* Append the line break */` |
|     27 | 5882 | `			ph7_result_string(pCtx,zBreak,iBreaklen);` |
|     13 | 5883 | `		}` |
|      1 | 5884 | `	}` |
|     11 | 5885 | `	return PH7_OK;` |
|      8 | 5886 |  |
|      - | 5887 | `/*` |
|      - | 5888 | ` * Check if the given character is a member of the given mask.` |
|      - | 5889 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 5890 | ` * Refer to [strtok()].` |
|      - | 5891 | ` */` |
|     30 | 5892 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 5893 |  |
|      - | 5894 | `	int i;` |
|     57 | 5895 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 5896 | `		if( c == zMask[i] ){` |
|     13 | 5897 | `			if( pOfft ){` |
|      5 | 5898 | `				*pOfft = i;` |
|      2 | 5899 | `			}` |
|     13 | 5900 | `			return TRUE;` |
|      - | 5901 | `		}` |
|     14 | 5902 | `	}` |
|     19 | 5903 | `	return FALSE;` |
|     16 | 5904 |  |
|      - | 5905 | `/*` |
|      - | 5906 | ` * Extract a single token from the input stream.` |
|      - | 5907 | ` * Refer to [strtok()].` |
|      - | 5908 | ` */` |
|      6 | 5909 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 5910 |  |
|      7 | 5911 | `	const char *zIn = *pzIn;` |
|      - | 5912 | `	const char *zPtr;` |
|      - | 5913 | `	/* Ignore leading delimiter */` |
|     11 | 5914 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5915 | `		zIn++;` |
|      1 | 5916 | `	}` |
|      7 | 5917 | `	if( zIn >= zEnd ){` |
|      - | 5918 | `		/* End of input */` |
|    ! 0 | 5919 | `		return SXERR_EOF;` |
|      - | 5920 | `	}` |
|      7 | 5921 | `	zPtr = zIn;` |
|      - | 5922 | `	/* Extract the token */` |
|     13 | 5923 | `	while( zIn < zEnd ){` |
|     11 | 5924 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 5925 | `			/* UTF-8 stream */` |
|    ! 0 | 5926 | `			zIn++;` |
|    ! 0 | 5927 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 5928 | `		}else{` |
|     11 | 5929 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5930 | `				break;` |
|      - | 5931 | `			}` |
|      7 | 5932 | `			zIn++;` |
|      - | 5933 | `		}` |
|      1 | 5934 | `	}` |
|      7 | 5935 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5936 | `	/* Update the cursor */` |
|      7 | 5937 | `	*pzIn = zIn;` |
|      - | 5938 | `	/* Return to the caller */` |
|      7 | 5939 | `	return SXRET_OK;` |
|      4 | 5940 |  |
|      - | 5941 | `/* strtok auxiliary private data */` |
|      - | 5942 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 5943 | `struct strtok_aux_data` |
|      - | 5944 |  |
|      - | 5945 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 5946 | `	const char *zIn;   /* Current input stream */` |
|      - | 5947 | `	const char *zEnd;  /* End of input */` |
|      - | 5948 | `};` |
|      - | 5949 | `/*` |
|      - | 5950 | ` * string strtok(string $str,string $token)` |
|      - | 5951 | ` * string strtok(string $token)` |
|      - | 5952 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 5953 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 5954 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 5955 | ` *  words by using the space character as the token.` |
|      - | 5956 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 5957 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 5958 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 5959 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 5960 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 5961 | ` *  the argument are found.` |
|      - | 5962 | ` * Parameters` |
|      - | 5963 | ` *  $str` |
|      - | 5964 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 5965 | ` * $token` |
|      - | 5966 | ` *  The delimiter used when splitting up str.` |
|      - | 5967 | ` * Return` |
|      - | 5968 | ` *   Current token or FALSE on EOF.` |
|      - | 5969 | ` */` |
|      8 | 5970 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5971 |  |
|      - | 5972 | `	strtok_aux_data *pAux;` |
|      - | 5973 | `	const char *zMask;` |
|      - | 5974 | `	SyString sToken;` |
|      - | 5975 | `	int nMasklen;` |
|      - | 5976 | `	sxi32 rc;` |
|      9 | 5977 | `	if( nArg < 2 ){` |
|      - | 5978 | `		/* Extract top aux data */` |
|      7 | 5979 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      7 | 5980 | `		if( pAux == 0 ){` |
|      - | 5981 | `			/* No aux data,return FALSE */` |
|    ! 0 | 5982 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5983 | `			return PH7_OK;` |
|      - | 5984 | `		}` |
|      7 | 5985 | `		nMasklen = 0;` |
|      7 | 5986 | `		zMask = ""; /* cc warning */` |
|      7 | 5987 | `		if( nArg > 0 ){` |
|      - | 5988 | `			/* Extract the mask */` |
|      5 | 5989 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 5990 | `		}` |
|      7 | 5991 | `		if( nMasklen < 1 ){` |
|      - | 5992 | `			/* Invalid mask,return FALSE */` |
|      3 | 5993 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|      3 | 5994 | `			ph7_context_free_chunk(pCtx,pAux);` |
|      3 | 5995 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|      3 | 5996 | `			ph7_result_bool(pCtx,0);` |
|      3 | 5997 | `			return PH7_OK;` |
|      - | 5998 | `		}` |
|      - | 5999 | `		/* Extract the token */` |
|      5 | 6000 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 6001 | `		if( rc != SXRET_OK ){` |
|      - | 6002 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 6003 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 6004 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6005 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 6006 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6007 | `		}else{` |
|      - | 6008 | `			/* Return the extracted token */` |
|      5 | 6009 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6010 | `		}` |
|      3 | 6011 | `	}else{` |
|      - | 6012 | `		const char *zInput,*zCur;` |
|      - | 6013 | `		char *zDup;` |
|      - | 6014 | `		int nLen;` |
|      - | 6015 | `		/* Extract the raw input */` |
|      3 | 6016 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 6017 | `		if( nLen < 1 ){` |
|      - | 6018 | `			/* Empty input,return FALSE */` |
|    ! 0 | 6019 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6020 | `			return PH7_OK;` |
|      - | 6021 | `		}` |
|      - | 6022 | `		/* Extract the mask */` |
|      3 | 6023 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 6024 | `		if( nMasklen < 1 ){` |
|      - | 6025 | `			/* Set a default mask */` |
|      - | 6026 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 6027 | `			zMask = TOK_MASK;` |
|    ! 0 | 6028 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 6029 | `#undef TOK_MASK` |
|    ! 0 | 6030 | `		}` |
|      - | 6031 | `		/* Extract a single token */` |
|      3 | 6032 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 6033 | `		if( rc != SXRET_OK ){` |
|      - | 6034 | `			/* Empty input */` |
|    ! 0 | 6035 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6036 | `			return PH7_OK;` |
|    ! 0 | 6037 | `		}else{` |
|      - | 6038 | `			/* Return the extracted token */` |
|      3 | 6039 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6040 | `		}` |
|      - | 6041 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 6042 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 6043 | `		if( pAux ){` |
|      3 | 6044 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 6045 | `			if( nLen < 1 ){` |
|    ! 0 | 6046 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6047 | `				return PH7_OK;` |
|      - | 6048 | `			}` |
|      - | 6049 | `			/* Duplicate input */` |
|      3 | 6050 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 6051 | `			if( zDup  ){` |
|      3 | 6052 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 6053 | `				/* Register the aux data */` |
|      3 | 6054 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 6055 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 6056 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 6057 | `			}` |
|      1 | 6058 | `		}` |
|      - | 6059 | `	}` |
|      7 | 6060 | `	return PH7_OK;` |
|      5 | 6061 |  |
|      - | 6062 | `/*` |
|      - | 6063 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 6064 | ` *  Pad a string to a certain length with another string` |
|      - | 6065 | ` * Parameters` |
|      - | 6066 | ` *  $input` |
|      - | 6067 | ` *   The input string.` |
|      - | 6068 | ` * $pad_length` |
|      - | 6069 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 6070 | ` *   string, no padding takes place.` |
|      - | 6071 | ` * $pad_string` |
|      - | 6072 | ` *   Note:` |
|      - | 6073 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 6074 | ` *    divided by the pad_string's length.` |
|      - | 6075 | ` * $pad_type` |
|      - | 6076 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 6077 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 6078 | ` * Return` |
|      - | 6079 | ` *  The padded string.` |
|      - | 6080 | ` */` |
|     10 | 6081 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6082 |  |
|      - | 6083 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 6084 | `	const char *zIn,*zPad;` |
|     11 | 6085 | `	if( nArg < 2 ){` |
|      - | 6086 | `		/* Missing arguments,return the empty string */` |
|      5 | 6087 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 6088 | `		return PH7_OK;` |
|      - | 6089 | `	}` |
|      - | 6090 | `	/* Extract the target string */` |
|      7 | 6091 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6092 | `	/* Padding length */` |
|      7 | 6093 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|      7 | 6094 | `	if( iPadlen > 0 ){` |
|      5 | 6095 | `		iPadlen -= iLen;` |
|      2 | 6096 | `	}` |
|      7 | 6097 | `	if( iPadlen < 1  ){` |
|      - | 6098 | `		/* Return the string verbatim */` |
|      3 | 6099 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      3 | 6100 | `		return PH7_OK;` |
|      - | 6101 | `	}` |
|      5 | 6102 | `	zPad = " "; /* Whitespace padding */` |
|      5 | 6103 | `	iStrpad = (int)sizeof(char);` |
|      5 | 6104 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      5 | 6105 | `	if( nArg > 2 ){` |
|      - | 6106 | `		/* Padding string */` |
|      5 | 6107 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      5 | 6108 | `		if( iStrpad < 1 ){` |
|      - | 6109 | `			/* Empty string */` |
|    ! 0 | 6110 | `			zPad = " "; /* Whitespace padding */` |
|    ! 0 | 6111 | `			iStrpad = (int)sizeof(char);` |
|    ! 0 | 6112 | `		}` |
|      5 | 6113 | `		if( nArg > 3 ){` |
|      - | 6114 | `			/* Padd type */` |
|      5 | 6115 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 6116 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 6117 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 6118 | `			}` |
|      2 | 6119 | `		}` |
|      2 | 6120 | `	}` |
|      5 | 6121 | `	iDiv = 1;` |
|      5 | 6122 | `	if( iType == 2 ){` |
|    ! 0 | 6123 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 6124 | `	}` |
|      - | 6125 | `	/* Perform the requested operation */` |
|      5 | 6126 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 6127 | `		jPad = iStrpad;` |
|      5 | 6128 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 6129 | `			/* Padding */` |
|      5 | 6130 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 6131 | `				break;` |
|      - | 6132 | `			}` |
|      3 | 6133 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      2 | 6134 | `		}` |
|      3 | 6135 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 6136 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 6137 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 6138 | `				if( jPad > iStrpad ){` |
|    ! 0 | 6139 | `					jPad = iStrpad;` |
|    ! 0 | 6140 | `				}` |
|      3 | 6141 | `				if( jPad < 1){` |
|    ! 0 | 6142 | `					break;` |
|      - | 6143 | `				}` |
|      3 | 6144 | `				ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 6145 | `			}` |
|      1 | 6146 | `		}` |
|      1 | 6147 | `	}` |
|      5 | 6148 | `	if( iLen > 0 ){` |
|      - | 6149 | `		/* Append the input string */` |
|      5 | 6150 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      2 | 6151 | `	}` |
|      5 | 6152 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 6153 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 6154 | `			/* Padding */` |
|      5 | 6155 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 6156 | `				break;` |
|      - | 6157 | `			}` |
|      3 | 6158 | `			ph7_result_string(pCtx,zPad,iStrpad);` |
|      2 | 6159 | `		}` |
|      5 | 6160 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 6161 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 6162 | `			if( jPad > iStrpad ){` |
|    ! 0 | 6163 | `				jPad = iStrpad;` |
|    ! 0 | 6164 | `			}` |
|      3 | 6165 | `			if( jPad < 1){` |
|    ! 0 | 6166 | `				break;` |
|      - | 6167 | `			}` |
|      3 | 6168 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 6169 | `		}` |
|      1 | 6170 | `	}` |
|      5 | 6171 | `	return PH7_OK;` |
|      6 | 6172 |  |
|      - | 6173 | `/*` |
|      - | 6174 | ` * String replacement private data.` |
|      - | 6175 | ` */` |
|      - | 6176 | `typedef struct str_replace_data str_replace_data;` |
|      - | 6177 | `struct str_replace_data` |
|      - | 6178 |  |
|      - | 6179 | `	/* The following two fields are only used by the strtr function */` |
|      - | 6180 | `	SyBlob *pWorker;         /* Working buffer */` |
|      - | 6181 | `	ProcStringMatch xMatch;  /* Pattern match routine */` |
|      - | 6182 | `	/* The following two fields are only used by the str_replace function */` |
|      - | 6183 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 6184 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 6185 | `};` |
|      - | 6186 | `/*` |
|      - | 6187 | ` * Remove a substring.` |
|      - | 6188 | ` */` |
|      - | 6189 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 6190 | `	for(;;){\` |
|      - | 6191 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 6192 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 6193 | `		++OFFT;\` |
|      - | 6194 | `	}\` |
|      - | 6195 |  |
|      - | 6196 | `/*` |
|      - | 6197 | ` * Shift right and insert algorithm.` |
|      - | 6198 | ` */` |
|      - | 6199 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 6200 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 6201 | `		for(;;){\` |
|      - | 6202 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 6203 | `			if(INLEN < 1 ) { break; }\` |
|      - | 6204 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 6205 | `			--INLEN; \` |
|      - | 6206 | `		}\` |
|      - | 6207 | `		for(;;){\` |
|      - | 6208 | `				if(ELEN < 1) { break; }\` |
|      - | 6209 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 6210 | `				OFFT++;\` |
|      - | 6211 | `				ENTRY++;\` |
|      - | 6212 | `				--ELEN;\` |
|      - | 6213 | `		}\` |
|      - | 6214 |  |
|      - | 6215 | `/*` |
|      - | 6216 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 6217 | ` * replacement string [i.e: zReplace].` |
|      - | 6218 | ` */` |
|     38 | 6219 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 6220 |  |
|     39 | 6221 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 6222 | `	sxu32 n,m;` |
|     39 | 6223 | `	n = SyBlobLength(pWorker);` |
|     39 | 6224 | `	m = nOfft;` |
|      - | 6225 | `	/* Delete the old entry */` |
|    475 | 6226 | `	STRDEL(zInput,n,m,nLen);` |
|     39 | 6227 | `	SyBlobLength(pWorker) -= nLen;` |
|     39 | 6228 | `	if( nReplen > 0 ){` |
|     33 | 6229 | `		sxi32 iRep = nReplen;` |
|      - | 6230 | `		sxi32 rc;` |
|      - | 6231 | `		/*` |
|      - | 6232 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 6233 | `		 * string.` |
|      - | 6234 | `		 */` |
|     33 | 6235 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     33 | 6236 | `		if( rc != SXRET_OK ){` |
|      - | 6237 | `			/* Simply ignore any memory failure problem */` |
|    ! 0 | 6238 | `			return SXRET_OK;` |
|      - | 6239 | `		}` |
|      - | 6240 | `		/* Perform the insertion now */` |
|     33 | 6241 | `		zInput = (char *)SyBlobData(pWorker);` |
|     33 | 6242 | `		n = SyBlobLength(pWorker);` |
|    163 | 6243 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     33 | 6244 | `		SyBlobLength(pWorker) += nReplen;` |
|     16 | 6245 | `	}` |
|     39 | 6246 | `	return SXRET_OK;` |
|     20 | 6247 |  |
|      - | 6248 | `/*` |
|      - | 6249 | ` * String replacement walker callback.` |
|      - | 6250 | ` * The following callback is invoked for each array entry that hold` |
|      - | 6251 | ` * the replace string.` |
|      - | 6252 | ` * Refer to the strtr() implementation for more information.` |
|      - | 6253 | ` */` |
|      8 | 6254 | `static int StringReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6255 |  |
|      9 | 6256 | `	str_replace_data *pRepData = (str_replace_data *)pUserData;` |
|      - | 6257 | `	const char *zTarget,*zReplace;` |
|      - | 6258 | `	SyBlob *pWorker;` |
|      - | 6259 | `	int tLen,nLen;` |
|      - | 6260 | `	sxu32 nOfft;` |
|      - | 6261 | `	sxi32 rc;` |
|      - | 6262 | `	/* Point to the working buffer */` |
|      9 | 6263 | `	pWorker = pRepData->pWorker;` |
|      9 | 6264 | `	if( !ph7_value_is_string(pKey) ){` |
|      - | 6265 | `		/* Target and replace must be a string */` |
|      3 | 6266 | `		return PH7_OK;` |
|      - | 6267 | `	}` |
|      - | 6268 | `	/* Extract the target and the replace */` |
|      7 | 6269 | `	zTarget = ph7_value_to_string(pKey,&tLen);` |
|      7 | 6270 | `	if( tLen < 1 ){` |
|      - | 6271 | `		/* Empty target,return immediately */` |
|    ! 0 | 6272 | `		return PH7_OK;` |
|      - | 6273 | `	}` |
|      - | 6274 | `	/* Perform a pattern search */` |
|      7 | 6275 | `	rc = pRepData->xMatch(SyBlobData(pWorker),SyBlobLength(pWorker),(const void *)zTarget,(sxu32)tLen,&nOfft);` |
|      7 | 6276 | `	if( rc != SXRET_OK ){` |
|      - | 6277 | `		/* Pattern not found */` |
|    ! 0 | 6278 | `		return PH7_OK;` |
|      - | 6279 | `	}` |
|      - | 6280 | `	/* Extract the replace string */` |
|      7 | 6281 | `	zReplace = ph7_value_to_string(pData,&nLen);` |
|      - | 6282 | `	/* Perform the replace process */` |
|      7 | 6283 | `	StringReplace(pWorker,nOfft,tLen,zReplace,nLen);` |
|      - | 6284 | `	/* All done */` |
|      7 | 6285 | `	return PH7_OK;` |
|      5 | 6286 |  |
|      - | 6287 | `/*` |
|      - | 6288 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 6289 | ` * to collect search/replace string.` |
|      - | 6290 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 6291 | ` */` |
|     26 | 6292 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6293 |  |
|     27 | 6294 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 6295 | `	SyString sWorker;` |
|      - | 6296 | `	const char *zIn;` |
|      - | 6297 | `	int nByte;` |
|      - | 6298 | `	/* Extract a string representation of the given argument */` |
|     27 | 6299 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 6300 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 6301 | `	if( nByte > 0 ){` |
|      - | 6302 | `		char *zDup;` |
|      - | 6303 | `		/* Duplicate the chunk */` |
|     25 | 6304 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 6305 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 6306 | `			);` |
|     25 | 6307 | `		if( zDup == 0 ){` |
|      - | 6308 | `			/* Ignore any memory failure problem */` |
|    ! 0 | 6309 | `			ph7_context_throw_error(pRep->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 6310 | `			return PH7_OK;` |
|      - | 6311 | `		}` |
|     25 | 6312 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 6313 | `		/* Save the chunk */` |
|     25 | 6314 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 6315 | `	}` |
|      - | 6316 | `	/* Save for later processing */` |
|     27 | 6317 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 6318 | `	/* All done */` |
|     13 | 6319 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 6320 | `	return PH7_OK;` |
|     14 | 6321 |  |
|      - | 6322 | `/*` |
|      - | 6323 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6324 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6325 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 6326 | ` * Parameters` |
|      - | 6327 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 6328 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 6329 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 6330 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 6331 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 6332 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 6333 | ` * $search` |
|      - | 6334 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 6335 | ` *  to designate multiple needles.` |
|      - | 6336 | ` * $replace` |
|      - | 6337 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 6338 | ` *  to designate multiple replacements.` |
|      - | 6339 | ` * $subject` |
|      - | 6340 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 6341 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 6342 | ` *  of subject, and the return value is an array as well.` |
|      - | 6343 | ` * $count (Not used)` |
|      - | 6344 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 6345 | ` * Return` |
|      - | 6346 | ` * This function returns a string or an array with the replaced values.` |
|      - | 6347 | ` */` |
|  13794 | 6348 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6349 |  |
|      - | 6350 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 6351 | `	ProcStringMatch xMatch;` |
|      - | 6352 | `	const char *zIn,*zFunc;` |
|      - | 6353 | `	str_replace_data sRep;` |
|      - | 6354 | `	SyBlob sWorker;` |
|      - | 6355 | `	SySet sReplace;` |
|      - | 6356 | `	SySet sSearch;` |
|      - | 6357 | `	int rep_str;` |
|      - | 6358 | `	int nByte;` |
|      - | 6359 | `	sxi32 rc;` |
|  13796 | 6360 | `	if( nArg < 3 ){` |
|      - | 6361 | `		/* Missing/Invalid arguments,return null */` |
|      7 | 6362 | `		ph7_result_null(pCtx);` |
|      7 | 6363 | `		return PH7_OK;` |
|      - | 6364 | `	}` |
|      - | 6365 | `	/* Initialize fields */` |
|  13790 | 6366 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  13790 | 6367 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  13790 | 6368 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  13790 | 6369 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  13790 | 6370 | `	sRep.pCtx = pCtx;` |
|  13790 | 6371 | `	sRep.pCollector = &sSearch;` |
|  13790 | 6372 | `	rep_str = 0;` |
|      - | 6373 | `	/* Extract the subject */` |
|  13790 | 6374 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  13790 | 6375 | `	if( nByte < 1 ){` |
|      - | 6376 | `		/* Nothing to replace,return the empty string */` |
|     38 | 6377 | `		ph7_result_string(pCtx,"",0);` |
|     38 | 6378 | `		return PH7_OK;` |
|      - | 6379 | `	}` |
|      - | 6380 | `	/* Copy the subject */` |
|  13754 | 6381 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 6382 | `	/* Search string */` |
|  13754 | 6383 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 6384 | `		/* Collect search string */` |
|      9 | 6385 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 6386 | `	}else{` |
|      - | 6387 | `		/* Single pattern */` |
|  13746 | 6388 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  13746 | 6389 | `		if( nByte < 1 ){` |
|      - | 6390 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 6391 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 6392 | `			return PH7_OK;` |
|      - | 6393 | `		}` |
|  13742 | 6394 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6395 | `		/* Save for later processing */` |
|  13742 | 6396 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 6397 | `	}` |
|      - | 6398 | `	/* Replace string */` |
|  13750 | 6399 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 6400 | `		/* Collect replace string */` |
|      7 | 6401 | `		sRep.pCollector = &sReplace;` |
|      7 | 6402 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 6403 | `	}else{` |
|      - | 6404 | `		/* Single needle */` |
|  13744 | 6405 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  13744 | 6406 | `		rep_str = 1;` |
|  13744 | 6407 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6408 | `		/* Save for later processing */` |
|  13744 | 6409 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 6410 | `	}` |
|      - | 6411 | `	/* Reset loop cursors */` |
|  13750 | 6412 | `	SySetResetCursor(&sSearch);` |
|  13750 | 6413 | `	SySetResetCursor(&sReplace);` |
|  13750 | 6414 | `	pReplace = pSearch = 0; /* cc warning */` |
|  13750 | 6415 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 6416 | `	/* Extract function name */` |
|  13750 | 6417 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 6418 | `	/* Set the default pattern match routine */` |
|  13750 | 6419 | `	xMatch = SyBlobSearch;` |
|  13750 | 6420 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 6421 | `		/* Case insensitive pattern match */` |
|     11 | 6422 | `		xMatch = iPatternMatch;` |
|      5 | 6423 | `	}` |
|      - | 6424 | `	/* Start the replace process */` |
|  27506 | 6425 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 6426 | `		sxu32 nCount,nOfft;` |
|  13758 | 6427 | `		if( pSearch->nByte <  1 ){` |
|      - | 6428 | `			/* Empty string,ignore */` |
|      3 | 6429 | `			continue;` |
|      - | 6430 | `		}` |
|      - | 6431 | `		/* Extract the replace string */` |
|  13756 | 6432 | `		if( rep_str ){` |
|  13746 | 6433 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|   6874 | 6434 | `		}else{` |
|     11 | 6435 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 6436 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 6437 | `				 * An empty string is used for the rest of replacement values` |
|      - | 6438 | `				 */` |
|      3 | 6439 | `				pReplace = 0;` |
|      1 | 6440 | `			}` |
|      - | 6441 | `		}` |
|  13756 | 6442 | `		if( pReplace == 0 ){` |
|      - | 6443 | `			/* Use an empty string instead */` |
|      3 | 6444 | `			pReplace = &sTemp;` |
|      1 | 6445 | `		}` |
|  13756 | 6446 | `		nOfft = nCount = 0;` |
|   6893 | 6447 | `		for(;;){` |
|  13788 | 6448 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 6449 | `				break;` |
|      - | 6450 | `			}` |
|      - | 6451 | `			/* Perform a pattern lookup */` |
|  20663 | 6452 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  13774 | 6453 | `				pSearch->nByte,&nOfft);` |
|  13776 | 6454 | `			if( rc != SXRET_OK ){` |
|      - | 6455 | `				/* Pattern not found */` |
|  13744 | 6456 | `				break;` |
|      - | 6457 | `			}` |
|      - | 6458 | `			/* Perform the replace operation */` |
|     33 | 6459 | `			StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|      - | 6460 | `			/* Increment offset counter */` |
|     33 | 6461 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 6462 | `		}` |
|      2 | 6463 | `	}` |
|      - | 6464 | `	/* All done,clean-up the mess left behind */` |
|  13750 | 6465 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  13750 | 6466 | `	SySetRelease(&sSearch);` |
|  13750 | 6467 | `	SySetRelease(&sReplace);` |
|  13750 | 6468 | `	SyBlobRelease(&sWorker);` |
|  13750 | 6469 | `	return PH7_OK;` |
|   6899 | 6470 |  |
|      - | 6471 | `/*` |
|      - | 6472 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 6473 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 6474 | ` *  Translate characters or replace substrings.` |
|      - | 6475 | ` * Parameters` |
|      - | 6476 | ` *  $str` |
|      - | 6477 | ` *  The string being translated.` |
|      - | 6478 | ` * $from` |
|      - | 6479 | ` *  The string being translated to to.` |
|      - | 6480 | ` * $to` |
|      - | 6481 | ` *  The string replacing from.` |
|      - | 6482 | ` * $replace_pairs` |
|      - | 6483 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 6484 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 6485 | ` * Return` |
|      - | 6486 | ` *  The translated string.` |
|      - | 6487 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 6488 | ` */` |
|     12 | 6489 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6490 |  |
|      - | 6491 | `	const char *zIn;` |
|      - | 6492 | `	int nLen;` |
|     13 | 6493 | `	if( nArg < 1 ){` |
|      - | 6494 | `		/* Nothing to replace,return FALSE */` |
|      7 | 6495 | `		ph7_result_bool(pCtx,0);` |
|      7 | 6496 | `		return PH7_OK;` |
|      - | 6497 | `	}` |
|      7 | 6498 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 6499 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 6500 | `		/* Invalid arguments */` |
|    ! 0 | 6501 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6502 | `		return PH7_OK;` |
|      - | 6503 | `	}` |
|      9 | 6504 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 6505 | `		str_replace_data sRepData;` |
|      - | 6506 | `		SyBlob sWorker;` |
|      - | 6507 | `		/* Initilaize the working buffer */` |
|      5 | 6508 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|      - | 6509 | `		/* Copy raw string */` |
|      5 | 6510 | `		SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nLen);` |
|      - | 6511 | `		/* Init our replace data instance */` |
|      5 | 6512 | `		sRepData.pWorker = &sWorker;` |
|      5 | 6513 | `		sRepData.xMatch = SyBlobSearch;` |
|      - | 6514 | `		/* Iterate throw array entries and perform the replace operation.*/` |
|      5 | 6515 | `		ph7_array_walk(apArg[1],StringReplaceWalker,&sRepData);` |
|      - | 6516 | `		/* All done, return the result string */` |
|      7 | 6517 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|      4 | 6518 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 6519 | `		/* Clean-up */` |
|      5 | 6520 | `		SyBlobRelease(&sWorker);` |
|      3 | 6521 | `	}else{` |
|      - | 6522 | `		int i,flen,tlen,c,iOfft;` |
|      - | 6523 | `		const char *zFrom,*zTo;` |
|      3 | 6524 | `		if( nArg < 3 ){` |
|      - | 6525 | `			/* Nothing to replace */` |
|    ! 0 | 6526 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6527 | `			return PH7_OK;` |
|      - | 6528 | `		}` |
|      - | 6529 | `		/* Extract given arguments */` |
|      3 | 6530 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 6531 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 6532 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 6533 | `			/* Nothing to replace */` |
|    ! 0 | 6534 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6535 | `			return PH7_OK;` |
|      - | 6536 | `		}` |
|      - | 6537 | `		/* Start the replace process */` |
|     13 | 6538 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 6539 | `			c = zIn[i];` |
|     11 | 6540 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 6541 | `				if ( iOfft < tlen ){` |
|      5 | 6542 | `					c = zTo[iOfft];` |
|      2 | 6543 | `				}` |
|      2 | 6544 | `			}` |
|     11 | 6545 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 6546 |  |
|      6 | 6547 | `		}` |
|      - | 6548 | `	}` |
|      7 | 6549 | `	return PH7_OK;` |
|      7 | 6550 |  |
|      - | 6551 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6552 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6553 | `/*` |
|      - | 6554 | ` * Parse an INI string.` |
|      - | 6555 |  |
|      - | 6556 | ` * According to wikipedia` |
|      - | 6557 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 6558 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 6559 | ` *  Format` |
|      - | 6560 | `*    Properties` |
|      - | 6561 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 6562 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 6563 | `*     Example:` |
|      - | 6564 | `*      name=value` |
|      - | 6565 | `*    Sections` |
|      - | 6566 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 6567 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 6568 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 6569 | `*     or the end of the file. Sections may not be nested.` |
|      - | 6570 | `*     Example:` |
|      - | 6571 | `*      [section]` |
|      - | 6572 | `*   Comments` |
|      - | 6573 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 6574 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 6575 | `*/` |
|     12 | 6576 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 6577 |  |
|      - | 6578 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 6579 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 6580 | `	SyHashEntry *pEntry;` |
|      - | 6581 | `	SyString sEntry;` |
|      - | 6582 | `	SyHash sHash;` |
|      - | 6583 | `	int c;` |
|      - | 6584 | `	/* Create an empty array and worker variables */` |
|     13 | 6585 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 6586 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 6587 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 6588 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 6589 | `		/* Out of memory */` |
|    ! 0 | 6590 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      - | 6591 | `		/* Return FALSE */` |
|    ! 0 | 6592 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6593 | `		return PH7_OK;` |
|      - | 6594 | `	}` |
|     13 | 6595 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 6596 | `	pCur = pArray;` |
|      - | 6597 | `	/* Start the parse process */` |
|     21 | 6598 | `	for(;;){` |
|      - | 6599 | `		/* Ignore leading white spaces */` |
|     69 | 6600 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 6601 | `			zIn++;` |
|      1 | 6602 | `		}` |
|     43 | 6603 | `		if( zIn >= zEnd ){` |
|      - | 6604 | `			/* No more input to process */` |
|     13 | 6605 | `			break;` |
|      - | 6606 | `		}` |
|     31 | 6607 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6608 | `			/* Comment til the end of line */` |
|    ! 0 | 6609 | `			zIn++;` |
|    ! 0 | 6610 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 6611 | `				zIn++;` |
|    ! 0 | 6612 | `			}` |
|    ! 0 | 6613 | `			continue;` |
|      - | 6614 | `		}` |
|      - | 6615 | `		/* Reset the string cursor of the working variable */` |
|     31 | 6616 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 6617 | `		if( zIn[0] == '[' ){` |
|      - | 6618 | `			/* Section: Extract the section name */` |
|      9 | 6619 | `			zIn++;` |
|      9 | 6620 | `			zCur = zIn;` |
|     73 | 6621 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 6622 | `				zIn++;` |
|      1 | 6623 | `			}` |
|      9 | 6624 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 6625 | `				/* Save the section name */` |
|      5 | 6626 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 6627 | `				SyStringFullTrim(&sEntry);` |
|      5 | 6628 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 6629 | `				if( sEntry.nByte > 0 ){` |
|      - | 6630 | `					/* Associate an array with the section */` |
|      5 | 6631 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 6632 | `					if( pSection ){` |
|      5 | 6633 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 6634 | `						pCur = pSection;` |
|      2 | 6635 | `					}` |
|      2 | 6636 | `				}` |
|      2 | 6637 | `			}` |
|      9 | 6638 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 6639 | `		}else{` |
|      - | 6640 | `			ph7_value *pOldCur;` |
|      - | 6641 | `			int is_array;` |
|      - | 6642 | `			int iLen;` |
|      - | 6643 | `			/* Properties */` |
|     23 | 6644 | `			is_array = 0;` |
|     23 | 6645 | `			zCur = zIn;` |
|     23 | 6646 | `			iLen = 0; /* cc warning */` |
|     23 | 6647 | `			pOldCur = pCur;` |
|    155 | 6648 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 6649 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 6650 | `					/* Array */` |
|    ! 0 | 6651 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 6652 | `					is_array = 1;` |
|    ! 0 | 6653 | `					if( iLen > 0 ){` |
|    ! 0 | 6654 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 6655 | `						/* Query the hashtable */` |
|    ! 0 | 6656 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 6657 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 6658 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 6659 | `						if( pEntry ){` |
|    ! 0 | 6660 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 6661 | `						}else{` |
|      - | 6662 | `							/* Create an empty array */` |
|    ! 0 | 6663 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 6664 | `							if( pvArr ){` |
|      - | 6665 | `								/* Save the entry */` |
|    ! 0 | 6666 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 6667 | `								/* Insert the entry */` |
|    ! 0 | 6668 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6669 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 6670 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 6671 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6672 | `							}` |
|      - | 6673 | `						}` |
|    ! 0 | 6674 | `						if( pvArr ){` |
|    ! 0 | 6675 | `							pCur = pvArr;` |
|    ! 0 | 6676 | `						}` |
|    ! 0 | 6677 | `					}` |
|    ! 0 | 6678 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 6679 | `						zIn++;` |
|    ! 0 | 6680 | `					}` |
|    ! 0 | 6681 | `				}` |
|    133 | 6682 | `				zIn++;` |
|      1 | 6683 | `			}` |
|     23 | 6684 | `			if( !is_array ){` |
|     23 | 6685 | `				iLen = (int)(zIn-zCur);` |
|     11 | 6686 | `			}` |
|      - | 6687 | `			/* Trim the key */` |
|     23 | 6688 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 6689 | `			SyStringFullTrim(&sEntry);` |
|     23 | 6690 | `			if( sEntry.nByte > 0 ){` |
|     23 | 6691 | `				if( !is_array ){` |
|      - | 6692 | `					/* Save the key name */` |
|     23 | 6693 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 6694 | `				}` |
|      - | 6695 | `				/* extract key value */` |
|     23 | 6696 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 6697 | `				zIn++; /* '=' */` |
|     39 | 6698 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 6699 | `					zIn++;` |
|      1 | 6700 | `				}` |
|     23 | 6701 | `				if( zIn < zEnd ){` |
|     21 | 6702 | `					zCur = zIn;` |
|     21 | 6703 | `					c = zIn[0];` |
|     21 | 6704 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6705 | `						zIn++;` |
|      - | 6706 | `						/* Delimit the value */` |
|    ! 0 | 6707 | `						while( zIn < zEnd ){` |
|    ! 0 | 6708 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 6709 | `								break;` |
|      - | 6710 | `							}` |
|    ! 0 | 6711 | `							zIn++;` |
|    ! 0 | 6712 | `						}` |
|    ! 0 | 6713 | `						if( zIn < zEnd ){` |
|    ! 0 | 6714 | `							zIn++;` |
|    ! 0 | 6715 | `						}` |
|    ! 0 | 6716 | `					}else{` |
|    125 | 6717 | `						while( zIn < zEnd ){` |
|    123 | 6718 | `							if( zIn[0] == '\n' ){` |
|     19 | 6719 | `								if( zIn[-1] != '\\' ){` |
|     19 | 6720 | `									break;` |
|    ! 0 | 6721 | `								}` |
|    105 | 6722 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6723 | `								/* Inline comments */` |
|    ! 0 | 6724 | `								break;` |
|      - | 6725 | `							}` |
|    105 | 6726 | `							zIn++;` |
|      1 | 6727 | `						}` |
|      - | 6728 | `					}` |
|      - | 6729 | `					/* Trim the value */` |
|     21 | 6730 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 6731 | `					SyStringFullTrim(&sEntry);` |
|     21 | 6732 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6733 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 6734 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 6735 | `					}` |
|     21 | 6736 | `					if( sEntry.nByte > 0 ){` |
|     21 | 6737 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 6738 | `					}` |
|      - | 6739 | `					/* Insert the key and it's value */` |
|     21 | 6740 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 6741 | `				}` |
|     12 | 6742 | `			}else{` |
|    ! 0 | 6743 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 6744 | `					zIn++;` |
|    ! 0 | 6745 | `				}` |
|      - | 6746 | `			}` |
|     23 | 6747 | `			pCur = pOldCur;` |
|      - | 6748 | `		}` |
|      1 | 6749 | `	}` |
|     13 | 6750 | `	SyHashRelease(&sHash);` |
|      - | 6751 | `	/* Return the parse of the INI string */` |
|     13 | 6752 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 6753 | `	return SXRET_OK;` |
|      7 | 6754 |  |
|      - | 6755 | `/*` |
|      - | 6756 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 6757 | ` *  Parse a configuration string.` |
|      - | 6758 | ` * Parameters` |
|      - | 6759 | ` *  $ini` |
|      - | 6760 | ` *   The contents of the ini file being parsed.` |
|      - | 6761 | ` *  $process_sections` |
|      - | 6762 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 6763 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 6764 | ` *  $scanner_mode (Not used)` |
|      - | 6765 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 6766 | ` *   then option values will not be parsed.` |
|      - | 6767 | ` * Return` |
|      - | 6768 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 6769 | ` */` |
|     10 | 6770 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6771 |  |
|      - | 6772 | `	const char *zIni;` |
|      - | 6773 | `	int nByte;` |
|     11 | 6774 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6775 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 6776 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6777 | `		return PH7_OK;` |
|      - | 6778 | `	}` |
|      - | 6779 | `	/* Extract the raw INI buffer */` |
|     11 | 6780 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 6781 | `	/* Process the INI buffer*/` |
|     11 | 6782 | `	PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|     11 | 6783 | `	return PH7_OK;` |
|      6 | 6784 |  |
|      - | 6785 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6786 |  |
|      - | 6787 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6788 |  |
|      - | 6789 | `/*` |
|      - | 6790 | ` * Ctype Functions.` |
|      - | 6791 | ` * Status:` |
|      - | 6792 | ` *    Stable.` |
|      - | 6793 | ` */` |
|      - | 6794 | `/*` |
|      - | 6795 | ` * bool ctype_alnum(string $text)` |
|      - | 6796 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 6797 | ` * Parameters` |
|      - | 6798 | ` *  $text` |
|      - | 6799 | ` *   The tested string.` |
|      - | 6800 | ` * Return` |
|      - | 6801 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 6802 | ` */` |
|     16 | 6803 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6804 |  |
|      - | 6805 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6806 | `	int nLen;` |
|     17 | 6807 | `	if( nArg < 1 ){` |
|      - | 6808 | `		/* Missing arguments,return FALSE */` |
|      3 | 6809 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6810 | `		return PH7_OK;` |
|      - | 6811 | `	}` |
|      - | 6812 | `	/* Extract the target string */` |
|     15 | 6813 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 6814 | `	zEnd = &zIn[nLen];` |
|     15 | 6815 | `	if( nLen < 1 ){` |
|      - | 6816 | `		/* Empty string,return FALSE */` |
|      3 | 6817 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6818 | `		return PH7_OK;` |
|      - | 6819 | `	}` |
|      - | 6820 | `	/* Perform the requested operation */` |
|     32 | 6821 | `	for(;;){` |
|     65 | 6822 | `		if( zIn >= zEnd ){` |
|      - | 6823 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6824 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6825 | `			return PH7_OK;` |
|      - | 6826 | `		}` |
|     57 | 6827 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 6828 | `			break;` |
|      - | 6829 | `		}` |
|      - | 6830 | `		/* Point to the next character */` |
|     53 | 6831 | `		zIn++;` |
|      1 | 6832 | `	}` |
|      - | 6833 | `	/* The test failed,return FALSE */` |
|      5 | 6834 | `	ph7_result_bool(pCtx,0);` |
|      5 | 6835 | `	return PH7_OK;` |
|      9 | 6836 |  |
|      - | 6837 | `/*` |
|      - | 6838 | ` * bool ctype_alpha(string $text)` |
|      - | 6839 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 6840 | ` * Parameters` |
|      - | 6841 | ` *  $text` |
|      - | 6842 | ` *   The tested string.` |
|      - | 6843 | ` * Return` |
|      - | 6844 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 6845 | ` */` |
|     18 | 6846 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6847 |  |
|      - | 6848 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6849 | `	int nLen;` |
|     19 | 6850 | `	if( nArg < 1 ){` |
|      - | 6851 | `		/* Missing arguments,return FALSE */` |
|      3 | 6852 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6853 | `		return PH7_OK;` |
|      - | 6854 | `	}` |
|      - | 6855 | `	/* Extract the target string */` |
|     17 | 6856 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6857 | `	zEnd = &zIn[nLen];` |
|     17 | 6858 | `	if( nLen < 1 ){` |
|      - | 6859 | `		/* Empty string,return FALSE */` |
|      3 | 6860 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6861 | `		return PH7_OK;` |
|      - | 6862 | `	}` |
|      - | 6863 | `	/* Perform the requested operation */` |
|     42 | 6864 | `	for(;;){` |
|     85 | 6865 | `		if( zIn >= zEnd ){` |
|      - | 6866 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6867 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6868 | `			return PH7_OK;` |
|      - | 6869 | `		}` |
|     77 | 6870 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 6871 | `			break;` |
|      - | 6872 | `		}` |
|      - | 6873 | `		/* Point to the next character */` |
|     71 | 6874 | `		zIn++;` |
|      1 | 6875 | `	}` |
|      - | 6876 | `	/* The test failed,return FALSE */` |
|      7 | 6877 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6878 | `	return PH7_OK;` |
|     10 | 6879 |  |
|      - | 6880 | `/*` |
|      - | 6881 | ` * bool ctype_cntrl(string $text)` |
|      - | 6882 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 6883 | ` * Parameters` |
|      - | 6884 | ` *  $text` |
|      - | 6885 | ` *   The tested string.` |
|      - | 6886 | ` * Return` |
|      - | 6887 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 6888 | ` */` |
|     18 | 6889 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6890 |  |
|      - | 6891 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6892 | `	int nLen;` |
|     19 | 6893 | `	if( nArg < 1 ){` |
|      - | 6894 | `		/* Missing arguments,return FALSE */` |
|      3 | 6895 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6896 | `		return PH7_OK;` |
|      - | 6897 | `	}` |
|      - | 6898 | `	/* Extract the target string */` |
|     17 | 6899 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6900 | `	zEnd = &zIn[nLen];` |
|     17 | 6901 | `	if( nLen < 1 ){` |
|      - | 6902 | `		/* Empty string,return FALSE */` |
|      3 | 6903 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6904 | `		return PH7_OK;` |
|      - | 6905 | `	}` |
|      - | 6906 | `	/* Perform the requested operation */` |
|     14 | 6907 | `	for(;;){` |
|     29 | 6908 | `		if( zIn >= zEnd ){` |
|      - | 6909 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6910 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6911 | `			return PH7_OK;` |
|      - | 6912 | `		}` |
|     21 | 6913 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6914 | `			/* UTF-8 stream  */` |
|    ! 0 | 6915 | `			break;` |
|      - | 6916 | `		}` |
|     21 | 6917 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 6918 | `			break;` |
|      - | 6919 | `		}` |
|      - | 6920 | `		/* Point to the next character */` |
|     15 | 6921 | `		zIn++;` |
|      1 | 6922 | `	}` |
|      - | 6923 | `	/* The test failed,return FALSE */` |
|      7 | 6924 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6925 | `	return PH7_OK;` |
|     10 | 6926 |  |
|      - | 6927 | `/*` |
|      - | 6928 | ` * bool ctype_digit(string $text)` |
|      - | 6929 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 6930 | ` * Parameters` |
|      - | 6931 | ` *  $text` |
|      - | 6932 | ` *   The tested string.` |
|      - | 6933 | ` * Return` |
|      - | 6934 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 6935 | ` */` |
|   1722 | 6936 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6937 |  |
|      - | 6938 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6939 | `	int nLen;` |
|   1724 | 6940 | `	if( nArg < 1 ){` |
|      - | 6941 | `		/* Missing arguments,return FALSE */` |
|      3 | 6942 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6943 | `		return PH7_OK;` |
|      - | 6944 | `	}` |
|      - | 6945 | `	/* Extract the target string */` |
|   1722 | 6946 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1722 | 6947 | `	zEnd = &zIn[nLen];` |
|   1722 | 6948 | `	if( nLen < 1 ){` |
|      - | 6949 | `		/* Empty string,return FALSE */` |
|      3 | 6950 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6951 | `		return PH7_OK;` |
|      - | 6952 | `	}` |
|      - | 6953 | `	/* Perform the requested operation */` |
|   1591 | 6954 | `	for(;;){` |
|   3184 | 6955 | `		if( zIn >= zEnd ){` |
|      - | 6956 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1434 | 6957 | `			ph7_result_bool(pCtx,1);` |
|   1434 | 6958 | `			return PH7_OK;` |
|      - | 6959 | `		}` |
|   1752 | 6960 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6961 | `			/* UTF-8 stream  */` |
|    ! 0 | 6962 | `			break;` |
|      - | 6963 | `		}` |
|   1752 | 6964 | `		if( !SyisDigit(zIn[0]) ){` |
|    288 | 6965 | `			break;` |
|      - | 6966 | `		}` |
|      - | 6967 | `		/* Point to the next character */` |
|   1466 | 6968 | `		zIn++;` |
|      2 | 6969 | `	}` |
|      - | 6970 | `	/* The test failed,return FALSE */` |
|    288 | 6971 | `	ph7_result_bool(pCtx,0);` |
|    288 | 6972 | `	return PH7_OK;` |
|    863 | 6973 |  |
|      - | 6974 | `/*` |
|      - | 6975 | ` * bool ctype_xdigit(string $text)` |
|      - | 6976 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 6977 | ` * Parameters` |
|      - | 6978 | ` *  $text` |
|      - | 6979 | ` *   The tested string.` |
|      - | 6980 | ` * Return` |
|      - | 6981 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 6982 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 6983 | ` */` |
|     20 | 6984 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6985 |  |
|      - | 6986 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6987 | `	int nLen;` |
|     21 | 6988 | `	if( nArg < 1 ){` |
|      - | 6989 | `		/* Missing arguments,return FALSE */` |
|      3 | 6990 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6991 | `		return PH7_OK;` |
|      - | 6992 | `	}` |
|      - | 6993 | `	/* Extract the target string */` |
|     19 | 6994 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 6995 | `	zEnd = &zIn[nLen];` |
|     19 | 6996 | `	if( nLen < 1 ){` |
|      - | 6997 | `		/* Empty string,return FALSE */` |
|      3 | 6998 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6999 | `		return PH7_OK;` |
|      - | 7000 | `	}` |
|      - | 7001 | `	/* Perform the requested operation */` |
|     46 | 7002 | `	for(;;){` |
|     93 | 7003 | `		if( zIn >= zEnd ){` |
|      - | 7004 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 7005 | `			ph7_result_bool(pCtx,1);` |
|     11 | 7006 | `			return PH7_OK;` |
|      - | 7007 | `		}` |
|     83 | 7008 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7009 | `			/* UTF-8 stream  */` |
|    ! 0 | 7010 | `			break;` |
|      - | 7011 | `		}` |
|     83 | 7012 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 7013 | `			break;` |
|      - | 7014 | `		}` |
|      - | 7015 | `		/* Point to the next character */` |
|     77 | 7016 | `		zIn++;` |
|      1 | 7017 | `	}` |
|      - | 7018 | `	/* The test failed,return FALSE */` |
|      7 | 7019 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7020 | `	return PH7_OK;` |
|     11 | 7021 |  |
|      - | 7022 | `/*` |
|      - | 7023 | ` * bool ctype_graph(string $text)` |
|      - | 7024 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 7025 | ` * Parameters` |
|      - | 7026 | ` *  $text` |
|      - | 7027 | ` *   The tested string.` |
|      - | 7028 | ` * Return` |
|      - | 7029 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 7030 | ` * (no white space), FALSE otherwise.` |
|      - | 7031 | ` */` |
|     18 | 7032 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7033 |  |
|      - | 7034 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7035 | `	int nLen;` |
|     19 | 7036 | `	if( nArg < 1 ){` |
|      - | 7037 | `		/* Missing arguments,return FALSE */` |
|      3 | 7038 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7039 | `		return PH7_OK;` |
|      - | 7040 | `	}` |
|      - | 7041 | `	/* Extract the target string */` |
|     17 | 7042 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7043 | `	zEnd = &zIn[nLen];` |
|     17 | 7044 | `	if( nLen < 1 ){` |
|      - | 7045 | `		/* Empty string,return FALSE */` |
|      3 | 7046 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7047 | `		return PH7_OK;` |
|      - | 7048 | `	}` |
|      - | 7049 | `	/* Perform the requested operation */` |
|     57 | 7050 | `	for(;;){` |
|    115 | 7051 | `		if( zIn >= zEnd ){` |
|      - | 7052 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7053 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7054 | `			return PH7_OK;` |
|      - | 7055 | `		}` |
|    107 | 7056 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7057 | `			/* UTF-8 stream  */` |
|    ! 0 | 7058 | `			break;` |
|      - | 7059 | `		}` |
|    107 | 7060 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 7061 | `			break;` |
|      - | 7062 | `		}` |
|      - | 7063 | `		/* Point to the next character */` |
|    101 | 7064 | `		zIn++;` |
|      1 | 7065 | `	}` |
|      - | 7066 | `	/* The test failed,return FALSE */` |
|      7 | 7067 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7068 | `	return PH7_OK;` |
|     10 | 7069 |  |
|      - | 7070 | `/*` |
|      - | 7071 | ` * bool ctype_print(string $text)` |
|      - | 7072 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 7073 | ` * Parameters` |
|      - | 7074 | ` *  $text` |
|      - | 7075 | ` *   The tested string.` |
|      - | 7076 | ` * Return` |
|      - | 7077 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 7078 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 7079 | ` *  or control function at all.` |
|      - | 7080 | ` */` |
|     18 | 7081 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7082 |  |
|      - | 7083 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7084 | `	int nLen;` |
|     19 | 7085 | `	if( nArg < 1 ){` |
|      - | 7086 | `		/* Missing arguments,return FALSE */` |
|      3 | 7087 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7088 | `		return PH7_OK;` |
|      - | 7089 | `	}` |
|      - | 7090 | `	/* Extract the target string */` |
|     17 | 7091 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7092 | `	zEnd = &zIn[nLen];` |
|     17 | 7093 | `	if( nLen < 1 ){` |
|      - | 7094 | `		/* Empty string,return FALSE */` |
|      3 | 7095 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7096 | `		return PH7_OK;` |
|      - | 7097 | `	}` |
|      - | 7098 | `	/* Perform the requested operation */` |
|     63 | 7099 | `	for(;;){` |
|    127 | 7100 | `		if( zIn >= zEnd ){` |
|      - | 7101 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7102 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7103 | `			return PH7_OK;` |
|      - | 7104 | `		}` |
|    119 | 7105 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7106 | `			/* UTF-8 stream  */` |
|    ! 0 | 7107 | `			break;` |
|      - | 7108 | `		}` |
|    119 | 7109 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 7110 | `			break;` |
|      - | 7111 | `		}` |
|      - | 7112 | `		/* Point to the next character */` |
|    113 | 7113 | `		zIn++;` |
|      1 | 7114 | `	}` |
|      - | 7115 | `	/* The test failed,return FALSE */` |
|      7 | 7116 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7117 | `	return PH7_OK;` |
|     10 | 7118 |  |
|      - | 7119 | `/*` |
|      - | 7120 | ` * bool ctype_punct(string $text)` |
|      - | 7121 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 7122 | ` * Parameters` |
|      - | 7123 | ` *  $text` |
|      - | 7124 | ` *   The tested string.` |
|      - | 7125 | ` * Return` |
|      - | 7126 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 7127 | ` *  digit or blank, FALSE otherwise.` |
|      - | 7128 | ` */` |
|     20 | 7129 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7130 |  |
|      - | 7131 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7132 | `	int nLen;` |
|     21 | 7133 | `	if( nArg < 1 ){` |
|      - | 7134 | `		/* Missing arguments,return FALSE */` |
|      3 | 7135 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7136 | `		return PH7_OK;` |
|      - | 7137 | `	}` |
|      - | 7138 | `	/* Extract the target string */` |
|     19 | 7139 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 7140 | `	zEnd = &zIn[nLen];` |
|     19 | 7141 | `	if( nLen < 1 ){` |
|      - | 7142 | `		/* Empty string,return FALSE */` |
|      3 | 7143 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7144 | `		return PH7_OK;` |
|      - | 7145 | `	}` |
|      - | 7146 | `	/* Perform the requested operation */` |
|     38 | 7147 | `	for(;;){` |
|     77 | 7148 | `		if( zIn >= zEnd ){` |
|      - | 7149 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7150 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7151 | `			return PH7_OK;` |
|      - | 7152 | `		}` |
|     69 | 7153 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7154 | `			/* UTF-8 stream  */` |
|    ! 0 | 7155 | `			break;` |
|      - | 7156 | `		}` |
|     69 | 7157 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 7158 | `			break;` |
|      - | 7159 | `		}` |
|      - | 7160 | `		/* Point to the next character */` |
|     61 | 7161 | `		zIn++;` |
|      1 | 7162 | `	}` |
|      - | 7163 | `	/* The test failed,return FALSE */` |
|      9 | 7164 | `	ph7_result_bool(pCtx,0);` |
|      9 | 7165 | `	return PH7_OK;` |
|     11 | 7166 |  |
|      - | 7167 | `/*` |
|      - | 7168 | ` * bool ctype_space(string $text)` |
|      - | 7169 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 7170 | ` * Parameters` |
|      - | 7171 | ` *  $text` |
|      - | 7172 | ` *   The tested string.` |
|      - | 7173 | ` * Return` |
|      - | 7174 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 7175 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 7176 | ` *  and form feed characters.` |
|      - | 7177 | ` */` |
|  54980 | 7178 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 7179 |  |
|      - | 7180 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7181 | `	int nLen;` |
|  54982 | 7182 | `	if( nArg < 1 ){` |
|      - | 7183 | `		/* Missing arguments,return FALSE */` |
|      3 | 7184 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7185 | `		return PH7_OK;` |
|      - | 7186 | `	}` |
|      - | 7187 | `	/* Extract the target string */` |
|  54980 | 7188 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  54980 | 7189 | `	zEnd = &zIn[nLen];` |
|  54980 | 7190 | `	if( nLen < 1 ){` |
|      - | 7191 | `		/* Empty string,return FALSE */` |
|      3 | 7192 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7193 | `		return PH7_OK;` |
|      - | 7194 | `	}` |
|      - | 7195 | `	/* Perform the requested operation */` |
|  28003 | 7196 | `	for(;;){` |
|  55964 | 7197 | `		if( zIn >= zEnd ){` |
|      - | 7198 | `			/* If we reach the end of the string,then the test succeeded. */` |
|    964 | 7199 | `			ph7_result_bool(pCtx,1);` |
|    964 | 7200 | `			return PH7_OK;` |
|      - | 7201 | `		}` |
|  55002 | 7202 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7203 | `			/* UTF-8 stream  */` |
|    ! 0 | 7204 | `			break;` |
|      - | 7205 | `		}` |
|  55002 | 7206 | `		if( !SyisSpace(zIn[0]) ){` |
|  54016 | 7207 | `			break;` |
|      - | 7208 | `		}` |
|      - | 7209 | `		/* Point to the next character */` |
|    988 | 7210 | `		zIn++;` |
|      2 | 7211 | `	}` |
|      - | 7212 | `	/* The test failed,return FALSE */` |
|  54016 | 7213 | `	ph7_result_bool(pCtx,0);` |
|  54016 | 7214 | `	return PH7_OK;` |
|  27514 | 7215 |  |
|      - | 7216 | `/*` |
|      - | 7217 | ` * bool ctype_lower(string $text)` |
|      - | 7218 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 7219 | ` * Parameters` |
|      - | 7220 | ` *  $text` |
|      - | 7221 | ` *   The tested string.` |
|      - | 7222 | ` * Return` |
|      - | 7223 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 7224 | ` */` |
|     18 | 7225 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7226 |  |
|      - | 7227 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7228 | `	int nLen;` |
|     19 | 7229 | `	if( nArg < 1 ){` |
|      - | 7230 | `		/* Missing arguments,return FALSE */` |
|      3 | 7231 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7232 | `		return PH7_OK;` |
|      - | 7233 | `	}` |
|      - | 7234 | `	/* Extract the target string */` |
|     17 | 7235 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7236 | `	zEnd = &zIn[nLen];` |
|     17 | 7237 | `	if( nLen < 1 ){` |
|      - | 7238 | `		/* Empty string,return FALSE */` |
|      3 | 7239 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7240 | `		return PH7_OK;` |
|      - | 7241 | `	}` |
|      - | 7242 | `	/* Perform the requested operation */` |
|     27 | 7243 | `	for(;;){` |
|     55 | 7244 | `		if( zIn >= zEnd ){` |
|      - | 7245 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7246 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7247 | `			return PH7_OK;` |
|      - | 7248 | `		}` |
|     51 | 7249 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 7250 | `			break;` |
|      - | 7251 | `		}` |
|      - | 7252 | `		/* Point to the next character */` |
|     41 | 7253 | `		zIn++;` |
|      1 | 7254 | `	}` |
|      - | 7255 | `	/* The test failed,return FALSE */` |
|     11 | 7256 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7257 | `	return PH7_OK;` |
|     10 | 7258 |  |
|      - | 7259 | `/*` |
|      - | 7260 | ` * bool ctype_upper(string $text)` |
|      - | 7261 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 7262 | ` * Parameters` |
|      - | 7263 | ` *  $text` |
|      - | 7264 | ` *   The tested string.` |
|      - | 7265 | ` * Return` |
|      - | 7266 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 7267 | ` */` |
|     18 | 7268 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7269 |  |
|      - | 7270 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7271 | `	int nLen;` |
|     19 | 7272 | `	if( nArg < 1 ){` |
|      - | 7273 | `		/* Missing arguments,return FALSE */` |
|      3 | 7274 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7275 | `		return PH7_OK;` |
|      - | 7276 | `	}` |
|      - | 7277 | `	/* Extract the target string */` |
|     17 | 7278 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7279 | `	zEnd = &zIn[nLen];` |
|     17 | 7280 | `	if( nLen < 1 ){` |
|      - | 7281 | `		/* Empty string,return FALSE */` |
|      3 | 7282 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7283 | `		return PH7_OK;` |
|      - | 7284 | `	}` |
|      - | 7285 | `	/* Perform the requested operation */` |
|     28 | 7286 | `	for(;;){` |
|     57 | 7287 | `		if( zIn >= zEnd ){` |
|      - | 7288 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7289 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7290 | `			return PH7_OK;` |
|      - | 7291 | `		}` |
|     53 | 7292 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 7293 | `			break;` |
|      - | 7294 | `		}` |
|      - | 7295 | `		/* Point to the next character */` |
|     43 | 7296 | `		zIn++;` |
|      1 | 7297 | `	}` |
|      - | 7298 | `	/* The test failed,return FALSE */` |
|     11 | 7299 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7300 | `	return PH7_OK;` |
|     10 | 7301 |  |
|      - | 7302 | `/*` |
|      - | 7303 | ` * Date/Time functions` |
|      - | 7304 | ` * Status:` |
|      - | 7305 | ` *    Devel.` |
|      - | 7306 | ` */` |
|      - | 7307 | `#include <time.h>` |
|      - | 7308 | `#ifdef __WINNT__` |
|      - | 7309 | `/* GetSystemTime() */` |
|      - | 7310 | `#include <Windows.h>` |
|      - | 7311 | `#ifdef _WIN32_WCE` |
|      - | 7312 | `/*` |
|      - | 7313 | `** WindowsCE does not have a localtime() function.  So create a` |
|      - | 7314 | `** substitute.` |
|      - | 7315 | `** Taken from the SQLite3 source tree.` |
|      - | 7316 | `** Status: Public domain` |
|      - | 7317 | `*/` |
|      - | 7318 | `struct tm *__cdecl localtime(const time_t *t)` |
|      - | 7319 |  |
|      - | 7320 | `  static struct tm y;` |
|      - | 7321 | `  FILETIME uTm, lTm;` |
|      - | 7322 | `  SYSTEMTIME pTm;` |
|      - | 7323 | `  ph7_int64 t64;` |
|      - | 7324 | `  t64 = *t;` |
|      - | 7325 | `  t64 = (t64 + 11644473600)*10000000;` |
|      - | 7326 | `  uTm.dwLowDateTime = (DWORD)(t64 & 0xFFFFFFFF);` |
|      - | 7327 | `  uTm.dwHighDateTime= (DWORD)(t64 >> 32);` |
|      - | 7328 | `  FileTimeToLocalFileTime(&uTm,&lTm);` |
|      - | 7329 | `  FileTimeToSystemTime(&lTm,&pTm);` |
|      - | 7330 | `  y.tm_year = pTm.wYear - 1900;` |
|      - | 7331 | `  y.tm_mon = pTm.wMonth - 1;` |
|      - | 7332 | `  y.tm_wday = pTm.wDayOfWeek;` |
|      - | 7333 | `  y.tm_mday = pTm.wDay;` |
|      - | 7334 | `  y.tm_hour = pTm.wHour;` |
|      - | 7335 | `  y.tm_min = pTm.wMinute;` |
|      - | 7336 | `  y.tm_sec = pTm.wSecond;` |
|      - | 7337 | `  return &y;` |
|      - | 7338 |  |
|      - | 7339 | `#endif /*_WIN32_WCE */` |
|      - | 7340 | `#elif defined(__UNIXES__)` |
|      - | 7341 | `#include <sys/time.h>` |
|      - | 7342 | `#endif /* __WINNT__*/` |
|      - | 7343 | ` /*` |
|      - | 7344 | `  * int64 time(void)` |
|      - | 7345 | `  *  Current Unix timestamp` |
|      - | 7346 | `  * Parameters` |
|      - | 7347 | `  *  None.` |
|      - | 7348 | `  * Return` |
|      - | 7349 | `  *  Returns the current time measured in the number of seconds` |
|      - | 7350 | `  *  since the Unix Epoch (January 1 1970 00:00:00 GMT).` |
|      - | 7351 | `  */` |
|      8 | 7352 | `static int PH7_builtin_time(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7353 |  |
|      - | 7354 | `	time_t tt;` |
|      4 | 7355 | `	SXUNUSED(nArg); /* cc warning */` |
|      4 | 7356 | `	SXUNUSED(apArg);` |
|      - | 7357 | `	/* Extract the current time */` |
|      9 | 7358 | `	time(&tt);` |
|      - | 7359 | `	/* Return as 64-bit integer */` |
|      9 | 7360 | `	ph7_result_int64(pCtx,(ph7_int64)tt);` |
|      9 | 7361 | `	return  PH7_OK;` |
|      1 | 7362 |  |
|      - | 7363 | `/*` |
|      - | 7364 | `  * string/float microtime([ bool $get_as_float = false ])` |
|      - | 7365 | `  *  microtime() returns the current Unix timestamp with microseconds.` |
|      - | 7366 | `  * Parameters` |
|      - | 7367 | `  *  $get_as_float` |
|      - | 7368 | `  *   If used and set to TRUE, microtime() will return a float instead of a string` |
|      - | 7369 | `  *   as described in the return values section below.` |
|      - | 7370 | `  * Return` |
|      - | 7371 | `  *  By default, microtime() returns a string in the form "msec sec", where sec` |
|      - | 7372 | `  *  is the current time measured in the number of seconds since the Unix` |
|      - | 7373 | `  *  epoch (0:00:00 January 1, 1970 GMT), and msec is the number of microseconds` |
|      - | 7374 | `  *  that have elapsed since sec expressed in seconds.` |
|      - | 7375 | `  *  If get_as_float is set to TRUE, then microtime() returns a float, which represents` |
|      - | 7376 | `  *  the current time in seconds since the Unix epoch accurate to the nearest microsecond.` |
|      - | 7377 | `  */` |
|     20 | 7378 | `static int PH7_builtin_microtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7379 |  |
|     21 | 7380 | `	int bFloat = 0;` |
|      - | 7381 | `	sytime sTime;` |
|      - | 7382 | `#if defined(__UNIXES__)` |
|      - | 7383 | `	struct timeval tv;` |
|     20 | 7384 | `	gettimeofday(&tv,0);` |
|     20 | 7385 | `	sTime.tm_sec  = (long)tv.tv_sec;` |
|     20 | 7386 | `	sTime.tm_usec = (long)tv.tv_usec;` |
|      - | 7387 | `#else` |
|      - | 7388 | `	time_t tt;` |
|      1 | 7389 | `	time(&tt);` |
|      1 | 7390 | `	sTime.tm_sec  = (long)tt;` |
|      1 | 7391 | `	sTime.tm_usec = (long)(tt%SX_USEC_PER_SEC);` |
|      - | 7392 | `#endif /* __UNIXES__ */` |
|     21 | 7393 | `	if( nArg > 0 ){` |
|     17 | 7394 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|      8 | 7395 | `	}` |
|     21 | 7396 | `	if( bFloat ){` |
|      - | 7397 | `		/* Return as float */` |
|     17 | 7398 | `		ph7_result_double(pCtx,(double)sTime.tm_sec);` |
|      9 | 7399 | `	}else{` |
|      - | 7400 | `		/* Return as string */` |
|      5 | 7401 | `		ph7_result_string_format(pCtx,"%ld %ld",sTime.tm_usec,sTime.tm_sec);` |
|      - | 7402 | `	}` |
|     21 | 7403 | `	return PH7_OK;` |
|      1 | 7404 |  |
|      - | 7405 | `/*` |
|      - | 7406 | ` * array getdate ([ int $timestamp = time() ])` |
|      - | 7407 | ` *  Get date/time information.` |
|      - | 7408 | ` * Parameter` |
|      - | 7409 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|      - | 7410 | ` *     that defaults to the current local time if a timestamp is not given.` |
|      - | 7411 | ` *     In other words, it defaults to the value of time().` |
|      - | 7412 | ` * Returns` |
|      - | 7413 | ` *  Returns an associative array of information related to the timestamp.` |
|      - | 7414 | ` *  Elements from the returned associative array are as follows:` |
|      - | 7415 | ` *   KEY                                                         VALUE` |
|      - | 7416 | ` * ---------                                                    -------` |
|      - | 7417 | ` * "seconds" 	Numeric representation of seconds 	            0 to 59` |
|      - | 7418 | ` * "minutes" 	Numeric representation of minutes 	            0 to 59` |
|      - | 7419 | ` * "hours" 	    Numeric representation of hours 	            0 to 23` |
|      - | 7420 | ` * "mday" 	    Numeric representation of the day of the month 	1 to 31` |
|      - | 7421 | ` * "wday" 	    Numeric representation of the day of the week 	0 (for Sunday) through 6 (for Saturday)` |
|      - | 7422 | ` * "mon" 	    Numeric representation of a month 	            1 through 12` |
|      - | 7423 | ` * "year" 	    A full numeric representation of a year,        4 digits 	Examples: 1999 or 2003` |
|      - | 7424 | ` * "yday" 	    Numeric representation of the day of the year   0 through 365` |
|      - | 7425 | ` * "weekday" 	A full textual representation of the day of the week 	Sunday through Saturday` |
|      - | 7426 | ` * "month" 	    A full textual representation of a month, such as January or March 	January through December` |
|      - | 7427 | ` * 0 	        Seconds since the Unix Epoch, similar to the values returned by time() and used by date().` |
|      - | 7428 | ` * NOTE:` |
|      - | 7429 | ` *   NULL is returned on failure.` |
|      - | 7430 | ` */` |
|      8 | 7431 | `static int PH7_builtin_getdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7432 |  |
|      - | 7433 | `	ph7_value *pValue,*pArray;` |
|      - | 7434 | `	Sytm sTm;` |
|      9 | 7435 | `	if( nArg < 1 ){` |
|      - | 7436 | `#ifdef __WINNT__` |
|      - | 7437 | `		SYSTEMTIME sOS;` |
|      1 | 7438 | `		GetSystemTime(&sOS);` |
|      1 | 7439 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 7440 | `#else` |
|      - | 7441 | `		struct tm *pTm;` |
|      - | 7442 | `		time_t t;` |
|      4 | 7443 | `		time(&t);` |
|      4 | 7444 | `		pTm = localtime(&t);` |
|      4 | 7445 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7446 | `#endif` |
|      3 | 7447 | `	}else{` |
|      - | 7448 | `		/* Use the given timestamp */` |
|      - | 7449 | `		time_t t;` |
|      - | 7450 | `		struct tm *pTm;` |
|      - | 7451 | `#ifdef __WINNT__` |
|      - | 7452 | `#ifdef _MSC_VER` |
|      - | 7453 | `#if _MSC_VER >= 1400 /* Visual Studio 2005 and up */` |
|      - | 7454 | `#pragma warning(disable:4996) /* _CRT_SECURE...*/` |
|      - | 7455 | `#endif` |
|      - | 7456 | `#endif` |
|      - | 7457 | `#endif` |
|      5 | 7458 | `		if( ph7_value_is_int(apArg[0]) ){` |
|      5 | 7459 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|      5 | 7460 | `			pTm = localtime(&t);` |
|      5 | 7461 | `			if( pTm == 0 ){` |
|    ! 0 | 7462 | `				time(&t);` |
|    ! 0 | 7463 | `			}` |
|      3 | 7464 | `		}else{` |
|    ! 0 | 7465 | `			time(&t);` |
|      - | 7466 | `		}` |
|      5 | 7467 | `		pTm = localtime(&t);` |
|      5 | 7468 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7469 | `	}` |
|      - | 7470 | `	/* Element value */` |
|      9 | 7471 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      9 | 7472 | `	if( pValue == 0 ){` |
|      - | 7473 | `		/* Return NULL */` |
|    ! 0 | 7474 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7475 | `		return PH7_OK;` |
|      - | 7476 | `	}` |
|      - | 7477 | `	/* Create a new array */` |
|      9 | 7478 | `	pArray = ph7_context_new_array(pCtx);` |
|      9 | 7479 | `	if( pArray == 0 ){` |
|      - | 7480 | `		/* Return NULL */` |
|    ! 0 | 7481 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7482 | `		return PH7_OK;` |
|      - | 7483 | `	}` |
|      - | 7484 | `	/* Fill the array */` |
|      - | 7485 | `	/* Seconds */` |
|      9 | 7486 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|      9 | 7487 | `	ph7_array_add_strkey_elem(pArray,"seconds",pValue);` |
|      - | 7488 | `	/* Minutes */` |
|      9 | 7489 | `	ph7_value_int(pValue,sTm.tm_min);` |
|      9 | 7490 | `	ph7_array_add_strkey_elem(pArray,"minutes",pValue);` |
|      - | 7491 | `	/* Hours */` |
|      9 | 7492 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|      9 | 7493 | `	ph7_array_add_strkey_elem(pArray,"hours",pValue);` |
|      - | 7494 | `	/* mday */` |
|      9 | 7495 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|      9 | 7496 | `	ph7_array_add_strkey_elem(pArray,"mday",pValue);` |
|      - | 7497 | `	/* wday */` |
|      9 | 7498 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|      9 | 7499 | `	ph7_array_add_strkey_elem(pArray,"wday",pValue);` |
|      - | 7500 | `	/* mon */` |
|      9 | 7501 | `	ph7_value_int(pValue,sTm.tm_mon+1);` |
|      9 | 7502 | `	ph7_array_add_strkey_elem(pArray,"mon",pValue);` |
|      - | 7503 | `	/* year */` |
|      9 | 7504 | `	ph7_value_int(pValue,sTm.tm_year);` |
|      9 | 7505 | `	ph7_array_add_strkey_elem(pArray,"year",pValue);` |
|      - | 7506 | `	/* yday */` |
|      9 | 7507 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|      9 | 7508 | `	ph7_array_add_strkey_elem(pArray,"yday",pValue);` |
|      - | 7509 | `	/* Weekday */` |
|      9 | 7510 | `	ph7_value_string(pValue,SyTimeGetDay(sTm.tm_wday),-1);` |
|      9 | 7511 | `	ph7_array_add_strkey_elem(pArray,"weekday",pValue);` |
|      - | 7512 | `	/* Month */` |
|      9 | 7513 | `	ph7_value_reset_string_cursor(pValue);` |
|      9 | 7514 | `	ph7_value_string(pValue,SyTimeGetMonth(sTm.tm_mon),-1);` |
|      9 | 7515 | `	ph7_array_add_strkey_elem(pArray,"month",pValue);` |
|      - | 7516 | `	/* Seconds since the epoch */` |
|      9 | 7517 | `	ph7_value_int64(pValue,(ph7_int64)time(0));` |
|      9 | 7518 | `	ph7_array_add_intkey_elem(pArray,0 /* Index zero */,pValue);` |
|      - | 7519 | `	/* Return the freshly created array */` |
|      9 | 7520 | `	ph7_result_value(pCtx,pArray);` |
|      9 | 7521 | `	return PH7_OK;` |
|      5 | 7522 |  |
|      - | 7523 | `/*` |
|      - | 7524 | ` * mixed gettimeofday([ bool $return_float = false ] )` |
|      - | 7525 | ` *   Returns an associative array containing the data returned from the system call.` |
|      - | 7526 | ` * Parameters` |
|      - | 7527 | ` *  $return_float` |
|      - | 7528 | ` *   When set to TRUE, a float instead of an array is returned.` |
|      - | 7529 | ` * Return` |
|      - | 7530 | ` *   By default an array is returned. If return_float is set, then` |
|      - | 7531 | ` *   a float is returned.` |
|      - | 7532 | ` */` |
|      4 | 7533 | `static int PH7_builtin_gettimeofday(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7534 |  |
|      5 | 7535 | `	int bFloat = 0;` |
|      - | 7536 | `	sytime sTime;` |
|      - | 7537 | `#if defined(__UNIXES__)` |
|      - | 7538 | `	struct timeval tv;` |
|      4 | 7539 | `	gettimeofday(&tv,0);` |
|      4 | 7540 | `	sTime.tm_sec  = (long)tv.tv_sec;` |
|      4 | 7541 | `	sTime.tm_usec = (long)tv.tv_usec;` |
|      - | 7542 | `#else` |
|      - | 7543 | `	time_t tt;` |
|      1 | 7544 | `	time(&tt);` |
|      1 | 7545 | `	sTime.tm_sec  = (long)tt;` |
|      1 | 7546 | `	sTime.tm_usec = (long)(tt%SX_USEC_PER_SEC);` |
|      - | 7547 | `#endif /* __UNIXES__ */` |
|      5 | 7548 | `	if( nArg > 0 ){` |
|      5 | 7549 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|      2 | 7550 | `	}` |
|      5 | 7551 | `	if( bFloat ){` |
|      - | 7552 | `		/* Return as float */` |
|      3 | 7553 | `		ph7_result_double(pCtx,(double)sTime.tm_sec);` |
|      2 | 7554 | `	}else{` |
|      - | 7555 | `		/* Return an associative array */` |
|      - | 7556 | `		ph7_value *pValue,*pArray;` |
|      - | 7557 | `		/* Create a new array */` |
|      3 | 7558 | `		pArray = ph7_context_new_array(pCtx);` |
|      - | 7559 | `		/* Element value */` |
|      3 | 7560 | `		pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 7561 | `		if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 7562 | `			/* Return NULL */` |
|    ! 0 | 7563 | `			ph7_result_null(pCtx);` |
|    ! 0 | 7564 | `			return PH7_OK;` |
|      - | 7565 | `		}` |
|      - | 7566 | `		/* Fill the array */` |
|      - | 7567 | `		/* sec */` |
|      3 | 7568 | `		ph7_value_int64(pValue,sTime.tm_sec);` |
|      3 | 7569 | `		ph7_array_add_strkey_elem(pArray,"sec",pValue);` |
|      - | 7570 | `		/* usec */` |
|      3 | 7571 | `		ph7_value_int64(pValue,sTime.tm_usec);` |
|      3 | 7572 | `		ph7_array_add_strkey_elem(pArray,"usec",pValue);` |
|      - | 7573 | `		/* Return the array */` |
|      3 | 7574 | `		ph7_result_value(pCtx,pArray);` |
|      - | 7575 | `	}` |
|      5 | 7576 | `	return PH7_OK;` |
|      3 | 7577 |  |
|      - | 7578 | `/* Check if the given year is leap or not */` |
|      - | 7579 | `#define IS_LEAP_YEAR(YEAR)	(YEAR % 400 ? ( YEAR % 100 ? ( YEAR % 4 ? 0 : 1 ) : 0 ) : 1)` |
|      - | 7580 | `/* ISO-8601 numeric representation of the day of the week */` |
|      - | 7581 | `static const int aISO8601[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|      - | 7582 | `/*` |
|      - | 7583 | ` * Format a given date string.` |
|      - | 7584 | ` * Supported format: (Taken from PHP online docs)` |
|      - | 7585 | ` * character 	Description` |
|      - | 7586 | ` * d          Day of the month` |
|      - | 7587 | ` * D          A textual representation of a days` |
|      - | 7588 | ` * j          Day of the month without leading zeros` |
|      - | 7589 | ` * l          A full textual representation of the day of the week` |
|      - | 7590 | ` * N          ISO-8601 numeric representation of the day of the week` |
|      - | 7591 | ` * w          Numeric representation of the day of the week` |
|      - | 7592 | ` * z          The day of the year (starting from 0)` |
|      - | 7593 | ` * F          A full textual representation of a month, such as January or March` |
|      - | 7594 | ` * m          Numeric representation of a month, with leading zeros 	01 through 12` |
|      - | 7595 | ` * M          A short textual representation of a month, three letters 	Jan through Dec` |
|      - | 7596 | ` * n          Numeric representation of a month, without leading zeros 	1 through 12` |
|      - | 7597 | ` * t          Number of days in the given month 	28 through 31` |
|      - | 7598 | ` * L          Whether it's a leap year 	1 if it is a leap year, 0 otherwise.` |
|      - | 7599 | ` * o          ISO-8601 year number. This has the same value as Y, except that if the ISO week number` |
|      - | 7600 | ` *            (W) belongs to the previous or next year, that year is used instead. (added in PHP 5.1.0) Examples: 1999 or 2003` |
|      - | 7601 | ` * Y          A full numeric representation of a year, 4 digits 	Examples: 1999 or 2003` |
|      - | 7602 | ` * y          A two digit representation of a year 	Examples: 99 or 03` |
|      - | 7603 | ` * a          Lowercase Ante meridiem and Post meridiem 	am or pm` |
|      - | 7604 | ` * A          Uppercase Ante meridiem and Post meridiem 	AM or PM` |
|      - | 7605 | ` * g          12-hour format of an hour without leading zeros 	1 through 12` |
|      - | 7606 | ` * G          24-hour format of an hour without leading zeros 	0 through 23` |
|      - | 7607 | ` * h          12-hour format of an hour with leading zeros 	01 through 12` |
|      - | 7608 | ` * H          24-hour format of an hour with leading zeros 	00 through 23` |
|      - | 7609 | ` * i          Minutes with leading zeros 	00 to 59` |
|      - | 7610 | ` * s          Seconds, with leading zeros 	00 through 59` |
|      - | 7611 | ` * u          Microseconds Example: 654321` |
|      - | 7612 | ` * e          Timezone identifier 	Examples: UTC, GMT, Atlantic/Azores` |
|      - | 7613 | ` * I          (capital i) Whether or not the date is in daylight saving time 	1 if Daylight Saving Time, 0 otherwise.` |
|      - | 7614 | ` * r          RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 +0200` |
|      - | 7615 | ` * U          Seconds since the Unix Epoch (January 1 1970 00:00:00 GMT)` |
|      - | 7616 | ` * S          English ordinal suffix for the day of the month, 2 characters` |
|      - | 7617 | ` * O          Difference to Greenwich time (GMT) in hours` |
|      - | 7618 | ` * Z          Timezone offset in seconds. The offset for timezones west of UTC is always negative, and for those` |
|      - | 7619 | ` *            east of UTC is always positive.` |
|      - | 7620 | ` * c         ISO 8601 date` |
|      - | 7621 | ` */` |
|     46 | 7622 | `static sxi32 DateFormat(ph7_context *pCtx,const char *zIn,int nLen,Sytm *pTm)` |
|      1 | 7623 |  |
|     47 | 7624 | `	const char *zEnd = &zIn[nLen];` |
|      - | 7625 | `	const char *zCur;` |
|      - | 7626 | `	/* Start the format process */` |
|     78 | 7627 | `	for(;;){` |
|    157 | 7628 | `		if( zIn >= zEnd ){` |
|      - | 7629 | `			/* No more input to process */` |
|     47 | 7630 | `			break;` |
|      - | 7631 | `		}` |
|    111 | 7632 | `		switch(zIn[0]){` |
|      7 | 7633 | `		case 'd':` |
|      - | 7634 | `			/* Day of the month, 2 digits with leading zeros */` |
|     15 | 7635 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mday);` |
|     15 | 7636 | `			break;` |
|    ! 0 | 7637 | `		case 'D':` |
|      - | 7638 | `			/*A textual representation of a day, three letters*/` |
|    ! 0 | 7639 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|    ! 0 | 7640 | `			ph7_result_string(pCtx,zCur,3);` |
|    ! 0 | 7641 | `			break;` |
|    ! 0 | 7642 | `		case 'j':` |
|      - | 7643 | `			/*	Day of the month without leading zeros */` |
|    ! 0 | 7644 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mday);` |
|    ! 0 | 7645 | `			break;` |
|      2 | 7646 | `		case 'l':` |
|      - | 7647 | `			/* A full textual representation of the day of the week */` |
|      5 | 7648 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|      5 | 7649 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|      5 | 7650 | `			break;` |
|    ! 0 | 7651 | `		case 'N':{` |
|      - | 7652 | `			/* ISO-8601 numeric representation of the day of the week */` |
|    ! 0 | 7653 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|    ! 0 | 7654 | `			break;` |
|      - | 7655 | `				 }` |
|    ! 0 | 7656 | `		case 'w':` |
|      - | 7657 | `			/*Numeric representation of the day of the week*/` |
|    ! 0 | 7658 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|    ! 0 | 7659 | `			break;` |
|    ! 0 | 7660 | `		case 'z':` |
|      - | 7661 | `			/*The day of the year*/` |
|    ! 0 | 7662 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_yday);` |
|    ! 0 | 7663 | `			break;` |
|      2 | 7664 | `		case 'F':` |
|      - | 7665 | `			/*A full textual representation of a month, such as January or March*/` |
|      5 | 7666 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|      5 | 7667 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|      5 | 7668 | `			break;` |
|      7 | 7669 | `		case 'm':` |
|      - | 7670 | `			/*Numeric representation of a month, with leading zeros*/` |
|     15 | 7671 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|     15 | 7672 | `			break;` |
|    ! 0 | 7673 | `		case 'M':` |
|      - | 7674 | `			/*A short textual representation of a month, three letters*/` |
|    ! 0 | 7675 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|    ! 0 | 7676 | `			ph7_result_string(pCtx,zCur,3);` |
|    ! 0 | 7677 | `			break;` |
|    ! 0 | 7678 | `		case 'n':` |
|      - | 7679 | `			/*Numeric representation of a month, without leading zeros*/` |
|    ! 0 | 7680 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mon + 1);` |
|    ! 0 | 7681 | `			break;` |
|    ! 0 | 7682 | `		case 't':{` |
|      - | 7683 | `			static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|    ! 0 | 7684 | `			int nDays = aMonDays[pTm->tm_mon % 12 ];` |
|    ! 0 | 7685 | `			if( pTm->tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(pTm->tm_year) ){` |
|    ! 0 | 7686 | `				nDays = 28;` |
|    ! 0 | 7687 | `			}` |
|      - | 7688 | `			/*Number of days in the given month*/` |
|    ! 0 | 7689 | `			ph7_result_string_format(pCtx,"%d",nDays);` |
|    ! 0 | 7690 | `			break;` |
|      - | 7691 | `				 }` |
|    ! 0 | 7692 | `		case 'L':{` |
|    ! 0 | 7693 | `			int isLeap = IS_LEAP_YEAR(pTm->tm_year);` |
|      - | 7694 | `			/* Whether it's a leap year */` |
|    ! 0 | 7695 | `			ph7_result_string_format(pCtx,"%d",isLeap);` |
|    ! 0 | 7696 | `			break;` |
|      - | 7697 | `				 }` |
|    ! 0 | 7698 | `		case 'o':` |
|      - | 7699 | `			/* ISO-8601 year number.*/` |
|    ! 0 | 7700 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|    ! 0 | 7701 | `			break;` |
|      9 | 7702 | `		case 'Y':` |
|      - | 7703 | `			/*	A full numeric representation of a year, 4 digits */` |
|     19 | 7704 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|     19 | 7705 | `			break;` |
|    ! 0 | 7706 | `		case 'y':` |
|      - | 7707 | `			/*A two digit representation of a year*/` |
|    ! 0 | 7708 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_year%100);` |
|    ! 0 | 7709 | `			break;` |
|    ! 0 | 7710 | `		case 'a':` |
|      - | 7711 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7712 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",2);` |
|    ! 0 | 7713 | `			break;` |
|    ! 0 | 7714 | `		case 'A':` |
|      - | 7715 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7716 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",2);` |
|    ! 0 | 7717 | `			break;` |
|    ! 0 | 7718 | `		case 'g':` |
|      - | 7719 | `			/*	12-hour format of an hour without leading zeros*/` |
|    ! 0 | 7720 | `			ph7_result_string_format(pCtx,"%d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7721 | `			break;` |
|    ! 0 | 7722 | `		case 'G':` |
|      - | 7723 | `			/* 24-hour format of an hour without leading zeros */` |
|    ! 0 | 7724 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_hour);` |
|    ! 0 | 7725 | `			break;` |
|    ! 0 | 7726 | `		case 'h':` |
|      - | 7727 | `			/* 12-hour format of an hour with leading zeros */` |
|    ! 0 | 7728 | `			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7729 | `			break;` |
|      3 | 7730 | `		case 'H':` |
|      - | 7731 | `			/*	24-hour format of an hour with leading zeros */` |
|      7 | 7732 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|      7 | 7733 | `			break;` |
|      3 | 7734 | `		case 'i':` |
|      - | 7735 | `			/* 	Minutes with leading zeros */` |
|      7 | 7736 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|      7 | 7737 | `			break;` |
|      3 | 7738 | `		case 's':` |
|      - | 7739 | `			/* 	second with leading zeros */` |
|      7 | 7740 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|      7 | 7741 | `			break;` |
|    ! 0 | 7742 | `		case 'u':` |
|      - | 7743 | `			/* 	Microseconds */` |
|    ! 0 | 7744 | `			ph7_result_string_format(pCtx,"%u",pTm->tm_sec * SX_USEC_PER_SEC);` |
|    ! 0 | 7745 | `			break;` |
|    ! 0 | 7746 | `		case 'S':{` |
|      - | 7747 | `			/* English ordinal suffix for the day of the month, 2 characters */` |
|      - | 7748 | `			static const char zSuffix[] = "thstndrdthththththth";` |
|    ! 0 | 7749 | `			int v = pTm->tm_mday;` |
|    ! 0 | 7750 | `			ph7_result_string(pCtx,&zSuffix[2 * (int)(v / 10 % 10 != 1 ? v % 10 : 0)],(int)sizeof(char) * 2);` |
|    ! 0 | 7751 | `			break;` |
|      - | 7752 | `				 }` |
|    ! 0 | 7753 | `		case 'e':` |
|      - | 7754 | `			/* 	Timezone identifier */` |
|    ! 0 | 7755 | `			zCur = pTm->tm_zone;` |
|    ! 0 | 7756 | `			if( zCur == 0 ){` |
|      - | 7757 | `				/* Assume GMT */` |
|    ! 0 | 7758 | `				zCur = "GMT";` |
|    ! 0 | 7759 | `			}` |
|    ! 0 | 7760 | `			ph7_result_string(pCtx,zCur,-1);` |
|    ! 0 | 7761 | `			break;` |
|    ! 0 | 7762 | `		case 'I':` |
|      - | 7763 | `			/* Whether or not the date is in daylight saving time */` |
|      - | 7764 | `#ifdef __WINNT__` |
|      - | 7765 | `#ifdef _MSC_VER` |
|      - | 7766 | `#ifndef _WIN32_WCE` |
|    ! 0 | 7767 | `			_get_daylight(&pTm->tm_isdst);` |
|      - | 7768 | `#endif` |
|      - | 7769 | `#endif` |
|      - | 7770 | `#endif` |
|    ! 0 | 7771 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_isdst == 1);` |
|    ! 0 | 7772 | `			break;` |
|    ! 0 | 7773 | `		case 'r':` |
|      - | 7774 | `			/* RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 */` |
|    ! 0 | 7775 | `			ph7_result_string_format(pCtx,"%.3s, %02d %.3s %4d %02d:%02d:%02d",` |
|    ! 0 | 7776 | `				SyTimeGetDay(pTm->tm_wday),` |
|    ! 0 | 7777 | `				pTm->tm_mday,` |
|    ! 0 | 7778 | `				SyTimeGetMonth(pTm->tm_mon),` |
|    ! 0 | 7779 | `				pTm->tm_year,` |
|    ! 0 | 7780 | `				pTm->tm_hour,` |
|    ! 0 | 7781 | `				pTm->tm_min,` |
|    ! 0 | 7782 | `				pTm->tm_sec` |
|      - | 7783 | `				);` |
|    ! 0 | 7784 | `			break;` |
|    ! 0 | 7785 | `		case 'U':{` |
|      - | 7786 | `			time_t tt;` |
|      - | 7787 | `			/* Seconds since the Unix Epoch */` |
|    ! 0 | 7788 | `			time(&tt);` |
|    ! 0 | 7789 | `			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);` |
|    ! 0 | 7790 | `			break;` |
|      - | 7791 | `				 }` |
|    ! 0 | 7792 | `		case 'O':` |
|      - | 7793 | `		case 'P':` |
|      - | 7794 | `			/* Difference to Greenwich time (GMT) in hours */` |
|    ! 0 | 7795 | `			ph7_result_string_format(pCtx,"%+05d",pTm->tm_gmtoff);` |
|    ! 0 | 7796 | `			break;` |
|    ! 0 | 7797 | `		case 'Z':` |
|      - | 7798 | `			/* Timezone offset in seconds. The offset for timezones west of UTC` |
|      - | 7799 | `			 * is always negative, and for those east of UTC is always positive.` |
|      - | 7800 | `			 */` |
|    ! 0 | 7801 | `			ph7_result_string_format(pCtx,"%+05d",pTm->tm_gmtoff);` |
|    ! 0 | 7802 | `			break;` |
|      1 | 7803 | `		case 'c':` |
|      - | 7804 | `			/* 	ISO 8601 date */` |
|      4 | 7805 | `			ph7_result_string_format(pCtx,"%4d-%02d-%02dT%02d:%02d:%02d%+05d",` |
|      1 | 7806 | `				pTm->tm_year,` |
|      2 | 7807 | `				pTm->tm_mon+1,` |
|      1 | 7808 | `				pTm->tm_mday,` |
|      1 | 7809 | `				pTm->tm_hour,` |
|      1 | 7810 | `				pTm->tm_min,` |
|      1 | 7811 | `				pTm->tm_sec,` |
|      1 | 7812 | `				pTm->tm_gmtoff` |
|      - | 7813 | `				);` |
|      3 | 7814 | `			break;` |
|      1 | 7815 | `		case '\\':` |
|      3 | 7816 | `			zIn++;` |
|      - | 7817 | `			/* Expand verbatim */` |
|      3 | 7818 | `			if( zIn < zEnd ){` |
|      3 | 7819 | `				ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|      1 | 7820 | `			}` |
|      3 | 7821 | `			break;` |
|     17 | 7822 | `		default:` |
|      - | 7823 | `			/* Unknown format specifer,expand verbatim */` |
|     35 | 7824 | `			ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|     34 | 7825 | `			break;` |
|      - | 7826 | `		}` |
|      - | 7827 | `		/* Point to the next character */` |
|    111 | 7828 | `		zIn++;` |
|      1 | 7829 | `	}` |
|     47 | 7830 | `	return SXRET_OK;` |
|      1 | 7831 |  |
|      - | 7832 | `/*` |
|      - | 7833 | ` * PH7 implementation of the strftime() function.` |
|      - | 7834 | ` * The following formats are supported:` |
|      - | 7835 | ` * %a 	An abbreviated textual representation of the day` |
|      - | 7836 | ` * %A 	A full textual representation of the day` |
|      - | 7837 | ` * %d 	Two-digit day of the month (with leading zeros)` |
|      - | 7838 | ` * %e 	Day of the month, with a space preceding single digits.` |
|      - | 7839 | ` * %j 	Day of the year, 3 digits with leading zeros` |
|      - | 7840 | ` * %u 	ISO-8601 numeric representation of the day of the week 	1 (for Monday) though 7 (for Sunday)` |
|      - | 7841 | ` * %w 	Numeric representation of the day of the week 0 (for Sunday) through 6 (for Saturday)` |
|      - | 7842 | ` * %U 	Week number of the given year, starting with the first Sunday as the first week` |
|      - | 7843 | ` * %V 	ISO-8601:1988 week number of the given year, starting with the first week of the year with at least` |
|      - | 7844 | ` *   4 weekdays, with Monday being the start of the week.` |
|      - | 7845 | ` * %W 	A numeric representation of the week of the year` |
|      - | 7846 | ` * %b 	Abbreviated month name, based on the locale` |
|      - | 7847 | ` * %B 	Full month name, based on the locale` |
|      - | 7848 | ` * %h 	Abbreviated month name, based on the locale (an alias of %b)` |
|      - | 7849 | ` * %m 	Two digit representation of the month` |
|      - | 7850 | ` * %C 	Two digit representation of the century (year divided by 100, truncated to an integer)` |
|      - | 7851 | ` * %g 	Two digit representation of the year going by ISO-8601:1988 standards (see %V)` |
|      - | 7852 | ` * %G 	The full four-digit version of %g` |
|      - | 7853 | ` * %y 	Two digit representation of the year` |
|      - | 7854 | ` * %Y 	Four digit representation for the year` |
|      - | 7855 | ` * %H 	Two digit representation of the hour in 24-hour format` |
|      - | 7856 | ` * %I 	Two digit representation of the hour in 12-hour format` |
|      - | 7857 | ` * %l (lower-case 'L') 	Hour in 12-hour format, with a space preceeding single digits` |
|      - | 7858 | ` * %M 	Two digit representation of the minute` |
|      - | 7859 | ` * %p 	UPPER-CASE 'AM' or 'PM' based on the given time` |
|      - | 7860 | ` * %P 	lower-case 'am' or 'pm' based on the given time` |
|      - | 7861 | ` * %r 	Same as "%I:%M:%S %p"` |
|      - | 7862 | ` * %R 	Same as "%H:%M"` |
|      - | 7863 | ` * %S 	Two digit representation of the second` |
|      - | 7864 | ` * %T 	Same as "%H:%M:%S"` |
|      - | 7865 | ` * %X 	Preferred time representation based on locale, without the date` |
|      - | 7866 | ` * %z 	Either the time zone offset from UTC or the abbreviation` |
|      - | 7867 | ` * %Z 	The time zone offset/abbreviation option NOT given by %z` |
|      - | 7868 | ` * %c 	Preferred date and time stamp based on local` |
|      - | 7869 | ` * %D 	Same as "%m/%d/%y"` |
|      - | 7870 | ` * %F 	Same as "%Y-%m-%d"` |
|      - | 7871 | ` * %s 	Unix Epoch Time timestamp (same as the time() function)` |
|      - | 7872 | ` * %x 	Preferred date representation based on locale, without the time` |
|      - | 7873 | ` * %n 	A newline character ("\n")` |
|      - | 7874 | ` * %t 	A Tab character ("\t")` |
|      - | 7875 | ` * %% 	A literal percentage character ("%")` |
|      - | 7876 | ` */` |
|     16 | 7877 | `static int PH7_Strftime(` |
|      - | 7878 | `	ph7_context *pCtx,  /* Call context */` |
|      - | 7879 | `	const char *zIn,    /* Input string */` |
|      - | 7880 | `	int nLen,           /* Input length */` |
|      - | 7881 | `	Sytm *pTm           /* Parse of the given time */` |
|      - | 7882 | `	)` |
|      1 | 7883 |  |
|     17 | 7884 | `	const char *zCur,*zEnd = &zIn[nLen];` |
|      - | 7885 | `	int c;` |
|      - | 7886 | `	/* Start the format process */` |
|     18 | 7887 | `	for(;;){` |
|     37 | 7888 | `		zCur = zIn;` |
|     41 | 7889 | `		while(zIn < zEnd && zIn[0] != '%' ){` |
|      5 | 7890 | `			zIn++;` |
|      1 | 7891 | `		}` |
|     37 | 7892 | `		if( zIn > zCur ){` |
|      - | 7893 | `			/* Consume input verbatim */` |
|      5 | 7894 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 7895 | `		}` |
|     37 | 7896 | `		zIn++; /* Jump the percent sign */` |
|     37 | 7897 | `		if( zIn >= zEnd ){` |
|      - | 7898 | `			/* No more input to process */` |
|     17 | 7899 | `			break;` |
|      - | 7900 | `		}` |
|     21 | 7901 | `		c = zIn[0];` |
|      - | 7902 | `		/* Act according to the current specifer */` |
|     21 | 7903 | `		switch(c){` |
|    ! 0 | 7904 | `		case '%':` |
|      - | 7905 | `			/* A literal percentage character ("%") */` |
|    ! 0 | 7906 | `			ph7_result_string(pCtx,"%",(int)sizeof(char));` |
|    ! 0 | 7907 | `			break;` |
|    ! 0 | 7908 | `		case 't':` |
|      - | 7909 | `			/* A Tab character */` |
|    ! 0 | 7910 | `			ph7_result_string(pCtx,"\t",(int)sizeof(char));` |
|    ! 0 | 7911 | `			break;` |
|    ! 0 | 7912 | `		case 'n':` |
|      - | 7913 | `			/* A newline character */` |
|    ! 0 | 7914 | `			ph7_result_string(pCtx,"\n",(int)sizeof(char));` |
|    ! 0 | 7915 | `			break;` |
|      1 | 7916 | `		case 'a':` |
|      - | 7917 | `			/* An abbreviated textual representation of the day */` |
|      3 | 7918 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),(int)sizeof(char)*3);` |
|      3 | 7919 | `			break;` |
|    ! 0 | 7920 | `		case 'A':` |
|      - | 7921 | `			/* A full textual representation of the day */` |
|    ! 0 | 7922 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),-1/*Compute length automatically*/);` |
|    ! 0 | 7923 | `			break;` |
|    ! 0 | 7924 | `		case 'e':` |
|      - | 7925 | `			/* Day of the month, 2 digits with leading space for single digit*/` |
|    ! 0 | 7926 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_mday);` |
|    ! 0 | 7927 | `			break;` |
|      2 | 7928 | `		case 'd':` |
|      - | 7929 | `			/* Two-digit day of the month (with leading zeros) */` |
|      5 | 7930 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon+1);` |
|      5 | 7931 | `			break;` |
|    ! 0 | 7932 | `		case 'j':` |
|      - | 7933 | `			/*The day of the year,3 digits with leading zeros*/` |
|    ! 0 | 7934 | `			ph7_result_string_format(pCtx,"%03d",pTm->tm_yday);` |
|    ! 0 | 7935 | `			break;` |
|    ! 0 | 7936 | `		case 'u':` |
|      - | 7937 | `			/* ISO-8601 numeric representation of the day of the week */` |
|    ! 0 | 7938 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|    ! 0 | 7939 | `			break;` |
|    ! 0 | 7940 | `		case 'w':` |
|      - | 7941 | `			/* Numeric representation of the day of the week */` |
|    ! 0 | 7942 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|    ! 0 | 7943 | `			break;` |
|    ! 0 | 7944 | `		case 'b':` |
|      - | 7945 | `		case 'h':` |
|      - | 7946 | `			/*A short textual representation of a month, three letters (Not based on locale)*/` |
|    ! 0 | 7947 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),(int)sizeof(char)*3);` |
|    ! 0 | 7948 | `			break;` |
|    ! 0 | 7949 | `		case 'B':` |
|      - | 7950 | `			/* Full month name (Not based on locale) */` |
|    ! 0 | 7951 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),-1/*Compute length automatically*/);` |
|    ! 0 | 7952 | `			break;` |
|      2 | 7953 | `		case 'm':` |
|      - | 7954 | `			/*Numeric representation of a month, with leading zeros*/` |
|      5 | 7955 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|      5 | 7956 | `			break;` |
|    ! 0 | 7957 | `		case 'C':` |
|      - | 7958 | `			/* Two digit representation of the century */` |
|    ! 0 | 7959 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year/100);` |
|    ! 0 | 7960 | `			break;` |
|    ! 0 | 7961 | `		case 'y':` |
|      - | 7962 | `		case 'g':` |
|      - | 7963 | `			/* Two digit representation of the year */` |
|    ! 0 | 7964 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year%100);` |
|    ! 0 | 7965 | `			break;` |
|      2 | 7966 | `		case 'Y':` |
|      - | 7967 | `		case 'G':` |
|      - | 7968 | `			/* Four digit representation of the year */` |
|      5 | 7969 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|      5 | 7970 | `			break;` |
|    ! 0 | 7971 | `		case 'I':` |
|      - | 7972 | `			/* 12-hour format of an hour with leading zeros */` |
|    ! 0 | 7973 | `			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7974 | `			break;` |
|    ! 0 | 7975 | `		case 'l':` |
|      - | 7976 | `			/* 12-hour format of an hour with leading space */` |
|    ! 0 | 7977 | `			ph7_result_string_format(pCtx,"%2d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7978 | `			break;` |
|      1 | 7979 | `		case 'H':` |
|      - | 7980 | `			/* 24-hour format of an hour with leading zeros */` |
|      3 | 7981 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|      3 | 7982 | `			break;` |
|      1 | 7983 | `		case 'M':` |
|      - | 7984 | `			/* Minutes with leading zeros */` |
|      3 | 7985 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|      3 | 7986 | `			break;` |
|    ! 0 | 7987 | `		case 'S':` |
|      - | 7988 | `			/* Seconds with leading zeros */` |
|    ! 0 | 7989 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|    ! 0 | 7990 | `			break;` |
|    ! 0 | 7991 | `		case 'z':` |
|      - | 7992 | `		case 'Z':` |
|      - | 7993 | `			/* 	Timezone identifier */` |
|    ! 0 | 7994 | `			zCur = pTm->tm_zone;` |
|    ! 0 | 7995 | `			if( zCur == 0 ){` |
|      - | 7996 | `				/* Assume GMT */` |
|    ! 0 | 7997 | `				zCur = "GMT";` |
|    ! 0 | 7998 | `			}` |
|    ! 0 | 7999 | `			ph7_result_string(pCtx,zCur,-1);` |
|    ! 0 | 8000 | `			break;` |
|    ! 0 | 8001 | `		case 'T':` |
|      - | 8002 | `		case 'X':` |
|      - | 8003 | `			/* Same as "%H:%M:%S" */` |
|    ! 0 | 8004 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d",pTm->tm_hour,pTm->tm_min,pTm->tm_sec);` |
|    ! 0 | 8005 | `			break;` |
|    ! 0 | 8006 | `		case 'R':` |
|      - | 8007 | `			/* Same as "%H:%M" */` |
|    ! 0 | 8008 | `			ph7_result_string_format(pCtx,"%02d:%02d",pTm->tm_hour,pTm->tm_min);` |
|    ! 0 | 8009 | `			break;` |
|    ! 0 | 8010 | `		case 'P':` |
|      - | 8011 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|    ! 0 | 8012 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",(int)sizeof(char)*2);` |
|    ! 0 | 8013 | `			break;` |
|    ! 0 | 8014 | `		case 'p':` |
|      - | 8015 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|    ! 0 | 8016 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",(int)sizeof(char)*2);` |
|    ! 0 | 8017 | `			break;` |
|    ! 0 | 8018 | `		case 'r':` |
|      - | 8019 | `			/* Same as "%I:%M:%S %p" */` |
|    ! 0 | 8020 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d %s",` |
|    ! 0 | 8021 | `				1+(pTm->tm_hour%12),` |
|    ! 0 | 8022 | `				pTm->tm_min,` |
|    ! 0 | 8023 | `				pTm->tm_sec,` |
|    ! 0 | 8024 | `				pTm->tm_hour > 12 ? "PM" : "AM"` |
|      - | 8025 | `				);` |
|    ! 0 | 8026 | `			break;` |
|      1 | 8027 | `		case 'D':` |
|      - | 8028 | `		case 'x':` |
|      - | 8029 | `			/* Same as "%m/%d/%y" */` |
|      4 | 8030 | `			ph7_result_string_format(pCtx,"%02d/%02d/%02d",` |
|      2 | 8031 | `				pTm->tm_mon+1,` |
|      1 | 8032 | `				pTm->tm_mday,` |
|      2 | 8033 | `				pTm->tm_year%100` |
|      - | 8034 | `				);` |
|      3 | 8035 | `			break;` |
|    ! 0 | 8036 | `		case 'F':` |
|      - | 8037 | `			/* Same as "%Y-%m-%d" */` |
|    ! 0 | 8038 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d",` |
|    ! 0 | 8039 | `				pTm->tm_year,` |
|    ! 0 | 8040 | `				pTm->tm_mon+1,` |
|    ! 0 | 8041 | `				pTm->tm_mday` |
|      - | 8042 | `				);` |
|    ! 0 | 8043 | `			break;` |
|    ! 0 | 8044 | `		case 'c':` |
|    ! 0 | 8045 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d %02d:%02d:%02d",` |
|    ! 0 | 8046 | `				pTm->tm_year,` |
|    ! 0 | 8047 | `				pTm->tm_mon+1,` |
|    ! 0 | 8048 | `				pTm->tm_mday,` |
|    ! 0 | 8049 | `				pTm->tm_hour,` |
|    ! 0 | 8050 | `				pTm->tm_min,` |
|    ! 0 | 8051 | `				pTm->tm_sec` |
|      - | 8052 | `				);` |
|    ! 0 | 8053 | `			break;` |
|    ! 0 | 8054 | `		case 's':{` |
|      - | 8055 | `			time_t tt;` |
|      - | 8056 | `			/* Seconds since the Unix Epoch */` |
|    ! 0 | 8057 | `			time(&tt);` |
|    ! 0 | 8058 | `			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);` |
|    ! 0 | 8059 | `			break;` |
|      - | 8060 | `				 }` |
|    ! 0 | 8061 | `		default:` |
|      - | 8062 | `			/* unknown specifer,simply ignore*/` |
|    ! 0 | 8063 | `			break;` |
|      - | 8064 | `		}` |
|      - | 8065 | `		/* Advance the cursor */` |
|     21 | 8066 | `		zIn++;` |
|      1 | 8067 | `	}` |
|     17 | 8068 | `	return SXRET_OK;` |
|      1 | 8069 |  |
|      - | 8070 | `/*` |
|      - | 8071 | ` * string date(string $format [, int $timestamp = time() ] )` |
|      - | 8072 | ` *  Returns a string formatted according to the given format string using` |
|      - | 8073 | ` *  the given integer timestamp or the current time if no timestamp is given.` |
|      - | 8074 | ` *  In other words, timestamp is optional and defaults to the value of time().` |
|      - | 8075 | ` * Parameters` |
|      - | 8076 | ` *  $format` |
|      - | 8077 | ` *   The format of the outputted date string (See code above)` |
|      - | 8078 | ` * $timestamp` |
|      - | 8079 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|      - | 8080 | ` *   that defaults to the current local time if a timestamp is not given.` |
|      - | 8081 | ` *   In other words, it defaults to the value of time().` |
|      - | 8082 | ` * Return` |
|      - | 8083 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|      - | 8084 | ` */` |
|     36 | 8085 | `static int PH7_builtin_date(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8086 |  |
|      - | 8087 | `	const char *zFormat;` |
|      - | 8088 | `	int nLen;` |
|      - | 8089 | `	Sytm sTm;` |
|     37 | 8090 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8091 | `		/* Missing/Invalid argument,return FALSE */` |
|      5 | 8092 | `		ph7_result_bool(pCtx,0);` |
|      5 | 8093 | `		return PH7_OK;` |
|      - | 8094 | `	}` |
|     33 | 8095 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     33 | 8096 | `	if( nLen < 1 ){` |
|      - | 8097 | `		/* Don't bother processing return the empty string */` |
|    ! 0 | 8098 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 8099 | `	}` |
|     33 | 8100 | `	if( nArg < 2 ){` |
|      - | 8101 | `#ifdef __WINNT__` |
|      - | 8102 | `		SYSTEMTIME sOS;` |
|      1 | 8103 | `		GetSystemTime(&sOS);` |
|      1 | 8104 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8105 | `#else` |
|      - | 8106 | `		struct tm *pTm;` |
|      - | 8107 | `		time_t t;` |
|     30 | 8108 | `		time(&t);` |
|     30 | 8109 | `		pTm = localtime(&t);` |
|     30 | 8110 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8111 | `#endif` |
|     16 | 8112 | `	}else{` |
|      - | 8113 | `		/* Use the given timestamp */` |
|      - | 8114 | `		time_t t;` |
|      - | 8115 | `		struct tm *pTm;` |
|      3 | 8116 | `		if( ph7_value_is_int(apArg[1]) ){` |
|      3 | 8117 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|      3 | 8118 | `			pTm = localtime(&t);` |
|      3 | 8119 | `			if( pTm == 0 ){` |
|    ! 0 | 8120 | `				time(&t);` |
|    ! 0 | 8121 | `			}` |
|      2 | 8122 | `		}else{` |
|    ! 0 | 8123 | `			time(&t);` |
|      - | 8124 | `		}` |
|      3 | 8125 | `		pTm = localtime(&t);` |
|      3 | 8126 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8127 | `	}` |
|      - | 8128 | `	/* Format the given string */` |
|     33 | 8129 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|     33 | 8130 | `	return PH7_OK;` |
|     19 | 8131 |  |
|      - | 8132 | `/*` |
|      - | 8133 | ` * string strftime(string $format [, int $timestamp = time() ] )` |
|      - | 8134 | ` *  Format a local time/date (PLATFORM INDEPENDANT IMPLEENTATION NOT BASED ON LOCALE)` |
|      - | 8135 | ` * Parameters` |
|      - | 8136 | ` *  $format` |
|      - | 8137 | ` *   The format of the outputted date string (See code above)` |
|      - | 8138 | ` * $timestamp` |
|      - | 8139 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|      - | 8140 | ` *   that defaults to the current local time if a timestamp is not given.` |
|      - | 8141 | ` *   In other words, it defaults to the value of time().` |
|      - | 8142 | ` * Return` |
|      - | 8143 | ` * Returns a string formatted according format using the given timestamp` |
|      - | 8144 | ` * or the current local time if no timestamp is given.` |
|      - | 8145 | ` */` |
|     20 | 8146 | `static int PH7_builtin_strftime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8147 |  |
|      - | 8148 | `	const char *zFormat;` |
|      - | 8149 | `	int nLen;` |
|      - | 8150 | `	Sytm sTm;` |
|     21 | 8151 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8152 | `		/* Missing/Invalid argument,return FALSE */` |
|      5 | 8153 | `		ph7_result_bool(pCtx,0);` |
|      5 | 8154 | `		return PH7_OK;` |
|      - | 8155 | `	}` |
|     17 | 8156 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8157 | `	if( nLen < 1 ){` |
|      - | 8158 | `		/* Don't bother processing return FALSE */` |
|    ! 0 | 8159 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8160 | `	}` |
|     17 | 8161 | `	if( nArg < 2 ){` |
|      - | 8162 | `#ifdef __WINNT__` |
|      - | 8163 | `		SYSTEMTIME sOS;` |
|      1 | 8164 | `		GetSystemTime(&sOS);` |
|      1 | 8165 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8166 | `#else` |
|      - | 8167 | `		struct tm *pTm;` |
|      - | 8168 | `		time_t t;` |
|     14 | 8169 | `		time(&t);` |
|     14 | 8170 | `		pTm = localtime(&t);` |
|     14 | 8171 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8172 | `#endif` |
|      8 | 8173 | `	}else{` |
|      - | 8174 | `		/* Use the given timestamp */` |
|      - | 8175 | `		time_t t;` |
|      - | 8176 | `		struct tm *pTm;` |
|      3 | 8177 | `		if( ph7_value_is_int(apArg[1]) ){` |
|      3 | 8178 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|      3 | 8179 | `			pTm = localtime(&t);` |
|      3 | 8180 | `			if( pTm == 0 ){` |
|    ! 0 | 8181 | `				time(&t);` |
|    ! 0 | 8182 | `			}` |
|      2 | 8183 | `		}else{` |
|    ! 0 | 8184 | `			time(&t);` |
|      - | 8185 | `		}` |
|      3 | 8186 | `		pTm = localtime(&t);` |
|      3 | 8187 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8188 | `	}` |
|      - | 8189 | `	/* Format the given string */` |
|     17 | 8190 | `	PH7_Strftime(pCtx,zFormat,nLen,&sTm);` |
|     17 | 8191 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|      - | 8192 | `		/* Nothing was formatted,return FALSE */` |
|    ! 0 | 8193 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8194 | `	}` |
|     17 | 8195 | `	return PH7_OK;` |
|     11 | 8196 |  |
|      - | 8197 | `/*` |
|      - | 8198 | ` * string gmdate(string $format [, int $timestamp = time() ] )` |
|      - | 8199 | ` *  Identical to the date() function except that the time returned` |
|      - | 8200 | ` *  is Greenwich Mean Time (GMT).` |
|      - | 8201 | ` * Parameters` |
|      - | 8202 | ` *  $format` |
|      - | 8203 | ` *  The format of the outputted date string (See code above)` |
|      - | 8204 | ` *  $timestamp` |
|      - | 8205 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|      - | 8206 | ` *   that defaults to the current local time if a timestamp is not given.` |
|      - | 8207 | ` *   In other words, it defaults to the value of time().` |
|      - | 8208 | ` * Return` |
|      - | 8209 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|      - | 8210 | ` */` |
|     16 | 8211 | `static int PH7_builtin_gmdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8212 |  |
|      - | 8213 | `	const char *zFormat;` |
|      - | 8214 | `	int nLen;` |
|      - | 8215 | `	Sytm sTm;` |
|     17 | 8216 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8217 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 | 8218 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8219 | `		return PH7_OK;` |
|      - | 8220 | `	}` |
|     15 | 8221 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 8222 | `	if( nLen < 1 ){` |
|      - | 8223 | `		/* Don't bother processing return the empty string */` |
|    ! 0 | 8224 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 8225 | `	}` |
|     15 | 8226 | `	if( nArg < 2 ){` |
|      - | 8227 | `#ifdef __WINNT__` |
|      - | 8228 | `		SYSTEMTIME sOS;` |
|      1 | 8229 | `		GetSystemTime(&sOS);` |
|      1 | 8230 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8231 | `#else` |
|      - | 8232 | `		struct tm *pTm;` |
|      - | 8233 | `		time_t t;` |
|     12 | 8234 | `		time(&t);` |
|     12 | 8235 | `		pTm = gmtime(&t);` |
|     12 | 8236 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8237 | `#endif` |
|      7 | 8238 | `	}else{` |
|      - | 8239 | `		/* Use the given timestamp */` |
|      - | 8240 | `		time_t t;` |
|      - | 8241 | `		struct tm *pTm;` |
|      3 | 8242 | `		if( ph7_value_is_int(apArg[1]) ){` |
|      3 | 8243 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|      3 | 8244 | `			pTm = gmtime(&t);` |
|      3 | 8245 | `			if( pTm == 0 ){` |
|    ! 0 | 8246 | `				time(&t);` |
|    ! 0 | 8247 | `			}` |
|      2 | 8248 | `		}else{` |
|    ! 0 | 8249 | `			time(&t);` |
|      - | 8250 | `		}` |
|      3 | 8251 | `		pTm = gmtime(&t);` |
|      3 | 8252 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8253 | `	}` |
|      - | 8254 | `	/* Format the given string */` |
|     15 | 8255 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|     15 | 8256 | `	return PH7_OK;` |
|      9 | 8257 |  |
|      - | 8258 | `/*` |
|      - | 8259 | ` * array localtime([ int $timestamp = time() [, bool $is_associative = false ]])` |
|      - | 8260 | ` *  Return the local time.` |
|      - | 8261 | ` * Parameter` |
|      - | 8262 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|      - | 8263 | ` *     that defaults to the current local time if a timestamp is not given.` |
|      - | 8264 | ` *     In other words, it defaults to the value of time().` |
|      - | 8265 | ` * $is_associative` |
|      - | 8266 | ` *   If set to FALSE or not supplied then the array is returned as a regular, numerically` |
|      - | 8267 | ` *   indexed array. If the argument is set to TRUE then localtime() returns an associative` |
|      - | 8268 | ` *   array containing all the different elements of the structure returned by the C function` |
|      - | 8269 | ` *   call to localtime. The names of the different keys of the associative array are as follows:` |
|      - | 8270 | ` *      "tm_sec" - seconds, 0 to 59` |
|      - | 8271 | ` *      "tm_min" - minutes, 0 to 59` |
|      - | 8272 | ` *      "tm_hour" - hours, 0 to 23` |
|      - | 8273 | ` *      "tm_mday" - day of the month, 1 to 31` |
|      - | 8274 | ` *      "tm_mon" - month of the year, 0 (Jan) to 11 (Dec)` |
|      - | 8275 | ` *      "tm_year" - years since 1900` |
|      - | 8276 | ` *      "tm_wday" - day of the week, 0 (Sun) to 6 (Sat)` |
|      - | 8277 | ` *      "tm_yday" - day of the year, 0 to 365` |
|      - | 8278 | ` *      "tm_isdst" - is daylight savings time in effect? Positive if yes, 0 if not, negative if unknown.` |
|      - | 8279 | ` * Returns` |
|      - | 8280 | ` *  An associative array of information related to the timestamp.` |
|      - | 8281 | ` */` |
|      8 | 8282 | `static int PH7_builtin_localtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8283 |  |
|      - | 8284 | `	ph7_value *pValue,*pArray;` |
|      9 | 8285 | `	int isAssoc = 0;` |
|      - | 8286 | `	Sytm sTm;` |
|      9 | 8287 | `	if( nArg < 1 ){` |
|      - | 8288 | `#ifdef __WINNT__` |
|      - | 8289 | `		SYSTEMTIME sOS;` |
|      1 | 8290 | `		GetSystemTime(&sOS); /* TODO(chems): GMT not local */` |
|      1 | 8291 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8292 | `#else` |
|      - | 8293 | `		struct tm *pTm;` |
|      - | 8294 | `		time_t t;` |
|      4 | 8295 | `		time(&t);` |
|      4 | 8296 | `		pTm = localtime(&t);` |
|      4 | 8297 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8298 | `#endif` |
|      3 | 8299 | `	}else{` |
|      - | 8300 | `		/* Use the given timestamp */` |
|      - | 8301 | `		time_t t;` |
|      - | 8302 | `		struct tm *pTm;` |
|      5 | 8303 | `		if( ph7_value_is_int(apArg[0]) ){` |
|      5 | 8304 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|      5 | 8305 | `			pTm = localtime(&t);` |
|      5 | 8306 | `			if( pTm == 0 ){` |
|    ! 0 | 8307 | `				time(&t);` |
|    ! 0 | 8308 | `			}` |
|      3 | 8309 | `		}else{` |
|    ! 0 | 8310 | `			time(&t);` |
|      - | 8311 | `		}` |
|      5 | 8312 | `		pTm = localtime(&t);` |
|      5 | 8313 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8314 | `	}` |
|      - | 8315 | `	/* Element value */` |
|      9 | 8316 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      9 | 8317 | `	if( pValue == 0 ){` |
|      - | 8318 | `		/* Return NULL */` |
|    ! 0 | 8319 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8320 | `		return PH7_OK;` |
|      - | 8321 | `	}` |
|      - | 8322 | `	/* Create a new array */` |
|      9 | 8323 | `	pArray = ph7_context_new_array(pCtx);` |
|      9 | 8324 | `	if( pArray == 0 ){` |
|      - | 8325 | `		/* Return NULL */` |
|    ! 0 | 8326 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8327 | `		return PH7_OK;` |
|      - | 8328 | `	}` |
|      9 | 8329 | `	if( nArg > 1 ){` |
|      3 | 8330 | `		isAssoc = ph7_value_to_bool(apArg[1]);` |
|      1 | 8331 | `	}` |
|      - | 8332 | `	/* Fill the array */` |
|      - | 8333 | `	/* Seconds */` |
|      9 | 8334 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|      9 | 8335 | `	if( isAssoc ){` |
|      3 | 8336 | `		ph7_array_add_strkey_elem(pArray,"tm_sec",pValue);` |
|      2 | 8337 | `	}else{` |
|      7 | 8338 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8339 | `	}` |
|      - | 8340 | `	/* Minutes */` |
|      9 | 8341 | `	ph7_value_int(pValue,sTm.tm_min);` |
|      9 | 8342 | `	if( isAssoc ){` |
|      3 | 8343 | `		ph7_array_add_strkey_elem(pArray,"tm_min",pValue);` |
|      2 | 8344 | `	}else{` |
|      7 | 8345 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8346 | `	}` |
|      - | 8347 | `	/* Hours */` |
|      9 | 8348 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|      9 | 8349 | `	if( isAssoc ){` |
|      3 | 8350 | `		ph7_array_add_strkey_elem(pArray,"tm_hour",pValue);` |
|      2 | 8351 | `	}else{` |
|      7 | 8352 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8353 | `	}` |
|      - | 8354 | `	/* mday */` |
|      9 | 8355 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|      9 | 8356 | `	if( isAssoc ){` |
|      3 | 8357 | `		ph7_array_add_strkey_elem(pArray,"tm_mday",pValue);` |
|      2 | 8358 | `	}else{` |
|      7 | 8359 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8360 | `	}` |
|      - | 8361 | `	/* mon */` |
|      9 | 8362 | `	ph7_value_int(pValue,sTm.tm_mon);` |
|      9 | 8363 | `	if( isAssoc ){` |
|      3 | 8364 | `		ph7_array_add_strkey_elem(pArray,"tm_mon",pValue);` |
|      2 | 8365 | `	}else{` |
|      7 | 8366 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8367 | `	}` |
|      - | 8368 | `	/* year since 1900 */` |
|      9 | 8369 | `	ph7_value_int(pValue,sTm.tm_year-1900);` |
|      9 | 8370 | `	if( isAssoc ){` |
|      3 | 8371 | `		ph7_array_add_strkey_elem(pArray,"tm_year",pValue);` |
|      2 | 8372 | `	}else{` |
|      7 | 8373 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8374 | `	}` |
|      - | 8375 | `	/* wday */` |
|      9 | 8376 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|      9 | 8377 | `	if( isAssoc ){` |
|      3 | 8378 | `		ph7_array_add_strkey_elem(pArray,"tm_wday",pValue);` |
|      2 | 8379 | `	}else{` |
|      7 | 8380 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8381 | `	}` |
|      - | 8382 | `	/* yday */` |
|      9 | 8383 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|      9 | 8384 | `	if( isAssoc ){` |
|      3 | 8385 | `		ph7_array_add_strkey_elem(pArray,"tm_yday",pValue);` |
|      2 | 8386 | `	}else{` |
|      7 | 8387 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8388 | `	}` |
|      - | 8389 | `	/* isdst */` |
|      - | 8390 | `#ifdef __WINNT__` |
|      - | 8391 | `#ifdef _MSC_VER` |
|      - | 8392 | `#ifndef _WIN32_WCE` |
|      1 | 8393 | `			_get_daylight(&sTm.tm_isdst);` |
|      - | 8394 | `#endif` |
|      - | 8395 | `#endif` |
|      - | 8396 | `#endif` |
|      9 | 8397 | `	ph7_value_int(pValue,sTm.tm_isdst);` |
|      9 | 8398 | `	if( isAssoc ){` |
|      3 | 8399 | `		ph7_array_add_strkey_elem(pArray,"tm_isdst",pValue);` |
|      2 | 8400 | `	}else{` |
|      7 | 8401 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8402 | `	}` |
|      - | 8403 | `	/* Return the array */` |
|      9 | 8404 | `	ph7_result_value(pCtx,pArray);` |
|      9 | 8405 | `	return PH7_OK;` |
|      5 | 8406 |  |
|      - | 8407 | `/*` |
|      - | 8408 | ` * int idate(string $format [, int $timestamp = time() ])` |
|      - | 8409 | ` *  Returns a number formatted according to the given format string` |
|      - | 8410 | ` *  using the given integer timestamp or the current local time if` |
|      - | 8411 | ` *  no timestamp is given. In other words, timestamp is optional and defaults` |
|      - | 8412 | ` *  to the value of time().` |
|      - | 8413 | ` *  Unlike the function date(), idate() accepts just one char in the format` |
|      - | 8414 | ` *  parameter.` |
|      - | 8415 | ` * $Parameters` |
|      - | 8416 | ` *  Supported format` |
|      - | 8417 | ` *   d 	Day of the month` |
|      - | 8418 | ` *   h 	Hour (12 hour format)` |
|      - | 8419 | ` *   H 	Hour (24 hour format)` |
|      - | 8420 | ` *   i 	Minutes` |
|      - | 8421 | ` *   I (uppercase i)1 if DST is activated, 0 otherwise` |
|      - | 8422 | ` *   L (uppercase l) returns 1 for leap year, 0 otherwise` |
|      - | 8423 | ` *   m 	Month number` |
|      - | 8424 | ` *   s 	Seconds` |
|      - | 8425 | ` *   t 	Days in current month` |
|      - | 8426 | ` *   U 	Seconds since the Unix Epoch - January 1 1970 00:00:00 UTC - this is the same as time()` |
|      - | 8427 | ` *   w 	Day of the week (0 on Sunday)` |
|      - | 8428 | ` *   W 	ISO-8601 week number of year, weeks starting on Monday` |
|      - | 8429 | ` *   y 	Year (1 or 2 digits - check note below)` |
|      - | 8430 | ` *   Y 	Year (4 digits)` |
|      - | 8431 | ` *   z 	Day of the year` |
|      - | 8432 | ` *   Z 	Timezone offset in seconds` |
|      - | 8433 | ` * $timestamp` |
|      - | 8434 | ` *  The optional timestamp parameter is an integer Unix timestamp that defaults` |
|      - | 8435 | ` *  to the current local time if a timestamp is not given. In other words, it defaults` |
|      - | 8436 | ` *  to the value of time().` |
|      - | 8437 | ` * Return` |
|      - | 8438 | ` *  An integer.` |
|      - | 8439 | ` */` |
|     42 | 8440 | `static int PH7_builtin_idate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 8441 |  |
|      - | 8442 | `	const char *zFormat;` |
|     44 | 8443 | `	ph7_int64 iVal = 0;` |
|      - | 8444 | `	int nLen;` |
|      - | 8445 | `	Sytm sTm;` |
|     44 | 8446 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8447 | `		/* Missing/Invalid argument,return -1 */` |
|      5 | 8448 | `		ph7_result_int(pCtx,-1);` |
|      5 | 8449 | `		return PH7_OK;` |
|      - | 8450 | `	}` |
|     40 | 8451 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     40 | 8452 | `	if( nLen < 1 ){` |
|      - | 8453 | `		/* Don't bother processing return -1*/` |
|    ! 0 | 8454 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 8455 | `	}` |
|     40 | 8456 | `	if( nArg < 2 ){` |
|      - | 8457 | `#ifdef __WINNT__` |
|      - | 8458 | `		SYSTEMTIME sOS;` |
|      2 | 8459 | `		GetSystemTime(&sOS);` |
|      2 | 8460 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8461 | `#else` |
|      - | 8462 | `		struct tm *pTm;` |
|      - | 8463 | `		time_t t;` |
|     28 | 8464 | `		time(&t);` |
|     28 | 8465 | `		pTm = localtime(&t);` |
|     28 | 8466 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8467 | `#endif` |
|     16 | 8468 | `	}else{` |
|      - | 8469 | `		/* Use the given timestamp */` |
|      - | 8470 | `		time_t t;` |
|      - | 8471 | `		struct tm *pTm;` |
|     11 | 8472 | `		if( ph7_value_is_int(apArg[1]) ){` |
|     11 | 8473 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|     11 | 8474 | `			pTm = localtime(&t);` |
|     11 | 8475 | `			if( pTm == 0 ){` |
|    ! 0 | 8476 | `				time(&t);` |
|    ! 0 | 8477 | `			}` |
|      6 | 8478 | `		}else{` |
|    ! 0 | 8479 | `			time(&t);` |
|      - | 8480 | `		}` |
|     11 | 8481 | `		pTm = localtime(&t);` |
|     11 | 8482 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8483 | `	}` |
|      - | 8484 | `	/* Perform the requested operation */` |
|     40 | 8485 | `	switch(zFormat[0]){` |
|      2 | 8486 | `	case 'd':` |
|      - | 8487 | `		/* Day of the month */` |
|      5 | 8488 | `		iVal = sTm.tm_mday;` |
|      5 | 8489 | `		break;` |
|    ! 0 | 8490 | `	case 'h':` |
|      - | 8491 | `		/*	Hour (12 hour format)*/` |
|    ! 0 | 8492 | `		iVal = 1 + (sTm.tm_hour % 12);` |
|    ! 0 | 8493 | `		break;` |
|      1 | 8494 | `	case 'H':` |
|      - | 8495 | `		/* Hour (24 hour format)*/` |
|      3 | 8496 | `		iVal = sTm.tm_hour;` |
|      3 | 8497 | `		break;` |
|      1 | 8498 | `	case 'i':` |
|      - | 8499 | `		/*Minutes*/` |
|      3 | 8500 | `		iVal = sTm.tm_min;` |
|      3 | 8501 | `		break;` |
|      1 | 8502 | `	case 'I':` |
|      - | 8503 | `		/*	returns 1 if DST is activated, 0 otherwise */` |
|      - | 8504 | `#ifdef __WINNT__` |
|      - | 8505 | `#ifdef _MSC_VER` |
|      - | 8506 | `#ifndef _WIN32_WCE` |
|      1 | 8507 | `			_get_daylight(&sTm.tm_isdst);` |
|      - | 8508 | `#endif` |
|      - | 8509 | `#endif` |
|      - | 8510 | `#endif` |
|      3 | 8511 | `		iVal = sTm.tm_isdst;` |
|      3 | 8512 | `		break;` |
|      1 | 8513 | `	case 'L':` |
|      - | 8514 | `		/* 	returns 1 for leap year, 0 otherwise */` |
|      3 | 8515 | `		iVal = IS_LEAP_YEAR(sTm.tm_year);` |
|      3 | 8516 | `		break;` |
|      2 | 8517 | `	case 'm':` |
|      - | 8518 | `		/* Month number*/` |
|      5 | 8519 | `		iVal = sTm.tm_mon;` |
|      5 | 8520 | `		break;` |
|      1 | 8521 | `	case 's':` |
|      - | 8522 | `		/*Seconds*/` |
|      3 | 8523 | `		iVal = sTm.tm_sec;` |
|      3 | 8524 | `		break;` |
|      1 | 8525 | `	case 't':{` |
|      - | 8526 | `		/*Days in current month*/` |
|      - | 8527 | `		static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|      3 | 8528 | `		int nDays = aMonDays[sTm.tm_mon % 12 ];` |
|      3 | 8529 | `		if( sTm.tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(sTm.tm_year) ){` |
|    ! 0 | 8530 | `			nDays = 28;` |
|    ! 0 | 8531 | `		}` |
|      3 | 8532 | `		iVal = nDays;` |
|      3 | 8533 | `		break;` |
|      - | 8534 | `			 }` |
|      1 | 8535 | `	case 'U':` |
|      - | 8536 | `		/*Seconds since the Unix Epoch*/` |
|      3 | 8537 | `		iVal = (ph7_int64)time(0);` |
|      3 | 8538 | `		break;` |
|      1 | 8539 | `	case 'w':` |
|      - | 8540 | `		/*	Day of the week (0 on Sunday) */` |
|      3 | 8541 | `		iVal = sTm.tm_wday;` |
|      3 | 8542 | `		break;` |
|      1 | 8543 | `	case 'W': {` |
|      - | 8544 | `		/* ISO-8601 week number of year, weeks starting on Monday */` |
|      - | 8545 | `		static const int aISO8601_local[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|      3 | 8546 | `		iVal = aISO8601_local[sTm.tm_wday % 7 ];` |
|      3 | 8547 | `		break;` |
|      - | 8548 | `			  }` |
|    ! 0 | 8549 | `	case 'y':` |
|      - | 8550 | `		/* Year (2 digits) */` |
|    ! 0 | 8551 | `		iVal = sTm.tm_year % 100;` |
|    ! 0 | 8552 | `		break;` |
|      3 | 8553 | `	case 'Y':` |
|      - | 8554 | `		/* Year (4 digits) */` |
|      7 | 8555 | `		iVal = sTm.tm_year;` |
|      7 | 8556 | `		break;` |
|      1 | 8557 | `	case 'z':` |
|      - | 8558 | `		/* Day of the year */` |
|      3 | 8559 | `		iVal = sTm.tm_yday;` |
|      3 | 8560 | `		break;` |
|      1 | 8561 | `	case 'Z':` |
|      - | 8562 | `		/*Timezone offset in seconds*/` |
|      3 | 8563 | `		iVal = sTm.tm_gmtoff;` |
|      3 | 8564 | `		break;` |
|      1 | 8565 | `	default:` |
|      - | 8566 | `		/* unknown format,throw a warning */` |
|      3 | 8567 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Unknown date format token");` |
|      2 | 8568 | `		break;` |
|      - | 8569 | `	}` |
|      - | 8570 | `	/* Return the time value */` |
|     40 | 8571 | `	ph7_result_int64(pCtx,iVal);` |
|     40 | 8572 | `	return PH7_OK;` |
|     23 | 8573 |  |
|      - | 8574 | `/*` |
|      - | 8575 | ` * int mktime/gmmktime([ int $hour = date("H") [, int $minute = date("i") [, int $second = date("s")` |
|      - | 8576 | ` *  [, int $month = date("n") [, int $day = date("j") [, int $year = date("Y") [, int $is_dst = -1 ]]]]]]] )` |
|      - | 8577 | ` *  Returns the Unix timestamp corresponding to the arguments given. This timestamp is a 64bit integer` |
|      - | 8578 | ` *  containing the number of seconds between the Unix Epoch (January 1 1970 00:00:00 GMT) and the time` |
|      - | 8579 | ` *  specified.` |
|      - | 8580 | ` *  Arguments may be left out in order from right to left; any arguments thus omitted will be set to` |
|      - | 8581 | ` *  the current value according to the local date and time.` |
|      - | 8582 | ` * Parameters` |
|      - | 8583 | ` * $hour` |
|      - | 8584 | ` *  The number of the hour relevant to the start of the day determined by month, day and year.` |
|      - | 8585 | ` *  Negative values reference the hour before midnight of the day in question. Values greater` |
|      - | 8586 | ` *  than 23 reference the appropriate hour in the following day(s).` |
|      - | 8587 | ` * $minute` |
|      - | 8588 | ` *  The number of the minute relevant to the start of the hour. Negative values reference` |
|      - | 8589 | ` *  the minute in the previous hour. Values greater than 59 reference the appropriate minute` |
|      - | 8590 | ` *  in the following hour(s).` |
|      - | 8591 | ` * $second` |
|      - | 8592 | ` *  The number of seconds relevant to the start of the minute. Negative values reference` |
|      - | 8593 | ` *  the second in the previous minute. Values greater than 59 reference the appropriate` |
|      - | 8594 | ` * second in the following minute(s).` |
|      - | 8595 | ` * $month` |
|      - | 8596 | ` *  The number of the month relevant to the end of the previous year. Values 1 to 12 reference` |
|      - | 8597 | ` *  the normal calendar months of the year in question. Values less than 1 (including negative values)` |
|      - | 8598 | ` *  reference the months in the previous year in reverse order, so 0 is December, -1 is November)...` |
|      - | 8599 | ` * $day` |
|      - | 8600 | ` *  The number of the day relevant to the end of the previous month. Values 1 to 28, 29, 30 or 31` |
|      - | 8601 | ` *  (depending upon the month) reference the normal days in the relevant month. Values less than 1` |
|      - | 8602 | ` *  (including negative values) reference the days in the previous month, so 0 is the last day` |
|      - | 8603 | ` *  of the previous month, -1 is the day before that, etc. Values greater than the number of days` |
|      - | 8604 | ` *  in the relevant month reference the appropriate day in the following month(s).` |
|      - | 8605 | ` * $year` |
|      - | 8606 | ` *  The number of the year, may be a two or four digit value, with values between 0-69 mapping` |
|      - | 8607 | ` *  to 2000-2069 and 70-100 to 1970-2000. On systems where time_t is a 32bit signed integer, as` |
|      - | 8608 | ` *  most common today, the valid range for year is somewhere between 1901 and 2038.` |
|      - | 8609 | ` * $is_dst` |
|      - | 8610 | ` *  This parameter can be set to 1 if the time is during daylight savings time (DST), 0 if it is not,` |
|      - | 8611 | ` *  or -1 (the default) if it is unknown whether the time is within daylight savings time or not.` |
|      - | 8612 | ` * Return` |
|      - | 8613 | ` *   mktime() returns the Unix timestamp of the arguments given.` |
|      - | 8614 | ` *   If the arguments are invalid, the function returns FALSE` |
|      - | 8615 | ` */` |
|      8 | 8616 | `static int PH7_builtin_mktime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8617 |  |
|      - | 8618 | `	const char *zFunction;` |
|      9 | 8619 | `	ph7_int64 iVal = 0;` |
|      - | 8620 | `	struct tm *pTm;` |
|      - | 8621 | `	time_t t;` |
|      - | 8622 | `	/* Extract function name */` |
|      9 | 8623 | `	zFunction = ph7_function_name(pCtx);` |
|      - | 8624 | `	/* Get the current time */` |
|      9 | 8625 | `	time(&t);` |
|      9 | 8626 | `	if( zFunction[0] == 'g' /* gmmktime */ ){` |
|      3 | 8627 | `		pTm = gmtime(&t);` |
|      2 | 8628 | `	}else{` |
|      - | 8629 | `		/* localtime */` |
|      7 | 8630 | `		pTm = localtime(&t);` |
|      - | 8631 | `	}` |
|      9 | 8632 | `	if( nArg > 0 ){` |
|      - | 8633 | `		int iTmp;` |
|      - | 8634 | `		/* Hour */` |
|      9 | 8635 | `		iTmp = ph7_value_to_int(apArg[0]);` |
|      9 | 8636 | `		pTm->tm_hour = iTmp;` |
|      9 | 8637 | `		if( nArg > 1 ){` |
|      - | 8638 | `			/* Minutes */` |
|      9 | 8639 | `			iTmp = ph7_value_to_int(apArg[1]);` |
|      9 | 8640 | `			pTm->tm_min = iTmp;` |
|      9 | 8641 | `			if( nArg > 2 ){` |
|      - | 8642 | `				/* Seconds */` |
|      9 | 8643 | `				iTmp = ph7_value_to_int(apArg[2]);` |
|      9 | 8644 | `				pTm->tm_sec = iTmp;` |
|      9 | 8645 | `				if( nArg > 3 ){` |
|      - | 8646 | `					/* Month */` |
|      9 | 8647 | `					iTmp = ph7_value_to_int(apArg[3]);` |
|      9 | 8648 | `					pTm->tm_mon = iTmp - 1;` |
|      9 | 8649 | `					if( nArg > 4 ){` |
|      - | 8650 | `						/* mday */` |
|      9 | 8651 | `						iTmp = ph7_value_to_int(apArg[4]);` |
|      9 | 8652 | `						pTm->tm_mday = iTmp;` |
|      9 | 8653 | `						if( nArg > 5 ){` |
|      - | 8654 | `							/* Year */` |
|      9 | 8655 | `							iTmp = ph7_value_to_int(apArg[5]);` |
|      9 | 8656 | `							if( iTmp > 1900 ){` |
|      9 | 8657 | `								iTmp -= 1900;` |
|      4 | 8658 | `							}` |
|      9 | 8659 | `							pTm->tm_year = iTmp;` |
|      9 | 8660 | `							if( nArg > 6 ){` |
|      - | 8661 | `								/* is_dst */` |
|    ! 0 | 8662 | `								iTmp = ph7_value_to_bool(apArg[6]);` |
|    ! 0 | 8663 | `								pTm->tm_isdst = iTmp;` |
|    ! 0 | 8664 | `							}` |
|      4 | 8665 | `						}` |
|      4 | 8666 | `					}` |
|      4 | 8667 | `				}` |
|      4 | 8668 | `			}` |
|      4 | 8669 | `		}` |
|      4 | 8670 | `	}` |
|      - | 8671 | `	/* Make the time */` |
|      9 | 8672 | `	iVal = (ph7_int64)mktime(pTm);` |
|      - | 8673 | `	/* Return the timesatmp as a 64bit integer */` |
|      9 | 8674 | `	ph7_result_int64(pCtx,iVal);` |
|      9 | 8675 | `	return PH7_OK;` |
|      1 | 8676 |  |
|      - | 8677 | `/*` |
|      - | 8678 | ` * Section:` |
|      - | 8679 | ` *    URL handling Functions.` |
|      - | 8680 | ` * Status:` |
|      - | 8681 | ` *    Stable.` |
|      - | 8682 | ` */` |
|      - | 8683 | `/*` |
|      - | 8684 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 8685 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 8686 | ` */` |
|   1026 | 8687 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 8688 |  |
|      - | 8689 | `	/* Store in the call context result buffer */` |
|   1028 | 8690 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 8691 | `	return SXRET_OK;` |
|      2 | 8692 |  |
|      - | 8693 | `/*` |
|      - | 8694 | ` * string base64_encode(string $data)` |
|      - | 8695 | ` * string convert_uuencode(string $data)` |
|      - | 8696 | ` *  Encodes data with MIME base64` |
|      - | 8697 | ` * Parameter` |
|      - | 8698 | ` *  $data` |
|      - | 8699 | ` *    Data to encode` |
|      - | 8700 | ` * Return` |
|      - | 8701 | ` *  Encoded data or FALSE on failure.` |
|      - | 8702 | ` */` |
|     10 | 8703 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8704 |  |
|      - | 8705 | `	const char *zIn;` |
|      - | 8706 | `	int nLen;` |
|     11 | 8707 | `	if( nArg < 1 ){` |
|      - | 8708 | `		/* Missing arguments,return FALSE */` |
|      5 | 8709 | `		ph7_result_bool(pCtx,0);` |
|      5 | 8710 | `		return PH7_OK;` |
|      - | 8711 | `	}` |
|      - | 8712 | `	/* Extract the input string */` |
|      7 | 8713 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8714 | `	if( nLen < 1 ){` |
|      - | 8715 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8716 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8717 | `		return PH7_OK;` |
|      - | 8718 | `	}` |
|      - | 8719 | `	/* Perform the BASE64 encoding */` |
|      7 | 8720 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 8721 | `	return PH7_OK;` |
|      6 | 8722 |  |
|      - | 8723 | `/*` |
|      - | 8724 | ` * string base64_decode(string $data)` |
|      - | 8725 | ` * string convert_uudecode(string $data)` |
|      - | 8726 | ` *  Decodes data encoded with MIME base64` |
|      - | 8727 | ` * Parameter` |
|      - | 8728 | ` *  $data` |
|      - | 8729 | ` *    Encoded data.` |
|      - | 8730 | ` * Return` |
|      - | 8731 | ` *  Returns the original data or FALSE on failure.` |
|      - | 8732 | ` */` |
|     36 | 8733 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 8734 |  |
|      - | 8735 | `	const char *zIn;` |
|      - | 8736 | `	int nLen;` |
|     38 | 8737 | `	if( nArg < 1 ){` |
|      - | 8738 | `		/* Missing arguments,return FALSE */` |
|      3 | 8739 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8740 | `		return PH7_OK;` |
|      - | 8741 | `	}` |
|      - | 8742 | `	/* Extract the input string */` |
|     36 | 8743 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 8744 | `	if( nLen < 1 ){` |
|      - | 8745 | `		/* Nothing to process,return FALSE */` |
|      3 | 8746 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8747 | `		return PH7_OK;` |
|      - | 8748 | `	}` |
|      - | 8749 | `	/* Perform the BASE64 decoding */` |
|     34 | 8750 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 8751 | `	return PH7_OK;` |
|     20 | 8752 |  |
|      - | 8753 | `/*` |
|      - | 8754 | ` * string urlencode(string $str)` |
|      - | 8755 | ` *  URL encoding` |
|      - | 8756 | ` * Parameter` |
|      - | 8757 | ` *  $data` |
|      - | 8758 | ` *   Input string.` |
|      - | 8759 | ` * Return` |
|      - | 8760 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 8761 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 8762 | ` *  encoded as plus (+) signs.` |
|      - | 8763 | ` */` |
|      6 | 8764 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8765 |  |
|      - | 8766 | `	const char *zIn;` |
|      - | 8767 | `	int nLen;` |
|      7 | 8768 | `	if( nArg < 1 ){` |
|      - | 8769 | `		/* Missing arguments,return FALSE */` |
|      3 | 8770 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8771 | `		return PH7_OK;` |
|      - | 8772 | `	}` |
|      - | 8773 | `	/* Extract the input string */` |
|      5 | 8774 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 8775 | `	if( nLen < 1 ){` |
|      - | 8776 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8777 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8778 | `		return PH7_OK;` |
|      - | 8779 | `	}` |
|      - | 8780 | `	/* Perform the URL encoding */` |
|      5 | 8781 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 8782 | `	return PH7_OK;` |
|      4 | 8783 |  |
|      - | 8784 | `/*` |
|      - | 8785 | ` * string urldecode(string $str)` |
|      - | 8786 | ` *  Decodes any %## encoding in the given string.` |
|      - | 8787 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 8788 | ` * Parameter` |
|      - | 8789 | ` *  $data` |
|      - | 8790 | ` *    Input string.` |
|      - | 8791 | ` * Return` |
|      - | 8792 | ` *  Decoded URL or FALSE on failure.` |
|      - | 8793 | ` */` |
|      8 | 8794 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8795 |  |
|      - | 8796 | `	const char *zIn;` |
|      - | 8797 | `	int nLen;` |
|      9 | 8798 | `	if( nArg < 1 ){` |
|      - | 8799 | `		/* Missing arguments,return FALSE */` |
|      3 | 8800 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8801 | `		return PH7_OK;` |
|      - | 8802 | `	}` |
|      - | 8803 | `	/* Extract the input string */` |
|      7 | 8804 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8805 | `	if( nLen < 1 ){` |
|      - | 8806 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8807 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8808 | `		return PH7_OK;` |
|      - | 8809 | `	}` |
|      - | 8810 | `	/* Perform the URL decoding */` |
|      7 | 8811 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 8812 | `	return PH7_OK;` |
|      5 | 8813 |  |
|      - | 8814 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8815 | `/* Table of the built-in functions */` |
|      - | 8816 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 8817 | `	   /* Variable handling functions */` |
|      - | 8818 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 8819 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 8820 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 8821 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 8822 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 8823 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 8824 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 8825 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 8826 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 8827 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 8828 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 8829 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 8830 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 8831 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 8832 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 8833 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 8834 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 8835 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 8836 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 8837 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8838 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 8839 | `	   /* Math functions */` |
|      - | 8840 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 8841 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 8842 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 8843 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 8844 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 8845 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 8846 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 8847 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 8848 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 8849 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 8850 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 8851 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 8852 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 8853 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 8854 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 8855 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 8856 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 8857 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 8858 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 8859 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 8860 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 8861 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 8862 | `	{ "round",    PH7_builtin_round        },` |
|      - | 8863 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 8864 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 8865 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 8866 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 8867 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 8868 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 8869 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 8870 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 8871 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8872 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8873 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 8874 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8875 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8876 | `	   /* String handling functions */` |
|      - | 8877 |  |
|      - | 8878 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 8879 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 8880 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 8881 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 8882 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 8883 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 8884 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 8885 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 8886 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 8887 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 8888 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 8889 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 8890 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 8891 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 8892 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 8893 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 8894 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 8895 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 8896 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 8897 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 8898 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 8899 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 8900 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 8901 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 8902 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 8903 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 8904 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 8905 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 8906 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 8907 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 8908 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 8909 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 8910 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 8911 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 8912 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 8913 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 8914 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 8915 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 8916 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 8917 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 8918 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 8919 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 8920 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 8921 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 8922 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 8923 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 8924 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 8925 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 8926 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 8927 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8928 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8929 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 8930 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 8931 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 8932 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 8933 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8934 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8935 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 8936 |  |
|      - | 8937 |  |
|      - | 8938 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 8939 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 8940 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 8941 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 8942 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 8943 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8944 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8945 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 8946 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 8947 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8948 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8949 |  |
|      - | 8950 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 8951 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 8952 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 8953 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 8954 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 8955 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 8956 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 8957 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 8958 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 8959 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 8960 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 8961 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 8962 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8963 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8964 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 8965 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8966 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8967 |  |
|      - | 8968 | `	         /* Ctype functions */` |
|      - | 8969 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 8970 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 8971 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 8972 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 8973 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 8974 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 8975 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 8976 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 8977 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 8978 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 8979 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 8980 | `	         /* Time functions */` |
|      - | 8981 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 8982 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 8983 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 8984 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 8985 | `	{ "date",        PH7_builtin_date         },` |
|      - | 8986 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 8987 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 8988 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 8989 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 8990 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 8991 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 8992 | `	        /* URL functions */` |
|      - | 8993 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 8994 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 8995 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 8996 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 8997 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 8998 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 8999 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 9000 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 9001 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9002 | `};` |
|      - | 9003 | `/*` |
|      - | 9004 | ` * Register the built-in functions defined above,the array functions` |
|      - | 9005 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 9006 | ` */` |
|   1320 | 9007 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      2 | 9008 |  |
|      - | 9009 | `	sxu32 n;` |
| 201962 | 9010 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 200642 | 9011 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 100322 | 9012 | `	}` |
|      - | 9013 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   1322 | 9014 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 9015 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   1322 | 9016 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   1322 | 9017 |  |
|      - | 9018 |  |
