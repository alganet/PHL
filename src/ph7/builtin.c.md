# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3783/4433 lines (85.34%)

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
|  18678 |  269 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  270 |  |
|  18680 |  271 | `	int res = 1; /* Assume empty by default */` |
|  18680 |  272 | `	if( nArg > 0 ){` |
|  18678 |  273 | `		res = ph7_value_is_empty(apArg[0]);` |
|   9338 |  274 | `	}` |
|  18680 |  275 | `	ph7_result_bool(pCtx,res);` |
|  18680 |  276 | `	return PH7_OK;` |
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
| 131018 | 1288 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1289 |  |
|      - | 1290 | `	const char *zSource,*zOfft;` |
|      - | 1291 | `	int nOfft,nLen,nSrcLen;` |
| 131020 | 1292 | `	if( nArg < 2 ){` |
|      - | 1293 | `		/* return FALSE */` |
|      5 | 1294 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1295 | `		return PH7_OK;` |
|      - | 1296 | `	}` |
|      - | 1297 | `	/* Extract the target string */` |
| 131016 | 1298 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 131016 | 1299 | `	if( nSrcLen < 1 ){` |
|      - | 1300 | `		/* Empty string,return FALSE */` |
|   8014 | 1301 | `		ph7_result_bool(pCtx,0);` |
|   8014 | 1302 | `		return PH7_OK;` |
|      - | 1303 | `	}` |
| 123004 | 1304 | `	nLen = nSrcLen; /* cc warning */` |
|      - | 1305 | `	/* Extract the offset */` |
| 123004 | 1306 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 123004 | 1307 | `	if( nOfft < 0 ){` |
|  21072 | 1308 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  21072 | 1309 | `		if( zOfft < zSource ){` |
|      - | 1310 | `			/* Invalid offset */` |
|      5 | 1311 | `			ph7_result_bool(pCtx,0);` |
|      5 | 1312 | `			return PH7_OK;` |
|      - | 1313 | `		}` |
|  21068 | 1314 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  21068 | 1315 | `		nOfft = (int)(zOfft-zSource);` |
| 112467 | 1316 | `	}else if( nOfft >= nSrcLen ){` |
|      - | 1317 | `		/* Invalid offset */` |
|      7 | 1318 | `		ph7_result_bool(pCtx,0);` |
|      7 | 1319 | `		return PH7_OK;` |
|    ! 0 | 1320 | `	}else{` |
| 101928 | 1321 | `		zOfft = &zSource[nOfft];` |
| 101928 | 1322 | `		nLen = nSrcLen - nOfft;` |
|      - | 1323 | `	}` |
| 122994 | 1324 | `	if( nArg > 2 ){` |
|      - | 1325 | `		/* Extract the length */` |
| 101926 | 1326 | `		nLen = ph7_value_to_int(apArg[2]);` |
| 101926 | 1327 | `		if( nLen == 0 ){` |
|      - | 1328 | `			/* Invalid length,return an empty string */` |
|      5 | 1329 | `			ph7_result_string(pCtx,"",0);` |
|      5 | 1330 | `			return PH7_OK;` |
| 101922 | 1331 | `		}else if( nLen < 0 ){` |
|  21070 | 1332 | `			nLen = nSrcLen + nLen - nOfft;` |
|  21070 | 1333 | `			if( nLen < 1 ){` |
|      - | 1334 | `				/* Invalid  length */` |
|      3 | 1335 | `				nLen = nSrcLen - nOfft;` |
|      1 | 1336 | `			}` |
|  10534 | 1337 | `		}` |
| 101922 | 1338 | `		if( nLen + nOfft > nSrcLen ){` |
|      - | 1339 | `			/* Invalid length */` |
|   2492 | 1340 | `			nLen = nSrcLen - nOfft;` |
|   1245 | 1341 | `		}` |
|  50960 | 1342 | `	}` |
|      - | 1343 | `	/* Return the substring */` |
| 122990 | 1344 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 122990 | 1345 | `	return PH7_OK;` |
|  65511 | 1346 |  |
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
|   2280 | 2315 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2316 |  |
|   2282 | 2317 | `	int iLen = 0;` |
|   2282 | 2318 | `	if( nArg > 0 ){` |
|   2280 | 2319 | `		ph7_value_to_string(apArg[0],&iLen);` |
|   1139 | 2320 | `	}` |
|      - | 2321 | `	/* String length */` |
|   2282 | 2322 | `	ph7_result_int(pCtx,iLen);` |
|   2282 | 2323 | `	return PH7_OK;` |
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
|  88384 | 2468 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      2 | 2469 |  |
|  44192 | 2470 | `	SXUNUSED(pKey);` |
|  88386 | 2471 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 2472 | `	const char *zData;` |
|      - | 2473 | `	int nLen;` |
|  88386 | 2474 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
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
|  88384 | 2491 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 2492 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
|  88384 | 2493 | `	if( pData->bFirst ){` |
|  21286 | 2494 | `		pData->bFirst = 0;` |
|  77742 | 2495 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 2496 | `		/* append the separator first */` |
|  67088 | 2497 | `		ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|  33543 | 2498 | `	}` |
|      - | 2499 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
|  88384 | 2500 | `	if( nLen > 0 ){` |
|  80372 | 2501 | `		ph7_result_string(pData->pCtx,zData,nLen);` |
|  40185 | 2502 | `	}` |
|  88384 | 2503 | `	return PH7_OK;` |
|  44194 | 2504 |  |
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
|  21312 | 2518 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2519 |  |
|      - | 2520 | `	struct implode_data imp_data;` |
|  21314 | 2521 | `	int i = 1;` |
|  21314 | 2522 | `	if( nArg < 1 ){` |
|      - | 2523 | `		/* Missing argument,return NULL */` |
|    ! 0 | 2524 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2525 | `		return PH7_OK;` |
|      - | 2526 | `	}` |
|      - | 2527 | `	/* Prepare the implode context */` |
|  21314 | 2528 | `	imp_data.pCtx = pCtx;` |
|  21314 | 2529 | `	imp_data.bRecursive = 0;` |
|  21314 | 2530 | `	imp_data.bFirst = 1;` |
|  21314 | 2531 | `	imp_data.nRecCount = 0;` |
|  21314 | 2532 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  21312 | 2533 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  10657 | 2534 | `	}else{` |
|      3 | 2535 | `		imp_data.zSep = 0;` |
|      3 | 2536 | `		imp_data.nSeplen = 0;` |
|      3 | 2537 | `		i = 0;` |
|      - | 2538 | `	}` |
|  21314 | 2539 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 2540 | `	/* Start the 'join' process */` |
|  42626 | 2541 | `	while( i < nArg ){` |
|  21314 | 2542 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2543 | `			/* Iterate throw array entries */` |
|  21314 | 2544 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|  10658 | 2545 | `		}else{` |
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
|  21314 | 2561 | `		i++;` |
|      2 | 2562 | `	}` |
|  21314 | 2563 | `	return PH7_OK;` |
|  10658 | 2564 |  |
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
|   3926 | 2653 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2654 |  |
|      - | 2655 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 2656 | `	int nDelim,nStrlen,iLimit;` |
|      - | 2657 | `	ph7_value *pArray;` |
|      - | 2658 | `	ph7_value *pValue;` |
|      - | 2659 | `	sxu32 nOfft;` |
|      - | 2660 | `	sxi32 rc;` |
|   3928 | 2661 | `	if( nArg < 2 ){` |
|      - | 2662 | `		/* Missing arguments,return FALSE */` |
|      9 | 2663 | `		ph7_result_bool(pCtx,0);` |
|      9 | 2664 | `		return PH7_OK;` |
|      - | 2665 | `	}` |
|      - | 2666 | `	/* Extract the delimiter */` |
|   3920 | 2667 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   3920 | 2668 | `	if( nDelim < 1 ){` |
|      - | 2669 | `		/* Empty delimiter,return FALSE */` |
|      3 | 2670 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2671 | `		return PH7_OK;` |
|      - | 2672 | `	}` |
|      - | 2673 | `	/* Extract the string */` |
|   3918 | 2674 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   3918 | 2675 | `	if( nStrlen < 1 ){` |
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
|   3916 | 2690 | `	zEnd = &zString[nStrlen];` |
|      - | 2691 | `	/* Create the array */` |
|   3916 | 2692 | `	pArray =  ph7_context_new_array(pCtx);` |
|   3916 | 2693 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   3916 | 2694 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2695 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 2696 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2697 | `		return PH7_OK;` |
|      - | 2698 | `	}` |
|      - | 2699 | `	/* Set a defualt limit */` |
|   3916 | 2700 | `	iLimit = SXI32_HIGH;` |
|   3916 | 2701 | `	if( nArg > 2 ){` |
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
|  44453 | 2712 | `	for(;;){` |
|  88908 | 2713 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
|  88908 | 2714 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 2715 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   3916 | 2716 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   3916 | 2717 | `			ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|   3916 | 2718 | `			break;` |
|      - | 2719 | `		}` |
|      - | 2720 | `		/* Point to the desired offset */` |
|  84994 | 2721 | `		zCur = &zString[nOfft];` |
|      - | 2722 | `		/* Perform the store operation (may be empty) */` |
|  84994 | 2723 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|  84994 | 2724 | `		ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|      - | 2725 | `		/* Point beyond the delimiter */` |
|  84994 | 2726 | `		zString = &zCur[nDelim];` |
|      - | 2727 | `		/* Reset the cursor */` |
|  84994 | 2728 | `		ph7_value_reset_string_cursor(pValue);` |
|      2 | 2729 | `	}` |
|      - | 2730 | `	/* Return the freshly created array */` |
|   3916 | 2731 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2732 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 2733 | `	 * released as soon we return from this foregin function.` |
|      - | 2734 | `	 */` |
|   3916 | 2735 | `	return PH7_OK;` |
|   1965 | 2736 |  |
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
|   9392 | 2752 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2753 |  |
|      - | 2754 | `	const char *zString;` |
|      - | 2755 | `	int nLen;` |
|   9394 | 2756 | `	if( nArg < 1 ){` |
|      - | 2757 | `		/* Missing arguments,return null */` |
|      3 | 2758 | `		ph7_result_null(pCtx);` |
|      3 | 2759 | `		return PH7_OK;` |
|      - | 2760 | `	}` |
|      - | 2761 | `	/* Extract the target string */` |
|   9392 | 2762 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|   9392 | 2763 | `	if( nLen < 1 ){` |
|      - | 2764 | `		/* Empty string,return */` |
|   1592 | 2765 | `		ph7_result_string(pCtx,"",0);` |
|   1592 | 2766 | `		return PH7_OK;` |
|      - | 2767 | `	}` |
|      - | 2768 | `	/* Start the trim process */` |
|   7802 | 2769 | `	if( nArg < 2 ){` |
|      - | 2770 | `		SyString sStr;` |
|      - | 2771 | `		/* Remove white spaces and NUL bytes */` |
|   7798 | 2772 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  18790 | 2773 | `		SyStringFullTrimSafe(&sStr);` |
|   7798 | 2774 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   3900 | 2775 | `	}else{` |
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
|   7802 | 2829 | `	return PH7_OK;` |
|   4698 | 2830 |  |
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
|  21070 | 2994 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2995 |  |
|      - | 2996 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2997 | `	int nLen;` |
|  21072 | 2998 | `	if( nArg < 1 ){` |
|      - | 2999 | `		/* Missing arguments,return null */` |
|      3 | 3000 | `		ph7_result_null(pCtx);` |
|      3 | 3001 | `		return PH7_OK;` |
|      - | 3002 | `	}` |
|      - | 3003 | `	/* Extract the target string */` |
|  21070 | 3004 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  21070 | 3005 | `	if( nLen < 1 ){` |
|      - | 3006 | `		/* Empty string,return */` |
|      3 | 3007 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3008 | `		return PH7_OK;` |
|      - | 3009 | `	}` |
|      - | 3010 | `	/* Perform the requested operation */` |
|  21068 | 3011 | `	zEnd = &zString[nLen];` |
|  66522 | 3012 | `	for(;;){` |
| 133046 | 3013 | `		if( zString >= zEnd ){` |
|      - | 3014 | `			/* No more input,break immediately */` |
|  21068 | 3015 | `			break;` |
|      - | 3016 | `		}` |
| 111980 | 3017 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 3018 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 3019 | `			zCur = zString;` |
|    ! 0 | 3020 | `			zString++;` |
|    ! 0 | 3021 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 3022 | `				zString++;` |
|    ! 0 | 3023 | `			}` |
|      - | 3024 | `			/* Append UTF-8 stream */` |
|    ! 0 | 3025 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 3026 | `		}else{` |
| 111980 | 3027 | `			int c = zString[0];` |
| 111980 | 3028 | `			if( SyisUpper(c) ){` |
| 111978 | 3029 | `				c = SyToLower(zString[0]);` |
|  55988 | 3030 | `			}` |
|      - | 3031 | `			/* Append character */` |
| 111980 | 3032 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 3033 | `			/* Advance the cursor */` |
| 111980 | 3034 | `			zString++;` |
|      - | 3035 | `		}` |
|      2 | 3036 | `	}` |
|  21068 | 3037 | `	return PH7_OK;` |
|  10537 | 3038 |  |
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
|      - | 3177 | ` * Parameters` |
|      - | 3178 | ` *  $str` |
|      - | 3179 | ` *   The input string.` |
|      - | 3180 | ` * Returns.` |
|      - | 3181 | ` *  The ASCII value as an integer.` |
|      - | 3182 | ` */` |
|     42 | 3183 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3184 |  |
|      - | 3185 | `	const char *zString;` |
|      - | 3186 | `	int nLen,c;` |
|     43 | 3187 | `	if( nArg < 1 ){` |
|      - | 3188 | `		/* Missing arguments,return -1 */` |
|      3 | 3189 | `		ph7_result_int(pCtx,-1);` |
|      3 | 3190 | `		return PH7_OK;` |
|      - | 3191 | `	}` |
|      - | 3192 | `	/* Extract the target string */` |
|     41 | 3193 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 3194 | `	if( nLen < 1 ){` |
|      - | 3195 | `		/* Empty string,return -1 */` |
|      3 | 3196 | `		ph7_result_int(pCtx,-1);` |
|      3 | 3197 | `		return PH7_OK;` |
|      - | 3198 | `	}` |
|      - | 3199 | `	/* Extract the ASCII value of the first character */` |
|     39 | 3200 | `	c = (unsigned char)zString[0];` |
|      - | 3201 | `	/* Return that value */` |
|     39 | 3202 | `	ph7_result_int(pCtx,c);` |
|     39 | 3203 | `	return PH7_OK;` |
|     22 | 3204 |  |
|      - | 3205 | `/*` |
|      - | 3206 | ` * string chr(int $codepoint)` |
|      - | 3207 | ` *  Returns a one-character string containing the character specified` |
|      - | 3208 | ` *  by the given codepoint.  Any integer is accepted; values outside` |
|      - | 3209 | ` *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.` |
|      - | 3210 | ` * Parameters` |
|      - | 3211 | ` *  $codepoint` |
|      - | 3212 | ` *   An integer codepoint.  Values outside 0-255 are deprecated and` |
|      - | 3213 | ` *   will be constrained to a single byte.` |
|      - | 3214 | ` * Returns` |
|      - | 3215 | ` *  A single-character string.` |
|      - | 3216 | ` */` |
|     44 | 3217 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3218 |  |
|      - | 3219 | `	int c;` |
|      - | 3220 | `	unsigned char ch;` |
|      - | 3221 | `	/* PHP requires exactly one argument. */` |
|     46 | 3222 | `	if( nArg != 1 ){` |
|      7 | 3223 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3224 | `			"ArgumentCountError",` |
|      - | 3225 | `			"chr() expects exactly 1 argument, %d given",` |
|      2 | 3226 | `			nArg` |
|      - | 3227 | `			);` |
|      - | 3228 | `	}` |
|      - | 3229 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 3230 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 3231 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 3232 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|     41 | 3233 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      - | 3234 | `		char zBuf[120];` |
|      4 | 3235 | `		SyBufferFormat(zBuf,sizeof(zBuf),` |
|      - | 3236 | `			"Implicit conversion from float %g to int loses precision",` |
|      1 | 3237 | `			ph7_value_to_double(apArg[0])` |
|      - | 3238 | `			);` |
|      3 | 3239 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zBuf);` |
|      1 | 3240 | `	}` |
|      - | 3241 | `	/* Extract the codepoint. */` |
|     41 | 3242 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 3243 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 3244 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 3245 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 3246 | `	 * name to avoid the API double-prefixing it. */` |
|     41 | 3247 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 3248 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 3249 | `			E_DEPRECATED,` |
|      - | 3250 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 3251 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 3252 | `			"The value used will be constrained using % 256"` |
|      - | 3253 | `			);` |
|      2 | 3254 | `	}` |
|      - | 3255 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 3256 | `	 * when taking the address of a wider int. */` |
|     41 | 3257 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 3258 | `	/* Return the specified character */` |
|     41 | 3259 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|     41 | 3260 | `	return PH7_OK;` |
|     24 | 3261 |  |
|      - | 3262 | `/*` |
|      - | 3263 | ` * Binary to hex consumer callback.` |
|      - | 3264 | ` * This callback is the default consumer used by the hash functions` |
|      - | 3265 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 3266 | ` */` |
|    226 | 3267 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 3268 |  |
|      - | 3269 | `	/* Append hex chunk verbatim */` |
|    227 | 3270 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|    227 | 3271 | `	return SXRET_OK;` |
|      1 | 3272 |  |
|      - | 3273 |  |
|      - | 3274 | `/*` |
|      - | 3275 | ` * string bin2hex(string $str)` |
|      - | 3276 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 3277 | ` * Parameters` |
|      - | 3278 | ` *  $str` |
|      - | 3279 | ` *   The input string.` |
|      - | 3280 | ` * Returns.` |
|      - | 3281 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 3282 | ` */` |
|     12 | 3283 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3284 |  |
|      - | 3285 | `	const char *zString;` |
|      - | 3286 | `	int nLen;` |
|     13 | 3287 | `	if( nArg < 1 ){` |
|      - | 3288 | `		/* Missing arguments,return null */` |
|      3 | 3289 | `		ph7_result_null(pCtx);` |
|      3 | 3290 | `		return PH7_OK;` |
|      - | 3291 | `	}` |
|      - | 3292 | `	/* Extract the target string */` |
|     11 | 3293 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 3294 | `	if( nLen < 1 ){` |
|      - | 3295 | `		/* Empty string,return */` |
|      3 | 3296 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3297 | `		return PH7_OK;` |
|      - | 3298 | `	}` |
|      - | 3299 | `	/* Perform the requested operation */` |
|      9 | 3300 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|      9 | 3301 | `	return PH7_OK;` |
|      7 | 3302 |  |
|      - | 3303 |  |
|      - | 3304 | `/* Search callback signature */` |
|      - | 3305 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 3306 | `/*` |
|      - | 3307 | ` * Case-insensitive pattern match.` |
|      - | 3308 | ` * Brute force is the default search method used here.` |
|      - | 3309 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 3310 | ` * well for short/medium texts on modern hardware.` |
|      - | 3311 | ` */` |
|    118 | 3312 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      1 | 3313 |  |
|    119 | 3314 | `	const char *zpIn = (const char *)pPattern;` |
|    119 | 3315 | `	const char *zIn = (const char *)pText;` |
|    119 | 3316 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    119 | 3317 | `	const char *zEnd = &zIn[nLen];` |
|      - | 3318 | `	const char *zPtr,*zPtr2;` |
|      - | 3319 | `	int c,d;` |
|    119 | 3320 | `	if( iPatLen > nLen ){` |
|      - | 3321 | `		/* Don't bother processing */` |
|     33 | 3322 | `		return SXERR_NOTFOUND;` |
|      - | 3323 | `	}` |
|    244 | 3324 | `	for(;;){` |
|    489 | 3325 | `		if( zIn >= zEnd ){` |
|     47 | 3326 | `			break;` |
|      - | 3327 | `		}` |
|    443 | 3328 | `		c = SyToLower(zIn[0]);` |
|    443 | 3329 | `		d = SyToLower(zpIn[0]);` |
|    443 | 3330 | `		if( c == d ){` |
|     41 | 3331 | `			zPtr   = &zIn[1];` |
|     41 | 3332 | `			zPtr2  = &zpIn[1];` |
|     71 | 3333 | `			for(;;){` |
|    143 | 3334 | `				if( zPtr2 >= zpEnd ){` |
|      - | 3335 | `					/* Pattern found */` |
|     41 | 3336 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 3337 | `					return SXRET_OK;` |
|      - | 3338 | `				}` |
|    103 | 3339 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 3340 | `					break;` |
|      - | 3341 | `				}` |
|    103 | 3342 | `				c = SyToLower(zPtr[0]);` |
|    103 | 3343 | `				d = SyToLower(zPtr2[0]);` |
|    103 | 3344 | `				if( c != d ){` |
|    ! 0 | 3345 | `					break;` |
|      - | 3346 | `				}` |
|    103 | 3347 | `				zPtr++; zPtr2++;` |
|      1 | 3348 | `			}` |
|    ! 0 | 3349 | `		}` |
|    403 | 3350 | `		zIn++;` |
|      1 | 3351 | `	}` |
|      - | 3352 | `	/* Pattern not found */` |
|     47 | 3353 | `	return SXERR_NOTFOUND;` |
|     60 | 3354 |  |
|      - | 3355 | `/*` |
|      - | 3356 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3357 | ` *  Find the first occurrence of a string.` |
|      - | 3358 | ` * Parameters` |
|      - | 3359 | ` *  $haystack` |
|      - | 3360 | ` *   The input string.` |
|      - | 3361 | ` * $needle` |
|      - | 3362 | ` *   Search pattern (must be a string).` |
|      - | 3363 | ` * $before_needle` |
|      - | 3364 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3365 | ` *   of the needle (excluding the needle).` |
|      - | 3366 | ` * Return` |
|      - | 3367 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3368 | ` */` |
|     10 | 3369 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3370 |  |
|     11 | 3371 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3372 | `	const char *zBlob,*zPattern;` |
|      - | 3373 | `	int nLen,nPatLen;` |
|      - | 3374 | `	sxu32 nOfft;` |
|      - | 3375 | `	sxi32 rc;` |
|     11 | 3376 | `	if( nArg < 2 ){` |
|      - | 3377 | `		/* Missing arguments,return FALSE */` |
|      5 | 3378 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3379 | `		return PH7_OK;` |
|      - | 3380 | `	}` |
|      - | 3381 | `	/* Extract the needle and the haystack */` |
|      7 | 3382 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 3383 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 3384 | `	nOfft = 0; /* cc warning */` |
|      9 | 3385 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3386 | `		int before = 0;` |
|      - | 3387 | `		/* Perform the lookup */` |
|      5 | 3388 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3389 | `		if( rc != SXRET_OK ){` |
|      - | 3390 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3391 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3392 | `			return PH7_OK;` |
|      - | 3393 | `		}` |
|      - | 3394 | `		/* Return the portion of the string */` |
|      5 | 3395 | `		if( nArg > 2 ){` |
|      3 | 3396 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3397 | `		}` |
|      5 | 3398 | `		if( before ){` |
|      3 | 3399 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3400 | `		}else{` |
|      3 | 3401 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3402 | `		}` |
|      3 | 3403 | `	}else{` |
|      3 | 3404 | `		ph7_result_bool(pCtx,0);` |
|      - | 3405 | `	}` |
|      7 | 3406 | `	return PH7_OK;` |
|      6 | 3407 |  |
|      - | 3408 | `/*` |
|      - | 3409 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3410 | ` *  Case-insensitive strstr().` |
|      - | 3411 | ` * Parameters` |
|      - | 3412 | ` *  $haystack` |
|      - | 3413 | ` *   The input string.` |
|      - | 3414 | ` * $needle` |
|      - | 3415 | ` *   Search pattern (must be a string).` |
|      - | 3416 | ` * $before_needle` |
|      - | 3417 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3418 | ` *   of the needle (excluding the needle).` |
|      - | 3419 | ` * Return` |
|      - | 3420 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3421 | ` */` |
|      6 | 3422 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3423 |  |
|      7 | 3424 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3425 | `	const char *zBlob,*zPattern;` |
|      - | 3426 | `	int nLen,nPatLen;` |
|      - | 3427 | `	sxu32 nOfft;` |
|      - | 3428 | `	sxi32 rc;` |
|      7 | 3429 | `	if( nArg < 2 ){` |
|      - | 3430 | `		/* Missing arguments,return FALSE */` |
|      3 | 3431 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3432 | `		return PH7_OK;` |
|      - | 3433 | `	}` |
|      - | 3434 | `	/* Extract the needle and the haystack */` |
|      5 | 3435 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3436 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 3437 | `	nOfft = 0; /* cc warning */` |
|      7 | 3438 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3439 | `		int before = 0;` |
|      - | 3440 | `		/* Perform the lookup */` |
|      5 | 3441 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3442 | `		if( rc != SXRET_OK ){` |
|      - | 3443 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3444 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3445 | `			return PH7_OK;` |
|      - | 3446 | `		}` |
|      - | 3447 | `		/* Return the portion of the string */` |
|      5 | 3448 | `		if( nArg > 2 ){` |
|      3 | 3449 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3450 | `		}` |
|      5 | 3451 | `		if( before ){` |
|      3 | 3452 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3453 | `		}else{` |
|      3 | 3454 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3455 | `		}` |
|      3 | 3456 | `	}else{` |
|    ! 0 | 3457 | `		ph7_result_bool(pCtx,0);` |
|      - | 3458 | `	}` |
|      5 | 3459 | `	return PH7_OK;` |
|      4 | 3460 |  |
|      - | 3461 | `/*` |
|      - | 3462 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3463 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
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
|     80 | 3476 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3477 |  |
|     82 | 3478 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3479 | `	const char *zBlob,*zPattern;` |
|      - | 3480 | `	int nLen,nPatLen,nStart;` |
|      - | 3481 | `	sxu32 nOfft;` |
|      - | 3482 | `	sxi32 rc;` |
|     82 | 3483 | `	if( nArg < 2 ){` |
|      - | 3484 | `		/* Missing arguments,return FALSE */` |
|      7 | 3485 | `		ph7_result_bool(pCtx,0);` |
|      7 | 3486 | `		return PH7_OK;` |
|      - | 3487 | `	}` |
|      - | 3488 | `	/* Extract the needle and the haystack */` |
|     76 | 3489 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     76 | 3490 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     76 | 3491 | `	nOfft = 0; /* cc warning */` |
|     76 | 3492 | `	nStart = 0;` |
|      - | 3493 | `	/* Peek the starting offset if available */` |
|     76 | 3494 | `	if( nArg > 2 ){` |
|    ! 0 | 3495 | `		nStart = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 3496 | `		if( nStart < 0 ){` |
|    ! 0 | 3497 | `			nStart = -nStart;` |
|    ! 0 | 3498 | `		}` |
|    ! 0 | 3499 | `		if( nStart >= nLen ){` |
|      - | 3500 | `			/* Invalid offset */` |
|    ! 0 | 3501 | `			nStart = 0;` |
|    ! 0 | 3502 | `		}else{` |
|    ! 0 | 3503 | `			zBlob += nStart;` |
|    ! 0 | 3504 | `			nLen -= nStart;` |
|      - | 3505 | `		}` |
|    ! 0 | 3506 | `	}` |
|     76 | 3507 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3508 | `		/* Perform the lookup */` |
|     74 | 3509 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     74 | 3510 | `		if( rc != SXRET_OK ){` |
|      - | 3511 | `			/* Pattern not found,return FALSE */` |
|      5 | 3512 | `			ph7_result_bool(pCtx,0);` |
|      5 | 3513 | `			return PH7_OK;` |
|      - | 3514 | `		}` |
|      - | 3515 | `		/* Return the pattern position */` |
|     70 | 3516 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|     36 | 3517 | `	}else{` |
|      3 | 3518 | `		ph7_result_bool(pCtx,0);` |
|      - | 3519 | `	}` |
|     72 | 3520 | `	return PH7_OK;` |
|     42 | 3521 |  |
|      - | 3522 | `/*` |
|      - | 3523 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3524 | ` *  Case-insensitive strpos.` |
|      - | 3525 | ` * Parameters` |
|      - | 3526 | ` *  $haystack` |
|      - | 3527 | ` *   The input string.` |
|      - | 3528 | ` * $needle` |
|      - | 3529 | ` *   Search pattern (must be a string).` |
|      - | 3530 | ` * $offset` |
|      - | 3531 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3532 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3533 | ` *   of haystack.` |
|      - | 3534 | ` * Return` |
|      - | 3535 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3536 | ` */` |
|     18 | 3537 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3538 |  |
|     19 | 3539 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3540 | `	const char *zBlob,*zPattern;` |
|      - | 3541 | `	int nLen,nPatLen,nStart;` |
|      - | 3542 | `	sxu32 nOfft;` |
|      - | 3543 | `	sxi32 rc;` |
|     19 | 3544 | `	if( nArg < 2 ){` |
|      - | 3545 | `		/* Missing arguments,return FALSE */` |
|      3 | 3546 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3547 | `		return PH7_OK;` |
|      - | 3548 | `	}` |
|      - | 3549 | `	/* Extract the needle and the haystack */` |
|     17 | 3550 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 3551 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     17 | 3552 | `	nOfft = 0; /* cc warning */` |
|     17 | 3553 | `	nStart = 0;` |
|      - | 3554 | `	/* Peek the starting offset if available */` |
|     17 | 3555 | `	if( nArg > 2 ){` |
|      5 | 3556 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 3557 | `		if( nStart < 0 ){` |
|      3 | 3558 | `			nStart = -nStart;` |
|      1 | 3559 | `		}` |
|      5 | 3560 | `		if( nStart >= nLen ){` |
|      - | 3561 | `			/* Invalid offset */` |
|    ! 0 | 3562 | `			nStart = 0;` |
|    ! 0 | 3563 | `		}else{` |
|      5 | 3564 | `			zBlob += nStart;` |
|      5 | 3565 | `			nLen -= nStart;` |
|      - | 3566 | `		}` |
|      2 | 3567 | `	}` |
|     17 | 3568 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3569 | `		/* Perform the lookup */` |
|     17 | 3570 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     17 | 3571 | `		if( rc != SXRET_OK ){` |
|      - | 3572 | `			/* Pattern not found,return FALSE */` |
|      3 | 3573 | `			ph7_result_bool(pCtx,0);` |
|      3 | 3574 | `			return PH7_OK;` |
|      - | 3575 | `		}` |
|      - | 3576 | `		/* Return the pattern position */` |
|     15 | 3577 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 3578 | `	}else{` |
|    ! 0 | 3579 | `		ph7_result_bool(pCtx,0);` |
|      - | 3580 | `	}` |
|     15 | 3581 | `	return PH7_OK;` |
|     10 | 3582 |  |
|      - | 3583 | `/*` |
|      - | 3584 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3585 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 3586 | ` * Parameters` |
|      - | 3587 | ` *  $haystack` |
|      - | 3588 | ` *   The input string.` |
|      - | 3589 | ` * $needle` |
|      - | 3590 | ` *   Search pattern (must be a string).` |
|      - | 3591 | ` * $offset` |
|      - | 3592 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3593 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3594 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3595 | ` * Return` |
|      - | 3596 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3597 | ` */` |
|     32 | 3598 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3599 |  |
|      - | 3600 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     33 | 3601 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3602 | `	int nLen,nPatLen;` |
|      - | 3603 | `	sxu32 nOfft;` |
|      - | 3604 | `	sxi32 rc;` |
|     33 | 3605 | `	if( nArg < 2 ){` |
|      - | 3606 | `		/* Missing arguments,return FALSE */` |
|      3 | 3607 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3608 | `		return PH7_OK;` |
|      - | 3609 | `	}` |
|      - | 3610 | `	/* Extract the needle and the haystack */` |
|     31 | 3611 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 3612 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3613 | `	/* Point to the end of the pattern */` |
|     31 | 3614 | `	zPtr = &zBlob[nLen - 1];` |
|     31 | 3615 | `	zEnd = &zBlob[nLen];` |
|      - | 3616 | `	/* Save the starting posistion */` |
|     31 | 3617 | `	zStart = zBlob;` |
|     31 | 3618 | `	nOfft = 0; /* cc warning */` |
|      - | 3619 | `	/* Peek the starting offset if available */` |
|     31 | 3620 | `	if( nArg > 2 ){` |
|      - | 3621 | `		int nStart;` |
|     21 | 3622 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 3623 | `		if( nStart < 0 ){` |
|     11 | 3624 | `			nStart = -nStart;` |
|     11 | 3625 | `			if( nStart >= nLen ){` |
|      - | 3626 | `				/* Invalid offset */` |
|      3 | 3627 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3628 | `				return PH7_OK;` |
|    ! 0 | 3629 | `			}else{` |
|      9 | 3630 | `				nLen -= nStart;` |
|      9 | 3631 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 3632 | `				zEnd = &zBlob[nLen];` |
|      - | 3633 | `			}` |
|      5 | 3634 | `		}else{` |
|     11 | 3635 | `			if( nStart >= nLen ){` |
|      - | 3636 | `				/* Invalid offset */` |
|      5 | 3637 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3638 | `				return PH7_OK;` |
|    ! 0 | 3639 | `			}else{` |
|      7 | 3640 | `				zBlob += nStart;` |
|      7 | 3641 | `				nLen -= nStart;` |
|      - | 3642 | `			}` |
|      - | 3643 | `		}` |
|      7 | 3644 | `	}` |
|     25 | 3645 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3646 | `		/* Perform the lookup */` |
|     57 | 3647 | `		for(;;){` |
|    115 | 3648 | `			if( zBlob >= zPtr ){` |
|     11 | 3649 | `				break;` |
|      - | 3650 | `			}` |
|    105 | 3651 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    105 | 3652 | `			if( rc == SXRET_OK ){` |
|      - | 3653 | `				/* Pattern found,return it's position */` |
|     13 | 3654 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 3655 | `				return PH7_OK;` |
|      - | 3656 | `			}` |
|     93 | 3657 | `			zPtr--;` |
|      1 | 3658 | `		}` |
|      - | 3659 | `		/* Pattern not found,return FALSE */` |
|     11 | 3660 | `		ph7_result_bool(pCtx,0);` |
|      6 | 3661 | `	}else{` |
|      3 | 3662 | `		ph7_result_bool(pCtx,0);` |
|      - | 3663 | `	}` |
|     13 | 3664 | `	return PH7_OK;` |
|     17 | 3665 |  |
|      - | 3666 | `/*` |
|      - | 3667 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3668 | ` *  Case-insensitive strrpos.` |
|      - | 3669 | ` * Parameters` |
|      - | 3670 | ` *  $haystack` |
|      - | 3671 | ` *   The input string.` |
|      - | 3672 | ` * $needle` |
|      - | 3673 | ` *   Search pattern (must be a string).` |
|      - | 3674 | ` * $offset` |
|      - | 3675 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3676 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3677 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3678 | ` * Return` |
|      - | 3679 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3680 | ` */` |
|     28 | 3681 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3682 |  |
|      - | 3683 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     29 | 3684 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3685 | `	int nLen,nPatLen;` |
|      - | 3686 | `	sxu32 nOfft;` |
|      - | 3687 | `	sxi32 rc;` |
|     29 | 3688 | `	if( nArg < 2 ){` |
|      - | 3689 | `		/* Missing arguments,return FALSE */` |
|      3 | 3690 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3691 | `		return PH7_OK;` |
|      - | 3692 | `	}` |
|      - | 3693 | `	/* Extract the needle and the haystack */` |
|     27 | 3694 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 3695 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3696 | `	/* Point to the end of the pattern */` |
|     27 | 3697 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 3698 | `	zEnd = &zBlob[nLen];` |
|      - | 3699 | `	/* Save the starting posistion */` |
|     27 | 3700 | `	zStart = zBlob;` |
|     27 | 3701 | `	nOfft = 0; /* cc warning */` |
|      - | 3702 | `	/* Peek the starting offset if available */` |
|     27 | 3703 | `	if( nArg > 2 ){` |
|      - | 3704 | `		int nStart;` |
|     15 | 3705 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3706 | `		if( nStart < 0 ){` |
|      7 | 3707 | `			nStart = -nStart;` |
|      7 | 3708 | `			if( nStart >= nLen ){` |
|      - | 3709 | `				/* Invalid offset */` |
|      3 | 3710 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3711 | `				return PH7_OK;` |
|    ! 0 | 3712 | `			}else{` |
|      5 | 3713 | `				nLen -= nStart;` |
|      5 | 3714 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 3715 | `				zEnd = &zBlob[nLen];` |
|      - | 3716 | `			}` |
|      3 | 3717 | `		}else{` |
|      9 | 3718 | `			if( nStart >= nLen ){` |
|      - | 3719 | `				/* Invalid offset */` |
|      5 | 3720 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3721 | `				return PH7_OK;` |
|    ! 0 | 3722 | `			}else{` |
|      5 | 3723 | `				zBlob += nStart;` |
|      5 | 3724 | `				nLen -= nStart;` |
|      - | 3725 | `			}` |
|      - | 3726 | `		}` |
|      4 | 3727 | `	}` |
|     21 | 3728 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3729 | `		/* Perform the lookup */` |
|     44 | 3730 | `		for(;;){` |
|     89 | 3731 | `			if( zBlob >= zPtr ){` |
|      9 | 3732 | `				break;` |
|      - | 3733 | `			}` |
|     81 | 3734 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 3735 | `			if( rc == SXRET_OK ){` |
|      - | 3736 | `				/* Pattern found,return it's position */` |
|     11 | 3737 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 3738 | `				return PH7_OK;` |
|      - | 3739 | `			}` |
|     71 | 3740 | `			zPtr--;` |
|      1 | 3741 | `		}` |
|      - | 3742 | `		/* Pattern not found,return FALSE */` |
|      9 | 3743 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3744 | `	}else{` |
|      3 | 3745 | `		ph7_result_bool(pCtx,0);` |
|      - | 3746 | `	}` |
|     11 | 3747 | `	return PH7_OK;` |
|     15 | 3748 |  |
|      - | 3749 | `/*` |
|      - | 3750 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3751 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3752 | ` * Parameters` |
|      - | 3753 | ` *  $haystack` |
|      - | 3754 | ` *   The input string.` |
|      - | 3755 | ` * $needle` |
|      - | 3756 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3757 | ` *  This behavior is different from that of strstr().` |
|      - | 3758 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3759 | ` *  as the ordinal value of a character.` |
|      - | 3760 | ` * Return` |
|      - | 3761 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3762 | ` */` |
|     24 | 3763 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3764 |  |
|      - | 3765 | `	const char *zBlob;` |
|      - | 3766 | `	int nLen,c;` |
|     25 | 3767 | `	if( nArg < 2 ){` |
|      - | 3768 | `		/* Missing arguments,return FALSE */` |
|      3 | 3769 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3770 | `		return PH7_OK;` |
|      - | 3771 | `	}` |
|      - | 3772 | `	/* Extract the haystack */` |
|     23 | 3773 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3774 | `	c = 0; /* cc warning */` |
|     23 | 3775 | `	if( nLen > 0 ){` |
|      - | 3776 | `		sxu32 nOfft;` |
|      - | 3777 | `		sxi32 rc;` |
|     21 | 3778 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 3779 | `			const char *zPattern;` |
|     11 | 3780 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 3781 | `														 * for NULL pointer.` |
|      - | 3782 | `														 */` |
|     11 | 3783 | `			c = zPattern[0];` |
|      6 | 3784 | `		}else{` |
|      - | 3785 | `			/* Int cast */` |
|     11 | 3786 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 3787 | `		}` |
|      - | 3788 | `		/* Perform the lookup */` |
|     21 | 3789 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3790 | `		if( rc != SXRET_OK ){` |
|      - | 3791 | `			/* No such entry,return FALSE */` |
|      7 | 3792 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3793 | `			return PH7_OK;` |
|      - | 3794 | `		}` |
|      - | 3795 | `		/* Return the string portion */` |
|     15 | 3796 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 3797 | `	}else{` |
|      3 | 3798 | `		ph7_result_bool(pCtx,0);` |
|      - | 3799 | `	}` |
|     17 | 3800 | `	return PH7_OK;` |
|     13 | 3801 |  |
|      - | 3802 | `/*` |
|      - | 3803 | ` * string strrev(string $string)` |
|      - | 3804 | ` *  Reverse a string.` |
|      - | 3805 | ` * Parameters` |
|      - | 3806 | ` *  $string` |
|      - | 3807 | ` *   String to be reversed.` |
|      - | 3808 | ` * Return` |
|      - | 3809 | ` *  The reversed string.` |
|      - | 3810 | ` */` |
|      4 | 3811 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3812 |  |
|      - | 3813 | `	const char *zIn,*zEnd;` |
|      - | 3814 | `	int nLen,c;` |
|      5 | 3815 | `	if( nArg < 1 ){` |
|      - | 3816 | `		/* Missing arguments,return NULL */` |
|      3 | 3817 | `		ph7_result_null(pCtx);` |
|      3 | 3818 | `		return PH7_OK;` |
|      - | 3819 | `	}` |
|      - | 3820 | `	/* Extract the target string */` |
|      3 | 3821 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3822 | `	if( nLen < 1 ){` |
|      - | 3823 | `		/* Empty string Return null */` |
|    ! 0 | 3824 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3825 | `		return PH7_OK;` |
|      - | 3826 | `	}` |
|      - | 3827 | `	/* Perform the requested operation */` |
|      3 | 3828 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3829 | `	for(;;){` |
|      9 | 3830 | `		if( zEnd < zIn ){` |
|      - | 3831 | `			/* No more input to process */` |
|      3 | 3832 | `			break;` |
|      - | 3833 | `		}` |
|      - | 3834 | `		/* Append current character */` |
|      7 | 3835 | `		c = zEnd[0];` |
|      7 | 3836 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3837 | `		zEnd--;` |
|      1 | 3838 | `	}` |
|      3 | 3839 | `	return PH7_OK;` |
|      3 | 3840 |  |
|      - | 3841 | `/*` |
|      - | 3842 | ` * string ucwords(string $string)` |
|      - | 3843 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3844 | ` *  The definition of a word is any string of characters that is immediately after` |
|      - | 3845 | ` *  a whitespace (These are: space, form-feed, newline, carriage return, horizontal tab, and vertical tab).` |
|      - | 3846 | ` * Parameters` |
|      - | 3847 | ` *  $string` |
|      - | 3848 | ` *   The input string.` |
|      - | 3849 | ` * Return` |
|      - | 3850 | ` *  The modified string..` |
|      - | 3851 | ` */` |
|     14 | 3852 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3853 |  |
|      - | 3854 | `	const char *zIn,*zCur,*zEnd;` |
|      - | 3855 | `	int nLen,c;` |
|     15 | 3856 | `	if( nArg < 1 ){` |
|      - | 3857 | `		/* Missing arguments,return NULL */` |
|      3 | 3858 | `		ph7_result_null(pCtx);` |
|      3 | 3859 | `		return PH7_OK;` |
|      - | 3860 | `	}` |
|      - | 3861 | `	/* Extract the target string */` |
|     13 | 3862 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 3863 | `	if( nLen < 1 ){` |
|      - | 3864 | `		/* Empty string – match PHP semantics */` |
|      3 | 3865 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3866 | `		return PH7_OK;` |
|      - | 3867 | `	}` |
|      - | 3868 | `	/* Perform the requested operation */` |
|     11 | 3869 | `	zEnd = &zIn[nLen];` |
|     21 | 3870 | `	for(;;){` |
|      - | 3871 | `		/* Jump leading white spaces */` |
|     43 | 3872 | `		zCur = zIn;` |
|     65 | 3873 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0x80 && SyisSpace(zIn[0]) ){` |
|     23 | 3874 | `			zIn++;` |
|      1 | 3875 | `		}` |
|     43 | 3876 | `		if( zCur < zIn ){` |
|      - | 3877 | `			/* Append white space stream */` |
|     23 | 3878 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 3879 | `		}` |
|     43 | 3880 | `		if( zIn >= zEnd ){` |
|      - | 3881 | `			/* No more input to process */` |
|     11 | 3882 | `			break;` |
|      - | 3883 | `		}` |
|     33 | 3884 | `		c = zIn[0];` |
|     33 | 3885 | `		if( c < 0x80 && SyisLower(c) ){` |
|     29 | 3886 | `			c = SyToUpper(c);` |
|     14 | 3887 | `		}` |
|      - | 3888 | `		/* Append the upper-cased character */` |
|     33 | 3889 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     33 | 3890 | `		zIn++;` |
|     33 | 3891 | `		zCur = zIn;` |
|      - | 3892 | `		/* Append the word varbatim */` |
|    149 | 3893 | `		while( zIn < zEnd ){` |
|    139 | 3894 | `			if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 3895 | `				/* UTF-8 stream */` |
|    ! 0 | 3896 | `				zIn++;` |
|    ! 0 | 3897 | `				SX_JMP_UTF8(zIn,zEnd);` |
|    138 | 3898 | `			}else if( !SyisSpace(zIn[0]) ){` |
|    117 | 3899 | `				zIn++;` |
|     59 | 3900 | `			}else{` |
|     23 | 3901 | `				break;` |
|      - | 3902 | `			}` |
|      1 | 3903 | `		}` |
|     33 | 3904 | `		if( zCur < zIn ){` |
|     33 | 3905 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 | 3906 | `		}` |
|      1 | 3907 | `	}` |
|     11 | 3908 | `	return PH7_OK;` |
|      8 | 3909 |  |
|      - | 3910 | `/*` |
|      - | 3911 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3912 | ` *  Returns input repeated multiplier times.` |
|      - | 3913 | ` * Parameters` |
|      - | 3914 | ` *  $string` |
|      - | 3915 | ` *   String to be repeated.` |
|      - | 3916 | ` * $multiplier` |
|      - | 3917 | ` *  Number of time the input string should be repeated.` |
|      - | 3918 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3919 | ` *  to 0, the function will return an empty string.` |
|      - | 3920 | ` * Return` |
|      - | 3921 | ` *  The repeated string.` |
|      - | 3922 | ` */` |
|  20212 | 3923 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3924 |  |
|      - | 3925 | `	const char *zIn;` |
|      - | 3926 | `	int nLen,nMul;` |
|      - | 3927 | `	int rc;` |
|  20213 | 3928 | `	if( nArg < 2 ){` |
|      - | 3929 | `		/* Missing arguments,return NULL */` |
|      3 | 3930 | `		ph7_result_null(pCtx);` |
|      3 | 3931 | `		return PH7_OK;` |
|      - | 3932 | `	}` |
|      - | 3933 | `	/* Extract the target string */` |
|  20211 | 3934 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|  20211 | 3935 | `	if( nLen < 1 ){` |
|      - | 3936 | `		/* Empty string.Return null */` |
|    ! 0 | 3937 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3938 | `		return PH7_OK;` |
|      - | 3939 | `	}` |
|      - | 3940 | `	/* Extract the multiplier */` |
|  20211 | 3941 | `	nMul = ph7_value_to_int(apArg[1]);` |
|  20211 | 3942 | `	if( nMul < 1 ){` |
|      - | 3943 | `		/* Return the empty string */` |
|      3 | 3944 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3945 | `		return PH7_OK;` |
|      - | 3946 | `	}` |
|      - | 3947 | `	/* Perform the requested operation */` |
| 120220 | 3948 | `	for(;;){` |
| 240441 | 3949 | `		if( !nMul ){` |
|  20209 | 3950 | `			break;` |
|      - | 3951 | `		}` |
|      - | 3952 | `		/* Append the copy */` |
| 220233 | 3953 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 220233 | 3954 | `		if( rc != PH7_OK ){` |
|      - | 3955 | `			/* Out of memory,break immediately */` |
|    ! 0 | 3956 | `			break;` |
|      - | 3957 | `		}` |
| 220233 | 3958 | `		nMul--;` |
|      1 | 3959 | `	}` |
|  20209 | 3960 | `	return PH7_OK;` |
|  10107 | 3961 |  |
|      - | 3962 | `/*` |
|      - | 3963 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3964 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3965 | ` * Parameters` |
|      - | 3966 | ` *  $string` |
|      - | 3967 | ` *   The input string.` |
|      - | 3968 | ` * $is_xhtml` |
|      - | 3969 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3970 | ` * Return` |
|      - | 3971 | ` *  The processed string.` |
|      - | 3972 | ` */` |
|      6 | 3973 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3974 |  |
|      - | 3975 | `	const char *zIn,*zCur,*zEnd;` |
|      7 | 3976 | `	int is_xhtml = 1; /* Default to XHTML-style '<br/>' like PHP */` |
|      - | 3977 | `	int nLen;` |
|      7 | 3978 | `	if( nArg < 1 ){` |
|      - | 3979 | `		/* Missing arguments,return the empty string */` |
|      3 | 3980 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3981 | `		return PH7_OK;` |
|      - | 3982 | `	}` |
|      - | 3983 | `	/* Extract the target string */` |
|      5 | 3984 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3985 | `	if( nLen < 1 ){` |
|      - | 3986 | `		/* Empty string,return null */` |
|    ! 0 | 3987 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3988 | `		return PH7_OK;` |
|      - | 3989 | `	}` |
|      5 | 3990 | `	if( nArg > 1 ){` |
|      3 | 3991 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3992 | `	}` |
|      5 | 3993 | `	zEnd = &zIn[nLen];` |
|      - | 3994 | `	/* Perform the requested operation */` |
|      4 | 3995 | `	for(;;){` |
|      9 | 3996 | `		zCur = zIn;` |
|      - | 3997 | `		/* Delimit the string */` |
|     21 | 3998 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3999 | `			zIn++;` |
|      1 | 4000 | `		}` |
|      9 | 4001 | `		if( zCur < zIn ){` |
|      - | 4002 | `			/* Output chunk verbatim */` |
|      9 | 4003 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 4004 | `		}` |
|      9 | 4005 | `		if( zIn >= zEnd ){` |
|      - | 4006 | `			/* No more input to process */` |
|      5 | 4007 | `			break;` |
|      - | 4008 | `		}` |
|      - | 4009 | `		/* Output the HTML line break */` |
|      - | 4010 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br/>' (legacy without space), otherwise use '<br>' */` |
|      5 | 4011 | `		if( is_xhtml ){` |
|      3 | 4012 | `			ph7_result_string(pCtx,"<br/>",(int)sizeof("<br/>")-1);` |
|      2 | 4013 | `		}else{` |
|      3 | 4014 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 4015 | `		}` |
|      5 | 4016 | `		zCur = zIn;` |
|      - | 4017 | `		/* Append trailing line */` |
|     11 | 4018 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 4019 | `			zIn++;` |
|      1 | 4020 | `		}` |
|      5 | 4021 | `		if( zCur < zIn ){` |
|      - | 4022 | `			/* Output chunk verbatim */` |
|      5 | 4023 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 4024 | `		}` |
|      1 | 4025 | `	}` |
|      5 | 4026 | `	return PH7_OK;` |
|      4 | 4027 |  |
|      - | 4028 | `/*` |
|      - | 4029 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 4030 | ` *  According to the PHP reference manual.` |
|      - | 4031 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 4032 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 4033 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 4034 | ` * This applies to both sprintf() and printf().` |
|      - | 4035 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 4036 | ` * or more of these elements, in order:` |
|      - | 4037 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 4038 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 4039 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 4040 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 4041 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 4042 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 4043 | ` *   it with a single quote ('). See the examples below.` |
|      - | 4044 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 4045 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 4046 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 4047 | ` *   should result in.` |
|      - | 4048 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 4049 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 4050 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 4051 | ` *   limit to the string.` |
|      - | 4052 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 4053 | ` *       % - a literal percent character. No argument is required.` |
|      - | 4054 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 4055 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 4056 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 4057 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 4058 | ` * 	     for the number of digits after the decimal point.` |
|      - | 4059 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 4060 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 4061 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 4062 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 4063 | ` *       g - shorter of %e and %f.` |
|      - | 4064 | ` *       G - shorter of %E and %f.` |
|      - | 4065 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 4066 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 4067 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 4068 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 4069 | ` */` |
|      - | 4070 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4071 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 4072 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 4073 | `/*` |
|      - | 4074 | `** Conversion types fall into various categories as defined by the` |
|      - | 4075 | `** following enumeration.` |
|      - | 4076 | `*/` |
|      - | 4077 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 4078 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 4079 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 4080 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 4081 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 4082 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 4083 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 4084 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 4085 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 4086 |  |
|      - | 4087 | `/*` |
|      - | 4088 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 4089 | `*/` |
|      - | 4090 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 4091 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 4092 | `/*` |
|      - | 4093 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 4094 | `** by an instance of the following structure` |
|      - | 4095 | `*/` |
|      - | 4096 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 4097 | `struct ph7_fmt_info` |
|      - | 4098 |  |
|      - | 4099 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 4100 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 4101 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 4102 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 4103 | `  char *charset; /* The character set for conversion */` |
|      - | 4104 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 4105 | `};` |
|      - | 4106 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 4107 | `/*` |
|      - | 4108 | `** "*val" is a double such that 0.1 <= *val < 10.0` |
|      - | 4109 | `** Return the ascii code for the leading digit of *val, then` |
|      - | 4110 | `** multiply "*val" by 10.0 to renormalize.` |
|      - | 4111 | `**` |
|      - | 4112 | `** Example:` |
|      - | 4113 | `**     input:     *val = 3.14159` |
|      - | 4114 | `**     output:    *val = 1.4159    function return = '3'` |
|      - | 4115 | `**` |
|      - | 4116 | `** The counter *cnt is incremented each time.  After counter exceeds` |
|      - | 4117 | `** 16 (the number of significant digits in a 64-bit float) '0' is` |
|      - | 4118 | `** always returned.` |
|      - | 4119 | `*/` |
|    404 | 4120 | `static int vxGetdigit(sxlongreal *val,int *cnt)` |
|      1 | 4121 |  |
|      - | 4122 | `  sxlongreal d;` |
|      - | 4123 | `  int digit;` |
|      - | 4124 |  |
|    405 | 4125 | `  if( (*cnt)++ >= 16 ){` |
|    ! 0 | 4126 | `	  return '0';` |
|      - | 4127 | `  }` |
|    405 | 4128 | `  digit = (int)*val;` |
|    405 | 4129 | `  d = digit;` |
|    405 | 4130 | `   *val = (*val - d)*10.0;` |
|    405 | 4131 | `  return digit + '0' ;` |
|    203 | 4132 |  |
|      - | 4133 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      - | 4134 | `/*` |
|      - | 4135 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 4136 | ` * used conversion types first.` |
|      - | 4137 | ` */` |
|      - | 4138 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 4139 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 4140 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 4141 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 4142 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 4143 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 4144 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 4145 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 4146 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 4147 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4148 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4149 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 4150 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 4151 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 4152 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 4153 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 4154 | `};` |
|      - | 4155 | `/*` |
|      - | 4156 | ` * Format a given string.` |
|      - | 4157 | ` * The root program.  All variations call this core.` |
|      - | 4158 | ` * INPUTS:` |
|      - | 4159 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 4160 | ` *            1. A pointer to the call context.` |
|      - | 4161 | ` *            2. A pointer to the list of characters to be output` |
|      - | 4162 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 4163 | ` *            3. An integer number of characters to be output.` |
|      - | 4164 | ` *               (Note: This number might be zero.)` |
|      - | 4165 | ` *            4. Upper layer private data.` |
|      - | 4166 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 4167 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 4168 | ` */` |
|    120 | 4169 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 4170 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 4171 | `	ph7_context *pCtx,  /* call context */` |
|      - | 4172 | `	const char *zIn,    /* Format string */` |
|      - | 4173 | `	int nByte,          /* Format string length */` |
|      - | 4174 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 4175 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 4176 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 4177 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 4178 | `	)` |
|      1 | 4179 |  |
|    121 | 4180 | `	char spaces[] = "                                                  ";` |
|      - | 4181 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    121 | 4182 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 4183 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 4184 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 4185 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 4186 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 4187 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 4188 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 4189 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 4190 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 4191 | `	ph7_int64 iVal;` |
|      - | 4192 | `	int precision;           /* Precision of the current field */` |
|      - | 4193 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 4194 | `	int c,rc,n;` |
|      - | 4195 | `	int length;              /* Length of the field */` |
|      - | 4196 | `	int prefix;` |
|      - | 4197 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 4198 | `	int width;               /* Width of the current field */` |
|      - | 4199 | `	int idx;` |
|    121 | 4200 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 4201 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 4202 | `	/* Start the format process */` |
|    123 | 4203 | `	for(;;){` |
|    247 | 4204 | `		zCur = zIn;` |
|    697 | 4205 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|    451 | 4206 | `			zIn++;` |
|      1 | 4207 | `		}` |
|    247 | 4208 | `		if( zCur < zIn ){` |
|      - | 4209 | `			/* Consume chunk verbatim */` |
|     95 | 4210 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|     95 | 4211 | `			if( rc == SXERR_ABORT ){` |
|      - | 4212 | `				/* Callback request an operation abort */` |
|    ! 0 | 4213 | `				break;` |
|      - | 4214 | `			}` |
|     47 | 4215 | `		}` |
|    247 | 4216 | `		if( zIn >= zEnd ){` |
|      - | 4217 | `			/* No more input to process,break immediately */` |
|    119 | 4218 | `			break;` |
|      - | 4219 | `		}` |
|      - | 4220 | `		/* Find out what flags are present */` |
|    129 | 4221 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    128 | 4222 | `			flag_alternateform = flag_zeropad = 0;` |
|    129 | 4223 | `		zIn++; /* Jump the precent sign */` |
|     64 | 4224 | `		do{` |
|    157 | 4225 | `			c = zIn[0];` |
|    157 | 4226 | `			switch( c ){` |
|      9 | 4227 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      3 | 4228 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 4229 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|      9 | 4230 | `			case '#':   flag_alternateform = 1;   c = 0;   break;` |
|      5 | 4231 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 4232 | `			case '\'':` |
|    ! 0 | 4233 | `				zIn++;` |
|    ! 0 | 4234 | `				if( zIn < zEnd ){` |
|      - | 4235 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 4236 | `					c = zIn[0];` |
|    ! 0 | 4237 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4238 | `						spaces[idx] = (char)c;` |
|    ! 0 | 4239 | `					}` |
|    ! 0 | 4240 | `					c = 0;` |
|    ! 0 | 4241 | `				}` |
|    ! 0 | 4242 | `				break;` |
|    128 | 4243 | `			default:                                       break;` |
|      - | 4244 | `			}` |
|    157 | 4245 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 4246 | `		/* Get the field width */` |
|    129 | 4247 | `		width = 0;` |
|    223 | 4248 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     31 | 4249 | `			width = width*10 + (zIn[0] - '0');` |
|     31 | 4250 | `			zIn++;` |
|      1 | 4251 | `		}` |
|    129 | 4252 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 4253 | `			/* Position specifer */` |
|    ! 0 | 4254 | `			if( width > 0 ){` |
|    ! 0 | 4255 | `				n = width;` |
|    ! 0 | 4256 | `				if( vf && n > 0 ){` |
|    ! 0 | 4257 | `					n--;` |
|    ! 0 | 4258 | `				}` |
|    ! 0 | 4259 | `			}` |
|    ! 0 | 4260 | `			zIn++;` |
|    ! 0 | 4261 | `			width = 0;` |
|    ! 0 | 4262 | `			if( zIn < zEnd && zIn[0] == '0' ){` |
|    ! 0 | 4263 | `				flag_zeropad = 1;` |
|    ! 0 | 4264 | `				zIn++;` |
|    ! 0 | 4265 | `			}` |
|    ! 0 | 4266 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 4267 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 4268 | `				zIn++;` |
|    ! 0 | 4269 | `			}` |
|    ! 0 | 4270 | `		}` |
|    129 | 4271 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 4272 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 4273 | `		}` |
|      - | 4274 | `		/* Get the precision */` |
|    129 | 4275 | `		precision = -1;` |
|    129 | 4276 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     57 | 4277 | `			precision = 0;` |
|     57 | 4278 | `			zIn++;` |
|    145 | 4279 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     61 | 4280 | `				precision = precision*10 + (zIn[0] - '0');` |
|     61 | 4281 | `				zIn++;` |
|      1 | 4282 | `			}` |
|     28 | 4283 | `		}` |
|    129 | 4284 | `		if( zIn >= zEnd ){` |
|      - | 4285 | `			/* No more input */` |
|      3 | 4286 | `			break;` |
|      - | 4287 | `		}` |
|      - | 4288 | `		/* Fetch the info entry for the field */` |
|    127 | 4289 | `		pInfo = 0;` |
|    127 | 4290 | `		xtype = PH7_FMT_ERROR;` |
|    127 | 4291 | `		c = zIn[0];` |
|    127 | 4292 | `		zIn++; /* Jump the format specifer */` |
|    699 | 4293 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|    697 | 4294 | `			if( c==aFmt[idx].fmttype ){` |
|    125 | 4295 | `				pInfo = &aFmt[idx];` |
|    125 | 4296 | `				xtype = pInfo->type;` |
|    125 | 4297 | `				break;` |
|      - | 4298 | `			}` |
|    287 | 4299 | `		}` |
|    127 | 4300 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    127 | 4301 | `		length = 0;` |
|      - | 4302 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 4303 | `		 /*` |
|      - | 4304 | `		  ** At this point, variables are initialized as follows:` |
|      - | 4305 | `		  **` |
|      - | 4306 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 4307 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 4308 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 4309 | `		  **                               field width was negative.` |
|      - | 4310 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 4311 | `		  **                               the conversion character.` |
|      - | 4312 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 4313 | `		  **   width                       The specified field width.  This is` |
|      - | 4314 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 4315 | `		  **   precision                   The specified precision.  The default` |
|      - | 4316 | `		  **                               is -1.` |
|      - | 4317 | `		  */` |
|    127 | 4318 | `		switch(xtype){` |
|    ! 0 | 4319 | `		case PH7_FMT_PERCENT:` |
|      - | 4320 | `			/* A literal percent character */` |
|    ! 0 | 4321 | `			zWorker[0] = '%';` |
|    ! 0 | 4322 | `			length = (int)sizeof(char);` |
|    ! 0 | 4323 | `			break;` |
|      3 | 4324 | `		case PH7_FMT_CHARX:` |
|      - | 4325 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 4326 | `			 * with that ASCII value` |
|      - | 4327 | `			 */` |
|      7 | 4328 | `			pArg = NEXT_ARG;` |
|      7 | 4329 | `			if( pArg == 0 ){` |
|      3 | 4330 | `				c = 0;` |
|      2 | 4331 | `			}else{` |
|      5 | 4332 | `				c = ph7_value_to_int(pArg);` |
|      - | 4333 | `			}` |
|      - | 4334 | `			/* NUL byte is an acceptable value */` |
|      7 | 4335 | `			zWorker[0] = (char)c;` |
|      7 | 4336 | `			length = (int)sizeof(char);` |
|      7 | 4337 | `			break;` |
|     12 | 4338 | `		case PH7_FMT_STRING:` |
|      - | 4339 | `			/* the argument is treated as and presented as a string */` |
|     25 | 4340 | `			pArg = NEXT_ARG;` |
|     25 | 4341 | `			if( pArg == 0 ){` |
|    ! 0 | 4342 | `				length = 0;` |
|    ! 0 | 4343 | `			}else{` |
|     25 | 4344 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 4345 | `			}` |
|     25 | 4346 | `			if( length < 1 ){` |
|    ! 0 | 4347 | `				zBuf = " ";` |
|    ! 0 | 4348 | `				length = (int)sizeof(char);` |
|    ! 0 | 4349 | `			}` |
|     25 | 4350 | `			if( precision>=0 && precision<length ){` |
|      3 | 4351 | `				length = precision;` |
|      1 | 4352 | `			}` |
|     25 | 4353 | `			if( flag_zeropad ){` |
|      - | 4354 | `				/* zero-padding works on strings too */` |
|    ! 0 | 4355 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4356 | `					spaces[idx] = '0';` |
|    ! 0 | 4357 | `				}` |
|    ! 0 | 4358 | `			}` |
|     25 | 4359 | `			break;` |
|     20 | 4360 | `		case PH7_FMT_RADIX:` |
|     41 | 4361 | `			pArg = NEXT_ARG;` |
|     41 | 4362 | `			if( pArg == 0 ){` |
|    ! 0 | 4363 | `				iVal = 0;` |
|    ! 0 | 4364 | `			}else{` |
|     41 | 4365 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 4366 | `			}` |
|      - | 4367 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|     41 | 4368 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 4369 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 4370 | `			}` |
|      - | 4371 | `#if 1` |
|      - | 4372 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 4373 | `        ** I think this is stupid.*/` |
|     41 | 4374 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 4375 | `#else` |
|      - | 4376 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 4377 | `        ** but leave the prefix for hex.*/` |
|      - | 4378 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 4379 | `#endif` |
|     41 | 4380 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|     23 | 4381 | `          if( iVal<0 ){` |
|      3 | 4382 | `            iVal = -iVal;` |
|      - | 4383 | `			/* Ticket 1433-003 */` |
|      3 | 4384 | `			if( iVal < 0 ){` |
|      - | 4385 | `				/* Overflow */` |
|    ! 0 | 4386 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4387 | `			}` |
|      3 | 4388 | `            prefix = '-';` |
|     22 | 4389 | `          }else if( flag_plussign )  prefix = '+';` |
|     19 | 4390 | `          else if( flag_blanksign )  prefix = ' ';` |
|     17 | 4391 | `          else                       prefix = 0;` |
|     12 | 4392 | `        }else{` |
|     19 | 4393 | `			if( iVal<0 ){` |
|    ! 0 | 4394 | `				iVal = -iVal;` |
|      - | 4395 | `				/* Ticket 1433-003 */` |
|    ! 0 | 4396 | `				if( iVal < 0 ){` |
|      - | 4397 | `					/* Overflow */` |
|    ! 0 | 4398 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4399 | `				}` |
|    ! 0 | 4400 | `			}` |
|     19 | 4401 | `			prefix = 0;` |
|      - | 4402 | `		}` |
|     41 | 4403 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      3 | 4404 | `          precision = width-(prefix!=0);` |
|      1 | 4405 | `        }` |
|     41 | 4406 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 4407 | `        {` |
|      - | 4408 | `          register char *cset;      /* Use registers for speed */` |
|      - | 4409 | `          register int base;` |
|     41 | 4410 | `          cset = pInfo->charset;` |
|     41 | 4411 | `          base = pInfo->base;` |
|     20 | 4412 | `          do{                                           /* Convert to ascii */` |
|     79 | 4413 | `            *(--zBuf) = cset[iVal%base];` |
|     79 | 4414 | `            iVal = iVal/base;` |
|     79 | 4415 | `          }while( iVal>0 );` |
|      - | 4416 | `        }` |
|     41 | 4417 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     55 | 4418 | `        for(idx=precision-length; idx>0; idx--){` |
|     15 | 4419 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|      8 | 4420 | `        }` |
|     41 | 4421 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|     41 | 4422 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 4423 | `          char *pre, x;` |
|      9 | 4424 | `          pre = pInfo->prefix;` |
|      9 | 4425 | `          if( *zBuf!=pre[0] ){` |
|     23 | 4426 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|      4 | 4427 | `          }` |
|      4 | 4428 | `        }` |
|     41 | 4429 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     41 | 4430 | `		break;` |
|     27 | 4431 | `		case PH7_FMT_FLOAT:` |
|      - | 4432 | `		case PH7_FMT_EXP:` |
|      - | 4433 | `		case PH7_FMT_GENERIC:{` |
|      - | 4434 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 4435 | `		long double realvalue;` |
|      - | 4436 | `		int  exp;                /* exponent of real numbers */` |
|      - | 4437 | `		double rounder;          /* Used for rounding floating point values */` |
|      - | 4438 | `		int flag_dp;            /* True if decimal point should be shown */` |
|      - | 4439 | `		int flag_rtz;           /* True if trailing zeros should be removed */` |
|      - | 4440 | `		int flag_exp;           /* True to force display of the exponent */` |
|      - | 4441 | `		int nsd;                 /* Number of significant digits returned */` |
|     55 | 4442 | `		pArg = NEXT_ARG;` |
|     55 | 4443 | `		if( pArg == 0 ){` |
|    ! 0 | 4444 | `			realvalue = 0;` |
|    ! 0 | 4445 | `		}else{` |
|     55 | 4446 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 4447 | `		}` |
|      - | 4448 | `		/* Special-case NaN and infinities since the normal formatting logic` |
|      - | 4449 | `		 * below assumes a finite positive realvalue. */` |
|     55 | 4450 | `		if( PH7_IS_NAN(realvalue) ){` |
|    ! 0 | 4451 | `			zBuf = "NAN";` |
|    ! 0 | 4452 | `			length = 3;` |
|    ! 0 | 4453 | `			break;` |
|      - | 4454 | `		}` |
|     55 | 4455 | `		if( PH7_IS_INF(realvalue) ){` |
|      - | 4456 | `			/* Infinity prints as INF or -INF depending on sign. */` |
|    ! 0 | 4457 | `			if( realvalue < 0.0 ){` |
|    ! 0 | 4458 | `				zBuf = "-INF";` |
|    ! 0 | 4459 | `				length = 4;` |
|    ! 0 | 4460 | `			}else{` |
|    ! 0 | 4461 | `				zBuf = "INF";` |
|    ! 0 | 4462 | `				length = 3;` |
|      - | 4463 | `			}` |
|    ! 0 | 4464 | `			break;` |
|      - | 4465 | `		}` |
|     55 | 4466 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|     55 | 4467 | `		if( precision>PH7_FMT_BUFSIZ-40) precision = PH7_FMT_BUFSIZ-40;` |
|     55 | 4468 | `        if( realvalue<0.0 ){` |
|    ! 0 | 4469 | `          realvalue = -realvalue;` |
|    ! 0 | 4470 | `          prefix = '-';` |
|    ! 0 | 4471 | `        }else{` |
|     55 | 4472 | `          if( flag_plussign )          prefix = '+';` |
|     55 | 4473 | `          else if( flag_blanksign )    prefix = ' ';` |
|     55 | 4474 | `          else                         prefix = 0;` |
|      - | 4475 | `        }` |
|     55 | 4476 | `        if( pInfo->type==PH7_FMT_GENERIC && precision>0 ) precision--;` |
|     55 | 4477 | `        rounder = 0.0;` |
|      - | 4478 | `#if 0` |
|      - | 4479 | `        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */` |
|      - | 4480 | `        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);` |
|      - | 4481 | `#else` |
|      - | 4482 | `        /* It makes more sense to use 0.5 */` |
|    387 | 4483 | `        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);` |
|      - | 4484 | `#endif` |
|     55 | 4485 | `        if( pInfo->type==PH7_FMT_FLOAT ) realvalue += rounder;` |
|      - | 4486 | `        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */` |
|     55 | 4487 | `        exp = 0;` |
|     55 | 4488 | `        if( realvalue>0.0 ){` |
|     59 | 4489 | `          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }` |
|     87 | 4490 | `          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }` |
|     59 | 4491 | `          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }` |
|     69 | 4492 | `          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }` |
|     55 | 4493 | `          if( exp>350 \|\| exp<-350 ){` |
|    ! 0 | 4494 | `            zBuf = "NaN";` |
|    ! 0 | 4495 | `            length = 3;` |
|    ! 0 | 4496 | `            break;` |
|      - | 4497 | `          }` |
|     27 | 4498 | `        }` |
|     55 | 4499 | `        zBuf = zWorker;` |
|      - | 4500 | `        /*` |
|      - | 4501 | `        ** If the field type is etGENERIC, then convert to either etEXP` |
|      - | 4502 | `        ** or etFLOAT, as appropriate.` |
|      - | 4503 | `        */` |
|     55 | 4504 | `        flag_exp = xtype==PH7_FMT_EXP;` |
|     55 | 4505 | `        if( xtype!=PH7_FMT_FLOAT ){` |
|    ! 0 | 4506 | `          realvalue += rounder;` |
|    ! 0 | 4507 | `          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }` |
|    ! 0 | 4508 | `        }` |
|     55 | 4509 | `        if( xtype==PH7_FMT_GENERIC ){` |
|    ! 0 | 4510 | `          flag_rtz = !flag_alternateform;` |
|    ! 0 | 4511 | `          if( exp<-4 \|\| exp>precision ){` |
|    ! 0 | 4512 | `            xtype = PH7_FMT_EXP;` |
|    ! 0 | 4513 | `          }else{` |
|    ! 0 | 4514 | `            precision = precision - exp;` |
|    ! 0 | 4515 | `            xtype = PH7_FMT_FLOAT;` |
|      - | 4516 | `          }` |
|    ! 0 | 4517 | `        }else{` |
|     55 | 4518 | `          flag_rtz = 0;` |
|      - | 4519 | `        }` |
|      - | 4520 | `        /*` |
|      - | 4521 | `        ** The "exp+precision" test causes output to be of type etEXP if` |
|      - | 4522 | `        ** the precision is too large to fit in buf[].` |
|      - | 4523 | `        */` |
|     55 | 4524 | `        nsd = 0;` |
|     55 | 4525 | `        if( xtype==PH7_FMT_FLOAT && exp+precision<PH7_FMT_BUFSIZ-30 ){` |
|     55 | 4526 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|     55 | 4527 | `          if( prefix ) *(zBuf++) = (char)prefix;         /* Sign */` |
|     55 | 4528 | `          if( exp<0 )  *(zBuf++) = '0';            /* Digits before "." */` |
|    145 | 4529 | `          else for(; exp>=0; exp--) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     55 | 4530 | `          if( flag_dp ) *(zBuf++) = '.';           /* The decimal point */` |
|     87 | 4531 | `          for(exp++; exp<0 && precision>0; precision--, exp++){` |
|     33 | 4532 | `            *(zBuf++) = '0';` |
|     17 | 4533 | `          }` |
|    355 | 4534 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     55 | 4535 | `          *(zBuf--) = 0;                           /* Null terminate */` |
|     55 | 4536 | `          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */` |
|    ! 0 | 4537 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 4538 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 4539 | `          }` |
|     55 | 4540 | `          zBuf++;                            /* point to next free slot */` |
|     28 | 4541 | `        }else{    /* etEXP or etGENERIC */` |
|    ! 0 | 4542 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|    ! 0 | 4543 | `          if( prefix ) *(zBuf++) = (char)prefix;   /* Sign */` |
|    ! 0 | 4544 | `          *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);  /* First digit */` |
|    ! 0 | 4545 | `          if( flag_dp ) *(zBuf++) = '.';     /* Decimal point */` |
|    ! 0 | 4546 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|    ! 0 | 4547 | `          zBuf--;                            /* point to last digit */` |
|    ! 0 | 4548 | `          if( flag_rtz && flag_dp ){          /* Remove tail zeros */` |
|    ! 0 | 4549 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 4550 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 4551 | `          }` |
|    ! 0 | 4552 | `          zBuf++;                            /* point to next free slot */` |
|    ! 0 | 4553 | `          if( exp \|\| flag_exp ){` |
|    ! 0 | 4554 | `            *(zBuf++) = pInfo->charset[0];` |
|    ! 0 | 4555 | `            if( exp<0 ){ *(zBuf++) = '-'; exp = -exp; } /* sign of exp */` |
|    ! 0 | 4556 | `            else       { *(zBuf++) = '+'; }` |
|    ! 0 | 4557 | `            if( exp>=100 ){` |
|    ! 0 | 4558 | `              *(zBuf++) = (char)((exp/100)+'0');                /* 100's digit */` |
|    ! 0 | 4559 | `              exp %= 100;` |
|    ! 0 | 4560 | `            }` |
|    ! 0 | 4561 | `            *(zBuf++) = (char)(exp/10+'0');                     /* 10's digit */` |
|    ! 0 | 4562 | `            *(zBuf++) = (char)(exp%10+'0');                     /* 1's digit */` |
|    ! 0 | 4563 | `          }` |
|      - | 4564 | `        }` |
|      - | 4565 | `        /* The converted number is in buf[] and zero terminated.Output it.` |
|      - | 4566 | `        ** Note that the number is in the usual order, not reversed as with` |
|      - | 4567 | `        ** integer conversions.*/` |
|     55 | 4568 | `        length = (int)(zBuf-zWorker);` |
|     55 | 4569 | `        zBuf = zWorker;` |
|      - | 4570 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 4571 | `        ** set and we are not left justified */` |
|     55 | 4572 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 4573 | `          int i;` |
|      3 | 4574 | `          int nPad = width - length;` |
|     13 | 4575 | `          for(i=width; i>=nPad; i--){` |
|     11 | 4576 | `            zBuf[i] = zBuf[i-nPad];` |
|      6 | 4577 | `          }` |
|      3 | 4578 | `          i = prefix!=0;` |
|      5 | 4579 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      3 | 4580 | `          length = width;` |
|      1 | 4581 | `        }` |
|      - | 4582 | `#else` |
|      - | 4583 | `         zBuf = " ";` |
|      - | 4584 | `		 length = (int)sizeof(char);` |
|      - | 4585 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|     55 | 4586 | `		 break;` |
|      - | 4587 | `							 }` |
|      1 | 4588 | `		default:` |
|      - | 4589 | `			/* Invalid format specifer */` |
|      3 | 4590 | `			zWorker[0] = '?';` |
|      3 | 4591 | `			length = (int)sizeof(char);` |
|      2 | 4592 | `			break;` |
|      - | 4593 | `		}` |
|      - | 4594 | `		 /*` |
|      - | 4595 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 4596 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 4597 | `		 ** the output.` |
|      - | 4598 | `		 */` |
|    127 | 4599 | `    if( !flag_leftjustify ){` |
|      - | 4600 | `      register int nspace;` |
|    119 | 4601 | `      nspace = width-length;` |
|    119 | 4602 | `      if( nspace>0 ){` |
|      5 | 4603 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4604 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4605 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4606 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4607 | `			}` |
|    ! 0 | 4608 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4609 | `        }` |
|      5 | 4610 | `        if( nspace>0 ){` |
|      5 | 4611 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      5 | 4612 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4613 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4614 | `			}` |
|      2 | 4615 | `		}` |
|      2 | 4616 | `      }` |
|     59 | 4617 | `    }` |
|    127 | 4618 | `    if( length>0 ){` |
|    127 | 4619 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    127 | 4620 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4621 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4622 | `		}` |
|     63 | 4623 | `    }` |
|    127 | 4624 | `    if( flag_leftjustify ){` |
|      - | 4625 | `      register int nspace;` |
|      9 | 4626 | `      nspace = width-length;` |
|      9 | 4627 | `      if( nspace>0 ){` |
|      9 | 4628 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4629 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4630 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4631 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4632 | `			}` |
|    ! 0 | 4633 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4634 | `        }` |
|      9 | 4635 | `        if( nspace>0 ){` |
|      9 | 4636 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      9 | 4637 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4638 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4639 | `			}` |
|      4 | 4640 | `		}` |
|      4 | 4641 | `      }` |
|      4 | 4642 | `    }` |
|      1 | 4643 | ` }/* for(;;) */` |
|    121 | 4644 | `	return SXRET_OK;` |
|     61 | 4645 |  |
|      - | 4646 | `/*` |
|      - | 4647 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 4648 | ` */` |
|     84 | 4649 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4650 |  |
|      - | 4651 | `	/* Consume directly */` |
|     85 | 4652 | `	ph7_result_string(pCtx,zInput,nLen);` |
|     42 | 4653 | `	SXUNUSED(pUserData); /* cc warning */` |
|     85 | 4654 | `	return PH7_OK;` |
|      1 | 4655 |  |
|      - | 4656 | `/*` |
|      - | 4657 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 4658 | ` *  Return a formatted string.` |
|      - | 4659 | ` * Parameters` |
|      - | 4660 | ` *  $format` |
|      - | 4661 | ` *    The format string (see block comment above)` |
|      - | 4662 | ` * Return` |
|      - | 4663 | ` *  A string produced according to the formatting string format.` |
|      - | 4664 | ` */` |
|     56 | 4665 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4666 |  |
|      - | 4667 | `	const char *zFormat;` |
|      - | 4668 | `	int nLen;` |
|     57 | 4669 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4670 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 4671 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4672 | `		return PH7_OK;` |
|      - | 4673 | `	}` |
|      - | 4674 | `	/* Extract the string format */` |
|     55 | 4675 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     55 | 4676 | `	if( nLen < 1 ){` |
|      - | 4677 | `		/* Empty string */` |
|    ! 0 | 4678 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4679 | `		return PH7_OK;` |
|      - | 4680 | `	}` |
|      - | 4681 | `	/* Format the string */` |
|     55 | 4682 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,0,FALSE);` |
|     55 | 4683 | `	return PH7_OK;` |
|     29 | 4684 |  |
|      - | 4685 | `/*` |
|      - | 4686 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 4687 | ` */` |
|    110 | 4688 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4689 |  |
|    111 | 4690 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 4691 | `	/* Call the VM output consumer directly */` |
|    111 | 4692 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 4693 | `	/* Increment counter */` |
|    111 | 4694 | `	*pCounter += nLen;` |
|    111 | 4695 | `	return PH7_OK;` |
|      1 | 4696 |  |
|      - | 4697 | `/*` |
|      - | 4698 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 4699 | ` *  Output a formatted string.` |
|      - | 4700 | ` * Parameters` |
|      - | 4701 | ` *  $format` |
|      - | 4702 | ` *   See sprintf() for a description of format.` |
|      - | 4703 | ` * Return` |
|      - | 4704 | ` *  The length of the outputted string.` |
|      - | 4705 | ` */` |
|     42 | 4706 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4707 |  |
|     43 | 4708 | `	ph7_int64 nCounter = 0;` |
|      - | 4709 | `	const char *zFormat;` |
|      - | 4710 | `	int nLen;` |
|     43 | 4711 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4712 | `		/* Missing/Invalid arguments,return 0 */` |
|      3 | 4713 | `		ph7_result_int(pCtx,0);` |
|      3 | 4714 | `		return PH7_OK;` |
|      - | 4715 | `	}` |
|      - | 4716 | `	/* Extract the string format */` |
|     41 | 4717 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 4718 | `	if( nLen < 1 ){` |
|      - | 4719 | `		/* Empty string */` |
|    ! 0 | 4720 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4721 | `		return PH7_OK;` |
|      - | 4722 | `	}` |
|      - | 4723 | `	/* Format the string */` |
|     41 | 4724 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 4725 | `	/* Return the length of the outputted string */` |
|     41 | 4726 | `	ph7_result_int64(pCtx,nCounter);` |
|     41 | 4727 | `	return PH7_OK;` |
|     22 | 4728 |  |
|      - | 4729 | `/*` |
|      - | 4730 | ` * int vprintf(string $format,array $args)` |
|      - | 4731 | ` *  Output a formatted string.` |
|      - | 4732 | ` * Parameters` |
|      - | 4733 | ` *  $format` |
|      - | 4734 | ` *   See sprintf() for a description of format.` |
|      - | 4735 | ` * Return` |
|      - | 4736 | ` *  The length of the outputted string.` |
|      - | 4737 | ` */` |
|      2 | 4738 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4739 |  |
|      3 | 4740 | `	ph7_int64 nCounter = 0;` |
|      - | 4741 | `	const char *zFormat;` |
|      - | 4742 | `	ph7_hashmap *pMap;` |
|      - | 4743 | `	SySet sArg;` |
|      - | 4744 | `	int nLen,n;` |
|      3 | 4745 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4746 | `		/* Missing/Invalid arguments,return 0 */` |
|    ! 0 | 4747 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4748 | `		return PH7_OK;` |
|      - | 4749 | `	}` |
|      - | 4750 | `	/* Extract the string format */` |
|      3 | 4751 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4752 | `	if( nLen < 1 ){` |
|      - | 4753 | `		/* Empty string */` |
|    ! 0 | 4754 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4755 | `		return PH7_OK;` |
|      - | 4756 | `	}` |
|      - | 4757 | `	/* Point to the hashmap */` |
|      3 | 4758 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4759 | `	/* Extract arguments from the hashmap */` |
|      3 | 4760 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4761 | `	/* Format the string */` |
|      3 | 4762 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 4763 | `	/* Return the length of the outputted string */` |
|      3 | 4764 | `	ph7_result_int64(pCtx,nCounter);` |
|      - | 4765 | `	/* Release the container */` |
|      3 | 4766 | `	SySetRelease(&sArg);` |
|      3 | 4767 | `	return PH7_OK;` |
|      2 | 4768 |  |
|      - | 4769 | `/*` |
|      - | 4770 | ` * int vsprintf(string $format,array $args)` |
|      - | 4771 | ` *  Output a formatted string.` |
|      - | 4772 | ` * Parameters` |
|      - | 4773 | ` *  $format` |
|      - | 4774 | ` *   See sprintf() for a description of format.` |
|      - | 4775 | ` * Return` |
|      - | 4776 | ` *  A string produced according to the formatting string format.` |
|      - | 4777 | ` */` |
|     10 | 4778 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4779 |  |
|      - | 4780 | `	const char *zFormat;` |
|      - | 4781 | `	ph7_hashmap *pMap;` |
|      - | 4782 | `	SySet sArg;` |
|      - | 4783 | `	int nLen,n;` |
|     11 | 4784 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4785 | `		/* Missing/Invalid arguments,return the empty string */` |
|      5 | 4786 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 4787 | `		return PH7_OK;` |
|      - | 4788 | `	}` |
|      - | 4789 | `	/* Extract the string format */` |
|      7 | 4790 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4791 | `	if( nLen < 1 ){` |
|      - | 4792 | `		/* Empty string */` |
|    ! 0 | 4793 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4794 | `		return PH7_OK;` |
|      - | 4795 | `	}` |
|      - | 4796 | `	/* Point to hashmap */` |
|      7 | 4797 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4798 | `	/* Extract arguments from the hashmap */` |
|      7 | 4799 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4800 | `	/* Format the string */` |
|      7 | 4801 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),0,TRUE);` |
|      - | 4802 | `	/* Release the container */` |
|      7 | 4803 | `	SySetRelease(&sArg);` |
|      7 | 4804 | `	return PH7_OK;` |
|      6 | 4805 |  |
|      - | 4806 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 4807 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4808 | `/*` |
|      - | 4809 | ` * Symisc eXtension.` |
|      - | 4810 | ` * string size_format(int64 $size)` |
|      - | 4811 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 4812 | ` *  Example:` |
|      - | 4813 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 4814 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 4815 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 4816 | ` * Parameter` |
|      - | 4817 | ` *  $size` |
|      - | 4818 | ` *    Entity size in bytes.` |
|      - | 4819 | ` * Return` |
|      - | 4820 | ` *   Formatted string representation of the given size.` |
|      - | 4821 | ` */` |
|     24 | 4822 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4823 |  |
|      - | 4824 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 4825 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 4826 | `	sxi32 nRest,i_32;` |
|      - | 4827 | `	ph7_int64 iSize;` |
|     25 | 4828 | `	int c = -1; /* index in zUnit[] */` |
|      - | 4829 |  |
|     25 | 4830 | `	if( nArg < 1 ){` |
|      - | 4831 | `		/* Missing argument,return the empty string */` |
|      3 | 4832 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4833 | `		return PH7_OK;` |
|      - | 4834 | `	}` |
|      - | 4835 | `	/* Extract the given size */` |
|     23 | 4836 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 4837 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 4838 | `		/* Don't bother formatting,return immediately */` |
|      5 | 4839 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 4840 | `		return PH7_OK;` |
|      - | 4841 | `	}` |
|     19 | 4842 | `	for(;;){` |
|     39 | 4843 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 4844 | `		iSize >>= 10;` |
|     39 | 4845 | `		c++;` |
|     39 | 4846 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 4847 | `			break;` |
|      - | 4848 | `		}` |
|      1 | 4849 | `	}` |
|     19 | 4850 | `	nRest /= 100;` |
|     19 | 4851 | `	if( nRest > 9 ){` |
|    ! 0 | 4852 | `		nRest = 9;` |
|    ! 0 | 4853 | `	}` |
|     19 | 4854 | `	if( iSize > 999 ){` |
|    ! 0 | 4855 | `		c++;` |
|    ! 0 | 4856 | `		nRest = 9;` |
|    ! 0 | 4857 | `		iSize = 0;` |
|    ! 0 | 4858 | `	}` |
|     19 | 4859 | `	i_32 = (sxi32)iSize;` |
|      - | 4860 | `	/* Format */` |
|     19 | 4861 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 4862 | `	return PH7_OK;` |
|     13 | 4863 |  |
|      - | 4864 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4865 | `/*` |
|      - | 4866 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 4867 | ` *   Calculate the md5 hash of a string.` |
|      - | 4868 | ` * Parameter` |
|      - | 4869 | ` *  $str` |
|      - | 4870 | ` *   Input string` |
|      - | 4871 | ` * $raw_output` |
|      - | 4872 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4873 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4874 | ` * Return` |
|      - | 4875 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 4876 | ` */` |
|     10 | 4877 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4878 |  |
|      - | 4879 | `	unsigned char zDigest[16];` |
|     11 | 4880 | `	int raw_output = FALSE;` |
|      - | 4881 | `	const void *pIn;` |
|      - | 4882 | `	int nLen;` |
|     11 | 4883 | `	if( nArg < 1 ){` |
|      - | 4884 | `		/* Missing arguments,return the empty string */` |
|      3 | 4885 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4886 | `		return PH7_OK;` |
|      - | 4887 | `	}` |
|      - | 4888 | `	/* Extract the input string */` |
|      9 | 4889 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 4890 | `	if( nLen < 1 ){` |
|      - | 4891 | `		/* Empty string */` |
|    ! 0 | 4892 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4893 | `		return PH7_OK;` |
|      - | 4894 | `	}` |
|      9 | 4895 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      3 | 4896 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 | 4897 | `	}` |
|      - | 4898 | `	/* Compute the MD5 digest */` |
|      9 | 4899 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|      9 | 4900 | `	if( raw_output ){` |
|      - | 4901 | `		/* Output raw digest */` |
|      3 | 4902 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      2 | 4903 | `	}else{` |
|      - | 4904 | `		/* Perform a binary to hex conversion */` |
|      7 | 4905 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4906 | `	}` |
|      9 | 4907 | `	return PH7_OK;` |
|      6 | 4908 |  |
|      - | 4909 | `/*` |
|      - | 4910 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 4911 | ` *   Calculate the sha1 hash of a string.` |
|      - | 4912 | ` * Parameter` |
|      - | 4913 | ` *  $str` |
|      - | 4914 | ` *   Input string` |
|      - | 4915 | ` * $raw_output` |
|      - | 4916 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4917 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4918 | ` * Return` |
|      - | 4919 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 4920 | ` */` |
|      8 | 4921 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4922 |  |
|      - | 4923 | `	unsigned char zDigest[20];` |
|      9 | 4924 | `	int raw_output = FALSE;` |
|      - | 4925 | `	const void *pIn;` |
|      - | 4926 | `	int nLen;` |
|      9 | 4927 | `	if( nArg < 1 ){` |
|      - | 4928 | `		/* Missing arguments,return the empty string */` |
|      3 | 4929 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4930 | `		return PH7_OK;` |
|      - | 4931 | `	}` |
|      - | 4932 | `	/* Extract the input string */` |
|      7 | 4933 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4934 | `	if( nLen < 1 ){` |
|      - | 4935 | `		/* Empty string */` |
|    ! 0 | 4936 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4937 | `		return PH7_OK;` |
|      - | 4938 | `	}` |
|      7 | 4939 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      3 | 4940 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 | 4941 | `	}` |
|      - | 4942 | `	/* Compute the SHA1 digest */` |
|      7 | 4943 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|      7 | 4944 | `	if( raw_output ){` |
|      - | 4945 | `		/* Output raw digest */` |
|      3 | 4946 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      2 | 4947 | `	}else{` |
|      - | 4948 | `		/* Perform a binary to hex conversion */` |
|      5 | 4949 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4950 | `	}` |
|      7 | 4951 | `	return PH7_OK;` |
|      5 | 4952 |  |
|      - | 4953 | `/*` |
|      - | 4954 | ` * int64 crc32(string $str)` |
|      - | 4955 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 4956 | ` * Parameter` |
|      - | 4957 | ` *  $str` |
|      - | 4958 | ` *   Input string` |
|      - | 4959 | ` * Return` |
|      - | 4960 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 4961 | ` */` |
|      4 | 4962 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4963 |  |
|      - | 4964 | `	const void *pIn;` |
|      - | 4965 | `	sxu32 nCRC;` |
|      - | 4966 | `	int nLen;` |
|      5 | 4967 | `	if( nArg < 1 ){` |
|      - | 4968 | `		/* Missing arguments,return 0 */` |
|      3 | 4969 | `		ph7_result_int(pCtx,0);` |
|      3 | 4970 | `		return PH7_OK;` |
|      - | 4971 | `	}` |
|      - | 4972 | `	/* Extract the input string */` |
|      3 | 4973 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4974 | `	if( nLen < 1 ){` |
|      - | 4975 | `		/* Empty string */` |
|    ! 0 | 4976 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4977 | `		return PH7_OK;` |
|      - | 4978 | `	}` |
|      - | 4979 | `	/* Calculate the sum */` |
|      3 | 4980 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 4981 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 4982 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 4983 | `	return PH7_OK;` |
|      3 | 4984 |  |
|      - | 4985 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 4986 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4987 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 4988 | `/*` |
|      - | 4989 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 4990 |  |
|      - | 4991 | ` */` |
|      4 | 4992 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 4993 | `	const char *zInput, /* Raw input */` |
|      - | 4994 | `	int nByte,  /* Input length */` |
|      - | 4995 | `	int delim,  /* Delimiter */` |
|      - | 4996 | `	int encl,   /* Enclosure */` |
|      - | 4997 | `	int escape,  /* Escape character */` |
|      - | 4998 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 4999 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 5000 | `	)` |
|      1 | 5001 |  |
|      5 | 5002 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 5003 | `	const char *zIn = zInput;` |
|      - | 5004 | `	const char *zPtr;` |
|      - | 5005 | `	int isEnc;` |
|      - | 5006 | `	/* Start processing */` |
|      8 | 5007 | `	for(;;){` |
|     17 | 5008 | `		if( zIn >= zEnd ){` |
|      - | 5009 | `			/* No more input to process */` |
|      5 | 5010 | `			break;` |
|      - | 5011 | `		}` |
|     13 | 5012 | `		isEnc = 0;` |
|     13 | 5013 | `		zPtr = zIn;` |
|      - | 5014 | `		/* Find the first delimiter */` |
|     27 | 5015 | `		while( zIn < zEnd ){` |
|     23 | 5016 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 5017 | `				/* Delimiter found,break imediately */` |
|      5 | 5018 | `				break;` |
|     15 | 5019 | `			}else if( zIn[0] == encl ){` |
|      - | 5020 | `				/* Inside enclosure? */` |
|    ! 0 | 5021 | `				isEnc = !isEnc;` |
|     15 | 5022 | `			}else if( zIn[0] == escape ){` |
|      - | 5023 | `				/* Escape sequence */` |
|    ! 0 | 5024 | `				zIn++;` |
|    ! 0 | 5025 | `			}` |
|      - | 5026 | `			/* Advance the cursor */` |
|     15 | 5027 | `			zIn++;` |
|      1 | 5028 | `		}` |
|     13 | 5029 | `		if( zIn > zPtr ){` |
|     13 | 5030 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 5031 | `			sxi32 rc;` |
|      - | 5032 | `			/* Invoke the supllied callback */` |
|     13 | 5033 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 5034 | `				zPtr++;` |
|    ! 0 | 5035 | `				nByteChunk-=2;` |
|    ! 0 | 5036 | `			}` |
|     13 | 5037 | `			if( nByteChunk > 0 ){` |
|     13 | 5038 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 5039 | `				if( rc == SXERR_ABORT ){` |
|      - | 5040 | `					/* User callback request an operation abort */` |
|    ! 0 | 5041 | `					break;` |
|      - | 5042 | `				}` |
|      6 | 5043 | `			}` |
|      6 | 5044 | `		}` |
|      - | 5045 | `		/* Ignore trailing delimiter */` |
|     21 | 5046 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 5047 | `			zIn++;` |
|      1 | 5048 | `		}` |
|      1 | 5049 | `	}` |
|      5 | 5050 | `	return SXRET_OK;` |
|      1 | 5051 |  |
|      - | 5052 | `/*` |
|      - | 5053 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 5054 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 5055 | ` * argument to this callback.` |
|      - | 5056 | ` */` |
|     12 | 5057 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 5058 |  |
|     13 | 5059 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 5060 | `	ph7_value sEntry;` |
|      - | 5061 | `	SyString sToken;` |
|      - | 5062 | `	/* Insert the token in the given array */` |
|     13 | 5063 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 5064 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 5065 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 5066 | `	if( sToken.nByte < 1){` |
|    ! 0 | 5067 | `		return SXRET_OK;` |
|      - | 5068 | `	}` |
|     13 | 5069 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 5070 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 5071 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 5072 | `	return SXRET_OK;` |
|      7 | 5073 |  |
|      - | 5074 | `/*` |
|      - | 5075 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 5076 | ` *  Parse a CSV string into an array.` |
|      - | 5077 | ` * Parameters` |
|      - | 5078 | ` *  $input` |
|      - | 5079 | ` *   The string to parse.` |
|      - | 5080 | ` *  $delimiter` |
|      - | 5081 | ` *   Set the field delimiter (one character only).` |
|      - | 5082 | ` *  $enclosure` |
|      - | 5083 | ` *   Set the field enclosure character (one character only).` |
|      - | 5084 | ` *  $escape` |
|      - | 5085 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 5086 | ` * Return` |
|      - | 5087 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 5088 | ` */` |
|      4 | 5089 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5090 |  |
|      - | 5091 | `	const char *zInput,*zPtr;` |
|      - | 5092 | `	ph7_value *pArray;` |
|      5 | 5093 | `	int delim  = ',';   /* Delimiter */` |
|      5 | 5094 | `	int encl   = '"' ;  /* Enclosure */` |
|      5 | 5095 | `	int escape = '\\';  /* Escape character */` |
|      - | 5096 | `	int nLen;` |
|      5 | 5097 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5098 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 5099 | `		ph7_result_null(pCtx);` |
|      3 | 5100 | `		return PH7_OK;` |
|      - | 5101 | `	}` |
|      - | 5102 | `	/* Extract the raw input */` |
|      3 | 5103 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5104 | `	if( nArg > 1 ){` |
|      - | 5105 | `		int i;` |
|      3 | 5106 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 5107 | `			/* Extract the delimiter */` |
|      3 | 5108 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 5109 | `			if( i > 0 ){` |
|      3 | 5110 | `				delim = zPtr[0];` |
|      1 | 5111 | `			}` |
|      1 | 5112 | `		}` |
|      3 | 5113 | `		if( nArg > 2 ){` |
|      3 | 5114 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 5115 | `				/* Extract the enclosure */` |
|      3 | 5116 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 5117 | `				if( i > 0 ){` |
|      3 | 5118 | `					encl = zPtr[0];` |
|      1 | 5119 | `				}` |
|      1 | 5120 | `			}` |
|      3 | 5121 | `			if( nArg > 3 ){` |
|      3 | 5122 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 5123 | `					/* Extract the escape character */` |
|      3 | 5124 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 5125 | `					if( i > 0 ){` |
|      3 | 5126 | `						escape = zPtr[0];` |
|      1 | 5127 | `					}` |
|      1 | 5128 | `				}` |
|      1 | 5129 | `			}` |
|      1 | 5130 | `		}` |
|      1 | 5131 | `	}` |
|      - | 5132 | `	/* Create our array */` |
|      3 | 5133 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5134 | `	if( pArray == 0 ){` |
|    ! 0 | 5135 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 5136 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5137 | `		return PH7_OK;` |
|      - | 5138 | `	}` |
|      - | 5139 | `	/* Parse the raw input */` |
|      3 | 5140 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 5141 | `	/* Return the freshly created array */` |
|      3 | 5142 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5143 | `	return PH7_OK;` |
|      3 | 5144 |  |
|      - | 5145 | `/*` |
|      - | 5146 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 5147 | ` * container.` |
|      - | 5148 | ` * Refer to [strip_tags()].` |
|      - | 5149 | ` */` |
|     10 | 5150 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5151 |  |
|     11 | 5152 | `	const char *zEnd = &zTag[nByte];` |
|      - | 5153 | `	const char *zPtr;` |
|      - | 5154 | `	SyString sEntry;` |
|      - | 5155 | `	/* Strip tags */` |
|     10 | 5156 | `	for(;;){` |
|     45 | 5157 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 5158 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 5159 | `				zTag++;` |
|      1 | 5160 | `		}` |
|     21 | 5161 | `		if( zTag >= zEnd ){` |
|     11 | 5162 | `			break;` |
|      - | 5163 | `		}` |
|     11 | 5164 | `		zPtr = zTag;` |
|      - | 5165 | `		/* Delimit the tag */` |
|     25 | 5166 | `		while(zTag < zEnd ){` |
|     25 | 5167 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5168 | `				/* UTF-8 stream */` |
|      3 | 5169 | `				zTag++;` |
|      5 | 5170 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 5171 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 5172 | `				break;` |
|    ! 0 | 5173 | `			}else{` |
|     13 | 5174 | `				zTag++;` |
|      - | 5175 | `			}` |
|      1 | 5176 | `		}` |
|     11 | 5177 | `		if( zTag > zPtr ){` |
|      - | 5178 | `			/* Perform the insertion */` |
|     11 | 5179 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 5180 | `			SyStringFullTrim(&sEntry);` |
|     11 | 5181 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 5182 | `		}` |
|      - | 5183 | `		/* Jump the trailing '>' */` |
|     11 | 5184 | `		zTag++;` |
|      1 | 5185 | `	}` |
|     11 | 5186 | `	return SXRET_OK;` |
|      1 | 5187 |  |
|      - | 5188 | `/*` |
|      - | 5189 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 5190 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 5191 | ` * Refer to [strip_tags()].` |
|      - | 5192 | ` */` |
|     36 | 5193 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5194 |  |
|     37 | 5195 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 5196 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 5197 | `		SyString sTag;` |
|     85 | 5198 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 5199 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 5200 | `			zTag++;` |
|      1 | 5201 | `		}` |
|      - | 5202 | `		/* Delimit the tag */` |
|     25 | 5203 | `		zCur = zTag;` |
|     77 | 5204 | `		while(zTag < zEnd ){` |
|     77 | 5205 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5206 | `				/* UTF-8 stream */` |
|      5 | 5207 | `				zTag++;` |
|      9 | 5208 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 5209 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 5210 | `				break;` |
|    ! 0 | 5211 | `			}else{` |
|     49 | 5212 | `				zTag++;` |
|      - | 5213 | `			}` |
|      1 | 5214 | `		}` |
|     25 | 5215 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 5216 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 5217 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 5218 | `		if( sTag.nByte > 0 ){` |
|      - | 5219 | `			SyString *aEntry,*pEntry;` |
|      - | 5220 | `			sxi32 rc;` |
|      - | 5221 | `			sxu32 n;` |
|      - | 5222 | `			/* Perform the lookup */` |
|     25 | 5223 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 5224 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 5225 | `				pEntry = &aEntry[n];` |
|      - | 5226 | `				/* Do the comparison */` |
|     25 | 5227 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 5228 | `				if( !rc ){` |
|     21 | 5229 | `					return SXRET_OK;` |
|      - | 5230 | `				}` |
|      3 | 5231 | `			}` |
|      2 | 5232 | `		}` |
|      2 | 5233 | `	}` |
|      - | 5234 | `	/* No such tag */` |
|     17 | 5235 | `	return SXERR_NOTFOUND;` |
|     19 | 5236 |  |
|      - | 5237 | `/*` |
|      - | 5238 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 5239 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 5240 | ` * Refer to [strip_tags()].` |
|      - | 5241 | ` */` |
|     16 | 5242 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 5243 |  |
|     17 | 5244 | `	const char *zEnd = &zIn[nByte];` |
|      - | 5245 | `	const char *zPtr,*zTag;` |
|      - | 5246 | `	SySet sSet;` |
|      - | 5247 | `	/* initialize the set of allowed tags */` |
|     17 | 5248 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 5249 | `	if( nTaglen > 0 ){` |
|      - | 5250 | `		/* Set of allowed tags */` |
|     11 | 5251 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 5252 | `	}` |
|      - | 5253 | `	/* Set the empty string */` |
|     17 | 5254 | `	ph7_result_string(pCtx,"",0);` |
|      - | 5255 | `	/* Start processing */` |
|     26 | 5256 | `	for(;;){` |
|     53 | 5257 | `		if(zIn >= zEnd){` |
|      - | 5258 | `			/* No more input to process */` |
|     15 | 5259 | `			break;` |
|      - | 5260 | `		}` |
|     39 | 5261 | `		zPtr = zIn;` |
|      - | 5262 | `		/* Find a tag */` |
|    133 | 5263 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 5264 | `			zIn++;` |
|      1 | 5265 | `		}` |
|     39 | 5266 | `		if( zIn > zPtr ){` |
|      - | 5267 | `			/* Consume raw input */` |
|     21 | 5268 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 5269 | `		}` |
|      - | 5270 | `		/* Ignore trailing null bytes */` |
|     39 | 5271 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 5272 | `			zIn++;` |
|    ! 0 | 5273 | `		}` |
|     39 | 5274 | `		if(zIn >= zEnd){` |
|      - | 5275 | `			/* No more input to process */` |
|      3 | 5276 | `			break;` |
|      - | 5277 | `		}` |
|     37 | 5278 | `		if( zIn[0] == '<' ){` |
|      - | 5279 | `			sxi32 rc;` |
|     37 | 5280 | `			zTag = zIn++;` |
|      - | 5281 | `			/* Delimit the tag */` |
|    127 | 5282 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 5283 | `				zIn++;` |
|      1 | 5284 | `			}` |
|     37 | 5285 | `			if( zIn < zEnd ){` |
|     37 | 5286 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 5287 | `			}` |
|      - | 5288 | `			/* Query the set */` |
|     37 | 5289 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 5290 | `			if( rc == SXRET_OK ){` |
|      - | 5291 | `				/* Keep the tag */` |
|     21 | 5292 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 5293 | `			}` |
|     18 | 5294 | `		}` |
|      1 | 5295 | `	}` |
|      - | 5296 | `	/* Cleanup */` |
|     17 | 5297 | `	SySetRelease(&sSet);` |
|     17 | 5298 | `	return SXRET_OK;` |
|      1 | 5299 |  |
|      - | 5300 | `/*` |
|      - | 5301 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 5302 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 5303 | ` * Parameters` |
|      - | 5304 | ` *  $str` |
|      - | 5305 | ` *  The input string.` |
|      - | 5306 | ` * $allowable_tags` |
|      - | 5307 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 5308 | ` * Return` |
|      - | 5309 | ` *  Returns the stripped string.` |
|      - | 5310 | ` */` |
|     16 | 5311 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5312 |  |
|     17 | 5313 | `	const char *zTaglist = 0;` |
|      - | 5314 | `	const char *zString;` |
|     17 | 5315 | `	int nTaglen = 0;` |
|      - | 5316 | `	int nLen;` |
|     17 | 5317 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5318 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 5319 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5320 | `		return PH7_OK;` |
|      - | 5321 | `	}` |
|      - | 5322 | `	/* Point to the raw string */` |
|     15 | 5323 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 5324 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 5325 | `		/* Allowed tag */` |
|     11 | 5326 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 5327 | `	}` |
|      - | 5328 | `	/* Process input */` |
|     15 | 5329 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 5330 | `	return PH7_OK;` |
|      9 | 5331 |  |
|      - | 5332 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5333 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5334 | `/*` |
|      - | 5335 | ` * string str_shuffle(string $str)` |
|      - | 5336 |  |
|      - | 5337 | ` *  Randomly shuffles a string.` |
|      - | 5338 | ` * Parameters` |
|      - | 5339 | ` *  $str` |
|      - | 5340 | ` *   The input string.` |
|      - | 5341 | ` * Return` |
|      - | 5342 | ` *  Returns the shuffled string.` |
|      - | 5343 | ` */` |
|     12 | 5344 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5345 |  |
|      - | 5346 | `	const char *zString;` |
|      - | 5347 | `	int nLen,i,c;` |
|      - | 5348 | `	sxu32 iR;` |
|     13 | 5349 | `	if( nArg < 1 ){` |
|      - | 5350 | `		/* Missing arguments,return the empty string */` |
|      3 | 5351 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5352 | `		return PH7_OK;` |
|      - | 5353 | `	}` |
|      - | 5354 | `	/* Extract the target string */` |
|     11 | 5355 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5356 | `	if( nLen < 1 ){` |
|      - | 5357 | `		/* Nothing to shuffle */` |
|      3 | 5358 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5359 | `		return PH7_OK;` |
|      - | 5360 | `	}` |
|      - | 5361 | `	/* Shuffle the string */` |
|     43 | 5362 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 5363 | `		/* Generate a random number first */` |
|     35 | 5364 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 5365 | `		/* Extract a random offset */` |
|     35 | 5366 | `		c = zString[iR % nLen];` |
|      - | 5367 | `		/* Append it */` |
|     35 | 5368 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 5369 | `	}` |
|      9 | 5370 | `	return PH7_OK;` |
|      7 | 5371 |  |
|      - | 5372 | `/*` |
|      - | 5373 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 5374 | ` *  Convert a string to an array.` |
|      - | 5375 | ` * Parameters` |
|      - | 5376 | ` * $string` |
|      - | 5377 | ` *  The input string.` |
|      - | 5378 | ` * $split_length` |
|      - | 5379 | ` *  Maximum length of the chunk.` |
|      - | 5380 | ` * Return` |
|      - | 5381 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 5382 | ` *  except possibly the last one which may be shorter.` |
|      - | 5383 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 5384 | ` *  as the first (and only) array element.` |
|      - | 5385 | ` *  An empty string returns an empty array.` |
|      - | 5386 | ` * Errors` |
|      - | 5387 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 5388 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 5389 | ` *  ValueError if $split_length is less than 1.` |
|      - | 5390 | ` */` |
|     28 | 5391 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5392 |  |
|      - | 5393 | `	const char *zString,*zEnd;` |
|      - | 5394 | `	ph7_value *pArray,*pValue;` |
|      - | 5395 | `	int split_len;` |
|      - | 5396 | `	int nLen;` |
|     30 | 5397 | `	if( nArg < 1 ){` |
|      4 | 5398 | `		return PH7_VmThrowException(pCtx,` |
|      - | 5399 | `			"ArgumentCountError",` |
|      - | 5400 | `			"str_split() expects at least 1 argument, %d given",` |
|      1 | 5401 | `			nArg` |
|      - | 5402 | `			);` |
|      - | 5403 | `	}` |
|      - | 5404 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     50 | 5405 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     38 | 5406 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 5407 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 5408 | `		return PH7_VmThrowException(pCtx,` |
|      - | 5409 | `			"TypeError",` |
|      - | 5410 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 5411 | `			ph7_type_name(apArg[0])` |
|      - | 5412 | `			);` |
|      - | 5413 | `	}` |
|      - | 5414 | `	/* Point to the target string */` |
|     26 | 5415 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     26 | 5416 | `	split_len = (int)sizeof(char);` |
|     26 | 5417 | `	if( nArg > 1 ){` |
|      - | 5418 | `		/* Split length */` |
|     16 | 5419 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     16 | 5420 | `		if( split_len < 1 ){` |
|      5 | 5421 | `			return PH7_VmThrowException(pCtx,` |
|      - | 5422 | `				"ValueError",` |
|      - | 5423 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 5424 | `				);` |
|      - | 5425 | `		}` |
|     11 | 5426 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 5427 | `			split_len = nLen;` |
|      1 | 5428 | `		}` |
|      5 | 5429 | `	}` |
|      - | 5430 | `	/* Create the array and the scalar value */` |
|     21 | 5431 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 5432 | `	/*Chunk value */` |
|     21 | 5433 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 5434 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 5435 | `		/* Return FALSE */` |
|    ! 0 | 5436 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5437 | `		return PH7_OK;` |
|      - | 5438 | `	}` |
|      - | 5439 | `	/* Point to the end of the string */` |
|     21 | 5440 | `	zEnd = &zString[nLen];` |
|      - | 5441 | `	/* Perform the requested operation */` |
|     48 | 5442 | `	for(;;){` |
|      - | 5443 | `		int nMax;` |
|     59 | 5444 | `		if( zString >= zEnd ){` |
|      - | 5445 | `			/* No more input to process */` |
|     21 | 5446 | `			break;` |
|      - | 5447 | `		}` |
|     39 | 5448 | `		nMax = (int)(zEnd-zString);` |
|     39 | 5449 | `		if( nMax < split_len ){` |
|      3 | 5450 | `			split_len = nMax;` |
|      1 | 5451 | `		}` |
|      - | 5452 | `		/* Copy the current chunk */` |
|     39 | 5453 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 5454 | `		/* Insert it */` |
|     39 | 5455 | `		ph7_array_add_elem(pArray,0,pValue); /* Will make it's own copy */` |
|      - | 5456 | `		/* reset the string cursor */` |
|     39 | 5457 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 5458 | `		/* Update position */` |
|     39 | 5459 | `		zString += split_len;` |
|      1 | 5460 | `	}` |
|      - | 5461 | `	/*` |
|      - | 5462 | `	 * Return the array.` |
|      - | 5463 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 5464 | `	 * upon we return from this function.` |
|      - | 5465 | `	 */` |
|     21 | 5466 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 5467 | `	return PH7_OK;` |
|     16 | 5468 |  |
|      - | 5469 | `/*` |
|      - | 5470 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 5471 | ` * Refer to [strspn()].` |
|      - | 5472 | ` */` |
|     28 | 5473 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 5474 |  |
|     29 | 5475 | `	const char *zIn = *pzIn;` |
|      - | 5476 | `	const char *zPtr;` |
|      - | 5477 | `	/* Ignore leading white spaces */` |
|     29 | 5478 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 5479 | `		zIn++;` |
|    ! 0 | 5480 | `	}` |
|     29 | 5481 | `	if( zIn >= zEnd ){` |
|      - | 5482 | `		/* End of input */` |
|    ! 0 | 5483 | `		return SXERR_EOF;` |
|      - | 5484 | `	}` |
|     29 | 5485 | `	zPtr = zIn;` |
|      - | 5486 | `	/* Extract the token */` |
|    201 | 5487 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 5488 | `		zIn++;` |
|      1 | 5489 | `	}` |
|     29 | 5490 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5491 | `	/* Synchronize pointers */` |
|     29 | 5492 | `	*pzIn = zIn;` |
|      - | 5493 | `	/* Return to the caller */` |
|     29 | 5494 | `	return SXRET_OK;` |
|     15 | 5495 |  |
|      - | 5496 | `/*` |
|      - | 5497 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 5498 | ` * return the longest match.` |
|      - | 5499 | ` * Refer to [strspn()].` |
|      - | 5500 | ` */` |
|     18 | 5501 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5502 |  |
|     19 | 5503 | `	const char *zEnd = &zString[nLen];` |
|     19 | 5504 | `	const char *zIn = zString;` |
|      - | 5505 | `	int i,c;` |
|     45 | 5506 | `	for(;;){` |
|     91 | 5507 | `		if( zString >= zEnd ){` |
|      7 | 5508 | `			break;` |
|      - | 5509 | `		}` |
|      - | 5510 | `		/* Extract current character */` |
|     85 | 5511 | `		c = zString[0];` |
|      - | 5512 | `		/* Perform the lookup */` |
|    383 | 5513 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 5514 | `			if( c == zMask[i] ){` |
|      - | 5515 | `				/* Character found */` |
|     73 | 5516 | `				break;` |
|      - | 5517 | `			}` |
|    150 | 5518 | `		}` |
|     85 | 5519 | `		if( i >= nMaskLen ){` |
|      - | 5520 | `			/* Character not in the current mask,break immediately */` |
|     13 | 5521 | `			break;` |
|      - | 5522 | `		}` |
|      - | 5523 | `		/* Advance cursor */` |
|     73 | 5524 | `		zString++;` |
|      1 | 5525 | `	}` |
|      - | 5526 | `	/* Longest match */` |
|     19 | 5527 | `	return (int)(zString-zIn);` |
|      1 | 5528 |  |
|      - | 5529 | `/*` |
|      - | 5530 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 5531 | ` * Refer to [strcspn()].` |
|      - | 5532 | ` */` |
|     10 | 5533 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5534 |  |
|     11 | 5535 | `	const char *zEnd = &zString[nLen];` |
|     11 | 5536 | `	const char *zIn = zString;` |
|      - | 5537 | `	int i,c;` |
|     12 | 5538 | `	for(;;){` |
|     25 | 5539 | `		if( zString >= zEnd ){` |
|      3 | 5540 | `			break;` |
|      - | 5541 | `		}` |
|      - | 5542 | `		/* Extract current character */` |
|     23 | 5543 | `		c = zString[0];` |
|      - | 5544 | `		/* Perform the lookup */` |
|     51 | 5545 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 5546 | `			if( c == zMask[i] ){` |
|      9 | 5547 | `				break;` |
|      - | 5548 | `			}` |
|     15 | 5549 | `		}` |
|     23 | 5550 | `		if( i < nMaskLen ){` |
|      - | 5551 | `			/* Character in the current mask,break immediately */` |
|      9 | 5552 | `			break;` |
|      - | 5553 | `		}` |
|      - | 5554 | `		/* Advance cursor */` |
|     15 | 5555 | `		zString++;` |
|      1 | 5556 | `	}` |
|      - | 5557 | `	/* Longest match */` |
|     11 | 5558 | `	return (int)(zString-zIn);` |
|      1 | 5559 |  |
|      - | 5560 | `/*` |
|      - | 5561 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5562 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 5563 | ` *  of characters contained within a given mask.` |
|      - | 5564 | ` * Parameters` |
|      - | 5565 | ` * $str` |
|      - | 5566 | ` *  The input string.` |
|      - | 5567 | ` * $mask` |
|      - | 5568 | ` *  The list of allowable characters.` |
|      - | 5569 | ` * $start` |
|      - | 5570 | ` *  The position in subject to start searching.` |
|      - | 5571 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5572 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5573 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5574 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5575 | ` *  start'th position from the end of subject.` |
|      - | 5576 | ` * $length` |
|      - | 5577 | ` *  The length of the segment from subject to examine.` |
|      - | 5578 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5579 | ` *  characters after the starting position.` |
|      - | 5580 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5581 | ` *  position up to length characters from the end of subject.` |
|      - | 5582 | ` * Return` |
|      - | 5583 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 5584 | ` * in mask.` |
|      - | 5585 | ` */` |
|     26 | 5586 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5587 |  |
|      - | 5588 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5589 | `	int iMasklen,iLen;` |
|      - | 5590 | `	SyString sToken;` |
|     27 | 5591 | `	int iCount = 0;` |
|      - | 5592 | `	int rc;` |
|     27 | 5593 | `	if( nArg < 2 ){` |
|      - | 5594 | `		/* Missing agruments,return zero */` |
|      3 | 5595 | `		ph7_result_int(pCtx,0);` |
|      3 | 5596 | `		return PH7_OK;` |
|      - | 5597 | `	}` |
|      - | 5598 | `	/* Extract the target string */` |
|     25 | 5599 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5600 | `	/* Extract the mask */` |
|     25 | 5601 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 5602 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 5603 | `		/* Nothing to process,return zero */` |
|      7 | 5604 | `		ph7_result_int(pCtx,0);` |
|      7 | 5605 | `		return PH7_OK;` |
|      - | 5606 | `	}` |
|     19 | 5607 | `	if( nArg > 2 ){` |
|      - | 5608 | `		int nOfft;` |
|      - | 5609 | `		/* Extract the offset */` |
|      9 | 5610 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 5611 | `		if( nOfft < 0 ){` |
|    ! 0 | 5612 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5613 | `			if( zBase > zString ){` |
|    ! 0 | 5614 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5615 | `				zString = zBase;` |
|    ! 0 | 5616 | `			}else{` |
|      - | 5617 | `				/* Invalid offset */` |
|    ! 0 | 5618 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5619 | `				return PH7_OK;` |
|      - | 5620 | `			}` |
|    ! 0 | 5621 | `		}else{` |
|      9 | 5622 | `			if( nOfft >= iLen ){` |
|      - | 5623 | `				/* Invalid offset */` |
|    ! 0 | 5624 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5625 | `				return PH7_OK;` |
|    ! 0 | 5626 | `			}else{` |
|      - | 5627 | `				/* Update offset */` |
|      9 | 5628 | `				zString += nOfft;` |
|      9 | 5629 | `				iLen -= nOfft;` |
|      - | 5630 | `			}` |
|      - | 5631 | `		}` |
|      9 | 5632 | `		if( nArg > 3 ){` |
|      - | 5633 | `			int iUserlen;` |
|      - | 5634 | `			/* Extract the desired length */` |
|      9 | 5635 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 5636 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 5637 | `				iLen = iUserlen;` |
|      2 | 5638 | `			}` |
|      4 | 5639 | `		}` |
|      4 | 5640 | `	}` |
|      - | 5641 | `	/* Point to the end of the string */` |
|     19 | 5642 | `	zEnd = &zString[iLen];` |
|      - | 5643 | `	/* Extract the first non-space token */` |
|     19 | 5644 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 5645 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5646 | `		/* Compare against the current mask */` |
|     19 | 5647 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 5648 | `	}` |
|      - | 5649 | `	/* Longest match */` |
|     19 | 5650 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 5651 | `	return PH7_OK;` |
|     14 | 5652 |  |
|      - | 5653 | `/*` |
|      - | 5654 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5655 | ` *  Find length of initial segment not matching mask.` |
|      - | 5656 | ` * Parameters` |
|      - | 5657 | ` * $str` |
|      - | 5658 | ` *  The input string.` |
|      - | 5659 | ` * $mask` |
|      - | 5660 | ` *  The list of not allowed characters.` |
|      - | 5661 | ` * $start` |
|      - | 5662 | ` *  The position in subject to start searching.` |
|      - | 5663 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5664 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5665 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5666 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5667 | ` *  start'th position from the end of subject.` |
|      - | 5668 | ` * $length` |
|      - | 5669 | ` *  The length of the segment from subject to examine.` |
|      - | 5670 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5671 | ` *  characters after the starting position.` |
|      - | 5672 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5673 | ` *  position up to length characters from the end of subject.` |
|      - | 5674 | ` * Return` |
|      - | 5675 | ` *  Returns the length of the segment as an integer.` |
|      - | 5676 | ` */` |
|     16 | 5677 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5678 |  |
|      - | 5679 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5680 | `	int iMasklen,iLen;` |
|      - | 5681 | `	SyString sToken;` |
|     17 | 5682 | `	int iCount = 0;` |
|      - | 5683 | `	int rc;` |
|     17 | 5684 | `	if( nArg < 2 ){` |
|      - | 5685 | `		/* Missing agruments,return zero */` |
|      3 | 5686 | `		ph7_result_int(pCtx,0);` |
|      3 | 5687 | `		return PH7_OK;` |
|      - | 5688 | `	}` |
|      - | 5689 | `	/* Extract the target string */` |
|     15 | 5690 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5691 | `	/* Extract the mask */` |
|     15 | 5692 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 5693 | `	if( iLen < 1 ){` |
|      - | 5694 | `		/* Nothing to process,return zero */` |
|    ! 0 | 5695 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5696 | `		return PH7_OK;` |
|      - | 5697 | `	}` |
|     15 | 5698 | `	if( iMasklen < 1 ){` |
|      - | 5699 | `		/* No given mask,return the string length */` |
|      3 | 5700 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 5701 | `		return PH7_OK;` |
|      - | 5702 | `	}` |
|     13 | 5703 | `	if( nArg > 2 ){` |
|      - | 5704 | `		int nOfft;` |
|      - | 5705 | `		/* Extract the offset */` |
|     11 | 5706 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 5707 | `		if( nOfft < 0 ){` |
|    ! 0 | 5708 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5709 | `			if( zBase > zString ){` |
|    ! 0 | 5710 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5711 | `				zString = zBase;` |
|    ! 0 | 5712 | `			}else{` |
|      - | 5713 | `				/* Invalid offset */` |
|    ! 0 | 5714 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5715 | `				return PH7_OK;` |
|      - | 5716 | `			}` |
|    ! 0 | 5717 | `		}else{` |
|     11 | 5718 | `			if( nOfft >= iLen ){` |
|      - | 5719 | `				/* Invalid offset */` |
|      3 | 5720 | `				ph7_result_int(pCtx,0);` |
|      3 | 5721 | `				return PH7_OK;` |
|    ! 0 | 5722 | `			}else{` |
|      - | 5723 | `				/* Update offset */` |
|      9 | 5724 | `				zString += nOfft;` |
|      9 | 5725 | `				iLen -= nOfft;` |
|      - | 5726 | `			}` |
|      - | 5727 | `		}` |
|      9 | 5728 | `		if( nArg > 3 ){` |
|      - | 5729 | `			int iUserlen;` |
|      - | 5730 | `			/* Extract the desired length */` |
|    ! 0 | 5731 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 5732 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 5733 | `				iLen = iUserlen;` |
|    ! 0 | 5734 | `			}` |
|    ! 0 | 5735 | `		}` |
|      4 | 5736 | `	}` |
|      - | 5737 | `	/* Point to the end of the string */` |
|     11 | 5738 | `	zEnd = &zString[iLen];` |
|      - | 5739 | `	/* Extract the first non-space token */` |
|     11 | 5740 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 5741 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5742 | `		/* Compare against the current mask */` |
|     11 | 5743 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 5744 | `	}` |
|      - | 5745 | `	/* Longest match */` |
|     11 | 5746 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 5747 | `	return PH7_OK;` |
|      9 | 5748 |  |
|      - | 5749 | `/*` |
|      - | 5750 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 5751 | ` *  Search a string for any of a set of characters.` |
|      - | 5752 | ` * Parameters` |
|      - | 5753 | ` *  $haystack` |
|      - | 5754 | ` *   The string where char_list is looked for.` |
|      - | 5755 | ` *  $char_list` |
|      - | 5756 | ` *   This parameter is case sensitive.` |
|      - | 5757 | ` * Return` |
|      - | 5758 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 5759 | ` */` |
|      6 | 5760 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5761 |  |
|      - | 5762 | `	const char *zString,*zList,*zEnd;` |
|      - | 5763 | `	int iLen,iListLen,i,c;` |
|      - | 5764 | `	sxu32 nOfft,nMax;` |
|      - | 5765 | `	sxi32 rc;` |
|      7 | 5766 | `	if( nArg < 2 ){` |
|      - | 5767 | `		/* Missing arguments,return FALSE */` |
|      3 | 5768 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5769 | `		return PH7_OK;` |
|      - | 5770 | `	}` |
|      - | 5771 | `	/* Extract the haystack and the char list */` |
|      5 | 5772 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 5773 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 5774 | `	if( iLen < 1 ){` |
|      - | 5775 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 5776 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5777 | `		return PH7_OK;` |
|      - | 5778 | `	}` |
|      - | 5779 | `	/* Point to the end of the string */` |
|      5 | 5780 | `	zEnd = &zString[iLen];` |
|      5 | 5781 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 5782 | `	/* perform the requested operation */` |
|     15 | 5783 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 5784 | `		c = zList[i];` |
|     11 | 5785 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 5786 | `		if( rc == SXRET_OK ){` |
|      5 | 5787 | `			if( nMax < nOfft ){` |
|      3 | 5788 | `				nOfft = nMax;` |
|      1 | 5789 | `			}` |
|      2 | 5790 | `		}` |
|      6 | 5791 | `	}` |
|      5 | 5792 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 5793 | `		/* No such substring,return FALSE */` |
|      3 | 5794 | `		ph7_result_bool(pCtx,0);` |
|      2 | 5795 | `	}else{` |
|      - | 5796 | `		/* Return the substring */` |
|      3 | 5797 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 5798 | `	}` |
|      5 | 5799 | `	return PH7_OK;` |
|      4 | 5800 |  |
|      - | 5801 | `/*` |
|      - | 5802 | ` * string soundex(string $str)` |
|      - | 5803 | ` *  Calculate the soundex key of a string.` |
|      - | 5804 | ` * Parameters` |
|      - | 5805 | ` *  $str` |
|      - | 5806 | ` *   The input string.` |
|      - | 5807 | ` * Return` |
|      - | 5808 | ` *  Returns the soundex key as a string.` |
|      - | 5809 | ` * Note:` |
|      - | 5810 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 5811 | ` * source tree.` |
|      - | 5812 | ` */` |
|     20 | 5813 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5814 |  |
|      - | 5815 | `	const unsigned char *zIn;` |
|      - | 5816 | `	char zResult[8];` |
|      - | 5817 | `	int i, j;` |
|      - | 5818 | `	static const unsigned char iCode[] = {` |
|      - | 5819 |  |
|      - | 5820 |  |
|      - | 5821 |  |
|      - | 5822 |  |
|      - | 5823 |  |
|      - | 5824 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5825 |  |
|      - | 5826 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5827 | `	};` |
|     21 | 5828 | `	if( nArg < 1 ){` |
|      - | 5829 | `		/* Missing arguments,return the empty string */` |
|      3 | 5830 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5831 | `		return PH7_OK;` |
|      - | 5832 | `	}` |
|     19 | 5833 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     19 | 5834 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     19 | 5835 | `	if( zIn[i] ){` |
|     17 | 5836 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 5837 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 5838 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 5839 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 5840 | `			if( code>0 ){` |
|     45 | 5841 | `				if( code!=prevcode ){` |
|     33 | 5842 | `					prevcode = (unsigned char)code;` |
|     33 | 5843 | `					zResult[j++] = (char)code + '0';` |
|     16 | 5844 | `				}` |
|     23 | 5845 | `			}else{` |
|     49 | 5846 | `				prevcode = 0;` |
|      - | 5847 | `			}` |
|     47 | 5848 | `		}` |
|     33 | 5849 | `		while( j<4 ){` |
|     17 | 5850 | `			zResult[j++] = '0';` |
|      1 | 5851 | `		}` |
|     17 | 5852 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 5853 | `	}else{` |
|      3 | 5854 | `	  ph7_result_string(pCtx,"?000",4);` |
|      - | 5855 | `	}` |
|     19 | 5856 | `	return PH7_OK;` |
|     11 | 5857 |  |
|      - | 5858 | `/*` |
|      - | 5859 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 5860 | ` *  Wraps a string to a given number of characters.` |
|      - | 5861 | ` * Parameters` |
|      - | 5862 | ` *  $str` |
|      - | 5863 | ` *   The input string.` |
|      - | 5864 | ` * $width` |
|      - | 5865 | ` *  The column width.` |
|      - | 5866 | ` * $break` |
|      - | 5867 | ` *  The line is broken using the optional break parameter.` |
|      - | 5868 | ` * Return` |
|      - | 5869 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 5870 | ` */` |
|     14 | 5871 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5872 |  |
|      - | 5873 | `	const char *zIn,*zEnd,*zBreak;` |
|      - | 5874 | `	int iLen,iBreaklen,iChunk;` |
|     15 | 5875 | `	if( nArg < 1 ){` |
|      - | 5876 | `		/* Missing arguments,return the empty string */` |
|      3 | 5877 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5878 | `		return PH7_OK;` |
|      - | 5879 | `	}` |
|      - | 5880 | `	/* Extract the input string */` |
|     13 | 5881 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|     13 | 5882 | `	if( iLen < 1 ){` |
|      - | 5883 | `		/* Nothing to process,return the empty string */` |
|      3 | 5884 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5885 | `		return PH7_OK;` |
|      - | 5886 | `	}` |
|      - | 5887 | `	/* Chunk length */` |
|     11 | 5888 | `	iChunk = 75;` |
|     11 | 5889 | `	iBreaklen = 0;` |
|     11 | 5890 | `	zBreak = ""; /* cc warning */` |
|     11 | 5891 | `	if( nArg > 1 ){` |
|     11 | 5892 | `		iChunk = ph7_value_to_int(apArg[1]);` |
|     11 | 5893 | `		if( iChunk < 1 ){` |
|    ! 0 | 5894 | `			iChunk = 75;` |
|    ! 0 | 5895 | `		}` |
|     11 | 5896 | `		if( nArg > 2 ){` |
|      3 | 5897 | `			zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      1 | 5898 | `		}` |
|      5 | 5899 | `	}` |
|     11 | 5900 | `	if( iBreaklen < 1 ){` |
|      - | 5901 | `		/* Set a default column break */` |
|      - | 5902 | `#ifdef __WINNT__` |
|      1 | 5903 | `		zBreak = "\r\n";` |
|      1 | 5904 | `		iBreaklen = (int)sizeof("\r\n")-1;` |
|      - | 5905 | `#else` |
|      8 | 5906 | `		zBreak = "\n";` |
|      8 | 5907 | `		iBreaklen = (int)sizeof(char);` |
|      - | 5908 | `#endif` |
|      4 | 5909 | `	}` |
|      - | 5910 | `	/* Perform the requested operation */` |
|     11 | 5911 | `	zEnd = &zIn[iLen];` |
|     41 | 5912 | `	for(;;){` |
|      - | 5913 | `		int nMax;` |
|     47 | 5914 | `		if( zIn >= zEnd ){` |
|      - | 5915 | `			/* No more input to process */` |
|     11 | 5916 | `			break;` |
|      - | 5917 | `		}` |
|     37 | 5918 | `		nMax = (int)(zEnd-zIn);` |
|     37 | 5919 | `		if( iChunk > nMax ){` |
|     11 | 5920 | `			iChunk = nMax;` |
|      5 | 5921 | `		}` |
|      - | 5922 | `		/* Append the column first */` |
|     37 | 5923 | `		ph7_result_string(pCtx,zIn,iChunk); /* Will make it's own copy */` |
|      - | 5924 | `		/* Advance the cursor */` |
|     37 | 5925 | `		zIn += iChunk;` |
|     37 | 5926 | `		if( zIn < zEnd ){` |
|      - | 5927 | `			/* Append the line break */` |
|     27 | 5928 | `			ph7_result_string(pCtx,zBreak,iBreaklen);` |
|     13 | 5929 | `		}` |
|      1 | 5930 | `	}` |
|     11 | 5931 | `	return PH7_OK;` |
|      8 | 5932 |  |
|      - | 5933 | `/*` |
|      - | 5934 | ` * Check if the given character is a member of the given mask.` |
|      - | 5935 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 5936 | ` * Refer to [strtok()].` |
|      - | 5937 | ` */` |
|     30 | 5938 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 5939 |  |
|      - | 5940 | `	int i;` |
|     57 | 5941 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 5942 | `		if( c == zMask[i] ){` |
|     13 | 5943 | `			if( pOfft ){` |
|      5 | 5944 | `				*pOfft = i;` |
|      2 | 5945 | `			}` |
|     13 | 5946 | `			return TRUE;` |
|      - | 5947 | `		}` |
|     14 | 5948 | `	}` |
|     19 | 5949 | `	return FALSE;` |
|     16 | 5950 |  |
|      - | 5951 | `/*` |
|      - | 5952 | ` * Extract a single token from the input stream.` |
|      - | 5953 | ` * Refer to [strtok()].` |
|      - | 5954 | ` */` |
|      6 | 5955 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 5956 |  |
|      7 | 5957 | `	const char *zIn = *pzIn;` |
|      - | 5958 | `	const char *zPtr;` |
|      - | 5959 | `	/* Ignore leading delimiter */` |
|     11 | 5960 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5961 | `		zIn++;` |
|      1 | 5962 | `	}` |
|      7 | 5963 | `	if( zIn >= zEnd ){` |
|      - | 5964 | `		/* End of input */` |
|    ! 0 | 5965 | `		return SXERR_EOF;` |
|      - | 5966 | `	}` |
|      7 | 5967 | `	zPtr = zIn;` |
|      - | 5968 | `	/* Extract the token */` |
|     13 | 5969 | `	while( zIn < zEnd ){` |
|     11 | 5970 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 5971 | `			/* UTF-8 stream */` |
|    ! 0 | 5972 | `			zIn++;` |
|    ! 0 | 5973 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 5974 | `		}else{` |
|     11 | 5975 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5976 | `				break;` |
|      - | 5977 | `			}` |
|      7 | 5978 | `			zIn++;` |
|      - | 5979 | `		}` |
|      1 | 5980 | `	}` |
|      7 | 5981 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5982 | `	/* Update the cursor */` |
|      7 | 5983 | `	*pzIn = zIn;` |
|      - | 5984 | `	/* Return to the caller */` |
|      7 | 5985 | `	return SXRET_OK;` |
|      4 | 5986 |  |
|      - | 5987 | `/* strtok auxiliary private data */` |
|      - | 5988 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 5989 | `struct strtok_aux_data` |
|      - | 5990 |  |
|      - | 5991 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 5992 | `	const char *zIn;   /* Current input stream */` |
|      - | 5993 | `	const char *zEnd;  /* End of input */` |
|      - | 5994 | `};` |
|      - | 5995 | `/*` |
|      - | 5996 | ` * string strtok(string $str,string $token)` |
|      - | 5997 | ` * string strtok(string $token)` |
|      - | 5998 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 5999 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 6000 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 6001 | ` *  words by using the space character as the token.` |
|      - | 6002 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 6003 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 6004 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 6005 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 6006 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 6007 | ` *  the argument are found.` |
|      - | 6008 | ` * Parameters` |
|      - | 6009 | ` *  $str` |
|      - | 6010 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 6011 | ` * $token` |
|      - | 6012 | ` *  The delimiter used when splitting up str.` |
|      - | 6013 | ` * Return` |
|      - | 6014 | ` *   Current token or FALSE on EOF.` |
|      - | 6015 | ` */` |
|      8 | 6016 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6017 |  |
|      - | 6018 | `	strtok_aux_data *pAux;` |
|      - | 6019 | `	const char *zMask;` |
|      - | 6020 | `	SyString sToken;` |
|      - | 6021 | `	int nMasklen;` |
|      - | 6022 | `	sxi32 rc;` |
|      9 | 6023 | `	if( nArg < 2 ){` |
|      - | 6024 | `		/* Extract top aux data */` |
|      7 | 6025 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      7 | 6026 | `		if( pAux == 0 ){` |
|      - | 6027 | `			/* No aux data,return FALSE */` |
|    ! 0 | 6028 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6029 | `			return PH7_OK;` |
|      - | 6030 | `		}` |
|      7 | 6031 | `		nMasklen = 0;` |
|      7 | 6032 | `		zMask = ""; /* cc warning */` |
|      7 | 6033 | `		if( nArg > 0 ){` |
|      - | 6034 | `			/* Extract the mask */` |
|      5 | 6035 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 6036 | `		}` |
|      7 | 6037 | `		if( nMasklen < 1 ){` |
|      - | 6038 | `			/* Invalid mask,return FALSE */` |
|      3 | 6039 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|      3 | 6040 | `			ph7_context_free_chunk(pCtx,pAux);` |
|      3 | 6041 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|      3 | 6042 | `			ph7_result_bool(pCtx,0);` |
|      3 | 6043 | `			return PH7_OK;` |
|      - | 6044 | `		}` |
|      - | 6045 | `		/* Extract the token */` |
|      5 | 6046 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 6047 | `		if( rc != SXRET_OK ){` |
|      - | 6048 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 6049 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 6050 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6051 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 6052 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6053 | `		}else{` |
|      - | 6054 | `			/* Return the extracted token */` |
|      5 | 6055 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6056 | `		}` |
|      3 | 6057 | `	}else{` |
|      - | 6058 | `		const char *zInput,*zCur;` |
|      - | 6059 | `		char *zDup;` |
|      - | 6060 | `		int nLen;` |
|      - | 6061 | `		/* Extract the raw input */` |
|      3 | 6062 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 6063 | `		if( nLen < 1 ){` |
|      - | 6064 | `			/* Empty input,return FALSE */` |
|    ! 0 | 6065 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6066 | `			return PH7_OK;` |
|      - | 6067 | `		}` |
|      - | 6068 | `		/* Extract the mask */` |
|      3 | 6069 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 6070 | `		if( nMasklen < 1 ){` |
|      - | 6071 | `			/* Set a default mask */` |
|      - | 6072 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 6073 | `			zMask = TOK_MASK;` |
|    ! 0 | 6074 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 6075 | `#undef TOK_MASK` |
|    ! 0 | 6076 | `		}` |
|      - | 6077 | `		/* Extract a single token */` |
|      3 | 6078 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 6079 | `		if( rc != SXRET_OK ){` |
|      - | 6080 | `			/* Empty input */` |
|    ! 0 | 6081 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6082 | `			return PH7_OK;` |
|    ! 0 | 6083 | `		}else{` |
|      - | 6084 | `			/* Return the extracted token */` |
|      3 | 6085 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6086 | `		}` |
|      - | 6087 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 6088 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 6089 | `		if( pAux ){` |
|      3 | 6090 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 6091 | `			if( nLen < 1 ){` |
|    ! 0 | 6092 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6093 | `				return PH7_OK;` |
|      - | 6094 | `			}` |
|      - | 6095 | `			/* Duplicate input */` |
|      3 | 6096 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 6097 | `			if( zDup  ){` |
|      3 | 6098 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 6099 | `				/* Register the aux data */` |
|      3 | 6100 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 6101 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 6102 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 6103 | `			}` |
|      1 | 6104 | `		}` |
|      - | 6105 | `	}` |
|      7 | 6106 | `	return PH7_OK;` |
|      5 | 6107 |  |
|      - | 6108 | `/*` |
|      - | 6109 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 6110 | ` *  Pad a string to a certain length with another string` |
|      - | 6111 | ` * Parameters` |
|      - | 6112 | ` *  $input` |
|      - | 6113 | ` *   The input string.` |
|      - | 6114 | ` * $pad_length` |
|      - | 6115 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 6116 | ` *   string, no padding takes place.` |
|      - | 6117 | ` * $pad_string` |
|      - | 6118 | ` *   Note:` |
|      - | 6119 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 6120 | ` *    divided by the pad_string's length.` |
|      - | 6121 | ` * $pad_type` |
|      - | 6122 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 6123 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 6124 | ` * Return` |
|      - | 6125 | ` *  The padded string.` |
|      - | 6126 | ` */` |
|     10 | 6127 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6128 |  |
|      - | 6129 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 6130 | `	const char *zIn,*zPad;` |
|     11 | 6131 | `	if( nArg < 2 ){` |
|      - | 6132 | `		/* Missing arguments,return the empty string */` |
|      5 | 6133 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 6134 | `		return PH7_OK;` |
|      - | 6135 | `	}` |
|      - | 6136 | `	/* Extract the target string */` |
|      7 | 6137 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6138 | `	/* Padding length */` |
|      7 | 6139 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|      7 | 6140 | `	if( iPadlen > 0 ){` |
|      5 | 6141 | `		iPadlen -= iLen;` |
|      2 | 6142 | `	}` |
|      7 | 6143 | `	if( iPadlen < 1  ){` |
|      - | 6144 | `		/* Return the string verbatim */` |
|      3 | 6145 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      3 | 6146 | `		return PH7_OK;` |
|      - | 6147 | `	}` |
|      5 | 6148 | `	zPad = " "; /* Whitespace padding */` |
|      5 | 6149 | `	iStrpad = (int)sizeof(char);` |
|      5 | 6150 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      5 | 6151 | `	if( nArg > 2 ){` |
|      - | 6152 | `		/* Padding string */` |
|      5 | 6153 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      5 | 6154 | `		if( iStrpad < 1 ){` |
|      - | 6155 | `			/* Empty string */` |
|    ! 0 | 6156 | `			zPad = " "; /* Whitespace padding */` |
|    ! 0 | 6157 | `			iStrpad = (int)sizeof(char);` |
|    ! 0 | 6158 | `		}` |
|      5 | 6159 | `		if( nArg > 3 ){` |
|      - | 6160 | `			/* Padd type */` |
|      5 | 6161 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 6162 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 6163 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 6164 | `			}` |
|      2 | 6165 | `		}` |
|      2 | 6166 | `	}` |
|      5 | 6167 | `	iDiv = 1;` |
|      5 | 6168 | `	if( iType == 2 ){` |
|    ! 0 | 6169 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 6170 | `	}` |
|      - | 6171 | `	/* Perform the requested operation */` |
|      5 | 6172 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 6173 | `		jPad = iStrpad;` |
|      5 | 6174 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 6175 | `			/* Padding */` |
|      5 | 6176 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 6177 | `				break;` |
|      - | 6178 | `			}` |
|      3 | 6179 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      2 | 6180 | `		}` |
|      3 | 6181 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 6182 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 6183 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 6184 | `				if( jPad > iStrpad ){` |
|    ! 0 | 6185 | `					jPad = iStrpad;` |
|    ! 0 | 6186 | `				}` |
|      3 | 6187 | `				if( jPad < 1){` |
|    ! 0 | 6188 | `					break;` |
|      - | 6189 | `				}` |
|      3 | 6190 | `				ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 6191 | `			}` |
|      1 | 6192 | `		}` |
|      1 | 6193 | `	}` |
|      5 | 6194 | `	if( iLen > 0 ){` |
|      - | 6195 | `		/* Append the input string */` |
|      5 | 6196 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      2 | 6197 | `	}` |
|      5 | 6198 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 6199 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 6200 | `			/* Padding */` |
|      5 | 6201 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 6202 | `				break;` |
|      - | 6203 | `			}` |
|      3 | 6204 | `			ph7_result_string(pCtx,zPad,iStrpad);` |
|      2 | 6205 | `		}` |
|      5 | 6206 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 6207 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 6208 | `			if( jPad > iStrpad ){` |
|    ! 0 | 6209 | `				jPad = iStrpad;` |
|    ! 0 | 6210 | `			}` |
|      3 | 6211 | `			if( jPad < 1){` |
|    ! 0 | 6212 | `				break;` |
|      - | 6213 | `			}` |
|      3 | 6214 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 6215 | `		}` |
|      1 | 6216 | `	}` |
|      5 | 6217 | `	return PH7_OK;` |
|      6 | 6218 |  |
|      - | 6219 | `/*` |
|      - | 6220 | ` * String replacement private data.` |
|      - | 6221 | ` */` |
|      - | 6222 | `typedef struct str_replace_data str_replace_data;` |
|      - | 6223 | `struct str_replace_data` |
|      - | 6224 |  |
|      - | 6225 | `	/* The following two fields are only used by the strtr function */` |
|      - | 6226 | `	SyBlob *pWorker;         /* Working buffer */` |
|      - | 6227 | `	ProcStringMatch xMatch;  /* Pattern match routine */` |
|      - | 6228 | `	/* The following two fields are only used by the str_replace function */` |
|      - | 6229 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 6230 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 6231 | `};` |
|      - | 6232 | `/*` |
|      - | 6233 | ` * Remove a substring.` |
|      - | 6234 | ` */` |
|      - | 6235 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 6236 | `	for(;;){\` |
|      - | 6237 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 6238 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 6239 | `		++OFFT;\` |
|      - | 6240 | `	}\` |
|      - | 6241 |  |
|      - | 6242 | `/*` |
|      - | 6243 | ` * Shift right and insert algorithm.` |
|      - | 6244 | ` */` |
|      - | 6245 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 6246 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 6247 | `		for(;;){\` |
|      - | 6248 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 6249 | `			if(INLEN < 1 ) { break; }\` |
|      - | 6250 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 6251 | `			--INLEN; \` |
|      - | 6252 | `		}\` |
|      - | 6253 | `		for(;;){\` |
|      - | 6254 | `				if(ELEN < 1) { break; }\` |
|      - | 6255 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 6256 | `				OFFT++;\` |
|      - | 6257 | `				ENTRY++;\` |
|      - | 6258 | `				--ELEN;\` |
|      - | 6259 | `		}\` |
|      - | 6260 |  |
|      - | 6261 | `/*` |
|      - | 6262 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 6263 | ` * replacement string [i.e: zReplace].` |
|      - | 6264 | ` */` |
|     38 | 6265 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 6266 |  |
|     39 | 6267 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 6268 | `	sxu32 n,m;` |
|     39 | 6269 | `	n = SyBlobLength(pWorker);` |
|     39 | 6270 | `	m = nOfft;` |
|      - | 6271 | `	/* Delete the old entry */` |
|    475 | 6272 | `	STRDEL(zInput,n,m,nLen);` |
|     39 | 6273 | `	SyBlobLength(pWorker) -= nLen;` |
|     39 | 6274 | `	if( nReplen > 0 ){` |
|     33 | 6275 | `		sxi32 iRep = nReplen;` |
|      - | 6276 | `		sxi32 rc;` |
|      - | 6277 | `		/*` |
|      - | 6278 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 6279 | `		 * string.` |
|      - | 6280 | `		 */` |
|     33 | 6281 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     33 | 6282 | `		if( rc != SXRET_OK ){` |
|      - | 6283 | `			/* Simply ignore any memory failure problem */` |
|    ! 0 | 6284 | `			return SXRET_OK;` |
|      - | 6285 | `		}` |
|      - | 6286 | `		/* Perform the insertion now */` |
|     33 | 6287 | `		zInput = (char *)SyBlobData(pWorker);` |
|     33 | 6288 | `		n = SyBlobLength(pWorker);` |
|    163 | 6289 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     33 | 6290 | `		SyBlobLength(pWorker) += nReplen;` |
|     16 | 6291 | `	}` |
|     39 | 6292 | `	return SXRET_OK;` |
|     20 | 6293 |  |
|      - | 6294 | `/*` |
|      - | 6295 | ` * String replacement walker callback.` |
|      - | 6296 | ` * The following callback is invoked for each array entry that hold` |
|      - | 6297 | ` * the replace string.` |
|      - | 6298 | ` * Refer to the strtr() implementation for more information.` |
|      - | 6299 | ` */` |
|      8 | 6300 | `static int StringReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6301 |  |
|      9 | 6302 | `	str_replace_data *pRepData = (str_replace_data *)pUserData;` |
|      - | 6303 | `	const char *zTarget,*zReplace;` |
|      - | 6304 | `	SyBlob *pWorker;` |
|      - | 6305 | `	int tLen,nLen;` |
|      - | 6306 | `	sxu32 nOfft;` |
|      - | 6307 | `	sxi32 rc;` |
|      - | 6308 | `	/* Point to the working buffer */` |
|      9 | 6309 | `	pWorker = pRepData->pWorker;` |
|      9 | 6310 | `	if( !ph7_value_is_string(pKey) ){` |
|      - | 6311 | `		/* Target and replace must be a string */` |
|      3 | 6312 | `		return PH7_OK;` |
|      - | 6313 | `	}` |
|      - | 6314 | `	/* Extract the target and the replace */` |
|      7 | 6315 | `	zTarget = ph7_value_to_string(pKey,&tLen);` |
|      7 | 6316 | `	if( tLen < 1 ){` |
|      - | 6317 | `		/* Empty target,return immediately */` |
|    ! 0 | 6318 | `		return PH7_OK;` |
|      - | 6319 | `	}` |
|      - | 6320 | `	/* Perform a pattern search */` |
|      7 | 6321 | `	rc = pRepData->xMatch(SyBlobData(pWorker),SyBlobLength(pWorker),(const void *)zTarget,(sxu32)tLen,&nOfft);` |
|      7 | 6322 | `	if( rc != SXRET_OK ){` |
|      - | 6323 | `		/* Pattern not found */` |
|    ! 0 | 6324 | `		return PH7_OK;` |
|      - | 6325 | `	}` |
|      - | 6326 | `	/* Extract the replace string */` |
|      7 | 6327 | `	zReplace = ph7_value_to_string(pData,&nLen);` |
|      - | 6328 | `	/* Perform the replace process */` |
|      7 | 6329 | `	StringReplace(pWorker,nOfft,tLen,zReplace,nLen);` |
|      - | 6330 | `	/* All done */` |
|      7 | 6331 | `	return PH7_OK;` |
|      5 | 6332 |  |
|      - | 6333 | `/*` |
|      - | 6334 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 6335 | ` * to collect search/replace string.` |
|      - | 6336 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 6337 | ` */` |
|     26 | 6338 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6339 |  |
|     27 | 6340 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 6341 | `	SyString sWorker;` |
|      - | 6342 | `	const char *zIn;` |
|      - | 6343 | `	int nByte;` |
|      - | 6344 | `	/* Extract a string representation of the given argument */` |
|     27 | 6345 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 6346 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 6347 | `	if( nByte > 0 ){` |
|      - | 6348 | `		char *zDup;` |
|      - | 6349 | `		/* Duplicate the chunk */` |
|     25 | 6350 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 6351 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 6352 | `			);` |
|     25 | 6353 | `		if( zDup == 0 ){` |
|      - | 6354 | `			/* Ignore any memory failure problem */` |
|    ! 0 | 6355 | `			ph7_context_throw_error(pRep->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 6356 | `			return PH7_OK;` |
|      - | 6357 | `		}` |
|     25 | 6358 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 6359 | `		/* Save the chunk */` |
|     25 | 6360 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 6361 | `	}` |
|      - | 6362 | `	/* Save for later processing */` |
|     27 | 6363 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 6364 | `	/* All done */` |
|     13 | 6365 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 6366 | `	return PH7_OK;` |
|     14 | 6367 |  |
|      - | 6368 | `/*` |
|      - | 6369 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6370 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6371 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 6372 | ` * Parameters` |
|      - | 6373 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 6374 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 6375 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 6376 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 6377 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 6378 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 6379 | ` * $search` |
|      - | 6380 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 6381 | ` *  to designate multiple needles.` |
|      - | 6382 | ` * $replace` |
|      - | 6383 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 6384 | ` *  to designate multiple replacements.` |
|      - | 6385 | ` * $subject` |
|      - | 6386 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 6387 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 6388 | ` *  of subject, and the return value is an array as well.` |
|      - | 6389 | ` * $count (Not used)` |
|      - | 6390 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 6391 | ` * Return` |
|      - | 6392 | ` * This function returns a string or an array with the replaced values.` |
|      - | 6393 | ` */` |
|  15554 | 6394 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6395 |  |
|      - | 6396 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 6397 | `	ProcStringMatch xMatch;` |
|      - | 6398 | `	const char *zIn,*zFunc;` |
|      - | 6399 | `	str_replace_data sRep;` |
|      - | 6400 | `	SyBlob sWorker;` |
|      - | 6401 | `	SySet sReplace;` |
|      - | 6402 | `	SySet sSearch;` |
|      - | 6403 | `	int rep_str;` |
|      - | 6404 | `	int nByte;` |
|      - | 6405 | `	sxi32 rc;` |
|  15556 | 6406 | `	if( nArg < 3 ){` |
|      - | 6407 | `		/* Missing/Invalid arguments,return null */` |
|      7 | 6408 | `		ph7_result_null(pCtx);` |
|      7 | 6409 | `		return PH7_OK;` |
|      - | 6410 | `	}` |
|      - | 6411 | `	/* Initialize fields */` |
|  15550 | 6412 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  15550 | 6413 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  15550 | 6414 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  15550 | 6415 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  15550 | 6416 | `	sRep.pCtx = pCtx;` |
|  15550 | 6417 | `	sRep.pCollector = &sSearch;` |
|  15550 | 6418 | `	rep_str = 0;` |
|      - | 6419 | `	/* Extract the subject */` |
|  15550 | 6420 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  15550 | 6421 | `	if( nByte < 1 ){` |
|      - | 6422 | `		/* Nothing to replace,return the empty string */` |
|     38 | 6423 | `		ph7_result_string(pCtx,"",0);` |
|     38 | 6424 | `		return PH7_OK;` |
|      - | 6425 | `	}` |
|      - | 6426 | `	/* Copy the subject */` |
|  15514 | 6427 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 6428 | `	/* Search string */` |
|  15514 | 6429 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 6430 | `		/* Collect search string */` |
|      9 | 6431 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 6432 | `	}else{` |
|      - | 6433 | `		/* Single pattern */` |
|  15506 | 6434 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  15506 | 6435 | `		if( nByte < 1 ){` |
|      - | 6436 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 6437 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 6438 | `			return PH7_OK;` |
|      - | 6439 | `		}` |
|  15502 | 6440 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6441 | `		/* Save for later processing */` |
|  15502 | 6442 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 6443 | `	}` |
|      - | 6444 | `	/* Replace string */` |
|  15510 | 6445 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 6446 | `		/* Collect replace string */` |
|      7 | 6447 | `		sRep.pCollector = &sReplace;` |
|      7 | 6448 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 6449 | `	}else{` |
|      - | 6450 | `		/* Single needle */` |
|  15504 | 6451 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  15504 | 6452 | `		rep_str = 1;` |
|  15504 | 6453 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6454 | `		/* Save for later processing */` |
|  15504 | 6455 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 6456 | `	}` |
|      - | 6457 | `	/* Reset loop cursors */` |
|  15510 | 6458 | `	SySetResetCursor(&sSearch);` |
|  15510 | 6459 | `	SySetResetCursor(&sReplace);` |
|  15510 | 6460 | `	pReplace = pSearch = 0; /* cc warning */` |
|  15510 | 6461 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 6462 | `	/* Extract function name */` |
|  15510 | 6463 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 6464 | `	/* Set the default pattern match routine */` |
|  15510 | 6465 | `	xMatch = SyBlobSearch;` |
|  15510 | 6466 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 6467 | `		/* Case insensitive pattern match */` |
|     11 | 6468 | `		xMatch = iPatternMatch;` |
|      5 | 6469 | `	}` |
|      - | 6470 | `	/* Start the replace process */` |
|  31026 | 6471 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 6472 | `		sxu32 nCount,nOfft;` |
|  15518 | 6473 | `		if( pSearch->nByte <  1 ){` |
|      - | 6474 | `			/* Empty string,ignore */` |
|      3 | 6475 | `			continue;` |
|      - | 6476 | `		}` |
|      - | 6477 | `		/* Extract the replace string */` |
|  15516 | 6478 | `		if( rep_str ){` |
|  15506 | 6479 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|   7754 | 6480 | `		}else{` |
|     11 | 6481 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 6482 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 6483 | `				 * An empty string is used for the rest of replacement values` |
|      - | 6484 | `				 */` |
|      3 | 6485 | `				pReplace = 0;` |
|      1 | 6486 | `			}` |
|      - | 6487 | `		}` |
|  15516 | 6488 | `		if( pReplace == 0 ){` |
|      - | 6489 | `			/* Use an empty string instead */` |
|      3 | 6490 | `			pReplace = &sTemp;` |
|      1 | 6491 | `		}` |
|  15516 | 6492 | `		nOfft = nCount = 0;` |
|   7773 | 6493 | `		for(;;){` |
|  15548 | 6494 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 6495 | `				break;` |
|      - | 6496 | `			}` |
|      - | 6497 | `			/* Perform a pattern lookup */` |
|  23303 | 6498 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  15534 | 6499 | `				pSearch->nByte,&nOfft);` |
|  15536 | 6500 | `			if( rc != SXRET_OK ){` |
|      - | 6501 | `				/* Pattern not found */` |
|  15504 | 6502 | `				break;` |
|      - | 6503 | `			}` |
|      - | 6504 | `			/* Perform the replace operation */` |
|     33 | 6505 | `			StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|      - | 6506 | `			/* Increment offset counter */` |
|     33 | 6507 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 6508 | `		}` |
|      2 | 6509 | `	}` |
|      - | 6510 | `	/* All done,clean-up the mess left behind */` |
|  15510 | 6511 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  15510 | 6512 | `	SySetRelease(&sSearch);` |
|  15510 | 6513 | `	SySetRelease(&sReplace);` |
|  15510 | 6514 | `	SyBlobRelease(&sWorker);` |
|  15510 | 6515 | `	return PH7_OK;` |
|   7779 | 6516 |  |
|      - | 6517 | `/*` |
|      - | 6518 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 6519 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 6520 | ` *  Translate characters or replace substrings.` |
|      - | 6521 | ` * Parameters` |
|      - | 6522 | ` *  $str` |
|      - | 6523 | ` *  The string being translated.` |
|      - | 6524 | ` * $from` |
|      - | 6525 | ` *  The string being translated to to.` |
|      - | 6526 | ` * $to` |
|      - | 6527 | ` *  The string replacing from.` |
|      - | 6528 | ` * $replace_pairs` |
|      - | 6529 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 6530 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 6531 | ` * Return` |
|      - | 6532 | ` *  The translated string.` |
|      - | 6533 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 6534 | ` */` |
|     12 | 6535 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6536 |  |
|      - | 6537 | `	const char *zIn;` |
|      - | 6538 | `	int nLen;` |
|     13 | 6539 | `	if( nArg < 1 ){` |
|      - | 6540 | `		/* Nothing to replace,return FALSE */` |
|      7 | 6541 | `		ph7_result_bool(pCtx,0);` |
|      7 | 6542 | `		return PH7_OK;` |
|      - | 6543 | `	}` |
|      7 | 6544 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 6545 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 6546 | `		/* Invalid arguments */` |
|    ! 0 | 6547 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6548 | `		return PH7_OK;` |
|      - | 6549 | `	}` |
|      9 | 6550 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 6551 | `		str_replace_data sRepData;` |
|      - | 6552 | `		SyBlob sWorker;` |
|      - | 6553 | `		/* Initilaize the working buffer */` |
|      5 | 6554 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|      - | 6555 | `		/* Copy raw string */` |
|      5 | 6556 | `		SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nLen);` |
|      - | 6557 | `		/* Init our replace data instance */` |
|      5 | 6558 | `		sRepData.pWorker = &sWorker;` |
|      5 | 6559 | `		sRepData.xMatch = SyBlobSearch;` |
|      - | 6560 | `		/* Iterate throw array entries and perform the replace operation.*/` |
|      5 | 6561 | `		ph7_array_walk(apArg[1],StringReplaceWalker,&sRepData);` |
|      - | 6562 | `		/* All done, return the result string */` |
|      7 | 6563 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|      4 | 6564 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 6565 | `		/* Clean-up */` |
|      5 | 6566 | `		SyBlobRelease(&sWorker);` |
|      3 | 6567 | `	}else{` |
|      - | 6568 | `		int i,flen,tlen,c,iOfft;` |
|      - | 6569 | `		const char *zFrom,*zTo;` |
|      3 | 6570 | `		if( nArg < 3 ){` |
|      - | 6571 | `			/* Nothing to replace */` |
|    ! 0 | 6572 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6573 | `			return PH7_OK;` |
|      - | 6574 | `		}` |
|      - | 6575 | `		/* Extract given arguments */` |
|      3 | 6576 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 6577 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 6578 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 6579 | `			/* Nothing to replace */` |
|    ! 0 | 6580 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6581 | `			return PH7_OK;` |
|      - | 6582 | `		}` |
|      - | 6583 | `		/* Start the replace process */` |
|     13 | 6584 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 6585 | `			c = zIn[i];` |
|     11 | 6586 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 6587 | `				if ( iOfft < tlen ){` |
|      5 | 6588 | `					c = zTo[iOfft];` |
|      2 | 6589 | `				}` |
|      2 | 6590 | `			}` |
|     11 | 6591 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 6592 |  |
|      6 | 6593 | `		}` |
|      - | 6594 | `	}` |
|      7 | 6595 | `	return PH7_OK;` |
|      7 | 6596 |  |
|      - | 6597 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6598 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6599 | `/*` |
|      - | 6600 | ` * Parse an INI string.` |
|      - | 6601 |  |
|      - | 6602 | ` * According to wikipedia` |
|      - | 6603 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 6604 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 6605 | ` *  Format` |
|      - | 6606 | `*    Properties` |
|      - | 6607 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 6608 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 6609 | `*     Example:` |
|      - | 6610 | `*      name=value` |
|      - | 6611 | `*    Sections` |
|      - | 6612 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 6613 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 6614 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 6615 | `*     or the end of the file. Sections may not be nested.` |
|      - | 6616 | `*     Example:` |
|      - | 6617 | `*      [section]` |
|      - | 6618 | `*   Comments` |
|      - | 6619 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 6620 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 6621 | `*/` |
|     12 | 6622 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 6623 |  |
|      - | 6624 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 6625 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 6626 | `	SyHashEntry *pEntry;` |
|      - | 6627 | `	SyString sEntry;` |
|      - | 6628 | `	SyHash sHash;` |
|      - | 6629 | `	int c;` |
|      - | 6630 | `	/* Create an empty array and worker variables */` |
|     13 | 6631 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 6632 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 6633 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 6634 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 6635 | `		/* Out of memory */` |
|    ! 0 | 6636 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      - | 6637 | `		/* Return FALSE */` |
|    ! 0 | 6638 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6639 | `		return PH7_OK;` |
|      - | 6640 | `	}` |
|     13 | 6641 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 6642 | `	pCur = pArray;` |
|      - | 6643 | `	/* Start the parse process */` |
|     21 | 6644 | `	for(;;){` |
|      - | 6645 | `		/* Ignore leading white spaces */` |
|     69 | 6646 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 6647 | `			zIn++;` |
|      1 | 6648 | `		}` |
|     43 | 6649 | `		if( zIn >= zEnd ){` |
|      - | 6650 | `			/* No more input to process */` |
|     13 | 6651 | `			break;` |
|      - | 6652 | `		}` |
|     31 | 6653 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6654 | `			/* Comment til the end of line */` |
|    ! 0 | 6655 | `			zIn++;` |
|    ! 0 | 6656 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 6657 | `				zIn++;` |
|    ! 0 | 6658 | `			}` |
|    ! 0 | 6659 | `			continue;` |
|      - | 6660 | `		}` |
|      - | 6661 | `		/* Reset the string cursor of the working variable */` |
|     31 | 6662 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 6663 | `		if( zIn[0] == '[' ){` |
|      - | 6664 | `			/* Section: Extract the section name */` |
|      9 | 6665 | `			zIn++;` |
|      9 | 6666 | `			zCur = zIn;` |
|     73 | 6667 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 6668 | `				zIn++;` |
|      1 | 6669 | `			}` |
|      9 | 6670 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 6671 | `				/* Save the section name */` |
|      5 | 6672 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 6673 | `				SyStringFullTrim(&sEntry);` |
|      5 | 6674 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 6675 | `				if( sEntry.nByte > 0 ){` |
|      - | 6676 | `					/* Associate an array with the section */` |
|      5 | 6677 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 6678 | `					if( pSection ){` |
|      5 | 6679 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 6680 | `						pCur = pSection;` |
|      2 | 6681 | `					}` |
|      2 | 6682 | `				}` |
|      2 | 6683 | `			}` |
|      9 | 6684 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 6685 | `		}else{` |
|      - | 6686 | `			ph7_value *pOldCur;` |
|      - | 6687 | `			int is_array;` |
|      - | 6688 | `			int iLen;` |
|      - | 6689 | `			/* Properties */` |
|     23 | 6690 | `			is_array = 0;` |
|     23 | 6691 | `			zCur = zIn;` |
|     23 | 6692 | `			iLen = 0; /* cc warning */` |
|     23 | 6693 | `			pOldCur = pCur;` |
|    155 | 6694 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 6695 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 6696 | `					/* Array */` |
|    ! 0 | 6697 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 6698 | `					is_array = 1;` |
|    ! 0 | 6699 | `					if( iLen > 0 ){` |
|    ! 0 | 6700 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 6701 | `						/* Query the hashtable */` |
|    ! 0 | 6702 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 6703 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 6704 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 6705 | `						if( pEntry ){` |
|    ! 0 | 6706 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 6707 | `						}else{` |
|      - | 6708 | `							/* Create an empty array */` |
|    ! 0 | 6709 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 6710 | `							if( pvArr ){` |
|      - | 6711 | `								/* Save the entry */` |
|    ! 0 | 6712 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 6713 | `								/* Insert the entry */` |
|    ! 0 | 6714 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6715 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 6716 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 6717 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6718 | `							}` |
|      - | 6719 | `						}` |
|    ! 0 | 6720 | `						if( pvArr ){` |
|    ! 0 | 6721 | `							pCur = pvArr;` |
|    ! 0 | 6722 | `						}` |
|    ! 0 | 6723 | `					}` |
|    ! 0 | 6724 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 6725 | `						zIn++;` |
|    ! 0 | 6726 | `					}` |
|    ! 0 | 6727 | `				}` |
|    133 | 6728 | `				zIn++;` |
|      1 | 6729 | `			}` |
|     23 | 6730 | `			if( !is_array ){` |
|     23 | 6731 | `				iLen = (int)(zIn-zCur);` |
|     11 | 6732 | `			}` |
|      - | 6733 | `			/* Trim the key */` |
|     23 | 6734 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 6735 | `			SyStringFullTrim(&sEntry);` |
|     23 | 6736 | `			if( sEntry.nByte > 0 ){` |
|     23 | 6737 | `				if( !is_array ){` |
|      - | 6738 | `					/* Save the key name */` |
|     23 | 6739 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 6740 | `				}` |
|      - | 6741 | `				/* extract key value */` |
|     23 | 6742 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 6743 | `				zIn++; /* '=' */` |
|     39 | 6744 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 6745 | `					zIn++;` |
|      1 | 6746 | `				}` |
|     23 | 6747 | `				if( zIn < zEnd ){` |
|     21 | 6748 | `					zCur = zIn;` |
|     21 | 6749 | `					c = zIn[0];` |
|     21 | 6750 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6751 | `						zIn++;` |
|      - | 6752 | `						/* Delimit the value */` |
|    ! 0 | 6753 | `						while( zIn < zEnd ){` |
|    ! 0 | 6754 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 6755 | `								break;` |
|      - | 6756 | `							}` |
|    ! 0 | 6757 | `							zIn++;` |
|    ! 0 | 6758 | `						}` |
|    ! 0 | 6759 | `						if( zIn < zEnd ){` |
|    ! 0 | 6760 | `							zIn++;` |
|    ! 0 | 6761 | `						}` |
|    ! 0 | 6762 | `					}else{` |
|    125 | 6763 | `						while( zIn < zEnd ){` |
|    123 | 6764 | `							if( zIn[0] == '\n' ){` |
|     19 | 6765 | `								if( zIn[-1] != '\\' ){` |
|     19 | 6766 | `									break;` |
|    ! 0 | 6767 | `								}` |
|    105 | 6768 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6769 | `								/* Inline comments */` |
|    ! 0 | 6770 | `								break;` |
|      - | 6771 | `							}` |
|    105 | 6772 | `							zIn++;` |
|      1 | 6773 | `						}` |
|      - | 6774 | `					}` |
|      - | 6775 | `					/* Trim the value */` |
|     21 | 6776 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 6777 | `					SyStringFullTrim(&sEntry);` |
|     21 | 6778 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6779 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 6780 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 6781 | `					}` |
|     21 | 6782 | `					if( sEntry.nByte > 0 ){` |
|     21 | 6783 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 6784 | `					}` |
|      - | 6785 | `					/* Insert the key and it's value */` |
|     21 | 6786 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 6787 | `				}` |
|     12 | 6788 | `			}else{` |
|    ! 0 | 6789 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 6790 | `					zIn++;` |
|    ! 0 | 6791 | `				}` |
|      - | 6792 | `			}` |
|     23 | 6793 | `			pCur = pOldCur;` |
|      - | 6794 | `		}` |
|      1 | 6795 | `	}` |
|     13 | 6796 | `	SyHashRelease(&sHash);` |
|      - | 6797 | `	/* Return the parse of the INI string */` |
|     13 | 6798 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 6799 | `	return SXRET_OK;` |
|      7 | 6800 |  |
|      - | 6801 | `/*` |
|      - | 6802 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 6803 | ` *  Parse a configuration string.` |
|      - | 6804 | ` * Parameters` |
|      - | 6805 | ` *  $ini` |
|      - | 6806 | ` *   The contents of the ini file being parsed.` |
|      - | 6807 | ` *  $process_sections` |
|      - | 6808 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 6809 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 6810 | ` *  $scanner_mode (Not used)` |
|      - | 6811 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 6812 | ` *   then option values will not be parsed.` |
|      - | 6813 | ` * Return` |
|      - | 6814 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 6815 | ` */` |
|     10 | 6816 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6817 |  |
|      - | 6818 | `	const char *zIni;` |
|      - | 6819 | `	int nByte;` |
|     11 | 6820 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6821 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 6822 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6823 | `		return PH7_OK;` |
|      - | 6824 | `	}` |
|      - | 6825 | `	/* Extract the raw INI buffer */` |
|     11 | 6826 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 6827 | `	/* Process the INI buffer*/` |
|     11 | 6828 | `	PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|     11 | 6829 | `	return PH7_OK;` |
|      6 | 6830 |  |
|      - | 6831 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6832 |  |
|      - | 6833 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6834 |  |
|      - | 6835 | `/*` |
|      - | 6836 | ` * Ctype Functions.` |
|      - | 6837 | ` * Status:` |
|      - | 6838 | ` *    Stable.` |
|      - | 6839 | ` */` |
|      - | 6840 | `/*` |
|      - | 6841 | ` * bool ctype_alnum(string $text)` |
|      - | 6842 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 6843 | ` * Parameters` |
|      - | 6844 | ` *  $text` |
|      - | 6845 | ` *   The tested string.` |
|      - | 6846 | ` * Return` |
|      - | 6847 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 6848 | ` */` |
|     16 | 6849 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6850 |  |
|      - | 6851 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6852 | `	int nLen;` |
|     17 | 6853 | `	if( nArg < 1 ){` |
|      - | 6854 | `		/* Missing arguments,return FALSE */` |
|      3 | 6855 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6856 | `		return PH7_OK;` |
|      - | 6857 | `	}` |
|      - | 6858 | `	/* Extract the target string */` |
|     15 | 6859 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 6860 | `	zEnd = &zIn[nLen];` |
|     15 | 6861 | `	if( nLen < 1 ){` |
|      - | 6862 | `		/* Empty string,return FALSE */` |
|      3 | 6863 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6864 | `		return PH7_OK;` |
|      - | 6865 | `	}` |
|      - | 6866 | `	/* Perform the requested operation */` |
|     32 | 6867 | `	for(;;){` |
|     65 | 6868 | `		if( zIn >= zEnd ){` |
|      - | 6869 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6870 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6871 | `			return PH7_OK;` |
|      - | 6872 | `		}` |
|     57 | 6873 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 6874 | `			break;` |
|      - | 6875 | `		}` |
|      - | 6876 | `		/* Point to the next character */` |
|     53 | 6877 | `		zIn++;` |
|      1 | 6878 | `	}` |
|      - | 6879 | `	/* The test failed,return FALSE */` |
|      5 | 6880 | `	ph7_result_bool(pCtx,0);` |
|      5 | 6881 | `	return PH7_OK;` |
|      9 | 6882 |  |
|      - | 6883 | `/*` |
|      - | 6884 | ` * bool ctype_alpha(string $text)` |
|      - | 6885 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 6886 | ` * Parameters` |
|      - | 6887 | ` *  $text` |
|      - | 6888 | ` *   The tested string.` |
|      - | 6889 | ` * Return` |
|      - | 6890 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 6891 | ` */` |
|     18 | 6892 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6893 |  |
|      - | 6894 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6895 | `	int nLen;` |
|     19 | 6896 | `	if( nArg < 1 ){` |
|      - | 6897 | `		/* Missing arguments,return FALSE */` |
|      3 | 6898 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6899 | `		return PH7_OK;` |
|      - | 6900 | `	}` |
|      - | 6901 | `	/* Extract the target string */` |
|     17 | 6902 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6903 | `	zEnd = &zIn[nLen];` |
|     17 | 6904 | `	if( nLen < 1 ){` |
|      - | 6905 | `		/* Empty string,return FALSE */` |
|      3 | 6906 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6907 | `		return PH7_OK;` |
|      - | 6908 | `	}` |
|      - | 6909 | `	/* Perform the requested operation */` |
|     42 | 6910 | `	for(;;){` |
|     85 | 6911 | `		if( zIn >= zEnd ){` |
|      - | 6912 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6913 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6914 | `			return PH7_OK;` |
|      - | 6915 | `		}` |
|     77 | 6916 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 6917 | `			break;` |
|      - | 6918 | `		}` |
|      - | 6919 | `		/* Point to the next character */` |
|     71 | 6920 | `		zIn++;` |
|      1 | 6921 | `	}` |
|      - | 6922 | `	/* The test failed,return FALSE */` |
|      7 | 6923 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6924 | `	return PH7_OK;` |
|     10 | 6925 |  |
|      - | 6926 | `/*` |
|      - | 6927 | ` * bool ctype_cntrl(string $text)` |
|      - | 6928 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 6929 | ` * Parameters` |
|      - | 6930 | ` *  $text` |
|      - | 6931 | ` *   The tested string.` |
|      - | 6932 | ` * Return` |
|      - | 6933 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 6934 | ` */` |
|     18 | 6935 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6936 |  |
|      - | 6937 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6938 | `	int nLen;` |
|     19 | 6939 | `	if( nArg < 1 ){` |
|      - | 6940 | `		/* Missing arguments,return FALSE */` |
|      3 | 6941 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6942 | `		return PH7_OK;` |
|      - | 6943 | `	}` |
|      - | 6944 | `	/* Extract the target string */` |
|     17 | 6945 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6946 | `	zEnd = &zIn[nLen];` |
|     17 | 6947 | `	if( nLen < 1 ){` |
|      - | 6948 | `		/* Empty string,return FALSE */` |
|      3 | 6949 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6950 | `		return PH7_OK;` |
|      - | 6951 | `	}` |
|      - | 6952 | `	/* Perform the requested operation */` |
|     14 | 6953 | `	for(;;){` |
|     29 | 6954 | `		if( zIn >= zEnd ){` |
|      - | 6955 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6956 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6957 | `			return PH7_OK;` |
|      - | 6958 | `		}` |
|     21 | 6959 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6960 | `			/* UTF-8 stream  */` |
|    ! 0 | 6961 | `			break;` |
|      - | 6962 | `		}` |
|     21 | 6963 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 6964 | `			break;` |
|      - | 6965 | `		}` |
|      - | 6966 | `		/* Point to the next character */` |
|     15 | 6967 | `		zIn++;` |
|      1 | 6968 | `	}` |
|      - | 6969 | `	/* The test failed,return FALSE */` |
|      7 | 6970 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6971 | `	return PH7_OK;` |
|     10 | 6972 |  |
|      - | 6973 | `/*` |
|      - | 6974 | ` * bool ctype_digit(string $text)` |
|      - | 6975 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 6976 | ` * Parameters` |
|      - | 6977 | ` *  $text` |
|      - | 6978 | ` *   The tested string.` |
|      - | 6979 | ` * Return` |
|      - | 6980 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 6981 | ` */` |
|   1804 | 6982 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6983 |  |
|      - | 6984 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6985 | `	int nLen;` |
|   1806 | 6986 | `	if( nArg < 1 ){` |
|      - | 6987 | `		/* Missing arguments,return FALSE */` |
|      3 | 6988 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6989 | `		return PH7_OK;` |
|      - | 6990 | `	}` |
|      - | 6991 | `	/* Extract the target string */` |
|   1804 | 6992 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1804 | 6993 | `	zEnd = &zIn[nLen];` |
|   1804 | 6994 | `	if( nLen < 1 ){` |
|      - | 6995 | `		/* Empty string,return FALSE */` |
|      3 | 6996 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6997 | `		return PH7_OK;` |
|      - | 6998 | `	}` |
|      - | 6999 | `	/* Perform the requested operation */` |
|   1662 | 7000 | `	for(;;){` |
|   3326 | 7001 | `		if( zIn >= zEnd ){` |
|      - | 7002 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1494 | 7003 | `			ph7_result_bool(pCtx,1);` |
|   1494 | 7004 | `			return PH7_OK;` |
|      - | 7005 | `		}` |
|   1834 | 7006 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7007 | `			/* UTF-8 stream  */` |
|    ! 0 | 7008 | `			break;` |
|      - | 7009 | `		}` |
|   1834 | 7010 | `		if( !SyisDigit(zIn[0]) ){` |
|    310 | 7011 | `			break;` |
|      - | 7012 | `		}` |
|      - | 7013 | `		/* Point to the next character */` |
|   1526 | 7014 | `		zIn++;` |
|      2 | 7015 | `	}` |
|      - | 7016 | `	/* The test failed,return FALSE */` |
|    310 | 7017 | `	ph7_result_bool(pCtx,0);` |
|    310 | 7018 | `	return PH7_OK;` |
|    904 | 7019 |  |
|      - | 7020 | `/*` |
|      - | 7021 | ` * bool ctype_xdigit(string $text)` |
|      - | 7022 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 7023 | ` * Parameters` |
|      - | 7024 | ` *  $text` |
|      - | 7025 | ` *   The tested string.` |
|      - | 7026 | ` * Return` |
|      - | 7027 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 7028 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 7029 | ` */` |
|     20 | 7030 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7031 |  |
|      - | 7032 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7033 | `	int nLen;` |
|     21 | 7034 | `	if( nArg < 1 ){` |
|      - | 7035 | `		/* Missing arguments,return FALSE */` |
|      3 | 7036 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7037 | `		return PH7_OK;` |
|      - | 7038 | `	}` |
|      - | 7039 | `	/* Extract the target string */` |
|     19 | 7040 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 7041 | `	zEnd = &zIn[nLen];` |
|     19 | 7042 | `	if( nLen < 1 ){` |
|      - | 7043 | `		/* Empty string,return FALSE */` |
|      3 | 7044 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7045 | `		return PH7_OK;` |
|      - | 7046 | `	}` |
|      - | 7047 | `	/* Perform the requested operation */` |
|     46 | 7048 | `	for(;;){` |
|     93 | 7049 | `		if( zIn >= zEnd ){` |
|      - | 7050 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 7051 | `			ph7_result_bool(pCtx,1);` |
|     11 | 7052 | `			return PH7_OK;` |
|      - | 7053 | `		}` |
|     83 | 7054 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7055 | `			/* UTF-8 stream  */` |
|    ! 0 | 7056 | `			break;` |
|      - | 7057 | `		}` |
|     83 | 7058 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 7059 | `			break;` |
|      - | 7060 | `		}` |
|      - | 7061 | `		/* Point to the next character */` |
|     77 | 7062 | `		zIn++;` |
|      1 | 7063 | `	}` |
|      - | 7064 | `	/* The test failed,return FALSE */` |
|      7 | 7065 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7066 | `	return PH7_OK;` |
|     11 | 7067 |  |
|      - | 7068 | `/*` |
|      - | 7069 | ` * bool ctype_graph(string $text)` |
|      - | 7070 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 7071 | ` * Parameters` |
|      - | 7072 | ` *  $text` |
|      - | 7073 | ` *   The tested string.` |
|      - | 7074 | ` * Return` |
|      - | 7075 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 7076 | ` * (no white space), FALSE otherwise.` |
|      - | 7077 | ` */` |
|     18 | 7078 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7079 |  |
|      - | 7080 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7081 | `	int nLen;` |
|     19 | 7082 | `	if( nArg < 1 ){` |
|      - | 7083 | `		/* Missing arguments,return FALSE */` |
|      3 | 7084 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7085 | `		return PH7_OK;` |
|      - | 7086 | `	}` |
|      - | 7087 | `	/* Extract the target string */` |
|     17 | 7088 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7089 | `	zEnd = &zIn[nLen];` |
|     17 | 7090 | `	if( nLen < 1 ){` |
|      - | 7091 | `		/* Empty string,return FALSE */` |
|      3 | 7092 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7093 | `		return PH7_OK;` |
|      - | 7094 | `	}` |
|      - | 7095 | `	/* Perform the requested operation */` |
|     57 | 7096 | `	for(;;){` |
|    115 | 7097 | `		if( zIn >= zEnd ){` |
|      - | 7098 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7099 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7100 | `			return PH7_OK;` |
|      - | 7101 | `		}` |
|    107 | 7102 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7103 | `			/* UTF-8 stream  */` |
|    ! 0 | 7104 | `			break;` |
|      - | 7105 | `		}` |
|    107 | 7106 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 7107 | `			break;` |
|      - | 7108 | `		}` |
|      - | 7109 | `		/* Point to the next character */` |
|    101 | 7110 | `		zIn++;` |
|      1 | 7111 | `	}` |
|      - | 7112 | `	/* The test failed,return FALSE */` |
|      7 | 7113 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7114 | `	return PH7_OK;` |
|     10 | 7115 |  |
|      - | 7116 | `/*` |
|      - | 7117 | ` * bool ctype_print(string $text)` |
|      - | 7118 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 7119 | ` * Parameters` |
|      - | 7120 | ` *  $text` |
|      - | 7121 | ` *   The tested string.` |
|      - | 7122 | ` * Return` |
|      - | 7123 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 7124 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 7125 | ` *  or control function at all.` |
|      - | 7126 | ` */` |
|     18 | 7127 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7128 |  |
|      - | 7129 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7130 | `	int nLen;` |
|     19 | 7131 | `	if( nArg < 1 ){` |
|      - | 7132 | `		/* Missing arguments,return FALSE */` |
|      3 | 7133 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7134 | `		return PH7_OK;` |
|      - | 7135 | `	}` |
|      - | 7136 | `	/* Extract the target string */` |
|     17 | 7137 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7138 | `	zEnd = &zIn[nLen];` |
|     17 | 7139 | `	if( nLen < 1 ){` |
|      - | 7140 | `		/* Empty string,return FALSE */` |
|      3 | 7141 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7142 | `		return PH7_OK;` |
|      - | 7143 | `	}` |
|      - | 7144 | `	/* Perform the requested operation */` |
|     63 | 7145 | `	for(;;){` |
|    127 | 7146 | `		if( zIn >= zEnd ){` |
|      - | 7147 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7148 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7149 | `			return PH7_OK;` |
|      - | 7150 | `		}` |
|    119 | 7151 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7152 | `			/* UTF-8 stream  */` |
|    ! 0 | 7153 | `			break;` |
|      - | 7154 | `		}` |
|    119 | 7155 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 7156 | `			break;` |
|      - | 7157 | `		}` |
|      - | 7158 | `		/* Point to the next character */` |
|    113 | 7159 | `		zIn++;` |
|      1 | 7160 | `	}` |
|      - | 7161 | `	/* The test failed,return FALSE */` |
|      7 | 7162 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7163 | `	return PH7_OK;` |
|     10 | 7164 |  |
|      - | 7165 | `/*` |
|      - | 7166 | ` * bool ctype_punct(string $text)` |
|      - | 7167 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 7168 | ` * Parameters` |
|      - | 7169 | ` *  $text` |
|      - | 7170 | ` *   The tested string.` |
|      - | 7171 | ` * Return` |
|      - | 7172 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 7173 | ` *  digit or blank, FALSE otherwise.` |
|      - | 7174 | ` */` |
|     20 | 7175 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7176 |  |
|      - | 7177 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7178 | `	int nLen;` |
|     21 | 7179 | `	if( nArg < 1 ){` |
|      - | 7180 | `		/* Missing arguments,return FALSE */` |
|      3 | 7181 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7182 | `		return PH7_OK;` |
|      - | 7183 | `	}` |
|      - | 7184 | `	/* Extract the target string */` |
|     19 | 7185 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 7186 | `	zEnd = &zIn[nLen];` |
|     19 | 7187 | `	if( nLen < 1 ){` |
|      - | 7188 | `		/* Empty string,return FALSE */` |
|      3 | 7189 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7190 | `		return PH7_OK;` |
|      - | 7191 | `	}` |
|      - | 7192 | `	/* Perform the requested operation */` |
|     38 | 7193 | `	for(;;){` |
|     77 | 7194 | `		if( zIn >= zEnd ){` |
|      - | 7195 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7196 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7197 | `			return PH7_OK;` |
|      - | 7198 | `		}` |
|     69 | 7199 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7200 | `			/* UTF-8 stream  */` |
|    ! 0 | 7201 | `			break;` |
|      - | 7202 | `		}` |
|     69 | 7203 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 7204 | `			break;` |
|      - | 7205 | `		}` |
|      - | 7206 | `		/* Point to the next character */` |
|     61 | 7207 | `		zIn++;` |
|      1 | 7208 | `	}` |
|      - | 7209 | `	/* The test failed,return FALSE */` |
|      9 | 7210 | `	ph7_result_bool(pCtx,0);` |
|      9 | 7211 | `	return PH7_OK;` |
|     11 | 7212 |  |
|      - | 7213 | `/*` |
|      - | 7214 | ` * bool ctype_space(string $text)` |
|      - | 7215 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 7216 | ` * Parameters` |
|      - | 7217 | ` *  $text` |
|      - | 7218 | ` *   The tested string.` |
|      - | 7219 | ` * Return` |
|      - | 7220 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 7221 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 7222 | ` *  and form feed characters.` |
|      - | 7223 | ` */` |
|  70000 | 7224 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 7225 |  |
|      - | 7226 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7227 | `	int nLen;` |
|  70002 | 7228 | `	if( nArg < 1 ){` |
|      - | 7229 | `		/* Missing arguments,return FALSE */` |
|      3 | 7230 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7231 | `		return PH7_OK;` |
|      - | 7232 | `	}` |
|      - | 7233 | `	/* Extract the target string */` |
|  70000 | 7234 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  70000 | 7235 | `	zEnd = &zIn[nLen];` |
|  70000 | 7236 | `	if( nLen < 1 ){` |
|      - | 7237 | `		/* Empty string,return FALSE */` |
|      3 | 7238 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7239 | `		return PH7_OK;` |
|      - | 7240 | `	}` |
|      - | 7241 | `	/* Perform the requested operation */` |
|  35688 | 7242 | `	for(;;){` |
|  71334 | 7243 | `		if( zIn >= zEnd ){` |
|      - | 7244 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1314 | 7245 | `			ph7_result_bool(pCtx,1);` |
|   1314 | 7246 | `			return PH7_OK;` |
|      - | 7247 | `		}` |
|  70022 | 7248 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7249 | `			/* UTF-8 stream  */` |
|    ! 0 | 7250 | `			break;` |
|      - | 7251 | `		}` |
|  70022 | 7252 | `		if( !SyisSpace(zIn[0]) ){` |
|  68686 | 7253 | `			break;` |
|      - | 7254 | `		}` |
|      - | 7255 | `		/* Point to the next character */` |
|   1338 | 7256 | `		zIn++;` |
|      2 | 7257 | `	}` |
|      - | 7258 | `	/* The test failed,return FALSE */` |
|  68686 | 7259 | `	ph7_result_bool(pCtx,0);` |
|  68686 | 7260 | `	return PH7_OK;` |
|  35024 | 7261 |  |
|      - | 7262 | `/*` |
|      - | 7263 | ` * bool ctype_lower(string $text)` |
|      - | 7264 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 7265 | ` * Parameters` |
|      - | 7266 | ` *  $text` |
|      - | 7267 | ` *   The tested string.` |
|      - | 7268 | ` * Return` |
|      - | 7269 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 7270 | ` */` |
|     18 | 7271 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7272 |  |
|      - | 7273 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7274 | `	int nLen;` |
|     19 | 7275 | `	if( nArg < 1 ){` |
|      - | 7276 | `		/* Missing arguments,return FALSE */` |
|      3 | 7277 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7278 | `		return PH7_OK;` |
|      - | 7279 | `	}` |
|      - | 7280 | `	/* Extract the target string */` |
|     17 | 7281 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7282 | `	zEnd = &zIn[nLen];` |
|     17 | 7283 | `	if( nLen < 1 ){` |
|      - | 7284 | `		/* Empty string,return FALSE */` |
|      3 | 7285 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7286 | `		return PH7_OK;` |
|      - | 7287 | `	}` |
|      - | 7288 | `	/* Perform the requested operation */` |
|     27 | 7289 | `	for(;;){` |
|     55 | 7290 | `		if( zIn >= zEnd ){` |
|      - | 7291 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7292 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7293 | `			return PH7_OK;` |
|      - | 7294 | `		}` |
|     51 | 7295 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 7296 | `			break;` |
|      - | 7297 | `		}` |
|      - | 7298 | `		/* Point to the next character */` |
|     41 | 7299 | `		zIn++;` |
|      1 | 7300 | `	}` |
|      - | 7301 | `	/* The test failed,return FALSE */` |
|     11 | 7302 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7303 | `	return PH7_OK;` |
|     10 | 7304 |  |
|      - | 7305 | `/*` |
|      - | 7306 | ` * bool ctype_upper(string $text)` |
|      - | 7307 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 7308 | ` * Parameters` |
|      - | 7309 | ` *  $text` |
|      - | 7310 | ` *   The tested string.` |
|      - | 7311 | ` * Return` |
|      - | 7312 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 7313 | ` */` |
|     18 | 7314 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7315 |  |
|      - | 7316 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7317 | `	int nLen;` |
|     19 | 7318 | `	if( nArg < 1 ){` |
|      - | 7319 | `		/* Missing arguments,return FALSE */` |
|      3 | 7320 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7321 | `		return PH7_OK;` |
|      - | 7322 | `	}` |
|      - | 7323 | `	/* Extract the target string */` |
|     17 | 7324 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7325 | `	zEnd = &zIn[nLen];` |
|     17 | 7326 | `	if( nLen < 1 ){` |
|      - | 7327 | `		/* Empty string,return FALSE */` |
|      3 | 7328 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7329 | `		return PH7_OK;` |
|      - | 7330 | `	}` |
|      - | 7331 | `	/* Perform the requested operation */` |
|     28 | 7332 | `	for(;;){` |
|     57 | 7333 | `		if( zIn >= zEnd ){` |
|      - | 7334 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7335 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7336 | `			return PH7_OK;` |
|      - | 7337 | `		}` |
|     53 | 7338 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 7339 | `			break;` |
|      - | 7340 | `		}` |
|      - | 7341 | `		/* Point to the next character */` |
|     43 | 7342 | `		zIn++;` |
|      1 | 7343 | `	}` |
|      - | 7344 | `	/* The test failed,return FALSE */` |
|     11 | 7345 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7346 | `	return PH7_OK;` |
|     10 | 7347 |  |
|      - | 7348 | `/*` |
|      - | 7349 | ` * Date/Time functions` |
|      - | 7350 | ` * Status:` |
|      - | 7351 | ` *    Devel.` |
|      - | 7352 | ` */` |
|      - | 7353 | `#include <time.h>` |
|      - | 7354 | `#ifdef __WINNT__` |
|      - | 7355 | `/* GetSystemTime() */` |
|      - | 7356 | `#include <Windows.h>` |
|      - | 7357 | `#ifdef _WIN32_WCE` |
|      - | 7358 | `/*` |
|      - | 7359 | `** WindowsCE does not have a localtime() function.  So create a` |
|      - | 7360 | `** substitute.` |
|      - | 7361 | `** Taken from the SQLite3 source tree.` |
|      - | 7362 | `** Status: Public domain` |
|      - | 7363 | `*/` |
|      - | 7364 | `struct tm *__cdecl localtime(const time_t *t)` |
|      - | 7365 |  |
|      - | 7366 | `  static struct tm y;` |
|      - | 7367 | `  FILETIME uTm, lTm;` |
|      - | 7368 | `  SYSTEMTIME pTm;` |
|      - | 7369 | `  ph7_int64 t64;` |
|      - | 7370 | `  t64 = *t;` |
|      - | 7371 | `  t64 = (t64 + 11644473600)*10000000;` |
|      - | 7372 | `  uTm.dwLowDateTime = (DWORD)(t64 & 0xFFFFFFFF);` |
|      - | 7373 | `  uTm.dwHighDateTime= (DWORD)(t64 >> 32);` |
|      - | 7374 | `  FileTimeToLocalFileTime(&uTm,&lTm);` |
|      - | 7375 | `  FileTimeToSystemTime(&lTm,&pTm);` |
|      - | 7376 | `  y.tm_year = pTm.wYear - 1900;` |
|      - | 7377 | `  y.tm_mon = pTm.wMonth - 1;` |
|      - | 7378 | `  y.tm_wday = pTm.wDayOfWeek;` |
|      - | 7379 | `  y.tm_mday = pTm.wDay;` |
|      - | 7380 | `  y.tm_hour = pTm.wHour;` |
|      - | 7381 | `  y.tm_min = pTm.wMinute;` |
|      - | 7382 | `  y.tm_sec = pTm.wSecond;` |
|      - | 7383 | `  return &y;` |
|      - | 7384 |  |
|      - | 7385 | `#endif /*_WIN32_WCE */` |
|      - | 7386 | `#elif defined(__UNIXES__)` |
|      - | 7387 | `#include <sys/time.h>` |
|      - | 7388 | `#endif /* __WINNT__*/` |
|      - | 7389 | ` /*` |
|      - | 7390 | `  * int64 time(void)` |
|      - | 7391 | `  *  Current Unix timestamp` |
|      - | 7392 | `  * Parameters` |
|      - | 7393 | `  *  None.` |
|      - | 7394 | `  * Return` |
|      - | 7395 | `  *  Returns the current time measured in the number of seconds` |
|      - | 7396 | `  *  since the Unix Epoch (January 1 1970 00:00:00 GMT).` |
|      - | 7397 | `  */` |
|      8 | 7398 | `static int PH7_builtin_time(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7399 |  |
|      - | 7400 | `	time_t tt;` |
|      4 | 7401 | `	SXUNUSED(nArg); /* cc warning */` |
|      4 | 7402 | `	SXUNUSED(apArg);` |
|      - | 7403 | `	/* Extract the current time */` |
|      9 | 7404 | `	time(&tt);` |
|      - | 7405 | `	/* Return as 64-bit integer */` |
|      9 | 7406 | `	ph7_result_int64(pCtx,(ph7_int64)tt);` |
|      9 | 7407 | `	return  PH7_OK;` |
|      1 | 7408 |  |
|      - | 7409 | `/*` |
|      - | 7410 | `  * string/float microtime([ bool $get_as_float = false ])` |
|      - | 7411 | `  *  microtime() returns the current Unix timestamp with microseconds.` |
|      - | 7412 | `  * Parameters` |
|      - | 7413 | `  *  $get_as_float` |
|      - | 7414 | `  *   If used and set to TRUE, microtime() will return a float instead of a string` |
|      - | 7415 | `  *   as described in the return values section below.` |
|      - | 7416 | `  * Return` |
|      - | 7417 | `  *  By default, microtime() returns a string in the form "msec sec", where sec` |
|      - | 7418 | `  *  is the current time measured in the number of seconds since the Unix` |
|      - | 7419 | `  *  epoch (0:00:00 January 1, 1970 GMT), and msec is the number of microseconds` |
|      - | 7420 | `  *  that have elapsed since sec expressed in seconds.` |
|      - | 7421 | `  *  If get_as_float is set to TRUE, then microtime() returns a float, which represents` |
|      - | 7422 | `  *  the current time in seconds since the Unix epoch accurate to the nearest microsecond.` |
|      - | 7423 | `  */` |
|     20 | 7424 | `static int PH7_builtin_microtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7425 |  |
|     21 | 7426 | `	int bFloat = 0;` |
|      - | 7427 | `	sytime sTime;` |
|      - | 7428 | `#if defined(__UNIXES__)` |
|      - | 7429 | `	struct timeval tv;` |
|     20 | 7430 | `	gettimeofday(&tv,0);` |
|     20 | 7431 | `	sTime.tm_sec  = (long)tv.tv_sec;` |
|     20 | 7432 | `	sTime.tm_usec = (long)tv.tv_usec;` |
|      - | 7433 | `#else` |
|      - | 7434 | `	time_t tt;` |
|      1 | 7435 | `	time(&tt);` |
|      1 | 7436 | `	sTime.tm_sec  = (long)tt;` |
|      1 | 7437 | `	sTime.tm_usec = (long)(tt%SX_USEC_PER_SEC);` |
|      - | 7438 | `#endif /* __UNIXES__ */` |
|     21 | 7439 | `	if( nArg > 0 ){` |
|     17 | 7440 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|      8 | 7441 | `	}` |
|     21 | 7442 | `	if( bFloat ){` |
|      - | 7443 | `		/* Return as float */` |
|     17 | 7444 | `		ph7_result_double(pCtx,(double)sTime.tm_sec);` |
|      9 | 7445 | `	}else{` |
|      - | 7446 | `		/* Return as string */` |
|      5 | 7447 | `		ph7_result_string_format(pCtx,"%ld %ld",sTime.tm_usec,sTime.tm_sec);` |
|      - | 7448 | `	}` |
|     21 | 7449 | `	return PH7_OK;` |
|      1 | 7450 |  |
|      - | 7451 | `/*` |
|      - | 7452 | ` * array getdate ([ int $timestamp = time() ])` |
|      - | 7453 | ` *  Get date/time information.` |
|      - | 7454 | ` * Parameter` |
|      - | 7455 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|      - | 7456 | ` *     that defaults to the current local time if a timestamp is not given.` |
|      - | 7457 | ` *     In other words, it defaults to the value of time().` |
|      - | 7458 | ` * Returns` |
|      - | 7459 | ` *  Returns an associative array of information related to the timestamp.` |
|      - | 7460 | ` *  Elements from the returned associative array are as follows:` |
|      - | 7461 | ` *   KEY                                                         VALUE` |
|      - | 7462 | ` * ---------                                                    -------` |
|      - | 7463 | ` * "seconds" 	Numeric representation of seconds 	            0 to 59` |
|      - | 7464 | ` * "minutes" 	Numeric representation of minutes 	            0 to 59` |
|      - | 7465 | ` * "hours" 	    Numeric representation of hours 	            0 to 23` |
|      - | 7466 | ` * "mday" 	    Numeric representation of the day of the month 	1 to 31` |
|      - | 7467 | ` * "wday" 	    Numeric representation of the day of the week 	0 (for Sunday) through 6 (for Saturday)` |
|      - | 7468 | ` * "mon" 	    Numeric representation of a month 	            1 through 12` |
|      - | 7469 | ` * "year" 	    A full numeric representation of a year,        4 digits 	Examples: 1999 or 2003` |
|      - | 7470 | ` * "yday" 	    Numeric representation of the day of the year   0 through 365` |
|      - | 7471 | ` * "weekday" 	A full textual representation of the day of the week 	Sunday through Saturday` |
|      - | 7472 | ` * "month" 	    A full textual representation of a month, such as January or March 	January through December` |
|      - | 7473 | ` * 0 	        Seconds since the Unix Epoch, similar to the values returned by time() and used by date().` |
|      - | 7474 | ` * NOTE:` |
|      - | 7475 | ` *   NULL is returned on failure.` |
|      - | 7476 | ` */` |
|      8 | 7477 | `static int PH7_builtin_getdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7478 |  |
|      - | 7479 | `	ph7_value *pValue,*pArray;` |
|      - | 7480 | `	Sytm sTm;` |
|      9 | 7481 | `	if( nArg < 1 ){` |
|      - | 7482 | `#ifdef __WINNT__` |
|      - | 7483 | `		SYSTEMTIME sOS;` |
|      1 | 7484 | `		GetSystemTime(&sOS);` |
|      1 | 7485 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 7486 | `#else` |
|      - | 7487 | `		struct tm *pTm;` |
|      - | 7488 | `		time_t t;` |
|      4 | 7489 | `		time(&t);` |
|      4 | 7490 | `		pTm = localtime(&t);` |
|      4 | 7491 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7492 | `#endif` |
|      3 | 7493 | `	}else{` |
|      - | 7494 | `		/* Use the given timestamp */` |
|      - | 7495 | `		time_t t;` |
|      - | 7496 | `		struct tm *pTm;` |
|      - | 7497 | `#ifdef __WINNT__` |
|      - | 7498 | `#ifdef _MSC_VER` |
|      - | 7499 | `#if _MSC_VER >= 1400 /* Visual Studio 2005 and up */` |
|      - | 7500 | `#pragma warning(disable:4996) /* _CRT_SECURE...*/` |
|      - | 7501 | `#endif` |
|      - | 7502 | `#endif` |
|      - | 7503 | `#endif` |
|      5 | 7504 | `		if( ph7_value_is_int(apArg[0]) ){` |
|      5 | 7505 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|      5 | 7506 | `			pTm = localtime(&t);` |
|      5 | 7507 | `			if( pTm == 0 ){` |
|    ! 0 | 7508 | `				time(&t);` |
|    ! 0 | 7509 | `			}` |
|      3 | 7510 | `		}else{` |
|    ! 0 | 7511 | `			time(&t);` |
|      - | 7512 | `		}` |
|      5 | 7513 | `		pTm = localtime(&t);` |
|      5 | 7514 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7515 | `	}` |
|      - | 7516 | `	/* Element value */` |
|      9 | 7517 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      9 | 7518 | `	if( pValue == 0 ){` |
|      - | 7519 | `		/* Return NULL */` |
|    ! 0 | 7520 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7521 | `		return PH7_OK;` |
|      - | 7522 | `	}` |
|      - | 7523 | `	/* Create a new array */` |
|      9 | 7524 | `	pArray = ph7_context_new_array(pCtx);` |
|      9 | 7525 | `	if( pArray == 0 ){` |
|      - | 7526 | `		/* Return NULL */` |
|    ! 0 | 7527 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7528 | `		return PH7_OK;` |
|      - | 7529 | `	}` |
|      - | 7530 | `	/* Fill the array */` |
|      - | 7531 | `	/* Seconds */` |
|      9 | 7532 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|      9 | 7533 | `	ph7_array_add_strkey_elem(pArray,"seconds",pValue);` |
|      - | 7534 | `	/* Minutes */` |
|      9 | 7535 | `	ph7_value_int(pValue,sTm.tm_min);` |
|      9 | 7536 | `	ph7_array_add_strkey_elem(pArray,"minutes",pValue);` |
|      - | 7537 | `	/* Hours */` |
|      9 | 7538 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|      9 | 7539 | `	ph7_array_add_strkey_elem(pArray,"hours",pValue);` |
|      - | 7540 | `	/* mday */` |
|      9 | 7541 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|      9 | 7542 | `	ph7_array_add_strkey_elem(pArray,"mday",pValue);` |
|      - | 7543 | `	/* wday */` |
|      9 | 7544 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|      9 | 7545 | `	ph7_array_add_strkey_elem(pArray,"wday",pValue);` |
|      - | 7546 | `	/* mon */` |
|      9 | 7547 | `	ph7_value_int(pValue,sTm.tm_mon+1);` |
|      9 | 7548 | `	ph7_array_add_strkey_elem(pArray,"mon",pValue);` |
|      - | 7549 | `	/* year */` |
|      9 | 7550 | `	ph7_value_int(pValue,sTm.tm_year);` |
|      9 | 7551 | `	ph7_array_add_strkey_elem(pArray,"year",pValue);` |
|      - | 7552 | `	/* yday */` |
|      9 | 7553 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|      9 | 7554 | `	ph7_array_add_strkey_elem(pArray,"yday",pValue);` |
|      - | 7555 | `	/* Weekday */` |
|      9 | 7556 | `	ph7_value_string(pValue,SyTimeGetDay(sTm.tm_wday),-1);` |
|      9 | 7557 | `	ph7_array_add_strkey_elem(pArray,"weekday",pValue);` |
|      - | 7558 | `	/* Month */` |
|      9 | 7559 | `	ph7_value_reset_string_cursor(pValue);` |
|      9 | 7560 | `	ph7_value_string(pValue,SyTimeGetMonth(sTm.tm_mon),-1);` |
|      9 | 7561 | `	ph7_array_add_strkey_elem(pArray,"month",pValue);` |
|      - | 7562 | `	/* Seconds since the epoch */` |
|      9 | 7563 | `	ph7_value_int64(pValue,(ph7_int64)time(0));` |
|      9 | 7564 | `	ph7_array_add_intkey_elem(pArray,0 /* Index zero */,pValue);` |
|      - | 7565 | `	/* Return the freshly created array */` |
|      9 | 7566 | `	ph7_result_value(pCtx,pArray);` |
|      9 | 7567 | `	return PH7_OK;` |
|      5 | 7568 |  |
|      - | 7569 | `/*` |
|      - | 7570 | ` * mixed gettimeofday([ bool $return_float = false ] )` |
|      - | 7571 | ` *   Returns an associative array containing the data returned from the system call.` |
|      - | 7572 | ` * Parameters` |
|      - | 7573 | ` *  $return_float` |
|      - | 7574 | ` *   When set to TRUE, a float instead of an array is returned.` |
|      - | 7575 | ` * Return` |
|      - | 7576 | ` *   By default an array is returned. If return_float is set, then` |
|      - | 7577 | ` *   a float is returned.` |
|      - | 7578 | ` */` |
|      4 | 7579 | `static int PH7_builtin_gettimeofday(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7580 |  |
|      5 | 7581 | `	int bFloat = 0;` |
|      - | 7582 | `	sytime sTime;` |
|      - | 7583 | `#if defined(__UNIXES__)` |
|      - | 7584 | `	struct timeval tv;` |
|      4 | 7585 | `	gettimeofday(&tv,0);` |
|      4 | 7586 | `	sTime.tm_sec  = (long)tv.tv_sec;` |
|      4 | 7587 | `	sTime.tm_usec = (long)tv.tv_usec;` |
|      - | 7588 | `#else` |
|      - | 7589 | `	time_t tt;` |
|      1 | 7590 | `	time(&tt);` |
|      1 | 7591 | `	sTime.tm_sec  = (long)tt;` |
|      1 | 7592 | `	sTime.tm_usec = (long)(tt%SX_USEC_PER_SEC);` |
|      - | 7593 | `#endif /* __UNIXES__ */` |
|      5 | 7594 | `	if( nArg > 0 ){` |
|      5 | 7595 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|      2 | 7596 | `	}` |
|      5 | 7597 | `	if( bFloat ){` |
|      - | 7598 | `		/* Return as float */` |
|      3 | 7599 | `		ph7_result_double(pCtx,(double)sTime.tm_sec);` |
|      2 | 7600 | `	}else{` |
|      - | 7601 | `		/* Return an associative array */` |
|      - | 7602 | `		ph7_value *pValue,*pArray;` |
|      - | 7603 | `		/* Create a new array */` |
|      3 | 7604 | `		pArray = ph7_context_new_array(pCtx);` |
|      - | 7605 | `		/* Element value */` |
|      3 | 7606 | `		pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 7607 | `		if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 7608 | `			/* Return NULL */` |
|    ! 0 | 7609 | `			ph7_result_null(pCtx);` |
|    ! 0 | 7610 | `			return PH7_OK;` |
|      - | 7611 | `		}` |
|      - | 7612 | `		/* Fill the array */` |
|      - | 7613 | `		/* sec */` |
|      3 | 7614 | `		ph7_value_int64(pValue,sTime.tm_sec);` |
|      3 | 7615 | `		ph7_array_add_strkey_elem(pArray,"sec",pValue);` |
|      - | 7616 | `		/* usec */` |
|      3 | 7617 | `		ph7_value_int64(pValue,sTime.tm_usec);` |
|      3 | 7618 | `		ph7_array_add_strkey_elem(pArray,"usec",pValue);` |
|      - | 7619 | `		/* Return the array */` |
|      3 | 7620 | `		ph7_result_value(pCtx,pArray);` |
|      - | 7621 | `	}` |
|      5 | 7622 | `	return PH7_OK;` |
|      3 | 7623 |  |
|      - | 7624 | `/* Check if the given year is leap or not */` |
|      - | 7625 | `#define IS_LEAP_YEAR(YEAR)	(YEAR % 400 ? ( YEAR % 100 ? ( YEAR % 4 ? 0 : 1 ) : 0 ) : 1)` |
|      - | 7626 | `/* ISO-8601 numeric representation of the day of the week */` |
|      - | 7627 | `static const int aISO8601[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|      - | 7628 | `/*` |
|      - | 7629 | ` * Format a given date string.` |
|      - | 7630 | ` * Supported format: (Taken from PHP online docs)` |
|      - | 7631 | ` * character 	Description` |
|      - | 7632 | ` * d          Day of the month` |
|      - | 7633 | ` * D          A textual representation of a days` |
|      - | 7634 | ` * j          Day of the month without leading zeros` |
|      - | 7635 | ` * l          A full textual representation of the day of the week` |
|      - | 7636 | ` * N          ISO-8601 numeric representation of the day of the week` |
|      - | 7637 | ` * w          Numeric representation of the day of the week` |
|      - | 7638 | ` * z          The day of the year (starting from 0)` |
|      - | 7639 | ` * F          A full textual representation of a month, such as January or March` |
|      - | 7640 | ` * m          Numeric representation of a month, with leading zeros 	01 through 12` |
|      - | 7641 | ` * M          A short textual representation of a month, three letters 	Jan through Dec` |
|      - | 7642 | ` * n          Numeric representation of a month, without leading zeros 	1 through 12` |
|      - | 7643 | ` * t          Number of days in the given month 	28 through 31` |
|      - | 7644 | ` * L          Whether it's a leap year 	1 if it is a leap year, 0 otherwise.` |
|      - | 7645 | ` * o          ISO-8601 year number. This has the same value as Y, except that if the ISO week number` |
|      - | 7646 | ` *            (W) belongs to the previous or next year, that year is used instead. (added in PHP 5.1.0) Examples: 1999 or 2003` |
|      - | 7647 | ` * Y          A full numeric representation of a year, 4 digits 	Examples: 1999 or 2003` |
|      - | 7648 | ` * y          A two digit representation of a year 	Examples: 99 or 03` |
|      - | 7649 | ` * a          Lowercase Ante meridiem and Post meridiem 	am or pm` |
|      - | 7650 | ` * A          Uppercase Ante meridiem and Post meridiem 	AM or PM` |
|      - | 7651 | ` * g          12-hour format of an hour without leading zeros 	1 through 12` |
|      - | 7652 | ` * G          24-hour format of an hour without leading zeros 	0 through 23` |
|      - | 7653 | ` * h          12-hour format of an hour with leading zeros 	01 through 12` |
|      - | 7654 | ` * H          24-hour format of an hour with leading zeros 	00 through 23` |
|      - | 7655 | ` * i          Minutes with leading zeros 	00 to 59` |
|      - | 7656 | ` * s          Seconds, with leading zeros 	00 through 59` |
|      - | 7657 | ` * u          Microseconds Example: 654321` |
|      - | 7658 | ` * e          Timezone identifier 	Examples: UTC, GMT, Atlantic/Azores` |
|      - | 7659 | ` * I          (capital i) Whether or not the date is in daylight saving time 	1 if Daylight Saving Time, 0 otherwise.` |
|      - | 7660 | ` * r          RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 +0200` |
|      - | 7661 | ` * U          Seconds since the Unix Epoch (January 1 1970 00:00:00 GMT)` |
|      - | 7662 | ` * S          English ordinal suffix for the day of the month, 2 characters` |
|      - | 7663 | ` * O          Difference to Greenwich time (GMT) in hours` |
|      - | 7664 | ` * Z          Timezone offset in seconds. The offset for timezones west of UTC is always negative, and for those` |
|      - | 7665 | ` *            east of UTC is always positive.` |
|      - | 7666 | ` * c         ISO 8601 date` |
|      - | 7667 | ` */` |
|     46 | 7668 | `static sxi32 DateFormat(ph7_context *pCtx,const char *zIn,int nLen,Sytm *pTm)` |
|      1 | 7669 |  |
|     47 | 7670 | `	const char *zEnd = &zIn[nLen];` |
|      - | 7671 | `	const char *zCur;` |
|      - | 7672 | `	/* Start the format process */` |
|     78 | 7673 | `	for(;;){` |
|    157 | 7674 | `		if( zIn >= zEnd ){` |
|      - | 7675 | `			/* No more input to process */` |
|     47 | 7676 | `			break;` |
|      - | 7677 | `		}` |
|    111 | 7678 | `		switch(zIn[0]){` |
|      7 | 7679 | `		case 'd':` |
|      - | 7680 | `			/* Day of the month, 2 digits with leading zeros */` |
|     15 | 7681 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mday);` |
|     15 | 7682 | `			break;` |
|    ! 0 | 7683 | `		case 'D':` |
|      - | 7684 | `			/*A textual representation of a day, three letters*/` |
|    ! 0 | 7685 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|    ! 0 | 7686 | `			ph7_result_string(pCtx,zCur,3);` |
|    ! 0 | 7687 | `			break;` |
|    ! 0 | 7688 | `		case 'j':` |
|      - | 7689 | `			/*	Day of the month without leading zeros */` |
|    ! 0 | 7690 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mday);` |
|    ! 0 | 7691 | `			break;` |
|      2 | 7692 | `		case 'l':` |
|      - | 7693 | `			/* A full textual representation of the day of the week */` |
|      5 | 7694 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|      5 | 7695 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|      5 | 7696 | `			break;` |
|    ! 0 | 7697 | `		case 'N':{` |
|      - | 7698 | `			/* ISO-8601 numeric representation of the day of the week */` |
|    ! 0 | 7699 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|    ! 0 | 7700 | `			break;` |
|      - | 7701 | `				 }` |
|    ! 0 | 7702 | `		case 'w':` |
|      - | 7703 | `			/*Numeric representation of the day of the week*/` |
|    ! 0 | 7704 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|    ! 0 | 7705 | `			break;` |
|    ! 0 | 7706 | `		case 'z':` |
|      - | 7707 | `			/*The day of the year*/` |
|    ! 0 | 7708 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_yday);` |
|    ! 0 | 7709 | `			break;` |
|      2 | 7710 | `		case 'F':` |
|      - | 7711 | `			/*A full textual representation of a month, such as January or March*/` |
|      5 | 7712 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|      5 | 7713 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|      5 | 7714 | `			break;` |
|      7 | 7715 | `		case 'm':` |
|      - | 7716 | `			/*Numeric representation of a month, with leading zeros*/` |
|     15 | 7717 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|     15 | 7718 | `			break;` |
|    ! 0 | 7719 | `		case 'M':` |
|      - | 7720 | `			/*A short textual representation of a month, three letters*/` |
|    ! 0 | 7721 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|    ! 0 | 7722 | `			ph7_result_string(pCtx,zCur,3);` |
|    ! 0 | 7723 | `			break;` |
|    ! 0 | 7724 | `		case 'n':` |
|      - | 7725 | `			/*Numeric representation of a month, without leading zeros*/` |
|    ! 0 | 7726 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mon + 1);` |
|    ! 0 | 7727 | `			break;` |
|    ! 0 | 7728 | `		case 't':{` |
|      - | 7729 | `			static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|    ! 0 | 7730 | `			int nDays = aMonDays[pTm->tm_mon % 12 ];` |
|    ! 0 | 7731 | `			if( pTm->tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(pTm->tm_year) ){` |
|    ! 0 | 7732 | `				nDays = 28;` |
|    ! 0 | 7733 | `			}` |
|      - | 7734 | `			/*Number of days in the given month*/` |
|    ! 0 | 7735 | `			ph7_result_string_format(pCtx,"%d",nDays);` |
|    ! 0 | 7736 | `			break;` |
|      - | 7737 | `				 }` |
|    ! 0 | 7738 | `		case 'L':{` |
|    ! 0 | 7739 | `			int isLeap = IS_LEAP_YEAR(pTm->tm_year);` |
|      - | 7740 | `			/* Whether it's a leap year */` |
|    ! 0 | 7741 | `			ph7_result_string_format(pCtx,"%d",isLeap);` |
|    ! 0 | 7742 | `			break;` |
|      - | 7743 | `				 }` |
|    ! 0 | 7744 | `		case 'o':` |
|      - | 7745 | `			/* ISO-8601 year number.*/` |
|    ! 0 | 7746 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|    ! 0 | 7747 | `			break;` |
|      9 | 7748 | `		case 'Y':` |
|      - | 7749 | `			/*	A full numeric representation of a year, 4 digits */` |
|     19 | 7750 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|     19 | 7751 | `			break;` |
|    ! 0 | 7752 | `		case 'y':` |
|      - | 7753 | `			/*A two digit representation of a year*/` |
|    ! 0 | 7754 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_year%100);` |
|    ! 0 | 7755 | `			break;` |
|    ! 0 | 7756 | `		case 'a':` |
|      - | 7757 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7758 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",2);` |
|    ! 0 | 7759 | `			break;` |
|    ! 0 | 7760 | `		case 'A':` |
|      - | 7761 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7762 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",2);` |
|    ! 0 | 7763 | `			break;` |
|    ! 0 | 7764 | `		case 'g':` |
|      - | 7765 | `			/*	12-hour format of an hour without leading zeros*/` |
|    ! 0 | 7766 | `			ph7_result_string_format(pCtx,"%d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7767 | `			break;` |
|    ! 0 | 7768 | `		case 'G':` |
|      - | 7769 | `			/* 24-hour format of an hour without leading zeros */` |
|    ! 0 | 7770 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_hour);` |
|    ! 0 | 7771 | `			break;` |
|    ! 0 | 7772 | `		case 'h':` |
|      - | 7773 | `			/* 12-hour format of an hour with leading zeros */` |
|    ! 0 | 7774 | `			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7775 | `			break;` |
|      3 | 7776 | `		case 'H':` |
|      - | 7777 | `			/*	24-hour format of an hour with leading zeros */` |
|      7 | 7778 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|      7 | 7779 | `			break;` |
|      3 | 7780 | `		case 'i':` |
|      - | 7781 | `			/* 	Minutes with leading zeros */` |
|      7 | 7782 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|      7 | 7783 | `			break;` |
|      3 | 7784 | `		case 's':` |
|      - | 7785 | `			/* 	second with leading zeros */` |
|      7 | 7786 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|      7 | 7787 | `			break;` |
|    ! 0 | 7788 | `		case 'u':` |
|      - | 7789 | `			/* 	Microseconds */` |
|    ! 0 | 7790 | `			ph7_result_string_format(pCtx,"%u",pTm->tm_sec * SX_USEC_PER_SEC);` |
|    ! 0 | 7791 | `			break;` |
|    ! 0 | 7792 | `		case 'S':{` |
|      - | 7793 | `			/* English ordinal suffix for the day of the month, 2 characters */` |
|      - | 7794 | `			static const char zSuffix[] = "thstndrdthththththth";` |
|    ! 0 | 7795 | `			int v = pTm->tm_mday;` |
|    ! 0 | 7796 | `			ph7_result_string(pCtx,&zSuffix[2 * (int)(v / 10 % 10 != 1 ? v % 10 : 0)],(int)sizeof(char) * 2);` |
|    ! 0 | 7797 | `			break;` |
|      - | 7798 | `				 }` |
|    ! 0 | 7799 | `		case 'e':` |
|      - | 7800 | `			/* 	Timezone identifier */` |
|    ! 0 | 7801 | `			zCur = pTm->tm_zone;` |
|    ! 0 | 7802 | `			if( zCur == 0 ){` |
|      - | 7803 | `				/* Assume GMT */` |
|    ! 0 | 7804 | `				zCur = "GMT";` |
|    ! 0 | 7805 | `			}` |
|    ! 0 | 7806 | `			ph7_result_string(pCtx,zCur,-1);` |
|    ! 0 | 7807 | `			break;` |
|    ! 0 | 7808 | `		case 'I':` |
|      - | 7809 | `			/* Whether or not the date is in daylight saving time */` |
|      - | 7810 | `#ifdef __WINNT__` |
|      - | 7811 | `#ifdef _MSC_VER` |
|      - | 7812 | `#ifndef _WIN32_WCE` |
|    ! 0 | 7813 | `			_get_daylight(&pTm->tm_isdst);` |
|      - | 7814 | `#endif` |
|      - | 7815 | `#endif` |
|      - | 7816 | `#endif` |
|    ! 0 | 7817 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_isdst == 1);` |
|    ! 0 | 7818 | `			break;` |
|    ! 0 | 7819 | `		case 'r':` |
|      - | 7820 | `			/* RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 */` |
|    ! 0 | 7821 | `			ph7_result_string_format(pCtx,"%.3s, %02d %.3s %4d %02d:%02d:%02d",` |
|    ! 0 | 7822 | `				SyTimeGetDay(pTm->tm_wday),` |
|    ! 0 | 7823 | `				pTm->tm_mday,` |
|    ! 0 | 7824 | `				SyTimeGetMonth(pTm->tm_mon),` |
|    ! 0 | 7825 | `				pTm->tm_year,` |
|    ! 0 | 7826 | `				pTm->tm_hour,` |
|    ! 0 | 7827 | `				pTm->tm_min,` |
|    ! 0 | 7828 | `				pTm->tm_sec` |
|      - | 7829 | `				);` |
|    ! 0 | 7830 | `			break;` |
|    ! 0 | 7831 | `		case 'U':{` |
|      - | 7832 | `			time_t tt;` |
|      - | 7833 | `			/* Seconds since the Unix Epoch */` |
|    ! 0 | 7834 | `			time(&tt);` |
|    ! 0 | 7835 | `			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);` |
|    ! 0 | 7836 | `			break;` |
|      - | 7837 | `				 }` |
|    ! 0 | 7838 | `		case 'O':` |
|      - | 7839 | `		case 'P':` |
|      - | 7840 | `			/* Difference to Greenwich time (GMT) in hours */` |
|    ! 0 | 7841 | `			ph7_result_string_format(pCtx,"%+05d",pTm->tm_gmtoff);` |
|    ! 0 | 7842 | `			break;` |
|    ! 0 | 7843 | `		case 'Z':` |
|      - | 7844 | `			/* Timezone offset in seconds. The offset for timezones west of UTC` |
|      - | 7845 | `			 * is always negative, and for those east of UTC is always positive.` |
|      - | 7846 | `			 */` |
|    ! 0 | 7847 | `			ph7_result_string_format(pCtx,"%+05d",pTm->tm_gmtoff);` |
|    ! 0 | 7848 | `			break;` |
|      1 | 7849 | `		case 'c':` |
|      - | 7850 | `			/* 	ISO 8601 date */` |
|      4 | 7851 | `			ph7_result_string_format(pCtx,"%4d-%02d-%02dT%02d:%02d:%02d%+05d",` |
|      1 | 7852 | `				pTm->tm_year,` |
|      2 | 7853 | `				pTm->tm_mon+1,` |
|      1 | 7854 | `				pTm->tm_mday,` |
|      1 | 7855 | `				pTm->tm_hour,` |
|      1 | 7856 | `				pTm->tm_min,` |
|      1 | 7857 | `				pTm->tm_sec,` |
|      1 | 7858 | `				pTm->tm_gmtoff` |
|      - | 7859 | `				);` |
|      3 | 7860 | `			break;` |
|      1 | 7861 | `		case '\\':` |
|      3 | 7862 | `			zIn++;` |
|      - | 7863 | `			/* Expand verbatim */` |
|      3 | 7864 | `			if( zIn < zEnd ){` |
|      3 | 7865 | `				ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|      1 | 7866 | `			}` |
|      3 | 7867 | `			break;` |
|     17 | 7868 | `		default:` |
|      - | 7869 | `			/* Unknown format specifer,expand verbatim */` |
|     35 | 7870 | `			ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|     34 | 7871 | `			break;` |
|      - | 7872 | `		}` |
|      - | 7873 | `		/* Point to the next character */` |
|    111 | 7874 | `		zIn++;` |
|      1 | 7875 | `	}` |
|     47 | 7876 | `	return SXRET_OK;` |
|      1 | 7877 |  |
|      - | 7878 | `/*` |
|      - | 7879 | ` * PH7 implementation of the strftime() function.` |
|      - | 7880 | ` * The following formats are supported:` |
|      - | 7881 | ` * %a 	An abbreviated textual representation of the day` |
|      - | 7882 | ` * %A 	A full textual representation of the day` |
|      - | 7883 | ` * %d 	Two-digit day of the month (with leading zeros)` |
|      - | 7884 | ` * %e 	Day of the month, with a space preceding single digits.` |
|      - | 7885 | ` * %j 	Day of the year, 3 digits with leading zeros` |
|      - | 7886 | ` * %u 	ISO-8601 numeric representation of the day of the week 	1 (for Monday) though 7 (for Sunday)` |
|      - | 7887 | ` * %w 	Numeric representation of the day of the week 0 (for Sunday) through 6 (for Saturday)` |
|      - | 7888 | ` * %U 	Week number of the given year, starting with the first Sunday as the first week` |
|      - | 7889 | ` * %V 	ISO-8601:1988 week number of the given year, starting with the first week of the year with at least` |
|      - | 7890 | ` *   4 weekdays, with Monday being the start of the week.` |
|      - | 7891 | ` * %W 	A numeric representation of the week of the year` |
|      - | 7892 | ` * %b 	Abbreviated month name, based on the locale` |
|      - | 7893 | ` * %B 	Full month name, based on the locale` |
|      - | 7894 | ` * %h 	Abbreviated month name, based on the locale (an alias of %b)` |
|      - | 7895 | ` * %m 	Two digit representation of the month` |
|      - | 7896 | ` * %C 	Two digit representation of the century (year divided by 100, truncated to an integer)` |
|      - | 7897 | ` * %g 	Two digit representation of the year going by ISO-8601:1988 standards (see %V)` |
|      - | 7898 | ` * %G 	The full four-digit version of %g` |
|      - | 7899 | ` * %y 	Two digit representation of the year` |
|      - | 7900 | ` * %Y 	Four digit representation for the year` |
|      - | 7901 | ` * %H 	Two digit representation of the hour in 24-hour format` |
|      - | 7902 | ` * %I 	Two digit representation of the hour in 12-hour format` |
|      - | 7903 | ` * %l (lower-case 'L') 	Hour in 12-hour format, with a space preceeding single digits` |
|      - | 7904 | ` * %M 	Two digit representation of the minute` |
|      - | 7905 | ` * %p 	UPPER-CASE 'AM' or 'PM' based on the given time` |
|      - | 7906 | ` * %P 	lower-case 'am' or 'pm' based on the given time` |
|      - | 7907 | ` * %r 	Same as "%I:%M:%S %p"` |
|      - | 7908 | ` * %R 	Same as "%H:%M"` |
|      - | 7909 | ` * %S 	Two digit representation of the second` |
|      - | 7910 | ` * %T 	Same as "%H:%M:%S"` |
|      - | 7911 | ` * %X 	Preferred time representation based on locale, without the date` |
|      - | 7912 | ` * %z 	Either the time zone offset from UTC or the abbreviation` |
|      - | 7913 | ` * %Z 	The time zone offset/abbreviation option NOT given by %z` |
|      - | 7914 | ` * %c 	Preferred date and time stamp based on local` |
|      - | 7915 | ` * %D 	Same as "%m/%d/%y"` |
|      - | 7916 | ` * %F 	Same as "%Y-%m-%d"` |
|      - | 7917 | ` * %s 	Unix Epoch Time timestamp (same as the time() function)` |
|      - | 7918 | ` * %x 	Preferred date representation based on locale, without the time` |
|      - | 7919 | ` * %n 	A newline character ("\n")` |
|      - | 7920 | ` * %t 	A Tab character ("\t")` |
|      - | 7921 | ` * %% 	A literal percentage character ("%")` |
|      - | 7922 | ` */` |
|     16 | 7923 | `static int PH7_Strftime(` |
|      - | 7924 | `	ph7_context *pCtx,  /* Call context */` |
|      - | 7925 | `	const char *zIn,    /* Input string */` |
|      - | 7926 | `	int nLen,           /* Input length */` |
|      - | 7927 | `	Sytm *pTm           /* Parse of the given time */` |
|      - | 7928 | `	)` |
|      1 | 7929 |  |
|     17 | 7930 | `	const char *zCur,*zEnd = &zIn[nLen];` |
|      - | 7931 | `	int c;` |
|      - | 7932 | `	/* Start the format process */` |
|     18 | 7933 | `	for(;;){` |
|     37 | 7934 | `		zCur = zIn;` |
|     41 | 7935 | `		while(zIn < zEnd && zIn[0] != '%' ){` |
|      5 | 7936 | `			zIn++;` |
|      1 | 7937 | `		}` |
|     37 | 7938 | `		if( zIn > zCur ){` |
|      - | 7939 | `			/* Consume input verbatim */` |
|      5 | 7940 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 7941 | `		}` |
|     37 | 7942 | `		zIn++; /* Jump the percent sign */` |
|     37 | 7943 | `		if( zIn >= zEnd ){` |
|      - | 7944 | `			/* No more input to process */` |
|     17 | 7945 | `			break;` |
|      - | 7946 | `		}` |
|     21 | 7947 | `		c = zIn[0];` |
|      - | 7948 | `		/* Act according to the current specifer */` |
|     21 | 7949 | `		switch(c){` |
|    ! 0 | 7950 | `		case '%':` |
|      - | 7951 | `			/* A literal percentage character ("%") */` |
|    ! 0 | 7952 | `			ph7_result_string(pCtx,"%",(int)sizeof(char));` |
|    ! 0 | 7953 | `			break;` |
|    ! 0 | 7954 | `		case 't':` |
|      - | 7955 | `			/* A Tab character */` |
|    ! 0 | 7956 | `			ph7_result_string(pCtx,"\t",(int)sizeof(char));` |
|    ! 0 | 7957 | `			break;` |
|    ! 0 | 7958 | `		case 'n':` |
|      - | 7959 | `			/* A newline character */` |
|    ! 0 | 7960 | `			ph7_result_string(pCtx,"\n",(int)sizeof(char));` |
|    ! 0 | 7961 | `			break;` |
|      1 | 7962 | `		case 'a':` |
|      - | 7963 | `			/* An abbreviated textual representation of the day */` |
|      3 | 7964 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),(int)sizeof(char)*3);` |
|      3 | 7965 | `			break;` |
|    ! 0 | 7966 | `		case 'A':` |
|      - | 7967 | `			/* A full textual representation of the day */` |
|    ! 0 | 7968 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),-1/*Compute length automatically*/);` |
|    ! 0 | 7969 | `			break;` |
|    ! 0 | 7970 | `		case 'e':` |
|      - | 7971 | `			/* Day of the month, 2 digits with leading space for single digit*/` |
|    ! 0 | 7972 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_mday);` |
|    ! 0 | 7973 | `			break;` |
|      2 | 7974 | `		case 'd':` |
|      - | 7975 | `			/* Two-digit day of the month (with leading zeros) */` |
|      5 | 7976 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon+1);` |
|      5 | 7977 | `			break;` |
|    ! 0 | 7978 | `		case 'j':` |
|      - | 7979 | `			/*The day of the year,3 digits with leading zeros*/` |
|    ! 0 | 7980 | `			ph7_result_string_format(pCtx,"%03d",pTm->tm_yday);` |
|    ! 0 | 7981 | `			break;` |
|    ! 0 | 7982 | `		case 'u':` |
|      - | 7983 | `			/* ISO-8601 numeric representation of the day of the week */` |
|    ! 0 | 7984 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|    ! 0 | 7985 | `			break;` |
|    ! 0 | 7986 | `		case 'w':` |
|      - | 7987 | `			/* Numeric representation of the day of the week */` |
|    ! 0 | 7988 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|    ! 0 | 7989 | `			break;` |
|    ! 0 | 7990 | `		case 'b':` |
|      - | 7991 | `		case 'h':` |
|      - | 7992 | `			/*A short textual representation of a month, three letters (Not based on locale)*/` |
|    ! 0 | 7993 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),(int)sizeof(char)*3);` |
|    ! 0 | 7994 | `			break;` |
|    ! 0 | 7995 | `		case 'B':` |
|      - | 7996 | `			/* Full month name (Not based on locale) */` |
|    ! 0 | 7997 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),-1/*Compute length automatically*/);` |
|    ! 0 | 7998 | `			break;` |
|      2 | 7999 | `		case 'm':` |
|      - | 8000 | `			/*Numeric representation of a month, with leading zeros*/` |
|      5 | 8001 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|      5 | 8002 | `			break;` |
|    ! 0 | 8003 | `		case 'C':` |
|      - | 8004 | `			/* Two digit representation of the century */` |
|    ! 0 | 8005 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year/100);` |
|    ! 0 | 8006 | `			break;` |
|    ! 0 | 8007 | `		case 'y':` |
|      - | 8008 | `		case 'g':` |
|      - | 8009 | `			/* Two digit representation of the year */` |
|    ! 0 | 8010 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year%100);` |
|    ! 0 | 8011 | `			break;` |
|      2 | 8012 | `		case 'Y':` |
|      - | 8013 | `		case 'G':` |
|      - | 8014 | `			/* Four digit representation of the year */` |
|      5 | 8015 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|      5 | 8016 | `			break;` |
|    ! 0 | 8017 | `		case 'I':` |
|      - | 8018 | `			/* 12-hour format of an hour with leading zeros */` |
|    ! 0 | 8019 | `			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));` |
|    ! 0 | 8020 | `			break;` |
|    ! 0 | 8021 | `		case 'l':` |
|      - | 8022 | `			/* 12-hour format of an hour with leading space */` |
|    ! 0 | 8023 | `			ph7_result_string_format(pCtx,"%2d",1+(pTm->tm_hour%12));` |
|    ! 0 | 8024 | `			break;` |
|      1 | 8025 | `		case 'H':` |
|      - | 8026 | `			/* 24-hour format of an hour with leading zeros */` |
|      3 | 8027 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|      3 | 8028 | `			break;` |
|      1 | 8029 | `		case 'M':` |
|      - | 8030 | `			/* Minutes with leading zeros */` |
|      3 | 8031 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|      3 | 8032 | `			break;` |
|    ! 0 | 8033 | `		case 'S':` |
|      - | 8034 | `			/* Seconds with leading zeros */` |
|    ! 0 | 8035 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|    ! 0 | 8036 | `			break;` |
|    ! 0 | 8037 | `		case 'z':` |
|      - | 8038 | `		case 'Z':` |
|      - | 8039 | `			/* 	Timezone identifier */` |
|    ! 0 | 8040 | `			zCur = pTm->tm_zone;` |
|    ! 0 | 8041 | `			if( zCur == 0 ){` |
|      - | 8042 | `				/* Assume GMT */` |
|    ! 0 | 8043 | `				zCur = "GMT";` |
|    ! 0 | 8044 | `			}` |
|    ! 0 | 8045 | `			ph7_result_string(pCtx,zCur,-1);` |
|    ! 0 | 8046 | `			break;` |
|    ! 0 | 8047 | `		case 'T':` |
|      - | 8048 | `		case 'X':` |
|      - | 8049 | `			/* Same as "%H:%M:%S" */` |
|    ! 0 | 8050 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d",pTm->tm_hour,pTm->tm_min,pTm->tm_sec);` |
|    ! 0 | 8051 | `			break;` |
|    ! 0 | 8052 | `		case 'R':` |
|      - | 8053 | `			/* Same as "%H:%M" */` |
|    ! 0 | 8054 | `			ph7_result_string_format(pCtx,"%02d:%02d",pTm->tm_hour,pTm->tm_min);` |
|    ! 0 | 8055 | `			break;` |
|    ! 0 | 8056 | `		case 'P':` |
|      - | 8057 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|    ! 0 | 8058 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",(int)sizeof(char)*2);` |
|    ! 0 | 8059 | `			break;` |
|    ! 0 | 8060 | `		case 'p':` |
|      - | 8061 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|    ! 0 | 8062 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",(int)sizeof(char)*2);` |
|    ! 0 | 8063 | `			break;` |
|    ! 0 | 8064 | `		case 'r':` |
|      - | 8065 | `			/* Same as "%I:%M:%S %p" */` |
|    ! 0 | 8066 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d %s",` |
|    ! 0 | 8067 | `				1+(pTm->tm_hour%12),` |
|    ! 0 | 8068 | `				pTm->tm_min,` |
|    ! 0 | 8069 | `				pTm->tm_sec,` |
|    ! 0 | 8070 | `				pTm->tm_hour > 12 ? "PM" : "AM"` |
|      - | 8071 | `				);` |
|    ! 0 | 8072 | `			break;` |
|      1 | 8073 | `		case 'D':` |
|      - | 8074 | `		case 'x':` |
|      - | 8075 | `			/* Same as "%m/%d/%y" */` |
|      4 | 8076 | `			ph7_result_string_format(pCtx,"%02d/%02d/%02d",` |
|      2 | 8077 | `				pTm->tm_mon+1,` |
|      1 | 8078 | `				pTm->tm_mday,` |
|      2 | 8079 | `				pTm->tm_year%100` |
|      - | 8080 | `				);` |
|      3 | 8081 | `			break;` |
|    ! 0 | 8082 | `		case 'F':` |
|      - | 8083 | `			/* Same as "%Y-%m-%d" */` |
|    ! 0 | 8084 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d",` |
|    ! 0 | 8085 | `				pTm->tm_year,` |
|    ! 0 | 8086 | `				pTm->tm_mon+1,` |
|    ! 0 | 8087 | `				pTm->tm_mday` |
|      - | 8088 | `				);` |
|    ! 0 | 8089 | `			break;` |
|    ! 0 | 8090 | `		case 'c':` |
|    ! 0 | 8091 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d %02d:%02d:%02d",` |
|    ! 0 | 8092 | `				pTm->tm_year,` |
|    ! 0 | 8093 | `				pTm->tm_mon+1,` |
|    ! 0 | 8094 | `				pTm->tm_mday,` |
|    ! 0 | 8095 | `				pTm->tm_hour,` |
|    ! 0 | 8096 | `				pTm->tm_min,` |
|    ! 0 | 8097 | `				pTm->tm_sec` |
|      - | 8098 | `				);` |
|    ! 0 | 8099 | `			break;` |
|    ! 0 | 8100 | `		case 's':{` |
|      - | 8101 | `			time_t tt;` |
|      - | 8102 | `			/* Seconds since the Unix Epoch */` |
|    ! 0 | 8103 | `			time(&tt);` |
|    ! 0 | 8104 | `			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);` |
|    ! 0 | 8105 | `			break;` |
|      - | 8106 | `				 }` |
|    ! 0 | 8107 | `		default:` |
|      - | 8108 | `			/* unknown specifer,simply ignore*/` |
|    ! 0 | 8109 | `			break;` |
|      - | 8110 | `		}` |
|      - | 8111 | `		/* Advance the cursor */` |
|     21 | 8112 | `		zIn++;` |
|      1 | 8113 | `	}` |
|     17 | 8114 | `	return SXRET_OK;` |
|      1 | 8115 |  |
|      - | 8116 | `/*` |
|      - | 8117 | ` * string date(string $format [, int $timestamp = time() ] )` |
|      - | 8118 | ` *  Returns a string formatted according to the given format string using` |
|      - | 8119 | ` *  the given integer timestamp or the current time if no timestamp is given.` |
|      - | 8120 | ` *  In other words, timestamp is optional and defaults to the value of time().` |
|      - | 8121 | ` * Parameters` |
|      - | 8122 | ` *  $format` |
|      - | 8123 | ` *   The format of the outputted date string (See code above)` |
|      - | 8124 | ` * $timestamp` |
|      - | 8125 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|      - | 8126 | ` *   that defaults to the current local time if a timestamp is not given.` |
|      - | 8127 | ` *   In other words, it defaults to the value of time().` |
|      - | 8128 | ` * Return` |
|      - | 8129 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|      - | 8130 | ` */` |
|     36 | 8131 | `static int PH7_builtin_date(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8132 |  |
|      - | 8133 | `	const char *zFormat;` |
|      - | 8134 | `	int nLen;` |
|      - | 8135 | `	Sytm sTm;` |
|     37 | 8136 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8137 | `		/* Missing/Invalid argument,return FALSE */` |
|      5 | 8138 | `		ph7_result_bool(pCtx,0);` |
|      5 | 8139 | `		return PH7_OK;` |
|      - | 8140 | `	}` |
|     33 | 8141 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     33 | 8142 | `	if( nLen < 1 ){` |
|      - | 8143 | `		/* Don't bother processing return the empty string */` |
|    ! 0 | 8144 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 8145 | `	}` |
|     33 | 8146 | `	if( nArg < 2 ){` |
|      - | 8147 | `#ifdef __WINNT__` |
|      - | 8148 | `		SYSTEMTIME sOS;` |
|      1 | 8149 | `		GetSystemTime(&sOS);` |
|      1 | 8150 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8151 | `#else` |
|      - | 8152 | `		struct tm *pTm;` |
|      - | 8153 | `		time_t t;` |
|     30 | 8154 | `		time(&t);` |
|     30 | 8155 | `		pTm = localtime(&t);` |
|     30 | 8156 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8157 | `#endif` |
|     16 | 8158 | `	}else{` |
|      - | 8159 | `		/* Use the given timestamp */` |
|      - | 8160 | `		time_t t;` |
|      - | 8161 | `		struct tm *pTm;` |
|      3 | 8162 | `		if( ph7_value_is_int(apArg[1]) ){` |
|      3 | 8163 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|      3 | 8164 | `			pTm = localtime(&t);` |
|      3 | 8165 | `			if( pTm == 0 ){` |
|    ! 0 | 8166 | `				time(&t);` |
|    ! 0 | 8167 | `			}` |
|      2 | 8168 | `		}else{` |
|    ! 0 | 8169 | `			time(&t);` |
|      - | 8170 | `		}` |
|      3 | 8171 | `		pTm = localtime(&t);` |
|      3 | 8172 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8173 | `	}` |
|      - | 8174 | `	/* Format the given string */` |
|     33 | 8175 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|     33 | 8176 | `	return PH7_OK;` |
|     19 | 8177 |  |
|      - | 8178 | `/*` |
|      - | 8179 | ` * string strftime(string $format [, int $timestamp = time() ] )` |
|      - | 8180 | ` *  Format a local time/date (PLATFORM INDEPENDANT IMPLEENTATION NOT BASED ON LOCALE)` |
|      - | 8181 | ` * Parameters` |
|      - | 8182 | ` *  $format` |
|      - | 8183 | ` *   The format of the outputted date string (See code above)` |
|      - | 8184 | ` * $timestamp` |
|      - | 8185 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|      - | 8186 | ` *   that defaults to the current local time if a timestamp is not given.` |
|      - | 8187 | ` *   In other words, it defaults to the value of time().` |
|      - | 8188 | ` * Return` |
|      - | 8189 | ` * Returns a string formatted according format using the given timestamp` |
|      - | 8190 | ` * or the current local time if no timestamp is given.` |
|      - | 8191 | ` */` |
|     20 | 8192 | `static int PH7_builtin_strftime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8193 |  |
|      - | 8194 | `	const char *zFormat;` |
|      - | 8195 | `	int nLen;` |
|      - | 8196 | `	Sytm sTm;` |
|     21 | 8197 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8198 | `		/* Missing/Invalid argument,return FALSE */` |
|      5 | 8199 | `		ph7_result_bool(pCtx,0);` |
|      5 | 8200 | `		return PH7_OK;` |
|      - | 8201 | `	}` |
|     17 | 8202 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8203 | `	if( nLen < 1 ){` |
|      - | 8204 | `		/* Don't bother processing return FALSE */` |
|    ! 0 | 8205 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8206 | `	}` |
|     17 | 8207 | `	if( nArg < 2 ){` |
|      - | 8208 | `#ifdef __WINNT__` |
|      - | 8209 | `		SYSTEMTIME sOS;` |
|      1 | 8210 | `		GetSystemTime(&sOS);` |
|      1 | 8211 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8212 | `#else` |
|      - | 8213 | `		struct tm *pTm;` |
|      - | 8214 | `		time_t t;` |
|     14 | 8215 | `		time(&t);` |
|     14 | 8216 | `		pTm = localtime(&t);` |
|     14 | 8217 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8218 | `#endif` |
|      8 | 8219 | `	}else{` |
|      - | 8220 | `		/* Use the given timestamp */` |
|      - | 8221 | `		time_t t;` |
|      - | 8222 | `		struct tm *pTm;` |
|      3 | 8223 | `		if( ph7_value_is_int(apArg[1]) ){` |
|      3 | 8224 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|      3 | 8225 | `			pTm = localtime(&t);` |
|      3 | 8226 | `			if( pTm == 0 ){` |
|    ! 0 | 8227 | `				time(&t);` |
|    ! 0 | 8228 | `			}` |
|      2 | 8229 | `		}else{` |
|    ! 0 | 8230 | `			time(&t);` |
|      - | 8231 | `		}` |
|      3 | 8232 | `		pTm = localtime(&t);` |
|      3 | 8233 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8234 | `	}` |
|      - | 8235 | `	/* Format the given string */` |
|     17 | 8236 | `	PH7_Strftime(pCtx,zFormat,nLen,&sTm);` |
|     17 | 8237 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|      - | 8238 | `		/* Nothing was formatted,return FALSE */` |
|    ! 0 | 8239 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8240 | `	}` |
|     17 | 8241 | `	return PH7_OK;` |
|     11 | 8242 |  |
|      - | 8243 | `/*` |
|      - | 8244 | ` * string gmdate(string $format [, int $timestamp = time() ] )` |
|      - | 8245 | ` *  Identical to the date() function except that the time returned` |
|      - | 8246 | ` *  is Greenwich Mean Time (GMT).` |
|      - | 8247 | ` * Parameters` |
|      - | 8248 | ` *  $format` |
|      - | 8249 | ` *  The format of the outputted date string (See code above)` |
|      - | 8250 | ` *  $timestamp` |
|      - | 8251 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|      - | 8252 | ` *   that defaults to the current local time if a timestamp is not given.` |
|      - | 8253 | ` *   In other words, it defaults to the value of time().` |
|      - | 8254 | ` * Return` |
|      - | 8255 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|      - | 8256 | ` */` |
|     16 | 8257 | `static int PH7_builtin_gmdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8258 |  |
|      - | 8259 | `	const char *zFormat;` |
|      - | 8260 | `	int nLen;` |
|      - | 8261 | `	Sytm sTm;` |
|     17 | 8262 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8263 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 | 8264 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8265 | `		return PH7_OK;` |
|      - | 8266 | `	}` |
|     15 | 8267 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 8268 | `	if( nLen < 1 ){` |
|      - | 8269 | `		/* Don't bother processing return the empty string */` |
|    ! 0 | 8270 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 8271 | `	}` |
|     15 | 8272 | `	if( nArg < 2 ){` |
|      - | 8273 | `#ifdef __WINNT__` |
|      - | 8274 | `		SYSTEMTIME sOS;` |
|      1 | 8275 | `		GetSystemTime(&sOS);` |
|      1 | 8276 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8277 | `#else` |
|      - | 8278 | `		struct tm *pTm;` |
|      - | 8279 | `		time_t t;` |
|     12 | 8280 | `		time(&t);` |
|     12 | 8281 | `		pTm = gmtime(&t);` |
|     12 | 8282 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8283 | `#endif` |
|      7 | 8284 | `	}else{` |
|      - | 8285 | `		/* Use the given timestamp */` |
|      - | 8286 | `		time_t t;` |
|      - | 8287 | `		struct tm *pTm;` |
|      3 | 8288 | `		if( ph7_value_is_int(apArg[1]) ){` |
|      3 | 8289 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|      3 | 8290 | `			pTm = gmtime(&t);` |
|      3 | 8291 | `			if( pTm == 0 ){` |
|    ! 0 | 8292 | `				time(&t);` |
|    ! 0 | 8293 | `			}` |
|      2 | 8294 | `		}else{` |
|    ! 0 | 8295 | `			time(&t);` |
|      - | 8296 | `		}` |
|      3 | 8297 | `		pTm = gmtime(&t);` |
|      3 | 8298 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8299 | `	}` |
|      - | 8300 | `	/* Format the given string */` |
|     15 | 8301 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|     15 | 8302 | `	return PH7_OK;` |
|      9 | 8303 |  |
|      - | 8304 | `/*` |
|      - | 8305 | ` * array localtime([ int $timestamp = time() [, bool $is_associative = false ]])` |
|      - | 8306 | ` *  Return the local time.` |
|      - | 8307 | ` * Parameter` |
|      - | 8308 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|      - | 8309 | ` *     that defaults to the current local time if a timestamp is not given.` |
|      - | 8310 | ` *     In other words, it defaults to the value of time().` |
|      - | 8311 | ` * $is_associative` |
|      - | 8312 | ` *   If set to FALSE or not supplied then the array is returned as a regular, numerically` |
|      - | 8313 | ` *   indexed array. If the argument is set to TRUE then localtime() returns an associative` |
|      - | 8314 | ` *   array containing all the different elements of the structure returned by the C function` |
|      - | 8315 | ` *   call to localtime. The names of the different keys of the associative array are as follows:` |
|      - | 8316 | ` *      "tm_sec" - seconds, 0 to 59` |
|      - | 8317 | ` *      "tm_min" - minutes, 0 to 59` |
|      - | 8318 | ` *      "tm_hour" - hours, 0 to 23` |
|      - | 8319 | ` *      "tm_mday" - day of the month, 1 to 31` |
|      - | 8320 | ` *      "tm_mon" - month of the year, 0 (Jan) to 11 (Dec)` |
|      - | 8321 | ` *      "tm_year" - years since 1900` |
|      - | 8322 | ` *      "tm_wday" - day of the week, 0 (Sun) to 6 (Sat)` |
|      - | 8323 | ` *      "tm_yday" - day of the year, 0 to 365` |
|      - | 8324 | ` *      "tm_isdst" - is daylight savings time in effect? Positive if yes, 0 if not, negative if unknown.` |
|      - | 8325 | ` * Returns` |
|      - | 8326 | ` *  An associative array of information related to the timestamp.` |
|      - | 8327 | ` */` |
|      8 | 8328 | `static int PH7_builtin_localtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8329 |  |
|      - | 8330 | `	ph7_value *pValue,*pArray;` |
|      9 | 8331 | `	int isAssoc = 0;` |
|      - | 8332 | `	Sytm sTm;` |
|      9 | 8333 | `	if( nArg < 1 ){` |
|      - | 8334 | `#ifdef __WINNT__` |
|      - | 8335 | `		SYSTEMTIME sOS;` |
|      1 | 8336 | `		GetSystemTime(&sOS); /* TODO(chems): GMT not local */` |
|      1 | 8337 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8338 | `#else` |
|      - | 8339 | `		struct tm *pTm;` |
|      - | 8340 | `		time_t t;` |
|      4 | 8341 | `		time(&t);` |
|      4 | 8342 | `		pTm = localtime(&t);` |
|      4 | 8343 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8344 | `#endif` |
|      3 | 8345 | `	}else{` |
|      - | 8346 | `		/* Use the given timestamp */` |
|      - | 8347 | `		time_t t;` |
|      - | 8348 | `		struct tm *pTm;` |
|      5 | 8349 | `		if( ph7_value_is_int(apArg[0]) ){` |
|      5 | 8350 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|      5 | 8351 | `			pTm = localtime(&t);` |
|      5 | 8352 | `			if( pTm == 0 ){` |
|    ! 0 | 8353 | `				time(&t);` |
|    ! 0 | 8354 | `			}` |
|      3 | 8355 | `		}else{` |
|    ! 0 | 8356 | `			time(&t);` |
|      - | 8357 | `		}` |
|      5 | 8358 | `		pTm = localtime(&t);` |
|      5 | 8359 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8360 | `	}` |
|      - | 8361 | `	/* Element value */` |
|      9 | 8362 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      9 | 8363 | `	if( pValue == 0 ){` |
|      - | 8364 | `		/* Return NULL */` |
|    ! 0 | 8365 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8366 | `		return PH7_OK;` |
|      - | 8367 | `	}` |
|      - | 8368 | `	/* Create a new array */` |
|      9 | 8369 | `	pArray = ph7_context_new_array(pCtx);` |
|      9 | 8370 | `	if( pArray == 0 ){` |
|      - | 8371 | `		/* Return NULL */` |
|    ! 0 | 8372 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8373 | `		return PH7_OK;` |
|      - | 8374 | `	}` |
|      9 | 8375 | `	if( nArg > 1 ){` |
|      3 | 8376 | `		isAssoc = ph7_value_to_bool(apArg[1]);` |
|      1 | 8377 | `	}` |
|      - | 8378 | `	/* Fill the array */` |
|      - | 8379 | `	/* Seconds */` |
|      9 | 8380 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|      9 | 8381 | `	if( isAssoc ){` |
|      3 | 8382 | `		ph7_array_add_strkey_elem(pArray,"tm_sec",pValue);` |
|      2 | 8383 | `	}else{` |
|      7 | 8384 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8385 | `	}` |
|      - | 8386 | `	/* Minutes */` |
|      9 | 8387 | `	ph7_value_int(pValue,sTm.tm_min);` |
|      9 | 8388 | `	if( isAssoc ){` |
|      3 | 8389 | `		ph7_array_add_strkey_elem(pArray,"tm_min",pValue);` |
|      2 | 8390 | `	}else{` |
|      7 | 8391 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8392 | `	}` |
|      - | 8393 | `	/* Hours */` |
|      9 | 8394 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|      9 | 8395 | `	if( isAssoc ){` |
|      3 | 8396 | `		ph7_array_add_strkey_elem(pArray,"tm_hour",pValue);` |
|      2 | 8397 | `	}else{` |
|      7 | 8398 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8399 | `	}` |
|      - | 8400 | `	/* mday */` |
|      9 | 8401 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|      9 | 8402 | `	if( isAssoc ){` |
|      3 | 8403 | `		ph7_array_add_strkey_elem(pArray,"tm_mday",pValue);` |
|      2 | 8404 | `	}else{` |
|      7 | 8405 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8406 | `	}` |
|      - | 8407 | `	/* mon */` |
|      9 | 8408 | `	ph7_value_int(pValue,sTm.tm_mon);` |
|      9 | 8409 | `	if( isAssoc ){` |
|      3 | 8410 | `		ph7_array_add_strkey_elem(pArray,"tm_mon",pValue);` |
|      2 | 8411 | `	}else{` |
|      7 | 8412 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8413 | `	}` |
|      - | 8414 | `	/* year since 1900 */` |
|      9 | 8415 | `	ph7_value_int(pValue,sTm.tm_year-1900);` |
|      9 | 8416 | `	if( isAssoc ){` |
|      3 | 8417 | `		ph7_array_add_strkey_elem(pArray,"tm_year",pValue);` |
|      2 | 8418 | `	}else{` |
|      7 | 8419 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8420 | `	}` |
|      - | 8421 | `	/* wday */` |
|      9 | 8422 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|      9 | 8423 | `	if( isAssoc ){` |
|      3 | 8424 | `		ph7_array_add_strkey_elem(pArray,"tm_wday",pValue);` |
|      2 | 8425 | `	}else{` |
|      7 | 8426 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8427 | `	}` |
|      - | 8428 | `	/* yday */` |
|      9 | 8429 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|      9 | 8430 | `	if( isAssoc ){` |
|      3 | 8431 | `		ph7_array_add_strkey_elem(pArray,"tm_yday",pValue);` |
|      2 | 8432 | `	}else{` |
|      7 | 8433 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8434 | `	}` |
|      - | 8435 | `	/* isdst */` |
|      - | 8436 | `#ifdef __WINNT__` |
|      - | 8437 | `#ifdef _MSC_VER` |
|      - | 8438 | `#ifndef _WIN32_WCE` |
|      1 | 8439 | `			_get_daylight(&sTm.tm_isdst);` |
|      - | 8440 | `#endif` |
|      - | 8441 | `#endif` |
|      - | 8442 | `#endif` |
|      9 | 8443 | `	ph7_value_int(pValue,sTm.tm_isdst);` |
|      9 | 8444 | `	if( isAssoc ){` |
|      3 | 8445 | `		ph7_array_add_strkey_elem(pArray,"tm_isdst",pValue);` |
|      2 | 8446 | `	}else{` |
|      7 | 8447 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8448 | `	}` |
|      - | 8449 | `	/* Return the array */` |
|      9 | 8450 | `	ph7_result_value(pCtx,pArray);` |
|      9 | 8451 | `	return PH7_OK;` |
|      5 | 8452 |  |
|      - | 8453 | `/*` |
|      - | 8454 | ` * int idate(string $format [, int $timestamp = time() ])` |
|      - | 8455 | ` *  Returns a number formatted according to the given format string` |
|      - | 8456 | ` *  using the given integer timestamp or the current local time if` |
|      - | 8457 | ` *  no timestamp is given. In other words, timestamp is optional and defaults` |
|      - | 8458 | ` *  to the value of time().` |
|      - | 8459 | ` *  Unlike the function date(), idate() accepts just one char in the format` |
|      - | 8460 | ` *  parameter.` |
|      - | 8461 | ` * $Parameters` |
|      - | 8462 | ` *  Supported format` |
|      - | 8463 | ` *   d 	Day of the month` |
|      - | 8464 | ` *   h 	Hour (12 hour format)` |
|      - | 8465 | ` *   H 	Hour (24 hour format)` |
|      - | 8466 | ` *   i 	Minutes` |
|      - | 8467 | ` *   I (uppercase i)1 if DST is activated, 0 otherwise` |
|      - | 8468 | ` *   L (uppercase l) returns 1 for leap year, 0 otherwise` |
|      - | 8469 | ` *   m 	Month number` |
|      - | 8470 | ` *   s 	Seconds` |
|      - | 8471 | ` *   t 	Days in current month` |
|      - | 8472 | ` *   U 	Seconds since the Unix Epoch - January 1 1970 00:00:00 UTC - this is the same as time()` |
|      - | 8473 | ` *   w 	Day of the week (0 on Sunday)` |
|      - | 8474 | ` *   W 	ISO-8601 week number of year, weeks starting on Monday` |
|      - | 8475 | ` *   y 	Year (1 or 2 digits - check note below)` |
|      - | 8476 | ` *   Y 	Year (4 digits)` |
|      - | 8477 | ` *   z 	Day of the year` |
|      - | 8478 | ` *   Z 	Timezone offset in seconds` |
|      - | 8479 | ` * $timestamp` |
|      - | 8480 | ` *  The optional timestamp parameter is an integer Unix timestamp that defaults` |
|      - | 8481 | ` *  to the current local time if a timestamp is not given. In other words, it defaults` |
|      - | 8482 | ` *  to the value of time().` |
|      - | 8483 | ` * Return` |
|      - | 8484 | ` *  An integer.` |
|      - | 8485 | ` */` |
|     42 | 8486 | `static int PH7_builtin_idate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 8487 |  |
|      - | 8488 | `	const char *zFormat;` |
|     44 | 8489 | `	ph7_int64 iVal = 0;` |
|      - | 8490 | `	int nLen;` |
|      - | 8491 | `	Sytm sTm;` |
|     44 | 8492 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8493 | `		/* Missing/Invalid argument,return -1 */` |
|      5 | 8494 | `		ph7_result_int(pCtx,-1);` |
|      5 | 8495 | `		return PH7_OK;` |
|      - | 8496 | `	}` |
|     40 | 8497 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     40 | 8498 | `	if( nLen < 1 ){` |
|      - | 8499 | `		/* Don't bother processing return -1*/` |
|    ! 0 | 8500 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 8501 | `	}` |
|     40 | 8502 | `	if( nArg < 2 ){` |
|      - | 8503 | `#ifdef __WINNT__` |
|      - | 8504 | `		SYSTEMTIME sOS;` |
|      2 | 8505 | `		GetSystemTime(&sOS);` |
|      2 | 8506 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8507 | `#else` |
|      - | 8508 | `		struct tm *pTm;` |
|      - | 8509 | `		time_t t;` |
|     28 | 8510 | `		time(&t);` |
|     28 | 8511 | `		pTm = localtime(&t);` |
|     28 | 8512 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8513 | `#endif` |
|     16 | 8514 | `	}else{` |
|      - | 8515 | `		/* Use the given timestamp */` |
|      - | 8516 | `		time_t t;` |
|      - | 8517 | `		struct tm *pTm;` |
|     11 | 8518 | `		if( ph7_value_is_int(apArg[1]) ){` |
|     11 | 8519 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|     11 | 8520 | `			pTm = localtime(&t);` |
|     11 | 8521 | `			if( pTm == 0 ){` |
|    ! 0 | 8522 | `				time(&t);` |
|    ! 0 | 8523 | `			}` |
|      6 | 8524 | `		}else{` |
|    ! 0 | 8525 | `			time(&t);` |
|      - | 8526 | `		}` |
|     11 | 8527 | `		pTm = localtime(&t);` |
|     11 | 8528 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8529 | `	}` |
|      - | 8530 | `	/* Perform the requested operation */` |
|     40 | 8531 | `	switch(zFormat[0]){` |
|      2 | 8532 | `	case 'd':` |
|      - | 8533 | `		/* Day of the month */` |
|      5 | 8534 | `		iVal = sTm.tm_mday;` |
|      5 | 8535 | `		break;` |
|    ! 0 | 8536 | `	case 'h':` |
|      - | 8537 | `		/*	Hour (12 hour format)*/` |
|    ! 0 | 8538 | `		iVal = 1 + (sTm.tm_hour % 12);` |
|    ! 0 | 8539 | `		break;` |
|      1 | 8540 | `	case 'H':` |
|      - | 8541 | `		/* Hour (24 hour format)*/` |
|      3 | 8542 | `		iVal = sTm.tm_hour;` |
|      3 | 8543 | `		break;` |
|      1 | 8544 | `	case 'i':` |
|      - | 8545 | `		/*Minutes*/` |
|      3 | 8546 | `		iVal = sTm.tm_min;` |
|      3 | 8547 | `		break;` |
|      1 | 8548 | `	case 'I':` |
|      - | 8549 | `		/*	returns 1 if DST is activated, 0 otherwise */` |
|      - | 8550 | `#ifdef __WINNT__` |
|      - | 8551 | `#ifdef _MSC_VER` |
|      - | 8552 | `#ifndef _WIN32_WCE` |
|      1 | 8553 | `			_get_daylight(&sTm.tm_isdst);` |
|      - | 8554 | `#endif` |
|      - | 8555 | `#endif` |
|      - | 8556 | `#endif` |
|      3 | 8557 | `		iVal = sTm.tm_isdst;` |
|      3 | 8558 | `		break;` |
|      1 | 8559 | `	case 'L':` |
|      - | 8560 | `		/* 	returns 1 for leap year, 0 otherwise */` |
|      3 | 8561 | `		iVal = IS_LEAP_YEAR(sTm.tm_year);` |
|      3 | 8562 | `		break;` |
|      2 | 8563 | `	case 'm':` |
|      - | 8564 | `		/* Month number*/` |
|      5 | 8565 | `		iVal = sTm.tm_mon;` |
|      5 | 8566 | `		break;` |
|      1 | 8567 | `	case 's':` |
|      - | 8568 | `		/*Seconds*/` |
|      3 | 8569 | `		iVal = sTm.tm_sec;` |
|      3 | 8570 | `		break;` |
|      1 | 8571 | `	case 't':{` |
|      - | 8572 | `		/*Days in current month*/` |
|      - | 8573 | `		static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|      3 | 8574 | `		int nDays = aMonDays[sTm.tm_mon % 12 ];` |
|      3 | 8575 | `		if( sTm.tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(sTm.tm_year) ){` |
|    ! 0 | 8576 | `			nDays = 28;` |
|    ! 0 | 8577 | `		}` |
|      3 | 8578 | `		iVal = nDays;` |
|      3 | 8579 | `		break;` |
|      - | 8580 | `			 }` |
|      1 | 8581 | `	case 'U':` |
|      - | 8582 | `		/*Seconds since the Unix Epoch*/` |
|      3 | 8583 | `		iVal = (ph7_int64)time(0);` |
|      3 | 8584 | `		break;` |
|      1 | 8585 | `	case 'w':` |
|      - | 8586 | `		/*	Day of the week (0 on Sunday) */` |
|      3 | 8587 | `		iVal = sTm.tm_wday;` |
|      3 | 8588 | `		break;` |
|      1 | 8589 | `	case 'W': {` |
|      - | 8590 | `		/* ISO-8601 week number of year, weeks starting on Monday */` |
|      - | 8591 | `		static const int aISO8601_local[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|      3 | 8592 | `		iVal = aISO8601_local[sTm.tm_wday % 7 ];` |
|      3 | 8593 | `		break;` |
|      - | 8594 | `			  }` |
|    ! 0 | 8595 | `	case 'y':` |
|      - | 8596 | `		/* Year (2 digits) */` |
|    ! 0 | 8597 | `		iVal = sTm.tm_year % 100;` |
|    ! 0 | 8598 | `		break;` |
|      3 | 8599 | `	case 'Y':` |
|      - | 8600 | `		/* Year (4 digits) */` |
|      7 | 8601 | `		iVal = sTm.tm_year;` |
|      7 | 8602 | `		break;` |
|      1 | 8603 | `	case 'z':` |
|      - | 8604 | `		/* Day of the year */` |
|      3 | 8605 | `		iVal = sTm.tm_yday;` |
|      3 | 8606 | `		break;` |
|      1 | 8607 | `	case 'Z':` |
|      - | 8608 | `		/*Timezone offset in seconds*/` |
|      3 | 8609 | `		iVal = sTm.tm_gmtoff;` |
|      3 | 8610 | `		break;` |
|      1 | 8611 | `	default:` |
|      - | 8612 | `		/* unknown format,throw a warning */` |
|      3 | 8613 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Unknown date format token");` |
|      2 | 8614 | `		break;` |
|      - | 8615 | `	}` |
|      - | 8616 | `	/* Return the time value */` |
|     40 | 8617 | `	ph7_result_int64(pCtx,iVal);` |
|     40 | 8618 | `	return PH7_OK;` |
|     23 | 8619 |  |
|      - | 8620 | `/*` |
|      - | 8621 | ` * int mktime/gmmktime([ int $hour = date("H") [, int $minute = date("i") [, int $second = date("s")` |
|      - | 8622 | ` *  [, int $month = date("n") [, int $day = date("j") [, int $year = date("Y") [, int $is_dst = -1 ]]]]]]] )` |
|      - | 8623 | ` *  Returns the Unix timestamp corresponding to the arguments given. This timestamp is a 64bit integer` |
|      - | 8624 | ` *  containing the number of seconds between the Unix Epoch (January 1 1970 00:00:00 GMT) and the time` |
|      - | 8625 | ` *  specified.` |
|      - | 8626 | ` *  Arguments may be left out in order from right to left; any arguments thus omitted will be set to` |
|      - | 8627 | ` *  the current value according to the local date and time.` |
|      - | 8628 | ` * Parameters` |
|      - | 8629 | ` * $hour` |
|      - | 8630 | ` *  The number of the hour relevant to the start of the day determined by month, day and year.` |
|      - | 8631 | ` *  Negative values reference the hour before midnight of the day in question. Values greater` |
|      - | 8632 | ` *  than 23 reference the appropriate hour in the following day(s).` |
|      - | 8633 | ` * $minute` |
|      - | 8634 | ` *  The number of the minute relevant to the start of the hour. Negative values reference` |
|      - | 8635 | ` *  the minute in the previous hour. Values greater than 59 reference the appropriate minute` |
|      - | 8636 | ` *  in the following hour(s).` |
|      - | 8637 | ` * $second` |
|      - | 8638 | ` *  The number of seconds relevant to the start of the minute. Negative values reference` |
|      - | 8639 | ` *  the second in the previous minute. Values greater than 59 reference the appropriate` |
|      - | 8640 | ` * second in the following minute(s).` |
|      - | 8641 | ` * $month` |
|      - | 8642 | ` *  The number of the month relevant to the end of the previous year. Values 1 to 12 reference` |
|      - | 8643 | ` *  the normal calendar months of the year in question. Values less than 1 (including negative values)` |
|      - | 8644 | ` *  reference the months in the previous year in reverse order, so 0 is December, -1 is November)...` |
|      - | 8645 | ` * $day` |
|      - | 8646 | ` *  The number of the day relevant to the end of the previous month. Values 1 to 28, 29, 30 or 31` |
|      - | 8647 | ` *  (depending upon the month) reference the normal days in the relevant month. Values less than 1` |
|      - | 8648 | ` *  (including negative values) reference the days in the previous month, so 0 is the last day` |
|      - | 8649 | ` *  of the previous month, -1 is the day before that, etc. Values greater than the number of days` |
|      - | 8650 | ` *  in the relevant month reference the appropriate day in the following month(s).` |
|      - | 8651 | ` * $year` |
|      - | 8652 | ` *  The number of the year, may be a two or four digit value, with values between 0-69 mapping` |
|      - | 8653 | ` *  to 2000-2069 and 70-100 to 1970-2000. On systems where time_t is a 32bit signed integer, as` |
|      - | 8654 | ` *  most common today, the valid range for year is somewhere between 1901 and 2038.` |
|      - | 8655 | ` * $is_dst` |
|      - | 8656 | ` *  This parameter can be set to 1 if the time is during daylight savings time (DST), 0 if it is not,` |
|      - | 8657 | ` *  or -1 (the default) if it is unknown whether the time is within daylight savings time or not.` |
|      - | 8658 | ` * Return` |
|      - | 8659 | ` *   mktime() returns the Unix timestamp of the arguments given.` |
|      - | 8660 | ` *   If the arguments are invalid, the function returns FALSE` |
|      - | 8661 | ` */` |
|      8 | 8662 | `static int PH7_builtin_mktime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8663 |  |
|      - | 8664 | `	const char *zFunction;` |
|      9 | 8665 | `	ph7_int64 iVal = 0;` |
|      - | 8666 | `	struct tm *pTm;` |
|      - | 8667 | `	time_t t;` |
|      - | 8668 | `	/* Extract function name */` |
|      9 | 8669 | `	zFunction = ph7_function_name(pCtx);` |
|      - | 8670 | `	/* Get the current time */` |
|      9 | 8671 | `	time(&t);` |
|      9 | 8672 | `	if( zFunction[0] == 'g' /* gmmktime */ ){` |
|      3 | 8673 | `		pTm = gmtime(&t);` |
|      2 | 8674 | `	}else{` |
|      - | 8675 | `		/* localtime */` |
|      7 | 8676 | `		pTm = localtime(&t);` |
|      - | 8677 | `	}` |
|      9 | 8678 | `	if( nArg > 0 ){` |
|      - | 8679 | `		int iTmp;` |
|      - | 8680 | `		/* Hour */` |
|      9 | 8681 | `		iTmp = ph7_value_to_int(apArg[0]);` |
|      9 | 8682 | `		pTm->tm_hour = iTmp;` |
|      9 | 8683 | `		if( nArg > 1 ){` |
|      - | 8684 | `			/* Minutes */` |
|      9 | 8685 | `			iTmp = ph7_value_to_int(apArg[1]);` |
|      9 | 8686 | `			pTm->tm_min = iTmp;` |
|      9 | 8687 | `			if( nArg > 2 ){` |
|      - | 8688 | `				/* Seconds */` |
|      9 | 8689 | `				iTmp = ph7_value_to_int(apArg[2]);` |
|      9 | 8690 | `				pTm->tm_sec = iTmp;` |
|      9 | 8691 | `				if( nArg > 3 ){` |
|      - | 8692 | `					/* Month */` |
|      9 | 8693 | `					iTmp = ph7_value_to_int(apArg[3]);` |
|      9 | 8694 | `					pTm->tm_mon = iTmp - 1;` |
|      9 | 8695 | `					if( nArg > 4 ){` |
|      - | 8696 | `						/* mday */` |
|      9 | 8697 | `						iTmp = ph7_value_to_int(apArg[4]);` |
|      9 | 8698 | `						pTm->tm_mday = iTmp;` |
|      9 | 8699 | `						if( nArg > 5 ){` |
|      - | 8700 | `							/* Year */` |
|      9 | 8701 | `							iTmp = ph7_value_to_int(apArg[5]);` |
|      9 | 8702 | `							if( iTmp > 1900 ){` |
|      9 | 8703 | `								iTmp -= 1900;` |
|      4 | 8704 | `							}` |
|      9 | 8705 | `							pTm->tm_year = iTmp;` |
|      9 | 8706 | `							if( nArg > 6 ){` |
|      - | 8707 | `								/* is_dst */` |
|    ! 0 | 8708 | `								iTmp = ph7_value_to_bool(apArg[6]);` |
|    ! 0 | 8709 | `								pTm->tm_isdst = iTmp;` |
|    ! 0 | 8710 | `							}` |
|      4 | 8711 | `						}` |
|      4 | 8712 | `					}` |
|      4 | 8713 | `				}` |
|      4 | 8714 | `			}` |
|      4 | 8715 | `		}` |
|      4 | 8716 | `	}` |
|      - | 8717 | `	/* Make the time */` |
|      9 | 8718 | `	iVal = (ph7_int64)mktime(pTm);` |
|      - | 8719 | `	/* Return the timesatmp as a 64bit integer */` |
|      9 | 8720 | `	ph7_result_int64(pCtx,iVal);` |
|      9 | 8721 | `	return PH7_OK;` |
|      1 | 8722 |  |
|      - | 8723 | `/*` |
|      - | 8724 | ` * Section:` |
|      - | 8725 | ` *    URL handling Functions.` |
|      - | 8726 | ` * Status:` |
|      - | 8727 | ` *    Stable.` |
|      - | 8728 | ` */` |
|      - | 8729 | `/*` |
|      - | 8730 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 8731 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 8732 | ` */` |
|   1026 | 8733 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 8734 |  |
|      - | 8735 | `	/* Store in the call context result buffer */` |
|   1028 | 8736 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 8737 | `	return SXRET_OK;` |
|      2 | 8738 |  |
|      - | 8739 | `/*` |
|      - | 8740 | ` * string base64_encode(string $data)` |
|      - | 8741 | ` * string convert_uuencode(string $data)` |
|      - | 8742 | ` *  Encodes data with MIME base64` |
|      - | 8743 | ` * Parameter` |
|      - | 8744 | ` *  $data` |
|      - | 8745 | ` *    Data to encode` |
|      - | 8746 | ` * Return` |
|      - | 8747 | ` *  Encoded data or FALSE on failure.` |
|      - | 8748 | ` */` |
|     10 | 8749 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8750 |  |
|      - | 8751 | `	const char *zIn;` |
|      - | 8752 | `	int nLen;` |
|     11 | 8753 | `	if( nArg < 1 ){` |
|      - | 8754 | `		/* Missing arguments,return FALSE */` |
|      5 | 8755 | `		ph7_result_bool(pCtx,0);` |
|      5 | 8756 | `		return PH7_OK;` |
|      - | 8757 | `	}` |
|      - | 8758 | `	/* Extract the input string */` |
|      7 | 8759 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8760 | `	if( nLen < 1 ){` |
|      - | 8761 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8762 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8763 | `		return PH7_OK;` |
|      - | 8764 | `	}` |
|      - | 8765 | `	/* Perform the BASE64 encoding */` |
|      7 | 8766 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 8767 | `	return PH7_OK;` |
|      6 | 8768 |  |
|      - | 8769 | `/*` |
|      - | 8770 | ` * string base64_decode(string $data)` |
|      - | 8771 | ` * string convert_uudecode(string $data)` |
|      - | 8772 | ` *  Decodes data encoded with MIME base64` |
|      - | 8773 | ` * Parameter` |
|      - | 8774 | ` *  $data` |
|      - | 8775 | ` *    Encoded data.` |
|      - | 8776 | ` * Return` |
|      - | 8777 | ` *  Returns the original data or FALSE on failure.` |
|      - | 8778 | ` */` |
|     36 | 8779 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 8780 |  |
|      - | 8781 | `	const char *zIn;` |
|      - | 8782 | `	int nLen;` |
|     38 | 8783 | `	if( nArg < 1 ){` |
|      - | 8784 | `		/* Missing arguments,return FALSE */` |
|      3 | 8785 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8786 | `		return PH7_OK;` |
|      - | 8787 | `	}` |
|      - | 8788 | `	/* Extract the input string */` |
|     36 | 8789 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 8790 | `	if( nLen < 1 ){` |
|      - | 8791 | `		/* Nothing to process,return FALSE */` |
|      3 | 8792 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8793 | `		return PH7_OK;` |
|      - | 8794 | `	}` |
|      - | 8795 | `	/* Perform the BASE64 decoding */` |
|     34 | 8796 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 8797 | `	return PH7_OK;` |
|     20 | 8798 |  |
|      - | 8799 | `/*` |
|      - | 8800 | ` * string urlencode(string $str)` |
|      - | 8801 | ` *  URL encoding` |
|      - | 8802 | ` * Parameter` |
|      - | 8803 | ` *  $data` |
|      - | 8804 | ` *   Input string.` |
|      - | 8805 | ` * Return` |
|      - | 8806 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 8807 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 8808 | ` *  encoded as plus (+) signs.` |
|      - | 8809 | ` */` |
|      6 | 8810 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8811 |  |
|      - | 8812 | `	const char *zIn;` |
|      - | 8813 | `	int nLen;` |
|      7 | 8814 | `	if( nArg < 1 ){` |
|      - | 8815 | `		/* Missing arguments,return FALSE */` |
|      3 | 8816 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8817 | `		return PH7_OK;` |
|      - | 8818 | `	}` |
|      - | 8819 | `	/* Extract the input string */` |
|      5 | 8820 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 8821 | `	if( nLen < 1 ){` |
|      - | 8822 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8823 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8824 | `		return PH7_OK;` |
|      - | 8825 | `	}` |
|      - | 8826 | `	/* Perform the URL encoding */` |
|      5 | 8827 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 8828 | `	return PH7_OK;` |
|      4 | 8829 |  |
|      - | 8830 | `/*` |
|      - | 8831 | ` * string urldecode(string $str)` |
|      - | 8832 | ` *  Decodes any %## encoding in the given string.` |
|      - | 8833 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 8834 | ` * Parameter` |
|      - | 8835 | ` *  $data` |
|      - | 8836 | ` *    Input string.` |
|      - | 8837 | ` * Return` |
|      - | 8838 | ` *  Decoded URL or FALSE on failure.` |
|      - | 8839 | ` */` |
|      8 | 8840 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8841 |  |
|      - | 8842 | `	const char *zIn;` |
|      - | 8843 | `	int nLen;` |
|      9 | 8844 | `	if( nArg < 1 ){` |
|      - | 8845 | `		/* Missing arguments,return FALSE */` |
|      3 | 8846 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8847 | `		return PH7_OK;` |
|      - | 8848 | `	}` |
|      - | 8849 | `	/* Extract the input string */` |
|      7 | 8850 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8851 | `	if( nLen < 1 ){` |
|      - | 8852 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8853 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8854 | `		return PH7_OK;` |
|      - | 8855 | `	}` |
|      - | 8856 | `	/* Perform the URL decoding */` |
|      7 | 8857 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 8858 | `	return PH7_OK;` |
|      5 | 8859 |  |
|      - | 8860 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8861 | `/* Table of the built-in functions */` |
|      - | 8862 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 8863 | `	   /* Variable handling functions */` |
|      - | 8864 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 8865 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 8866 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 8867 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 8868 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 8869 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 8870 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 8871 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 8872 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 8873 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 8874 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 8875 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 8876 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 8877 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 8878 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 8879 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 8880 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 8881 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 8882 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 8883 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8884 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 8885 | `	   /* Math functions */` |
|      - | 8886 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 8887 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 8888 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 8889 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 8890 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 8891 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 8892 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 8893 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 8894 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 8895 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 8896 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 8897 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 8898 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 8899 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 8900 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 8901 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 8902 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 8903 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 8904 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 8905 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 8906 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 8907 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 8908 | `	{ "round",    PH7_builtin_round        },` |
|      - | 8909 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 8910 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 8911 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 8912 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 8913 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 8914 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 8915 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 8916 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 8917 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8918 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8919 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 8920 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8921 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8922 | `	   /* String handling functions */` |
|      - | 8923 |  |
|      - | 8924 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 8925 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 8926 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 8927 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 8928 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 8929 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 8930 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 8931 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 8932 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 8933 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 8934 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 8935 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 8936 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 8937 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 8938 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 8939 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 8940 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 8941 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 8942 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 8943 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 8944 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 8945 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 8946 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 8947 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 8948 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 8949 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 8950 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 8951 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 8952 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 8953 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 8954 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 8955 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 8956 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 8957 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 8958 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 8959 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 8960 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 8961 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 8962 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 8963 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 8964 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 8965 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 8966 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 8967 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 8968 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 8969 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 8970 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 8971 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 8972 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 8973 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8974 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8975 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 8976 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 8977 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 8978 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 8979 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8980 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8981 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 8982 |  |
|      - | 8983 |  |
|      - | 8984 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 8985 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 8986 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 8987 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 8988 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 8989 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8990 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8991 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 8992 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 8993 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8994 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8995 |  |
|      - | 8996 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 8997 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 8998 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 8999 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 9000 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 9001 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 9002 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 9003 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 9004 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 9005 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 9006 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 9007 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 9008 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9009 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9010 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 9011 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9012 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9013 |  |
|      - | 9014 | `	         /* Ctype functions */` |
|      - | 9015 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 9016 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 9017 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 9018 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 9019 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 9020 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 9021 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 9022 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 9023 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 9024 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 9025 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 9026 | `	         /* Time functions */` |
|      - | 9027 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 9028 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 9029 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 9030 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 9031 | `	{ "date",        PH7_builtin_date         },` |
|      - | 9032 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 9033 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 9034 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 9035 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 9036 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 9037 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 9038 | `	        /* URL functions */` |
|      - | 9039 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 9040 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 9041 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 9042 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 9043 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 9044 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 9045 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 9046 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 9047 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9048 | `};` |
|      - | 9049 | `/*` |
|      - | 9050 | ` * Register the built-in functions defined above,the array functions` |
|      - | 9051 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 9052 | ` */` |
|   1664 | 9053 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      2 | 9054 |  |
|      - | 9055 | `	sxu32 n;` |
| 254594 | 9056 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 252930 | 9057 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 126466 | 9058 | `	}` |
|      - | 9059 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   1666 | 9060 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 9061 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   1666 | 9062 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   1666 | 9063 |  |
|      - | 9064 |  |
