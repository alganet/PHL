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
|     26 |  116 | `static int PH7_builtin_is_numeric(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  117 |  |
|     27 |  118 | `	int res = 0; /* Assume false by default */` |
|     27 |  119 | `	if( nArg > 0 ){` |
|     25 |  120 | `		res = ph7_value_is_numeric(apArg[0]);` |
|     12 |  121 | `	}` |
|      - |  122 | `	/* Query result */` |
|     27 |  123 | `	ph7_result_bool(pCtx,res);` |
|     27 |  124 | `	return PH7_OK;` |
|      1 |  125 |  |
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
|  12948 |  269 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  270 |  |
|  12950 |  271 | `	int res = 1; /* Assume empty by default */` |
|  12950 |  272 | `	if( nArg > 0 ){` |
|  12948 |  273 | `		res = ph7_value_is_empty(apArg[0]);` |
|   6473 |  274 | `	}` |
|  12950 |  275 | `	ph7_result_bool(pCtx,res);` |
|  12950 |  276 | `	return PH7_OK;` |
|      - |  277 |  |
|      2 |  278 |  |
|      - |  279 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |  280 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - |  281 | `/*` |
|      - |  282 | ` * Section:` |
|      - |  283 | ` *    Math Functions.` |
|      - |  284 | ` * Status:` |
|      - |  285 | ` *    Stable.` |
|      - |  286 | ` */` |
|      - |  287 | `#include <stdlib.h> /* abs */` |
|      - |  288 | `#include <math.h>` |
|      - |  289 | `/*` |
|      - |  290 | ` * float sqrt(float $arg )` |
|      - |  291 | ` *  Square root of the given number.` |
|      - |  292 | ` * Parameter` |
|      - |  293 | ` *  The number to process.` |
|      - |  294 | ` * Return` |
|      - |  295 | ` *  The square root of arg or the special value Nan of failure.` |
|      - |  296 | ` */` |
|      6 |  297 | `static int PH7_builtin_sqrt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  298 |  |
|      - |  299 | `	double r,x;` |
|      7 |  300 | `	if( nArg < 1 ){` |
|      - |  301 | `		/* Missing argument,return 0 */` |
|      5 |  302 | `		ph7_result_int(pCtx,0);` |
|      5 |  303 | `		return PH7_OK;` |
|      - |  304 | `	}` |
|      3 |  305 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  306 | `	/* Perform the requested operation */` |
|      3 |  307 | `	r = sqrt(x);` |
|      - |  308 | `	/* store the result back */` |
|      3 |  309 | `	ph7_result_double(pCtx,r);` |
|      3 |  310 | `	return PH7_OK;` |
|      4 |  311 |  |
|      - |  312 | `/*` |
|      - |  313 | ` * float exp(float $arg )` |
|      - |  314 | ` *  Calculates the exponent of e.` |
|      - |  315 | ` * Parameter` |
|      - |  316 | ` *  The number to process.` |
|      - |  317 | ` * Return` |
|      - |  318 | ` *  'e' raised to the power of arg.` |
|      - |  319 | ` */` |
|     20 |  320 | `static int PH7_builtin_exp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  321 |  |
|      - |  322 | `	double r,x;` |
|     21 |  323 | `	if( nArg < 1 ){` |
|      - |  324 | `		/* Missing argument,return 0 */` |
|      3 |  325 | `		ph7_result_int(pCtx,0);` |
|      3 |  326 | `		return PH7_OK;` |
|      - |  327 | `	}` |
|     19 |  328 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  329 | `	/* Perform the requested operation */` |
|     19 |  330 | `	r = exp(x);` |
|      - |  331 | `	/* store the result back */` |
|     19 |  332 | `	ph7_result_double(pCtx,r);` |
|     19 |  333 | `	return PH7_OK;` |
|     11 |  334 |  |
|      - |  335 | `/*` |
|      - |  336 | ` * float floor(float $arg )` |
|      - |  337 | ` *  Round fractions down.` |
|      - |  338 | ` * Parameter` |
|      - |  339 | ` *  The number to process.` |
|      - |  340 | ` * Return` |
|      - |  341 | ` *  Returns the next lowest integer value by rounding down value if necessary.` |
|      - |  342 | ` */` |
|      4 |  343 | `static int PH7_builtin_floor(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  344 |  |
|      - |  345 | `	double r,x;` |
|      5 |  346 | `	if( nArg < 1 ){` |
|      - |  347 | `		/* Missing argument,return 0 */` |
|      3 |  348 | `		ph7_result_int(pCtx,0);` |
|      3 |  349 | `		return PH7_OK;` |
|      - |  350 | `	}` |
|      3 |  351 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  352 | `	/* Perform the requested operation */` |
|      3 |  353 | `	r = floor(x);` |
|      - |  354 | `	/* store the result back */` |
|      3 |  355 | `	ph7_result_double(pCtx,r);` |
|      3 |  356 | `	return PH7_OK;` |
|      3 |  357 |  |
|      - |  358 | `/*` |
|      - |  359 | ` * float cos(float $arg )` |
|      - |  360 | ` *  Cosine.` |
|      - |  361 | ` * Parameter` |
|      - |  362 | ` *  The number to process.` |
|      - |  363 | ` * Return` |
|      - |  364 | ` *  The cosine of arg.` |
|      - |  365 | ` */` |
|      4 |  366 | `static int PH7_builtin_cos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  367 |  |
|      - |  368 | `	double r,x;` |
|      5 |  369 | `	if( nArg < 1 ){` |
|      - |  370 | `		/* Missing argument,return 0 */` |
|      3 |  371 | `		ph7_result_int(pCtx,0);` |
|      3 |  372 | `		return PH7_OK;` |
|      - |  373 | `	}` |
|      3 |  374 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  375 | `	/* Perform the requested operation */` |
|      3 |  376 | `	r = cos(x);` |
|      - |  377 | `	/* store the result back */` |
|      3 |  378 | `	ph7_result_double(pCtx,r);` |
|      3 |  379 | `	return PH7_OK;` |
|      3 |  380 |  |
|      - |  381 | `/*` |
|      - |  382 | ` * float acos(float $arg )` |
|      - |  383 | ` *  Arc cosine.` |
|      - |  384 | ` * Parameter` |
|      - |  385 | ` *  The number to process.` |
|      - |  386 | ` * Return` |
|      - |  387 | ` *  The arc cosine of arg.` |
|      - |  388 | ` */` |
|     18 |  389 | `static int PH7_builtin_acos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  390 |  |
|      - |  391 | `	double r,x;` |
|     19 |  392 | `	if( nArg < 1 ){` |
|      - |  393 | `		/* Missing argument,return 0 */` |
|      5 |  394 | `		ph7_result_int(pCtx,0);` |
|      5 |  395 | `		return PH7_OK;` |
|      - |  396 | `	}` |
|     15 |  397 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  398 | `	/* Perform the requested operation */` |
|     15 |  399 | `	r = acos(x);` |
|      - |  400 | `	/* store the result back */` |
|     15 |  401 | `	ph7_result_double(pCtx,r);` |
|     15 |  402 | `	return PH7_OK;` |
|     10 |  403 |  |
|      - |  404 | `/*` |
|      - |  405 | ` * float cosh(float $arg )` |
|      - |  406 | ` *  Hyperbolic cosine.` |
|      - |  407 | ` * Parameter` |
|      - |  408 | ` *  The number to process.` |
|      - |  409 | ` * Return` |
|      - |  410 | ` *  The hyperbolic cosine of arg.` |
|      - |  411 | ` */` |
|     18 |  412 | `static int PH7_builtin_cosh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  413 |  |
|      - |  414 | `	double r,x;` |
|     19 |  415 | `	if( nArg < 1 ){` |
|      - |  416 | `		/* Missing argument,return 0 */` |
|      3 |  417 | `		ph7_result_int(pCtx,0);` |
|      3 |  418 | `		return PH7_OK;` |
|      - |  419 | `	}` |
|     17 |  420 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  421 | `	/* Perform the requested operation */` |
|     17 |  422 | `	r = cosh(x);` |
|      - |  423 | `	/* store the result back */` |
|     17 |  424 | `	ph7_result_double(pCtx,r);` |
|     17 |  425 | `	return PH7_OK;` |
|     10 |  426 |  |
|      - |  427 | `/*` |
|      - |  428 | ` * float sin(float $arg )` |
|      - |  429 | ` *  Sine.` |
|      - |  430 | ` * Parameter` |
|      - |  431 | ` *  The number to process.` |
|      - |  432 | ` * Return` |
|      - |  433 | ` *  The sine of arg.` |
|      - |  434 | ` */` |
|      8 |  435 | `static int PH7_builtin_sin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  436 |  |
|      - |  437 | `	double r,x;` |
|      9 |  438 | `	if( nArg < 1 ){` |
|      - |  439 | `		/* Missing argument,return 0 */` |
|      7 |  440 | `		ph7_result_int(pCtx,0);` |
|      7 |  441 | `		return PH7_OK;` |
|      - |  442 | `	}` |
|      3 |  443 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  444 | `	/* Perform the requested operation */` |
|      3 |  445 | `	r = sin(x);` |
|      - |  446 | `	/* store the result back */` |
|      3 |  447 | `	ph7_result_double(pCtx,r);` |
|      3 |  448 | `	return PH7_OK;` |
|      5 |  449 |  |
|      - |  450 | `/*` |
|      - |  451 | ` * float asin(float $arg )` |
|      - |  452 | ` *  Arc sine.` |
|      - |  453 | ` * Parameter` |
|      - |  454 | ` *  The number to process.` |
|      - |  455 | ` * Return` |
|      - |  456 | ` *  The arc sine of arg.` |
|      - |  457 | ` */` |
|     14 |  458 | `static int PH7_builtin_asin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  459 |  |
|      - |  460 | `	double r,x;` |
|     15 |  461 | `	if( nArg < 1 ){` |
|      - |  462 | `		/* Missing argument,return 0 */` |
|      3 |  463 | `		ph7_result_int(pCtx,0);` |
|      3 |  464 | `		return PH7_OK;` |
|      - |  465 | `	}` |
|     13 |  466 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  467 | `	/* Perform the requested operation */` |
|     13 |  468 | `	r = asin(x);` |
|      - |  469 | `	/* store the result back */` |
|     13 |  470 | `	ph7_result_double(pCtx,r);` |
|     13 |  471 | `	return PH7_OK;` |
|      8 |  472 |  |
|      - |  473 | `/*` |
|      - |  474 | ` * float sinh(float $arg )` |
|      - |  475 | ` *  Hyperbolic sine.` |
|      - |  476 | ` * Parameter` |
|      - |  477 | ` *  The number to process.` |
|      - |  478 | ` * Return` |
|      - |  479 | ` *  The hyperbolic sine of arg.` |
|      - |  480 | ` */` |
|     20 |  481 | `static int PH7_builtin_sinh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  482 |  |
|      - |  483 | `	double r,x;` |
|     21 |  484 | `	if( nArg < 1 ){` |
|      - |  485 | `		/* Missing argument,return 0 */` |
|      3 |  486 | `		ph7_result_int(pCtx,0);` |
|      3 |  487 | `		return PH7_OK;` |
|      - |  488 | `	}` |
|     19 |  489 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  490 | `	/* Perform the requested operation */` |
|     19 |  491 | `	r = sinh(x);` |
|      - |  492 | `	/* store the result back */` |
|     19 |  493 | `	ph7_result_double(pCtx,r);` |
|     19 |  494 | `	return PH7_OK;` |
|     11 |  495 |  |
|      - |  496 | `/*` |
|      - |  497 | ` * float ceil(float $arg )` |
|      - |  498 | ` *  Round fractions up.` |
|      - |  499 | ` * Parameter` |
|      - |  500 | ` *  The number to process.` |
|      - |  501 | ` * Return` |
|      - |  502 | ` *  The next highest integer value by rounding up value if necessary.` |
|      - |  503 | ` */` |
|      6 |  504 | `static int PH7_builtin_ceil(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  505 |  |
|      - |  506 | `	double r,x;` |
|      7 |  507 | `	if( nArg < 1 ){` |
|      - |  508 | `		/* Missing argument,return 0 */` |
|      5 |  509 | `		ph7_result_int(pCtx,0);` |
|      5 |  510 | `		return PH7_OK;` |
|      - |  511 | `	}` |
|      3 |  512 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  513 | `	/* Perform the requested operation */` |
|      3 |  514 | `	r = ceil(x);` |
|      - |  515 | `	/* store the result back */` |
|      3 |  516 | `	ph7_result_double(pCtx,r);` |
|      3 |  517 | `	return PH7_OK;` |
|      4 |  518 |  |
|      - |  519 | `/*` |
|      - |  520 | ` * float tan(float $arg )` |
|      - |  521 | ` *  Tangent.` |
|      - |  522 | ` * Parameter` |
|      - |  523 | ` *  The number to process.` |
|      - |  524 | ` * Return` |
|      - |  525 | ` *  The tangent of arg.` |
|      - |  526 | ` */` |
|      6 |  527 | `static int PH7_builtin_tan(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  528 |  |
|      - |  529 | `	double r,x;` |
|      7 |  530 | `	if( nArg < 1 ){` |
|      - |  531 | `		/* Missing argument,return 0 */` |
|      3 |  532 | `		ph7_result_int(pCtx,0);` |
|      3 |  533 | `		return PH7_OK;` |
|      - |  534 | `	}` |
|      5 |  535 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  536 | `	/* Perform the requested operation */` |
|      5 |  537 | `	r = tan(x);` |
|      - |  538 | `	/* store the result back */` |
|      5 |  539 | `	ph7_result_double(pCtx,r);` |
|      5 |  540 | `	return PH7_OK;` |
|      4 |  541 |  |
|      - |  542 | `/*` |
|      - |  543 | ` * float atan(float $arg )` |
|      - |  544 | ` *  Arc tangent.` |
|      - |  545 | ` * Parameter` |
|      - |  546 | ` *  The number to process.` |
|      - |  547 | ` * Return` |
|      - |  548 | ` *  The arc tangent of arg.` |
|      - |  549 | ` */` |
|     16 |  550 | `static int PH7_builtin_atan(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  551 |  |
|      - |  552 | `	double r,x;` |
|     17 |  553 | `	if( nArg < 1 ){` |
|      - |  554 | `		/* Missing argument,return 0 */` |
|      5 |  555 | `		ph7_result_int(pCtx,0);` |
|      5 |  556 | `		return PH7_OK;` |
|      - |  557 | `	}` |
|     13 |  558 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  559 | `	/* Perform the requested operation */` |
|     13 |  560 | `	r = atan(x);` |
|      - |  561 | `	/* store the result back */` |
|     13 |  562 | `	ph7_result_double(pCtx,r);` |
|     13 |  563 | `	return PH7_OK;` |
|      9 |  564 |  |
|      - |  565 | `/*` |
|      - |  566 | ` * float tanh(float $arg )` |
|      - |  567 | ` *  Hyperbolic tangent.` |
|      - |  568 | ` * Parameter` |
|      - |  569 | ` *  The number to process.` |
|      - |  570 | ` * Return` |
|      - |  571 | ` *  The Hyperbolic tangent of arg.` |
|      - |  572 | ` */` |
|     20 |  573 | `static int PH7_builtin_tanh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  574 |  |
|      - |  575 | `	double r,x;` |
|     21 |  576 | `	if( nArg < 1 ){` |
|      - |  577 | `		/* Missing argument,return 0 */` |
|      3 |  578 | `		ph7_result_int(pCtx,0);` |
|      3 |  579 | `		return PH7_OK;` |
|      - |  580 | `	}` |
|     19 |  581 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  582 | `	/* Perform the requested operation */` |
|     19 |  583 | `	r = tanh(x);` |
|      - |  584 | `	/* store the result back */` |
|     19 |  585 | `	ph7_result_double(pCtx,r);` |
|     19 |  586 | `	return PH7_OK;` |
|     11 |  587 |  |
|      - |  588 | `/*` |
|      - |  589 | ` * float atan2(float $y,float $x)` |
|      - |  590 | ` *  Arc tangent of two variable.` |
|      - |  591 | ` * Parameter` |
|      - |  592 | ` *  $y = Dividend parameter.` |
|      - |  593 | ` *  $x = Divisor parameter.` |
|      - |  594 | ` * Return` |
|      - |  595 | ` *  The arc tangent of y/x in radian.` |
|      - |  596 | ` */` |
|     10 |  597 | `static int PH7_builtin_atan2(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  598 |  |
|      - |  599 | `	double r,x,y;` |
|     11 |  600 | `	if( nArg < 2 ){` |
|      - |  601 | `		/* Missing arguments,return 0 */` |
|      5 |  602 | `		ph7_result_int(pCtx,0);` |
|      5 |  603 | `		return PH7_OK;` |
|      - |  604 | `	}` |
|      7 |  605 | `	y = ph7_value_to_double(apArg[0]);` |
|      7 |  606 | `	x = ph7_value_to_double(apArg[1]);` |
|      - |  607 | `	/* Perform the requested operation */` |
|      7 |  608 | `	r = atan2(y,x);` |
|      - |  609 | `	/* store the result back */` |
|      7 |  610 | `	ph7_result_double(pCtx,r);` |
|      7 |  611 | `	return PH7_OK;` |
|      6 |  612 |  |
|      - |  613 | `/*` |
|      - |  614 | ` * float/int64 abs(float/int64 $arg )` |
|      - |  615 | ` *  Absolute value.` |
|      - |  616 | ` * Parameter` |
|      - |  617 | ` *  The number to process.` |
|      - |  618 | ` * Return` |
|      - |  619 | ` *  The absolute value of number.` |
|      - |  620 | ` */` |
|     76 |  621 | `static int PH7_builtin_abs(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  622 |  |
|      - |  623 | `	int is_float;` |
|     77 |  624 | `	if( nArg < 1 ){` |
|      - |  625 | `		/* Missing argument,return 0 */` |
|      3 |  626 | `		ph7_result_int(pCtx,0);` |
|      3 |  627 | `		return PH7_OK;` |
|      - |  628 | `	}` |
|     75 |  629 | `	is_float = ph7_value_is_float(apArg[0]);` |
|     75 |  630 | `	if( is_float ){` |
|      - |  631 | `		double r,x;` |
|     71 |  632 | `		x = ph7_value_to_double(apArg[0]);` |
|      - |  633 | `		/* Perform the requested operation */` |
|     71 |  634 | `		r = fabs(x);` |
|     71 |  635 | `		ph7_result_double(pCtx,r);` |
|     36 |  636 | `	}else{` |
|      - |  637 | `		int r,x;` |
|      5 |  638 | `		x = ph7_value_to_int(apArg[0]);` |
|      - |  639 | `		/* Perform the requested operation */` |
|      5 |  640 | `		r = abs(x);` |
|      5 |  641 | `		ph7_result_int(pCtx,r);` |
|      - |  642 | `	}` |
|     75 |  643 | `	return PH7_OK;` |
|     39 |  644 |  |
|      - |  645 | `/*` |
|      - |  646 | ` * float log(float $arg,[int/float $base])` |
|      - |  647 | ` *  Natural logarithm.` |
|      - |  648 | ` * Parameter` |
|      - |  649 | ` *  $arg: The number to process.` |
|      - |  650 | ` *  $base: The optional logarithmic base to use. (only base-10 is supported)` |
|      - |  651 | ` * Return` |
|      - |  652 | ` *  The logarithm of arg to base, if given, or the natural logarithm.` |
|      - |  653 | ` * Note:` |
|      - |  654 | ` *  only Natural log and base-10 log are supported.` |
|      - |  655 | ` */` |
|     14 |  656 | `static int PH7_builtin_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  657 |  |
|      - |  658 | `	double r,x;` |
|     15 |  659 | `	if( nArg < 1 ){` |
|      - |  660 | `		/* Missing argument,return 0 */` |
|      3 |  661 | `		ph7_result_int(pCtx,0);` |
|      3 |  662 | `		return PH7_OK;` |
|      - |  663 | `	}` |
|     13 |  664 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  665 | `	/* Perform the requested operation */` |
|     13 |  666 | `	if( nArg == 2 && ph7_value_is_numeric(apArg[1]) && ph7_value_to_int(apArg[1]) == 10 ){` |
|      - |  667 | `		/* Base-10 log */` |
|      5 |  668 | `		r = log10(x);` |
|      3 |  669 | `	}else{` |
|      9 |  670 | `		r = log(x);` |
|      - |  671 | `	}` |
|      - |  672 | `	/* store the result back */` |
|     13 |  673 | `	ph7_result_double(pCtx,r);` |
|     13 |  674 | `	return PH7_OK;` |
|      8 |  675 |  |
|      - |  676 | `/*` |
|      - |  677 | ` * float log10(float $arg )` |
|      - |  678 | ` *  Base-10 logarithm.` |
|      - |  679 | ` * Parameter` |
|      - |  680 | ` *  The number to process.` |
|      - |  681 | ` * Return` |
|      - |  682 | ` *  The Base-10 logarithm of the given number.` |
|      - |  683 | ` */` |
|     16 |  684 | `static int PH7_builtin_log10(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  685 |  |
|      - |  686 | `	double r,x;` |
|     17 |  687 | `	if( nArg < 1 ){` |
|      - |  688 | `		/* Missing argument,return 0 */` |
|      3 |  689 | `		ph7_result_int(pCtx,0);` |
|      3 |  690 | `		return PH7_OK;` |
|      - |  691 | `	}` |
|     15 |  692 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  693 | `	/* Perform the requested operation */` |
|     15 |  694 | `	r = log10(x);` |
|      - |  695 | `	/* store the result back */` |
|     15 |  696 | `	ph7_result_double(pCtx,r);` |
|     15 |  697 | `	return PH7_OK;` |
|      9 |  698 |  |
|      - |  699 | `/*` |
|      - |  700 | ` * number pow(number $base,number $exp)` |
|      - |  701 | ` *  Exponential expression.` |
|      - |  702 | ` * Parameter` |
|      - |  703 | ` *  base` |
|      - |  704 | ` *  The base to use.` |
|      - |  705 | ` * exp` |
|      - |  706 | ` *  The exponent.` |
|      - |  707 | ` * Return` |
|      - |  708 | ` *  base raised to the power of exp.` |
|      - |  709 | ` *  If the result can be represented as integer it will be returned` |
|      - |  710 | ` *  as type integer, else it will be returned as type float.` |
|      - |  711 | ` */` |
|      8 |  712 | `static int PH7_builtin_pow(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  713 |  |
|      - |  714 | `	double r,x,y;` |
|      9 |  715 | `	if( nArg < 1 ){` |
|      - |  716 | `		/* Missing argument,return 0 */` |
|      5 |  717 | `		ph7_result_int(pCtx,0);` |
|      5 |  718 | `		return PH7_OK;` |
|      - |  719 | `	}` |
|      5 |  720 | `	x = ph7_value_to_double(apArg[0]);` |
|      5 |  721 | `	y = ph7_value_to_double(apArg[1]);` |
|      - |  722 | `	/* Perform the requested operation */` |
|      5 |  723 | `	r = pow(x,y);` |
|      5 |  724 | `	ph7_result_double(pCtx,r);` |
|      5 |  725 | `	return PH7_OK;` |
|      5 |  726 |  |
|      - |  727 | `/*` |
|      - |  728 | ` * float pi(void)` |
|      - |  729 | ` *  Returns an approximation of pi.` |
|      - |  730 | ` * Note` |
|      - |  731 | ` *  you can use the M_PI constant which yields identical results to pi().` |
|      - |  732 | ` * Return` |
|      - |  733 | ` *  The value of pi as float.` |
|      - |  734 | ` */` |
|      2 |  735 | `static int PH7_builtin_pi(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  736 |  |
|      1 |  737 | `	SXUNUSED(nArg); /* cc warning */` |
|      1 |  738 | `	SXUNUSED(apArg);` |
|      3 |  739 | `	ph7_result_double(pCtx,PH7_PI);` |
|      3 |  740 | `	return PH7_OK;` |
|      1 |  741 |  |
|      - |  742 | `/*` |
|      - |  743 | ` * float fmod(float $x,float $y)` |
|      - |  744 | ` *  Returns the floating point remainder (modulo) of the division of the arguments.` |
|      - |  745 | ` * Parameters` |
|      - |  746 | ` * $x` |
|      - |  747 | ` *  The dividend` |
|      - |  748 | ` * $y` |
|      - |  749 | ` *  The divisor` |
|      - |  750 | ` * Return` |
|      - |  751 | ` *  The floating point remainder of x/y.` |
|      - |  752 | ` */` |
|      8 |  753 | `static int PH7_builtin_fmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  754 |  |
|      - |  755 | `	double x,y,r;` |
|      9 |  756 | `	if( nArg < 2 ){` |
|      - |  757 | `		/* Missing arguments */` |
|      7 |  758 | `		ph7_result_double(pCtx,0);` |
|      7 |  759 | `		return PH7_OK;` |
|      - |  760 | `	}` |
|      - |  761 | `	/* Extract given arguments */` |
|      3 |  762 | `	x = ph7_value_to_double(apArg[0]);` |
|      3 |  763 | `	y = ph7_value_to_double(apArg[1]);` |
|      - |  764 | `	/* Perform the requested operation */` |
|      3 |  765 | `	r = fmod(x,y);` |
|      - |  766 | `	/* Processing result */` |
|      3 |  767 | `	ph7_result_double(pCtx,r);` |
|      3 |  768 | `	return PH7_OK;` |
|      5 |  769 |  |
|      - |  770 | `/*` |
|      - |  771 | ` * float hypot(float $x,float $y)` |
|      - |  772 | ` *  Calculate the length of the hypotenuse of a right-angle triangle .` |
|      - |  773 | ` * Parameters` |
|      - |  774 | ` * $x` |
|      - |  775 | ` *  Length of first side` |
|      - |  776 | ` * $y` |
|      - |  777 | ` *  Length of first side` |
|      - |  778 | ` * Return` |
|      - |  779 | ` *  Calculated length of the hypotenuse.` |
|      - |  780 | ` */` |
|      6 |  781 | `static int PH7_builtin_hypot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  782 |  |
|      - |  783 | `	double x,y,r;` |
|      7 |  784 | `	if( nArg < 2 ){` |
|      - |  785 | `		/* Missing arguments */` |
|      5 |  786 | `		ph7_result_double(pCtx,0);` |
|      5 |  787 | `		return PH7_OK;` |
|      - |  788 | `	}` |
|      - |  789 | `	/* Extract given arguments */` |
|      3 |  790 | `	x = ph7_value_to_double(apArg[0]);` |
|      3 |  791 | `	y = ph7_value_to_double(apArg[1]);` |
|      - |  792 | `	/* Perform the requested operation */` |
|      3 |  793 | `	r = hypot(x,y);` |
|      - |  794 | `	/* Processing result */` |
|      3 |  795 | `	ph7_result_double(pCtx,r);` |
|      3 |  796 | `	return PH7_OK;` |
|      4 |  797 |  |
|      - |  798 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - |  799 | `/*` |
|      - |  800 | ` * float round ( float $val [, int $precision = 0 [, int $mode = PHP_ROUND_HALF_UP ]] )` |
|      - |  801 | ` *  Exponential expression.` |
|      - |  802 | ` * Parameter` |
|      - |  803 | ` *  $val` |
|      - |  804 | ` *   The value to round.` |
|      - |  805 | ` * $precision` |
|      - |  806 | ` *   The optional number of decimal digits to round to.` |
|      - |  807 | ` * $mode` |
|      - |  808 | ` *   One of PHP_ROUND_HALF_UP, PHP_ROUND_HALF_DOWN, PHP_ROUND_HALF_EVEN, or PHP_ROUND_HALF_ODD.` |
|      - |  809 | ` *   (not supported).` |
|      - |  810 | ` * Return` |
|      - |  811 | ` *  The rounded value.` |
|      - |  812 | ` */` |
|     20 |  813 | `static int PH7_builtin_round(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  814 |  |
|     21 |  815 | `	int n = 0;` |
|      - |  816 | `	double r;` |
|     21 |  817 | `	if( nArg < 1 ){` |
|      - |  818 | `		/* Missing argument,return 0 */` |
|      5 |  819 | `		ph7_result_int(pCtx,0);` |
|      5 |  820 | `		return PH7_OK;` |
|      - |  821 | `	}` |
|      - |  822 | `	/* Extract the precision if available */` |
|     17 |  823 | `	if( nArg > 1 ){` |
|      5 |  824 | `		n = ph7_value_to_int(apArg[1]);` |
|      5 |  825 | `		if( n>30 ){` |
|      3 |  826 | `			n = 30;` |
|      1 |  827 | `		}` |
|      5 |  828 | `		if( n<0 ){` |
|      3 |  829 | `			n = 0;` |
|      1 |  830 | `		}` |
|      2 |  831 | `	}` |
|     17 |  832 | `	r = ph7_value_to_double(apArg[0]);` |
|      - |  833 | `	/* If Y==0 and X will fit in a 64-bit int,` |
|      - |  834 | `     * handle the rounding directly.Otherwise` |
|      - |  835 | `	 * use our own cutsom printf [i.e:SyBufferFormat()].` |
|      - |  836 | `     */` |
|     17 |  837 | `	if( n==0 && r>=0 && r < (double)(LARGEST_INT64-1) ){` |
|     13 |  838 | `    r = (double)((ph7_int64)(r+0.5));` |
|     11 |  839 | `	}else if( n==0 && r<0 && (-r) < (double)(LARGEST_INT64-1) ){` |
|      3 |  840 | `    r = -(double)((ph7_int64)((-r)+0.5));` |
|      2 |  841 | `  }else{` |
|      - |  842 | `	  char zBuf[256];` |
|      - |  843 | `	  sxu32 nLen;` |
|      3 |  844 | `	  nLen = SyBufferFormat(zBuf,sizeof(zBuf),"%.*f",n,r);` |
|      - |  845 | `	  /* Convert the string to real number */` |
|      3 |  846 | `	  SyStrToReal(zBuf,nLen,(void *)&r,0);` |
|      - |  847 | `  }` |
|      - |  848 | `  /* Return thr rounded value */` |
|     17 |  849 | `  ph7_result_double(pCtx,r);` |
|     17 |  850 | `  return PH7_OK;` |
|     11 |  851 |  |
|      - |  852 | `/*` |
|      - |  853 | ` * string dechex(int $number)` |
|      - |  854 | ` *  Decimal to hexadecimal.` |
|      - |  855 | ` * Parameters` |
|      - |  856 | ` *  $number` |
|      - |  857 | ` *   Decimal value to convert` |
|      - |  858 | ` * Return` |
|      - |  859 | ` *  Hexadecimal string representation of number` |
|      - |  860 | ` */` |
|      6 |  861 | `static int PH7_builtin_dechex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  862 |  |
|      - |  863 | `	int iVal;` |
|      7 |  864 | `	if( nArg < 1 ){` |
|      - |  865 | `		/* Missing arguments,return null */` |
|      5 |  866 | `		ph7_result_null(pCtx);` |
|      5 |  867 | `		return PH7_OK;` |
|      - |  868 | `	}` |
|      - |  869 | `	/* Extract the given number */` |
|      3 |  870 | `	iVal = ph7_value_to_int(apArg[0]);` |
|      - |  871 | `	/* Format */` |
|      3 |  872 | `	ph7_result_string_format(pCtx,"%x",iVal);` |
|      3 |  873 | `	return PH7_OK;` |
|      4 |  874 |  |
|      - |  875 | `/*` |
|      - |  876 | ` * string decoct(int $number)` |
|      - |  877 | ` *  Decimal to Octal.` |
|      - |  878 | ` * Parameters` |
|      - |  879 | ` *  $number` |
|      - |  880 | ` *   Decimal value to convert` |
|      - |  881 | ` * Return` |
|      - |  882 | ` *  Octal string representation of number` |
|      - |  883 | ` */` |
|      8 |  884 | `static int PH7_builtin_decoct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  885 |  |
|      - |  886 | `	int iVal;` |
|      9 |  887 | `	if( nArg < 1 ){` |
|      - |  888 | `		/* Missing arguments,return null */` |
|      3 |  889 | `		ph7_result_null(pCtx);` |
|      3 |  890 | `		return PH7_OK;` |
|      - |  891 | `	}` |
|      - |  892 | `	/* Extract the given number */` |
|      7 |  893 | `	iVal = ph7_value_to_int(apArg[0]);` |
|      - |  894 | `	/* Format */` |
|      7 |  895 | `	ph7_result_string_format(pCtx,"%o",iVal);` |
|      7 |  896 | `	return PH7_OK;` |
|      5 |  897 |  |
|      - |  898 | `/*` |
|      - |  899 | ` * string decbin(int $number)` |
|      - |  900 | ` *  Decimal to binary.` |
|      - |  901 | ` * Parameters` |
|      - |  902 | ` *  $number` |
|      - |  903 | ` *   Decimal value to convert` |
|      - |  904 | ` * Return` |
|      - |  905 | ` *  Binary string representation of number` |
|      - |  906 | ` */` |
|      4 |  907 | `static int PH7_builtin_decbin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  908 |  |
|      - |  909 | `	int iVal;` |
|      5 |  910 | `	if( nArg < 1 ){` |
|      - |  911 | `		/* Missing arguments,return null */` |
|      3 |  912 | `		ph7_result_null(pCtx);` |
|      3 |  913 | `		return PH7_OK;` |
|      - |  914 | `	}` |
|      - |  915 | `	/* Extract the given number */` |
|      3 |  916 | `	iVal = ph7_value_to_int(apArg[0]);` |
|      - |  917 | `	/* Format */` |
|      3 |  918 | `	ph7_result_string_format(pCtx,"%B",iVal);` |
|      3 |  919 | `	return PH7_OK;` |
|      3 |  920 |  |
|      - |  921 | `/*` |
|      - |  922 | ` * int64 hexdec(string $hex_string)` |
|      - |  923 | ` *  Hexadecimal to decimal.` |
|      - |  924 | ` * Parameters` |
|      - |  925 | ` *  $hex_string` |
|      - |  926 | ` *   The hexadecimal string to convert` |
|      - |  927 | ` * Return` |
|      - |  928 | ` *  The decimal representation of hex_string` |
|      - |  929 | ` */` |
|     24 |  930 | `static int PH7_builtin_hexdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  931 |  |
|      - |  932 | `	const char *zString,*zEnd;` |
|      - |  933 | `	ph7_int64 iVal;` |
|      - |  934 | `	int nLen;` |
|     25 |  935 | `	if( nArg < 1 ){` |
|      - |  936 | `		/* Missing arguments,return -1 */` |
|      5 |  937 | `		ph7_result_int(pCtx,-1);` |
|      5 |  938 | `		return PH7_OK;` |
|      - |  939 | `	}` |
|     21 |  940 | `	iVal = 0;` |
|     21 |  941 | `	if( ph7_value_is_string(apArg[0]) ){` |
|      - |  942 | `		/* Extract the given string */` |
|     15 |  943 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|      - |  944 | `		/* Delimit the string */` |
|     15 |  945 | `		zEnd = &zString[nLen];` |
|      - |  946 | `		/* Ignore non hex-stream */` |
|     21 |  947 | `		while( zString < zEnd ){` |
|     21 |  948 | `			if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - |  949 | `				/* UTF-8 stream */` |
|      5 |  950 | `				zString++;` |
|      9 |  951 | `				while( zString < zEnd && (((unsigned char)zString[0] & 0xc0) == 0x80) ){` |
|      5 |  952 | `					zString++;` |
|      1 |  953 | `				}` |
|      3 |  954 | `			}else{` |
|     17 |  955 | `				if( SyisHex(zString[0]) ){` |
|     15 |  956 | `					break;` |
|      - |  957 | `				}` |
|      - |  958 | `				/* Ignore */` |
|      3 |  959 | `				zString++;` |
|      - |  960 | `			}` |
|      1 |  961 | `		}` |
|     15 |  962 | `		if( zString < zEnd ){` |
|      - |  963 | `			/* Cast */` |
|     15 |  964 | `			SyHexStrToInt64(zString,(sxu32)(zEnd-zString),(void *)&iVal,0);` |
|      7 |  965 | `		}` |
|      8 |  966 | `	}else{` |
|      - |  967 | `		/* Extract as a 64-bit integer */` |
|      7 |  968 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|      - |  969 | `	}` |
|      - |  970 | `	/* Return the number */` |
|     21 |  971 | `	ph7_result_int64(pCtx,iVal);` |
|     21 |  972 | `	return PH7_OK;` |
|     13 |  973 |  |
|      - |  974 | `/*` |
|      - |  975 | ` * int64 bindec(string $bin_string)` |
|      - |  976 | ` *  Binary to decimal.` |
|      - |  977 | ` * Parameters` |
|      - |  978 | ` *  $bin_string` |
|      - |  979 | ` *   The binary string to convert` |
|      - |  980 | ` * Return` |
|      - |  981 | ` *  Returns the decimal equivalent of the binary number represented by the binary_string argument.` |
|      - |  982 | ` */` |
|     12 |  983 | `static int PH7_builtin_bindec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  984 |  |
|      - |  985 | `	const char *zString;` |
|      - |  986 | `	ph7_int64 iVal;` |
|      - |  987 | `	int nLen;` |
|     13 |  988 | `	if( nArg < 1 ){` |
|      - |  989 | `		/* Missing arguments,return -1 */` |
|      5 |  990 | `		ph7_result_int(pCtx,-1);` |
|      5 |  991 | `		return PH7_OK;` |
|      - |  992 | `	}` |
|      9 |  993 | `	iVal = 0;` |
|      9 |  994 | `	if( ph7_value_is_string(apArg[0]) ){` |
|      - |  995 | `		/* Extract the given string */` |
|      5 |  996 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 |  997 | `		if( nLen > 0 ){` |
|      - |  998 | `			/* Perform a binary cast */` |
|      5 |  999 | `			SyBinaryStrToInt64(zString,(sxu32)nLen,(void *)&iVal,0);` |
|      2 | 1000 | `		}` |
|      3 | 1001 | `	}else{` |
|      - | 1002 | `		/* Extract as a 64-bit integer */` |
|      5 | 1003 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|      - | 1004 | `	}` |
|      - | 1005 | `	/* Return the number */` |
|      9 | 1006 | `	ph7_result_int64(pCtx,iVal);` |
|      9 | 1007 | `	return PH7_OK;` |
|      7 | 1008 |  |
|      - | 1009 | `/*` |
|      - | 1010 | ` * int64 octdec(string $oct_string)` |
|      - | 1011 | ` *  Octal to decimal.` |
|      - | 1012 | ` * Parameters` |
|      - | 1013 | ` *  $oct_string` |
|      - | 1014 | ` *   The octal string to convert` |
|      - | 1015 | ` * Return` |
|      - | 1016 | ` *  Returns the decimal equivalent of the octal number represented by the octal_string argument.` |
|      - | 1017 | ` */` |
|      6 | 1018 | `static int PH7_builtin_octdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1019 |  |
|      - | 1020 | `	const char *zString;` |
|      - | 1021 | `	ph7_int64 iVal;` |
|      - | 1022 | `	int nLen;` |
|      7 | 1023 | `	if( nArg < 1 ){` |
|      - | 1024 | `		/* Missing arguments,return -1 */` |
|      3 | 1025 | `		ph7_result_int(pCtx,-1);` |
|      3 | 1026 | `		return PH7_OK;` |
|      - | 1027 | `	}` |
|      5 | 1028 | `	iVal = 0;` |
|      5 | 1029 | `	if( ph7_value_is_string(apArg[0]) ){` |
|      - | 1030 | `		/* Extract the given string */` |
|      3 | 1031 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 1032 | `		if( nLen > 0 ){` |
|      - | 1033 | `			/* Perform the cast */` |
|      3 | 1034 | `			SyOctalStrToInt64(zString,(sxu32)nLen,(void *)&iVal,0);` |
|      1 | 1035 | `		}` |
|      2 | 1036 | `	}else{` |
|      - | 1037 | `		/* Extract as a 64-bit integer */` |
|      3 | 1038 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|      - | 1039 | `	}` |
|      - | 1040 | `	/* Return the number */` |
|      5 | 1041 | `	ph7_result_int64(pCtx,iVal);` |
|      5 | 1042 | `	return PH7_OK;` |
|      4 | 1043 |  |
|      - | 1044 | `/*` |
|      - | 1045 | ` * srand([int $seed])` |
|      - | 1046 | ` * mt_srand([int $seed])` |
|      - | 1047 | ` *  Seed the random number generator.` |
|      - | 1048 | ` * Parameters` |
|      - | 1049 | ` * $seed` |
|      - | 1050 | ` *  Optional seed value` |
|      - | 1051 | ` * Return` |
|      - | 1052 | ` *  null.` |
|      - | 1053 | ` * Note:` |
|      - | 1054 | ` *  THIS FUNCTION IS A NO-OP.` |
|      - | 1055 | ` *  THE PH7 PRNG IS AUTOMATICALLY SEEDED WHEN THE VM IS CREATED.` |
|      - | 1056 | ` */` |
|     20 | 1057 | `static int PH7_builtin_srand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1058 |  |
|     10 | 1059 | `	SXUNUSED(nArg);` |
|     10 | 1060 | `	SXUNUSED(apArg);` |
|     21 | 1061 | `	ph7_result_null(pCtx);` |
|     21 | 1062 | `	return PH7_OK;` |
|      1 | 1063 |  |
|      - | 1064 | `/*` |
|      - | 1065 | ` * string base_convert(string $number,int $frombase,int $tobase)` |
|      - | 1066 | ` *  Convert a number between arbitrary bases.` |
|      - | 1067 | ` * Parameters` |
|      - | 1068 | ` * $number` |
|      - | 1069 | ` *  The number to convert` |
|      - | 1070 | ` * $frombase` |
|      - | 1071 | ` *  The base number is in` |
|      - | 1072 | ` * $tobase` |
|      - | 1073 | ` *  The base to convert number to` |
|      - | 1074 | ` * Return` |
|      - | 1075 | ` *  Number converted to base tobase` |
|      - | 1076 | ` */` |
|     48 | 1077 | `static int PH7_builtin_base_convert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1078 |  |
|      - | 1079 | `	int nLen,iFbase,iTobase;` |
|      - | 1080 | `	const char *zNum;` |
|      - | 1081 | `	ph7_int64 iNum;` |
|     49 | 1082 | `	if( nArg < 3 ){` |
|      - | 1083 | `		/* Return the empty string*/` |
|     13 | 1084 | `		ph7_result_string(pCtx,"",0);` |
|     13 | 1085 | `		return PH7_OK;` |
|      - | 1086 | `	}` |
|      - | 1087 | `	/* Base numbers */` |
|     37 | 1088 | `	iFbase  = ph7_value_to_int(apArg[1]);` |
|     37 | 1089 | `	iTobase = ph7_value_to_int(apArg[2]);` |
|     37 | 1090 | `	if( ph7_value_is_string(apArg[0]) ){` |
|      - | 1091 | `		/* Extract the target number */` |
|     29 | 1092 | `		zNum = ph7_value_to_string(apArg[0],&nLen);` |
|     29 | 1093 | `		if( nLen < 1 ){` |
|      - | 1094 | `			/* Return the empty string*/` |
|    ! 0 | 1095 | `			ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1096 | `			return PH7_OK;` |
|      - | 1097 | `		}` |
|      - | 1098 | `		/* Base conversion */` |
|     29 | 1099 | `		switch(iFbase){` |
|      5 | 1100 | `		case 16:` |
|      - | 1101 | `			/* Hex */` |
|     11 | 1102 | `			SyHexStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|     11 | 1103 | `			break;` |
|      3 | 1104 | `		case 8:` |
|      - | 1105 | `			/* Octal */` |
|      7 | 1106 | `			SyOctalStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|      7 | 1107 | `			break;` |
|      2 | 1108 | `		case 2:` |
|      - | 1109 | `			/* Binary */` |
|      5 | 1110 | `			SyBinaryStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|      5 | 1111 | `			break;` |
|      4 | 1112 | `		default:` |
|      - | 1113 | `			/* Decimal */` |
|      9 | 1114 | `			SyStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|      8 | 1115 | `			break;` |
|      - | 1116 | `		}` |
|     15 | 1117 | `	}else{` |
|      9 | 1118 | `		iNum = ph7_value_to_int64(apArg[0]);` |
|      - | 1119 | `	}` |
|     37 | 1120 | `	switch(iTobase){` |
|      4 | 1121 | `	case 16:` |
|      - | 1122 | `		/* Hex */` |
|      9 | 1123 | `		ph7_result_string_format(pCtx,"%qx",iNum); /* Quad hex */` |
|      9 | 1124 | `		break;` |
|      1 | 1125 | `	case 8:` |
|      - | 1126 | `		/* Octal */` |
|      3 | 1127 | `		ph7_result_string_format(pCtx,"%qo",iNum); /* Quad octal */` |
|      3 | 1128 | `		break;` |
|      1 | 1129 | `	case 2:` |
|      - | 1130 | `		/* Binary */` |
|      3 | 1131 | `		ph7_result_string_format(pCtx,"%qB",iNum); /* Quad binary */` |
|      3 | 1132 | `		break;` |
|     12 | 1133 | `	default:` |
|      - | 1134 | `		/* Decimal */` |
|     25 | 1135 | `		ph7_result_string_format(pCtx,"%qd",iNum); /* Quad decimal */` |
|     24 | 1136 | `		break;` |
|      - | 1137 | `	}` |
|     37 | 1138 | `	return PH7_OK;` |
|     25 | 1139 |  |
|      - | 1140 | `/*` |
|      - | 1141 | ` * Section:` |
|      - | 1142 | ` *    String handling Functions.` |
|      - | 1143 | ` * Status:` |
|      - | 1144 | ` *    Stable.` |
|      - | 1145 | ` */` |
|      - | 1146 | `/*` |
|      - | 1147 | ` * string substr(string $string,int $start[, int $length ])` |
|      - | 1148 | ` *  Return part of a string.` |
|      - | 1149 | ` * Parameters` |
|      - | 1150 | ` *  $string` |
|      - | 1151 | ` *   The input string. Must be one character or longer.` |
|      - | 1152 | ` * $start` |
|      - | 1153 | ` *   If start is non-negative, the returned string will start at the start'th position` |
|      - | 1154 | ` *   in string, counting from zero. For instance, in the string 'abcdef', the character` |
|      - | 1155 | ` *   at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 1156 | ` *   If start is negative, the returned string will start at the start'th character` |
|      - | 1157 | ` *   from the end of string.` |
|      - | 1158 | ` *   If string is less than or equal to start characters long, FALSE will be returned.` |
|      - | 1159 | ` * $length` |
|      - | 1160 | ` *   If length is given and is positive, the string returned will contain at most length` |
|      - | 1161 | ` *   characters beginning from start (depending on the length of string).` |
|      - | 1162 | ` *   If length is given and is negative, then that many characters will be omitted from` |
|      - | 1163 | ` *   the end of string (after the start position has been calculated when a start is negative).` |
|      - | 1164 | ` *   If start denotes the position of this truncation or beyond, false will be returned.` |
|      - | 1165 | ` *   If length is given and is 0, FALSE or NULL an empty string will be returned.` |
|      - | 1166 | ` *   If length is omitted, the substring starting from start until the end of the string` |
|      - | 1167 | ` *   will be returned.` |
|      - | 1168 | ` * Return` |
|      - | 1169 | ` *  Returns the extracted part of string, or FALSE on failure or an empty string.` |
|      - | 1170 | ` */` |
|  93818 | 1171 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1172 |  |
|      - | 1173 | `	const char *zSource,*zOfft;` |
|      - | 1174 | `	int nOfft,nLen,nSrcLen;` |
|  93819 | 1175 | `	if( nArg < 2 ){` |
|      - | 1176 | `		/* return FALSE */` |
|      5 | 1177 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1178 | `		return PH7_OK;` |
|      - | 1179 | `	}` |
|      - | 1180 | `	/* Extract the target string */` |
|  93815 | 1181 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|  93815 | 1182 | `	if( nSrcLen < 1 ){` |
|      - | 1183 | `		/* Empty string,return FALSE */` |
|   5993 | 1184 | `		ph7_result_bool(pCtx,0);` |
|   5993 | 1185 | `		return PH7_OK;` |
|      - | 1186 | `	}` |
|  87823 | 1187 | `	nLen = nSrcLen; /* cc warning */` |
|      - | 1188 | `	/* Extract the offset */` |
|  87823 | 1189 | `	nOfft = ph7_value_to_int(apArg[1]);` |
|  87823 | 1190 | `	if( nOfft < 0 ){` |
|  14373 | 1191 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  14373 | 1192 | `		if( zOfft < zSource ){` |
|      - | 1193 | `			/* Invalid offset */` |
|      5 | 1194 | `			ph7_result_bool(pCtx,0);` |
|      5 | 1195 | `			return PH7_OK;` |
|      - | 1196 | `		}` |
|  14369 | 1197 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  14369 | 1198 | `		nOfft = (int)(zOfft-zSource);` |
|  80635 | 1199 | `	}else if( nOfft >= nSrcLen ){` |
|      - | 1200 | `		/* Invalid offset */` |
|      7 | 1201 | `		ph7_result_bool(pCtx,0);` |
|      7 | 1202 | `		return PH7_OK;` |
|    ! 0 | 1203 | `	}else{` |
|  73445 | 1204 | `		zOfft = &zSource[nOfft];` |
|  73445 | 1205 | `		nLen = nSrcLen - nOfft;` |
|      - | 1206 | `	}` |
|  87813 | 1207 | `	if( nArg > 2 ){` |
|      - | 1208 | `		/* Extract the length */` |
|  73443 | 1209 | `		nLen = ph7_value_to_int(apArg[2]);` |
|  73443 | 1210 | `		if( nLen == 0 ){` |
|      - | 1211 | `			/* Invalid length,return an empty string */` |
|      5 | 1212 | `			ph7_result_string(pCtx,"",0);` |
|      5 | 1213 | `			return PH7_OK;` |
|  73439 | 1214 | `		}else if( nLen < 0 ){` |
|  14371 | 1215 | `			nLen = nSrcLen + nLen - nOfft;` |
|  14371 | 1216 | `			if( nLen < 1 ){` |
|      - | 1217 | `				/* Invalid  length */` |
|      3 | 1218 | `				nLen = nSrcLen - nOfft;` |
|      1 | 1219 | `			}` |
|   7185 | 1220 | `		}` |
|  73439 | 1221 | `		if( nLen + nOfft > nSrcLen ){` |
|      - | 1222 | `			/* Invalid length */` |
|   1821 | 1223 | `			nLen = nSrcLen - nOfft;` |
|    910 | 1224 | `		}` |
|  36719 | 1225 | `	}` |
|      - | 1226 | `	/* Return the substring */` |
|  87809 | 1227 | `	ph7_result_string(pCtx,zOfft,nLen);` |
|  87809 | 1228 | `	return PH7_OK;` |
|  46910 | 1229 |  |
|      - | 1230 | `/*` |
|      - | 1231 | ` * int substr_compare(string $main_str,string $str ,int $offset[,int $length[,bool $case_insensitivity = false ]])` |
|      - | 1232 | ` *  Binary safe comparison of two strings from an offset, up to length characters.` |
|      - | 1233 | ` * Parameters` |
|      - | 1234 | ` *  $main_str` |
|      - | 1235 | ` *  The main string being compared.` |
|      - | 1236 | ` *  $str` |
|      - | 1237 | ` *   The secondary string being compared.` |
|      - | 1238 | ` * $offset` |
|      - | 1239 | ` *  The start position for the comparison. If negative, it starts counting from` |
|      - | 1240 | ` *  the end of the string.` |
|      - | 1241 | ` * $length` |
|      - | 1242 | ` *  The length of the comparison. The default value is the largest of the length` |
|      - | 1243 | ` *  of the str compared to the length of main_str less the offset.` |
|      - | 1244 | ` * $case_insensitivity` |
|      - | 1245 | ` *  If case_insensitivity is TRUE, comparison is case insensitive.` |
|      - | 1246 | ` * Return` |
|      - | 1247 | ` *  Returns < 0 if main_str from position offset is less than str, > 0 if it is greater than` |
|      - | 1248 | ` *  str, and 0 if they are equal. If offset is equal to or greater than the length of main_str` |
|      - | 1249 | ` *  or length is set and is less than 1, substr_compare() prints a warning and returns FALSE.` |
|      - | 1250 | ` */` |
|     26 | 1251 | `static int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1252 |  |
|      - | 1253 | `	const char *zSource,*zOfft,*zSub;` |
|      - | 1254 | `	int nOfft,nLen,nSrcLen,nSublen;` |
|     27 | 1255 | `	int iCase = 0;` |
|      - | 1256 | `	int rc;` |
|     27 | 1257 | `	if( nArg < 3 ){` |
|      - | 1258 | `		/* Missing arguments,return FALSE */` |
|      5 | 1259 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1260 | `		return PH7_OK;` |
|      - | 1261 | `	}` |
|      - | 1262 | `	/* Extract the target string */` |
|     23 | 1263 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|     23 | 1264 | `	if( nSrcLen < 1 ){` |
|      - | 1265 | `		/* Empty string,return FALSE */` |
|      3 | 1266 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1267 | `		return PH7_OK;` |
|      - | 1268 | `	}` |
|     21 | 1269 | `	nLen = nSrcLen; /* cc warning */` |
|      - | 1270 | `	/* Extract the substring */` |
|     21 | 1271 | `	zSub = ph7_value_to_string(apArg[1],&nSublen);` |
|     21 | 1272 | `	if( nSublen < 1 \|\| nSublen > nSrcLen){` |
|      - | 1273 | `		/* Empty string,return FALSE */` |
|      3 | 1274 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1275 | `		return PH7_OK;` |
|      - | 1276 | `	}` |
|      - | 1277 | `	/* Extract the offset */` |
|     19 | 1278 | `	nOfft = ph7_value_to_int(apArg[2]);` |
|     19 | 1279 | `	if( nOfft < 0 ){` |
|      5 | 1280 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|      5 | 1281 | `		if( zOfft < zSource ){` |
|      - | 1282 | `			/* Invalid offset */` |
|      3 | 1283 | `			ph7_result_bool(pCtx,0);` |
|      3 | 1284 | `			return PH7_OK;` |
|      - | 1285 | `		}` |
|      3 | 1286 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|      3 | 1287 | `		nOfft = (int)(zOfft-zSource);` |
|     16 | 1288 | `	}else if( nOfft >= nSrcLen ){` |
|      - | 1289 | `		/* Invalid offset */` |
|      3 | 1290 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1291 | `		return PH7_OK;` |
|    ! 0 | 1292 | `	}else{` |
|     13 | 1293 | `		zOfft = &zSource[nOfft];` |
|     13 | 1294 | `		nLen = nSrcLen - nOfft;` |
|      - | 1295 | `	}` |
|     15 | 1296 | `	if( nArg > 3 ){` |
|      - | 1297 | `		/* Extract the length */` |
|     13 | 1298 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     13 | 1299 | `		if( nLen < 1 ){` |
|      - | 1300 | `			/* Invalid  length */` |
|      5 | 1301 | `			ph7_result_int(pCtx,1);` |
|      5 | 1302 | `			return PH7_OK;` |
|      9 | 1303 | `		}else if( nLen + nOfft > nSrcLen ){` |
|      - | 1304 | `			/* Invalid length */` |
|      3 | 1305 | `			nLen = nSrcLen - nOfft;` |
|      1 | 1306 | `		}` |
|      9 | 1307 | `		if( nArg > 4 ){` |
|      - | 1308 | `			/* Case-sensitive or not */` |
|      5 | 1309 | `			iCase = ph7_value_to_bool(apArg[4]);` |
|      2 | 1310 | `		}` |
|      4 | 1311 | `	}` |
|      - | 1312 | `	/* Perform the comparison */` |
|     11 | 1313 | `	if( iCase ){` |
|      3 | 1314 | `		rc = SyStrnicmp(zOfft,zSub,(sxu32)nLen);` |
|      2 | 1315 | `	}else{` |
|      9 | 1316 | `		rc = SyStrncmp(zOfft,zSub,(sxu32)nLen);` |
|      - | 1317 | `	}` |
|      - | 1318 | `	/* Comparison result */` |
|     11 | 1319 | `	ph7_result_int(pCtx,rc);` |
|     11 | 1320 | `	return PH7_OK;` |
|     14 | 1321 |  |
|      - | 1322 | `/*` |
|      - | 1323 | ` * int substr_count(string $haystack,string $needle[,int $offset = 0 [,int $length ]])` |
|      - | 1324 | ` *  Count the number of substring occurrences.` |
|      - | 1325 | ` * Parameters` |
|      - | 1326 | ` * $haystack` |
|      - | 1327 | ` *   The string to search in` |
|      - | 1328 | ` * $needle` |
|      - | 1329 | ` *   The substring to search for` |
|      - | 1330 | ` * $offset` |
|      - | 1331 | ` *  The offset where to start counting` |
|      - | 1332 | ` * $length (NOT USED)` |
|      - | 1333 | ` *  The maximum length after the specified offset to search for the substring.` |
|      - | 1334 | ` *  It outputs a warning if the offset plus the length is greater than the haystack length.` |
|      - | 1335 | ` * Return` |
|      - | 1336 | ` *  Toral number of substring occurrences.` |
|      - | 1337 | ` */` |
|     24 | 1338 | `static int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1339 |  |
|      - | 1340 | `	const char *zText,*zPattern,*zEnd;` |
|      - | 1341 | `	int nTextlen,nPatlen;` |
|     25 | 1342 | `	int iCount = 0;` |
|      - | 1343 | `	sxu32 nOfft;` |
|      - | 1344 | `	sxi32 rc;` |
|     25 | 1345 | `	if( nArg < 2 ){` |
|      - | 1346 | `		/* Missing arguments */` |
|      5 | 1347 | `		ph7_result_int(pCtx,0);` |
|      5 | 1348 | `		return PH7_OK;` |
|      - | 1349 | `	}` |
|      - | 1350 | `	/* Point to the haystack */` |
|     21 | 1351 | `	zText = ph7_value_to_string(apArg[0],&nTextlen);` |
|      - | 1352 | `	/* Point to the neddle */` |
|     21 | 1353 | `	zPattern = ph7_value_to_string(apArg[1],&nPatlen);` |
|     21 | 1354 | `	if( nTextlen < 1 \|\| nPatlen < 1 \|\| nPatlen > nTextlen ){` |
|      - | 1355 | `		/* NOOP,return zero */` |
|      3 | 1356 | `		ph7_result_int(pCtx,0);` |
|      3 | 1357 | `		return PH7_OK;` |
|      - | 1358 | `	}` |
|     19 | 1359 | `	if( nArg > 2 ){` |
|      - | 1360 | `		int iOfft;` |
|      - | 1361 | `		/* Extract the offset */` |
|     15 | 1362 | `		iOfft = ph7_value_to_int(apArg[2]);` |
|     15 | 1363 | `		if( iOfft < 0 \|\| iOfft > nTextlen ){` |
|      - | 1364 | `			/* Invalid offset,return zero */` |
|      3 | 1365 | `			ph7_result_int(pCtx,0);` |
|      3 | 1366 | `			return PH7_OK;` |
|      - | 1367 | `		}` |
|      - | 1368 | `		/* Point to the desired offset */` |
|     13 | 1369 | `		zText = &zText[iOfft];` |
|      - | 1370 | `		/* Adjust length */` |
|     13 | 1371 | `		nTextlen -= iOfft;` |
|      6 | 1372 | `	}` |
|      - | 1373 | `	/* Point to the end of the string */` |
|     17 | 1374 | `	zEnd = &zText[nTextlen];` |
|     17 | 1375 | `	if( nArg > 3 ){` |
|      - | 1376 | `		int nLen;` |
|      - | 1377 | `		/* Extract the length */` |
|     13 | 1378 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     13 | 1379 | `		if( nLen < 0 \|\| nLen > nTextlen ){` |
|      - | 1380 | `			/* Invalid length,return 0 */` |
|      7 | 1381 | `			ph7_result_int(pCtx,0);` |
|      7 | 1382 | `			return PH7_OK;` |
|      - | 1383 | `		}` |
|      - | 1384 | `		/* Adjust pointer */` |
|      7 | 1385 | `		nTextlen = nLen;` |
|      7 | 1386 | `		zEnd = &zText[nTextlen];` |
|      3 | 1387 | `	}` |
|      - | 1388 | `	/* Perform the search */` |
|     12 | 1389 | `	for(;;){` |
|     25 | 1390 | `		rc = SyBlobSearch((const void *)zText,(sxu32)(zEnd-zText),(const void *)zPattern,nPatlen,&nOfft);` |
|     25 | 1391 | `		if( rc != SXRET_OK ){` |
|      - | 1392 | `			/* Pattern not found,break immediately */` |
|      9 | 1393 | `			break;` |
|      - | 1394 | `		}` |
|      - | 1395 | `		/* Increment counter and update the offset */` |
|     17 | 1396 | `		iCount++;` |
|     17 | 1397 | `		zText += nOfft + nPatlen;` |
|     17 | 1398 | `		if( zText >= zEnd ){` |
|      3 | 1399 | `			break;` |
|      - | 1400 | `		}` |
|      1 | 1401 | `	}` |
|      - | 1402 | `	/* Pattern count */` |
|     11 | 1403 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 1404 | `	return PH7_OK;` |
|     13 | 1405 |  |
|      - | 1406 | `/*` |
|      - | 1407 | ` * string chunk_split(string $body[,int $chunklen = 76 [, string $end = "\r\n" ]])` |
|      - | 1408 | ` *   Split a string into smaller chunks.` |
|      - | 1409 | ` * Parameters` |
|      - | 1410 | ` *  $body` |
|      - | 1411 | ` *   The string to be chunked.` |
|      - | 1412 | ` * $chunklen` |
|      - | 1413 | ` *   The chunk length.` |
|      - | 1414 | ` * $end` |
|      - | 1415 | ` *   The line ending sequence.` |
|      - | 1416 | ` * Return` |
|      - | 1417 | ` *  The chunked string or NULL on failure.` |
|      - | 1418 | ` */` |
|     16 | 1419 | `static int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1420 |  |
|     17 | 1421 | `	const char *zIn,*zEnd,*zSep = "\r\n";` |
|      - | 1422 | `	int nSepLen,nChunkLen,nLen;` |
|     17 | 1423 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1424 | `		/* Nothing to split,return null */` |
|      5 | 1425 | `		ph7_result_null(pCtx);` |
|      5 | 1426 | `		return PH7_OK;` |
|      - | 1427 | `	}` |
|      - | 1428 | `	/* initialize/Extract arguments */` |
|     13 | 1429 | `	nSepLen = (int)sizeof("\r\n") - 1;` |
|     13 | 1430 | `	nChunkLen = 76;` |
|     13 | 1431 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 1432 | `	zEnd = &zIn[nLen];` |
|     13 | 1433 | `	if( nArg > 1 ){` |
|      - | 1434 | `		/* Chunk length */` |
|     13 | 1435 | `		nChunkLen = ph7_value_to_int(apArg[1]);` |
|     13 | 1436 | `		if( nChunkLen < 1 ){` |
|      - | 1437 | `			/* Switch back to the default length */` |
|      3 | 1438 | `			nChunkLen = 76;` |
|      1 | 1439 | `		}` |
|     13 | 1440 | `		if( nArg > 2 ){` |
|      - | 1441 | `			/* Separator */` |
|      9 | 1442 | `			zSep = ph7_value_to_string(apArg[2],&nSepLen);` |
|      9 | 1443 | `			if( nSepLen < 1 ){` |
|      - | 1444 | `				/* Switch back to the default separator */` |
|      3 | 1445 | `				zSep = "\r\n";` |
|      3 | 1446 | `				nSepLen = (int)sizeof("\r\n") - 1;` |
|      1 | 1447 | `			}` |
|      4 | 1448 | `		}` |
|      6 | 1449 | `	}` |
|      - | 1450 | `	/* Perform the requested operation */` |
|     13 | 1451 | `	if( nChunkLen > nLen ){` |
|      - | 1452 | `		/* Nothing to split,return the string and the separator */` |
|      9 | 1453 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);` |
|      9 | 1454 | `		return PH7_OK;` |
|      - | 1455 | `	}` |
|     17 | 1456 | `	while( zIn < zEnd ){` |
|     13 | 1457 | `		if( nChunkLen > (int)(zEnd-zIn) ){` |
|      3 | 1458 | `			nChunkLen = (int)(zEnd - zIn);` |
|      1 | 1459 | `		}` |
|      - | 1460 | `		/* Append the chunk and the separator */` |
|     13 | 1461 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);` |
|      - | 1462 | `		/* Point beyond the chunk */` |
|     13 | 1463 | `		zIn += nChunkLen;` |
|      1 | 1464 | `	}` |
|      5 | 1465 | `	return PH7_OK;` |
|      9 | 1466 |  |
|      - | 1467 | `/*` |
|      - | 1468 | ` * string addslashes(string $str)` |
|      - | 1469 | ` *  Quote string with slashes.` |
|      - | 1470 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1471 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1472 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1473 | ` * Parameter` |
|      - | 1474 | ` *  str: The string to be escaped.` |
|      - | 1475 | ` * Return` |
|      - | 1476 | ` *  Returns the escaped string` |
|      - | 1477 | ` */` |
|     10 | 1478 | `static int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1479 |  |
|      - | 1480 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1481 | `	int nLen;` |
|     11 | 1482 | `	if( nArg < 1 ){` |
|      - | 1483 | `		/* Nothing to process,retun NULL */` |
|      5 | 1484 | `		ph7_result_null(pCtx);` |
|      5 | 1485 | `		return PH7_OK;` |
|      - | 1486 | `	}` |
|      - | 1487 | `	/* Extract the string to process */` |
|      7 | 1488 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 1489 | `	if( nLen < 1 ){` |
|      - | 1490 | `		/* Return the empty string */` |
|      5 | 1491 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1492 | `		return PH7_OK;` |
|      - | 1493 | `	}` |
|      3 | 1494 | `	zEnd = &zIn[nLen];` |
|      3 | 1495 | `	zCur = 0; /* cc warning */` |
|      3 | 1496 | `	for(;;){` |
|      7 | 1497 | `		if( zIn >= zEnd ){` |
|      - | 1498 | `			/* No more input */` |
|      3 | 1499 | `			break;` |
|      - | 1500 | `		}` |
|      5 | 1501 | `		zCur = zIn;` |
|     15 | 1502 | `		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' ){` |
|     11 | 1503 | `			zIn++;` |
|      1 | 1504 | `		}` |
|      5 | 1505 | `		if( zIn > zCur ){` |
|      - | 1506 | `			/* Append raw contents */` |
|      5 | 1507 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 1508 | `		}` |
|      5 | 1509 | `		if( zIn < zEnd ){` |
|      3 | 1510 | `			int c = zIn[0];` |
|      3 | 1511 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|      1 | 1512 | `		}` |
|      5 | 1513 | `		zIn++;` |
|      1 | 1514 | `	}` |
|      3 | 1515 | `	return PH7_OK;` |
|      6 | 1516 |  |
|      - | 1517 | `/*` |
|      - | 1518 | ` * Check if the given character is present in the given mask.` |
|      - | 1519 | ` * Return TRUE if present. FALSE otherwise.` |
|      - | 1520 | ` */` |
|     76 | 1521 | `static int cSlashCheckMask(int c,const char *zMask,int nLen)` |
|      1 | 1522 |  |
|     77 | 1523 | `	const char *zEnd = &zMask[nLen];` |
|    495 | 1524 | `	while( zMask < zEnd ){` |
|    449 | 1525 | `		if( zMask[0] == c ){` |
|      - | 1526 | `			/* Character present,return TRUE */` |
|     31 | 1527 | `			return 1;` |
|      - | 1528 | `		}` |
|      - | 1529 | `		/* Advance the pointer */` |
|    419 | 1530 | `		zMask++;` |
|      1 | 1531 | `	}` |
|      - | 1532 | `	/* Not present */` |
|     47 | 1533 | `	return 0;` |
|     39 | 1534 |  |
|      - | 1535 | `/*` |
|      - | 1536 | ` * string addcslashes(string $str,string $charlist)` |
|      - | 1537 | ` *  Quote string with slashes in a C style.` |
|      - | 1538 | ` * Parameter` |
|      - | 1539 | ` *  $str:` |
|      - | 1540 | ` *    The string to be escaped.` |
|      - | 1541 | ` *  $charlist:` |
|      - | 1542 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|      - | 1543 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|      - | 1544 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|      - | 1545 | ` * Return` |
|      - | 1546 | ` *  Returns the escaped string.` |
|      - | 1547 | ` * Note:` |
|      - | 1548 | ` *  Range characters [i.e: 'A..Z'] is not implemented in the current release.` |
|      - | 1549 | ` */` |
|     12 | 1550 | `static int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1551 |  |
|      - | 1552 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - | 1553 | `	int nLen,nMask;` |
|     13 | 1554 | `	if( nArg < 1 ){` |
|      - | 1555 | `		/* Nothing to process,retun NULL */` |
|      3 | 1556 | `		ph7_result_null(pCtx);` |
|      3 | 1557 | `		return PH7_OK;` |
|      - | 1558 | `	}` |
|      - | 1559 | `	/* Extract the string to process */` |
|     11 | 1560 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 1561 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 1562 | `		/* Return the string untouched */` |
|      5 | 1563 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      5 | 1564 | `		return PH7_OK;` |
|      - | 1565 | `	}` |
|      - | 1566 | `	/* Extract the desired mask */` |
|      7 | 1567 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|      7 | 1568 | `	zEnd = &zIn[nLen];` |
|      7 | 1569 | `	zCur = 0; /* cc warning */` |
|      8 | 1570 | `	for(;;){` |
|     17 | 1571 | `		if( zIn >= zEnd ){` |
|      - | 1572 | `			/* No more input */` |
|      7 | 1573 | `			break;` |
|      - | 1574 | `		}` |
|     11 | 1575 | `		zCur = zIn;` |
|     31 | 1576 | `		while( zIn < zEnd && !cSlashCheckMask(zIn[0],zMask,nMask) ){` |
|     21 | 1577 | `			zIn++;` |
|      1 | 1578 | `		}` |
|     11 | 1579 | `		if( zIn > zCur ){` |
|      - | 1580 | `			/* Append raw contents */` |
|     11 | 1581 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      5 | 1582 | `		}` |
|     11 | 1583 | `		if( zIn < zEnd ){` |
|      5 | 1584 | `			int c = zIn[0];` |
|      5 | 1585 | `			if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|      - | 1586 | `				/* Convert to octal */` |
|      3 | 1587 | `				ph7_result_string_format(pCtx,"\\%o",c);` |
|      2 | 1588 | `			}else{` |
|      3 | 1589 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1590 | `			}` |
|      2 | 1591 | `		}` |
|     11 | 1592 | `		zIn++;` |
|      1 | 1593 | `	}` |
|      7 | 1594 | `	return PH7_OK;` |
|      7 | 1595 |  |
|      - | 1596 | `/*` |
|      - | 1597 | ` * string quotemeta(string $str)` |
|      - | 1598 | ` *  Quote meta characters.` |
|      - | 1599 | ` * Parameter` |
|      - | 1600 | ` *  $str:` |
|      - | 1601 | ` *    The string to be escaped.` |
|      - | 1602 | ` * Return` |
|      - | 1603 | ` *  Returns the escaped string.` |
|      - | 1604 | `*/` |
|     10 | 1605 | `static int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1606 |  |
|      - | 1607 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1608 | `	int nLen;` |
|     11 | 1609 | `	if( nArg < 1 ){` |
|      - | 1610 | `		/* Nothing to process,retun NULL */` |
|      3 | 1611 | `		ph7_result_null(pCtx);` |
|      3 | 1612 | `		return PH7_OK;` |
|      - | 1613 | `	}` |
|      - | 1614 | `	/* Extract the string to process */` |
|      9 | 1615 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 1616 | `	if( nLen < 1 ){` |
|      - | 1617 | `		/* Return the empty string */` |
|      3 | 1618 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1619 | `		return PH7_OK;` |
|      - | 1620 | `	}` |
|      7 | 1621 | `	zEnd = &zIn[nLen];` |
|      7 | 1622 | `	zCur = 0; /* cc warning */` |
|     17 | 1623 | `	for(;;){` |
|     35 | 1624 | `		if( zIn >= zEnd ){` |
|      - | 1625 | `			/* No more input */` |
|      7 | 1626 | `			break;` |
|      - | 1627 | `		}` |
|     29 | 1628 | `		zCur = zIn;` |
|     55 | 1629 | `		while( zIn < zEnd && !cSlashCheckMask(zIn[0],".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1) ){` |
|     27 | 1630 | `			zIn++;` |
|      1 | 1631 | `		}` |
|     29 | 1632 | `		if( zIn > zCur ){` |
|      - | 1633 | `			/* Append raw contents */` |
|     11 | 1634 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      5 | 1635 | `		}` |
|     29 | 1636 | `		if( zIn < zEnd ){` |
|     27 | 1637 | `			int c = zIn[0];` |
|     27 | 1638 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|     13 | 1639 | `		}` |
|     29 | 1640 | `		zIn++;` |
|      1 | 1641 | `	}` |
|      7 | 1642 | `	return PH7_OK;` |
|      6 | 1643 |  |
|      - | 1644 | `/*` |
|      - | 1645 | ` * string stripslashes(string $str)` |
|      - | 1646 | ` *  Un-quotes a quoted string.` |
|      - | 1647 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1648 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1649 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1650 | ` * Parameter` |
|      - | 1651 | ` *  $str` |
|      - | 1652 | ` *   The input string.` |
|      - | 1653 | ` * Return` |
|      - | 1654 | ` *  Returns a string with backslashes stripped off.` |
|      - | 1655 | ` */` |
|      8 | 1656 | `static int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1657 |  |
|      - | 1658 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1659 | `	int nLen;` |
|      9 | 1660 | `	if( nArg < 1 ){` |
|      - | 1661 | `		/* Nothing to process,retun NULL */` |
|      3 | 1662 | `		ph7_result_null(pCtx);` |
|      3 | 1663 | `		return PH7_OK;` |
|      - | 1664 | `	}` |
|      - | 1665 | `	/* Extract the string to process */` |
|      7 | 1666 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 1667 | `	if( zIn == 0 ){` |
|    ! 0 | 1668 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1669 | `		return PH7_OK;` |
|      - | 1670 | `	}` |
|      7 | 1671 | `	zEnd = &zIn[nLen];` |
|      7 | 1672 | `	zCur = 0; /* cc warning */` |
|      - | 1673 | `	/* Encode the string */` |
|      4 | 1674 | `	for(;;){` |
|      9 | 1675 | `		if( zIn >= zEnd ){` |
|      - | 1676 | `			/* No more input */` |
|      5 | 1677 | `			break;` |
|      - | 1678 | `		}` |
|      5 | 1679 | `		zCur = zIn;` |
|     17 | 1680 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 | 1681 | `			zIn++;` |
|      1 | 1682 | `		}` |
|      5 | 1683 | `		if( zIn > zCur ){` |
|      - | 1684 | `			/* Append raw contents */` |
|      5 | 1685 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 1686 | `		}` |
|      5 | 1687 | `		if( &zIn[1] < zEnd ){` |
|      3 | 1688 | `			int c = zIn[1];` |
|      3 | 1689 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - | 1690 | `				/* Ignore the backslash */` |
|      3 | 1691 | `				zIn++;` |
|      1 | 1692 | `			}` |
|      2 | 1693 | `		}else{` |
|      3 | 1694 | `			break;` |
|      - | 1695 | `		}` |
|      1 | 1696 | `	}` |
|      7 | 1697 | `	return PH7_OK;` |
|      5 | 1698 |  |
|      - | 1699 | `/*` |
|      - | 1700 | ` * string htmlspecialchars(string $string [, int $flags = ENT_COMPAT \| ENT_HTML401 [, string $charset]])` |
|      - | 1701 | ` *  HTML escaping of special characters.` |
|      - | 1702 | ` *  The translations performed are:` |
|      - | 1703 | ` *   '&' (ampersand) ==> '&amp;'` |
|      - | 1704 | ` *   '"' (double quote) ==> '&quot;' when ENT_NOQUOTES is not set.` |
|      - | 1705 | ` *   "'" (single quote) ==> '&#039;' only when ENT_QUOTES is set.` |
|      - | 1706 | ` *   '<' (less than) ==> '&lt;'` |
|      - | 1707 | ` *   '>' (greater than) ==> '&gt;'` |
|      - | 1708 | ` * Parameters` |
|      - | 1709 | ` *  $string` |
|      - | 1710 | ` *   The string being converted.` |
|      - | 1711 | ` * $flags` |
|      - | 1712 | ` *   A bitmask of one or more of the following flags, which specify how to handle quotes.` |
|      - | 1713 | ` *   The default is ENT_COMPAT \| ENT_HTML401.` |
|      - | 1714 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone.` |
|      - | 1715 | ` *   ENT_QUOTES 	Will convert both double and single quotes.` |
|      - | 1716 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted.` |
|      - | 1717 | ` *   ENT_IGNORE 	Silently discard invalid code unit sequences instead of returning an empty string.` |
|      - | 1718 | ` * $charset` |
|      - | 1719 | ` *  Defines character set used in conversion. The default character set is ISO-8859-1. (Not used)` |
|      - | 1720 | ` * Return` |
|      - | 1721 | ` *  The escaped string or NULL on failure.` |
|      - | 1722 | ` */` |
|     20 | 1723 | `static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1724 |  |
|      - | 1725 | `	const char *zCur,*zIn,*zEnd;` |
|     21 | 1726 | `	int iFlags = 0x01\|0x40; /* ENT_COMPAT \| ENT_HTML401 */` |
|      - | 1727 | `	int nLen,c;` |
|     21 | 1728 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1729 | `		/* Missing/Invalid arguments,return NULL */` |
|     11 | 1730 | `		ph7_result_null(pCtx);` |
|     11 | 1731 | `		return PH7_OK;` |
|      - | 1732 | `	}` |
|      - | 1733 | `	/* Extract the target string */` |
|     11 | 1734 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 1735 | `	zEnd = &zIn[nLen];` |
|      - | 1736 | `	/* Extract the flags if available */` |
|     11 | 1737 | `	if( nArg > 1 ){` |
|      9 | 1738 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 | 1739 | `		if( iFlags < 0 ){` |
|      3 | 1740 | `			iFlags = 0x01\|0x40;` |
|      1 | 1741 | `		}` |
|      4 | 1742 | `	}` |
|      - | 1743 | `	/* Perform the requested operation */` |
|     23 | 1744 | `	for(;;){` |
|     47 | 1745 | `		if( zIn >= zEnd ){` |
|      9 | 1746 | `			break;` |
|      - | 1747 | `		}` |
|     39 | 1748 | `		zCur = zIn;` |
|     83 | 1749 | `		while( zIn < zEnd && zIn[0] != '&' && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '<' && zIn[0] != '>' ){` |
|     45 | 1750 | `			zIn++;` |
|      1 | 1751 | `		}` |
|     39 | 1752 | `		if( zCur < zIn ){` |
|      - | 1753 | `			/* Append the raw string verbatim */` |
|     17 | 1754 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      8 | 1755 | `		}` |
|     39 | 1756 | `		if( zIn >= zEnd ){` |
|      3 | 1757 | `			break;` |
|      - | 1758 | `		}` |
|     37 | 1759 | `		c = zIn[0];` |
|     37 | 1760 | `		if( c == '&' ){` |
|      - | 1761 | `			/* Expand '&amp;' */` |
|      9 | 1762 | `			ph7_result_string(pCtx,"&amp;",(int)sizeof("&amp;")-1);` |
|     33 | 1763 | `		}else if( c == '<' ){` |
|      - | 1764 | `			/* Expand '&lt;' */` |
|      7 | 1765 | `			ph7_result_string(pCtx,"&lt;",(int)sizeof("&lt;")-1);` |
|     26 | 1766 | `		}else if( c == '>' ){` |
|      - | 1767 | `			/* Expand '&gt;' */` |
|      9 | 1768 | `			ph7_result_string(pCtx,"&gt;",(int)sizeof("&gt;")-1);` |
|     19 | 1769 | `		}else if( c == '\'' ){` |
|      5 | 1770 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 1771 | `				/* Expand '&#039;' */` |
|      5 | 1772 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      3 | 1773 | `			}else{` |
|      - | 1774 | `				/* Leave the single quote untouched */` |
|    ! 0 | 1775 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      1 | 1776 | `			}` |
|     13 | 1777 | `		}else if( c == '"' ){` |
|     11 | 1778 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      - | 1779 | `				/* Expand '&quot;' */` |
|      7 | 1780 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      4 | 1781 | `			}else{` |
|      - | 1782 | `				/* Leave the double quote untouched */` |
|      5 | 1783 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      - | 1784 | `			}` |
|      5 | 1785 | `		}` |
|      - | 1786 | `		/* Ignore the unsafe HTML character */` |
|     37 | 1787 | `		zIn++;` |
|      1 | 1788 | `	}` |
|     11 | 1789 | `	return PH7_OK;` |
|     11 | 1790 |  |
|      - | 1791 | `/*` |
|      - | 1792 | ` * string htmlspecialchars_decode(string $string[,int $quote_style = ENT_COMPAT ])` |
|      - | 1793 | ` *  Unescape HTML entities.` |
|      - | 1794 | ` * Parameters` |
|      - | 1795 | ` *  $string` |
|      - | 1796 | ` *   The string to decode` |
|      - | 1797 | ` *  $quote_style` |
|      - | 1798 | ` *    The quote style. One of the following constants:` |
|      - | 1799 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone (default)` |
|      - | 1800 | ` *   ENT_QUOTES 	Will convert both double and single quotes` |
|      - | 1801 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted` |
|      - | 1802 | ` * Return` |
|      - | 1803 | ` *  The unescaped string or NULL on failure.` |
|      - | 1804 | ` */` |
|     16 | 1805 | `static int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1806 |  |
|      - | 1807 | `	const char *zCur,*zIn,*zEnd;` |
|     17 | 1808 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 1809 | `	int nLen,nJump;` |
|     17 | 1810 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1811 | `		/* Missing/Invalid arguments,return NULL */` |
|      7 | 1812 | `		ph7_result_null(pCtx);` |
|      7 | 1813 | `		return PH7_OK;` |
|      - | 1814 | `	}` |
|      - | 1815 | `	/* Extract the target string */` |
|     11 | 1816 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 1817 | `	zEnd = &zIn[nLen];` |
|      - | 1818 | `	/* Extract the flags if available */` |
|     11 | 1819 | `	if( nArg > 1 ){` |
|      7 | 1820 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      7 | 1821 | `		if( iFlags < 0 ){` |
|      3 | 1822 | `			iFlags = 0x01;` |
|      1 | 1823 | `		}` |
|      3 | 1824 | `	}` |
|      - | 1825 | `	/* Perform the requested operation */` |
|     15 | 1826 | `	for(;;){` |
|     31 | 1827 | `		if( zIn >= zEnd ){` |
|     11 | 1828 | `			break;` |
|      - | 1829 | `		}` |
|     21 | 1830 | `		zCur = zIn;` |
|     51 | 1831 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|     31 | 1832 | `			zIn++;` |
|      1 | 1833 | `		}` |
|     21 | 1834 | `		if( zCur < zIn ){` |
|      - | 1835 | `			/* Append the raw string verbatim */` |
|      9 | 1836 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 1837 | `		}` |
|     21 | 1838 | `		nLen = (int)(zEnd-zIn);` |
|     21 | 1839 | `		nJump = (int)sizeof(char);` |
|     21 | 1840 | `		if( nLen >= (int)sizeof("&amp;")-1 && SyStrnicmp(zIn,"&amp;",sizeof("&amp;")-1) == 0 ){` |
|      - | 1841 | `			/* &amp; ==> '&' */` |
|      3 | 1842 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|      3 | 1843 | `			nJump = (int)sizeof("&amp;")-1;` |
|     20 | 1844 | `		}else if( nLen >= (int)sizeof("&lt;")-1 && SyStrnicmp(zIn,"&lt;",sizeof("&lt;")-1) == 0 ){` |
|      - | 1845 | `			/* &lt; ==> < */` |
|      3 | 1846 | `			ph7_result_string(pCtx,"<",(int)sizeof(char));` |
|      3 | 1847 | `			nJump = (int)sizeof("&lt;")-1;` |
|     18 | 1848 | `		}else if( nLen >= (int)sizeof("&gt;")-1 && SyStrnicmp(zIn,"&gt;",sizeof("&gt;")-1) == 0 ){` |
|      - | 1849 | `			/* &gt; ==> '>' */` |
|      3 | 1850 | `			ph7_result_string(pCtx,">",(int)sizeof(char));` |
|      3 | 1851 | `			nJump = (int)sizeof("&gt;")-1;` |
|     16 | 1852 | `		}else if( nLen >= (int)sizeof("&quot;")-1 && SyStrnicmp(zIn,"&quot;",sizeof("&quot;")-1) == 0 ){` |
|      - | 1853 | `			/* &quot; ==> '"' */` |
|     13 | 1854 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      9 | 1855 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      5 | 1856 | `			}else{` |
|      - | 1857 | `				/* Leave untouched */` |
|      5 | 1858 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      - | 1859 | `			}` |
|     13 | 1860 | `			nJump = (int)sizeof("&quot;")-1;` |
|      9 | 1861 | `		}else if( nLen >= (int)sizeof("&#039;")-1 && SyStrnicmp(zIn,"&#039;",sizeof("&#039;")-1) == 0 ){` |
|      - | 1862 | `			/* &#039; ==> ''' */` |
|      3 | 1863 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 1864 | `				/* Expand ''' */` |
|      3 | 1865 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      2 | 1866 | `			}else{` |
|      - | 1867 | `				/* Leave untouched */` |
|    ! 0 | 1868 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      - | 1869 | `			}` |
|      3 | 1870 | `			nJump = (int)sizeof("&#039;")-1;` |
|      1 | 1871 | `		}else if( nLen >= (int)sizeof(char) ){` |
|      - | 1872 | `			/* expand '&' */` |
|    ! 0 | 1873 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 1874 | `		}else{` |
|      - | 1875 | `			/* No more input to process */` |
|    ! 0 | 1876 | `			break;` |
|      - | 1877 | `		}` |
|     21 | 1878 | `		zIn += nJump;` |
|      1 | 1879 | `	}` |
|     11 | 1880 | `	return PH7_OK;` |
|      9 | 1881 |  |
|      - | 1882 | `/* HTML encoding/Decoding table` |
|      - | 1883 | ` * Source: Symisc RunTime API.[chm@symisc.net]` |
|      - | 1884 | ` */` |
|      - | 1885 | `static const char *azHtmlEscape[] = {` |
|      - | 1886 | ` 	"&lt;","<","&gt;",">","&amp;","&","&quot;","\"","&#39;","'",` |
|      - | 1887 | `	"&#33;","!","&#36;","$","&#35;","#","&#37;","%","&#40;","(",` |
|      - | 1888 | `	"&#41;",")","&#123;","{","&#125;","}","&#61;","=","&#43;","+",` |
|      - | 1889 | `	"&#63;","?","&#91;","[","&#93;","]","&#64;","@","&#44;",","` |
|      - | 1890 | ` };` |
|      - | 1891 | `/*` |
|      - | 1892 | ` * array get_html_translation_table(void)` |
|      - | 1893 | ` *  Returns the translation table used by htmlspecialchars() and htmlentities().` |
|      - | 1894 | ` * Parameters` |
|      - | 1895 | ` *  None` |
|      - | 1896 | ` * Return` |
|      - | 1897 | ` *  The translation table as an array or NULL on failure.` |
|      - | 1898 | ` */` |
|      4 | 1899 | `static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1900 |  |
|      - | 1901 | `	ph7_value *pArray,*pValue;` |
|      - | 1902 | `	sxu32 n;` |
|      - | 1903 | `	/* Element value */` |
|      5 | 1904 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      5 | 1905 | `	if( pValue == 0 ){` |
|    ! 0 | 1906 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 1907 | `		SXUNUSED(apArg);` |
|      - | 1908 | `		/* Return NULL */` |
|    ! 0 | 1909 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1910 | `		return PH7_OK;` |
|      - | 1911 | `	}` |
|      - | 1912 | `	/* Create a new array */` |
|      5 | 1913 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 | 1914 | `	if( pArray == 0 ){` |
|      - | 1915 | `		/* Return NULL */` |
|    ! 0 | 1916 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1917 | `		return PH7_OK;` |
|      - | 1918 | `	}` |
|      - | 1919 | `	/* Make the table */` |
|     85 | 1920 | `	for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|      - | 1921 | `		/* Prepare the value */` |
|     81 | 1922 | `		ph7_value_string(pValue,azHtmlEscape[n],-1 /* Compute length automatically */);` |
|      - | 1923 | `		/* Insert the value */` |
|     81 | 1924 | `		ph7_array_add_strkey_elem(pArray,azHtmlEscape[n+1],pValue);` |
|      - | 1925 | `		/* Reset the string cursor */` |
|     81 | 1926 | `		ph7_value_reset_string_cursor(pValue);` |
|     41 | 1927 | `	}` |
|      - | 1928 | `	/*` |
|      - | 1929 | `	 * Return the array.` |
|      - | 1930 | `	 * Don't worry about freeing memory, everything will be automatically` |
|      - | 1931 | `	 * released upon we return from this function.` |
|      - | 1932 | `	 */` |
|      5 | 1933 | `	ph7_result_value(pCtx,pArray);` |
|      5 | 1934 | `	return PH7_OK;` |
|      3 | 1935 |  |
|      - | 1936 | `/*` |
|      - | 1937 | ` * string htmlentities( string $string [, int $flags = ENT_COMPAT \| ENT_HTML401]);` |
|      - | 1938 | ` *   Convert all applicable characters to HTML entities` |
|      - | 1939 | ` * Parameters` |
|      - | 1940 | ` * $string` |
|      - | 1941 | ` *   The input string.` |
|      - | 1942 | ` * $flags` |
|      - | 1943 | ` *  A bitmask of one or more of the flags (see block-comment on PH7_builtin_htmlspecialchars())` |
|      - | 1944 | ` * Return` |
|      - | 1945 | ` * The encoded string.` |
|      - | 1946 | ` */` |
|     10 | 1947 | `static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1948 |  |
|     11 | 1949 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 1950 | `	const char *zIn,*zEnd;` |
|      - | 1951 | `	int nLen,c;` |
|      - | 1952 | `	sxu32 n;` |
|     11 | 1953 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1954 | `		/* Missing/Invalid arguments,return NULL */` |
|      7 | 1955 | `		ph7_result_null(pCtx);` |
|      7 | 1956 | `		return PH7_OK;` |
|      - | 1957 | `	}` |
|      - | 1958 | `	/* Extract the target string */` |
|      5 | 1959 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 1960 | `	zEnd = &zIn[nLen];` |
|      - | 1961 | `	/* Extract the flags if available */` |
|      5 | 1962 | `	if( nArg > 1 ){` |
|      3 | 1963 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      3 | 1964 | `		if( iFlags < 0 ){` |
|      3 | 1965 | `			iFlags = 0x01;` |
|      1 | 1966 | `		}` |
|      1 | 1967 | `	}` |
|      - | 1968 | `	/* Perform the requested operation */` |
|     11 | 1969 | `	for(;;){` |
|     23 | 1970 | `		if( zIn >= zEnd ){` |
|      - | 1971 | `			/* No more input to process */` |
|      5 | 1972 | `			break;` |
|      - | 1973 | `		}` |
|     19 | 1974 | `		c = zIn[0];` |
|      - | 1975 | `		/* Perform a linear lookup on the decoding table */` |
|    233 | 1976 | `		for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    223 | 1977 | `			if( azHtmlEscape[n+1][0] == c ){` |
|      - | 1978 | `				/* Got one */` |
|      9 | 1979 | `				break;` |
|      - | 1980 | `			}` |
|    108 | 1981 | `		}` |
|     19 | 1982 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|      - | 1983 | `			/* Output the safe sequence [i.e: '<' ==> '&lt;"] */` |
|      9 | 1984 | `			if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 1985 | `				/* Expand the double quote verbatim */` |
|    ! 0 | 1986 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      9 | 1987 | `			}else if(c == '\'' && ((iFlags & 0x02 /*ENT_QUOTES*/) == 0 \|\| (iFlags & 0x04) /*ENT_NOQUOTES*/) ){` |
|      - | 1988 | `				/* expand single quote verbatim */` |
|    ! 0 | 1989 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|    ! 0 | 1990 | `			}else{` |
|      9 | 1991 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1/*Compute length automatically */);` |
|      - | 1992 | `			}` |
|      5 | 1993 | `		}else{` |
|      - | 1994 | `			/* Output character verbatim */` |
|     11 | 1995 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 1996 | `		}` |
|     19 | 1997 | `		zIn++;` |
|      1 | 1998 | `	}` |
|      5 | 1999 | `	return PH7_OK;` |
|      6 | 2000 |  |
|      - | 2001 | `/*` |
|      - | 2002 | ` * string html_entity_decode(string $string [, int $quote_style = ENT_COMPAT [, string $charset = 'UTF-8' ]])` |
|      - | 2003 | ` *   Perform the reverse operation of html_entity_decode().` |
|      - | 2004 | ` * Parameters` |
|      - | 2005 | ` * $string` |
|      - | 2006 | ` *   The input string.` |
|      - | 2007 | ` * $flags` |
|      - | 2008 | ` *  A bitmask of one or more of the flags (see comment on PH7_builtin_htmlspecialchars())` |
|      - | 2009 | ` * Return` |
|      - | 2010 | ` * The decoded string.` |
|      - | 2011 | ` */` |
|     28 | 2012 | `static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2013 |  |
|      - | 2014 | `	const char *zCur,*zIn,*zEnd;` |
|     29 | 2015 | `	int iFlags = 0x01; /* ENT_COMPAT  */` |
|      - | 2016 | `	int nLen;` |
|      - | 2017 | `	sxu32 n;` |
|     29 | 2018 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2019 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 2020 | `		ph7_result_null(pCtx);` |
|      5 | 2021 | `		return PH7_OK;` |
|      - | 2022 | `	}` |
|      - | 2023 | `	/* Extract the target string */` |
|     25 | 2024 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 2025 | `	zEnd = &zIn[nLen];` |
|      - | 2026 | `	/* Extract the flags if available */` |
|     25 | 2027 | `	if( nArg > 1 ){` |
|     15 | 2028 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     15 | 2029 | `		if( iFlags < 0 ){` |
|      3 | 2030 | `			iFlags = 0x01;` |
|      1 | 2031 | `		}` |
|      7 | 2032 | `	}` |
|      - | 2033 | `	/* Perform the requested operation */` |
|     27 | 2034 | `	for(;;){` |
|     55 | 2035 | `		if( zIn >= zEnd ){` |
|      - | 2036 | `			/* No more input to process */` |
|     13 | 2037 | `			break;` |
|      - | 2038 | `		}` |
|     43 | 2039 | `		zCur = zIn;` |
|    173 | 2040 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|    131 | 2041 | `			zIn++;` |
|      1 | 2042 | `		}` |
|     43 | 2043 | `		if( zCur < zIn ){` |
|      - | 2044 | `			/* Append raw string verbatim */` |
|     27 | 2045 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     13 | 2046 | `		}` |
|     43 | 2047 | `		if( zIn >= zEnd ){` |
|     13 | 2048 | `			break;` |
|      - | 2049 | `		}` |
|     31 | 2050 | `		nLen = (int)(zEnd-zIn);` |
|      - | 2051 | `		/* Find an encoded sequence */` |
|    113 | 2052 | `		for(n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    113 | 2053 | `			int iLen = (int)SyStrlen(azHtmlEscape[n]);` |
|    113 | 2054 | `			if( nLen >= iLen && SyStrnicmp(zIn,azHtmlEscape[n],(sxu32)iLen) == 0 ){` |
|      - | 2055 | `				/* Got one */` |
|     31 | 2056 | `				zIn += iLen;` |
|     31 | 2057 | `				break;` |
|      - | 2058 | `			}` |
|     42 | 2059 | `		}` |
|     31 | 2060 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|     31 | 2061 | `			int c = azHtmlEscape[n+1][0];` |
|      - | 2062 | `			/* Output the decoded character */` |
|     31 | 2063 | `			if( c == '\'' && ((iFlags & 0x02) == 0 /*ENT_QUOTES*/\|\| (iFlags & 0x04) /*ENT_NOQUOTES*/)  ){` |
|      - | 2064 | `				/* Do not process single quotes */` |
|      9 | 2065 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|     27 | 2066 | `			}else if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 2067 | `				/* Do not process double quotes */` |
|      5 | 2068 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|      3 | 2069 | `			}else{` |
|     19 | 2070 | `				ph7_result_string(pCtx,azHtmlEscape[n+1],-1); /* Compute length automatically */` |
|      - | 2071 | `			}` |
|     16 | 2072 | `		}else{` |
|      - | 2073 | `			/* Append '&' */` |
|    ! 0 | 2074 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 2075 | `			zIn++;` |
|      - | 2076 | `		}` |
|      1 | 2077 | `	}` |
|     25 | 2078 | `	return PH7_OK;` |
|     15 | 2079 |  |
|      - | 2080 | `/*` |
|      - | 2081 | ` * int strlen($string)` |
|      - | 2082 | ` *  return the length of the given string.` |
|      - | 2083 | ` * Parameter` |
|      - | 2084 | ` *  string: The string being measured for length.` |
|      - | 2085 | ` * Return` |
|      - | 2086 | ` *  length of the given string.` |
|      - | 2087 | ` */` |
|    840 | 2088 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2089 |  |
|    841 | 2090 | `	int iLen = 0;` |
|    841 | 2091 | `	if( nArg > 0 ){` |
|    839 | 2092 | `		ph7_value_to_string(apArg[0],&iLen);` |
|    419 | 2093 | `	}` |
|      - | 2094 | `	/* String length */` |
|    841 | 2095 | `	ph7_result_int(pCtx,iLen);` |
|    841 | 2096 | `	return PH7_OK;` |
|      1 | 2097 |  |
|      - | 2098 | `/*` |
|      - | 2099 | ` * int strcmp(string $str1,string $str2)` |
|      - | 2100 | ` *  Perform a binary safe string comparison.` |
|      - | 2101 | ` * Parameter` |
|      - | 2102 | ` *  str1: The first string` |
|      - | 2103 | ` *  str2: The second string` |
|      - | 2104 | ` * Return` |
|      - | 2105 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2106 | ` *  than str2, and 0 if they are equal.` |
|      - | 2107 | ` */` |
|     50 | 2108 | `static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2109 |  |
|      - | 2110 | `	const char *z1,*z2;` |
|      - | 2111 | `	int n1,n2;` |
|      - | 2112 | `	int res;` |
|     51 | 2113 | `	if( nArg < 2 ){` |
|      5 | 2114 | `		res = nArg == 0 ? 0 : 1;` |
|      5 | 2115 | `		ph7_result_int(pCtx,res);` |
|      5 | 2116 | `		return PH7_OK;` |
|      - | 2117 | `	}` |
|      - | 2118 | `	/* Perform the comparison */` |
|     47 | 2119 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     47 | 2120 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     47 | 2121 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2122 | `	/* Comparison result */` |
|     47 | 2123 | `	ph7_result_int(pCtx,res);` |
|     47 | 2124 | `	return PH7_OK;` |
|     26 | 2125 |  |
|      - | 2126 | `/*` |
|      - | 2127 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 2128 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 2129 | ` * Parameter` |
|      - | 2130 | ` *  str1: The first string` |
|      - | 2131 | ` *  str2: The second string` |
|      - | 2132 | ` * Return` |
|      - | 2133 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2134 | ` *  than str2, and 0 if they are equal.` |
|      - | 2135 | ` */` |
|     20 | 2136 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2137 |  |
|      - | 2138 | `	const char *z1,*z2;` |
|      - | 2139 | `	int res;` |
|      - | 2140 | `	int n;` |
|     21 | 2141 | `	if( nArg < 3 ){` |
|      - | 2142 | `		/* Perform a standard comparison */` |
|      5 | 2143 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 2144 | `	}` |
|      - | 2145 | `	/* Desired comparison length */` |
|     17 | 2146 | `	n  = ph7_value_to_int(apArg[2]);` |
|     17 | 2147 | `	if( n < 0 ){` |
|      - | 2148 | `		/* Invalid length */` |
|      3 | 2149 | `		ph7_result_int(pCtx,-1);` |
|      3 | 2150 | `		return PH7_OK;` |
|      - | 2151 | `	}` |
|      - | 2152 | `	/* Perform the comparison */` |
|     15 | 2153 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     15 | 2154 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     15 | 2155 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 2156 | `	/* Comparison result */` |
|     15 | 2157 | `	ph7_result_int(pCtx,res);` |
|     15 | 2158 | `	return PH7_OK;` |
|     11 | 2159 |  |
|      - | 2160 | `/*` |
|      - | 2161 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 2162 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 2163 | ` * Parameter` |
|      - | 2164 | ` *  str1: The first string` |
|      - | 2165 | ` *  str2: The second string` |
|      - | 2166 | ` * Return` |
|      - | 2167 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2168 | ` *  than str2, and 0 if they are equal.` |
|      - | 2169 | ` */` |
|     18 | 2170 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2171 |  |
|      - | 2172 | `	const char *z1,*z2;` |
|      - | 2173 | `	int n1,n2;` |
|      - | 2174 | `	int res;` |
|     19 | 2175 | `	if( nArg < 2 ){` |
|      9 | 2176 | `		res = nArg == 0 ? 0 : 1;` |
|      9 | 2177 | `		ph7_result_int(pCtx,res);` |
|      9 | 2178 | `		return PH7_OK;` |
|      - | 2179 | `	}` |
|      - | 2180 | `	/* Perform the comparison */` |
|     11 | 2181 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     11 | 2182 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     11 | 2183 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2184 | `	/* Comparison result */` |
|     11 | 2185 | `	ph7_result_int(pCtx,res);` |
|     11 | 2186 | `	return PH7_OK;` |
|     10 | 2187 |  |
|      - | 2188 | `/*` |
|      - | 2189 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 2190 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 2191 | ` * Parameter` |
|      - | 2192 | ` *  $str1: The first string` |
|      - | 2193 | ` *  $str2: The second string` |
|      - | 2194 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 2195 | ` * Return` |
|      - | 2196 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2197 | ` *  than str2, and 0 if they are equal.` |
|      - | 2198 | ` */` |
|      8 | 2199 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2200 |  |
|      - | 2201 | `	const char *z1,*z2;` |
|      - | 2202 | `	int res;` |
|      - | 2203 | `	int n;` |
|      9 | 2204 | `	if( nArg < 3 ){` |
|      - | 2205 | `		/* Perform a standard comparison */` |
|      5 | 2206 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 2207 | `	}` |
|      - | 2208 | `	/* Desired comparison length */` |
|      5 | 2209 | `	n  = ph7_value_to_int(apArg[2]);` |
|      5 | 2210 | `	if( n < 0 ){` |
|      - | 2211 | `		/* Invalid length */` |
|    ! 0 | 2212 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2213 | `		return PH7_OK;` |
|      - | 2214 | `	}` |
|      - | 2215 | `	/* Perform the comparison */` |
|      5 | 2216 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|      5 | 2217 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|      5 | 2218 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 2219 | `	/* Comparison result */` |
|      5 | 2220 | `	ph7_result_int(pCtx,res);` |
|      5 | 2221 | `	return PH7_OK;` |
|      5 | 2222 |  |
|      - | 2223 | `/*` |
|      - | 2224 | ` * Implode context [i.e: it's private data].` |
|      - | 2225 | ` * A pointer to the following structure is forwarded` |
|      - | 2226 | ` * verbatim to the array walker callback defined below.` |
|      - | 2227 | ` */` |
|      - | 2228 | `struct implode_data {` |
|      - | 2229 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 2230 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 2231 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 2232 | `	int nSeplen;          /* Separator length */` |
|      - | 2233 | `	int bFirst;           /* TRUE if first call */` |
|      - | 2234 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 2235 | `};` |
|      - | 2236 | `/*` |
|      - | 2237 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 2238 | ` * The following routine is invoked for each array entry passed` |
|      - | 2239 | ` * to the implode() function.` |
|      - | 2240 | ` */` |
|  71044 | 2241 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      1 | 2242 |  |
|  35522 | 2243 | `	SXUNUSED(pKey);` |
|  71045 | 2244 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 2245 | `	const char *zData;` |
|      - | 2246 | `	int nLen;` |
|  71045 | 2247 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 2248 | `		if( pData->nSeplen > 0 ){` |
|      3 | 2249 | `			if( !pData->bFirst ){` |
|      - | 2250 | `				/* append the separator first */` |
|      3 | 2251 | `				ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|      2 | 2252 | `			}else{` |
|    ! 0 | 2253 | `				pData->bFirst = 0;` |
|      - | 2254 | `			}` |
|      1 | 2255 | `		}` |
|      - | 2256 | `		/* Recurse */` |
|      3 | 2257 | `		pData->bFirst = 1;` |
|      3 | 2258 | `		pData->nRecCount++;` |
|      3 | 2259 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 2260 | `		pData->nRecCount--;` |
|      3 | 2261 | `		return PH7_OK;` |
|      - | 2262 | `	}` |
|      - | 2263 | `	/* Extract the string representation of the entry value */` |
|  71043 | 2264 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 2265 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
|  71043 | 2266 | `	if( pData->bFirst ){` |
|  14485 | 2267 | `		pData->bFirst = 0;` |
|  63801 | 2268 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 2269 | `		/* append the separator first */` |
|  56547 | 2270 | `		ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|  28273 | 2271 | `	}` |
|      - | 2272 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
|  71043 | 2273 | `	if( nLen > 0 ){` |
|  65051 | 2274 | `		ph7_result_string(pData->pCtx,zData,nLen);` |
|  32525 | 2275 | `	}` |
|  71043 | 2276 | `	return PH7_OK;` |
|  35523 | 2277 |  |
|      - | 2278 | `/*` |
|      - | 2279 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 2280 | ` * string implode(array $pieces,...)` |
|      - | 2281 | ` *  Join array elements with a string.` |
|      - | 2282 | ` * $glue` |
|      - | 2283 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 2284 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 2285 | ` * $pieces` |
|      - | 2286 | ` *   The array of strings to implode.` |
|      - | 2287 | ` * Return` |
|      - | 2288 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 2289 | ` *  order, with the glue string between each element.` |
|      - | 2290 | ` */` |
|  14510 | 2291 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2292 |  |
|      - | 2293 | `	struct implode_data imp_data;` |
|  14511 | 2294 | `	int i = 1;` |
|  14511 | 2295 | `	if( nArg < 1 ){` |
|      - | 2296 | `		/* Missing argument,return NULL */` |
|    ! 0 | 2297 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2298 | `		return PH7_OK;` |
|      - | 2299 | `	}` |
|      - | 2300 | `	/* Prepare the implode context */` |
|  14511 | 2301 | `	imp_data.pCtx = pCtx;` |
|  14511 | 2302 | `	imp_data.bRecursive = 0;` |
|  14511 | 2303 | `	imp_data.bFirst = 1;` |
|  14511 | 2304 | `	imp_data.nRecCount = 0;` |
|  14511 | 2305 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  14509 | 2306 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|   7255 | 2307 | `	}else{` |
|      3 | 2308 | `		imp_data.zSep = 0;` |
|      3 | 2309 | `		imp_data.nSeplen = 0;` |
|      3 | 2310 | `		i = 0;` |
|      - | 2311 | `	}` |
|  14511 | 2312 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 2313 | `	/* Start the 'join' process */` |
|  29021 | 2314 | `	while( i < nArg ){` |
|  14511 | 2315 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2316 | `			/* Iterate throw array entries */` |
|  14511 | 2317 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|   7256 | 2318 | `		}else{` |
|      - | 2319 | `			const char *zData;` |
|      - | 2320 | `			int nLen;` |
|      - | 2321 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 2322 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2323 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 2324 | `			if( imp_data.bFirst ){` |
|    ! 0 | 2325 | `				imp_data.bFirst = 0;` |
|    ! 0 | 2326 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2327 | `				ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen);` |
|    ! 0 | 2328 | `			}` |
|      - | 2329 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 2330 | `			if( nLen > 0 ){` |
|    ! 0 | 2331 | `				ph7_result_string(pCtx,zData,nLen);` |
|    ! 0 | 2332 | `			}` |
|      - | 2333 | `		}` |
|  14511 | 2334 | `		i++;` |
|      1 | 2335 | `	}` |
|  14511 | 2336 | `	return PH7_OK;` |
|   7256 | 2337 |  |
|      - | 2338 | `/*` |
|      - | 2339 | ` * Symisc eXtension:` |
|      - | 2340 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 2341 | ` * Purpose` |
|      - | 2342 | ` *  Same as implode() but recurse on arrays.` |
|      - | 2343 | ` * Example:` |
|      - | 2344 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 2345 | ` *   echo implode_recursive("/",$a);` |
|      - | 2346 | ` *   Will output` |
|      - | 2347 | ` *     usr/home/dean.` |
|      - | 2348 | ` *   While the standard implode would produce.` |
|      - | 2349 | ` *    usr/Array.` |
|      - | 2350 | ` * Parameter` |
|      - | 2351 | ` *  Refer to implode().` |
|      - | 2352 | ` * Return` |
|      - | 2353 | ` *  Refer to implode().` |
|      - | 2354 | ` */` |
|     12 | 2355 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2356 |  |
|      - | 2357 | `	struct implode_data imp_data;` |
|     13 | 2358 | `	int i = 1;` |
|     13 | 2359 | `	if( nArg < 1 ){` |
|      - | 2360 | `		/* Missing argument,return NULL */` |
|      3 | 2361 | `		ph7_result_null(pCtx);` |
|      3 | 2362 | `		return PH7_OK;` |
|      - | 2363 | `	}` |
|      - | 2364 | `	/* Prepare the implode context */` |
|     11 | 2365 | `	imp_data.pCtx = pCtx;` |
|     11 | 2366 | `	imp_data.bRecursive = 1;` |
|     11 | 2367 | `	imp_data.bFirst = 1;` |
|     11 | 2368 | `	imp_data.nRecCount = 0;` |
|     11 | 2369 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 2370 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 2371 | `	}else{` |
|    ! 0 | 2372 | `		imp_data.zSep = 0;` |
|    ! 0 | 2373 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 2374 | `		i = 0;` |
|      - | 2375 | `	}` |
|     11 | 2376 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 2377 | `	/* Start the 'join' process */` |
|     21 | 2378 | `	while( i < nArg ){` |
|     11 | 2379 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2380 | `			/* Iterate throw array entries */` |
|      3 | 2381 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      2 | 2382 | `		}else{` |
|      - | 2383 | `			const char *zData;` |
|      - | 2384 | `			int nLen;` |
|      - | 2385 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 2386 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2387 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 2388 | `			if( imp_data.bFirst ){` |
|      9 | 2389 | `				imp_data.bFirst = 0;` |
|      4 | 2390 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2391 | `				ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen);` |
|    ! 0 | 2392 | `			}` |
|      - | 2393 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 2394 | `			if( nLen > 0 ){` |
|      9 | 2395 | `				ph7_result_string(pCtx,zData,nLen);` |
|      4 | 2396 | `			}` |
|      - | 2397 | `		}` |
|     11 | 2398 | `		i++;` |
|      1 | 2399 | `	}` |
|     11 | 2400 | `	return PH7_OK;` |
|      7 | 2401 |  |
|      - | 2402 | `/*` |
|      - | 2403 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 2404 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 2405 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 2406 | ` * Parameters` |
|      - | 2407 | ` *  $delimiter` |
|      - | 2408 | ` *   The boundary string.` |
|      - | 2409 | ` * $string` |
|      - | 2410 | ` *   The input string.` |
|      - | 2411 | ` * $limit` |
|      - | 2412 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 2413 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 2414 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 2415 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 2416 | ` * Returns` |
|      - | 2417 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 2418 | ` *  on boundaries formed by the delimiter.` |
|      - | 2419 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 2420 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 2421 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 2422 | ` *  will be returned.` |
|      - | 2423 | ` * NOTE:` |
|      - | 2424 | ` *  Negative limit is not supported.` |
|      - | 2425 | ` */` |
|   2646 | 2426 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2427 |  |
|      - | 2428 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 2429 | `	int nDelim,nStrlen,iLimit;` |
|      - | 2430 | `	ph7_value *pArray;` |
|      - | 2431 | `	ph7_value *pValue;` |
|      - | 2432 | `	sxu32 nOfft;` |
|      - | 2433 | `	sxi32 rc;` |
|   2647 | 2434 | `	if( nArg < 2 ){` |
|      - | 2435 | `		/* Missing arguments,return FALSE */` |
|      9 | 2436 | `		ph7_result_bool(pCtx,0);` |
|      9 | 2437 | `		return PH7_OK;` |
|      - | 2438 | `	}` |
|      - | 2439 | `	/* Extract the delimiter */` |
|   2639 | 2440 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   2639 | 2441 | `	if( nDelim < 1 ){` |
|      - | 2442 | `		/* Empty delimiter,return FALSE */` |
|      3 | 2443 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2444 | `		return PH7_OK;` |
|      - | 2445 | `	}` |
|      - | 2446 | `	/* Extract the string */` |
|   2637 | 2447 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   2637 | 2448 | `	if( nStrlen < 1 ){` |
|      - | 2449 | `		/* Empty string: return an array with a single empty element (PHP behavior) */` |
|      3 | 2450 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      3 | 2451 | `		ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      3 | 2452 | `		if( pArrayTmp == 0 \|\| pValueTmp == 0 ){` |
|      - | 2453 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 2454 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2455 | `			return PH7_OK;` |
|      - | 2456 | `		}` |
|      3 | 2457 | `		ph7_value_string(pValueTmp, "", 0);` |
|      3 | 2458 | `		ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp);` |
|      3 | 2459 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      3 | 2460 | `		return PH7_OK;` |
|      - | 2461 | `	}` |
|      - | 2462 | `	/* Point to the end of the string */` |
|   2635 | 2463 | `	zEnd = &zString[nStrlen];` |
|      - | 2464 | `	/* Create the array */` |
|   2635 | 2465 | `	pArray =  ph7_context_new_array(pCtx);` |
|   2635 | 2466 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   2635 | 2467 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2468 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 2469 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2470 | `		return PH7_OK;` |
|      - | 2471 | `	}` |
|      - | 2472 | `	/* Set a defualt limit */` |
|   2635 | 2473 | `	iLimit = SXI32_HIGH;` |
|   2635 | 2474 | `	if( nArg > 2 ){` |
|      9 | 2475 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|      9 | 2476 | `		 if( iLimit < 0 ){` |
|      3 | 2477 | `			iLimit = -iLimit;` |
|      1 | 2478 | `		}` |
|      9 | 2479 | `		if( iLimit == 0 ){` |
|      3 | 2480 | `			iLimit = 1;` |
|      1 | 2481 | `		}` |
|      9 | 2482 | `		iLimit--;` |
|      4 | 2483 | `	}` |
|      - | 2484 | `	/* Start exploding */` |
|  32551 | 2485 | `	for(;;){` |
|  65103 | 2486 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
|  65103 | 2487 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 2488 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   2635 | 2489 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   2635 | 2490 | `			ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|   2635 | 2491 | `			break;` |
|      - | 2492 | `		}` |
|      - | 2493 | `		/* Point to the desired offset */` |
|  62469 | 2494 | `		zCur = &zString[nOfft];` |
|      - | 2495 | `		/* Perform the store operation (may be empty) */` |
|  62469 | 2496 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|  62469 | 2497 | `		ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|      - | 2498 | `		/* Point beyond the delimiter */` |
|  62469 | 2499 | `		zString = &zCur[nDelim];` |
|      - | 2500 | `		/* Reset the cursor */` |
|  62469 | 2501 | `		ph7_value_reset_string_cursor(pValue);` |
|      1 | 2502 | `	}` |
|      - | 2503 | `	/* Return the freshly created array */` |
|   2635 | 2504 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2505 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 2506 | `	 * released as soon we return from this foregin function.` |
|      - | 2507 | `	 */` |
|   2635 | 2508 | `	return PH7_OK;` |
|   1324 | 2509 |  |
|      - | 2510 | `/*` |
|      - | 2511 | ` * string trim(string $str[,string $charlist ])` |
|      - | 2512 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2513 | ` * Parameters` |
|      - | 2514 | ` *  $str` |
|      - | 2515 | ` *   The string that will be trimmed.` |
|      - | 2516 | ` * $charlist` |
|      - | 2517 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2518 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2519 | ` *   With .. you can specify a range of characters.` |
|      - | 2520 | ` * Returns.` |
|      - | 2521 | ` *  Thr processed string.` |
|      - | 2522 | ` * NOTE:` |
|      - | 2523 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 2524 | ` */` |
|   6530 | 2525 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2526 |  |
|      - | 2527 | `	const char *zString;` |
|      - | 2528 | `	int nLen;` |
|   6532 | 2529 | `	if( nArg < 1 ){` |
|      - | 2530 | `		/* Missing arguments,return null */` |
|      3 | 2531 | `		ph7_result_null(pCtx);` |
|      3 | 2532 | `		return PH7_OK;` |
|      - | 2533 | `	}` |
|      - | 2534 | `	/* Extract the target string */` |
|   6530 | 2535 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|   6530 | 2536 | `	if( nLen < 1 ){` |
|      - | 2537 | `		/* Empty string,return */` |
|   1291 | 2538 | `		ph7_result_string(pCtx,"",0);` |
|   1291 | 2539 | `		return PH7_OK;` |
|      - | 2540 | `	}` |
|      - | 2541 | `	/* Start the trim process */` |
|   5240 | 2542 | `	if( nArg < 2 ){` |
|      - | 2543 | `		SyString sStr;` |
|      - | 2544 | `		/* Remove white spaces and NUL bytes */` |
|   5236 | 2545 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  12666 | 2546 | `		SyStringFullTrimSafe(&sStr);` |
|   5236 | 2547 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   2619 | 2548 | `	}else{` |
|      - | 2549 | `		/* Char list */` |
|      - | 2550 | `		const char *zList;` |
|      - | 2551 | `		int nListlen;` |
|      5 | 2552 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 2553 | `		if( nListlen < 1 ){` |
|      - | 2554 | `			/* Return the string unchanged */` |
|      3 | 2555 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2556 | `		}else{` |
|      3 | 2557 | `			const char *zEnd = &zString[nLen];` |
|      3 | 2558 | `			const char *zCur = zString;` |
|      - | 2559 | `			const char *zPtr;` |
|      - | 2560 | `			int i;` |
|      - | 2561 | `			/* Left trim */` |
|      4 | 2562 | `			for(;;){` |
|      9 | 2563 | `				if( zCur >= zEnd ){` |
|    ! 0 | 2564 | `					break;` |
|      - | 2565 | `				}` |
|      9 | 2566 | `				zPtr = zCur;` |
|     17 | 2567 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 2568 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|      7 | 2569 | `						zCur++;` |
|      3 | 2570 | `					}` |
|      5 | 2571 | `				}` |
|      9 | 2572 | `				if( zCur == zPtr ){` |
|      - | 2573 | `					/* No match,break immediately */` |
|      3 | 2574 | `					break;` |
|      - | 2575 | `				}` |
|      1 | 2576 | `			}` |
|      - | 2577 | `			/* Right trim */` |
|      3 | 2578 | `			zEnd--;` |
|      4 | 2579 | `			for(;;){` |
|      9 | 2580 | `				if( zEnd <= zCur ){` |
|    ! 0 | 2581 | `					break;` |
|      - | 2582 | `				}` |
|      9 | 2583 | `				zPtr = zEnd;` |
|     17 | 2584 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 2585 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      7 | 2586 | `						zEnd--;` |
|      3 | 2587 | `					}` |
|      5 | 2588 | `				}` |
|      9 | 2589 | `				if( zEnd == zPtr ){` |
|      3 | 2590 | `					break;` |
|      - | 2591 | `				}` |
|      1 | 2592 | `			}` |
|      3 | 2593 | `			if( zCur >= zEnd ){` |
|      - | 2594 | `				/* Return the empty string */` |
|    ! 0 | 2595 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2596 | `			}else{` |
|      3 | 2597 | `				zEnd++;` |
|      3 | 2598 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2599 | `			}` |
|      - | 2600 | `		}` |
|      - | 2601 | `	}` |
|   5240 | 2602 | `	return PH7_OK;` |
|   3267 | 2603 |  |
|      - | 2604 | `/*` |
|      - | 2605 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 2606 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 2607 | ` * Parameters` |
|      - | 2608 | ` *  $str` |
|      - | 2609 | ` *   The string that will be trimmed.` |
|      - | 2610 | ` * $charlist` |
|      - | 2611 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2612 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2613 | ` *   With .. you can specify a range of characters.` |
|      - | 2614 | ` * Returns.` |
|      - | 2615 | ` *  Thr processed string.` |
|      - | 2616 | ` * NOTE:` |
|      - | 2617 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 2618 | ` */` |
|     26 | 2619 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2620 |  |
|      - | 2621 | `	const char *zString;` |
|      - | 2622 | `	int nLen;` |
|     27 | 2623 | `	if( nArg < 1 ){` |
|      - | 2624 | `		/* Missing arguments,return null */` |
|      3 | 2625 | `		ph7_result_null(pCtx);` |
|      3 | 2626 | `		return PH7_OK;` |
|      - | 2627 | `	}` |
|      - | 2628 | `	/* Extract the target string */` |
|     25 | 2629 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 2630 | `	if( nLen < 1 ){` |
|      - | 2631 | `		/* Empty string,return */` |
|      5 | 2632 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2633 | `		return PH7_OK;` |
|      - | 2634 | `	}` |
|      - | 2635 | `	/* Start the trim process */` |
|     21 | 2636 | `	if( nArg < 2 ){` |
|      - | 2637 | `		SyString sStr;` |
|      - | 2638 | `		/* Remove white spaces and NUL bytes*/` |
|     17 | 2639 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     41 | 2640 | `		SyStringRightTrimSafe(&sStr);` |
|     17 | 2641 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      9 | 2642 | `	}else{` |
|      - | 2643 | `		/* Char list */` |
|      - | 2644 | `		const char *zList;` |
|      - | 2645 | `		int nListlen;` |
|      5 | 2646 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 2647 | `		if( nListlen < 1 ){` |
|      - | 2648 | `			/* Return the string unchanged */` |
|    ! 0 | 2649 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 2650 | `		}else{` |
|      5 | 2651 | `			const char *zEnd = &zString[nLen - 1];` |
|      5 | 2652 | `			const char *zCur = zString;` |
|      - | 2653 | `			const char *zPtr;` |
|      - | 2654 | `			int i;` |
|      - | 2655 | `			/* Right trim */` |
|      6 | 2656 | `			for(;;){` |
|     13 | 2657 | `				if( zEnd <= zCur ){` |
|    ! 0 | 2658 | `					break;` |
|      - | 2659 | `				}` |
|     13 | 2660 | `				zPtr = zEnd;` |
|     25 | 2661 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     13 | 2662 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      9 | 2663 | `						zEnd--;` |
|      4 | 2664 | `					}` |
|      7 | 2665 | `				}` |
|     13 | 2666 | `				if( zEnd == zPtr ){` |
|      5 | 2667 | `					break;` |
|      - | 2668 | `				}` |
|      1 | 2669 | `			}` |
|      5 | 2670 | `			if( zEnd <= zCur ){` |
|      - | 2671 | `				/* Return the empty string */` |
|    ! 0 | 2672 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2673 | `			}else{` |
|      5 | 2674 | `				zEnd++;` |
|      5 | 2675 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2676 | `			}` |
|      - | 2677 | `		}` |
|      - | 2678 | `	}` |
|     21 | 2679 | `	return PH7_OK;` |
|     14 | 2680 |  |
|      - | 2681 | `/*` |
|      - | 2682 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 2683 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2684 | ` * Parameters` |
|      - | 2685 | ` *  $str` |
|      - | 2686 | ` *   The string that will be trimmed.` |
|      - | 2687 | ` * $charlist` |
|      - | 2688 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2689 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2690 | ` *   With .. you can specify a range of characters.` |
|      - | 2691 | ` * Returns.` |
|      - | 2692 | ` *  Thr processed string.` |
|      - | 2693 | ` * NOTE:` |
|      - | 2694 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 2695 | ` */` |
|     12 | 2696 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2697 |  |
|      - | 2698 | `	const char *zString;` |
|      - | 2699 | `	int nLen;` |
|     13 | 2700 | `	if( nArg < 1 ){` |
|      - | 2701 | `		/* Missing arguments,return null */` |
|      3 | 2702 | `		ph7_result_null(pCtx);` |
|      3 | 2703 | `		return PH7_OK;` |
|      - | 2704 | `	}` |
|      - | 2705 | `	/* Extract the target string */` |
|     11 | 2706 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 2707 | `	if( nLen < 1 ){` |
|      - | 2708 | `		/* Empty string,return */` |
|    ! 0 | 2709 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2710 | `		return PH7_OK;` |
|      - | 2711 | `	}` |
|      - | 2712 | `	/* Start the trim process */` |
|     11 | 2713 | `	if( nArg < 2 ){` |
|      - | 2714 | `		SyString sStr;` |
|      - | 2715 | `		/* Remove white spaces and NUL byte */` |
|      3 | 2716 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|      8 | 2717 | `		SyStringLeftTrimSafe(&sStr);` |
|      3 | 2718 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      2 | 2719 | `	}else{` |
|      - | 2720 | `		/* Char list */` |
|      - | 2721 | `		const char *zList;` |
|      - | 2722 | `		int nListlen;` |
|      9 | 2723 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      9 | 2724 | `		if( nListlen < 1 ){` |
|      - | 2725 | `			/* Return the string unchanged */` |
|      3 | 2726 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2727 | `		}else{` |
|      7 | 2728 | `			const char *zEnd = &zString[nLen];` |
|      7 | 2729 | `			const char *zCur = zString;` |
|      - | 2730 | `			const char *zPtr;` |
|      - | 2731 | `			int i;` |
|      - | 2732 | `			/* Left trim */` |
|      7 | 2733 | `			for(;;){` |
|     15 | 2734 | `				if( zCur >= zEnd ){` |
|    ! 0 | 2735 | `					break;` |
|      - | 2736 | `				}` |
|     15 | 2737 | `				zPtr = zCur;` |
|     41 | 2738 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     27 | 2739 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|     13 | 2740 | `						zCur++;` |
|      6 | 2741 | `					}` |
|     14 | 2742 | `				}` |
|     15 | 2743 | `				if( zCur == zPtr ){` |
|      - | 2744 | `					/* No match,break immediately */` |
|      7 | 2745 | `					break;` |
|      - | 2746 | `				}` |
|      1 | 2747 | `			}` |
|      7 | 2748 | `			if( zCur >= zEnd ){` |
|      - | 2749 | `				/* Return the empty string */` |
|    ! 0 | 2750 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2751 | `			}else{` |
|      7 | 2752 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2753 | `			}` |
|      - | 2754 | `		}` |
|      - | 2755 | `	}` |
|     11 | 2756 | `	return PH7_OK;` |
|      7 | 2757 |  |
|      - | 2758 | `/*` |
|      - | 2759 | ` * string strtolower(string $str)` |
|      - | 2760 | ` *  Make a string lowercase.` |
|      - | 2761 | ` * Parameters` |
|      - | 2762 | ` *  $str` |
|      - | 2763 | ` *   The input string.` |
|      - | 2764 | ` * Returns.` |
|      - | 2765 | ` *  The lowercased string.` |
|      - | 2766 | ` */` |
|  14372 | 2767 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2768 |  |
|      - | 2769 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2770 | `	int nLen;` |
|  14373 | 2771 | `	if( nArg < 1 ){` |
|      - | 2772 | `		/* Missing arguments,return null */` |
|      3 | 2773 | `		ph7_result_null(pCtx);` |
|      3 | 2774 | `		return PH7_OK;` |
|      - | 2775 | `	}` |
|      - | 2776 | `	/* Extract the target string */` |
|  14371 | 2777 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  14371 | 2778 | `	if( nLen < 1 ){` |
|      - | 2779 | `		/* Empty string,return */` |
|      3 | 2780 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2781 | `		return PH7_OK;` |
|      - | 2782 | `	}` |
|      - | 2783 | `	/* Perform the requested operation */` |
|  14369 | 2784 | `	zEnd = &zString[nLen];` |
|  45283 | 2785 | `	for(;;){` |
|  90567 | 2786 | `		if( zString >= zEnd ){` |
|      - | 2787 | `			/* No more input,break immediately */` |
|  14369 | 2788 | `			break;` |
|      - | 2789 | `		}` |
|  76199 | 2790 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2791 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2792 | `			zCur = zString;` |
|    ! 0 | 2793 | `			zString++;` |
|    ! 0 | 2794 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2795 | `				zString++;` |
|    ! 0 | 2796 | `			}` |
|      - | 2797 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2798 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2799 | `		}else{` |
|  76199 | 2800 | `			int c = zString[0];` |
|  76199 | 2801 | `			if( SyisUpper(c) ){` |
|  76197 | 2802 | `				c = SyToLower(zString[0]);` |
|  38098 | 2803 | `			}` |
|      - | 2804 | `			/* Append character */` |
|  76199 | 2805 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2806 | `			/* Advance the cursor */` |
|  76199 | 2807 | `			zString++;` |
|      - | 2808 | `		}` |
|      1 | 2809 | `	}` |
|  14369 | 2810 | `	return PH7_OK;` |
|   7187 | 2811 |  |
|      - | 2812 | `/*` |
|      - | 2813 | ` * string strtolower(string $str)` |
|      - | 2814 | ` *  Make a string uppercase.` |
|      - | 2815 | ` * Parameters` |
|      - | 2816 | ` *  $str` |
|      - | 2817 | ` *   The input string.` |
|      - | 2818 | ` * Returns.` |
|      - | 2819 | ` *  The uppercased string.` |
|      - | 2820 | ` */` |
|     10 | 2821 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2822 |  |
|      - | 2823 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2824 | `	int nLen;` |
|     11 | 2825 | `	if( nArg < 1 ){` |
|      - | 2826 | `		/* Missing arguments,return null */` |
|      3 | 2827 | `		ph7_result_null(pCtx);` |
|      3 | 2828 | `		return PH7_OK;` |
|      - | 2829 | `	}` |
|      - | 2830 | `	/* Extract the target string */` |
|      9 | 2831 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 2832 | `	if( nLen < 1 ){` |
|      - | 2833 | `		/* Empty string,return */` |
|      3 | 2834 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2835 | `		return PH7_OK;` |
|      - | 2836 | `	}` |
|      - | 2837 | `	/* Perform the requested operation */` |
|      7 | 2838 | `	zEnd = &zString[nLen];` |
|     19 | 2839 | `	for(;;){` |
|     39 | 2840 | `		if( zString >= zEnd ){` |
|      - | 2841 | `			/* No more input,break immediately */` |
|      7 | 2842 | `			break;` |
|      - | 2843 | `		}` |
|     33 | 2844 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2845 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2846 | `			zCur = zString;` |
|    ! 0 | 2847 | `			zString++;` |
|    ! 0 | 2848 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2849 | `				zString++;` |
|    ! 0 | 2850 | `			}` |
|      - | 2851 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2852 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2853 | `		}else{` |
|     33 | 2854 | `			int c = zString[0];` |
|     33 | 2855 | `			if( SyisLower(c) ){` |
|     27 | 2856 | `				c = SyToUpper(zString[0]);` |
|     13 | 2857 | `			}` |
|      - | 2858 | `			/* Append character */` |
|     33 | 2859 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2860 | `			/* Advance the cursor */` |
|     33 | 2861 | `			zString++;` |
|      - | 2862 | `		}` |
|      1 | 2863 | `	}` |
|      7 | 2864 | `	return PH7_OK;` |
|      6 | 2865 |  |
|      - | 2866 | `/*` |
|      - | 2867 | ` * string ucfirst(string $str)` |
|      - | 2868 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2869 | ` *  character is alphabetic.` |
|      - | 2870 | ` * Parameters` |
|      - | 2871 | ` *  $str` |
|      - | 2872 | ` *   The input string.` |
|      - | 2873 | ` * Returns.` |
|      - | 2874 | ` *  The processed string.` |
|      - | 2875 | ` */` |
|      6 | 2876 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2877 |  |
|      - | 2878 | `	const char *zString,*zEnd;` |
|      - | 2879 | `	int nLen,c;` |
|      7 | 2880 | `	if( nArg < 1 ){` |
|      - | 2881 | `		/* Missing arguments,return null */` |
|      3 | 2882 | `		ph7_result_null(pCtx);` |
|      3 | 2883 | `		return PH7_OK;` |
|      - | 2884 | `	}` |
|      - | 2885 | `	/* Extract the target string */` |
|      5 | 2886 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2887 | `	if( nLen < 1 ){` |
|      - | 2888 | `		/* Empty string,return */` |
|      3 | 2889 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2890 | `		return PH7_OK;` |
|      - | 2891 | `	}` |
|      - | 2892 | `	/* Perform the requested operation */` |
|      3 | 2893 | `	zEnd = &zString[nLen];` |
|      3 | 2894 | `	c = zString[0];` |
|      3 | 2895 | `	if( SyisLower(c) ){` |
|      3 | 2896 | `		c = SyToUpper(c);` |
|      1 | 2897 | `	}` |
|      - | 2898 | `	/* Append the first character */` |
|      3 | 2899 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2900 | `	zString++;` |
|      3 | 2901 | `	if( zString < zEnd ){` |
|      - | 2902 | `		/* Append the rest of the input verbatim */` |
|      3 | 2903 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2904 | `	}` |
|      3 | 2905 | `	return PH7_OK;` |
|      4 | 2906 |  |
|      - | 2907 | `/*` |
|      - | 2908 | ` * string lcfirst(string $str)` |
|      - | 2909 | ` *  Make a string's first character lowercase.` |
|      - | 2910 | ` * Parameters` |
|      - | 2911 | ` *  $str` |
|      - | 2912 | ` *   The input string.` |
|      - | 2913 | ` * Returns.` |
|      - | 2914 | ` *  The processed string.` |
|      - | 2915 | ` */` |
|      6 | 2916 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2917 |  |
|      - | 2918 | `	const char *zString,*zEnd;` |
|      - | 2919 | `	int nLen,c;` |
|      7 | 2920 | `	if( nArg < 1 ){` |
|      - | 2921 | `		/* Missing arguments,return null */` |
|      3 | 2922 | `		ph7_result_null(pCtx);` |
|      3 | 2923 | `		return PH7_OK;` |
|      - | 2924 | `	}` |
|      - | 2925 | `	/* Extract the target string */` |
|      5 | 2926 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2927 | `	if( nLen < 1 ){` |
|      - | 2928 | `		/* Empty string,return */` |
|      3 | 2929 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2930 | `		return PH7_OK;` |
|      - | 2931 | `	}` |
|      - | 2932 | `	/* Perform the requested operation */` |
|      3 | 2933 | `	zEnd = &zString[nLen];` |
|      3 | 2934 | `	c = zString[0];` |
|      3 | 2935 | `	if( SyisUpper(c) ){` |
|      3 | 2936 | `		c = SyToLower(c);` |
|      1 | 2937 | `	}` |
|      - | 2938 | `	/* Append the first character */` |
|      3 | 2939 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2940 | `	zString++;` |
|      3 | 2941 | `	if( zString < zEnd ){` |
|      - | 2942 | `		/* Append the rest of the input verbatim */` |
|      3 | 2943 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2944 | `	}` |
|      3 | 2945 | `	return PH7_OK;` |
|      4 | 2946 |  |
|      - | 2947 | `/*` |
|      - | 2948 | ` * int ord(string $string)` |
|      - | 2949 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 2950 | ` * Parameters` |
|      - | 2951 | ` *  $str` |
|      - | 2952 | ` *   The input string.` |
|      - | 2953 | ` * Returns.` |
|      - | 2954 | ` *  The ASCII value as an integer.` |
|      - | 2955 | ` */` |
|     32 | 2956 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2957 |  |
|      - | 2958 | `	const char *zString;` |
|      - | 2959 | `	int nLen,c;` |
|     33 | 2960 | `	if( nArg < 1 ){` |
|      - | 2961 | `		/* Missing arguments,return -1 */` |
|      3 | 2962 | `		ph7_result_int(pCtx,-1);` |
|      3 | 2963 | `		return PH7_OK;` |
|      - | 2964 | `	}` |
|      - | 2965 | `	/* Extract the target string */` |
|     31 | 2966 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 2967 | `	if( nLen < 1 ){` |
|      - | 2968 | `		/* Empty string,return -1 */` |
|      3 | 2969 | `		ph7_result_int(pCtx,-1);` |
|      3 | 2970 | `		return PH7_OK;` |
|      - | 2971 | `	}` |
|      - | 2972 | `	/* Extract the ASCII value of the first character */` |
|     29 | 2973 | `	c = zString[0];` |
|      - | 2974 | `	/* Return that value */` |
|     29 | 2975 | `	ph7_result_int(pCtx,c);` |
|     29 | 2976 | `	return PH7_OK;` |
|     17 | 2977 |  |
|      - | 2978 | `/*` |
|      - | 2979 | ` * string chr(int $ascii)` |
|      - | 2980 | ` *  Returns a one-character string containing the character specified by ascii.` |
|      - | 2981 | ` * Parameters` |
|      - | 2982 | ` *  $ascii` |
|      - | 2983 | ` *   The ascii code.` |
|      - | 2984 | ` * Returns.` |
|      - | 2985 | ` *  The specified character.` |
|      - | 2986 | ` */` |
|     28 | 2987 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2988 |  |
|      - | 2989 | `	int c;` |
|     29 | 2990 | `	if( nArg < 1 ){` |
|      - | 2991 | `		/* Missing arguments,return null */` |
|      3 | 2992 | `		ph7_result_null(pCtx);` |
|      3 | 2993 | `		return PH7_OK;` |
|      - | 2994 | `	}` |
|      - | 2995 | `	/* Extract the ASCII value */` |
|     27 | 2996 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 2997 | `	/* Return the specified character */` |
|     27 | 2998 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     27 | 2999 | `	return PH7_OK;` |
|     15 | 3000 |  |
|      - | 3001 | `/*` |
|      - | 3002 | ` * Binary to hex consumer callback.` |
|      - | 3003 | ` * This callback is the default consumer used by the hash functions` |
|      - | 3004 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 3005 | ` */` |
|    226 | 3006 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 3007 |  |
|      - | 3008 | `	/* Append hex chunk verbatim */` |
|    227 | 3009 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|    227 | 3010 | `	return SXRET_OK;` |
|      1 | 3011 |  |
|      - | 3012 | `/*` |
|      - | 3013 | ` * string bin2hex(string $str)` |
|      - | 3014 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 3015 | ` * Parameters` |
|      - | 3016 | ` *  $str` |
|      - | 3017 | ` *   The input string.` |
|      - | 3018 | ` * Returns.` |
|      - | 3019 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 3020 | ` */` |
|     12 | 3021 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3022 |  |
|      - | 3023 | `	const char *zString;` |
|      - | 3024 | `	int nLen;` |
|     13 | 3025 | `	if( nArg < 1 ){` |
|      - | 3026 | `		/* Missing arguments,return null */` |
|      3 | 3027 | `		ph7_result_null(pCtx);` |
|      3 | 3028 | `		return PH7_OK;` |
|      - | 3029 | `	}` |
|      - | 3030 | `	/* Extract the target string */` |
|     11 | 3031 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 3032 | `	if( nLen < 1 ){` |
|      - | 3033 | `		/* Empty string,return */` |
|      3 | 3034 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3035 | `		return PH7_OK;` |
|      - | 3036 | `	}` |
|      - | 3037 | `	/* Perform the requested operation */` |
|      9 | 3038 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|      9 | 3039 | `	return PH7_OK;` |
|      7 | 3040 |  |
|      - | 3041 | `/* Search callback signature */` |
|      - | 3042 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 3043 | `/*` |
|      - | 3044 | ` * Case-insensitive pattern match.` |
|      - | 3045 | ` * Brute force is the default search method used here.` |
|      - | 3046 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 3047 | ` * well for short/medium texts on modern hardware.` |
|      - | 3048 | ` */` |
|    118 | 3049 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      1 | 3050 |  |
|    119 | 3051 | `	const char *zpIn = (const char *)pPattern;` |
|    119 | 3052 | `	const char *zIn = (const char *)pText;` |
|    119 | 3053 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    119 | 3054 | `	const char *zEnd = &zIn[nLen];` |
|      - | 3055 | `	const char *zPtr,*zPtr2;` |
|      - | 3056 | `	int c,d;` |
|    119 | 3057 | `	if( iPatLen > nLen ){` |
|      - | 3058 | `		/* Don't bother processing */` |
|     33 | 3059 | `		return SXERR_NOTFOUND;` |
|      - | 3060 | `	}` |
|    244 | 3061 | `	for(;;){` |
|    489 | 3062 | `		if( zIn >= zEnd ){` |
|     47 | 3063 | `			break;` |
|      - | 3064 | `		}` |
|    443 | 3065 | `		c = SyToLower(zIn[0]);` |
|    443 | 3066 | `		d = SyToLower(zpIn[0]);` |
|    443 | 3067 | `		if( c == d ){` |
|     41 | 3068 | `			zPtr   = &zIn[1];` |
|     41 | 3069 | `			zPtr2  = &zpIn[1];` |
|     71 | 3070 | `			for(;;){` |
|    143 | 3071 | `				if( zPtr2 >= zpEnd ){` |
|      - | 3072 | `					/* Pattern found */` |
|     41 | 3073 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 3074 | `					return SXRET_OK;` |
|      - | 3075 | `				}` |
|    103 | 3076 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 3077 | `					break;` |
|      - | 3078 | `				}` |
|    103 | 3079 | `				c = SyToLower(zPtr[0]);` |
|    103 | 3080 | `				d = SyToLower(zPtr2[0]);` |
|    103 | 3081 | `				if( c != d ){` |
|    ! 0 | 3082 | `					break;` |
|      - | 3083 | `				}` |
|    103 | 3084 | `				zPtr++; zPtr2++;` |
|      1 | 3085 | `			}` |
|    ! 0 | 3086 | `		}` |
|    403 | 3087 | `		zIn++;` |
|      1 | 3088 | `	}` |
|      - | 3089 | `	/* Pattern not found */` |
|     47 | 3090 | `	return SXERR_NOTFOUND;` |
|     60 | 3091 |  |
|      - | 3092 | `/*` |
|      - | 3093 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3094 | ` *  Find the first occurrence of a string.` |
|      - | 3095 | ` * Parameters` |
|      - | 3096 | ` *  $haystack` |
|      - | 3097 | ` *   The input string.` |
|      - | 3098 | ` * $needle` |
|      - | 3099 | ` *   Search pattern (must be a string).` |
|      - | 3100 | ` * $before_needle` |
|      - | 3101 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3102 | ` *   of the needle (excluding the needle).` |
|      - | 3103 | ` * Return` |
|      - | 3104 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3105 | ` */` |
|     10 | 3106 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3107 |  |
|     11 | 3108 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3109 | `	const char *zBlob,*zPattern;` |
|      - | 3110 | `	int nLen,nPatLen;` |
|      - | 3111 | `	sxu32 nOfft;` |
|      - | 3112 | `	sxi32 rc;` |
|     11 | 3113 | `	if( nArg < 2 ){` |
|      - | 3114 | `		/* Missing arguments,return FALSE */` |
|      5 | 3115 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3116 | `		return PH7_OK;` |
|      - | 3117 | `	}` |
|      - | 3118 | `	/* Extract the needle and the haystack */` |
|      7 | 3119 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 3120 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 3121 | `	nOfft = 0; /* cc warning */` |
|      9 | 3122 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3123 | `		int before = 0;` |
|      - | 3124 | `		/* Perform the lookup */` |
|      5 | 3125 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3126 | `		if( rc != SXRET_OK ){` |
|      - | 3127 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3128 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3129 | `			return PH7_OK;` |
|      - | 3130 | `		}` |
|      - | 3131 | `		/* Return the portion of the string */` |
|      5 | 3132 | `		if( nArg > 2 ){` |
|      3 | 3133 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3134 | `		}` |
|      5 | 3135 | `		if( before ){` |
|      3 | 3136 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3137 | `		}else{` |
|      3 | 3138 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3139 | `		}` |
|      3 | 3140 | `	}else{` |
|      3 | 3141 | `		ph7_result_bool(pCtx,0);` |
|      - | 3142 | `	}` |
|      7 | 3143 | `	return PH7_OK;` |
|      6 | 3144 |  |
|      - | 3145 | `/*` |
|      - | 3146 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3147 | ` *  Case-insensitive strstr().` |
|      - | 3148 | ` * Parameters` |
|      - | 3149 | ` *  $haystack` |
|      - | 3150 | ` *   The input string.` |
|      - | 3151 | ` * $needle` |
|      - | 3152 | ` *   Search pattern (must be a string).` |
|      - | 3153 | ` * $before_needle` |
|      - | 3154 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3155 | ` *   of the needle (excluding the needle).` |
|      - | 3156 | ` * Return` |
|      - | 3157 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3158 | ` */` |
|      6 | 3159 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3160 |  |
|      7 | 3161 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3162 | `	const char *zBlob,*zPattern;` |
|      - | 3163 | `	int nLen,nPatLen;` |
|      - | 3164 | `	sxu32 nOfft;` |
|      - | 3165 | `	sxi32 rc;` |
|      7 | 3166 | `	if( nArg < 2 ){` |
|      - | 3167 | `		/* Missing arguments,return FALSE */` |
|      3 | 3168 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3169 | `		return PH7_OK;` |
|      - | 3170 | `	}` |
|      - | 3171 | `	/* Extract the needle and the haystack */` |
|      5 | 3172 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3173 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 3174 | `	nOfft = 0; /* cc warning */` |
|      7 | 3175 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3176 | `		int before = 0;` |
|      - | 3177 | `		/* Perform the lookup */` |
|      5 | 3178 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3179 | `		if( rc != SXRET_OK ){` |
|      - | 3180 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3181 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3182 | `			return PH7_OK;` |
|      - | 3183 | `		}` |
|      - | 3184 | `		/* Return the portion of the string */` |
|      5 | 3185 | `		if( nArg > 2 ){` |
|      3 | 3186 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3187 | `		}` |
|      5 | 3188 | `		if( before ){` |
|      3 | 3189 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3190 | `		}else{` |
|      3 | 3191 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3192 | `		}` |
|      3 | 3193 | `	}else{` |
|    ! 0 | 3194 | `		ph7_result_bool(pCtx,0);` |
|      - | 3195 | `	}` |
|      5 | 3196 | `	return PH7_OK;` |
|      4 | 3197 |  |
|      - | 3198 | `/*` |
|      - | 3199 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3200 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 3201 | ` * Parameters` |
|      - | 3202 | ` *  $haystack` |
|      - | 3203 | ` *   The input string.` |
|      - | 3204 | ` * $needle` |
|      - | 3205 | ` *   Search pattern (must be a string).` |
|      - | 3206 | ` * $offset` |
|      - | 3207 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3208 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3209 | ` *   of haystack.` |
|      - | 3210 | ` * Return` |
|      - | 3211 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3212 | ` */` |
|     78 | 3213 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3214 |  |
|     80 | 3215 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3216 | `	const char *zBlob,*zPattern;` |
|      - | 3217 | `	int nLen,nPatLen,nStart;` |
|      - | 3218 | `	sxu32 nOfft;` |
|      - | 3219 | `	sxi32 rc;` |
|     80 | 3220 | `	if( nArg < 2 ){` |
|      - | 3221 | `		/* Missing arguments,return FALSE */` |
|      7 | 3222 | `		ph7_result_bool(pCtx,0);` |
|      7 | 3223 | `		return PH7_OK;` |
|      - | 3224 | `	}` |
|      - | 3225 | `	/* Extract the needle and the haystack */` |
|     74 | 3226 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     74 | 3227 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     74 | 3228 | `	nOfft = 0; /* cc warning */` |
|     74 | 3229 | `	nStart = 0;` |
|      - | 3230 | `	/* Peek the starting offset if available */` |
|     74 | 3231 | `	if( nArg > 2 ){` |
|    ! 0 | 3232 | `		nStart = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 3233 | `		if( nStart < 0 ){` |
|    ! 0 | 3234 | `			nStart = -nStart;` |
|    ! 0 | 3235 | `		}` |
|    ! 0 | 3236 | `		if( nStart >= nLen ){` |
|      - | 3237 | `			/* Invalid offset */` |
|    ! 0 | 3238 | `			nStart = 0;` |
|    ! 0 | 3239 | `		}else{` |
|    ! 0 | 3240 | `			zBlob += nStart;` |
|    ! 0 | 3241 | `			nLen -= nStart;` |
|      - | 3242 | `		}` |
|    ! 0 | 3243 | `	}` |
|     74 | 3244 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3245 | `		/* Perform the lookup */` |
|     72 | 3246 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     72 | 3247 | `		if( rc != SXRET_OK ){` |
|      - | 3248 | `			/* Pattern not found,return FALSE */` |
|      3 | 3249 | `			ph7_result_bool(pCtx,0);` |
|      3 | 3250 | `			return PH7_OK;` |
|      - | 3251 | `		}` |
|      - | 3252 | `		/* Return the pattern position */` |
|     70 | 3253 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|     36 | 3254 | `	}else{` |
|      3 | 3255 | `		ph7_result_bool(pCtx,0);` |
|      - | 3256 | `	}` |
|     72 | 3257 | `	return PH7_OK;` |
|     41 | 3258 |  |
|      - | 3259 | `/*` |
|      - | 3260 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3261 | ` *  Case-insensitive strpos.` |
|      - | 3262 | ` * Parameters` |
|      - | 3263 | ` *  $haystack` |
|      - | 3264 | ` *   The input string.` |
|      - | 3265 | ` * $needle` |
|      - | 3266 | ` *   Search pattern (must be a string).` |
|      - | 3267 | ` * $offset` |
|      - | 3268 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3269 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3270 | ` *   of haystack.` |
|      - | 3271 | ` * Return` |
|      - | 3272 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3273 | ` */` |
|     18 | 3274 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3275 |  |
|     19 | 3276 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3277 | `	const char *zBlob,*zPattern;` |
|      - | 3278 | `	int nLen,nPatLen,nStart;` |
|      - | 3279 | `	sxu32 nOfft;` |
|      - | 3280 | `	sxi32 rc;` |
|     19 | 3281 | `	if( nArg < 2 ){` |
|      - | 3282 | `		/* Missing arguments,return FALSE */` |
|      3 | 3283 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3284 | `		return PH7_OK;` |
|      - | 3285 | `	}` |
|      - | 3286 | `	/* Extract the needle and the haystack */` |
|     17 | 3287 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 3288 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     17 | 3289 | `	nOfft = 0; /* cc warning */` |
|     17 | 3290 | `	nStart = 0;` |
|      - | 3291 | `	/* Peek the starting offset if available */` |
|     17 | 3292 | `	if( nArg > 2 ){` |
|      5 | 3293 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 3294 | `		if( nStart < 0 ){` |
|      3 | 3295 | `			nStart = -nStart;` |
|      1 | 3296 | `		}` |
|      5 | 3297 | `		if( nStart >= nLen ){` |
|      - | 3298 | `			/* Invalid offset */` |
|    ! 0 | 3299 | `			nStart = 0;` |
|    ! 0 | 3300 | `		}else{` |
|      5 | 3301 | `			zBlob += nStart;` |
|      5 | 3302 | `			nLen -= nStart;` |
|      - | 3303 | `		}` |
|      2 | 3304 | `	}` |
|     17 | 3305 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3306 | `		/* Perform the lookup */` |
|     17 | 3307 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     17 | 3308 | `		if( rc != SXRET_OK ){` |
|      - | 3309 | `			/* Pattern not found,return FALSE */` |
|      3 | 3310 | `			ph7_result_bool(pCtx,0);` |
|      3 | 3311 | `			return PH7_OK;` |
|      - | 3312 | `		}` |
|      - | 3313 | `		/* Return the pattern position */` |
|     15 | 3314 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 3315 | `	}else{` |
|    ! 0 | 3316 | `		ph7_result_bool(pCtx,0);` |
|      - | 3317 | `	}` |
|     15 | 3318 | `	return PH7_OK;` |
|     10 | 3319 |  |
|      - | 3320 | `/*` |
|      - | 3321 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3322 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 3323 | ` * Parameters` |
|      - | 3324 | ` *  $haystack` |
|      - | 3325 | ` *   The input string.` |
|      - | 3326 | ` * $needle` |
|      - | 3327 | ` *   Search pattern (must be a string).` |
|      - | 3328 | ` * $offset` |
|      - | 3329 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3330 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3331 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3332 | ` * Return` |
|      - | 3333 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3334 | ` */` |
|     32 | 3335 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3336 |  |
|      - | 3337 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     33 | 3338 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3339 | `	int nLen,nPatLen;` |
|      - | 3340 | `	sxu32 nOfft;` |
|      - | 3341 | `	sxi32 rc;` |
|     33 | 3342 | `	if( nArg < 2 ){` |
|      - | 3343 | `		/* Missing arguments,return FALSE */` |
|      3 | 3344 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3345 | `		return PH7_OK;` |
|      - | 3346 | `	}` |
|      - | 3347 | `	/* Extract the needle and the haystack */` |
|     31 | 3348 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 3349 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3350 | `	/* Point to the end of the pattern */` |
|     31 | 3351 | `	zPtr = &zBlob[nLen - 1];` |
|     31 | 3352 | `	zEnd = &zBlob[nLen];` |
|      - | 3353 | `	/* Save the starting posistion */` |
|     31 | 3354 | `	zStart = zBlob;` |
|     31 | 3355 | `	nOfft = 0; /* cc warning */` |
|      - | 3356 | `	/* Peek the starting offset if available */` |
|     31 | 3357 | `	if( nArg > 2 ){` |
|      - | 3358 | `		int nStart;` |
|     21 | 3359 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 3360 | `		if( nStart < 0 ){` |
|     11 | 3361 | `			nStart = -nStart;` |
|     11 | 3362 | `			if( nStart >= nLen ){` |
|      - | 3363 | `				/* Invalid offset */` |
|      3 | 3364 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3365 | `				return PH7_OK;` |
|    ! 0 | 3366 | `			}else{` |
|      9 | 3367 | `				nLen -= nStart;` |
|      9 | 3368 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 3369 | `				zEnd = &zBlob[nLen];` |
|      - | 3370 | `			}` |
|      5 | 3371 | `		}else{` |
|     11 | 3372 | `			if( nStart >= nLen ){` |
|      - | 3373 | `				/* Invalid offset */` |
|      5 | 3374 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3375 | `				return PH7_OK;` |
|    ! 0 | 3376 | `			}else{` |
|      7 | 3377 | `				zBlob += nStart;` |
|      7 | 3378 | `				nLen -= nStart;` |
|      - | 3379 | `			}` |
|      - | 3380 | `		}` |
|      7 | 3381 | `	}` |
|     25 | 3382 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3383 | `		/* Perform the lookup */` |
|     57 | 3384 | `		for(;;){` |
|    115 | 3385 | `			if( zBlob >= zPtr ){` |
|     11 | 3386 | `				break;` |
|      - | 3387 | `			}` |
|    105 | 3388 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    105 | 3389 | `			if( rc == SXRET_OK ){` |
|      - | 3390 | `				/* Pattern found,return it's position */` |
|     13 | 3391 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 3392 | `				return PH7_OK;` |
|      - | 3393 | `			}` |
|     93 | 3394 | `			zPtr--;` |
|      1 | 3395 | `		}` |
|      - | 3396 | `		/* Pattern not found,return FALSE */` |
|     11 | 3397 | `		ph7_result_bool(pCtx,0);` |
|      6 | 3398 | `	}else{` |
|      3 | 3399 | `		ph7_result_bool(pCtx,0);` |
|      - | 3400 | `	}` |
|     13 | 3401 | `	return PH7_OK;` |
|     17 | 3402 |  |
|      - | 3403 | `/*` |
|      - | 3404 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3405 | ` *  Case-insensitive strrpos.` |
|      - | 3406 | ` * Parameters` |
|      - | 3407 | ` *  $haystack` |
|      - | 3408 | ` *   The input string.` |
|      - | 3409 | ` * $needle` |
|      - | 3410 | ` *   Search pattern (must be a string).` |
|      - | 3411 | ` * $offset` |
|      - | 3412 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3413 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3414 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3415 | ` * Return` |
|      - | 3416 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3417 | ` */` |
|     28 | 3418 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3419 |  |
|      - | 3420 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     29 | 3421 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3422 | `	int nLen,nPatLen;` |
|      - | 3423 | `	sxu32 nOfft;` |
|      - | 3424 | `	sxi32 rc;` |
|     29 | 3425 | `	if( nArg < 2 ){` |
|      - | 3426 | `		/* Missing arguments,return FALSE */` |
|      3 | 3427 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3428 | `		return PH7_OK;` |
|      - | 3429 | `	}` |
|      - | 3430 | `	/* Extract the needle and the haystack */` |
|     27 | 3431 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 3432 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3433 | `	/* Point to the end of the pattern */` |
|     27 | 3434 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 3435 | `	zEnd = &zBlob[nLen];` |
|      - | 3436 | `	/* Save the starting posistion */` |
|     27 | 3437 | `	zStart = zBlob;` |
|     27 | 3438 | `	nOfft = 0; /* cc warning */` |
|      - | 3439 | `	/* Peek the starting offset if available */` |
|     27 | 3440 | `	if( nArg > 2 ){` |
|      - | 3441 | `		int nStart;` |
|     15 | 3442 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3443 | `		if( nStart < 0 ){` |
|      7 | 3444 | `			nStart = -nStart;` |
|      7 | 3445 | `			if( nStart >= nLen ){` |
|      - | 3446 | `				/* Invalid offset */` |
|      3 | 3447 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3448 | `				return PH7_OK;` |
|    ! 0 | 3449 | `			}else{` |
|      5 | 3450 | `				nLen -= nStart;` |
|      5 | 3451 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 3452 | `				zEnd = &zBlob[nLen];` |
|      - | 3453 | `			}` |
|      3 | 3454 | `		}else{` |
|      9 | 3455 | `			if( nStart >= nLen ){` |
|      - | 3456 | `				/* Invalid offset */` |
|      5 | 3457 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3458 | `				return PH7_OK;` |
|    ! 0 | 3459 | `			}else{` |
|      5 | 3460 | `				zBlob += nStart;` |
|      5 | 3461 | `				nLen -= nStart;` |
|      - | 3462 | `			}` |
|      - | 3463 | `		}` |
|      4 | 3464 | `	}` |
|     21 | 3465 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3466 | `		/* Perform the lookup */` |
|     44 | 3467 | `		for(;;){` |
|     89 | 3468 | `			if( zBlob >= zPtr ){` |
|      9 | 3469 | `				break;` |
|      - | 3470 | `			}` |
|     81 | 3471 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 3472 | `			if( rc == SXRET_OK ){` |
|      - | 3473 | `				/* Pattern found,return it's position */` |
|     11 | 3474 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 3475 | `				return PH7_OK;` |
|      - | 3476 | `			}` |
|     71 | 3477 | `			zPtr--;` |
|      1 | 3478 | `		}` |
|      - | 3479 | `		/* Pattern not found,return FALSE */` |
|      9 | 3480 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3481 | `	}else{` |
|      3 | 3482 | `		ph7_result_bool(pCtx,0);` |
|      - | 3483 | `	}` |
|     11 | 3484 | `	return PH7_OK;` |
|     15 | 3485 |  |
|      - | 3486 | `/*` |
|      - | 3487 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3488 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3489 | ` * Parameters` |
|      - | 3490 | ` *  $haystack` |
|      - | 3491 | ` *   The input string.` |
|      - | 3492 | ` * $needle` |
|      - | 3493 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3494 | ` *  This behavior is different from that of strstr().` |
|      - | 3495 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3496 | ` *  as the ordinal value of a character.` |
|      - | 3497 | ` * Return` |
|      - | 3498 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3499 | ` */` |
|     24 | 3500 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3501 |  |
|      - | 3502 | `	const char *zBlob;` |
|      - | 3503 | `	int nLen,c;` |
|     25 | 3504 | `	if( nArg < 2 ){` |
|      - | 3505 | `		/* Missing arguments,return FALSE */` |
|      3 | 3506 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3507 | `		return PH7_OK;` |
|      - | 3508 | `	}` |
|      - | 3509 | `	/* Extract the haystack */` |
|     23 | 3510 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3511 | `	c = 0; /* cc warning */` |
|     23 | 3512 | `	if( nLen > 0 ){` |
|      - | 3513 | `		sxu32 nOfft;` |
|      - | 3514 | `		sxi32 rc;` |
|     21 | 3515 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 3516 | `			const char *zPattern;` |
|     11 | 3517 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 3518 | `														 * for NULL pointer.` |
|      - | 3519 | `														 */` |
|     11 | 3520 | `			c = zPattern[0];` |
|      6 | 3521 | `		}else{` |
|      - | 3522 | `			/* Int cast */` |
|     11 | 3523 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 3524 | `		}` |
|      - | 3525 | `		/* Perform the lookup */` |
|     21 | 3526 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3527 | `		if( rc != SXRET_OK ){` |
|      - | 3528 | `			/* No such entry,return FALSE */` |
|      7 | 3529 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3530 | `			return PH7_OK;` |
|      - | 3531 | `		}` |
|      - | 3532 | `		/* Return the string portion */` |
|     15 | 3533 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 3534 | `	}else{` |
|      3 | 3535 | `		ph7_result_bool(pCtx,0);` |
|      - | 3536 | `	}` |
|     17 | 3537 | `	return PH7_OK;` |
|     13 | 3538 |  |
|      - | 3539 | `/*` |
|      - | 3540 | ` * string strrev(string $string)` |
|      - | 3541 | ` *  Reverse a string.` |
|      - | 3542 | ` * Parameters` |
|      - | 3543 | ` *  $string` |
|      - | 3544 | ` *   String to be reversed.` |
|      - | 3545 | ` * Return` |
|      - | 3546 | ` *  The reversed string.` |
|      - | 3547 | ` */` |
|      4 | 3548 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3549 |  |
|      - | 3550 | `	const char *zIn,*zEnd;` |
|      - | 3551 | `	int nLen,c;` |
|      5 | 3552 | `	if( nArg < 1 ){` |
|      - | 3553 | `		/* Missing arguments,return NULL */` |
|      3 | 3554 | `		ph7_result_null(pCtx);` |
|      3 | 3555 | `		return PH7_OK;` |
|      - | 3556 | `	}` |
|      - | 3557 | `	/* Extract the target string */` |
|      3 | 3558 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3559 | `	if( nLen < 1 ){` |
|      - | 3560 | `		/* Empty string Return null */` |
|    ! 0 | 3561 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3562 | `		return PH7_OK;` |
|      - | 3563 | `	}` |
|      - | 3564 | `	/* Perform the requested operation */` |
|      3 | 3565 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3566 | `	for(;;){` |
|      9 | 3567 | `		if( zEnd < zIn ){` |
|      - | 3568 | `			/* No more input to process */` |
|      3 | 3569 | `			break;` |
|      - | 3570 | `		}` |
|      - | 3571 | `		/* Append current character */` |
|      7 | 3572 | `		c = zEnd[0];` |
|      7 | 3573 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3574 | `		zEnd--;` |
|      1 | 3575 | `	}` |
|      3 | 3576 | `	return PH7_OK;` |
|      3 | 3577 |  |
|      - | 3578 | `/*` |
|      - | 3579 | ` * string ucwords(string $string)` |
|      - | 3580 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3581 | ` *  The definition of a word is any string of characters that is immediately after` |
|      - | 3582 | ` *  a whitespace (These are: space, form-feed, newline, carriage return, horizontal tab, and vertical tab).` |
|      - | 3583 | ` * Parameters` |
|      - | 3584 | ` *  $string` |
|      - | 3585 | ` *   The input string.` |
|      - | 3586 | ` * Return` |
|      - | 3587 | ` *  The modified string..` |
|      - | 3588 | ` */` |
|     14 | 3589 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3590 |  |
|      - | 3591 | `	const char *zIn,*zCur,*zEnd;` |
|      - | 3592 | `	int nLen,c;` |
|     15 | 3593 | `	if( nArg < 1 ){` |
|      - | 3594 | `		/* Missing arguments,return NULL */` |
|      3 | 3595 | `		ph7_result_null(pCtx);` |
|      3 | 3596 | `		return PH7_OK;` |
|      - | 3597 | `	}` |
|      - | 3598 | `	/* Extract the target string */` |
|     13 | 3599 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 3600 | `	if( nLen < 1 ){` |
|      - | 3601 | `		/* Empty string Return null */` |
|      3 | 3602 | `		ph7_result_null(pCtx);` |
|      3 | 3603 | `		return PH7_OK;` |
|      - | 3604 | `	}` |
|      - | 3605 | `	/* Perform the requested operation */` |
|     11 | 3606 | `	zEnd = &zIn[nLen];` |
|     21 | 3607 | `	for(;;){` |
|      - | 3608 | `		/* Jump leading white spaces */` |
|     43 | 3609 | `		zCur = zIn;` |
|     65 | 3610 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0x80 && SyisSpace(zIn[0]) ){` |
|     23 | 3611 | `			zIn++;` |
|      1 | 3612 | `		}` |
|     43 | 3613 | `		if( zCur < zIn ){` |
|      - | 3614 | `			/* Append white space stream */` |
|     23 | 3615 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 3616 | `		}` |
|     43 | 3617 | `		if( zIn >= zEnd ){` |
|      - | 3618 | `			/* No more input to process */` |
|     11 | 3619 | `			break;` |
|      - | 3620 | `		}` |
|     33 | 3621 | `		c = zIn[0];` |
|     33 | 3622 | `		if( c < 0x80 && SyisLower(c) ){` |
|     29 | 3623 | `			c = SyToUpper(c);` |
|     14 | 3624 | `		}` |
|      - | 3625 | `		/* Append the upper-cased character */` |
|     33 | 3626 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     33 | 3627 | `		zIn++;` |
|     33 | 3628 | `		zCur = zIn;` |
|      - | 3629 | `		/* Append the word varbatim */` |
|    149 | 3630 | `		while( zIn < zEnd ){` |
|    139 | 3631 | `			if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 3632 | `				/* UTF-8 stream */` |
|    ! 0 | 3633 | `				zIn++;` |
|    ! 0 | 3634 | `				SX_JMP_UTF8(zIn,zEnd);` |
|    138 | 3635 | `			}else if( !SyisSpace(zIn[0]) ){` |
|    117 | 3636 | `				zIn++;` |
|     59 | 3637 | `			}else{` |
|     23 | 3638 | `				break;` |
|      - | 3639 | `			}` |
|      1 | 3640 | `		}` |
|     33 | 3641 | `		if( zCur < zIn ){` |
|     33 | 3642 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 | 3643 | `		}` |
|      1 | 3644 | `	}` |
|     11 | 3645 | `	return PH7_OK;` |
|      8 | 3646 |  |
|      - | 3647 | `/*` |
|      - | 3648 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3649 | ` *  Returns input repeated multiplier times.` |
|      - | 3650 | ` * Parameters` |
|      - | 3651 | ` *  $string` |
|      - | 3652 | ` *   String to be repeated.` |
|      - | 3653 | ` * $multiplier` |
|      - | 3654 | ` *  Number of time the input string should be repeated.` |
|      - | 3655 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3656 | ` *  to 0, the function will return an empty string.` |
|      - | 3657 | ` * Return` |
|      - | 3658 | ` *  The repeated string.` |
|      - | 3659 | ` */` |
|  20212 | 3660 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3661 |  |
|      - | 3662 | `	const char *zIn;` |
|      - | 3663 | `	int nLen,nMul;` |
|      - | 3664 | `	int rc;` |
|  20213 | 3665 | `	if( nArg < 2 ){` |
|      - | 3666 | `		/* Missing arguments,return NULL */` |
|      3 | 3667 | `		ph7_result_null(pCtx);` |
|      3 | 3668 | `		return PH7_OK;` |
|      - | 3669 | `	}` |
|      - | 3670 | `	/* Extract the target string */` |
|  20211 | 3671 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|  20211 | 3672 | `	if( nLen < 1 ){` |
|      - | 3673 | `		/* Empty string.Return null */` |
|    ! 0 | 3674 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3675 | `		return PH7_OK;` |
|      - | 3676 | `	}` |
|      - | 3677 | `	/* Extract the multiplier */` |
|  20211 | 3678 | `	nMul = ph7_value_to_int(apArg[1]);` |
|  20211 | 3679 | `	if( nMul < 1 ){` |
|      - | 3680 | `		/* Return the empty string */` |
|      3 | 3681 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3682 | `		return PH7_OK;` |
|      - | 3683 | `	}` |
|      - | 3684 | `	/* Perform the requested operation */` |
| 120220 | 3685 | `	for(;;){` |
| 240441 | 3686 | `		if( !nMul ){` |
|  20209 | 3687 | `			break;` |
|      - | 3688 | `		}` |
|      - | 3689 | `		/* Append the copy */` |
| 220233 | 3690 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 220233 | 3691 | `		if( rc != PH7_OK ){` |
|      - | 3692 | `			/* Out of memory,break immediately */` |
|    ! 0 | 3693 | `			break;` |
|      - | 3694 | `		}` |
| 220233 | 3695 | `		nMul--;` |
|      1 | 3696 | `	}` |
|  20209 | 3697 | `	return PH7_OK;` |
|  10107 | 3698 |  |
|      - | 3699 | `/*` |
|      - | 3700 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3701 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3702 | ` * Parameters` |
|      - | 3703 | ` *  $string` |
|      - | 3704 | ` *   The input string.` |
|      - | 3705 | ` * $is_xhtml` |
|      - | 3706 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3707 | ` * Return` |
|      - | 3708 | ` *  The processed string.` |
|      - | 3709 | ` */` |
|      6 | 3710 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3711 |  |
|      - | 3712 | `	const char *zIn,*zCur,*zEnd;` |
|      7 | 3713 | `	int is_xhtml = 1; /* Default to XHTML-style '<br/>' like PHP */` |
|      - | 3714 | `	int nLen;` |
|      7 | 3715 | `	if( nArg < 1 ){` |
|      - | 3716 | `		/* Missing arguments,return the empty string */` |
|      3 | 3717 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3718 | `		return PH7_OK;` |
|      - | 3719 | `	}` |
|      - | 3720 | `	/* Extract the target string */` |
|      5 | 3721 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3722 | `	if( nLen < 1 ){` |
|      - | 3723 | `		/* Empty string,return null */` |
|    ! 0 | 3724 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3725 | `		return PH7_OK;` |
|      - | 3726 | `	}` |
|      5 | 3727 | `	if( nArg > 1 ){` |
|      3 | 3728 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3729 | `	}` |
|      5 | 3730 | `	zEnd = &zIn[nLen];` |
|      - | 3731 | `	/* Perform the requested operation */` |
|      4 | 3732 | `	for(;;){` |
|      9 | 3733 | `		zCur = zIn;` |
|      - | 3734 | `		/* Delimit the string */` |
|     21 | 3735 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3736 | `			zIn++;` |
|      1 | 3737 | `		}` |
|      9 | 3738 | `		if( zCur < zIn ){` |
|      - | 3739 | `			/* Output chunk verbatim */` |
|      9 | 3740 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3741 | `		}` |
|      9 | 3742 | `		if( zIn >= zEnd ){` |
|      - | 3743 | `			/* No more input to process */` |
|      5 | 3744 | `			break;` |
|      - | 3745 | `		}` |
|      - | 3746 | `		/* Output the HTML line break */` |
|      - | 3747 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br/>' (legacy without space), otherwise use '<br>' */` |
|      5 | 3748 | `		if( is_xhtml ){` |
|      3 | 3749 | `			ph7_result_string(pCtx,"<br/>",(int)sizeof("<br/>")-1);` |
|      2 | 3750 | `		}else{` |
|      3 | 3751 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3752 | `		}` |
|      5 | 3753 | `		zCur = zIn;` |
|      - | 3754 | `		/* Append trailing line */` |
|     11 | 3755 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3756 | `			zIn++;` |
|      1 | 3757 | `		}` |
|      5 | 3758 | `		if( zCur < zIn ){` |
|      - | 3759 | `			/* Output chunk verbatim */` |
|      5 | 3760 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 3761 | `		}` |
|      1 | 3762 | `	}` |
|      5 | 3763 | `	return PH7_OK;` |
|      4 | 3764 |  |
|      - | 3765 | `/*` |
|      - | 3766 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3767 | ` *  According to the PHP reference manual.` |
|      - | 3768 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3769 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3770 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 3771 | ` * This applies to both sprintf() and printf().` |
|      - | 3772 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 3773 | ` * or more of these elements, in order:` |
|      - | 3774 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 3775 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 3776 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 3777 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 3778 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 3779 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 3780 | ` *   it with a single quote ('). See the examples below.` |
|      - | 3781 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 3782 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 3783 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 3784 | ` *   should result in.` |
|      - | 3785 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 3786 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 3787 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 3788 | ` *   limit to the string.` |
|      - | 3789 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 3790 | ` *       % - a literal percent character. No argument is required.` |
|      - | 3791 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 3792 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 3793 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 3794 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 3795 | ` * 	     for the number of digits after the decimal point.` |
|      - | 3796 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 3797 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 3798 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 3799 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 3800 | ` *       g - shorter of %e and %f.` |
|      - | 3801 | ` *       G - shorter of %E and %f.` |
|      - | 3802 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 3803 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 3804 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 3805 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 3806 | ` */` |
|      - | 3807 | `/*` |
|      - | 3808 | ` * This implementation is based on the one found in the SQLite3 source tree.` |
|      - | 3809 | ` */` |
|      - | 3810 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 3811 | `/*` |
|      - | 3812 | `** Conversion types fall into various categories as defined by the` |
|      - | 3813 | `** following enumeration.` |
|      - | 3814 | `*/` |
|      - | 3815 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 3816 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 3817 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 3818 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 3819 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 3820 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 3821 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 3822 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 3823 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 3824 | `/*` |
|      - | 3825 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 3826 | `*/` |
|      - | 3827 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 3828 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 3829 | `/*` |
|      - | 3830 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 3831 | `** by an instance of the following structure` |
|      - | 3832 | `*/` |
|      - | 3833 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 3834 | `struct ph7_fmt_info` |
|      - | 3835 |  |
|      - | 3836 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 3837 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 3838 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 3839 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 3840 | `  char *charset; /* The character set for conversion */` |
|      - | 3841 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 3842 | `};` |
|      - | 3843 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3844 | `/*` |
|      - | 3845 | `** "*val" is a double such that 0.1 <= *val < 10.0` |
|      - | 3846 | `** Return the ascii code for the leading digit of *val, then` |
|      - | 3847 | `** multiply "*val" by 10.0 to renormalize.` |
|      - | 3848 | `**` |
|      - | 3849 | `** Example:` |
|      - | 3850 | `**     input:     *val = 3.14159` |
|      - | 3851 | `**     output:    *val = 1.4159    function return = '3'` |
|      - | 3852 | `**` |
|      - | 3853 | `** The counter *cnt is incremented each time.  After counter exceeds` |
|      - | 3854 | `** 16 (the number of significant digits in a 64-bit float) '0' is` |
|      - | 3855 | `** always returned.` |
|      - | 3856 | `*/` |
|    404 | 3857 | `static int vxGetdigit(sxlongreal *val,int *cnt)` |
|      1 | 3858 |  |
|      - | 3859 | `  sxlongreal d;` |
|      - | 3860 | `  int digit;` |
|      - | 3861 |  |
|    405 | 3862 | `  if( (*cnt)++ >= 16 ){` |
|    ! 0 | 3863 | `	  return '0';` |
|      - | 3864 | `  }` |
|    405 | 3865 | `  digit = (int)*val;` |
|    405 | 3866 | `  d = digit;` |
|    405 | 3867 | `   *val = (*val - d)*10.0;` |
|    405 | 3868 | `  return digit + '0' ;` |
|    203 | 3869 |  |
|      - | 3870 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      - | 3871 | `/*` |
|      - | 3872 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 3873 | ` * used conversion types first.` |
|      - | 3874 | ` */` |
|      - | 3875 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 3876 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 3877 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 3878 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 3879 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 3880 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 3881 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 3882 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 3883 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 3884 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3885 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3886 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 3887 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 3888 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 3889 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 3890 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 3891 | `};` |
|      - | 3892 | `/*` |
|      - | 3893 | ` * Format a given string.` |
|      - | 3894 | ` * The root program.  All variations call this core.` |
|      - | 3895 | ` * INPUTS:` |
|      - | 3896 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 3897 | ` *            1. A pointer to the call context.` |
|      - | 3898 | ` *            2. A pointer to the list of characters to be output` |
|      - | 3899 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 3900 | ` *            3. An integer number of characters to be output.` |
|      - | 3901 | ` *               (Note: This number might be zero.)` |
|      - | 3902 | ` *            4. Upper layer private data.` |
|      - | 3903 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 3904 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 3905 | ` */` |
|    120 | 3906 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 3907 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 3908 | `	ph7_context *pCtx,  /* call context */` |
|      - | 3909 | `	const char *zIn,    /* Format string */` |
|      - | 3910 | `	int nByte,          /* Format string length */` |
|      - | 3911 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 3912 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 3913 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 3914 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 3915 | `	)` |
|      1 | 3916 |  |
|    121 | 3917 | `	char spaces[] = "                                                  ";` |
|      - | 3918 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    121 | 3919 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 3920 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 3921 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 3922 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 3923 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 3924 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 3925 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 3926 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 3927 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 3928 | `	ph7_int64 iVal;` |
|      - | 3929 | `	int precision;           /* Precision of the current field */` |
|      - | 3930 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 3931 | `	int c,rc,n;` |
|      - | 3932 | `	int length;              /* Length of the field */` |
|      - | 3933 | `	int prefix;` |
|      - | 3934 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 3935 | `	int width;               /* Width of the current field */` |
|      - | 3936 | `	int idx;` |
|    121 | 3937 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 3938 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 3939 | `	/* Start the format process */` |
|    123 | 3940 | `	for(;;){` |
|    247 | 3941 | `		zCur = zIn;` |
|    697 | 3942 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|    451 | 3943 | `			zIn++;` |
|      1 | 3944 | `		}` |
|    247 | 3945 | `		if( zCur < zIn ){` |
|      - | 3946 | `			/* Consume chunk verbatim */` |
|     95 | 3947 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|     95 | 3948 | `			if( rc == SXERR_ABORT ){` |
|      - | 3949 | `				/* Callback request an operation abort */` |
|    ! 0 | 3950 | `				break;` |
|      - | 3951 | `			}` |
|     47 | 3952 | `		}` |
|    247 | 3953 | `		if( zIn >= zEnd ){` |
|      - | 3954 | `			/* No more input to process,break immediately */` |
|    119 | 3955 | `			break;` |
|      - | 3956 | `		}` |
|      - | 3957 | `		/* Find out what flags are present */` |
|    129 | 3958 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    128 | 3959 | `			flag_alternateform = flag_zeropad = 0;` |
|    129 | 3960 | `		zIn++; /* Jump the precent sign */` |
|     64 | 3961 | `		do{` |
|    157 | 3962 | `			c = zIn[0];` |
|    157 | 3963 | `			switch( c ){` |
|      9 | 3964 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      3 | 3965 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 3966 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|      9 | 3967 | `			case '#':   flag_alternateform = 1;   c = 0;   break;` |
|      5 | 3968 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 3969 | `			case '\'':` |
|    ! 0 | 3970 | `				zIn++;` |
|    ! 0 | 3971 | `				if( zIn < zEnd ){` |
|      - | 3972 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 3973 | `					c = zIn[0];` |
|    ! 0 | 3974 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3975 | `						spaces[idx] = (char)c;` |
|    ! 0 | 3976 | `					}` |
|    ! 0 | 3977 | `					c = 0;` |
|    ! 0 | 3978 | `				}` |
|    ! 0 | 3979 | `				break;` |
|    128 | 3980 | `			default:                                       break;` |
|      - | 3981 | `			}` |
|    157 | 3982 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 3983 | `		/* Get the field width */` |
|    129 | 3984 | `		width = 0;` |
|    223 | 3985 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     31 | 3986 | `			width = width*10 + (zIn[0] - '0');` |
|     31 | 3987 | `			zIn++;` |
|      1 | 3988 | `		}` |
|    129 | 3989 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 3990 | `			/* Position specifer */` |
|    ! 0 | 3991 | `			if( width > 0 ){` |
|    ! 0 | 3992 | `				n = width;` |
|    ! 0 | 3993 | `				if( vf && n > 0 ){` |
|    ! 0 | 3994 | `					n--;` |
|    ! 0 | 3995 | `				}` |
|    ! 0 | 3996 | `			}` |
|    ! 0 | 3997 | `			zIn++;` |
|    ! 0 | 3998 | `			width = 0;` |
|    ! 0 | 3999 | `			if( zIn < zEnd && zIn[0] == '0' ){` |
|    ! 0 | 4000 | `				flag_zeropad = 1;` |
|    ! 0 | 4001 | `				zIn++;` |
|    ! 0 | 4002 | `			}` |
|    ! 0 | 4003 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 4004 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 4005 | `				zIn++;` |
|    ! 0 | 4006 | `			}` |
|    ! 0 | 4007 | `		}` |
|    129 | 4008 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 4009 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 4010 | `		}` |
|      - | 4011 | `		/* Get the precision */` |
|    129 | 4012 | `		precision = -1;` |
|    129 | 4013 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     57 | 4014 | `			precision = 0;` |
|     57 | 4015 | `			zIn++;` |
|    145 | 4016 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     61 | 4017 | `				precision = precision*10 + (zIn[0] - '0');` |
|     61 | 4018 | `				zIn++;` |
|      1 | 4019 | `			}` |
|     28 | 4020 | `		}` |
|    129 | 4021 | `		if( zIn >= zEnd ){` |
|      - | 4022 | `			/* No more input */` |
|      3 | 4023 | `			break;` |
|      - | 4024 | `		}` |
|      - | 4025 | `		/* Fetch the info entry for the field */` |
|    127 | 4026 | `		pInfo = 0;` |
|    127 | 4027 | `		xtype = PH7_FMT_ERROR;` |
|    127 | 4028 | `		c = zIn[0];` |
|    127 | 4029 | `		zIn++; /* Jump the format specifer */` |
|    699 | 4030 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|    697 | 4031 | `			if( c==aFmt[idx].fmttype ){` |
|    125 | 4032 | `				pInfo = &aFmt[idx];` |
|    125 | 4033 | `				xtype = pInfo->type;` |
|    125 | 4034 | `				break;` |
|      - | 4035 | `			}` |
|    287 | 4036 | `		}` |
|    127 | 4037 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    127 | 4038 | `		length = 0;` |
|      - | 4039 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 4040 | `		 /*` |
|      - | 4041 | `		  ** At this point, variables are initialized as follows:` |
|      - | 4042 | `		  **` |
|      - | 4043 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 4044 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 4045 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 4046 | `		  **                               field width was negative.` |
|      - | 4047 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 4048 | `		  **                               the conversion character.` |
|      - | 4049 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 4050 | `		  **   width                       The specified field width.  This is` |
|      - | 4051 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 4052 | `		  **   precision                   The specified precision.  The default` |
|      - | 4053 | `		  **                               is -1.` |
|      - | 4054 | `		  */` |
|    127 | 4055 | `		switch(xtype){` |
|    ! 0 | 4056 | `		case PH7_FMT_PERCENT:` |
|      - | 4057 | `			/* A literal percent character */` |
|    ! 0 | 4058 | `			zWorker[0] = '%';` |
|    ! 0 | 4059 | `			length = (int)sizeof(char);` |
|    ! 0 | 4060 | `			break;` |
|      3 | 4061 | `		case PH7_FMT_CHARX:` |
|      - | 4062 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 4063 | `			 * with that ASCII value` |
|      - | 4064 | `			 */` |
|      7 | 4065 | `			pArg = NEXT_ARG;` |
|      7 | 4066 | `			if( pArg == 0 ){` |
|      3 | 4067 | `				c = 0;` |
|      2 | 4068 | `			}else{` |
|      5 | 4069 | `				c = ph7_value_to_int(pArg);` |
|      - | 4070 | `			}` |
|      - | 4071 | `			/* NUL byte is an acceptable value */` |
|      7 | 4072 | `			zWorker[0] = (char)c;` |
|      7 | 4073 | `			length = (int)sizeof(char);` |
|      7 | 4074 | `			break;` |
|     12 | 4075 | `		case PH7_FMT_STRING:` |
|      - | 4076 | `			/* the argument is treated as and presented as a string */` |
|     25 | 4077 | `			pArg = NEXT_ARG;` |
|     25 | 4078 | `			if( pArg == 0 ){` |
|    ! 0 | 4079 | `				length = 0;` |
|    ! 0 | 4080 | `			}else{` |
|     25 | 4081 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 4082 | `			}` |
|     25 | 4083 | `			if( length < 1 ){` |
|    ! 0 | 4084 | `				zBuf = " ";` |
|    ! 0 | 4085 | `				length = (int)sizeof(char);` |
|    ! 0 | 4086 | `			}` |
|     25 | 4087 | `			if( precision>=0 && precision<length ){` |
|      3 | 4088 | `				length = precision;` |
|      1 | 4089 | `			}` |
|     25 | 4090 | `			if( flag_zeropad ){` |
|      - | 4091 | `				/* zero-padding works on strings too */` |
|    ! 0 | 4092 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4093 | `					spaces[idx] = '0';` |
|    ! 0 | 4094 | `				}` |
|    ! 0 | 4095 | `			}` |
|     25 | 4096 | `			break;` |
|     20 | 4097 | `		case PH7_FMT_RADIX:` |
|     41 | 4098 | `			pArg = NEXT_ARG;` |
|     41 | 4099 | `			if( pArg == 0 ){` |
|    ! 0 | 4100 | `				iVal = 0;` |
|    ! 0 | 4101 | `			}else{` |
|     41 | 4102 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 4103 | `			}` |
|      - | 4104 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|     41 | 4105 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 4106 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 4107 | `			}` |
|      - | 4108 | `#if 1` |
|      - | 4109 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 4110 | `        ** I think this is stupid.*/` |
|     41 | 4111 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 4112 | `#else` |
|      - | 4113 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 4114 | `        ** but leave the prefix for hex.*/` |
|      - | 4115 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 4116 | `#endif` |
|     41 | 4117 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|     23 | 4118 | `          if( iVal<0 ){` |
|      3 | 4119 | `            iVal = -iVal;` |
|      - | 4120 | `			/* Ticket 1433-003 */` |
|      3 | 4121 | `			if( iVal < 0 ){` |
|      - | 4122 | `				/* Overflow */` |
|    ! 0 | 4123 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4124 | `			}` |
|      3 | 4125 | `            prefix = '-';` |
|     22 | 4126 | `          }else if( flag_plussign )  prefix = '+';` |
|     19 | 4127 | `          else if( flag_blanksign )  prefix = ' ';` |
|     17 | 4128 | `          else                       prefix = 0;` |
|     12 | 4129 | `        }else{` |
|     19 | 4130 | `			if( iVal<0 ){` |
|    ! 0 | 4131 | `				iVal = -iVal;` |
|      - | 4132 | `				/* Ticket 1433-003 */` |
|    ! 0 | 4133 | `				if( iVal < 0 ){` |
|      - | 4134 | `					/* Overflow */` |
|    ! 0 | 4135 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4136 | `				}` |
|    ! 0 | 4137 | `			}` |
|     19 | 4138 | `			prefix = 0;` |
|      - | 4139 | `		}` |
|     41 | 4140 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      3 | 4141 | `          precision = width-(prefix!=0);` |
|      1 | 4142 | `        }` |
|     41 | 4143 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 4144 | `        {` |
|      - | 4145 | `          register char *cset;      /* Use registers for speed */` |
|      - | 4146 | `          register int base;` |
|     41 | 4147 | `          cset = pInfo->charset;` |
|     41 | 4148 | `          base = pInfo->base;` |
|     20 | 4149 | `          do{                                           /* Convert to ascii */` |
|     79 | 4150 | `            *(--zBuf) = cset[iVal%base];` |
|     79 | 4151 | `            iVal = iVal/base;` |
|     79 | 4152 | `          }while( iVal>0 );` |
|      - | 4153 | `        }` |
|     41 | 4154 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     55 | 4155 | `        for(idx=precision-length; idx>0; idx--){` |
|     15 | 4156 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|      8 | 4157 | `        }` |
|     41 | 4158 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|     41 | 4159 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 4160 | `          char *pre, x;` |
|      9 | 4161 | `          pre = pInfo->prefix;` |
|      9 | 4162 | `          if( *zBuf!=pre[0] ){` |
|     23 | 4163 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|      4 | 4164 | `          }` |
|      4 | 4165 | `        }` |
|     41 | 4166 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     41 | 4167 | `		break;` |
|     27 | 4168 | `		case PH7_FMT_FLOAT:` |
|      - | 4169 | `		case PH7_FMT_EXP:` |
|      - | 4170 | `		case PH7_FMT_GENERIC:{` |
|      - | 4171 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 4172 | `		long double realvalue;` |
|      - | 4173 | `		int  exp;                /* exponent of real numbers */` |
|      - | 4174 | `		double rounder;          /* Used for rounding floating point values */` |
|      - | 4175 | `		int flag_dp;            /* True if decimal point should be shown */` |
|      - | 4176 | `		int flag_rtz;           /* True if trailing zeros should be removed */` |
|      - | 4177 | `		int flag_exp;           /* True to force display of the exponent */` |
|      - | 4178 | `		int nsd;                 /* Number of significant digits returned */` |
|     55 | 4179 | `		pArg = NEXT_ARG;` |
|     55 | 4180 | `		if( pArg == 0 ){` |
|    ! 0 | 4181 | `			realvalue = 0;` |
|    ! 0 | 4182 | `		}else{` |
|     55 | 4183 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 4184 | `		}` |
|     55 | 4185 | `        if( precision<0 ) precision = 6;         /* Set default precision */` |
|     55 | 4186 | `        if( precision>PH7_FMT_BUFSIZ-40) precision = PH7_FMT_BUFSIZ-40;` |
|     55 | 4187 | `        if( realvalue<0.0 ){` |
|    ! 0 | 4188 | `          realvalue = -realvalue;` |
|    ! 0 | 4189 | `          prefix = '-';` |
|    ! 0 | 4190 | `        }else{` |
|     55 | 4191 | `          if( flag_plussign )          prefix = '+';` |
|     55 | 4192 | `          else if( flag_blanksign )    prefix = ' ';` |
|     55 | 4193 | `          else                         prefix = 0;` |
|      - | 4194 | `        }` |
|     55 | 4195 | `        if( pInfo->type==PH7_FMT_GENERIC && precision>0 ) precision--;` |
|     55 | 4196 | `        rounder = 0.0;` |
|      - | 4197 | `#if 0` |
|      - | 4198 | `        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */` |
|      - | 4199 | `        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);` |
|      - | 4200 | `#else` |
|      - | 4201 | `        /* It makes more sense to use 0.5 */` |
|    387 | 4202 | `        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);` |
|      - | 4203 | `#endif` |
|     55 | 4204 | `        if( pInfo->type==PH7_FMT_FLOAT ) realvalue += rounder;` |
|      - | 4205 | `        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */` |
|     55 | 4206 | `        exp = 0;` |
|     55 | 4207 | `        if( realvalue>0.0 ){` |
|     59 | 4208 | `          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }` |
|     87 | 4209 | `          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }` |
|     59 | 4210 | `          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }` |
|     69 | 4211 | `          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }` |
|     55 | 4212 | `          if( exp>350 \|\| exp<-350 ){` |
|    ! 0 | 4213 | `            zBuf = "NaN";` |
|    ! 0 | 4214 | `            length = 3;` |
|    ! 0 | 4215 | `            break;` |
|      - | 4216 | `          }` |
|     27 | 4217 | `        }` |
|     55 | 4218 | `        zBuf = zWorker;` |
|      - | 4219 | `        /*` |
|      - | 4220 | `        ** If the field type is etGENERIC, then convert to either etEXP` |
|      - | 4221 | `        ** or etFLOAT, as appropriate.` |
|      - | 4222 | `        */` |
|     55 | 4223 | `        flag_exp = xtype==PH7_FMT_EXP;` |
|     55 | 4224 | `        if( xtype!=PH7_FMT_FLOAT ){` |
|    ! 0 | 4225 | `          realvalue += rounder;` |
|    ! 0 | 4226 | `          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }` |
|    ! 0 | 4227 | `        }` |
|     55 | 4228 | `        if( xtype==PH7_FMT_GENERIC ){` |
|    ! 0 | 4229 | `          flag_rtz = !flag_alternateform;` |
|    ! 0 | 4230 | `          if( exp<-4 \|\| exp>precision ){` |
|    ! 0 | 4231 | `            xtype = PH7_FMT_EXP;` |
|    ! 0 | 4232 | `          }else{` |
|    ! 0 | 4233 | `            precision = precision - exp;` |
|    ! 0 | 4234 | `            xtype = PH7_FMT_FLOAT;` |
|      - | 4235 | `          }` |
|    ! 0 | 4236 | `        }else{` |
|     55 | 4237 | `          flag_rtz = 0;` |
|      - | 4238 | `        }` |
|      - | 4239 | `        /*` |
|      - | 4240 | `        ** The "exp+precision" test causes output to be of type etEXP if` |
|      - | 4241 | `        ** the precision is too large to fit in buf[].` |
|      - | 4242 | `        */` |
|     55 | 4243 | `        nsd = 0;` |
|     55 | 4244 | `        if( xtype==PH7_FMT_FLOAT && exp+precision<PH7_FMT_BUFSIZ-30 ){` |
|     55 | 4245 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|     55 | 4246 | `          if( prefix ) *(zBuf++) = (char)prefix;         /* Sign */` |
|     55 | 4247 | `          if( exp<0 )  *(zBuf++) = '0';            /* Digits before "." */` |
|    145 | 4248 | `          else for(; exp>=0; exp--) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     55 | 4249 | `          if( flag_dp ) *(zBuf++) = '.';           /* The decimal point */` |
|     87 | 4250 | `          for(exp++; exp<0 && precision>0; precision--, exp++){` |
|     33 | 4251 | `            *(zBuf++) = '0';` |
|     17 | 4252 | `          }` |
|    355 | 4253 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     55 | 4254 | `          *(zBuf--) = 0;                           /* Null terminate */` |
|     55 | 4255 | `          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */` |
|    ! 0 | 4256 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 4257 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 4258 | `          }` |
|     55 | 4259 | `          zBuf++;                            /* point to next free slot */` |
|     28 | 4260 | `        }else{    /* etEXP or etGENERIC */` |
|    ! 0 | 4261 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|    ! 0 | 4262 | `          if( prefix ) *(zBuf++) = (char)prefix;   /* Sign */` |
|    ! 0 | 4263 | `          *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);  /* First digit */` |
|    ! 0 | 4264 | `          if( flag_dp ) *(zBuf++) = '.';     /* Decimal point */` |
|    ! 0 | 4265 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|    ! 0 | 4266 | `          zBuf--;                            /* point to last digit */` |
|    ! 0 | 4267 | `          if( flag_rtz && flag_dp ){          /* Remove tail zeros */` |
|    ! 0 | 4268 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 4269 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 4270 | `          }` |
|    ! 0 | 4271 | `          zBuf++;                            /* point to next free slot */` |
|    ! 0 | 4272 | `          if( exp \|\| flag_exp ){` |
|    ! 0 | 4273 | `            *(zBuf++) = pInfo->charset[0];` |
|    ! 0 | 4274 | `            if( exp<0 ){ *(zBuf++) = '-'; exp = -exp; } /* sign of exp */` |
|    ! 0 | 4275 | `            else       { *(zBuf++) = '+'; }` |
|    ! 0 | 4276 | `            if( exp>=100 ){` |
|    ! 0 | 4277 | `              *(zBuf++) = (char)((exp/100)+'0');                /* 100's digit */` |
|    ! 0 | 4278 | `              exp %= 100;` |
|    ! 0 | 4279 | `            }` |
|    ! 0 | 4280 | `            *(zBuf++) = (char)(exp/10+'0');                     /* 10's digit */` |
|    ! 0 | 4281 | `            *(zBuf++) = (char)(exp%10+'0');                     /* 1's digit */` |
|    ! 0 | 4282 | `          }` |
|      - | 4283 | `        }` |
|      - | 4284 | `        /* The converted number is in buf[] and zero terminated.Output it.` |
|      - | 4285 | `        ** Note that the number is in the usual order, not reversed as with` |
|      - | 4286 | `        ** integer conversions.*/` |
|     55 | 4287 | `        length = (int)(zBuf-zWorker);` |
|     55 | 4288 | `        zBuf = zWorker;` |
|      - | 4289 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 4290 | `        ** set and we are not left justified */` |
|     55 | 4291 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 4292 | `          int i;` |
|      3 | 4293 | `          int nPad = width - length;` |
|     13 | 4294 | `          for(i=width; i>=nPad; i--){` |
|     11 | 4295 | `            zBuf[i] = zBuf[i-nPad];` |
|      6 | 4296 | `          }` |
|      3 | 4297 | `          i = prefix!=0;` |
|      5 | 4298 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      3 | 4299 | `          length = width;` |
|      1 | 4300 | `        }` |
|      - | 4301 | `#else` |
|      - | 4302 | `         zBuf = " ";` |
|      - | 4303 | `		 length = (int)sizeof(char);` |
|      - | 4304 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|     55 | 4305 | `		 break;` |
|      - | 4306 | `							 }` |
|      1 | 4307 | `		default:` |
|      - | 4308 | `			/* Invalid format specifer */` |
|      3 | 4309 | `			zWorker[0] = '?';` |
|      3 | 4310 | `			length = (int)sizeof(char);` |
|      2 | 4311 | `			break;` |
|      - | 4312 | `		}` |
|      - | 4313 | `		 /*` |
|      - | 4314 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 4315 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 4316 | `		 ** the output.` |
|      - | 4317 | `		 */` |
|    127 | 4318 | `    if( !flag_leftjustify ){` |
|      - | 4319 | `      register int nspace;` |
|    119 | 4320 | `      nspace = width-length;` |
|    119 | 4321 | `      if( nspace>0 ){` |
|      5 | 4322 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4323 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4324 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4325 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4326 | `			}` |
|    ! 0 | 4327 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4328 | `        }` |
|      5 | 4329 | `        if( nspace>0 ){` |
|      5 | 4330 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      5 | 4331 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4332 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4333 | `			}` |
|      2 | 4334 | `		}` |
|      2 | 4335 | `      }` |
|     59 | 4336 | `    }` |
|    127 | 4337 | `    if( length>0 ){` |
|    127 | 4338 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    127 | 4339 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4340 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4341 | `		}` |
|     63 | 4342 | `    }` |
|    127 | 4343 | `    if( flag_leftjustify ){` |
|      - | 4344 | `      register int nspace;` |
|      9 | 4345 | `      nspace = width-length;` |
|      9 | 4346 | `      if( nspace>0 ){` |
|      9 | 4347 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4348 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4349 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4350 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4351 | `			}` |
|    ! 0 | 4352 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4353 | `        }` |
|      9 | 4354 | `        if( nspace>0 ){` |
|      9 | 4355 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      9 | 4356 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4357 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4358 | `			}` |
|      4 | 4359 | `		}` |
|      4 | 4360 | `      }` |
|      4 | 4361 | `    }` |
|      1 | 4362 | ` }/* for(;;) */` |
|    121 | 4363 | `	return SXRET_OK;` |
|     61 | 4364 |  |
|      - | 4365 | `/*` |
|      - | 4366 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 4367 | ` */` |
|     84 | 4368 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4369 |  |
|      - | 4370 | `	/* Consume directly */` |
|     85 | 4371 | `	ph7_result_string(pCtx,zInput,nLen);` |
|     42 | 4372 | `	SXUNUSED(pUserData); /* cc warning */` |
|     85 | 4373 | `	return PH7_OK;` |
|      1 | 4374 |  |
|      - | 4375 | `/*` |
|      - | 4376 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 4377 | ` *  Return a formatted string.` |
|      - | 4378 | ` * Parameters` |
|      - | 4379 | ` *  $format` |
|      - | 4380 | ` *    The format string (see block comment above)` |
|      - | 4381 | ` * Return` |
|      - | 4382 | ` *  A string produced according to the formatting string format.` |
|      - | 4383 | ` */` |
|     56 | 4384 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4385 |  |
|      - | 4386 | `	const char *zFormat;` |
|      - | 4387 | `	int nLen;` |
|     57 | 4388 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4389 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 4390 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4391 | `		return PH7_OK;` |
|      - | 4392 | `	}` |
|      - | 4393 | `	/* Extract the string format */` |
|     55 | 4394 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     55 | 4395 | `	if( nLen < 1 ){` |
|      - | 4396 | `		/* Empty string */` |
|    ! 0 | 4397 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4398 | `		return PH7_OK;` |
|      - | 4399 | `	}` |
|      - | 4400 | `	/* Format the string */` |
|     55 | 4401 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,0,FALSE);` |
|     55 | 4402 | `	return PH7_OK;` |
|     29 | 4403 |  |
|      - | 4404 | `/*` |
|      - | 4405 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 4406 | ` */` |
|    110 | 4407 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4408 |  |
|    111 | 4409 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 4410 | `	/* Call the VM output consumer directly */` |
|    111 | 4411 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 4412 | `	/* Increment counter */` |
|    111 | 4413 | `	*pCounter += nLen;` |
|    111 | 4414 | `	return PH7_OK;` |
|      1 | 4415 |  |
|      - | 4416 | `/*` |
|      - | 4417 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 4418 | ` *  Output a formatted string.` |
|      - | 4419 | ` * Parameters` |
|      - | 4420 | ` *  $format` |
|      - | 4421 | ` *   See sprintf() for a description of format.` |
|      - | 4422 | ` * Return` |
|      - | 4423 | ` *  The length of the outputted string.` |
|      - | 4424 | ` */` |
|     42 | 4425 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4426 |  |
|     43 | 4427 | `	ph7_int64 nCounter = 0;` |
|      - | 4428 | `	const char *zFormat;` |
|      - | 4429 | `	int nLen;` |
|     43 | 4430 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4431 | `		/* Missing/Invalid arguments,return 0 */` |
|      3 | 4432 | `		ph7_result_int(pCtx,0);` |
|      3 | 4433 | `		return PH7_OK;` |
|      - | 4434 | `	}` |
|      - | 4435 | `	/* Extract the string format */` |
|     41 | 4436 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 4437 | `	if( nLen < 1 ){` |
|      - | 4438 | `		/* Empty string */` |
|    ! 0 | 4439 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4440 | `		return PH7_OK;` |
|      - | 4441 | `	}` |
|      - | 4442 | `	/* Format the string */` |
|     41 | 4443 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 4444 | `	/* Return the length of the outputted string */` |
|     41 | 4445 | `	ph7_result_int64(pCtx,nCounter);` |
|     41 | 4446 | `	return PH7_OK;` |
|     22 | 4447 |  |
|      - | 4448 | `/*` |
|      - | 4449 | ` * int vprintf(string $format,array $args)` |
|      - | 4450 | ` *  Output a formatted string.` |
|      - | 4451 | ` * Parameters` |
|      - | 4452 | ` *  $format` |
|      - | 4453 | ` *   See sprintf() for a description of format.` |
|      - | 4454 | ` * Return` |
|      - | 4455 | ` *  The length of the outputted string.` |
|      - | 4456 | ` */` |
|      2 | 4457 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4458 |  |
|      3 | 4459 | `	ph7_int64 nCounter = 0;` |
|      - | 4460 | `	const char *zFormat;` |
|      - | 4461 | `	ph7_hashmap *pMap;` |
|      - | 4462 | `	SySet sArg;` |
|      - | 4463 | `	int nLen,n;` |
|      3 | 4464 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4465 | `		/* Missing/Invalid arguments,return 0 */` |
|    ! 0 | 4466 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4467 | `		return PH7_OK;` |
|      - | 4468 | `	}` |
|      - | 4469 | `	/* Extract the string format */` |
|      3 | 4470 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4471 | `	if( nLen < 1 ){` |
|      - | 4472 | `		/* Empty string */` |
|    ! 0 | 4473 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4474 | `		return PH7_OK;` |
|      - | 4475 | `	}` |
|      - | 4476 | `	/* Point to the hashmap */` |
|      3 | 4477 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4478 | `	/* Extract arguments from the hashmap */` |
|      3 | 4479 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4480 | `	/* Format the string */` |
|      3 | 4481 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 4482 | `	/* Return the length of the outputted string */` |
|      3 | 4483 | `	ph7_result_int64(pCtx,nCounter);` |
|      - | 4484 | `	/* Release the container */` |
|      3 | 4485 | `	SySetRelease(&sArg);` |
|      3 | 4486 | `	return PH7_OK;` |
|      2 | 4487 |  |
|      - | 4488 | `/*` |
|      - | 4489 | ` * int vsprintf(string $format,array $args)` |
|      - | 4490 | ` *  Output a formatted string.` |
|      - | 4491 | ` * Parameters` |
|      - | 4492 | ` *  $format` |
|      - | 4493 | ` *   See sprintf() for a description of format.` |
|      - | 4494 | ` * Return` |
|      - | 4495 | ` *  A string produced according to the formatting string format.` |
|      - | 4496 | ` */` |
|     10 | 4497 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4498 |  |
|      - | 4499 | `	const char *zFormat;` |
|      - | 4500 | `	ph7_hashmap *pMap;` |
|      - | 4501 | `	SySet sArg;` |
|      - | 4502 | `	int nLen,n;` |
|     11 | 4503 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4504 | `		/* Missing/Invalid arguments,return the empty string */` |
|      5 | 4505 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 4506 | `		return PH7_OK;` |
|      - | 4507 | `	}` |
|      - | 4508 | `	/* Extract the string format */` |
|      7 | 4509 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4510 | `	if( nLen < 1 ){` |
|      - | 4511 | `		/* Empty string */` |
|    ! 0 | 4512 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4513 | `		return PH7_OK;` |
|      - | 4514 | `	}` |
|      - | 4515 | `	/* Point to hashmap */` |
|      7 | 4516 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4517 | `	/* Extract arguments from the hashmap */` |
|      7 | 4518 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4519 | `	/* Format the string */` |
|      7 | 4520 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),0,TRUE);` |
|      - | 4521 | `	/* Release the container */` |
|      7 | 4522 | `	SySetRelease(&sArg);` |
|      7 | 4523 | `	return PH7_OK;` |
|      6 | 4524 |  |
|      - | 4525 | `/*` |
|      - | 4526 | ` * Symisc eXtension.` |
|      - | 4527 | ` * string size_format(int64 $size)` |
|      - | 4528 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 4529 | ` *  Example:` |
|      - | 4530 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 4531 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 4532 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 4533 | ` * Parameter` |
|      - | 4534 | ` *  $size` |
|      - | 4535 | ` *    Entity size in bytes.` |
|      - | 4536 | ` * Return` |
|      - | 4537 | ` *   Formatted string representation of the given size.` |
|      - | 4538 | ` */` |
|     24 | 4539 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4540 |  |
|      - | 4541 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 4542 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 4543 | `	sxi32 nRest,i_32;` |
|      - | 4544 | `	ph7_int64 iSize;` |
|     25 | 4545 | `	int c = -1; /* index in zUnit[] */` |
|      - | 4546 |  |
|     25 | 4547 | `	if( nArg < 1 ){` |
|      - | 4548 | `		/* Missing argument,return the empty string */` |
|      3 | 4549 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4550 | `		return PH7_OK;` |
|      - | 4551 | `	}` |
|      - | 4552 | `	/* Extract the given size */` |
|     23 | 4553 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 4554 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 4555 | `		/* Don't bother formatting,return immediately */` |
|      5 | 4556 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 4557 | `		return PH7_OK;` |
|      - | 4558 | `	}` |
|     19 | 4559 | `	for(;;){` |
|     39 | 4560 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 4561 | `		iSize >>= 10;` |
|     39 | 4562 | `		c++;` |
|     39 | 4563 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 4564 | `			break;` |
|      - | 4565 | `		}` |
|      1 | 4566 | `	}` |
|     19 | 4567 | `	nRest /= 100;` |
|     19 | 4568 | `	if( nRest > 9 ){` |
|    ! 0 | 4569 | `		nRest = 9;` |
|    ! 0 | 4570 | `	}` |
|     19 | 4571 | `	if( iSize > 999 ){` |
|    ! 0 | 4572 | `		c++;` |
|    ! 0 | 4573 | `		nRest = 9;` |
|    ! 0 | 4574 | `		iSize = 0;` |
|    ! 0 | 4575 | `	}` |
|     19 | 4576 | `	i_32 = (sxi32)iSize;` |
|      - | 4577 | `	/* Format */` |
|     19 | 4578 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 4579 | `	return PH7_OK;` |
|     13 | 4580 |  |
|      - | 4581 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4582 | `/*` |
|      - | 4583 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 4584 | ` *   Calculate the md5 hash of a string.` |
|      - | 4585 | ` * Parameter` |
|      - | 4586 | ` *  $str` |
|      - | 4587 | ` *   Input string` |
|      - | 4588 | ` * $raw_output` |
|      - | 4589 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4590 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4591 | ` * Return` |
|      - | 4592 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 4593 | ` */` |
|     10 | 4594 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4595 |  |
|      - | 4596 | `	unsigned char zDigest[16];` |
|     11 | 4597 | `	int raw_output = FALSE;` |
|      - | 4598 | `	const void *pIn;` |
|      - | 4599 | `	int nLen;` |
|     11 | 4600 | `	if( nArg < 1 ){` |
|      - | 4601 | `		/* Missing arguments,return the empty string */` |
|      3 | 4602 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4603 | `		return PH7_OK;` |
|      - | 4604 | `	}` |
|      - | 4605 | `	/* Extract the input string */` |
|      9 | 4606 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 4607 | `	if( nLen < 1 ){` |
|      - | 4608 | `		/* Empty string */` |
|    ! 0 | 4609 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4610 | `		return PH7_OK;` |
|      - | 4611 | `	}` |
|      9 | 4612 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      3 | 4613 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 | 4614 | `	}` |
|      - | 4615 | `	/* Compute the MD5 digest */` |
|      9 | 4616 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|      9 | 4617 | `	if( raw_output ){` |
|      - | 4618 | `		/* Output raw digest */` |
|      3 | 4619 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      2 | 4620 | `	}else{` |
|      - | 4621 | `		/* Perform a binary to hex conversion */` |
|      7 | 4622 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4623 | `	}` |
|      9 | 4624 | `	return PH7_OK;` |
|      6 | 4625 |  |
|      - | 4626 | `/*` |
|      - | 4627 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 4628 | ` *   Calculate the sha1 hash of a string.` |
|      - | 4629 | ` * Parameter` |
|      - | 4630 | ` *  $str` |
|      - | 4631 | ` *   Input string` |
|      - | 4632 | ` * $raw_output` |
|      - | 4633 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4634 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4635 | ` * Return` |
|      - | 4636 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 4637 | ` */` |
|      8 | 4638 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4639 |  |
|      - | 4640 | `	unsigned char zDigest[20];` |
|      9 | 4641 | `	int raw_output = FALSE;` |
|      - | 4642 | `	const void *pIn;` |
|      - | 4643 | `	int nLen;` |
|      9 | 4644 | `	if( nArg < 1 ){` |
|      - | 4645 | `		/* Missing arguments,return the empty string */` |
|      3 | 4646 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4647 | `		return PH7_OK;` |
|      - | 4648 | `	}` |
|      - | 4649 | `	/* Extract the input string */` |
|      7 | 4650 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4651 | `	if( nLen < 1 ){` |
|      - | 4652 | `		/* Empty string */` |
|    ! 0 | 4653 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4654 | `		return PH7_OK;` |
|      - | 4655 | `	}` |
|      7 | 4656 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      3 | 4657 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 | 4658 | `	}` |
|      - | 4659 | `	/* Compute the SHA1 digest */` |
|      7 | 4660 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|      7 | 4661 | `	if( raw_output ){` |
|      - | 4662 | `		/* Output raw digest */` |
|      3 | 4663 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      2 | 4664 | `	}else{` |
|      - | 4665 | `		/* Perform a binary to hex conversion */` |
|      5 | 4666 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4667 | `	}` |
|      7 | 4668 | `	return PH7_OK;` |
|      5 | 4669 |  |
|      - | 4670 | `/*` |
|      - | 4671 | ` * int64 crc32(string $str)` |
|      - | 4672 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 4673 | ` * Parameter` |
|      - | 4674 | ` *  $str` |
|      - | 4675 | ` *   Input string` |
|      - | 4676 | ` * Return` |
|      - | 4677 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 4678 | ` */` |
|      4 | 4679 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4680 |  |
|      - | 4681 | `	const void *pIn;` |
|      - | 4682 | `	sxu32 nCRC;` |
|      - | 4683 | `	int nLen;` |
|      5 | 4684 | `	if( nArg < 1 ){` |
|      - | 4685 | `		/* Missing arguments,return 0 */` |
|      3 | 4686 | `		ph7_result_int(pCtx,0);` |
|      3 | 4687 | `		return PH7_OK;` |
|      - | 4688 | `	}` |
|      - | 4689 | `	/* Extract the input string */` |
|      3 | 4690 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4691 | `	if( nLen < 1 ){` |
|      - | 4692 | `		/* Empty string */` |
|    ! 0 | 4693 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4694 | `		return PH7_OK;` |
|      - | 4695 | `	}` |
|      - | 4696 | `	/* Calculate the sum */` |
|      3 | 4697 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 4698 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 4699 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 4700 | `	return PH7_OK;` |
|      3 | 4701 |  |
|      - | 4702 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 4703 | `/*` |
|      - | 4704 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 4705 | ` */` |
|      4 | 4706 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 4707 | `	const char *zInput, /* Raw input */` |
|      - | 4708 | `	int nByte,  /* Input length */` |
|      - | 4709 | `	int delim,  /* Delimiter */` |
|      - | 4710 | `	int encl,   /* Enclosure */` |
|      - | 4711 | `	int escape,  /* Escape character */` |
|      - | 4712 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 4713 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 4714 | `	)` |
|      1 | 4715 |  |
|      5 | 4716 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 4717 | `	const char *zIn = zInput;` |
|      - | 4718 | `	const char *zPtr;` |
|      - | 4719 | `	int isEnc;` |
|      - | 4720 | `	/* Start processing */` |
|      8 | 4721 | `	for(;;){` |
|     17 | 4722 | `		if( zIn >= zEnd ){` |
|      - | 4723 | `			/* No more input to process */` |
|      5 | 4724 | `			break;` |
|      - | 4725 | `		}` |
|     13 | 4726 | `		isEnc = 0;` |
|     13 | 4727 | `		zPtr = zIn;` |
|      - | 4728 | `		/* Find the first delimiter */` |
|     27 | 4729 | `		while( zIn < zEnd ){` |
|     23 | 4730 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 4731 | `				/* Delimiter found,break imediately */` |
|      5 | 4732 | `				break;` |
|     15 | 4733 | `			}else if( zIn[0] == encl ){` |
|      - | 4734 | `				/* Inside enclosure? */` |
|    ! 0 | 4735 | `				isEnc = !isEnc;` |
|     15 | 4736 | `			}else if( zIn[0] == escape ){` |
|      - | 4737 | `				/* Escape sequence */` |
|    ! 0 | 4738 | `				zIn++;` |
|    ! 0 | 4739 | `			}` |
|      - | 4740 | `			/* Advance the cursor */` |
|     15 | 4741 | `			zIn++;` |
|      1 | 4742 | `		}` |
|     13 | 4743 | `		if( zIn > zPtr ){` |
|     13 | 4744 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 4745 | `			sxi32 rc;` |
|      - | 4746 | `			/* Invoke the supllied callback */` |
|     13 | 4747 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 4748 | `				zPtr++;` |
|    ! 0 | 4749 | `				nByteChunk-=2;` |
|    ! 0 | 4750 | `			}` |
|     13 | 4751 | `			if( nByteChunk > 0 ){` |
|     13 | 4752 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 4753 | `				if( rc == SXERR_ABORT ){` |
|      - | 4754 | `					/* User callback request an operation abort */` |
|    ! 0 | 4755 | `					break;` |
|      - | 4756 | `				}` |
|      6 | 4757 | `			}` |
|      6 | 4758 | `		}` |
|      - | 4759 | `		/* Ignore trailing delimiter */` |
|     21 | 4760 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 4761 | `			zIn++;` |
|      1 | 4762 | `		}` |
|      1 | 4763 | `	}` |
|      5 | 4764 | `	return SXRET_OK;` |
|      1 | 4765 |  |
|      - | 4766 | `/*` |
|      - | 4767 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 4768 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 4769 | ` * argument to this callback.` |
|      - | 4770 | ` */` |
|     12 | 4771 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 4772 |  |
|     13 | 4773 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 4774 | `	ph7_value sEntry;` |
|      - | 4775 | `	SyString sToken;` |
|      - | 4776 | `	/* Insert the token in the given array */` |
|     13 | 4777 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 4778 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 4779 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 4780 | `	if( sToken.nByte < 1){` |
|    ! 0 | 4781 | `		return SXRET_OK;` |
|      - | 4782 | `	}` |
|     13 | 4783 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 4784 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 4785 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 4786 | `	return SXRET_OK;` |
|      7 | 4787 |  |
|      - | 4788 | `/*` |
|      - | 4789 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 4790 | ` *  Parse a CSV string into an array.` |
|      - | 4791 | ` * Parameters` |
|      - | 4792 | ` *  $input` |
|      - | 4793 | ` *   The string to parse.` |
|      - | 4794 | ` *  $delimiter` |
|      - | 4795 | ` *   Set the field delimiter (one character only).` |
|      - | 4796 | ` *  $enclosure` |
|      - | 4797 | ` *   Set the field enclosure character (one character only).` |
|      - | 4798 | ` *  $escape` |
|      - | 4799 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 4800 | ` * Return` |
|      - | 4801 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 4802 | ` */` |
|      4 | 4803 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4804 |  |
|      - | 4805 | `	const char *zInput,*zPtr;` |
|      - | 4806 | `	ph7_value *pArray;` |
|      5 | 4807 | `	int delim  = ',';   /* Delimiter */` |
|      5 | 4808 | `	int encl   = '"' ;  /* Enclosure */` |
|      5 | 4809 | `	int escape = '\\';  /* Escape character */` |
|      - | 4810 | `	int nLen;` |
|      5 | 4811 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4812 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 4813 | `		ph7_result_null(pCtx);` |
|      3 | 4814 | `		return PH7_OK;` |
|      - | 4815 | `	}` |
|      - | 4816 | `	/* Extract the raw input */` |
|      3 | 4817 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4818 | `	if( nArg > 1 ){` |
|      - | 4819 | `		int i;` |
|      3 | 4820 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 4821 | `			/* Extract the delimiter */` |
|      3 | 4822 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 4823 | `			if( i > 0 ){` |
|      3 | 4824 | `				delim = zPtr[0];` |
|      1 | 4825 | `			}` |
|      1 | 4826 | `		}` |
|      3 | 4827 | `		if( nArg > 2 ){` |
|      3 | 4828 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 4829 | `				/* Extract the enclosure */` |
|      3 | 4830 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 4831 | `				if( i > 0 ){` |
|      3 | 4832 | `					encl = zPtr[0];` |
|      1 | 4833 | `				}` |
|      1 | 4834 | `			}` |
|      3 | 4835 | `			if( nArg > 3 ){` |
|      3 | 4836 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 4837 | `					/* Extract the escape character */` |
|      3 | 4838 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 4839 | `					if( i > 0 ){` |
|      3 | 4840 | `						escape = zPtr[0];` |
|      1 | 4841 | `					}` |
|      1 | 4842 | `				}` |
|      1 | 4843 | `			}` |
|      1 | 4844 | `		}` |
|      1 | 4845 | `	}` |
|      - | 4846 | `	/* Create our array */` |
|      3 | 4847 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4848 | `	if( pArray == 0 ){` |
|    ! 0 | 4849 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4850 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4851 | `		return PH7_OK;` |
|      - | 4852 | `	}` |
|      - | 4853 | `	/* Parse the raw input */` |
|      3 | 4854 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 4855 | `	/* Return the freshly created array */` |
|      3 | 4856 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4857 | `	return PH7_OK;` |
|      3 | 4858 |  |
|      - | 4859 | `/*` |
|      - | 4860 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 4861 | ` * container.` |
|      - | 4862 | ` * Refer to [strip_tags()].` |
|      - | 4863 | ` */` |
|     10 | 4864 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 4865 |  |
|     11 | 4866 | `	const char *zEnd = &zTag[nByte];` |
|      - | 4867 | `	const char *zPtr;` |
|      - | 4868 | `	SyString sEntry;` |
|      - | 4869 | `	/* Strip tags */` |
|     10 | 4870 | `	for(;;){` |
|     45 | 4871 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 4872 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 4873 | `				zTag++;` |
|      1 | 4874 | `		}` |
|     21 | 4875 | `		if( zTag >= zEnd ){` |
|     11 | 4876 | `			break;` |
|      - | 4877 | `		}` |
|     11 | 4878 | `		zPtr = zTag;` |
|      - | 4879 | `		/* Delimit the tag */` |
|     25 | 4880 | `		while(zTag < zEnd ){` |
|     25 | 4881 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 4882 | `				/* UTF-8 stream */` |
|      3 | 4883 | `				zTag++;` |
|      5 | 4884 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 4885 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 4886 | `				break;` |
|    ! 0 | 4887 | `			}else{` |
|     13 | 4888 | `				zTag++;` |
|      - | 4889 | `			}` |
|      1 | 4890 | `		}` |
|     11 | 4891 | `		if( zTag > zPtr ){` |
|      - | 4892 | `			/* Perform the insertion */` |
|     11 | 4893 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 4894 | `			SyStringFullTrim(&sEntry);` |
|     11 | 4895 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 4896 | `		}` |
|      - | 4897 | `		/* Jump the trailing '>' */` |
|     11 | 4898 | `		zTag++;` |
|      1 | 4899 | `	}` |
|     11 | 4900 | `	return SXRET_OK;` |
|      1 | 4901 |  |
|      - | 4902 | `/*` |
|      - | 4903 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 4904 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 4905 | ` * Refer to [strip_tags()].` |
|      - | 4906 | ` */` |
|     36 | 4907 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 4908 |  |
|     37 | 4909 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 4910 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 4911 | `		SyString sTag;` |
|     85 | 4912 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 4913 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 4914 | `			zTag++;` |
|      1 | 4915 | `		}` |
|      - | 4916 | `		/* Delimit the tag */` |
|     25 | 4917 | `		zCur = zTag;` |
|     77 | 4918 | `		while(zTag < zEnd ){` |
|     77 | 4919 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 4920 | `				/* UTF-8 stream */` |
|      5 | 4921 | `				zTag++;` |
|      9 | 4922 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 4923 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 4924 | `				break;` |
|    ! 0 | 4925 | `			}else{` |
|     49 | 4926 | `				zTag++;` |
|      - | 4927 | `			}` |
|      1 | 4928 | `		}` |
|     25 | 4929 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 4930 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 4931 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 4932 | `		if( sTag.nByte > 0 ){` |
|      - | 4933 | `			SyString *aEntry,*pEntry;` |
|      - | 4934 | `			sxi32 rc;` |
|      - | 4935 | `			sxu32 n;` |
|      - | 4936 | `			/* Perform the lookup */` |
|     25 | 4937 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 4938 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 4939 | `				pEntry = &aEntry[n];` |
|      - | 4940 | `				/* Do the comparison */` |
|     25 | 4941 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 4942 | `				if( !rc ){` |
|     21 | 4943 | `					return SXRET_OK;` |
|      - | 4944 | `				}` |
|      3 | 4945 | `			}` |
|      2 | 4946 | `		}` |
|      2 | 4947 | `	}` |
|      - | 4948 | `	/* No such tag */` |
|     17 | 4949 | `	return SXERR_NOTFOUND;` |
|     19 | 4950 |  |
|      - | 4951 | `/*` |
|      - | 4952 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 4953 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 4954 | ` * Refer to [strip_tags()].` |
|      - | 4955 | ` */` |
|     16 | 4956 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 4957 |  |
|     17 | 4958 | `	const char *zEnd = &zIn[nByte];` |
|      - | 4959 | `	const char *zPtr,*zTag;` |
|      - | 4960 | `	SySet sSet;` |
|      - | 4961 | `	/* initialize the set of allowed tags */` |
|     17 | 4962 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 4963 | `	if( nTaglen > 0 ){` |
|      - | 4964 | `		/* Set of allowed tags */` |
|     11 | 4965 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 4966 | `	}` |
|      - | 4967 | `	/* Set the empty string */` |
|     17 | 4968 | `	ph7_result_string(pCtx,"",0);` |
|      - | 4969 | `	/* Start processing */` |
|     26 | 4970 | `	for(;;){` |
|     53 | 4971 | `		if(zIn >= zEnd){` |
|      - | 4972 | `			/* No more input to process */` |
|     15 | 4973 | `			break;` |
|      - | 4974 | `		}` |
|     39 | 4975 | `		zPtr = zIn;` |
|      - | 4976 | `		/* Find a tag */` |
|    133 | 4977 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 4978 | `			zIn++;` |
|      1 | 4979 | `		}` |
|     39 | 4980 | `		if( zIn > zPtr ){` |
|      - | 4981 | `			/* Consume raw input */` |
|     21 | 4982 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 4983 | `		}` |
|      - | 4984 | `		/* Ignore trailing null bytes */` |
|     39 | 4985 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 4986 | `			zIn++;` |
|    ! 0 | 4987 | `		}` |
|     39 | 4988 | `		if(zIn >= zEnd){` |
|      - | 4989 | `			/* No more input to process */` |
|      3 | 4990 | `			break;` |
|      - | 4991 | `		}` |
|     37 | 4992 | `		if( zIn[0] == '<' ){` |
|      - | 4993 | `			sxi32 rc;` |
|     37 | 4994 | `			zTag = zIn++;` |
|      - | 4995 | `			/* Delimit the tag */` |
|    127 | 4996 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 4997 | `				zIn++;` |
|      1 | 4998 | `			}` |
|     37 | 4999 | `			if( zIn < zEnd ){` |
|     37 | 5000 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 5001 | `			}` |
|      - | 5002 | `			/* Query the set */` |
|     37 | 5003 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 5004 | `			if( rc == SXRET_OK ){` |
|      - | 5005 | `				/* Keep the tag */` |
|     21 | 5006 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 5007 | `			}` |
|     18 | 5008 | `		}` |
|      1 | 5009 | `	}` |
|      - | 5010 | `	/* Cleanup */` |
|     17 | 5011 | `	SySetRelease(&sSet);` |
|     17 | 5012 | `	return SXRET_OK;` |
|      1 | 5013 |  |
|      - | 5014 | `/*` |
|      - | 5015 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 5016 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 5017 | ` * Parameters` |
|      - | 5018 | ` *  $str` |
|      - | 5019 | ` *  The input string.` |
|      - | 5020 | ` * $allowable_tags` |
|      - | 5021 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 5022 | ` * Return` |
|      - | 5023 | ` *  Returns the stripped string.` |
|      - | 5024 | ` */` |
|     16 | 5025 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5026 |  |
|     17 | 5027 | `	const char *zTaglist = 0;` |
|      - | 5028 | `	const char *zString;` |
|     17 | 5029 | `	int nTaglen = 0;` |
|      - | 5030 | `	int nLen;` |
|     17 | 5031 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5032 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 5033 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5034 | `		return PH7_OK;` |
|      - | 5035 | `	}` |
|      - | 5036 | `	/* Point to the raw string */` |
|     15 | 5037 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 5038 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 5039 | `		/* Allowed tag */` |
|     11 | 5040 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 5041 | `	}` |
|      - | 5042 | `	/* Process input */` |
|     15 | 5043 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 5044 | `	return PH7_OK;` |
|      9 | 5045 |  |
|      - | 5046 | `/*` |
|      - | 5047 | ` * string str_shuffle(string $str)` |
|      - | 5048 | ` *  Randomly shuffles a string.` |
|      - | 5049 | ` * Parameters` |
|      - | 5050 | ` *  $str` |
|      - | 5051 | ` *   The input string.` |
|      - | 5052 | ` * Return` |
|      - | 5053 | ` *  Returns the shuffled string.` |
|      - | 5054 | ` */` |
|     12 | 5055 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5056 |  |
|      - | 5057 | `	const char *zString;` |
|      - | 5058 | `	int nLen,i,c;` |
|      - | 5059 | `	sxu32 iR;` |
|     13 | 5060 | `	if( nArg < 1 ){` |
|      - | 5061 | `		/* Missing arguments,return the empty string */` |
|      3 | 5062 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5063 | `		return PH7_OK;` |
|      - | 5064 | `	}` |
|      - | 5065 | `	/* Extract the target string */` |
|     11 | 5066 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5067 | `	if( nLen < 1 ){` |
|      - | 5068 | `		/* Nothing to shuffle */` |
|      3 | 5069 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5070 | `		return PH7_OK;` |
|      - | 5071 | `	}` |
|      - | 5072 | `	/* Shuffle the string */` |
|     43 | 5073 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 5074 | `		/* Generate a random number first */` |
|     35 | 5075 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 5076 | `		/* Extract a random offset */` |
|     35 | 5077 | `		c = zString[iR % nLen];` |
|      - | 5078 | `		/* Append it */` |
|     35 | 5079 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 5080 | `	}` |
|      9 | 5081 | `	return PH7_OK;` |
|      7 | 5082 |  |
|      - | 5083 | `/*` |
|      - | 5084 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 5085 | ` *  Convert a string to an array.` |
|      - | 5086 | ` * Parameters` |
|      - | 5087 | ` * $str` |
|      - | 5088 | ` *  The input string.` |
|      - | 5089 | ` * $split_length` |
|      - | 5090 | ` *  Maximum length of the chunk.` |
|      - | 5091 | ` * Return` |
|      - | 5092 | ` *  If the optional split_length parameter is specified, the returned array` |
|      - | 5093 | ` *  will be broken down into chunks with each being split_length in length, otherwise` |
|      - | 5094 | ` *  each chunk will be one character in length. FALSE is returned if split_length is less than 1.` |
|      - | 5095 | ` *  If the split_length length exceeds the length of string, the entire string is returned` |
|      - | 5096 | ` *  as the first (and only) array element.` |
|      - | 5097 | ` */` |
|      8 | 5098 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5099 |  |
|      - | 5100 | `	const char *zString,*zEnd;` |
|      - | 5101 | `	ph7_value *pArray,*pValue;` |
|      - | 5102 | `	int split_len;` |
|      - | 5103 | `	int nLen;` |
|      9 | 5104 | `	if( nArg < 1 ){` |
|      - | 5105 | `		/* Missing arguments,return FALSE */` |
|      5 | 5106 | `		ph7_result_bool(pCtx,0);` |
|      5 | 5107 | `		return PH7_OK;` |
|      - | 5108 | `	}` |
|      - | 5109 | `	/* Point to the target string */` |
|      5 | 5110 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 5111 | `	if( nLen < 1 ){` |
|      - | 5112 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 5113 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5114 | `		return PH7_OK;` |
|      - | 5115 | `	}` |
|      5 | 5116 | `	split_len = (int)sizeof(char);` |
|      5 | 5117 | `	if( nArg > 1 ){` |
|      - | 5118 | `		/* Split length */` |
|      5 | 5119 | `		split_len = ph7_value_to_int(apArg[1]);` |
|      5 | 5120 | `		if( split_len < 1 ){` |
|      - | 5121 | `			/* Invalid length,return FALSE */` |
|      3 | 5122 | `			ph7_result_bool(pCtx,0);` |
|      3 | 5123 | `			return PH7_OK;` |
|      - | 5124 | `		}` |
|      3 | 5125 | `		if( split_len > nLen ){` |
|    ! 0 | 5126 | `			split_len = nLen;` |
|    ! 0 | 5127 | `		}` |
|      1 | 5128 | `	}` |
|      - | 5129 | `	/* Create the array and the scalar value */` |
|      3 | 5130 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 5131 | `	/*Chunk value */` |
|      3 | 5132 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 5133 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 5134 | `		/* Return FALSE */` |
|    ! 0 | 5135 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5136 | `		return PH7_OK;` |
|      - | 5137 | `	}` |
|      - | 5138 | `	/* Point to the end of the string */` |
|      3 | 5139 | `	zEnd = &zString[nLen];` |
|      - | 5140 | `	/* Perform the requested operation */` |
|      7 | 5141 | `	for(;;){` |
|      - | 5142 | `		int nMax;` |
|      9 | 5143 | `		if( zString >= zEnd ){` |
|      - | 5144 | `			/* No more input to process */` |
|      3 | 5145 | `			break;` |
|      - | 5146 | `		}` |
|      7 | 5147 | `		nMax = (int)(zEnd-zString);` |
|      7 | 5148 | `		if( nMax < split_len ){` |
|    ! 0 | 5149 | `			split_len = nMax;` |
|    ! 0 | 5150 | `		}` |
|      - | 5151 | `		/* Copy the current chunk */` |
|      7 | 5152 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 5153 | `		/* Insert it */` |
|      7 | 5154 | `		ph7_array_add_elem(pArray,0,pValue); /* Will make it's own copy */` |
|      - | 5155 | `		/* reset the string cursor */` |
|      7 | 5156 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 5157 | `		/* Update position */` |
|      7 | 5158 | `		zString += split_len;` |
|      1 | 5159 | `	}` |
|      - | 5160 | `	/*` |
|      - | 5161 | `	 * Return the array.` |
|      - | 5162 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 5163 | `	 * upon we return from this function.` |
|      - | 5164 | `	 */` |
|      3 | 5165 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5166 | `	return PH7_OK;` |
|      5 | 5167 |  |
|      - | 5168 | `/*` |
|      - | 5169 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 5170 | ` * Refer to [strspn()].` |
|      - | 5171 | ` */` |
|     28 | 5172 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 5173 |  |
|     29 | 5174 | `	const char *zIn = *pzIn;` |
|      - | 5175 | `	const char *zPtr;` |
|      - | 5176 | `	/* Ignore leading white spaces */` |
|     29 | 5177 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 5178 | `		zIn++;` |
|    ! 0 | 5179 | `	}` |
|     29 | 5180 | `	if( zIn >= zEnd ){` |
|      - | 5181 | `		/* End of input */` |
|    ! 0 | 5182 | `		return SXERR_EOF;` |
|      - | 5183 | `	}` |
|     29 | 5184 | `	zPtr = zIn;` |
|      - | 5185 | `	/* Extract the token */` |
|    201 | 5186 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 5187 | `		zIn++;` |
|      1 | 5188 | `	}` |
|     29 | 5189 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5190 | `	/* Synchronize pointers */` |
|     29 | 5191 | `	*pzIn = zIn;` |
|      - | 5192 | `	/* Return to the caller */` |
|     29 | 5193 | `	return SXRET_OK;` |
|     15 | 5194 |  |
|      - | 5195 | `/*` |
|      - | 5196 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 5197 | ` * return the longest match.` |
|      - | 5198 | ` * Refer to [strspn()].` |
|      - | 5199 | ` */` |
|     18 | 5200 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5201 |  |
|     19 | 5202 | `	const char *zEnd = &zString[nLen];` |
|     19 | 5203 | `	const char *zIn = zString;` |
|      - | 5204 | `	int i,c;` |
|     45 | 5205 | `	for(;;){` |
|     91 | 5206 | `		if( zString >= zEnd ){` |
|      7 | 5207 | `			break;` |
|      - | 5208 | `		}` |
|      - | 5209 | `		/* Extract current character */` |
|     85 | 5210 | `		c = zString[0];` |
|      - | 5211 | `		/* Perform the lookup */` |
|    383 | 5212 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 5213 | `			if( c == zMask[i] ){` |
|      - | 5214 | `				/* Character found */` |
|     73 | 5215 | `				break;` |
|      - | 5216 | `			}` |
|    150 | 5217 | `		}` |
|     85 | 5218 | `		if( i >= nMaskLen ){` |
|      - | 5219 | `			/* Character not in the current mask,break immediately */` |
|     13 | 5220 | `			break;` |
|      - | 5221 | `		}` |
|      - | 5222 | `		/* Advance cursor */` |
|     73 | 5223 | `		zString++;` |
|      1 | 5224 | `	}` |
|      - | 5225 | `	/* Longest match */` |
|     19 | 5226 | `	return (int)(zString-zIn);` |
|      1 | 5227 |  |
|      - | 5228 | `/*` |
|      - | 5229 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 5230 | ` * Refer to [strcspn()].` |
|      - | 5231 | ` */` |
|     10 | 5232 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5233 |  |
|     11 | 5234 | `	const char *zEnd = &zString[nLen];` |
|     11 | 5235 | `	const char *zIn = zString;` |
|      - | 5236 | `	int i,c;` |
|     12 | 5237 | `	for(;;){` |
|     25 | 5238 | `		if( zString >= zEnd ){` |
|      3 | 5239 | `			break;` |
|      - | 5240 | `		}` |
|      - | 5241 | `		/* Extract current character */` |
|     23 | 5242 | `		c = zString[0];` |
|      - | 5243 | `		/* Perform the lookup */` |
|     51 | 5244 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 5245 | `			if( c == zMask[i] ){` |
|      9 | 5246 | `				break;` |
|      - | 5247 | `			}` |
|     15 | 5248 | `		}` |
|     23 | 5249 | `		if( i < nMaskLen ){` |
|      - | 5250 | `			/* Character in the current mask,break immediately */` |
|      9 | 5251 | `			break;` |
|      - | 5252 | `		}` |
|      - | 5253 | `		/* Advance cursor */` |
|     15 | 5254 | `		zString++;` |
|      1 | 5255 | `	}` |
|      - | 5256 | `	/* Longest match */` |
|     11 | 5257 | `	return (int)(zString-zIn);` |
|      1 | 5258 |  |
|      - | 5259 | `/*` |
|      - | 5260 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5261 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 5262 | ` *  of characters contained within a given mask.` |
|      - | 5263 | ` * Parameters` |
|      - | 5264 | ` * $str` |
|      - | 5265 | ` *  The input string.` |
|      - | 5266 | ` * $mask` |
|      - | 5267 | ` *  The list of allowable characters.` |
|      - | 5268 | ` * $start` |
|      - | 5269 | ` *  The position in subject to start searching.` |
|      - | 5270 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5271 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5272 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5273 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5274 | ` *  start'th position from the end of subject.` |
|      - | 5275 | ` * $length` |
|      - | 5276 | ` *  The length of the segment from subject to examine.` |
|      - | 5277 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5278 | ` *  characters after the starting position.` |
|      - | 5279 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5280 | ` *  position up to length characters from the end of subject.` |
|      - | 5281 | ` * Return` |
|      - | 5282 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 5283 | ` * in mask.` |
|      - | 5284 | ` */` |
|     26 | 5285 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5286 |  |
|      - | 5287 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5288 | `	int iMasklen,iLen;` |
|      - | 5289 | `	SyString sToken;` |
|     27 | 5290 | `	int iCount = 0;` |
|      - | 5291 | `	int rc;` |
|     27 | 5292 | `	if( nArg < 2 ){` |
|      - | 5293 | `		/* Missing agruments,return zero */` |
|      3 | 5294 | `		ph7_result_int(pCtx,0);` |
|      3 | 5295 | `		return PH7_OK;` |
|      - | 5296 | `	}` |
|      - | 5297 | `	/* Extract the target string */` |
|     25 | 5298 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5299 | `	/* Extract the mask */` |
|     25 | 5300 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 5301 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 5302 | `		/* Nothing to process,return zero */` |
|      7 | 5303 | `		ph7_result_int(pCtx,0);` |
|      7 | 5304 | `		return PH7_OK;` |
|      - | 5305 | `	}` |
|     19 | 5306 | `	if( nArg > 2 ){` |
|      - | 5307 | `		int nOfft;` |
|      - | 5308 | `		/* Extract the offset */` |
|      9 | 5309 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 5310 | `		if( nOfft < 0 ){` |
|    ! 0 | 5311 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5312 | `			if( zBase > zString ){` |
|    ! 0 | 5313 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5314 | `				zString = zBase;` |
|    ! 0 | 5315 | `			}else{` |
|      - | 5316 | `				/* Invalid offset */` |
|    ! 0 | 5317 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5318 | `				return PH7_OK;` |
|      - | 5319 | `			}` |
|    ! 0 | 5320 | `		}else{` |
|      9 | 5321 | `			if( nOfft >= iLen ){` |
|      - | 5322 | `				/* Invalid offset */` |
|    ! 0 | 5323 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5324 | `				return PH7_OK;` |
|    ! 0 | 5325 | `			}else{` |
|      - | 5326 | `				/* Update offset */` |
|      9 | 5327 | `				zString += nOfft;` |
|      9 | 5328 | `				iLen -= nOfft;` |
|      - | 5329 | `			}` |
|      - | 5330 | `		}` |
|      9 | 5331 | `		if( nArg > 3 ){` |
|      - | 5332 | `			int iUserlen;` |
|      - | 5333 | `			/* Extract the desired length */` |
|      9 | 5334 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 5335 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 5336 | `				iLen = iUserlen;` |
|      2 | 5337 | `			}` |
|      4 | 5338 | `		}` |
|      4 | 5339 | `	}` |
|      - | 5340 | `	/* Point to the end of the string */` |
|     19 | 5341 | `	zEnd = &zString[iLen];` |
|      - | 5342 | `	/* Extract the first non-space token */` |
|     19 | 5343 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 5344 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5345 | `		/* Compare against the current mask */` |
|     19 | 5346 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 5347 | `	}` |
|      - | 5348 | `	/* Longest match */` |
|     19 | 5349 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 5350 | `	return PH7_OK;` |
|     14 | 5351 |  |
|      - | 5352 | `/*` |
|      - | 5353 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5354 | ` *  Find length of initial segment not matching mask.` |
|      - | 5355 | ` * Parameters` |
|      - | 5356 | ` * $str` |
|      - | 5357 | ` *  The input string.` |
|      - | 5358 | ` * $mask` |
|      - | 5359 | ` *  The list of not allowed characters.` |
|      - | 5360 | ` * $start` |
|      - | 5361 | ` *  The position in subject to start searching.` |
|      - | 5362 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5363 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5364 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5365 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5366 | ` *  start'th position from the end of subject.` |
|      - | 5367 | ` * $length` |
|      - | 5368 | ` *  The length of the segment from subject to examine.` |
|      - | 5369 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5370 | ` *  characters after the starting position.` |
|      - | 5371 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5372 | ` *  position up to length characters from the end of subject.` |
|      - | 5373 | ` * Return` |
|      - | 5374 | ` *  Returns the length of the segment as an integer.` |
|      - | 5375 | ` */` |
|     16 | 5376 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5377 |  |
|      - | 5378 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5379 | `	int iMasklen,iLen;` |
|      - | 5380 | `	SyString sToken;` |
|     17 | 5381 | `	int iCount = 0;` |
|      - | 5382 | `	int rc;` |
|     17 | 5383 | `	if( nArg < 2 ){` |
|      - | 5384 | `		/* Missing agruments,return zero */` |
|      3 | 5385 | `		ph7_result_int(pCtx,0);` |
|      3 | 5386 | `		return PH7_OK;` |
|      - | 5387 | `	}` |
|      - | 5388 | `	/* Extract the target string */` |
|     15 | 5389 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5390 | `	/* Extract the mask */` |
|     15 | 5391 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 5392 | `	if( iLen < 1 ){` |
|      - | 5393 | `		/* Nothing to process,return zero */` |
|    ! 0 | 5394 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5395 | `		return PH7_OK;` |
|      - | 5396 | `	}` |
|     15 | 5397 | `	if( iMasklen < 1 ){` |
|      - | 5398 | `		/* No given mask,return the string length */` |
|      3 | 5399 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 5400 | `		return PH7_OK;` |
|      - | 5401 | `	}` |
|     13 | 5402 | `	if( nArg > 2 ){` |
|      - | 5403 | `		int nOfft;` |
|      - | 5404 | `		/* Extract the offset */` |
|     11 | 5405 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 5406 | `		if( nOfft < 0 ){` |
|    ! 0 | 5407 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5408 | `			if( zBase > zString ){` |
|    ! 0 | 5409 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5410 | `				zString = zBase;` |
|    ! 0 | 5411 | `			}else{` |
|      - | 5412 | `				/* Invalid offset */` |
|    ! 0 | 5413 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5414 | `				return PH7_OK;` |
|      - | 5415 | `			}` |
|    ! 0 | 5416 | `		}else{` |
|     11 | 5417 | `			if( nOfft >= iLen ){` |
|      - | 5418 | `				/* Invalid offset */` |
|      3 | 5419 | `				ph7_result_int(pCtx,0);` |
|      3 | 5420 | `				return PH7_OK;` |
|    ! 0 | 5421 | `			}else{` |
|      - | 5422 | `				/* Update offset */` |
|      9 | 5423 | `				zString += nOfft;` |
|      9 | 5424 | `				iLen -= nOfft;` |
|      - | 5425 | `			}` |
|      - | 5426 | `		}` |
|      9 | 5427 | `		if( nArg > 3 ){` |
|      - | 5428 | `			int iUserlen;` |
|      - | 5429 | `			/* Extract the desired length */` |
|    ! 0 | 5430 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 5431 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 5432 | `				iLen = iUserlen;` |
|    ! 0 | 5433 | `			}` |
|    ! 0 | 5434 | `		}` |
|      4 | 5435 | `	}` |
|      - | 5436 | `	/* Point to the end of the string */` |
|     11 | 5437 | `	zEnd = &zString[iLen];` |
|      - | 5438 | `	/* Extract the first non-space token */` |
|     11 | 5439 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 5440 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5441 | `		/* Compare against the current mask */` |
|     11 | 5442 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 5443 | `	}` |
|      - | 5444 | `	/* Longest match */` |
|     11 | 5445 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 5446 | `	return PH7_OK;` |
|      9 | 5447 |  |
|      - | 5448 | `/*` |
|      - | 5449 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 5450 | ` *  Search a string for any of a set of characters.` |
|      - | 5451 | ` * Parameters` |
|      - | 5452 | ` *  $haystack` |
|      - | 5453 | ` *   The string where char_list is looked for.` |
|      - | 5454 | ` *  $char_list` |
|      - | 5455 | ` *   This parameter is case sensitive.` |
|      - | 5456 | ` * Return` |
|      - | 5457 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 5458 | ` */` |
|      6 | 5459 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5460 |  |
|      - | 5461 | `	const char *zString,*zList,*zEnd;` |
|      - | 5462 | `	int iLen,iListLen,i,c;` |
|      - | 5463 | `	sxu32 nOfft,nMax;` |
|      - | 5464 | `	sxi32 rc;` |
|      7 | 5465 | `	if( nArg < 2 ){` |
|      - | 5466 | `		/* Missing arguments,return FALSE */` |
|      3 | 5467 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5468 | `		return PH7_OK;` |
|      - | 5469 | `	}` |
|      - | 5470 | `	/* Extract the haystack and the char list */` |
|      5 | 5471 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 5472 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 5473 | `	if( iLen < 1 ){` |
|      - | 5474 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 5475 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5476 | `		return PH7_OK;` |
|      - | 5477 | `	}` |
|      - | 5478 | `	/* Point to the end of the string */` |
|      5 | 5479 | `	zEnd = &zString[iLen];` |
|      5 | 5480 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 5481 | `	/* perform the requested operation */` |
|     15 | 5482 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 5483 | `		c = zList[i];` |
|     11 | 5484 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 5485 | `		if( rc == SXRET_OK ){` |
|      5 | 5486 | `			if( nMax < nOfft ){` |
|      3 | 5487 | `				nOfft = nMax;` |
|      1 | 5488 | `			}` |
|      2 | 5489 | `		}` |
|      6 | 5490 | `	}` |
|      5 | 5491 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 5492 | `		/* No such substring,return FALSE */` |
|      3 | 5493 | `		ph7_result_bool(pCtx,0);` |
|      2 | 5494 | `	}else{` |
|      - | 5495 | `		/* Return the substring */` |
|      3 | 5496 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 5497 | `	}` |
|      5 | 5498 | `	return PH7_OK;` |
|      4 | 5499 |  |
|      - | 5500 | `/*` |
|      - | 5501 | ` * string soundex(string $str)` |
|      - | 5502 | ` *  Calculate the soundex key of a string.` |
|      - | 5503 | ` * Parameters` |
|      - | 5504 | ` *  $str` |
|      - | 5505 | ` *   The input string.` |
|      - | 5506 | ` * Return` |
|      - | 5507 | ` *  Returns the soundex key as a string.` |
|      - | 5508 | ` * Note:` |
|      - | 5509 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 5510 | ` * source tree.` |
|      - | 5511 | ` */` |
|     20 | 5512 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5513 |  |
|      - | 5514 | `	const unsigned char *zIn;` |
|      - | 5515 | `	char zResult[8];` |
|      - | 5516 | `	int i, j;` |
|      - | 5517 | `	static const unsigned char iCode[] = {` |
|      - | 5518 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 5519 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 5520 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 5521 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 5522 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 5523 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5524 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 5525 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5526 | `	};` |
|     21 | 5527 | `	if( nArg < 1 ){` |
|      - | 5528 | `		/* Missing arguments,return the empty string */` |
|      3 | 5529 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5530 | `		return PH7_OK;` |
|      - | 5531 | `	}` |
|     19 | 5532 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     19 | 5533 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     19 | 5534 | `	if( zIn[i] ){` |
|     17 | 5535 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 5536 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 5537 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 5538 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 5539 | `			if( code>0 ){` |
|     45 | 5540 | `				if( code!=prevcode ){` |
|     33 | 5541 | `					prevcode = (unsigned char)code;` |
|     33 | 5542 | `					zResult[j++] = (char)code + '0';` |
|     16 | 5543 | `				}` |
|     23 | 5544 | `			}else{` |
|     49 | 5545 | `				prevcode = 0;` |
|      - | 5546 | `			}` |
|     47 | 5547 | `		}` |
|     33 | 5548 | `		while( j<4 ){` |
|     17 | 5549 | `			zResult[j++] = '0';` |
|      1 | 5550 | `		}` |
|     17 | 5551 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 5552 | `	}else{` |
|      3 | 5553 | `	  ph7_result_string(pCtx,"?000",4);` |
|      - | 5554 | `	}` |
|     19 | 5555 | `	return PH7_OK;` |
|     11 | 5556 |  |
|      - | 5557 | `/*` |
|      - | 5558 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 5559 | ` *  Wraps a string to a given number of characters.` |
|      - | 5560 | ` * Parameters` |
|      - | 5561 | ` *  $str` |
|      - | 5562 | ` *   The input string.` |
|      - | 5563 | ` * $width` |
|      - | 5564 | ` *  The column width.` |
|      - | 5565 | ` * $break` |
|      - | 5566 | ` *  The line is broken using the optional break parameter.` |
|      - | 5567 | ` * Return` |
|      - | 5568 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 5569 | ` */` |
|     14 | 5570 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5571 |  |
|      - | 5572 | `	const char *zIn,*zEnd,*zBreak;` |
|      - | 5573 | `	int iLen,iBreaklen,iChunk;` |
|     15 | 5574 | `	if( nArg < 1 ){` |
|      - | 5575 | `		/* Missing arguments,return the empty string */` |
|      3 | 5576 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5577 | `		return PH7_OK;` |
|      - | 5578 | `	}` |
|      - | 5579 | `	/* Extract the input string */` |
|     13 | 5580 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|     13 | 5581 | `	if( iLen < 1 ){` |
|      - | 5582 | `		/* Nothing to process,return the empty string */` |
|      3 | 5583 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5584 | `		return PH7_OK;` |
|      - | 5585 | `	}` |
|      - | 5586 | `	/* Chunk length */` |
|     11 | 5587 | `	iChunk = 75;` |
|     11 | 5588 | `	iBreaklen = 0;` |
|     11 | 5589 | `	zBreak = ""; /* cc warning */` |
|     11 | 5590 | `	if( nArg > 1 ){` |
|     11 | 5591 | `		iChunk = ph7_value_to_int(apArg[1]);` |
|     11 | 5592 | `		if( iChunk < 1 ){` |
|    ! 0 | 5593 | `			iChunk = 75;` |
|    ! 0 | 5594 | `		}` |
|     11 | 5595 | `		if( nArg > 2 ){` |
|      3 | 5596 | `			zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      1 | 5597 | `		}` |
|      5 | 5598 | `	}` |
|     11 | 5599 | `	if( iBreaklen < 1 ){` |
|      - | 5600 | `		/* Set a default column break */` |
|      - | 5601 | `#ifdef __WINNT__` |
|      1 | 5602 | `		zBreak = "\r\n";` |
|      1 | 5603 | `		iBreaklen = (int)sizeof("\r\n")-1;` |
|      - | 5604 | `#else` |
|      8 | 5605 | `		zBreak = "\n";` |
|      8 | 5606 | `		iBreaklen = (int)sizeof(char);` |
|      - | 5607 | `#endif` |
|      4 | 5608 | `	}` |
|      - | 5609 | `	/* Perform the requested operation */` |
|     11 | 5610 | `	zEnd = &zIn[iLen];` |
|     41 | 5611 | `	for(;;){` |
|      - | 5612 | `		int nMax;` |
|     47 | 5613 | `		if( zIn >= zEnd ){` |
|      - | 5614 | `			/* No more input to process */` |
|     11 | 5615 | `			break;` |
|      - | 5616 | `		}` |
|     37 | 5617 | `		nMax = (int)(zEnd-zIn);` |
|     37 | 5618 | `		if( iChunk > nMax ){` |
|     11 | 5619 | `			iChunk = nMax;` |
|      5 | 5620 | `		}` |
|      - | 5621 | `		/* Append the column first */` |
|     37 | 5622 | `		ph7_result_string(pCtx,zIn,iChunk); /* Will make it's own copy */` |
|      - | 5623 | `		/* Advance the cursor */` |
|     37 | 5624 | `		zIn += iChunk;` |
|     37 | 5625 | `		if( zIn < zEnd ){` |
|      - | 5626 | `			/* Append the line break */` |
|     27 | 5627 | `			ph7_result_string(pCtx,zBreak,iBreaklen);` |
|     13 | 5628 | `		}` |
|      1 | 5629 | `	}` |
|     11 | 5630 | `	return PH7_OK;` |
|      8 | 5631 |  |
|      - | 5632 | `/*` |
|      - | 5633 | ` * Check if the given character is a member of the given mask.` |
|      - | 5634 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 5635 | ` * Refer to [strtok()].` |
|      - | 5636 | ` */` |
|     30 | 5637 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 5638 |  |
|      - | 5639 | `	int i;` |
|     57 | 5640 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 5641 | `		if( c == zMask[i] ){` |
|     13 | 5642 | `			if( pOfft ){` |
|      5 | 5643 | `				*pOfft = i;` |
|      2 | 5644 | `			}` |
|     13 | 5645 | `			return TRUE;` |
|      - | 5646 | `		}` |
|     14 | 5647 | `	}` |
|     19 | 5648 | `	return FALSE;` |
|     16 | 5649 |  |
|      - | 5650 | `/*` |
|      - | 5651 | ` * Extract a single token from the input stream.` |
|      - | 5652 | ` * Refer to [strtok()].` |
|      - | 5653 | ` */` |
|      6 | 5654 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 5655 |  |
|      7 | 5656 | `	const char *zIn = *pzIn;` |
|      - | 5657 | `	const char *zPtr;` |
|      - | 5658 | `	/* Ignore leading delimiter */` |
|     11 | 5659 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5660 | `		zIn++;` |
|      1 | 5661 | `	}` |
|      7 | 5662 | `	if( zIn >= zEnd ){` |
|      - | 5663 | `		/* End of input */` |
|    ! 0 | 5664 | `		return SXERR_EOF;` |
|      - | 5665 | `	}` |
|      7 | 5666 | `	zPtr = zIn;` |
|      - | 5667 | `	/* Extract the token */` |
|     13 | 5668 | `	while( zIn < zEnd ){` |
|     11 | 5669 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 5670 | `			/* UTF-8 stream */` |
|    ! 0 | 5671 | `			zIn++;` |
|    ! 0 | 5672 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 5673 | `		}else{` |
|     11 | 5674 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5675 | `				break;` |
|      - | 5676 | `			}` |
|      7 | 5677 | `			zIn++;` |
|      - | 5678 | `		}` |
|      1 | 5679 | `	}` |
|      7 | 5680 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5681 | `	/* Update the cursor */` |
|      7 | 5682 | `	*pzIn = zIn;` |
|      - | 5683 | `	/* Return to the caller */` |
|      7 | 5684 | `	return SXRET_OK;` |
|      4 | 5685 |  |
|      - | 5686 | `/* strtok auxiliary private data */` |
|      - | 5687 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 5688 | `struct strtok_aux_data` |
|      - | 5689 |  |
|      - | 5690 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 5691 | `	const char *zIn;   /* Current input stream */` |
|      - | 5692 | `	const char *zEnd;  /* End of input */` |
|      - | 5693 | `};` |
|      - | 5694 | `/*` |
|      - | 5695 | ` * string strtok(string $str,string $token)` |
|      - | 5696 | ` * string strtok(string $token)` |
|      - | 5697 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 5698 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 5699 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 5700 | ` *  words by using the space character as the token.` |
|      - | 5701 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 5702 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 5703 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 5704 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 5705 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 5706 | ` *  the argument are found.` |
|      - | 5707 | ` * Parameters` |
|      - | 5708 | ` *  $str` |
|      - | 5709 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 5710 | ` * $token` |
|      - | 5711 | ` *  The delimiter used when splitting up str.` |
|      - | 5712 | ` * Return` |
|      - | 5713 | ` *   Current token or FALSE on EOF.` |
|      - | 5714 | ` */` |
|      8 | 5715 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5716 |  |
|      - | 5717 | `	strtok_aux_data *pAux;` |
|      - | 5718 | `	const char *zMask;` |
|      - | 5719 | `	SyString sToken;` |
|      - | 5720 | `	int nMasklen;` |
|      - | 5721 | `	sxi32 rc;` |
|      9 | 5722 | `	if( nArg < 2 ){` |
|      - | 5723 | `		/* Extract top aux data */` |
|      7 | 5724 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      7 | 5725 | `		if( pAux == 0 ){` |
|      - | 5726 | `			/* No aux data,return FALSE */` |
|    ! 0 | 5727 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5728 | `			return PH7_OK;` |
|      - | 5729 | `		}` |
|      7 | 5730 | `		nMasklen = 0;` |
|      7 | 5731 | `		zMask = ""; /* cc warning */` |
|      7 | 5732 | `		if( nArg > 0 ){` |
|      - | 5733 | `			/* Extract the mask */` |
|      5 | 5734 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 5735 | `		}` |
|      7 | 5736 | `		if( nMasklen < 1 ){` |
|      - | 5737 | `			/* Invalid mask,return FALSE */` |
|      3 | 5738 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|      3 | 5739 | `			ph7_context_free_chunk(pCtx,pAux);` |
|      3 | 5740 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|      3 | 5741 | `			ph7_result_bool(pCtx,0);` |
|      3 | 5742 | `			return PH7_OK;` |
|      - | 5743 | `		}` |
|      - | 5744 | `		/* Extract the token */` |
|      5 | 5745 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 5746 | `		if( rc != SXRET_OK ){` |
|      - | 5747 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 5748 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 5749 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5750 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 5751 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5752 | `		}else{` |
|      - | 5753 | `			/* Return the extracted token */` |
|      5 | 5754 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 5755 | `		}` |
|      3 | 5756 | `	}else{` |
|      - | 5757 | `		const char *zInput,*zCur;` |
|      - | 5758 | `		char *zDup;` |
|      - | 5759 | `		int nLen;` |
|      - | 5760 | `		/* Extract the raw input */` |
|      3 | 5761 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5762 | `		if( nLen < 1 ){` |
|      - | 5763 | `			/* Empty input,return FALSE */` |
|    ! 0 | 5764 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5765 | `			return PH7_OK;` |
|      - | 5766 | `		}` |
|      - | 5767 | `		/* Extract the mask */` |
|      3 | 5768 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 5769 | `		if( nMasklen < 1 ){` |
|      - | 5770 | `			/* Set a default mask */` |
|      - | 5771 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 5772 | `			zMask = TOK_MASK;` |
|    ! 0 | 5773 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 5774 | `#undef TOK_MASK` |
|    ! 0 | 5775 | `		}` |
|      - | 5776 | `		/* Extract a single token */` |
|      3 | 5777 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 5778 | `		if( rc != SXRET_OK ){` |
|      - | 5779 | `			/* Empty input */` |
|    ! 0 | 5780 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5781 | `			return PH7_OK;` |
|    ! 0 | 5782 | `		}else{` |
|      - | 5783 | `			/* Return the extracted token */` |
|      3 | 5784 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 5785 | `		}` |
|      - | 5786 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 5787 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 5788 | `		if( pAux ){` |
|      3 | 5789 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 5790 | `			if( nLen < 1 ){` |
|    ! 0 | 5791 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5792 | `				return PH7_OK;` |
|      - | 5793 | `			}` |
|      - | 5794 | `			/* Duplicate input */` |
|      3 | 5795 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 5796 | `			if( zDup  ){` |
|      3 | 5797 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 5798 | `				/* Register the aux data */` |
|      3 | 5799 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 5800 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 5801 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 5802 | `			}` |
|      1 | 5803 | `		}` |
|      - | 5804 | `	}` |
|      7 | 5805 | `	return PH7_OK;` |
|      5 | 5806 |  |
|      - | 5807 | `/*` |
|      - | 5808 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 5809 | ` *  Pad a string to a certain length with another string` |
|      - | 5810 | ` * Parameters` |
|      - | 5811 | ` *  $input` |
|      - | 5812 | ` *   The input string.` |
|      - | 5813 | ` * $pad_length` |
|      - | 5814 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 5815 | ` *   string, no padding takes place.` |
|      - | 5816 | ` * $pad_string` |
|      - | 5817 | ` *   Note:` |
|      - | 5818 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 5819 | ` *    divided by the pad_string's length.` |
|      - | 5820 | ` * $pad_type` |
|      - | 5821 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 5822 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 5823 | ` * Return` |
|      - | 5824 | ` *  The padded string.` |
|      - | 5825 | ` */` |
|     10 | 5826 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5827 |  |
|      - | 5828 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 5829 | `	const char *zIn,*zPad;` |
|     11 | 5830 | `	if( nArg < 2 ){` |
|      - | 5831 | `		/* Missing arguments,return the empty string */` |
|      5 | 5832 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 5833 | `		return PH7_OK;` |
|      - | 5834 | `	}` |
|      - | 5835 | `	/* Extract the target string */` |
|      7 | 5836 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5837 | `	/* Padding length */` |
|      7 | 5838 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|      7 | 5839 | `	if( iPadlen > 0 ){` |
|      5 | 5840 | `		iPadlen -= iLen;` |
|      2 | 5841 | `	}` |
|      7 | 5842 | `	if( iPadlen < 1  ){` |
|      - | 5843 | `		/* Return the string verbatim */` |
|      3 | 5844 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      3 | 5845 | `		return PH7_OK;` |
|      - | 5846 | `	}` |
|      5 | 5847 | `	zPad = " "; /* Whitespace padding */` |
|      5 | 5848 | `	iStrpad = (int)sizeof(char);` |
|      5 | 5849 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      5 | 5850 | `	if( nArg > 2 ){` |
|      - | 5851 | `		/* Padding string */` |
|      5 | 5852 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      5 | 5853 | `		if( iStrpad < 1 ){` |
|      - | 5854 | `			/* Empty string */` |
|    ! 0 | 5855 | `			zPad = " "; /* Whitespace padding */` |
|    ! 0 | 5856 | `			iStrpad = (int)sizeof(char);` |
|    ! 0 | 5857 | `		}` |
|      5 | 5858 | `		if( nArg > 3 ){` |
|      - | 5859 | `			/* Padd type */` |
|      5 | 5860 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 5861 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 5862 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 5863 | `			}` |
|      2 | 5864 | `		}` |
|      2 | 5865 | `	}` |
|      5 | 5866 | `	iDiv = 1;` |
|      5 | 5867 | `	if( iType == 2 ){` |
|    ! 0 | 5868 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 5869 | `	}` |
|      - | 5870 | `	/* Perform the requested operation */` |
|      5 | 5871 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 5872 | `		jPad = iStrpad;` |
|      5 | 5873 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 5874 | `			/* Padding */` |
|      5 | 5875 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 5876 | `				break;` |
|      - | 5877 | `			}` |
|      3 | 5878 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      2 | 5879 | `		}` |
|      3 | 5880 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 5881 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 5882 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 5883 | `				if( jPad > iStrpad ){` |
|    ! 0 | 5884 | `					jPad = iStrpad;` |
|    ! 0 | 5885 | `				}` |
|      3 | 5886 | `				if( jPad < 1){` |
|    ! 0 | 5887 | `					break;` |
|      - | 5888 | `				}` |
|      3 | 5889 | `				ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 5890 | `			}` |
|      1 | 5891 | `		}` |
|      1 | 5892 | `	}` |
|      5 | 5893 | `	if( iLen > 0 ){` |
|      - | 5894 | `		/* Append the input string */` |
|      5 | 5895 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      2 | 5896 | `	}` |
|      5 | 5897 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 5898 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 5899 | `			/* Padding */` |
|      5 | 5900 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 5901 | `				break;` |
|      - | 5902 | `			}` |
|      3 | 5903 | `			ph7_result_string(pCtx,zPad,iStrpad);` |
|      2 | 5904 | `		}` |
|      5 | 5905 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 5906 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 5907 | `			if( jPad > iStrpad ){` |
|    ! 0 | 5908 | `				jPad = iStrpad;` |
|    ! 0 | 5909 | `			}` |
|      3 | 5910 | `			if( jPad < 1){` |
|    ! 0 | 5911 | `				break;` |
|      - | 5912 | `			}` |
|      3 | 5913 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 5914 | `		}` |
|      1 | 5915 | `	}` |
|      5 | 5916 | `	return PH7_OK;` |
|      6 | 5917 |  |
|      - | 5918 | `/*` |
|      - | 5919 | ` * String replacement private data.` |
|      - | 5920 | ` */` |
|      - | 5921 | `typedef struct str_replace_data str_replace_data;` |
|      - | 5922 | `struct str_replace_data` |
|      - | 5923 |  |
|      - | 5924 | `	/* The following two fields are only used by the strtr function */` |
|      - | 5925 | `	SyBlob *pWorker;         /* Working buffer */` |
|      - | 5926 | `	ProcStringMatch xMatch;  /* Pattern match routine */` |
|      - | 5927 | `	/* The following two fields are only used by the str_replace function */` |
|      - | 5928 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 5929 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 5930 | `};` |
|      - | 5931 | `/*` |
|      - | 5932 | ` * Remove a substring.` |
|      - | 5933 | ` */` |
|      - | 5934 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 5935 | `	for(;;){\` |
|      - | 5936 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 5937 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 5938 | `		++OFFT;\` |
|      - | 5939 | `	}\` |
|      - | 5940 |  |
|      - | 5941 | `/*` |
|      - | 5942 | ` * Shift right and insert algorithm.` |
|      - | 5943 | ` */` |
|      - | 5944 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 5945 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 5946 | `		for(;;){\` |
|      - | 5947 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 5948 | `			if(INLEN < 1 ) { break; }\` |
|      - | 5949 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 5950 | `			--INLEN; \` |
|      - | 5951 | `		}\` |
|      - | 5952 | `		for(;;){\` |
|      - | 5953 | `				if(ELEN < 1) { break; }\` |
|      - | 5954 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 5955 | `				OFFT++;\` |
|      - | 5956 | `				ENTRY++;\` |
|      - | 5957 | `				--ELEN;\` |
|      - | 5958 | `		}\` |
|      - | 5959 |  |
|      - | 5960 | `/*` |
|      - | 5961 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 5962 | ` * replacement string [i.e: zReplace].` |
|      - | 5963 | ` */` |
|     38 | 5964 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 5965 |  |
|     39 | 5966 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 5967 | `	sxu32 n,m;` |
|     39 | 5968 | `	n = SyBlobLength(pWorker);` |
|     39 | 5969 | `	m = nOfft;` |
|      - | 5970 | `	/* Delete the old entry */` |
|    475 | 5971 | `	STRDEL(zInput,n,m,nLen);` |
|     39 | 5972 | `	SyBlobLength(pWorker) -= nLen;` |
|     39 | 5973 | `	if( nReplen > 0 ){` |
|     33 | 5974 | `		sxi32 iRep = nReplen;` |
|      - | 5975 | `		sxi32 rc;` |
|      - | 5976 | `		/*` |
|      - | 5977 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 5978 | `		 * string.` |
|      - | 5979 | `		 */` |
|     33 | 5980 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     33 | 5981 | `		if( rc != SXRET_OK ){` |
|      - | 5982 | `			/* Simply ignore any memory failure problem */` |
|    ! 0 | 5983 | `			return SXRET_OK;` |
|      - | 5984 | `		}` |
|      - | 5985 | `		/* Perform the insertion now */` |
|     33 | 5986 | `		zInput = (char *)SyBlobData(pWorker);` |
|     33 | 5987 | `		n = SyBlobLength(pWorker);` |
|    163 | 5988 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     33 | 5989 | `		SyBlobLength(pWorker) += nReplen;` |
|     16 | 5990 | `	}` |
|     39 | 5991 | `	return SXRET_OK;` |
|     20 | 5992 |  |
|      - | 5993 | `/*` |
|      - | 5994 | ` * String replacement walker callback.` |
|      - | 5995 | ` * The following callback is invoked for each array entry that hold` |
|      - | 5996 | ` * the replace string.` |
|      - | 5997 | ` * Refer to the strtr() implementation for more information.` |
|      - | 5998 | ` */` |
|      8 | 5999 | `static int StringReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6000 |  |
|      9 | 6001 | `	str_replace_data *pRepData = (str_replace_data *)pUserData;` |
|      - | 6002 | `	const char *zTarget,*zReplace;` |
|      - | 6003 | `	SyBlob *pWorker;` |
|      - | 6004 | `	int tLen,nLen;` |
|      - | 6005 | `	sxu32 nOfft;` |
|      - | 6006 | `	sxi32 rc;` |
|      - | 6007 | `	/* Point to the working buffer */` |
|      9 | 6008 | `	pWorker = pRepData->pWorker;` |
|      9 | 6009 | `	if( !ph7_value_is_string(pKey) ){` |
|      - | 6010 | `		/* Target and replace must be a string */` |
|      3 | 6011 | `		return PH7_OK;` |
|      - | 6012 | `	}` |
|      - | 6013 | `	/* Extract the target and the replace */` |
|      7 | 6014 | `	zTarget = ph7_value_to_string(pKey,&tLen);` |
|      7 | 6015 | `	if( tLen < 1 ){` |
|      - | 6016 | `		/* Empty target,return immediately */` |
|    ! 0 | 6017 | `		return PH7_OK;` |
|      - | 6018 | `	}` |
|      - | 6019 | `	/* Perform a pattern search */` |
|      7 | 6020 | `	rc = pRepData->xMatch(SyBlobData(pWorker),SyBlobLength(pWorker),(const void *)zTarget,(sxu32)tLen,&nOfft);` |
|      7 | 6021 | `	if( rc != SXRET_OK ){` |
|      - | 6022 | `		/* Pattern not found */` |
|    ! 0 | 6023 | `		return PH7_OK;` |
|      - | 6024 | `	}` |
|      - | 6025 | `	/* Extract the replace string */` |
|      7 | 6026 | `	zReplace = ph7_value_to_string(pData,&nLen);` |
|      - | 6027 | `	/* Perform the replace process */` |
|      7 | 6028 | `	StringReplace(pWorker,nOfft,tLen,zReplace,nLen);` |
|      - | 6029 | `	/* All done */` |
|      7 | 6030 | `	return PH7_OK;` |
|      5 | 6031 |  |
|      - | 6032 | `/*` |
|      - | 6033 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 6034 | ` * to collect search/replace string.` |
|      - | 6035 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 6036 | ` */` |
|     26 | 6037 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6038 |  |
|     27 | 6039 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 6040 | `	SyString sWorker;` |
|      - | 6041 | `	const char *zIn;` |
|      - | 6042 | `	int nByte;` |
|      - | 6043 | `	/* Extract a string representation of the given argument */` |
|     27 | 6044 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 6045 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 6046 | `	if( nByte > 0 ){` |
|      - | 6047 | `		char *zDup;` |
|      - | 6048 | `		/* Duplicate the chunk */` |
|     25 | 6049 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 6050 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 6051 | `			);` |
|     25 | 6052 | `		if( zDup == 0 ){` |
|      - | 6053 | `			/* Ignore any memory failure problem */` |
|    ! 0 | 6054 | `			ph7_context_throw_error(pRep->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 6055 | `			return PH7_OK;` |
|      - | 6056 | `		}` |
|     25 | 6057 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 6058 | `		/* Save the chunk */` |
|     25 | 6059 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 6060 | `	}` |
|      - | 6061 | `	/* Save for later processing */` |
|     27 | 6062 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 6063 | `	/* All done */` |
|     13 | 6064 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 6065 | `	return PH7_OK;` |
|     14 | 6066 |  |
|      - | 6067 | `/*` |
|      - | 6068 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6069 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6070 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 6071 | ` * Parameters` |
|      - | 6072 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 6073 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 6074 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 6075 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 6076 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 6077 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 6078 | ` * $search` |
|      - | 6079 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 6080 | ` *  to designate multiple needles.` |
|      - | 6081 | ` * $replace` |
|      - | 6082 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 6083 | ` *  to designate multiple replacements.` |
|      - | 6084 | ` * $subject` |
|      - | 6085 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 6086 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 6087 | ` *  of subject, and the return value is an array as well.` |
|      - | 6088 | ` * $count (Not used)` |
|      - | 6089 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 6090 | ` * Return` |
|      - | 6091 | ` * This function returns a string or an array with the replaced values.` |
|      - | 6092 | ` */` |
|  10442 | 6093 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6094 |  |
|      - | 6095 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 6096 | `	ProcStringMatch xMatch;` |
|      - | 6097 | `	const char *zIn,*zFunc;` |
|      - | 6098 | `	str_replace_data sRep;` |
|      - | 6099 | `	SyBlob sWorker;` |
|      - | 6100 | `	SySet sReplace;` |
|      - | 6101 | `	SySet sSearch;` |
|      - | 6102 | `	int rep_str;` |
|      - | 6103 | `	int nByte;` |
|      - | 6104 | `	sxi32 rc;` |
|  10443 | 6105 | `	if( nArg < 3 ){` |
|      - | 6106 | `		/* Missing/Invalid arguments,return null */` |
|      7 | 6107 | `		ph7_result_null(pCtx);` |
|      7 | 6108 | `		return PH7_OK;` |
|      - | 6109 | `	}` |
|      - | 6110 | `	/* Initialize fields */` |
|  10437 | 6111 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  10437 | 6112 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  10437 | 6113 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  10437 | 6114 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  10437 | 6115 | `	sRep.pCtx = pCtx;` |
|  10437 | 6116 | `	sRep.pCollector = &sSearch;` |
|  10437 | 6117 | `	rep_str = 0;` |
|      - | 6118 | `	/* Extract the subject */` |
|  10437 | 6119 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  10437 | 6120 | `	if( nByte < 1 ){` |
|      - | 6121 | `		/* Nothing to replace,return the empty string */` |
|     33 | 6122 | `		ph7_result_string(pCtx,"",0);` |
|     33 | 6123 | `		return PH7_OK;` |
|      - | 6124 | `	}` |
|      - | 6125 | `	/* Copy the subject */` |
|  10405 | 6126 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 6127 | `	/* Search string */` |
|  10405 | 6128 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 6129 | `		/* Collect search string */` |
|      9 | 6130 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 6131 | `	}else{` |
|      - | 6132 | `		/* Single pattern */` |
|  10397 | 6133 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  10397 | 6134 | `		if( nByte < 1 ){` |
|      - | 6135 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 6136 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 6137 | `			return PH7_OK;` |
|      - | 6138 | `		}` |
|  10393 | 6139 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6140 | `		/* Save for later processing */` |
|  10393 | 6141 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 6142 | `	}` |
|      - | 6143 | `	/* Replace string */` |
|  10401 | 6144 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 6145 | `		/* Collect replace string */` |
|      7 | 6146 | `		sRep.pCollector = &sReplace;` |
|      7 | 6147 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 6148 | `	}else{` |
|      - | 6149 | `		/* Single needle */` |
|  10395 | 6150 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  10395 | 6151 | `		rep_str = 1;` |
|  10395 | 6152 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6153 | `		/* Save for later processing */` |
|  10395 | 6154 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 6155 | `	}` |
|      - | 6156 | `	/* Reset loop cursors */` |
|  10401 | 6157 | `	SySetResetCursor(&sSearch);` |
|  10401 | 6158 | `	SySetResetCursor(&sReplace);` |
|  10401 | 6159 | `	pReplace = pSearch = 0; /* cc warning */` |
|  10401 | 6160 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 6161 | `	/* Extract function name */` |
|  10401 | 6162 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 6163 | `	/* Set the default pattern match routine */` |
|  10401 | 6164 | `	xMatch = SyBlobSearch;` |
|  10401 | 6165 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 6166 | `		/* Case insensitive pattern match */` |
|     11 | 6167 | `		xMatch = iPatternMatch;` |
|      5 | 6168 | `	}` |
|      - | 6169 | `	/* Start the replace process */` |
|  20809 | 6170 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 6171 | `		sxu32 nCount,nOfft;` |
|  10409 | 6172 | `		if( pSearch->nByte <  1 ){` |
|      - | 6173 | `			/* Empty string,ignore */` |
|      3 | 6174 | `			continue;` |
|      - | 6175 | `		}` |
|      - | 6176 | `		/* Extract the replace string */` |
|  10407 | 6177 | `		if( rep_str ){` |
|  10397 | 6178 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|   5199 | 6179 | `		}else{` |
|     11 | 6180 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 6181 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 6182 | `				 * An empty string is used for the rest of replacement values` |
|      - | 6183 | `				 */` |
|      3 | 6184 | `				pReplace = 0;` |
|      1 | 6185 | `			}` |
|      - | 6186 | `		}` |
|  10407 | 6187 | `		if( pReplace == 0 ){` |
|      - | 6188 | `			/* Use an empty string instead */` |
|      3 | 6189 | `			pReplace = &sTemp;` |
|      1 | 6190 | `		}` |
|  10407 | 6191 | `		nOfft = nCount = 0;` |
|   5219 | 6192 | `		for(;;){` |
|  10439 | 6193 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 6194 | `				break;` |
|      - | 6195 | `			}` |
|      - | 6196 | `			/* Perform a pattern lookup */` |
|  15640 | 6197 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  10426 | 6198 | `				pSearch->nByte,&nOfft);` |
|  10427 | 6199 | `			if( rc != SXRET_OK ){` |
|      - | 6200 | `				/* Pattern not found */` |
|  10395 | 6201 | `				break;` |
|      - | 6202 | `			}` |
|      - | 6203 | `			/* Perform the replace operation */` |
|     33 | 6204 | `			StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|      - | 6205 | `			/* Increment offset counter */` |
|     33 | 6206 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 6207 | `		}` |
|      1 | 6208 | `	}` |
|      - | 6209 | `	/* All done,clean-up the mess left behind */` |
|  10401 | 6210 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  10401 | 6211 | `	SySetRelease(&sSearch);` |
|  10401 | 6212 | `	SySetRelease(&sReplace);` |
|  10401 | 6213 | `	SyBlobRelease(&sWorker);` |
|  10401 | 6214 | `	return PH7_OK;` |
|   5222 | 6215 |  |
|      - | 6216 | `/*` |
|      - | 6217 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 6218 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 6219 | ` *  Translate characters or replace substrings.` |
|      - | 6220 | ` * Parameters` |
|      - | 6221 | ` *  $str` |
|      - | 6222 | ` *  The string being translated.` |
|      - | 6223 | ` * $from` |
|      - | 6224 | ` *  The string being translated to to.` |
|      - | 6225 | ` * $to` |
|      - | 6226 | ` *  The string replacing from.` |
|      - | 6227 | ` * $replace_pairs` |
|      - | 6228 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 6229 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 6230 | ` * Return` |
|      - | 6231 | ` *  The translated string.` |
|      - | 6232 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 6233 | ` */` |
|     12 | 6234 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6235 |  |
|      - | 6236 | `	const char *zIn;` |
|      - | 6237 | `	int nLen;` |
|     13 | 6238 | `	if( nArg < 1 ){` |
|      - | 6239 | `		/* Nothing to replace,return FALSE */` |
|      7 | 6240 | `		ph7_result_bool(pCtx,0);` |
|      7 | 6241 | `		return PH7_OK;` |
|      - | 6242 | `	}` |
|      7 | 6243 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 6244 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 6245 | `		/* Invalid arguments */` |
|    ! 0 | 6246 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6247 | `		return PH7_OK;` |
|      - | 6248 | `	}` |
|      9 | 6249 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 6250 | `		str_replace_data sRepData;` |
|      - | 6251 | `		SyBlob sWorker;` |
|      - | 6252 | `		/* Initilaize the working buffer */` |
|      5 | 6253 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|      - | 6254 | `		/* Copy raw string */` |
|      5 | 6255 | `		SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nLen);` |
|      - | 6256 | `		/* Init our replace data instance */` |
|      5 | 6257 | `		sRepData.pWorker = &sWorker;` |
|      5 | 6258 | `		sRepData.xMatch = SyBlobSearch;` |
|      - | 6259 | `		/* Iterate throw array entries and perform the replace operation.*/` |
|      5 | 6260 | `		ph7_array_walk(apArg[1],StringReplaceWalker,&sRepData);` |
|      - | 6261 | `		/* All done, return the result string */` |
|      7 | 6262 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|      4 | 6263 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 6264 | `		/* Clean-up */` |
|      5 | 6265 | `		SyBlobRelease(&sWorker);` |
|      3 | 6266 | `	}else{` |
|      - | 6267 | `		int i,flen,tlen,c,iOfft;` |
|      - | 6268 | `		const char *zFrom,*zTo;` |
|      3 | 6269 | `		if( nArg < 3 ){` |
|      - | 6270 | `			/* Nothing to replace */` |
|    ! 0 | 6271 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6272 | `			return PH7_OK;` |
|      - | 6273 | `		}` |
|      - | 6274 | `		/* Extract given arguments */` |
|      3 | 6275 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 6276 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 6277 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 6278 | `			/* Nothing to replace */` |
|    ! 0 | 6279 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6280 | `			return PH7_OK;` |
|      - | 6281 | `		}` |
|      - | 6282 | `		/* Start the replace process */` |
|     13 | 6283 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 6284 | `			c = zIn[i];` |
|     11 | 6285 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 6286 | `				if ( iOfft < tlen ){` |
|      5 | 6287 | `					c = zTo[iOfft];` |
|      2 | 6288 | `				}` |
|      2 | 6289 | `			}` |
|     11 | 6290 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 6291 |  |
|      6 | 6292 | `		}` |
|      - | 6293 | `	}` |
|      7 | 6294 | `	return PH7_OK;` |
|      7 | 6295 |  |
|      - | 6296 | `/*` |
|      - | 6297 | ` * Parse an INI string.` |
|      - | 6298 | ` * According to wikipedia` |
|      - | 6299 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 6300 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 6301 | ` *  Format` |
|      - | 6302 | `*    Properties` |
|      - | 6303 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 6304 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 6305 | `*     Example:` |
|      - | 6306 | `*      name=value` |
|      - | 6307 | `*    Sections` |
|      - | 6308 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 6309 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 6310 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 6311 | `*     or the end of the file. Sections may not be nested.` |
|      - | 6312 | `*     Example:` |
|      - | 6313 | `*      [section]` |
|      - | 6314 | `*   Comments` |
|      - | 6315 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 6316 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 6317 | `*/` |
|     10 | 6318 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 6319 |  |
|      - | 6320 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     11 | 6321 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 6322 | `	SyHashEntry *pEntry;` |
|      - | 6323 | `	SyString sEntry;` |
|      - | 6324 | `	SyHash sHash;` |
|      - | 6325 | `	int c;` |
|      - | 6326 | `	/* Create an empty array and worker variables */` |
|     11 | 6327 | `	pArray = ph7_context_new_array(pCtx);` |
|     11 | 6328 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     11 | 6329 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     11 | 6330 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 6331 | `		/* Out of memory */` |
|    ! 0 | 6332 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      - | 6333 | `		/* Return FALSE */` |
|    ! 0 | 6334 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6335 | `		return PH7_OK;` |
|      - | 6336 | `	}` |
|     11 | 6337 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     11 | 6338 | `	pCur = pArray;` |
|      - | 6339 | `	/* Start the parse process */` |
|     20 | 6340 | `	for(;;){` |
|      - | 6341 | `		/* Ignore leading white spaces */` |
|     67 | 6342 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 6343 | `			zIn++;` |
|      1 | 6344 | `		}` |
|     41 | 6345 | `		if( zIn >= zEnd ){` |
|      - | 6346 | `			/* No more input to process */` |
|     11 | 6347 | `			break;` |
|      - | 6348 | `		}` |
|     31 | 6349 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6350 | `			/* Comment til the end of line */` |
|    ! 0 | 6351 | `			zIn++;` |
|    ! 0 | 6352 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 6353 | `				zIn++;` |
|    ! 0 | 6354 | `			}` |
|    ! 0 | 6355 | `			continue;` |
|      - | 6356 | `		}` |
|      - | 6357 | `		/* Reset the string cursor of the working variable */` |
|     31 | 6358 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 6359 | `		if( zIn[0] == '[' ){` |
|      - | 6360 | `			/* Section: Extract the section name */` |
|      9 | 6361 | `			zIn++;` |
|      9 | 6362 | `			zCur = zIn;` |
|     73 | 6363 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 6364 | `				zIn++;` |
|      1 | 6365 | `			}` |
|      9 | 6366 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 6367 | `				/* Save the section name */` |
|      5 | 6368 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 6369 | `				SyStringFullTrim(&sEntry);` |
|      5 | 6370 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 6371 | `				if( sEntry.nByte > 0 ){` |
|      - | 6372 | `					/* Associate an array with the section */` |
|      5 | 6373 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 6374 | `					if( pSection ){` |
|      5 | 6375 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 6376 | `						pCur = pSection;` |
|      2 | 6377 | `					}` |
|      2 | 6378 | `				}` |
|      2 | 6379 | `			}` |
|      9 | 6380 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 6381 | `		}else{` |
|      - | 6382 | `			ph7_value *pOldCur;` |
|      - | 6383 | `			int is_array;` |
|      - | 6384 | `			int iLen;` |
|      - | 6385 | `			/* Properties */` |
|     23 | 6386 | `			is_array = 0;` |
|     23 | 6387 | `			zCur = zIn;` |
|     23 | 6388 | `			iLen = 0; /* cc warning */` |
|     23 | 6389 | `			pOldCur = pCur;` |
|    155 | 6390 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 6391 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 6392 | `					/* Array */` |
|    ! 0 | 6393 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 6394 | `					is_array = 1;` |
|    ! 0 | 6395 | `					if( iLen > 0 ){` |
|    ! 0 | 6396 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 6397 | `						/* Query the hashtable */` |
|    ! 0 | 6398 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 6399 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 6400 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 6401 | `						if( pEntry ){` |
|    ! 0 | 6402 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 6403 | `						}else{` |
|      - | 6404 | `							/* Create an empty array */` |
|    ! 0 | 6405 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 6406 | `							if( pvArr ){` |
|      - | 6407 | `								/* Save the entry */` |
|    ! 0 | 6408 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 6409 | `								/* Insert the entry */` |
|    ! 0 | 6410 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6411 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 6412 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 6413 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6414 | `							}` |
|      - | 6415 | `						}` |
|    ! 0 | 6416 | `						if( pvArr ){` |
|    ! 0 | 6417 | `							pCur = pvArr;` |
|    ! 0 | 6418 | `						}` |
|    ! 0 | 6419 | `					}` |
|    ! 0 | 6420 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 6421 | `						zIn++;` |
|    ! 0 | 6422 | `					}` |
|    ! 0 | 6423 | `				}` |
|    133 | 6424 | `				zIn++;` |
|      1 | 6425 | `			}` |
|     23 | 6426 | `			if( !is_array ){` |
|     23 | 6427 | `				iLen = (int)(zIn-zCur);` |
|     11 | 6428 | `			}` |
|      - | 6429 | `			/* Trim the key */` |
|     23 | 6430 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 6431 | `			SyStringFullTrim(&sEntry);` |
|     23 | 6432 | `			if( sEntry.nByte > 0 ){` |
|     23 | 6433 | `				if( !is_array ){` |
|      - | 6434 | `					/* Save the key name */` |
|     23 | 6435 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 6436 | `				}` |
|      - | 6437 | `				/* extract key value */` |
|     23 | 6438 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 6439 | `				zIn++; /* '=' */` |
|     39 | 6440 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 6441 | `					zIn++;` |
|      1 | 6442 | `				}` |
|     23 | 6443 | `				if( zIn < zEnd ){` |
|     21 | 6444 | `					zCur = zIn;` |
|     21 | 6445 | `					c = zIn[0];` |
|     21 | 6446 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6447 | `						zIn++;` |
|      - | 6448 | `						/* Delimit the value */` |
|    ! 0 | 6449 | `						while( zIn < zEnd ){` |
|    ! 0 | 6450 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 6451 | `								break;` |
|      - | 6452 | `							}` |
|    ! 0 | 6453 | `							zIn++;` |
|    ! 0 | 6454 | `						}` |
|    ! 0 | 6455 | `						if( zIn < zEnd ){` |
|    ! 0 | 6456 | `							zIn++;` |
|    ! 0 | 6457 | `						}` |
|    ! 0 | 6458 | `					}else{` |
|    125 | 6459 | `						while( zIn < zEnd ){` |
|    123 | 6460 | `							if( zIn[0] == '\n' ){` |
|     19 | 6461 | `								if( zIn[-1] != '\\' ){` |
|     19 | 6462 | `									break;` |
|    ! 0 | 6463 | `								}` |
|    105 | 6464 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6465 | `								/* Inline comments */` |
|    ! 0 | 6466 | `								break;` |
|      - | 6467 | `							}` |
|    105 | 6468 | `							zIn++;` |
|      1 | 6469 | `						}` |
|      - | 6470 | `					}` |
|      - | 6471 | `					/* Trim the value */` |
|     21 | 6472 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 6473 | `					SyStringFullTrim(&sEntry);` |
|     21 | 6474 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6475 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 6476 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 6477 | `					}` |
|     21 | 6478 | `					if( sEntry.nByte > 0 ){` |
|     21 | 6479 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 6480 | `					}` |
|      - | 6481 | `					/* Insert the key and it's value */` |
|     21 | 6482 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 6483 | `				}` |
|     12 | 6484 | `			}else{` |
|    ! 0 | 6485 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 6486 | `					zIn++;` |
|    ! 0 | 6487 | `				}` |
|      - | 6488 | `			}` |
|     23 | 6489 | `			pCur = pOldCur;` |
|      - | 6490 | `		}` |
|      1 | 6491 | `	}` |
|     11 | 6492 | `	SyHashRelease(&sHash);` |
|      - | 6493 | `	/* Return the parse of the INI string */` |
|     11 | 6494 | `	ph7_result_value(pCtx,pArray);` |
|     11 | 6495 | `	return SXRET_OK;` |
|      6 | 6496 |  |
|      - | 6497 | `/*` |
|      - | 6498 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 6499 | ` *  Parse a configuration string.` |
|      - | 6500 | ` * Parameters` |
|      - | 6501 | ` *  $ini` |
|      - | 6502 | ` *   The contents of the ini file being parsed.` |
|      - | 6503 | ` *  $process_sections` |
|      - | 6504 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 6505 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 6506 | ` *  $scanner_mode (Not used)` |
|      - | 6507 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 6508 | ` *   then option values will not be parsed.` |
|      - | 6509 | ` * Return` |
|      - | 6510 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 6511 | ` */` |
|     10 | 6512 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6513 |  |
|      - | 6514 | `	const char *zIni;` |
|      - | 6515 | `	int nByte;` |
|     11 | 6516 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6517 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      3 | 6518 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6519 | `		return PH7_OK;` |
|      - | 6520 | `	}` |
|      - | 6521 | `	/* Extract the raw INI buffer */` |
|      9 | 6522 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 6523 | `	/* Process the INI buffer*/` |
|      9 | 6524 | `	PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      9 | 6525 | `	return PH7_OK;` |
|      6 | 6526 |  |
|      - | 6527 | `/*` |
|      - | 6528 | ` * Ctype Functions.` |
|      - | 6529 | ` * Status:` |
|      - | 6530 | ` *    Stable.` |
|      - | 6531 | ` */` |
|      - | 6532 | `/*` |
|      - | 6533 | ` * bool ctype_alnum(string $text)` |
|      - | 6534 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 6535 | ` * Parameters` |
|      - | 6536 | ` *  $text` |
|      - | 6537 | ` *   The tested string.` |
|      - | 6538 | ` * Return` |
|      - | 6539 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 6540 | ` */` |
|     16 | 6541 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6542 |  |
|      - | 6543 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6544 | `	int nLen;` |
|     17 | 6545 | `	if( nArg < 1 ){` |
|      - | 6546 | `		/* Missing arguments,return FALSE */` |
|      3 | 6547 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6548 | `		return PH7_OK;` |
|      - | 6549 | `	}` |
|      - | 6550 | `	/* Extract the target string */` |
|     15 | 6551 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 6552 | `	zEnd = &zIn[nLen];` |
|     15 | 6553 | `	if( nLen < 1 ){` |
|      - | 6554 | `		/* Empty string,return FALSE */` |
|      3 | 6555 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6556 | `		return PH7_OK;` |
|      - | 6557 | `	}` |
|      - | 6558 | `	/* Perform the requested operation */` |
|     32 | 6559 | `	for(;;){` |
|     65 | 6560 | `		if( zIn >= zEnd ){` |
|      - | 6561 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6562 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6563 | `			return PH7_OK;` |
|      - | 6564 | `		}` |
|     57 | 6565 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 6566 | `			break;` |
|      - | 6567 | `		}` |
|      - | 6568 | `		/* Point to the next character */` |
|     53 | 6569 | `		zIn++;` |
|      1 | 6570 | `	}` |
|      - | 6571 | `	/* The test failed,return FALSE */` |
|      5 | 6572 | `	ph7_result_bool(pCtx,0);` |
|      5 | 6573 | `	return PH7_OK;` |
|      9 | 6574 |  |
|      - | 6575 | `/*` |
|      - | 6576 | ` * bool ctype_alpha(string $text)` |
|      - | 6577 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 6578 | ` * Parameters` |
|      - | 6579 | ` *  $text` |
|      - | 6580 | ` *   The tested string.` |
|      - | 6581 | ` * Return` |
|      - | 6582 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 6583 | ` */` |
|     18 | 6584 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6585 |  |
|      - | 6586 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6587 | `	int nLen;` |
|     19 | 6588 | `	if( nArg < 1 ){` |
|      - | 6589 | `		/* Missing arguments,return FALSE */` |
|      3 | 6590 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6591 | `		return PH7_OK;` |
|      - | 6592 | `	}` |
|      - | 6593 | `	/* Extract the target string */` |
|     17 | 6594 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6595 | `	zEnd = &zIn[nLen];` |
|     17 | 6596 | `	if( nLen < 1 ){` |
|      - | 6597 | `		/* Empty string,return FALSE */` |
|      3 | 6598 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6599 | `		return PH7_OK;` |
|      - | 6600 | `	}` |
|      - | 6601 | `	/* Perform the requested operation */` |
|     42 | 6602 | `	for(;;){` |
|     85 | 6603 | `		if( zIn >= zEnd ){` |
|      - | 6604 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6605 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6606 | `			return PH7_OK;` |
|      - | 6607 | `		}` |
|     77 | 6608 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 6609 | `			break;` |
|      - | 6610 | `		}` |
|      - | 6611 | `		/* Point to the next character */` |
|     71 | 6612 | `		zIn++;` |
|      1 | 6613 | `	}` |
|      - | 6614 | `	/* The test failed,return FALSE */` |
|      7 | 6615 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6616 | `	return PH7_OK;` |
|     10 | 6617 |  |
|      - | 6618 | `/*` |
|      - | 6619 | ` * bool ctype_cntrl(string $text)` |
|      - | 6620 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 6621 | ` * Parameters` |
|      - | 6622 | ` *  $text` |
|      - | 6623 | ` *   The tested string.` |
|      - | 6624 | ` * Return` |
|      - | 6625 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 6626 | ` */` |
|     18 | 6627 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6628 |  |
|      - | 6629 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6630 | `	int nLen;` |
|     19 | 6631 | `	if( nArg < 1 ){` |
|      - | 6632 | `		/* Missing arguments,return FALSE */` |
|      3 | 6633 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6634 | `		return PH7_OK;` |
|      - | 6635 | `	}` |
|      - | 6636 | `	/* Extract the target string */` |
|     17 | 6637 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6638 | `	zEnd = &zIn[nLen];` |
|     17 | 6639 | `	if( nLen < 1 ){` |
|      - | 6640 | `		/* Empty string,return FALSE */` |
|      3 | 6641 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6642 | `		return PH7_OK;` |
|      - | 6643 | `	}` |
|      - | 6644 | `	/* Perform the requested operation */` |
|     14 | 6645 | `	for(;;){` |
|     29 | 6646 | `		if( zIn >= zEnd ){` |
|      - | 6647 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6648 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6649 | `			return PH7_OK;` |
|      - | 6650 | `		}` |
|     21 | 6651 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6652 | `			/* UTF-8 stream  */` |
|    ! 0 | 6653 | `			break;` |
|      - | 6654 | `		}` |
|     21 | 6655 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 6656 | `			break;` |
|      - | 6657 | `		}` |
|      - | 6658 | `		/* Point to the next character */` |
|     15 | 6659 | `		zIn++;` |
|      1 | 6660 | `	}` |
|      - | 6661 | `	/* The test failed,return FALSE */` |
|      7 | 6662 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6663 | `	return PH7_OK;` |
|     10 | 6664 |  |
|      - | 6665 | `/*` |
|      - | 6666 | ` * bool ctype_digit(string $text)` |
|      - | 6667 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 6668 | ` * Parameters` |
|      - | 6669 | ` *  $text` |
|      - | 6670 | ` *   The tested string.` |
|      - | 6671 | ` * Return` |
|      - | 6672 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 6673 | ` */` |
|    950 | 6674 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6675 |  |
|      - | 6676 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6677 | `	int nLen;` |
|    951 | 6678 | `	if( nArg < 1 ){` |
|      - | 6679 | `		/* Missing arguments,return FALSE */` |
|      3 | 6680 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6681 | `		return PH7_OK;` |
|      - | 6682 | `	}` |
|      - | 6683 | `	/* Extract the target string */` |
|    949 | 6684 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    949 | 6685 | `	zEnd = &zIn[nLen];` |
|    949 | 6686 | `	if( nLen < 1 ){` |
|      - | 6687 | `		/* Empty string,return FALSE */` |
|      3 | 6688 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6689 | `		return PH7_OK;` |
|      - | 6690 | `	}` |
|      - | 6691 | `	/* Perform the requested operation */` |
|    938 | 6692 | `	for(;;){` |
|   1877 | 6693 | `		if( zIn >= zEnd ){` |
|      - | 6694 | `			/* If we reach the end of the string,then the test succeeded. */` |
|    899 | 6695 | `			ph7_result_bool(pCtx,1);` |
|    899 | 6696 | `			return PH7_OK;` |
|      - | 6697 | `		}` |
|    979 | 6698 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6699 | `			/* UTF-8 stream  */` |
|    ! 0 | 6700 | `			break;` |
|      - | 6701 | `		}` |
|    979 | 6702 | `		if( !SyisDigit(zIn[0]) ){` |
|     49 | 6703 | `			break;` |
|      - | 6704 | `		}` |
|      - | 6705 | `		/* Point to the next character */` |
|    931 | 6706 | `		zIn++;` |
|      1 | 6707 | `	}` |
|      - | 6708 | `	/* The test failed,return FALSE */` |
|     49 | 6709 | `	ph7_result_bool(pCtx,0);` |
|     49 | 6710 | `	return PH7_OK;` |
|    476 | 6711 |  |
|      - | 6712 | `/*` |
|      - | 6713 | ` * bool ctype_xdigit(string $text)` |
|      - | 6714 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 6715 | ` * Parameters` |
|      - | 6716 | ` *  $text` |
|      - | 6717 | ` *   The tested string.` |
|      - | 6718 | ` * Return` |
|      - | 6719 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 6720 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 6721 | ` */` |
|     20 | 6722 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6723 |  |
|      - | 6724 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6725 | `	int nLen;` |
|     21 | 6726 | `	if( nArg < 1 ){` |
|      - | 6727 | `		/* Missing arguments,return FALSE */` |
|      3 | 6728 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6729 | `		return PH7_OK;` |
|      - | 6730 | `	}` |
|      - | 6731 | `	/* Extract the target string */` |
|     19 | 6732 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 6733 | `	zEnd = &zIn[nLen];` |
|     19 | 6734 | `	if( nLen < 1 ){` |
|      - | 6735 | `		/* Empty string,return FALSE */` |
|      3 | 6736 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6737 | `		return PH7_OK;` |
|      - | 6738 | `	}` |
|      - | 6739 | `	/* Perform the requested operation */` |
|     46 | 6740 | `	for(;;){` |
|     93 | 6741 | `		if( zIn >= zEnd ){` |
|      - | 6742 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 6743 | `			ph7_result_bool(pCtx,1);` |
|     11 | 6744 | `			return PH7_OK;` |
|      - | 6745 | `		}` |
|     83 | 6746 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6747 | `			/* UTF-8 stream  */` |
|    ! 0 | 6748 | `			break;` |
|      - | 6749 | `		}` |
|     83 | 6750 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 6751 | `			break;` |
|      - | 6752 | `		}` |
|      - | 6753 | `		/* Point to the next character */` |
|     77 | 6754 | `		zIn++;` |
|      1 | 6755 | `	}` |
|      - | 6756 | `	/* The test failed,return FALSE */` |
|      7 | 6757 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6758 | `	return PH7_OK;` |
|     11 | 6759 |  |
|      - | 6760 | `/*` |
|      - | 6761 | ` * bool ctype_graph(string $text)` |
|      - | 6762 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 6763 | ` * Parameters` |
|      - | 6764 | ` *  $text` |
|      - | 6765 | ` *   The tested string.` |
|      - | 6766 | ` * Return` |
|      - | 6767 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 6768 | ` * (no white space), FALSE otherwise.` |
|      - | 6769 | ` */` |
|     18 | 6770 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6771 |  |
|      - | 6772 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6773 | `	int nLen;` |
|     19 | 6774 | `	if( nArg < 1 ){` |
|      - | 6775 | `		/* Missing arguments,return FALSE */` |
|      3 | 6776 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6777 | `		return PH7_OK;` |
|      - | 6778 | `	}` |
|      - | 6779 | `	/* Extract the target string */` |
|     17 | 6780 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6781 | `	zEnd = &zIn[nLen];` |
|     17 | 6782 | `	if( nLen < 1 ){` |
|      - | 6783 | `		/* Empty string,return FALSE */` |
|      3 | 6784 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6785 | `		return PH7_OK;` |
|      - | 6786 | `	}` |
|      - | 6787 | `	/* Perform the requested operation */` |
|     57 | 6788 | `	for(;;){` |
|    115 | 6789 | `		if( zIn >= zEnd ){` |
|      - | 6790 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6791 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6792 | `			return PH7_OK;` |
|      - | 6793 | `		}` |
|    107 | 6794 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6795 | `			/* UTF-8 stream  */` |
|    ! 0 | 6796 | `			break;` |
|      - | 6797 | `		}` |
|    107 | 6798 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 6799 | `			break;` |
|      - | 6800 | `		}` |
|      - | 6801 | `		/* Point to the next character */` |
|    101 | 6802 | `		zIn++;` |
|      1 | 6803 | `	}` |
|      - | 6804 | `	/* The test failed,return FALSE */` |
|      7 | 6805 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6806 | `	return PH7_OK;` |
|     10 | 6807 |  |
|      - | 6808 | `/*` |
|      - | 6809 | ` * bool ctype_print(string $text)` |
|      - | 6810 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 6811 | ` * Parameters` |
|      - | 6812 | ` *  $text` |
|      - | 6813 | ` *   The tested string.` |
|      - | 6814 | ` * Return` |
|      - | 6815 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 6816 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 6817 | ` *  or control function at all.` |
|      - | 6818 | ` */` |
|     18 | 6819 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
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
|     63 | 6837 | `	for(;;){` |
|    127 | 6838 | `		if( zIn >= zEnd ){` |
|      - | 6839 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6840 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6841 | `			return PH7_OK;` |
|      - | 6842 | `		}` |
|    119 | 6843 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6844 | `			/* UTF-8 stream  */` |
|    ! 0 | 6845 | `			break;` |
|      - | 6846 | `		}` |
|    119 | 6847 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 6848 | `			break;` |
|      - | 6849 | `		}` |
|      - | 6850 | `		/* Point to the next character */` |
|    113 | 6851 | `		zIn++;` |
|      1 | 6852 | `	}` |
|      - | 6853 | `	/* The test failed,return FALSE */` |
|      7 | 6854 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6855 | `	return PH7_OK;` |
|     10 | 6856 |  |
|      - | 6857 | `/*` |
|      - | 6858 | ` * bool ctype_punct(string $text)` |
|      - | 6859 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 6860 | ` * Parameters` |
|      - | 6861 | ` *  $text` |
|      - | 6862 | ` *   The tested string.` |
|      - | 6863 | ` * Return` |
|      - | 6864 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 6865 | ` *  digit or blank, FALSE otherwise.` |
|      - | 6866 | ` */` |
|     20 | 6867 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6868 |  |
|      - | 6869 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6870 | `	int nLen;` |
|     21 | 6871 | `	if( nArg < 1 ){` |
|      - | 6872 | `		/* Missing arguments,return FALSE */` |
|      3 | 6873 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6874 | `		return PH7_OK;` |
|      - | 6875 | `	}` |
|      - | 6876 | `	/* Extract the target string */` |
|     19 | 6877 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 6878 | `	zEnd = &zIn[nLen];` |
|     19 | 6879 | `	if( nLen < 1 ){` |
|      - | 6880 | `		/* Empty string,return FALSE */` |
|      3 | 6881 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6882 | `		return PH7_OK;` |
|      - | 6883 | `	}` |
|      - | 6884 | `	/* Perform the requested operation */` |
|     38 | 6885 | `	for(;;){` |
|     77 | 6886 | `		if( zIn >= zEnd ){` |
|      - | 6887 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6888 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6889 | `			return PH7_OK;` |
|      - | 6890 | `		}` |
|     69 | 6891 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6892 | `			/* UTF-8 stream  */` |
|    ! 0 | 6893 | `			break;` |
|      - | 6894 | `		}` |
|     69 | 6895 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 6896 | `			break;` |
|      - | 6897 | `		}` |
|      - | 6898 | `		/* Point to the next character */` |
|     61 | 6899 | `		zIn++;` |
|      1 | 6900 | `	}` |
|      - | 6901 | `	/* The test failed,return FALSE */` |
|      9 | 6902 | `	ph7_result_bool(pCtx,0);` |
|      9 | 6903 | `	return PH7_OK;` |
|     11 | 6904 |  |
|      - | 6905 | `/*` |
|      - | 6906 | ` * bool ctype_space(string $text)` |
|      - | 6907 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 6908 | ` * Parameters` |
|      - | 6909 | ` *  $text` |
|      - | 6910 | ` *   The tested string.` |
|      - | 6911 | ` * Return` |
|      - | 6912 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 6913 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 6914 | ` *  and form feed characters.` |
|      - | 6915 | ` */` |
|    603 | 6916 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6917 |  |
|      - | 6918 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6919 | `	int nLen;` |
|    604 | 6920 | `	if( nArg < 1 ){` |
|      - | 6921 | `		/* Missing arguments,return FALSE */` |
|      3 | 6922 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6923 | `		return PH7_OK;` |
|      - | 6924 | `	}` |
|      - | 6925 | `	/* Extract the target string */` |
|    602 | 6926 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    602 | 6927 | `	zEnd = &zIn[nLen];` |
|    602 | 6928 | `	if( nLen < 1 ){` |
|      - | 6929 | `		/* Empty string,return FALSE */` |
|      3 | 6930 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6931 | `		return PH7_OK;` |
|      - | 6932 | `	}` |
|      - | 6933 | `	/* Perform the requested operation */` |
|    328 | 6934 | `	for(;;){` |
|    654 | 6935 | `		if( zIn >= zEnd ){` |
|      - | 6936 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     31 | 6937 | `			ph7_result_bool(pCtx,1);` |
|     31 | 6938 | `			return PH7_OK;` |
|      - | 6939 | `		}` |
|    624 | 6940 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6941 | `			/* UTF-8 stream  */` |
|    ! 0 | 6942 | `			break;` |
|      - | 6943 | `		}` |
|    624 | 6944 | `		if( !SyisSpace(zIn[0]) ){` |
|    570 | 6945 | `			break;` |
|      - | 6946 | `		}` |
|      - | 6947 | `		/* Point to the next character */` |
|     55 | 6948 | `		zIn++;` |
|      1 | 6949 | `	}` |
|      - | 6950 | `	/* The test failed,return FALSE */` |
|    570 | 6951 | `	ph7_result_bool(pCtx,0);` |
|    570 | 6952 | `	return PH7_OK;` |
|    304 | 6953 |  |
|      - | 6954 | `/*` |
|      - | 6955 | ` * bool ctype_lower(string $text)` |
|      - | 6956 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 6957 | ` * Parameters` |
|      - | 6958 | ` *  $text` |
|      - | 6959 | ` *   The tested string.` |
|      - | 6960 | ` * Return` |
|      - | 6961 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 6962 | ` */` |
|     18 | 6963 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6964 |  |
|      - | 6965 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6966 | `	int nLen;` |
|     19 | 6967 | `	if( nArg < 1 ){` |
|      - | 6968 | `		/* Missing arguments,return FALSE */` |
|      3 | 6969 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6970 | `		return PH7_OK;` |
|      - | 6971 | `	}` |
|      - | 6972 | `	/* Extract the target string */` |
|     17 | 6973 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6974 | `	zEnd = &zIn[nLen];` |
|     17 | 6975 | `	if( nLen < 1 ){` |
|      - | 6976 | `		/* Empty string,return FALSE */` |
|      3 | 6977 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6978 | `		return PH7_OK;` |
|      - | 6979 | `	}` |
|      - | 6980 | `	/* Perform the requested operation */` |
|     27 | 6981 | `	for(;;){` |
|     55 | 6982 | `		if( zIn >= zEnd ){` |
|      - | 6983 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 6984 | `			ph7_result_bool(pCtx,1);` |
|      5 | 6985 | `			return PH7_OK;` |
|      - | 6986 | `		}` |
|     51 | 6987 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 6988 | `			break;` |
|      - | 6989 | `		}` |
|      - | 6990 | `		/* Point to the next character */` |
|     41 | 6991 | `		zIn++;` |
|      1 | 6992 | `	}` |
|      - | 6993 | `	/* The test failed,return FALSE */` |
|     11 | 6994 | `	ph7_result_bool(pCtx,0);` |
|     11 | 6995 | `	return PH7_OK;` |
|     10 | 6996 |  |
|      - | 6997 | `/*` |
|      - | 6998 | ` * bool ctype_upper(string $text)` |
|      - | 6999 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 7000 | ` * Parameters` |
|      - | 7001 | ` *  $text` |
|      - | 7002 | ` *   The tested string.` |
|      - | 7003 | ` * Return` |
|      - | 7004 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 7005 | ` */` |
|     18 | 7006 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7007 |  |
|      - | 7008 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7009 | `	int nLen;` |
|     19 | 7010 | `	if( nArg < 1 ){` |
|      - | 7011 | `		/* Missing arguments,return FALSE */` |
|      3 | 7012 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7013 | `		return PH7_OK;` |
|      - | 7014 | `	}` |
|      - | 7015 | `	/* Extract the target string */` |
|     17 | 7016 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7017 | `	zEnd = &zIn[nLen];` |
|     17 | 7018 | `	if( nLen < 1 ){` |
|      - | 7019 | `		/* Empty string,return FALSE */` |
|      3 | 7020 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7021 | `		return PH7_OK;` |
|      - | 7022 | `	}` |
|      - | 7023 | `	/* Perform the requested operation */` |
|     28 | 7024 | `	for(;;){` |
|     57 | 7025 | `		if( zIn >= zEnd ){` |
|      - | 7026 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7027 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7028 | `			return PH7_OK;` |
|      - | 7029 | `		}` |
|     53 | 7030 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 7031 | `			break;` |
|      - | 7032 | `		}` |
|      - | 7033 | `		/* Point to the next character */` |
|     43 | 7034 | `		zIn++;` |
|      1 | 7035 | `	}` |
|      - | 7036 | `	/* The test failed,return FALSE */` |
|     11 | 7037 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7038 | `	return PH7_OK;` |
|     10 | 7039 |  |
|      - | 7040 | `/*` |
|      - | 7041 | ` * Date/Time functions` |
|      - | 7042 | ` * Status:` |
|      - | 7043 | ` *    Devel.` |
|      - | 7044 | ` */` |
|      - | 7045 | `#include <time.h>` |
|      - | 7046 | `#ifdef __WINNT__` |
|      - | 7047 | `/* GetSystemTime() */` |
|      - | 7048 | `#include <Windows.h>` |
|      - | 7049 | `#ifdef _WIN32_WCE` |
|      - | 7050 | `/*` |
|      - | 7051 | `** WindowsCE does not have a localtime() function.  So create a` |
|      - | 7052 | `** substitute.` |
|      - | 7053 | `** Taken from the SQLite3 source tree.` |
|      - | 7054 | `** Status: Public domain` |
|      - | 7055 | `*/` |
|      - | 7056 | `struct tm *__cdecl localtime(const time_t *t)` |
|      - | 7057 |  |
|      - | 7058 | `  static struct tm y;` |
|      - | 7059 | `  FILETIME uTm, lTm;` |
|      - | 7060 | `  SYSTEMTIME pTm;` |
|      - | 7061 | `  ph7_int64 t64;` |
|      - | 7062 | `  t64 = *t;` |
|      - | 7063 | `  t64 = (t64 + 11644473600)*10000000;` |
|      - | 7064 | `  uTm.dwLowDateTime = (DWORD)(t64 & 0xFFFFFFFF);` |
|      - | 7065 | `  uTm.dwHighDateTime= (DWORD)(t64 >> 32);` |
|      - | 7066 | `  FileTimeToLocalFileTime(&uTm,&lTm);` |
|      - | 7067 | `  FileTimeToSystemTime(&lTm,&pTm);` |
|      - | 7068 | `  y.tm_year = pTm.wYear - 1900;` |
|      - | 7069 | `  y.tm_mon = pTm.wMonth - 1;` |
|      - | 7070 | `  y.tm_wday = pTm.wDayOfWeek;` |
|      - | 7071 | `  y.tm_mday = pTm.wDay;` |
|      - | 7072 | `  y.tm_hour = pTm.wHour;` |
|      - | 7073 | `  y.tm_min = pTm.wMinute;` |
|      - | 7074 | `  y.tm_sec = pTm.wSecond;` |
|      - | 7075 | `  return &y;` |
|      - | 7076 |  |
|      - | 7077 | `#endif /*_WIN32_WCE */` |
|      - | 7078 | `#elif defined(__UNIXES__)` |
|      - | 7079 | `#include <sys/time.h>` |
|      - | 7080 | `#endif /* __WINNT__*/` |
|      - | 7081 | ` /*` |
|      - | 7082 | `  * int64 time(void)` |
|      - | 7083 | `  *  Current Unix timestamp` |
|      - | 7084 | `  * Parameters` |
|      - | 7085 | `  *  None.` |
|      - | 7086 | `  * Return` |
|      - | 7087 | `  *  Returns the current time measured in the number of seconds` |
|      - | 7088 | `  *  since the Unix Epoch (January 1 1970 00:00:00 GMT).` |
|      - | 7089 | `  */` |
|      8 | 7090 | `static int PH7_builtin_time(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7091 |  |
|      - | 7092 | `	time_t tt;` |
|      4 | 7093 | `	SXUNUSED(nArg); /* cc warning */` |
|      4 | 7094 | `	SXUNUSED(apArg);` |
|      - | 7095 | `	/* Extract the current time */` |
|      9 | 7096 | `	time(&tt);` |
|      - | 7097 | `	/* Return as 64-bit integer */` |
|      9 | 7098 | `	ph7_result_int64(pCtx,(ph7_int64)tt);` |
|      9 | 7099 | `	return  PH7_OK;` |
|      1 | 7100 |  |
|      - | 7101 | `/*` |
|      - | 7102 | `  * string/float microtime([ bool $get_as_float = false ])` |
|      - | 7103 | `  *  microtime() returns the current Unix timestamp with microseconds.` |
|      - | 7104 | `  * Parameters` |
|      - | 7105 | `  *  $get_as_float` |
|      - | 7106 | `  *   If used and set to TRUE, microtime() will return a float instead of a string` |
|      - | 7107 | `  *   as described in the return values section below.` |
|      - | 7108 | `  * Return` |
|      - | 7109 | `  *  By default, microtime() returns a string in the form "msec sec", where sec` |
|      - | 7110 | `  *  is the current time measured in the number of seconds since the Unix` |
|      - | 7111 | `  *  epoch (0:00:00 January 1, 1970 GMT), and msec is the number of microseconds` |
|      - | 7112 | `  *  that have elapsed since sec expressed in seconds.` |
|      - | 7113 | `  *  If get_as_float is set to TRUE, then microtime() returns a float, which represents` |
|      - | 7114 | `  *  the current time in seconds since the Unix epoch accurate to the nearest microsecond.` |
|      - | 7115 | `  */` |
|     20 | 7116 | `static int PH7_builtin_microtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7117 |  |
|     21 | 7118 | `	int bFloat = 0;` |
|      - | 7119 | `	sytime sTime;` |
|      - | 7120 | `#if defined(__UNIXES__)` |
|      - | 7121 | `	struct timeval tv;` |
|     20 | 7122 | `	gettimeofday(&tv,0);` |
|     20 | 7123 | `	sTime.tm_sec  = (long)tv.tv_sec;` |
|     20 | 7124 | `	sTime.tm_usec = (long)tv.tv_usec;` |
|      - | 7125 | `#else` |
|      - | 7126 | `	time_t tt;` |
|      1 | 7127 | `	time(&tt);` |
|      1 | 7128 | `	sTime.tm_sec  = (long)tt;` |
|      1 | 7129 | `	sTime.tm_usec = (long)(tt%SX_USEC_PER_SEC);` |
|      - | 7130 | `#endif /* __UNIXES__ */` |
|     21 | 7131 | `	if( nArg > 0 ){` |
|     17 | 7132 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|      8 | 7133 | `	}` |
|     21 | 7134 | `	if( bFloat ){` |
|      - | 7135 | `		/* Return as float */` |
|     17 | 7136 | `		ph7_result_double(pCtx,(double)sTime.tm_sec);` |
|      9 | 7137 | `	}else{` |
|      - | 7138 | `		/* Return as string */` |
|      5 | 7139 | `		ph7_result_string_format(pCtx,"%ld %ld",sTime.tm_usec,sTime.tm_sec);` |
|      - | 7140 | `	}` |
|     21 | 7141 | `	return PH7_OK;` |
|      1 | 7142 |  |
|      - | 7143 | `/*` |
|      - | 7144 | ` * array getdate ([ int $timestamp = time() ])` |
|      - | 7145 | ` *  Get date/time information.` |
|      - | 7146 | ` * Parameter` |
|      - | 7147 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|      - | 7148 | ` *     that defaults to the current local time if a timestamp is not given.` |
|      - | 7149 | ` *     In other words, it defaults to the value of time().` |
|      - | 7150 | ` * Returns` |
|      - | 7151 | ` *  Returns an associative array of information related to the timestamp.` |
|      - | 7152 | ` *  Elements from the returned associative array are as follows:` |
|      - | 7153 | ` *   KEY                                                         VALUE` |
|      - | 7154 | ` * ---------                                                    -------` |
|      - | 7155 | ` * "seconds" 	Numeric representation of seconds 	            0 to 59` |
|      - | 7156 | ` * "minutes" 	Numeric representation of minutes 	            0 to 59` |
|      - | 7157 | ` * "hours" 	    Numeric representation of hours 	            0 to 23` |
|      - | 7158 | ` * "mday" 	    Numeric representation of the day of the month 	1 to 31` |
|      - | 7159 | ` * "wday" 	    Numeric representation of the day of the week 	0 (for Sunday) through 6 (for Saturday)` |
|      - | 7160 | ` * "mon" 	    Numeric representation of a month 	            1 through 12` |
|      - | 7161 | ` * "year" 	    A full numeric representation of a year,        4 digits 	Examples: 1999 or 2003` |
|      - | 7162 | ` * "yday" 	    Numeric representation of the day of the year   0 through 365` |
|      - | 7163 | ` * "weekday" 	A full textual representation of the day of the week 	Sunday through Saturday` |
|      - | 7164 | ` * "month" 	    A full textual representation of a month, such as January or March 	January through December` |
|      - | 7165 | ` * 0 	        Seconds since the Unix Epoch, similar to the values returned by time() and used by date().` |
|      - | 7166 | ` * NOTE:` |
|      - | 7167 | ` *   NULL is returned on failure.` |
|      - | 7168 | ` */` |
|      8 | 7169 | `static int PH7_builtin_getdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7170 |  |
|      - | 7171 | `	ph7_value *pValue,*pArray;` |
|      - | 7172 | `	Sytm sTm;` |
|      9 | 7173 | `	if( nArg < 1 ){` |
|      - | 7174 | `#ifdef __WINNT__` |
|      - | 7175 | `		SYSTEMTIME sOS;` |
|      1 | 7176 | `		GetSystemTime(&sOS);` |
|      1 | 7177 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 7178 | `#else` |
|      - | 7179 | `		struct tm *pTm;` |
|      - | 7180 | `		time_t t;` |
|      4 | 7181 | `		time(&t);` |
|      4 | 7182 | `		pTm = localtime(&t);` |
|      4 | 7183 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7184 | `#endif` |
|      3 | 7185 | `	}else{` |
|      - | 7186 | `		/* Use the given timestamp */` |
|      - | 7187 | `		time_t t;` |
|      - | 7188 | `		struct tm *pTm;` |
|      - | 7189 | `#ifdef __WINNT__` |
|      - | 7190 | `#ifdef _MSC_VER` |
|      - | 7191 | `#if _MSC_VER >= 1400 /* Visual Studio 2005 and up */` |
|      - | 7192 | `#pragma warning(disable:4996) /* _CRT_SECURE...*/` |
|      - | 7193 | `#endif` |
|      - | 7194 | `#endif` |
|      - | 7195 | `#endif` |
|      5 | 7196 | `		if( ph7_value_is_int(apArg[0]) ){` |
|      5 | 7197 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|      5 | 7198 | `			pTm = localtime(&t);` |
|      5 | 7199 | `			if( pTm == 0 ){` |
|    ! 0 | 7200 | `				time(&t);` |
|    ! 0 | 7201 | `			}` |
|      3 | 7202 | `		}else{` |
|    ! 0 | 7203 | `			time(&t);` |
|      - | 7204 | `		}` |
|      5 | 7205 | `		pTm = localtime(&t);` |
|      5 | 7206 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7207 | `	}` |
|      - | 7208 | `	/* Element value */` |
|      9 | 7209 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      9 | 7210 | `	if( pValue == 0 ){` |
|      - | 7211 | `		/* Return NULL */` |
|    ! 0 | 7212 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7213 | `		return PH7_OK;` |
|      - | 7214 | `	}` |
|      - | 7215 | `	/* Create a new array */` |
|      9 | 7216 | `	pArray = ph7_context_new_array(pCtx);` |
|      9 | 7217 | `	if( pArray == 0 ){` |
|      - | 7218 | `		/* Return NULL */` |
|    ! 0 | 7219 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7220 | `		return PH7_OK;` |
|      - | 7221 | `	}` |
|      - | 7222 | `	/* Fill the array */` |
|      - | 7223 | `	/* Seconds */` |
|      9 | 7224 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|      9 | 7225 | `	ph7_array_add_strkey_elem(pArray,"seconds",pValue);` |
|      - | 7226 | `	/* Minutes */` |
|      9 | 7227 | `	ph7_value_int(pValue,sTm.tm_min);` |
|      9 | 7228 | `	ph7_array_add_strkey_elem(pArray,"minutes",pValue);` |
|      - | 7229 | `	/* Hours */` |
|      9 | 7230 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|      9 | 7231 | `	ph7_array_add_strkey_elem(pArray,"hours",pValue);` |
|      - | 7232 | `	/* mday */` |
|      9 | 7233 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|      9 | 7234 | `	ph7_array_add_strkey_elem(pArray,"mday",pValue);` |
|      - | 7235 | `	/* wday */` |
|      9 | 7236 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|      9 | 7237 | `	ph7_array_add_strkey_elem(pArray,"wday",pValue);` |
|      - | 7238 | `	/* mon */` |
|      9 | 7239 | `	ph7_value_int(pValue,sTm.tm_mon+1);` |
|      9 | 7240 | `	ph7_array_add_strkey_elem(pArray,"mon",pValue);` |
|      - | 7241 | `	/* year */` |
|      9 | 7242 | `	ph7_value_int(pValue,sTm.tm_year);` |
|      9 | 7243 | `	ph7_array_add_strkey_elem(pArray,"year",pValue);` |
|      - | 7244 | `	/* yday */` |
|      9 | 7245 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|      9 | 7246 | `	ph7_array_add_strkey_elem(pArray,"yday",pValue);` |
|      - | 7247 | `	/* Weekday */` |
|      9 | 7248 | `	ph7_value_string(pValue,SyTimeGetDay(sTm.tm_wday),-1);` |
|      9 | 7249 | `	ph7_array_add_strkey_elem(pArray,"weekday",pValue);` |
|      - | 7250 | `	/* Month */` |
|      9 | 7251 | `	ph7_value_reset_string_cursor(pValue);` |
|      9 | 7252 | `	ph7_value_string(pValue,SyTimeGetMonth(sTm.tm_mon),-1);` |
|      9 | 7253 | `	ph7_array_add_strkey_elem(pArray,"month",pValue);` |
|      - | 7254 | `	/* Seconds since the epoch */` |
|      9 | 7255 | `	ph7_value_int64(pValue,(ph7_int64)time(0));` |
|      9 | 7256 | `	ph7_array_add_intkey_elem(pArray,0 /* Index zero */,pValue);` |
|      - | 7257 | `	/* Return the freshly created array */` |
|      9 | 7258 | `	ph7_result_value(pCtx,pArray);` |
|      9 | 7259 | `	return PH7_OK;` |
|      5 | 7260 |  |
|      - | 7261 | `/*` |
|      - | 7262 | ` * mixed gettimeofday([ bool $return_float = false ] )` |
|      - | 7263 | ` *   Returns an associative array containing the data returned from the system call.` |
|      - | 7264 | ` * Parameters` |
|      - | 7265 | ` *  $return_float` |
|      - | 7266 | ` *   When set to TRUE, a float instead of an array is returned.` |
|      - | 7267 | ` * Return` |
|      - | 7268 | ` *   By default an array is returned. If return_float is set, then` |
|      - | 7269 | ` *   a float is returned.` |
|      - | 7270 | ` */` |
|      4 | 7271 | `static int PH7_builtin_gettimeofday(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7272 |  |
|      5 | 7273 | `	int bFloat = 0;` |
|      - | 7274 | `	sytime sTime;` |
|      - | 7275 | `#if defined(__UNIXES__)` |
|      - | 7276 | `	struct timeval tv;` |
|      4 | 7277 | `	gettimeofday(&tv,0);` |
|      4 | 7278 | `	sTime.tm_sec  = (long)tv.tv_sec;` |
|      4 | 7279 | `	sTime.tm_usec = (long)tv.tv_usec;` |
|      - | 7280 | `#else` |
|      - | 7281 | `	time_t tt;` |
|      1 | 7282 | `	time(&tt);` |
|      1 | 7283 | `	sTime.tm_sec  = (long)tt;` |
|      1 | 7284 | `	sTime.tm_usec = (long)(tt%SX_USEC_PER_SEC);` |
|      - | 7285 | `#endif /* __UNIXES__ */` |
|      5 | 7286 | `	if( nArg > 0 ){` |
|      5 | 7287 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|      2 | 7288 | `	}` |
|      5 | 7289 | `	if( bFloat ){` |
|      - | 7290 | `		/* Return as float */` |
|      3 | 7291 | `		ph7_result_double(pCtx,(double)sTime.tm_sec);` |
|      2 | 7292 | `	}else{` |
|      - | 7293 | `		/* Return an associative array */` |
|      - | 7294 | `		ph7_value *pValue,*pArray;` |
|      - | 7295 | `		/* Create a new array */` |
|      3 | 7296 | `		pArray = ph7_context_new_array(pCtx);` |
|      - | 7297 | `		/* Element value */` |
|      3 | 7298 | `		pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 7299 | `		if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 7300 | `			/* Return NULL */` |
|    ! 0 | 7301 | `			ph7_result_null(pCtx);` |
|    ! 0 | 7302 | `			return PH7_OK;` |
|      - | 7303 | `		}` |
|      - | 7304 | `		/* Fill the array */` |
|      - | 7305 | `		/* sec */` |
|      3 | 7306 | `		ph7_value_int64(pValue,sTime.tm_sec);` |
|      3 | 7307 | `		ph7_array_add_strkey_elem(pArray,"sec",pValue);` |
|      - | 7308 | `		/* usec */` |
|      3 | 7309 | `		ph7_value_int64(pValue,sTime.tm_usec);` |
|      3 | 7310 | `		ph7_array_add_strkey_elem(pArray,"usec",pValue);` |
|      - | 7311 | `		/* Return the array */` |
|      3 | 7312 | `		ph7_result_value(pCtx,pArray);` |
|      - | 7313 | `	}` |
|      5 | 7314 | `	return PH7_OK;` |
|      3 | 7315 |  |
|      - | 7316 | `/* Check if the given year is leap or not */` |
|      - | 7317 | `#define IS_LEAP_YEAR(YEAR)	(YEAR % 400 ? ( YEAR % 100 ? ( YEAR % 4 ? 0 : 1 ) : 0 ) : 1)` |
|      - | 7318 | `/* ISO-8601 numeric representation of the day of the week */` |
|      - | 7319 | `static const int aISO8601[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|      - | 7320 | `/*` |
|      - | 7321 | ` * Format a given date string.` |
|      - | 7322 | ` * Supported format: (Taken from PHP online docs)` |
|      - | 7323 | ` * character 	Description` |
|      - | 7324 | ` * d          Day of the month` |
|      - | 7325 | ` * D          A textual representation of a days` |
|      - | 7326 | ` * j          Day of the month without leading zeros` |
|      - | 7327 | ` * l          A full textual representation of the day of the week` |
|      - | 7328 | ` * N          ISO-8601 numeric representation of the day of the week` |
|      - | 7329 | ` * w          Numeric representation of the day of the week` |
|      - | 7330 | ` * z          The day of the year (starting from 0)` |
|      - | 7331 | ` * F          A full textual representation of a month, such as January or March` |
|      - | 7332 | ` * m          Numeric representation of a month, with leading zeros 	01 through 12` |
|      - | 7333 | ` * M          A short textual representation of a month, three letters 	Jan through Dec` |
|      - | 7334 | ` * n          Numeric representation of a month, without leading zeros 	1 through 12` |
|      - | 7335 | ` * t          Number of days in the given month 	28 through 31` |
|      - | 7336 | ` * L          Whether it's a leap year 	1 if it is a leap year, 0 otherwise.` |
|      - | 7337 | ` * o          ISO-8601 year number. This has the same value as Y, except that if the ISO week number` |
|      - | 7338 | ` *            (W) belongs to the previous or next year, that year is used instead. (added in PHP 5.1.0) Examples: 1999 or 2003` |
|      - | 7339 | ` * Y          A full numeric representation of a year, 4 digits 	Examples: 1999 or 2003` |
|      - | 7340 | ` * y          A two digit representation of a year 	Examples: 99 or 03` |
|      - | 7341 | ` * a          Lowercase Ante meridiem and Post meridiem 	am or pm` |
|      - | 7342 | ` * A          Uppercase Ante meridiem and Post meridiem 	AM or PM` |
|      - | 7343 | ` * g          12-hour format of an hour without leading zeros 	1 through 12` |
|      - | 7344 | ` * G          24-hour format of an hour without leading zeros 	0 through 23` |
|      - | 7345 | ` * h          12-hour format of an hour with leading zeros 	01 through 12` |
|      - | 7346 | ` * H          24-hour format of an hour with leading zeros 	00 through 23` |
|      - | 7347 | ` * i          Minutes with leading zeros 	00 to 59` |
|      - | 7348 | ` * s          Seconds, with leading zeros 	00 through 59` |
|      - | 7349 | ` * u          Microseconds Example: 654321` |
|      - | 7350 | ` * e          Timezone identifier 	Examples: UTC, GMT, Atlantic/Azores` |
|      - | 7351 | ` * I          (capital i) Whether or not the date is in daylight saving time 	1 if Daylight Saving Time, 0 otherwise.` |
|      - | 7352 | ` * r          RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 +0200` |
|      - | 7353 | ` * U          Seconds since the Unix Epoch (January 1 1970 00:00:00 GMT)` |
|      - | 7354 | ` * S          English ordinal suffix for the day of the month, 2 characters` |
|      - | 7355 | ` * O          Difference to Greenwich time (GMT) in hours` |
|      - | 7356 | ` * Z          Timezone offset in seconds. The offset for timezones west of UTC is always negative, and for those` |
|      - | 7357 | ` *            east of UTC is always positive.` |
|      - | 7358 | ` * c         ISO 8601 date` |
|      - | 7359 | ` */` |
|     46 | 7360 | `static sxi32 DateFormat(ph7_context *pCtx,const char *zIn,int nLen,Sytm *pTm)` |
|      1 | 7361 |  |
|     47 | 7362 | `	const char *zEnd = &zIn[nLen];` |
|      - | 7363 | `	const char *zCur;` |
|      - | 7364 | `	/* Start the format process */` |
|     78 | 7365 | `	for(;;){` |
|    157 | 7366 | `		if( zIn >= zEnd ){` |
|      - | 7367 | `			/* No more input to process */` |
|     47 | 7368 | `			break;` |
|      - | 7369 | `		}` |
|    111 | 7370 | `		switch(zIn[0]){` |
|      7 | 7371 | `		case 'd':` |
|      - | 7372 | `			/* Day of the month, 2 digits with leading zeros */` |
|     15 | 7373 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mday);` |
|     15 | 7374 | `			break;` |
|    ! 0 | 7375 | `		case 'D':` |
|      - | 7376 | `			/*A textual representation of a day, three letters*/` |
|    ! 0 | 7377 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|    ! 0 | 7378 | `			ph7_result_string(pCtx,zCur,3);` |
|    ! 0 | 7379 | `			break;` |
|    ! 0 | 7380 | `		case 'j':` |
|      - | 7381 | `			/*	Day of the month without leading zeros */` |
|    ! 0 | 7382 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mday);` |
|    ! 0 | 7383 | `			break;` |
|      2 | 7384 | `		case 'l':` |
|      - | 7385 | `			/* A full textual representation of the day of the week */` |
|      5 | 7386 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|      5 | 7387 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|      5 | 7388 | `			break;` |
|    ! 0 | 7389 | `		case 'N':{` |
|      - | 7390 | `			/* ISO-8601 numeric representation of the day of the week */` |
|    ! 0 | 7391 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|    ! 0 | 7392 | `			break;` |
|      - | 7393 | `				 }` |
|    ! 0 | 7394 | `		case 'w':` |
|      - | 7395 | `			/*Numeric representation of the day of the week*/` |
|    ! 0 | 7396 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|    ! 0 | 7397 | `			break;` |
|    ! 0 | 7398 | `		case 'z':` |
|      - | 7399 | `			/*The day of the year*/` |
|    ! 0 | 7400 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_yday);` |
|    ! 0 | 7401 | `			break;` |
|      2 | 7402 | `		case 'F':` |
|      - | 7403 | `			/*A full textual representation of a month, such as January or March*/` |
|      5 | 7404 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|      5 | 7405 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|      5 | 7406 | `			break;` |
|      7 | 7407 | `		case 'm':` |
|      - | 7408 | `			/*Numeric representation of a month, with leading zeros*/` |
|     15 | 7409 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|     15 | 7410 | `			break;` |
|    ! 0 | 7411 | `		case 'M':` |
|      - | 7412 | `			/*A short textual representation of a month, three letters*/` |
|    ! 0 | 7413 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|    ! 0 | 7414 | `			ph7_result_string(pCtx,zCur,3);` |
|    ! 0 | 7415 | `			break;` |
|    ! 0 | 7416 | `		case 'n':` |
|      - | 7417 | `			/*Numeric representation of a month, without leading zeros*/` |
|    ! 0 | 7418 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mon + 1);` |
|    ! 0 | 7419 | `			break;` |
|    ! 0 | 7420 | `		case 't':{` |
|      - | 7421 | `			static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|    ! 0 | 7422 | `			int nDays = aMonDays[pTm->tm_mon % 12 ];` |
|    ! 0 | 7423 | `			if( pTm->tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(pTm->tm_year) ){` |
|    ! 0 | 7424 | `				nDays = 28;` |
|    ! 0 | 7425 | `			}` |
|      - | 7426 | `			/*Number of days in the given month*/` |
|    ! 0 | 7427 | `			ph7_result_string_format(pCtx,"%d",nDays);` |
|    ! 0 | 7428 | `			break;` |
|      - | 7429 | `				 }` |
|    ! 0 | 7430 | `		case 'L':{` |
|    ! 0 | 7431 | `			int isLeap = IS_LEAP_YEAR(pTm->tm_year);` |
|      - | 7432 | `			/* Whether it's a leap year */` |
|    ! 0 | 7433 | `			ph7_result_string_format(pCtx,"%d",isLeap);` |
|    ! 0 | 7434 | `			break;` |
|      - | 7435 | `				 }` |
|    ! 0 | 7436 | `		case 'o':` |
|      - | 7437 | `			/* ISO-8601 year number.*/` |
|    ! 0 | 7438 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|    ! 0 | 7439 | `			break;` |
|      9 | 7440 | `		case 'Y':` |
|      - | 7441 | `			/*	A full numeric representation of a year, 4 digits */` |
|     19 | 7442 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|     19 | 7443 | `			break;` |
|    ! 0 | 7444 | `		case 'y':` |
|      - | 7445 | `			/*A two digit representation of a year*/` |
|    ! 0 | 7446 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_year%100);` |
|    ! 0 | 7447 | `			break;` |
|    ! 0 | 7448 | `		case 'a':` |
|      - | 7449 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7450 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",2);` |
|    ! 0 | 7451 | `			break;` |
|    ! 0 | 7452 | `		case 'A':` |
|      - | 7453 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7454 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",2);` |
|    ! 0 | 7455 | `			break;` |
|    ! 0 | 7456 | `		case 'g':` |
|      - | 7457 | `			/*	12-hour format of an hour without leading zeros*/` |
|    ! 0 | 7458 | `			ph7_result_string_format(pCtx,"%d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7459 | `			break;` |
|    ! 0 | 7460 | `		case 'G':` |
|      - | 7461 | `			/* 24-hour format of an hour without leading zeros */` |
|    ! 0 | 7462 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_hour);` |
|    ! 0 | 7463 | `			break;` |
|    ! 0 | 7464 | `		case 'h':` |
|      - | 7465 | `			/* 12-hour format of an hour with leading zeros */` |
|    ! 0 | 7466 | `			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7467 | `			break;` |
|      3 | 7468 | `		case 'H':` |
|      - | 7469 | `			/*	24-hour format of an hour with leading zeros */` |
|      7 | 7470 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|      7 | 7471 | `			break;` |
|      3 | 7472 | `		case 'i':` |
|      - | 7473 | `			/* 	Minutes with leading zeros */` |
|      7 | 7474 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|      7 | 7475 | `			break;` |
|      3 | 7476 | `		case 's':` |
|      - | 7477 | `			/* 	second with leading zeros */` |
|      7 | 7478 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|      7 | 7479 | `			break;` |
|    ! 0 | 7480 | `		case 'u':` |
|      - | 7481 | `			/* 	Microseconds */` |
|    ! 0 | 7482 | `			ph7_result_string_format(pCtx,"%u",pTm->tm_sec * SX_USEC_PER_SEC);` |
|    ! 0 | 7483 | `			break;` |
|    ! 0 | 7484 | `		case 'S':{` |
|      - | 7485 | `			/* English ordinal suffix for the day of the month, 2 characters */` |
|      - | 7486 | `			static const char zSuffix[] = "thstndrdthththththth";` |
|    ! 0 | 7487 | `			int v = pTm->tm_mday;` |
|    ! 0 | 7488 | `			ph7_result_string(pCtx,&zSuffix[2 * (int)(v / 10 % 10 != 1 ? v % 10 : 0)],(int)sizeof(char) * 2);` |
|    ! 0 | 7489 | `			break;` |
|      - | 7490 | `				 }` |
|    ! 0 | 7491 | `		case 'e':` |
|      - | 7492 | `			/* 	Timezone identifier */` |
|    ! 0 | 7493 | `			zCur = pTm->tm_zone;` |
|    ! 0 | 7494 | `			if( zCur == 0 ){` |
|      - | 7495 | `				/* Assume GMT */` |
|    ! 0 | 7496 | `				zCur = "GMT";` |
|    ! 0 | 7497 | `			}` |
|    ! 0 | 7498 | `			ph7_result_string(pCtx,zCur,-1);` |
|    ! 0 | 7499 | `			break;` |
|    ! 0 | 7500 | `		case 'I':` |
|      - | 7501 | `			/* Whether or not the date is in daylight saving time */` |
|      - | 7502 | `#ifdef __WINNT__` |
|      - | 7503 | `#ifdef _MSC_VER` |
|      - | 7504 | `#ifndef _WIN32_WCE` |
|    ! 0 | 7505 | `			_get_daylight(&pTm->tm_isdst);` |
|      - | 7506 | `#endif` |
|      - | 7507 | `#endif` |
|      - | 7508 | `#endif` |
|    ! 0 | 7509 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_isdst == 1);` |
|    ! 0 | 7510 | `			break;` |
|    ! 0 | 7511 | `		case 'r':` |
|      - | 7512 | `			/* RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 */` |
|    ! 0 | 7513 | `			ph7_result_string_format(pCtx,"%.3s, %02d %.3s %4d %02d:%02d:%02d",` |
|    ! 0 | 7514 | `				SyTimeGetDay(pTm->tm_wday),` |
|    ! 0 | 7515 | `				pTm->tm_mday,` |
|    ! 0 | 7516 | `				SyTimeGetMonth(pTm->tm_mon),` |
|    ! 0 | 7517 | `				pTm->tm_year,` |
|    ! 0 | 7518 | `				pTm->tm_hour,` |
|    ! 0 | 7519 | `				pTm->tm_min,` |
|    ! 0 | 7520 | `				pTm->tm_sec` |
|      - | 7521 | `				);` |
|    ! 0 | 7522 | `			break;` |
|    ! 0 | 7523 | `		case 'U':{` |
|      - | 7524 | `			time_t tt;` |
|      - | 7525 | `			/* Seconds since the Unix Epoch */` |
|    ! 0 | 7526 | `			time(&tt);` |
|    ! 0 | 7527 | `			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);` |
|    ! 0 | 7528 | `			break;` |
|      - | 7529 | `				 }` |
|    ! 0 | 7530 | `		case 'O':` |
|      - | 7531 | `		case 'P':` |
|      - | 7532 | `			/* Difference to Greenwich time (GMT) in hours */` |
|    ! 0 | 7533 | `			ph7_result_string_format(pCtx,"%+05d",pTm->tm_gmtoff);` |
|    ! 0 | 7534 | `			break;` |
|    ! 0 | 7535 | `		case 'Z':` |
|      - | 7536 | `			/* Timezone offset in seconds. The offset for timezones west of UTC` |
|      - | 7537 | `			 * is always negative, and for those east of UTC is always positive.` |
|      - | 7538 | `			 */` |
|    ! 0 | 7539 | `			ph7_result_string_format(pCtx,"%+05d",pTm->tm_gmtoff);` |
|    ! 0 | 7540 | `			break;` |
|      1 | 7541 | `		case 'c':` |
|      - | 7542 | `			/* 	ISO 8601 date */` |
|      4 | 7543 | `			ph7_result_string_format(pCtx,"%4d-%02d-%02dT%02d:%02d:%02d%+05d",` |
|      1 | 7544 | `				pTm->tm_year,` |
|      2 | 7545 | `				pTm->tm_mon+1,` |
|      1 | 7546 | `				pTm->tm_mday,` |
|      1 | 7547 | `				pTm->tm_hour,` |
|      1 | 7548 | `				pTm->tm_min,` |
|      1 | 7549 | `				pTm->tm_sec,` |
|      1 | 7550 | `				pTm->tm_gmtoff` |
|      - | 7551 | `				);` |
|      3 | 7552 | `			break;` |
|      1 | 7553 | `		case '\\':` |
|      3 | 7554 | `			zIn++;` |
|      - | 7555 | `			/* Expand verbatim */` |
|      3 | 7556 | `			if( zIn < zEnd ){` |
|      3 | 7557 | `				ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|      1 | 7558 | `			}` |
|      3 | 7559 | `			break;` |
|     17 | 7560 | `		default:` |
|      - | 7561 | `			/* Unknown format specifer,expand verbatim */` |
|     35 | 7562 | `			ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|     34 | 7563 | `			break;` |
|      - | 7564 | `		}` |
|      - | 7565 | `		/* Point to the next character */` |
|    111 | 7566 | `		zIn++;` |
|      1 | 7567 | `	}` |
|     47 | 7568 | `	return SXRET_OK;` |
|      1 | 7569 |  |
|      - | 7570 | `/*` |
|      - | 7571 | ` * PH7 implementation of the strftime() function.` |
|      - | 7572 | ` * The following formats are supported:` |
|      - | 7573 | ` * %a 	An abbreviated textual representation of the day` |
|      - | 7574 | ` * %A 	A full textual representation of the day` |
|      - | 7575 | ` * %d 	Two-digit day of the month (with leading zeros)` |
|      - | 7576 | ` * %e 	Day of the month, with a space preceding single digits.` |
|      - | 7577 | ` * %j 	Day of the year, 3 digits with leading zeros` |
|      - | 7578 | ` * %u 	ISO-8601 numeric representation of the day of the week 	1 (for Monday) though 7 (for Sunday)` |
|      - | 7579 | ` * %w 	Numeric representation of the day of the week 0 (for Sunday) through 6 (for Saturday)` |
|      - | 7580 | ` * %U 	Week number of the given year, starting with the first Sunday as the first week` |
|      - | 7581 | ` * %V 	ISO-8601:1988 week number of the given year, starting with the first week of the year with at least` |
|      - | 7582 | ` *   4 weekdays, with Monday being the start of the week.` |
|      - | 7583 | ` * %W 	A numeric representation of the week of the year` |
|      - | 7584 | ` * %b 	Abbreviated month name, based on the locale` |
|      - | 7585 | ` * %B 	Full month name, based on the locale` |
|      - | 7586 | ` * %h 	Abbreviated month name, based on the locale (an alias of %b)` |
|      - | 7587 | ` * %m 	Two digit representation of the month` |
|      - | 7588 | ` * %C 	Two digit representation of the century (year divided by 100, truncated to an integer)` |
|      - | 7589 | ` * %g 	Two digit representation of the year going by ISO-8601:1988 standards (see %V)` |
|      - | 7590 | ` * %G 	The full four-digit version of %g` |
|      - | 7591 | ` * %y 	Two digit representation of the year` |
|      - | 7592 | ` * %Y 	Four digit representation for the year` |
|      - | 7593 | ` * %H 	Two digit representation of the hour in 24-hour format` |
|      - | 7594 | ` * %I 	Two digit representation of the hour in 12-hour format` |
|      - | 7595 | ` * %l (lower-case 'L') 	Hour in 12-hour format, with a space preceeding single digits` |
|      - | 7596 | ` * %M 	Two digit representation of the minute` |
|      - | 7597 | ` * %p 	UPPER-CASE 'AM' or 'PM' based on the given time` |
|      - | 7598 | ` * %P 	lower-case 'am' or 'pm' based on the given time` |
|      - | 7599 | ` * %r 	Same as "%I:%M:%S %p"` |
|      - | 7600 | ` * %R 	Same as "%H:%M"` |
|      - | 7601 | ` * %S 	Two digit representation of the second` |
|      - | 7602 | ` * %T 	Same as "%H:%M:%S"` |
|      - | 7603 | ` * %X 	Preferred time representation based on locale, without the date` |
|      - | 7604 | ` * %z 	Either the time zone offset from UTC or the abbreviation` |
|      - | 7605 | ` * %Z 	The time zone offset/abbreviation option NOT given by %z` |
|      - | 7606 | ` * %c 	Preferred date and time stamp based on local` |
|      - | 7607 | ` * %D 	Same as "%m/%d/%y"` |
|      - | 7608 | ` * %F 	Same as "%Y-%m-%d"` |
|      - | 7609 | ` * %s 	Unix Epoch Time timestamp (same as the time() function)` |
|      - | 7610 | ` * %x 	Preferred date representation based on locale, without the time` |
|      - | 7611 | ` * %n 	A newline character ("\n")` |
|      - | 7612 | ` * %t 	A Tab character ("\t")` |
|      - | 7613 | ` * %% 	A literal percentage character ("%")` |
|      - | 7614 | ` */` |
|     16 | 7615 | `static int PH7_Strftime(` |
|      - | 7616 | `	ph7_context *pCtx,  /* Call context */` |
|      - | 7617 | `	const char *zIn,    /* Input string */` |
|      - | 7618 | `	int nLen,           /* Input length */` |
|      - | 7619 | `	Sytm *pTm           /* Parse of the given time */` |
|      - | 7620 | `	)` |
|      1 | 7621 |  |
|     17 | 7622 | `	const char *zCur,*zEnd = &zIn[nLen];` |
|      - | 7623 | `	int c;` |
|      - | 7624 | `	/* Start the format process */` |
|     18 | 7625 | `	for(;;){` |
|     37 | 7626 | `		zCur = zIn;` |
|     41 | 7627 | `		while(zIn < zEnd && zIn[0] != '%' ){` |
|      5 | 7628 | `			zIn++;` |
|      1 | 7629 | `		}` |
|     37 | 7630 | `		if( zIn > zCur ){` |
|      - | 7631 | `			/* Consume input verbatim */` |
|      5 | 7632 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 7633 | `		}` |
|     37 | 7634 | `		zIn++; /* Jump the percent sign */` |
|     37 | 7635 | `		if( zIn >= zEnd ){` |
|      - | 7636 | `			/* No more input to process */` |
|     17 | 7637 | `			break;` |
|      - | 7638 | `		}` |
|     21 | 7639 | `		c = zIn[0];` |
|      - | 7640 | `		/* Act according to the current specifer */` |
|     21 | 7641 | `		switch(c){` |
|    ! 0 | 7642 | `		case '%':` |
|      - | 7643 | `			/* A literal percentage character ("%") */` |
|    ! 0 | 7644 | `			ph7_result_string(pCtx,"%",(int)sizeof(char));` |
|    ! 0 | 7645 | `			break;` |
|    ! 0 | 7646 | `		case 't':` |
|      - | 7647 | `			/* A Tab character */` |
|    ! 0 | 7648 | `			ph7_result_string(pCtx,"\t",(int)sizeof(char));` |
|    ! 0 | 7649 | `			break;` |
|    ! 0 | 7650 | `		case 'n':` |
|      - | 7651 | `			/* A newline character */` |
|    ! 0 | 7652 | `			ph7_result_string(pCtx,"\n",(int)sizeof(char));` |
|    ! 0 | 7653 | `			break;` |
|      1 | 7654 | `		case 'a':` |
|      - | 7655 | `			/* An abbreviated textual representation of the day */` |
|      3 | 7656 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),(int)sizeof(char)*3);` |
|      3 | 7657 | `			break;` |
|    ! 0 | 7658 | `		case 'A':` |
|      - | 7659 | `			/* A full textual representation of the day */` |
|    ! 0 | 7660 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),-1/*Compute length automatically*/);` |
|    ! 0 | 7661 | `			break;` |
|    ! 0 | 7662 | `		case 'e':` |
|      - | 7663 | `			/* Day of the month, 2 digits with leading space for single digit*/` |
|    ! 0 | 7664 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_mday);` |
|    ! 0 | 7665 | `			break;` |
|      2 | 7666 | `		case 'd':` |
|      - | 7667 | `			/* Two-digit day of the month (with leading zeros) */` |
|      5 | 7668 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon+1);` |
|      5 | 7669 | `			break;` |
|    ! 0 | 7670 | `		case 'j':` |
|      - | 7671 | `			/*The day of the year,3 digits with leading zeros*/` |
|    ! 0 | 7672 | `			ph7_result_string_format(pCtx,"%03d",pTm->tm_yday);` |
|    ! 0 | 7673 | `			break;` |
|    ! 0 | 7674 | `		case 'u':` |
|      - | 7675 | `			/* ISO-8601 numeric representation of the day of the week */` |
|    ! 0 | 7676 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|    ! 0 | 7677 | `			break;` |
|    ! 0 | 7678 | `		case 'w':` |
|      - | 7679 | `			/* Numeric representation of the day of the week */` |
|    ! 0 | 7680 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|    ! 0 | 7681 | `			break;` |
|    ! 0 | 7682 | `		case 'b':` |
|      - | 7683 | `		case 'h':` |
|      - | 7684 | `			/*A short textual representation of a month, three letters (Not based on locale)*/` |
|    ! 0 | 7685 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),(int)sizeof(char)*3);` |
|    ! 0 | 7686 | `			break;` |
|    ! 0 | 7687 | `		case 'B':` |
|      - | 7688 | `			/* Full month name (Not based on locale) */` |
|    ! 0 | 7689 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),-1/*Compute length automatically*/);` |
|    ! 0 | 7690 | `			break;` |
|      2 | 7691 | `		case 'm':` |
|      - | 7692 | `			/*Numeric representation of a month, with leading zeros*/` |
|      5 | 7693 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|      5 | 7694 | `			break;` |
|    ! 0 | 7695 | `		case 'C':` |
|      - | 7696 | `			/* Two digit representation of the century */` |
|    ! 0 | 7697 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year/100);` |
|    ! 0 | 7698 | `			break;` |
|    ! 0 | 7699 | `		case 'y':` |
|      - | 7700 | `		case 'g':` |
|      - | 7701 | `			/* Two digit representation of the year */` |
|    ! 0 | 7702 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year%100);` |
|    ! 0 | 7703 | `			break;` |
|      2 | 7704 | `		case 'Y':` |
|      - | 7705 | `		case 'G':` |
|      - | 7706 | `			/* Four digit representation of the year */` |
|      5 | 7707 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|      5 | 7708 | `			break;` |
|    ! 0 | 7709 | `		case 'I':` |
|      - | 7710 | `			/* 12-hour format of an hour with leading zeros */` |
|    ! 0 | 7711 | `			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7712 | `			break;` |
|    ! 0 | 7713 | `		case 'l':` |
|      - | 7714 | `			/* 12-hour format of an hour with leading space */` |
|    ! 0 | 7715 | `			ph7_result_string_format(pCtx,"%2d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7716 | `			break;` |
|      1 | 7717 | `		case 'H':` |
|      - | 7718 | `			/* 24-hour format of an hour with leading zeros */` |
|      3 | 7719 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|      3 | 7720 | `			break;` |
|      1 | 7721 | `		case 'M':` |
|      - | 7722 | `			/* Minutes with leading zeros */` |
|      3 | 7723 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|      3 | 7724 | `			break;` |
|    ! 0 | 7725 | `		case 'S':` |
|      - | 7726 | `			/* Seconds with leading zeros */` |
|    ! 0 | 7727 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|    ! 0 | 7728 | `			break;` |
|    ! 0 | 7729 | `		case 'z':` |
|      - | 7730 | `		case 'Z':` |
|      - | 7731 | `			/* 	Timezone identifier */` |
|    ! 0 | 7732 | `			zCur = pTm->tm_zone;` |
|    ! 0 | 7733 | `			if( zCur == 0 ){` |
|      - | 7734 | `				/* Assume GMT */` |
|    ! 0 | 7735 | `				zCur = "GMT";` |
|    ! 0 | 7736 | `			}` |
|    ! 0 | 7737 | `			ph7_result_string(pCtx,zCur,-1);` |
|    ! 0 | 7738 | `			break;` |
|    ! 0 | 7739 | `		case 'T':` |
|      - | 7740 | `		case 'X':` |
|      - | 7741 | `			/* Same as "%H:%M:%S" */` |
|    ! 0 | 7742 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d",pTm->tm_hour,pTm->tm_min,pTm->tm_sec);` |
|    ! 0 | 7743 | `			break;` |
|    ! 0 | 7744 | `		case 'R':` |
|      - | 7745 | `			/* Same as "%H:%M" */` |
|    ! 0 | 7746 | `			ph7_result_string_format(pCtx,"%02d:%02d",pTm->tm_hour,pTm->tm_min);` |
|    ! 0 | 7747 | `			break;` |
|    ! 0 | 7748 | `		case 'P':` |
|      - | 7749 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7750 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",(int)sizeof(char)*2);` |
|    ! 0 | 7751 | `			break;` |
|    ! 0 | 7752 | `		case 'p':` |
|      - | 7753 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7754 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",(int)sizeof(char)*2);` |
|    ! 0 | 7755 | `			break;` |
|    ! 0 | 7756 | `		case 'r':` |
|      - | 7757 | `			/* Same as "%I:%M:%S %p" */` |
|    ! 0 | 7758 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d %s",` |
|    ! 0 | 7759 | `				1+(pTm->tm_hour%12),` |
|    ! 0 | 7760 | `				pTm->tm_min,` |
|    ! 0 | 7761 | `				pTm->tm_sec,` |
|    ! 0 | 7762 | `				pTm->tm_hour > 12 ? "PM" : "AM"` |
|      - | 7763 | `				);` |
|    ! 0 | 7764 | `			break;` |
|      1 | 7765 | `		case 'D':` |
|      - | 7766 | `		case 'x':` |
|      - | 7767 | `			/* Same as "%m/%d/%y" */` |
|      4 | 7768 | `			ph7_result_string_format(pCtx,"%02d/%02d/%02d",` |
|      2 | 7769 | `				pTm->tm_mon+1,` |
|      1 | 7770 | `				pTm->tm_mday,` |
|      2 | 7771 | `				pTm->tm_year%100` |
|      - | 7772 | `				);` |
|      3 | 7773 | `			break;` |
|    ! 0 | 7774 | `		case 'F':` |
|      - | 7775 | `			/* Same as "%Y-%m-%d" */` |
|    ! 0 | 7776 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d",` |
|    ! 0 | 7777 | `				pTm->tm_year,` |
|    ! 0 | 7778 | `				pTm->tm_mon+1,` |
|    ! 0 | 7779 | `				pTm->tm_mday` |
|      - | 7780 | `				);` |
|    ! 0 | 7781 | `			break;` |
|    ! 0 | 7782 | `		case 'c':` |
|    ! 0 | 7783 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d %02d:%02d:%02d",` |
|    ! 0 | 7784 | `				pTm->tm_year,` |
|    ! 0 | 7785 | `				pTm->tm_mon+1,` |
|    ! 0 | 7786 | `				pTm->tm_mday,` |
|    ! 0 | 7787 | `				pTm->tm_hour,` |
|    ! 0 | 7788 | `				pTm->tm_min,` |
|    ! 0 | 7789 | `				pTm->tm_sec` |
|      - | 7790 | `				);` |
|    ! 0 | 7791 | `			break;` |
|    ! 0 | 7792 | `		case 's':{` |
|      - | 7793 | `			time_t tt;` |
|      - | 7794 | `			/* Seconds since the Unix Epoch */` |
|    ! 0 | 7795 | `			time(&tt);` |
|    ! 0 | 7796 | `			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);` |
|    ! 0 | 7797 | `			break;` |
|      - | 7798 | `				 }` |
|    ! 0 | 7799 | `		default:` |
|      - | 7800 | `			/* unknown specifer,simply ignore*/` |
|    ! 0 | 7801 | `			break;` |
|      - | 7802 | `		}` |
|      - | 7803 | `		/* Advance the cursor */` |
|     21 | 7804 | `		zIn++;` |
|      1 | 7805 | `	}` |
|     17 | 7806 | `	return SXRET_OK;` |
|      1 | 7807 |  |
|      - | 7808 | `/*` |
|      - | 7809 | ` * string date(string $format [, int $timestamp = time() ] )` |
|      - | 7810 | ` *  Returns a string formatted according to the given format string using` |
|      - | 7811 | ` *  the given integer timestamp or the current time if no timestamp is given.` |
|      - | 7812 | ` *  In other words, timestamp is optional and defaults to the value of time().` |
|      - | 7813 | ` * Parameters` |
|      - | 7814 | ` *  $format` |
|      - | 7815 | ` *   The format of the outputted date string (See code above)` |
|      - | 7816 | ` * $timestamp` |
|      - | 7817 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|      - | 7818 | ` *   that defaults to the current local time if a timestamp is not given.` |
|      - | 7819 | ` *   In other words, it defaults to the value of time().` |
|      - | 7820 | ` * Return` |
|      - | 7821 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|      - | 7822 | ` */` |
|     36 | 7823 | `static int PH7_builtin_date(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7824 |  |
|      - | 7825 | `	const char *zFormat;` |
|      - | 7826 | `	int nLen;` |
|      - | 7827 | `	Sytm sTm;` |
|     37 | 7828 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7829 | `		/* Missing/Invalid argument,return FALSE */` |
|      5 | 7830 | `		ph7_result_bool(pCtx,0);` |
|      5 | 7831 | `		return PH7_OK;` |
|      - | 7832 | `	}` |
|     33 | 7833 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     33 | 7834 | `	if( nLen < 1 ){` |
|      - | 7835 | `		/* Don't bother processing return the empty string */` |
|    ! 0 | 7836 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7837 | `	}` |
|     33 | 7838 | `	if( nArg < 2 ){` |
|      - | 7839 | `#ifdef __WINNT__` |
|      - | 7840 | `		SYSTEMTIME sOS;` |
|      1 | 7841 | `		GetSystemTime(&sOS);` |
|      1 | 7842 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 7843 | `#else` |
|      - | 7844 | `		struct tm *pTm;` |
|      - | 7845 | `		time_t t;` |
|     30 | 7846 | `		time(&t);` |
|     30 | 7847 | `		pTm = localtime(&t);` |
|     30 | 7848 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7849 | `#endif` |
|     16 | 7850 | `	}else{` |
|      - | 7851 | `		/* Use the given timestamp */` |
|      - | 7852 | `		time_t t;` |
|      - | 7853 | `		struct tm *pTm;` |
|      3 | 7854 | `		if( ph7_value_is_int(apArg[1]) ){` |
|      3 | 7855 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|      3 | 7856 | `			pTm = localtime(&t);` |
|      3 | 7857 | `			if( pTm == 0 ){` |
|    ! 0 | 7858 | `				time(&t);` |
|    ! 0 | 7859 | `			}` |
|      2 | 7860 | `		}else{` |
|    ! 0 | 7861 | `			time(&t);` |
|      - | 7862 | `		}` |
|      3 | 7863 | `		pTm = localtime(&t);` |
|      3 | 7864 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7865 | `	}` |
|      - | 7866 | `	/* Format the given string */` |
|     33 | 7867 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|     33 | 7868 | `	return PH7_OK;` |
|     19 | 7869 |  |
|      - | 7870 | `/*` |
|      - | 7871 | ` * string strftime(string $format [, int $timestamp = time() ] )` |
|      - | 7872 | ` *  Format a local time/date (PLATFORM INDEPENDANT IMPLEENTATION NOT BASED ON LOCALE)` |
|      - | 7873 | ` * Parameters` |
|      - | 7874 | ` *  $format` |
|      - | 7875 | ` *   The format of the outputted date string (See code above)` |
|      - | 7876 | ` * $timestamp` |
|      - | 7877 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|      - | 7878 | ` *   that defaults to the current local time if a timestamp is not given.` |
|      - | 7879 | ` *   In other words, it defaults to the value of time().` |
|      - | 7880 | ` * Return` |
|      - | 7881 | ` * Returns a string formatted according format using the given timestamp` |
|      - | 7882 | ` * or the current local time if no timestamp is given.` |
|      - | 7883 | ` */` |
|     20 | 7884 | `static int PH7_builtin_strftime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7885 |  |
|      - | 7886 | `	const char *zFormat;` |
|      - | 7887 | `	int nLen;` |
|      - | 7888 | `	Sytm sTm;` |
|     21 | 7889 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7890 | `		/* Missing/Invalid argument,return FALSE */` |
|      5 | 7891 | `		ph7_result_bool(pCtx,0);` |
|      5 | 7892 | `		return PH7_OK;` |
|      - | 7893 | `	}` |
|     17 | 7894 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7895 | `	if( nLen < 1 ){` |
|      - | 7896 | `		/* Don't bother processing return FALSE */` |
|    ! 0 | 7897 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7898 | `	}` |
|     17 | 7899 | `	if( nArg < 2 ){` |
|      - | 7900 | `#ifdef __WINNT__` |
|      - | 7901 | `		SYSTEMTIME sOS;` |
|      1 | 7902 | `		GetSystemTime(&sOS);` |
|      1 | 7903 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 7904 | `#else` |
|      - | 7905 | `		struct tm *pTm;` |
|      - | 7906 | `		time_t t;` |
|     14 | 7907 | `		time(&t);` |
|     14 | 7908 | `		pTm = localtime(&t);` |
|     14 | 7909 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7910 | `#endif` |
|      8 | 7911 | `	}else{` |
|      - | 7912 | `		/* Use the given timestamp */` |
|      - | 7913 | `		time_t t;` |
|      - | 7914 | `		struct tm *pTm;` |
|      3 | 7915 | `		if( ph7_value_is_int(apArg[1]) ){` |
|      3 | 7916 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|      3 | 7917 | `			pTm = localtime(&t);` |
|      3 | 7918 | `			if( pTm == 0 ){` |
|    ! 0 | 7919 | `				time(&t);` |
|    ! 0 | 7920 | `			}` |
|      2 | 7921 | `		}else{` |
|    ! 0 | 7922 | `			time(&t);` |
|      - | 7923 | `		}` |
|      3 | 7924 | `		pTm = localtime(&t);` |
|      3 | 7925 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7926 | `	}` |
|      - | 7927 | `	/* Format the given string */` |
|     17 | 7928 | `	PH7_Strftime(pCtx,zFormat,nLen,&sTm);` |
|     17 | 7929 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|      - | 7930 | `		/* Nothing was formatted,return FALSE */` |
|    ! 0 | 7931 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7932 | `	}` |
|     17 | 7933 | `	return PH7_OK;` |
|     11 | 7934 |  |
|      - | 7935 | `/*` |
|      - | 7936 | ` * string gmdate(string $format [, int $timestamp = time() ] )` |
|      - | 7937 | ` *  Identical to the date() function except that the time returned` |
|      - | 7938 | ` *  is Greenwich Mean Time (GMT).` |
|      - | 7939 | ` * Parameters` |
|      - | 7940 | ` *  $format` |
|      - | 7941 | ` *  The format of the outputted date string (See code above)` |
|      - | 7942 | ` *  $timestamp` |
|      - | 7943 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|      - | 7944 | ` *   that defaults to the current local time if a timestamp is not given.` |
|      - | 7945 | ` *   In other words, it defaults to the value of time().` |
|      - | 7946 | ` * Return` |
|      - | 7947 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|      - | 7948 | ` */` |
|     16 | 7949 | `static int PH7_builtin_gmdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7950 |  |
|      - | 7951 | `	const char *zFormat;` |
|      - | 7952 | `	int nLen;` |
|      - | 7953 | `	Sytm sTm;` |
|     17 | 7954 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7955 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 | 7956 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7957 | `		return PH7_OK;` |
|      - | 7958 | `	}` |
|     15 | 7959 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 7960 | `	if( nLen < 1 ){` |
|      - | 7961 | `		/* Don't bother processing return the empty string */` |
|    ! 0 | 7962 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7963 | `	}` |
|     15 | 7964 | `	if( nArg < 2 ){` |
|      - | 7965 | `#ifdef __WINNT__` |
|      - | 7966 | `		SYSTEMTIME sOS;` |
|      1 | 7967 | `		GetSystemTime(&sOS);` |
|      1 | 7968 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 7969 | `#else` |
|      - | 7970 | `		struct tm *pTm;` |
|      - | 7971 | `		time_t t;` |
|     12 | 7972 | `		time(&t);` |
|     12 | 7973 | `		pTm = gmtime(&t);` |
|     12 | 7974 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7975 | `#endif` |
|      7 | 7976 | `	}else{` |
|      - | 7977 | `		/* Use the given timestamp */` |
|      - | 7978 | `		time_t t;` |
|      - | 7979 | `		struct tm *pTm;` |
|      3 | 7980 | `		if( ph7_value_is_int(apArg[1]) ){` |
|      3 | 7981 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|      3 | 7982 | `			pTm = gmtime(&t);` |
|      3 | 7983 | `			if( pTm == 0 ){` |
|    ! 0 | 7984 | `				time(&t);` |
|    ! 0 | 7985 | `			}` |
|      2 | 7986 | `		}else{` |
|    ! 0 | 7987 | `			time(&t);` |
|      - | 7988 | `		}` |
|      3 | 7989 | `		pTm = gmtime(&t);` |
|      3 | 7990 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7991 | `	}` |
|      - | 7992 | `	/* Format the given string */` |
|     15 | 7993 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|     15 | 7994 | `	return PH7_OK;` |
|      9 | 7995 |  |
|      - | 7996 | `/*` |
|      - | 7997 | ` * array localtime([ int $timestamp = time() [, bool $is_associative = false ]])` |
|      - | 7998 | ` *  Return the local time.` |
|      - | 7999 | ` * Parameter` |
|      - | 8000 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|      - | 8001 | ` *     that defaults to the current local time if a timestamp is not given.` |
|      - | 8002 | ` *     In other words, it defaults to the value of time().` |
|      - | 8003 | ` * $is_associative` |
|      - | 8004 | ` *   If set to FALSE or not supplied then the array is returned as a regular, numerically` |
|      - | 8005 | ` *   indexed array. If the argument is set to TRUE then localtime() returns an associative` |
|      - | 8006 | ` *   array containing all the different elements of the structure returned by the C function` |
|      - | 8007 | ` *   call to localtime. The names of the different keys of the associative array are as follows:` |
|      - | 8008 | ` *      "tm_sec" - seconds, 0 to 59` |
|      - | 8009 | ` *      "tm_min" - minutes, 0 to 59` |
|      - | 8010 | ` *      "tm_hour" - hours, 0 to 23` |
|      - | 8011 | ` *      "tm_mday" - day of the month, 1 to 31` |
|      - | 8012 | ` *      "tm_mon" - month of the year, 0 (Jan) to 11 (Dec)` |
|      - | 8013 | ` *      "tm_year" - years since 1900` |
|      - | 8014 | ` *      "tm_wday" - day of the week, 0 (Sun) to 6 (Sat)` |
|      - | 8015 | ` *      "tm_yday" - day of the year, 0 to 365` |
|      - | 8016 | ` *      "tm_isdst" - is daylight savings time in effect? Positive if yes, 0 if not, negative if unknown.` |
|      - | 8017 | ` * Returns` |
|      - | 8018 | ` *  An associative array of information related to the timestamp.` |
|      - | 8019 | ` */` |
|      8 | 8020 | `static int PH7_builtin_localtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8021 |  |
|      - | 8022 | `	ph7_value *pValue,*pArray;` |
|      9 | 8023 | `	int isAssoc = 0;` |
|      - | 8024 | `	Sytm sTm;` |
|      9 | 8025 | `	if( nArg < 1 ){` |
|      - | 8026 | `#ifdef __WINNT__` |
|      - | 8027 | `		SYSTEMTIME sOS;` |
|      1 | 8028 | `		GetSystemTime(&sOS); /* TODO(chems): GMT not local */` |
|      1 | 8029 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8030 | `#else` |
|      - | 8031 | `		struct tm *pTm;` |
|      - | 8032 | `		time_t t;` |
|      4 | 8033 | `		time(&t);` |
|      4 | 8034 | `		pTm = localtime(&t);` |
|      4 | 8035 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8036 | `#endif` |
|      3 | 8037 | `	}else{` |
|      - | 8038 | `		/* Use the given timestamp */` |
|      - | 8039 | `		time_t t;` |
|      - | 8040 | `		struct tm *pTm;` |
|      5 | 8041 | `		if( ph7_value_is_int(apArg[0]) ){` |
|      5 | 8042 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|      5 | 8043 | `			pTm = localtime(&t);` |
|      5 | 8044 | `			if( pTm == 0 ){` |
|    ! 0 | 8045 | `				time(&t);` |
|    ! 0 | 8046 | `			}` |
|      3 | 8047 | `		}else{` |
|    ! 0 | 8048 | `			time(&t);` |
|      - | 8049 | `		}` |
|      5 | 8050 | `		pTm = localtime(&t);` |
|      5 | 8051 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8052 | `	}` |
|      - | 8053 | `	/* Element value */` |
|      9 | 8054 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      9 | 8055 | `	if( pValue == 0 ){` |
|      - | 8056 | `		/* Return NULL */` |
|    ! 0 | 8057 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8058 | `		return PH7_OK;` |
|      - | 8059 | `	}` |
|      - | 8060 | `	/* Create a new array */` |
|      9 | 8061 | `	pArray = ph7_context_new_array(pCtx);` |
|      9 | 8062 | `	if( pArray == 0 ){` |
|      - | 8063 | `		/* Return NULL */` |
|    ! 0 | 8064 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8065 | `		return PH7_OK;` |
|      - | 8066 | `	}` |
|      9 | 8067 | `	if( nArg > 1 ){` |
|      3 | 8068 | `		isAssoc = ph7_value_to_bool(apArg[1]);` |
|      1 | 8069 | `	}` |
|      - | 8070 | `	/* Fill the array */` |
|      - | 8071 | `	/* Seconds */` |
|      9 | 8072 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|      9 | 8073 | `	if( isAssoc ){` |
|      3 | 8074 | `		ph7_array_add_strkey_elem(pArray,"tm_sec",pValue);` |
|      2 | 8075 | `	}else{` |
|      7 | 8076 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8077 | `	}` |
|      - | 8078 | `	/* Minutes */` |
|      9 | 8079 | `	ph7_value_int(pValue,sTm.tm_min);` |
|      9 | 8080 | `	if( isAssoc ){` |
|      3 | 8081 | `		ph7_array_add_strkey_elem(pArray,"tm_min",pValue);` |
|      2 | 8082 | `	}else{` |
|      7 | 8083 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8084 | `	}` |
|      - | 8085 | `	/* Hours */` |
|      9 | 8086 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|      9 | 8087 | `	if( isAssoc ){` |
|      3 | 8088 | `		ph7_array_add_strkey_elem(pArray,"tm_hour",pValue);` |
|      2 | 8089 | `	}else{` |
|      7 | 8090 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8091 | `	}` |
|      - | 8092 | `	/* mday */` |
|      9 | 8093 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|      9 | 8094 | `	if( isAssoc ){` |
|      3 | 8095 | `		ph7_array_add_strkey_elem(pArray,"tm_mday",pValue);` |
|      2 | 8096 | `	}else{` |
|      7 | 8097 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8098 | `	}` |
|      - | 8099 | `	/* mon */` |
|      9 | 8100 | `	ph7_value_int(pValue,sTm.tm_mon);` |
|      9 | 8101 | `	if( isAssoc ){` |
|      3 | 8102 | `		ph7_array_add_strkey_elem(pArray,"tm_mon",pValue);` |
|      2 | 8103 | `	}else{` |
|      7 | 8104 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8105 | `	}` |
|      - | 8106 | `	/* year since 1900 */` |
|      9 | 8107 | `	ph7_value_int(pValue,sTm.tm_year-1900);` |
|      9 | 8108 | `	if( isAssoc ){` |
|      3 | 8109 | `		ph7_array_add_strkey_elem(pArray,"tm_year",pValue);` |
|      2 | 8110 | `	}else{` |
|      7 | 8111 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8112 | `	}` |
|      - | 8113 | `	/* wday */` |
|      9 | 8114 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|      9 | 8115 | `	if( isAssoc ){` |
|      3 | 8116 | `		ph7_array_add_strkey_elem(pArray,"tm_wday",pValue);` |
|      2 | 8117 | `	}else{` |
|      7 | 8118 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8119 | `	}` |
|      - | 8120 | `	/* yday */` |
|      9 | 8121 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|      9 | 8122 | `	if( isAssoc ){` |
|      3 | 8123 | `		ph7_array_add_strkey_elem(pArray,"tm_yday",pValue);` |
|      2 | 8124 | `	}else{` |
|      7 | 8125 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8126 | `	}` |
|      - | 8127 | `	/* isdst */` |
|      - | 8128 | `#ifdef __WINNT__` |
|      - | 8129 | `#ifdef _MSC_VER` |
|      - | 8130 | `#ifndef _WIN32_WCE` |
|      1 | 8131 | `			_get_daylight(&sTm.tm_isdst);` |
|      - | 8132 | `#endif` |
|      - | 8133 | `#endif` |
|      - | 8134 | `#endif` |
|      9 | 8135 | `	ph7_value_int(pValue,sTm.tm_isdst);` |
|      9 | 8136 | `	if( isAssoc ){` |
|      3 | 8137 | `		ph7_array_add_strkey_elem(pArray,"tm_isdst",pValue);` |
|      2 | 8138 | `	}else{` |
|      7 | 8139 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8140 | `	}` |
|      - | 8141 | `	/* Return the array */` |
|      9 | 8142 | `	ph7_result_value(pCtx,pArray);` |
|      9 | 8143 | `	return PH7_OK;` |
|      5 | 8144 |  |
|      - | 8145 | `/*` |
|      - | 8146 | ` * int idate(string $format [, int $timestamp = time() ])` |
|      - | 8147 | ` *  Returns a number formatted according to the given format string` |
|      - | 8148 | ` *  using the given integer timestamp or the current local time if` |
|      - | 8149 | ` *  no timestamp is given. In other words, timestamp is optional and defaults` |
|      - | 8150 | ` *  to the value of time().` |
|      - | 8151 | ` *  Unlike the function date(), idate() accepts just one char in the format` |
|      - | 8152 | ` *  parameter.` |
|      - | 8153 | ` * $Parameters` |
|      - | 8154 | ` *  Supported format` |
|      - | 8155 | ` *   d 	Day of the month` |
|      - | 8156 | ` *   h 	Hour (12 hour format)` |
|      - | 8157 | ` *   H 	Hour (24 hour format)` |
|      - | 8158 | ` *   i 	Minutes` |
|      - | 8159 | ` *   I (uppercase i)1 if DST is activated, 0 otherwise` |
|      - | 8160 | ` *   L (uppercase l) returns 1 for leap year, 0 otherwise` |
|      - | 8161 | ` *   m 	Month number` |
|      - | 8162 | ` *   s 	Seconds` |
|      - | 8163 | ` *   t 	Days in current month` |
|      - | 8164 | ` *   U 	Seconds since the Unix Epoch - January 1 1970 00:00:00 UTC - this is the same as time()` |
|      - | 8165 | ` *   w 	Day of the week (0 on Sunday)` |
|      - | 8166 | ` *   W 	ISO-8601 week number of year, weeks starting on Monday` |
|      - | 8167 | ` *   y 	Year (1 or 2 digits - check note below)` |
|      - | 8168 | ` *   Y 	Year (4 digits)` |
|      - | 8169 | ` *   z 	Day of the year` |
|      - | 8170 | ` *   Z 	Timezone offset in seconds` |
|      - | 8171 | ` * $timestamp` |
|      - | 8172 | ` *  The optional timestamp parameter is an integer Unix timestamp that defaults` |
|      - | 8173 | ` *  to the current local time if a timestamp is not given. In other words, it defaults` |
|      - | 8174 | ` *  to the value of time().` |
|      - | 8175 | ` * Return` |
|      - | 8176 | ` *  An integer.` |
|      - | 8177 | ` */` |
|     40 | 8178 | `static int PH7_builtin_idate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 8179 |  |
|      - | 8180 | `	const char *zFormat;` |
|     42 | 8181 | `	ph7_int64 iVal = 0;` |
|      - | 8182 | `	int nLen;` |
|      - | 8183 | `	Sytm sTm;` |
|     42 | 8184 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8185 | `		/* Missing/Invalid argument,return -1 */` |
|      5 | 8186 | `		ph7_result_int(pCtx,-1);` |
|      5 | 8187 | `		return PH7_OK;` |
|      - | 8188 | `	}` |
|     42 | 8189 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     42 | 8190 | `	if( nLen < 1 ){` |
|      - | 8191 | `		/* Don't bother processing return -1*/` |
|    ! 0 | 8192 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 8193 | `	}` |
|     42 | 8194 | `	if( nArg < 2 ){` |
|      - | 8195 | `#ifdef __WINNT__` |
|      - | 8196 | `		SYSTEMTIME sOS;` |
|      2 | 8197 | `		GetSystemTime(&sOS);` |
|      2 | 8198 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8199 | `#else` |
|      - | 8200 | `		struct tm *pTm;` |
|      - | 8201 | `		time_t t;` |
|     30 | 8202 | `		time(&t);` |
|     30 | 8203 | `		pTm = localtime(&t);` |
|     30 | 8204 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8205 | `#endif` |
|     18 | 8206 | `	}else{` |
|      - | 8207 | `		/* Use the given timestamp */` |
|      - | 8208 | `		time_t t;` |
|      - | 8209 | `		struct tm *pTm;` |
|     11 | 8210 | `		if( ph7_value_is_int(apArg[1]) ){` |
|     11 | 8211 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|     11 | 8212 | `			pTm = localtime(&t);` |
|     11 | 8213 | `			if( pTm == 0 ){` |
|    ! 0 | 8214 | `				time(&t);` |
|    ! 0 | 8215 | `			}` |
|      6 | 8216 | `		}else{` |
|    ! 0 | 8217 | `			time(&t);` |
|      - | 8218 | `		}` |
|     11 | 8219 | `		pTm = localtime(&t);` |
|     11 | 8220 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8221 | `	}` |
|      - | 8222 | `	/* Perform the requested operation */` |
|     42 | 8223 | `	switch(zFormat[0]){` |
|      2 | 8224 | `	case 'd':` |
|      - | 8225 | `		/* Day of the month */` |
|      5 | 8226 | `		iVal = sTm.tm_mday;` |
|      5 | 8227 | `		break;` |
|    ! 0 | 8228 | `	case 'h':` |
|      - | 8229 | `		/*	Hour (12 hour format)*/` |
|    ! 0 | 8230 | `		iVal = 1 + (sTm.tm_hour % 12);` |
|    ! 0 | 8231 | `		break;` |
|      1 | 8232 | `	case 'H':` |
|      - | 8233 | `		/* Hour (24 hour format)*/` |
|      3 | 8234 | `		iVal = sTm.tm_hour;` |
|      3 | 8235 | `		break;` |
|      1 | 8236 | `	case 'i':` |
|      - | 8237 | `		/*Minutes*/` |
|      3 | 8238 | `		iVal = sTm.tm_min;` |
|      3 | 8239 | `		break;` |
|      1 | 8240 | `	case 'I':` |
|      - | 8241 | `		/*	returns 1 if DST is activated, 0 otherwise */` |
|      - | 8242 | `#ifdef __WINNT__` |
|      - | 8243 | `#ifdef _MSC_VER` |
|      - | 8244 | `#ifndef _WIN32_WCE` |
|      1 | 8245 | `			_get_daylight(&sTm.tm_isdst);` |
|      - | 8246 | `#endif` |
|      - | 8247 | `#endif` |
|      - | 8248 | `#endif` |
|      3 | 8249 | `		iVal = sTm.tm_isdst;` |
|      3 | 8250 | `		break;` |
|      1 | 8251 | `	case 'L':` |
|      - | 8252 | `		/* 	returns 1 for leap year, 0 otherwise */` |
|      3 | 8253 | `		iVal = IS_LEAP_YEAR(sTm.tm_year);` |
|      3 | 8254 | `		break;` |
|      2 | 8255 | `	case 'm':` |
|      - | 8256 | `		/* Month number*/` |
|      5 | 8257 | `		iVal = sTm.tm_mon;` |
|      5 | 8258 | `		break;` |
|      1 | 8259 | `	case 's':` |
|      - | 8260 | `		/*Seconds*/` |
|      3 | 8261 | `		iVal = sTm.tm_sec;` |
|      3 | 8262 | `		break;` |
|      1 | 8263 | `	case 't':{` |
|      - | 8264 | `		/*Days in current month*/` |
|      - | 8265 | `		static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|      5 | 8266 | `		int nDays = aMonDays[sTm.tm_mon % 12 ];` |
|      5 | 8267 | `		if( sTm.tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(sTm.tm_year) ){` |
|      3 | 8268 | `			nDays = 28;` |
|      1 | 8269 | `		}` |
|      7 | 8270 | `		iVal = nDays;` |
|      7 | 8271 | `		break;` |
|      - | 8272 | `			 }` |
|      1 | 8273 | `	case 'U':` |
|      - | 8274 | `		/*Seconds since the Unix Epoch*/` |
|      3 | 8275 | `		iVal = (ph7_int64)time(0);` |
|      3 | 8276 | `		break;` |
|      1 | 8277 | `	case 'w':` |
|      - | 8278 | `		/*	Day of the week (0 on Sunday) */` |
|      3 | 8279 | `		iVal = sTm.tm_wday;` |
|      3 | 8280 | `		break;` |
|      1 | 8281 | `	case 'W': {` |
|      - | 8282 | `		/* ISO-8601 week number of year, weeks starting on Monday */` |
|      - | 8283 | `		static const int aISO8601_local[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|      3 | 8284 | `		iVal = aISO8601_local[sTm.tm_wday % 7 ];` |
|      3 | 8285 | `		break;` |
|      - | 8286 | `			  }` |
|    ! 0 | 8287 | `	case 'y':` |
|      - | 8288 | `		/* Year (2 digits) */` |
|    ! 0 | 8289 | `		iVal = sTm.tm_year % 100;` |
|    ! 0 | 8290 | `		break;` |
|      3 | 8291 | `	case 'Y':` |
|      - | 8292 | `		/* Year (4 digits) */` |
|      7 | 8293 | `		iVal = sTm.tm_year;` |
|      7 | 8294 | `		break;` |
|      1 | 8295 | `	case 'z':` |
|      - | 8296 | `		/* Day of the year */` |
|      3 | 8297 | `		iVal = sTm.tm_yday;` |
|      3 | 8298 | `		break;` |
|      1 | 8299 | `	case 'Z':` |
|      - | 8300 | `		/*Timezone offset in seconds*/` |
|      3 | 8301 | `		iVal = sTm.tm_gmtoff;` |
|      3 | 8302 | `		break;` |
|      1 | 8303 | `	default:` |
|      - | 8304 | `		/* unknown format,throw a warning */` |
|      3 | 8305 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Unknown date format token");` |
|      2 | 8306 | `		break;` |
|      - | 8307 | `	}` |
|      - | 8308 | `	/* Return the time value */` |
|     40 | 8309 | `	ph7_result_int64(pCtx,iVal);` |
|     40 | 8310 | `	return PH7_OK;` |
|     23 | 8311 |  |
|      - | 8312 | `/*` |
|      - | 8313 | ` * int mktime/gmmktime([ int $hour = date("H") [, int $minute = date("i") [, int $second = date("s")` |
|      - | 8314 | ` *  [, int $month = date("n") [, int $day = date("j") [, int $year = date("Y") [, int $is_dst = -1 ]]]]]]] )` |
|      - | 8315 | ` *  Returns the Unix timestamp corresponding to the arguments given. This timestamp is a 64bit integer` |
|      - | 8316 | ` *  containing the number of seconds between the Unix Epoch (January 1 1970 00:00:00 GMT) and the time` |
|      - | 8317 | ` *  specified.` |
|      - | 8318 | ` *  Arguments may be left out in order from right to left; any arguments thus omitted will be set to` |
|      - | 8319 | ` *  the current value according to the local date and time.` |
|      - | 8320 | ` * Parameters` |
|      - | 8321 | ` * $hour` |
|      - | 8322 | ` *  The number of the hour relevant to the start of the day determined by month, day and year.` |
|      - | 8323 | ` *  Negative values reference the hour before midnight of the day in question. Values greater` |
|      - | 8324 | ` *  than 23 reference the appropriate hour in the following day(s).` |
|      - | 8325 | ` * $minute` |
|      - | 8326 | ` *  The number of the minute relevant to the start of the hour. Negative values reference` |
|      - | 8327 | ` *  the minute in the previous hour. Values greater than 59 reference the appropriate minute` |
|      - | 8328 | ` *  in the following hour(s).` |
|      - | 8329 | ` * $second` |
|      - | 8330 | ` *  The number of seconds relevant to the start of the minute. Negative values reference` |
|      - | 8331 | ` *  the second in the previous minute. Values greater than 59 reference the appropriate` |
|      - | 8332 | ` * second in the following minute(s).` |
|      - | 8333 | ` * $month` |
|      - | 8334 | ` *  The number of the month relevant to the end of the previous year. Values 1 to 12 reference` |
|      - | 8335 | ` *  the normal calendar months of the year in question. Values less than 1 (including negative values)` |
|      - | 8336 | ` *  reference the months in the previous year in reverse order, so 0 is December, -1 is November)...` |
|      - | 8337 | ` * $day` |
|      - | 8338 | ` *  The number of the day relevant to the end of the previous month. Values 1 to 28, 29, 30 or 31` |
|      - | 8339 | ` *  (depending upon the month) reference the normal days in the relevant month. Values less than 1` |
|      - | 8340 | ` *  (including negative values) reference the days in the previous month, so 0 is the last day` |
|      - | 8341 | ` *  of the previous month, -1 is the day before that, etc. Values greater than the number of days` |
|      - | 8342 | ` *  in the relevant month reference the appropriate day in the following month(s).` |
|      - | 8343 | ` * $year` |
|      - | 8344 | ` *  The number of the year, may be a two or four digit value, with values between 0-69 mapping` |
|      - | 8345 | ` *  to 2000-2069 and 70-100 to 1970-2000. On systems where time_t is a 32bit signed integer, as` |
|      - | 8346 | ` *  most common today, the valid range for year is somewhere between 1901 and 2038.` |
|      - | 8347 | ` * $is_dst` |
|      - | 8348 | ` *  This parameter can be set to 1 if the time is during daylight savings time (DST), 0 if it is not,` |
|      - | 8349 | ` *  or -1 (the default) if it is unknown whether the time is within daylight savings time or not.` |
|      - | 8350 | ` * Return` |
|      - | 8351 | ` *   mktime() returns the Unix timestamp of the arguments given.` |
|      - | 8352 | ` *   If the arguments are invalid, the function returns FALSE` |
|      - | 8353 | ` */` |
|      8 | 8354 | `static int PH7_builtin_mktime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8355 |  |
|      - | 8356 | `	const char *zFunction;` |
|      9 | 8357 | `	ph7_int64 iVal = 0;` |
|      - | 8358 | `	struct tm *pTm;` |
|      - | 8359 | `	time_t t;` |
|      - | 8360 | `	/* Extract function name */` |
|      9 | 8361 | `	zFunction = ph7_function_name(pCtx);` |
|      - | 8362 | `	/* Get the current time */` |
|      9 | 8363 | `	time(&t);` |
|      9 | 8364 | `	if( zFunction[0] == 'g' /* gmmktime */ ){` |
|      3 | 8365 | `		pTm = gmtime(&t);` |
|      2 | 8366 | `	}else{` |
|      - | 8367 | `		/* localtime */` |
|      7 | 8368 | `		pTm = localtime(&t);` |
|      - | 8369 | `	}` |
|      9 | 8370 | `	if( nArg > 0 ){` |
|      - | 8371 | `		int iTmp;` |
|      - | 8372 | `		/* Hour */` |
|      9 | 8373 | `		iTmp = ph7_value_to_int(apArg[0]);` |
|      9 | 8374 | `		pTm->tm_hour = iTmp;` |
|      9 | 8375 | `		if( nArg > 1 ){` |
|      - | 8376 | `			/* Minutes */` |
|      9 | 8377 | `			iTmp = ph7_value_to_int(apArg[1]);` |
|      9 | 8378 | `			pTm->tm_min = iTmp;` |
|      9 | 8379 | `			if( nArg > 2 ){` |
|      - | 8380 | `				/* Seconds */` |
|      9 | 8381 | `				iTmp = ph7_value_to_int(apArg[2]);` |
|      9 | 8382 | `				pTm->tm_sec = iTmp;` |
|      9 | 8383 | `				if( nArg > 3 ){` |
|      - | 8384 | `					/* Month */` |
|      9 | 8385 | `					iTmp = ph7_value_to_int(apArg[3]);` |
|      9 | 8386 | `					pTm->tm_mon = iTmp - 1;` |
|      9 | 8387 | `					if( nArg > 4 ){` |
|      - | 8388 | `						/* mday */` |
|      9 | 8389 | `						iTmp = ph7_value_to_int(apArg[4]);` |
|      9 | 8390 | `						pTm->tm_mday = iTmp;` |
|      9 | 8391 | `						if( nArg > 5 ){` |
|      - | 8392 | `							/* Year */` |
|      9 | 8393 | `							iTmp = ph7_value_to_int(apArg[5]);` |
|      9 | 8394 | `							if( iTmp > 1900 ){` |
|      9 | 8395 | `								iTmp -= 1900;` |
|      4 | 8396 | `							}` |
|      9 | 8397 | `							pTm->tm_year = iTmp;` |
|      9 | 8398 | `							if( nArg > 6 ){` |
|      - | 8399 | `								/* is_dst */` |
|    ! 0 | 8400 | `								iTmp = ph7_value_to_bool(apArg[6]);` |
|    ! 0 | 8401 | `								pTm->tm_isdst = iTmp;` |
|    ! 0 | 8402 | `							}` |
|      4 | 8403 | `						}` |
|      4 | 8404 | `					}` |
|      4 | 8405 | `				}` |
|      4 | 8406 | `			}` |
|      4 | 8407 | `		}` |
|      4 | 8408 | `	}` |
|      - | 8409 | `	/* Make the time */` |
|      9 | 8410 | `	iVal = (ph7_int64)mktime(pTm);` |
|      - | 8411 | `	/* Return the timesatmp as a 64bit integer */` |
|      9 | 8412 | `	ph7_result_int64(pCtx,iVal);` |
|      9 | 8413 | `	return PH7_OK;` |
|      1 | 8414 |  |
|      - | 8415 | `/*` |
|      - | 8416 | ` * Section:` |
|      - | 8417 | ` *    URL handling Functions.` |
|      - | 8418 | ` * Status:` |
|      - | 8419 | ` *    Stable.` |
|      - | 8420 | ` */` |
|      - | 8421 | `/*` |
|      - | 8422 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 8423 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 8424 | ` */` |
|   1026 | 8425 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 8426 |  |
|      - | 8427 | `	/* Store in the call context result buffer */` |
|   1028 | 8428 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 8429 | `	return SXRET_OK;` |
|      2 | 8430 |  |
|      - | 8431 | `/*` |
|      - | 8432 | ` * string base64_encode(string $data)` |
|      - | 8433 | ` * string convert_uuencode(string $data)` |
|      - | 8434 | ` *  Encodes data with MIME base64` |
|      - | 8435 | ` * Parameter` |
|      - | 8436 | ` *  $data` |
|      - | 8437 | ` *    Data to encode` |
|      - | 8438 | ` * Return` |
|      - | 8439 | ` *  Encoded data or FALSE on failure.` |
|      - | 8440 | ` */` |
|     10 | 8441 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8442 |  |
|      - | 8443 | `	const char *zIn;` |
|      - | 8444 | `	int nLen;` |
|     11 | 8445 | `	if( nArg < 1 ){` |
|      - | 8446 | `		/* Missing arguments,return FALSE */` |
|      5 | 8447 | `		ph7_result_bool(pCtx,0);` |
|      5 | 8448 | `		return PH7_OK;` |
|      - | 8449 | `	}` |
|      - | 8450 | `	/* Extract the input string */` |
|      7 | 8451 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8452 | `	if( nLen < 1 ){` |
|      - | 8453 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8454 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8455 | `		return PH7_OK;` |
|      - | 8456 | `	}` |
|      - | 8457 | `	/* Perform the BASE64 encoding */` |
|      7 | 8458 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 8459 | `	return PH7_OK;` |
|      6 | 8460 |  |
|      - | 8461 | `/*` |
|      - | 8462 | ` * string base64_decode(string $data)` |
|      - | 8463 | ` * string convert_uudecode(string $data)` |
|      - | 8464 | ` *  Decodes data encoded with MIME base64` |
|      - | 8465 | ` * Parameter` |
|      - | 8466 | ` *  $data` |
|      - | 8467 | ` *    Encoded data.` |
|      - | 8468 | ` * Return` |
|      - | 8469 | ` *  Returns the original data or FALSE on failure.` |
|      - | 8470 | ` */` |
|     36 | 8471 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 8472 |  |
|      - | 8473 | `	const char *zIn;` |
|      - | 8474 | `	int nLen;` |
|     38 | 8475 | `	if( nArg < 1 ){` |
|      - | 8476 | `		/* Missing arguments,return FALSE */` |
|      3 | 8477 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8478 | `		return PH7_OK;` |
|      - | 8479 | `	}` |
|      - | 8480 | `	/* Extract the input string */` |
|     36 | 8481 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 8482 | `	if( nLen < 1 ){` |
|      - | 8483 | `		/* Nothing to process,return FALSE */` |
|      3 | 8484 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8485 | `		return PH7_OK;` |
|      - | 8486 | `	}` |
|      - | 8487 | `	/* Perform the BASE64 decoding */` |
|     34 | 8488 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 8489 | `	return PH7_OK;` |
|     20 | 8490 |  |
|      - | 8491 | `/*` |
|      - | 8492 | ` * string urlencode(string $str)` |
|      - | 8493 | ` *  URL encoding` |
|      - | 8494 | ` * Parameter` |
|      - | 8495 | ` *  $data` |
|      - | 8496 | ` *   Input string.` |
|      - | 8497 | ` * Return` |
|      - | 8498 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 8499 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 8500 | ` *  encoded as plus (+) signs.` |
|      - | 8501 | ` */` |
|      6 | 8502 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8503 |  |
|      - | 8504 | `	const char *zIn;` |
|      - | 8505 | `	int nLen;` |
|      7 | 8506 | `	if( nArg < 1 ){` |
|      - | 8507 | `		/* Missing arguments,return FALSE */` |
|      3 | 8508 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8509 | `		return PH7_OK;` |
|      - | 8510 | `	}` |
|      - | 8511 | `	/* Extract the input string */` |
|      5 | 8512 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 8513 | `	if( nLen < 1 ){` |
|      - | 8514 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8515 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8516 | `		return PH7_OK;` |
|      - | 8517 | `	}` |
|      - | 8518 | `	/* Perform the URL encoding */` |
|      5 | 8519 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 8520 | `	return PH7_OK;` |
|      4 | 8521 |  |
|      - | 8522 | `/*` |
|      - | 8523 | ` * string urldecode(string $str)` |
|      - | 8524 | ` *  Decodes any %## encoding in the given string.` |
|      - | 8525 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 8526 | ` * Parameter` |
|      - | 8527 | ` *  $data` |
|      - | 8528 | ` *    Input string.` |
|      - | 8529 | ` * Return` |
|      - | 8530 | ` *  Decoded URL or FALSE on failure.` |
|      - | 8531 | ` */` |
|      8 | 8532 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8533 |  |
|      - | 8534 | `	const char *zIn;` |
|      - | 8535 | `	int nLen;` |
|      9 | 8536 | `	if( nArg < 1 ){` |
|      - | 8537 | `		/* Missing arguments,return FALSE */` |
|      3 | 8538 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8539 | `		return PH7_OK;` |
|      - | 8540 | `	}` |
|      - | 8541 | `	/* Extract the input string */` |
|      7 | 8542 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8543 | `	if( nLen < 1 ){` |
|      - | 8544 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8545 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8546 | `		return PH7_OK;` |
|      - | 8547 | `	}` |
|      - | 8548 | `	/* Perform the URL decoding */` |
|      7 | 8549 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 8550 | `	return PH7_OK;` |
|      5 | 8551 |  |
|      - | 8552 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 8553 | `/* Table of the built-in functions */` |
|      - | 8554 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 8555 | `	   /* Variable handling functions */` |
|      - | 8556 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 8557 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 8558 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 8559 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 8560 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 8561 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 8562 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 8563 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 8564 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 8565 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 8566 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 8567 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 8568 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 8569 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 8570 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 8571 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 8572 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 8573 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 8574 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 8575 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 8576 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 8577 | `	   /* Math functions */` |
|      - | 8578 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 8579 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 8580 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 8581 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 8582 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 8583 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 8584 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 8585 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 8586 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 8587 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 8588 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 8589 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 8590 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 8591 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 8592 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 8593 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 8594 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 8595 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 8596 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 8597 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 8598 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 8599 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 8600 | `	{ "round",    PH7_builtin_round        },` |
|      - | 8601 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 8602 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 8603 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 8604 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 8605 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 8606 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 8607 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 8608 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 8609 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 8610 | `	   /* String handling functions */` |
|      - | 8611 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 8612 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 8613 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 8614 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 8615 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 8616 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 8617 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 8618 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 8619 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 8620 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 8621 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 8622 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 8623 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 8624 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 8625 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 8626 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 8627 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 8628 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 8629 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 8630 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 8631 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 8632 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 8633 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 8634 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 8635 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 8636 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 8637 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 8638 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 8639 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 8640 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 8641 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 8642 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 8643 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 8644 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 8645 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 8646 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 8647 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 8648 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 8649 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 8650 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 8651 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 8652 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 8653 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 8654 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 8655 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 8656 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 8657 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 8658 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 8659 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 8660 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 8661 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 8662 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 8663 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 8664 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 8665 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 8666 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 8667 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 8668 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 8669 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 8670 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 8671 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 8672 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 8673 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 8674 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 8675 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 8676 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 8677 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 8678 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 8679 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 8680 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 8681 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 8682 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 8683 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 8684 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 8685 | `	         /* Ctype functions */` |
|      - | 8686 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 8687 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 8688 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 8689 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 8690 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 8691 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 8692 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 8693 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 8694 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 8695 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 8696 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 8697 | `	         /* Time functions */` |
|      - | 8698 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 8699 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 8700 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 8701 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 8702 | `	{ "date",        PH7_builtin_date         },` |
|      - | 8703 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 8704 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 8705 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 8706 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 8707 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 8708 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 8709 | `	        /* URL functions */` |
|      - | 8710 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 8711 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 8712 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 8713 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 8714 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 8715 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 8716 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 8717 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 8718 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 8719 | `};` |
|      - | 8720 | `/*` |
|      - | 8721 | ` * Register the built-in functions defined above,the array functions` |
|      - | 8722 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 8723 | ` */` |
|    924 | 8724 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      2 | 8725 |  |
|      - | 8726 | `	sxu32 n;` |
| 141374 | 8727 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 140450 | 8728 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
|  70226 | 8729 | `	}` |
|      - | 8730 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|    926 | 8731 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 8732 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|    926 | 8733 | `	PH7_RegisterIORoutine(&(*pVm));` |
|    926 | 8734 |  |
|      - | 8735 |  |
