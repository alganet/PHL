# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3774/4424 lines (85.31%)

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
|     92 |  152 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  153 |  |
|     94 |  154 | `	int res = 0; /* Assume false by default */` |
|     94 |  155 | `	if( nArg > 0 ){` |
|     92 |  156 | `		res = ph7_value_is_array(apArg[0]);` |
|     45 |  157 | `	}` |
|      - |  158 | `	/* Query result */` |
|     94 |  159 | `	ph7_result_bool(pCtx,res);` |
|     94 |  160 | `	return PH7_OK;` |
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
|  17386 |  269 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  270 |  |
|  17388 |  271 | `	int res = 1; /* Assume empty by default */` |
|  17388 |  272 | `	if( nArg > 0 ){` |
|  17386 |  273 | `		res = ph7_value_is_empty(apArg[0]);` |
|   8692 |  274 | `	}` |
|  17388 |  275 | `	ph7_result_bool(pCtx,res);` |
|  17388 |  276 | `	return PH7_OK;` |
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
| 122428 | 1288 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1289 |  |
|      - | 1290 | `	const char *zSource,*zOfft;` |
|      - | 1291 | `	int nOfft,nLen,nSrcLen;` |
| 122430 | 1292 | `	if( nArg < 2 ){` |
|      - | 1293 | `		/* return FALSE */` |
|      5 | 1294 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1295 | `		return PH7_OK;` |
|      - | 1296 | `	}` |
|      - | 1297 | `	/* Extract the target string */` |
| 122426 | 1298 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 122426 | 1299 | `	if( nSrcLen < 1 ){` |
|      - | 1300 | `		/* Empty string,return FALSE */` |
|   7604 | 1301 | `		ph7_result_bool(pCtx,0);` |
|   7604 | 1302 | `		return PH7_OK;` |
|      - | 1303 | `	}` |
| 114824 | 1304 | `	nLen = nSrcLen; /* cc warning */` |
|      - | 1305 | `	/* Extract the offset */` |
| 114824 | 1306 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 114824 | 1307 | `	if( nOfft < 0 ){` |
|  19442 | 1308 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  19442 | 1309 | `		if( zOfft < zSource ){` |
|      - | 1310 | `			/* Invalid offset */` |
|      5 | 1311 | `			ph7_result_bool(pCtx,0);` |
|      5 | 1312 | `			return PH7_OK;` |
|      - | 1313 | `		}` |
|  19438 | 1314 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  19438 | 1315 | `		nOfft = (int)(zOfft-zSource);` |
| 105102 | 1316 | `	}else if( nOfft >= nSrcLen ){` |
|      - | 1317 | `		/* Invalid offset */` |
|      7 | 1318 | `		ph7_result_bool(pCtx,0);` |
|      7 | 1319 | `		return PH7_OK;` |
|    ! 0 | 1320 | `	}else{` |
|  95378 | 1321 | `		zOfft = &zSource[nOfft];` |
|  95378 | 1322 | `		nLen = nSrcLen - nOfft;` |
|      - | 1323 | `	}` |
| 114814 | 1324 | `	if( nArg > 2 ){` |
|      - | 1325 | `		/* Extract the length */` |
|  95376 | 1326 | `		nLen = ph7_value_to_int(apArg[2]);` |
|  95376 | 1327 | `		if( nLen == 0 ){` |
|      - | 1328 | `			/* Invalid length,return an empty string */` |
|      5 | 1329 | `			ph7_result_string(pCtx,"",0);` |
|      5 | 1330 | `			return PH7_OK;` |
|  95372 | 1331 | `		}else if( nLen < 0 ){` |
|  19440 | 1332 | `			nLen = nSrcLen + nLen - nOfft;` |
|  19440 | 1333 | `			if( nLen < 1 ){` |
|      - | 1334 | `				/* Invalid  length */` |
|      3 | 1335 | `				nLen = nSrcLen - nOfft;` |
|      1 | 1336 | `			}` |
|   9719 | 1337 | `		}` |
|  95372 | 1338 | `		if( nLen + nOfft > nSrcLen ){` |
|      - | 1339 | `			/* Invalid length */` |
|   2298 | 1340 | `			nLen = nSrcLen - nOfft;` |
|   1148 | 1341 | `		}` |
|  47685 | 1342 | `	}` |
|      - | 1343 | `	/* Return the substring */` |
| 114810 | 1344 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 114810 | 1345 | `	return PH7_OK;` |
|  61216 | 1346 |  |
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
|   2000 | 2315 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2316 |  |
|   2002 | 2317 | `	int iLen = 0;` |
|   2002 | 2318 | `	if( nArg > 0 ){` |
|   2000 | 2319 | `		ph7_value_to_string(apArg[0],&iLen);` |
|    999 | 2320 | `	}` |
|      - | 2321 | `	/* String length */` |
|   2002 | 2322 | `	ph7_result_int(pCtx,iLen);` |
|   2002 | 2323 | `	return PH7_OK;` |
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
|  84546 | 2468 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      2 | 2469 |  |
|  42273 | 2470 | `	SXUNUSED(pKey);` |
|  84548 | 2471 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 2472 | `	const char *zData;` |
|      - | 2473 | `	int nLen;` |
|  84548 | 2474 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
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
|  84546 | 2491 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 2492 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
|  84546 | 2493 | `	if( pData->bFirst ){` |
|  19608 | 2494 | `		pData->bFirst = 0;` |
|  74743 | 2495 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 2496 | `		/* append the separator first */` |
|  64928 | 2497 | `		ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|  32463 | 2498 | `	}` |
|      - | 2499 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
|  84546 | 2500 | `	if( nLen > 0 ){` |
|  76944 | 2501 | `		ph7_result_string(pData->pCtx,zData,nLen);` |
|  38471 | 2502 | `	}` |
|  84546 | 2503 | `	return PH7_OK;` |
|  42275 | 2504 |  |
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
|  19634 | 2518 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2519 |  |
|      - | 2520 | `	struct implode_data imp_data;` |
|  19636 | 2521 | `	int i = 1;` |
|  19636 | 2522 | `	if( nArg < 1 ){` |
|      - | 2523 | `		/* Missing argument,return NULL */` |
|    ! 0 | 2524 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2525 | `		return PH7_OK;` |
|      - | 2526 | `	}` |
|      - | 2527 | `	/* Prepare the implode context */` |
|  19636 | 2528 | `	imp_data.pCtx = pCtx;` |
|  19636 | 2529 | `	imp_data.bRecursive = 0;` |
|  19636 | 2530 | `	imp_data.bFirst = 1;` |
|  19636 | 2531 | `	imp_data.nRecCount = 0;` |
|  19636 | 2532 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  19634 | 2533 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|   9818 | 2534 | `	}else{` |
|      3 | 2535 | `		imp_data.zSep = 0;` |
|      3 | 2536 | `		imp_data.nSeplen = 0;` |
|      3 | 2537 | `		i = 0;` |
|      - | 2538 | `	}` |
|  19636 | 2539 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 2540 | `	/* Start the 'join' process */` |
|  39270 | 2541 | `	while( i < nArg ){` |
|  19636 | 2542 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2543 | `			/* Iterate throw array entries */` |
|  19636 | 2544 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|   9819 | 2545 | `		}else{` |
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
|  19636 | 2561 | `		i++;` |
|      2 | 2562 | `	}` |
|  19636 | 2563 | `	return PH7_OK;` |
|   9819 | 2564 |  |
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
|   3598 | 2653 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2654 |  |
|      - | 2655 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 2656 | `	int nDelim,nStrlen,iLimit;` |
|      - | 2657 | `	ph7_value *pArray;` |
|      - | 2658 | `	ph7_value *pValue;` |
|      - | 2659 | `	sxu32 nOfft;` |
|      - | 2660 | `	sxi32 rc;` |
|   3600 | 2661 | `	if( nArg < 2 ){` |
|      - | 2662 | `		/* Missing arguments,return FALSE */` |
|      9 | 2663 | `		ph7_result_bool(pCtx,0);` |
|      9 | 2664 | `		return PH7_OK;` |
|      - | 2665 | `	}` |
|      - | 2666 | `	/* Extract the delimiter */` |
|   3592 | 2667 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   3592 | 2668 | `	if( nDelim < 1 ){` |
|      - | 2669 | `		/* Empty delimiter,return FALSE */` |
|      3 | 2670 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2671 | `		return PH7_OK;` |
|      - | 2672 | `	}` |
|      - | 2673 | `	/* Extract the string */` |
|   3590 | 2674 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   3590 | 2675 | `	if( nStrlen < 1 ){` |
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
|   3588 | 2690 | `	zEnd = &zString[nStrlen];` |
|      - | 2691 | `	/* Create the array */` |
|   3588 | 2692 | `	pArray =  ph7_context_new_array(pCtx);` |
|   3588 | 2693 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   3588 | 2694 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2695 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 2696 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2697 | `		return PH7_OK;` |
|      - | 2698 | `	}` |
|      - | 2699 | `	/* Set a defualt limit */` |
|   3588 | 2700 | `	iLimit = SXI32_HIGH;` |
|   3588 | 2701 | `	if( nArg > 2 ){` |
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
|  41788 | 2712 | `	for(;;){` |
|  83578 | 2713 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
|  83578 | 2714 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 2715 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   3588 | 2716 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   3588 | 2717 | `			ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|   3588 | 2718 | `			break;` |
|      - | 2719 | `		}` |
|      - | 2720 | `		/* Point to the desired offset */` |
|  79992 | 2721 | `		zCur = &zString[nOfft];` |
|      - | 2722 | `		/* Perform the store operation (may be empty) */` |
|  79992 | 2723 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|  79992 | 2724 | `		ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|      - | 2725 | `		/* Point beyond the delimiter */` |
|  79992 | 2726 | `		zString = &zCur[nDelim];` |
|      - | 2727 | `		/* Reset the cursor */` |
|  79992 | 2728 | `		ph7_value_reset_string_cursor(pValue);` |
|      2 | 2729 | `	}` |
|      - | 2730 | `	/* Return the freshly created array */` |
|   3588 | 2731 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2732 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 2733 | `	 * released as soon we return from this foregin function.` |
|      - | 2734 | `	 */` |
|   3588 | 2735 | `	return PH7_OK;` |
|   1801 | 2736 |  |
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
|   8740 | 2752 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2753 |  |
|      - | 2754 | `	const char *zString;` |
|      - | 2755 | `	int nLen;` |
|   8742 | 2756 | `	if( nArg < 1 ){` |
|      - | 2757 | `		/* Missing arguments,return null */` |
|      3 | 2758 | `		ph7_result_null(pCtx);` |
|      3 | 2759 | `		return PH7_OK;` |
|      - | 2760 | `	}` |
|      - | 2761 | `	/* Extract the target string */` |
|   8740 | 2762 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|   8740 | 2763 | `	if( nLen < 1 ){` |
|      - | 2764 | `		/* Empty string,return */` |
|   1602 | 2765 | `		ph7_result_string(pCtx,"",0);` |
|   1602 | 2766 | `		return PH7_OK;` |
|      - | 2767 | `	}` |
|      - | 2768 | `	/* Start the trim process */` |
|   7140 | 2769 | `	if( nArg < 2 ){` |
|      - | 2770 | `		SyString sStr;` |
|      - | 2771 | `		/* Remove white spaces and NUL bytes */` |
|   7136 | 2772 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  17266 | 2773 | `		SyStringFullTrimSafe(&sStr);` |
|   7136 | 2774 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   3569 | 2775 | `	}else{` |
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
|   7140 | 2829 | `	return PH7_OK;` |
|   4372 | 2830 |  |
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
|  19440 | 2994 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2995 |  |
|      - | 2996 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2997 | `	int nLen;` |
|  19442 | 2998 | `	if( nArg < 1 ){` |
|      - | 2999 | `		/* Missing arguments,return null */` |
|      3 | 3000 | `		ph7_result_null(pCtx);` |
|      3 | 3001 | `		return PH7_OK;` |
|      - | 3002 | `	}` |
|      - | 3003 | `	/* Extract the target string */` |
|  19440 | 3004 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  19440 | 3005 | `	if( nLen < 1 ){` |
|      - | 3006 | `		/* Empty string,return */` |
|      3 | 3007 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3008 | `		return PH7_OK;` |
|      - | 3009 | `	}` |
|      - | 3010 | `	/* Perform the requested operation */` |
|  19438 | 3011 | `	zEnd = &zString[nLen];` |
|  61404 | 3012 | `	for(;;){` |
| 122810 | 3013 | `		if( zString >= zEnd ){` |
|      - | 3014 | `			/* No more input,break immediately */` |
|  19438 | 3015 | `			break;` |
|      - | 3016 | `		}` |
| 103374 | 3017 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 3018 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 3019 | `			zCur = zString;` |
|    ! 0 | 3020 | `			zString++;` |
|    ! 0 | 3021 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 3022 | `				zString++;` |
|    ! 0 | 3023 | `			}` |
|      - | 3024 | `			/* Append UTF-8 stream */` |
|    ! 0 | 3025 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 3026 | `		}else{` |
| 103374 | 3027 | `			int c = zString[0];` |
| 103374 | 3028 | `			if( SyisUpper(c) ){` |
| 103372 | 3029 | `				c = SyToLower(zString[0]);` |
|  51685 | 3030 | `			}` |
|      - | 3031 | `			/* Append character */` |
| 103374 | 3032 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 3033 | `			/* Advance the cursor */` |
| 103374 | 3034 | `			zString++;` |
|      - | 3035 | `		}` |
|      2 | 3036 | `	}` |
|  19438 | 3037 | `	return PH7_OK;` |
|   9722 | 3038 |  |
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
|      - | 5342 | ` * $string` |
|      - | 5343 | ` *  The input string.` |
|      - | 5344 | ` * $split_length` |
|      - | 5345 | ` *  Maximum length of the chunk.` |
|      - | 5346 | ` * Return` |
|      - | 5347 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 5348 | ` *  except possibly the last one which may be shorter.` |
|      - | 5349 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 5350 | ` *  as the first (and only) array element.` |
|      - | 5351 | ` *  An empty string returns an empty array.` |
|      - | 5352 | ` * Errors` |
|      - | 5353 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 5354 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 5355 | ` *  ValueError if $split_length is less than 1.` |
|      - | 5356 | ` */` |
|     28 | 5357 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5358 |  |
|      - | 5359 | `	const char *zString,*zEnd;` |
|      - | 5360 | `	ph7_value *pArray,*pValue;` |
|      - | 5361 | `	int split_len;` |
|      - | 5362 | `	int nLen;` |
|     30 | 5363 | `	if( nArg < 1 ){` |
|      4 | 5364 | `		return PH7_VmThrowException(pCtx,` |
|      - | 5365 | `			"ArgumentCountError",` |
|      - | 5366 | `			"str_split() expects at least 1 argument, %d given",` |
|      1 | 5367 | `			nArg` |
|      - | 5368 | `			);` |
|      - | 5369 | `	}` |
|      - | 5370 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     50 | 5371 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     38 | 5372 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 5373 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 5374 | `		return PH7_VmThrowException(pCtx,` |
|      - | 5375 | `			"TypeError",` |
|      - | 5376 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 5377 | `			ph7_type_name(apArg[0])` |
|      - | 5378 | `			);` |
|      - | 5379 | `	}` |
|      - | 5380 | `	/* Point to the target string */` |
|     26 | 5381 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     26 | 5382 | `	split_len = (int)sizeof(char);` |
|     26 | 5383 | `	if( nArg > 1 ){` |
|      - | 5384 | `		/* Split length */` |
|     16 | 5385 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     16 | 5386 | `		if( split_len < 1 ){` |
|      5 | 5387 | `			return PH7_VmThrowException(pCtx,` |
|      - | 5388 | `				"ValueError",` |
|      - | 5389 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 5390 | `				);` |
|      - | 5391 | `		}` |
|     11 | 5392 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 5393 | `			split_len = nLen;` |
|      1 | 5394 | `		}` |
|      5 | 5395 | `	}` |
|      - | 5396 | `	/* Create the array and the scalar value */` |
|     21 | 5397 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 5398 | `	/*Chunk value */` |
|     21 | 5399 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 5400 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 5401 | `		/* Return FALSE */` |
|    ! 0 | 5402 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5403 | `		return PH7_OK;` |
|      - | 5404 | `	}` |
|      - | 5405 | `	/* Point to the end of the string */` |
|     21 | 5406 | `	zEnd = &zString[nLen];` |
|      - | 5407 | `	/* Perform the requested operation */` |
|     48 | 5408 | `	for(;;){` |
|      - | 5409 | `		int nMax;` |
|     59 | 5410 | `		if( zString >= zEnd ){` |
|      - | 5411 | `			/* No more input to process */` |
|     21 | 5412 | `			break;` |
|      - | 5413 | `		}` |
|     39 | 5414 | `		nMax = (int)(zEnd-zString);` |
|     39 | 5415 | `		if( nMax < split_len ){` |
|      3 | 5416 | `			split_len = nMax;` |
|      1 | 5417 | `		}` |
|      - | 5418 | `		/* Copy the current chunk */` |
|     39 | 5419 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 5420 | `		/* Insert it */` |
|     39 | 5421 | `		ph7_array_add_elem(pArray,0,pValue); /* Will make it's own copy */` |
|      - | 5422 | `		/* reset the string cursor */` |
|     39 | 5423 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 5424 | `		/* Update position */` |
|     39 | 5425 | `		zString += split_len;` |
|      1 | 5426 | `	}` |
|      - | 5427 | `	/*` |
|      - | 5428 | `	 * Return the array.` |
|      - | 5429 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 5430 | `	 * upon we return from this function.` |
|      - | 5431 | `	 */` |
|     21 | 5432 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 5433 | `	return PH7_OK;` |
|     16 | 5434 |  |
|      - | 5435 | `/*` |
|      - | 5436 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 5437 | ` * Refer to [strspn()].` |
|      - | 5438 | ` */` |
|     28 | 5439 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 5440 |  |
|     29 | 5441 | `	const char *zIn = *pzIn;` |
|      - | 5442 | `	const char *zPtr;` |
|      - | 5443 | `	/* Ignore leading white spaces */` |
|     29 | 5444 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 5445 | `		zIn++;` |
|    ! 0 | 5446 | `	}` |
|     29 | 5447 | `	if( zIn >= zEnd ){` |
|      - | 5448 | `		/* End of input */` |
|    ! 0 | 5449 | `		return SXERR_EOF;` |
|      - | 5450 | `	}` |
|     29 | 5451 | `	zPtr = zIn;` |
|      - | 5452 | `	/* Extract the token */` |
|    201 | 5453 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 5454 | `		zIn++;` |
|      1 | 5455 | `	}` |
|     29 | 5456 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5457 | `	/* Synchronize pointers */` |
|     29 | 5458 | `	*pzIn = zIn;` |
|      - | 5459 | `	/* Return to the caller */` |
|     29 | 5460 | `	return SXRET_OK;` |
|     15 | 5461 |  |
|      - | 5462 | `/*` |
|      - | 5463 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 5464 | ` * return the longest match.` |
|      - | 5465 | ` * Refer to [strspn()].` |
|      - | 5466 | ` */` |
|     18 | 5467 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5468 |  |
|     19 | 5469 | `	const char *zEnd = &zString[nLen];` |
|     19 | 5470 | `	const char *zIn = zString;` |
|      - | 5471 | `	int i,c;` |
|     45 | 5472 | `	for(;;){` |
|     91 | 5473 | `		if( zString >= zEnd ){` |
|      7 | 5474 | `			break;` |
|      - | 5475 | `		}` |
|      - | 5476 | `		/* Extract current character */` |
|     85 | 5477 | `		c = zString[0];` |
|      - | 5478 | `		/* Perform the lookup */` |
|    383 | 5479 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 5480 | `			if( c == zMask[i] ){` |
|      - | 5481 | `				/* Character found */` |
|     73 | 5482 | `				break;` |
|      - | 5483 | `			}` |
|    150 | 5484 | `		}` |
|     85 | 5485 | `		if( i >= nMaskLen ){` |
|      - | 5486 | `			/* Character not in the current mask,break immediately */` |
|     13 | 5487 | `			break;` |
|      - | 5488 | `		}` |
|      - | 5489 | `		/* Advance cursor */` |
|     73 | 5490 | `		zString++;` |
|      1 | 5491 | `	}` |
|      - | 5492 | `	/* Longest match */` |
|     19 | 5493 | `	return (int)(zString-zIn);` |
|      1 | 5494 |  |
|      - | 5495 | `/*` |
|      - | 5496 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 5497 | ` * Refer to [strcspn()].` |
|      - | 5498 | ` */` |
|     10 | 5499 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5500 |  |
|     11 | 5501 | `	const char *zEnd = &zString[nLen];` |
|     11 | 5502 | `	const char *zIn = zString;` |
|      - | 5503 | `	int i,c;` |
|     12 | 5504 | `	for(;;){` |
|     25 | 5505 | `		if( zString >= zEnd ){` |
|      3 | 5506 | `			break;` |
|      - | 5507 | `		}` |
|      - | 5508 | `		/* Extract current character */` |
|     23 | 5509 | `		c = zString[0];` |
|      - | 5510 | `		/* Perform the lookup */` |
|     51 | 5511 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 5512 | `			if( c == zMask[i] ){` |
|      9 | 5513 | `				break;` |
|      - | 5514 | `			}` |
|     15 | 5515 | `		}` |
|     23 | 5516 | `		if( i < nMaskLen ){` |
|      - | 5517 | `			/* Character in the current mask,break immediately */` |
|      9 | 5518 | `			break;` |
|      - | 5519 | `		}` |
|      - | 5520 | `		/* Advance cursor */` |
|     15 | 5521 | `		zString++;` |
|      1 | 5522 | `	}` |
|      - | 5523 | `	/* Longest match */` |
|     11 | 5524 | `	return (int)(zString-zIn);` |
|      1 | 5525 |  |
|      - | 5526 | `/*` |
|      - | 5527 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5528 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 5529 | ` *  of characters contained within a given mask.` |
|      - | 5530 | ` * Parameters` |
|      - | 5531 | ` * $str` |
|      - | 5532 | ` *  The input string.` |
|      - | 5533 | ` * $mask` |
|      - | 5534 | ` *  The list of allowable characters.` |
|      - | 5535 | ` * $start` |
|      - | 5536 | ` *  The position in subject to start searching.` |
|      - | 5537 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5538 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5539 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5540 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5541 | ` *  start'th position from the end of subject.` |
|      - | 5542 | ` * $length` |
|      - | 5543 | ` *  The length of the segment from subject to examine.` |
|      - | 5544 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5545 | ` *  characters after the starting position.` |
|      - | 5546 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5547 | ` *  position up to length characters from the end of subject.` |
|      - | 5548 | ` * Return` |
|      - | 5549 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 5550 | ` * in mask.` |
|      - | 5551 | ` */` |
|     26 | 5552 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5553 |  |
|      - | 5554 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5555 | `	int iMasklen,iLen;` |
|      - | 5556 | `	SyString sToken;` |
|     27 | 5557 | `	int iCount = 0;` |
|      - | 5558 | `	int rc;` |
|     27 | 5559 | `	if( nArg < 2 ){` |
|      - | 5560 | `		/* Missing agruments,return zero */` |
|      3 | 5561 | `		ph7_result_int(pCtx,0);` |
|      3 | 5562 | `		return PH7_OK;` |
|      - | 5563 | `	}` |
|      - | 5564 | `	/* Extract the target string */` |
|     25 | 5565 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5566 | `	/* Extract the mask */` |
|     25 | 5567 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 5568 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 5569 | `		/* Nothing to process,return zero */` |
|      7 | 5570 | `		ph7_result_int(pCtx,0);` |
|      7 | 5571 | `		return PH7_OK;` |
|      - | 5572 | `	}` |
|     19 | 5573 | `	if( nArg > 2 ){` |
|      - | 5574 | `		int nOfft;` |
|      - | 5575 | `		/* Extract the offset */` |
|      9 | 5576 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 5577 | `		if( nOfft < 0 ){` |
|    ! 0 | 5578 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5579 | `			if( zBase > zString ){` |
|    ! 0 | 5580 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5581 | `				zString = zBase;` |
|    ! 0 | 5582 | `			}else{` |
|      - | 5583 | `				/* Invalid offset */` |
|    ! 0 | 5584 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5585 | `				return PH7_OK;` |
|      - | 5586 | `			}` |
|    ! 0 | 5587 | `		}else{` |
|      9 | 5588 | `			if( nOfft >= iLen ){` |
|      - | 5589 | `				/* Invalid offset */` |
|    ! 0 | 5590 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5591 | `				return PH7_OK;` |
|    ! 0 | 5592 | `			}else{` |
|      - | 5593 | `				/* Update offset */` |
|      9 | 5594 | `				zString += nOfft;` |
|      9 | 5595 | `				iLen -= nOfft;` |
|      - | 5596 | `			}` |
|      - | 5597 | `		}` |
|      9 | 5598 | `		if( nArg > 3 ){` |
|      - | 5599 | `			int iUserlen;` |
|      - | 5600 | `			/* Extract the desired length */` |
|      9 | 5601 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 5602 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 5603 | `				iLen = iUserlen;` |
|      2 | 5604 | `			}` |
|      4 | 5605 | `		}` |
|      4 | 5606 | `	}` |
|      - | 5607 | `	/* Point to the end of the string */` |
|     19 | 5608 | `	zEnd = &zString[iLen];` |
|      - | 5609 | `	/* Extract the first non-space token */` |
|     19 | 5610 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 5611 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5612 | `		/* Compare against the current mask */` |
|     19 | 5613 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 5614 | `	}` |
|      - | 5615 | `	/* Longest match */` |
|     19 | 5616 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 5617 | `	return PH7_OK;` |
|     14 | 5618 |  |
|      - | 5619 | `/*` |
|      - | 5620 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5621 | ` *  Find length of initial segment not matching mask.` |
|      - | 5622 | ` * Parameters` |
|      - | 5623 | ` * $str` |
|      - | 5624 | ` *  The input string.` |
|      - | 5625 | ` * $mask` |
|      - | 5626 | ` *  The list of not allowed characters.` |
|      - | 5627 | ` * $start` |
|      - | 5628 | ` *  The position in subject to start searching.` |
|      - | 5629 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5630 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5631 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5632 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5633 | ` *  start'th position from the end of subject.` |
|      - | 5634 | ` * $length` |
|      - | 5635 | ` *  The length of the segment from subject to examine.` |
|      - | 5636 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5637 | ` *  characters after the starting position.` |
|      - | 5638 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5639 | ` *  position up to length characters from the end of subject.` |
|      - | 5640 | ` * Return` |
|      - | 5641 | ` *  Returns the length of the segment as an integer.` |
|      - | 5642 | ` */` |
|     16 | 5643 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5644 |  |
|      - | 5645 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5646 | `	int iMasklen,iLen;` |
|      - | 5647 | `	SyString sToken;` |
|     17 | 5648 | `	int iCount = 0;` |
|      - | 5649 | `	int rc;` |
|     17 | 5650 | `	if( nArg < 2 ){` |
|      - | 5651 | `		/* Missing agruments,return zero */` |
|      3 | 5652 | `		ph7_result_int(pCtx,0);` |
|      3 | 5653 | `		return PH7_OK;` |
|      - | 5654 | `	}` |
|      - | 5655 | `	/* Extract the target string */` |
|     15 | 5656 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5657 | `	/* Extract the mask */` |
|     15 | 5658 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 5659 | `	if( iLen < 1 ){` |
|      - | 5660 | `		/* Nothing to process,return zero */` |
|    ! 0 | 5661 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5662 | `		return PH7_OK;` |
|      - | 5663 | `	}` |
|     15 | 5664 | `	if( iMasklen < 1 ){` |
|      - | 5665 | `		/* No given mask,return the string length */` |
|      3 | 5666 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 5667 | `		return PH7_OK;` |
|      - | 5668 | `	}` |
|     13 | 5669 | `	if( nArg > 2 ){` |
|      - | 5670 | `		int nOfft;` |
|      - | 5671 | `		/* Extract the offset */` |
|     11 | 5672 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 5673 | `		if( nOfft < 0 ){` |
|    ! 0 | 5674 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5675 | `			if( zBase > zString ){` |
|    ! 0 | 5676 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5677 | `				zString = zBase;` |
|    ! 0 | 5678 | `			}else{` |
|      - | 5679 | `				/* Invalid offset */` |
|    ! 0 | 5680 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5681 | `				return PH7_OK;` |
|      - | 5682 | `			}` |
|    ! 0 | 5683 | `		}else{` |
|     11 | 5684 | `			if( nOfft >= iLen ){` |
|      - | 5685 | `				/* Invalid offset */` |
|      3 | 5686 | `				ph7_result_int(pCtx,0);` |
|      3 | 5687 | `				return PH7_OK;` |
|    ! 0 | 5688 | `			}else{` |
|      - | 5689 | `				/* Update offset */` |
|      9 | 5690 | `				zString += nOfft;` |
|      9 | 5691 | `				iLen -= nOfft;` |
|      - | 5692 | `			}` |
|      - | 5693 | `		}` |
|      9 | 5694 | `		if( nArg > 3 ){` |
|      - | 5695 | `			int iUserlen;` |
|      - | 5696 | `			/* Extract the desired length */` |
|    ! 0 | 5697 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 5698 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 5699 | `				iLen = iUserlen;` |
|    ! 0 | 5700 | `			}` |
|    ! 0 | 5701 | `		}` |
|      4 | 5702 | `	}` |
|      - | 5703 | `	/* Point to the end of the string */` |
|     11 | 5704 | `	zEnd = &zString[iLen];` |
|      - | 5705 | `	/* Extract the first non-space token */` |
|     11 | 5706 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 5707 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5708 | `		/* Compare against the current mask */` |
|     11 | 5709 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 5710 | `	}` |
|      - | 5711 | `	/* Longest match */` |
|     11 | 5712 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 5713 | `	return PH7_OK;` |
|      9 | 5714 |  |
|      - | 5715 | `/*` |
|      - | 5716 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 5717 | ` *  Search a string for any of a set of characters.` |
|      - | 5718 | ` * Parameters` |
|      - | 5719 | ` *  $haystack` |
|      - | 5720 | ` *   The string where char_list is looked for.` |
|      - | 5721 | ` *  $char_list` |
|      - | 5722 | ` *   This parameter is case sensitive.` |
|      - | 5723 | ` * Return` |
|      - | 5724 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 5725 | ` */` |
|      6 | 5726 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5727 |  |
|      - | 5728 | `	const char *zString,*zList,*zEnd;` |
|      - | 5729 | `	int iLen,iListLen,i,c;` |
|      - | 5730 | `	sxu32 nOfft,nMax;` |
|      - | 5731 | `	sxi32 rc;` |
|      7 | 5732 | `	if( nArg < 2 ){` |
|      - | 5733 | `		/* Missing arguments,return FALSE */` |
|      3 | 5734 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5735 | `		return PH7_OK;` |
|      - | 5736 | `	}` |
|      - | 5737 | `	/* Extract the haystack and the char list */` |
|      5 | 5738 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 5739 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 5740 | `	if( iLen < 1 ){` |
|      - | 5741 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 5742 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5743 | `		return PH7_OK;` |
|      - | 5744 | `	}` |
|      - | 5745 | `	/* Point to the end of the string */` |
|      5 | 5746 | `	zEnd = &zString[iLen];` |
|      5 | 5747 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 5748 | `	/* perform the requested operation */` |
|     15 | 5749 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 5750 | `		c = zList[i];` |
|     11 | 5751 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 5752 | `		if( rc == SXRET_OK ){` |
|      5 | 5753 | `			if( nMax < nOfft ){` |
|      3 | 5754 | `				nOfft = nMax;` |
|      1 | 5755 | `			}` |
|      2 | 5756 | `		}` |
|      6 | 5757 | `	}` |
|      5 | 5758 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 5759 | `		/* No such substring,return FALSE */` |
|      3 | 5760 | `		ph7_result_bool(pCtx,0);` |
|      2 | 5761 | `	}else{` |
|      - | 5762 | `		/* Return the substring */` |
|      3 | 5763 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 5764 | `	}` |
|      5 | 5765 | `	return PH7_OK;` |
|      4 | 5766 |  |
|      - | 5767 | `/*` |
|      - | 5768 | ` * string soundex(string $str)` |
|      - | 5769 | ` *  Calculate the soundex key of a string.` |
|      - | 5770 | ` * Parameters` |
|      - | 5771 | ` *  $str` |
|      - | 5772 | ` *   The input string.` |
|      - | 5773 | ` * Return` |
|      - | 5774 | ` *  Returns the soundex key as a string.` |
|      - | 5775 | ` * Note:` |
|      - | 5776 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 5777 | ` * source tree.` |
|      - | 5778 | ` */` |
|     20 | 5779 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5780 |  |
|      - | 5781 | `	const unsigned char *zIn;` |
|      - | 5782 | `	char zResult[8];` |
|      - | 5783 | `	int i, j;` |
|      - | 5784 | `	static const unsigned char iCode[] = {` |
|      - | 5785 |  |
|      - | 5786 |  |
|      - | 5787 |  |
|      - | 5788 |  |
|      - | 5789 |  |
|      - | 5790 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5791 |  |
|      - | 5792 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5793 | `	};` |
|     21 | 5794 | `	if( nArg < 1 ){` |
|      - | 5795 | `		/* Missing arguments,return the empty string */` |
|      3 | 5796 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5797 | `		return PH7_OK;` |
|      - | 5798 | `	}` |
|     19 | 5799 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     19 | 5800 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     19 | 5801 | `	if( zIn[i] ){` |
|     17 | 5802 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 5803 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 5804 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 5805 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 5806 | `			if( code>0 ){` |
|     45 | 5807 | `				if( code!=prevcode ){` |
|     33 | 5808 | `					prevcode = (unsigned char)code;` |
|     33 | 5809 | `					zResult[j++] = (char)code + '0';` |
|     16 | 5810 | `				}` |
|     23 | 5811 | `			}else{` |
|     49 | 5812 | `				prevcode = 0;` |
|      - | 5813 | `			}` |
|     47 | 5814 | `		}` |
|     33 | 5815 | `		while( j<4 ){` |
|     17 | 5816 | `			zResult[j++] = '0';` |
|      1 | 5817 | `		}` |
|     17 | 5818 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 5819 | `	}else{` |
|      3 | 5820 | `	  ph7_result_string(pCtx,"?000",4);` |
|      - | 5821 | `	}` |
|     19 | 5822 | `	return PH7_OK;` |
|     11 | 5823 |  |
|      - | 5824 | `/*` |
|      - | 5825 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 5826 | ` *  Wraps a string to a given number of characters.` |
|      - | 5827 | ` * Parameters` |
|      - | 5828 | ` *  $str` |
|      - | 5829 | ` *   The input string.` |
|      - | 5830 | ` * $width` |
|      - | 5831 | ` *  The column width.` |
|      - | 5832 | ` * $break` |
|      - | 5833 | ` *  The line is broken using the optional break parameter.` |
|      - | 5834 | ` * Return` |
|      - | 5835 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 5836 | ` */` |
|     14 | 5837 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5838 |  |
|      - | 5839 | `	const char *zIn,*zEnd,*zBreak;` |
|      - | 5840 | `	int iLen,iBreaklen,iChunk;` |
|     15 | 5841 | `	if( nArg < 1 ){` |
|      - | 5842 | `		/* Missing arguments,return the empty string */` |
|      3 | 5843 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5844 | `		return PH7_OK;` |
|      - | 5845 | `	}` |
|      - | 5846 | `	/* Extract the input string */` |
|     13 | 5847 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|     13 | 5848 | `	if( iLen < 1 ){` |
|      - | 5849 | `		/* Nothing to process,return the empty string */` |
|      3 | 5850 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5851 | `		return PH7_OK;` |
|      - | 5852 | `	}` |
|      - | 5853 | `	/* Chunk length */` |
|     11 | 5854 | `	iChunk = 75;` |
|     11 | 5855 | `	iBreaklen = 0;` |
|     11 | 5856 | `	zBreak = ""; /* cc warning */` |
|     11 | 5857 | `	if( nArg > 1 ){` |
|     11 | 5858 | `		iChunk = ph7_value_to_int(apArg[1]);` |
|     11 | 5859 | `		if( iChunk < 1 ){` |
|    ! 0 | 5860 | `			iChunk = 75;` |
|    ! 0 | 5861 | `		}` |
|     11 | 5862 | `		if( nArg > 2 ){` |
|      3 | 5863 | `			zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      1 | 5864 | `		}` |
|      5 | 5865 | `	}` |
|     11 | 5866 | `	if( iBreaklen < 1 ){` |
|      - | 5867 | `		/* Set a default column break */` |
|      - | 5868 | `#ifdef __WINNT__` |
|      1 | 5869 | `		zBreak = "\r\n";` |
|      1 | 5870 | `		iBreaklen = (int)sizeof("\r\n")-1;` |
|      - | 5871 | `#else` |
|      8 | 5872 | `		zBreak = "\n";` |
|      8 | 5873 | `		iBreaklen = (int)sizeof(char);` |
|      - | 5874 | `#endif` |
|      4 | 5875 | `	}` |
|      - | 5876 | `	/* Perform the requested operation */` |
|     11 | 5877 | `	zEnd = &zIn[iLen];` |
|     41 | 5878 | `	for(;;){` |
|      - | 5879 | `		int nMax;` |
|     47 | 5880 | `		if( zIn >= zEnd ){` |
|      - | 5881 | `			/* No more input to process */` |
|     11 | 5882 | `			break;` |
|      - | 5883 | `		}` |
|     37 | 5884 | `		nMax = (int)(zEnd-zIn);` |
|     37 | 5885 | `		if( iChunk > nMax ){` |
|     11 | 5886 | `			iChunk = nMax;` |
|      5 | 5887 | `		}` |
|      - | 5888 | `		/* Append the column first */` |
|     37 | 5889 | `		ph7_result_string(pCtx,zIn,iChunk); /* Will make it's own copy */` |
|      - | 5890 | `		/* Advance the cursor */` |
|     37 | 5891 | `		zIn += iChunk;` |
|     37 | 5892 | `		if( zIn < zEnd ){` |
|      - | 5893 | `			/* Append the line break */` |
|     27 | 5894 | `			ph7_result_string(pCtx,zBreak,iBreaklen);` |
|     13 | 5895 | `		}` |
|      1 | 5896 | `	}` |
|     11 | 5897 | `	return PH7_OK;` |
|      8 | 5898 |  |
|      - | 5899 | `/*` |
|      - | 5900 | ` * Check if the given character is a member of the given mask.` |
|      - | 5901 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 5902 | ` * Refer to [strtok()].` |
|      - | 5903 | ` */` |
|     30 | 5904 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 5905 |  |
|      - | 5906 | `	int i;` |
|     57 | 5907 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 5908 | `		if( c == zMask[i] ){` |
|     13 | 5909 | `			if( pOfft ){` |
|      5 | 5910 | `				*pOfft = i;` |
|      2 | 5911 | `			}` |
|     13 | 5912 | `			return TRUE;` |
|      - | 5913 | `		}` |
|     14 | 5914 | `	}` |
|     19 | 5915 | `	return FALSE;` |
|     16 | 5916 |  |
|      - | 5917 | `/*` |
|      - | 5918 | ` * Extract a single token from the input stream.` |
|      - | 5919 | ` * Refer to [strtok()].` |
|      - | 5920 | ` */` |
|      6 | 5921 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 5922 |  |
|      7 | 5923 | `	const char *zIn = *pzIn;` |
|      - | 5924 | `	const char *zPtr;` |
|      - | 5925 | `	/* Ignore leading delimiter */` |
|     11 | 5926 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5927 | `		zIn++;` |
|      1 | 5928 | `	}` |
|      7 | 5929 | `	if( zIn >= zEnd ){` |
|      - | 5930 | `		/* End of input */` |
|    ! 0 | 5931 | `		return SXERR_EOF;` |
|      - | 5932 | `	}` |
|      7 | 5933 | `	zPtr = zIn;` |
|      - | 5934 | `	/* Extract the token */` |
|     13 | 5935 | `	while( zIn < zEnd ){` |
|     11 | 5936 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 5937 | `			/* UTF-8 stream */` |
|    ! 0 | 5938 | `			zIn++;` |
|    ! 0 | 5939 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 5940 | `		}else{` |
|     11 | 5941 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5942 | `				break;` |
|      - | 5943 | `			}` |
|      7 | 5944 | `			zIn++;` |
|      - | 5945 | `		}` |
|      1 | 5946 | `	}` |
|      7 | 5947 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5948 | `	/* Update the cursor */` |
|      7 | 5949 | `	*pzIn = zIn;` |
|      - | 5950 | `	/* Return to the caller */` |
|      7 | 5951 | `	return SXRET_OK;` |
|      4 | 5952 |  |
|      - | 5953 | `/* strtok auxiliary private data */` |
|      - | 5954 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 5955 | `struct strtok_aux_data` |
|      - | 5956 |  |
|      - | 5957 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 5958 | `	const char *zIn;   /* Current input stream */` |
|      - | 5959 | `	const char *zEnd;  /* End of input */` |
|      - | 5960 | `};` |
|      - | 5961 | `/*` |
|      - | 5962 | ` * string strtok(string $str,string $token)` |
|      - | 5963 | ` * string strtok(string $token)` |
|      - | 5964 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 5965 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 5966 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 5967 | ` *  words by using the space character as the token.` |
|      - | 5968 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 5969 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 5970 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 5971 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 5972 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 5973 | ` *  the argument are found.` |
|      - | 5974 | ` * Parameters` |
|      - | 5975 | ` *  $str` |
|      - | 5976 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 5977 | ` * $token` |
|      - | 5978 | ` *  The delimiter used when splitting up str.` |
|      - | 5979 | ` * Return` |
|      - | 5980 | ` *   Current token or FALSE on EOF.` |
|      - | 5981 | ` */` |
|      8 | 5982 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5983 |  |
|      - | 5984 | `	strtok_aux_data *pAux;` |
|      - | 5985 | `	const char *zMask;` |
|      - | 5986 | `	SyString sToken;` |
|      - | 5987 | `	int nMasklen;` |
|      - | 5988 | `	sxi32 rc;` |
|      9 | 5989 | `	if( nArg < 2 ){` |
|      - | 5990 | `		/* Extract top aux data */` |
|      7 | 5991 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      7 | 5992 | `		if( pAux == 0 ){` |
|      - | 5993 | `			/* No aux data,return FALSE */` |
|    ! 0 | 5994 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5995 | `			return PH7_OK;` |
|      - | 5996 | `		}` |
|      7 | 5997 | `		nMasklen = 0;` |
|      7 | 5998 | `		zMask = ""; /* cc warning */` |
|      7 | 5999 | `		if( nArg > 0 ){` |
|      - | 6000 | `			/* Extract the mask */` |
|      5 | 6001 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 6002 | `		}` |
|      7 | 6003 | `		if( nMasklen < 1 ){` |
|      - | 6004 | `			/* Invalid mask,return FALSE */` |
|      3 | 6005 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|      3 | 6006 | `			ph7_context_free_chunk(pCtx,pAux);` |
|      3 | 6007 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|      3 | 6008 | `			ph7_result_bool(pCtx,0);` |
|      3 | 6009 | `			return PH7_OK;` |
|      - | 6010 | `		}` |
|      - | 6011 | `		/* Extract the token */` |
|      5 | 6012 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 6013 | `		if( rc != SXRET_OK ){` |
|      - | 6014 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 6015 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 6016 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6017 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 6018 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6019 | `		}else{` |
|      - | 6020 | `			/* Return the extracted token */` |
|      5 | 6021 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6022 | `		}` |
|      3 | 6023 | `	}else{` |
|      - | 6024 | `		const char *zInput,*zCur;` |
|      - | 6025 | `		char *zDup;` |
|      - | 6026 | `		int nLen;` |
|      - | 6027 | `		/* Extract the raw input */` |
|      3 | 6028 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 6029 | `		if( nLen < 1 ){` |
|      - | 6030 | `			/* Empty input,return FALSE */` |
|    ! 0 | 6031 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6032 | `			return PH7_OK;` |
|      - | 6033 | `		}` |
|      - | 6034 | `		/* Extract the mask */` |
|      3 | 6035 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 6036 | `		if( nMasklen < 1 ){` |
|      - | 6037 | `			/* Set a default mask */` |
|      - | 6038 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 6039 | `			zMask = TOK_MASK;` |
|    ! 0 | 6040 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 6041 | `#undef TOK_MASK` |
|    ! 0 | 6042 | `		}` |
|      - | 6043 | `		/* Extract a single token */` |
|      3 | 6044 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 6045 | `		if( rc != SXRET_OK ){` |
|      - | 6046 | `			/* Empty input */` |
|    ! 0 | 6047 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6048 | `			return PH7_OK;` |
|    ! 0 | 6049 | `		}else{` |
|      - | 6050 | `			/* Return the extracted token */` |
|      3 | 6051 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6052 | `		}` |
|      - | 6053 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 6054 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 6055 | `		if( pAux ){` |
|      3 | 6056 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 6057 | `			if( nLen < 1 ){` |
|    ! 0 | 6058 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6059 | `				return PH7_OK;` |
|      - | 6060 | `			}` |
|      - | 6061 | `			/* Duplicate input */` |
|      3 | 6062 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 6063 | `			if( zDup  ){` |
|      3 | 6064 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 6065 | `				/* Register the aux data */` |
|      3 | 6066 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 6067 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 6068 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 6069 | `			}` |
|      1 | 6070 | `		}` |
|      - | 6071 | `	}` |
|      7 | 6072 | `	return PH7_OK;` |
|      5 | 6073 |  |
|      - | 6074 | `/*` |
|      - | 6075 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 6076 | ` *  Pad a string to a certain length with another string` |
|      - | 6077 | ` * Parameters` |
|      - | 6078 | ` *  $input` |
|      - | 6079 | ` *   The input string.` |
|      - | 6080 | ` * $pad_length` |
|      - | 6081 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 6082 | ` *   string, no padding takes place.` |
|      - | 6083 | ` * $pad_string` |
|      - | 6084 | ` *   Note:` |
|      - | 6085 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 6086 | ` *    divided by the pad_string's length.` |
|      - | 6087 | ` * $pad_type` |
|      - | 6088 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 6089 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 6090 | ` * Return` |
|      - | 6091 | ` *  The padded string.` |
|      - | 6092 | ` */` |
|     10 | 6093 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6094 |  |
|      - | 6095 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 6096 | `	const char *zIn,*zPad;` |
|     11 | 6097 | `	if( nArg < 2 ){` |
|      - | 6098 | `		/* Missing arguments,return the empty string */` |
|      5 | 6099 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 6100 | `		return PH7_OK;` |
|      - | 6101 | `	}` |
|      - | 6102 | `	/* Extract the target string */` |
|      7 | 6103 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6104 | `	/* Padding length */` |
|      7 | 6105 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|      7 | 6106 | `	if( iPadlen > 0 ){` |
|      5 | 6107 | `		iPadlen -= iLen;` |
|      2 | 6108 | `	}` |
|      7 | 6109 | `	if( iPadlen < 1  ){` |
|      - | 6110 | `		/* Return the string verbatim */` |
|      3 | 6111 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      3 | 6112 | `		return PH7_OK;` |
|      - | 6113 | `	}` |
|      5 | 6114 | `	zPad = " "; /* Whitespace padding */` |
|      5 | 6115 | `	iStrpad = (int)sizeof(char);` |
|      5 | 6116 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      5 | 6117 | `	if( nArg > 2 ){` |
|      - | 6118 | `		/* Padding string */` |
|      5 | 6119 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      5 | 6120 | `		if( iStrpad < 1 ){` |
|      - | 6121 | `			/* Empty string */` |
|    ! 0 | 6122 | `			zPad = " "; /* Whitespace padding */` |
|    ! 0 | 6123 | `			iStrpad = (int)sizeof(char);` |
|    ! 0 | 6124 | `		}` |
|      5 | 6125 | `		if( nArg > 3 ){` |
|      - | 6126 | `			/* Padd type */` |
|      5 | 6127 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 6128 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 6129 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 6130 | `			}` |
|      2 | 6131 | `		}` |
|      2 | 6132 | `	}` |
|      5 | 6133 | `	iDiv = 1;` |
|      5 | 6134 | `	if( iType == 2 ){` |
|    ! 0 | 6135 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 6136 | `	}` |
|      - | 6137 | `	/* Perform the requested operation */` |
|      5 | 6138 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 6139 | `		jPad = iStrpad;` |
|      5 | 6140 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 6141 | `			/* Padding */` |
|      5 | 6142 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 6143 | `				break;` |
|      - | 6144 | `			}` |
|      3 | 6145 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      2 | 6146 | `		}` |
|      3 | 6147 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 6148 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 6149 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 6150 | `				if( jPad > iStrpad ){` |
|    ! 0 | 6151 | `					jPad = iStrpad;` |
|    ! 0 | 6152 | `				}` |
|      3 | 6153 | `				if( jPad < 1){` |
|    ! 0 | 6154 | `					break;` |
|      - | 6155 | `				}` |
|      3 | 6156 | `				ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 6157 | `			}` |
|      1 | 6158 | `		}` |
|      1 | 6159 | `	}` |
|      5 | 6160 | `	if( iLen > 0 ){` |
|      - | 6161 | `		/* Append the input string */` |
|      5 | 6162 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      2 | 6163 | `	}` |
|      5 | 6164 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 6165 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 6166 | `			/* Padding */` |
|      5 | 6167 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 6168 | `				break;` |
|      - | 6169 | `			}` |
|      3 | 6170 | `			ph7_result_string(pCtx,zPad,iStrpad);` |
|      2 | 6171 | `		}` |
|      5 | 6172 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 6173 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 6174 | `			if( jPad > iStrpad ){` |
|    ! 0 | 6175 | `				jPad = iStrpad;` |
|    ! 0 | 6176 | `			}` |
|      3 | 6177 | `			if( jPad < 1){` |
|    ! 0 | 6178 | `				break;` |
|      - | 6179 | `			}` |
|      3 | 6180 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 6181 | `		}` |
|      1 | 6182 | `	}` |
|      5 | 6183 | `	return PH7_OK;` |
|      6 | 6184 |  |
|      - | 6185 | `/*` |
|      - | 6186 | ` * String replacement private data.` |
|      - | 6187 | ` */` |
|      - | 6188 | `typedef struct str_replace_data str_replace_data;` |
|      - | 6189 | `struct str_replace_data` |
|      - | 6190 |  |
|      - | 6191 | `	/* The following two fields are only used by the strtr function */` |
|      - | 6192 | `	SyBlob *pWorker;         /* Working buffer */` |
|      - | 6193 | `	ProcStringMatch xMatch;  /* Pattern match routine */` |
|      - | 6194 | `	/* The following two fields are only used by the str_replace function */` |
|      - | 6195 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 6196 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 6197 | `};` |
|      - | 6198 | `/*` |
|      - | 6199 | ` * Remove a substring.` |
|      - | 6200 | ` */` |
|      - | 6201 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 6202 | `	for(;;){\` |
|      - | 6203 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 6204 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 6205 | `		++OFFT;\` |
|      - | 6206 | `	}\` |
|      - | 6207 |  |
|      - | 6208 | `/*` |
|      - | 6209 | ` * Shift right and insert algorithm.` |
|      - | 6210 | ` */` |
|      - | 6211 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 6212 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 6213 | `		for(;;){\` |
|      - | 6214 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 6215 | `			if(INLEN < 1 ) { break; }\` |
|      - | 6216 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 6217 | `			--INLEN; \` |
|      - | 6218 | `		}\` |
|      - | 6219 | `		for(;;){\` |
|      - | 6220 | `				if(ELEN < 1) { break; }\` |
|      - | 6221 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 6222 | `				OFFT++;\` |
|      - | 6223 | `				ENTRY++;\` |
|      - | 6224 | `				--ELEN;\` |
|      - | 6225 | `		}\` |
|      - | 6226 |  |
|      - | 6227 | `/*` |
|      - | 6228 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 6229 | ` * replacement string [i.e: zReplace].` |
|      - | 6230 | ` */` |
|     38 | 6231 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 6232 |  |
|     39 | 6233 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 6234 | `	sxu32 n,m;` |
|     39 | 6235 | `	n = SyBlobLength(pWorker);` |
|     39 | 6236 | `	m = nOfft;` |
|      - | 6237 | `	/* Delete the old entry */` |
|    475 | 6238 | `	STRDEL(zInput,n,m,nLen);` |
|     39 | 6239 | `	SyBlobLength(pWorker) -= nLen;` |
|     39 | 6240 | `	if( nReplen > 0 ){` |
|     33 | 6241 | `		sxi32 iRep = nReplen;` |
|      - | 6242 | `		sxi32 rc;` |
|      - | 6243 | `		/*` |
|      - | 6244 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 6245 | `		 * string.` |
|      - | 6246 | `		 */` |
|     33 | 6247 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     33 | 6248 | `		if( rc != SXRET_OK ){` |
|      - | 6249 | `			/* Simply ignore any memory failure problem */` |
|    ! 0 | 6250 | `			return SXRET_OK;` |
|      - | 6251 | `		}` |
|      - | 6252 | `		/* Perform the insertion now */` |
|     33 | 6253 | `		zInput = (char *)SyBlobData(pWorker);` |
|     33 | 6254 | `		n = SyBlobLength(pWorker);` |
|    163 | 6255 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     33 | 6256 | `		SyBlobLength(pWorker) += nReplen;` |
|     16 | 6257 | `	}` |
|     39 | 6258 | `	return SXRET_OK;` |
|     20 | 6259 |  |
|      - | 6260 | `/*` |
|      - | 6261 | ` * String replacement walker callback.` |
|      - | 6262 | ` * The following callback is invoked for each array entry that hold` |
|      - | 6263 | ` * the replace string.` |
|      - | 6264 | ` * Refer to the strtr() implementation for more information.` |
|      - | 6265 | ` */` |
|      8 | 6266 | `static int StringReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6267 |  |
|      9 | 6268 | `	str_replace_data *pRepData = (str_replace_data *)pUserData;` |
|      - | 6269 | `	const char *zTarget,*zReplace;` |
|      - | 6270 | `	SyBlob *pWorker;` |
|      - | 6271 | `	int tLen,nLen;` |
|      - | 6272 | `	sxu32 nOfft;` |
|      - | 6273 | `	sxi32 rc;` |
|      - | 6274 | `	/* Point to the working buffer */` |
|      9 | 6275 | `	pWorker = pRepData->pWorker;` |
|      9 | 6276 | `	if( !ph7_value_is_string(pKey) ){` |
|      - | 6277 | `		/* Target and replace must be a string */` |
|      3 | 6278 | `		return PH7_OK;` |
|      - | 6279 | `	}` |
|      - | 6280 | `	/* Extract the target and the replace */` |
|      7 | 6281 | `	zTarget = ph7_value_to_string(pKey,&tLen);` |
|      7 | 6282 | `	if( tLen < 1 ){` |
|      - | 6283 | `		/* Empty target,return immediately */` |
|    ! 0 | 6284 | `		return PH7_OK;` |
|      - | 6285 | `	}` |
|      - | 6286 | `	/* Perform a pattern search */` |
|      7 | 6287 | `	rc = pRepData->xMatch(SyBlobData(pWorker),SyBlobLength(pWorker),(const void *)zTarget,(sxu32)tLen,&nOfft);` |
|      7 | 6288 | `	if( rc != SXRET_OK ){` |
|      - | 6289 | `		/* Pattern not found */` |
|    ! 0 | 6290 | `		return PH7_OK;` |
|      - | 6291 | `	}` |
|      - | 6292 | `	/* Extract the replace string */` |
|      7 | 6293 | `	zReplace = ph7_value_to_string(pData,&nLen);` |
|      - | 6294 | `	/* Perform the replace process */` |
|      7 | 6295 | `	StringReplace(pWorker,nOfft,tLen,zReplace,nLen);` |
|      - | 6296 | `	/* All done */` |
|      7 | 6297 | `	return PH7_OK;` |
|      5 | 6298 |  |
|      - | 6299 | `/*` |
|      - | 6300 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 6301 | ` * to collect search/replace string.` |
|      - | 6302 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 6303 | ` */` |
|     26 | 6304 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6305 |  |
|     27 | 6306 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 6307 | `	SyString sWorker;` |
|      - | 6308 | `	const char *zIn;` |
|      - | 6309 | `	int nByte;` |
|      - | 6310 | `	/* Extract a string representation of the given argument */` |
|     27 | 6311 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 6312 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 6313 | `	if( nByte > 0 ){` |
|      - | 6314 | `		char *zDup;` |
|      - | 6315 | `		/* Duplicate the chunk */` |
|     25 | 6316 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 6317 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 6318 | `			);` |
|     25 | 6319 | `		if( zDup == 0 ){` |
|      - | 6320 | `			/* Ignore any memory failure problem */` |
|    ! 0 | 6321 | `			ph7_context_throw_error(pRep->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 6322 | `			return PH7_OK;` |
|      - | 6323 | `		}` |
|     25 | 6324 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 6325 | `		/* Save the chunk */` |
|     25 | 6326 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 6327 | `	}` |
|      - | 6328 | `	/* Save for later processing */` |
|     27 | 6329 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 6330 | `	/* All done */` |
|     13 | 6331 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 6332 | `	return PH7_OK;` |
|     14 | 6333 |  |
|      - | 6334 | `/*` |
|      - | 6335 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6336 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6337 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 6338 | ` * Parameters` |
|      - | 6339 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 6340 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 6341 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 6342 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 6343 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 6344 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 6345 | ` * $search` |
|      - | 6346 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 6347 | ` *  to designate multiple needles.` |
|      - | 6348 | ` * $replace` |
|      - | 6349 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 6350 | ` *  to designate multiple replacements.` |
|      - | 6351 | ` * $subject` |
|      - | 6352 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 6353 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 6354 | ` *  of subject, and the return value is an array as well.` |
|      - | 6355 | ` * $count (Not used)` |
|      - | 6356 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 6357 | ` * Return` |
|      - | 6358 | ` * This function returns a string or an array with the replaced values.` |
|      - | 6359 | ` */` |
|  14242 | 6360 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6361 |  |
|      - | 6362 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 6363 | `	ProcStringMatch xMatch;` |
|      - | 6364 | `	const char *zIn,*zFunc;` |
|      - | 6365 | `	str_replace_data sRep;` |
|      - | 6366 | `	SyBlob sWorker;` |
|      - | 6367 | `	SySet sReplace;` |
|      - | 6368 | `	SySet sSearch;` |
|      - | 6369 | `	int rep_str;` |
|      - | 6370 | `	int nByte;` |
|      - | 6371 | `	sxi32 rc;` |
|  14244 | 6372 | `	if( nArg < 3 ){` |
|      - | 6373 | `		/* Missing/Invalid arguments,return null */` |
|      7 | 6374 | `		ph7_result_null(pCtx);` |
|      7 | 6375 | `		return PH7_OK;` |
|      - | 6376 | `	}` |
|      - | 6377 | `	/* Initialize fields */` |
|  14238 | 6378 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  14238 | 6379 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  14238 | 6380 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  14238 | 6381 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  14238 | 6382 | `	sRep.pCtx = pCtx;` |
|  14238 | 6383 | `	sRep.pCollector = &sSearch;` |
|  14238 | 6384 | `	rep_str = 0;` |
|      - | 6385 | `	/* Extract the subject */` |
|  14238 | 6386 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  14238 | 6387 | `	if( nByte < 1 ){` |
|      - | 6388 | `		/* Nothing to replace,return the empty string */` |
|     38 | 6389 | `		ph7_result_string(pCtx,"",0);` |
|     38 | 6390 | `		return PH7_OK;` |
|      - | 6391 | `	}` |
|      - | 6392 | `	/* Copy the subject */` |
|  14202 | 6393 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 6394 | `	/* Search string */` |
|  14202 | 6395 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 6396 | `		/* Collect search string */` |
|      9 | 6397 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 6398 | `	}else{` |
|      - | 6399 | `		/* Single pattern */` |
|  14194 | 6400 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  14194 | 6401 | `		if( nByte < 1 ){` |
|      - | 6402 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 6403 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 6404 | `			return PH7_OK;` |
|      - | 6405 | `		}` |
|  14190 | 6406 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6407 | `		/* Save for later processing */` |
|  14190 | 6408 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 6409 | `	}` |
|      - | 6410 | `	/* Replace string */` |
|  14198 | 6411 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 6412 | `		/* Collect replace string */` |
|      7 | 6413 | `		sRep.pCollector = &sReplace;` |
|      7 | 6414 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 6415 | `	}else{` |
|      - | 6416 | `		/* Single needle */` |
|  14192 | 6417 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  14192 | 6418 | `		rep_str = 1;` |
|  14192 | 6419 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6420 | `		/* Save for later processing */` |
|  14192 | 6421 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 6422 | `	}` |
|      - | 6423 | `	/* Reset loop cursors */` |
|  14198 | 6424 | `	SySetResetCursor(&sSearch);` |
|  14198 | 6425 | `	SySetResetCursor(&sReplace);` |
|  14198 | 6426 | `	pReplace = pSearch = 0; /* cc warning */` |
|  14198 | 6427 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 6428 | `	/* Extract function name */` |
|  14198 | 6429 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 6430 | `	/* Set the default pattern match routine */` |
|  14198 | 6431 | `	xMatch = SyBlobSearch;` |
|  14198 | 6432 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 6433 | `		/* Case insensitive pattern match */` |
|     11 | 6434 | `		xMatch = iPatternMatch;` |
|      5 | 6435 | `	}` |
|      - | 6436 | `	/* Start the replace process */` |
|  28402 | 6437 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 6438 | `		sxu32 nCount,nOfft;` |
|  14206 | 6439 | `		if( pSearch->nByte <  1 ){` |
|      - | 6440 | `			/* Empty string,ignore */` |
|      3 | 6441 | `			continue;` |
|      - | 6442 | `		}` |
|      - | 6443 | `		/* Extract the replace string */` |
|  14204 | 6444 | `		if( rep_str ){` |
|  14194 | 6445 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|   7098 | 6446 | `		}else{` |
|     11 | 6447 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 6448 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 6449 | `				 * An empty string is used for the rest of replacement values` |
|      - | 6450 | `				 */` |
|      3 | 6451 | `				pReplace = 0;` |
|      1 | 6452 | `			}` |
|      - | 6453 | `		}` |
|  14204 | 6454 | `		if( pReplace == 0 ){` |
|      - | 6455 | `			/* Use an empty string instead */` |
|      3 | 6456 | `			pReplace = &sTemp;` |
|      1 | 6457 | `		}` |
|  14204 | 6458 | `		nOfft = nCount = 0;` |
|   7117 | 6459 | `		for(;;){` |
|  14236 | 6460 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 6461 | `				break;` |
|      - | 6462 | `			}` |
|      - | 6463 | `			/* Perform a pattern lookup */` |
|  21335 | 6464 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  14222 | 6465 | `				pSearch->nByte,&nOfft);` |
|  14224 | 6466 | `			if( rc != SXRET_OK ){` |
|      - | 6467 | `				/* Pattern not found */` |
|  14192 | 6468 | `				break;` |
|      - | 6469 | `			}` |
|      - | 6470 | `			/* Perform the replace operation */` |
|     33 | 6471 | `			StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|      - | 6472 | `			/* Increment offset counter */` |
|     33 | 6473 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 6474 | `		}` |
|      2 | 6475 | `	}` |
|      - | 6476 | `	/* All done,clean-up the mess left behind */` |
|  14198 | 6477 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  14198 | 6478 | `	SySetRelease(&sSearch);` |
|  14198 | 6479 | `	SySetRelease(&sReplace);` |
|  14198 | 6480 | `	SyBlobRelease(&sWorker);` |
|  14198 | 6481 | `	return PH7_OK;` |
|   7123 | 6482 |  |
|      - | 6483 | `/*` |
|      - | 6484 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 6485 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 6486 | ` *  Translate characters or replace substrings.` |
|      - | 6487 | ` * Parameters` |
|      - | 6488 | ` *  $str` |
|      - | 6489 | ` *  The string being translated.` |
|      - | 6490 | ` * $from` |
|      - | 6491 | ` *  The string being translated to to.` |
|      - | 6492 | ` * $to` |
|      - | 6493 | ` *  The string replacing from.` |
|      - | 6494 | ` * $replace_pairs` |
|      - | 6495 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 6496 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 6497 | ` * Return` |
|      - | 6498 | ` *  The translated string.` |
|      - | 6499 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 6500 | ` */` |
|     12 | 6501 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6502 |  |
|      - | 6503 | `	const char *zIn;` |
|      - | 6504 | `	int nLen;` |
|     13 | 6505 | `	if( nArg < 1 ){` |
|      - | 6506 | `		/* Nothing to replace,return FALSE */` |
|      7 | 6507 | `		ph7_result_bool(pCtx,0);` |
|      7 | 6508 | `		return PH7_OK;` |
|      - | 6509 | `	}` |
|      7 | 6510 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 6511 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 6512 | `		/* Invalid arguments */` |
|    ! 0 | 6513 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6514 | `		return PH7_OK;` |
|      - | 6515 | `	}` |
|      9 | 6516 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 6517 | `		str_replace_data sRepData;` |
|      - | 6518 | `		SyBlob sWorker;` |
|      - | 6519 | `		/* Initilaize the working buffer */` |
|      5 | 6520 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|      - | 6521 | `		/* Copy raw string */` |
|      5 | 6522 | `		SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nLen);` |
|      - | 6523 | `		/* Init our replace data instance */` |
|      5 | 6524 | `		sRepData.pWorker = &sWorker;` |
|      5 | 6525 | `		sRepData.xMatch = SyBlobSearch;` |
|      - | 6526 | `		/* Iterate throw array entries and perform the replace operation.*/` |
|      5 | 6527 | `		ph7_array_walk(apArg[1],StringReplaceWalker,&sRepData);` |
|      - | 6528 | `		/* All done, return the result string */` |
|      7 | 6529 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|      4 | 6530 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 6531 | `		/* Clean-up */` |
|      5 | 6532 | `		SyBlobRelease(&sWorker);` |
|      3 | 6533 | `	}else{` |
|      - | 6534 | `		int i,flen,tlen,c,iOfft;` |
|      - | 6535 | `		const char *zFrom,*zTo;` |
|      3 | 6536 | `		if( nArg < 3 ){` |
|      - | 6537 | `			/* Nothing to replace */` |
|    ! 0 | 6538 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6539 | `			return PH7_OK;` |
|      - | 6540 | `		}` |
|      - | 6541 | `		/* Extract given arguments */` |
|      3 | 6542 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 6543 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 6544 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 6545 | `			/* Nothing to replace */` |
|    ! 0 | 6546 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6547 | `			return PH7_OK;` |
|      - | 6548 | `		}` |
|      - | 6549 | `		/* Start the replace process */` |
|     13 | 6550 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 6551 | `			c = zIn[i];` |
|     11 | 6552 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 6553 | `				if ( iOfft < tlen ){` |
|      5 | 6554 | `					c = zTo[iOfft];` |
|      2 | 6555 | `				}` |
|      2 | 6556 | `			}` |
|     11 | 6557 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 6558 |  |
|      6 | 6559 | `		}` |
|      - | 6560 | `	}` |
|      7 | 6561 | `	return PH7_OK;` |
|      7 | 6562 |  |
|      - | 6563 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6564 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6565 | `/*` |
|      - | 6566 | ` * Parse an INI string.` |
|      - | 6567 |  |
|      - | 6568 | ` * According to wikipedia` |
|      - | 6569 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 6570 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 6571 | ` *  Format` |
|      - | 6572 | `*    Properties` |
|      - | 6573 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 6574 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 6575 | `*     Example:` |
|      - | 6576 | `*      name=value` |
|      - | 6577 | `*    Sections` |
|      - | 6578 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 6579 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 6580 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 6581 | `*     or the end of the file. Sections may not be nested.` |
|      - | 6582 | `*     Example:` |
|      - | 6583 | `*      [section]` |
|      - | 6584 | `*   Comments` |
|      - | 6585 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 6586 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 6587 | `*/` |
|     12 | 6588 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 6589 |  |
|      - | 6590 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 6591 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 6592 | `	SyHashEntry *pEntry;` |
|      - | 6593 | `	SyString sEntry;` |
|      - | 6594 | `	SyHash sHash;` |
|      - | 6595 | `	int c;` |
|      - | 6596 | `	/* Create an empty array and worker variables */` |
|     13 | 6597 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 6598 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 6599 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 6600 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 6601 | `		/* Out of memory */` |
|    ! 0 | 6602 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      - | 6603 | `		/* Return FALSE */` |
|    ! 0 | 6604 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6605 | `		return PH7_OK;` |
|      - | 6606 | `	}` |
|     13 | 6607 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 6608 | `	pCur = pArray;` |
|      - | 6609 | `	/* Start the parse process */` |
|     21 | 6610 | `	for(;;){` |
|      - | 6611 | `		/* Ignore leading white spaces */` |
|     69 | 6612 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 6613 | `			zIn++;` |
|      1 | 6614 | `		}` |
|     43 | 6615 | `		if( zIn >= zEnd ){` |
|      - | 6616 | `			/* No more input to process */` |
|     13 | 6617 | `			break;` |
|      - | 6618 | `		}` |
|     31 | 6619 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6620 | `			/* Comment til the end of line */` |
|    ! 0 | 6621 | `			zIn++;` |
|    ! 0 | 6622 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 6623 | `				zIn++;` |
|    ! 0 | 6624 | `			}` |
|    ! 0 | 6625 | `			continue;` |
|      - | 6626 | `		}` |
|      - | 6627 | `		/* Reset the string cursor of the working variable */` |
|     31 | 6628 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 6629 | `		if( zIn[0] == '[' ){` |
|      - | 6630 | `			/* Section: Extract the section name */` |
|      9 | 6631 | `			zIn++;` |
|      9 | 6632 | `			zCur = zIn;` |
|     73 | 6633 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 6634 | `				zIn++;` |
|      1 | 6635 | `			}` |
|      9 | 6636 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 6637 | `				/* Save the section name */` |
|      5 | 6638 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 6639 | `				SyStringFullTrim(&sEntry);` |
|      5 | 6640 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 6641 | `				if( sEntry.nByte > 0 ){` |
|      - | 6642 | `					/* Associate an array with the section */` |
|      5 | 6643 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 6644 | `					if( pSection ){` |
|      5 | 6645 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 6646 | `						pCur = pSection;` |
|      2 | 6647 | `					}` |
|      2 | 6648 | `				}` |
|      2 | 6649 | `			}` |
|      9 | 6650 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 6651 | `		}else{` |
|      - | 6652 | `			ph7_value *pOldCur;` |
|      - | 6653 | `			int is_array;` |
|      - | 6654 | `			int iLen;` |
|      - | 6655 | `			/* Properties */` |
|     23 | 6656 | `			is_array = 0;` |
|     23 | 6657 | `			zCur = zIn;` |
|     23 | 6658 | `			iLen = 0; /* cc warning */` |
|     23 | 6659 | `			pOldCur = pCur;` |
|    155 | 6660 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 6661 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 6662 | `					/* Array */` |
|    ! 0 | 6663 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 6664 | `					is_array = 1;` |
|    ! 0 | 6665 | `					if( iLen > 0 ){` |
|    ! 0 | 6666 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 6667 | `						/* Query the hashtable */` |
|    ! 0 | 6668 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 6669 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 6670 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 6671 | `						if( pEntry ){` |
|    ! 0 | 6672 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 6673 | `						}else{` |
|      - | 6674 | `							/* Create an empty array */` |
|    ! 0 | 6675 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 6676 | `							if( pvArr ){` |
|      - | 6677 | `								/* Save the entry */` |
|    ! 0 | 6678 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 6679 | `								/* Insert the entry */` |
|    ! 0 | 6680 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6681 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 6682 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 6683 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6684 | `							}` |
|      - | 6685 | `						}` |
|    ! 0 | 6686 | `						if( pvArr ){` |
|    ! 0 | 6687 | `							pCur = pvArr;` |
|    ! 0 | 6688 | `						}` |
|    ! 0 | 6689 | `					}` |
|    ! 0 | 6690 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 6691 | `						zIn++;` |
|    ! 0 | 6692 | `					}` |
|    ! 0 | 6693 | `				}` |
|    133 | 6694 | `				zIn++;` |
|      1 | 6695 | `			}` |
|     23 | 6696 | `			if( !is_array ){` |
|     23 | 6697 | `				iLen = (int)(zIn-zCur);` |
|     11 | 6698 | `			}` |
|      - | 6699 | `			/* Trim the key */` |
|     23 | 6700 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 6701 | `			SyStringFullTrim(&sEntry);` |
|     23 | 6702 | `			if( sEntry.nByte > 0 ){` |
|     23 | 6703 | `				if( !is_array ){` |
|      - | 6704 | `					/* Save the key name */` |
|     23 | 6705 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 6706 | `				}` |
|      - | 6707 | `				/* extract key value */` |
|     23 | 6708 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 6709 | `				zIn++; /* '=' */` |
|     39 | 6710 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 6711 | `					zIn++;` |
|      1 | 6712 | `				}` |
|     23 | 6713 | `				if( zIn < zEnd ){` |
|     21 | 6714 | `					zCur = zIn;` |
|     21 | 6715 | `					c = zIn[0];` |
|     21 | 6716 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6717 | `						zIn++;` |
|      - | 6718 | `						/* Delimit the value */` |
|    ! 0 | 6719 | `						while( zIn < zEnd ){` |
|    ! 0 | 6720 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 6721 | `								break;` |
|      - | 6722 | `							}` |
|    ! 0 | 6723 | `							zIn++;` |
|    ! 0 | 6724 | `						}` |
|    ! 0 | 6725 | `						if( zIn < zEnd ){` |
|    ! 0 | 6726 | `							zIn++;` |
|    ! 0 | 6727 | `						}` |
|    ! 0 | 6728 | `					}else{` |
|    125 | 6729 | `						while( zIn < zEnd ){` |
|    123 | 6730 | `							if( zIn[0] == '\n' ){` |
|     19 | 6731 | `								if( zIn[-1] != '\\' ){` |
|     19 | 6732 | `									break;` |
|    ! 0 | 6733 | `								}` |
|    105 | 6734 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6735 | `								/* Inline comments */` |
|    ! 0 | 6736 | `								break;` |
|      - | 6737 | `							}` |
|    105 | 6738 | `							zIn++;` |
|      1 | 6739 | `						}` |
|      - | 6740 | `					}` |
|      - | 6741 | `					/* Trim the value */` |
|     21 | 6742 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 6743 | `					SyStringFullTrim(&sEntry);` |
|     21 | 6744 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6745 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 6746 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 6747 | `					}` |
|     21 | 6748 | `					if( sEntry.nByte > 0 ){` |
|     21 | 6749 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 6750 | `					}` |
|      - | 6751 | `					/* Insert the key and it's value */` |
|     21 | 6752 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 6753 | `				}` |
|     12 | 6754 | `			}else{` |
|    ! 0 | 6755 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 6756 | `					zIn++;` |
|    ! 0 | 6757 | `				}` |
|      - | 6758 | `			}` |
|     23 | 6759 | `			pCur = pOldCur;` |
|      - | 6760 | `		}` |
|      1 | 6761 | `	}` |
|     13 | 6762 | `	SyHashRelease(&sHash);` |
|      - | 6763 | `	/* Return the parse of the INI string */` |
|     13 | 6764 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 6765 | `	return SXRET_OK;` |
|      7 | 6766 |  |
|      - | 6767 | `/*` |
|      - | 6768 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 6769 | ` *  Parse a configuration string.` |
|      - | 6770 | ` * Parameters` |
|      - | 6771 | ` *  $ini` |
|      - | 6772 | ` *   The contents of the ini file being parsed.` |
|      - | 6773 | ` *  $process_sections` |
|      - | 6774 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 6775 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 6776 | ` *  $scanner_mode (Not used)` |
|      - | 6777 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 6778 | ` *   then option values will not be parsed.` |
|      - | 6779 | ` * Return` |
|      - | 6780 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 6781 | ` */` |
|     10 | 6782 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6783 |  |
|      - | 6784 | `	const char *zIni;` |
|      - | 6785 | `	int nByte;` |
|     11 | 6786 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6787 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 6788 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6789 | `		return PH7_OK;` |
|      - | 6790 | `	}` |
|      - | 6791 | `	/* Extract the raw INI buffer */` |
|     11 | 6792 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 6793 | `	/* Process the INI buffer*/` |
|     11 | 6794 | `	PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|     11 | 6795 | `	return PH7_OK;` |
|      6 | 6796 |  |
|      - | 6797 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6798 |  |
|      - | 6799 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6800 |  |
|      - | 6801 | `/*` |
|      - | 6802 | ` * Ctype Functions.` |
|      - | 6803 | ` * Status:` |
|      - | 6804 | ` *    Stable.` |
|      - | 6805 | ` */` |
|      - | 6806 | `/*` |
|      - | 6807 | ` * bool ctype_alnum(string $text)` |
|      - | 6808 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 6809 | ` * Parameters` |
|      - | 6810 | ` *  $text` |
|      - | 6811 | ` *   The tested string.` |
|      - | 6812 | ` * Return` |
|      - | 6813 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 6814 | ` */` |
|     16 | 6815 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6816 |  |
|      - | 6817 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6818 | `	int nLen;` |
|     17 | 6819 | `	if( nArg < 1 ){` |
|      - | 6820 | `		/* Missing arguments,return FALSE */` |
|      3 | 6821 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6822 | `		return PH7_OK;` |
|      - | 6823 | `	}` |
|      - | 6824 | `	/* Extract the target string */` |
|     15 | 6825 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 6826 | `	zEnd = &zIn[nLen];` |
|     15 | 6827 | `	if( nLen < 1 ){` |
|      - | 6828 | `		/* Empty string,return FALSE */` |
|      3 | 6829 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6830 | `		return PH7_OK;` |
|      - | 6831 | `	}` |
|      - | 6832 | `	/* Perform the requested operation */` |
|     32 | 6833 | `	for(;;){` |
|     65 | 6834 | `		if( zIn >= zEnd ){` |
|      - | 6835 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6836 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6837 | `			return PH7_OK;` |
|      - | 6838 | `		}` |
|     57 | 6839 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 6840 | `			break;` |
|      - | 6841 | `		}` |
|      - | 6842 | `		/* Point to the next character */` |
|     53 | 6843 | `		zIn++;` |
|      1 | 6844 | `	}` |
|      - | 6845 | `	/* The test failed,return FALSE */` |
|      5 | 6846 | `	ph7_result_bool(pCtx,0);` |
|      5 | 6847 | `	return PH7_OK;` |
|      9 | 6848 |  |
|      - | 6849 | `/*` |
|      - | 6850 | ` * bool ctype_alpha(string $text)` |
|      - | 6851 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 6852 | ` * Parameters` |
|      - | 6853 | ` *  $text` |
|      - | 6854 | ` *   The tested string.` |
|      - | 6855 | ` * Return` |
|      - | 6856 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 6857 | ` */` |
|     18 | 6858 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6859 |  |
|      - | 6860 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6861 | `	int nLen;` |
|     19 | 6862 | `	if( nArg < 1 ){` |
|      - | 6863 | `		/* Missing arguments,return FALSE */` |
|      3 | 6864 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6865 | `		return PH7_OK;` |
|      - | 6866 | `	}` |
|      - | 6867 | `	/* Extract the target string */` |
|     17 | 6868 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6869 | `	zEnd = &zIn[nLen];` |
|     17 | 6870 | `	if( nLen < 1 ){` |
|      - | 6871 | `		/* Empty string,return FALSE */` |
|      3 | 6872 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6873 | `		return PH7_OK;` |
|      - | 6874 | `	}` |
|      - | 6875 | `	/* Perform the requested operation */` |
|     42 | 6876 | `	for(;;){` |
|     85 | 6877 | `		if( zIn >= zEnd ){` |
|      - | 6878 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6879 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6880 | `			return PH7_OK;` |
|      - | 6881 | `		}` |
|     77 | 6882 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 6883 | `			break;` |
|      - | 6884 | `		}` |
|      - | 6885 | `		/* Point to the next character */` |
|     71 | 6886 | `		zIn++;` |
|      1 | 6887 | `	}` |
|      - | 6888 | `	/* The test failed,return FALSE */` |
|      7 | 6889 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6890 | `	return PH7_OK;` |
|     10 | 6891 |  |
|      - | 6892 | `/*` |
|      - | 6893 | ` * bool ctype_cntrl(string $text)` |
|      - | 6894 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 6895 | ` * Parameters` |
|      - | 6896 | ` *  $text` |
|      - | 6897 | ` *   The tested string.` |
|      - | 6898 | ` * Return` |
|      - | 6899 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 6900 | ` */` |
|     18 | 6901 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6902 |  |
|      - | 6903 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6904 | `	int nLen;` |
|     19 | 6905 | `	if( nArg < 1 ){` |
|      - | 6906 | `		/* Missing arguments,return FALSE */` |
|      3 | 6907 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6908 | `		return PH7_OK;` |
|      - | 6909 | `	}` |
|      - | 6910 | `	/* Extract the target string */` |
|     17 | 6911 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6912 | `	zEnd = &zIn[nLen];` |
|     17 | 6913 | `	if( nLen < 1 ){` |
|      - | 6914 | `		/* Empty string,return FALSE */` |
|      3 | 6915 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6916 | `		return PH7_OK;` |
|      - | 6917 | `	}` |
|      - | 6918 | `	/* Perform the requested operation */` |
|     14 | 6919 | `	for(;;){` |
|     29 | 6920 | `		if( zIn >= zEnd ){` |
|      - | 6921 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6922 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6923 | `			return PH7_OK;` |
|      - | 6924 | `		}` |
|     21 | 6925 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6926 | `			/* UTF-8 stream  */` |
|    ! 0 | 6927 | `			break;` |
|      - | 6928 | `		}` |
|     21 | 6929 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 6930 | `			break;` |
|      - | 6931 | `		}` |
|      - | 6932 | `		/* Point to the next character */` |
|     15 | 6933 | `		zIn++;` |
|      1 | 6934 | `	}` |
|      - | 6935 | `	/* The test failed,return FALSE */` |
|      7 | 6936 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6937 | `	return PH7_OK;` |
|     10 | 6938 |  |
|      - | 6939 | `/*` |
|      - | 6940 | ` * bool ctype_digit(string $text)` |
|      - | 6941 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 6942 | ` * Parameters` |
|      - | 6943 | ` *  $text` |
|      - | 6944 | ` *   The tested string.` |
|      - | 6945 | ` * Return` |
|      - | 6946 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 6947 | ` */` |
|   1722 | 6948 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6949 |  |
|      - | 6950 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6951 | `	int nLen;` |
|   1724 | 6952 | `	if( nArg < 1 ){` |
|      - | 6953 | `		/* Missing arguments,return FALSE */` |
|      3 | 6954 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6955 | `		return PH7_OK;` |
|      - | 6956 | `	}` |
|      - | 6957 | `	/* Extract the target string */` |
|   1722 | 6958 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1722 | 6959 | `	zEnd = &zIn[nLen];` |
|   1722 | 6960 | `	if( nLen < 1 ){` |
|      - | 6961 | `		/* Empty string,return FALSE */` |
|      3 | 6962 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6963 | `		return PH7_OK;` |
|      - | 6964 | `	}` |
|      - | 6965 | `	/* Perform the requested operation */` |
|   1591 | 6966 | `	for(;;){` |
|   3184 | 6967 | `		if( zIn >= zEnd ){` |
|      - | 6968 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1434 | 6969 | `			ph7_result_bool(pCtx,1);` |
|   1434 | 6970 | `			return PH7_OK;` |
|      - | 6971 | `		}` |
|   1752 | 6972 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6973 | `			/* UTF-8 stream  */` |
|    ! 0 | 6974 | `			break;` |
|      - | 6975 | `		}` |
|   1752 | 6976 | `		if( !SyisDigit(zIn[0]) ){` |
|    288 | 6977 | `			break;` |
|      - | 6978 | `		}` |
|      - | 6979 | `		/* Point to the next character */` |
|   1466 | 6980 | `		zIn++;` |
|      2 | 6981 | `	}` |
|      - | 6982 | `	/* The test failed,return FALSE */` |
|    288 | 6983 | `	ph7_result_bool(pCtx,0);` |
|    288 | 6984 | `	return PH7_OK;` |
|    863 | 6985 |  |
|      - | 6986 | `/*` |
|      - | 6987 | ` * bool ctype_xdigit(string $text)` |
|      - | 6988 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 6989 | ` * Parameters` |
|      - | 6990 | ` *  $text` |
|      - | 6991 | ` *   The tested string.` |
|      - | 6992 | ` * Return` |
|      - | 6993 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 6994 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 6995 | ` */` |
|     20 | 6996 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6997 |  |
|      - | 6998 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6999 | `	int nLen;` |
|     21 | 7000 | `	if( nArg < 1 ){` |
|      - | 7001 | `		/* Missing arguments,return FALSE */` |
|      3 | 7002 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7003 | `		return PH7_OK;` |
|      - | 7004 | `	}` |
|      - | 7005 | `	/* Extract the target string */` |
|     19 | 7006 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 7007 | `	zEnd = &zIn[nLen];` |
|     19 | 7008 | `	if( nLen < 1 ){` |
|      - | 7009 | `		/* Empty string,return FALSE */` |
|      3 | 7010 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7011 | `		return PH7_OK;` |
|      - | 7012 | `	}` |
|      - | 7013 | `	/* Perform the requested operation */` |
|     46 | 7014 | `	for(;;){` |
|     93 | 7015 | `		if( zIn >= zEnd ){` |
|      - | 7016 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 7017 | `			ph7_result_bool(pCtx,1);` |
|     11 | 7018 | `			return PH7_OK;` |
|      - | 7019 | `		}` |
|     83 | 7020 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7021 | `			/* UTF-8 stream  */` |
|    ! 0 | 7022 | `			break;` |
|      - | 7023 | `		}` |
|     83 | 7024 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 7025 | `			break;` |
|      - | 7026 | `		}` |
|      - | 7027 | `		/* Point to the next character */` |
|     77 | 7028 | `		zIn++;` |
|      1 | 7029 | `	}` |
|      - | 7030 | `	/* The test failed,return FALSE */` |
|      7 | 7031 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7032 | `	return PH7_OK;` |
|     11 | 7033 |  |
|      - | 7034 | `/*` |
|      - | 7035 | ` * bool ctype_graph(string $text)` |
|      - | 7036 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 7037 | ` * Parameters` |
|      - | 7038 | ` *  $text` |
|      - | 7039 | ` *   The tested string.` |
|      - | 7040 | ` * Return` |
|      - | 7041 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 7042 | ` * (no white space), FALSE otherwise.` |
|      - | 7043 | ` */` |
|     18 | 7044 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7045 |  |
|      - | 7046 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7047 | `	int nLen;` |
|     19 | 7048 | `	if( nArg < 1 ){` |
|      - | 7049 | `		/* Missing arguments,return FALSE */` |
|      3 | 7050 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7051 | `		return PH7_OK;` |
|      - | 7052 | `	}` |
|      - | 7053 | `	/* Extract the target string */` |
|     17 | 7054 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7055 | `	zEnd = &zIn[nLen];` |
|     17 | 7056 | `	if( nLen < 1 ){` |
|      - | 7057 | `		/* Empty string,return FALSE */` |
|      3 | 7058 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7059 | `		return PH7_OK;` |
|      - | 7060 | `	}` |
|      - | 7061 | `	/* Perform the requested operation */` |
|     57 | 7062 | `	for(;;){` |
|    115 | 7063 | `		if( zIn >= zEnd ){` |
|      - | 7064 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7065 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7066 | `			return PH7_OK;` |
|      - | 7067 | `		}` |
|    107 | 7068 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7069 | `			/* UTF-8 stream  */` |
|    ! 0 | 7070 | `			break;` |
|      - | 7071 | `		}` |
|    107 | 7072 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 7073 | `			break;` |
|      - | 7074 | `		}` |
|      - | 7075 | `		/* Point to the next character */` |
|    101 | 7076 | `		zIn++;` |
|      1 | 7077 | `	}` |
|      - | 7078 | `	/* The test failed,return FALSE */` |
|      7 | 7079 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7080 | `	return PH7_OK;` |
|     10 | 7081 |  |
|      - | 7082 | `/*` |
|      - | 7083 | ` * bool ctype_print(string $text)` |
|      - | 7084 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 7085 | ` * Parameters` |
|      - | 7086 | ` *  $text` |
|      - | 7087 | ` *   The tested string.` |
|      - | 7088 | ` * Return` |
|      - | 7089 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 7090 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 7091 | ` *  or control function at all.` |
|      - | 7092 | ` */` |
|     18 | 7093 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7094 |  |
|      - | 7095 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7096 | `	int nLen;` |
|     19 | 7097 | `	if( nArg < 1 ){` |
|      - | 7098 | `		/* Missing arguments,return FALSE */` |
|      3 | 7099 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7100 | `		return PH7_OK;` |
|      - | 7101 | `	}` |
|      - | 7102 | `	/* Extract the target string */` |
|     17 | 7103 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7104 | `	zEnd = &zIn[nLen];` |
|     17 | 7105 | `	if( nLen < 1 ){` |
|      - | 7106 | `		/* Empty string,return FALSE */` |
|      3 | 7107 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7108 | `		return PH7_OK;` |
|      - | 7109 | `	}` |
|      - | 7110 | `	/* Perform the requested operation */` |
|     63 | 7111 | `	for(;;){` |
|    127 | 7112 | `		if( zIn >= zEnd ){` |
|      - | 7113 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7114 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7115 | `			return PH7_OK;` |
|      - | 7116 | `		}` |
|    119 | 7117 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7118 | `			/* UTF-8 stream  */` |
|    ! 0 | 7119 | `			break;` |
|      - | 7120 | `		}` |
|    119 | 7121 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 7122 | `			break;` |
|      - | 7123 | `		}` |
|      - | 7124 | `		/* Point to the next character */` |
|    113 | 7125 | `		zIn++;` |
|      1 | 7126 | `	}` |
|      - | 7127 | `	/* The test failed,return FALSE */` |
|      7 | 7128 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7129 | `	return PH7_OK;` |
|     10 | 7130 |  |
|      - | 7131 | `/*` |
|      - | 7132 | ` * bool ctype_punct(string $text)` |
|      - | 7133 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 7134 | ` * Parameters` |
|      - | 7135 | ` *  $text` |
|      - | 7136 | ` *   The tested string.` |
|      - | 7137 | ` * Return` |
|      - | 7138 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 7139 | ` *  digit or blank, FALSE otherwise.` |
|      - | 7140 | ` */` |
|     20 | 7141 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7142 |  |
|      - | 7143 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7144 | `	int nLen;` |
|     21 | 7145 | `	if( nArg < 1 ){` |
|      - | 7146 | `		/* Missing arguments,return FALSE */` |
|      3 | 7147 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7148 | `		return PH7_OK;` |
|      - | 7149 | `	}` |
|      - | 7150 | `	/* Extract the target string */` |
|     19 | 7151 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 7152 | `	zEnd = &zIn[nLen];` |
|     19 | 7153 | `	if( nLen < 1 ){` |
|      - | 7154 | `		/* Empty string,return FALSE */` |
|      3 | 7155 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7156 | `		return PH7_OK;` |
|      - | 7157 | `	}` |
|      - | 7158 | `	/* Perform the requested operation */` |
|     38 | 7159 | `	for(;;){` |
|     77 | 7160 | `		if( zIn >= zEnd ){` |
|      - | 7161 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7162 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7163 | `			return PH7_OK;` |
|      - | 7164 | `		}` |
|     69 | 7165 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7166 | `			/* UTF-8 stream  */` |
|    ! 0 | 7167 | `			break;` |
|      - | 7168 | `		}` |
|     69 | 7169 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 7170 | `			break;` |
|      - | 7171 | `		}` |
|      - | 7172 | `		/* Point to the next character */` |
|     61 | 7173 | `		zIn++;` |
|      1 | 7174 | `	}` |
|      - | 7175 | `	/* The test failed,return FALSE */` |
|      9 | 7176 | `	ph7_result_bool(pCtx,0);` |
|      9 | 7177 | `	return PH7_OK;` |
|     11 | 7178 |  |
|      - | 7179 | `/*` |
|      - | 7180 | ` * bool ctype_space(string $text)` |
|      - | 7181 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 7182 | ` * Parameters` |
|      - | 7183 | ` *  $text` |
|      - | 7184 | ` *   The tested string.` |
|      - | 7185 | ` * Return` |
|      - | 7186 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 7187 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 7188 | ` *  and form feed characters.` |
|      - | 7189 | ` */` |
|  58124 | 7190 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 7191 |  |
|      - | 7192 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7193 | `	int nLen;` |
|  58126 | 7194 | `	if( nArg < 1 ){` |
|      - | 7195 | `		/* Missing arguments,return FALSE */` |
|      3 | 7196 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7197 | `		return PH7_OK;` |
|      - | 7198 | `	}` |
|      - | 7199 | `	/* Extract the target string */` |
|  58124 | 7200 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  58124 | 7201 | `	zEnd = &zIn[nLen];` |
|  58124 | 7202 | `	if( nLen < 1 ){` |
|      - | 7203 | `		/* Empty string,return FALSE */` |
|      3 | 7204 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7205 | `		return PH7_OK;` |
|      - | 7206 | `	}` |
|      - | 7207 | `	/* Perform the requested operation */` |
|  29608 | 7208 | `	for(;;){` |
|  59174 | 7209 | `		if( zIn >= zEnd ){` |
|      - | 7210 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1030 | 7211 | `			ph7_result_bool(pCtx,1);` |
|   1030 | 7212 | `			return PH7_OK;` |
|      - | 7213 | `		}` |
|  58146 | 7214 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7215 | `			/* UTF-8 stream  */` |
|    ! 0 | 7216 | `			break;` |
|      - | 7217 | `		}` |
|  58146 | 7218 | `		if( !SyisSpace(zIn[0]) ){` |
|  57094 | 7219 | `			break;` |
|      - | 7220 | `		}` |
|      - | 7221 | `		/* Point to the next character */` |
|   1054 | 7222 | `		zIn++;` |
|      2 | 7223 | `	}` |
|      - | 7224 | `	/* The test failed,return FALSE */` |
|  57094 | 7225 | `	ph7_result_bool(pCtx,0);` |
|  57094 | 7226 | `	return PH7_OK;` |
|  29086 | 7227 |  |
|      - | 7228 | `/*` |
|      - | 7229 | ` * bool ctype_lower(string $text)` |
|      - | 7230 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 7231 | ` * Parameters` |
|      - | 7232 | ` *  $text` |
|      - | 7233 | ` *   The tested string.` |
|      - | 7234 | ` * Return` |
|      - | 7235 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 7236 | ` */` |
|     18 | 7237 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7238 |  |
|      - | 7239 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7240 | `	int nLen;` |
|     19 | 7241 | `	if( nArg < 1 ){` |
|      - | 7242 | `		/* Missing arguments,return FALSE */` |
|      3 | 7243 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7244 | `		return PH7_OK;` |
|      - | 7245 | `	}` |
|      - | 7246 | `	/* Extract the target string */` |
|     17 | 7247 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7248 | `	zEnd = &zIn[nLen];` |
|     17 | 7249 | `	if( nLen < 1 ){` |
|      - | 7250 | `		/* Empty string,return FALSE */` |
|      3 | 7251 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7252 | `		return PH7_OK;` |
|      - | 7253 | `	}` |
|      - | 7254 | `	/* Perform the requested operation */` |
|     27 | 7255 | `	for(;;){` |
|     55 | 7256 | `		if( zIn >= zEnd ){` |
|      - | 7257 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7258 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7259 | `			return PH7_OK;` |
|      - | 7260 | `		}` |
|     51 | 7261 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 7262 | `			break;` |
|      - | 7263 | `		}` |
|      - | 7264 | `		/* Point to the next character */` |
|     41 | 7265 | `		zIn++;` |
|      1 | 7266 | `	}` |
|      - | 7267 | `	/* The test failed,return FALSE */` |
|     11 | 7268 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7269 | `	return PH7_OK;` |
|     10 | 7270 |  |
|      - | 7271 | `/*` |
|      - | 7272 | ` * bool ctype_upper(string $text)` |
|      - | 7273 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 7274 | ` * Parameters` |
|      - | 7275 | ` *  $text` |
|      - | 7276 | ` *   The tested string.` |
|      - | 7277 | ` * Return` |
|      - | 7278 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 7279 | ` */` |
|     18 | 7280 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7281 |  |
|      - | 7282 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7283 | `	int nLen;` |
|     19 | 7284 | `	if( nArg < 1 ){` |
|      - | 7285 | `		/* Missing arguments,return FALSE */` |
|      3 | 7286 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7287 | `		return PH7_OK;` |
|      - | 7288 | `	}` |
|      - | 7289 | `	/* Extract the target string */` |
|     17 | 7290 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7291 | `	zEnd = &zIn[nLen];` |
|     17 | 7292 | `	if( nLen < 1 ){` |
|      - | 7293 | `		/* Empty string,return FALSE */` |
|      3 | 7294 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7295 | `		return PH7_OK;` |
|      - | 7296 | `	}` |
|      - | 7297 | `	/* Perform the requested operation */` |
|     28 | 7298 | `	for(;;){` |
|     57 | 7299 | `		if( zIn >= zEnd ){` |
|      - | 7300 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7301 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7302 | `			return PH7_OK;` |
|      - | 7303 | `		}` |
|     53 | 7304 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 7305 | `			break;` |
|      - | 7306 | `		}` |
|      - | 7307 | `		/* Point to the next character */` |
|     43 | 7308 | `		zIn++;` |
|      1 | 7309 | `	}` |
|      - | 7310 | `	/* The test failed,return FALSE */` |
|     11 | 7311 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7312 | `	return PH7_OK;` |
|     10 | 7313 |  |
|      - | 7314 | `/*` |
|      - | 7315 | ` * Date/Time functions` |
|      - | 7316 | ` * Status:` |
|      - | 7317 | ` *    Devel.` |
|      - | 7318 | ` */` |
|      - | 7319 | `#include <time.h>` |
|      - | 7320 | `#ifdef __WINNT__` |
|      - | 7321 | `/* GetSystemTime() */` |
|      - | 7322 | `#include <Windows.h>` |
|      - | 7323 | `#ifdef _WIN32_WCE` |
|      - | 7324 | `/*` |
|      - | 7325 | `** WindowsCE does not have a localtime() function.  So create a` |
|      - | 7326 | `** substitute.` |
|      - | 7327 | `** Taken from the SQLite3 source tree.` |
|      - | 7328 | `** Status: Public domain` |
|      - | 7329 | `*/` |
|      - | 7330 | `struct tm *__cdecl localtime(const time_t *t)` |
|      - | 7331 |  |
|      - | 7332 | `  static struct tm y;` |
|      - | 7333 | `  FILETIME uTm, lTm;` |
|      - | 7334 | `  SYSTEMTIME pTm;` |
|      - | 7335 | `  ph7_int64 t64;` |
|      - | 7336 | `  t64 = *t;` |
|      - | 7337 | `  t64 = (t64 + 11644473600)*10000000;` |
|      - | 7338 | `  uTm.dwLowDateTime = (DWORD)(t64 & 0xFFFFFFFF);` |
|      - | 7339 | `  uTm.dwHighDateTime= (DWORD)(t64 >> 32);` |
|      - | 7340 | `  FileTimeToLocalFileTime(&uTm,&lTm);` |
|      - | 7341 | `  FileTimeToSystemTime(&lTm,&pTm);` |
|      - | 7342 | `  y.tm_year = pTm.wYear - 1900;` |
|      - | 7343 | `  y.tm_mon = pTm.wMonth - 1;` |
|      - | 7344 | `  y.tm_wday = pTm.wDayOfWeek;` |
|      - | 7345 | `  y.tm_mday = pTm.wDay;` |
|      - | 7346 | `  y.tm_hour = pTm.wHour;` |
|      - | 7347 | `  y.tm_min = pTm.wMinute;` |
|      - | 7348 | `  y.tm_sec = pTm.wSecond;` |
|      - | 7349 | `  return &y;` |
|      - | 7350 |  |
|      - | 7351 | `#endif /*_WIN32_WCE */` |
|      - | 7352 | `#elif defined(__UNIXES__)` |
|      - | 7353 | `#include <sys/time.h>` |
|      - | 7354 | `#endif /* __WINNT__*/` |
|      - | 7355 | ` /*` |
|      - | 7356 | `  * int64 time(void)` |
|      - | 7357 | `  *  Current Unix timestamp` |
|      - | 7358 | `  * Parameters` |
|      - | 7359 | `  *  None.` |
|      - | 7360 | `  * Return` |
|      - | 7361 | `  *  Returns the current time measured in the number of seconds` |
|      - | 7362 | `  *  since the Unix Epoch (January 1 1970 00:00:00 GMT).` |
|      - | 7363 | `  */` |
|      8 | 7364 | `static int PH7_builtin_time(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7365 |  |
|      - | 7366 | `	time_t tt;` |
|      4 | 7367 | `	SXUNUSED(nArg); /* cc warning */` |
|      4 | 7368 | `	SXUNUSED(apArg);` |
|      - | 7369 | `	/* Extract the current time */` |
|      9 | 7370 | `	time(&tt);` |
|      - | 7371 | `	/* Return as 64-bit integer */` |
|      9 | 7372 | `	ph7_result_int64(pCtx,(ph7_int64)tt);` |
|      9 | 7373 | `	return  PH7_OK;` |
|      1 | 7374 |  |
|      - | 7375 | `/*` |
|      - | 7376 | `  * string/float microtime([ bool $get_as_float = false ])` |
|      - | 7377 | `  *  microtime() returns the current Unix timestamp with microseconds.` |
|      - | 7378 | `  * Parameters` |
|      - | 7379 | `  *  $get_as_float` |
|      - | 7380 | `  *   If used and set to TRUE, microtime() will return a float instead of a string` |
|      - | 7381 | `  *   as described in the return values section below.` |
|      - | 7382 | `  * Return` |
|      - | 7383 | `  *  By default, microtime() returns a string in the form "msec sec", where sec` |
|      - | 7384 | `  *  is the current time measured in the number of seconds since the Unix` |
|      - | 7385 | `  *  epoch (0:00:00 January 1, 1970 GMT), and msec is the number of microseconds` |
|      - | 7386 | `  *  that have elapsed since sec expressed in seconds.` |
|      - | 7387 | `  *  If get_as_float is set to TRUE, then microtime() returns a float, which represents` |
|      - | 7388 | `  *  the current time in seconds since the Unix epoch accurate to the nearest microsecond.` |
|      - | 7389 | `  */` |
|     20 | 7390 | `static int PH7_builtin_microtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7391 |  |
|     21 | 7392 | `	int bFloat = 0;` |
|      - | 7393 | `	sytime sTime;` |
|      - | 7394 | `#if defined(__UNIXES__)` |
|      - | 7395 | `	struct timeval tv;` |
|     20 | 7396 | `	gettimeofday(&tv,0);` |
|     20 | 7397 | `	sTime.tm_sec  = (long)tv.tv_sec;` |
|     20 | 7398 | `	sTime.tm_usec = (long)tv.tv_usec;` |
|      - | 7399 | `#else` |
|      - | 7400 | `	time_t tt;` |
|      1 | 7401 | `	time(&tt);` |
|      1 | 7402 | `	sTime.tm_sec  = (long)tt;` |
|      1 | 7403 | `	sTime.tm_usec = (long)(tt%SX_USEC_PER_SEC);` |
|      - | 7404 | `#endif /* __UNIXES__ */` |
|     21 | 7405 | `	if( nArg > 0 ){` |
|     17 | 7406 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|      8 | 7407 | `	}` |
|     21 | 7408 | `	if( bFloat ){` |
|      - | 7409 | `		/* Return as float */` |
|     17 | 7410 | `		ph7_result_double(pCtx,(double)sTime.tm_sec);` |
|      9 | 7411 | `	}else{` |
|      - | 7412 | `		/* Return as string */` |
|      5 | 7413 | `		ph7_result_string_format(pCtx,"%ld %ld",sTime.tm_usec,sTime.tm_sec);` |
|      - | 7414 | `	}` |
|     21 | 7415 | `	return PH7_OK;` |
|      1 | 7416 |  |
|      - | 7417 | `/*` |
|      - | 7418 | ` * array getdate ([ int $timestamp = time() ])` |
|      - | 7419 | ` *  Get date/time information.` |
|      - | 7420 | ` * Parameter` |
|      - | 7421 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|      - | 7422 | ` *     that defaults to the current local time if a timestamp is not given.` |
|      - | 7423 | ` *     In other words, it defaults to the value of time().` |
|      - | 7424 | ` * Returns` |
|      - | 7425 | ` *  Returns an associative array of information related to the timestamp.` |
|      - | 7426 | ` *  Elements from the returned associative array are as follows:` |
|      - | 7427 | ` *   KEY                                                         VALUE` |
|      - | 7428 | ` * ---------                                                    -------` |
|      - | 7429 | ` * "seconds" 	Numeric representation of seconds 	            0 to 59` |
|      - | 7430 | ` * "minutes" 	Numeric representation of minutes 	            0 to 59` |
|      - | 7431 | ` * "hours" 	    Numeric representation of hours 	            0 to 23` |
|      - | 7432 | ` * "mday" 	    Numeric representation of the day of the month 	1 to 31` |
|      - | 7433 | ` * "wday" 	    Numeric representation of the day of the week 	0 (for Sunday) through 6 (for Saturday)` |
|      - | 7434 | ` * "mon" 	    Numeric representation of a month 	            1 through 12` |
|      - | 7435 | ` * "year" 	    A full numeric representation of a year,        4 digits 	Examples: 1999 or 2003` |
|      - | 7436 | ` * "yday" 	    Numeric representation of the day of the year   0 through 365` |
|      - | 7437 | ` * "weekday" 	A full textual representation of the day of the week 	Sunday through Saturday` |
|      - | 7438 | ` * "month" 	    A full textual representation of a month, such as January or March 	January through December` |
|      - | 7439 | ` * 0 	        Seconds since the Unix Epoch, similar to the values returned by time() and used by date().` |
|      - | 7440 | ` * NOTE:` |
|      - | 7441 | ` *   NULL is returned on failure.` |
|      - | 7442 | ` */` |
|      8 | 7443 | `static int PH7_builtin_getdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7444 |  |
|      - | 7445 | `	ph7_value *pValue,*pArray;` |
|      - | 7446 | `	Sytm sTm;` |
|      9 | 7447 | `	if( nArg < 1 ){` |
|      - | 7448 | `#ifdef __WINNT__` |
|      - | 7449 | `		SYSTEMTIME sOS;` |
|      1 | 7450 | `		GetSystemTime(&sOS);` |
|      1 | 7451 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 7452 | `#else` |
|      - | 7453 | `		struct tm *pTm;` |
|      - | 7454 | `		time_t t;` |
|      4 | 7455 | `		time(&t);` |
|      4 | 7456 | `		pTm = localtime(&t);` |
|      4 | 7457 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7458 | `#endif` |
|      3 | 7459 | `	}else{` |
|      - | 7460 | `		/* Use the given timestamp */` |
|      - | 7461 | `		time_t t;` |
|      - | 7462 | `		struct tm *pTm;` |
|      - | 7463 | `#ifdef __WINNT__` |
|      - | 7464 | `#ifdef _MSC_VER` |
|      - | 7465 | `#if _MSC_VER >= 1400 /* Visual Studio 2005 and up */` |
|      - | 7466 | `#pragma warning(disable:4996) /* _CRT_SECURE...*/` |
|      - | 7467 | `#endif` |
|      - | 7468 | `#endif` |
|      - | 7469 | `#endif` |
|      5 | 7470 | `		if( ph7_value_is_int(apArg[0]) ){` |
|      5 | 7471 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|      5 | 7472 | `			pTm = localtime(&t);` |
|      5 | 7473 | `			if( pTm == 0 ){` |
|    ! 0 | 7474 | `				time(&t);` |
|    ! 0 | 7475 | `			}` |
|      3 | 7476 | `		}else{` |
|    ! 0 | 7477 | `			time(&t);` |
|      - | 7478 | `		}` |
|      5 | 7479 | `		pTm = localtime(&t);` |
|      5 | 7480 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7481 | `	}` |
|      - | 7482 | `	/* Element value */` |
|      9 | 7483 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      9 | 7484 | `	if( pValue == 0 ){` |
|      - | 7485 | `		/* Return NULL */` |
|    ! 0 | 7486 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7487 | `		return PH7_OK;` |
|      - | 7488 | `	}` |
|      - | 7489 | `	/* Create a new array */` |
|      9 | 7490 | `	pArray = ph7_context_new_array(pCtx);` |
|      9 | 7491 | `	if( pArray == 0 ){` |
|      - | 7492 | `		/* Return NULL */` |
|    ! 0 | 7493 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7494 | `		return PH7_OK;` |
|      - | 7495 | `	}` |
|      - | 7496 | `	/* Fill the array */` |
|      - | 7497 | `	/* Seconds */` |
|      9 | 7498 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|      9 | 7499 | `	ph7_array_add_strkey_elem(pArray,"seconds",pValue);` |
|      - | 7500 | `	/* Minutes */` |
|      9 | 7501 | `	ph7_value_int(pValue,sTm.tm_min);` |
|      9 | 7502 | `	ph7_array_add_strkey_elem(pArray,"minutes",pValue);` |
|      - | 7503 | `	/* Hours */` |
|      9 | 7504 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|      9 | 7505 | `	ph7_array_add_strkey_elem(pArray,"hours",pValue);` |
|      - | 7506 | `	/* mday */` |
|      9 | 7507 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|      9 | 7508 | `	ph7_array_add_strkey_elem(pArray,"mday",pValue);` |
|      - | 7509 | `	/* wday */` |
|      9 | 7510 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|      9 | 7511 | `	ph7_array_add_strkey_elem(pArray,"wday",pValue);` |
|      - | 7512 | `	/* mon */` |
|      9 | 7513 | `	ph7_value_int(pValue,sTm.tm_mon+1);` |
|      9 | 7514 | `	ph7_array_add_strkey_elem(pArray,"mon",pValue);` |
|      - | 7515 | `	/* year */` |
|      9 | 7516 | `	ph7_value_int(pValue,sTm.tm_year);` |
|      9 | 7517 | `	ph7_array_add_strkey_elem(pArray,"year",pValue);` |
|      - | 7518 | `	/* yday */` |
|      9 | 7519 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|      9 | 7520 | `	ph7_array_add_strkey_elem(pArray,"yday",pValue);` |
|      - | 7521 | `	/* Weekday */` |
|      9 | 7522 | `	ph7_value_string(pValue,SyTimeGetDay(sTm.tm_wday),-1);` |
|      9 | 7523 | `	ph7_array_add_strkey_elem(pArray,"weekday",pValue);` |
|      - | 7524 | `	/* Month */` |
|      9 | 7525 | `	ph7_value_reset_string_cursor(pValue);` |
|      9 | 7526 | `	ph7_value_string(pValue,SyTimeGetMonth(sTm.tm_mon),-1);` |
|      9 | 7527 | `	ph7_array_add_strkey_elem(pArray,"month",pValue);` |
|      - | 7528 | `	/* Seconds since the epoch */` |
|      9 | 7529 | `	ph7_value_int64(pValue,(ph7_int64)time(0));` |
|      9 | 7530 | `	ph7_array_add_intkey_elem(pArray,0 /* Index zero */,pValue);` |
|      - | 7531 | `	/* Return the freshly created array */` |
|      9 | 7532 | `	ph7_result_value(pCtx,pArray);` |
|      9 | 7533 | `	return PH7_OK;` |
|      5 | 7534 |  |
|      - | 7535 | `/*` |
|      - | 7536 | ` * mixed gettimeofday([ bool $return_float = false ] )` |
|      - | 7537 | ` *   Returns an associative array containing the data returned from the system call.` |
|      - | 7538 | ` * Parameters` |
|      - | 7539 | ` *  $return_float` |
|      - | 7540 | ` *   When set to TRUE, a float instead of an array is returned.` |
|      - | 7541 | ` * Return` |
|      - | 7542 | ` *   By default an array is returned. If return_float is set, then` |
|      - | 7543 | ` *   a float is returned.` |
|      - | 7544 | ` */` |
|      4 | 7545 | `static int PH7_builtin_gettimeofday(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7546 |  |
|      5 | 7547 | `	int bFloat = 0;` |
|      - | 7548 | `	sytime sTime;` |
|      - | 7549 | `#if defined(__UNIXES__)` |
|      - | 7550 | `	struct timeval tv;` |
|      4 | 7551 | `	gettimeofday(&tv,0);` |
|      4 | 7552 | `	sTime.tm_sec  = (long)tv.tv_sec;` |
|      4 | 7553 | `	sTime.tm_usec = (long)tv.tv_usec;` |
|      - | 7554 | `#else` |
|      - | 7555 | `	time_t tt;` |
|      1 | 7556 | `	time(&tt);` |
|      1 | 7557 | `	sTime.tm_sec  = (long)tt;` |
|      1 | 7558 | `	sTime.tm_usec = (long)(tt%SX_USEC_PER_SEC);` |
|      - | 7559 | `#endif /* __UNIXES__ */` |
|      5 | 7560 | `	if( nArg > 0 ){` |
|      5 | 7561 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|      2 | 7562 | `	}` |
|      5 | 7563 | `	if( bFloat ){` |
|      - | 7564 | `		/* Return as float */` |
|      3 | 7565 | `		ph7_result_double(pCtx,(double)sTime.tm_sec);` |
|      2 | 7566 | `	}else{` |
|      - | 7567 | `		/* Return an associative array */` |
|      - | 7568 | `		ph7_value *pValue,*pArray;` |
|      - | 7569 | `		/* Create a new array */` |
|      3 | 7570 | `		pArray = ph7_context_new_array(pCtx);` |
|      - | 7571 | `		/* Element value */` |
|      3 | 7572 | `		pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 7573 | `		if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 7574 | `			/* Return NULL */` |
|    ! 0 | 7575 | `			ph7_result_null(pCtx);` |
|    ! 0 | 7576 | `			return PH7_OK;` |
|      - | 7577 | `		}` |
|      - | 7578 | `		/* Fill the array */` |
|      - | 7579 | `		/* sec */` |
|      3 | 7580 | `		ph7_value_int64(pValue,sTime.tm_sec);` |
|      3 | 7581 | `		ph7_array_add_strkey_elem(pArray,"sec",pValue);` |
|      - | 7582 | `		/* usec */` |
|      3 | 7583 | `		ph7_value_int64(pValue,sTime.tm_usec);` |
|      3 | 7584 | `		ph7_array_add_strkey_elem(pArray,"usec",pValue);` |
|      - | 7585 | `		/* Return the array */` |
|      3 | 7586 | `		ph7_result_value(pCtx,pArray);` |
|      - | 7587 | `	}` |
|      5 | 7588 | `	return PH7_OK;` |
|      3 | 7589 |  |
|      - | 7590 | `/* Check if the given year is leap or not */` |
|      - | 7591 | `#define IS_LEAP_YEAR(YEAR)	(YEAR % 400 ? ( YEAR % 100 ? ( YEAR % 4 ? 0 : 1 ) : 0 ) : 1)` |
|      - | 7592 | `/* ISO-8601 numeric representation of the day of the week */` |
|      - | 7593 | `static const int aISO8601[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|      - | 7594 | `/*` |
|      - | 7595 | ` * Format a given date string.` |
|      - | 7596 | ` * Supported format: (Taken from PHP online docs)` |
|      - | 7597 | ` * character 	Description` |
|      - | 7598 | ` * d          Day of the month` |
|      - | 7599 | ` * D          A textual representation of a days` |
|      - | 7600 | ` * j          Day of the month without leading zeros` |
|      - | 7601 | ` * l          A full textual representation of the day of the week` |
|      - | 7602 | ` * N          ISO-8601 numeric representation of the day of the week` |
|      - | 7603 | ` * w          Numeric representation of the day of the week` |
|      - | 7604 | ` * z          The day of the year (starting from 0)` |
|      - | 7605 | ` * F          A full textual representation of a month, such as January or March` |
|      - | 7606 | ` * m          Numeric representation of a month, with leading zeros 	01 through 12` |
|      - | 7607 | ` * M          A short textual representation of a month, three letters 	Jan through Dec` |
|      - | 7608 | ` * n          Numeric representation of a month, without leading zeros 	1 through 12` |
|      - | 7609 | ` * t          Number of days in the given month 	28 through 31` |
|      - | 7610 | ` * L          Whether it's a leap year 	1 if it is a leap year, 0 otherwise.` |
|      - | 7611 | ` * o          ISO-8601 year number. This has the same value as Y, except that if the ISO week number` |
|      - | 7612 | ` *            (W) belongs to the previous or next year, that year is used instead. (added in PHP 5.1.0) Examples: 1999 or 2003` |
|      - | 7613 | ` * Y          A full numeric representation of a year, 4 digits 	Examples: 1999 or 2003` |
|      - | 7614 | ` * y          A two digit representation of a year 	Examples: 99 or 03` |
|      - | 7615 | ` * a          Lowercase Ante meridiem and Post meridiem 	am or pm` |
|      - | 7616 | ` * A          Uppercase Ante meridiem and Post meridiem 	AM or PM` |
|      - | 7617 | ` * g          12-hour format of an hour without leading zeros 	1 through 12` |
|      - | 7618 | ` * G          24-hour format of an hour without leading zeros 	0 through 23` |
|      - | 7619 | ` * h          12-hour format of an hour with leading zeros 	01 through 12` |
|      - | 7620 | ` * H          24-hour format of an hour with leading zeros 	00 through 23` |
|      - | 7621 | ` * i          Minutes with leading zeros 	00 to 59` |
|      - | 7622 | ` * s          Seconds, with leading zeros 	00 through 59` |
|      - | 7623 | ` * u          Microseconds Example: 654321` |
|      - | 7624 | ` * e          Timezone identifier 	Examples: UTC, GMT, Atlantic/Azores` |
|      - | 7625 | ` * I          (capital i) Whether or not the date is in daylight saving time 	1 if Daylight Saving Time, 0 otherwise.` |
|      - | 7626 | ` * r          RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 +0200` |
|      - | 7627 | ` * U          Seconds since the Unix Epoch (January 1 1970 00:00:00 GMT)` |
|      - | 7628 | ` * S          English ordinal suffix for the day of the month, 2 characters` |
|      - | 7629 | ` * O          Difference to Greenwich time (GMT) in hours` |
|      - | 7630 | ` * Z          Timezone offset in seconds. The offset for timezones west of UTC is always negative, and for those` |
|      - | 7631 | ` *            east of UTC is always positive.` |
|      - | 7632 | ` * c         ISO 8601 date` |
|      - | 7633 | ` */` |
|     46 | 7634 | `static sxi32 DateFormat(ph7_context *pCtx,const char *zIn,int nLen,Sytm *pTm)` |
|      1 | 7635 |  |
|     47 | 7636 | `	const char *zEnd = &zIn[nLen];` |
|      - | 7637 | `	const char *zCur;` |
|      - | 7638 | `	/* Start the format process */` |
|     78 | 7639 | `	for(;;){` |
|    157 | 7640 | `		if( zIn >= zEnd ){` |
|      - | 7641 | `			/* No more input to process */` |
|     47 | 7642 | `			break;` |
|      - | 7643 | `		}` |
|    111 | 7644 | `		switch(zIn[0]){` |
|      7 | 7645 | `		case 'd':` |
|      - | 7646 | `			/* Day of the month, 2 digits with leading zeros */` |
|     15 | 7647 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mday);` |
|     15 | 7648 | `			break;` |
|    ! 0 | 7649 | `		case 'D':` |
|      - | 7650 | `			/*A textual representation of a day, three letters*/` |
|    ! 0 | 7651 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|    ! 0 | 7652 | `			ph7_result_string(pCtx,zCur,3);` |
|    ! 0 | 7653 | `			break;` |
|    ! 0 | 7654 | `		case 'j':` |
|      - | 7655 | `			/*	Day of the month without leading zeros */` |
|    ! 0 | 7656 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mday);` |
|    ! 0 | 7657 | `			break;` |
|      2 | 7658 | `		case 'l':` |
|      - | 7659 | `			/* A full textual representation of the day of the week */` |
|      5 | 7660 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|      5 | 7661 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|      5 | 7662 | `			break;` |
|    ! 0 | 7663 | `		case 'N':{` |
|      - | 7664 | `			/* ISO-8601 numeric representation of the day of the week */` |
|    ! 0 | 7665 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|    ! 0 | 7666 | `			break;` |
|      - | 7667 | `				 }` |
|    ! 0 | 7668 | `		case 'w':` |
|      - | 7669 | `			/*Numeric representation of the day of the week*/` |
|    ! 0 | 7670 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|    ! 0 | 7671 | `			break;` |
|    ! 0 | 7672 | `		case 'z':` |
|      - | 7673 | `			/*The day of the year*/` |
|    ! 0 | 7674 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_yday);` |
|    ! 0 | 7675 | `			break;` |
|      2 | 7676 | `		case 'F':` |
|      - | 7677 | `			/*A full textual representation of a month, such as January or March*/` |
|      5 | 7678 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|      5 | 7679 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|      5 | 7680 | `			break;` |
|      7 | 7681 | `		case 'm':` |
|      - | 7682 | `			/*Numeric representation of a month, with leading zeros*/` |
|     15 | 7683 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|     15 | 7684 | `			break;` |
|    ! 0 | 7685 | `		case 'M':` |
|      - | 7686 | `			/*A short textual representation of a month, three letters*/` |
|    ! 0 | 7687 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|    ! 0 | 7688 | `			ph7_result_string(pCtx,zCur,3);` |
|    ! 0 | 7689 | `			break;` |
|    ! 0 | 7690 | `		case 'n':` |
|      - | 7691 | `			/*Numeric representation of a month, without leading zeros*/` |
|    ! 0 | 7692 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mon + 1);` |
|    ! 0 | 7693 | `			break;` |
|    ! 0 | 7694 | `		case 't':{` |
|      - | 7695 | `			static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|    ! 0 | 7696 | `			int nDays = aMonDays[pTm->tm_mon % 12 ];` |
|    ! 0 | 7697 | `			if( pTm->tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(pTm->tm_year) ){` |
|    ! 0 | 7698 | `				nDays = 28;` |
|    ! 0 | 7699 | `			}` |
|      - | 7700 | `			/*Number of days in the given month*/` |
|    ! 0 | 7701 | `			ph7_result_string_format(pCtx,"%d",nDays);` |
|    ! 0 | 7702 | `			break;` |
|      - | 7703 | `				 }` |
|    ! 0 | 7704 | `		case 'L':{` |
|    ! 0 | 7705 | `			int isLeap = IS_LEAP_YEAR(pTm->tm_year);` |
|      - | 7706 | `			/* Whether it's a leap year */` |
|    ! 0 | 7707 | `			ph7_result_string_format(pCtx,"%d",isLeap);` |
|    ! 0 | 7708 | `			break;` |
|      - | 7709 | `				 }` |
|    ! 0 | 7710 | `		case 'o':` |
|      - | 7711 | `			/* ISO-8601 year number.*/` |
|    ! 0 | 7712 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|    ! 0 | 7713 | `			break;` |
|      9 | 7714 | `		case 'Y':` |
|      - | 7715 | `			/*	A full numeric representation of a year, 4 digits */` |
|     19 | 7716 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|     19 | 7717 | `			break;` |
|    ! 0 | 7718 | `		case 'y':` |
|      - | 7719 | `			/*A two digit representation of a year*/` |
|    ! 0 | 7720 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_year%100);` |
|    ! 0 | 7721 | `			break;` |
|    ! 0 | 7722 | `		case 'a':` |
|      - | 7723 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7724 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",2);` |
|    ! 0 | 7725 | `			break;` |
|    ! 0 | 7726 | `		case 'A':` |
|      - | 7727 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7728 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",2);` |
|    ! 0 | 7729 | `			break;` |
|    ! 0 | 7730 | `		case 'g':` |
|      - | 7731 | `			/*	12-hour format of an hour without leading zeros*/` |
|    ! 0 | 7732 | `			ph7_result_string_format(pCtx,"%d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7733 | `			break;` |
|    ! 0 | 7734 | `		case 'G':` |
|      - | 7735 | `			/* 24-hour format of an hour without leading zeros */` |
|    ! 0 | 7736 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_hour);` |
|    ! 0 | 7737 | `			break;` |
|    ! 0 | 7738 | `		case 'h':` |
|      - | 7739 | `			/* 12-hour format of an hour with leading zeros */` |
|    ! 0 | 7740 | `			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7741 | `			break;` |
|      3 | 7742 | `		case 'H':` |
|      - | 7743 | `			/*	24-hour format of an hour with leading zeros */` |
|      7 | 7744 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|      7 | 7745 | `			break;` |
|      3 | 7746 | `		case 'i':` |
|      - | 7747 | `			/* 	Minutes with leading zeros */` |
|      7 | 7748 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|      7 | 7749 | `			break;` |
|      3 | 7750 | `		case 's':` |
|      - | 7751 | `			/* 	second with leading zeros */` |
|      7 | 7752 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|      7 | 7753 | `			break;` |
|    ! 0 | 7754 | `		case 'u':` |
|      - | 7755 | `			/* 	Microseconds */` |
|    ! 0 | 7756 | `			ph7_result_string_format(pCtx,"%u",pTm->tm_sec * SX_USEC_PER_SEC);` |
|    ! 0 | 7757 | `			break;` |
|    ! 0 | 7758 | `		case 'S':{` |
|      - | 7759 | `			/* English ordinal suffix for the day of the month, 2 characters */` |
|      - | 7760 | `			static const char zSuffix[] = "thstndrdthththththth";` |
|    ! 0 | 7761 | `			int v = pTm->tm_mday;` |
|    ! 0 | 7762 | `			ph7_result_string(pCtx,&zSuffix[2 * (int)(v / 10 % 10 != 1 ? v % 10 : 0)],(int)sizeof(char) * 2);` |
|    ! 0 | 7763 | `			break;` |
|      - | 7764 | `				 }` |
|    ! 0 | 7765 | `		case 'e':` |
|      - | 7766 | `			/* 	Timezone identifier */` |
|    ! 0 | 7767 | `			zCur = pTm->tm_zone;` |
|    ! 0 | 7768 | `			if( zCur == 0 ){` |
|      - | 7769 | `				/* Assume GMT */` |
|    ! 0 | 7770 | `				zCur = "GMT";` |
|    ! 0 | 7771 | `			}` |
|    ! 0 | 7772 | `			ph7_result_string(pCtx,zCur,-1);` |
|    ! 0 | 7773 | `			break;` |
|    ! 0 | 7774 | `		case 'I':` |
|      - | 7775 | `			/* Whether or not the date is in daylight saving time */` |
|      - | 7776 | `#ifdef __WINNT__` |
|      - | 7777 | `#ifdef _MSC_VER` |
|      - | 7778 | `#ifndef _WIN32_WCE` |
|    ! 0 | 7779 | `			_get_daylight(&pTm->tm_isdst);` |
|      - | 7780 | `#endif` |
|      - | 7781 | `#endif` |
|      - | 7782 | `#endif` |
|    ! 0 | 7783 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_isdst == 1);` |
|    ! 0 | 7784 | `			break;` |
|    ! 0 | 7785 | `		case 'r':` |
|      - | 7786 | `			/* RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 */` |
|    ! 0 | 7787 | `			ph7_result_string_format(pCtx,"%.3s, %02d %.3s %4d %02d:%02d:%02d",` |
|    ! 0 | 7788 | `				SyTimeGetDay(pTm->tm_wday),` |
|    ! 0 | 7789 | `				pTm->tm_mday,` |
|    ! 0 | 7790 | `				SyTimeGetMonth(pTm->tm_mon),` |
|    ! 0 | 7791 | `				pTm->tm_year,` |
|    ! 0 | 7792 | `				pTm->tm_hour,` |
|    ! 0 | 7793 | `				pTm->tm_min,` |
|    ! 0 | 7794 | `				pTm->tm_sec` |
|      - | 7795 | `				);` |
|    ! 0 | 7796 | `			break;` |
|    ! 0 | 7797 | `		case 'U':{` |
|      - | 7798 | `			time_t tt;` |
|      - | 7799 | `			/* Seconds since the Unix Epoch */` |
|    ! 0 | 7800 | `			time(&tt);` |
|    ! 0 | 7801 | `			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);` |
|    ! 0 | 7802 | `			break;` |
|      - | 7803 | `				 }` |
|    ! 0 | 7804 | `		case 'O':` |
|      - | 7805 | `		case 'P':` |
|      - | 7806 | `			/* Difference to Greenwich time (GMT) in hours */` |
|    ! 0 | 7807 | `			ph7_result_string_format(pCtx,"%+05d",pTm->tm_gmtoff);` |
|    ! 0 | 7808 | `			break;` |
|    ! 0 | 7809 | `		case 'Z':` |
|      - | 7810 | `			/* Timezone offset in seconds. The offset for timezones west of UTC` |
|      - | 7811 | `			 * is always negative, and for those east of UTC is always positive.` |
|      - | 7812 | `			 */` |
|    ! 0 | 7813 | `			ph7_result_string_format(pCtx,"%+05d",pTm->tm_gmtoff);` |
|    ! 0 | 7814 | `			break;` |
|      1 | 7815 | `		case 'c':` |
|      - | 7816 | `			/* 	ISO 8601 date */` |
|      4 | 7817 | `			ph7_result_string_format(pCtx,"%4d-%02d-%02dT%02d:%02d:%02d%+05d",` |
|      1 | 7818 | `				pTm->tm_year,` |
|      2 | 7819 | `				pTm->tm_mon+1,` |
|      1 | 7820 | `				pTm->tm_mday,` |
|      1 | 7821 | `				pTm->tm_hour,` |
|      1 | 7822 | `				pTm->tm_min,` |
|      1 | 7823 | `				pTm->tm_sec,` |
|      1 | 7824 | `				pTm->tm_gmtoff` |
|      - | 7825 | `				);` |
|      3 | 7826 | `			break;` |
|      1 | 7827 | `		case '\\':` |
|      3 | 7828 | `			zIn++;` |
|      - | 7829 | `			/* Expand verbatim */` |
|      3 | 7830 | `			if( zIn < zEnd ){` |
|      3 | 7831 | `				ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|      1 | 7832 | `			}` |
|      3 | 7833 | `			break;` |
|     17 | 7834 | `		default:` |
|      - | 7835 | `			/* Unknown format specifer,expand verbatim */` |
|     35 | 7836 | `			ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|     34 | 7837 | `			break;` |
|      - | 7838 | `		}` |
|      - | 7839 | `		/* Point to the next character */` |
|    111 | 7840 | `		zIn++;` |
|      1 | 7841 | `	}` |
|     47 | 7842 | `	return SXRET_OK;` |
|      1 | 7843 |  |
|      - | 7844 | `/*` |
|      - | 7845 | ` * PH7 implementation of the strftime() function.` |
|      - | 7846 | ` * The following formats are supported:` |
|      - | 7847 | ` * %a 	An abbreviated textual representation of the day` |
|      - | 7848 | ` * %A 	A full textual representation of the day` |
|      - | 7849 | ` * %d 	Two-digit day of the month (with leading zeros)` |
|      - | 7850 | ` * %e 	Day of the month, with a space preceding single digits.` |
|      - | 7851 | ` * %j 	Day of the year, 3 digits with leading zeros` |
|      - | 7852 | ` * %u 	ISO-8601 numeric representation of the day of the week 	1 (for Monday) though 7 (for Sunday)` |
|      - | 7853 | ` * %w 	Numeric representation of the day of the week 0 (for Sunday) through 6 (for Saturday)` |
|      - | 7854 | ` * %U 	Week number of the given year, starting with the first Sunday as the first week` |
|      - | 7855 | ` * %V 	ISO-8601:1988 week number of the given year, starting with the first week of the year with at least` |
|      - | 7856 | ` *   4 weekdays, with Monday being the start of the week.` |
|      - | 7857 | ` * %W 	A numeric representation of the week of the year` |
|      - | 7858 | ` * %b 	Abbreviated month name, based on the locale` |
|      - | 7859 | ` * %B 	Full month name, based on the locale` |
|      - | 7860 | ` * %h 	Abbreviated month name, based on the locale (an alias of %b)` |
|      - | 7861 | ` * %m 	Two digit representation of the month` |
|      - | 7862 | ` * %C 	Two digit representation of the century (year divided by 100, truncated to an integer)` |
|      - | 7863 | ` * %g 	Two digit representation of the year going by ISO-8601:1988 standards (see %V)` |
|      - | 7864 | ` * %G 	The full four-digit version of %g` |
|      - | 7865 | ` * %y 	Two digit representation of the year` |
|      - | 7866 | ` * %Y 	Four digit representation for the year` |
|      - | 7867 | ` * %H 	Two digit representation of the hour in 24-hour format` |
|      - | 7868 | ` * %I 	Two digit representation of the hour in 12-hour format` |
|      - | 7869 | ` * %l (lower-case 'L') 	Hour in 12-hour format, with a space preceeding single digits` |
|      - | 7870 | ` * %M 	Two digit representation of the minute` |
|      - | 7871 | ` * %p 	UPPER-CASE 'AM' or 'PM' based on the given time` |
|      - | 7872 | ` * %P 	lower-case 'am' or 'pm' based on the given time` |
|      - | 7873 | ` * %r 	Same as "%I:%M:%S %p"` |
|      - | 7874 | ` * %R 	Same as "%H:%M"` |
|      - | 7875 | ` * %S 	Two digit representation of the second` |
|      - | 7876 | ` * %T 	Same as "%H:%M:%S"` |
|      - | 7877 | ` * %X 	Preferred time representation based on locale, without the date` |
|      - | 7878 | ` * %z 	Either the time zone offset from UTC or the abbreviation` |
|      - | 7879 | ` * %Z 	The time zone offset/abbreviation option NOT given by %z` |
|      - | 7880 | ` * %c 	Preferred date and time stamp based on local` |
|      - | 7881 | ` * %D 	Same as "%m/%d/%y"` |
|      - | 7882 | ` * %F 	Same as "%Y-%m-%d"` |
|      - | 7883 | ` * %s 	Unix Epoch Time timestamp (same as the time() function)` |
|      - | 7884 | ` * %x 	Preferred date representation based on locale, without the time` |
|      - | 7885 | ` * %n 	A newline character ("\n")` |
|      - | 7886 | ` * %t 	A Tab character ("\t")` |
|      - | 7887 | ` * %% 	A literal percentage character ("%")` |
|      - | 7888 | ` */` |
|     16 | 7889 | `static int PH7_Strftime(` |
|      - | 7890 | `	ph7_context *pCtx,  /* Call context */` |
|      - | 7891 | `	const char *zIn,    /* Input string */` |
|      - | 7892 | `	int nLen,           /* Input length */` |
|      - | 7893 | `	Sytm *pTm           /* Parse of the given time */` |
|      - | 7894 | `	)` |
|      1 | 7895 |  |
|     17 | 7896 | `	const char *zCur,*zEnd = &zIn[nLen];` |
|      - | 7897 | `	int c;` |
|      - | 7898 | `	/* Start the format process */` |
|     18 | 7899 | `	for(;;){` |
|     37 | 7900 | `		zCur = zIn;` |
|     41 | 7901 | `		while(zIn < zEnd && zIn[0] != '%' ){` |
|      5 | 7902 | `			zIn++;` |
|      1 | 7903 | `		}` |
|     37 | 7904 | `		if( zIn > zCur ){` |
|      - | 7905 | `			/* Consume input verbatim */` |
|      5 | 7906 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 7907 | `		}` |
|     37 | 7908 | `		zIn++; /* Jump the percent sign */` |
|     37 | 7909 | `		if( zIn >= zEnd ){` |
|      - | 7910 | `			/* No more input to process */` |
|     17 | 7911 | `			break;` |
|      - | 7912 | `		}` |
|     21 | 7913 | `		c = zIn[0];` |
|      - | 7914 | `		/* Act according to the current specifer */` |
|     21 | 7915 | `		switch(c){` |
|    ! 0 | 7916 | `		case '%':` |
|      - | 7917 | `			/* A literal percentage character ("%") */` |
|    ! 0 | 7918 | `			ph7_result_string(pCtx,"%",(int)sizeof(char));` |
|    ! 0 | 7919 | `			break;` |
|    ! 0 | 7920 | `		case 't':` |
|      - | 7921 | `			/* A Tab character */` |
|    ! 0 | 7922 | `			ph7_result_string(pCtx,"\t",(int)sizeof(char));` |
|    ! 0 | 7923 | `			break;` |
|    ! 0 | 7924 | `		case 'n':` |
|      - | 7925 | `			/* A newline character */` |
|    ! 0 | 7926 | `			ph7_result_string(pCtx,"\n",(int)sizeof(char));` |
|    ! 0 | 7927 | `			break;` |
|      1 | 7928 | `		case 'a':` |
|      - | 7929 | `			/* An abbreviated textual representation of the day */` |
|      3 | 7930 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),(int)sizeof(char)*3);` |
|      3 | 7931 | `			break;` |
|    ! 0 | 7932 | `		case 'A':` |
|      - | 7933 | `			/* A full textual representation of the day */` |
|    ! 0 | 7934 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),-1/*Compute length automatically*/);` |
|    ! 0 | 7935 | `			break;` |
|    ! 0 | 7936 | `		case 'e':` |
|      - | 7937 | `			/* Day of the month, 2 digits with leading space for single digit*/` |
|    ! 0 | 7938 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_mday);` |
|    ! 0 | 7939 | `			break;` |
|      2 | 7940 | `		case 'd':` |
|      - | 7941 | `			/* Two-digit day of the month (with leading zeros) */` |
|      5 | 7942 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon+1);` |
|      5 | 7943 | `			break;` |
|    ! 0 | 7944 | `		case 'j':` |
|      - | 7945 | `			/*The day of the year,3 digits with leading zeros*/` |
|    ! 0 | 7946 | `			ph7_result_string_format(pCtx,"%03d",pTm->tm_yday);` |
|    ! 0 | 7947 | `			break;` |
|    ! 0 | 7948 | `		case 'u':` |
|      - | 7949 | `			/* ISO-8601 numeric representation of the day of the week */` |
|    ! 0 | 7950 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|    ! 0 | 7951 | `			break;` |
|    ! 0 | 7952 | `		case 'w':` |
|      - | 7953 | `			/* Numeric representation of the day of the week */` |
|    ! 0 | 7954 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|    ! 0 | 7955 | `			break;` |
|    ! 0 | 7956 | `		case 'b':` |
|      - | 7957 | `		case 'h':` |
|      - | 7958 | `			/*A short textual representation of a month, three letters (Not based on locale)*/` |
|    ! 0 | 7959 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),(int)sizeof(char)*3);` |
|    ! 0 | 7960 | `			break;` |
|    ! 0 | 7961 | `		case 'B':` |
|      - | 7962 | `			/* Full month name (Not based on locale) */` |
|    ! 0 | 7963 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),-1/*Compute length automatically*/);` |
|    ! 0 | 7964 | `			break;` |
|      2 | 7965 | `		case 'm':` |
|      - | 7966 | `			/*Numeric representation of a month, with leading zeros*/` |
|      5 | 7967 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|      5 | 7968 | `			break;` |
|    ! 0 | 7969 | `		case 'C':` |
|      - | 7970 | `			/* Two digit representation of the century */` |
|    ! 0 | 7971 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year/100);` |
|    ! 0 | 7972 | `			break;` |
|    ! 0 | 7973 | `		case 'y':` |
|      - | 7974 | `		case 'g':` |
|      - | 7975 | `			/* Two digit representation of the year */` |
|    ! 0 | 7976 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year%100);` |
|    ! 0 | 7977 | `			break;` |
|      2 | 7978 | `		case 'Y':` |
|      - | 7979 | `		case 'G':` |
|      - | 7980 | `			/* Four digit representation of the year */` |
|      5 | 7981 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|      5 | 7982 | `			break;` |
|    ! 0 | 7983 | `		case 'I':` |
|      - | 7984 | `			/* 12-hour format of an hour with leading zeros */` |
|    ! 0 | 7985 | `			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7986 | `			break;` |
|    ! 0 | 7987 | `		case 'l':` |
|      - | 7988 | `			/* 12-hour format of an hour with leading space */` |
|    ! 0 | 7989 | `			ph7_result_string_format(pCtx,"%2d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7990 | `			break;` |
|      1 | 7991 | `		case 'H':` |
|      - | 7992 | `			/* 24-hour format of an hour with leading zeros */` |
|      3 | 7993 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|      3 | 7994 | `			break;` |
|      1 | 7995 | `		case 'M':` |
|      - | 7996 | `			/* Minutes with leading zeros */` |
|      3 | 7997 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|      3 | 7998 | `			break;` |
|    ! 0 | 7999 | `		case 'S':` |
|      - | 8000 | `			/* Seconds with leading zeros */` |
|    ! 0 | 8001 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|    ! 0 | 8002 | `			break;` |
|    ! 0 | 8003 | `		case 'z':` |
|      - | 8004 | `		case 'Z':` |
|      - | 8005 | `			/* 	Timezone identifier */` |
|    ! 0 | 8006 | `			zCur = pTm->tm_zone;` |
|    ! 0 | 8007 | `			if( zCur == 0 ){` |
|      - | 8008 | `				/* Assume GMT */` |
|    ! 0 | 8009 | `				zCur = "GMT";` |
|    ! 0 | 8010 | `			}` |
|    ! 0 | 8011 | `			ph7_result_string(pCtx,zCur,-1);` |
|    ! 0 | 8012 | `			break;` |
|    ! 0 | 8013 | `		case 'T':` |
|      - | 8014 | `		case 'X':` |
|      - | 8015 | `			/* Same as "%H:%M:%S" */` |
|    ! 0 | 8016 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d",pTm->tm_hour,pTm->tm_min,pTm->tm_sec);` |
|    ! 0 | 8017 | `			break;` |
|    ! 0 | 8018 | `		case 'R':` |
|      - | 8019 | `			/* Same as "%H:%M" */` |
|    ! 0 | 8020 | `			ph7_result_string_format(pCtx,"%02d:%02d",pTm->tm_hour,pTm->tm_min);` |
|    ! 0 | 8021 | `			break;` |
|    ! 0 | 8022 | `		case 'P':` |
|      - | 8023 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|    ! 0 | 8024 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",(int)sizeof(char)*2);` |
|    ! 0 | 8025 | `			break;` |
|    ! 0 | 8026 | `		case 'p':` |
|      - | 8027 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|    ! 0 | 8028 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",(int)sizeof(char)*2);` |
|    ! 0 | 8029 | `			break;` |
|    ! 0 | 8030 | `		case 'r':` |
|      - | 8031 | `			/* Same as "%I:%M:%S %p" */` |
|    ! 0 | 8032 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d %s",` |
|    ! 0 | 8033 | `				1+(pTm->tm_hour%12),` |
|    ! 0 | 8034 | `				pTm->tm_min,` |
|    ! 0 | 8035 | `				pTm->tm_sec,` |
|    ! 0 | 8036 | `				pTm->tm_hour > 12 ? "PM" : "AM"` |
|      - | 8037 | `				);` |
|    ! 0 | 8038 | `			break;` |
|      1 | 8039 | `		case 'D':` |
|      - | 8040 | `		case 'x':` |
|      - | 8041 | `			/* Same as "%m/%d/%y" */` |
|      4 | 8042 | `			ph7_result_string_format(pCtx,"%02d/%02d/%02d",` |
|      2 | 8043 | `				pTm->tm_mon+1,` |
|      1 | 8044 | `				pTm->tm_mday,` |
|      2 | 8045 | `				pTm->tm_year%100` |
|      - | 8046 | `				);` |
|      3 | 8047 | `			break;` |
|    ! 0 | 8048 | `		case 'F':` |
|      - | 8049 | `			/* Same as "%Y-%m-%d" */` |
|    ! 0 | 8050 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d",` |
|    ! 0 | 8051 | `				pTm->tm_year,` |
|    ! 0 | 8052 | `				pTm->tm_mon+1,` |
|    ! 0 | 8053 | `				pTm->tm_mday` |
|      - | 8054 | `				);` |
|    ! 0 | 8055 | `			break;` |
|    ! 0 | 8056 | `		case 'c':` |
|    ! 0 | 8057 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d %02d:%02d:%02d",` |
|    ! 0 | 8058 | `				pTm->tm_year,` |
|    ! 0 | 8059 | `				pTm->tm_mon+1,` |
|    ! 0 | 8060 | `				pTm->tm_mday,` |
|    ! 0 | 8061 | `				pTm->tm_hour,` |
|    ! 0 | 8062 | `				pTm->tm_min,` |
|    ! 0 | 8063 | `				pTm->tm_sec` |
|      - | 8064 | `				);` |
|    ! 0 | 8065 | `			break;` |
|    ! 0 | 8066 | `		case 's':{` |
|      - | 8067 | `			time_t tt;` |
|      - | 8068 | `			/* Seconds since the Unix Epoch */` |
|    ! 0 | 8069 | `			time(&tt);` |
|    ! 0 | 8070 | `			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);` |
|    ! 0 | 8071 | `			break;` |
|      - | 8072 | `				 }` |
|    ! 0 | 8073 | `		default:` |
|      - | 8074 | `			/* unknown specifer,simply ignore*/` |
|    ! 0 | 8075 | `			break;` |
|      - | 8076 | `		}` |
|      - | 8077 | `		/* Advance the cursor */` |
|     21 | 8078 | `		zIn++;` |
|      1 | 8079 | `	}` |
|     17 | 8080 | `	return SXRET_OK;` |
|      1 | 8081 |  |
|      - | 8082 | `/*` |
|      - | 8083 | ` * string date(string $format [, int $timestamp = time() ] )` |
|      - | 8084 | ` *  Returns a string formatted according to the given format string using` |
|      - | 8085 | ` *  the given integer timestamp or the current time if no timestamp is given.` |
|      - | 8086 | ` *  In other words, timestamp is optional and defaults to the value of time().` |
|      - | 8087 | ` * Parameters` |
|      - | 8088 | ` *  $format` |
|      - | 8089 | ` *   The format of the outputted date string (See code above)` |
|      - | 8090 | ` * $timestamp` |
|      - | 8091 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|      - | 8092 | ` *   that defaults to the current local time if a timestamp is not given.` |
|      - | 8093 | ` *   In other words, it defaults to the value of time().` |
|      - | 8094 | ` * Return` |
|      - | 8095 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|      - | 8096 | ` */` |
|     36 | 8097 | `static int PH7_builtin_date(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8098 |  |
|      - | 8099 | `	const char *zFormat;` |
|      - | 8100 | `	int nLen;` |
|      - | 8101 | `	Sytm sTm;` |
|     37 | 8102 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8103 | `		/* Missing/Invalid argument,return FALSE */` |
|      5 | 8104 | `		ph7_result_bool(pCtx,0);` |
|      5 | 8105 | `		return PH7_OK;` |
|      - | 8106 | `	}` |
|     33 | 8107 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     33 | 8108 | `	if( nLen < 1 ){` |
|      - | 8109 | `		/* Don't bother processing return the empty string */` |
|    ! 0 | 8110 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 8111 | `	}` |
|     33 | 8112 | `	if( nArg < 2 ){` |
|      - | 8113 | `#ifdef __WINNT__` |
|      - | 8114 | `		SYSTEMTIME sOS;` |
|      1 | 8115 | `		GetSystemTime(&sOS);` |
|      1 | 8116 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8117 | `#else` |
|      - | 8118 | `		struct tm *pTm;` |
|      - | 8119 | `		time_t t;` |
|     30 | 8120 | `		time(&t);` |
|     30 | 8121 | `		pTm = localtime(&t);` |
|     30 | 8122 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8123 | `#endif` |
|     16 | 8124 | `	}else{` |
|      - | 8125 | `		/* Use the given timestamp */` |
|      - | 8126 | `		time_t t;` |
|      - | 8127 | `		struct tm *pTm;` |
|      3 | 8128 | `		if( ph7_value_is_int(apArg[1]) ){` |
|      3 | 8129 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|      3 | 8130 | `			pTm = localtime(&t);` |
|      3 | 8131 | `			if( pTm == 0 ){` |
|    ! 0 | 8132 | `				time(&t);` |
|    ! 0 | 8133 | `			}` |
|      2 | 8134 | `		}else{` |
|    ! 0 | 8135 | `			time(&t);` |
|      - | 8136 | `		}` |
|      3 | 8137 | `		pTm = localtime(&t);` |
|      3 | 8138 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8139 | `	}` |
|      - | 8140 | `	/* Format the given string */` |
|     33 | 8141 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|     33 | 8142 | `	return PH7_OK;` |
|     19 | 8143 |  |
|      - | 8144 | `/*` |
|      - | 8145 | ` * string strftime(string $format [, int $timestamp = time() ] )` |
|      - | 8146 | ` *  Format a local time/date (PLATFORM INDEPENDANT IMPLEENTATION NOT BASED ON LOCALE)` |
|      - | 8147 | ` * Parameters` |
|      - | 8148 | ` *  $format` |
|      - | 8149 | ` *   The format of the outputted date string (See code above)` |
|      - | 8150 | ` * $timestamp` |
|      - | 8151 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|      - | 8152 | ` *   that defaults to the current local time if a timestamp is not given.` |
|      - | 8153 | ` *   In other words, it defaults to the value of time().` |
|      - | 8154 | ` * Return` |
|      - | 8155 | ` * Returns a string formatted according format using the given timestamp` |
|      - | 8156 | ` * or the current local time if no timestamp is given.` |
|      - | 8157 | ` */` |
|     20 | 8158 | `static int PH7_builtin_strftime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8159 |  |
|      - | 8160 | `	const char *zFormat;` |
|      - | 8161 | `	int nLen;` |
|      - | 8162 | `	Sytm sTm;` |
|     21 | 8163 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8164 | `		/* Missing/Invalid argument,return FALSE */` |
|      5 | 8165 | `		ph7_result_bool(pCtx,0);` |
|      5 | 8166 | `		return PH7_OK;` |
|      - | 8167 | `	}` |
|     17 | 8168 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8169 | `	if( nLen < 1 ){` |
|      - | 8170 | `		/* Don't bother processing return FALSE */` |
|    ! 0 | 8171 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8172 | `	}` |
|     17 | 8173 | `	if( nArg < 2 ){` |
|      - | 8174 | `#ifdef __WINNT__` |
|      - | 8175 | `		SYSTEMTIME sOS;` |
|      1 | 8176 | `		GetSystemTime(&sOS);` |
|      1 | 8177 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8178 | `#else` |
|      - | 8179 | `		struct tm *pTm;` |
|      - | 8180 | `		time_t t;` |
|     14 | 8181 | `		time(&t);` |
|     14 | 8182 | `		pTm = localtime(&t);` |
|     14 | 8183 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8184 | `#endif` |
|      8 | 8185 | `	}else{` |
|      - | 8186 | `		/* Use the given timestamp */` |
|      - | 8187 | `		time_t t;` |
|      - | 8188 | `		struct tm *pTm;` |
|      3 | 8189 | `		if( ph7_value_is_int(apArg[1]) ){` |
|      3 | 8190 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|      3 | 8191 | `			pTm = localtime(&t);` |
|      3 | 8192 | `			if( pTm == 0 ){` |
|    ! 0 | 8193 | `				time(&t);` |
|    ! 0 | 8194 | `			}` |
|      2 | 8195 | `		}else{` |
|    ! 0 | 8196 | `			time(&t);` |
|      - | 8197 | `		}` |
|      3 | 8198 | `		pTm = localtime(&t);` |
|      3 | 8199 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8200 | `	}` |
|      - | 8201 | `	/* Format the given string */` |
|     17 | 8202 | `	PH7_Strftime(pCtx,zFormat,nLen,&sTm);` |
|     17 | 8203 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|      - | 8204 | `		/* Nothing was formatted,return FALSE */` |
|    ! 0 | 8205 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8206 | `	}` |
|     17 | 8207 | `	return PH7_OK;` |
|     11 | 8208 |  |
|      - | 8209 | `/*` |
|      - | 8210 | ` * string gmdate(string $format [, int $timestamp = time() ] )` |
|      - | 8211 | ` *  Identical to the date() function except that the time returned` |
|      - | 8212 | ` *  is Greenwich Mean Time (GMT).` |
|      - | 8213 | ` * Parameters` |
|      - | 8214 | ` *  $format` |
|      - | 8215 | ` *  The format of the outputted date string (See code above)` |
|      - | 8216 | ` *  $timestamp` |
|      - | 8217 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|      - | 8218 | ` *   that defaults to the current local time if a timestamp is not given.` |
|      - | 8219 | ` *   In other words, it defaults to the value of time().` |
|      - | 8220 | ` * Return` |
|      - | 8221 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|      - | 8222 | ` */` |
|     16 | 8223 | `static int PH7_builtin_gmdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8224 |  |
|      - | 8225 | `	const char *zFormat;` |
|      - | 8226 | `	int nLen;` |
|      - | 8227 | `	Sytm sTm;` |
|     17 | 8228 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8229 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 | 8230 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8231 | `		return PH7_OK;` |
|      - | 8232 | `	}` |
|     15 | 8233 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 8234 | `	if( nLen < 1 ){` |
|      - | 8235 | `		/* Don't bother processing return the empty string */` |
|    ! 0 | 8236 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 8237 | `	}` |
|     15 | 8238 | `	if( nArg < 2 ){` |
|      - | 8239 | `#ifdef __WINNT__` |
|      - | 8240 | `		SYSTEMTIME sOS;` |
|      1 | 8241 | `		GetSystemTime(&sOS);` |
|      1 | 8242 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8243 | `#else` |
|      - | 8244 | `		struct tm *pTm;` |
|      - | 8245 | `		time_t t;` |
|     12 | 8246 | `		time(&t);` |
|     12 | 8247 | `		pTm = gmtime(&t);` |
|     12 | 8248 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8249 | `#endif` |
|      7 | 8250 | `	}else{` |
|      - | 8251 | `		/* Use the given timestamp */` |
|      - | 8252 | `		time_t t;` |
|      - | 8253 | `		struct tm *pTm;` |
|      3 | 8254 | `		if( ph7_value_is_int(apArg[1]) ){` |
|      3 | 8255 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|      3 | 8256 | `			pTm = gmtime(&t);` |
|      3 | 8257 | `			if( pTm == 0 ){` |
|    ! 0 | 8258 | `				time(&t);` |
|    ! 0 | 8259 | `			}` |
|      2 | 8260 | `		}else{` |
|    ! 0 | 8261 | `			time(&t);` |
|      - | 8262 | `		}` |
|      3 | 8263 | `		pTm = gmtime(&t);` |
|      3 | 8264 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8265 | `	}` |
|      - | 8266 | `	/* Format the given string */` |
|     15 | 8267 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|     15 | 8268 | `	return PH7_OK;` |
|      9 | 8269 |  |
|      - | 8270 | `/*` |
|      - | 8271 | ` * array localtime([ int $timestamp = time() [, bool $is_associative = false ]])` |
|      - | 8272 | ` *  Return the local time.` |
|      - | 8273 | ` * Parameter` |
|      - | 8274 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|      - | 8275 | ` *     that defaults to the current local time if a timestamp is not given.` |
|      - | 8276 | ` *     In other words, it defaults to the value of time().` |
|      - | 8277 | ` * $is_associative` |
|      - | 8278 | ` *   If set to FALSE or not supplied then the array is returned as a regular, numerically` |
|      - | 8279 | ` *   indexed array. If the argument is set to TRUE then localtime() returns an associative` |
|      - | 8280 | ` *   array containing all the different elements of the structure returned by the C function` |
|      - | 8281 | ` *   call to localtime. The names of the different keys of the associative array are as follows:` |
|      - | 8282 | ` *      "tm_sec" - seconds, 0 to 59` |
|      - | 8283 | ` *      "tm_min" - minutes, 0 to 59` |
|      - | 8284 | ` *      "tm_hour" - hours, 0 to 23` |
|      - | 8285 | ` *      "tm_mday" - day of the month, 1 to 31` |
|      - | 8286 | ` *      "tm_mon" - month of the year, 0 (Jan) to 11 (Dec)` |
|      - | 8287 | ` *      "tm_year" - years since 1900` |
|      - | 8288 | ` *      "tm_wday" - day of the week, 0 (Sun) to 6 (Sat)` |
|      - | 8289 | ` *      "tm_yday" - day of the year, 0 to 365` |
|      - | 8290 | ` *      "tm_isdst" - is daylight savings time in effect? Positive if yes, 0 if not, negative if unknown.` |
|      - | 8291 | ` * Returns` |
|      - | 8292 | ` *  An associative array of information related to the timestamp.` |
|      - | 8293 | ` */` |
|      8 | 8294 | `static int PH7_builtin_localtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8295 |  |
|      - | 8296 | `	ph7_value *pValue,*pArray;` |
|      9 | 8297 | `	int isAssoc = 0;` |
|      - | 8298 | `	Sytm sTm;` |
|      9 | 8299 | `	if( nArg < 1 ){` |
|      - | 8300 | `#ifdef __WINNT__` |
|      - | 8301 | `		SYSTEMTIME sOS;` |
|      1 | 8302 | `		GetSystemTime(&sOS); /* TODO(chems): GMT not local */` |
|      1 | 8303 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8304 | `#else` |
|      - | 8305 | `		struct tm *pTm;` |
|      - | 8306 | `		time_t t;` |
|      4 | 8307 | `		time(&t);` |
|      4 | 8308 | `		pTm = localtime(&t);` |
|      4 | 8309 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8310 | `#endif` |
|      3 | 8311 | `	}else{` |
|      - | 8312 | `		/* Use the given timestamp */` |
|      - | 8313 | `		time_t t;` |
|      - | 8314 | `		struct tm *pTm;` |
|      5 | 8315 | `		if( ph7_value_is_int(apArg[0]) ){` |
|      5 | 8316 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|      5 | 8317 | `			pTm = localtime(&t);` |
|      5 | 8318 | `			if( pTm == 0 ){` |
|    ! 0 | 8319 | `				time(&t);` |
|    ! 0 | 8320 | `			}` |
|      3 | 8321 | `		}else{` |
|    ! 0 | 8322 | `			time(&t);` |
|      - | 8323 | `		}` |
|      5 | 8324 | `		pTm = localtime(&t);` |
|      5 | 8325 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8326 | `	}` |
|      - | 8327 | `	/* Element value */` |
|      9 | 8328 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      9 | 8329 | `	if( pValue == 0 ){` |
|      - | 8330 | `		/* Return NULL */` |
|    ! 0 | 8331 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8332 | `		return PH7_OK;` |
|      - | 8333 | `	}` |
|      - | 8334 | `	/* Create a new array */` |
|      9 | 8335 | `	pArray = ph7_context_new_array(pCtx);` |
|      9 | 8336 | `	if( pArray == 0 ){` |
|      - | 8337 | `		/* Return NULL */` |
|    ! 0 | 8338 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8339 | `		return PH7_OK;` |
|      - | 8340 | `	}` |
|      9 | 8341 | `	if( nArg > 1 ){` |
|      3 | 8342 | `		isAssoc = ph7_value_to_bool(apArg[1]);` |
|      1 | 8343 | `	}` |
|      - | 8344 | `	/* Fill the array */` |
|      - | 8345 | `	/* Seconds */` |
|      9 | 8346 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|      9 | 8347 | `	if( isAssoc ){` |
|      3 | 8348 | `		ph7_array_add_strkey_elem(pArray,"tm_sec",pValue);` |
|      2 | 8349 | `	}else{` |
|      7 | 8350 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8351 | `	}` |
|      - | 8352 | `	/* Minutes */` |
|      9 | 8353 | `	ph7_value_int(pValue,sTm.tm_min);` |
|      9 | 8354 | `	if( isAssoc ){` |
|      3 | 8355 | `		ph7_array_add_strkey_elem(pArray,"tm_min",pValue);` |
|      2 | 8356 | `	}else{` |
|      7 | 8357 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8358 | `	}` |
|      - | 8359 | `	/* Hours */` |
|      9 | 8360 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|      9 | 8361 | `	if( isAssoc ){` |
|      3 | 8362 | `		ph7_array_add_strkey_elem(pArray,"tm_hour",pValue);` |
|      2 | 8363 | `	}else{` |
|      7 | 8364 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8365 | `	}` |
|      - | 8366 | `	/* mday */` |
|      9 | 8367 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|      9 | 8368 | `	if( isAssoc ){` |
|      3 | 8369 | `		ph7_array_add_strkey_elem(pArray,"tm_mday",pValue);` |
|      2 | 8370 | `	}else{` |
|      7 | 8371 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8372 | `	}` |
|      - | 8373 | `	/* mon */` |
|      9 | 8374 | `	ph7_value_int(pValue,sTm.tm_mon);` |
|      9 | 8375 | `	if( isAssoc ){` |
|      3 | 8376 | `		ph7_array_add_strkey_elem(pArray,"tm_mon",pValue);` |
|      2 | 8377 | `	}else{` |
|      7 | 8378 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8379 | `	}` |
|      - | 8380 | `	/* year since 1900 */` |
|      9 | 8381 | `	ph7_value_int(pValue,sTm.tm_year-1900);` |
|      9 | 8382 | `	if( isAssoc ){` |
|      3 | 8383 | `		ph7_array_add_strkey_elem(pArray,"tm_year",pValue);` |
|      2 | 8384 | `	}else{` |
|      7 | 8385 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8386 | `	}` |
|      - | 8387 | `	/* wday */` |
|      9 | 8388 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|      9 | 8389 | `	if( isAssoc ){` |
|      3 | 8390 | `		ph7_array_add_strkey_elem(pArray,"tm_wday",pValue);` |
|      2 | 8391 | `	}else{` |
|      7 | 8392 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8393 | `	}` |
|      - | 8394 | `	/* yday */` |
|      9 | 8395 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|      9 | 8396 | `	if( isAssoc ){` |
|      3 | 8397 | `		ph7_array_add_strkey_elem(pArray,"tm_yday",pValue);` |
|      2 | 8398 | `	}else{` |
|      7 | 8399 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8400 | `	}` |
|      - | 8401 | `	/* isdst */` |
|      - | 8402 | `#ifdef __WINNT__` |
|      - | 8403 | `#ifdef _MSC_VER` |
|      - | 8404 | `#ifndef _WIN32_WCE` |
|      1 | 8405 | `			_get_daylight(&sTm.tm_isdst);` |
|      - | 8406 | `#endif` |
|      - | 8407 | `#endif` |
|      - | 8408 | `#endif` |
|      9 | 8409 | `	ph7_value_int(pValue,sTm.tm_isdst);` |
|      9 | 8410 | `	if( isAssoc ){` |
|      3 | 8411 | `		ph7_array_add_strkey_elem(pArray,"tm_isdst",pValue);` |
|      2 | 8412 | `	}else{` |
|      7 | 8413 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8414 | `	}` |
|      - | 8415 | `	/* Return the array */` |
|      9 | 8416 | `	ph7_result_value(pCtx,pArray);` |
|      9 | 8417 | `	return PH7_OK;` |
|      5 | 8418 |  |
|      - | 8419 | `/*` |
|      - | 8420 | ` * int idate(string $format [, int $timestamp = time() ])` |
|      - | 8421 | ` *  Returns a number formatted according to the given format string` |
|      - | 8422 | ` *  using the given integer timestamp or the current local time if` |
|      - | 8423 | ` *  no timestamp is given. In other words, timestamp is optional and defaults` |
|      - | 8424 | ` *  to the value of time().` |
|      - | 8425 | ` *  Unlike the function date(), idate() accepts just one char in the format` |
|      - | 8426 | ` *  parameter.` |
|      - | 8427 | ` * $Parameters` |
|      - | 8428 | ` *  Supported format` |
|      - | 8429 | ` *   d 	Day of the month` |
|      - | 8430 | ` *   h 	Hour (12 hour format)` |
|      - | 8431 | ` *   H 	Hour (24 hour format)` |
|      - | 8432 | ` *   i 	Minutes` |
|      - | 8433 | ` *   I (uppercase i)1 if DST is activated, 0 otherwise` |
|      - | 8434 | ` *   L (uppercase l) returns 1 for leap year, 0 otherwise` |
|      - | 8435 | ` *   m 	Month number` |
|      - | 8436 | ` *   s 	Seconds` |
|      - | 8437 | ` *   t 	Days in current month` |
|      - | 8438 | ` *   U 	Seconds since the Unix Epoch - January 1 1970 00:00:00 UTC - this is the same as time()` |
|      - | 8439 | ` *   w 	Day of the week (0 on Sunday)` |
|      - | 8440 | ` *   W 	ISO-8601 week number of year, weeks starting on Monday` |
|      - | 8441 | ` *   y 	Year (1 or 2 digits - check note below)` |
|      - | 8442 | ` *   Y 	Year (4 digits)` |
|      - | 8443 | ` *   z 	Day of the year` |
|      - | 8444 | ` *   Z 	Timezone offset in seconds` |
|      - | 8445 | ` * $timestamp` |
|      - | 8446 | ` *  The optional timestamp parameter is an integer Unix timestamp that defaults` |
|      - | 8447 | ` *  to the current local time if a timestamp is not given. In other words, it defaults` |
|      - | 8448 | ` *  to the value of time().` |
|      - | 8449 | ` * Return` |
|      - | 8450 | ` *  An integer.` |
|      - | 8451 | ` */` |
|     42 | 8452 | `static int PH7_builtin_idate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 8453 |  |
|      - | 8454 | `	const char *zFormat;` |
|     44 | 8455 | `	ph7_int64 iVal = 0;` |
|      - | 8456 | `	int nLen;` |
|      - | 8457 | `	Sytm sTm;` |
|     44 | 8458 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8459 | `		/* Missing/Invalid argument,return -1 */` |
|      5 | 8460 | `		ph7_result_int(pCtx,-1);` |
|      5 | 8461 | `		return PH7_OK;` |
|      - | 8462 | `	}` |
|     40 | 8463 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     40 | 8464 | `	if( nLen < 1 ){` |
|      - | 8465 | `		/* Don't bother processing return -1*/` |
|    ! 0 | 8466 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 8467 | `	}` |
|     40 | 8468 | `	if( nArg < 2 ){` |
|      - | 8469 | `#ifdef __WINNT__` |
|      - | 8470 | `		SYSTEMTIME sOS;` |
|      2 | 8471 | `		GetSystemTime(&sOS);` |
|      2 | 8472 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8473 | `#else` |
|      - | 8474 | `		struct tm *pTm;` |
|      - | 8475 | `		time_t t;` |
|     28 | 8476 | `		time(&t);` |
|     28 | 8477 | `		pTm = localtime(&t);` |
|     28 | 8478 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8479 | `#endif` |
|     16 | 8480 | `	}else{` |
|      - | 8481 | `		/* Use the given timestamp */` |
|      - | 8482 | `		time_t t;` |
|      - | 8483 | `		struct tm *pTm;` |
|     11 | 8484 | `		if( ph7_value_is_int(apArg[1]) ){` |
|     11 | 8485 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|     11 | 8486 | `			pTm = localtime(&t);` |
|     11 | 8487 | `			if( pTm == 0 ){` |
|    ! 0 | 8488 | `				time(&t);` |
|    ! 0 | 8489 | `			}` |
|      6 | 8490 | `		}else{` |
|    ! 0 | 8491 | `			time(&t);` |
|      - | 8492 | `		}` |
|     11 | 8493 | `		pTm = localtime(&t);` |
|     11 | 8494 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8495 | `	}` |
|      - | 8496 | `	/* Perform the requested operation */` |
|     40 | 8497 | `	switch(zFormat[0]){` |
|      2 | 8498 | `	case 'd':` |
|      - | 8499 | `		/* Day of the month */` |
|      5 | 8500 | `		iVal = sTm.tm_mday;` |
|      5 | 8501 | `		break;` |
|    ! 0 | 8502 | `	case 'h':` |
|      - | 8503 | `		/*	Hour (12 hour format)*/` |
|    ! 0 | 8504 | `		iVal = 1 + (sTm.tm_hour % 12);` |
|    ! 0 | 8505 | `		break;` |
|      1 | 8506 | `	case 'H':` |
|      - | 8507 | `		/* Hour (24 hour format)*/` |
|      3 | 8508 | `		iVal = sTm.tm_hour;` |
|      3 | 8509 | `		break;` |
|      1 | 8510 | `	case 'i':` |
|      - | 8511 | `		/*Minutes*/` |
|      3 | 8512 | `		iVal = sTm.tm_min;` |
|      3 | 8513 | `		break;` |
|      1 | 8514 | `	case 'I':` |
|      - | 8515 | `		/*	returns 1 if DST is activated, 0 otherwise */` |
|      - | 8516 | `#ifdef __WINNT__` |
|      - | 8517 | `#ifdef _MSC_VER` |
|      - | 8518 | `#ifndef _WIN32_WCE` |
|      1 | 8519 | `			_get_daylight(&sTm.tm_isdst);` |
|      - | 8520 | `#endif` |
|      - | 8521 | `#endif` |
|      - | 8522 | `#endif` |
|      3 | 8523 | `		iVal = sTm.tm_isdst;` |
|      3 | 8524 | `		break;` |
|      1 | 8525 | `	case 'L':` |
|      - | 8526 | `		/* 	returns 1 for leap year, 0 otherwise */` |
|      3 | 8527 | `		iVal = IS_LEAP_YEAR(sTm.tm_year);` |
|      3 | 8528 | `		break;` |
|      2 | 8529 | `	case 'm':` |
|      - | 8530 | `		/* Month number*/` |
|      5 | 8531 | `		iVal = sTm.tm_mon;` |
|      5 | 8532 | `		break;` |
|      1 | 8533 | `	case 's':` |
|      - | 8534 | `		/*Seconds*/` |
|      3 | 8535 | `		iVal = sTm.tm_sec;` |
|      3 | 8536 | `		break;` |
|      1 | 8537 | `	case 't':{` |
|      - | 8538 | `		/*Days in current month*/` |
|      - | 8539 | `		static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|      3 | 8540 | `		int nDays = aMonDays[sTm.tm_mon % 12 ];` |
|      3 | 8541 | `		if( sTm.tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(sTm.tm_year) ){` |
|    ! 0 | 8542 | `			nDays = 28;` |
|    ! 0 | 8543 | `		}` |
|      3 | 8544 | `		iVal = nDays;` |
|      3 | 8545 | `		break;` |
|      - | 8546 | `			 }` |
|      1 | 8547 | `	case 'U':` |
|      - | 8548 | `		/*Seconds since the Unix Epoch*/` |
|      3 | 8549 | `		iVal = (ph7_int64)time(0);` |
|      3 | 8550 | `		break;` |
|      1 | 8551 | `	case 'w':` |
|      - | 8552 | `		/*	Day of the week (0 on Sunday) */` |
|      3 | 8553 | `		iVal = sTm.tm_wday;` |
|      3 | 8554 | `		break;` |
|      1 | 8555 | `	case 'W': {` |
|      - | 8556 | `		/* ISO-8601 week number of year, weeks starting on Monday */` |
|      - | 8557 | `		static const int aISO8601_local[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|      3 | 8558 | `		iVal = aISO8601_local[sTm.tm_wday % 7 ];` |
|      3 | 8559 | `		break;` |
|      - | 8560 | `			  }` |
|    ! 0 | 8561 | `	case 'y':` |
|      - | 8562 | `		/* Year (2 digits) */` |
|    ! 0 | 8563 | `		iVal = sTm.tm_year % 100;` |
|    ! 0 | 8564 | `		break;` |
|      3 | 8565 | `	case 'Y':` |
|      - | 8566 | `		/* Year (4 digits) */` |
|      7 | 8567 | `		iVal = sTm.tm_year;` |
|      7 | 8568 | `		break;` |
|      1 | 8569 | `	case 'z':` |
|      - | 8570 | `		/* Day of the year */` |
|      3 | 8571 | `		iVal = sTm.tm_yday;` |
|      3 | 8572 | `		break;` |
|      1 | 8573 | `	case 'Z':` |
|      - | 8574 | `		/*Timezone offset in seconds*/` |
|      3 | 8575 | `		iVal = sTm.tm_gmtoff;` |
|      3 | 8576 | `		break;` |
|      1 | 8577 | `	default:` |
|      - | 8578 | `		/* unknown format,throw a warning */` |
|      3 | 8579 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Unknown date format token");` |
|      2 | 8580 | `		break;` |
|      - | 8581 | `	}` |
|      - | 8582 | `	/* Return the time value */` |
|     40 | 8583 | `	ph7_result_int64(pCtx,iVal);` |
|     40 | 8584 | `	return PH7_OK;` |
|     23 | 8585 |  |
|      - | 8586 | `/*` |
|      - | 8587 | ` * int mktime/gmmktime([ int $hour = date("H") [, int $minute = date("i") [, int $second = date("s")` |
|      - | 8588 | ` *  [, int $month = date("n") [, int $day = date("j") [, int $year = date("Y") [, int $is_dst = -1 ]]]]]]] )` |
|      - | 8589 | ` *  Returns the Unix timestamp corresponding to the arguments given. This timestamp is a 64bit integer` |
|      - | 8590 | ` *  containing the number of seconds between the Unix Epoch (January 1 1970 00:00:00 GMT) and the time` |
|      - | 8591 | ` *  specified.` |
|      - | 8592 | ` *  Arguments may be left out in order from right to left; any arguments thus omitted will be set to` |
|      - | 8593 | ` *  the current value according to the local date and time.` |
|      - | 8594 | ` * Parameters` |
|      - | 8595 | ` * $hour` |
|      - | 8596 | ` *  The number of the hour relevant to the start of the day determined by month, day and year.` |
|      - | 8597 | ` *  Negative values reference the hour before midnight of the day in question. Values greater` |
|      - | 8598 | ` *  than 23 reference the appropriate hour in the following day(s).` |
|      - | 8599 | ` * $minute` |
|      - | 8600 | ` *  The number of the minute relevant to the start of the hour. Negative values reference` |
|      - | 8601 | ` *  the minute in the previous hour. Values greater than 59 reference the appropriate minute` |
|      - | 8602 | ` *  in the following hour(s).` |
|      - | 8603 | ` * $second` |
|      - | 8604 | ` *  The number of seconds relevant to the start of the minute. Negative values reference` |
|      - | 8605 | ` *  the second in the previous minute. Values greater than 59 reference the appropriate` |
|      - | 8606 | ` * second in the following minute(s).` |
|      - | 8607 | ` * $month` |
|      - | 8608 | ` *  The number of the month relevant to the end of the previous year. Values 1 to 12 reference` |
|      - | 8609 | ` *  the normal calendar months of the year in question. Values less than 1 (including negative values)` |
|      - | 8610 | ` *  reference the months in the previous year in reverse order, so 0 is December, -1 is November)...` |
|      - | 8611 | ` * $day` |
|      - | 8612 | ` *  The number of the day relevant to the end of the previous month. Values 1 to 28, 29, 30 or 31` |
|      - | 8613 | ` *  (depending upon the month) reference the normal days in the relevant month. Values less than 1` |
|      - | 8614 | ` *  (including negative values) reference the days in the previous month, so 0 is the last day` |
|      - | 8615 | ` *  of the previous month, -1 is the day before that, etc. Values greater than the number of days` |
|      - | 8616 | ` *  in the relevant month reference the appropriate day in the following month(s).` |
|      - | 8617 | ` * $year` |
|      - | 8618 | ` *  The number of the year, may be a two or four digit value, with values between 0-69 mapping` |
|      - | 8619 | ` *  to 2000-2069 and 70-100 to 1970-2000. On systems where time_t is a 32bit signed integer, as` |
|      - | 8620 | ` *  most common today, the valid range for year is somewhere between 1901 and 2038.` |
|      - | 8621 | ` * $is_dst` |
|      - | 8622 | ` *  This parameter can be set to 1 if the time is during daylight savings time (DST), 0 if it is not,` |
|      - | 8623 | ` *  or -1 (the default) if it is unknown whether the time is within daylight savings time or not.` |
|      - | 8624 | ` * Return` |
|      - | 8625 | ` *   mktime() returns the Unix timestamp of the arguments given.` |
|      - | 8626 | ` *   If the arguments are invalid, the function returns FALSE` |
|      - | 8627 | ` */` |
|      8 | 8628 | `static int PH7_builtin_mktime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8629 |  |
|      - | 8630 | `	const char *zFunction;` |
|      9 | 8631 | `	ph7_int64 iVal = 0;` |
|      - | 8632 | `	struct tm *pTm;` |
|      - | 8633 | `	time_t t;` |
|      - | 8634 | `	/* Extract function name */` |
|      9 | 8635 | `	zFunction = ph7_function_name(pCtx);` |
|      - | 8636 | `	/* Get the current time */` |
|      9 | 8637 | `	time(&t);` |
|      9 | 8638 | `	if( zFunction[0] == 'g' /* gmmktime */ ){` |
|      3 | 8639 | `		pTm = gmtime(&t);` |
|      2 | 8640 | `	}else{` |
|      - | 8641 | `		/* localtime */` |
|      7 | 8642 | `		pTm = localtime(&t);` |
|      - | 8643 | `	}` |
|      9 | 8644 | `	if( nArg > 0 ){` |
|      - | 8645 | `		int iTmp;` |
|      - | 8646 | `		/* Hour */` |
|      9 | 8647 | `		iTmp = ph7_value_to_int(apArg[0]);` |
|      9 | 8648 | `		pTm->tm_hour = iTmp;` |
|      9 | 8649 | `		if( nArg > 1 ){` |
|      - | 8650 | `			/* Minutes */` |
|      9 | 8651 | `			iTmp = ph7_value_to_int(apArg[1]);` |
|      9 | 8652 | `			pTm->tm_min = iTmp;` |
|      9 | 8653 | `			if( nArg > 2 ){` |
|      - | 8654 | `				/* Seconds */` |
|      9 | 8655 | `				iTmp = ph7_value_to_int(apArg[2]);` |
|      9 | 8656 | `				pTm->tm_sec = iTmp;` |
|      9 | 8657 | `				if( nArg > 3 ){` |
|      - | 8658 | `					/* Month */` |
|      9 | 8659 | `					iTmp = ph7_value_to_int(apArg[3]);` |
|      9 | 8660 | `					pTm->tm_mon = iTmp - 1;` |
|      9 | 8661 | `					if( nArg > 4 ){` |
|      - | 8662 | `						/* mday */` |
|      9 | 8663 | `						iTmp = ph7_value_to_int(apArg[4]);` |
|      9 | 8664 | `						pTm->tm_mday = iTmp;` |
|      9 | 8665 | `						if( nArg > 5 ){` |
|      - | 8666 | `							/* Year */` |
|      9 | 8667 | `							iTmp = ph7_value_to_int(apArg[5]);` |
|      9 | 8668 | `							if( iTmp > 1900 ){` |
|      9 | 8669 | `								iTmp -= 1900;` |
|      4 | 8670 | `							}` |
|      9 | 8671 | `							pTm->tm_year = iTmp;` |
|      9 | 8672 | `							if( nArg > 6 ){` |
|      - | 8673 | `								/* is_dst */` |
|    ! 0 | 8674 | `								iTmp = ph7_value_to_bool(apArg[6]);` |
|    ! 0 | 8675 | `								pTm->tm_isdst = iTmp;` |
|    ! 0 | 8676 | `							}` |
|      4 | 8677 | `						}` |
|      4 | 8678 | `					}` |
|      4 | 8679 | `				}` |
|      4 | 8680 | `			}` |
|      4 | 8681 | `		}` |
|      4 | 8682 | `	}` |
|      - | 8683 | `	/* Make the time */` |
|      9 | 8684 | `	iVal = (ph7_int64)mktime(pTm);` |
|      - | 8685 | `	/* Return the timesatmp as a 64bit integer */` |
|      9 | 8686 | `	ph7_result_int64(pCtx,iVal);` |
|      9 | 8687 | `	return PH7_OK;` |
|      1 | 8688 |  |
|      - | 8689 | `/*` |
|      - | 8690 | ` * Section:` |
|      - | 8691 | ` *    URL handling Functions.` |
|      - | 8692 | ` * Status:` |
|      - | 8693 | ` *    Stable.` |
|      - | 8694 | ` */` |
|      - | 8695 | `/*` |
|      - | 8696 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 8697 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 8698 | ` */` |
|   1026 | 8699 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 8700 |  |
|      - | 8701 | `	/* Store in the call context result buffer */` |
|   1028 | 8702 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 8703 | `	return SXRET_OK;` |
|      2 | 8704 |  |
|      - | 8705 | `/*` |
|      - | 8706 | ` * string base64_encode(string $data)` |
|      - | 8707 | ` * string convert_uuencode(string $data)` |
|      - | 8708 | ` *  Encodes data with MIME base64` |
|      - | 8709 | ` * Parameter` |
|      - | 8710 | ` *  $data` |
|      - | 8711 | ` *    Data to encode` |
|      - | 8712 | ` * Return` |
|      - | 8713 | ` *  Encoded data or FALSE on failure.` |
|      - | 8714 | ` */` |
|     10 | 8715 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8716 |  |
|      - | 8717 | `	const char *zIn;` |
|      - | 8718 | `	int nLen;` |
|     11 | 8719 | `	if( nArg < 1 ){` |
|      - | 8720 | `		/* Missing arguments,return FALSE */` |
|      5 | 8721 | `		ph7_result_bool(pCtx,0);` |
|      5 | 8722 | `		return PH7_OK;` |
|      - | 8723 | `	}` |
|      - | 8724 | `	/* Extract the input string */` |
|      7 | 8725 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8726 | `	if( nLen < 1 ){` |
|      - | 8727 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8728 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8729 | `		return PH7_OK;` |
|      - | 8730 | `	}` |
|      - | 8731 | `	/* Perform the BASE64 encoding */` |
|      7 | 8732 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 8733 | `	return PH7_OK;` |
|      6 | 8734 |  |
|      - | 8735 | `/*` |
|      - | 8736 | ` * string base64_decode(string $data)` |
|      - | 8737 | ` * string convert_uudecode(string $data)` |
|      - | 8738 | ` *  Decodes data encoded with MIME base64` |
|      - | 8739 | ` * Parameter` |
|      - | 8740 | ` *  $data` |
|      - | 8741 | ` *    Encoded data.` |
|      - | 8742 | ` * Return` |
|      - | 8743 | ` *  Returns the original data or FALSE on failure.` |
|      - | 8744 | ` */` |
|     36 | 8745 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 8746 |  |
|      - | 8747 | `	const char *zIn;` |
|      - | 8748 | `	int nLen;` |
|     38 | 8749 | `	if( nArg < 1 ){` |
|      - | 8750 | `		/* Missing arguments,return FALSE */` |
|      3 | 8751 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8752 | `		return PH7_OK;` |
|      - | 8753 | `	}` |
|      - | 8754 | `	/* Extract the input string */` |
|     36 | 8755 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 8756 | `	if( nLen < 1 ){` |
|      - | 8757 | `		/* Nothing to process,return FALSE */` |
|      3 | 8758 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8759 | `		return PH7_OK;` |
|      - | 8760 | `	}` |
|      - | 8761 | `	/* Perform the BASE64 decoding */` |
|     34 | 8762 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 8763 | `	return PH7_OK;` |
|     20 | 8764 |  |
|      - | 8765 | `/*` |
|      - | 8766 | ` * string urlencode(string $str)` |
|      - | 8767 | ` *  URL encoding` |
|      - | 8768 | ` * Parameter` |
|      - | 8769 | ` *  $data` |
|      - | 8770 | ` *   Input string.` |
|      - | 8771 | ` * Return` |
|      - | 8772 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 8773 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 8774 | ` *  encoded as plus (+) signs.` |
|      - | 8775 | ` */` |
|      6 | 8776 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8777 |  |
|      - | 8778 | `	const char *zIn;` |
|      - | 8779 | `	int nLen;` |
|      7 | 8780 | `	if( nArg < 1 ){` |
|      - | 8781 | `		/* Missing arguments,return FALSE */` |
|      3 | 8782 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8783 | `		return PH7_OK;` |
|      - | 8784 | `	}` |
|      - | 8785 | `	/* Extract the input string */` |
|      5 | 8786 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 8787 | `	if( nLen < 1 ){` |
|      - | 8788 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8789 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8790 | `		return PH7_OK;` |
|      - | 8791 | `	}` |
|      - | 8792 | `	/* Perform the URL encoding */` |
|      5 | 8793 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 8794 | `	return PH7_OK;` |
|      4 | 8795 |  |
|      - | 8796 | `/*` |
|      - | 8797 | ` * string urldecode(string $str)` |
|      - | 8798 | ` *  Decodes any %## encoding in the given string.` |
|      - | 8799 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 8800 | ` * Parameter` |
|      - | 8801 | ` *  $data` |
|      - | 8802 | ` *    Input string.` |
|      - | 8803 | ` * Return` |
|      - | 8804 | ` *  Decoded URL or FALSE on failure.` |
|      - | 8805 | ` */` |
|      8 | 8806 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8807 |  |
|      - | 8808 | `	const char *zIn;` |
|      - | 8809 | `	int nLen;` |
|      9 | 8810 | `	if( nArg < 1 ){` |
|      - | 8811 | `		/* Missing arguments,return FALSE */` |
|      3 | 8812 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8813 | `		return PH7_OK;` |
|      - | 8814 | `	}` |
|      - | 8815 | `	/* Extract the input string */` |
|      7 | 8816 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8817 | `	if( nLen < 1 ){` |
|      - | 8818 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8819 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8820 | `		return PH7_OK;` |
|      - | 8821 | `	}` |
|      - | 8822 | `	/* Perform the URL decoding */` |
|      7 | 8823 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 8824 | `	return PH7_OK;` |
|      5 | 8825 |  |
|      - | 8826 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8827 | `/* Table of the built-in functions */` |
|      - | 8828 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 8829 | `	   /* Variable handling functions */` |
|      - | 8830 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 8831 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 8832 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 8833 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 8834 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 8835 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 8836 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 8837 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 8838 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 8839 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 8840 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 8841 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 8842 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 8843 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 8844 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 8845 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 8846 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 8847 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 8848 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 8849 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8850 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 8851 | `	   /* Math functions */` |
|      - | 8852 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 8853 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 8854 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 8855 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 8856 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 8857 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 8858 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 8859 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 8860 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 8861 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 8862 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 8863 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 8864 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 8865 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 8866 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 8867 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 8868 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 8869 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 8870 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 8871 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 8872 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 8873 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 8874 | `	{ "round",    PH7_builtin_round        },` |
|      - | 8875 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 8876 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 8877 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 8878 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 8879 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 8880 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 8881 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 8882 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 8883 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8884 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8885 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 8886 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8887 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8888 | `	   /* String handling functions */` |
|      - | 8889 |  |
|      - | 8890 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 8891 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 8892 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 8893 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 8894 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 8895 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 8896 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 8897 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 8898 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 8899 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 8900 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 8901 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 8902 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 8903 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 8904 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 8905 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 8906 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 8907 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 8908 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 8909 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 8910 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 8911 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 8912 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 8913 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 8914 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 8915 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 8916 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 8917 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 8918 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 8919 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 8920 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 8921 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 8922 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 8923 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 8924 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 8925 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 8926 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 8927 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 8928 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 8929 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 8930 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 8931 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 8932 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 8933 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 8934 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 8935 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 8936 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 8937 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 8938 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 8939 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8940 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8941 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 8942 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 8943 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 8944 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 8945 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8946 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8947 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 8948 |  |
|      - | 8949 |  |
|      - | 8950 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 8951 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 8952 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 8953 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 8954 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 8955 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8956 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8957 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 8958 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 8959 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8960 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8961 |  |
|      - | 8962 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 8963 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 8964 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 8965 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 8966 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 8967 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 8968 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 8969 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 8970 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 8971 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 8972 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 8973 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 8974 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8975 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8976 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 8977 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8978 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8979 |  |
|      - | 8980 | `	         /* Ctype functions */` |
|      - | 8981 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 8982 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 8983 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 8984 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 8985 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 8986 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 8987 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 8988 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 8989 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 8990 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 8991 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 8992 | `	         /* Time functions */` |
|      - | 8993 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 8994 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 8995 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 8996 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 8997 | `	{ "date",        PH7_builtin_date         },` |
|      - | 8998 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 8999 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 9000 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 9001 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 9002 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 9003 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 9004 | `	        /* URL functions */` |
|      - | 9005 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 9006 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 9007 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 9008 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 9009 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 9010 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 9011 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 9012 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 9013 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9014 | `};` |
|      - | 9015 | `/*` |
|      - | 9016 | ` * Register the built-in functions defined above,the array functions` |
|      - | 9017 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 9018 | ` */` |
|   1384 | 9019 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      2 | 9020 |  |
|      - | 9021 | `	sxu32 n;` |
| 211754 | 9022 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 210370 | 9023 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 105186 | 9024 | `	}` |
|      - | 9025 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   1386 | 9026 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 9027 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   1386 | 9028 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   1386 | 9029 |  |
|      - | 9030 |  |
