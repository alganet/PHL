# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3703/4350 lines (85.13%)

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
|  15458 |  269 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  270 |  |
|  15460 |  271 | `	int res = 1; /* Assume empty by default */` |
|  15460 |  272 | `	if( nArg > 0 ){` |
|  15458 |  273 | `		res = ph7_value_is_empty(apArg[0]);` |
|   7728 |  274 | `	}` |
|  15460 |  275 | `	ph7_result_bool(pCtx,res);` |
|  15460 |  276 | `	return PH7_OK;` |
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
|     22 |  398 | `static int PH7_builtin_acos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  399 |  |
|      - |  400 | `	double r, x;` |
|      - |  401 | `	/* PHP enforces exactly one argument and a floatable parameter. */` |
|     24 |  402 | `	if( nArg != 1 ){` |
|      4 |  403 | `		return PH7_VmThrowException(pCtx,` |
|      - |  404 | `			"ArgumentCountError",` |
|      - |  405 | `			"acos() expects exactly 1 argument, %d given",` |
|      1 |  406 | `			nArg` |
|      - |  407 | `			);` |
|      - |  408 | `	}` |
|      - |  409 | `	/* Type checking: reject non-numeric values (arrays, objects, resources, strings)` |
|      - |  410 | `	 * PHP8 reports a TypeError for wrong types.  Numeric strings are allowed but` |
|      - |  411 | `	 * the float conversion will handle them. */` |
|     22 |  412 | `	if( !ph7_value_is_numeric(apArg[0]) ){` |
|      7 |  413 | `		return PH7_VmThrowException(pCtx,` |
|      - |  414 | `			"TypeError",` |
|      - |  415 | `			"acos(): Argument #1 ($num) must be of type float, %s given",` |
|      2 |  416 | `			ph7_type_name(apArg[0])` |
|      - |  417 | `			);` |
|      - |  418 | `	}` |
|      - |  419 | `	/* Convert to double now that we know it's numeric. */` |
|     17 |  420 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  421 | `	/* Handle domain error ourselves.  PHP returns NAN for \|x\|>1. */` |
|     17 |  422 | `	if( x < -1.0 \|\| x > 1.0 ){` |
|      5 |  423 | `		r = NAN;` |
|      3 |  424 | `	}else{` |
|     13 |  425 | `		r = acos(x);` |
|      - |  426 | `	}` |
|      - |  427 | `	/* store the result back */` |
|     17 |  428 | `	ph7_result_double(pCtx,r);` |
|     17 |  429 | `	return PH7_OK;` |
|     13 |  430 |  |
|      - |  431 | `/*` |
|      - |  432 | ` * float cosh(float $arg )` |
|      - |  433 | ` *  Hyperbolic cosine.` |
|      - |  434 | ` * Parameter` |
|      - |  435 | ` *  The number to process.` |
|      - |  436 | ` * Return` |
|      - |  437 | ` *  The hyperbolic cosine of arg.` |
|      - |  438 | ` */` |
|     18 |  439 | `static int PH7_builtin_cosh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  440 |  |
|      - |  441 | `	double r,x;` |
|     19 |  442 | `	if( nArg < 1 ){` |
|      - |  443 | `		/* Missing argument,return 0 */` |
|      3 |  444 | `		ph7_result_int(pCtx,0);` |
|      3 |  445 | `		return PH7_OK;` |
|      - |  446 | `	}` |
|     17 |  447 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  448 | `	/* Perform the requested operation */` |
|     17 |  449 | `	r = cosh(x);` |
|      - |  450 | `	/* store the result back */` |
|     17 |  451 | `	ph7_result_double(pCtx,r);` |
|     17 |  452 | `	return PH7_OK;` |
|     10 |  453 |  |
|      - |  454 | `/*` |
|      - |  455 | ` * float sin(float $arg )` |
|      - |  456 | ` *  Sine.` |
|      - |  457 | ` * Parameter` |
|      - |  458 | ` *  The number to process.` |
|      - |  459 | ` * Return` |
|      - |  460 | ` *  The sine of arg.` |
|      - |  461 | ` */` |
|      8 |  462 | `static int PH7_builtin_sin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  463 |  |
|      - |  464 | `	double r,x;` |
|      9 |  465 | `	if( nArg < 1 ){` |
|      - |  466 | `		/* Missing argument,return 0 */` |
|      7 |  467 | `		ph7_result_int(pCtx,0);` |
|      7 |  468 | `		return PH7_OK;` |
|      - |  469 | `	}` |
|      3 |  470 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  471 | `	/* Perform the requested operation */` |
|      3 |  472 | `	r = sin(x);` |
|      - |  473 | `	/* store the result back */` |
|      3 |  474 | `	ph7_result_double(pCtx,r);` |
|      3 |  475 | `	return PH7_OK;` |
|      5 |  476 |  |
|      - |  477 | `/*` |
|      - |  478 | ` * float asin(float $arg )` |
|      - |  479 | ` *  Arc sine.` |
|      - |  480 | ` * Parameter` |
|      - |  481 | ` *  The number to process.` |
|      - |  482 | ` * Return` |
|      - |  483 | ` *  The arc sine of arg.` |
|      - |  484 | ` */` |
|     14 |  485 | `static int PH7_builtin_asin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  486 |  |
|      - |  487 | `	double r,x;` |
|     15 |  488 | `	if( nArg < 1 ){` |
|      - |  489 | `		/* Missing argument,return 0 */` |
|      3 |  490 | `		ph7_result_int(pCtx,0);` |
|      3 |  491 | `		return PH7_OK;` |
|      - |  492 | `	}` |
|     13 |  493 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  494 | `	/* Perform the requested operation */` |
|     13 |  495 | `	r = asin(x);` |
|      - |  496 | `	/* store the result back */` |
|     13 |  497 | `	ph7_result_double(pCtx,r);` |
|     13 |  498 | `	return PH7_OK;` |
|      8 |  499 |  |
|      - |  500 | `/*` |
|      - |  501 | ` * float sinh(float $arg )` |
|      - |  502 | ` *  Hyperbolic sine.` |
|      - |  503 | ` * Parameter` |
|      - |  504 | ` *  The number to process.` |
|      - |  505 | ` * Return` |
|      - |  506 | ` *  The hyperbolic sine of arg.` |
|      - |  507 | ` */` |
|     20 |  508 | `static int PH7_builtin_sinh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  509 |  |
|      - |  510 | `	double r,x;` |
|     21 |  511 | `	if( nArg < 1 ){` |
|      - |  512 | `		/* Missing argument,return 0 */` |
|      3 |  513 | `		ph7_result_int(pCtx,0);` |
|      3 |  514 | `		return PH7_OK;` |
|      - |  515 | `	}` |
|     19 |  516 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  517 | `	/* Perform the requested operation */` |
|     19 |  518 | `	r = sinh(x);` |
|      - |  519 | `	/* store the result back */` |
|     19 |  520 | `	ph7_result_double(pCtx,r);` |
|     19 |  521 | `	return PH7_OK;` |
|     11 |  522 |  |
|      - |  523 | `/*` |
|      - |  524 | ` * float ceil(float $arg )` |
|      - |  525 | ` *  Round fractions up.` |
|      - |  526 | ` * Parameter` |
|      - |  527 | ` *  The number to process.` |
|      - |  528 | ` * Return` |
|      - |  529 | ` *  The next highest integer value by rounding up value if necessary.` |
|      - |  530 | ` */` |
|      6 |  531 | `static int PH7_builtin_ceil(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  532 |  |
|      - |  533 | `	double r,x;` |
|      7 |  534 | `	if( nArg < 1 ){` |
|      - |  535 | `		/* Missing argument,return 0 */` |
|      5 |  536 | `		ph7_result_int(pCtx,0);` |
|      5 |  537 | `		return PH7_OK;` |
|      - |  538 | `	}` |
|      3 |  539 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  540 | `	/* Perform the requested operation */` |
|      3 |  541 | `	r = ceil(x);` |
|      - |  542 | `	/* store the result back */` |
|      3 |  543 | `	ph7_result_double(pCtx,r);` |
|      3 |  544 | `	return PH7_OK;` |
|      4 |  545 |  |
|      - |  546 | `/*` |
|      - |  547 | ` * float tan(float $arg )` |
|      - |  548 | ` *  Tangent.` |
|      - |  549 | ` * Parameter` |
|      - |  550 | ` *  The number to process.` |
|      - |  551 | ` * Return` |
|      - |  552 | ` *  The tangent of arg.` |
|      - |  553 | ` */` |
|      6 |  554 | `static int PH7_builtin_tan(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  555 |  |
|      - |  556 | `	double r,x;` |
|      7 |  557 | `	if( nArg < 1 ){` |
|      - |  558 | `		/* Missing argument,return 0 */` |
|      3 |  559 | `		ph7_result_int(pCtx,0);` |
|      3 |  560 | `		return PH7_OK;` |
|      - |  561 | `	}` |
|      5 |  562 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  563 | `	/* Perform the requested operation */` |
|      5 |  564 | `	r = tan(x);` |
|      - |  565 | `	/* store the result back */` |
|      5 |  566 | `	ph7_result_double(pCtx,r);` |
|      5 |  567 | `	return PH7_OK;` |
|      4 |  568 |  |
|      - |  569 | `/*` |
|      - |  570 | ` * float atan(float $arg )` |
|      - |  571 | ` *  Arc tangent.` |
|      - |  572 | ` * Parameter` |
|      - |  573 | ` *  The number to process.` |
|      - |  574 | ` * Return` |
|      - |  575 | ` *  The arc tangent of arg.` |
|      - |  576 | ` */` |
|     16 |  577 | `static int PH7_builtin_atan(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  578 |  |
|      - |  579 | `	double r,x;` |
|     17 |  580 | `	if( nArg < 1 ){` |
|      - |  581 | `		/* Missing argument,return 0 */` |
|      5 |  582 | `		ph7_result_int(pCtx,0);` |
|      5 |  583 | `		return PH7_OK;` |
|      - |  584 | `	}` |
|     13 |  585 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  586 | `	/* Perform the requested operation */` |
|     13 |  587 | `	r = atan(x);` |
|      - |  588 | `	/* store the result back */` |
|     13 |  589 | `	ph7_result_double(pCtx,r);` |
|     13 |  590 | `	return PH7_OK;` |
|      9 |  591 |  |
|      - |  592 | `/*` |
|      - |  593 | ` * float tanh(float $arg )` |
|      - |  594 | ` *  Hyperbolic tangent.` |
|      - |  595 | ` * Parameter` |
|      - |  596 | ` *  The number to process.` |
|      - |  597 | ` * Return` |
|      - |  598 | ` *  The Hyperbolic tangent of arg.` |
|      - |  599 | ` */` |
|     20 |  600 | `static int PH7_builtin_tanh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  601 |  |
|      - |  602 | `	double r,x;` |
|     21 |  603 | `	if( nArg < 1 ){` |
|      - |  604 | `		/* Missing argument,return 0 */` |
|      3 |  605 | `		ph7_result_int(pCtx,0);` |
|      3 |  606 | `		return PH7_OK;` |
|      - |  607 | `	}` |
|     19 |  608 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  609 | `	/* Perform the requested operation */` |
|     19 |  610 | `	r = tanh(x);` |
|      - |  611 | `	/* store the result back */` |
|     19 |  612 | `	ph7_result_double(pCtx,r);` |
|     19 |  613 | `	return PH7_OK;` |
|     11 |  614 |  |
|      - |  615 | `/*` |
|      - |  616 | ` * float atan2(float $y,float $x)` |
|      - |  617 | ` *  Arc tangent of two variable.` |
|      - |  618 | ` * Parameter` |
|      - |  619 | ` *  $y = Dividend parameter.` |
|      - |  620 | ` *  $x = Divisor parameter.` |
|      - |  621 | ` * Return` |
|      - |  622 | ` *  The arc tangent of y/x in radian.` |
|      - |  623 | ` */` |
|     10 |  624 | `static int PH7_builtin_atan2(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  625 |  |
|      - |  626 | `	double r,x,y;` |
|     11 |  627 | `	if( nArg < 2 ){` |
|      - |  628 | `		/* Missing arguments,return 0 */` |
|      5 |  629 | `		ph7_result_int(pCtx,0);` |
|      5 |  630 | `		return PH7_OK;` |
|      - |  631 | `	}` |
|      7 |  632 | `	y = ph7_value_to_double(apArg[0]);` |
|      7 |  633 | `	x = ph7_value_to_double(apArg[1]);` |
|      - |  634 | `	/* Perform the requested operation */` |
|      7 |  635 | `	r = atan2(y,x);` |
|      - |  636 | `	/* store the result back */` |
|      7 |  637 | `	ph7_result_double(pCtx,r);` |
|      7 |  638 | `	return PH7_OK;` |
|      6 |  639 |  |
|      - |  640 | `/*` |
|      - |  641 | ` * float/int64 abs(float/int64 $arg )` |
|      - |  642 | ` *  Absolute value.` |
|      - |  643 | ` * Parameter` |
|      - |  644 | ` *  The number to process.` |
|      - |  645 | ` * Return` |
|      - |  646 | ` *  The absolute value of number.` |
|      - |  647 | ` */` |
|    100 |  648 | `static int PH7_builtin_abs(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  649 |  |
|      - |  650 | `	int is_float;` |
|      - |  651 | `	/* PHP requires exactly one argument. */` |
|    102 |  652 | `	if( nArg != 1 ){` |
|     11 |  653 | `		return PH7_VmThrowException(pCtx,` |
|      - |  654 | `			"ArgumentCountError",` |
|      - |  655 | `			"abs() expects exactly 1 argument, %d given",` |
|      3 |  656 | `			nArg` |
|      - |  657 | `			);` |
|      - |  658 | `	}` |
|      - |  659 |  |
|      - |  660 | `	/* Numeric strings with decimal/exponent are treated as real values. */` |
|     96 |  661 | `	is_float = ph7_value_is_float(apArg[0]);` |
|     96 |  662 | `	if( !is_float && ph7_value_is_string(apArg[0]) ){` |
|      - |  663 | `		int len;` |
|     10 |  664 | `		sxu8 bReal = FALSE;` |
|     10 |  665 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|      - |  666 | `		sxi32 rcNum;` |
|     10 |  667 | `		rcNum = SyStrIsNumeric(zStr, len, &bReal, 0);` |
|     10 |  668 | `		if( rcNum != SXRET_OK ){` |
|      3 |  669 | `			return PH7_VmThrowException(pCtx,` |
|      - |  670 | `				"TypeError",` |
|      - |  671 | `				"abs(): Argument #1 ($num) must be of type int\|float, string given"` |
|      - |  672 | `				);` |
|      - |  673 | `		}` |
|      7 |  674 | `		if( bReal ){` |
|      5 |  675 | `			is_float = 1;` |
|      2 |  676 | `		}` |
|      3 |  677 | `	}` |
|     94 |  678 | `	if( is_float ){` |
|      - |  679 | `		double r,x;` |
|     77 |  680 | `		x = ph7_value_to_double(apArg[0]);` |
|      - |  681 | `		/* Perform the requested operation */` |
|     77 |  682 | `		r = fabs(x);` |
|     77 |  683 | `		ph7_result_double(pCtx,r);` |
|     39 |  684 | `	}else{` |
|      - |  685 | `		int r,x;` |
|     18 |  686 | `		x = ph7_value_to_int(apArg[0]);` |
|      - |  687 | `		/* Perform the requested operation */` |
|     18 |  688 | `		r = abs(x);` |
|     18 |  689 | `		ph7_result_int(pCtx,r);` |
|      - |  690 | `	}` |
|     94 |  691 | `	return PH7_OK;` |
|     52 |  692 |  |
|      - |  693 | `/*` |
|      - |  694 | ` * float log(float $arg,[int/float $base])` |
|      - |  695 | ` *  Natural logarithm.` |
|      - |  696 | ` * Parameter` |
|      - |  697 | ` *  $arg: The number to process.` |
|      - |  698 | ` *  $base: The optional logarithmic base to use. (only base-10 is supported)` |
|      - |  699 | ` * Return` |
|      - |  700 | ` *  The logarithm of arg to base, if given, or the natural logarithm.` |
|      - |  701 | ` * Note:` |
|      - |  702 | ` *  only Natural log and base-10 log are supported.` |
|      - |  703 | ` */` |
|     14 |  704 | `static int PH7_builtin_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  705 |  |
|      - |  706 | `	double r,x;` |
|     15 |  707 | `	if( nArg < 1 ){` |
|      - |  708 | `		/* Missing argument,return 0 */` |
|      3 |  709 | `		ph7_result_int(pCtx,0);` |
|      3 |  710 | `		return PH7_OK;` |
|      - |  711 | `	}` |
|     13 |  712 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  713 | `	/* Perform the requested operation */` |
|     13 |  714 | `	if( nArg == 2 && ph7_value_is_numeric(apArg[1]) && ph7_value_to_int(apArg[1]) == 10 ){` |
|      - |  715 | `		/* Base-10 log */` |
|      5 |  716 | `		r = log10(x);` |
|      3 |  717 | `	}else{` |
|      9 |  718 | `		r = log(x);` |
|      - |  719 | `	}` |
|      - |  720 | `	/* store the result back */` |
|     13 |  721 | `	ph7_result_double(pCtx,r);` |
|     13 |  722 | `	return PH7_OK;` |
|      8 |  723 |  |
|      - |  724 | `/*` |
|      - |  725 | ` * float log10(float $arg )` |
|      - |  726 | ` *  Base-10 logarithm.` |
|      - |  727 | ` * Parameter` |
|      - |  728 | ` *  The number to process.` |
|      - |  729 | ` * Return` |
|      - |  730 | ` *  The Base-10 logarithm of the given number.` |
|      - |  731 | ` */` |
|     16 |  732 | `static int PH7_builtin_log10(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  733 |  |
|      - |  734 | `	double r,x;` |
|     17 |  735 | `	if( nArg < 1 ){` |
|      - |  736 | `		/* Missing argument,return 0 */` |
|      3 |  737 | `		ph7_result_int(pCtx,0);` |
|      3 |  738 | `		return PH7_OK;` |
|      - |  739 | `	}` |
|     15 |  740 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  741 | `	/* Perform the requested operation */` |
|     15 |  742 | `	r = log10(x);` |
|      - |  743 | `	/* store the result back */` |
|     15 |  744 | `	ph7_result_double(pCtx,r);` |
|     15 |  745 | `	return PH7_OK;` |
|      9 |  746 |  |
|      - |  747 | `/*` |
|      - |  748 | ` * number pow(number $base,number $exp)` |
|      - |  749 | ` *  Exponential expression.` |
|      - |  750 | ` * Parameter` |
|      - |  751 | ` *  base` |
|      - |  752 | ` *  The base to use.` |
|      - |  753 | ` * exp` |
|      - |  754 | ` *  The exponent.` |
|      - |  755 | ` * Return` |
|      - |  756 | ` *  base raised to the power of exp.` |
|      - |  757 | ` *  If the result can be represented as integer it will be returned` |
|      - |  758 | ` *  as type integer, else it will be returned as type float.` |
|      - |  759 | ` */` |
|      8 |  760 | `static int PH7_builtin_pow(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  761 |  |
|      - |  762 | `	double r,x,y;` |
|      9 |  763 | `	if( nArg < 1 ){` |
|      - |  764 | `		/* Missing argument,return 0 */` |
|      5 |  765 | `		ph7_result_int(pCtx,0);` |
|      5 |  766 | `		return PH7_OK;` |
|      - |  767 | `	}` |
|      5 |  768 | `	x = ph7_value_to_double(apArg[0]);` |
|      5 |  769 | `	y = ph7_value_to_double(apArg[1]);` |
|      - |  770 | `	/* Perform the requested operation */` |
|      5 |  771 | `	r = pow(x,y);` |
|      5 |  772 | `	ph7_result_double(pCtx,r);` |
|      5 |  773 | `	return PH7_OK;` |
|      5 |  774 |  |
|      - |  775 | `/*` |
|      - |  776 | ` * float pi(void)` |
|      - |  777 | ` *  Returns an approximation of pi.` |
|      - |  778 | ` * Note` |
|      - |  779 | ` *  you can use the M_PI constant which yields identical results to pi().` |
|      - |  780 | ` * Return` |
|      - |  781 | ` *  The value of pi as float.` |
|      - |  782 | ` */` |
|      2 |  783 | `static int PH7_builtin_pi(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  784 |  |
|      1 |  785 | `	SXUNUSED(nArg); /* cc warning */` |
|      1 |  786 | `	SXUNUSED(apArg);` |
|      3 |  787 | `	ph7_result_double(pCtx,PH7_PI);` |
|      3 |  788 | `	return PH7_OK;` |
|      1 |  789 |  |
|      - |  790 | `/*` |
|      - |  791 | ` * float fmod(float $x,float $y)` |
|      - |  792 | ` *  Returns the floating point remainder (modulo) of the division of the arguments.` |
|      - |  793 | ` * Parameters` |
|      - |  794 | ` * $x` |
|      - |  795 | ` *  The dividend` |
|      - |  796 | ` * $y` |
|      - |  797 | ` *  The divisor` |
|      - |  798 | ` * Return` |
|      - |  799 | ` *  The floating point remainder of x/y.` |
|      - |  800 | ` */` |
|      8 |  801 | `static int PH7_builtin_fmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  802 |  |
|      - |  803 | `	double x,y,r;` |
|      9 |  804 | `	if( nArg < 2 ){` |
|      - |  805 | `		/* Missing arguments */` |
|      7 |  806 | `		ph7_result_double(pCtx,0);` |
|      7 |  807 | `		return PH7_OK;` |
|      - |  808 | `	}` |
|      - |  809 | `	/* Extract given arguments */` |
|      3 |  810 | `	x = ph7_value_to_double(apArg[0]);` |
|      3 |  811 | `	y = ph7_value_to_double(apArg[1]);` |
|      - |  812 | `	/* Perform the requested operation */` |
|      3 |  813 | `	r = fmod(x,y);` |
|      - |  814 | `	/* Processing result */` |
|      3 |  815 | `	ph7_result_double(pCtx,r);` |
|      3 |  816 | `	return PH7_OK;` |
|      5 |  817 |  |
|      - |  818 | `/*` |
|      - |  819 | ` * float hypot(float $x,float $y)` |
|      - |  820 | ` *  Calculate the length of the hypotenuse of a right-angle triangle .` |
|      - |  821 | ` * Parameters` |
|      - |  822 | ` * $x` |
|      - |  823 | ` *  Length of first side` |
|      - |  824 | ` * $y` |
|      - |  825 | ` *  Length of first side` |
|      - |  826 | ` * Return` |
|      - |  827 | ` *  Calculated length of the hypotenuse.` |
|      - |  828 | ` */` |
|      6 |  829 | `static int PH7_builtin_hypot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  830 |  |
|      - |  831 | `	double x,y,r;` |
|      7 |  832 | `	if( nArg < 2 ){` |
|      - |  833 | `		/* Missing arguments */` |
|      5 |  834 | `		ph7_result_double(pCtx,0);` |
|      5 |  835 | `		return PH7_OK;` |
|      - |  836 | `	}` |
|      - |  837 | `	/* Extract given arguments */` |
|      3 |  838 | `	x = ph7_value_to_double(apArg[0]);` |
|      3 |  839 | `	y = ph7_value_to_double(apArg[1]);` |
|      - |  840 | `	/* Perform the requested operation */` |
|      3 |  841 | `	r = hypot(x,y);` |
|      - |  842 | `	/* Processing result */` |
|      3 |  843 | `	ph7_result_double(pCtx,r);` |
|      3 |  844 | `	return PH7_OK;` |
|      4 |  845 |  |
|      - |  846 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - |  847 | `/*` |
|      - |  848 | ` * float round ( float $val [, int $precision = 0 [, int $mode = PHP_ROUND_HALF_UP ]] )` |
|      - |  849 | ` *  Exponential expression.` |
|      - |  850 | ` * Parameter` |
|      - |  851 | ` *  $val` |
|      - |  852 | ` *   The value to round.` |
|      - |  853 | ` * $precision` |
|      - |  854 | ` *   The optional number of decimal digits to round to.` |
|      - |  855 | ` * $mode` |
|      - |  856 | ` *   One of PHP_ROUND_HALF_UP, PHP_ROUND_HALF_DOWN, PHP_ROUND_HALF_EVEN, or PHP_ROUND_HALF_ODD.` |
|      - |  857 | ` *   (not supported).` |
|      - |  858 | ` * Return` |
|      - |  859 | ` *  The rounded value.` |
|      - |  860 | ` */` |
|     20 |  861 | `static int PH7_builtin_round(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  862 |  |
|     21 |  863 | `	int n = 0;` |
|      - |  864 | `	double r;` |
|     21 |  865 | `	if( nArg < 1 ){` |
|      - |  866 | `		/* Missing argument,return 0 */` |
|      5 |  867 | `		ph7_result_int(pCtx,0);` |
|      5 |  868 | `		return PH7_OK;` |
|      - |  869 | `	}` |
|      - |  870 | `	/* Extract the precision if available */` |
|     17 |  871 | `	if( nArg > 1 ){` |
|      5 |  872 | `		n = ph7_value_to_int(apArg[1]);` |
|      5 |  873 | `		if( n>30 ){` |
|      3 |  874 | `			n = 30;` |
|      1 |  875 | `		}` |
|      5 |  876 | `		if( n<0 ){` |
|      3 |  877 | `			n = 0;` |
|      1 |  878 | `		}` |
|      2 |  879 | `	}` |
|     17 |  880 | `	r = ph7_value_to_double(apArg[0]);` |
|      - |  881 | `	/* If Y==0 and X will fit in a 64-bit int,` |
|      - |  882 | `     * handle the rounding directly.Otherwise` |
|      - |  883 | `	 * use our own cutsom printf [i.e:SyBufferFormat()].` |
|      - |  884 | `     */` |
|     17 |  885 | `	if( n==0 && r>=0 && r < (double)(LARGEST_INT64-1) ){` |
|     13 |  886 | `    r = (double)((ph7_int64)(r+0.5));` |
|     11 |  887 | `	}else if( n==0 && r<0 && (-r) < (double)(LARGEST_INT64-1) ){` |
|      3 |  888 | `    r = -(double)((ph7_int64)((-r)+0.5));` |
|      2 |  889 | `  }else{` |
|      - |  890 | `	  char zBuf[256];` |
|      - |  891 | `	  sxu32 nLen;` |
|      3 |  892 | `	  nLen = SyBufferFormat(zBuf,sizeof(zBuf),"%.*f",n,r);` |
|      - |  893 | `	  /* Convert the string to real number */` |
|      3 |  894 | `	  SyStrToReal(zBuf,nLen,(void *)&r,0);` |
|      - |  895 | `  }` |
|      - |  896 | `  /* Return thr rounded value */` |
|     17 |  897 | `  ph7_result_double(pCtx,r);` |
|     17 |  898 | `  return PH7_OK;` |
|     11 |  899 |  |
|      - |  900 | `/*` |
|      - |  901 | ` * string dechex(int $number)` |
|      - |  902 | ` *  Decimal to hexadecimal.` |
|      - |  903 | ` * Parameters` |
|      - |  904 | ` *  $number` |
|      - |  905 | ` *   Decimal value to convert` |
|      - |  906 | ` * Return` |
|      - |  907 | ` *  Hexadecimal string representation of number` |
|      - |  908 | ` */` |
|      6 |  909 | `static int PH7_builtin_dechex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  910 |  |
|      - |  911 | `	int iVal;` |
|      7 |  912 | `	if( nArg < 1 ){` |
|      - |  913 | `		/* Missing arguments,return null */` |
|      5 |  914 | `		ph7_result_null(pCtx);` |
|      5 |  915 | `		return PH7_OK;` |
|      - |  916 | `	}` |
|      - |  917 | `	/* Extract the given number */` |
|      3 |  918 | `	iVal = ph7_value_to_int(apArg[0]);` |
|      - |  919 | `	/* Format */` |
|      3 |  920 | `	ph7_result_string_format(pCtx,"%x",iVal);` |
|      3 |  921 | `	return PH7_OK;` |
|      4 |  922 |  |
|      - |  923 | `/*` |
|      - |  924 | ` * string decoct(int $number)` |
|      - |  925 | ` *  Decimal to Octal.` |
|      - |  926 | ` * Parameters` |
|      - |  927 | ` *  $number` |
|      - |  928 | ` *   Decimal value to convert` |
|      - |  929 | ` * Return` |
|      - |  930 | ` *  Octal string representation of number` |
|      - |  931 | ` */` |
|      8 |  932 | `static int PH7_builtin_decoct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  933 |  |
|      - |  934 | `	int iVal;` |
|      9 |  935 | `	if( nArg < 1 ){` |
|      - |  936 | `		/* Missing arguments,return null */` |
|      3 |  937 | `		ph7_result_null(pCtx);` |
|      3 |  938 | `		return PH7_OK;` |
|      - |  939 | `	}` |
|      - |  940 | `	/* Extract the given number */` |
|      7 |  941 | `	iVal = ph7_value_to_int(apArg[0]);` |
|      - |  942 | `	/* Format */` |
|      7 |  943 | `	ph7_result_string_format(pCtx,"%o",iVal);` |
|      7 |  944 | `	return PH7_OK;` |
|      5 |  945 |  |
|      - |  946 | `/*` |
|      - |  947 | ` * string decbin(int $number)` |
|      - |  948 | ` *  Decimal to binary.` |
|      - |  949 | ` * Parameters` |
|      - |  950 | ` *  $number` |
|      - |  951 | ` *   Decimal value to convert` |
|      - |  952 | ` * Return` |
|      - |  953 | ` *  Binary string representation of number` |
|      - |  954 | ` */` |
|      4 |  955 | `static int PH7_builtin_decbin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  956 |  |
|      - |  957 | `	int iVal;` |
|      5 |  958 | `	if( nArg < 1 ){` |
|      - |  959 | `		/* Missing arguments,return null */` |
|      3 |  960 | `		ph7_result_null(pCtx);` |
|      3 |  961 | `		return PH7_OK;` |
|      - |  962 | `	}` |
|      - |  963 | `	/* Extract the given number */` |
|      3 |  964 | `	iVal = ph7_value_to_int(apArg[0]);` |
|      - |  965 | `	/* Format */` |
|      3 |  966 | `	ph7_result_string_format(pCtx,"%B",iVal);` |
|      3 |  967 | `	return PH7_OK;` |
|      3 |  968 |  |
|      - |  969 | `/*` |
|      - |  970 | ` * int64 hexdec(string $hex_string)` |
|      - |  971 | ` *  Hexadecimal to decimal.` |
|      - |  972 | ` * Parameters` |
|      - |  973 | ` *  $hex_string` |
|      - |  974 | ` *   The hexadecimal string to convert` |
|      - |  975 | ` * Return` |
|      - |  976 | ` *  The decimal representation of hex_string` |
|      - |  977 | ` */` |
|     24 |  978 | `static int PH7_builtin_hexdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  979 |  |
|      - |  980 | `	const char *zString,*zEnd;` |
|      - |  981 | `	ph7_int64 iVal;` |
|      - |  982 | `	int nLen;` |
|     25 |  983 | `	if( nArg < 1 ){` |
|      - |  984 | `		/* Missing arguments,return -1 */` |
|      5 |  985 | `		ph7_result_int(pCtx,-1);` |
|      5 |  986 | `		return PH7_OK;` |
|      - |  987 | `	}` |
|     21 |  988 | `	iVal = 0;` |
|     21 |  989 | `	if( ph7_value_is_string(apArg[0]) ){` |
|      - |  990 | `		/* Extract the given string */` |
|     15 |  991 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|      - |  992 | `		/* Delimit the string */` |
|     15 |  993 | `		zEnd = &zString[nLen];` |
|      - |  994 | `		/* Ignore non hex-stream */` |
|     21 |  995 | `		while( zString < zEnd ){` |
|     21 |  996 | `			if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - |  997 | `				/* UTF-8 stream */` |
|      5 |  998 | `				zString++;` |
|      9 |  999 | `				while( zString < zEnd && (((unsigned char)zString[0] & 0xc0) == 0x80) ){` |
|      5 | 1000 | `					zString++;` |
|      1 | 1001 | `				}` |
|      3 | 1002 | `			}else{` |
|     17 | 1003 | `				if( SyisHex(zString[0]) ){` |
|     15 | 1004 | `					break;` |
|      - | 1005 | `				}` |
|      - | 1006 | `				/* Ignore */` |
|      3 | 1007 | `				zString++;` |
|      - | 1008 | `			}` |
|      1 | 1009 | `		}` |
|     15 | 1010 | `		if( zString < zEnd ){` |
|      - | 1011 | `			/* Cast */` |
|     15 | 1012 | `			SyHexStrToInt64(zString,(sxu32)(zEnd-zString),(void *)&iVal,0);` |
|      7 | 1013 | `		}` |
|      8 | 1014 | `	}else{` |
|      - | 1015 | `		/* Extract as a 64-bit integer */` |
|      7 | 1016 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|      - | 1017 | `	}` |
|      - | 1018 | `	/* Return the number */` |
|     21 | 1019 | `	ph7_result_int64(pCtx,iVal);` |
|     21 | 1020 | `	return PH7_OK;` |
|     13 | 1021 |  |
|      - | 1022 | `/*` |
|      - | 1023 | ` * int64 bindec(string $bin_string)` |
|      - | 1024 | ` *  Binary to decimal.` |
|      - | 1025 | ` * Parameters` |
|      - | 1026 | ` *  $bin_string` |
|      - | 1027 | ` *   The binary string to convert` |
|      - | 1028 | ` * Return` |
|      - | 1029 | ` *  Returns the decimal equivalent of the binary number represented by the binary_string argument.` |
|      - | 1030 | ` */` |
|     12 | 1031 | `static int PH7_builtin_bindec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1032 |  |
|      - | 1033 | `	const char *zString;` |
|      - | 1034 | `	ph7_int64 iVal;` |
|      - | 1035 | `	int nLen;` |
|     13 | 1036 | `	if( nArg < 1 ){` |
|      - | 1037 | `		/* Missing arguments,return -1 */` |
|      5 | 1038 | `		ph7_result_int(pCtx,-1);` |
|      5 | 1039 | `		return PH7_OK;` |
|      - | 1040 | `	}` |
|      9 | 1041 | `	iVal = 0;` |
|      9 | 1042 | `	if( ph7_value_is_string(apArg[0]) ){` |
|      - | 1043 | `		/* Extract the given string */` |
|      5 | 1044 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 1045 | `		if( nLen > 0 ){` |
|      - | 1046 | `			/* Perform a binary cast */` |
|      5 | 1047 | `			SyBinaryStrToInt64(zString,(sxu32)nLen,(void *)&iVal,0);` |
|      2 | 1048 | `		}` |
|      3 | 1049 | `	}else{` |
|      - | 1050 | `		/* Extract as a 64-bit integer */` |
|      5 | 1051 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|      - | 1052 | `	}` |
|      - | 1053 | `	/* Return the number */` |
|      9 | 1054 | `	ph7_result_int64(pCtx,iVal);` |
|      9 | 1055 | `	return PH7_OK;` |
|      7 | 1056 |  |
|      - | 1057 | `/*` |
|      - | 1058 | ` * int64 octdec(string $oct_string)` |
|      - | 1059 | ` *  Octal to decimal.` |
|      - | 1060 | ` * Parameters` |
|      - | 1061 | ` *  $oct_string` |
|      - | 1062 | ` *   The octal string to convert` |
|      - | 1063 | ` * Return` |
|      - | 1064 | ` *  Returns the decimal equivalent of the octal number represented by the octal_string argument.` |
|      - | 1065 | ` */` |
|      6 | 1066 | `static int PH7_builtin_octdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1067 |  |
|      - | 1068 | `	const char *zString;` |
|      - | 1069 | `	ph7_int64 iVal;` |
|      - | 1070 | `	int nLen;` |
|      7 | 1071 | `	if( nArg < 1 ){` |
|      - | 1072 | `		/* Missing arguments,return -1 */` |
|      3 | 1073 | `		ph7_result_int(pCtx,-1);` |
|      3 | 1074 | `		return PH7_OK;` |
|      - | 1075 | `	}` |
|      5 | 1076 | `	iVal = 0;` |
|      5 | 1077 | `	if( ph7_value_is_string(apArg[0]) ){` |
|      - | 1078 | `		/* Extract the given string */` |
|      3 | 1079 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 1080 | `		if( nLen > 0 ){` |
|      - | 1081 | `			/* Perform the cast */` |
|      3 | 1082 | `			SyOctalStrToInt64(zString,(sxu32)nLen,(void *)&iVal,0);` |
|      1 | 1083 | `		}` |
|      2 | 1084 | `	}else{` |
|      - | 1085 | `		/* Extract as a 64-bit integer */` |
|      3 | 1086 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|      - | 1087 | `	}` |
|      - | 1088 | `	/* Return the number */` |
|      5 | 1089 | `	ph7_result_int64(pCtx,iVal);` |
|      5 | 1090 | `	return PH7_OK;` |
|      4 | 1091 |  |
|      - | 1092 | `/*` |
|      - | 1093 | ` * srand([int $seed])` |
|      - | 1094 | ` * mt_srand([int $seed])` |
|      - | 1095 | ` *  Seed the random number generator.` |
|      - | 1096 | ` * Parameters` |
|      - | 1097 | ` * $seed` |
|      - | 1098 | ` *  Optional seed value` |
|      - | 1099 | ` * Return` |
|      - | 1100 | ` *  null.` |
|      - | 1101 | ` * Note:` |
|      - | 1102 | ` *  THIS FUNCTION IS A NO-OP.` |
|      - | 1103 | ` *  THE PH7 PRNG IS AUTOMATICALLY SEEDED WHEN THE VM IS CREATED.` |
|      - | 1104 | ` */` |
|     20 | 1105 | `static int PH7_builtin_srand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1106 |  |
|     10 | 1107 | `	SXUNUSED(nArg);` |
|     10 | 1108 | `	SXUNUSED(apArg);` |
|     21 | 1109 | `	ph7_result_null(pCtx);` |
|     21 | 1110 | `	return PH7_OK;` |
|      1 | 1111 |  |
|      - | 1112 | `/*` |
|      - | 1113 | ` * string base_convert(string $number,int $frombase,int $tobase)` |
|      - | 1114 | ` *  Convert a number between arbitrary bases.` |
|      - | 1115 | ` * Parameters` |
|      - | 1116 | ` * $number` |
|      - | 1117 | ` *  The number to convert` |
|      - | 1118 | ` * $frombase` |
|      - | 1119 | ` *  The base number is in` |
|      - | 1120 | ` * $tobase` |
|      - | 1121 | ` *  The base to convert number to` |
|      - | 1122 | ` * Return` |
|      - | 1123 | ` *  Number converted to base tobase` |
|      - | 1124 | ` */` |
|      - | 1125 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 1126 | `#ifdef PH7_NEED_FMT_AND_INI` |
|     48 | 1127 | `static int PH7_builtin_base_convert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      - | 1128 |  |
|      - | 1129 |  |
|      1 | 1130 |  |
|      - | 1131 | `	int nLen,iFbase,iTobase;` |
|      - | 1132 | `	const char *zNum;` |
|      - | 1133 | `	ph7_int64 iNum;` |
|     49 | 1134 | `	if( nArg < 3 ){` |
|      - | 1135 | `		/* Return the empty string*/` |
|     13 | 1136 | `		ph7_result_string(pCtx,"",0);` |
|     13 | 1137 | `		return PH7_OK;` |
|      - | 1138 | `	}` |
|      - | 1139 | `	/* Base numbers */` |
|     37 | 1140 | `	iFbase  = ph7_value_to_int(apArg[1]);` |
|     37 | 1141 | `	iTobase = ph7_value_to_int(apArg[2]);` |
|     37 | 1142 | `	if( ph7_value_is_string(apArg[0]) ){` |
|      - | 1143 | `		/* Extract the target number */` |
|     29 | 1144 | `		zNum = ph7_value_to_string(apArg[0],&nLen);` |
|     29 | 1145 | `		if( nLen < 1 ){` |
|      - | 1146 | `			/* Return the empty string*/` |
|    ! 0 | 1147 | `			ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1148 | `			return PH7_OK;` |
|      - | 1149 | `		}` |
|      - | 1150 | `		/* Base conversion */` |
|     29 | 1151 | `		switch(iFbase){` |
|      5 | 1152 | `		case 16:` |
|      - | 1153 | `			/* Hex */` |
|     11 | 1154 | `			SyHexStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|     11 | 1155 | `			break;` |
|      3 | 1156 | `		case 8:` |
|      - | 1157 | `			/* Octal */` |
|      7 | 1158 | `			SyOctalStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|      7 | 1159 | `			break;` |
|      2 | 1160 | `		case 2:` |
|      - | 1161 | `			/* Binary */` |
|      5 | 1162 | `			SyBinaryStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|      5 | 1163 | `			break;` |
|      4 | 1164 | `		default:` |
|      - | 1165 | `			/* Decimal */` |
|      9 | 1166 | `			SyStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|      8 | 1167 | `			break;` |
|      - | 1168 | `		}` |
|     15 | 1169 | `	}else{` |
|      9 | 1170 | `		iNum = ph7_value_to_int64(apArg[0]);` |
|      - | 1171 | `	}` |
|     37 | 1172 | `	switch(iTobase){` |
|      4 | 1173 | `	case 16:` |
|      - | 1174 | `		/* Hex */` |
|      9 | 1175 | `		ph7_result_string_format(pCtx,"%qx",iNum); /* Quad hex */` |
|      9 | 1176 | `		break;` |
|      1 | 1177 | `	case 8:` |
|      - | 1178 | `		/* Octal */` |
|      3 | 1179 | `		ph7_result_string_format(pCtx,"%qo",iNum); /* Quad octal */` |
|      3 | 1180 | `		break;` |
|      1 | 1181 | `	case 2:` |
|      - | 1182 | `		/* Binary */` |
|      3 | 1183 | `		ph7_result_string_format(pCtx,"%qB",iNum); /* Quad binary */` |
|      3 | 1184 | `		break;` |
|     12 | 1185 | `	default:` |
|      - | 1186 | `		/* Decimal */` |
|     25 | 1187 | `		ph7_result_string_format(pCtx,"%qd",iNum); /* Quad decimal */` |
|     24 | 1188 | `		break;` |
|      - | 1189 | `	}` |
|     37 | 1190 | `	return PH7_OK;` |
|     25 | 1191 |  |
|      - | 1192 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 1193 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 1194 | `/*` |
|      - | 1195 | ` * Section:` |
|      - | 1196 | ` *    String handling Functions.` |
|      - | 1197 | ` * Status:` |
|      - | 1198 | ` *    Stable.` |
|      - | 1199 | ` */` |
|      - | 1200 | `/*` |
|      - | 1201 | ` * string substr(string $string,int $start[, int $length ])` |
|      - | 1202 | ` *  Return part of a string.` |
|      - | 1203 | ` * Parameters` |
|      - | 1204 | ` *  $string` |
|      - | 1205 | ` *   The input string. Must be one character or longer.` |
|      - | 1206 | ` * $start` |
|      - | 1207 | ` *   If start is non-negative, the returned string will start at the start'th position` |
|      - | 1208 | ` *   in string, counting from zero. For instance, in the string 'abcdef', the character` |
|      - | 1209 | ` *   at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 1210 | ` *   If start is negative, the returned string will start at the start'th character` |
|      - | 1211 | ` *   from the end of string.` |
|      - | 1212 | ` *   If string is less than or equal to start characters long, FALSE will be returned.` |
|      - | 1213 | ` * $length` |
|      - | 1214 | ` *   If length is given and is positive, the string returned will contain at most length` |
|      - | 1215 | ` *   characters beginning from start (depending on the length of string).` |
|      - | 1216 | ` *   If length is given and is negative, then that many characters will be omitted from` |
|      - | 1217 | ` *   the end of string (after the start position has been calculated when a start is negative).` |
|      - | 1218 | ` *   If start denotes the position of this truncation or beyond, false will be returned.` |
|      - | 1219 | ` *   If length is given and is 0, FALSE or NULL an empty string will be returned.` |
|      - | 1220 | ` *   If length is omitted, the substring starting from start until the end of the string` |
|      - | 1221 | ` *   will be returned.` |
|      - | 1222 | ` * Return` |
|      - | 1223 | ` *  Returns the extracted part of string, or FALSE on failure or an empty string.` |
|      - | 1224 | ` */` |
| 109558 | 1225 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1226 |  |
|      - | 1227 | `	const char *zSource,*zOfft;` |
|      - | 1228 | `	int nOfft,nLen,nSrcLen;` |
| 109560 | 1229 | `	if( nArg < 2 ){` |
|      - | 1230 | `		/* return FALSE */` |
|      5 | 1231 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1232 | `		return PH7_OK;` |
|      - | 1233 | `	}` |
|      - | 1234 | `	/* Extract the target string */` |
| 109556 | 1235 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 109556 | 1236 | `	if( nSrcLen < 1 ){` |
|      - | 1237 | `		/* Empty string,return FALSE */` |
|   6918 | 1238 | `		ph7_result_bool(pCtx,0);` |
|   6918 | 1239 | `		return PH7_OK;` |
|      - | 1240 | `	}` |
| 102640 | 1241 | `	nLen = nSrcLen; /* cc warning */` |
|      - | 1242 | `	/* Extract the offset */` |
| 102640 | 1243 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 102640 | 1244 | `	if( nOfft < 0 ){` |
|  16952 | 1245 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  16952 | 1246 | `		if( zOfft < zSource ){` |
|      - | 1247 | `			/* Invalid offset */` |
|      5 | 1248 | `			ph7_result_bool(pCtx,0);` |
|      5 | 1249 | `			return PH7_OK;` |
|      - | 1250 | `		}` |
|  16948 | 1251 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  16948 | 1252 | `		nOfft = (int)(zOfft-zSource);` |
|  94163 | 1253 | `	}else if( nOfft >= nSrcLen ){` |
|      - | 1254 | `		/* Invalid offset */` |
|      7 | 1255 | `		ph7_result_bool(pCtx,0);` |
|      7 | 1256 | `		return PH7_OK;` |
|    ! 0 | 1257 | `	}else{` |
|  85684 | 1258 | `		zOfft = &zSource[nOfft];` |
|  85684 | 1259 | `		nLen = nSrcLen - nOfft;` |
|      - | 1260 | `	}` |
| 102630 | 1261 | `	if( nArg > 2 ){` |
|      - | 1262 | `		/* Extract the length */` |
|  85682 | 1263 | `		nLen = ph7_value_to_int(apArg[2]);` |
|  85682 | 1264 | `		if( nLen == 0 ){` |
|      - | 1265 | `			/* Invalid length,return an empty string */` |
|      5 | 1266 | `			ph7_result_string(pCtx,"",0);` |
|      5 | 1267 | `			return PH7_OK;` |
|  85678 | 1268 | `		}else if( nLen < 0 ){` |
|  16950 | 1269 | `			nLen = nSrcLen + nLen - nOfft;` |
|  16950 | 1270 | `			if( nLen < 1 ){` |
|      - | 1271 | `				/* Invalid  length */` |
|      3 | 1272 | `				nLen = nSrcLen - nOfft;` |
|      1 | 1273 | `			}` |
|   8474 | 1274 | `		}` |
|  85678 | 1275 | `		if( nLen + nOfft > nSrcLen ){` |
|      - | 1276 | `			/* Invalid length */` |
|   2164 | 1277 | `			nLen = nSrcLen - nOfft;` |
|   1081 | 1278 | `		}` |
|  42838 | 1279 | `	}` |
|      - | 1280 | `	/* Return the substring */` |
| 102626 | 1281 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 102626 | 1282 | `	return PH7_OK;` |
|  54781 | 1283 |  |
|      - | 1284 | `/*` |
|      - | 1285 | ` * int substr_compare(string $main_str,string $str ,int $offset[,int $length[,bool $case_insensitivity = false ]])` |
|      - | 1286 | ` *  Binary safe comparison of two strings from an offset, up to length characters.` |
|      - | 1287 | ` * Parameters` |
|      - | 1288 | ` *  $main_str` |
|      - | 1289 | ` *  The main string being compared.` |
|      - | 1290 | ` *  $str` |
|      - | 1291 | ` *   The secondary string being compared.` |
|      - | 1292 | ` * $offset` |
|      - | 1293 | ` *  The start position for the comparison. If negative, it starts counting from` |
|      - | 1294 | ` *  the end of the string.` |
|      - | 1295 | ` * $length` |
|      - | 1296 | ` *  The length of the comparison. The default value is the largest of the length` |
|      - | 1297 | ` *  of the str compared to the length of main_str less the offset.` |
|      - | 1298 | ` * $case_insensitivity` |
|      - | 1299 | ` *  If case_insensitivity is TRUE, comparison is case insensitive.` |
|      - | 1300 | ` * Return` |
|      - | 1301 | ` *  Returns < 0 if main_str from position offset is less than str, > 0 if it is greater than` |
|      - | 1302 | ` *  str, and 0 if they are equal. If offset is equal to or greater than the length of main_str` |
|      - | 1303 | ` *  or length is set and is less than 1, substr_compare() prints a warning and returns FALSE.` |
|      - | 1304 | ` */` |
|     26 | 1305 | `static int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1306 |  |
|      - | 1307 | `	const char *zSource,*zOfft,*zSub;` |
|      - | 1308 | `	int nOfft,nLen,nSrcLen,nSublen;` |
|     27 | 1309 | `	int iCase = 0;` |
|      - | 1310 | `	int rc;` |
|     27 | 1311 | `	if( nArg < 3 ){` |
|      - | 1312 | `		/* Missing arguments,return FALSE */` |
|      5 | 1313 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1314 | `		return PH7_OK;` |
|      - | 1315 | `	}` |
|      - | 1316 | `	/* Extract the target string */` |
|     23 | 1317 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|     23 | 1318 | `	if( nSrcLen < 1 ){` |
|      - | 1319 | `		/* Empty string,return FALSE */` |
|      3 | 1320 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1321 | `		return PH7_OK;` |
|      - | 1322 | `	}` |
|     21 | 1323 | `	nLen = nSrcLen; /* cc warning */` |
|      - | 1324 | `	/* Extract the substring */` |
|     21 | 1325 | `	zSub = ph7_value_to_string(apArg[1],&nSublen);` |
|     21 | 1326 | `	if( nSublen < 1 \|\| nSublen > nSrcLen){` |
|      - | 1327 | `		/* Empty string,return FALSE */` |
|      3 | 1328 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1329 | `		return PH7_OK;` |
|      - | 1330 | `	}` |
|      - | 1331 | `	/* Extract the offset */` |
|     19 | 1332 | `	nOfft = ph7_value_to_int(apArg[2]);` |
|     19 | 1333 | `	if( nOfft < 0 ){` |
|      5 | 1334 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|      5 | 1335 | `		if( zOfft < zSource ){` |
|      - | 1336 | `			/* Invalid offset */` |
|      3 | 1337 | `			ph7_result_bool(pCtx,0);` |
|      3 | 1338 | `			return PH7_OK;` |
|      - | 1339 | `		}` |
|      3 | 1340 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|      3 | 1341 | `		nOfft = (int)(zOfft-zSource);` |
|     16 | 1342 | `	}else if( nOfft >= nSrcLen ){` |
|      - | 1343 | `		/* Invalid offset */` |
|      3 | 1344 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1345 | `		return PH7_OK;` |
|    ! 0 | 1346 | `	}else{` |
|     13 | 1347 | `		zOfft = &zSource[nOfft];` |
|     13 | 1348 | `		nLen = nSrcLen - nOfft;` |
|      - | 1349 | `	}` |
|     15 | 1350 | `	if( nArg > 3 ){` |
|      - | 1351 | `		/* Extract the length */` |
|     13 | 1352 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     13 | 1353 | `		if( nLen < 1 ){` |
|      - | 1354 | `			/* Invalid  length */` |
|      5 | 1355 | `			ph7_result_int(pCtx,1);` |
|      5 | 1356 | `			return PH7_OK;` |
|      9 | 1357 | `		}else if( nLen + nOfft > nSrcLen ){` |
|      - | 1358 | `			/* Invalid length */` |
|      3 | 1359 | `			nLen = nSrcLen - nOfft;` |
|      1 | 1360 | `		}` |
|      9 | 1361 | `		if( nArg > 4 ){` |
|      - | 1362 | `			/* Case-sensitive or not */` |
|      5 | 1363 | `			iCase = ph7_value_to_bool(apArg[4]);` |
|      2 | 1364 | `		}` |
|      4 | 1365 | `	}` |
|      - | 1366 | `	/* Perform the comparison */` |
|     11 | 1367 | `	if( iCase ){` |
|      3 | 1368 | `		rc = SyStrnicmp(zOfft,zSub,(sxu32)nLen);` |
|      2 | 1369 | `	}else{` |
|      9 | 1370 | `		rc = SyStrncmp(zOfft,zSub,(sxu32)nLen);` |
|      - | 1371 | `	}` |
|      - | 1372 | `	/* Comparison result */` |
|     11 | 1373 | `	ph7_result_int(pCtx,rc);` |
|     11 | 1374 | `	return PH7_OK;` |
|     14 | 1375 |  |
|      - | 1376 | `/*` |
|      - | 1377 | ` * int substr_count(string $haystack,string $needle[,int $offset = 0 [,int $length ]])` |
|      - | 1378 | ` *  Count the number of substring occurrences.` |
|      - | 1379 | ` * Parameters` |
|      - | 1380 | ` * $haystack` |
|      - | 1381 | ` *   The string to search in` |
|      - | 1382 | ` * $needle` |
|      - | 1383 | ` *   The substring to search for` |
|      - | 1384 | ` * $offset` |
|      - | 1385 | ` *  The offset where to start counting` |
|      - | 1386 | ` * $length (NOT USED)` |
|      - | 1387 | ` *  The maximum length after the specified offset to search for the substring.` |
|      - | 1388 | ` *  It outputs a warning if the offset plus the length is greater than the haystack length.` |
|      - | 1389 | ` * Return` |
|      - | 1390 | ` *  Toral number of substring occurrences.` |
|      - | 1391 | ` */` |
|     24 | 1392 | `static int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1393 |  |
|      - | 1394 | `	const char *zText,*zPattern,*zEnd;` |
|      - | 1395 | `	int nTextlen,nPatlen;` |
|     25 | 1396 | `	int iCount = 0;` |
|      - | 1397 | `	sxu32 nOfft;` |
|      - | 1398 | `	sxi32 rc;` |
|     25 | 1399 | `	if( nArg < 2 ){` |
|      - | 1400 | `		/* Missing arguments */` |
|      5 | 1401 | `		ph7_result_int(pCtx,0);` |
|      5 | 1402 | `		return PH7_OK;` |
|      - | 1403 | `	}` |
|      - | 1404 | `	/* Point to the haystack */` |
|     21 | 1405 | `	zText = ph7_value_to_string(apArg[0],&nTextlen);` |
|      - | 1406 | `	/* Point to the neddle */` |
|     21 | 1407 | `	zPattern = ph7_value_to_string(apArg[1],&nPatlen);` |
|     21 | 1408 | `	if( nTextlen < 1 \|\| nPatlen < 1 \|\| nPatlen > nTextlen ){` |
|      - | 1409 | `		/* NOOP,return zero */` |
|      3 | 1410 | `		ph7_result_int(pCtx,0);` |
|      3 | 1411 | `		return PH7_OK;` |
|      - | 1412 | `	}` |
|     19 | 1413 | `	if( nArg > 2 ){` |
|      - | 1414 | `		int iOfft;` |
|      - | 1415 | `		/* Extract the offset */` |
|     15 | 1416 | `		iOfft = ph7_value_to_int(apArg[2]);` |
|     15 | 1417 | `		if( iOfft < 0 \|\| iOfft > nTextlen ){` |
|      - | 1418 | `			/* Invalid offset,return zero */` |
|      3 | 1419 | `			ph7_result_int(pCtx,0);` |
|      3 | 1420 | `			return PH7_OK;` |
|      - | 1421 | `		}` |
|      - | 1422 | `		/* Point to the desired offset */` |
|     13 | 1423 | `		zText = &zText[iOfft];` |
|      - | 1424 | `		/* Adjust length */` |
|     13 | 1425 | `		nTextlen -= iOfft;` |
|      6 | 1426 | `	}` |
|      - | 1427 | `	/* Point to the end of the string */` |
|     17 | 1428 | `	zEnd = &zText[nTextlen];` |
|     17 | 1429 | `	if( nArg > 3 ){` |
|      - | 1430 | `		int nLen;` |
|      - | 1431 | `		/* Extract the length */` |
|     13 | 1432 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     13 | 1433 | `		if( nLen < 0 \|\| nLen > nTextlen ){` |
|      - | 1434 | `			/* Invalid length,return 0 */` |
|      7 | 1435 | `			ph7_result_int(pCtx,0);` |
|      7 | 1436 | `			return PH7_OK;` |
|      - | 1437 | `		}` |
|      - | 1438 | `		/* Adjust pointer */` |
|      7 | 1439 | `		nTextlen = nLen;` |
|      7 | 1440 | `		zEnd = &zText[nTextlen];` |
|      3 | 1441 | `	}` |
|      - | 1442 | `	/* Perform the search */` |
|     12 | 1443 | `	for(;;){` |
|     25 | 1444 | `		rc = SyBlobSearch((const void *)zText,(sxu32)(zEnd-zText),(const void *)zPattern,nPatlen,&nOfft);` |
|     25 | 1445 | `		if( rc != SXRET_OK ){` |
|      - | 1446 | `			/* Pattern not found,break immediately */` |
|      9 | 1447 | `			break;` |
|      - | 1448 | `		}` |
|      - | 1449 | `		/* Increment counter and update the offset */` |
|     17 | 1450 | `		iCount++;` |
|     17 | 1451 | `		zText += nOfft + nPatlen;` |
|     17 | 1452 | `		if( zText >= zEnd ){` |
|      3 | 1453 | `			break;` |
|      - | 1454 | `		}` |
|      1 | 1455 | `	}` |
|      - | 1456 | `	/* Pattern count */` |
|     11 | 1457 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 1458 | `	return PH7_OK;` |
|     13 | 1459 |  |
|      - | 1460 | `/*` |
|      - | 1461 | ` * string chunk_split(string $body[,int $chunklen = 76 [, string $end = "\r\n" ]])` |
|      - | 1462 | ` *   Split a string into smaller chunks.` |
|      - | 1463 | ` * Parameters` |
|      - | 1464 | ` *  $body` |
|      - | 1465 | ` *   The string to be chunked.` |
|      - | 1466 | ` * $chunklen` |
|      - | 1467 | ` *   The chunk length.` |
|      - | 1468 | ` * $end` |
|      - | 1469 | ` *   The line ending sequence.` |
|      - | 1470 | ` * Return` |
|      - | 1471 | ` *  The chunked string or NULL on failure.` |
|      - | 1472 | ` */` |
|     16 | 1473 | `static int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1474 |  |
|     17 | 1475 | `	const char *zIn,*zEnd,*zSep = "\r\n";` |
|      - | 1476 | `	int nSepLen,nChunkLen,nLen;` |
|     17 | 1477 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1478 | `		/* Nothing to split,return null */` |
|      5 | 1479 | `		ph7_result_null(pCtx);` |
|      5 | 1480 | `		return PH7_OK;` |
|      - | 1481 | `	}` |
|      - | 1482 | `	/* initialize/Extract arguments */` |
|     13 | 1483 | `	nSepLen = (int)sizeof("\r\n") - 1;` |
|     13 | 1484 | `	nChunkLen = 76;` |
|     13 | 1485 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 1486 | `	zEnd = &zIn[nLen];` |
|     13 | 1487 | `	if( nArg > 1 ){` |
|      - | 1488 | `		/* Chunk length */` |
|     13 | 1489 | `		nChunkLen = ph7_value_to_int(apArg[1]);` |
|     13 | 1490 | `		if( nChunkLen < 1 ){` |
|      - | 1491 | `			/* Switch back to the default length */` |
|      3 | 1492 | `			nChunkLen = 76;` |
|      1 | 1493 | `		}` |
|     13 | 1494 | `		if( nArg > 2 ){` |
|      - | 1495 | `			/* Separator */` |
|      9 | 1496 | `			zSep = ph7_value_to_string(apArg[2],&nSepLen);` |
|      9 | 1497 | `			if( nSepLen < 1 ){` |
|      - | 1498 | `				/* Switch back to the default separator */` |
|      3 | 1499 | `				zSep = "\r\n";` |
|      3 | 1500 | `				nSepLen = (int)sizeof("\r\n") - 1;` |
|      1 | 1501 | `			}` |
|      4 | 1502 | `		}` |
|      6 | 1503 | `	}` |
|      - | 1504 | `	/* Perform the requested operation */` |
|     13 | 1505 | `	if( nChunkLen > nLen ){` |
|      - | 1506 | `		/* Nothing to split,return the string and the separator */` |
|      9 | 1507 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);` |
|      9 | 1508 | `		return PH7_OK;` |
|      - | 1509 | `	}` |
|     17 | 1510 | `	while( zIn < zEnd ){` |
|     13 | 1511 | `		if( nChunkLen > (int)(zEnd-zIn) ){` |
|      3 | 1512 | `			nChunkLen = (int)(zEnd - zIn);` |
|      1 | 1513 | `		}` |
|      - | 1514 | `		/* Append the chunk and the separator */` |
|     13 | 1515 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);` |
|      - | 1516 | `		/* Point beyond the chunk */` |
|     13 | 1517 | `		zIn += nChunkLen;` |
|      1 | 1518 | `	}` |
|      5 | 1519 | `	return PH7_OK;` |
|      9 | 1520 |  |
|      - | 1521 | `/*` |
|      - | 1522 | ` * string addslashes(string $str)` |
|      - | 1523 | ` *  Quote string with slashes.` |
|      - | 1524 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1525 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1526 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1527 | ` * Parameter` |
|      - | 1528 | ` *  str: The string to be escaped.` |
|      - | 1529 | ` * Return` |
|      - | 1530 | ` *  Returns the escaped string` |
|      - | 1531 | ` */` |
|     10 | 1532 | `static int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1533 |  |
|      - | 1534 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1535 | `	int nLen;` |
|     11 | 1536 | `	if( nArg < 1 ){` |
|      - | 1537 | `		/* Nothing to process,retun NULL */` |
|      5 | 1538 | `		ph7_result_null(pCtx);` |
|      5 | 1539 | `		return PH7_OK;` |
|      - | 1540 | `	}` |
|      - | 1541 | `	/* Extract the string to process */` |
|      7 | 1542 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 1543 | `	if( nLen < 1 ){` |
|      - | 1544 | `		/* Return the empty string */` |
|      5 | 1545 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1546 | `		return PH7_OK;` |
|      - | 1547 | `	}` |
|      3 | 1548 | `	zEnd = &zIn[nLen];` |
|      3 | 1549 | `	zCur = 0; /* cc warning */` |
|      3 | 1550 | `	for(;;){` |
|      7 | 1551 | `		if( zIn >= zEnd ){` |
|      - | 1552 | `			/* No more input */` |
|      3 | 1553 | `			break;` |
|      - | 1554 | `		}` |
|      5 | 1555 | `		zCur = zIn;` |
|     15 | 1556 | `		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' ){` |
|     11 | 1557 | `			zIn++;` |
|      1 | 1558 | `		}` |
|      5 | 1559 | `		if( zIn > zCur ){` |
|      - | 1560 | `			/* Append raw contents */` |
|      5 | 1561 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 1562 | `		}` |
|      5 | 1563 | `		if( zIn < zEnd ){` |
|      3 | 1564 | `			int c = zIn[0];` |
|      3 | 1565 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|      1 | 1566 | `		}` |
|      5 | 1567 | `		zIn++;` |
|      1 | 1568 | `	}` |
|      3 | 1569 | `	return PH7_OK;` |
|      6 | 1570 |  |
|      - | 1571 | `/*` |
|      - | 1572 | ` * Check if the given character is present in the given mask.` |
|      - | 1573 | ` * Return TRUE if present. FALSE otherwise.` |
|      - | 1574 | ` */` |
|     76 | 1575 | `static int cSlashCheckMask(int c,const char *zMask,int nLen)` |
|      1 | 1576 |  |
|     77 | 1577 | `	const char *zEnd = &zMask[nLen];` |
|    495 | 1578 | `	while( zMask < zEnd ){` |
|    449 | 1579 | `		if( zMask[0] == c ){` |
|      - | 1580 | `			/* Character present,return TRUE */` |
|     31 | 1581 | `			return 1;` |
|      - | 1582 | `		}` |
|      - | 1583 | `		/* Advance the pointer */` |
|    419 | 1584 | `		zMask++;` |
|      1 | 1585 | `	}` |
|      - | 1586 | `	/* Not present */` |
|     47 | 1587 | `	return 0;` |
|     39 | 1588 |  |
|      - | 1589 | `/*` |
|      - | 1590 | ` * string addcslashes(string $str,string $charlist)` |
|      - | 1591 | ` *  Quote string with slashes in a C style.` |
|      - | 1592 | ` * Parameter` |
|      - | 1593 | ` *  $str:` |
|      - | 1594 | ` *    The string to be escaped.` |
|      - | 1595 | ` *  $charlist:` |
|      - | 1596 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|      - | 1597 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|      - | 1598 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|      - | 1599 | ` * Return` |
|      - | 1600 | ` *  Returns the escaped string.` |
|      - | 1601 | ` * Note:` |
|      - | 1602 | ` *  Range characters [i.e: 'A..Z'] is not implemented in the current release.` |
|      - | 1603 | ` */` |
|     12 | 1604 | `static int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1605 |  |
|      - | 1606 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - | 1607 | `	int nLen,nMask;` |
|     13 | 1608 | `	if( nArg < 1 ){` |
|      - | 1609 | `		/* Nothing to process,retun NULL */` |
|      3 | 1610 | `		ph7_result_null(pCtx);` |
|      3 | 1611 | `		return PH7_OK;` |
|      - | 1612 | `	}` |
|      - | 1613 | `	/* Extract the string to process */` |
|     11 | 1614 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 1615 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 1616 | `		/* Return the string untouched */` |
|      5 | 1617 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      5 | 1618 | `		return PH7_OK;` |
|      - | 1619 | `	}` |
|      - | 1620 | `	/* Extract the desired mask */` |
|      7 | 1621 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|      7 | 1622 | `	zEnd = &zIn[nLen];` |
|      7 | 1623 | `	zCur = 0; /* cc warning */` |
|      8 | 1624 | `	for(;;){` |
|     17 | 1625 | `		if( zIn >= zEnd ){` |
|      - | 1626 | `			/* No more input */` |
|      7 | 1627 | `			break;` |
|      - | 1628 | `		}` |
|     11 | 1629 | `		zCur = zIn;` |
|     31 | 1630 | `		while( zIn < zEnd && !cSlashCheckMask(zIn[0],zMask,nMask) ){` |
|     21 | 1631 | `			zIn++;` |
|      1 | 1632 | `		}` |
|     11 | 1633 | `		if( zIn > zCur ){` |
|      - | 1634 | `			/* Append raw contents */` |
|     11 | 1635 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      5 | 1636 | `		}` |
|     11 | 1637 | `		if( zIn < zEnd ){` |
|      5 | 1638 | `			int c = zIn[0];` |
|      5 | 1639 | `			if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|      - | 1640 | `				/* Convert to octal */` |
|      3 | 1641 | `				ph7_result_string_format(pCtx,"\\%o",c);` |
|      2 | 1642 | `			}else{` |
|      3 | 1643 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1644 | `			}` |
|      2 | 1645 | `		}` |
|     11 | 1646 | `		zIn++;` |
|      1 | 1647 | `	}` |
|      7 | 1648 | `	return PH7_OK;` |
|      7 | 1649 |  |
|      - | 1650 | `/*` |
|      - | 1651 | ` * string quotemeta(string $str)` |
|      - | 1652 | ` *  Quote meta characters.` |
|      - | 1653 | ` * Parameter` |
|      - | 1654 | ` *  $str:` |
|      - | 1655 | ` *    The string to be escaped.` |
|      - | 1656 | ` * Return` |
|      - | 1657 | ` *  Returns the escaped string.` |
|      - | 1658 | `*/` |
|     10 | 1659 | `static int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1660 |  |
|      - | 1661 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1662 | `	int nLen;` |
|     11 | 1663 | `	if( nArg < 1 ){` |
|      - | 1664 | `		/* Nothing to process,retun NULL */` |
|      3 | 1665 | `		ph7_result_null(pCtx);` |
|      3 | 1666 | `		return PH7_OK;` |
|      - | 1667 | `	}` |
|      - | 1668 | `	/* Extract the string to process */` |
|      9 | 1669 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 1670 | `	if( nLen < 1 ){` |
|      - | 1671 | `		/* Return the empty string */` |
|      3 | 1672 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1673 | `		return PH7_OK;` |
|      - | 1674 | `	}` |
|      7 | 1675 | `	zEnd = &zIn[nLen];` |
|      7 | 1676 | `	zCur = 0; /* cc warning */` |
|     17 | 1677 | `	for(;;){` |
|     35 | 1678 | `		if( zIn >= zEnd ){` |
|      - | 1679 | `			/* No more input */` |
|      7 | 1680 | `			break;` |
|      - | 1681 | `		}` |
|     29 | 1682 | `		zCur = zIn;` |
|     55 | 1683 | `		while( zIn < zEnd && !cSlashCheckMask(zIn[0],".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1) ){` |
|     27 | 1684 | `			zIn++;` |
|      1 | 1685 | `		}` |
|     29 | 1686 | `		if( zIn > zCur ){` |
|      - | 1687 | `			/* Append raw contents */` |
|     11 | 1688 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      5 | 1689 | `		}` |
|     29 | 1690 | `		if( zIn < zEnd ){` |
|     27 | 1691 | `			int c = zIn[0];` |
|     27 | 1692 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|     13 | 1693 | `		}` |
|     29 | 1694 | `		zIn++;` |
|      1 | 1695 | `	}` |
|      7 | 1696 | `	return PH7_OK;` |
|      6 | 1697 |  |
|      - | 1698 | `/*` |
|      - | 1699 | ` * string stripslashes(string $str)` |
|      - | 1700 | ` *  Un-quotes a quoted string.` |
|      - | 1701 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1702 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1703 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1704 | ` * Parameter` |
|      - | 1705 | ` *  $str` |
|      - | 1706 | ` *   The input string.` |
|      - | 1707 | ` * Return` |
|      - | 1708 | ` *  Returns a string with backslashes stripped off.` |
|      - | 1709 | ` */` |
|      8 | 1710 | `static int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1711 |  |
|      - | 1712 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1713 | `	int nLen;` |
|      9 | 1714 | `	if( nArg < 1 ){` |
|      - | 1715 | `		/* Nothing to process,retun NULL */` |
|      3 | 1716 | `		ph7_result_null(pCtx);` |
|      3 | 1717 | `		return PH7_OK;` |
|      - | 1718 | `	}` |
|      - | 1719 | `	/* Extract the string to process */` |
|      7 | 1720 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 1721 | `	if( zIn == 0 ){` |
|    ! 0 | 1722 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1723 | `		return PH7_OK;` |
|      - | 1724 | `	}` |
|      7 | 1725 | `	zEnd = &zIn[nLen];` |
|      7 | 1726 | `	zCur = 0; /* cc warning */` |
|      - | 1727 | `	/* Encode the string */` |
|      4 | 1728 | `	for(;;){` |
|      9 | 1729 | `		if( zIn >= zEnd ){` |
|      - | 1730 | `			/* No more input */` |
|      5 | 1731 | `			break;` |
|      - | 1732 | `		}` |
|      5 | 1733 | `		zCur = zIn;` |
|     17 | 1734 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 | 1735 | `			zIn++;` |
|      1 | 1736 | `		}` |
|      5 | 1737 | `		if( zIn > zCur ){` |
|      - | 1738 | `			/* Append raw contents */` |
|      5 | 1739 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 1740 | `		}` |
|      5 | 1741 | `		if( &zIn[1] < zEnd ){` |
|      3 | 1742 | `			int c = zIn[1];` |
|      3 | 1743 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - | 1744 | `				/* Ignore the backslash */` |
|      3 | 1745 | `				zIn++;` |
|      1 | 1746 | `			}` |
|      2 | 1747 | `		}else{` |
|      3 | 1748 | `			break;` |
|      - | 1749 | `		}` |
|      1 | 1750 | `	}` |
|      7 | 1751 | `	return PH7_OK;` |
|      5 | 1752 |  |
|      - | 1753 | `/*` |
|      - | 1754 | ` * string htmlspecialchars(string $string [, int $flags = ENT_COMPAT \| ENT_HTML401 [, string $charset]])` |
|      - | 1755 | ` *  HTML escaping of special characters.` |
|      - | 1756 | ` *  The translations performed are:` |
|      - | 1757 | ` *   '&' (ampersand) ==> '&amp;'` |
|      - | 1758 | ` *   '"' (double quote) ==> '&quot;' when ENT_NOQUOTES is not set.` |
|      - | 1759 | ` *   "'" (single quote) ==> '&#039;' only when ENT_QUOTES is set.` |
|      - | 1760 | ` *   '<' (less than) ==> '&lt;'` |
|      - | 1761 | ` *   '>' (greater than) ==> '&gt;'` |
|      - | 1762 | ` * Parameters` |
|      - | 1763 | ` *  $string` |
|      - | 1764 | ` *   The string being converted.` |
|      - | 1765 | ` * $flags` |
|      - | 1766 | ` *   A bitmask of one or more of the following flags, which specify how to handle quotes.` |
|      - | 1767 | ` *   The default is ENT_COMPAT \| ENT_HTML401.` |
|      - | 1768 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone.` |
|      - | 1769 | ` *   ENT_QUOTES 	Will convert both double and single quotes.` |
|      - | 1770 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted.` |
|      - | 1771 | ` *   ENT_IGNORE 	Silently discard invalid code unit sequences instead of returning an empty string.` |
|      - | 1772 | ` * $charset` |
|      - | 1773 | ` *  Defines character set used in conversion. The default character set is ISO-8859-1. (Not used)` |
|      - | 1774 | ` * Return` |
|      - | 1775 | ` *  The escaped string or NULL on failure.` |
|      - | 1776 | ` */` |
|     20 | 1777 | `static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1778 |  |
|      - | 1779 | `	const char *zCur,*zIn,*zEnd;` |
|     21 | 1780 | `	int iFlags = 0x01\|0x40; /* ENT_COMPAT \| ENT_HTML401 */` |
|      - | 1781 | `	int nLen,c;` |
|     21 | 1782 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1783 | `		/* Missing/Invalid arguments,return NULL */` |
|     11 | 1784 | `		ph7_result_null(pCtx);` |
|     11 | 1785 | `		return PH7_OK;` |
|      - | 1786 | `	}` |
|      - | 1787 | `	/* Extract the target string */` |
|     11 | 1788 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 1789 | `	zEnd = &zIn[nLen];` |
|      - | 1790 | `	/* Extract the flags if available */` |
|     11 | 1791 | `	if( nArg > 1 ){` |
|      9 | 1792 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 | 1793 | `		if( iFlags < 0 ){` |
|      3 | 1794 | `			iFlags = 0x01\|0x40;` |
|      1 | 1795 | `		}` |
|      4 | 1796 | `	}` |
|      - | 1797 | `	/* Perform the requested operation */` |
|     23 | 1798 | `	for(;;){` |
|     47 | 1799 | `		if( zIn >= zEnd ){` |
|      9 | 1800 | `			break;` |
|      - | 1801 | `		}` |
|     39 | 1802 | `		zCur = zIn;` |
|     83 | 1803 | `		while( zIn < zEnd && zIn[0] != '&' && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '<' && zIn[0] != '>' ){` |
|     45 | 1804 | `			zIn++;` |
|      1 | 1805 | `		}` |
|     39 | 1806 | `		if( zCur < zIn ){` |
|      - | 1807 | `			/* Append the raw string verbatim */` |
|     17 | 1808 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      8 | 1809 | `		}` |
|     39 | 1810 | `		if( zIn >= zEnd ){` |
|      3 | 1811 | `			break;` |
|      - | 1812 | `		}` |
|     37 | 1813 | `		c = zIn[0];` |
|     37 | 1814 | `		if( c == '&' ){` |
|      - | 1815 | `			/* Expand '&amp;' */` |
|      9 | 1816 | `			ph7_result_string(pCtx,"&amp;",(int)sizeof("&amp;")-1);` |
|     33 | 1817 | `		}else if( c == '<' ){` |
|      - | 1818 | `			/* Expand '&lt;' */` |
|      7 | 1819 | `			ph7_result_string(pCtx,"&lt;",(int)sizeof("&lt;")-1);` |
|     26 | 1820 | `		}else if( c == '>' ){` |
|      - | 1821 | `			/* Expand '&gt;' */` |
|      9 | 1822 | `			ph7_result_string(pCtx,"&gt;",(int)sizeof("&gt;")-1);` |
|     19 | 1823 | `		}else if( c == '\'' ){` |
|      5 | 1824 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 1825 | `				/* Expand '&#039;' */` |
|      5 | 1826 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      3 | 1827 | `			}else{` |
|      - | 1828 | `				/* Leave the single quote untouched */` |
|    ! 0 | 1829 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      1 | 1830 | `			}` |
|     13 | 1831 | `		}else if( c == '"' ){` |
|     11 | 1832 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      - | 1833 | `				/* Expand '&quot;' */` |
|      7 | 1834 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      4 | 1835 | `			}else{` |
|      - | 1836 | `				/* Leave the double quote untouched */` |
|      5 | 1837 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      - | 1838 | `			}` |
|      5 | 1839 | `		}` |
|      - | 1840 | `		/* Ignore the unsafe HTML character */` |
|     37 | 1841 | `		zIn++;` |
|      1 | 1842 | `	}` |
|     11 | 1843 | `	return PH7_OK;` |
|     11 | 1844 |  |
|      - | 1845 | `/*` |
|      - | 1846 | ` * string htmlspecialchars_decode(string $string[,int $quote_style = ENT_COMPAT ])` |
|      - | 1847 | ` *  Unescape HTML entities.` |
|      - | 1848 | ` * Parameters` |
|      - | 1849 | ` *  $string` |
|      - | 1850 | ` *   The string to decode` |
|      - | 1851 | ` *  $quote_style` |
|      - | 1852 | ` *    The quote style. One of the following constants:` |
|      - | 1853 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone (default)` |
|      - | 1854 | ` *   ENT_QUOTES 	Will convert both double and single quotes` |
|      - | 1855 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted` |
|      - | 1856 | ` * Return` |
|      - | 1857 | ` *  The unescaped string or NULL on failure.` |
|      - | 1858 | ` */` |
|     16 | 1859 | `static int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1860 |  |
|      - | 1861 | `	const char *zCur,*zIn,*zEnd;` |
|     17 | 1862 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 1863 | `	int nLen,nJump;` |
|     17 | 1864 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1865 | `		/* Missing/Invalid arguments,return NULL */` |
|      7 | 1866 | `		ph7_result_null(pCtx);` |
|      7 | 1867 | `		return PH7_OK;` |
|      - | 1868 | `	}` |
|      - | 1869 | `	/* Extract the target string */` |
|     11 | 1870 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 1871 | `	zEnd = &zIn[nLen];` |
|      - | 1872 | `	/* Extract the flags if available */` |
|     11 | 1873 | `	if( nArg > 1 ){` |
|      7 | 1874 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      7 | 1875 | `		if( iFlags < 0 ){` |
|      3 | 1876 | `			iFlags = 0x01;` |
|      1 | 1877 | `		}` |
|      3 | 1878 | `	}` |
|      - | 1879 | `	/* Perform the requested operation */` |
|     15 | 1880 | `	for(;;){` |
|     31 | 1881 | `		if( zIn >= zEnd ){` |
|     11 | 1882 | `			break;` |
|      - | 1883 | `		}` |
|     21 | 1884 | `		zCur = zIn;` |
|     51 | 1885 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|     31 | 1886 | `			zIn++;` |
|      1 | 1887 | `		}` |
|     21 | 1888 | `		if( zCur < zIn ){` |
|      - | 1889 | `			/* Append the raw string verbatim */` |
|      9 | 1890 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 1891 | `		}` |
|     21 | 1892 | `		nLen = (int)(zEnd-zIn);` |
|     21 | 1893 | `		nJump = (int)sizeof(char);` |
|     21 | 1894 | `		if( nLen >= (int)sizeof("&amp;")-1 && SyStrnicmp(zIn,"&amp;",sizeof("&amp;")-1) == 0 ){` |
|      - | 1895 | `			/* &amp; ==> '&' */` |
|      3 | 1896 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|      3 | 1897 | `			nJump = (int)sizeof("&amp;")-1;` |
|     20 | 1898 | `		}else if( nLen >= (int)sizeof("&lt;")-1 && SyStrnicmp(zIn,"&lt;",sizeof("&lt;")-1) == 0 ){` |
|      - | 1899 | `			/* &lt; ==> < */` |
|      3 | 1900 | `			ph7_result_string(pCtx,"<",(int)sizeof(char));` |
|      3 | 1901 | `			nJump = (int)sizeof("&lt;")-1;` |
|     18 | 1902 | `		}else if( nLen >= (int)sizeof("&gt;")-1 && SyStrnicmp(zIn,"&gt;",sizeof("&gt;")-1) == 0 ){` |
|      - | 1903 | `			/* &gt; ==> '>' */` |
|      3 | 1904 | `			ph7_result_string(pCtx,">",(int)sizeof(char));` |
|      3 | 1905 | `			nJump = (int)sizeof("&gt;")-1;` |
|     16 | 1906 | `		}else if( nLen >= (int)sizeof("&quot;")-1 && SyStrnicmp(zIn,"&quot;",sizeof("&quot;")-1) == 0 ){` |
|      - | 1907 | `			/* &quot; ==> '"' */` |
|     13 | 1908 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      9 | 1909 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      5 | 1910 | `			}else{` |
|      - | 1911 | `				/* Leave untouched */` |
|      5 | 1912 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      - | 1913 | `			}` |
|     13 | 1914 | `			nJump = (int)sizeof("&quot;")-1;` |
|      9 | 1915 | `		}else if( nLen >= (int)sizeof("&#039;")-1 && SyStrnicmp(zIn,"&#039;",sizeof("&#039;")-1) == 0 ){` |
|      - | 1916 | `			/* &#039; ==> ''' */` |
|      3 | 1917 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 1918 | `				/* Expand ''' */` |
|      3 | 1919 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      2 | 1920 | `			}else{` |
|      - | 1921 | `				/* Leave untouched */` |
|    ! 0 | 1922 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      - | 1923 | `			}` |
|      3 | 1924 | `			nJump = (int)sizeof("&#039;")-1;` |
|      1 | 1925 | `		}else if( nLen >= (int)sizeof(char) ){` |
|      - | 1926 | `			/* expand '&' */` |
|    ! 0 | 1927 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 1928 | `		}else{` |
|      - | 1929 | `			/* No more input to process */` |
|    ! 0 | 1930 | `			break;` |
|      - | 1931 | `		}` |
|     21 | 1932 | `		zIn += nJump;` |
|      1 | 1933 | `	}` |
|     11 | 1934 | `	return PH7_OK;` |
|      9 | 1935 |  |
|      - | 1936 | `/* HTML encoding/Decoding table` |
|      - | 1937 | ` * Source: Symisc RunTime API.[chm@symisc.net]` |
|      - | 1938 | ` */` |
|      - | 1939 | `static const char *azHtmlEscape[] = {` |
|      - | 1940 | ` 	"&lt;","<","&gt;",">","&amp;","&","&quot;","\"","&#39;","'",` |
|      - | 1941 | `	"&#33;","!","&#36;","$","&#35;","#","&#37;","%","&#40;","(",` |
|      - | 1942 | `	"&#41;",")","&#123;","{","&#125;","}","&#61;","=","&#43;","+",` |
|      - | 1943 | `	"&#63;","?","&#91;","[","&#93;","]","&#64;","@","&#44;",","` |
|      - | 1944 | ` };` |
|      - | 1945 | `/*` |
|      - | 1946 | ` * array get_html_translation_table(void)` |
|      - | 1947 | ` *  Returns the translation table used by htmlspecialchars() and htmlentities().` |
|      - | 1948 | ` * Parameters` |
|      - | 1949 | ` *  None` |
|      - | 1950 | ` * Return` |
|      - | 1951 | ` *  The translation table as an array or NULL on failure.` |
|      - | 1952 | ` */` |
|      4 | 1953 | `static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1954 |  |
|      - | 1955 | `	ph7_value *pArray,*pValue;` |
|      - | 1956 | `	sxu32 n;` |
|      - | 1957 | `	/* Element value */` |
|      5 | 1958 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      5 | 1959 | `	if( pValue == 0 ){` |
|    ! 0 | 1960 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 1961 | `		SXUNUSED(apArg);` |
|      - | 1962 | `		/* Return NULL */` |
|    ! 0 | 1963 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1964 | `		return PH7_OK;` |
|      - | 1965 | `	}` |
|      - | 1966 | `	/* Create a new array */` |
|      5 | 1967 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 | 1968 | `	if( pArray == 0 ){` |
|      - | 1969 | `		/* Return NULL */` |
|    ! 0 | 1970 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1971 | `		return PH7_OK;` |
|      - | 1972 | `	}` |
|      - | 1973 | `	/* Make the table */` |
|     85 | 1974 | `	for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|      - | 1975 | `		/* Prepare the value */` |
|     81 | 1976 | `		ph7_value_string(pValue,azHtmlEscape[n],-1 /* Compute length automatically */);` |
|      - | 1977 | `		/* Insert the value */` |
|     81 | 1978 | `		ph7_array_add_strkey_elem(pArray,azHtmlEscape[n+1],pValue);` |
|      - | 1979 | `		/* Reset the string cursor */` |
|     81 | 1980 | `		ph7_value_reset_string_cursor(pValue);` |
|     41 | 1981 | `	}` |
|      - | 1982 | `	/*` |
|      - | 1983 | `	 * Return the array.` |
|      - | 1984 | `	 * Don't worry about freeing memory, everything will be automatically` |
|      - | 1985 | `	 * released upon we return from this function.` |
|      - | 1986 | `	 */` |
|      5 | 1987 | `	ph7_result_value(pCtx,pArray);` |
|      5 | 1988 | `	return PH7_OK;` |
|      3 | 1989 |  |
|      - | 1990 | `/*` |
|      - | 1991 | ` * string htmlentities( string $string [, int $flags = ENT_COMPAT \| ENT_HTML401]);` |
|      - | 1992 | ` *   Convert all applicable characters to HTML entities` |
|      - | 1993 | ` * Parameters` |
|      - | 1994 | ` * $string` |
|      - | 1995 | ` *   The input string.` |
|      - | 1996 | ` * $flags` |
|      - | 1997 | ` *  A bitmask of one or more of the flags (see block-comment on PH7_builtin_htmlspecialchars())` |
|      - | 1998 | ` * Return` |
|      - | 1999 | ` * The encoded string.` |
|      - | 2000 | ` */` |
|     10 | 2001 | `static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2002 |  |
|     11 | 2003 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 2004 | `	const char *zIn,*zEnd;` |
|      - | 2005 | `	int nLen,c;` |
|      - | 2006 | `	sxu32 n;` |
|     11 | 2007 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2008 | `		/* Missing/Invalid arguments,return NULL */` |
|      7 | 2009 | `		ph7_result_null(pCtx);` |
|      7 | 2010 | `		return PH7_OK;` |
|      - | 2011 | `	}` |
|      - | 2012 | `	/* Extract the target string */` |
|      5 | 2013 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2014 | `	zEnd = &zIn[nLen];` |
|      - | 2015 | `	/* Extract the flags if available */` |
|      5 | 2016 | `	if( nArg > 1 ){` |
|      3 | 2017 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      3 | 2018 | `		if( iFlags < 0 ){` |
|      3 | 2019 | `			iFlags = 0x01;` |
|      1 | 2020 | `		}` |
|      1 | 2021 | `	}` |
|      - | 2022 | `	/* Perform the requested operation */` |
|     11 | 2023 | `	for(;;){` |
|     23 | 2024 | `		if( zIn >= zEnd ){` |
|      - | 2025 | `			/* No more input to process */` |
|      5 | 2026 | `			break;` |
|      - | 2027 | `		}` |
|     19 | 2028 | `		c = zIn[0];` |
|      - | 2029 | `		/* Perform a linear lookup on the decoding table */` |
|    233 | 2030 | `		for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    223 | 2031 | `			if( azHtmlEscape[n+1][0] == c ){` |
|      - | 2032 | `				/* Got one */` |
|      9 | 2033 | `				break;` |
|      - | 2034 | `			}` |
|    108 | 2035 | `		}` |
|     19 | 2036 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|      - | 2037 | `			/* Output the safe sequence [i.e: '<' ==> '&lt;"] */` |
|      9 | 2038 | `			if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 2039 | `				/* Expand the double quote verbatim */` |
|    ! 0 | 2040 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      9 | 2041 | `			}else if(c == '\'' && ((iFlags & 0x02 /*ENT_QUOTES*/) == 0 \|\| (iFlags & 0x04) /*ENT_NOQUOTES*/) ){` |
|      - | 2042 | `				/* expand single quote verbatim */` |
|    ! 0 | 2043 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|    ! 0 | 2044 | `			}else{` |
|      9 | 2045 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1/*Compute length automatically */);` |
|      - | 2046 | `			}` |
|      5 | 2047 | `		}else{` |
|      - | 2048 | `			/* Output character verbatim */` |
|     11 | 2049 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2050 | `		}` |
|     19 | 2051 | `		zIn++;` |
|      1 | 2052 | `	}` |
|      5 | 2053 | `	return PH7_OK;` |
|      6 | 2054 |  |
|      - | 2055 | `/*` |
|      - | 2056 | ` * string html_entity_decode(string $string [, int $quote_style = ENT_COMPAT [, string $charset = 'UTF-8' ]])` |
|      - | 2057 | ` *   Perform the reverse operation of html_entity_decode().` |
|      - | 2058 | ` * Parameters` |
|      - | 2059 | ` * $string` |
|      - | 2060 | ` *   The input string.` |
|      - | 2061 | ` * $flags` |
|      - | 2062 | ` *  A bitmask of one or more of the flags (see comment on PH7_builtin_htmlspecialchars())` |
|      - | 2063 | ` * Return` |
|      - | 2064 | ` * The decoded string.` |
|      - | 2065 | ` */` |
|     28 | 2066 | `static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2067 |  |
|      - | 2068 | `	const char *zCur,*zIn,*zEnd;` |
|     29 | 2069 | `	int iFlags = 0x01; /* ENT_COMPAT  */` |
|      - | 2070 | `	int nLen;` |
|      - | 2071 | `	sxu32 n;` |
|     29 | 2072 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2073 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 2074 | `		ph7_result_null(pCtx);` |
|      5 | 2075 | `		return PH7_OK;` |
|      - | 2076 | `	}` |
|      - | 2077 | `	/* Extract the target string */` |
|     25 | 2078 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 2079 | `	zEnd = &zIn[nLen];` |
|      - | 2080 | `	/* Extract the flags if available */` |
|     25 | 2081 | `	if( nArg > 1 ){` |
|     15 | 2082 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     15 | 2083 | `		if( iFlags < 0 ){` |
|      3 | 2084 | `			iFlags = 0x01;` |
|      1 | 2085 | `		}` |
|      7 | 2086 | `	}` |
|      - | 2087 | `	/* Perform the requested operation */` |
|     27 | 2088 | `	for(;;){` |
|     55 | 2089 | `		if( zIn >= zEnd ){` |
|      - | 2090 | `			/* No more input to process */` |
|     13 | 2091 | `			break;` |
|      - | 2092 | `		}` |
|     43 | 2093 | `		zCur = zIn;` |
|    173 | 2094 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|    131 | 2095 | `			zIn++;` |
|      1 | 2096 | `		}` |
|     43 | 2097 | `		if( zCur < zIn ){` |
|      - | 2098 | `			/* Append raw string verbatim */` |
|     27 | 2099 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     13 | 2100 | `		}` |
|     43 | 2101 | `		if( zIn >= zEnd ){` |
|     13 | 2102 | `			break;` |
|      - | 2103 | `		}` |
|     31 | 2104 | `		nLen = (int)(zEnd-zIn);` |
|      - | 2105 | `		/* Find an encoded sequence */` |
|    113 | 2106 | `		for(n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    113 | 2107 | `			int iLen = (int)SyStrlen(azHtmlEscape[n]);` |
|    113 | 2108 | `			if( nLen >= iLen && SyStrnicmp(zIn,azHtmlEscape[n],(sxu32)iLen) == 0 ){` |
|      - | 2109 | `				/* Got one */` |
|     31 | 2110 | `				zIn += iLen;` |
|     31 | 2111 | `				break;` |
|      - | 2112 | `			}` |
|     42 | 2113 | `		}` |
|     31 | 2114 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|     31 | 2115 | `			int c = azHtmlEscape[n+1][0];` |
|      - | 2116 | `			/* Output the decoded character */` |
|     31 | 2117 | `			if( c == '\'' && ((iFlags & 0x02) == 0 /*ENT_QUOTES*/\|\| (iFlags & 0x04) /*ENT_NOQUOTES*/)  ){` |
|      - | 2118 | `				/* Do not process single quotes */` |
|      9 | 2119 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|     27 | 2120 | `			}else if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 2121 | `				/* Do not process double quotes */` |
|      5 | 2122 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|      3 | 2123 | `			}else{` |
|     19 | 2124 | `				ph7_result_string(pCtx,azHtmlEscape[n+1],-1); /* Compute length automatically */` |
|      - | 2125 | `			}` |
|     16 | 2126 | `		}else{` |
|      - | 2127 | `			/* Append '&' */` |
|    ! 0 | 2128 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 2129 | `			zIn++;` |
|      - | 2130 | `		}` |
|      1 | 2131 | `	}` |
|     25 | 2132 | `	return PH7_OK;` |
|     15 | 2133 |  |
|      - | 2134 | `/*` |
|      - | 2135 | ` * int strlen($string)` |
|      - | 2136 | ` *  return the length of the given string.` |
|      - | 2137 | ` * Parameter` |
|      - | 2138 | ` *  string: The string being measured for length.` |
|      - | 2139 | ` * Return` |
|      - | 2140 | ` *  length of the given string.` |
|      - | 2141 | ` */` |
|   1520 | 2142 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2143 |  |
|   1522 | 2144 | `	int iLen = 0;` |
|   1522 | 2145 | `	if( nArg > 0 ){` |
|   1520 | 2146 | `		ph7_value_to_string(apArg[0],&iLen);` |
|    759 | 2147 | `	}` |
|      - | 2148 | `	/* String length */` |
|   1522 | 2149 | `	ph7_result_int(pCtx,iLen);` |
|   1522 | 2150 | `	return PH7_OK;` |
|      2 | 2151 |  |
|      - | 2152 | `/*` |
|      - | 2153 | ` * int strcmp(string $str1,string $str2)` |
|      - | 2154 | ` *  Perform a binary safe string comparison.` |
|      - | 2155 | ` * Parameter` |
|      - | 2156 | ` *  str1: The first string` |
|      - | 2157 | ` *  str2: The second string` |
|      - | 2158 | ` * Return` |
|      - | 2159 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2160 | ` *  than str2, and 0 if they are equal.` |
|      - | 2161 | ` */` |
|     50 | 2162 | `static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2163 |  |
|      - | 2164 | `	const char *z1,*z2;` |
|      - | 2165 | `	int n1,n2;` |
|      - | 2166 | `	int res;` |
|     51 | 2167 | `	if( nArg < 2 ){` |
|      5 | 2168 | `		res = nArg == 0 ? 0 : 1;` |
|      5 | 2169 | `		ph7_result_int(pCtx,res);` |
|      5 | 2170 | `		return PH7_OK;` |
|      - | 2171 | `	}` |
|      - | 2172 | `	/* Perform the comparison */` |
|     47 | 2173 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     47 | 2174 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     47 | 2175 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2176 | `	/* Comparison result */` |
|     47 | 2177 | `	ph7_result_int(pCtx,res);` |
|     47 | 2178 | `	return PH7_OK;` |
|     26 | 2179 |  |
|      - | 2180 | `/*` |
|      - | 2181 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 2182 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 2183 | ` * Parameter` |
|      - | 2184 | ` *  str1: The first string` |
|      - | 2185 | ` *  str2: The second string` |
|      - | 2186 | ` * Return` |
|      - | 2187 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2188 | ` *  than str2, and 0 if they are equal.` |
|      - | 2189 | ` */` |
|     20 | 2190 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2191 |  |
|      - | 2192 | `	const char *z1,*z2;` |
|      - | 2193 | `	int res;` |
|      - | 2194 | `	int n;` |
|     21 | 2195 | `	if( nArg < 3 ){` |
|      - | 2196 | `		/* Perform a standard comparison */` |
|      5 | 2197 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 2198 | `	}` |
|      - | 2199 | `	/* Desired comparison length */` |
|     17 | 2200 | `	n  = ph7_value_to_int(apArg[2]);` |
|     17 | 2201 | `	if( n < 0 ){` |
|      - | 2202 | `		/* Invalid length */` |
|      3 | 2203 | `		ph7_result_int(pCtx,-1);` |
|      3 | 2204 | `		return PH7_OK;` |
|      - | 2205 | `	}` |
|      - | 2206 | `	/* Perform the comparison */` |
|     15 | 2207 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     15 | 2208 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     15 | 2209 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 2210 | `	/* Comparison result */` |
|     15 | 2211 | `	ph7_result_int(pCtx,res);` |
|     15 | 2212 | `	return PH7_OK;` |
|     11 | 2213 |  |
|      - | 2214 | `/*` |
|      - | 2215 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 2216 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 2217 | ` * Parameter` |
|      - | 2218 | ` *  str1: The first string` |
|      - | 2219 | ` *  str2: The second string` |
|      - | 2220 | ` * Return` |
|      - | 2221 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2222 | ` *  than str2, and 0 if they are equal.` |
|      - | 2223 | ` */` |
|     18 | 2224 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2225 |  |
|      - | 2226 | `	const char *z1,*z2;` |
|      - | 2227 | `	int n1,n2;` |
|      - | 2228 | `	int res;` |
|     19 | 2229 | `	if( nArg < 2 ){` |
|      9 | 2230 | `		res = nArg == 0 ? 0 : 1;` |
|      9 | 2231 | `		ph7_result_int(pCtx,res);` |
|      9 | 2232 | `		return PH7_OK;` |
|      - | 2233 | `	}` |
|      - | 2234 | `	/* Perform the comparison */` |
|     11 | 2235 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     11 | 2236 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     11 | 2237 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2238 | `	/* Comparison result */` |
|     11 | 2239 | `	ph7_result_int(pCtx,res);` |
|     11 | 2240 | `	return PH7_OK;` |
|     10 | 2241 |  |
|      - | 2242 | `/*` |
|      - | 2243 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 2244 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 2245 | ` * Parameter` |
|      - | 2246 | ` *  $str1: The first string` |
|      - | 2247 | ` *  $str2: The second string` |
|      - | 2248 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 2249 | ` * Return` |
|      - | 2250 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2251 | ` *  than str2, and 0 if they are equal.` |
|      - | 2252 | ` */` |
|      8 | 2253 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2254 |  |
|      - | 2255 | `	const char *z1,*z2;` |
|      - | 2256 | `	int res;` |
|      - | 2257 | `	int n;` |
|      9 | 2258 | `	if( nArg < 3 ){` |
|      - | 2259 | `		/* Perform a standard comparison */` |
|      5 | 2260 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 2261 | `	}` |
|      - | 2262 | `	/* Desired comparison length */` |
|      5 | 2263 | `	n  = ph7_value_to_int(apArg[2]);` |
|      5 | 2264 | `	if( n < 0 ){` |
|      - | 2265 | `		/* Invalid length */` |
|    ! 0 | 2266 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2267 | `		return PH7_OK;` |
|      - | 2268 | `	}` |
|      - | 2269 | `	/* Perform the comparison */` |
|      5 | 2270 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|      5 | 2271 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|      5 | 2272 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 2273 | `	/* Comparison result */` |
|      5 | 2274 | `	ph7_result_int(pCtx,res);` |
|      5 | 2275 | `	return PH7_OK;` |
|      5 | 2276 |  |
|      - | 2277 | `/*` |
|      - | 2278 | ` * Implode context [i.e: it's private data].` |
|      - | 2279 | ` * A pointer to the following structure is forwarded` |
|      - | 2280 | ` * verbatim to the array walker callback defined below.` |
|      - | 2281 | ` */` |
|      - | 2282 | `struct implode_data {` |
|      - | 2283 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 2284 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 2285 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 2286 | `	int nSeplen;          /* Separator length */` |
|      - | 2287 | `	int bFirst;           /* TRUE if first call */` |
|      - | 2288 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 2289 | `};` |
|      - | 2290 | `/*` |
|      - | 2291 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 2292 | ` * The following routine is invoked for each array entry passed` |
|      - | 2293 | ` * to the implode() function.` |
|      - | 2294 | ` */` |
|  79050 | 2295 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      2 | 2296 |  |
|  39525 | 2297 | `	SXUNUSED(pKey);` |
|  79052 | 2298 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 2299 | `	const char *zData;` |
|      - | 2300 | `	int nLen;` |
|  79052 | 2301 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 2302 | `		if( pData->nSeplen > 0 ){` |
|      3 | 2303 | `			if( !pData->bFirst ){` |
|      - | 2304 | `				/* append the separator first */` |
|      3 | 2305 | `				ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|      2 | 2306 | `			}else{` |
|    ! 0 | 2307 | `				pData->bFirst = 0;` |
|      - | 2308 | `			}` |
|      1 | 2309 | `		}` |
|      - | 2310 | `		/* Recurse */` |
|      3 | 2311 | `		pData->bFirst = 1;` |
|      3 | 2312 | `		pData->nRecCount++;` |
|      3 | 2313 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 2314 | `		pData->nRecCount--;` |
|      3 | 2315 | `		return PH7_OK;` |
|      - | 2316 | `	}` |
|      - | 2317 | `	/* Extract the string representation of the entry value */` |
|  79050 | 2318 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 2319 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
|  79050 | 2320 | `	if( pData->bFirst ){` |
|  17062 | 2321 | `		pData->bFirst = 0;` |
|  70520 | 2322 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 2323 | `		/* append the separator first */` |
|  61978 | 2324 | `		ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|  30988 | 2325 | `	}` |
|      - | 2326 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
|  79050 | 2327 | `	if( nLen > 0 ){` |
|  72134 | 2328 | `		ph7_result_string(pData->pCtx,zData,nLen);` |
|  36066 | 2329 | `	}` |
|  79050 | 2330 | `	return PH7_OK;` |
|  39527 | 2331 |  |
|      - | 2332 | `/*` |
|      - | 2333 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 2334 | ` * string implode(array $pieces,...)` |
|      - | 2335 | ` *  Join array elements with a string.` |
|      - | 2336 | ` * $glue` |
|      - | 2337 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 2338 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 2339 | ` * $pieces` |
|      - | 2340 | ` *   The array of strings to implode.` |
|      - | 2341 | ` * Return` |
|      - | 2342 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 2343 | ` *  order, with the glue string between each element.` |
|      - | 2344 | ` */` |
|  17088 | 2345 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2346 |  |
|      - | 2347 | `	struct implode_data imp_data;` |
|  17090 | 2348 | `	int i = 1;` |
|  17090 | 2349 | `	if( nArg < 1 ){` |
|      - | 2350 | `		/* Missing argument,return NULL */` |
|    ! 0 | 2351 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2352 | `		return PH7_OK;` |
|      - | 2353 | `	}` |
|      - | 2354 | `	/* Prepare the implode context */` |
|  17090 | 2355 | `	imp_data.pCtx = pCtx;` |
|  17090 | 2356 | `	imp_data.bRecursive = 0;` |
|  17090 | 2357 | `	imp_data.bFirst = 1;` |
|  17090 | 2358 | `	imp_data.nRecCount = 0;` |
|  17090 | 2359 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  17088 | 2360 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|   8545 | 2361 | `	}else{` |
|      3 | 2362 | `		imp_data.zSep = 0;` |
|      3 | 2363 | `		imp_data.nSeplen = 0;` |
|      3 | 2364 | `		i = 0;` |
|      - | 2365 | `	}` |
|  17090 | 2366 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 2367 | `	/* Start the 'join' process */` |
|  34178 | 2368 | `	while( i < nArg ){` |
|  17090 | 2369 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2370 | `			/* Iterate throw array entries */` |
|  17090 | 2371 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|   8546 | 2372 | `		}else{` |
|      - | 2373 | `			const char *zData;` |
|      - | 2374 | `			int nLen;` |
|      - | 2375 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 2376 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2377 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 2378 | `			if( imp_data.bFirst ){` |
|    ! 0 | 2379 | `				imp_data.bFirst = 0;` |
|    ! 0 | 2380 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2381 | `				ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen);` |
|    ! 0 | 2382 | `			}` |
|      - | 2383 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 2384 | `			if( nLen > 0 ){` |
|    ! 0 | 2385 | `				ph7_result_string(pCtx,zData,nLen);` |
|    ! 0 | 2386 | `			}` |
|      - | 2387 | `		}` |
|  17090 | 2388 | `		i++;` |
|      2 | 2389 | `	}` |
|  17090 | 2390 | `	return PH7_OK;` |
|   8546 | 2391 |  |
|      - | 2392 | `/*` |
|      - | 2393 | ` * Symisc eXtension:` |
|      - | 2394 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 2395 | ` * Purpose` |
|      - | 2396 | ` *  Same as implode() but recurse on arrays.` |
|      - | 2397 | ` * Example:` |
|      - | 2398 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 2399 | ` *   echo implode_recursive("/",$a);` |
|      - | 2400 | ` *   Will output` |
|      - | 2401 | ` *     usr/home/dean.` |
|      - | 2402 | ` *   While the standard implode would produce.` |
|      - | 2403 | ` *    usr/Array.` |
|      - | 2404 | ` * Parameter` |
|      - | 2405 | ` *  Refer to implode().` |
|      - | 2406 | ` * Return` |
|      - | 2407 | ` *  Refer to implode().` |
|      - | 2408 | ` */` |
|     12 | 2409 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2410 |  |
|      - | 2411 | `	struct implode_data imp_data;` |
|     13 | 2412 | `	int i = 1;` |
|     13 | 2413 | `	if( nArg < 1 ){` |
|      - | 2414 | `		/* Missing argument,return NULL */` |
|      3 | 2415 | `		ph7_result_null(pCtx);` |
|      3 | 2416 | `		return PH7_OK;` |
|      - | 2417 | `	}` |
|      - | 2418 | `	/* Prepare the implode context */` |
|     11 | 2419 | `	imp_data.pCtx = pCtx;` |
|     11 | 2420 | `	imp_data.bRecursive = 1;` |
|     11 | 2421 | `	imp_data.bFirst = 1;` |
|     11 | 2422 | `	imp_data.nRecCount = 0;` |
|     11 | 2423 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 2424 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 2425 | `	}else{` |
|    ! 0 | 2426 | `		imp_data.zSep = 0;` |
|    ! 0 | 2427 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 2428 | `		i = 0;` |
|      - | 2429 | `	}` |
|     11 | 2430 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 2431 | `	/* Start the 'join' process */` |
|     21 | 2432 | `	while( i < nArg ){` |
|     11 | 2433 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2434 | `			/* Iterate throw array entries */` |
|      3 | 2435 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      2 | 2436 | `		}else{` |
|      - | 2437 | `			const char *zData;` |
|      - | 2438 | `			int nLen;` |
|      - | 2439 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 2440 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2441 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 2442 | `			if( imp_data.bFirst ){` |
|      9 | 2443 | `				imp_data.bFirst = 0;` |
|      4 | 2444 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2445 | `				ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen);` |
|    ! 0 | 2446 | `			}` |
|      - | 2447 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 2448 | `			if( nLen > 0 ){` |
|      9 | 2449 | `				ph7_result_string(pCtx,zData,nLen);` |
|      4 | 2450 | `			}` |
|      - | 2451 | `		}` |
|     11 | 2452 | `		i++;` |
|      1 | 2453 | `	}` |
|     11 | 2454 | `	return PH7_OK;` |
|      7 | 2455 |  |
|      - | 2456 | `/*` |
|      - | 2457 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 2458 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 2459 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 2460 | ` * Parameters` |
|      - | 2461 | ` *  $delimiter` |
|      - | 2462 | ` *   The boundary string.` |
|      - | 2463 | ` * $string` |
|      - | 2464 | ` *   The input string.` |
|      - | 2465 | ` * $limit` |
|      - | 2466 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 2467 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 2468 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 2469 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 2470 | ` * Returns` |
|      - | 2471 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 2472 | ` *  on boundaries formed by the delimiter.` |
|      - | 2473 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 2474 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 2475 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 2476 | ` *  will be returned.` |
|      - | 2477 | ` * NOTE:` |
|      - | 2478 | ` *  Negative limit is not supported.` |
|      - | 2479 | ` */` |
|   3088 | 2480 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2481 |  |
|      - | 2482 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 2483 | `	int nDelim,nStrlen,iLimit;` |
|      - | 2484 | `	ph7_value *pArray;` |
|      - | 2485 | `	ph7_value *pValue;` |
|      - | 2486 | `	sxu32 nOfft;` |
|      - | 2487 | `	sxi32 rc;` |
|   3090 | 2488 | `	if( nArg < 2 ){` |
|      - | 2489 | `		/* Missing arguments,return FALSE */` |
|      9 | 2490 | `		ph7_result_bool(pCtx,0);` |
|      9 | 2491 | `		return PH7_OK;` |
|      - | 2492 | `	}` |
|      - | 2493 | `	/* Extract the delimiter */` |
|   3082 | 2494 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   3082 | 2495 | `	if( nDelim < 1 ){` |
|      - | 2496 | `		/* Empty delimiter,return FALSE */` |
|      3 | 2497 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2498 | `		return PH7_OK;` |
|      - | 2499 | `	}` |
|      - | 2500 | `	/* Extract the string */` |
|   3080 | 2501 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   3080 | 2502 | `	if( nStrlen < 1 ){` |
|      - | 2503 | `		/* Empty string: return an array with a single empty element (PHP behavior) */` |
|      3 | 2504 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      3 | 2505 | `		ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      3 | 2506 | `		if( pArrayTmp == 0 \|\| pValueTmp == 0 ){` |
|      - | 2507 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 2508 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2509 | `			return PH7_OK;` |
|      - | 2510 | `		}` |
|      3 | 2511 | `		ph7_value_string(pValueTmp, "", 0);` |
|      3 | 2512 | `		ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp);` |
|      3 | 2513 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      3 | 2514 | `		return PH7_OK;` |
|      - | 2515 | `	}` |
|      - | 2516 | `	/* Point to the end of the string */` |
|   3078 | 2517 | `	zEnd = &zString[nStrlen];` |
|      - | 2518 | `	/* Create the array */` |
|   3078 | 2519 | `	pArray =  ph7_context_new_array(pCtx);` |
|   3078 | 2520 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   3078 | 2521 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2522 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 2523 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2524 | `		return PH7_OK;` |
|      - | 2525 | `	}` |
|      - | 2526 | `	/* Set a defualt limit */` |
|   3078 | 2527 | `	iLimit = SXI32_HIGH;` |
|   3078 | 2528 | `	if( nArg > 2 ){` |
|      9 | 2529 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|      9 | 2530 | `		 if( iLimit < 0 ){` |
|      3 | 2531 | `			iLimit = -iLimit;` |
|      1 | 2532 | `		}` |
|      9 | 2533 | `		if( iLimit == 0 ){` |
|      3 | 2534 | `			iLimit = 1;` |
|      1 | 2535 | `		}` |
|      9 | 2536 | `		iLimit--;` |
|      4 | 2537 | `	}` |
|      - | 2538 | `	/* Start exploding */` |
|  37843 | 2539 | `	for(;;){` |
|  75688 | 2540 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
|  75688 | 2541 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 2542 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   3078 | 2543 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   3078 | 2544 | `			ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|   3078 | 2545 | `			break;` |
|      - | 2546 | `		}` |
|      - | 2547 | `		/* Point to the desired offset */` |
|  72612 | 2548 | `		zCur = &zString[nOfft];` |
|      - | 2549 | `		/* Perform the store operation (may be empty) */` |
|  72612 | 2550 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|  72612 | 2551 | `		ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|      - | 2552 | `		/* Point beyond the delimiter */` |
|  72612 | 2553 | `		zString = &zCur[nDelim];` |
|      - | 2554 | `		/* Reset the cursor */` |
|  72612 | 2555 | `		ph7_value_reset_string_cursor(pValue);` |
|      2 | 2556 | `	}` |
|      - | 2557 | `	/* Return the freshly created array */` |
|   3078 | 2558 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2559 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 2560 | `	 * released as soon we return from this foregin function.` |
|      - | 2561 | `	 */` |
|   3078 | 2562 | `	return PH7_OK;` |
|   1546 | 2563 |  |
|      - | 2564 | `/*` |
|      - | 2565 | ` * string trim(string $str[,string $charlist ])` |
|      - | 2566 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2567 | ` * Parameters` |
|      - | 2568 | ` *  $str` |
|      - | 2569 | ` *   The string that will be trimmed.` |
|      - | 2570 | ` * $charlist` |
|      - | 2571 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2572 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2573 | ` *   With .. you can specify a range of characters.` |
|      - | 2574 | ` * Returns.` |
|      - | 2575 | ` *  Thr processed string.` |
|      - | 2576 | ` * NOTE:` |
|      - | 2577 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 2578 | ` */` |
|   7780 | 2579 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2580 |  |
|      - | 2581 | `	const char *zString;` |
|      - | 2582 | `	int nLen;` |
|   7782 | 2583 | `	if( nArg < 1 ){` |
|      - | 2584 | `		/* Missing arguments,return null */` |
|      3 | 2585 | `		ph7_result_null(pCtx);` |
|      3 | 2586 | `		return PH7_OK;` |
|      - | 2587 | `	}` |
|      - | 2588 | `	/* Extract the target string */` |
|   7780 | 2589 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|   7780 | 2590 | `	if( nLen < 1 ){` |
|      - | 2591 | `		/* Empty string,return */` |
|   1662 | 2592 | `		ph7_result_string(pCtx,"",0);` |
|   1662 | 2593 | `		return PH7_OK;` |
|      - | 2594 | `	}` |
|      - | 2595 | `	/* Start the trim process */` |
|   6120 | 2596 | `	if( nArg < 2 ){` |
|      - | 2597 | `		SyString sStr;` |
|      - | 2598 | `		/* Remove white spaces and NUL bytes */` |
|   6116 | 2599 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  14838 | 2600 | `		SyStringFullTrimSafe(&sStr);` |
|   6116 | 2601 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   3059 | 2602 | `	}else{` |
|      - | 2603 | `		/* Char list */` |
|      - | 2604 | `		const char *zList;` |
|      - | 2605 | `		int nListlen;` |
|      5 | 2606 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 2607 | `		if( nListlen < 1 ){` |
|      - | 2608 | `			/* Return the string unchanged */` |
|      3 | 2609 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2610 | `		}else{` |
|      3 | 2611 | `			const char *zEnd = &zString[nLen];` |
|      3 | 2612 | `			const char *zCur = zString;` |
|      - | 2613 | `			const char *zPtr;` |
|      - | 2614 | `			int i;` |
|      - | 2615 | `			/* Left trim */` |
|      4 | 2616 | `			for(;;){` |
|      9 | 2617 | `				if( zCur >= zEnd ){` |
|    ! 0 | 2618 | `					break;` |
|      - | 2619 | `				}` |
|      9 | 2620 | `				zPtr = zCur;` |
|     17 | 2621 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 2622 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|      7 | 2623 | `						zCur++;` |
|      3 | 2624 | `					}` |
|      5 | 2625 | `				}` |
|      9 | 2626 | `				if( zCur == zPtr ){` |
|      - | 2627 | `					/* No match,break immediately */` |
|      3 | 2628 | `					break;` |
|      - | 2629 | `				}` |
|      1 | 2630 | `			}` |
|      - | 2631 | `			/* Right trim */` |
|      3 | 2632 | `			zEnd--;` |
|      4 | 2633 | `			for(;;){` |
|      9 | 2634 | `				if( zEnd <= zCur ){` |
|    ! 0 | 2635 | `					break;` |
|      - | 2636 | `				}` |
|      9 | 2637 | `				zPtr = zEnd;` |
|     17 | 2638 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 2639 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      7 | 2640 | `						zEnd--;` |
|      3 | 2641 | `					}` |
|      5 | 2642 | `				}` |
|      9 | 2643 | `				if( zEnd == zPtr ){` |
|      3 | 2644 | `					break;` |
|      - | 2645 | `				}` |
|      1 | 2646 | `			}` |
|      3 | 2647 | `			if( zCur >= zEnd ){` |
|      - | 2648 | `				/* Return the empty string */` |
|    ! 0 | 2649 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2650 | `			}else{` |
|      3 | 2651 | `				zEnd++;` |
|      3 | 2652 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2653 | `			}` |
|      - | 2654 | `		}` |
|      - | 2655 | `	}` |
|   6120 | 2656 | `	return PH7_OK;` |
|   3892 | 2657 |  |
|      - | 2658 | `/*` |
|      - | 2659 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 2660 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 2661 | ` * Parameters` |
|      - | 2662 | ` *  $str` |
|      - | 2663 | ` *   The string that will be trimmed.` |
|      - | 2664 | ` * $charlist` |
|      - | 2665 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2666 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2667 | ` *   With .. you can specify a range of characters.` |
|      - | 2668 | ` * Returns.` |
|      - | 2669 | ` *  Thr processed string.` |
|      - | 2670 | ` * NOTE:` |
|      - | 2671 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 2672 | ` */` |
|     26 | 2673 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2674 |  |
|      - | 2675 | `	const char *zString;` |
|      - | 2676 | `	int nLen;` |
|     27 | 2677 | `	if( nArg < 1 ){` |
|      - | 2678 | `		/* Missing arguments,return null */` |
|      3 | 2679 | `		ph7_result_null(pCtx);` |
|      3 | 2680 | `		return PH7_OK;` |
|      - | 2681 | `	}` |
|      - | 2682 | `	/* Extract the target string */` |
|     25 | 2683 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 2684 | `	if( nLen < 1 ){` |
|      - | 2685 | `		/* Empty string,return */` |
|      5 | 2686 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2687 | `		return PH7_OK;` |
|      - | 2688 | `	}` |
|      - | 2689 | `	/* Start the trim process */` |
|     21 | 2690 | `	if( nArg < 2 ){` |
|      - | 2691 | `		SyString sStr;` |
|      - | 2692 | `		/* Remove white spaces and NUL bytes*/` |
|     17 | 2693 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     41 | 2694 | `		SyStringRightTrimSafe(&sStr);` |
|     17 | 2695 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      9 | 2696 | `	}else{` |
|      - | 2697 | `		/* Char list */` |
|      - | 2698 | `		const char *zList;` |
|      - | 2699 | `		int nListlen;` |
|      5 | 2700 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 2701 | `		if( nListlen < 1 ){` |
|      - | 2702 | `			/* Return the string unchanged */` |
|    ! 0 | 2703 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 2704 | `		}else{` |
|      5 | 2705 | `			const char *zEnd = &zString[nLen - 1];` |
|      5 | 2706 | `			const char *zCur = zString;` |
|      - | 2707 | `			const char *zPtr;` |
|      - | 2708 | `			int i;` |
|      - | 2709 | `			/* Right trim */` |
|      6 | 2710 | `			for(;;){` |
|     13 | 2711 | `				if( zEnd <= zCur ){` |
|    ! 0 | 2712 | `					break;` |
|      - | 2713 | `				}` |
|     13 | 2714 | `				zPtr = zEnd;` |
|     25 | 2715 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     13 | 2716 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      9 | 2717 | `						zEnd--;` |
|      4 | 2718 | `					}` |
|      7 | 2719 | `				}` |
|     13 | 2720 | `				if( zEnd == zPtr ){` |
|      5 | 2721 | `					break;` |
|      - | 2722 | `				}` |
|      1 | 2723 | `			}` |
|      5 | 2724 | `			if( zEnd <= zCur ){` |
|      - | 2725 | `				/* Return the empty string */` |
|    ! 0 | 2726 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2727 | `			}else{` |
|      5 | 2728 | `				zEnd++;` |
|      5 | 2729 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2730 | `			}` |
|      - | 2731 | `		}` |
|      - | 2732 | `	}` |
|     21 | 2733 | `	return PH7_OK;` |
|     14 | 2734 |  |
|      - | 2735 | `/*` |
|      - | 2736 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 2737 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2738 | ` * Parameters` |
|      - | 2739 | ` *  $str` |
|      - | 2740 | ` *   The string that will be trimmed.` |
|      - | 2741 | ` * $charlist` |
|      - | 2742 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2743 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2744 | ` *   With .. you can specify a range of characters.` |
|      - | 2745 | ` * Returns.` |
|      - | 2746 | ` *  Thr processed string.` |
|      - | 2747 | ` * NOTE:` |
|      - | 2748 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 2749 | ` */` |
|     12 | 2750 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2751 |  |
|      - | 2752 | `	const char *zString;` |
|      - | 2753 | `	int nLen;` |
|     13 | 2754 | `	if( nArg < 1 ){` |
|      - | 2755 | `		/* Missing arguments,return null */` |
|      3 | 2756 | `		ph7_result_null(pCtx);` |
|      3 | 2757 | `		return PH7_OK;` |
|      - | 2758 | `	}` |
|      - | 2759 | `	/* Extract the target string */` |
|     11 | 2760 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 2761 | `	if( nLen < 1 ){` |
|      - | 2762 | `		/* Empty string,return */` |
|    ! 0 | 2763 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2764 | `		return PH7_OK;` |
|      - | 2765 | `	}` |
|      - | 2766 | `	/* Start the trim process */` |
|     11 | 2767 | `	if( nArg < 2 ){` |
|      - | 2768 | `		SyString sStr;` |
|      - | 2769 | `		/* Remove white spaces and NUL byte */` |
|      3 | 2770 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|      8 | 2771 | `		SyStringLeftTrimSafe(&sStr);` |
|      3 | 2772 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      2 | 2773 | `	}else{` |
|      - | 2774 | `		/* Char list */` |
|      - | 2775 | `		const char *zList;` |
|      - | 2776 | `		int nListlen;` |
|      9 | 2777 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      9 | 2778 | `		if( nListlen < 1 ){` |
|      - | 2779 | `			/* Return the string unchanged */` |
|      3 | 2780 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2781 | `		}else{` |
|      7 | 2782 | `			const char *zEnd = &zString[nLen];` |
|      7 | 2783 | `			const char *zCur = zString;` |
|      - | 2784 | `			const char *zPtr;` |
|      - | 2785 | `			int i;` |
|      - | 2786 | `			/* Left trim */` |
|      7 | 2787 | `			for(;;){` |
|     15 | 2788 | `				if( zCur >= zEnd ){` |
|    ! 0 | 2789 | `					break;` |
|      - | 2790 | `				}` |
|     15 | 2791 | `				zPtr = zCur;` |
|     41 | 2792 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     27 | 2793 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|     13 | 2794 | `						zCur++;` |
|      6 | 2795 | `					}` |
|     14 | 2796 | `				}` |
|     15 | 2797 | `				if( zCur == zPtr ){` |
|      - | 2798 | `					/* No match,break immediately */` |
|      7 | 2799 | `					break;` |
|      - | 2800 | `				}` |
|      1 | 2801 | `			}` |
|      7 | 2802 | `			if( zCur >= zEnd ){` |
|      - | 2803 | `				/* Return the empty string */` |
|    ! 0 | 2804 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2805 | `			}else{` |
|      7 | 2806 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2807 | `			}` |
|      - | 2808 | `		}` |
|      - | 2809 | `	}` |
|     11 | 2810 | `	return PH7_OK;` |
|      7 | 2811 |  |
|      - | 2812 | `/*` |
|      - | 2813 | ` * string strtolower(string $str)` |
|      - | 2814 | ` *  Make a string lowercase.` |
|      - | 2815 | ` * Parameters` |
|      - | 2816 | ` *  $str` |
|      - | 2817 | ` *   The input string.` |
|      - | 2818 | ` * Returns.` |
|      - | 2819 | ` *  The lowercased string.` |
|      - | 2820 | ` */` |
|  16950 | 2821 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2822 |  |
|      - | 2823 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2824 | `	int nLen;` |
|  16952 | 2825 | `	if( nArg < 1 ){` |
|      - | 2826 | `		/* Missing arguments,return null */` |
|      3 | 2827 | `		ph7_result_null(pCtx);` |
|      3 | 2828 | `		return PH7_OK;` |
|      - | 2829 | `	}` |
|      - | 2830 | `	/* Extract the target string */` |
|  16950 | 2831 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  16950 | 2832 | `	if( nLen < 1 ){` |
|      - | 2833 | `		/* Empty string,return */` |
|      3 | 2834 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2835 | `		return PH7_OK;` |
|      - | 2836 | `	}` |
|      - | 2837 | `	/* Perform the requested operation */` |
|  16948 | 2838 | `	zEnd = &zString[nLen];` |
|  53589 | 2839 | `	for(;;){` |
| 107180 | 2840 | `		if( zString >= zEnd ){` |
|      - | 2841 | `			/* No more input,break immediately */` |
|  16948 | 2842 | `			break;` |
|      - | 2843 | `		}` |
|  90234 | 2844 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2845 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2846 | `			zCur = zString;` |
|    ! 0 | 2847 | `			zString++;` |
|    ! 0 | 2848 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2849 | `				zString++;` |
|    ! 0 | 2850 | `			}` |
|      - | 2851 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2852 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2853 | `		}else{` |
|  90234 | 2854 | `			int c = zString[0];` |
|  90234 | 2855 | `			if( SyisUpper(c) ){` |
|  90232 | 2856 | `				c = SyToLower(zString[0]);` |
|  45115 | 2857 | `			}` |
|      - | 2858 | `			/* Append character */` |
|  90234 | 2859 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2860 | `			/* Advance the cursor */` |
|  90234 | 2861 | `			zString++;` |
|      - | 2862 | `		}` |
|      2 | 2863 | `	}` |
|  16948 | 2864 | `	return PH7_OK;` |
|   8477 | 2865 |  |
|      - | 2866 | `/*` |
|      - | 2867 | ` * string strtolower(string $str)` |
|      - | 2868 | ` *  Make a string uppercase.` |
|      - | 2869 | ` * Parameters` |
|      - | 2870 | ` *  $str` |
|      - | 2871 | ` *   The input string.` |
|      - | 2872 | ` * Returns.` |
|      - | 2873 | ` *  The uppercased string.` |
|      - | 2874 | ` */` |
|     10 | 2875 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2876 |  |
|      - | 2877 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2878 | `	int nLen;` |
|     11 | 2879 | `	if( nArg < 1 ){` |
|      - | 2880 | `		/* Missing arguments,return null */` |
|      3 | 2881 | `		ph7_result_null(pCtx);` |
|      3 | 2882 | `		return PH7_OK;` |
|      - | 2883 | `	}` |
|      - | 2884 | `	/* Extract the target string */` |
|      9 | 2885 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 2886 | `	if( nLen < 1 ){` |
|      - | 2887 | `		/* Empty string,return */` |
|      3 | 2888 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2889 | `		return PH7_OK;` |
|      - | 2890 | `	}` |
|      - | 2891 | `	/* Perform the requested operation */` |
|      7 | 2892 | `	zEnd = &zString[nLen];` |
|     19 | 2893 | `	for(;;){` |
|     39 | 2894 | `		if( zString >= zEnd ){` |
|      - | 2895 | `			/* No more input,break immediately */` |
|      7 | 2896 | `			break;` |
|      - | 2897 | `		}` |
|     33 | 2898 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2899 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2900 | `			zCur = zString;` |
|    ! 0 | 2901 | `			zString++;` |
|    ! 0 | 2902 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2903 | `				zString++;` |
|    ! 0 | 2904 | `			}` |
|      - | 2905 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2906 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2907 | `		}else{` |
|     33 | 2908 | `			int c = zString[0];` |
|     33 | 2909 | `			if( SyisLower(c) ){` |
|     27 | 2910 | `				c = SyToUpper(zString[0]);` |
|     13 | 2911 | `			}` |
|      - | 2912 | `			/* Append character */` |
|     33 | 2913 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2914 | `			/* Advance the cursor */` |
|     33 | 2915 | `			zString++;` |
|      - | 2916 | `		}` |
|      1 | 2917 | `	}` |
|      7 | 2918 | `	return PH7_OK;` |
|      6 | 2919 |  |
|      - | 2920 | `/*` |
|      - | 2921 | ` * string ucfirst(string $str)` |
|      - | 2922 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2923 | ` *  character is alphabetic.` |
|      - | 2924 | ` * Parameters` |
|      - | 2925 | ` *  $str` |
|      - | 2926 | ` *   The input string.` |
|      - | 2927 | ` * Returns.` |
|      - | 2928 | ` *  The processed string.` |
|      - | 2929 | ` */` |
|      6 | 2930 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2931 |  |
|      - | 2932 | `	const char *zString,*zEnd;` |
|      - | 2933 | `	int nLen,c;` |
|      7 | 2934 | `	if( nArg < 1 ){` |
|      - | 2935 | `		/* Missing arguments,return null */` |
|      3 | 2936 | `		ph7_result_null(pCtx);` |
|      3 | 2937 | `		return PH7_OK;` |
|      - | 2938 | `	}` |
|      - | 2939 | `	/* Extract the target string */` |
|      5 | 2940 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2941 | `	if( nLen < 1 ){` |
|      - | 2942 | `		/* Empty string,return */` |
|      3 | 2943 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2944 | `		return PH7_OK;` |
|      - | 2945 | `	}` |
|      - | 2946 | `	/* Perform the requested operation */` |
|      3 | 2947 | `	zEnd = &zString[nLen];` |
|      3 | 2948 | `	c = zString[0];` |
|      3 | 2949 | `	if( SyisLower(c) ){` |
|      3 | 2950 | `		c = SyToUpper(c);` |
|      1 | 2951 | `	}` |
|      - | 2952 | `	/* Append the first character */` |
|      3 | 2953 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2954 | `	zString++;` |
|      3 | 2955 | `	if( zString < zEnd ){` |
|      - | 2956 | `		/* Append the rest of the input verbatim */` |
|      3 | 2957 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2958 | `	}` |
|      3 | 2959 | `	return PH7_OK;` |
|      4 | 2960 |  |
|      - | 2961 | `/*` |
|      - | 2962 | ` * string lcfirst(string $str)` |
|      - | 2963 | ` *  Make a string's first character lowercase.` |
|      - | 2964 | ` * Parameters` |
|      - | 2965 | ` *  $str` |
|      - | 2966 | ` *   The input string.` |
|      - | 2967 | ` * Returns.` |
|      - | 2968 | ` *  The processed string.` |
|      - | 2969 | ` */` |
|      6 | 2970 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2971 |  |
|      - | 2972 | `	const char *zString,*zEnd;` |
|      - | 2973 | `	int nLen,c;` |
|      7 | 2974 | `	if( nArg < 1 ){` |
|      - | 2975 | `		/* Missing arguments,return null */` |
|      3 | 2976 | `		ph7_result_null(pCtx);` |
|      3 | 2977 | `		return PH7_OK;` |
|      - | 2978 | `	}` |
|      - | 2979 | `	/* Extract the target string */` |
|      5 | 2980 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2981 | `	if( nLen < 1 ){` |
|      - | 2982 | `		/* Empty string,return */` |
|      3 | 2983 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2984 | `		return PH7_OK;` |
|      - | 2985 | `	}` |
|      - | 2986 | `	/* Perform the requested operation */` |
|      3 | 2987 | `	zEnd = &zString[nLen];` |
|      3 | 2988 | `	c = zString[0];` |
|      3 | 2989 | `	if( SyisUpper(c) ){` |
|      3 | 2990 | `		c = SyToLower(c);` |
|      1 | 2991 | `	}` |
|      - | 2992 | `	/* Append the first character */` |
|      3 | 2993 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2994 | `	zString++;` |
|      3 | 2995 | `	if( zString < zEnd ){` |
|      - | 2996 | `		/* Append the rest of the input verbatim */` |
|      3 | 2997 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2998 | `	}` |
|      3 | 2999 | `	return PH7_OK;` |
|      4 | 3000 |  |
|      - | 3001 | `/*` |
|      - | 3002 | ` * int ord(string $string)` |
|      - | 3003 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 3004 | ` * Parameters` |
|      - | 3005 | ` *  $str` |
|      - | 3006 | ` *   The input string.` |
|      - | 3007 | ` * Returns.` |
|      - | 3008 | ` *  The ASCII value as an integer.` |
|      - | 3009 | ` */` |
|     32 | 3010 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3011 |  |
|      - | 3012 | `	const char *zString;` |
|      - | 3013 | `	int nLen,c;` |
|     33 | 3014 | `	if( nArg < 1 ){` |
|      - | 3015 | `		/* Missing arguments,return -1 */` |
|      3 | 3016 | `		ph7_result_int(pCtx,-1);` |
|      3 | 3017 | `		return PH7_OK;` |
|      - | 3018 | `	}` |
|      - | 3019 | `	/* Extract the target string */` |
|     31 | 3020 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 3021 | `	if( nLen < 1 ){` |
|      - | 3022 | `		/* Empty string,return -1 */` |
|      3 | 3023 | `		ph7_result_int(pCtx,-1);` |
|      3 | 3024 | `		return PH7_OK;` |
|      - | 3025 | `	}` |
|      - | 3026 | `	/* Extract the ASCII value of the first character */` |
|     29 | 3027 | `	c = zString[0];` |
|      - | 3028 | `	/* Return that value */` |
|     29 | 3029 | `	ph7_result_int(pCtx,c);` |
|     29 | 3030 | `	return PH7_OK;` |
|     17 | 3031 |  |
|      - | 3032 | `/*` |
|      - | 3033 | ` * string chr(int $ascii)` |
|      - | 3034 | ` *  Returns a one-character string containing the character specified by ascii.` |
|      - | 3035 | ` * Parameters` |
|      - | 3036 | ` *  $ascii` |
|      - | 3037 | ` *   The ascii code.` |
|      - | 3038 | ` * Returns.` |
|      - | 3039 | ` *  The specified character.` |
|      - | 3040 | ` */` |
|     28 | 3041 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3042 |  |
|      - | 3043 | `	int c;` |
|     29 | 3044 | `	if( nArg < 1 ){` |
|      - | 3045 | `		/* Missing arguments,return null */` |
|      3 | 3046 | `		ph7_result_null(pCtx);` |
|      3 | 3047 | `		return PH7_OK;` |
|      - | 3048 | `	}` |
|      - | 3049 | `	/* Extract the ASCII value */` |
|     27 | 3050 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 3051 | `	/* Return the specified character */` |
|     27 | 3052 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     27 | 3053 | `	return PH7_OK;` |
|     15 | 3054 |  |
|      - | 3055 | `/*` |
|      - | 3056 | ` * Binary to hex consumer callback.` |
|      - | 3057 | ` * This callback is the default consumer used by the hash functions` |
|      - | 3058 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 3059 | ` */` |
|    226 | 3060 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 3061 |  |
|      - | 3062 | `	/* Append hex chunk verbatim */` |
|    227 | 3063 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|    227 | 3064 | `	return SXRET_OK;` |
|      1 | 3065 |  |
|      - | 3066 |  |
|      - | 3067 | `/*` |
|      - | 3068 | ` * string bin2hex(string $str)` |
|      - | 3069 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 3070 | ` * Parameters` |
|      - | 3071 | ` *  $str` |
|      - | 3072 | ` *   The input string.` |
|      - | 3073 | ` * Returns.` |
|      - | 3074 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 3075 | ` */` |
|     12 | 3076 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3077 |  |
|      - | 3078 | `	const char *zString;` |
|      - | 3079 | `	int nLen;` |
|     13 | 3080 | `	if( nArg < 1 ){` |
|      - | 3081 | `		/* Missing arguments,return null */` |
|      3 | 3082 | `		ph7_result_null(pCtx);` |
|      3 | 3083 | `		return PH7_OK;` |
|      - | 3084 | `	}` |
|      - | 3085 | `	/* Extract the target string */` |
|     11 | 3086 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 3087 | `	if( nLen < 1 ){` |
|      - | 3088 | `		/* Empty string,return */` |
|      3 | 3089 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3090 | `		return PH7_OK;` |
|      - | 3091 | `	}` |
|      - | 3092 | `	/* Perform the requested operation */` |
|      9 | 3093 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|      9 | 3094 | `	return PH7_OK;` |
|      7 | 3095 |  |
|      - | 3096 |  |
|      - | 3097 | `/* Search callback signature */` |
|      - | 3098 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 3099 | `/*` |
|      - | 3100 | ` * Case-insensitive pattern match.` |
|      - | 3101 | ` * Brute force is the default search method used here.` |
|      - | 3102 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 3103 | ` * well for short/medium texts on modern hardware.` |
|      - | 3104 | ` */` |
|    118 | 3105 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      1 | 3106 |  |
|    119 | 3107 | `	const char *zpIn = (const char *)pPattern;` |
|    119 | 3108 | `	const char *zIn = (const char *)pText;` |
|    119 | 3109 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    119 | 3110 | `	const char *zEnd = &zIn[nLen];` |
|      - | 3111 | `	const char *zPtr,*zPtr2;` |
|      - | 3112 | `	int c,d;` |
|    119 | 3113 | `	if( iPatLen > nLen ){` |
|      - | 3114 | `		/* Don't bother processing */` |
|     33 | 3115 | `		return SXERR_NOTFOUND;` |
|      - | 3116 | `	}` |
|    244 | 3117 | `	for(;;){` |
|    489 | 3118 | `		if( zIn >= zEnd ){` |
|     47 | 3119 | `			break;` |
|      - | 3120 | `		}` |
|    443 | 3121 | `		c = SyToLower(zIn[0]);` |
|    443 | 3122 | `		d = SyToLower(zpIn[0]);` |
|    443 | 3123 | `		if( c == d ){` |
|     41 | 3124 | `			zPtr   = &zIn[1];` |
|     41 | 3125 | `			zPtr2  = &zpIn[1];` |
|     71 | 3126 | `			for(;;){` |
|    143 | 3127 | `				if( zPtr2 >= zpEnd ){` |
|      - | 3128 | `					/* Pattern found */` |
|     41 | 3129 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 3130 | `					return SXRET_OK;` |
|      - | 3131 | `				}` |
|    103 | 3132 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 3133 | `					break;` |
|      - | 3134 | `				}` |
|    103 | 3135 | `				c = SyToLower(zPtr[0]);` |
|    103 | 3136 | `				d = SyToLower(zPtr2[0]);` |
|    103 | 3137 | `				if( c != d ){` |
|    ! 0 | 3138 | `					break;` |
|      - | 3139 | `				}` |
|    103 | 3140 | `				zPtr++; zPtr2++;` |
|      1 | 3141 | `			}` |
|    ! 0 | 3142 | `		}` |
|    403 | 3143 | `		zIn++;` |
|      1 | 3144 | `	}` |
|      - | 3145 | `	/* Pattern not found */` |
|     47 | 3146 | `	return SXERR_NOTFOUND;` |
|     60 | 3147 |  |
|      - | 3148 | `/*` |
|      - | 3149 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3150 | ` *  Find the first occurrence of a string.` |
|      - | 3151 | ` * Parameters` |
|      - | 3152 | ` *  $haystack` |
|      - | 3153 | ` *   The input string.` |
|      - | 3154 | ` * $needle` |
|      - | 3155 | ` *   Search pattern (must be a string).` |
|      - | 3156 | ` * $before_needle` |
|      - | 3157 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3158 | ` *   of the needle (excluding the needle).` |
|      - | 3159 | ` * Return` |
|      - | 3160 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3161 | ` */` |
|     10 | 3162 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3163 |  |
|     11 | 3164 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3165 | `	const char *zBlob,*zPattern;` |
|      - | 3166 | `	int nLen,nPatLen;` |
|      - | 3167 | `	sxu32 nOfft;` |
|      - | 3168 | `	sxi32 rc;` |
|     11 | 3169 | `	if( nArg < 2 ){` |
|      - | 3170 | `		/* Missing arguments,return FALSE */` |
|      5 | 3171 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3172 | `		return PH7_OK;` |
|      - | 3173 | `	}` |
|      - | 3174 | `	/* Extract the needle and the haystack */` |
|      7 | 3175 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 3176 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 3177 | `	nOfft = 0; /* cc warning */` |
|      9 | 3178 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3179 | `		int before = 0;` |
|      - | 3180 | `		/* Perform the lookup */` |
|      5 | 3181 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3182 | `		if( rc != SXRET_OK ){` |
|      - | 3183 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3184 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3185 | `			return PH7_OK;` |
|      - | 3186 | `		}` |
|      - | 3187 | `		/* Return the portion of the string */` |
|      5 | 3188 | `		if( nArg > 2 ){` |
|      3 | 3189 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3190 | `		}` |
|      5 | 3191 | `		if( before ){` |
|      3 | 3192 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3193 | `		}else{` |
|      3 | 3194 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3195 | `		}` |
|      3 | 3196 | `	}else{` |
|      3 | 3197 | `		ph7_result_bool(pCtx,0);` |
|      - | 3198 | `	}` |
|      7 | 3199 | `	return PH7_OK;` |
|      6 | 3200 |  |
|      - | 3201 | `/*` |
|      - | 3202 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3203 | ` *  Case-insensitive strstr().` |
|      - | 3204 | ` * Parameters` |
|      - | 3205 | ` *  $haystack` |
|      - | 3206 | ` *   The input string.` |
|      - | 3207 | ` * $needle` |
|      - | 3208 | ` *   Search pattern (must be a string).` |
|      - | 3209 | ` * $before_needle` |
|      - | 3210 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3211 | ` *   of the needle (excluding the needle).` |
|      - | 3212 | ` * Return` |
|      - | 3213 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3214 | ` */` |
|      6 | 3215 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3216 |  |
|      7 | 3217 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3218 | `	const char *zBlob,*zPattern;` |
|      - | 3219 | `	int nLen,nPatLen;` |
|      - | 3220 | `	sxu32 nOfft;` |
|      - | 3221 | `	sxi32 rc;` |
|      7 | 3222 | `	if( nArg < 2 ){` |
|      - | 3223 | `		/* Missing arguments,return FALSE */` |
|      3 | 3224 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3225 | `		return PH7_OK;` |
|      - | 3226 | `	}` |
|      - | 3227 | `	/* Extract the needle and the haystack */` |
|      5 | 3228 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3229 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 3230 | `	nOfft = 0; /* cc warning */` |
|      7 | 3231 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3232 | `		int before = 0;` |
|      - | 3233 | `		/* Perform the lookup */` |
|      5 | 3234 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3235 | `		if( rc != SXRET_OK ){` |
|      - | 3236 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3237 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3238 | `			return PH7_OK;` |
|      - | 3239 | `		}` |
|      - | 3240 | `		/* Return the portion of the string */` |
|      5 | 3241 | `		if( nArg > 2 ){` |
|      3 | 3242 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3243 | `		}` |
|      5 | 3244 | `		if( before ){` |
|      3 | 3245 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3246 | `		}else{` |
|      3 | 3247 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3248 | `		}` |
|      3 | 3249 | `	}else{` |
|    ! 0 | 3250 | `		ph7_result_bool(pCtx,0);` |
|      - | 3251 | `	}` |
|      5 | 3252 | `	return PH7_OK;` |
|      4 | 3253 |  |
|      - | 3254 | `/*` |
|      - | 3255 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3256 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 3257 | ` * Parameters` |
|      - | 3258 | ` *  $haystack` |
|      - | 3259 | ` *   The input string.` |
|      - | 3260 | ` * $needle` |
|      - | 3261 | ` *   Search pattern (must be a string).` |
|      - | 3262 | ` * $offset` |
|      - | 3263 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3264 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3265 | ` *   of haystack.` |
|      - | 3266 | ` * Return` |
|      - | 3267 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3268 | ` */` |
|     80 | 3269 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3270 |  |
|     82 | 3271 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3272 | `	const char *zBlob,*zPattern;` |
|      - | 3273 | `	int nLen,nPatLen,nStart;` |
|      - | 3274 | `	sxu32 nOfft;` |
|      - | 3275 | `	sxi32 rc;` |
|     82 | 3276 | `	if( nArg < 2 ){` |
|      - | 3277 | `		/* Missing arguments,return FALSE */` |
|      7 | 3278 | `		ph7_result_bool(pCtx,0);` |
|      7 | 3279 | `		return PH7_OK;` |
|      - | 3280 | `	}` |
|      - | 3281 | `	/* Extract the needle and the haystack */` |
|     76 | 3282 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     76 | 3283 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     76 | 3284 | `	nOfft = 0; /* cc warning */` |
|     76 | 3285 | `	nStart = 0;` |
|      - | 3286 | `	/* Peek the starting offset if available */` |
|     76 | 3287 | `	if( nArg > 2 ){` |
|    ! 0 | 3288 | `		nStart = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 3289 | `		if( nStart < 0 ){` |
|    ! 0 | 3290 | `			nStart = -nStart;` |
|    ! 0 | 3291 | `		}` |
|    ! 0 | 3292 | `		if( nStart >= nLen ){` |
|      - | 3293 | `			/* Invalid offset */` |
|    ! 0 | 3294 | `			nStart = 0;` |
|    ! 0 | 3295 | `		}else{` |
|    ! 0 | 3296 | `			zBlob += nStart;` |
|    ! 0 | 3297 | `			nLen -= nStart;` |
|      - | 3298 | `		}` |
|    ! 0 | 3299 | `	}` |
|     76 | 3300 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3301 | `		/* Perform the lookup */` |
|     74 | 3302 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     74 | 3303 | `		if( rc != SXRET_OK ){` |
|      - | 3304 | `			/* Pattern not found,return FALSE */` |
|      3 | 3305 | `			ph7_result_bool(pCtx,0);` |
|      3 | 3306 | `			return PH7_OK;` |
|      - | 3307 | `		}` |
|      - | 3308 | `		/* Return the pattern position */` |
|     72 | 3309 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|     37 | 3310 | `	}else{` |
|      3 | 3311 | `		ph7_result_bool(pCtx,0);` |
|      - | 3312 | `	}` |
|     74 | 3313 | `	return PH7_OK;` |
|     42 | 3314 |  |
|      - | 3315 | `/*` |
|      - | 3316 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3317 | ` *  Case-insensitive strpos.` |
|      - | 3318 | ` * Parameters` |
|      - | 3319 | ` *  $haystack` |
|      - | 3320 | ` *   The input string.` |
|      - | 3321 | ` * $needle` |
|      - | 3322 | ` *   Search pattern (must be a string).` |
|      - | 3323 | ` * $offset` |
|      - | 3324 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3325 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3326 | ` *   of haystack.` |
|      - | 3327 | ` * Return` |
|      - | 3328 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3329 | ` */` |
|     18 | 3330 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3331 |  |
|     19 | 3332 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3333 | `	const char *zBlob,*zPattern;` |
|      - | 3334 | `	int nLen,nPatLen,nStart;` |
|      - | 3335 | `	sxu32 nOfft;` |
|      - | 3336 | `	sxi32 rc;` |
|     19 | 3337 | `	if( nArg < 2 ){` |
|      - | 3338 | `		/* Missing arguments,return FALSE */` |
|      3 | 3339 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3340 | `		return PH7_OK;` |
|      - | 3341 | `	}` |
|      - | 3342 | `	/* Extract the needle and the haystack */` |
|     17 | 3343 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 3344 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     17 | 3345 | `	nOfft = 0; /* cc warning */` |
|     17 | 3346 | `	nStart = 0;` |
|      - | 3347 | `	/* Peek the starting offset if available */` |
|     17 | 3348 | `	if( nArg > 2 ){` |
|      5 | 3349 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 3350 | `		if( nStart < 0 ){` |
|      3 | 3351 | `			nStart = -nStart;` |
|      1 | 3352 | `		}` |
|      5 | 3353 | `		if( nStart >= nLen ){` |
|      - | 3354 | `			/* Invalid offset */` |
|    ! 0 | 3355 | `			nStart = 0;` |
|    ! 0 | 3356 | `		}else{` |
|      5 | 3357 | `			zBlob += nStart;` |
|      5 | 3358 | `			nLen -= nStart;` |
|      - | 3359 | `		}` |
|      2 | 3360 | `	}` |
|     17 | 3361 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3362 | `		/* Perform the lookup */` |
|     17 | 3363 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     17 | 3364 | `		if( rc != SXRET_OK ){` |
|      - | 3365 | `			/* Pattern not found,return FALSE */` |
|      3 | 3366 | `			ph7_result_bool(pCtx,0);` |
|      3 | 3367 | `			return PH7_OK;` |
|      - | 3368 | `		}` |
|      - | 3369 | `		/* Return the pattern position */` |
|     15 | 3370 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 3371 | `	}else{` |
|    ! 0 | 3372 | `		ph7_result_bool(pCtx,0);` |
|      - | 3373 | `	}` |
|     15 | 3374 | `	return PH7_OK;` |
|     10 | 3375 |  |
|      - | 3376 | `/*` |
|      - | 3377 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3378 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 3379 | ` * Parameters` |
|      - | 3380 | ` *  $haystack` |
|      - | 3381 | ` *   The input string.` |
|      - | 3382 | ` * $needle` |
|      - | 3383 | ` *   Search pattern (must be a string).` |
|      - | 3384 | ` * $offset` |
|      - | 3385 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3386 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3387 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3388 | ` * Return` |
|      - | 3389 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3390 | ` */` |
|     32 | 3391 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3392 |  |
|      - | 3393 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     33 | 3394 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3395 | `	int nLen,nPatLen;` |
|      - | 3396 | `	sxu32 nOfft;` |
|      - | 3397 | `	sxi32 rc;` |
|     33 | 3398 | `	if( nArg < 2 ){` |
|      - | 3399 | `		/* Missing arguments,return FALSE */` |
|      3 | 3400 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3401 | `		return PH7_OK;` |
|      - | 3402 | `	}` |
|      - | 3403 | `	/* Extract the needle and the haystack */` |
|     31 | 3404 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 3405 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3406 | `	/* Point to the end of the pattern */` |
|     31 | 3407 | `	zPtr = &zBlob[nLen - 1];` |
|     31 | 3408 | `	zEnd = &zBlob[nLen];` |
|      - | 3409 | `	/* Save the starting posistion */` |
|     31 | 3410 | `	zStart = zBlob;` |
|     31 | 3411 | `	nOfft = 0; /* cc warning */` |
|      - | 3412 | `	/* Peek the starting offset if available */` |
|     31 | 3413 | `	if( nArg > 2 ){` |
|      - | 3414 | `		int nStart;` |
|     21 | 3415 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 3416 | `		if( nStart < 0 ){` |
|     11 | 3417 | `			nStart = -nStart;` |
|     11 | 3418 | `			if( nStart >= nLen ){` |
|      - | 3419 | `				/* Invalid offset */` |
|      3 | 3420 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3421 | `				return PH7_OK;` |
|    ! 0 | 3422 | `			}else{` |
|      9 | 3423 | `				nLen -= nStart;` |
|      9 | 3424 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 3425 | `				zEnd = &zBlob[nLen];` |
|      - | 3426 | `			}` |
|      5 | 3427 | `		}else{` |
|     11 | 3428 | `			if( nStart >= nLen ){` |
|      - | 3429 | `				/* Invalid offset */` |
|      5 | 3430 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3431 | `				return PH7_OK;` |
|    ! 0 | 3432 | `			}else{` |
|      7 | 3433 | `				zBlob += nStart;` |
|      7 | 3434 | `				nLen -= nStart;` |
|      - | 3435 | `			}` |
|      - | 3436 | `		}` |
|      7 | 3437 | `	}` |
|     25 | 3438 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3439 | `		/* Perform the lookup */` |
|     57 | 3440 | `		for(;;){` |
|    115 | 3441 | `			if( zBlob >= zPtr ){` |
|     11 | 3442 | `				break;` |
|      - | 3443 | `			}` |
|    105 | 3444 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    105 | 3445 | `			if( rc == SXRET_OK ){` |
|      - | 3446 | `				/* Pattern found,return it's position */` |
|     13 | 3447 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 3448 | `				return PH7_OK;` |
|      - | 3449 | `			}` |
|     93 | 3450 | `			zPtr--;` |
|      1 | 3451 | `		}` |
|      - | 3452 | `		/* Pattern not found,return FALSE */` |
|     11 | 3453 | `		ph7_result_bool(pCtx,0);` |
|      6 | 3454 | `	}else{` |
|      3 | 3455 | `		ph7_result_bool(pCtx,0);` |
|      - | 3456 | `	}` |
|     13 | 3457 | `	return PH7_OK;` |
|     17 | 3458 |  |
|      - | 3459 | `/*` |
|      - | 3460 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3461 | ` *  Case-insensitive strrpos.` |
|      - | 3462 | ` * Parameters` |
|      - | 3463 | ` *  $haystack` |
|      - | 3464 | ` *   The input string.` |
|      - | 3465 | ` * $needle` |
|      - | 3466 | ` *   Search pattern (must be a string).` |
|      - | 3467 | ` * $offset` |
|      - | 3468 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3469 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3470 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3471 | ` * Return` |
|      - | 3472 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3473 | ` */` |
|     28 | 3474 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3475 |  |
|      - | 3476 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     29 | 3477 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3478 | `	int nLen,nPatLen;` |
|      - | 3479 | `	sxu32 nOfft;` |
|      - | 3480 | `	sxi32 rc;` |
|     29 | 3481 | `	if( nArg < 2 ){` |
|      - | 3482 | `		/* Missing arguments,return FALSE */` |
|      3 | 3483 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3484 | `		return PH7_OK;` |
|      - | 3485 | `	}` |
|      - | 3486 | `	/* Extract the needle and the haystack */` |
|     27 | 3487 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 3488 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3489 | `	/* Point to the end of the pattern */` |
|     27 | 3490 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 3491 | `	zEnd = &zBlob[nLen];` |
|      - | 3492 | `	/* Save the starting posistion */` |
|     27 | 3493 | `	zStart = zBlob;` |
|     27 | 3494 | `	nOfft = 0; /* cc warning */` |
|      - | 3495 | `	/* Peek the starting offset if available */` |
|     27 | 3496 | `	if( nArg > 2 ){` |
|      - | 3497 | `		int nStart;` |
|     15 | 3498 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3499 | `		if( nStart < 0 ){` |
|      7 | 3500 | `			nStart = -nStart;` |
|      7 | 3501 | `			if( nStart >= nLen ){` |
|      - | 3502 | `				/* Invalid offset */` |
|      3 | 3503 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3504 | `				return PH7_OK;` |
|    ! 0 | 3505 | `			}else{` |
|      5 | 3506 | `				nLen -= nStart;` |
|      5 | 3507 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 3508 | `				zEnd = &zBlob[nLen];` |
|      - | 3509 | `			}` |
|      3 | 3510 | `		}else{` |
|      9 | 3511 | `			if( nStart >= nLen ){` |
|      - | 3512 | `				/* Invalid offset */` |
|      5 | 3513 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3514 | `				return PH7_OK;` |
|    ! 0 | 3515 | `			}else{` |
|      5 | 3516 | `				zBlob += nStart;` |
|      5 | 3517 | `				nLen -= nStart;` |
|      - | 3518 | `			}` |
|      - | 3519 | `		}` |
|      4 | 3520 | `	}` |
|     21 | 3521 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3522 | `		/* Perform the lookup */` |
|     44 | 3523 | `		for(;;){` |
|     89 | 3524 | `			if( zBlob >= zPtr ){` |
|      9 | 3525 | `				break;` |
|      - | 3526 | `			}` |
|     81 | 3527 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 3528 | `			if( rc == SXRET_OK ){` |
|      - | 3529 | `				/* Pattern found,return it's position */` |
|     11 | 3530 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 3531 | `				return PH7_OK;` |
|      - | 3532 | `			}` |
|     71 | 3533 | `			zPtr--;` |
|      1 | 3534 | `		}` |
|      - | 3535 | `		/* Pattern not found,return FALSE */` |
|      9 | 3536 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3537 | `	}else{` |
|      3 | 3538 | `		ph7_result_bool(pCtx,0);` |
|      - | 3539 | `	}` |
|     11 | 3540 | `	return PH7_OK;` |
|     15 | 3541 |  |
|      - | 3542 | `/*` |
|      - | 3543 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3544 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3545 | ` * Parameters` |
|      - | 3546 | ` *  $haystack` |
|      - | 3547 | ` *   The input string.` |
|      - | 3548 | ` * $needle` |
|      - | 3549 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3550 | ` *  This behavior is different from that of strstr().` |
|      - | 3551 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3552 | ` *  as the ordinal value of a character.` |
|      - | 3553 | ` * Return` |
|      - | 3554 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3555 | ` */` |
|     24 | 3556 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3557 |  |
|      - | 3558 | `	const char *zBlob;` |
|      - | 3559 | `	int nLen,c;` |
|     25 | 3560 | `	if( nArg < 2 ){` |
|      - | 3561 | `		/* Missing arguments,return FALSE */` |
|      3 | 3562 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3563 | `		return PH7_OK;` |
|      - | 3564 | `	}` |
|      - | 3565 | `	/* Extract the haystack */` |
|     23 | 3566 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3567 | `	c = 0; /* cc warning */` |
|     23 | 3568 | `	if( nLen > 0 ){` |
|      - | 3569 | `		sxu32 nOfft;` |
|      - | 3570 | `		sxi32 rc;` |
|     21 | 3571 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 3572 | `			const char *zPattern;` |
|     11 | 3573 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 3574 | `														 * for NULL pointer.` |
|      - | 3575 | `														 */` |
|     11 | 3576 | `			c = zPattern[0];` |
|      6 | 3577 | `		}else{` |
|      - | 3578 | `			/* Int cast */` |
|     11 | 3579 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 3580 | `		}` |
|      - | 3581 | `		/* Perform the lookup */` |
|     21 | 3582 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3583 | `		if( rc != SXRET_OK ){` |
|      - | 3584 | `			/* No such entry,return FALSE */` |
|      7 | 3585 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3586 | `			return PH7_OK;` |
|      - | 3587 | `		}` |
|      - | 3588 | `		/* Return the string portion */` |
|     15 | 3589 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 3590 | `	}else{` |
|      3 | 3591 | `		ph7_result_bool(pCtx,0);` |
|      - | 3592 | `	}` |
|     17 | 3593 | `	return PH7_OK;` |
|     13 | 3594 |  |
|      - | 3595 | `/*` |
|      - | 3596 | ` * string strrev(string $string)` |
|      - | 3597 | ` *  Reverse a string.` |
|      - | 3598 | ` * Parameters` |
|      - | 3599 | ` *  $string` |
|      - | 3600 | ` *   String to be reversed.` |
|      - | 3601 | ` * Return` |
|      - | 3602 | ` *  The reversed string.` |
|      - | 3603 | ` */` |
|      4 | 3604 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3605 |  |
|      - | 3606 | `	const char *zIn,*zEnd;` |
|      - | 3607 | `	int nLen,c;` |
|      5 | 3608 | `	if( nArg < 1 ){` |
|      - | 3609 | `		/* Missing arguments,return NULL */` |
|      3 | 3610 | `		ph7_result_null(pCtx);` |
|      3 | 3611 | `		return PH7_OK;` |
|      - | 3612 | `	}` |
|      - | 3613 | `	/* Extract the target string */` |
|      3 | 3614 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3615 | `	if( nLen < 1 ){` |
|      - | 3616 | `		/* Empty string Return null */` |
|    ! 0 | 3617 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3618 | `		return PH7_OK;` |
|      - | 3619 | `	}` |
|      - | 3620 | `	/* Perform the requested operation */` |
|      3 | 3621 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3622 | `	for(;;){` |
|      9 | 3623 | `		if( zEnd < zIn ){` |
|      - | 3624 | `			/* No more input to process */` |
|      3 | 3625 | `			break;` |
|      - | 3626 | `		}` |
|      - | 3627 | `		/* Append current character */` |
|      7 | 3628 | `		c = zEnd[0];` |
|      7 | 3629 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3630 | `		zEnd--;` |
|      1 | 3631 | `	}` |
|      3 | 3632 | `	return PH7_OK;` |
|      3 | 3633 |  |
|      - | 3634 | `/*` |
|      - | 3635 | ` * string ucwords(string $string)` |
|      - | 3636 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3637 | ` *  The definition of a word is any string of characters that is immediately after` |
|      - | 3638 | ` *  a whitespace (These are: space, form-feed, newline, carriage return, horizontal tab, and vertical tab).` |
|      - | 3639 | ` * Parameters` |
|      - | 3640 | ` *  $string` |
|      - | 3641 | ` *   The input string.` |
|      - | 3642 | ` * Return` |
|      - | 3643 | ` *  The modified string..` |
|      - | 3644 | ` */` |
|     14 | 3645 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3646 |  |
|      - | 3647 | `	const char *zIn,*zCur,*zEnd;` |
|      - | 3648 | `	int nLen,c;` |
|     15 | 3649 | `	if( nArg < 1 ){` |
|      - | 3650 | `		/* Missing arguments,return NULL */` |
|      3 | 3651 | `		ph7_result_null(pCtx);` |
|      3 | 3652 | `		return PH7_OK;` |
|      - | 3653 | `	}` |
|      - | 3654 | `	/* Extract the target string */` |
|     13 | 3655 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 3656 | `	if( nLen < 1 ){` |
|      - | 3657 | `		/* Empty string Return null */` |
|      3 | 3658 | `		ph7_result_null(pCtx);` |
|      3 | 3659 | `		return PH7_OK;` |
|      - | 3660 | `	}` |
|      - | 3661 | `	/* Perform the requested operation */` |
|     11 | 3662 | `	zEnd = &zIn[nLen];` |
|     21 | 3663 | `	for(;;){` |
|      - | 3664 | `		/* Jump leading white spaces */` |
|     43 | 3665 | `		zCur = zIn;` |
|     65 | 3666 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0x80 && SyisSpace(zIn[0]) ){` |
|     23 | 3667 | `			zIn++;` |
|      1 | 3668 | `		}` |
|     43 | 3669 | `		if( zCur < zIn ){` |
|      - | 3670 | `			/* Append white space stream */` |
|     23 | 3671 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 3672 | `		}` |
|     43 | 3673 | `		if( zIn >= zEnd ){` |
|      - | 3674 | `			/* No more input to process */` |
|     11 | 3675 | `			break;` |
|      - | 3676 | `		}` |
|     33 | 3677 | `		c = zIn[0];` |
|     33 | 3678 | `		if( c < 0x80 && SyisLower(c) ){` |
|     29 | 3679 | `			c = SyToUpper(c);` |
|     14 | 3680 | `		}` |
|      - | 3681 | `		/* Append the upper-cased character */` |
|     33 | 3682 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     33 | 3683 | `		zIn++;` |
|     33 | 3684 | `		zCur = zIn;` |
|      - | 3685 | `		/* Append the word varbatim */` |
|    149 | 3686 | `		while( zIn < zEnd ){` |
|    139 | 3687 | `			if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 3688 | `				/* UTF-8 stream */` |
|    ! 0 | 3689 | `				zIn++;` |
|    ! 0 | 3690 | `				SX_JMP_UTF8(zIn,zEnd);` |
|    138 | 3691 | `			}else if( !SyisSpace(zIn[0]) ){` |
|    117 | 3692 | `				zIn++;` |
|     59 | 3693 | `			}else{` |
|     23 | 3694 | `				break;` |
|      - | 3695 | `			}` |
|      1 | 3696 | `		}` |
|     33 | 3697 | `		if( zCur < zIn ){` |
|     33 | 3698 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 | 3699 | `		}` |
|      1 | 3700 | `	}` |
|     11 | 3701 | `	return PH7_OK;` |
|      8 | 3702 |  |
|      - | 3703 | `/*` |
|      - | 3704 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3705 | ` *  Returns input repeated multiplier times.` |
|      - | 3706 | ` * Parameters` |
|      - | 3707 | ` *  $string` |
|      - | 3708 | ` *   String to be repeated.` |
|      - | 3709 | ` * $multiplier` |
|      - | 3710 | ` *  Number of time the input string should be repeated.` |
|      - | 3711 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3712 | ` *  to 0, the function will return an empty string.` |
|      - | 3713 | ` * Return` |
|      - | 3714 | ` *  The repeated string.` |
|      - | 3715 | ` */` |
|  20212 | 3716 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3717 |  |
|      - | 3718 | `	const char *zIn;` |
|      - | 3719 | `	int nLen,nMul;` |
|      - | 3720 | `	int rc;` |
|  20213 | 3721 | `	if( nArg < 2 ){` |
|      - | 3722 | `		/* Missing arguments,return NULL */` |
|      3 | 3723 | `		ph7_result_null(pCtx);` |
|      3 | 3724 | `		return PH7_OK;` |
|      - | 3725 | `	}` |
|      - | 3726 | `	/* Extract the target string */` |
|  20211 | 3727 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|  20211 | 3728 | `	if( nLen < 1 ){` |
|      - | 3729 | `		/* Empty string.Return null */` |
|    ! 0 | 3730 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3731 | `		return PH7_OK;` |
|      - | 3732 | `	}` |
|      - | 3733 | `	/* Extract the multiplier */` |
|  20211 | 3734 | `	nMul = ph7_value_to_int(apArg[1]);` |
|  20211 | 3735 | `	if( nMul < 1 ){` |
|      - | 3736 | `		/* Return the empty string */` |
|      3 | 3737 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3738 | `		return PH7_OK;` |
|      - | 3739 | `	}` |
|      - | 3740 | `	/* Perform the requested operation */` |
| 120220 | 3741 | `	for(;;){` |
| 240441 | 3742 | `		if( !nMul ){` |
|  20209 | 3743 | `			break;` |
|      - | 3744 | `		}` |
|      - | 3745 | `		/* Append the copy */` |
| 220233 | 3746 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 220233 | 3747 | `		if( rc != PH7_OK ){` |
|      - | 3748 | `			/* Out of memory,break immediately */` |
|    ! 0 | 3749 | `			break;` |
|      - | 3750 | `		}` |
| 220233 | 3751 | `		nMul--;` |
|      1 | 3752 | `	}` |
|  20209 | 3753 | `	return PH7_OK;` |
|  10107 | 3754 |  |
|      - | 3755 | `/*` |
|      - | 3756 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3757 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3758 | ` * Parameters` |
|      - | 3759 | ` *  $string` |
|      - | 3760 | ` *   The input string.` |
|      - | 3761 | ` * $is_xhtml` |
|      - | 3762 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3763 | ` * Return` |
|      - | 3764 | ` *  The processed string.` |
|      - | 3765 | ` */` |
|      6 | 3766 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3767 |  |
|      - | 3768 | `	const char *zIn,*zCur,*zEnd;` |
|      7 | 3769 | `	int is_xhtml = 1; /* Default to XHTML-style '<br/>' like PHP */` |
|      - | 3770 | `	int nLen;` |
|      7 | 3771 | `	if( nArg < 1 ){` |
|      - | 3772 | `		/* Missing arguments,return the empty string */` |
|      3 | 3773 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3774 | `		return PH7_OK;` |
|      - | 3775 | `	}` |
|      - | 3776 | `	/* Extract the target string */` |
|      5 | 3777 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3778 | `	if( nLen < 1 ){` |
|      - | 3779 | `		/* Empty string,return null */` |
|    ! 0 | 3780 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3781 | `		return PH7_OK;` |
|      - | 3782 | `	}` |
|      5 | 3783 | `	if( nArg > 1 ){` |
|      3 | 3784 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3785 | `	}` |
|      5 | 3786 | `	zEnd = &zIn[nLen];` |
|      - | 3787 | `	/* Perform the requested operation */` |
|      4 | 3788 | `	for(;;){` |
|      9 | 3789 | `		zCur = zIn;` |
|      - | 3790 | `		/* Delimit the string */` |
|     21 | 3791 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3792 | `			zIn++;` |
|      1 | 3793 | `		}` |
|      9 | 3794 | `		if( zCur < zIn ){` |
|      - | 3795 | `			/* Output chunk verbatim */` |
|      9 | 3796 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3797 | `		}` |
|      9 | 3798 | `		if( zIn >= zEnd ){` |
|      - | 3799 | `			/* No more input to process */` |
|      5 | 3800 | `			break;` |
|      - | 3801 | `		}` |
|      - | 3802 | `		/* Output the HTML line break */` |
|      - | 3803 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br/>' (legacy without space), otherwise use '<br>' */` |
|      5 | 3804 | `		if( is_xhtml ){` |
|      3 | 3805 | `			ph7_result_string(pCtx,"<br/>",(int)sizeof("<br/>")-1);` |
|      2 | 3806 | `		}else{` |
|      3 | 3807 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3808 | `		}` |
|      5 | 3809 | `		zCur = zIn;` |
|      - | 3810 | `		/* Append trailing line */` |
|     11 | 3811 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3812 | `			zIn++;` |
|      1 | 3813 | `		}` |
|      5 | 3814 | `		if( zCur < zIn ){` |
|      - | 3815 | `			/* Output chunk verbatim */` |
|      5 | 3816 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 3817 | `		}` |
|      1 | 3818 | `	}` |
|      5 | 3819 | `	return PH7_OK;` |
|      4 | 3820 |  |
|      - | 3821 | `/*` |
|      - | 3822 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3823 | ` *  According to the PHP reference manual.` |
|      - | 3824 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3825 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3826 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 3827 | ` * This applies to both sprintf() and printf().` |
|      - | 3828 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 3829 | ` * or more of these elements, in order:` |
|      - | 3830 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 3831 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 3832 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 3833 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 3834 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 3835 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 3836 | ` *   it with a single quote ('). See the examples below.` |
|      - | 3837 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 3838 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 3839 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 3840 | ` *   should result in.` |
|      - | 3841 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 3842 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 3843 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 3844 | ` *   limit to the string.` |
|      - | 3845 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 3846 | ` *       % - a literal percent character. No argument is required.` |
|      - | 3847 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 3848 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 3849 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 3850 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 3851 | ` * 	     for the number of digits after the decimal point.` |
|      - | 3852 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 3853 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 3854 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 3855 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 3856 | ` *       g - shorter of %e and %f.` |
|      - | 3857 | ` *       G - shorter of %E and %f.` |
|      - | 3858 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 3859 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 3860 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 3861 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 3862 | ` */` |
|      - | 3863 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3864 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 3865 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 3866 | `/*` |
|      - | 3867 | `** Conversion types fall into various categories as defined by the` |
|      - | 3868 | `** following enumeration.` |
|      - | 3869 | `*/` |
|      - | 3870 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 3871 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 3872 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 3873 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 3874 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 3875 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 3876 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 3877 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 3878 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 3879 |  |
|      - | 3880 | `/*` |
|      - | 3881 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 3882 | `*/` |
|      - | 3883 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 3884 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 3885 | `/*` |
|      - | 3886 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 3887 | `** by an instance of the following structure` |
|      - | 3888 | `*/` |
|      - | 3889 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 3890 | `struct ph7_fmt_info` |
|      - | 3891 |  |
|      - | 3892 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 3893 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 3894 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 3895 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 3896 | `  char *charset; /* The character set for conversion */` |
|      - | 3897 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 3898 | `};` |
|      - | 3899 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3900 | `/*` |
|      - | 3901 | `** "*val" is a double such that 0.1 <= *val < 10.0` |
|      - | 3902 | `** Return the ascii code for the leading digit of *val, then` |
|      - | 3903 | `** multiply "*val" by 10.0 to renormalize.` |
|      - | 3904 | `**` |
|      - | 3905 | `** Example:` |
|      - | 3906 | `**     input:     *val = 3.14159` |
|      - | 3907 | `**     output:    *val = 1.4159    function return = '3'` |
|      - | 3908 | `**` |
|      - | 3909 | `** The counter *cnt is incremented each time.  After counter exceeds` |
|      - | 3910 | `** 16 (the number of significant digits in a 64-bit float) '0' is` |
|      - | 3911 | `** always returned.` |
|      - | 3912 | `*/` |
|    404 | 3913 | `static int vxGetdigit(sxlongreal *val,int *cnt)` |
|      1 | 3914 |  |
|      - | 3915 | `  sxlongreal d;` |
|      - | 3916 | `  int digit;` |
|      - | 3917 |  |
|    405 | 3918 | `  if( (*cnt)++ >= 16 ){` |
|    ! 0 | 3919 | `	  return '0';` |
|      - | 3920 | `  }` |
|    405 | 3921 | `  digit = (int)*val;` |
|    405 | 3922 | `  d = digit;` |
|    405 | 3923 | `   *val = (*val - d)*10.0;` |
|    405 | 3924 | `  return digit + '0' ;` |
|    203 | 3925 |  |
|      - | 3926 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      - | 3927 | `/*` |
|      - | 3928 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 3929 | ` * used conversion types first.` |
|      - | 3930 | ` */` |
|      - | 3931 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 3932 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 3933 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 3934 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 3935 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 3936 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 3937 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 3938 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 3939 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 3940 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3941 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3942 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 3943 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 3944 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 3945 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 3946 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 3947 | `};` |
|      - | 3948 | `/*` |
|      - | 3949 | ` * Format a given string.` |
|      - | 3950 | ` * The root program.  All variations call this core.` |
|      - | 3951 | ` * INPUTS:` |
|      - | 3952 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 3953 | ` *            1. A pointer to the call context.` |
|      - | 3954 | ` *            2. A pointer to the list of characters to be output` |
|      - | 3955 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 3956 | ` *            3. An integer number of characters to be output.` |
|      - | 3957 | ` *               (Note: This number might be zero.)` |
|      - | 3958 | ` *            4. Upper layer private data.` |
|      - | 3959 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 3960 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 3961 | ` */` |
|    120 | 3962 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 3963 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 3964 | `	ph7_context *pCtx,  /* call context */` |
|      - | 3965 | `	const char *zIn,    /* Format string */` |
|      - | 3966 | `	int nByte,          /* Format string length */` |
|      - | 3967 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 3968 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 3969 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 3970 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 3971 | `	)` |
|      1 | 3972 |  |
|    121 | 3973 | `	char spaces[] = "                                                  ";` |
|      - | 3974 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    121 | 3975 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 3976 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 3977 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 3978 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 3979 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 3980 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 3981 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 3982 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 3983 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 3984 | `	ph7_int64 iVal;` |
|      - | 3985 | `	int precision;           /* Precision of the current field */` |
|      - | 3986 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 3987 | `	int c,rc,n;` |
|      - | 3988 | `	int length;              /* Length of the field */` |
|      - | 3989 | `	int prefix;` |
|      - | 3990 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 3991 | `	int width;               /* Width of the current field */` |
|      - | 3992 | `	int idx;` |
|    121 | 3993 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 3994 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 3995 | `	/* Start the format process */` |
|    123 | 3996 | `	for(;;){` |
|    247 | 3997 | `		zCur = zIn;` |
|    697 | 3998 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|    451 | 3999 | `			zIn++;` |
|      1 | 4000 | `		}` |
|    247 | 4001 | `		if( zCur < zIn ){` |
|      - | 4002 | `			/* Consume chunk verbatim */` |
|     95 | 4003 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|     95 | 4004 | `			if( rc == SXERR_ABORT ){` |
|      - | 4005 | `				/* Callback request an operation abort */` |
|    ! 0 | 4006 | `				break;` |
|      - | 4007 | `			}` |
|     47 | 4008 | `		}` |
|    247 | 4009 | `		if( zIn >= zEnd ){` |
|      - | 4010 | `			/* No more input to process,break immediately */` |
|    119 | 4011 | `			break;` |
|      - | 4012 | `		}` |
|      - | 4013 | `		/* Find out what flags are present */` |
|    129 | 4014 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    128 | 4015 | `			flag_alternateform = flag_zeropad = 0;` |
|    129 | 4016 | `		zIn++; /* Jump the precent sign */` |
|     64 | 4017 | `		do{` |
|    157 | 4018 | `			c = zIn[0];` |
|    157 | 4019 | `			switch( c ){` |
|      9 | 4020 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      3 | 4021 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 4022 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|      9 | 4023 | `			case '#':   flag_alternateform = 1;   c = 0;   break;` |
|      5 | 4024 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 4025 | `			case '\'':` |
|    ! 0 | 4026 | `				zIn++;` |
|    ! 0 | 4027 | `				if( zIn < zEnd ){` |
|      - | 4028 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 4029 | `					c = zIn[0];` |
|    ! 0 | 4030 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4031 | `						spaces[idx] = (char)c;` |
|    ! 0 | 4032 | `					}` |
|    ! 0 | 4033 | `					c = 0;` |
|    ! 0 | 4034 | `				}` |
|    ! 0 | 4035 | `				break;` |
|    128 | 4036 | `			default:                                       break;` |
|      - | 4037 | `			}` |
|    157 | 4038 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 4039 | `		/* Get the field width */` |
|    129 | 4040 | `		width = 0;` |
|    223 | 4041 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     31 | 4042 | `			width = width*10 + (zIn[0] - '0');` |
|     31 | 4043 | `			zIn++;` |
|      1 | 4044 | `		}` |
|    129 | 4045 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 4046 | `			/* Position specifer */` |
|    ! 0 | 4047 | `			if( width > 0 ){` |
|    ! 0 | 4048 | `				n = width;` |
|    ! 0 | 4049 | `				if( vf && n > 0 ){` |
|    ! 0 | 4050 | `					n--;` |
|    ! 0 | 4051 | `				}` |
|    ! 0 | 4052 | `			}` |
|    ! 0 | 4053 | `			zIn++;` |
|    ! 0 | 4054 | `			width = 0;` |
|    ! 0 | 4055 | `			if( zIn < zEnd && zIn[0] == '0' ){` |
|    ! 0 | 4056 | `				flag_zeropad = 1;` |
|    ! 0 | 4057 | `				zIn++;` |
|    ! 0 | 4058 | `			}` |
|    ! 0 | 4059 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 4060 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 4061 | `				zIn++;` |
|    ! 0 | 4062 | `			}` |
|    ! 0 | 4063 | `		}` |
|    129 | 4064 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 4065 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 4066 | `		}` |
|      - | 4067 | `		/* Get the precision */` |
|    129 | 4068 | `		precision = -1;` |
|    129 | 4069 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     57 | 4070 | `			precision = 0;` |
|     57 | 4071 | `			zIn++;` |
|    145 | 4072 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     61 | 4073 | `				precision = precision*10 + (zIn[0] - '0');` |
|     61 | 4074 | `				zIn++;` |
|      1 | 4075 | `			}` |
|     28 | 4076 | `		}` |
|    129 | 4077 | `		if( zIn >= zEnd ){` |
|      - | 4078 | `			/* No more input */` |
|      3 | 4079 | `			break;` |
|      - | 4080 | `		}` |
|      - | 4081 | `		/* Fetch the info entry for the field */` |
|    127 | 4082 | `		pInfo = 0;` |
|    127 | 4083 | `		xtype = PH7_FMT_ERROR;` |
|    127 | 4084 | `		c = zIn[0];` |
|    127 | 4085 | `		zIn++; /* Jump the format specifer */` |
|    699 | 4086 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|    697 | 4087 | `			if( c==aFmt[idx].fmttype ){` |
|    125 | 4088 | `				pInfo = &aFmt[idx];` |
|    125 | 4089 | `				xtype = pInfo->type;` |
|    125 | 4090 | `				break;` |
|      - | 4091 | `			}` |
|    287 | 4092 | `		}` |
|    127 | 4093 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    127 | 4094 | `		length = 0;` |
|      - | 4095 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 4096 | `		 /*` |
|      - | 4097 | `		  ** At this point, variables are initialized as follows:` |
|      - | 4098 | `		  **` |
|      - | 4099 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 4100 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 4101 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 4102 | `		  **                               field width was negative.` |
|      - | 4103 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 4104 | `		  **                               the conversion character.` |
|      - | 4105 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 4106 | `		  **   width                       The specified field width.  This is` |
|      - | 4107 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 4108 | `		  **   precision                   The specified precision.  The default` |
|      - | 4109 | `		  **                               is -1.` |
|      - | 4110 | `		  */` |
|    127 | 4111 | `		switch(xtype){` |
|    ! 0 | 4112 | `		case PH7_FMT_PERCENT:` |
|      - | 4113 | `			/* A literal percent character */` |
|    ! 0 | 4114 | `			zWorker[0] = '%';` |
|    ! 0 | 4115 | `			length = (int)sizeof(char);` |
|    ! 0 | 4116 | `			break;` |
|      3 | 4117 | `		case PH7_FMT_CHARX:` |
|      - | 4118 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 4119 | `			 * with that ASCII value` |
|      - | 4120 | `			 */` |
|      7 | 4121 | `			pArg = NEXT_ARG;` |
|      7 | 4122 | `			if( pArg == 0 ){` |
|      3 | 4123 | `				c = 0;` |
|      2 | 4124 | `			}else{` |
|      5 | 4125 | `				c = ph7_value_to_int(pArg);` |
|      - | 4126 | `			}` |
|      - | 4127 | `			/* NUL byte is an acceptable value */` |
|      7 | 4128 | `			zWorker[0] = (char)c;` |
|      7 | 4129 | `			length = (int)sizeof(char);` |
|      7 | 4130 | `			break;` |
|     12 | 4131 | `		case PH7_FMT_STRING:` |
|      - | 4132 | `			/* the argument is treated as and presented as a string */` |
|     25 | 4133 | `			pArg = NEXT_ARG;` |
|     25 | 4134 | `			if( pArg == 0 ){` |
|    ! 0 | 4135 | `				length = 0;` |
|    ! 0 | 4136 | `			}else{` |
|     25 | 4137 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 4138 | `			}` |
|     25 | 4139 | `			if( length < 1 ){` |
|    ! 0 | 4140 | `				zBuf = " ";` |
|    ! 0 | 4141 | `				length = (int)sizeof(char);` |
|    ! 0 | 4142 | `			}` |
|     25 | 4143 | `			if( precision>=0 && precision<length ){` |
|      3 | 4144 | `				length = precision;` |
|      1 | 4145 | `			}` |
|     25 | 4146 | `			if( flag_zeropad ){` |
|      - | 4147 | `				/* zero-padding works on strings too */` |
|    ! 0 | 4148 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4149 | `					spaces[idx] = '0';` |
|    ! 0 | 4150 | `				}` |
|    ! 0 | 4151 | `			}` |
|     25 | 4152 | `			break;` |
|     20 | 4153 | `		case PH7_FMT_RADIX:` |
|     41 | 4154 | `			pArg = NEXT_ARG;` |
|     41 | 4155 | `			if( pArg == 0 ){` |
|    ! 0 | 4156 | `				iVal = 0;` |
|    ! 0 | 4157 | `			}else{` |
|     41 | 4158 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 4159 | `			}` |
|      - | 4160 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|     41 | 4161 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 4162 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 4163 | `			}` |
|      - | 4164 | `#if 1` |
|      - | 4165 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 4166 | `        ** I think this is stupid.*/` |
|     41 | 4167 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 4168 | `#else` |
|      - | 4169 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 4170 | `        ** but leave the prefix for hex.*/` |
|      - | 4171 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 4172 | `#endif` |
|     41 | 4173 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|     23 | 4174 | `          if( iVal<0 ){` |
|      3 | 4175 | `            iVal = -iVal;` |
|      - | 4176 | `			/* Ticket 1433-003 */` |
|      3 | 4177 | `			if( iVal < 0 ){` |
|      - | 4178 | `				/* Overflow */` |
|    ! 0 | 4179 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4180 | `			}` |
|      3 | 4181 | `            prefix = '-';` |
|     22 | 4182 | `          }else if( flag_plussign )  prefix = '+';` |
|     19 | 4183 | `          else if( flag_blanksign )  prefix = ' ';` |
|     17 | 4184 | `          else                       prefix = 0;` |
|     12 | 4185 | `        }else{` |
|     19 | 4186 | `			if( iVal<0 ){` |
|    ! 0 | 4187 | `				iVal = -iVal;` |
|      - | 4188 | `				/* Ticket 1433-003 */` |
|    ! 0 | 4189 | `				if( iVal < 0 ){` |
|      - | 4190 | `					/* Overflow */` |
|    ! 0 | 4191 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4192 | `				}` |
|    ! 0 | 4193 | `			}` |
|     19 | 4194 | `			prefix = 0;` |
|      - | 4195 | `		}` |
|     41 | 4196 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      3 | 4197 | `          precision = width-(prefix!=0);` |
|      1 | 4198 | `        }` |
|     41 | 4199 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 4200 | `        {` |
|      - | 4201 | `          register char *cset;      /* Use registers for speed */` |
|      - | 4202 | `          register int base;` |
|     41 | 4203 | `          cset = pInfo->charset;` |
|     41 | 4204 | `          base = pInfo->base;` |
|     20 | 4205 | `          do{                                           /* Convert to ascii */` |
|     79 | 4206 | `            *(--zBuf) = cset[iVal%base];` |
|     79 | 4207 | `            iVal = iVal/base;` |
|     79 | 4208 | `          }while( iVal>0 );` |
|      - | 4209 | `        }` |
|     41 | 4210 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     55 | 4211 | `        for(idx=precision-length; idx>0; idx--){` |
|     15 | 4212 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|      8 | 4213 | `        }` |
|     41 | 4214 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|     41 | 4215 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 4216 | `          char *pre, x;` |
|      9 | 4217 | `          pre = pInfo->prefix;` |
|      9 | 4218 | `          if( *zBuf!=pre[0] ){` |
|     23 | 4219 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|      4 | 4220 | `          }` |
|      4 | 4221 | `        }` |
|     41 | 4222 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     41 | 4223 | `		break;` |
|     27 | 4224 | `		case PH7_FMT_FLOAT:` |
|      - | 4225 | `		case PH7_FMT_EXP:` |
|      - | 4226 | `		case PH7_FMT_GENERIC:{` |
|      - | 4227 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 4228 | `		long double realvalue;` |
|      - | 4229 | `		int  exp;                /* exponent of real numbers */` |
|      - | 4230 | `		double rounder;          /* Used for rounding floating point values */` |
|      - | 4231 | `		int flag_dp;            /* True if decimal point should be shown */` |
|      - | 4232 | `		int flag_rtz;           /* True if trailing zeros should be removed */` |
|      - | 4233 | `		int flag_exp;           /* True to force display of the exponent */` |
|      - | 4234 | `		int nsd;                 /* Number of significant digits returned */` |
|     55 | 4235 | `		pArg = NEXT_ARG;` |
|     55 | 4236 | `		if( pArg == 0 ){` |
|    ! 0 | 4237 | `			realvalue = 0;` |
|    ! 0 | 4238 | `		}else{` |
|     55 | 4239 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 4240 | `		}` |
|      - | 4241 | `		/* Special-case NaN and infinities since the normal formatting logic` |
|      - | 4242 | `		 * below assumes a finite positive realvalue. */` |
|     55 | 4243 | `		if( PH7_IS_NAN(realvalue) ){` |
|    ! 0 | 4244 | `			zBuf = "NAN";` |
|    ! 0 | 4245 | `			length = 3;` |
|    ! 0 | 4246 | `			break;` |
|      - | 4247 | `		}` |
|     55 | 4248 | `		if( PH7_IS_INF(realvalue) ){` |
|      - | 4249 | `			/* Infinity prints as INF or -INF depending on sign. */` |
|    ! 0 | 4250 | `			if( realvalue < 0.0 ){` |
|    ! 0 | 4251 | `				zBuf = "-INF";` |
|    ! 0 | 4252 | `				length = 4;` |
|    ! 0 | 4253 | `			}else{` |
|    ! 0 | 4254 | `				zBuf = "INF";` |
|    ! 0 | 4255 | `				length = 3;` |
|      - | 4256 | `			}` |
|    ! 0 | 4257 | `			break;` |
|      - | 4258 | `		}` |
|     55 | 4259 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|     55 | 4260 | `		if( precision>PH7_FMT_BUFSIZ-40) precision = PH7_FMT_BUFSIZ-40;` |
|     55 | 4261 | `        if( realvalue<0.0 ){` |
|    ! 0 | 4262 | `          realvalue = -realvalue;` |
|    ! 0 | 4263 | `          prefix = '-';` |
|    ! 0 | 4264 | `        }else{` |
|     55 | 4265 | `          if( flag_plussign )          prefix = '+';` |
|     55 | 4266 | `          else if( flag_blanksign )    prefix = ' ';` |
|     55 | 4267 | `          else                         prefix = 0;` |
|      - | 4268 | `        }` |
|     55 | 4269 | `        if( pInfo->type==PH7_FMT_GENERIC && precision>0 ) precision--;` |
|     55 | 4270 | `        rounder = 0.0;` |
|      - | 4271 | `#if 0` |
|      - | 4272 | `        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */` |
|      - | 4273 | `        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);` |
|      - | 4274 | `#else` |
|      - | 4275 | `        /* It makes more sense to use 0.5 */` |
|    387 | 4276 | `        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);` |
|      - | 4277 | `#endif` |
|     55 | 4278 | `        if( pInfo->type==PH7_FMT_FLOAT ) realvalue += rounder;` |
|      - | 4279 | `        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */` |
|     55 | 4280 | `        exp = 0;` |
|     55 | 4281 | `        if( realvalue>0.0 ){` |
|     59 | 4282 | `          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }` |
|     87 | 4283 | `          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }` |
|     59 | 4284 | `          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }` |
|     69 | 4285 | `          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }` |
|     55 | 4286 | `          if( exp>350 \|\| exp<-350 ){` |
|    ! 0 | 4287 | `            zBuf = "NaN";` |
|    ! 0 | 4288 | `            length = 3;` |
|    ! 0 | 4289 | `            break;` |
|      - | 4290 | `          }` |
|     27 | 4291 | `        }` |
|     55 | 4292 | `        zBuf = zWorker;` |
|      - | 4293 | `        /*` |
|      - | 4294 | `        ** If the field type is etGENERIC, then convert to either etEXP` |
|      - | 4295 | `        ** or etFLOAT, as appropriate.` |
|      - | 4296 | `        */` |
|     55 | 4297 | `        flag_exp = xtype==PH7_FMT_EXP;` |
|     55 | 4298 | `        if( xtype!=PH7_FMT_FLOAT ){` |
|    ! 0 | 4299 | `          realvalue += rounder;` |
|    ! 0 | 4300 | `          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }` |
|    ! 0 | 4301 | `        }` |
|     55 | 4302 | `        if( xtype==PH7_FMT_GENERIC ){` |
|    ! 0 | 4303 | `          flag_rtz = !flag_alternateform;` |
|    ! 0 | 4304 | `          if( exp<-4 \|\| exp>precision ){` |
|    ! 0 | 4305 | `            xtype = PH7_FMT_EXP;` |
|    ! 0 | 4306 | `          }else{` |
|    ! 0 | 4307 | `            precision = precision - exp;` |
|    ! 0 | 4308 | `            xtype = PH7_FMT_FLOAT;` |
|      - | 4309 | `          }` |
|    ! 0 | 4310 | `        }else{` |
|     55 | 4311 | `          flag_rtz = 0;` |
|      - | 4312 | `        }` |
|      - | 4313 | `        /*` |
|      - | 4314 | `        ** The "exp+precision" test causes output to be of type etEXP if` |
|      - | 4315 | `        ** the precision is too large to fit in buf[].` |
|      - | 4316 | `        */` |
|     55 | 4317 | `        nsd = 0;` |
|     55 | 4318 | `        if( xtype==PH7_FMT_FLOAT && exp+precision<PH7_FMT_BUFSIZ-30 ){` |
|     55 | 4319 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|     55 | 4320 | `          if( prefix ) *(zBuf++) = (char)prefix;         /* Sign */` |
|     55 | 4321 | `          if( exp<0 )  *(zBuf++) = '0';            /* Digits before "." */` |
|    145 | 4322 | `          else for(; exp>=0; exp--) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     55 | 4323 | `          if( flag_dp ) *(zBuf++) = '.';           /* The decimal point */` |
|     87 | 4324 | `          for(exp++; exp<0 && precision>0; precision--, exp++){` |
|     33 | 4325 | `            *(zBuf++) = '0';` |
|     17 | 4326 | `          }` |
|    355 | 4327 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     55 | 4328 | `          *(zBuf--) = 0;                           /* Null terminate */` |
|     55 | 4329 | `          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */` |
|    ! 0 | 4330 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 4331 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 4332 | `          }` |
|     55 | 4333 | `          zBuf++;                            /* point to next free slot */` |
|     28 | 4334 | `        }else{    /* etEXP or etGENERIC */` |
|    ! 0 | 4335 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|    ! 0 | 4336 | `          if( prefix ) *(zBuf++) = (char)prefix;   /* Sign */` |
|    ! 0 | 4337 | `          *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);  /* First digit */` |
|    ! 0 | 4338 | `          if( flag_dp ) *(zBuf++) = '.';     /* Decimal point */` |
|    ! 0 | 4339 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|    ! 0 | 4340 | `          zBuf--;                            /* point to last digit */` |
|    ! 0 | 4341 | `          if( flag_rtz && flag_dp ){          /* Remove tail zeros */` |
|    ! 0 | 4342 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 4343 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 4344 | `          }` |
|    ! 0 | 4345 | `          zBuf++;                            /* point to next free slot */` |
|    ! 0 | 4346 | `          if( exp \|\| flag_exp ){` |
|    ! 0 | 4347 | `            *(zBuf++) = pInfo->charset[0];` |
|    ! 0 | 4348 | `            if( exp<0 ){ *(zBuf++) = '-'; exp = -exp; } /* sign of exp */` |
|    ! 0 | 4349 | `            else       { *(zBuf++) = '+'; }` |
|    ! 0 | 4350 | `            if( exp>=100 ){` |
|    ! 0 | 4351 | `              *(zBuf++) = (char)((exp/100)+'0');                /* 100's digit */` |
|    ! 0 | 4352 | `              exp %= 100;` |
|    ! 0 | 4353 | `            }` |
|    ! 0 | 4354 | `            *(zBuf++) = (char)(exp/10+'0');                     /* 10's digit */` |
|    ! 0 | 4355 | `            *(zBuf++) = (char)(exp%10+'0');                     /* 1's digit */` |
|    ! 0 | 4356 | `          }` |
|      - | 4357 | `        }` |
|      - | 4358 | `        /* The converted number is in buf[] and zero terminated.Output it.` |
|      - | 4359 | `        ** Note that the number is in the usual order, not reversed as with` |
|      - | 4360 | `        ** integer conversions.*/` |
|     55 | 4361 | `        length = (int)(zBuf-zWorker);` |
|     55 | 4362 | `        zBuf = zWorker;` |
|      - | 4363 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 4364 | `        ** set and we are not left justified */` |
|     55 | 4365 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 4366 | `          int i;` |
|      3 | 4367 | `          int nPad = width - length;` |
|     13 | 4368 | `          for(i=width; i>=nPad; i--){` |
|     11 | 4369 | `            zBuf[i] = zBuf[i-nPad];` |
|      6 | 4370 | `          }` |
|      3 | 4371 | `          i = prefix!=0;` |
|      5 | 4372 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      3 | 4373 | `          length = width;` |
|      1 | 4374 | `        }` |
|      - | 4375 | `#else` |
|      - | 4376 | `         zBuf = " ";` |
|      - | 4377 | `		 length = (int)sizeof(char);` |
|      - | 4378 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|     55 | 4379 | `		 break;` |
|      - | 4380 | `							 }` |
|      1 | 4381 | `		default:` |
|      - | 4382 | `			/* Invalid format specifer */` |
|      3 | 4383 | `			zWorker[0] = '?';` |
|      3 | 4384 | `			length = (int)sizeof(char);` |
|      2 | 4385 | `			break;` |
|      - | 4386 | `		}` |
|      - | 4387 | `		 /*` |
|      - | 4388 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 4389 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 4390 | `		 ** the output.` |
|      - | 4391 | `		 */` |
|    127 | 4392 | `    if( !flag_leftjustify ){` |
|      - | 4393 | `      register int nspace;` |
|    119 | 4394 | `      nspace = width-length;` |
|    119 | 4395 | `      if( nspace>0 ){` |
|      5 | 4396 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4397 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4398 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4399 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4400 | `			}` |
|    ! 0 | 4401 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4402 | `        }` |
|      5 | 4403 | `        if( nspace>0 ){` |
|      5 | 4404 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      5 | 4405 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4406 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4407 | `			}` |
|      2 | 4408 | `		}` |
|      2 | 4409 | `      }` |
|     59 | 4410 | `    }` |
|    127 | 4411 | `    if( length>0 ){` |
|    127 | 4412 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    127 | 4413 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4414 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4415 | `		}` |
|     63 | 4416 | `    }` |
|    127 | 4417 | `    if( flag_leftjustify ){` |
|      - | 4418 | `      register int nspace;` |
|      9 | 4419 | `      nspace = width-length;` |
|      9 | 4420 | `      if( nspace>0 ){` |
|      9 | 4421 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4422 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4423 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4424 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4425 | `			}` |
|    ! 0 | 4426 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4427 | `        }` |
|      9 | 4428 | `        if( nspace>0 ){` |
|      9 | 4429 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      9 | 4430 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4431 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4432 | `			}` |
|      4 | 4433 | `		}` |
|      4 | 4434 | `      }` |
|      4 | 4435 | `    }` |
|      1 | 4436 | ` }/* for(;;) */` |
|    121 | 4437 | `	return SXRET_OK;` |
|     61 | 4438 |  |
|      - | 4439 | `/*` |
|      - | 4440 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 4441 | ` */` |
|     84 | 4442 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4443 |  |
|      - | 4444 | `	/* Consume directly */` |
|     85 | 4445 | `	ph7_result_string(pCtx,zInput,nLen);` |
|     42 | 4446 | `	SXUNUSED(pUserData); /* cc warning */` |
|     85 | 4447 | `	return PH7_OK;` |
|      1 | 4448 |  |
|      - | 4449 | `/*` |
|      - | 4450 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 4451 | ` *  Return a formatted string.` |
|      - | 4452 | ` * Parameters` |
|      - | 4453 | ` *  $format` |
|      - | 4454 | ` *    The format string (see block comment above)` |
|      - | 4455 | ` * Return` |
|      - | 4456 | ` *  A string produced according to the formatting string format.` |
|      - | 4457 | ` */` |
|     56 | 4458 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4459 |  |
|      - | 4460 | `	const char *zFormat;` |
|      - | 4461 | `	int nLen;` |
|     57 | 4462 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4463 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 4464 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4465 | `		return PH7_OK;` |
|      - | 4466 | `	}` |
|      - | 4467 | `	/* Extract the string format */` |
|     55 | 4468 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     55 | 4469 | `	if( nLen < 1 ){` |
|      - | 4470 | `		/* Empty string */` |
|    ! 0 | 4471 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4472 | `		return PH7_OK;` |
|      - | 4473 | `	}` |
|      - | 4474 | `	/* Format the string */` |
|     55 | 4475 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,0,FALSE);` |
|     55 | 4476 | `	return PH7_OK;` |
|     29 | 4477 |  |
|      - | 4478 | `/*` |
|      - | 4479 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 4480 | ` */` |
|    110 | 4481 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4482 |  |
|    111 | 4483 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 4484 | `	/* Call the VM output consumer directly */` |
|    111 | 4485 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 4486 | `	/* Increment counter */` |
|    111 | 4487 | `	*pCounter += nLen;` |
|    111 | 4488 | `	return PH7_OK;` |
|      1 | 4489 |  |
|      - | 4490 | `/*` |
|      - | 4491 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 4492 | ` *  Output a formatted string.` |
|      - | 4493 | ` * Parameters` |
|      - | 4494 | ` *  $format` |
|      - | 4495 | ` *   See sprintf() for a description of format.` |
|      - | 4496 | ` * Return` |
|      - | 4497 | ` *  The length of the outputted string.` |
|      - | 4498 | ` */` |
|     42 | 4499 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4500 |  |
|     43 | 4501 | `	ph7_int64 nCounter = 0;` |
|      - | 4502 | `	const char *zFormat;` |
|      - | 4503 | `	int nLen;` |
|     43 | 4504 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4505 | `		/* Missing/Invalid arguments,return 0 */` |
|      3 | 4506 | `		ph7_result_int(pCtx,0);` |
|      3 | 4507 | `		return PH7_OK;` |
|      - | 4508 | `	}` |
|      - | 4509 | `	/* Extract the string format */` |
|     41 | 4510 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 4511 | `	if( nLen < 1 ){` |
|      - | 4512 | `		/* Empty string */` |
|    ! 0 | 4513 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4514 | `		return PH7_OK;` |
|      - | 4515 | `	}` |
|      - | 4516 | `	/* Format the string */` |
|     41 | 4517 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 4518 | `	/* Return the length of the outputted string */` |
|     41 | 4519 | `	ph7_result_int64(pCtx,nCounter);` |
|     41 | 4520 | `	return PH7_OK;` |
|     22 | 4521 |  |
|      - | 4522 | `/*` |
|      - | 4523 | ` * int vprintf(string $format,array $args)` |
|      - | 4524 | ` *  Output a formatted string.` |
|      - | 4525 | ` * Parameters` |
|      - | 4526 | ` *  $format` |
|      - | 4527 | ` *   See sprintf() for a description of format.` |
|      - | 4528 | ` * Return` |
|      - | 4529 | ` *  The length of the outputted string.` |
|      - | 4530 | ` */` |
|      2 | 4531 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4532 |  |
|      3 | 4533 | `	ph7_int64 nCounter = 0;` |
|      - | 4534 | `	const char *zFormat;` |
|      - | 4535 | `	ph7_hashmap *pMap;` |
|      - | 4536 | `	SySet sArg;` |
|      - | 4537 | `	int nLen,n;` |
|      3 | 4538 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4539 | `		/* Missing/Invalid arguments,return 0 */` |
|    ! 0 | 4540 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4541 | `		return PH7_OK;` |
|      - | 4542 | `	}` |
|      - | 4543 | `	/* Extract the string format */` |
|      3 | 4544 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4545 | `	if( nLen < 1 ){` |
|      - | 4546 | `		/* Empty string */` |
|    ! 0 | 4547 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4548 | `		return PH7_OK;` |
|      - | 4549 | `	}` |
|      - | 4550 | `	/* Point to the hashmap */` |
|      3 | 4551 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4552 | `	/* Extract arguments from the hashmap */` |
|      3 | 4553 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4554 | `	/* Format the string */` |
|      3 | 4555 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 4556 | `	/* Return the length of the outputted string */` |
|      3 | 4557 | `	ph7_result_int64(pCtx,nCounter);` |
|      - | 4558 | `	/* Release the container */` |
|      3 | 4559 | `	SySetRelease(&sArg);` |
|      3 | 4560 | `	return PH7_OK;` |
|      2 | 4561 |  |
|      - | 4562 | `/*` |
|      - | 4563 | ` * int vsprintf(string $format,array $args)` |
|      - | 4564 | ` *  Output a formatted string.` |
|      - | 4565 | ` * Parameters` |
|      - | 4566 | ` *  $format` |
|      - | 4567 | ` *   See sprintf() for a description of format.` |
|      - | 4568 | ` * Return` |
|      - | 4569 | ` *  A string produced according to the formatting string format.` |
|      - | 4570 | ` */` |
|     10 | 4571 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4572 |  |
|      - | 4573 | `	const char *zFormat;` |
|      - | 4574 | `	ph7_hashmap *pMap;` |
|      - | 4575 | `	SySet sArg;` |
|      - | 4576 | `	int nLen,n;` |
|     11 | 4577 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4578 | `		/* Missing/Invalid arguments,return the empty string */` |
|      5 | 4579 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 4580 | `		return PH7_OK;` |
|      - | 4581 | `	}` |
|      - | 4582 | `	/* Extract the string format */` |
|      7 | 4583 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4584 | `	if( nLen < 1 ){` |
|      - | 4585 | `		/* Empty string */` |
|    ! 0 | 4586 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4587 | `		return PH7_OK;` |
|      - | 4588 | `	}` |
|      - | 4589 | `	/* Point to hashmap */` |
|      7 | 4590 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4591 | `	/* Extract arguments from the hashmap */` |
|      7 | 4592 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4593 | `	/* Format the string */` |
|      7 | 4594 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),0,TRUE);` |
|      - | 4595 | `	/* Release the container */` |
|      7 | 4596 | `	SySetRelease(&sArg);` |
|      7 | 4597 | `	return PH7_OK;` |
|      6 | 4598 |  |
|      - | 4599 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 4600 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4601 | `/*` |
|      - | 4602 | ` * Symisc eXtension.` |
|      - | 4603 | ` * string size_format(int64 $size)` |
|      - | 4604 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 4605 | ` *  Example:` |
|      - | 4606 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 4607 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 4608 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 4609 | ` * Parameter` |
|      - | 4610 | ` *  $size` |
|      - | 4611 | ` *    Entity size in bytes.` |
|      - | 4612 | ` * Return` |
|      - | 4613 | ` *   Formatted string representation of the given size.` |
|      - | 4614 | ` */` |
|     24 | 4615 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4616 |  |
|      - | 4617 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 4618 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 4619 | `	sxi32 nRest,i_32;` |
|      - | 4620 | `	ph7_int64 iSize;` |
|     25 | 4621 | `	int c = -1; /* index in zUnit[] */` |
|      - | 4622 |  |
|     25 | 4623 | `	if( nArg < 1 ){` |
|      - | 4624 | `		/* Missing argument,return the empty string */` |
|      3 | 4625 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4626 | `		return PH7_OK;` |
|      - | 4627 | `	}` |
|      - | 4628 | `	/* Extract the given size */` |
|     23 | 4629 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 4630 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 4631 | `		/* Don't bother formatting,return immediately */` |
|      5 | 4632 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 4633 | `		return PH7_OK;` |
|      - | 4634 | `	}` |
|     19 | 4635 | `	for(;;){` |
|     39 | 4636 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 4637 | `		iSize >>= 10;` |
|     39 | 4638 | `		c++;` |
|     39 | 4639 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 4640 | `			break;` |
|      - | 4641 | `		}` |
|      1 | 4642 | `	}` |
|     19 | 4643 | `	nRest /= 100;` |
|     19 | 4644 | `	if( nRest > 9 ){` |
|    ! 0 | 4645 | `		nRest = 9;` |
|    ! 0 | 4646 | `	}` |
|     19 | 4647 | `	if( iSize > 999 ){` |
|    ! 0 | 4648 | `		c++;` |
|    ! 0 | 4649 | `		nRest = 9;` |
|    ! 0 | 4650 | `		iSize = 0;` |
|    ! 0 | 4651 | `	}` |
|     19 | 4652 | `	i_32 = (sxi32)iSize;` |
|      - | 4653 | `	/* Format */` |
|     19 | 4654 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 4655 | `	return PH7_OK;` |
|     13 | 4656 |  |
|      - | 4657 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4658 | `/*` |
|      - | 4659 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 4660 | ` *   Calculate the md5 hash of a string.` |
|      - | 4661 | ` * Parameter` |
|      - | 4662 | ` *  $str` |
|      - | 4663 | ` *   Input string` |
|      - | 4664 | ` * $raw_output` |
|      - | 4665 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4666 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4667 | ` * Return` |
|      - | 4668 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 4669 | ` */` |
|     10 | 4670 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4671 |  |
|      - | 4672 | `	unsigned char zDigest[16];` |
|     11 | 4673 | `	int raw_output = FALSE;` |
|      - | 4674 | `	const void *pIn;` |
|      - | 4675 | `	int nLen;` |
|     11 | 4676 | `	if( nArg < 1 ){` |
|      - | 4677 | `		/* Missing arguments,return the empty string */` |
|      3 | 4678 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4679 | `		return PH7_OK;` |
|      - | 4680 | `	}` |
|      - | 4681 | `	/* Extract the input string */` |
|      9 | 4682 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 4683 | `	if( nLen < 1 ){` |
|      - | 4684 | `		/* Empty string */` |
|    ! 0 | 4685 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4686 | `		return PH7_OK;` |
|      - | 4687 | `	}` |
|      9 | 4688 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      3 | 4689 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 | 4690 | `	}` |
|      - | 4691 | `	/* Compute the MD5 digest */` |
|      9 | 4692 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|      9 | 4693 | `	if( raw_output ){` |
|      - | 4694 | `		/* Output raw digest */` |
|      3 | 4695 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      2 | 4696 | `	}else{` |
|      - | 4697 | `		/* Perform a binary to hex conversion */` |
|      7 | 4698 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4699 | `	}` |
|      9 | 4700 | `	return PH7_OK;` |
|      6 | 4701 |  |
|      - | 4702 | `/*` |
|      - | 4703 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 4704 | ` *   Calculate the sha1 hash of a string.` |
|      - | 4705 | ` * Parameter` |
|      - | 4706 | ` *  $str` |
|      - | 4707 | ` *   Input string` |
|      - | 4708 | ` * $raw_output` |
|      - | 4709 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4710 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4711 | ` * Return` |
|      - | 4712 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 4713 | ` */` |
|      8 | 4714 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4715 |  |
|      - | 4716 | `	unsigned char zDigest[20];` |
|      9 | 4717 | `	int raw_output = FALSE;` |
|      - | 4718 | `	const void *pIn;` |
|      - | 4719 | `	int nLen;` |
|      9 | 4720 | `	if( nArg < 1 ){` |
|      - | 4721 | `		/* Missing arguments,return the empty string */` |
|      3 | 4722 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4723 | `		return PH7_OK;` |
|      - | 4724 | `	}` |
|      - | 4725 | `	/* Extract the input string */` |
|      7 | 4726 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4727 | `	if( nLen < 1 ){` |
|      - | 4728 | `		/* Empty string */` |
|    ! 0 | 4729 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4730 | `		return PH7_OK;` |
|      - | 4731 | `	}` |
|      7 | 4732 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      3 | 4733 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 | 4734 | `	}` |
|      - | 4735 | `	/* Compute the SHA1 digest */` |
|      7 | 4736 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|      7 | 4737 | `	if( raw_output ){` |
|      - | 4738 | `		/* Output raw digest */` |
|      3 | 4739 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      2 | 4740 | `	}else{` |
|      - | 4741 | `		/* Perform a binary to hex conversion */` |
|      5 | 4742 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4743 | `	}` |
|      7 | 4744 | `	return PH7_OK;` |
|      5 | 4745 |  |
|      - | 4746 | `/*` |
|      - | 4747 | ` * int64 crc32(string $str)` |
|      - | 4748 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 4749 | ` * Parameter` |
|      - | 4750 | ` *  $str` |
|      - | 4751 | ` *   Input string` |
|      - | 4752 | ` * Return` |
|      - | 4753 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 4754 | ` */` |
|      4 | 4755 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4756 |  |
|      - | 4757 | `	const void *pIn;` |
|      - | 4758 | `	sxu32 nCRC;` |
|      - | 4759 | `	int nLen;` |
|      5 | 4760 | `	if( nArg < 1 ){` |
|      - | 4761 | `		/* Missing arguments,return 0 */` |
|      3 | 4762 | `		ph7_result_int(pCtx,0);` |
|      3 | 4763 | `		return PH7_OK;` |
|      - | 4764 | `	}` |
|      - | 4765 | `	/* Extract the input string */` |
|      3 | 4766 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4767 | `	if( nLen < 1 ){` |
|      - | 4768 | `		/* Empty string */` |
|    ! 0 | 4769 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4770 | `		return PH7_OK;` |
|      - | 4771 | `	}` |
|      - | 4772 | `	/* Calculate the sum */` |
|      3 | 4773 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 4774 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 4775 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 4776 | `	return PH7_OK;` |
|      3 | 4777 |  |
|      - | 4778 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 4779 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4780 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 4781 | `/*` |
|      - | 4782 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 4783 |  |
|      - | 4784 | ` */` |
|      4 | 4785 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 4786 | `	const char *zInput, /* Raw input */` |
|      - | 4787 | `	int nByte,  /* Input length */` |
|      - | 4788 | `	int delim,  /* Delimiter */` |
|      - | 4789 | `	int encl,   /* Enclosure */` |
|      - | 4790 | `	int escape,  /* Escape character */` |
|      - | 4791 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 4792 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 4793 | `	)` |
|      1 | 4794 |  |
|      5 | 4795 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 4796 | `	const char *zIn = zInput;` |
|      - | 4797 | `	const char *zPtr;` |
|      - | 4798 | `	int isEnc;` |
|      - | 4799 | `	/* Start processing */` |
|      8 | 4800 | `	for(;;){` |
|     17 | 4801 | `		if( zIn >= zEnd ){` |
|      - | 4802 | `			/* No more input to process */` |
|      5 | 4803 | `			break;` |
|      - | 4804 | `		}` |
|     13 | 4805 | `		isEnc = 0;` |
|     13 | 4806 | `		zPtr = zIn;` |
|      - | 4807 | `		/* Find the first delimiter */` |
|     27 | 4808 | `		while( zIn < zEnd ){` |
|     23 | 4809 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 4810 | `				/* Delimiter found,break imediately */` |
|      5 | 4811 | `				break;` |
|     15 | 4812 | `			}else if( zIn[0] == encl ){` |
|      - | 4813 | `				/* Inside enclosure? */` |
|    ! 0 | 4814 | `				isEnc = !isEnc;` |
|     15 | 4815 | `			}else if( zIn[0] == escape ){` |
|      - | 4816 | `				/* Escape sequence */` |
|    ! 0 | 4817 | `				zIn++;` |
|    ! 0 | 4818 | `			}` |
|      - | 4819 | `			/* Advance the cursor */` |
|     15 | 4820 | `			zIn++;` |
|      1 | 4821 | `		}` |
|     13 | 4822 | `		if( zIn > zPtr ){` |
|     13 | 4823 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 4824 | `			sxi32 rc;` |
|      - | 4825 | `			/* Invoke the supllied callback */` |
|     13 | 4826 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 4827 | `				zPtr++;` |
|    ! 0 | 4828 | `				nByteChunk-=2;` |
|    ! 0 | 4829 | `			}` |
|     13 | 4830 | `			if( nByteChunk > 0 ){` |
|     13 | 4831 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 4832 | `				if( rc == SXERR_ABORT ){` |
|      - | 4833 | `					/* User callback request an operation abort */` |
|    ! 0 | 4834 | `					break;` |
|      - | 4835 | `				}` |
|      6 | 4836 | `			}` |
|      6 | 4837 | `		}` |
|      - | 4838 | `		/* Ignore trailing delimiter */` |
|     21 | 4839 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 4840 | `			zIn++;` |
|      1 | 4841 | `		}` |
|      1 | 4842 | `	}` |
|      5 | 4843 | `	return SXRET_OK;` |
|      1 | 4844 |  |
|      - | 4845 | `/*` |
|      - | 4846 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 4847 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 4848 | ` * argument to this callback.` |
|      - | 4849 | ` */` |
|     12 | 4850 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 4851 |  |
|     13 | 4852 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 4853 | `	ph7_value sEntry;` |
|      - | 4854 | `	SyString sToken;` |
|      - | 4855 | `	/* Insert the token in the given array */` |
|     13 | 4856 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 4857 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 4858 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 4859 | `	if( sToken.nByte < 1){` |
|    ! 0 | 4860 | `		return SXRET_OK;` |
|      - | 4861 | `	}` |
|     13 | 4862 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 4863 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 4864 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 4865 | `	return SXRET_OK;` |
|      7 | 4866 |  |
|      - | 4867 | `/*` |
|      - | 4868 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 4869 | ` *  Parse a CSV string into an array.` |
|      - | 4870 | ` * Parameters` |
|      - | 4871 | ` *  $input` |
|      - | 4872 | ` *   The string to parse.` |
|      - | 4873 | ` *  $delimiter` |
|      - | 4874 | ` *   Set the field delimiter (one character only).` |
|      - | 4875 | ` *  $enclosure` |
|      - | 4876 | ` *   Set the field enclosure character (one character only).` |
|      - | 4877 | ` *  $escape` |
|      - | 4878 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 4879 | ` * Return` |
|      - | 4880 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 4881 | ` */` |
|      4 | 4882 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4883 |  |
|      - | 4884 | `	const char *zInput,*zPtr;` |
|      - | 4885 | `	ph7_value *pArray;` |
|      5 | 4886 | `	int delim  = ',';   /* Delimiter */` |
|      5 | 4887 | `	int encl   = '"' ;  /* Enclosure */` |
|      5 | 4888 | `	int escape = '\\';  /* Escape character */` |
|      - | 4889 | `	int nLen;` |
|      5 | 4890 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4891 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 4892 | `		ph7_result_null(pCtx);` |
|      3 | 4893 | `		return PH7_OK;` |
|      - | 4894 | `	}` |
|      - | 4895 | `	/* Extract the raw input */` |
|      3 | 4896 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4897 | `	if( nArg > 1 ){` |
|      - | 4898 | `		int i;` |
|      3 | 4899 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 4900 | `			/* Extract the delimiter */` |
|      3 | 4901 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 4902 | `			if( i > 0 ){` |
|      3 | 4903 | `				delim = zPtr[0];` |
|      1 | 4904 | `			}` |
|      1 | 4905 | `		}` |
|      3 | 4906 | `		if( nArg > 2 ){` |
|      3 | 4907 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 4908 | `				/* Extract the enclosure */` |
|      3 | 4909 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 4910 | `				if( i > 0 ){` |
|      3 | 4911 | `					encl = zPtr[0];` |
|      1 | 4912 | `				}` |
|      1 | 4913 | `			}` |
|      3 | 4914 | `			if( nArg > 3 ){` |
|      3 | 4915 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 4916 | `					/* Extract the escape character */` |
|      3 | 4917 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 4918 | `					if( i > 0 ){` |
|      3 | 4919 | `						escape = zPtr[0];` |
|      1 | 4920 | `					}` |
|      1 | 4921 | `				}` |
|      1 | 4922 | `			}` |
|      1 | 4923 | `		}` |
|      1 | 4924 | `	}` |
|      - | 4925 | `	/* Create our array */` |
|      3 | 4926 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4927 | `	if( pArray == 0 ){` |
|    ! 0 | 4928 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4929 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4930 | `		return PH7_OK;` |
|      - | 4931 | `	}` |
|      - | 4932 | `	/* Parse the raw input */` |
|      3 | 4933 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 4934 | `	/* Return the freshly created array */` |
|      3 | 4935 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4936 | `	return PH7_OK;` |
|      3 | 4937 |  |
|      - | 4938 | `/*` |
|      - | 4939 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 4940 | ` * container.` |
|      - | 4941 | ` * Refer to [strip_tags()].` |
|      - | 4942 | ` */` |
|     10 | 4943 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 4944 |  |
|     11 | 4945 | `	const char *zEnd = &zTag[nByte];` |
|      - | 4946 | `	const char *zPtr;` |
|      - | 4947 | `	SyString sEntry;` |
|      - | 4948 | `	/* Strip tags */` |
|     10 | 4949 | `	for(;;){` |
|     45 | 4950 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 4951 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 4952 | `				zTag++;` |
|      1 | 4953 | `		}` |
|     21 | 4954 | `		if( zTag >= zEnd ){` |
|     11 | 4955 | `			break;` |
|      - | 4956 | `		}` |
|     11 | 4957 | `		zPtr = zTag;` |
|      - | 4958 | `		/* Delimit the tag */` |
|     25 | 4959 | `		while(zTag < zEnd ){` |
|     25 | 4960 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 4961 | `				/* UTF-8 stream */` |
|      3 | 4962 | `				zTag++;` |
|      5 | 4963 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 4964 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 4965 | `				break;` |
|    ! 0 | 4966 | `			}else{` |
|     13 | 4967 | `				zTag++;` |
|      - | 4968 | `			}` |
|      1 | 4969 | `		}` |
|     11 | 4970 | `		if( zTag > zPtr ){` |
|      - | 4971 | `			/* Perform the insertion */` |
|     11 | 4972 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 4973 | `			SyStringFullTrim(&sEntry);` |
|     11 | 4974 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 4975 | `		}` |
|      - | 4976 | `		/* Jump the trailing '>' */` |
|     11 | 4977 | `		zTag++;` |
|      1 | 4978 | `	}` |
|     11 | 4979 | `	return SXRET_OK;` |
|      1 | 4980 |  |
|      - | 4981 | `/*` |
|      - | 4982 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 4983 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 4984 | ` * Refer to [strip_tags()].` |
|      - | 4985 | ` */` |
|     36 | 4986 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 4987 |  |
|     37 | 4988 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 4989 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 4990 | `		SyString sTag;` |
|     85 | 4991 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 4992 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 4993 | `			zTag++;` |
|      1 | 4994 | `		}` |
|      - | 4995 | `		/* Delimit the tag */` |
|     25 | 4996 | `		zCur = zTag;` |
|     77 | 4997 | `		while(zTag < zEnd ){` |
|     77 | 4998 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 4999 | `				/* UTF-8 stream */` |
|      5 | 5000 | `				zTag++;` |
|      9 | 5001 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 5002 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 5003 | `				break;` |
|    ! 0 | 5004 | `			}else{` |
|     49 | 5005 | `				zTag++;` |
|      - | 5006 | `			}` |
|      1 | 5007 | `		}` |
|     25 | 5008 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 5009 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 5010 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 5011 | `		if( sTag.nByte > 0 ){` |
|      - | 5012 | `			SyString *aEntry,*pEntry;` |
|      - | 5013 | `			sxi32 rc;` |
|      - | 5014 | `			sxu32 n;` |
|      - | 5015 | `			/* Perform the lookup */` |
|     25 | 5016 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 5017 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 5018 | `				pEntry = &aEntry[n];` |
|      - | 5019 | `				/* Do the comparison */` |
|     25 | 5020 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 5021 | `				if( !rc ){` |
|     21 | 5022 | `					return SXRET_OK;` |
|      - | 5023 | `				}` |
|      3 | 5024 | `			}` |
|      2 | 5025 | `		}` |
|      2 | 5026 | `	}` |
|      - | 5027 | `	/* No such tag */` |
|     17 | 5028 | `	return SXERR_NOTFOUND;` |
|     19 | 5029 |  |
|      - | 5030 | `/*` |
|      - | 5031 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 5032 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 5033 | ` * Refer to [strip_tags()].` |
|      - | 5034 | ` */` |
|     16 | 5035 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 5036 |  |
|     17 | 5037 | `	const char *zEnd = &zIn[nByte];` |
|      - | 5038 | `	const char *zPtr,*zTag;` |
|      - | 5039 | `	SySet sSet;` |
|      - | 5040 | `	/* initialize the set of allowed tags */` |
|     17 | 5041 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 5042 | `	if( nTaglen > 0 ){` |
|      - | 5043 | `		/* Set of allowed tags */` |
|     11 | 5044 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 5045 | `	}` |
|      - | 5046 | `	/* Set the empty string */` |
|     17 | 5047 | `	ph7_result_string(pCtx,"",0);` |
|      - | 5048 | `	/* Start processing */` |
|     26 | 5049 | `	for(;;){` |
|     53 | 5050 | `		if(zIn >= zEnd){` |
|      - | 5051 | `			/* No more input to process */` |
|     15 | 5052 | `			break;` |
|      - | 5053 | `		}` |
|     39 | 5054 | `		zPtr = zIn;` |
|      - | 5055 | `		/* Find a tag */` |
|    133 | 5056 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 5057 | `			zIn++;` |
|      1 | 5058 | `		}` |
|     39 | 5059 | `		if( zIn > zPtr ){` |
|      - | 5060 | `			/* Consume raw input */` |
|     21 | 5061 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 5062 | `		}` |
|      - | 5063 | `		/* Ignore trailing null bytes */` |
|     39 | 5064 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 5065 | `			zIn++;` |
|    ! 0 | 5066 | `		}` |
|     39 | 5067 | `		if(zIn >= zEnd){` |
|      - | 5068 | `			/* No more input to process */` |
|      3 | 5069 | `			break;` |
|      - | 5070 | `		}` |
|     37 | 5071 | `		if( zIn[0] == '<' ){` |
|      - | 5072 | `			sxi32 rc;` |
|     37 | 5073 | `			zTag = zIn++;` |
|      - | 5074 | `			/* Delimit the tag */` |
|    127 | 5075 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 5076 | `				zIn++;` |
|      1 | 5077 | `			}` |
|     37 | 5078 | `			if( zIn < zEnd ){` |
|     37 | 5079 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 5080 | `			}` |
|      - | 5081 | `			/* Query the set */` |
|     37 | 5082 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 5083 | `			if( rc == SXRET_OK ){` |
|      - | 5084 | `				/* Keep the tag */` |
|     21 | 5085 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 5086 | `			}` |
|     18 | 5087 | `		}` |
|      1 | 5088 | `	}` |
|      - | 5089 | `	/* Cleanup */` |
|     17 | 5090 | `	SySetRelease(&sSet);` |
|     17 | 5091 | `	return SXRET_OK;` |
|      1 | 5092 |  |
|      - | 5093 | `/*` |
|      - | 5094 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 5095 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 5096 | ` * Parameters` |
|      - | 5097 | ` *  $str` |
|      - | 5098 | ` *  The input string.` |
|      - | 5099 | ` * $allowable_tags` |
|      - | 5100 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 5101 | ` * Return` |
|      - | 5102 | ` *  Returns the stripped string.` |
|      - | 5103 | ` */` |
|     16 | 5104 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5105 |  |
|     17 | 5106 | `	const char *zTaglist = 0;` |
|      - | 5107 | `	const char *zString;` |
|     17 | 5108 | `	int nTaglen = 0;` |
|      - | 5109 | `	int nLen;` |
|     17 | 5110 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5111 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 5112 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5113 | `		return PH7_OK;` |
|      - | 5114 | `	}` |
|      - | 5115 | `	/* Point to the raw string */` |
|     15 | 5116 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 5117 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 5118 | `		/* Allowed tag */` |
|     11 | 5119 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 5120 | `	}` |
|      - | 5121 | `	/* Process input */` |
|     15 | 5122 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 5123 | `	return PH7_OK;` |
|      9 | 5124 |  |
|      - | 5125 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5126 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5127 | `/*` |
|      - | 5128 | ` * string str_shuffle(string $str)` |
|      - | 5129 |  |
|      - | 5130 | ` *  Randomly shuffles a string.` |
|      - | 5131 | ` * Parameters` |
|      - | 5132 | ` *  $str` |
|      - | 5133 | ` *   The input string.` |
|      - | 5134 | ` * Return` |
|      - | 5135 | ` *  Returns the shuffled string.` |
|      - | 5136 | ` */` |
|     12 | 5137 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5138 |  |
|      - | 5139 | `	const char *zString;` |
|      - | 5140 | `	int nLen,i,c;` |
|      - | 5141 | `	sxu32 iR;` |
|     13 | 5142 | `	if( nArg < 1 ){` |
|      - | 5143 | `		/* Missing arguments,return the empty string */` |
|      3 | 5144 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5145 | `		return PH7_OK;` |
|      - | 5146 | `	}` |
|      - | 5147 | `	/* Extract the target string */` |
|     11 | 5148 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5149 | `	if( nLen < 1 ){` |
|      - | 5150 | `		/* Nothing to shuffle */` |
|      3 | 5151 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5152 | `		return PH7_OK;` |
|      - | 5153 | `	}` |
|      - | 5154 | `	/* Shuffle the string */` |
|     43 | 5155 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 5156 | `		/* Generate a random number first */` |
|     35 | 5157 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 5158 | `		/* Extract a random offset */` |
|     35 | 5159 | `		c = zString[iR % nLen];` |
|      - | 5160 | `		/* Append it */` |
|     35 | 5161 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 5162 | `	}` |
|      9 | 5163 | `	return PH7_OK;` |
|      7 | 5164 |  |
|      - | 5165 | `/*` |
|      - | 5166 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 5167 | ` *  Convert a string to an array.` |
|      - | 5168 | ` * Parameters` |
|      - | 5169 | ` * $str` |
|      - | 5170 | ` *  The input string.` |
|      - | 5171 | ` * $split_length` |
|      - | 5172 | ` *  Maximum length of the chunk.` |
|      - | 5173 | ` * Return` |
|      - | 5174 | ` *  If the optional split_length parameter is specified, the returned array` |
|      - | 5175 | ` *  will be broken down into chunks with each being split_length in length, otherwise` |
|      - | 5176 | ` *  each chunk will be one character in length. FALSE is returned if split_length is less than 1.` |
|      - | 5177 | ` *  If the split_length length exceeds the length of string, the entire string is returned` |
|      - | 5178 | ` *  as the first (and only) array element.` |
|      - | 5179 | ` */` |
|      8 | 5180 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5181 |  |
|      - | 5182 | `	const char *zString,*zEnd;` |
|      - | 5183 | `	ph7_value *pArray,*pValue;` |
|      - | 5184 | `	int split_len;` |
|      - | 5185 | `	int nLen;` |
|      9 | 5186 | `	if( nArg < 1 ){` |
|      - | 5187 | `		/* Missing arguments,return FALSE */` |
|      5 | 5188 | `		ph7_result_bool(pCtx,0);` |
|      5 | 5189 | `		return PH7_OK;` |
|      - | 5190 | `	}` |
|      - | 5191 | `	/* Point to the target string */` |
|      5 | 5192 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 5193 | `	if( nLen < 1 ){` |
|      - | 5194 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 5195 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5196 | `		return PH7_OK;` |
|      - | 5197 | `	}` |
|      5 | 5198 | `	split_len = (int)sizeof(char);` |
|      5 | 5199 | `	if( nArg > 1 ){` |
|      - | 5200 | `		/* Split length */` |
|      5 | 5201 | `		split_len = ph7_value_to_int(apArg[1]);` |
|      5 | 5202 | `		if( split_len < 1 ){` |
|      - | 5203 | `			/* Invalid length,return FALSE */` |
|      3 | 5204 | `			ph7_result_bool(pCtx,0);` |
|      3 | 5205 | `			return PH7_OK;` |
|      - | 5206 | `		}` |
|      3 | 5207 | `		if( split_len > nLen ){` |
|    ! 0 | 5208 | `			split_len = nLen;` |
|    ! 0 | 5209 | `		}` |
|      1 | 5210 | `	}` |
|      - | 5211 | `	/* Create the array and the scalar value */` |
|      3 | 5212 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 5213 | `	/*Chunk value */` |
|      3 | 5214 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 5215 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 5216 | `		/* Return FALSE */` |
|    ! 0 | 5217 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5218 | `		return PH7_OK;` |
|      - | 5219 | `	}` |
|      - | 5220 | `	/* Point to the end of the string */` |
|      3 | 5221 | `	zEnd = &zString[nLen];` |
|      - | 5222 | `	/* Perform the requested operation */` |
|      7 | 5223 | `	for(;;){` |
|      - | 5224 | `		int nMax;` |
|      9 | 5225 | `		if( zString >= zEnd ){` |
|      - | 5226 | `			/* No more input to process */` |
|      3 | 5227 | `			break;` |
|      - | 5228 | `		}` |
|      7 | 5229 | `		nMax = (int)(zEnd-zString);` |
|      7 | 5230 | `		if( nMax < split_len ){` |
|    ! 0 | 5231 | `			split_len = nMax;` |
|    ! 0 | 5232 | `		}` |
|      - | 5233 | `		/* Copy the current chunk */` |
|      7 | 5234 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 5235 | `		/* Insert it */` |
|      7 | 5236 | `		ph7_array_add_elem(pArray,0,pValue); /* Will make it's own copy */` |
|      - | 5237 | `		/* reset the string cursor */` |
|      7 | 5238 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 5239 | `		/* Update position */` |
|      7 | 5240 | `		zString += split_len;` |
|      1 | 5241 | `	}` |
|      - | 5242 | `	/*` |
|      - | 5243 | `	 * Return the array.` |
|      - | 5244 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 5245 | `	 * upon we return from this function.` |
|      - | 5246 | `	 */` |
|      3 | 5247 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5248 | `	return PH7_OK;` |
|      5 | 5249 |  |
|      - | 5250 | `/*` |
|      - | 5251 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 5252 | ` * Refer to [strspn()].` |
|      - | 5253 | ` */` |
|     28 | 5254 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 5255 |  |
|     29 | 5256 | `	const char *zIn = *pzIn;` |
|      - | 5257 | `	const char *zPtr;` |
|      - | 5258 | `	/* Ignore leading white spaces */` |
|     29 | 5259 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 5260 | `		zIn++;` |
|    ! 0 | 5261 | `	}` |
|     29 | 5262 | `	if( zIn >= zEnd ){` |
|      - | 5263 | `		/* End of input */` |
|    ! 0 | 5264 | `		return SXERR_EOF;` |
|      - | 5265 | `	}` |
|     29 | 5266 | `	zPtr = zIn;` |
|      - | 5267 | `	/* Extract the token */` |
|    201 | 5268 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 5269 | `		zIn++;` |
|      1 | 5270 | `	}` |
|     29 | 5271 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5272 | `	/* Synchronize pointers */` |
|     29 | 5273 | `	*pzIn = zIn;` |
|      - | 5274 | `	/* Return to the caller */` |
|     29 | 5275 | `	return SXRET_OK;` |
|     15 | 5276 |  |
|      - | 5277 | `/*` |
|      - | 5278 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 5279 | ` * return the longest match.` |
|      - | 5280 | ` * Refer to [strspn()].` |
|      - | 5281 | ` */` |
|     18 | 5282 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5283 |  |
|     19 | 5284 | `	const char *zEnd = &zString[nLen];` |
|     19 | 5285 | `	const char *zIn = zString;` |
|      - | 5286 | `	int i,c;` |
|     45 | 5287 | `	for(;;){` |
|     91 | 5288 | `		if( zString >= zEnd ){` |
|      7 | 5289 | `			break;` |
|      - | 5290 | `		}` |
|      - | 5291 | `		/* Extract current character */` |
|     85 | 5292 | `		c = zString[0];` |
|      - | 5293 | `		/* Perform the lookup */` |
|    383 | 5294 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 5295 | `			if( c == zMask[i] ){` |
|      - | 5296 | `				/* Character found */` |
|     73 | 5297 | `				break;` |
|      - | 5298 | `			}` |
|    150 | 5299 | `		}` |
|     85 | 5300 | `		if( i >= nMaskLen ){` |
|      - | 5301 | `			/* Character not in the current mask,break immediately */` |
|     13 | 5302 | `			break;` |
|      - | 5303 | `		}` |
|      - | 5304 | `		/* Advance cursor */` |
|     73 | 5305 | `		zString++;` |
|      1 | 5306 | `	}` |
|      - | 5307 | `	/* Longest match */` |
|     19 | 5308 | `	return (int)(zString-zIn);` |
|      1 | 5309 |  |
|      - | 5310 | `/*` |
|      - | 5311 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 5312 | ` * Refer to [strcspn()].` |
|      - | 5313 | ` */` |
|     10 | 5314 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5315 |  |
|     11 | 5316 | `	const char *zEnd = &zString[nLen];` |
|     11 | 5317 | `	const char *zIn = zString;` |
|      - | 5318 | `	int i,c;` |
|     12 | 5319 | `	for(;;){` |
|     25 | 5320 | `		if( zString >= zEnd ){` |
|      3 | 5321 | `			break;` |
|      - | 5322 | `		}` |
|      - | 5323 | `		/* Extract current character */` |
|     23 | 5324 | `		c = zString[0];` |
|      - | 5325 | `		/* Perform the lookup */` |
|     51 | 5326 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 5327 | `			if( c == zMask[i] ){` |
|      9 | 5328 | `				break;` |
|      - | 5329 | `			}` |
|     15 | 5330 | `		}` |
|     23 | 5331 | `		if( i < nMaskLen ){` |
|      - | 5332 | `			/* Character in the current mask,break immediately */` |
|      9 | 5333 | `			break;` |
|      - | 5334 | `		}` |
|      - | 5335 | `		/* Advance cursor */` |
|     15 | 5336 | `		zString++;` |
|      1 | 5337 | `	}` |
|      - | 5338 | `	/* Longest match */` |
|     11 | 5339 | `	return (int)(zString-zIn);` |
|      1 | 5340 |  |
|      - | 5341 | `/*` |
|      - | 5342 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5343 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 5344 | ` *  of characters contained within a given mask.` |
|      - | 5345 | ` * Parameters` |
|      - | 5346 | ` * $str` |
|      - | 5347 | ` *  The input string.` |
|      - | 5348 | ` * $mask` |
|      - | 5349 | ` *  The list of allowable characters.` |
|      - | 5350 | ` * $start` |
|      - | 5351 | ` *  The position in subject to start searching.` |
|      - | 5352 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5353 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5354 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5355 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5356 | ` *  start'th position from the end of subject.` |
|      - | 5357 | ` * $length` |
|      - | 5358 | ` *  The length of the segment from subject to examine.` |
|      - | 5359 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5360 | ` *  characters after the starting position.` |
|      - | 5361 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5362 | ` *  position up to length characters from the end of subject.` |
|      - | 5363 | ` * Return` |
|      - | 5364 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 5365 | ` * in mask.` |
|      - | 5366 | ` */` |
|     26 | 5367 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5368 |  |
|      - | 5369 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5370 | `	int iMasklen,iLen;` |
|      - | 5371 | `	SyString sToken;` |
|     27 | 5372 | `	int iCount = 0;` |
|      - | 5373 | `	int rc;` |
|     27 | 5374 | `	if( nArg < 2 ){` |
|      - | 5375 | `		/* Missing agruments,return zero */` |
|      3 | 5376 | `		ph7_result_int(pCtx,0);` |
|      3 | 5377 | `		return PH7_OK;` |
|      - | 5378 | `	}` |
|      - | 5379 | `	/* Extract the target string */` |
|     25 | 5380 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5381 | `	/* Extract the mask */` |
|     25 | 5382 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 5383 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 5384 | `		/* Nothing to process,return zero */` |
|      7 | 5385 | `		ph7_result_int(pCtx,0);` |
|      7 | 5386 | `		return PH7_OK;` |
|      - | 5387 | `	}` |
|     19 | 5388 | `	if( nArg > 2 ){` |
|      - | 5389 | `		int nOfft;` |
|      - | 5390 | `		/* Extract the offset */` |
|      9 | 5391 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 5392 | `		if( nOfft < 0 ){` |
|    ! 0 | 5393 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5394 | `			if( zBase > zString ){` |
|    ! 0 | 5395 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5396 | `				zString = zBase;` |
|    ! 0 | 5397 | `			}else{` |
|      - | 5398 | `				/* Invalid offset */` |
|    ! 0 | 5399 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5400 | `				return PH7_OK;` |
|      - | 5401 | `			}` |
|    ! 0 | 5402 | `		}else{` |
|      9 | 5403 | `			if( nOfft >= iLen ){` |
|      - | 5404 | `				/* Invalid offset */` |
|    ! 0 | 5405 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5406 | `				return PH7_OK;` |
|    ! 0 | 5407 | `			}else{` |
|      - | 5408 | `				/* Update offset */` |
|      9 | 5409 | `				zString += nOfft;` |
|      9 | 5410 | `				iLen -= nOfft;` |
|      - | 5411 | `			}` |
|      - | 5412 | `		}` |
|      9 | 5413 | `		if( nArg > 3 ){` |
|      - | 5414 | `			int iUserlen;` |
|      - | 5415 | `			/* Extract the desired length */` |
|      9 | 5416 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 5417 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 5418 | `				iLen = iUserlen;` |
|      2 | 5419 | `			}` |
|      4 | 5420 | `		}` |
|      4 | 5421 | `	}` |
|      - | 5422 | `	/* Point to the end of the string */` |
|     19 | 5423 | `	zEnd = &zString[iLen];` |
|      - | 5424 | `	/* Extract the first non-space token */` |
|     19 | 5425 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 5426 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5427 | `		/* Compare against the current mask */` |
|     19 | 5428 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 5429 | `	}` |
|      - | 5430 | `	/* Longest match */` |
|     19 | 5431 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 5432 | `	return PH7_OK;` |
|     14 | 5433 |  |
|      - | 5434 | `/*` |
|      - | 5435 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5436 | ` *  Find length of initial segment not matching mask.` |
|      - | 5437 | ` * Parameters` |
|      - | 5438 | ` * $str` |
|      - | 5439 | ` *  The input string.` |
|      - | 5440 | ` * $mask` |
|      - | 5441 | ` *  The list of not allowed characters.` |
|      - | 5442 | ` * $start` |
|      - | 5443 | ` *  The position in subject to start searching.` |
|      - | 5444 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5445 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5446 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5447 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5448 | ` *  start'th position from the end of subject.` |
|      - | 5449 | ` * $length` |
|      - | 5450 | ` *  The length of the segment from subject to examine.` |
|      - | 5451 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5452 | ` *  characters after the starting position.` |
|      - | 5453 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5454 | ` *  position up to length characters from the end of subject.` |
|      - | 5455 | ` * Return` |
|      - | 5456 | ` *  Returns the length of the segment as an integer.` |
|      - | 5457 | ` */` |
|     16 | 5458 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5459 |  |
|      - | 5460 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5461 | `	int iMasklen,iLen;` |
|      - | 5462 | `	SyString sToken;` |
|     17 | 5463 | `	int iCount = 0;` |
|      - | 5464 | `	int rc;` |
|     17 | 5465 | `	if( nArg < 2 ){` |
|      - | 5466 | `		/* Missing agruments,return zero */` |
|      3 | 5467 | `		ph7_result_int(pCtx,0);` |
|      3 | 5468 | `		return PH7_OK;` |
|      - | 5469 | `	}` |
|      - | 5470 | `	/* Extract the target string */` |
|     15 | 5471 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5472 | `	/* Extract the mask */` |
|     15 | 5473 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 5474 | `	if( iLen < 1 ){` |
|      - | 5475 | `		/* Nothing to process,return zero */` |
|    ! 0 | 5476 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5477 | `		return PH7_OK;` |
|      - | 5478 | `	}` |
|     15 | 5479 | `	if( iMasklen < 1 ){` |
|      - | 5480 | `		/* No given mask,return the string length */` |
|      3 | 5481 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 5482 | `		return PH7_OK;` |
|      - | 5483 | `	}` |
|     13 | 5484 | `	if( nArg > 2 ){` |
|      - | 5485 | `		int nOfft;` |
|      - | 5486 | `		/* Extract the offset */` |
|     11 | 5487 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 5488 | `		if( nOfft < 0 ){` |
|    ! 0 | 5489 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5490 | `			if( zBase > zString ){` |
|    ! 0 | 5491 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5492 | `				zString = zBase;` |
|    ! 0 | 5493 | `			}else{` |
|      - | 5494 | `				/* Invalid offset */` |
|    ! 0 | 5495 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5496 | `				return PH7_OK;` |
|      - | 5497 | `			}` |
|    ! 0 | 5498 | `		}else{` |
|     11 | 5499 | `			if( nOfft >= iLen ){` |
|      - | 5500 | `				/* Invalid offset */` |
|      3 | 5501 | `				ph7_result_int(pCtx,0);` |
|      3 | 5502 | `				return PH7_OK;` |
|    ! 0 | 5503 | `			}else{` |
|      - | 5504 | `				/* Update offset */` |
|      9 | 5505 | `				zString += nOfft;` |
|      9 | 5506 | `				iLen -= nOfft;` |
|      - | 5507 | `			}` |
|      - | 5508 | `		}` |
|      9 | 5509 | `		if( nArg > 3 ){` |
|      - | 5510 | `			int iUserlen;` |
|      - | 5511 | `			/* Extract the desired length */` |
|    ! 0 | 5512 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 5513 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 5514 | `				iLen = iUserlen;` |
|    ! 0 | 5515 | `			}` |
|    ! 0 | 5516 | `		}` |
|      4 | 5517 | `	}` |
|      - | 5518 | `	/* Point to the end of the string */` |
|     11 | 5519 | `	zEnd = &zString[iLen];` |
|      - | 5520 | `	/* Extract the first non-space token */` |
|     11 | 5521 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 5522 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5523 | `		/* Compare against the current mask */` |
|     11 | 5524 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 5525 | `	}` |
|      - | 5526 | `	/* Longest match */` |
|     11 | 5527 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 5528 | `	return PH7_OK;` |
|      9 | 5529 |  |
|      - | 5530 | `/*` |
|      - | 5531 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 5532 | ` *  Search a string for any of a set of characters.` |
|      - | 5533 | ` * Parameters` |
|      - | 5534 | ` *  $haystack` |
|      - | 5535 | ` *   The string where char_list is looked for.` |
|      - | 5536 | ` *  $char_list` |
|      - | 5537 | ` *   This parameter is case sensitive.` |
|      - | 5538 | ` * Return` |
|      - | 5539 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 5540 | ` */` |
|      6 | 5541 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5542 |  |
|      - | 5543 | `	const char *zString,*zList,*zEnd;` |
|      - | 5544 | `	int iLen,iListLen,i,c;` |
|      - | 5545 | `	sxu32 nOfft,nMax;` |
|      - | 5546 | `	sxi32 rc;` |
|      7 | 5547 | `	if( nArg < 2 ){` |
|      - | 5548 | `		/* Missing arguments,return FALSE */` |
|      3 | 5549 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5550 | `		return PH7_OK;` |
|      - | 5551 | `	}` |
|      - | 5552 | `	/* Extract the haystack and the char list */` |
|      5 | 5553 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 5554 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 5555 | `	if( iLen < 1 ){` |
|      - | 5556 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 5557 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5558 | `		return PH7_OK;` |
|      - | 5559 | `	}` |
|      - | 5560 | `	/* Point to the end of the string */` |
|      5 | 5561 | `	zEnd = &zString[iLen];` |
|      5 | 5562 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 5563 | `	/* perform the requested operation */` |
|     15 | 5564 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 5565 | `		c = zList[i];` |
|     11 | 5566 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 5567 | `		if( rc == SXRET_OK ){` |
|      5 | 5568 | `			if( nMax < nOfft ){` |
|      3 | 5569 | `				nOfft = nMax;` |
|      1 | 5570 | `			}` |
|      2 | 5571 | `		}` |
|      6 | 5572 | `	}` |
|      5 | 5573 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 5574 | `		/* No such substring,return FALSE */` |
|      3 | 5575 | `		ph7_result_bool(pCtx,0);` |
|      2 | 5576 | `	}else{` |
|      - | 5577 | `		/* Return the substring */` |
|      3 | 5578 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 5579 | `	}` |
|      5 | 5580 | `	return PH7_OK;` |
|      4 | 5581 |  |
|      - | 5582 | `/*` |
|      - | 5583 | ` * string soundex(string $str)` |
|      - | 5584 | ` *  Calculate the soundex key of a string.` |
|      - | 5585 | ` * Parameters` |
|      - | 5586 | ` *  $str` |
|      - | 5587 | ` *   The input string.` |
|      - | 5588 | ` * Return` |
|      - | 5589 | ` *  Returns the soundex key as a string.` |
|      - | 5590 | ` * Note:` |
|      - | 5591 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 5592 | ` * source tree.` |
|      - | 5593 | ` */` |
|     20 | 5594 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5595 |  |
|      - | 5596 | `	const unsigned char *zIn;` |
|      - | 5597 | `	char zResult[8];` |
|      - | 5598 | `	int i, j;` |
|      - | 5599 | `	static const unsigned char iCode[] = {` |
|      - | 5600 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 5601 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 5602 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 5603 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 5604 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 5605 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5606 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 5607 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5608 | `	};` |
|     21 | 5609 | `	if( nArg < 1 ){` |
|      - | 5610 | `		/* Missing arguments,return the empty string */` |
|      3 | 5611 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5612 | `		return PH7_OK;` |
|      - | 5613 | `	}` |
|     19 | 5614 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     19 | 5615 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     19 | 5616 | `	if( zIn[i] ){` |
|     17 | 5617 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 5618 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 5619 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 5620 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 5621 | `			if( code>0 ){` |
|     45 | 5622 | `				if( code!=prevcode ){` |
|     33 | 5623 | `					prevcode = (unsigned char)code;` |
|     33 | 5624 | `					zResult[j++] = (char)code + '0';` |
|     16 | 5625 | `				}` |
|     23 | 5626 | `			}else{` |
|     49 | 5627 | `				prevcode = 0;` |
|      - | 5628 | `			}` |
|     47 | 5629 | `		}` |
|     33 | 5630 | `		while( j<4 ){` |
|     17 | 5631 | `			zResult[j++] = '0';` |
|      1 | 5632 | `		}` |
|     17 | 5633 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 5634 | `	}else{` |
|      3 | 5635 | `	  ph7_result_string(pCtx,"?000",4);` |
|      - | 5636 | `	}` |
|     19 | 5637 | `	return PH7_OK;` |
|     11 | 5638 |  |
|      - | 5639 | `/*` |
|      - | 5640 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 5641 | ` *  Wraps a string to a given number of characters.` |
|      - | 5642 | ` * Parameters` |
|      - | 5643 | ` *  $str` |
|      - | 5644 | ` *   The input string.` |
|      - | 5645 | ` * $width` |
|      - | 5646 | ` *  The column width.` |
|      - | 5647 | ` * $break` |
|      - | 5648 | ` *  The line is broken using the optional break parameter.` |
|      - | 5649 | ` * Return` |
|      - | 5650 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 5651 | ` */` |
|     14 | 5652 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5653 |  |
|      - | 5654 | `	const char *zIn,*zEnd,*zBreak;` |
|      - | 5655 | `	int iLen,iBreaklen,iChunk;` |
|     15 | 5656 | `	if( nArg < 1 ){` |
|      - | 5657 | `		/* Missing arguments,return the empty string */` |
|      3 | 5658 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5659 | `		return PH7_OK;` |
|      - | 5660 | `	}` |
|      - | 5661 | `	/* Extract the input string */` |
|     13 | 5662 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|     13 | 5663 | `	if( iLen < 1 ){` |
|      - | 5664 | `		/* Nothing to process,return the empty string */` |
|      3 | 5665 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5666 | `		return PH7_OK;` |
|      - | 5667 | `	}` |
|      - | 5668 | `	/* Chunk length */` |
|     11 | 5669 | `	iChunk = 75;` |
|     11 | 5670 | `	iBreaklen = 0;` |
|     11 | 5671 | `	zBreak = ""; /* cc warning */` |
|     11 | 5672 | `	if( nArg > 1 ){` |
|     11 | 5673 | `		iChunk = ph7_value_to_int(apArg[1]);` |
|     11 | 5674 | `		if( iChunk < 1 ){` |
|    ! 0 | 5675 | `			iChunk = 75;` |
|    ! 0 | 5676 | `		}` |
|     11 | 5677 | `		if( nArg > 2 ){` |
|      3 | 5678 | `			zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      1 | 5679 | `		}` |
|      5 | 5680 | `	}` |
|     11 | 5681 | `	if( iBreaklen < 1 ){` |
|      - | 5682 | `		/* Set a default column break */` |
|      - | 5683 | `#ifdef __WINNT__` |
|      1 | 5684 | `		zBreak = "\r\n";` |
|      1 | 5685 | `		iBreaklen = (int)sizeof("\r\n")-1;` |
|      - | 5686 | `#else` |
|      8 | 5687 | `		zBreak = "\n";` |
|      8 | 5688 | `		iBreaklen = (int)sizeof(char);` |
|      - | 5689 | `#endif` |
|      4 | 5690 | `	}` |
|      - | 5691 | `	/* Perform the requested operation */` |
|     11 | 5692 | `	zEnd = &zIn[iLen];` |
|     41 | 5693 | `	for(;;){` |
|      - | 5694 | `		int nMax;` |
|     47 | 5695 | `		if( zIn >= zEnd ){` |
|      - | 5696 | `			/* No more input to process */` |
|     11 | 5697 | `			break;` |
|      - | 5698 | `		}` |
|     37 | 5699 | `		nMax = (int)(zEnd-zIn);` |
|     37 | 5700 | `		if( iChunk > nMax ){` |
|     11 | 5701 | `			iChunk = nMax;` |
|      5 | 5702 | `		}` |
|      - | 5703 | `		/* Append the column first */` |
|     37 | 5704 | `		ph7_result_string(pCtx,zIn,iChunk); /* Will make it's own copy */` |
|      - | 5705 | `		/* Advance the cursor */` |
|     37 | 5706 | `		zIn += iChunk;` |
|     37 | 5707 | `		if( zIn < zEnd ){` |
|      - | 5708 | `			/* Append the line break */` |
|     27 | 5709 | `			ph7_result_string(pCtx,zBreak,iBreaklen);` |
|     13 | 5710 | `		}` |
|      1 | 5711 | `	}` |
|     11 | 5712 | `	return PH7_OK;` |
|      8 | 5713 |  |
|      - | 5714 | `/*` |
|      - | 5715 | ` * Check if the given character is a member of the given mask.` |
|      - | 5716 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 5717 | ` * Refer to [strtok()].` |
|      - | 5718 | ` */` |
|     30 | 5719 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 5720 |  |
|      - | 5721 | `	int i;` |
|     57 | 5722 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 5723 | `		if( c == zMask[i] ){` |
|     13 | 5724 | `			if( pOfft ){` |
|      5 | 5725 | `				*pOfft = i;` |
|      2 | 5726 | `			}` |
|     13 | 5727 | `			return TRUE;` |
|      - | 5728 | `		}` |
|     14 | 5729 | `	}` |
|     19 | 5730 | `	return FALSE;` |
|     16 | 5731 |  |
|      - | 5732 | `/*` |
|      - | 5733 | ` * Extract a single token from the input stream.` |
|      - | 5734 | ` * Refer to [strtok()].` |
|      - | 5735 | ` */` |
|      6 | 5736 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 5737 |  |
|      7 | 5738 | `	const char *zIn = *pzIn;` |
|      - | 5739 | `	const char *zPtr;` |
|      - | 5740 | `	/* Ignore leading delimiter */` |
|     11 | 5741 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5742 | `		zIn++;` |
|      1 | 5743 | `	}` |
|      7 | 5744 | `	if( zIn >= zEnd ){` |
|      - | 5745 | `		/* End of input */` |
|    ! 0 | 5746 | `		return SXERR_EOF;` |
|      - | 5747 | `	}` |
|      7 | 5748 | `	zPtr = zIn;` |
|      - | 5749 | `	/* Extract the token */` |
|     13 | 5750 | `	while( zIn < zEnd ){` |
|     11 | 5751 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 5752 | `			/* UTF-8 stream */` |
|    ! 0 | 5753 | `			zIn++;` |
|    ! 0 | 5754 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 5755 | `		}else{` |
|     11 | 5756 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5757 | `				break;` |
|      - | 5758 | `			}` |
|      7 | 5759 | `			zIn++;` |
|      - | 5760 | `		}` |
|      1 | 5761 | `	}` |
|      7 | 5762 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5763 | `	/* Update the cursor */` |
|      7 | 5764 | `	*pzIn = zIn;` |
|      - | 5765 | `	/* Return to the caller */` |
|      7 | 5766 | `	return SXRET_OK;` |
|      4 | 5767 |  |
|      - | 5768 | `/* strtok auxiliary private data */` |
|      - | 5769 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 5770 | `struct strtok_aux_data` |
|      - | 5771 |  |
|      - | 5772 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 5773 | `	const char *zIn;   /* Current input stream */` |
|      - | 5774 | `	const char *zEnd;  /* End of input */` |
|      - | 5775 | `};` |
|      - | 5776 | `/*` |
|      - | 5777 | ` * string strtok(string $str,string $token)` |
|      - | 5778 | ` * string strtok(string $token)` |
|      - | 5779 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 5780 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 5781 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 5782 | ` *  words by using the space character as the token.` |
|      - | 5783 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 5784 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 5785 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 5786 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 5787 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 5788 | ` *  the argument are found.` |
|      - | 5789 | ` * Parameters` |
|      - | 5790 | ` *  $str` |
|      - | 5791 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 5792 | ` * $token` |
|      - | 5793 | ` *  The delimiter used when splitting up str.` |
|      - | 5794 | ` * Return` |
|      - | 5795 | ` *   Current token or FALSE on EOF.` |
|      - | 5796 | ` */` |
|      8 | 5797 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5798 |  |
|      - | 5799 | `	strtok_aux_data *pAux;` |
|      - | 5800 | `	const char *zMask;` |
|      - | 5801 | `	SyString sToken;` |
|      - | 5802 | `	int nMasklen;` |
|      - | 5803 | `	sxi32 rc;` |
|      9 | 5804 | `	if( nArg < 2 ){` |
|      - | 5805 | `		/* Extract top aux data */` |
|      7 | 5806 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      7 | 5807 | `		if( pAux == 0 ){` |
|      - | 5808 | `			/* No aux data,return FALSE */` |
|    ! 0 | 5809 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5810 | `			return PH7_OK;` |
|      - | 5811 | `		}` |
|      7 | 5812 | `		nMasklen = 0;` |
|      7 | 5813 | `		zMask = ""; /* cc warning */` |
|      7 | 5814 | `		if( nArg > 0 ){` |
|      - | 5815 | `			/* Extract the mask */` |
|      5 | 5816 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 5817 | `		}` |
|      7 | 5818 | `		if( nMasklen < 1 ){` |
|      - | 5819 | `			/* Invalid mask,return FALSE */` |
|      3 | 5820 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|      3 | 5821 | `			ph7_context_free_chunk(pCtx,pAux);` |
|      3 | 5822 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|      3 | 5823 | `			ph7_result_bool(pCtx,0);` |
|      3 | 5824 | `			return PH7_OK;` |
|      - | 5825 | `		}` |
|      - | 5826 | `		/* Extract the token */` |
|      5 | 5827 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 5828 | `		if( rc != SXRET_OK ){` |
|      - | 5829 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 5830 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 5831 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5832 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 5833 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5834 | `		}else{` |
|      - | 5835 | `			/* Return the extracted token */` |
|      5 | 5836 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 5837 | `		}` |
|      3 | 5838 | `	}else{` |
|      - | 5839 | `		const char *zInput,*zCur;` |
|      - | 5840 | `		char *zDup;` |
|      - | 5841 | `		int nLen;` |
|      - | 5842 | `		/* Extract the raw input */` |
|      3 | 5843 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5844 | `		if( nLen < 1 ){` |
|      - | 5845 | `			/* Empty input,return FALSE */` |
|    ! 0 | 5846 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5847 | `			return PH7_OK;` |
|      - | 5848 | `		}` |
|      - | 5849 | `		/* Extract the mask */` |
|      3 | 5850 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 5851 | `		if( nMasklen < 1 ){` |
|      - | 5852 | `			/* Set a default mask */` |
|      - | 5853 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 5854 | `			zMask = TOK_MASK;` |
|    ! 0 | 5855 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 5856 | `#undef TOK_MASK` |
|    ! 0 | 5857 | `		}` |
|      - | 5858 | `		/* Extract a single token */` |
|      3 | 5859 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 5860 | `		if( rc != SXRET_OK ){` |
|      - | 5861 | `			/* Empty input */` |
|    ! 0 | 5862 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5863 | `			return PH7_OK;` |
|    ! 0 | 5864 | `		}else{` |
|      - | 5865 | `			/* Return the extracted token */` |
|      3 | 5866 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 5867 | `		}` |
|      - | 5868 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 5869 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 5870 | `		if( pAux ){` |
|      3 | 5871 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 5872 | `			if( nLen < 1 ){` |
|    ! 0 | 5873 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5874 | `				return PH7_OK;` |
|      - | 5875 | `			}` |
|      - | 5876 | `			/* Duplicate input */` |
|      3 | 5877 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 5878 | `			if( zDup  ){` |
|      3 | 5879 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 5880 | `				/* Register the aux data */` |
|      3 | 5881 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 5882 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 5883 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 5884 | `			}` |
|      1 | 5885 | `		}` |
|      - | 5886 | `	}` |
|      7 | 5887 | `	return PH7_OK;` |
|      5 | 5888 |  |
|      - | 5889 | `/*` |
|      - | 5890 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 5891 | ` *  Pad a string to a certain length with another string` |
|      - | 5892 | ` * Parameters` |
|      - | 5893 | ` *  $input` |
|      - | 5894 | ` *   The input string.` |
|      - | 5895 | ` * $pad_length` |
|      - | 5896 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 5897 | ` *   string, no padding takes place.` |
|      - | 5898 | ` * $pad_string` |
|      - | 5899 | ` *   Note:` |
|      - | 5900 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 5901 | ` *    divided by the pad_string's length.` |
|      - | 5902 | ` * $pad_type` |
|      - | 5903 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 5904 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 5905 | ` * Return` |
|      - | 5906 | ` *  The padded string.` |
|      - | 5907 | ` */` |
|     10 | 5908 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5909 |  |
|      - | 5910 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 5911 | `	const char *zIn,*zPad;` |
|     11 | 5912 | `	if( nArg < 2 ){` |
|      - | 5913 | `		/* Missing arguments,return the empty string */` |
|      5 | 5914 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 5915 | `		return PH7_OK;` |
|      - | 5916 | `	}` |
|      - | 5917 | `	/* Extract the target string */` |
|      7 | 5918 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5919 | `	/* Padding length */` |
|      7 | 5920 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|      7 | 5921 | `	if( iPadlen > 0 ){` |
|      5 | 5922 | `		iPadlen -= iLen;` |
|      2 | 5923 | `	}` |
|      7 | 5924 | `	if( iPadlen < 1  ){` |
|      - | 5925 | `		/* Return the string verbatim */` |
|      3 | 5926 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      3 | 5927 | `		return PH7_OK;` |
|      - | 5928 | `	}` |
|      5 | 5929 | `	zPad = " "; /* Whitespace padding */` |
|      5 | 5930 | `	iStrpad = (int)sizeof(char);` |
|      5 | 5931 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      5 | 5932 | `	if( nArg > 2 ){` |
|      - | 5933 | `		/* Padding string */` |
|      5 | 5934 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      5 | 5935 | `		if( iStrpad < 1 ){` |
|      - | 5936 | `			/* Empty string */` |
|    ! 0 | 5937 | `			zPad = " "; /* Whitespace padding */` |
|    ! 0 | 5938 | `			iStrpad = (int)sizeof(char);` |
|    ! 0 | 5939 | `		}` |
|      5 | 5940 | `		if( nArg > 3 ){` |
|      - | 5941 | `			/* Padd type */` |
|      5 | 5942 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 5943 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 5944 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 5945 | `			}` |
|      2 | 5946 | `		}` |
|      2 | 5947 | `	}` |
|      5 | 5948 | `	iDiv = 1;` |
|      5 | 5949 | `	if( iType == 2 ){` |
|    ! 0 | 5950 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 5951 | `	}` |
|      - | 5952 | `	/* Perform the requested operation */` |
|      5 | 5953 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 5954 | `		jPad = iStrpad;` |
|      5 | 5955 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 5956 | `			/* Padding */` |
|      5 | 5957 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 5958 | `				break;` |
|      - | 5959 | `			}` |
|      3 | 5960 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      2 | 5961 | `		}` |
|      3 | 5962 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 5963 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 5964 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 5965 | `				if( jPad > iStrpad ){` |
|    ! 0 | 5966 | `					jPad = iStrpad;` |
|    ! 0 | 5967 | `				}` |
|      3 | 5968 | `				if( jPad < 1){` |
|    ! 0 | 5969 | `					break;` |
|      - | 5970 | `				}` |
|      3 | 5971 | `				ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 5972 | `			}` |
|      1 | 5973 | `		}` |
|      1 | 5974 | `	}` |
|      5 | 5975 | `	if( iLen > 0 ){` |
|      - | 5976 | `		/* Append the input string */` |
|      5 | 5977 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      2 | 5978 | `	}` |
|      5 | 5979 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 5980 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 5981 | `			/* Padding */` |
|      5 | 5982 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 5983 | `				break;` |
|      - | 5984 | `			}` |
|      3 | 5985 | `			ph7_result_string(pCtx,zPad,iStrpad);` |
|      2 | 5986 | `		}` |
|      5 | 5987 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 5988 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 5989 | `			if( jPad > iStrpad ){` |
|    ! 0 | 5990 | `				jPad = iStrpad;` |
|    ! 0 | 5991 | `			}` |
|      3 | 5992 | `			if( jPad < 1){` |
|    ! 0 | 5993 | `				break;` |
|      - | 5994 | `			}` |
|      3 | 5995 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 5996 | `		}` |
|      1 | 5997 | `	}` |
|      5 | 5998 | `	return PH7_OK;` |
|      6 | 5999 |  |
|      - | 6000 | `/*` |
|      - | 6001 | ` * String replacement private data.` |
|      - | 6002 | ` */` |
|      - | 6003 | `typedef struct str_replace_data str_replace_data;` |
|      - | 6004 | `struct str_replace_data` |
|      - | 6005 |  |
|      - | 6006 | `	/* The following two fields are only used by the strtr function */` |
|      - | 6007 | `	SyBlob *pWorker;         /* Working buffer */` |
|      - | 6008 | `	ProcStringMatch xMatch;  /* Pattern match routine */` |
|      - | 6009 | `	/* The following two fields are only used by the str_replace function */` |
|      - | 6010 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 6011 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 6012 | `};` |
|      - | 6013 | `/*` |
|      - | 6014 | ` * Remove a substring.` |
|      - | 6015 | ` */` |
|      - | 6016 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 6017 | `	for(;;){\` |
|      - | 6018 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 6019 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 6020 | `		++OFFT;\` |
|      - | 6021 | `	}\` |
|      - | 6022 |  |
|      - | 6023 | `/*` |
|      - | 6024 | ` * Shift right and insert algorithm.` |
|      - | 6025 | ` */` |
|      - | 6026 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 6027 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 6028 | `		for(;;){\` |
|      - | 6029 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 6030 | `			if(INLEN < 1 ) { break; }\` |
|      - | 6031 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 6032 | `			--INLEN; \` |
|      - | 6033 | `		}\` |
|      - | 6034 | `		for(;;){\` |
|      - | 6035 | `				if(ELEN < 1) { break; }\` |
|      - | 6036 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 6037 | `				OFFT++;\` |
|      - | 6038 | `				ENTRY++;\` |
|      - | 6039 | `				--ELEN;\` |
|      - | 6040 | `		}\` |
|      - | 6041 |  |
|      - | 6042 | `/*` |
|      - | 6043 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 6044 | ` * replacement string [i.e: zReplace].` |
|      - | 6045 | ` */` |
|     38 | 6046 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 6047 |  |
|     39 | 6048 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 6049 | `	sxu32 n,m;` |
|     39 | 6050 | `	n = SyBlobLength(pWorker);` |
|     39 | 6051 | `	m = nOfft;` |
|      - | 6052 | `	/* Delete the old entry */` |
|    475 | 6053 | `	STRDEL(zInput,n,m,nLen);` |
|     39 | 6054 | `	SyBlobLength(pWorker) -= nLen;` |
|     39 | 6055 | `	if( nReplen > 0 ){` |
|     33 | 6056 | `		sxi32 iRep = nReplen;` |
|      - | 6057 | `		sxi32 rc;` |
|      - | 6058 | `		/*` |
|      - | 6059 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 6060 | `		 * string.` |
|      - | 6061 | `		 */` |
|     33 | 6062 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     33 | 6063 | `		if( rc != SXRET_OK ){` |
|      - | 6064 | `			/* Simply ignore any memory failure problem */` |
|    ! 0 | 6065 | `			return SXRET_OK;` |
|      - | 6066 | `		}` |
|      - | 6067 | `		/* Perform the insertion now */` |
|     33 | 6068 | `		zInput = (char *)SyBlobData(pWorker);` |
|     33 | 6069 | `		n = SyBlobLength(pWorker);` |
|    163 | 6070 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     33 | 6071 | `		SyBlobLength(pWorker) += nReplen;` |
|     16 | 6072 | `	}` |
|     39 | 6073 | `	return SXRET_OK;` |
|     20 | 6074 |  |
|      - | 6075 | `/*` |
|      - | 6076 | ` * String replacement walker callback.` |
|      - | 6077 | ` * The following callback is invoked for each array entry that hold` |
|      - | 6078 | ` * the replace string.` |
|      - | 6079 | ` * Refer to the strtr() implementation for more information.` |
|      - | 6080 | ` */` |
|      8 | 6081 | `static int StringReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6082 |  |
|      9 | 6083 | `	str_replace_data *pRepData = (str_replace_data *)pUserData;` |
|      - | 6084 | `	const char *zTarget,*zReplace;` |
|      - | 6085 | `	SyBlob *pWorker;` |
|      - | 6086 | `	int tLen,nLen;` |
|      - | 6087 | `	sxu32 nOfft;` |
|      - | 6088 | `	sxi32 rc;` |
|      - | 6089 | `	/* Point to the working buffer */` |
|      9 | 6090 | `	pWorker = pRepData->pWorker;` |
|      9 | 6091 | `	if( !ph7_value_is_string(pKey) ){` |
|      - | 6092 | `		/* Target and replace must be a string */` |
|      3 | 6093 | `		return PH7_OK;` |
|      - | 6094 | `	}` |
|      - | 6095 | `	/* Extract the target and the replace */` |
|      7 | 6096 | `	zTarget = ph7_value_to_string(pKey,&tLen);` |
|      7 | 6097 | `	if( tLen < 1 ){` |
|      - | 6098 | `		/* Empty target,return immediately */` |
|    ! 0 | 6099 | `		return PH7_OK;` |
|      - | 6100 | `	}` |
|      - | 6101 | `	/* Perform a pattern search */` |
|      7 | 6102 | `	rc = pRepData->xMatch(SyBlobData(pWorker),SyBlobLength(pWorker),(const void *)zTarget,(sxu32)tLen,&nOfft);` |
|      7 | 6103 | `	if( rc != SXRET_OK ){` |
|      - | 6104 | `		/* Pattern not found */` |
|    ! 0 | 6105 | `		return PH7_OK;` |
|      - | 6106 | `	}` |
|      - | 6107 | `	/* Extract the replace string */` |
|      7 | 6108 | `	zReplace = ph7_value_to_string(pData,&nLen);` |
|      - | 6109 | `	/* Perform the replace process */` |
|      7 | 6110 | `	StringReplace(pWorker,nOfft,tLen,zReplace,nLen);` |
|      - | 6111 | `	/* All done */` |
|      7 | 6112 | `	return PH7_OK;` |
|      5 | 6113 |  |
|      - | 6114 | `/*` |
|      - | 6115 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 6116 | ` * to collect search/replace string.` |
|      - | 6117 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 6118 | ` */` |
|     26 | 6119 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6120 |  |
|     27 | 6121 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 6122 | `	SyString sWorker;` |
|      - | 6123 | `	const char *zIn;` |
|      - | 6124 | `	int nByte;` |
|      - | 6125 | `	/* Extract a string representation of the given argument */` |
|     27 | 6126 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 6127 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 6128 | `	if( nByte > 0 ){` |
|      - | 6129 | `		char *zDup;` |
|      - | 6130 | `		/* Duplicate the chunk */` |
|     25 | 6131 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 6132 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 6133 | `			);` |
|     25 | 6134 | `		if( zDup == 0 ){` |
|      - | 6135 | `			/* Ignore any memory failure problem */` |
|    ! 0 | 6136 | `			ph7_context_throw_error(pRep->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 6137 | `			return PH7_OK;` |
|      - | 6138 | `		}` |
|     25 | 6139 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 6140 | `		/* Save the chunk */` |
|     25 | 6141 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 6142 | `	}` |
|      - | 6143 | `	/* Save for later processing */` |
|     27 | 6144 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 6145 | `	/* All done */` |
|     13 | 6146 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 6147 | `	return PH7_OK;` |
|     14 | 6148 |  |
|      - | 6149 | `/*` |
|      - | 6150 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6151 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6152 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 6153 | ` * Parameters` |
|      - | 6154 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 6155 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 6156 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 6157 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 6158 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 6159 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 6160 | ` * $search` |
|      - | 6161 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 6162 | ` *  to designate multiple needles.` |
|      - | 6163 | ` * $replace` |
|      - | 6164 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 6165 | ` *  to designate multiple replacements.` |
|      - | 6166 | ` * $subject` |
|      - | 6167 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 6168 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 6169 | ` *  of subject, and the return value is an array as well.` |
|      - | 6170 | ` * $count (Not used)` |
|      - | 6171 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 6172 | ` * Return` |
|      - | 6173 | ` * This function returns a string or an array with the replaced values.` |
|      - | 6174 | ` */` |
|  12202 | 6175 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6176 |  |
|      - | 6177 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 6178 | `	ProcStringMatch xMatch;` |
|      - | 6179 | `	const char *zIn,*zFunc;` |
|      - | 6180 | `	str_replace_data sRep;` |
|      - | 6181 | `	SyBlob sWorker;` |
|      - | 6182 | `	SySet sReplace;` |
|      - | 6183 | `	SySet sSearch;` |
|      - | 6184 | `	int rep_str;` |
|      - | 6185 | `	int nByte;` |
|      - | 6186 | `	sxi32 rc;` |
|  12204 | 6187 | `	if( nArg < 3 ){` |
|      - | 6188 | `		/* Missing/Invalid arguments,return null */` |
|      7 | 6189 | `		ph7_result_null(pCtx);` |
|      7 | 6190 | `		return PH7_OK;` |
|      - | 6191 | `	}` |
|      - | 6192 | `	/* Initialize fields */` |
|  12198 | 6193 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  12198 | 6194 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  12198 | 6195 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  12198 | 6196 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  12198 | 6197 | `	sRep.pCtx = pCtx;` |
|  12198 | 6198 | `	sRep.pCollector = &sSearch;` |
|  12198 | 6199 | `	rep_str = 0;` |
|      - | 6200 | `	/* Extract the subject */` |
|  12198 | 6201 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  12198 | 6202 | `	if( nByte < 1 ){` |
|      - | 6203 | `		/* Nothing to replace,return the empty string */` |
|     38 | 6204 | `		ph7_result_string(pCtx,"",0);` |
|     38 | 6205 | `		return PH7_OK;` |
|      - | 6206 | `	}` |
|      - | 6207 | `	/* Copy the subject */` |
|  12162 | 6208 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 6209 | `	/* Search string */` |
|  12162 | 6210 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 6211 | `		/* Collect search string */` |
|      9 | 6212 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 6213 | `	}else{` |
|      - | 6214 | `		/* Single pattern */` |
|  12154 | 6215 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  12154 | 6216 | `		if( nByte < 1 ){` |
|      - | 6217 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 6218 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 6219 | `			return PH7_OK;` |
|      - | 6220 | `		}` |
|  12150 | 6221 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6222 | `		/* Save for later processing */` |
|  12150 | 6223 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 6224 | `	}` |
|      - | 6225 | `	/* Replace string */` |
|  12158 | 6226 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 6227 | `		/* Collect replace string */` |
|      7 | 6228 | `		sRep.pCollector = &sReplace;` |
|      7 | 6229 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 6230 | `	}else{` |
|      - | 6231 | `		/* Single needle */` |
|  12152 | 6232 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  12152 | 6233 | `		rep_str = 1;` |
|  12152 | 6234 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6235 | `		/* Save for later processing */` |
|  12152 | 6236 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 6237 | `	}` |
|      - | 6238 | `	/* Reset loop cursors */` |
|  12158 | 6239 | `	SySetResetCursor(&sSearch);` |
|  12158 | 6240 | `	SySetResetCursor(&sReplace);` |
|  12158 | 6241 | `	pReplace = pSearch = 0; /* cc warning */` |
|  12158 | 6242 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 6243 | `	/* Extract function name */` |
|  12158 | 6244 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 6245 | `	/* Set the default pattern match routine */` |
|  12158 | 6246 | `	xMatch = SyBlobSearch;` |
|  12158 | 6247 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 6248 | `		/* Case insensitive pattern match */` |
|     11 | 6249 | `		xMatch = iPatternMatch;` |
|      5 | 6250 | `	}` |
|      - | 6251 | `	/* Start the replace process */` |
|  24322 | 6252 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 6253 | `		sxu32 nCount,nOfft;` |
|  12166 | 6254 | `		if( pSearch->nByte <  1 ){` |
|      - | 6255 | `			/* Empty string,ignore */` |
|      3 | 6256 | `			continue;` |
|      - | 6257 | `		}` |
|      - | 6258 | `		/* Extract the replace string */` |
|  12164 | 6259 | `		if( rep_str ){` |
|  12154 | 6260 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|   6078 | 6261 | `		}else{` |
|     11 | 6262 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 6263 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 6264 | `				 * An empty string is used for the rest of replacement values` |
|      - | 6265 | `				 */` |
|      3 | 6266 | `				pReplace = 0;` |
|      1 | 6267 | `			}` |
|      - | 6268 | `		}` |
|  12164 | 6269 | `		if( pReplace == 0 ){` |
|      - | 6270 | `			/* Use an empty string instead */` |
|      3 | 6271 | `			pReplace = &sTemp;` |
|      1 | 6272 | `		}` |
|  12164 | 6273 | `		nOfft = nCount = 0;` |
|   6097 | 6274 | `		for(;;){` |
|  12196 | 6275 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 6276 | `				break;` |
|      - | 6277 | `			}` |
|      - | 6278 | `			/* Perform a pattern lookup */` |
|  18275 | 6279 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  12182 | 6280 | `				pSearch->nByte,&nOfft);` |
|  12184 | 6281 | `			if( rc != SXRET_OK ){` |
|      - | 6282 | `				/* Pattern not found */` |
|  12152 | 6283 | `				break;` |
|      - | 6284 | `			}` |
|      - | 6285 | `			/* Perform the replace operation */` |
|     33 | 6286 | `			StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|      - | 6287 | `			/* Increment offset counter */` |
|     33 | 6288 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 6289 | `		}` |
|      2 | 6290 | `	}` |
|      - | 6291 | `	/* All done,clean-up the mess left behind */` |
|  12158 | 6292 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  12158 | 6293 | `	SySetRelease(&sSearch);` |
|  12158 | 6294 | `	SySetRelease(&sReplace);` |
|  12158 | 6295 | `	SyBlobRelease(&sWorker);` |
|  12158 | 6296 | `	return PH7_OK;` |
|   6103 | 6297 |  |
|      - | 6298 | `/*` |
|      - | 6299 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 6300 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 6301 | ` *  Translate characters or replace substrings.` |
|      - | 6302 | ` * Parameters` |
|      - | 6303 | ` *  $str` |
|      - | 6304 | ` *  The string being translated.` |
|      - | 6305 | ` * $from` |
|      - | 6306 | ` *  The string being translated to to.` |
|      - | 6307 | ` * $to` |
|      - | 6308 | ` *  The string replacing from.` |
|      - | 6309 | ` * $replace_pairs` |
|      - | 6310 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 6311 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 6312 | ` * Return` |
|      - | 6313 | ` *  The translated string.` |
|      - | 6314 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 6315 | ` */` |
|     12 | 6316 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6317 |  |
|      - | 6318 | `	const char *zIn;` |
|      - | 6319 | `	int nLen;` |
|     13 | 6320 | `	if( nArg < 1 ){` |
|      - | 6321 | `		/* Nothing to replace,return FALSE */` |
|      7 | 6322 | `		ph7_result_bool(pCtx,0);` |
|      7 | 6323 | `		return PH7_OK;` |
|      - | 6324 | `	}` |
|      7 | 6325 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 6326 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 6327 | `		/* Invalid arguments */` |
|    ! 0 | 6328 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6329 | `		return PH7_OK;` |
|      - | 6330 | `	}` |
|      9 | 6331 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 6332 | `		str_replace_data sRepData;` |
|      - | 6333 | `		SyBlob sWorker;` |
|      - | 6334 | `		/* Initilaize the working buffer */` |
|      5 | 6335 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|      - | 6336 | `		/* Copy raw string */` |
|      5 | 6337 | `		SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nLen);` |
|      - | 6338 | `		/* Init our replace data instance */` |
|      5 | 6339 | `		sRepData.pWorker = &sWorker;` |
|      5 | 6340 | `		sRepData.xMatch = SyBlobSearch;` |
|      - | 6341 | `		/* Iterate throw array entries and perform the replace operation.*/` |
|      5 | 6342 | `		ph7_array_walk(apArg[1],StringReplaceWalker,&sRepData);` |
|      - | 6343 | `		/* All done, return the result string */` |
|      7 | 6344 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|      4 | 6345 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 6346 | `		/* Clean-up */` |
|      5 | 6347 | `		SyBlobRelease(&sWorker);` |
|      3 | 6348 | `	}else{` |
|      - | 6349 | `		int i,flen,tlen,c,iOfft;` |
|      - | 6350 | `		const char *zFrom,*zTo;` |
|      3 | 6351 | `		if( nArg < 3 ){` |
|      - | 6352 | `			/* Nothing to replace */` |
|    ! 0 | 6353 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6354 | `			return PH7_OK;` |
|      - | 6355 | `		}` |
|      - | 6356 | `		/* Extract given arguments */` |
|      3 | 6357 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 6358 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 6359 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 6360 | `			/* Nothing to replace */` |
|    ! 0 | 6361 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6362 | `			return PH7_OK;` |
|      - | 6363 | `		}` |
|      - | 6364 | `		/* Start the replace process */` |
|     13 | 6365 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 6366 | `			c = zIn[i];` |
|     11 | 6367 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 6368 | `				if ( iOfft < tlen ){` |
|      5 | 6369 | `					c = zTo[iOfft];` |
|      2 | 6370 | `				}` |
|      2 | 6371 | `			}` |
|     11 | 6372 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 6373 |  |
|      6 | 6374 | `		}` |
|      - | 6375 | `	}` |
|      7 | 6376 | `	return PH7_OK;` |
|      7 | 6377 |  |
|      - | 6378 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6379 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6380 | `/*` |
|      - | 6381 | ` * Parse an INI string.` |
|      - | 6382 |  |
|      - | 6383 | ` * According to wikipedia` |
|      - | 6384 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 6385 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 6386 | ` *  Format` |
|      - | 6387 | `*    Properties` |
|      - | 6388 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 6389 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 6390 | `*     Example:` |
|      - | 6391 | `*      name=value` |
|      - | 6392 | `*    Sections` |
|      - | 6393 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 6394 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 6395 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 6396 | `*     or the end of the file. Sections may not be nested.` |
|      - | 6397 | `*     Example:` |
|      - | 6398 | `*      [section]` |
|      - | 6399 | `*   Comments` |
|      - | 6400 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 6401 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 6402 | `*/` |
|     10 | 6403 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 6404 |  |
|      - | 6405 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     11 | 6406 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 6407 | `	SyHashEntry *pEntry;` |
|      - | 6408 | `	SyString sEntry;` |
|      - | 6409 | `	SyHash sHash;` |
|      - | 6410 | `	int c;` |
|      - | 6411 | `	/* Create an empty array and worker variables */` |
|     11 | 6412 | `	pArray = ph7_context_new_array(pCtx);` |
|     11 | 6413 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     11 | 6414 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     11 | 6415 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 6416 | `		/* Out of memory */` |
|    ! 0 | 6417 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      - | 6418 | `		/* Return FALSE */` |
|    ! 0 | 6419 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6420 | `		return PH7_OK;` |
|      - | 6421 | `	}` |
|     11 | 6422 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     11 | 6423 | `	pCur = pArray;` |
|      - | 6424 | `	/* Start the parse process */` |
|     20 | 6425 | `	for(;;){` |
|      - | 6426 | `		/* Ignore leading white spaces */` |
|     67 | 6427 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 6428 | `			zIn++;` |
|      1 | 6429 | `		}` |
|     41 | 6430 | `		if( zIn >= zEnd ){` |
|      - | 6431 | `			/* No more input to process */` |
|     11 | 6432 | `			break;` |
|      - | 6433 | `		}` |
|     31 | 6434 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6435 | `			/* Comment til the end of line */` |
|    ! 0 | 6436 | `			zIn++;` |
|    ! 0 | 6437 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 6438 | `				zIn++;` |
|    ! 0 | 6439 | `			}` |
|    ! 0 | 6440 | `			continue;` |
|      - | 6441 | `		}` |
|      - | 6442 | `		/* Reset the string cursor of the working variable */` |
|     31 | 6443 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 6444 | `		if( zIn[0] == '[' ){` |
|      - | 6445 | `			/* Section: Extract the section name */` |
|      9 | 6446 | `			zIn++;` |
|      9 | 6447 | `			zCur = zIn;` |
|     73 | 6448 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 6449 | `				zIn++;` |
|      1 | 6450 | `			}` |
|      9 | 6451 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 6452 | `				/* Save the section name */` |
|      5 | 6453 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 6454 | `				SyStringFullTrim(&sEntry);` |
|      5 | 6455 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 6456 | `				if( sEntry.nByte > 0 ){` |
|      - | 6457 | `					/* Associate an array with the section */` |
|      5 | 6458 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 6459 | `					if( pSection ){` |
|      5 | 6460 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 6461 | `						pCur = pSection;` |
|      2 | 6462 | `					}` |
|      2 | 6463 | `				}` |
|      2 | 6464 | `			}` |
|      9 | 6465 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 6466 | `		}else{` |
|      - | 6467 | `			ph7_value *pOldCur;` |
|      - | 6468 | `			int is_array;` |
|      - | 6469 | `			int iLen;` |
|      - | 6470 | `			/* Properties */` |
|     23 | 6471 | `			is_array = 0;` |
|     23 | 6472 | `			zCur = zIn;` |
|     23 | 6473 | `			iLen = 0; /* cc warning */` |
|     23 | 6474 | `			pOldCur = pCur;` |
|    155 | 6475 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 6476 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 6477 | `					/* Array */` |
|    ! 0 | 6478 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 6479 | `					is_array = 1;` |
|    ! 0 | 6480 | `					if( iLen > 0 ){` |
|    ! 0 | 6481 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 6482 | `						/* Query the hashtable */` |
|    ! 0 | 6483 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 6484 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 6485 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 6486 | `						if( pEntry ){` |
|    ! 0 | 6487 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 6488 | `						}else{` |
|      - | 6489 | `							/* Create an empty array */` |
|    ! 0 | 6490 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 6491 | `							if( pvArr ){` |
|      - | 6492 | `								/* Save the entry */` |
|    ! 0 | 6493 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 6494 | `								/* Insert the entry */` |
|    ! 0 | 6495 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6496 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 6497 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 6498 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6499 | `							}` |
|      - | 6500 | `						}` |
|    ! 0 | 6501 | `						if( pvArr ){` |
|    ! 0 | 6502 | `							pCur = pvArr;` |
|    ! 0 | 6503 | `						}` |
|    ! 0 | 6504 | `					}` |
|    ! 0 | 6505 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 6506 | `						zIn++;` |
|    ! 0 | 6507 | `					}` |
|    ! 0 | 6508 | `				}` |
|    133 | 6509 | `				zIn++;` |
|      1 | 6510 | `			}` |
|     23 | 6511 | `			if( !is_array ){` |
|     23 | 6512 | `				iLen = (int)(zIn-zCur);` |
|     11 | 6513 | `			}` |
|      - | 6514 | `			/* Trim the key */` |
|     23 | 6515 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 6516 | `			SyStringFullTrim(&sEntry);` |
|     23 | 6517 | `			if( sEntry.nByte > 0 ){` |
|     23 | 6518 | `				if( !is_array ){` |
|      - | 6519 | `					/* Save the key name */` |
|     23 | 6520 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 6521 | `				}` |
|      - | 6522 | `				/* extract key value */` |
|     23 | 6523 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 6524 | `				zIn++; /* '=' */` |
|     39 | 6525 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 6526 | `					zIn++;` |
|      1 | 6527 | `				}` |
|     23 | 6528 | `				if( zIn < zEnd ){` |
|     21 | 6529 | `					zCur = zIn;` |
|     21 | 6530 | `					c = zIn[0];` |
|     21 | 6531 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6532 | `						zIn++;` |
|      - | 6533 | `						/* Delimit the value */` |
|    ! 0 | 6534 | `						while( zIn < zEnd ){` |
|    ! 0 | 6535 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 6536 | `								break;` |
|      - | 6537 | `							}` |
|    ! 0 | 6538 | `							zIn++;` |
|    ! 0 | 6539 | `						}` |
|    ! 0 | 6540 | `						if( zIn < zEnd ){` |
|    ! 0 | 6541 | `							zIn++;` |
|    ! 0 | 6542 | `						}` |
|    ! 0 | 6543 | `					}else{` |
|    125 | 6544 | `						while( zIn < zEnd ){` |
|    123 | 6545 | `							if( zIn[0] == '\n' ){` |
|     19 | 6546 | `								if( zIn[-1] != '\\' ){` |
|     19 | 6547 | `									break;` |
|    ! 0 | 6548 | `								}` |
|    105 | 6549 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6550 | `								/* Inline comments */` |
|    ! 0 | 6551 | `								break;` |
|      - | 6552 | `							}` |
|    105 | 6553 | `							zIn++;` |
|      1 | 6554 | `						}` |
|      - | 6555 | `					}` |
|      - | 6556 | `					/* Trim the value */` |
|     21 | 6557 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 6558 | `					SyStringFullTrim(&sEntry);` |
|     21 | 6559 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6560 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 6561 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 6562 | `					}` |
|     21 | 6563 | `					if( sEntry.nByte > 0 ){` |
|     21 | 6564 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 6565 | `					}` |
|      - | 6566 | `					/* Insert the key and it's value */` |
|     21 | 6567 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 6568 | `				}` |
|     12 | 6569 | `			}else{` |
|    ! 0 | 6570 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 6571 | `					zIn++;` |
|    ! 0 | 6572 | `				}` |
|      - | 6573 | `			}` |
|     23 | 6574 | `			pCur = pOldCur;` |
|      - | 6575 | `		}` |
|      1 | 6576 | `	}` |
|     11 | 6577 | `	SyHashRelease(&sHash);` |
|      - | 6578 | `	/* Return the parse of the INI string */` |
|     11 | 6579 | `	ph7_result_value(pCtx,pArray);` |
|     11 | 6580 | `	return SXRET_OK;` |
|      6 | 6581 |  |
|      - | 6582 | `/*` |
|      - | 6583 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 6584 | ` *  Parse a configuration string.` |
|      - | 6585 | ` * Parameters` |
|      - | 6586 | ` *  $ini` |
|      - | 6587 | ` *   The contents of the ini file being parsed.` |
|      - | 6588 | ` *  $process_sections` |
|      - | 6589 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 6590 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 6591 | ` *  $scanner_mode (Not used)` |
|      - | 6592 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 6593 | ` *   then option values will not be parsed.` |
|      - | 6594 | ` * Return` |
|      - | 6595 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 6596 | ` */` |
|     10 | 6597 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6598 |  |
|      - | 6599 | `	const char *zIni;` |
|      - | 6600 | `	int nByte;` |
|     11 | 6601 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6602 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      3 | 6603 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6604 | `		return PH7_OK;` |
|      - | 6605 | `	}` |
|      - | 6606 | `	/* Extract the raw INI buffer */` |
|      9 | 6607 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 6608 | `	/* Process the INI buffer*/` |
|      9 | 6609 | `	PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      9 | 6610 | `	return PH7_OK;` |
|      6 | 6611 |  |
|      - | 6612 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6613 |  |
|      - | 6614 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6615 |  |
|      - | 6616 | `/*` |
|      - | 6617 | ` * Ctype Functions.` |
|      - | 6618 | ` * Status:` |
|      - | 6619 | ` *    Stable.` |
|      - | 6620 | ` */` |
|      - | 6621 | `/*` |
|      - | 6622 | ` * bool ctype_alnum(string $text)` |
|      - | 6623 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 6624 | ` * Parameters` |
|      - | 6625 | ` *  $text` |
|      - | 6626 | ` *   The tested string.` |
|      - | 6627 | ` * Return` |
|      - | 6628 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 6629 | ` */` |
|     16 | 6630 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6631 |  |
|      - | 6632 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6633 | `	int nLen;` |
|     17 | 6634 | `	if( nArg < 1 ){` |
|      - | 6635 | `		/* Missing arguments,return FALSE */` |
|      3 | 6636 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6637 | `		return PH7_OK;` |
|      - | 6638 | `	}` |
|      - | 6639 | `	/* Extract the target string */` |
|     15 | 6640 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 6641 | `	zEnd = &zIn[nLen];` |
|     15 | 6642 | `	if( nLen < 1 ){` |
|      - | 6643 | `		/* Empty string,return FALSE */` |
|      3 | 6644 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6645 | `		return PH7_OK;` |
|      - | 6646 | `	}` |
|      - | 6647 | `	/* Perform the requested operation */` |
|     32 | 6648 | `	for(;;){` |
|     65 | 6649 | `		if( zIn >= zEnd ){` |
|      - | 6650 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6651 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6652 | `			return PH7_OK;` |
|      - | 6653 | `		}` |
|     57 | 6654 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 6655 | `			break;` |
|      - | 6656 | `		}` |
|      - | 6657 | `		/* Point to the next character */` |
|     53 | 6658 | `		zIn++;` |
|      1 | 6659 | `	}` |
|      - | 6660 | `	/* The test failed,return FALSE */` |
|      5 | 6661 | `	ph7_result_bool(pCtx,0);` |
|      5 | 6662 | `	return PH7_OK;` |
|      9 | 6663 |  |
|      - | 6664 | `/*` |
|      - | 6665 | ` * bool ctype_alpha(string $text)` |
|      - | 6666 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 6667 | ` * Parameters` |
|      - | 6668 | ` *  $text` |
|      - | 6669 | ` *   The tested string.` |
|      - | 6670 | ` * Return` |
|      - | 6671 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 6672 | ` */` |
|     18 | 6673 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6674 |  |
|      - | 6675 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6676 | `	int nLen;` |
|     19 | 6677 | `	if( nArg < 1 ){` |
|      - | 6678 | `		/* Missing arguments,return FALSE */` |
|      3 | 6679 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6680 | `		return PH7_OK;` |
|      - | 6681 | `	}` |
|      - | 6682 | `	/* Extract the target string */` |
|     17 | 6683 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6684 | `	zEnd = &zIn[nLen];` |
|     17 | 6685 | `	if( nLen < 1 ){` |
|      - | 6686 | `		/* Empty string,return FALSE */` |
|      3 | 6687 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6688 | `		return PH7_OK;` |
|      - | 6689 | `	}` |
|      - | 6690 | `	/* Perform the requested operation */` |
|     42 | 6691 | `	for(;;){` |
|     85 | 6692 | `		if( zIn >= zEnd ){` |
|      - | 6693 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6694 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6695 | `			return PH7_OK;` |
|      - | 6696 | `		}` |
|     77 | 6697 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 6698 | `			break;` |
|      - | 6699 | `		}` |
|      - | 6700 | `		/* Point to the next character */` |
|     71 | 6701 | `		zIn++;` |
|      1 | 6702 | `	}` |
|      - | 6703 | `	/* The test failed,return FALSE */` |
|      7 | 6704 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6705 | `	return PH7_OK;` |
|     10 | 6706 |  |
|      - | 6707 | `/*` |
|      - | 6708 | ` * bool ctype_cntrl(string $text)` |
|      - | 6709 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 6710 | ` * Parameters` |
|      - | 6711 | ` *  $text` |
|      - | 6712 | ` *   The tested string.` |
|      - | 6713 | ` * Return` |
|      - | 6714 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 6715 | ` */` |
|     18 | 6716 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6717 |  |
|      - | 6718 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6719 | `	int nLen;` |
|     19 | 6720 | `	if( nArg < 1 ){` |
|      - | 6721 | `		/* Missing arguments,return FALSE */` |
|      3 | 6722 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6723 | `		return PH7_OK;` |
|      - | 6724 | `	}` |
|      - | 6725 | `	/* Extract the target string */` |
|     17 | 6726 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6727 | `	zEnd = &zIn[nLen];` |
|     17 | 6728 | `	if( nLen < 1 ){` |
|      - | 6729 | `		/* Empty string,return FALSE */` |
|      3 | 6730 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6731 | `		return PH7_OK;` |
|      - | 6732 | `	}` |
|      - | 6733 | `	/* Perform the requested operation */` |
|     14 | 6734 | `	for(;;){` |
|     29 | 6735 | `		if( zIn >= zEnd ){` |
|      - | 6736 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6737 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6738 | `			return PH7_OK;` |
|      - | 6739 | `		}` |
|     21 | 6740 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6741 | `			/* UTF-8 stream  */` |
|    ! 0 | 6742 | `			break;` |
|      - | 6743 | `		}` |
|     21 | 6744 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 6745 | `			break;` |
|      - | 6746 | `		}` |
|      - | 6747 | `		/* Point to the next character */` |
|     15 | 6748 | `		zIn++;` |
|      1 | 6749 | `	}` |
|      - | 6750 | `	/* The test failed,return FALSE */` |
|      7 | 6751 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6752 | `	return PH7_OK;` |
|     10 | 6753 |  |
|      - | 6754 | `/*` |
|      - | 6755 | ` * bool ctype_digit(string $text)` |
|      - | 6756 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 6757 | ` * Parameters` |
|      - | 6758 | ` *  $text` |
|      - | 6759 | ` *   The tested string.` |
|      - | 6760 | ` * Return` |
|      - | 6761 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 6762 | ` */` |
|   1488 | 6763 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6764 |  |
|      - | 6765 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6766 | `	int nLen;` |
|   1490 | 6767 | `	if( nArg < 1 ){` |
|      - | 6768 | `		/* Missing arguments,return FALSE */` |
|      3 | 6769 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6770 | `		return PH7_OK;` |
|      - | 6771 | `	}` |
|      - | 6772 | `	/* Extract the target string */` |
|   1488 | 6773 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1488 | 6774 | `	zEnd = &zIn[nLen];` |
|   1488 | 6775 | `	if( nLen < 1 ){` |
|      - | 6776 | `		/* Empty string,return FALSE */` |
|      3 | 6777 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6778 | `		return PH7_OK;` |
|      - | 6779 | `	}` |
|      - | 6780 | `	/* Perform the requested operation */` |
|   1394 | 6781 | `	for(;;){` |
|   2790 | 6782 | `		if( zIn >= zEnd ){` |
|      - | 6783 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1274 | 6784 | `			ph7_result_bool(pCtx,1);` |
|   1274 | 6785 | `			return PH7_OK;` |
|      - | 6786 | `		}` |
|   1518 | 6787 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6788 | `			/* UTF-8 stream  */` |
|    ! 0 | 6789 | `			break;` |
|      - | 6790 | `		}` |
|   1518 | 6791 | `		if( !SyisDigit(zIn[0]) ){` |
|    214 | 6792 | `			break;` |
|      - | 6793 | `		}` |
|      - | 6794 | `		/* Point to the next character */` |
|   1306 | 6795 | `		zIn++;` |
|      2 | 6796 | `	}` |
|      - | 6797 | `	/* The test failed,return FALSE */` |
|    214 | 6798 | `	ph7_result_bool(pCtx,0);` |
|    214 | 6799 | `	return PH7_OK;` |
|    746 | 6800 |  |
|      - | 6801 | `/*` |
|      - | 6802 | ` * bool ctype_xdigit(string $text)` |
|      - | 6803 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 6804 | ` * Parameters` |
|      - | 6805 | ` *  $text` |
|      - | 6806 | ` *   The tested string.` |
|      - | 6807 | ` * Return` |
|      - | 6808 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 6809 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 6810 | ` */` |
|     20 | 6811 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6812 |  |
|      - | 6813 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6814 | `	int nLen;` |
|     21 | 6815 | `	if( nArg < 1 ){` |
|      - | 6816 | `		/* Missing arguments,return FALSE */` |
|      3 | 6817 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6818 | `		return PH7_OK;` |
|      - | 6819 | `	}` |
|      - | 6820 | `	/* Extract the target string */` |
|     19 | 6821 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 6822 | `	zEnd = &zIn[nLen];` |
|     19 | 6823 | `	if( nLen < 1 ){` |
|      - | 6824 | `		/* Empty string,return FALSE */` |
|      3 | 6825 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6826 | `		return PH7_OK;` |
|      - | 6827 | `	}` |
|      - | 6828 | `	/* Perform the requested operation */` |
|     46 | 6829 | `	for(;;){` |
|     93 | 6830 | `		if( zIn >= zEnd ){` |
|      - | 6831 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 6832 | `			ph7_result_bool(pCtx,1);` |
|     11 | 6833 | `			return PH7_OK;` |
|      - | 6834 | `		}` |
|     83 | 6835 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6836 | `			/* UTF-8 stream  */` |
|    ! 0 | 6837 | `			break;` |
|      - | 6838 | `		}` |
|     83 | 6839 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 6840 | `			break;` |
|      - | 6841 | `		}` |
|      - | 6842 | `		/* Point to the next character */` |
|     77 | 6843 | `		zIn++;` |
|      1 | 6844 | `	}` |
|      - | 6845 | `	/* The test failed,return FALSE */` |
|      7 | 6846 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6847 | `	return PH7_OK;` |
|     11 | 6848 |  |
|      - | 6849 | `/*` |
|      - | 6850 | ` * bool ctype_graph(string $text)` |
|      - | 6851 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 6852 | ` * Parameters` |
|      - | 6853 | ` *  $text` |
|      - | 6854 | ` *   The tested string.` |
|      - | 6855 | ` * Return` |
|      - | 6856 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 6857 | ` * (no white space), FALSE otherwise.` |
|      - | 6858 | ` */` |
|     18 | 6859 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6860 |  |
|      - | 6861 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6862 | `	int nLen;` |
|     19 | 6863 | `	if( nArg < 1 ){` |
|      - | 6864 | `		/* Missing arguments,return FALSE */` |
|      3 | 6865 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6866 | `		return PH7_OK;` |
|      - | 6867 | `	}` |
|      - | 6868 | `	/* Extract the target string */` |
|     17 | 6869 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6870 | `	zEnd = &zIn[nLen];` |
|     17 | 6871 | `	if( nLen < 1 ){` |
|      - | 6872 | `		/* Empty string,return FALSE */` |
|      3 | 6873 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6874 | `		return PH7_OK;` |
|      - | 6875 | `	}` |
|      - | 6876 | `	/* Perform the requested operation */` |
|     57 | 6877 | `	for(;;){` |
|    115 | 6878 | `		if( zIn >= zEnd ){` |
|      - | 6879 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6880 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6881 | `			return PH7_OK;` |
|      - | 6882 | `		}` |
|    107 | 6883 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6884 | `			/* UTF-8 stream  */` |
|    ! 0 | 6885 | `			break;` |
|      - | 6886 | `		}` |
|    107 | 6887 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 6888 | `			break;` |
|      - | 6889 | `		}` |
|      - | 6890 | `		/* Point to the next character */` |
|    101 | 6891 | `		zIn++;` |
|      1 | 6892 | `	}` |
|      - | 6893 | `	/* The test failed,return FALSE */` |
|      7 | 6894 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6895 | `	return PH7_OK;` |
|     10 | 6896 |  |
|      - | 6897 | `/*` |
|      - | 6898 | ` * bool ctype_print(string $text)` |
|      - | 6899 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 6900 | ` * Parameters` |
|      - | 6901 | ` *  $text` |
|      - | 6902 | ` *   The tested string.` |
|      - | 6903 | ` * Return` |
|      - | 6904 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 6905 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 6906 | ` *  or control function at all.` |
|      - | 6907 | ` */` |
|     18 | 6908 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6909 |  |
|      - | 6910 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6911 | `	int nLen;` |
|     19 | 6912 | `	if( nArg < 1 ){` |
|      - | 6913 | `		/* Missing arguments,return FALSE */` |
|      3 | 6914 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6915 | `		return PH7_OK;` |
|      - | 6916 | `	}` |
|      - | 6917 | `	/* Extract the target string */` |
|     17 | 6918 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6919 | `	zEnd = &zIn[nLen];` |
|     17 | 6920 | `	if( nLen < 1 ){` |
|      - | 6921 | `		/* Empty string,return FALSE */` |
|      3 | 6922 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6923 | `		return PH7_OK;` |
|      - | 6924 | `	}` |
|      - | 6925 | `	/* Perform the requested operation */` |
|     63 | 6926 | `	for(;;){` |
|    127 | 6927 | `		if( zIn >= zEnd ){` |
|      - | 6928 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6929 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6930 | `			return PH7_OK;` |
|      - | 6931 | `		}` |
|    119 | 6932 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6933 | `			/* UTF-8 stream  */` |
|    ! 0 | 6934 | `			break;` |
|      - | 6935 | `		}` |
|    119 | 6936 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 6937 | `			break;` |
|      - | 6938 | `		}` |
|      - | 6939 | `		/* Point to the next character */` |
|    113 | 6940 | `		zIn++;` |
|      1 | 6941 | `	}` |
|      - | 6942 | `	/* The test failed,return FALSE */` |
|      7 | 6943 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6944 | `	return PH7_OK;` |
|     10 | 6945 |  |
|      - | 6946 | `/*` |
|      - | 6947 | ` * bool ctype_punct(string $text)` |
|      - | 6948 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 6949 | ` * Parameters` |
|      - | 6950 | ` *  $text` |
|      - | 6951 | ` *   The tested string.` |
|      - | 6952 | ` * Return` |
|      - | 6953 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 6954 | ` *  digit or blank, FALSE otherwise.` |
|      - | 6955 | ` */` |
|     20 | 6956 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6957 |  |
|      - | 6958 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6959 | `	int nLen;` |
|     21 | 6960 | `	if( nArg < 1 ){` |
|      - | 6961 | `		/* Missing arguments,return FALSE */` |
|      3 | 6962 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6963 | `		return PH7_OK;` |
|      - | 6964 | `	}` |
|      - | 6965 | `	/* Extract the target string */` |
|     19 | 6966 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 6967 | `	zEnd = &zIn[nLen];` |
|     19 | 6968 | `	if( nLen < 1 ){` |
|      - | 6969 | `		/* Empty string,return FALSE */` |
|      3 | 6970 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6971 | `		return PH7_OK;` |
|      - | 6972 | `	}` |
|      - | 6973 | `	/* Perform the requested operation */` |
|     38 | 6974 | `	for(;;){` |
|     77 | 6975 | `		if( zIn >= zEnd ){` |
|      - | 6976 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6977 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6978 | `			return PH7_OK;` |
|      - | 6979 | `		}` |
|     69 | 6980 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6981 | `			/* UTF-8 stream  */` |
|    ! 0 | 6982 | `			break;` |
|      - | 6983 | `		}` |
|     69 | 6984 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 6985 | `			break;` |
|      - | 6986 | `		}` |
|      - | 6987 | `		/* Point to the next character */` |
|     61 | 6988 | `		zIn++;` |
|      1 | 6989 | `	}` |
|      - | 6990 | `	/* The test failed,return FALSE */` |
|      9 | 6991 | `	ph7_result_bool(pCtx,0);` |
|      9 | 6992 | `	return PH7_OK;` |
|     11 | 6993 |  |
|      - | 6994 | `/*` |
|      - | 6995 | ` * bool ctype_space(string $text)` |
|      - | 6996 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 6997 | ` * Parameters` |
|      - | 6998 | ` *  $text` |
|      - | 6999 | ` *   The tested string.` |
|      - | 7000 | ` * Return` |
|      - | 7001 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 7002 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 7003 | ` *  and form feed characters.` |
|      - | 7004 | ` */` |
|  35962 | 7005 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 7006 |  |
|      - | 7007 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7008 | `	int nLen;` |
|  35964 | 7009 | `	if( nArg < 1 ){` |
|      - | 7010 | `		/* Missing arguments,return FALSE */` |
|      3 | 7011 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7012 | `		return PH7_OK;` |
|      - | 7013 | `	}` |
|      - | 7014 | `	/* Extract the target string */` |
|  35962 | 7015 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  35962 | 7016 | `	zEnd = &zIn[nLen];` |
|  35962 | 7017 | `	if( nLen < 1 ){` |
|      - | 7018 | `		/* Empty string,return FALSE */` |
|      3 | 7019 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7020 | `		return PH7_OK;` |
|      - | 7021 | `	}` |
|      - | 7022 | `	/* Perform the requested operation */` |
|  18302 | 7023 | `	for(;;){` |
|  36562 | 7024 | `		if( zIn >= zEnd ){` |
|      - | 7025 | `			/* If we reach the end of the string,then the test succeeded. */` |
|    580 | 7026 | `			ph7_result_bool(pCtx,1);` |
|    580 | 7027 | `			return PH7_OK;` |
|      - | 7028 | `		}` |
|  35984 | 7029 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7030 | `			/* UTF-8 stream  */` |
|    ! 0 | 7031 | `			break;` |
|      - | 7032 | `		}` |
|  35984 | 7033 | `		if( !SyisSpace(zIn[0]) ){` |
|  35382 | 7034 | `			break;` |
|      - | 7035 | `		}` |
|      - | 7036 | `		/* Point to the next character */` |
|    604 | 7037 | `		zIn++;` |
|      2 | 7038 | `	}` |
|      - | 7039 | `	/* The test failed,return FALSE */` |
|  35382 | 7040 | `	ph7_result_bool(pCtx,0);` |
|  35382 | 7041 | `	return PH7_OK;` |
|  18005 | 7042 |  |
|      - | 7043 | `/*` |
|      - | 7044 | ` * bool ctype_lower(string $text)` |
|      - | 7045 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 7046 | ` * Parameters` |
|      - | 7047 | ` *  $text` |
|      - | 7048 | ` *   The tested string.` |
|      - | 7049 | ` * Return` |
|      - | 7050 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 7051 | ` */` |
|     18 | 7052 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7053 |  |
|      - | 7054 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7055 | `	int nLen;` |
|     19 | 7056 | `	if( nArg < 1 ){` |
|      - | 7057 | `		/* Missing arguments,return FALSE */` |
|      3 | 7058 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7059 | `		return PH7_OK;` |
|      - | 7060 | `	}` |
|      - | 7061 | `	/* Extract the target string */` |
|     17 | 7062 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7063 | `	zEnd = &zIn[nLen];` |
|     17 | 7064 | `	if( nLen < 1 ){` |
|      - | 7065 | `		/* Empty string,return FALSE */` |
|      3 | 7066 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7067 | `		return PH7_OK;` |
|      - | 7068 | `	}` |
|      - | 7069 | `	/* Perform the requested operation */` |
|     27 | 7070 | `	for(;;){` |
|     55 | 7071 | `		if( zIn >= zEnd ){` |
|      - | 7072 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7073 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7074 | `			return PH7_OK;` |
|      - | 7075 | `		}` |
|     51 | 7076 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 7077 | `			break;` |
|      - | 7078 | `		}` |
|      - | 7079 | `		/* Point to the next character */` |
|     41 | 7080 | `		zIn++;` |
|      1 | 7081 | `	}` |
|      - | 7082 | `	/* The test failed,return FALSE */` |
|     11 | 7083 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7084 | `	return PH7_OK;` |
|     10 | 7085 |  |
|      - | 7086 | `/*` |
|      - | 7087 | ` * bool ctype_upper(string $text)` |
|      - | 7088 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 7089 | ` * Parameters` |
|      - | 7090 | ` *  $text` |
|      - | 7091 | ` *   The tested string.` |
|      - | 7092 | ` * Return` |
|      - | 7093 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 7094 | ` */` |
|     18 | 7095 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7096 |  |
|      - | 7097 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7098 | `	int nLen;` |
|     19 | 7099 | `	if( nArg < 1 ){` |
|      - | 7100 | `		/* Missing arguments,return FALSE */` |
|      3 | 7101 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7102 | `		return PH7_OK;` |
|      - | 7103 | `	}` |
|      - | 7104 | `	/* Extract the target string */` |
|     17 | 7105 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7106 | `	zEnd = &zIn[nLen];` |
|     17 | 7107 | `	if( nLen < 1 ){` |
|      - | 7108 | `		/* Empty string,return FALSE */` |
|      3 | 7109 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7110 | `		return PH7_OK;` |
|      - | 7111 | `	}` |
|      - | 7112 | `	/* Perform the requested operation */` |
|     28 | 7113 | `	for(;;){` |
|     57 | 7114 | `		if( zIn >= zEnd ){` |
|      - | 7115 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7116 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7117 | `			return PH7_OK;` |
|      - | 7118 | `		}` |
|     53 | 7119 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 7120 | `			break;` |
|      - | 7121 | `		}` |
|      - | 7122 | `		/* Point to the next character */` |
|     43 | 7123 | `		zIn++;` |
|      1 | 7124 | `	}` |
|      - | 7125 | `	/* The test failed,return FALSE */` |
|     11 | 7126 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7127 | `	return PH7_OK;` |
|     10 | 7128 |  |
|      - | 7129 | `/*` |
|      - | 7130 | ` * Date/Time functions` |
|      - | 7131 | ` * Status:` |
|      - | 7132 | ` *    Devel.` |
|      - | 7133 | ` */` |
|      - | 7134 | `#include <time.h>` |
|      - | 7135 | `#ifdef __WINNT__` |
|      - | 7136 | `/* GetSystemTime() */` |
|      - | 7137 | `#include <Windows.h>` |
|      - | 7138 | `#ifdef _WIN32_WCE` |
|      - | 7139 | `/*` |
|      - | 7140 | `** WindowsCE does not have a localtime() function.  So create a` |
|      - | 7141 | `** substitute.` |
|      - | 7142 | `** Taken from the SQLite3 source tree.` |
|      - | 7143 | `** Status: Public domain` |
|      - | 7144 | `*/` |
|      - | 7145 | `struct tm *__cdecl localtime(const time_t *t)` |
|      - | 7146 |  |
|      - | 7147 | `  static struct tm y;` |
|      - | 7148 | `  FILETIME uTm, lTm;` |
|      - | 7149 | `  SYSTEMTIME pTm;` |
|      - | 7150 | `  ph7_int64 t64;` |
|      - | 7151 | `  t64 = *t;` |
|      - | 7152 | `  t64 = (t64 + 11644473600)*10000000;` |
|      - | 7153 | `  uTm.dwLowDateTime = (DWORD)(t64 & 0xFFFFFFFF);` |
|      - | 7154 | `  uTm.dwHighDateTime= (DWORD)(t64 >> 32);` |
|      - | 7155 | `  FileTimeToLocalFileTime(&uTm,&lTm);` |
|      - | 7156 | `  FileTimeToSystemTime(&lTm,&pTm);` |
|      - | 7157 | `  y.tm_year = pTm.wYear - 1900;` |
|      - | 7158 | `  y.tm_mon = pTm.wMonth - 1;` |
|      - | 7159 | `  y.tm_wday = pTm.wDayOfWeek;` |
|      - | 7160 | `  y.tm_mday = pTm.wDay;` |
|      - | 7161 | `  y.tm_hour = pTm.wHour;` |
|      - | 7162 | `  y.tm_min = pTm.wMinute;` |
|      - | 7163 | `  y.tm_sec = pTm.wSecond;` |
|      - | 7164 | `  return &y;` |
|      - | 7165 |  |
|      - | 7166 | `#endif /*_WIN32_WCE */` |
|      - | 7167 | `#elif defined(__UNIXES__)` |
|      - | 7168 | `#include <sys/time.h>` |
|      - | 7169 | `#endif /* __WINNT__*/` |
|      - | 7170 | ` /*` |
|      - | 7171 | `  * int64 time(void)` |
|      - | 7172 | `  *  Current Unix timestamp` |
|      - | 7173 | `  * Parameters` |
|      - | 7174 | `  *  None.` |
|      - | 7175 | `  * Return` |
|      - | 7176 | `  *  Returns the current time measured in the number of seconds` |
|      - | 7177 | `  *  since the Unix Epoch (January 1 1970 00:00:00 GMT).` |
|      - | 7178 | `  */` |
|      8 | 7179 | `static int PH7_builtin_time(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7180 |  |
|      - | 7181 | `	time_t tt;` |
|      4 | 7182 | `	SXUNUSED(nArg); /* cc warning */` |
|      4 | 7183 | `	SXUNUSED(apArg);` |
|      - | 7184 | `	/* Extract the current time */` |
|      9 | 7185 | `	time(&tt);` |
|      - | 7186 | `	/* Return as 64-bit integer */` |
|      9 | 7187 | `	ph7_result_int64(pCtx,(ph7_int64)tt);` |
|      9 | 7188 | `	return  PH7_OK;` |
|      1 | 7189 |  |
|      - | 7190 | `/*` |
|      - | 7191 | `  * string/float microtime([ bool $get_as_float = false ])` |
|      - | 7192 | `  *  microtime() returns the current Unix timestamp with microseconds.` |
|      - | 7193 | `  * Parameters` |
|      - | 7194 | `  *  $get_as_float` |
|      - | 7195 | `  *   If used and set to TRUE, microtime() will return a float instead of a string` |
|      - | 7196 | `  *   as described in the return values section below.` |
|      - | 7197 | `  * Return` |
|      - | 7198 | `  *  By default, microtime() returns a string in the form "msec sec", where sec` |
|      - | 7199 | `  *  is the current time measured in the number of seconds since the Unix` |
|      - | 7200 | `  *  epoch (0:00:00 January 1, 1970 GMT), and msec is the number of microseconds` |
|      - | 7201 | `  *  that have elapsed since sec expressed in seconds.` |
|      - | 7202 | `  *  If get_as_float is set to TRUE, then microtime() returns a float, which represents` |
|      - | 7203 | `  *  the current time in seconds since the Unix epoch accurate to the nearest microsecond.` |
|      - | 7204 | `  */` |
|     20 | 7205 | `static int PH7_builtin_microtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7206 |  |
|     21 | 7207 | `	int bFloat = 0;` |
|      - | 7208 | `	sytime sTime;` |
|      - | 7209 | `#if defined(__UNIXES__)` |
|      - | 7210 | `	struct timeval tv;` |
|     20 | 7211 | `	gettimeofday(&tv,0);` |
|     20 | 7212 | `	sTime.tm_sec  = (long)tv.tv_sec;` |
|     20 | 7213 | `	sTime.tm_usec = (long)tv.tv_usec;` |
|      - | 7214 | `#else` |
|      - | 7215 | `	time_t tt;` |
|      1 | 7216 | `	time(&tt);` |
|      1 | 7217 | `	sTime.tm_sec  = (long)tt;` |
|      1 | 7218 | `	sTime.tm_usec = (long)(tt%SX_USEC_PER_SEC);` |
|      - | 7219 | `#endif /* __UNIXES__ */` |
|     21 | 7220 | `	if( nArg > 0 ){` |
|     17 | 7221 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|      8 | 7222 | `	}` |
|     21 | 7223 | `	if( bFloat ){` |
|      - | 7224 | `		/* Return as float */` |
|     17 | 7225 | `		ph7_result_double(pCtx,(double)sTime.tm_sec);` |
|      9 | 7226 | `	}else{` |
|      - | 7227 | `		/* Return as string */` |
|      5 | 7228 | `		ph7_result_string_format(pCtx,"%ld %ld",sTime.tm_usec,sTime.tm_sec);` |
|      - | 7229 | `	}` |
|     21 | 7230 | `	return PH7_OK;` |
|      1 | 7231 |  |
|      - | 7232 | `/*` |
|      - | 7233 | ` * array getdate ([ int $timestamp = time() ])` |
|      - | 7234 | ` *  Get date/time information.` |
|      - | 7235 | ` * Parameter` |
|      - | 7236 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|      - | 7237 | ` *     that defaults to the current local time if a timestamp is not given.` |
|      - | 7238 | ` *     In other words, it defaults to the value of time().` |
|      - | 7239 | ` * Returns` |
|      - | 7240 | ` *  Returns an associative array of information related to the timestamp.` |
|      - | 7241 | ` *  Elements from the returned associative array are as follows:` |
|      - | 7242 | ` *   KEY                                                         VALUE` |
|      - | 7243 | ` * ---------                                                    -------` |
|      - | 7244 | ` * "seconds" 	Numeric representation of seconds 	            0 to 59` |
|      - | 7245 | ` * "minutes" 	Numeric representation of minutes 	            0 to 59` |
|      - | 7246 | ` * "hours" 	    Numeric representation of hours 	            0 to 23` |
|      - | 7247 | ` * "mday" 	    Numeric representation of the day of the month 	1 to 31` |
|      - | 7248 | ` * "wday" 	    Numeric representation of the day of the week 	0 (for Sunday) through 6 (for Saturday)` |
|      - | 7249 | ` * "mon" 	    Numeric representation of a month 	            1 through 12` |
|      - | 7250 | ` * "year" 	    A full numeric representation of a year,        4 digits 	Examples: 1999 or 2003` |
|      - | 7251 | ` * "yday" 	    Numeric representation of the day of the year   0 through 365` |
|      - | 7252 | ` * "weekday" 	A full textual representation of the day of the week 	Sunday through Saturday` |
|      - | 7253 | ` * "month" 	    A full textual representation of a month, such as January or March 	January through December` |
|      - | 7254 | ` * 0 	        Seconds since the Unix Epoch, similar to the values returned by time() and used by date().` |
|      - | 7255 | ` * NOTE:` |
|      - | 7256 | ` *   NULL is returned on failure.` |
|      - | 7257 | ` */` |
|      8 | 7258 | `static int PH7_builtin_getdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7259 |  |
|      - | 7260 | `	ph7_value *pValue,*pArray;` |
|      - | 7261 | `	Sytm sTm;` |
|      9 | 7262 | `	if( nArg < 1 ){` |
|      - | 7263 | `#ifdef __WINNT__` |
|      - | 7264 | `		SYSTEMTIME sOS;` |
|      1 | 7265 | `		GetSystemTime(&sOS);` |
|      1 | 7266 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 7267 | `#else` |
|      - | 7268 | `		struct tm *pTm;` |
|      - | 7269 | `		time_t t;` |
|      4 | 7270 | `		time(&t);` |
|      4 | 7271 | `		pTm = localtime(&t);` |
|      4 | 7272 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7273 | `#endif` |
|      3 | 7274 | `	}else{` |
|      - | 7275 | `		/* Use the given timestamp */` |
|      - | 7276 | `		time_t t;` |
|      - | 7277 | `		struct tm *pTm;` |
|      - | 7278 | `#ifdef __WINNT__` |
|      - | 7279 | `#ifdef _MSC_VER` |
|      - | 7280 | `#if _MSC_VER >= 1400 /* Visual Studio 2005 and up */` |
|      - | 7281 | `#pragma warning(disable:4996) /* _CRT_SECURE...*/` |
|      - | 7282 | `#endif` |
|      - | 7283 | `#endif` |
|      - | 7284 | `#endif` |
|      5 | 7285 | `		if( ph7_value_is_int(apArg[0]) ){` |
|      5 | 7286 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|      5 | 7287 | `			pTm = localtime(&t);` |
|      5 | 7288 | `			if( pTm == 0 ){` |
|    ! 0 | 7289 | `				time(&t);` |
|    ! 0 | 7290 | `			}` |
|      3 | 7291 | `		}else{` |
|    ! 0 | 7292 | `			time(&t);` |
|      - | 7293 | `		}` |
|      5 | 7294 | `		pTm = localtime(&t);` |
|      5 | 7295 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7296 | `	}` |
|      - | 7297 | `	/* Element value */` |
|      9 | 7298 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      9 | 7299 | `	if( pValue == 0 ){` |
|      - | 7300 | `		/* Return NULL */` |
|    ! 0 | 7301 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7302 | `		return PH7_OK;` |
|      - | 7303 | `	}` |
|      - | 7304 | `	/* Create a new array */` |
|      9 | 7305 | `	pArray = ph7_context_new_array(pCtx);` |
|      9 | 7306 | `	if( pArray == 0 ){` |
|      - | 7307 | `		/* Return NULL */` |
|    ! 0 | 7308 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7309 | `		return PH7_OK;` |
|      - | 7310 | `	}` |
|      - | 7311 | `	/* Fill the array */` |
|      - | 7312 | `	/* Seconds */` |
|      9 | 7313 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|      9 | 7314 | `	ph7_array_add_strkey_elem(pArray,"seconds",pValue);` |
|      - | 7315 | `	/* Minutes */` |
|      9 | 7316 | `	ph7_value_int(pValue,sTm.tm_min);` |
|      9 | 7317 | `	ph7_array_add_strkey_elem(pArray,"minutes",pValue);` |
|      - | 7318 | `	/* Hours */` |
|      9 | 7319 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|      9 | 7320 | `	ph7_array_add_strkey_elem(pArray,"hours",pValue);` |
|      - | 7321 | `	/* mday */` |
|      9 | 7322 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|      9 | 7323 | `	ph7_array_add_strkey_elem(pArray,"mday",pValue);` |
|      - | 7324 | `	/* wday */` |
|      9 | 7325 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|      9 | 7326 | `	ph7_array_add_strkey_elem(pArray,"wday",pValue);` |
|      - | 7327 | `	/* mon */` |
|      9 | 7328 | `	ph7_value_int(pValue,sTm.tm_mon+1);` |
|      9 | 7329 | `	ph7_array_add_strkey_elem(pArray,"mon",pValue);` |
|      - | 7330 | `	/* year */` |
|      9 | 7331 | `	ph7_value_int(pValue,sTm.tm_year);` |
|      9 | 7332 | `	ph7_array_add_strkey_elem(pArray,"year",pValue);` |
|      - | 7333 | `	/* yday */` |
|      9 | 7334 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|      9 | 7335 | `	ph7_array_add_strkey_elem(pArray,"yday",pValue);` |
|      - | 7336 | `	/* Weekday */` |
|      9 | 7337 | `	ph7_value_string(pValue,SyTimeGetDay(sTm.tm_wday),-1);` |
|      9 | 7338 | `	ph7_array_add_strkey_elem(pArray,"weekday",pValue);` |
|      - | 7339 | `	/* Month */` |
|      9 | 7340 | `	ph7_value_reset_string_cursor(pValue);` |
|      9 | 7341 | `	ph7_value_string(pValue,SyTimeGetMonth(sTm.tm_mon),-1);` |
|      9 | 7342 | `	ph7_array_add_strkey_elem(pArray,"month",pValue);` |
|      - | 7343 | `	/* Seconds since the epoch */` |
|      9 | 7344 | `	ph7_value_int64(pValue,(ph7_int64)time(0));` |
|      9 | 7345 | `	ph7_array_add_intkey_elem(pArray,0 /* Index zero */,pValue);` |
|      - | 7346 | `	/* Return the freshly created array */` |
|      9 | 7347 | `	ph7_result_value(pCtx,pArray);` |
|      9 | 7348 | `	return PH7_OK;` |
|      5 | 7349 |  |
|      - | 7350 | `/*` |
|      - | 7351 | ` * mixed gettimeofday([ bool $return_float = false ] )` |
|      - | 7352 | ` *   Returns an associative array containing the data returned from the system call.` |
|      - | 7353 | ` * Parameters` |
|      - | 7354 | ` *  $return_float` |
|      - | 7355 | ` *   When set to TRUE, a float instead of an array is returned.` |
|      - | 7356 | ` * Return` |
|      - | 7357 | ` *   By default an array is returned. If return_float is set, then` |
|      - | 7358 | ` *   a float is returned.` |
|      - | 7359 | ` */` |
|      4 | 7360 | `static int PH7_builtin_gettimeofday(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7361 |  |
|      5 | 7362 | `	int bFloat = 0;` |
|      - | 7363 | `	sytime sTime;` |
|      - | 7364 | `#if defined(__UNIXES__)` |
|      - | 7365 | `	struct timeval tv;` |
|      4 | 7366 | `	gettimeofday(&tv,0);` |
|      4 | 7367 | `	sTime.tm_sec  = (long)tv.tv_sec;` |
|      4 | 7368 | `	sTime.tm_usec = (long)tv.tv_usec;` |
|      - | 7369 | `#else` |
|      - | 7370 | `	time_t tt;` |
|      1 | 7371 | `	time(&tt);` |
|      1 | 7372 | `	sTime.tm_sec  = (long)tt;` |
|      1 | 7373 | `	sTime.tm_usec = (long)(tt%SX_USEC_PER_SEC);` |
|      - | 7374 | `#endif /* __UNIXES__ */` |
|      5 | 7375 | `	if( nArg > 0 ){` |
|      5 | 7376 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|      2 | 7377 | `	}` |
|      5 | 7378 | `	if( bFloat ){` |
|      - | 7379 | `		/* Return as float */` |
|      3 | 7380 | `		ph7_result_double(pCtx,(double)sTime.tm_sec);` |
|      2 | 7381 | `	}else{` |
|      - | 7382 | `		/* Return an associative array */` |
|      - | 7383 | `		ph7_value *pValue,*pArray;` |
|      - | 7384 | `		/* Create a new array */` |
|      3 | 7385 | `		pArray = ph7_context_new_array(pCtx);` |
|      - | 7386 | `		/* Element value */` |
|      3 | 7387 | `		pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 7388 | `		if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 7389 | `			/* Return NULL */` |
|    ! 0 | 7390 | `			ph7_result_null(pCtx);` |
|    ! 0 | 7391 | `			return PH7_OK;` |
|      - | 7392 | `		}` |
|      - | 7393 | `		/* Fill the array */` |
|      - | 7394 | `		/* sec */` |
|      3 | 7395 | `		ph7_value_int64(pValue,sTime.tm_sec);` |
|      3 | 7396 | `		ph7_array_add_strkey_elem(pArray,"sec",pValue);` |
|      - | 7397 | `		/* usec */` |
|      3 | 7398 | `		ph7_value_int64(pValue,sTime.tm_usec);` |
|      3 | 7399 | `		ph7_array_add_strkey_elem(pArray,"usec",pValue);` |
|      - | 7400 | `		/* Return the array */` |
|      3 | 7401 | `		ph7_result_value(pCtx,pArray);` |
|      - | 7402 | `	}` |
|      5 | 7403 | `	return PH7_OK;` |
|      3 | 7404 |  |
|      - | 7405 | `/* Check if the given year is leap or not */` |
|      - | 7406 | `#define IS_LEAP_YEAR(YEAR)	(YEAR % 400 ? ( YEAR % 100 ? ( YEAR % 4 ? 0 : 1 ) : 0 ) : 1)` |
|      - | 7407 | `/* ISO-8601 numeric representation of the day of the week */` |
|      - | 7408 | `static const int aISO8601[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|      - | 7409 | `/*` |
|      - | 7410 | ` * Format a given date string.` |
|      - | 7411 | ` * Supported format: (Taken from PHP online docs)` |
|      - | 7412 | ` * character 	Description` |
|      - | 7413 | ` * d          Day of the month` |
|      - | 7414 | ` * D          A textual representation of a days` |
|      - | 7415 | ` * j          Day of the month without leading zeros` |
|      - | 7416 | ` * l          A full textual representation of the day of the week` |
|      - | 7417 | ` * N          ISO-8601 numeric representation of the day of the week` |
|      - | 7418 | ` * w          Numeric representation of the day of the week` |
|      - | 7419 | ` * z          The day of the year (starting from 0)` |
|      - | 7420 | ` * F          A full textual representation of a month, such as January or March` |
|      - | 7421 | ` * m          Numeric representation of a month, with leading zeros 	01 through 12` |
|      - | 7422 | ` * M          A short textual representation of a month, three letters 	Jan through Dec` |
|      - | 7423 | ` * n          Numeric representation of a month, without leading zeros 	1 through 12` |
|      - | 7424 | ` * t          Number of days in the given month 	28 through 31` |
|      - | 7425 | ` * L          Whether it's a leap year 	1 if it is a leap year, 0 otherwise.` |
|      - | 7426 | ` * o          ISO-8601 year number. This has the same value as Y, except that if the ISO week number` |
|      - | 7427 | ` *            (W) belongs to the previous or next year, that year is used instead. (added in PHP 5.1.0) Examples: 1999 or 2003` |
|      - | 7428 | ` * Y          A full numeric representation of a year, 4 digits 	Examples: 1999 or 2003` |
|      - | 7429 | ` * y          A two digit representation of a year 	Examples: 99 or 03` |
|      - | 7430 | ` * a          Lowercase Ante meridiem and Post meridiem 	am or pm` |
|      - | 7431 | ` * A          Uppercase Ante meridiem and Post meridiem 	AM or PM` |
|      - | 7432 | ` * g          12-hour format of an hour without leading zeros 	1 through 12` |
|      - | 7433 | ` * G          24-hour format of an hour without leading zeros 	0 through 23` |
|      - | 7434 | ` * h          12-hour format of an hour with leading zeros 	01 through 12` |
|      - | 7435 | ` * H          24-hour format of an hour with leading zeros 	00 through 23` |
|      - | 7436 | ` * i          Minutes with leading zeros 	00 to 59` |
|      - | 7437 | ` * s          Seconds, with leading zeros 	00 through 59` |
|      - | 7438 | ` * u          Microseconds Example: 654321` |
|      - | 7439 | ` * e          Timezone identifier 	Examples: UTC, GMT, Atlantic/Azores` |
|      - | 7440 | ` * I          (capital i) Whether or not the date is in daylight saving time 	1 if Daylight Saving Time, 0 otherwise.` |
|      - | 7441 | ` * r          RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 +0200` |
|      - | 7442 | ` * U          Seconds since the Unix Epoch (January 1 1970 00:00:00 GMT)` |
|      - | 7443 | ` * S          English ordinal suffix for the day of the month, 2 characters` |
|      - | 7444 | ` * O          Difference to Greenwich time (GMT) in hours` |
|      - | 7445 | ` * Z          Timezone offset in seconds. The offset for timezones west of UTC is always negative, and for those` |
|      - | 7446 | ` *            east of UTC is always positive.` |
|      - | 7447 | ` * c         ISO 8601 date` |
|      - | 7448 | ` */` |
|     46 | 7449 | `static sxi32 DateFormat(ph7_context *pCtx,const char *zIn,int nLen,Sytm *pTm)` |
|      1 | 7450 |  |
|     47 | 7451 | `	const char *zEnd = &zIn[nLen];` |
|      - | 7452 | `	const char *zCur;` |
|      - | 7453 | `	/* Start the format process */` |
|     78 | 7454 | `	for(;;){` |
|    157 | 7455 | `		if( zIn >= zEnd ){` |
|      - | 7456 | `			/* No more input to process */` |
|     47 | 7457 | `			break;` |
|      - | 7458 | `		}` |
|    111 | 7459 | `		switch(zIn[0]){` |
|      7 | 7460 | `		case 'd':` |
|      - | 7461 | `			/* Day of the month, 2 digits with leading zeros */` |
|     15 | 7462 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mday);` |
|     15 | 7463 | `			break;` |
|    ! 0 | 7464 | `		case 'D':` |
|      - | 7465 | `			/*A textual representation of a day, three letters*/` |
|    ! 0 | 7466 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|    ! 0 | 7467 | `			ph7_result_string(pCtx,zCur,3);` |
|    ! 0 | 7468 | `			break;` |
|    ! 0 | 7469 | `		case 'j':` |
|      - | 7470 | `			/*	Day of the month without leading zeros */` |
|    ! 0 | 7471 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mday);` |
|    ! 0 | 7472 | `			break;` |
|      2 | 7473 | `		case 'l':` |
|      - | 7474 | `			/* A full textual representation of the day of the week */` |
|      5 | 7475 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|      5 | 7476 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|      5 | 7477 | `			break;` |
|    ! 0 | 7478 | `		case 'N':{` |
|      - | 7479 | `			/* ISO-8601 numeric representation of the day of the week */` |
|    ! 0 | 7480 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|    ! 0 | 7481 | `			break;` |
|      - | 7482 | `				 }` |
|    ! 0 | 7483 | `		case 'w':` |
|      - | 7484 | `			/*Numeric representation of the day of the week*/` |
|    ! 0 | 7485 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|    ! 0 | 7486 | `			break;` |
|    ! 0 | 7487 | `		case 'z':` |
|      - | 7488 | `			/*The day of the year*/` |
|    ! 0 | 7489 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_yday);` |
|    ! 0 | 7490 | `			break;` |
|      2 | 7491 | `		case 'F':` |
|      - | 7492 | `			/*A full textual representation of a month, such as January or March*/` |
|      5 | 7493 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|      5 | 7494 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|      5 | 7495 | `			break;` |
|      7 | 7496 | `		case 'm':` |
|      - | 7497 | `			/*Numeric representation of a month, with leading zeros*/` |
|     15 | 7498 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|     15 | 7499 | `			break;` |
|    ! 0 | 7500 | `		case 'M':` |
|      - | 7501 | `			/*A short textual representation of a month, three letters*/` |
|    ! 0 | 7502 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|    ! 0 | 7503 | `			ph7_result_string(pCtx,zCur,3);` |
|    ! 0 | 7504 | `			break;` |
|    ! 0 | 7505 | `		case 'n':` |
|      - | 7506 | `			/*Numeric representation of a month, without leading zeros*/` |
|    ! 0 | 7507 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mon + 1);` |
|    ! 0 | 7508 | `			break;` |
|    ! 0 | 7509 | `		case 't':{` |
|      - | 7510 | `			static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|    ! 0 | 7511 | `			int nDays = aMonDays[pTm->tm_mon % 12 ];` |
|    ! 0 | 7512 | `			if( pTm->tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(pTm->tm_year) ){` |
|    ! 0 | 7513 | `				nDays = 28;` |
|    ! 0 | 7514 | `			}` |
|      - | 7515 | `			/*Number of days in the given month*/` |
|    ! 0 | 7516 | `			ph7_result_string_format(pCtx,"%d",nDays);` |
|    ! 0 | 7517 | `			break;` |
|      - | 7518 | `				 }` |
|    ! 0 | 7519 | `		case 'L':{` |
|    ! 0 | 7520 | `			int isLeap = IS_LEAP_YEAR(pTm->tm_year);` |
|      - | 7521 | `			/* Whether it's a leap year */` |
|    ! 0 | 7522 | `			ph7_result_string_format(pCtx,"%d",isLeap);` |
|    ! 0 | 7523 | `			break;` |
|      - | 7524 | `				 }` |
|    ! 0 | 7525 | `		case 'o':` |
|      - | 7526 | `			/* ISO-8601 year number.*/` |
|    ! 0 | 7527 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|    ! 0 | 7528 | `			break;` |
|      9 | 7529 | `		case 'Y':` |
|      - | 7530 | `			/*	A full numeric representation of a year, 4 digits */` |
|     19 | 7531 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|     19 | 7532 | `			break;` |
|    ! 0 | 7533 | `		case 'y':` |
|      - | 7534 | `			/*A two digit representation of a year*/` |
|    ! 0 | 7535 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_year%100);` |
|    ! 0 | 7536 | `			break;` |
|    ! 0 | 7537 | `		case 'a':` |
|      - | 7538 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7539 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",2);` |
|    ! 0 | 7540 | `			break;` |
|    ! 0 | 7541 | `		case 'A':` |
|      - | 7542 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7543 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",2);` |
|    ! 0 | 7544 | `			break;` |
|    ! 0 | 7545 | `		case 'g':` |
|      - | 7546 | `			/*	12-hour format of an hour without leading zeros*/` |
|    ! 0 | 7547 | `			ph7_result_string_format(pCtx,"%d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7548 | `			break;` |
|    ! 0 | 7549 | `		case 'G':` |
|      - | 7550 | `			/* 24-hour format of an hour without leading zeros */` |
|    ! 0 | 7551 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_hour);` |
|    ! 0 | 7552 | `			break;` |
|    ! 0 | 7553 | `		case 'h':` |
|      - | 7554 | `			/* 12-hour format of an hour with leading zeros */` |
|    ! 0 | 7555 | `			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7556 | `			break;` |
|      3 | 7557 | `		case 'H':` |
|      - | 7558 | `			/*	24-hour format of an hour with leading zeros */` |
|      7 | 7559 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|      7 | 7560 | `			break;` |
|      3 | 7561 | `		case 'i':` |
|      - | 7562 | `			/* 	Minutes with leading zeros */` |
|      7 | 7563 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|      7 | 7564 | `			break;` |
|      3 | 7565 | `		case 's':` |
|      - | 7566 | `			/* 	second with leading zeros */` |
|      7 | 7567 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|      7 | 7568 | `			break;` |
|    ! 0 | 7569 | `		case 'u':` |
|      - | 7570 | `			/* 	Microseconds */` |
|    ! 0 | 7571 | `			ph7_result_string_format(pCtx,"%u",pTm->tm_sec * SX_USEC_PER_SEC);` |
|    ! 0 | 7572 | `			break;` |
|    ! 0 | 7573 | `		case 'S':{` |
|      - | 7574 | `			/* English ordinal suffix for the day of the month, 2 characters */` |
|      - | 7575 | `			static const char zSuffix[] = "thstndrdthththththth";` |
|    ! 0 | 7576 | `			int v = pTm->tm_mday;` |
|    ! 0 | 7577 | `			ph7_result_string(pCtx,&zSuffix[2 * (int)(v / 10 % 10 != 1 ? v % 10 : 0)],(int)sizeof(char) * 2);` |
|    ! 0 | 7578 | `			break;` |
|      - | 7579 | `				 }` |
|    ! 0 | 7580 | `		case 'e':` |
|      - | 7581 | `			/* 	Timezone identifier */` |
|    ! 0 | 7582 | `			zCur = pTm->tm_zone;` |
|    ! 0 | 7583 | `			if( zCur == 0 ){` |
|      - | 7584 | `				/* Assume GMT */` |
|    ! 0 | 7585 | `				zCur = "GMT";` |
|    ! 0 | 7586 | `			}` |
|    ! 0 | 7587 | `			ph7_result_string(pCtx,zCur,-1);` |
|    ! 0 | 7588 | `			break;` |
|    ! 0 | 7589 | `		case 'I':` |
|      - | 7590 | `			/* Whether or not the date is in daylight saving time */` |
|      - | 7591 | `#ifdef __WINNT__` |
|      - | 7592 | `#ifdef _MSC_VER` |
|      - | 7593 | `#ifndef _WIN32_WCE` |
|    ! 0 | 7594 | `			_get_daylight(&pTm->tm_isdst);` |
|      - | 7595 | `#endif` |
|      - | 7596 | `#endif` |
|      - | 7597 | `#endif` |
|    ! 0 | 7598 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_isdst == 1);` |
|    ! 0 | 7599 | `			break;` |
|    ! 0 | 7600 | `		case 'r':` |
|      - | 7601 | `			/* RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 */` |
|    ! 0 | 7602 | `			ph7_result_string_format(pCtx,"%.3s, %02d %.3s %4d %02d:%02d:%02d",` |
|    ! 0 | 7603 | `				SyTimeGetDay(pTm->tm_wday),` |
|    ! 0 | 7604 | `				pTm->tm_mday,` |
|    ! 0 | 7605 | `				SyTimeGetMonth(pTm->tm_mon),` |
|    ! 0 | 7606 | `				pTm->tm_year,` |
|    ! 0 | 7607 | `				pTm->tm_hour,` |
|    ! 0 | 7608 | `				pTm->tm_min,` |
|    ! 0 | 7609 | `				pTm->tm_sec` |
|      - | 7610 | `				);` |
|    ! 0 | 7611 | `			break;` |
|    ! 0 | 7612 | `		case 'U':{` |
|      - | 7613 | `			time_t tt;` |
|      - | 7614 | `			/* Seconds since the Unix Epoch */` |
|    ! 0 | 7615 | `			time(&tt);` |
|    ! 0 | 7616 | `			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);` |
|    ! 0 | 7617 | `			break;` |
|      - | 7618 | `				 }` |
|    ! 0 | 7619 | `		case 'O':` |
|      - | 7620 | `		case 'P':` |
|      - | 7621 | `			/* Difference to Greenwich time (GMT) in hours */` |
|    ! 0 | 7622 | `			ph7_result_string_format(pCtx,"%+05d",pTm->tm_gmtoff);` |
|    ! 0 | 7623 | `			break;` |
|    ! 0 | 7624 | `		case 'Z':` |
|      - | 7625 | `			/* Timezone offset in seconds. The offset for timezones west of UTC` |
|      - | 7626 | `			 * is always negative, and for those east of UTC is always positive.` |
|      - | 7627 | `			 */` |
|    ! 0 | 7628 | `			ph7_result_string_format(pCtx,"%+05d",pTm->tm_gmtoff);` |
|    ! 0 | 7629 | `			break;` |
|      1 | 7630 | `		case 'c':` |
|      - | 7631 | `			/* 	ISO 8601 date */` |
|      4 | 7632 | `			ph7_result_string_format(pCtx,"%4d-%02d-%02dT%02d:%02d:%02d%+05d",` |
|      1 | 7633 | `				pTm->tm_year,` |
|      2 | 7634 | `				pTm->tm_mon+1,` |
|      1 | 7635 | `				pTm->tm_mday,` |
|      1 | 7636 | `				pTm->tm_hour,` |
|      1 | 7637 | `				pTm->tm_min,` |
|      1 | 7638 | `				pTm->tm_sec,` |
|      1 | 7639 | `				pTm->tm_gmtoff` |
|      - | 7640 | `				);` |
|      3 | 7641 | `			break;` |
|      1 | 7642 | `		case '\\':` |
|      3 | 7643 | `			zIn++;` |
|      - | 7644 | `			/* Expand verbatim */` |
|      3 | 7645 | `			if( zIn < zEnd ){` |
|      3 | 7646 | `				ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|      1 | 7647 | `			}` |
|      3 | 7648 | `			break;` |
|     17 | 7649 | `		default:` |
|      - | 7650 | `			/* Unknown format specifer,expand verbatim */` |
|     35 | 7651 | `			ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|     34 | 7652 | `			break;` |
|      - | 7653 | `		}` |
|      - | 7654 | `		/* Point to the next character */` |
|    111 | 7655 | `		zIn++;` |
|      1 | 7656 | `	}` |
|     47 | 7657 | `	return SXRET_OK;` |
|      1 | 7658 |  |
|      - | 7659 | `/*` |
|      - | 7660 | ` * PH7 implementation of the strftime() function.` |
|      - | 7661 | ` * The following formats are supported:` |
|      - | 7662 | ` * %a 	An abbreviated textual representation of the day` |
|      - | 7663 | ` * %A 	A full textual representation of the day` |
|      - | 7664 | ` * %d 	Two-digit day of the month (with leading zeros)` |
|      - | 7665 | ` * %e 	Day of the month, with a space preceding single digits.` |
|      - | 7666 | ` * %j 	Day of the year, 3 digits with leading zeros` |
|      - | 7667 | ` * %u 	ISO-8601 numeric representation of the day of the week 	1 (for Monday) though 7 (for Sunday)` |
|      - | 7668 | ` * %w 	Numeric representation of the day of the week 0 (for Sunday) through 6 (for Saturday)` |
|      - | 7669 | ` * %U 	Week number of the given year, starting with the first Sunday as the first week` |
|      - | 7670 | ` * %V 	ISO-8601:1988 week number of the given year, starting with the first week of the year with at least` |
|      - | 7671 | ` *   4 weekdays, with Monday being the start of the week.` |
|      - | 7672 | ` * %W 	A numeric representation of the week of the year` |
|      - | 7673 | ` * %b 	Abbreviated month name, based on the locale` |
|      - | 7674 | ` * %B 	Full month name, based on the locale` |
|      - | 7675 | ` * %h 	Abbreviated month name, based on the locale (an alias of %b)` |
|      - | 7676 | ` * %m 	Two digit representation of the month` |
|      - | 7677 | ` * %C 	Two digit representation of the century (year divided by 100, truncated to an integer)` |
|      - | 7678 | ` * %g 	Two digit representation of the year going by ISO-8601:1988 standards (see %V)` |
|      - | 7679 | ` * %G 	The full four-digit version of %g` |
|      - | 7680 | ` * %y 	Two digit representation of the year` |
|      - | 7681 | ` * %Y 	Four digit representation for the year` |
|      - | 7682 | ` * %H 	Two digit representation of the hour in 24-hour format` |
|      - | 7683 | ` * %I 	Two digit representation of the hour in 12-hour format` |
|      - | 7684 | ` * %l (lower-case 'L') 	Hour in 12-hour format, with a space preceeding single digits` |
|      - | 7685 | ` * %M 	Two digit representation of the minute` |
|      - | 7686 | ` * %p 	UPPER-CASE 'AM' or 'PM' based on the given time` |
|      - | 7687 | ` * %P 	lower-case 'am' or 'pm' based on the given time` |
|      - | 7688 | ` * %r 	Same as "%I:%M:%S %p"` |
|      - | 7689 | ` * %R 	Same as "%H:%M"` |
|      - | 7690 | ` * %S 	Two digit representation of the second` |
|      - | 7691 | ` * %T 	Same as "%H:%M:%S"` |
|      - | 7692 | ` * %X 	Preferred time representation based on locale, without the date` |
|      - | 7693 | ` * %z 	Either the time zone offset from UTC or the abbreviation` |
|      - | 7694 | ` * %Z 	The time zone offset/abbreviation option NOT given by %z` |
|      - | 7695 | ` * %c 	Preferred date and time stamp based on local` |
|      - | 7696 | ` * %D 	Same as "%m/%d/%y"` |
|      - | 7697 | ` * %F 	Same as "%Y-%m-%d"` |
|      - | 7698 | ` * %s 	Unix Epoch Time timestamp (same as the time() function)` |
|      - | 7699 | ` * %x 	Preferred date representation based on locale, without the time` |
|      - | 7700 | ` * %n 	A newline character ("\n")` |
|      - | 7701 | ` * %t 	A Tab character ("\t")` |
|      - | 7702 | ` * %% 	A literal percentage character ("%")` |
|      - | 7703 | ` */` |
|     16 | 7704 | `static int PH7_Strftime(` |
|      - | 7705 | `	ph7_context *pCtx,  /* Call context */` |
|      - | 7706 | `	const char *zIn,    /* Input string */` |
|      - | 7707 | `	int nLen,           /* Input length */` |
|      - | 7708 | `	Sytm *pTm           /* Parse of the given time */` |
|      - | 7709 | `	)` |
|      1 | 7710 |  |
|     17 | 7711 | `	const char *zCur,*zEnd = &zIn[nLen];` |
|      - | 7712 | `	int c;` |
|      - | 7713 | `	/* Start the format process */` |
|     18 | 7714 | `	for(;;){` |
|     37 | 7715 | `		zCur = zIn;` |
|     41 | 7716 | `		while(zIn < zEnd && zIn[0] != '%' ){` |
|      5 | 7717 | `			zIn++;` |
|      1 | 7718 | `		}` |
|     37 | 7719 | `		if( zIn > zCur ){` |
|      - | 7720 | `			/* Consume input verbatim */` |
|      5 | 7721 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 7722 | `		}` |
|     37 | 7723 | `		zIn++; /* Jump the percent sign */` |
|     37 | 7724 | `		if( zIn >= zEnd ){` |
|      - | 7725 | `			/* No more input to process */` |
|     17 | 7726 | `			break;` |
|      - | 7727 | `		}` |
|     21 | 7728 | `		c = zIn[0];` |
|      - | 7729 | `		/* Act according to the current specifer */` |
|     21 | 7730 | `		switch(c){` |
|    ! 0 | 7731 | `		case '%':` |
|      - | 7732 | `			/* A literal percentage character ("%") */` |
|    ! 0 | 7733 | `			ph7_result_string(pCtx,"%",(int)sizeof(char));` |
|    ! 0 | 7734 | `			break;` |
|    ! 0 | 7735 | `		case 't':` |
|      - | 7736 | `			/* A Tab character */` |
|    ! 0 | 7737 | `			ph7_result_string(pCtx,"\t",(int)sizeof(char));` |
|    ! 0 | 7738 | `			break;` |
|    ! 0 | 7739 | `		case 'n':` |
|      - | 7740 | `			/* A newline character */` |
|    ! 0 | 7741 | `			ph7_result_string(pCtx,"\n",(int)sizeof(char));` |
|    ! 0 | 7742 | `			break;` |
|      1 | 7743 | `		case 'a':` |
|      - | 7744 | `			/* An abbreviated textual representation of the day */` |
|      3 | 7745 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),(int)sizeof(char)*3);` |
|      3 | 7746 | `			break;` |
|    ! 0 | 7747 | `		case 'A':` |
|      - | 7748 | `			/* A full textual representation of the day */` |
|    ! 0 | 7749 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),-1/*Compute length automatically*/);` |
|    ! 0 | 7750 | `			break;` |
|    ! 0 | 7751 | `		case 'e':` |
|      - | 7752 | `			/* Day of the month, 2 digits with leading space for single digit*/` |
|    ! 0 | 7753 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_mday);` |
|    ! 0 | 7754 | `			break;` |
|      2 | 7755 | `		case 'd':` |
|      - | 7756 | `			/* Two-digit day of the month (with leading zeros) */` |
|      5 | 7757 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon+1);` |
|      5 | 7758 | `			break;` |
|    ! 0 | 7759 | `		case 'j':` |
|      - | 7760 | `			/*The day of the year,3 digits with leading zeros*/` |
|    ! 0 | 7761 | `			ph7_result_string_format(pCtx,"%03d",pTm->tm_yday);` |
|    ! 0 | 7762 | `			break;` |
|    ! 0 | 7763 | `		case 'u':` |
|      - | 7764 | `			/* ISO-8601 numeric representation of the day of the week */` |
|    ! 0 | 7765 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|    ! 0 | 7766 | `			break;` |
|    ! 0 | 7767 | `		case 'w':` |
|      - | 7768 | `			/* Numeric representation of the day of the week */` |
|    ! 0 | 7769 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|    ! 0 | 7770 | `			break;` |
|    ! 0 | 7771 | `		case 'b':` |
|      - | 7772 | `		case 'h':` |
|      - | 7773 | `			/*A short textual representation of a month, three letters (Not based on locale)*/` |
|    ! 0 | 7774 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),(int)sizeof(char)*3);` |
|    ! 0 | 7775 | `			break;` |
|    ! 0 | 7776 | `		case 'B':` |
|      - | 7777 | `			/* Full month name (Not based on locale) */` |
|    ! 0 | 7778 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),-1/*Compute length automatically*/);` |
|    ! 0 | 7779 | `			break;` |
|      2 | 7780 | `		case 'm':` |
|      - | 7781 | `			/*Numeric representation of a month, with leading zeros*/` |
|      5 | 7782 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|      5 | 7783 | `			break;` |
|    ! 0 | 7784 | `		case 'C':` |
|      - | 7785 | `			/* Two digit representation of the century */` |
|    ! 0 | 7786 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year/100);` |
|    ! 0 | 7787 | `			break;` |
|    ! 0 | 7788 | `		case 'y':` |
|      - | 7789 | `		case 'g':` |
|      - | 7790 | `			/* Two digit representation of the year */` |
|    ! 0 | 7791 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year%100);` |
|    ! 0 | 7792 | `			break;` |
|      2 | 7793 | `		case 'Y':` |
|      - | 7794 | `		case 'G':` |
|      - | 7795 | `			/* Four digit representation of the year */` |
|      5 | 7796 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|      5 | 7797 | `			break;` |
|    ! 0 | 7798 | `		case 'I':` |
|      - | 7799 | `			/* 12-hour format of an hour with leading zeros */` |
|    ! 0 | 7800 | `			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7801 | `			break;` |
|    ! 0 | 7802 | `		case 'l':` |
|      - | 7803 | `			/* 12-hour format of an hour with leading space */` |
|    ! 0 | 7804 | `			ph7_result_string_format(pCtx,"%2d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7805 | `			break;` |
|      1 | 7806 | `		case 'H':` |
|      - | 7807 | `			/* 24-hour format of an hour with leading zeros */` |
|      3 | 7808 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|      3 | 7809 | `			break;` |
|      1 | 7810 | `		case 'M':` |
|      - | 7811 | `			/* Minutes with leading zeros */` |
|      3 | 7812 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|      3 | 7813 | `			break;` |
|    ! 0 | 7814 | `		case 'S':` |
|      - | 7815 | `			/* Seconds with leading zeros */` |
|    ! 0 | 7816 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|    ! 0 | 7817 | `			break;` |
|    ! 0 | 7818 | `		case 'z':` |
|      - | 7819 | `		case 'Z':` |
|      - | 7820 | `			/* 	Timezone identifier */` |
|    ! 0 | 7821 | `			zCur = pTm->tm_zone;` |
|    ! 0 | 7822 | `			if( zCur == 0 ){` |
|      - | 7823 | `				/* Assume GMT */` |
|    ! 0 | 7824 | `				zCur = "GMT";` |
|    ! 0 | 7825 | `			}` |
|    ! 0 | 7826 | `			ph7_result_string(pCtx,zCur,-1);` |
|    ! 0 | 7827 | `			break;` |
|    ! 0 | 7828 | `		case 'T':` |
|      - | 7829 | `		case 'X':` |
|      - | 7830 | `			/* Same as "%H:%M:%S" */` |
|    ! 0 | 7831 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d",pTm->tm_hour,pTm->tm_min,pTm->tm_sec);` |
|    ! 0 | 7832 | `			break;` |
|    ! 0 | 7833 | `		case 'R':` |
|      - | 7834 | `			/* Same as "%H:%M" */` |
|    ! 0 | 7835 | `			ph7_result_string_format(pCtx,"%02d:%02d",pTm->tm_hour,pTm->tm_min);` |
|    ! 0 | 7836 | `			break;` |
|    ! 0 | 7837 | `		case 'P':` |
|      - | 7838 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7839 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",(int)sizeof(char)*2);` |
|    ! 0 | 7840 | `			break;` |
|    ! 0 | 7841 | `		case 'p':` |
|      - | 7842 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7843 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",(int)sizeof(char)*2);` |
|    ! 0 | 7844 | `			break;` |
|    ! 0 | 7845 | `		case 'r':` |
|      - | 7846 | `			/* Same as "%I:%M:%S %p" */` |
|    ! 0 | 7847 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d %s",` |
|    ! 0 | 7848 | `				1+(pTm->tm_hour%12),` |
|    ! 0 | 7849 | `				pTm->tm_min,` |
|    ! 0 | 7850 | `				pTm->tm_sec,` |
|    ! 0 | 7851 | `				pTm->tm_hour > 12 ? "PM" : "AM"` |
|      - | 7852 | `				);` |
|    ! 0 | 7853 | `			break;` |
|      1 | 7854 | `		case 'D':` |
|      - | 7855 | `		case 'x':` |
|      - | 7856 | `			/* Same as "%m/%d/%y" */` |
|      4 | 7857 | `			ph7_result_string_format(pCtx,"%02d/%02d/%02d",` |
|      2 | 7858 | `				pTm->tm_mon+1,` |
|      1 | 7859 | `				pTm->tm_mday,` |
|      2 | 7860 | `				pTm->tm_year%100` |
|      - | 7861 | `				);` |
|      3 | 7862 | `			break;` |
|    ! 0 | 7863 | `		case 'F':` |
|      - | 7864 | `			/* Same as "%Y-%m-%d" */` |
|    ! 0 | 7865 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d",` |
|    ! 0 | 7866 | `				pTm->tm_year,` |
|    ! 0 | 7867 | `				pTm->tm_mon+1,` |
|    ! 0 | 7868 | `				pTm->tm_mday` |
|      - | 7869 | `				);` |
|    ! 0 | 7870 | `			break;` |
|    ! 0 | 7871 | `		case 'c':` |
|    ! 0 | 7872 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d %02d:%02d:%02d",` |
|    ! 0 | 7873 | `				pTm->tm_year,` |
|    ! 0 | 7874 | `				pTm->tm_mon+1,` |
|    ! 0 | 7875 | `				pTm->tm_mday,` |
|    ! 0 | 7876 | `				pTm->tm_hour,` |
|    ! 0 | 7877 | `				pTm->tm_min,` |
|    ! 0 | 7878 | `				pTm->tm_sec` |
|      - | 7879 | `				);` |
|    ! 0 | 7880 | `			break;` |
|    ! 0 | 7881 | `		case 's':{` |
|      - | 7882 | `			time_t tt;` |
|      - | 7883 | `			/* Seconds since the Unix Epoch */` |
|    ! 0 | 7884 | `			time(&tt);` |
|    ! 0 | 7885 | `			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);` |
|    ! 0 | 7886 | `			break;` |
|      - | 7887 | `				 }` |
|    ! 0 | 7888 | `		default:` |
|      - | 7889 | `			/* unknown specifer,simply ignore*/` |
|    ! 0 | 7890 | `			break;` |
|      - | 7891 | `		}` |
|      - | 7892 | `		/* Advance the cursor */` |
|     21 | 7893 | `		zIn++;` |
|      1 | 7894 | `	}` |
|     17 | 7895 | `	return SXRET_OK;` |
|      1 | 7896 |  |
|      - | 7897 | `/*` |
|      - | 7898 | ` * string date(string $format [, int $timestamp = time() ] )` |
|      - | 7899 | ` *  Returns a string formatted according to the given format string using` |
|      - | 7900 | ` *  the given integer timestamp or the current time if no timestamp is given.` |
|      - | 7901 | ` *  In other words, timestamp is optional and defaults to the value of time().` |
|      - | 7902 | ` * Parameters` |
|      - | 7903 | ` *  $format` |
|      - | 7904 | ` *   The format of the outputted date string (See code above)` |
|      - | 7905 | ` * $timestamp` |
|      - | 7906 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|      - | 7907 | ` *   that defaults to the current local time if a timestamp is not given.` |
|      - | 7908 | ` *   In other words, it defaults to the value of time().` |
|      - | 7909 | ` * Return` |
|      - | 7910 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|      - | 7911 | ` */` |
|     36 | 7912 | `static int PH7_builtin_date(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7913 |  |
|      - | 7914 | `	const char *zFormat;` |
|      - | 7915 | `	int nLen;` |
|      - | 7916 | `	Sytm sTm;` |
|     37 | 7917 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7918 | `		/* Missing/Invalid argument,return FALSE */` |
|      5 | 7919 | `		ph7_result_bool(pCtx,0);` |
|      5 | 7920 | `		return PH7_OK;` |
|      - | 7921 | `	}` |
|     33 | 7922 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     33 | 7923 | `	if( nLen < 1 ){` |
|      - | 7924 | `		/* Don't bother processing return the empty string */` |
|    ! 0 | 7925 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7926 | `	}` |
|     33 | 7927 | `	if( nArg < 2 ){` |
|      - | 7928 | `#ifdef __WINNT__` |
|      - | 7929 | `		SYSTEMTIME sOS;` |
|      1 | 7930 | `		GetSystemTime(&sOS);` |
|      1 | 7931 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 7932 | `#else` |
|      - | 7933 | `		struct tm *pTm;` |
|      - | 7934 | `		time_t t;` |
|     30 | 7935 | `		time(&t);` |
|     30 | 7936 | `		pTm = localtime(&t);` |
|     30 | 7937 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7938 | `#endif` |
|     16 | 7939 | `	}else{` |
|      - | 7940 | `		/* Use the given timestamp */` |
|      - | 7941 | `		time_t t;` |
|      - | 7942 | `		struct tm *pTm;` |
|      3 | 7943 | `		if( ph7_value_is_int(apArg[1]) ){` |
|      3 | 7944 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|      3 | 7945 | `			pTm = localtime(&t);` |
|      3 | 7946 | `			if( pTm == 0 ){` |
|    ! 0 | 7947 | `				time(&t);` |
|    ! 0 | 7948 | `			}` |
|      2 | 7949 | `		}else{` |
|    ! 0 | 7950 | `			time(&t);` |
|      - | 7951 | `		}` |
|      3 | 7952 | `		pTm = localtime(&t);` |
|      3 | 7953 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7954 | `	}` |
|      - | 7955 | `	/* Format the given string */` |
|     33 | 7956 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|     33 | 7957 | `	return PH7_OK;` |
|     19 | 7958 |  |
|      - | 7959 | `/*` |
|      - | 7960 | ` * string strftime(string $format [, int $timestamp = time() ] )` |
|      - | 7961 | ` *  Format a local time/date (PLATFORM INDEPENDANT IMPLEENTATION NOT BASED ON LOCALE)` |
|      - | 7962 | ` * Parameters` |
|      - | 7963 | ` *  $format` |
|      - | 7964 | ` *   The format of the outputted date string (See code above)` |
|      - | 7965 | ` * $timestamp` |
|      - | 7966 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|      - | 7967 | ` *   that defaults to the current local time if a timestamp is not given.` |
|      - | 7968 | ` *   In other words, it defaults to the value of time().` |
|      - | 7969 | ` * Return` |
|      - | 7970 | ` * Returns a string formatted according format using the given timestamp` |
|      - | 7971 | ` * or the current local time if no timestamp is given.` |
|      - | 7972 | ` */` |
|     20 | 7973 | `static int PH7_builtin_strftime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7974 |  |
|      - | 7975 | `	const char *zFormat;` |
|      - | 7976 | `	int nLen;` |
|      - | 7977 | `	Sytm sTm;` |
|     21 | 7978 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7979 | `		/* Missing/Invalid argument,return FALSE */` |
|      5 | 7980 | `		ph7_result_bool(pCtx,0);` |
|      5 | 7981 | `		return PH7_OK;` |
|      - | 7982 | `	}` |
|     17 | 7983 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7984 | `	if( nLen < 1 ){` |
|      - | 7985 | `		/* Don't bother processing return FALSE */` |
|    ! 0 | 7986 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7987 | `	}` |
|     17 | 7988 | `	if( nArg < 2 ){` |
|      - | 7989 | `#ifdef __WINNT__` |
|      - | 7990 | `		SYSTEMTIME sOS;` |
|      1 | 7991 | `		GetSystemTime(&sOS);` |
|      1 | 7992 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 7993 | `#else` |
|      - | 7994 | `		struct tm *pTm;` |
|      - | 7995 | `		time_t t;` |
|     14 | 7996 | `		time(&t);` |
|     14 | 7997 | `		pTm = localtime(&t);` |
|     14 | 7998 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7999 | `#endif` |
|      8 | 8000 | `	}else{` |
|      - | 8001 | `		/* Use the given timestamp */` |
|      - | 8002 | `		time_t t;` |
|      - | 8003 | `		struct tm *pTm;` |
|      3 | 8004 | `		if( ph7_value_is_int(apArg[1]) ){` |
|      3 | 8005 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|      3 | 8006 | `			pTm = localtime(&t);` |
|      3 | 8007 | `			if( pTm == 0 ){` |
|    ! 0 | 8008 | `				time(&t);` |
|    ! 0 | 8009 | `			}` |
|      2 | 8010 | `		}else{` |
|    ! 0 | 8011 | `			time(&t);` |
|      - | 8012 | `		}` |
|      3 | 8013 | `		pTm = localtime(&t);` |
|      3 | 8014 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8015 | `	}` |
|      - | 8016 | `	/* Format the given string */` |
|     17 | 8017 | `	PH7_Strftime(pCtx,zFormat,nLen,&sTm);` |
|     17 | 8018 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|      - | 8019 | `		/* Nothing was formatted,return FALSE */` |
|    ! 0 | 8020 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8021 | `	}` |
|     17 | 8022 | `	return PH7_OK;` |
|     11 | 8023 |  |
|      - | 8024 | `/*` |
|      - | 8025 | ` * string gmdate(string $format [, int $timestamp = time() ] )` |
|      - | 8026 | ` *  Identical to the date() function except that the time returned` |
|      - | 8027 | ` *  is Greenwich Mean Time (GMT).` |
|      - | 8028 | ` * Parameters` |
|      - | 8029 | ` *  $format` |
|      - | 8030 | ` *  The format of the outputted date string (See code above)` |
|      - | 8031 | ` *  $timestamp` |
|      - | 8032 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|      - | 8033 | ` *   that defaults to the current local time if a timestamp is not given.` |
|      - | 8034 | ` *   In other words, it defaults to the value of time().` |
|      - | 8035 | ` * Return` |
|      - | 8036 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|      - | 8037 | ` */` |
|     16 | 8038 | `static int PH7_builtin_gmdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8039 |  |
|      - | 8040 | `	const char *zFormat;` |
|      - | 8041 | `	int nLen;` |
|      - | 8042 | `	Sytm sTm;` |
|     17 | 8043 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8044 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 | 8045 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8046 | `		return PH7_OK;` |
|      - | 8047 | `	}` |
|     15 | 8048 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 8049 | `	if( nLen < 1 ){` |
|      - | 8050 | `		/* Don't bother processing return the empty string */` |
|    ! 0 | 8051 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 8052 | `	}` |
|     15 | 8053 | `	if( nArg < 2 ){` |
|      - | 8054 | `#ifdef __WINNT__` |
|      - | 8055 | `		SYSTEMTIME sOS;` |
|      1 | 8056 | `		GetSystemTime(&sOS);` |
|      1 | 8057 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8058 | `#else` |
|      - | 8059 | `		struct tm *pTm;` |
|      - | 8060 | `		time_t t;` |
|     12 | 8061 | `		time(&t);` |
|     12 | 8062 | `		pTm = gmtime(&t);` |
|     12 | 8063 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8064 | `#endif` |
|      7 | 8065 | `	}else{` |
|      - | 8066 | `		/* Use the given timestamp */` |
|      - | 8067 | `		time_t t;` |
|      - | 8068 | `		struct tm *pTm;` |
|      3 | 8069 | `		if( ph7_value_is_int(apArg[1]) ){` |
|      3 | 8070 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|      3 | 8071 | `			pTm = gmtime(&t);` |
|      3 | 8072 | `			if( pTm == 0 ){` |
|    ! 0 | 8073 | `				time(&t);` |
|    ! 0 | 8074 | `			}` |
|      2 | 8075 | `		}else{` |
|    ! 0 | 8076 | `			time(&t);` |
|      - | 8077 | `		}` |
|      3 | 8078 | `		pTm = gmtime(&t);` |
|      3 | 8079 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8080 | `	}` |
|      - | 8081 | `	/* Format the given string */` |
|     15 | 8082 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|     15 | 8083 | `	return PH7_OK;` |
|      9 | 8084 |  |
|      - | 8085 | `/*` |
|      - | 8086 | ` * array localtime([ int $timestamp = time() [, bool $is_associative = false ]])` |
|      - | 8087 | ` *  Return the local time.` |
|      - | 8088 | ` * Parameter` |
|      - | 8089 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|      - | 8090 | ` *     that defaults to the current local time if a timestamp is not given.` |
|      - | 8091 | ` *     In other words, it defaults to the value of time().` |
|      - | 8092 | ` * $is_associative` |
|      - | 8093 | ` *   If set to FALSE or not supplied then the array is returned as a regular, numerically` |
|      - | 8094 | ` *   indexed array. If the argument is set to TRUE then localtime() returns an associative` |
|      - | 8095 | ` *   array containing all the different elements of the structure returned by the C function` |
|      - | 8096 | ` *   call to localtime. The names of the different keys of the associative array are as follows:` |
|      - | 8097 | ` *      "tm_sec" - seconds, 0 to 59` |
|      - | 8098 | ` *      "tm_min" - minutes, 0 to 59` |
|      - | 8099 | ` *      "tm_hour" - hours, 0 to 23` |
|      - | 8100 | ` *      "tm_mday" - day of the month, 1 to 31` |
|      - | 8101 | ` *      "tm_mon" - month of the year, 0 (Jan) to 11 (Dec)` |
|      - | 8102 | ` *      "tm_year" - years since 1900` |
|      - | 8103 | ` *      "tm_wday" - day of the week, 0 (Sun) to 6 (Sat)` |
|      - | 8104 | ` *      "tm_yday" - day of the year, 0 to 365` |
|      - | 8105 | ` *      "tm_isdst" - is daylight savings time in effect? Positive if yes, 0 if not, negative if unknown.` |
|      - | 8106 | ` * Returns` |
|      - | 8107 | ` *  An associative array of information related to the timestamp.` |
|      - | 8108 | ` */` |
|      8 | 8109 | `static int PH7_builtin_localtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8110 |  |
|      - | 8111 | `	ph7_value *pValue,*pArray;` |
|      9 | 8112 | `	int isAssoc = 0;` |
|      - | 8113 | `	Sytm sTm;` |
|      9 | 8114 | `	if( nArg < 1 ){` |
|      - | 8115 | `#ifdef __WINNT__` |
|      - | 8116 | `		SYSTEMTIME sOS;` |
|      1 | 8117 | `		GetSystemTime(&sOS); /* TODO(chems): GMT not local */` |
|      1 | 8118 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8119 | `#else` |
|      - | 8120 | `		struct tm *pTm;` |
|      - | 8121 | `		time_t t;` |
|      4 | 8122 | `		time(&t);` |
|      4 | 8123 | `		pTm = localtime(&t);` |
|      4 | 8124 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8125 | `#endif` |
|      3 | 8126 | `	}else{` |
|      - | 8127 | `		/* Use the given timestamp */` |
|      - | 8128 | `		time_t t;` |
|      - | 8129 | `		struct tm *pTm;` |
|      5 | 8130 | `		if( ph7_value_is_int(apArg[0]) ){` |
|      5 | 8131 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|      5 | 8132 | `			pTm = localtime(&t);` |
|      5 | 8133 | `			if( pTm == 0 ){` |
|    ! 0 | 8134 | `				time(&t);` |
|    ! 0 | 8135 | `			}` |
|      3 | 8136 | `		}else{` |
|    ! 0 | 8137 | `			time(&t);` |
|      - | 8138 | `		}` |
|      5 | 8139 | `		pTm = localtime(&t);` |
|      5 | 8140 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8141 | `	}` |
|      - | 8142 | `	/* Element value */` |
|      9 | 8143 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      9 | 8144 | `	if( pValue == 0 ){` |
|      - | 8145 | `		/* Return NULL */` |
|    ! 0 | 8146 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8147 | `		return PH7_OK;` |
|      - | 8148 | `	}` |
|      - | 8149 | `	/* Create a new array */` |
|      9 | 8150 | `	pArray = ph7_context_new_array(pCtx);` |
|      9 | 8151 | `	if( pArray == 0 ){` |
|      - | 8152 | `		/* Return NULL */` |
|    ! 0 | 8153 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8154 | `		return PH7_OK;` |
|      - | 8155 | `	}` |
|      9 | 8156 | `	if( nArg > 1 ){` |
|      3 | 8157 | `		isAssoc = ph7_value_to_bool(apArg[1]);` |
|      1 | 8158 | `	}` |
|      - | 8159 | `	/* Fill the array */` |
|      - | 8160 | `	/* Seconds */` |
|      9 | 8161 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|      9 | 8162 | `	if( isAssoc ){` |
|      3 | 8163 | `		ph7_array_add_strkey_elem(pArray,"tm_sec",pValue);` |
|      2 | 8164 | `	}else{` |
|      7 | 8165 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8166 | `	}` |
|      - | 8167 | `	/* Minutes */` |
|      9 | 8168 | `	ph7_value_int(pValue,sTm.tm_min);` |
|      9 | 8169 | `	if( isAssoc ){` |
|      3 | 8170 | `		ph7_array_add_strkey_elem(pArray,"tm_min",pValue);` |
|      2 | 8171 | `	}else{` |
|      7 | 8172 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8173 | `	}` |
|      - | 8174 | `	/* Hours */` |
|      9 | 8175 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|      9 | 8176 | `	if( isAssoc ){` |
|      3 | 8177 | `		ph7_array_add_strkey_elem(pArray,"tm_hour",pValue);` |
|      2 | 8178 | `	}else{` |
|      7 | 8179 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8180 | `	}` |
|      - | 8181 | `	/* mday */` |
|      9 | 8182 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|      9 | 8183 | `	if( isAssoc ){` |
|      3 | 8184 | `		ph7_array_add_strkey_elem(pArray,"tm_mday",pValue);` |
|      2 | 8185 | `	}else{` |
|      7 | 8186 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8187 | `	}` |
|      - | 8188 | `	/* mon */` |
|      9 | 8189 | `	ph7_value_int(pValue,sTm.tm_mon);` |
|      9 | 8190 | `	if( isAssoc ){` |
|      3 | 8191 | `		ph7_array_add_strkey_elem(pArray,"tm_mon",pValue);` |
|      2 | 8192 | `	}else{` |
|      7 | 8193 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8194 | `	}` |
|      - | 8195 | `	/* year since 1900 */` |
|      9 | 8196 | `	ph7_value_int(pValue,sTm.tm_year-1900);` |
|      9 | 8197 | `	if( isAssoc ){` |
|      3 | 8198 | `		ph7_array_add_strkey_elem(pArray,"tm_year",pValue);` |
|      2 | 8199 | `	}else{` |
|      7 | 8200 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8201 | `	}` |
|      - | 8202 | `	/* wday */` |
|      9 | 8203 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|      9 | 8204 | `	if( isAssoc ){` |
|      3 | 8205 | `		ph7_array_add_strkey_elem(pArray,"tm_wday",pValue);` |
|      2 | 8206 | `	}else{` |
|      7 | 8207 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8208 | `	}` |
|      - | 8209 | `	/* yday */` |
|      9 | 8210 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|      9 | 8211 | `	if( isAssoc ){` |
|      3 | 8212 | `		ph7_array_add_strkey_elem(pArray,"tm_yday",pValue);` |
|      2 | 8213 | `	}else{` |
|      7 | 8214 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8215 | `	}` |
|      - | 8216 | `	/* isdst */` |
|      - | 8217 | `#ifdef __WINNT__` |
|      - | 8218 | `#ifdef _MSC_VER` |
|      - | 8219 | `#ifndef _WIN32_WCE` |
|      1 | 8220 | `			_get_daylight(&sTm.tm_isdst);` |
|      - | 8221 | `#endif` |
|      - | 8222 | `#endif` |
|      - | 8223 | `#endif` |
|      9 | 8224 | `	ph7_value_int(pValue,sTm.tm_isdst);` |
|      9 | 8225 | `	if( isAssoc ){` |
|      3 | 8226 | `		ph7_array_add_strkey_elem(pArray,"tm_isdst",pValue);` |
|      2 | 8227 | `	}else{` |
|      7 | 8228 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8229 | `	}` |
|      - | 8230 | `	/* Return the array */` |
|      9 | 8231 | `	ph7_result_value(pCtx,pArray);` |
|      9 | 8232 | `	return PH7_OK;` |
|      5 | 8233 |  |
|      - | 8234 | `/*` |
|      - | 8235 | ` * int idate(string $format [, int $timestamp = time() ])` |
|      - | 8236 | ` *  Returns a number formatted according to the given format string` |
|      - | 8237 | ` *  using the given integer timestamp or the current local time if` |
|      - | 8238 | ` *  no timestamp is given. In other words, timestamp is optional and defaults` |
|      - | 8239 | ` *  to the value of time().` |
|      - | 8240 | ` *  Unlike the function date(), idate() accepts just one char in the format` |
|      - | 8241 | ` *  parameter.` |
|      - | 8242 | ` * $Parameters` |
|      - | 8243 | ` *  Supported format` |
|      - | 8244 | ` *   d 	Day of the month` |
|      - | 8245 | ` *   h 	Hour (12 hour format)` |
|      - | 8246 | ` *   H 	Hour (24 hour format)` |
|      - | 8247 | ` *   i 	Minutes` |
|      - | 8248 | ` *   I (uppercase i)1 if DST is activated, 0 otherwise` |
|      - | 8249 | ` *   L (uppercase l) returns 1 for leap year, 0 otherwise` |
|      - | 8250 | ` *   m 	Month number` |
|      - | 8251 | ` *   s 	Seconds` |
|      - | 8252 | ` *   t 	Days in current month` |
|      - | 8253 | ` *   U 	Seconds since the Unix Epoch - January 1 1970 00:00:00 UTC - this is the same as time()` |
|      - | 8254 | ` *   w 	Day of the week (0 on Sunday)` |
|      - | 8255 | ` *   W 	ISO-8601 week number of year, weeks starting on Monday` |
|      - | 8256 | ` *   y 	Year (1 or 2 digits - check note below)` |
|      - | 8257 | ` *   Y 	Year (4 digits)` |
|      - | 8258 | ` *   z 	Day of the year` |
|      - | 8259 | ` *   Z 	Timezone offset in seconds` |
|      - | 8260 | ` * $timestamp` |
|      - | 8261 | ` *  The optional timestamp parameter is an integer Unix timestamp that defaults` |
|      - | 8262 | ` *  to the current local time if a timestamp is not given. In other words, it defaults` |
|      - | 8263 | ` *  to the value of time().` |
|      - | 8264 | ` * Return` |
|      - | 8265 | ` *  An integer.` |
|      - | 8266 | ` */` |
|     40 | 8267 | `static int PH7_builtin_idate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 8268 |  |
|      - | 8269 | `	const char *zFormat;` |
|     42 | 8270 | `	ph7_int64 iVal = 0;` |
|      - | 8271 | `	int nLen;` |
|      - | 8272 | `	Sytm sTm;` |
|     42 | 8273 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8274 | `		/* Missing/Invalid argument,return -1 */` |
|      5 | 8275 | `		ph7_result_int(pCtx,-1);` |
|      5 | 8276 | `		return PH7_OK;` |
|      - | 8277 | `	}` |
|     42 | 8278 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     42 | 8279 | `	if( nLen < 1 ){` |
|      - | 8280 | `		/* Don't bother processing return -1*/` |
|    ! 0 | 8281 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 8282 | `	}` |
|     42 | 8283 | `	if( nArg < 2 ){` |
|      - | 8284 | `#ifdef __WINNT__` |
|      - | 8285 | `		SYSTEMTIME sOS;` |
|      2 | 8286 | `		GetSystemTime(&sOS);` |
|      2 | 8287 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8288 | `#else` |
|      - | 8289 | `		struct tm *pTm;` |
|      - | 8290 | `		time_t t;` |
|     30 | 8291 | `		time(&t);` |
|     30 | 8292 | `		pTm = localtime(&t);` |
|     30 | 8293 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8294 | `#endif` |
|     18 | 8295 | `	}else{` |
|      - | 8296 | `		/* Use the given timestamp */` |
|      - | 8297 | `		time_t t;` |
|      - | 8298 | `		struct tm *pTm;` |
|     11 | 8299 | `		if( ph7_value_is_int(apArg[1]) ){` |
|     11 | 8300 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|     11 | 8301 | `			pTm = localtime(&t);` |
|     11 | 8302 | `			if( pTm == 0 ){` |
|    ! 0 | 8303 | `				time(&t);` |
|    ! 0 | 8304 | `			}` |
|      6 | 8305 | `		}else{` |
|    ! 0 | 8306 | `			time(&t);` |
|      - | 8307 | `		}` |
|     11 | 8308 | `		pTm = localtime(&t);` |
|     11 | 8309 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8310 | `	}` |
|      - | 8311 | `	/* Perform the requested operation */` |
|     42 | 8312 | `	switch(zFormat[0]){` |
|      2 | 8313 | `	case 'd':` |
|      - | 8314 | `		/* Day of the month */` |
|      5 | 8315 | `		iVal = sTm.tm_mday;` |
|      5 | 8316 | `		break;` |
|    ! 0 | 8317 | `	case 'h':` |
|      - | 8318 | `		/*	Hour (12 hour format)*/` |
|    ! 0 | 8319 | `		iVal = 1 + (sTm.tm_hour % 12);` |
|    ! 0 | 8320 | `		break;` |
|      1 | 8321 | `	case 'H':` |
|      - | 8322 | `		/* Hour (24 hour format)*/` |
|      3 | 8323 | `		iVal = sTm.tm_hour;` |
|      3 | 8324 | `		break;` |
|      1 | 8325 | `	case 'i':` |
|      - | 8326 | `		/*Minutes*/` |
|      3 | 8327 | `		iVal = sTm.tm_min;` |
|      3 | 8328 | `		break;` |
|      1 | 8329 | `	case 'I':` |
|      - | 8330 | `		/*	returns 1 if DST is activated, 0 otherwise */` |
|      - | 8331 | `#ifdef __WINNT__` |
|      - | 8332 | `#ifdef _MSC_VER` |
|      - | 8333 | `#ifndef _WIN32_WCE` |
|      1 | 8334 | `			_get_daylight(&sTm.tm_isdst);` |
|      - | 8335 | `#endif` |
|      - | 8336 | `#endif` |
|      - | 8337 | `#endif` |
|      3 | 8338 | `		iVal = sTm.tm_isdst;` |
|      3 | 8339 | `		break;` |
|      1 | 8340 | `	case 'L':` |
|      - | 8341 | `		/* 	returns 1 for leap year, 0 otherwise */` |
|      3 | 8342 | `		iVal = IS_LEAP_YEAR(sTm.tm_year);` |
|      3 | 8343 | `		break;` |
|      2 | 8344 | `	case 'm':` |
|      - | 8345 | `		/* Month number*/` |
|      5 | 8346 | `		iVal = sTm.tm_mon;` |
|      5 | 8347 | `		break;` |
|      1 | 8348 | `	case 's':` |
|      - | 8349 | `		/*Seconds*/` |
|      3 | 8350 | `		iVal = sTm.tm_sec;` |
|      3 | 8351 | `		break;` |
|      1 | 8352 | `	case 't':{` |
|      - | 8353 | `		/*Days in current month*/` |
|      - | 8354 | `		static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|      5 | 8355 | `		int nDays = aMonDays[sTm.tm_mon % 12 ];` |
|      5 | 8356 | `		if( sTm.tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(sTm.tm_year) ){` |
|      3 | 8357 | `			nDays = 28;` |
|      1 | 8358 | `		}` |
|      7 | 8359 | `		iVal = nDays;` |
|      7 | 8360 | `		break;` |
|      - | 8361 | `			 }` |
|      1 | 8362 | `	case 'U':` |
|      - | 8363 | `		/*Seconds since the Unix Epoch*/` |
|      3 | 8364 | `		iVal = (ph7_int64)time(0);` |
|      3 | 8365 | `		break;` |
|      1 | 8366 | `	case 'w':` |
|      - | 8367 | `		/*	Day of the week (0 on Sunday) */` |
|      3 | 8368 | `		iVal = sTm.tm_wday;` |
|      3 | 8369 | `		break;` |
|      1 | 8370 | `	case 'W': {` |
|      - | 8371 | `		/* ISO-8601 week number of year, weeks starting on Monday */` |
|      - | 8372 | `		static const int aISO8601_local[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|      3 | 8373 | `		iVal = aISO8601_local[sTm.tm_wday % 7 ];` |
|      3 | 8374 | `		break;` |
|      - | 8375 | `			  }` |
|    ! 0 | 8376 | `	case 'y':` |
|      - | 8377 | `		/* Year (2 digits) */` |
|    ! 0 | 8378 | `		iVal = sTm.tm_year % 100;` |
|    ! 0 | 8379 | `		break;` |
|      3 | 8380 | `	case 'Y':` |
|      - | 8381 | `		/* Year (4 digits) */` |
|      7 | 8382 | `		iVal = sTm.tm_year;` |
|      7 | 8383 | `		break;` |
|      1 | 8384 | `	case 'z':` |
|      - | 8385 | `		/* Day of the year */` |
|      3 | 8386 | `		iVal = sTm.tm_yday;` |
|      3 | 8387 | `		break;` |
|      1 | 8388 | `	case 'Z':` |
|      - | 8389 | `		/*Timezone offset in seconds*/` |
|      3 | 8390 | `		iVal = sTm.tm_gmtoff;` |
|      3 | 8391 | `		break;` |
|      1 | 8392 | `	default:` |
|      - | 8393 | `		/* unknown format,throw a warning */` |
|      3 | 8394 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Unknown date format token");` |
|      2 | 8395 | `		break;` |
|      - | 8396 | `	}` |
|      - | 8397 | `	/* Return the time value */` |
|     40 | 8398 | `	ph7_result_int64(pCtx,iVal);` |
|     40 | 8399 | `	return PH7_OK;` |
|     23 | 8400 |  |
|      - | 8401 | `/*` |
|      - | 8402 | ` * int mktime/gmmktime([ int $hour = date("H") [, int $minute = date("i") [, int $second = date("s")` |
|      - | 8403 | ` *  [, int $month = date("n") [, int $day = date("j") [, int $year = date("Y") [, int $is_dst = -1 ]]]]]]] )` |
|      - | 8404 | ` *  Returns the Unix timestamp corresponding to the arguments given. This timestamp is a 64bit integer` |
|      - | 8405 | ` *  containing the number of seconds between the Unix Epoch (January 1 1970 00:00:00 GMT) and the time` |
|      - | 8406 | ` *  specified.` |
|      - | 8407 | ` *  Arguments may be left out in order from right to left; any arguments thus omitted will be set to` |
|      - | 8408 | ` *  the current value according to the local date and time.` |
|      - | 8409 | ` * Parameters` |
|      - | 8410 | ` * $hour` |
|      - | 8411 | ` *  The number of the hour relevant to the start of the day determined by month, day and year.` |
|      - | 8412 | ` *  Negative values reference the hour before midnight of the day in question. Values greater` |
|      - | 8413 | ` *  than 23 reference the appropriate hour in the following day(s).` |
|      - | 8414 | ` * $minute` |
|      - | 8415 | ` *  The number of the minute relevant to the start of the hour. Negative values reference` |
|      - | 8416 | ` *  the minute in the previous hour. Values greater than 59 reference the appropriate minute` |
|      - | 8417 | ` *  in the following hour(s).` |
|      - | 8418 | ` * $second` |
|      - | 8419 | ` *  The number of seconds relevant to the start of the minute. Negative values reference` |
|      - | 8420 | ` *  the second in the previous minute. Values greater than 59 reference the appropriate` |
|      - | 8421 | ` * second in the following minute(s).` |
|      - | 8422 | ` * $month` |
|      - | 8423 | ` *  The number of the month relevant to the end of the previous year. Values 1 to 12 reference` |
|      - | 8424 | ` *  the normal calendar months of the year in question. Values less than 1 (including negative values)` |
|      - | 8425 | ` *  reference the months in the previous year in reverse order, so 0 is December, -1 is November)...` |
|      - | 8426 | ` * $day` |
|      - | 8427 | ` *  The number of the day relevant to the end of the previous month. Values 1 to 28, 29, 30 or 31` |
|      - | 8428 | ` *  (depending upon the month) reference the normal days in the relevant month. Values less than 1` |
|      - | 8429 | ` *  (including negative values) reference the days in the previous month, so 0 is the last day` |
|      - | 8430 | ` *  of the previous month, -1 is the day before that, etc. Values greater than the number of days` |
|      - | 8431 | ` *  in the relevant month reference the appropriate day in the following month(s).` |
|      - | 8432 | ` * $year` |
|      - | 8433 | ` *  The number of the year, may be a two or four digit value, with values between 0-69 mapping` |
|      - | 8434 | ` *  to 2000-2069 and 70-100 to 1970-2000. On systems where time_t is a 32bit signed integer, as` |
|      - | 8435 | ` *  most common today, the valid range for year is somewhere between 1901 and 2038.` |
|      - | 8436 | ` * $is_dst` |
|      - | 8437 | ` *  This parameter can be set to 1 if the time is during daylight savings time (DST), 0 if it is not,` |
|      - | 8438 | ` *  or -1 (the default) if it is unknown whether the time is within daylight savings time or not.` |
|      - | 8439 | ` * Return` |
|      - | 8440 | ` *   mktime() returns the Unix timestamp of the arguments given.` |
|      - | 8441 | ` *   If the arguments are invalid, the function returns FALSE` |
|      - | 8442 | ` */` |
|      8 | 8443 | `static int PH7_builtin_mktime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8444 |  |
|      - | 8445 | `	const char *zFunction;` |
|      9 | 8446 | `	ph7_int64 iVal = 0;` |
|      - | 8447 | `	struct tm *pTm;` |
|      - | 8448 | `	time_t t;` |
|      - | 8449 | `	/* Extract function name */` |
|      9 | 8450 | `	zFunction = ph7_function_name(pCtx);` |
|      - | 8451 | `	/* Get the current time */` |
|      9 | 8452 | `	time(&t);` |
|      9 | 8453 | `	if( zFunction[0] == 'g' /* gmmktime */ ){` |
|      3 | 8454 | `		pTm = gmtime(&t);` |
|      2 | 8455 | `	}else{` |
|      - | 8456 | `		/* localtime */` |
|      7 | 8457 | `		pTm = localtime(&t);` |
|      - | 8458 | `	}` |
|      9 | 8459 | `	if( nArg > 0 ){` |
|      - | 8460 | `		int iTmp;` |
|      - | 8461 | `		/* Hour */` |
|      9 | 8462 | `		iTmp = ph7_value_to_int(apArg[0]);` |
|      9 | 8463 | `		pTm->tm_hour = iTmp;` |
|      9 | 8464 | `		if( nArg > 1 ){` |
|      - | 8465 | `			/* Minutes */` |
|      9 | 8466 | `			iTmp = ph7_value_to_int(apArg[1]);` |
|      9 | 8467 | `			pTm->tm_min = iTmp;` |
|      9 | 8468 | `			if( nArg > 2 ){` |
|      - | 8469 | `				/* Seconds */` |
|      9 | 8470 | `				iTmp = ph7_value_to_int(apArg[2]);` |
|      9 | 8471 | `				pTm->tm_sec = iTmp;` |
|      9 | 8472 | `				if( nArg > 3 ){` |
|      - | 8473 | `					/* Month */` |
|      9 | 8474 | `					iTmp = ph7_value_to_int(apArg[3]);` |
|      9 | 8475 | `					pTm->tm_mon = iTmp - 1;` |
|      9 | 8476 | `					if( nArg > 4 ){` |
|      - | 8477 | `						/* mday */` |
|      9 | 8478 | `						iTmp = ph7_value_to_int(apArg[4]);` |
|      9 | 8479 | `						pTm->tm_mday = iTmp;` |
|      9 | 8480 | `						if( nArg > 5 ){` |
|      - | 8481 | `							/* Year */` |
|      9 | 8482 | `							iTmp = ph7_value_to_int(apArg[5]);` |
|      9 | 8483 | `							if( iTmp > 1900 ){` |
|      9 | 8484 | `								iTmp -= 1900;` |
|      4 | 8485 | `							}` |
|      9 | 8486 | `							pTm->tm_year = iTmp;` |
|      9 | 8487 | `							if( nArg > 6 ){` |
|      - | 8488 | `								/* is_dst */` |
|    ! 0 | 8489 | `								iTmp = ph7_value_to_bool(apArg[6]);` |
|    ! 0 | 8490 | `								pTm->tm_isdst = iTmp;` |
|    ! 0 | 8491 | `							}` |
|      4 | 8492 | `						}` |
|      4 | 8493 | `					}` |
|      4 | 8494 | `				}` |
|      4 | 8495 | `			}` |
|      4 | 8496 | `		}` |
|      4 | 8497 | `	}` |
|      - | 8498 | `	/* Make the time */` |
|      9 | 8499 | `	iVal = (ph7_int64)mktime(pTm);` |
|      - | 8500 | `	/* Return the timesatmp as a 64bit integer */` |
|      9 | 8501 | `	ph7_result_int64(pCtx,iVal);` |
|      9 | 8502 | `	return PH7_OK;` |
|      1 | 8503 |  |
|      - | 8504 | `/*` |
|      - | 8505 | ` * Section:` |
|      - | 8506 | ` *    URL handling Functions.` |
|      - | 8507 | ` * Status:` |
|      - | 8508 | ` *    Stable.` |
|      - | 8509 | ` */` |
|      - | 8510 | `/*` |
|      - | 8511 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 8512 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 8513 | ` */` |
|   1026 | 8514 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 8515 |  |
|      - | 8516 | `	/* Store in the call context result buffer */` |
|   1028 | 8517 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 8518 | `	return SXRET_OK;` |
|      2 | 8519 |  |
|      - | 8520 | `/*` |
|      - | 8521 | ` * string base64_encode(string $data)` |
|      - | 8522 | ` * string convert_uuencode(string $data)` |
|      - | 8523 | ` *  Encodes data with MIME base64` |
|      - | 8524 | ` * Parameter` |
|      - | 8525 | ` *  $data` |
|      - | 8526 | ` *    Data to encode` |
|      - | 8527 | ` * Return` |
|      - | 8528 | ` *  Encoded data or FALSE on failure.` |
|      - | 8529 | ` */` |
|     10 | 8530 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8531 |  |
|      - | 8532 | `	const char *zIn;` |
|      - | 8533 | `	int nLen;` |
|     11 | 8534 | `	if( nArg < 1 ){` |
|      - | 8535 | `		/* Missing arguments,return FALSE */` |
|      5 | 8536 | `		ph7_result_bool(pCtx,0);` |
|      5 | 8537 | `		return PH7_OK;` |
|      - | 8538 | `	}` |
|      - | 8539 | `	/* Extract the input string */` |
|      7 | 8540 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8541 | `	if( nLen < 1 ){` |
|      - | 8542 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8543 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8544 | `		return PH7_OK;` |
|      - | 8545 | `	}` |
|      - | 8546 | `	/* Perform the BASE64 encoding */` |
|      7 | 8547 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 8548 | `	return PH7_OK;` |
|      6 | 8549 |  |
|      - | 8550 | `/*` |
|      - | 8551 | ` * string base64_decode(string $data)` |
|      - | 8552 | ` * string convert_uudecode(string $data)` |
|      - | 8553 | ` *  Decodes data encoded with MIME base64` |
|      - | 8554 | ` * Parameter` |
|      - | 8555 | ` *  $data` |
|      - | 8556 | ` *    Encoded data.` |
|      - | 8557 | ` * Return` |
|      - | 8558 | ` *  Returns the original data or FALSE on failure.` |
|      - | 8559 | ` */` |
|     36 | 8560 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 8561 |  |
|      - | 8562 | `	const char *zIn;` |
|      - | 8563 | `	int nLen;` |
|     38 | 8564 | `	if( nArg < 1 ){` |
|      - | 8565 | `		/* Missing arguments,return FALSE */` |
|      3 | 8566 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8567 | `		return PH7_OK;` |
|      - | 8568 | `	}` |
|      - | 8569 | `	/* Extract the input string */` |
|     36 | 8570 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 8571 | `	if( nLen < 1 ){` |
|      - | 8572 | `		/* Nothing to process,return FALSE */` |
|      3 | 8573 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8574 | `		return PH7_OK;` |
|      - | 8575 | `	}` |
|      - | 8576 | `	/* Perform the BASE64 decoding */` |
|     34 | 8577 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 8578 | `	return PH7_OK;` |
|     20 | 8579 |  |
|      - | 8580 | `/*` |
|      - | 8581 | ` * string urlencode(string $str)` |
|      - | 8582 | ` *  URL encoding` |
|      - | 8583 | ` * Parameter` |
|      - | 8584 | ` *  $data` |
|      - | 8585 | ` *   Input string.` |
|      - | 8586 | ` * Return` |
|      - | 8587 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 8588 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 8589 | ` *  encoded as plus (+) signs.` |
|      - | 8590 | ` */` |
|      6 | 8591 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8592 |  |
|      - | 8593 | `	const char *zIn;` |
|      - | 8594 | `	int nLen;` |
|      7 | 8595 | `	if( nArg < 1 ){` |
|      - | 8596 | `		/* Missing arguments,return FALSE */` |
|      3 | 8597 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8598 | `		return PH7_OK;` |
|      - | 8599 | `	}` |
|      - | 8600 | `	/* Extract the input string */` |
|      5 | 8601 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 8602 | `	if( nLen < 1 ){` |
|      - | 8603 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8604 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8605 | `		return PH7_OK;` |
|      - | 8606 | `	}` |
|      - | 8607 | `	/* Perform the URL encoding */` |
|      5 | 8608 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 8609 | `	return PH7_OK;` |
|      4 | 8610 |  |
|      - | 8611 | `/*` |
|      - | 8612 | ` * string urldecode(string $str)` |
|      - | 8613 | ` *  Decodes any %## encoding in the given string.` |
|      - | 8614 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 8615 | ` * Parameter` |
|      - | 8616 | ` *  $data` |
|      - | 8617 | ` *    Input string.` |
|      - | 8618 | ` * Return` |
|      - | 8619 | ` *  Decoded URL or FALSE on failure.` |
|      - | 8620 | ` */` |
|      8 | 8621 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8622 |  |
|      - | 8623 | `	const char *zIn;` |
|      - | 8624 | `	int nLen;` |
|      9 | 8625 | `	if( nArg < 1 ){` |
|      - | 8626 | `		/* Missing arguments,return FALSE */` |
|      3 | 8627 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8628 | `		return PH7_OK;` |
|      - | 8629 | `	}` |
|      - | 8630 | `	/* Extract the input string */` |
|      7 | 8631 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8632 | `	if( nLen < 1 ){` |
|      - | 8633 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8634 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8635 | `		return PH7_OK;` |
|      - | 8636 | `	}` |
|      - | 8637 | `	/* Perform the URL decoding */` |
|      7 | 8638 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 8639 | `	return PH7_OK;` |
|      5 | 8640 |  |
|      - | 8641 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8642 | `/* Table of the built-in functions */` |
|      - | 8643 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 8644 | `	   /* Variable handling functions */` |
|      - | 8645 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 8646 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 8647 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 8648 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 8649 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 8650 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 8651 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 8652 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 8653 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 8654 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 8655 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 8656 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 8657 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 8658 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 8659 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 8660 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 8661 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 8662 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 8663 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 8664 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8665 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 8666 | `	   /* Math functions */` |
|      - | 8667 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 8668 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 8669 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 8670 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 8671 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 8672 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 8673 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 8674 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 8675 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 8676 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 8677 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 8678 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 8679 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 8680 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 8681 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 8682 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 8683 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 8684 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 8685 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 8686 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 8687 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 8688 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 8689 | `	{ "round",    PH7_builtin_round        },` |
|      - | 8690 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 8691 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 8692 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 8693 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 8694 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 8695 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 8696 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 8697 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 8698 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8699 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8700 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 8701 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8702 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8703 | `	   /* String handling functions */` |
|      - | 8704 |  |
|      - | 8705 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 8706 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 8707 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 8708 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 8709 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 8710 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 8711 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 8712 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 8713 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 8714 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 8715 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 8716 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 8717 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 8718 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 8719 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 8720 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 8721 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 8722 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 8723 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 8724 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 8725 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 8726 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 8727 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 8728 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 8729 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 8730 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 8731 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 8732 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 8733 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 8734 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 8735 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 8736 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 8737 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 8738 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 8739 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 8740 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 8741 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 8742 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 8743 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 8744 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 8745 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 8746 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 8747 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 8748 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 8749 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 8750 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 8751 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 8752 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 8753 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 8754 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8755 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8756 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 8757 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 8758 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 8759 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 8760 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8761 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8762 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 8763 |  |
|      - | 8764 |  |
|      - | 8765 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 8766 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 8767 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 8768 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 8769 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 8770 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8771 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8772 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 8773 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 8774 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8775 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8776 |  |
|      - | 8777 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 8778 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 8779 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 8780 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 8781 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 8782 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 8783 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 8784 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 8785 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 8786 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 8787 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 8788 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 8789 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8790 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8791 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 8792 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8793 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8794 |  |
|      - | 8795 | `	         /* Ctype functions */` |
|      - | 8796 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 8797 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 8798 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 8799 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 8800 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 8801 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 8802 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 8803 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 8804 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 8805 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 8806 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 8807 | `	         /* Time functions */` |
|      - | 8808 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 8809 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 8810 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 8811 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 8812 | `	{ "date",        PH7_builtin_date         },` |
|      - | 8813 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 8814 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 8815 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 8816 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 8817 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 8818 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 8819 | `	        /* URL functions */` |
|      - | 8820 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 8821 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 8822 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 8823 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 8824 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 8825 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 8826 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 8827 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 8828 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8829 | `};` |
|      - | 8830 | `/*` |
|      - | 8831 | ` * Register the built-in functions defined above,the array functions` |
|      - | 8832 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 8833 | ` */` |
|    956 | 8834 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      2 | 8835 |  |
|      - | 8836 | `	sxu32 n;` |
| 146270 | 8837 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 145314 | 8838 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
|  72658 | 8839 | `	}` |
|      - | 8840 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|    958 | 8841 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 8842 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|    958 | 8843 | `	PH7_RegisterIORoutine(&(*pVm));` |
|    958 | 8844 |  |
|      - | 8845 |  |
