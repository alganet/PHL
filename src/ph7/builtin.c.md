# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3685/4322 lines (85.26%)

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
|  15302 |  269 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  270 |  |
|  15304 |  271 | `	int res = 1; /* Assume empty by default */` |
|  15304 |  272 | `	if( nArg > 0 ){` |
|  15302 |  273 | `		res = ph7_value_is_empty(apArg[0]);` |
|   7650 |  274 | `	}` |
|  15304 |  275 | `	ph7_result_bool(pCtx,res);` |
|  15304 |  276 | `	return PH7_OK;` |
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
|      4 |  352 | `static int PH7_builtin_floor(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  353 |  |
|      - |  354 | `	double r,x;` |
|      5 |  355 | `	if( nArg < 1 ){` |
|      - |  356 | `		/* Missing argument,return 0 */` |
|      3 |  357 | `		ph7_result_int(pCtx,0);` |
|      3 |  358 | `		return PH7_OK;` |
|      - |  359 | `	}` |
|      3 |  360 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  361 | `	/* Perform the requested operation */` |
|      3 |  362 | `	r = floor(x);` |
|      - |  363 | `	/* store the result back */` |
|      3 |  364 | `	ph7_result_double(pCtx,r);` |
|      3 |  365 | `	return PH7_OK;` |
|      3 |  366 |  |
|      - |  367 | `/*` |
|      - |  368 | ` * float cos(float $arg )` |
|      - |  369 | ` *  Cosine.` |
|      - |  370 | ` * Parameter` |
|      - |  371 | ` *  The number to process.` |
|      - |  372 | ` * Return` |
|      - |  373 | ` *  The cosine of arg.` |
|      - |  374 | ` */` |
|      4 |  375 | `static int PH7_builtin_cos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  376 |  |
|      - |  377 | `	double r,x;` |
|      5 |  378 | `	if( nArg < 1 ){` |
|      - |  379 | `		/* Missing argument,return 0 */` |
|      3 |  380 | `		ph7_result_int(pCtx,0);` |
|      3 |  381 | `		return PH7_OK;` |
|      - |  382 | `	}` |
|      3 |  383 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  384 | `	/* Perform the requested operation */` |
|      3 |  385 | `	r = cos(x);` |
|      - |  386 | `	/* store the result back */` |
|      3 |  387 | `	ph7_result_double(pCtx,r);` |
|      3 |  388 | `	return PH7_OK;` |
|      3 |  389 |  |
|      - |  390 | `/*` |
|      - |  391 | ` * float acos(float $arg )` |
|      - |  392 | ` *  Arc cosine.` |
|      - |  393 | ` * Parameter` |
|      - |  394 | ` *  The number to process.` |
|      - |  395 | ` * Return` |
|      - |  396 | ` *  The arc cosine of arg.` |
|      - |  397 | ` */` |
|     18 |  398 | `static int PH7_builtin_acos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  399 |  |
|      - |  400 | `	double r,x;` |
|     19 |  401 | `	if( nArg < 1 ){` |
|      - |  402 | `		/* Missing argument,return 0 */` |
|      5 |  403 | `		ph7_result_int(pCtx,0);` |
|      5 |  404 | `		return PH7_OK;` |
|      - |  405 | `	}` |
|     15 |  406 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  407 | `	/* Perform the requested operation */` |
|     15 |  408 | `	r = acos(x);` |
|      - |  409 | `	/* store the result back */` |
|     15 |  410 | `	ph7_result_double(pCtx,r);` |
|     15 |  411 | `	return PH7_OK;` |
|     10 |  412 |  |
|      - |  413 | `/*` |
|      - |  414 | ` * float cosh(float $arg )` |
|      - |  415 | ` *  Hyperbolic cosine.` |
|      - |  416 | ` * Parameter` |
|      - |  417 | ` *  The number to process.` |
|      - |  418 | ` * Return` |
|      - |  419 | ` *  The hyperbolic cosine of arg.` |
|      - |  420 | ` */` |
|     18 |  421 | `static int PH7_builtin_cosh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  422 |  |
|      - |  423 | `	double r,x;` |
|     19 |  424 | `	if( nArg < 1 ){` |
|      - |  425 | `		/* Missing argument,return 0 */` |
|      3 |  426 | `		ph7_result_int(pCtx,0);` |
|      3 |  427 | `		return PH7_OK;` |
|      - |  428 | `	}` |
|     17 |  429 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  430 | `	/* Perform the requested operation */` |
|     17 |  431 | `	r = cosh(x);` |
|      - |  432 | `	/* store the result back */` |
|     17 |  433 | `	ph7_result_double(pCtx,r);` |
|     17 |  434 | `	return PH7_OK;` |
|     10 |  435 |  |
|      - |  436 | `/*` |
|      - |  437 | ` * float sin(float $arg )` |
|      - |  438 | ` *  Sine.` |
|      - |  439 | ` * Parameter` |
|      - |  440 | ` *  The number to process.` |
|      - |  441 | ` * Return` |
|      - |  442 | ` *  The sine of arg.` |
|      - |  443 | ` */` |
|      8 |  444 | `static int PH7_builtin_sin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  445 |  |
|      - |  446 | `	double r,x;` |
|      9 |  447 | `	if( nArg < 1 ){` |
|      - |  448 | `		/* Missing argument,return 0 */` |
|      7 |  449 | `		ph7_result_int(pCtx,0);` |
|      7 |  450 | `		return PH7_OK;` |
|      - |  451 | `	}` |
|      3 |  452 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  453 | `	/* Perform the requested operation */` |
|      3 |  454 | `	r = sin(x);` |
|      - |  455 | `	/* store the result back */` |
|      3 |  456 | `	ph7_result_double(pCtx,r);` |
|      3 |  457 | `	return PH7_OK;` |
|      5 |  458 |  |
|      - |  459 | `/*` |
|      - |  460 | ` * float asin(float $arg )` |
|      - |  461 | ` *  Arc sine.` |
|      - |  462 | ` * Parameter` |
|      - |  463 | ` *  The number to process.` |
|      - |  464 | ` * Return` |
|      - |  465 | ` *  The arc sine of arg.` |
|      - |  466 | ` */` |
|     14 |  467 | `static int PH7_builtin_asin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  468 |  |
|      - |  469 | `	double r,x;` |
|     15 |  470 | `	if( nArg < 1 ){` |
|      - |  471 | `		/* Missing argument,return 0 */` |
|      3 |  472 | `		ph7_result_int(pCtx,0);` |
|      3 |  473 | `		return PH7_OK;` |
|      - |  474 | `	}` |
|     13 |  475 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  476 | `	/* Perform the requested operation */` |
|     13 |  477 | `	r = asin(x);` |
|      - |  478 | `	/* store the result back */` |
|     13 |  479 | `	ph7_result_double(pCtx,r);` |
|     13 |  480 | `	return PH7_OK;` |
|      8 |  481 |  |
|      - |  482 | `/*` |
|      - |  483 | ` * float sinh(float $arg )` |
|      - |  484 | ` *  Hyperbolic sine.` |
|      - |  485 | ` * Parameter` |
|      - |  486 | ` *  The number to process.` |
|      - |  487 | ` * Return` |
|      - |  488 | ` *  The hyperbolic sine of arg.` |
|      - |  489 | ` */` |
|     20 |  490 | `static int PH7_builtin_sinh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  491 |  |
|      - |  492 | `	double r,x;` |
|     21 |  493 | `	if( nArg < 1 ){` |
|      - |  494 | `		/* Missing argument,return 0 */` |
|      3 |  495 | `		ph7_result_int(pCtx,0);` |
|      3 |  496 | `		return PH7_OK;` |
|      - |  497 | `	}` |
|     19 |  498 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  499 | `	/* Perform the requested operation */` |
|     19 |  500 | `	r = sinh(x);` |
|      - |  501 | `	/* store the result back */` |
|     19 |  502 | `	ph7_result_double(pCtx,r);` |
|     19 |  503 | `	return PH7_OK;` |
|     11 |  504 |  |
|      - |  505 | `/*` |
|      - |  506 | ` * float ceil(float $arg )` |
|      - |  507 | ` *  Round fractions up.` |
|      - |  508 | ` * Parameter` |
|      - |  509 | ` *  The number to process.` |
|      - |  510 | ` * Return` |
|      - |  511 | ` *  The next highest integer value by rounding up value if necessary.` |
|      - |  512 | ` */` |
|      6 |  513 | `static int PH7_builtin_ceil(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  514 |  |
|      - |  515 | `	double r,x;` |
|      7 |  516 | `	if( nArg < 1 ){` |
|      - |  517 | `		/* Missing argument,return 0 */` |
|      5 |  518 | `		ph7_result_int(pCtx,0);` |
|      5 |  519 | `		return PH7_OK;` |
|      - |  520 | `	}` |
|      3 |  521 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  522 | `	/* Perform the requested operation */` |
|      3 |  523 | `	r = ceil(x);` |
|      - |  524 | `	/* store the result back */` |
|      3 |  525 | `	ph7_result_double(pCtx,r);` |
|      3 |  526 | `	return PH7_OK;` |
|      4 |  527 |  |
|      - |  528 | `/*` |
|      - |  529 | ` * float tan(float $arg )` |
|      - |  530 | ` *  Tangent.` |
|      - |  531 | ` * Parameter` |
|      - |  532 | ` *  The number to process.` |
|      - |  533 | ` * Return` |
|      - |  534 | ` *  The tangent of arg.` |
|      - |  535 | ` */` |
|      6 |  536 | `static int PH7_builtin_tan(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  537 |  |
|      - |  538 | `	double r,x;` |
|      7 |  539 | `	if( nArg < 1 ){` |
|      - |  540 | `		/* Missing argument,return 0 */` |
|      3 |  541 | `		ph7_result_int(pCtx,0);` |
|      3 |  542 | `		return PH7_OK;` |
|      - |  543 | `	}` |
|      5 |  544 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  545 | `	/* Perform the requested operation */` |
|      5 |  546 | `	r = tan(x);` |
|      - |  547 | `	/* store the result back */` |
|      5 |  548 | `	ph7_result_double(pCtx,r);` |
|      5 |  549 | `	return PH7_OK;` |
|      4 |  550 |  |
|      - |  551 | `/*` |
|      - |  552 | ` * float atan(float $arg )` |
|      - |  553 | ` *  Arc tangent.` |
|      - |  554 | ` * Parameter` |
|      - |  555 | ` *  The number to process.` |
|      - |  556 | ` * Return` |
|      - |  557 | ` *  The arc tangent of arg.` |
|      - |  558 | ` */` |
|     16 |  559 | `static int PH7_builtin_atan(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  560 |  |
|      - |  561 | `	double r,x;` |
|     17 |  562 | `	if( nArg < 1 ){` |
|      - |  563 | `		/* Missing argument,return 0 */` |
|      5 |  564 | `		ph7_result_int(pCtx,0);` |
|      5 |  565 | `		return PH7_OK;` |
|      - |  566 | `	}` |
|     13 |  567 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  568 | `	/* Perform the requested operation */` |
|     13 |  569 | `	r = atan(x);` |
|      - |  570 | `	/* store the result back */` |
|     13 |  571 | `	ph7_result_double(pCtx,r);` |
|     13 |  572 | `	return PH7_OK;` |
|      9 |  573 |  |
|      - |  574 | `/*` |
|      - |  575 | ` * float tanh(float $arg )` |
|      - |  576 | ` *  Hyperbolic tangent.` |
|      - |  577 | ` * Parameter` |
|      - |  578 | ` *  The number to process.` |
|      - |  579 | ` * Return` |
|      - |  580 | ` *  The Hyperbolic tangent of arg.` |
|      - |  581 | ` */` |
|     20 |  582 | `static int PH7_builtin_tanh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  583 |  |
|      - |  584 | `	double r,x;` |
|     21 |  585 | `	if( nArg < 1 ){` |
|      - |  586 | `		/* Missing argument,return 0 */` |
|      3 |  587 | `		ph7_result_int(pCtx,0);` |
|      3 |  588 | `		return PH7_OK;` |
|      - |  589 | `	}` |
|     19 |  590 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  591 | `	/* Perform the requested operation */` |
|     19 |  592 | `	r = tanh(x);` |
|      - |  593 | `	/* store the result back */` |
|     19 |  594 | `	ph7_result_double(pCtx,r);` |
|     19 |  595 | `	return PH7_OK;` |
|     11 |  596 |  |
|      - |  597 | `/*` |
|      - |  598 | ` * float atan2(float $y,float $x)` |
|      - |  599 | ` *  Arc tangent of two variable.` |
|      - |  600 | ` * Parameter` |
|      - |  601 | ` *  $y = Dividend parameter.` |
|      - |  602 | ` *  $x = Divisor parameter.` |
|      - |  603 | ` * Return` |
|      - |  604 | ` *  The arc tangent of y/x in radian.` |
|      - |  605 | ` */` |
|     10 |  606 | `static int PH7_builtin_atan2(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  607 |  |
|      - |  608 | `	double r,x,y;` |
|     11 |  609 | `	if( nArg < 2 ){` |
|      - |  610 | `		/* Missing arguments,return 0 */` |
|      5 |  611 | `		ph7_result_int(pCtx,0);` |
|      5 |  612 | `		return PH7_OK;` |
|      - |  613 | `	}` |
|      7 |  614 | `	y = ph7_value_to_double(apArg[0]);` |
|      7 |  615 | `	x = ph7_value_to_double(apArg[1]);` |
|      - |  616 | `	/* Perform the requested operation */` |
|      7 |  617 | `	r = atan2(y,x);` |
|      - |  618 | `	/* store the result back */` |
|      7 |  619 | `	ph7_result_double(pCtx,r);` |
|      7 |  620 | `	return PH7_OK;` |
|      6 |  621 |  |
|      - |  622 | `/*` |
|      - |  623 | ` * float/int64 abs(float/int64 $arg )` |
|      - |  624 | ` *  Absolute value.` |
|      - |  625 | ` * Parameter` |
|      - |  626 | ` *  The number to process.` |
|      - |  627 | ` * Return` |
|      - |  628 | ` *  The absolute value of number.` |
|      - |  629 | ` */` |
|     76 |  630 | `static int PH7_builtin_abs(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  631 |  |
|      - |  632 | `	int is_float;` |
|     77 |  633 | `	if( nArg < 1 ){` |
|      - |  634 | `		/* Missing argument,return 0 */` |
|      3 |  635 | `		ph7_result_int(pCtx,0);` |
|      3 |  636 | `		return PH7_OK;` |
|      - |  637 | `	}` |
|     75 |  638 | `	is_float = ph7_value_is_float(apArg[0]);` |
|     75 |  639 | `	if( is_float ){` |
|      - |  640 | `		double r,x;` |
|     71 |  641 | `		x = ph7_value_to_double(apArg[0]);` |
|      - |  642 | `		/* Perform the requested operation */` |
|     71 |  643 | `		r = fabs(x);` |
|     71 |  644 | `		ph7_result_double(pCtx,r);` |
|     36 |  645 | `	}else{` |
|      - |  646 | `		int r,x;` |
|      5 |  647 | `		x = ph7_value_to_int(apArg[0]);` |
|      - |  648 | `		/* Perform the requested operation */` |
|      5 |  649 | `		r = abs(x);` |
|      5 |  650 | `		ph7_result_int(pCtx,r);` |
|      - |  651 | `	}` |
|     75 |  652 | `	return PH7_OK;` |
|     39 |  653 |  |
|      - |  654 | `/*` |
|      - |  655 | ` * float log(float $arg,[int/float $base])` |
|      - |  656 | ` *  Natural logarithm.` |
|      - |  657 | ` * Parameter` |
|      - |  658 | ` *  $arg: The number to process.` |
|      - |  659 | ` *  $base: The optional logarithmic base to use. (only base-10 is supported)` |
|      - |  660 | ` * Return` |
|      - |  661 | ` *  The logarithm of arg to base, if given, or the natural logarithm.` |
|      - |  662 | ` * Note:` |
|      - |  663 | ` *  only Natural log and base-10 log are supported.` |
|      - |  664 | ` */` |
|     14 |  665 | `static int PH7_builtin_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  666 |  |
|      - |  667 | `	double r,x;` |
|     15 |  668 | `	if( nArg < 1 ){` |
|      - |  669 | `		/* Missing argument,return 0 */` |
|      3 |  670 | `		ph7_result_int(pCtx,0);` |
|      3 |  671 | `		return PH7_OK;` |
|      - |  672 | `	}` |
|     13 |  673 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  674 | `	/* Perform the requested operation */` |
|     13 |  675 | `	if( nArg == 2 && ph7_value_is_numeric(apArg[1]) && ph7_value_to_int(apArg[1]) == 10 ){` |
|      - |  676 | `		/* Base-10 log */` |
|      5 |  677 | `		r = log10(x);` |
|      3 |  678 | `	}else{` |
|      9 |  679 | `		r = log(x);` |
|      - |  680 | `	}` |
|      - |  681 | `	/* store the result back */` |
|     13 |  682 | `	ph7_result_double(pCtx,r);` |
|     13 |  683 | `	return PH7_OK;` |
|      8 |  684 |  |
|      - |  685 | `/*` |
|      - |  686 | ` * float log10(float $arg )` |
|      - |  687 | ` *  Base-10 logarithm.` |
|      - |  688 | ` * Parameter` |
|      - |  689 | ` *  The number to process.` |
|      - |  690 | ` * Return` |
|      - |  691 | ` *  The Base-10 logarithm of the given number.` |
|      - |  692 | ` */` |
|     16 |  693 | `static int PH7_builtin_log10(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  694 |  |
|      - |  695 | `	double r,x;` |
|     17 |  696 | `	if( nArg < 1 ){` |
|      - |  697 | `		/* Missing argument,return 0 */` |
|      3 |  698 | `		ph7_result_int(pCtx,0);` |
|      3 |  699 | `		return PH7_OK;` |
|      - |  700 | `	}` |
|     15 |  701 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  702 | `	/* Perform the requested operation */` |
|     15 |  703 | `	r = log10(x);` |
|      - |  704 | `	/* store the result back */` |
|     15 |  705 | `	ph7_result_double(pCtx,r);` |
|     15 |  706 | `	return PH7_OK;` |
|      9 |  707 |  |
|      - |  708 | `/*` |
|      - |  709 | ` * number pow(number $base,number $exp)` |
|      - |  710 | ` *  Exponential expression.` |
|      - |  711 | ` * Parameter` |
|      - |  712 | ` *  base` |
|      - |  713 | ` *  The base to use.` |
|      - |  714 | ` * exp` |
|      - |  715 | ` *  The exponent.` |
|      - |  716 | ` * Return` |
|      - |  717 | ` *  base raised to the power of exp.` |
|      - |  718 | ` *  If the result can be represented as integer it will be returned` |
|      - |  719 | ` *  as type integer, else it will be returned as type float.` |
|      - |  720 | ` */` |
|      8 |  721 | `static int PH7_builtin_pow(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  722 |  |
|      - |  723 | `	double r,x,y;` |
|      9 |  724 | `	if( nArg < 1 ){` |
|      - |  725 | `		/* Missing argument,return 0 */` |
|      5 |  726 | `		ph7_result_int(pCtx,0);` |
|      5 |  727 | `		return PH7_OK;` |
|      - |  728 | `	}` |
|      5 |  729 | `	x = ph7_value_to_double(apArg[0]);` |
|      5 |  730 | `	y = ph7_value_to_double(apArg[1]);` |
|      - |  731 | `	/* Perform the requested operation */` |
|      5 |  732 | `	r = pow(x,y);` |
|      5 |  733 | `	ph7_result_double(pCtx,r);` |
|      5 |  734 | `	return PH7_OK;` |
|      5 |  735 |  |
|      - |  736 | `/*` |
|      - |  737 | ` * float pi(void)` |
|      - |  738 | ` *  Returns an approximation of pi.` |
|      - |  739 | ` * Note` |
|      - |  740 | ` *  you can use the M_PI constant which yields identical results to pi().` |
|      - |  741 | ` * Return` |
|      - |  742 | ` *  The value of pi as float.` |
|      - |  743 | ` */` |
|      2 |  744 | `static int PH7_builtin_pi(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  745 |  |
|      1 |  746 | `	SXUNUSED(nArg); /* cc warning */` |
|      1 |  747 | `	SXUNUSED(apArg);` |
|      3 |  748 | `	ph7_result_double(pCtx,PH7_PI);` |
|      3 |  749 | `	return PH7_OK;` |
|      1 |  750 |  |
|      - |  751 | `/*` |
|      - |  752 | ` * float fmod(float $x,float $y)` |
|      - |  753 | ` *  Returns the floating point remainder (modulo) of the division of the arguments.` |
|      - |  754 | ` * Parameters` |
|      - |  755 | ` * $x` |
|      - |  756 | ` *  The dividend` |
|      - |  757 | ` * $y` |
|      - |  758 | ` *  The divisor` |
|      - |  759 | ` * Return` |
|      - |  760 | ` *  The floating point remainder of x/y.` |
|      - |  761 | ` */` |
|      8 |  762 | `static int PH7_builtin_fmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  763 |  |
|      - |  764 | `	double x,y,r;` |
|      9 |  765 | `	if( nArg < 2 ){` |
|      - |  766 | `		/* Missing arguments */` |
|      7 |  767 | `		ph7_result_double(pCtx,0);` |
|      7 |  768 | `		return PH7_OK;` |
|      - |  769 | `	}` |
|      - |  770 | `	/* Extract given arguments */` |
|      3 |  771 | `	x = ph7_value_to_double(apArg[0]);` |
|      3 |  772 | `	y = ph7_value_to_double(apArg[1]);` |
|      - |  773 | `	/* Perform the requested operation */` |
|      3 |  774 | `	r = fmod(x,y);` |
|      - |  775 | `	/* Processing result */` |
|      3 |  776 | `	ph7_result_double(pCtx,r);` |
|      3 |  777 | `	return PH7_OK;` |
|      5 |  778 |  |
|      - |  779 | `/*` |
|      - |  780 | ` * float hypot(float $x,float $y)` |
|      - |  781 | ` *  Calculate the length of the hypotenuse of a right-angle triangle .` |
|      - |  782 | ` * Parameters` |
|      - |  783 | ` * $x` |
|      - |  784 | ` *  Length of first side` |
|      - |  785 | ` * $y` |
|      - |  786 | ` *  Length of first side` |
|      - |  787 | ` * Return` |
|      - |  788 | ` *  Calculated length of the hypotenuse.` |
|      - |  789 | ` */` |
|      6 |  790 | `static int PH7_builtin_hypot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  791 |  |
|      - |  792 | `	double x,y,r;` |
|      7 |  793 | `	if( nArg < 2 ){` |
|      - |  794 | `		/* Missing arguments */` |
|      5 |  795 | `		ph7_result_double(pCtx,0);` |
|      5 |  796 | `		return PH7_OK;` |
|      - |  797 | `	}` |
|      - |  798 | `	/* Extract given arguments */` |
|      3 |  799 | `	x = ph7_value_to_double(apArg[0]);` |
|      3 |  800 | `	y = ph7_value_to_double(apArg[1]);` |
|      - |  801 | `	/* Perform the requested operation */` |
|      3 |  802 | `	r = hypot(x,y);` |
|      - |  803 | `	/* Processing result */` |
|      3 |  804 | `	ph7_result_double(pCtx,r);` |
|      3 |  805 | `	return PH7_OK;` |
|      4 |  806 |  |
|      - |  807 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - |  808 | `/*` |
|      - |  809 | ` * float round ( float $val [, int $precision = 0 [, int $mode = PHP_ROUND_HALF_UP ]] )` |
|      - |  810 | ` *  Exponential expression.` |
|      - |  811 | ` * Parameter` |
|      - |  812 | ` *  $val` |
|      - |  813 | ` *   The value to round.` |
|      - |  814 | ` * $precision` |
|      - |  815 | ` *   The optional number of decimal digits to round to.` |
|      - |  816 | ` * $mode` |
|      - |  817 | ` *   One of PHP_ROUND_HALF_UP, PHP_ROUND_HALF_DOWN, PHP_ROUND_HALF_EVEN, or PHP_ROUND_HALF_ODD.` |
|      - |  818 | ` *   (not supported).` |
|      - |  819 | ` * Return` |
|      - |  820 | ` *  The rounded value.` |
|      - |  821 | ` */` |
|     20 |  822 | `static int PH7_builtin_round(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  823 |  |
|     21 |  824 | `	int n = 0;` |
|      - |  825 | `	double r;` |
|     21 |  826 | `	if( nArg < 1 ){` |
|      - |  827 | `		/* Missing argument,return 0 */` |
|      5 |  828 | `		ph7_result_int(pCtx,0);` |
|      5 |  829 | `		return PH7_OK;` |
|      - |  830 | `	}` |
|      - |  831 | `	/* Extract the precision if available */` |
|     17 |  832 | `	if( nArg > 1 ){` |
|      5 |  833 | `		n = ph7_value_to_int(apArg[1]);` |
|      5 |  834 | `		if( n>30 ){` |
|      3 |  835 | `			n = 30;` |
|      1 |  836 | `		}` |
|      5 |  837 | `		if( n<0 ){` |
|      3 |  838 | `			n = 0;` |
|      1 |  839 | `		}` |
|      2 |  840 | `	}` |
|     17 |  841 | `	r = ph7_value_to_double(apArg[0]);` |
|      - |  842 | `	/* If Y==0 and X will fit in a 64-bit int,` |
|      - |  843 | `     * handle the rounding directly.Otherwise` |
|      - |  844 | `	 * use our own cutsom printf [i.e:SyBufferFormat()].` |
|      - |  845 | `     */` |
|     17 |  846 | `	if( n==0 && r>=0 && r < (double)(LARGEST_INT64-1) ){` |
|     13 |  847 | `    r = (double)((ph7_int64)(r+0.5));` |
|     11 |  848 | `	}else if( n==0 && r<0 && (-r) < (double)(LARGEST_INT64-1) ){` |
|      3 |  849 | `    r = -(double)((ph7_int64)((-r)+0.5));` |
|      2 |  850 | `  }else{` |
|      - |  851 | `	  char zBuf[256];` |
|      - |  852 | `	  sxu32 nLen;` |
|      3 |  853 | `	  nLen = SyBufferFormat(zBuf,sizeof(zBuf),"%.*f",n,r);` |
|      - |  854 | `	  /* Convert the string to real number */` |
|      3 |  855 | `	  SyStrToReal(zBuf,nLen,(void *)&r,0);` |
|      - |  856 | `  }` |
|      - |  857 | `  /* Return thr rounded value */` |
|     17 |  858 | `  ph7_result_double(pCtx,r);` |
|     17 |  859 | `  return PH7_OK;` |
|     11 |  860 |  |
|      - |  861 | `/*` |
|      - |  862 | ` * string dechex(int $number)` |
|      - |  863 | ` *  Decimal to hexadecimal.` |
|      - |  864 | ` * Parameters` |
|      - |  865 | ` *  $number` |
|      - |  866 | ` *   Decimal value to convert` |
|      - |  867 | ` * Return` |
|      - |  868 | ` *  Hexadecimal string representation of number` |
|      - |  869 | ` */` |
|      6 |  870 | `static int PH7_builtin_dechex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  871 |  |
|      - |  872 | `	int iVal;` |
|      7 |  873 | `	if( nArg < 1 ){` |
|      - |  874 | `		/* Missing arguments,return null */` |
|      5 |  875 | `		ph7_result_null(pCtx);` |
|      5 |  876 | `		return PH7_OK;` |
|      - |  877 | `	}` |
|      - |  878 | `	/* Extract the given number */` |
|      3 |  879 | `	iVal = ph7_value_to_int(apArg[0]);` |
|      - |  880 | `	/* Format */` |
|      3 |  881 | `	ph7_result_string_format(pCtx,"%x",iVal);` |
|      3 |  882 | `	return PH7_OK;` |
|      4 |  883 |  |
|      - |  884 | `/*` |
|      - |  885 | ` * string decoct(int $number)` |
|      - |  886 | ` *  Decimal to Octal.` |
|      - |  887 | ` * Parameters` |
|      - |  888 | ` *  $number` |
|      - |  889 | ` *   Decimal value to convert` |
|      - |  890 | ` * Return` |
|      - |  891 | ` *  Octal string representation of number` |
|      - |  892 | ` */` |
|      8 |  893 | `static int PH7_builtin_decoct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  894 |  |
|      - |  895 | `	int iVal;` |
|      9 |  896 | `	if( nArg < 1 ){` |
|      - |  897 | `		/* Missing arguments,return null */` |
|      3 |  898 | `		ph7_result_null(pCtx);` |
|      3 |  899 | `		return PH7_OK;` |
|      - |  900 | `	}` |
|      - |  901 | `	/* Extract the given number */` |
|      7 |  902 | `	iVal = ph7_value_to_int(apArg[0]);` |
|      - |  903 | `	/* Format */` |
|      7 |  904 | `	ph7_result_string_format(pCtx,"%o",iVal);` |
|      7 |  905 | `	return PH7_OK;` |
|      5 |  906 |  |
|      - |  907 | `/*` |
|      - |  908 | ` * string decbin(int $number)` |
|      - |  909 | ` *  Decimal to binary.` |
|      - |  910 | ` * Parameters` |
|      - |  911 | ` *  $number` |
|      - |  912 | ` *   Decimal value to convert` |
|      - |  913 | ` * Return` |
|      - |  914 | ` *  Binary string representation of number` |
|      - |  915 | ` */` |
|      4 |  916 | `static int PH7_builtin_decbin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  917 |  |
|      - |  918 | `	int iVal;` |
|      5 |  919 | `	if( nArg < 1 ){` |
|      - |  920 | `		/* Missing arguments,return null */` |
|      3 |  921 | `		ph7_result_null(pCtx);` |
|      3 |  922 | `		return PH7_OK;` |
|      - |  923 | `	}` |
|      - |  924 | `	/* Extract the given number */` |
|      3 |  925 | `	iVal = ph7_value_to_int(apArg[0]);` |
|      - |  926 | `	/* Format */` |
|      3 |  927 | `	ph7_result_string_format(pCtx,"%B",iVal);` |
|      3 |  928 | `	return PH7_OK;` |
|      3 |  929 |  |
|      - |  930 | `/*` |
|      - |  931 | ` * int64 hexdec(string $hex_string)` |
|      - |  932 | ` *  Hexadecimal to decimal.` |
|      - |  933 | ` * Parameters` |
|      - |  934 | ` *  $hex_string` |
|      - |  935 | ` *   The hexadecimal string to convert` |
|      - |  936 | ` * Return` |
|      - |  937 | ` *  The decimal representation of hex_string` |
|      - |  938 | ` */` |
|     24 |  939 | `static int PH7_builtin_hexdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  940 |  |
|      - |  941 | `	const char *zString,*zEnd;` |
|      - |  942 | `	ph7_int64 iVal;` |
|      - |  943 | `	int nLen;` |
|     25 |  944 | `	if( nArg < 1 ){` |
|      - |  945 | `		/* Missing arguments,return -1 */` |
|      5 |  946 | `		ph7_result_int(pCtx,-1);` |
|      5 |  947 | `		return PH7_OK;` |
|      - |  948 | `	}` |
|     21 |  949 | `	iVal = 0;` |
|     21 |  950 | `	if( ph7_value_is_string(apArg[0]) ){` |
|      - |  951 | `		/* Extract the given string */` |
|     15 |  952 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|      - |  953 | `		/* Delimit the string */` |
|     15 |  954 | `		zEnd = &zString[nLen];` |
|      - |  955 | `		/* Ignore non hex-stream */` |
|     21 |  956 | `		while( zString < zEnd ){` |
|     21 |  957 | `			if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - |  958 | `				/* UTF-8 stream */` |
|      5 |  959 | `				zString++;` |
|      9 |  960 | `				while( zString < zEnd && (((unsigned char)zString[0] & 0xc0) == 0x80) ){` |
|      5 |  961 | `					zString++;` |
|      1 |  962 | `				}` |
|      3 |  963 | `			}else{` |
|     17 |  964 | `				if( SyisHex(zString[0]) ){` |
|     15 |  965 | `					break;` |
|      - |  966 | `				}` |
|      - |  967 | `				/* Ignore */` |
|      3 |  968 | `				zString++;` |
|      - |  969 | `			}` |
|      1 |  970 | `		}` |
|     15 |  971 | `		if( zString < zEnd ){` |
|      - |  972 | `			/* Cast */` |
|     15 |  973 | `			SyHexStrToInt64(zString,(sxu32)(zEnd-zString),(void *)&iVal,0);` |
|      7 |  974 | `		}` |
|      8 |  975 | `	}else{` |
|      - |  976 | `		/* Extract as a 64-bit integer */` |
|      7 |  977 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|      - |  978 | `	}` |
|      - |  979 | `	/* Return the number */` |
|     21 |  980 | `	ph7_result_int64(pCtx,iVal);` |
|     21 |  981 | `	return PH7_OK;` |
|     13 |  982 |  |
|      - |  983 | `/*` |
|      - |  984 | ` * int64 bindec(string $bin_string)` |
|      - |  985 | ` *  Binary to decimal.` |
|      - |  986 | ` * Parameters` |
|      - |  987 | ` *  $bin_string` |
|      - |  988 | ` *   The binary string to convert` |
|      - |  989 | ` * Return` |
|      - |  990 | ` *  Returns the decimal equivalent of the binary number represented by the binary_string argument.` |
|      - |  991 | ` */` |
|     12 |  992 | `static int PH7_builtin_bindec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  993 |  |
|      - |  994 | `	const char *zString;` |
|      - |  995 | `	ph7_int64 iVal;` |
|      - |  996 | `	int nLen;` |
|     13 |  997 | `	if( nArg < 1 ){` |
|      - |  998 | `		/* Missing arguments,return -1 */` |
|      5 |  999 | `		ph7_result_int(pCtx,-1);` |
|      5 | 1000 | `		return PH7_OK;` |
|      - | 1001 | `	}` |
|      9 | 1002 | `	iVal = 0;` |
|      9 | 1003 | `	if( ph7_value_is_string(apArg[0]) ){` |
|      - | 1004 | `		/* Extract the given string */` |
|      5 | 1005 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 1006 | `		if( nLen > 0 ){` |
|      - | 1007 | `			/* Perform a binary cast */` |
|      5 | 1008 | `			SyBinaryStrToInt64(zString,(sxu32)nLen,(void *)&iVal,0);` |
|      2 | 1009 | `		}` |
|      3 | 1010 | `	}else{` |
|      - | 1011 | `		/* Extract as a 64-bit integer */` |
|      5 | 1012 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|      - | 1013 | `	}` |
|      - | 1014 | `	/* Return the number */` |
|      9 | 1015 | `	ph7_result_int64(pCtx,iVal);` |
|      9 | 1016 | `	return PH7_OK;` |
|      7 | 1017 |  |
|      - | 1018 | `/*` |
|      - | 1019 | ` * int64 octdec(string $oct_string)` |
|      - | 1020 | ` *  Octal to decimal.` |
|      - | 1021 | ` * Parameters` |
|      - | 1022 | ` *  $oct_string` |
|      - | 1023 | ` *   The octal string to convert` |
|      - | 1024 | ` * Return` |
|      - | 1025 | ` *  Returns the decimal equivalent of the octal number represented by the octal_string argument.` |
|      - | 1026 | ` */` |
|      6 | 1027 | `static int PH7_builtin_octdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1028 |  |
|      - | 1029 | `	const char *zString;` |
|      - | 1030 | `	ph7_int64 iVal;` |
|      - | 1031 | `	int nLen;` |
|      7 | 1032 | `	if( nArg < 1 ){` |
|      - | 1033 | `		/* Missing arguments,return -1 */` |
|      3 | 1034 | `		ph7_result_int(pCtx,-1);` |
|      3 | 1035 | `		return PH7_OK;` |
|      - | 1036 | `	}` |
|      5 | 1037 | `	iVal = 0;` |
|      5 | 1038 | `	if( ph7_value_is_string(apArg[0]) ){` |
|      - | 1039 | `		/* Extract the given string */` |
|      3 | 1040 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 1041 | `		if( nLen > 0 ){` |
|      - | 1042 | `			/* Perform the cast */` |
|      3 | 1043 | `			SyOctalStrToInt64(zString,(sxu32)nLen,(void *)&iVal,0);` |
|      1 | 1044 | `		}` |
|      2 | 1045 | `	}else{` |
|      - | 1046 | `		/* Extract as a 64-bit integer */` |
|      3 | 1047 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|      - | 1048 | `	}` |
|      - | 1049 | `	/* Return the number */` |
|      5 | 1050 | `	ph7_result_int64(pCtx,iVal);` |
|      5 | 1051 | `	return PH7_OK;` |
|      4 | 1052 |  |
|      - | 1053 | `/*` |
|      - | 1054 | ` * srand([int $seed])` |
|      - | 1055 | ` * mt_srand([int $seed])` |
|      - | 1056 | ` *  Seed the random number generator.` |
|      - | 1057 | ` * Parameters` |
|      - | 1058 | ` * $seed` |
|      - | 1059 | ` *  Optional seed value` |
|      - | 1060 | ` * Return` |
|      - | 1061 | ` *  null.` |
|      - | 1062 | ` * Note:` |
|      - | 1063 | ` *  THIS FUNCTION IS A NO-OP.` |
|      - | 1064 | ` *  THE PH7 PRNG IS AUTOMATICALLY SEEDED WHEN THE VM IS CREATED.` |
|      - | 1065 | ` */` |
|     20 | 1066 | `static int PH7_builtin_srand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1067 |  |
|     10 | 1068 | `	SXUNUSED(nArg);` |
|     10 | 1069 | `	SXUNUSED(apArg);` |
|     21 | 1070 | `	ph7_result_null(pCtx);` |
|     21 | 1071 | `	return PH7_OK;` |
|      1 | 1072 |  |
|      - | 1073 | `/*` |
|      - | 1074 | ` * string base_convert(string $number,int $frombase,int $tobase)` |
|      - | 1075 | ` *  Convert a number between arbitrary bases.` |
|      - | 1076 | ` * Parameters` |
|      - | 1077 | ` * $number` |
|      - | 1078 | ` *  The number to convert` |
|      - | 1079 | ` * $frombase` |
|      - | 1080 | ` *  The base number is in` |
|      - | 1081 | ` * $tobase` |
|      - | 1082 | ` *  The base to convert number to` |
|      - | 1083 | ` * Return` |
|      - | 1084 | ` *  Number converted to base tobase` |
|      - | 1085 | ` */` |
|      - | 1086 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 1087 | `#ifdef PH7_NEED_FMT_AND_INI` |
|     48 | 1088 | `static int PH7_builtin_base_convert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      - | 1089 |  |
|      - | 1090 |  |
|      1 | 1091 |  |
|      - | 1092 | `	int nLen,iFbase,iTobase;` |
|      - | 1093 | `	const char *zNum;` |
|      - | 1094 | `	ph7_int64 iNum;` |
|     49 | 1095 | `	if( nArg < 3 ){` |
|      - | 1096 | `		/* Return the empty string*/` |
|     13 | 1097 | `		ph7_result_string(pCtx,"",0);` |
|     13 | 1098 | `		return PH7_OK;` |
|      - | 1099 | `	}` |
|      - | 1100 | `	/* Base numbers */` |
|     37 | 1101 | `	iFbase  = ph7_value_to_int(apArg[1]);` |
|     37 | 1102 | `	iTobase = ph7_value_to_int(apArg[2]);` |
|     37 | 1103 | `	if( ph7_value_is_string(apArg[0]) ){` |
|      - | 1104 | `		/* Extract the target number */` |
|     29 | 1105 | `		zNum = ph7_value_to_string(apArg[0],&nLen);` |
|     29 | 1106 | `		if( nLen < 1 ){` |
|      - | 1107 | `			/* Return the empty string*/` |
|    ! 0 | 1108 | `			ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1109 | `			return PH7_OK;` |
|      - | 1110 | `		}` |
|      - | 1111 | `		/* Base conversion */` |
|     29 | 1112 | `		switch(iFbase){` |
|      5 | 1113 | `		case 16:` |
|      - | 1114 | `			/* Hex */` |
|     11 | 1115 | `			SyHexStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|     11 | 1116 | `			break;` |
|      3 | 1117 | `		case 8:` |
|      - | 1118 | `			/* Octal */` |
|      7 | 1119 | `			SyOctalStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|      7 | 1120 | `			break;` |
|      2 | 1121 | `		case 2:` |
|      - | 1122 | `			/* Binary */` |
|      5 | 1123 | `			SyBinaryStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|      5 | 1124 | `			break;` |
|      4 | 1125 | `		default:` |
|      - | 1126 | `			/* Decimal */` |
|      9 | 1127 | `			SyStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|      8 | 1128 | `			break;` |
|      - | 1129 | `		}` |
|     15 | 1130 | `	}else{` |
|      9 | 1131 | `		iNum = ph7_value_to_int64(apArg[0]);` |
|      - | 1132 | `	}` |
|     37 | 1133 | `	switch(iTobase){` |
|      4 | 1134 | `	case 16:` |
|      - | 1135 | `		/* Hex */` |
|      9 | 1136 | `		ph7_result_string_format(pCtx,"%qx",iNum); /* Quad hex */` |
|      9 | 1137 | `		break;` |
|      1 | 1138 | `	case 8:` |
|      - | 1139 | `		/* Octal */` |
|      3 | 1140 | `		ph7_result_string_format(pCtx,"%qo",iNum); /* Quad octal */` |
|      3 | 1141 | `		break;` |
|      1 | 1142 | `	case 2:` |
|      - | 1143 | `		/* Binary */` |
|      3 | 1144 | `		ph7_result_string_format(pCtx,"%qB",iNum); /* Quad binary */` |
|      3 | 1145 | `		break;` |
|     12 | 1146 | `	default:` |
|      - | 1147 | `		/* Decimal */` |
|     25 | 1148 | `		ph7_result_string_format(pCtx,"%qd",iNum); /* Quad decimal */` |
|     24 | 1149 | `		break;` |
|      - | 1150 | `	}` |
|     37 | 1151 | `	return PH7_OK;` |
|     25 | 1152 |  |
|      - | 1153 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 1154 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 1155 | `/*` |
|      - | 1156 | ` * Section:` |
|      - | 1157 | ` *    String handling Functions.` |
|      - | 1158 | ` * Status:` |
|      - | 1159 | ` *    Stable.` |
|      - | 1160 | ` */` |
|      - | 1161 | `/*` |
|      - | 1162 | ` * string substr(string $string,int $start[, int $length ])` |
|      - | 1163 | ` *  Return part of a string.` |
|      - | 1164 | ` * Parameters` |
|      - | 1165 | ` *  $string` |
|      - | 1166 | ` *   The input string. Must be one character or longer.` |
|      - | 1167 | ` * $start` |
|      - | 1168 | ` *   If start is non-negative, the returned string will start at the start'th position` |
|      - | 1169 | ` *   in string, counting from zero. For instance, in the string 'abcdef', the character` |
|      - | 1170 | ` *   at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 1171 | ` *   If start is negative, the returned string will start at the start'th character` |
|      - | 1172 | ` *   from the end of string.` |
|      - | 1173 | ` *   If string is less than or equal to start characters long, FALSE will be returned.` |
|      - | 1174 | ` * $length` |
|      - | 1175 | ` *   If length is given and is positive, the string returned will contain at most length` |
|      - | 1176 | ` *   characters beginning from start (depending on the length of string).` |
|      - | 1177 | ` *   If length is given and is negative, then that many characters will be omitted from` |
|      - | 1178 | ` *   the end of string (after the start position has been calculated when a start is negative).` |
|      - | 1179 | ` *   If start denotes the position of this truncation or beyond, false will be returned.` |
|      - | 1180 | ` *   If length is given and is 0, FALSE or NULL an empty string will be returned.` |
|      - | 1181 | ` *   If length is omitted, the substring starting from start until the end of the string` |
|      - | 1182 | ` *   will be returned.` |
|      - | 1183 | ` * Return` |
|      - | 1184 | ` *  Returns the extracted part of string, or FALSE on failure or an empty string.` |
|      - | 1185 | ` */` |
| 108426 | 1186 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1187 |  |
|      - | 1188 | `	const char *zSource,*zOfft;` |
|      - | 1189 | `	int nOfft,nLen,nSrcLen;` |
| 108428 | 1190 | `	if( nArg < 2 ){` |
|      - | 1191 | `		/* return FALSE */` |
|      5 | 1192 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1193 | `		return PH7_OK;` |
|      - | 1194 | `	}` |
|      - | 1195 | `	/* Extract the target string */` |
| 108424 | 1196 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 108424 | 1197 | `	if( nSrcLen < 1 ){` |
|      - | 1198 | `		/* Empty string,return FALSE */` |
|   6858 | 1199 | `		ph7_result_bool(pCtx,0);` |
|   6858 | 1200 | `		return PH7_OK;` |
|      - | 1201 | `	}` |
| 101568 | 1202 | `	nLen = nSrcLen; /* cc warning */` |
|      - | 1203 | `	/* Extract the offset */` |
| 101568 | 1204 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 101568 | 1205 | `	if( nOfft < 0 ){` |
|  16748 | 1206 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  16748 | 1207 | `		if( zOfft < zSource ){` |
|      - | 1208 | `			/* Invalid offset */` |
|      5 | 1209 | `			ph7_result_bool(pCtx,0);` |
|      5 | 1210 | `			return PH7_OK;` |
|      - | 1211 | `		}` |
|  16744 | 1212 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  16744 | 1213 | `		nOfft = (int)(zOfft-zSource);` |
|  93193 | 1214 | `	}else if( nOfft >= nSrcLen ){` |
|      - | 1215 | `		/* Invalid offset */` |
|      7 | 1216 | `		ph7_result_bool(pCtx,0);` |
|      7 | 1217 | `		return PH7_OK;` |
|    ! 0 | 1218 | `	}else{` |
|  84816 | 1219 | `		zOfft = &zSource[nOfft];` |
|  84816 | 1220 | `		nLen = nSrcLen - nOfft;` |
|      - | 1221 | `	}` |
| 101558 | 1222 | `	if( nArg > 2 ){` |
|      - | 1223 | `		/* Extract the length */` |
|  84814 | 1224 | `		nLen = ph7_value_to_int(apArg[2]);` |
|  84814 | 1225 | `		if( nLen == 0 ){` |
|      - | 1226 | `			/* Invalid length,return an empty string */` |
|      5 | 1227 | `			ph7_result_string(pCtx,"",0);` |
|      5 | 1228 | `			return PH7_OK;` |
|  84810 | 1229 | `		}else if( nLen < 0 ){` |
|  16746 | 1230 | `			nLen = nSrcLen + nLen - nOfft;` |
|  16746 | 1231 | `			if( nLen < 1 ){` |
|      - | 1232 | `				/* Invalid  length */` |
|      3 | 1233 | `				nLen = nSrcLen - nOfft;` |
|      1 | 1234 | `			}` |
|   8372 | 1235 | `		}` |
|  84810 | 1236 | `		if( nLen + nOfft > nSrcLen ){` |
|      - | 1237 | `			/* Invalid length */` |
|   2134 | 1238 | `			nLen = nSrcLen - nOfft;` |
|   1066 | 1239 | `		}` |
|  42404 | 1240 | `	}` |
|      - | 1241 | `	/* Return the substring */` |
| 101554 | 1242 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 101554 | 1243 | `	return PH7_OK;` |
|  54215 | 1244 |  |
|      - | 1245 | `/*` |
|      - | 1246 | ` * int substr_compare(string $main_str,string $str ,int $offset[,int $length[,bool $case_insensitivity = false ]])` |
|      - | 1247 | ` *  Binary safe comparison of two strings from an offset, up to length characters.` |
|      - | 1248 | ` * Parameters` |
|      - | 1249 | ` *  $main_str` |
|      - | 1250 | ` *  The main string being compared.` |
|      - | 1251 | ` *  $str` |
|      - | 1252 | ` *   The secondary string being compared.` |
|      - | 1253 | ` * $offset` |
|      - | 1254 | ` *  The start position for the comparison. If negative, it starts counting from` |
|      - | 1255 | ` *  the end of the string.` |
|      - | 1256 | ` * $length` |
|      - | 1257 | ` *  The length of the comparison. The default value is the largest of the length` |
|      - | 1258 | ` *  of the str compared to the length of main_str less the offset.` |
|      - | 1259 | ` * $case_insensitivity` |
|      - | 1260 | ` *  If case_insensitivity is TRUE, comparison is case insensitive.` |
|      - | 1261 | ` * Return` |
|      - | 1262 | ` *  Returns < 0 if main_str from position offset is less than str, > 0 if it is greater than` |
|      - | 1263 | ` *  str, and 0 if they are equal. If offset is equal to or greater than the length of main_str` |
|      - | 1264 | ` *  or length is set and is less than 1, substr_compare() prints a warning and returns FALSE.` |
|      - | 1265 | ` */` |
|     26 | 1266 | `static int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1267 |  |
|      - | 1268 | `	const char *zSource,*zOfft,*zSub;` |
|      - | 1269 | `	int nOfft,nLen,nSrcLen,nSublen;` |
|     27 | 1270 | `	int iCase = 0;` |
|      - | 1271 | `	int rc;` |
|     27 | 1272 | `	if( nArg < 3 ){` |
|      - | 1273 | `		/* Missing arguments,return FALSE */` |
|      5 | 1274 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1275 | `		return PH7_OK;` |
|      - | 1276 | `	}` |
|      - | 1277 | `	/* Extract the target string */` |
|     23 | 1278 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|     23 | 1279 | `	if( nSrcLen < 1 ){` |
|      - | 1280 | `		/* Empty string,return FALSE */` |
|      3 | 1281 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1282 | `		return PH7_OK;` |
|      - | 1283 | `	}` |
|     21 | 1284 | `	nLen = nSrcLen; /* cc warning */` |
|      - | 1285 | `	/* Extract the substring */` |
|     21 | 1286 | `	zSub = ph7_value_to_string(apArg[1],&nSublen);` |
|     21 | 1287 | `	if( nSublen < 1 \|\| nSublen > nSrcLen){` |
|      - | 1288 | `		/* Empty string,return FALSE */` |
|      3 | 1289 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1290 | `		return PH7_OK;` |
|      - | 1291 | `	}` |
|      - | 1292 | `	/* Extract the offset */` |
|     19 | 1293 | `	nOfft = ph7_value_to_int(apArg[2]);` |
|     19 | 1294 | `	if( nOfft < 0 ){` |
|      5 | 1295 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|      5 | 1296 | `		if( zOfft < zSource ){` |
|      - | 1297 | `			/* Invalid offset */` |
|      3 | 1298 | `			ph7_result_bool(pCtx,0);` |
|      3 | 1299 | `			return PH7_OK;` |
|      - | 1300 | `		}` |
|      3 | 1301 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|      3 | 1302 | `		nOfft = (int)(zOfft-zSource);` |
|     16 | 1303 | `	}else if( nOfft >= nSrcLen ){` |
|      - | 1304 | `		/* Invalid offset */` |
|      3 | 1305 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1306 | `		return PH7_OK;` |
|    ! 0 | 1307 | `	}else{` |
|     13 | 1308 | `		zOfft = &zSource[nOfft];` |
|     13 | 1309 | `		nLen = nSrcLen - nOfft;` |
|      - | 1310 | `	}` |
|     15 | 1311 | `	if( nArg > 3 ){` |
|      - | 1312 | `		/* Extract the length */` |
|     13 | 1313 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     13 | 1314 | `		if( nLen < 1 ){` |
|      - | 1315 | `			/* Invalid  length */` |
|      5 | 1316 | `			ph7_result_int(pCtx,1);` |
|      5 | 1317 | `			return PH7_OK;` |
|      9 | 1318 | `		}else if( nLen + nOfft > nSrcLen ){` |
|      - | 1319 | `			/* Invalid length */` |
|      3 | 1320 | `			nLen = nSrcLen - nOfft;` |
|      1 | 1321 | `		}` |
|      9 | 1322 | `		if( nArg > 4 ){` |
|      - | 1323 | `			/* Case-sensitive or not */` |
|      5 | 1324 | `			iCase = ph7_value_to_bool(apArg[4]);` |
|      2 | 1325 | `		}` |
|      4 | 1326 | `	}` |
|      - | 1327 | `	/* Perform the comparison */` |
|     11 | 1328 | `	if( iCase ){` |
|      3 | 1329 | `		rc = SyStrnicmp(zOfft,zSub,(sxu32)nLen);` |
|      2 | 1330 | `	}else{` |
|      9 | 1331 | `		rc = SyStrncmp(zOfft,zSub,(sxu32)nLen);` |
|      - | 1332 | `	}` |
|      - | 1333 | `	/* Comparison result */` |
|     11 | 1334 | `	ph7_result_int(pCtx,rc);` |
|     11 | 1335 | `	return PH7_OK;` |
|     14 | 1336 |  |
|      - | 1337 | `/*` |
|      - | 1338 | ` * int substr_count(string $haystack,string $needle[,int $offset = 0 [,int $length ]])` |
|      - | 1339 | ` *  Count the number of substring occurrences.` |
|      - | 1340 | ` * Parameters` |
|      - | 1341 | ` * $haystack` |
|      - | 1342 | ` *   The string to search in` |
|      - | 1343 | ` * $needle` |
|      - | 1344 | ` *   The substring to search for` |
|      - | 1345 | ` * $offset` |
|      - | 1346 | ` *  The offset where to start counting` |
|      - | 1347 | ` * $length (NOT USED)` |
|      - | 1348 | ` *  The maximum length after the specified offset to search for the substring.` |
|      - | 1349 | ` *  It outputs a warning if the offset plus the length is greater than the haystack length.` |
|      - | 1350 | ` * Return` |
|      - | 1351 | ` *  Toral number of substring occurrences.` |
|      - | 1352 | ` */` |
|     24 | 1353 | `static int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1354 |  |
|      - | 1355 | `	const char *zText,*zPattern,*zEnd;` |
|      - | 1356 | `	int nTextlen,nPatlen;` |
|     25 | 1357 | `	int iCount = 0;` |
|      - | 1358 | `	sxu32 nOfft;` |
|      - | 1359 | `	sxi32 rc;` |
|     25 | 1360 | `	if( nArg < 2 ){` |
|      - | 1361 | `		/* Missing arguments */` |
|      5 | 1362 | `		ph7_result_int(pCtx,0);` |
|      5 | 1363 | `		return PH7_OK;` |
|      - | 1364 | `	}` |
|      - | 1365 | `	/* Point to the haystack */` |
|     21 | 1366 | `	zText = ph7_value_to_string(apArg[0],&nTextlen);` |
|      - | 1367 | `	/* Point to the neddle */` |
|     21 | 1368 | `	zPattern = ph7_value_to_string(apArg[1],&nPatlen);` |
|     21 | 1369 | `	if( nTextlen < 1 \|\| nPatlen < 1 \|\| nPatlen > nTextlen ){` |
|      - | 1370 | `		/* NOOP,return zero */` |
|      3 | 1371 | `		ph7_result_int(pCtx,0);` |
|      3 | 1372 | `		return PH7_OK;` |
|      - | 1373 | `	}` |
|     19 | 1374 | `	if( nArg > 2 ){` |
|      - | 1375 | `		int iOfft;` |
|      - | 1376 | `		/* Extract the offset */` |
|     15 | 1377 | `		iOfft = ph7_value_to_int(apArg[2]);` |
|     15 | 1378 | `		if( iOfft < 0 \|\| iOfft > nTextlen ){` |
|      - | 1379 | `			/* Invalid offset,return zero */` |
|      3 | 1380 | `			ph7_result_int(pCtx,0);` |
|      3 | 1381 | `			return PH7_OK;` |
|      - | 1382 | `		}` |
|      - | 1383 | `		/* Point to the desired offset */` |
|     13 | 1384 | `		zText = &zText[iOfft];` |
|      - | 1385 | `		/* Adjust length */` |
|     13 | 1386 | `		nTextlen -= iOfft;` |
|      6 | 1387 | `	}` |
|      - | 1388 | `	/* Point to the end of the string */` |
|     17 | 1389 | `	zEnd = &zText[nTextlen];` |
|     17 | 1390 | `	if( nArg > 3 ){` |
|      - | 1391 | `		int nLen;` |
|      - | 1392 | `		/* Extract the length */` |
|     13 | 1393 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     13 | 1394 | `		if( nLen < 0 \|\| nLen > nTextlen ){` |
|      - | 1395 | `			/* Invalid length,return 0 */` |
|      7 | 1396 | `			ph7_result_int(pCtx,0);` |
|      7 | 1397 | `			return PH7_OK;` |
|      - | 1398 | `		}` |
|      - | 1399 | `		/* Adjust pointer */` |
|      7 | 1400 | `		nTextlen = nLen;` |
|      7 | 1401 | `		zEnd = &zText[nTextlen];` |
|      3 | 1402 | `	}` |
|      - | 1403 | `	/* Perform the search */` |
|     12 | 1404 | `	for(;;){` |
|     25 | 1405 | `		rc = SyBlobSearch((const void *)zText,(sxu32)(zEnd-zText),(const void *)zPattern,nPatlen,&nOfft);` |
|     25 | 1406 | `		if( rc != SXRET_OK ){` |
|      - | 1407 | `			/* Pattern not found,break immediately */` |
|      9 | 1408 | `			break;` |
|      - | 1409 | `		}` |
|      - | 1410 | `		/* Increment counter and update the offset */` |
|     17 | 1411 | `		iCount++;` |
|     17 | 1412 | `		zText += nOfft + nPatlen;` |
|     17 | 1413 | `		if( zText >= zEnd ){` |
|      3 | 1414 | `			break;` |
|      - | 1415 | `		}` |
|      1 | 1416 | `	}` |
|      - | 1417 | `	/* Pattern count */` |
|     11 | 1418 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 1419 | `	return PH7_OK;` |
|     13 | 1420 |  |
|      - | 1421 | `/*` |
|      - | 1422 | ` * string chunk_split(string $body[,int $chunklen = 76 [, string $end = "\r\n" ]])` |
|      - | 1423 | ` *   Split a string into smaller chunks.` |
|      - | 1424 | ` * Parameters` |
|      - | 1425 | ` *  $body` |
|      - | 1426 | ` *   The string to be chunked.` |
|      - | 1427 | ` * $chunklen` |
|      - | 1428 | ` *   The chunk length.` |
|      - | 1429 | ` * $end` |
|      - | 1430 | ` *   The line ending sequence.` |
|      - | 1431 | ` * Return` |
|      - | 1432 | ` *  The chunked string or NULL on failure.` |
|      - | 1433 | ` */` |
|     16 | 1434 | `static int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1435 |  |
|     17 | 1436 | `	const char *zIn,*zEnd,*zSep = "\r\n";` |
|      - | 1437 | `	int nSepLen,nChunkLen,nLen;` |
|     17 | 1438 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1439 | `		/* Nothing to split,return null */` |
|      5 | 1440 | `		ph7_result_null(pCtx);` |
|      5 | 1441 | `		return PH7_OK;` |
|      - | 1442 | `	}` |
|      - | 1443 | `	/* initialize/Extract arguments */` |
|     13 | 1444 | `	nSepLen = (int)sizeof("\r\n") - 1;` |
|     13 | 1445 | `	nChunkLen = 76;` |
|     13 | 1446 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 1447 | `	zEnd = &zIn[nLen];` |
|     13 | 1448 | `	if( nArg > 1 ){` |
|      - | 1449 | `		/* Chunk length */` |
|     13 | 1450 | `		nChunkLen = ph7_value_to_int(apArg[1]);` |
|     13 | 1451 | `		if( nChunkLen < 1 ){` |
|      - | 1452 | `			/* Switch back to the default length */` |
|      3 | 1453 | `			nChunkLen = 76;` |
|      1 | 1454 | `		}` |
|     13 | 1455 | `		if( nArg > 2 ){` |
|      - | 1456 | `			/* Separator */` |
|      9 | 1457 | `			zSep = ph7_value_to_string(apArg[2],&nSepLen);` |
|      9 | 1458 | `			if( nSepLen < 1 ){` |
|      - | 1459 | `				/* Switch back to the default separator */` |
|      3 | 1460 | `				zSep = "\r\n";` |
|      3 | 1461 | `				nSepLen = (int)sizeof("\r\n") - 1;` |
|      1 | 1462 | `			}` |
|      4 | 1463 | `		}` |
|      6 | 1464 | `	}` |
|      - | 1465 | `	/* Perform the requested operation */` |
|     13 | 1466 | `	if( nChunkLen > nLen ){` |
|      - | 1467 | `		/* Nothing to split,return the string and the separator */` |
|      9 | 1468 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);` |
|      9 | 1469 | `		return PH7_OK;` |
|      - | 1470 | `	}` |
|     17 | 1471 | `	while( zIn < zEnd ){` |
|     13 | 1472 | `		if( nChunkLen > (int)(zEnd-zIn) ){` |
|      3 | 1473 | `			nChunkLen = (int)(zEnd - zIn);` |
|      1 | 1474 | `		}` |
|      - | 1475 | `		/* Append the chunk and the separator */` |
|     13 | 1476 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);` |
|      - | 1477 | `		/* Point beyond the chunk */` |
|     13 | 1478 | `		zIn += nChunkLen;` |
|      1 | 1479 | `	}` |
|      5 | 1480 | `	return PH7_OK;` |
|      9 | 1481 |  |
|      - | 1482 | `/*` |
|      - | 1483 | ` * string addslashes(string $str)` |
|      - | 1484 | ` *  Quote string with slashes.` |
|      - | 1485 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1486 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1487 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1488 | ` * Parameter` |
|      - | 1489 | ` *  str: The string to be escaped.` |
|      - | 1490 | ` * Return` |
|      - | 1491 | ` *  Returns the escaped string` |
|      - | 1492 | ` */` |
|     10 | 1493 | `static int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1494 |  |
|      - | 1495 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1496 | `	int nLen;` |
|     11 | 1497 | `	if( nArg < 1 ){` |
|      - | 1498 | `		/* Nothing to process,retun NULL */` |
|      5 | 1499 | `		ph7_result_null(pCtx);` |
|      5 | 1500 | `		return PH7_OK;` |
|      - | 1501 | `	}` |
|      - | 1502 | `	/* Extract the string to process */` |
|      7 | 1503 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 1504 | `	if( nLen < 1 ){` |
|      - | 1505 | `		/* Return the empty string */` |
|      5 | 1506 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1507 | `		return PH7_OK;` |
|      - | 1508 | `	}` |
|      3 | 1509 | `	zEnd = &zIn[nLen];` |
|      3 | 1510 | `	zCur = 0; /* cc warning */` |
|      3 | 1511 | `	for(;;){` |
|      7 | 1512 | `		if( zIn >= zEnd ){` |
|      - | 1513 | `			/* No more input */` |
|      3 | 1514 | `			break;` |
|      - | 1515 | `		}` |
|      5 | 1516 | `		zCur = zIn;` |
|     15 | 1517 | `		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' ){` |
|     11 | 1518 | `			zIn++;` |
|      1 | 1519 | `		}` |
|      5 | 1520 | `		if( zIn > zCur ){` |
|      - | 1521 | `			/* Append raw contents */` |
|      5 | 1522 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 1523 | `		}` |
|      5 | 1524 | `		if( zIn < zEnd ){` |
|      3 | 1525 | `			int c = zIn[0];` |
|      3 | 1526 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|      1 | 1527 | `		}` |
|      5 | 1528 | `		zIn++;` |
|      1 | 1529 | `	}` |
|      3 | 1530 | `	return PH7_OK;` |
|      6 | 1531 |  |
|      - | 1532 | `/*` |
|      - | 1533 | ` * Check if the given character is present in the given mask.` |
|      - | 1534 | ` * Return TRUE if present. FALSE otherwise.` |
|      - | 1535 | ` */` |
|     76 | 1536 | `static int cSlashCheckMask(int c,const char *zMask,int nLen)` |
|      1 | 1537 |  |
|     77 | 1538 | `	const char *zEnd = &zMask[nLen];` |
|    495 | 1539 | `	while( zMask < zEnd ){` |
|    449 | 1540 | `		if( zMask[0] == c ){` |
|      - | 1541 | `			/* Character present,return TRUE */` |
|     31 | 1542 | `			return 1;` |
|      - | 1543 | `		}` |
|      - | 1544 | `		/* Advance the pointer */` |
|    419 | 1545 | `		zMask++;` |
|      1 | 1546 | `	}` |
|      - | 1547 | `	/* Not present */` |
|     47 | 1548 | `	return 0;` |
|     39 | 1549 |  |
|      - | 1550 | `/*` |
|      - | 1551 | ` * string addcslashes(string $str,string $charlist)` |
|      - | 1552 | ` *  Quote string with slashes in a C style.` |
|      - | 1553 | ` * Parameter` |
|      - | 1554 | ` *  $str:` |
|      - | 1555 | ` *    The string to be escaped.` |
|      - | 1556 | ` *  $charlist:` |
|      - | 1557 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|      - | 1558 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|      - | 1559 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|      - | 1560 | ` * Return` |
|      - | 1561 | ` *  Returns the escaped string.` |
|      - | 1562 | ` * Note:` |
|      - | 1563 | ` *  Range characters [i.e: 'A..Z'] is not implemented in the current release.` |
|      - | 1564 | ` */` |
|     12 | 1565 | `static int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1566 |  |
|      - | 1567 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - | 1568 | `	int nLen,nMask;` |
|     13 | 1569 | `	if( nArg < 1 ){` |
|      - | 1570 | `		/* Nothing to process,retun NULL */` |
|      3 | 1571 | `		ph7_result_null(pCtx);` |
|      3 | 1572 | `		return PH7_OK;` |
|      - | 1573 | `	}` |
|      - | 1574 | `	/* Extract the string to process */` |
|     11 | 1575 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 1576 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 1577 | `		/* Return the string untouched */` |
|      5 | 1578 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      5 | 1579 | `		return PH7_OK;` |
|      - | 1580 | `	}` |
|      - | 1581 | `	/* Extract the desired mask */` |
|      7 | 1582 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|      7 | 1583 | `	zEnd = &zIn[nLen];` |
|      7 | 1584 | `	zCur = 0; /* cc warning */` |
|      8 | 1585 | `	for(;;){` |
|     17 | 1586 | `		if( zIn >= zEnd ){` |
|      - | 1587 | `			/* No more input */` |
|      7 | 1588 | `			break;` |
|      - | 1589 | `		}` |
|     11 | 1590 | `		zCur = zIn;` |
|     31 | 1591 | `		while( zIn < zEnd && !cSlashCheckMask(zIn[0],zMask,nMask) ){` |
|     21 | 1592 | `			zIn++;` |
|      1 | 1593 | `		}` |
|     11 | 1594 | `		if( zIn > zCur ){` |
|      - | 1595 | `			/* Append raw contents */` |
|     11 | 1596 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      5 | 1597 | `		}` |
|     11 | 1598 | `		if( zIn < zEnd ){` |
|      5 | 1599 | `			int c = zIn[0];` |
|      5 | 1600 | `			if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|      - | 1601 | `				/* Convert to octal */` |
|      3 | 1602 | `				ph7_result_string_format(pCtx,"\\%o",c);` |
|      2 | 1603 | `			}else{` |
|      3 | 1604 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1605 | `			}` |
|      2 | 1606 | `		}` |
|     11 | 1607 | `		zIn++;` |
|      1 | 1608 | `	}` |
|      7 | 1609 | `	return PH7_OK;` |
|      7 | 1610 |  |
|      - | 1611 | `/*` |
|      - | 1612 | ` * string quotemeta(string $str)` |
|      - | 1613 | ` *  Quote meta characters.` |
|      - | 1614 | ` * Parameter` |
|      - | 1615 | ` *  $str:` |
|      - | 1616 | ` *    The string to be escaped.` |
|      - | 1617 | ` * Return` |
|      - | 1618 | ` *  Returns the escaped string.` |
|      - | 1619 | `*/` |
|     10 | 1620 | `static int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1621 |  |
|      - | 1622 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1623 | `	int nLen;` |
|     11 | 1624 | `	if( nArg < 1 ){` |
|      - | 1625 | `		/* Nothing to process,retun NULL */` |
|      3 | 1626 | `		ph7_result_null(pCtx);` |
|      3 | 1627 | `		return PH7_OK;` |
|      - | 1628 | `	}` |
|      - | 1629 | `	/* Extract the string to process */` |
|      9 | 1630 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 1631 | `	if( nLen < 1 ){` |
|      - | 1632 | `		/* Return the empty string */` |
|      3 | 1633 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1634 | `		return PH7_OK;` |
|      - | 1635 | `	}` |
|      7 | 1636 | `	zEnd = &zIn[nLen];` |
|      7 | 1637 | `	zCur = 0; /* cc warning */` |
|     17 | 1638 | `	for(;;){` |
|     35 | 1639 | `		if( zIn >= zEnd ){` |
|      - | 1640 | `			/* No more input */` |
|      7 | 1641 | `			break;` |
|      - | 1642 | `		}` |
|     29 | 1643 | `		zCur = zIn;` |
|     55 | 1644 | `		while( zIn < zEnd && !cSlashCheckMask(zIn[0],".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1) ){` |
|     27 | 1645 | `			zIn++;` |
|      1 | 1646 | `		}` |
|     29 | 1647 | `		if( zIn > zCur ){` |
|      - | 1648 | `			/* Append raw contents */` |
|     11 | 1649 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      5 | 1650 | `		}` |
|     29 | 1651 | `		if( zIn < zEnd ){` |
|     27 | 1652 | `			int c = zIn[0];` |
|     27 | 1653 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|     13 | 1654 | `		}` |
|     29 | 1655 | `		zIn++;` |
|      1 | 1656 | `	}` |
|      7 | 1657 | `	return PH7_OK;` |
|      6 | 1658 |  |
|      - | 1659 | `/*` |
|      - | 1660 | ` * string stripslashes(string $str)` |
|      - | 1661 | ` *  Un-quotes a quoted string.` |
|      - | 1662 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1663 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1664 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1665 | ` * Parameter` |
|      - | 1666 | ` *  $str` |
|      - | 1667 | ` *   The input string.` |
|      - | 1668 | ` * Return` |
|      - | 1669 | ` *  Returns a string with backslashes stripped off.` |
|      - | 1670 | ` */` |
|      8 | 1671 | `static int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1672 |  |
|      - | 1673 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1674 | `	int nLen;` |
|      9 | 1675 | `	if( nArg < 1 ){` |
|      - | 1676 | `		/* Nothing to process,retun NULL */` |
|      3 | 1677 | `		ph7_result_null(pCtx);` |
|      3 | 1678 | `		return PH7_OK;` |
|      - | 1679 | `	}` |
|      - | 1680 | `	/* Extract the string to process */` |
|      7 | 1681 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 1682 | `	if( zIn == 0 ){` |
|    ! 0 | 1683 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1684 | `		return PH7_OK;` |
|      - | 1685 | `	}` |
|      7 | 1686 | `	zEnd = &zIn[nLen];` |
|      7 | 1687 | `	zCur = 0; /* cc warning */` |
|      - | 1688 | `	/* Encode the string */` |
|      4 | 1689 | `	for(;;){` |
|      9 | 1690 | `		if( zIn >= zEnd ){` |
|      - | 1691 | `			/* No more input */` |
|      5 | 1692 | `			break;` |
|      - | 1693 | `		}` |
|      5 | 1694 | `		zCur = zIn;` |
|     17 | 1695 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 | 1696 | `			zIn++;` |
|      1 | 1697 | `		}` |
|      5 | 1698 | `		if( zIn > zCur ){` |
|      - | 1699 | `			/* Append raw contents */` |
|      5 | 1700 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 1701 | `		}` |
|      5 | 1702 | `		if( &zIn[1] < zEnd ){` |
|      3 | 1703 | `			int c = zIn[1];` |
|      3 | 1704 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - | 1705 | `				/* Ignore the backslash */` |
|      3 | 1706 | `				zIn++;` |
|      1 | 1707 | `			}` |
|      2 | 1708 | `		}else{` |
|      3 | 1709 | `			break;` |
|      - | 1710 | `		}` |
|      1 | 1711 | `	}` |
|      7 | 1712 | `	return PH7_OK;` |
|      5 | 1713 |  |
|      - | 1714 | `/*` |
|      - | 1715 | ` * string htmlspecialchars(string $string [, int $flags = ENT_COMPAT \| ENT_HTML401 [, string $charset]])` |
|      - | 1716 | ` *  HTML escaping of special characters.` |
|      - | 1717 | ` *  The translations performed are:` |
|      - | 1718 | ` *   '&' (ampersand) ==> '&amp;'` |
|      - | 1719 | ` *   '"' (double quote) ==> '&quot;' when ENT_NOQUOTES is not set.` |
|      - | 1720 | ` *   "'" (single quote) ==> '&#039;' only when ENT_QUOTES is set.` |
|      - | 1721 | ` *   '<' (less than) ==> '&lt;'` |
|      - | 1722 | ` *   '>' (greater than) ==> '&gt;'` |
|      - | 1723 | ` * Parameters` |
|      - | 1724 | ` *  $string` |
|      - | 1725 | ` *   The string being converted.` |
|      - | 1726 | ` * $flags` |
|      - | 1727 | ` *   A bitmask of one or more of the following flags, which specify how to handle quotes.` |
|      - | 1728 | ` *   The default is ENT_COMPAT \| ENT_HTML401.` |
|      - | 1729 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone.` |
|      - | 1730 | ` *   ENT_QUOTES 	Will convert both double and single quotes.` |
|      - | 1731 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted.` |
|      - | 1732 | ` *   ENT_IGNORE 	Silently discard invalid code unit sequences instead of returning an empty string.` |
|      - | 1733 | ` * $charset` |
|      - | 1734 | ` *  Defines character set used in conversion. The default character set is ISO-8859-1. (Not used)` |
|      - | 1735 | ` * Return` |
|      - | 1736 | ` *  The escaped string or NULL on failure.` |
|      - | 1737 | ` */` |
|     20 | 1738 | `static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1739 |  |
|      - | 1740 | `	const char *zCur,*zIn,*zEnd;` |
|     21 | 1741 | `	int iFlags = 0x01\|0x40; /* ENT_COMPAT \| ENT_HTML401 */` |
|      - | 1742 | `	int nLen,c;` |
|     21 | 1743 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1744 | `		/* Missing/Invalid arguments,return NULL */` |
|     11 | 1745 | `		ph7_result_null(pCtx);` |
|     11 | 1746 | `		return PH7_OK;` |
|      - | 1747 | `	}` |
|      - | 1748 | `	/* Extract the target string */` |
|     11 | 1749 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 1750 | `	zEnd = &zIn[nLen];` |
|      - | 1751 | `	/* Extract the flags if available */` |
|     11 | 1752 | `	if( nArg > 1 ){` |
|      9 | 1753 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 | 1754 | `		if( iFlags < 0 ){` |
|      3 | 1755 | `			iFlags = 0x01\|0x40;` |
|      1 | 1756 | `		}` |
|      4 | 1757 | `	}` |
|      - | 1758 | `	/* Perform the requested operation */` |
|     23 | 1759 | `	for(;;){` |
|     47 | 1760 | `		if( zIn >= zEnd ){` |
|      9 | 1761 | `			break;` |
|      - | 1762 | `		}` |
|     39 | 1763 | `		zCur = zIn;` |
|     83 | 1764 | `		while( zIn < zEnd && zIn[0] != '&' && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '<' && zIn[0] != '>' ){` |
|     45 | 1765 | `			zIn++;` |
|      1 | 1766 | `		}` |
|     39 | 1767 | `		if( zCur < zIn ){` |
|      - | 1768 | `			/* Append the raw string verbatim */` |
|     17 | 1769 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      8 | 1770 | `		}` |
|     39 | 1771 | `		if( zIn >= zEnd ){` |
|      3 | 1772 | `			break;` |
|      - | 1773 | `		}` |
|     37 | 1774 | `		c = zIn[0];` |
|     37 | 1775 | `		if( c == '&' ){` |
|      - | 1776 | `			/* Expand '&amp;' */` |
|      9 | 1777 | `			ph7_result_string(pCtx,"&amp;",(int)sizeof("&amp;")-1);` |
|     33 | 1778 | `		}else if( c == '<' ){` |
|      - | 1779 | `			/* Expand '&lt;' */` |
|      7 | 1780 | `			ph7_result_string(pCtx,"&lt;",(int)sizeof("&lt;")-1);` |
|     26 | 1781 | `		}else if( c == '>' ){` |
|      - | 1782 | `			/* Expand '&gt;' */` |
|      9 | 1783 | `			ph7_result_string(pCtx,"&gt;",(int)sizeof("&gt;")-1);` |
|     19 | 1784 | `		}else if( c == '\'' ){` |
|      5 | 1785 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 1786 | `				/* Expand '&#039;' */` |
|      5 | 1787 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      3 | 1788 | `			}else{` |
|      - | 1789 | `				/* Leave the single quote untouched */` |
|    ! 0 | 1790 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      1 | 1791 | `			}` |
|     13 | 1792 | `		}else if( c == '"' ){` |
|     11 | 1793 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      - | 1794 | `				/* Expand '&quot;' */` |
|      7 | 1795 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      4 | 1796 | `			}else{` |
|      - | 1797 | `				/* Leave the double quote untouched */` |
|      5 | 1798 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      - | 1799 | `			}` |
|      5 | 1800 | `		}` |
|      - | 1801 | `		/* Ignore the unsafe HTML character */` |
|     37 | 1802 | `		zIn++;` |
|      1 | 1803 | `	}` |
|     11 | 1804 | `	return PH7_OK;` |
|     11 | 1805 |  |
|      - | 1806 | `/*` |
|      - | 1807 | ` * string htmlspecialchars_decode(string $string[,int $quote_style = ENT_COMPAT ])` |
|      - | 1808 | ` *  Unescape HTML entities.` |
|      - | 1809 | ` * Parameters` |
|      - | 1810 | ` *  $string` |
|      - | 1811 | ` *   The string to decode` |
|      - | 1812 | ` *  $quote_style` |
|      - | 1813 | ` *    The quote style. One of the following constants:` |
|      - | 1814 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone (default)` |
|      - | 1815 | ` *   ENT_QUOTES 	Will convert both double and single quotes` |
|      - | 1816 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted` |
|      - | 1817 | ` * Return` |
|      - | 1818 | ` *  The unescaped string or NULL on failure.` |
|      - | 1819 | ` */` |
|     16 | 1820 | `static int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1821 |  |
|      - | 1822 | `	const char *zCur,*zIn,*zEnd;` |
|     17 | 1823 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 1824 | `	int nLen,nJump;` |
|     17 | 1825 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1826 | `		/* Missing/Invalid arguments,return NULL */` |
|      7 | 1827 | `		ph7_result_null(pCtx);` |
|      7 | 1828 | `		return PH7_OK;` |
|      - | 1829 | `	}` |
|      - | 1830 | `	/* Extract the target string */` |
|     11 | 1831 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 1832 | `	zEnd = &zIn[nLen];` |
|      - | 1833 | `	/* Extract the flags if available */` |
|     11 | 1834 | `	if( nArg > 1 ){` |
|      7 | 1835 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      7 | 1836 | `		if( iFlags < 0 ){` |
|      3 | 1837 | `			iFlags = 0x01;` |
|      1 | 1838 | `		}` |
|      3 | 1839 | `	}` |
|      - | 1840 | `	/* Perform the requested operation */` |
|     15 | 1841 | `	for(;;){` |
|     31 | 1842 | `		if( zIn >= zEnd ){` |
|     11 | 1843 | `			break;` |
|      - | 1844 | `		}` |
|     21 | 1845 | `		zCur = zIn;` |
|     51 | 1846 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|     31 | 1847 | `			zIn++;` |
|      1 | 1848 | `		}` |
|     21 | 1849 | `		if( zCur < zIn ){` |
|      - | 1850 | `			/* Append the raw string verbatim */` |
|      9 | 1851 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 1852 | `		}` |
|     21 | 1853 | `		nLen = (int)(zEnd-zIn);` |
|     21 | 1854 | `		nJump = (int)sizeof(char);` |
|     21 | 1855 | `		if( nLen >= (int)sizeof("&amp;")-1 && SyStrnicmp(zIn,"&amp;",sizeof("&amp;")-1) == 0 ){` |
|      - | 1856 | `			/* &amp; ==> '&' */` |
|      3 | 1857 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|      3 | 1858 | `			nJump = (int)sizeof("&amp;")-1;` |
|     20 | 1859 | `		}else if( nLen >= (int)sizeof("&lt;")-1 && SyStrnicmp(zIn,"&lt;",sizeof("&lt;")-1) == 0 ){` |
|      - | 1860 | `			/* &lt; ==> < */` |
|      3 | 1861 | `			ph7_result_string(pCtx,"<",(int)sizeof(char));` |
|      3 | 1862 | `			nJump = (int)sizeof("&lt;")-1;` |
|     18 | 1863 | `		}else if( nLen >= (int)sizeof("&gt;")-1 && SyStrnicmp(zIn,"&gt;",sizeof("&gt;")-1) == 0 ){` |
|      - | 1864 | `			/* &gt; ==> '>' */` |
|      3 | 1865 | `			ph7_result_string(pCtx,">",(int)sizeof(char));` |
|      3 | 1866 | `			nJump = (int)sizeof("&gt;")-1;` |
|     16 | 1867 | `		}else if( nLen >= (int)sizeof("&quot;")-1 && SyStrnicmp(zIn,"&quot;",sizeof("&quot;")-1) == 0 ){` |
|      - | 1868 | `			/* &quot; ==> '"' */` |
|     13 | 1869 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      9 | 1870 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      5 | 1871 | `			}else{` |
|      - | 1872 | `				/* Leave untouched */` |
|      5 | 1873 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      - | 1874 | `			}` |
|     13 | 1875 | `			nJump = (int)sizeof("&quot;")-1;` |
|      9 | 1876 | `		}else if( nLen >= (int)sizeof("&#039;")-1 && SyStrnicmp(zIn,"&#039;",sizeof("&#039;")-1) == 0 ){` |
|      - | 1877 | `			/* &#039; ==> ''' */` |
|      3 | 1878 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 1879 | `				/* Expand ''' */` |
|      3 | 1880 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      2 | 1881 | `			}else{` |
|      - | 1882 | `				/* Leave untouched */` |
|    ! 0 | 1883 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      - | 1884 | `			}` |
|      3 | 1885 | `			nJump = (int)sizeof("&#039;")-1;` |
|      1 | 1886 | `		}else if( nLen >= (int)sizeof(char) ){` |
|      - | 1887 | `			/* expand '&' */` |
|    ! 0 | 1888 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 1889 | `		}else{` |
|      - | 1890 | `			/* No more input to process */` |
|    ! 0 | 1891 | `			break;` |
|      - | 1892 | `		}` |
|     21 | 1893 | `		zIn += nJump;` |
|      1 | 1894 | `	}` |
|     11 | 1895 | `	return PH7_OK;` |
|      9 | 1896 |  |
|      - | 1897 | `/* HTML encoding/Decoding table` |
|      - | 1898 | ` * Source: Symisc RunTime API.[chm@symisc.net]` |
|      - | 1899 | ` */` |
|      - | 1900 | `static const char *azHtmlEscape[] = {` |
|      - | 1901 | ` 	"&lt;","<","&gt;",">","&amp;","&","&quot;","\"","&#39;","'",` |
|      - | 1902 | `	"&#33;","!","&#36;","$","&#35;","#","&#37;","%","&#40;","(",` |
|      - | 1903 | `	"&#41;",")","&#123;","{","&#125;","}","&#61;","=","&#43;","+",` |
|      - | 1904 | `	"&#63;","?","&#91;","[","&#93;","]","&#64;","@","&#44;",","` |
|      - | 1905 | ` };` |
|      - | 1906 | `/*` |
|      - | 1907 | ` * array get_html_translation_table(void)` |
|      - | 1908 | ` *  Returns the translation table used by htmlspecialchars() and htmlentities().` |
|      - | 1909 | ` * Parameters` |
|      - | 1910 | ` *  None` |
|      - | 1911 | ` * Return` |
|      - | 1912 | ` *  The translation table as an array or NULL on failure.` |
|      - | 1913 | ` */` |
|      4 | 1914 | `static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1915 |  |
|      - | 1916 | `	ph7_value *pArray,*pValue;` |
|      - | 1917 | `	sxu32 n;` |
|      - | 1918 | `	/* Element value */` |
|      5 | 1919 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      5 | 1920 | `	if( pValue == 0 ){` |
|    ! 0 | 1921 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 1922 | `		SXUNUSED(apArg);` |
|      - | 1923 | `		/* Return NULL */` |
|    ! 0 | 1924 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1925 | `		return PH7_OK;` |
|      - | 1926 | `	}` |
|      - | 1927 | `	/* Create a new array */` |
|      5 | 1928 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 | 1929 | `	if( pArray == 0 ){` |
|      - | 1930 | `		/* Return NULL */` |
|    ! 0 | 1931 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1932 | `		return PH7_OK;` |
|      - | 1933 | `	}` |
|      - | 1934 | `	/* Make the table */` |
|     85 | 1935 | `	for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|      - | 1936 | `		/* Prepare the value */` |
|     81 | 1937 | `		ph7_value_string(pValue,azHtmlEscape[n],-1 /* Compute length automatically */);` |
|      - | 1938 | `		/* Insert the value */` |
|     81 | 1939 | `		ph7_array_add_strkey_elem(pArray,azHtmlEscape[n+1],pValue);` |
|      - | 1940 | `		/* Reset the string cursor */` |
|     81 | 1941 | `		ph7_value_reset_string_cursor(pValue);` |
|     41 | 1942 | `	}` |
|      - | 1943 | `	/*` |
|      - | 1944 | `	 * Return the array.` |
|      - | 1945 | `	 * Don't worry about freeing memory, everything will be automatically` |
|      - | 1946 | `	 * released upon we return from this function.` |
|      - | 1947 | `	 */` |
|      5 | 1948 | `	ph7_result_value(pCtx,pArray);` |
|      5 | 1949 | `	return PH7_OK;` |
|      3 | 1950 |  |
|      - | 1951 | `/*` |
|      - | 1952 | ` * string htmlentities( string $string [, int $flags = ENT_COMPAT \| ENT_HTML401]);` |
|      - | 1953 | ` *   Convert all applicable characters to HTML entities` |
|      - | 1954 | ` * Parameters` |
|      - | 1955 | ` * $string` |
|      - | 1956 | ` *   The input string.` |
|      - | 1957 | ` * $flags` |
|      - | 1958 | ` *  A bitmask of one or more of the flags (see block-comment on PH7_builtin_htmlspecialchars())` |
|      - | 1959 | ` * Return` |
|      - | 1960 | ` * The encoded string.` |
|      - | 1961 | ` */` |
|     10 | 1962 | `static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1963 |  |
|     11 | 1964 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 1965 | `	const char *zIn,*zEnd;` |
|      - | 1966 | `	int nLen,c;` |
|      - | 1967 | `	sxu32 n;` |
|     11 | 1968 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1969 | `		/* Missing/Invalid arguments,return NULL */` |
|      7 | 1970 | `		ph7_result_null(pCtx);` |
|      7 | 1971 | `		return PH7_OK;` |
|      - | 1972 | `	}` |
|      - | 1973 | `	/* Extract the target string */` |
|      5 | 1974 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 1975 | `	zEnd = &zIn[nLen];` |
|      - | 1976 | `	/* Extract the flags if available */` |
|      5 | 1977 | `	if( nArg > 1 ){` |
|      3 | 1978 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      3 | 1979 | `		if( iFlags < 0 ){` |
|      3 | 1980 | `			iFlags = 0x01;` |
|      1 | 1981 | `		}` |
|      1 | 1982 | `	}` |
|      - | 1983 | `	/* Perform the requested operation */` |
|     11 | 1984 | `	for(;;){` |
|     23 | 1985 | `		if( zIn >= zEnd ){` |
|      - | 1986 | `			/* No more input to process */` |
|      5 | 1987 | `			break;` |
|      - | 1988 | `		}` |
|     19 | 1989 | `		c = zIn[0];` |
|      - | 1990 | `		/* Perform a linear lookup on the decoding table */` |
|    233 | 1991 | `		for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    223 | 1992 | `			if( azHtmlEscape[n+1][0] == c ){` |
|      - | 1993 | `				/* Got one */` |
|      9 | 1994 | `				break;` |
|      - | 1995 | `			}` |
|    108 | 1996 | `		}` |
|     19 | 1997 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|      - | 1998 | `			/* Output the safe sequence [i.e: '<' ==> '&lt;"] */` |
|      9 | 1999 | `			if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 2000 | `				/* Expand the double quote verbatim */` |
|    ! 0 | 2001 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      9 | 2002 | `			}else if(c == '\'' && ((iFlags & 0x02 /*ENT_QUOTES*/) == 0 \|\| (iFlags & 0x04) /*ENT_NOQUOTES*/) ){` |
|      - | 2003 | `				/* expand single quote verbatim */` |
|    ! 0 | 2004 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|    ! 0 | 2005 | `			}else{` |
|      9 | 2006 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1/*Compute length automatically */);` |
|      - | 2007 | `			}` |
|      5 | 2008 | `		}else{` |
|      - | 2009 | `			/* Output character verbatim */` |
|     11 | 2010 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2011 | `		}` |
|     19 | 2012 | `		zIn++;` |
|      1 | 2013 | `	}` |
|      5 | 2014 | `	return PH7_OK;` |
|      6 | 2015 |  |
|      - | 2016 | `/*` |
|      - | 2017 | ` * string html_entity_decode(string $string [, int $quote_style = ENT_COMPAT [, string $charset = 'UTF-8' ]])` |
|      - | 2018 | ` *   Perform the reverse operation of html_entity_decode().` |
|      - | 2019 | ` * Parameters` |
|      - | 2020 | ` * $string` |
|      - | 2021 | ` *   The input string.` |
|      - | 2022 | ` * $flags` |
|      - | 2023 | ` *  A bitmask of one or more of the flags (see comment on PH7_builtin_htmlspecialchars())` |
|      - | 2024 | ` * Return` |
|      - | 2025 | ` * The decoded string.` |
|      - | 2026 | ` */` |
|     28 | 2027 | `static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2028 |  |
|      - | 2029 | `	const char *zCur,*zIn,*zEnd;` |
|     29 | 2030 | `	int iFlags = 0x01; /* ENT_COMPAT  */` |
|      - | 2031 | `	int nLen;` |
|      - | 2032 | `	sxu32 n;` |
|     29 | 2033 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2034 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 2035 | `		ph7_result_null(pCtx);` |
|      5 | 2036 | `		return PH7_OK;` |
|      - | 2037 | `	}` |
|      - | 2038 | `	/* Extract the target string */` |
|     25 | 2039 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 2040 | `	zEnd = &zIn[nLen];` |
|      - | 2041 | `	/* Extract the flags if available */` |
|     25 | 2042 | `	if( nArg > 1 ){` |
|     15 | 2043 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     15 | 2044 | `		if( iFlags < 0 ){` |
|      3 | 2045 | `			iFlags = 0x01;` |
|      1 | 2046 | `		}` |
|      7 | 2047 | `	}` |
|      - | 2048 | `	/* Perform the requested operation */` |
|     27 | 2049 | `	for(;;){` |
|     55 | 2050 | `		if( zIn >= zEnd ){` |
|      - | 2051 | `			/* No more input to process */` |
|     13 | 2052 | `			break;` |
|      - | 2053 | `		}` |
|     43 | 2054 | `		zCur = zIn;` |
|    173 | 2055 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|    131 | 2056 | `			zIn++;` |
|      1 | 2057 | `		}` |
|     43 | 2058 | `		if( zCur < zIn ){` |
|      - | 2059 | `			/* Append raw string verbatim */` |
|     27 | 2060 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     13 | 2061 | `		}` |
|     43 | 2062 | `		if( zIn >= zEnd ){` |
|     13 | 2063 | `			break;` |
|      - | 2064 | `		}` |
|     31 | 2065 | `		nLen = (int)(zEnd-zIn);` |
|      - | 2066 | `		/* Find an encoded sequence */` |
|    113 | 2067 | `		for(n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    113 | 2068 | `			int iLen = (int)SyStrlen(azHtmlEscape[n]);` |
|    113 | 2069 | `			if( nLen >= iLen && SyStrnicmp(zIn,azHtmlEscape[n],(sxu32)iLen) == 0 ){` |
|      - | 2070 | `				/* Got one */` |
|     31 | 2071 | `				zIn += iLen;` |
|     31 | 2072 | `				break;` |
|      - | 2073 | `			}` |
|     42 | 2074 | `		}` |
|     31 | 2075 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|     31 | 2076 | `			int c = azHtmlEscape[n+1][0];` |
|      - | 2077 | `			/* Output the decoded character */` |
|     31 | 2078 | `			if( c == '\'' && ((iFlags & 0x02) == 0 /*ENT_QUOTES*/\|\| (iFlags & 0x04) /*ENT_NOQUOTES*/)  ){` |
|      - | 2079 | `				/* Do not process single quotes */` |
|      9 | 2080 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|     27 | 2081 | `			}else if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 2082 | `				/* Do not process double quotes */` |
|      5 | 2083 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|      3 | 2084 | `			}else{` |
|     19 | 2085 | `				ph7_result_string(pCtx,azHtmlEscape[n+1],-1); /* Compute length automatically */` |
|      - | 2086 | `			}` |
|     16 | 2087 | `		}else{` |
|      - | 2088 | `			/* Append '&' */` |
|    ! 0 | 2089 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 2090 | `			zIn++;` |
|      - | 2091 | `		}` |
|      1 | 2092 | `	}` |
|     25 | 2093 | `	return PH7_OK;` |
|     15 | 2094 |  |
|      - | 2095 | `/*` |
|      - | 2096 | ` * int strlen($string)` |
|      - | 2097 | ` *  return the length of the given string.` |
|      - | 2098 | ` * Parameter` |
|      - | 2099 | ` *  string: The string being measured for length.` |
|      - | 2100 | ` * Return` |
|      - | 2101 | ` *  length of the given string.` |
|      - | 2102 | ` */` |
|   1496 | 2103 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2104 |  |
|   1498 | 2105 | `	int iLen = 0;` |
|   1498 | 2106 | `	if( nArg > 0 ){` |
|   1496 | 2107 | `		ph7_value_to_string(apArg[0],&iLen);` |
|    747 | 2108 | `	}` |
|      - | 2109 | `	/* String length */` |
|   1498 | 2110 | `	ph7_result_int(pCtx,iLen);` |
|   1498 | 2111 | `	return PH7_OK;` |
|      2 | 2112 |  |
|      - | 2113 | `/*` |
|      - | 2114 | ` * int strcmp(string $str1,string $str2)` |
|      - | 2115 | ` *  Perform a binary safe string comparison.` |
|      - | 2116 | ` * Parameter` |
|      - | 2117 | ` *  str1: The first string` |
|      - | 2118 | ` *  str2: The second string` |
|      - | 2119 | ` * Return` |
|      - | 2120 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2121 | ` *  than str2, and 0 if they are equal.` |
|      - | 2122 | ` */` |
|     50 | 2123 | `static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2124 |  |
|      - | 2125 | `	const char *z1,*z2;` |
|      - | 2126 | `	int n1,n2;` |
|      - | 2127 | `	int res;` |
|     51 | 2128 | `	if( nArg < 2 ){` |
|      5 | 2129 | `		res = nArg == 0 ? 0 : 1;` |
|      5 | 2130 | `		ph7_result_int(pCtx,res);` |
|      5 | 2131 | `		return PH7_OK;` |
|      - | 2132 | `	}` |
|      - | 2133 | `	/* Perform the comparison */` |
|     47 | 2134 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     47 | 2135 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     47 | 2136 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2137 | `	/* Comparison result */` |
|     47 | 2138 | `	ph7_result_int(pCtx,res);` |
|     47 | 2139 | `	return PH7_OK;` |
|     26 | 2140 |  |
|      - | 2141 | `/*` |
|      - | 2142 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 2143 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 2144 | ` * Parameter` |
|      - | 2145 | ` *  str1: The first string` |
|      - | 2146 | ` *  str2: The second string` |
|      - | 2147 | ` * Return` |
|      - | 2148 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2149 | ` *  than str2, and 0 if they are equal.` |
|      - | 2150 | ` */` |
|     20 | 2151 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2152 |  |
|      - | 2153 | `	const char *z1,*z2;` |
|      - | 2154 | `	int res;` |
|      - | 2155 | `	int n;` |
|     21 | 2156 | `	if( nArg < 3 ){` |
|      - | 2157 | `		/* Perform a standard comparison */` |
|      5 | 2158 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 2159 | `	}` |
|      - | 2160 | `	/* Desired comparison length */` |
|     17 | 2161 | `	n  = ph7_value_to_int(apArg[2]);` |
|     17 | 2162 | `	if( n < 0 ){` |
|      - | 2163 | `		/* Invalid length */` |
|      3 | 2164 | `		ph7_result_int(pCtx,-1);` |
|      3 | 2165 | `		return PH7_OK;` |
|      - | 2166 | `	}` |
|      - | 2167 | `	/* Perform the comparison */` |
|     15 | 2168 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     15 | 2169 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     15 | 2170 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 2171 | `	/* Comparison result */` |
|     15 | 2172 | `	ph7_result_int(pCtx,res);` |
|     15 | 2173 | `	return PH7_OK;` |
|     11 | 2174 |  |
|      - | 2175 | `/*` |
|      - | 2176 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 2177 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 2178 | ` * Parameter` |
|      - | 2179 | ` *  str1: The first string` |
|      - | 2180 | ` *  str2: The second string` |
|      - | 2181 | ` * Return` |
|      - | 2182 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2183 | ` *  than str2, and 0 if they are equal.` |
|      - | 2184 | ` */` |
|     18 | 2185 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2186 |  |
|      - | 2187 | `	const char *z1,*z2;` |
|      - | 2188 | `	int n1,n2;` |
|      - | 2189 | `	int res;` |
|     19 | 2190 | `	if( nArg < 2 ){` |
|      9 | 2191 | `		res = nArg == 0 ? 0 : 1;` |
|      9 | 2192 | `		ph7_result_int(pCtx,res);` |
|      9 | 2193 | `		return PH7_OK;` |
|      - | 2194 | `	}` |
|      - | 2195 | `	/* Perform the comparison */` |
|     11 | 2196 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     11 | 2197 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     11 | 2198 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2199 | `	/* Comparison result */` |
|     11 | 2200 | `	ph7_result_int(pCtx,res);` |
|     11 | 2201 | `	return PH7_OK;` |
|     10 | 2202 |  |
|      - | 2203 | `/*` |
|      - | 2204 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 2205 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 2206 | ` * Parameter` |
|      - | 2207 | ` *  $str1: The first string` |
|      - | 2208 | ` *  $str2: The second string` |
|      - | 2209 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 2210 | ` * Return` |
|      - | 2211 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2212 | ` *  than str2, and 0 if they are equal.` |
|      - | 2213 | ` */` |
|      8 | 2214 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2215 |  |
|      - | 2216 | `	const char *z1,*z2;` |
|      - | 2217 | `	int res;` |
|      - | 2218 | `	int n;` |
|      9 | 2219 | `	if( nArg < 3 ){` |
|      - | 2220 | `		/* Perform a standard comparison */` |
|      5 | 2221 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 2222 | `	}` |
|      - | 2223 | `	/* Desired comparison length */` |
|      5 | 2224 | `	n  = ph7_value_to_int(apArg[2]);` |
|      5 | 2225 | `	if( n < 0 ){` |
|      - | 2226 | `		/* Invalid length */` |
|    ! 0 | 2227 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2228 | `		return PH7_OK;` |
|      - | 2229 | `	}` |
|      - | 2230 | `	/* Perform the comparison */` |
|      5 | 2231 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|      5 | 2232 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|      5 | 2233 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 2234 | `	/* Comparison result */` |
|      5 | 2235 | `	ph7_result_int(pCtx,res);` |
|      5 | 2236 | `	return PH7_OK;` |
|      5 | 2237 |  |
|      - | 2238 | `/*` |
|      - | 2239 | ` * Implode context [i.e: it's private data].` |
|      - | 2240 | ` * A pointer to the following structure is forwarded` |
|      - | 2241 | ` * verbatim to the array walker callback defined below.` |
|      - | 2242 | ` */` |
|      - | 2243 | `struct implode_data {` |
|      - | 2244 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 2245 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 2246 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 2247 | `	int nSeplen;          /* Separator length */` |
|      - | 2248 | `	int bFirst;           /* TRUE if first call */` |
|      - | 2249 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 2250 | `};` |
|      - | 2251 | `/*` |
|      - | 2252 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 2253 | ` * The following routine is invoked for each array entry passed` |
|      - | 2254 | ` * to the implode() function.` |
|      - | 2255 | ` */` |
|  78530 | 2256 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      2 | 2257 |  |
|  39265 | 2258 | `	SXUNUSED(pKey);` |
|  78532 | 2259 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 2260 | `	const char *zData;` |
|      - | 2261 | `	int nLen;` |
|  78532 | 2262 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 2263 | `		if( pData->nSeplen > 0 ){` |
|      3 | 2264 | `			if( !pData->bFirst ){` |
|      - | 2265 | `				/* append the separator first */` |
|      3 | 2266 | `				ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|      2 | 2267 | `			}else{` |
|    ! 0 | 2268 | `				pData->bFirst = 0;` |
|      - | 2269 | `			}` |
|      1 | 2270 | `		}` |
|      - | 2271 | `		/* Recurse */` |
|      3 | 2272 | `		pData->bFirst = 1;` |
|      3 | 2273 | `		pData->nRecCount++;` |
|      3 | 2274 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 2275 | `		pData->nRecCount--;` |
|      3 | 2276 | `		return PH7_OK;` |
|      - | 2277 | `	}` |
|      - | 2278 | `	/* Extract the string representation of the entry value */` |
|  78530 | 2279 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 2280 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
|  78530 | 2281 | `	if( pData->bFirst ){` |
|  16858 | 2282 | `		pData->bFirst = 0;` |
|  70102 | 2283 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 2284 | `		/* append the separator first */` |
|  61662 | 2285 | `		ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|  30830 | 2286 | `	}` |
|      - | 2287 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
|  78530 | 2288 | `	if( nLen > 0 ){` |
|  71674 | 2289 | `		ph7_result_string(pData->pCtx,zData,nLen);` |
|  35836 | 2290 | `	}` |
|  78530 | 2291 | `	return PH7_OK;` |
|  39267 | 2292 |  |
|      - | 2293 | `/*` |
|      - | 2294 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 2295 | ` * string implode(array $pieces,...)` |
|      - | 2296 | ` *  Join array elements with a string.` |
|      - | 2297 | ` * $glue` |
|      - | 2298 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 2299 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 2300 | ` * $pieces` |
|      - | 2301 | ` *   The array of strings to implode.` |
|      - | 2302 | ` * Return` |
|      - | 2303 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 2304 | ` *  order, with the glue string between each element.` |
|      - | 2305 | ` */` |
|  16884 | 2306 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2307 |  |
|      - | 2308 | `	struct implode_data imp_data;` |
|  16886 | 2309 | `	int i = 1;` |
|  16886 | 2310 | `	if( nArg < 1 ){` |
|      - | 2311 | `		/* Missing argument,return NULL */` |
|    ! 0 | 2312 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2313 | `		return PH7_OK;` |
|      - | 2314 | `	}` |
|      - | 2315 | `	/* Prepare the implode context */` |
|  16886 | 2316 | `	imp_data.pCtx = pCtx;` |
|  16886 | 2317 | `	imp_data.bRecursive = 0;` |
|  16886 | 2318 | `	imp_data.bFirst = 1;` |
|  16886 | 2319 | `	imp_data.nRecCount = 0;` |
|  16886 | 2320 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  16884 | 2321 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|   8443 | 2322 | `	}else{` |
|      3 | 2323 | `		imp_data.zSep = 0;` |
|      3 | 2324 | `		imp_data.nSeplen = 0;` |
|      3 | 2325 | `		i = 0;` |
|      - | 2326 | `	}` |
|  16886 | 2327 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 2328 | `	/* Start the 'join' process */` |
|  33770 | 2329 | `	while( i < nArg ){` |
|  16886 | 2330 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2331 | `			/* Iterate throw array entries */` |
|  16886 | 2332 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|   8444 | 2333 | `		}else{` |
|      - | 2334 | `			const char *zData;` |
|      - | 2335 | `			int nLen;` |
|      - | 2336 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 2337 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2338 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 2339 | `			if( imp_data.bFirst ){` |
|    ! 0 | 2340 | `				imp_data.bFirst = 0;` |
|    ! 0 | 2341 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2342 | `				ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen);` |
|    ! 0 | 2343 | `			}` |
|      - | 2344 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 2345 | `			if( nLen > 0 ){` |
|    ! 0 | 2346 | `				ph7_result_string(pCtx,zData,nLen);` |
|    ! 0 | 2347 | `			}` |
|      - | 2348 | `		}` |
|  16886 | 2349 | `		i++;` |
|      2 | 2350 | `	}` |
|  16886 | 2351 | `	return PH7_OK;` |
|   8444 | 2352 |  |
|      - | 2353 | `/*` |
|      - | 2354 | ` * Symisc eXtension:` |
|      - | 2355 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 2356 | ` * Purpose` |
|      - | 2357 | ` *  Same as implode() but recurse on arrays.` |
|      - | 2358 | ` * Example:` |
|      - | 2359 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 2360 | ` *   echo implode_recursive("/",$a);` |
|      - | 2361 | ` *   Will output` |
|      - | 2362 | ` *     usr/home/dean.` |
|      - | 2363 | ` *   While the standard implode would produce.` |
|      - | 2364 | ` *    usr/Array.` |
|      - | 2365 | ` * Parameter` |
|      - | 2366 | ` *  Refer to implode().` |
|      - | 2367 | ` * Return` |
|      - | 2368 | ` *  Refer to implode().` |
|      - | 2369 | ` */` |
|     12 | 2370 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2371 |  |
|      - | 2372 | `	struct implode_data imp_data;` |
|     13 | 2373 | `	int i = 1;` |
|     13 | 2374 | `	if( nArg < 1 ){` |
|      - | 2375 | `		/* Missing argument,return NULL */` |
|      3 | 2376 | `		ph7_result_null(pCtx);` |
|      3 | 2377 | `		return PH7_OK;` |
|      - | 2378 | `	}` |
|      - | 2379 | `	/* Prepare the implode context */` |
|     11 | 2380 | `	imp_data.pCtx = pCtx;` |
|     11 | 2381 | `	imp_data.bRecursive = 1;` |
|     11 | 2382 | `	imp_data.bFirst = 1;` |
|     11 | 2383 | `	imp_data.nRecCount = 0;` |
|     11 | 2384 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 2385 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 2386 | `	}else{` |
|    ! 0 | 2387 | `		imp_data.zSep = 0;` |
|    ! 0 | 2388 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 2389 | `		i = 0;` |
|      - | 2390 | `	}` |
|     11 | 2391 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 2392 | `	/* Start the 'join' process */` |
|     21 | 2393 | `	while( i < nArg ){` |
|     11 | 2394 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2395 | `			/* Iterate throw array entries */` |
|      3 | 2396 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      2 | 2397 | `		}else{` |
|      - | 2398 | `			const char *zData;` |
|      - | 2399 | `			int nLen;` |
|      - | 2400 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 2401 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2402 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 2403 | `			if( imp_data.bFirst ){` |
|      9 | 2404 | `				imp_data.bFirst = 0;` |
|      4 | 2405 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2406 | `				ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen);` |
|    ! 0 | 2407 | `			}` |
|      - | 2408 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 2409 | `			if( nLen > 0 ){` |
|      9 | 2410 | `				ph7_result_string(pCtx,zData,nLen);` |
|      4 | 2411 | `			}` |
|      - | 2412 | `		}` |
|     11 | 2413 | `		i++;` |
|      1 | 2414 | `	}` |
|     11 | 2415 | `	return PH7_OK;` |
|      7 | 2416 |  |
|      - | 2417 | `/*` |
|      - | 2418 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 2419 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 2420 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 2421 | ` * Parameters` |
|      - | 2422 | ` *  $delimiter` |
|      - | 2423 | ` *   The boundary string.` |
|      - | 2424 | ` * $string` |
|      - | 2425 | ` *   The input string.` |
|      - | 2426 | ` * $limit` |
|      - | 2427 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 2428 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 2429 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 2430 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 2431 | ` * Returns` |
|      - | 2432 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 2433 | ` *  on boundaries formed by the delimiter.` |
|      - | 2434 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 2435 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 2436 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 2437 | ` *  will be returned.` |
|      - | 2438 | ` * NOTE:` |
|      - | 2439 | ` *  Negative limit is not supported.` |
|      - | 2440 | ` */` |
|   3046 | 2441 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2442 |  |
|      - | 2443 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 2444 | `	int nDelim,nStrlen,iLimit;` |
|      - | 2445 | `	ph7_value *pArray;` |
|      - | 2446 | `	ph7_value *pValue;` |
|      - | 2447 | `	sxu32 nOfft;` |
|      - | 2448 | `	sxi32 rc;` |
|   3048 | 2449 | `	if( nArg < 2 ){` |
|      - | 2450 | `		/* Missing arguments,return FALSE */` |
|      9 | 2451 | `		ph7_result_bool(pCtx,0);` |
|      9 | 2452 | `		return PH7_OK;` |
|      - | 2453 | `	}` |
|      - | 2454 | `	/* Extract the delimiter */` |
|   3040 | 2455 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   3040 | 2456 | `	if( nDelim < 1 ){` |
|      - | 2457 | `		/* Empty delimiter,return FALSE */` |
|      3 | 2458 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2459 | `		return PH7_OK;` |
|      - | 2460 | `	}` |
|      - | 2461 | `	/* Extract the string */` |
|   3038 | 2462 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   3038 | 2463 | `	if( nStrlen < 1 ){` |
|      - | 2464 | `		/* Empty string: return an array with a single empty element (PHP behavior) */` |
|      3 | 2465 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      3 | 2466 | `		ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      3 | 2467 | `		if( pArrayTmp == 0 \|\| pValueTmp == 0 ){` |
|      - | 2468 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 2469 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2470 | `			return PH7_OK;` |
|      - | 2471 | `		}` |
|      3 | 2472 | `		ph7_value_string(pValueTmp, "", 0);` |
|      3 | 2473 | `		ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp);` |
|      3 | 2474 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      3 | 2475 | `		return PH7_OK;` |
|      - | 2476 | `	}` |
|      - | 2477 | `	/* Point to the end of the string */` |
|   3036 | 2478 | `	zEnd = &zString[nStrlen];` |
|      - | 2479 | `	/* Create the array */` |
|   3036 | 2480 | `	pArray =  ph7_context_new_array(pCtx);` |
|   3036 | 2481 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   3036 | 2482 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2483 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 2484 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2485 | `		return PH7_OK;` |
|      - | 2486 | `	}` |
|      - | 2487 | `	/* Set a defualt limit */` |
|   3036 | 2488 | `	iLimit = SXI32_HIGH;` |
|   3036 | 2489 | `	if( nArg > 2 ){` |
|      9 | 2490 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|      9 | 2491 | `		 if( iLimit < 0 ){` |
|      3 | 2492 | `			iLimit = -iLimit;` |
|      1 | 2493 | `		}` |
|      9 | 2494 | `		if( iLimit == 0 ){` |
|      3 | 2495 | `			iLimit = 1;` |
|      1 | 2496 | `		}` |
|      9 | 2497 | `		iLimit--;` |
|      4 | 2498 | `	}` |
|      - | 2499 | `	/* Start exploding */` |
|  37481 | 2500 | `	for(;;){` |
|  74964 | 2501 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
|  74964 | 2502 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 2503 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   3036 | 2504 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   3036 | 2505 | `			ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|   3036 | 2506 | `			break;` |
|      - | 2507 | `		}` |
|      - | 2508 | `		/* Point to the desired offset */` |
|  71930 | 2509 | `		zCur = &zString[nOfft];` |
|      - | 2510 | `		/* Perform the store operation (may be empty) */` |
|  71930 | 2511 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|  71930 | 2512 | `		ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|      - | 2513 | `		/* Point beyond the delimiter */` |
|  71930 | 2514 | `		zString = &zCur[nDelim];` |
|      - | 2515 | `		/* Reset the cursor */` |
|  71930 | 2516 | `		ph7_value_reset_string_cursor(pValue);` |
|      2 | 2517 | `	}` |
|      - | 2518 | `	/* Return the freshly created array */` |
|   3036 | 2519 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2520 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 2521 | `	 * released as soon we return from this foregin function.` |
|      - | 2522 | `	 */` |
|   3036 | 2523 | `	return PH7_OK;` |
|   1525 | 2524 |  |
|      - | 2525 | `/*` |
|      - | 2526 | ` * string trim(string $str[,string $charlist ])` |
|      - | 2527 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2528 | ` * Parameters` |
|      - | 2529 | ` *  $str` |
|      - | 2530 | ` *   The string that will be trimmed.` |
|      - | 2531 | ` * $charlist` |
|      - | 2532 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2533 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2534 | ` *   With .. you can specify a range of characters.` |
|      - | 2535 | ` * Returns.` |
|      - | 2536 | ` *  Thr processed string.` |
|      - | 2537 | ` * NOTE:` |
|      - | 2538 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 2539 | ` */` |
|   7702 | 2540 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2541 |  |
|      - | 2542 | `	const char *zString;` |
|      - | 2543 | `	int nLen;` |
|   7704 | 2544 | `	if( nArg < 1 ){` |
|      - | 2545 | `		/* Missing arguments,return null */` |
|      3 | 2546 | `		ph7_result_null(pCtx);` |
|      3 | 2547 | `		return PH7_OK;` |
|      - | 2548 | `	}` |
|      - | 2549 | `	/* Extract the target string */` |
|   7702 | 2550 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|   7702 | 2551 | `	if( nLen < 1 ){` |
|      - | 2552 | `		/* Empty string,return */` |
|   1668 | 2553 | `		ph7_result_string(pCtx,"",0);` |
|   1668 | 2554 | `		return PH7_OK;` |
|      - | 2555 | `	}` |
|      - | 2556 | `	/* Start the trim process */` |
|   6036 | 2557 | `	if( nArg < 2 ){` |
|      - | 2558 | `		SyString sStr;` |
|      - | 2559 | `		/* Remove white spaces and NUL bytes */` |
|   6032 | 2560 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  14640 | 2561 | `		SyStringFullTrimSafe(&sStr);` |
|   6032 | 2562 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   3017 | 2563 | `	}else{` |
|      - | 2564 | `		/* Char list */` |
|      - | 2565 | `		const char *zList;` |
|      - | 2566 | `		int nListlen;` |
|      5 | 2567 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 2568 | `		if( nListlen < 1 ){` |
|      - | 2569 | `			/* Return the string unchanged */` |
|      3 | 2570 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2571 | `		}else{` |
|      3 | 2572 | `			const char *zEnd = &zString[nLen];` |
|      3 | 2573 | `			const char *zCur = zString;` |
|      - | 2574 | `			const char *zPtr;` |
|      - | 2575 | `			int i;` |
|      - | 2576 | `			/* Left trim */` |
|      4 | 2577 | `			for(;;){` |
|      9 | 2578 | `				if( zCur >= zEnd ){` |
|    ! 0 | 2579 | `					break;` |
|      - | 2580 | `				}` |
|      9 | 2581 | `				zPtr = zCur;` |
|     17 | 2582 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 2583 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|      7 | 2584 | `						zCur++;` |
|      3 | 2585 | `					}` |
|      5 | 2586 | `				}` |
|      9 | 2587 | `				if( zCur == zPtr ){` |
|      - | 2588 | `					/* No match,break immediately */` |
|      3 | 2589 | `					break;` |
|      - | 2590 | `				}` |
|      1 | 2591 | `			}` |
|      - | 2592 | `			/* Right trim */` |
|      3 | 2593 | `			zEnd--;` |
|      4 | 2594 | `			for(;;){` |
|      9 | 2595 | `				if( zEnd <= zCur ){` |
|    ! 0 | 2596 | `					break;` |
|      - | 2597 | `				}` |
|      9 | 2598 | `				zPtr = zEnd;` |
|     17 | 2599 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 2600 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      7 | 2601 | `						zEnd--;` |
|      3 | 2602 | `					}` |
|      5 | 2603 | `				}` |
|      9 | 2604 | `				if( zEnd == zPtr ){` |
|      3 | 2605 | `					break;` |
|      - | 2606 | `				}` |
|      1 | 2607 | `			}` |
|      3 | 2608 | `			if( zCur >= zEnd ){` |
|      - | 2609 | `				/* Return the empty string */` |
|    ! 0 | 2610 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2611 | `			}else{` |
|      3 | 2612 | `				zEnd++;` |
|      3 | 2613 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2614 | `			}` |
|      - | 2615 | `		}` |
|      - | 2616 | `	}` |
|   6036 | 2617 | `	return PH7_OK;` |
|   3853 | 2618 |  |
|      - | 2619 | `/*` |
|      - | 2620 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 2621 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 2622 | ` * Parameters` |
|      - | 2623 | ` *  $str` |
|      - | 2624 | ` *   The string that will be trimmed.` |
|      - | 2625 | ` * $charlist` |
|      - | 2626 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2627 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2628 | ` *   With .. you can specify a range of characters.` |
|      - | 2629 | ` * Returns.` |
|      - | 2630 | ` *  Thr processed string.` |
|      - | 2631 | ` * NOTE:` |
|      - | 2632 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 2633 | ` */` |
|     26 | 2634 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2635 |  |
|      - | 2636 | `	const char *zString;` |
|      - | 2637 | `	int nLen;` |
|     27 | 2638 | `	if( nArg < 1 ){` |
|      - | 2639 | `		/* Missing arguments,return null */` |
|      3 | 2640 | `		ph7_result_null(pCtx);` |
|      3 | 2641 | `		return PH7_OK;` |
|      - | 2642 | `	}` |
|      - | 2643 | `	/* Extract the target string */` |
|     25 | 2644 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 2645 | `	if( nLen < 1 ){` |
|      - | 2646 | `		/* Empty string,return */` |
|      5 | 2647 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2648 | `		return PH7_OK;` |
|      - | 2649 | `	}` |
|      - | 2650 | `	/* Start the trim process */` |
|     21 | 2651 | `	if( nArg < 2 ){` |
|      - | 2652 | `		SyString sStr;` |
|      - | 2653 | `		/* Remove white spaces and NUL bytes*/` |
|     17 | 2654 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     41 | 2655 | `		SyStringRightTrimSafe(&sStr);` |
|     17 | 2656 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      9 | 2657 | `	}else{` |
|      - | 2658 | `		/* Char list */` |
|      - | 2659 | `		const char *zList;` |
|      - | 2660 | `		int nListlen;` |
|      5 | 2661 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 2662 | `		if( nListlen < 1 ){` |
|      - | 2663 | `			/* Return the string unchanged */` |
|    ! 0 | 2664 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 2665 | `		}else{` |
|      5 | 2666 | `			const char *zEnd = &zString[nLen - 1];` |
|      5 | 2667 | `			const char *zCur = zString;` |
|      - | 2668 | `			const char *zPtr;` |
|      - | 2669 | `			int i;` |
|      - | 2670 | `			/* Right trim */` |
|      6 | 2671 | `			for(;;){` |
|     13 | 2672 | `				if( zEnd <= zCur ){` |
|    ! 0 | 2673 | `					break;` |
|      - | 2674 | `				}` |
|     13 | 2675 | `				zPtr = zEnd;` |
|     25 | 2676 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     13 | 2677 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      9 | 2678 | `						zEnd--;` |
|      4 | 2679 | `					}` |
|      7 | 2680 | `				}` |
|     13 | 2681 | `				if( zEnd == zPtr ){` |
|      5 | 2682 | `					break;` |
|      - | 2683 | `				}` |
|      1 | 2684 | `			}` |
|      5 | 2685 | `			if( zEnd <= zCur ){` |
|      - | 2686 | `				/* Return the empty string */` |
|    ! 0 | 2687 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2688 | `			}else{` |
|      5 | 2689 | `				zEnd++;` |
|      5 | 2690 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2691 | `			}` |
|      - | 2692 | `		}` |
|      - | 2693 | `	}` |
|     21 | 2694 | `	return PH7_OK;` |
|     14 | 2695 |  |
|      - | 2696 | `/*` |
|      - | 2697 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 2698 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2699 | ` * Parameters` |
|      - | 2700 | ` *  $str` |
|      - | 2701 | ` *   The string that will be trimmed.` |
|      - | 2702 | ` * $charlist` |
|      - | 2703 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2704 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2705 | ` *   With .. you can specify a range of characters.` |
|      - | 2706 | ` * Returns.` |
|      - | 2707 | ` *  Thr processed string.` |
|      - | 2708 | ` * NOTE:` |
|      - | 2709 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 2710 | ` */` |
|     12 | 2711 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2712 |  |
|      - | 2713 | `	const char *zString;` |
|      - | 2714 | `	int nLen;` |
|     13 | 2715 | `	if( nArg < 1 ){` |
|      - | 2716 | `		/* Missing arguments,return null */` |
|      3 | 2717 | `		ph7_result_null(pCtx);` |
|      3 | 2718 | `		return PH7_OK;` |
|      - | 2719 | `	}` |
|      - | 2720 | `	/* Extract the target string */` |
|     11 | 2721 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 2722 | `	if( nLen < 1 ){` |
|      - | 2723 | `		/* Empty string,return */` |
|    ! 0 | 2724 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2725 | `		return PH7_OK;` |
|      - | 2726 | `	}` |
|      - | 2727 | `	/* Start the trim process */` |
|     11 | 2728 | `	if( nArg < 2 ){` |
|      - | 2729 | `		SyString sStr;` |
|      - | 2730 | `		/* Remove white spaces and NUL byte */` |
|      3 | 2731 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|      8 | 2732 | `		SyStringLeftTrimSafe(&sStr);` |
|      3 | 2733 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      2 | 2734 | `	}else{` |
|      - | 2735 | `		/* Char list */` |
|      - | 2736 | `		const char *zList;` |
|      - | 2737 | `		int nListlen;` |
|      9 | 2738 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      9 | 2739 | `		if( nListlen < 1 ){` |
|      - | 2740 | `			/* Return the string unchanged */` |
|      3 | 2741 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2742 | `		}else{` |
|      7 | 2743 | `			const char *zEnd = &zString[nLen];` |
|      7 | 2744 | `			const char *zCur = zString;` |
|      - | 2745 | `			const char *zPtr;` |
|      - | 2746 | `			int i;` |
|      - | 2747 | `			/* Left trim */` |
|      7 | 2748 | `			for(;;){` |
|     15 | 2749 | `				if( zCur >= zEnd ){` |
|    ! 0 | 2750 | `					break;` |
|      - | 2751 | `				}` |
|     15 | 2752 | `				zPtr = zCur;` |
|     41 | 2753 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     27 | 2754 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|     13 | 2755 | `						zCur++;` |
|      6 | 2756 | `					}` |
|     14 | 2757 | `				}` |
|     15 | 2758 | `				if( zCur == zPtr ){` |
|      - | 2759 | `					/* No match,break immediately */` |
|      7 | 2760 | `					break;` |
|      - | 2761 | `				}` |
|      1 | 2762 | `			}` |
|      7 | 2763 | `			if( zCur >= zEnd ){` |
|      - | 2764 | `				/* Return the empty string */` |
|    ! 0 | 2765 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2766 | `			}else{` |
|      7 | 2767 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2768 | `			}` |
|      - | 2769 | `		}` |
|      - | 2770 | `	}` |
|     11 | 2771 | `	return PH7_OK;` |
|      7 | 2772 |  |
|      - | 2773 | `/*` |
|      - | 2774 | ` * string strtolower(string $str)` |
|      - | 2775 | ` *  Make a string lowercase.` |
|      - | 2776 | ` * Parameters` |
|      - | 2777 | ` *  $str` |
|      - | 2778 | ` *   The input string.` |
|      - | 2779 | ` * Returns.` |
|      - | 2780 | ` *  The lowercased string.` |
|      - | 2781 | ` */` |
|  16746 | 2782 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2783 |  |
|      - | 2784 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2785 | `	int nLen;` |
|  16748 | 2786 | `	if( nArg < 1 ){` |
|      - | 2787 | `		/* Missing arguments,return null */` |
|      3 | 2788 | `		ph7_result_null(pCtx);` |
|      3 | 2789 | `		return PH7_OK;` |
|      - | 2790 | `	}` |
|      - | 2791 | `	/* Extract the target string */` |
|  16746 | 2792 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  16746 | 2793 | `	if( nLen < 1 ){` |
|      - | 2794 | `		/* Empty string,return */` |
|      3 | 2795 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2796 | `		return PH7_OK;` |
|      - | 2797 | `	}` |
|      - | 2798 | `	/* Perform the requested operation */` |
|  16744 | 2799 | `	zEnd = &zString[nLen];` |
|  52953 | 2800 | `	for(;;){` |
| 105908 | 2801 | `		if( zString >= zEnd ){` |
|      - | 2802 | `			/* No more input,break immediately */` |
|  16744 | 2803 | `			break;` |
|      - | 2804 | `		}` |
|  89166 | 2805 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2806 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2807 | `			zCur = zString;` |
|    ! 0 | 2808 | `			zString++;` |
|    ! 0 | 2809 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2810 | `				zString++;` |
|    ! 0 | 2811 | `			}` |
|      - | 2812 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2813 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2814 | `		}else{` |
|  89166 | 2815 | `			int c = zString[0];` |
|  89166 | 2816 | `			if( SyisUpper(c) ){` |
|  89164 | 2817 | `				c = SyToLower(zString[0]);` |
|  44581 | 2818 | `			}` |
|      - | 2819 | `			/* Append character */` |
|  89166 | 2820 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2821 | `			/* Advance the cursor */` |
|  89166 | 2822 | `			zString++;` |
|      - | 2823 | `		}` |
|      2 | 2824 | `	}` |
|  16744 | 2825 | `	return PH7_OK;` |
|   8375 | 2826 |  |
|      - | 2827 | `/*` |
|      - | 2828 | ` * string strtolower(string $str)` |
|      - | 2829 | ` *  Make a string uppercase.` |
|      - | 2830 | ` * Parameters` |
|      - | 2831 | ` *  $str` |
|      - | 2832 | ` *   The input string.` |
|      - | 2833 | ` * Returns.` |
|      - | 2834 | ` *  The uppercased string.` |
|      - | 2835 | ` */` |
|     10 | 2836 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2837 |  |
|      - | 2838 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2839 | `	int nLen;` |
|     11 | 2840 | `	if( nArg < 1 ){` |
|      - | 2841 | `		/* Missing arguments,return null */` |
|      3 | 2842 | `		ph7_result_null(pCtx);` |
|      3 | 2843 | `		return PH7_OK;` |
|      - | 2844 | `	}` |
|      - | 2845 | `	/* Extract the target string */` |
|      9 | 2846 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 2847 | `	if( nLen < 1 ){` |
|      - | 2848 | `		/* Empty string,return */` |
|      3 | 2849 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2850 | `		return PH7_OK;` |
|      - | 2851 | `	}` |
|      - | 2852 | `	/* Perform the requested operation */` |
|      7 | 2853 | `	zEnd = &zString[nLen];` |
|     19 | 2854 | `	for(;;){` |
|     39 | 2855 | `		if( zString >= zEnd ){` |
|      - | 2856 | `			/* No more input,break immediately */` |
|      7 | 2857 | `			break;` |
|      - | 2858 | `		}` |
|     33 | 2859 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2860 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2861 | `			zCur = zString;` |
|    ! 0 | 2862 | `			zString++;` |
|    ! 0 | 2863 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2864 | `				zString++;` |
|    ! 0 | 2865 | `			}` |
|      - | 2866 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2867 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2868 | `		}else{` |
|     33 | 2869 | `			int c = zString[0];` |
|     33 | 2870 | `			if( SyisLower(c) ){` |
|     27 | 2871 | `				c = SyToUpper(zString[0]);` |
|     13 | 2872 | `			}` |
|      - | 2873 | `			/* Append character */` |
|     33 | 2874 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2875 | `			/* Advance the cursor */` |
|     33 | 2876 | `			zString++;` |
|      - | 2877 | `		}` |
|      1 | 2878 | `	}` |
|      7 | 2879 | `	return PH7_OK;` |
|      6 | 2880 |  |
|      - | 2881 | `/*` |
|      - | 2882 | ` * string ucfirst(string $str)` |
|      - | 2883 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2884 | ` *  character is alphabetic.` |
|      - | 2885 | ` * Parameters` |
|      - | 2886 | ` *  $str` |
|      - | 2887 | ` *   The input string.` |
|      - | 2888 | ` * Returns.` |
|      - | 2889 | ` *  The processed string.` |
|      - | 2890 | ` */` |
|      6 | 2891 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2892 |  |
|      - | 2893 | `	const char *zString,*zEnd;` |
|      - | 2894 | `	int nLen,c;` |
|      7 | 2895 | `	if( nArg < 1 ){` |
|      - | 2896 | `		/* Missing arguments,return null */` |
|      3 | 2897 | `		ph7_result_null(pCtx);` |
|      3 | 2898 | `		return PH7_OK;` |
|      - | 2899 | `	}` |
|      - | 2900 | `	/* Extract the target string */` |
|      5 | 2901 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2902 | `	if( nLen < 1 ){` |
|      - | 2903 | `		/* Empty string,return */` |
|      3 | 2904 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2905 | `		return PH7_OK;` |
|      - | 2906 | `	}` |
|      - | 2907 | `	/* Perform the requested operation */` |
|      3 | 2908 | `	zEnd = &zString[nLen];` |
|      3 | 2909 | `	c = zString[0];` |
|      3 | 2910 | `	if( SyisLower(c) ){` |
|      3 | 2911 | `		c = SyToUpper(c);` |
|      1 | 2912 | `	}` |
|      - | 2913 | `	/* Append the first character */` |
|      3 | 2914 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2915 | `	zString++;` |
|      3 | 2916 | `	if( zString < zEnd ){` |
|      - | 2917 | `		/* Append the rest of the input verbatim */` |
|      3 | 2918 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2919 | `	}` |
|      3 | 2920 | `	return PH7_OK;` |
|      4 | 2921 |  |
|      - | 2922 | `/*` |
|      - | 2923 | ` * string lcfirst(string $str)` |
|      - | 2924 | ` *  Make a string's first character lowercase.` |
|      - | 2925 | ` * Parameters` |
|      - | 2926 | ` *  $str` |
|      - | 2927 | ` *   The input string.` |
|      - | 2928 | ` * Returns.` |
|      - | 2929 | ` *  The processed string.` |
|      - | 2930 | ` */` |
|      6 | 2931 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2932 |  |
|      - | 2933 | `	const char *zString,*zEnd;` |
|      - | 2934 | `	int nLen,c;` |
|      7 | 2935 | `	if( nArg < 1 ){` |
|      - | 2936 | `		/* Missing arguments,return null */` |
|      3 | 2937 | `		ph7_result_null(pCtx);` |
|      3 | 2938 | `		return PH7_OK;` |
|      - | 2939 | `	}` |
|      - | 2940 | `	/* Extract the target string */` |
|      5 | 2941 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2942 | `	if( nLen < 1 ){` |
|      - | 2943 | `		/* Empty string,return */` |
|      3 | 2944 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2945 | `		return PH7_OK;` |
|      - | 2946 | `	}` |
|      - | 2947 | `	/* Perform the requested operation */` |
|      3 | 2948 | `	zEnd = &zString[nLen];` |
|      3 | 2949 | `	c = zString[0];` |
|      3 | 2950 | `	if( SyisUpper(c) ){` |
|      3 | 2951 | `		c = SyToLower(c);` |
|      1 | 2952 | `	}` |
|      - | 2953 | `	/* Append the first character */` |
|      3 | 2954 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2955 | `	zString++;` |
|      3 | 2956 | `	if( zString < zEnd ){` |
|      - | 2957 | `		/* Append the rest of the input verbatim */` |
|      3 | 2958 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2959 | `	}` |
|      3 | 2960 | `	return PH7_OK;` |
|      4 | 2961 |  |
|      - | 2962 | `/*` |
|      - | 2963 | ` * int ord(string $string)` |
|      - | 2964 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 2965 | ` * Parameters` |
|      - | 2966 | ` *  $str` |
|      - | 2967 | ` *   The input string.` |
|      - | 2968 | ` * Returns.` |
|      - | 2969 | ` *  The ASCII value as an integer.` |
|      - | 2970 | ` */` |
|     32 | 2971 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2972 |  |
|      - | 2973 | `	const char *zString;` |
|      - | 2974 | `	int nLen,c;` |
|     33 | 2975 | `	if( nArg < 1 ){` |
|      - | 2976 | `		/* Missing arguments,return -1 */` |
|      3 | 2977 | `		ph7_result_int(pCtx,-1);` |
|      3 | 2978 | `		return PH7_OK;` |
|      - | 2979 | `	}` |
|      - | 2980 | `	/* Extract the target string */` |
|     31 | 2981 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 2982 | `	if( nLen < 1 ){` |
|      - | 2983 | `		/* Empty string,return -1 */` |
|      3 | 2984 | `		ph7_result_int(pCtx,-1);` |
|      3 | 2985 | `		return PH7_OK;` |
|      - | 2986 | `	}` |
|      - | 2987 | `	/* Extract the ASCII value of the first character */` |
|     29 | 2988 | `	c = zString[0];` |
|      - | 2989 | `	/* Return that value */` |
|     29 | 2990 | `	ph7_result_int(pCtx,c);` |
|     29 | 2991 | `	return PH7_OK;` |
|     17 | 2992 |  |
|      - | 2993 | `/*` |
|      - | 2994 | ` * string chr(int $ascii)` |
|      - | 2995 | ` *  Returns a one-character string containing the character specified by ascii.` |
|      - | 2996 | ` * Parameters` |
|      - | 2997 | ` *  $ascii` |
|      - | 2998 | ` *   The ascii code.` |
|      - | 2999 | ` * Returns.` |
|      - | 3000 | ` *  The specified character.` |
|      - | 3001 | ` */` |
|     28 | 3002 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3003 |  |
|      - | 3004 | `	int c;` |
|     29 | 3005 | `	if( nArg < 1 ){` |
|      - | 3006 | `		/* Missing arguments,return null */` |
|      3 | 3007 | `		ph7_result_null(pCtx);` |
|      3 | 3008 | `		return PH7_OK;` |
|      - | 3009 | `	}` |
|      - | 3010 | `	/* Extract the ASCII value */` |
|     27 | 3011 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 3012 | `	/* Return the specified character */` |
|     27 | 3013 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     27 | 3014 | `	return PH7_OK;` |
|     15 | 3015 |  |
|      - | 3016 | `/*` |
|      - | 3017 | ` * Binary to hex consumer callback.` |
|      - | 3018 | ` * This callback is the default consumer used by the hash functions` |
|      - | 3019 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 3020 | ` */` |
|    226 | 3021 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 3022 |  |
|      - | 3023 | `	/* Append hex chunk verbatim */` |
|    227 | 3024 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|    227 | 3025 | `	return SXRET_OK;` |
|      1 | 3026 |  |
|      - | 3027 |  |
|      - | 3028 | `/*` |
|      - | 3029 | ` * string bin2hex(string $str)` |
|      - | 3030 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 3031 | ` * Parameters` |
|      - | 3032 | ` *  $str` |
|      - | 3033 | ` *   The input string.` |
|      - | 3034 | ` * Returns.` |
|      - | 3035 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 3036 | ` */` |
|     12 | 3037 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3038 |  |
|      - | 3039 | `	const char *zString;` |
|      - | 3040 | `	int nLen;` |
|     13 | 3041 | `	if( nArg < 1 ){` |
|      - | 3042 | `		/* Missing arguments,return null */` |
|      3 | 3043 | `		ph7_result_null(pCtx);` |
|      3 | 3044 | `		return PH7_OK;` |
|      - | 3045 | `	}` |
|      - | 3046 | `	/* Extract the target string */` |
|     11 | 3047 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 3048 | `	if( nLen < 1 ){` |
|      - | 3049 | `		/* Empty string,return */` |
|      3 | 3050 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3051 | `		return PH7_OK;` |
|      - | 3052 | `	}` |
|      - | 3053 | `	/* Perform the requested operation */` |
|      9 | 3054 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|      9 | 3055 | `	return PH7_OK;` |
|      7 | 3056 |  |
|      - | 3057 |  |
|      - | 3058 | `/* Search callback signature */` |
|      - | 3059 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 3060 | `/*` |
|      - | 3061 | ` * Case-insensitive pattern match.` |
|      - | 3062 | ` * Brute force is the default search method used here.` |
|      - | 3063 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 3064 | ` * well for short/medium texts on modern hardware.` |
|      - | 3065 | ` */` |
|    118 | 3066 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      1 | 3067 |  |
|    119 | 3068 | `	const char *zpIn = (const char *)pPattern;` |
|    119 | 3069 | `	const char *zIn = (const char *)pText;` |
|    119 | 3070 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    119 | 3071 | `	const char *zEnd = &zIn[nLen];` |
|      - | 3072 | `	const char *zPtr,*zPtr2;` |
|      - | 3073 | `	int c,d;` |
|    119 | 3074 | `	if( iPatLen > nLen ){` |
|      - | 3075 | `		/* Don't bother processing */` |
|     33 | 3076 | `		return SXERR_NOTFOUND;` |
|      - | 3077 | `	}` |
|    244 | 3078 | `	for(;;){` |
|    489 | 3079 | `		if( zIn >= zEnd ){` |
|     47 | 3080 | `			break;` |
|      - | 3081 | `		}` |
|    443 | 3082 | `		c = SyToLower(zIn[0]);` |
|    443 | 3083 | `		d = SyToLower(zpIn[0]);` |
|    443 | 3084 | `		if( c == d ){` |
|     41 | 3085 | `			zPtr   = &zIn[1];` |
|     41 | 3086 | `			zPtr2  = &zpIn[1];` |
|     71 | 3087 | `			for(;;){` |
|    143 | 3088 | `				if( zPtr2 >= zpEnd ){` |
|      - | 3089 | `					/* Pattern found */` |
|     41 | 3090 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 3091 | `					return SXRET_OK;` |
|      - | 3092 | `				}` |
|    103 | 3093 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 3094 | `					break;` |
|      - | 3095 | `				}` |
|    103 | 3096 | `				c = SyToLower(zPtr[0]);` |
|    103 | 3097 | `				d = SyToLower(zPtr2[0]);` |
|    103 | 3098 | `				if( c != d ){` |
|    ! 0 | 3099 | `					break;` |
|      - | 3100 | `				}` |
|    103 | 3101 | `				zPtr++; zPtr2++;` |
|      1 | 3102 | `			}` |
|    ! 0 | 3103 | `		}` |
|    403 | 3104 | `		zIn++;` |
|      1 | 3105 | `	}` |
|      - | 3106 | `	/* Pattern not found */` |
|     47 | 3107 | `	return SXERR_NOTFOUND;` |
|     60 | 3108 |  |
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
|     10 | 3123 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3124 |  |
|     11 | 3125 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3126 | `	const char *zBlob,*zPattern;` |
|      - | 3127 | `	int nLen,nPatLen;` |
|      - | 3128 | `	sxu32 nOfft;` |
|      - | 3129 | `	sxi32 rc;` |
|     11 | 3130 | `	if( nArg < 2 ){` |
|      - | 3131 | `		/* Missing arguments,return FALSE */` |
|      5 | 3132 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3133 | `		return PH7_OK;` |
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
|      6 | 3161 |  |
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
|      6 | 3176 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3177 |  |
|      7 | 3178 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3179 | `	const char *zBlob,*zPattern;` |
|      - | 3180 | `	int nLen,nPatLen;` |
|      - | 3181 | `	sxu32 nOfft;` |
|      - | 3182 | `	sxi32 rc;` |
|      7 | 3183 | `	if( nArg < 2 ){` |
|      - | 3184 | `		/* Missing arguments,return FALSE */` |
|      3 | 3185 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3186 | `		return PH7_OK;` |
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
|      4 | 3214 |  |
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
|     80 | 3230 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3231 |  |
|     82 | 3232 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3233 | `	const char *zBlob,*zPattern;` |
|      - | 3234 | `	int nLen,nPatLen,nStart;` |
|      - | 3235 | `	sxu32 nOfft;` |
|      - | 3236 | `	sxi32 rc;` |
|     82 | 3237 | `	if( nArg < 2 ){` |
|      - | 3238 | `		/* Missing arguments,return FALSE */` |
|      7 | 3239 | `		ph7_result_bool(pCtx,0);` |
|      7 | 3240 | `		return PH7_OK;` |
|      - | 3241 | `	}` |
|      - | 3242 | `	/* Extract the needle and the haystack */` |
|     76 | 3243 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     76 | 3244 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     76 | 3245 | `	nOfft = 0; /* cc warning */` |
|     76 | 3246 | `	nStart = 0;` |
|      - | 3247 | `	/* Peek the starting offset if available */` |
|     76 | 3248 | `	if( nArg > 2 ){` |
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
|     76 | 3261 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3262 | `		/* Perform the lookup */` |
|     74 | 3263 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     74 | 3264 | `		if( rc != SXRET_OK ){` |
|      - | 3265 | `			/* Pattern not found,return FALSE */` |
|      3 | 3266 | `			ph7_result_bool(pCtx,0);` |
|      3 | 3267 | `			return PH7_OK;` |
|      - | 3268 | `		}` |
|      - | 3269 | `		/* Return the pattern position */` |
|     72 | 3270 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|     37 | 3271 | `	}else{` |
|      3 | 3272 | `		ph7_result_bool(pCtx,0);` |
|      - | 3273 | `	}` |
|     74 | 3274 | `	return PH7_OK;` |
|     42 | 3275 |  |
|      - | 3276 | `/*` |
|      - | 3277 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3278 | ` *  Case-insensitive strpos.` |
|      - | 3279 | ` * Parameters` |
|      - | 3280 | ` *  $haystack` |
|      - | 3281 | ` *   The input string.` |
|      - | 3282 | ` * $needle` |
|      - | 3283 | ` *   Search pattern (must be a string).` |
|      - | 3284 | ` * $offset` |
|      - | 3285 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3286 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3287 | ` *   of haystack.` |
|      - | 3288 | ` * Return` |
|      - | 3289 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3290 | ` */` |
|     18 | 3291 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3292 |  |
|     19 | 3293 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3294 | `	const char *zBlob,*zPattern;` |
|      - | 3295 | `	int nLen,nPatLen,nStart;` |
|      - | 3296 | `	sxu32 nOfft;` |
|      - | 3297 | `	sxi32 rc;` |
|     19 | 3298 | `	if( nArg < 2 ){` |
|      - | 3299 | `		/* Missing arguments,return FALSE */` |
|      3 | 3300 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3301 | `		return PH7_OK;` |
|      - | 3302 | `	}` |
|      - | 3303 | `	/* Extract the needle and the haystack */` |
|     17 | 3304 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 3305 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     17 | 3306 | `	nOfft = 0; /* cc warning */` |
|     17 | 3307 | `	nStart = 0;` |
|      - | 3308 | `	/* Peek the starting offset if available */` |
|     17 | 3309 | `	if( nArg > 2 ){` |
|      5 | 3310 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 3311 | `		if( nStart < 0 ){` |
|      3 | 3312 | `			nStart = -nStart;` |
|      1 | 3313 | `		}` |
|      5 | 3314 | `		if( nStart >= nLen ){` |
|      - | 3315 | `			/* Invalid offset */` |
|    ! 0 | 3316 | `			nStart = 0;` |
|    ! 0 | 3317 | `		}else{` |
|      5 | 3318 | `			zBlob += nStart;` |
|      5 | 3319 | `			nLen -= nStart;` |
|      - | 3320 | `		}` |
|      2 | 3321 | `	}` |
|     17 | 3322 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3323 | `		/* Perform the lookup */` |
|     17 | 3324 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     17 | 3325 | `		if( rc != SXRET_OK ){` |
|      - | 3326 | `			/* Pattern not found,return FALSE */` |
|      3 | 3327 | `			ph7_result_bool(pCtx,0);` |
|      3 | 3328 | `			return PH7_OK;` |
|      - | 3329 | `		}` |
|      - | 3330 | `		/* Return the pattern position */` |
|     15 | 3331 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 3332 | `	}else{` |
|    ! 0 | 3333 | `		ph7_result_bool(pCtx,0);` |
|      - | 3334 | `	}` |
|     15 | 3335 | `	return PH7_OK;` |
|     10 | 3336 |  |
|      - | 3337 | `/*` |
|      - | 3338 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3339 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 3340 | ` * Parameters` |
|      - | 3341 | ` *  $haystack` |
|      - | 3342 | ` *   The input string.` |
|      - | 3343 | ` * $needle` |
|      - | 3344 | ` *   Search pattern (must be a string).` |
|      - | 3345 | ` * $offset` |
|      - | 3346 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3347 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3348 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3349 | ` * Return` |
|      - | 3350 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3351 | ` */` |
|     32 | 3352 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3353 |  |
|      - | 3354 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     33 | 3355 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3356 | `	int nLen,nPatLen;` |
|      - | 3357 | `	sxu32 nOfft;` |
|      - | 3358 | `	sxi32 rc;` |
|     33 | 3359 | `	if( nArg < 2 ){` |
|      - | 3360 | `		/* Missing arguments,return FALSE */` |
|      3 | 3361 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3362 | `		return PH7_OK;` |
|      - | 3363 | `	}` |
|      - | 3364 | `	/* Extract the needle and the haystack */` |
|     31 | 3365 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 3366 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3367 | `	/* Point to the end of the pattern */` |
|     31 | 3368 | `	zPtr = &zBlob[nLen - 1];` |
|     31 | 3369 | `	zEnd = &zBlob[nLen];` |
|      - | 3370 | `	/* Save the starting posistion */` |
|     31 | 3371 | `	zStart = zBlob;` |
|     31 | 3372 | `	nOfft = 0; /* cc warning */` |
|      - | 3373 | `	/* Peek the starting offset if available */` |
|     31 | 3374 | `	if( nArg > 2 ){` |
|      - | 3375 | `		int nStart;` |
|     21 | 3376 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 3377 | `		if( nStart < 0 ){` |
|     11 | 3378 | `			nStart = -nStart;` |
|     11 | 3379 | `			if( nStart >= nLen ){` |
|      - | 3380 | `				/* Invalid offset */` |
|      3 | 3381 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3382 | `				return PH7_OK;` |
|    ! 0 | 3383 | `			}else{` |
|      9 | 3384 | `				nLen -= nStart;` |
|      9 | 3385 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 3386 | `				zEnd = &zBlob[nLen];` |
|      - | 3387 | `			}` |
|      5 | 3388 | `		}else{` |
|     11 | 3389 | `			if( nStart >= nLen ){` |
|      - | 3390 | `				/* Invalid offset */` |
|      5 | 3391 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3392 | `				return PH7_OK;` |
|    ! 0 | 3393 | `			}else{` |
|      7 | 3394 | `				zBlob += nStart;` |
|      7 | 3395 | `				nLen -= nStart;` |
|      - | 3396 | `			}` |
|      - | 3397 | `		}` |
|      7 | 3398 | `	}` |
|     25 | 3399 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3400 | `		/* Perform the lookup */` |
|     57 | 3401 | `		for(;;){` |
|    115 | 3402 | `			if( zBlob >= zPtr ){` |
|     11 | 3403 | `				break;` |
|      - | 3404 | `			}` |
|    105 | 3405 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    105 | 3406 | `			if( rc == SXRET_OK ){` |
|      - | 3407 | `				/* Pattern found,return it's position */` |
|     13 | 3408 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 3409 | `				return PH7_OK;` |
|      - | 3410 | `			}` |
|     93 | 3411 | `			zPtr--;` |
|      1 | 3412 | `		}` |
|      - | 3413 | `		/* Pattern not found,return FALSE */` |
|     11 | 3414 | `		ph7_result_bool(pCtx,0);` |
|      6 | 3415 | `	}else{` |
|      3 | 3416 | `		ph7_result_bool(pCtx,0);` |
|      - | 3417 | `	}` |
|     13 | 3418 | `	return PH7_OK;` |
|     17 | 3419 |  |
|      - | 3420 | `/*` |
|      - | 3421 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3422 | ` *  Case-insensitive strrpos.` |
|      - | 3423 | ` * Parameters` |
|      - | 3424 | ` *  $haystack` |
|      - | 3425 | ` *   The input string.` |
|      - | 3426 | ` * $needle` |
|      - | 3427 | ` *   Search pattern (must be a string).` |
|      - | 3428 | ` * $offset` |
|      - | 3429 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3430 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3431 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3432 | ` * Return` |
|      - | 3433 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3434 | ` */` |
|     28 | 3435 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3436 |  |
|      - | 3437 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     29 | 3438 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3439 | `	int nLen,nPatLen;` |
|      - | 3440 | `	sxu32 nOfft;` |
|      - | 3441 | `	sxi32 rc;` |
|     29 | 3442 | `	if( nArg < 2 ){` |
|      - | 3443 | `		/* Missing arguments,return FALSE */` |
|      3 | 3444 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3445 | `		return PH7_OK;` |
|      - | 3446 | `	}` |
|      - | 3447 | `	/* Extract the needle and the haystack */` |
|     27 | 3448 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 3449 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3450 | `	/* Point to the end of the pattern */` |
|     27 | 3451 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 3452 | `	zEnd = &zBlob[nLen];` |
|      - | 3453 | `	/* Save the starting posistion */` |
|     27 | 3454 | `	zStart = zBlob;` |
|     27 | 3455 | `	nOfft = 0; /* cc warning */` |
|      - | 3456 | `	/* Peek the starting offset if available */` |
|     27 | 3457 | `	if( nArg > 2 ){` |
|      - | 3458 | `		int nStart;` |
|     15 | 3459 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3460 | `		if( nStart < 0 ){` |
|      7 | 3461 | `			nStart = -nStart;` |
|      7 | 3462 | `			if( nStart >= nLen ){` |
|      - | 3463 | `				/* Invalid offset */` |
|      3 | 3464 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3465 | `				return PH7_OK;` |
|    ! 0 | 3466 | `			}else{` |
|      5 | 3467 | `				nLen -= nStart;` |
|      5 | 3468 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 3469 | `				zEnd = &zBlob[nLen];` |
|      - | 3470 | `			}` |
|      3 | 3471 | `		}else{` |
|      9 | 3472 | `			if( nStart >= nLen ){` |
|      - | 3473 | `				/* Invalid offset */` |
|      5 | 3474 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3475 | `				return PH7_OK;` |
|    ! 0 | 3476 | `			}else{` |
|      5 | 3477 | `				zBlob += nStart;` |
|      5 | 3478 | `				nLen -= nStart;` |
|      - | 3479 | `			}` |
|      - | 3480 | `		}` |
|      4 | 3481 | `	}` |
|     21 | 3482 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3483 | `		/* Perform the lookup */` |
|     44 | 3484 | `		for(;;){` |
|     89 | 3485 | `			if( zBlob >= zPtr ){` |
|      9 | 3486 | `				break;` |
|      - | 3487 | `			}` |
|     81 | 3488 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 3489 | `			if( rc == SXRET_OK ){` |
|      - | 3490 | `				/* Pattern found,return it's position */` |
|     11 | 3491 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 3492 | `				return PH7_OK;` |
|      - | 3493 | `			}` |
|     71 | 3494 | `			zPtr--;` |
|      1 | 3495 | `		}` |
|      - | 3496 | `		/* Pattern not found,return FALSE */` |
|      9 | 3497 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3498 | `	}else{` |
|      3 | 3499 | `		ph7_result_bool(pCtx,0);` |
|      - | 3500 | `	}` |
|     11 | 3501 | `	return PH7_OK;` |
|     15 | 3502 |  |
|      - | 3503 | `/*` |
|      - | 3504 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3505 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3506 | ` * Parameters` |
|      - | 3507 | ` *  $haystack` |
|      - | 3508 | ` *   The input string.` |
|      - | 3509 | ` * $needle` |
|      - | 3510 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3511 | ` *  This behavior is different from that of strstr().` |
|      - | 3512 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3513 | ` *  as the ordinal value of a character.` |
|      - | 3514 | ` * Return` |
|      - | 3515 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3516 | ` */` |
|     24 | 3517 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3518 |  |
|      - | 3519 | `	const char *zBlob;` |
|      - | 3520 | `	int nLen,c;` |
|     25 | 3521 | `	if( nArg < 2 ){` |
|      - | 3522 | `		/* Missing arguments,return FALSE */` |
|      3 | 3523 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3524 | `		return PH7_OK;` |
|      - | 3525 | `	}` |
|      - | 3526 | `	/* Extract the haystack */` |
|     23 | 3527 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3528 | `	c = 0; /* cc warning */` |
|     23 | 3529 | `	if( nLen > 0 ){` |
|      - | 3530 | `		sxu32 nOfft;` |
|      - | 3531 | `		sxi32 rc;` |
|     21 | 3532 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 3533 | `			const char *zPattern;` |
|     11 | 3534 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 3535 | `														 * for NULL pointer.` |
|      - | 3536 | `														 */` |
|     11 | 3537 | `			c = zPattern[0];` |
|      6 | 3538 | `		}else{` |
|      - | 3539 | `			/* Int cast */` |
|     11 | 3540 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 3541 | `		}` |
|      - | 3542 | `		/* Perform the lookup */` |
|     21 | 3543 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3544 | `		if( rc != SXRET_OK ){` |
|      - | 3545 | `			/* No such entry,return FALSE */` |
|      7 | 3546 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3547 | `			return PH7_OK;` |
|      - | 3548 | `		}` |
|      - | 3549 | `		/* Return the string portion */` |
|     15 | 3550 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 3551 | `	}else{` |
|      3 | 3552 | `		ph7_result_bool(pCtx,0);` |
|      - | 3553 | `	}` |
|     17 | 3554 | `	return PH7_OK;` |
|     13 | 3555 |  |
|      - | 3556 | `/*` |
|      - | 3557 | ` * string strrev(string $string)` |
|      - | 3558 | ` *  Reverse a string.` |
|      - | 3559 | ` * Parameters` |
|      - | 3560 | ` *  $string` |
|      - | 3561 | ` *   String to be reversed.` |
|      - | 3562 | ` * Return` |
|      - | 3563 | ` *  The reversed string.` |
|      - | 3564 | ` */` |
|      4 | 3565 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3566 |  |
|      - | 3567 | `	const char *zIn,*zEnd;` |
|      - | 3568 | `	int nLen,c;` |
|      5 | 3569 | `	if( nArg < 1 ){` |
|      - | 3570 | `		/* Missing arguments,return NULL */` |
|      3 | 3571 | `		ph7_result_null(pCtx);` |
|      3 | 3572 | `		return PH7_OK;` |
|      - | 3573 | `	}` |
|      - | 3574 | `	/* Extract the target string */` |
|      3 | 3575 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3576 | `	if( nLen < 1 ){` |
|      - | 3577 | `		/* Empty string Return null */` |
|    ! 0 | 3578 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3579 | `		return PH7_OK;` |
|      - | 3580 | `	}` |
|      - | 3581 | `	/* Perform the requested operation */` |
|      3 | 3582 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3583 | `	for(;;){` |
|      9 | 3584 | `		if( zEnd < zIn ){` |
|      - | 3585 | `			/* No more input to process */` |
|      3 | 3586 | `			break;` |
|      - | 3587 | `		}` |
|      - | 3588 | `		/* Append current character */` |
|      7 | 3589 | `		c = zEnd[0];` |
|      7 | 3590 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3591 | `		zEnd--;` |
|      1 | 3592 | `	}` |
|      3 | 3593 | `	return PH7_OK;` |
|      3 | 3594 |  |
|      - | 3595 | `/*` |
|      - | 3596 | ` * string ucwords(string $string)` |
|      - | 3597 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3598 | ` *  The definition of a word is any string of characters that is immediately after` |
|      - | 3599 | ` *  a whitespace (These are: space, form-feed, newline, carriage return, horizontal tab, and vertical tab).` |
|      - | 3600 | ` * Parameters` |
|      - | 3601 | ` *  $string` |
|      - | 3602 | ` *   The input string.` |
|      - | 3603 | ` * Return` |
|      - | 3604 | ` *  The modified string..` |
|      - | 3605 | ` */` |
|     14 | 3606 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3607 |  |
|      - | 3608 | `	const char *zIn,*zCur,*zEnd;` |
|      - | 3609 | `	int nLen,c;` |
|     15 | 3610 | `	if( nArg < 1 ){` |
|      - | 3611 | `		/* Missing arguments,return NULL */` |
|      3 | 3612 | `		ph7_result_null(pCtx);` |
|      3 | 3613 | `		return PH7_OK;` |
|      - | 3614 | `	}` |
|      - | 3615 | `	/* Extract the target string */` |
|     13 | 3616 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 3617 | `	if( nLen < 1 ){` |
|      - | 3618 | `		/* Empty string Return null */` |
|      3 | 3619 | `		ph7_result_null(pCtx);` |
|      3 | 3620 | `		return PH7_OK;` |
|      - | 3621 | `	}` |
|      - | 3622 | `	/* Perform the requested operation */` |
|     11 | 3623 | `	zEnd = &zIn[nLen];` |
|     21 | 3624 | `	for(;;){` |
|      - | 3625 | `		/* Jump leading white spaces */` |
|     43 | 3626 | `		zCur = zIn;` |
|     65 | 3627 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0x80 && SyisSpace(zIn[0]) ){` |
|     23 | 3628 | `			zIn++;` |
|      1 | 3629 | `		}` |
|     43 | 3630 | `		if( zCur < zIn ){` |
|      - | 3631 | `			/* Append white space stream */` |
|     23 | 3632 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 3633 | `		}` |
|     43 | 3634 | `		if( zIn >= zEnd ){` |
|      - | 3635 | `			/* No more input to process */` |
|     11 | 3636 | `			break;` |
|      - | 3637 | `		}` |
|     33 | 3638 | `		c = zIn[0];` |
|     33 | 3639 | `		if( c < 0x80 && SyisLower(c) ){` |
|     29 | 3640 | `			c = SyToUpper(c);` |
|     14 | 3641 | `		}` |
|      - | 3642 | `		/* Append the upper-cased character */` |
|     33 | 3643 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     33 | 3644 | `		zIn++;` |
|     33 | 3645 | `		zCur = zIn;` |
|      - | 3646 | `		/* Append the word varbatim */` |
|    149 | 3647 | `		while( zIn < zEnd ){` |
|    139 | 3648 | `			if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 3649 | `				/* UTF-8 stream */` |
|    ! 0 | 3650 | `				zIn++;` |
|    ! 0 | 3651 | `				SX_JMP_UTF8(zIn,zEnd);` |
|    138 | 3652 | `			}else if( !SyisSpace(zIn[0]) ){` |
|    117 | 3653 | `				zIn++;` |
|     59 | 3654 | `			}else{` |
|     23 | 3655 | `				break;` |
|      - | 3656 | `			}` |
|      1 | 3657 | `		}` |
|     33 | 3658 | `		if( zCur < zIn ){` |
|     33 | 3659 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 | 3660 | `		}` |
|      1 | 3661 | `	}` |
|     11 | 3662 | `	return PH7_OK;` |
|      8 | 3663 |  |
|      - | 3664 | `/*` |
|      - | 3665 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3666 | ` *  Returns input repeated multiplier times.` |
|      - | 3667 | ` * Parameters` |
|      - | 3668 | ` *  $string` |
|      - | 3669 | ` *   String to be repeated.` |
|      - | 3670 | ` * $multiplier` |
|      - | 3671 | ` *  Number of time the input string should be repeated.` |
|      - | 3672 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3673 | ` *  to 0, the function will return an empty string.` |
|      - | 3674 | ` * Return` |
|      - | 3675 | ` *  The repeated string.` |
|      - | 3676 | ` */` |
|  20212 | 3677 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3678 |  |
|      - | 3679 | `	const char *zIn;` |
|      - | 3680 | `	int nLen,nMul;` |
|      - | 3681 | `	int rc;` |
|  20213 | 3682 | `	if( nArg < 2 ){` |
|      - | 3683 | `		/* Missing arguments,return NULL */` |
|      3 | 3684 | `		ph7_result_null(pCtx);` |
|      3 | 3685 | `		return PH7_OK;` |
|      - | 3686 | `	}` |
|      - | 3687 | `	/* Extract the target string */` |
|  20211 | 3688 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|  20211 | 3689 | `	if( nLen < 1 ){` |
|      - | 3690 | `		/* Empty string.Return null */` |
|    ! 0 | 3691 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3692 | `		return PH7_OK;` |
|      - | 3693 | `	}` |
|      - | 3694 | `	/* Extract the multiplier */` |
|  20211 | 3695 | `	nMul = ph7_value_to_int(apArg[1]);` |
|  20211 | 3696 | `	if( nMul < 1 ){` |
|      - | 3697 | `		/* Return the empty string */` |
|      3 | 3698 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3699 | `		return PH7_OK;` |
|      - | 3700 | `	}` |
|      - | 3701 | `	/* Perform the requested operation */` |
| 120220 | 3702 | `	for(;;){` |
| 240441 | 3703 | `		if( !nMul ){` |
|  20209 | 3704 | `			break;` |
|      - | 3705 | `		}` |
|      - | 3706 | `		/* Append the copy */` |
| 220233 | 3707 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 220233 | 3708 | `		if( rc != PH7_OK ){` |
|      - | 3709 | `			/* Out of memory,break immediately */` |
|    ! 0 | 3710 | `			break;` |
|      - | 3711 | `		}` |
| 220233 | 3712 | `		nMul--;` |
|      1 | 3713 | `	}` |
|  20209 | 3714 | `	return PH7_OK;` |
|  10107 | 3715 |  |
|      - | 3716 | `/*` |
|      - | 3717 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3718 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3719 | ` * Parameters` |
|      - | 3720 | ` *  $string` |
|      - | 3721 | ` *   The input string.` |
|      - | 3722 | ` * $is_xhtml` |
|      - | 3723 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3724 | ` * Return` |
|      - | 3725 | ` *  The processed string.` |
|      - | 3726 | ` */` |
|      6 | 3727 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3728 |  |
|      - | 3729 | `	const char *zIn,*zCur,*zEnd;` |
|      7 | 3730 | `	int is_xhtml = 1; /* Default to XHTML-style '<br/>' like PHP */` |
|      - | 3731 | `	int nLen;` |
|      7 | 3732 | `	if( nArg < 1 ){` |
|      - | 3733 | `		/* Missing arguments,return the empty string */` |
|      3 | 3734 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3735 | `		return PH7_OK;` |
|      - | 3736 | `	}` |
|      - | 3737 | `	/* Extract the target string */` |
|      5 | 3738 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3739 | `	if( nLen < 1 ){` |
|      - | 3740 | `		/* Empty string,return null */` |
|    ! 0 | 3741 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3742 | `		return PH7_OK;` |
|      - | 3743 | `	}` |
|      5 | 3744 | `	if( nArg > 1 ){` |
|      3 | 3745 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3746 | `	}` |
|      5 | 3747 | `	zEnd = &zIn[nLen];` |
|      - | 3748 | `	/* Perform the requested operation */` |
|      4 | 3749 | `	for(;;){` |
|      9 | 3750 | `		zCur = zIn;` |
|      - | 3751 | `		/* Delimit the string */` |
|     21 | 3752 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3753 | `			zIn++;` |
|      1 | 3754 | `		}` |
|      9 | 3755 | `		if( zCur < zIn ){` |
|      - | 3756 | `			/* Output chunk verbatim */` |
|      9 | 3757 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3758 | `		}` |
|      9 | 3759 | `		if( zIn >= zEnd ){` |
|      - | 3760 | `			/* No more input to process */` |
|      5 | 3761 | `			break;` |
|      - | 3762 | `		}` |
|      - | 3763 | `		/* Output the HTML line break */` |
|      - | 3764 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br/>' (legacy without space), otherwise use '<br>' */` |
|      5 | 3765 | `		if( is_xhtml ){` |
|      3 | 3766 | `			ph7_result_string(pCtx,"<br/>",(int)sizeof("<br/>")-1);` |
|      2 | 3767 | `		}else{` |
|      3 | 3768 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3769 | `		}` |
|      5 | 3770 | `		zCur = zIn;` |
|      - | 3771 | `		/* Append trailing line */` |
|     11 | 3772 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3773 | `			zIn++;` |
|      1 | 3774 | `		}` |
|      5 | 3775 | `		if( zCur < zIn ){` |
|      - | 3776 | `			/* Output chunk verbatim */` |
|      5 | 3777 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 3778 | `		}` |
|      1 | 3779 | `	}` |
|      5 | 3780 | `	return PH7_OK;` |
|      4 | 3781 |  |
|      - | 3782 | `/*` |
|      - | 3783 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3784 | ` *  According to the PHP reference manual.` |
|      - | 3785 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3786 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3787 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 3788 | ` * This applies to both sprintf() and printf().` |
|      - | 3789 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 3790 | ` * or more of these elements, in order:` |
|      - | 3791 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 3792 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 3793 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 3794 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 3795 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 3796 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 3797 | ` *   it with a single quote ('). See the examples below.` |
|      - | 3798 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 3799 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 3800 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 3801 | ` *   should result in.` |
|      - | 3802 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 3803 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 3804 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 3805 | ` *   limit to the string.` |
|      - | 3806 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 3807 | ` *       % - a literal percent character. No argument is required.` |
|      - | 3808 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 3809 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 3810 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 3811 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 3812 | ` * 	     for the number of digits after the decimal point.` |
|      - | 3813 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 3814 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 3815 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 3816 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 3817 | ` *       g - shorter of %e and %f.` |
|      - | 3818 | ` *       G - shorter of %E and %f.` |
|      - | 3819 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 3820 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 3821 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 3822 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 3823 | ` */` |
|      - | 3824 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3825 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 3826 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 3827 | `/*` |
|      - | 3828 | `** Conversion types fall into various categories as defined by the` |
|      - | 3829 | `** following enumeration.` |
|      - | 3830 | `*/` |
|      - | 3831 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 3832 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 3833 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 3834 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 3835 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 3836 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 3837 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 3838 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 3839 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 3840 |  |
|      - | 3841 | `/*` |
|      - | 3842 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 3843 | `*/` |
|      - | 3844 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 3845 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 3846 | `/*` |
|      - | 3847 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 3848 | `** by an instance of the following structure` |
|      - | 3849 | `*/` |
|      - | 3850 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 3851 | `struct ph7_fmt_info` |
|      - | 3852 |  |
|      - | 3853 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 3854 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 3855 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 3856 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 3857 | `  char *charset; /* The character set for conversion */` |
|      - | 3858 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 3859 | `};` |
|      - | 3860 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3861 | `/*` |
|      - | 3862 | `** "*val" is a double such that 0.1 <= *val < 10.0` |
|      - | 3863 | `** Return the ascii code for the leading digit of *val, then` |
|      - | 3864 | `** multiply "*val" by 10.0 to renormalize.` |
|      - | 3865 | `**` |
|      - | 3866 | `** Example:` |
|      - | 3867 | `**     input:     *val = 3.14159` |
|      - | 3868 | `**     output:    *val = 1.4159    function return = '3'` |
|      - | 3869 | `**` |
|      - | 3870 | `** The counter *cnt is incremented each time.  After counter exceeds` |
|      - | 3871 | `** 16 (the number of significant digits in a 64-bit float) '0' is` |
|      - | 3872 | `** always returned.` |
|      - | 3873 | `*/` |
|    404 | 3874 | `static int vxGetdigit(sxlongreal *val,int *cnt)` |
|      1 | 3875 |  |
|      - | 3876 | `  sxlongreal d;` |
|      - | 3877 | `  int digit;` |
|      - | 3878 |  |
|    405 | 3879 | `  if( (*cnt)++ >= 16 ){` |
|    ! 0 | 3880 | `	  return '0';` |
|      - | 3881 | `  }` |
|    405 | 3882 | `  digit = (int)*val;` |
|    405 | 3883 | `  d = digit;` |
|    405 | 3884 | `   *val = (*val - d)*10.0;` |
|    405 | 3885 | `  return digit + '0' ;` |
|    203 | 3886 |  |
|      - | 3887 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      - | 3888 | `/*` |
|      - | 3889 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 3890 | ` * used conversion types first.` |
|      - | 3891 | ` */` |
|      - | 3892 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 3893 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 3894 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 3895 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 3896 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 3897 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 3898 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 3899 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 3900 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 3901 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3902 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3903 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 3904 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 3905 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 3906 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 3907 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 3908 | `};` |
|      - | 3909 | `/*` |
|      - | 3910 | ` * Format a given string.` |
|      - | 3911 | ` * The root program.  All variations call this core.` |
|      - | 3912 | ` * INPUTS:` |
|      - | 3913 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 3914 | ` *            1. A pointer to the call context.` |
|      - | 3915 | ` *            2. A pointer to the list of characters to be output` |
|      - | 3916 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 3917 | ` *            3. An integer number of characters to be output.` |
|      - | 3918 | ` *               (Note: This number might be zero.)` |
|      - | 3919 | ` *            4. Upper layer private data.` |
|      - | 3920 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 3921 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 3922 | ` */` |
|    120 | 3923 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 3924 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 3925 | `	ph7_context *pCtx,  /* call context */` |
|      - | 3926 | `	const char *zIn,    /* Format string */` |
|      - | 3927 | `	int nByte,          /* Format string length */` |
|      - | 3928 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 3929 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 3930 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 3931 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 3932 | `	)` |
|      1 | 3933 |  |
|    121 | 3934 | `	char spaces[] = "                                                  ";` |
|      - | 3935 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    121 | 3936 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 3937 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 3938 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 3939 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 3940 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 3941 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 3942 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 3943 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 3944 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 3945 | `	ph7_int64 iVal;` |
|      - | 3946 | `	int precision;           /* Precision of the current field */` |
|      - | 3947 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 3948 | `	int c,rc,n;` |
|      - | 3949 | `	int length;              /* Length of the field */` |
|      - | 3950 | `	int prefix;` |
|      - | 3951 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 3952 | `	int width;               /* Width of the current field */` |
|      - | 3953 | `	int idx;` |
|    121 | 3954 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 3955 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 3956 | `	/* Start the format process */` |
|    123 | 3957 | `	for(;;){` |
|    247 | 3958 | `		zCur = zIn;` |
|    697 | 3959 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|    451 | 3960 | `			zIn++;` |
|      1 | 3961 | `		}` |
|    247 | 3962 | `		if( zCur < zIn ){` |
|      - | 3963 | `			/* Consume chunk verbatim */` |
|     95 | 3964 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|     95 | 3965 | `			if( rc == SXERR_ABORT ){` |
|      - | 3966 | `				/* Callback request an operation abort */` |
|    ! 0 | 3967 | `				break;` |
|      - | 3968 | `			}` |
|     47 | 3969 | `		}` |
|    247 | 3970 | `		if( zIn >= zEnd ){` |
|      - | 3971 | `			/* No more input to process,break immediately */` |
|    119 | 3972 | `			break;` |
|      - | 3973 | `		}` |
|      - | 3974 | `		/* Find out what flags are present */` |
|    129 | 3975 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    128 | 3976 | `			flag_alternateform = flag_zeropad = 0;` |
|    129 | 3977 | `		zIn++; /* Jump the precent sign */` |
|     64 | 3978 | `		do{` |
|    157 | 3979 | `			c = zIn[0];` |
|    157 | 3980 | `			switch( c ){` |
|      9 | 3981 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      3 | 3982 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 3983 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|      9 | 3984 | `			case '#':   flag_alternateform = 1;   c = 0;   break;` |
|      5 | 3985 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 3986 | `			case '\'':` |
|    ! 0 | 3987 | `				zIn++;` |
|    ! 0 | 3988 | `				if( zIn < zEnd ){` |
|      - | 3989 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 3990 | `					c = zIn[0];` |
|    ! 0 | 3991 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3992 | `						spaces[idx] = (char)c;` |
|    ! 0 | 3993 | `					}` |
|    ! 0 | 3994 | `					c = 0;` |
|    ! 0 | 3995 | `				}` |
|    ! 0 | 3996 | `				break;` |
|    128 | 3997 | `			default:                                       break;` |
|      - | 3998 | `			}` |
|    157 | 3999 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 4000 | `		/* Get the field width */` |
|    129 | 4001 | `		width = 0;` |
|    223 | 4002 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     31 | 4003 | `			width = width*10 + (zIn[0] - '0');` |
|     31 | 4004 | `			zIn++;` |
|      1 | 4005 | `		}` |
|    129 | 4006 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 4007 | `			/* Position specifer */` |
|    ! 0 | 4008 | `			if( width > 0 ){` |
|    ! 0 | 4009 | `				n = width;` |
|    ! 0 | 4010 | `				if( vf && n > 0 ){` |
|    ! 0 | 4011 | `					n--;` |
|    ! 0 | 4012 | `				}` |
|    ! 0 | 4013 | `			}` |
|    ! 0 | 4014 | `			zIn++;` |
|    ! 0 | 4015 | `			width = 0;` |
|    ! 0 | 4016 | `			if( zIn < zEnd && zIn[0] == '0' ){` |
|    ! 0 | 4017 | `				flag_zeropad = 1;` |
|    ! 0 | 4018 | `				zIn++;` |
|    ! 0 | 4019 | `			}` |
|    ! 0 | 4020 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 4021 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 4022 | `				zIn++;` |
|    ! 0 | 4023 | `			}` |
|    ! 0 | 4024 | `		}` |
|    129 | 4025 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 4026 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 4027 | `		}` |
|      - | 4028 | `		/* Get the precision */` |
|    129 | 4029 | `		precision = -1;` |
|    129 | 4030 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     57 | 4031 | `			precision = 0;` |
|     57 | 4032 | `			zIn++;` |
|    145 | 4033 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     61 | 4034 | `				precision = precision*10 + (zIn[0] - '0');` |
|     61 | 4035 | `				zIn++;` |
|      1 | 4036 | `			}` |
|     28 | 4037 | `		}` |
|    129 | 4038 | `		if( zIn >= zEnd ){` |
|      - | 4039 | `			/* No more input */` |
|      3 | 4040 | `			break;` |
|      - | 4041 | `		}` |
|      - | 4042 | `		/* Fetch the info entry for the field */` |
|    127 | 4043 | `		pInfo = 0;` |
|    127 | 4044 | `		xtype = PH7_FMT_ERROR;` |
|    127 | 4045 | `		c = zIn[0];` |
|    127 | 4046 | `		zIn++; /* Jump the format specifer */` |
|    699 | 4047 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|    697 | 4048 | `			if( c==aFmt[idx].fmttype ){` |
|    125 | 4049 | `				pInfo = &aFmt[idx];` |
|    125 | 4050 | `				xtype = pInfo->type;` |
|    125 | 4051 | `				break;` |
|      - | 4052 | `			}` |
|    287 | 4053 | `		}` |
|    127 | 4054 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    127 | 4055 | `		length = 0;` |
|      - | 4056 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 4057 | `		 /*` |
|      - | 4058 | `		  ** At this point, variables are initialized as follows:` |
|      - | 4059 | `		  **` |
|      - | 4060 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 4061 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 4062 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 4063 | `		  **                               field width was negative.` |
|      - | 4064 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 4065 | `		  **                               the conversion character.` |
|      - | 4066 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 4067 | `		  **   width                       The specified field width.  This is` |
|      - | 4068 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 4069 | `		  **   precision                   The specified precision.  The default` |
|      - | 4070 | `		  **                               is -1.` |
|      - | 4071 | `		  */` |
|    127 | 4072 | `		switch(xtype){` |
|    ! 0 | 4073 | `		case PH7_FMT_PERCENT:` |
|      - | 4074 | `			/* A literal percent character */` |
|    ! 0 | 4075 | `			zWorker[0] = '%';` |
|    ! 0 | 4076 | `			length = (int)sizeof(char);` |
|    ! 0 | 4077 | `			break;` |
|      3 | 4078 | `		case PH7_FMT_CHARX:` |
|      - | 4079 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 4080 | `			 * with that ASCII value` |
|      - | 4081 | `			 */` |
|      7 | 4082 | `			pArg = NEXT_ARG;` |
|      7 | 4083 | `			if( pArg == 0 ){` |
|      3 | 4084 | `				c = 0;` |
|      2 | 4085 | `			}else{` |
|      5 | 4086 | `				c = ph7_value_to_int(pArg);` |
|      - | 4087 | `			}` |
|      - | 4088 | `			/* NUL byte is an acceptable value */` |
|      7 | 4089 | `			zWorker[0] = (char)c;` |
|      7 | 4090 | `			length = (int)sizeof(char);` |
|      7 | 4091 | `			break;` |
|     12 | 4092 | `		case PH7_FMT_STRING:` |
|      - | 4093 | `			/* the argument is treated as and presented as a string */` |
|     25 | 4094 | `			pArg = NEXT_ARG;` |
|     25 | 4095 | `			if( pArg == 0 ){` |
|    ! 0 | 4096 | `				length = 0;` |
|    ! 0 | 4097 | `			}else{` |
|     25 | 4098 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 4099 | `			}` |
|     25 | 4100 | `			if( length < 1 ){` |
|    ! 0 | 4101 | `				zBuf = " ";` |
|    ! 0 | 4102 | `				length = (int)sizeof(char);` |
|    ! 0 | 4103 | `			}` |
|     25 | 4104 | `			if( precision>=0 && precision<length ){` |
|      3 | 4105 | `				length = precision;` |
|      1 | 4106 | `			}` |
|     25 | 4107 | `			if( flag_zeropad ){` |
|      - | 4108 | `				/* zero-padding works on strings too */` |
|    ! 0 | 4109 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4110 | `					spaces[idx] = '0';` |
|    ! 0 | 4111 | `				}` |
|    ! 0 | 4112 | `			}` |
|     25 | 4113 | `			break;` |
|     20 | 4114 | `		case PH7_FMT_RADIX:` |
|     41 | 4115 | `			pArg = NEXT_ARG;` |
|     41 | 4116 | `			if( pArg == 0 ){` |
|    ! 0 | 4117 | `				iVal = 0;` |
|    ! 0 | 4118 | `			}else{` |
|     41 | 4119 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 4120 | `			}` |
|      - | 4121 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|     41 | 4122 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 4123 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 4124 | `			}` |
|      - | 4125 | `#if 1` |
|      - | 4126 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 4127 | `        ** I think this is stupid.*/` |
|     41 | 4128 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 4129 | `#else` |
|      - | 4130 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 4131 | `        ** but leave the prefix for hex.*/` |
|      - | 4132 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 4133 | `#endif` |
|     41 | 4134 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|     23 | 4135 | `          if( iVal<0 ){` |
|      3 | 4136 | `            iVal = -iVal;` |
|      - | 4137 | `			/* Ticket 1433-003 */` |
|      3 | 4138 | `			if( iVal < 0 ){` |
|      - | 4139 | `				/* Overflow */` |
|    ! 0 | 4140 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4141 | `			}` |
|      3 | 4142 | `            prefix = '-';` |
|     22 | 4143 | `          }else if( flag_plussign )  prefix = '+';` |
|     19 | 4144 | `          else if( flag_blanksign )  prefix = ' ';` |
|     17 | 4145 | `          else                       prefix = 0;` |
|     12 | 4146 | `        }else{` |
|     19 | 4147 | `			if( iVal<0 ){` |
|    ! 0 | 4148 | `				iVal = -iVal;` |
|      - | 4149 | `				/* Ticket 1433-003 */` |
|    ! 0 | 4150 | `				if( iVal < 0 ){` |
|      - | 4151 | `					/* Overflow */` |
|    ! 0 | 4152 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4153 | `				}` |
|    ! 0 | 4154 | `			}` |
|     19 | 4155 | `			prefix = 0;` |
|      - | 4156 | `		}` |
|     41 | 4157 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      3 | 4158 | `          precision = width-(prefix!=0);` |
|      1 | 4159 | `        }` |
|     41 | 4160 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 4161 | `        {` |
|      - | 4162 | `          register char *cset;      /* Use registers for speed */` |
|      - | 4163 | `          register int base;` |
|     41 | 4164 | `          cset = pInfo->charset;` |
|     41 | 4165 | `          base = pInfo->base;` |
|     20 | 4166 | `          do{                                           /* Convert to ascii */` |
|     79 | 4167 | `            *(--zBuf) = cset[iVal%base];` |
|     79 | 4168 | `            iVal = iVal/base;` |
|     79 | 4169 | `          }while( iVal>0 );` |
|      - | 4170 | `        }` |
|     41 | 4171 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     55 | 4172 | `        for(idx=precision-length; idx>0; idx--){` |
|     15 | 4173 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|      8 | 4174 | `        }` |
|     41 | 4175 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|     41 | 4176 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 4177 | `          char *pre, x;` |
|      9 | 4178 | `          pre = pInfo->prefix;` |
|      9 | 4179 | `          if( *zBuf!=pre[0] ){` |
|     23 | 4180 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|      4 | 4181 | `          }` |
|      4 | 4182 | `        }` |
|     41 | 4183 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     41 | 4184 | `		break;` |
|     27 | 4185 | `		case PH7_FMT_FLOAT:` |
|      - | 4186 | `		case PH7_FMT_EXP:` |
|      - | 4187 | `		case PH7_FMT_GENERIC:{` |
|      - | 4188 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 4189 | `		long double realvalue;` |
|      - | 4190 | `		int  exp;                /* exponent of real numbers */` |
|      - | 4191 | `		double rounder;          /* Used for rounding floating point values */` |
|      - | 4192 | `		int flag_dp;            /* True if decimal point should be shown */` |
|      - | 4193 | `		int flag_rtz;           /* True if trailing zeros should be removed */` |
|      - | 4194 | `		int flag_exp;           /* True to force display of the exponent */` |
|      - | 4195 | `		int nsd;                 /* Number of significant digits returned */` |
|     55 | 4196 | `		pArg = NEXT_ARG;` |
|     55 | 4197 | `		if( pArg == 0 ){` |
|    ! 0 | 4198 | `			realvalue = 0;` |
|    ! 0 | 4199 | `		}else{` |
|     55 | 4200 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 4201 | `		}` |
|     55 | 4202 | `        if( precision<0 ) precision = 6;         /* Set default precision */` |
|     55 | 4203 | `        if( precision>PH7_FMT_BUFSIZ-40) precision = PH7_FMT_BUFSIZ-40;` |
|     55 | 4204 | `        if( realvalue<0.0 ){` |
|    ! 0 | 4205 | `          realvalue = -realvalue;` |
|    ! 0 | 4206 | `          prefix = '-';` |
|    ! 0 | 4207 | `        }else{` |
|     55 | 4208 | `          if( flag_plussign )          prefix = '+';` |
|     55 | 4209 | `          else if( flag_blanksign )    prefix = ' ';` |
|     55 | 4210 | `          else                         prefix = 0;` |
|      - | 4211 | `        }` |
|     55 | 4212 | `        if( pInfo->type==PH7_FMT_GENERIC && precision>0 ) precision--;` |
|     55 | 4213 | `        rounder = 0.0;` |
|      - | 4214 | `#if 0` |
|      - | 4215 | `        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */` |
|      - | 4216 | `        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);` |
|      - | 4217 | `#else` |
|      - | 4218 | `        /* It makes more sense to use 0.5 */` |
|    387 | 4219 | `        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);` |
|      - | 4220 | `#endif` |
|     55 | 4221 | `        if( pInfo->type==PH7_FMT_FLOAT ) realvalue += rounder;` |
|      - | 4222 | `        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */` |
|     55 | 4223 | `        exp = 0;` |
|     55 | 4224 | `        if( realvalue>0.0 ){` |
|     59 | 4225 | `          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }` |
|     87 | 4226 | `          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }` |
|     59 | 4227 | `          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }` |
|     69 | 4228 | `          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }` |
|     55 | 4229 | `          if( exp>350 \|\| exp<-350 ){` |
|    ! 0 | 4230 | `            zBuf = "NaN";` |
|    ! 0 | 4231 | `            length = 3;` |
|    ! 0 | 4232 | `            break;` |
|      - | 4233 | `          }` |
|     27 | 4234 | `        }` |
|     55 | 4235 | `        zBuf = zWorker;` |
|      - | 4236 | `        /*` |
|      - | 4237 | `        ** If the field type is etGENERIC, then convert to either etEXP` |
|      - | 4238 | `        ** or etFLOAT, as appropriate.` |
|      - | 4239 | `        */` |
|     55 | 4240 | `        flag_exp = xtype==PH7_FMT_EXP;` |
|     55 | 4241 | `        if( xtype!=PH7_FMT_FLOAT ){` |
|    ! 0 | 4242 | `          realvalue += rounder;` |
|    ! 0 | 4243 | `          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }` |
|    ! 0 | 4244 | `        }` |
|     55 | 4245 | `        if( xtype==PH7_FMT_GENERIC ){` |
|    ! 0 | 4246 | `          flag_rtz = !flag_alternateform;` |
|    ! 0 | 4247 | `          if( exp<-4 \|\| exp>precision ){` |
|    ! 0 | 4248 | `            xtype = PH7_FMT_EXP;` |
|    ! 0 | 4249 | `          }else{` |
|    ! 0 | 4250 | `            precision = precision - exp;` |
|    ! 0 | 4251 | `            xtype = PH7_FMT_FLOAT;` |
|      - | 4252 | `          }` |
|    ! 0 | 4253 | `        }else{` |
|     55 | 4254 | `          flag_rtz = 0;` |
|      - | 4255 | `        }` |
|      - | 4256 | `        /*` |
|      - | 4257 | `        ** The "exp+precision" test causes output to be of type etEXP if` |
|      - | 4258 | `        ** the precision is too large to fit in buf[].` |
|      - | 4259 | `        */` |
|     55 | 4260 | `        nsd = 0;` |
|     55 | 4261 | `        if( xtype==PH7_FMT_FLOAT && exp+precision<PH7_FMT_BUFSIZ-30 ){` |
|     55 | 4262 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|     55 | 4263 | `          if( prefix ) *(zBuf++) = (char)prefix;         /* Sign */` |
|     55 | 4264 | `          if( exp<0 )  *(zBuf++) = '0';            /* Digits before "." */` |
|    145 | 4265 | `          else for(; exp>=0; exp--) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     55 | 4266 | `          if( flag_dp ) *(zBuf++) = '.';           /* The decimal point */` |
|     87 | 4267 | `          for(exp++; exp<0 && precision>0; precision--, exp++){` |
|     33 | 4268 | `            *(zBuf++) = '0';` |
|     17 | 4269 | `          }` |
|    355 | 4270 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     55 | 4271 | `          *(zBuf--) = 0;                           /* Null terminate */` |
|     55 | 4272 | `          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */` |
|    ! 0 | 4273 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 4274 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 4275 | `          }` |
|     55 | 4276 | `          zBuf++;                            /* point to next free slot */` |
|     28 | 4277 | `        }else{    /* etEXP or etGENERIC */` |
|    ! 0 | 4278 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|    ! 0 | 4279 | `          if( prefix ) *(zBuf++) = (char)prefix;   /* Sign */` |
|    ! 0 | 4280 | `          *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);  /* First digit */` |
|    ! 0 | 4281 | `          if( flag_dp ) *(zBuf++) = '.';     /* Decimal point */` |
|    ! 0 | 4282 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|    ! 0 | 4283 | `          zBuf--;                            /* point to last digit */` |
|    ! 0 | 4284 | `          if( flag_rtz && flag_dp ){          /* Remove tail zeros */` |
|    ! 0 | 4285 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 4286 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 4287 | `          }` |
|    ! 0 | 4288 | `          zBuf++;                            /* point to next free slot */` |
|    ! 0 | 4289 | `          if( exp \|\| flag_exp ){` |
|    ! 0 | 4290 | `            *(zBuf++) = pInfo->charset[0];` |
|    ! 0 | 4291 | `            if( exp<0 ){ *(zBuf++) = '-'; exp = -exp; } /* sign of exp */` |
|    ! 0 | 4292 | `            else       { *(zBuf++) = '+'; }` |
|    ! 0 | 4293 | `            if( exp>=100 ){` |
|    ! 0 | 4294 | `              *(zBuf++) = (char)((exp/100)+'0');                /* 100's digit */` |
|    ! 0 | 4295 | `              exp %= 100;` |
|    ! 0 | 4296 | `            }` |
|    ! 0 | 4297 | `            *(zBuf++) = (char)(exp/10+'0');                     /* 10's digit */` |
|    ! 0 | 4298 | `            *(zBuf++) = (char)(exp%10+'0');                     /* 1's digit */` |
|    ! 0 | 4299 | `          }` |
|      - | 4300 | `        }` |
|      - | 4301 | `        /* The converted number is in buf[] and zero terminated.Output it.` |
|      - | 4302 | `        ** Note that the number is in the usual order, not reversed as with` |
|      - | 4303 | `        ** integer conversions.*/` |
|     55 | 4304 | `        length = (int)(zBuf-zWorker);` |
|     55 | 4305 | `        zBuf = zWorker;` |
|      - | 4306 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 4307 | `        ** set and we are not left justified */` |
|     55 | 4308 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 4309 | `          int i;` |
|      3 | 4310 | `          int nPad = width - length;` |
|     13 | 4311 | `          for(i=width; i>=nPad; i--){` |
|     11 | 4312 | `            zBuf[i] = zBuf[i-nPad];` |
|      6 | 4313 | `          }` |
|      3 | 4314 | `          i = prefix!=0;` |
|      5 | 4315 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      3 | 4316 | `          length = width;` |
|      1 | 4317 | `        }` |
|      - | 4318 | `#else` |
|      - | 4319 | `         zBuf = " ";` |
|      - | 4320 | `		 length = (int)sizeof(char);` |
|      - | 4321 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|     55 | 4322 | `		 break;` |
|      - | 4323 | `							 }` |
|      1 | 4324 | `		default:` |
|      - | 4325 | `			/* Invalid format specifer */` |
|      3 | 4326 | `			zWorker[0] = '?';` |
|      3 | 4327 | `			length = (int)sizeof(char);` |
|      2 | 4328 | `			break;` |
|      - | 4329 | `		}` |
|      - | 4330 | `		 /*` |
|      - | 4331 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 4332 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 4333 | `		 ** the output.` |
|      - | 4334 | `		 */` |
|    127 | 4335 | `    if( !flag_leftjustify ){` |
|      - | 4336 | `      register int nspace;` |
|    119 | 4337 | `      nspace = width-length;` |
|    119 | 4338 | `      if( nspace>0 ){` |
|      5 | 4339 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4340 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4341 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4342 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4343 | `			}` |
|    ! 0 | 4344 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4345 | `        }` |
|      5 | 4346 | `        if( nspace>0 ){` |
|      5 | 4347 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      5 | 4348 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4349 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4350 | `			}` |
|      2 | 4351 | `		}` |
|      2 | 4352 | `      }` |
|     59 | 4353 | `    }` |
|    127 | 4354 | `    if( length>0 ){` |
|    127 | 4355 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    127 | 4356 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4357 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4358 | `		}` |
|     63 | 4359 | `    }` |
|    127 | 4360 | `    if( flag_leftjustify ){` |
|      - | 4361 | `      register int nspace;` |
|      9 | 4362 | `      nspace = width-length;` |
|      9 | 4363 | `      if( nspace>0 ){` |
|      9 | 4364 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4365 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4366 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4367 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4368 | `			}` |
|    ! 0 | 4369 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4370 | `        }` |
|      9 | 4371 | `        if( nspace>0 ){` |
|      9 | 4372 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      9 | 4373 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4374 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4375 | `			}` |
|      4 | 4376 | `		}` |
|      4 | 4377 | `      }` |
|      4 | 4378 | `    }` |
|      1 | 4379 | ` }/* for(;;) */` |
|    121 | 4380 | `	return SXRET_OK;` |
|     61 | 4381 |  |
|      - | 4382 | `/*` |
|      - | 4383 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 4384 | ` */` |
|     84 | 4385 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4386 |  |
|      - | 4387 | `	/* Consume directly */` |
|     85 | 4388 | `	ph7_result_string(pCtx,zInput,nLen);` |
|     42 | 4389 | `	SXUNUSED(pUserData); /* cc warning */` |
|     85 | 4390 | `	return PH7_OK;` |
|      1 | 4391 |  |
|      - | 4392 | `/*` |
|      - | 4393 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 4394 | ` *  Return a formatted string.` |
|      - | 4395 | ` * Parameters` |
|      - | 4396 | ` *  $format` |
|      - | 4397 | ` *    The format string (see block comment above)` |
|      - | 4398 | ` * Return` |
|      - | 4399 | ` *  A string produced according to the formatting string format.` |
|      - | 4400 | ` */` |
|     56 | 4401 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4402 |  |
|      - | 4403 | `	const char *zFormat;` |
|      - | 4404 | `	int nLen;` |
|     57 | 4405 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4406 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 4407 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4408 | `		return PH7_OK;` |
|      - | 4409 | `	}` |
|      - | 4410 | `	/* Extract the string format */` |
|     55 | 4411 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     55 | 4412 | `	if( nLen < 1 ){` |
|      - | 4413 | `		/* Empty string */` |
|    ! 0 | 4414 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4415 | `		return PH7_OK;` |
|      - | 4416 | `	}` |
|      - | 4417 | `	/* Format the string */` |
|     55 | 4418 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,0,FALSE);` |
|     55 | 4419 | `	return PH7_OK;` |
|     29 | 4420 |  |
|      - | 4421 | `/*` |
|      - | 4422 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 4423 | ` */` |
|    110 | 4424 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4425 |  |
|    111 | 4426 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 4427 | `	/* Call the VM output consumer directly */` |
|    111 | 4428 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 4429 | `	/* Increment counter */` |
|    111 | 4430 | `	*pCounter += nLen;` |
|    111 | 4431 | `	return PH7_OK;` |
|      1 | 4432 |  |
|      - | 4433 | `/*` |
|      - | 4434 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 4435 | ` *  Output a formatted string.` |
|      - | 4436 | ` * Parameters` |
|      - | 4437 | ` *  $format` |
|      - | 4438 | ` *   See sprintf() for a description of format.` |
|      - | 4439 | ` * Return` |
|      - | 4440 | ` *  The length of the outputted string.` |
|      - | 4441 | ` */` |
|     42 | 4442 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4443 |  |
|     43 | 4444 | `	ph7_int64 nCounter = 0;` |
|      - | 4445 | `	const char *zFormat;` |
|      - | 4446 | `	int nLen;` |
|     43 | 4447 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4448 | `		/* Missing/Invalid arguments,return 0 */` |
|      3 | 4449 | `		ph7_result_int(pCtx,0);` |
|      3 | 4450 | `		return PH7_OK;` |
|      - | 4451 | `	}` |
|      - | 4452 | `	/* Extract the string format */` |
|     41 | 4453 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 4454 | `	if( nLen < 1 ){` |
|      - | 4455 | `		/* Empty string */` |
|    ! 0 | 4456 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4457 | `		return PH7_OK;` |
|      - | 4458 | `	}` |
|      - | 4459 | `	/* Format the string */` |
|     41 | 4460 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 4461 | `	/* Return the length of the outputted string */` |
|     41 | 4462 | `	ph7_result_int64(pCtx,nCounter);` |
|     41 | 4463 | `	return PH7_OK;` |
|     22 | 4464 |  |
|      - | 4465 | `/*` |
|      - | 4466 | ` * int vprintf(string $format,array $args)` |
|      - | 4467 | ` *  Output a formatted string.` |
|      - | 4468 | ` * Parameters` |
|      - | 4469 | ` *  $format` |
|      - | 4470 | ` *   See sprintf() for a description of format.` |
|      - | 4471 | ` * Return` |
|      - | 4472 | ` *  The length of the outputted string.` |
|      - | 4473 | ` */` |
|      2 | 4474 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4475 |  |
|      3 | 4476 | `	ph7_int64 nCounter = 0;` |
|      - | 4477 | `	const char *zFormat;` |
|      - | 4478 | `	ph7_hashmap *pMap;` |
|      - | 4479 | `	SySet sArg;` |
|      - | 4480 | `	int nLen,n;` |
|      3 | 4481 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4482 | `		/* Missing/Invalid arguments,return 0 */` |
|    ! 0 | 4483 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4484 | `		return PH7_OK;` |
|      - | 4485 | `	}` |
|      - | 4486 | `	/* Extract the string format */` |
|      3 | 4487 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4488 | `	if( nLen < 1 ){` |
|      - | 4489 | `		/* Empty string */` |
|    ! 0 | 4490 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4491 | `		return PH7_OK;` |
|      - | 4492 | `	}` |
|      - | 4493 | `	/* Point to the hashmap */` |
|      3 | 4494 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4495 | `	/* Extract arguments from the hashmap */` |
|      3 | 4496 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4497 | `	/* Format the string */` |
|      3 | 4498 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 4499 | `	/* Return the length of the outputted string */` |
|      3 | 4500 | `	ph7_result_int64(pCtx,nCounter);` |
|      - | 4501 | `	/* Release the container */` |
|      3 | 4502 | `	SySetRelease(&sArg);` |
|      3 | 4503 | `	return PH7_OK;` |
|      2 | 4504 |  |
|      - | 4505 | `/*` |
|      - | 4506 | ` * int vsprintf(string $format,array $args)` |
|      - | 4507 | ` *  Output a formatted string.` |
|      - | 4508 | ` * Parameters` |
|      - | 4509 | ` *  $format` |
|      - | 4510 | ` *   See sprintf() for a description of format.` |
|      - | 4511 | ` * Return` |
|      - | 4512 | ` *  A string produced according to the formatting string format.` |
|      - | 4513 | ` */` |
|     10 | 4514 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4515 |  |
|      - | 4516 | `	const char *zFormat;` |
|      - | 4517 | `	ph7_hashmap *pMap;` |
|      - | 4518 | `	SySet sArg;` |
|      - | 4519 | `	int nLen,n;` |
|     11 | 4520 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4521 | `		/* Missing/Invalid arguments,return the empty string */` |
|      5 | 4522 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 4523 | `		return PH7_OK;` |
|      - | 4524 | `	}` |
|      - | 4525 | `	/* Extract the string format */` |
|      7 | 4526 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4527 | `	if( nLen < 1 ){` |
|      - | 4528 | `		/* Empty string */` |
|    ! 0 | 4529 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4530 | `		return PH7_OK;` |
|      - | 4531 | `	}` |
|      - | 4532 | `	/* Point to hashmap */` |
|      7 | 4533 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4534 | `	/* Extract arguments from the hashmap */` |
|      7 | 4535 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4536 | `	/* Format the string */` |
|      7 | 4537 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),0,TRUE);` |
|      - | 4538 | `	/* Release the container */` |
|      7 | 4539 | `	SySetRelease(&sArg);` |
|      7 | 4540 | `	return PH7_OK;` |
|      6 | 4541 |  |
|      - | 4542 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 4543 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4544 | `/*` |
|      - | 4545 | ` * Symisc eXtension.` |
|      - | 4546 | ` * string size_format(int64 $size)` |
|      - | 4547 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 4548 | ` *  Example:` |
|      - | 4549 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 4550 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 4551 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 4552 | ` * Parameter` |
|      - | 4553 | ` *  $size` |
|      - | 4554 | ` *    Entity size in bytes.` |
|      - | 4555 | ` * Return` |
|      - | 4556 | ` *   Formatted string representation of the given size.` |
|      - | 4557 | ` */` |
|     24 | 4558 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4559 |  |
|      - | 4560 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 4561 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 4562 | `	sxi32 nRest,i_32;` |
|      - | 4563 | `	ph7_int64 iSize;` |
|     25 | 4564 | `	int c = -1; /* index in zUnit[] */` |
|      - | 4565 |  |
|     25 | 4566 | `	if( nArg < 1 ){` |
|      - | 4567 | `		/* Missing argument,return the empty string */` |
|      3 | 4568 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4569 | `		return PH7_OK;` |
|      - | 4570 | `	}` |
|      - | 4571 | `	/* Extract the given size */` |
|     23 | 4572 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 4573 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 4574 | `		/* Don't bother formatting,return immediately */` |
|      5 | 4575 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 4576 | `		return PH7_OK;` |
|      - | 4577 | `	}` |
|     19 | 4578 | `	for(;;){` |
|     39 | 4579 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 4580 | `		iSize >>= 10;` |
|     39 | 4581 | `		c++;` |
|     39 | 4582 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 4583 | `			break;` |
|      - | 4584 | `		}` |
|      1 | 4585 | `	}` |
|     19 | 4586 | `	nRest /= 100;` |
|     19 | 4587 | `	if( nRest > 9 ){` |
|    ! 0 | 4588 | `		nRest = 9;` |
|    ! 0 | 4589 | `	}` |
|     19 | 4590 | `	if( iSize > 999 ){` |
|    ! 0 | 4591 | `		c++;` |
|    ! 0 | 4592 | `		nRest = 9;` |
|    ! 0 | 4593 | `		iSize = 0;` |
|    ! 0 | 4594 | `	}` |
|     19 | 4595 | `	i_32 = (sxi32)iSize;` |
|      - | 4596 | `	/* Format */` |
|     19 | 4597 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 4598 | `	return PH7_OK;` |
|     13 | 4599 |  |
|      - | 4600 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4601 | `/*` |
|      - | 4602 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 4603 | ` *   Calculate the md5 hash of a string.` |
|      - | 4604 | ` * Parameter` |
|      - | 4605 | ` *  $str` |
|      - | 4606 | ` *   Input string` |
|      - | 4607 | ` * $raw_output` |
|      - | 4608 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4609 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4610 | ` * Return` |
|      - | 4611 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 4612 | ` */` |
|     10 | 4613 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4614 |  |
|      - | 4615 | `	unsigned char zDigest[16];` |
|     11 | 4616 | `	int raw_output = FALSE;` |
|      - | 4617 | `	const void *pIn;` |
|      - | 4618 | `	int nLen;` |
|     11 | 4619 | `	if( nArg < 1 ){` |
|      - | 4620 | `		/* Missing arguments,return the empty string */` |
|      3 | 4621 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4622 | `		return PH7_OK;` |
|      - | 4623 | `	}` |
|      - | 4624 | `	/* Extract the input string */` |
|      9 | 4625 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 4626 | `	if( nLen < 1 ){` |
|      - | 4627 | `		/* Empty string */` |
|    ! 0 | 4628 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4629 | `		return PH7_OK;` |
|      - | 4630 | `	}` |
|      9 | 4631 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      3 | 4632 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 | 4633 | `	}` |
|      - | 4634 | `	/* Compute the MD5 digest */` |
|      9 | 4635 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|      9 | 4636 | `	if( raw_output ){` |
|      - | 4637 | `		/* Output raw digest */` |
|      3 | 4638 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      2 | 4639 | `	}else{` |
|      - | 4640 | `		/* Perform a binary to hex conversion */` |
|      7 | 4641 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4642 | `	}` |
|      9 | 4643 | `	return PH7_OK;` |
|      6 | 4644 |  |
|      - | 4645 | `/*` |
|      - | 4646 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 4647 | ` *   Calculate the sha1 hash of a string.` |
|      - | 4648 | ` * Parameter` |
|      - | 4649 | ` *  $str` |
|      - | 4650 | ` *   Input string` |
|      - | 4651 | ` * $raw_output` |
|      - | 4652 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4653 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4654 | ` * Return` |
|      - | 4655 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 4656 | ` */` |
|      8 | 4657 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4658 |  |
|      - | 4659 | `	unsigned char zDigest[20];` |
|      9 | 4660 | `	int raw_output = FALSE;` |
|      - | 4661 | `	const void *pIn;` |
|      - | 4662 | `	int nLen;` |
|      9 | 4663 | `	if( nArg < 1 ){` |
|      - | 4664 | `		/* Missing arguments,return the empty string */` |
|      3 | 4665 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4666 | `		return PH7_OK;` |
|      - | 4667 | `	}` |
|      - | 4668 | `	/* Extract the input string */` |
|      7 | 4669 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4670 | `	if( nLen < 1 ){` |
|      - | 4671 | `		/* Empty string */` |
|    ! 0 | 4672 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4673 | `		return PH7_OK;` |
|      - | 4674 | `	}` |
|      7 | 4675 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      3 | 4676 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 | 4677 | `	}` |
|      - | 4678 | `	/* Compute the SHA1 digest */` |
|      7 | 4679 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|      7 | 4680 | `	if( raw_output ){` |
|      - | 4681 | `		/* Output raw digest */` |
|      3 | 4682 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      2 | 4683 | `	}else{` |
|      - | 4684 | `		/* Perform a binary to hex conversion */` |
|      5 | 4685 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4686 | `	}` |
|      7 | 4687 | `	return PH7_OK;` |
|      5 | 4688 |  |
|      - | 4689 | `/*` |
|      - | 4690 | ` * int64 crc32(string $str)` |
|      - | 4691 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 4692 | ` * Parameter` |
|      - | 4693 | ` *  $str` |
|      - | 4694 | ` *   Input string` |
|      - | 4695 | ` * Return` |
|      - | 4696 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 4697 | ` */` |
|      4 | 4698 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4699 |  |
|      - | 4700 | `	const void *pIn;` |
|      - | 4701 | `	sxu32 nCRC;` |
|      - | 4702 | `	int nLen;` |
|      5 | 4703 | `	if( nArg < 1 ){` |
|      - | 4704 | `		/* Missing arguments,return 0 */` |
|      3 | 4705 | `		ph7_result_int(pCtx,0);` |
|      3 | 4706 | `		return PH7_OK;` |
|      - | 4707 | `	}` |
|      - | 4708 | `	/* Extract the input string */` |
|      3 | 4709 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4710 | `	if( nLen < 1 ){` |
|      - | 4711 | `		/* Empty string */` |
|    ! 0 | 4712 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4713 | `		return PH7_OK;` |
|      - | 4714 | `	}` |
|      - | 4715 | `	/* Calculate the sum */` |
|      3 | 4716 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 4717 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 4718 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 4719 | `	return PH7_OK;` |
|      3 | 4720 |  |
|      - | 4721 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 4722 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4723 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 4724 | `/*` |
|      - | 4725 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 4726 |  |
|      - | 4727 | ` */` |
|      4 | 4728 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 4729 | `	const char *zInput, /* Raw input */` |
|      - | 4730 | `	int nByte,  /* Input length */` |
|      - | 4731 | `	int delim,  /* Delimiter */` |
|      - | 4732 | `	int encl,   /* Enclosure */` |
|      - | 4733 | `	int escape,  /* Escape character */` |
|      - | 4734 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 4735 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 4736 | `	)` |
|      1 | 4737 |  |
|      5 | 4738 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 4739 | `	const char *zIn = zInput;` |
|      - | 4740 | `	const char *zPtr;` |
|      - | 4741 | `	int isEnc;` |
|      - | 4742 | `	/* Start processing */` |
|      8 | 4743 | `	for(;;){` |
|     17 | 4744 | `		if( zIn >= zEnd ){` |
|      - | 4745 | `			/* No more input to process */` |
|      5 | 4746 | `			break;` |
|      - | 4747 | `		}` |
|     13 | 4748 | `		isEnc = 0;` |
|     13 | 4749 | `		zPtr = zIn;` |
|      - | 4750 | `		/* Find the first delimiter */` |
|     27 | 4751 | `		while( zIn < zEnd ){` |
|     23 | 4752 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 4753 | `				/* Delimiter found,break imediately */` |
|      5 | 4754 | `				break;` |
|     15 | 4755 | `			}else if( zIn[0] == encl ){` |
|      - | 4756 | `				/* Inside enclosure? */` |
|    ! 0 | 4757 | `				isEnc = !isEnc;` |
|     15 | 4758 | `			}else if( zIn[0] == escape ){` |
|      - | 4759 | `				/* Escape sequence */` |
|    ! 0 | 4760 | `				zIn++;` |
|    ! 0 | 4761 | `			}` |
|      - | 4762 | `			/* Advance the cursor */` |
|     15 | 4763 | `			zIn++;` |
|      1 | 4764 | `		}` |
|     13 | 4765 | `		if( zIn > zPtr ){` |
|     13 | 4766 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 4767 | `			sxi32 rc;` |
|      - | 4768 | `			/* Invoke the supllied callback */` |
|     13 | 4769 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 4770 | `				zPtr++;` |
|    ! 0 | 4771 | `				nByteChunk-=2;` |
|    ! 0 | 4772 | `			}` |
|     13 | 4773 | `			if( nByteChunk > 0 ){` |
|     13 | 4774 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 4775 | `				if( rc == SXERR_ABORT ){` |
|      - | 4776 | `					/* User callback request an operation abort */` |
|    ! 0 | 4777 | `					break;` |
|      - | 4778 | `				}` |
|      6 | 4779 | `			}` |
|      6 | 4780 | `		}` |
|      - | 4781 | `		/* Ignore trailing delimiter */` |
|     21 | 4782 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 4783 | `			zIn++;` |
|      1 | 4784 | `		}` |
|      1 | 4785 | `	}` |
|      5 | 4786 | `	return SXRET_OK;` |
|      1 | 4787 |  |
|      - | 4788 | `/*` |
|      - | 4789 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 4790 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 4791 | ` * argument to this callback.` |
|      - | 4792 | ` */` |
|     12 | 4793 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 4794 |  |
|     13 | 4795 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 4796 | `	ph7_value sEntry;` |
|      - | 4797 | `	SyString sToken;` |
|      - | 4798 | `	/* Insert the token in the given array */` |
|     13 | 4799 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 4800 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 4801 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 4802 | `	if( sToken.nByte < 1){` |
|    ! 0 | 4803 | `		return SXRET_OK;` |
|      - | 4804 | `	}` |
|     13 | 4805 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 4806 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 4807 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 4808 | `	return SXRET_OK;` |
|      7 | 4809 |  |
|      - | 4810 | `/*` |
|      - | 4811 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 4812 | ` *  Parse a CSV string into an array.` |
|      - | 4813 | ` * Parameters` |
|      - | 4814 | ` *  $input` |
|      - | 4815 | ` *   The string to parse.` |
|      - | 4816 | ` *  $delimiter` |
|      - | 4817 | ` *   Set the field delimiter (one character only).` |
|      - | 4818 | ` *  $enclosure` |
|      - | 4819 | ` *   Set the field enclosure character (one character only).` |
|      - | 4820 | ` *  $escape` |
|      - | 4821 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 4822 | ` * Return` |
|      - | 4823 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 4824 | ` */` |
|      4 | 4825 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4826 |  |
|      - | 4827 | `	const char *zInput,*zPtr;` |
|      - | 4828 | `	ph7_value *pArray;` |
|      5 | 4829 | `	int delim  = ',';   /* Delimiter */` |
|      5 | 4830 | `	int encl   = '"' ;  /* Enclosure */` |
|      5 | 4831 | `	int escape = '\\';  /* Escape character */` |
|      - | 4832 | `	int nLen;` |
|      5 | 4833 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4834 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 4835 | `		ph7_result_null(pCtx);` |
|      3 | 4836 | `		return PH7_OK;` |
|      - | 4837 | `	}` |
|      - | 4838 | `	/* Extract the raw input */` |
|      3 | 4839 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4840 | `	if( nArg > 1 ){` |
|      - | 4841 | `		int i;` |
|      3 | 4842 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 4843 | `			/* Extract the delimiter */` |
|      3 | 4844 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 4845 | `			if( i > 0 ){` |
|      3 | 4846 | `				delim = zPtr[0];` |
|      1 | 4847 | `			}` |
|      1 | 4848 | `		}` |
|      3 | 4849 | `		if( nArg > 2 ){` |
|      3 | 4850 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 4851 | `				/* Extract the enclosure */` |
|      3 | 4852 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 4853 | `				if( i > 0 ){` |
|      3 | 4854 | `					encl = zPtr[0];` |
|      1 | 4855 | `				}` |
|      1 | 4856 | `			}` |
|      3 | 4857 | `			if( nArg > 3 ){` |
|      3 | 4858 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 4859 | `					/* Extract the escape character */` |
|      3 | 4860 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 4861 | `					if( i > 0 ){` |
|      3 | 4862 | `						escape = zPtr[0];` |
|      1 | 4863 | `					}` |
|      1 | 4864 | `				}` |
|      1 | 4865 | `			}` |
|      1 | 4866 | `		}` |
|      1 | 4867 | `	}` |
|      - | 4868 | `	/* Create our array */` |
|      3 | 4869 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4870 | `	if( pArray == 0 ){` |
|    ! 0 | 4871 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4872 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4873 | `		return PH7_OK;` |
|      - | 4874 | `	}` |
|      - | 4875 | `	/* Parse the raw input */` |
|      3 | 4876 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 4877 | `	/* Return the freshly created array */` |
|      3 | 4878 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4879 | `	return PH7_OK;` |
|      3 | 4880 |  |
|      - | 4881 | `/*` |
|      - | 4882 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 4883 | ` * container.` |
|      - | 4884 | ` * Refer to [strip_tags()].` |
|      - | 4885 | ` */` |
|     10 | 4886 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 4887 |  |
|     11 | 4888 | `	const char *zEnd = &zTag[nByte];` |
|      - | 4889 | `	const char *zPtr;` |
|      - | 4890 | `	SyString sEntry;` |
|      - | 4891 | `	/* Strip tags */` |
|     10 | 4892 | `	for(;;){` |
|     45 | 4893 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 4894 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 4895 | `				zTag++;` |
|      1 | 4896 | `		}` |
|     21 | 4897 | `		if( zTag >= zEnd ){` |
|     11 | 4898 | `			break;` |
|      - | 4899 | `		}` |
|     11 | 4900 | `		zPtr = zTag;` |
|      - | 4901 | `		/* Delimit the tag */` |
|     25 | 4902 | `		while(zTag < zEnd ){` |
|     25 | 4903 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 4904 | `				/* UTF-8 stream */` |
|      3 | 4905 | `				zTag++;` |
|      5 | 4906 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 4907 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 4908 | `				break;` |
|    ! 0 | 4909 | `			}else{` |
|     13 | 4910 | `				zTag++;` |
|      - | 4911 | `			}` |
|      1 | 4912 | `		}` |
|     11 | 4913 | `		if( zTag > zPtr ){` |
|      - | 4914 | `			/* Perform the insertion */` |
|     11 | 4915 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 4916 | `			SyStringFullTrim(&sEntry);` |
|     11 | 4917 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 4918 | `		}` |
|      - | 4919 | `		/* Jump the trailing '>' */` |
|     11 | 4920 | `		zTag++;` |
|      1 | 4921 | `	}` |
|     11 | 4922 | `	return SXRET_OK;` |
|      1 | 4923 |  |
|      - | 4924 | `/*` |
|      - | 4925 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 4926 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 4927 | ` * Refer to [strip_tags()].` |
|      - | 4928 | ` */` |
|     36 | 4929 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 4930 |  |
|     37 | 4931 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 4932 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 4933 | `		SyString sTag;` |
|     85 | 4934 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 4935 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 4936 | `			zTag++;` |
|      1 | 4937 | `		}` |
|      - | 4938 | `		/* Delimit the tag */` |
|     25 | 4939 | `		zCur = zTag;` |
|     77 | 4940 | `		while(zTag < zEnd ){` |
|     77 | 4941 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 4942 | `				/* UTF-8 stream */` |
|      5 | 4943 | `				zTag++;` |
|      9 | 4944 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 4945 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 4946 | `				break;` |
|    ! 0 | 4947 | `			}else{` |
|     49 | 4948 | `				zTag++;` |
|      - | 4949 | `			}` |
|      1 | 4950 | `		}` |
|     25 | 4951 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 4952 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 4953 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 4954 | `		if( sTag.nByte > 0 ){` |
|      - | 4955 | `			SyString *aEntry,*pEntry;` |
|      - | 4956 | `			sxi32 rc;` |
|      - | 4957 | `			sxu32 n;` |
|      - | 4958 | `			/* Perform the lookup */` |
|     25 | 4959 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 4960 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 4961 | `				pEntry = &aEntry[n];` |
|      - | 4962 | `				/* Do the comparison */` |
|     25 | 4963 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 4964 | `				if( !rc ){` |
|     21 | 4965 | `					return SXRET_OK;` |
|      - | 4966 | `				}` |
|      3 | 4967 | `			}` |
|      2 | 4968 | `		}` |
|      2 | 4969 | `	}` |
|      - | 4970 | `	/* No such tag */` |
|     17 | 4971 | `	return SXERR_NOTFOUND;` |
|     19 | 4972 |  |
|      - | 4973 | `/*` |
|      - | 4974 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 4975 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 4976 | ` * Refer to [strip_tags()].` |
|      - | 4977 | ` */` |
|     16 | 4978 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 4979 |  |
|     17 | 4980 | `	const char *zEnd = &zIn[nByte];` |
|      - | 4981 | `	const char *zPtr,*zTag;` |
|      - | 4982 | `	SySet sSet;` |
|      - | 4983 | `	/* initialize the set of allowed tags */` |
|     17 | 4984 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 4985 | `	if( nTaglen > 0 ){` |
|      - | 4986 | `		/* Set of allowed tags */` |
|     11 | 4987 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 4988 | `	}` |
|      - | 4989 | `	/* Set the empty string */` |
|     17 | 4990 | `	ph7_result_string(pCtx,"",0);` |
|      - | 4991 | `	/* Start processing */` |
|     26 | 4992 | `	for(;;){` |
|     53 | 4993 | `		if(zIn >= zEnd){` |
|      - | 4994 | `			/* No more input to process */` |
|     15 | 4995 | `			break;` |
|      - | 4996 | `		}` |
|     39 | 4997 | `		zPtr = zIn;` |
|      - | 4998 | `		/* Find a tag */` |
|    133 | 4999 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 5000 | `			zIn++;` |
|      1 | 5001 | `		}` |
|     39 | 5002 | `		if( zIn > zPtr ){` |
|      - | 5003 | `			/* Consume raw input */` |
|     21 | 5004 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 5005 | `		}` |
|      - | 5006 | `		/* Ignore trailing null bytes */` |
|     39 | 5007 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 5008 | `			zIn++;` |
|    ! 0 | 5009 | `		}` |
|     39 | 5010 | `		if(zIn >= zEnd){` |
|      - | 5011 | `			/* No more input to process */` |
|      3 | 5012 | `			break;` |
|      - | 5013 | `		}` |
|     37 | 5014 | `		if( zIn[0] == '<' ){` |
|      - | 5015 | `			sxi32 rc;` |
|     37 | 5016 | `			zTag = zIn++;` |
|      - | 5017 | `			/* Delimit the tag */` |
|    127 | 5018 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 5019 | `				zIn++;` |
|      1 | 5020 | `			}` |
|     37 | 5021 | `			if( zIn < zEnd ){` |
|     37 | 5022 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 5023 | `			}` |
|      - | 5024 | `			/* Query the set */` |
|     37 | 5025 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 5026 | `			if( rc == SXRET_OK ){` |
|      - | 5027 | `				/* Keep the tag */` |
|     21 | 5028 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 5029 | `			}` |
|     18 | 5030 | `		}` |
|      1 | 5031 | `	}` |
|      - | 5032 | `	/* Cleanup */` |
|     17 | 5033 | `	SySetRelease(&sSet);` |
|     17 | 5034 | `	return SXRET_OK;` |
|      1 | 5035 |  |
|      - | 5036 | `/*` |
|      - | 5037 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 5038 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 5039 | ` * Parameters` |
|      - | 5040 | ` *  $str` |
|      - | 5041 | ` *  The input string.` |
|      - | 5042 | ` * $allowable_tags` |
|      - | 5043 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 5044 | ` * Return` |
|      - | 5045 | ` *  Returns the stripped string.` |
|      - | 5046 | ` */` |
|     16 | 5047 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5048 |  |
|     17 | 5049 | `	const char *zTaglist = 0;` |
|      - | 5050 | `	const char *zString;` |
|     17 | 5051 | `	int nTaglen = 0;` |
|      - | 5052 | `	int nLen;` |
|     17 | 5053 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5054 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 5055 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5056 | `		return PH7_OK;` |
|      - | 5057 | `	}` |
|      - | 5058 | `	/* Point to the raw string */` |
|     15 | 5059 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 5060 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 5061 | `		/* Allowed tag */` |
|     11 | 5062 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 5063 | `	}` |
|      - | 5064 | `	/* Process input */` |
|     15 | 5065 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 5066 | `	return PH7_OK;` |
|      9 | 5067 |  |
|      - | 5068 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5069 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5070 | `/*` |
|      - | 5071 | ` * string str_shuffle(string $str)` |
|      - | 5072 |  |
|      - | 5073 | ` *  Randomly shuffles a string.` |
|      - | 5074 | ` * Parameters` |
|      - | 5075 | ` *  $str` |
|      - | 5076 | ` *   The input string.` |
|      - | 5077 | ` * Return` |
|      - | 5078 | ` *  Returns the shuffled string.` |
|      - | 5079 | ` */` |
|     12 | 5080 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5081 |  |
|      - | 5082 | `	const char *zString;` |
|      - | 5083 | `	int nLen,i,c;` |
|      - | 5084 | `	sxu32 iR;` |
|     13 | 5085 | `	if( nArg < 1 ){` |
|      - | 5086 | `		/* Missing arguments,return the empty string */` |
|      3 | 5087 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5088 | `		return PH7_OK;` |
|      - | 5089 | `	}` |
|      - | 5090 | `	/* Extract the target string */` |
|     11 | 5091 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5092 | `	if( nLen < 1 ){` |
|      - | 5093 | `		/* Nothing to shuffle */` |
|      3 | 5094 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5095 | `		return PH7_OK;` |
|      - | 5096 | `	}` |
|      - | 5097 | `	/* Shuffle the string */` |
|     43 | 5098 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 5099 | `		/* Generate a random number first */` |
|     35 | 5100 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 5101 | `		/* Extract a random offset */` |
|     35 | 5102 | `		c = zString[iR % nLen];` |
|      - | 5103 | `		/* Append it */` |
|     35 | 5104 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 5105 | `	}` |
|      9 | 5106 | `	return PH7_OK;` |
|      7 | 5107 |  |
|      - | 5108 | `/*` |
|      - | 5109 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 5110 | ` *  Convert a string to an array.` |
|      - | 5111 | ` * Parameters` |
|      - | 5112 | ` * $str` |
|      - | 5113 | ` *  The input string.` |
|      - | 5114 | ` * $split_length` |
|      - | 5115 | ` *  Maximum length of the chunk.` |
|      - | 5116 | ` * Return` |
|      - | 5117 | ` *  If the optional split_length parameter is specified, the returned array` |
|      - | 5118 | ` *  will be broken down into chunks with each being split_length in length, otherwise` |
|      - | 5119 | ` *  each chunk will be one character in length. FALSE is returned if split_length is less than 1.` |
|      - | 5120 | ` *  If the split_length length exceeds the length of string, the entire string is returned` |
|      - | 5121 | ` *  as the first (and only) array element.` |
|      - | 5122 | ` */` |
|      8 | 5123 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5124 |  |
|      - | 5125 | `	const char *zString,*zEnd;` |
|      - | 5126 | `	ph7_value *pArray,*pValue;` |
|      - | 5127 | `	int split_len;` |
|      - | 5128 | `	int nLen;` |
|      9 | 5129 | `	if( nArg < 1 ){` |
|      - | 5130 | `		/* Missing arguments,return FALSE */` |
|      5 | 5131 | `		ph7_result_bool(pCtx,0);` |
|      5 | 5132 | `		return PH7_OK;` |
|      - | 5133 | `	}` |
|      - | 5134 | `	/* Point to the target string */` |
|      5 | 5135 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 5136 | `	if( nLen < 1 ){` |
|      - | 5137 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 5138 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5139 | `		return PH7_OK;` |
|      - | 5140 | `	}` |
|      5 | 5141 | `	split_len = (int)sizeof(char);` |
|      5 | 5142 | `	if( nArg > 1 ){` |
|      - | 5143 | `		/* Split length */` |
|      5 | 5144 | `		split_len = ph7_value_to_int(apArg[1]);` |
|      5 | 5145 | `		if( split_len < 1 ){` |
|      - | 5146 | `			/* Invalid length,return FALSE */` |
|      3 | 5147 | `			ph7_result_bool(pCtx,0);` |
|      3 | 5148 | `			return PH7_OK;` |
|      - | 5149 | `		}` |
|      3 | 5150 | `		if( split_len > nLen ){` |
|    ! 0 | 5151 | `			split_len = nLen;` |
|    ! 0 | 5152 | `		}` |
|      1 | 5153 | `	}` |
|      - | 5154 | `	/* Create the array and the scalar value */` |
|      3 | 5155 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 5156 | `	/*Chunk value */` |
|      3 | 5157 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 5158 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 5159 | `		/* Return FALSE */` |
|    ! 0 | 5160 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5161 | `		return PH7_OK;` |
|      - | 5162 | `	}` |
|      - | 5163 | `	/* Point to the end of the string */` |
|      3 | 5164 | `	zEnd = &zString[nLen];` |
|      - | 5165 | `	/* Perform the requested operation */` |
|      7 | 5166 | `	for(;;){` |
|      - | 5167 | `		int nMax;` |
|      9 | 5168 | `		if( zString >= zEnd ){` |
|      - | 5169 | `			/* No more input to process */` |
|      3 | 5170 | `			break;` |
|      - | 5171 | `		}` |
|      7 | 5172 | `		nMax = (int)(zEnd-zString);` |
|      7 | 5173 | `		if( nMax < split_len ){` |
|    ! 0 | 5174 | `			split_len = nMax;` |
|    ! 0 | 5175 | `		}` |
|      - | 5176 | `		/* Copy the current chunk */` |
|      7 | 5177 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 5178 | `		/* Insert it */` |
|      7 | 5179 | `		ph7_array_add_elem(pArray,0,pValue); /* Will make it's own copy */` |
|      - | 5180 | `		/* reset the string cursor */` |
|      7 | 5181 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 5182 | `		/* Update position */` |
|      7 | 5183 | `		zString += split_len;` |
|      1 | 5184 | `	}` |
|      - | 5185 | `	/*` |
|      - | 5186 | `	 * Return the array.` |
|      - | 5187 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 5188 | `	 * upon we return from this function.` |
|      - | 5189 | `	 */` |
|      3 | 5190 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5191 | `	return PH7_OK;` |
|      5 | 5192 |  |
|      - | 5193 | `/*` |
|      - | 5194 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 5195 | ` * Refer to [strspn()].` |
|      - | 5196 | ` */` |
|     28 | 5197 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 5198 |  |
|     29 | 5199 | `	const char *zIn = *pzIn;` |
|      - | 5200 | `	const char *zPtr;` |
|      - | 5201 | `	/* Ignore leading white spaces */` |
|     29 | 5202 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 5203 | `		zIn++;` |
|    ! 0 | 5204 | `	}` |
|     29 | 5205 | `	if( zIn >= zEnd ){` |
|      - | 5206 | `		/* End of input */` |
|    ! 0 | 5207 | `		return SXERR_EOF;` |
|      - | 5208 | `	}` |
|     29 | 5209 | `	zPtr = zIn;` |
|      - | 5210 | `	/* Extract the token */` |
|    201 | 5211 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 5212 | `		zIn++;` |
|      1 | 5213 | `	}` |
|     29 | 5214 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5215 | `	/* Synchronize pointers */` |
|     29 | 5216 | `	*pzIn = zIn;` |
|      - | 5217 | `	/* Return to the caller */` |
|     29 | 5218 | `	return SXRET_OK;` |
|     15 | 5219 |  |
|      - | 5220 | `/*` |
|      - | 5221 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 5222 | ` * return the longest match.` |
|      - | 5223 | ` * Refer to [strspn()].` |
|      - | 5224 | ` */` |
|     18 | 5225 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5226 |  |
|     19 | 5227 | `	const char *zEnd = &zString[nLen];` |
|     19 | 5228 | `	const char *zIn = zString;` |
|      - | 5229 | `	int i,c;` |
|     45 | 5230 | `	for(;;){` |
|     91 | 5231 | `		if( zString >= zEnd ){` |
|      7 | 5232 | `			break;` |
|      - | 5233 | `		}` |
|      - | 5234 | `		/* Extract current character */` |
|     85 | 5235 | `		c = zString[0];` |
|      - | 5236 | `		/* Perform the lookup */` |
|    383 | 5237 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 5238 | `			if( c == zMask[i] ){` |
|      - | 5239 | `				/* Character found */` |
|     73 | 5240 | `				break;` |
|      - | 5241 | `			}` |
|    150 | 5242 | `		}` |
|     85 | 5243 | `		if( i >= nMaskLen ){` |
|      - | 5244 | `			/* Character not in the current mask,break immediately */` |
|     13 | 5245 | `			break;` |
|      - | 5246 | `		}` |
|      - | 5247 | `		/* Advance cursor */` |
|     73 | 5248 | `		zString++;` |
|      1 | 5249 | `	}` |
|      - | 5250 | `	/* Longest match */` |
|     19 | 5251 | `	return (int)(zString-zIn);` |
|      1 | 5252 |  |
|      - | 5253 | `/*` |
|      - | 5254 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 5255 | ` * Refer to [strcspn()].` |
|      - | 5256 | ` */` |
|     10 | 5257 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5258 |  |
|     11 | 5259 | `	const char *zEnd = &zString[nLen];` |
|     11 | 5260 | `	const char *zIn = zString;` |
|      - | 5261 | `	int i,c;` |
|     12 | 5262 | `	for(;;){` |
|     25 | 5263 | `		if( zString >= zEnd ){` |
|      3 | 5264 | `			break;` |
|      - | 5265 | `		}` |
|      - | 5266 | `		/* Extract current character */` |
|     23 | 5267 | `		c = zString[0];` |
|      - | 5268 | `		/* Perform the lookup */` |
|     51 | 5269 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 5270 | `			if( c == zMask[i] ){` |
|      9 | 5271 | `				break;` |
|      - | 5272 | `			}` |
|     15 | 5273 | `		}` |
|     23 | 5274 | `		if( i < nMaskLen ){` |
|      - | 5275 | `			/* Character in the current mask,break immediately */` |
|      9 | 5276 | `			break;` |
|      - | 5277 | `		}` |
|      - | 5278 | `		/* Advance cursor */` |
|     15 | 5279 | `		zString++;` |
|      1 | 5280 | `	}` |
|      - | 5281 | `	/* Longest match */` |
|     11 | 5282 | `	return (int)(zString-zIn);` |
|      1 | 5283 |  |
|      - | 5284 | `/*` |
|      - | 5285 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5286 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 5287 | ` *  of characters contained within a given mask.` |
|      - | 5288 | ` * Parameters` |
|      - | 5289 | ` * $str` |
|      - | 5290 | ` *  The input string.` |
|      - | 5291 | ` * $mask` |
|      - | 5292 | ` *  The list of allowable characters.` |
|      - | 5293 | ` * $start` |
|      - | 5294 | ` *  The position in subject to start searching.` |
|      - | 5295 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5296 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5297 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5298 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5299 | ` *  start'th position from the end of subject.` |
|      - | 5300 | ` * $length` |
|      - | 5301 | ` *  The length of the segment from subject to examine.` |
|      - | 5302 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5303 | ` *  characters after the starting position.` |
|      - | 5304 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5305 | ` *  position up to length characters from the end of subject.` |
|      - | 5306 | ` * Return` |
|      - | 5307 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 5308 | ` * in mask.` |
|      - | 5309 | ` */` |
|     26 | 5310 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5311 |  |
|      - | 5312 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5313 | `	int iMasklen,iLen;` |
|      - | 5314 | `	SyString sToken;` |
|     27 | 5315 | `	int iCount = 0;` |
|      - | 5316 | `	int rc;` |
|     27 | 5317 | `	if( nArg < 2 ){` |
|      - | 5318 | `		/* Missing agruments,return zero */` |
|      3 | 5319 | `		ph7_result_int(pCtx,0);` |
|      3 | 5320 | `		return PH7_OK;` |
|      - | 5321 | `	}` |
|      - | 5322 | `	/* Extract the target string */` |
|     25 | 5323 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5324 | `	/* Extract the mask */` |
|     25 | 5325 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 5326 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 5327 | `		/* Nothing to process,return zero */` |
|      7 | 5328 | `		ph7_result_int(pCtx,0);` |
|      7 | 5329 | `		return PH7_OK;` |
|      - | 5330 | `	}` |
|     19 | 5331 | `	if( nArg > 2 ){` |
|      - | 5332 | `		int nOfft;` |
|      - | 5333 | `		/* Extract the offset */` |
|      9 | 5334 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 5335 | `		if( nOfft < 0 ){` |
|    ! 0 | 5336 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5337 | `			if( zBase > zString ){` |
|    ! 0 | 5338 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5339 | `				zString = zBase;` |
|    ! 0 | 5340 | `			}else{` |
|      - | 5341 | `				/* Invalid offset */` |
|    ! 0 | 5342 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5343 | `				return PH7_OK;` |
|      - | 5344 | `			}` |
|    ! 0 | 5345 | `		}else{` |
|      9 | 5346 | `			if( nOfft >= iLen ){` |
|      - | 5347 | `				/* Invalid offset */` |
|    ! 0 | 5348 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5349 | `				return PH7_OK;` |
|    ! 0 | 5350 | `			}else{` |
|      - | 5351 | `				/* Update offset */` |
|      9 | 5352 | `				zString += nOfft;` |
|      9 | 5353 | `				iLen -= nOfft;` |
|      - | 5354 | `			}` |
|      - | 5355 | `		}` |
|      9 | 5356 | `		if( nArg > 3 ){` |
|      - | 5357 | `			int iUserlen;` |
|      - | 5358 | `			/* Extract the desired length */` |
|      9 | 5359 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 5360 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 5361 | `				iLen = iUserlen;` |
|      2 | 5362 | `			}` |
|      4 | 5363 | `		}` |
|      4 | 5364 | `	}` |
|      - | 5365 | `	/* Point to the end of the string */` |
|     19 | 5366 | `	zEnd = &zString[iLen];` |
|      - | 5367 | `	/* Extract the first non-space token */` |
|     19 | 5368 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 5369 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5370 | `		/* Compare against the current mask */` |
|     19 | 5371 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 5372 | `	}` |
|      - | 5373 | `	/* Longest match */` |
|     19 | 5374 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 5375 | `	return PH7_OK;` |
|     14 | 5376 |  |
|      - | 5377 | `/*` |
|      - | 5378 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5379 | ` *  Find length of initial segment not matching mask.` |
|      - | 5380 | ` * Parameters` |
|      - | 5381 | ` * $str` |
|      - | 5382 | ` *  The input string.` |
|      - | 5383 | ` * $mask` |
|      - | 5384 | ` *  The list of not allowed characters.` |
|      - | 5385 | ` * $start` |
|      - | 5386 | ` *  The position in subject to start searching.` |
|      - | 5387 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5388 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5389 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5390 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5391 | ` *  start'th position from the end of subject.` |
|      - | 5392 | ` * $length` |
|      - | 5393 | ` *  The length of the segment from subject to examine.` |
|      - | 5394 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5395 | ` *  characters after the starting position.` |
|      - | 5396 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5397 | ` *  position up to length characters from the end of subject.` |
|      - | 5398 | ` * Return` |
|      - | 5399 | ` *  Returns the length of the segment as an integer.` |
|      - | 5400 | ` */` |
|     16 | 5401 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5402 |  |
|      - | 5403 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5404 | `	int iMasklen,iLen;` |
|      - | 5405 | `	SyString sToken;` |
|     17 | 5406 | `	int iCount = 0;` |
|      - | 5407 | `	int rc;` |
|     17 | 5408 | `	if( nArg < 2 ){` |
|      - | 5409 | `		/* Missing agruments,return zero */` |
|      3 | 5410 | `		ph7_result_int(pCtx,0);` |
|      3 | 5411 | `		return PH7_OK;` |
|      - | 5412 | `	}` |
|      - | 5413 | `	/* Extract the target string */` |
|     15 | 5414 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5415 | `	/* Extract the mask */` |
|     15 | 5416 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 5417 | `	if( iLen < 1 ){` |
|      - | 5418 | `		/* Nothing to process,return zero */` |
|    ! 0 | 5419 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5420 | `		return PH7_OK;` |
|      - | 5421 | `	}` |
|     15 | 5422 | `	if( iMasklen < 1 ){` |
|      - | 5423 | `		/* No given mask,return the string length */` |
|      3 | 5424 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 5425 | `		return PH7_OK;` |
|      - | 5426 | `	}` |
|     13 | 5427 | `	if( nArg > 2 ){` |
|      - | 5428 | `		int nOfft;` |
|      - | 5429 | `		/* Extract the offset */` |
|     11 | 5430 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 5431 | `		if( nOfft < 0 ){` |
|    ! 0 | 5432 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5433 | `			if( zBase > zString ){` |
|    ! 0 | 5434 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5435 | `				zString = zBase;` |
|    ! 0 | 5436 | `			}else{` |
|      - | 5437 | `				/* Invalid offset */` |
|    ! 0 | 5438 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5439 | `				return PH7_OK;` |
|      - | 5440 | `			}` |
|    ! 0 | 5441 | `		}else{` |
|     11 | 5442 | `			if( nOfft >= iLen ){` |
|      - | 5443 | `				/* Invalid offset */` |
|      3 | 5444 | `				ph7_result_int(pCtx,0);` |
|      3 | 5445 | `				return PH7_OK;` |
|    ! 0 | 5446 | `			}else{` |
|      - | 5447 | `				/* Update offset */` |
|      9 | 5448 | `				zString += nOfft;` |
|      9 | 5449 | `				iLen -= nOfft;` |
|      - | 5450 | `			}` |
|      - | 5451 | `		}` |
|      9 | 5452 | `		if( nArg > 3 ){` |
|      - | 5453 | `			int iUserlen;` |
|      - | 5454 | `			/* Extract the desired length */` |
|    ! 0 | 5455 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 5456 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 5457 | `				iLen = iUserlen;` |
|    ! 0 | 5458 | `			}` |
|    ! 0 | 5459 | `		}` |
|      4 | 5460 | `	}` |
|      - | 5461 | `	/* Point to the end of the string */` |
|     11 | 5462 | `	zEnd = &zString[iLen];` |
|      - | 5463 | `	/* Extract the first non-space token */` |
|     11 | 5464 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 5465 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5466 | `		/* Compare against the current mask */` |
|     11 | 5467 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 5468 | `	}` |
|      - | 5469 | `	/* Longest match */` |
|     11 | 5470 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 5471 | `	return PH7_OK;` |
|      9 | 5472 |  |
|      - | 5473 | `/*` |
|      - | 5474 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 5475 | ` *  Search a string for any of a set of characters.` |
|      - | 5476 | ` * Parameters` |
|      - | 5477 | ` *  $haystack` |
|      - | 5478 | ` *   The string where char_list is looked for.` |
|      - | 5479 | ` *  $char_list` |
|      - | 5480 | ` *   This parameter is case sensitive.` |
|      - | 5481 | ` * Return` |
|      - | 5482 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 5483 | ` */` |
|      6 | 5484 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5485 |  |
|      - | 5486 | `	const char *zString,*zList,*zEnd;` |
|      - | 5487 | `	int iLen,iListLen,i,c;` |
|      - | 5488 | `	sxu32 nOfft,nMax;` |
|      - | 5489 | `	sxi32 rc;` |
|      7 | 5490 | `	if( nArg < 2 ){` |
|      - | 5491 | `		/* Missing arguments,return FALSE */` |
|      3 | 5492 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5493 | `		return PH7_OK;` |
|      - | 5494 | `	}` |
|      - | 5495 | `	/* Extract the haystack and the char list */` |
|      5 | 5496 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 5497 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 5498 | `	if( iLen < 1 ){` |
|      - | 5499 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 5500 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5501 | `		return PH7_OK;` |
|      - | 5502 | `	}` |
|      - | 5503 | `	/* Point to the end of the string */` |
|      5 | 5504 | `	zEnd = &zString[iLen];` |
|      5 | 5505 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 5506 | `	/* perform the requested operation */` |
|     15 | 5507 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 5508 | `		c = zList[i];` |
|     11 | 5509 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 5510 | `		if( rc == SXRET_OK ){` |
|      5 | 5511 | `			if( nMax < nOfft ){` |
|      3 | 5512 | `				nOfft = nMax;` |
|      1 | 5513 | `			}` |
|      2 | 5514 | `		}` |
|      6 | 5515 | `	}` |
|      5 | 5516 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 5517 | `		/* No such substring,return FALSE */` |
|      3 | 5518 | `		ph7_result_bool(pCtx,0);` |
|      2 | 5519 | `	}else{` |
|      - | 5520 | `		/* Return the substring */` |
|      3 | 5521 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 5522 | `	}` |
|      5 | 5523 | `	return PH7_OK;` |
|      4 | 5524 |  |
|      - | 5525 | `/*` |
|      - | 5526 | ` * string soundex(string $str)` |
|      - | 5527 | ` *  Calculate the soundex key of a string.` |
|      - | 5528 | ` * Parameters` |
|      - | 5529 | ` *  $str` |
|      - | 5530 | ` *   The input string.` |
|      - | 5531 | ` * Return` |
|      - | 5532 | ` *  Returns the soundex key as a string.` |
|      - | 5533 | ` * Note:` |
|      - | 5534 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 5535 | ` * source tree.` |
|      - | 5536 | ` */` |
|     20 | 5537 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5538 |  |
|      - | 5539 | `	const unsigned char *zIn;` |
|      - | 5540 | `	char zResult[8];` |
|      - | 5541 | `	int i, j;` |
|      - | 5542 | `	static const unsigned char iCode[] = {` |
|      - | 5543 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 5544 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 5545 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 5546 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 5547 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 5548 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5549 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 5550 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5551 | `	};` |
|     21 | 5552 | `	if( nArg < 1 ){` |
|      - | 5553 | `		/* Missing arguments,return the empty string */` |
|      3 | 5554 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5555 | `		return PH7_OK;` |
|      - | 5556 | `	}` |
|     19 | 5557 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     19 | 5558 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     19 | 5559 | `	if( zIn[i] ){` |
|     17 | 5560 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 5561 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 5562 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 5563 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 5564 | `			if( code>0 ){` |
|     45 | 5565 | `				if( code!=prevcode ){` |
|     33 | 5566 | `					prevcode = (unsigned char)code;` |
|     33 | 5567 | `					zResult[j++] = (char)code + '0';` |
|     16 | 5568 | `				}` |
|     23 | 5569 | `			}else{` |
|     49 | 5570 | `				prevcode = 0;` |
|      - | 5571 | `			}` |
|     47 | 5572 | `		}` |
|     33 | 5573 | `		while( j<4 ){` |
|     17 | 5574 | `			zResult[j++] = '0';` |
|      1 | 5575 | `		}` |
|     17 | 5576 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 5577 | `	}else{` |
|      3 | 5578 | `	  ph7_result_string(pCtx,"?000",4);` |
|      - | 5579 | `	}` |
|     19 | 5580 | `	return PH7_OK;` |
|     11 | 5581 |  |
|      - | 5582 | `/*` |
|      - | 5583 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 5584 | ` *  Wraps a string to a given number of characters.` |
|      - | 5585 | ` * Parameters` |
|      - | 5586 | ` *  $str` |
|      - | 5587 | ` *   The input string.` |
|      - | 5588 | ` * $width` |
|      - | 5589 | ` *  The column width.` |
|      - | 5590 | ` * $break` |
|      - | 5591 | ` *  The line is broken using the optional break parameter.` |
|      - | 5592 | ` * Return` |
|      - | 5593 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 5594 | ` */` |
|     14 | 5595 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5596 |  |
|      - | 5597 | `	const char *zIn,*zEnd,*zBreak;` |
|      - | 5598 | `	int iLen,iBreaklen,iChunk;` |
|     15 | 5599 | `	if( nArg < 1 ){` |
|      - | 5600 | `		/* Missing arguments,return the empty string */` |
|      3 | 5601 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5602 | `		return PH7_OK;` |
|      - | 5603 | `	}` |
|      - | 5604 | `	/* Extract the input string */` |
|     13 | 5605 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|     13 | 5606 | `	if( iLen < 1 ){` |
|      - | 5607 | `		/* Nothing to process,return the empty string */` |
|      3 | 5608 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5609 | `		return PH7_OK;` |
|      - | 5610 | `	}` |
|      - | 5611 | `	/* Chunk length */` |
|     11 | 5612 | `	iChunk = 75;` |
|     11 | 5613 | `	iBreaklen = 0;` |
|     11 | 5614 | `	zBreak = ""; /* cc warning */` |
|     11 | 5615 | `	if( nArg > 1 ){` |
|     11 | 5616 | `		iChunk = ph7_value_to_int(apArg[1]);` |
|     11 | 5617 | `		if( iChunk < 1 ){` |
|    ! 0 | 5618 | `			iChunk = 75;` |
|    ! 0 | 5619 | `		}` |
|     11 | 5620 | `		if( nArg > 2 ){` |
|      3 | 5621 | `			zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      1 | 5622 | `		}` |
|      5 | 5623 | `	}` |
|     11 | 5624 | `	if( iBreaklen < 1 ){` |
|      - | 5625 | `		/* Set a default column break */` |
|      - | 5626 | `#ifdef __WINNT__` |
|      1 | 5627 | `		zBreak = "\r\n";` |
|      1 | 5628 | `		iBreaklen = (int)sizeof("\r\n")-1;` |
|      - | 5629 | `#else` |
|      8 | 5630 | `		zBreak = "\n";` |
|      8 | 5631 | `		iBreaklen = (int)sizeof(char);` |
|      - | 5632 | `#endif` |
|      4 | 5633 | `	}` |
|      - | 5634 | `	/* Perform the requested operation */` |
|     11 | 5635 | `	zEnd = &zIn[iLen];` |
|     41 | 5636 | `	for(;;){` |
|      - | 5637 | `		int nMax;` |
|     47 | 5638 | `		if( zIn >= zEnd ){` |
|      - | 5639 | `			/* No more input to process */` |
|     11 | 5640 | `			break;` |
|      - | 5641 | `		}` |
|     37 | 5642 | `		nMax = (int)(zEnd-zIn);` |
|     37 | 5643 | `		if( iChunk > nMax ){` |
|     11 | 5644 | `			iChunk = nMax;` |
|      5 | 5645 | `		}` |
|      - | 5646 | `		/* Append the column first */` |
|     37 | 5647 | `		ph7_result_string(pCtx,zIn,iChunk); /* Will make it's own copy */` |
|      - | 5648 | `		/* Advance the cursor */` |
|     37 | 5649 | `		zIn += iChunk;` |
|     37 | 5650 | `		if( zIn < zEnd ){` |
|      - | 5651 | `			/* Append the line break */` |
|     27 | 5652 | `			ph7_result_string(pCtx,zBreak,iBreaklen);` |
|     13 | 5653 | `		}` |
|      1 | 5654 | `	}` |
|     11 | 5655 | `	return PH7_OK;` |
|      8 | 5656 |  |
|      - | 5657 | `/*` |
|      - | 5658 | ` * Check if the given character is a member of the given mask.` |
|      - | 5659 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 5660 | ` * Refer to [strtok()].` |
|      - | 5661 | ` */` |
|     30 | 5662 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 5663 |  |
|      - | 5664 | `	int i;` |
|     57 | 5665 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 5666 | `		if( c == zMask[i] ){` |
|     13 | 5667 | `			if( pOfft ){` |
|      5 | 5668 | `				*pOfft = i;` |
|      2 | 5669 | `			}` |
|     13 | 5670 | `			return TRUE;` |
|      - | 5671 | `		}` |
|     14 | 5672 | `	}` |
|     19 | 5673 | `	return FALSE;` |
|     16 | 5674 |  |
|      - | 5675 | `/*` |
|      - | 5676 | ` * Extract a single token from the input stream.` |
|      - | 5677 | ` * Refer to [strtok()].` |
|      - | 5678 | ` */` |
|      6 | 5679 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 5680 |  |
|      7 | 5681 | `	const char *zIn = *pzIn;` |
|      - | 5682 | `	const char *zPtr;` |
|      - | 5683 | `	/* Ignore leading delimiter */` |
|     11 | 5684 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5685 | `		zIn++;` |
|      1 | 5686 | `	}` |
|      7 | 5687 | `	if( zIn >= zEnd ){` |
|      - | 5688 | `		/* End of input */` |
|    ! 0 | 5689 | `		return SXERR_EOF;` |
|      - | 5690 | `	}` |
|      7 | 5691 | `	zPtr = zIn;` |
|      - | 5692 | `	/* Extract the token */` |
|     13 | 5693 | `	while( zIn < zEnd ){` |
|     11 | 5694 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 5695 | `			/* UTF-8 stream */` |
|    ! 0 | 5696 | `			zIn++;` |
|    ! 0 | 5697 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 5698 | `		}else{` |
|     11 | 5699 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5700 | `				break;` |
|      - | 5701 | `			}` |
|      7 | 5702 | `			zIn++;` |
|      - | 5703 | `		}` |
|      1 | 5704 | `	}` |
|      7 | 5705 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5706 | `	/* Update the cursor */` |
|      7 | 5707 | `	*pzIn = zIn;` |
|      - | 5708 | `	/* Return to the caller */` |
|      7 | 5709 | `	return SXRET_OK;` |
|      4 | 5710 |  |
|      - | 5711 | `/* strtok auxiliary private data */` |
|      - | 5712 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 5713 | `struct strtok_aux_data` |
|      - | 5714 |  |
|      - | 5715 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 5716 | `	const char *zIn;   /* Current input stream */` |
|      - | 5717 | `	const char *zEnd;  /* End of input */` |
|      - | 5718 | `};` |
|      - | 5719 | `/*` |
|      - | 5720 | ` * string strtok(string $str,string $token)` |
|      - | 5721 | ` * string strtok(string $token)` |
|      - | 5722 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 5723 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 5724 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 5725 | ` *  words by using the space character as the token.` |
|      - | 5726 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 5727 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 5728 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 5729 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 5730 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 5731 | ` *  the argument are found.` |
|      - | 5732 | ` * Parameters` |
|      - | 5733 | ` *  $str` |
|      - | 5734 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 5735 | ` * $token` |
|      - | 5736 | ` *  The delimiter used when splitting up str.` |
|      - | 5737 | ` * Return` |
|      - | 5738 | ` *   Current token or FALSE on EOF.` |
|      - | 5739 | ` */` |
|      8 | 5740 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5741 |  |
|      - | 5742 | `	strtok_aux_data *pAux;` |
|      - | 5743 | `	const char *zMask;` |
|      - | 5744 | `	SyString sToken;` |
|      - | 5745 | `	int nMasklen;` |
|      - | 5746 | `	sxi32 rc;` |
|      9 | 5747 | `	if( nArg < 2 ){` |
|      - | 5748 | `		/* Extract top aux data */` |
|      7 | 5749 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      7 | 5750 | `		if( pAux == 0 ){` |
|      - | 5751 | `			/* No aux data,return FALSE */` |
|    ! 0 | 5752 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5753 | `			return PH7_OK;` |
|      - | 5754 | `		}` |
|      7 | 5755 | `		nMasklen = 0;` |
|      7 | 5756 | `		zMask = ""; /* cc warning */` |
|      7 | 5757 | `		if( nArg > 0 ){` |
|      - | 5758 | `			/* Extract the mask */` |
|      5 | 5759 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 5760 | `		}` |
|      7 | 5761 | `		if( nMasklen < 1 ){` |
|      - | 5762 | `			/* Invalid mask,return FALSE */` |
|      3 | 5763 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|      3 | 5764 | `			ph7_context_free_chunk(pCtx,pAux);` |
|      3 | 5765 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|      3 | 5766 | `			ph7_result_bool(pCtx,0);` |
|      3 | 5767 | `			return PH7_OK;` |
|      - | 5768 | `		}` |
|      - | 5769 | `		/* Extract the token */` |
|      5 | 5770 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 5771 | `		if( rc != SXRET_OK ){` |
|      - | 5772 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 5773 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 5774 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5775 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 5776 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5777 | `		}else{` |
|      - | 5778 | `			/* Return the extracted token */` |
|      5 | 5779 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 5780 | `		}` |
|      3 | 5781 | `	}else{` |
|      - | 5782 | `		const char *zInput,*zCur;` |
|      - | 5783 | `		char *zDup;` |
|      - | 5784 | `		int nLen;` |
|      - | 5785 | `		/* Extract the raw input */` |
|      3 | 5786 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5787 | `		if( nLen < 1 ){` |
|      - | 5788 | `			/* Empty input,return FALSE */` |
|    ! 0 | 5789 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5790 | `			return PH7_OK;` |
|      - | 5791 | `		}` |
|      - | 5792 | `		/* Extract the mask */` |
|      3 | 5793 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 5794 | `		if( nMasklen < 1 ){` |
|      - | 5795 | `			/* Set a default mask */` |
|      - | 5796 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 5797 | `			zMask = TOK_MASK;` |
|    ! 0 | 5798 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 5799 | `#undef TOK_MASK` |
|    ! 0 | 5800 | `		}` |
|      - | 5801 | `		/* Extract a single token */` |
|      3 | 5802 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 5803 | `		if( rc != SXRET_OK ){` |
|      - | 5804 | `			/* Empty input */` |
|    ! 0 | 5805 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5806 | `			return PH7_OK;` |
|    ! 0 | 5807 | `		}else{` |
|      - | 5808 | `			/* Return the extracted token */` |
|      3 | 5809 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 5810 | `		}` |
|      - | 5811 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 5812 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 5813 | `		if( pAux ){` |
|      3 | 5814 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 5815 | `			if( nLen < 1 ){` |
|    ! 0 | 5816 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5817 | `				return PH7_OK;` |
|      - | 5818 | `			}` |
|      - | 5819 | `			/* Duplicate input */` |
|      3 | 5820 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 5821 | `			if( zDup  ){` |
|      3 | 5822 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 5823 | `				/* Register the aux data */` |
|      3 | 5824 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 5825 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 5826 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 5827 | `			}` |
|      1 | 5828 | `		}` |
|      - | 5829 | `	}` |
|      7 | 5830 | `	return PH7_OK;` |
|      5 | 5831 |  |
|      - | 5832 | `/*` |
|      - | 5833 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 5834 | ` *  Pad a string to a certain length with another string` |
|      - | 5835 | ` * Parameters` |
|      - | 5836 | ` *  $input` |
|      - | 5837 | ` *   The input string.` |
|      - | 5838 | ` * $pad_length` |
|      - | 5839 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 5840 | ` *   string, no padding takes place.` |
|      - | 5841 | ` * $pad_string` |
|      - | 5842 | ` *   Note:` |
|      - | 5843 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 5844 | ` *    divided by the pad_string's length.` |
|      - | 5845 | ` * $pad_type` |
|      - | 5846 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 5847 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 5848 | ` * Return` |
|      - | 5849 | ` *  The padded string.` |
|      - | 5850 | ` */` |
|     10 | 5851 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5852 |  |
|      - | 5853 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 5854 | `	const char *zIn,*zPad;` |
|     11 | 5855 | `	if( nArg < 2 ){` |
|      - | 5856 | `		/* Missing arguments,return the empty string */` |
|      5 | 5857 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 5858 | `		return PH7_OK;` |
|      - | 5859 | `	}` |
|      - | 5860 | `	/* Extract the target string */` |
|      7 | 5861 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5862 | `	/* Padding length */` |
|      7 | 5863 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|      7 | 5864 | `	if( iPadlen > 0 ){` |
|      5 | 5865 | `		iPadlen -= iLen;` |
|      2 | 5866 | `	}` |
|      7 | 5867 | `	if( iPadlen < 1  ){` |
|      - | 5868 | `		/* Return the string verbatim */` |
|      3 | 5869 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      3 | 5870 | `		return PH7_OK;` |
|      - | 5871 | `	}` |
|      5 | 5872 | `	zPad = " "; /* Whitespace padding */` |
|      5 | 5873 | `	iStrpad = (int)sizeof(char);` |
|      5 | 5874 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      5 | 5875 | `	if( nArg > 2 ){` |
|      - | 5876 | `		/* Padding string */` |
|      5 | 5877 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      5 | 5878 | `		if( iStrpad < 1 ){` |
|      - | 5879 | `			/* Empty string */` |
|    ! 0 | 5880 | `			zPad = " "; /* Whitespace padding */` |
|    ! 0 | 5881 | `			iStrpad = (int)sizeof(char);` |
|    ! 0 | 5882 | `		}` |
|      5 | 5883 | `		if( nArg > 3 ){` |
|      - | 5884 | `			/* Padd type */` |
|      5 | 5885 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 5886 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 5887 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 5888 | `			}` |
|      2 | 5889 | `		}` |
|      2 | 5890 | `	}` |
|      5 | 5891 | `	iDiv = 1;` |
|      5 | 5892 | `	if( iType == 2 ){` |
|    ! 0 | 5893 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 5894 | `	}` |
|      - | 5895 | `	/* Perform the requested operation */` |
|      5 | 5896 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 5897 | `		jPad = iStrpad;` |
|      5 | 5898 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 5899 | `			/* Padding */` |
|      5 | 5900 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 5901 | `				break;` |
|      - | 5902 | `			}` |
|      3 | 5903 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      2 | 5904 | `		}` |
|      3 | 5905 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 5906 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 5907 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 5908 | `				if( jPad > iStrpad ){` |
|    ! 0 | 5909 | `					jPad = iStrpad;` |
|    ! 0 | 5910 | `				}` |
|      3 | 5911 | `				if( jPad < 1){` |
|    ! 0 | 5912 | `					break;` |
|      - | 5913 | `				}` |
|      3 | 5914 | `				ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 5915 | `			}` |
|      1 | 5916 | `		}` |
|      1 | 5917 | `	}` |
|      5 | 5918 | `	if( iLen > 0 ){` |
|      - | 5919 | `		/* Append the input string */` |
|      5 | 5920 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      2 | 5921 | `	}` |
|      5 | 5922 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 5923 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 5924 | `			/* Padding */` |
|      5 | 5925 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 5926 | `				break;` |
|      - | 5927 | `			}` |
|      3 | 5928 | `			ph7_result_string(pCtx,zPad,iStrpad);` |
|      2 | 5929 | `		}` |
|      5 | 5930 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 5931 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 5932 | `			if( jPad > iStrpad ){` |
|    ! 0 | 5933 | `				jPad = iStrpad;` |
|    ! 0 | 5934 | `			}` |
|      3 | 5935 | `			if( jPad < 1){` |
|    ! 0 | 5936 | `				break;` |
|      - | 5937 | `			}` |
|      3 | 5938 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 5939 | `		}` |
|      1 | 5940 | `	}` |
|      5 | 5941 | `	return PH7_OK;` |
|      6 | 5942 |  |
|      - | 5943 | `/*` |
|      - | 5944 | ` * String replacement private data.` |
|      - | 5945 | ` */` |
|      - | 5946 | `typedef struct str_replace_data str_replace_data;` |
|      - | 5947 | `struct str_replace_data` |
|      - | 5948 |  |
|      - | 5949 | `	/* The following two fields are only used by the strtr function */` |
|      - | 5950 | `	SyBlob *pWorker;         /* Working buffer */` |
|      - | 5951 | `	ProcStringMatch xMatch;  /* Pattern match routine */` |
|      - | 5952 | `	/* The following two fields are only used by the str_replace function */` |
|      - | 5953 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 5954 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 5955 | `};` |
|      - | 5956 | `/*` |
|      - | 5957 | ` * Remove a substring.` |
|      - | 5958 | ` */` |
|      - | 5959 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 5960 | `	for(;;){\` |
|      - | 5961 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 5962 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 5963 | `		++OFFT;\` |
|      - | 5964 | `	}\` |
|      - | 5965 |  |
|      - | 5966 | `/*` |
|      - | 5967 | ` * Shift right and insert algorithm.` |
|      - | 5968 | ` */` |
|      - | 5969 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 5970 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 5971 | `		for(;;){\` |
|      - | 5972 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 5973 | `			if(INLEN < 1 ) { break; }\` |
|      - | 5974 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 5975 | `			--INLEN; \` |
|      - | 5976 | `		}\` |
|      - | 5977 | `		for(;;){\` |
|      - | 5978 | `				if(ELEN < 1) { break; }\` |
|      - | 5979 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 5980 | `				OFFT++;\` |
|      - | 5981 | `				ENTRY++;\` |
|      - | 5982 | `				--ELEN;\` |
|      - | 5983 | `		}\` |
|      - | 5984 |  |
|      - | 5985 | `/*` |
|      - | 5986 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 5987 | ` * replacement string [i.e: zReplace].` |
|      - | 5988 | ` */` |
|     38 | 5989 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 5990 |  |
|     39 | 5991 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 5992 | `	sxu32 n,m;` |
|     39 | 5993 | `	n = SyBlobLength(pWorker);` |
|     39 | 5994 | `	m = nOfft;` |
|      - | 5995 | `	/* Delete the old entry */` |
|    475 | 5996 | `	STRDEL(zInput,n,m,nLen);` |
|     39 | 5997 | `	SyBlobLength(pWorker) -= nLen;` |
|     39 | 5998 | `	if( nReplen > 0 ){` |
|     33 | 5999 | `		sxi32 iRep = nReplen;` |
|      - | 6000 | `		sxi32 rc;` |
|      - | 6001 | `		/*` |
|      - | 6002 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 6003 | `		 * string.` |
|      - | 6004 | `		 */` |
|     33 | 6005 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     33 | 6006 | `		if( rc != SXRET_OK ){` |
|      - | 6007 | `			/* Simply ignore any memory failure problem */` |
|    ! 0 | 6008 | `			return SXRET_OK;` |
|      - | 6009 | `		}` |
|      - | 6010 | `		/* Perform the insertion now */` |
|     33 | 6011 | `		zInput = (char *)SyBlobData(pWorker);` |
|     33 | 6012 | `		n = SyBlobLength(pWorker);` |
|    163 | 6013 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     33 | 6014 | `		SyBlobLength(pWorker) += nReplen;` |
|     16 | 6015 | `	}` |
|     39 | 6016 | `	return SXRET_OK;` |
|     20 | 6017 |  |
|      - | 6018 | `/*` |
|      - | 6019 | ` * String replacement walker callback.` |
|      - | 6020 | ` * The following callback is invoked for each array entry that hold` |
|      - | 6021 | ` * the replace string.` |
|      - | 6022 | ` * Refer to the strtr() implementation for more information.` |
|      - | 6023 | ` */` |
|      8 | 6024 | `static int StringReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6025 |  |
|      9 | 6026 | `	str_replace_data *pRepData = (str_replace_data *)pUserData;` |
|      - | 6027 | `	const char *zTarget,*zReplace;` |
|      - | 6028 | `	SyBlob *pWorker;` |
|      - | 6029 | `	int tLen,nLen;` |
|      - | 6030 | `	sxu32 nOfft;` |
|      - | 6031 | `	sxi32 rc;` |
|      - | 6032 | `	/* Point to the working buffer */` |
|      9 | 6033 | `	pWorker = pRepData->pWorker;` |
|      9 | 6034 | `	if( !ph7_value_is_string(pKey) ){` |
|      - | 6035 | `		/* Target and replace must be a string */` |
|      3 | 6036 | `		return PH7_OK;` |
|      - | 6037 | `	}` |
|      - | 6038 | `	/* Extract the target and the replace */` |
|      7 | 6039 | `	zTarget = ph7_value_to_string(pKey,&tLen);` |
|      7 | 6040 | `	if( tLen < 1 ){` |
|      - | 6041 | `		/* Empty target,return immediately */` |
|    ! 0 | 6042 | `		return PH7_OK;` |
|      - | 6043 | `	}` |
|      - | 6044 | `	/* Perform a pattern search */` |
|      7 | 6045 | `	rc = pRepData->xMatch(SyBlobData(pWorker),SyBlobLength(pWorker),(const void *)zTarget,(sxu32)tLen,&nOfft);` |
|      7 | 6046 | `	if( rc != SXRET_OK ){` |
|      - | 6047 | `		/* Pattern not found */` |
|    ! 0 | 6048 | `		return PH7_OK;` |
|      - | 6049 | `	}` |
|      - | 6050 | `	/* Extract the replace string */` |
|      7 | 6051 | `	zReplace = ph7_value_to_string(pData,&nLen);` |
|      - | 6052 | `	/* Perform the replace process */` |
|      7 | 6053 | `	StringReplace(pWorker,nOfft,tLen,zReplace,nLen);` |
|      - | 6054 | `	/* All done */` |
|      7 | 6055 | `	return PH7_OK;` |
|      5 | 6056 |  |
|      - | 6057 | `/*` |
|      - | 6058 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 6059 | ` * to collect search/replace string.` |
|      - | 6060 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 6061 | ` */` |
|     26 | 6062 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6063 |  |
|     27 | 6064 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 6065 | `	SyString sWorker;` |
|      - | 6066 | `	const char *zIn;` |
|      - | 6067 | `	int nByte;` |
|      - | 6068 | `	/* Extract a string representation of the given argument */` |
|     27 | 6069 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 6070 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 6071 | `	if( nByte > 0 ){` |
|      - | 6072 | `		char *zDup;` |
|      - | 6073 | `		/* Duplicate the chunk */` |
|     25 | 6074 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 6075 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 6076 | `			);` |
|     25 | 6077 | `		if( zDup == 0 ){` |
|      - | 6078 | `			/* Ignore any memory failure problem */` |
|    ! 0 | 6079 | `			ph7_context_throw_error(pRep->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 6080 | `			return PH7_OK;` |
|      - | 6081 | `		}` |
|     25 | 6082 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 6083 | `		/* Save the chunk */` |
|     25 | 6084 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 6085 | `	}` |
|      - | 6086 | `	/* Save for later processing */` |
|     27 | 6087 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 6088 | `	/* All done */` |
|     13 | 6089 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 6090 | `	return PH7_OK;` |
|     14 | 6091 |  |
|      - | 6092 | `/*` |
|      - | 6093 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6094 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6095 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 6096 | ` * Parameters` |
|      - | 6097 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 6098 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 6099 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 6100 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 6101 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 6102 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 6103 | ` * $search` |
|      - | 6104 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 6105 | ` *  to designate multiple needles.` |
|      - | 6106 | ` * $replace` |
|      - | 6107 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 6108 | ` *  to designate multiple replacements.` |
|      - | 6109 | ` * $subject` |
|      - | 6110 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 6111 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 6112 | ` *  of subject, and the return value is an array as well.` |
|      - | 6113 | ` * $count (Not used)` |
|      - | 6114 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 6115 | ` * Return` |
|      - | 6116 | ` * This function returns a string or an array with the replaced values.` |
|      - | 6117 | ` */` |
|  12034 | 6118 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6119 |  |
|      - | 6120 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 6121 | `	ProcStringMatch xMatch;` |
|      - | 6122 | `	const char *zIn,*zFunc;` |
|      - | 6123 | `	str_replace_data sRep;` |
|      - | 6124 | `	SyBlob sWorker;` |
|      - | 6125 | `	SySet sReplace;` |
|      - | 6126 | `	SySet sSearch;` |
|      - | 6127 | `	int rep_str;` |
|      - | 6128 | `	int nByte;` |
|      - | 6129 | `	sxi32 rc;` |
|  12036 | 6130 | `	if( nArg < 3 ){` |
|      - | 6131 | `		/* Missing/Invalid arguments,return null */` |
|      7 | 6132 | `		ph7_result_null(pCtx);` |
|      7 | 6133 | `		return PH7_OK;` |
|      - | 6134 | `	}` |
|      - | 6135 | `	/* Initialize fields */` |
|  12030 | 6136 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  12030 | 6137 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  12030 | 6138 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  12030 | 6139 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  12030 | 6140 | `	sRep.pCtx = pCtx;` |
|  12030 | 6141 | `	sRep.pCollector = &sSearch;` |
|  12030 | 6142 | `	rep_str = 0;` |
|      - | 6143 | `	/* Extract the subject */` |
|  12030 | 6144 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  12030 | 6145 | `	if( nByte < 1 ){` |
|      - | 6146 | `		/* Nothing to replace,return the empty string */` |
|     38 | 6147 | `		ph7_result_string(pCtx,"",0);` |
|     38 | 6148 | `		return PH7_OK;` |
|      - | 6149 | `	}` |
|      - | 6150 | `	/* Copy the subject */` |
|  11994 | 6151 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 6152 | `	/* Search string */` |
|  11994 | 6153 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 6154 | `		/* Collect search string */` |
|      9 | 6155 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 6156 | `	}else{` |
|      - | 6157 | `		/* Single pattern */` |
|  11986 | 6158 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  11986 | 6159 | `		if( nByte < 1 ){` |
|      - | 6160 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 6161 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 6162 | `			return PH7_OK;` |
|      - | 6163 | `		}` |
|  11982 | 6164 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6165 | `		/* Save for later processing */` |
|  11982 | 6166 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 6167 | `	}` |
|      - | 6168 | `	/* Replace string */` |
|  11990 | 6169 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 6170 | `		/* Collect replace string */` |
|      7 | 6171 | `		sRep.pCollector = &sReplace;` |
|      7 | 6172 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 6173 | `	}else{` |
|      - | 6174 | `		/* Single needle */` |
|  11984 | 6175 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  11984 | 6176 | `		rep_str = 1;` |
|  11984 | 6177 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6178 | `		/* Save for later processing */` |
|  11984 | 6179 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 6180 | `	}` |
|      - | 6181 | `	/* Reset loop cursors */` |
|  11990 | 6182 | `	SySetResetCursor(&sSearch);` |
|  11990 | 6183 | `	SySetResetCursor(&sReplace);` |
|  11990 | 6184 | `	pReplace = pSearch = 0; /* cc warning */` |
|  11990 | 6185 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 6186 | `	/* Extract function name */` |
|  11990 | 6187 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 6188 | `	/* Set the default pattern match routine */` |
|  11990 | 6189 | `	xMatch = SyBlobSearch;` |
|  11990 | 6190 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 6191 | `		/* Case insensitive pattern match */` |
|     11 | 6192 | `		xMatch = iPatternMatch;` |
|      5 | 6193 | `	}` |
|      - | 6194 | `	/* Start the replace process */` |
|  23986 | 6195 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 6196 | `		sxu32 nCount,nOfft;` |
|  11998 | 6197 | `		if( pSearch->nByte <  1 ){` |
|      - | 6198 | `			/* Empty string,ignore */` |
|      3 | 6199 | `			continue;` |
|      - | 6200 | `		}` |
|      - | 6201 | `		/* Extract the replace string */` |
|  11996 | 6202 | `		if( rep_str ){` |
|  11986 | 6203 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|   5994 | 6204 | `		}else{` |
|     11 | 6205 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 6206 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 6207 | `				 * An empty string is used for the rest of replacement values` |
|      - | 6208 | `				 */` |
|      3 | 6209 | `				pReplace = 0;` |
|      1 | 6210 | `			}` |
|      - | 6211 | `		}` |
|  11996 | 6212 | `		if( pReplace == 0 ){` |
|      - | 6213 | `			/* Use an empty string instead */` |
|      3 | 6214 | `			pReplace = &sTemp;` |
|      1 | 6215 | `		}` |
|  11996 | 6216 | `		nOfft = nCount = 0;` |
|   6013 | 6217 | `		for(;;){` |
|  12028 | 6218 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 6219 | `				break;` |
|      - | 6220 | `			}` |
|      - | 6221 | `			/* Perform a pattern lookup */` |
|  18023 | 6222 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  12014 | 6223 | `				pSearch->nByte,&nOfft);` |
|  12016 | 6224 | `			if( rc != SXRET_OK ){` |
|      - | 6225 | `				/* Pattern not found */` |
|  11984 | 6226 | `				break;` |
|      - | 6227 | `			}` |
|      - | 6228 | `			/* Perform the replace operation */` |
|     33 | 6229 | `			StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|      - | 6230 | `			/* Increment offset counter */` |
|     33 | 6231 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 6232 | `		}` |
|      2 | 6233 | `	}` |
|      - | 6234 | `	/* All done,clean-up the mess left behind */` |
|  11990 | 6235 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  11990 | 6236 | `	SySetRelease(&sSearch);` |
|  11990 | 6237 | `	SySetRelease(&sReplace);` |
|  11990 | 6238 | `	SyBlobRelease(&sWorker);` |
|  11990 | 6239 | `	return PH7_OK;` |
|   6019 | 6240 |  |
|      - | 6241 | `/*` |
|      - | 6242 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 6243 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 6244 | ` *  Translate characters or replace substrings.` |
|      - | 6245 | ` * Parameters` |
|      - | 6246 | ` *  $str` |
|      - | 6247 | ` *  The string being translated.` |
|      - | 6248 | ` * $from` |
|      - | 6249 | ` *  The string being translated to to.` |
|      - | 6250 | ` * $to` |
|      - | 6251 | ` *  The string replacing from.` |
|      - | 6252 | ` * $replace_pairs` |
|      - | 6253 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 6254 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 6255 | ` * Return` |
|      - | 6256 | ` *  The translated string.` |
|      - | 6257 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 6258 | ` */` |
|     12 | 6259 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6260 |  |
|      - | 6261 | `	const char *zIn;` |
|      - | 6262 | `	int nLen;` |
|     13 | 6263 | `	if( nArg < 1 ){` |
|      - | 6264 | `		/* Nothing to replace,return FALSE */` |
|      7 | 6265 | `		ph7_result_bool(pCtx,0);` |
|      7 | 6266 | `		return PH7_OK;` |
|      - | 6267 | `	}` |
|      7 | 6268 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 6269 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 6270 | `		/* Invalid arguments */` |
|    ! 0 | 6271 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6272 | `		return PH7_OK;` |
|      - | 6273 | `	}` |
|      9 | 6274 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 6275 | `		str_replace_data sRepData;` |
|      - | 6276 | `		SyBlob sWorker;` |
|      - | 6277 | `		/* Initilaize the working buffer */` |
|      5 | 6278 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|      - | 6279 | `		/* Copy raw string */` |
|      5 | 6280 | `		SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nLen);` |
|      - | 6281 | `		/* Init our replace data instance */` |
|      5 | 6282 | `		sRepData.pWorker = &sWorker;` |
|      5 | 6283 | `		sRepData.xMatch = SyBlobSearch;` |
|      - | 6284 | `		/* Iterate throw array entries and perform the replace operation.*/` |
|      5 | 6285 | `		ph7_array_walk(apArg[1],StringReplaceWalker,&sRepData);` |
|      - | 6286 | `		/* All done, return the result string */` |
|      7 | 6287 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|      4 | 6288 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 6289 | `		/* Clean-up */` |
|      5 | 6290 | `		SyBlobRelease(&sWorker);` |
|      3 | 6291 | `	}else{` |
|      - | 6292 | `		int i,flen,tlen,c,iOfft;` |
|      - | 6293 | `		const char *zFrom,*zTo;` |
|      3 | 6294 | `		if( nArg < 3 ){` |
|      - | 6295 | `			/* Nothing to replace */` |
|    ! 0 | 6296 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6297 | `			return PH7_OK;` |
|      - | 6298 | `		}` |
|      - | 6299 | `		/* Extract given arguments */` |
|      3 | 6300 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 6301 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 6302 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 6303 | `			/* Nothing to replace */` |
|    ! 0 | 6304 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6305 | `			return PH7_OK;` |
|      - | 6306 | `		}` |
|      - | 6307 | `		/* Start the replace process */` |
|     13 | 6308 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 6309 | `			c = zIn[i];` |
|     11 | 6310 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 6311 | `				if ( iOfft < tlen ){` |
|      5 | 6312 | `					c = zTo[iOfft];` |
|      2 | 6313 | `				}` |
|      2 | 6314 | `			}` |
|     11 | 6315 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 6316 |  |
|      6 | 6317 | `		}` |
|      - | 6318 | `	}` |
|      7 | 6319 | `	return PH7_OK;` |
|      7 | 6320 |  |
|      - | 6321 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6322 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6323 | `/*` |
|      - | 6324 | ` * Parse an INI string.` |
|      - | 6325 |  |
|      - | 6326 | ` * According to wikipedia` |
|      - | 6327 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 6328 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 6329 | ` *  Format` |
|      - | 6330 | `*    Properties` |
|      - | 6331 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 6332 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 6333 | `*     Example:` |
|      - | 6334 | `*      name=value` |
|      - | 6335 | `*    Sections` |
|      - | 6336 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 6337 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 6338 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 6339 | `*     or the end of the file. Sections may not be nested.` |
|      - | 6340 | `*     Example:` |
|      - | 6341 | `*      [section]` |
|      - | 6342 | `*   Comments` |
|      - | 6343 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 6344 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 6345 | `*/` |
|     10 | 6346 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 6347 |  |
|      - | 6348 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     11 | 6349 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 6350 | `	SyHashEntry *pEntry;` |
|      - | 6351 | `	SyString sEntry;` |
|      - | 6352 | `	SyHash sHash;` |
|      - | 6353 | `	int c;` |
|      - | 6354 | `	/* Create an empty array and worker variables */` |
|     11 | 6355 | `	pArray = ph7_context_new_array(pCtx);` |
|     11 | 6356 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     11 | 6357 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     11 | 6358 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 6359 | `		/* Out of memory */` |
|    ! 0 | 6360 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      - | 6361 | `		/* Return FALSE */` |
|    ! 0 | 6362 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6363 | `		return PH7_OK;` |
|      - | 6364 | `	}` |
|     11 | 6365 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     11 | 6366 | `	pCur = pArray;` |
|      - | 6367 | `	/* Start the parse process */` |
|     20 | 6368 | `	for(;;){` |
|      - | 6369 | `		/* Ignore leading white spaces */` |
|     67 | 6370 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 6371 | `			zIn++;` |
|      1 | 6372 | `		}` |
|     41 | 6373 | `		if( zIn >= zEnd ){` |
|      - | 6374 | `			/* No more input to process */` |
|     11 | 6375 | `			break;` |
|      - | 6376 | `		}` |
|     31 | 6377 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6378 | `			/* Comment til the end of line */` |
|    ! 0 | 6379 | `			zIn++;` |
|    ! 0 | 6380 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 6381 | `				zIn++;` |
|    ! 0 | 6382 | `			}` |
|    ! 0 | 6383 | `			continue;` |
|      - | 6384 | `		}` |
|      - | 6385 | `		/* Reset the string cursor of the working variable */` |
|     31 | 6386 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 6387 | `		if( zIn[0] == '[' ){` |
|      - | 6388 | `			/* Section: Extract the section name */` |
|      9 | 6389 | `			zIn++;` |
|      9 | 6390 | `			zCur = zIn;` |
|     73 | 6391 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 6392 | `				zIn++;` |
|      1 | 6393 | `			}` |
|      9 | 6394 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 6395 | `				/* Save the section name */` |
|      5 | 6396 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 6397 | `				SyStringFullTrim(&sEntry);` |
|      5 | 6398 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 6399 | `				if( sEntry.nByte > 0 ){` |
|      - | 6400 | `					/* Associate an array with the section */` |
|      5 | 6401 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 6402 | `					if( pSection ){` |
|      5 | 6403 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 6404 | `						pCur = pSection;` |
|      2 | 6405 | `					}` |
|      2 | 6406 | `				}` |
|      2 | 6407 | `			}` |
|      9 | 6408 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 6409 | `		}else{` |
|      - | 6410 | `			ph7_value *pOldCur;` |
|      - | 6411 | `			int is_array;` |
|      - | 6412 | `			int iLen;` |
|      - | 6413 | `			/* Properties */` |
|     23 | 6414 | `			is_array = 0;` |
|     23 | 6415 | `			zCur = zIn;` |
|     23 | 6416 | `			iLen = 0; /* cc warning */` |
|     23 | 6417 | `			pOldCur = pCur;` |
|    155 | 6418 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 6419 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 6420 | `					/* Array */` |
|    ! 0 | 6421 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 6422 | `					is_array = 1;` |
|    ! 0 | 6423 | `					if( iLen > 0 ){` |
|    ! 0 | 6424 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 6425 | `						/* Query the hashtable */` |
|    ! 0 | 6426 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 6427 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 6428 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 6429 | `						if( pEntry ){` |
|    ! 0 | 6430 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 6431 | `						}else{` |
|      - | 6432 | `							/* Create an empty array */` |
|    ! 0 | 6433 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 6434 | `							if( pvArr ){` |
|      - | 6435 | `								/* Save the entry */` |
|    ! 0 | 6436 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 6437 | `								/* Insert the entry */` |
|    ! 0 | 6438 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6439 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 6440 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 6441 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6442 | `							}` |
|      - | 6443 | `						}` |
|    ! 0 | 6444 | `						if( pvArr ){` |
|    ! 0 | 6445 | `							pCur = pvArr;` |
|    ! 0 | 6446 | `						}` |
|    ! 0 | 6447 | `					}` |
|    ! 0 | 6448 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 6449 | `						zIn++;` |
|    ! 0 | 6450 | `					}` |
|    ! 0 | 6451 | `				}` |
|    133 | 6452 | `				zIn++;` |
|      1 | 6453 | `			}` |
|     23 | 6454 | `			if( !is_array ){` |
|     23 | 6455 | `				iLen = (int)(zIn-zCur);` |
|     11 | 6456 | `			}` |
|      - | 6457 | `			/* Trim the key */` |
|     23 | 6458 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 6459 | `			SyStringFullTrim(&sEntry);` |
|     23 | 6460 | `			if( sEntry.nByte > 0 ){` |
|     23 | 6461 | `				if( !is_array ){` |
|      - | 6462 | `					/* Save the key name */` |
|     23 | 6463 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 6464 | `				}` |
|      - | 6465 | `				/* extract key value */` |
|     23 | 6466 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 6467 | `				zIn++; /* '=' */` |
|     39 | 6468 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 6469 | `					zIn++;` |
|      1 | 6470 | `				}` |
|     23 | 6471 | `				if( zIn < zEnd ){` |
|     21 | 6472 | `					zCur = zIn;` |
|     21 | 6473 | `					c = zIn[0];` |
|     21 | 6474 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6475 | `						zIn++;` |
|      - | 6476 | `						/* Delimit the value */` |
|    ! 0 | 6477 | `						while( zIn < zEnd ){` |
|    ! 0 | 6478 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 6479 | `								break;` |
|      - | 6480 | `							}` |
|    ! 0 | 6481 | `							zIn++;` |
|    ! 0 | 6482 | `						}` |
|    ! 0 | 6483 | `						if( zIn < zEnd ){` |
|    ! 0 | 6484 | `							zIn++;` |
|    ! 0 | 6485 | `						}` |
|    ! 0 | 6486 | `					}else{` |
|    125 | 6487 | `						while( zIn < zEnd ){` |
|    123 | 6488 | `							if( zIn[0] == '\n' ){` |
|     19 | 6489 | `								if( zIn[-1] != '\\' ){` |
|     19 | 6490 | `									break;` |
|    ! 0 | 6491 | `								}` |
|    105 | 6492 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6493 | `								/* Inline comments */` |
|    ! 0 | 6494 | `								break;` |
|      - | 6495 | `							}` |
|    105 | 6496 | `							zIn++;` |
|      1 | 6497 | `						}` |
|      - | 6498 | `					}` |
|      - | 6499 | `					/* Trim the value */` |
|     21 | 6500 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 6501 | `					SyStringFullTrim(&sEntry);` |
|     21 | 6502 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6503 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 6504 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 6505 | `					}` |
|     21 | 6506 | `					if( sEntry.nByte > 0 ){` |
|     21 | 6507 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 6508 | `					}` |
|      - | 6509 | `					/* Insert the key and it's value */` |
|     21 | 6510 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 6511 | `				}` |
|     12 | 6512 | `			}else{` |
|    ! 0 | 6513 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 6514 | `					zIn++;` |
|    ! 0 | 6515 | `				}` |
|      - | 6516 | `			}` |
|     23 | 6517 | `			pCur = pOldCur;` |
|      - | 6518 | `		}` |
|      1 | 6519 | `	}` |
|     11 | 6520 | `	SyHashRelease(&sHash);` |
|      - | 6521 | `	/* Return the parse of the INI string */` |
|     11 | 6522 | `	ph7_result_value(pCtx,pArray);` |
|     11 | 6523 | `	return SXRET_OK;` |
|      6 | 6524 |  |
|      - | 6525 | `/*` |
|      - | 6526 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 6527 | ` *  Parse a configuration string.` |
|      - | 6528 | ` * Parameters` |
|      - | 6529 | ` *  $ini` |
|      - | 6530 | ` *   The contents of the ini file being parsed.` |
|      - | 6531 | ` *  $process_sections` |
|      - | 6532 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 6533 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 6534 | ` *  $scanner_mode (Not used)` |
|      - | 6535 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 6536 | ` *   then option values will not be parsed.` |
|      - | 6537 | ` * Return` |
|      - | 6538 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 6539 | ` */` |
|     10 | 6540 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6541 |  |
|      - | 6542 | `	const char *zIni;` |
|      - | 6543 | `	int nByte;` |
|     11 | 6544 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6545 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      3 | 6546 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6547 | `		return PH7_OK;` |
|      - | 6548 | `	}` |
|      - | 6549 | `	/* Extract the raw INI buffer */` |
|      9 | 6550 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 6551 | `	/* Process the INI buffer*/` |
|      9 | 6552 | `	PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      9 | 6553 | `	return PH7_OK;` |
|      6 | 6554 |  |
|      - | 6555 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6556 |  |
|      - | 6557 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6558 |  |
|      - | 6559 | `/*` |
|      - | 6560 | ` * Ctype Functions.` |
|      - | 6561 | ` * Status:` |
|      - | 6562 | ` *    Stable.` |
|      - | 6563 | ` */` |
|      - | 6564 | `/*` |
|      - | 6565 | ` * bool ctype_alnum(string $text)` |
|      - | 6566 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 6567 | ` * Parameters` |
|      - | 6568 | ` *  $text` |
|      - | 6569 | ` *   The tested string.` |
|      - | 6570 | ` * Return` |
|      - | 6571 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 6572 | ` */` |
|     16 | 6573 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6574 |  |
|      - | 6575 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6576 | `	int nLen;` |
|     17 | 6577 | `	if( nArg < 1 ){` |
|      - | 6578 | `		/* Missing arguments,return FALSE */` |
|      3 | 6579 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6580 | `		return PH7_OK;` |
|      - | 6581 | `	}` |
|      - | 6582 | `	/* Extract the target string */` |
|     15 | 6583 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 6584 | `	zEnd = &zIn[nLen];` |
|     15 | 6585 | `	if( nLen < 1 ){` |
|      - | 6586 | `		/* Empty string,return FALSE */` |
|      3 | 6587 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6588 | `		return PH7_OK;` |
|      - | 6589 | `	}` |
|      - | 6590 | `	/* Perform the requested operation */` |
|     32 | 6591 | `	for(;;){` |
|     65 | 6592 | `		if( zIn >= zEnd ){` |
|      - | 6593 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6594 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6595 | `			return PH7_OK;` |
|      - | 6596 | `		}` |
|     57 | 6597 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 6598 | `			break;` |
|      - | 6599 | `		}` |
|      - | 6600 | `		/* Point to the next character */` |
|     53 | 6601 | `		zIn++;` |
|      1 | 6602 | `	}` |
|      - | 6603 | `	/* The test failed,return FALSE */` |
|      5 | 6604 | `	ph7_result_bool(pCtx,0);` |
|      5 | 6605 | `	return PH7_OK;` |
|      9 | 6606 |  |
|      - | 6607 | `/*` |
|      - | 6608 | ` * bool ctype_alpha(string $text)` |
|      - | 6609 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 6610 | ` * Parameters` |
|      - | 6611 | ` *  $text` |
|      - | 6612 | ` *   The tested string.` |
|      - | 6613 | ` * Return` |
|      - | 6614 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 6615 | ` */` |
|     18 | 6616 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6617 |  |
|      - | 6618 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6619 | `	int nLen;` |
|     19 | 6620 | `	if( nArg < 1 ){` |
|      - | 6621 | `		/* Missing arguments,return FALSE */` |
|      3 | 6622 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6623 | `		return PH7_OK;` |
|      - | 6624 | `	}` |
|      - | 6625 | `	/* Extract the target string */` |
|     17 | 6626 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6627 | `	zEnd = &zIn[nLen];` |
|     17 | 6628 | `	if( nLen < 1 ){` |
|      - | 6629 | `		/* Empty string,return FALSE */` |
|      3 | 6630 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6631 | `		return PH7_OK;` |
|      - | 6632 | `	}` |
|      - | 6633 | `	/* Perform the requested operation */` |
|     42 | 6634 | `	for(;;){` |
|     85 | 6635 | `		if( zIn >= zEnd ){` |
|      - | 6636 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6637 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6638 | `			return PH7_OK;` |
|      - | 6639 | `		}` |
|     77 | 6640 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 6641 | `			break;` |
|      - | 6642 | `		}` |
|      - | 6643 | `		/* Point to the next character */` |
|     71 | 6644 | `		zIn++;` |
|      1 | 6645 | `	}` |
|      - | 6646 | `	/* The test failed,return FALSE */` |
|      7 | 6647 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6648 | `	return PH7_OK;` |
|     10 | 6649 |  |
|      - | 6650 | `/*` |
|      - | 6651 | ` * bool ctype_cntrl(string $text)` |
|      - | 6652 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 6653 | ` * Parameters` |
|      - | 6654 | ` *  $text` |
|      - | 6655 | ` *   The tested string.` |
|      - | 6656 | ` * Return` |
|      - | 6657 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 6658 | ` */` |
|     18 | 6659 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6660 |  |
|      - | 6661 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6662 | `	int nLen;` |
|     19 | 6663 | `	if( nArg < 1 ){` |
|      - | 6664 | `		/* Missing arguments,return FALSE */` |
|      3 | 6665 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6666 | `		return PH7_OK;` |
|      - | 6667 | `	}` |
|      - | 6668 | `	/* Extract the target string */` |
|     17 | 6669 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6670 | `	zEnd = &zIn[nLen];` |
|     17 | 6671 | `	if( nLen < 1 ){` |
|      - | 6672 | `		/* Empty string,return FALSE */` |
|      3 | 6673 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6674 | `		return PH7_OK;` |
|      - | 6675 | `	}` |
|      - | 6676 | `	/* Perform the requested operation */` |
|     14 | 6677 | `	for(;;){` |
|     29 | 6678 | `		if( zIn >= zEnd ){` |
|      - | 6679 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6680 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6681 | `			return PH7_OK;` |
|      - | 6682 | `		}` |
|     21 | 6683 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6684 | `			/* UTF-8 stream  */` |
|    ! 0 | 6685 | `			break;` |
|      - | 6686 | `		}` |
|     21 | 6687 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 6688 | `			break;` |
|      - | 6689 | `		}` |
|      - | 6690 | `		/* Point to the next character */` |
|     15 | 6691 | `		zIn++;` |
|      1 | 6692 | `	}` |
|      - | 6693 | `	/* The test failed,return FALSE */` |
|      7 | 6694 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6695 | `	return PH7_OK;` |
|     10 | 6696 |  |
|      - | 6697 | `/*` |
|      - | 6698 | ` * bool ctype_digit(string $text)` |
|      - | 6699 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 6700 | ` * Parameters` |
|      - | 6701 | ` *  $text` |
|      - | 6702 | ` *   The tested string.` |
|      - | 6703 | ` * Return` |
|      - | 6704 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 6705 | ` */` |
|   1470 | 6706 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6707 |  |
|      - | 6708 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6709 | `	int nLen;` |
|   1472 | 6710 | `	if( nArg < 1 ){` |
|      - | 6711 | `		/* Missing arguments,return FALSE */` |
|      3 | 6712 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6713 | `		return PH7_OK;` |
|      - | 6714 | `	}` |
|      - | 6715 | `	/* Extract the target string */` |
|   1470 | 6716 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1470 | 6717 | `	zEnd = &zIn[nLen];` |
|   1470 | 6718 | `	if( nLen < 1 ){` |
|      - | 6719 | `		/* Empty string,return FALSE */` |
|      3 | 6720 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6721 | `		return PH7_OK;` |
|      - | 6722 | `	}` |
|      - | 6723 | `	/* Perform the requested operation */` |
|   1379 | 6724 | `	for(;;){` |
|   2760 | 6725 | `		if( zIn >= zEnd ){` |
|      - | 6726 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1262 | 6727 | `			ph7_result_bool(pCtx,1);` |
|   1262 | 6728 | `			return PH7_OK;` |
|      - | 6729 | `		}` |
|   1500 | 6730 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6731 | `			/* UTF-8 stream  */` |
|    ! 0 | 6732 | `			break;` |
|      - | 6733 | `		}` |
|   1500 | 6734 | `		if( !SyisDigit(zIn[0]) ){` |
|    208 | 6735 | `			break;` |
|      - | 6736 | `		}` |
|      - | 6737 | `		/* Point to the next character */` |
|   1294 | 6738 | `		zIn++;` |
|      2 | 6739 | `	}` |
|      - | 6740 | `	/* The test failed,return FALSE */` |
|    208 | 6741 | `	ph7_result_bool(pCtx,0);` |
|    208 | 6742 | `	return PH7_OK;` |
|    737 | 6743 |  |
|      - | 6744 | `/*` |
|      - | 6745 | ` * bool ctype_xdigit(string $text)` |
|      - | 6746 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 6747 | ` * Parameters` |
|      - | 6748 | ` *  $text` |
|      - | 6749 | ` *   The tested string.` |
|      - | 6750 | ` * Return` |
|      - | 6751 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 6752 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 6753 | ` */` |
|     20 | 6754 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6755 |  |
|      - | 6756 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6757 | `	int nLen;` |
|     21 | 6758 | `	if( nArg < 1 ){` |
|      - | 6759 | `		/* Missing arguments,return FALSE */` |
|      3 | 6760 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6761 | `		return PH7_OK;` |
|      - | 6762 | `	}` |
|      - | 6763 | `	/* Extract the target string */` |
|     19 | 6764 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 6765 | `	zEnd = &zIn[nLen];` |
|     19 | 6766 | `	if( nLen < 1 ){` |
|      - | 6767 | `		/* Empty string,return FALSE */` |
|      3 | 6768 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6769 | `		return PH7_OK;` |
|      - | 6770 | `	}` |
|      - | 6771 | `	/* Perform the requested operation */` |
|     46 | 6772 | `	for(;;){` |
|     93 | 6773 | `		if( zIn >= zEnd ){` |
|      - | 6774 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 6775 | `			ph7_result_bool(pCtx,1);` |
|     11 | 6776 | `			return PH7_OK;` |
|      - | 6777 | `		}` |
|     83 | 6778 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6779 | `			/* UTF-8 stream  */` |
|    ! 0 | 6780 | `			break;` |
|      - | 6781 | `		}` |
|     83 | 6782 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 6783 | `			break;` |
|      - | 6784 | `		}` |
|      - | 6785 | `		/* Point to the next character */` |
|     77 | 6786 | `		zIn++;` |
|      1 | 6787 | `	}` |
|      - | 6788 | `	/* The test failed,return FALSE */` |
|      7 | 6789 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6790 | `	return PH7_OK;` |
|     11 | 6791 |  |
|      - | 6792 | `/*` |
|      - | 6793 | ` * bool ctype_graph(string $text)` |
|      - | 6794 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 6795 | ` * Parameters` |
|      - | 6796 | ` *  $text` |
|      - | 6797 | ` *   The tested string.` |
|      - | 6798 | ` * Return` |
|      - | 6799 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 6800 | ` * (no white space), FALSE otherwise.` |
|      - | 6801 | ` */` |
|     18 | 6802 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6803 |  |
|      - | 6804 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6805 | `	int nLen;` |
|     19 | 6806 | `	if( nArg < 1 ){` |
|      - | 6807 | `		/* Missing arguments,return FALSE */` |
|      3 | 6808 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6809 | `		return PH7_OK;` |
|      - | 6810 | `	}` |
|      - | 6811 | `	/* Extract the target string */` |
|     17 | 6812 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6813 | `	zEnd = &zIn[nLen];` |
|     17 | 6814 | `	if( nLen < 1 ){` |
|      - | 6815 | `		/* Empty string,return FALSE */` |
|      3 | 6816 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6817 | `		return PH7_OK;` |
|      - | 6818 | `	}` |
|      - | 6819 | `	/* Perform the requested operation */` |
|     57 | 6820 | `	for(;;){` |
|    115 | 6821 | `		if( zIn >= zEnd ){` |
|      - | 6822 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6823 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6824 | `			return PH7_OK;` |
|      - | 6825 | `		}` |
|    107 | 6826 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6827 | `			/* UTF-8 stream  */` |
|    ! 0 | 6828 | `			break;` |
|      - | 6829 | `		}` |
|    107 | 6830 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 6831 | `			break;` |
|      - | 6832 | `		}` |
|      - | 6833 | `		/* Point to the next character */` |
|    101 | 6834 | `		zIn++;` |
|      1 | 6835 | `	}` |
|      - | 6836 | `	/* The test failed,return FALSE */` |
|      7 | 6837 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6838 | `	return PH7_OK;` |
|     10 | 6839 |  |
|      - | 6840 | `/*` |
|      - | 6841 | ` * bool ctype_print(string $text)` |
|      - | 6842 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 6843 | ` * Parameters` |
|      - | 6844 | ` *  $text` |
|      - | 6845 | ` *   The tested string.` |
|      - | 6846 | ` * Return` |
|      - | 6847 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 6848 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 6849 | ` *  or control function at all.` |
|      - | 6850 | ` */` |
|     18 | 6851 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6852 |  |
|      - | 6853 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6854 | `	int nLen;` |
|     19 | 6855 | `	if( nArg < 1 ){` |
|      - | 6856 | `		/* Missing arguments,return FALSE */` |
|      3 | 6857 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6858 | `		return PH7_OK;` |
|      - | 6859 | `	}` |
|      - | 6860 | `	/* Extract the target string */` |
|     17 | 6861 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6862 | `	zEnd = &zIn[nLen];` |
|     17 | 6863 | `	if( nLen < 1 ){` |
|      - | 6864 | `		/* Empty string,return FALSE */` |
|      3 | 6865 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6866 | `		return PH7_OK;` |
|      - | 6867 | `	}` |
|      - | 6868 | `	/* Perform the requested operation */` |
|     63 | 6869 | `	for(;;){` |
|    127 | 6870 | `		if( zIn >= zEnd ){` |
|      - | 6871 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6872 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6873 | `			return PH7_OK;` |
|      - | 6874 | `		}` |
|    119 | 6875 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6876 | `			/* UTF-8 stream  */` |
|    ! 0 | 6877 | `			break;` |
|      - | 6878 | `		}` |
|    119 | 6879 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 6880 | `			break;` |
|      - | 6881 | `		}` |
|      - | 6882 | `		/* Point to the next character */` |
|    113 | 6883 | `		zIn++;` |
|      1 | 6884 | `	}` |
|      - | 6885 | `	/* The test failed,return FALSE */` |
|      7 | 6886 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6887 | `	return PH7_OK;` |
|     10 | 6888 |  |
|      - | 6889 | `/*` |
|      - | 6890 | ` * bool ctype_punct(string $text)` |
|      - | 6891 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 6892 | ` * Parameters` |
|      - | 6893 | ` *  $text` |
|      - | 6894 | ` *   The tested string.` |
|      - | 6895 | ` * Return` |
|      - | 6896 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 6897 | ` *  digit or blank, FALSE otherwise.` |
|      - | 6898 | ` */` |
|     20 | 6899 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6900 |  |
|      - | 6901 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6902 | `	int nLen;` |
|     21 | 6903 | `	if( nArg < 1 ){` |
|      - | 6904 | `		/* Missing arguments,return FALSE */` |
|      3 | 6905 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6906 | `		return PH7_OK;` |
|      - | 6907 | `	}` |
|      - | 6908 | `	/* Extract the target string */` |
|     19 | 6909 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 6910 | `	zEnd = &zIn[nLen];` |
|     19 | 6911 | `	if( nLen < 1 ){` |
|      - | 6912 | `		/* Empty string,return FALSE */` |
|      3 | 6913 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6914 | `		return PH7_OK;` |
|      - | 6915 | `	}` |
|      - | 6916 | `	/* Perform the requested operation */` |
|     38 | 6917 | `	for(;;){` |
|     77 | 6918 | `		if( zIn >= zEnd ){` |
|      - | 6919 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6920 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6921 | `			return PH7_OK;` |
|      - | 6922 | `		}` |
|     69 | 6923 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6924 | `			/* UTF-8 stream  */` |
|    ! 0 | 6925 | `			break;` |
|      - | 6926 | `		}` |
|     69 | 6927 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 6928 | `			break;` |
|      - | 6929 | `		}` |
|      - | 6930 | `		/* Point to the next character */` |
|     61 | 6931 | `		zIn++;` |
|      1 | 6932 | `	}` |
|      - | 6933 | `	/* The test failed,return FALSE */` |
|      9 | 6934 | `	ph7_result_bool(pCtx,0);` |
|      9 | 6935 | `	return PH7_OK;` |
|     11 | 6936 |  |
|      - | 6937 | `/*` |
|      - | 6938 | ` * bool ctype_space(string $text)` |
|      - | 6939 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 6940 | ` * Parameters` |
|      - | 6941 | ` *  $text` |
|      - | 6942 | ` *   The tested string.` |
|      - | 6943 | ` * Return` |
|      - | 6944 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 6945 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 6946 | ` *  and form feed characters.` |
|      - | 6947 | ` */` |
|  35056 | 6948 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6949 |  |
|      - | 6950 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6951 | `	int nLen;` |
|  35058 | 6952 | `	if( nArg < 1 ){` |
|      - | 6953 | `		/* Missing arguments,return FALSE */` |
|      3 | 6954 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6955 | `		return PH7_OK;` |
|      - | 6956 | `	}` |
|      - | 6957 | `	/* Extract the target string */` |
|  35056 | 6958 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  35056 | 6959 | `	zEnd = &zIn[nLen];` |
|  35056 | 6960 | `	if( nLen < 1 ){` |
|      - | 6961 | `		/* Empty string,return FALSE */` |
|      3 | 6962 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6963 | `		return PH7_OK;` |
|      - | 6964 | `	}` |
|      - | 6965 | `	/* Perform the requested operation */` |
|  17836 | 6966 | `	for(;;){` |
|  35630 | 6967 | `		if( zIn >= zEnd ){` |
|      - | 6968 | `			/* If we reach the end of the string,then the test succeeded. */` |
|    554 | 6969 | `			ph7_result_bool(pCtx,1);` |
|    554 | 6970 | `			return PH7_OK;` |
|      - | 6971 | `		}` |
|  35078 | 6972 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6973 | `			/* UTF-8 stream  */` |
|    ! 0 | 6974 | `			break;` |
|      - | 6975 | `		}` |
|  35078 | 6976 | `		if( !SyisSpace(zIn[0]) ){` |
|  34502 | 6977 | `			break;` |
|      - | 6978 | `		}` |
|      - | 6979 | `		/* Point to the next character */` |
|    578 | 6980 | `		zIn++;` |
|      2 | 6981 | `	}` |
|      - | 6982 | `	/* The test failed,return FALSE */` |
|  34502 | 6983 | `	ph7_result_bool(pCtx,0);` |
|  34502 | 6984 | `	return PH7_OK;` |
|  17552 | 6985 |  |
|      - | 6986 | `/*` |
|      - | 6987 | ` * bool ctype_lower(string $text)` |
|      - | 6988 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 6989 | ` * Parameters` |
|      - | 6990 | ` *  $text` |
|      - | 6991 | ` *   The tested string.` |
|      - | 6992 | ` * Return` |
|      - | 6993 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 6994 | ` */` |
|     18 | 6995 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6996 |  |
|      - | 6997 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6998 | `	int nLen;` |
|     19 | 6999 | `	if( nArg < 1 ){` |
|      - | 7000 | `		/* Missing arguments,return FALSE */` |
|      3 | 7001 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7002 | `		return PH7_OK;` |
|      - | 7003 | `	}` |
|      - | 7004 | `	/* Extract the target string */` |
|     17 | 7005 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7006 | `	zEnd = &zIn[nLen];` |
|     17 | 7007 | `	if( nLen < 1 ){` |
|      - | 7008 | `		/* Empty string,return FALSE */` |
|      3 | 7009 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7010 | `		return PH7_OK;` |
|      - | 7011 | `	}` |
|      - | 7012 | `	/* Perform the requested operation */` |
|     27 | 7013 | `	for(;;){` |
|     55 | 7014 | `		if( zIn >= zEnd ){` |
|      - | 7015 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7016 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7017 | `			return PH7_OK;` |
|      - | 7018 | `		}` |
|     51 | 7019 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 7020 | `			break;` |
|      - | 7021 | `		}` |
|      - | 7022 | `		/* Point to the next character */` |
|     41 | 7023 | `		zIn++;` |
|      1 | 7024 | `	}` |
|      - | 7025 | `	/* The test failed,return FALSE */` |
|     11 | 7026 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7027 | `	return PH7_OK;` |
|     10 | 7028 |  |
|      - | 7029 | `/*` |
|      - | 7030 | ` * bool ctype_upper(string $text)` |
|      - | 7031 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 7032 | ` * Parameters` |
|      - | 7033 | ` *  $text` |
|      - | 7034 | ` *   The tested string.` |
|      - | 7035 | ` * Return` |
|      - | 7036 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 7037 | ` */` |
|     18 | 7038 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7039 |  |
|      - | 7040 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7041 | `	int nLen;` |
|     19 | 7042 | `	if( nArg < 1 ){` |
|      - | 7043 | `		/* Missing arguments,return FALSE */` |
|      3 | 7044 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7045 | `		return PH7_OK;` |
|      - | 7046 | `	}` |
|      - | 7047 | `	/* Extract the target string */` |
|     17 | 7048 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7049 | `	zEnd = &zIn[nLen];` |
|     17 | 7050 | `	if( nLen < 1 ){` |
|      - | 7051 | `		/* Empty string,return FALSE */` |
|      3 | 7052 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7053 | `		return PH7_OK;` |
|      - | 7054 | `	}` |
|      - | 7055 | `	/* Perform the requested operation */` |
|     28 | 7056 | `	for(;;){` |
|     57 | 7057 | `		if( zIn >= zEnd ){` |
|      - | 7058 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7059 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7060 | `			return PH7_OK;` |
|      - | 7061 | `		}` |
|     53 | 7062 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 7063 | `			break;` |
|      - | 7064 | `		}` |
|      - | 7065 | `		/* Point to the next character */` |
|     43 | 7066 | `		zIn++;` |
|      1 | 7067 | `	}` |
|      - | 7068 | `	/* The test failed,return FALSE */` |
|     11 | 7069 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7070 | `	return PH7_OK;` |
|     10 | 7071 |  |
|      - | 7072 | `/*` |
|      - | 7073 | ` * Date/Time functions` |
|      - | 7074 | ` * Status:` |
|      - | 7075 | ` *    Devel.` |
|      - | 7076 | ` */` |
|      - | 7077 | `#include <time.h>` |
|      - | 7078 | `#ifdef __WINNT__` |
|      - | 7079 | `/* GetSystemTime() */` |
|      - | 7080 | `#include <Windows.h>` |
|      - | 7081 | `#ifdef _WIN32_WCE` |
|      - | 7082 | `/*` |
|      - | 7083 | `** WindowsCE does not have a localtime() function.  So create a` |
|      - | 7084 | `** substitute.` |
|      - | 7085 | `** Taken from the SQLite3 source tree.` |
|      - | 7086 | `** Status: Public domain` |
|      - | 7087 | `*/` |
|      - | 7088 | `struct tm *__cdecl localtime(const time_t *t)` |
|      - | 7089 |  |
|      - | 7090 | `  static struct tm y;` |
|      - | 7091 | `  FILETIME uTm, lTm;` |
|      - | 7092 | `  SYSTEMTIME pTm;` |
|      - | 7093 | `  ph7_int64 t64;` |
|      - | 7094 | `  t64 = *t;` |
|      - | 7095 | `  t64 = (t64 + 11644473600)*10000000;` |
|      - | 7096 | `  uTm.dwLowDateTime = (DWORD)(t64 & 0xFFFFFFFF);` |
|      - | 7097 | `  uTm.dwHighDateTime= (DWORD)(t64 >> 32);` |
|      - | 7098 | `  FileTimeToLocalFileTime(&uTm,&lTm);` |
|      - | 7099 | `  FileTimeToSystemTime(&lTm,&pTm);` |
|      - | 7100 | `  y.tm_year = pTm.wYear - 1900;` |
|      - | 7101 | `  y.tm_mon = pTm.wMonth - 1;` |
|      - | 7102 | `  y.tm_wday = pTm.wDayOfWeek;` |
|      - | 7103 | `  y.tm_mday = pTm.wDay;` |
|      - | 7104 | `  y.tm_hour = pTm.wHour;` |
|      - | 7105 | `  y.tm_min = pTm.wMinute;` |
|      - | 7106 | `  y.tm_sec = pTm.wSecond;` |
|      - | 7107 | `  return &y;` |
|      - | 7108 |  |
|      - | 7109 | `#endif /*_WIN32_WCE */` |
|      - | 7110 | `#elif defined(__UNIXES__)` |
|      - | 7111 | `#include <sys/time.h>` |
|      - | 7112 | `#endif /* __WINNT__*/` |
|      - | 7113 | ` /*` |
|      - | 7114 | `  * int64 time(void)` |
|      - | 7115 | `  *  Current Unix timestamp` |
|      - | 7116 | `  * Parameters` |
|      - | 7117 | `  *  None.` |
|      - | 7118 | `  * Return` |
|      - | 7119 | `  *  Returns the current time measured in the number of seconds` |
|      - | 7120 | `  *  since the Unix Epoch (January 1 1970 00:00:00 GMT).` |
|      - | 7121 | `  */` |
|      8 | 7122 | `static int PH7_builtin_time(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7123 |  |
|      - | 7124 | `	time_t tt;` |
|      4 | 7125 | `	SXUNUSED(nArg); /* cc warning */` |
|      4 | 7126 | `	SXUNUSED(apArg);` |
|      - | 7127 | `	/* Extract the current time */` |
|      9 | 7128 | `	time(&tt);` |
|      - | 7129 | `	/* Return as 64-bit integer */` |
|      9 | 7130 | `	ph7_result_int64(pCtx,(ph7_int64)tt);` |
|      9 | 7131 | `	return  PH7_OK;` |
|      1 | 7132 |  |
|      - | 7133 | `/*` |
|      - | 7134 | `  * string/float microtime([ bool $get_as_float = false ])` |
|      - | 7135 | `  *  microtime() returns the current Unix timestamp with microseconds.` |
|      - | 7136 | `  * Parameters` |
|      - | 7137 | `  *  $get_as_float` |
|      - | 7138 | `  *   If used and set to TRUE, microtime() will return a float instead of a string` |
|      - | 7139 | `  *   as described in the return values section below.` |
|      - | 7140 | `  * Return` |
|      - | 7141 | `  *  By default, microtime() returns a string in the form "msec sec", where sec` |
|      - | 7142 | `  *  is the current time measured in the number of seconds since the Unix` |
|      - | 7143 | `  *  epoch (0:00:00 January 1, 1970 GMT), and msec is the number of microseconds` |
|      - | 7144 | `  *  that have elapsed since sec expressed in seconds.` |
|      - | 7145 | `  *  If get_as_float is set to TRUE, then microtime() returns a float, which represents` |
|      - | 7146 | `  *  the current time in seconds since the Unix epoch accurate to the nearest microsecond.` |
|      - | 7147 | `  */` |
|     20 | 7148 | `static int PH7_builtin_microtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7149 |  |
|     21 | 7150 | `	int bFloat = 0;` |
|      - | 7151 | `	sytime sTime;` |
|      - | 7152 | `#if defined(__UNIXES__)` |
|      - | 7153 | `	struct timeval tv;` |
|     20 | 7154 | `	gettimeofday(&tv,0);` |
|     20 | 7155 | `	sTime.tm_sec  = (long)tv.tv_sec;` |
|     20 | 7156 | `	sTime.tm_usec = (long)tv.tv_usec;` |
|      - | 7157 | `#else` |
|      - | 7158 | `	time_t tt;` |
|      1 | 7159 | `	time(&tt);` |
|      1 | 7160 | `	sTime.tm_sec  = (long)tt;` |
|      1 | 7161 | `	sTime.tm_usec = (long)(tt%SX_USEC_PER_SEC);` |
|      - | 7162 | `#endif /* __UNIXES__ */` |
|     21 | 7163 | `	if( nArg > 0 ){` |
|     17 | 7164 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|      8 | 7165 | `	}` |
|     21 | 7166 | `	if( bFloat ){` |
|      - | 7167 | `		/* Return as float */` |
|     17 | 7168 | `		ph7_result_double(pCtx,(double)sTime.tm_sec);` |
|      9 | 7169 | `	}else{` |
|      - | 7170 | `		/* Return as string */` |
|      5 | 7171 | `		ph7_result_string_format(pCtx,"%ld %ld",sTime.tm_usec,sTime.tm_sec);` |
|      - | 7172 | `	}` |
|     21 | 7173 | `	return PH7_OK;` |
|      1 | 7174 |  |
|      - | 7175 | `/*` |
|      - | 7176 | ` * array getdate ([ int $timestamp = time() ])` |
|      - | 7177 | ` *  Get date/time information.` |
|      - | 7178 | ` * Parameter` |
|      - | 7179 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|      - | 7180 | ` *     that defaults to the current local time if a timestamp is not given.` |
|      - | 7181 | ` *     In other words, it defaults to the value of time().` |
|      - | 7182 | ` * Returns` |
|      - | 7183 | ` *  Returns an associative array of information related to the timestamp.` |
|      - | 7184 | ` *  Elements from the returned associative array are as follows:` |
|      - | 7185 | ` *   KEY                                                         VALUE` |
|      - | 7186 | ` * ---------                                                    -------` |
|      - | 7187 | ` * "seconds" 	Numeric representation of seconds 	            0 to 59` |
|      - | 7188 | ` * "minutes" 	Numeric representation of minutes 	            0 to 59` |
|      - | 7189 | ` * "hours" 	    Numeric representation of hours 	            0 to 23` |
|      - | 7190 | ` * "mday" 	    Numeric representation of the day of the month 	1 to 31` |
|      - | 7191 | ` * "wday" 	    Numeric representation of the day of the week 	0 (for Sunday) through 6 (for Saturday)` |
|      - | 7192 | ` * "mon" 	    Numeric representation of a month 	            1 through 12` |
|      - | 7193 | ` * "year" 	    A full numeric representation of a year,        4 digits 	Examples: 1999 or 2003` |
|      - | 7194 | ` * "yday" 	    Numeric representation of the day of the year   0 through 365` |
|      - | 7195 | ` * "weekday" 	A full textual representation of the day of the week 	Sunday through Saturday` |
|      - | 7196 | ` * "month" 	    A full textual representation of a month, such as January or March 	January through December` |
|      - | 7197 | ` * 0 	        Seconds since the Unix Epoch, similar to the values returned by time() and used by date().` |
|      - | 7198 | ` * NOTE:` |
|      - | 7199 | ` *   NULL is returned on failure.` |
|      - | 7200 | ` */` |
|      8 | 7201 | `static int PH7_builtin_getdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7202 |  |
|      - | 7203 | `	ph7_value *pValue,*pArray;` |
|      - | 7204 | `	Sytm sTm;` |
|      9 | 7205 | `	if( nArg < 1 ){` |
|      - | 7206 | `#ifdef __WINNT__` |
|      - | 7207 | `		SYSTEMTIME sOS;` |
|      1 | 7208 | `		GetSystemTime(&sOS);` |
|      1 | 7209 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 7210 | `#else` |
|      - | 7211 | `		struct tm *pTm;` |
|      - | 7212 | `		time_t t;` |
|      4 | 7213 | `		time(&t);` |
|      4 | 7214 | `		pTm = localtime(&t);` |
|      4 | 7215 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7216 | `#endif` |
|      3 | 7217 | `	}else{` |
|      - | 7218 | `		/* Use the given timestamp */` |
|      - | 7219 | `		time_t t;` |
|      - | 7220 | `		struct tm *pTm;` |
|      - | 7221 | `#ifdef __WINNT__` |
|      - | 7222 | `#ifdef _MSC_VER` |
|      - | 7223 | `#if _MSC_VER >= 1400 /* Visual Studio 2005 and up */` |
|      - | 7224 | `#pragma warning(disable:4996) /* _CRT_SECURE...*/` |
|      - | 7225 | `#endif` |
|      - | 7226 | `#endif` |
|      - | 7227 | `#endif` |
|      5 | 7228 | `		if( ph7_value_is_int(apArg[0]) ){` |
|      5 | 7229 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|      5 | 7230 | `			pTm = localtime(&t);` |
|      5 | 7231 | `			if( pTm == 0 ){` |
|    ! 0 | 7232 | `				time(&t);` |
|    ! 0 | 7233 | `			}` |
|      3 | 7234 | `		}else{` |
|    ! 0 | 7235 | `			time(&t);` |
|      - | 7236 | `		}` |
|      5 | 7237 | `		pTm = localtime(&t);` |
|      5 | 7238 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7239 | `	}` |
|      - | 7240 | `	/* Element value */` |
|      9 | 7241 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      9 | 7242 | `	if( pValue == 0 ){` |
|      - | 7243 | `		/* Return NULL */` |
|    ! 0 | 7244 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7245 | `		return PH7_OK;` |
|      - | 7246 | `	}` |
|      - | 7247 | `	/* Create a new array */` |
|      9 | 7248 | `	pArray = ph7_context_new_array(pCtx);` |
|      9 | 7249 | `	if( pArray == 0 ){` |
|      - | 7250 | `		/* Return NULL */` |
|    ! 0 | 7251 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7252 | `		return PH7_OK;` |
|      - | 7253 | `	}` |
|      - | 7254 | `	/* Fill the array */` |
|      - | 7255 | `	/* Seconds */` |
|      9 | 7256 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|      9 | 7257 | `	ph7_array_add_strkey_elem(pArray,"seconds",pValue);` |
|      - | 7258 | `	/* Minutes */` |
|      9 | 7259 | `	ph7_value_int(pValue,sTm.tm_min);` |
|      9 | 7260 | `	ph7_array_add_strkey_elem(pArray,"minutes",pValue);` |
|      - | 7261 | `	/* Hours */` |
|      9 | 7262 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|      9 | 7263 | `	ph7_array_add_strkey_elem(pArray,"hours",pValue);` |
|      - | 7264 | `	/* mday */` |
|      9 | 7265 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|      9 | 7266 | `	ph7_array_add_strkey_elem(pArray,"mday",pValue);` |
|      - | 7267 | `	/* wday */` |
|      9 | 7268 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|      9 | 7269 | `	ph7_array_add_strkey_elem(pArray,"wday",pValue);` |
|      - | 7270 | `	/* mon */` |
|      9 | 7271 | `	ph7_value_int(pValue,sTm.tm_mon+1);` |
|      9 | 7272 | `	ph7_array_add_strkey_elem(pArray,"mon",pValue);` |
|      - | 7273 | `	/* year */` |
|      9 | 7274 | `	ph7_value_int(pValue,sTm.tm_year);` |
|      9 | 7275 | `	ph7_array_add_strkey_elem(pArray,"year",pValue);` |
|      - | 7276 | `	/* yday */` |
|      9 | 7277 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|      9 | 7278 | `	ph7_array_add_strkey_elem(pArray,"yday",pValue);` |
|      - | 7279 | `	/* Weekday */` |
|      9 | 7280 | `	ph7_value_string(pValue,SyTimeGetDay(sTm.tm_wday),-1);` |
|      9 | 7281 | `	ph7_array_add_strkey_elem(pArray,"weekday",pValue);` |
|      - | 7282 | `	/* Month */` |
|      9 | 7283 | `	ph7_value_reset_string_cursor(pValue);` |
|      9 | 7284 | `	ph7_value_string(pValue,SyTimeGetMonth(sTm.tm_mon),-1);` |
|      9 | 7285 | `	ph7_array_add_strkey_elem(pArray,"month",pValue);` |
|      - | 7286 | `	/* Seconds since the epoch */` |
|      9 | 7287 | `	ph7_value_int64(pValue,(ph7_int64)time(0));` |
|      9 | 7288 | `	ph7_array_add_intkey_elem(pArray,0 /* Index zero */,pValue);` |
|      - | 7289 | `	/* Return the freshly created array */` |
|      9 | 7290 | `	ph7_result_value(pCtx,pArray);` |
|      9 | 7291 | `	return PH7_OK;` |
|      5 | 7292 |  |
|      - | 7293 | `/*` |
|      - | 7294 | ` * mixed gettimeofday([ bool $return_float = false ] )` |
|      - | 7295 | ` *   Returns an associative array containing the data returned from the system call.` |
|      - | 7296 | ` * Parameters` |
|      - | 7297 | ` *  $return_float` |
|      - | 7298 | ` *   When set to TRUE, a float instead of an array is returned.` |
|      - | 7299 | ` * Return` |
|      - | 7300 | ` *   By default an array is returned. If return_float is set, then` |
|      - | 7301 | ` *   a float is returned.` |
|      - | 7302 | ` */` |
|      4 | 7303 | `static int PH7_builtin_gettimeofday(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7304 |  |
|      5 | 7305 | `	int bFloat = 0;` |
|      - | 7306 | `	sytime sTime;` |
|      - | 7307 | `#if defined(__UNIXES__)` |
|      - | 7308 | `	struct timeval tv;` |
|      4 | 7309 | `	gettimeofday(&tv,0);` |
|      4 | 7310 | `	sTime.tm_sec  = (long)tv.tv_sec;` |
|      4 | 7311 | `	sTime.tm_usec = (long)tv.tv_usec;` |
|      - | 7312 | `#else` |
|      - | 7313 | `	time_t tt;` |
|      1 | 7314 | `	time(&tt);` |
|      1 | 7315 | `	sTime.tm_sec  = (long)tt;` |
|      1 | 7316 | `	sTime.tm_usec = (long)(tt%SX_USEC_PER_SEC);` |
|      - | 7317 | `#endif /* __UNIXES__ */` |
|      5 | 7318 | `	if( nArg > 0 ){` |
|      5 | 7319 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|      2 | 7320 | `	}` |
|      5 | 7321 | `	if( bFloat ){` |
|      - | 7322 | `		/* Return as float */` |
|      3 | 7323 | `		ph7_result_double(pCtx,(double)sTime.tm_sec);` |
|      2 | 7324 | `	}else{` |
|      - | 7325 | `		/* Return an associative array */` |
|      - | 7326 | `		ph7_value *pValue,*pArray;` |
|      - | 7327 | `		/* Create a new array */` |
|      3 | 7328 | `		pArray = ph7_context_new_array(pCtx);` |
|      - | 7329 | `		/* Element value */` |
|      3 | 7330 | `		pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 7331 | `		if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 7332 | `			/* Return NULL */` |
|    ! 0 | 7333 | `			ph7_result_null(pCtx);` |
|    ! 0 | 7334 | `			return PH7_OK;` |
|      - | 7335 | `		}` |
|      - | 7336 | `		/* Fill the array */` |
|      - | 7337 | `		/* sec */` |
|      3 | 7338 | `		ph7_value_int64(pValue,sTime.tm_sec);` |
|      3 | 7339 | `		ph7_array_add_strkey_elem(pArray,"sec",pValue);` |
|      - | 7340 | `		/* usec */` |
|      3 | 7341 | `		ph7_value_int64(pValue,sTime.tm_usec);` |
|      3 | 7342 | `		ph7_array_add_strkey_elem(pArray,"usec",pValue);` |
|      - | 7343 | `		/* Return the array */` |
|      3 | 7344 | `		ph7_result_value(pCtx,pArray);` |
|      - | 7345 | `	}` |
|      5 | 7346 | `	return PH7_OK;` |
|      3 | 7347 |  |
|      - | 7348 | `/* Check if the given year is leap or not */` |
|      - | 7349 | `#define IS_LEAP_YEAR(YEAR)	(YEAR % 400 ? ( YEAR % 100 ? ( YEAR % 4 ? 0 : 1 ) : 0 ) : 1)` |
|      - | 7350 | `/* ISO-8601 numeric representation of the day of the week */` |
|      - | 7351 | `static const int aISO8601[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|      - | 7352 | `/*` |
|      - | 7353 | ` * Format a given date string.` |
|      - | 7354 | ` * Supported format: (Taken from PHP online docs)` |
|      - | 7355 | ` * character 	Description` |
|      - | 7356 | ` * d          Day of the month` |
|      - | 7357 | ` * D          A textual representation of a days` |
|      - | 7358 | ` * j          Day of the month without leading zeros` |
|      - | 7359 | ` * l          A full textual representation of the day of the week` |
|      - | 7360 | ` * N          ISO-8601 numeric representation of the day of the week` |
|      - | 7361 | ` * w          Numeric representation of the day of the week` |
|      - | 7362 | ` * z          The day of the year (starting from 0)` |
|      - | 7363 | ` * F          A full textual representation of a month, such as January or March` |
|      - | 7364 | ` * m          Numeric representation of a month, with leading zeros 	01 through 12` |
|      - | 7365 | ` * M          A short textual representation of a month, three letters 	Jan through Dec` |
|      - | 7366 | ` * n          Numeric representation of a month, without leading zeros 	1 through 12` |
|      - | 7367 | ` * t          Number of days in the given month 	28 through 31` |
|      - | 7368 | ` * L          Whether it's a leap year 	1 if it is a leap year, 0 otherwise.` |
|      - | 7369 | ` * o          ISO-8601 year number. This has the same value as Y, except that if the ISO week number` |
|      - | 7370 | ` *            (W) belongs to the previous or next year, that year is used instead. (added in PHP 5.1.0) Examples: 1999 or 2003` |
|      - | 7371 | ` * Y          A full numeric representation of a year, 4 digits 	Examples: 1999 or 2003` |
|      - | 7372 | ` * y          A two digit representation of a year 	Examples: 99 or 03` |
|      - | 7373 | ` * a          Lowercase Ante meridiem and Post meridiem 	am or pm` |
|      - | 7374 | ` * A          Uppercase Ante meridiem and Post meridiem 	AM or PM` |
|      - | 7375 | ` * g          12-hour format of an hour without leading zeros 	1 through 12` |
|      - | 7376 | ` * G          24-hour format of an hour without leading zeros 	0 through 23` |
|      - | 7377 | ` * h          12-hour format of an hour with leading zeros 	01 through 12` |
|      - | 7378 | ` * H          24-hour format of an hour with leading zeros 	00 through 23` |
|      - | 7379 | ` * i          Minutes with leading zeros 	00 to 59` |
|      - | 7380 | ` * s          Seconds, with leading zeros 	00 through 59` |
|      - | 7381 | ` * u          Microseconds Example: 654321` |
|      - | 7382 | ` * e          Timezone identifier 	Examples: UTC, GMT, Atlantic/Azores` |
|      - | 7383 | ` * I          (capital i) Whether or not the date is in daylight saving time 	1 if Daylight Saving Time, 0 otherwise.` |
|      - | 7384 | ` * r          RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 +0200` |
|      - | 7385 | ` * U          Seconds since the Unix Epoch (January 1 1970 00:00:00 GMT)` |
|      - | 7386 | ` * S          English ordinal suffix for the day of the month, 2 characters` |
|      - | 7387 | ` * O          Difference to Greenwich time (GMT) in hours` |
|      - | 7388 | ` * Z          Timezone offset in seconds. The offset for timezones west of UTC is always negative, and for those` |
|      - | 7389 | ` *            east of UTC is always positive.` |
|      - | 7390 | ` * c         ISO 8601 date` |
|      - | 7391 | ` */` |
|     46 | 7392 | `static sxi32 DateFormat(ph7_context *pCtx,const char *zIn,int nLen,Sytm *pTm)` |
|      1 | 7393 |  |
|     47 | 7394 | `	const char *zEnd = &zIn[nLen];` |
|      - | 7395 | `	const char *zCur;` |
|      - | 7396 | `	/* Start the format process */` |
|     78 | 7397 | `	for(;;){` |
|    157 | 7398 | `		if( zIn >= zEnd ){` |
|      - | 7399 | `			/* No more input to process */` |
|     47 | 7400 | `			break;` |
|      - | 7401 | `		}` |
|    111 | 7402 | `		switch(zIn[0]){` |
|      7 | 7403 | `		case 'd':` |
|      - | 7404 | `			/* Day of the month, 2 digits with leading zeros */` |
|     15 | 7405 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mday);` |
|     15 | 7406 | `			break;` |
|    ! 0 | 7407 | `		case 'D':` |
|      - | 7408 | `			/*A textual representation of a day, three letters*/` |
|    ! 0 | 7409 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|    ! 0 | 7410 | `			ph7_result_string(pCtx,zCur,3);` |
|    ! 0 | 7411 | `			break;` |
|    ! 0 | 7412 | `		case 'j':` |
|      - | 7413 | `			/*	Day of the month without leading zeros */` |
|    ! 0 | 7414 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mday);` |
|    ! 0 | 7415 | `			break;` |
|      2 | 7416 | `		case 'l':` |
|      - | 7417 | `			/* A full textual representation of the day of the week */` |
|      5 | 7418 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|      5 | 7419 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|      5 | 7420 | `			break;` |
|    ! 0 | 7421 | `		case 'N':{` |
|      - | 7422 | `			/* ISO-8601 numeric representation of the day of the week */` |
|    ! 0 | 7423 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|    ! 0 | 7424 | `			break;` |
|      - | 7425 | `				 }` |
|    ! 0 | 7426 | `		case 'w':` |
|      - | 7427 | `			/*Numeric representation of the day of the week*/` |
|    ! 0 | 7428 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|    ! 0 | 7429 | `			break;` |
|    ! 0 | 7430 | `		case 'z':` |
|      - | 7431 | `			/*The day of the year*/` |
|    ! 0 | 7432 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_yday);` |
|    ! 0 | 7433 | `			break;` |
|      2 | 7434 | `		case 'F':` |
|      - | 7435 | `			/*A full textual representation of a month, such as January or March*/` |
|      5 | 7436 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|      5 | 7437 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|      5 | 7438 | `			break;` |
|      7 | 7439 | `		case 'm':` |
|      - | 7440 | `			/*Numeric representation of a month, with leading zeros*/` |
|     15 | 7441 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|     15 | 7442 | `			break;` |
|    ! 0 | 7443 | `		case 'M':` |
|      - | 7444 | `			/*A short textual representation of a month, three letters*/` |
|    ! 0 | 7445 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|    ! 0 | 7446 | `			ph7_result_string(pCtx,zCur,3);` |
|    ! 0 | 7447 | `			break;` |
|    ! 0 | 7448 | `		case 'n':` |
|      - | 7449 | `			/*Numeric representation of a month, without leading zeros*/` |
|    ! 0 | 7450 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mon + 1);` |
|    ! 0 | 7451 | `			break;` |
|    ! 0 | 7452 | `		case 't':{` |
|      - | 7453 | `			static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|    ! 0 | 7454 | `			int nDays = aMonDays[pTm->tm_mon % 12 ];` |
|    ! 0 | 7455 | `			if( pTm->tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(pTm->tm_year) ){` |
|    ! 0 | 7456 | `				nDays = 28;` |
|    ! 0 | 7457 | `			}` |
|      - | 7458 | `			/*Number of days in the given month*/` |
|    ! 0 | 7459 | `			ph7_result_string_format(pCtx,"%d",nDays);` |
|    ! 0 | 7460 | `			break;` |
|      - | 7461 | `				 }` |
|    ! 0 | 7462 | `		case 'L':{` |
|    ! 0 | 7463 | `			int isLeap = IS_LEAP_YEAR(pTm->tm_year);` |
|      - | 7464 | `			/* Whether it's a leap year */` |
|    ! 0 | 7465 | `			ph7_result_string_format(pCtx,"%d",isLeap);` |
|    ! 0 | 7466 | `			break;` |
|      - | 7467 | `				 }` |
|    ! 0 | 7468 | `		case 'o':` |
|      - | 7469 | `			/* ISO-8601 year number.*/` |
|    ! 0 | 7470 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|    ! 0 | 7471 | `			break;` |
|      9 | 7472 | `		case 'Y':` |
|      - | 7473 | `			/*	A full numeric representation of a year, 4 digits */` |
|     19 | 7474 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|     19 | 7475 | `			break;` |
|    ! 0 | 7476 | `		case 'y':` |
|      - | 7477 | `			/*A two digit representation of a year*/` |
|    ! 0 | 7478 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_year%100);` |
|    ! 0 | 7479 | `			break;` |
|    ! 0 | 7480 | `		case 'a':` |
|      - | 7481 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7482 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",2);` |
|    ! 0 | 7483 | `			break;` |
|    ! 0 | 7484 | `		case 'A':` |
|      - | 7485 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7486 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",2);` |
|    ! 0 | 7487 | `			break;` |
|    ! 0 | 7488 | `		case 'g':` |
|      - | 7489 | `			/*	12-hour format of an hour without leading zeros*/` |
|    ! 0 | 7490 | `			ph7_result_string_format(pCtx,"%d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7491 | `			break;` |
|    ! 0 | 7492 | `		case 'G':` |
|      - | 7493 | `			/* 24-hour format of an hour without leading zeros */` |
|    ! 0 | 7494 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_hour);` |
|    ! 0 | 7495 | `			break;` |
|    ! 0 | 7496 | `		case 'h':` |
|      - | 7497 | `			/* 12-hour format of an hour with leading zeros */` |
|    ! 0 | 7498 | `			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7499 | `			break;` |
|      3 | 7500 | `		case 'H':` |
|      - | 7501 | `			/*	24-hour format of an hour with leading zeros */` |
|      7 | 7502 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|      7 | 7503 | `			break;` |
|      3 | 7504 | `		case 'i':` |
|      - | 7505 | `			/* 	Minutes with leading zeros */` |
|      7 | 7506 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|      7 | 7507 | `			break;` |
|      3 | 7508 | `		case 's':` |
|      - | 7509 | `			/* 	second with leading zeros */` |
|      7 | 7510 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|      7 | 7511 | `			break;` |
|    ! 0 | 7512 | `		case 'u':` |
|      - | 7513 | `			/* 	Microseconds */` |
|    ! 0 | 7514 | `			ph7_result_string_format(pCtx,"%u",pTm->tm_sec * SX_USEC_PER_SEC);` |
|    ! 0 | 7515 | `			break;` |
|    ! 0 | 7516 | `		case 'S':{` |
|      - | 7517 | `			/* English ordinal suffix for the day of the month, 2 characters */` |
|      - | 7518 | `			static const char zSuffix[] = "thstndrdthththththth";` |
|    ! 0 | 7519 | `			int v = pTm->tm_mday;` |
|    ! 0 | 7520 | `			ph7_result_string(pCtx,&zSuffix[2 * (int)(v / 10 % 10 != 1 ? v % 10 : 0)],(int)sizeof(char) * 2);` |
|    ! 0 | 7521 | `			break;` |
|      - | 7522 | `				 }` |
|    ! 0 | 7523 | `		case 'e':` |
|      - | 7524 | `			/* 	Timezone identifier */` |
|    ! 0 | 7525 | `			zCur = pTm->tm_zone;` |
|    ! 0 | 7526 | `			if( zCur == 0 ){` |
|      - | 7527 | `				/* Assume GMT */` |
|    ! 0 | 7528 | `				zCur = "GMT";` |
|    ! 0 | 7529 | `			}` |
|    ! 0 | 7530 | `			ph7_result_string(pCtx,zCur,-1);` |
|    ! 0 | 7531 | `			break;` |
|    ! 0 | 7532 | `		case 'I':` |
|      - | 7533 | `			/* Whether or not the date is in daylight saving time */` |
|      - | 7534 | `#ifdef __WINNT__` |
|      - | 7535 | `#ifdef _MSC_VER` |
|      - | 7536 | `#ifndef _WIN32_WCE` |
|    ! 0 | 7537 | `			_get_daylight(&pTm->tm_isdst);` |
|      - | 7538 | `#endif` |
|      - | 7539 | `#endif` |
|      - | 7540 | `#endif` |
|    ! 0 | 7541 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_isdst == 1);` |
|    ! 0 | 7542 | `			break;` |
|    ! 0 | 7543 | `		case 'r':` |
|      - | 7544 | `			/* RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 */` |
|    ! 0 | 7545 | `			ph7_result_string_format(pCtx,"%.3s, %02d %.3s %4d %02d:%02d:%02d",` |
|    ! 0 | 7546 | `				SyTimeGetDay(pTm->tm_wday),` |
|    ! 0 | 7547 | `				pTm->tm_mday,` |
|    ! 0 | 7548 | `				SyTimeGetMonth(pTm->tm_mon),` |
|    ! 0 | 7549 | `				pTm->tm_year,` |
|    ! 0 | 7550 | `				pTm->tm_hour,` |
|    ! 0 | 7551 | `				pTm->tm_min,` |
|    ! 0 | 7552 | `				pTm->tm_sec` |
|      - | 7553 | `				);` |
|    ! 0 | 7554 | `			break;` |
|    ! 0 | 7555 | `		case 'U':{` |
|      - | 7556 | `			time_t tt;` |
|      - | 7557 | `			/* Seconds since the Unix Epoch */` |
|    ! 0 | 7558 | `			time(&tt);` |
|    ! 0 | 7559 | `			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);` |
|    ! 0 | 7560 | `			break;` |
|      - | 7561 | `				 }` |
|    ! 0 | 7562 | `		case 'O':` |
|      - | 7563 | `		case 'P':` |
|      - | 7564 | `			/* Difference to Greenwich time (GMT) in hours */` |
|    ! 0 | 7565 | `			ph7_result_string_format(pCtx,"%+05d",pTm->tm_gmtoff);` |
|    ! 0 | 7566 | `			break;` |
|    ! 0 | 7567 | `		case 'Z':` |
|      - | 7568 | `			/* Timezone offset in seconds. The offset for timezones west of UTC` |
|      - | 7569 | `			 * is always negative, and for those east of UTC is always positive.` |
|      - | 7570 | `			 */` |
|    ! 0 | 7571 | `			ph7_result_string_format(pCtx,"%+05d",pTm->tm_gmtoff);` |
|    ! 0 | 7572 | `			break;` |
|      1 | 7573 | `		case 'c':` |
|      - | 7574 | `			/* 	ISO 8601 date */` |
|      4 | 7575 | `			ph7_result_string_format(pCtx,"%4d-%02d-%02dT%02d:%02d:%02d%+05d",` |
|      1 | 7576 | `				pTm->tm_year,` |
|      2 | 7577 | `				pTm->tm_mon+1,` |
|      1 | 7578 | `				pTm->tm_mday,` |
|      1 | 7579 | `				pTm->tm_hour,` |
|      1 | 7580 | `				pTm->tm_min,` |
|      1 | 7581 | `				pTm->tm_sec,` |
|      1 | 7582 | `				pTm->tm_gmtoff` |
|      - | 7583 | `				);` |
|      3 | 7584 | `			break;` |
|      1 | 7585 | `		case '\\':` |
|      3 | 7586 | `			zIn++;` |
|      - | 7587 | `			/* Expand verbatim */` |
|      3 | 7588 | `			if( zIn < zEnd ){` |
|      3 | 7589 | `				ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|      1 | 7590 | `			}` |
|      3 | 7591 | `			break;` |
|     17 | 7592 | `		default:` |
|      - | 7593 | `			/* Unknown format specifer,expand verbatim */` |
|     35 | 7594 | `			ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|     34 | 7595 | `			break;` |
|      - | 7596 | `		}` |
|      - | 7597 | `		/* Point to the next character */` |
|    111 | 7598 | `		zIn++;` |
|      1 | 7599 | `	}` |
|     47 | 7600 | `	return SXRET_OK;` |
|      1 | 7601 |  |
|      - | 7602 | `/*` |
|      - | 7603 | ` * PH7 implementation of the strftime() function.` |
|      - | 7604 | ` * The following formats are supported:` |
|      - | 7605 | ` * %a 	An abbreviated textual representation of the day` |
|      - | 7606 | ` * %A 	A full textual representation of the day` |
|      - | 7607 | ` * %d 	Two-digit day of the month (with leading zeros)` |
|      - | 7608 | ` * %e 	Day of the month, with a space preceding single digits.` |
|      - | 7609 | ` * %j 	Day of the year, 3 digits with leading zeros` |
|      - | 7610 | ` * %u 	ISO-8601 numeric representation of the day of the week 	1 (for Monday) though 7 (for Sunday)` |
|      - | 7611 | ` * %w 	Numeric representation of the day of the week 0 (for Sunday) through 6 (for Saturday)` |
|      - | 7612 | ` * %U 	Week number of the given year, starting with the first Sunday as the first week` |
|      - | 7613 | ` * %V 	ISO-8601:1988 week number of the given year, starting with the first week of the year with at least` |
|      - | 7614 | ` *   4 weekdays, with Monday being the start of the week.` |
|      - | 7615 | ` * %W 	A numeric representation of the week of the year` |
|      - | 7616 | ` * %b 	Abbreviated month name, based on the locale` |
|      - | 7617 | ` * %B 	Full month name, based on the locale` |
|      - | 7618 | ` * %h 	Abbreviated month name, based on the locale (an alias of %b)` |
|      - | 7619 | ` * %m 	Two digit representation of the month` |
|      - | 7620 | ` * %C 	Two digit representation of the century (year divided by 100, truncated to an integer)` |
|      - | 7621 | ` * %g 	Two digit representation of the year going by ISO-8601:1988 standards (see %V)` |
|      - | 7622 | ` * %G 	The full four-digit version of %g` |
|      - | 7623 | ` * %y 	Two digit representation of the year` |
|      - | 7624 | ` * %Y 	Four digit representation for the year` |
|      - | 7625 | ` * %H 	Two digit representation of the hour in 24-hour format` |
|      - | 7626 | ` * %I 	Two digit representation of the hour in 12-hour format` |
|      - | 7627 | ` * %l (lower-case 'L') 	Hour in 12-hour format, with a space preceeding single digits` |
|      - | 7628 | ` * %M 	Two digit representation of the minute` |
|      - | 7629 | ` * %p 	UPPER-CASE 'AM' or 'PM' based on the given time` |
|      - | 7630 | ` * %P 	lower-case 'am' or 'pm' based on the given time` |
|      - | 7631 | ` * %r 	Same as "%I:%M:%S %p"` |
|      - | 7632 | ` * %R 	Same as "%H:%M"` |
|      - | 7633 | ` * %S 	Two digit representation of the second` |
|      - | 7634 | ` * %T 	Same as "%H:%M:%S"` |
|      - | 7635 | ` * %X 	Preferred time representation based on locale, without the date` |
|      - | 7636 | ` * %z 	Either the time zone offset from UTC or the abbreviation` |
|      - | 7637 | ` * %Z 	The time zone offset/abbreviation option NOT given by %z` |
|      - | 7638 | ` * %c 	Preferred date and time stamp based on local` |
|      - | 7639 | ` * %D 	Same as "%m/%d/%y"` |
|      - | 7640 | ` * %F 	Same as "%Y-%m-%d"` |
|      - | 7641 | ` * %s 	Unix Epoch Time timestamp (same as the time() function)` |
|      - | 7642 | ` * %x 	Preferred date representation based on locale, without the time` |
|      - | 7643 | ` * %n 	A newline character ("\n")` |
|      - | 7644 | ` * %t 	A Tab character ("\t")` |
|      - | 7645 | ` * %% 	A literal percentage character ("%")` |
|      - | 7646 | ` */` |
|     16 | 7647 | `static int PH7_Strftime(` |
|      - | 7648 | `	ph7_context *pCtx,  /* Call context */` |
|      - | 7649 | `	const char *zIn,    /* Input string */` |
|      - | 7650 | `	int nLen,           /* Input length */` |
|      - | 7651 | `	Sytm *pTm           /* Parse of the given time */` |
|      - | 7652 | `	)` |
|      1 | 7653 |  |
|     17 | 7654 | `	const char *zCur,*zEnd = &zIn[nLen];` |
|      - | 7655 | `	int c;` |
|      - | 7656 | `	/* Start the format process */` |
|     18 | 7657 | `	for(;;){` |
|     37 | 7658 | `		zCur = zIn;` |
|     41 | 7659 | `		while(zIn < zEnd && zIn[0] != '%' ){` |
|      5 | 7660 | `			zIn++;` |
|      1 | 7661 | `		}` |
|     37 | 7662 | `		if( zIn > zCur ){` |
|      - | 7663 | `			/* Consume input verbatim */` |
|      5 | 7664 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 7665 | `		}` |
|     37 | 7666 | `		zIn++; /* Jump the percent sign */` |
|     37 | 7667 | `		if( zIn >= zEnd ){` |
|      - | 7668 | `			/* No more input to process */` |
|     17 | 7669 | `			break;` |
|      - | 7670 | `		}` |
|     21 | 7671 | `		c = zIn[0];` |
|      - | 7672 | `		/* Act according to the current specifer */` |
|     21 | 7673 | `		switch(c){` |
|    ! 0 | 7674 | `		case '%':` |
|      - | 7675 | `			/* A literal percentage character ("%") */` |
|    ! 0 | 7676 | `			ph7_result_string(pCtx,"%",(int)sizeof(char));` |
|    ! 0 | 7677 | `			break;` |
|    ! 0 | 7678 | `		case 't':` |
|      - | 7679 | `			/* A Tab character */` |
|    ! 0 | 7680 | `			ph7_result_string(pCtx,"\t",(int)sizeof(char));` |
|    ! 0 | 7681 | `			break;` |
|    ! 0 | 7682 | `		case 'n':` |
|      - | 7683 | `			/* A newline character */` |
|    ! 0 | 7684 | `			ph7_result_string(pCtx,"\n",(int)sizeof(char));` |
|    ! 0 | 7685 | `			break;` |
|      1 | 7686 | `		case 'a':` |
|      - | 7687 | `			/* An abbreviated textual representation of the day */` |
|      3 | 7688 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),(int)sizeof(char)*3);` |
|      3 | 7689 | `			break;` |
|    ! 0 | 7690 | `		case 'A':` |
|      - | 7691 | `			/* A full textual representation of the day */` |
|    ! 0 | 7692 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),-1/*Compute length automatically*/);` |
|    ! 0 | 7693 | `			break;` |
|    ! 0 | 7694 | `		case 'e':` |
|      - | 7695 | `			/* Day of the month, 2 digits with leading space for single digit*/` |
|    ! 0 | 7696 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_mday);` |
|    ! 0 | 7697 | `			break;` |
|      2 | 7698 | `		case 'd':` |
|      - | 7699 | `			/* Two-digit day of the month (with leading zeros) */` |
|      5 | 7700 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon+1);` |
|      5 | 7701 | `			break;` |
|    ! 0 | 7702 | `		case 'j':` |
|      - | 7703 | `			/*The day of the year,3 digits with leading zeros*/` |
|    ! 0 | 7704 | `			ph7_result_string_format(pCtx,"%03d",pTm->tm_yday);` |
|    ! 0 | 7705 | `			break;` |
|    ! 0 | 7706 | `		case 'u':` |
|      - | 7707 | `			/* ISO-8601 numeric representation of the day of the week */` |
|    ! 0 | 7708 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|    ! 0 | 7709 | `			break;` |
|    ! 0 | 7710 | `		case 'w':` |
|      - | 7711 | `			/* Numeric representation of the day of the week */` |
|    ! 0 | 7712 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|    ! 0 | 7713 | `			break;` |
|    ! 0 | 7714 | `		case 'b':` |
|      - | 7715 | `		case 'h':` |
|      - | 7716 | `			/*A short textual representation of a month, three letters (Not based on locale)*/` |
|    ! 0 | 7717 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),(int)sizeof(char)*3);` |
|    ! 0 | 7718 | `			break;` |
|    ! 0 | 7719 | `		case 'B':` |
|      - | 7720 | `			/* Full month name (Not based on locale) */` |
|    ! 0 | 7721 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),-1/*Compute length automatically*/);` |
|    ! 0 | 7722 | `			break;` |
|      2 | 7723 | `		case 'm':` |
|      - | 7724 | `			/*Numeric representation of a month, with leading zeros*/` |
|      5 | 7725 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|      5 | 7726 | `			break;` |
|    ! 0 | 7727 | `		case 'C':` |
|      - | 7728 | `			/* Two digit representation of the century */` |
|    ! 0 | 7729 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year/100);` |
|    ! 0 | 7730 | `			break;` |
|    ! 0 | 7731 | `		case 'y':` |
|      - | 7732 | `		case 'g':` |
|      - | 7733 | `			/* Two digit representation of the year */` |
|    ! 0 | 7734 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year%100);` |
|    ! 0 | 7735 | `			break;` |
|      2 | 7736 | `		case 'Y':` |
|      - | 7737 | `		case 'G':` |
|      - | 7738 | `			/* Four digit representation of the year */` |
|      5 | 7739 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|      5 | 7740 | `			break;` |
|    ! 0 | 7741 | `		case 'I':` |
|      - | 7742 | `			/* 12-hour format of an hour with leading zeros */` |
|    ! 0 | 7743 | `			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7744 | `			break;` |
|    ! 0 | 7745 | `		case 'l':` |
|      - | 7746 | `			/* 12-hour format of an hour with leading space */` |
|    ! 0 | 7747 | `			ph7_result_string_format(pCtx,"%2d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7748 | `			break;` |
|      1 | 7749 | `		case 'H':` |
|      - | 7750 | `			/* 24-hour format of an hour with leading zeros */` |
|      3 | 7751 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|      3 | 7752 | `			break;` |
|      1 | 7753 | `		case 'M':` |
|      - | 7754 | `			/* Minutes with leading zeros */` |
|      3 | 7755 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|      3 | 7756 | `			break;` |
|    ! 0 | 7757 | `		case 'S':` |
|      - | 7758 | `			/* Seconds with leading zeros */` |
|    ! 0 | 7759 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|    ! 0 | 7760 | `			break;` |
|    ! 0 | 7761 | `		case 'z':` |
|      - | 7762 | `		case 'Z':` |
|      - | 7763 | `			/* 	Timezone identifier */` |
|    ! 0 | 7764 | `			zCur = pTm->tm_zone;` |
|    ! 0 | 7765 | `			if( zCur == 0 ){` |
|      - | 7766 | `				/* Assume GMT */` |
|    ! 0 | 7767 | `				zCur = "GMT";` |
|    ! 0 | 7768 | `			}` |
|    ! 0 | 7769 | `			ph7_result_string(pCtx,zCur,-1);` |
|    ! 0 | 7770 | `			break;` |
|    ! 0 | 7771 | `		case 'T':` |
|      - | 7772 | `		case 'X':` |
|      - | 7773 | `			/* Same as "%H:%M:%S" */` |
|    ! 0 | 7774 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d",pTm->tm_hour,pTm->tm_min,pTm->tm_sec);` |
|    ! 0 | 7775 | `			break;` |
|    ! 0 | 7776 | `		case 'R':` |
|      - | 7777 | `			/* Same as "%H:%M" */` |
|    ! 0 | 7778 | `			ph7_result_string_format(pCtx,"%02d:%02d",pTm->tm_hour,pTm->tm_min);` |
|    ! 0 | 7779 | `			break;` |
|    ! 0 | 7780 | `		case 'P':` |
|      - | 7781 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7782 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",(int)sizeof(char)*2);` |
|    ! 0 | 7783 | `			break;` |
|    ! 0 | 7784 | `		case 'p':` |
|      - | 7785 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7786 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",(int)sizeof(char)*2);` |
|    ! 0 | 7787 | `			break;` |
|    ! 0 | 7788 | `		case 'r':` |
|      - | 7789 | `			/* Same as "%I:%M:%S %p" */` |
|    ! 0 | 7790 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d %s",` |
|    ! 0 | 7791 | `				1+(pTm->tm_hour%12),` |
|    ! 0 | 7792 | `				pTm->tm_min,` |
|    ! 0 | 7793 | `				pTm->tm_sec,` |
|    ! 0 | 7794 | `				pTm->tm_hour > 12 ? "PM" : "AM"` |
|      - | 7795 | `				);` |
|    ! 0 | 7796 | `			break;` |
|      1 | 7797 | `		case 'D':` |
|      - | 7798 | `		case 'x':` |
|      - | 7799 | `			/* Same as "%m/%d/%y" */` |
|      4 | 7800 | `			ph7_result_string_format(pCtx,"%02d/%02d/%02d",` |
|      2 | 7801 | `				pTm->tm_mon+1,` |
|      1 | 7802 | `				pTm->tm_mday,` |
|      2 | 7803 | `				pTm->tm_year%100` |
|      - | 7804 | `				);` |
|      3 | 7805 | `			break;` |
|    ! 0 | 7806 | `		case 'F':` |
|      - | 7807 | `			/* Same as "%Y-%m-%d" */` |
|    ! 0 | 7808 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d",` |
|    ! 0 | 7809 | `				pTm->tm_year,` |
|    ! 0 | 7810 | `				pTm->tm_mon+1,` |
|    ! 0 | 7811 | `				pTm->tm_mday` |
|      - | 7812 | `				);` |
|    ! 0 | 7813 | `			break;` |
|    ! 0 | 7814 | `		case 'c':` |
|    ! 0 | 7815 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d %02d:%02d:%02d",` |
|    ! 0 | 7816 | `				pTm->tm_year,` |
|    ! 0 | 7817 | `				pTm->tm_mon+1,` |
|    ! 0 | 7818 | `				pTm->tm_mday,` |
|    ! 0 | 7819 | `				pTm->tm_hour,` |
|    ! 0 | 7820 | `				pTm->tm_min,` |
|    ! 0 | 7821 | `				pTm->tm_sec` |
|      - | 7822 | `				);` |
|    ! 0 | 7823 | `			break;` |
|    ! 0 | 7824 | `		case 's':{` |
|      - | 7825 | `			time_t tt;` |
|      - | 7826 | `			/* Seconds since the Unix Epoch */` |
|    ! 0 | 7827 | `			time(&tt);` |
|    ! 0 | 7828 | `			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);` |
|    ! 0 | 7829 | `			break;` |
|      - | 7830 | `				 }` |
|    ! 0 | 7831 | `		default:` |
|      - | 7832 | `			/* unknown specifer,simply ignore*/` |
|    ! 0 | 7833 | `			break;` |
|      - | 7834 | `		}` |
|      - | 7835 | `		/* Advance the cursor */` |
|     21 | 7836 | `		zIn++;` |
|      1 | 7837 | `	}` |
|     17 | 7838 | `	return SXRET_OK;` |
|      1 | 7839 |  |
|      - | 7840 | `/*` |
|      - | 7841 | ` * string date(string $format [, int $timestamp = time() ] )` |
|      - | 7842 | ` *  Returns a string formatted according to the given format string using` |
|      - | 7843 | ` *  the given integer timestamp or the current time if no timestamp is given.` |
|      - | 7844 | ` *  In other words, timestamp is optional and defaults to the value of time().` |
|      - | 7845 | ` * Parameters` |
|      - | 7846 | ` *  $format` |
|      - | 7847 | ` *   The format of the outputted date string (See code above)` |
|      - | 7848 | ` * $timestamp` |
|      - | 7849 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|      - | 7850 | ` *   that defaults to the current local time if a timestamp is not given.` |
|      - | 7851 | ` *   In other words, it defaults to the value of time().` |
|      - | 7852 | ` * Return` |
|      - | 7853 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|      - | 7854 | ` */` |
|     36 | 7855 | `static int PH7_builtin_date(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7856 |  |
|      - | 7857 | `	const char *zFormat;` |
|      - | 7858 | `	int nLen;` |
|      - | 7859 | `	Sytm sTm;` |
|     37 | 7860 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7861 | `		/* Missing/Invalid argument,return FALSE */` |
|      5 | 7862 | `		ph7_result_bool(pCtx,0);` |
|      5 | 7863 | `		return PH7_OK;` |
|      - | 7864 | `	}` |
|     33 | 7865 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     33 | 7866 | `	if( nLen < 1 ){` |
|      - | 7867 | `		/* Don't bother processing return the empty string */` |
|    ! 0 | 7868 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7869 | `	}` |
|     33 | 7870 | `	if( nArg < 2 ){` |
|      - | 7871 | `#ifdef __WINNT__` |
|      - | 7872 | `		SYSTEMTIME sOS;` |
|      1 | 7873 | `		GetSystemTime(&sOS);` |
|      1 | 7874 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 7875 | `#else` |
|      - | 7876 | `		struct tm *pTm;` |
|      - | 7877 | `		time_t t;` |
|     30 | 7878 | `		time(&t);` |
|     30 | 7879 | `		pTm = localtime(&t);` |
|     30 | 7880 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7881 | `#endif` |
|     16 | 7882 | `	}else{` |
|      - | 7883 | `		/* Use the given timestamp */` |
|      - | 7884 | `		time_t t;` |
|      - | 7885 | `		struct tm *pTm;` |
|      3 | 7886 | `		if( ph7_value_is_int(apArg[1]) ){` |
|      3 | 7887 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|      3 | 7888 | `			pTm = localtime(&t);` |
|      3 | 7889 | `			if( pTm == 0 ){` |
|    ! 0 | 7890 | `				time(&t);` |
|    ! 0 | 7891 | `			}` |
|      2 | 7892 | `		}else{` |
|    ! 0 | 7893 | `			time(&t);` |
|      - | 7894 | `		}` |
|      3 | 7895 | `		pTm = localtime(&t);` |
|      3 | 7896 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7897 | `	}` |
|      - | 7898 | `	/* Format the given string */` |
|     33 | 7899 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|     33 | 7900 | `	return PH7_OK;` |
|     19 | 7901 |  |
|      - | 7902 | `/*` |
|      - | 7903 | ` * string strftime(string $format [, int $timestamp = time() ] )` |
|      - | 7904 | ` *  Format a local time/date (PLATFORM INDEPENDANT IMPLEENTATION NOT BASED ON LOCALE)` |
|      - | 7905 | ` * Parameters` |
|      - | 7906 | ` *  $format` |
|      - | 7907 | ` *   The format of the outputted date string (See code above)` |
|      - | 7908 | ` * $timestamp` |
|      - | 7909 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|      - | 7910 | ` *   that defaults to the current local time if a timestamp is not given.` |
|      - | 7911 | ` *   In other words, it defaults to the value of time().` |
|      - | 7912 | ` * Return` |
|      - | 7913 | ` * Returns a string formatted according format using the given timestamp` |
|      - | 7914 | ` * or the current local time if no timestamp is given.` |
|      - | 7915 | ` */` |
|     20 | 7916 | `static int PH7_builtin_strftime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7917 |  |
|      - | 7918 | `	const char *zFormat;` |
|      - | 7919 | `	int nLen;` |
|      - | 7920 | `	Sytm sTm;` |
|     21 | 7921 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7922 | `		/* Missing/Invalid argument,return FALSE */` |
|      5 | 7923 | `		ph7_result_bool(pCtx,0);` |
|      5 | 7924 | `		return PH7_OK;` |
|      - | 7925 | `	}` |
|     17 | 7926 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7927 | `	if( nLen < 1 ){` |
|      - | 7928 | `		/* Don't bother processing return FALSE */` |
|    ! 0 | 7929 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7930 | `	}` |
|     17 | 7931 | `	if( nArg < 2 ){` |
|      - | 7932 | `#ifdef __WINNT__` |
|      - | 7933 | `		SYSTEMTIME sOS;` |
|      1 | 7934 | `		GetSystemTime(&sOS);` |
|      1 | 7935 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 7936 | `#else` |
|      - | 7937 | `		struct tm *pTm;` |
|      - | 7938 | `		time_t t;` |
|     14 | 7939 | `		time(&t);` |
|     14 | 7940 | `		pTm = localtime(&t);` |
|     14 | 7941 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7942 | `#endif` |
|      8 | 7943 | `	}else{` |
|      - | 7944 | `		/* Use the given timestamp */` |
|      - | 7945 | `		time_t t;` |
|      - | 7946 | `		struct tm *pTm;` |
|      3 | 7947 | `		if( ph7_value_is_int(apArg[1]) ){` |
|      3 | 7948 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|      3 | 7949 | `			pTm = localtime(&t);` |
|      3 | 7950 | `			if( pTm == 0 ){` |
|    ! 0 | 7951 | `				time(&t);` |
|    ! 0 | 7952 | `			}` |
|      2 | 7953 | `		}else{` |
|    ! 0 | 7954 | `			time(&t);` |
|      - | 7955 | `		}` |
|      3 | 7956 | `		pTm = localtime(&t);` |
|      3 | 7957 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7958 | `	}` |
|      - | 7959 | `	/* Format the given string */` |
|     17 | 7960 | `	PH7_Strftime(pCtx,zFormat,nLen,&sTm);` |
|     17 | 7961 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|      - | 7962 | `		/* Nothing was formatted,return FALSE */` |
|    ! 0 | 7963 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7964 | `	}` |
|     17 | 7965 | `	return PH7_OK;` |
|     11 | 7966 |  |
|      - | 7967 | `/*` |
|      - | 7968 | ` * string gmdate(string $format [, int $timestamp = time() ] )` |
|      - | 7969 | ` *  Identical to the date() function except that the time returned` |
|      - | 7970 | ` *  is Greenwich Mean Time (GMT).` |
|      - | 7971 | ` * Parameters` |
|      - | 7972 | ` *  $format` |
|      - | 7973 | ` *  The format of the outputted date string (See code above)` |
|      - | 7974 | ` *  $timestamp` |
|      - | 7975 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|      - | 7976 | ` *   that defaults to the current local time if a timestamp is not given.` |
|      - | 7977 | ` *   In other words, it defaults to the value of time().` |
|      - | 7978 | ` * Return` |
|      - | 7979 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|      - | 7980 | ` */` |
|     16 | 7981 | `static int PH7_builtin_gmdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7982 |  |
|      - | 7983 | `	const char *zFormat;` |
|      - | 7984 | `	int nLen;` |
|      - | 7985 | `	Sytm sTm;` |
|     17 | 7986 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7987 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 | 7988 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7989 | `		return PH7_OK;` |
|      - | 7990 | `	}` |
|     15 | 7991 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 7992 | `	if( nLen < 1 ){` |
|      - | 7993 | `		/* Don't bother processing return the empty string */` |
|    ! 0 | 7994 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7995 | `	}` |
|     15 | 7996 | `	if( nArg < 2 ){` |
|      - | 7997 | `#ifdef __WINNT__` |
|      - | 7998 | `		SYSTEMTIME sOS;` |
|      1 | 7999 | `		GetSystemTime(&sOS);` |
|      1 | 8000 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8001 | `#else` |
|      - | 8002 | `		struct tm *pTm;` |
|      - | 8003 | `		time_t t;` |
|     12 | 8004 | `		time(&t);` |
|     12 | 8005 | `		pTm = gmtime(&t);` |
|     12 | 8006 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8007 | `#endif` |
|      7 | 8008 | `	}else{` |
|      - | 8009 | `		/* Use the given timestamp */` |
|      - | 8010 | `		time_t t;` |
|      - | 8011 | `		struct tm *pTm;` |
|      3 | 8012 | `		if( ph7_value_is_int(apArg[1]) ){` |
|      3 | 8013 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|      3 | 8014 | `			pTm = gmtime(&t);` |
|      3 | 8015 | `			if( pTm == 0 ){` |
|    ! 0 | 8016 | `				time(&t);` |
|    ! 0 | 8017 | `			}` |
|      2 | 8018 | `		}else{` |
|    ! 0 | 8019 | `			time(&t);` |
|      - | 8020 | `		}` |
|      3 | 8021 | `		pTm = gmtime(&t);` |
|      3 | 8022 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8023 | `	}` |
|      - | 8024 | `	/* Format the given string */` |
|     15 | 8025 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|     15 | 8026 | `	return PH7_OK;` |
|      9 | 8027 |  |
|      - | 8028 | `/*` |
|      - | 8029 | ` * array localtime([ int $timestamp = time() [, bool $is_associative = false ]])` |
|      - | 8030 | ` *  Return the local time.` |
|      - | 8031 | ` * Parameter` |
|      - | 8032 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|      - | 8033 | ` *     that defaults to the current local time if a timestamp is not given.` |
|      - | 8034 | ` *     In other words, it defaults to the value of time().` |
|      - | 8035 | ` * $is_associative` |
|      - | 8036 | ` *   If set to FALSE or not supplied then the array is returned as a regular, numerically` |
|      - | 8037 | ` *   indexed array. If the argument is set to TRUE then localtime() returns an associative` |
|      - | 8038 | ` *   array containing all the different elements of the structure returned by the C function` |
|      - | 8039 | ` *   call to localtime. The names of the different keys of the associative array are as follows:` |
|      - | 8040 | ` *      "tm_sec" - seconds, 0 to 59` |
|      - | 8041 | ` *      "tm_min" - minutes, 0 to 59` |
|      - | 8042 | ` *      "tm_hour" - hours, 0 to 23` |
|      - | 8043 | ` *      "tm_mday" - day of the month, 1 to 31` |
|      - | 8044 | ` *      "tm_mon" - month of the year, 0 (Jan) to 11 (Dec)` |
|      - | 8045 | ` *      "tm_year" - years since 1900` |
|      - | 8046 | ` *      "tm_wday" - day of the week, 0 (Sun) to 6 (Sat)` |
|      - | 8047 | ` *      "tm_yday" - day of the year, 0 to 365` |
|      - | 8048 | ` *      "tm_isdst" - is daylight savings time in effect? Positive if yes, 0 if not, negative if unknown.` |
|      - | 8049 | ` * Returns` |
|      - | 8050 | ` *  An associative array of information related to the timestamp.` |
|      - | 8051 | ` */` |
|      8 | 8052 | `static int PH7_builtin_localtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8053 |  |
|      - | 8054 | `	ph7_value *pValue,*pArray;` |
|      9 | 8055 | `	int isAssoc = 0;` |
|      - | 8056 | `	Sytm sTm;` |
|      9 | 8057 | `	if( nArg < 1 ){` |
|      - | 8058 | `#ifdef __WINNT__` |
|      - | 8059 | `		SYSTEMTIME sOS;` |
|      1 | 8060 | `		GetSystemTime(&sOS); /* TODO(chems): GMT not local */` |
|      1 | 8061 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8062 | `#else` |
|      - | 8063 | `		struct tm *pTm;` |
|      - | 8064 | `		time_t t;` |
|      4 | 8065 | `		time(&t);` |
|      4 | 8066 | `		pTm = localtime(&t);` |
|      4 | 8067 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8068 | `#endif` |
|      3 | 8069 | `	}else{` |
|      - | 8070 | `		/* Use the given timestamp */` |
|      - | 8071 | `		time_t t;` |
|      - | 8072 | `		struct tm *pTm;` |
|      5 | 8073 | `		if( ph7_value_is_int(apArg[0]) ){` |
|      5 | 8074 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|      5 | 8075 | `			pTm = localtime(&t);` |
|      5 | 8076 | `			if( pTm == 0 ){` |
|    ! 0 | 8077 | `				time(&t);` |
|    ! 0 | 8078 | `			}` |
|      3 | 8079 | `		}else{` |
|    ! 0 | 8080 | `			time(&t);` |
|      - | 8081 | `		}` |
|      5 | 8082 | `		pTm = localtime(&t);` |
|      5 | 8083 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8084 | `	}` |
|      - | 8085 | `	/* Element value */` |
|      9 | 8086 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      9 | 8087 | `	if( pValue == 0 ){` |
|      - | 8088 | `		/* Return NULL */` |
|    ! 0 | 8089 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8090 | `		return PH7_OK;` |
|      - | 8091 | `	}` |
|      - | 8092 | `	/* Create a new array */` |
|      9 | 8093 | `	pArray = ph7_context_new_array(pCtx);` |
|      9 | 8094 | `	if( pArray == 0 ){` |
|      - | 8095 | `		/* Return NULL */` |
|    ! 0 | 8096 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8097 | `		return PH7_OK;` |
|      - | 8098 | `	}` |
|      9 | 8099 | `	if( nArg > 1 ){` |
|      3 | 8100 | `		isAssoc = ph7_value_to_bool(apArg[1]);` |
|      1 | 8101 | `	}` |
|      - | 8102 | `	/* Fill the array */` |
|      - | 8103 | `	/* Seconds */` |
|      9 | 8104 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|      9 | 8105 | `	if( isAssoc ){` |
|      3 | 8106 | `		ph7_array_add_strkey_elem(pArray,"tm_sec",pValue);` |
|      2 | 8107 | `	}else{` |
|      7 | 8108 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8109 | `	}` |
|      - | 8110 | `	/* Minutes */` |
|      9 | 8111 | `	ph7_value_int(pValue,sTm.tm_min);` |
|      9 | 8112 | `	if( isAssoc ){` |
|      3 | 8113 | `		ph7_array_add_strkey_elem(pArray,"tm_min",pValue);` |
|      2 | 8114 | `	}else{` |
|      7 | 8115 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8116 | `	}` |
|      - | 8117 | `	/* Hours */` |
|      9 | 8118 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|      9 | 8119 | `	if( isAssoc ){` |
|      3 | 8120 | `		ph7_array_add_strkey_elem(pArray,"tm_hour",pValue);` |
|      2 | 8121 | `	}else{` |
|      7 | 8122 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8123 | `	}` |
|      - | 8124 | `	/* mday */` |
|      9 | 8125 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|      9 | 8126 | `	if( isAssoc ){` |
|      3 | 8127 | `		ph7_array_add_strkey_elem(pArray,"tm_mday",pValue);` |
|      2 | 8128 | `	}else{` |
|      7 | 8129 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8130 | `	}` |
|      - | 8131 | `	/* mon */` |
|      9 | 8132 | `	ph7_value_int(pValue,sTm.tm_mon);` |
|      9 | 8133 | `	if( isAssoc ){` |
|      3 | 8134 | `		ph7_array_add_strkey_elem(pArray,"tm_mon",pValue);` |
|      2 | 8135 | `	}else{` |
|      7 | 8136 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8137 | `	}` |
|      - | 8138 | `	/* year since 1900 */` |
|      9 | 8139 | `	ph7_value_int(pValue,sTm.tm_year-1900);` |
|      9 | 8140 | `	if( isAssoc ){` |
|      3 | 8141 | `		ph7_array_add_strkey_elem(pArray,"tm_year",pValue);` |
|      2 | 8142 | `	}else{` |
|      7 | 8143 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8144 | `	}` |
|      - | 8145 | `	/* wday */` |
|      9 | 8146 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|      9 | 8147 | `	if( isAssoc ){` |
|      3 | 8148 | `		ph7_array_add_strkey_elem(pArray,"tm_wday",pValue);` |
|      2 | 8149 | `	}else{` |
|      7 | 8150 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8151 | `	}` |
|      - | 8152 | `	/* yday */` |
|      9 | 8153 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|      9 | 8154 | `	if( isAssoc ){` |
|      3 | 8155 | `		ph7_array_add_strkey_elem(pArray,"tm_yday",pValue);` |
|      2 | 8156 | `	}else{` |
|      7 | 8157 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8158 | `	}` |
|      - | 8159 | `	/* isdst */` |
|      - | 8160 | `#ifdef __WINNT__` |
|      - | 8161 | `#ifdef _MSC_VER` |
|      - | 8162 | `#ifndef _WIN32_WCE` |
|      1 | 8163 | `			_get_daylight(&sTm.tm_isdst);` |
|      - | 8164 | `#endif` |
|      - | 8165 | `#endif` |
|      - | 8166 | `#endif` |
|      9 | 8167 | `	ph7_value_int(pValue,sTm.tm_isdst);` |
|      9 | 8168 | `	if( isAssoc ){` |
|      3 | 8169 | `		ph7_array_add_strkey_elem(pArray,"tm_isdst",pValue);` |
|      2 | 8170 | `	}else{` |
|      7 | 8171 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8172 | `	}` |
|      - | 8173 | `	/* Return the array */` |
|      9 | 8174 | `	ph7_result_value(pCtx,pArray);` |
|      9 | 8175 | `	return PH7_OK;` |
|      5 | 8176 |  |
|      - | 8177 | `/*` |
|      - | 8178 | ` * int idate(string $format [, int $timestamp = time() ])` |
|      - | 8179 | ` *  Returns a number formatted according to the given format string` |
|      - | 8180 | ` *  using the given integer timestamp or the current local time if` |
|      - | 8181 | ` *  no timestamp is given. In other words, timestamp is optional and defaults` |
|      - | 8182 | ` *  to the value of time().` |
|      - | 8183 | ` *  Unlike the function date(), idate() accepts just one char in the format` |
|      - | 8184 | ` *  parameter.` |
|      - | 8185 | ` * $Parameters` |
|      - | 8186 | ` *  Supported format` |
|      - | 8187 | ` *   d 	Day of the month` |
|      - | 8188 | ` *   h 	Hour (12 hour format)` |
|      - | 8189 | ` *   H 	Hour (24 hour format)` |
|      - | 8190 | ` *   i 	Minutes` |
|      - | 8191 | ` *   I (uppercase i)1 if DST is activated, 0 otherwise` |
|      - | 8192 | ` *   L (uppercase l) returns 1 for leap year, 0 otherwise` |
|      - | 8193 | ` *   m 	Month number` |
|      - | 8194 | ` *   s 	Seconds` |
|      - | 8195 | ` *   t 	Days in current month` |
|      - | 8196 | ` *   U 	Seconds since the Unix Epoch - January 1 1970 00:00:00 UTC - this is the same as time()` |
|      - | 8197 | ` *   w 	Day of the week (0 on Sunday)` |
|      - | 8198 | ` *   W 	ISO-8601 week number of year, weeks starting on Monday` |
|      - | 8199 | ` *   y 	Year (1 or 2 digits - check note below)` |
|      - | 8200 | ` *   Y 	Year (4 digits)` |
|      - | 8201 | ` *   z 	Day of the year` |
|      - | 8202 | ` *   Z 	Timezone offset in seconds` |
|      - | 8203 | ` * $timestamp` |
|      - | 8204 | ` *  The optional timestamp parameter is an integer Unix timestamp that defaults` |
|      - | 8205 | ` *  to the current local time if a timestamp is not given. In other words, it defaults` |
|      - | 8206 | ` *  to the value of time().` |
|      - | 8207 | ` * Return` |
|      - | 8208 | ` *  An integer.` |
|      - | 8209 | ` */` |
|     40 | 8210 | `static int PH7_builtin_idate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 8211 |  |
|      - | 8212 | `	const char *zFormat;` |
|     42 | 8213 | `	ph7_int64 iVal = 0;` |
|      - | 8214 | `	int nLen;` |
|      - | 8215 | `	Sytm sTm;` |
|     42 | 8216 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8217 | `		/* Missing/Invalid argument,return -1 */` |
|      5 | 8218 | `		ph7_result_int(pCtx,-1);` |
|      5 | 8219 | `		return PH7_OK;` |
|      - | 8220 | `	}` |
|     42 | 8221 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     42 | 8222 | `	if( nLen < 1 ){` |
|      - | 8223 | `		/* Don't bother processing return -1*/` |
|    ! 0 | 8224 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 8225 | `	}` |
|     42 | 8226 | `	if( nArg < 2 ){` |
|      - | 8227 | `#ifdef __WINNT__` |
|      - | 8228 | `		SYSTEMTIME sOS;` |
|      2 | 8229 | `		GetSystemTime(&sOS);` |
|      2 | 8230 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8231 | `#else` |
|      - | 8232 | `		struct tm *pTm;` |
|      - | 8233 | `		time_t t;` |
|     30 | 8234 | `		time(&t);` |
|     30 | 8235 | `		pTm = localtime(&t);` |
|     30 | 8236 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8237 | `#endif` |
|     18 | 8238 | `	}else{` |
|      - | 8239 | `		/* Use the given timestamp */` |
|      - | 8240 | `		time_t t;` |
|      - | 8241 | `		struct tm *pTm;` |
|     11 | 8242 | `		if( ph7_value_is_int(apArg[1]) ){` |
|     11 | 8243 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|     11 | 8244 | `			pTm = localtime(&t);` |
|     11 | 8245 | `			if( pTm == 0 ){` |
|    ! 0 | 8246 | `				time(&t);` |
|    ! 0 | 8247 | `			}` |
|      6 | 8248 | `		}else{` |
|    ! 0 | 8249 | `			time(&t);` |
|      - | 8250 | `		}` |
|     11 | 8251 | `		pTm = localtime(&t);` |
|     11 | 8252 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8253 | `	}` |
|      - | 8254 | `	/* Perform the requested operation */` |
|     42 | 8255 | `	switch(zFormat[0]){` |
|      2 | 8256 | `	case 'd':` |
|      - | 8257 | `		/* Day of the month */` |
|      5 | 8258 | `		iVal = sTm.tm_mday;` |
|      5 | 8259 | `		break;` |
|    ! 0 | 8260 | `	case 'h':` |
|      - | 8261 | `		/*	Hour (12 hour format)*/` |
|    ! 0 | 8262 | `		iVal = 1 + (sTm.tm_hour % 12);` |
|    ! 0 | 8263 | `		break;` |
|      1 | 8264 | `	case 'H':` |
|      - | 8265 | `		/* Hour (24 hour format)*/` |
|      3 | 8266 | `		iVal = sTm.tm_hour;` |
|      3 | 8267 | `		break;` |
|      1 | 8268 | `	case 'i':` |
|      - | 8269 | `		/*Minutes*/` |
|      3 | 8270 | `		iVal = sTm.tm_min;` |
|      3 | 8271 | `		break;` |
|      1 | 8272 | `	case 'I':` |
|      - | 8273 | `		/*	returns 1 if DST is activated, 0 otherwise */` |
|      - | 8274 | `#ifdef __WINNT__` |
|      - | 8275 | `#ifdef _MSC_VER` |
|      - | 8276 | `#ifndef _WIN32_WCE` |
|      1 | 8277 | `			_get_daylight(&sTm.tm_isdst);` |
|      - | 8278 | `#endif` |
|      - | 8279 | `#endif` |
|      - | 8280 | `#endif` |
|      3 | 8281 | `		iVal = sTm.tm_isdst;` |
|      3 | 8282 | `		break;` |
|      1 | 8283 | `	case 'L':` |
|      - | 8284 | `		/* 	returns 1 for leap year, 0 otherwise */` |
|      3 | 8285 | `		iVal = IS_LEAP_YEAR(sTm.tm_year);` |
|      3 | 8286 | `		break;` |
|      2 | 8287 | `	case 'm':` |
|      - | 8288 | `		/* Month number*/` |
|      5 | 8289 | `		iVal = sTm.tm_mon;` |
|      5 | 8290 | `		break;` |
|      1 | 8291 | `	case 's':` |
|      - | 8292 | `		/*Seconds*/` |
|      3 | 8293 | `		iVal = sTm.tm_sec;` |
|      3 | 8294 | `		break;` |
|      1 | 8295 | `	case 't':{` |
|      - | 8296 | `		/*Days in current month*/` |
|      - | 8297 | `		static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|      5 | 8298 | `		int nDays = aMonDays[sTm.tm_mon % 12 ];` |
|      5 | 8299 | `		if( sTm.tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(sTm.tm_year) ){` |
|      3 | 8300 | `			nDays = 28;` |
|      1 | 8301 | `		}` |
|      7 | 8302 | `		iVal = nDays;` |
|      7 | 8303 | `		break;` |
|      - | 8304 | `			 }` |
|      1 | 8305 | `	case 'U':` |
|      - | 8306 | `		/*Seconds since the Unix Epoch*/` |
|      3 | 8307 | `		iVal = (ph7_int64)time(0);` |
|      3 | 8308 | `		break;` |
|      1 | 8309 | `	case 'w':` |
|      - | 8310 | `		/*	Day of the week (0 on Sunday) */` |
|      3 | 8311 | `		iVal = sTm.tm_wday;` |
|      3 | 8312 | `		break;` |
|      1 | 8313 | `	case 'W': {` |
|      - | 8314 | `		/* ISO-8601 week number of year, weeks starting on Monday */` |
|      - | 8315 | `		static const int aISO8601_local[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|      3 | 8316 | `		iVal = aISO8601_local[sTm.tm_wday % 7 ];` |
|      3 | 8317 | `		break;` |
|      - | 8318 | `			  }` |
|    ! 0 | 8319 | `	case 'y':` |
|      - | 8320 | `		/* Year (2 digits) */` |
|    ! 0 | 8321 | `		iVal = sTm.tm_year % 100;` |
|    ! 0 | 8322 | `		break;` |
|      3 | 8323 | `	case 'Y':` |
|      - | 8324 | `		/* Year (4 digits) */` |
|      7 | 8325 | `		iVal = sTm.tm_year;` |
|      7 | 8326 | `		break;` |
|      1 | 8327 | `	case 'z':` |
|      - | 8328 | `		/* Day of the year */` |
|      3 | 8329 | `		iVal = sTm.tm_yday;` |
|      3 | 8330 | `		break;` |
|      1 | 8331 | `	case 'Z':` |
|      - | 8332 | `		/*Timezone offset in seconds*/` |
|      3 | 8333 | `		iVal = sTm.tm_gmtoff;` |
|      3 | 8334 | `		break;` |
|      1 | 8335 | `	default:` |
|      - | 8336 | `		/* unknown format,throw a warning */` |
|      3 | 8337 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Unknown date format token");` |
|      2 | 8338 | `		break;` |
|      - | 8339 | `	}` |
|      - | 8340 | `	/* Return the time value */` |
|     40 | 8341 | `	ph7_result_int64(pCtx,iVal);` |
|     40 | 8342 | `	return PH7_OK;` |
|     23 | 8343 |  |
|      - | 8344 | `/*` |
|      - | 8345 | ` * int mktime/gmmktime([ int $hour = date("H") [, int $minute = date("i") [, int $second = date("s")` |
|      - | 8346 | ` *  [, int $month = date("n") [, int $day = date("j") [, int $year = date("Y") [, int $is_dst = -1 ]]]]]]] )` |
|      - | 8347 | ` *  Returns the Unix timestamp corresponding to the arguments given. This timestamp is a 64bit integer` |
|      - | 8348 | ` *  containing the number of seconds between the Unix Epoch (January 1 1970 00:00:00 GMT) and the time` |
|      - | 8349 | ` *  specified.` |
|      - | 8350 | ` *  Arguments may be left out in order from right to left; any arguments thus omitted will be set to` |
|      - | 8351 | ` *  the current value according to the local date and time.` |
|      - | 8352 | ` * Parameters` |
|      - | 8353 | ` * $hour` |
|      - | 8354 | ` *  The number of the hour relevant to the start of the day determined by month, day and year.` |
|      - | 8355 | ` *  Negative values reference the hour before midnight of the day in question. Values greater` |
|      - | 8356 | ` *  than 23 reference the appropriate hour in the following day(s).` |
|      - | 8357 | ` * $minute` |
|      - | 8358 | ` *  The number of the minute relevant to the start of the hour. Negative values reference` |
|      - | 8359 | ` *  the minute in the previous hour. Values greater than 59 reference the appropriate minute` |
|      - | 8360 | ` *  in the following hour(s).` |
|      - | 8361 | ` * $second` |
|      - | 8362 | ` *  The number of seconds relevant to the start of the minute. Negative values reference` |
|      - | 8363 | ` *  the second in the previous minute. Values greater than 59 reference the appropriate` |
|      - | 8364 | ` * second in the following minute(s).` |
|      - | 8365 | ` * $month` |
|      - | 8366 | ` *  The number of the month relevant to the end of the previous year. Values 1 to 12 reference` |
|      - | 8367 | ` *  the normal calendar months of the year in question. Values less than 1 (including negative values)` |
|      - | 8368 | ` *  reference the months in the previous year in reverse order, so 0 is December, -1 is November)...` |
|      - | 8369 | ` * $day` |
|      - | 8370 | ` *  The number of the day relevant to the end of the previous month. Values 1 to 28, 29, 30 or 31` |
|      - | 8371 | ` *  (depending upon the month) reference the normal days in the relevant month. Values less than 1` |
|      - | 8372 | ` *  (including negative values) reference the days in the previous month, so 0 is the last day` |
|      - | 8373 | ` *  of the previous month, -1 is the day before that, etc. Values greater than the number of days` |
|      - | 8374 | ` *  in the relevant month reference the appropriate day in the following month(s).` |
|      - | 8375 | ` * $year` |
|      - | 8376 | ` *  The number of the year, may be a two or four digit value, with values between 0-69 mapping` |
|      - | 8377 | ` *  to 2000-2069 and 70-100 to 1970-2000. On systems where time_t is a 32bit signed integer, as` |
|      - | 8378 | ` *  most common today, the valid range for year is somewhere between 1901 and 2038.` |
|      - | 8379 | ` * $is_dst` |
|      - | 8380 | ` *  This parameter can be set to 1 if the time is during daylight savings time (DST), 0 if it is not,` |
|      - | 8381 | ` *  or -1 (the default) if it is unknown whether the time is within daylight savings time or not.` |
|      - | 8382 | ` * Return` |
|      - | 8383 | ` *   mktime() returns the Unix timestamp of the arguments given.` |
|      - | 8384 | ` *   If the arguments are invalid, the function returns FALSE` |
|      - | 8385 | ` */` |
|      8 | 8386 | `static int PH7_builtin_mktime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8387 |  |
|      - | 8388 | `	const char *zFunction;` |
|      9 | 8389 | `	ph7_int64 iVal = 0;` |
|      - | 8390 | `	struct tm *pTm;` |
|      - | 8391 | `	time_t t;` |
|      - | 8392 | `	/* Extract function name */` |
|      9 | 8393 | `	zFunction = ph7_function_name(pCtx);` |
|      - | 8394 | `	/* Get the current time */` |
|      9 | 8395 | `	time(&t);` |
|      9 | 8396 | `	if( zFunction[0] == 'g' /* gmmktime */ ){` |
|      3 | 8397 | `		pTm = gmtime(&t);` |
|      2 | 8398 | `	}else{` |
|      - | 8399 | `		/* localtime */` |
|      7 | 8400 | `		pTm = localtime(&t);` |
|      - | 8401 | `	}` |
|      9 | 8402 | `	if( nArg > 0 ){` |
|      - | 8403 | `		int iTmp;` |
|      - | 8404 | `		/* Hour */` |
|      9 | 8405 | `		iTmp = ph7_value_to_int(apArg[0]);` |
|      9 | 8406 | `		pTm->tm_hour = iTmp;` |
|      9 | 8407 | `		if( nArg > 1 ){` |
|      - | 8408 | `			/* Minutes */` |
|      9 | 8409 | `			iTmp = ph7_value_to_int(apArg[1]);` |
|      9 | 8410 | `			pTm->tm_min = iTmp;` |
|      9 | 8411 | `			if( nArg > 2 ){` |
|      - | 8412 | `				/* Seconds */` |
|      9 | 8413 | `				iTmp = ph7_value_to_int(apArg[2]);` |
|      9 | 8414 | `				pTm->tm_sec = iTmp;` |
|      9 | 8415 | `				if( nArg > 3 ){` |
|      - | 8416 | `					/* Month */` |
|      9 | 8417 | `					iTmp = ph7_value_to_int(apArg[3]);` |
|      9 | 8418 | `					pTm->tm_mon = iTmp - 1;` |
|      9 | 8419 | `					if( nArg > 4 ){` |
|      - | 8420 | `						/* mday */` |
|      9 | 8421 | `						iTmp = ph7_value_to_int(apArg[4]);` |
|      9 | 8422 | `						pTm->tm_mday = iTmp;` |
|      9 | 8423 | `						if( nArg > 5 ){` |
|      - | 8424 | `							/* Year */` |
|      9 | 8425 | `							iTmp = ph7_value_to_int(apArg[5]);` |
|      9 | 8426 | `							if( iTmp > 1900 ){` |
|      9 | 8427 | `								iTmp -= 1900;` |
|      4 | 8428 | `							}` |
|      9 | 8429 | `							pTm->tm_year = iTmp;` |
|      9 | 8430 | `							if( nArg > 6 ){` |
|      - | 8431 | `								/* is_dst */` |
|    ! 0 | 8432 | `								iTmp = ph7_value_to_bool(apArg[6]);` |
|    ! 0 | 8433 | `								pTm->tm_isdst = iTmp;` |
|    ! 0 | 8434 | `							}` |
|      4 | 8435 | `						}` |
|      4 | 8436 | `					}` |
|      4 | 8437 | `				}` |
|      4 | 8438 | `			}` |
|      4 | 8439 | `		}` |
|      4 | 8440 | `	}` |
|      - | 8441 | `	/* Make the time */` |
|      9 | 8442 | `	iVal = (ph7_int64)mktime(pTm);` |
|      - | 8443 | `	/* Return the timesatmp as a 64bit integer */` |
|      9 | 8444 | `	ph7_result_int64(pCtx,iVal);` |
|      9 | 8445 | `	return PH7_OK;` |
|      1 | 8446 |  |
|      - | 8447 | `/*` |
|      - | 8448 | ` * Section:` |
|      - | 8449 | ` *    URL handling Functions.` |
|      - | 8450 | ` * Status:` |
|      - | 8451 | ` *    Stable.` |
|      - | 8452 | ` */` |
|      - | 8453 | `/*` |
|      - | 8454 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 8455 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 8456 | ` */` |
|   1026 | 8457 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 8458 |  |
|      - | 8459 | `	/* Store in the call context result buffer */` |
|   1028 | 8460 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 8461 | `	return SXRET_OK;` |
|      2 | 8462 |  |
|      - | 8463 | `/*` |
|      - | 8464 | ` * string base64_encode(string $data)` |
|      - | 8465 | ` * string convert_uuencode(string $data)` |
|      - | 8466 | ` *  Encodes data with MIME base64` |
|      - | 8467 | ` * Parameter` |
|      - | 8468 | ` *  $data` |
|      - | 8469 | ` *    Data to encode` |
|      - | 8470 | ` * Return` |
|      - | 8471 | ` *  Encoded data or FALSE on failure.` |
|      - | 8472 | ` */` |
|     10 | 8473 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8474 |  |
|      - | 8475 | `	const char *zIn;` |
|      - | 8476 | `	int nLen;` |
|     11 | 8477 | `	if( nArg < 1 ){` |
|      - | 8478 | `		/* Missing arguments,return FALSE */` |
|      5 | 8479 | `		ph7_result_bool(pCtx,0);` |
|      5 | 8480 | `		return PH7_OK;` |
|      - | 8481 | `	}` |
|      - | 8482 | `	/* Extract the input string */` |
|      7 | 8483 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8484 | `	if( nLen < 1 ){` |
|      - | 8485 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8486 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8487 | `		return PH7_OK;` |
|      - | 8488 | `	}` |
|      - | 8489 | `	/* Perform the BASE64 encoding */` |
|      7 | 8490 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 8491 | `	return PH7_OK;` |
|      6 | 8492 |  |
|      - | 8493 | `/*` |
|      - | 8494 | ` * string base64_decode(string $data)` |
|      - | 8495 | ` * string convert_uudecode(string $data)` |
|      - | 8496 | ` *  Decodes data encoded with MIME base64` |
|      - | 8497 | ` * Parameter` |
|      - | 8498 | ` *  $data` |
|      - | 8499 | ` *    Encoded data.` |
|      - | 8500 | ` * Return` |
|      - | 8501 | ` *  Returns the original data or FALSE on failure.` |
|      - | 8502 | ` */` |
|     36 | 8503 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 8504 |  |
|      - | 8505 | `	const char *zIn;` |
|      - | 8506 | `	int nLen;` |
|     38 | 8507 | `	if( nArg < 1 ){` |
|      - | 8508 | `		/* Missing arguments,return FALSE */` |
|      3 | 8509 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8510 | `		return PH7_OK;` |
|      - | 8511 | `	}` |
|      - | 8512 | `	/* Extract the input string */` |
|     36 | 8513 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 8514 | `	if( nLen < 1 ){` |
|      - | 8515 | `		/* Nothing to process,return FALSE */` |
|      3 | 8516 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8517 | `		return PH7_OK;` |
|      - | 8518 | `	}` |
|      - | 8519 | `	/* Perform the BASE64 decoding */` |
|     34 | 8520 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 8521 | `	return PH7_OK;` |
|     20 | 8522 |  |
|      - | 8523 | `/*` |
|      - | 8524 | ` * string urlencode(string $str)` |
|      - | 8525 | ` *  URL encoding` |
|      - | 8526 | ` * Parameter` |
|      - | 8527 | ` *  $data` |
|      - | 8528 | ` *   Input string.` |
|      - | 8529 | ` * Return` |
|      - | 8530 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 8531 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 8532 | ` *  encoded as plus (+) signs.` |
|      - | 8533 | ` */` |
|      6 | 8534 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8535 |  |
|      - | 8536 | `	const char *zIn;` |
|      - | 8537 | `	int nLen;` |
|      7 | 8538 | `	if( nArg < 1 ){` |
|      - | 8539 | `		/* Missing arguments,return FALSE */` |
|      3 | 8540 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8541 | `		return PH7_OK;` |
|      - | 8542 | `	}` |
|      - | 8543 | `	/* Extract the input string */` |
|      5 | 8544 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 8545 | `	if( nLen < 1 ){` |
|      - | 8546 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8547 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8548 | `		return PH7_OK;` |
|      - | 8549 | `	}` |
|      - | 8550 | `	/* Perform the URL encoding */` |
|      5 | 8551 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 8552 | `	return PH7_OK;` |
|      4 | 8553 |  |
|      - | 8554 | `/*` |
|      - | 8555 | ` * string urldecode(string $str)` |
|      - | 8556 | ` *  Decodes any %## encoding in the given string.` |
|      - | 8557 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 8558 | ` * Parameter` |
|      - | 8559 | ` *  $data` |
|      - | 8560 | ` *    Input string.` |
|      - | 8561 | ` * Return` |
|      - | 8562 | ` *  Decoded URL or FALSE on failure.` |
|      - | 8563 | ` */` |
|      8 | 8564 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8565 |  |
|      - | 8566 | `	const char *zIn;` |
|      - | 8567 | `	int nLen;` |
|      9 | 8568 | `	if( nArg < 1 ){` |
|      - | 8569 | `		/* Missing arguments,return FALSE */` |
|      3 | 8570 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8571 | `		return PH7_OK;` |
|      - | 8572 | `	}` |
|      - | 8573 | `	/* Extract the input string */` |
|      7 | 8574 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8575 | `	if( nLen < 1 ){` |
|      - | 8576 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8577 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8578 | `		return PH7_OK;` |
|      - | 8579 | `	}` |
|      - | 8580 | `	/* Perform the URL decoding */` |
|      7 | 8581 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 8582 | `	return PH7_OK;` |
|      5 | 8583 |  |
|      - | 8584 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8585 | `/* Table of the built-in functions */` |
|      - | 8586 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 8587 | `	   /* Variable handling functions */` |
|      - | 8588 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 8589 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 8590 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 8591 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 8592 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 8593 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 8594 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 8595 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 8596 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 8597 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 8598 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 8599 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 8600 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 8601 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 8602 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 8603 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 8604 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 8605 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 8606 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 8607 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8608 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 8609 | `	   /* Math functions */` |
|      - | 8610 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 8611 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 8612 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 8613 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 8614 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 8615 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 8616 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 8617 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 8618 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 8619 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 8620 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 8621 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 8622 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 8623 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 8624 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 8625 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 8626 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 8627 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 8628 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 8629 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 8630 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 8631 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 8632 | `	{ "round",    PH7_builtin_round        },` |
|      - | 8633 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 8634 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 8635 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 8636 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 8637 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 8638 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 8639 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 8640 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 8641 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8642 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8643 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 8644 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8645 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8646 | `	   /* String handling functions */` |
|      - | 8647 |  |
|      - | 8648 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 8649 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 8650 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 8651 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 8652 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 8653 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 8654 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 8655 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 8656 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 8657 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 8658 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 8659 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 8660 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 8661 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 8662 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 8663 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 8664 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 8665 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 8666 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 8667 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 8668 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 8669 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 8670 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 8671 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 8672 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 8673 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 8674 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 8675 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 8676 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 8677 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 8678 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 8679 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 8680 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 8681 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 8682 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 8683 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 8684 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 8685 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 8686 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 8687 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 8688 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 8689 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 8690 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 8691 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 8692 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 8693 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 8694 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 8695 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 8696 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 8697 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8698 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8699 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 8700 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 8701 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 8702 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 8703 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8704 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8705 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 8706 |  |
|      - | 8707 |  |
|      - | 8708 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 8709 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 8710 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 8711 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 8712 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 8713 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8714 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8715 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 8716 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 8717 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8718 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8719 |  |
|      - | 8720 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 8721 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 8722 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 8723 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 8724 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 8725 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 8726 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 8727 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 8728 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 8729 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 8730 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 8731 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 8732 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8733 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8734 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 8735 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8736 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8737 |  |
|      - | 8738 | `	         /* Ctype functions */` |
|      - | 8739 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 8740 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 8741 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 8742 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 8743 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 8744 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 8745 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 8746 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 8747 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 8748 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 8749 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 8750 | `	         /* Time functions */` |
|      - | 8751 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 8752 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 8753 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 8754 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 8755 | `	{ "date",        PH7_builtin_date         },` |
|      - | 8756 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 8757 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 8758 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 8759 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 8760 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 8761 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 8762 | `	        /* URL functions */` |
|      - | 8763 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 8764 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 8765 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 8766 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 8767 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 8768 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 8769 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 8770 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 8771 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8772 | `};` |
|      - | 8773 | `/*` |
|      - | 8774 | ` * Register the built-in functions defined above,the array functions` |
|      - | 8775 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 8776 | ` */` |
|    926 | 8777 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      2 | 8778 |  |
|      - | 8779 | `	sxu32 n;` |
| 141680 | 8780 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 140754 | 8781 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
|  70378 | 8782 | `	}` |
|      - | 8783 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|    928 | 8784 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 8785 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|    928 | 8786 | `	PH7_RegisterIORoutine(&(*pVm));` |
|    928 | 8787 |  |
|      - | 8788 |  |
