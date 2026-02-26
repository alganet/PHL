# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3761/4411 lines (85.26%)

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
|  15910 |  269 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  270 |  |
|  15912 |  271 | `	int res = 1; /* Assume empty by default */` |
|  15912 |  272 | `	if( nArg > 0 ){` |
|  15910 |  273 | `		res = ph7_value_is_empty(apArg[0]);` |
|   7954 |  274 | `	}` |
|  15912 |  275 | `	ph7_result_bool(pCtx,res);` |
|  15912 |  276 | `	return PH7_OK;` |
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
| 112538 | 1288 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1289 |  |
|      - | 1290 | `	const char *zSource,*zOfft;` |
|      - | 1291 | `	int nOfft,nLen,nSrcLen;` |
| 112540 | 1292 | `	if( nArg < 2 ){` |
|      - | 1293 | `		/* return FALSE */` |
|      5 | 1294 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1295 | `		return PH7_OK;` |
|      - | 1296 | `	}` |
|      - | 1297 | `	/* Extract the target string */` |
| 112536 | 1298 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 112536 | 1299 | `	if( nSrcLen < 1 ){` |
|      - | 1300 | `		/* Empty string,return FALSE */` |
|   7124 | 1301 | `		ph7_result_bool(pCtx,0);` |
|   7124 | 1302 | `		return PH7_OK;` |
|      - | 1303 | `	}` |
| 105414 | 1304 | `	nLen = nSrcLen; /* cc warning */` |
|      - | 1305 | `	/* Extract the offset */` |
| 105414 | 1306 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 105414 | 1307 | `	if( nOfft < 0 ){` |
|  17546 | 1308 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  17546 | 1309 | `		if( zOfft < zSource ){` |
|      - | 1310 | `			/* Invalid offset */` |
|      5 | 1311 | `			ph7_result_bool(pCtx,0);` |
|      5 | 1312 | `			return PH7_OK;` |
|      - | 1313 | `		}` |
|  17542 | 1314 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  17542 | 1315 | `		nOfft = (int)(zOfft-zSource);` |
|  96640 | 1316 | `	}else if( nOfft >= nSrcLen ){` |
|      - | 1317 | `		/* Invalid offset */` |
|      7 | 1318 | `		ph7_result_bool(pCtx,0);` |
|      7 | 1319 | `		return PH7_OK;` |
|    ! 0 | 1320 | `	}else{` |
|  87864 | 1321 | `		zOfft = &zSource[nOfft];` |
|  87864 | 1322 | `		nLen = nSrcLen - nOfft;` |
|      - | 1323 | `	}` |
| 105404 | 1324 | `	if( nArg > 2 ){` |
|      - | 1325 | `		/* Extract the length */` |
|  87862 | 1326 | `		nLen = ph7_value_to_int(apArg[2]);` |
|  87862 | 1327 | `		if( nLen == 0 ){` |
|      - | 1328 | `			/* Invalid length,return an empty string */` |
|      5 | 1329 | `			ph7_result_string(pCtx,"",0);` |
|      5 | 1330 | `			return PH7_OK;` |
|  87858 | 1331 | `		}else if( nLen < 0 ){` |
|  17544 | 1332 | `			nLen = nSrcLen + nLen - nOfft;` |
|  17544 | 1333 | `			if( nLen < 1 ){` |
|      - | 1334 | `				/* Invalid  length */` |
|      3 | 1335 | `				nLen = nSrcLen - nOfft;` |
|      1 | 1336 | `			}` |
|   8771 | 1337 | `		}` |
|  87858 | 1338 | `		if( nLen + nOfft > nSrcLen ){` |
|      - | 1339 | `			/* Invalid length */` |
|   2174 | 1340 | `			nLen = nSrcLen - nOfft;` |
|   1086 | 1341 | `		}` |
|  43928 | 1342 | `	}` |
|      - | 1343 | `	/* Return the substring */` |
| 105400 | 1344 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 105400 | 1345 | `	return PH7_OK;` |
|  56271 | 1346 |  |
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
|    118 | 1667 | `static int cSlashCheckMask(int c,const char *zMask,int nLen)` |
|      1 | 1668 |  |
|    119 | 1669 | `	const char *zEnd = &zMask[nLen];` |
|    557 | 1670 | `	while( zMask < zEnd ){` |
|    481 | 1671 | `		if( zMask[0] == c ){` |
|      - | 1672 | `			/* Character present,return TRUE */` |
|     43 | 1673 | `			return 1;` |
|      - | 1674 | `		}` |
|      - | 1675 | `		/* Advance the pointer */` |
|    439 | 1676 | `		zMask++;` |
|      1 | 1677 | `	}` |
|      - | 1678 | `	/* Not present */` |
|     77 | 1679 | `	return 0;` |
|     60 | 1680 |  |
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
|     34 | 1696 | `static int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1697 |  |
|      - | 1698 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - | 1699 | `	int nLen,nMask;` |
|      - | 1700 | `	/* PHP enforces exactly two arguments. */` |
|     36 | 1701 | `	if( nArg != 2 ){` |
|      7 | 1702 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1703 | `			"ArgumentCountError",` |
|      - | 1704 | `			"addcslashes() expects exactly 2 arguments, %d given",` |
|      2 | 1705 | `			nArg` |
|      - | 1706 | `			);` |
|      - | 1707 | `	}` |
|      - | 1708 | `	/* First argument must be a string-ish value.  NULL is treated as an` |
|      - | 1709 | `	 * explicit type error (we mirror addslashes behavior). */` |
|     57 | 1710 | `	if( ph7_value_is_null(apArg[0]) \|\|` |
|     41 | 1711 | `	    ph7_value_is_array(apArg[0]) \|\|` |
|     41 | 1712 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     26 | 1713 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      7 | 1714 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1715 | `			"TypeError",` |
|      - | 1716 | `			"addcslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      2 | 1717 | `			ph7_type_name(apArg[0])` |
|      - | 1718 | `			);` |
|      - | 1719 | `	}` |
|      - | 1720 | `	/* Second argument must be a string.  NULL is explicitly rejected as a` |
|      - | 1721 | `	 * TypeError (PHP only emits a deprecation).  Arrays/objects/resources` |
|      - | 1722 | `	 * also trigger a TypeError. */` |
|     49 | 1723 | `	if( ph7_value_is_null(apArg[1]) \|\|` |
|     35 | 1724 | `	    ph7_value_is_array(apArg[1]) \|\|` |
|     35 | 1725 | `	    ph7_value_is_object(apArg[1]) \|\|` |
|     22 | 1726 | `	    ph7_value_is_resource(apArg[1]) ){` |
|      7 | 1727 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1728 | `			"TypeError",` |
|      - | 1729 | `			"addcslashes(): Argument #2 ($characters) must be of type string, %s given",` |
|      4 | 1730 | `			ph7_type_name(apArg[1])` |
|      - | 1731 | `			);` |
|      - | 1732 | `	}` |
|      - | 1733 | `	/* Extract the string to process */` |
|     23 | 1734 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1735 | `	/* NULL would never reach here due to the check above. */` |
|     23 | 1736 | `	if( nLen < 1 ){` |
|      - | 1737 | `		/* Empty string returns itself. */` |
|      3 | 1738 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      3 | 1739 | `		return PH7_OK;` |
|      - | 1740 | `	}` |
|      - | 1741 | `	/* Extract the desired mask */` |
|     21 | 1742 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|     21 | 1743 | `	zEnd = &zIn[nLen];` |
|     21 | 1744 | `	zCur = 0; /* cc warning */` |
|     27 | 1745 | `	for(;;){` |
|     55 | 1746 | `		if( zIn >= zEnd ){` |
|      - | 1747 | `			/* No more input */` |
|     21 | 1748 | `			break;` |
|      - | 1749 | `		}` |
|     35 | 1750 | `		zCur = zIn;` |
|     85 | 1751 | `		while( zIn < zEnd && !cSlashCheckMask(zIn[0],zMask,nMask) ){` |
|     51 | 1752 | `			zIn++;` |
|      1 | 1753 | `		}` |
|     35 | 1754 | `		if( zIn > zCur ){` |
|      - | 1755 | `			/* Append raw contents */` |
|     33 | 1756 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 | 1757 | `		}` |
|     35 | 1758 | `		if( zIn < zEnd ){` |
|      - | 1759 | `			/* Make sure we treat the byte as unsigned to avoid negative values` |
|      - | 1760 | `			 * on platforms where char is signed. */` |
|     17 | 1761 | `			int c = (unsigned char)zIn[0];` |
|      - | 1762 | `			/* Handle special C-like escapes for common control characters first.` |
|      - | 1763 | `			 * PHP outputs "\n" "\r" "\t" "\v" "\f" when those chars are` |
|      - | 1764 | `			 * in the mask. NUL is left to the octal conversion below. */` |
|     17 | 1765 | `			if( c == '\n' ){` |
|      3 | 1766 | `				ph7_result_string(pCtx,"\\n",2);` |
|     16 | 1767 | `			}else if( c == '\r' ){` |
|      3 | 1768 | `				ph7_result_string(pCtx,"\\r",2);` |
|     14 | 1769 | `			}else if( c == '\t' ){` |
|      3 | 1770 | `				ph7_result_string(pCtx,"\\t",2);` |
|     12 | 1771 | `			}else if( c == '\v' ){` |
|      3 | 1772 | `				ph7_result_string(pCtx,"\\v",2);` |
|     10 | 1773 | `			}else if( c == '\f' ){` |
|      3 | 1774 | `				ph7_result_string(pCtx,"\\f",2);` |
|      8 | 1775 | `			}else if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|      - | 1776 | `				/* Convert to octal */` |
|      5 | 1777 | `				ph7_result_string_format(pCtx,"\\%o",c);` |
|      3 | 1778 | `			}else{` |
|      3 | 1779 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1780 | `			}` |
|      8 | 1781 | `		}` |
|     35 | 1782 | `		zIn++;` |
|      1 | 1783 | `	}` |
|     21 | 1784 | `	return PH7_OK;` |
|     19 | 1785 |  |
|      - | 1786 | `/*` |
|      - | 1787 | ` * string quotemeta(string $str)` |
|      - | 1788 | ` *  Quote meta characters.` |
|      - | 1789 | ` * Parameter` |
|      - | 1790 | ` *  $str:` |
|      - | 1791 | ` *    The string to be escaped.` |
|      - | 1792 | ` * Return` |
|      - | 1793 | ` *  Returns the escaped string.` |
|      - | 1794 | `*/` |
|     10 | 1795 | `static int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1796 |  |
|      - | 1797 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1798 | `	int nLen;` |
|     11 | 1799 | `	if( nArg < 1 ){` |
|      - | 1800 | `		/* Nothing to process,retun NULL */` |
|      3 | 1801 | `		ph7_result_null(pCtx);` |
|      3 | 1802 | `		return PH7_OK;` |
|      - | 1803 | `	}` |
|      - | 1804 | `	/* Extract the string to process */` |
|      9 | 1805 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 1806 | `	if( nLen < 1 ){` |
|      - | 1807 | `		/* Return the empty string */` |
|      3 | 1808 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1809 | `		return PH7_OK;` |
|      - | 1810 | `	}` |
|      7 | 1811 | `	zEnd = &zIn[nLen];` |
|      7 | 1812 | `	zCur = 0; /* cc warning */` |
|     17 | 1813 | `	for(;;){` |
|     35 | 1814 | `		if( zIn >= zEnd ){` |
|      - | 1815 | `			/* No more input */` |
|      7 | 1816 | `			break;` |
|      - | 1817 | `		}` |
|     29 | 1818 | `		zCur = zIn;` |
|     55 | 1819 | `		while( zIn < zEnd && !cSlashCheckMask(zIn[0],".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1) ){` |
|     27 | 1820 | `			zIn++;` |
|      1 | 1821 | `		}` |
|     29 | 1822 | `		if( zIn > zCur ){` |
|      - | 1823 | `			/* Append raw contents */` |
|     11 | 1824 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      5 | 1825 | `		}` |
|     29 | 1826 | `		if( zIn < zEnd ){` |
|     27 | 1827 | `			int c = zIn[0];` |
|     27 | 1828 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|     13 | 1829 | `		}` |
|     29 | 1830 | `		zIn++;` |
|      1 | 1831 | `	}` |
|      7 | 1832 | `	return PH7_OK;` |
|      6 | 1833 |  |
|      - | 1834 | `/*` |
|      - | 1835 | ` * string stripslashes(string $str)` |
|      - | 1836 | ` *  Un-quotes a quoted string.` |
|      - | 1837 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1838 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1839 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1840 | ` * Parameter` |
|      - | 1841 | ` *  $str` |
|      - | 1842 | ` *   The input string.` |
|      - | 1843 | ` * Return` |
|      - | 1844 | ` *  Returns a string with backslashes stripped off.` |
|      - | 1845 | ` */` |
|      8 | 1846 | `static int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1847 |  |
|      - | 1848 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1849 | `	int nLen;` |
|      9 | 1850 | `	if( nArg < 1 ){` |
|      - | 1851 | `		/* Nothing to process,retun NULL */` |
|      3 | 1852 | `		ph7_result_null(pCtx);` |
|      3 | 1853 | `		return PH7_OK;` |
|      - | 1854 | `	}` |
|      - | 1855 | `	/* Extract the string to process */` |
|      7 | 1856 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 1857 | `	if( zIn == 0 ){` |
|    ! 0 | 1858 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1859 | `		return PH7_OK;` |
|      - | 1860 | `	}` |
|      7 | 1861 | `	zEnd = &zIn[nLen];` |
|      7 | 1862 | `	zCur = 0; /* cc warning */` |
|      - | 1863 | `	/* Encode the string */` |
|      4 | 1864 | `	for(;;){` |
|      9 | 1865 | `		if( zIn >= zEnd ){` |
|      - | 1866 | `			/* No more input */` |
|      5 | 1867 | `			break;` |
|      - | 1868 | `		}` |
|      5 | 1869 | `		zCur = zIn;` |
|     17 | 1870 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 | 1871 | `			zIn++;` |
|      1 | 1872 | `		}` |
|      5 | 1873 | `		if( zIn > zCur ){` |
|      - | 1874 | `			/* Append raw contents */` |
|      5 | 1875 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 1876 | `		}` |
|      5 | 1877 | `		if( &zIn[1] < zEnd ){` |
|      3 | 1878 | `			int c = zIn[1];` |
|      3 | 1879 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - | 1880 | `				/* Ignore the backslash */` |
|      3 | 1881 | `				zIn++;` |
|      1 | 1882 | `			}` |
|      2 | 1883 | `		}else{` |
|      3 | 1884 | `			break;` |
|      - | 1885 | `		}` |
|      1 | 1886 | `	}` |
|      7 | 1887 | `	return PH7_OK;` |
|      5 | 1888 |  |
|      - | 1889 | `/*` |
|      - | 1890 | ` * string htmlspecialchars(string $string [, int $flags = ENT_COMPAT \| ENT_HTML401 [, string $charset]])` |
|      - | 1891 | ` *  HTML escaping of special characters.` |
|      - | 1892 | ` *  The translations performed are:` |
|      - | 1893 | ` *   '&' (ampersand) ==> '&amp;'` |
|      - | 1894 | ` *   '"' (double quote) ==> '&quot;' when ENT_NOQUOTES is not set.` |
|      - | 1895 | ` *   "'" (single quote) ==> '&#039;' only when ENT_QUOTES is set.` |
|      - | 1896 | ` *   '<' (less than) ==> '&lt;'` |
|      - | 1897 | ` *   '>' (greater than) ==> '&gt;'` |
|      - | 1898 | ` * Parameters` |
|      - | 1899 | ` *  $string` |
|      - | 1900 | ` *   The string being converted.` |
|      - | 1901 | ` * $flags` |
|      - | 1902 | ` *   A bitmask of one or more of the following flags, which specify how to handle quotes.` |
|      - | 1903 | ` *   The default is ENT_COMPAT \| ENT_HTML401.` |
|      - | 1904 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone.` |
|      - | 1905 | ` *   ENT_QUOTES 	Will convert both double and single quotes.` |
|      - | 1906 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted.` |
|      - | 1907 | ` *   ENT_IGNORE 	Silently discard invalid code unit sequences instead of returning an empty string.` |
|      - | 1908 | ` * $charset` |
|      - | 1909 | ` *  Defines character set used in conversion. The default character set is ISO-8859-1. (Not used)` |
|      - | 1910 | ` * Return` |
|      - | 1911 | ` *  The escaped string or NULL on failure.` |
|      - | 1912 | ` */` |
|     20 | 1913 | `static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1914 |  |
|      - | 1915 | `	const char *zCur,*zIn,*zEnd;` |
|     21 | 1916 | `	int iFlags = 0x01\|0x40; /* ENT_COMPAT \| ENT_HTML401 */` |
|      - | 1917 | `	int nLen,c;` |
|     21 | 1918 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1919 | `		/* Missing/Invalid arguments,return NULL */` |
|      9 | 1920 | `		ph7_result_null(pCtx);` |
|      9 | 1921 | `		return PH7_OK;` |
|      - | 1922 | `	}` |
|      - | 1923 | `	/* Extract the target string */` |
|     13 | 1924 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1925 | `	/* Return early when the input is empty, mirroring PHP's behavior. */` |
|     13 | 1926 | `	if( nLen == 0 ){` |
|      3 | 1927 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1928 | `		return PH7_OK;` |
|      - | 1929 | `	}` |
|     11 | 1930 | `	zEnd = &zIn[nLen];` |
|      - | 1931 | `	/* Extract the flags if available */` |
|     11 | 1932 | `	if( nArg > 1 ){` |
|      9 | 1933 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 | 1934 | `		if( iFlags < 0 ){` |
|      3 | 1935 | `			iFlags = 0x01\|0x40;` |
|      1 | 1936 | `		}` |
|      4 | 1937 | `	}` |
|      - | 1938 | `	/* Perform the requested operation */` |
|     23 | 1939 | `	for(;;){` |
|     47 | 1940 | `		if( zIn >= zEnd ){` |
|      9 | 1941 | `			break;` |
|      - | 1942 | `		}` |
|     39 | 1943 | `		zCur = zIn;` |
|     83 | 1944 | `		while( zIn < zEnd && zIn[0] != '&' && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '<' && zIn[0] != '>' ){` |
|     45 | 1945 | `			zIn++;` |
|      1 | 1946 | `		}` |
|     39 | 1947 | `		if( zCur < zIn ){` |
|      - | 1948 | `			/* Append the raw string verbatim */` |
|     17 | 1949 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      8 | 1950 | `		}` |
|     39 | 1951 | `		if( zIn >= zEnd ){` |
|      3 | 1952 | `			break;` |
|      - | 1953 | `		}` |
|     37 | 1954 | `		c = zIn[0];` |
|     37 | 1955 | `		if( c == '&' ){` |
|      - | 1956 | `			/* Expand '&amp;' */` |
|      9 | 1957 | `			ph7_result_string(pCtx,"&amp;",(int)sizeof("&amp;")-1);` |
|     33 | 1958 | `		}else if( c == '<' ){` |
|      - | 1959 | `			/* Expand '&lt;' */` |
|      7 | 1960 | `			ph7_result_string(pCtx,"&lt;",(int)sizeof("&lt;")-1);` |
|     26 | 1961 | `		}else if( c == '>' ){` |
|      - | 1962 | `			/* Expand '&gt;' */` |
|      9 | 1963 | `			ph7_result_string(pCtx,"&gt;",(int)sizeof("&gt;")-1);` |
|     19 | 1964 | `		}else if( c == '\'' ){` |
|      5 | 1965 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 1966 | `				/* Expand '&#039;' */` |
|      5 | 1967 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      3 | 1968 | `			}else{` |
|      - | 1969 | `				/* Leave the single quote untouched */` |
|    ! 0 | 1970 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      1 | 1971 | `			}` |
|     13 | 1972 | `		}else if( c == '"' ){` |
|     11 | 1973 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      - | 1974 | `				/* Expand '&quot;' */` |
|      7 | 1975 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      4 | 1976 | `			}else{` |
|      - | 1977 | `				/* Leave the double quote untouched */` |
|      5 | 1978 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      - | 1979 | `			}` |
|      5 | 1980 | `		}` |
|      - | 1981 | `		/* Ignore the unsafe HTML character */` |
|     37 | 1982 | `		zIn++;` |
|      1 | 1983 | `	}` |
|     11 | 1984 | `	return PH7_OK;` |
|     11 | 1985 |  |
|      - | 1986 | `/*` |
|      - | 1987 | ` * string htmlspecialchars_decode(string $string[,int $quote_style = ENT_COMPAT ])` |
|      - | 1988 | ` *  Unescape HTML entities.` |
|      - | 1989 | ` * Parameters` |
|      - | 1990 | ` *  $string` |
|      - | 1991 | ` *   The string to decode` |
|      - | 1992 | ` *  $quote_style` |
|      - | 1993 | ` *    The quote style. One of the following constants:` |
|      - | 1994 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone (default)` |
|      - | 1995 | ` *   ENT_QUOTES 	Will convert both double and single quotes` |
|      - | 1996 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted` |
|      - | 1997 | ` * Return` |
|      - | 1998 | ` *  The unescaped string or NULL on failure.` |
|      - | 1999 | ` */` |
|     16 | 2000 | `static int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2001 |  |
|      - | 2002 | `	const char *zCur,*zIn,*zEnd;` |
|     17 | 2003 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 2004 | `	int nLen,nJump;` |
|     17 | 2005 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2006 | `		/* Missing/Invalid arguments,return NULL */` |
|      7 | 2007 | `		ph7_result_null(pCtx);` |
|      7 | 2008 | `		return PH7_OK;` |
|      - | 2009 | `	}` |
|      - | 2010 | `	/* Extract the target string */` |
|     11 | 2011 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 2012 | `	zEnd = &zIn[nLen];` |
|      - | 2013 | `	/* Extract the flags if available */` |
|     11 | 2014 | `	if( nArg > 1 ){` |
|      7 | 2015 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      7 | 2016 | `		if( iFlags < 0 ){` |
|      3 | 2017 | `			iFlags = 0x01;` |
|      1 | 2018 | `		}` |
|      3 | 2019 | `	}` |
|      - | 2020 | `	/* Perform the requested operation */` |
|     15 | 2021 | `	for(;;){` |
|     31 | 2022 | `		if( zIn >= zEnd ){` |
|     11 | 2023 | `			break;` |
|      - | 2024 | `		}` |
|     21 | 2025 | `		zCur = zIn;` |
|     51 | 2026 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|     31 | 2027 | `			zIn++;` |
|      1 | 2028 | `		}` |
|     21 | 2029 | `		if( zCur < zIn ){` |
|      - | 2030 | `			/* Append the raw string verbatim */` |
|      9 | 2031 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 2032 | `		}` |
|     21 | 2033 | `		nLen = (int)(zEnd-zIn);` |
|     21 | 2034 | `		nJump = (int)sizeof(char);` |
|     21 | 2035 | `		if( nLen >= (int)sizeof("&amp;")-1 && SyStrnicmp(zIn,"&amp;",sizeof("&amp;")-1) == 0 ){` |
|      - | 2036 | `			/* &amp; ==> '&' */` |
|      3 | 2037 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|      3 | 2038 | `			nJump = (int)sizeof("&amp;")-1;` |
|     20 | 2039 | `		}else if( nLen >= (int)sizeof("&lt;")-1 && SyStrnicmp(zIn,"&lt;",sizeof("&lt;")-1) == 0 ){` |
|      - | 2040 | `			/* &lt; ==> < */` |
|      3 | 2041 | `			ph7_result_string(pCtx,"<",(int)sizeof(char));` |
|      3 | 2042 | `			nJump = (int)sizeof("&lt;")-1;` |
|     18 | 2043 | `		}else if( nLen >= (int)sizeof("&gt;")-1 && SyStrnicmp(zIn,"&gt;",sizeof("&gt;")-1) == 0 ){` |
|      - | 2044 | `			/* &gt; ==> '>' */` |
|      3 | 2045 | `			ph7_result_string(pCtx,">",(int)sizeof(char));` |
|      3 | 2046 | `			nJump = (int)sizeof("&gt;")-1;` |
|     16 | 2047 | `		}else if( nLen >= (int)sizeof("&quot;")-1 && SyStrnicmp(zIn,"&quot;",sizeof("&quot;")-1) == 0 ){` |
|      - | 2048 | `			/* &quot; ==> '"' */` |
|     13 | 2049 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      9 | 2050 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      5 | 2051 | `			}else{` |
|      - | 2052 | `				/* Leave untouched */` |
|      5 | 2053 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      - | 2054 | `			}` |
|     13 | 2055 | `			nJump = (int)sizeof("&quot;")-1;` |
|      9 | 2056 | `		}else if( nLen >= (int)sizeof("&#039;")-1 && SyStrnicmp(zIn,"&#039;",sizeof("&#039;")-1) == 0 ){` |
|      - | 2057 | `			/* &#039; ==> ''' */` |
|      3 | 2058 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 2059 | `				/* Expand ''' */` |
|      3 | 2060 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      2 | 2061 | `			}else{` |
|      - | 2062 | `				/* Leave untouched */` |
|    ! 0 | 2063 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      - | 2064 | `			}` |
|      3 | 2065 | `			nJump = (int)sizeof("&#039;")-1;` |
|      1 | 2066 | `		}else if( nLen >= (int)sizeof(char) ){` |
|      - | 2067 | `			/* expand '&' */` |
|    ! 0 | 2068 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 2069 | `		}else{` |
|      - | 2070 | `			/* No more input to process */` |
|    ! 0 | 2071 | `			break;` |
|      - | 2072 | `		}` |
|     21 | 2073 | `		zIn += nJump;` |
|      1 | 2074 | `	}` |
|     11 | 2075 | `	return PH7_OK;` |
|      9 | 2076 |  |
|      - | 2077 | `/* HTML encoding/Decoding table` |
|      - | 2078 | ` * Source: Symisc RunTime API.[chm@symisc.net]` |
|      - | 2079 | ` */` |
|      - | 2080 | `static const char *azHtmlEscape[] = {` |
|      - | 2081 | ` 	"&lt;","<","&gt;",">","&amp;","&","&quot;","\"","&#39;","'",` |
|      - | 2082 | `	"&#33;","!","&#36;","$","&#35;","#","&#37;","%","&#40;","(",` |
|      - | 2083 | `	"&#41;",")","&#123;","{","&#125;","}","&#61;","=","&#43;","+",` |
|      - | 2084 | `	"&#63;","?","&#91;","[","&#93;","]","&#64;","@","&#44;",","` |
|      - | 2085 | ` };` |
|      - | 2086 | `/*` |
|      - | 2087 | ` * array get_html_translation_table(void)` |
|      - | 2088 | ` *  Returns the translation table used by htmlspecialchars() and htmlentities().` |
|      - | 2089 | ` * Parameters` |
|      - | 2090 | ` *  None` |
|      - | 2091 | ` * Return` |
|      - | 2092 | ` *  The translation table as an array or NULL on failure.` |
|      - | 2093 | ` */` |
|      4 | 2094 | `static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2095 |  |
|      - | 2096 | `	ph7_value *pArray,*pValue;` |
|      - | 2097 | `	sxu32 n;` |
|      - | 2098 | `	/* Element value */` |
|      5 | 2099 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      5 | 2100 | `	if( pValue == 0 ){` |
|    ! 0 | 2101 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2102 | `		SXUNUSED(apArg);` |
|      - | 2103 | `		/* Return NULL */` |
|    ! 0 | 2104 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2105 | `		return PH7_OK;` |
|      - | 2106 | `	}` |
|      - | 2107 | `	/* Create a new array */` |
|      5 | 2108 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 | 2109 | `	if( pArray == 0 ){` |
|      - | 2110 | `		/* Return NULL */` |
|    ! 0 | 2111 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2112 | `		return PH7_OK;` |
|      - | 2113 | `	}` |
|      - | 2114 | `	/* Make the table */` |
|     85 | 2115 | `	for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|      - | 2116 | `		/* Prepare the value */` |
|     81 | 2117 | `		ph7_value_string(pValue,azHtmlEscape[n],-1 /* Compute length automatically */);` |
|      - | 2118 | `		/* Insert the value */` |
|     81 | 2119 | `		ph7_array_add_strkey_elem(pArray,azHtmlEscape[n+1],pValue);` |
|      - | 2120 | `		/* Reset the string cursor */` |
|     81 | 2121 | `		ph7_value_reset_string_cursor(pValue);` |
|     41 | 2122 | `	}` |
|      - | 2123 | `	/*` |
|      - | 2124 | `	 * Return the array.` |
|      - | 2125 | `	 * Don't worry about freeing memory, everything will be automatically` |
|      - | 2126 | `	 * released upon we return from this function.` |
|      - | 2127 | `	 */` |
|      5 | 2128 | `	ph7_result_value(pCtx,pArray);` |
|      5 | 2129 | `	return PH7_OK;` |
|      3 | 2130 |  |
|      - | 2131 | `/*` |
|      - | 2132 | ` * string htmlentities( string $string [, int $flags = ENT_COMPAT \| ENT_HTML401]);` |
|      - | 2133 | ` *   Convert all applicable characters to HTML entities` |
|      - | 2134 | ` * Parameters` |
|      - | 2135 | ` * $string` |
|      - | 2136 | ` *   The input string.` |
|      - | 2137 | ` * $flags` |
|      - | 2138 | ` *  A bitmask of one or more of the flags (see block-comment on PH7_builtin_htmlspecialchars())` |
|      - | 2139 | ` * Return` |
|      - | 2140 | ` * The encoded string.` |
|      - | 2141 | ` */` |
|     10 | 2142 | `static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2143 |  |
|     11 | 2144 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 2145 | `	const char *zIn,*zEnd;` |
|      - | 2146 | `	int nLen,c;` |
|      - | 2147 | `	sxu32 n;` |
|     11 | 2148 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2149 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 2150 | `		ph7_result_null(pCtx);` |
|      5 | 2151 | `		return PH7_OK;` |
|      - | 2152 | `	}` |
|      - | 2153 | `	/* Extract the target string */` |
|      7 | 2154 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 2155 | `	/* Handle empty string up front */` |
|      7 | 2156 | `	if( nLen == 0 ){` |
|      3 | 2157 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2158 | `		return PH7_OK;` |
|      - | 2159 | `	}` |
|      5 | 2160 | `	zEnd = &zIn[nLen];` |
|      - | 2161 | `	/* Extract the flags if available */` |
|      5 | 2162 | `	if( nArg > 1 ){` |
|      3 | 2163 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      3 | 2164 | `		if( iFlags < 0 ){` |
|      3 | 2165 | `			iFlags = 0x01;` |
|      1 | 2166 | `		}` |
|      1 | 2167 | `	}` |
|      - | 2168 | `	/* Perform the requested operation */` |
|     11 | 2169 | `	for(;;){` |
|     23 | 2170 | `		if( zIn >= zEnd ){` |
|      - | 2171 | `			/* No more input to process */` |
|      5 | 2172 | `			break;` |
|      - | 2173 | `		}` |
|     19 | 2174 | `		c = zIn[0];` |
|      - | 2175 | `		/* Perform a linear lookup on the decoding table */` |
|    233 | 2176 | `		for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    223 | 2177 | `			if( azHtmlEscape[n+1][0] == c ){` |
|      - | 2178 | `				/* Got one */` |
|      9 | 2179 | `				break;` |
|      - | 2180 | `			}` |
|    108 | 2181 | `		}` |
|     19 | 2182 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|      - | 2183 | `			/* Output the safe sequence [i.e: '<' ==> '&lt;"] */` |
|      9 | 2184 | `			if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 2185 | `				/* Expand the double quote verbatim */` |
|    ! 0 | 2186 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      9 | 2187 | `			}else if(c == '\'' && ((iFlags & 0x02 /*ENT_QUOTES*/) == 0 \|\| (iFlags & 0x04) /*ENT_NOQUOTES*/) ){` |
|      - | 2188 | `				/* expand single quote verbatim */` |
|    ! 0 | 2189 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|    ! 0 | 2190 | `			}else{` |
|      9 | 2191 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1/*Compute length automatically */);` |
|      - | 2192 | `			}` |
|      5 | 2193 | `		}else{` |
|      - | 2194 | `			/* Output character verbatim */` |
|     11 | 2195 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2196 | `		}` |
|     19 | 2197 | `		zIn++;` |
|      1 | 2198 | `	}` |
|      5 | 2199 | `	return PH7_OK;` |
|      6 | 2200 |  |
|      - | 2201 | `/*` |
|      - | 2202 | ` * string html_entity_decode(string $string [, int $quote_style = ENT_COMPAT [, string $charset = 'UTF-8' ]])` |
|      - | 2203 | ` *   Perform the reverse operation of html_entity_decode().` |
|      - | 2204 | ` * Parameters` |
|      - | 2205 | ` * $string` |
|      - | 2206 | ` *   The input string.` |
|      - | 2207 | ` * $flags` |
|      - | 2208 | ` *  A bitmask of one or more of the flags (see comment on PH7_builtin_htmlspecialchars())` |
|      - | 2209 | ` * Return` |
|      - | 2210 | ` * The decoded string.` |
|      - | 2211 | ` */` |
|     28 | 2212 | `static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2213 |  |
|      - | 2214 | `	const char *zCur,*zIn,*zEnd;` |
|     29 | 2215 | `	int iFlags = 0x01; /* ENT_COMPAT  */` |
|      - | 2216 | `	int nLen;` |
|      - | 2217 | `	sxu32 n;` |
|     29 | 2218 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2219 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 2220 | `		ph7_result_null(pCtx);` |
|      5 | 2221 | `		return PH7_OK;` |
|      - | 2222 | `	}` |
|      - | 2223 | `	/* Extract the target string */` |
|     25 | 2224 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 2225 | `	zEnd = &zIn[nLen];` |
|      - | 2226 | `	/* Extract the flags if available */` |
|     25 | 2227 | `	if( nArg > 1 ){` |
|     15 | 2228 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     15 | 2229 | `		if( iFlags < 0 ){` |
|      3 | 2230 | `			iFlags = 0x01;` |
|      1 | 2231 | `		}` |
|      7 | 2232 | `	}` |
|      - | 2233 | `	/* Perform the requested operation */` |
|     27 | 2234 | `	for(;;){` |
|     55 | 2235 | `		if( zIn >= zEnd ){` |
|      - | 2236 | `			/* No more input to process */` |
|     13 | 2237 | `			break;` |
|      - | 2238 | `		}` |
|     43 | 2239 | `		zCur = zIn;` |
|    173 | 2240 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|    131 | 2241 | `			zIn++;` |
|      1 | 2242 | `		}` |
|     43 | 2243 | `		if( zCur < zIn ){` |
|      - | 2244 | `			/* Append raw string verbatim */` |
|     27 | 2245 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     13 | 2246 | `		}` |
|     43 | 2247 | `		if( zIn >= zEnd ){` |
|     13 | 2248 | `			break;` |
|      - | 2249 | `		}` |
|     31 | 2250 | `		nLen = (int)(zEnd-zIn);` |
|      - | 2251 | `		/* Find an encoded sequence */` |
|    113 | 2252 | `		for(n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    113 | 2253 | `			int iLen = (int)SyStrlen(azHtmlEscape[n]);` |
|    113 | 2254 | `			if( nLen >= iLen && SyStrnicmp(zIn,azHtmlEscape[n],(sxu32)iLen) == 0 ){` |
|      - | 2255 | `				/* Got one */` |
|     31 | 2256 | `				zIn += iLen;` |
|     31 | 2257 | `				break;` |
|      - | 2258 | `			}` |
|     42 | 2259 | `		}` |
|     31 | 2260 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|     31 | 2261 | `			int c = azHtmlEscape[n+1][0];` |
|      - | 2262 | `			/* Output the decoded character */` |
|     31 | 2263 | `			if( c == '\'' && ((iFlags & 0x02) == 0 /*ENT_QUOTES*/\|\| (iFlags & 0x04) /*ENT_NOQUOTES*/)  ){` |
|      - | 2264 | `				/* Do not process single quotes */` |
|      9 | 2265 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|     27 | 2266 | `			}else if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 2267 | `				/* Do not process double quotes */` |
|      5 | 2268 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|      3 | 2269 | `			}else{` |
|     19 | 2270 | `				ph7_result_string(pCtx,azHtmlEscape[n+1],-1); /* Compute length automatically */` |
|      - | 2271 | `			}` |
|     16 | 2272 | `		}else{` |
|      - | 2273 | `			/* Append '&' */` |
|    ! 0 | 2274 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 2275 | `			zIn++;` |
|      - | 2276 | `		}` |
|      1 | 2277 | `	}` |
|     25 | 2278 | `	return PH7_OK;` |
|     15 | 2279 |  |
|      - | 2280 | `/*` |
|      - | 2281 | ` * int strlen($string)` |
|      - | 2282 | ` *  return the length of the given string.` |
|      - | 2283 | ` * Parameter` |
|      - | 2284 | ` *  string: The string being measured for length.` |
|      - | 2285 | ` * Return` |
|      - | 2286 | ` *  length of the given string.` |
|      - | 2287 | ` */` |
|   1672 | 2288 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2289 |  |
|   1674 | 2290 | `	int iLen = 0;` |
|   1674 | 2291 | `	if( nArg > 0 ){` |
|   1672 | 2292 | `		ph7_value_to_string(apArg[0],&iLen);` |
|    835 | 2293 | `	}` |
|      - | 2294 | `	/* String length */` |
|   1674 | 2295 | `	ph7_result_int(pCtx,iLen);` |
|   1674 | 2296 | `	return PH7_OK;` |
|      2 | 2297 |  |
|      - | 2298 | `/*` |
|      - | 2299 | ` * int strcmp(string $str1,string $str2)` |
|      - | 2300 | ` *  Perform a binary safe string comparison.` |
|      - | 2301 | ` * Parameter` |
|      - | 2302 | ` *  str1: The first string` |
|      - | 2303 | ` *  str2: The second string` |
|      - | 2304 | ` * Return` |
|      - | 2305 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2306 | ` *  than str2, and 0 if they are equal.` |
|      - | 2307 | ` */` |
|     50 | 2308 | `static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2309 |  |
|      - | 2310 | `	const char *z1,*z2;` |
|      - | 2311 | `	int n1,n2;` |
|      - | 2312 | `	int res;` |
|     51 | 2313 | `	if( nArg < 2 ){` |
|      5 | 2314 | `		res = nArg == 0 ? 0 : 1;` |
|      5 | 2315 | `		ph7_result_int(pCtx,res);` |
|      5 | 2316 | `		return PH7_OK;` |
|      - | 2317 | `	}` |
|      - | 2318 | `	/* Perform the comparison */` |
|     47 | 2319 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     47 | 2320 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     47 | 2321 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2322 | `	/* Comparison result */` |
|     47 | 2323 | `	ph7_result_int(pCtx,res);` |
|     47 | 2324 | `	return PH7_OK;` |
|     26 | 2325 |  |
|      - | 2326 | `/*` |
|      - | 2327 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 2328 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 2329 | ` * Parameter` |
|      - | 2330 | ` *  str1: The first string` |
|      - | 2331 | ` *  str2: The second string` |
|      - | 2332 | ` * Return` |
|      - | 2333 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2334 | ` *  than str2, and 0 if they are equal.` |
|      - | 2335 | ` */` |
|     20 | 2336 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2337 |  |
|      - | 2338 | `	const char *z1,*z2;` |
|      - | 2339 | `	int res;` |
|      - | 2340 | `	int n;` |
|     21 | 2341 | `	if( nArg < 3 ){` |
|      - | 2342 | `		/* Perform a standard comparison */` |
|      5 | 2343 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 2344 | `	}` |
|      - | 2345 | `	/* Desired comparison length */` |
|     17 | 2346 | `	n  = ph7_value_to_int(apArg[2]);` |
|     17 | 2347 | `	if( n < 0 ){` |
|      - | 2348 | `		/* Invalid length */` |
|      3 | 2349 | `		ph7_result_int(pCtx,-1);` |
|      3 | 2350 | `		return PH7_OK;` |
|      - | 2351 | `	}` |
|      - | 2352 | `	/* Perform the comparison */` |
|     15 | 2353 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     15 | 2354 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     15 | 2355 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 2356 | `	/* Comparison result */` |
|     15 | 2357 | `	ph7_result_int(pCtx,res);` |
|     15 | 2358 | `	return PH7_OK;` |
|     11 | 2359 |  |
|      - | 2360 | `/*` |
|      - | 2361 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 2362 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 2363 | ` * Parameter` |
|      - | 2364 | ` *  str1: The first string` |
|      - | 2365 | ` *  str2: The second string` |
|      - | 2366 | ` * Return` |
|      - | 2367 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2368 | ` *  than str2, and 0 if they are equal.` |
|      - | 2369 | ` */` |
|     18 | 2370 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2371 |  |
|      - | 2372 | `	const char *z1,*z2;` |
|      - | 2373 | `	int n1,n2;` |
|      - | 2374 | `	int res;` |
|     19 | 2375 | `	if( nArg < 2 ){` |
|      9 | 2376 | `		res = nArg == 0 ? 0 : 1;` |
|      9 | 2377 | `		ph7_result_int(pCtx,res);` |
|      9 | 2378 | `		return PH7_OK;` |
|      - | 2379 | `	}` |
|      - | 2380 | `	/* Perform the comparison */` |
|     11 | 2381 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     11 | 2382 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     11 | 2383 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2384 | `	/* Comparison result */` |
|     11 | 2385 | `	ph7_result_int(pCtx,res);` |
|     11 | 2386 | `	return PH7_OK;` |
|     10 | 2387 |  |
|      - | 2388 | `/*` |
|      - | 2389 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 2390 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 2391 | ` * Parameter` |
|      - | 2392 | ` *  $str1: The first string` |
|      - | 2393 | ` *  $str2: The second string` |
|      - | 2394 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 2395 | ` * Return` |
|      - | 2396 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2397 | ` *  than str2, and 0 if they are equal.` |
|      - | 2398 | ` */` |
|      8 | 2399 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2400 |  |
|      - | 2401 | `	const char *z1,*z2;` |
|      - | 2402 | `	int res;` |
|      - | 2403 | `	int n;` |
|      9 | 2404 | `	if( nArg < 3 ){` |
|      - | 2405 | `		/* Perform a standard comparison */` |
|      5 | 2406 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 2407 | `	}` |
|      - | 2408 | `	/* Desired comparison length */` |
|      5 | 2409 | `	n  = ph7_value_to_int(apArg[2]);` |
|      5 | 2410 | `	if( n < 0 ){` |
|      - | 2411 | `		/* Invalid length */` |
|    ! 0 | 2412 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2413 | `		return PH7_OK;` |
|      - | 2414 | `	}` |
|      - | 2415 | `	/* Perform the comparison */` |
|      5 | 2416 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|      5 | 2417 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|      5 | 2418 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 2419 | `	/* Comparison result */` |
|      5 | 2420 | `	ph7_result_int(pCtx,res);` |
|      5 | 2421 | `	return PH7_OK;` |
|      5 | 2422 |  |
|      - | 2423 | `/*` |
|      - | 2424 | ` * Implode context [i.e: it's private data].` |
|      - | 2425 | ` * A pointer to the following structure is forwarded` |
|      - | 2426 | ` * verbatim to the array walker callback defined below.` |
|      - | 2427 | ` */` |
|      - | 2428 | `struct implode_data {` |
|      - | 2429 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 2430 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 2431 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 2432 | `	int nSeplen;          /* Separator length */` |
|      - | 2433 | `	int bFirst;           /* TRUE if first call */` |
|      - | 2434 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 2435 | `};` |
|      - | 2436 | `/*` |
|      - | 2437 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 2438 | ` * The following routine is invoked for each array entry passed` |
|      - | 2439 | ` * to the implode() function.` |
|      - | 2440 | ` */` |
|  80280 | 2441 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      2 | 2442 |  |
|  40140 | 2443 | `	SXUNUSED(pKey);` |
|  80282 | 2444 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 2445 | `	const char *zData;` |
|      - | 2446 | `	int nLen;` |
|  80282 | 2447 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 2448 | `		if( pData->nSeplen > 0 ){` |
|      3 | 2449 | `			if( !pData->bFirst ){` |
|      - | 2450 | `				/* append the separator first */` |
|      3 | 2451 | `				ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|      2 | 2452 | `			}else{` |
|    ! 0 | 2453 | `				pData->bFirst = 0;` |
|      - | 2454 | `			}` |
|      1 | 2455 | `		}` |
|      - | 2456 | `		/* Recurse */` |
|      3 | 2457 | `		pData->bFirst = 1;` |
|      3 | 2458 | `		pData->nRecCount++;` |
|      3 | 2459 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 2460 | `		pData->nRecCount--;` |
|      3 | 2461 | `		return PH7_OK;` |
|      - | 2462 | `	}` |
|      - | 2463 | `	/* Extract the string representation of the entry value */` |
|  80280 | 2464 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 2465 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
|  80280 | 2466 | `	if( pData->bFirst ){` |
|  17680 | 2467 | `		pData->bFirst = 0;` |
|  71441 | 2468 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 2469 | `		/* append the separator first */` |
|  62590 | 2470 | `		ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|  31294 | 2471 | `	}` |
|      - | 2472 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
|  80280 | 2473 | `	if( nLen > 0 ){` |
|  73158 | 2474 | `		ph7_result_string(pData->pCtx,zData,nLen);` |
|  36578 | 2475 | `	}` |
|  80280 | 2476 | `	return PH7_OK;` |
|  40142 | 2477 |  |
|      - | 2478 | `/*` |
|      - | 2479 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 2480 | ` * string implode(array $pieces,...)` |
|      - | 2481 | ` *  Join array elements with a string.` |
|      - | 2482 | ` * $glue` |
|      - | 2483 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 2484 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 2485 | ` * $pieces` |
|      - | 2486 | ` *   The array of strings to implode.` |
|      - | 2487 | ` * Return` |
|      - | 2488 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 2489 | ` *  order, with the glue string between each element.` |
|      - | 2490 | ` */` |
|  17706 | 2491 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2492 |  |
|      - | 2493 | `	struct implode_data imp_data;` |
|  17708 | 2494 | `	int i = 1;` |
|  17708 | 2495 | `	if( nArg < 1 ){` |
|      - | 2496 | `		/* Missing argument,return NULL */` |
|    ! 0 | 2497 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2498 | `		return PH7_OK;` |
|      - | 2499 | `	}` |
|      - | 2500 | `	/* Prepare the implode context */` |
|  17708 | 2501 | `	imp_data.pCtx = pCtx;` |
|  17708 | 2502 | `	imp_data.bRecursive = 0;` |
|  17708 | 2503 | `	imp_data.bFirst = 1;` |
|  17708 | 2504 | `	imp_data.nRecCount = 0;` |
|  17708 | 2505 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  17706 | 2506 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|   8854 | 2507 | `	}else{` |
|      3 | 2508 | `		imp_data.zSep = 0;` |
|      3 | 2509 | `		imp_data.nSeplen = 0;` |
|      3 | 2510 | `		i = 0;` |
|      - | 2511 | `	}` |
|  17708 | 2512 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 2513 | `	/* Start the 'join' process */` |
|  35414 | 2514 | `	while( i < nArg ){` |
|  17708 | 2515 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2516 | `			/* Iterate throw array entries */` |
|  17708 | 2517 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|   8855 | 2518 | `		}else{` |
|      - | 2519 | `			const char *zData;` |
|      - | 2520 | `			int nLen;` |
|      - | 2521 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 2522 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2523 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 2524 | `			if( imp_data.bFirst ){` |
|    ! 0 | 2525 | `				imp_data.bFirst = 0;` |
|    ! 0 | 2526 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2527 | `				ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen);` |
|    ! 0 | 2528 | `			}` |
|      - | 2529 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 2530 | `			if( nLen > 0 ){` |
|    ! 0 | 2531 | `				ph7_result_string(pCtx,zData,nLen);` |
|    ! 0 | 2532 | `			}` |
|      - | 2533 | `		}` |
|  17708 | 2534 | `		i++;` |
|      2 | 2535 | `	}` |
|  17708 | 2536 | `	return PH7_OK;` |
|   8855 | 2537 |  |
|      - | 2538 | `/*` |
|      - | 2539 | ` * Symisc eXtension:` |
|      - | 2540 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 2541 | ` * Purpose` |
|      - | 2542 | ` *  Same as implode() but recurse on arrays.` |
|      - | 2543 | ` * Example:` |
|      - | 2544 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 2545 | ` *   echo implode_recursive("/",$a);` |
|      - | 2546 | ` *   Will output` |
|      - | 2547 | ` *     usr/home/dean.` |
|      - | 2548 | ` *   While the standard implode would produce.` |
|      - | 2549 | ` *    usr/Array.` |
|      - | 2550 | ` * Parameter` |
|      - | 2551 | ` *  Refer to implode().` |
|      - | 2552 | ` * Return` |
|      - | 2553 | ` *  Refer to implode().` |
|      - | 2554 | ` */` |
|     12 | 2555 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2556 |  |
|      - | 2557 | `	struct implode_data imp_data;` |
|     13 | 2558 | `	int i = 1;` |
|     13 | 2559 | `	if( nArg < 1 ){` |
|      - | 2560 | `		/* Missing argument,return NULL */` |
|      3 | 2561 | `		ph7_result_null(pCtx);` |
|      3 | 2562 | `		return PH7_OK;` |
|      - | 2563 | `	}` |
|      - | 2564 | `	/* Prepare the implode context */` |
|     11 | 2565 | `	imp_data.pCtx = pCtx;` |
|     11 | 2566 | `	imp_data.bRecursive = 1;` |
|     11 | 2567 | `	imp_data.bFirst = 1;` |
|     11 | 2568 | `	imp_data.nRecCount = 0;` |
|     11 | 2569 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 2570 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 2571 | `	}else{` |
|    ! 0 | 2572 | `		imp_data.zSep = 0;` |
|    ! 0 | 2573 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 2574 | `		i = 0;` |
|      - | 2575 | `	}` |
|     11 | 2576 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 2577 | `	/* Start the 'join' process */` |
|     21 | 2578 | `	while( i < nArg ){` |
|     11 | 2579 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2580 | `			/* Iterate throw array entries */` |
|      3 | 2581 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      2 | 2582 | `		}else{` |
|      - | 2583 | `			const char *zData;` |
|      - | 2584 | `			int nLen;` |
|      - | 2585 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 2586 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2587 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 2588 | `			if( imp_data.bFirst ){` |
|      9 | 2589 | `				imp_data.bFirst = 0;` |
|      4 | 2590 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2591 | `				ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen);` |
|    ! 0 | 2592 | `			}` |
|      - | 2593 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 2594 | `			if( nLen > 0 ){` |
|      9 | 2595 | `				ph7_result_string(pCtx,zData,nLen);` |
|      4 | 2596 | `			}` |
|      - | 2597 | `		}` |
|     11 | 2598 | `		i++;` |
|      1 | 2599 | `	}` |
|     11 | 2600 | `	return PH7_OK;` |
|      7 | 2601 |  |
|      - | 2602 | `/*` |
|      - | 2603 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 2604 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 2605 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 2606 | ` * Parameters` |
|      - | 2607 | ` *  $delimiter` |
|      - | 2608 | ` *   The boundary string.` |
|      - | 2609 | ` * $string` |
|      - | 2610 | ` *   The input string.` |
|      - | 2611 | ` * $limit` |
|      - | 2612 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 2613 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 2614 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 2615 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 2616 | ` * Returns` |
|      - | 2617 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 2618 | ` *  on boundaries formed by the delimiter.` |
|      - | 2619 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 2620 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 2621 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 2622 | ` *  will be returned.` |
|      - | 2623 | ` * NOTE:` |
|      - | 2624 | ` *  Negative limit is not supported.` |
|      - | 2625 | ` */` |
|   3212 | 2626 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2627 |  |
|      - | 2628 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 2629 | `	int nDelim,nStrlen,iLimit;` |
|      - | 2630 | `	ph7_value *pArray;` |
|      - | 2631 | `	ph7_value *pValue;` |
|      - | 2632 | `	sxu32 nOfft;` |
|      - | 2633 | `	sxi32 rc;` |
|   3214 | 2634 | `	if( nArg < 2 ){` |
|      - | 2635 | `		/* Missing arguments,return FALSE */` |
|      9 | 2636 | `		ph7_result_bool(pCtx,0);` |
|      9 | 2637 | `		return PH7_OK;` |
|      - | 2638 | `	}` |
|      - | 2639 | `	/* Extract the delimiter */` |
|   3206 | 2640 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   3206 | 2641 | `	if( nDelim < 1 ){` |
|      - | 2642 | `		/* Empty delimiter,return FALSE */` |
|      3 | 2643 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2644 | `		return PH7_OK;` |
|      - | 2645 | `	}` |
|      - | 2646 | `	/* Extract the string */` |
|   3204 | 2647 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   3204 | 2648 | `	if( nStrlen < 1 ){` |
|      - | 2649 | `		/* Empty string: return an array with a single empty element (PHP behavior) */` |
|      3 | 2650 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      3 | 2651 | `		ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      3 | 2652 | `		if( pArrayTmp == 0 \|\| pValueTmp == 0 ){` |
|      - | 2653 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 2654 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2655 | `			return PH7_OK;` |
|      - | 2656 | `		}` |
|      3 | 2657 | `		ph7_value_string(pValueTmp, "", 0);` |
|      3 | 2658 | `		ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp);` |
|      3 | 2659 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      3 | 2660 | `		return PH7_OK;` |
|      - | 2661 | `	}` |
|      - | 2662 | `	/* Point to the end of the string */` |
|   3202 | 2663 | `	zEnd = &zString[nStrlen];` |
|      - | 2664 | `	/* Create the array */` |
|   3202 | 2665 | `	pArray =  ph7_context_new_array(pCtx);` |
|   3202 | 2666 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   3202 | 2667 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2668 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 2669 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2670 | `		return PH7_OK;` |
|      - | 2671 | `	}` |
|      - | 2672 | `	/* Set a defualt limit */` |
|   3202 | 2673 | `	iLimit = SXI32_HIGH;` |
|   3202 | 2674 | `	if( nArg > 2 ){` |
|      9 | 2675 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|      9 | 2676 | `		 if( iLimit < 0 ){` |
|      3 | 2677 | `			iLimit = -iLimit;` |
|      1 | 2678 | `		}` |
|      9 | 2679 | `		if( iLimit == 0 ){` |
|      3 | 2680 | `			iLimit = 1;` |
|      1 | 2681 | `		}` |
|      9 | 2682 | `		iLimit--;` |
|      4 | 2683 | `	}` |
|      - | 2684 | `	/* Start exploding */` |
|  38739 | 2685 | `	for(;;){` |
|  77480 | 2686 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
|  77480 | 2687 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 2688 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   3202 | 2689 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   3202 | 2690 | `			ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|   3202 | 2691 | `			break;` |
|      - | 2692 | `		}` |
|      - | 2693 | `		/* Point to the desired offset */` |
|  74280 | 2694 | `		zCur = &zString[nOfft];` |
|      - | 2695 | `		/* Perform the store operation (may be empty) */` |
|  74280 | 2696 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|  74280 | 2697 | `		ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|      - | 2698 | `		/* Point beyond the delimiter */` |
|  74280 | 2699 | `		zString = &zCur[nDelim];` |
|      - | 2700 | `		/* Reset the cursor */` |
|  74280 | 2701 | `		ph7_value_reset_string_cursor(pValue);` |
|      2 | 2702 | `	}` |
|      - | 2703 | `	/* Return the freshly created array */` |
|   3202 | 2704 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2705 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 2706 | `	 * released as soon we return from this foregin function.` |
|      - | 2707 | `	 */` |
|   3202 | 2708 | `	return PH7_OK;` |
|   1608 | 2709 |  |
|      - | 2710 | `/*` |
|      - | 2711 | ` * string trim(string $str[,string $charlist ])` |
|      - | 2712 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2713 | ` * Parameters` |
|      - | 2714 | ` *  $str` |
|      - | 2715 | ` *   The string that will be trimmed.` |
|      - | 2716 | ` * $charlist` |
|      - | 2717 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2718 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2719 | ` *   With .. you can specify a range of characters.` |
|      - | 2720 | ` * Returns.` |
|      - | 2721 | ` *  Thr processed string.` |
|      - | 2722 | ` * NOTE:` |
|      - | 2723 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 2724 | ` */` |
|   8002 | 2725 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2726 |  |
|      - | 2727 | `	const char *zString;` |
|      - | 2728 | `	int nLen;` |
|   8004 | 2729 | `	if( nArg < 1 ){` |
|      - | 2730 | `		/* Missing arguments,return null */` |
|      3 | 2731 | `		ph7_result_null(pCtx);` |
|      3 | 2732 | `		return PH7_OK;` |
|      - | 2733 | `	}` |
|      - | 2734 | `	/* Extract the target string */` |
|   8002 | 2735 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|   8002 | 2736 | `	if( nLen < 1 ){` |
|      - | 2737 | `		/* Empty string,return */` |
|   1636 | 2738 | `		ph7_result_string(pCtx,"",0);` |
|   1636 | 2739 | `		return PH7_OK;` |
|      - | 2740 | `	}` |
|      - | 2741 | `	/* Start the trim process */` |
|   6368 | 2742 | `	if( nArg < 2 ){` |
|      - | 2743 | `		SyString sStr;` |
|      - | 2744 | `		/* Remove white spaces and NUL bytes */` |
|   6364 | 2745 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  15438 | 2746 | `		SyStringFullTrimSafe(&sStr);` |
|   6364 | 2747 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   3183 | 2748 | `	}else{` |
|      - | 2749 | `		/* Char list */` |
|      - | 2750 | `		const char *zList;` |
|      - | 2751 | `		int nListlen;` |
|      5 | 2752 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 2753 | `		if( nListlen < 1 ){` |
|      - | 2754 | `			/* Return the string unchanged */` |
|      3 | 2755 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2756 | `		}else{` |
|      3 | 2757 | `			const char *zEnd = &zString[nLen];` |
|      3 | 2758 | `			const char *zCur = zString;` |
|      - | 2759 | `			const char *zPtr;` |
|      - | 2760 | `			int i;` |
|      - | 2761 | `			/* Left trim */` |
|      4 | 2762 | `			for(;;){` |
|      9 | 2763 | `				if( zCur >= zEnd ){` |
|    ! 0 | 2764 | `					break;` |
|      - | 2765 | `				}` |
|      9 | 2766 | `				zPtr = zCur;` |
|     17 | 2767 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 2768 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|      7 | 2769 | `						zCur++;` |
|      3 | 2770 | `					}` |
|      5 | 2771 | `				}` |
|      9 | 2772 | `				if( zCur == zPtr ){` |
|      - | 2773 | `					/* No match,break immediately */` |
|      3 | 2774 | `					break;` |
|      - | 2775 | `				}` |
|      1 | 2776 | `			}` |
|      - | 2777 | `			/* Right trim */` |
|      3 | 2778 | `			zEnd--;` |
|      4 | 2779 | `			for(;;){` |
|      9 | 2780 | `				if( zEnd <= zCur ){` |
|    ! 0 | 2781 | `					break;` |
|      - | 2782 | `				}` |
|      9 | 2783 | `				zPtr = zEnd;` |
|     17 | 2784 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 2785 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      7 | 2786 | `						zEnd--;` |
|      3 | 2787 | `					}` |
|      5 | 2788 | `				}` |
|      9 | 2789 | `				if( zEnd == zPtr ){` |
|      3 | 2790 | `					break;` |
|      - | 2791 | `				}` |
|      1 | 2792 | `			}` |
|      3 | 2793 | `			if( zCur >= zEnd ){` |
|      - | 2794 | `				/* Return the empty string */` |
|    ! 0 | 2795 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2796 | `			}else{` |
|      3 | 2797 | `				zEnd++;` |
|      3 | 2798 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2799 | `			}` |
|      - | 2800 | `		}` |
|      - | 2801 | `	}` |
|   6368 | 2802 | `	return PH7_OK;` |
|   4003 | 2803 |  |
|      - | 2804 | `/*` |
|      - | 2805 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 2806 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 2807 | ` * Parameters` |
|      - | 2808 | ` *  $str` |
|      - | 2809 | ` *   The string that will be trimmed.` |
|      - | 2810 | ` * $charlist` |
|      - | 2811 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2812 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2813 | ` *   With .. you can specify a range of characters.` |
|      - | 2814 | ` * Returns.` |
|      - | 2815 | ` *  Thr processed string.` |
|      - | 2816 | ` * NOTE:` |
|      - | 2817 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 2818 | ` */` |
|     26 | 2819 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2820 |  |
|      - | 2821 | `	const char *zString;` |
|      - | 2822 | `	int nLen;` |
|     27 | 2823 | `	if( nArg < 1 ){` |
|      - | 2824 | `		/* Missing arguments,return null */` |
|      3 | 2825 | `		ph7_result_null(pCtx);` |
|      3 | 2826 | `		return PH7_OK;` |
|      - | 2827 | `	}` |
|      - | 2828 | `	/* Extract the target string */` |
|     25 | 2829 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 2830 | `	if( nLen < 1 ){` |
|      - | 2831 | `		/* Empty string,return */` |
|      5 | 2832 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2833 | `		return PH7_OK;` |
|      - | 2834 | `	}` |
|      - | 2835 | `	/* Start the trim process */` |
|     21 | 2836 | `	if( nArg < 2 ){` |
|      - | 2837 | `		SyString sStr;` |
|      - | 2838 | `		/* Remove white spaces and NUL bytes*/` |
|     17 | 2839 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     41 | 2840 | `		SyStringRightTrimSafe(&sStr);` |
|     17 | 2841 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      9 | 2842 | `	}else{` |
|      - | 2843 | `		/* Char list */` |
|      - | 2844 | `		const char *zList;` |
|      - | 2845 | `		int nListlen;` |
|      5 | 2846 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 2847 | `		if( nListlen < 1 ){` |
|      - | 2848 | `			/* Return the string unchanged */` |
|    ! 0 | 2849 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 2850 | `		}else{` |
|      5 | 2851 | `			const char *zEnd = &zString[nLen - 1];` |
|      5 | 2852 | `			const char *zCur = zString;` |
|      - | 2853 | `			const char *zPtr;` |
|      - | 2854 | `			int i;` |
|      - | 2855 | `			/* Right trim */` |
|      6 | 2856 | `			for(;;){` |
|     13 | 2857 | `				if( zEnd <= zCur ){` |
|    ! 0 | 2858 | `					break;` |
|      - | 2859 | `				}` |
|     13 | 2860 | `				zPtr = zEnd;` |
|     25 | 2861 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     13 | 2862 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      9 | 2863 | `						zEnd--;` |
|      4 | 2864 | `					}` |
|      7 | 2865 | `				}` |
|     13 | 2866 | `				if( zEnd == zPtr ){` |
|      5 | 2867 | `					break;` |
|      - | 2868 | `				}` |
|      1 | 2869 | `			}` |
|      5 | 2870 | `			if( zEnd <= zCur ){` |
|      - | 2871 | `				/* Return the empty string */` |
|    ! 0 | 2872 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2873 | `			}else{` |
|      5 | 2874 | `				zEnd++;` |
|      5 | 2875 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2876 | `			}` |
|      - | 2877 | `		}` |
|      - | 2878 | `	}` |
|     21 | 2879 | `	return PH7_OK;` |
|     14 | 2880 |  |
|      - | 2881 | `/*` |
|      - | 2882 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 2883 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2884 | ` * Parameters` |
|      - | 2885 | ` *  $str` |
|      - | 2886 | ` *   The string that will be trimmed.` |
|      - | 2887 | ` * $charlist` |
|      - | 2888 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2889 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2890 | ` *   With .. you can specify a range of characters.` |
|      - | 2891 | ` * Returns.` |
|      - | 2892 | ` *  Thr processed string.` |
|      - | 2893 | ` * NOTE:` |
|      - | 2894 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 2895 | ` */` |
|     12 | 2896 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2897 |  |
|      - | 2898 | `	const char *zString;` |
|      - | 2899 | `	int nLen;` |
|     13 | 2900 | `	if( nArg < 1 ){` |
|      - | 2901 | `		/* Missing arguments,return null */` |
|      3 | 2902 | `		ph7_result_null(pCtx);` |
|      3 | 2903 | `		return PH7_OK;` |
|      - | 2904 | `	}` |
|      - | 2905 | `	/* Extract the target string */` |
|     11 | 2906 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 2907 | `	if( nLen < 1 ){` |
|      - | 2908 | `		/* Empty string,return */` |
|    ! 0 | 2909 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2910 | `		return PH7_OK;` |
|      - | 2911 | `	}` |
|      - | 2912 | `	/* Start the trim process */` |
|     11 | 2913 | `	if( nArg < 2 ){` |
|      - | 2914 | `		SyString sStr;` |
|      - | 2915 | `		/* Remove white spaces and NUL byte */` |
|      3 | 2916 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|      8 | 2917 | `		SyStringLeftTrimSafe(&sStr);` |
|      3 | 2918 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      2 | 2919 | `	}else{` |
|      - | 2920 | `		/* Char list */` |
|      - | 2921 | `		const char *zList;` |
|      - | 2922 | `		int nListlen;` |
|      9 | 2923 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      9 | 2924 | `		if( nListlen < 1 ){` |
|      - | 2925 | `			/* Return the string unchanged */` |
|      3 | 2926 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2927 | `		}else{` |
|      7 | 2928 | `			const char *zEnd = &zString[nLen];` |
|      7 | 2929 | `			const char *zCur = zString;` |
|      - | 2930 | `			const char *zPtr;` |
|      - | 2931 | `			int i;` |
|      - | 2932 | `			/* Left trim */` |
|      7 | 2933 | `			for(;;){` |
|     15 | 2934 | `				if( zCur >= zEnd ){` |
|    ! 0 | 2935 | `					break;` |
|      - | 2936 | `				}` |
|     15 | 2937 | `				zPtr = zCur;` |
|     41 | 2938 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     27 | 2939 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|     13 | 2940 | `						zCur++;` |
|      6 | 2941 | `					}` |
|     14 | 2942 | `				}` |
|     15 | 2943 | `				if( zCur == zPtr ){` |
|      - | 2944 | `					/* No match,break immediately */` |
|      7 | 2945 | `					break;` |
|      - | 2946 | `				}` |
|      1 | 2947 | `			}` |
|      7 | 2948 | `			if( zCur >= zEnd ){` |
|      - | 2949 | `				/* Return the empty string */` |
|    ! 0 | 2950 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2951 | `			}else{` |
|      7 | 2952 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2953 | `			}` |
|      - | 2954 | `		}` |
|      - | 2955 | `	}` |
|     11 | 2956 | `	return PH7_OK;` |
|      7 | 2957 |  |
|      - | 2958 | `/*` |
|      - | 2959 | ` * string strtolower(string $str)` |
|      - | 2960 | ` *  Make a string lowercase.` |
|      - | 2961 | ` * Parameters` |
|      - | 2962 | ` *  $str` |
|      - | 2963 | ` *   The input string.` |
|      - | 2964 | ` * Returns.` |
|      - | 2965 | ` *  The lowercased string.` |
|      - | 2966 | ` */` |
|  17544 | 2967 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2968 |  |
|      - | 2969 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2970 | `	int nLen;` |
|  17546 | 2971 | `	if( nArg < 1 ){` |
|      - | 2972 | `		/* Missing arguments,return null */` |
|      3 | 2973 | `		ph7_result_null(pCtx);` |
|      3 | 2974 | `		return PH7_OK;` |
|      - | 2975 | `	}` |
|      - | 2976 | `	/* Extract the target string */` |
|  17544 | 2977 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  17544 | 2978 | `	if( nLen < 1 ){` |
|      - | 2979 | `		/* Empty string,return */` |
|      3 | 2980 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2981 | `		return PH7_OK;` |
|      - | 2982 | `	}` |
|      - | 2983 | `	/* Perform the requested operation */` |
|  17542 | 2984 | `	zEnd = &zString[nLen];` |
|  55458 | 2985 | `	for(;;){` |
| 110918 | 2986 | `		if( zString >= zEnd ){` |
|      - | 2987 | `			/* No more input,break immediately */` |
|  17542 | 2988 | `			break;` |
|      - | 2989 | `		}` |
|  93378 | 2990 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2991 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2992 | `			zCur = zString;` |
|    ! 0 | 2993 | `			zString++;` |
|    ! 0 | 2994 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2995 | `				zString++;` |
|    ! 0 | 2996 | `			}` |
|      - | 2997 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2998 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2999 | `		}else{` |
|  93378 | 3000 | `			int c = zString[0];` |
|  93378 | 3001 | `			if( SyisUpper(c) ){` |
|  93376 | 3002 | `				c = SyToLower(zString[0]);` |
|  46687 | 3003 | `			}` |
|      - | 3004 | `			/* Append character */` |
|  93378 | 3005 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 3006 | `			/* Advance the cursor */` |
|  93378 | 3007 | `			zString++;` |
|      - | 3008 | `		}` |
|      2 | 3009 | `	}` |
|  17542 | 3010 | `	return PH7_OK;` |
|   8774 | 3011 |  |
|      - | 3012 | `/*` |
|      - | 3013 | ` * string strtolower(string $str)` |
|      - | 3014 | ` *  Make a string uppercase.` |
|      - | 3015 | ` * Parameters` |
|      - | 3016 | ` *  $str` |
|      - | 3017 | ` *   The input string.` |
|      - | 3018 | ` * Returns.` |
|      - | 3019 | ` *  The uppercased string.` |
|      - | 3020 | ` */` |
|     10 | 3021 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3022 |  |
|      - | 3023 | `	const char *zString,*zCur,*zEnd;` |
|      - | 3024 | `	int nLen;` |
|     11 | 3025 | `	if( nArg < 1 ){` |
|      - | 3026 | `		/* Missing arguments,return null */` |
|      3 | 3027 | `		ph7_result_null(pCtx);` |
|      3 | 3028 | `		return PH7_OK;` |
|      - | 3029 | `	}` |
|      - | 3030 | `	/* Extract the target string */` |
|      9 | 3031 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 3032 | `	if( nLen < 1 ){` |
|      - | 3033 | `		/* Empty string,return */` |
|      3 | 3034 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3035 | `		return PH7_OK;` |
|      - | 3036 | `	}` |
|      - | 3037 | `	/* Perform the requested operation */` |
|      7 | 3038 | `	zEnd = &zString[nLen];` |
|     19 | 3039 | `	for(;;){` |
|     39 | 3040 | `		if( zString >= zEnd ){` |
|      - | 3041 | `			/* No more input,break immediately */` |
|      7 | 3042 | `			break;` |
|      - | 3043 | `		}` |
|     33 | 3044 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 3045 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 3046 | `			zCur = zString;` |
|    ! 0 | 3047 | `			zString++;` |
|    ! 0 | 3048 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 3049 | `				zString++;` |
|    ! 0 | 3050 | `			}` |
|      - | 3051 | `			/* Append UTF-8 stream */` |
|    ! 0 | 3052 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 3053 | `		}else{` |
|     33 | 3054 | `			int c = zString[0];` |
|     33 | 3055 | `			if( SyisLower(c) ){` |
|     27 | 3056 | `				c = SyToUpper(zString[0]);` |
|     13 | 3057 | `			}` |
|      - | 3058 | `			/* Append character */` |
|     33 | 3059 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 3060 | `			/* Advance the cursor */` |
|     33 | 3061 | `			zString++;` |
|      - | 3062 | `		}` |
|      1 | 3063 | `	}` |
|      7 | 3064 | `	return PH7_OK;` |
|      6 | 3065 |  |
|      - | 3066 | `/*` |
|      - | 3067 | ` * string ucfirst(string $str)` |
|      - | 3068 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 3069 | ` *  character is alphabetic.` |
|      - | 3070 | ` * Parameters` |
|      - | 3071 | ` *  $str` |
|      - | 3072 | ` *   The input string.` |
|      - | 3073 | ` * Returns.` |
|      - | 3074 | ` *  The processed string.` |
|      - | 3075 | ` */` |
|      6 | 3076 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3077 |  |
|      - | 3078 | `	const char *zString,*zEnd;` |
|      - | 3079 | `	int nLen,c;` |
|      7 | 3080 | `	if( nArg < 1 ){` |
|      - | 3081 | `		/* Missing arguments,return null */` |
|      3 | 3082 | `		ph7_result_null(pCtx);` |
|      3 | 3083 | `		return PH7_OK;` |
|      - | 3084 | `	}` |
|      - | 3085 | `	/* Extract the target string */` |
|      5 | 3086 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3087 | `	if( nLen < 1 ){` |
|      - | 3088 | `		/* Empty string,return */` |
|      3 | 3089 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3090 | `		return PH7_OK;` |
|      - | 3091 | `	}` |
|      - | 3092 | `	/* Perform the requested operation */` |
|      3 | 3093 | `	zEnd = &zString[nLen];` |
|      3 | 3094 | `	c = zString[0];` |
|      3 | 3095 | `	if( SyisLower(c) ){` |
|      3 | 3096 | `		c = SyToUpper(c);` |
|      1 | 3097 | `	}` |
|      - | 3098 | `	/* Append the first character */` |
|      3 | 3099 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 3100 | `	zString++;` |
|      3 | 3101 | `	if( zString < zEnd ){` |
|      - | 3102 | `		/* Append the rest of the input verbatim */` |
|      3 | 3103 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 3104 | `	}` |
|      3 | 3105 | `	return PH7_OK;` |
|      4 | 3106 |  |
|      - | 3107 | `/*` |
|      - | 3108 | ` * string lcfirst(string $str)` |
|      - | 3109 | ` *  Make a string's first character lowercase.` |
|      - | 3110 | ` * Parameters` |
|      - | 3111 | ` *  $str` |
|      - | 3112 | ` *   The input string.` |
|      - | 3113 | ` * Returns.` |
|      - | 3114 | ` *  The processed string.` |
|      - | 3115 | ` */` |
|      6 | 3116 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3117 |  |
|      - | 3118 | `	const char *zString,*zEnd;` |
|      - | 3119 | `	int nLen,c;` |
|      7 | 3120 | `	if( nArg < 1 ){` |
|      - | 3121 | `		/* Missing arguments,return null */` |
|      3 | 3122 | `		ph7_result_null(pCtx);` |
|      3 | 3123 | `		return PH7_OK;` |
|      - | 3124 | `	}` |
|      - | 3125 | `	/* Extract the target string */` |
|      5 | 3126 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3127 | `	if( nLen < 1 ){` |
|      - | 3128 | `		/* Empty string,return */` |
|      3 | 3129 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3130 | `		return PH7_OK;` |
|      - | 3131 | `	}` |
|      - | 3132 | `	/* Perform the requested operation */` |
|      3 | 3133 | `	zEnd = &zString[nLen];` |
|      3 | 3134 | `	c = zString[0];` |
|      3 | 3135 | `	if( SyisUpper(c) ){` |
|      3 | 3136 | `		c = SyToLower(c);` |
|      1 | 3137 | `	}` |
|      - | 3138 | `	/* Append the first character */` |
|      3 | 3139 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 3140 | `	zString++;` |
|      3 | 3141 | `	if( zString < zEnd ){` |
|      - | 3142 | `		/* Append the rest of the input verbatim */` |
|      3 | 3143 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 3144 | `	}` |
|      3 | 3145 | `	return PH7_OK;` |
|      4 | 3146 |  |
|      - | 3147 | `/*` |
|      - | 3148 | ` * int ord(string $string)` |
|      - | 3149 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 3150 | ` * Parameters` |
|      - | 3151 | ` *  $str` |
|      - | 3152 | ` *   The input string.` |
|      - | 3153 | ` * Returns.` |
|      - | 3154 | ` *  The ASCII value as an integer.` |
|      - | 3155 | ` */` |
|     32 | 3156 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3157 |  |
|      - | 3158 | `	const char *zString;` |
|      - | 3159 | `	int nLen,c;` |
|     33 | 3160 | `	if( nArg < 1 ){` |
|      - | 3161 | `		/* Missing arguments,return -1 */` |
|      3 | 3162 | `		ph7_result_int(pCtx,-1);` |
|      3 | 3163 | `		return PH7_OK;` |
|      - | 3164 | `	}` |
|      - | 3165 | `	/* Extract the target string */` |
|     31 | 3166 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 3167 | `	if( nLen < 1 ){` |
|      - | 3168 | `		/* Empty string,return -1 */` |
|      3 | 3169 | `		ph7_result_int(pCtx,-1);` |
|      3 | 3170 | `		return PH7_OK;` |
|      - | 3171 | `	}` |
|      - | 3172 | `	/* Extract the ASCII value of the first character */` |
|     29 | 3173 | `	c = zString[0];` |
|      - | 3174 | `	/* Return that value */` |
|     29 | 3175 | `	ph7_result_int(pCtx,c);` |
|     29 | 3176 | `	return PH7_OK;` |
|     17 | 3177 |  |
|      - | 3178 | `/*` |
|      - | 3179 | ` * string chr(int $ascii)` |
|      - | 3180 | ` *  Returns a one-character string containing the character specified by ascii.` |
|      - | 3181 | ` * Parameters` |
|      - | 3182 | ` *  $ascii` |
|      - | 3183 | ` *   The ascii code.` |
|      - | 3184 | ` * Returns.` |
|      - | 3185 | ` *  The specified character.` |
|      - | 3186 | ` */` |
|     28 | 3187 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3188 |  |
|      - | 3189 | `	int c;` |
|     29 | 3190 | `	if( nArg < 1 ){` |
|      - | 3191 | `		/* Missing arguments,return null */` |
|      3 | 3192 | `		ph7_result_null(pCtx);` |
|      3 | 3193 | `		return PH7_OK;` |
|      - | 3194 | `	}` |
|      - | 3195 | `	/* Extract the ASCII value */` |
|     27 | 3196 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 3197 | `	/* Return the specified character */` |
|     27 | 3198 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     27 | 3199 | `	return PH7_OK;` |
|     15 | 3200 |  |
|      - | 3201 | `/*` |
|      - | 3202 | ` * Binary to hex consumer callback.` |
|      - | 3203 | ` * This callback is the default consumer used by the hash functions` |
|      - | 3204 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 3205 | ` */` |
|    226 | 3206 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 3207 |  |
|      - | 3208 | `	/* Append hex chunk verbatim */` |
|    227 | 3209 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|    227 | 3210 | `	return SXRET_OK;` |
|      1 | 3211 |  |
|      - | 3212 |  |
|      - | 3213 | `/*` |
|      - | 3214 | ` * string bin2hex(string $str)` |
|      - | 3215 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 3216 | ` * Parameters` |
|      - | 3217 | ` *  $str` |
|      - | 3218 | ` *   The input string.` |
|      - | 3219 | ` * Returns.` |
|      - | 3220 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 3221 | ` */` |
|     12 | 3222 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3223 |  |
|      - | 3224 | `	const char *zString;` |
|      - | 3225 | `	int nLen;` |
|     13 | 3226 | `	if( nArg < 1 ){` |
|      - | 3227 | `		/* Missing arguments,return null */` |
|      3 | 3228 | `		ph7_result_null(pCtx);` |
|      3 | 3229 | `		return PH7_OK;` |
|      - | 3230 | `	}` |
|      - | 3231 | `	/* Extract the target string */` |
|     11 | 3232 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 3233 | `	if( nLen < 1 ){` |
|      - | 3234 | `		/* Empty string,return */` |
|      3 | 3235 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3236 | `		return PH7_OK;` |
|      - | 3237 | `	}` |
|      - | 3238 | `	/* Perform the requested operation */` |
|      9 | 3239 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|      9 | 3240 | `	return PH7_OK;` |
|      7 | 3241 |  |
|      - | 3242 |  |
|      - | 3243 | `/* Search callback signature */` |
|      - | 3244 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 3245 | `/*` |
|      - | 3246 | ` * Case-insensitive pattern match.` |
|      - | 3247 | ` * Brute force is the default search method used here.` |
|      - | 3248 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 3249 | ` * well for short/medium texts on modern hardware.` |
|      - | 3250 | ` */` |
|    118 | 3251 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      1 | 3252 |  |
|    119 | 3253 | `	const char *zpIn = (const char *)pPattern;` |
|    119 | 3254 | `	const char *zIn = (const char *)pText;` |
|    119 | 3255 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    119 | 3256 | `	const char *zEnd = &zIn[nLen];` |
|      - | 3257 | `	const char *zPtr,*zPtr2;` |
|      - | 3258 | `	int c,d;` |
|    119 | 3259 | `	if( iPatLen > nLen ){` |
|      - | 3260 | `		/* Don't bother processing */` |
|     33 | 3261 | `		return SXERR_NOTFOUND;` |
|      - | 3262 | `	}` |
|    244 | 3263 | `	for(;;){` |
|    489 | 3264 | `		if( zIn >= zEnd ){` |
|     47 | 3265 | `			break;` |
|      - | 3266 | `		}` |
|    443 | 3267 | `		c = SyToLower(zIn[0]);` |
|    443 | 3268 | `		d = SyToLower(zpIn[0]);` |
|    443 | 3269 | `		if( c == d ){` |
|     41 | 3270 | `			zPtr   = &zIn[1];` |
|     41 | 3271 | `			zPtr2  = &zpIn[1];` |
|     71 | 3272 | `			for(;;){` |
|    143 | 3273 | `				if( zPtr2 >= zpEnd ){` |
|      - | 3274 | `					/* Pattern found */` |
|     41 | 3275 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 3276 | `					return SXRET_OK;` |
|      - | 3277 | `				}` |
|    103 | 3278 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 3279 | `					break;` |
|      - | 3280 | `				}` |
|    103 | 3281 | `				c = SyToLower(zPtr[0]);` |
|    103 | 3282 | `				d = SyToLower(zPtr2[0]);` |
|    103 | 3283 | `				if( c != d ){` |
|    ! 0 | 3284 | `					break;` |
|      - | 3285 | `				}` |
|    103 | 3286 | `				zPtr++; zPtr2++;` |
|      1 | 3287 | `			}` |
|    ! 0 | 3288 | `		}` |
|    403 | 3289 | `		zIn++;` |
|      1 | 3290 | `	}` |
|      - | 3291 | `	/* Pattern not found */` |
|     47 | 3292 | `	return SXERR_NOTFOUND;` |
|     60 | 3293 |  |
|      - | 3294 | `/*` |
|      - | 3295 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3296 | ` *  Find the first occurrence of a string.` |
|      - | 3297 | ` * Parameters` |
|      - | 3298 | ` *  $haystack` |
|      - | 3299 | ` *   The input string.` |
|      - | 3300 | ` * $needle` |
|      - | 3301 | ` *   Search pattern (must be a string).` |
|      - | 3302 | ` * $before_needle` |
|      - | 3303 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3304 | ` *   of the needle (excluding the needle).` |
|      - | 3305 | ` * Return` |
|      - | 3306 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3307 | ` */` |
|     10 | 3308 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3309 |  |
|     11 | 3310 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3311 | `	const char *zBlob,*zPattern;` |
|      - | 3312 | `	int nLen,nPatLen;` |
|      - | 3313 | `	sxu32 nOfft;` |
|      - | 3314 | `	sxi32 rc;` |
|     11 | 3315 | `	if( nArg < 2 ){` |
|      - | 3316 | `		/* Missing arguments,return FALSE */` |
|      5 | 3317 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3318 | `		return PH7_OK;` |
|      - | 3319 | `	}` |
|      - | 3320 | `	/* Extract the needle and the haystack */` |
|      7 | 3321 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 3322 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 3323 | `	nOfft = 0; /* cc warning */` |
|      9 | 3324 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3325 | `		int before = 0;` |
|      - | 3326 | `		/* Perform the lookup */` |
|      5 | 3327 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3328 | `		if( rc != SXRET_OK ){` |
|      - | 3329 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3330 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3331 | `			return PH7_OK;` |
|      - | 3332 | `		}` |
|      - | 3333 | `		/* Return the portion of the string */` |
|      5 | 3334 | `		if( nArg > 2 ){` |
|      3 | 3335 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3336 | `		}` |
|      5 | 3337 | `		if( before ){` |
|      3 | 3338 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3339 | `		}else{` |
|      3 | 3340 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3341 | `		}` |
|      3 | 3342 | `	}else{` |
|      3 | 3343 | `		ph7_result_bool(pCtx,0);` |
|      - | 3344 | `	}` |
|      7 | 3345 | `	return PH7_OK;` |
|      6 | 3346 |  |
|      - | 3347 | `/*` |
|      - | 3348 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3349 | ` *  Case-insensitive strstr().` |
|      - | 3350 | ` * Parameters` |
|      - | 3351 | ` *  $haystack` |
|      - | 3352 | ` *   The input string.` |
|      - | 3353 | ` * $needle` |
|      - | 3354 | ` *   Search pattern (must be a string).` |
|      - | 3355 | ` * $before_needle` |
|      - | 3356 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3357 | ` *   of the needle (excluding the needle).` |
|      - | 3358 | ` * Return` |
|      - | 3359 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3360 | ` */` |
|      6 | 3361 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3362 |  |
|      7 | 3363 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3364 | `	const char *zBlob,*zPattern;` |
|      - | 3365 | `	int nLen,nPatLen;` |
|      - | 3366 | `	sxu32 nOfft;` |
|      - | 3367 | `	sxi32 rc;` |
|      7 | 3368 | `	if( nArg < 2 ){` |
|      - | 3369 | `		/* Missing arguments,return FALSE */` |
|      3 | 3370 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3371 | `		return PH7_OK;` |
|      - | 3372 | `	}` |
|      - | 3373 | `	/* Extract the needle and the haystack */` |
|      5 | 3374 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3375 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 3376 | `	nOfft = 0; /* cc warning */` |
|      7 | 3377 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3378 | `		int before = 0;` |
|      - | 3379 | `		/* Perform the lookup */` |
|      5 | 3380 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3381 | `		if( rc != SXRET_OK ){` |
|      - | 3382 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3383 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3384 | `			return PH7_OK;` |
|      - | 3385 | `		}` |
|      - | 3386 | `		/* Return the portion of the string */` |
|      5 | 3387 | `		if( nArg > 2 ){` |
|      3 | 3388 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3389 | `		}` |
|      5 | 3390 | `		if( before ){` |
|      3 | 3391 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3392 | `		}else{` |
|      3 | 3393 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3394 | `		}` |
|      3 | 3395 | `	}else{` |
|    ! 0 | 3396 | `		ph7_result_bool(pCtx,0);` |
|      - | 3397 | `	}` |
|      5 | 3398 | `	return PH7_OK;` |
|      4 | 3399 |  |
|      - | 3400 | `/*` |
|      - | 3401 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3402 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 3403 | ` * Parameters` |
|      - | 3404 | ` *  $haystack` |
|      - | 3405 | ` *   The input string.` |
|      - | 3406 | ` * $needle` |
|      - | 3407 | ` *   Search pattern (must be a string).` |
|      - | 3408 | ` * $offset` |
|      - | 3409 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3410 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3411 | ` *   of haystack.` |
|      - | 3412 | ` * Return` |
|      - | 3413 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3414 | ` */` |
|     80 | 3415 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3416 |  |
|     82 | 3417 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3418 | `	const char *zBlob,*zPattern;` |
|      - | 3419 | `	int nLen,nPatLen,nStart;` |
|      - | 3420 | `	sxu32 nOfft;` |
|      - | 3421 | `	sxi32 rc;` |
|     82 | 3422 | `	if( nArg < 2 ){` |
|      - | 3423 | `		/* Missing arguments,return FALSE */` |
|      7 | 3424 | `		ph7_result_bool(pCtx,0);` |
|      7 | 3425 | `		return PH7_OK;` |
|      - | 3426 | `	}` |
|      - | 3427 | `	/* Extract the needle and the haystack */` |
|     76 | 3428 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     76 | 3429 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     76 | 3430 | `	nOfft = 0; /* cc warning */` |
|     76 | 3431 | `	nStart = 0;` |
|      - | 3432 | `	/* Peek the starting offset if available */` |
|     76 | 3433 | `	if( nArg > 2 ){` |
|    ! 0 | 3434 | `		nStart = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 3435 | `		if( nStart < 0 ){` |
|    ! 0 | 3436 | `			nStart = -nStart;` |
|    ! 0 | 3437 | `		}` |
|    ! 0 | 3438 | `		if( nStart >= nLen ){` |
|      - | 3439 | `			/* Invalid offset */` |
|    ! 0 | 3440 | `			nStart = 0;` |
|    ! 0 | 3441 | `		}else{` |
|    ! 0 | 3442 | `			zBlob += nStart;` |
|    ! 0 | 3443 | `			nLen -= nStart;` |
|      - | 3444 | `		}` |
|    ! 0 | 3445 | `	}` |
|     76 | 3446 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3447 | `		/* Perform the lookup */` |
|     74 | 3448 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     74 | 3449 | `		if( rc != SXRET_OK ){` |
|      - | 3450 | `			/* Pattern not found,return FALSE */` |
|      3 | 3451 | `			ph7_result_bool(pCtx,0);` |
|      3 | 3452 | `			return PH7_OK;` |
|      - | 3453 | `		}` |
|      - | 3454 | `		/* Return the pattern position */` |
|     72 | 3455 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|     37 | 3456 | `	}else{` |
|      3 | 3457 | `		ph7_result_bool(pCtx,0);` |
|      - | 3458 | `	}` |
|     74 | 3459 | `	return PH7_OK;` |
|     42 | 3460 |  |
|      - | 3461 | `/*` |
|      - | 3462 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3463 | ` *  Case-insensitive strpos.` |
|      - | 3464 | ` * Parameters` |
|      - | 3465 | ` *  $haystack` |
|      - | 3466 | ` *   The input string.` |
|      - | 3467 | ` * $needle` |
|      - | 3468 | ` *   Search pattern (must be a string).` |
|      - | 3469 | ` * $offset` |
|      - | 3470 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3471 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3472 | ` *   of haystack.` |
|      - | 3473 | ` * Return` |
|      - | 3474 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3475 | ` */` |
|     18 | 3476 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3477 |  |
|     19 | 3478 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3479 | `	const char *zBlob,*zPattern;` |
|      - | 3480 | `	int nLen,nPatLen,nStart;` |
|      - | 3481 | `	sxu32 nOfft;` |
|      - | 3482 | `	sxi32 rc;` |
|     19 | 3483 | `	if( nArg < 2 ){` |
|      - | 3484 | `		/* Missing arguments,return FALSE */` |
|      3 | 3485 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3486 | `		return PH7_OK;` |
|      - | 3487 | `	}` |
|      - | 3488 | `	/* Extract the needle and the haystack */` |
|     17 | 3489 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 3490 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     17 | 3491 | `	nOfft = 0; /* cc warning */` |
|     17 | 3492 | `	nStart = 0;` |
|      - | 3493 | `	/* Peek the starting offset if available */` |
|     17 | 3494 | `	if( nArg > 2 ){` |
|      5 | 3495 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 3496 | `		if( nStart < 0 ){` |
|      3 | 3497 | `			nStart = -nStart;` |
|      1 | 3498 | `		}` |
|      5 | 3499 | `		if( nStart >= nLen ){` |
|      - | 3500 | `			/* Invalid offset */` |
|    ! 0 | 3501 | `			nStart = 0;` |
|    ! 0 | 3502 | `		}else{` |
|      5 | 3503 | `			zBlob += nStart;` |
|      5 | 3504 | `			nLen -= nStart;` |
|      - | 3505 | `		}` |
|      2 | 3506 | `	}` |
|     17 | 3507 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3508 | `		/* Perform the lookup */` |
|     17 | 3509 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     17 | 3510 | `		if( rc != SXRET_OK ){` |
|      - | 3511 | `			/* Pattern not found,return FALSE */` |
|      3 | 3512 | `			ph7_result_bool(pCtx,0);` |
|      3 | 3513 | `			return PH7_OK;` |
|      - | 3514 | `		}` |
|      - | 3515 | `		/* Return the pattern position */` |
|     15 | 3516 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 3517 | `	}else{` |
|    ! 0 | 3518 | `		ph7_result_bool(pCtx,0);` |
|      - | 3519 | `	}` |
|     15 | 3520 | `	return PH7_OK;` |
|     10 | 3521 |  |
|      - | 3522 | `/*` |
|      - | 3523 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3524 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 3525 | ` * Parameters` |
|      - | 3526 | ` *  $haystack` |
|      - | 3527 | ` *   The input string.` |
|      - | 3528 | ` * $needle` |
|      - | 3529 | ` *   Search pattern (must be a string).` |
|      - | 3530 | ` * $offset` |
|      - | 3531 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3532 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3533 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3534 | ` * Return` |
|      - | 3535 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3536 | ` */` |
|     32 | 3537 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3538 |  |
|      - | 3539 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     33 | 3540 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3541 | `	int nLen,nPatLen;` |
|      - | 3542 | `	sxu32 nOfft;` |
|      - | 3543 | `	sxi32 rc;` |
|     33 | 3544 | `	if( nArg < 2 ){` |
|      - | 3545 | `		/* Missing arguments,return FALSE */` |
|      3 | 3546 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3547 | `		return PH7_OK;` |
|      - | 3548 | `	}` |
|      - | 3549 | `	/* Extract the needle and the haystack */` |
|     31 | 3550 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 3551 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3552 | `	/* Point to the end of the pattern */` |
|     31 | 3553 | `	zPtr = &zBlob[nLen - 1];` |
|     31 | 3554 | `	zEnd = &zBlob[nLen];` |
|      - | 3555 | `	/* Save the starting posistion */` |
|     31 | 3556 | `	zStart = zBlob;` |
|     31 | 3557 | `	nOfft = 0; /* cc warning */` |
|      - | 3558 | `	/* Peek the starting offset if available */` |
|     31 | 3559 | `	if( nArg > 2 ){` |
|      - | 3560 | `		int nStart;` |
|     21 | 3561 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 3562 | `		if( nStart < 0 ){` |
|     11 | 3563 | `			nStart = -nStart;` |
|     11 | 3564 | `			if( nStart >= nLen ){` |
|      - | 3565 | `				/* Invalid offset */` |
|      3 | 3566 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3567 | `				return PH7_OK;` |
|    ! 0 | 3568 | `			}else{` |
|      9 | 3569 | `				nLen -= nStart;` |
|      9 | 3570 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 3571 | `				zEnd = &zBlob[nLen];` |
|      - | 3572 | `			}` |
|      5 | 3573 | `		}else{` |
|     11 | 3574 | `			if( nStart >= nLen ){` |
|      - | 3575 | `				/* Invalid offset */` |
|      5 | 3576 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3577 | `				return PH7_OK;` |
|    ! 0 | 3578 | `			}else{` |
|      7 | 3579 | `				zBlob += nStart;` |
|      7 | 3580 | `				nLen -= nStart;` |
|      - | 3581 | `			}` |
|      - | 3582 | `		}` |
|      7 | 3583 | `	}` |
|     25 | 3584 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3585 | `		/* Perform the lookup */` |
|     57 | 3586 | `		for(;;){` |
|    115 | 3587 | `			if( zBlob >= zPtr ){` |
|     11 | 3588 | `				break;` |
|      - | 3589 | `			}` |
|    105 | 3590 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    105 | 3591 | `			if( rc == SXRET_OK ){` |
|      - | 3592 | `				/* Pattern found,return it's position */` |
|     13 | 3593 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 3594 | `				return PH7_OK;` |
|      - | 3595 | `			}` |
|     93 | 3596 | `			zPtr--;` |
|      1 | 3597 | `		}` |
|      - | 3598 | `		/* Pattern not found,return FALSE */` |
|     11 | 3599 | `		ph7_result_bool(pCtx,0);` |
|      6 | 3600 | `	}else{` |
|      3 | 3601 | `		ph7_result_bool(pCtx,0);` |
|      - | 3602 | `	}` |
|     13 | 3603 | `	return PH7_OK;` |
|     17 | 3604 |  |
|      - | 3605 | `/*` |
|      - | 3606 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3607 | ` *  Case-insensitive strrpos.` |
|      - | 3608 | ` * Parameters` |
|      - | 3609 | ` *  $haystack` |
|      - | 3610 | ` *   The input string.` |
|      - | 3611 | ` * $needle` |
|      - | 3612 | ` *   Search pattern (must be a string).` |
|      - | 3613 | ` * $offset` |
|      - | 3614 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3615 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3616 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3617 | ` * Return` |
|      - | 3618 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3619 | ` */` |
|     28 | 3620 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3621 |  |
|      - | 3622 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     29 | 3623 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3624 | `	int nLen,nPatLen;` |
|      - | 3625 | `	sxu32 nOfft;` |
|      - | 3626 | `	sxi32 rc;` |
|     29 | 3627 | `	if( nArg < 2 ){` |
|      - | 3628 | `		/* Missing arguments,return FALSE */` |
|      3 | 3629 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3630 | `		return PH7_OK;` |
|      - | 3631 | `	}` |
|      - | 3632 | `	/* Extract the needle and the haystack */` |
|     27 | 3633 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 3634 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3635 | `	/* Point to the end of the pattern */` |
|     27 | 3636 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 3637 | `	zEnd = &zBlob[nLen];` |
|      - | 3638 | `	/* Save the starting posistion */` |
|     27 | 3639 | `	zStart = zBlob;` |
|     27 | 3640 | `	nOfft = 0; /* cc warning */` |
|      - | 3641 | `	/* Peek the starting offset if available */` |
|     27 | 3642 | `	if( nArg > 2 ){` |
|      - | 3643 | `		int nStart;` |
|     15 | 3644 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3645 | `		if( nStart < 0 ){` |
|      7 | 3646 | `			nStart = -nStart;` |
|      7 | 3647 | `			if( nStart >= nLen ){` |
|      - | 3648 | `				/* Invalid offset */` |
|      3 | 3649 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3650 | `				return PH7_OK;` |
|    ! 0 | 3651 | `			}else{` |
|      5 | 3652 | `				nLen -= nStart;` |
|      5 | 3653 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 3654 | `				zEnd = &zBlob[nLen];` |
|      - | 3655 | `			}` |
|      3 | 3656 | `		}else{` |
|      9 | 3657 | `			if( nStart >= nLen ){` |
|      - | 3658 | `				/* Invalid offset */` |
|      5 | 3659 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3660 | `				return PH7_OK;` |
|    ! 0 | 3661 | `			}else{` |
|      5 | 3662 | `				zBlob += nStart;` |
|      5 | 3663 | `				nLen -= nStart;` |
|      - | 3664 | `			}` |
|      - | 3665 | `		}` |
|      4 | 3666 | `	}` |
|     21 | 3667 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3668 | `		/* Perform the lookup */` |
|     44 | 3669 | `		for(;;){` |
|     89 | 3670 | `			if( zBlob >= zPtr ){` |
|      9 | 3671 | `				break;` |
|      - | 3672 | `			}` |
|     81 | 3673 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 3674 | `			if( rc == SXRET_OK ){` |
|      - | 3675 | `				/* Pattern found,return it's position */` |
|     11 | 3676 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 3677 | `				return PH7_OK;` |
|      - | 3678 | `			}` |
|     71 | 3679 | `			zPtr--;` |
|      1 | 3680 | `		}` |
|      - | 3681 | `		/* Pattern not found,return FALSE */` |
|      9 | 3682 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3683 | `	}else{` |
|      3 | 3684 | `		ph7_result_bool(pCtx,0);` |
|      - | 3685 | `	}` |
|     11 | 3686 | `	return PH7_OK;` |
|     15 | 3687 |  |
|      - | 3688 | `/*` |
|      - | 3689 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3690 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3691 | ` * Parameters` |
|      - | 3692 | ` *  $haystack` |
|      - | 3693 | ` *   The input string.` |
|      - | 3694 | ` * $needle` |
|      - | 3695 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3696 | ` *  This behavior is different from that of strstr().` |
|      - | 3697 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3698 | ` *  as the ordinal value of a character.` |
|      - | 3699 | ` * Return` |
|      - | 3700 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3701 | ` */` |
|     24 | 3702 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3703 |  |
|      - | 3704 | `	const char *zBlob;` |
|      - | 3705 | `	int nLen,c;` |
|     25 | 3706 | `	if( nArg < 2 ){` |
|      - | 3707 | `		/* Missing arguments,return FALSE */` |
|      3 | 3708 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3709 | `		return PH7_OK;` |
|      - | 3710 | `	}` |
|      - | 3711 | `	/* Extract the haystack */` |
|     23 | 3712 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3713 | `	c = 0; /* cc warning */` |
|     23 | 3714 | `	if( nLen > 0 ){` |
|      - | 3715 | `		sxu32 nOfft;` |
|      - | 3716 | `		sxi32 rc;` |
|     21 | 3717 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 3718 | `			const char *zPattern;` |
|     11 | 3719 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 3720 | `														 * for NULL pointer.` |
|      - | 3721 | `														 */` |
|     11 | 3722 | `			c = zPattern[0];` |
|      6 | 3723 | `		}else{` |
|      - | 3724 | `			/* Int cast */` |
|     11 | 3725 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 3726 | `		}` |
|      - | 3727 | `		/* Perform the lookup */` |
|     21 | 3728 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3729 | `		if( rc != SXRET_OK ){` |
|      - | 3730 | `			/* No such entry,return FALSE */` |
|      7 | 3731 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3732 | `			return PH7_OK;` |
|      - | 3733 | `		}` |
|      - | 3734 | `		/* Return the string portion */` |
|     15 | 3735 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 3736 | `	}else{` |
|      3 | 3737 | `		ph7_result_bool(pCtx,0);` |
|      - | 3738 | `	}` |
|     17 | 3739 | `	return PH7_OK;` |
|     13 | 3740 |  |
|      - | 3741 | `/*` |
|      - | 3742 | ` * string strrev(string $string)` |
|      - | 3743 | ` *  Reverse a string.` |
|      - | 3744 | ` * Parameters` |
|      - | 3745 | ` *  $string` |
|      - | 3746 | ` *   String to be reversed.` |
|      - | 3747 | ` * Return` |
|      - | 3748 | ` *  The reversed string.` |
|      - | 3749 | ` */` |
|      4 | 3750 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3751 |  |
|      - | 3752 | `	const char *zIn,*zEnd;` |
|      - | 3753 | `	int nLen,c;` |
|      5 | 3754 | `	if( nArg < 1 ){` |
|      - | 3755 | `		/* Missing arguments,return NULL */` |
|      3 | 3756 | `		ph7_result_null(pCtx);` |
|      3 | 3757 | `		return PH7_OK;` |
|      - | 3758 | `	}` |
|      - | 3759 | `	/* Extract the target string */` |
|      3 | 3760 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3761 | `	if( nLen < 1 ){` |
|      - | 3762 | `		/* Empty string Return null */` |
|    ! 0 | 3763 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3764 | `		return PH7_OK;` |
|      - | 3765 | `	}` |
|      - | 3766 | `	/* Perform the requested operation */` |
|      3 | 3767 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3768 | `	for(;;){` |
|      9 | 3769 | `		if( zEnd < zIn ){` |
|      - | 3770 | `			/* No more input to process */` |
|      3 | 3771 | `			break;` |
|      - | 3772 | `		}` |
|      - | 3773 | `		/* Append current character */` |
|      7 | 3774 | `		c = zEnd[0];` |
|      7 | 3775 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3776 | `		zEnd--;` |
|      1 | 3777 | `	}` |
|      3 | 3778 | `	return PH7_OK;` |
|      3 | 3779 |  |
|      - | 3780 | `/*` |
|      - | 3781 | ` * string ucwords(string $string)` |
|      - | 3782 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3783 | ` *  The definition of a word is any string of characters that is immediately after` |
|      - | 3784 | ` *  a whitespace (These are: space, form-feed, newline, carriage return, horizontal tab, and vertical tab).` |
|      - | 3785 | ` * Parameters` |
|      - | 3786 | ` *  $string` |
|      - | 3787 | ` *   The input string.` |
|      - | 3788 | ` * Return` |
|      - | 3789 | ` *  The modified string..` |
|      - | 3790 | ` */` |
|     14 | 3791 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3792 |  |
|      - | 3793 | `	const char *zIn,*zCur,*zEnd;` |
|      - | 3794 | `	int nLen,c;` |
|     15 | 3795 | `	if( nArg < 1 ){` |
|      - | 3796 | `		/* Missing arguments,return NULL */` |
|      3 | 3797 | `		ph7_result_null(pCtx);` |
|      3 | 3798 | `		return PH7_OK;` |
|      - | 3799 | `	}` |
|      - | 3800 | `	/* Extract the target string */` |
|     13 | 3801 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 3802 | `	if( nLen < 1 ){` |
|      - | 3803 | `		/* Empty string – match PHP semantics */` |
|      3 | 3804 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3805 | `		return PH7_OK;` |
|      - | 3806 | `	}` |
|      - | 3807 | `	/* Perform the requested operation */` |
|     11 | 3808 | `	zEnd = &zIn[nLen];` |
|     21 | 3809 | `	for(;;){` |
|      - | 3810 | `		/* Jump leading white spaces */` |
|     43 | 3811 | `		zCur = zIn;` |
|     65 | 3812 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0x80 && SyisSpace(zIn[0]) ){` |
|     23 | 3813 | `			zIn++;` |
|      1 | 3814 | `		}` |
|     43 | 3815 | `		if( zCur < zIn ){` |
|      - | 3816 | `			/* Append white space stream */` |
|     23 | 3817 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 3818 | `		}` |
|     43 | 3819 | `		if( zIn >= zEnd ){` |
|      - | 3820 | `			/* No more input to process */` |
|     11 | 3821 | `			break;` |
|      - | 3822 | `		}` |
|     33 | 3823 | `		c = zIn[0];` |
|     33 | 3824 | `		if( c < 0x80 && SyisLower(c) ){` |
|     29 | 3825 | `			c = SyToUpper(c);` |
|     14 | 3826 | `		}` |
|      - | 3827 | `		/* Append the upper-cased character */` |
|     33 | 3828 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     33 | 3829 | `		zIn++;` |
|     33 | 3830 | `		zCur = zIn;` |
|      - | 3831 | `		/* Append the word varbatim */` |
|    149 | 3832 | `		while( zIn < zEnd ){` |
|    139 | 3833 | `			if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 3834 | `				/* UTF-8 stream */` |
|    ! 0 | 3835 | `				zIn++;` |
|    ! 0 | 3836 | `				SX_JMP_UTF8(zIn,zEnd);` |
|    138 | 3837 | `			}else if( !SyisSpace(zIn[0]) ){` |
|    117 | 3838 | `				zIn++;` |
|     59 | 3839 | `			}else{` |
|     23 | 3840 | `				break;` |
|      - | 3841 | `			}` |
|      1 | 3842 | `		}` |
|     33 | 3843 | `		if( zCur < zIn ){` |
|     33 | 3844 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 | 3845 | `		}` |
|      1 | 3846 | `	}` |
|     11 | 3847 | `	return PH7_OK;` |
|      8 | 3848 |  |
|      - | 3849 | `/*` |
|      - | 3850 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3851 | ` *  Returns input repeated multiplier times.` |
|      - | 3852 | ` * Parameters` |
|      - | 3853 | ` *  $string` |
|      - | 3854 | ` *   String to be repeated.` |
|      - | 3855 | ` * $multiplier` |
|      - | 3856 | ` *  Number of time the input string should be repeated.` |
|      - | 3857 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3858 | ` *  to 0, the function will return an empty string.` |
|      - | 3859 | ` * Return` |
|      - | 3860 | ` *  The repeated string.` |
|      - | 3861 | ` */` |
|  20212 | 3862 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3863 |  |
|      - | 3864 | `	const char *zIn;` |
|      - | 3865 | `	int nLen,nMul;` |
|      - | 3866 | `	int rc;` |
|  20213 | 3867 | `	if( nArg < 2 ){` |
|      - | 3868 | `		/* Missing arguments,return NULL */` |
|      3 | 3869 | `		ph7_result_null(pCtx);` |
|      3 | 3870 | `		return PH7_OK;` |
|      - | 3871 | `	}` |
|      - | 3872 | `	/* Extract the target string */` |
|  20211 | 3873 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|  20211 | 3874 | `	if( nLen < 1 ){` |
|      - | 3875 | `		/* Empty string.Return null */` |
|    ! 0 | 3876 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3877 | `		return PH7_OK;` |
|      - | 3878 | `	}` |
|      - | 3879 | `	/* Extract the multiplier */` |
|  20211 | 3880 | `	nMul = ph7_value_to_int(apArg[1]);` |
|  20211 | 3881 | `	if( nMul < 1 ){` |
|      - | 3882 | `		/* Return the empty string */` |
|      3 | 3883 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3884 | `		return PH7_OK;` |
|      - | 3885 | `	}` |
|      - | 3886 | `	/* Perform the requested operation */` |
| 120220 | 3887 | `	for(;;){` |
| 240441 | 3888 | `		if( !nMul ){` |
|  20209 | 3889 | `			break;` |
|      - | 3890 | `		}` |
|      - | 3891 | `		/* Append the copy */` |
| 220233 | 3892 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 220233 | 3893 | `		if( rc != PH7_OK ){` |
|      - | 3894 | `			/* Out of memory,break immediately */` |
|    ! 0 | 3895 | `			break;` |
|      - | 3896 | `		}` |
| 220233 | 3897 | `		nMul--;` |
|      1 | 3898 | `	}` |
|  20209 | 3899 | `	return PH7_OK;` |
|  10107 | 3900 |  |
|      - | 3901 | `/*` |
|      - | 3902 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3903 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3904 | ` * Parameters` |
|      - | 3905 | ` *  $string` |
|      - | 3906 | ` *   The input string.` |
|      - | 3907 | ` * $is_xhtml` |
|      - | 3908 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3909 | ` * Return` |
|      - | 3910 | ` *  The processed string.` |
|      - | 3911 | ` */` |
|      6 | 3912 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3913 |  |
|      - | 3914 | `	const char *zIn,*zCur,*zEnd;` |
|      7 | 3915 | `	int is_xhtml = 1; /* Default to XHTML-style '<br/>' like PHP */` |
|      - | 3916 | `	int nLen;` |
|      7 | 3917 | `	if( nArg < 1 ){` |
|      - | 3918 | `		/* Missing arguments,return the empty string */` |
|      3 | 3919 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3920 | `		return PH7_OK;` |
|      - | 3921 | `	}` |
|      - | 3922 | `	/* Extract the target string */` |
|      5 | 3923 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3924 | `	if( nLen < 1 ){` |
|      - | 3925 | `		/* Empty string,return null */` |
|    ! 0 | 3926 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3927 | `		return PH7_OK;` |
|      - | 3928 | `	}` |
|      5 | 3929 | `	if( nArg > 1 ){` |
|      3 | 3930 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3931 | `	}` |
|      5 | 3932 | `	zEnd = &zIn[nLen];` |
|      - | 3933 | `	/* Perform the requested operation */` |
|      4 | 3934 | `	for(;;){` |
|      9 | 3935 | `		zCur = zIn;` |
|      - | 3936 | `		/* Delimit the string */` |
|     21 | 3937 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3938 | `			zIn++;` |
|      1 | 3939 | `		}` |
|      9 | 3940 | `		if( zCur < zIn ){` |
|      - | 3941 | `			/* Output chunk verbatim */` |
|      9 | 3942 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3943 | `		}` |
|      9 | 3944 | `		if( zIn >= zEnd ){` |
|      - | 3945 | `			/* No more input to process */` |
|      5 | 3946 | `			break;` |
|      - | 3947 | `		}` |
|      - | 3948 | `		/* Output the HTML line break */` |
|      - | 3949 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br/>' (legacy without space), otherwise use '<br>' */` |
|      5 | 3950 | `		if( is_xhtml ){` |
|      3 | 3951 | `			ph7_result_string(pCtx,"<br/>",(int)sizeof("<br/>")-1);` |
|      2 | 3952 | `		}else{` |
|      3 | 3953 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3954 | `		}` |
|      5 | 3955 | `		zCur = zIn;` |
|      - | 3956 | `		/* Append trailing line */` |
|     11 | 3957 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3958 | `			zIn++;` |
|      1 | 3959 | `		}` |
|      5 | 3960 | `		if( zCur < zIn ){` |
|      - | 3961 | `			/* Output chunk verbatim */` |
|      5 | 3962 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 3963 | `		}` |
|      1 | 3964 | `	}` |
|      5 | 3965 | `	return PH7_OK;` |
|      4 | 3966 |  |
|      - | 3967 | `/*` |
|      - | 3968 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3969 | ` *  According to the PHP reference manual.` |
|      - | 3970 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3971 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3972 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 3973 | ` * This applies to both sprintf() and printf().` |
|      - | 3974 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 3975 | ` * or more of these elements, in order:` |
|      - | 3976 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 3977 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 3978 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 3979 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 3980 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 3981 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 3982 | ` *   it with a single quote ('). See the examples below.` |
|      - | 3983 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 3984 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 3985 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 3986 | ` *   should result in.` |
|      - | 3987 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 3988 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 3989 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 3990 | ` *   limit to the string.` |
|      - | 3991 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 3992 | ` *       % - a literal percent character. No argument is required.` |
|      - | 3993 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 3994 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 3995 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 3996 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 3997 | ` * 	     for the number of digits after the decimal point.` |
|      - | 3998 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 3999 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 4000 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 4001 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 4002 | ` *       g - shorter of %e and %f.` |
|      - | 4003 | ` *       G - shorter of %E and %f.` |
|      - | 4004 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 4005 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 4006 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 4007 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 4008 | ` */` |
|      - | 4009 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4010 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 4011 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 4012 | `/*` |
|      - | 4013 | `** Conversion types fall into various categories as defined by the` |
|      - | 4014 | `** following enumeration.` |
|      - | 4015 | `*/` |
|      - | 4016 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 4017 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 4018 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 4019 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 4020 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 4021 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 4022 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 4023 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 4024 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 4025 |  |
|      - | 4026 | `/*` |
|      - | 4027 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 4028 | `*/` |
|      - | 4029 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 4030 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 4031 | `/*` |
|      - | 4032 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 4033 | `** by an instance of the following structure` |
|      - | 4034 | `*/` |
|      - | 4035 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 4036 | `struct ph7_fmt_info` |
|      - | 4037 |  |
|      - | 4038 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 4039 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 4040 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 4041 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 4042 | `  char *charset; /* The character set for conversion */` |
|      - | 4043 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 4044 | `};` |
|      - | 4045 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 4046 | `/*` |
|      - | 4047 | `** "*val" is a double such that 0.1 <= *val < 10.0` |
|      - | 4048 | `** Return the ascii code for the leading digit of *val, then` |
|      - | 4049 | `** multiply "*val" by 10.0 to renormalize.` |
|      - | 4050 | `**` |
|      - | 4051 | `** Example:` |
|      - | 4052 | `**     input:     *val = 3.14159` |
|      - | 4053 | `**     output:    *val = 1.4159    function return = '3'` |
|      - | 4054 | `**` |
|      - | 4055 | `** The counter *cnt is incremented each time.  After counter exceeds` |
|      - | 4056 | `** 16 (the number of significant digits in a 64-bit float) '0' is` |
|      - | 4057 | `** always returned.` |
|      - | 4058 | `*/` |
|    404 | 4059 | `static int vxGetdigit(sxlongreal *val,int *cnt)` |
|      1 | 4060 |  |
|      - | 4061 | `  sxlongreal d;` |
|      - | 4062 | `  int digit;` |
|      - | 4063 |  |
|    405 | 4064 | `  if( (*cnt)++ >= 16 ){` |
|    ! 0 | 4065 | `	  return '0';` |
|      - | 4066 | `  }` |
|    405 | 4067 | `  digit = (int)*val;` |
|    405 | 4068 | `  d = digit;` |
|    405 | 4069 | `   *val = (*val - d)*10.0;` |
|    405 | 4070 | `  return digit + '0' ;` |
|    203 | 4071 |  |
|      - | 4072 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      - | 4073 | `/*` |
|      - | 4074 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 4075 | ` * used conversion types first.` |
|      - | 4076 | ` */` |
|      - | 4077 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 4078 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 4079 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 4080 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 4081 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 4082 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 4083 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 4084 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 4085 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 4086 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4087 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4088 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 4089 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 4090 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 4091 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 4092 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 4093 | `};` |
|      - | 4094 | `/*` |
|      - | 4095 | ` * Format a given string.` |
|      - | 4096 | ` * The root program.  All variations call this core.` |
|      - | 4097 | ` * INPUTS:` |
|      - | 4098 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 4099 | ` *            1. A pointer to the call context.` |
|      - | 4100 | ` *            2. A pointer to the list of characters to be output` |
|      - | 4101 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 4102 | ` *            3. An integer number of characters to be output.` |
|      - | 4103 | ` *               (Note: This number might be zero.)` |
|      - | 4104 | ` *            4. Upper layer private data.` |
|      - | 4105 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 4106 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 4107 | ` */` |
|    120 | 4108 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 4109 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 4110 | `	ph7_context *pCtx,  /* call context */` |
|      - | 4111 | `	const char *zIn,    /* Format string */` |
|      - | 4112 | `	int nByte,          /* Format string length */` |
|      - | 4113 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 4114 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 4115 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 4116 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 4117 | `	)` |
|      1 | 4118 |  |
|    121 | 4119 | `	char spaces[] = "                                                  ";` |
|      - | 4120 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    121 | 4121 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 4122 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 4123 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 4124 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 4125 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 4126 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 4127 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 4128 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 4129 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 4130 | `	ph7_int64 iVal;` |
|      - | 4131 | `	int precision;           /* Precision of the current field */` |
|      - | 4132 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 4133 | `	int c,rc,n;` |
|      - | 4134 | `	int length;              /* Length of the field */` |
|      - | 4135 | `	int prefix;` |
|      - | 4136 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 4137 | `	int width;               /* Width of the current field */` |
|      - | 4138 | `	int idx;` |
|    121 | 4139 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 4140 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 4141 | `	/* Start the format process */` |
|    123 | 4142 | `	for(;;){` |
|    247 | 4143 | `		zCur = zIn;` |
|    697 | 4144 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|    451 | 4145 | `			zIn++;` |
|      1 | 4146 | `		}` |
|    247 | 4147 | `		if( zCur < zIn ){` |
|      - | 4148 | `			/* Consume chunk verbatim */` |
|     95 | 4149 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|     95 | 4150 | `			if( rc == SXERR_ABORT ){` |
|      - | 4151 | `				/* Callback request an operation abort */` |
|    ! 0 | 4152 | `				break;` |
|      - | 4153 | `			}` |
|     47 | 4154 | `		}` |
|    247 | 4155 | `		if( zIn >= zEnd ){` |
|      - | 4156 | `			/* No more input to process,break immediately */` |
|    119 | 4157 | `			break;` |
|      - | 4158 | `		}` |
|      - | 4159 | `		/* Find out what flags are present */` |
|    129 | 4160 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    128 | 4161 | `			flag_alternateform = flag_zeropad = 0;` |
|    129 | 4162 | `		zIn++; /* Jump the precent sign */` |
|     64 | 4163 | `		do{` |
|    157 | 4164 | `			c = zIn[0];` |
|    157 | 4165 | `			switch( c ){` |
|      9 | 4166 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      3 | 4167 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 4168 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|      9 | 4169 | `			case '#':   flag_alternateform = 1;   c = 0;   break;` |
|      5 | 4170 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 4171 | `			case '\'':` |
|    ! 0 | 4172 | `				zIn++;` |
|    ! 0 | 4173 | `				if( zIn < zEnd ){` |
|      - | 4174 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 4175 | `					c = zIn[0];` |
|    ! 0 | 4176 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4177 | `						spaces[idx] = (char)c;` |
|    ! 0 | 4178 | `					}` |
|    ! 0 | 4179 | `					c = 0;` |
|    ! 0 | 4180 | `				}` |
|    ! 0 | 4181 | `				break;` |
|    128 | 4182 | `			default:                                       break;` |
|      - | 4183 | `			}` |
|    157 | 4184 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 4185 | `		/* Get the field width */` |
|    129 | 4186 | `		width = 0;` |
|    223 | 4187 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     31 | 4188 | `			width = width*10 + (zIn[0] - '0');` |
|     31 | 4189 | `			zIn++;` |
|      1 | 4190 | `		}` |
|    129 | 4191 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 4192 | `			/* Position specifer */` |
|    ! 0 | 4193 | `			if( width > 0 ){` |
|    ! 0 | 4194 | `				n = width;` |
|    ! 0 | 4195 | `				if( vf && n > 0 ){` |
|    ! 0 | 4196 | `					n--;` |
|    ! 0 | 4197 | `				}` |
|    ! 0 | 4198 | `			}` |
|    ! 0 | 4199 | `			zIn++;` |
|    ! 0 | 4200 | `			width = 0;` |
|    ! 0 | 4201 | `			if( zIn < zEnd && zIn[0] == '0' ){` |
|    ! 0 | 4202 | `				flag_zeropad = 1;` |
|    ! 0 | 4203 | `				zIn++;` |
|    ! 0 | 4204 | `			}` |
|    ! 0 | 4205 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 4206 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 4207 | `				zIn++;` |
|    ! 0 | 4208 | `			}` |
|    ! 0 | 4209 | `		}` |
|    129 | 4210 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 4211 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 4212 | `		}` |
|      - | 4213 | `		/* Get the precision */` |
|    129 | 4214 | `		precision = -1;` |
|    129 | 4215 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     57 | 4216 | `			precision = 0;` |
|     57 | 4217 | `			zIn++;` |
|    145 | 4218 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     61 | 4219 | `				precision = precision*10 + (zIn[0] - '0');` |
|     61 | 4220 | `				zIn++;` |
|      1 | 4221 | `			}` |
|     28 | 4222 | `		}` |
|    129 | 4223 | `		if( zIn >= zEnd ){` |
|      - | 4224 | `			/* No more input */` |
|      3 | 4225 | `			break;` |
|      - | 4226 | `		}` |
|      - | 4227 | `		/* Fetch the info entry for the field */` |
|    127 | 4228 | `		pInfo = 0;` |
|    127 | 4229 | `		xtype = PH7_FMT_ERROR;` |
|    127 | 4230 | `		c = zIn[0];` |
|    127 | 4231 | `		zIn++; /* Jump the format specifer */` |
|    699 | 4232 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|    697 | 4233 | `			if( c==aFmt[idx].fmttype ){` |
|    125 | 4234 | `				pInfo = &aFmt[idx];` |
|    125 | 4235 | `				xtype = pInfo->type;` |
|    125 | 4236 | `				break;` |
|      - | 4237 | `			}` |
|    287 | 4238 | `		}` |
|    127 | 4239 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    127 | 4240 | `		length = 0;` |
|      - | 4241 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 4242 | `		 /*` |
|      - | 4243 | `		  ** At this point, variables are initialized as follows:` |
|      - | 4244 | `		  **` |
|      - | 4245 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 4246 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 4247 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 4248 | `		  **                               field width was negative.` |
|      - | 4249 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 4250 | `		  **                               the conversion character.` |
|      - | 4251 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 4252 | `		  **   width                       The specified field width.  This is` |
|      - | 4253 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 4254 | `		  **   precision                   The specified precision.  The default` |
|      - | 4255 | `		  **                               is -1.` |
|      - | 4256 | `		  */` |
|    127 | 4257 | `		switch(xtype){` |
|    ! 0 | 4258 | `		case PH7_FMT_PERCENT:` |
|      - | 4259 | `			/* A literal percent character */` |
|    ! 0 | 4260 | `			zWorker[0] = '%';` |
|    ! 0 | 4261 | `			length = (int)sizeof(char);` |
|    ! 0 | 4262 | `			break;` |
|      3 | 4263 | `		case PH7_FMT_CHARX:` |
|      - | 4264 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 4265 | `			 * with that ASCII value` |
|      - | 4266 | `			 */` |
|      7 | 4267 | `			pArg = NEXT_ARG;` |
|      7 | 4268 | `			if( pArg == 0 ){` |
|      3 | 4269 | `				c = 0;` |
|      2 | 4270 | `			}else{` |
|      5 | 4271 | `				c = ph7_value_to_int(pArg);` |
|      - | 4272 | `			}` |
|      - | 4273 | `			/* NUL byte is an acceptable value */` |
|      7 | 4274 | `			zWorker[0] = (char)c;` |
|      7 | 4275 | `			length = (int)sizeof(char);` |
|      7 | 4276 | `			break;` |
|     12 | 4277 | `		case PH7_FMT_STRING:` |
|      - | 4278 | `			/* the argument is treated as and presented as a string */` |
|     25 | 4279 | `			pArg = NEXT_ARG;` |
|     25 | 4280 | `			if( pArg == 0 ){` |
|    ! 0 | 4281 | `				length = 0;` |
|    ! 0 | 4282 | `			}else{` |
|     25 | 4283 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 4284 | `			}` |
|     25 | 4285 | `			if( length < 1 ){` |
|    ! 0 | 4286 | `				zBuf = " ";` |
|    ! 0 | 4287 | `				length = (int)sizeof(char);` |
|    ! 0 | 4288 | `			}` |
|     25 | 4289 | `			if( precision>=0 && precision<length ){` |
|      3 | 4290 | `				length = precision;` |
|      1 | 4291 | `			}` |
|     25 | 4292 | `			if( flag_zeropad ){` |
|      - | 4293 | `				/* zero-padding works on strings too */` |
|    ! 0 | 4294 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4295 | `					spaces[idx] = '0';` |
|    ! 0 | 4296 | `				}` |
|    ! 0 | 4297 | `			}` |
|     25 | 4298 | `			break;` |
|     20 | 4299 | `		case PH7_FMT_RADIX:` |
|     41 | 4300 | `			pArg = NEXT_ARG;` |
|     41 | 4301 | `			if( pArg == 0 ){` |
|    ! 0 | 4302 | `				iVal = 0;` |
|    ! 0 | 4303 | `			}else{` |
|     41 | 4304 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 4305 | `			}` |
|      - | 4306 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|     41 | 4307 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 4308 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 4309 | `			}` |
|      - | 4310 | `#if 1` |
|      - | 4311 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 4312 | `        ** I think this is stupid.*/` |
|     41 | 4313 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 4314 | `#else` |
|      - | 4315 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 4316 | `        ** but leave the prefix for hex.*/` |
|      - | 4317 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 4318 | `#endif` |
|     41 | 4319 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|     23 | 4320 | `          if( iVal<0 ){` |
|      3 | 4321 | `            iVal = -iVal;` |
|      - | 4322 | `			/* Ticket 1433-003 */` |
|      3 | 4323 | `			if( iVal < 0 ){` |
|      - | 4324 | `				/* Overflow */` |
|    ! 0 | 4325 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4326 | `			}` |
|      3 | 4327 | `            prefix = '-';` |
|     22 | 4328 | `          }else if( flag_plussign )  prefix = '+';` |
|     19 | 4329 | `          else if( flag_blanksign )  prefix = ' ';` |
|     17 | 4330 | `          else                       prefix = 0;` |
|     12 | 4331 | `        }else{` |
|     19 | 4332 | `			if( iVal<0 ){` |
|    ! 0 | 4333 | `				iVal = -iVal;` |
|      - | 4334 | `				/* Ticket 1433-003 */` |
|    ! 0 | 4335 | `				if( iVal < 0 ){` |
|      - | 4336 | `					/* Overflow */` |
|    ! 0 | 4337 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4338 | `				}` |
|    ! 0 | 4339 | `			}` |
|     19 | 4340 | `			prefix = 0;` |
|      - | 4341 | `		}` |
|     41 | 4342 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      3 | 4343 | `          precision = width-(prefix!=0);` |
|      1 | 4344 | `        }` |
|     41 | 4345 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 4346 | `        {` |
|      - | 4347 | `          register char *cset;      /* Use registers for speed */` |
|      - | 4348 | `          register int base;` |
|     41 | 4349 | `          cset = pInfo->charset;` |
|     41 | 4350 | `          base = pInfo->base;` |
|     20 | 4351 | `          do{                                           /* Convert to ascii */` |
|     79 | 4352 | `            *(--zBuf) = cset[iVal%base];` |
|     79 | 4353 | `            iVal = iVal/base;` |
|     79 | 4354 | `          }while( iVal>0 );` |
|      - | 4355 | `        }` |
|     41 | 4356 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     55 | 4357 | `        for(idx=precision-length; idx>0; idx--){` |
|     15 | 4358 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|      8 | 4359 | `        }` |
|     41 | 4360 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|     41 | 4361 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 4362 | `          char *pre, x;` |
|      9 | 4363 | `          pre = pInfo->prefix;` |
|      9 | 4364 | `          if( *zBuf!=pre[0] ){` |
|     23 | 4365 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|      4 | 4366 | `          }` |
|      4 | 4367 | `        }` |
|     41 | 4368 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     41 | 4369 | `		break;` |
|     27 | 4370 | `		case PH7_FMT_FLOAT:` |
|      - | 4371 | `		case PH7_FMT_EXP:` |
|      - | 4372 | `		case PH7_FMT_GENERIC:{` |
|      - | 4373 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 4374 | `		long double realvalue;` |
|      - | 4375 | `		int  exp;                /* exponent of real numbers */` |
|      - | 4376 | `		double rounder;          /* Used for rounding floating point values */` |
|      - | 4377 | `		int flag_dp;            /* True if decimal point should be shown */` |
|      - | 4378 | `		int flag_rtz;           /* True if trailing zeros should be removed */` |
|      - | 4379 | `		int flag_exp;           /* True to force display of the exponent */` |
|      - | 4380 | `		int nsd;                 /* Number of significant digits returned */` |
|     55 | 4381 | `		pArg = NEXT_ARG;` |
|     55 | 4382 | `		if( pArg == 0 ){` |
|    ! 0 | 4383 | `			realvalue = 0;` |
|    ! 0 | 4384 | `		}else{` |
|     55 | 4385 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 4386 | `		}` |
|      - | 4387 | `		/* Special-case NaN and infinities since the normal formatting logic` |
|      - | 4388 | `		 * below assumes a finite positive realvalue. */` |
|     55 | 4389 | `		if( PH7_IS_NAN(realvalue) ){` |
|    ! 0 | 4390 | `			zBuf = "NAN";` |
|    ! 0 | 4391 | `			length = 3;` |
|    ! 0 | 4392 | `			break;` |
|      - | 4393 | `		}` |
|     55 | 4394 | `		if( PH7_IS_INF(realvalue) ){` |
|      - | 4395 | `			/* Infinity prints as INF or -INF depending on sign. */` |
|    ! 0 | 4396 | `			if( realvalue < 0.0 ){` |
|    ! 0 | 4397 | `				zBuf = "-INF";` |
|    ! 0 | 4398 | `				length = 4;` |
|    ! 0 | 4399 | `			}else{` |
|    ! 0 | 4400 | `				zBuf = "INF";` |
|    ! 0 | 4401 | `				length = 3;` |
|      - | 4402 | `			}` |
|    ! 0 | 4403 | `			break;` |
|      - | 4404 | `		}` |
|     55 | 4405 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|     55 | 4406 | `		if( precision>PH7_FMT_BUFSIZ-40) precision = PH7_FMT_BUFSIZ-40;` |
|     55 | 4407 | `        if( realvalue<0.0 ){` |
|    ! 0 | 4408 | `          realvalue = -realvalue;` |
|    ! 0 | 4409 | `          prefix = '-';` |
|    ! 0 | 4410 | `        }else{` |
|     55 | 4411 | `          if( flag_plussign )          prefix = '+';` |
|     55 | 4412 | `          else if( flag_blanksign )    prefix = ' ';` |
|     55 | 4413 | `          else                         prefix = 0;` |
|      - | 4414 | `        }` |
|     55 | 4415 | `        if( pInfo->type==PH7_FMT_GENERIC && precision>0 ) precision--;` |
|     55 | 4416 | `        rounder = 0.0;` |
|      - | 4417 | `#if 0` |
|      - | 4418 | `        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */` |
|      - | 4419 | `        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);` |
|      - | 4420 | `#else` |
|      - | 4421 | `        /* It makes more sense to use 0.5 */` |
|    387 | 4422 | `        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);` |
|      - | 4423 | `#endif` |
|     55 | 4424 | `        if( pInfo->type==PH7_FMT_FLOAT ) realvalue += rounder;` |
|      - | 4425 | `        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */` |
|     55 | 4426 | `        exp = 0;` |
|     55 | 4427 | `        if( realvalue>0.0 ){` |
|     59 | 4428 | `          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }` |
|     87 | 4429 | `          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }` |
|     59 | 4430 | `          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }` |
|     69 | 4431 | `          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }` |
|     55 | 4432 | `          if( exp>350 \|\| exp<-350 ){` |
|    ! 0 | 4433 | `            zBuf = "NaN";` |
|    ! 0 | 4434 | `            length = 3;` |
|    ! 0 | 4435 | `            break;` |
|      - | 4436 | `          }` |
|     27 | 4437 | `        }` |
|     55 | 4438 | `        zBuf = zWorker;` |
|      - | 4439 | `        /*` |
|      - | 4440 | `        ** If the field type is etGENERIC, then convert to either etEXP` |
|      - | 4441 | `        ** or etFLOAT, as appropriate.` |
|      - | 4442 | `        */` |
|     55 | 4443 | `        flag_exp = xtype==PH7_FMT_EXP;` |
|     55 | 4444 | `        if( xtype!=PH7_FMT_FLOAT ){` |
|    ! 0 | 4445 | `          realvalue += rounder;` |
|    ! 0 | 4446 | `          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }` |
|    ! 0 | 4447 | `        }` |
|     55 | 4448 | `        if( xtype==PH7_FMT_GENERIC ){` |
|    ! 0 | 4449 | `          flag_rtz = !flag_alternateform;` |
|    ! 0 | 4450 | `          if( exp<-4 \|\| exp>precision ){` |
|    ! 0 | 4451 | `            xtype = PH7_FMT_EXP;` |
|    ! 0 | 4452 | `          }else{` |
|    ! 0 | 4453 | `            precision = precision - exp;` |
|    ! 0 | 4454 | `            xtype = PH7_FMT_FLOAT;` |
|      - | 4455 | `          }` |
|    ! 0 | 4456 | `        }else{` |
|     55 | 4457 | `          flag_rtz = 0;` |
|      - | 4458 | `        }` |
|      - | 4459 | `        /*` |
|      - | 4460 | `        ** The "exp+precision" test causes output to be of type etEXP if` |
|      - | 4461 | `        ** the precision is too large to fit in buf[].` |
|      - | 4462 | `        */` |
|     55 | 4463 | `        nsd = 0;` |
|     55 | 4464 | `        if( xtype==PH7_FMT_FLOAT && exp+precision<PH7_FMT_BUFSIZ-30 ){` |
|     55 | 4465 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|     55 | 4466 | `          if( prefix ) *(zBuf++) = (char)prefix;         /* Sign */` |
|     55 | 4467 | `          if( exp<0 )  *(zBuf++) = '0';            /* Digits before "." */` |
|    145 | 4468 | `          else for(; exp>=0; exp--) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     55 | 4469 | `          if( flag_dp ) *(zBuf++) = '.';           /* The decimal point */` |
|     87 | 4470 | `          for(exp++; exp<0 && precision>0; precision--, exp++){` |
|     33 | 4471 | `            *(zBuf++) = '0';` |
|     17 | 4472 | `          }` |
|    355 | 4473 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     55 | 4474 | `          *(zBuf--) = 0;                           /* Null terminate */` |
|     55 | 4475 | `          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */` |
|    ! 0 | 4476 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 4477 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 4478 | `          }` |
|     55 | 4479 | `          zBuf++;                            /* point to next free slot */` |
|     28 | 4480 | `        }else{    /* etEXP or etGENERIC */` |
|    ! 0 | 4481 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|    ! 0 | 4482 | `          if( prefix ) *(zBuf++) = (char)prefix;   /* Sign */` |
|    ! 0 | 4483 | `          *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);  /* First digit */` |
|    ! 0 | 4484 | `          if( flag_dp ) *(zBuf++) = '.';     /* Decimal point */` |
|    ! 0 | 4485 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|    ! 0 | 4486 | `          zBuf--;                            /* point to last digit */` |
|    ! 0 | 4487 | `          if( flag_rtz && flag_dp ){          /* Remove tail zeros */` |
|    ! 0 | 4488 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 4489 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 4490 | `          }` |
|    ! 0 | 4491 | `          zBuf++;                            /* point to next free slot */` |
|    ! 0 | 4492 | `          if( exp \|\| flag_exp ){` |
|    ! 0 | 4493 | `            *(zBuf++) = pInfo->charset[0];` |
|    ! 0 | 4494 | `            if( exp<0 ){ *(zBuf++) = '-'; exp = -exp; } /* sign of exp */` |
|    ! 0 | 4495 | `            else       { *(zBuf++) = '+'; }` |
|    ! 0 | 4496 | `            if( exp>=100 ){` |
|    ! 0 | 4497 | `              *(zBuf++) = (char)((exp/100)+'0');                /* 100's digit */` |
|    ! 0 | 4498 | `              exp %= 100;` |
|    ! 0 | 4499 | `            }` |
|    ! 0 | 4500 | `            *(zBuf++) = (char)(exp/10+'0');                     /* 10's digit */` |
|    ! 0 | 4501 | `            *(zBuf++) = (char)(exp%10+'0');                     /* 1's digit */` |
|    ! 0 | 4502 | `          }` |
|      - | 4503 | `        }` |
|      - | 4504 | `        /* The converted number is in buf[] and zero terminated.Output it.` |
|      - | 4505 | `        ** Note that the number is in the usual order, not reversed as with` |
|      - | 4506 | `        ** integer conversions.*/` |
|     55 | 4507 | `        length = (int)(zBuf-zWorker);` |
|     55 | 4508 | `        zBuf = zWorker;` |
|      - | 4509 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 4510 | `        ** set and we are not left justified */` |
|     55 | 4511 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 4512 | `          int i;` |
|      3 | 4513 | `          int nPad = width - length;` |
|     13 | 4514 | `          for(i=width; i>=nPad; i--){` |
|     11 | 4515 | `            zBuf[i] = zBuf[i-nPad];` |
|      6 | 4516 | `          }` |
|      3 | 4517 | `          i = prefix!=0;` |
|      5 | 4518 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      3 | 4519 | `          length = width;` |
|      1 | 4520 | `        }` |
|      - | 4521 | `#else` |
|      - | 4522 | `         zBuf = " ";` |
|      - | 4523 | `		 length = (int)sizeof(char);` |
|      - | 4524 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|     55 | 4525 | `		 break;` |
|      - | 4526 | `							 }` |
|      1 | 4527 | `		default:` |
|      - | 4528 | `			/* Invalid format specifer */` |
|      3 | 4529 | `			zWorker[0] = '?';` |
|      3 | 4530 | `			length = (int)sizeof(char);` |
|      2 | 4531 | `			break;` |
|      - | 4532 | `		}` |
|      - | 4533 | `		 /*` |
|      - | 4534 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 4535 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 4536 | `		 ** the output.` |
|      - | 4537 | `		 */` |
|    127 | 4538 | `    if( !flag_leftjustify ){` |
|      - | 4539 | `      register int nspace;` |
|    119 | 4540 | `      nspace = width-length;` |
|    119 | 4541 | `      if( nspace>0 ){` |
|      5 | 4542 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4543 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4544 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4545 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4546 | `			}` |
|    ! 0 | 4547 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4548 | `        }` |
|      5 | 4549 | `        if( nspace>0 ){` |
|      5 | 4550 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      5 | 4551 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4552 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4553 | `			}` |
|      2 | 4554 | `		}` |
|      2 | 4555 | `      }` |
|     59 | 4556 | `    }` |
|    127 | 4557 | `    if( length>0 ){` |
|    127 | 4558 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    127 | 4559 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4560 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4561 | `		}` |
|     63 | 4562 | `    }` |
|    127 | 4563 | `    if( flag_leftjustify ){` |
|      - | 4564 | `      register int nspace;` |
|      9 | 4565 | `      nspace = width-length;` |
|      9 | 4566 | `      if( nspace>0 ){` |
|      9 | 4567 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4568 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4569 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4570 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4571 | `			}` |
|    ! 0 | 4572 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4573 | `        }` |
|      9 | 4574 | `        if( nspace>0 ){` |
|      9 | 4575 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      9 | 4576 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4577 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4578 | `			}` |
|      4 | 4579 | `		}` |
|      4 | 4580 | `      }` |
|      4 | 4581 | `    }` |
|      1 | 4582 | ` }/* for(;;) */` |
|    121 | 4583 | `	return SXRET_OK;` |
|     61 | 4584 |  |
|      - | 4585 | `/*` |
|      - | 4586 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 4587 | ` */` |
|     84 | 4588 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4589 |  |
|      - | 4590 | `	/* Consume directly */` |
|     85 | 4591 | `	ph7_result_string(pCtx,zInput,nLen);` |
|     42 | 4592 | `	SXUNUSED(pUserData); /* cc warning */` |
|     85 | 4593 | `	return PH7_OK;` |
|      1 | 4594 |  |
|      - | 4595 | `/*` |
|      - | 4596 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 4597 | ` *  Return a formatted string.` |
|      - | 4598 | ` * Parameters` |
|      - | 4599 | ` *  $format` |
|      - | 4600 | ` *    The format string (see block comment above)` |
|      - | 4601 | ` * Return` |
|      - | 4602 | ` *  A string produced according to the formatting string format.` |
|      - | 4603 | ` */` |
|     56 | 4604 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4605 |  |
|      - | 4606 | `	const char *zFormat;` |
|      - | 4607 | `	int nLen;` |
|     57 | 4608 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4609 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 4610 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4611 | `		return PH7_OK;` |
|      - | 4612 | `	}` |
|      - | 4613 | `	/* Extract the string format */` |
|     55 | 4614 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     55 | 4615 | `	if( nLen < 1 ){` |
|      - | 4616 | `		/* Empty string */` |
|    ! 0 | 4617 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4618 | `		return PH7_OK;` |
|      - | 4619 | `	}` |
|      - | 4620 | `	/* Format the string */` |
|     55 | 4621 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,0,FALSE);` |
|     55 | 4622 | `	return PH7_OK;` |
|     29 | 4623 |  |
|      - | 4624 | `/*` |
|      - | 4625 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 4626 | ` */` |
|    110 | 4627 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4628 |  |
|    111 | 4629 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 4630 | `	/* Call the VM output consumer directly */` |
|    111 | 4631 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 4632 | `	/* Increment counter */` |
|    111 | 4633 | `	*pCounter += nLen;` |
|    111 | 4634 | `	return PH7_OK;` |
|      1 | 4635 |  |
|      - | 4636 | `/*` |
|      - | 4637 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 4638 | ` *  Output a formatted string.` |
|      - | 4639 | ` * Parameters` |
|      - | 4640 | ` *  $format` |
|      - | 4641 | ` *   See sprintf() for a description of format.` |
|      - | 4642 | ` * Return` |
|      - | 4643 | ` *  The length of the outputted string.` |
|      - | 4644 | ` */` |
|     42 | 4645 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4646 |  |
|     43 | 4647 | `	ph7_int64 nCounter = 0;` |
|      - | 4648 | `	const char *zFormat;` |
|      - | 4649 | `	int nLen;` |
|     43 | 4650 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4651 | `		/* Missing/Invalid arguments,return 0 */` |
|      3 | 4652 | `		ph7_result_int(pCtx,0);` |
|      3 | 4653 | `		return PH7_OK;` |
|      - | 4654 | `	}` |
|      - | 4655 | `	/* Extract the string format */` |
|     41 | 4656 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 4657 | `	if( nLen < 1 ){` |
|      - | 4658 | `		/* Empty string */` |
|    ! 0 | 4659 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4660 | `		return PH7_OK;` |
|      - | 4661 | `	}` |
|      - | 4662 | `	/* Format the string */` |
|     41 | 4663 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 4664 | `	/* Return the length of the outputted string */` |
|     41 | 4665 | `	ph7_result_int64(pCtx,nCounter);` |
|     41 | 4666 | `	return PH7_OK;` |
|     22 | 4667 |  |
|      - | 4668 | `/*` |
|      - | 4669 | ` * int vprintf(string $format,array $args)` |
|      - | 4670 | ` *  Output a formatted string.` |
|      - | 4671 | ` * Parameters` |
|      - | 4672 | ` *  $format` |
|      - | 4673 | ` *   See sprintf() for a description of format.` |
|      - | 4674 | ` * Return` |
|      - | 4675 | ` *  The length of the outputted string.` |
|      - | 4676 | ` */` |
|      2 | 4677 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4678 |  |
|      3 | 4679 | `	ph7_int64 nCounter = 0;` |
|      - | 4680 | `	const char *zFormat;` |
|      - | 4681 | `	ph7_hashmap *pMap;` |
|      - | 4682 | `	SySet sArg;` |
|      - | 4683 | `	int nLen,n;` |
|      3 | 4684 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4685 | `		/* Missing/Invalid arguments,return 0 */` |
|    ! 0 | 4686 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4687 | `		return PH7_OK;` |
|      - | 4688 | `	}` |
|      - | 4689 | `	/* Extract the string format */` |
|      3 | 4690 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4691 | `	if( nLen < 1 ){` |
|      - | 4692 | `		/* Empty string */` |
|    ! 0 | 4693 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4694 | `		return PH7_OK;` |
|      - | 4695 | `	}` |
|      - | 4696 | `	/* Point to the hashmap */` |
|      3 | 4697 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4698 | `	/* Extract arguments from the hashmap */` |
|      3 | 4699 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4700 | `	/* Format the string */` |
|      3 | 4701 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 4702 | `	/* Return the length of the outputted string */` |
|      3 | 4703 | `	ph7_result_int64(pCtx,nCounter);` |
|      - | 4704 | `	/* Release the container */` |
|      3 | 4705 | `	SySetRelease(&sArg);` |
|      3 | 4706 | `	return PH7_OK;` |
|      2 | 4707 |  |
|      - | 4708 | `/*` |
|      - | 4709 | ` * int vsprintf(string $format,array $args)` |
|      - | 4710 | ` *  Output a formatted string.` |
|      - | 4711 | ` * Parameters` |
|      - | 4712 | ` *  $format` |
|      - | 4713 | ` *   See sprintf() for a description of format.` |
|      - | 4714 | ` * Return` |
|      - | 4715 | ` *  A string produced according to the formatting string format.` |
|      - | 4716 | ` */` |
|     10 | 4717 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4718 |  |
|      - | 4719 | `	const char *zFormat;` |
|      - | 4720 | `	ph7_hashmap *pMap;` |
|      - | 4721 | `	SySet sArg;` |
|      - | 4722 | `	int nLen,n;` |
|     11 | 4723 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4724 | `		/* Missing/Invalid arguments,return the empty string */` |
|      5 | 4725 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 4726 | `		return PH7_OK;` |
|      - | 4727 | `	}` |
|      - | 4728 | `	/* Extract the string format */` |
|      7 | 4729 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4730 | `	if( nLen < 1 ){` |
|      - | 4731 | `		/* Empty string */` |
|    ! 0 | 4732 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4733 | `		return PH7_OK;` |
|      - | 4734 | `	}` |
|      - | 4735 | `	/* Point to hashmap */` |
|      7 | 4736 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4737 | `	/* Extract arguments from the hashmap */` |
|      7 | 4738 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4739 | `	/* Format the string */` |
|      7 | 4740 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),0,TRUE);` |
|      - | 4741 | `	/* Release the container */` |
|      7 | 4742 | `	SySetRelease(&sArg);` |
|      7 | 4743 | `	return PH7_OK;` |
|      6 | 4744 |  |
|      - | 4745 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 4746 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4747 | `/*` |
|      - | 4748 | ` * Symisc eXtension.` |
|      - | 4749 | ` * string size_format(int64 $size)` |
|      - | 4750 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 4751 | ` *  Example:` |
|      - | 4752 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 4753 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 4754 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 4755 | ` * Parameter` |
|      - | 4756 | ` *  $size` |
|      - | 4757 | ` *    Entity size in bytes.` |
|      - | 4758 | ` * Return` |
|      - | 4759 | ` *   Formatted string representation of the given size.` |
|      - | 4760 | ` */` |
|     24 | 4761 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4762 |  |
|      - | 4763 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 4764 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 4765 | `	sxi32 nRest,i_32;` |
|      - | 4766 | `	ph7_int64 iSize;` |
|     25 | 4767 | `	int c = -1; /* index in zUnit[] */` |
|      - | 4768 |  |
|     25 | 4769 | `	if( nArg < 1 ){` |
|      - | 4770 | `		/* Missing argument,return the empty string */` |
|      3 | 4771 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4772 | `		return PH7_OK;` |
|      - | 4773 | `	}` |
|      - | 4774 | `	/* Extract the given size */` |
|     23 | 4775 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 4776 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 4777 | `		/* Don't bother formatting,return immediately */` |
|      5 | 4778 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 4779 | `		return PH7_OK;` |
|      - | 4780 | `	}` |
|     19 | 4781 | `	for(;;){` |
|     39 | 4782 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 4783 | `		iSize >>= 10;` |
|     39 | 4784 | `		c++;` |
|     39 | 4785 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 4786 | `			break;` |
|      - | 4787 | `		}` |
|      1 | 4788 | `	}` |
|     19 | 4789 | `	nRest /= 100;` |
|     19 | 4790 | `	if( nRest > 9 ){` |
|    ! 0 | 4791 | `		nRest = 9;` |
|    ! 0 | 4792 | `	}` |
|     19 | 4793 | `	if( iSize > 999 ){` |
|    ! 0 | 4794 | `		c++;` |
|    ! 0 | 4795 | `		nRest = 9;` |
|    ! 0 | 4796 | `		iSize = 0;` |
|    ! 0 | 4797 | `	}` |
|     19 | 4798 | `	i_32 = (sxi32)iSize;` |
|      - | 4799 | `	/* Format */` |
|     19 | 4800 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 4801 | `	return PH7_OK;` |
|     13 | 4802 |  |
|      - | 4803 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4804 | `/*` |
|      - | 4805 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 4806 | ` *   Calculate the md5 hash of a string.` |
|      - | 4807 | ` * Parameter` |
|      - | 4808 | ` *  $str` |
|      - | 4809 | ` *   Input string` |
|      - | 4810 | ` * $raw_output` |
|      - | 4811 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4812 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4813 | ` * Return` |
|      - | 4814 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 4815 | ` */` |
|     10 | 4816 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4817 |  |
|      - | 4818 | `	unsigned char zDigest[16];` |
|     11 | 4819 | `	int raw_output = FALSE;` |
|      - | 4820 | `	const void *pIn;` |
|      - | 4821 | `	int nLen;` |
|     11 | 4822 | `	if( nArg < 1 ){` |
|      - | 4823 | `		/* Missing arguments,return the empty string */` |
|      3 | 4824 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4825 | `		return PH7_OK;` |
|      - | 4826 | `	}` |
|      - | 4827 | `	/* Extract the input string */` |
|      9 | 4828 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 4829 | `	if( nLen < 1 ){` |
|      - | 4830 | `		/* Empty string */` |
|    ! 0 | 4831 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4832 | `		return PH7_OK;` |
|      - | 4833 | `	}` |
|      9 | 4834 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      3 | 4835 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 | 4836 | `	}` |
|      - | 4837 | `	/* Compute the MD5 digest */` |
|      9 | 4838 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|      9 | 4839 | `	if( raw_output ){` |
|      - | 4840 | `		/* Output raw digest */` |
|      3 | 4841 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      2 | 4842 | `	}else{` |
|      - | 4843 | `		/* Perform a binary to hex conversion */` |
|      7 | 4844 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4845 | `	}` |
|      9 | 4846 | `	return PH7_OK;` |
|      6 | 4847 |  |
|      - | 4848 | `/*` |
|      - | 4849 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 4850 | ` *   Calculate the sha1 hash of a string.` |
|      - | 4851 | ` * Parameter` |
|      - | 4852 | ` *  $str` |
|      - | 4853 | ` *   Input string` |
|      - | 4854 | ` * $raw_output` |
|      - | 4855 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4856 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4857 | ` * Return` |
|      - | 4858 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 4859 | ` */` |
|      8 | 4860 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4861 |  |
|      - | 4862 | `	unsigned char zDigest[20];` |
|      9 | 4863 | `	int raw_output = FALSE;` |
|      - | 4864 | `	const void *pIn;` |
|      - | 4865 | `	int nLen;` |
|      9 | 4866 | `	if( nArg < 1 ){` |
|      - | 4867 | `		/* Missing arguments,return the empty string */` |
|      3 | 4868 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4869 | `		return PH7_OK;` |
|      - | 4870 | `	}` |
|      - | 4871 | `	/* Extract the input string */` |
|      7 | 4872 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4873 | `	if( nLen < 1 ){` |
|      - | 4874 | `		/* Empty string */` |
|    ! 0 | 4875 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4876 | `		return PH7_OK;` |
|      - | 4877 | `	}` |
|      7 | 4878 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      3 | 4879 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 | 4880 | `	}` |
|      - | 4881 | `	/* Compute the SHA1 digest */` |
|      7 | 4882 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|      7 | 4883 | `	if( raw_output ){` |
|      - | 4884 | `		/* Output raw digest */` |
|      3 | 4885 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      2 | 4886 | `	}else{` |
|      - | 4887 | `		/* Perform a binary to hex conversion */` |
|      5 | 4888 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4889 | `	}` |
|      7 | 4890 | `	return PH7_OK;` |
|      5 | 4891 |  |
|      - | 4892 | `/*` |
|      - | 4893 | ` * int64 crc32(string $str)` |
|      - | 4894 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 4895 | ` * Parameter` |
|      - | 4896 | ` *  $str` |
|      - | 4897 | ` *   Input string` |
|      - | 4898 | ` * Return` |
|      - | 4899 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 4900 | ` */` |
|      4 | 4901 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4902 |  |
|      - | 4903 | `	const void *pIn;` |
|      - | 4904 | `	sxu32 nCRC;` |
|      - | 4905 | `	int nLen;` |
|      5 | 4906 | `	if( nArg < 1 ){` |
|      - | 4907 | `		/* Missing arguments,return 0 */` |
|      3 | 4908 | `		ph7_result_int(pCtx,0);` |
|      3 | 4909 | `		return PH7_OK;` |
|      - | 4910 | `	}` |
|      - | 4911 | `	/* Extract the input string */` |
|      3 | 4912 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4913 | `	if( nLen < 1 ){` |
|      - | 4914 | `		/* Empty string */` |
|    ! 0 | 4915 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4916 | `		return PH7_OK;` |
|      - | 4917 | `	}` |
|      - | 4918 | `	/* Calculate the sum */` |
|      3 | 4919 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 4920 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 4921 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 4922 | `	return PH7_OK;` |
|      3 | 4923 |  |
|      - | 4924 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 4925 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4926 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 4927 | `/*` |
|      - | 4928 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 4929 |  |
|      - | 4930 | ` */` |
|      4 | 4931 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 4932 | `	const char *zInput, /* Raw input */` |
|      - | 4933 | `	int nByte,  /* Input length */` |
|      - | 4934 | `	int delim,  /* Delimiter */` |
|      - | 4935 | `	int encl,   /* Enclosure */` |
|      - | 4936 | `	int escape,  /* Escape character */` |
|      - | 4937 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 4938 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 4939 | `	)` |
|      1 | 4940 |  |
|      5 | 4941 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 4942 | `	const char *zIn = zInput;` |
|      - | 4943 | `	const char *zPtr;` |
|      - | 4944 | `	int isEnc;` |
|      - | 4945 | `	/* Start processing */` |
|      8 | 4946 | `	for(;;){` |
|     17 | 4947 | `		if( zIn >= zEnd ){` |
|      - | 4948 | `			/* No more input to process */` |
|      5 | 4949 | `			break;` |
|      - | 4950 | `		}` |
|     13 | 4951 | `		isEnc = 0;` |
|     13 | 4952 | `		zPtr = zIn;` |
|      - | 4953 | `		/* Find the first delimiter */` |
|     27 | 4954 | `		while( zIn < zEnd ){` |
|     23 | 4955 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 4956 | `				/* Delimiter found,break imediately */` |
|      5 | 4957 | `				break;` |
|     15 | 4958 | `			}else if( zIn[0] == encl ){` |
|      - | 4959 | `				/* Inside enclosure? */` |
|    ! 0 | 4960 | `				isEnc = !isEnc;` |
|     15 | 4961 | `			}else if( zIn[0] == escape ){` |
|      - | 4962 | `				/* Escape sequence */` |
|    ! 0 | 4963 | `				zIn++;` |
|    ! 0 | 4964 | `			}` |
|      - | 4965 | `			/* Advance the cursor */` |
|     15 | 4966 | `			zIn++;` |
|      1 | 4967 | `		}` |
|     13 | 4968 | `		if( zIn > zPtr ){` |
|     13 | 4969 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 4970 | `			sxi32 rc;` |
|      - | 4971 | `			/* Invoke the supllied callback */` |
|     13 | 4972 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 4973 | `				zPtr++;` |
|    ! 0 | 4974 | `				nByteChunk-=2;` |
|    ! 0 | 4975 | `			}` |
|     13 | 4976 | `			if( nByteChunk > 0 ){` |
|     13 | 4977 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 4978 | `				if( rc == SXERR_ABORT ){` |
|      - | 4979 | `					/* User callback request an operation abort */` |
|    ! 0 | 4980 | `					break;` |
|      - | 4981 | `				}` |
|      6 | 4982 | `			}` |
|      6 | 4983 | `		}` |
|      - | 4984 | `		/* Ignore trailing delimiter */` |
|     21 | 4985 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 4986 | `			zIn++;` |
|      1 | 4987 | `		}` |
|      1 | 4988 | `	}` |
|      5 | 4989 | `	return SXRET_OK;` |
|      1 | 4990 |  |
|      - | 4991 | `/*` |
|      - | 4992 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 4993 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 4994 | ` * argument to this callback.` |
|      - | 4995 | ` */` |
|     12 | 4996 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 4997 |  |
|     13 | 4998 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 4999 | `	ph7_value sEntry;` |
|      - | 5000 | `	SyString sToken;` |
|      - | 5001 | `	/* Insert the token in the given array */` |
|     13 | 5002 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 5003 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 5004 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 5005 | `	if( sToken.nByte < 1){` |
|    ! 0 | 5006 | `		return SXRET_OK;` |
|      - | 5007 | `	}` |
|     13 | 5008 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 5009 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 5010 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 5011 | `	return SXRET_OK;` |
|      7 | 5012 |  |
|      - | 5013 | `/*` |
|      - | 5014 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 5015 | ` *  Parse a CSV string into an array.` |
|      - | 5016 | ` * Parameters` |
|      - | 5017 | ` *  $input` |
|      - | 5018 | ` *   The string to parse.` |
|      - | 5019 | ` *  $delimiter` |
|      - | 5020 | ` *   Set the field delimiter (one character only).` |
|      - | 5021 | ` *  $enclosure` |
|      - | 5022 | ` *   Set the field enclosure character (one character only).` |
|      - | 5023 | ` *  $escape` |
|      - | 5024 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 5025 | ` * Return` |
|      - | 5026 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 5027 | ` */` |
|      4 | 5028 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5029 |  |
|      - | 5030 | `	const char *zInput,*zPtr;` |
|      - | 5031 | `	ph7_value *pArray;` |
|      5 | 5032 | `	int delim  = ',';   /* Delimiter */` |
|      5 | 5033 | `	int encl   = '"' ;  /* Enclosure */` |
|      5 | 5034 | `	int escape = '\\';  /* Escape character */` |
|      - | 5035 | `	int nLen;` |
|      5 | 5036 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5037 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 5038 | `		ph7_result_null(pCtx);` |
|      3 | 5039 | `		return PH7_OK;` |
|      - | 5040 | `	}` |
|      - | 5041 | `	/* Extract the raw input */` |
|      3 | 5042 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5043 | `	if( nArg > 1 ){` |
|      - | 5044 | `		int i;` |
|      3 | 5045 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 5046 | `			/* Extract the delimiter */` |
|      3 | 5047 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 5048 | `			if( i > 0 ){` |
|      3 | 5049 | `				delim = zPtr[0];` |
|      1 | 5050 | `			}` |
|      1 | 5051 | `		}` |
|      3 | 5052 | `		if( nArg > 2 ){` |
|      3 | 5053 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 5054 | `				/* Extract the enclosure */` |
|      3 | 5055 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 5056 | `				if( i > 0 ){` |
|      3 | 5057 | `					encl = zPtr[0];` |
|      1 | 5058 | `				}` |
|      1 | 5059 | `			}` |
|      3 | 5060 | `			if( nArg > 3 ){` |
|      3 | 5061 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 5062 | `					/* Extract the escape character */` |
|      3 | 5063 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 5064 | `					if( i > 0 ){` |
|      3 | 5065 | `						escape = zPtr[0];` |
|      1 | 5066 | `					}` |
|      1 | 5067 | `				}` |
|      1 | 5068 | `			}` |
|      1 | 5069 | `		}` |
|      1 | 5070 | `	}` |
|      - | 5071 | `	/* Create our array */` |
|      3 | 5072 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5073 | `	if( pArray == 0 ){` |
|    ! 0 | 5074 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 5075 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5076 | `		return PH7_OK;` |
|      - | 5077 | `	}` |
|      - | 5078 | `	/* Parse the raw input */` |
|      3 | 5079 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 5080 | `	/* Return the freshly created array */` |
|      3 | 5081 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5082 | `	return PH7_OK;` |
|      3 | 5083 |  |
|      - | 5084 | `/*` |
|      - | 5085 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 5086 | ` * container.` |
|      - | 5087 | ` * Refer to [strip_tags()].` |
|      - | 5088 | ` */` |
|     10 | 5089 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5090 |  |
|     11 | 5091 | `	const char *zEnd = &zTag[nByte];` |
|      - | 5092 | `	const char *zPtr;` |
|      - | 5093 | `	SyString sEntry;` |
|      - | 5094 | `	/* Strip tags */` |
|     10 | 5095 | `	for(;;){` |
|     45 | 5096 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 5097 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 5098 | `				zTag++;` |
|      1 | 5099 | `		}` |
|     21 | 5100 | `		if( zTag >= zEnd ){` |
|     11 | 5101 | `			break;` |
|      - | 5102 | `		}` |
|     11 | 5103 | `		zPtr = zTag;` |
|      - | 5104 | `		/* Delimit the tag */` |
|     25 | 5105 | `		while(zTag < zEnd ){` |
|     25 | 5106 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5107 | `				/* UTF-8 stream */` |
|      3 | 5108 | `				zTag++;` |
|      5 | 5109 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 5110 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 5111 | `				break;` |
|    ! 0 | 5112 | `			}else{` |
|     13 | 5113 | `				zTag++;` |
|      - | 5114 | `			}` |
|      1 | 5115 | `		}` |
|     11 | 5116 | `		if( zTag > zPtr ){` |
|      - | 5117 | `			/* Perform the insertion */` |
|     11 | 5118 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 5119 | `			SyStringFullTrim(&sEntry);` |
|     11 | 5120 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 5121 | `		}` |
|      - | 5122 | `		/* Jump the trailing '>' */` |
|     11 | 5123 | `		zTag++;` |
|      1 | 5124 | `	}` |
|     11 | 5125 | `	return SXRET_OK;` |
|      1 | 5126 |  |
|      - | 5127 | `/*` |
|      - | 5128 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 5129 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 5130 | ` * Refer to [strip_tags()].` |
|      - | 5131 | ` */` |
|     36 | 5132 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5133 |  |
|     37 | 5134 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 5135 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 5136 | `		SyString sTag;` |
|     85 | 5137 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 5138 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 5139 | `			zTag++;` |
|      1 | 5140 | `		}` |
|      - | 5141 | `		/* Delimit the tag */` |
|     25 | 5142 | `		zCur = zTag;` |
|     77 | 5143 | `		while(zTag < zEnd ){` |
|     77 | 5144 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5145 | `				/* UTF-8 stream */` |
|      5 | 5146 | `				zTag++;` |
|      9 | 5147 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 5148 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 5149 | `				break;` |
|    ! 0 | 5150 | `			}else{` |
|     49 | 5151 | `				zTag++;` |
|      - | 5152 | `			}` |
|      1 | 5153 | `		}` |
|     25 | 5154 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 5155 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 5156 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 5157 | `		if( sTag.nByte > 0 ){` |
|      - | 5158 | `			SyString *aEntry,*pEntry;` |
|      - | 5159 | `			sxi32 rc;` |
|      - | 5160 | `			sxu32 n;` |
|      - | 5161 | `			/* Perform the lookup */` |
|     25 | 5162 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 5163 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 5164 | `				pEntry = &aEntry[n];` |
|      - | 5165 | `				/* Do the comparison */` |
|     25 | 5166 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 5167 | `				if( !rc ){` |
|     21 | 5168 | `					return SXRET_OK;` |
|      - | 5169 | `				}` |
|      3 | 5170 | `			}` |
|      2 | 5171 | `		}` |
|      2 | 5172 | `	}` |
|      - | 5173 | `	/* No such tag */` |
|     17 | 5174 | `	return SXERR_NOTFOUND;` |
|     19 | 5175 |  |
|      - | 5176 | `/*` |
|      - | 5177 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 5178 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 5179 | ` * Refer to [strip_tags()].` |
|      - | 5180 | ` */` |
|     16 | 5181 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 5182 |  |
|     17 | 5183 | `	const char *zEnd = &zIn[nByte];` |
|      - | 5184 | `	const char *zPtr,*zTag;` |
|      - | 5185 | `	SySet sSet;` |
|      - | 5186 | `	/* initialize the set of allowed tags */` |
|     17 | 5187 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 5188 | `	if( nTaglen > 0 ){` |
|      - | 5189 | `		/* Set of allowed tags */` |
|     11 | 5190 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 5191 | `	}` |
|      - | 5192 | `	/* Set the empty string */` |
|     17 | 5193 | `	ph7_result_string(pCtx,"",0);` |
|      - | 5194 | `	/* Start processing */` |
|     26 | 5195 | `	for(;;){` |
|     53 | 5196 | `		if(zIn >= zEnd){` |
|      - | 5197 | `			/* No more input to process */` |
|     15 | 5198 | `			break;` |
|      - | 5199 | `		}` |
|     39 | 5200 | `		zPtr = zIn;` |
|      - | 5201 | `		/* Find a tag */` |
|    133 | 5202 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 5203 | `			zIn++;` |
|      1 | 5204 | `		}` |
|     39 | 5205 | `		if( zIn > zPtr ){` |
|      - | 5206 | `			/* Consume raw input */` |
|     21 | 5207 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 5208 | `		}` |
|      - | 5209 | `		/* Ignore trailing null bytes */` |
|     39 | 5210 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 5211 | `			zIn++;` |
|    ! 0 | 5212 | `		}` |
|     39 | 5213 | `		if(zIn >= zEnd){` |
|      - | 5214 | `			/* No more input to process */` |
|      3 | 5215 | `			break;` |
|      - | 5216 | `		}` |
|     37 | 5217 | `		if( zIn[0] == '<' ){` |
|      - | 5218 | `			sxi32 rc;` |
|     37 | 5219 | `			zTag = zIn++;` |
|      - | 5220 | `			/* Delimit the tag */` |
|    127 | 5221 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 5222 | `				zIn++;` |
|      1 | 5223 | `			}` |
|     37 | 5224 | `			if( zIn < zEnd ){` |
|     37 | 5225 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 5226 | `			}` |
|      - | 5227 | `			/* Query the set */` |
|     37 | 5228 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 5229 | `			if( rc == SXRET_OK ){` |
|      - | 5230 | `				/* Keep the tag */` |
|     21 | 5231 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 5232 | `			}` |
|     18 | 5233 | `		}` |
|      1 | 5234 | `	}` |
|      - | 5235 | `	/* Cleanup */` |
|     17 | 5236 | `	SySetRelease(&sSet);` |
|     17 | 5237 | `	return SXRET_OK;` |
|      1 | 5238 |  |
|      - | 5239 | `/*` |
|      - | 5240 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 5241 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 5242 | ` * Parameters` |
|      - | 5243 | ` *  $str` |
|      - | 5244 | ` *  The input string.` |
|      - | 5245 | ` * $allowable_tags` |
|      - | 5246 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 5247 | ` * Return` |
|      - | 5248 | ` *  Returns the stripped string.` |
|      - | 5249 | ` */` |
|     16 | 5250 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5251 |  |
|     17 | 5252 | `	const char *zTaglist = 0;` |
|      - | 5253 | `	const char *zString;` |
|     17 | 5254 | `	int nTaglen = 0;` |
|      - | 5255 | `	int nLen;` |
|     17 | 5256 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5257 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 5258 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5259 | `		return PH7_OK;` |
|      - | 5260 | `	}` |
|      - | 5261 | `	/* Point to the raw string */` |
|     15 | 5262 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 5263 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 5264 | `		/* Allowed tag */` |
|     11 | 5265 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 5266 | `	}` |
|      - | 5267 | `	/* Process input */` |
|     15 | 5268 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 5269 | `	return PH7_OK;` |
|      9 | 5270 |  |
|      - | 5271 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5272 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5273 | `/*` |
|      - | 5274 | ` * string str_shuffle(string $str)` |
|      - | 5275 |  |
|      - | 5276 | ` *  Randomly shuffles a string.` |
|      - | 5277 | ` * Parameters` |
|      - | 5278 | ` *  $str` |
|      - | 5279 | ` *   The input string.` |
|      - | 5280 | ` * Return` |
|      - | 5281 | ` *  Returns the shuffled string.` |
|      - | 5282 | ` */` |
|     12 | 5283 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5284 |  |
|      - | 5285 | `	const char *zString;` |
|      - | 5286 | `	int nLen,i,c;` |
|      - | 5287 | `	sxu32 iR;` |
|     13 | 5288 | `	if( nArg < 1 ){` |
|      - | 5289 | `		/* Missing arguments,return the empty string */` |
|      3 | 5290 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5291 | `		return PH7_OK;` |
|      - | 5292 | `	}` |
|      - | 5293 | `	/* Extract the target string */` |
|     11 | 5294 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5295 | `	if( nLen < 1 ){` |
|      - | 5296 | `		/* Nothing to shuffle */` |
|      3 | 5297 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5298 | `		return PH7_OK;` |
|      - | 5299 | `	}` |
|      - | 5300 | `	/* Shuffle the string */` |
|     43 | 5301 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 5302 | `		/* Generate a random number first */` |
|     35 | 5303 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 5304 | `		/* Extract a random offset */` |
|     35 | 5305 | `		c = zString[iR % nLen];` |
|      - | 5306 | `		/* Append it */` |
|     35 | 5307 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 5308 | `	}` |
|      9 | 5309 | `	return PH7_OK;` |
|      7 | 5310 |  |
|      - | 5311 | `/*` |
|      - | 5312 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 5313 | ` *  Convert a string to an array.` |
|      - | 5314 | ` * Parameters` |
|      - | 5315 | ` * $str` |
|      - | 5316 | ` *  The input string.` |
|      - | 5317 | ` * $split_length` |
|      - | 5318 | ` *  Maximum length of the chunk.` |
|      - | 5319 | ` * Return` |
|      - | 5320 | ` *  If the optional split_length parameter is specified, the returned array` |
|      - | 5321 | ` *  will be broken down into chunks with each being split_length in length, otherwise` |
|      - | 5322 | ` *  each chunk will be one character in length. FALSE is returned if split_length is less than 1.` |
|      - | 5323 | ` *  If the split_length length exceeds the length of string, the entire string is returned` |
|      - | 5324 | ` *  as the first (and only) array element.` |
|      - | 5325 | ` */` |
|      8 | 5326 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5327 |  |
|      - | 5328 | `	const char *zString,*zEnd;` |
|      - | 5329 | `	ph7_value *pArray,*pValue;` |
|      - | 5330 | `	int split_len;` |
|      - | 5331 | `	int nLen;` |
|      9 | 5332 | `	if( nArg < 1 ){` |
|      - | 5333 | `		/* Missing arguments,return FALSE */` |
|      5 | 5334 | `		ph7_result_bool(pCtx,0);` |
|      5 | 5335 | `		return PH7_OK;` |
|      - | 5336 | `	}` |
|      - | 5337 | `	/* Point to the target string */` |
|      5 | 5338 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 5339 | `	if( nLen < 1 ){` |
|      - | 5340 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 5341 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5342 | `		return PH7_OK;` |
|      - | 5343 | `	}` |
|      5 | 5344 | `	split_len = (int)sizeof(char);` |
|      5 | 5345 | `	if( nArg > 1 ){` |
|      - | 5346 | `		/* Split length */` |
|      5 | 5347 | `		split_len = ph7_value_to_int(apArg[1]);` |
|      5 | 5348 | `		if( split_len < 1 ){` |
|      - | 5349 | `			/* Invalid length,return FALSE */` |
|      3 | 5350 | `			ph7_result_bool(pCtx,0);` |
|      3 | 5351 | `			return PH7_OK;` |
|      - | 5352 | `		}` |
|      3 | 5353 | `		if( split_len > nLen ){` |
|    ! 0 | 5354 | `			split_len = nLen;` |
|    ! 0 | 5355 | `		}` |
|      1 | 5356 | `	}` |
|      - | 5357 | `	/* Create the array and the scalar value */` |
|      3 | 5358 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 5359 | `	/*Chunk value */` |
|      3 | 5360 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 5361 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 5362 | `		/* Return FALSE */` |
|    ! 0 | 5363 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5364 | `		return PH7_OK;` |
|      - | 5365 | `	}` |
|      - | 5366 | `	/* Point to the end of the string */` |
|      3 | 5367 | `	zEnd = &zString[nLen];` |
|      - | 5368 | `	/* Perform the requested operation */` |
|      7 | 5369 | `	for(;;){` |
|      - | 5370 | `		int nMax;` |
|      9 | 5371 | `		if( zString >= zEnd ){` |
|      - | 5372 | `			/* No more input to process */` |
|      3 | 5373 | `			break;` |
|      - | 5374 | `		}` |
|      7 | 5375 | `		nMax = (int)(zEnd-zString);` |
|      7 | 5376 | `		if( nMax < split_len ){` |
|    ! 0 | 5377 | `			split_len = nMax;` |
|    ! 0 | 5378 | `		}` |
|      - | 5379 | `		/* Copy the current chunk */` |
|      7 | 5380 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 5381 | `		/* Insert it */` |
|      7 | 5382 | `		ph7_array_add_elem(pArray,0,pValue); /* Will make it's own copy */` |
|      - | 5383 | `		/* reset the string cursor */` |
|      7 | 5384 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 5385 | `		/* Update position */` |
|      7 | 5386 | `		zString += split_len;` |
|      1 | 5387 | `	}` |
|      - | 5388 | `	/*` |
|      - | 5389 | `	 * Return the array.` |
|      - | 5390 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 5391 | `	 * upon we return from this function.` |
|      - | 5392 | `	 */` |
|      3 | 5393 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5394 | `	return PH7_OK;` |
|      5 | 5395 |  |
|      - | 5396 | `/*` |
|      - | 5397 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 5398 | ` * Refer to [strspn()].` |
|      - | 5399 | ` */` |
|     28 | 5400 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 5401 |  |
|     29 | 5402 | `	const char *zIn = *pzIn;` |
|      - | 5403 | `	const char *zPtr;` |
|      - | 5404 | `	/* Ignore leading white spaces */` |
|     29 | 5405 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 5406 | `		zIn++;` |
|    ! 0 | 5407 | `	}` |
|     29 | 5408 | `	if( zIn >= zEnd ){` |
|      - | 5409 | `		/* End of input */` |
|    ! 0 | 5410 | `		return SXERR_EOF;` |
|      - | 5411 | `	}` |
|     29 | 5412 | `	zPtr = zIn;` |
|      - | 5413 | `	/* Extract the token */` |
|    201 | 5414 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 5415 | `		zIn++;` |
|      1 | 5416 | `	}` |
|     29 | 5417 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5418 | `	/* Synchronize pointers */` |
|     29 | 5419 | `	*pzIn = zIn;` |
|      - | 5420 | `	/* Return to the caller */` |
|     29 | 5421 | `	return SXRET_OK;` |
|     15 | 5422 |  |
|      - | 5423 | `/*` |
|      - | 5424 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 5425 | ` * return the longest match.` |
|      - | 5426 | ` * Refer to [strspn()].` |
|      - | 5427 | ` */` |
|     18 | 5428 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5429 |  |
|     19 | 5430 | `	const char *zEnd = &zString[nLen];` |
|     19 | 5431 | `	const char *zIn = zString;` |
|      - | 5432 | `	int i,c;` |
|     45 | 5433 | `	for(;;){` |
|     91 | 5434 | `		if( zString >= zEnd ){` |
|      7 | 5435 | `			break;` |
|      - | 5436 | `		}` |
|      - | 5437 | `		/* Extract current character */` |
|     85 | 5438 | `		c = zString[0];` |
|      - | 5439 | `		/* Perform the lookup */` |
|    383 | 5440 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 5441 | `			if( c == zMask[i] ){` |
|      - | 5442 | `				/* Character found */` |
|     73 | 5443 | `				break;` |
|      - | 5444 | `			}` |
|    150 | 5445 | `		}` |
|     85 | 5446 | `		if( i >= nMaskLen ){` |
|      - | 5447 | `			/* Character not in the current mask,break immediately */` |
|     13 | 5448 | `			break;` |
|      - | 5449 | `		}` |
|      - | 5450 | `		/* Advance cursor */` |
|     73 | 5451 | `		zString++;` |
|      1 | 5452 | `	}` |
|      - | 5453 | `	/* Longest match */` |
|     19 | 5454 | `	return (int)(zString-zIn);` |
|      1 | 5455 |  |
|      - | 5456 | `/*` |
|      - | 5457 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 5458 | ` * Refer to [strcspn()].` |
|      - | 5459 | ` */` |
|     10 | 5460 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5461 |  |
|     11 | 5462 | `	const char *zEnd = &zString[nLen];` |
|     11 | 5463 | `	const char *zIn = zString;` |
|      - | 5464 | `	int i,c;` |
|     12 | 5465 | `	for(;;){` |
|     25 | 5466 | `		if( zString >= zEnd ){` |
|      3 | 5467 | `			break;` |
|      - | 5468 | `		}` |
|      - | 5469 | `		/* Extract current character */` |
|     23 | 5470 | `		c = zString[0];` |
|      - | 5471 | `		/* Perform the lookup */` |
|     51 | 5472 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 5473 | `			if( c == zMask[i] ){` |
|      9 | 5474 | `				break;` |
|      - | 5475 | `			}` |
|     15 | 5476 | `		}` |
|     23 | 5477 | `		if( i < nMaskLen ){` |
|      - | 5478 | `			/* Character in the current mask,break immediately */` |
|      9 | 5479 | `			break;` |
|      - | 5480 | `		}` |
|      - | 5481 | `		/* Advance cursor */` |
|     15 | 5482 | `		zString++;` |
|      1 | 5483 | `	}` |
|      - | 5484 | `	/* Longest match */` |
|     11 | 5485 | `	return (int)(zString-zIn);` |
|      1 | 5486 |  |
|      - | 5487 | `/*` |
|      - | 5488 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5489 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 5490 | ` *  of characters contained within a given mask.` |
|      - | 5491 | ` * Parameters` |
|      - | 5492 | ` * $str` |
|      - | 5493 | ` *  The input string.` |
|      - | 5494 | ` * $mask` |
|      - | 5495 | ` *  The list of allowable characters.` |
|      - | 5496 | ` * $start` |
|      - | 5497 | ` *  The position in subject to start searching.` |
|      - | 5498 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5499 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5500 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5501 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5502 | ` *  start'th position from the end of subject.` |
|      - | 5503 | ` * $length` |
|      - | 5504 | ` *  The length of the segment from subject to examine.` |
|      - | 5505 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5506 | ` *  characters after the starting position.` |
|      - | 5507 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5508 | ` *  position up to length characters from the end of subject.` |
|      - | 5509 | ` * Return` |
|      - | 5510 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 5511 | ` * in mask.` |
|      - | 5512 | ` */` |
|     26 | 5513 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5514 |  |
|      - | 5515 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5516 | `	int iMasklen,iLen;` |
|      - | 5517 | `	SyString sToken;` |
|     27 | 5518 | `	int iCount = 0;` |
|      - | 5519 | `	int rc;` |
|     27 | 5520 | `	if( nArg < 2 ){` |
|      - | 5521 | `		/* Missing agruments,return zero */` |
|      3 | 5522 | `		ph7_result_int(pCtx,0);` |
|      3 | 5523 | `		return PH7_OK;` |
|      - | 5524 | `	}` |
|      - | 5525 | `	/* Extract the target string */` |
|     25 | 5526 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5527 | `	/* Extract the mask */` |
|     25 | 5528 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 5529 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 5530 | `		/* Nothing to process,return zero */` |
|      7 | 5531 | `		ph7_result_int(pCtx,0);` |
|      7 | 5532 | `		return PH7_OK;` |
|      - | 5533 | `	}` |
|     19 | 5534 | `	if( nArg > 2 ){` |
|      - | 5535 | `		int nOfft;` |
|      - | 5536 | `		/* Extract the offset */` |
|      9 | 5537 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 5538 | `		if( nOfft < 0 ){` |
|    ! 0 | 5539 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5540 | `			if( zBase > zString ){` |
|    ! 0 | 5541 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5542 | `				zString = zBase;` |
|    ! 0 | 5543 | `			}else{` |
|      - | 5544 | `				/* Invalid offset */` |
|    ! 0 | 5545 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5546 | `				return PH7_OK;` |
|      - | 5547 | `			}` |
|    ! 0 | 5548 | `		}else{` |
|      9 | 5549 | `			if( nOfft >= iLen ){` |
|      - | 5550 | `				/* Invalid offset */` |
|    ! 0 | 5551 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5552 | `				return PH7_OK;` |
|    ! 0 | 5553 | `			}else{` |
|      - | 5554 | `				/* Update offset */` |
|      9 | 5555 | `				zString += nOfft;` |
|      9 | 5556 | `				iLen -= nOfft;` |
|      - | 5557 | `			}` |
|      - | 5558 | `		}` |
|      9 | 5559 | `		if( nArg > 3 ){` |
|      - | 5560 | `			int iUserlen;` |
|      - | 5561 | `			/* Extract the desired length */` |
|      9 | 5562 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 5563 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 5564 | `				iLen = iUserlen;` |
|      2 | 5565 | `			}` |
|      4 | 5566 | `		}` |
|      4 | 5567 | `	}` |
|      - | 5568 | `	/* Point to the end of the string */` |
|     19 | 5569 | `	zEnd = &zString[iLen];` |
|      - | 5570 | `	/* Extract the first non-space token */` |
|     19 | 5571 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 5572 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5573 | `		/* Compare against the current mask */` |
|     19 | 5574 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 5575 | `	}` |
|      - | 5576 | `	/* Longest match */` |
|     19 | 5577 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 5578 | `	return PH7_OK;` |
|     14 | 5579 |  |
|      - | 5580 | `/*` |
|      - | 5581 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5582 | ` *  Find length of initial segment not matching mask.` |
|      - | 5583 | ` * Parameters` |
|      - | 5584 | ` * $str` |
|      - | 5585 | ` *  The input string.` |
|      - | 5586 | ` * $mask` |
|      - | 5587 | ` *  The list of not allowed characters.` |
|      - | 5588 | ` * $start` |
|      - | 5589 | ` *  The position in subject to start searching.` |
|      - | 5590 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5591 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5592 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5593 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5594 | ` *  start'th position from the end of subject.` |
|      - | 5595 | ` * $length` |
|      - | 5596 | ` *  The length of the segment from subject to examine.` |
|      - | 5597 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5598 | ` *  characters after the starting position.` |
|      - | 5599 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5600 | ` *  position up to length characters from the end of subject.` |
|      - | 5601 | ` * Return` |
|      - | 5602 | ` *  Returns the length of the segment as an integer.` |
|      - | 5603 | ` */` |
|     16 | 5604 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5605 |  |
|      - | 5606 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5607 | `	int iMasklen,iLen;` |
|      - | 5608 | `	SyString sToken;` |
|     17 | 5609 | `	int iCount = 0;` |
|      - | 5610 | `	int rc;` |
|     17 | 5611 | `	if( nArg < 2 ){` |
|      - | 5612 | `		/* Missing agruments,return zero */` |
|      3 | 5613 | `		ph7_result_int(pCtx,0);` |
|      3 | 5614 | `		return PH7_OK;` |
|      - | 5615 | `	}` |
|      - | 5616 | `	/* Extract the target string */` |
|     15 | 5617 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5618 | `	/* Extract the mask */` |
|     15 | 5619 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 5620 | `	if( iLen < 1 ){` |
|      - | 5621 | `		/* Nothing to process,return zero */` |
|    ! 0 | 5622 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5623 | `		return PH7_OK;` |
|      - | 5624 | `	}` |
|     15 | 5625 | `	if( iMasklen < 1 ){` |
|      - | 5626 | `		/* No given mask,return the string length */` |
|      3 | 5627 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 5628 | `		return PH7_OK;` |
|      - | 5629 | `	}` |
|     13 | 5630 | `	if( nArg > 2 ){` |
|      - | 5631 | `		int nOfft;` |
|      - | 5632 | `		/* Extract the offset */` |
|     11 | 5633 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 5634 | `		if( nOfft < 0 ){` |
|    ! 0 | 5635 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5636 | `			if( zBase > zString ){` |
|    ! 0 | 5637 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5638 | `				zString = zBase;` |
|    ! 0 | 5639 | `			}else{` |
|      - | 5640 | `				/* Invalid offset */` |
|    ! 0 | 5641 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5642 | `				return PH7_OK;` |
|      - | 5643 | `			}` |
|    ! 0 | 5644 | `		}else{` |
|     11 | 5645 | `			if( nOfft >= iLen ){` |
|      - | 5646 | `				/* Invalid offset */` |
|      3 | 5647 | `				ph7_result_int(pCtx,0);` |
|      3 | 5648 | `				return PH7_OK;` |
|    ! 0 | 5649 | `			}else{` |
|      - | 5650 | `				/* Update offset */` |
|      9 | 5651 | `				zString += nOfft;` |
|      9 | 5652 | `				iLen -= nOfft;` |
|      - | 5653 | `			}` |
|      - | 5654 | `		}` |
|      9 | 5655 | `		if( nArg > 3 ){` |
|      - | 5656 | `			int iUserlen;` |
|      - | 5657 | `			/* Extract the desired length */` |
|    ! 0 | 5658 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 5659 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 5660 | `				iLen = iUserlen;` |
|    ! 0 | 5661 | `			}` |
|    ! 0 | 5662 | `		}` |
|      4 | 5663 | `	}` |
|      - | 5664 | `	/* Point to the end of the string */` |
|     11 | 5665 | `	zEnd = &zString[iLen];` |
|      - | 5666 | `	/* Extract the first non-space token */` |
|     11 | 5667 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 5668 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5669 | `		/* Compare against the current mask */` |
|     11 | 5670 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 5671 | `	}` |
|      - | 5672 | `	/* Longest match */` |
|     11 | 5673 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 5674 | `	return PH7_OK;` |
|      9 | 5675 |  |
|      - | 5676 | `/*` |
|      - | 5677 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 5678 | ` *  Search a string for any of a set of characters.` |
|      - | 5679 | ` * Parameters` |
|      - | 5680 | ` *  $haystack` |
|      - | 5681 | ` *   The string where char_list is looked for.` |
|      - | 5682 | ` *  $char_list` |
|      - | 5683 | ` *   This parameter is case sensitive.` |
|      - | 5684 | ` * Return` |
|      - | 5685 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 5686 | ` */` |
|      6 | 5687 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5688 |  |
|      - | 5689 | `	const char *zString,*zList,*zEnd;` |
|      - | 5690 | `	int iLen,iListLen,i,c;` |
|      - | 5691 | `	sxu32 nOfft,nMax;` |
|      - | 5692 | `	sxi32 rc;` |
|      7 | 5693 | `	if( nArg < 2 ){` |
|      - | 5694 | `		/* Missing arguments,return FALSE */` |
|      3 | 5695 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5696 | `		return PH7_OK;` |
|      - | 5697 | `	}` |
|      - | 5698 | `	/* Extract the haystack and the char list */` |
|      5 | 5699 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 5700 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 5701 | `	if( iLen < 1 ){` |
|      - | 5702 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 5703 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5704 | `		return PH7_OK;` |
|      - | 5705 | `	}` |
|      - | 5706 | `	/* Point to the end of the string */` |
|      5 | 5707 | `	zEnd = &zString[iLen];` |
|      5 | 5708 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 5709 | `	/* perform the requested operation */` |
|     15 | 5710 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 5711 | `		c = zList[i];` |
|     11 | 5712 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 5713 | `		if( rc == SXRET_OK ){` |
|      5 | 5714 | `			if( nMax < nOfft ){` |
|      3 | 5715 | `				nOfft = nMax;` |
|      1 | 5716 | `			}` |
|      2 | 5717 | `		}` |
|      6 | 5718 | `	}` |
|      5 | 5719 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 5720 | `		/* No such substring,return FALSE */` |
|      3 | 5721 | `		ph7_result_bool(pCtx,0);` |
|      2 | 5722 | `	}else{` |
|      - | 5723 | `		/* Return the substring */` |
|      3 | 5724 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 5725 | `	}` |
|      5 | 5726 | `	return PH7_OK;` |
|      4 | 5727 |  |
|      - | 5728 | `/*` |
|      - | 5729 | ` * string soundex(string $str)` |
|      - | 5730 | ` *  Calculate the soundex key of a string.` |
|      - | 5731 | ` * Parameters` |
|      - | 5732 | ` *  $str` |
|      - | 5733 | ` *   The input string.` |
|      - | 5734 | ` * Return` |
|      - | 5735 | ` *  Returns the soundex key as a string.` |
|      - | 5736 | ` * Note:` |
|      - | 5737 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 5738 | ` * source tree.` |
|      - | 5739 | ` */` |
|     20 | 5740 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5741 |  |
|      - | 5742 | `	const unsigned char *zIn;` |
|      - | 5743 | `	char zResult[8];` |
|      - | 5744 | `	int i, j;` |
|      - | 5745 | `	static const unsigned char iCode[] = {` |
|      - | 5746 |  |
|      - | 5747 |  |
|      - | 5748 |  |
|      - | 5749 |  |
|      - | 5750 |  |
|      - | 5751 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5752 |  |
|      - | 5753 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5754 | `	};` |
|     21 | 5755 | `	if( nArg < 1 ){` |
|      - | 5756 | `		/* Missing arguments,return the empty string */` |
|      3 | 5757 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5758 | `		return PH7_OK;` |
|      - | 5759 | `	}` |
|     19 | 5760 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     19 | 5761 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     19 | 5762 | `	if( zIn[i] ){` |
|     17 | 5763 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 5764 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 5765 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 5766 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 5767 | `			if( code>0 ){` |
|     45 | 5768 | `				if( code!=prevcode ){` |
|     33 | 5769 | `					prevcode = (unsigned char)code;` |
|     33 | 5770 | `					zResult[j++] = (char)code + '0';` |
|     16 | 5771 | `				}` |
|     23 | 5772 | `			}else{` |
|     49 | 5773 | `				prevcode = 0;` |
|      - | 5774 | `			}` |
|     47 | 5775 | `		}` |
|     33 | 5776 | `		while( j<4 ){` |
|     17 | 5777 | `			zResult[j++] = '0';` |
|      1 | 5778 | `		}` |
|     17 | 5779 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 5780 | `	}else{` |
|      3 | 5781 | `	  ph7_result_string(pCtx,"?000",4);` |
|      - | 5782 | `	}` |
|     19 | 5783 | `	return PH7_OK;` |
|     11 | 5784 |  |
|      - | 5785 | `/*` |
|      - | 5786 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 5787 | ` *  Wraps a string to a given number of characters.` |
|      - | 5788 | ` * Parameters` |
|      - | 5789 | ` *  $str` |
|      - | 5790 | ` *   The input string.` |
|      - | 5791 | ` * $width` |
|      - | 5792 | ` *  The column width.` |
|      - | 5793 | ` * $break` |
|      - | 5794 | ` *  The line is broken using the optional break parameter.` |
|      - | 5795 | ` * Return` |
|      - | 5796 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 5797 | ` */` |
|     14 | 5798 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5799 |  |
|      - | 5800 | `	const char *zIn,*zEnd,*zBreak;` |
|      - | 5801 | `	int iLen,iBreaklen,iChunk;` |
|     15 | 5802 | `	if( nArg < 1 ){` |
|      - | 5803 | `		/* Missing arguments,return the empty string */` |
|      3 | 5804 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5805 | `		return PH7_OK;` |
|      - | 5806 | `	}` |
|      - | 5807 | `	/* Extract the input string */` |
|     13 | 5808 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|     13 | 5809 | `	if( iLen < 1 ){` |
|      - | 5810 | `		/* Nothing to process,return the empty string */` |
|      3 | 5811 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5812 | `		return PH7_OK;` |
|      - | 5813 | `	}` |
|      - | 5814 | `	/* Chunk length */` |
|     11 | 5815 | `	iChunk = 75;` |
|     11 | 5816 | `	iBreaklen = 0;` |
|     11 | 5817 | `	zBreak = ""; /* cc warning */` |
|     11 | 5818 | `	if( nArg > 1 ){` |
|     11 | 5819 | `		iChunk = ph7_value_to_int(apArg[1]);` |
|     11 | 5820 | `		if( iChunk < 1 ){` |
|    ! 0 | 5821 | `			iChunk = 75;` |
|    ! 0 | 5822 | `		}` |
|     11 | 5823 | `		if( nArg > 2 ){` |
|      3 | 5824 | `			zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      1 | 5825 | `		}` |
|      5 | 5826 | `	}` |
|     11 | 5827 | `	if( iBreaklen < 1 ){` |
|      - | 5828 | `		/* Set a default column break */` |
|      - | 5829 | `#ifdef __WINNT__` |
|      1 | 5830 | `		zBreak = "\r\n";` |
|      1 | 5831 | `		iBreaklen = (int)sizeof("\r\n")-1;` |
|      - | 5832 | `#else` |
|      8 | 5833 | `		zBreak = "\n";` |
|      8 | 5834 | `		iBreaklen = (int)sizeof(char);` |
|      - | 5835 | `#endif` |
|      4 | 5836 | `	}` |
|      - | 5837 | `	/* Perform the requested operation */` |
|     11 | 5838 | `	zEnd = &zIn[iLen];` |
|     41 | 5839 | `	for(;;){` |
|      - | 5840 | `		int nMax;` |
|     47 | 5841 | `		if( zIn >= zEnd ){` |
|      - | 5842 | `			/* No more input to process */` |
|     11 | 5843 | `			break;` |
|      - | 5844 | `		}` |
|     37 | 5845 | `		nMax = (int)(zEnd-zIn);` |
|     37 | 5846 | `		if( iChunk > nMax ){` |
|     11 | 5847 | `			iChunk = nMax;` |
|      5 | 5848 | `		}` |
|      - | 5849 | `		/* Append the column first */` |
|     37 | 5850 | `		ph7_result_string(pCtx,zIn,iChunk); /* Will make it's own copy */` |
|      - | 5851 | `		/* Advance the cursor */` |
|     37 | 5852 | `		zIn += iChunk;` |
|     37 | 5853 | `		if( zIn < zEnd ){` |
|      - | 5854 | `			/* Append the line break */` |
|     27 | 5855 | `			ph7_result_string(pCtx,zBreak,iBreaklen);` |
|     13 | 5856 | `		}` |
|      1 | 5857 | `	}` |
|     11 | 5858 | `	return PH7_OK;` |
|      8 | 5859 |  |
|      - | 5860 | `/*` |
|      - | 5861 | ` * Check if the given character is a member of the given mask.` |
|      - | 5862 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 5863 | ` * Refer to [strtok()].` |
|      - | 5864 | ` */` |
|     30 | 5865 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 5866 |  |
|      - | 5867 | `	int i;` |
|     57 | 5868 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 5869 | `		if( c == zMask[i] ){` |
|     13 | 5870 | `			if( pOfft ){` |
|      5 | 5871 | `				*pOfft = i;` |
|      2 | 5872 | `			}` |
|     13 | 5873 | `			return TRUE;` |
|      - | 5874 | `		}` |
|     14 | 5875 | `	}` |
|     19 | 5876 | `	return FALSE;` |
|     16 | 5877 |  |
|      - | 5878 | `/*` |
|      - | 5879 | ` * Extract a single token from the input stream.` |
|      - | 5880 | ` * Refer to [strtok()].` |
|      - | 5881 | ` */` |
|      6 | 5882 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 5883 |  |
|      7 | 5884 | `	const char *zIn = *pzIn;` |
|      - | 5885 | `	const char *zPtr;` |
|      - | 5886 | `	/* Ignore leading delimiter */` |
|     11 | 5887 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5888 | `		zIn++;` |
|      1 | 5889 | `	}` |
|      7 | 5890 | `	if( zIn >= zEnd ){` |
|      - | 5891 | `		/* End of input */` |
|    ! 0 | 5892 | `		return SXERR_EOF;` |
|      - | 5893 | `	}` |
|      7 | 5894 | `	zPtr = zIn;` |
|      - | 5895 | `	/* Extract the token */` |
|     13 | 5896 | `	while( zIn < zEnd ){` |
|     11 | 5897 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 5898 | `			/* UTF-8 stream */` |
|    ! 0 | 5899 | `			zIn++;` |
|    ! 0 | 5900 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 5901 | `		}else{` |
|     11 | 5902 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5903 | `				break;` |
|      - | 5904 | `			}` |
|      7 | 5905 | `			zIn++;` |
|      - | 5906 | `		}` |
|      1 | 5907 | `	}` |
|      7 | 5908 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5909 | `	/* Update the cursor */` |
|      7 | 5910 | `	*pzIn = zIn;` |
|      - | 5911 | `	/* Return to the caller */` |
|      7 | 5912 | `	return SXRET_OK;` |
|      4 | 5913 |  |
|      - | 5914 | `/* strtok auxiliary private data */` |
|      - | 5915 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 5916 | `struct strtok_aux_data` |
|      - | 5917 |  |
|      - | 5918 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 5919 | `	const char *zIn;   /* Current input stream */` |
|      - | 5920 | `	const char *zEnd;  /* End of input */` |
|      - | 5921 | `};` |
|      - | 5922 | `/*` |
|      - | 5923 | ` * string strtok(string $str,string $token)` |
|      - | 5924 | ` * string strtok(string $token)` |
|      - | 5925 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 5926 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 5927 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 5928 | ` *  words by using the space character as the token.` |
|      - | 5929 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 5930 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 5931 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 5932 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 5933 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 5934 | ` *  the argument are found.` |
|      - | 5935 | ` * Parameters` |
|      - | 5936 | ` *  $str` |
|      - | 5937 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 5938 | ` * $token` |
|      - | 5939 | ` *  The delimiter used when splitting up str.` |
|      - | 5940 | ` * Return` |
|      - | 5941 | ` *   Current token or FALSE on EOF.` |
|      - | 5942 | ` */` |
|      8 | 5943 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5944 |  |
|      - | 5945 | `	strtok_aux_data *pAux;` |
|      - | 5946 | `	const char *zMask;` |
|      - | 5947 | `	SyString sToken;` |
|      - | 5948 | `	int nMasklen;` |
|      - | 5949 | `	sxi32 rc;` |
|      9 | 5950 | `	if( nArg < 2 ){` |
|      - | 5951 | `		/* Extract top aux data */` |
|      7 | 5952 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      7 | 5953 | `		if( pAux == 0 ){` |
|      - | 5954 | `			/* No aux data,return FALSE */` |
|    ! 0 | 5955 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5956 | `			return PH7_OK;` |
|      - | 5957 | `		}` |
|      7 | 5958 | `		nMasklen = 0;` |
|      7 | 5959 | `		zMask = ""; /* cc warning */` |
|      7 | 5960 | `		if( nArg > 0 ){` |
|      - | 5961 | `			/* Extract the mask */` |
|      5 | 5962 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 5963 | `		}` |
|      7 | 5964 | `		if( nMasklen < 1 ){` |
|      - | 5965 | `			/* Invalid mask,return FALSE */` |
|      3 | 5966 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|      3 | 5967 | `			ph7_context_free_chunk(pCtx,pAux);` |
|      3 | 5968 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|      3 | 5969 | `			ph7_result_bool(pCtx,0);` |
|      3 | 5970 | `			return PH7_OK;` |
|      - | 5971 | `		}` |
|      - | 5972 | `		/* Extract the token */` |
|      5 | 5973 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 5974 | `		if( rc != SXRET_OK ){` |
|      - | 5975 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 5976 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 5977 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5978 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 5979 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5980 | `		}else{` |
|      - | 5981 | `			/* Return the extracted token */` |
|      5 | 5982 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 5983 | `		}` |
|      3 | 5984 | `	}else{` |
|      - | 5985 | `		const char *zInput,*zCur;` |
|      - | 5986 | `		char *zDup;` |
|      - | 5987 | `		int nLen;` |
|      - | 5988 | `		/* Extract the raw input */` |
|      3 | 5989 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5990 | `		if( nLen < 1 ){` |
|      - | 5991 | `			/* Empty input,return FALSE */` |
|    ! 0 | 5992 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5993 | `			return PH7_OK;` |
|      - | 5994 | `		}` |
|      - | 5995 | `		/* Extract the mask */` |
|      3 | 5996 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 5997 | `		if( nMasklen < 1 ){` |
|      - | 5998 | `			/* Set a default mask */` |
|      - | 5999 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 6000 | `			zMask = TOK_MASK;` |
|    ! 0 | 6001 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 6002 | `#undef TOK_MASK` |
|    ! 0 | 6003 | `		}` |
|      - | 6004 | `		/* Extract a single token */` |
|      3 | 6005 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 6006 | `		if( rc != SXRET_OK ){` |
|      - | 6007 | `			/* Empty input */` |
|    ! 0 | 6008 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6009 | `			return PH7_OK;` |
|    ! 0 | 6010 | `		}else{` |
|      - | 6011 | `			/* Return the extracted token */` |
|      3 | 6012 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6013 | `		}` |
|      - | 6014 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 6015 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 6016 | `		if( pAux ){` |
|      3 | 6017 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 6018 | `			if( nLen < 1 ){` |
|    ! 0 | 6019 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6020 | `				return PH7_OK;` |
|      - | 6021 | `			}` |
|      - | 6022 | `			/* Duplicate input */` |
|      3 | 6023 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 6024 | `			if( zDup  ){` |
|      3 | 6025 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 6026 | `				/* Register the aux data */` |
|      3 | 6027 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 6028 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 6029 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 6030 | `			}` |
|      1 | 6031 | `		}` |
|      - | 6032 | `	}` |
|      7 | 6033 | `	return PH7_OK;` |
|      5 | 6034 |  |
|      - | 6035 | `/*` |
|      - | 6036 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 6037 | ` *  Pad a string to a certain length with another string` |
|      - | 6038 | ` * Parameters` |
|      - | 6039 | ` *  $input` |
|      - | 6040 | ` *   The input string.` |
|      - | 6041 | ` * $pad_length` |
|      - | 6042 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 6043 | ` *   string, no padding takes place.` |
|      - | 6044 | ` * $pad_string` |
|      - | 6045 | ` *   Note:` |
|      - | 6046 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 6047 | ` *    divided by the pad_string's length.` |
|      - | 6048 | ` * $pad_type` |
|      - | 6049 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 6050 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 6051 | ` * Return` |
|      - | 6052 | ` *  The padded string.` |
|      - | 6053 | ` */` |
|     10 | 6054 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6055 |  |
|      - | 6056 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 6057 | `	const char *zIn,*zPad;` |
|     11 | 6058 | `	if( nArg < 2 ){` |
|      - | 6059 | `		/* Missing arguments,return the empty string */` |
|      5 | 6060 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 6061 | `		return PH7_OK;` |
|      - | 6062 | `	}` |
|      - | 6063 | `	/* Extract the target string */` |
|      7 | 6064 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6065 | `	/* Padding length */` |
|      7 | 6066 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|      7 | 6067 | `	if( iPadlen > 0 ){` |
|      5 | 6068 | `		iPadlen -= iLen;` |
|      2 | 6069 | `	}` |
|      7 | 6070 | `	if( iPadlen < 1  ){` |
|      - | 6071 | `		/* Return the string verbatim */` |
|      3 | 6072 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      3 | 6073 | `		return PH7_OK;` |
|      - | 6074 | `	}` |
|      5 | 6075 | `	zPad = " "; /* Whitespace padding */` |
|      5 | 6076 | `	iStrpad = (int)sizeof(char);` |
|      5 | 6077 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      5 | 6078 | `	if( nArg > 2 ){` |
|      - | 6079 | `		/* Padding string */` |
|      5 | 6080 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      5 | 6081 | `		if( iStrpad < 1 ){` |
|      - | 6082 | `			/* Empty string */` |
|    ! 0 | 6083 | `			zPad = " "; /* Whitespace padding */` |
|    ! 0 | 6084 | `			iStrpad = (int)sizeof(char);` |
|    ! 0 | 6085 | `		}` |
|      5 | 6086 | `		if( nArg > 3 ){` |
|      - | 6087 | `			/* Padd type */` |
|      5 | 6088 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 6089 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 6090 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 6091 | `			}` |
|      2 | 6092 | `		}` |
|      2 | 6093 | `	}` |
|      5 | 6094 | `	iDiv = 1;` |
|      5 | 6095 | `	if( iType == 2 ){` |
|    ! 0 | 6096 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 6097 | `	}` |
|      - | 6098 | `	/* Perform the requested operation */` |
|      5 | 6099 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 6100 | `		jPad = iStrpad;` |
|      5 | 6101 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 6102 | `			/* Padding */` |
|      5 | 6103 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 6104 | `				break;` |
|      - | 6105 | `			}` |
|      3 | 6106 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      2 | 6107 | `		}` |
|      3 | 6108 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 6109 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 6110 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 6111 | `				if( jPad > iStrpad ){` |
|    ! 0 | 6112 | `					jPad = iStrpad;` |
|    ! 0 | 6113 | `				}` |
|      3 | 6114 | `				if( jPad < 1){` |
|    ! 0 | 6115 | `					break;` |
|      - | 6116 | `				}` |
|      3 | 6117 | `				ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 6118 | `			}` |
|      1 | 6119 | `		}` |
|      1 | 6120 | `	}` |
|      5 | 6121 | `	if( iLen > 0 ){` |
|      - | 6122 | `		/* Append the input string */` |
|      5 | 6123 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      2 | 6124 | `	}` |
|      5 | 6125 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 6126 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 6127 | `			/* Padding */` |
|      5 | 6128 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 6129 | `				break;` |
|      - | 6130 | `			}` |
|      3 | 6131 | `			ph7_result_string(pCtx,zPad,iStrpad);` |
|      2 | 6132 | `		}` |
|      5 | 6133 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 6134 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 6135 | `			if( jPad > iStrpad ){` |
|    ! 0 | 6136 | `				jPad = iStrpad;` |
|    ! 0 | 6137 | `			}` |
|      3 | 6138 | `			if( jPad < 1){` |
|    ! 0 | 6139 | `				break;` |
|      - | 6140 | `			}` |
|      3 | 6141 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 6142 | `		}` |
|      1 | 6143 | `	}` |
|      5 | 6144 | `	return PH7_OK;` |
|      6 | 6145 |  |
|      - | 6146 | `/*` |
|      - | 6147 | ` * String replacement private data.` |
|      - | 6148 | ` */` |
|      - | 6149 | `typedef struct str_replace_data str_replace_data;` |
|      - | 6150 | `struct str_replace_data` |
|      - | 6151 |  |
|      - | 6152 | `	/* The following two fields are only used by the strtr function */` |
|      - | 6153 | `	SyBlob *pWorker;         /* Working buffer */` |
|      - | 6154 | `	ProcStringMatch xMatch;  /* Pattern match routine */` |
|      - | 6155 | `	/* The following two fields are only used by the str_replace function */` |
|      - | 6156 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 6157 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 6158 | `};` |
|      - | 6159 | `/*` |
|      - | 6160 | ` * Remove a substring.` |
|      - | 6161 | ` */` |
|      - | 6162 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 6163 | `	for(;;){\` |
|      - | 6164 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 6165 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 6166 | `		++OFFT;\` |
|      - | 6167 | `	}\` |
|      - | 6168 |  |
|      - | 6169 | `/*` |
|      - | 6170 | ` * Shift right and insert algorithm.` |
|      - | 6171 | ` */` |
|      - | 6172 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 6173 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 6174 | `		for(;;){\` |
|      - | 6175 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 6176 | `			if(INLEN < 1 ) { break; }\` |
|      - | 6177 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 6178 | `			--INLEN; \` |
|      - | 6179 | `		}\` |
|      - | 6180 | `		for(;;){\` |
|      - | 6181 | `				if(ELEN < 1) { break; }\` |
|      - | 6182 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 6183 | `				OFFT++;\` |
|      - | 6184 | `				ENTRY++;\` |
|      - | 6185 | `				--ELEN;\` |
|      - | 6186 | `		}\` |
|      - | 6187 |  |
|      - | 6188 | `/*` |
|      - | 6189 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 6190 | ` * replacement string [i.e: zReplace].` |
|      - | 6191 | ` */` |
|     38 | 6192 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 6193 |  |
|     39 | 6194 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 6195 | `	sxu32 n,m;` |
|     39 | 6196 | `	n = SyBlobLength(pWorker);` |
|     39 | 6197 | `	m = nOfft;` |
|      - | 6198 | `	/* Delete the old entry */` |
|    475 | 6199 | `	STRDEL(zInput,n,m,nLen);` |
|     39 | 6200 | `	SyBlobLength(pWorker) -= nLen;` |
|     39 | 6201 | `	if( nReplen > 0 ){` |
|     33 | 6202 | `		sxi32 iRep = nReplen;` |
|      - | 6203 | `		sxi32 rc;` |
|      - | 6204 | `		/*` |
|      - | 6205 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 6206 | `		 * string.` |
|      - | 6207 | `		 */` |
|     33 | 6208 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     33 | 6209 | `		if( rc != SXRET_OK ){` |
|      - | 6210 | `			/* Simply ignore any memory failure problem */` |
|    ! 0 | 6211 | `			return SXRET_OK;` |
|      - | 6212 | `		}` |
|      - | 6213 | `		/* Perform the insertion now */` |
|     33 | 6214 | `		zInput = (char *)SyBlobData(pWorker);` |
|     33 | 6215 | `		n = SyBlobLength(pWorker);` |
|    163 | 6216 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     33 | 6217 | `		SyBlobLength(pWorker) += nReplen;` |
|     16 | 6218 | `	}` |
|     39 | 6219 | `	return SXRET_OK;` |
|     20 | 6220 |  |
|      - | 6221 | `/*` |
|      - | 6222 | ` * String replacement walker callback.` |
|      - | 6223 | ` * The following callback is invoked for each array entry that hold` |
|      - | 6224 | ` * the replace string.` |
|      - | 6225 | ` * Refer to the strtr() implementation for more information.` |
|      - | 6226 | ` */` |
|      8 | 6227 | `static int StringReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6228 |  |
|      9 | 6229 | `	str_replace_data *pRepData = (str_replace_data *)pUserData;` |
|      - | 6230 | `	const char *zTarget,*zReplace;` |
|      - | 6231 | `	SyBlob *pWorker;` |
|      - | 6232 | `	int tLen,nLen;` |
|      - | 6233 | `	sxu32 nOfft;` |
|      - | 6234 | `	sxi32 rc;` |
|      - | 6235 | `	/* Point to the working buffer */` |
|      9 | 6236 | `	pWorker = pRepData->pWorker;` |
|      9 | 6237 | `	if( !ph7_value_is_string(pKey) ){` |
|      - | 6238 | `		/* Target and replace must be a string */` |
|      3 | 6239 | `		return PH7_OK;` |
|      - | 6240 | `	}` |
|      - | 6241 | `	/* Extract the target and the replace */` |
|      7 | 6242 | `	zTarget = ph7_value_to_string(pKey,&tLen);` |
|      7 | 6243 | `	if( tLen < 1 ){` |
|      - | 6244 | `		/* Empty target,return immediately */` |
|    ! 0 | 6245 | `		return PH7_OK;` |
|      - | 6246 | `	}` |
|      - | 6247 | `	/* Perform a pattern search */` |
|      7 | 6248 | `	rc = pRepData->xMatch(SyBlobData(pWorker),SyBlobLength(pWorker),(const void *)zTarget,(sxu32)tLen,&nOfft);` |
|      7 | 6249 | `	if( rc != SXRET_OK ){` |
|      - | 6250 | `		/* Pattern not found */` |
|    ! 0 | 6251 | `		return PH7_OK;` |
|      - | 6252 | `	}` |
|      - | 6253 | `	/* Extract the replace string */` |
|      7 | 6254 | `	zReplace = ph7_value_to_string(pData,&nLen);` |
|      - | 6255 | `	/* Perform the replace process */` |
|      7 | 6256 | `	StringReplace(pWorker,nOfft,tLen,zReplace,nLen);` |
|      - | 6257 | `	/* All done */` |
|      7 | 6258 | `	return PH7_OK;` |
|      5 | 6259 |  |
|      - | 6260 | `/*` |
|      - | 6261 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 6262 | ` * to collect search/replace string.` |
|      - | 6263 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 6264 | ` */` |
|     26 | 6265 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6266 |  |
|     27 | 6267 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 6268 | `	SyString sWorker;` |
|      - | 6269 | `	const char *zIn;` |
|      - | 6270 | `	int nByte;` |
|      - | 6271 | `	/* Extract a string representation of the given argument */` |
|     27 | 6272 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 6273 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 6274 | `	if( nByte > 0 ){` |
|      - | 6275 | `		char *zDup;` |
|      - | 6276 | `		/* Duplicate the chunk */` |
|     25 | 6277 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 6278 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 6279 | `			);` |
|     25 | 6280 | `		if( zDup == 0 ){` |
|      - | 6281 | `			/* Ignore any memory failure problem */` |
|    ! 0 | 6282 | `			ph7_context_throw_error(pRep->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 6283 | `			return PH7_OK;` |
|      - | 6284 | `		}` |
|     25 | 6285 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 6286 | `		/* Save the chunk */` |
|     25 | 6287 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 6288 | `	}` |
|      - | 6289 | `	/* Save for later processing */` |
|     27 | 6290 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 6291 | `	/* All done */` |
|     13 | 6292 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 6293 | `	return PH7_OK;` |
|     14 | 6294 |  |
|      - | 6295 | `/*` |
|      - | 6296 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6297 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6298 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 6299 | ` * Parameters` |
|      - | 6300 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 6301 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 6302 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 6303 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 6304 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 6305 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 6306 | ` * $search` |
|      - | 6307 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 6308 | ` *  to designate multiple needles.` |
|      - | 6309 | ` * $replace` |
|      - | 6310 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 6311 | ` *  to designate multiple replacements.` |
|      - | 6312 | ` * $subject` |
|      - | 6313 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 6314 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 6315 | ` *  of subject, and the return value is an array as well.` |
|      - | 6316 | ` * $count (Not used)` |
|      - | 6317 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 6318 | ` * Return` |
|      - | 6319 | ` * This function returns a string or an array with the replaced values.` |
|      - | 6320 | ` */` |
|  12698 | 6321 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6322 |  |
|      - | 6323 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 6324 | `	ProcStringMatch xMatch;` |
|      - | 6325 | `	const char *zIn,*zFunc;` |
|      - | 6326 | `	str_replace_data sRep;` |
|      - | 6327 | `	SyBlob sWorker;` |
|      - | 6328 | `	SySet sReplace;` |
|      - | 6329 | `	SySet sSearch;` |
|      - | 6330 | `	int rep_str;` |
|      - | 6331 | `	int nByte;` |
|      - | 6332 | `	sxi32 rc;` |
|  12700 | 6333 | `	if( nArg < 3 ){` |
|      - | 6334 | `		/* Missing/Invalid arguments,return null */` |
|      7 | 6335 | `		ph7_result_null(pCtx);` |
|      7 | 6336 | `		return PH7_OK;` |
|      - | 6337 | `	}` |
|      - | 6338 | `	/* Initialize fields */` |
|  12694 | 6339 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  12694 | 6340 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  12694 | 6341 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  12694 | 6342 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  12694 | 6343 | `	sRep.pCtx = pCtx;` |
|  12694 | 6344 | `	sRep.pCollector = &sSearch;` |
|  12694 | 6345 | `	rep_str = 0;` |
|      - | 6346 | `	/* Extract the subject */` |
|  12694 | 6347 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  12694 | 6348 | `	if( nByte < 1 ){` |
|      - | 6349 | `		/* Nothing to replace,return the empty string */` |
|     38 | 6350 | `		ph7_result_string(pCtx,"",0);` |
|     38 | 6351 | `		return PH7_OK;` |
|      - | 6352 | `	}` |
|      - | 6353 | `	/* Copy the subject */` |
|  12658 | 6354 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 6355 | `	/* Search string */` |
|  12658 | 6356 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 6357 | `		/* Collect search string */` |
|      9 | 6358 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 6359 | `	}else{` |
|      - | 6360 | `		/* Single pattern */` |
|  12650 | 6361 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  12650 | 6362 | `		if( nByte < 1 ){` |
|      - | 6363 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 6364 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 6365 | `			return PH7_OK;` |
|      - | 6366 | `		}` |
|  12646 | 6367 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6368 | `		/* Save for later processing */` |
|  12646 | 6369 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 6370 | `	}` |
|      - | 6371 | `	/* Replace string */` |
|  12654 | 6372 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 6373 | `		/* Collect replace string */` |
|      7 | 6374 | `		sRep.pCollector = &sReplace;` |
|      7 | 6375 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 6376 | `	}else{` |
|      - | 6377 | `		/* Single needle */` |
|  12648 | 6378 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  12648 | 6379 | `		rep_str = 1;` |
|  12648 | 6380 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6381 | `		/* Save for later processing */` |
|  12648 | 6382 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 6383 | `	}` |
|      - | 6384 | `	/* Reset loop cursors */` |
|  12654 | 6385 | `	SySetResetCursor(&sSearch);` |
|  12654 | 6386 | `	SySetResetCursor(&sReplace);` |
|  12654 | 6387 | `	pReplace = pSearch = 0; /* cc warning */` |
|  12654 | 6388 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 6389 | `	/* Extract function name */` |
|  12654 | 6390 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 6391 | `	/* Set the default pattern match routine */` |
|  12654 | 6392 | `	xMatch = SyBlobSearch;` |
|  12654 | 6393 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 6394 | `		/* Case insensitive pattern match */` |
|     11 | 6395 | `		xMatch = iPatternMatch;` |
|      5 | 6396 | `	}` |
|      - | 6397 | `	/* Start the replace process */` |
|  25314 | 6398 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 6399 | `		sxu32 nCount,nOfft;` |
|  12662 | 6400 | `		if( pSearch->nByte <  1 ){` |
|      - | 6401 | `			/* Empty string,ignore */` |
|      3 | 6402 | `			continue;` |
|      - | 6403 | `		}` |
|      - | 6404 | `		/* Extract the replace string */` |
|  12660 | 6405 | `		if( rep_str ){` |
|  12650 | 6406 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|   6326 | 6407 | `		}else{` |
|     11 | 6408 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 6409 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 6410 | `				 * An empty string is used for the rest of replacement values` |
|      - | 6411 | `				 */` |
|      3 | 6412 | `				pReplace = 0;` |
|      1 | 6413 | `			}` |
|      - | 6414 | `		}` |
|  12660 | 6415 | `		if( pReplace == 0 ){` |
|      - | 6416 | `			/* Use an empty string instead */` |
|      3 | 6417 | `			pReplace = &sTemp;` |
|      1 | 6418 | `		}` |
|  12660 | 6419 | `		nOfft = nCount = 0;` |
|   6345 | 6420 | `		for(;;){` |
|  12692 | 6421 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 6422 | `				break;` |
|      - | 6423 | `			}` |
|      - | 6424 | `			/* Perform a pattern lookup */` |
|  19019 | 6425 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  12678 | 6426 | `				pSearch->nByte,&nOfft);` |
|  12680 | 6427 | `			if( rc != SXRET_OK ){` |
|      - | 6428 | `				/* Pattern not found */` |
|  12648 | 6429 | `				break;` |
|      - | 6430 | `			}` |
|      - | 6431 | `			/* Perform the replace operation */` |
|     33 | 6432 | `			StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|      - | 6433 | `			/* Increment offset counter */` |
|     33 | 6434 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 6435 | `		}` |
|      2 | 6436 | `	}` |
|      - | 6437 | `	/* All done,clean-up the mess left behind */` |
|  12654 | 6438 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  12654 | 6439 | `	SySetRelease(&sSearch);` |
|  12654 | 6440 | `	SySetRelease(&sReplace);` |
|  12654 | 6441 | `	SyBlobRelease(&sWorker);` |
|  12654 | 6442 | `	return PH7_OK;` |
|   6351 | 6443 |  |
|      - | 6444 | `/*` |
|      - | 6445 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 6446 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 6447 | ` *  Translate characters or replace substrings.` |
|      - | 6448 | ` * Parameters` |
|      - | 6449 | ` *  $str` |
|      - | 6450 | ` *  The string being translated.` |
|      - | 6451 | ` * $from` |
|      - | 6452 | ` *  The string being translated to to.` |
|      - | 6453 | ` * $to` |
|      - | 6454 | ` *  The string replacing from.` |
|      - | 6455 | ` * $replace_pairs` |
|      - | 6456 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 6457 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 6458 | ` * Return` |
|      - | 6459 | ` *  The translated string.` |
|      - | 6460 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 6461 | ` */` |
|     12 | 6462 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6463 |  |
|      - | 6464 | `	const char *zIn;` |
|      - | 6465 | `	int nLen;` |
|     13 | 6466 | `	if( nArg < 1 ){` |
|      - | 6467 | `		/* Nothing to replace,return FALSE */` |
|      7 | 6468 | `		ph7_result_bool(pCtx,0);` |
|      7 | 6469 | `		return PH7_OK;` |
|      - | 6470 | `	}` |
|      7 | 6471 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 6472 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 6473 | `		/* Invalid arguments */` |
|    ! 0 | 6474 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6475 | `		return PH7_OK;` |
|      - | 6476 | `	}` |
|      9 | 6477 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 6478 | `		str_replace_data sRepData;` |
|      - | 6479 | `		SyBlob sWorker;` |
|      - | 6480 | `		/* Initilaize the working buffer */` |
|      5 | 6481 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|      - | 6482 | `		/* Copy raw string */` |
|      5 | 6483 | `		SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nLen);` |
|      - | 6484 | `		/* Init our replace data instance */` |
|      5 | 6485 | `		sRepData.pWorker = &sWorker;` |
|      5 | 6486 | `		sRepData.xMatch = SyBlobSearch;` |
|      - | 6487 | `		/* Iterate throw array entries and perform the replace operation.*/` |
|      5 | 6488 | `		ph7_array_walk(apArg[1],StringReplaceWalker,&sRepData);` |
|      - | 6489 | `		/* All done, return the result string */` |
|      7 | 6490 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|      4 | 6491 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 6492 | `		/* Clean-up */` |
|      5 | 6493 | `		SyBlobRelease(&sWorker);` |
|      3 | 6494 | `	}else{` |
|      - | 6495 | `		int i,flen,tlen,c,iOfft;` |
|      - | 6496 | `		const char *zFrom,*zTo;` |
|      3 | 6497 | `		if( nArg < 3 ){` |
|      - | 6498 | `			/* Nothing to replace */` |
|    ! 0 | 6499 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6500 | `			return PH7_OK;` |
|      - | 6501 | `		}` |
|      - | 6502 | `		/* Extract given arguments */` |
|      3 | 6503 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 6504 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 6505 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 6506 | `			/* Nothing to replace */` |
|    ! 0 | 6507 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6508 | `			return PH7_OK;` |
|      - | 6509 | `		}` |
|      - | 6510 | `		/* Start the replace process */` |
|     13 | 6511 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 6512 | `			c = zIn[i];` |
|     11 | 6513 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 6514 | `				if ( iOfft < tlen ){` |
|      5 | 6515 | `					c = zTo[iOfft];` |
|      2 | 6516 | `				}` |
|      2 | 6517 | `			}` |
|     11 | 6518 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 6519 |  |
|      6 | 6520 | `		}` |
|      - | 6521 | `	}` |
|      7 | 6522 | `	return PH7_OK;` |
|      7 | 6523 |  |
|      - | 6524 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6525 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6526 | `/*` |
|      - | 6527 | ` * Parse an INI string.` |
|      - | 6528 |  |
|      - | 6529 | ` * According to wikipedia` |
|      - | 6530 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 6531 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 6532 | ` *  Format` |
|      - | 6533 | `*    Properties` |
|      - | 6534 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 6535 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 6536 | `*     Example:` |
|      - | 6537 | `*      name=value` |
|      - | 6538 | `*    Sections` |
|      - | 6539 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 6540 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 6541 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 6542 | `*     or the end of the file. Sections may not be nested.` |
|      - | 6543 | `*     Example:` |
|      - | 6544 | `*      [section]` |
|      - | 6545 | `*   Comments` |
|      - | 6546 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 6547 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 6548 | `*/` |
|     12 | 6549 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 6550 |  |
|      - | 6551 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 6552 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 6553 | `	SyHashEntry *pEntry;` |
|      - | 6554 | `	SyString sEntry;` |
|      - | 6555 | `	SyHash sHash;` |
|      - | 6556 | `	int c;` |
|      - | 6557 | `	/* Create an empty array and worker variables */` |
|     13 | 6558 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 6559 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 6560 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 6561 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 6562 | `		/* Out of memory */` |
|    ! 0 | 6563 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      - | 6564 | `		/* Return FALSE */` |
|    ! 0 | 6565 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6566 | `		return PH7_OK;` |
|      - | 6567 | `	}` |
|     13 | 6568 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 6569 | `	pCur = pArray;` |
|      - | 6570 | `	/* Start the parse process */` |
|     21 | 6571 | `	for(;;){` |
|      - | 6572 | `		/* Ignore leading white spaces */` |
|     69 | 6573 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 6574 | `			zIn++;` |
|      1 | 6575 | `		}` |
|     43 | 6576 | `		if( zIn >= zEnd ){` |
|      - | 6577 | `			/* No more input to process */` |
|     13 | 6578 | `			break;` |
|      - | 6579 | `		}` |
|     31 | 6580 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6581 | `			/* Comment til the end of line */` |
|    ! 0 | 6582 | `			zIn++;` |
|    ! 0 | 6583 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 6584 | `				zIn++;` |
|    ! 0 | 6585 | `			}` |
|    ! 0 | 6586 | `			continue;` |
|      - | 6587 | `		}` |
|      - | 6588 | `		/* Reset the string cursor of the working variable */` |
|     31 | 6589 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 6590 | `		if( zIn[0] == '[' ){` |
|      - | 6591 | `			/* Section: Extract the section name */` |
|      9 | 6592 | `			zIn++;` |
|      9 | 6593 | `			zCur = zIn;` |
|     73 | 6594 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 6595 | `				zIn++;` |
|      1 | 6596 | `			}` |
|      9 | 6597 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 6598 | `				/* Save the section name */` |
|      5 | 6599 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 6600 | `				SyStringFullTrim(&sEntry);` |
|      5 | 6601 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 6602 | `				if( sEntry.nByte > 0 ){` |
|      - | 6603 | `					/* Associate an array with the section */` |
|      5 | 6604 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 6605 | `					if( pSection ){` |
|      5 | 6606 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 6607 | `						pCur = pSection;` |
|      2 | 6608 | `					}` |
|      2 | 6609 | `				}` |
|      2 | 6610 | `			}` |
|      9 | 6611 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 6612 | `		}else{` |
|      - | 6613 | `			ph7_value *pOldCur;` |
|      - | 6614 | `			int is_array;` |
|      - | 6615 | `			int iLen;` |
|      - | 6616 | `			/* Properties */` |
|     23 | 6617 | `			is_array = 0;` |
|     23 | 6618 | `			zCur = zIn;` |
|     23 | 6619 | `			iLen = 0; /* cc warning */` |
|     23 | 6620 | `			pOldCur = pCur;` |
|    155 | 6621 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 6622 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 6623 | `					/* Array */` |
|    ! 0 | 6624 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 6625 | `					is_array = 1;` |
|    ! 0 | 6626 | `					if( iLen > 0 ){` |
|    ! 0 | 6627 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 6628 | `						/* Query the hashtable */` |
|    ! 0 | 6629 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 6630 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 6631 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 6632 | `						if( pEntry ){` |
|    ! 0 | 6633 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 6634 | `						}else{` |
|      - | 6635 | `							/* Create an empty array */` |
|    ! 0 | 6636 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 6637 | `							if( pvArr ){` |
|      - | 6638 | `								/* Save the entry */` |
|    ! 0 | 6639 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 6640 | `								/* Insert the entry */` |
|    ! 0 | 6641 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6642 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 6643 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 6644 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6645 | `							}` |
|      - | 6646 | `						}` |
|    ! 0 | 6647 | `						if( pvArr ){` |
|    ! 0 | 6648 | `							pCur = pvArr;` |
|    ! 0 | 6649 | `						}` |
|    ! 0 | 6650 | `					}` |
|    ! 0 | 6651 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 6652 | `						zIn++;` |
|    ! 0 | 6653 | `					}` |
|    ! 0 | 6654 | `				}` |
|    133 | 6655 | `				zIn++;` |
|      1 | 6656 | `			}` |
|     23 | 6657 | `			if( !is_array ){` |
|     23 | 6658 | `				iLen = (int)(zIn-zCur);` |
|     11 | 6659 | `			}` |
|      - | 6660 | `			/* Trim the key */` |
|     23 | 6661 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 6662 | `			SyStringFullTrim(&sEntry);` |
|     23 | 6663 | `			if( sEntry.nByte > 0 ){` |
|     23 | 6664 | `				if( !is_array ){` |
|      - | 6665 | `					/* Save the key name */` |
|     23 | 6666 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 6667 | `				}` |
|      - | 6668 | `				/* extract key value */` |
|     23 | 6669 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 6670 | `				zIn++; /* '=' */` |
|     39 | 6671 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 6672 | `					zIn++;` |
|      1 | 6673 | `				}` |
|     23 | 6674 | `				if( zIn < zEnd ){` |
|     21 | 6675 | `					zCur = zIn;` |
|     21 | 6676 | `					c = zIn[0];` |
|     21 | 6677 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6678 | `						zIn++;` |
|      - | 6679 | `						/* Delimit the value */` |
|    ! 0 | 6680 | `						while( zIn < zEnd ){` |
|    ! 0 | 6681 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 6682 | `								break;` |
|      - | 6683 | `							}` |
|    ! 0 | 6684 | `							zIn++;` |
|    ! 0 | 6685 | `						}` |
|    ! 0 | 6686 | `						if( zIn < zEnd ){` |
|    ! 0 | 6687 | `							zIn++;` |
|    ! 0 | 6688 | `						}` |
|    ! 0 | 6689 | `					}else{` |
|    125 | 6690 | `						while( zIn < zEnd ){` |
|    123 | 6691 | `							if( zIn[0] == '\n' ){` |
|     19 | 6692 | `								if( zIn[-1] != '\\' ){` |
|     19 | 6693 | `									break;` |
|    ! 0 | 6694 | `								}` |
|    105 | 6695 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6696 | `								/* Inline comments */` |
|    ! 0 | 6697 | `								break;` |
|      - | 6698 | `							}` |
|    105 | 6699 | `							zIn++;` |
|      1 | 6700 | `						}` |
|      - | 6701 | `					}` |
|      - | 6702 | `					/* Trim the value */` |
|     21 | 6703 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 6704 | `					SyStringFullTrim(&sEntry);` |
|     21 | 6705 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6706 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 6707 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 6708 | `					}` |
|     21 | 6709 | `					if( sEntry.nByte > 0 ){` |
|     21 | 6710 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 6711 | `					}` |
|      - | 6712 | `					/* Insert the key and it's value */` |
|     21 | 6713 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 6714 | `				}` |
|     12 | 6715 | `			}else{` |
|    ! 0 | 6716 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 6717 | `					zIn++;` |
|    ! 0 | 6718 | `				}` |
|      - | 6719 | `			}` |
|     23 | 6720 | `			pCur = pOldCur;` |
|      - | 6721 | `		}` |
|      1 | 6722 | `	}` |
|     13 | 6723 | `	SyHashRelease(&sHash);` |
|      - | 6724 | `	/* Return the parse of the INI string */` |
|     13 | 6725 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 6726 | `	return SXRET_OK;` |
|      7 | 6727 |  |
|      - | 6728 | `/*` |
|      - | 6729 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 6730 | ` *  Parse a configuration string.` |
|      - | 6731 | ` * Parameters` |
|      - | 6732 | ` *  $ini` |
|      - | 6733 | ` *   The contents of the ini file being parsed.` |
|      - | 6734 | ` *  $process_sections` |
|      - | 6735 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 6736 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 6737 | ` *  $scanner_mode (Not used)` |
|      - | 6738 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 6739 | ` *   then option values will not be parsed.` |
|      - | 6740 | ` * Return` |
|      - | 6741 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 6742 | ` */` |
|     10 | 6743 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6744 |  |
|      - | 6745 | `	const char *zIni;` |
|      - | 6746 | `	int nByte;` |
|     11 | 6747 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6748 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 6749 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6750 | `		return PH7_OK;` |
|      - | 6751 | `	}` |
|      - | 6752 | `	/* Extract the raw INI buffer */` |
|     11 | 6753 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 6754 | `	/* Process the INI buffer*/` |
|     11 | 6755 | `	PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|     11 | 6756 | `	return PH7_OK;` |
|      6 | 6757 |  |
|      - | 6758 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6759 |  |
|      - | 6760 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6761 |  |
|      - | 6762 | `/*` |
|      - | 6763 | ` * Ctype Functions.` |
|      - | 6764 | ` * Status:` |
|      - | 6765 | ` *    Stable.` |
|      - | 6766 | ` */` |
|      - | 6767 | `/*` |
|      - | 6768 | ` * bool ctype_alnum(string $text)` |
|      - | 6769 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 6770 | ` * Parameters` |
|      - | 6771 | ` *  $text` |
|      - | 6772 | ` *   The tested string.` |
|      - | 6773 | ` * Return` |
|      - | 6774 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 6775 | ` */` |
|     16 | 6776 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6777 |  |
|      - | 6778 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6779 | `	int nLen;` |
|     17 | 6780 | `	if( nArg < 1 ){` |
|      - | 6781 | `		/* Missing arguments,return FALSE */` |
|      3 | 6782 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6783 | `		return PH7_OK;` |
|      - | 6784 | `	}` |
|      - | 6785 | `	/* Extract the target string */` |
|     15 | 6786 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 6787 | `	zEnd = &zIn[nLen];` |
|     15 | 6788 | `	if( nLen < 1 ){` |
|      - | 6789 | `		/* Empty string,return FALSE */` |
|      3 | 6790 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6791 | `		return PH7_OK;` |
|      - | 6792 | `	}` |
|      - | 6793 | `	/* Perform the requested operation */` |
|     32 | 6794 | `	for(;;){` |
|     65 | 6795 | `		if( zIn >= zEnd ){` |
|      - | 6796 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6797 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6798 | `			return PH7_OK;` |
|      - | 6799 | `		}` |
|     57 | 6800 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 6801 | `			break;` |
|      - | 6802 | `		}` |
|      - | 6803 | `		/* Point to the next character */` |
|     53 | 6804 | `		zIn++;` |
|      1 | 6805 | `	}` |
|      - | 6806 | `	/* The test failed,return FALSE */` |
|      5 | 6807 | `	ph7_result_bool(pCtx,0);` |
|      5 | 6808 | `	return PH7_OK;` |
|      9 | 6809 |  |
|      - | 6810 | `/*` |
|      - | 6811 | ` * bool ctype_alpha(string $text)` |
|      - | 6812 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 6813 | ` * Parameters` |
|      - | 6814 | ` *  $text` |
|      - | 6815 | ` *   The tested string.` |
|      - | 6816 | ` * Return` |
|      - | 6817 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 6818 | ` */` |
|     18 | 6819 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6820 |  |
|      - | 6821 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6822 | `	int nLen;` |
|     19 | 6823 | `	if( nArg < 1 ){` |
|      - | 6824 | `		/* Missing arguments,return FALSE */` |
|      3 | 6825 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6826 | `		return PH7_OK;` |
|      - | 6827 | `	}` |
|      - | 6828 | `	/* Extract the target string */` |
|     17 | 6829 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6830 | `	zEnd = &zIn[nLen];` |
|     17 | 6831 | `	if( nLen < 1 ){` |
|      - | 6832 | `		/* Empty string,return FALSE */` |
|      3 | 6833 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6834 | `		return PH7_OK;` |
|      - | 6835 | `	}` |
|      - | 6836 | `	/* Perform the requested operation */` |
|     42 | 6837 | `	for(;;){` |
|     85 | 6838 | `		if( zIn >= zEnd ){` |
|      - | 6839 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6840 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6841 | `			return PH7_OK;` |
|      - | 6842 | `		}` |
|     77 | 6843 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 6844 | `			break;` |
|      - | 6845 | `		}` |
|      - | 6846 | `		/* Point to the next character */` |
|     71 | 6847 | `		zIn++;` |
|      1 | 6848 | `	}` |
|      - | 6849 | `	/* The test failed,return FALSE */` |
|      7 | 6850 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6851 | `	return PH7_OK;` |
|     10 | 6852 |  |
|      - | 6853 | `/*` |
|      - | 6854 | ` * bool ctype_cntrl(string $text)` |
|      - | 6855 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 6856 | ` * Parameters` |
|      - | 6857 | ` *  $text` |
|      - | 6858 | ` *   The tested string.` |
|      - | 6859 | ` * Return` |
|      - | 6860 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 6861 | ` */` |
|     18 | 6862 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6863 |  |
|      - | 6864 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6865 | `	int nLen;` |
|     19 | 6866 | `	if( nArg < 1 ){` |
|      - | 6867 | `		/* Missing arguments,return FALSE */` |
|      3 | 6868 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6869 | `		return PH7_OK;` |
|      - | 6870 | `	}` |
|      - | 6871 | `	/* Extract the target string */` |
|     17 | 6872 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6873 | `	zEnd = &zIn[nLen];` |
|     17 | 6874 | `	if( nLen < 1 ){` |
|      - | 6875 | `		/* Empty string,return FALSE */` |
|      3 | 6876 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6877 | `		return PH7_OK;` |
|      - | 6878 | `	}` |
|      - | 6879 | `	/* Perform the requested operation */` |
|     14 | 6880 | `	for(;;){` |
|     29 | 6881 | `		if( zIn >= zEnd ){` |
|      - | 6882 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6883 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6884 | `			return PH7_OK;` |
|      - | 6885 | `		}` |
|     21 | 6886 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6887 | `			/* UTF-8 stream  */` |
|    ! 0 | 6888 | `			break;` |
|      - | 6889 | `		}` |
|     21 | 6890 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 6891 | `			break;` |
|      - | 6892 | `		}` |
|      - | 6893 | `		/* Point to the next character */` |
|     15 | 6894 | `		zIn++;` |
|      1 | 6895 | `	}` |
|      - | 6896 | `	/* The test failed,return FALSE */` |
|      7 | 6897 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6898 | `	return PH7_OK;` |
|     10 | 6899 |  |
|      - | 6900 | `/*` |
|      - | 6901 | ` * bool ctype_digit(string $text)` |
|      - | 6902 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 6903 | ` * Parameters` |
|      - | 6904 | ` *  $text` |
|      - | 6905 | ` *   The tested string.` |
|      - | 6906 | ` * Return` |
|      - | 6907 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 6908 | ` */` |
|   1572 | 6909 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6910 |  |
|      - | 6911 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6912 | `	int nLen;` |
|   1574 | 6913 | `	if( nArg < 1 ){` |
|      - | 6914 | `		/* Missing arguments,return FALSE */` |
|      3 | 6915 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6916 | `		return PH7_OK;` |
|      - | 6917 | `	}` |
|      - | 6918 | `	/* Extract the target string */` |
|   1572 | 6919 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1572 | 6920 | `	zEnd = &zIn[nLen];` |
|   1572 | 6921 | `	if( nLen < 1 ){` |
|      - | 6922 | `		/* Empty string,return FALSE */` |
|      3 | 6923 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6924 | `		return PH7_OK;` |
|      - | 6925 | `	}` |
|      - | 6926 | `	/* Perform the requested operation */` |
|   1464 | 6927 | `	for(;;){` |
|   2930 | 6928 | `		if( zIn >= zEnd ){` |
|      - | 6929 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1330 | 6930 | `			ph7_result_bool(pCtx,1);` |
|   1330 | 6931 | `			return PH7_OK;` |
|      - | 6932 | `		}` |
|   1602 | 6933 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6934 | `			/* UTF-8 stream  */` |
|    ! 0 | 6935 | `			break;` |
|      - | 6936 | `		}` |
|   1602 | 6937 | `		if( !SyisDigit(zIn[0]) ){` |
|    242 | 6938 | `			break;` |
|      - | 6939 | `		}` |
|      - | 6940 | `		/* Point to the next character */` |
|   1362 | 6941 | `		zIn++;` |
|      2 | 6942 | `	}` |
|      - | 6943 | `	/* The test failed,return FALSE */` |
|    242 | 6944 | `	ph7_result_bool(pCtx,0);` |
|    242 | 6945 | `	return PH7_OK;` |
|    788 | 6946 |  |
|      - | 6947 | `/*` |
|      - | 6948 | ` * bool ctype_xdigit(string $text)` |
|      - | 6949 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 6950 | ` * Parameters` |
|      - | 6951 | ` *  $text` |
|      - | 6952 | ` *   The tested string.` |
|      - | 6953 | ` * Return` |
|      - | 6954 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 6955 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 6956 | ` */` |
|     20 | 6957 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6958 |  |
|      - | 6959 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6960 | `	int nLen;` |
|     21 | 6961 | `	if( nArg < 1 ){` |
|      - | 6962 | `		/* Missing arguments,return FALSE */` |
|      3 | 6963 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6964 | `		return PH7_OK;` |
|      - | 6965 | `	}` |
|      - | 6966 | `	/* Extract the target string */` |
|     19 | 6967 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 6968 | `	zEnd = &zIn[nLen];` |
|     19 | 6969 | `	if( nLen < 1 ){` |
|      - | 6970 | `		/* Empty string,return FALSE */` |
|      3 | 6971 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6972 | `		return PH7_OK;` |
|      - | 6973 | `	}` |
|      - | 6974 | `	/* Perform the requested operation */` |
|     46 | 6975 | `	for(;;){` |
|     93 | 6976 | `		if( zIn >= zEnd ){` |
|      - | 6977 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 6978 | `			ph7_result_bool(pCtx,1);` |
|     11 | 6979 | `			return PH7_OK;` |
|      - | 6980 | `		}` |
|     83 | 6981 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6982 | `			/* UTF-8 stream  */` |
|    ! 0 | 6983 | `			break;` |
|      - | 6984 | `		}` |
|     83 | 6985 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 6986 | `			break;` |
|      - | 6987 | `		}` |
|      - | 6988 | `		/* Point to the next character */` |
|     77 | 6989 | `		zIn++;` |
|      1 | 6990 | `	}` |
|      - | 6991 | `	/* The test failed,return FALSE */` |
|      7 | 6992 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6993 | `	return PH7_OK;` |
|     11 | 6994 |  |
|      - | 6995 | `/*` |
|      - | 6996 | ` * bool ctype_graph(string $text)` |
|      - | 6997 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 6998 | ` * Parameters` |
|      - | 6999 | ` *  $text` |
|      - | 7000 | ` *   The tested string.` |
|      - | 7001 | ` * Return` |
|      - | 7002 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 7003 | ` * (no white space), FALSE otherwise.` |
|      - | 7004 | ` */` |
|     18 | 7005 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7006 |  |
|      - | 7007 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7008 | `	int nLen;` |
|     19 | 7009 | `	if( nArg < 1 ){` |
|      - | 7010 | `		/* Missing arguments,return FALSE */` |
|      3 | 7011 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7012 | `		return PH7_OK;` |
|      - | 7013 | `	}` |
|      - | 7014 | `	/* Extract the target string */` |
|     17 | 7015 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7016 | `	zEnd = &zIn[nLen];` |
|     17 | 7017 | `	if( nLen < 1 ){` |
|      - | 7018 | `		/* Empty string,return FALSE */` |
|      3 | 7019 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7020 | `		return PH7_OK;` |
|      - | 7021 | `	}` |
|      - | 7022 | `	/* Perform the requested operation */` |
|     57 | 7023 | `	for(;;){` |
|    115 | 7024 | `		if( zIn >= zEnd ){` |
|      - | 7025 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7026 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7027 | `			return PH7_OK;` |
|      - | 7028 | `		}` |
|    107 | 7029 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7030 | `			/* UTF-8 stream  */` |
|    ! 0 | 7031 | `			break;` |
|      - | 7032 | `		}` |
|    107 | 7033 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 7034 | `			break;` |
|      - | 7035 | `		}` |
|      - | 7036 | `		/* Point to the next character */` |
|    101 | 7037 | `		zIn++;` |
|      1 | 7038 | `	}` |
|      - | 7039 | `	/* The test failed,return FALSE */` |
|      7 | 7040 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7041 | `	return PH7_OK;` |
|     10 | 7042 |  |
|      - | 7043 | `/*` |
|      - | 7044 | ` * bool ctype_print(string $text)` |
|      - | 7045 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 7046 | ` * Parameters` |
|      - | 7047 | ` *  $text` |
|      - | 7048 | ` *   The tested string.` |
|      - | 7049 | ` * Return` |
|      - | 7050 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 7051 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 7052 | ` *  or control function at all.` |
|      - | 7053 | ` */` |
|     18 | 7054 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7055 |  |
|      - | 7056 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7057 | `	int nLen;` |
|     19 | 7058 | `	if( nArg < 1 ){` |
|      - | 7059 | `		/* Missing arguments,return FALSE */` |
|      3 | 7060 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7061 | `		return PH7_OK;` |
|      - | 7062 | `	}` |
|      - | 7063 | `	/* Extract the target string */` |
|     17 | 7064 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7065 | `	zEnd = &zIn[nLen];` |
|     17 | 7066 | `	if( nLen < 1 ){` |
|      - | 7067 | `		/* Empty string,return FALSE */` |
|      3 | 7068 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7069 | `		return PH7_OK;` |
|      - | 7070 | `	}` |
|      - | 7071 | `	/* Perform the requested operation */` |
|     63 | 7072 | `	for(;;){` |
|    127 | 7073 | `		if( zIn >= zEnd ){` |
|      - | 7074 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7075 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7076 | `			return PH7_OK;` |
|      - | 7077 | `		}` |
|    119 | 7078 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7079 | `			/* UTF-8 stream  */` |
|    ! 0 | 7080 | `			break;` |
|      - | 7081 | `		}` |
|    119 | 7082 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 7083 | `			break;` |
|      - | 7084 | `		}` |
|      - | 7085 | `		/* Point to the next character */` |
|    113 | 7086 | `		zIn++;` |
|      1 | 7087 | `	}` |
|      - | 7088 | `	/* The test failed,return FALSE */` |
|      7 | 7089 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7090 | `	return PH7_OK;` |
|     10 | 7091 |  |
|      - | 7092 | `/*` |
|      - | 7093 | ` * bool ctype_punct(string $text)` |
|      - | 7094 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 7095 | ` * Parameters` |
|      - | 7096 | ` *  $text` |
|      - | 7097 | ` *   The tested string.` |
|      - | 7098 | ` * Return` |
|      - | 7099 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 7100 | ` *  digit or blank, FALSE otherwise.` |
|      - | 7101 | ` */` |
|     20 | 7102 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7103 |  |
|      - | 7104 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7105 | `	int nLen;` |
|     21 | 7106 | `	if( nArg < 1 ){` |
|      - | 7107 | `		/* Missing arguments,return FALSE */` |
|      3 | 7108 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7109 | `		return PH7_OK;` |
|      - | 7110 | `	}` |
|      - | 7111 | `	/* Extract the target string */` |
|     19 | 7112 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 7113 | `	zEnd = &zIn[nLen];` |
|     19 | 7114 | `	if( nLen < 1 ){` |
|      - | 7115 | `		/* Empty string,return FALSE */` |
|      3 | 7116 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7117 | `		return PH7_OK;` |
|      - | 7118 | `	}` |
|      - | 7119 | `	/* Perform the requested operation */` |
|     38 | 7120 | `	for(;;){` |
|     77 | 7121 | `		if( zIn >= zEnd ){` |
|      - | 7122 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7123 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7124 | `			return PH7_OK;` |
|      - | 7125 | `		}` |
|     69 | 7126 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7127 | `			/* UTF-8 stream  */` |
|    ! 0 | 7128 | `			break;` |
|      - | 7129 | `		}` |
|     69 | 7130 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 7131 | `			break;` |
|      - | 7132 | `		}` |
|      - | 7133 | `		/* Point to the next character */` |
|     61 | 7134 | `		zIn++;` |
|      1 | 7135 | `	}` |
|      - | 7136 | `	/* The test failed,return FALSE */` |
|      9 | 7137 | `	ph7_result_bool(pCtx,0);` |
|      9 | 7138 | `	return PH7_OK;` |
|     11 | 7139 |  |
|      - | 7140 | `/*` |
|      - | 7141 | ` * bool ctype_space(string $text)` |
|      - | 7142 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 7143 | ` * Parameters` |
|      - | 7144 | ` *  $text` |
|      - | 7145 | ` *   The tested string.` |
|      - | 7146 | ` * Return` |
|      - | 7147 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 7148 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 7149 | ` *  and form feed characters.` |
|      - | 7150 | ` */` |
|  42860 | 7151 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 7152 |  |
|      - | 7153 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7154 | `	int nLen;` |
|  42862 | 7155 | `	if( nArg < 1 ){` |
|      - | 7156 | `		/* Missing arguments,return FALSE */` |
|      3 | 7157 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7158 | `		return PH7_OK;` |
|      - | 7159 | `	}` |
|      - | 7160 | `	/* Extract the target string */` |
|  42860 | 7161 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  42860 | 7162 | `	zEnd = &zIn[nLen];` |
|  42860 | 7163 | `	if( nLen < 1 ){` |
|      - | 7164 | `		/* Empty string,return FALSE */` |
|      3 | 7165 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7166 | `		return PH7_OK;` |
|      - | 7167 | `	}` |
|      - | 7168 | `	/* Perform the requested operation */` |
|  21836 | 7169 | `	for(;;){` |
|  43630 | 7170 | `		if( zIn >= zEnd ){` |
|      - | 7171 | `			/* If we reach the end of the string,then the test succeeded. */` |
|    750 | 7172 | `			ph7_result_bool(pCtx,1);` |
|    750 | 7173 | `			return PH7_OK;` |
|      - | 7174 | `		}` |
|  42882 | 7175 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7176 | `			/* UTF-8 stream  */` |
|    ! 0 | 7177 | `			break;` |
|      - | 7178 | `		}` |
|  42882 | 7179 | `		if( !SyisSpace(zIn[0]) ){` |
|  42110 | 7180 | `			break;` |
|      - | 7181 | `		}` |
|      - | 7182 | `		/* Point to the next character */` |
|    774 | 7183 | `		zIn++;` |
|      2 | 7184 | `	}` |
|      - | 7185 | `	/* The test failed,return FALSE */` |
|  42110 | 7186 | `	ph7_result_bool(pCtx,0);` |
|  42110 | 7187 | `	return PH7_OK;` |
|  21454 | 7188 |  |
|      - | 7189 | `/*` |
|      - | 7190 | ` * bool ctype_lower(string $text)` |
|      - | 7191 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 7192 | ` * Parameters` |
|      - | 7193 | ` *  $text` |
|      - | 7194 | ` *   The tested string.` |
|      - | 7195 | ` * Return` |
|      - | 7196 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 7197 | ` */` |
|     18 | 7198 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7199 |  |
|      - | 7200 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7201 | `	int nLen;` |
|     19 | 7202 | `	if( nArg < 1 ){` |
|      - | 7203 | `		/* Missing arguments,return FALSE */` |
|      3 | 7204 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7205 | `		return PH7_OK;` |
|      - | 7206 | `	}` |
|      - | 7207 | `	/* Extract the target string */` |
|     17 | 7208 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7209 | `	zEnd = &zIn[nLen];` |
|     17 | 7210 | `	if( nLen < 1 ){` |
|      - | 7211 | `		/* Empty string,return FALSE */` |
|      3 | 7212 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7213 | `		return PH7_OK;` |
|      - | 7214 | `	}` |
|      - | 7215 | `	/* Perform the requested operation */` |
|     27 | 7216 | `	for(;;){` |
|     55 | 7217 | `		if( zIn >= zEnd ){` |
|      - | 7218 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7219 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7220 | `			return PH7_OK;` |
|      - | 7221 | `		}` |
|     51 | 7222 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 7223 | `			break;` |
|      - | 7224 | `		}` |
|      - | 7225 | `		/* Point to the next character */` |
|     41 | 7226 | `		zIn++;` |
|      1 | 7227 | `	}` |
|      - | 7228 | `	/* The test failed,return FALSE */` |
|     11 | 7229 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7230 | `	return PH7_OK;` |
|     10 | 7231 |  |
|      - | 7232 | `/*` |
|      - | 7233 | ` * bool ctype_upper(string $text)` |
|      - | 7234 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 7235 | ` * Parameters` |
|      - | 7236 | ` *  $text` |
|      - | 7237 | ` *   The tested string.` |
|      - | 7238 | ` * Return` |
|      - | 7239 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 7240 | ` */` |
|     18 | 7241 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7242 |  |
|      - | 7243 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7244 | `	int nLen;` |
|     19 | 7245 | `	if( nArg < 1 ){` |
|      - | 7246 | `		/* Missing arguments,return FALSE */` |
|      3 | 7247 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7248 | `		return PH7_OK;` |
|      - | 7249 | `	}` |
|      - | 7250 | `	/* Extract the target string */` |
|     17 | 7251 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7252 | `	zEnd = &zIn[nLen];` |
|     17 | 7253 | `	if( nLen < 1 ){` |
|      - | 7254 | `		/* Empty string,return FALSE */` |
|      3 | 7255 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7256 | `		return PH7_OK;` |
|      - | 7257 | `	}` |
|      - | 7258 | `	/* Perform the requested operation */` |
|     28 | 7259 | `	for(;;){` |
|     57 | 7260 | `		if( zIn >= zEnd ){` |
|      - | 7261 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7262 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7263 | `			return PH7_OK;` |
|      - | 7264 | `		}` |
|     53 | 7265 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 7266 | `			break;` |
|      - | 7267 | `		}` |
|      - | 7268 | `		/* Point to the next character */` |
|     43 | 7269 | `		zIn++;` |
|      1 | 7270 | `	}` |
|      - | 7271 | `	/* The test failed,return FALSE */` |
|     11 | 7272 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7273 | `	return PH7_OK;` |
|     10 | 7274 |  |
|      - | 7275 | `/*` |
|      - | 7276 | ` * Date/Time functions` |
|      - | 7277 | ` * Status:` |
|      - | 7278 | ` *    Devel.` |
|      - | 7279 | ` */` |
|      - | 7280 | `#include <time.h>` |
|      - | 7281 | `#ifdef __WINNT__` |
|      - | 7282 | `/* GetSystemTime() */` |
|      - | 7283 | `#include <Windows.h>` |
|      - | 7284 | `#ifdef _WIN32_WCE` |
|      - | 7285 | `/*` |
|      - | 7286 | `** WindowsCE does not have a localtime() function.  So create a` |
|      - | 7287 | `** substitute.` |
|      - | 7288 | `** Taken from the SQLite3 source tree.` |
|      - | 7289 | `** Status: Public domain` |
|      - | 7290 | `*/` |
|      - | 7291 | `struct tm *__cdecl localtime(const time_t *t)` |
|      - | 7292 |  |
|      - | 7293 | `  static struct tm y;` |
|      - | 7294 | `  FILETIME uTm, lTm;` |
|      - | 7295 | `  SYSTEMTIME pTm;` |
|      - | 7296 | `  ph7_int64 t64;` |
|      - | 7297 | `  t64 = *t;` |
|      - | 7298 | `  t64 = (t64 + 11644473600)*10000000;` |
|      - | 7299 | `  uTm.dwLowDateTime = (DWORD)(t64 & 0xFFFFFFFF);` |
|      - | 7300 | `  uTm.dwHighDateTime= (DWORD)(t64 >> 32);` |
|      - | 7301 | `  FileTimeToLocalFileTime(&uTm,&lTm);` |
|      - | 7302 | `  FileTimeToSystemTime(&lTm,&pTm);` |
|      - | 7303 | `  y.tm_year = pTm.wYear - 1900;` |
|      - | 7304 | `  y.tm_mon = pTm.wMonth - 1;` |
|      - | 7305 | `  y.tm_wday = pTm.wDayOfWeek;` |
|      - | 7306 | `  y.tm_mday = pTm.wDay;` |
|      - | 7307 | `  y.tm_hour = pTm.wHour;` |
|      - | 7308 | `  y.tm_min = pTm.wMinute;` |
|      - | 7309 | `  y.tm_sec = pTm.wSecond;` |
|      - | 7310 | `  return &y;` |
|      - | 7311 |  |
|      - | 7312 | `#endif /*_WIN32_WCE */` |
|      - | 7313 | `#elif defined(__UNIXES__)` |
|      - | 7314 | `#include <sys/time.h>` |
|      - | 7315 | `#endif /* __WINNT__*/` |
|      - | 7316 | ` /*` |
|      - | 7317 | `  * int64 time(void)` |
|      - | 7318 | `  *  Current Unix timestamp` |
|      - | 7319 | `  * Parameters` |
|      - | 7320 | `  *  None.` |
|      - | 7321 | `  * Return` |
|      - | 7322 | `  *  Returns the current time measured in the number of seconds` |
|      - | 7323 | `  *  since the Unix Epoch (January 1 1970 00:00:00 GMT).` |
|      - | 7324 | `  */` |
|      8 | 7325 | `static int PH7_builtin_time(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7326 |  |
|      - | 7327 | `	time_t tt;` |
|      4 | 7328 | `	SXUNUSED(nArg); /* cc warning */` |
|      4 | 7329 | `	SXUNUSED(apArg);` |
|      - | 7330 | `	/* Extract the current time */` |
|      9 | 7331 | `	time(&tt);` |
|      - | 7332 | `	/* Return as 64-bit integer */` |
|      9 | 7333 | `	ph7_result_int64(pCtx,(ph7_int64)tt);` |
|      9 | 7334 | `	return  PH7_OK;` |
|      1 | 7335 |  |
|      - | 7336 | `/*` |
|      - | 7337 | `  * string/float microtime([ bool $get_as_float = false ])` |
|      - | 7338 | `  *  microtime() returns the current Unix timestamp with microseconds.` |
|      - | 7339 | `  * Parameters` |
|      - | 7340 | `  *  $get_as_float` |
|      - | 7341 | `  *   If used and set to TRUE, microtime() will return a float instead of a string` |
|      - | 7342 | `  *   as described in the return values section below.` |
|      - | 7343 | `  * Return` |
|      - | 7344 | `  *  By default, microtime() returns a string in the form "msec sec", where sec` |
|      - | 7345 | `  *  is the current time measured in the number of seconds since the Unix` |
|      - | 7346 | `  *  epoch (0:00:00 January 1, 1970 GMT), and msec is the number of microseconds` |
|      - | 7347 | `  *  that have elapsed since sec expressed in seconds.` |
|      - | 7348 | `  *  If get_as_float is set to TRUE, then microtime() returns a float, which represents` |
|      - | 7349 | `  *  the current time in seconds since the Unix epoch accurate to the nearest microsecond.` |
|      - | 7350 | `  */` |
|     20 | 7351 | `static int PH7_builtin_microtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7352 |  |
|     21 | 7353 | `	int bFloat = 0;` |
|      - | 7354 | `	sytime sTime;` |
|      - | 7355 | `#if defined(__UNIXES__)` |
|      - | 7356 | `	struct timeval tv;` |
|     20 | 7357 | `	gettimeofday(&tv,0);` |
|     20 | 7358 | `	sTime.tm_sec  = (long)tv.tv_sec;` |
|     20 | 7359 | `	sTime.tm_usec = (long)tv.tv_usec;` |
|      - | 7360 | `#else` |
|      - | 7361 | `	time_t tt;` |
|      1 | 7362 | `	time(&tt);` |
|      1 | 7363 | `	sTime.tm_sec  = (long)tt;` |
|      1 | 7364 | `	sTime.tm_usec = (long)(tt%SX_USEC_PER_SEC);` |
|      - | 7365 | `#endif /* __UNIXES__ */` |
|     21 | 7366 | `	if( nArg > 0 ){` |
|     17 | 7367 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|      8 | 7368 | `	}` |
|     21 | 7369 | `	if( bFloat ){` |
|      - | 7370 | `		/* Return as float */` |
|     17 | 7371 | `		ph7_result_double(pCtx,(double)sTime.tm_sec);` |
|      9 | 7372 | `	}else{` |
|      - | 7373 | `		/* Return as string */` |
|      5 | 7374 | `		ph7_result_string_format(pCtx,"%ld %ld",sTime.tm_usec,sTime.tm_sec);` |
|      - | 7375 | `	}` |
|     21 | 7376 | `	return PH7_OK;` |
|      1 | 7377 |  |
|      - | 7378 | `/*` |
|      - | 7379 | ` * array getdate ([ int $timestamp = time() ])` |
|      - | 7380 | ` *  Get date/time information.` |
|      - | 7381 | ` * Parameter` |
|      - | 7382 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|      - | 7383 | ` *     that defaults to the current local time if a timestamp is not given.` |
|      - | 7384 | ` *     In other words, it defaults to the value of time().` |
|      - | 7385 | ` * Returns` |
|      - | 7386 | ` *  Returns an associative array of information related to the timestamp.` |
|      - | 7387 | ` *  Elements from the returned associative array are as follows:` |
|      - | 7388 | ` *   KEY                                                         VALUE` |
|      - | 7389 | ` * ---------                                                    -------` |
|      - | 7390 | ` * "seconds" 	Numeric representation of seconds 	            0 to 59` |
|      - | 7391 | ` * "minutes" 	Numeric representation of minutes 	            0 to 59` |
|      - | 7392 | ` * "hours" 	    Numeric representation of hours 	            0 to 23` |
|      - | 7393 | ` * "mday" 	    Numeric representation of the day of the month 	1 to 31` |
|      - | 7394 | ` * "wday" 	    Numeric representation of the day of the week 	0 (for Sunday) through 6 (for Saturday)` |
|      - | 7395 | ` * "mon" 	    Numeric representation of a month 	            1 through 12` |
|      - | 7396 | ` * "year" 	    A full numeric representation of a year,        4 digits 	Examples: 1999 or 2003` |
|      - | 7397 | ` * "yday" 	    Numeric representation of the day of the year   0 through 365` |
|      - | 7398 | ` * "weekday" 	A full textual representation of the day of the week 	Sunday through Saturday` |
|      - | 7399 | ` * "month" 	    A full textual representation of a month, such as January or March 	January through December` |
|      - | 7400 | ` * 0 	        Seconds since the Unix Epoch, similar to the values returned by time() and used by date().` |
|      - | 7401 | ` * NOTE:` |
|      - | 7402 | ` *   NULL is returned on failure.` |
|      - | 7403 | ` */` |
|      8 | 7404 | `static int PH7_builtin_getdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7405 |  |
|      - | 7406 | `	ph7_value *pValue,*pArray;` |
|      - | 7407 | `	Sytm sTm;` |
|      9 | 7408 | `	if( nArg < 1 ){` |
|      - | 7409 | `#ifdef __WINNT__` |
|      - | 7410 | `		SYSTEMTIME sOS;` |
|      1 | 7411 | `		GetSystemTime(&sOS);` |
|      1 | 7412 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 7413 | `#else` |
|      - | 7414 | `		struct tm *pTm;` |
|      - | 7415 | `		time_t t;` |
|      4 | 7416 | `		time(&t);` |
|      4 | 7417 | `		pTm = localtime(&t);` |
|      4 | 7418 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7419 | `#endif` |
|      3 | 7420 | `	}else{` |
|      - | 7421 | `		/* Use the given timestamp */` |
|      - | 7422 | `		time_t t;` |
|      - | 7423 | `		struct tm *pTm;` |
|      - | 7424 | `#ifdef __WINNT__` |
|      - | 7425 | `#ifdef _MSC_VER` |
|      - | 7426 | `#if _MSC_VER >= 1400 /* Visual Studio 2005 and up */` |
|      - | 7427 | `#pragma warning(disable:4996) /* _CRT_SECURE...*/` |
|      - | 7428 | `#endif` |
|      - | 7429 | `#endif` |
|      - | 7430 | `#endif` |
|      5 | 7431 | `		if( ph7_value_is_int(apArg[0]) ){` |
|      5 | 7432 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|      5 | 7433 | `			pTm = localtime(&t);` |
|      5 | 7434 | `			if( pTm == 0 ){` |
|    ! 0 | 7435 | `				time(&t);` |
|    ! 0 | 7436 | `			}` |
|      3 | 7437 | `		}else{` |
|    ! 0 | 7438 | `			time(&t);` |
|      - | 7439 | `		}` |
|      5 | 7440 | `		pTm = localtime(&t);` |
|      5 | 7441 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7442 | `	}` |
|      - | 7443 | `	/* Element value */` |
|      9 | 7444 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      9 | 7445 | `	if( pValue == 0 ){` |
|      - | 7446 | `		/* Return NULL */` |
|    ! 0 | 7447 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7448 | `		return PH7_OK;` |
|      - | 7449 | `	}` |
|      - | 7450 | `	/* Create a new array */` |
|      9 | 7451 | `	pArray = ph7_context_new_array(pCtx);` |
|      9 | 7452 | `	if( pArray == 0 ){` |
|      - | 7453 | `		/* Return NULL */` |
|    ! 0 | 7454 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7455 | `		return PH7_OK;` |
|      - | 7456 | `	}` |
|      - | 7457 | `	/* Fill the array */` |
|      - | 7458 | `	/* Seconds */` |
|      9 | 7459 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|      9 | 7460 | `	ph7_array_add_strkey_elem(pArray,"seconds",pValue);` |
|      - | 7461 | `	/* Minutes */` |
|      9 | 7462 | `	ph7_value_int(pValue,sTm.tm_min);` |
|      9 | 7463 | `	ph7_array_add_strkey_elem(pArray,"minutes",pValue);` |
|      - | 7464 | `	/* Hours */` |
|      9 | 7465 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|      9 | 7466 | `	ph7_array_add_strkey_elem(pArray,"hours",pValue);` |
|      - | 7467 | `	/* mday */` |
|      9 | 7468 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|      9 | 7469 | `	ph7_array_add_strkey_elem(pArray,"mday",pValue);` |
|      - | 7470 | `	/* wday */` |
|      9 | 7471 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|      9 | 7472 | `	ph7_array_add_strkey_elem(pArray,"wday",pValue);` |
|      - | 7473 | `	/* mon */` |
|      9 | 7474 | `	ph7_value_int(pValue,sTm.tm_mon+1);` |
|      9 | 7475 | `	ph7_array_add_strkey_elem(pArray,"mon",pValue);` |
|      - | 7476 | `	/* year */` |
|      9 | 7477 | `	ph7_value_int(pValue,sTm.tm_year);` |
|      9 | 7478 | `	ph7_array_add_strkey_elem(pArray,"year",pValue);` |
|      - | 7479 | `	/* yday */` |
|      9 | 7480 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|      9 | 7481 | `	ph7_array_add_strkey_elem(pArray,"yday",pValue);` |
|      - | 7482 | `	/* Weekday */` |
|      9 | 7483 | `	ph7_value_string(pValue,SyTimeGetDay(sTm.tm_wday),-1);` |
|      9 | 7484 | `	ph7_array_add_strkey_elem(pArray,"weekday",pValue);` |
|      - | 7485 | `	/* Month */` |
|      9 | 7486 | `	ph7_value_reset_string_cursor(pValue);` |
|      9 | 7487 | `	ph7_value_string(pValue,SyTimeGetMonth(sTm.tm_mon),-1);` |
|      9 | 7488 | `	ph7_array_add_strkey_elem(pArray,"month",pValue);` |
|      - | 7489 | `	/* Seconds since the epoch */` |
|      9 | 7490 | `	ph7_value_int64(pValue,(ph7_int64)time(0));` |
|      9 | 7491 | `	ph7_array_add_intkey_elem(pArray,0 /* Index zero */,pValue);` |
|      - | 7492 | `	/* Return the freshly created array */` |
|      9 | 7493 | `	ph7_result_value(pCtx,pArray);` |
|      9 | 7494 | `	return PH7_OK;` |
|      5 | 7495 |  |
|      - | 7496 | `/*` |
|      - | 7497 | ` * mixed gettimeofday([ bool $return_float = false ] )` |
|      - | 7498 | ` *   Returns an associative array containing the data returned from the system call.` |
|      - | 7499 | ` * Parameters` |
|      - | 7500 | ` *  $return_float` |
|      - | 7501 | ` *   When set to TRUE, a float instead of an array is returned.` |
|      - | 7502 | ` * Return` |
|      - | 7503 | ` *   By default an array is returned. If return_float is set, then` |
|      - | 7504 | ` *   a float is returned.` |
|      - | 7505 | ` */` |
|      4 | 7506 | `static int PH7_builtin_gettimeofday(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7507 |  |
|      5 | 7508 | `	int bFloat = 0;` |
|      - | 7509 | `	sytime sTime;` |
|      - | 7510 | `#if defined(__UNIXES__)` |
|      - | 7511 | `	struct timeval tv;` |
|      4 | 7512 | `	gettimeofday(&tv,0);` |
|      4 | 7513 | `	sTime.tm_sec  = (long)tv.tv_sec;` |
|      4 | 7514 | `	sTime.tm_usec = (long)tv.tv_usec;` |
|      - | 7515 | `#else` |
|      - | 7516 | `	time_t tt;` |
|      1 | 7517 | `	time(&tt);` |
|      1 | 7518 | `	sTime.tm_sec  = (long)tt;` |
|      1 | 7519 | `	sTime.tm_usec = (long)(tt%SX_USEC_PER_SEC);` |
|      - | 7520 | `#endif /* __UNIXES__ */` |
|      5 | 7521 | `	if( nArg > 0 ){` |
|      5 | 7522 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|      2 | 7523 | `	}` |
|      5 | 7524 | `	if( bFloat ){` |
|      - | 7525 | `		/* Return as float */` |
|      3 | 7526 | `		ph7_result_double(pCtx,(double)sTime.tm_sec);` |
|      2 | 7527 | `	}else{` |
|      - | 7528 | `		/* Return an associative array */` |
|      - | 7529 | `		ph7_value *pValue,*pArray;` |
|      - | 7530 | `		/* Create a new array */` |
|      3 | 7531 | `		pArray = ph7_context_new_array(pCtx);` |
|      - | 7532 | `		/* Element value */` |
|      3 | 7533 | `		pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 7534 | `		if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 7535 | `			/* Return NULL */` |
|    ! 0 | 7536 | `			ph7_result_null(pCtx);` |
|    ! 0 | 7537 | `			return PH7_OK;` |
|      - | 7538 | `		}` |
|      - | 7539 | `		/* Fill the array */` |
|      - | 7540 | `		/* sec */` |
|      3 | 7541 | `		ph7_value_int64(pValue,sTime.tm_sec);` |
|      3 | 7542 | `		ph7_array_add_strkey_elem(pArray,"sec",pValue);` |
|      - | 7543 | `		/* usec */` |
|      3 | 7544 | `		ph7_value_int64(pValue,sTime.tm_usec);` |
|      3 | 7545 | `		ph7_array_add_strkey_elem(pArray,"usec",pValue);` |
|      - | 7546 | `		/* Return the array */` |
|      3 | 7547 | `		ph7_result_value(pCtx,pArray);` |
|      - | 7548 | `	}` |
|      5 | 7549 | `	return PH7_OK;` |
|      3 | 7550 |  |
|      - | 7551 | `/* Check if the given year is leap or not */` |
|      - | 7552 | `#define IS_LEAP_YEAR(YEAR)	(YEAR % 400 ? ( YEAR % 100 ? ( YEAR % 4 ? 0 : 1 ) : 0 ) : 1)` |
|      - | 7553 | `/* ISO-8601 numeric representation of the day of the week */` |
|      - | 7554 | `static const int aISO8601[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|      - | 7555 | `/*` |
|      - | 7556 | ` * Format a given date string.` |
|      - | 7557 | ` * Supported format: (Taken from PHP online docs)` |
|      - | 7558 | ` * character 	Description` |
|      - | 7559 | ` * d          Day of the month` |
|      - | 7560 | ` * D          A textual representation of a days` |
|      - | 7561 | ` * j          Day of the month without leading zeros` |
|      - | 7562 | ` * l          A full textual representation of the day of the week` |
|      - | 7563 | ` * N          ISO-8601 numeric representation of the day of the week` |
|      - | 7564 | ` * w          Numeric representation of the day of the week` |
|      - | 7565 | ` * z          The day of the year (starting from 0)` |
|      - | 7566 | ` * F          A full textual representation of a month, such as January or March` |
|      - | 7567 | ` * m          Numeric representation of a month, with leading zeros 	01 through 12` |
|      - | 7568 | ` * M          A short textual representation of a month, three letters 	Jan through Dec` |
|      - | 7569 | ` * n          Numeric representation of a month, without leading zeros 	1 through 12` |
|      - | 7570 | ` * t          Number of days in the given month 	28 through 31` |
|      - | 7571 | ` * L          Whether it's a leap year 	1 if it is a leap year, 0 otherwise.` |
|      - | 7572 | ` * o          ISO-8601 year number. This has the same value as Y, except that if the ISO week number` |
|      - | 7573 | ` *            (W) belongs to the previous or next year, that year is used instead. (added in PHP 5.1.0) Examples: 1999 or 2003` |
|      - | 7574 | ` * Y          A full numeric representation of a year, 4 digits 	Examples: 1999 or 2003` |
|      - | 7575 | ` * y          A two digit representation of a year 	Examples: 99 or 03` |
|      - | 7576 | ` * a          Lowercase Ante meridiem and Post meridiem 	am or pm` |
|      - | 7577 | ` * A          Uppercase Ante meridiem and Post meridiem 	AM or PM` |
|      - | 7578 | ` * g          12-hour format of an hour without leading zeros 	1 through 12` |
|      - | 7579 | ` * G          24-hour format of an hour without leading zeros 	0 through 23` |
|      - | 7580 | ` * h          12-hour format of an hour with leading zeros 	01 through 12` |
|      - | 7581 | ` * H          24-hour format of an hour with leading zeros 	00 through 23` |
|      - | 7582 | ` * i          Minutes with leading zeros 	00 to 59` |
|      - | 7583 | ` * s          Seconds, with leading zeros 	00 through 59` |
|      - | 7584 | ` * u          Microseconds Example: 654321` |
|      - | 7585 | ` * e          Timezone identifier 	Examples: UTC, GMT, Atlantic/Azores` |
|      - | 7586 | ` * I          (capital i) Whether or not the date is in daylight saving time 	1 if Daylight Saving Time, 0 otherwise.` |
|      - | 7587 | ` * r          RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 +0200` |
|      - | 7588 | ` * U          Seconds since the Unix Epoch (January 1 1970 00:00:00 GMT)` |
|      - | 7589 | ` * S          English ordinal suffix for the day of the month, 2 characters` |
|      - | 7590 | ` * O          Difference to Greenwich time (GMT) in hours` |
|      - | 7591 | ` * Z          Timezone offset in seconds. The offset for timezones west of UTC is always negative, and for those` |
|      - | 7592 | ` *            east of UTC is always positive.` |
|      - | 7593 | ` * c         ISO 8601 date` |
|      - | 7594 | ` */` |
|     46 | 7595 | `static sxi32 DateFormat(ph7_context *pCtx,const char *zIn,int nLen,Sytm *pTm)` |
|      1 | 7596 |  |
|     47 | 7597 | `	const char *zEnd = &zIn[nLen];` |
|      - | 7598 | `	const char *zCur;` |
|      - | 7599 | `	/* Start the format process */` |
|     78 | 7600 | `	for(;;){` |
|    157 | 7601 | `		if( zIn >= zEnd ){` |
|      - | 7602 | `			/* No more input to process */` |
|     47 | 7603 | `			break;` |
|      - | 7604 | `		}` |
|    111 | 7605 | `		switch(zIn[0]){` |
|      7 | 7606 | `		case 'd':` |
|      - | 7607 | `			/* Day of the month, 2 digits with leading zeros */` |
|     15 | 7608 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mday);` |
|     15 | 7609 | `			break;` |
|    ! 0 | 7610 | `		case 'D':` |
|      - | 7611 | `			/*A textual representation of a day, three letters*/` |
|    ! 0 | 7612 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|    ! 0 | 7613 | `			ph7_result_string(pCtx,zCur,3);` |
|    ! 0 | 7614 | `			break;` |
|    ! 0 | 7615 | `		case 'j':` |
|      - | 7616 | `			/*	Day of the month without leading zeros */` |
|    ! 0 | 7617 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mday);` |
|    ! 0 | 7618 | `			break;` |
|      2 | 7619 | `		case 'l':` |
|      - | 7620 | `			/* A full textual representation of the day of the week */` |
|      5 | 7621 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|      5 | 7622 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|      5 | 7623 | `			break;` |
|    ! 0 | 7624 | `		case 'N':{` |
|      - | 7625 | `			/* ISO-8601 numeric representation of the day of the week */` |
|    ! 0 | 7626 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|    ! 0 | 7627 | `			break;` |
|      - | 7628 | `				 }` |
|    ! 0 | 7629 | `		case 'w':` |
|      - | 7630 | `			/*Numeric representation of the day of the week*/` |
|    ! 0 | 7631 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|    ! 0 | 7632 | `			break;` |
|    ! 0 | 7633 | `		case 'z':` |
|      - | 7634 | `			/*The day of the year*/` |
|    ! 0 | 7635 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_yday);` |
|    ! 0 | 7636 | `			break;` |
|      2 | 7637 | `		case 'F':` |
|      - | 7638 | `			/*A full textual representation of a month, such as January or March*/` |
|      5 | 7639 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|      5 | 7640 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|      5 | 7641 | `			break;` |
|      7 | 7642 | `		case 'm':` |
|      - | 7643 | `			/*Numeric representation of a month, with leading zeros*/` |
|     15 | 7644 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|     15 | 7645 | `			break;` |
|    ! 0 | 7646 | `		case 'M':` |
|      - | 7647 | `			/*A short textual representation of a month, three letters*/` |
|    ! 0 | 7648 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|    ! 0 | 7649 | `			ph7_result_string(pCtx,zCur,3);` |
|    ! 0 | 7650 | `			break;` |
|    ! 0 | 7651 | `		case 'n':` |
|      - | 7652 | `			/*Numeric representation of a month, without leading zeros*/` |
|    ! 0 | 7653 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mon + 1);` |
|    ! 0 | 7654 | `			break;` |
|    ! 0 | 7655 | `		case 't':{` |
|      - | 7656 | `			static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|    ! 0 | 7657 | `			int nDays = aMonDays[pTm->tm_mon % 12 ];` |
|    ! 0 | 7658 | `			if( pTm->tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(pTm->tm_year) ){` |
|    ! 0 | 7659 | `				nDays = 28;` |
|    ! 0 | 7660 | `			}` |
|      - | 7661 | `			/*Number of days in the given month*/` |
|    ! 0 | 7662 | `			ph7_result_string_format(pCtx,"%d",nDays);` |
|    ! 0 | 7663 | `			break;` |
|      - | 7664 | `				 }` |
|    ! 0 | 7665 | `		case 'L':{` |
|    ! 0 | 7666 | `			int isLeap = IS_LEAP_YEAR(pTm->tm_year);` |
|      - | 7667 | `			/* Whether it's a leap year */` |
|    ! 0 | 7668 | `			ph7_result_string_format(pCtx,"%d",isLeap);` |
|    ! 0 | 7669 | `			break;` |
|      - | 7670 | `				 }` |
|    ! 0 | 7671 | `		case 'o':` |
|      - | 7672 | `			/* ISO-8601 year number.*/` |
|    ! 0 | 7673 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|    ! 0 | 7674 | `			break;` |
|      9 | 7675 | `		case 'Y':` |
|      - | 7676 | `			/*	A full numeric representation of a year, 4 digits */` |
|     19 | 7677 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|     19 | 7678 | `			break;` |
|    ! 0 | 7679 | `		case 'y':` |
|      - | 7680 | `			/*A two digit representation of a year*/` |
|    ! 0 | 7681 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_year%100);` |
|    ! 0 | 7682 | `			break;` |
|    ! 0 | 7683 | `		case 'a':` |
|      - | 7684 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7685 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",2);` |
|    ! 0 | 7686 | `			break;` |
|    ! 0 | 7687 | `		case 'A':` |
|      - | 7688 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7689 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",2);` |
|    ! 0 | 7690 | `			break;` |
|    ! 0 | 7691 | `		case 'g':` |
|      - | 7692 | `			/*	12-hour format of an hour without leading zeros*/` |
|    ! 0 | 7693 | `			ph7_result_string_format(pCtx,"%d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7694 | `			break;` |
|    ! 0 | 7695 | `		case 'G':` |
|      - | 7696 | `			/* 24-hour format of an hour without leading zeros */` |
|    ! 0 | 7697 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_hour);` |
|    ! 0 | 7698 | `			break;` |
|    ! 0 | 7699 | `		case 'h':` |
|      - | 7700 | `			/* 12-hour format of an hour with leading zeros */` |
|    ! 0 | 7701 | `			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7702 | `			break;` |
|      3 | 7703 | `		case 'H':` |
|      - | 7704 | `			/*	24-hour format of an hour with leading zeros */` |
|      7 | 7705 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|      7 | 7706 | `			break;` |
|      3 | 7707 | `		case 'i':` |
|      - | 7708 | `			/* 	Minutes with leading zeros */` |
|      7 | 7709 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|      7 | 7710 | `			break;` |
|      3 | 7711 | `		case 's':` |
|      - | 7712 | `			/* 	second with leading zeros */` |
|      7 | 7713 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|      7 | 7714 | `			break;` |
|    ! 0 | 7715 | `		case 'u':` |
|      - | 7716 | `			/* 	Microseconds */` |
|    ! 0 | 7717 | `			ph7_result_string_format(pCtx,"%u",pTm->tm_sec * SX_USEC_PER_SEC);` |
|    ! 0 | 7718 | `			break;` |
|    ! 0 | 7719 | `		case 'S':{` |
|      - | 7720 | `			/* English ordinal suffix for the day of the month, 2 characters */` |
|      - | 7721 | `			static const char zSuffix[] = "thstndrdthththththth";` |
|    ! 0 | 7722 | `			int v = pTm->tm_mday;` |
|    ! 0 | 7723 | `			ph7_result_string(pCtx,&zSuffix[2 * (int)(v / 10 % 10 != 1 ? v % 10 : 0)],(int)sizeof(char) * 2);` |
|    ! 0 | 7724 | `			break;` |
|      - | 7725 | `				 }` |
|    ! 0 | 7726 | `		case 'e':` |
|      - | 7727 | `			/* 	Timezone identifier */` |
|    ! 0 | 7728 | `			zCur = pTm->tm_zone;` |
|    ! 0 | 7729 | `			if( zCur == 0 ){` |
|      - | 7730 | `				/* Assume GMT */` |
|    ! 0 | 7731 | `				zCur = "GMT";` |
|    ! 0 | 7732 | `			}` |
|    ! 0 | 7733 | `			ph7_result_string(pCtx,zCur,-1);` |
|    ! 0 | 7734 | `			break;` |
|    ! 0 | 7735 | `		case 'I':` |
|      - | 7736 | `			/* Whether or not the date is in daylight saving time */` |
|      - | 7737 | `#ifdef __WINNT__` |
|      - | 7738 | `#ifdef _MSC_VER` |
|      - | 7739 | `#ifndef _WIN32_WCE` |
|    ! 0 | 7740 | `			_get_daylight(&pTm->tm_isdst);` |
|      - | 7741 | `#endif` |
|      - | 7742 | `#endif` |
|      - | 7743 | `#endif` |
|    ! 0 | 7744 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_isdst == 1);` |
|    ! 0 | 7745 | `			break;` |
|    ! 0 | 7746 | `		case 'r':` |
|      - | 7747 | `			/* RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 */` |
|    ! 0 | 7748 | `			ph7_result_string_format(pCtx,"%.3s, %02d %.3s %4d %02d:%02d:%02d",` |
|    ! 0 | 7749 | `				SyTimeGetDay(pTm->tm_wday),` |
|    ! 0 | 7750 | `				pTm->tm_mday,` |
|    ! 0 | 7751 | `				SyTimeGetMonth(pTm->tm_mon),` |
|    ! 0 | 7752 | `				pTm->tm_year,` |
|    ! 0 | 7753 | `				pTm->tm_hour,` |
|    ! 0 | 7754 | `				pTm->tm_min,` |
|    ! 0 | 7755 | `				pTm->tm_sec` |
|      - | 7756 | `				);` |
|    ! 0 | 7757 | `			break;` |
|    ! 0 | 7758 | `		case 'U':{` |
|      - | 7759 | `			time_t tt;` |
|      - | 7760 | `			/* Seconds since the Unix Epoch */` |
|    ! 0 | 7761 | `			time(&tt);` |
|    ! 0 | 7762 | `			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);` |
|    ! 0 | 7763 | `			break;` |
|      - | 7764 | `				 }` |
|    ! 0 | 7765 | `		case 'O':` |
|      - | 7766 | `		case 'P':` |
|      - | 7767 | `			/* Difference to Greenwich time (GMT) in hours */` |
|    ! 0 | 7768 | `			ph7_result_string_format(pCtx,"%+05d",pTm->tm_gmtoff);` |
|    ! 0 | 7769 | `			break;` |
|    ! 0 | 7770 | `		case 'Z':` |
|      - | 7771 | `			/* Timezone offset in seconds. The offset for timezones west of UTC` |
|      - | 7772 | `			 * is always negative, and for those east of UTC is always positive.` |
|      - | 7773 | `			 */` |
|    ! 0 | 7774 | `			ph7_result_string_format(pCtx,"%+05d",pTm->tm_gmtoff);` |
|    ! 0 | 7775 | `			break;` |
|      1 | 7776 | `		case 'c':` |
|      - | 7777 | `			/* 	ISO 8601 date */` |
|      4 | 7778 | `			ph7_result_string_format(pCtx,"%4d-%02d-%02dT%02d:%02d:%02d%+05d",` |
|      1 | 7779 | `				pTm->tm_year,` |
|      2 | 7780 | `				pTm->tm_mon+1,` |
|      1 | 7781 | `				pTm->tm_mday,` |
|      1 | 7782 | `				pTm->tm_hour,` |
|      1 | 7783 | `				pTm->tm_min,` |
|      1 | 7784 | `				pTm->tm_sec,` |
|      1 | 7785 | `				pTm->tm_gmtoff` |
|      - | 7786 | `				);` |
|      3 | 7787 | `			break;` |
|      1 | 7788 | `		case '\\':` |
|      3 | 7789 | `			zIn++;` |
|      - | 7790 | `			/* Expand verbatim */` |
|      3 | 7791 | `			if( zIn < zEnd ){` |
|      3 | 7792 | `				ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|      1 | 7793 | `			}` |
|      3 | 7794 | `			break;` |
|     17 | 7795 | `		default:` |
|      - | 7796 | `			/* Unknown format specifer,expand verbatim */` |
|     35 | 7797 | `			ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|     34 | 7798 | `			break;` |
|      - | 7799 | `		}` |
|      - | 7800 | `		/* Point to the next character */` |
|    111 | 7801 | `		zIn++;` |
|      1 | 7802 | `	}` |
|     47 | 7803 | `	return SXRET_OK;` |
|      1 | 7804 |  |
|      - | 7805 | `/*` |
|      - | 7806 | ` * PH7 implementation of the strftime() function.` |
|      - | 7807 | ` * The following formats are supported:` |
|      - | 7808 | ` * %a 	An abbreviated textual representation of the day` |
|      - | 7809 | ` * %A 	A full textual representation of the day` |
|      - | 7810 | ` * %d 	Two-digit day of the month (with leading zeros)` |
|      - | 7811 | ` * %e 	Day of the month, with a space preceding single digits.` |
|      - | 7812 | ` * %j 	Day of the year, 3 digits with leading zeros` |
|      - | 7813 | ` * %u 	ISO-8601 numeric representation of the day of the week 	1 (for Monday) though 7 (for Sunday)` |
|      - | 7814 | ` * %w 	Numeric representation of the day of the week 0 (for Sunday) through 6 (for Saturday)` |
|      - | 7815 | ` * %U 	Week number of the given year, starting with the first Sunday as the first week` |
|      - | 7816 | ` * %V 	ISO-8601:1988 week number of the given year, starting with the first week of the year with at least` |
|      - | 7817 | ` *   4 weekdays, with Monday being the start of the week.` |
|      - | 7818 | ` * %W 	A numeric representation of the week of the year` |
|      - | 7819 | ` * %b 	Abbreviated month name, based on the locale` |
|      - | 7820 | ` * %B 	Full month name, based on the locale` |
|      - | 7821 | ` * %h 	Abbreviated month name, based on the locale (an alias of %b)` |
|      - | 7822 | ` * %m 	Two digit representation of the month` |
|      - | 7823 | ` * %C 	Two digit representation of the century (year divided by 100, truncated to an integer)` |
|      - | 7824 | ` * %g 	Two digit representation of the year going by ISO-8601:1988 standards (see %V)` |
|      - | 7825 | ` * %G 	The full four-digit version of %g` |
|      - | 7826 | ` * %y 	Two digit representation of the year` |
|      - | 7827 | ` * %Y 	Four digit representation for the year` |
|      - | 7828 | ` * %H 	Two digit representation of the hour in 24-hour format` |
|      - | 7829 | ` * %I 	Two digit representation of the hour in 12-hour format` |
|      - | 7830 | ` * %l (lower-case 'L') 	Hour in 12-hour format, with a space preceeding single digits` |
|      - | 7831 | ` * %M 	Two digit representation of the minute` |
|      - | 7832 | ` * %p 	UPPER-CASE 'AM' or 'PM' based on the given time` |
|      - | 7833 | ` * %P 	lower-case 'am' or 'pm' based on the given time` |
|      - | 7834 | ` * %r 	Same as "%I:%M:%S %p"` |
|      - | 7835 | ` * %R 	Same as "%H:%M"` |
|      - | 7836 | ` * %S 	Two digit representation of the second` |
|      - | 7837 | ` * %T 	Same as "%H:%M:%S"` |
|      - | 7838 | ` * %X 	Preferred time representation based on locale, without the date` |
|      - | 7839 | ` * %z 	Either the time zone offset from UTC or the abbreviation` |
|      - | 7840 | ` * %Z 	The time zone offset/abbreviation option NOT given by %z` |
|      - | 7841 | ` * %c 	Preferred date and time stamp based on local` |
|      - | 7842 | ` * %D 	Same as "%m/%d/%y"` |
|      - | 7843 | ` * %F 	Same as "%Y-%m-%d"` |
|      - | 7844 | ` * %s 	Unix Epoch Time timestamp (same as the time() function)` |
|      - | 7845 | ` * %x 	Preferred date representation based on locale, without the time` |
|      - | 7846 | ` * %n 	A newline character ("\n")` |
|      - | 7847 | ` * %t 	A Tab character ("\t")` |
|      - | 7848 | ` * %% 	A literal percentage character ("%")` |
|      - | 7849 | ` */` |
|     16 | 7850 | `static int PH7_Strftime(` |
|      - | 7851 | `	ph7_context *pCtx,  /* Call context */` |
|      - | 7852 | `	const char *zIn,    /* Input string */` |
|      - | 7853 | `	int nLen,           /* Input length */` |
|      - | 7854 | `	Sytm *pTm           /* Parse of the given time */` |
|      - | 7855 | `	)` |
|      1 | 7856 |  |
|     17 | 7857 | `	const char *zCur,*zEnd = &zIn[nLen];` |
|      - | 7858 | `	int c;` |
|      - | 7859 | `	/* Start the format process */` |
|     18 | 7860 | `	for(;;){` |
|     37 | 7861 | `		zCur = zIn;` |
|     41 | 7862 | `		while(zIn < zEnd && zIn[0] != '%' ){` |
|      5 | 7863 | `			zIn++;` |
|      1 | 7864 | `		}` |
|     37 | 7865 | `		if( zIn > zCur ){` |
|      - | 7866 | `			/* Consume input verbatim */` |
|      5 | 7867 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 7868 | `		}` |
|     37 | 7869 | `		zIn++; /* Jump the percent sign */` |
|     37 | 7870 | `		if( zIn >= zEnd ){` |
|      - | 7871 | `			/* No more input to process */` |
|     17 | 7872 | `			break;` |
|      - | 7873 | `		}` |
|     21 | 7874 | `		c = zIn[0];` |
|      - | 7875 | `		/* Act according to the current specifer */` |
|     21 | 7876 | `		switch(c){` |
|    ! 0 | 7877 | `		case '%':` |
|      - | 7878 | `			/* A literal percentage character ("%") */` |
|    ! 0 | 7879 | `			ph7_result_string(pCtx,"%",(int)sizeof(char));` |
|    ! 0 | 7880 | `			break;` |
|    ! 0 | 7881 | `		case 't':` |
|      - | 7882 | `			/* A Tab character */` |
|    ! 0 | 7883 | `			ph7_result_string(pCtx,"\t",(int)sizeof(char));` |
|    ! 0 | 7884 | `			break;` |
|    ! 0 | 7885 | `		case 'n':` |
|      - | 7886 | `			/* A newline character */` |
|    ! 0 | 7887 | `			ph7_result_string(pCtx,"\n",(int)sizeof(char));` |
|    ! 0 | 7888 | `			break;` |
|      1 | 7889 | `		case 'a':` |
|      - | 7890 | `			/* An abbreviated textual representation of the day */` |
|      3 | 7891 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),(int)sizeof(char)*3);` |
|      3 | 7892 | `			break;` |
|    ! 0 | 7893 | `		case 'A':` |
|      - | 7894 | `			/* A full textual representation of the day */` |
|    ! 0 | 7895 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),-1/*Compute length automatically*/);` |
|    ! 0 | 7896 | `			break;` |
|    ! 0 | 7897 | `		case 'e':` |
|      - | 7898 | `			/* Day of the month, 2 digits with leading space for single digit*/` |
|    ! 0 | 7899 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_mday);` |
|    ! 0 | 7900 | `			break;` |
|      2 | 7901 | `		case 'd':` |
|      - | 7902 | `			/* Two-digit day of the month (with leading zeros) */` |
|      5 | 7903 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon+1);` |
|      5 | 7904 | `			break;` |
|    ! 0 | 7905 | `		case 'j':` |
|      - | 7906 | `			/*The day of the year,3 digits with leading zeros*/` |
|    ! 0 | 7907 | `			ph7_result_string_format(pCtx,"%03d",pTm->tm_yday);` |
|    ! 0 | 7908 | `			break;` |
|    ! 0 | 7909 | `		case 'u':` |
|      - | 7910 | `			/* ISO-8601 numeric representation of the day of the week */` |
|    ! 0 | 7911 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|    ! 0 | 7912 | `			break;` |
|    ! 0 | 7913 | `		case 'w':` |
|      - | 7914 | `			/* Numeric representation of the day of the week */` |
|    ! 0 | 7915 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|    ! 0 | 7916 | `			break;` |
|    ! 0 | 7917 | `		case 'b':` |
|      - | 7918 | `		case 'h':` |
|      - | 7919 | `			/*A short textual representation of a month, three letters (Not based on locale)*/` |
|    ! 0 | 7920 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),(int)sizeof(char)*3);` |
|    ! 0 | 7921 | `			break;` |
|    ! 0 | 7922 | `		case 'B':` |
|      - | 7923 | `			/* Full month name (Not based on locale) */` |
|    ! 0 | 7924 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),-1/*Compute length automatically*/);` |
|    ! 0 | 7925 | `			break;` |
|      2 | 7926 | `		case 'm':` |
|      - | 7927 | `			/*Numeric representation of a month, with leading zeros*/` |
|      5 | 7928 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|      5 | 7929 | `			break;` |
|    ! 0 | 7930 | `		case 'C':` |
|      - | 7931 | `			/* Two digit representation of the century */` |
|    ! 0 | 7932 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year/100);` |
|    ! 0 | 7933 | `			break;` |
|    ! 0 | 7934 | `		case 'y':` |
|      - | 7935 | `		case 'g':` |
|      - | 7936 | `			/* Two digit representation of the year */` |
|    ! 0 | 7937 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year%100);` |
|    ! 0 | 7938 | `			break;` |
|      2 | 7939 | `		case 'Y':` |
|      - | 7940 | `		case 'G':` |
|      - | 7941 | `			/* Four digit representation of the year */` |
|      5 | 7942 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|      5 | 7943 | `			break;` |
|    ! 0 | 7944 | `		case 'I':` |
|      - | 7945 | `			/* 12-hour format of an hour with leading zeros */` |
|    ! 0 | 7946 | `			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7947 | `			break;` |
|    ! 0 | 7948 | `		case 'l':` |
|      - | 7949 | `			/* 12-hour format of an hour with leading space */` |
|    ! 0 | 7950 | `			ph7_result_string_format(pCtx,"%2d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7951 | `			break;` |
|      1 | 7952 | `		case 'H':` |
|      - | 7953 | `			/* 24-hour format of an hour with leading zeros */` |
|      3 | 7954 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|      3 | 7955 | `			break;` |
|      1 | 7956 | `		case 'M':` |
|      - | 7957 | `			/* Minutes with leading zeros */` |
|      3 | 7958 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|      3 | 7959 | `			break;` |
|    ! 0 | 7960 | `		case 'S':` |
|      - | 7961 | `			/* Seconds with leading zeros */` |
|    ! 0 | 7962 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|    ! 0 | 7963 | `			break;` |
|    ! 0 | 7964 | `		case 'z':` |
|      - | 7965 | `		case 'Z':` |
|      - | 7966 | `			/* 	Timezone identifier */` |
|    ! 0 | 7967 | `			zCur = pTm->tm_zone;` |
|    ! 0 | 7968 | `			if( zCur == 0 ){` |
|      - | 7969 | `				/* Assume GMT */` |
|    ! 0 | 7970 | `				zCur = "GMT";` |
|    ! 0 | 7971 | `			}` |
|    ! 0 | 7972 | `			ph7_result_string(pCtx,zCur,-1);` |
|    ! 0 | 7973 | `			break;` |
|    ! 0 | 7974 | `		case 'T':` |
|      - | 7975 | `		case 'X':` |
|      - | 7976 | `			/* Same as "%H:%M:%S" */` |
|    ! 0 | 7977 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d",pTm->tm_hour,pTm->tm_min,pTm->tm_sec);` |
|    ! 0 | 7978 | `			break;` |
|    ! 0 | 7979 | `		case 'R':` |
|      - | 7980 | `			/* Same as "%H:%M" */` |
|    ! 0 | 7981 | `			ph7_result_string_format(pCtx,"%02d:%02d",pTm->tm_hour,pTm->tm_min);` |
|    ! 0 | 7982 | `			break;` |
|    ! 0 | 7983 | `		case 'P':` |
|      - | 7984 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7985 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",(int)sizeof(char)*2);` |
|    ! 0 | 7986 | `			break;` |
|    ! 0 | 7987 | `		case 'p':` |
|      - | 7988 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7989 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",(int)sizeof(char)*2);` |
|    ! 0 | 7990 | `			break;` |
|    ! 0 | 7991 | `		case 'r':` |
|      - | 7992 | `			/* Same as "%I:%M:%S %p" */` |
|    ! 0 | 7993 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d %s",` |
|    ! 0 | 7994 | `				1+(pTm->tm_hour%12),` |
|    ! 0 | 7995 | `				pTm->tm_min,` |
|    ! 0 | 7996 | `				pTm->tm_sec,` |
|    ! 0 | 7997 | `				pTm->tm_hour > 12 ? "PM" : "AM"` |
|      - | 7998 | `				);` |
|    ! 0 | 7999 | `			break;` |
|      1 | 8000 | `		case 'D':` |
|      - | 8001 | `		case 'x':` |
|      - | 8002 | `			/* Same as "%m/%d/%y" */` |
|      4 | 8003 | `			ph7_result_string_format(pCtx,"%02d/%02d/%02d",` |
|      2 | 8004 | `				pTm->tm_mon+1,` |
|      1 | 8005 | `				pTm->tm_mday,` |
|      2 | 8006 | `				pTm->tm_year%100` |
|      - | 8007 | `				);` |
|      3 | 8008 | `			break;` |
|    ! 0 | 8009 | `		case 'F':` |
|      - | 8010 | `			/* Same as "%Y-%m-%d" */` |
|    ! 0 | 8011 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d",` |
|    ! 0 | 8012 | `				pTm->tm_year,` |
|    ! 0 | 8013 | `				pTm->tm_mon+1,` |
|    ! 0 | 8014 | `				pTm->tm_mday` |
|      - | 8015 | `				);` |
|    ! 0 | 8016 | `			break;` |
|    ! 0 | 8017 | `		case 'c':` |
|    ! 0 | 8018 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d %02d:%02d:%02d",` |
|    ! 0 | 8019 | `				pTm->tm_year,` |
|    ! 0 | 8020 | `				pTm->tm_mon+1,` |
|    ! 0 | 8021 | `				pTm->tm_mday,` |
|    ! 0 | 8022 | `				pTm->tm_hour,` |
|    ! 0 | 8023 | `				pTm->tm_min,` |
|    ! 0 | 8024 | `				pTm->tm_sec` |
|      - | 8025 | `				);` |
|    ! 0 | 8026 | `			break;` |
|    ! 0 | 8027 | `		case 's':{` |
|      - | 8028 | `			time_t tt;` |
|      - | 8029 | `			/* Seconds since the Unix Epoch */` |
|    ! 0 | 8030 | `			time(&tt);` |
|    ! 0 | 8031 | `			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);` |
|    ! 0 | 8032 | `			break;` |
|      - | 8033 | `				 }` |
|    ! 0 | 8034 | `		default:` |
|      - | 8035 | `			/* unknown specifer,simply ignore*/` |
|    ! 0 | 8036 | `			break;` |
|      - | 8037 | `		}` |
|      - | 8038 | `		/* Advance the cursor */` |
|     21 | 8039 | `		zIn++;` |
|      1 | 8040 | `	}` |
|     17 | 8041 | `	return SXRET_OK;` |
|      1 | 8042 |  |
|      - | 8043 | `/*` |
|      - | 8044 | ` * string date(string $format [, int $timestamp = time() ] )` |
|      - | 8045 | ` *  Returns a string formatted according to the given format string using` |
|      - | 8046 | ` *  the given integer timestamp or the current time if no timestamp is given.` |
|      - | 8047 | ` *  In other words, timestamp is optional and defaults to the value of time().` |
|      - | 8048 | ` * Parameters` |
|      - | 8049 | ` *  $format` |
|      - | 8050 | ` *   The format of the outputted date string (See code above)` |
|      - | 8051 | ` * $timestamp` |
|      - | 8052 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|      - | 8053 | ` *   that defaults to the current local time if a timestamp is not given.` |
|      - | 8054 | ` *   In other words, it defaults to the value of time().` |
|      - | 8055 | ` * Return` |
|      - | 8056 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|      - | 8057 | ` */` |
|     36 | 8058 | `static int PH7_builtin_date(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8059 |  |
|      - | 8060 | `	const char *zFormat;` |
|      - | 8061 | `	int nLen;` |
|      - | 8062 | `	Sytm sTm;` |
|     37 | 8063 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8064 | `		/* Missing/Invalid argument,return FALSE */` |
|      5 | 8065 | `		ph7_result_bool(pCtx,0);` |
|      5 | 8066 | `		return PH7_OK;` |
|      - | 8067 | `	}` |
|     33 | 8068 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     33 | 8069 | `	if( nLen < 1 ){` |
|      - | 8070 | `		/* Don't bother processing return the empty string */` |
|    ! 0 | 8071 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 8072 | `	}` |
|     33 | 8073 | `	if( nArg < 2 ){` |
|      - | 8074 | `#ifdef __WINNT__` |
|      - | 8075 | `		SYSTEMTIME sOS;` |
|      1 | 8076 | `		GetSystemTime(&sOS);` |
|      1 | 8077 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8078 | `#else` |
|      - | 8079 | `		struct tm *pTm;` |
|      - | 8080 | `		time_t t;` |
|     30 | 8081 | `		time(&t);` |
|     30 | 8082 | `		pTm = localtime(&t);` |
|     30 | 8083 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8084 | `#endif` |
|     16 | 8085 | `	}else{` |
|      - | 8086 | `		/* Use the given timestamp */` |
|      - | 8087 | `		time_t t;` |
|      - | 8088 | `		struct tm *pTm;` |
|      3 | 8089 | `		if( ph7_value_is_int(apArg[1]) ){` |
|      3 | 8090 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|      3 | 8091 | `			pTm = localtime(&t);` |
|      3 | 8092 | `			if( pTm == 0 ){` |
|    ! 0 | 8093 | `				time(&t);` |
|    ! 0 | 8094 | `			}` |
|      2 | 8095 | `		}else{` |
|    ! 0 | 8096 | `			time(&t);` |
|      - | 8097 | `		}` |
|      3 | 8098 | `		pTm = localtime(&t);` |
|      3 | 8099 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8100 | `	}` |
|      - | 8101 | `	/* Format the given string */` |
|     33 | 8102 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|     33 | 8103 | `	return PH7_OK;` |
|     19 | 8104 |  |
|      - | 8105 | `/*` |
|      - | 8106 | ` * string strftime(string $format [, int $timestamp = time() ] )` |
|      - | 8107 | ` *  Format a local time/date (PLATFORM INDEPENDANT IMPLEENTATION NOT BASED ON LOCALE)` |
|      - | 8108 | ` * Parameters` |
|      - | 8109 | ` *  $format` |
|      - | 8110 | ` *   The format of the outputted date string (See code above)` |
|      - | 8111 | ` * $timestamp` |
|      - | 8112 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|      - | 8113 | ` *   that defaults to the current local time if a timestamp is not given.` |
|      - | 8114 | ` *   In other words, it defaults to the value of time().` |
|      - | 8115 | ` * Return` |
|      - | 8116 | ` * Returns a string formatted according format using the given timestamp` |
|      - | 8117 | ` * or the current local time if no timestamp is given.` |
|      - | 8118 | ` */` |
|     20 | 8119 | `static int PH7_builtin_strftime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8120 |  |
|      - | 8121 | `	const char *zFormat;` |
|      - | 8122 | `	int nLen;` |
|      - | 8123 | `	Sytm sTm;` |
|     21 | 8124 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8125 | `		/* Missing/Invalid argument,return FALSE */` |
|      5 | 8126 | `		ph7_result_bool(pCtx,0);` |
|      5 | 8127 | `		return PH7_OK;` |
|      - | 8128 | `	}` |
|     17 | 8129 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8130 | `	if( nLen < 1 ){` |
|      - | 8131 | `		/* Don't bother processing return FALSE */` |
|    ! 0 | 8132 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8133 | `	}` |
|     17 | 8134 | `	if( nArg < 2 ){` |
|      - | 8135 | `#ifdef __WINNT__` |
|      - | 8136 | `		SYSTEMTIME sOS;` |
|      1 | 8137 | `		GetSystemTime(&sOS);` |
|      1 | 8138 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8139 | `#else` |
|      - | 8140 | `		struct tm *pTm;` |
|      - | 8141 | `		time_t t;` |
|     14 | 8142 | `		time(&t);` |
|     14 | 8143 | `		pTm = localtime(&t);` |
|     14 | 8144 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8145 | `#endif` |
|      8 | 8146 | `	}else{` |
|      - | 8147 | `		/* Use the given timestamp */` |
|      - | 8148 | `		time_t t;` |
|      - | 8149 | `		struct tm *pTm;` |
|      3 | 8150 | `		if( ph7_value_is_int(apArg[1]) ){` |
|      3 | 8151 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|      3 | 8152 | `			pTm = localtime(&t);` |
|      3 | 8153 | `			if( pTm == 0 ){` |
|    ! 0 | 8154 | `				time(&t);` |
|    ! 0 | 8155 | `			}` |
|      2 | 8156 | `		}else{` |
|    ! 0 | 8157 | `			time(&t);` |
|      - | 8158 | `		}` |
|      3 | 8159 | `		pTm = localtime(&t);` |
|      3 | 8160 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8161 | `	}` |
|      - | 8162 | `	/* Format the given string */` |
|     17 | 8163 | `	PH7_Strftime(pCtx,zFormat,nLen,&sTm);` |
|     17 | 8164 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|      - | 8165 | `		/* Nothing was formatted,return FALSE */` |
|    ! 0 | 8166 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8167 | `	}` |
|     17 | 8168 | `	return PH7_OK;` |
|     11 | 8169 |  |
|      - | 8170 | `/*` |
|      - | 8171 | ` * string gmdate(string $format [, int $timestamp = time() ] )` |
|      - | 8172 | ` *  Identical to the date() function except that the time returned` |
|      - | 8173 | ` *  is Greenwich Mean Time (GMT).` |
|      - | 8174 | ` * Parameters` |
|      - | 8175 | ` *  $format` |
|      - | 8176 | ` *  The format of the outputted date string (See code above)` |
|      - | 8177 | ` *  $timestamp` |
|      - | 8178 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|      - | 8179 | ` *   that defaults to the current local time if a timestamp is not given.` |
|      - | 8180 | ` *   In other words, it defaults to the value of time().` |
|      - | 8181 | ` * Return` |
|      - | 8182 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|      - | 8183 | ` */` |
|     16 | 8184 | `static int PH7_builtin_gmdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8185 |  |
|      - | 8186 | `	const char *zFormat;` |
|      - | 8187 | `	int nLen;` |
|      - | 8188 | `	Sytm sTm;` |
|     17 | 8189 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8190 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 | 8191 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8192 | `		return PH7_OK;` |
|      - | 8193 | `	}` |
|     15 | 8194 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 8195 | `	if( nLen < 1 ){` |
|      - | 8196 | `		/* Don't bother processing return the empty string */` |
|    ! 0 | 8197 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 8198 | `	}` |
|     15 | 8199 | `	if( nArg < 2 ){` |
|      - | 8200 | `#ifdef __WINNT__` |
|      - | 8201 | `		SYSTEMTIME sOS;` |
|      1 | 8202 | `		GetSystemTime(&sOS);` |
|      1 | 8203 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8204 | `#else` |
|      - | 8205 | `		struct tm *pTm;` |
|      - | 8206 | `		time_t t;` |
|     12 | 8207 | `		time(&t);` |
|     12 | 8208 | `		pTm = gmtime(&t);` |
|     12 | 8209 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8210 | `#endif` |
|      7 | 8211 | `	}else{` |
|      - | 8212 | `		/* Use the given timestamp */` |
|      - | 8213 | `		time_t t;` |
|      - | 8214 | `		struct tm *pTm;` |
|      3 | 8215 | `		if( ph7_value_is_int(apArg[1]) ){` |
|      3 | 8216 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|      3 | 8217 | `			pTm = gmtime(&t);` |
|      3 | 8218 | `			if( pTm == 0 ){` |
|    ! 0 | 8219 | `				time(&t);` |
|    ! 0 | 8220 | `			}` |
|      2 | 8221 | `		}else{` |
|    ! 0 | 8222 | `			time(&t);` |
|      - | 8223 | `		}` |
|      3 | 8224 | `		pTm = gmtime(&t);` |
|      3 | 8225 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8226 | `	}` |
|      - | 8227 | `	/* Format the given string */` |
|     15 | 8228 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|     15 | 8229 | `	return PH7_OK;` |
|      9 | 8230 |  |
|      - | 8231 | `/*` |
|      - | 8232 | ` * array localtime([ int $timestamp = time() [, bool $is_associative = false ]])` |
|      - | 8233 | ` *  Return the local time.` |
|      - | 8234 | ` * Parameter` |
|      - | 8235 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|      - | 8236 | ` *     that defaults to the current local time if a timestamp is not given.` |
|      - | 8237 | ` *     In other words, it defaults to the value of time().` |
|      - | 8238 | ` * $is_associative` |
|      - | 8239 | ` *   If set to FALSE or not supplied then the array is returned as a regular, numerically` |
|      - | 8240 | ` *   indexed array. If the argument is set to TRUE then localtime() returns an associative` |
|      - | 8241 | ` *   array containing all the different elements of the structure returned by the C function` |
|      - | 8242 | ` *   call to localtime. The names of the different keys of the associative array are as follows:` |
|      - | 8243 | ` *      "tm_sec" - seconds, 0 to 59` |
|      - | 8244 | ` *      "tm_min" - minutes, 0 to 59` |
|      - | 8245 | ` *      "tm_hour" - hours, 0 to 23` |
|      - | 8246 | ` *      "tm_mday" - day of the month, 1 to 31` |
|      - | 8247 | ` *      "tm_mon" - month of the year, 0 (Jan) to 11 (Dec)` |
|      - | 8248 | ` *      "tm_year" - years since 1900` |
|      - | 8249 | ` *      "tm_wday" - day of the week, 0 (Sun) to 6 (Sat)` |
|      - | 8250 | ` *      "tm_yday" - day of the year, 0 to 365` |
|      - | 8251 | ` *      "tm_isdst" - is daylight savings time in effect? Positive if yes, 0 if not, negative if unknown.` |
|      - | 8252 | ` * Returns` |
|      - | 8253 | ` *  An associative array of information related to the timestamp.` |
|      - | 8254 | ` */` |
|      8 | 8255 | `static int PH7_builtin_localtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8256 |  |
|      - | 8257 | `	ph7_value *pValue,*pArray;` |
|      9 | 8258 | `	int isAssoc = 0;` |
|      - | 8259 | `	Sytm sTm;` |
|      9 | 8260 | `	if( nArg < 1 ){` |
|      - | 8261 | `#ifdef __WINNT__` |
|      - | 8262 | `		SYSTEMTIME sOS;` |
|      1 | 8263 | `		GetSystemTime(&sOS); /* TODO(chems): GMT not local */` |
|      1 | 8264 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8265 | `#else` |
|      - | 8266 | `		struct tm *pTm;` |
|      - | 8267 | `		time_t t;` |
|      4 | 8268 | `		time(&t);` |
|      4 | 8269 | `		pTm = localtime(&t);` |
|      4 | 8270 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8271 | `#endif` |
|      3 | 8272 | `	}else{` |
|      - | 8273 | `		/* Use the given timestamp */` |
|      - | 8274 | `		time_t t;` |
|      - | 8275 | `		struct tm *pTm;` |
|      5 | 8276 | `		if( ph7_value_is_int(apArg[0]) ){` |
|      5 | 8277 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|      5 | 8278 | `			pTm = localtime(&t);` |
|      5 | 8279 | `			if( pTm == 0 ){` |
|    ! 0 | 8280 | `				time(&t);` |
|    ! 0 | 8281 | `			}` |
|      3 | 8282 | `		}else{` |
|    ! 0 | 8283 | `			time(&t);` |
|      - | 8284 | `		}` |
|      5 | 8285 | `		pTm = localtime(&t);` |
|      5 | 8286 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8287 | `	}` |
|      - | 8288 | `	/* Element value */` |
|      9 | 8289 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      9 | 8290 | `	if( pValue == 0 ){` |
|      - | 8291 | `		/* Return NULL */` |
|    ! 0 | 8292 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8293 | `		return PH7_OK;` |
|      - | 8294 | `	}` |
|      - | 8295 | `	/* Create a new array */` |
|      9 | 8296 | `	pArray = ph7_context_new_array(pCtx);` |
|      9 | 8297 | `	if( pArray == 0 ){` |
|      - | 8298 | `		/* Return NULL */` |
|    ! 0 | 8299 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8300 | `		return PH7_OK;` |
|      - | 8301 | `	}` |
|      9 | 8302 | `	if( nArg > 1 ){` |
|      3 | 8303 | `		isAssoc = ph7_value_to_bool(apArg[1]);` |
|      1 | 8304 | `	}` |
|      - | 8305 | `	/* Fill the array */` |
|      - | 8306 | `	/* Seconds */` |
|      9 | 8307 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|      9 | 8308 | `	if( isAssoc ){` |
|      3 | 8309 | `		ph7_array_add_strkey_elem(pArray,"tm_sec",pValue);` |
|      2 | 8310 | `	}else{` |
|      7 | 8311 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8312 | `	}` |
|      - | 8313 | `	/* Minutes */` |
|      9 | 8314 | `	ph7_value_int(pValue,sTm.tm_min);` |
|      9 | 8315 | `	if( isAssoc ){` |
|      3 | 8316 | `		ph7_array_add_strkey_elem(pArray,"tm_min",pValue);` |
|      2 | 8317 | `	}else{` |
|      7 | 8318 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8319 | `	}` |
|      - | 8320 | `	/* Hours */` |
|      9 | 8321 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|      9 | 8322 | `	if( isAssoc ){` |
|      3 | 8323 | `		ph7_array_add_strkey_elem(pArray,"tm_hour",pValue);` |
|      2 | 8324 | `	}else{` |
|      7 | 8325 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8326 | `	}` |
|      - | 8327 | `	/* mday */` |
|      9 | 8328 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|      9 | 8329 | `	if( isAssoc ){` |
|      3 | 8330 | `		ph7_array_add_strkey_elem(pArray,"tm_mday",pValue);` |
|      2 | 8331 | `	}else{` |
|      7 | 8332 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8333 | `	}` |
|      - | 8334 | `	/* mon */` |
|      9 | 8335 | `	ph7_value_int(pValue,sTm.tm_mon);` |
|      9 | 8336 | `	if( isAssoc ){` |
|      3 | 8337 | `		ph7_array_add_strkey_elem(pArray,"tm_mon",pValue);` |
|      2 | 8338 | `	}else{` |
|      7 | 8339 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8340 | `	}` |
|      - | 8341 | `	/* year since 1900 */` |
|      9 | 8342 | `	ph7_value_int(pValue,sTm.tm_year-1900);` |
|      9 | 8343 | `	if( isAssoc ){` |
|      3 | 8344 | `		ph7_array_add_strkey_elem(pArray,"tm_year",pValue);` |
|      2 | 8345 | `	}else{` |
|      7 | 8346 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8347 | `	}` |
|      - | 8348 | `	/* wday */` |
|      9 | 8349 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|      9 | 8350 | `	if( isAssoc ){` |
|      3 | 8351 | `		ph7_array_add_strkey_elem(pArray,"tm_wday",pValue);` |
|      2 | 8352 | `	}else{` |
|      7 | 8353 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8354 | `	}` |
|      - | 8355 | `	/* yday */` |
|      9 | 8356 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|      9 | 8357 | `	if( isAssoc ){` |
|      3 | 8358 | `		ph7_array_add_strkey_elem(pArray,"tm_yday",pValue);` |
|      2 | 8359 | `	}else{` |
|      7 | 8360 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8361 | `	}` |
|      - | 8362 | `	/* isdst */` |
|      - | 8363 | `#ifdef __WINNT__` |
|      - | 8364 | `#ifdef _MSC_VER` |
|      - | 8365 | `#ifndef _WIN32_WCE` |
|      1 | 8366 | `			_get_daylight(&sTm.tm_isdst);` |
|      - | 8367 | `#endif` |
|      - | 8368 | `#endif` |
|      - | 8369 | `#endif` |
|      9 | 8370 | `	ph7_value_int(pValue,sTm.tm_isdst);` |
|      9 | 8371 | `	if( isAssoc ){` |
|      3 | 8372 | `		ph7_array_add_strkey_elem(pArray,"tm_isdst",pValue);` |
|      2 | 8373 | `	}else{` |
|      7 | 8374 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8375 | `	}` |
|      - | 8376 | `	/* Return the array */` |
|      9 | 8377 | `	ph7_result_value(pCtx,pArray);` |
|      9 | 8378 | `	return PH7_OK;` |
|      5 | 8379 |  |
|      - | 8380 | `/*` |
|      - | 8381 | ` * int idate(string $format [, int $timestamp = time() ])` |
|      - | 8382 | ` *  Returns a number formatted according to the given format string` |
|      - | 8383 | ` *  using the given integer timestamp or the current local time if` |
|      - | 8384 | ` *  no timestamp is given. In other words, timestamp is optional and defaults` |
|      - | 8385 | ` *  to the value of time().` |
|      - | 8386 | ` *  Unlike the function date(), idate() accepts just one char in the format` |
|      - | 8387 | ` *  parameter.` |
|      - | 8388 | ` * $Parameters` |
|      - | 8389 | ` *  Supported format` |
|      - | 8390 | ` *   d 	Day of the month` |
|      - | 8391 | ` *   h 	Hour (12 hour format)` |
|      - | 8392 | ` *   H 	Hour (24 hour format)` |
|      - | 8393 | ` *   i 	Minutes` |
|      - | 8394 | ` *   I (uppercase i)1 if DST is activated, 0 otherwise` |
|      - | 8395 | ` *   L (uppercase l) returns 1 for leap year, 0 otherwise` |
|      - | 8396 | ` *   m 	Month number` |
|      - | 8397 | ` *   s 	Seconds` |
|      - | 8398 | ` *   t 	Days in current month` |
|      - | 8399 | ` *   U 	Seconds since the Unix Epoch - January 1 1970 00:00:00 UTC - this is the same as time()` |
|      - | 8400 | ` *   w 	Day of the week (0 on Sunday)` |
|      - | 8401 | ` *   W 	ISO-8601 week number of year, weeks starting on Monday` |
|      - | 8402 | ` *   y 	Year (1 or 2 digits - check note below)` |
|      - | 8403 | ` *   Y 	Year (4 digits)` |
|      - | 8404 | ` *   z 	Day of the year` |
|      - | 8405 | ` *   Z 	Timezone offset in seconds` |
|      - | 8406 | ` * $timestamp` |
|      - | 8407 | ` *  The optional timestamp parameter is an integer Unix timestamp that defaults` |
|      - | 8408 | ` *  to the current local time if a timestamp is not given. In other words, it defaults` |
|      - | 8409 | ` *  to the value of time().` |
|      - | 8410 | ` * Return` |
|      - | 8411 | ` *  An integer.` |
|      - | 8412 | ` */` |
|     40 | 8413 | `static int PH7_builtin_idate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 8414 |  |
|      - | 8415 | `	const char *zFormat;` |
|     42 | 8416 | `	ph7_int64 iVal = 0;` |
|      - | 8417 | `	int nLen;` |
|      - | 8418 | `	Sytm sTm;` |
|     42 | 8419 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8420 | `		/* Missing/Invalid argument,return -1 */` |
|      5 | 8421 | `		ph7_result_int(pCtx,-1);` |
|      5 | 8422 | `		return PH7_OK;` |
|      - | 8423 | `	}` |
|     42 | 8424 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     42 | 8425 | `	if( nLen < 1 ){` |
|      - | 8426 | `		/* Don't bother processing return -1*/` |
|    ! 0 | 8427 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 8428 | `	}` |
|     42 | 8429 | `	if( nArg < 2 ){` |
|      - | 8430 | `#ifdef __WINNT__` |
|      - | 8431 | `		SYSTEMTIME sOS;` |
|      2 | 8432 | `		GetSystemTime(&sOS);` |
|      2 | 8433 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8434 | `#else` |
|      - | 8435 | `		struct tm *pTm;` |
|      - | 8436 | `		time_t t;` |
|     30 | 8437 | `		time(&t);` |
|     30 | 8438 | `		pTm = localtime(&t);` |
|     30 | 8439 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8440 | `#endif` |
|     18 | 8441 | `	}else{` |
|      - | 8442 | `		/* Use the given timestamp */` |
|      - | 8443 | `		time_t t;` |
|      - | 8444 | `		struct tm *pTm;` |
|     11 | 8445 | `		if( ph7_value_is_int(apArg[1]) ){` |
|     11 | 8446 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|     11 | 8447 | `			pTm = localtime(&t);` |
|     11 | 8448 | `			if( pTm == 0 ){` |
|    ! 0 | 8449 | `				time(&t);` |
|    ! 0 | 8450 | `			}` |
|      6 | 8451 | `		}else{` |
|    ! 0 | 8452 | `			time(&t);` |
|      - | 8453 | `		}` |
|     11 | 8454 | `		pTm = localtime(&t);` |
|     11 | 8455 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8456 | `	}` |
|      - | 8457 | `	/* Perform the requested operation */` |
|     42 | 8458 | `	switch(zFormat[0]){` |
|      2 | 8459 | `	case 'd':` |
|      - | 8460 | `		/* Day of the month */` |
|      5 | 8461 | `		iVal = sTm.tm_mday;` |
|      5 | 8462 | `		break;` |
|    ! 0 | 8463 | `	case 'h':` |
|      - | 8464 | `		/*	Hour (12 hour format)*/` |
|    ! 0 | 8465 | `		iVal = 1 + (sTm.tm_hour % 12);` |
|    ! 0 | 8466 | `		break;` |
|      1 | 8467 | `	case 'H':` |
|      - | 8468 | `		/* Hour (24 hour format)*/` |
|      3 | 8469 | `		iVal = sTm.tm_hour;` |
|      3 | 8470 | `		break;` |
|      1 | 8471 | `	case 'i':` |
|      - | 8472 | `		/*Minutes*/` |
|      3 | 8473 | `		iVal = sTm.tm_min;` |
|      3 | 8474 | `		break;` |
|      1 | 8475 | `	case 'I':` |
|      - | 8476 | `		/*	returns 1 if DST is activated, 0 otherwise */` |
|      - | 8477 | `#ifdef __WINNT__` |
|      - | 8478 | `#ifdef _MSC_VER` |
|      - | 8479 | `#ifndef _WIN32_WCE` |
|      1 | 8480 | `			_get_daylight(&sTm.tm_isdst);` |
|      - | 8481 | `#endif` |
|      - | 8482 | `#endif` |
|      - | 8483 | `#endif` |
|      3 | 8484 | `		iVal = sTm.tm_isdst;` |
|      3 | 8485 | `		break;` |
|      1 | 8486 | `	case 'L':` |
|      - | 8487 | `		/* 	returns 1 for leap year, 0 otherwise */` |
|      3 | 8488 | `		iVal = IS_LEAP_YEAR(sTm.tm_year);` |
|      3 | 8489 | `		break;` |
|      2 | 8490 | `	case 'm':` |
|      - | 8491 | `		/* Month number*/` |
|      5 | 8492 | `		iVal = sTm.tm_mon;` |
|      5 | 8493 | `		break;` |
|      1 | 8494 | `	case 's':` |
|      - | 8495 | `		/*Seconds*/` |
|      3 | 8496 | `		iVal = sTm.tm_sec;` |
|      3 | 8497 | `		break;` |
|      1 | 8498 | `	case 't':{` |
|      - | 8499 | `		/*Days in current month*/` |
|      - | 8500 | `		static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|      5 | 8501 | `		int nDays = aMonDays[sTm.tm_mon % 12 ];` |
|      5 | 8502 | `		if( sTm.tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(sTm.tm_year) ){` |
|      3 | 8503 | `			nDays = 28;` |
|      1 | 8504 | `		}` |
|      7 | 8505 | `		iVal = nDays;` |
|      7 | 8506 | `		break;` |
|      - | 8507 | `			 }` |
|      1 | 8508 | `	case 'U':` |
|      - | 8509 | `		/*Seconds since the Unix Epoch*/` |
|      3 | 8510 | `		iVal = (ph7_int64)time(0);` |
|      3 | 8511 | `		break;` |
|      1 | 8512 | `	case 'w':` |
|      - | 8513 | `		/*	Day of the week (0 on Sunday) */` |
|      3 | 8514 | `		iVal = sTm.tm_wday;` |
|      3 | 8515 | `		break;` |
|      1 | 8516 | `	case 'W': {` |
|      - | 8517 | `		/* ISO-8601 week number of year, weeks starting on Monday */` |
|      - | 8518 | `		static const int aISO8601_local[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|      3 | 8519 | `		iVal = aISO8601_local[sTm.tm_wday % 7 ];` |
|      3 | 8520 | `		break;` |
|      - | 8521 | `			  }` |
|    ! 0 | 8522 | `	case 'y':` |
|      - | 8523 | `		/* Year (2 digits) */` |
|    ! 0 | 8524 | `		iVal = sTm.tm_year % 100;` |
|    ! 0 | 8525 | `		break;` |
|      3 | 8526 | `	case 'Y':` |
|      - | 8527 | `		/* Year (4 digits) */` |
|      7 | 8528 | `		iVal = sTm.tm_year;` |
|      7 | 8529 | `		break;` |
|      1 | 8530 | `	case 'z':` |
|      - | 8531 | `		/* Day of the year */` |
|      3 | 8532 | `		iVal = sTm.tm_yday;` |
|      3 | 8533 | `		break;` |
|      1 | 8534 | `	case 'Z':` |
|      - | 8535 | `		/*Timezone offset in seconds*/` |
|      3 | 8536 | `		iVal = sTm.tm_gmtoff;` |
|      3 | 8537 | `		break;` |
|      1 | 8538 | `	default:` |
|      - | 8539 | `		/* unknown format,throw a warning */` |
|      3 | 8540 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Unknown date format token");` |
|      2 | 8541 | `		break;` |
|      - | 8542 | `	}` |
|      - | 8543 | `	/* Return the time value */` |
|     40 | 8544 | `	ph7_result_int64(pCtx,iVal);` |
|     40 | 8545 | `	return PH7_OK;` |
|     23 | 8546 |  |
|      - | 8547 | `/*` |
|      - | 8548 | ` * int mktime/gmmktime([ int $hour = date("H") [, int $minute = date("i") [, int $second = date("s")` |
|      - | 8549 | ` *  [, int $month = date("n") [, int $day = date("j") [, int $year = date("Y") [, int $is_dst = -1 ]]]]]]] )` |
|      - | 8550 | ` *  Returns the Unix timestamp corresponding to the arguments given. This timestamp is a 64bit integer` |
|      - | 8551 | ` *  containing the number of seconds between the Unix Epoch (January 1 1970 00:00:00 GMT) and the time` |
|      - | 8552 | ` *  specified.` |
|      - | 8553 | ` *  Arguments may be left out in order from right to left; any arguments thus omitted will be set to` |
|      - | 8554 | ` *  the current value according to the local date and time.` |
|      - | 8555 | ` * Parameters` |
|      - | 8556 | ` * $hour` |
|      - | 8557 | ` *  The number of the hour relevant to the start of the day determined by month, day and year.` |
|      - | 8558 | ` *  Negative values reference the hour before midnight of the day in question. Values greater` |
|      - | 8559 | ` *  than 23 reference the appropriate hour in the following day(s).` |
|      - | 8560 | ` * $minute` |
|      - | 8561 | ` *  The number of the minute relevant to the start of the hour. Negative values reference` |
|      - | 8562 | ` *  the minute in the previous hour. Values greater than 59 reference the appropriate minute` |
|      - | 8563 | ` *  in the following hour(s).` |
|      - | 8564 | ` * $second` |
|      - | 8565 | ` *  The number of seconds relevant to the start of the minute. Negative values reference` |
|      - | 8566 | ` *  the second in the previous minute. Values greater than 59 reference the appropriate` |
|      - | 8567 | ` * second in the following minute(s).` |
|      - | 8568 | ` * $month` |
|      - | 8569 | ` *  The number of the month relevant to the end of the previous year. Values 1 to 12 reference` |
|      - | 8570 | ` *  the normal calendar months of the year in question. Values less than 1 (including negative values)` |
|      - | 8571 | ` *  reference the months in the previous year in reverse order, so 0 is December, -1 is November)...` |
|      - | 8572 | ` * $day` |
|      - | 8573 | ` *  The number of the day relevant to the end of the previous month. Values 1 to 28, 29, 30 or 31` |
|      - | 8574 | ` *  (depending upon the month) reference the normal days in the relevant month. Values less than 1` |
|      - | 8575 | ` *  (including negative values) reference the days in the previous month, so 0 is the last day` |
|      - | 8576 | ` *  of the previous month, -1 is the day before that, etc. Values greater than the number of days` |
|      - | 8577 | ` *  in the relevant month reference the appropriate day in the following month(s).` |
|      - | 8578 | ` * $year` |
|      - | 8579 | ` *  The number of the year, may be a two or four digit value, with values between 0-69 mapping` |
|      - | 8580 | ` *  to 2000-2069 and 70-100 to 1970-2000. On systems where time_t is a 32bit signed integer, as` |
|      - | 8581 | ` *  most common today, the valid range for year is somewhere between 1901 and 2038.` |
|      - | 8582 | ` * $is_dst` |
|      - | 8583 | ` *  This parameter can be set to 1 if the time is during daylight savings time (DST), 0 if it is not,` |
|      - | 8584 | ` *  or -1 (the default) if it is unknown whether the time is within daylight savings time or not.` |
|      - | 8585 | ` * Return` |
|      - | 8586 | ` *   mktime() returns the Unix timestamp of the arguments given.` |
|      - | 8587 | ` *   If the arguments are invalid, the function returns FALSE` |
|      - | 8588 | ` */` |
|      8 | 8589 | `static int PH7_builtin_mktime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8590 |  |
|      - | 8591 | `	const char *zFunction;` |
|      9 | 8592 | `	ph7_int64 iVal = 0;` |
|      - | 8593 | `	struct tm *pTm;` |
|      - | 8594 | `	time_t t;` |
|      - | 8595 | `	/* Extract function name */` |
|      9 | 8596 | `	zFunction = ph7_function_name(pCtx);` |
|      - | 8597 | `	/* Get the current time */` |
|      9 | 8598 | `	time(&t);` |
|      9 | 8599 | `	if( zFunction[0] == 'g' /* gmmktime */ ){` |
|      3 | 8600 | `		pTm = gmtime(&t);` |
|      2 | 8601 | `	}else{` |
|      - | 8602 | `		/* localtime */` |
|      7 | 8603 | `		pTm = localtime(&t);` |
|      - | 8604 | `	}` |
|      9 | 8605 | `	if( nArg > 0 ){` |
|      - | 8606 | `		int iTmp;` |
|      - | 8607 | `		/* Hour */` |
|      9 | 8608 | `		iTmp = ph7_value_to_int(apArg[0]);` |
|      9 | 8609 | `		pTm->tm_hour = iTmp;` |
|      9 | 8610 | `		if( nArg > 1 ){` |
|      - | 8611 | `			/* Minutes */` |
|      9 | 8612 | `			iTmp = ph7_value_to_int(apArg[1]);` |
|      9 | 8613 | `			pTm->tm_min = iTmp;` |
|      9 | 8614 | `			if( nArg > 2 ){` |
|      - | 8615 | `				/* Seconds */` |
|      9 | 8616 | `				iTmp = ph7_value_to_int(apArg[2]);` |
|      9 | 8617 | `				pTm->tm_sec = iTmp;` |
|      9 | 8618 | `				if( nArg > 3 ){` |
|      - | 8619 | `					/* Month */` |
|      9 | 8620 | `					iTmp = ph7_value_to_int(apArg[3]);` |
|      9 | 8621 | `					pTm->tm_mon = iTmp - 1;` |
|      9 | 8622 | `					if( nArg > 4 ){` |
|      - | 8623 | `						/* mday */` |
|      9 | 8624 | `						iTmp = ph7_value_to_int(apArg[4]);` |
|      9 | 8625 | `						pTm->tm_mday = iTmp;` |
|      9 | 8626 | `						if( nArg > 5 ){` |
|      - | 8627 | `							/* Year */` |
|      9 | 8628 | `							iTmp = ph7_value_to_int(apArg[5]);` |
|      9 | 8629 | `							if( iTmp > 1900 ){` |
|      9 | 8630 | `								iTmp -= 1900;` |
|      4 | 8631 | `							}` |
|      9 | 8632 | `							pTm->tm_year = iTmp;` |
|      9 | 8633 | `							if( nArg > 6 ){` |
|      - | 8634 | `								/* is_dst */` |
|    ! 0 | 8635 | `								iTmp = ph7_value_to_bool(apArg[6]);` |
|    ! 0 | 8636 | `								pTm->tm_isdst = iTmp;` |
|    ! 0 | 8637 | `							}` |
|      4 | 8638 | `						}` |
|      4 | 8639 | `					}` |
|      4 | 8640 | `				}` |
|      4 | 8641 | `			}` |
|      4 | 8642 | `		}` |
|      4 | 8643 | `	}` |
|      - | 8644 | `	/* Make the time */` |
|      9 | 8645 | `	iVal = (ph7_int64)mktime(pTm);` |
|      - | 8646 | `	/* Return the timesatmp as a 64bit integer */` |
|      9 | 8647 | `	ph7_result_int64(pCtx,iVal);` |
|      9 | 8648 | `	return PH7_OK;` |
|      1 | 8649 |  |
|      - | 8650 | `/*` |
|      - | 8651 | ` * Section:` |
|      - | 8652 | ` *    URL handling Functions.` |
|      - | 8653 | ` * Status:` |
|      - | 8654 | ` *    Stable.` |
|      - | 8655 | ` */` |
|      - | 8656 | `/*` |
|      - | 8657 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 8658 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 8659 | ` */` |
|   1026 | 8660 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 8661 |  |
|      - | 8662 | `	/* Store in the call context result buffer */` |
|   1028 | 8663 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 8664 | `	return SXRET_OK;` |
|      2 | 8665 |  |
|      - | 8666 | `/*` |
|      - | 8667 | ` * string base64_encode(string $data)` |
|      - | 8668 | ` * string convert_uuencode(string $data)` |
|      - | 8669 | ` *  Encodes data with MIME base64` |
|      - | 8670 | ` * Parameter` |
|      - | 8671 | ` *  $data` |
|      - | 8672 | ` *    Data to encode` |
|      - | 8673 | ` * Return` |
|      - | 8674 | ` *  Encoded data or FALSE on failure.` |
|      - | 8675 | ` */` |
|     10 | 8676 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8677 |  |
|      - | 8678 | `	const char *zIn;` |
|      - | 8679 | `	int nLen;` |
|     11 | 8680 | `	if( nArg < 1 ){` |
|      - | 8681 | `		/* Missing arguments,return FALSE */` |
|      5 | 8682 | `		ph7_result_bool(pCtx,0);` |
|      5 | 8683 | `		return PH7_OK;` |
|      - | 8684 | `	}` |
|      - | 8685 | `	/* Extract the input string */` |
|      7 | 8686 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8687 | `	if( nLen < 1 ){` |
|      - | 8688 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8689 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8690 | `		return PH7_OK;` |
|      - | 8691 | `	}` |
|      - | 8692 | `	/* Perform the BASE64 encoding */` |
|      7 | 8693 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 8694 | `	return PH7_OK;` |
|      6 | 8695 |  |
|      - | 8696 | `/*` |
|      - | 8697 | ` * string base64_decode(string $data)` |
|      - | 8698 | ` * string convert_uudecode(string $data)` |
|      - | 8699 | ` *  Decodes data encoded with MIME base64` |
|      - | 8700 | ` * Parameter` |
|      - | 8701 | ` *  $data` |
|      - | 8702 | ` *    Encoded data.` |
|      - | 8703 | ` * Return` |
|      - | 8704 | ` *  Returns the original data or FALSE on failure.` |
|      - | 8705 | ` */` |
|     36 | 8706 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 8707 |  |
|      - | 8708 | `	const char *zIn;` |
|      - | 8709 | `	int nLen;` |
|     38 | 8710 | `	if( nArg < 1 ){` |
|      - | 8711 | `		/* Missing arguments,return FALSE */` |
|      3 | 8712 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8713 | `		return PH7_OK;` |
|      - | 8714 | `	}` |
|      - | 8715 | `	/* Extract the input string */` |
|     36 | 8716 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 8717 | `	if( nLen < 1 ){` |
|      - | 8718 | `		/* Nothing to process,return FALSE */` |
|      3 | 8719 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8720 | `		return PH7_OK;` |
|      - | 8721 | `	}` |
|      - | 8722 | `	/* Perform the BASE64 decoding */` |
|     34 | 8723 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 8724 | `	return PH7_OK;` |
|     20 | 8725 |  |
|      - | 8726 | `/*` |
|      - | 8727 | ` * string urlencode(string $str)` |
|      - | 8728 | ` *  URL encoding` |
|      - | 8729 | ` * Parameter` |
|      - | 8730 | ` *  $data` |
|      - | 8731 | ` *   Input string.` |
|      - | 8732 | ` * Return` |
|      - | 8733 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 8734 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 8735 | ` *  encoded as plus (+) signs.` |
|      - | 8736 | ` */` |
|      6 | 8737 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8738 |  |
|      - | 8739 | `	const char *zIn;` |
|      - | 8740 | `	int nLen;` |
|      7 | 8741 | `	if( nArg < 1 ){` |
|      - | 8742 | `		/* Missing arguments,return FALSE */` |
|      3 | 8743 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8744 | `		return PH7_OK;` |
|      - | 8745 | `	}` |
|      - | 8746 | `	/* Extract the input string */` |
|      5 | 8747 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 8748 | `	if( nLen < 1 ){` |
|      - | 8749 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8750 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8751 | `		return PH7_OK;` |
|      - | 8752 | `	}` |
|      - | 8753 | `	/* Perform the URL encoding */` |
|      5 | 8754 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 8755 | `	return PH7_OK;` |
|      4 | 8756 |  |
|      - | 8757 | `/*` |
|      - | 8758 | ` * string urldecode(string $str)` |
|      - | 8759 | ` *  Decodes any %## encoding in the given string.` |
|      - | 8760 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 8761 | ` * Parameter` |
|      - | 8762 | ` *  $data` |
|      - | 8763 | ` *    Input string.` |
|      - | 8764 | ` * Return` |
|      - | 8765 | ` *  Decoded URL or FALSE on failure.` |
|      - | 8766 | ` */` |
|      8 | 8767 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8768 |  |
|      - | 8769 | `	const char *zIn;` |
|      - | 8770 | `	int nLen;` |
|      9 | 8771 | `	if( nArg < 1 ){` |
|      - | 8772 | `		/* Missing arguments,return FALSE */` |
|      3 | 8773 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8774 | `		return PH7_OK;` |
|      - | 8775 | `	}` |
|      - | 8776 | `	/* Extract the input string */` |
|      7 | 8777 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8778 | `	if( nLen < 1 ){` |
|      - | 8779 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8780 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8781 | `		return PH7_OK;` |
|      - | 8782 | `	}` |
|      - | 8783 | `	/* Perform the URL decoding */` |
|      7 | 8784 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 8785 | `	return PH7_OK;` |
|      5 | 8786 |  |
|      - | 8787 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8788 | `/* Table of the built-in functions */` |
|      - | 8789 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 8790 | `	   /* Variable handling functions */` |
|      - | 8791 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 8792 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 8793 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 8794 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 8795 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 8796 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 8797 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 8798 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 8799 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 8800 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 8801 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 8802 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 8803 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 8804 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 8805 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 8806 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 8807 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 8808 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 8809 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 8810 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8811 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 8812 | `	   /* Math functions */` |
|      - | 8813 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 8814 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 8815 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 8816 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 8817 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 8818 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 8819 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 8820 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 8821 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 8822 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 8823 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 8824 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 8825 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 8826 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 8827 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 8828 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 8829 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 8830 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 8831 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 8832 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 8833 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 8834 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 8835 | `	{ "round",    PH7_builtin_round        },` |
|      - | 8836 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 8837 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 8838 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 8839 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 8840 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 8841 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 8842 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 8843 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 8844 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8845 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8846 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 8847 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8848 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8849 | `	   /* String handling functions */` |
|      - | 8850 |  |
|      - | 8851 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 8852 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 8853 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 8854 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 8855 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 8856 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 8857 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 8858 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 8859 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 8860 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 8861 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 8862 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 8863 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 8864 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 8865 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 8866 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 8867 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 8868 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 8869 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 8870 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 8871 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 8872 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 8873 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 8874 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 8875 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 8876 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 8877 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 8878 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 8879 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 8880 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 8881 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 8882 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 8883 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 8884 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 8885 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 8886 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 8887 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 8888 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 8889 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 8890 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 8891 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 8892 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 8893 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 8894 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 8895 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 8896 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 8897 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 8898 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 8899 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 8900 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8901 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8902 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 8903 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 8904 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 8905 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 8906 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8907 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8908 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 8909 |  |
|      - | 8910 |  |
|      - | 8911 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 8912 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 8913 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 8914 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 8915 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 8916 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8917 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8918 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 8919 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 8920 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8921 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8922 |  |
|      - | 8923 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 8924 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 8925 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 8926 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 8927 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 8928 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 8929 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 8930 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 8931 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 8932 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 8933 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 8934 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 8935 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8936 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8937 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 8938 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8939 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8940 |  |
|      - | 8941 | `	         /* Ctype functions */` |
|      - | 8942 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 8943 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 8944 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 8945 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 8946 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 8947 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 8948 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 8949 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 8950 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 8951 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 8952 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 8953 | `	         /* Time functions */` |
|      - | 8954 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 8955 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 8956 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 8957 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 8958 | `	{ "date",        PH7_builtin_date         },` |
|      - | 8959 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 8960 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 8961 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 8962 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 8963 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 8964 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 8965 | `	        /* URL functions */` |
|      - | 8966 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 8967 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 8968 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 8969 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 8970 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 8971 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 8972 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 8973 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 8974 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8975 | `};` |
|      - | 8976 | `/*` |
|      - | 8977 | ` * Register the built-in functions defined above,the array functions` |
|      - | 8978 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 8979 | ` */` |
|   1118 | 8980 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      2 | 8981 |  |
|      - | 8982 | `	sxu32 n;` |
| 171056 | 8983 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 169938 | 8984 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
|  84970 | 8985 | `	}` |
|      - | 8986 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   1120 | 8987 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 8988 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   1120 | 8989 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   1120 | 8990 |  |
|      - | 8991 |  |
