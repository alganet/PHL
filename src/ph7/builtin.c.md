# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3739/4389 lines (85.19%)

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
|     24 |   98 | `static int PH7_builtin_is_null(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   99 |  |
|     25 |  100 | `	int res = 0; /* Assume false by default */` |
|     25 |  101 | `	if( nArg > 0 ){` |
|     23 |  102 | `		res = ph7_value_is_null(apArg[0]);` |
|     11 |  103 | `	}` |
|      - |  104 | `	/* Query result */` |
|     25 |  105 | `	ph7_result_bool(pCtx,res);` |
|     25 |  106 | `	return PH7_OK;` |
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
|     94 |  152 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  153 |  |
|     96 |  154 | `	int res = 0; /* Assume false by default */` |
|     96 |  155 | `	if( nArg > 0 ){` |
|     94 |  156 | `		res = ph7_value_is_array(apArg[0]);` |
|     46 |  157 | `	}` |
|      - |  158 | `	/* Query result */` |
|     96 |  159 | `	ph7_result_bool(pCtx,res);` |
|     96 |  160 | `	return PH7_OK;` |
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
|  15546 |  269 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  270 |  |
|  15548 |  271 | `	int res = 1; /* Assume empty by default */` |
|  15548 |  272 | `	if( nArg > 0 ){` |
|  15546 |  273 | `		res = ph7_value_is_empty(apArg[0]);` |
|   7772 |  274 | `	}` |
|  15548 |  275 | `	ph7_result_bool(pCtx,res);` |
|  15548 |  276 | `	return PH7_OK;` |
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
|    100 |  711 | `static int PH7_builtin_abs(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  712 |  |
|      - |  713 | `	int is_float;` |
|      - |  714 | `	/* PHP requires exactly one argument. */` |
|    102 |  715 | `	if( nArg != 1 ){` |
|     11 |  716 | `		return PH7_VmThrowException(pCtx,` |
|      - |  717 | `			"ArgumentCountError",` |
|      - |  718 | `			"abs() expects exactly 1 argument, %d given",` |
|      3 |  719 | `			nArg` |
|      - |  720 | `			);` |
|      - |  721 | `	}` |
|      - |  722 |  |
|      - |  723 | `	/* Numeric strings with decimal/exponent are treated as real values. */` |
|     96 |  724 | `	is_float = ph7_value_is_float(apArg[0]);` |
|     96 |  725 | `	if( !is_float && ph7_value_is_string(apArg[0]) ){` |
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
|     94 |  741 | `	if( is_float ){` |
|      - |  742 | `		double r,x;` |
|     77 |  743 | `		x = ph7_value_to_double(apArg[0]);` |
|      - |  744 | `		/* Perform the requested operation */` |
|     77 |  745 | `		r = fabs(x);` |
|     77 |  746 | `		ph7_result_double(pCtx,r);` |
|     39 |  747 | `	}else{` |
|      - |  748 | `		int r,x;` |
|     18 |  749 | `		x = ph7_value_to_int(apArg[0]);` |
|      - |  750 | `		/* Perform the requested operation */` |
|     18 |  751 | `		r = abs(x);` |
|     18 |  752 | `		ph7_result_int(pCtx,r);` |
|      - |  753 | `	}` |
|     94 |  754 | `	return PH7_OK;` |
|     52 |  755 |  |
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
| 110240 | 1288 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1289 |  |
|      - | 1290 | `	const char *zSource,*zOfft;` |
|      - | 1291 | `	int nOfft,nLen,nSrcLen;` |
| 110242 | 1292 | `	if( nArg < 2 ){` |
|      - | 1293 | `		/* return FALSE */` |
|      5 | 1294 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1295 | `		return PH7_OK;` |
|      - | 1296 | `	}` |
|      - | 1297 | `	/* Extract the target string */` |
| 110238 | 1298 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 110238 | 1299 | `	if( nSrcLen < 1 ){` |
|      - | 1300 | `		/* Empty string,return FALSE */` |
|   6988 | 1301 | `		ph7_result_bool(pCtx,0);` |
|   6988 | 1302 | `		return PH7_OK;` |
|      - | 1303 | `	}` |
| 103252 | 1304 | `	nLen = nSrcLen; /* cc warning */` |
|      - | 1305 | `	/* Extract the offset */` |
| 103252 | 1306 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 103252 | 1307 | `	if( nOfft < 0 ){` |
|  17088 | 1308 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  17088 | 1309 | `		if( zOfft < zSource ){` |
|      - | 1310 | `			/* Invalid offset */` |
|      5 | 1311 | `			ph7_result_bool(pCtx,0);` |
|      5 | 1312 | `			return PH7_OK;` |
|      - | 1313 | `		}` |
|  17084 | 1314 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  17084 | 1315 | `		nOfft = (int)(zOfft-zSource);` |
|  94707 | 1316 | `	}else if( nOfft >= nSrcLen ){` |
|      - | 1317 | `		/* Invalid offset */` |
|      7 | 1318 | `		ph7_result_bool(pCtx,0);` |
|      7 | 1319 | `		return PH7_OK;` |
|    ! 0 | 1320 | `	}else{` |
|  86160 | 1321 | `		zOfft = &zSource[nOfft];` |
|  86160 | 1322 | `		nLen = nSrcLen - nOfft;` |
|      - | 1323 | `	}` |
| 103242 | 1324 | `	if( nArg > 2 ){` |
|      - | 1325 | `		/* Extract the length */` |
|  86158 | 1326 | `		nLen = ph7_value_to_int(apArg[2]);` |
|  86158 | 1327 | `		if( nLen == 0 ){` |
|      - | 1328 | `			/* Invalid length,return an empty string */` |
|      5 | 1329 | `			ph7_result_string(pCtx,"",0);` |
|      5 | 1330 | `			return PH7_OK;` |
|  86154 | 1331 | `		}else if( nLen < 0 ){` |
|  17086 | 1332 | `			nLen = nSrcLen + nLen - nOfft;` |
|  17086 | 1333 | `			if( nLen < 1 ){` |
|      - | 1334 | `				/* Invalid  length */` |
|      3 | 1335 | `				nLen = nSrcLen - nOfft;` |
|      1 | 1336 | `			}` |
|   8542 | 1337 | `		}` |
|  86154 | 1338 | `		if( nLen + nOfft > nSrcLen ){` |
|      - | 1339 | `			/* Invalid length */` |
|   2152 | 1340 | `			nLen = nSrcLen - nOfft;` |
|   1075 | 1341 | `		}` |
|  43076 | 1342 | `	}` |
|      - | 1343 | `	/* Return the substring */` |
| 103238 | 1344 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 103238 | 1345 | `	return PH7_OK;` |
|  55122 | 1346 |  |
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
|      - | 1607 | `	/* Reject explicit NULL right away.  With the compiler fix above,` |
|      - | 1608 | `	 * string literals (including the empty string) are represented as real` |
|      - | 1609 | `	 * strings, so this check won’t fire for "" anymore. */` |
|     22 | 1610 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      4 | 1611 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1612 | `			"TypeError",` |
|      - | 1613 | `			"addslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 1614 | `			ph7_type_name(apArg[0])` |
|      - | 1615 | `			);` |
|      - | 1616 | `	}` |
|      - | 1617 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     34 | 1618 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     26 | 1619 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     16 | 1620 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 1621 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1622 | `			"TypeError",` |
|      - | 1623 | `			"addslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 1624 | `			ph7_type_name(apArg[0])` |
|      - | 1625 | `			);` |
|      - | 1626 | `	}` |
|      - | 1627 | `	/* Convert to string representation first and obtain length. */` |
|     17 | 1628 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 1629 | `	if( nLen < 1 ){` |
|      - | 1630 | `		/* Return the empty string */` |
|      3 | 1631 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1632 | `		return PH7_OK;` |
|      - | 1633 | `	}` |
|     15 | 1634 | `	zEnd = &zIn[nLen];` |
|     15 | 1635 | `	zCur = 0; /* cc warning */` |
|     20 | 1636 | `	for(;;){` |
|     41 | 1637 | `		if( zIn >= zEnd ){` |
|      - | 1638 | `			/* No more input */` |
|     15 | 1639 | `			break;` |
|      - | 1640 | `		}` |
|     27 | 1641 | `		zCur = zIn;` |
|      - | 1642 | `		/* scan until a character that needs escaping (', ", \\, or NUL) */` |
|     89 | 1643 | `		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' && zIn[0] != '\0' ){` |
|     63 | 1644 | `			zIn++;` |
|      1 | 1645 | `		}` |
|     27 | 1646 | `		if( zIn > zCur ){` |
|      - | 1647 | `			/* Append raw contents */` |
|     23 | 1648 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 1649 | `		}` |
|     27 | 1650 | `		if( zIn < zEnd ){` |
|     17 | 1651 | `			int c = zIn[0];` |
|     17 | 1652 | `			if( c == '\0' ){` |
|      - | 1653 | `				/* PHP escapes NUL as "\\0" (two characters) */` |
|      3 | 1654 | `				ph7_result_string(pCtx,"\\0",2);` |
|      2 | 1655 | `			}else{` |
|     15 | 1656 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1657 | `			}` |
|      8 | 1658 | `		}` |
|     27 | 1659 | `		zIn++;` |
|      1 | 1660 | `	}` |
|     15 | 1661 | `	return PH7_OK;` |
|     14 | 1662 |  |
|      - | 1663 | `/*` |
|      - | 1664 | ` * Check if the given character is present in the given mask.` |
|      - | 1665 | ` * Return TRUE if present. FALSE otherwise.` |
|      - | 1666 | ` */` |
|     76 | 1667 | `static int cSlashCheckMask(int c,const char *zMask,int nLen)` |
|      1 | 1668 |  |
|     77 | 1669 | `	const char *zEnd = &zMask[nLen];` |
|    495 | 1670 | `	while( zMask < zEnd ){` |
|    449 | 1671 | `		if( zMask[0] == c ){` |
|      - | 1672 | `			/* Character present,return TRUE */` |
|     31 | 1673 | `			return 1;` |
|      - | 1674 | `		}` |
|      - | 1675 | `		/* Advance the pointer */` |
|    419 | 1676 | `		zMask++;` |
|      1 | 1677 | `	}` |
|      - | 1678 | `	/* Not present */` |
|     47 | 1679 | `	return 0;` |
|     39 | 1680 |  |
|      - | 1681 | `/*` |
|      - | 1682 | ` * string addcslashes(string $str,string $charlist)` |
|      - | 1683 | ` *  Quote string with slashes in a C style.` |
|      - | 1684 | ` * Parameter` |
|      - | 1685 | ` *  $str:` |
|      - | 1686 | ` *    The string to be escaped.` |
|      - | 1687 | ` *  $charlist:` |
|      - | 1688 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|      - | 1689 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|      - | 1690 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|      - | 1691 | ` * Return` |
|      - | 1692 | ` *  Returns the escaped string.` |
|      - | 1693 | ` * Note:` |
|      - | 1694 | ` *  Range characters [i.e: 'A..Z'] is not implemented in the current release.` |
|      - | 1695 | ` */` |
|     12 | 1696 | `static int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1697 |  |
|      - | 1698 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - | 1699 | `	int nLen,nMask;` |
|     13 | 1700 | `	if( nArg < 1 ){` |
|      - | 1701 | `		/* Nothing to process,retun NULL */` |
|      3 | 1702 | `		ph7_result_null(pCtx);` |
|      3 | 1703 | `		return PH7_OK;` |
|      - | 1704 | `	}` |
|      - | 1705 | `	/* Extract the string to process */` |
|     11 | 1706 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 1707 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 1708 | `		/* Return the string untouched */` |
|      5 | 1709 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      5 | 1710 | `		return PH7_OK;` |
|      - | 1711 | `	}` |
|      - | 1712 | `	/* Extract the desired mask */` |
|      7 | 1713 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|      7 | 1714 | `	zEnd = &zIn[nLen];` |
|      7 | 1715 | `	zCur = 0; /* cc warning */` |
|      8 | 1716 | `	for(;;){` |
|     17 | 1717 | `		if( zIn >= zEnd ){` |
|      - | 1718 | `			/* No more input */` |
|      7 | 1719 | `			break;` |
|      - | 1720 | `		}` |
|     11 | 1721 | `		zCur = zIn;` |
|     31 | 1722 | `		while( zIn < zEnd && !cSlashCheckMask(zIn[0],zMask,nMask) ){` |
|     21 | 1723 | `			zIn++;` |
|      1 | 1724 | `		}` |
|     11 | 1725 | `		if( zIn > zCur ){` |
|      - | 1726 | `			/* Append raw contents */` |
|     11 | 1727 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      5 | 1728 | `		}` |
|     11 | 1729 | `		if( zIn < zEnd ){` |
|      5 | 1730 | `			int c = zIn[0];` |
|      5 | 1731 | `			if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|      - | 1732 | `				/* Convert to octal */` |
|      3 | 1733 | `				ph7_result_string_format(pCtx,"\\%o",c);` |
|      2 | 1734 | `			}else{` |
|      3 | 1735 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1736 | `			}` |
|      2 | 1737 | `		}` |
|     11 | 1738 | `		zIn++;` |
|      1 | 1739 | `	}` |
|      7 | 1740 | `	return PH7_OK;` |
|      7 | 1741 |  |
|      - | 1742 | `/*` |
|      - | 1743 | ` * string quotemeta(string $str)` |
|      - | 1744 | ` *  Quote meta characters.` |
|      - | 1745 | ` * Parameter` |
|      - | 1746 | ` *  $str:` |
|      - | 1747 | ` *    The string to be escaped.` |
|      - | 1748 | ` * Return` |
|      - | 1749 | ` *  Returns the escaped string.` |
|      - | 1750 | `*/` |
|     10 | 1751 | `static int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1752 |  |
|      - | 1753 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1754 | `	int nLen;` |
|     11 | 1755 | `	if( nArg < 1 ){` |
|      - | 1756 | `		/* Nothing to process,retun NULL */` |
|      3 | 1757 | `		ph7_result_null(pCtx);` |
|      3 | 1758 | `		return PH7_OK;` |
|      - | 1759 | `	}` |
|      - | 1760 | `	/* Extract the string to process */` |
|      9 | 1761 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 1762 | `	if( nLen < 1 ){` |
|      - | 1763 | `		/* Return the empty string */` |
|      3 | 1764 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1765 | `		return PH7_OK;` |
|      - | 1766 | `	}` |
|      7 | 1767 | `	zEnd = &zIn[nLen];` |
|      7 | 1768 | `	zCur = 0; /* cc warning */` |
|     17 | 1769 | `	for(;;){` |
|     35 | 1770 | `		if( zIn >= zEnd ){` |
|      - | 1771 | `			/* No more input */` |
|      7 | 1772 | `			break;` |
|      - | 1773 | `		}` |
|     29 | 1774 | `		zCur = zIn;` |
|     55 | 1775 | `		while( zIn < zEnd && !cSlashCheckMask(zIn[0],".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1) ){` |
|     27 | 1776 | `			zIn++;` |
|      1 | 1777 | `		}` |
|     29 | 1778 | `		if( zIn > zCur ){` |
|      - | 1779 | `			/* Append raw contents */` |
|     11 | 1780 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      5 | 1781 | `		}` |
|     29 | 1782 | `		if( zIn < zEnd ){` |
|     27 | 1783 | `			int c = zIn[0];` |
|     27 | 1784 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|     13 | 1785 | `		}` |
|     29 | 1786 | `		zIn++;` |
|      1 | 1787 | `	}` |
|      7 | 1788 | `	return PH7_OK;` |
|      6 | 1789 |  |
|      - | 1790 | `/*` |
|      - | 1791 | ` * string stripslashes(string $str)` |
|      - | 1792 | ` *  Un-quotes a quoted string.` |
|      - | 1793 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1794 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1795 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1796 | ` * Parameter` |
|      - | 1797 | ` *  $str` |
|      - | 1798 | ` *   The input string.` |
|      - | 1799 | ` * Return` |
|      - | 1800 | ` *  Returns a string with backslashes stripped off.` |
|      - | 1801 | ` */` |
|      8 | 1802 | `static int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1803 |  |
|      - | 1804 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1805 | `	int nLen;` |
|      9 | 1806 | `	if( nArg < 1 ){` |
|      - | 1807 | `		/* Nothing to process,retun NULL */` |
|      3 | 1808 | `		ph7_result_null(pCtx);` |
|      3 | 1809 | `		return PH7_OK;` |
|      - | 1810 | `	}` |
|      - | 1811 | `	/* Extract the string to process */` |
|      7 | 1812 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 1813 | `	if( zIn == 0 ){` |
|    ! 0 | 1814 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1815 | `		return PH7_OK;` |
|      - | 1816 | `	}` |
|      7 | 1817 | `	zEnd = &zIn[nLen];` |
|      7 | 1818 | `	zCur = 0; /* cc warning */` |
|      - | 1819 | `	/* Encode the string */` |
|      4 | 1820 | `	for(;;){` |
|      9 | 1821 | `		if( zIn >= zEnd ){` |
|      - | 1822 | `			/* No more input */` |
|      5 | 1823 | `			break;` |
|      - | 1824 | `		}` |
|      5 | 1825 | `		zCur = zIn;` |
|     17 | 1826 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 | 1827 | `			zIn++;` |
|      1 | 1828 | `		}` |
|      5 | 1829 | `		if( zIn > zCur ){` |
|      - | 1830 | `			/* Append raw contents */` |
|      5 | 1831 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 1832 | `		}` |
|      5 | 1833 | `		if( &zIn[1] < zEnd ){` |
|      3 | 1834 | `			int c = zIn[1];` |
|      3 | 1835 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - | 1836 | `				/* Ignore the backslash */` |
|      3 | 1837 | `				zIn++;` |
|      1 | 1838 | `			}` |
|      2 | 1839 | `		}else{` |
|      3 | 1840 | `			break;` |
|      - | 1841 | `		}` |
|      1 | 1842 | `	}` |
|      7 | 1843 | `	return PH7_OK;` |
|      5 | 1844 |  |
|      - | 1845 | `/*` |
|      - | 1846 | ` * string htmlspecialchars(string $string [, int $flags = ENT_COMPAT \| ENT_HTML401 [, string $charset]])` |
|      - | 1847 | ` *  HTML escaping of special characters.` |
|      - | 1848 | ` *  The translations performed are:` |
|      - | 1849 | ` *   '&' (ampersand) ==> '&amp;'` |
|      - | 1850 | ` *   '"' (double quote) ==> '&quot;' when ENT_NOQUOTES is not set.` |
|      - | 1851 | ` *   "'" (single quote) ==> '&#039;' only when ENT_QUOTES is set.` |
|      - | 1852 | ` *   '<' (less than) ==> '&lt;'` |
|      - | 1853 | ` *   '>' (greater than) ==> '&gt;'` |
|      - | 1854 | ` * Parameters` |
|      - | 1855 | ` *  $string` |
|      - | 1856 | ` *   The string being converted.` |
|      - | 1857 | ` * $flags` |
|      - | 1858 | ` *   A bitmask of one or more of the following flags, which specify how to handle quotes.` |
|      - | 1859 | ` *   The default is ENT_COMPAT \| ENT_HTML401.` |
|      - | 1860 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone.` |
|      - | 1861 | ` *   ENT_QUOTES 	Will convert both double and single quotes.` |
|      - | 1862 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted.` |
|      - | 1863 | ` *   ENT_IGNORE 	Silently discard invalid code unit sequences instead of returning an empty string.` |
|      - | 1864 | ` * $charset` |
|      - | 1865 | ` *  Defines character set used in conversion. The default character set is ISO-8859-1. (Not used)` |
|      - | 1866 | ` * Return` |
|      - | 1867 | ` *  The escaped string or NULL on failure.` |
|      - | 1868 | ` */` |
|     20 | 1869 | `static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1870 |  |
|      - | 1871 | `	const char *zCur,*zIn,*zEnd;` |
|     21 | 1872 | `	int iFlags = 0x01\|0x40; /* ENT_COMPAT \| ENT_HTML401 */` |
|      - | 1873 | `	int nLen,c;` |
|     21 | 1874 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1875 | `		/* Missing/Invalid arguments,return NULL */` |
|      9 | 1876 | `		ph7_result_null(pCtx);` |
|      9 | 1877 | `		return PH7_OK;` |
|      - | 1878 | `	}` |
|      - | 1879 | `	/* Extract the target string */` |
|     13 | 1880 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1881 | `	/* Return early when the input is empty, mirroring PHP's behavior. */` |
|     13 | 1882 | `	if( nLen == 0 ){` |
|      3 | 1883 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1884 | `		return PH7_OK;` |
|      - | 1885 | `	}` |
|     11 | 1886 | `	zEnd = &zIn[nLen];` |
|      - | 1887 | `	/* Extract the flags if available */` |
|     11 | 1888 | `	if( nArg > 1 ){` |
|      9 | 1889 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 | 1890 | `		if( iFlags < 0 ){` |
|      3 | 1891 | `			iFlags = 0x01\|0x40;` |
|      1 | 1892 | `		}` |
|      4 | 1893 | `	}` |
|      - | 1894 | `	/* Perform the requested operation */` |
|     23 | 1895 | `	for(;;){` |
|     47 | 1896 | `		if( zIn >= zEnd ){` |
|      9 | 1897 | `			break;` |
|      - | 1898 | `		}` |
|     39 | 1899 | `		zCur = zIn;` |
|     83 | 1900 | `		while( zIn < zEnd && zIn[0] != '&' && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '<' && zIn[0] != '>' ){` |
|     45 | 1901 | `			zIn++;` |
|      1 | 1902 | `		}` |
|     39 | 1903 | `		if( zCur < zIn ){` |
|      - | 1904 | `			/* Append the raw string verbatim */` |
|     17 | 1905 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      8 | 1906 | `		}` |
|     39 | 1907 | `		if( zIn >= zEnd ){` |
|      3 | 1908 | `			break;` |
|      - | 1909 | `		}` |
|     37 | 1910 | `		c = zIn[0];` |
|     37 | 1911 | `		if( c == '&' ){` |
|      - | 1912 | `			/* Expand '&amp;' */` |
|      9 | 1913 | `			ph7_result_string(pCtx,"&amp;",(int)sizeof("&amp;")-1);` |
|     33 | 1914 | `		}else if( c == '<' ){` |
|      - | 1915 | `			/* Expand '&lt;' */` |
|      7 | 1916 | `			ph7_result_string(pCtx,"&lt;",(int)sizeof("&lt;")-1);` |
|     26 | 1917 | `		}else if( c == '>' ){` |
|      - | 1918 | `			/* Expand '&gt;' */` |
|      9 | 1919 | `			ph7_result_string(pCtx,"&gt;",(int)sizeof("&gt;")-1);` |
|     19 | 1920 | `		}else if( c == '\'' ){` |
|      5 | 1921 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 1922 | `				/* Expand '&#039;' */` |
|      5 | 1923 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      3 | 1924 | `			}else{` |
|      - | 1925 | `				/* Leave the single quote untouched */` |
|    ! 0 | 1926 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      1 | 1927 | `			}` |
|     13 | 1928 | `		}else if( c == '"' ){` |
|     11 | 1929 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      - | 1930 | `				/* Expand '&quot;' */` |
|      7 | 1931 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      4 | 1932 | `			}else{` |
|      - | 1933 | `				/* Leave the double quote untouched */` |
|      5 | 1934 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      - | 1935 | `			}` |
|      5 | 1936 | `		}` |
|      - | 1937 | `		/* Ignore the unsafe HTML character */` |
|     37 | 1938 | `		zIn++;` |
|      1 | 1939 | `	}` |
|     11 | 1940 | `	return PH7_OK;` |
|     11 | 1941 |  |
|      - | 1942 | `/*` |
|      - | 1943 | ` * string htmlspecialchars_decode(string $string[,int $quote_style = ENT_COMPAT ])` |
|      - | 1944 | ` *  Unescape HTML entities.` |
|      - | 1945 | ` * Parameters` |
|      - | 1946 | ` *  $string` |
|      - | 1947 | ` *   The string to decode` |
|      - | 1948 | ` *  $quote_style` |
|      - | 1949 | ` *    The quote style. One of the following constants:` |
|      - | 1950 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone (default)` |
|      - | 1951 | ` *   ENT_QUOTES 	Will convert both double and single quotes` |
|      - | 1952 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted` |
|      - | 1953 | ` * Return` |
|      - | 1954 | ` *  The unescaped string or NULL on failure.` |
|      - | 1955 | ` */` |
|     16 | 1956 | `static int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1957 |  |
|      - | 1958 | `	const char *zCur,*zIn,*zEnd;` |
|     17 | 1959 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 1960 | `	int nLen,nJump;` |
|     17 | 1961 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1962 | `		/* Missing/Invalid arguments,return NULL */` |
|      7 | 1963 | `		ph7_result_null(pCtx);` |
|      7 | 1964 | `		return PH7_OK;` |
|      - | 1965 | `	}` |
|      - | 1966 | `	/* Extract the target string */` |
|     11 | 1967 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 1968 | `	zEnd = &zIn[nLen];` |
|      - | 1969 | `	/* Extract the flags if available */` |
|     11 | 1970 | `	if( nArg > 1 ){` |
|      7 | 1971 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      7 | 1972 | `		if( iFlags < 0 ){` |
|      3 | 1973 | `			iFlags = 0x01;` |
|      1 | 1974 | `		}` |
|      3 | 1975 | `	}` |
|      - | 1976 | `	/* Perform the requested operation */` |
|     15 | 1977 | `	for(;;){` |
|     31 | 1978 | `		if( zIn >= zEnd ){` |
|     11 | 1979 | `			break;` |
|      - | 1980 | `		}` |
|     21 | 1981 | `		zCur = zIn;` |
|     51 | 1982 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|     31 | 1983 | `			zIn++;` |
|      1 | 1984 | `		}` |
|     21 | 1985 | `		if( zCur < zIn ){` |
|      - | 1986 | `			/* Append the raw string verbatim */` |
|      9 | 1987 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 1988 | `		}` |
|     21 | 1989 | `		nLen = (int)(zEnd-zIn);` |
|     21 | 1990 | `		nJump = (int)sizeof(char);` |
|     21 | 1991 | `		if( nLen >= (int)sizeof("&amp;")-1 && SyStrnicmp(zIn,"&amp;",sizeof("&amp;")-1) == 0 ){` |
|      - | 1992 | `			/* &amp; ==> '&' */` |
|      3 | 1993 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|      3 | 1994 | `			nJump = (int)sizeof("&amp;")-1;` |
|     20 | 1995 | `		}else if( nLen >= (int)sizeof("&lt;")-1 && SyStrnicmp(zIn,"&lt;",sizeof("&lt;")-1) == 0 ){` |
|      - | 1996 | `			/* &lt; ==> < */` |
|      3 | 1997 | `			ph7_result_string(pCtx,"<",(int)sizeof(char));` |
|      3 | 1998 | `			nJump = (int)sizeof("&lt;")-1;` |
|     18 | 1999 | `		}else if( nLen >= (int)sizeof("&gt;")-1 && SyStrnicmp(zIn,"&gt;",sizeof("&gt;")-1) == 0 ){` |
|      - | 2000 | `			/* &gt; ==> '>' */` |
|      3 | 2001 | `			ph7_result_string(pCtx,">",(int)sizeof(char));` |
|      3 | 2002 | `			nJump = (int)sizeof("&gt;")-1;` |
|     16 | 2003 | `		}else if( nLen >= (int)sizeof("&quot;")-1 && SyStrnicmp(zIn,"&quot;",sizeof("&quot;")-1) == 0 ){` |
|      - | 2004 | `			/* &quot; ==> '"' */` |
|     13 | 2005 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      9 | 2006 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      5 | 2007 | `			}else{` |
|      - | 2008 | `				/* Leave untouched */` |
|      5 | 2009 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      - | 2010 | `			}` |
|     13 | 2011 | `			nJump = (int)sizeof("&quot;")-1;` |
|      9 | 2012 | `		}else if( nLen >= (int)sizeof("&#039;")-1 && SyStrnicmp(zIn,"&#039;",sizeof("&#039;")-1) == 0 ){` |
|      - | 2013 | `			/* &#039; ==> ''' */` |
|      3 | 2014 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 2015 | `				/* Expand ''' */` |
|      3 | 2016 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      2 | 2017 | `			}else{` |
|      - | 2018 | `				/* Leave untouched */` |
|    ! 0 | 2019 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      - | 2020 | `			}` |
|      3 | 2021 | `			nJump = (int)sizeof("&#039;")-1;` |
|      1 | 2022 | `		}else if( nLen >= (int)sizeof(char) ){` |
|      - | 2023 | `			/* expand '&' */` |
|    ! 0 | 2024 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 2025 | `		}else{` |
|      - | 2026 | `			/* No more input to process */` |
|    ! 0 | 2027 | `			break;` |
|      - | 2028 | `		}` |
|     21 | 2029 | `		zIn += nJump;` |
|      1 | 2030 | `	}` |
|     11 | 2031 | `	return PH7_OK;` |
|      9 | 2032 |  |
|      - | 2033 | `/* HTML encoding/Decoding table` |
|      - | 2034 | ` * Source: Symisc RunTime API.[chm@symisc.net]` |
|      - | 2035 | ` */` |
|      - | 2036 | `static const char *azHtmlEscape[] = {` |
|      - | 2037 | ` 	"&lt;","<","&gt;",">","&amp;","&","&quot;","\"","&#39;","'",` |
|      - | 2038 | `	"&#33;","!","&#36;","$","&#35;","#","&#37;","%","&#40;","(",` |
|      - | 2039 | `	"&#41;",")","&#123;","{","&#125;","}","&#61;","=","&#43;","+",` |
|      - | 2040 | `	"&#63;","?","&#91;","[","&#93;","]","&#64;","@","&#44;",","` |
|      - | 2041 | ` };` |
|      - | 2042 | `/*` |
|      - | 2043 | ` * array get_html_translation_table(void)` |
|      - | 2044 | ` *  Returns the translation table used by htmlspecialchars() and htmlentities().` |
|      - | 2045 | ` * Parameters` |
|      - | 2046 | ` *  None` |
|      - | 2047 | ` * Return` |
|      - | 2048 | ` *  The translation table as an array or NULL on failure.` |
|      - | 2049 | ` */` |
|      4 | 2050 | `static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2051 |  |
|      - | 2052 | `	ph7_value *pArray,*pValue;` |
|      - | 2053 | `	sxu32 n;` |
|      - | 2054 | `	/* Element value */` |
|      5 | 2055 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      5 | 2056 | `	if( pValue == 0 ){` |
|    ! 0 | 2057 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2058 | `		SXUNUSED(apArg);` |
|      - | 2059 | `		/* Return NULL */` |
|    ! 0 | 2060 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2061 | `		return PH7_OK;` |
|      - | 2062 | `	}` |
|      - | 2063 | `	/* Create a new array */` |
|      5 | 2064 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 | 2065 | `	if( pArray == 0 ){` |
|      - | 2066 | `		/* Return NULL */` |
|    ! 0 | 2067 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2068 | `		return PH7_OK;` |
|      - | 2069 | `	}` |
|      - | 2070 | `	/* Make the table */` |
|     85 | 2071 | `	for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|      - | 2072 | `		/* Prepare the value */` |
|     81 | 2073 | `		ph7_value_string(pValue,azHtmlEscape[n],-1 /* Compute length automatically */);` |
|      - | 2074 | `		/* Insert the value */` |
|     81 | 2075 | `		ph7_array_add_strkey_elem(pArray,azHtmlEscape[n+1],pValue);` |
|      - | 2076 | `		/* Reset the string cursor */` |
|     81 | 2077 | `		ph7_value_reset_string_cursor(pValue);` |
|     41 | 2078 | `	}` |
|      - | 2079 | `	/*` |
|      - | 2080 | `	 * Return the array.` |
|      - | 2081 | `	 * Don't worry about freeing memory, everything will be automatically` |
|      - | 2082 | `	 * released upon we return from this function.` |
|      - | 2083 | `	 */` |
|      5 | 2084 | `	ph7_result_value(pCtx,pArray);` |
|      5 | 2085 | `	return PH7_OK;` |
|      3 | 2086 |  |
|      - | 2087 | `/*` |
|      - | 2088 | ` * string htmlentities( string $string [, int $flags = ENT_COMPAT \| ENT_HTML401]);` |
|      - | 2089 | ` *   Convert all applicable characters to HTML entities` |
|      - | 2090 | ` * Parameters` |
|      - | 2091 | ` * $string` |
|      - | 2092 | ` *   The input string.` |
|      - | 2093 | ` * $flags` |
|      - | 2094 | ` *  A bitmask of one or more of the flags (see block-comment on PH7_builtin_htmlspecialchars())` |
|      - | 2095 | ` * Return` |
|      - | 2096 | ` * The encoded string.` |
|      - | 2097 | ` */` |
|     10 | 2098 | `static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2099 |  |
|     11 | 2100 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 2101 | `	const char *zIn,*zEnd;` |
|      - | 2102 | `	int nLen,c;` |
|      - | 2103 | `	sxu32 n;` |
|     11 | 2104 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2105 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 2106 | `		ph7_result_null(pCtx);` |
|      5 | 2107 | `		return PH7_OK;` |
|      - | 2108 | `	}` |
|      - | 2109 | `	/* Extract the target string */` |
|      7 | 2110 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 2111 | `	/* Handle empty string up front */` |
|      7 | 2112 | `	if( nLen == 0 ){` |
|      3 | 2113 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2114 | `		return PH7_OK;` |
|      - | 2115 | `	}` |
|      5 | 2116 | `	zEnd = &zIn[nLen];` |
|      - | 2117 | `	/* Extract the flags if available */` |
|      5 | 2118 | `	if( nArg > 1 ){` |
|      3 | 2119 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      3 | 2120 | `		if( iFlags < 0 ){` |
|      3 | 2121 | `			iFlags = 0x01;` |
|      1 | 2122 | `		}` |
|      1 | 2123 | `	}` |
|      - | 2124 | `	/* Perform the requested operation */` |
|     11 | 2125 | `	for(;;){` |
|     23 | 2126 | `		if( zIn >= zEnd ){` |
|      - | 2127 | `			/* No more input to process */` |
|      5 | 2128 | `			break;` |
|      - | 2129 | `		}` |
|     19 | 2130 | `		c = zIn[0];` |
|      - | 2131 | `		/* Perform a linear lookup on the decoding table */` |
|    233 | 2132 | `		for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    223 | 2133 | `			if( azHtmlEscape[n+1][0] == c ){` |
|      - | 2134 | `				/* Got one */` |
|      9 | 2135 | `				break;` |
|      - | 2136 | `			}` |
|    108 | 2137 | `		}` |
|     19 | 2138 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|      - | 2139 | `			/* Output the safe sequence [i.e: '<' ==> '&lt;"] */` |
|      9 | 2140 | `			if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 2141 | `				/* Expand the double quote verbatim */` |
|    ! 0 | 2142 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      9 | 2143 | `			}else if(c == '\'' && ((iFlags & 0x02 /*ENT_QUOTES*/) == 0 \|\| (iFlags & 0x04) /*ENT_NOQUOTES*/) ){` |
|      - | 2144 | `				/* expand single quote verbatim */` |
|    ! 0 | 2145 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|    ! 0 | 2146 | `			}else{` |
|      9 | 2147 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1/*Compute length automatically */);` |
|      - | 2148 | `			}` |
|      5 | 2149 | `		}else{` |
|      - | 2150 | `			/* Output character verbatim */` |
|     11 | 2151 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2152 | `		}` |
|     19 | 2153 | `		zIn++;` |
|      1 | 2154 | `	}` |
|      5 | 2155 | `	return PH7_OK;` |
|      6 | 2156 |  |
|      - | 2157 | `/*` |
|      - | 2158 | ` * string html_entity_decode(string $string [, int $quote_style = ENT_COMPAT [, string $charset = 'UTF-8' ]])` |
|      - | 2159 | ` *   Perform the reverse operation of html_entity_decode().` |
|      - | 2160 | ` * Parameters` |
|      - | 2161 | ` * $string` |
|      - | 2162 | ` *   The input string.` |
|      - | 2163 | ` * $flags` |
|      - | 2164 | ` *  A bitmask of one or more of the flags (see comment on PH7_builtin_htmlspecialchars())` |
|      - | 2165 | ` * Return` |
|      - | 2166 | ` * The decoded string.` |
|      - | 2167 | ` */` |
|     28 | 2168 | `static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2169 |  |
|      - | 2170 | `	const char *zCur,*zIn,*zEnd;` |
|     29 | 2171 | `	int iFlags = 0x01; /* ENT_COMPAT  */` |
|      - | 2172 | `	int nLen;` |
|      - | 2173 | `	sxu32 n;` |
|     29 | 2174 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2175 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 2176 | `		ph7_result_null(pCtx);` |
|      5 | 2177 | `		return PH7_OK;` |
|      - | 2178 | `	}` |
|      - | 2179 | `	/* Extract the target string */` |
|     25 | 2180 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 2181 | `	zEnd = &zIn[nLen];` |
|      - | 2182 | `	/* Extract the flags if available */` |
|     25 | 2183 | `	if( nArg > 1 ){` |
|     15 | 2184 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     15 | 2185 | `		if( iFlags < 0 ){` |
|      3 | 2186 | `			iFlags = 0x01;` |
|      1 | 2187 | `		}` |
|      7 | 2188 | `	}` |
|      - | 2189 | `	/* Perform the requested operation */` |
|     27 | 2190 | `	for(;;){` |
|     55 | 2191 | `		if( zIn >= zEnd ){` |
|      - | 2192 | `			/* No more input to process */` |
|     13 | 2193 | `			break;` |
|      - | 2194 | `		}` |
|     43 | 2195 | `		zCur = zIn;` |
|    173 | 2196 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|    131 | 2197 | `			zIn++;` |
|      1 | 2198 | `		}` |
|     43 | 2199 | `		if( zCur < zIn ){` |
|      - | 2200 | `			/* Append raw string verbatim */` |
|     27 | 2201 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     13 | 2202 | `		}` |
|     43 | 2203 | `		if( zIn >= zEnd ){` |
|     13 | 2204 | `			break;` |
|      - | 2205 | `		}` |
|     31 | 2206 | `		nLen = (int)(zEnd-zIn);` |
|      - | 2207 | `		/* Find an encoded sequence */` |
|    113 | 2208 | `		for(n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    113 | 2209 | `			int iLen = (int)SyStrlen(azHtmlEscape[n]);` |
|    113 | 2210 | `			if( nLen >= iLen && SyStrnicmp(zIn,azHtmlEscape[n],(sxu32)iLen) == 0 ){` |
|      - | 2211 | `				/* Got one */` |
|     31 | 2212 | `				zIn += iLen;` |
|     31 | 2213 | `				break;` |
|      - | 2214 | `			}` |
|     42 | 2215 | `		}` |
|     31 | 2216 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|     31 | 2217 | `			int c = azHtmlEscape[n+1][0];` |
|      - | 2218 | `			/* Output the decoded character */` |
|     31 | 2219 | `			if( c == '\'' && ((iFlags & 0x02) == 0 /*ENT_QUOTES*/\|\| (iFlags & 0x04) /*ENT_NOQUOTES*/)  ){` |
|      - | 2220 | `				/* Do not process single quotes */` |
|      9 | 2221 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|     27 | 2222 | `			}else if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 2223 | `				/* Do not process double quotes */` |
|      5 | 2224 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|      3 | 2225 | `			}else{` |
|     19 | 2226 | `				ph7_result_string(pCtx,azHtmlEscape[n+1],-1); /* Compute length automatically */` |
|      - | 2227 | `			}` |
|     16 | 2228 | `		}else{` |
|      - | 2229 | `			/* Append '&' */` |
|    ! 0 | 2230 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 2231 | `			zIn++;` |
|      - | 2232 | `		}` |
|      1 | 2233 | `	}` |
|     25 | 2234 | `	return PH7_OK;` |
|     15 | 2235 |  |
|      - | 2236 | `/*` |
|      - | 2237 | ` * int strlen($string)` |
|      - | 2238 | ` *  return the length of the given string.` |
|      - | 2239 | ` * Parameter` |
|      - | 2240 | ` *  string: The string being measured for length.` |
|      - | 2241 | ` * Return` |
|      - | 2242 | ` *  length of the given string.` |
|      - | 2243 | ` */` |
|   1560 | 2244 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2245 |  |
|   1562 | 2246 | `	int iLen = 0;` |
|   1562 | 2247 | `	if( nArg > 0 ){` |
|   1560 | 2248 | `		ph7_value_to_string(apArg[0],&iLen);` |
|    779 | 2249 | `	}` |
|      - | 2250 | `	/* String length */` |
|   1562 | 2251 | `	ph7_result_int(pCtx,iLen);` |
|   1562 | 2252 | `	return PH7_OK;` |
|      2 | 2253 |  |
|      - | 2254 | `/*` |
|      - | 2255 | ` * int strcmp(string $str1,string $str2)` |
|      - | 2256 | ` *  Perform a binary safe string comparison.` |
|      - | 2257 | ` * Parameter` |
|      - | 2258 | ` *  str1: The first string` |
|      - | 2259 | ` *  str2: The second string` |
|      - | 2260 | ` * Return` |
|      - | 2261 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2262 | ` *  than str2, and 0 if they are equal.` |
|      - | 2263 | ` */` |
|     50 | 2264 | `static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2265 |  |
|      - | 2266 | `	const char *z1,*z2;` |
|      - | 2267 | `	int n1,n2;` |
|      - | 2268 | `	int res;` |
|     51 | 2269 | `	if( nArg < 2 ){` |
|      5 | 2270 | `		res = nArg == 0 ? 0 : 1;` |
|      5 | 2271 | `		ph7_result_int(pCtx,res);` |
|      5 | 2272 | `		return PH7_OK;` |
|      - | 2273 | `	}` |
|      - | 2274 | `	/* Perform the comparison */` |
|     47 | 2275 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     47 | 2276 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     47 | 2277 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2278 | `	/* Comparison result */` |
|     47 | 2279 | `	ph7_result_int(pCtx,res);` |
|     47 | 2280 | `	return PH7_OK;` |
|     26 | 2281 |  |
|      - | 2282 | `/*` |
|      - | 2283 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 2284 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 2285 | ` * Parameter` |
|      - | 2286 | ` *  str1: The first string` |
|      - | 2287 | ` *  str2: The second string` |
|      - | 2288 | ` * Return` |
|      - | 2289 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2290 | ` *  than str2, and 0 if they are equal.` |
|      - | 2291 | ` */` |
|     20 | 2292 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2293 |  |
|      - | 2294 | `	const char *z1,*z2;` |
|      - | 2295 | `	int res;` |
|      - | 2296 | `	int n;` |
|     21 | 2297 | `	if( nArg < 3 ){` |
|      - | 2298 | `		/* Perform a standard comparison */` |
|      5 | 2299 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 2300 | `	}` |
|      - | 2301 | `	/* Desired comparison length */` |
|     17 | 2302 | `	n  = ph7_value_to_int(apArg[2]);` |
|     17 | 2303 | `	if( n < 0 ){` |
|      - | 2304 | `		/* Invalid length */` |
|      3 | 2305 | `		ph7_result_int(pCtx,-1);` |
|      3 | 2306 | `		return PH7_OK;` |
|      - | 2307 | `	}` |
|      - | 2308 | `	/* Perform the comparison */` |
|     15 | 2309 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     15 | 2310 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     15 | 2311 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 2312 | `	/* Comparison result */` |
|     15 | 2313 | `	ph7_result_int(pCtx,res);` |
|     15 | 2314 | `	return PH7_OK;` |
|     11 | 2315 |  |
|      - | 2316 | `/*` |
|      - | 2317 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 2318 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 2319 | ` * Parameter` |
|      - | 2320 | ` *  str1: The first string` |
|      - | 2321 | ` *  str2: The second string` |
|      - | 2322 | ` * Return` |
|      - | 2323 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2324 | ` *  than str2, and 0 if they are equal.` |
|      - | 2325 | ` */` |
|     18 | 2326 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2327 |  |
|      - | 2328 | `	const char *z1,*z2;` |
|      - | 2329 | `	int n1,n2;` |
|      - | 2330 | `	int res;` |
|     19 | 2331 | `	if( nArg < 2 ){` |
|      9 | 2332 | `		res = nArg == 0 ? 0 : 1;` |
|      9 | 2333 | `		ph7_result_int(pCtx,res);` |
|      9 | 2334 | `		return PH7_OK;` |
|      - | 2335 | `	}` |
|      - | 2336 | `	/* Perform the comparison */` |
|     11 | 2337 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     11 | 2338 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     11 | 2339 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2340 | `	/* Comparison result */` |
|     11 | 2341 | `	ph7_result_int(pCtx,res);` |
|     11 | 2342 | `	return PH7_OK;` |
|     10 | 2343 |  |
|      - | 2344 | `/*` |
|      - | 2345 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 2346 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 2347 | ` * Parameter` |
|      - | 2348 | ` *  $str1: The first string` |
|      - | 2349 | ` *  $str2: The second string` |
|      - | 2350 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 2351 | ` * Return` |
|      - | 2352 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2353 | ` *  than str2, and 0 if they are equal.` |
|      - | 2354 | ` */` |
|      8 | 2355 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2356 |  |
|      - | 2357 | `	const char *z1,*z2;` |
|      - | 2358 | `	int res;` |
|      - | 2359 | `	int n;` |
|      9 | 2360 | `	if( nArg < 3 ){` |
|      - | 2361 | `		/* Perform a standard comparison */` |
|      5 | 2362 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 2363 | `	}` |
|      - | 2364 | `	/* Desired comparison length */` |
|      5 | 2365 | `	n  = ph7_value_to_int(apArg[2]);` |
|      5 | 2366 | `	if( n < 0 ){` |
|      - | 2367 | `		/* Invalid length */` |
|    ! 0 | 2368 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2369 | `		return PH7_OK;` |
|      - | 2370 | `	}` |
|      - | 2371 | `	/* Perform the comparison */` |
|      5 | 2372 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|      5 | 2373 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|      5 | 2374 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 2375 | `	/* Comparison result */` |
|      5 | 2376 | `	ph7_result_int(pCtx,res);` |
|      5 | 2377 | `	return PH7_OK;` |
|      5 | 2378 |  |
|      - | 2379 | `/*` |
|      - | 2380 | ` * Implode context [i.e: it's private data].` |
|      - | 2381 | ` * A pointer to the following structure is forwarded` |
|      - | 2382 | ` * verbatim to the array walker callback defined below.` |
|      - | 2383 | ` */` |
|      - | 2384 | `struct implode_data {` |
|      - | 2385 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 2386 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 2387 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 2388 | `	int nSeplen;          /* Separator length */` |
|      - | 2389 | `	int bFirst;           /* TRUE if first call */` |
|      - | 2390 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 2391 | `};` |
|      - | 2392 | `/*` |
|      - | 2393 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 2394 | ` * The following routine is invoked for each array entry passed` |
|      - | 2395 | ` * to the implode() function.` |
|      - | 2396 | ` */` |
|  79324 | 2397 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      2 | 2398 |  |
|  39662 | 2399 | `	SXUNUSED(pKey);` |
|  79326 | 2400 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 2401 | `	const char *zData;` |
|      - | 2402 | `	int nLen;` |
|  79326 | 2403 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 2404 | `		if( pData->nSeplen > 0 ){` |
|      3 | 2405 | `			if( !pData->bFirst ){` |
|      - | 2406 | `				/* append the separator first */` |
|      3 | 2407 | `				ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|      2 | 2408 | `			}else{` |
|    ! 0 | 2409 | `				pData->bFirst = 0;` |
|      - | 2410 | `			}` |
|      1 | 2411 | `		}` |
|      - | 2412 | `		/* Recurse */` |
|      3 | 2413 | `		pData->bFirst = 1;` |
|      3 | 2414 | `		pData->nRecCount++;` |
|      3 | 2415 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 2416 | `		pData->nRecCount--;` |
|      3 | 2417 | `		return PH7_OK;` |
|      - | 2418 | `	}` |
|      - | 2419 | `	/* Extract the string representation of the entry value */` |
|  79324 | 2420 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 2421 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
|  79324 | 2422 | `	if( pData->bFirst ){` |
|  17198 | 2423 | `		pData->bFirst = 0;` |
|  70726 | 2424 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 2425 | `		/* append the separator first */` |
|  62116 | 2426 | `		ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|  31057 | 2427 | `	}` |
|      - | 2428 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
|  79324 | 2429 | `	if( nLen > 0 ){` |
|  72338 | 2430 | `		ph7_result_string(pData->pCtx,zData,nLen);` |
|  36168 | 2431 | `	}` |
|  79324 | 2432 | `	return PH7_OK;` |
|  39664 | 2433 |  |
|      - | 2434 | `/*` |
|      - | 2435 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 2436 | ` * string implode(array $pieces,...)` |
|      - | 2437 | ` *  Join array elements with a string.` |
|      - | 2438 | ` * $glue` |
|      - | 2439 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 2440 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 2441 | ` * $pieces` |
|      - | 2442 | ` *   The array of strings to implode.` |
|      - | 2443 | ` * Return` |
|      - | 2444 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 2445 | ` *  order, with the glue string between each element.` |
|      - | 2446 | ` */` |
|  17224 | 2447 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2448 |  |
|      - | 2449 | `	struct implode_data imp_data;` |
|  17226 | 2450 | `	int i = 1;` |
|  17226 | 2451 | `	if( nArg < 1 ){` |
|      - | 2452 | `		/* Missing argument,return NULL */` |
|    ! 0 | 2453 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2454 | `		return PH7_OK;` |
|      - | 2455 | `	}` |
|      - | 2456 | `	/* Prepare the implode context */` |
|  17226 | 2457 | `	imp_data.pCtx = pCtx;` |
|  17226 | 2458 | `	imp_data.bRecursive = 0;` |
|  17226 | 2459 | `	imp_data.bFirst = 1;` |
|  17226 | 2460 | `	imp_data.nRecCount = 0;` |
|  17226 | 2461 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  17224 | 2462 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|   8613 | 2463 | `	}else{` |
|      3 | 2464 | `		imp_data.zSep = 0;` |
|      3 | 2465 | `		imp_data.nSeplen = 0;` |
|      3 | 2466 | `		i = 0;` |
|      - | 2467 | `	}` |
|  17226 | 2468 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 2469 | `	/* Start the 'join' process */` |
|  34450 | 2470 | `	while( i < nArg ){` |
|  17226 | 2471 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2472 | `			/* Iterate throw array entries */` |
|  17226 | 2473 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|   8614 | 2474 | `		}else{` |
|      - | 2475 | `			const char *zData;` |
|      - | 2476 | `			int nLen;` |
|      - | 2477 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 2478 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2479 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 2480 | `			if( imp_data.bFirst ){` |
|    ! 0 | 2481 | `				imp_data.bFirst = 0;` |
|    ! 0 | 2482 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2483 | `				ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen);` |
|    ! 0 | 2484 | `			}` |
|      - | 2485 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 2486 | `			if( nLen > 0 ){` |
|    ! 0 | 2487 | `				ph7_result_string(pCtx,zData,nLen);` |
|    ! 0 | 2488 | `			}` |
|      - | 2489 | `		}` |
|  17226 | 2490 | `		i++;` |
|      2 | 2491 | `	}` |
|  17226 | 2492 | `	return PH7_OK;` |
|   8614 | 2493 |  |
|      - | 2494 | `/*` |
|      - | 2495 | ` * Symisc eXtension:` |
|      - | 2496 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 2497 | ` * Purpose` |
|      - | 2498 | ` *  Same as implode() but recurse on arrays.` |
|      - | 2499 | ` * Example:` |
|      - | 2500 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 2501 | ` *   echo implode_recursive("/",$a);` |
|      - | 2502 | ` *   Will output` |
|      - | 2503 | ` *     usr/home/dean.` |
|      - | 2504 | ` *   While the standard implode would produce.` |
|      - | 2505 | ` *    usr/Array.` |
|      - | 2506 | ` * Parameter` |
|      - | 2507 | ` *  Refer to implode().` |
|      - | 2508 | ` * Return` |
|      - | 2509 | ` *  Refer to implode().` |
|      - | 2510 | ` */` |
|     12 | 2511 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2512 |  |
|      - | 2513 | `	struct implode_data imp_data;` |
|     13 | 2514 | `	int i = 1;` |
|     13 | 2515 | `	if( nArg < 1 ){` |
|      - | 2516 | `		/* Missing argument,return NULL */` |
|      3 | 2517 | `		ph7_result_null(pCtx);` |
|      3 | 2518 | `		return PH7_OK;` |
|      - | 2519 | `	}` |
|      - | 2520 | `	/* Prepare the implode context */` |
|     11 | 2521 | `	imp_data.pCtx = pCtx;` |
|     11 | 2522 | `	imp_data.bRecursive = 1;` |
|     11 | 2523 | `	imp_data.bFirst = 1;` |
|     11 | 2524 | `	imp_data.nRecCount = 0;` |
|     11 | 2525 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 2526 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 2527 | `	}else{` |
|    ! 0 | 2528 | `		imp_data.zSep = 0;` |
|    ! 0 | 2529 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 2530 | `		i = 0;` |
|      - | 2531 | `	}` |
|     11 | 2532 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 2533 | `	/* Start the 'join' process */` |
|     21 | 2534 | `	while( i < nArg ){` |
|     11 | 2535 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2536 | `			/* Iterate throw array entries */` |
|      3 | 2537 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      2 | 2538 | `		}else{` |
|      - | 2539 | `			const char *zData;` |
|      - | 2540 | `			int nLen;` |
|      - | 2541 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 2542 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2543 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 2544 | `			if( imp_data.bFirst ){` |
|      9 | 2545 | `				imp_data.bFirst = 0;` |
|      4 | 2546 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2547 | `				ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen);` |
|    ! 0 | 2548 | `			}` |
|      - | 2549 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 2550 | `			if( nLen > 0 ){` |
|      9 | 2551 | `				ph7_result_string(pCtx,zData,nLen);` |
|      4 | 2552 | `			}` |
|      - | 2553 | `		}` |
|     11 | 2554 | `		i++;` |
|      1 | 2555 | `	}` |
|     11 | 2556 | `	return PH7_OK;` |
|      7 | 2557 |  |
|      - | 2558 | `/*` |
|      - | 2559 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 2560 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 2561 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 2562 | ` * Parameters` |
|      - | 2563 | ` *  $delimiter` |
|      - | 2564 | ` *   The boundary string.` |
|      - | 2565 | ` * $string` |
|      - | 2566 | ` *   The input string.` |
|      - | 2567 | ` * $limit` |
|      - | 2568 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 2569 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 2570 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 2571 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 2572 | ` * Returns` |
|      - | 2573 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 2574 | ` *  on boundaries formed by the delimiter.` |
|      - | 2575 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 2576 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 2577 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 2578 | ` *  will be returned.` |
|      - | 2579 | ` * NOTE:` |
|      - | 2580 | ` *  Negative limit is not supported.` |
|      - | 2581 | ` */` |
|   3120 | 2582 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2583 |  |
|      - | 2584 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 2585 | `	int nDelim,nStrlen,iLimit;` |
|      - | 2586 | `	ph7_value *pArray;` |
|      - | 2587 | `	ph7_value *pValue;` |
|      - | 2588 | `	sxu32 nOfft;` |
|      - | 2589 | `	sxi32 rc;` |
|   3122 | 2590 | `	if( nArg < 2 ){` |
|      - | 2591 | `		/* Missing arguments,return FALSE */` |
|      9 | 2592 | `		ph7_result_bool(pCtx,0);` |
|      9 | 2593 | `		return PH7_OK;` |
|      - | 2594 | `	}` |
|      - | 2595 | `	/* Extract the delimiter */` |
|   3114 | 2596 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   3114 | 2597 | `	if( nDelim < 1 ){` |
|      - | 2598 | `		/* Empty delimiter,return FALSE */` |
|      3 | 2599 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2600 | `		return PH7_OK;` |
|      - | 2601 | `	}` |
|      - | 2602 | `	/* Extract the string */` |
|   3112 | 2603 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   3112 | 2604 | `	if( nStrlen < 1 ){` |
|      - | 2605 | `		/* Empty string: return an array with a single empty element (PHP behavior) */` |
|      3 | 2606 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      3 | 2607 | `		ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      3 | 2608 | `		if( pArrayTmp == 0 \|\| pValueTmp == 0 ){` |
|      - | 2609 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 2610 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2611 | `			return PH7_OK;` |
|      - | 2612 | `		}` |
|      3 | 2613 | `		ph7_value_string(pValueTmp, "", 0);` |
|      3 | 2614 | `		ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp);` |
|      3 | 2615 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      3 | 2616 | `		return PH7_OK;` |
|      - | 2617 | `	}` |
|      - | 2618 | `	/* Point to the end of the string */` |
|   3110 | 2619 | `	zEnd = &zString[nStrlen];` |
|      - | 2620 | `	/* Create the array */` |
|   3110 | 2621 | `	pArray =  ph7_context_new_array(pCtx);` |
|   3110 | 2622 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   3110 | 2623 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2624 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 2625 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2626 | `		return PH7_OK;` |
|      - | 2627 | `	}` |
|      - | 2628 | `	/* Set a defualt limit */` |
|   3110 | 2629 | `	iLimit = SXI32_HIGH;` |
|   3110 | 2630 | `	if( nArg > 2 ){` |
|      9 | 2631 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|      9 | 2632 | `		 if( iLimit < 0 ){` |
|      3 | 2633 | `			iLimit = -iLimit;` |
|      1 | 2634 | `		}` |
|      9 | 2635 | `		if( iLimit == 0 ){` |
|      3 | 2636 | `			iLimit = 1;` |
|      1 | 2637 | `		}` |
|      9 | 2638 | `		iLimit--;` |
|      4 | 2639 | `	}` |
|      - | 2640 | `	/* Start exploding */` |
|  38048 | 2641 | `	for(;;){` |
|  76098 | 2642 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
|  76098 | 2643 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 2644 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   3110 | 2645 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   3110 | 2646 | `			ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|   3110 | 2647 | `			break;` |
|      - | 2648 | `		}` |
|      - | 2649 | `		/* Point to the desired offset */` |
|  72990 | 2650 | `		zCur = &zString[nOfft];` |
|      - | 2651 | `		/* Perform the store operation (may be empty) */` |
|  72990 | 2652 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|  72990 | 2653 | `		ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|      - | 2654 | `		/* Point beyond the delimiter */` |
|  72990 | 2655 | `		zString = &zCur[nDelim];` |
|      - | 2656 | `		/* Reset the cursor */` |
|  72990 | 2657 | `		ph7_value_reset_string_cursor(pValue);` |
|      2 | 2658 | `	}` |
|      - | 2659 | `	/* Return the freshly created array */` |
|   3110 | 2660 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2661 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 2662 | `	 * released as soon we return from this foregin function.` |
|      - | 2663 | `	 */` |
|   3110 | 2664 | `	return PH7_OK;` |
|   1562 | 2665 |  |
|      - | 2666 | `/*` |
|      - | 2667 | ` * string trim(string $str[,string $charlist ])` |
|      - | 2668 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2669 | ` * Parameters` |
|      - | 2670 | ` *  $str` |
|      - | 2671 | ` *   The string that will be trimmed.` |
|      - | 2672 | ` * $charlist` |
|      - | 2673 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2674 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2675 | ` *   With .. you can specify a range of characters.` |
|      - | 2676 | ` * Returns.` |
|      - | 2677 | ` *  Thr processed string.` |
|      - | 2678 | ` * NOTE:` |
|      - | 2679 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 2680 | ` */` |
|   7820 | 2681 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2682 |  |
|      - | 2683 | `	const char *zString;` |
|      - | 2684 | `	int nLen;` |
|   7822 | 2685 | `	if( nArg < 1 ){` |
|      - | 2686 | `		/* Missing arguments,return null */` |
|      3 | 2687 | `		ph7_result_null(pCtx);` |
|      3 | 2688 | `		return PH7_OK;` |
|      - | 2689 | `	}` |
|      - | 2690 | `	/* Extract the target string */` |
|   7820 | 2691 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|   7820 | 2692 | `	if( nLen < 1 ){` |
|      - | 2693 | `		/* Empty string,return */` |
|   1638 | 2694 | `		ph7_result_string(pCtx,"",0);` |
|   1638 | 2695 | `		return PH7_OK;` |
|      - | 2696 | `	}` |
|      - | 2697 | `	/* Start the trim process */` |
|   6184 | 2698 | `	if( nArg < 2 ){` |
|      - | 2699 | `		SyString sStr;` |
|      - | 2700 | `		/* Remove white spaces and NUL bytes */` |
|   6180 | 2701 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  14996 | 2702 | `		SyStringFullTrimSafe(&sStr);` |
|   6180 | 2703 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   3091 | 2704 | `	}else{` |
|      - | 2705 | `		/* Char list */` |
|      - | 2706 | `		const char *zList;` |
|      - | 2707 | `		int nListlen;` |
|      5 | 2708 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 2709 | `		if( nListlen < 1 ){` |
|      - | 2710 | `			/* Return the string unchanged */` |
|      3 | 2711 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2712 | `		}else{` |
|      3 | 2713 | `			const char *zEnd = &zString[nLen];` |
|      3 | 2714 | `			const char *zCur = zString;` |
|      - | 2715 | `			const char *zPtr;` |
|      - | 2716 | `			int i;` |
|      - | 2717 | `			/* Left trim */` |
|      4 | 2718 | `			for(;;){` |
|      9 | 2719 | `				if( zCur >= zEnd ){` |
|    ! 0 | 2720 | `					break;` |
|      - | 2721 | `				}` |
|      9 | 2722 | `				zPtr = zCur;` |
|     17 | 2723 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 2724 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|      7 | 2725 | `						zCur++;` |
|      3 | 2726 | `					}` |
|      5 | 2727 | `				}` |
|      9 | 2728 | `				if( zCur == zPtr ){` |
|      - | 2729 | `					/* No match,break immediately */` |
|      3 | 2730 | `					break;` |
|      - | 2731 | `				}` |
|      1 | 2732 | `			}` |
|      - | 2733 | `			/* Right trim */` |
|      3 | 2734 | `			zEnd--;` |
|      4 | 2735 | `			for(;;){` |
|      9 | 2736 | `				if( zEnd <= zCur ){` |
|    ! 0 | 2737 | `					break;` |
|      - | 2738 | `				}` |
|      9 | 2739 | `				zPtr = zEnd;` |
|     17 | 2740 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 2741 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      7 | 2742 | `						zEnd--;` |
|      3 | 2743 | `					}` |
|      5 | 2744 | `				}` |
|      9 | 2745 | `				if( zEnd == zPtr ){` |
|      3 | 2746 | `					break;` |
|      - | 2747 | `				}` |
|      1 | 2748 | `			}` |
|      3 | 2749 | `			if( zCur >= zEnd ){` |
|      - | 2750 | `				/* Return the empty string */` |
|    ! 0 | 2751 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2752 | `			}else{` |
|      3 | 2753 | `				zEnd++;` |
|      3 | 2754 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2755 | `			}` |
|      - | 2756 | `		}` |
|      - | 2757 | `	}` |
|   6184 | 2758 | `	return PH7_OK;` |
|   3912 | 2759 |  |
|      - | 2760 | `/*` |
|      - | 2761 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 2762 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 2763 | ` * Parameters` |
|      - | 2764 | ` *  $str` |
|      - | 2765 | ` *   The string that will be trimmed.` |
|      - | 2766 | ` * $charlist` |
|      - | 2767 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2768 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2769 | ` *   With .. you can specify a range of characters.` |
|      - | 2770 | ` * Returns.` |
|      - | 2771 | ` *  Thr processed string.` |
|      - | 2772 | ` * NOTE:` |
|      - | 2773 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 2774 | ` */` |
|     26 | 2775 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2776 |  |
|      - | 2777 | `	const char *zString;` |
|      - | 2778 | `	int nLen;` |
|     27 | 2779 | `	if( nArg < 1 ){` |
|      - | 2780 | `		/* Missing arguments,return null */` |
|      3 | 2781 | `		ph7_result_null(pCtx);` |
|      3 | 2782 | `		return PH7_OK;` |
|      - | 2783 | `	}` |
|      - | 2784 | `	/* Extract the target string */` |
|     25 | 2785 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 2786 | `	if( nLen < 1 ){` |
|      - | 2787 | `		/* Empty string,return */` |
|      5 | 2788 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2789 | `		return PH7_OK;` |
|      - | 2790 | `	}` |
|      - | 2791 | `	/* Start the trim process */` |
|     21 | 2792 | `	if( nArg < 2 ){` |
|      - | 2793 | `		SyString sStr;` |
|      - | 2794 | `		/* Remove white spaces and NUL bytes*/` |
|     17 | 2795 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     41 | 2796 | `		SyStringRightTrimSafe(&sStr);` |
|     17 | 2797 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      9 | 2798 | `	}else{` |
|      - | 2799 | `		/* Char list */` |
|      - | 2800 | `		const char *zList;` |
|      - | 2801 | `		int nListlen;` |
|      5 | 2802 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 2803 | `		if( nListlen < 1 ){` |
|      - | 2804 | `			/* Return the string unchanged */` |
|    ! 0 | 2805 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 2806 | `		}else{` |
|      5 | 2807 | `			const char *zEnd = &zString[nLen - 1];` |
|      5 | 2808 | `			const char *zCur = zString;` |
|      - | 2809 | `			const char *zPtr;` |
|      - | 2810 | `			int i;` |
|      - | 2811 | `			/* Right trim */` |
|      6 | 2812 | `			for(;;){` |
|     13 | 2813 | `				if( zEnd <= zCur ){` |
|    ! 0 | 2814 | `					break;` |
|      - | 2815 | `				}` |
|     13 | 2816 | `				zPtr = zEnd;` |
|     25 | 2817 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     13 | 2818 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      9 | 2819 | `						zEnd--;` |
|      4 | 2820 | `					}` |
|      7 | 2821 | `				}` |
|     13 | 2822 | `				if( zEnd == zPtr ){` |
|      5 | 2823 | `					break;` |
|      - | 2824 | `				}` |
|      1 | 2825 | `			}` |
|      5 | 2826 | `			if( zEnd <= zCur ){` |
|      - | 2827 | `				/* Return the empty string */` |
|    ! 0 | 2828 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2829 | `			}else{` |
|      5 | 2830 | `				zEnd++;` |
|      5 | 2831 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2832 | `			}` |
|      - | 2833 | `		}` |
|      - | 2834 | `	}` |
|     21 | 2835 | `	return PH7_OK;` |
|     14 | 2836 |  |
|      - | 2837 | `/*` |
|      - | 2838 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 2839 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2840 | ` * Parameters` |
|      - | 2841 | ` *  $str` |
|      - | 2842 | ` *   The string that will be trimmed.` |
|      - | 2843 | ` * $charlist` |
|      - | 2844 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2845 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2846 | ` *   With .. you can specify a range of characters.` |
|      - | 2847 | ` * Returns.` |
|      - | 2848 | ` *  Thr processed string.` |
|      - | 2849 | ` * NOTE:` |
|      - | 2850 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 2851 | ` */` |
|     12 | 2852 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2853 |  |
|      - | 2854 | `	const char *zString;` |
|      - | 2855 | `	int nLen;` |
|     13 | 2856 | `	if( nArg < 1 ){` |
|      - | 2857 | `		/* Missing arguments,return null */` |
|      3 | 2858 | `		ph7_result_null(pCtx);` |
|      3 | 2859 | `		return PH7_OK;` |
|      - | 2860 | `	}` |
|      - | 2861 | `	/* Extract the target string */` |
|     11 | 2862 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 2863 | `	if( nLen < 1 ){` |
|      - | 2864 | `		/* Empty string,return */` |
|    ! 0 | 2865 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2866 | `		return PH7_OK;` |
|      - | 2867 | `	}` |
|      - | 2868 | `	/* Start the trim process */` |
|     11 | 2869 | `	if( nArg < 2 ){` |
|      - | 2870 | `		SyString sStr;` |
|      - | 2871 | `		/* Remove white spaces and NUL byte */` |
|      3 | 2872 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|      8 | 2873 | `		SyStringLeftTrimSafe(&sStr);` |
|      3 | 2874 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      2 | 2875 | `	}else{` |
|      - | 2876 | `		/* Char list */` |
|      - | 2877 | `		const char *zList;` |
|      - | 2878 | `		int nListlen;` |
|      9 | 2879 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      9 | 2880 | `		if( nListlen < 1 ){` |
|      - | 2881 | `			/* Return the string unchanged */` |
|      3 | 2882 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2883 | `		}else{` |
|      7 | 2884 | `			const char *zEnd = &zString[nLen];` |
|      7 | 2885 | `			const char *zCur = zString;` |
|      - | 2886 | `			const char *zPtr;` |
|      - | 2887 | `			int i;` |
|      - | 2888 | `			/* Left trim */` |
|      7 | 2889 | `			for(;;){` |
|     15 | 2890 | `				if( zCur >= zEnd ){` |
|    ! 0 | 2891 | `					break;` |
|      - | 2892 | `				}` |
|     15 | 2893 | `				zPtr = zCur;` |
|     41 | 2894 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     27 | 2895 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|     13 | 2896 | `						zCur++;` |
|      6 | 2897 | `					}` |
|     14 | 2898 | `				}` |
|     15 | 2899 | `				if( zCur == zPtr ){` |
|      - | 2900 | `					/* No match,break immediately */` |
|      7 | 2901 | `					break;` |
|      - | 2902 | `				}` |
|      1 | 2903 | `			}` |
|      7 | 2904 | `			if( zCur >= zEnd ){` |
|      - | 2905 | `				/* Return the empty string */` |
|    ! 0 | 2906 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2907 | `			}else{` |
|      7 | 2908 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2909 | `			}` |
|      - | 2910 | `		}` |
|      - | 2911 | `	}` |
|     11 | 2912 | `	return PH7_OK;` |
|      7 | 2913 |  |
|      - | 2914 | `/*` |
|      - | 2915 | ` * string strtolower(string $str)` |
|      - | 2916 | ` *  Make a string lowercase.` |
|      - | 2917 | ` * Parameters` |
|      - | 2918 | ` *  $str` |
|      - | 2919 | ` *   The input string.` |
|      - | 2920 | ` * Returns.` |
|      - | 2921 | ` *  The lowercased string.` |
|      - | 2922 | ` */` |
|  17086 | 2923 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2924 |  |
|      - | 2925 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2926 | `	int nLen;` |
|  17088 | 2927 | `	if( nArg < 1 ){` |
|      - | 2928 | `		/* Missing arguments,return null */` |
|      3 | 2929 | `		ph7_result_null(pCtx);` |
|      3 | 2930 | `		return PH7_OK;` |
|      - | 2931 | `	}` |
|      - | 2932 | `	/* Extract the target string */` |
|  17086 | 2933 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  17086 | 2934 | `	if( nLen < 1 ){` |
|      - | 2935 | `		/* Empty string,return */` |
|      3 | 2936 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2937 | `		return PH7_OK;` |
|      - | 2938 | `	}` |
|      - | 2939 | `	/* Perform the requested operation */` |
|  17084 | 2940 | `	zEnd = &zString[nLen];` |
|  54011 | 2941 | `	for(;;){` |
| 108024 | 2942 | `		if( zString >= zEnd ){` |
|      - | 2943 | `			/* No more input,break immediately */` |
|  17084 | 2944 | `			break;` |
|      - | 2945 | `		}` |
|  90942 | 2946 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2947 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2948 | `			zCur = zString;` |
|    ! 0 | 2949 | `			zString++;` |
|    ! 0 | 2950 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2951 | `				zString++;` |
|    ! 0 | 2952 | `			}` |
|      - | 2953 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2954 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2955 | `		}else{` |
|  90942 | 2956 | `			int c = zString[0];` |
|  90942 | 2957 | `			if( SyisUpper(c) ){` |
|  90940 | 2958 | `				c = SyToLower(zString[0]);` |
|  45469 | 2959 | `			}` |
|      - | 2960 | `			/* Append character */` |
|  90942 | 2961 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2962 | `			/* Advance the cursor */` |
|  90942 | 2963 | `			zString++;` |
|      - | 2964 | `		}` |
|      2 | 2965 | `	}` |
|  17084 | 2966 | `	return PH7_OK;` |
|   8545 | 2967 |  |
|      - | 2968 | `/*` |
|      - | 2969 | ` * string strtolower(string $str)` |
|      - | 2970 | ` *  Make a string uppercase.` |
|      - | 2971 | ` * Parameters` |
|      - | 2972 | ` *  $str` |
|      - | 2973 | ` *   The input string.` |
|      - | 2974 | ` * Returns.` |
|      - | 2975 | ` *  The uppercased string.` |
|      - | 2976 | ` */` |
|     10 | 2977 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2978 |  |
|      - | 2979 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2980 | `	int nLen;` |
|     11 | 2981 | `	if( nArg < 1 ){` |
|      - | 2982 | `		/* Missing arguments,return null */` |
|      3 | 2983 | `		ph7_result_null(pCtx);` |
|      3 | 2984 | `		return PH7_OK;` |
|      - | 2985 | `	}` |
|      - | 2986 | `	/* Extract the target string */` |
|      9 | 2987 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 2988 | `	if( nLen < 1 ){` |
|      - | 2989 | `		/* Empty string,return */` |
|      3 | 2990 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2991 | `		return PH7_OK;` |
|      - | 2992 | `	}` |
|      - | 2993 | `	/* Perform the requested operation */` |
|      7 | 2994 | `	zEnd = &zString[nLen];` |
|     19 | 2995 | `	for(;;){` |
|     39 | 2996 | `		if( zString >= zEnd ){` |
|      - | 2997 | `			/* No more input,break immediately */` |
|      7 | 2998 | `			break;` |
|      - | 2999 | `		}` |
|     33 | 3000 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 3001 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 3002 | `			zCur = zString;` |
|    ! 0 | 3003 | `			zString++;` |
|    ! 0 | 3004 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 3005 | `				zString++;` |
|    ! 0 | 3006 | `			}` |
|      - | 3007 | `			/* Append UTF-8 stream */` |
|    ! 0 | 3008 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 3009 | `		}else{` |
|     33 | 3010 | `			int c = zString[0];` |
|     33 | 3011 | `			if( SyisLower(c) ){` |
|     27 | 3012 | `				c = SyToUpper(zString[0]);` |
|     13 | 3013 | `			}` |
|      - | 3014 | `			/* Append character */` |
|     33 | 3015 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 3016 | `			/* Advance the cursor */` |
|     33 | 3017 | `			zString++;` |
|      - | 3018 | `		}` |
|      1 | 3019 | `	}` |
|      7 | 3020 | `	return PH7_OK;` |
|      6 | 3021 |  |
|      - | 3022 | `/*` |
|      - | 3023 | ` * string ucfirst(string $str)` |
|      - | 3024 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 3025 | ` *  character is alphabetic.` |
|      - | 3026 | ` * Parameters` |
|      - | 3027 | ` *  $str` |
|      - | 3028 | ` *   The input string.` |
|      - | 3029 | ` * Returns.` |
|      - | 3030 | ` *  The processed string.` |
|      - | 3031 | ` */` |
|      6 | 3032 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3033 |  |
|      - | 3034 | `	const char *zString,*zEnd;` |
|      - | 3035 | `	int nLen,c;` |
|      7 | 3036 | `	if( nArg < 1 ){` |
|      - | 3037 | `		/* Missing arguments,return null */` |
|      3 | 3038 | `		ph7_result_null(pCtx);` |
|      3 | 3039 | `		return PH7_OK;` |
|      - | 3040 | `	}` |
|      - | 3041 | `	/* Extract the target string */` |
|      5 | 3042 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3043 | `	if( nLen < 1 ){` |
|      - | 3044 | `		/* Empty string,return */` |
|      3 | 3045 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3046 | `		return PH7_OK;` |
|      - | 3047 | `	}` |
|      - | 3048 | `	/* Perform the requested operation */` |
|      3 | 3049 | `	zEnd = &zString[nLen];` |
|      3 | 3050 | `	c = zString[0];` |
|      3 | 3051 | `	if( SyisLower(c) ){` |
|      3 | 3052 | `		c = SyToUpper(c);` |
|      1 | 3053 | `	}` |
|      - | 3054 | `	/* Append the first character */` |
|      3 | 3055 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 3056 | `	zString++;` |
|      3 | 3057 | `	if( zString < zEnd ){` |
|      - | 3058 | `		/* Append the rest of the input verbatim */` |
|      3 | 3059 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 3060 | `	}` |
|      3 | 3061 | `	return PH7_OK;` |
|      4 | 3062 |  |
|      - | 3063 | `/*` |
|      - | 3064 | ` * string lcfirst(string $str)` |
|      - | 3065 | ` *  Make a string's first character lowercase.` |
|      - | 3066 | ` * Parameters` |
|      - | 3067 | ` *  $str` |
|      - | 3068 | ` *   The input string.` |
|      - | 3069 | ` * Returns.` |
|      - | 3070 | ` *  The processed string.` |
|      - | 3071 | ` */` |
|      6 | 3072 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3073 |  |
|      - | 3074 | `	const char *zString,*zEnd;` |
|      - | 3075 | `	int nLen,c;` |
|      7 | 3076 | `	if( nArg < 1 ){` |
|      - | 3077 | `		/* Missing arguments,return null */` |
|      3 | 3078 | `		ph7_result_null(pCtx);` |
|      3 | 3079 | `		return PH7_OK;` |
|      - | 3080 | `	}` |
|      - | 3081 | `	/* Extract the target string */` |
|      5 | 3082 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3083 | `	if( nLen < 1 ){` |
|      - | 3084 | `		/* Empty string,return */` |
|      3 | 3085 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3086 | `		return PH7_OK;` |
|      - | 3087 | `	}` |
|      - | 3088 | `	/* Perform the requested operation */` |
|      3 | 3089 | `	zEnd = &zString[nLen];` |
|      3 | 3090 | `	c = zString[0];` |
|      3 | 3091 | `	if( SyisUpper(c) ){` |
|      3 | 3092 | `		c = SyToLower(c);` |
|      1 | 3093 | `	}` |
|      - | 3094 | `	/* Append the first character */` |
|      3 | 3095 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 3096 | `	zString++;` |
|      3 | 3097 | `	if( zString < zEnd ){` |
|      - | 3098 | `		/* Append the rest of the input verbatim */` |
|      3 | 3099 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 3100 | `	}` |
|      3 | 3101 | `	return PH7_OK;` |
|      4 | 3102 |  |
|      - | 3103 | `/*` |
|      - | 3104 | ` * int ord(string $string)` |
|      - | 3105 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 3106 | ` * Parameters` |
|      - | 3107 | ` *  $str` |
|      - | 3108 | ` *   The input string.` |
|      - | 3109 | ` * Returns.` |
|      - | 3110 | ` *  The ASCII value as an integer.` |
|      - | 3111 | ` */` |
|     32 | 3112 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3113 |  |
|      - | 3114 | `	const char *zString;` |
|      - | 3115 | `	int nLen,c;` |
|     33 | 3116 | `	if( nArg < 1 ){` |
|      - | 3117 | `		/* Missing arguments,return -1 */` |
|      3 | 3118 | `		ph7_result_int(pCtx,-1);` |
|      3 | 3119 | `		return PH7_OK;` |
|      - | 3120 | `	}` |
|      - | 3121 | `	/* Extract the target string */` |
|     31 | 3122 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 3123 | `	if( nLen < 1 ){` |
|      - | 3124 | `		/* Empty string,return -1 */` |
|      3 | 3125 | `		ph7_result_int(pCtx,-1);` |
|      3 | 3126 | `		return PH7_OK;` |
|      - | 3127 | `	}` |
|      - | 3128 | `	/* Extract the ASCII value of the first character */` |
|     29 | 3129 | `	c = zString[0];` |
|      - | 3130 | `	/* Return that value */` |
|     29 | 3131 | `	ph7_result_int(pCtx,c);` |
|     29 | 3132 | `	return PH7_OK;` |
|     17 | 3133 |  |
|      - | 3134 | `/*` |
|      - | 3135 | ` * string chr(int $ascii)` |
|      - | 3136 | ` *  Returns a one-character string containing the character specified by ascii.` |
|      - | 3137 | ` * Parameters` |
|      - | 3138 | ` *  $ascii` |
|      - | 3139 | ` *   The ascii code.` |
|      - | 3140 | ` * Returns.` |
|      - | 3141 | ` *  The specified character.` |
|      - | 3142 | ` */` |
|     28 | 3143 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3144 |  |
|      - | 3145 | `	int c;` |
|     29 | 3146 | `	if( nArg < 1 ){` |
|      - | 3147 | `		/* Missing arguments,return null */` |
|      3 | 3148 | `		ph7_result_null(pCtx);` |
|      3 | 3149 | `		return PH7_OK;` |
|      - | 3150 | `	}` |
|      - | 3151 | `	/* Extract the ASCII value */` |
|     27 | 3152 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 3153 | `	/* Return the specified character */` |
|     27 | 3154 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     27 | 3155 | `	return PH7_OK;` |
|     15 | 3156 |  |
|      - | 3157 | `/*` |
|      - | 3158 | ` * Binary to hex consumer callback.` |
|      - | 3159 | ` * This callback is the default consumer used by the hash functions` |
|      - | 3160 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 3161 | ` */` |
|    226 | 3162 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 3163 |  |
|      - | 3164 | `	/* Append hex chunk verbatim */` |
|    227 | 3165 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|    227 | 3166 | `	return SXRET_OK;` |
|      1 | 3167 |  |
|      - | 3168 |  |
|      - | 3169 | `/*` |
|      - | 3170 | ` * string bin2hex(string $str)` |
|      - | 3171 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 3172 | ` * Parameters` |
|      - | 3173 | ` *  $str` |
|      - | 3174 | ` *   The input string.` |
|      - | 3175 | ` * Returns.` |
|      - | 3176 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 3177 | ` */` |
|     12 | 3178 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3179 |  |
|      - | 3180 | `	const char *zString;` |
|      - | 3181 | `	int nLen;` |
|     13 | 3182 | `	if( nArg < 1 ){` |
|      - | 3183 | `		/* Missing arguments,return null */` |
|      3 | 3184 | `		ph7_result_null(pCtx);` |
|      3 | 3185 | `		return PH7_OK;` |
|      - | 3186 | `	}` |
|      - | 3187 | `	/* Extract the target string */` |
|     11 | 3188 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 3189 | `	if( nLen < 1 ){` |
|      - | 3190 | `		/* Empty string,return */` |
|      3 | 3191 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3192 | `		return PH7_OK;` |
|      - | 3193 | `	}` |
|      - | 3194 | `	/* Perform the requested operation */` |
|      9 | 3195 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|      9 | 3196 | `	return PH7_OK;` |
|      7 | 3197 |  |
|      - | 3198 |  |
|      - | 3199 | `/* Search callback signature */` |
|      - | 3200 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 3201 | `/*` |
|      - | 3202 | ` * Case-insensitive pattern match.` |
|      - | 3203 | ` * Brute force is the default search method used here.` |
|      - | 3204 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 3205 | ` * well for short/medium texts on modern hardware.` |
|      - | 3206 | ` */` |
|    118 | 3207 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      1 | 3208 |  |
|    119 | 3209 | `	const char *zpIn = (const char *)pPattern;` |
|    119 | 3210 | `	const char *zIn = (const char *)pText;` |
|    119 | 3211 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    119 | 3212 | `	const char *zEnd = &zIn[nLen];` |
|      - | 3213 | `	const char *zPtr,*zPtr2;` |
|      - | 3214 | `	int c,d;` |
|    119 | 3215 | `	if( iPatLen > nLen ){` |
|      - | 3216 | `		/* Don't bother processing */` |
|     33 | 3217 | `		return SXERR_NOTFOUND;` |
|      - | 3218 | `	}` |
|    244 | 3219 | `	for(;;){` |
|    489 | 3220 | `		if( zIn >= zEnd ){` |
|     47 | 3221 | `			break;` |
|      - | 3222 | `		}` |
|    443 | 3223 | `		c = SyToLower(zIn[0]);` |
|    443 | 3224 | `		d = SyToLower(zpIn[0]);` |
|    443 | 3225 | `		if( c == d ){` |
|     41 | 3226 | `			zPtr   = &zIn[1];` |
|     41 | 3227 | `			zPtr2  = &zpIn[1];` |
|     71 | 3228 | `			for(;;){` |
|    143 | 3229 | `				if( zPtr2 >= zpEnd ){` |
|      - | 3230 | `					/* Pattern found */` |
|     41 | 3231 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 3232 | `					return SXRET_OK;` |
|      - | 3233 | `				}` |
|    103 | 3234 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 3235 | `					break;` |
|      - | 3236 | `				}` |
|    103 | 3237 | `				c = SyToLower(zPtr[0]);` |
|    103 | 3238 | `				d = SyToLower(zPtr2[0]);` |
|    103 | 3239 | `				if( c != d ){` |
|    ! 0 | 3240 | `					break;` |
|      - | 3241 | `				}` |
|    103 | 3242 | `				zPtr++; zPtr2++;` |
|      1 | 3243 | `			}` |
|    ! 0 | 3244 | `		}` |
|    403 | 3245 | `		zIn++;` |
|      1 | 3246 | `	}` |
|      - | 3247 | `	/* Pattern not found */` |
|     47 | 3248 | `	return SXERR_NOTFOUND;` |
|     60 | 3249 |  |
|      - | 3250 | `/*` |
|      - | 3251 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3252 | ` *  Find the first occurrence of a string.` |
|      - | 3253 | ` * Parameters` |
|      - | 3254 | ` *  $haystack` |
|      - | 3255 | ` *   The input string.` |
|      - | 3256 | ` * $needle` |
|      - | 3257 | ` *   Search pattern (must be a string).` |
|      - | 3258 | ` * $before_needle` |
|      - | 3259 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3260 | ` *   of the needle (excluding the needle).` |
|      - | 3261 | ` * Return` |
|      - | 3262 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3263 | ` */` |
|     10 | 3264 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3265 |  |
|     11 | 3266 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3267 | `	const char *zBlob,*zPattern;` |
|      - | 3268 | `	int nLen,nPatLen;` |
|      - | 3269 | `	sxu32 nOfft;` |
|      - | 3270 | `	sxi32 rc;` |
|     11 | 3271 | `	if( nArg < 2 ){` |
|      - | 3272 | `		/* Missing arguments,return FALSE */` |
|      5 | 3273 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3274 | `		return PH7_OK;` |
|      - | 3275 | `	}` |
|      - | 3276 | `	/* Extract the needle and the haystack */` |
|      7 | 3277 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 3278 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 3279 | `	nOfft = 0; /* cc warning */` |
|      9 | 3280 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3281 | `		int before = 0;` |
|      - | 3282 | `		/* Perform the lookup */` |
|      5 | 3283 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3284 | `		if( rc != SXRET_OK ){` |
|      - | 3285 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3286 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3287 | `			return PH7_OK;` |
|      - | 3288 | `		}` |
|      - | 3289 | `		/* Return the portion of the string */` |
|      5 | 3290 | `		if( nArg > 2 ){` |
|      3 | 3291 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3292 | `		}` |
|      5 | 3293 | `		if( before ){` |
|      3 | 3294 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3295 | `		}else{` |
|      3 | 3296 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3297 | `		}` |
|      3 | 3298 | `	}else{` |
|      3 | 3299 | `		ph7_result_bool(pCtx,0);` |
|      - | 3300 | `	}` |
|      7 | 3301 | `	return PH7_OK;` |
|      6 | 3302 |  |
|      - | 3303 | `/*` |
|      - | 3304 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3305 | ` *  Case-insensitive strstr().` |
|      - | 3306 | ` * Parameters` |
|      - | 3307 | ` *  $haystack` |
|      - | 3308 | ` *   The input string.` |
|      - | 3309 | ` * $needle` |
|      - | 3310 | ` *   Search pattern (must be a string).` |
|      - | 3311 | ` * $before_needle` |
|      - | 3312 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3313 | ` *   of the needle (excluding the needle).` |
|      - | 3314 | ` * Return` |
|      - | 3315 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3316 | ` */` |
|      6 | 3317 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3318 |  |
|      7 | 3319 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3320 | `	const char *zBlob,*zPattern;` |
|      - | 3321 | `	int nLen,nPatLen;` |
|      - | 3322 | `	sxu32 nOfft;` |
|      - | 3323 | `	sxi32 rc;` |
|      7 | 3324 | `	if( nArg < 2 ){` |
|      - | 3325 | `		/* Missing arguments,return FALSE */` |
|      3 | 3326 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3327 | `		return PH7_OK;` |
|      - | 3328 | `	}` |
|      - | 3329 | `	/* Extract the needle and the haystack */` |
|      5 | 3330 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3331 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 3332 | `	nOfft = 0; /* cc warning */` |
|      7 | 3333 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3334 | `		int before = 0;` |
|      - | 3335 | `		/* Perform the lookup */` |
|      5 | 3336 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3337 | `		if( rc != SXRET_OK ){` |
|      - | 3338 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3339 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3340 | `			return PH7_OK;` |
|      - | 3341 | `		}` |
|      - | 3342 | `		/* Return the portion of the string */` |
|      5 | 3343 | `		if( nArg > 2 ){` |
|      3 | 3344 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3345 | `		}` |
|      5 | 3346 | `		if( before ){` |
|      3 | 3347 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3348 | `		}else{` |
|      3 | 3349 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3350 | `		}` |
|      3 | 3351 | `	}else{` |
|    ! 0 | 3352 | `		ph7_result_bool(pCtx,0);` |
|      - | 3353 | `	}` |
|      5 | 3354 | `	return PH7_OK;` |
|      4 | 3355 |  |
|      - | 3356 | `/*` |
|      - | 3357 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3358 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 3359 | ` * Parameters` |
|      - | 3360 | ` *  $haystack` |
|      - | 3361 | ` *   The input string.` |
|      - | 3362 | ` * $needle` |
|      - | 3363 | ` *   Search pattern (must be a string).` |
|      - | 3364 | ` * $offset` |
|      - | 3365 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3366 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3367 | ` *   of haystack.` |
|      - | 3368 | ` * Return` |
|      - | 3369 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3370 | ` */` |
|     80 | 3371 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3372 |  |
|     82 | 3373 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3374 | `	const char *zBlob,*zPattern;` |
|      - | 3375 | `	int nLen,nPatLen,nStart;` |
|      - | 3376 | `	sxu32 nOfft;` |
|      - | 3377 | `	sxi32 rc;` |
|     82 | 3378 | `	if( nArg < 2 ){` |
|      - | 3379 | `		/* Missing arguments,return FALSE */` |
|      7 | 3380 | `		ph7_result_bool(pCtx,0);` |
|      7 | 3381 | `		return PH7_OK;` |
|      - | 3382 | `	}` |
|      - | 3383 | `	/* Extract the needle and the haystack */` |
|     76 | 3384 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     76 | 3385 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     76 | 3386 | `	nOfft = 0; /* cc warning */` |
|     76 | 3387 | `	nStart = 0;` |
|      - | 3388 | `	/* Peek the starting offset if available */` |
|     76 | 3389 | `	if( nArg > 2 ){` |
|    ! 0 | 3390 | `		nStart = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 3391 | `		if( nStart < 0 ){` |
|    ! 0 | 3392 | `			nStart = -nStart;` |
|    ! 0 | 3393 | `		}` |
|    ! 0 | 3394 | `		if( nStart >= nLen ){` |
|      - | 3395 | `			/* Invalid offset */` |
|    ! 0 | 3396 | `			nStart = 0;` |
|    ! 0 | 3397 | `		}else{` |
|    ! 0 | 3398 | `			zBlob += nStart;` |
|    ! 0 | 3399 | `			nLen -= nStart;` |
|      - | 3400 | `		}` |
|    ! 0 | 3401 | `	}` |
|     76 | 3402 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3403 | `		/* Perform the lookup */` |
|     74 | 3404 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     74 | 3405 | `		if( rc != SXRET_OK ){` |
|      - | 3406 | `			/* Pattern not found,return FALSE */` |
|      3 | 3407 | `			ph7_result_bool(pCtx,0);` |
|      3 | 3408 | `			return PH7_OK;` |
|      - | 3409 | `		}` |
|      - | 3410 | `		/* Return the pattern position */` |
|     72 | 3411 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|     37 | 3412 | `	}else{` |
|      3 | 3413 | `		ph7_result_bool(pCtx,0);` |
|      - | 3414 | `	}` |
|     74 | 3415 | `	return PH7_OK;` |
|     42 | 3416 |  |
|      - | 3417 | `/*` |
|      - | 3418 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3419 | ` *  Case-insensitive strpos.` |
|      - | 3420 | ` * Parameters` |
|      - | 3421 | ` *  $haystack` |
|      - | 3422 | ` *   The input string.` |
|      - | 3423 | ` * $needle` |
|      - | 3424 | ` *   Search pattern (must be a string).` |
|      - | 3425 | ` * $offset` |
|      - | 3426 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3427 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3428 | ` *   of haystack.` |
|      - | 3429 | ` * Return` |
|      - | 3430 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3431 | ` */` |
|     18 | 3432 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3433 |  |
|     19 | 3434 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3435 | `	const char *zBlob,*zPattern;` |
|      - | 3436 | `	int nLen,nPatLen,nStart;` |
|      - | 3437 | `	sxu32 nOfft;` |
|      - | 3438 | `	sxi32 rc;` |
|     19 | 3439 | `	if( nArg < 2 ){` |
|      - | 3440 | `		/* Missing arguments,return FALSE */` |
|      3 | 3441 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3442 | `		return PH7_OK;` |
|      - | 3443 | `	}` |
|      - | 3444 | `	/* Extract the needle and the haystack */` |
|     17 | 3445 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 3446 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     17 | 3447 | `	nOfft = 0; /* cc warning */` |
|     17 | 3448 | `	nStart = 0;` |
|      - | 3449 | `	/* Peek the starting offset if available */` |
|     17 | 3450 | `	if( nArg > 2 ){` |
|      5 | 3451 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 3452 | `		if( nStart < 0 ){` |
|      3 | 3453 | `			nStart = -nStart;` |
|      1 | 3454 | `		}` |
|      5 | 3455 | `		if( nStart >= nLen ){` |
|      - | 3456 | `			/* Invalid offset */` |
|    ! 0 | 3457 | `			nStart = 0;` |
|    ! 0 | 3458 | `		}else{` |
|      5 | 3459 | `			zBlob += nStart;` |
|      5 | 3460 | `			nLen -= nStart;` |
|      - | 3461 | `		}` |
|      2 | 3462 | `	}` |
|     17 | 3463 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3464 | `		/* Perform the lookup */` |
|     17 | 3465 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     17 | 3466 | `		if( rc != SXRET_OK ){` |
|      - | 3467 | `			/* Pattern not found,return FALSE */` |
|      3 | 3468 | `			ph7_result_bool(pCtx,0);` |
|      3 | 3469 | `			return PH7_OK;` |
|      - | 3470 | `		}` |
|      - | 3471 | `		/* Return the pattern position */` |
|     15 | 3472 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 3473 | `	}else{` |
|    ! 0 | 3474 | `		ph7_result_bool(pCtx,0);` |
|      - | 3475 | `	}` |
|     15 | 3476 | `	return PH7_OK;` |
|     10 | 3477 |  |
|      - | 3478 | `/*` |
|      - | 3479 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3480 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 3481 | ` * Parameters` |
|      - | 3482 | ` *  $haystack` |
|      - | 3483 | ` *   The input string.` |
|      - | 3484 | ` * $needle` |
|      - | 3485 | ` *   Search pattern (must be a string).` |
|      - | 3486 | ` * $offset` |
|      - | 3487 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3488 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3489 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3490 | ` * Return` |
|      - | 3491 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3492 | ` */` |
|     32 | 3493 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3494 |  |
|      - | 3495 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     33 | 3496 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3497 | `	int nLen,nPatLen;` |
|      - | 3498 | `	sxu32 nOfft;` |
|      - | 3499 | `	sxi32 rc;` |
|     33 | 3500 | `	if( nArg < 2 ){` |
|      - | 3501 | `		/* Missing arguments,return FALSE */` |
|      3 | 3502 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3503 | `		return PH7_OK;` |
|      - | 3504 | `	}` |
|      - | 3505 | `	/* Extract the needle and the haystack */` |
|     31 | 3506 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 3507 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3508 | `	/* Point to the end of the pattern */` |
|     31 | 3509 | `	zPtr = &zBlob[nLen - 1];` |
|     31 | 3510 | `	zEnd = &zBlob[nLen];` |
|      - | 3511 | `	/* Save the starting posistion */` |
|     31 | 3512 | `	zStart = zBlob;` |
|     31 | 3513 | `	nOfft = 0; /* cc warning */` |
|      - | 3514 | `	/* Peek the starting offset if available */` |
|     31 | 3515 | `	if( nArg > 2 ){` |
|      - | 3516 | `		int nStart;` |
|     21 | 3517 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 3518 | `		if( nStart < 0 ){` |
|     11 | 3519 | `			nStart = -nStart;` |
|     11 | 3520 | `			if( nStart >= nLen ){` |
|      - | 3521 | `				/* Invalid offset */` |
|      3 | 3522 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3523 | `				return PH7_OK;` |
|    ! 0 | 3524 | `			}else{` |
|      9 | 3525 | `				nLen -= nStart;` |
|      9 | 3526 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 3527 | `				zEnd = &zBlob[nLen];` |
|      - | 3528 | `			}` |
|      5 | 3529 | `		}else{` |
|     11 | 3530 | `			if( nStart >= nLen ){` |
|      - | 3531 | `				/* Invalid offset */` |
|      5 | 3532 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3533 | `				return PH7_OK;` |
|    ! 0 | 3534 | `			}else{` |
|      7 | 3535 | `				zBlob += nStart;` |
|      7 | 3536 | `				nLen -= nStart;` |
|      - | 3537 | `			}` |
|      - | 3538 | `		}` |
|      7 | 3539 | `	}` |
|     25 | 3540 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3541 | `		/* Perform the lookup */` |
|     57 | 3542 | `		for(;;){` |
|    115 | 3543 | `			if( zBlob >= zPtr ){` |
|     11 | 3544 | `				break;` |
|      - | 3545 | `			}` |
|    105 | 3546 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    105 | 3547 | `			if( rc == SXRET_OK ){` |
|      - | 3548 | `				/* Pattern found,return it's position */` |
|     13 | 3549 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 3550 | `				return PH7_OK;` |
|      - | 3551 | `			}` |
|     93 | 3552 | `			zPtr--;` |
|      1 | 3553 | `		}` |
|      - | 3554 | `		/* Pattern not found,return FALSE */` |
|     11 | 3555 | `		ph7_result_bool(pCtx,0);` |
|      6 | 3556 | `	}else{` |
|      3 | 3557 | `		ph7_result_bool(pCtx,0);` |
|      - | 3558 | `	}` |
|     13 | 3559 | `	return PH7_OK;` |
|     17 | 3560 |  |
|      - | 3561 | `/*` |
|      - | 3562 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3563 | ` *  Case-insensitive strrpos.` |
|      - | 3564 | ` * Parameters` |
|      - | 3565 | ` *  $haystack` |
|      - | 3566 | ` *   The input string.` |
|      - | 3567 | ` * $needle` |
|      - | 3568 | ` *   Search pattern (must be a string).` |
|      - | 3569 | ` * $offset` |
|      - | 3570 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3571 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3572 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3573 | ` * Return` |
|      - | 3574 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3575 | ` */` |
|     28 | 3576 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3577 |  |
|      - | 3578 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     29 | 3579 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3580 | `	int nLen,nPatLen;` |
|      - | 3581 | `	sxu32 nOfft;` |
|      - | 3582 | `	sxi32 rc;` |
|     29 | 3583 | `	if( nArg < 2 ){` |
|      - | 3584 | `		/* Missing arguments,return FALSE */` |
|      3 | 3585 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3586 | `		return PH7_OK;` |
|      - | 3587 | `	}` |
|      - | 3588 | `	/* Extract the needle and the haystack */` |
|     27 | 3589 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 3590 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3591 | `	/* Point to the end of the pattern */` |
|     27 | 3592 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 3593 | `	zEnd = &zBlob[nLen];` |
|      - | 3594 | `	/* Save the starting posistion */` |
|     27 | 3595 | `	zStart = zBlob;` |
|     27 | 3596 | `	nOfft = 0; /* cc warning */` |
|      - | 3597 | `	/* Peek the starting offset if available */` |
|     27 | 3598 | `	if( nArg > 2 ){` |
|      - | 3599 | `		int nStart;` |
|     15 | 3600 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3601 | `		if( nStart < 0 ){` |
|      7 | 3602 | `			nStart = -nStart;` |
|      7 | 3603 | `			if( nStart >= nLen ){` |
|      - | 3604 | `				/* Invalid offset */` |
|      3 | 3605 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3606 | `				return PH7_OK;` |
|    ! 0 | 3607 | `			}else{` |
|      5 | 3608 | `				nLen -= nStart;` |
|      5 | 3609 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 3610 | `				zEnd = &zBlob[nLen];` |
|      - | 3611 | `			}` |
|      3 | 3612 | `		}else{` |
|      9 | 3613 | `			if( nStart >= nLen ){` |
|      - | 3614 | `				/* Invalid offset */` |
|      5 | 3615 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3616 | `				return PH7_OK;` |
|    ! 0 | 3617 | `			}else{` |
|      5 | 3618 | `				zBlob += nStart;` |
|      5 | 3619 | `				nLen -= nStart;` |
|      - | 3620 | `			}` |
|      - | 3621 | `		}` |
|      4 | 3622 | `	}` |
|     21 | 3623 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3624 | `		/* Perform the lookup */` |
|     44 | 3625 | `		for(;;){` |
|     89 | 3626 | `			if( zBlob >= zPtr ){` |
|      9 | 3627 | `				break;` |
|      - | 3628 | `			}` |
|     81 | 3629 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 3630 | `			if( rc == SXRET_OK ){` |
|      - | 3631 | `				/* Pattern found,return it's position */` |
|     11 | 3632 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 3633 | `				return PH7_OK;` |
|      - | 3634 | `			}` |
|     71 | 3635 | `			zPtr--;` |
|      1 | 3636 | `		}` |
|      - | 3637 | `		/* Pattern not found,return FALSE */` |
|      9 | 3638 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3639 | `	}else{` |
|      3 | 3640 | `		ph7_result_bool(pCtx,0);` |
|      - | 3641 | `	}` |
|     11 | 3642 | `	return PH7_OK;` |
|     15 | 3643 |  |
|      - | 3644 | `/*` |
|      - | 3645 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3646 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3647 | ` * Parameters` |
|      - | 3648 | ` *  $haystack` |
|      - | 3649 | ` *   The input string.` |
|      - | 3650 | ` * $needle` |
|      - | 3651 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3652 | ` *  This behavior is different from that of strstr().` |
|      - | 3653 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3654 | ` *  as the ordinal value of a character.` |
|      - | 3655 | ` * Return` |
|      - | 3656 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3657 | ` */` |
|     24 | 3658 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3659 |  |
|      - | 3660 | `	const char *zBlob;` |
|      - | 3661 | `	int nLen,c;` |
|     25 | 3662 | `	if( nArg < 2 ){` |
|      - | 3663 | `		/* Missing arguments,return FALSE */` |
|      3 | 3664 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3665 | `		return PH7_OK;` |
|      - | 3666 | `	}` |
|      - | 3667 | `	/* Extract the haystack */` |
|     23 | 3668 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3669 | `	c = 0; /* cc warning */` |
|     23 | 3670 | `	if( nLen > 0 ){` |
|      - | 3671 | `		sxu32 nOfft;` |
|      - | 3672 | `		sxi32 rc;` |
|     21 | 3673 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 3674 | `			const char *zPattern;` |
|     11 | 3675 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 3676 | `														 * for NULL pointer.` |
|      - | 3677 | `														 */` |
|     11 | 3678 | `			c = zPattern[0];` |
|      6 | 3679 | `		}else{` |
|      - | 3680 | `			/* Int cast */` |
|     11 | 3681 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 3682 | `		}` |
|      - | 3683 | `		/* Perform the lookup */` |
|     21 | 3684 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3685 | `		if( rc != SXRET_OK ){` |
|      - | 3686 | `			/* No such entry,return FALSE */` |
|      7 | 3687 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3688 | `			return PH7_OK;` |
|      - | 3689 | `		}` |
|      - | 3690 | `		/* Return the string portion */` |
|     15 | 3691 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 3692 | `	}else{` |
|      3 | 3693 | `		ph7_result_bool(pCtx,0);` |
|      - | 3694 | `	}` |
|     17 | 3695 | `	return PH7_OK;` |
|     13 | 3696 |  |
|      - | 3697 | `/*` |
|      - | 3698 | ` * string strrev(string $string)` |
|      - | 3699 | ` *  Reverse a string.` |
|      - | 3700 | ` * Parameters` |
|      - | 3701 | ` *  $string` |
|      - | 3702 | ` *   String to be reversed.` |
|      - | 3703 | ` * Return` |
|      - | 3704 | ` *  The reversed string.` |
|      - | 3705 | ` */` |
|      4 | 3706 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3707 |  |
|      - | 3708 | `	const char *zIn,*zEnd;` |
|      - | 3709 | `	int nLen,c;` |
|      5 | 3710 | `	if( nArg < 1 ){` |
|      - | 3711 | `		/* Missing arguments,return NULL */` |
|      3 | 3712 | `		ph7_result_null(pCtx);` |
|      3 | 3713 | `		return PH7_OK;` |
|      - | 3714 | `	}` |
|      - | 3715 | `	/* Extract the target string */` |
|      3 | 3716 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3717 | `	if( nLen < 1 ){` |
|      - | 3718 | `		/* Empty string Return null */` |
|    ! 0 | 3719 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3720 | `		return PH7_OK;` |
|      - | 3721 | `	}` |
|      - | 3722 | `	/* Perform the requested operation */` |
|      3 | 3723 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3724 | `	for(;;){` |
|      9 | 3725 | `		if( zEnd < zIn ){` |
|      - | 3726 | `			/* No more input to process */` |
|      3 | 3727 | `			break;` |
|      - | 3728 | `		}` |
|      - | 3729 | `		/* Append current character */` |
|      7 | 3730 | `		c = zEnd[0];` |
|      7 | 3731 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3732 | `		zEnd--;` |
|      1 | 3733 | `	}` |
|      3 | 3734 | `	return PH7_OK;` |
|      3 | 3735 |  |
|      - | 3736 | `/*` |
|      - | 3737 | ` * string ucwords(string $string)` |
|      - | 3738 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3739 | ` *  The definition of a word is any string of characters that is immediately after` |
|      - | 3740 | ` *  a whitespace (These are: space, form-feed, newline, carriage return, horizontal tab, and vertical tab).` |
|      - | 3741 | ` * Parameters` |
|      - | 3742 | ` *  $string` |
|      - | 3743 | ` *   The input string.` |
|      - | 3744 | ` * Return` |
|      - | 3745 | ` *  The modified string..` |
|      - | 3746 | ` */` |
|     14 | 3747 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3748 |  |
|      - | 3749 | `	const char *zIn,*zCur,*zEnd;` |
|      - | 3750 | `	int nLen,c;` |
|     15 | 3751 | `	if( nArg < 1 ){` |
|      - | 3752 | `		/* Missing arguments,return NULL */` |
|      3 | 3753 | `		ph7_result_null(pCtx);` |
|      3 | 3754 | `		return PH7_OK;` |
|      - | 3755 | `	}` |
|      - | 3756 | `	/* Extract the target string */` |
|     13 | 3757 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 3758 | `	if( nLen < 1 ){` |
|      - | 3759 | `		/* Empty string – match PHP semantics */` |
|      3 | 3760 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3761 | `		return PH7_OK;` |
|      - | 3762 | `	}` |
|      - | 3763 | `	/* Perform the requested operation */` |
|     11 | 3764 | `	zEnd = &zIn[nLen];` |
|     21 | 3765 | `	for(;;){` |
|      - | 3766 | `		/* Jump leading white spaces */` |
|     43 | 3767 | `		zCur = zIn;` |
|     65 | 3768 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0x80 && SyisSpace(zIn[0]) ){` |
|     23 | 3769 | `			zIn++;` |
|      1 | 3770 | `		}` |
|     43 | 3771 | `		if( zCur < zIn ){` |
|      - | 3772 | `			/* Append white space stream */` |
|     23 | 3773 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 3774 | `		}` |
|     43 | 3775 | `		if( zIn >= zEnd ){` |
|      - | 3776 | `			/* No more input to process */` |
|     11 | 3777 | `			break;` |
|      - | 3778 | `		}` |
|     33 | 3779 | `		c = zIn[0];` |
|     33 | 3780 | `		if( c < 0x80 && SyisLower(c) ){` |
|     29 | 3781 | `			c = SyToUpper(c);` |
|     14 | 3782 | `		}` |
|      - | 3783 | `		/* Append the upper-cased character */` |
|     33 | 3784 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     33 | 3785 | `		zIn++;` |
|     33 | 3786 | `		zCur = zIn;` |
|      - | 3787 | `		/* Append the word varbatim */` |
|    149 | 3788 | `		while( zIn < zEnd ){` |
|    139 | 3789 | `			if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 3790 | `				/* UTF-8 stream */` |
|    ! 0 | 3791 | `				zIn++;` |
|    ! 0 | 3792 | `				SX_JMP_UTF8(zIn,zEnd);` |
|    138 | 3793 | `			}else if( !SyisSpace(zIn[0]) ){` |
|    117 | 3794 | `				zIn++;` |
|     59 | 3795 | `			}else{` |
|     23 | 3796 | `				break;` |
|      - | 3797 | `			}` |
|      1 | 3798 | `		}` |
|     33 | 3799 | `		if( zCur < zIn ){` |
|     33 | 3800 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 | 3801 | `		}` |
|      1 | 3802 | `	}` |
|     11 | 3803 | `	return PH7_OK;` |
|      8 | 3804 |  |
|      - | 3805 | `/*` |
|      - | 3806 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3807 | ` *  Returns input repeated multiplier times.` |
|      - | 3808 | ` * Parameters` |
|      - | 3809 | ` *  $string` |
|      - | 3810 | ` *   String to be repeated.` |
|      - | 3811 | ` * $multiplier` |
|      - | 3812 | ` *  Number of time the input string should be repeated.` |
|      - | 3813 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3814 | ` *  to 0, the function will return an empty string.` |
|      - | 3815 | ` * Return` |
|      - | 3816 | ` *  The repeated string.` |
|      - | 3817 | ` */` |
|  20212 | 3818 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3819 |  |
|      - | 3820 | `	const char *zIn;` |
|      - | 3821 | `	int nLen,nMul;` |
|      - | 3822 | `	int rc;` |
|  20213 | 3823 | `	if( nArg < 2 ){` |
|      - | 3824 | `		/* Missing arguments,return NULL */` |
|      3 | 3825 | `		ph7_result_null(pCtx);` |
|      3 | 3826 | `		return PH7_OK;` |
|      - | 3827 | `	}` |
|      - | 3828 | `	/* Extract the target string */` |
|  20211 | 3829 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|  20211 | 3830 | `	if( nLen < 1 ){` |
|      - | 3831 | `		/* Empty string.Return null */` |
|    ! 0 | 3832 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3833 | `		return PH7_OK;` |
|      - | 3834 | `	}` |
|      - | 3835 | `	/* Extract the multiplier */` |
|  20211 | 3836 | `	nMul = ph7_value_to_int(apArg[1]);` |
|  20211 | 3837 | `	if( nMul < 1 ){` |
|      - | 3838 | `		/* Return the empty string */` |
|      3 | 3839 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3840 | `		return PH7_OK;` |
|      - | 3841 | `	}` |
|      - | 3842 | `	/* Perform the requested operation */` |
| 120220 | 3843 | `	for(;;){` |
| 240441 | 3844 | `		if( !nMul ){` |
|  20209 | 3845 | `			break;` |
|      - | 3846 | `		}` |
|      - | 3847 | `		/* Append the copy */` |
| 220233 | 3848 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 220233 | 3849 | `		if( rc != PH7_OK ){` |
|      - | 3850 | `			/* Out of memory,break immediately */` |
|    ! 0 | 3851 | `			break;` |
|      - | 3852 | `		}` |
| 220233 | 3853 | `		nMul--;` |
|      1 | 3854 | `	}` |
|  20209 | 3855 | `	return PH7_OK;` |
|  10107 | 3856 |  |
|      - | 3857 | `/*` |
|      - | 3858 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3859 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3860 | ` * Parameters` |
|      - | 3861 | ` *  $string` |
|      - | 3862 | ` *   The input string.` |
|      - | 3863 | ` * $is_xhtml` |
|      - | 3864 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3865 | ` * Return` |
|      - | 3866 | ` *  The processed string.` |
|      - | 3867 | ` */` |
|      6 | 3868 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3869 |  |
|      - | 3870 | `	const char *zIn,*zCur,*zEnd;` |
|      7 | 3871 | `	int is_xhtml = 1; /* Default to XHTML-style '<br/>' like PHP */` |
|      - | 3872 | `	int nLen;` |
|      7 | 3873 | `	if( nArg < 1 ){` |
|      - | 3874 | `		/* Missing arguments,return the empty string */` |
|      3 | 3875 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3876 | `		return PH7_OK;` |
|      - | 3877 | `	}` |
|      - | 3878 | `	/* Extract the target string */` |
|      5 | 3879 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3880 | `	if( nLen < 1 ){` |
|      - | 3881 | `		/* Empty string,return null */` |
|    ! 0 | 3882 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3883 | `		return PH7_OK;` |
|      - | 3884 | `	}` |
|      5 | 3885 | `	if( nArg > 1 ){` |
|      3 | 3886 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3887 | `	}` |
|      5 | 3888 | `	zEnd = &zIn[nLen];` |
|      - | 3889 | `	/* Perform the requested operation */` |
|      4 | 3890 | `	for(;;){` |
|      9 | 3891 | `		zCur = zIn;` |
|      - | 3892 | `		/* Delimit the string */` |
|     21 | 3893 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3894 | `			zIn++;` |
|      1 | 3895 | `		}` |
|      9 | 3896 | `		if( zCur < zIn ){` |
|      - | 3897 | `			/* Output chunk verbatim */` |
|      9 | 3898 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3899 | `		}` |
|      9 | 3900 | `		if( zIn >= zEnd ){` |
|      - | 3901 | `			/* No more input to process */` |
|      5 | 3902 | `			break;` |
|      - | 3903 | `		}` |
|      - | 3904 | `		/* Output the HTML line break */` |
|      - | 3905 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br/>' (legacy without space), otherwise use '<br>' */` |
|      5 | 3906 | `		if( is_xhtml ){` |
|      3 | 3907 | `			ph7_result_string(pCtx,"<br/>",(int)sizeof("<br/>")-1);` |
|      2 | 3908 | `		}else{` |
|      3 | 3909 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3910 | `		}` |
|      5 | 3911 | `		zCur = zIn;` |
|      - | 3912 | `		/* Append trailing line */` |
|     11 | 3913 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3914 | `			zIn++;` |
|      1 | 3915 | `		}` |
|      5 | 3916 | `		if( zCur < zIn ){` |
|      - | 3917 | `			/* Output chunk verbatim */` |
|      5 | 3918 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 3919 | `		}` |
|      1 | 3920 | `	}` |
|      5 | 3921 | `	return PH7_OK;` |
|      4 | 3922 |  |
|      - | 3923 | `/*` |
|      - | 3924 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3925 | ` *  According to the PHP reference manual.` |
|      - | 3926 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3927 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3928 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 3929 | ` * This applies to both sprintf() and printf().` |
|      - | 3930 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 3931 | ` * or more of these elements, in order:` |
|      - | 3932 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 3933 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 3934 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 3935 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 3936 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 3937 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 3938 | ` *   it with a single quote ('). See the examples below.` |
|      - | 3939 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 3940 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 3941 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 3942 | ` *   should result in.` |
|      - | 3943 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 3944 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 3945 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 3946 | ` *   limit to the string.` |
|      - | 3947 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 3948 | ` *       % - a literal percent character. No argument is required.` |
|      - | 3949 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 3950 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 3951 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 3952 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 3953 | ` * 	     for the number of digits after the decimal point.` |
|      - | 3954 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 3955 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 3956 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 3957 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 3958 | ` *       g - shorter of %e and %f.` |
|      - | 3959 | ` *       G - shorter of %E and %f.` |
|      - | 3960 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 3961 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 3962 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 3963 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 3964 | ` */` |
|      - | 3965 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3966 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 3967 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 3968 | `/*` |
|      - | 3969 | `** Conversion types fall into various categories as defined by the` |
|      - | 3970 | `** following enumeration.` |
|      - | 3971 | `*/` |
|      - | 3972 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 3973 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 3974 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 3975 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 3976 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 3977 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 3978 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 3979 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 3980 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 3981 |  |
|      - | 3982 | `/*` |
|      - | 3983 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 3984 | `*/` |
|      - | 3985 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 3986 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 3987 | `/*` |
|      - | 3988 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 3989 | `** by an instance of the following structure` |
|      - | 3990 | `*/` |
|      - | 3991 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 3992 | `struct ph7_fmt_info` |
|      - | 3993 |  |
|      - | 3994 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 3995 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 3996 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 3997 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 3998 | `  char *charset; /* The character set for conversion */` |
|      - | 3999 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 4000 | `};` |
|      - | 4001 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 4002 | `/*` |
|      - | 4003 | `** "*val" is a double such that 0.1 <= *val < 10.0` |
|      - | 4004 | `** Return the ascii code for the leading digit of *val, then` |
|      - | 4005 | `** multiply "*val" by 10.0 to renormalize.` |
|      - | 4006 | `**` |
|      - | 4007 | `** Example:` |
|      - | 4008 | `**     input:     *val = 3.14159` |
|      - | 4009 | `**     output:    *val = 1.4159    function return = '3'` |
|      - | 4010 | `**` |
|      - | 4011 | `** The counter *cnt is incremented each time.  After counter exceeds` |
|      - | 4012 | `** 16 (the number of significant digits in a 64-bit float) '0' is` |
|      - | 4013 | `** always returned.` |
|      - | 4014 | `*/` |
|    404 | 4015 | `static int vxGetdigit(sxlongreal *val,int *cnt)` |
|      1 | 4016 |  |
|      - | 4017 | `  sxlongreal d;` |
|      - | 4018 | `  int digit;` |
|      - | 4019 |  |
|    405 | 4020 | `  if( (*cnt)++ >= 16 ){` |
|    ! 0 | 4021 | `	  return '0';` |
|      - | 4022 | `  }` |
|    405 | 4023 | `  digit = (int)*val;` |
|    405 | 4024 | `  d = digit;` |
|    405 | 4025 | `   *val = (*val - d)*10.0;` |
|    405 | 4026 | `  return digit + '0' ;` |
|    203 | 4027 |  |
|      - | 4028 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      - | 4029 | `/*` |
|      - | 4030 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 4031 | ` * used conversion types first.` |
|      - | 4032 | ` */` |
|      - | 4033 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 4034 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 4035 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 4036 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 4037 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 4038 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 4039 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 4040 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 4041 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 4042 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4043 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4044 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 4045 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 4046 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 4047 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 4048 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 4049 | `};` |
|      - | 4050 | `/*` |
|      - | 4051 | ` * Format a given string.` |
|      - | 4052 | ` * The root program.  All variations call this core.` |
|      - | 4053 | ` * INPUTS:` |
|      - | 4054 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 4055 | ` *            1. A pointer to the call context.` |
|      - | 4056 | ` *            2. A pointer to the list of characters to be output` |
|      - | 4057 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 4058 | ` *            3. An integer number of characters to be output.` |
|      - | 4059 | ` *               (Note: This number might be zero.)` |
|      - | 4060 | ` *            4. Upper layer private data.` |
|      - | 4061 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 4062 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 4063 | ` */` |
|    120 | 4064 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 4065 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 4066 | `	ph7_context *pCtx,  /* call context */` |
|      - | 4067 | `	const char *zIn,    /* Format string */` |
|      - | 4068 | `	int nByte,          /* Format string length */` |
|      - | 4069 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 4070 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 4071 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 4072 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 4073 | `	)` |
|      1 | 4074 |  |
|    121 | 4075 | `	char spaces[] = "                                                  ";` |
|      - | 4076 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    121 | 4077 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 4078 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 4079 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 4080 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 4081 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 4082 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 4083 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 4084 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 4085 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 4086 | `	ph7_int64 iVal;` |
|      - | 4087 | `	int precision;           /* Precision of the current field */` |
|      - | 4088 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 4089 | `	int c,rc,n;` |
|      - | 4090 | `	int length;              /* Length of the field */` |
|      - | 4091 | `	int prefix;` |
|      - | 4092 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 4093 | `	int width;               /* Width of the current field */` |
|      - | 4094 | `	int idx;` |
|    121 | 4095 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 4096 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 4097 | `	/* Start the format process */` |
|    123 | 4098 | `	for(;;){` |
|    247 | 4099 | `		zCur = zIn;` |
|    697 | 4100 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|    451 | 4101 | `			zIn++;` |
|      1 | 4102 | `		}` |
|    247 | 4103 | `		if( zCur < zIn ){` |
|      - | 4104 | `			/* Consume chunk verbatim */` |
|     95 | 4105 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|     95 | 4106 | `			if( rc == SXERR_ABORT ){` |
|      - | 4107 | `				/* Callback request an operation abort */` |
|    ! 0 | 4108 | `				break;` |
|      - | 4109 | `			}` |
|     47 | 4110 | `		}` |
|    247 | 4111 | `		if( zIn >= zEnd ){` |
|      - | 4112 | `			/* No more input to process,break immediately */` |
|    119 | 4113 | `			break;` |
|      - | 4114 | `		}` |
|      - | 4115 | `		/* Find out what flags are present */` |
|    129 | 4116 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    128 | 4117 | `			flag_alternateform = flag_zeropad = 0;` |
|    129 | 4118 | `		zIn++; /* Jump the precent sign */` |
|     64 | 4119 | `		do{` |
|    157 | 4120 | `			c = zIn[0];` |
|    157 | 4121 | `			switch( c ){` |
|      9 | 4122 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      3 | 4123 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 4124 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|      9 | 4125 | `			case '#':   flag_alternateform = 1;   c = 0;   break;` |
|      5 | 4126 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 4127 | `			case '\'':` |
|    ! 0 | 4128 | `				zIn++;` |
|    ! 0 | 4129 | `				if( zIn < zEnd ){` |
|      - | 4130 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 4131 | `					c = zIn[0];` |
|    ! 0 | 4132 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4133 | `						spaces[idx] = (char)c;` |
|    ! 0 | 4134 | `					}` |
|    ! 0 | 4135 | `					c = 0;` |
|    ! 0 | 4136 | `				}` |
|    ! 0 | 4137 | `				break;` |
|    128 | 4138 | `			default:                                       break;` |
|      - | 4139 | `			}` |
|    157 | 4140 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 4141 | `		/* Get the field width */` |
|    129 | 4142 | `		width = 0;` |
|    223 | 4143 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     31 | 4144 | `			width = width*10 + (zIn[0] - '0');` |
|     31 | 4145 | `			zIn++;` |
|      1 | 4146 | `		}` |
|    129 | 4147 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 4148 | `			/* Position specifer */` |
|    ! 0 | 4149 | `			if( width > 0 ){` |
|    ! 0 | 4150 | `				n = width;` |
|    ! 0 | 4151 | `				if( vf && n > 0 ){` |
|    ! 0 | 4152 | `					n--;` |
|    ! 0 | 4153 | `				}` |
|    ! 0 | 4154 | `			}` |
|    ! 0 | 4155 | `			zIn++;` |
|    ! 0 | 4156 | `			width = 0;` |
|    ! 0 | 4157 | `			if( zIn < zEnd && zIn[0] == '0' ){` |
|    ! 0 | 4158 | `				flag_zeropad = 1;` |
|    ! 0 | 4159 | `				zIn++;` |
|    ! 0 | 4160 | `			}` |
|    ! 0 | 4161 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 4162 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 4163 | `				zIn++;` |
|    ! 0 | 4164 | `			}` |
|    ! 0 | 4165 | `		}` |
|    129 | 4166 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 4167 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 4168 | `		}` |
|      - | 4169 | `		/* Get the precision */` |
|    129 | 4170 | `		precision = -1;` |
|    129 | 4171 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     57 | 4172 | `			precision = 0;` |
|     57 | 4173 | `			zIn++;` |
|    145 | 4174 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     61 | 4175 | `				precision = precision*10 + (zIn[0] - '0');` |
|     61 | 4176 | `				zIn++;` |
|      1 | 4177 | `			}` |
|     28 | 4178 | `		}` |
|    129 | 4179 | `		if( zIn >= zEnd ){` |
|      - | 4180 | `			/* No more input */` |
|      3 | 4181 | `			break;` |
|      - | 4182 | `		}` |
|      - | 4183 | `		/* Fetch the info entry for the field */` |
|    127 | 4184 | `		pInfo = 0;` |
|    127 | 4185 | `		xtype = PH7_FMT_ERROR;` |
|    127 | 4186 | `		c = zIn[0];` |
|    127 | 4187 | `		zIn++; /* Jump the format specifer */` |
|    699 | 4188 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|    697 | 4189 | `			if( c==aFmt[idx].fmttype ){` |
|    125 | 4190 | `				pInfo = &aFmt[idx];` |
|    125 | 4191 | `				xtype = pInfo->type;` |
|    125 | 4192 | `				break;` |
|      - | 4193 | `			}` |
|    287 | 4194 | `		}` |
|    127 | 4195 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    127 | 4196 | `		length = 0;` |
|      - | 4197 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 4198 | `		 /*` |
|      - | 4199 | `		  ** At this point, variables are initialized as follows:` |
|      - | 4200 | `		  **` |
|      - | 4201 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 4202 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 4203 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 4204 | `		  **                               field width was negative.` |
|      - | 4205 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 4206 | `		  **                               the conversion character.` |
|      - | 4207 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 4208 | `		  **   width                       The specified field width.  This is` |
|      - | 4209 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 4210 | `		  **   precision                   The specified precision.  The default` |
|      - | 4211 | `		  **                               is -1.` |
|      - | 4212 | `		  */` |
|    127 | 4213 | `		switch(xtype){` |
|    ! 0 | 4214 | `		case PH7_FMT_PERCENT:` |
|      - | 4215 | `			/* A literal percent character */` |
|    ! 0 | 4216 | `			zWorker[0] = '%';` |
|    ! 0 | 4217 | `			length = (int)sizeof(char);` |
|    ! 0 | 4218 | `			break;` |
|      3 | 4219 | `		case PH7_FMT_CHARX:` |
|      - | 4220 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 4221 | `			 * with that ASCII value` |
|      - | 4222 | `			 */` |
|      7 | 4223 | `			pArg = NEXT_ARG;` |
|      7 | 4224 | `			if( pArg == 0 ){` |
|      3 | 4225 | `				c = 0;` |
|      2 | 4226 | `			}else{` |
|      5 | 4227 | `				c = ph7_value_to_int(pArg);` |
|      - | 4228 | `			}` |
|      - | 4229 | `			/* NUL byte is an acceptable value */` |
|      7 | 4230 | `			zWorker[0] = (char)c;` |
|      7 | 4231 | `			length = (int)sizeof(char);` |
|      7 | 4232 | `			break;` |
|     12 | 4233 | `		case PH7_FMT_STRING:` |
|      - | 4234 | `			/* the argument is treated as and presented as a string */` |
|     25 | 4235 | `			pArg = NEXT_ARG;` |
|     25 | 4236 | `			if( pArg == 0 ){` |
|    ! 0 | 4237 | `				length = 0;` |
|    ! 0 | 4238 | `			}else{` |
|     25 | 4239 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 4240 | `			}` |
|     25 | 4241 | `			if( length < 1 ){` |
|    ! 0 | 4242 | `				zBuf = " ";` |
|    ! 0 | 4243 | `				length = (int)sizeof(char);` |
|    ! 0 | 4244 | `			}` |
|     25 | 4245 | `			if( precision>=0 && precision<length ){` |
|      3 | 4246 | `				length = precision;` |
|      1 | 4247 | `			}` |
|     25 | 4248 | `			if( flag_zeropad ){` |
|      - | 4249 | `				/* zero-padding works on strings too */` |
|    ! 0 | 4250 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4251 | `					spaces[idx] = '0';` |
|    ! 0 | 4252 | `				}` |
|    ! 0 | 4253 | `			}` |
|     25 | 4254 | `			break;` |
|     20 | 4255 | `		case PH7_FMT_RADIX:` |
|     41 | 4256 | `			pArg = NEXT_ARG;` |
|     41 | 4257 | `			if( pArg == 0 ){` |
|    ! 0 | 4258 | `				iVal = 0;` |
|    ! 0 | 4259 | `			}else{` |
|     41 | 4260 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 4261 | `			}` |
|      - | 4262 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|     41 | 4263 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 4264 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 4265 | `			}` |
|      - | 4266 | `#if 1` |
|      - | 4267 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 4268 | `        ** I think this is stupid.*/` |
|     41 | 4269 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 4270 | `#else` |
|      - | 4271 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 4272 | `        ** but leave the prefix for hex.*/` |
|      - | 4273 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 4274 | `#endif` |
|     41 | 4275 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|     23 | 4276 | `          if( iVal<0 ){` |
|      3 | 4277 | `            iVal = -iVal;` |
|      - | 4278 | `			/* Ticket 1433-003 */` |
|      3 | 4279 | `			if( iVal < 0 ){` |
|      - | 4280 | `				/* Overflow */` |
|    ! 0 | 4281 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4282 | `			}` |
|      3 | 4283 | `            prefix = '-';` |
|     22 | 4284 | `          }else if( flag_plussign )  prefix = '+';` |
|     19 | 4285 | `          else if( flag_blanksign )  prefix = ' ';` |
|     17 | 4286 | `          else                       prefix = 0;` |
|     12 | 4287 | `        }else{` |
|     19 | 4288 | `			if( iVal<0 ){` |
|    ! 0 | 4289 | `				iVal = -iVal;` |
|      - | 4290 | `				/* Ticket 1433-003 */` |
|    ! 0 | 4291 | `				if( iVal < 0 ){` |
|      - | 4292 | `					/* Overflow */` |
|    ! 0 | 4293 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4294 | `				}` |
|    ! 0 | 4295 | `			}` |
|     19 | 4296 | `			prefix = 0;` |
|      - | 4297 | `		}` |
|     41 | 4298 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      3 | 4299 | `          precision = width-(prefix!=0);` |
|      1 | 4300 | `        }` |
|     41 | 4301 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 4302 | `        {` |
|      - | 4303 | `          register char *cset;      /* Use registers for speed */` |
|      - | 4304 | `          register int base;` |
|     41 | 4305 | `          cset = pInfo->charset;` |
|     41 | 4306 | `          base = pInfo->base;` |
|     20 | 4307 | `          do{                                           /* Convert to ascii */` |
|     79 | 4308 | `            *(--zBuf) = cset[iVal%base];` |
|     79 | 4309 | `            iVal = iVal/base;` |
|     79 | 4310 | `          }while( iVal>0 );` |
|      - | 4311 | `        }` |
|     41 | 4312 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     55 | 4313 | `        for(idx=precision-length; idx>0; idx--){` |
|     15 | 4314 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|      8 | 4315 | `        }` |
|     41 | 4316 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|     41 | 4317 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 4318 | `          char *pre, x;` |
|      9 | 4319 | `          pre = pInfo->prefix;` |
|      9 | 4320 | `          if( *zBuf!=pre[0] ){` |
|     23 | 4321 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|      4 | 4322 | `          }` |
|      4 | 4323 | `        }` |
|     41 | 4324 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     41 | 4325 | `		break;` |
|     27 | 4326 | `		case PH7_FMT_FLOAT:` |
|      - | 4327 | `		case PH7_FMT_EXP:` |
|      - | 4328 | `		case PH7_FMT_GENERIC:{` |
|      - | 4329 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 4330 | `		long double realvalue;` |
|      - | 4331 | `		int  exp;                /* exponent of real numbers */` |
|      - | 4332 | `		double rounder;          /* Used for rounding floating point values */` |
|      - | 4333 | `		int flag_dp;            /* True if decimal point should be shown */` |
|      - | 4334 | `		int flag_rtz;           /* True if trailing zeros should be removed */` |
|      - | 4335 | `		int flag_exp;           /* True to force display of the exponent */` |
|      - | 4336 | `		int nsd;                 /* Number of significant digits returned */` |
|     55 | 4337 | `		pArg = NEXT_ARG;` |
|     55 | 4338 | `		if( pArg == 0 ){` |
|    ! 0 | 4339 | `			realvalue = 0;` |
|    ! 0 | 4340 | `		}else{` |
|     55 | 4341 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 4342 | `		}` |
|      - | 4343 | `		/* Special-case NaN and infinities since the normal formatting logic` |
|      - | 4344 | `		 * below assumes a finite positive realvalue. */` |
|     55 | 4345 | `		if( PH7_IS_NAN(realvalue) ){` |
|    ! 0 | 4346 | `			zBuf = "NAN";` |
|    ! 0 | 4347 | `			length = 3;` |
|    ! 0 | 4348 | `			break;` |
|      - | 4349 | `		}` |
|     55 | 4350 | `		if( PH7_IS_INF(realvalue) ){` |
|      - | 4351 | `			/* Infinity prints as INF or -INF depending on sign. */` |
|    ! 0 | 4352 | `			if( realvalue < 0.0 ){` |
|    ! 0 | 4353 | `				zBuf = "-INF";` |
|    ! 0 | 4354 | `				length = 4;` |
|    ! 0 | 4355 | `			}else{` |
|    ! 0 | 4356 | `				zBuf = "INF";` |
|    ! 0 | 4357 | `				length = 3;` |
|      - | 4358 | `			}` |
|    ! 0 | 4359 | `			break;` |
|      - | 4360 | `		}` |
|     55 | 4361 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|     55 | 4362 | `		if( precision>PH7_FMT_BUFSIZ-40) precision = PH7_FMT_BUFSIZ-40;` |
|     55 | 4363 | `        if( realvalue<0.0 ){` |
|    ! 0 | 4364 | `          realvalue = -realvalue;` |
|    ! 0 | 4365 | `          prefix = '-';` |
|    ! 0 | 4366 | `        }else{` |
|     55 | 4367 | `          if( flag_plussign )          prefix = '+';` |
|     55 | 4368 | `          else if( flag_blanksign )    prefix = ' ';` |
|     55 | 4369 | `          else                         prefix = 0;` |
|      - | 4370 | `        }` |
|     55 | 4371 | `        if( pInfo->type==PH7_FMT_GENERIC && precision>0 ) precision--;` |
|     55 | 4372 | `        rounder = 0.0;` |
|      - | 4373 | `#if 0` |
|      - | 4374 | `        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */` |
|      - | 4375 | `        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);` |
|      - | 4376 | `#else` |
|      - | 4377 | `        /* It makes more sense to use 0.5 */` |
|    387 | 4378 | `        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);` |
|      - | 4379 | `#endif` |
|     55 | 4380 | `        if( pInfo->type==PH7_FMT_FLOAT ) realvalue += rounder;` |
|      - | 4381 | `        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */` |
|     55 | 4382 | `        exp = 0;` |
|     55 | 4383 | `        if( realvalue>0.0 ){` |
|     59 | 4384 | `          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }` |
|     87 | 4385 | `          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }` |
|     59 | 4386 | `          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }` |
|     69 | 4387 | `          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }` |
|     55 | 4388 | `          if( exp>350 \|\| exp<-350 ){` |
|    ! 0 | 4389 | `            zBuf = "NaN";` |
|    ! 0 | 4390 | `            length = 3;` |
|    ! 0 | 4391 | `            break;` |
|      - | 4392 | `          }` |
|     27 | 4393 | `        }` |
|     55 | 4394 | `        zBuf = zWorker;` |
|      - | 4395 | `        /*` |
|      - | 4396 | `        ** If the field type is etGENERIC, then convert to either etEXP` |
|      - | 4397 | `        ** or etFLOAT, as appropriate.` |
|      - | 4398 | `        */` |
|     55 | 4399 | `        flag_exp = xtype==PH7_FMT_EXP;` |
|     55 | 4400 | `        if( xtype!=PH7_FMT_FLOAT ){` |
|    ! 0 | 4401 | `          realvalue += rounder;` |
|    ! 0 | 4402 | `          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }` |
|    ! 0 | 4403 | `        }` |
|     55 | 4404 | `        if( xtype==PH7_FMT_GENERIC ){` |
|    ! 0 | 4405 | `          flag_rtz = !flag_alternateform;` |
|    ! 0 | 4406 | `          if( exp<-4 \|\| exp>precision ){` |
|    ! 0 | 4407 | `            xtype = PH7_FMT_EXP;` |
|    ! 0 | 4408 | `          }else{` |
|    ! 0 | 4409 | `            precision = precision - exp;` |
|    ! 0 | 4410 | `            xtype = PH7_FMT_FLOAT;` |
|      - | 4411 | `          }` |
|    ! 0 | 4412 | `        }else{` |
|     55 | 4413 | `          flag_rtz = 0;` |
|      - | 4414 | `        }` |
|      - | 4415 | `        /*` |
|      - | 4416 | `        ** The "exp+precision" test causes output to be of type etEXP if` |
|      - | 4417 | `        ** the precision is too large to fit in buf[].` |
|      - | 4418 | `        */` |
|     55 | 4419 | `        nsd = 0;` |
|     55 | 4420 | `        if( xtype==PH7_FMT_FLOAT && exp+precision<PH7_FMT_BUFSIZ-30 ){` |
|     55 | 4421 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|     55 | 4422 | `          if( prefix ) *(zBuf++) = (char)prefix;         /* Sign */` |
|     55 | 4423 | `          if( exp<0 )  *(zBuf++) = '0';            /* Digits before "." */` |
|    145 | 4424 | `          else for(; exp>=0; exp--) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     55 | 4425 | `          if( flag_dp ) *(zBuf++) = '.';           /* The decimal point */` |
|     87 | 4426 | `          for(exp++; exp<0 && precision>0; precision--, exp++){` |
|     33 | 4427 | `            *(zBuf++) = '0';` |
|     17 | 4428 | `          }` |
|    355 | 4429 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     55 | 4430 | `          *(zBuf--) = 0;                           /* Null terminate */` |
|     55 | 4431 | `          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */` |
|    ! 0 | 4432 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 4433 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 4434 | `          }` |
|     55 | 4435 | `          zBuf++;                            /* point to next free slot */` |
|     28 | 4436 | `        }else{    /* etEXP or etGENERIC */` |
|    ! 0 | 4437 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|    ! 0 | 4438 | `          if( prefix ) *(zBuf++) = (char)prefix;   /* Sign */` |
|    ! 0 | 4439 | `          *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);  /* First digit */` |
|    ! 0 | 4440 | `          if( flag_dp ) *(zBuf++) = '.';     /* Decimal point */` |
|    ! 0 | 4441 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|    ! 0 | 4442 | `          zBuf--;                            /* point to last digit */` |
|    ! 0 | 4443 | `          if( flag_rtz && flag_dp ){          /* Remove tail zeros */` |
|    ! 0 | 4444 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 4445 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 4446 | `          }` |
|    ! 0 | 4447 | `          zBuf++;                            /* point to next free slot */` |
|    ! 0 | 4448 | `          if( exp \|\| flag_exp ){` |
|    ! 0 | 4449 | `            *(zBuf++) = pInfo->charset[0];` |
|    ! 0 | 4450 | `            if( exp<0 ){ *(zBuf++) = '-'; exp = -exp; } /* sign of exp */` |
|    ! 0 | 4451 | `            else       { *(zBuf++) = '+'; }` |
|    ! 0 | 4452 | `            if( exp>=100 ){` |
|    ! 0 | 4453 | `              *(zBuf++) = (char)((exp/100)+'0');                /* 100's digit */` |
|    ! 0 | 4454 | `              exp %= 100;` |
|    ! 0 | 4455 | `            }` |
|    ! 0 | 4456 | `            *(zBuf++) = (char)(exp/10+'0');                     /* 10's digit */` |
|    ! 0 | 4457 | `            *(zBuf++) = (char)(exp%10+'0');                     /* 1's digit */` |
|    ! 0 | 4458 | `          }` |
|      - | 4459 | `        }` |
|      - | 4460 | `        /* The converted number is in buf[] and zero terminated.Output it.` |
|      - | 4461 | `        ** Note that the number is in the usual order, not reversed as with` |
|      - | 4462 | `        ** integer conversions.*/` |
|     55 | 4463 | `        length = (int)(zBuf-zWorker);` |
|     55 | 4464 | `        zBuf = zWorker;` |
|      - | 4465 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 4466 | `        ** set and we are not left justified */` |
|     55 | 4467 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 4468 | `          int i;` |
|      3 | 4469 | `          int nPad = width - length;` |
|     13 | 4470 | `          for(i=width; i>=nPad; i--){` |
|     11 | 4471 | `            zBuf[i] = zBuf[i-nPad];` |
|      6 | 4472 | `          }` |
|      3 | 4473 | `          i = prefix!=0;` |
|      5 | 4474 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      3 | 4475 | `          length = width;` |
|      1 | 4476 | `        }` |
|      - | 4477 | `#else` |
|      - | 4478 | `         zBuf = " ";` |
|      - | 4479 | `		 length = (int)sizeof(char);` |
|      - | 4480 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|     55 | 4481 | `		 break;` |
|      - | 4482 | `							 }` |
|      1 | 4483 | `		default:` |
|      - | 4484 | `			/* Invalid format specifer */` |
|      3 | 4485 | `			zWorker[0] = '?';` |
|      3 | 4486 | `			length = (int)sizeof(char);` |
|      2 | 4487 | `			break;` |
|      - | 4488 | `		}` |
|      - | 4489 | `		 /*` |
|      - | 4490 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 4491 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 4492 | `		 ** the output.` |
|      - | 4493 | `		 */` |
|    127 | 4494 | `    if( !flag_leftjustify ){` |
|      - | 4495 | `      register int nspace;` |
|    119 | 4496 | `      nspace = width-length;` |
|    119 | 4497 | `      if( nspace>0 ){` |
|      5 | 4498 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4499 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4500 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4501 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4502 | `			}` |
|    ! 0 | 4503 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4504 | `        }` |
|      5 | 4505 | `        if( nspace>0 ){` |
|      5 | 4506 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      5 | 4507 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4508 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4509 | `			}` |
|      2 | 4510 | `		}` |
|      2 | 4511 | `      }` |
|     59 | 4512 | `    }` |
|    127 | 4513 | `    if( length>0 ){` |
|    127 | 4514 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    127 | 4515 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4516 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4517 | `		}` |
|     63 | 4518 | `    }` |
|    127 | 4519 | `    if( flag_leftjustify ){` |
|      - | 4520 | `      register int nspace;` |
|      9 | 4521 | `      nspace = width-length;` |
|      9 | 4522 | `      if( nspace>0 ){` |
|      9 | 4523 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4524 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4525 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4526 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4527 | `			}` |
|    ! 0 | 4528 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4529 | `        }` |
|      9 | 4530 | `        if( nspace>0 ){` |
|      9 | 4531 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      9 | 4532 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4533 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4534 | `			}` |
|      4 | 4535 | `		}` |
|      4 | 4536 | `      }` |
|      4 | 4537 | `    }` |
|      1 | 4538 | ` }/* for(;;) */` |
|    121 | 4539 | `	return SXRET_OK;` |
|     61 | 4540 |  |
|      - | 4541 | `/*` |
|      - | 4542 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 4543 | ` */` |
|     84 | 4544 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4545 |  |
|      - | 4546 | `	/* Consume directly */` |
|     85 | 4547 | `	ph7_result_string(pCtx,zInput,nLen);` |
|     42 | 4548 | `	SXUNUSED(pUserData); /* cc warning */` |
|     85 | 4549 | `	return PH7_OK;` |
|      1 | 4550 |  |
|      - | 4551 | `/*` |
|      - | 4552 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 4553 | ` *  Return a formatted string.` |
|      - | 4554 | ` * Parameters` |
|      - | 4555 | ` *  $format` |
|      - | 4556 | ` *    The format string (see block comment above)` |
|      - | 4557 | ` * Return` |
|      - | 4558 | ` *  A string produced according to the formatting string format.` |
|      - | 4559 | ` */` |
|     56 | 4560 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4561 |  |
|      - | 4562 | `	const char *zFormat;` |
|      - | 4563 | `	int nLen;` |
|     57 | 4564 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4565 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 4566 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4567 | `		return PH7_OK;` |
|      - | 4568 | `	}` |
|      - | 4569 | `	/* Extract the string format */` |
|     55 | 4570 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     55 | 4571 | `	if( nLen < 1 ){` |
|      - | 4572 | `		/* Empty string */` |
|    ! 0 | 4573 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4574 | `		return PH7_OK;` |
|      - | 4575 | `	}` |
|      - | 4576 | `	/* Format the string */` |
|     55 | 4577 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,0,FALSE);` |
|     55 | 4578 | `	return PH7_OK;` |
|     29 | 4579 |  |
|      - | 4580 | `/*` |
|      - | 4581 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 4582 | ` */` |
|    110 | 4583 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4584 |  |
|    111 | 4585 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 4586 | `	/* Call the VM output consumer directly */` |
|    111 | 4587 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 4588 | `	/* Increment counter */` |
|    111 | 4589 | `	*pCounter += nLen;` |
|    111 | 4590 | `	return PH7_OK;` |
|      1 | 4591 |  |
|      - | 4592 | `/*` |
|      - | 4593 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 4594 | ` *  Output a formatted string.` |
|      - | 4595 | ` * Parameters` |
|      - | 4596 | ` *  $format` |
|      - | 4597 | ` *   See sprintf() for a description of format.` |
|      - | 4598 | ` * Return` |
|      - | 4599 | ` *  The length of the outputted string.` |
|      - | 4600 | ` */` |
|     42 | 4601 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4602 |  |
|     43 | 4603 | `	ph7_int64 nCounter = 0;` |
|      - | 4604 | `	const char *zFormat;` |
|      - | 4605 | `	int nLen;` |
|     43 | 4606 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4607 | `		/* Missing/Invalid arguments,return 0 */` |
|      3 | 4608 | `		ph7_result_int(pCtx,0);` |
|      3 | 4609 | `		return PH7_OK;` |
|      - | 4610 | `	}` |
|      - | 4611 | `	/* Extract the string format */` |
|     41 | 4612 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 4613 | `	if( nLen < 1 ){` |
|      - | 4614 | `		/* Empty string */` |
|    ! 0 | 4615 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4616 | `		return PH7_OK;` |
|      - | 4617 | `	}` |
|      - | 4618 | `	/* Format the string */` |
|     41 | 4619 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 4620 | `	/* Return the length of the outputted string */` |
|     41 | 4621 | `	ph7_result_int64(pCtx,nCounter);` |
|     41 | 4622 | `	return PH7_OK;` |
|     22 | 4623 |  |
|      - | 4624 | `/*` |
|      - | 4625 | ` * int vprintf(string $format,array $args)` |
|      - | 4626 | ` *  Output a formatted string.` |
|      - | 4627 | ` * Parameters` |
|      - | 4628 | ` *  $format` |
|      - | 4629 | ` *   See sprintf() for a description of format.` |
|      - | 4630 | ` * Return` |
|      - | 4631 | ` *  The length of the outputted string.` |
|      - | 4632 | ` */` |
|      2 | 4633 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4634 |  |
|      3 | 4635 | `	ph7_int64 nCounter = 0;` |
|      - | 4636 | `	const char *zFormat;` |
|      - | 4637 | `	ph7_hashmap *pMap;` |
|      - | 4638 | `	SySet sArg;` |
|      - | 4639 | `	int nLen,n;` |
|      3 | 4640 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4641 | `		/* Missing/Invalid arguments,return 0 */` |
|    ! 0 | 4642 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4643 | `		return PH7_OK;` |
|      - | 4644 | `	}` |
|      - | 4645 | `	/* Extract the string format */` |
|      3 | 4646 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4647 | `	if( nLen < 1 ){` |
|      - | 4648 | `		/* Empty string */` |
|    ! 0 | 4649 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4650 | `		return PH7_OK;` |
|      - | 4651 | `	}` |
|      - | 4652 | `	/* Point to the hashmap */` |
|      3 | 4653 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4654 | `	/* Extract arguments from the hashmap */` |
|      3 | 4655 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4656 | `	/* Format the string */` |
|      3 | 4657 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 4658 | `	/* Return the length of the outputted string */` |
|      3 | 4659 | `	ph7_result_int64(pCtx,nCounter);` |
|      - | 4660 | `	/* Release the container */` |
|      3 | 4661 | `	SySetRelease(&sArg);` |
|      3 | 4662 | `	return PH7_OK;` |
|      2 | 4663 |  |
|      - | 4664 | `/*` |
|      - | 4665 | ` * int vsprintf(string $format,array $args)` |
|      - | 4666 | ` *  Output a formatted string.` |
|      - | 4667 | ` * Parameters` |
|      - | 4668 | ` *  $format` |
|      - | 4669 | ` *   See sprintf() for a description of format.` |
|      - | 4670 | ` * Return` |
|      - | 4671 | ` *  A string produced according to the formatting string format.` |
|      - | 4672 | ` */` |
|     10 | 4673 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4674 |  |
|      - | 4675 | `	const char *zFormat;` |
|      - | 4676 | `	ph7_hashmap *pMap;` |
|      - | 4677 | `	SySet sArg;` |
|      - | 4678 | `	int nLen,n;` |
|     11 | 4679 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4680 | `		/* Missing/Invalid arguments,return the empty string */` |
|      5 | 4681 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 4682 | `		return PH7_OK;` |
|      - | 4683 | `	}` |
|      - | 4684 | `	/* Extract the string format */` |
|      7 | 4685 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4686 | `	if( nLen < 1 ){` |
|      - | 4687 | `		/* Empty string */` |
|    ! 0 | 4688 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4689 | `		return PH7_OK;` |
|      - | 4690 | `	}` |
|      - | 4691 | `	/* Point to hashmap */` |
|      7 | 4692 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4693 | `	/* Extract arguments from the hashmap */` |
|      7 | 4694 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4695 | `	/* Format the string */` |
|      7 | 4696 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),0,TRUE);` |
|      - | 4697 | `	/* Release the container */` |
|      7 | 4698 | `	SySetRelease(&sArg);` |
|      7 | 4699 | `	return PH7_OK;` |
|      6 | 4700 |  |
|      - | 4701 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 4702 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4703 | `/*` |
|      - | 4704 | ` * Symisc eXtension.` |
|      - | 4705 | ` * string size_format(int64 $size)` |
|      - | 4706 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 4707 | ` *  Example:` |
|      - | 4708 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 4709 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 4710 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 4711 | ` * Parameter` |
|      - | 4712 | ` *  $size` |
|      - | 4713 | ` *    Entity size in bytes.` |
|      - | 4714 | ` * Return` |
|      - | 4715 | ` *   Formatted string representation of the given size.` |
|      - | 4716 | ` */` |
|     24 | 4717 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4718 |  |
|      - | 4719 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 4720 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 4721 | `	sxi32 nRest,i_32;` |
|      - | 4722 | `	ph7_int64 iSize;` |
|     25 | 4723 | `	int c = -1; /* index in zUnit[] */` |
|      - | 4724 |  |
|     25 | 4725 | `	if( nArg < 1 ){` |
|      - | 4726 | `		/* Missing argument,return the empty string */` |
|      3 | 4727 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4728 | `		return PH7_OK;` |
|      - | 4729 | `	}` |
|      - | 4730 | `	/* Extract the given size */` |
|     23 | 4731 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 4732 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 4733 | `		/* Don't bother formatting,return immediately */` |
|      5 | 4734 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 4735 | `		return PH7_OK;` |
|      - | 4736 | `	}` |
|     19 | 4737 | `	for(;;){` |
|     39 | 4738 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 4739 | `		iSize >>= 10;` |
|     39 | 4740 | `		c++;` |
|     39 | 4741 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 4742 | `			break;` |
|      - | 4743 | `		}` |
|      1 | 4744 | `	}` |
|     19 | 4745 | `	nRest /= 100;` |
|     19 | 4746 | `	if( nRest > 9 ){` |
|    ! 0 | 4747 | `		nRest = 9;` |
|    ! 0 | 4748 | `	}` |
|     19 | 4749 | `	if( iSize > 999 ){` |
|    ! 0 | 4750 | `		c++;` |
|    ! 0 | 4751 | `		nRest = 9;` |
|    ! 0 | 4752 | `		iSize = 0;` |
|    ! 0 | 4753 | `	}` |
|     19 | 4754 | `	i_32 = (sxi32)iSize;` |
|      - | 4755 | `	/* Format */` |
|     19 | 4756 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 4757 | `	return PH7_OK;` |
|     13 | 4758 |  |
|      - | 4759 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4760 | `/*` |
|      - | 4761 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 4762 | ` *   Calculate the md5 hash of a string.` |
|      - | 4763 | ` * Parameter` |
|      - | 4764 | ` *  $str` |
|      - | 4765 | ` *   Input string` |
|      - | 4766 | ` * $raw_output` |
|      - | 4767 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4768 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4769 | ` * Return` |
|      - | 4770 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 4771 | ` */` |
|     10 | 4772 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4773 |  |
|      - | 4774 | `	unsigned char zDigest[16];` |
|     11 | 4775 | `	int raw_output = FALSE;` |
|      - | 4776 | `	const void *pIn;` |
|      - | 4777 | `	int nLen;` |
|     11 | 4778 | `	if( nArg < 1 ){` |
|      - | 4779 | `		/* Missing arguments,return the empty string */` |
|      3 | 4780 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4781 | `		return PH7_OK;` |
|      - | 4782 | `	}` |
|      - | 4783 | `	/* Extract the input string */` |
|      9 | 4784 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 4785 | `	if( nLen < 1 ){` |
|      - | 4786 | `		/* Empty string */` |
|    ! 0 | 4787 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4788 | `		return PH7_OK;` |
|      - | 4789 | `	}` |
|      9 | 4790 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      3 | 4791 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 | 4792 | `	}` |
|      - | 4793 | `	/* Compute the MD5 digest */` |
|      9 | 4794 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|      9 | 4795 | `	if( raw_output ){` |
|      - | 4796 | `		/* Output raw digest */` |
|      3 | 4797 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      2 | 4798 | `	}else{` |
|      - | 4799 | `		/* Perform a binary to hex conversion */` |
|      7 | 4800 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4801 | `	}` |
|      9 | 4802 | `	return PH7_OK;` |
|      6 | 4803 |  |
|      - | 4804 | `/*` |
|      - | 4805 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 4806 | ` *   Calculate the sha1 hash of a string.` |
|      - | 4807 | ` * Parameter` |
|      - | 4808 | ` *  $str` |
|      - | 4809 | ` *   Input string` |
|      - | 4810 | ` * $raw_output` |
|      - | 4811 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4812 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4813 | ` * Return` |
|      - | 4814 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 4815 | ` */` |
|      8 | 4816 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4817 |  |
|      - | 4818 | `	unsigned char zDigest[20];` |
|      9 | 4819 | `	int raw_output = FALSE;` |
|      - | 4820 | `	const void *pIn;` |
|      - | 4821 | `	int nLen;` |
|      9 | 4822 | `	if( nArg < 1 ){` |
|      - | 4823 | `		/* Missing arguments,return the empty string */` |
|      3 | 4824 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4825 | `		return PH7_OK;` |
|      - | 4826 | `	}` |
|      - | 4827 | `	/* Extract the input string */` |
|      7 | 4828 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4829 | `	if( nLen < 1 ){` |
|      - | 4830 | `		/* Empty string */` |
|    ! 0 | 4831 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4832 | `		return PH7_OK;` |
|      - | 4833 | `	}` |
|      7 | 4834 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      3 | 4835 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 | 4836 | `	}` |
|      - | 4837 | `	/* Compute the SHA1 digest */` |
|      7 | 4838 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|      7 | 4839 | `	if( raw_output ){` |
|      - | 4840 | `		/* Output raw digest */` |
|      3 | 4841 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      2 | 4842 | `	}else{` |
|      - | 4843 | `		/* Perform a binary to hex conversion */` |
|      5 | 4844 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4845 | `	}` |
|      7 | 4846 | `	return PH7_OK;` |
|      5 | 4847 |  |
|      - | 4848 | `/*` |
|      - | 4849 | ` * int64 crc32(string $str)` |
|      - | 4850 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 4851 | ` * Parameter` |
|      - | 4852 | ` *  $str` |
|      - | 4853 | ` *   Input string` |
|      - | 4854 | ` * Return` |
|      - | 4855 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 4856 | ` */` |
|      4 | 4857 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4858 |  |
|      - | 4859 | `	const void *pIn;` |
|      - | 4860 | `	sxu32 nCRC;` |
|      - | 4861 | `	int nLen;` |
|      5 | 4862 | `	if( nArg < 1 ){` |
|      - | 4863 | `		/* Missing arguments,return 0 */` |
|      3 | 4864 | `		ph7_result_int(pCtx,0);` |
|      3 | 4865 | `		return PH7_OK;` |
|      - | 4866 | `	}` |
|      - | 4867 | `	/* Extract the input string */` |
|      3 | 4868 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4869 | `	if( nLen < 1 ){` |
|      - | 4870 | `		/* Empty string */` |
|    ! 0 | 4871 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4872 | `		return PH7_OK;` |
|      - | 4873 | `	}` |
|      - | 4874 | `	/* Calculate the sum */` |
|      3 | 4875 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 4876 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 4877 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 4878 | `	return PH7_OK;` |
|      3 | 4879 |  |
|      - | 4880 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 4881 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4882 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 4883 | `/*` |
|      - | 4884 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 4885 |  |
|      - | 4886 | ` */` |
|      4 | 4887 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 4888 | `	const char *zInput, /* Raw input */` |
|      - | 4889 | `	int nByte,  /* Input length */` |
|      - | 4890 | `	int delim,  /* Delimiter */` |
|      - | 4891 | `	int encl,   /* Enclosure */` |
|      - | 4892 | `	int escape,  /* Escape character */` |
|      - | 4893 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 4894 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 4895 | `	)` |
|      1 | 4896 |  |
|      5 | 4897 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 4898 | `	const char *zIn = zInput;` |
|      - | 4899 | `	const char *zPtr;` |
|      - | 4900 | `	int isEnc;` |
|      - | 4901 | `	/* Start processing */` |
|      8 | 4902 | `	for(;;){` |
|     17 | 4903 | `		if( zIn >= zEnd ){` |
|      - | 4904 | `			/* No more input to process */` |
|      5 | 4905 | `			break;` |
|      - | 4906 | `		}` |
|     13 | 4907 | `		isEnc = 0;` |
|     13 | 4908 | `		zPtr = zIn;` |
|      - | 4909 | `		/* Find the first delimiter */` |
|     27 | 4910 | `		while( zIn < zEnd ){` |
|     23 | 4911 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 4912 | `				/* Delimiter found,break imediately */` |
|      5 | 4913 | `				break;` |
|     15 | 4914 | `			}else if( zIn[0] == encl ){` |
|      - | 4915 | `				/* Inside enclosure? */` |
|    ! 0 | 4916 | `				isEnc = !isEnc;` |
|     15 | 4917 | `			}else if( zIn[0] == escape ){` |
|      - | 4918 | `				/* Escape sequence */` |
|    ! 0 | 4919 | `				zIn++;` |
|    ! 0 | 4920 | `			}` |
|      - | 4921 | `			/* Advance the cursor */` |
|     15 | 4922 | `			zIn++;` |
|      1 | 4923 | `		}` |
|     13 | 4924 | `		if( zIn > zPtr ){` |
|     13 | 4925 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 4926 | `			sxi32 rc;` |
|      - | 4927 | `			/* Invoke the supllied callback */` |
|     13 | 4928 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 4929 | `				zPtr++;` |
|    ! 0 | 4930 | `				nByteChunk-=2;` |
|    ! 0 | 4931 | `			}` |
|     13 | 4932 | `			if( nByteChunk > 0 ){` |
|     13 | 4933 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 4934 | `				if( rc == SXERR_ABORT ){` |
|      - | 4935 | `					/* User callback request an operation abort */` |
|    ! 0 | 4936 | `					break;` |
|      - | 4937 | `				}` |
|      6 | 4938 | `			}` |
|      6 | 4939 | `		}` |
|      - | 4940 | `		/* Ignore trailing delimiter */` |
|     21 | 4941 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 4942 | `			zIn++;` |
|      1 | 4943 | `		}` |
|      1 | 4944 | `	}` |
|      5 | 4945 | `	return SXRET_OK;` |
|      1 | 4946 |  |
|      - | 4947 | `/*` |
|      - | 4948 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 4949 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 4950 | ` * argument to this callback.` |
|      - | 4951 | ` */` |
|     12 | 4952 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 4953 |  |
|     13 | 4954 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 4955 | `	ph7_value sEntry;` |
|      - | 4956 | `	SyString sToken;` |
|      - | 4957 | `	/* Insert the token in the given array */` |
|     13 | 4958 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 4959 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 4960 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 4961 | `	if( sToken.nByte < 1){` |
|    ! 0 | 4962 | `		return SXRET_OK;` |
|      - | 4963 | `	}` |
|     13 | 4964 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 4965 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 4966 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 4967 | `	return SXRET_OK;` |
|      7 | 4968 |  |
|      - | 4969 | `/*` |
|      - | 4970 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 4971 | ` *  Parse a CSV string into an array.` |
|      - | 4972 | ` * Parameters` |
|      - | 4973 | ` *  $input` |
|      - | 4974 | ` *   The string to parse.` |
|      - | 4975 | ` *  $delimiter` |
|      - | 4976 | ` *   Set the field delimiter (one character only).` |
|      - | 4977 | ` *  $enclosure` |
|      - | 4978 | ` *   Set the field enclosure character (one character only).` |
|      - | 4979 | ` *  $escape` |
|      - | 4980 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 4981 | ` * Return` |
|      - | 4982 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 4983 | ` */` |
|      4 | 4984 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4985 |  |
|      - | 4986 | `	const char *zInput,*zPtr;` |
|      - | 4987 | `	ph7_value *pArray;` |
|      5 | 4988 | `	int delim  = ',';   /* Delimiter */` |
|      5 | 4989 | `	int encl   = '"' ;  /* Enclosure */` |
|      5 | 4990 | `	int escape = '\\';  /* Escape character */` |
|      - | 4991 | `	int nLen;` |
|      5 | 4992 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4993 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 4994 | `		ph7_result_null(pCtx);` |
|      3 | 4995 | `		return PH7_OK;` |
|      - | 4996 | `	}` |
|      - | 4997 | `	/* Extract the raw input */` |
|      3 | 4998 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4999 | `	if( nArg > 1 ){` |
|      - | 5000 | `		int i;` |
|      3 | 5001 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 5002 | `			/* Extract the delimiter */` |
|      3 | 5003 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 5004 | `			if( i > 0 ){` |
|      3 | 5005 | `				delim = zPtr[0];` |
|      1 | 5006 | `			}` |
|      1 | 5007 | `		}` |
|      3 | 5008 | `		if( nArg > 2 ){` |
|      3 | 5009 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 5010 | `				/* Extract the enclosure */` |
|      3 | 5011 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 5012 | `				if( i > 0 ){` |
|      3 | 5013 | `					encl = zPtr[0];` |
|      1 | 5014 | `				}` |
|      1 | 5015 | `			}` |
|      3 | 5016 | `			if( nArg > 3 ){` |
|      3 | 5017 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 5018 | `					/* Extract the escape character */` |
|      3 | 5019 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 5020 | `					if( i > 0 ){` |
|      3 | 5021 | `						escape = zPtr[0];` |
|      1 | 5022 | `					}` |
|      1 | 5023 | `				}` |
|      1 | 5024 | `			}` |
|      1 | 5025 | `		}` |
|      1 | 5026 | `	}` |
|      - | 5027 | `	/* Create our array */` |
|      3 | 5028 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5029 | `	if( pArray == 0 ){` |
|    ! 0 | 5030 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 5031 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5032 | `		return PH7_OK;` |
|      - | 5033 | `	}` |
|      - | 5034 | `	/* Parse the raw input */` |
|      3 | 5035 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 5036 | `	/* Return the freshly created array */` |
|      3 | 5037 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5038 | `	return PH7_OK;` |
|      3 | 5039 |  |
|      - | 5040 | `/*` |
|      - | 5041 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 5042 | ` * container.` |
|      - | 5043 | ` * Refer to [strip_tags()].` |
|      - | 5044 | ` */` |
|     10 | 5045 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5046 |  |
|     11 | 5047 | `	const char *zEnd = &zTag[nByte];` |
|      - | 5048 | `	const char *zPtr;` |
|      - | 5049 | `	SyString sEntry;` |
|      - | 5050 | `	/* Strip tags */` |
|     10 | 5051 | `	for(;;){` |
|     45 | 5052 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 5053 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 5054 | `				zTag++;` |
|      1 | 5055 | `		}` |
|     21 | 5056 | `		if( zTag >= zEnd ){` |
|     11 | 5057 | `			break;` |
|      - | 5058 | `		}` |
|     11 | 5059 | `		zPtr = zTag;` |
|      - | 5060 | `		/* Delimit the tag */` |
|     25 | 5061 | `		while(zTag < zEnd ){` |
|     25 | 5062 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5063 | `				/* UTF-8 stream */` |
|      3 | 5064 | `				zTag++;` |
|      5 | 5065 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 5066 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 5067 | `				break;` |
|    ! 0 | 5068 | `			}else{` |
|     13 | 5069 | `				zTag++;` |
|      - | 5070 | `			}` |
|      1 | 5071 | `		}` |
|     11 | 5072 | `		if( zTag > zPtr ){` |
|      - | 5073 | `			/* Perform the insertion */` |
|     11 | 5074 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 5075 | `			SyStringFullTrim(&sEntry);` |
|     11 | 5076 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 5077 | `		}` |
|      - | 5078 | `		/* Jump the trailing '>' */` |
|     11 | 5079 | `		zTag++;` |
|      1 | 5080 | `	}` |
|     11 | 5081 | `	return SXRET_OK;` |
|      1 | 5082 |  |
|      - | 5083 | `/*` |
|      - | 5084 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 5085 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 5086 | ` * Refer to [strip_tags()].` |
|      - | 5087 | ` */` |
|     36 | 5088 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5089 |  |
|     37 | 5090 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 5091 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 5092 | `		SyString sTag;` |
|     85 | 5093 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 5094 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 5095 | `			zTag++;` |
|      1 | 5096 | `		}` |
|      - | 5097 | `		/* Delimit the tag */` |
|     25 | 5098 | `		zCur = zTag;` |
|     77 | 5099 | `		while(zTag < zEnd ){` |
|     77 | 5100 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5101 | `				/* UTF-8 stream */` |
|      5 | 5102 | `				zTag++;` |
|      9 | 5103 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 5104 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 5105 | `				break;` |
|    ! 0 | 5106 | `			}else{` |
|     49 | 5107 | `				zTag++;` |
|      - | 5108 | `			}` |
|      1 | 5109 | `		}` |
|     25 | 5110 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 5111 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 5112 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 5113 | `		if( sTag.nByte > 0 ){` |
|      - | 5114 | `			SyString *aEntry,*pEntry;` |
|      - | 5115 | `			sxi32 rc;` |
|      - | 5116 | `			sxu32 n;` |
|      - | 5117 | `			/* Perform the lookup */` |
|     25 | 5118 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 5119 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 5120 | `				pEntry = &aEntry[n];` |
|      - | 5121 | `				/* Do the comparison */` |
|     25 | 5122 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 5123 | `				if( !rc ){` |
|     21 | 5124 | `					return SXRET_OK;` |
|      - | 5125 | `				}` |
|      3 | 5126 | `			}` |
|      2 | 5127 | `		}` |
|      2 | 5128 | `	}` |
|      - | 5129 | `	/* No such tag */` |
|     17 | 5130 | `	return SXERR_NOTFOUND;` |
|     19 | 5131 |  |
|      - | 5132 | `/*` |
|      - | 5133 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 5134 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 5135 | ` * Refer to [strip_tags()].` |
|      - | 5136 | ` */` |
|     16 | 5137 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 5138 |  |
|     17 | 5139 | `	const char *zEnd = &zIn[nByte];` |
|      - | 5140 | `	const char *zPtr,*zTag;` |
|      - | 5141 | `	SySet sSet;` |
|      - | 5142 | `	/* initialize the set of allowed tags */` |
|     17 | 5143 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 5144 | `	if( nTaglen > 0 ){` |
|      - | 5145 | `		/* Set of allowed tags */` |
|     11 | 5146 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 5147 | `	}` |
|      - | 5148 | `	/* Set the empty string */` |
|     17 | 5149 | `	ph7_result_string(pCtx,"",0);` |
|      - | 5150 | `	/* Start processing */` |
|     26 | 5151 | `	for(;;){` |
|     53 | 5152 | `		if(zIn >= zEnd){` |
|      - | 5153 | `			/* No more input to process */` |
|     15 | 5154 | `			break;` |
|      - | 5155 | `		}` |
|     39 | 5156 | `		zPtr = zIn;` |
|      - | 5157 | `		/* Find a tag */` |
|    133 | 5158 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 5159 | `			zIn++;` |
|      1 | 5160 | `		}` |
|     39 | 5161 | `		if( zIn > zPtr ){` |
|      - | 5162 | `			/* Consume raw input */` |
|     21 | 5163 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 5164 | `		}` |
|      - | 5165 | `		/* Ignore trailing null bytes */` |
|     39 | 5166 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 5167 | `			zIn++;` |
|    ! 0 | 5168 | `		}` |
|     39 | 5169 | `		if(zIn >= zEnd){` |
|      - | 5170 | `			/* No more input to process */` |
|      3 | 5171 | `			break;` |
|      - | 5172 | `		}` |
|     37 | 5173 | `		if( zIn[0] == '<' ){` |
|      - | 5174 | `			sxi32 rc;` |
|     37 | 5175 | `			zTag = zIn++;` |
|      - | 5176 | `			/* Delimit the tag */` |
|    127 | 5177 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 5178 | `				zIn++;` |
|      1 | 5179 | `			}` |
|     37 | 5180 | `			if( zIn < zEnd ){` |
|     37 | 5181 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 5182 | `			}` |
|      - | 5183 | `			/* Query the set */` |
|     37 | 5184 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 5185 | `			if( rc == SXRET_OK ){` |
|      - | 5186 | `				/* Keep the tag */` |
|     21 | 5187 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 5188 | `			}` |
|     18 | 5189 | `		}` |
|      1 | 5190 | `	}` |
|      - | 5191 | `	/* Cleanup */` |
|     17 | 5192 | `	SySetRelease(&sSet);` |
|     17 | 5193 | `	return SXRET_OK;` |
|      1 | 5194 |  |
|      - | 5195 | `/*` |
|      - | 5196 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 5197 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 5198 | ` * Parameters` |
|      - | 5199 | ` *  $str` |
|      - | 5200 | ` *  The input string.` |
|      - | 5201 | ` * $allowable_tags` |
|      - | 5202 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 5203 | ` * Return` |
|      - | 5204 | ` *  Returns the stripped string.` |
|      - | 5205 | ` */` |
|     16 | 5206 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5207 |  |
|     17 | 5208 | `	const char *zTaglist = 0;` |
|      - | 5209 | `	const char *zString;` |
|     17 | 5210 | `	int nTaglen = 0;` |
|      - | 5211 | `	int nLen;` |
|     17 | 5212 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5213 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 5214 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5215 | `		return PH7_OK;` |
|      - | 5216 | `	}` |
|      - | 5217 | `	/* Point to the raw string */` |
|     15 | 5218 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 5219 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 5220 | `		/* Allowed tag */` |
|     11 | 5221 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 5222 | `	}` |
|      - | 5223 | `	/* Process input */` |
|     15 | 5224 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 5225 | `	return PH7_OK;` |
|      9 | 5226 |  |
|      - | 5227 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5228 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5229 | `/*` |
|      - | 5230 | ` * string str_shuffle(string $str)` |
|      - | 5231 |  |
|      - | 5232 | ` *  Randomly shuffles a string.` |
|      - | 5233 | ` * Parameters` |
|      - | 5234 | ` *  $str` |
|      - | 5235 | ` *   The input string.` |
|      - | 5236 | ` * Return` |
|      - | 5237 | ` *  Returns the shuffled string.` |
|      - | 5238 | ` */` |
|     12 | 5239 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5240 |  |
|      - | 5241 | `	const char *zString;` |
|      - | 5242 | `	int nLen,i,c;` |
|      - | 5243 | `	sxu32 iR;` |
|     13 | 5244 | `	if( nArg < 1 ){` |
|      - | 5245 | `		/* Missing arguments,return the empty string */` |
|      3 | 5246 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5247 | `		return PH7_OK;` |
|      - | 5248 | `	}` |
|      - | 5249 | `	/* Extract the target string */` |
|     11 | 5250 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5251 | `	if( nLen < 1 ){` |
|      - | 5252 | `		/* Nothing to shuffle */` |
|      3 | 5253 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5254 | `		return PH7_OK;` |
|      - | 5255 | `	}` |
|      - | 5256 | `	/* Shuffle the string */` |
|     43 | 5257 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 5258 | `		/* Generate a random number first */` |
|     35 | 5259 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 5260 | `		/* Extract a random offset */` |
|     35 | 5261 | `		c = zString[iR % nLen];` |
|      - | 5262 | `		/* Append it */` |
|     35 | 5263 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 5264 | `	}` |
|      9 | 5265 | `	return PH7_OK;` |
|      7 | 5266 |  |
|      - | 5267 | `/*` |
|      - | 5268 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 5269 | ` *  Convert a string to an array.` |
|      - | 5270 | ` * Parameters` |
|      - | 5271 | ` * $str` |
|      - | 5272 | ` *  The input string.` |
|      - | 5273 | ` * $split_length` |
|      - | 5274 | ` *  Maximum length of the chunk.` |
|      - | 5275 | ` * Return` |
|      - | 5276 | ` *  If the optional split_length parameter is specified, the returned array` |
|      - | 5277 | ` *  will be broken down into chunks with each being split_length in length, otherwise` |
|      - | 5278 | ` *  each chunk will be one character in length. FALSE is returned if split_length is less than 1.` |
|      - | 5279 | ` *  If the split_length length exceeds the length of string, the entire string is returned` |
|      - | 5280 | ` *  as the first (and only) array element.` |
|      - | 5281 | ` */` |
|      8 | 5282 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5283 |  |
|      - | 5284 | `	const char *zString,*zEnd;` |
|      - | 5285 | `	ph7_value *pArray,*pValue;` |
|      - | 5286 | `	int split_len;` |
|      - | 5287 | `	int nLen;` |
|      9 | 5288 | `	if( nArg < 1 ){` |
|      - | 5289 | `		/* Missing arguments,return FALSE */` |
|      5 | 5290 | `		ph7_result_bool(pCtx,0);` |
|      5 | 5291 | `		return PH7_OK;` |
|      - | 5292 | `	}` |
|      - | 5293 | `	/* Point to the target string */` |
|      5 | 5294 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 5295 | `	if( nLen < 1 ){` |
|      - | 5296 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 5297 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5298 | `		return PH7_OK;` |
|      - | 5299 | `	}` |
|      5 | 5300 | `	split_len = (int)sizeof(char);` |
|      5 | 5301 | `	if( nArg > 1 ){` |
|      - | 5302 | `		/* Split length */` |
|      5 | 5303 | `		split_len = ph7_value_to_int(apArg[1]);` |
|      5 | 5304 | `		if( split_len < 1 ){` |
|      - | 5305 | `			/* Invalid length,return FALSE */` |
|      3 | 5306 | `			ph7_result_bool(pCtx,0);` |
|      3 | 5307 | `			return PH7_OK;` |
|      - | 5308 | `		}` |
|      3 | 5309 | `		if( split_len > nLen ){` |
|    ! 0 | 5310 | `			split_len = nLen;` |
|    ! 0 | 5311 | `		}` |
|      1 | 5312 | `	}` |
|      - | 5313 | `	/* Create the array and the scalar value */` |
|      3 | 5314 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 5315 | `	/*Chunk value */` |
|      3 | 5316 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 5317 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 5318 | `		/* Return FALSE */` |
|    ! 0 | 5319 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5320 | `		return PH7_OK;` |
|      - | 5321 | `	}` |
|      - | 5322 | `	/* Point to the end of the string */` |
|      3 | 5323 | `	zEnd = &zString[nLen];` |
|      - | 5324 | `	/* Perform the requested operation */` |
|      7 | 5325 | `	for(;;){` |
|      - | 5326 | `		int nMax;` |
|      9 | 5327 | `		if( zString >= zEnd ){` |
|      - | 5328 | `			/* No more input to process */` |
|      3 | 5329 | `			break;` |
|      - | 5330 | `		}` |
|      7 | 5331 | `		nMax = (int)(zEnd-zString);` |
|      7 | 5332 | `		if( nMax < split_len ){` |
|    ! 0 | 5333 | `			split_len = nMax;` |
|    ! 0 | 5334 | `		}` |
|      - | 5335 | `		/* Copy the current chunk */` |
|      7 | 5336 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 5337 | `		/* Insert it */` |
|      7 | 5338 | `		ph7_array_add_elem(pArray,0,pValue); /* Will make it's own copy */` |
|      - | 5339 | `		/* reset the string cursor */` |
|      7 | 5340 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 5341 | `		/* Update position */` |
|      7 | 5342 | `		zString += split_len;` |
|      1 | 5343 | `	}` |
|      - | 5344 | `	/*` |
|      - | 5345 | `	 * Return the array.` |
|      - | 5346 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 5347 | `	 * upon we return from this function.` |
|      - | 5348 | `	 */` |
|      3 | 5349 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5350 | `	return PH7_OK;` |
|      5 | 5351 |  |
|      - | 5352 | `/*` |
|      - | 5353 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 5354 | ` * Refer to [strspn()].` |
|      - | 5355 | ` */` |
|     28 | 5356 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 5357 |  |
|     29 | 5358 | `	const char *zIn = *pzIn;` |
|      - | 5359 | `	const char *zPtr;` |
|      - | 5360 | `	/* Ignore leading white spaces */` |
|     29 | 5361 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 5362 | `		zIn++;` |
|    ! 0 | 5363 | `	}` |
|     29 | 5364 | `	if( zIn >= zEnd ){` |
|      - | 5365 | `		/* End of input */` |
|    ! 0 | 5366 | `		return SXERR_EOF;` |
|      - | 5367 | `	}` |
|     29 | 5368 | `	zPtr = zIn;` |
|      - | 5369 | `	/* Extract the token */` |
|    201 | 5370 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 5371 | `		zIn++;` |
|      1 | 5372 | `	}` |
|     29 | 5373 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5374 | `	/* Synchronize pointers */` |
|     29 | 5375 | `	*pzIn = zIn;` |
|      - | 5376 | `	/* Return to the caller */` |
|     29 | 5377 | `	return SXRET_OK;` |
|     15 | 5378 |  |
|      - | 5379 | `/*` |
|      - | 5380 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 5381 | ` * return the longest match.` |
|      - | 5382 | ` * Refer to [strspn()].` |
|      - | 5383 | ` */` |
|     18 | 5384 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5385 |  |
|     19 | 5386 | `	const char *zEnd = &zString[nLen];` |
|     19 | 5387 | `	const char *zIn = zString;` |
|      - | 5388 | `	int i,c;` |
|     45 | 5389 | `	for(;;){` |
|     91 | 5390 | `		if( zString >= zEnd ){` |
|      7 | 5391 | `			break;` |
|      - | 5392 | `		}` |
|      - | 5393 | `		/* Extract current character */` |
|     85 | 5394 | `		c = zString[0];` |
|      - | 5395 | `		/* Perform the lookup */` |
|    383 | 5396 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 5397 | `			if( c == zMask[i] ){` |
|      - | 5398 | `				/* Character found */` |
|     73 | 5399 | `				break;` |
|      - | 5400 | `			}` |
|    150 | 5401 | `		}` |
|     85 | 5402 | `		if( i >= nMaskLen ){` |
|      - | 5403 | `			/* Character not in the current mask,break immediately */` |
|     13 | 5404 | `			break;` |
|      - | 5405 | `		}` |
|      - | 5406 | `		/* Advance cursor */` |
|     73 | 5407 | `		zString++;` |
|      1 | 5408 | `	}` |
|      - | 5409 | `	/* Longest match */` |
|     19 | 5410 | `	return (int)(zString-zIn);` |
|      1 | 5411 |  |
|      - | 5412 | `/*` |
|      - | 5413 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 5414 | ` * Refer to [strcspn()].` |
|      - | 5415 | ` */` |
|     10 | 5416 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5417 |  |
|     11 | 5418 | `	const char *zEnd = &zString[nLen];` |
|     11 | 5419 | `	const char *zIn = zString;` |
|      - | 5420 | `	int i,c;` |
|     12 | 5421 | `	for(;;){` |
|     25 | 5422 | `		if( zString >= zEnd ){` |
|      3 | 5423 | `			break;` |
|      - | 5424 | `		}` |
|      - | 5425 | `		/* Extract current character */` |
|     23 | 5426 | `		c = zString[0];` |
|      - | 5427 | `		/* Perform the lookup */` |
|     51 | 5428 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 5429 | `			if( c == zMask[i] ){` |
|      9 | 5430 | `				break;` |
|      - | 5431 | `			}` |
|     15 | 5432 | `		}` |
|     23 | 5433 | `		if( i < nMaskLen ){` |
|      - | 5434 | `			/* Character in the current mask,break immediately */` |
|      9 | 5435 | `			break;` |
|      - | 5436 | `		}` |
|      - | 5437 | `		/* Advance cursor */` |
|     15 | 5438 | `		zString++;` |
|      1 | 5439 | `	}` |
|      - | 5440 | `	/* Longest match */` |
|     11 | 5441 | `	return (int)(zString-zIn);` |
|      1 | 5442 |  |
|      - | 5443 | `/*` |
|      - | 5444 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5445 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 5446 | ` *  of characters contained within a given mask.` |
|      - | 5447 | ` * Parameters` |
|      - | 5448 | ` * $str` |
|      - | 5449 | ` *  The input string.` |
|      - | 5450 | ` * $mask` |
|      - | 5451 | ` *  The list of allowable characters.` |
|      - | 5452 | ` * $start` |
|      - | 5453 | ` *  The position in subject to start searching.` |
|      - | 5454 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5455 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5456 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5457 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5458 | ` *  start'th position from the end of subject.` |
|      - | 5459 | ` * $length` |
|      - | 5460 | ` *  The length of the segment from subject to examine.` |
|      - | 5461 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5462 | ` *  characters after the starting position.` |
|      - | 5463 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5464 | ` *  position up to length characters from the end of subject.` |
|      - | 5465 | ` * Return` |
|      - | 5466 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 5467 | ` * in mask.` |
|      - | 5468 | ` */` |
|     26 | 5469 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5470 |  |
|      - | 5471 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5472 | `	int iMasklen,iLen;` |
|      - | 5473 | `	SyString sToken;` |
|     27 | 5474 | `	int iCount = 0;` |
|      - | 5475 | `	int rc;` |
|     27 | 5476 | `	if( nArg < 2 ){` |
|      - | 5477 | `		/* Missing agruments,return zero */` |
|      3 | 5478 | `		ph7_result_int(pCtx,0);` |
|      3 | 5479 | `		return PH7_OK;` |
|      - | 5480 | `	}` |
|      - | 5481 | `	/* Extract the target string */` |
|     25 | 5482 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5483 | `	/* Extract the mask */` |
|     25 | 5484 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 5485 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 5486 | `		/* Nothing to process,return zero */` |
|      7 | 5487 | `		ph7_result_int(pCtx,0);` |
|      7 | 5488 | `		return PH7_OK;` |
|      - | 5489 | `	}` |
|     19 | 5490 | `	if( nArg > 2 ){` |
|      - | 5491 | `		int nOfft;` |
|      - | 5492 | `		/* Extract the offset */` |
|      9 | 5493 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 5494 | `		if( nOfft < 0 ){` |
|    ! 0 | 5495 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5496 | `			if( zBase > zString ){` |
|    ! 0 | 5497 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5498 | `				zString = zBase;` |
|    ! 0 | 5499 | `			}else{` |
|      - | 5500 | `				/* Invalid offset */` |
|    ! 0 | 5501 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5502 | `				return PH7_OK;` |
|      - | 5503 | `			}` |
|    ! 0 | 5504 | `		}else{` |
|      9 | 5505 | `			if( nOfft >= iLen ){` |
|      - | 5506 | `				/* Invalid offset */` |
|    ! 0 | 5507 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5508 | `				return PH7_OK;` |
|    ! 0 | 5509 | `			}else{` |
|      - | 5510 | `				/* Update offset */` |
|      9 | 5511 | `				zString += nOfft;` |
|      9 | 5512 | `				iLen -= nOfft;` |
|      - | 5513 | `			}` |
|      - | 5514 | `		}` |
|      9 | 5515 | `		if( nArg > 3 ){` |
|      - | 5516 | `			int iUserlen;` |
|      - | 5517 | `			/* Extract the desired length */` |
|      9 | 5518 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 5519 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 5520 | `				iLen = iUserlen;` |
|      2 | 5521 | `			}` |
|      4 | 5522 | `		}` |
|      4 | 5523 | `	}` |
|      - | 5524 | `	/* Point to the end of the string */` |
|     19 | 5525 | `	zEnd = &zString[iLen];` |
|      - | 5526 | `	/* Extract the first non-space token */` |
|     19 | 5527 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 5528 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5529 | `		/* Compare against the current mask */` |
|     19 | 5530 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 5531 | `	}` |
|      - | 5532 | `	/* Longest match */` |
|     19 | 5533 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 5534 | `	return PH7_OK;` |
|     14 | 5535 |  |
|      - | 5536 | `/*` |
|      - | 5537 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5538 | ` *  Find length of initial segment not matching mask.` |
|      - | 5539 | ` * Parameters` |
|      - | 5540 | ` * $str` |
|      - | 5541 | ` *  The input string.` |
|      - | 5542 | ` * $mask` |
|      - | 5543 | ` *  The list of not allowed characters.` |
|      - | 5544 | ` * $start` |
|      - | 5545 | ` *  The position in subject to start searching.` |
|      - | 5546 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5547 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5548 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5549 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5550 | ` *  start'th position from the end of subject.` |
|      - | 5551 | ` * $length` |
|      - | 5552 | ` *  The length of the segment from subject to examine.` |
|      - | 5553 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5554 | ` *  characters after the starting position.` |
|      - | 5555 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5556 | ` *  position up to length characters from the end of subject.` |
|      - | 5557 | ` * Return` |
|      - | 5558 | ` *  Returns the length of the segment as an integer.` |
|      - | 5559 | ` */` |
|     16 | 5560 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5561 |  |
|      - | 5562 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5563 | `	int iMasklen,iLen;` |
|      - | 5564 | `	SyString sToken;` |
|     17 | 5565 | `	int iCount = 0;` |
|      - | 5566 | `	int rc;` |
|     17 | 5567 | `	if( nArg < 2 ){` |
|      - | 5568 | `		/* Missing agruments,return zero */` |
|      3 | 5569 | `		ph7_result_int(pCtx,0);` |
|      3 | 5570 | `		return PH7_OK;` |
|      - | 5571 | `	}` |
|      - | 5572 | `	/* Extract the target string */` |
|     15 | 5573 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5574 | `	/* Extract the mask */` |
|     15 | 5575 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 5576 | `	if( iLen < 1 ){` |
|      - | 5577 | `		/* Nothing to process,return zero */` |
|    ! 0 | 5578 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5579 | `		return PH7_OK;` |
|      - | 5580 | `	}` |
|     15 | 5581 | `	if( iMasklen < 1 ){` |
|      - | 5582 | `		/* No given mask,return the string length */` |
|      3 | 5583 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 5584 | `		return PH7_OK;` |
|      - | 5585 | `	}` |
|     13 | 5586 | `	if( nArg > 2 ){` |
|      - | 5587 | `		int nOfft;` |
|      - | 5588 | `		/* Extract the offset */` |
|     11 | 5589 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 5590 | `		if( nOfft < 0 ){` |
|    ! 0 | 5591 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5592 | `			if( zBase > zString ){` |
|    ! 0 | 5593 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5594 | `				zString = zBase;` |
|    ! 0 | 5595 | `			}else{` |
|      - | 5596 | `				/* Invalid offset */` |
|    ! 0 | 5597 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5598 | `				return PH7_OK;` |
|      - | 5599 | `			}` |
|    ! 0 | 5600 | `		}else{` |
|     11 | 5601 | `			if( nOfft >= iLen ){` |
|      - | 5602 | `				/* Invalid offset */` |
|      3 | 5603 | `				ph7_result_int(pCtx,0);` |
|      3 | 5604 | `				return PH7_OK;` |
|    ! 0 | 5605 | `			}else{` |
|      - | 5606 | `				/* Update offset */` |
|      9 | 5607 | `				zString += nOfft;` |
|      9 | 5608 | `				iLen -= nOfft;` |
|      - | 5609 | `			}` |
|      - | 5610 | `		}` |
|      9 | 5611 | `		if( nArg > 3 ){` |
|      - | 5612 | `			int iUserlen;` |
|      - | 5613 | `			/* Extract the desired length */` |
|    ! 0 | 5614 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 5615 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 5616 | `				iLen = iUserlen;` |
|    ! 0 | 5617 | `			}` |
|    ! 0 | 5618 | `		}` |
|      4 | 5619 | `	}` |
|      - | 5620 | `	/* Point to the end of the string */` |
|     11 | 5621 | `	zEnd = &zString[iLen];` |
|      - | 5622 | `	/* Extract the first non-space token */` |
|     11 | 5623 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 5624 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5625 | `		/* Compare against the current mask */` |
|     11 | 5626 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 5627 | `	}` |
|      - | 5628 | `	/* Longest match */` |
|     11 | 5629 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 5630 | `	return PH7_OK;` |
|      9 | 5631 |  |
|      - | 5632 | `/*` |
|      - | 5633 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 5634 | ` *  Search a string for any of a set of characters.` |
|      - | 5635 | ` * Parameters` |
|      - | 5636 | ` *  $haystack` |
|      - | 5637 | ` *   The string where char_list is looked for.` |
|      - | 5638 | ` *  $char_list` |
|      - | 5639 | ` *   This parameter is case sensitive.` |
|      - | 5640 | ` * Return` |
|      - | 5641 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 5642 | ` */` |
|      6 | 5643 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5644 |  |
|      - | 5645 | `	const char *zString,*zList,*zEnd;` |
|      - | 5646 | `	int iLen,iListLen,i,c;` |
|      - | 5647 | `	sxu32 nOfft,nMax;` |
|      - | 5648 | `	sxi32 rc;` |
|      7 | 5649 | `	if( nArg < 2 ){` |
|      - | 5650 | `		/* Missing arguments,return FALSE */` |
|      3 | 5651 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5652 | `		return PH7_OK;` |
|      - | 5653 | `	}` |
|      - | 5654 | `	/* Extract the haystack and the char list */` |
|      5 | 5655 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 5656 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 5657 | `	if( iLen < 1 ){` |
|      - | 5658 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 5659 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5660 | `		return PH7_OK;` |
|      - | 5661 | `	}` |
|      - | 5662 | `	/* Point to the end of the string */` |
|      5 | 5663 | `	zEnd = &zString[iLen];` |
|      5 | 5664 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 5665 | `	/* perform the requested operation */` |
|     15 | 5666 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 5667 | `		c = zList[i];` |
|     11 | 5668 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 5669 | `		if( rc == SXRET_OK ){` |
|      5 | 5670 | `			if( nMax < nOfft ){` |
|      3 | 5671 | `				nOfft = nMax;` |
|      1 | 5672 | `			}` |
|      2 | 5673 | `		}` |
|      6 | 5674 | `	}` |
|      5 | 5675 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 5676 | `		/* No such substring,return FALSE */` |
|      3 | 5677 | `		ph7_result_bool(pCtx,0);` |
|      2 | 5678 | `	}else{` |
|      - | 5679 | `		/* Return the substring */` |
|      3 | 5680 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 5681 | `	}` |
|      5 | 5682 | `	return PH7_OK;` |
|      4 | 5683 |  |
|      - | 5684 | `/*` |
|      - | 5685 | ` * string soundex(string $str)` |
|      - | 5686 | ` *  Calculate the soundex key of a string.` |
|      - | 5687 | ` * Parameters` |
|      - | 5688 | ` *  $str` |
|      - | 5689 | ` *   The input string.` |
|      - | 5690 | ` * Return` |
|      - | 5691 | ` *  Returns the soundex key as a string.` |
|      - | 5692 | ` * Note:` |
|      - | 5693 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 5694 | ` * source tree.` |
|      - | 5695 | ` */` |
|     20 | 5696 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5697 |  |
|      - | 5698 | `	const unsigned char *zIn;` |
|      - | 5699 | `	char zResult[8];` |
|      - | 5700 | `	int i, j;` |
|      - | 5701 | `	static const unsigned char iCode[] = {` |
|      - | 5702 |  |
|      - | 5703 |  |
|      - | 5704 |  |
|      - | 5705 |  |
|      - | 5706 |  |
|      - | 5707 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5708 |  |
|      - | 5709 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5710 | `	};` |
|     21 | 5711 | `	if( nArg < 1 ){` |
|      - | 5712 | `		/* Missing arguments,return the empty string */` |
|      3 | 5713 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5714 | `		return PH7_OK;` |
|      - | 5715 | `	}` |
|     19 | 5716 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     19 | 5717 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     19 | 5718 | `	if( zIn[i] ){` |
|     17 | 5719 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 5720 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 5721 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 5722 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 5723 | `			if( code>0 ){` |
|     45 | 5724 | `				if( code!=prevcode ){` |
|     33 | 5725 | `					prevcode = (unsigned char)code;` |
|     33 | 5726 | `					zResult[j++] = (char)code + '0';` |
|     16 | 5727 | `				}` |
|     23 | 5728 | `			}else{` |
|     49 | 5729 | `				prevcode = 0;` |
|      - | 5730 | `			}` |
|     47 | 5731 | `		}` |
|     33 | 5732 | `		while( j<4 ){` |
|     17 | 5733 | `			zResult[j++] = '0';` |
|      1 | 5734 | `		}` |
|     17 | 5735 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 5736 | `	}else{` |
|      3 | 5737 | `	  ph7_result_string(pCtx,"?000",4);` |
|      - | 5738 | `	}` |
|     19 | 5739 | `	return PH7_OK;` |
|     11 | 5740 |  |
|      - | 5741 | `/*` |
|      - | 5742 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 5743 | ` *  Wraps a string to a given number of characters.` |
|      - | 5744 | ` * Parameters` |
|      - | 5745 | ` *  $str` |
|      - | 5746 | ` *   The input string.` |
|      - | 5747 | ` * $width` |
|      - | 5748 | ` *  The column width.` |
|      - | 5749 | ` * $break` |
|      - | 5750 | ` *  The line is broken using the optional break parameter.` |
|      - | 5751 | ` * Return` |
|      - | 5752 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 5753 | ` */` |
|     14 | 5754 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5755 |  |
|      - | 5756 | `	const char *zIn,*zEnd,*zBreak;` |
|      - | 5757 | `	int iLen,iBreaklen,iChunk;` |
|     15 | 5758 | `	if( nArg < 1 ){` |
|      - | 5759 | `		/* Missing arguments,return the empty string */` |
|      3 | 5760 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5761 | `		return PH7_OK;` |
|      - | 5762 | `	}` |
|      - | 5763 | `	/* Extract the input string */` |
|     13 | 5764 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|     13 | 5765 | `	if( iLen < 1 ){` |
|      - | 5766 | `		/* Nothing to process,return the empty string */` |
|      3 | 5767 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5768 | `		return PH7_OK;` |
|      - | 5769 | `	}` |
|      - | 5770 | `	/* Chunk length */` |
|     11 | 5771 | `	iChunk = 75;` |
|     11 | 5772 | `	iBreaklen = 0;` |
|     11 | 5773 | `	zBreak = ""; /* cc warning */` |
|     11 | 5774 | `	if( nArg > 1 ){` |
|     11 | 5775 | `		iChunk = ph7_value_to_int(apArg[1]);` |
|     11 | 5776 | `		if( iChunk < 1 ){` |
|    ! 0 | 5777 | `			iChunk = 75;` |
|    ! 0 | 5778 | `		}` |
|     11 | 5779 | `		if( nArg > 2 ){` |
|      3 | 5780 | `			zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      1 | 5781 | `		}` |
|      5 | 5782 | `	}` |
|     11 | 5783 | `	if( iBreaklen < 1 ){` |
|      - | 5784 | `		/* Set a default column break */` |
|      - | 5785 | `#ifdef __WINNT__` |
|      1 | 5786 | `		zBreak = "\r\n";` |
|      1 | 5787 | `		iBreaklen = (int)sizeof("\r\n")-1;` |
|      - | 5788 | `#else` |
|      8 | 5789 | `		zBreak = "\n";` |
|      8 | 5790 | `		iBreaklen = (int)sizeof(char);` |
|      - | 5791 | `#endif` |
|      4 | 5792 | `	}` |
|      - | 5793 | `	/* Perform the requested operation */` |
|     11 | 5794 | `	zEnd = &zIn[iLen];` |
|     41 | 5795 | `	for(;;){` |
|      - | 5796 | `		int nMax;` |
|     47 | 5797 | `		if( zIn >= zEnd ){` |
|      - | 5798 | `			/* No more input to process */` |
|     11 | 5799 | `			break;` |
|      - | 5800 | `		}` |
|     37 | 5801 | `		nMax = (int)(zEnd-zIn);` |
|     37 | 5802 | `		if( iChunk > nMax ){` |
|     11 | 5803 | `			iChunk = nMax;` |
|      5 | 5804 | `		}` |
|      - | 5805 | `		/* Append the column first */` |
|     37 | 5806 | `		ph7_result_string(pCtx,zIn,iChunk); /* Will make it's own copy */` |
|      - | 5807 | `		/* Advance the cursor */` |
|     37 | 5808 | `		zIn += iChunk;` |
|     37 | 5809 | `		if( zIn < zEnd ){` |
|      - | 5810 | `			/* Append the line break */` |
|     27 | 5811 | `			ph7_result_string(pCtx,zBreak,iBreaklen);` |
|     13 | 5812 | `		}` |
|      1 | 5813 | `	}` |
|     11 | 5814 | `	return PH7_OK;` |
|      8 | 5815 |  |
|      - | 5816 | `/*` |
|      - | 5817 | ` * Check if the given character is a member of the given mask.` |
|      - | 5818 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 5819 | ` * Refer to [strtok()].` |
|      - | 5820 | ` */` |
|     30 | 5821 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 5822 |  |
|      - | 5823 | `	int i;` |
|     57 | 5824 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 5825 | `		if( c == zMask[i] ){` |
|     13 | 5826 | `			if( pOfft ){` |
|      5 | 5827 | `				*pOfft = i;` |
|      2 | 5828 | `			}` |
|     13 | 5829 | `			return TRUE;` |
|      - | 5830 | `		}` |
|     14 | 5831 | `	}` |
|     19 | 5832 | `	return FALSE;` |
|     16 | 5833 |  |
|      - | 5834 | `/*` |
|      - | 5835 | ` * Extract a single token from the input stream.` |
|      - | 5836 | ` * Refer to [strtok()].` |
|      - | 5837 | ` */` |
|      6 | 5838 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 5839 |  |
|      7 | 5840 | `	const char *zIn = *pzIn;` |
|      - | 5841 | `	const char *zPtr;` |
|      - | 5842 | `	/* Ignore leading delimiter */` |
|     11 | 5843 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5844 | `		zIn++;` |
|      1 | 5845 | `	}` |
|      7 | 5846 | `	if( zIn >= zEnd ){` |
|      - | 5847 | `		/* End of input */` |
|    ! 0 | 5848 | `		return SXERR_EOF;` |
|      - | 5849 | `	}` |
|      7 | 5850 | `	zPtr = zIn;` |
|      - | 5851 | `	/* Extract the token */` |
|     13 | 5852 | `	while( zIn < zEnd ){` |
|     11 | 5853 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 5854 | `			/* UTF-8 stream */` |
|    ! 0 | 5855 | `			zIn++;` |
|    ! 0 | 5856 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 5857 | `		}else{` |
|     11 | 5858 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5859 | `				break;` |
|      - | 5860 | `			}` |
|      7 | 5861 | `			zIn++;` |
|      - | 5862 | `		}` |
|      1 | 5863 | `	}` |
|      7 | 5864 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5865 | `	/* Update the cursor */` |
|      7 | 5866 | `	*pzIn = zIn;` |
|      - | 5867 | `	/* Return to the caller */` |
|      7 | 5868 | `	return SXRET_OK;` |
|      4 | 5869 |  |
|      - | 5870 | `/* strtok auxiliary private data */` |
|      - | 5871 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 5872 | `struct strtok_aux_data` |
|      - | 5873 |  |
|      - | 5874 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 5875 | `	const char *zIn;   /* Current input stream */` |
|      - | 5876 | `	const char *zEnd;  /* End of input */` |
|      - | 5877 | `};` |
|      - | 5878 | `/*` |
|      - | 5879 | ` * string strtok(string $str,string $token)` |
|      - | 5880 | ` * string strtok(string $token)` |
|      - | 5881 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 5882 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 5883 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 5884 | ` *  words by using the space character as the token.` |
|      - | 5885 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 5886 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 5887 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 5888 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 5889 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 5890 | ` *  the argument are found.` |
|      - | 5891 | ` * Parameters` |
|      - | 5892 | ` *  $str` |
|      - | 5893 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 5894 | ` * $token` |
|      - | 5895 | ` *  The delimiter used when splitting up str.` |
|      - | 5896 | ` * Return` |
|      - | 5897 | ` *   Current token or FALSE on EOF.` |
|      - | 5898 | ` */` |
|      8 | 5899 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5900 |  |
|      - | 5901 | `	strtok_aux_data *pAux;` |
|      - | 5902 | `	const char *zMask;` |
|      - | 5903 | `	SyString sToken;` |
|      - | 5904 | `	int nMasklen;` |
|      - | 5905 | `	sxi32 rc;` |
|      9 | 5906 | `	if( nArg < 2 ){` |
|      - | 5907 | `		/* Extract top aux data */` |
|      7 | 5908 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      7 | 5909 | `		if( pAux == 0 ){` |
|      - | 5910 | `			/* No aux data,return FALSE */` |
|    ! 0 | 5911 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5912 | `			return PH7_OK;` |
|      - | 5913 | `		}` |
|      7 | 5914 | `		nMasklen = 0;` |
|      7 | 5915 | `		zMask = ""; /* cc warning */` |
|      7 | 5916 | `		if( nArg > 0 ){` |
|      - | 5917 | `			/* Extract the mask */` |
|      5 | 5918 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 5919 | `		}` |
|      7 | 5920 | `		if( nMasklen < 1 ){` |
|      - | 5921 | `			/* Invalid mask,return FALSE */` |
|      3 | 5922 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|      3 | 5923 | `			ph7_context_free_chunk(pCtx,pAux);` |
|      3 | 5924 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|      3 | 5925 | `			ph7_result_bool(pCtx,0);` |
|      3 | 5926 | `			return PH7_OK;` |
|      - | 5927 | `		}` |
|      - | 5928 | `		/* Extract the token */` |
|      5 | 5929 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 5930 | `		if( rc != SXRET_OK ){` |
|      - | 5931 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 5932 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 5933 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5934 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 5935 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5936 | `		}else{` |
|      - | 5937 | `			/* Return the extracted token */` |
|      5 | 5938 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 5939 | `		}` |
|      3 | 5940 | `	}else{` |
|      - | 5941 | `		const char *zInput,*zCur;` |
|      - | 5942 | `		char *zDup;` |
|      - | 5943 | `		int nLen;` |
|      - | 5944 | `		/* Extract the raw input */` |
|      3 | 5945 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5946 | `		if( nLen < 1 ){` |
|      - | 5947 | `			/* Empty input,return FALSE */` |
|    ! 0 | 5948 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5949 | `			return PH7_OK;` |
|      - | 5950 | `		}` |
|      - | 5951 | `		/* Extract the mask */` |
|      3 | 5952 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 5953 | `		if( nMasklen < 1 ){` |
|      - | 5954 | `			/* Set a default mask */` |
|      - | 5955 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 5956 | `			zMask = TOK_MASK;` |
|    ! 0 | 5957 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 5958 | `#undef TOK_MASK` |
|    ! 0 | 5959 | `		}` |
|      - | 5960 | `		/* Extract a single token */` |
|      3 | 5961 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 5962 | `		if( rc != SXRET_OK ){` |
|      - | 5963 | `			/* Empty input */` |
|    ! 0 | 5964 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5965 | `			return PH7_OK;` |
|    ! 0 | 5966 | `		}else{` |
|      - | 5967 | `			/* Return the extracted token */` |
|      3 | 5968 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 5969 | `		}` |
|      - | 5970 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 5971 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 5972 | `		if( pAux ){` |
|      3 | 5973 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 5974 | `			if( nLen < 1 ){` |
|    ! 0 | 5975 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5976 | `				return PH7_OK;` |
|      - | 5977 | `			}` |
|      - | 5978 | `			/* Duplicate input */` |
|      3 | 5979 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 5980 | `			if( zDup  ){` |
|      3 | 5981 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 5982 | `				/* Register the aux data */` |
|      3 | 5983 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 5984 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 5985 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 5986 | `			}` |
|      1 | 5987 | `		}` |
|      - | 5988 | `	}` |
|      7 | 5989 | `	return PH7_OK;` |
|      5 | 5990 |  |
|      - | 5991 | `/*` |
|      - | 5992 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 5993 | ` *  Pad a string to a certain length with another string` |
|      - | 5994 | ` * Parameters` |
|      - | 5995 | ` *  $input` |
|      - | 5996 | ` *   The input string.` |
|      - | 5997 | ` * $pad_length` |
|      - | 5998 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 5999 | ` *   string, no padding takes place.` |
|      - | 6000 | ` * $pad_string` |
|      - | 6001 | ` *   Note:` |
|      - | 6002 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 6003 | ` *    divided by the pad_string's length.` |
|      - | 6004 | ` * $pad_type` |
|      - | 6005 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 6006 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 6007 | ` * Return` |
|      - | 6008 | ` *  The padded string.` |
|      - | 6009 | ` */` |
|     10 | 6010 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6011 |  |
|      - | 6012 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 6013 | `	const char *zIn,*zPad;` |
|     11 | 6014 | `	if( nArg < 2 ){` |
|      - | 6015 | `		/* Missing arguments,return the empty string */` |
|      5 | 6016 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 6017 | `		return PH7_OK;` |
|      - | 6018 | `	}` |
|      - | 6019 | `	/* Extract the target string */` |
|      7 | 6020 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6021 | `	/* Padding length */` |
|      7 | 6022 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|      7 | 6023 | `	if( iPadlen > 0 ){` |
|      5 | 6024 | `		iPadlen -= iLen;` |
|      2 | 6025 | `	}` |
|      7 | 6026 | `	if( iPadlen < 1  ){` |
|      - | 6027 | `		/* Return the string verbatim */` |
|      3 | 6028 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      3 | 6029 | `		return PH7_OK;` |
|      - | 6030 | `	}` |
|      5 | 6031 | `	zPad = " "; /* Whitespace padding */` |
|      5 | 6032 | `	iStrpad = (int)sizeof(char);` |
|      5 | 6033 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      5 | 6034 | `	if( nArg > 2 ){` |
|      - | 6035 | `		/* Padding string */` |
|      5 | 6036 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      5 | 6037 | `		if( iStrpad < 1 ){` |
|      - | 6038 | `			/* Empty string */` |
|    ! 0 | 6039 | `			zPad = " "; /* Whitespace padding */` |
|    ! 0 | 6040 | `			iStrpad = (int)sizeof(char);` |
|    ! 0 | 6041 | `		}` |
|      5 | 6042 | `		if( nArg > 3 ){` |
|      - | 6043 | `			/* Padd type */` |
|      5 | 6044 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 6045 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 6046 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 6047 | `			}` |
|      2 | 6048 | `		}` |
|      2 | 6049 | `	}` |
|      5 | 6050 | `	iDiv = 1;` |
|      5 | 6051 | `	if( iType == 2 ){` |
|    ! 0 | 6052 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 6053 | `	}` |
|      - | 6054 | `	/* Perform the requested operation */` |
|      5 | 6055 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 6056 | `		jPad = iStrpad;` |
|      5 | 6057 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 6058 | `			/* Padding */` |
|      5 | 6059 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 6060 | `				break;` |
|      - | 6061 | `			}` |
|      3 | 6062 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      2 | 6063 | `		}` |
|      3 | 6064 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 6065 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 6066 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 6067 | `				if( jPad > iStrpad ){` |
|    ! 0 | 6068 | `					jPad = iStrpad;` |
|    ! 0 | 6069 | `				}` |
|      3 | 6070 | `				if( jPad < 1){` |
|    ! 0 | 6071 | `					break;` |
|      - | 6072 | `				}` |
|      3 | 6073 | `				ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 6074 | `			}` |
|      1 | 6075 | `		}` |
|      1 | 6076 | `	}` |
|      5 | 6077 | `	if( iLen > 0 ){` |
|      - | 6078 | `		/* Append the input string */` |
|      5 | 6079 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      2 | 6080 | `	}` |
|      5 | 6081 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 6082 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 6083 | `			/* Padding */` |
|      5 | 6084 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 6085 | `				break;` |
|      - | 6086 | `			}` |
|      3 | 6087 | `			ph7_result_string(pCtx,zPad,iStrpad);` |
|      2 | 6088 | `		}` |
|      5 | 6089 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 6090 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 6091 | `			if( jPad > iStrpad ){` |
|    ! 0 | 6092 | `				jPad = iStrpad;` |
|    ! 0 | 6093 | `			}` |
|      3 | 6094 | `			if( jPad < 1){` |
|    ! 0 | 6095 | `				break;` |
|      - | 6096 | `			}` |
|      3 | 6097 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 6098 | `		}` |
|      1 | 6099 | `	}` |
|      5 | 6100 | `	return PH7_OK;` |
|      6 | 6101 |  |
|      - | 6102 | `/*` |
|      - | 6103 | ` * String replacement private data.` |
|      - | 6104 | ` */` |
|      - | 6105 | `typedef struct str_replace_data str_replace_data;` |
|      - | 6106 | `struct str_replace_data` |
|      - | 6107 |  |
|      - | 6108 | `	/* The following two fields are only used by the strtr function */` |
|      - | 6109 | `	SyBlob *pWorker;         /* Working buffer */` |
|      - | 6110 | `	ProcStringMatch xMatch;  /* Pattern match routine */` |
|      - | 6111 | `	/* The following two fields are only used by the str_replace function */` |
|      - | 6112 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 6113 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 6114 | `};` |
|      - | 6115 | `/*` |
|      - | 6116 | ` * Remove a substring.` |
|      - | 6117 | ` */` |
|      - | 6118 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 6119 | `	for(;;){\` |
|      - | 6120 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 6121 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 6122 | `		++OFFT;\` |
|      - | 6123 | `	}\` |
|      - | 6124 |  |
|      - | 6125 | `/*` |
|      - | 6126 | ` * Shift right and insert algorithm.` |
|      - | 6127 | ` */` |
|      - | 6128 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 6129 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 6130 | `		for(;;){\` |
|      - | 6131 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 6132 | `			if(INLEN < 1 ) { break; }\` |
|      - | 6133 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 6134 | `			--INLEN; \` |
|      - | 6135 | `		}\` |
|      - | 6136 | `		for(;;){\` |
|      - | 6137 | `				if(ELEN < 1) { break; }\` |
|      - | 6138 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 6139 | `				OFFT++;\` |
|      - | 6140 | `				ENTRY++;\` |
|      - | 6141 | `				--ELEN;\` |
|      - | 6142 | `		}\` |
|      - | 6143 |  |
|      - | 6144 | `/*` |
|      - | 6145 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 6146 | ` * replacement string [i.e: zReplace].` |
|      - | 6147 | ` */` |
|     38 | 6148 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 6149 |  |
|     39 | 6150 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 6151 | `	sxu32 n,m;` |
|     39 | 6152 | `	n = SyBlobLength(pWorker);` |
|     39 | 6153 | `	m = nOfft;` |
|      - | 6154 | `	/* Delete the old entry */` |
|    475 | 6155 | `	STRDEL(zInput,n,m,nLen);` |
|     39 | 6156 | `	SyBlobLength(pWorker) -= nLen;` |
|     39 | 6157 | `	if( nReplen > 0 ){` |
|     33 | 6158 | `		sxi32 iRep = nReplen;` |
|      - | 6159 | `		sxi32 rc;` |
|      - | 6160 | `		/*` |
|      - | 6161 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 6162 | `		 * string.` |
|      - | 6163 | `		 */` |
|     33 | 6164 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     33 | 6165 | `		if( rc != SXRET_OK ){` |
|      - | 6166 | `			/* Simply ignore any memory failure problem */` |
|    ! 0 | 6167 | `			return SXRET_OK;` |
|      - | 6168 | `		}` |
|      - | 6169 | `		/* Perform the insertion now */` |
|     33 | 6170 | `		zInput = (char *)SyBlobData(pWorker);` |
|     33 | 6171 | `		n = SyBlobLength(pWorker);` |
|    163 | 6172 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     33 | 6173 | `		SyBlobLength(pWorker) += nReplen;` |
|     16 | 6174 | `	}` |
|     39 | 6175 | `	return SXRET_OK;` |
|     20 | 6176 |  |
|      - | 6177 | `/*` |
|      - | 6178 | ` * String replacement walker callback.` |
|      - | 6179 | ` * The following callback is invoked for each array entry that hold` |
|      - | 6180 | ` * the replace string.` |
|      - | 6181 | ` * Refer to the strtr() implementation for more information.` |
|      - | 6182 | ` */` |
|      8 | 6183 | `static int StringReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6184 |  |
|      9 | 6185 | `	str_replace_data *pRepData = (str_replace_data *)pUserData;` |
|      - | 6186 | `	const char *zTarget,*zReplace;` |
|      - | 6187 | `	SyBlob *pWorker;` |
|      - | 6188 | `	int tLen,nLen;` |
|      - | 6189 | `	sxu32 nOfft;` |
|      - | 6190 | `	sxi32 rc;` |
|      - | 6191 | `	/* Point to the working buffer */` |
|      9 | 6192 | `	pWorker = pRepData->pWorker;` |
|      9 | 6193 | `	if( !ph7_value_is_string(pKey) ){` |
|      - | 6194 | `		/* Target and replace must be a string */` |
|      3 | 6195 | `		return PH7_OK;` |
|      - | 6196 | `	}` |
|      - | 6197 | `	/* Extract the target and the replace */` |
|      7 | 6198 | `	zTarget = ph7_value_to_string(pKey,&tLen);` |
|      7 | 6199 | `	if( tLen < 1 ){` |
|      - | 6200 | `		/* Empty target,return immediately */` |
|    ! 0 | 6201 | `		return PH7_OK;` |
|      - | 6202 | `	}` |
|      - | 6203 | `	/* Perform a pattern search */` |
|      7 | 6204 | `	rc = pRepData->xMatch(SyBlobData(pWorker),SyBlobLength(pWorker),(const void *)zTarget,(sxu32)tLen,&nOfft);` |
|      7 | 6205 | `	if( rc != SXRET_OK ){` |
|      - | 6206 | `		/* Pattern not found */` |
|    ! 0 | 6207 | `		return PH7_OK;` |
|      - | 6208 | `	}` |
|      - | 6209 | `	/* Extract the replace string */` |
|      7 | 6210 | `	zReplace = ph7_value_to_string(pData,&nLen);` |
|      - | 6211 | `	/* Perform the replace process */` |
|      7 | 6212 | `	StringReplace(pWorker,nOfft,tLen,zReplace,nLen);` |
|      - | 6213 | `	/* All done */` |
|      7 | 6214 | `	return PH7_OK;` |
|      5 | 6215 |  |
|      - | 6216 | `/*` |
|      - | 6217 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 6218 | ` * to collect search/replace string.` |
|      - | 6219 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 6220 | ` */` |
|     26 | 6221 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6222 |  |
|     27 | 6223 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 6224 | `	SyString sWorker;` |
|      - | 6225 | `	const char *zIn;` |
|      - | 6226 | `	int nByte;` |
|      - | 6227 | `	/* Extract a string representation of the given argument */` |
|     27 | 6228 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 6229 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 6230 | `	if( nByte > 0 ){` |
|      - | 6231 | `		char *zDup;` |
|      - | 6232 | `		/* Duplicate the chunk */` |
|     25 | 6233 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 6234 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 6235 | `			);` |
|     25 | 6236 | `		if( zDup == 0 ){` |
|      - | 6237 | `			/* Ignore any memory failure problem */` |
|    ! 0 | 6238 | `			ph7_context_throw_error(pRep->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 6239 | `			return PH7_OK;` |
|      - | 6240 | `		}` |
|     25 | 6241 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 6242 | `		/* Save the chunk */` |
|     25 | 6243 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 6244 | `	}` |
|      - | 6245 | `	/* Save for later processing */` |
|     27 | 6246 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 6247 | `	/* All done */` |
|     13 | 6248 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 6249 | `	return PH7_OK;` |
|     14 | 6250 |  |
|      - | 6251 | `/*` |
|      - | 6252 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6253 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6254 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 6255 | ` * Parameters` |
|      - | 6256 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 6257 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 6258 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 6259 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 6260 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 6261 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 6262 | ` * $search` |
|      - | 6263 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 6264 | ` *  to designate multiple needles.` |
|      - | 6265 | ` * $replace` |
|      - | 6266 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 6267 | ` *  to designate multiple replacements.` |
|      - | 6268 | ` * $subject` |
|      - | 6269 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 6270 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 6271 | ` *  of subject, and the return value is an array as well.` |
|      - | 6272 | ` * $count (Not used)` |
|      - | 6273 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 6274 | ` * Return` |
|      - | 6275 | ` * This function returns a string or an array with the replaced values.` |
|      - | 6276 | ` */` |
|  12330 | 6277 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6278 |  |
|      - | 6279 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 6280 | `	ProcStringMatch xMatch;` |
|      - | 6281 | `	const char *zIn,*zFunc;` |
|      - | 6282 | `	str_replace_data sRep;` |
|      - | 6283 | `	SyBlob sWorker;` |
|      - | 6284 | `	SySet sReplace;` |
|      - | 6285 | `	SySet sSearch;` |
|      - | 6286 | `	int rep_str;` |
|      - | 6287 | `	int nByte;` |
|      - | 6288 | `	sxi32 rc;` |
|  12332 | 6289 | `	if( nArg < 3 ){` |
|      - | 6290 | `		/* Missing/Invalid arguments,return null */` |
|      7 | 6291 | `		ph7_result_null(pCtx);` |
|      7 | 6292 | `		return PH7_OK;` |
|      - | 6293 | `	}` |
|      - | 6294 | `	/* Initialize fields */` |
|  12326 | 6295 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  12326 | 6296 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  12326 | 6297 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  12326 | 6298 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  12326 | 6299 | `	sRep.pCtx = pCtx;` |
|  12326 | 6300 | `	sRep.pCollector = &sSearch;` |
|  12326 | 6301 | `	rep_str = 0;` |
|      - | 6302 | `	/* Extract the subject */` |
|  12326 | 6303 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  12326 | 6304 | `	if( nByte < 1 ){` |
|      - | 6305 | `		/* Nothing to replace,return the empty string */` |
|     38 | 6306 | `		ph7_result_string(pCtx,"",0);` |
|     38 | 6307 | `		return PH7_OK;` |
|      - | 6308 | `	}` |
|      - | 6309 | `	/* Copy the subject */` |
|  12290 | 6310 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 6311 | `	/* Search string */` |
|  12290 | 6312 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 6313 | `		/* Collect search string */` |
|      9 | 6314 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 6315 | `	}else{` |
|      - | 6316 | `		/* Single pattern */` |
|  12282 | 6317 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  12282 | 6318 | `		if( nByte < 1 ){` |
|      - | 6319 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 6320 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 6321 | `			return PH7_OK;` |
|      - | 6322 | `		}` |
|  12278 | 6323 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6324 | `		/* Save for later processing */` |
|  12278 | 6325 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 6326 | `	}` |
|      - | 6327 | `	/* Replace string */` |
|  12286 | 6328 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 6329 | `		/* Collect replace string */` |
|      7 | 6330 | `		sRep.pCollector = &sReplace;` |
|      7 | 6331 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 6332 | `	}else{` |
|      - | 6333 | `		/* Single needle */` |
|  12280 | 6334 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  12280 | 6335 | `		rep_str = 1;` |
|  12280 | 6336 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6337 | `		/* Save for later processing */` |
|  12280 | 6338 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 6339 | `	}` |
|      - | 6340 | `	/* Reset loop cursors */` |
|  12286 | 6341 | `	SySetResetCursor(&sSearch);` |
|  12286 | 6342 | `	SySetResetCursor(&sReplace);` |
|  12286 | 6343 | `	pReplace = pSearch = 0; /* cc warning */` |
|  12286 | 6344 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 6345 | `	/* Extract function name */` |
|  12286 | 6346 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 6347 | `	/* Set the default pattern match routine */` |
|  12286 | 6348 | `	xMatch = SyBlobSearch;` |
|  12286 | 6349 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 6350 | `		/* Case insensitive pattern match */` |
|     11 | 6351 | `		xMatch = iPatternMatch;` |
|      5 | 6352 | `	}` |
|      - | 6353 | `	/* Start the replace process */` |
|  24578 | 6354 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 6355 | `		sxu32 nCount,nOfft;` |
|  12294 | 6356 | `		if( pSearch->nByte <  1 ){` |
|      - | 6357 | `			/* Empty string,ignore */` |
|      3 | 6358 | `			continue;` |
|      - | 6359 | `		}` |
|      - | 6360 | `		/* Extract the replace string */` |
|  12292 | 6361 | `		if( rep_str ){` |
|  12282 | 6362 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|   6142 | 6363 | `		}else{` |
|     11 | 6364 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 6365 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 6366 | `				 * An empty string is used for the rest of replacement values` |
|      - | 6367 | `				 */` |
|      3 | 6368 | `				pReplace = 0;` |
|      1 | 6369 | `			}` |
|      - | 6370 | `		}` |
|  12292 | 6371 | `		if( pReplace == 0 ){` |
|      - | 6372 | `			/* Use an empty string instead */` |
|      3 | 6373 | `			pReplace = &sTemp;` |
|      1 | 6374 | `		}` |
|  12292 | 6375 | `		nOfft = nCount = 0;` |
|   6161 | 6376 | `		for(;;){` |
|  12324 | 6377 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 6378 | `				break;` |
|      - | 6379 | `			}` |
|      - | 6380 | `			/* Perform a pattern lookup */` |
|  18467 | 6381 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  12310 | 6382 | `				pSearch->nByte,&nOfft);` |
|  12312 | 6383 | `			if( rc != SXRET_OK ){` |
|      - | 6384 | `				/* Pattern not found */` |
|  12280 | 6385 | `				break;` |
|      - | 6386 | `			}` |
|      - | 6387 | `			/* Perform the replace operation */` |
|     33 | 6388 | `			StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|      - | 6389 | `			/* Increment offset counter */` |
|     33 | 6390 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 6391 | `		}` |
|      2 | 6392 | `	}` |
|      - | 6393 | `	/* All done,clean-up the mess left behind */` |
|  12286 | 6394 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  12286 | 6395 | `	SySetRelease(&sSearch);` |
|  12286 | 6396 | `	SySetRelease(&sReplace);` |
|  12286 | 6397 | `	SyBlobRelease(&sWorker);` |
|  12286 | 6398 | `	return PH7_OK;` |
|   6167 | 6399 |  |
|      - | 6400 | `/*` |
|      - | 6401 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 6402 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 6403 | ` *  Translate characters or replace substrings.` |
|      - | 6404 | ` * Parameters` |
|      - | 6405 | ` *  $str` |
|      - | 6406 | ` *  The string being translated.` |
|      - | 6407 | ` * $from` |
|      - | 6408 | ` *  The string being translated to to.` |
|      - | 6409 | ` * $to` |
|      - | 6410 | ` *  The string replacing from.` |
|      - | 6411 | ` * $replace_pairs` |
|      - | 6412 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 6413 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 6414 | ` * Return` |
|      - | 6415 | ` *  The translated string.` |
|      - | 6416 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 6417 | ` */` |
|     12 | 6418 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6419 |  |
|      - | 6420 | `	const char *zIn;` |
|      - | 6421 | `	int nLen;` |
|     13 | 6422 | `	if( nArg < 1 ){` |
|      - | 6423 | `		/* Nothing to replace,return FALSE */` |
|      7 | 6424 | `		ph7_result_bool(pCtx,0);` |
|      7 | 6425 | `		return PH7_OK;` |
|      - | 6426 | `	}` |
|      7 | 6427 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 6428 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 6429 | `		/* Invalid arguments */` |
|    ! 0 | 6430 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6431 | `		return PH7_OK;` |
|      - | 6432 | `	}` |
|      9 | 6433 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 6434 | `		str_replace_data sRepData;` |
|      - | 6435 | `		SyBlob sWorker;` |
|      - | 6436 | `		/* Initilaize the working buffer */` |
|      5 | 6437 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|      - | 6438 | `		/* Copy raw string */` |
|      5 | 6439 | `		SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nLen);` |
|      - | 6440 | `		/* Init our replace data instance */` |
|      5 | 6441 | `		sRepData.pWorker = &sWorker;` |
|      5 | 6442 | `		sRepData.xMatch = SyBlobSearch;` |
|      - | 6443 | `		/* Iterate throw array entries and perform the replace operation.*/` |
|      5 | 6444 | `		ph7_array_walk(apArg[1],StringReplaceWalker,&sRepData);` |
|      - | 6445 | `		/* All done, return the result string */` |
|      7 | 6446 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|      4 | 6447 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 6448 | `		/* Clean-up */` |
|      5 | 6449 | `		SyBlobRelease(&sWorker);` |
|      3 | 6450 | `	}else{` |
|      - | 6451 | `		int i,flen,tlen,c,iOfft;` |
|      - | 6452 | `		const char *zFrom,*zTo;` |
|      3 | 6453 | `		if( nArg < 3 ){` |
|      - | 6454 | `			/* Nothing to replace */` |
|    ! 0 | 6455 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6456 | `			return PH7_OK;` |
|      - | 6457 | `		}` |
|      - | 6458 | `		/* Extract given arguments */` |
|      3 | 6459 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 6460 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 6461 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 6462 | `			/* Nothing to replace */` |
|    ! 0 | 6463 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6464 | `			return PH7_OK;` |
|      - | 6465 | `		}` |
|      - | 6466 | `		/* Start the replace process */` |
|     13 | 6467 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 6468 | `			c = zIn[i];` |
|     11 | 6469 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 6470 | `				if ( iOfft < tlen ){` |
|      5 | 6471 | `					c = zTo[iOfft];` |
|      2 | 6472 | `				}` |
|      2 | 6473 | `			}` |
|     11 | 6474 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 6475 |  |
|      6 | 6476 | `		}` |
|      - | 6477 | `	}` |
|      7 | 6478 | `	return PH7_OK;` |
|      7 | 6479 |  |
|      - | 6480 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6481 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6482 | `/*` |
|      - | 6483 | ` * Parse an INI string.` |
|      - | 6484 |  |
|      - | 6485 | ` * According to wikipedia` |
|      - | 6486 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 6487 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 6488 | ` *  Format` |
|      - | 6489 | `*    Properties` |
|      - | 6490 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 6491 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 6492 | `*     Example:` |
|      - | 6493 | `*      name=value` |
|      - | 6494 | `*    Sections` |
|      - | 6495 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 6496 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 6497 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 6498 | `*     or the end of the file. Sections may not be nested.` |
|      - | 6499 | `*     Example:` |
|      - | 6500 | `*      [section]` |
|      - | 6501 | `*   Comments` |
|      - | 6502 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 6503 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 6504 | `*/` |
|     12 | 6505 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 6506 |  |
|      - | 6507 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 6508 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 6509 | `	SyHashEntry *pEntry;` |
|      - | 6510 | `	SyString sEntry;` |
|      - | 6511 | `	SyHash sHash;` |
|      - | 6512 | `	int c;` |
|      - | 6513 | `	/* Create an empty array and worker variables */` |
|     13 | 6514 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 6515 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 6516 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 6517 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 6518 | `		/* Out of memory */` |
|    ! 0 | 6519 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      - | 6520 | `		/* Return FALSE */` |
|    ! 0 | 6521 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6522 | `		return PH7_OK;` |
|      - | 6523 | `	}` |
|     13 | 6524 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 6525 | `	pCur = pArray;` |
|      - | 6526 | `	/* Start the parse process */` |
|     21 | 6527 | `	for(;;){` |
|      - | 6528 | `		/* Ignore leading white spaces */` |
|     69 | 6529 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 6530 | `			zIn++;` |
|      1 | 6531 | `		}` |
|     43 | 6532 | `		if( zIn >= zEnd ){` |
|      - | 6533 | `			/* No more input to process */` |
|     13 | 6534 | `			break;` |
|      - | 6535 | `		}` |
|     31 | 6536 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6537 | `			/* Comment til the end of line */` |
|    ! 0 | 6538 | `			zIn++;` |
|    ! 0 | 6539 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 6540 | `				zIn++;` |
|    ! 0 | 6541 | `			}` |
|    ! 0 | 6542 | `			continue;` |
|      - | 6543 | `		}` |
|      - | 6544 | `		/* Reset the string cursor of the working variable */` |
|     31 | 6545 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 6546 | `		if( zIn[0] == '[' ){` |
|      - | 6547 | `			/* Section: Extract the section name */` |
|      9 | 6548 | `			zIn++;` |
|      9 | 6549 | `			zCur = zIn;` |
|     73 | 6550 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 6551 | `				zIn++;` |
|      1 | 6552 | `			}` |
|      9 | 6553 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 6554 | `				/* Save the section name */` |
|      5 | 6555 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 6556 | `				SyStringFullTrim(&sEntry);` |
|      5 | 6557 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 6558 | `				if( sEntry.nByte > 0 ){` |
|      - | 6559 | `					/* Associate an array with the section */` |
|      5 | 6560 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 6561 | `					if( pSection ){` |
|      5 | 6562 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 6563 | `						pCur = pSection;` |
|      2 | 6564 | `					}` |
|      2 | 6565 | `				}` |
|      2 | 6566 | `			}` |
|      9 | 6567 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 6568 | `		}else{` |
|      - | 6569 | `			ph7_value *pOldCur;` |
|      - | 6570 | `			int is_array;` |
|      - | 6571 | `			int iLen;` |
|      - | 6572 | `			/* Properties */` |
|     23 | 6573 | `			is_array = 0;` |
|     23 | 6574 | `			zCur = zIn;` |
|     23 | 6575 | `			iLen = 0; /* cc warning */` |
|     23 | 6576 | `			pOldCur = pCur;` |
|    155 | 6577 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 6578 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 6579 | `					/* Array */` |
|    ! 0 | 6580 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 6581 | `					is_array = 1;` |
|    ! 0 | 6582 | `					if( iLen > 0 ){` |
|    ! 0 | 6583 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 6584 | `						/* Query the hashtable */` |
|    ! 0 | 6585 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 6586 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 6587 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 6588 | `						if( pEntry ){` |
|    ! 0 | 6589 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 6590 | `						}else{` |
|      - | 6591 | `							/* Create an empty array */` |
|    ! 0 | 6592 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 6593 | `							if( pvArr ){` |
|      - | 6594 | `								/* Save the entry */` |
|    ! 0 | 6595 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 6596 | `								/* Insert the entry */` |
|    ! 0 | 6597 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6598 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 6599 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 6600 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6601 | `							}` |
|      - | 6602 | `						}` |
|    ! 0 | 6603 | `						if( pvArr ){` |
|    ! 0 | 6604 | `							pCur = pvArr;` |
|    ! 0 | 6605 | `						}` |
|    ! 0 | 6606 | `					}` |
|    ! 0 | 6607 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 6608 | `						zIn++;` |
|    ! 0 | 6609 | `					}` |
|    ! 0 | 6610 | `				}` |
|    133 | 6611 | `				zIn++;` |
|      1 | 6612 | `			}` |
|     23 | 6613 | `			if( !is_array ){` |
|     23 | 6614 | `				iLen = (int)(zIn-zCur);` |
|     11 | 6615 | `			}` |
|      - | 6616 | `			/* Trim the key */` |
|     23 | 6617 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 6618 | `			SyStringFullTrim(&sEntry);` |
|     23 | 6619 | `			if( sEntry.nByte > 0 ){` |
|     23 | 6620 | `				if( !is_array ){` |
|      - | 6621 | `					/* Save the key name */` |
|     23 | 6622 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 6623 | `				}` |
|      - | 6624 | `				/* extract key value */` |
|     23 | 6625 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 6626 | `				zIn++; /* '=' */` |
|     39 | 6627 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 6628 | `					zIn++;` |
|      1 | 6629 | `				}` |
|     23 | 6630 | `				if( zIn < zEnd ){` |
|     21 | 6631 | `					zCur = zIn;` |
|     21 | 6632 | `					c = zIn[0];` |
|     21 | 6633 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6634 | `						zIn++;` |
|      - | 6635 | `						/* Delimit the value */` |
|    ! 0 | 6636 | `						while( zIn < zEnd ){` |
|    ! 0 | 6637 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 6638 | `								break;` |
|      - | 6639 | `							}` |
|    ! 0 | 6640 | `							zIn++;` |
|    ! 0 | 6641 | `						}` |
|    ! 0 | 6642 | `						if( zIn < zEnd ){` |
|    ! 0 | 6643 | `							zIn++;` |
|    ! 0 | 6644 | `						}` |
|    ! 0 | 6645 | `					}else{` |
|    125 | 6646 | `						while( zIn < zEnd ){` |
|    123 | 6647 | `							if( zIn[0] == '\n' ){` |
|     19 | 6648 | `								if( zIn[-1] != '\\' ){` |
|     19 | 6649 | `									break;` |
|    ! 0 | 6650 | `								}` |
|    105 | 6651 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6652 | `								/* Inline comments */` |
|    ! 0 | 6653 | `								break;` |
|      - | 6654 | `							}` |
|    105 | 6655 | `							zIn++;` |
|      1 | 6656 | `						}` |
|      - | 6657 | `					}` |
|      - | 6658 | `					/* Trim the value */` |
|     21 | 6659 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 6660 | `					SyStringFullTrim(&sEntry);` |
|     21 | 6661 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6662 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 6663 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 6664 | `					}` |
|     21 | 6665 | `					if( sEntry.nByte > 0 ){` |
|     21 | 6666 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 6667 | `					}` |
|      - | 6668 | `					/* Insert the key and it's value */` |
|     21 | 6669 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 6670 | `				}` |
|     12 | 6671 | `			}else{` |
|    ! 0 | 6672 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 6673 | `					zIn++;` |
|    ! 0 | 6674 | `				}` |
|      - | 6675 | `			}` |
|     23 | 6676 | `			pCur = pOldCur;` |
|      - | 6677 | `		}` |
|      1 | 6678 | `	}` |
|     13 | 6679 | `	SyHashRelease(&sHash);` |
|      - | 6680 | `	/* Return the parse of the INI string */` |
|     13 | 6681 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 6682 | `	return SXRET_OK;` |
|      7 | 6683 |  |
|      - | 6684 | `/*` |
|      - | 6685 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 6686 | ` *  Parse a configuration string.` |
|      - | 6687 | ` * Parameters` |
|      - | 6688 | ` *  $ini` |
|      - | 6689 | ` *   The contents of the ini file being parsed.` |
|      - | 6690 | ` *  $process_sections` |
|      - | 6691 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 6692 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 6693 | ` *  $scanner_mode (Not used)` |
|      - | 6694 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 6695 | ` *   then option values will not be parsed.` |
|      - | 6696 | ` * Return` |
|      - | 6697 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 6698 | ` */` |
|     10 | 6699 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6700 |  |
|      - | 6701 | `	const char *zIni;` |
|      - | 6702 | `	int nByte;` |
|     11 | 6703 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6704 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 6705 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6706 | `		return PH7_OK;` |
|      - | 6707 | `	}` |
|      - | 6708 | `	/* Extract the raw INI buffer */` |
|     11 | 6709 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 6710 | `	/* Process the INI buffer*/` |
|     11 | 6711 | `	PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|     11 | 6712 | `	return PH7_OK;` |
|      6 | 6713 |  |
|      - | 6714 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6715 |  |
|      - | 6716 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6717 |  |
|      - | 6718 | `/*` |
|      - | 6719 | ` * Ctype Functions.` |
|      - | 6720 | ` * Status:` |
|      - | 6721 | ` *    Stable.` |
|      - | 6722 | ` */` |
|      - | 6723 | `/*` |
|      - | 6724 | ` * bool ctype_alnum(string $text)` |
|      - | 6725 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 6726 | ` * Parameters` |
|      - | 6727 | ` *  $text` |
|      - | 6728 | ` *   The tested string.` |
|      - | 6729 | ` * Return` |
|      - | 6730 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 6731 | ` */` |
|     16 | 6732 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6733 |  |
|      - | 6734 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6735 | `	int nLen;` |
|     17 | 6736 | `	if( nArg < 1 ){` |
|      - | 6737 | `		/* Missing arguments,return FALSE */` |
|      3 | 6738 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6739 | `		return PH7_OK;` |
|      - | 6740 | `	}` |
|      - | 6741 | `	/* Extract the target string */` |
|     15 | 6742 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 6743 | `	zEnd = &zIn[nLen];` |
|     15 | 6744 | `	if( nLen < 1 ){` |
|      - | 6745 | `		/* Empty string,return FALSE */` |
|      3 | 6746 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6747 | `		return PH7_OK;` |
|      - | 6748 | `	}` |
|      - | 6749 | `	/* Perform the requested operation */` |
|     32 | 6750 | `	for(;;){` |
|     65 | 6751 | `		if( zIn >= zEnd ){` |
|      - | 6752 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6753 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6754 | `			return PH7_OK;` |
|      - | 6755 | `		}` |
|     57 | 6756 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 6757 | `			break;` |
|      - | 6758 | `		}` |
|      - | 6759 | `		/* Point to the next character */` |
|     53 | 6760 | `		zIn++;` |
|      1 | 6761 | `	}` |
|      - | 6762 | `	/* The test failed,return FALSE */` |
|      5 | 6763 | `	ph7_result_bool(pCtx,0);` |
|      5 | 6764 | `	return PH7_OK;` |
|      9 | 6765 |  |
|      - | 6766 | `/*` |
|      - | 6767 | ` * bool ctype_alpha(string $text)` |
|      - | 6768 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 6769 | ` * Parameters` |
|      - | 6770 | ` *  $text` |
|      - | 6771 | ` *   The tested string.` |
|      - | 6772 | ` * Return` |
|      - | 6773 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 6774 | ` */` |
|     18 | 6775 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6776 |  |
|      - | 6777 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6778 | `	int nLen;` |
|     19 | 6779 | `	if( nArg < 1 ){` |
|      - | 6780 | `		/* Missing arguments,return FALSE */` |
|      3 | 6781 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6782 | `		return PH7_OK;` |
|      - | 6783 | `	}` |
|      - | 6784 | `	/* Extract the target string */` |
|     17 | 6785 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6786 | `	zEnd = &zIn[nLen];` |
|     17 | 6787 | `	if( nLen < 1 ){` |
|      - | 6788 | `		/* Empty string,return FALSE */` |
|      3 | 6789 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6790 | `		return PH7_OK;` |
|      - | 6791 | `	}` |
|      - | 6792 | `	/* Perform the requested operation */` |
|     42 | 6793 | `	for(;;){` |
|     85 | 6794 | `		if( zIn >= zEnd ){` |
|      - | 6795 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6796 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6797 | `			return PH7_OK;` |
|      - | 6798 | `		}` |
|     77 | 6799 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 6800 | `			break;` |
|      - | 6801 | `		}` |
|      - | 6802 | `		/* Point to the next character */` |
|     71 | 6803 | `		zIn++;` |
|      1 | 6804 | `	}` |
|      - | 6805 | `	/* The test failed,return FALSE */` |
|      7 | 6806 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6807 | `	return PH7_OK;` |
|     10 | 6808 |  |
|      - | 6809 | `/*` |
|      - | 6810 | ` * bool ctype_cntrl(string $text)` |
|      - | 6811 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 6812 | ` * Parameters` |
|      - | 6813 | ` *  $text` |
|      - | 6814 | ` *   The tested string.` |
|      - | 6815 | ` * Return` |
|      - | 6816 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 6817 | ` */` |
|     18 | 6818 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6819 |  |
|      - | 6820 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6821 | `	int nLen;` |
|     19 | 6822 | `	if( nArg < 1 ){` |
|      - | 6823 | `		/* Missing arguments,return FALSE */` |
|      3 | 6824 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6825 | `		return PH7_OK;` |
|      - | 6826 | `	}` |
|      - | 6827 | `	/* Extract the target string */` |
|     17 | 6828 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6829 | `	zEnd = &zIn[nLen];` |
|     17 | 6830 | `	if( nLen < 1 ){` |
|      - | 6831 | `		/* Empty string,return FALSE */` |
|      3 | 6832 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6833 | `		return PH7_OK;` |
|      - | 6834 | `	}` |
|      - | 6835 | `	/* Perform the requested operation */` |
|     14 | 6836 | `	for(;;){` |
|     29 | 6837 | `		if( zIn >= zEnd ){` |
|      - | 6838 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6839 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6840 | `			return PH7_OK;` |
|      - | 6841 | `		}` |
|     21 | 6842 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6843 | `			/* UTF-8 stream  */` |
|    ! 0 | 6844 | `			break;` |
|      - | 6845 | `		}` |
|     21 | 6846 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 6847 | `			break;` |
|      - | 6848 | `		}` |
|      - | 6849 | `		/* Point to the next character */` |
|     15 | 6850 | `		zIn++;` |
|      1 | 6851 | `	}` |
|      - | 6852 | `	/* The test failed,return FALSE */` |
|      7 | 6853 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6854 | `	return PH7_OK;` |
|     10 | 6855 |  |
|      - | 6856 | `/*` |
|      - | 6857 | ` * bool ctype_digit(string $text)` |
|      - | 6858 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 6859 | ` * Parameters` |
|      - | 6860 | ` *  $text` |
|      - | 6861 | ` *   The tested string.` |
|      - | 6862 | ` * Return` |
|      - | 6863 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 6864 | ` */` |
|   1524 | 6865 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6866 |  |
|      - | 6867 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6868 | `	int nLen;` |
|   1526 | 6869 | `	if( nArg < 1 ){` |
|      - | 6870 | `		/* Missing arguments,return FALSE */` |
|      3 | 6871 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6872 | `		return PH7_OK;` |
|      - | 6873 | `	}` |
|      - | 6874 | `	/* Extract the target string */` |
|   1524 | 6875 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1524 | 6876 | `	zEnd = &zIn[nLen];` |
|   1524 | 6877 | `	if( nLen < 1 ){` |
|      - | 6878 | `		/* Empty string,return FALSE */` |
|      3 | 6879 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6880 | `		return PH7_OK;` |
|      - | 6881 | `	}` |
|      - | 6882 | `	/* Perform the requested operation */` |
|   1424 | 6883 | `	for(;;){` |
|   2850 | 6884 | `		if( zIn >= zEnd ){` |
|      - | 6885 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1298 | 6886 | `			ph7_result_bool(pCtx,1);` |
|   1298 | 6887 | `			return PH7_OK;` |
|      - | 6888 | `		}` |
|   1554 | 6889 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6890 | `			/* UTF-8 stream  */` |
|    ! 0 | 6891 | `			break;` |
|      - | 6892 | `		}` |
|   1554 | 6893 | `		if( !SyisDigit(zIn[0]) ){` |
|    226 | 6894 | `			break;` |
|      - | 6895 | `		}` |
|      - | 6896 | `		/* Point to the next character */` |
|   1330 | 6897 | `		zIn++;` |
|      2 | 6898 | `	}` |
|      - | 6899 | `	/* The test failed,return FALSE */` |
|    226 | 6900 | `	ph7_result_bool(pCtx,0);` |
|    226 | 6901 | `	return PH7_OK;` |
|    764 | 6902 |  |
|      - | 6903 | `/*` |
|      - | 6904 | ` * bool ctype_xdigit(string $text)` |
|      - | 6905 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 6906 | ` * Parameters` |
|      - | 6907 | ` *  $text` |
|      - | 6908 | ` *   The tested string.` |
|      - | 6909 | ` * Return` |
|      - | 6910 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 6911 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 6912 | ` */` |
|     20 | 6913 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6914 |  |
|      - | 6915 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6916 | `	int nLen;` |
|     21 | 6917 | `	if( nArg < 1 ){` |
|      - | 6918 | `		/* Missing arguments,return FALSE */` |
|      3 | 6919 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6920 | `		return PH7_OK;` |
|      - | 6921 | `	}` |
|      - | 6922 | `	/* Extract the target string */` |
|     19 | 6923 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 6924 | `	zEnd = &zIn[nLen];` |
|     19 | 6925 | `	if( nLen < 1 ){` |
|      - | 6926 | `		/* Empty string,return FALSE */` |
|      3 | 6927 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6928 | `		return PH7_OK;` |
|      - | 6929 | `	}` |
|      - | 6930 | `	/* Perform the requested operation */` |
|     46 | 6931 | `	for(;;){` |
|     93 | 6932 | `		if( zIn >= zEnd ){` |
|      - | 6933 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 6934 | `			ph7_result_bool(pCtx,1);` |
|     11 | 6935 | `			return PH7_OK;` |
|      - | 6936 | `		}` |
|     83 | 6937 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6938 | `			/* UTF-8 stream  */` |
|    ! 0 | 6939 | `			break;` |
|      - | 6940 | `		}` |
|     83 | 6941 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 6942 | `			break;` |
|      - | 6943 | `		}` |
|      - | 6944 | `		/* Point to the next character */` |
|     77 | 6945 | `		zIn++;` |
|      1 | 6946 | `	}` |
|      - | 6947 | `	/* The test failed,return FALSE */` |
|      7 | 6948 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6949 | `	return PH7_OK;` |
|     11 | 6950 |  |
|      - | 6951 | `/*` |
|      - | 6952 | ` * bool ctype_graph(string $text)` |
|      - | 6953 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 6954 | ` * Parameters` |
|      - | 6955 | ` *  $text` |
|      - | 6956 | ` *   The tested string.` |
|      - | 6957 | ` * Return` |
|      - | 6958 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 6959 | ` * (no white space), FALSE otherwise.` |
|      - | 6960 | ` */` |
|     18 | 6961 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6962 |  |
|      - | 6963 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6964 | `	int nLen;` |
|     19 | 6965 | `	if( nArg < 1 ){` |
|      - | 6966 | `		/* Missing arguments,return FALSE */` |
|      3 | 6967 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6968 | `		return PH7_OK;` |
|      - | 6969 | `	}` |
|      - | 6970 | `	/* Extract the target string */` |
|     17 | 6971 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6972 | `	zEnd = &zIn[nLen];` |
|     17 | 6973 | `	if( nLen < 1 ){` |
|      - | 6974 | `		/* Empty string,return FALSE */` |
|      3 | 6975 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6976 | `		return PH7_OK;` |
|      - | 6977 | `	}` |
|      - | 6978 | `	/* Perform the requested operation */` |
|     57 | 6979 | `	for(;;){` |
|    115 | 6980 | `		if( zIn >= zEnd ){` |
|      - | 6981 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6982 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6983 | `			return PH7_OK;` |
|      - | 6984 | `		}` |
|    107 | 6985 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6986 | `			/* UTF-8 stream  */` |
|    ! 0 | 6987 | `			break;` |
|      - | 6988 | `		}` |
|    107 | 6989 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 6990 | `			break;` |
|      - | 6991 | `		}` |
|      - | 6992 | `		/* Point to the next character */` |
|    101 | 6993 | `		zIn++;` |
|      1 | 6994 | `	}` |
|      - | 6995 | `	/* The test failed,return FALSE */` |
|      7 | 6996 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6997 | `	return PH7_OK;` |
|     10 | 6998 |  |
|      - | 6999 | `/*` |
|      - | 7000 | ` * bool ctype_print(string $text)` |
|      - | 7001 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 7002 | ` * Parameters` |
|      - | 7003 | ` *  $text` |
|      - | 7004 | ` *   The tested string.` |
|      - | 7005 | ` * Return` |
|      - | 7006 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 7007 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 7008 | ` *  or control function at all.` |
|      - | 7009 | ` */` |
|     18 | 7010 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7011 |  |
|      - | 7012 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7013 | `	int nLen;` |
|     19 | 7014 | `	if( nArg < 1 ){` |
|      - | 7015 | `		/* Missing arguments,return FALSE */` |
|      3 | 7016 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7017 | `		return PH7_OK;` |
|      - | 7018 | `	}` |
|      - | 7019 | `	/* Extract the target string */` |
|     17 | 7020 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7021 | `	zEnd = &zIn[nLen];` |
|     17 | 7022 | `	if( nLen < 1 ){` |
|      - | 7023 | `		/* Empty string,return FALSE */` |
|      3 | 7024 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7025 | `		return PH7_OK;` |
|      - | 7026 | `	}` |
|      - | 7027 | `	/* Perform the requested operation */` |
|     63 | 7028 | `	for(;;){` |
|    127 | 7029 | `		if( zIn >= zEnd ){` |
|      - | 7030 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7031 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7032 | `			return PH7_OK;` |
|      - | 7033 | `		}` |
|    119 | 7034 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7035 | `			/* UTF-8 stream  */` |
|    ! 0 | 7036 | `			break;` |
|      - | 7037 | `		}` |
|    119 | 7038 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 7039 | `			break;` |
|      - | 7040 | `		}` |
|      - | 7041 | `		/* Point to the next character */` |
|    113 | 7042 | `		zIn++;` |
|      1 | 7043 | `	}` |
|      - | 7044 | `	/* The test failed,return FALSE */` |
|      7 | 7045 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7046 | `	return PH7_OK;` |
|     10 | 7047 |  |
|      - | 7048 | `/*` |
|      - | 7049 | ` * bool ctype_punct(string $text)` |
|      - | 7050 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 7051 | ` * Parameters` |
|      - | 7052 | ` *  $text` |
|      - | 7053 | ` *   The tested string.` |
|      - | 7054 | ` * Return` |
|      - | 7055 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 7056 | ` *  digit or blank, FALSE otherwise.` |
|      - | 7057 | ` */` |
|     20 | 7058 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7059 |  |
|      - | 7060 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7061 | `	int nLen;` |
|     21 | 7062 | `	if( nArg < 1 ){` |
|      - | 7063 | `		/* Missing arguments,return FALSE */` |
|      3 | 7064 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7065 | `		return PH7_OK;` |
|      - | 7066 | `	}` |
|      - | 7067 | `	/* Extract the target string */` |
|     19 | 7068 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 7069 | `	zEnd = &zIn[nLen];` |
|     19 | 7070 | `	if( nLen < 1 ){` |
|      - | 7071 | `		/* Empty string,return FALSE */` |
|      3 | 7072 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7073 | `		return PH7_OK;` |
|      - | 7074 | `	}` |
|      - | 7075 | `	/* Perform the requested operation */` |
|     38 | 7076 | `	for(;;){` |
|     77 | 7077 | `		if( zIn >= zEnd ){` |
|      - | 7078 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7079 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7080 | `			return PH7_OK;` |
|      - | 7081 | `		}` |
|     69 | 7082 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7083 | `			/* UTF-8 stream  */` |
|    ! 0 | 7084 | `			break;` |
|      - | 7085 | `		}` |
|     69 | 7086 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 7087 | `			break;` |
|      - | 7088 | `		}` |
|      - | 7089 | `		/* Point to the next character */` |
|     61 | 7090 | `		zIn++;` |
|      1 | 7091 | `	}` |
|      - | 7092 | `	/* The test failed,return FALSE */` |
|      9 | 7093 | `	ph7_result_bool(pCtx,0);` |
|      9 | 7094 | `	return PH7_OK;` |
|     11 | 7095 |  |
|      - | 7096 | `/*` |
|      - | 7097 | ` * bool ctype_space(string $text)` |
|      - | 7098 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 7099 | ` * Parameters` |
|      - | 7100 | ` *  $text` |
|      - | 7101 | ` *   The tested string.` |
|      - | 7102 | ` * Return` |
|      - | 7103 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 7104 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 7105 | ` *  and form feed characters.` |
|      - | 7106 | ` */` |
|  37570 | 7107 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 7108 |  |
|      - | 7109 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7110 | `	int nLen;` |
|  37572 | 7111 | `	if( nArg < 1 ){` |
|      - | 7112 | `		/* Missing arguments,return FALSE */` |
|      3 | 7113 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7114 | `		return PH7_OK;` |
|      - | 7115 | `	}` |
|      - | 7116 | `	/* Extract the target string */` |
|  37570 | 7117 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  37570 | 7118 | `	zEnd = &zIn[nLen];` |
|  37570 | 7119 | `	if( nLen < 1 ){` |
|      - | 7120 | `		/* Empty string,return FALSE */` |
|      3 | 7121 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7122 | `		return PH7_OK;` |
|      - | 7123 | `	}` |
|      - | 7124 | `	/* Perform the requested operation */` |
|  19127 | 7125 | `	for(;;){` |
|  38212 | 7126 | `		if( zIn >= zEnd ){` |
|      - | 7127 | `			/* If we reach the end of the string,then the test succeeded. */` |
|    622 | 7128 | `			ph7_result_bool(pCtx,1);` |
|    622 | 7129 | `			return PH7_OK;` |
|      - | 7130 | `		}` |
|  37592 | 7131 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7132 | `			/* UTF-8 stream  */` |
|    ! 0 | 7133 | `			break;` |
|      - | 7134 | `		}` |
|  37592 | 7135 | `		if( !SyisSpace(zIn[0]) ){` |
|  36948 | 7136 | `			break;` |
|      - | 7137 | `		}` |
|      - | 7138 | `		/* Point to the next character */` |
|    646 | 7139 | `		zIn++;` |
|      2 | 7140 | `	}` |
|      - | 7141 | `	/* The test failed,return FALSE */` |
|  36948 | 7142 | `	ph7_result_bool(pCtx,0);` |
|  36948 | 7143 | `	return PH7_OK;` |
|  18809 | 7144 |  |
|      - | 7145 | `/*` |
|      - | 7146 | ` * bool ctype_lower(string $text)` |
|      - | 7147 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 7148 | ` * Parameters` |
|      - | 7149 | ` *  $text` |
|      - | 7150 | ` *   The tested string.` |
|      - | 7151 | ` * Return` |
|      - | 7152 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 7153 | ` */` |
|     18 | 7154 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7155 |  |
|      - | 7156 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7157 | `	int nLen;` |
|     19 | 7158 | `	if( nArg < 1 ){` |
|      - | 7159 | `		/* Missing arguments,return FALSE */` |
|      3 | 7160 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7161 | `		return PH7_OK;` |
|      - | 7162 | `	}` |
|      - | 7163 | `	/* Extract the target string */` |
|     17 | 7164 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7165 | `	zEnd = &zIn[nLen];` |
|     17 | 7166 | `	if( nLen < 1 ){` |
|      - | 7167 | `		/* Empty string,return FALSE */` |
|      3 | 7168 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7169 | `		return PH7_OK;` |
|      - | 7170 | `	}` |
|      - | 7171 | `	/* Perform the requested operation */` |
|     27 | 7172 | `	for(;;){` |
|     55 | 7173 | `		if( zIn >= zEnd ){` |
|      - | 7174 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7175 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7176 | `			return PH7_OK;` |
|      - | 7177 | `		}` |
|     51 | 7178 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 7179 | `			break;` |
|      - | 7180 | `		}` |
|      - | 7181 | `		/* Point to the next character */` |
|     41 | 7182 | `		zIn++;` |
|      1 | 7183 | `	}` |
|      - | 7184 | `	/* The test failed,return FALSE */` |
|     11 | 7185 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7186 | `	return PH7_OK;` |
|     10 | 7187 |  |
|      - | 7188 | `/*` |
|      - | 7189 | ` * bool ctype_upper(string $text)` |
|      - | 7190 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 7191 | ` * Parameters` |
|      - | 7192 | ` *  $text` |
|      - | 7193 | ` *   The tested string.` |
|      - | 7194 | ` * Return` |
|      - | 7195 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 7196 | ` */` |
|     18 | 7197 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7198 |  |
|      - | 7199 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7200 | `	int nLen;` |
|     19 | 7201 | `	if( nArg < 1 ){` |
|      - | 7202 | `		/* Missing arguments,return FALSE */` |
|      3 | 7203 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7204 | `		return PH7_OK;` |
|      - | 7205 | `	}` |
|      - | 7206 | `	/* Extract the target string */` |
|     17 | 7207 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7208 | `	zEnd = &zIn[nLen];` |
|     17 | 7209 | `	if( nLen < 1 ){` |
|      - | 7210 | `		/* Empty string,return FALSE */` |
|      3 | 7211 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7212 | `		return PH7_OK;` |
|      - | 7213 | `	}` |
|      - | 7214 | `	/* Perform the requested operation */` |
|     28 | 7215 | `	for(;;){` |
|     57 | 7216 | `		if( zIn >= zEnd ){` |
|      - | 7217 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7218 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7219 | `			return PH7_OK;` |
|      - | 7220 | `		}` |
|     53 | 7221 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 7222 | `			break;` |
|      - | 7223 | `		}` |
|      - | 7224 | `		/* Point to the next character */` |
|     43 | 7225 | `		zIn++;` |
|      1 | 7226 | `	}` |
|      - | 7227 | `	/* The test failed,return FALSE */` |
|     11 | 7228 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7229 | `	return PH7_OK;` |
|     10 | 7230 |  |
|      - | 7231 | `/*` |
|      - | 7232 | ` * Date/Time functions` |
|      - | 7233 | ` * Status:` |
|      - | 7234 | ` *    Devel.` |
|      - | 7235 | ` */` |
|      - | 7236 | `#include <time.h>` |
|      - | 7237 | `#ifdef __WINNT__` |
|      - | 7238 | `/* GetSystemTime() */` |
|      - | 7239 | `#include <Windows.h>` |
|      - | 7240 | `#ifdef _WIN32_WCE` |
|      - | 7241 | `/*` |
|      - | 7242 | `** WindowsCE does not have a localtime() function.  So create a` |
|      - | 7243 | `** substitute.` |
|      - | 7244 | `** Taken from the SQLite3 source tree.` |
|      - | 7245 | `** Status: Public domain` |
|      - | 7246 | `*/` |
|      - | 7247 | `struct tm *__cdecl localtime(const time_t *t)` |
|      - | 7248 |  |
|      - | 7249 | `  static struct tm y;` |
|      - | 7250 | `  FILETIME uTm, lTm;` |
|      - | 7251 | `  SYSTEMTIME pTm;` |
|      - | 7252 | `  ph7_int64 t64;` |
|      - | 7253 | `  t64 = *t;` |
|      - | 7254 | `  t64 = (t64 + 11644473600)*10000000;` |
|      - | 7255 | `  uTm.dwLowDateTime = (DWORD)(t64 & 0xFFFFFFFF);` |
|      - | 7256 | `  uTm.dwHighDateTime= (DWORD)(t64 >> 32);` |
|      - | 7257 | `  FileTimeToLocalFileTime(&uTm,&lTm);` |
|      - | 7258 | `  FileTimeToSystemTime(&lTm,&pTm);` |
|      - | 7259 | `  y.tm_year = pTm.wYear - 1900;` |
|      - | 7260 | `  y.tm_mon = pTm.wMonth - 1;` |
|      - | 7261 | `  y.tm_wday = pTm.wDayOfWeek;` |
|      - | 7262 | `  y.tm_mday = pTm.wDay;` |
|      - | 7263 | `  y.tm_hour = pTm.wHour;` |
|      - | 7264 | `  y.tm_min = pTm.wMinute;` |
|      - | 7265 | `  y.tm_sec = pTm.wSecond;` |
|      - | 7266 | `  return &y;` |
|      - | 7267 |  |
|      - | 7268 | `#endif /*_WIN32_WCE */` |
|      - | 7269 | `#elif defined(__UNIXES__)` |
|      - | 7270 | `#include <sys/time.h>` |
|      - | 7271 | `#endif /* __WINNT__*/` |
|      - | 7272 | ` /*` |
|      - | 7273 | `  * int64 time(void)` |
|      - | 7274 | `  *  Current Unix timestamp` |
|      - | 7275 | `  * Parameters` |
|      - | 7276 | `  *  None.` |
|      - | 7277 | `  * Return` |
|      - | 7278 | `  *  Returns the current time measured in the number of seconds` |
|      - | 7279 | `  *  since the Unix Epoch (January 1 1970 00:00:00 GMT).` |
|      - | 7280 | `  */` |
|      8 | 7281 | `static int PH7_builtin_time(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7282 |  |
|      - | 7283 | `	time_t tt;` |
|      4 | 7284 | `	SXUNUSED(nArg); /* cc warning */` |
|      4 | 7285 | `	SXUNUSED(apArg);` |
|      - | 7286 | `	/* Extract the current time */` |
|      9 | 7287 | `	time(&tt);` |
|      - | 7288 | `	/* Return as 64-bit integer */` |
|      9 | 7289 | `	ph7_result_int64(pCtx,(ph7_int64)tt);` |
|      9 | 7290 | `	return  PH7_OK;` |
|      1 | 7291 |  |
|      - | 7292 | `/*` |
|      - | 7293 | `  * string/float microtime([ bool $get_as_float = false ])` |
|      - | 7294 | `  *  microtime() returns the current Unix timestamp with microseconds.` |
|      - | 7295 | `  * Parameters` |
|      - | 7296 | `  *  $get_as_float` |
|      - | 7297 | `  *   If used and set to TRUE, microtime() will return a float instead of a string` |
|      - | 7298 | `  *   as described in the return values section below.` |
|      - | 7299 | `  * Return` |
|      - | 7300 | `  *  By default, microtime() returns a string in the form "msec sec", where sec` |
|      - | 7301 | `  *  is the current time measured in the number of seconds since the Unix` |
|      - | 7302 | `  *  epoch (0:00:00 January 1, 1970 GMT), and msec is the number of microseconds` |
|      - | 7303 | `  *  that have elapsed since sec expressed in seconds.` |
|      - | 7304 | `  *  If get_as_float is set to TRUE, then microtime() returns a float, which represents` |
|      - | 7305 | `  *  the current time in seconds since the Unix epoch accurate to the nearest microsecond.` |
|      - | 7306 | `  */` |
|     20 | 7307 | `static int PH7_builtin_microtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7308 |  |
|     21 | 7309 | `	int bFloat = 0;` |
|      - | 7310 | `	sytime sTime;` |
|      - | 7311 | `#if defined(__UNIXES__)` |
|      - | 7312 | `	struct timeval tv;` |
|     20 | 7313 | `	gettimeofday(&tv,0);` |
|     20 | 7314 | `	sTime.tm_sec  = (long)tv.tv_sec;` |
|     20 | 7315 | `	sTime.tm_usec = (long)tv.tv_usec;` |
|      - | 7316 | `#else` |
|      - | 7317 | `	time_t tt;` |
|      1 | 7318 | `	time(&tt);` |
|      1 | 7319 | `	sTime.tm_sec  = (long)tt;` |
|      1 | 7320 | `	sTime.tm_usec = (long)(tt%SX_USEC_PER_SEC);` |
|      - | 7321 | `#endif /* __UNIXES__ */` |
|     21 | 7322 | `	if( nArg > 0 ){` |
|     17 | 7323 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|      8 | 7324 | `	}` |
|     21 | 7325 | `	if( bFloat ){` |
|      - | 7326 | `		/* Return as float */` |
|     17 | 7327 | `		ph7_result_double(pCtx,(double)sTime.tm_sec);` |
|      9 | 7328 | `	}else{` |
|      - | 7329 | `		/* Return as string */` |
|      5 | 7330 | `		ph7_result_string_format(pCtx,"%ld %ld",sTime.tm_usec,sTime.tm_sec);` |
|      - | 7331 | `	}` |
|     21 | 7332 | `	return PH7_OK;` |
|      1 | 7333 |  |
|      - | 7334 | `/*` |
|      - | 7335 | ` * array getdate ([ int $timestamp = time() ])` |
|      - | 7336 | ` *  Get date/time information.` |
|      - | 7337 | ` * Parameter` |
|      - | 7338 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|      - | 7339 | ` *     that defaults to the current local time if a timestamp is not given.` |
|      - | 7340 | ` *     In other words, it defaults to the value of time().` |
|      - | 7341 | ` * Returns` |
|      - | 7342 | ` *  Returns an associative array of information related to the timestamp.` |
|      - | 7343 | ` *  Elements from the returned associative array are as follows:` |
|      - | 7344 | ` *   KEY                                                         VALUE` |
|      - | 7345 | ` * ---------                                                    -------` |
|      - | 7346 | ` * "seconds" 	Numeric representation of seconds 	            0 to 59` |
|      - | 7347 | ` * "minutes" 	Numeric representation of minutes 	            0 to 59` |
|      - | 7348 | ` * "hours" 	    Numeric representation of hours 	            0 to 23` |
|      - | 7349 | ` * "mday" 	    Numeric representation of the day of the month 	1 to 31` |
|      - | 7350 | ` * "wday" 	    Numeric representation of the day of the week 	0 (for Sunday) through 6 (for Saturday)` |
|      - | 7351 | ` * "mon" 	    Numeric representation of a month 	            1 through 12` |
|      - | 7352 | ` * "year" 	    A full numeric representation of a year,        4 digits 	Examples: 1999 or 2003` |
|      - | 7353 | ` * "yday" 	    Numeric representation of the day of the year   0 through 365` |
|      - | 7354 | ` * "weekday" 	A full textual representation of the day of the week 	Sunday through Saturday` |
|      - | 7355 | ` * "month" 	    A full textual representation of a month, such as January or March 	January through December` |
|      - | 7356 | ` * 0 	        Seconds since the Unix Epoch, similar to the values returned by time() and used by date().` |
|      - | 7357 | ` * NOTE:` |
|      - | 7358 | ` *   NULL is returned on failure.` |
|      - | 7359 | ` */` |
|      8 | 7360 | `static int PH7_builtin_getdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7361 |  |
|      - | 7362 | `	ph7_value *pValue,*pArray;` |
|      - | 7363 | `	Sytm sTm;` |
|      9 | 7364 | `	if( nArg < 1 ){` |
|      - | 7365 | `#ifdef __WINNT__` |
|      - | 7366 | `		SYSTEMTIME sOS;` |
|      1 | 7367 | `		GetSystemTime(&sOS);` |
|      1 | 7368 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 7369 | `#else` |
|      - | 7370 | `		struct tm *pTm;` |
|      - | 7371 | `		time_t t;` |
|      4 | 7372 | `		time(&t);` |
|      4 | 7373 | `		pTm = localtime(&t);` |
|      4 | 7374 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7375 | `#endif` |
|      3 | 7376 | `	}else{` |
|      - | 7377 | `		/* Use the given timestamp */` |
|      - | 7378 | `		time_t t;` |
|      - | 7379 | `		struct tm *pTm;` |
|      - | 7380 | `#ifdef __WINNT__` |
|      - | 7381 | `#ifdef _MSC_VER` |
|      - | 7382 | `#if _MSC_VER >= 1400 /* Visual Studio 2005 and up */` |
|      - | 7383 | `#pragma warning(disable:4996) /* _CRT_SECURE...*/` |
|      - | 7384 | `#endif` |
|      - | 7385 | `#endif` |
|      - | 7386 | `#endif` |
|      5 | 7387 | `		if( ph7_value_is_int(apArg[0]) ){` |
|      5 | 7388 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|      5 | 7389 | `			pTm = localtime(&t);` |
|      5 | 7390 | `			if( pTm == 0 ){` |
|    ! 0 | 7391 | `				time(&t);` |
|    ! 0 | 7392 | `			}` |
|      3 | 7393 | `		}else{` |
|    ! 0 | 7394 | `			time(&t);` |
|      - | 7395 | `		}` |
|      5 | 7396 | `		pTm = localtime(&t);` |
|      5 | 7397 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7398 | `	}` |
|      - | 7399 | `	/* Element value */` |
|      9 | 7400 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      9 | 7401 | `	if( pValue == 0 ){` |
|      - | 7402 | `		/* Return NULL */` |
|    ! 0 | 7403 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7404 | `		return PH7_OK;` |
|      - | 7405 | `	}` |
|      - | 7406 | `	/* Create a new array */` |
|      9 | 7407 | `	pArray = ph7_context_new_array(pCtx);` |
|      9 | 7408 | `	if( pArray == 0 ){` |
|      - | 7409 | `		/* Return NULL */` |
|    ! 0 | 7410 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7411 | `		return PH7_OK;` |
|      - | 7412 | `	}` |
|      - | 7413 | `	/* Fill the array */` |
|      - | 7414 | `	/* Seconds */` |
|      9 | 7415 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|      9 | 7416 | `	ph7_array_add_strkey_elem(pArray,"seconds",pValue);` |
|      - | 7417 | `	/* Minutes */` |
|      9 | 7418 | `	ph7_value_int(pValue,sTm.tm_min);` |
|      9 | 7419 | `	ph7_array_add_strkey_elem(pArray,"minutes",pValue);` |
|      - | 7420 | `	/* Hours */` |
|      9 | 7421 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|      9 | 7422 | `	ph7_array_add_strkey_elem(pArray,"hours",pValue);` |
|      - | 7423 | `	/* mday */` |
|      9 | 7424 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|      9 | 7425 | `	ph7_array_add_strkey_elem(pArray,"mday",pValue);` |
|      - | 7426 | `	/* wday */` |
|      9 | 7427 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|      9 | 7428 | `	ph7_array_add_strkey_elem(pArray,"wday",pValue);` |
|      - | 7429 | `	/* mon */` |
|      9 | 7430 | `	ph7_value_int(pValue,sTm.tm_mon+1);` |
|      9 | 7431 | `	ph7_array_add_strkey_elem(pArray,"mon",pValue);` |
|      - | 7432 | `	/* year */` |
|      9 | 7433 | `	ph7_value_int(pValue,sTm.tm_year);` |
|      9 | 7434 | `	ph7_array_add_strkey_elem(pArray,"year",pValue);` |
|      - | 7435 | `	/* yday */` |
|      9 | 7436 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|      9 | 7437 | `	ph7_array_add_strkey_elem(pArray,"yday",pValue);` |
|      - | 7438 | `	/* Weekday */` |
|      9 | 7439 | `	ph7_value_string(pValue,SyTimeGetDay(sTm.tm_wday),-1);` |
|      9 | 7440 | `	ph7_array_add_strkey_elem(pArray,"weekday",pValue);` |
|      - | 7441 | `	/* Month */` |
|      9 | 7442 | `	ph7_value_reset_string_cursor(pValue);` |
|      9 | 7443 | `	ph7_value_string(pValue,SyTimeGetMonth(sTm.tm_mon),-1);` |
|      9 | 7444 | `	ph7_array_add_strkey_elem(pArray,"month",pValue);` |
|      - | 7445 | `	/* Seconds since the epoch */` |
|      9 | 7446 | `	ph7_value_int64(pValue,(ph7_int64)time(0));` |
|      9 | 7447 | `	ph7_array_add_intkey_elem(pArray,0 /* Index zero */,pValue);` |
|      - | 7448 | `	/* Return the freshly created array */` |
|      9 | 7449 | `	ph7_result_value(pCtx,pArray);` |
|      9 | 7450 | `	return PH7_OK;` |
|      5 | 7451 |  |
|      - | 7452 | `/*` |
|      - | 7453 | ` * mixed gettimeofday([ bool $return_float = false ] )` |
|      - | 7454 | ` *   Returns an associative array containing the data returned from the system call.` |
|      - | 7455 | ` * Parameters` |
|      - | 7456 | ` *  $return_float` |
|      - | 7457 | ` *   When set to TRUE, a float instead of an array is returned.` |
|      - | 7458 | ` * Return` |
|      - | 7459 | ` *   By default an array is returned. If return_float is set, then` |
|      - | 7460 | ` *   a float is returned.` |
|      - | 7461 | ` */` |
|      4 | 7462 | `static int PH7_builtin_gettimeofday(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7463 |  |
|      5 | 7464 | `	int bFloat = 0;` |
|      - | 7465 | `	sytime sTime;` |
|      - | 7466 | `#if defined(__UNIXES__)` |
|      - | 7467 | `	struct timeval tv;` |
|      4 | 7468 | `	gettimeofday(&tv,0);` |
|      4 | 7469 | `	sTime.tm_sec  = (long)tv.tv_sec;` |
|      4 | 7470 | `	sTime.tm_usec = (long)tv.tv_usec;` |
|      - | 7471 | `#else` |
|      - | 7472 | `	time_t tt;` |
|      1 | 7473 | `	time(&tt);` |
|      1 | 7474 | `	sTime.tm_sec  = (long)tt;` |
|      1 | 7475 | `	sTime.tm_usec = (long)(tt%SX_USEC_PER_SEC);` |
|      - | 7476 | `#endif /* __UNIXES__ */` |
|      5 | 7477 | `	if( nArg > 0 ){` |
|      5 | 7478 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|      2 | 7479 | `	}` |
|      5 | 7480 | `	if( bFloat ){` |
|      - | 7481 | `		/* Return as float */` |
|      3 | 7482 | `		ph7_result_double(pCtx,(double)sTime.tm_sec);` |
|      2 | 7483 | `	}else{` |
|      - | 7484 | `		/* Return an associative array */` |
|      - | 7485 | `		ph7_value *pValue,*pArray;` |
|      - | 7486 | `		/* Create a new array */` |
|      3 | 7487 | `		pArray = ph7_context_new_array(pCtx);` |
|      - | 7488 | `		/* Element value */` |
|      3 | 7489 | `		pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 7490 | `		if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 7491 | `			/* Return NULL */` |
|    ! 0 | 7492 | `			ph7_result_null(pCtx);` |
|    ! 0 | 7493 | `			return PH7_OK;` |
|      - | 7494 | `		}` |
|      - | 7495 | `		/* Fill the array */` |
|      - | 7496 | `		/* sec */` |
|      3 | 7497 | `		ph7_value_int64(pValue,sTime.tm_sec);` |
|      3 | 7498 | `		ph7_array_add_strkey_elem(pArray,"sec",pValue);` |
|      - | 7499 | `		/* usec */` |
|      3 | 7500 | `		ph7_value_int64(pValue,sTime.tm_usec);` |
|      3 | 7501 | `		ph7_array_add_strkey_elem(pArray,"usec",pValue);` |
|      - | 7502 | `		/* Return the array */` |
|      3 | 7503 | `		ph7_result_value(pCtx,pArray);` |
|      - | 7504 | `	}` |
|      5 | 7505 | `	return PH7_OK;` |
|      3 | 7506 |  |
|      - | 7507 | `/* Check if the given year is leap or not */` |
|      - | 7508 | `#define IS_LEAP_YEAR(YEAR)	(YEAR % 400 ? ( YEAR % 100 ? ( YEAR % 4 ? 0 : 1 ) : 0 ) : 1)` |
|      - | 7509 | `/* ISO-8601 numeric representation of the day of the week */` |
|      - | 7510 | `static const int aISO8601[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|      - | 7511 | `/*` |
|      - | 7512 | ` * Format a given date string.` |
|      - | 7513 | ` * Supported format: (Taken from PHP online docs)` |
|      - | 7514 | ` * character 	Description` |
|      - | 7515 | ` * d          Day of the month` |
|      - | 7516 | ` * D          A textual representation of a days` |
|      - | 7517 | ` * j          Day of the month without leading zeros` |
|      - | 7518 | ` * l          A full textual representation of the day of the week` |
|      - | 7519 | ` * N          ISO-8601 numeric representation of the day of the week` |
|      - | 7520 | ` * w          Numeric representation of the day of the week` |
|      - | 7521 | ` * z          The day of the year (starting from 0)` |
|      - | 7522 | ` * F          A full textual representation of a month, such as January or March` |
|      - | 7523 | ` * m          Numeric representation of a month, with leading zeros 	01 through 12` |
|      - | 7524 | ` * M          A short textual representation of a month, three letters 	Jan through Dec` |
|      - | 7525 | ` * n          Numeric representation of a month, without leading zeros 	1 through 12` |
|      - | 7526 | ` * t          Number of days in the given month 	28 through 31` |
|      - | 7527 | ` * L          Whether it's a leap year 	1 if it is a leap year, 0 otherwise.` |
|      - | 7528 | ` * o          ISO-8601 year number. This has the same value as Y, except that if the ISO week number` |
|      - | 7529 | ` *            (W) belongs to the previous or next year, that year is used instead. (added in PHP 5.1.0) Examples: 1999 or 2003` |
|      - | 7530 | ` * Y          A full numeric representation of a year, 4 digits 	Examples: 1999 or 2003` |
|      - | 7531 | ` * y          A two digit representation of a year 	Examples: 99 or 03` |
|      - | 7532 | ` * a          Lowercase Ante meridiem and Post meridiem 	am or pm` |
|      - | 7533 | ` * A          Uppercase Ante meridiem and Post meridiem 	AM or PM` |
|      - | 7534 | ` * g          12-hour format of an hour without leading zeros 	1 through 12` |
|      - | 7535 | ` * G          24-hour format of an hour without leading zeros 	0 through 23` |
|      - | 7536 | ` * h          12-hour format of an hour with leading zeros 	01 through 12` |
|      - | 7537 | ` * H          24-hour format of an hour with leading zeros 	00 through 23` |
|      - | 7538 | ` * i          Minutes with leading zeros 	00 to 59` |
|      - | 7539 | ` * s          Seconds, with leading zeros 	00 through 59` |
|      - | 7540 | ` * u          Microseconds Example: 654321` |
|      - | 7541 | ` * e          Timezone identifier 	Examples: UTC, GMT, Atlantic/Azores` |
|      - | 7542 | ` * I          (capital i) Whether or not the date is in daylight saving time 	1 if Daylight Saving Time, 0 otherwise.` |
|      - | 7543 | ` * r          RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 +0200` |
|      - | 7544 | ` * U          Seconds since the Unix Epoch (January 1 1970 00:00:00 GMT)` |
|      - | 7545 | ` * S          English ordinal suffix for the day of the month, 2 characters` |
|      - | 7546 | ` * O          Difference to Greenwich time (GMT) in hours` |
|      - | 7547 | ` * Z          Timezone offset in seconds. The offset for timezones west of UTC is always negative, and for those` |
|      - | 7548 | ` *            east of UTC is always positive.` |
|      - | 7549 | ` * c         ISO 8601 date` |
|      - | 7550 | ` */` |
|     46 | 7551 | `static sxi32 DateFormat(ph7_context *pCtx,const char *zIn,int nLen,Sytm *pTm)` |
|      1 | 7552 |  |
|     47 | 7553 | `	const char *zEnd = &zIn[nLen];` |
|      - | 7554 | `	const char *zCur;` |
|      - | 7555 | `	/* Start the format process */` |
|     78 | 7556 | `	for(;;){` |
|    157 | 7557 | `		if( zIn >= zEnd ){` |
|      - | 7558 | `			/* No more input to process */` |
|     47 | 7559 | `			break;` |
|      - | 7560 | `		}` |
|    111 | 7561 | `		switch(zIn[0]){` |
|      7 | 7562 | `		case 'd':` |
|      - | 7563 | `			/* Day of the month, 2 digits with leading zeros */` |
|     15 | 7564 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mday);` |
|     15 | 7565 | `			break;` |
|    ! 0 | 7566 | `		case 'D':` |
|      - | 7567 | `			/*A textual representation of a day, three letters*/` |
|    ! 0 | 7568 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|    ! 0 | 7569 | `			ph7_result_string(pCtx,zCur,3);` |
|    ! 0 | 7570 | `			break;` |
|    ! 0 | 7571 | `		case 'j':` |
|      - | 7572 | `			/*	Day of the month without leading zeros */` |
|    ! 0 | 7573 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mday);` |
|    ! 0 | 7574 | `			break;` |
|      2 | 7575 | `		case 'l':` |
|      - | 7576 | `			/* A full textual representation of the day of the week */` |
|      5 | 7577 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|      5 | 7578 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|      5 | 7579 | `			break;` |
|    ! 0 | 7580 | `		case 'N':{` |
|      - | 7581 | `			/* ISO-8601 numeric representation of the day of the week */` |
|    ! 0 | 7582 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|    ! 0 | 7583 | `			break;` |
|      - | 7584 | `				 }` |
|    ! 0 | 7585 | `		case 'w':` |
|      - | 7586 | `			/*Numeric representation of the day of the week*/` |
|    ! 0 | 7587 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|    ! 0 | 7588 | `			break;` |
|    ! 0 | 7589 | `		case 'z':` |
|      - | 7590 | `			/*The day of the year*/` |
|    ! 0 | 7591 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_yday);` |
|    ! 0 | 7592 | `			break;` |
|      2 | 7593 | `		case 'F':` |
|      - | 7594 | `			/*A full textual representation of a month, such as January or March*/` |
|      5 | 7595 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|      5 | 7596 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|      5 | 7597 | `			break;` |
|      7 | 7598 | `		case 'm':` |
|      - | 7599 | `			/*Numeric representation of a month, with leading zeros*/` |
|     15 | 7600 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|     15 | 7601 | `			break;` |
|    ! 0 | 7602 | `		case 'M':` |
|      - | 7603 | `			/*A short textual representation of a month, three letters*/` |
|    ! 0 | 7604 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|    ! 0 | 7605 | `			ph7_result_string(pCtx,zCur,3);` |
|    ! 0 | 7606 | `			break;` |
|    ! 0 | 7607 | `		case 'n':` |
|      - | 7608 | `			/*Numeric representation of a month, without leading zeros*/` |
|    ! 0 | 7609 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mon + 1);` |
|    ! 0 | 7610 | `			break;` |
|    ! 0 | 7611 | `		case 't':{` |
|      - | 7612 | `			static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|    ! 0 | 7613 | `			int nDays = aMonDays[pTm->tm_mon % 12 ];` |
|    ! 0 | 7614 | `			if( pTm->tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(pTm->tm_year) ){` |
|    ! 0 | 7615 | `				nDays = 28;` |
|    ! 0 | 7616 | `			}` |
|      - | 7617 | `			/*Number of days in the given month*/` |
|    ! 0 | 7618 | `			ph7_result_string_format(pCtx,"%d",nDays);` |
|    ! 0 | 7619 | `			break;` |
|      - | 7620 | `				 }` |
|    ! 0 | 7621 | `		case 'L':{` |
|    ! 0 | 7622 | `			int isLeap = IS_LEAP_YEAR(pTm->tm_year);` |
|      - | 7623 | `			/* Whether it's a leap year */` |
|    ! 0 | 7624 | `			ph7_result_string_format(pCtx,"%d",isLeap);` |
|    ! 0 | 7625 | `			break;` |
|      - | 7626 | `				 }` |
|    ! 0 | 7627 | `		case 'o':` |
|      - | 7628 | `			/* ISO-8601 year number.*/` |
|    ! 0 | 7629 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|    ! 0 | 7630 | `			break;` |
|      9 | 7631 | `		case 'Y':` |
|      - | 7632 | `			/*	A full numeric representation of a year, 4 digits */` |
|     19 | 7633 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|     19 | 7634 | `			break;` |
|    ! 0 | 7635 | `		case 'y':` |
|      - | 7636 | `			/*A two digit representation of a year*/` |
|    ! 0 | 7637 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_year%100);` |
|    ! 0 | 7638 | `			break;` |
|    ! 0 | 7639 | `		case 'a':` |
|      - | 7640 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7641 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",2);` |
|    ! 0 | 7642 | `			break;` |
|    ! 0 | 7643 | `		case 'A':` |
|      - | 7644 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7645 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",2);` |
|    ! 0 | 7646 | `			break;` |
|    ! 0 | 7647 | `		case 'g':` |
|      - | 7648 | `			/*	12-hour format of an hour without leading zeros*/` |
|    ! 0 | 7649 | `			ph7_result_string_format(pCtx,"%d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7650 | `			break;` |
|    ! 0 | 7651 | `		case 'G':` |
|      - | 7652 | `			/* 24-hour format of an hour without leading zeros */` |
|    ! 0 | 7653 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_hour);` |
|    ! 0 | 7654 | `			break;` |
|    ! 0 | 7655 | `		case 'h':` |
|      - | 7656 | `			/* 12-hour format of an hour with leading zeros */` |
|    ! 0 | 7657 | `			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7658 | `			break;` |
|      3 | 7659 | `		case 'H':` |
|      - | 7660 | `			/*	24-hour format of an hour with leading zeros */` |
|      7 | 7661 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|      7 | 7662 | `			break;` |
|      3 | 7663 | `		case 'i':` |
|      - | 7664 | `			/* 	Minutes with leading zeros */` |
|      7 | 7665 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|      7 | 7666 | `			break;` |
|      3 | 7667 | `		case 's':` |
|      - | 7668 | `			/* 	second with leading zeros */` |
|      7 | 7669 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|      7 | 7670 | `			break;` |
|    ! 0 | 7671 | `		case 'u':` |
|      - | 7672 | `			/* 	Microseconds */` |
|    ! 0 | 7673 | `			ph7_result_string_format(pCtx,"%u",pTm->tm_sec * SX_USEC_PER_SEC);` |
|    ! 0 | 7674 | `			break;` |
|    ! 0 | 7675 | `		case 'S':{` |
|      - | 7676 | `			/* English ordinal suffix for the day of the month, 2 characters */` |
|      - | 7677 | `			static const char zSuffix[] = "thstndrdthththththth";` |
|    ! 0 | 7678 | `			int v = pTm->tm_mday;` |
|    ! 0 | 7679 | `			ph7_result_string(pCtx,&zSuffix[2 * (int)(v / 10 % 10 != 1 ? v % 10 : 0)],(int)sizeof(char) * 2);` |
|    ! 0 | 7680 | `			break;` |
|      - | 7681 | `				 }` |
|    ! 0 | 7682 | `		case 'e':` |
|      - | 7683 | `			/* 	Timezone identifier */` |
|    ! 0 | 7684 | `			zCur = pTm->tm_zone;` |
|    ! 0 | 7685 | `			if( zCur == 0 ){` |
|      - | 7686 | `				/* Assume GMT */` |
|    ! 0 | 7687 | `				zCur = "GMT";` |
|    ! 0 | 7688 | `			}` |
|    ! 0 | 7689 | `			ph7_result_string(pCtx,zCur,-1);` |
|    ! 0 | 7690 | `			break;` |
|    ! 0 | 7691 | `		case 'I':` |
|      - | 7692 | `			/* Whether or not the date is in daylight saving time */` |
|      - | 7693 | `#ifdef __WINNT__` |
|      - | 7694 | `#ifdef _MSC_VER` |
|      - | 7695 | `#ifndef _WIN32_WCE` |
|    ! 0 | 7696 | `			_get_daylight(&pTm->tm_isdst);` |
|      - | 7697 | `#endif` |
|      - | 7698 | `#endif` |
|      - | 7699 | `#endif` |
|    ! 0 | 7700 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_isdst == 1);` |
|    ! 0 | 7701 | `			break;` |
|    ! 0 | 7702 | `		case 'r':` |
|      - | 7703 | `			/* RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 */` |
|    ! 0 | 7704 | `			ph7_result_string_format(pCtx,"%.3s, %02d %.3s %4d %02d:%02d:%02d",` |
|    ! 0 | 7705 | `				SyTimeGetDay(pTm->tm_wday),` |
|    ! 0 | 7706 | `				pTm->tm_mday,` |
|    ! 0 | 7707 | `				SyTimeGetMonth(pTm->tm_mon),` |
|    ! 0 | 7708 | `				pTm->tm_year,` |
|    ! 0 | 7709 | `				pTm->tm_hour,` |
|    ! 0 | 7710 | `				pTm->tm_min,` |
|    ! 0 | 7711 | `				pTm->tm_sec` |
|      - | 7712 | `				);` |
|    ! 0 | 7713 | `			break;` |
|    ! 0 | 7714 | `		case 'U':{` |
|      - | 7715 | `			time_t tt;` |
|      - | 7716 | `			/* Seconds since the Unix Epoch */` |
|    ! 0 | 7717 | `			time(&tt);` |
|    ! 0 | 7718 | `			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);` |
|    ! 0 | 7719 | `			break;` |
|      - | 7720 | `				 }` |
|    ! 0 | 7721 | `		case 'O':` |
|      - | 7722 | `		case 'P':` |
|      - | 7723 | `			/* Difference to Greenwich time (GMT) in hours */` |
|    ! 0 | 7724 | `			ph7_result_string_format(pCtx,"%+05d",pTm->tm_gmtoff);` |
|    ! 0 | 7725 | `			break;` |
|    ! 0 | 7726 | `		case 'Z':` |
|      - | 7727 | `			/* Timezone offset in seconds. The offset for timezones west of UTC` |
|      - | 7728 | `			 * is always negative, and for those east of UTC is always positive.` |
|      - | 7729 | `			 */` |
|    ! 0 | 7730 | `			ph7_result_string_format(pCtx,"%+05d",pTm->tm_gmtoff);` |
|    ! 0 | 7731 | `			break;` |
|      1 | 7732 | `		case 'c':` |
|      - | 7733 | `			/* 	ISO 8601 date */` |
|      4 | 7734 | `			ph7_result_string_format(pCtx,"%4d-%02d-%02dT%02d:%02d:%02d%+05d",` |
|      1 | 7735 | `				pTm->tm_year,` |
|      2 | 7736 | `				pTm->tm_mon+1,` |
|      1 | 7737 | `				pTm->tm_mday,` |
|      1 | 7738 | `				pTm->tm_hour,` |
|      1 | 7739 | `				pTm->tm_min,` |
|      1 | 7740 | `				pTm->tm_sec,` |
|      1 | 7741 | `				pTm->tm_gmtoff` |
|      - | 7742 | `				);` |
|      3 | 7743 | `			break;` |
|      1 | 7744 | `		case '\\':` |
|      3 | 7745 | `			zIn++;` |
|      - | 7746 | `			/* Expand verbatim */` |
|      3 | 7747 | `			if( zIn < zEnd ){` |
|      3 | 7748 | `				ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|      1 | 7749 | `			}` |
|      3 | 7750 | `			break;` |
|     17 | 7751 | `		default:` |
|      - | 7752 | `			/* Unknown format specifer,expand verbatim */` |
|     35 | 7753 | `			ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|     34 | 7754 | `			break;` |
|      - | 7755 | `		}` |
|      - | 7756 | `		/* Point to the next character */` |
|    111 | 7757 | `		zIn++;` |
|      1 | 7758 | `	}` |
|     47 | 7759 | `	return SXRET_OK;` |
|      1 | 7760 |  |
|      - | 7761 | `/*` |
|      - | 7762 | ` * PH7 implementation of the strftime() function.` |
|      - | 7763 | ` * The following formats are supported:` |
|      - | 7764 | ` * %a 	An abbreviated textual representation of the day` |
|      - | 7765 | ` * %A 	A full textual representation of the day` |
|      - | 7766 | ` * %d 	Two-digit day of the month (with leading zeros)` |
|      - | 7767 | ` * %e 	Day of the month, with a space preceding single digits.` |
|      - | 7768 | ` * %j 	Day of the year, 3 digits with leading zeros` |
|      - | 7769 | ` * %u 	ISO-8601 numeric representation of the day of the week 	1 (for Monday) though 7 (for Sunday)` |
|      - | 7770 | ` * %w 	Numeric representation of the day of the week 0 (for Sunday) through 6 (for Saturday)` |
|      - | 7771 | ` * %U 	Week number of the given year, starting with the first Sunday as the first week` |
|      - | 7772 | ` * %V 	ISO-8601:1988 week number of the given year, starting with the first week of the year with at least` |
|      - | 7773 | ` *   4 weekdays, with Monday being the start of the week.` |
|      - | 7774 | ` * %W 	A numeric representation of the week of the year` |
|      - | 7775 | ` * %b 	Abbreviated month name, based on the locale` |
|      - | 7776 | ` * %B 	Full month name, based on the locale` |
|      - | 7777 | ` * %h 	Abbreviated month name, based on the locale (an alias of %b)` |
|      - | 7778 | ` * %m 	Two digit representation of the month` |
|      - | 7779 | ` * %C 	Two digit representation of the century (year divided by 100, truncated to an integer)` |
|      - | 7780 | ` * %g 	Two digit representation of the year going by ISO-8601:1988 standards (see %V)` |
|      - | 7781 | ` * %G 	The full four-digit version of %g` |
|      - | 7782 | ` * %y 	Two digit representation of the year` |
|      - | 7783 | ` * %Y 	Four digit representation for the year` |
|      - | 7784 | ` * %H 	Two digit representation of the hour in 24-hour format` |
|      - | 7785 | ` * %I 	Two digit representation of the hour in 12-hour format` |
|      - | 7786 | ` * %l (lower-case 'L') 	Hour in 12-hour format, with a space preceeding single digits` |
|      - | 7787 | ` * %M 	Two digit representation of the minute` |
|      - | 7788 | ` * %p 	UPPER-CASE 'AM' or 'PM' based on the given time` |
|      - | 7789 | ` * %P 	lower-case 'am' or 'pm' based on the given time` |
|      - | 7790 | ` * %r 	Same as "%I:%M:%S %p"` |
|      - | 7791 | ` * %R 	Same as "%H:%M"` |
|      - | 7792 | ` * %S 	Two digit representation of the second` |
|      - | 7793 | ` * %T 	Same as "%H:%M:%S"` |
|      - | 7794 | ` * %X 	Preferred time representation based on locale, without the date` |
|      - | 7795 | ` * %z 	Either the time zone offset from UTC or the abbreviation` |
|      - | 7796 | ` * %Z 	The time zone offset/abbreviation option NOT given by %z` |
|      - | 7797 | ` * %c 	Preferred date and time stamp based on local` |
|      - | 7798 | ` * %D 	Same as "%m/%d/%y"` |
|      - | 7799 | ` * %F 	Same as "%Y-%m-%d"` |
|      - | 7800 | ` * %s 	Unix Epoch Time timestamp (same as the time() function)` |
|      - | 7801 | ` * %x 	Preferred date representation based on locale, without the time` |
|      - | 7802 | ` * %n 	A newline character ("\n")` |
|      - | 7803 | ` * %t 	A Tab character ("\t")` |
|      - | 7804 | ` * %% 	A literal percentage character ("%")` |
|      - | 7805 | ` */` |
|     16 | 7806 | `static int PH7_Strftime(` |
|      - | 7807 | `	ph7_context *pCtx,  /* Call context */` |
|      - | 7808 | `	const char *zIn,    /* Input string */` |
|      - | 7809 | `	int nLen,           /* Input length */` |
|      - | 7810 | `	Sytm *pTm           /* Parse of the given time */` |
|      - | 7811 | `	)` |
|      1 | 7812 |  |
|     17 | 7813 | `	const char *zCur,*zEnd = &zIn[nLen];` |
|      - | 7814 | `	int c;` |
|      - | 7815 | `	/* Start the format process */` |
|     18 | 7816 | `	for(;;){` |
|     37 | 7817 | `		zCur = zIn;` |
|     41 | 7818 | `		while(zIn < zEnd && zIn[0] != '%' ){` |
|      5 | 7819 | `			zIn++;` |
|      1 | 7820 | `		}` |
|     37 | 7821 | `		if( zIn > zCur ){` |
|      - | 7822 | `			/* Consume input verbatim */` |
|      5 | 7823 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 7824 | `		}` |
|     37 | 7825 | `		zIn++; /* Jump the percent sign */` |
|     37 | 7826 | `		if( zIn >= zEnd ){` |
|      - | 7827 | `			/* No more input to process */` |
|     17 | 7828 | `			break;` |
|      - | 7829 | `		}` |
|     21 | 7830 | `		c = zIn[0];` |
|      - | 7831 | `		/* Act according to the current specifer */` |
|     21 | 7832 | `		switch(c){` |
|    ! 0 | 7833 | `		case '%':` |
|      - | 7834 | `			/* A literal percentage character ("%") */` |
|    ! 0 | 7835 | `			ph7_result_string(pCtx,"%",(int)sizeof(char));` |
|    ! 0 | 7836 | `			break;` |
|    ! 0 | 7837 | `		case 't':` |
|      - | 7838 | `			/* A Tab character */` |
|    ! 0 | 7839 | `			ph7_result_string(pCtx,"\t",(int)sizeof(char));` |
|    ! 0 | 7840 | `			break;` |
|    ! 0 | 7841 | `		case 'n':` |
|      - | 7842 | `			/* A newline character */` |
|    ! 0 | 7843 | `			ph7_result_string(pCtx,"\n",(int)sizeof(char));` |
|    ! 0 | 7844 | `			break;` |
|      1 | 7845 | `		case 'a':` |
|      - | 7846 | `			/* An abbreviated textual representation of the day */` |
|      3 | 7847 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),(int)sizeof(char)*3);` |
|      3 | 7848 | `			break;` |
|    ! 0 | 7849 | `		case 'A':` |
|      - | 7850 | `			/* A full textual representation of the day */` |
|    ! 0 | 7851 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),-1/*Compute length automatically*/);` |
|    ! 0 | 7852 | `			break;` |
|    ! 0 | 7853 | `		case 'e':` |
|      - | 7854 | `			/* Day of the month, 2 digits with leading space for single digit*/` |
|    ! 0 | 7855 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_mday);` |
|    ! 0 | 7856 | `			break;` |
|      2 | 7857 | `		case 'd':` |
|      - | 7858 | `			/* Two-digit day of the month (with leading zeros) */` |
|      5 | 7859 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon+1);` |
|      5 | 7860 | `			break;` |
|    ! 0 | 7861 | `		case 'j':` |
|      - | 7862 | `			/*The day of the year,3 digits with leading zeros*/` |
|    ! 0 | 7863 | `			ph7_result_string_format(pCtx,"%03d",pTm->tm_yday);` |
|    ! 0 | 7864 | `			break;` |
|    ! 0 | 7865 | `		case 'u':` |
|      - | 7866 | `			/* ISO-8601 numeric representation of the day of the week */` |
|    ! 0 | 7867 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|    ! 0 | 7868 | `			break;` |
|    ! 0 | 7869 | `		case 'w':` |
|      - | 7870 | `			/* Numeric representation of the day of the week */` |
|    ! 0 | 7871 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|    ! 0 | 7872 | `			break;` |
|    ! 0 | 7873 | `		case 'b':` |
|      - | 7874 | `		case 'h':` |
|      - | 7875 | `			/*A short textual representation of a month, three letters (Not based on locale)*/` |
|    ! 0 | 7876 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),(int)sizeof(char)*3);` |
|    ! 0 | 7877 | `			break;` |
|    ! 0 | 7878 | `		case 'B':` |
|      - | 7879 | `			/* Full month name (Not based on locale) */` |
|    ! 0 | 7880 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),-1/*Compute length automatically*/);` |
|    ! 0 | 7881 | `			break;` |
|      2 | 7882 | `		case 'm':` |
|      - | 7883 | `			/*Numeric representation of a month, with leading zeros*/` |
|      5 | 7884 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|      5 | 7885 | `			break;` |
|    ! 0 | 7886 | `		case 'C':` |
|      - | 7887 | `			/* Two digit representation of the century */` |
|    ! 0 | 7888 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year/100);` |
|    ! 0 | 7889 | `			break;` |
|    ! 0 | 7890 | `		case 'y':` |
|      - | 7891 | `		case 'g':` |
|      - | 7892 | `			/* Two digit representation of the year */` |
|    ! 0 | 7893 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year%100);` |
|    ! 0 | 7894 | `			break;` |
|      2 | 7895 | `		case 'Y':` |
|      - | 7896 | `		case 'G':` |
|      - | 7897 | `			/* Four digit representation of the year */` |
|      5 | 7898 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|      5 | 7899 | `			break;` |
|    ! 0 | 7900 | `		case 'I':` |
|      - | 7901 | `			/* 12-hour format of an hour with leading zeros */` |
|    ! 0 | 7902 | `			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7903 | `			break;` |
|    ! 0 | 7904 | `		case 'l':` |
|      - | 7905 | `			/* 12-hour format of an hour with leading space */` |
|    ! 0 | 7906 | `			ph7_result_string_format(pCtx,"%2d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7907 | `			break;` |
|      1 | 7908 | `		case 'H':` |
|      - | 7909 | `			/* 24-hour format of an hour with leading zeros */` |
|      3 | 7910 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|      3 | 7911 | `			break;` |
|      1 | 7912 | `		case 'M':` |
|      - | 7913 | `			/* Minutes with leading zeros */` |
|      3 | 7914 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|      3 | 7915 | `			break;` |
|    ! 0 | 7916 | `		case 'S':` |
|      - | 7917 | `			/* Seconds with leading zeros */` |
|    ! 0 | 7918 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|    ! 0 | 7919 | `			break;` |
|    ! 0 | 7920 | `		case 'z':` |
|      - | 7921 | `		case 'Z':` |
|      - | 7922 | `			/* 	Timezone identifier */` |
|    ! 0 | 7923 | `			zCur = pTm->tm_zone;` |
|    ! 0 | 7924 | `			if( zCur == 0 ){` |
|      - | 7925 | `				/* Assume GMT */` |
|    ! 0 | 7926 | `				zCur = "GMT";` |
|    ! 0 | 7927 | `			}` |
|    ! 0 | 7928 | `			ph7_result_string(pCtx,zCur,-1);` |
|    ! 0 | 7929 | `			break;` |
|    ! 0 | 7930 | `		case 'T':` |
|      - | 7931 | `		case 'X':` |
|      - | 7932 | `			/* Same as "%H:%M:%S" */` |
|    ! 0 | 7933 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d",pTm->tm_hour,pTm->tm_min,pTm->tm_sec);` |
|    ! 0 | 7934 | `			break;` |
|    ! 0 | 7935 | `		case 'R':` |
|      - | 7936 | `			/* Same as "%H:%M" */` |
|    ! 0 | 7937 | `			ph7_result_string_format(pCtx,"%02d:%02d",pTm->tm_hour,pTm->tm_min);` |
|    ! 0 | 7938 | `			break;` |
|    ! 0 | 7939 | `		case 'P':` |
|      - | 7940 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7941 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",(int)sizeof(char)*2);` |
|    ! 0 | 7942 | `			break;` |
|    ! 0 | 7943 | `		case 'p':` |
|      - | 7944 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7945 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",(int)sizeof(char)*2);` |
|    ! 0 | 7946 | `			break;` |
|    ! 0 | 7947 | `		case 'r':` |
|      - | 7948 | `			/* Same as "%I:%M:%S %p" */` |
|    ! 0 | 7949 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d %s",` |
|    ! 0 | 7950 | `				1+(pTm->tm_hour%12),` |
|    ! 0 | 7951 | `				pTm->tm_min,` |
|    ! 0 | 7952 | `				pTm->tm_sec,` |
|    ! 0 | 7953 | `				pTm->tm_hour > 12 ? "PM" : "AM"` |
|      - | 7954 | `				);` |
|    ! 0 | 7955 | `			break;` |
|      1 | 7956 | `		case 'D':` |
|      - | 7957 | `		case 'x':` |
|      - | 7958 | `			/* Same as "%m/%d/%y" */` |
|      4 | 7959 | `			ph7_result_string_format(pCtx,"%02d/%02d/%02d",` |
|      2 | 7960 | `				pTm->tm_mon+1,` |
|      1 | 7961 | `				pTm->tm_mday,` |
|      2 | 7962 | `				pTm->tm_year%100` |
|      - | 7963 | `				);` |
|      3 | 7964 | `			break;` |
|    ! 0 | 7965 | `		case 'F':` |
|      - | 7966 | `			/* Same as "%Y-%m-%d" */` |
|    ! 0 | 7967 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d",` |
|    ! 0 | 7968 | `				pTm->tm_year,` |
|    ! 0 | 7969 | `				pTm->tm_mon+1,` |
|    ! 0 | 7970 | `				pTm->tm_mday` |
|      - | 7971 | `				);` |
|    ! 0 | 7972 | `			break;` |
|    ! 0 | 7973 | `		case 'c':` |
|    ! 0 | 7974 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d %02d:%02d:%02d",` |
|    ! 0 | 7975 | `				pTm->tm_year,` |
|    ! 0 | 7976 | `				pTm->tm_mon+1,` |
|    ! 0 | 7977 | `				pTm->tm_mday,` |
|    ! 0 | 7978 | `				pTm->tm_hour,` |
|    ! 0 | 7979 | `				pTm->tm_min,` |
|    ! 0 | 7980 | `				pTm->tm_sec` |
|      - | 7981 | `				);` |
|    ! 0 | 7982 | `			break;` |
|    ! 0 | 7983 | `		case 's':{` |
|      - | 7984 | `			time_t tt;` |
|      - | 7985 | `			/* Seconds since the Unix Epoch */` |
|    ! 0 | 7986 | `			time(&tt);` |
|    ! 0 | 7987 | `			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);` |
|    ! 0 | 7988 | `			break;` |
|      - | 7989 | `				 }` |
|    ! 0 | 7990 | `		default:` |
|      - | 7991 | `			/* unknown specifer,simply ignore*/` |
|    ! 0 | 7992 | `			break;` |
|      - | 7993 | `		}` |
|      - | 7994 | `		/* Advance the cursor */` |
|     21 | 7995 | `		zIn++;` |
|      1 | 7996 | `	}` |
|     17 | 7997 | `	return SXRET_OK;` |
|      1 | 7998 |  |
|      - | 7999 | `/*` |
|      - | 8000 | ` * string date(string $format [, int $timestamp = time() ] )` |
|      - | 8001 | ` *  Returns a string formatted according to the given format string using` |
|      - | 8002 | ` *  the given integer timestamp or the current time if no timestamp is given.` |
|      - | 8003 | ` *  In other words, timestamp is optional and defaults to the value of time().` |
|      - | 8004 | ` * Parameters` |
|      - | 8005 | ` *  $format` |
|      - | 8006 | ` *   The format of the outputted date string (See code above)` |
|      - | 8007 | ` * $timestamp` |
|      - | 8008 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|      - | 8009 | ` *   that defaults to the current local time if a timestamp is not given.` |
|      - | 8010 | ` *   In other words, it defaults to the value of time().` |
|      - | 8011 | ` * Return` |
|      - | 8012 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|      - | 8013 | ` */` |
|     36 | 8014 | `static int PH7_builtin_date(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8015 |  |
|      - | 8016 | `	const char *zFormat;` |
|      - | 8017 | `	int nLen;` |
|      - | 8018 | `	Sytm sTm;` |
|     37 | 8019 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8020 | `		/* Missing/Invalid argument,return FALSE */` |
|      5 | 8021 | `		ph7_result_bool(pCtx,0);` |
|      5 | 8022 | `		return PH7_OK;` |
|      - | 8023 | `	}` |
|     33 | 8024 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     33 | 8025 | `	if( nLen < 1 ){` |
|      - | 8026 | `		/* Don't bother processing return the empty string */` |
|    ! 0 | 8027 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 8028 | `	}` |
|     33 | 8029 | `	if( nArg < 2 ){` |
|      - | 8030 | `#ifdef __WINNT__` |
|      - | 8031 | `		SYSTEMTIME sOS;` |
|      1 | 8032 | `		GetSystemTime(&sOS);` |
|      1 | 8033 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8034 | `#else` |
|      - | 8035 | `		struct tm *pTm;` |
|      - | 8036 | `		time_t t;` |
|     30 | 8037 | `		time(&t);` |
|     30 | 8038 | `		pTm = localtime(&t);` |
|     30 | 8039 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8040 | `#endif` |
|     16 | 8041 | `	}else{` |
|      - | 8042 | `		/* Use the given timestamp */` |
|      - | 8043 | `		time_t t;` |
|      - | 8044 | `		struct tm *pTm;` |
|      3 | 8045 | `		if( ph7_value_is_int(apArg[1]) ){` |
|      3 | 8046 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|      3 | 8047 | `			pTm = localtime(&t);` |
|      3 | 8048 | `			if( pTm == 0 ){` |
|    ! 0 | 8049 | `				time(&t);` |
|    ! 0 | 8050 | `			}` |
|      2 | 8051 | `		}else{` |
|    ! 0 | 8052 | `			time(&t);` |
|      - | 8053 | `		}` |
|      3 | 8054 | `		pTm = localtime(&t);` |
|      3 | 8055 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8056 | `	}` |
|      - | 8057 | `	/* Format the given string */` |
|     33 | 8058 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|     33 | 8059 | `	return PH7_OK;` |
|     19 | 8060 |  |
|      - | 8061 | `/*` |
|      - | 8062 | ` * string strftime(string $format [, int $timestamp = time() ] )` |
|      - | 8063 | ` *  Format a local time/date (PLATFORM INDEPENDANT IMPLEENTATION NOT BASED ON LOCALE)` |
|      - | 8064 | ` * Parameters` |
|      - | 8065 | ` *  $format` |
|      - | 8066 | ` *   The format of the outputted date string (See code above)` |
|      - | 8067 | ` * $timestamp` |
|      - | 8068 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|      - | 8069 | ` *   that defaults to the current local time if a timestamp is not given.` |
|      - | 8070 | ` *   In other words, it defaults to the value of time().` |
|      - | 8071 | ` * Return` |
|      - | 8072 | ` * Returns a string formatted according format using the given timestamp` |
|      - | 8073 | ` * or the current local time if no timestamp is given.` |
|      - | 8074 | ` */` |
|     20 | 8075 | `static int PH7_builtin_strftime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8076 |  |
|      - | 8077 | `	const char *zFormat;` |
|      - | 8078 | `	int nLen;` |
|      - | 8079 | `	Sytm sTm;` |
|     21 | 8080 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8081 | `		/* Missing/Invalid argument,return FALSE */` |
|      5 | 8082 | `		ph7_result_bool(pCtx,0);` |
|      5 | 8083 | `		return PH7_OK;` |
|      - | 8084 | `	}` |
|     17 | 8085 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8086 | `	if( nLen < 1 ){` |
|      - | 8087 | `		/* Don't bother processing return FALSE */` |
|    ! 0 | 8088 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8089 | `	}` |
|     17 | 8090 | `	if( nArg < 2 ){` |
|      - | 8091 | `#ifdef __WINNT__` |
|      - | 8092 | `		SYSTEMTIME sOS;` |
|      1 | 8093 | `		GetSystemTime(&sOS);` |
|      1 | 8094 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8095 | `#else` |
|      - | 8096 | `		struct tm *pTm;` |
|      - | 8097 | `		time_t t;` |
|     14 | 8098 | `		time(&t);` |
|     14 | 8099 | `		pTm = localtime(&t);` |
|     14 | 8100 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8101 | `#endif` |
|      8 | 8102 | `	}else{` |
|      - | 8103 | `		/* Use the given timestamp */` |
|      - | 8104 | `		time_t t;` |
|      - | 8105 | `		struct tm *pTm;` |
|      3 | 8106 | `		if( ph7_value_is_int(apArg[1]) ){` |
|      3 | 8107 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|      3 | 8108 | `			pTm = localtime(&t);` |
|      3 | 8109 | `			if( pTm == 0 ){` |
|    ! 0 | 8110 | `				time(&t);` |
|    ! 0 | 8111 | `			}` |
|      2 | 8112 | `		}else{` |
|    ! 0 | 8113 | `			time(&t);` |
|      - | 8114 | `		}` |
|      3 | 8115 | `		pTm = localtime(&t);` |
|      3 | 8116 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8117 | `	}` |
|      - | 8118 | `	/* Format the given string */` |
|     17 | 8119 | `	PH7_Strftime(pCtx,zFormat,nLen,&sTm);` |
|     17 | 8120 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|      - | 8121 | `		/* Nothing was formatted,return FALSE */` |
|    ! 0 | 8122 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8123 | `	}` |
|     17 | 8124 | `	return PH7_OK;` |
|     11 | 8125 |  |
|      - | 8126 | `/*` |
|      - | 8127 | ` * string gmdate(string $format [, int $timestamp = time() ] )` |
|      - | 8128 | ` *  Identical to the date() function except that the time returned` |
|      - | 8129 | ` *  is Greenwich Mean Time (GMT).` |
|      - | 8130 | ` * Parameters` |
|      - | 8131 | ` *  $format` |
|      - | 8132 | ` *  The format of the outputted date string (See code above)` |
|      - | 8133 | ` *  $timestamp` |
|      - | 8134 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|      - | 8135 | ` *   that defaults to the current local time if a timestamp is not given.` |
|      - | 8136 | ` *   In other words, it defaults to the value of time().` |
|      - | 8137 | ` * Return` |
|      - | 8138 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|      - | 8139 | ` */` |
|     16 | 8140 | `static int PH7_builtin_gmdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8141 |  |
|      - | 8142 | `	const char *zFormat;` |
|      - | 8143 | `	int nLen;` |
|      - | 8144 | `	Sytm sTm;` |
|     17 | 8145 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8146 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 | 8147 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8148 | `		return PH7_OK;` |
|      - | 8149 | `	}` |
|     15 | 8150 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 8151 | `	if( nLen < 1 ){` |
|      - | 8152 | `		/* Don't bother processing return the empty string */` |
|    ! 0 | 8153 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 8154 | `	}` |
|     15 | 8155 | `	if( nArg < 2 ){` |
|      - | 8156 | `#ifdef __WINNT__` |
|      - | 8157 | `		SYSTEMTIME sOS;` |
|      1 | 8158 | `		GetSystemTime(&sOS);` |
|      1 | 8159 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8160 | `#else` |
|      - | 8161 | `		struct tm *pTm;` |
|      - | 8162 | `		time_t t;` |
|     12 | 8163 | `		time(&t);` |
|     12 | 8164 | `		pTm = gmtime(&t);` |
|     12 | 8165 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8166 | `#endif` |
|      7 | 8167 | `	}else{` |
|      - | 8168 | `		/* Use the given timestamp */` |
|      - | 8169 | `		time_t t;` |
|      - | 8170 | `		struct tm *pTm;` |
|      3 | 8171 | `		if( ph7_value_is_int(apArg[1]) ){` |
|      3 | 8172 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|      3 | 8173 | `			pTm = gmtime(&t);` |
|      3 | 8174 | `			if( pTm == 0 ){` |
|    ! 0 | 8175 | `				time(&t);` |
|    ! 0 | 8176 | `			}` |
|      2 | 8177 | `		}else{` |
|    ! 0 | 8178 | `			time(&t);` |
|      - | 8179 | `		}` |
|      3 | 8180 | `		pTm = gmtime(&t);` |
|      3 | 8181 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8182 | `	}` |
|      - | 8183 | `	/* Format the given string */` |
|     15 | 8184 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|     15 | 8185 | `	return PH7_OK;` |
|      9 | 8186 |  |
|      - | 8187 | `/*` |
|      - | 8188 | ` * array localtime([ int $timestamp = time() [, bool $is_associative = false ]])` |
|      - | 8189 | ` *  Return the local time.` |
|      - | 8190 | ` * Parameter` |
|      - | 8191 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|      - | 8192 | ` *     that defaults to the current local time if a timestamp is not given.` |
|      - | 8193 | ` *     In other words, it defaults to the value of time().` |
|      - | 8194 | ` * $is_associative` |
|      - | 8195 | ` *   If set to FALSE or not supplied then the array is returned as a regular, numerically` |
|      - | 8196 | ` *   indexed array. If the argument is set to TRUE then localtime() returns an associative` |
|      - | 8197 | ` *   array containing all the different elements of the structure returned by the C function` |
|      - | 8198 | ` *   call to localtime. The names of the different keys of the associative array are as follows:` |
|      - | 8199 | ` *      "tm_sec" - seconds, 0 to 59` |
|      - | 8200 | ` *      "tm_min" - minutes, 0 to 59` |
|      - | 8201 | ` *      "tm_hour" - hours, 0 to 23` |
|      - | 8202 | ` *      "tm_mday" - day of the month, 1 to 31` |
|      - | 8203 | ` *      "tm_mon" - month of the year, 0 (Jan) to 11 (Dec)` |
|      - | 8204 | ` *      "tm_year" - years since 1900` |
|      - | 8205 | ` *      "tm_wday" - day of the week, 0 (Sun) to 6 (Sat)` |
|      - | 8206 | ` *      "tm_yday" - day of the year, 0 to 365` |
|      - | 8207 | ` *      "tm_isdst" - is daylight savings time in effect? Positive if yes, 0 if not, negative if unknown.` |
|      - | 8208 | ` * Returns` |
|      - | 8209 | ` *  An associative array of information related to the timestamp.` |
|      - | 8210 | ` */` |
|      8 | 8211 | `static int PH7_builtin_localtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8212 |  |
|      - | 8213 | `	ph7_value *pValue,*pArray;` |
|      9 | 8214 | `	int isAssoc = 0;` |
|      - | 8215 | `	Sytm sTm;` |
|      9 | 8216 | `	if( nArg < 1 ){` |
|      - | 8217 | `#ifdef __WINNT__` |
|      - | 8218 | `		SYSTEMTIME sOS;` |
|      1 | 8219 | `		GetSystemTime(&sOS); /* TODO(chems): GMT not local */` |
|      1 | 8220 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8221 | `#else` |
|      - | 8222 | `		struct tm *pTm;` |
|      - | 8223 | `		time_t t;` |
|      4 | 8224 | `		time(&t);` |
|      4 | 8225 | `		pTm = localtime(&t);` |
|      4 | 8226 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8227 | `#endif` |
|      3 | 8228 | `	}else{` |
|      - | 8229 | `		/* Use the given timestamp */` |
|      - | 8230 | `		time_t t;` |
|      - | 8231 | `		struct tm *pTm;` |
|      5 | 8232 | `		if( ph7_value_is_int(apArg[0]) ){` |
|      5 | 8233 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|      5 | 8234 | `			pTm = localtime(&t);` |
|      5 | 8235 | `			if( pTm == 0 ){` |
|    ! 0 | 8236 | `				time(&t);` |
|    ! 0 | 8237 | `			}` |
|      3 | 8238 | `		}else{` |
|    ! 0 | 8239 | `			time(&t);` |
|      - | 8240 | `		}` |
|      5 | 8241 | `		pTm = localtime(&t);` |
|      5 | 8242 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8243 | `	}` |
|      - | 8244 | `	/* Element value */` |
|      9 | 8245 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      9 | 8246 | `	if( pValue == 0 ){` |
|      - | 8247 | `		/* Return NULL */` |
|    ! 0 | 8248 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8249 | `		return PH7_OK;` |
|      - | 8250 | `	}` |
|      - | 8251 | `	/* Create a new array */` |
|      9 | 8252 | `	pArray = ph7_context_new_array(pCtx);` |
|      9 | 8253 | `	if( pArray == 0 ){` |
|      - | 8254 | `		/* Return NULL */` |
|    ! 0 | 8255 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8256 | `		return PH7_OK;` |
|      - | 8257 | `	}` |
|      9 | 8258 | `	if( nArg > 1 ){` |
|      3 | 8259 | `		isAssoc = ph7_value_to_bool(apArg[1]);` |
|      1 | 8260 | `	}` |
|      - | 8261 | `	/* Fill the array */` |
|      - | 8262 | `	/* Seconds */` |
|      9 | 8263 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|      9 | 8264 | `	if( isAssoc ){` |
|      3 | 8265 | `		ph7_array_add_strkey_elem(pArray,"tm_sec",pValue);` |
|      2 | 8266 | `	}else{` |
|      7 | 8267 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8268 | `	}` |
|      - | 8269 | `	/* Minutes */` |
|      9 | 8270 | `	ph7_value_int(pValue,sTm.tm_min);` |
|      9 | 8271 | `	if( isAssoc ){` |
|      3 | 8272 | `		ph7_array_add_strkey_elem(pArray,"tm_min",pValue);` |
|      2 | 8273 | `	}else{` |
|      7 | 8274 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8275 | `	}` |
|      - | 8276 | `	/* Hours */` |
|      9 | 8277 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|      9 | 8278 | `	if( isAssoc ){` |
|      3 | 8279 | `		ph7_array_add_strkey_elem(pArray,"tm_hour",pValue);` |
|      2 | 8280 | `	}else{` |
|      7 | 8281 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8282 | `	}` |
|      - | 8283 | `	/* mday */` |
|      9 | 8284 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|      9 | 8285 | `	if( isAssoc ){` |
|      3 | 8286 | `		ph7_array_add_strkey_elem(pArray,"tm_mday",pValue);` |
|      2 | 8287 | `	}else{` |
|      7 | 8288 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8289 | `	}` |
|      - | 8290 | `	/* mon */` |
|      9 | 8291 | `	ph7_value_int(pValue,sTm.tm_mon);` |
|      9 | 8292 | `	if( isAssoc ){` |
|      3 | 8293 | `		ph7_array_add_strkey_elem(pArray,"tm_mon",pValue);` |
|      2 | 8294 | `	}else{` |
|      7 | 8295 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8296 | `	}` |
|      - | 8297 | `	/* year since 1900 */` |
|      9 | 8298 | `	ph7_value_int(pValue,sTm.tm_year-1900);` |
|      9 | 8299 | `	if( isAssoc ){` |
|      3 | 8300 | `		ph7_array_add_strkey_elem(pArray,"tm_year",pValue);` |
|      2 | 8301 | `	}else{` |
|      7 | 8302 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8303 | `	}` |
|      - | 8304 | `	/* wday */` |
|      9 | 8305 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|      9 | 8306 | `	if( isAssoc ){` |
|      3 | 8307 | `		ph7_array_add_strkey_elem(pArray,"tm_wday",pValue);` |
|      2 | 8308 | `	}else{` |
|      7 | 8309 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8310 | `	}` |
|      - | 8311 | `	/* yday */` |
|      9 | 8312 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|      9 | 8313 | `	if( isAssoc ){` |
|      3 | 8314 | `		ph7_array_add_strkey_elem(pArray,"tm_yday",pValue);` |
|      2 | 8315 | `	}else{` |
|      7 | 8316 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8317 | `	}` |
|      - | 8318 | `	/* isdst */` |
|      - | 8319 | `#ifdef __WINNT__` |
|      - | 8320 | `#ifdef _MSC_VER` |
|      - | 8321 | `#ifndef _WIN32_WCE` |
|      1 | 8322 | `			_get_daylight(&sTm.tm_isdst);` |
|      - | 8323 | `#endif` |
|      - | 8324 | `#endif` |
|      - | 8325 | `#endif` |
|      9 | 8326 | `	ph7_value_int(pValue,sTm.tm_isdst);` |
|      9 | 8327 | `	if( isAssoc ){` |
|      3 | 8328 | `		ph7_array_add_strkey_elem(pArray,"tm_isdst",pValue);` |
|      2 | 8329 | `	}else{` |
|      7 | 8330 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8331 | `	}` |
|      - | 8332 | `	/* Return the array */` |
|      9 | 8333 | `	ph7_result_value(pCtx,pArray);` |
|      9 | 8334 | `	return PH7_OK;` |
|      5 | 8335 |  |
|      - | 8336 | `/*` |
|      - | 8337 | ` * int idate(string $format [, int $timestamp = time() ])` |
|      - | 8338 | ` *  Returns a number formatted according to the given format string` |
|      - | 8339 | ` *  using the given integer timestamp or the current local time if` |
|      - | 8340 | ` *  no timestamp is given. In other words, timestamp is optional and defaults` |
|      - | 8341 | ` *  to the value of time().` |
|      - | 8342 | ` *  Unlike the function date(), idate() accepts just one char in the format` |
|      - | 8343 | ` *  parameter.` |
|      - | 8344 | ` * $Parameters` |
|      - | 8345 | ` *  Supported format` |
|      - | 8346 | ` *   d 	Day of the month` |
|      - | 8347 | ` *   h 	Hour (12 hour format)` |
|      - | 8348 | ` *   H 	Hour (24 hour format)` |
|      - | 8349 | ` *   i 	Minutes` |
|      - | 8350 | ` *   I (uppercase i)1 if DST is activated, 0 otherwise` |
|      - | 8351 | ` *   L (uppercase l) returns 1 for leap year, 0 otherwise` |
|      - | 8352 | ` *   m 	Month number` |
|      - | 8353 | ` *   s 	Seconds` |
|      - | 8354 | ` *   t 	Days in current month` |
|      - | 8355 | ` *   U 	Seconds since the Unix Epoch - January 1 1970 00:00:00 UTC - this is the same as time()` |
|      - | 8356 | ` *   w 	Day of the week (0 on Sunday)` |
|      - | 8357 | ` *   W 	ISO-8601 week number of year, weeks starting on Monday` |
|      - | 8358 | ` *   y 	Year (1 or 2 digits - check note below)` |
|      - | 8359 | ` *   Y 	Year (4 digits)` |
|      - | 8360 | ` *   z 	Day of the year` |
|      - | 8361 | ` *   Z 	Timezone offset in seconds` |
|      - | 8362 | ` * $timestamp` |
|      - | 8363 | ` *  The optional timestamp parameter is an integer Unix timestamp that defaults` |
|      - | 8364 | ` *  to the current local time if a timestamp is not given. In other words, it defaults` |
|      - | 8365 | ` *  to the value of time().` |
|      - | 8366 | ` * Return` |
|      - | 8367 | ` *  An integer.` |
|      - | 8368 | ` */` |
|     40 | 8369 | `static int PH7_builtin_idate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 8370 |  |
|      - | 8371 | `	const char *zFormat;` |
|     42 | 8372 | `	ph7_int64 iVal = 0;` |
|      - | 8373 | `	int nLen;` |
|      - | 8374 | `	Sytm sTm;` |
|     42 | 8375 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8376 | `		/* Missing/Invalid argument,return -1 */` |
|      5 | 8377 | `		ph7_result_int(pCtx,-1);` |
|      5 | 8378 | `		return PH7_OK;` |
|      - | 8379 | `	}` |
|     42 | 8380 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     42 | 8381 | `	if( nLen < 1 ){` |
|      - | 8382 | `		/* Don't bother processing return -1*/` |
|    ! 0 | 8383 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 8384 | `	}` |
|     42 | 8385 | `	if( nArg < 2 ){` |
|      - | 8386 | `#ifdef __WINNT__` |
|      - | 8387 | `		SYSTEMTIME sOS;` |
|      2 | 8388 | `		GetSystemTime(&sOS);` |
|      2 | 8389 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8390 | `#else` |
|      - | 8391 | `		struct tm *pTm;` |
|      - | 8392 | `		time_t t;` |
|     30 | 8393 | `		time(&t);` |
|     30 | 8394 | `		pTm = localtime(&t);` |
|     30 | 8395 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8396 | `#endif` |
|     18 | 8397 | `	}else{` |
|      - | 8398 | `		/* Use the given timestamp */` |
|      - | 8399 | `		time_t t;` |
|      - | 8400 | `		struct tm *pTm;` |
|     11 | 8401 | `		if( ph7_value_is_int(apArg[1]) ){` |
|     11 | 8402 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|     11 | 8403 | `			pTm = localtime(&t);` |
|     11 | 8404 | `			if( pTm == 0 ){` |
|    ! 0 | 8405 | `				time(&t);` |
|    ! 0 | 8406 | `			}` |
|      6 | 8407 | `		}else{` |
|    ! 0 | 8408 | `			time(&t);` |
|      - | 8409 | `		}` |
|     11 | 8410 | `		pTm = localtime(&t);` |
|     11 | 8411 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8412 | `	}` |
|      - | 8413 | `	/* Perform the requested operation */` |
|     42 | 8414 | `	switch(zFormat[0]){` |
|      2 | 8415 | `	case 'd':` |
|      - | 8416 | `		/* Day of the month */` |
|      5 | 8417 | `		iVal = sTm.tm_mday;` |
|      5 | 8418 | `		break;` |
|    ! 0 | 8419 | `	case 'h':` |
|      - | 8420 | `		/*	Hour (12 hour format)*/` |
|    ! 0 | 8421 | `		iVal = 1 + (sTm.tm_hour % 12);` |
|    ! 0 | 8422 | `		break;` |
|      1 | 8423 | `	case 'H':` |
|      - | 8424 | `		/* Hour (24 hour format)*/` |
|      3 | 8425 | `		iVal = sTm.tm_hour;` |
|      3 | 8426 | `		break;` |
|      1 | 8427 | `	case 'i':` |
|      - | 8428 | `		/*Minutes*/` |
|      3 | 8429 | `		iVal = sTm.tm_min;` |
|      3 | 8430 | `		break;` |
|      1 | 8431 | `	case 'I':` |
|      - | 8432 | `		/*	returns 1 if DST is activated, 0 otherwise */` |
|      - | 8433 | `#ifdef __WINNT__` |
|      - | 8434 | `#ifdef _MSC_VER` |
|      - | 8435 | `#ifndef _WIN32_WCE` |
|      1 | 8436 | `			_get_daylight(&sTm.tm_isdst);` |
|      - | 8437 | `#endif` |
|      - | 8438 | `#endif` |
|      - | 8439 | `#endif` |
|      3 | 8440 | `		iVal = sTm.tm_isdst;` |
|      3 | 8441 | `		break;` |
|      1 | 8442 | `	case 'L':` |
|      - | 8443 | `		/* 	returns 1 for leap year, 0 otherwise */` |
|      3 | 8444 | `		iVal = IS_LEAP_YEAR(sTm.tm_year);` |
|      3 | 8445 | `		break;` |
|      2 | 8446 | `	case 'm':` |
|      - | 8447 | `		/* Month number*/` |
|      5 | 8448 | `		iVal = sTm.tm_mon;` |
|      5 | 8449 | `		break;` |
|      1 | 8450 | `	case 's':` |
|      - | 8451 | `		/*Seconds*/` |
|      3 | 8452 | `		iVal = sTm.tm_sec;` |
|      3 | 8453 | `		break;` |
|      1 | 8454 | `	case 't':{` |
|      - | 8455 | `		/*Days in current month*/` |
|      - | 8456 | `		static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|      5 | 8457 | `		int nDays = aMonDays[sTm.tm_mon % 12 ];` |
|      5 | 8458 | `		if( sTm.tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(sTm.tm_year) ){` |
|      3 | 8459 | `			nDays = 28;` |
|      1 | 8460 | `		}` |
|      7 | 8461 | `		iVal = nDays;` |
|      7 | 8462 | `		break;` |
|      - | 8463 | `			 }` |
|      1 | 8464 | `	case 'U':` |
|      - | 8465 | `		/*Seconds since the Unix Epoch*/` |
|      3 | 8466 | `		iVal = (ph7_int64)time(0);` |
|      3 | 8467 | `		break;` |
|      1 | 8468 | `	case 'w':` |
|      - | 8469 | `		/*	Day of the week (0 on Sunday) */` |
|      3 | 8470 | `		iVal = sTm.tm_wday;` |
|      3 | 8471 | `		break;` |
|      1 | 8472 | `	case 'W': {` |
|      - | 8473 | `		/* ISO-8601 week number of year, weeks starting on Monday */` |
|      - | 8474 | `		static const int aISO8601_local[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|      3 | 8475 | `		iVal = aISO8601_local[sTm.tm_wday % 7 ];` |
|      3 | 8476 | `		break;` |
|      - | 8477 | `			  }` |
|    ! 0 | 8478 | `	case 'y':` |
|      - | 8479 | `		/* Year (2 digits) */` |
|    ! 0 | 8480 | `		iVal = sTm.tm_year % 100;` |
|    ! 0 | 8481 | `		break;` |
|      3 | 8482 | `	case 'Y':` |
|      - | 8483 | `		/* Year (4 digits) */` |
|      7 | 8484 | `		iVal = sTm.tm_year;` |
|      7 | 8485 | `		break;` |
|      1 | 8486 | `	case 'z':` |
|      - | 8487 | `		/* Day of the year */` |
|      3 | 8488 | `		iVal = sTm.tm_yday;` |
|      3 | 8489 | `		break;` |
|      1 | 8490 | `	case 'Z':` |
|      - | 8491 | `		/*Timezone offset in seconds*/` |
|      3 | 8492 | `		iVal = sTm.tm_gmtoff;` |
|      3 | 8493 | `		break;` |
|      1 | 8494 | `	default:` |
|      - | 8495 | `		/* unknown format,throw a warning */` |
|      3 | 8496 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Unknown date format token");` |
|      2 | 8497 | `		break;` |
|      - | 8498 | `	}` |
|      - | 8499 | `	/* Return the time value */` |
|     40 | 8500 | `	ph7_result_int64(pCtx,iVal);` |
|     40 | 8501 | `	return PH7_OK;` |
|     23 | 8502 |  |
|      - | 8503 | `/*` |
|      - | 8504 | ` * int mktime/gmmktime([ int $hour = date("H") [, int $minute = date("i") [, int $second = date("s")` |
|      - | 8505 | ` *  [, int $month = date("n") [, int $day = date("j") [, int $year = date("Y") [, int $is_dst = -1 ]]]]]]] )` |
|      - | 8506 | ` *  Returns the Unix timestamp corresponding to the arguments given. This timestamp is a 64bit integer` |
|      - | 8507 | ` *  containing the number of seconds between the Unix Epoch (January 1 1970 00:00:00 GMT) and the time` |
|      - | 8508 | ` *  specified.` |
|      - | 8509 | ` *  Arguments may be left out in order from right to left; any arguments thus omitted will be set to` |
|      - | 8510 | ` *  the current value according to the local date and time.` |
|      - | 8511 | ` * Parameters` |
|      - | 8512 | ` * $hour` |
|      - | 8513 | ` *  The number of the hour relevant to the start of the day determined by month, day and year.` |
|      - | 8514 | ` *  Negative values reference the hour before midnight of the day in question. Values greater` |
|      - | 8515 | ` *  than 23 reference the appropriate hour in the following day(s).` |
|      - | 8516 | ` * $minute` |
|      - | 8517 | ` *  The number of the minute relevant to the start of the hour. Negative values reference` |
|      - | 8518 | ` *  the minute in the previous hour. Values greater than 59 reference the appropriate minute` |
|      - | 8519 | ` *  in the following hour(s).` |
|      - | 8520 | ` * $second` |
|      - | 8521 | ` *  The number of seconds relevant to the start of the minute. Negative values reference` |
|      - | 8522 | ` *  the second in the previous minute. Values greater than 59 reference the appropriate` |
|      - | 8523 | ` * second in the following minute(s).` |
|      - | 8524 | ` * $month` |
|      - | 8525 | ` *  The number of the month relevant to the end of the previous year. Values 1 to 12 reference` |
|      - | 8526 | ` *  the normal calendar months of the year in question. Values less than 1 (including negative values)` |
|      - | 8527 | ` *  reference the months in the previous year in reverse order, so 0 is December, -1 is November)...` |
|      - | 8528 | ` * $day` |
|      - | 8529 | ` *  The number of the day relevant to the end of the previous month. Values 1 to 28, 29, 30 or 31` |
|      - | 8530 | ` *  (depending upon the month) reference the normal days in the relevant month. Values less than 1` |
|      - | 8531 | ` *  (including negative values) reference the days in the previous month, so 0 is the last day` |
|      - | 8532 | ` *  of the previous month, -1 is the day before that, etc. Values greater than the number of days` |
|      - | 8533 | ` *  in the relevant month reference the appropriate day in the following month(s).` |
|      - | 8534 | ` * $year` |
|      - | 8535 | ` *  The number of the year, may be a two or four digit value, with values between 0-69 mapping` |
|      - | 8536 | ` *  to 2000-2069 and 70-100 to 1970-2000. On systems where time_t is a 32bit signed integer, as` |
|      - | 8537 | ` *  most common today, the valid range for year is somewhere between 1901 and 2038.` |
|      - | 8538 | ` * $is_dst` |
|      - | 8539 | ` *  This parameter can be set to 1 if the time is during daylight savings time (DST), 0 if it is not,` |
|      - | 8540 | ` *  or -1 (the default) if it is unknown whether the time is within daylight savings time or not.` |
|      - | 8541 | ` * Return` |
|      - | 8542 | ` *   mktime() returns the Unix timestamp of the arguments given.` |
|      - | 8543 | ` *   If the arguments are invalid, the function returns FALSE` |
|      - | 8544 | ` */` |
|      8 | 8545 | `static int PH7_builtin_mktime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8546 |  |
|      - | 8547 | `	const char *zFunction;` |
|      9 | 8548 | `	ph7_int64 iVal = 0;` |
|      - | 8549 | `	struct tm *pTm;` |
|      - | 8550 | `	time_t t;` |
|      - | 8551 | `	/* Extract function name */` |
|      9 | 8552 | `	zFunction = ph7_function_name(pCtx);` |
|      - | 8553 | `	/* Get the current time */` |
|      9 | 8554 | `	time(&t);` |
|      9 | 8555 | `	if( zFunction[0] == 'g' /* gmmktime */ ){` |
|      3 | 8556 | `		pTm = gmtime(&t);` |
|      2 | 8557 | `	}else{` |
|      - | 8558 | `		/* localtime */` |
|      7 | 8559 | `		pTm = localtime(&t);` |
|      - | 8560 | `	}` |
|      9 | 8561 | `	if( nArg > 0 ){` |
|      - | 8562 | `		int iTmp;` |
|      - | 8563 | `		/* Hour */` |
|      9 | 8564 | `		iTmp = ph7_value_to_int(apArg[0]);` |
|      9 | 8565 | `		pTm->tm_hour = iTmp;` |
|      9 | 8566 | `		if( nArg > 1 ){` |
|      - | 8567 | `			/* Minutes */` |
|      9 | 8568 | `			iTmp = ph7_value_to_int(apArg[1]);` |
|      9 | 8569 | `			pTm->tm_min = iTmp;` |
|      9 | 8570 | `			if( nArg > 2 ){` |
|      - | 8571 | `				/* Seconds */` |
|      9 | 8572 | `				iTmp = ph7_value_to_int(apArg[2]);` |
|      9 | 8573 | `				pTm->tm_sec = iTmp;` |
|      9 | 8574 | `				if( nArg > 3 ){` |
|      - | 8575 | `					/* Month */` |
|      9 | 8576 | `					iTmp = ph7_value_to_int(apArg[3]);` |
|      9 | 8577 | `					pTm->tm_mon = iTmp - 1;` |
|      9 | 8578 | `					if( nArg > 4 ){` |
|      - | 8579 | `						/* mday */` |
|      9 | 8580 | `						iTmp = ph7_value_to_int(apArg[4]);` |
|      9 | 8581 | `						pTm->tm_mday = iTmp;` |
|      9 | 8582 | `						if( nArg > 5 ){` |
|      - | 8583 | `							/* Year */` |
|      9 | 8584 | `							iTmp = ph7_value_to_int(apArg[5]);` |
|      9 | 8585 | `							if( iTmp > 1900 ){` |
|      9 | 8586 | `								iTmp -= 1900;` |
|      4 | 8587 | `							}` |
|      9 | 8588 | `							pTm->tm_year = iTmp;` |
|      9 | 8589 | `							if( nArg > 6 ){` |
|      - | 8590 | `								/* is_dst */` |
|    ! 0 | 8591 | `								iTmp = ph7_value_to_bool(apArg[6]);` |
|    ! 0 | 8592 | `								pTm->tm_isdst = iTmp;` |
|    ! 0 | 8593 | `							}` |
|      4 | 8594 | `						}` |
|      4 | 8595 | `					}` |
|      4 | 8596 | `				}` |
|      4 | 8597 | `			}` |
|      4 | 8598 | `		}` |
|      4 | 8599 | `	}` |
|      - | 8600 | `	/* Make the time */` |
|      9 | 8601 | `	iVal = (ph7_int64)mktime(pTm);` |
|      - | 8602 | `	/* Return the timesatmp as a 64bit integer */` |
|      9 | 8603 | `	ph7_result_int64(pCtx,iVal);` |
|      9 | 8604 | `	return PH7_OK;` |
|      1 | 8605 |  |
|      - | 8606 | `/*` |
|      - | 8607 | ` * Section:` |
|      - | 8608 | ` *    URL handling Functions.` |
|      - | 8609 | ` * Status:` |
|      - | 8610 | ` *    Stable.` |
|      - | 8611 | ` */` |
|      - | 8612 | `/*` |
|      - | 8613 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 8614 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 8615 | ` */` |
|   1026 | 8616 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 8617 |  |
|      - | 8618 | `	/* Store in the call context result buffer */` |
|   1028 | 8619 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 8620 | `	return SXRET_OK;` |
|      2 | 8621 |  |
|      - | 8622 | `/*` |
|      - | 8623 | ` * string base64_encode(string $data)` |
|      - | 8624 | ` * string convert_uuencode(string $data)` |
|      - | 8625 | ` *  Encodes data with MIME base64` |
|      - | 8626 | ` * Parameter` |
|      - | 8627 | ` *  $data` |
|      - | 8628 | ` *    Data to encode` |
|      - | 8629 | ` * Return` |
|      - | 8630 | ` *  Encoded data or FALSE on failure.` |
|      - | 8631 | ` */` |
|     10 | 8632 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8633 |  |
|      - | 8634 | `	const char *zIn;` |
|      - | 8635 | `	int nLen;` |
|     11 | 8636 | `	if( nArg < 1 ){` |
|      - | 8637 | `		/* Missing arguments,return FALSE */` |
|      5 | 8638 | `		ph7_result_bool(pCtx,0);` |
|      5 | 8639 | `		return PH7_OK;` |
|      - | 8640 | `	}` |
|      - | 8641 | `	/* Extract the input string */` |
|      7 | 8642 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8643 | `	if( nLen < 1 ){` |
|      - | 8644 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8645 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8646 | `		return PH7_OK;` |
|      - | 8647 | `	}` |
|      - | 8648 | `	/* Perform the BASE64 encoding */` |
|      7 | 8649 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 8650 | `	return PH7_OK;` |
|      6 | 8651 |  |
|      - | 8652 | `/*` |
|      - | 8653 | ` * string base64_decode(string $data)` |
|      - | 8654 | ` * string convert_uudecode(string $data)` |
|      - | 8655 | ` *  Decodes data encoded with MIME base64` |
|      - | 8656 | ` * Parameter` |
|      - | 8657 | ` *  $data` |
|      - | 8658 | ` *    Encoded data.` |
|      - | 8659 | ` * Return` |
|      - | 8660 | ` *  Returns the original data or FALSE on failure.` |
|      - | 8661 | ` */` |
|     36 | 8662 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 8663 |  |
|      - | 8664 | `	const char *zIn;` |
|      - | 8665 | `	int nLen;` |
|     38 | 8666 | `	if( nArg < 1 ){` |
|      - | 8667 | `		/* Missing arguments,return FALSE */` |
|      3 | 8668 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8669 | `		return PH7_OK;` |
|      - | 8670 | `	}` |
|      - | 8671 | `	/* Extract the input string */` |
|     36 | 8672 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 8673 | `	if( nLen < 1 ){` |
|      - | 8674 | `		/* Nothing to process,return FALSE */` |
|      3 | 8675 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8676 | `		return PH7_OK;` |
|      - | 8677 | `	}` |
|      - | 8678 | `	/* Perform the BASE64 decoding */` |
|     34 | 8679 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 8680 | `	return PH7_OK;` |
|     20 | 8681 |  |
|      - | 8682 | `/*` |
|      - | 8683 | ` * string urlencode(string $str)` |
|      - | 8684 | ` *  URL encoding` |
|      - | 8685 | ` * Parameter` |
|      - | 8686 | ` *  $data` |
|      - | 8687 | ` *   Input string.` |
|      - | 8688 | ` * Return` |
|      - | 8689 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 8690 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 8691 | ` *  encoded as plus (+) signs.` |
|      - | 8692 | ` */` |
|      6 | 8693 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8694 |  |
|      - | 8695 | `	const char *zIn;` |
|      - | 8696 | `	int nLen;` |
|      7 | 8697 | `	if( nArg < 1 ){` |
|      - | 8698 | `		/* Missing arguments,return FALSE */` |
|      3 | 8699 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8700 | `		return PH7_OK;` |
|      - | 8701 | `	}` |
|      - | 8702 | `	/* Extract the input string */` |
|      5 | 8703 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 8704 | `	if( nLen < 1 ){` |
|      - | 8705 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8706 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8707 | `		return PH7_OK;` |
|      - | 8708 | `	}` |
|      - | 8709 | `	/* Perform the URL encoding */` |
|      5 | 8710 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 8711 | `	return PH7_OK;` |
|      4 | 8712 |  |
|      - | 8713 | `/*` |
|      - | 8714 | ` * string urldecode(string $str)` |
|      - | 8715 | ` *  Decodes any %## encoding in the given string.` |
|      - | 8716 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 8717 | ` * Parameter` |
|      - | 8718 | ` *  $data` |
|      - | 8719 | ` *    Input string.` |
|      - | 8720 | ` * Return` |
|      - | 8721 | ` *  Decoded URL or FALSE on failure.` |
|      - | 8722 | ` */` |
|      8 | 8723 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8724 |  |
|      - | 8725 | `	const char *zIn;` |
|      - | 8726 | `	int nLen;` |
|      9 | 8727 | `	if( nArg < 1 ){` |
|      - | 8728 | `		/* Missing arguments,return FALSE */` |
|      3 | 8729 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8730 | `		return PH7_OK;` |
|      - | 8731 | `	}` |
|      - | 8732 | `	/* Extract the input string */` |
|      7 | 8733 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8734 | `	if( nLen < 1 ){` |
|      - | 8735 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8736 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8737 | `		return PH7_OK;` |
|      - | 8738 | `	}` |
|      - | 8739 | `	/* Perform the URL decoding */` |
|      7 | 8740 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 8741 | `	return PH7_OK;` |
|      5 | 8742 |  |
|      - | 8743 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8744 | `/* Table of the built-in functions */` |
|      - | 8745 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 8746 | `	   /* Variable handling functions */` |
|      - | 8747 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 8748 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 8749 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 8750 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 8751 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 8752 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 8753 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 8754 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 8755 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 8756 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 8757 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 8758 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 8759 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 8760 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 8761 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 8762 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 8763 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 8764 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 8765 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 8766 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8767 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 8768 | `	   /* Math functions */` |
|      - | 8769 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 8770 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 8771 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 8772 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 8773 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 8774 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 8775 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 8776 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 8777 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 8778 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 8779 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 8780 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 8781 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 8782 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 8783 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 8784 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 8785 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 8786 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 8787 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 8788 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 8789 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 8790 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 8791 | `	{ "round",    PH7_builtin_round        },` |
|      - | 8792 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 8793 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 8794 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 8795 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 8796 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 8797 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 8798 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 8799 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 8800 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8801 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8802 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 8803 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8804 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8805 | `	   /* String handling functions */` |
|      - | 8806 |  |
|      - | 8807 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 8808 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 8809 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 8810 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 8811 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 8812 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 8813 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 8814 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 8815 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 8816 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 8817 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 8818 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 8819 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 8820 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 8821 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 8822 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 8823 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 8824 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 8825 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 8826 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 8827 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 8828 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 8829 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 8830 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 8831 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 8832 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 8833 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 8834 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 8835 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 8836 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 8837 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 8838 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 8839 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 8840 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 8841 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 8842 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 8843 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 8844 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 8845 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 8846 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 8847 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 8848 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 8849 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 8850 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 8851 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 8852 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 8853 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 8854 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 8855 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 8856 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8857 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8858 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 8859 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 8860 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 8861 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 8862 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8863 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8864 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 8865 |  |
|      - | 8866 |  |
|      - | 8867 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 8868 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 8869 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 8870 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 8871 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 8872 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8873 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8874 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 8875 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 8876 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8877 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8878 |  |
|      - | 8879 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 8880 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 8881 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 8882 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 8883 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 8884 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 8885 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 8886 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 8887 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 8888 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 8889 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 8890 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 8891 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8892 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8893 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 8894 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8895 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8896 |  |
|      - | 8897 | `	         /* Ctype functions */` |
|      - | 8898 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 8899 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 8900 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 8901 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 8902 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 8903 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 8904 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 8905 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 8906 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 8907 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 8908 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 8909 | `	         /* Time functions */` |
|      - | 8910 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 8911 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 8912 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 8913 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 8914 | `	{ "date",        PH7_builtin_date         },` |
|      - | 8915 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 8916 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 8917 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 8918 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 8919 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 8920 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 8921 | `	        /* URL functions */` |
|      - | 8922 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 8923 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 8924 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 8925 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 8926 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 8927 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 8928 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 8929 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 8930 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8931 | `};` |
|      - | 8932 | `/*` |
|      - | 8933 | ` * Register the built-in functions defined above,the array functions` |
|      - | 8934 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 8935 | ` */` |
|   1006 | 8936 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      2 | 8937 |  |
|      - | 8938 | `	sxu32 n;` |
| 153920 | 8939 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 152914 | 8940 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
|  76458 | 8941 | `	}` |
|      - | 8942 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   1008 | 8943 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 8944 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   1008 | 8945 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   1008 | 8946 |  |
|      - | 8947 |  |
