# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3332/3748 lines (88.90%)

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
|    122 |  152 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  153 |  |
|    124 |  154 | `	int res = 0; /* Assume false by default */` |
|    124 |  155 | `	if( nArg > 0 ){` |
|    122 |  156 | `		res = ph7_value_is_array(apArg[0]);` |
|     60 |  157 | `	}` |
|      - |  158 | `	/* Query result */` |
|    124 |  159 | `	ph7_result_bool(pCtx,res);` |
|    124 |  160 | `	return PH7_OK;` |
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
|  18770 |  269 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  270 |  |
|  18772 |  271 | `	int res = 1; /* Assume empty by default */` |
|  18772 |  272 | `	if( nArg > 0 ){` |
|  18770 |  273 | `		res = ph7_value_is_empty(apArg[0]);` |
|   9384 |  274 | `	}` |
|  18772 |  275 | `	ph7_result_bool(pCtx,res);` |
|  18772 |  276 | `	return PH7_OK;` |
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
| 131536 | 1288 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1289 |  |
|      - | 1290 | `	const char *zSource,*zOfft;` |
|      - | 1291 | `	int nOfft,nLen,nSrcLen;` |
| 131538 | 1292 | `	if( nArg < 2 ){` |
|      - | 1293 | `		/* return FALSE */` |
|      5 | 1294 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1295 | `		return PH7_OK;` |
|      - | 1296 | `	}` |
|      - | 1297 | `	/* Extract the target string */` |
| 131534 | 1298 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 131534 | 1299 | `	if( nSrcLen < 1 ){` |
|      - | 1300 | `		/* Empty string,return FALSE */` |
|   8056 | 1301 | `		ph7_result_bool(pCtx,0);` |
|   8056 | 1302 | `		return PH7_OK;` |
|      - | 1303 | `	}` |
| 123480 | 1304 | `	nLen = nSrcLen; /* cc warning */` |
|      - | 1305 | `	/* Extract the offset */` |
| 123480 | 1306 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 123480 | 1307 | `	if( nOfft < 0 ){` |
|  21178 | 1308 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  21178 | 1309 | `		if( zOfft < zSource ){` |
|      - | 1310 | `			/* Invalid offset */` |
|      5 | 1311 | `			ph7_result_bool(pCtx,0);` |
|      5 | 1312 | `			return PH7_OK;` |
|      - | 1313 | `		}` |
|  21174 | 1314 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  21174 | 1315 | `		nOfft = (int)(zOfft-zSource);` |
| 112890 | 1316 | `	}else if( nOfft >= nSrcLen ){` |
|      - | 1317 | `		/* Invalid offset */` |
|      7 | 1318 | `		ph7_result_bool(pCtx,0);` |
|      7 | 1319 | `		return PH7_OK;` |
|    ! 0 | 1320 | `	}else{` |
| 102298 | 1321 | `		zOfft = &zSource[nOfft];` |
| 102298 | 1322 | `		nLen = nSrcLen - nOfft;` |
|      - | 1323 | `	}` |
| 123470 | 1324 | `	if( nArg > 2 ){` |
|      - | 1325 | `		/* Extract the length */` |
| 102296 | 1326 | `		nLen = ph7_value_to_int(apArg[2]);` |
| 102296 | 1327 | `		if( nLen == 0 ){` |
|      - | 1328 | `			/* Invalid length,return an empty string */` |
|      5 | 1329 | `			ph7_result_string(pCtx,"",0);` |
|      5 | 1330 | `			return PH7_OK;` |
| 102292 | 1331 | `		}else if( nLen < 0 ){` |
|  21176 | 1332 | `			nLen = nSrcLen + nLen - nOfft;` |
|  21176 | 1333 | `			if( nLen < 1 ){` |
|      - | 1334 | `				/* Invalid  length */` |
|      3 | 1335 | `				nLen = nSrcLen - nOfft;` |
|      1 | 1336 | `			}` |
|  10587 | 1337 | `		}` |
| 102292 | 1338 | `		if( nLen + nOfft > nSrcLen ){` |
|      - | 1339 | `			/* Invalid length */` |
|   2492 | 1340 | `			nLen = nSrcLen - nOfft;` |
|   1245 | 1341 | `		}` |
|  51145 | 1342 | `	}` |
|      - | 1343 | `	/* Return the substring */` |
| 123466 | 1344 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 123466 | 1345 | `	return PH7_OK;` |
|  65770 | 1346 |  |
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
|   2308 | 2315 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2316 |  |
|   2310 | 2317 | `	int iLen = 0;` |
|   2310 | 2318 | `	if( nArg > 0 ){` |
|   2308 | 2319 | `		ph7_value_to_string(apArg[0],&iLen);` |
|   1153 | 2320 | `	}` |
|      - | 2321 | `	/* String length */` |
|   2310 | 2322 | `	ph7_result_int(pCtx,iLen);` |
|   2310 | 2323 | `	return PH7_OK;` |
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
|  88584 | 2468 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      2 | 2469 |  |
|  44292 | 2470 | `	SXUNUSED(pKey);` |
|  88586 | 2471 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 2472 | `	const char *zData;` |
|      - | 2473 | `	int nLen;` |
|  88586 | 2474 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
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
|  88584 | 2491 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 2492 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
|  88584 | 2493 | `	if( pData->bFirst ){` |
|  21392 | 2494 | `		pData->bFirst = 0;` |
|  77889 | 2495 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 2496 | `		/* append the separator first */` |
|  67182 | 2497 | `		ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|  33590 | 2498 | `	}` |
|      - | 2499 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
|  88584 | 2500 | `	if( nLen > 0 ){` |
|  80530 | 2501 | `		ph7_result_string(pData->pCtx,zData,nLen);` |
|  40264 | 2502 | `	}` |
|  88584 | 2503 | `	return PH7_OK;` |
|  44294 | 2504 |  |
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
|  21418 | 2518 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2519 |  |
|      - | 2520 | `	struct implode_data imp_data;` |
|  21420 | 2521 | `	int i = 1;` |
|  21420 | 2522 | `	if( nArg < 1 ){` |
|      - | 2523 | `		/* Missing argument,return NULL */` |
|    ! 0 | 2524 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2525 | `		return PH7_OK;` |
|      - | 2526 | `	}` |
|      - | 2527 | `	/* Prepare the implode context */` |
|  21420 | 2528 | `	imp_data.pCtx = pCtx;` |
|  21420 | 2529 | `	imp_data.bRecursive = 0;` |
|  21420 | 2530 | `	imp_data.bFirst = 1;` |
|  21420 | 2531 | `	imp_data.nRecCount = 0;` |
|  21420 | 2532 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  21418 | 2533 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  10710 | 2534 | `	}else{` |
|      3 | 2535 | `		imp_data.zSep = 0;` |
|      3 | 2536 | `		imp_data.nSeplen = 0;` |
|      3 | 2537 | `		i = 0;` |
|      - | 2538 | `	}` |
|  21420 | 2539 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 2540 | `	/* Start the 'join' process */` |
|  42838 | 2541 | `	while( i < nArg ){` |
|  21420 | 2542 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2543 | `			/* Iterate throw array entries */` |
|  21420 | 2544 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|  10711 | 2545 | `		}else{` |
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
|  21420 | 2561 | `		i++;` |
|      2 | 2562 | `	}` |
|  21420 | 2563 | `	return PH7_OK;` |
|  10711 | 2564 |  |
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
|   3946 | 2653 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2654 |  |
|      - | 2655 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 2656 | `	int nDelim,nStrlen,iLimit;` |
|      - | 2657 | `	ph7_value *pArray;` |
|      - | 2658 | `	ph7_value *pValue;` |
|      - | 2659 | `	sxu32 nOfft;` |
|      - | 2660 | `	sxi32 rc;` |
|   3948 | 2661 | `	if( nArg < 2 ){` |
|      - | 2662 | `		/* Missing arguments,return FALSE */` |
|      9 | 2663 | `		ph7_result_bool(pCtx,0);` |
|      9 | 2664 | `		return PH7_OK;` |
|      - | 2665 | `	}` |
|      - | 2666 | `	/* Extract the delimiter */` |
|   3940 | 2667 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   3940 | 2668 | `	if( nDelim < 1 ){` |
|      - | 2669 | `		/* Empty delimiter,return FALSE */` |
|      3 | 2670 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2671 | `		return PH7_OK;` |
|      - | 2672 | `	}` |
|      - | 2673 | `	/* Extract the string */` |
|   3938 | 2674 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   3938 | 2675 | `	if( nStrlen < 1 ){` |
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
|   3936 | 2690 | `	zEnd = &zString[nStrlen];` |
|      - | 2691 | `	/* Create the array */` |
|   3936 | 2692 | `	pArray =  ph7_context_new_array(pCtx);` |
|   3936 | 2693 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   3936 | 2694 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2695 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 2696 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2697 | `		return PH7_OK;` |
|      - | 2698 | `	}` |
|      - | 2699 | `	/* Set a defualt limit */` |
|   3936 | 2700 | `	iLimit = SXI32_HIGH;` |
|   3936 | 2701 | `	if( nArg > 2 ){` |
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
|  44606 | 2712 | `	for(;;){` |
|  89214 | 2713 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
|  89214 | 2714 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 2715 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   3936 | 2716 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   3936 | 2717 | `			ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|   3936 | 2718 | `			break;` |
|      - | 2719 | `		}` |
|      - | 2720 | `		/* Point to the desired offset */` |
|  85280 | 2721 | `		zCur = &zString[nOfft];` |
|      - | 2722 | `		/* Perform the store operation (may be empty) */` |
|  85280 | 2723 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|  85280 | 2724 | `		ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|      - | 2725 | `		/* Point beyond the delimiter */` |
|  85280 | 2726 | `		zString = &zCur[nDelim];` |
|      - | 2727 | `		/* Reset the cursor */` |
|  85280 | 2728 | `		ph7_value_reset_string_cursor(pValue);` |
|      2 | 2729 | `	}` |
|      - | 2730 | `	/* Return the freshly created array */` |
|   3936 | 2731 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2732 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 2733 | `	 * released as soon we return from this foregin function.` |
|      - | 2734 | `	 */` |
|   3936 | 2735 | `	return PH7_OK;` |
|   1975 | 2736 |  |
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
|   9438 | 2752 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2753 |  |
|      - | 2754 | `	const char *zString;` |
|      - | 2755 | `	int nLen;` |
|   9440 | 2756 | `	if( nArg < 1 ){` |
|      - | 2757 | `		/* Missing arguments,return null */` |
|      3 | 2758 | `		ph7_result_null(pCtx);` |
|      3 | 2759 | `		return PH7_OK;` |
|      - | 2760 | `	}` |
|      - | 2761 | `	/* Extract the target string */` |
|   9438 | 2762 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|   9438 | 2763 | `	if( nLen < 1 ){` |
|      - | 2764 | `		/* Empty string,return */` |
|   1598 | 2765 | `		ph7_result_string(pCtx,"",0);` |
|   1598 | 2766 | `		return PH7_OK;` |
|      - | 2767 | `	}` |
|      - | 2768 | `	/* Start the trim process */` |
|   7842 | 2769 | `	if( nArg < 2 ){` |
|      - | 2770 | `		SyString sStr;` |
|      - | 2771 | `		/* Remove white spaces and NUL bytes */` |
|   7838 | 2772 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  18894 | 2773 | `		SyStringFullTrimSafe(&sStr);` |
|   7838 | 2774 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   3920 | 2775 | `	}else{` |
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
|   7842 | 2829 | `	return PH7_OK;` |
|   4721 | 2830 |  |
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
|  21176 | 2994 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2995 |  |
|      - | 2996 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2997 | `	int nLen;` |
|  21178 | 2998 | `	if( nArg < 1 ){` |
|      - | 2999 | `		/* Missing arguments,return null */` |
|      3 | 3000 | `		ph7_result_null(pCtx);` |
|      3 | 3001 | `		return PH7_OK;` |
|      - | 3002 | `	}` |
|      - | 3003 | `	/* Extract the target string */` |
|  21176 | 3004 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  21176 | 3005 | `	if( nLen < 1 ){` |
|      - | 3006 | `		/* Empty string,return */` |
|      3 | 3007 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3008 | `		return PH7_OK;` |
|      - | 3009 | `	}` |
|      - | 3010 | `	/* Perform the requested operation */` |
|  21174 | 3011 | `	zEnd = &zString[nLen];` |
|  66860 | 3012 | `	for(;;){` |
| 133722 | 3013 | `		if( zString >= zEnd ){` |
|      - | 3014 | `			/* No more input,break immediately */` |
|  21174 | 3015 | `			break;` |
|      - | 3016 | `		}` |
| 112550 | 3017 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 3018 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 3019 | `			zCur = zString;` |
|    ! 0 | 3020 | `			zString++;` |
|    ! 0 | 3021 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 3022 | `				zString++;` |
|    ! 0 | 3023 | `			}` |
|      - | 3024 | `			/* Append UTF-8 stream */` |
|    ! 0 | 3025 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 3026 | `		}else{` |
| 112550 | 3027 | `			int c = zString[0];` |
| 112550 | 3028 | `			if( SyisUpper(c) ){` |
| 112548 | 3029 | `				c = SyToLower(zString[0]);` |
|  56273 | 3030 | `			}` |
|      - | 3031 | `			/* Append character */` |
| 112550 | 3032 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 3033 | `			/* Advance the cursor */` |
| 112550 | 3034 | `			zString++;` |
|      - | 3035 | `		}` |
|      2 | 3036 | `	}` |
|  21174 | 3037 | `	return PH7_OK;` |
|  10590 | 3038 |  |
|      - | 3039 | `/*` |
|      - | 3040 | ` * string strtolower(string $str)` |
|      - | 3041 | ` *  Make a string uppercase.` |
|      - | 3042 | ` * Parameters` |
|      - | 3043 | ` *  $str` |
|      - | 3044 | ` *   The input string.` |
|      - | 3045 | ` * Returns.` |
|      - | 3046 | ` *  The uppercased string.` |
|      - | 3047 | ` */` |
|     14 | 3048 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3049 |  |
|      - | 3050 | `	const char *zString,*zCur,*zEnd;` |
|      - | 3051 | `	int nLen;` |
|     15 | 3052 | `	if( nArg < 1 ){` |
|      - | 3053 | `		/* Missing arguments,return null */` |
|      3 | 3054 | `		ph7_result_null(pCtx);` |
|      3 | 3055 | `		return PH7_OK;` |
|      - | 3056 | `	}` |
|      - | 3057 | `	/* Extract the target string */` |
|     13 | 3058 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 3059 | `	if( nLen < 1 ){` |
|      - | 3060 | `		/* Empty string,return */` |
|      3 | 3061 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3062 | `		return PH7_OK;` |
|      - | 3063 | `	}` |
|      - | 3064 | `	/* Perform the requested operation */` |
|     11 | 3065 | `	zEnd = &zString[nLen];` |
|     31 | 3066 | `	for(;;){` |
|     63 | 3067 | `		if( zString >= zEnd ){` |
|      - | 3068 | `			/* No more input,break immediately */` |
|     11 | 3069 | `			break;` |
|      - | 3070 | `		}` |
|     53 | 3071 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 3072 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 3073 | `			zCur = zString;` |
|    ! 0 | 3074 | `			zString++;` |
|    ! 0 | 3075 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 3076 | `				zString++;` |
|    ! 0 | 3077 | `			}` |
|      - | 3078 | `			/* Append UTF-8 stream */` |
|    ! 0 | 3079 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 3080 | `		}else{` |
|     53 | 3081 | `			int c = zString[0];` |
|     53 | 3082 | `			if( SyisLower(c) ){` |
|     47 | 3083 | `				c = SyToUpper(zString[0]);` |
|     23 | 3084 | `			}` |
|      - | 3085 | `			/* Append character */` |
|     53 | 3086 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 3087 | `			/* Advance the cursor */` |
|     53 | 3088 | `			zString++;` |
|      - | 3089 | `		}` |
|      1 | 3090 | `	}` |
|     11 | 3091 | `	return PH7_OK;` |
|      8 | 3092 |  |
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
|      - | 3177 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 3178 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 3179 | ` * Parameters` |
|      - | 3180 | ` *  $string` |
|      - | 3181 | ` *   The input string.` |
|      - | 3182 | ` * Returns` |
|      - | 3183 | ` *  The ASCII value as an integer.` |
|      - | 3184 | ` */` |
|     62 | 3185 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3186 |  |
|      - | 3187 | `	const char *zString;` |
|      - | 3188 | `	int nLen,c;` |
|      - | 3189 | `	/* PHP requires exactly one argument. */` |
|     64 | 3190 | `	if( nArg != 1 ){` |
|      7 | 3191 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3192 | `			"ArgumentCountError",` |
|      - | 3193 | `			"ord() expects exactly 1 argument, %d given",` |
|      2 | 3194 | `			nArg` |
|      - | 3195 | `			);` |
|      - | 3196 | `	}` |
|      - | 3197 | `	/* Passing null is deprecated (E_DEPRECATED).  PHP emits this before` |
|      - | 3198 | `	 * the empty-string deprecation, so we check null first. */` |
|     59 | 3199 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 3200 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 3201 | `			"ord(): Passing null to parameter #1 ($character) "` |
|      - | 3202 | `			"of type string is deprecated"` |
|      - | 3203 | `			);` |
|      1 | 3204 | `	}` |
|      - | 3205 | `	/* Extract the target string */` |
|     59 | 3206 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     59 | 3207 | `	if( nLen < 1 ){` |
|      - | 3208 | `		/* Empty string is deprecated (E_DEPRECATED). */` |
|      5 | 3209 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 3210 | `			"ord(): Providing an empty string is deprecated"` |
|      - | 3211 | `			);` |
|      5 | 3212 | `		ph7_result_int(pCtx,0);` |
|      5 | 3213 | `		return PH7_OK;` |
|      - | 3214 | `	}` |
|      - | 3215 | `	/* A string longer than one byte is deprecated (E_DEPRECATED). */` |
|     55 | 3216 | `	if( nLen > 1 ){` |
|      7 | 3217 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 3218 | `			"ord(): Providing a string that is not one byte long "` |
|      - | 3219 | `			"is deprecated. Use ord($str[0]) instead"` |
|      - | 3220 | `			);` |
|      3 | 3221 | `	}` |
|      - | 3222 | `	/* Extract the ASCII value of the first character */` |
|     55 | 3223 | `	c = (unsigned char)zString[0];` |
|      - | 3224 | `	/* Return that value */` |
|     55 | 3225 | `	ph7_result_int(pCtx,c);` |
|     55 | 3226 | `	return PH7_OK;` |
|     33 | 3227 |  |
|      - | 3228 | `/*` |
|      - | 3229 | ` * string chr(int $codepoint)` |
|      - | 3230 | ` *  Returns a one-character string containing the character specified` |
|      - | 3231 | ` *  by the given codepoint.  Any integer is accepted; values outside` |
|      - | 3232 | ` *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.` |
|      - | 3233 | ` * Parameters` |
|      - | 3234 | ` *  $codepoint` |
|      - | 3235 | ` *   An integer codepoint.  Values outside 0-255 are deprecated and` |
|      - | 3236 | ` *   will be constrained to a single byte.` |
|      - | 3237 | ` * Returns` |
|      - | 3238 | ` *  A single-character string.` |
|      - | 3239 | ` */` |
|     44 | 3240 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3241 |  |
|      - | 3242 | `	int c;` |
|      - | 3243 | `	unsigned char ch;` |
|      - | 3244 | `	/* PHP requires exactly one argument. */` |
|     46 | 3245 | `	if( nArg != 1 ){` |
|      7 | 3246 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3247 | `			"ArgumentCountError",` |
|      - | 3248 | `			"chr() expects exactly 1 argument, %d given",` |
|      2 | 3249 | `			nArg` |
|      - | 3250 | `			);` |
|      - | 3251 | `	}` |
|      - | 3252 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 3253 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 3254 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 3255 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|     41 | 3256 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      - | 3257 | `		char zBuf[120];` |
|      4 | 3258 | `		SyBufferFormat(zBuf,sizeof(zBuf),` |
|      - | 3259 | `			"Implicit conversion from float %g to int loses precision",` |
|      1 | 3260 | `			ph7_value_to_double(apArg[0])` |
|      - | 3261 | `			);` |
|      3 | 3262 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zBuf);` |
|      1 | 3263 | `	}` |
|      - | 3264 | `	/* Extract the codepoint. */` |
|     41 | 3265 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 3266 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 3267 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 3268 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 3269 | `	 * name to avoid the API double-prefixing it. */` |
|     41 | 3270 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 3271 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 3272 | `			E_DEPRECATED,` |
|      - | 3273 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 3274 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 3275 | `			"The value used will be constrained using % 256"` |
|      - | 3276 | `			);` |
|      2 | 3277 | `	}` |
|      - | 3278 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 3279 | `	 * when taking the address of a wider int. */` |
|     41 | 3280 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 3281 | `	/* Return the specified character */` |
|     41 | 3282 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|     41 | 3283 | `	return PH7_OK;` |
|     24 | 3284 |  |
|      - | 3285 | `/*` |
|      - | 3286 | ` * Binary to hex consumer callback.` |
|      - | 3287 | ` * This callback is the default consumer used by the hash functions` |
|      - | 3288 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 3289 | ` */` |
|    226 | 3290 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 3291 |  |
|      - | 3292 | `	/* Append hex chunk verbatim */` |
|    227 | 3293 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|    227 | 3294 | `	return SXRET_OK;` |
|      1 | 3295 |  |
|      - | 3296 |  |
|      - | 3297 | `/*` |
|      - | 3298 | ` * string bin2hex(string $str)` |
|      - | 3299 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 3300 | ` * Parameters` |
|      - | 3301 | ` *  $str` |
|      - | 3302 | ` *   The input string.` |
|      - | 3303 | ` * Returns.` |
|      - | 3304 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 3305 | ` */` |
|     12 | 3306 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3307 |  |
|      - | 3308 | `	const char *zString;` |
|      - | 3309 | `	int nLen;` |
|     13 | 3310 | `	if( nArg < 1 ){` |
|      - | 3311 | `		/* Missing arguments,return null */` |
|      3 | 3312 | `		ph7_result_null(pCtx);` |
|      3 | 3313 | `		return PH7_OK;` |
|      - | 3314 | `	}` |
|      - | 3315 | `	/* Extract the target string */` |
|     11 | 3316 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 3317 | `	if( nLen < 1 ){` |
|      - | 3318 | `		/* Empty string,return */` |
|      3 | 3319 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3320 | `		return PH7_OK;` |
|      - | 3321 | `	}` |
|      - | 3322 | `	/* Perform the requested operation */` |
|      9 | 3323 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|      9 | 3324 | `	return PH7_OK;` |
|      7 | 3325 |  |
|      - | 3326 |  |
|      - | 3327 | `/* Search callback signature */` |
|      - | 3328 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 3329 | `/*` |
|      - | 3330 | ` * Case-insensitive pattern match.` |
|      - | 3331 | ` * Brute force is the default search method used here.` |
|      - | 3332 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 3333 | ` * well for short/medium texts on modern hardware.` |
|      - | 3334 | ` */` |
|    118 | 3335 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      1 | 3336 |  |
|    119 | 3337 | `	const char *zpIn = (const char *)pPattern;` |
|    119 | 3338 | `	const char *zIn = (const char *)pText;` |
|    119 | 3339 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    119 | 3340 | `	const char *zEnd = &zIn[nLen];` |
|      - | 3341 | `	const char *zPtr,*zPtr2;` |
|      - | 3342 | `	int c,d;` |
|    119 | 3343 | `	if( iPatLen > nLen ){` |
|      - | 3344 | `		/* Don't bother processing */` |
|     33 | 3345 | `		return SXERR_NOTFOUND;` |
|      - | 3346 | `	}` |
|    244 | 3347 | `	for(;;){` |
|    489 | 3348 | `		if( zIn >= zEnd ){` |
|     47 | 3349 | `			break;` |
|      - | 3350 | `		}` |
|    443 | 3351 | `		c = SyToLower(zIn[0]);` |
|    443 | 3352 | `		d = SyToLower(zpIn[0]);` |
|    443 | 3353 | `		if( c == d ){` |
|     41 | 3354 | `			zPtr   = &zIn[1];` |
|     41 | 3355 | `			zPtr2  = &zpIn[1];` |
|     71 | 3356 | `			for(;;){` |
|    143 | 3357 | `				if( zPtr2 >= zpEnd ){` |
|      - | 3358 | `					/* Pattern found */` |
|     41 | 3359 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 3360 | `					return SXRET_OK;` |
|      - | 3361 | `				}` |
|    103 | 3362 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 3363 | `					break;` |
|      - | 3364 | `				}` |
|    103 | 3365 | `				c = SyToLower(zPtr[0]);` |
|    103 | 3366 | `				d = SyToLower(zPtr2[0]);` |
|    103 | 3367 | `				if( c != d ){` |
|    ! 0 | 3368 | `					break;` |
|      - | 3369 | `				}` |
|    103 | 3370 | `				zPtr++; zPtr2++;` |
|      1 | 3371 | `			}` |
|    ! 0 | 3372 | `		}` |
|    403 | 3373 | `		zIn++;` |
|      1 | 3374 | `	}` |
|      - | 3375 | `	/* Pattern not found */` |
|     47 | 3376 | `	return SXERR_NOTFOUND;` |
|     60 | 3377 |  |
|      - | 3378 | `/*` |
|      - | 3379 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3380 | ` *  Find the first occurrence of a string.` |
|      - | 3381 | ` * Parameters` |
|      - | 3382 | ` *  $haystack` |
|      - | 3383 | ` *   The input string.` |
|      - | 3384 | ` * $needle` |
|      - | 3385 | ` *   Search pattern (must be a string).` |
|      - | 3386 | ` * $before_needle` |
|      - | 3387 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3388 | ` *   of the needle (excluding the needle).` |
|      - | 3389 | ` * Return` |
|      - | 3390 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3391 | ` */` |
|     10 | 3392 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3393 |  |
|     11 | 3394 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3395 | `	const char *zBlob,*zPattern;` |
|      - | 3396 | `	int nLen,nPatLen;` |
|      - | 3397 | `	sxu32 nOfft;` |
|      - | 3398 | `	sxi32 rc;` |
|     11 | 3399 | `	if( nArg < 2 ){` |
|      - | 3400 | `		/* Missing arguments,return FALSE */` |
|      5 | 3401 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3402 | `		return PH7_OK;` |
|      - | 3403 | `	}` |
|      - | 3404 | `	/* Extract the needle and the haystack */` |
|      7 | 3405 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 3406 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 3407 | `	nOfft = 0; /* cc warning */` |
|      9 | 3408 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3409 | `		int before = 0;` |
|      - | 3410 | `		/* Perform the lookup */` |
|      5 | 3411 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3412 | `		if( rc != SXRET_OK ){` |
|      - | 3413 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3414 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3415 | `			return PH7_OK;` |
|      - | 3416 | `		}` |
|      - | 3417 | `		/* Return the portion of the string */` |
|      5 | 3418 | `		if( nArg > 2 ){` |
|      3 | 3419 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3420 | `		}` |
|      5 | 3421 | `		if( before ){` |
|      3 | 3422 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3423 | `		}else{` |
|      3 | 3424 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3425 | `		}` |
|      3 | 3426 | `	}else{` |
|      3 | 3427 | `		ph7_result_bool(pCtx,0);` |
|      - | 3428 | `	}` |
|      7 | 3429 | `	return PH7_OK;` |
|      6 | 3430 |  |
|      - | 3431 | `/*` |
|      - | 3432 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3433 | ` *  Case-insensitive strstr().` |
|      - | 3434 | ` * Parameters` |
|      - | 3435 | ` *  $haystack` |
|      - | 3436 | ` *   The input string.` |
|      - | 3437 | ` * $needle` |
|      - | 3438 | ` *   Search pattern (must be a string).` |
|      - | 3439 | ` * $before_needle` |
|      - | 3440 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3441 | ` *   of the needle (excluding the needle).` |
|      - | 3442 | ` * Return` |
|      - | 3443 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3444 | ` */` |
|      6 | 3445 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3446 |  |
|      7 | 3447 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3448 | `	const char *zBlob,*zPattern;` |
|      - | 3449 | `	int nLen,nPatLen;` |
|      - | 3450 | `	sxu32 nOfft;` |
|      - | 3451 | `	sxi32 rc;` |
|      7 | 3452 | `	if( nArg < 2 ){` |
|      - | 3453 | `		/* Missing arguments,return FALSE */` |
|      3 | 3454 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3455 | `		return PH7_OK;` |
|      - | 3456 | `	}` |
|      - | 3457 | `	/* Extract the needle and the haystack */` |
|      5 | 3458 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3459 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 3460 | `	nOfft = 0; /* cc warning */` |
|      7 | 3461 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3462 | `		int before = 0;` |
|      - | 3463 | `		/* Perform the lookup */` |
|      5 | 3464 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3465 | `		if( rc != SXRET_OK ){` |
|      - | 3466 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3467 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3468 | `			return PH7_OK;` |
|      - | 3469 | `		}` |
|      - | 3470 | `		/* Return the portion of the string */` |
|      5 | 3471 | `		if( nArg > 2 ){` |
|      3 | 3472 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3473 | `		}` |
|      5 | 3474 | `		if( before ){` |
|      3 | 3475 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3476 | `		}else{` |
|      3 | 3477 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3478 | `		}` |
|      3 | 3479 | `	}else{` |
|    ! 0 | 3480 | `		ph7_result_bool(pCtx,0);` |
|      - | 3481 | `	}` |
|      5 | 3482 | `	return PH7_OK;` |
|      4 | 3483 |  |
|      - | 3484 | `/*` |
|      - | 3485 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3486 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 3487 | ` * Parameters` |
|      - | 3488 | ` *  $haystack` |
|      - | 3489 | ` *   The input string.` |
|      - | 3490 | ` * $needle` |
|      - | 3491 | ` *   Search pattern (must be a string).` |
|      - | 3492 | ` * $offset` |
|      - | 3493 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3494 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3495 | ` *   of haystack.` |
|      - | 3496 | ` * Return` |
|      - | 3497 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3498 | ` */` |
|     80 | 3499 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3500 |  |
|     82 | 3501 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3502 | `	const char *zBlob,*zPattern;` |
|      - | 3503 | `	int nLen,nPatLen,nStart;` |
|      - | 3504 | `	sxu32 nOfft;` |
|      - | 3505 | `	sxi32 rc;` |
|     82 | 3506 | `	if( nArg < 2 ){` |
|      - | 3507 | `		/* Missing arguments,return FALSE */` |
|      7 | 3508 | `		ph7_result_bool(pCtx,0);` |
|      7 | 3509 | `		return PH7_OK;` |
|      - | 3510 | `	}` |
|      - | 3511 | `	/* Extract the needle and the haystack */` |
|     76 | 3512 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     76 | 3513 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     76 | 3514 | `	nOfft = 0; /* cc warning */` |
|     76 | 3515 | `	nStart = 0;` |
|      - | 3516 | `	/* Peek the starting offset if available */` |
|     76 | 3517 | `	if( nArg > 2 ){` |
|    ! 0 | 3518 | `		nStart = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 3519 | `		if( nStart < 0 ){` |
|    ! 0 | 3520 | `			nStart = -nStart;` |
|    ! 0 | 3521 | `		}` |
|    ! 0 | 3522 | `		if( nStart >= nLen ){` |
|      - | 3523 | `			/* Invalid offset */` |
|    ! 0 | 3524 | `			nStart = 0;` |
|    ! 0 | 3525 | `		}else{` |
|    ! 0 | 3526 | `			zBlob += nStart;` |
|    ! 0 | 3527 | `			nLen -= nStart;` |
|      - | 3528 | `		}` |
|    ! 0 | 3529 | `	}` |
|     76 | 3530 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3531 | `		/* Perform the lookup */` |
|     74 | 3532 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     74 | 3533 | `		if( rc != SXRET_OK ){` |
|      - | 3534 | `			/* Pattern not found,return FALSE */` |
|      5 | 3535 | `			ph7_result_bool(pCtx,0);` |
|      5 | 3536 | `			return PH7_OK;` |
|      - | 3537 | `		}` |
|      - | 3538 | `		/* Return the pattern position */` |
|     70 | 3539 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|     36 | 3540 | `	}else{` |
|      3 | 3541 | `		ph7_result_bool(pCtx,0);` |
|      - | 3542 | `	}` |
|     72 | 3543 | `	return PH7_OK;` |
|     42 | 3544 |  |
|      - | 3545 | `/*` |
|      - | 3546 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3547 | ` *  Case-insensitive strpos.` |
|      - | 3548 | ` * Parameters` |
|      - | 3549 | ` *  $haystack` |
|      - | 3550 | ` *   The input string.` |
|      - | 3551 | ` * $needle` |
|      - | 3552 | ` *   Search pattern (must be a string).` |
|      - | 3553 | ` * $offset` |
|      - | 3554 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3555 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3556 | ` *   of haystack.` |
|      - | 3557 | ` * Return` |
|      - | 3558 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3559 | ` */` |
|     18 | 3560 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3561 |  |
|     19 | 3562 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3563 | `	const char *zBlob,*zPattern;` |
|      - | 3564 | `	int nLen,nPatLen,nStart;` |
|      - | 3565 | `	sxu32 nOfft;` |
|      - | 3566 | `	sxi32 rc;` |
|     19 | 3567 | `	if( nArg < 2 ){` |
|      - | 3568 | `		/* Missing arguments,return FALSE */` |
|      3 | 3569 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3570 | `		return PH7_OK;` |
|      - | 3571 | `	}` |
|      - | 3572 | `	/* Extract the needle and the haystack */` |
|     17 | 3573 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 3574 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     17 | 3575 | `	nOfft = 0; /* cc warning */` |
|     17 | 3576 | `	nStart = 0;` |
|      - | 3577 | `	/* Peek the starting offset if available */` |
|     17 | 3578 | `	if( nArg > 2 ){` |
|      5 | 3579 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 3580 | `		if( nStart < 0 ){` |
|      3 | 3581 | `			nStart = -nStart;` |
|      1 | 3582 | `		}` |
|      5 | 3583 | `		if( nStart >= nLen ){` |
|      - | 3584 | `			/* Invalid offset */` |
|    ! 0 | 3585 | `			nStart = 0;` |
|    ! 0 | 3586 | `		}else{` |
|      5 | 3587 | `			zBlob += nStart;` |
|      5 | 3588 | `			nLen -= nStart;` |
|      - | 3589 | `		}` |
|      2 | 3590 | `	}` |
|     17 | 3591 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3592 | `		/* Perform the lookup */` |
|     17 | 3593 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     17 | 3594 | `		if( rc != SXRET_OK ){` |
|      - | 3595 | `			/* Pattern not found,return FALSE */` |
|      3 | 3596 | `			ph7_result_bool(pCtx,0);` |
|      3 | 3597 | `			return PH7_OK;` |
|      - | 3598 | `		}` |
|      - | 3599 | `		/* Return the pattern position */` |
|     15 | 3600 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 3601 | `	}else{` |
|    ! 0 | 3602 | `		ph7_result_bool(pCtx,0);` |
|      - | 3603 | `	}` |
|     15 | 3604 | `	return PH7_OK;` |
|     10 | 3605 |  |
|      - | 3606 | `/*` |
|      - | 3607 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3608 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 3609 | ` * Parameters` |
|      - | 3610 | ` *  $haystack` |
|      - | 3611 | ` *   The input string.` |
|      - | 3612 | ` * $needle` |
|      - | 3613 | ` *   Search pattern (must be a string).` |
|      - | 3614 | ` * $offset` |
|      - | 3615 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3616 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3617 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3618 | ` * Return` |
|      - | 3619 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3620 | ` */` |
|     32 | 3621 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3622 |  |
|      - | 3623 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     33 | 3624 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3625 | `	int nLen,nPatLen;` |
|      - | 3626 | `	sxu32 nOfft;` |
|      - | 3627 | `	sxi32 rc;` |
|     33 | 3628 | `	if( nArg < 2 ){` |
|      - | 3629 | `		/* Missing arguments,return FALSE */` |
|      3 | 3630 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3631 | `		return PH7_OK;` |
|      - | 3632 | `	}` |
|      - | 3633 | `	/* Extract the needle and the haystack */` |
|     31 | 3634 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 3635 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3636 | `	/* Point to the end of the pattern */` |
|     31 | 3637 | `	zPtr = &zBlob[nLen - 1];` |
|     31 | 3638 | `	zEnd = &zBlob[nLen];` |
|      - | 3639 | `	/* Save the starting posistion */` |
|     31 | 3640 | `	zStart = zBlob;` |
|     31 | 3641 | `	nOfft = 0; /* cc warning */` |
|      - | 3642 | `	/* Peek the starting offset if available */` |
|     31 | 3643 | `	if( nArg > 2 ){` |
|      - | 3644 | `		int nStart;` |
|     21 | 3645 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 3646 | `		if( nStart < 0 ){` |
|     11 | 3647 | `			nStart = -nStart;` |
|     11 | 3648 | `			if( nStart >= nLen ){` |
|      - | 3649 | `				/* Invalid offset */` |
|      3 | 3650 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3651 | `				return PH7_OK;` |
|    ! 0 | 3652 | `			}else{` |
|      9 | 3653 | `				nLen -= nStart;` |
|      9 | 3654 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 3655 | `				zEnd = &zBlob[nLen];` |
|      - | 3656 | `			}` |
|      5 | 3657 | `		}else{` |
|     11 | 3658 | `			if( nStart >= nLen ){` |
|      - | 3659 | `				/* Invalid offset */` |
|      5 | 3660 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3661 | `				return PH7_OK;` |
|    ! 0 | 3662 | `			}else{` |
|      7 | 3663 | `				zBlob += nStart;` |
|      7 | 3664 | `				nLen -= nStart;` |
|      - | 3665 | `			}` |
|      - | 3666 | `		}` |
|      7 | 3667 | `	}` |
|     25 | 3668 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3669 | `		/* Perform the lookup */` |
|     57 | 3670 | `		for(;;){` |
|    115 | 3671 | `			if( zBlob >= zPtr ){` |
|     11 | 3672 | `				break;` |
|      - | 3673 | `			}` |
|    105 | 3674 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    105 | 3675 | `			if( rc == SXRET_OK ){` |
|      - | 3676 | `				/* Pattern found,return it's position */` |
|     13 | 3677 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 3678 | `				return PH7_OK;` |
|      - | 3679 | `			}` |
|     93 | 3680 | `			zPtr--;` |
|      1 | 3681 | `		}` |
|      - | 3682 | `		/* Pattern not found,return FALSE */` |
|     11 | 3683 | `		ph7_result_bool(pCtx,0);` |
|      6 | 3684 | `	}else{` |
|      3 | 3685 | `		ph7_result_bool(pCtx,0);` |
|      - | 3686 | `	}` |
|     13 | 3687 | `	return PH7_OK;` |
|     17 | 3688 |  |
|      - | 3689 | `/*` |
|      - | 3690 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3691 | ` *  Case-insensitive strrpos.` |
|      - | 3692 | ` * Parameters` |
|      - | 3693 | ` *  $haystack` |
|      - | 3694 | ` *   The input string.` |
|      - | 3695 | ` * $needle` |
|      - | 3696 | ` *   Search pattern (must be a string).` |
|      - | 3697 | ` * $offset` |
|      - | 3698 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3699 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3700 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3701 | ` * Return` |
|      - | 3702 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3703 | ` */` |
|     28 | 3704 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3705 |  |
|      - | 3706 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     29 | 3707 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3708 | `	int nLen,nPatLen;` |
|      - | 3709 | `	sxu32 nOfft;` |
|      - | 3710 | `	sxi32 rc;` |
|     29 | 3711 | `	if( nArg < 2 ){` |
|      - | 3712 | `		/* Missing arguments,return FALSE */` |
|      3 | 3713 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3714 | `		return PH7_OK;` |
|      - | 3715 | `	}` |
|      - | 3716 | `	/* Extract the needle and the haystack */` |
|     27 | 3717 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 3718 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3719 | `	/* Point to the end of the pattern */` |
|     27 | 3720 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 3721 | `	zEnd = &zBlob[nLen];` |
|      - | 3722 | `	/* Save the starting posistion */` |
|     27 | 3723 | `	zStart = zBlob;` |
|     27 | 3724 | `	nOfft = 0; /* cc warning */` |
|      - | 3725 | `	/* Peek the starting offset if available */` |
|     27 | 3726 | `	if( nArg > 2 ){` |
|      - | 3727 | `		int nStart;` |
|     15 | 3728 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3729 | `		if( nStart < 0 ){` |
|      7 | 3730 | `			nStart = -nStart;` |
|      7 | 3731 | `			if( nStart >= nLen ){` |
|      - | 3732 | `				/* Invalid offset */` |
|      3 | 3733 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3734 | `				return PH7_OK;` |
|    ! 0 | 3735 | `			}else{` |
|      5 | 3736 | `				nLen -= nStart;` |
|      5 | 3737 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 3738 | `				zEnd = &zBlob[nLen];` |
|      - | 3739 | `			}` |
|      3 | 3740 | `		}else{` |
|      9 | 3741 | `			if( nStart >= nLen ){` |
|      - | 3742 | `				/* Invalid offset */` |
|      5 | 3743 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3744 | `				return PH7_OK;` |
|    ! 0 | 3745 | `			}else{` |
|      5 | 3746 | `				zBlob += nStart;` |
|      5 | 3747 | `				nLen -= nStart;` |
|      - | 3748 | `			}` |
|      - | 3749 | `		}` |
|      4 | 3750 | `	}` |
|     21 | 3751 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3752 | `		/* Perform the lookup */` |
|     44 | 3753 | `		for(;;){` |
|     89 | 3754 | `			if( zBlob >= zPtr ){` |
|      9 | 3755 | `				break;` |
|      - | 3756 | `			}` |
|     81 | 3757 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 3758 | `			if( rc == SXRET_OK ){` |
|      - | 3759 | `				/* Pattern found,return it's position */` |
|     11 | 3760 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 3761 | `				return PH7_OK;` |
|      - | 3762 | `			}` |
|     71 | 3763 | `			zPtr--;` |
|      1 | 3764 | `		}` |
|      - | 3765 | `		/* Pattern not found,return FALSE */` |
|      9 | 3766 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3767 | `	}else{` |
|      3 | 3768 | `		ph7_result_bool(pCtx,0);` |
|      - | 3769 | `	}` |
|     11 | 3770 | `	return PH7_OK;` |
|     15 | 3771 |  |
|      - | 3772 | `/*` |
|      - | 3773 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3774 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3775 | ` * Parameters` |
|      - | 3776 | ` *  $haystack` |
|      - | 3777 | ` *   The input string.` |
|      - | 3778 | ` * $needle` |
|      - | 3779 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3780 | ` *  This behavior is different from that of strstr().` |
|      - | 3781 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3782 | ` *  as the ordinal value of a character.` |
|      - | 3783 | ` * Return` |
|      - | 3784 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3785 | ` */` |
|     24 | 3786 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3787 |  |
|      - | 3788 | `	const char *zBlob;` |
|      - | 3789 | `	int nLen,c;` |
|     25 | 3790 | `	if( nArg < 2 ){` |
|      - | 3791 | `		/* Missing arguments,return FALSE */` |
|      3 | 3792 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3793 | `		return PH7_OK;` |
|      - | 3794 | `	}` |
|      - | 3795 | `	/* Extract the haystack */` |
|     23 | 3796 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3797 | `	c = 0; /* cc warning */` |
|     23 | 3798 | `	if( nLen > 0 ){` |
|      - | 3799 | `		sxu32 nOfft;` |
|      - | 3800 | `		sxi32 rc;` |
|     21 | 3801 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 3802 | `			const char *zPattern;` |
|     11 | 3803 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 3804 | `														 * for NULL pointer.` |
|      - | 3805 | `														 */` |
|     11 | 3806 | `			c = zPattern[0];` |
|      6 | 3807 | `		}else{` |
|      - | 3808 | `			/* Int cast */` |
|     11 | 3809 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 3810 | `		}` |
|      - | 3811 | `		/* Perform the lookup */` |
|     21 | 3812 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3813 | `		if( rc != SXRET_OK ){` |
|      - | 3814 | `			/* No such entry,return FALSE */` |
|      7 | 3815 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3816 | `			return PH7_OK;` |
|      - | 3817 | `		}` |
|      - | 3818 | `		/* Return the string portion */` |
|     15 | 3819 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 3820 | `	}else{` |
|      3 | 3821 | `		ph7_result_bool(pCtx,0);` |
|      - | 3822 | `	}` |
|     17 | 3823 | `	return PH7_OK;` |
|     13 | 3824 |  |
|      - | 3825 | `/*` |
|      - | 3826 | ` * string strrev(string $string)` |
|      - | 3827 | ` *  Reverse a string.` |
|      - | 3828 | ` * Parameters` |
|      - | 3829 | ` *  $string` |
|      - | 3830 | ` *   String to be reversed.` |
|      - | 3831 | ` * Return` |
|      - | 3832 | ` *  The reversed string.` |
|      - | 3833 | ` */` |
|      4 | 3834 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3835 |  |
|      - | 3836 | `	const char *zIn,*zEnd;` |
|      - | 3837 | `	int nLen,c;` |
|      5 | 3838 | `	if( nArg < 1 ){` |
|      - | 3839 | `		/* Missing arguments,return NULL */` |
|      3 | 3840 | `		ph7_result_null(pCtx);` |
|      3 | 3841 | `		return PH7_OK;` |
|      - | 3842 | `	}` |
|      - | 3843 | `	/* Extract the target string */` |
|      3 | 3844 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3845 | `	if( nLen < 1 ){` |
|      - | 3846 | `		/* Empty string Return null */` |
|    ! 0 | 3847 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3848 | `		return PH7_OK;` |
|      - | 3849 | `	}` |
|      - | 3850 | `	/* Perform the requested operation */` |
|      3 | 3851 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3852 | `	for(;;){` |
|      9 | 3853 | `		if( zEnd < zIn ){` |
|      - | 3854 | `			/* No more input to process */` |
|      3 | 3855 | `			break;` |
|      - | 3856 | `		}` |
|      - | 3857 | `		/* Append current character */` |
|      7 | 3858 | `		c = zEnd[0];` |
|      7 | 3859 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3860 | `		zEnd--;` |
|      1 | 3861 | `	}` |
|      3 | 3862 | `	return PH7_OK;` |
|      3 | 3863 |  |
|      - | 3864 | `/*` |
|      - | 3865 | ` * string ucwords(string $string)` |
|      - | 3866 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3867 | ` *  The definition of a word is any string of characters that is immediately after` |
|      - | 3868 | ` *  a whitespace (These are: space, form-feed, newline, carriage return, horizontal tab, and vertical tab).` |
|      - | 3869 | ` * Parameters` |
|      - | 3870 | ` *  $string` |
|      - | 3871 | ` *   The input string.` |
|      - | 3872 | ` * Return` |
|      - | 3873 | ` *  The modified string..` |
|      - | 3874 | ` */` |
|     14 | 3875 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3876 |  |
|      - | 3877 | `	const char *zIn,*zCur,*zEnd;` |
|      - | 3878 | `	int nLen,c;` |
|     15 | 3879 | `	if( nArg < 1 ){` |
|      - | 3880 | `		/* Missing arguments,return NULL */` |
|      3 | 3881 | `		ph7_result_null(pCtx);` |
|      3 | 3882 | `		return PH7_OK;` |
|      - | 3883 | `	}` |
|      - | 3884 | `	/* Extract the target string */` |
|     13 | 3885 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 3886 | `	if( nLen < 1 ){` |
|      - | 3887 | `		/* Empty string – match PHP semantics */` |
|      3 | 3888 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3889 | `		return PH7_OK;` |
|      - | 3890 | `	}` |
|      - | 3891 | `	/* Perform the requested operation */` |
|     11 | 3892 | `	zEnd = &zIn[nLen];` |
|     21 | 3893 | `	for(;;){` |
|      - | 3894 | `		/* Jump leading white spaces */` |
|     43 | 3895 | `		zCur = zIn;` |
|     65 | 3896 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0x80 && SyisSpace(zIn[0]) ){` |
|     23 | 3897 | `			zIn++;` |
|      1 | 3898 | `		}` |
|     43 | 3899 | `		if( zCur < zIn ){` |
|      - | 3900 | `			/* Append white space stream */` |
|     23 | 3901 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 3902 | `		}` |
|     43 | 3903 | `		if( zIn >= zEnd ){` |
|      - | 3904 | `			/* No more input to process */` |
|     11 | 3905 | `			break;` |
|      - | 3906 | `		}` |
|     33 | 3907 | `		c = zIn[0];` |
|     33 | 3908 | `		if( c < 0x80 && SyisLower(c) ){` |
|     29 | 3909 | `			c = SyToUpper(c);` |
|     14 | 3910 | `		}` |
|      - | 3911 | `		/* Append the upper-cased character */` |
|     33 | 3912 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     33 | 3913 | `		zIn++;` |
|     33 | 3914 | `		zCur = zIn;` |
|      - | 3915 | `		/* Append the word varbatim */` |
|    149 | 3916 | `		while( zIn < zEnd ){` |
|    139 | 3917 | `			if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 3918 | `				/* UTF-8 stream */` |
|    ! 0 | 3919 | `				zIn++;` |
|    ! 0 | 3920 | `				SX_JMP_UTF8(zIn,zEnd);` |
|    138 | 3921 | `			}else if( !SyisSpace(zIn[0]) ){` |
|    117 | 3922 | `				zIn++;` |
|     59 | 3923 | `			}else{` |
|     23 | 3924 | `				break;` |
|      - | 3925 | `			}` |
|      1 | 3926 | `		}` |
|     33 | 3927 | `		if( zCur < zIn ){` |
|     33 | 3928 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 | 3929 | `		}` |
|      1 | 3930 | `	}` |
|     11 | 3931 | `	return PH7_OK;` |
|      8 | 3932 |  |
|      - | 3933 | `/*` |
|      - | 3934 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3935 | ` *  Returns input repeated multiplier times.` |
|      - | 3936 | ` * Parameters` |
|      - | 3937 | ` *  $string` |
|      - | 3938 | ` *   String to be repeated.` |
|      - | 3939 | ` * $multiplier` |
|      - | 3940 | ` *  Number of time the input string should be repeated.` |
|      - | 3941 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3942 | ` *  to 0, the function will return an empty string.` |
|      - | 3943 | ` * Return` |
|      - | 3944 | ` *  The repeated string.` |
|      - | 3945 | ` */` |
|  20212 | 3946 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3947 |  |
|      - | 3948 | `	const char *zIn;` |
|      - | 3949 | `	int nLen,nMul;` |
|      - | 3950 | `	int rc;` |
|  20213 | 3951 | `	if( nArg < 2 ){` |
|      - | 3952 | `		/* Missing arguments,return NULL */` |
|      3 | 3953 | `		ph7_result_null(pCtx);` |
|      3 | 3954 | `		return PH7_OK;` |
|      - | 3955 | `	}` |
|      - | 3956 | `	/* Extract the target string */` |
|  20211 | 3957 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|  20211 | 3958 | `	if( nLen < 1 ){` |
|      - | 3959 | `		/* Empty string.Return null */` |
|    ! 0 | 3960 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3961 | `		return PH7_OK;` |
|      - | 3962 | `	}` |
|      - | 3963 | `	/* Extract the multiplier */` |
|  20211 | 3964 | `	nMul = ph7_value_to_int(apArg[1]);` |
|  20211 | 3965 | `	if( nMul < 1 ){` |
|      - | 3966 | `		/* Return the empty string */` |
|      3 | 3967 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3968 | `		return PH7_OK;` |
|      - | 3969 | `	}` |
|      - | 3970 | `	/* Perform the requested operation */` |
| 120220 | 3971 | `	for(;;){` |
| 240441 | 3972 | `		if( !nMul ){` |
|  20209 | 3973 | `			break;` |
|      - | 3974 | `		}` |
|      - | 3975 | `		/* Append the copy */` |
| 220233 | 3976 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 220233 | 3977 | `		if( rc != PH7_OK ){` |
|      - | 3978 | `			/* Out of memory,break immediately */` |
|    ! 0 | 3979 | `			break;` |
|      - | 3980 | `		}` |
| 220233 | 3981 | `		nMul--;` |
|      1 | 3982 | `	}` |
|  20209 | 3983 | `	return PH7_OK;` |
|  10107 | 3984 |  |
|      - | 3985 | `/*` |
|      - | 3986 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3987 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3988 | ` * Parameters` |
|      - | 3989 | ` *  $string` |
|      - | 3990 | ` *   The input string.` |
|      - | 3991 | ` * $is_xhtml` |
|      - | 3992 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3993 | ` * Return` |
|      - | 3994 | ` *  The processed string.` |
|      - | 3995 | ` */` |
|      6 | 3996 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3997 |  |
|      - | 3998 | `	const char *zIn,*zCur,*zEnd;` |
|      7 | 3999 | `	int is_xhtml = 1; /* Default to XHTML-style '<br/>' like PHP */` |
|      - | 4000 | `	int nLen;` |
|      7 | 4001 | `	if( nArg < 1 ){` |
|      - | 4002 | `		/* Missing arguments,return the empty string */` |
|      3 | 4003 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4004 | `		return PH7_OK;` |
|      - | 4005 | `	}` |
|      - | 4006 | `	/* Extract the target string */` |
|      5 | 4007 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 4008 | `	if( nLen < 1 ){` |
|      - | 4009 | `		/* Empty string,return null */` |
|    ! 0 | 4010 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4011 | `		return PH7_OK;` |
|      - | 4012 | `	}` |
|      5 | 4013 | `	if( nArg > 1 ){` |
|      3 | 4014 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 4015 | `	}` |
|      5 | 4016 | `	zEnd = &zIn[nLen];` |
|      - | 4017 | `	/* Perform the requested operation */` |
|      4 | 4018 | `	for(;;){` |
|      9 | 4019 | `		zCur = zIn;` |
|      - | 4020 | `		/* Delimit the string */` |
|     21 | 4021 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 4022 | `			zIn++;` |
|      1 | 4023 | `		}` |
|      9 | 4024 | `		if( zCur < zIn ){` |
|      - | 4025 | `			/* Output chunk verbatim */` |
|      9 | 4026 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 4027 | `		}` |
|      9 | 4028 | `		if( zIn >= zEnd ){` |
|      - | 4029 | `			/* No more input to process */` |
|      5 | 4030 | `			break;` |
|      - | 4031 | `		}` |
|      - | 4032 | `		/* Output the HTML line break */` |
|      - | 4033 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br/>' (legacy without space), otherwise use '<br>' */` |
|      5 | 4034 | `		if( is_xhtml ){` |
|      3 | 4035 | `			ph7_result_string(pCtx,"<br/>",(int)sizeof("<br/>")-1);` |
|      2 | 4036 | `		}else{` |
|      3 | 4037 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 4038 | `		}` |
|      5 | 4039 | `		zCur = zIn;` |
|      - | 4040 | `		/* Append trailing line */` |
|     11 | 4041 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 4042 | `			zIn++;` |
|      1 | 4043 | `		}` |
|      5 | 4044 | `		if( zCur < zIn ){` |
|      - | 4045 | `			/* Output chunk verbatim */` |
|      5 | 4046 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 4047 | `		}` |
|      1 | 4048 | `	}` |
|      5 | 4049 | `	return PH7_OK;` |
|      4 | 4050 |  |
|      - | 4051 | `/*` |
|      - | 4052 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 4053 | ` *  According to the PHP reference manual.` |
|      - | 4054 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 4055 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 4056 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 4057 | ` * This applies to both sprintf() and printf().` |
|      - | 4058 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 4059 | ` * or more of these elements, in order:` |
|      - | 4060 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 4061 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 4062 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 4063 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 4064 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 4065 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 4066 | ` *   it with a single quote ('). See the examples below.` |
|      - | 4067 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 4068 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 4069 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 4070 | ` *   should result in.` |
|      - | 4071 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 4072 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 4073 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 4074 | ` *   limit to the string.` |
|      - | 4075 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 4076 | ` *       % - a literal percent character. No argument is required.` |
|      - | 4077 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 4078 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 4079 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 4080 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 4081 | ` * 	     for the number of digits after the decimal point.` |
|      - | 4082 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 4083 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 4084 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 4085 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 4086 | ` *       g - shorter of %e and %f.` |
|      - | 4087 | ` *       G - shorter of %E and %f.` |
|      - | 4088 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 4089 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 4090 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 4091 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 4092 | ` */` |
|      - | 4093 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4094 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 4095 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 4096 | `/*` |
|      - | 4097 | `** Conversion types fall into various categories as defined by the` |
|      - | 4098 | `** following enumeration.` |
|      - | 4099 | `*/` |
|      - | 4100 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 4101 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 4102 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 4103 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 4104 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 4105 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 4106 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 4107 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 4108 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 4109 |  |
|      - | 4110 | `/*` |
|      - | 4111 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 4112 | `*/` |
|      - | 4113 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 4114 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 4115 | `/*` |
|      - | 4116 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 4117 | `** by an instance of the following structure` |
|      - | 4118 | `*/` |
|      - | 4119 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 4120 | `struct ph7_fmt_info` |
|      - | 4121 |  |
|      - | 4122 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 4123 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 4124 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 4125 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 4126 | `  char *charset; /* The character set for conversion */` |
|      - | 4127 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 4128 | `};` |
|      - | 4129 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 4130 | `/*` |
|      - | 4131 | `** "*val" is a double such that 0.1 <= *val < 10.0` |
|      - | 4132 | `** Return the ascii code for the leading digit of *val, then` |
|      - | 4133 | `** multiply "*val" by 10.0 to renormalize.` |
|      - | 4134 | `**` |
|      - | 4135 | `** Example:` |
|      - | 4136 | `**     input:     *val = 3.14159` |
|      - | 4137 | `**     output:    *val = 1.4159    function return = '3'` |
|      - | 4138 | `**` |
|      - | 4139 | `** The counter *cnt is incremented each time.  After counter exceeds` |
|      - | 4140 | `** 16 (the number of significant digits in a 64-bit float) '0' is` |
|      - | 4141 | `** always returned.` |
|      - | 4142 | `*/` |
|    404 | 4143 | `static int vxGetdigit(sxlongreal *val,int *cnt)` |
|      1 | 4144 |  |
|      - | 4145 | `  sxlongreal d;` |
|      - | 4146 | `  int digit;` |
|      - | 4147 |  |
|    405 | 4148 | `  if( (*cnt)++ >= 16 ){` |
|    ! 0 | 4149 | `	  return '0';` |
|      - | 4150 | `  }` |
|    405 | 4151 | `  digit = (int)*val;` |
|    405 | 4152 | `  d = digit;` |
|    405 | 4153 | `   *val = (*val - d)*10.0;` |
|    405 | 4154 | `  return digit + '0' ;` |
|    203 | 4155 |  |
|      - | 4156 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      - | 4157 | `/*` |
|      - | 4158 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 4159 | ` * used conversion types first.` |
|      - | 4160 | ` */` |
|      - | 4161 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 4162 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 4163 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 4164 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 4165 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 4166 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 4167 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 4168 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 4169 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 4170 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4171 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4172 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 4173 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 4174 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 4175 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 4176 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 4177 | `};` |
|      - | 4178 | `/*` |
|      - | 4179 | ` * Format a given string.` |
|      - | 4180 | ` * The root program.  All variations call this core.` |
|      - | 4181 | ` * INPUTS:` |
|      - | 4182 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 4183 | ` *            1. A pointer to the call context.` |
|      - | 4184 | ` *            2. A pointer to the list of characters to be output` |
|      - | 4185 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 4186 | ` *            3. An integer number of characters to be output.` |
|      - | 4187 | ` *               (Note: This number might be zero.)` |
|      - | 4188 | ` *            4. Upper layer private data.` |
|      - | 4189 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 4190 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 4191 | ` */` |
|    120 | 4192 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 4193 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 4194 | `	ph7_context *pCtx,  /* call context */` |
|      - | 4195 | `	const char *zIn,    /* Format string */` |
|      - | 4196 | `	int nByte,          /* Format string length */` |
|      - | 4197 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 4198 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 4199 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 4200 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 4201 | `	)` |
|      1 | 4202 |  |
|    121 | 4203 | `	char spaces[] = "                                                  ";` |
|      - | 4204 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    121 | 4205 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 4206 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 4207 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 4208 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 4209 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 4210 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 4211 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 4212 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 4213 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 4214 | `	ph7_int64 iVal;` |
|      - | 4215 | `	int precision;           /* Precision of the current field */` |
|      - | 4216 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 4217 | `	int c,rc,n;` |
|      - | 4218 | `	int length;              /* Length of the field */` |
|      - | 4219 | `	int prefix;` |
|      - | 4220 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 4221 | `	int width;               /* Width of the current field */` |
|      - | 4222 | `	int idx;` |
|    121 | 4223 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 4224 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 4225 | `	/* Start the format process */` |
|    123 | 4226 | `	for(;;){` |
|    247 | 4227 | `		zCur = zIn;` |
|    697 | 4228 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|    451 | 4229 | `			zIn++;` |
|      1 | 4230 | `		}` |
|    247 | 4231 | `		if( zCur < zIn ){` |
|      - | 4232 | `			/* Consume chunk verbatim */` |
|     95 | 4233 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|     95 | 4234 | `			if( rc == SXERR_ABORT ){` |
|      - | 4235 | `				/* Callback request an operation abort */` |
|    ! 0 | 4236 | `				break;` |
|      - | 4237 | `			}` |
|     47 | 4238 | `		}` |
|    247 | 4239 | `		if( zIn >= zEnd ){` |
|      - | 4240 | `			/* No more input to process,break immediately */` |
|    119 | 4241 | `			break;` |
|      - | 4242 | `		}` |
|      - | 4243 | `		/* Find out what flags are present */` |
|    129 | 4244 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    128 | 4245 | `			flag_alternateform = flag_zeropad = 0;` |
|    129 | 4246 | `		zIn++; /* Jump the precent sign */` |
|     64 | 4247 | `		do{` |
|    157 | 4248 | `			c = zIn[0];` |
|    157 | 4249 | `			switch( c ){` |
|      9 | 4250 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      3 | 4251 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 4252 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|      9 | 4253 | `			case '#':   flag_alternateform = 1;   c = 0;   break;` |
|      5 | 4254 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 4255 | `			case '\'':` |
|    ! 0 | 4256 | `				zIn++;` |
|    ! 0 | 4257 | `				if( zIn < zEnd ){` |
|      - | 4258 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 4259 | `					c = zIn[0];` |
|    ! 0 | 4260 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4261 | `						spaces[idx] = (char)c;` |
|    ! 0 | 4262 | `					}` |
|    ! 0 | 4263 | `					c = 0;` |
|    ! 0 | 4264 | `				}` |
|    ! 0 | 4265 | `				break;` |
|    128 | 4266 | `			default:                                       break;` |
|      - | 4267 | `			}` |
|    157 | 4268 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 4269 | `		/* Get the field width */` |
|    129 | 4270 | `		width = 0;` |
|    223 | 4271 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     31 | 4272 | `			width = width*10 + (zIn[0] - '0');` |
|     31 | 4273 | `			zIn++;` |
|      1 | 4274 | `		}` |
|    129 | 4275 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 4276 | `			/* Position specifer */` |
|    ! 0 | 4277 | `			if( width > 0 ){` |
|    ! 0 | 4278 | `				n = width;` |
|    ! 0 | 4279 | `				if( vf && n > 0 ){` |
|    ! 0 | 4280 | `					n--;` |
|    ! 0 | 4281 | `				}` |
|    ! 0 | 4282 | `			}` |
|    ! 0 | 4283 | `			zIn++;` |
|    ! 0 | 4284 | `			width = 0;` |
|    ! 0 | 4285 | `			if( zIn < zEnd && zIn[0] == '0' ){` |
|    ! 0 | 4286 | `				flag_zeropad = 1;` |
|    ! 0 | 4287 | `				zIn++;` |
|    ! 0 | 4288 | `			}` |
|    ! 0 | 4289 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 4290 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 4291 | `				zIn++;` |
|    ! 0 | 4292 | `			}` |
|    ! 0 | 4293 | `		}` |
|    129 | 4294 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 4295 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 4296 | `		}` |
|      - | 4297 | `		/* Get the precision */` |
|    129 | 4298 | `		precision = -1;` |
|    129 | 4299 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     57 | 4300 | `			precision = 0;` |
|     57 | 4301 | `			zIn++;` |
|    145 | 4302 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     61 | 4303 | `				precision = precision*10 + (zIn[0] - '0');` |
|     61 | 4304 | `				zIn++;` |
|      1 | 4305 | `			}` |
|     28 | 4306 | `		}` |
|    129 | 4307 | `		if( zIn >= zEnd ){` |
|      - | 4308 | `			/* No more input */` |
|      3 | 4309 | `			break;` |
|      - | 4310 | `		}` |
|      - | 4311 | `		/* Fetch the info entry for the field */` |
|    127 | 4312 | `		pInfo = 0;` |
|    127 | 4313 | `		xtype = PH7_FMT_ERROR;` |
|    127 | 4314 | `		c = zIn[0];` |
|    127 | 4315 | `		zIn++; /* Jump the format specifer */` |
|    699 | 4316 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|    697 | 4317 | `			if( c==aFmt[idx].fmttype ){` |
|    125 | 4318 | `				pInfo = &aFmt[idx];` |
|    125 | 4319 | `				xtype = pInfo->type;` |
|    125 | 4320 | `				break;` |
|      - | 4321 | `			}` |
|    287 | 4322 | `		}` |
|    127 | 4323 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    127 | 4324 | `		length = 0;` |
|      - | 4325 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 4326 | `		 /*` |
|      - | 4327 | `		  ** At this point, variables are initialized as follows:` |
|      - | 4328 | `		  **` |
|      - | 4329 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 4330 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 4331 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 4332 | `		  **                               field width was negative.` |
|      - | 4333 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 4334 | `		  **                               the conversion character.` |
|      - | 4335 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 4336 | `		  **   width                       The specified field width.  This is` |
|      - | 4337 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 4338 | `		  **   precision                   The specified precision.  The default` |
|      - | 4339 | `		  **                               is -1.` |
|      - | 4340 | `		  */` |
|    127 | 4341 | `		switch(xtype){` |
|    ! 0 | 4342 | `		case PH7_FMT_PERCENT:` |
|      - | 4343 | `			/* A literal percent character */` |
|    ! 0 | 4344 | `			zWorker[0] = '%';` |
|    ! 0 | 4345 | `			length = (int)sizeof(char);` |
|    ! 0 | 4346 | `			break;` |
|      3 | 4347 | `		case PH7_FMT_CHARX:` |
|      - | 4348 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 4349 | `			 * with that ASCII value` |
|      - | 4350 | `			 */` |
|      7 | 4351 | `			pArg = NEXT_ARG;` |
|      7 | 4352 | `			if( pArg == 0 ){` |
|      3 | 4353 | `				c = 0;` |
|      2 | 4354 | `			}else{` |
|      5 | 4355 | `				c = ph7_value_to_int(pArg);` |
|      - | 4356 | `			}` |
|      - | 4357 | `			/* NUL byte is an acceptable value */` |
|      7 | 4358 | `			zWorker[0] = (char)c;` |
|      7 | 4359 | `			length = (int)sizeof(char);` |
|      7 | 4360 | `			break;` |
|     12 | 4361 | `		case PH7_FMT_STRING:` |
|      - | 4362 | `			/* the argument is treated as and presented as a string */` |
|     25 | 4363 | `			pArg = NEXT_ARG;` |
|     25 | 4364 | `			if( pArg == 0 ){` |
|    ! 0 | 4365 | `				length = 0;` |
|    ! 0 | 4366 | `			}else{` |
|     25 | 4367 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 4368 | `			}` |
|     25 | 4369 | `			if( length < 1 ){` |
|    ! 0 | 4370 | `				zBuf = " ";` |
|    ! 0 | 4371 | `				length = (int)sizeof(char);` |
|    ! 0 | 4372 | `			}` |
|     25 | 4373 | `			if( precision>=0 && precision<length ){` |
|      3 | 4374 | `				length = precision;` |
|      1 | 4375 | `			}` |
|     25 | 4376 | `			if( flag_zeropad ){` |
|      - | 4377 | `				/* zero-padding works on strings too */` |
|    ! 0 | 4378 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4379 | `					spaces[idx] = '0';` |
|    ! 0 | 4380 | `				}` |
|    ! 0 | 4381 | `			}` |
|     25 | 4382 | `			break;` |
|     20 | 4383 | `		case PH7_FMT_RADIX:` |
|     41 | 4384 | `			pArg = NEXT_ARG;` |
|     41 | 4385 | `			if( pArg == 0 ){` |
|    ! 0 | 4386 | `				iVal = 0;` |
|    ! 0 | 4387 | `			}else{` |
|     41 | 4388 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 4389 | `			}` |
|      - | 4390 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|     41 | 4391 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 4392 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 4393 | `			}` |
|      - | 4394 | `#if 1` |
|      - | 4395 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 4396 | `        ** I think this is stupid.*/` |
|     41 | 4397 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 4398 | `#else` |
|      - | 4399 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 4400 | `        ** but leave the prefix for hex.*/` |
|      - | 4401 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 4402 | `#endif` |
|     41 | 4403 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|     23 | 4404 | `          if( iVal<0 ){` |
|      3 | 4405 | `            iVal = -iVal;` |
|      - | 4406 | `			/* Ticket 1433-003 */` |
|      3 | 4407 | `			if( iVal < 0 ){` |
|      - | 4408 | `				/* Overflow */` |
|    ! 0 | 4409 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4410 | `			}` |
|      3 | 4411 | `            prefix = '-';` |
|     22 | 4412 | `          }else if( flag_plussign )  prefix = '+';` |
|     19 | 4413 | `          else if( flag_blanksign )  prefix = ' ';` |
|     17 | 4414 | `          else                       prefix = 0;` |
|     12 | 4415 | `        }else{` |
|     19 | 4416 | `			if( iVal<0 ){` |
|    ! 0 | 4417 | `				iVal = -iVal;` |
|      - | 4418 | `				/* Ticket 1433-003 */` |
|    ! 0 | 4419 | `				if( iVal < 0 ){` |
|      - | 4420 | `					/* Overflow */` |
|    ! 0 | 4421 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4422 | `				}` |
|    ! 0 | 4423 | `			}` |
|     19 | 4424 | `			prefix = 0;` |
|      - | 4425 | `		}` |
|     41 | 4426 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      3 | 4427 | `          precision = width-(prefix!=0);` |
|      1 | 4428 | `        }` |
|     41 | 4429 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 4430 | `        {` |
|      - | 4431 | `          register char *cset;      /* Use registers for speed */` |
|      - | 4432 | `          register int base;` |
|     41 | 4433 | `          cset = pInfo->charset;` |
|     41 | 4434 | `          base = pInfo->base;` |
|     20 | 4435 | `          do{                                           /* Convert to ascii */` |
|     79 | 4436 | `            *(--zBuf) = cset[iVal%base];` |
|     79 | 4437 | `            iVal = iVal/base;` |
|     79 | 4438 | `          }while( iVal>0 );` |
|      - | 4439 | `        }` |
|     41 | 4440 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     55 | 4441 | `        for(idx=precision-length; idx>0; idx--){` |
|     15 | 4442 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|      8 | 4443 | `        }` |
|     41 | 4444 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|     41 | 4445 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 4446 | `          char *pre, x;` |
|      9 | 4447 | `          pre = pInfo->prefix;` |
|      9 | 4448 | `          if( *zBuf!=pre[0] ){` |
|     23 | 4449 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|      4 | 4450 | `          }` |
|      4 | 4451 | `        }` |
|     41 | 4452 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     41 | 4453 | `		break;` |
|     27 | 4454 | `		case PH7_FMT_FLOAT:` |
|      - | 4455 | `		case PH7_FMT_EXP:` |
|      - | 4456 | `		case PH7_FMT_GENERIC:{` |
|      - | 4457 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 4458 | `		long double realvalue;` |
|      - | 4459 | `		int  exp;                /* exponent of real numbers */` |
|      - | 4460 | `		double rounder;          /* Used for rounding floating point values */` |
|      - | 4461 | `		int flag_dp;            /* True if decimal point should be shown */` |
|      - | 4462 | `		int flag_rtz;           /* True if trailing zeros should be removed */` |
|      - | 4463 | `		int flag_exp;           /* True to force display of the exponent */` |
|      - | 4464 | `		int nsd;                 /* Number of significant digits returned */` |
|     55 | 4465 | `		pArg = NEXT_ARG;` |
|     55 | 4466 | `		if( pArg == 0 ){` |
|    ! 0 | 4467 | `			realvalue = 0;` |
|    ! 0 | 4468 | `		}else{` |
|     55 | 4469 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 4470 | `		}` |
|      - | 4471 | `		/* Special-case NaN and infinities since the normal formatting logic` |
|      - | 4472 | `		 * below assumes a finite positive realvalue. */` |
|     55 | 4473 | `		if( PH7_IS_NAN(realvalue) ){` |
|    ! 0 | 4474 | `			zBuf = "NAN";` |
|    ! 0 | 4475 | `			length = 3;` |
|    ! 0 | 4476 | `			break;` |
|      - | 4477 | `		}` |
|     55 | 4478 | `		if( PH7_IS_INF(realvalue) ){` |
|      - | 4479 | `			/* Infinity prints as INF or -INF depending on sign. */` |
|    ! 0 | 4480 | `			if( realvalue < 0.0 ){` |
|    ! 0 | 4481 | `				zBuf = "-INF";` |
|    ! 0 | 4482 | `				length = 4;` |
|    ! 0 | 4483 | `			}else{` |
|    ! 0 | 4484 | `				zBuf = "INF";` |
|    ! 0 | 4485 | `				length = 3;` |
|      - | 4486 | `			}` |
|    ! 0 | 4487 | `			break;` |
|      - | 4488 | `		}` |
|     55 | 4489 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|     55 | 4490 | `		if( precision>PH7_FMT_BUFSIZ-40) precision = PH7_FMT_BUFSIZ-40;` |
|     55 | 4491 | `        if( realvalue<0.0 ){` |
|    ! 0 | 4492 | `          realvalue = -realvalue;` |
|    ! 0 | 4493 | `          prefix = '-';` |
|    ! 0 | 4494 | `        }else{` |
|     55 | 4495 | `          if( flag_plussign )          prefix = '+';` |
|     55 | 4496 | `          else if( flag_blanksign )    prefix = ' ';` |
|     55 | 4497 | `          else                         prefix = 0;` |
|      - | 4498 | `        }` |
|     55 | 4499 | `        if( pInfo->type==PH7_FMT_GENERIC && precision>0 ) precision--;` |
|     55 | 4500 | `        rounder = 0.0;` |
|      - | 4501 | `#if 0` |
|      - | 4502 | `        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */` |
|      - | 4503 | `        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);` |
|      - | 4504 | `#else` |
|      - | 4505 | `        /* It makes more sense to use 0.5 */` |
|    387 | 4506 | `        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);` |
|      - | 4507 | `#endif` |
|     55 | 4508 | `        if( pInfo->type==PH7_FMT_FLOAT ) realvalue += rounder;` |
|      - | 4509 | `        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */` |
|     55 | 4510 | `        exp = 0;` |
|     55 | 4511 | `        if( realvalue>0.0 ){` |
|     59 | 4512 | `          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }` |
|     87 | 4513 | `          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }` |
|     59 | 4514 | `          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }` |
|     69 | 4515 | `          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }` |
|     55 | 4516 | `          if( exp>350 \|\| exp<-350 ){` |
|    ! 0 | 4517 | `            zBuf = "NaN";` |
|    ! 0 | 4518 | `            length = 3;` |
|    ! 0 | 4519 | `            break;` |
|      - | 4520 | `          }` |
|     27 | 4521 | `        }` |
|     55 | 4522 | `        zBuf = zWorker;` |
|      - | 4523 | `        /*` |
|      - | 4524 | `        ** If the field type is etGENERIC, then convert to either etEXP` |
|      - | 4525 | `        ** or etFLOAT, as appropriate.` |
|      - | 4526 | `        */` |
|     55 | 4527 | `        flag_exp = xtype==PH7_FMT_EXP;` |
|     55 | 4528 | `        if( xtype!=PH7_FMT_FLOAT ){` |
|    ! 0 | 4529 | `          realvalue += rounder;` |
|    ! 0 | 4530 | `          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }` |
|    ! 0 | 4531 | `        }` |
|     55 | 4532 | `        if( xtype==PH7_FMT_GENERIC ){` |
|    ! 0 | 4533 | `          flag_rtz = !flag_alternateform;` |
|    ! 0 | 4534 | `          if( exp<-4 \|\| exp>precision ){` |
|    ! 0 | 4535 | `            xtype = PH7_FMT_EXP;` |
|    ! 0 | 4536 | `          }else{` |
|    ! 0 | 4537 | `            precision = precision - exp;` |
|    ! 0 | 4538 | `            xtype = PH7_FMT_FLOAT;` |
|      - | 4539 | `          }` |
|    ! 0 | 4540 | `        }else{` |
|     55 | 4541 | `          flag_rtz = 0;` |
|      - | 4542 | `        }` |
|      - | 4543 | `        /*` |
|      - | 4544 | `        ** The "exp+precision" test causes output to be of type etEXP if` |
|      - | 4545 | `        ** the precision is too large to fit in buf[].` |
|      - | 4546 | `        */` |
|     55 | 4547 | `        nsd = 0;` |
|     55 | 4548 | `        if( xtype==PH7_FMT_FLOAT && exp+precision<PH7_FMT_BUFSIZ-30 ){` |
|     55 | 4549 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|     55 | 4550 | `          if( prefix ) *(zBuf++) = (char)prefix;         /* Sign */` |
|     55 | 4551 | `          if( exp<0 )  *(zBuf++) = '0';            /* Digits before "." */` |
|    145 | 4552 | `          else for(; exp>=0; exp--) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     55 | 4553 | `          if( flag_dp ) *(zBuf++) = '.';           /* The decimal point */` |
|     87 | 4554 | `          for(exp++; exp<0 && precision>0; precision--, exp++){` |
|     33 | 4555 | `            *(zBuf++) = '0';` |
|     17 | 4556 | `          }` |
|    355 | 4557 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     55 | 4558 | `          *(zBuf--) = 0;                           /* Null terminate */` |
|     55 | 4559 | `          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */` |
|    ! 0 | 4560 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 4561 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 4562 | `          }` |
|     55 | 4563 | `          zBuf++;                            /* point to next free slot */` |
|     28 | 4564 | `        }else{    /* etEXP or etGENERIC */` |
|    ! 0 | 4565 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|    ! 0 | 4566 | `          if( prefix ) *(zBuf++) = (char)prefix;   /* Sign */` |
|    ! 0 | 4567 | `          *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);  /* First digit */` |
|    ! 0 | 4568 | `          if( flag_dp ) *(zBuf++) = '.';     /* Decimal point */` |
|    ! 0 | 4569 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|    ! 0 | 4570 | `          zBuf--;                            /* point to last digit */` |
|    ! 0 | 4571 | `          if( flag_rtz && flag_dp ){          /* Remove tail zeros */` |
|    ! 0 | 4572 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 4573 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 4574 | `          }` |
|    ! 0 | 4575 | `          zBuf++;                            /* point to next free slot */` |
|    ! 0 | 4576 | `          if( exp \|\| flag_exp ){` |
|    ! 0 | 4577 | `            *(zBuf++) = pInfo->charset[0];` |
|    ! 0 | 4578 | `            if( exp<0 ){ *(zBuf++) = '-'; exp = -exp; } /* sign of exp */` |
|    ! 0 | 4579 | `            else       { *(zBuf++) = '+'; }` |
|    ! 0 | 4580 | `            if( exp>=100 ){` |
|    ! 0 | 4581 | `              *(zBuf++) = (char)((exp/100)+'0');                /* 100's digit */` |
|    ! 0 | 4582 | `              exp %= 100;` |
|    ! 0 | 4583 | `            }` |
|    ! 0 | 4584 | `            *(zBuf++) = (char)(exp/10+'0');                     /* 10's digit */` |
|    ! 0 | 4585 | `            *(zBuf++) = (char)(exp%10+'0');                     /* 1's digit */` |
|    ! 0 | 4586 | `          }` |
|      - | 4587 | `        }` |
|      - | 4588 | `        /* The converted number is in buf[] and zero terminated.Output it.` |
|      - | 4589 | `        ** Note that the number is in the usual order, not reversed as with` |
|      - | 4590 | `        ** integer conversions.*/` |
|     55 | 4591 | `        length = (int)(zBuf-zWorker);` |
|     55 | 4592 | `        zBuf = zWorker;` |
|      - | 4593 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 4594 | `        ** set and we are not left justified */` |
|     55 | 4595 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 4596 | `          int i;` |
|      3 | 4597 | `          int nPad = width - length;` |
|     13 | 4598 | `          for(i=width; i>=nPad; i--){` |
|     11 | 4599 | `            zBuf[i] = zBuf[i-nPad];` |
|      6 | 4600 | `          }` |
|      3 | 4601 | `          i = prefix!=0;` |
|      5 | 4602 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      3 | 4603 | `          length = width;` |
|      1 | 4604 | `        }` |
|      - | 4605 | `#else` |
|      - | 4606 | `         zBuf = " ";` |
|      - | 4607 | `		 length = (int)sizeof(char);` |
|      - | 4608 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|     55 | 4609 | `		 break;` |
|      - | 4610 | `							 }` |
|      1 | 4611 | `		default:` |
|      - | 4612 | `			/* Invalid format specifer */` |
|      3 | 4613 | `			zWorker[0] = '?';` |
|      3 | 4614 | `			length = (int)sizeof(char);` |
|      2 | 4615 | `			break;` |
|      - | 4616 | `		}` |
|      - | 4617 | `		 /*` |
|      - | 4618 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 4619 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 4620 | `		 ** the output.` |
|      - | 4621 | `		 */` |
|    127 | 4622 | `    if( !flag_leftjustify ){` |
|      - | 4623 | `      register int nspace;` |
|    119 | 4624 | `      nspace = width-length;` |
|    119 | 4625 | `      if( nspace>0 ){` |
|      5 | 4626 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4627 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4628 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4629 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4630 | `			}` |
|    ! 0 | 4631 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4632 | `        }` |
|      5 | 4633 | `        if( nspace>0 ){` |
|      5 | 4634 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      5 | 4635 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4636 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4637 | `			}` |
|      2 | 4638 | `		}` |
|      2 | 4639 | `      }` |
|     59 | 4640 | `    }` |
|    127 | 4641 | `    if( length>0 ){` |
|    127 | 4642 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    127 | 4643 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4644 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4645 | `		}` |
|     63 | 4646 | `    }` |
|    127 | 4647 | `    if( flag_leftjustify ){` |
|      - | 4648 | `      register int nspace;` |
|      9 | 4649 | `      nspace = width-length;` |
|      9 | 4650 | `      if( nspace>0 ){` |
|      9 | 4651 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4652 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4653 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4654 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4655 | `			}` |
|    ! 0 | 4656 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4657 | `        }` |
|      9 | 4658 | `        if( nspace>0 ){` |
|      9 | 4659 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      9 | 4660 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4661 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4662 | `			}` |
|      4 | 4663 | `		}` |
|      4 | 4664 | `      }` |
|      4 | 4665 | `    }` |
|      1 | 4666 | ` }/* for(;;) */` |
|    121 | 4667 | `	return SXRET_OK;` |
|     61 | 4668 |  |
|      - | 4669 | `/*` |
|      - | 4670 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 4671 | ` */` |
|     84 | 4672 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4673 |  |
|      - | 4674 | `	/* Consume directly */` |
|     85 | 4675 | `	ph7_result_string(pCtx,zInput,nLen);` |
|     42 | 4676 | `	SXUNUSED(pUserData); /* cc warning */` |
|     85 | 4677 | `	return PH7_OK;` |
|      1 | 4678 |  |
|      - | 4679 | `/*` |
|      - | 4680 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 4681 | ` *  Return a formatted string.` |
|      - | 4682 | ` * Parameters` |
|      - | 4683 | ` *  $format` |
|      - | 4684 | ` *    The format string (see block comment above)` |
|      - | 4685 | ` * Return` |
|      - | 4686 | ` *  A string produced according to the formatting string format.` |
|      - | 4687 | ` */` |
|     56 | 4688 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4689 |  |
|      - | 4690 | `	const char *zFormat;` |
|      - | 4691 | `	int nLen;` |
|     57 | 4692 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4693 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 4694 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4695 | `		return PH7_OK;` |
|      - | 4696 | `	}` |
|      - | 4697 | `	/* Extract the string format */` |
|     55 | 4698 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     55 | 4699 | `	if( nLen < 1 ){` |
|      - | 4700 | `		/* Empty string */` |
|    ! 0 | 4701 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4702 | `		return PH7_OK;` |
|      - | 4703 | `	}` |
|      - | 4704 | `	/* Format the string */` |
|     55 | 4705 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,0,FALSE);` |
|     55 | 4706 | `	return PH7_OK;` |
|     29 | 4707 |  |
|      - | 4708 | `/*` |
|      - | 4709 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 4710 | ` */` |
|    110 | 4711 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4712 |  |
|    111 | 4713 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 4714 | `	/* Call the VM output consumer directly */` |
|    111 | 4715 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 4716 | `	/* Increment counter */` |
|    111 | 4717 | `	*pCounter += nLen;` |
|    111 | 4718 | `	return PH7_OK;` |
|      1 | 4719 |  |
|      - | 4720 | `/*` |
|      - | 4721 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 4722 | ` *  Output a formatted string.` |
|      - | 4723 | ` * Parameters` |
|      - | 4724 | ` *  $format` |
|      - | 4725 | ` *   See sprintf() for a description of format.` |
|      - | 4726 | ` * Return` |
|      - | 4727 | ` *  The length of the outputted string.` |
|      - | 4728 | ` */` |
|     42 | 4729 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4730 |  |
|     43 | 4731 | `	ph7_int64 nCounter = 0;` |
|      - | 4732 | `	const char *zFormat;` |
|      - | 4733 | `	int nLen;` |
|     43 | 4734 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4735 | `		/* Missing/Invalid arguments,return 0 */` |
|      3 | 4736 | `		ph7_result_int(pCtx,0);` |
|      3 | 4737 | `		return PH7_OK;` |
|      - | 4738 | `	}` |
|      - | 4739 | `	/* Extract the string format */` |
|     41 | 4740 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 4741 | `	if( nLen < 1 ){` |
|      - | 4742 | `		/* Empty string */` |
|    ! 0 | 4743 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4744 | `		return PH7_OK;` |
|      - | 4745 | `	}` |
|      - | 4746 | `	/* Format the string */` |
|     41 | 4747 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 4748 | `	/* Return the length of the outputted string */` |
|     41 | 4749 | `	ph7_result_int64(pCtx,nCounter);` |
|     41 | 4750 | `	return PH7_OK;` |
|     22 | 4751 |  |
|      - | 4752 | `/*` |
|      - | 4753 | ` * int vprintf(string $format,array $args)` |
|      - | 4754 | ` *  Output a formatted string.` |
|      - | 4755 | ` * Parameters` |
|      - | 4756 | ` *  $format` |
|      - | 4757 | ` *   See sprintf() for a description of format.` |
|      - | 4758 | ` * Return` |
|      - | 4759 | ` *  The length of the outputted string.` |
|      - | 4760 | ` */` |
|      2 | 4761 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4762 |  |
|      3 | 4763 | `	ph7_int64 nCounter = 0;` |
|      - | 4764 | `	const char *zFormat;` |
|      - | 4765 | `	ph7_hashmap *pMap;` |
|      - | 4766 | `	SySet sArg;` |
|      - | 4767 | `	int nLen,n;` |
|      3 | 4768 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4769 | `		/* Missing/Invalid arguments,return 0 */` |
|    ! 0 | 4770 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4771 | `		return PH7_OK;` |
|      - | 4772 | `	}` |
|      - | 4773 | `	/* Extract the string format */` |
|      3 | 4774 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4775 | `	if( nLen < 1 ){` |
|      - | 4776 | `		/* Empty string */` |
|    ! 0 | 4777 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4778 | `		return PH7_OK;` |
|      - | 4779 | `	}` |
|      - | 4780 | `	/* Point to the hashmap */` |
|      3 | 4781 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4782 | `	/* Extract arguments from the hashmap */` |
|      3 | 4783 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4784 | `	/* Format the string */` |
|      3 | 4785 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 4786 | `	/* Return the length of the outputted string */` |
|      3 | 4787 | `	ph7_result_int64(pCtx,nCounter);` |
|      - | 4788 | `	/* Release the container */` |
|      3 | 4789 | `	SySetRelease(&sArg);` |
|      3 | 4790 | `	return PH7_OK;` |
|      2 | 4791 |  |
|      - | 4792 | `/*` |
|      - | 4793 | ` * int vsprintf(string $format,array $args)` |
|      - | 4794 | ` *  Output a formatted string.` |
|      - | 4795 | ` * Parameters` |
|      - | 4796 | ` *  $format` |
|      - | 4797 | ` *   See sprintf() for a description of format.` |
|      - | 4798 | ` * Return` |
|      - | 4799 | ` *  A string produced according to the formatting string format.` |
|      - | 4800 | ` */` |
|     10 | 4801 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4802 |  |
|      - | 4803 | `	const char *zFormat;` |
|      - | 4804 | `	ph7_hashmap *pMap;` |
|      - | 4805 | `	SySet sArg;` |
|      - | 4806 | `	int nLen,n;` |
|     11 | 4807 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4808 | `		/* Missing/Invalid arguments,return the empty string */` |
|      5 | 4809 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 4810 | `		return PH7_OK;` |
|      - | 4811 | `	}` |
|      - | 4812 | `	/* Extract the string format */` |
|      7 | 4813 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4814 | `	if( nLen < 1 ){` |
|      - | 4815 | `		/* Empty string */` |
|    ! 0 | 4816 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4817 | `		return PH7_OK;` |
|      - | 4818 | `	}` |
|      - | 4819 | `	/* Point to hashmap */` |
|      7 | 4820 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4821 | `	/* Extract arguments from the hashmap */` |
|      7 | 4822 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4823 | `	/* Format the string */` |
|      7 | 4824 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),0,TRUE);` |
|      - | 4825 | `	/* Release the container */` |
|      7 | 4826 | `	SySetRelease(&sArg);` |
|      7 | 4827 | `	return PH7_OK;` |
|      6 | 4828 |  |
|      - | 4829 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 4830 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4831 | `/*` |
|      - | 4832 | ` * Symisc eXtension.` |
|      - | 4833 | ` * string size_format(int64 $size)` |
|      - | 4834 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 4835 | ` *  Example:` |
|      - | 4836 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 4837 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 4838 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 4839 | ` * Parameter` |
|      - | 4840 | ` *  $size` |
|      - | 4841 | ` *    Entity size in bytes.` |
|      - | 4842 | ` * Return` |
|      - | 4843 | ` *   Formatted string representation of the given size.` |
|      - | 4844 | ` */` |
|     24 | 4845 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4846 |  |
|      - | 4847 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 4848 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 4849 | `	sxi32 nRest,i_32;` |
|      - | 4850 | `	ph7_int64 iSize;` |
|     25 | 4851 | `	int c = -1; /* index in zUnit[] */` |
|      - | 4852 |  |
|     25 | 4853 | `	if( nArg < 1 ){` |
|      - | 4854 | `		/* Missing argument,return the empty string */` |
|      3 | 4855 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4856 | `		return PH7_OK;` |
|      - | 4857 | `	}` |
|      - | 4858 | `	/* Extract the given size */` |
|     23 | 4859 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 4860 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 4861 | `		/* Don't bother formatting,return immediately */` |
|      5 | 4862 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 4863 | `		return PH7_OK;` |
|      - | 4864 | `	}` |
|     19 | 4865 | `	for(;;){` |
|     39 | 4866 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 4867 | `		iSize >>= 10;` |
|     39 | 4868 | `		c++;` |
|     39 | 4869 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 4870 | `			break;` |
|      - | 4871 | `		}` |
|      1 | 4872 | `	}` |
|     19 | 4873 | `	nRest /= 100;` |
|     19 | 4874 | `	if( nRest > 9 ){` |
|    ! 0 | 4875 | `		nRest = 9;` |
|    ! 0 | 4876 | `	}` |
|     19 | 4877 | `	if( iSize > 999 ){` |
|    ! 0 | 4878 | `		c++;` |
|    ! 0 | 4879 | `		nRest = 9;` |
|    ! 0 | 4880 | `		iSize = 0;` |
|    ! 0 | 4881 | `	}` |
|     19 | 4882 | `	i_32 = (sxi32)iSize;` |
|      - | 4883 | `	/* Format */` |
|     19 | 4884 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 4885 | `	return PH7_OK;` |
|     13 | 4886 |  |
|      - | 4887 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4888 | `/*` |
|      - | 4889 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 4890 | ` *   Calculate the md5 hash of a string.` |
|      - | 4891 | ` * Parameter` |
|      - | 4892 | ` *  $str` |
|      - | 4893 | ` *   Input string` |
|      - | 4894 | ` * $raw_output` |
|      - | 4895 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4896 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4897 | ` * Return` |
|      - | 4898 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 4899 | ` */` |
|     10 | 4900 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4901 |  |
|      - | 4902 | `	unsigned char zDigest[16];` |
|     11 | 4903 | `	int raw_output = FALSE;` |
|      - | 4904 | `	const void *pIn;` |
|      - | 4905 | `	int nLen;` |
|     11 | 4906 | `	if( nArg < 1 ){` |
|      - | 4907 | `		/* Missing arguments,return the empty string */` |
|      3 | 4908 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4909 | `		return PH7_OK;` |
|      - | 4910 | `	}` |
|      - | 4911 | `	/* Extract the input string */` |
|      9 | 4912 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 4913 | `	if( nLen < 1 ){` |
|      - | 4914 | `		/* Empty string */` |
|    ! 0 | 4915 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4916 | `		return PH7_OK;` |
|      - | 4917 | `	}` |
|      9 | 4918 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      3 | 4919 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 | 4920 | `	}` |
|      - | 4921 | `	/* Compute the MD5 digest */` |
|      9 | 4922 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|      9 | 4923 | `	if( raw_output ){` |
|      - | 4924 | `		/* Output raw digest */` |
|      3 | 4925 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      2 | 4926 | `	}else{` |
|      - | 4927 | `		/* Perform a binary to hex conversion */` |
|      7 | 4928 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4929 | `	}` |
|      9 | 4930 | `	return PH7_OK;` |
|      6 | 4931 |  |
|      - | 4932 | `/*` |
|      - | 4933 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 4934 | ` *   Calculate the sha1 hash of a string.` |
|      - | 4935 | ` * Parameter` |
|      - | 4936 | ` *  $str` |
|      - | 4937 | ` *   Input string` |
|      - | 4938 | ` * $raw_output` |
|      - | 4939 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4940 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4941 | ` * Return` |
|      - | 4942 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 4943 | ` */` |
|      8 | 4944 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4945 |  |
|      - | 4946 | `	unsigned char zDigest[20];` |
|      9 | 4947 | `	int raw_output = FALSE;` |
|      - | 4948 | `	const void *pIn;` |
|      - | 4949 | `	int nLen;` |
|      9 | 4950 | `	if( nArg < 1 ){` |
|      - | 4951 | `		/* Missing arguments,return the empty string */` |
|      3 | 4952 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4953 | `		return PH7_OK;` |
|      - | 4954 | `	}` |
|      - | 4955 | `	/* Extract the input string */` |
|      7 | 4956 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4957 | `	if( nLen < 1 ){` |
|      - | 4958 | `		/* Empty string */` |
|    ! 0 | 4959 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4960 | `		return PH7_OK;` |
|      - | 4961 | `	}` |
|      7 | 4962 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      3 | 4963 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 | 4964 | `	}` |
|      - | 4965 | `	/* Compute the SHA1 digest */` |
|      7 | 4966 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|      7 | 4967 | `	if( raw_output ){` |
|      - | 4968 | `		/* Output raw digest */` |
|      3 | 4969 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      2 | 4970 | `	}else{` |
|      - | 4971 | `		/* Perform a binary to hex conversion */` |
|      5 | 4972 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4973 | `	}` |
|      7 | 4974 | `	return PH7_OK;` |
|      5 | 4975 |  |
|      - | 4976 | `/*` |
|      - | 4977 | ` * int64 crc32(string $str)` |
|      - | 4978 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 4979 | ` * Parameter` |
|      - | 4980 | ` *  $str` |
|      - | 4981 | ` *   Input string` |
|      - | 4982 | ` * Return` |
|      - | 4983 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 4984 | ` */` |
|      4 | 4985 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4986 |  |
|      - | 4987 | `	const void *pIn;` |
|      - | 4988 | `	sxu32 nCRC;` |
|      - | 4989 | `	int nLen;` |
|      5 | 4990 | `	if( nArg < 1 ){` |
|      - | 4991 | `		/* Missing arguments,return 0 */` |
|      3 | 4992 | `		ph7_result_int(pCtx,0);` |
|      3 | 4993 | `		return PH7_OK;` |
|      - | 4994 | `	}` |
|      - | 4995 | `	/* Extract the input string */` |
|      3 | 4996 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4997 | `	if( nLen < 1 ){` |
|      - | 4998 | `		/* Empty string */` |
|    ! 0 | 4999 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5000 | `		return PH7_OK;` |
|      - | 5001 | `	}` |
|      - | 5002 | `	/* Calculate the sum */` |
|      3 | 5003 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 5004 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 5005 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 5006 | `	return PH7_OK;` |
|      3 | 5007 |  |
|      - | 5008 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 5009 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 5010 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 5011 | `/*` |
|      - | 5012 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 5013 |  |
|      - | 5014 | ` */` |
|      4 | 5015 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 5016 | `	const char *zInput, /* Raw input */` |
|      - | 5017 | `	int nByte,  /* Input length */` |
|      - | 5018 | `	int delim,  /* Delimiter */` |
|      - | 5019 | `	int encl,   /* Enclosure */` |
|      - | 5020 | `	int escape,  /* Escape character */` |
|      - | 5021 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 5022 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 5023 | `	)` |
|      1 | 5024 |  |
|      5 | 5025 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 5026 | `	const char *zIn = zInput;` |
|      - | 5027 | `	const char *zPtr;` |
|      - | 5028 | `	int isEnc;` |
|      - | 5029 | `	/* Start processing */` |
|      8 | 5030 | `	for(;;){` |
|     17 | 5031 | `		if( zIn >= zEnd ){` |
|      - | 5032 | `			/* No more input to process */` |
|      5 | 5033 | `			break;` |
|      - | 5034 | `		}` |
|     13 | 5035 | `		isEnc = 0;` |
|     13 | 5036 | `		zPtr = zIn;` |
|      - | 5037 | `		/* Find the first delimiter */` |
|     27 | 5038 | `		while( zIn < zEnd ){` |
|     23 | 5039 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 5040 | `				/* Delimiter found,break imediately */` |
|      5 | 5041 | `				break;` |
|     15 | 5042 | `			}else if( zIn[0] == encl ){` |
|      - | 5043 | `				/* Inside enclosure? */` |
|    ! 0 | 5044 | `				isEnc = !isEnc;` |
|     15 | 5045 | `			}else if( zIn[0] == escape ){` |
|      - | 5046 | `				/* Escape sequence */` |
|    ! 0 | 5047 | `				zIn++;` |
|    ! 0 | 5048 | `			}` |
|      - | 5049 | `			/* Advance the cursor */` |
|     15 | 5050 | `			zIn++;` |
|      1 | 5051 | `		}` |
|     13 | 5052 | `		if( zIn > zPtr ){` |
|     13 | 5053 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 5054 | `			sxi32 rc;` |
|      - | 5055 | `			/* Invoke the supllied callback */` |
|     13 | 5056 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 5057 | `				zPtr++;` |
|    ! 0 | 5058 | `				nByteChunk-=2;` |
|    ! 0 | 5059 | `			}` |
|     13 | 5060 | `			if( nByteChunk > 0 ){` |
|     13 | 5061 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 5062 | `				if( rc == SXERR_ABORT ){` |
|      - | 5063 | `					/* User callback request an operation abort */` |
|    ! 0 | 5064 | `					break;` |
|      - | 5065 | `				}` |
|      6 | 5066 | `			}` |
|      6 | 5067 | `		}` |
|      - | 5068 | `		/* Ignore trailing delimiter */` |
|     21 | 5069 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 5070 | `			zIn++;` |
|      1 | 5071 | `		}` |
|      1 | 5072 | `	}` |
|      5 | 5073 | `	return SXRET_OK;` |
|      1 | 5074 |  |
|      - | 5075 | `/*` |
|      - | 5076 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 5077 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 5078 | ` * argument to this callback.` |
|      - | 5079 | ` */` |
|     12 | 5080 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 5081 |  |
|     13 | 5082 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 5083 | `	ph7_value sEntry;` |
|      - | 5084 | `	SyString sToken;` |
|      - | 5085 | `	/* Insert the token in the given array */` |
|     13 | 5086 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 5087 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 5088 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 5089 | `	if( sToken.nByte < 1){` |
|    ! 0 | 5090 | `		return SXRET_OK;` |
|      - | 5091 | `	}` |
|     13 | 5092 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 5093 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 5094 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 5095 | `	return SXRET_OK;` |
|      7 | 5096 |  |
|      - | 5097 | `/*` |
|      - | 5098 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 5099 | ` *  Parse a CSV string into an array.` |
|      - | 5100 | ` * Parameters` |
|      - | 5101 | ` *  $input` |
|      - | 5102 | ` *   The string to parse.` |
|      - | 5103 | ` *  $delimiter` |
|      - | 5104 | ` *   Set the field delimiter (one character only).` |
|      - | 5105 | ` *  $enclosure` |
|      - | 5106 | ` *   Set the field enclosure character (one character only).` |
|      - | 5107 | ` *  $escape` |
|      - | 5108 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 5109 | ` * Return` |
|      - | 5110 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 5111 | ` */` |
|      4 | 5112 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5113 |  |
|      - | 5114 | `	const char *zInput,*zPtr;` |
|      - | 5115 | `	ph7_value *pArray;` |
|      5 | 5116 | `	int delim  = ',';   /* Delimiter */` |
|      5 | 5117 | `	int encl   = '"' ;  /* Enclosure */` |
|      5 | 5118 | `	int escape = '\\';  /* Escape character */` |
|      - | 5119 | `	int nLen;` |
|      5 | 5120 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5121 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 5122 | `		ph7_result_null(pCtx);` |
|      3 | 5123 | `		return PH7_OK;` |
|      - | 5124 | `	}` |
|      - | 5125 | `	/* Extract the raw input */` |
|      3 | 5126 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5127 | `	if( nArg > 1 ){` |
|      - | 5128 | `		int i;` |
|      3 | 5129 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 5130 | `			/* Extract the delimiter */` |
|      3 | 5131 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 5132 | `			if( i > 0 ){` |
|      3 | 5133 | `				delim = zPtr[0];` |
|      1 | 5134 | `			}` |
|      1 | 5135 | `		}` |
|      3 | 5136 | `		if( nArg > 2 ){` |
|      3 | 5137 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 5138 | `				/* Extract the enclosure */` |
|      3 | 5139 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 5140 | `				if( i > 0 ){` |
|      3 | 5141 | `					encl = zPtr[0];` |
|      1 | 5142 | `				}` |
|      1 | 5143 | `			}` |
|      3 | 5144 | `			if( nArg > 3 ){` |
|      3 | 5145 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 5146 | `					/* Extract the escape character */` |
|      3 | 5147 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 5148 | `					if( i > 0 ){` |
|      3 | 5149 | `						escape = zPtr[0];` |
|      1 | 5150 | `					}` |
|      1 | 5151 | `				}` |
|      1 | 5152 | `			}` |
|      1 | 5153 | `		}` |
|      1 | 5154 | `	}` |
|      - | 5155 | `	/* Create our array */` |
|      3 | 5156 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5157 | `	if( pArray == 0 ){` |
|    ! 0 | 5158 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 5159 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5160 | `		return PH7_OK;` |
|      - | 5161 | `	}` |
|      - | 5162 | `	/* Parse the raw input */` |
|      3 | 5163 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 5164 | `	/* Return the freshly created array */` |
|      3 | 5165 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5166 | `	return PH7_OK;` |
|      3 | 5167 |  |
|      - | 5168 | `/*` |
|      - | 5169 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 5170 | ` * container.` |
|      - | 5171 | ` * Refer to [strip_tags()].` |
|      - | 5172 | ` */` |
|     10 | 5173 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5174 |  |
|     11 | 5175 | `	const char *zEnd = &zTag[nByte];` |
|      - | 5176 | `	const char *zPtr;` |
|      - | 5177 | `	SyString sEntry;` |
|      - | 5178 | `	/* Strip tags */` |
|     10 | 5179 | `	for(;;){` |
|     45 | 5180 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 5181 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 5182 | `				zTag++;` |
|      1 | 5183 | `		}` |
|     21 | 5184 | `		if( zTag >= zEnd ){` |
|     11 | 5185 | `			break;` |
|      - | 5186 | `		}` |
|     11 | 5187 | `		zPtr = zTag;` |
|      - | 5188 | `		/* Delimit the tag */` |
|     25 | 5189 | `		while(zTag < zEnd ){` |
|     25 | 5190 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5191 | `				/* UTF-8 stream */` |
|      3 | 5192 | `				zTag++;` |
|      5 | 5193 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 5194 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 5195 | `				break;` |
|    ! 0 | 5196 | `			}else{` |
|     13 | 5197 | `				zTag++;` |
|      - | 5198 | `			}` |
|      1 | 5199 | `		}` |
|     11 | 5200 | `		if( zTag > zPtr ){` |
|      - | 5201 | `			/* Perform the insertion */` |
|     11 | 5202 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 5203 | `			SyStringFullTrim(&sEntry);` |
|     11 | 5204 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 5205 | `		}` |
|      - | 5206 | `		/* Jump the trailing '>' */` |
|     11 | 5207 | `		zTag++;` |
|      1 | 5208 | `	}` |
|     11 | 5209 | `	return SXRET_OK;` |
|      1 | 5210 |  |
|      - | 5211 | `/*` |
|      - | 5212 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 5213 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 5214 | ` * Refer to [strip_tags()].` |
|      - | 5215 | ` */` |
|     36 | 5216 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5217 |  |
|     37 | 5218 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 5219 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 5220 | `		SyString sTag;` |
|     85 | 5221 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 5222 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 5223 | `			zTag++;` |
|      1 | 5224 | `		}` |
|      - | 5225 | `		/* Delimit the tag */` |
|     25 | 5226 | `		zCur = zTag;` |
|     77 | 5227 | `		while(zTag < zEnd ){` |
|     77 | 5228 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5229 | `				/* UTF-8 stream */` |
|      5 | 5230 | `				zTag++;` |
|      9 | 5231 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 5232 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 5233 | `				break;` |
|    ! 0 | 5234 | `			}else{` |
|     49 | 5235 | `				zTag++;` |
|      - | 5236 | `			}` |
|      1 | 5237 | `		}` |
|     25 | 5238 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 5239 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 5240 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 5241 | `		if( sTag.nByte > 0 ){` |
|      - | 5242 | `			SyString *aEntry,*pEntry;` |
|      - | 5243 | `			sxi32 rc;` |
|      - | 5244 | `			sxu32 n;` |
|      - | 5245 | `			/* Perform the lookup */` |
|     25 | 5246 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 5247 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 5248 | `				pEntry = &aEntry[n];` |
|      - | 5249 | `				/* Do the comparison */` |
|     25 | 5250 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 5251 | `				if( !rc ){` |
|     21 | 5252 | `					return SXRET_OK;` |
|      - | 5253 | `				}` |
|      3 | 5254 | `			}` |
|      2 | 5255 | `		}` |
|      2 | 5256 | `	}` |
|      - | 5257 | `	/* No such tag */` |
|     17 | 5258 | `	return SXERR_NOTFOUND;` |
|     19 | 5259 |  |
|      - | 5260 | `/*` |
|      - | 5261 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 5262 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 5263 | ` * Refer to [strip_tags()].` |
|      - | 5264 | ` */` |
|     16 | 5265 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 5266 |  |
|     17 | 5267 | `	const char *zEnd = &zIn[nByte];` |
|      - | 5268 | `	const char *zPtr,*zTag;` |
|      - | 5269 | `	SySet sSet;` |
|      - | 5270 | `	/* initialize the set of allowed tags */` |
|     17 | 5271 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 5272 | `	if( nTaglen > 0 ){` |
|      - | 5273 | `		/* Set of allowed tags */` |
|     11 | 5274 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 5275 | `	}` |
|      - | 5276 | `	/* Set the empty string */` |
|     17 | 5277 | `	ph7_result_string(pCtx,"",0);` |
|      - | 5278 | `	/* Start processing */` |
|     26 | 5279 | `	for(;;){` |
|     53 | 5280 | `		if(zIn >= zEnd){` |
|      - | 5281 | `			/* No more input to process */` |
|     15 | 5282 | `			break;` |
|      - | 5283 | `		}` |
|     39 | 5284 | `		zPtr = zIn;` |
|      - | 5285 | `		/* Find a tag */` |
|    133 | 5286 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 5287 | `			zIn++;` |
|      1 | 5288 | `		}` |
|     39 | 5289 | `		if( zIn > zPtr ){` |
|      - | 5290 | `			/* Consume raw input */` |
|     21 | 5291 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 5292 | `		}` |
|      - | 5293 | `		/* Ignore trailing null bytes */` |
|     39 | 5294 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 5295 | `			zIn++;` |
|    ! 0 | 5296 | `		}` |
|     39 | 5297 | `		if(zIn >= zEnd){` |
|      - | 5298 | `			/* No more input to process */` |
|      3 | 5299 | `			break;` |
|      - | 5300 | `		}` |
|     37 | 5301 | `		if( zIn[0] == '<' ){` |
|      - | 5302 | `			sxi32 rc;` |
|     37 | 5303 | `			zTag = zIn++;` |
|      - | 5304 | `			/* Delimit the tag */` |
|    127 | 5305 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 5306 | `				zIn++;` |
|      1 | 5307 | `			}` |
|     37 | 5308 | `			if( zIn < zEnd ){` |
|     37 | 5309 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 5310 | `			}` |
|      - | 5311 | `			/* Query the set */` |
|     37 | 5312 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 5313 | `			if( rc == SXRET_OK ){` |
|      - | 5314 | `				/* Keep the tag */` |
|     21 | 5315 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 5316 | `			}` |
|     18 | 5317 | `		}` |
|      1 | 5318 | `	}` |
|      - | 5319 | `	/* Cleanup */` |
|     17 | 5320 | `	SySetRelease(&sSet);` |
|     17 | 5321 | `	return SXRET_OK;` |
|      1 | 5322 |  |
|      - | 5323 | `/*` |
|      - | 5324 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 5325 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 5326 | ` * Parameters` |
|      - | 5327 | ` *  $str` |
|      - | 5328 | ` *  The input string.` |
|      - | 5329 | ` * $allowable_tags` |
|      - | 5330 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 5331 | ` * Return` |
|      - | 5332 | ` *  Returns the stripped string.` |
|      - | 5333 | ` */` |
|     16 | 5334 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5335 |  |
|     17 | 5336 | `	const char *zTaglist = 0;` |
|      - | 5337 | `	const char *zString;` |
|     17 | 5338 | `	int nTaglen = 0;` |
|      - | 5339 | `	int nLen;` |
|     17 | 5340 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5341 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 5342 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5343 | `		return PH7_OK;` |
|      - | 5344 | `	}` |
|      - | 5345 | `	/* Point to the raw string */` |
|     15 | 5346 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 5347 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 5348 | `		/* Allowed tag */` |
|     11 | 5349 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 5350 | `	}` |
|      - | 5351 | `	/* Process input */` |
|     15 | 5352 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 5353 | `	return PH7_OK;` |
|      9 | 5354 |  |
|      - | 5355 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5356 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5357 | `/*` |
|      - | 5358 | ` * string str_shuffle(string $str)` |
|      - | 5359 |  |
|      - | 5360 | ` *  Randomly shuffles a string.` |
|      - | 5361 | ` * Parameters` |
|      - | 5362 | ` *  $str` |
|      - | 5363 | ` *   The input string.` |
|      - | 5364 | ` * Return` |
|      - | 5365 | ` *  Returns the shuffled string.` |
|      - | 5366 | ` */` |
|     12 | 5367 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5368 |  |
|      - | 5369 | `	const char *zString;` |
|      - | 5370 | `	int nLen,i,c;` |
|      - | 5371 | `	sxu32 iR;` |
|     13 | 5372 | `	if( nArg < 1 ){` |
|      - | 5373 | `		/* Missing arguments,return the empty string */` |
|      3 | 5374 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5375 | `		return PH7_OK;` |
|      - | 5376 | `	}` |
|      - | 5377 | `	/* Extract the target string */` |
|     11 | 5378 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5379 | `	if( nLen < 1 ){` |
|      - | 5380 | `		/* Nothing to shuffle */` |
|      3 | 5381 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5382 | `		return PH7_OK;` |
|      - | 5383 | `	}` |
|      - | 5384 | `	/* Shuffle the string */` |
|     43 | 5385 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 5386 | `		/* Generate a random number first */` |
|     35 | 5387 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 5388 | `		/* Extract a random offset */` |
|     35 | 5389 | `		c = zString[iR % nLen];` |
|      - | 5390 | `		/* Append it */` |
|     35 | 5391 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 5392 | `	}` |
|      9 | 5393 | `	return PH7_OK;` |
|      7 | 5394 |  |
|      - | 5395 | `/*` |
|      - | 5396 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 5397 | ` *  Convert a string to an array.` |
|      - | 5398 | ` * Parameters` |
|      - | 5399 | ` * $string` |
|      - | 5400 | ` *  The input string.` |
|      - | 5401 | ` * $split_length` |
|      - | 5402 | ` *  Maximum length of the chunk.` |
|      - | 5403 | ` * Return` |
|      - | 5404 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 5405 | ` *  except possibly the last one which may be shorter.` |
|      - | 5406 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 5407 | ` *  as the first (and only) array element.` |
|      - | 5408 | ` *  An empty string returns an empty array.` |
|      - | 5409 | ` * Errors` |
|      - | 5410 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 5411 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 5412 | ` *  ValueError if $split_length is less than 1.` |
|      - | 5413 | ` */` |
|     28 | 5414 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5415 |  |
|      - | 5416 | `	const char *zString,*zEnd;` |
|      - | 5417 | `	ph7_value *pArray,*pValue;` |
|      - | 5418 | `	int split_len;` |
|      - | 5419 | `	int nLen;` |
|     30 | 5420 | `	if( nArg < 1 ){` |
|      4 | 5421 | `		return PH7_VmThrowException(pCtx,` |
|      - | 5422 | `			"ArgumentCountError",` |
|      - | 5423 | `			"str_split() expects at least 1 argument, %d given",` |
|      1 | 5424 | `			nArg` |
|      - | 5425 | `			);` |
|      - | 5426 | `	}` |
|      - | 5427 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     50 | 5428 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     38 | 5429 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 5430 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 5431 | `		return PH7_VmThrowException(pCtx,` |
|      - | 5432 | `			"TypeError",` |
|      - | 5433 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 5434 | `			ph7_type_name(apArg[0])` |
|      - | 5435 | `			);` |
|      - | 5436 | `	}` |
|      - | 5437 | `	/* Point to the target string */` |
|     26 | 5438 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     26 | 5439 | `	split_len = (int)sizeof(char);` |
|     26 | 5440 | `	if( nArg > 1 ){` |
|      - | 5441 | `		/* Split length */` |
|     16 | 5442 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     16 | 5443 | `		if( split_len < 1 ){` |
|      5 | 5444 | `			return PH7_VmThrowException(pCtx,` |
|      - | 5445 | `				"ValueError",` |
|      - | 5446 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 5447 | `				);` |
|      - | 5448 | `		}` |
|     11 | 5449 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 5450 | `			split_len = nLen;` |
|      1 | 5451 | `		}` |
|      5 | 5452 | `	}` |
|      - | 5453 | `	/* Create the array and the scalar value */` |
|     21 | 5454 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 5455 | `	/*Chunk value */` |
|     21 | 5456 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 5457 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 5458 | `		/* Return FALSE */` |
|    ! 0 | 5459 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5460 | `		return PH7_OK;` |
|      - | 5461 | `	}` |
|      - | 5462 | `	/* Point to the end of the string */` |
|     21 | 5463 | `	zEnd = &zString[nLen];` |
|      - | 5464 | `	/* Perform the requested operation */` |
|     48 | 5465 | `	for(;;){` |
|      - | 5466 | `		int nMax;` |
|     59 | 5467 | `		if( zString >= zEnd ){` |
|      - | 5468 | `			/* No more input to process */` |
|     21 | 5469 | `			break;` |
|      - | 5470 | `		}` |
|     39 | 5471 | `		nMax = (int)(zEnd-zString);` |
|     39 | 5472 | `		if( nMax < split_len ){` |
|      3 | 5473 | `			split_len = nMax;` |
|      1 | 5474 | `		}` |
|      - | 5475 | `		/* Copy the current chunk */` |
|     39 | 5476 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 5477 | `		/* Insert it */` |
|     39 | 5478 | `		ph7_array_add_elem(pArray,0,pValue); /* Will make it's own copy */` |
|      - | 5479 | `		/* reset the string cursor */` |
|     39 | 5480 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 5481 | `		/* Update position */` |
|     39 | 5482 | `		zString += split_len;` |
|      1 | 5483 | `	}` |
|      - | 5484 | `	/*` |
|      - | 5485 | `	 * Return the array.` |
|      - | 5486 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 5487 | `	 * upon we return from this function.` |
|      - | 5488 | `	 */` |
|     21 | 5489 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 5490 | `	return PH7_OK;` |
|     16 | 5491 |  |
|      - | 5492 | `/*` |
|      - | 5493 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 5494 | ` * Refer to [strspn()].` |
|      - | 5495 | ` */` |
|     28 | 5496 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 5497 |  |
|     29 | 5498 | `	const char *zIn = *pzIn;` |
|      - | 5499 | `	const char *zPtr;` |
|      - | 5500 | `	/* Ignore leading white spaces */` |
|     29 | 5501 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 5502 | `		zIn++;` |
|    ! 0 | 5503 | `	}` |
|     29 | 5504 | `	if( zIn >= zEnd ){` |
|      - | 5505 | `		/* End of input */` |
|    ! 0 | 5506 | `		return SXERR_EOF;` |
|      - | 5507 | `	}` |
|     29 | 5508 | `	zPtr = zIn;` |
|      - | 5509 | `	/* Extract the token */` |
|    201 | 5510 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 5511 | `		zIn++;` |
|      1 | 5512 | `	}` |
|     29 | 5513 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5514 | `	/* Synchronize pointers */` |
|     29 | 5515 | `	*pzIn = zIn;` |
|      - | 5516 | `	/* Return to the caller */` |
|     29 | 5517 | `	return SXRET_OK;` |
|     15 | 5518 |  |
|      - | 5519 | `/*` |
|      - | 5520 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 5521 | ` * return the longest match.` |
|      - | 5522 | ` * Refer to [strspn()].` |
|      - | 5523 | ` */` |
|     18 | 5524 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5525 |  |
|     19 | 5526 | `	const char *zEnd = &zString[nLen];` |
|     19 | 5527 | `	const char *zIn = zString;` |
|      - | 5528 | `	int i,c;` |
|     45 | 5529 | `	for(;;){` |
|     91 | 5530 | `		if( zString >= zEnd ){` |
|      7 | 5531 | `			break;` |
|      - | 5532 | `		}` |
|      - | 5533 | `		/* Extract current character */` |
|     85 | 5534 | `		c = zString[0];` |
|      - | 5535 | `		/* Perform the lookup */` |
|    383 | 5536 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 5537 | `			if( c == zMask[i] ){` |
|      - | 5538 | `				/* Character found */` |
|     73 | 5539 | `				break;` |
|      - | 5540 | `			}` |
|    150 | 5541 | `		}` |
|     85 | 5542 | `		if( i >= nMaskLen ){` |
|      - | 5543 | `			/* Character not in the current mask,break immediately */` |
|     13 | 5544 | `			break;` |
|      - | 5545 | `		}` |
|      - | 5546 | `		/* Advance cursor */` |
|     73 | 5547 | `		zString++;` |
|      1 | 5548 | `	}` |
|      - | 5549 | `	/* Longest match */` |
|     19 | 5550 | `	return (int)(zString-zIn);` |
|      1 | 5551 |  |
|      - | 5552 | `/*` |
|      - | 5553 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 5554 | ` * Refer to [strcspn()].` |
|      - | 5555 | ` */` |
|     10 | 5556 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5557 |  |
|     11 | 5558 | `	const char *zEnd = &zString[nLen];` |
|     11 | 5559 | `	const char *zIn = zString;` |
|      - | 5560 | `	int i,c;` |
|     12 | 5561 | `	for(;;){` |
|     25 | 5562 | `		if( zString >= zEnd ){` |
|      3 | 5563 | `			break;` |
|      - | 5564 | `		}` |
|      - | 5565 | `		/* Extract current character */` |
|     23 | 5566 | `		c = zString[0];` |
|      - | 5567 | `		/* Perform the lookup */` |
|     51 | 5568 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 5569 | `			if( c == zMask[i] ){` |
|      9 | 5570 | `				break;` |
|      - | 5571 | `			}` |
|     15 | 5572 | `		}` |
|     23 | 5573 | `		if( i < nMaskLen ){` |
|      - | 5574 | `			/* Character in the current mask,break immediately */` |
|      9 | 5575 | `			break;` |
|      - | 5576 | `		}` |
|      - | 5577 | `		/* Advance cursor */` |
|     15 | 5578 | `		zString++;` |
|      1 | 5579 | `	}` |
|      - | 5580 | `	/* Longest match */` |
|     11 | 5581 | `	return (int)(zString-zIn);` |
|      1 | 5582 |  |
|      - | 5583 | `/*` |
|      - | 5584 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5585 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 5586 | ` *  of characters contained within a given mask.` |
|      - | 5587 | ` * Parameters` |
|      - | 5588 | ` * $str` |
|      - | 5589 | ` *  The input string.` |
|      - | 5590 | ` * $mask` |
|      - | 5591 | ` *  The list of allowable characters.` |
|      - | 5592 | ` * $start` |
|      - | 5593 | ` *  The position in subject to start searching.` |
|      - | 5594 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5595 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5596 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5597 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5598 | ` *  start'th position from the end of subject.` |
|      - | 5599 | ` * $length` |
|      - | 5600 | ` *  The length of the segment from subject to examine.` |
|      - | 5601 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5602 | ` *  characters after the starting position.` |
|      - | 5603 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5604 | ` *  position up to length characters from the end of subject.` |
|      - | 5605 | ` * Return` |
|      - | 5606 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 5607 | ` * in mask.` |
|      - | 5608 | ` */` |
|     26 | 5609 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5610 |  |
|      - | 5611 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5612 | `	int iMasklen,iLen;` |
|      - | 5613 | `	SyString sToken;` |
|     27 | 5614 | `	int iCount = 0;` |
|      - | 5615 | `	int rc;` |
|     27 | 5616 | `	if( nArg < 2 ){` |
|      - | 5617 | `		/* Missing agruments,return zero */` |
|      3 | 5618 | `		ph7_result_int(pCtx,0);` |
|      3 | 5619 | `		return PH7_OK;` |
|      - | 5620 | `	}` |
|      - | 5621 | `	/* Extract the target string */` |
|     25 | 5622 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5623 | `	/* Extract the mask */` |
|     25 | 5624 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 5625 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 5626 | `		/* Nothing to process,return zero */` |
|      7 | 5627 | `		ph7_result_int(pCtx,0);` |
|      7 | 5628 | `		return PH7_OK;` |
|      - | 5629 | `	}` |
|     19 | 5630 | `	if( nArg > 2 ){` |
|      - | 5631 | `		int nOfft;` |
|      - | 5632 | `		/* Extract the offset */` |
|      9 | 5633 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 5634 | `		if( nOfft < 0 ){` |
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
|      9 | 5645 | `			if( nOfft >= iLen ){` |
|      - | 5646 | `				/* Invalid offset */` |
|    ! 0 | 5647 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5648 | `				return PH7_OK;` |
|    ! 0 | 5649 | `			}else{` |
|      - | 5650 | `				/* Update offset */` |
|      9 | 5651 | `				zString += nOfft;` |
|      9 | 5652 | `				iLen -= nOfft;` |
|      - | 5653 | `			}` |
|      - | 5654 | `		}` |
|      9 | 5655 | `		if( nArg > 3 ){` |
|      - | 5656 | `			int iUserlen;` |
|      - | 5657 | `			/* Extract the desired length */` |
|      9 | 5658 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 5659 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 5660 | `				iLen = iUserlen;` |
|      2 | 5661 | `			}` |
|      4 | 5662 | `		}` |
|      4 | 5663 | `	}` |
|      - | 5664 | `	/* Point to the end of the string */` |
|     19 | 5665 | `	zEnd = &zString[iLen];` |
|      - | 5666 | `	/* Extract the first non-space token */` |
|     19 | 5667 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 5668 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5669 | `		/* Compare against the current mask */` |
|     19 | 5670 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 5671 | `	}` |
|      - | 5672 | `	/* Longest match */` |
|     19 | 5673 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 5674 | `	return PH7_OK;` |
|     14 | 5675 |  |
|      - | 5676 | `/*` |
|      - | 5677 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5678 | ` *  Find length of initial segment not matching mask.` |
|      - | 5679 | ` * Parameters` |
|      - | 5680 | ` * $str` |
|      - | 5681 | ` *  The input string.` |
|      - | 5682 | ` * $mask` |
|      - | 5683 | ` *  The list of not allowed characters.` |
|      - | 5684 | ` * $start` |
|      - | 5685 | ` *  The position in subject to start searching.` |
|      - | 5686 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5687 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5688 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5689 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5690 | ` *  start'th position from the end of subject.` |
|      - | 5691 | ` * $length` |
|      - | 5692 | ` *  The length of the segment from subject to examine.` |
|      - | 5693 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5694 | ` *  characters after the starting position.` |
|      - | 5695 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5696 | ` *  position up to length characters from the end of subject.` |
|      - | 5697 | ` * Return` |
|      - | 5698 | ` *  Returns the length of the segment as an integer.` |
|      - | 5699 | ` */` |
|     16 | 5700 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5701 |  |
|      - | 5702 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5703 | `	int iMasklen,iLen;` |
|      - | 5704 | `	SyString sToken;` |
|     17 | 5705 | `	int iCount = 0;` |
|      - | 5706 | `	int rc;` |
|     17 | 5707 | `	if( nArg < 2 ){` |
|      - | 5708 | `		/* Missing agruments,return zero */` |
|      3 | 5709 | `		ph7_result_int(pCtx,0);` |
|      3 | 5710 | `		return PH7_OK;` |
|      - | 5711 | `	}` |
|      - | 5712 | `	/* Extract the target string */` |
|     15 | 5713 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5714 | `	/* Extract the mask */` |
|     15 | 5715 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 5716 | `	if( iLen < 1 ){` |
|      - | 5717 | `		/* Nothing to process,return zero */` |
|    ! 0 | 5718 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5719 | `		return PH7_OK;` |
|      - | 5720 | `	}` |
|     15 | 5721 | `	if( iMasklen < 1 ){` |
|      - | 5722 | `		/* No given mask,return the string length */` |
|      3 | 5723 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 5724 | `		return PH7_OK;` |
|      - | 5725 | `	}` |
|     13 | 5726 | `	if( nArg > 2 ){` |
|      - | 5727 | `		int nOfft;` |
|      - | 5728 | `		/* Extract the offset */` |
|     11 | 5729 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 5730 | `		if( nOfft < 0 ){` |
|    ! 0 | 5731 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5732 | `			if( zBase > zString ){` |
|    ! 0 | 5733 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5734 | `				zString = zBase;` |
|    ! 0 | 5735 | `			}else{` |
|      - | 5736 | `				/* Invalid offset */` |
|    ! 0 | 5737 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5738 | `				return PH7_OK;` |
|      - | 5739 | `			}` |
|    ! 0 | 5740 | `		}else{` |
|     11 | 5741 | `			if( nOfft >= iLen ){` |
|      - | 5742 | `				/* Invalid offset */` |
|      3 | 5743 | `				ph7_result_int(pCtx,0);` |
|      3 | 5744 | `				return PH7_OK;` |
|    ! 0 | 5745 | `			}else{` |
|      - | 5746 | `				/* Update offset */` |
|      9 | 5747 | `				zString += nOfft;` |
|      9 | 5748 | `				iLen -= nOfft;` |
|      - | 5749 | `			}` |
|      - | 5750 | `		}` |
|      9 | 5751 | `		if( nArg > 3 ){` |
|      - | 5752 | `			int iUserlen;` |
|      - | 5753 | `			/* Extract the desired length */` |
|    ! 0 | 5754 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 5755 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 5756 | `				iLen = iUserlen;` |
|    ! 0 | 5757 | `			}` |
|    ! 0 | 5758 | `		}` |
|      4 | 5759 | `	}` |
|      - | 5760 | `	/* Point to the end of the string */` |
|     11 | 5761 | `	zEnd = &zString[iLen];` |
|      - | 5762 | `	/* Extract the first non-space token */` |
|     11 | 5763 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 5764 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5765 | `		/* Compare against the current mask */` |
|     11 | 5766 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 5767 | `	}` |
|      - | 5768 | `	/* Longest match */` |
|     11 | 5769 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 5770 | `	return PH7_OK;` |
|      9 | 5771 |  |
|      - | 5772 | `/*` |
|      - | 5773 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 5774 | ` *  Search a string for any of a set of characters.` |
|      - | 5775 | ` * Parameters` |
|      - | 5776 | ` *  $haystack` |
|      - | 5777 | ` *   The string where char_list is looked for.` |
|      - | 5778 | ` *  $char_list` |
|      - | 5779 | ` *   This parameter is case sensitive.` |
|      - | 5780 | ` * Return` |
|      - | 5781 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 5782 | ` */` |
|      6 | 5783 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5784 |  |
|      - | 5785 | `	const char *zString,*zList,*zEnd;` |
|      - | 5786 | `	int iLen,iListLen,i,c;` |
|      - | 5787 | `	sxu32 nOfft,nMax;` |
|      - | 5788 | `	sxi32 rc;` |
|      7 | 5789 | `	if( nArg < 2 ){` |
|      - | 5790 | `		/* Missing arguments,return FALSE */` |
|      3 | 5791 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5792 | `		return PH7_OK;` |
|      - | 5793 | `	}` |
|      - | 5794 | `	/* Extract the haystack and the char list */` |
|      5 | 5795 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 5796 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 5797 | `	if( iLen < 1 ){` |
|      - | 5798 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 5799 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5800 | `		return PH7_OK;` |
|      - | 5801 | `	}` |
|      - | 5802 | `	/* Point to the end of the string */` |
|      5 | 5803 | `	zEnd = &zString[iLen];` |
|      5 | 5804 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 5805 | `	/* perform the requested operation */` |
|     15 | 5806 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 5807 | `		c = zList[i];` |
|     11 | 5808 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 5809 | `		if( rc == SXRET_OK ){` |
|      5 | 5810 | `			if( nMax < nOfft ){` |
|      3 | 5811 | `				nOfft = nMax;` |
|      1 | 5812 | `			}` |
|      2 | 5813 | `		}` |
|      6 | 5814 | `	}` |
|      5 | 5815 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 5816 | `		/* No such substring,return FALSE */` |
|      3 | 5817 | `		ph7_result_bool(pCtx,0);` |
|      2 | 5818 | `	}else{` |
|      - | 5819 | `		/* Return the substring */` |
|      3 | 5820 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 5821 | `	}` |
|      5 | 5822 | `	return PH7_OK;` |
|      4 | 5823 |  |
|      - | 5824 | `/*` |
|      - | 5825 | ` * string soundex(string $str)` |
|      - | 5826 | ` *  Calculate the soundex key of a string.` |
|      - | 5827 | ` * Parameters` |
|      - | 5828 | ` *  $str` |
|      - | 5829 | ` *   The input string.` |
|      - | 5830 | ` * Return` |
|      - | 5831 | ` *  Returns the soundex key as a string.` |
|      - | 5832 | ` * Note:` |
|      - | 5833 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 5834 | ` * source tree.` |
|      - | 5835 | ` */` |
|     20 | 5836 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5837 |  |
|      - | 5838 | `	const unsigned char *zIn;` |
|      - | 5839 | `	char zResult[8];` |
|      - | 5840 | `	int i, j;` |
|      - | 5841 | `	static const unsigned char iCode[] = {` |
|      - | 5842 |  |
|      - | 5843 |  |
|      - | 5844 |  |
|      - | 5845 |  |
|      - | 5846 |  |
|      - | 5847 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5848 |  |
|      - | 5849 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5850 | `	};` |
|     21 | 5851 | `	if( nArg < 1 ){` |
|      - | 5852 | `		/* Missing arguments,return the empty string */` |
|      3 | 5853 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5854 | `		return PH7_OK;` |
|      - | 5855 | `	}` |
|     19 | 5856 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     19 | 5857 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     19 | 5858 | `	if( zIn[i] ){` |
|     17 | 5859 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 5860 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 5861 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 5862 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 5863 | `			if( code>0 ){` |
|     45 | 5864 | `				if( code!=prevcode ){` |
|     33 | 5865 | `					prevcode = (unsigned char)code;` |
|     33 | 5866 | `					zResult[j++] = (char)code + '0';` |
|     16 | 5867 | `				}` |
|     23 | 5868 | `			}else{` |
|     49 | 5869 | `				prevcode = 0;` |
|      - | 5870 | `			}` |
|     47 | 5871 | `		}` |
|     33 | 5872 | `		while( j<4 ){` |
|     17 | 5873 | `			zResult[j++] = '0';` |
|      1 | 5874 | `		}` |
|     17 | 5875 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 5876 | `	}else{` |
|      3 | 5877 | `	  ph7_result_string(pCtx,"?000",4);` |
|      - | 5878 | `	}` |
|     19 | 5879 | `	return PH7_OK;` |
|     11 | 5880 |  |
|      - | 5881 | `/*` |
|      - | 5882 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 5883 | ` *  Wraps a string to a given number of characters.` |
|      - | 5884 | ` * Parameters` |
|      - | 5885 | ` *  $str` |
|      - | 5886 | ` *   The input string.` |
|      - | 5887 | ` * $width` |
|      - | 5888 | ` *  The column width.` |
|      - | 5889 | ` * $break` |
|      - | 5890 | ` *  The line is broken using the optional break parameter.` |
|      - | 5891 | ` * Return` |
|      - | 5892 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 5893 | ` */` |
|     14 | 5894 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5895 |  |
|      - | 5896 | `	const char *zIn,*zEnd,*zBreak;` |
|      - | 5897 | `	int iLen,iBreaklen,iChunk;` |
|     15 | 5898 | `	if( nArg < 1 ){` |
|      - | 5899 | `		/* Missing arguments,return the empty string */` |
|      3 | 5900 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5901 | `		return PH7_OK;` |
|      - | 5902 | `	}` |
|      - | 5903 | `	/* Extract the input string */` |
|     13 | 5904 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|     13 | 5905 | `	if( iLen < 1 ){` |
|      - | 5906 | `		/* Nothing to process,return the empty string */` |
|      3 | 5907 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5908 | `		return PH7_OK;` |
|      - | 5909 | `	}` |
|      - | 5910 | `	/* Chunk length */` |
|     11 | 5911 | `	iChunk = 75;` |
|     11 | 5912 | `	iBreaklen = 0;` |
|     11 | 5913 | `	zBreak = ""; /* cc warning */` |
|     11 | 5914 | `	if( nArg > 1 ){` |
|     11 | 5915 | `		iChunk = ph7_value_to_int(apArg[1]);` |
|     11 | 5916 | `		if( iChunk < 1 ){` |
|    ! 0 | 5917 | `			iChunk = 75;` |
|    ! 0 | 5918 | `		}` |
|     11 | 5919 | `		if( nArg > 2 ){` |
|      3 | 5920 | `			zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      1 | 5921 | `		}` |
|      5 | 5922 | `	}` |
|     11 | 5923 | `	if( iBreaklen < 1 ){` |
|      - | 5924 | `		/* Set a default column break */` |
|      - | 5925 | `#ifdef __WINNT__` |
|      1 | 5926 | `		zBreak = "\r\n";` |
|      1 | 5927 | `		iBreaklen = (int)sizeof("\r\n")-1;` |
|      - | 5928 | `#else` |
|      8 | 5929 | `		zBreak = "\n";` |
|      8 | 5930 | `		iBreaklen = (int)sizeof(char);` |
|      - | 5931 | `#endif` |
|      4 | 5932 | `	}` |
|      - | 5933 | `	/* Perform the requested operation */` |
|     11 | 5934 | `	zEnd = &zIn[iLen];` |
|     41 | 5935 | `	for(;;){` |
|      - | 5936 | `		int nMax;` |
|     47 | 5937 | `		if( zIn >= zEnd ){` |
|      - | 5938 | `			/* No more input to process */` |
|     11 | 5939 | `			break;` |
|      - | 5940 | `		}` |
|     37 | 5941 | `		nMax = (int)(zEnd-zIn);` |
|     37 | 5942 | `		if( iChunk > nMax ){` |
|     11 | 5943 | `			iChunk = nMax;` |
|      5 | 5944 | `		}` |
|      - | 5945 | `		/* Append the column first */` |
|     37 | 5946 | `		ph7_result_string(pCtx,zIn,iChunk); /* Will make it's own copy */` |
|      - | 5947 | `		/* Advance the cursor */` |
|     37 | 5948 | `		zIn += iChunk;` |
|     37 | 5949 | `		if( zIn < zEnd ){` |
|      - | 5950 | `			/* Append the line break */` |
|     27 | 5951 | `			ph7_result_string(pCtx,zBreak,iBreaklen);` |
|     13 | 5952 | `		}` |
|      1 | 5953 | `	}` |
|     11 | 5954 | `	return PH7_OK;` |
|      8 | 5955 |  |
|      - | 5956 | `/*` |
|      - | 5957 | ` * Check if the given character is a member of the given mask.` |
|      - | 5958 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 5959 | ` * Refer to [strtok()].` |
|      - | 5960 | ` */` |
|     30 | 5961 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 5962 |  |
|      - | 5963 | `	int i;` |
|     57 | 5964 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 5965 | `		if( c == zMask[i] ){` |
|     13 | 5966 | `			if( pOfft ){` |
|      5 | 5967 | `				*pOfft = i;` |
|      2 | 5968 | `			}` |
|     13 | 5969 | `			return TRUE;` |
|      - | 5970 | `		}` |
|     14 | 5971 | `	}` |
|     19 | 5972 | `	return FALSE;` |
|     16 | 5973 |  |
|      - | 5974 | `/*` |
|      - | 5975 | ` * Extract a single token from the input stream.` |
|      - | 5976 | ` * Refer to [strtok()].` |
|      - | 5977 | ` */` |
|      6 | 5978 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 5979 |  |
|      7 | 5980 | `	const char *zIn = *pzIn;` |
|      - | 5981 | `	const char *zPtr;` |
|      - | 5982 | `	/* Ignore leading delimiter */` |
|     11 | 5983 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5984 | `		zIn++;` |
|      1 | 5985 | `	}` |
|      7 | 5986 | `	if( zIn >= zEnd ){` |
|      - | 5987 | `		/* End of input */` |
|    ! 0 | 5988 | `		return SXERR_EOF;` |
|      - | 5989 | `	}` |
|      7 | 5990 | `	zPtr = zIn;` |
|      - | 5991 | `	/* Extract the token */` |
|     13 | 5992 | `	while( zIn < zEnd ){` |
|     11 | 5993 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 5994 | `			/* UTF-8 stream */` |
|    ! 0 | 5995 | `			zIn++;` |
|    ! 0 | 5996 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 5997 | `		}else{` |
|     11 | 5998 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5999 | `				break;` |
|      - | 6000 | `			}` |
|      7 | 6001 | `			zIn++;` |
|      - | 6002 | `		}` |
|      1 | 6003 | `	}` |
|      7 | 6004 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 6005 | `	/* Update the cursor */` |
|      7 | 6006 | `	*pzIn = zIn;` |
|      - | 6007 | `	/* Return to the caller */` |
|      7 | 6008 | `	return SXRET_OK;` |
|      4 | 6009 |  |
|      - | 6010 | `/* strtok auxiliary private data */` |
|      - | 6011 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 6012 | `struct strtok_aux_data` |
|      - | 6013 |  |
|      - | 6014 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 6015 | `	const char *zIn;   /* Current input stream */` |
|      - | 6016 | `	const char *zEnd;  /* End of input */` |
|      - | 6017 | `};` |
|      - | 6018 | `/*` |
|      - | 6019 | ` * string strtok(string $str,string $token)` |
|      - | 6020 | ` * string strtok(string $token)` |
|      - | 6021 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 6022 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 6023 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 6024 | ` *  words by using the space character as the token.` |
|      - | 6025 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 6026 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 6027 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 6028 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 6029 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 6030 | ` *  the argument are found.` |
|      - | 6031 | ` * Parameters` |
|      - | 6032 | ` *  $str` |
|      - | 6033 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 6034 | ` * $token` |
|      - | 6035 | ` *  The delimiter used when splitting up str.` |
|      - | 6036 | ` * Return` |
|      - | 6037 | ` *   Current token or FALSE on EOF.` |
|      - | 6038 | ` */` |
|      8 | 6039 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6040 |  |
|      - | 6041 | `	strtok_aux_data *pAux;` |
|      - | 6042 | `	const char *zMask;` |
|      - | 6043 | `	SyString sToken;` |
|      - | 6044 | `	int nMasklen;` |
|      - | 6045 | `	sxi32 rc;` |
|      9 | 6046 | `	if( nArg < 2 ){` |
|      - | 6047 | `		/* Extract top aux data */` |
|      7 | 6048 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      7 | 6049 | `		if( pAux == 0 ){` |
|      - | 6050 | `			/* No aux data,return FALSE */` |
|    ! 0 | 6051 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6052 | `			return PH7_OK;` |
|      - | 6053 | `		}` |
|      7 | 6054 | `		nMasklen = 0;` |
|      7 | 6055 | `		zMask = ""; /* cc warning */` |
|      7 | 6056 | `		if( nArg > 0 ){` |
|      - | 6057 | `			/* Extract the mask */` |
|      5 | 6058 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 6059 | `		}` |
|      7 | 6060 | `		if( nMasklen < 1 ){` |
|      - | 6061 | `			/* Invalid mask,return FALSE */` |
|      3 | 6062 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|      3 | 6063 | `			ph7_context_free_chunk(pCtx,pAux);` |
|      3 | 6064 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|      3 | 6065 | `			ph7_result_bool(pCtx,0);` |
|      3 | 6066 | `			return PH7_OK;` |
|      - | 6067 | `		}` |
|      - | 6068 | `		/* Extract the token */` |
|      5 | 6069 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 6070 | `		if( rc != SXRET_OK ){` |
|      - | 6071 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 6072 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 6073 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6074 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 6075 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6076 | `		}else{` |
|      - | 6077 | `			/* Return the extracted token */` |
|      5 | 6078 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6079 | `		}` |
|      3 | 6080 | `	}else{` |
|      - | 6081 | `		const char *zInput,*zCur;` |
|      - | 6082 | `		char *zDup;` |
|      - | 6083 | `		int nLen;` |
|      - | 6084 | `		/* Extract the raw input */` |
|      3 | 6085 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 6086 | `		if( nLen < 1 ){` |
|      - | 6087 | `			/* Empty input,return FALSE */` |
|    ! 0 | 6088 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6089 | `			return PH7_OK;` |
|      - | 6090 | `		}` |
|      - | 6091 | `		/* Extract the mask */` |
|      3 | 6092 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 6093 | `		if( nMasklen < 1 ){` |
|      - | 6094 | `			/* Set a default mask */` |
|      - | 6095 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 6096 | `			zMask = TOK_MASK;` |
|    ! 0 | 6097 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 6098 | `#undef TOK_MASK` |
|    ! 0 | 6099 | `		}` |
|      - | 6100 | `		/* Extract a single token */` |
|      3 | 6101 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 6102 | `		if( rc != SXRET_OK ){` |
|      - | 6103 | `			/* Empty input */` |
|    ! 0 | 6104 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6105 | `			return PH7_OK;` |
|    ! 0 | 6106 | `		}else{` |
|      - | 6107 | `			/* Return the extracted token */` |
|      3 | 6108 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6109 | `		}` |
|      - | 6110 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 6111 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 6112 | `		if( pAux ){` |
|      3 | 6113 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 6114 | `			if( nLen < 1 ){` |
|    ! 0 | 6115 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6116 | `				return PH7_OK;` |
|      - | 6117 | `			}` |
|      - | 6118 | `			/* Duplicate input */` |
|      3 | 6119 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 6120 | `			if( zDup  ){` |
|      3 | 6121 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 6122 | `				/* Register the aux data */` |
|      3 | 6123 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 6124 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 6125 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 6126 | `			}` |
|      1 | 6127 | `		}` |
|      - | 6128 | `	}` |
|      7 | 6129 | `	return PH7_OK;` |
|      5 | 6130 |  |
|      - | 6131 | `/*` |
|      - | 6132 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 6133 | ` *  Pad a string to a certain length with another string` |
|      - | 6134 | ` * Parameters` |
|      - | 6135 | ` *  $input` |
|      - | 6136 | ` *   The input string.` |
|      - | 6137 | ` * $pad_length` |
|      - | 6138 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 6139 | ` *   string, no padding takes place.` |
|      - | 6140 | ` * $pad_string` |
|      - | 6141 | ` *   Note:` |
|      - | 6142 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 6143 | ` *    divided by the pad_string's length.` |
|      - | 6144 | ` * $pad_type` |
|      - | 6145 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 6146 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 6147 | ` * Return` |
|      - | 6148 | ` *  The padded string.` |
|      - | 6149 | ` */` |
|     10 | 6150 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6151 |  |
|      - | 6152 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 6153 | `	const char *zIn,*zPad;` |
|     11 | 6154 | `	if( nArg < 2 ){` |
|      - | 6155 | `		/* Missing arguments,return the empty string */` |
|      5 | 6156 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 6157 | `		return PH7_OK;` |
|      - | 6158 | `	}` |
|      - | 6159 | `	/* Extract the target string */` |
|      7 | 6160 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6161 | `	/* Padding length */` |
|      7 | 6162 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|      7 | 6163 | `	if( iPadlen > 0 ){` |
|      5 | 6164 | `		iPadlen -= iLen;` |
|      2 | 6165 | `	}` |
|      7 | 6166 | `	if( iPadlen < 1  ){` |
|      - | 6167 | `		/* Return the string verbatim */` |
|      3 | 6168 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      3 | 6169 | `		return PH7_OK;` |
|      - | 6170 | `	}` |
|      5 | 6171 | `	zPad = " "; /* Whitespace padding */` |
|      5 | 6172 | `	iStrpad = (int)sizeof(char);` |
|      5 | 6173 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      5 | 6174 | `	if( nArg > 2 ){` |
|      - | 6175 | `		/* Padding string */` |
|      5 | 6176 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      5 | 6177 | `		if( iStrpad < 1 ){` |
|      - | 6178 | `			/* Empty string */` |
|    ! 0 | 6179 | `			zPad = " "; /* Whitespace padding */` |
|    ! 0 | 6180 | `			iStrpad = (int)sizeof(char);` |
|    ! 0 | 6181 | `		}` |
|      5 | 6182 | `		if( nArg > 3 ){` |
|      - | 6183 | `			/* Padd type */` |
|      5 | 6184 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 6185 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 6186 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 6187 | `			}` |
|      2 | 6188 | `		}` |
|      2 | 6189 | `	}` |
|      5 | 6190 | `	iDiv = 1;` |
|      5 | 6191 | `	if( iType == 2 ){` |
|    ! 0 | 6192 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 6193 | `	}` |
|      - | 6194 | `	/* Perform the requested operation */` |
|      5 | 6195 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 6196 | `		jPad = iStrpad;` |
|      5 | 6197 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 6198 | `			/* Padding */` |
|      5 | 6199 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 6200 | `				break;` |
|      - | 6201 | `			}` |
|      3 | 6202 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      2 | 6203 | `		}` |
|      3 | 6204 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 6205 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 6206 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 6207 | `				if( jPad > iStrpad ){` |
|    ! 0 | 6208 | `					jPad = iStrpad;` |
|    ! 0 | 6209 | `				}` |
|      3 | 6210 | `				if( jPad < 1){` |
|    ! 0 | 6211 | `					break;` |
|      - | 6212 | `				}` |
|      3 | 6213 | `				ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 6214 | `			}` |
|      1 | 6215 | `		}` |
|      1 | 6216 | `	}` |
|      5 | 6217 | `	if( iLen > 0 ){` |
|      - | 6218 | `		/* Append the input string */` |
|      5 | 6219 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      2 | 6220 | `	}` |
|      5 | 6221 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 6222 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 6223 | `			/* Padding */` |
|      5 | 6224 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 6225 | `				break;` |
|      - | 6226 | `			}` |
|      3 | 6227 | `			ph7_result_string(pCtx,zPad,iStrpad);` |
|      2 | 6228 | `		}` |
|      5 | 6229 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 6230 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 6231 | `			if( jPad > iStrpad ){` |
|    ! 0 | 6232 | `				jPad = iStrpad;` |
|    ! 0 | 6233 | `			}` |
|      3 | 6234 | `			if( jPad < 1){` |
|    ! 0 | 6235 | `				break;` |
|      - | 6236 | `			}` |
|      3 | 6237 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 6238 | `		}` |
|      1 | 6239 | `	}` |
|      5 | 6240 | `	return PH7_OK;` |
|      6 | 6241 |  |
|      - | 6242 | `/*` |
|      - | 6243 | ` * String replacement private data.` |
|      - | 6244 | ` */` |
|      - | 6245 | `typedef struct str_replace_data str_replace_data;` |
|      - | 6246 | `struct str_replace_data` |
|      - | 6247 |  |
|      - | 6248 | `	/* The following two fields are only used by the strtr function */` |
|      - | 6249 | `	SyBlob *pWorker;         /* Working buffer */` |
|      - | 6250 | `	ProcStringMatch xMatch;  /* Pattern match routine */` |
|      - | 6251 | `	/* The following two fields are only used by the str_replace function */` |
|      - | 6252 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 6253 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 6254 | `};` |
|      - | 6255 | `/*` |
|      - | 6256 | ` * Remove a substring.` |
|      - | 6257 | ` */` |
|      - | 6258 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 6259 | `	for(;;){\` |
|      - | 6260 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 6261 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 6262 | `		++OFFT;\` |
|      - | 6263 | `	}\` |
|      - | 6264 |  |
|      - | 6265 | `/*` |
|      - | 6266 | ` * Shift right and insert algorithm.` |
|      - | 6267 | ` */` |
|      - | 6268 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 6269 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 6270 | `		for(;;){\` |
|      - | 6271 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 6272 | `			if(INLEN < 1 ) { break; }\` |
|      - | 6273 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 6274 | `			--INLEN; \` |
|      - | 6275 | `		}\` |
|      - | 6276 | `		for(;;){\` |
|      - | 6277 | `				if(ELEN < 1) { break; }\` |
|      - | 6278 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 6279 | `				OFFT++;\` |
|      - | 6280 | `				ENTRY++;\` |
|      - | 6281 | `				--ELEN;\` |
|      - | 6282 | `		}\` |
|      - | 6283 |  |
|      - | 6284 | `/*` |
|      - | 6285 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 6286 | ` * replacement string [i.e: zReplace].` |
|      - | 6287 | ` */` |
|     38 | 6288 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 6289 |  |
|     39 | 6290 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 6291 | `	sxu32 n,m;` |
|     39 | 6292 | `	n = SyBlobLength(pWorker);` |
|     39 | 6293 | `	m = nOfft;` |
|      - | 6294 | `	/* Delete the old entry */` |
|    475 | 6295 | `	STRDEL(zInput,n,m,nLen);` |
|     39 | 6296 | `	SyBlobLength(pWorker) -= nLen;` |
|     39 | 6297 | `	if( nReplen > 0 ){` |
|     33 | 6298 | `		sxi32 iRep = nReplen;` |
|      - | 6299 | `		sxi32 rc;` |
|      - | 6300 | `		/*` |
|      - | 6301 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 6302 | `		 * string.` |
|      - | 6303 | `		 */` |
|     33 | 6304 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     33 | 6305 | `		if( rc != SXRET_OK ){` |
|      - | 6306 | `			/* Simply ignore any memory failure problem */` |
|    ! 0 | 6307 | `			return SXRET_OK;` |
|      - | 6308 | `		}` |
|      - | 6309 | `		/* Perform the insertion now */` |
|     33 | 6310 | `		zInput = (char *)SyBlobData(pWorker);` |
|     33 | 6311 | `		n = SyBlobLength(pWorker);` |
|    163 | 6312 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     33 | 6313 | `		SyBlobLength(pWorker) += nReplen;` |
|     16 | 6314 | `	}` |
|     39 | 6315 | `	return SXRET_OK;` |
|     20 | 6316 |  |
|      - | 6317 | `/*` |
|      - | 6318 | ` * String replacement walker callback.` |
|      - | 6319 | ` * The following callback is invoked for each array entry that hold` |
|      - | 6320 | ` * the replace string.` |
|      - | 6321 | ` * Refer to the strtr() implementation for more information.` |
|      - | 6322 | ` */` |
|      8 | 6323 | `static int StringReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6324 |  |
|      9 | 6325 | `	str_replace_data *pRepData = (str_replace_data *)pUserData;` |
|      - | 6326 | `	const char *zTarget,*zReplace;` |
|      - | 6327 | `	SyBlob *pWorker;` |
|      - | 6328 | `	int tLen,nLen;` |
|      - | 6329 | `	sxu32 nOfft;` |
|      - | 6330 | `	sxi32 rc;` |
|      - | 6331 | `	/* Point to the working buffer */` |
|      9 | 6332 | `	pWorker = pRepData->pWorker;` |
|      9 | 6333 | `	if( !ph7_value_is_string(pKey) ){` |
|      - | 6334 | `		/* Target and replace must be a string */` |
|      3 | 6335 | `		return PH7_OK;` |
|      - | 6336 | `	}` |
|      - | 6337 | `	/* Extract the target and the replace */` |
|      7 | 6338 | `	zTarget = ph7_value_to_string(pKey,&tLen);` |
|      7 | 6339 | `	if( tLen < 1 ){` |
|      - | 6340 | `		/* Empty target,return immediately */` |
|    ! 0 | 6341 | `		return PH7_OK;` |
|      - | 6342 | `	}` |
|      - | 6343 | `	/* Perform a pattern search */` |
|      7 | 6344 | `	rc = pRepData->xMatch(SyBlobData(pWorker),SyBlobLength(pWorker),(const void *)zTarget,(sxu32)tLen,&nOfft);` |
|      7 | 6345 | `	if( rc != SXRET_OK ){` |
|      - | 6346 | `		/* Pattern not found */` |
|    ! 0 | 6347 | `		return PH7_OK;` |
|      - | 6348 | `	}` |
|      - | 6349 | `	/* Extract the replace string */` |
|      7 | 6350 | `	zReplace = ph7_value_to_string(pData,&nLen);` |
|      - | 6351 | `	/* Perform the replace process */` |
|      7 | 6352 | `	StringReplace(pWorker,nOfft,tLen,zReplace,nLen);` |
|      - | 6353 | `	/* All done */` |
|      7 | 6354 | `	return PH7_OK;` |
|      5 | 6355 |  |
|      - | 6356 | `/*` |
|      - | 6357 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 6358 | ` * to collect search/replace string.` |
|      - | 6359 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 6360 | ` */` |
|     26 | 6361 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6362 |  |
|     27 | 6363 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 6364 | `	SyString sWorker;` |
|      - | 6365 | `	const char *zIn;` |
|      - | 6366 | `	int nByte;` |
|      - | 6367 | `	/* Extract a string representation of the given argument */` |
|     27 | 6368 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 6369 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 6370 | `	if( nByte > 0 ){` |
|      - | 6371 | `		char *zDup;` |
|      - | 6372 | `		/* Duplicate the chunk */` |
|     25 | 6373 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 6374 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 6375 | `			);` |
|     25 | 6376 | `		if( zDup == 0 ){` |
|      - | 6377 | `			/* Ignore any memory failure problem */` |
|    ! 0 | 6378 | `			ph7_context_throw_error(pRep->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 6379 | `			return PH7_OK;` |
|      - | 6380 | `		}` |
|     25 | 6381 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 6382 | `		/* Save the chunk */` |
|     25 | 6383 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 6384 | `	}` |
|      - | 6385 | `	/* Save for later processing */` |
|     27 | 6386 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 6387 | `	/* All done */` |
|     13 | 6388 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 6389 | `	return PH7_OK;` |
|     14 | 6390 |  |
|      - | 6391 | `/*` |
|      - | 6392 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6393 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6394 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 6395 | ` * Parameters` |
|      - | 6396 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 6397 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 6398 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 6399 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 6400 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 6401 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 6402 | ` * $search` |
|      - | 6403 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 6404 | ` *  to designate multiple needles.` |
|      - | 6405 | ` * $replace` |
|      - | 6406 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 6407 | ` *  to designate multiple replacements.` |
|      - | 6408 | ` * $subject` |
|      - | 6409 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 6410 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 6411 | ` *  of subject, and the return value is an array as well.` |
|      - | 6412 | ` * $count (Not used)` |
|      - | 6413 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 6414 | ` * Return` |
|      - | 6415 | ` * This function returns a string or an array with the replaced values.` |
|      - | 6416 | ` */` |
|  15634 | 6417 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6418 |  |
|      - | 6419 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 6420 | `	ProcStringMatch xMatch;` |
|      - | 6421 | `	const char *zIn,*zFunc;` |
|      - | 6422 | `	str_replace_data sRep;` |
|      - | 6423 | `	SyBlob sWorker;` |
|      - | 6424 | `	SySet sReplace;` |
|      - | 6425 | `	SySet sSearch;` |
|      - | 6426 | `	int rep_str;` |
|      - | 6427 | `	int nByte;` |
|      - | 6428 | `	sxi32 rc;` |
|  15636 | 6429 | `	if( nArg < 3 ){` |
|      - | 6430 | `		/* Missing/Invalid arguments,return null */` |
|      7 | 6431 | `		ph7_result_null(pCtx);` |
|      7 | 6432 | `		return PH7_OK;` |
|      - | 6433 | `	}` |
|      - | 6434 | `	/* Initialize fields */` |
|  15630 | 6435 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  15630 | 6436 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  15630 | 6437 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  15630 | 6438 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  15630 | 6439 | `	sRep.pCtx = pCtx;` |
|  15630 | 6440 | `	sRep.pCollector = &sSearch;` |
|  15630 | 6441 | `	rep_str = 0;` |
|      - | 6442 | `	/* Extract the subject */` |
|  15630 | 6443 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  15630 | 6444 | `	if( nByte < 1 ){` |
|      - | 6445 | `		/* Nothing to replace,return the empty string */` |
|     38 | 6446 | `		ph7_result_string(pCtx,"",0);` |
|     38 | 6447 | `		return PH7_OK;` |
|      - | 6448 | `	}` |
|      - | 6449 | `	/* Copy the subject */` |
|  15594 | 6450 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 6451 | `	/* Search string */` |
|  15594 | 6452 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 6453 | `		/* Collect search string */` |
|      9 | 6454 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 6455 | `	}else{` |
|      - | 6456 | `		/* Single pattern */` |
|  15586 | 6457 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  15586 | 6458 | `		if( nByte < 1 ){` |
|      - | 6459 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 6460 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 6461 | `			return PH7_OK;` |
|      - | 6462 | `		}` |
|  15582 | 6463 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6464 | `		/* Save for later processing */` |
|  15582 | 6465 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 6466 | `	}` |
|      - | 6467 | `	/* Replace string */` |
|  15590 | 6468 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 6469 | `		/* Collect replace string */` |
|      7 | 6470 | `		sRep.pCollector = &sReplace;` |
|      7 | 6471 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 6472 | `	}else{` |
|      - | 6473 | `		/* Single needle */` |
|  15584 | 6474 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  15584 | 6475 | `		rep_str = 1;` |
|  15584 | 6476 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6477 | `		/* Save for later processing */` |
|  15584 | 6478 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 6479 | `	}` |
|      - | 6480 | `	/* Reset loop cursors */` |
|  15590 | 6481 | `	SySetResetCursor(&sSearch);` |
|  15590 | 6482 | `	SySetResetCursor(&sReplace);` |
|  15590 | 6483 | `	pReplace = pSearch = 0; /* cc warning */` |
|  15590 | 6484 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 6485 | `	/* Extract function name */` |
|  15590 | 6486 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 6487 | `	/* Set the default pattern match routine */` |
|  15590 | 6488 | `	xMatch = SyBlobSearch;` |
|  15590 | 6489 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 6490 | `		/* Case insensitive pattern match */` |
|     11 | 6491 | `		xMatch = iPatternMatch;` |
|      5 | 6492 | `	}` |
|      - | 6493 | `	/* Start the replace process */` |
|  31186 | 6494 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 6495 | `		sxu32 nCount,nOfft;` |
|  15598 | 6496 | `		if( pSearch->nByte <  1 ){` |
|      - | 6497 | `			/* Empty string,ignore */` |
|      3 | 6498 | `			continue;` |
|      - | 6499 | `		}` |
|      - | 6500 | `		/* Extract the replace string */` |
|  15596 | 6501 | `		if( rep_str ){` |
|  15586 | 6502 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|   7794 | 6503 | `		}else{` |
|     11 | 6504 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 6505 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 6506 | `				 * An empty string is used for the rest of replacement values` |
|      - | 6507 | `				 */` |
|      3 | 6508 | `				pReplace = 0;` |
|      1 | 6509 | `			}` |
|      - | 6510 | `		}` |
|  15596 | 6511 | `		if( pReplace == 0 ){` |
|      - | 6512 | `			/* Use an empty string instead */` |
|      3 | 6513 | `			pReplace = &sTemp;` |
|      1 | 6514 | `		}` |
|  15596 | 6515 | `		nOfft = nCount = 0;` |
|   7813 | 6516 | `		for(;;){` |
|  15628 | 6517 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 6518 | `				break;` |
|      - | 6519 | `			}` |
|      - | 6520 | `			/* Perform a pattern lookup */` |
|  23423 | 6521 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  15614 | 6522 | `				pSearch->nByte,&nOfft);` |
|  15616 | 6523 | `			if( rc != SXRET_OK ){` |
|      - | 6524 | `				/* Pattern not found */` |
|  15584 | 6525 | `				break;` |
|      - | 6526 | `			}` |
|      - | 6527 | `			/* Perform the replace operation */` |
|     33 | 6528 | `			StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|      - | 6529 | `			/* Increment offset counter */` |
|     33 | 6530 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 6531 | `		}` |
|      2 | 6532 | `	}` |
|      - | 6533 | `	/* All done,clean-up the mess left behind */` |
|  15590 | 6534 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  15590 | 6535 | `	SySetRelease(&sSearch);` |
|  15590 | 6536 | `	SySetRelease(&sReplace);` |
|  15590 | 6537 | `	SyBlobRelease(&sWorker);` |
|  15590 | 6538 | `	return PH7_OK;` |
|   7819 | 6539 |  |
|      - | 6540 | `/*` |
|      - | 6541 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 6542 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 6543 | ` *  Translate characters or replace substrings.` |
|      - | 6544 | ` * Parameters` |
|      - | 6545 | ` *  $str` |
|      - | 6546 | ` *  The string being translated.` |
|      - | 6547 | ` * $from` |
|      - | 6548 | ` *  The string being translated to to.` |
|      - | 6549 | ` * $to` |
|      - | 6550 | ` *  The string replacing from.` |
|      - | 6551 | ` * $replace_pairs` |
|      - | 6552 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 6553 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 6554 | ` * Return` |
|      - | 6555 | ` *  The translated string.` |
|      - | 6556 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 6557 | ` */` |
|     12 | 6558 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6559 |  |
|      - | 6560 | `	const char *zIn;` |
|      - | 6561 | `	int nLen;` |
|     13 | 6562 | `	if( nArg < 1 ){` |
|      - | 6563 | `		/* Nothing to replace,return FALSE */` |
|      7 | 6564 | `		ph7_result_bool(pCtx,0);` |
|      7 | 6565 | `		return PH7_OK;` |
|      - | 6566 | `	}` |
|      7 | 6567 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 6568 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 6569 | `		/* Invalid arguments */` |
|    ! 0 | 6570 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6571 | `		return PH7_OK;` |
|      - | 6572 | `	}` |
|      9 | 6573 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 6574 | `		str_replace_data sRepData;` |
|      - | 6575 | `		SyBlob sWorker;` |
|      - | 6576 | `		/* Initilaize the working buffer */` |
|      5 | 6577 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|      - | 6578 | `		/* Copy raw string */` |
|      5 | 6579 | `		SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nLen);` |
|      - | 6580 | `		/* Init our replace data instance */` |
|      5 | 6581 | `		sRepData.pWorker = &sWorker;` |
|      5 | 6582 | `		sRepData.xMatch = SyBlobSearch;` |
|      - | 6583 | `		/* Iterate throw array entries and perform the replace operation.*/` |
|      5 | 6584 | `		ph7_array_walk(apArg[1],StringReplaceWalker,&sRepData);` |
|      - | 6585 | `		/* All done, return the result string */` |
|      7 | 6586 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|      4 | 6587 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 6588 | `		/* Clean-up */` |
|      5 | 6589 | `		SyBlobRelease(&sWorker);` |
|      3 | 6590 | `	}else{` |
|      - | 6591 | `		int i,flen,tlen,c,iOfft;` |
|      - | 6592 | `		const char *zFrom,*zTo;` |
|      3 | 6593 | `		if( nArg < 3 ){` |
|      - | 6594 | `			/* Nothing to replace */` |
|    ! 0 | 6595 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6596 | `			return PH7_OK;` |
|      - | 6597 | `		}` |
|      - | 6598 | `		/* Extract given arguments */` |
|      3 | 6599 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 6600 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 6601 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 6602 | `			/* Nothing to replace */` |
|    ! 0 | 6603 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6604 | `			return PH7_OK;` |
|      - | 6605 | `		}` |
|      - | 6606 | `		/* Start the replace process */` |
|     13 | 6607 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 6608 | `			c = zIn[i];` |
|     11 | 6609 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 6610 | `				if ( iOfft < tlen ){` |
|      5 | 6611 | `					c = zTo[iOfft];` |
|      2 | 6612 | `				}` |
|      2 | 6613 | `			}` |
|     11 | 6614 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 6615 |  |
|      6 | 6616 | `		}` |
|      - | 6617 | `	}` |
|      7 | 6618 | `	return PH7_OK;` |
|      7 | 6619 |  |
|      - | 6620 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6621 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6622 | `/*` |
|      - | 6623 | ` * Parse an INI string.` |
|      - | 6624 |  |
|      - | 6625 | ` * According to wikipedia` |
|      - | 6626 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 6627 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 6628 | ` *  Format` |
|      - | 6629 | `*    Properties` |
|      - | 6630 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 6631 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 6632 | `*     Example:` |
|      - | 6633 | `*      name=value` |
|      - | 6634 | `*    Sections` |
|      - | 6635 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 6636 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 6637 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 6638 | `*     or the end of the file. Sections may not be nested.` |
|      - | 6639 | `*     Example:` |
|      - | 6640 | `*      [section]` |
|      - | 6641 | `*   Comments` |
|      - | 6642 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 6643 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 6644 | `*/` |
|     12 | 6645 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 6646 |  |
|      - | 6647 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 6648 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 6649 | `	SyHashEntry *pEntry;` |
|      - | 6650 | `	SyString sEntry;` |
|      - | 6651 | `	SyHash sHash;` |
|      - | 6652 | `	int c;` |
|      - | 6653 | `	/* Create an empty array and worker variables */` |
|     13 | 6654 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 6655 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 6656 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 6657 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 6658 | `		/* Out of memory */` |
|    ! 0 | 6659 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      - | 6660 | `		/* Return FALSE */` |
|    ! 0 | 6661 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6662 | `		return PH7_OK;` |
|      - | 6663 | `	}` |
|     13 | 6664 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 6665 | `	pCur = pArray;` |
|      - | 6666 | `	/* Start the parse process */` |
|     21 | 6667 | `	for(;;){` |
|      - | 6668 | `		/* Ignore leading white spaces */` |
|     69 | 6669 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 6670 | `			zIn++;` |
|      1 | 6671 | `		}` |
|     43 | 6672 | `		if( zIn >= zEnd ){` |
|      - | 6673 | `			/* No more input to process */` |
|     13 | 6674 | `			break;` |
|      - | 6675 | `		}` |
|     31 | 6676 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6677 | `			/* Comment til the end of line */` |
|    ! 0 | 6678 | `			zIn++;` |
|    ! 0 | 6679 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 6680 | `				zIn++;` |
|    ! 0 | 6681 | `			}` |
|    ! 0 | 6682 | `			continue;` |
|      - | 6683 | `		}` |
|      - | 6684 | `		/* Reset the string cursor of the working variable */` |
|     31 | 6685 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 6686 | `		if( zIn[0] == '[' ){` |
|      - | 6687 | `			/* Section: Extract the section name */` |
|      9 | 6688 | `			zIn++;` |
|      9 | 6689 | `			zCur = zIn;` |
|     73 | 6690 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 6691 | `				zIn++;` |
|      1 | 6692 | `			}` |
|      9 | 6693 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 6694 | `				/* Save the section name */` |
|      5 | 6695 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 6696 | `				SyStringFullTrim(&sEntry);` |
|      5 | 6697 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 6698 | `				if( sEntry.nByte > 0 ){` |
|      - | 6699 | `					/* Associate an array with the section */` |
|      5 | 6700 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 6701 | `					if( pSection ){` |
|      5 | 6702 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 6703 | `						pCur = pSection;` |
|      2 | 6704 | `					}` |
|      2 | 6705 | `				}` |
|      2 | 6706 | `			}` |
|      9 | 6707 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 6708 | `		}else{` |
|      - | 6709 | `			ph7_value *pOldCur;` |
|      - | 6710 | `			int is_array;` |
|      - | 6711 | `			int iLen;` |
|      - | 6712 | `			/* Properties */` |
|     23 | 6713 | `			is_array = 0;` |
|     23 | 6714 | `			zCur = zIn;` |
|     23 | 6715 | `			iLen = 0; /* cc warning */` |
|     23 | 6716 | `			pOldCur = pCur;` |
|    155 | 6717 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 6718 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 6719 | `					/* Array */` |
|    ! 0 | 6720 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 6721 | `					is_array = 1;` |
|    ! 0 | 6722 | `					if( iLen > 0 ){` |
|    ! 0 | 6723 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 6724 | `						/* Query the hashtable */` |
|    ! 0 | 6725 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 6726 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 6727 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 6728 | `						if( pEntry ){` |
|    ! 0 | 6729 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 6730 | `						}else{` |
|      - | 6731 | `							/* Create an empty array */` |
|    ! 0 | 6732 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 6733 | `							if( pvArr ){` |
|      - | 6734 | `								/* Save the entry */` |
|    ! 0 | 6735 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 6736 | `								/* Insert the entry */` |
|    ! 0 | 6737 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6738 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 6739 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 6740 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6741 | `							}` |
|      - | 6742 | `						}` |
|    ! 0 | 6743 | `						if( pvArr ){` |
|    ! 0 | 6744 | `							pCur = pvArr;` |
|    ! 0 | 6745 | `						}` |
|    ! 0 | 6746 | `					}` |
|    ! 0 | 6747 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 6748 | `						zIn++;` |
|    ! 0 | 6749 | `					}` |
|    ! 0 | 6750 | `				}` |
|    133 | 6751 | `				zIn++;` |
|      1 | 6752 | `			}` |
|     23 | 6753 | `			if( !is_array ){` |
|     23 | 6754 | `				iLen = (int)(zIn-zCur);` |
|     11 | 6755 | `			}` |
|      - | 6756 | `			/* Trim the key */` |
|     23 | 6757 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 6758 | `			SyStringFullTrim(&sEntry);` |
|     23 | 6759 | `			if( sEntry.nByte > 0 ){` |
|     23 | 6760 | `				if( !is_array ){` |
|      - | 6761 | `					/* Save the key name */` |
|     23 | 6762 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 6763 | `				}` |
|      - | 6764 | `				/* extract key value */` |
|     23 | 6765 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 6766 | `				zIn++; /* '=' */` |
|     39 | 6767 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 6768 | `					zIn++;` |
|      1 | 6769 | `				}` |
|     23 | 6770 | `				if( zIn < zEnd ){` |
|     21 | 6771 | `					zCur = zIn;` |
|     21 | 6772 | `					c = zIn[0];` |
|     21 | 6773 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6774 | `						zIn++;` |
|      - | 6775 | `						/* Delimit the value */` |
|    ! 0 | 6776 | `						while( zIn < zEnd ){` |
|    ! 0 | 6777 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 6778 | `								break;` |
|      - | 6779 | `							}` |
|    ! 0 | 6780 | `							zIn++;` |
|    ! 0 | 6781 | `						}` |
|    ! 0 | 6782 | `						if( zIn < zEnd ){` |
|    ! 0 | 6783 | `							zIn++;` |
|    ! 0 | 6784 | `						}` |
|    ! 0 | 6785 | `					}else{` |
|    125 | 6786 | `						while( zIn < zEnd ){` |
|    123 | 6787 | `							if( zIn[0] == '\n' ){` |
|     19 | 6788 | `								if( zIn[-1] != '\\' ){` |
|     19 | 6789 | `									break;` |
|    ! 0 | 6790 | `								}` |
|    105 | 6791 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6792 | `								/* Inline comments */` |
|    ! 0 | 6793 | `								break;` |
|      - | 6794 | `							}` |
|    105 | 6795 | `							zIn++;` |
|      1 | 6796 | `						}` |
|      - | 6797 | `					}` |
|      - | 6798 | `					/* Trim the value */` |
|     21 | 6799 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 6800 | `					SyStringFullTrim(&sEntry);` |
|     21 | 6801 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6802 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 6803 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 6804 | `					}` |
|     21 | 6805 | `					if( sEntry.nByte > 0 ){` |
|     21 | 6806 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 6807 | `					}` |
|      - | 6808 | `					/* Insert the key and it's value */` |
|     21 | 6809 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 6810 | `				}` |
|     12 | 6811 | `			}else{` |
|    ! 0 | 6812 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 6813 | `					zIn++;` |
|    ! 0 | 6814 | `				}` |
|      - | 6815 | `			}` |
|     23 | 6816 | `			pCur = pOldCur;` |
|      - | 6817 | `		}` |
|      1 | 6818 | `	}` |
|     13 | 6819 | `	SyHashRelease(&sHash);` |
|      - | 6820 | `	/* Return the parse of the INI string */` |
|     13 | 6821 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 6822 | `	return SXRET_OK;` |
|      7 | 6823 |  |
|      - | 6824 | `/*` |
|      - | 6825 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 6826 | ` *  Parse a configuration string.` |
|      - | 6827 | ` * Parameters` |
|      - | 6828 | ` *  $ini` |
|      - | 6829 | ` *   The contents of the ini file being parsed.` |
|      - | 6830 | ` *  $process_sections` |
|      - | 6831 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 6832 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 6833 | ` *  $scanner_mode (Not used)` |
|      - | 6834 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 6835 | ` *   then option values will not be parsed.` |
|      - | 6836 | ` * Return` |
|      - | 6837 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 6838 | ` */` |
|     10 | 6839 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6840 |  |
|      - | 6841 | `	const char *zIni;` |
|      - | 6842 | `	int nByte;` |
|     11 | 6843 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6844 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 6845 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6846 | `		return PH7_OK;` |
|      - | 6847 | `	}` |
|      - | 6848 | `	/* Extract the raw INI buffer */` |
|     11 | 6849 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 6850 | `	/* Process the INI buffer*/` |
|     11 | 6851 | `	PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|     11 | 6852 | `	return PH7_OK;` |
|      6 | 6853 |  |
|      - | 6854 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6855 |  |
|      - | 6856 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6857 |  |
|      - | 6858 | `/*` |
|      - | 6859 | ` * Ctype Functions.` |
|      - | 6860 | ` * Status:` |
|      - | 6861 | ` *    Stable.` |
|      - | 6862 | ` */` |
|      - | 6863 | `/*` |
|      - | 6864 | ` * bool ctype_alnum(string $text)` |
|      - | 6865 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 6866 | ` * Parameters` |
|      - | 6867 | ` *  $text` |
|      - | 6868 | ` *   The tested string.` |
|      - | 6869 | ` * Return` |
|      - | 6870 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 6871 | ` */` |
|     16 | 6872 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6873 |  |
|      - | 6874 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6875 | `	int nLen;` |
|     17 | 6876 | `	if( nArg < 1 ){` |
|      - | 6877 | `		/* Missing arguments,return FALSE */` |
|      3 | 6878 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6879 | `		return PH7_OK;` |
|      - | 6880 | `	}` |
|      - | 6881 | `	/* Extract the target string */` |
|     15 | 6882 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 6883 | `	zEnd = &zIn[nLen];` |
|     15 | 6884 | `	if( nLen < 1 ){` |
|      - | 6885 | `		/* Empty string,return FALSE */` |
|      3 | 6886 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6887 | `		return PH7_OK;` |
|      - | 6888 | `	}` |
|      - | 6889 | `	/* Perform the requested operation */` |
|     32 | 6890 | `	for(;;){` |
|     65 | 6891 | `		if( zIn >= zEnd ){` |
|      - | 6892 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6893 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6894 | `			return PH7_OK;` |
|      - | 6895 | `		}` |
|     57 | 6896 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 6897 | `			break;` |
|      - | 6898 | `		}` |
|      - | 6899 | `		/* Point to the next character */` |
|     53 | 6900 | `		zIn++;` |
|      1 | 6901 | `	}` |
|      - | 6902 | `	/* The test failed,return FALSE */` |
|      5 | 6903 | `	ph7_result_bool(pCtx,0);` |
|      5 | 6904 | `	return PH7_OK;` |
|      9 | 6905 |  |
|      - | 6906 | `/*` |
|      - | 6907 | ` * bool ctype_alpha(string $text)` |
|      - | 6908 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 6909 | ` * Parameters` |
|      - | 6910 | ` *  $text` |
|      - | 6911 | ` *   The tested string.` |
|      - | 6912 | ` * Return` |
|      - | 6913 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 6914 | ` */` |
|     18 | 6915 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6916 |  |
|      - | 6917 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6918 | `	int nLen;` |
|     19 | 6919 | `	if( nArg < 1 ){` |
|      - | 6920 | `		/* Missing arguments,return FALSE */` |
|      3 | 6921 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6922 | `		return PH7_OK;` |
|      - | 6923 | `	}` |
|      - | 6924 | `	/* Extract the target string */` |
|     17 | 6925 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6926 | `	zEnd = &zIn[nLen];` |
|     17 | 6927 | `	if( nLen < 1 ){` |
|      - | 6928 | `		/* Empty string,return FALSE */` |
|      3 | 6929 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6930 | `		return PH7_OK;` |
|      - | 6931 | `	}` |
|      - | 6932 | `	/* Perform the requested operation */` |
|     42 | 6933 | `	for(;;){` |
|     85 | 6934 | `		if( zIn >= zEnd ){` |
|      - | 6935 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6936 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6937 | `			return PH7_OK;` |
|      - | 6938 | `		}` |
|     77 | 6939 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 6940 | `			break;` |
|      - | 6941 | `		}` |
|      - | 6942 | `		/* Point to the next character */` |
|     71 | 6943 | `		zIn++;` |
|      1 | 6944 | `	}` |
|      - | 6945 | `	/* The test failed,return FALSE */` |
|      7 | 6946 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6947 | `	return PH7_OK;` |
|     10 | 6948 |  |
|      - | 6949 | `/*` |
|      - | 6950 | ` * bool ctype_cntrl(string $text)` |
|      - | 6951 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 6952 | ` * Parameters` |
|      - | 6953 | ` *  $text` |
|      - | 6954 | ` *   The tested string.` |
|      - | 6955 | ` * Return` |
|      - | 6956 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 6957 | ` */` |
|     18 | 6958 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6959 |  |
|      - | 6960 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6961 | `	int nLen;` |
|     19 | 6962 | `	if( nArg < 1 ){` |
|      - | 6963 | `		/* Missing arguments,return FALSE */` |
|      3 | 6964 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6965 | `		return PH7_OK;` |
|      - | 6966 | `	}` |
|      - | 6967 | `	/* Extract the target string */` |
|     17 | 6968 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6969 | `	zEnd = &zIn[nLen];` |
|     17 | 6970 | `	if( nLen < 1 ){` |
|      - | 6971 | `		/* Empty string,return FALSE */` |
|      3 | 6972 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6973 | `		return PH7_OK;` |
|      - | 6974 | `	}` |
|      - | 6975 | `	/* Perform the requested operation */` |
|     14 | 6976 | `	for(;;){` |
|     29 | 6977 | `		if( zIn >= zEnd ){` |
|      - | 6978 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6979 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6980 | `			return PH7_OK;` |
|      - | 6981 | `		}` |
|     21 | 6982 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6983 | `			/* UTF-8 stream  */` |
|    ! 0 | 6984 | `			break;` |
|      - | 6985 | `		}` |
|     21 | 6986 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 6987 | `			break;` |
|      - | 6988 | `		}` |
|      - | 6989 | `		/* Point to the next character */` |
|     15 | 6990 | `		zIn++;` |
|      1 | 6991 | `	}` |
|      - | 6992 | `	/* The test failed,return FALSE */` |
|      7 | 6993 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6994 | `	return PH7_OK;` |
|     10 | 6995 |  |
|      - | 6996 | `/*` |
|      - | 6997 | ` * bool ctype_digit(string $text)` |
|      - | 6998 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 6999 | ` * Parameters` |
|      - | 7000 | ` *  $text` |
|      - | 7001 | ` *   The tested string.` |
|      - | 7002 | ` * Return` |
|      - | 7003 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 7004 | ` */` |
|   1924 | 7005 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 7006 |  |
|      - | 7007 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7008 | `	int nLen;` |
|   1926 | 7009 | `	if( nArg < 1 ){` |
|      - | 7010 | `		/* Missing arguments,return FALSE */` |
|      3 | 7011 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7012 | `		return PH7_OK;` |
|      - | 7013 | `	}` |
|      - | 7014 | `	/* Extract the target string */` |
|   1924 | 7015 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1924 | 7016 | `	zEnd = &zIn[nLen];` |
|   1924 | 7017 | `	if( nLen < 1 ){` |
|      - | 7018 | `		/* Empty string,return FALSE */` |
|      3 | 7019 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7020 | `		return PH7_OK;` |
|      - | 7021 | `	}` |
|      - | 7022 | `	/* Perform the requested operation */` |
|   1768 | 7023 | `	for(;;){` |
|   3538 | 7024 | `		if( zIn >= zEnd ){` |
|      - | 7025 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1586 | 7026 | `			ph7_result_bool(pCtx,1);` |
|   1586 | 7027 | `			return PH7_OK;` |
|      - | 7028 | `		}` |
|   1954 | 7029 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7030 | `			/* UTF-8 stream  */` |
|    ! 0 | 7031 | `			break;` |
|      - | 7032 | `		}` |
|   1954 | 7033 | `		if( !SyisDigit(zIn[0]) ){` |
|    338 | 7034 | `			break;` |
|      - | 7035 | `		}` |
|      - | 7036 | `		/* Point to the next character */` |
|   1618 | 7037 | `		zIn++;` |
|      2 | 7038 | `	}` |
|      - | 7039 | `	/* The test failed,return FALSE */` |
|    338 | 7040 | `	ph7_result_bool(pCtx,0);` |
|    338 | 7041 | `	return PH7_OK;` |
|    964 | 7042 |  |
|      - | 7043 | `/*` |
|      - | 7044 | ` * bool ctype_xdigit(string $text)` |
|      - | 7045 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 7046 | ` * Parameters` |
|      - | 7047 | ` *  $text` |
|      - | 7048 | ` *   The tested string.` |
|      - | 7049 | ` * Return` |
|      - | 7050 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 7051 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 7052 | ` */` |
|     20 | 7053 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7054 |  |
|      - | 7055 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7056 | `	int nLen;` |
|     21 | 7057 | `	if( nArg < 1 ){` |
|      - | 7058 | `		/* Missing arguments,return FALSE */` |
|      3 | 7059 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7060 | `		return PH7_OK;` |
|      - | 7061 | `	}` |
|      - | 7062 | `	/* Extract the target string */` |
|     19 | 7063 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 7064 | `	zEnd = &zIn[nLen];` |
|     19 | 7065 | `	if( nLen < 1 ){` |
|      - | 7066 | `		/* Empty string,return FALSE */` |
|      3 | 7067 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7068 | `		return PH7_OK;` |
|      - | 7069 | `	}` |
|      - | 7070 | `	/* Perform the requested operation */` |
|     46 | 7071 | `	for(;;){` |
|     93 | 7072 | `		if( zIn >= zEnd ){` |
|      - | 7073 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 7074 | `			ph7_result_bool(pCtx,1);` |
|     11 | 7075 | `			return PH7_OK;` |
|      - | 7076 | `		}` |
|     83 | 7077 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7078 | `			/* UTF-8 stream  */` |
|    ! 0 | 7079 | `			break;` |
|      - | 7080 | `		}` |
|     83 | 7081 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 7082 | `			break;` |
|      - | 7083 | `		}` |
|      - | 7084 | `		/* Point to the next character */` |
|     77 | 7085 | `		zIn++;` |
|      1 | 7086 | `	}` |
|      - | 7087 | `	/* The test failed,return FALSE */` |
|      7 | 7088 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7089 | `	return PH7_OK;` |
|     11 | 7090 |  |
|      - | 7091 | `/*` |
|      - | 7092 | ` * bool ctype_graph(string $text)` |
|      - | 7093 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 7094 | ` * Parameters` |
|      - | 7095 | ` *  $text` |
|      - | 7096 | ` *   The tested string.` |
|      - | 7097 | ` * Return` |
|      - | 7098 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 7099 | ` * (no white space), FALSE otherwise.` |
|      - | 7100 | ` */` |
|     18 | 7101 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7102 |  |
|      - | 7103 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7104 | `	int nLen;` |
|     19 | 7105 | `	if( nArg < 1 ){` |
|      - | 7106 | `		/* Missing arguments,return FALSE */` |
|      3 | 7107 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7108 | `		return PH7_OK;` |
|      - | 7109 | `	}` |
|      - | 7110 | `	/* Extract the target string */` |
|     17 | 7111 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7112 | `	zEnd = &zIn[nLen];` |
|     17 | 7113 | `	if( nLen < 1 ){` |
|      - | 7114 | `		/* Empty string,return FALSE */` |
|      3 | 7115 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7116 | `		return PH7_OK;` |
|      - | 7117 | `	}` |
|      - | 7118 | `	/* Perform the requested operation */` |
|     57 | 7119 | `	for(;;){` |
|    115 | 7120 | `		if( zIn >= zEnd ){` |
|      - | 7121 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7122 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7123 | `			return PH7_OK;` |
|      - | 7124 | `		}` |
|    107 | 7125 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7126 | `			/* UTF-8 stream  */` |
|    ! 0 | 7127 | `			break;` |
|      - | 7128 | `		}` |
|    107 | 7129 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 7130 | `			break;` |
|      - | 7131 | `		}` |
|      - | 7132 | `		/* Point to the next character */` |
|    101 | 7133 | `		zIn++;` |
|      1 | 7134 | `	}` |
|      - | 7135 | `	/* The test failed,return FALSE */` |
|      7 | 7136 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7137 | `	return PH7_OK;` |
|     10 | 7138 |  |
|      - | 7139 | `/*` |
|      - | 7140 | ` * bool ctype_print(string $text)` |
|      - | 7141 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 7142 | ` * Parameters` |
|      - | 7143 | ` *  $text` |
|      - | 7144 | ` *   The tested string.` |
|      - | 7145 | ` * Return` |
|      - | 7146 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 7147 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 7148 | ` *  or control function at all.` |
|      - | 7149 | ` */` |
|     18 | 7150 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7151 |  |
|      - | 7152 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7153 | `	int nLen;` |
|     19 | 7154 | `	if( nArg < 1 ){` |
|      - | 7155 | `		/* Missing arguments,return FALSE */` |
|      3 | 7156 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7157 | `		return PH7_OK;` |
|      - | 7158 | `	}` |
|      - | 7159 | `	/* Extract the target string */` |
|     17 | 7160 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7161 | `	zEnd = &zIn[nLen];` |
|     17 | 7162 | `	if( nLen < 1 ){` |
|      - | 7163 | `		/* Empty string,return FALSE */` |
|      3 | 7164 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7165 | `		return PH7_OK;` |
|      - | 7166 | `	}` |
|      - | 7167 | `	/* Perform the requested operation */` |
|     63 | 7168 | `	for(;;){` |
|    127 | 7169 | `		if( zIn >= zEnd ){` |
|      - | 7170 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7171 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7172 | `			return PH7_OK;` |
|      - | 7173 | `		}` |
|    119 | 7174 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7175 | `			/* UTF-8 stream  */` |
|    ! 0 | 7176 | `			break;` |
|      - | 7177 | `		}` |
|    119 | 7178 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 7179 | `			break;` |
|      - | 7180 | `		}` |
|      - | 7181 | `		/* Point to the next character */` |
|    113 | 7182 | `		zIn++;` |
|      1 | 7183 | `	}` |
|      - | 7184 | `	/* The test failed,return FALSE */` |
|      7 | 7185 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7186 | `	return PH7_OK;` |
|     10 | 7187 |  |
|      - | 7188 | `/*` |
|      - | 7189 | ` * bool ctype_punct(string $text)` |
|      - | 7190 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 7191 | ` * Parameters` |
|      - | 7192 | ` *  $text` |
|      - | 7193 | ` *   The tested string.` |
|      - | 7194 | ` * Return` |
|      - | 7195 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 7196 | ` *  digit or blank, FALSE otherwise.` |
|      - | 7197 | ` */` |
|     20 | 7198 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7199 |  |
|      - | 7200 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7201 | `	int nLen;` |
|     21 | 7202 | `	if( nArg < 1 ){` |
|      - | 7203 | `		/* Missing arguments,return FALSE */` |
|      3 | 7204 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7205 | `		return PH7_OK;` |
|      - | 7206 | `	}` |
|      - | 7207 | `	/* Extract the target string */` |
|     19 | 7208 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 7209 | `	zEnd = &zIn[nLen];` |
|     19 | 7210 | `	if( nLen < 1 ){` |
|      - | 7211 | `		/* Empty string,return FALSE */` |
|      3 | 7212 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7213 | `		return PH7_OK;` |
|      - | 7214 | `	}` |
|      - | 7215 | `	/* Perform the requested operation */` |
|     38 | 7216 | `	for(;;){` |
|     77 | 7217 | `		if( zIn >= zEnd ){` |
|      - | 7218 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7219 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7220 | `			return PH7_OK;` |
|      - | 7221 | `		}` |
|     69 | 7222 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7223 | `			/* UTF-8 stream  */` |
|    ! 0 | 7224 | `			break;` |
|      - | 7225 | `		}` |
|     69 | 7226 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 7227 | `			break;` |
|      - | 7228 | `		}` |
|      - | 7229 | `		/* Point to the next character */` |
|     61 | 7230 | `		zIn++;` |
|      1 | 7231 | `	}` |
|      - | 7232 | `	/* The test failed,return FALSE */` |
|      9 | 7233 | `	ph7_result_bool(pCtx,0);` |
|      9 | 7234 | `	return PH7_OK;` |
|     11 | 7235 |  |
|      - | 7236 | `/*` |
|      - | 7237 | ` * bool ctype_space(string $text)` |
|      - | 7238 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 7239 | ` * Parameters` |
|      - | 7240 | ` *  $text` |
|      - | 7241 | ` *   The tested string.` |
|      - | 7242 | ` * Return` |
|      - | 7243 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 7244 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 7245 | ` *  and form feed characters.` |
|      - | 7246 | ` */` |
|  70976 | 7247 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 7248 |  |
|      - | 7249 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7250 | `	int nLen;` |
|  70978 | 7251 | `	if( nArg < 1 ){` |
|      - | 7252 | `		/* Missing arguments,return FALSE */` |
|      3 | 7253 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7254 | `		return PH7_OK;` |
|      - | 7255 | `	}` |
|      - | 7256 | `	/* Extract the target string */` |
|  70976 | 7257 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  70976 | 7258 | `	zEnd = &zIn[nLen];` |
|  70976 | 7259 | `	if( nLen < 1 ){` |
|      - | 7260 | `		/* Empty string,return FALSE */` |
|      3 | 7261 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7262 | `		return PH7_OK;` |
|      - | 7263 | `	}` |
|      - | 7264 | `	/* Perform the requested operation */` |
|  36186 | 7265 | `	for(;;){` |
|  72330 | 7266 | `		if( zIn >= zEnd ){` |
|      - | 7267 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1334 | 7268 | `			ph7_result_bool(pCtx,1);` |
|   1334 | 7269 | `			return PH7_OK;` |
|      - | 7270 | `		}` |
|  70998 | 7271 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7272 | `			/* UTF-8 stream  */` |
|    ! 0 | 7273 | `			break;` |
|      - | 7274 | `		}` |
|  70998 | 7275 | `		if( !SyisSpace(zIn[0]) ){` |
|  69642 | 7276 | `			break;` |
|      - | 7277 | `		}` |
|      - | 7278 | `		/* Point to the next character */` |
|   1358 | 7279 | `		zIn++;` |
|      2 | 7280 | `	}` |
|      - | 7281 | `	/* The test failed,return FALSE */` |
|  69642 | 7282 | `	ph7_result_bool(pCtx,0);` |
|  69642 | 7283 | `	return PH7_OK;` |
|  35512 | 7284 |  |
|      - | 7285 | `/*` |
|      - | 7286 | ` * bool ctype_lower(string $text)` |
|      - | 7287 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 7288 | ` * Parameters` |
|      - | 7289 | ` *  $text` |
|      - | 7290 | ` *   The tested string.` |
|      - | 7291 | ` * Return` |
|      - | 7292 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 7293 | ` */` |
|     18 | 7294 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7295 |  |
|      - | 7296 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7297 | `	int nLen;` |
|     19 | 7298 | `	if( nArg < 1 ){` |
|      - | 7299 | `		/* Missing arguments,return FALSE */` |
|      3 | 7300 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7301 | `		return PH7_OK;` |
|      - | 7302 | `	}` |
|      - | 7303 | `	/* Extract the target string */` |
|     17 | 7304 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7305 | `	zEnd = &zIn[nLen];` |
|     17 | 7306 | `	if( nLen < 1 ){` |
|      - | 7307 | `		/* Empty string,return FALSE */` |
|      3 | 7308 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7309 | `		return PH7_OK;` |
|      - | 7310 | `	}` |
|      - | 7311 | `	/* Perform the requested operation */` |
|     27 | 7312 | `	for(;;){` |
|     55 | 7313 | `		if( zIn >= zEnd ){` |
|      - | 7314 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7315 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7316 | `			return PH7_OK;` |
|      - | 7317 | `		}` |
|     51 | 7318 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 7319 | `			break;` |
|      - | 7320 | `		}` |
|      - | 7321 | `		/* Point to the next character */` |
|     41 | 7322 | `		zIn++;` |
|      1 | 7323 | `	}` |
|      - | 7324 | `	/* The test failed,return FALSE */` |
|     11 | 7325 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7326 | `	return PH7_OK;` |
|     10 | 7327 |  |
|      - | 7328 | `/*` |
|      - | 7329 | ` * bool ctype_upper(string $text)` |
|      - | 7330 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 7331 | ` * Parameters` |
|      - | 7332 | ` *  $text` |
|      - | 7333 | ` *   The tested string.` |
|      - | 7334 | ` * Return` |
|      - | 7335 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 7336 | ` */` |
|     18 | 7337 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7338 |  |
|      - | 7339 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7340 | `	int nLen;` |
|     19 | 7341 | `	if( nArg < 1 ){` |
|      - | 7342 | `		/* Missing arguments,return FALSE */` |
|      3 | 7343 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7344 | `		return PH7_OK;` |
|      - | 7345 | `	}` |
|      - | 7346 | `	/* Extract the target string */` |
|     17 | 7347 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7348 | `	zEnd = &zIn[nLen];` |
|     17 | 7349 | `	if( nLen < 1 ){` |
|      - | 7350 | `		/* Empty string,return FALSE */` |
|      3 | 7351 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7352 | `		return PH7_OK;` |
|      - | 7353 | `	}` |
|      - | 7354 | `	/* Perform the requested operation */` |
|     28 | 7355 | `	for(;;){` |
|     57 | 7356 | `		if( zIn >= zEnd ){` |
|      - | 7357 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7358 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7359 | `			return PH7_OK;` |
|      - | 7360 | `		}` |
|     53 | 7361 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 7362 | `			break;` |
|      - | 7363 | `		}` |
|      - | 7364 | `		/* Point to the next character */` |
|     43 | 7365 | `		zIn++;` |
|      1 | 7366 | `	}` |
|      - | 7367 | `	/* The test failed,return FALSE */` |
|     11 | 7368 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7369 | `	return PH7_OK;` |
|     10 | 7370 |  |
|      - | 7371 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 7372 | `/*` |
|      - | 7373 | ` * Section:` |
|      - | 7374 | ` *    URL handling Functions.` |
|      - | 7375 | ` * Status:` |
|      - | 7376 | ` *    Stable.` |
|      - | 7377 | ` */` |
|      - | 7378 | `/*` |
|      - | 7379 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 7380 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 7381 | ` */` |
|   1026 | 7382 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 7383 |  |
|      - | 7384 | `	/* Store in the call context result buffer */` |
|   1028 | 7385 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 7386 | `	return SXRET_OK;` |
|      2 | 7387 |  |
|      - | 7388 | `/*` |
|      - | 7389 | ` * string base64_encode(string $data)` |
|      - | 7390 | ` * string convert_uuencode(string $data)` |
|      - | 7391 | ` *  Encodes data with MIME base64` |
|      - | 7392 | ` * Parameter` |
|      - | 7393 | ` *  $data` |
|      - | 7394 | ` *    Data to encode` |
|      - | 7395 | ` * Return` |
|      - | 7396 | ` *  Encoded data or FALSE on failure.` |
|      - | 7397 | ` */` |
|     10 | 7398 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7399 |  |
|      - | 7400 | `	const char *zIn;` |
|      - | 7401 | `	int nLen;` |
|     11 | 7402 | `	if( nArg < 1 ){` |
|      - | 7403 | `		/* Missing arguments,return FALSE */` |
|      5 | 7404 | `		ph7_result_bool(pCtx,0);` |
|      5 | 7405 | `		return PH7_OK;` |
|      - | 7406 | `	}` |
|      - | 7407 | `	/* Extract the input string */` |
|      7 | 7408 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 7409 | `	if( nLen < 1 ){` |
|      - | 7410 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7411 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7412 | `		return PH7_OK;` |
|      - | 7413 | `	}` |
|      - | 7414 | `	/* Perform the BASE64 encoding */` |
|      7 | 7415 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 7416 | `	return PH7_OK;` |
|      6 | 7417 |  |
|      - | 7418 | `/*` |
|      - | 7419 | ` * string base64_decode(string $data)` |
|      - | 7420 | ` * string convert_uudecode(string $data)` |
|      - | 7421 | ` *  Decodes data encoded with MIME base64` |
|      - | 7422 | ` * Parameter` |
|      - | 7423 | ` *  $data` |
|      - | 7424 | ` *    Encoded data.` |
|      - | 7425 | ` * Return` |
|      - | 7426 | ` *  Returns the original data or FALSE on failure.` |
|      - | 7427 | ` */` |
|     36 | 7428 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 7429 |  |
|      - | 7430 | `	const char *zIn;` |
|      - | 7431 | `	int nLen;` |
|     38 | 7432 | `	if( nArg < 1 ){` |
|      - | 7433 | `		/* Missing arguments,return FALSE */` |
|      3 | 7434 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7435 | `		return PH7_OK;` |
|      - | 7436 | `	}` |
|      - | 7437 | `	/* Extract the input string */` |
|     36 | 7438 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 7439 | `	if( nLen < 1 ){` |
|      - | 7440 | `		/* Nothing to process,return FALSE */` |
|      3 | 7441 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7442 | `		return PH7_OK;` |
|      - | 7443 | `	}` |
|      - | 7444 | `	/* Perform the BASE64 decoding */` |
|     34 | 7445 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 7446 | `	return PH7_OK;` |
|     20 | 7447 |  |
|      - | 7448 | `/*` |
|      - | 7449 | ` * string urlencode(string $str)` |
|      - | 7450 | ` *  URL encoding` |
|      - | 7451 | ` * Parameter` |
|      - | 7452 | ` *  $data` |
|      - | 7453 | ` *   Input string.` |
|      - | 7454 | ` * Return` |
|      - | 7455 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 7456 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 7457 | ` *  encoded as plus (+) signs.` |
|      - | 7458 | ` */` |
|      6 | 7459 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7460 |  |
|      - | 7461 | `	const char *zIn;` |
|      - | 7462 | `	int nLen;` |
|      7 | 7463 | `	if( nArg < 1 ){` |
|      - | 7464 | `		/* Missing arguments,return FALSE */` |
|      3 | 7465 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7466 | `		return PH7_OK;` |
|      - | 7467 | `	}` |
|      - | 7468 | `	/* Extract the input string */` |
|      5 | 7469 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 7470 | `	if( nLen < 1 ){` |
|      - | 7471 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7472 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7473 | `		return PH7_OK;` |
|      - | 7474 | `	}` |
|      - | 7475 | `	/* Perform the URL encoding */` |
|      5 | 7476 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 7477 | `	return PH7_OK;` |
|      4 | 7478 |  |
|      - | 7479 | `/*` |
|      - | 7480 | ` * string urldecode(string $str)` |
|      - | 7481 | ` *  Decodes any %## encoding in the given string.` |
|      - | 7482 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 7483 | ` * Parameter` |
|      - | 7484 | ` *  $data` |
|      - | 7485 | ` *    Input string.` |
|      - | 7486 | ` * Return` |
|      - | 7487 | ` *  Decoded URL or FALSE on failure.` |
|      - | 7488 | ` */` |
|      8 | 7489 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7490 |  |
|      - | 7491 | `	const char *zIn;` |
|      - | 7492 | `	int nLen;` |
|      9 | 7493 | `	if( nArg < 1 ){` |
|      - | 7494 | `		/* Missing arguments,return FALSE */` |
|      3 | 7495 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7496 | `		return PH7_OK;` |
|      - | 7497 | `	}` |
|      - | 7498 | `	/* Extract the input string */` |
|      7 | 7499 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 7500 | `	if( nLen < 1 ){` |
|      - | 7501 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7502 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7503 | `		return PH7_OK;` |
|      - | 7504 | `	}` |
|      - | 7505 | `	/* Perform the URL decoding */` |
|      7 | 7506 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 7507 | `	return PH7_OK;` |
|      5 | 7508 |  |
|      - | 7509 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7510 | `/* Table of the built-in functions */` |
|      - | 7511 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 7512 | `	   /* Variable handling functions */` |
|      - | 7513 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 7514 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 7515 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 7516 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 7517 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 7518 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 7519 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 7520 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 7521 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 7522 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 7523 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 7524 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 7525 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 7526 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 7527 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 7528 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 7529 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 7530 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 7531 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 7532 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7533 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 7534 | `	   /* Math functions */` |
|      - | 7535 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 7536 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 7537 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 7538 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 7539 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 7540 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 7541 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 7542 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 7543 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 7544 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 7545 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 7546 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 7547 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 7548 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 7549 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 7550 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 7551 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 7552 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 7553 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 7554 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 7555 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 7556 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 7557 | `	{ "round",    PH7_builtin_round        },` |
|      - | 7558 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 7559 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 7560 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 7561 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 7562 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 7563 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 7564 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 7565 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 7566 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7567 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7568 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 7569 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7570 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7571 | `	   /* String handling functions */` |
|      - | 7572 |  |
|      - | 7573 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 7574 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 7575 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 7576 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 7577 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 7578 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 7579 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 7580 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 7581 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 7582 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 7583 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 7584 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 7585 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 7586 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 7587 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 7588 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 7589 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 7590 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 7591 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 7592 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 7593 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 7594 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 7595 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 7596 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 7597 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 7598 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 7599 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 7600 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 7601 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 7602 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 7603 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 7604 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 7605 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 7606 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 7607 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 7608 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 7609 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 7610 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 7611 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 7612 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 7613 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 7614 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 7615 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 7616 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 7617 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 7618 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 7619 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 7620 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 7621 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 7622 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7623 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7624 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 7625 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 7626 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 7627 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 7628 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7629 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7630 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 7631 |  |
|      - | 7632 |  |
|      - | 7633 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 7634 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 7635 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 7636 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 7637 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 7638 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7639 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7640 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 7641 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 7642 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7643 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7644 |  |
|      - | 7645 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 7646 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 7647 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 7648 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 7649 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 7650 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 7651 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 7652 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 7653 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 7654 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 7655 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 7656 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 7657 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7658 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7659 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 7660 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7661 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7662 |  |
|      - | 7663 | `	         /* Ctype functions */` |
|      - | 7664 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 7665 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 7666 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 7667 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 7668 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 7669 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 7670 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 7671 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 7672 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 7673 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 7674 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 7675 | `	         /* Time functions */` |
|      - | 7676 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 7677 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 7678 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 7679 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 7680 | `	{ "date",        PH7_builtin_date         },` |
|      - | 7681 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 7682 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 7683 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 7684 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 7685 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 7686 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 7687 | `	        /* URL functions */` |
|      - | 7688 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 7689 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 7690 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 7691 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 7692 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 7693 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 7694 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 7695 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 7696 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7697 | `};` |
|      - | 7698 | `/*` |
|      - | 7699 | ` * Register the built-in functions defined above,the array functions` |
|      - | 7700 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 7701 | ` */` |
|   1672 | 7702 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      2 | 7703 |  |
|      - | 7704 | `	sxu32 n;` |
| 255818 | 7705 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 254146 | 7706 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 127074 | 7707 | `	}` |
|      - | 7708 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   1674 | 7709 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 7710 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   1674 | 7711 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   1674 | 7712 |  |
|      - | 7713 |  |
