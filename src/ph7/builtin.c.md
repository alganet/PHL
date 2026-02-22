# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3695/4332 lines (85.30%)

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
|  15390 |  269 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  270 |  |
|  15392 |  271 | `	int res = 1; /* Assume empty by default */` |
|  15392 |  272 | `	if( nArg > 0 ){` |
|  15390 |  273 | `		res = ph7_value_is_empty(apArg[0]);` |
|   7694 |  274 | `	}` |
|  15392 |  275 | `	ph7_result_bool(pCtx,res);` |
|  15392 |  276 | `	return PH7_OK;` |
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
|    100 |  630 | `static int PH7_builtin_abs(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  631 |  |
|      - |  632 | `	int is_float;` |
|      - |  633 | `	/* PHP requires exactly one argument. */` |
|    102 |  634 | `	if( nArg != 1 ){` |
|     11 |  635 | `		return PH7_VmThrowException(pCtx,` |
|      - |  636 | `			"ArgumentCountError",` |
|      - |  637 | `			"abs() expects exactly 1 argument, %d given",` |
|      3 |  638 | `			nArg` |
|      - |  639 | `			);` |
|      - |  640 | `	}` |
|      - |  641 |  |
|      - |  642 | `	/* Numeric strings with decimal/exponent are treated as real values. */` |
|     96 |  643 | `	is_float = ph7_value_is_float(apArg[0]);` |
|     96 |  644 | `	if( !is_float && ph7_value_is_string(apArg[0]) ){` |
|      - |  645 | `		int len;` |
|     10 |  646 | `		sxu8 bReal = FALSE;` |
|     10 |  647 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|      - |  648 | `		sxi32 rcNum;` |
|     10 |  649 | `		rcNum = SyStrIsNumeric(zStr, len, &bReal, 0);` |
|     10 |  650 | `		if( rcNum != SXRET_OK ){` |
|      3 |  651 | `			return PH7_VmThrowException(pCtx,` |
|      - |  652 | `				"TypeError",` |
|      - |  653 | `				"abs(): Argument #1 ($num) must be of type int\|float, string given"` |
|      - |  654 | `				);` |
|      - |  655 | `		}` |
|      7 |  656 | `		if( bReal ){` |
|      5 |  657 | `			is_float = 1;` |
|      2 |  658 | `		}` |
|      3 |  659 | `	}` |
|     94 |  660 | `	if( is_float ){` |
|      - |  661 | `		double r,x;` |
|     77 |  662 | `		x = ph7_value_to_double(apArg[0]);` |
|      - |  663 | `		/* Perform the requested operation */` |
|     77 |  664 | `		r = fabs(x);` |
|     77 |  665 | `		ph7_result_double(pCtx,r);` |
|     39 |  666 | `	}else{` |
|      - |  667 | `		int r,x;` |
|     18 |  668 | `		x = ph7_value_to_int(apArg[0]);` |
|      - |  669 | `		/* Perform the requested operation */` |
|     18 |  670 | `		r = abs(x);` |
|     18 |  671 | `		ph7_result_int(pCtx,r);` |
|      - |  672 | `	}` |
|     94 |  673 | `	return PH7_OK;` |
|     52 |  674 |  |
|      - |  675 | `/*` |
|      - |  676 | ` * float log(float $arg,[int/float $base])` |
|      - |  677 | ` *  Natural logarithm.` |
|      - |  678 | ` * Parameter` |
|      - |  679 | ` *  $arg: The number to process.` |
|      - |  680 | ` *  $base: The optional logarithmic base to use. (only base-10 is supported)` |
|      - |  681 | ` * Return` |
|      - |  682 | ` *  The logarithm of arg to base, if given, or the natural logarithm.` |
|      - |  683 | ` * Note:` |
|      - |  684 | ` *  only Natural log and base-10 log are supported.` |
|      - |  685 | ` */` |
|     14 |  686 | `static int PH7_builtin_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  687 |  |
|      - |  688 | `	double r,x;` |
|     15 |  689 | `	if( nArg < 1 ){` |
|      - |  690 | `		/* Missing argument,return 0 */` |
|      3 |  691 | `		ph7_result_int(pCtx,0);` |
|      3 |  692 | `		return PH7_OK;` |
|      - |  693 | `	}` |
|     13 |  694 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  695 | `	/* Perform the requested operation */` |
|     13 |  696 | `	if( nArg == 2 && ph7_value_is_numeric(apArg[1]) && ph7_value_to_int(apArg[1]) == 10 ){` |
|      - |  697 | `		/* Base-10 log */` |
|      5 |  698 | `		r = log10(x);` |
|      3 |  699 | `	}else{` |
|      9 |  700 | `		r = log(x);` |
|      - |  701 | `	}` |
|      - |  702 | `	/* store the result back */` |
|     13 |  703 | `	ph7_result_double(pCtx,r);` |
|     13 |  704 | `	return PH7_OK;` |
|      8 |  705 |  |
|      - |  706 | `/*` |
|      - |  707 | ` * float log10(float $arg )` |
|      - |  708 | ` *  Base-10 logarithm.` |
|      - |  709 | ` * Parameter` |
|      - |  710 | ` *  The number to process.` |
|      - |  711 | ` * Return` |
|      - |  712 | ` *  The Base-10 logarithm of the given number.` |
|      - |  713 | ` */` |
|     16 |  714 | `static int PH7_builtin_log10(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  715 |  |
|      - |  716 | `	double r,x;` |
|     17 |  717 | `	if( nArg < 1 ){` |
|      - |  718 | `		/* Missing argument,return 0 */` |
|      3 |  719 | `		ph7_result_int(pCtx,0);` |
|      3 |  720 | `		return PH7_OK;` |
|      - |  721 | `	}` |
|     15 |  722 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  723 | `	/* Perform the requested operation */` |
|     15 |  724 | `	r = log10(x);` |
|      - |  725 | `	/* store the result back */` |
|     15 |  726 | `	ph7_result_double(pCtx,r);` |
|     15 |  727 | `	return PH7_OK;` |
|      9 |  728 |  |
|      - |  729 | `/*` |
|      - |  730 | ` * number pow(number $base,number $exp)` |
|      - |  731 | ` *  Exponential expression.` |
|      - |  732 | ` * Parameter` |
|      - |  733 | ` *  base` |
|      - |  734 | ` *  The base to use.` |
|      - |  735 | ` * exp` |
|      - |  736 | ` *  The exponent.` |
|      - |  737 | ` * Return` |
|      - |  738 | ` *  base raised to the power of exp.` |
|      - |  739 | ` *  If the result can be represented as integer it will be returned` |
|      - |  740 | ` *  as type integer, else it will be returned as type float.` |
|      - |  741 | ` */` |
|      8 |  742 | `static int PH7_builtin_pow(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  743 |  |
|      - |  744 | `	double r,x,y;` |
|      9 |  745 | `	if( nArg < 1 ){` |
|      - |  746 | `		/* Missing argument,return 0 */` |
|      5 |  747 | `		ph7_result_int(pCtx,0);` |
|      5 |  748 | `		return PH7_OK;` |
|      - |  749 | `	}` |
|      5 |  750 | `	x = ph7_value_to_double(apArg[0]);` |
|      5 |  751 | `	y = ph7_value_to_double(apArg[1]);` |
|      - |  752 | `	/* Perform the requested operation */` |
|      5 |  753 | `	r = pow(x,y);` |
|      5 |  754 | `	ph7_result_double(pCtx,r);` |
|      5 |  755 | `	return PH7_OK;` |
|      5 |  756 |  |
|      - |  757 | `/*` |
|      - |  758 | ` * float pi(void)` |
|      - |  759 | ` *  Returns an approximation of pi.` |
|      - |  760 | ` * Note` |
|      - |  761 | ` *  you can use the M_PI constant which yields identical results to pi().` |
|      - |  762 | ` * Return` |
|      - |  763 | ` *  The value of pi as float.` |
|      - |  764 | ` */` |
|      2 |  765 | `static int PH7_builtin_pi(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  766 |  |
|      1 |  767 | `	SXUNUSED(nArg); /* cc warning */` |
|      1 |  768 | `	SXUNUSED(apArg);` |
|      3 |  769 | `	ph7_result_double(pCtx,PH7_PI);` |
|      3 |  770 | `	return PH7_OK;` |
|      1 |  771 |  |
|      - |  772 | `/*` |
|      - |  773 | ` * float fmod(float $x,float $y)` |
|      - |  774 | ` *  Returns the floating point remainder (modulo) of the division of the arguments.` |
|      - |  775 | ` * Parameters` |
|      - |  776 | ` * $x` |
|      - |  777 | ` *  The dividend` |
|      - |  778 | ` * $y` |
|      - |  779 | ` *  The divisor` |
|      - |  780 | ` * Return` |
|      - |  781 | ` *  The floating point remainder of x/y.` |
|      - |  782 | ` */` |
|      8 |  783 | `static int PH7_builtin_fmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  784 |  |
|      - |  785 | `	double x,y,r;` |
|      9 |  786 | `	if( nArg < 2 ){` |
|      - |  787 | `		/* Missing arguments */` |
|      7 |  788 | `		ph7_result_double(pCtx,0);` |
|      7 |  789 | `		return PH7_OK;` |
|      - |  790 | `	}` |
|      - |  791 | `	/* Extract given arguments */` |
|      3 |  792 | `	x = ph7_value_to_double(apArg[0]);` |
|      3 |  793 | `	y = ph7_value_to_double(apArg[1]);` |
|      - |  794 | `	/* Perform the requested operation */` |
|      3 |  795 | `	r = fmod(x,y);` |
|      - |  796 | `	/* Processing result */` |
|      3 |  797 | `	ph7_result_double(pCtx,r);` |
|      3 |  798 | `	return PH7_OK;` |
|      5 |  799 |  |
|      - |  800 | `/*` |
|      - |  801 | ` * float hypot(float $x,float $y)` |
|      - |  802 | ` *  Calculate the length of the hypotenuse of a right-angle triangle .` |
|      - |  803 | ` * Parameters` |
|      - |  804 | ` * $x` |
|      - |  805 | ` *  Length of first side` |
|      - |  806 | ` * $y` |
|      - |  807 | ` *  Length of first side` |
|      - |  808 | ` * Return` |
|      - |  809 | ` *  Calculated length of the hypotenuse.` |
|      - |  810 | ` */` |
|      6 |  811 | `static int PH7_builtin_hypot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  812 |  |
|      - |  813 | `	double x,y,r;` |
|      7 |  814 | `	if( nArg < 2 ){` |
|      - |  815 | `		/* Missing arguments */` |
|      5 |  816 | `		ph7_result_double(pCtx,0);` |
|      5 |  817 | `		return PH7_OK;` |
|      - |  818 | `	}` |
|      - |  819 | `	/* Extract given arguments */` |
|      3 |  820 | `	x = ph7_value_to_double(apArg[0]);` |
|      3 |  821 | `	y = ph7_value_to_double(apArg[1]);` |
|      - |  822 | `	/* Perform the requested operation */` |
|      3 |  823 | `	r = hypot(x,y);` |
|      - |  824 | `	/* Processing result */` |
|      3 |  825 | `	ph7_result_double(pCtx,r);` |
|      3 |  826 | `	return PH7_OK;` |
|      4 |  827 |  |
|      - |  828 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - |  829 | `/*` |
|      - |  830 | ` * float round ( float $val [, int $precision = 0 [, int $mode = PHP_ROUND_HALF_UP ]] )` |
|      - |  831 | ` *  Exponential expression.` |
|      - |  832 | ` * Parameter` |
|      - |  833 | ` *  $val` |
|      - |  834 | ` *   The value to round.` |
|      - |  835 | ` * $precision` |
|      - |  836 | ` *   The optional number of decimal digits to round to.` |
|      - |  837 | ` * $mode` |
|      - |  838 | ` *   One of PHP_ROUND_HALF_UP, PHP_ROUND_HALF_DOWN, PHP_ROUND_HALF_EVEN, or PHP_ROUND_HALF_ODD.` |
|      - |  839 | ` *   (not supported).` |
|      - |  840 | ` * Return` |
|      - |  841 | ` *  The rounded value.` |
|      - |  842 | ` */` |
|     20 |  843 | `static int PH7_builtin_round(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  844 |  |
|     21 |  845 | `	int n = 0;` |
|      - |  846 | `	double r;` |
|     21 |  847 | `	if( nArg < 1 ){` |
|      - |  848 | `		/* Missing argument,return 0 */` |
|      5 |  849 | `		ph7_result_int(pCtx,0);` |
|      5 |  850 | `		return PH7_OK;` |
|      - |  851 | `	}` |
|      - |  852 | `	/* Extract the precision if available */` |
|     17 |  853 | `	if( nArg > 1 ){` |
|      5 |  854 | `		n = ph7_value_to_int(apArg[1]);` |
|      5 |  855 | `		if( n>30 ){` |
|      3 |  856 | `			n = 30;` |
|      1 |  857 | `		}` |
|      5 |  858 | `		if( n<0 ){` |
|      3 |  859 | `			n = 0;` |
|      1 |  860 | `		}` |
|      2 |  861 | `	}` |
|     17 |  862 | `	r = ph7_value_to_double(apArg[0]);` |
|      - |  863 | `	/* If Y==0 and X will fit in a 64-bit int,` |
|      - |  864 | `     * handle the rounding directly.Otherwise` |
|      - |  865 | `	 * use our own cutsom printf [i.e:SyBufferFormat()].` |
|      - |  866 | `     */` |
|     17 |  867 | `	if( n==0 && r>=0 && r < (double)(LARGEST_INT64-1) ){` |
|     13 |  868 | `    r = (double)((ph7_int64)(r+0.5));` |
|     11 |  869 | `	}else if( n==0 && r<0 && (-r) < (double)(LARGEST_INT64-1) ){` |
|      3 |  870 | `    r = -(double)((ph7_int64)((-r)+0.5));` |
|      2 |  871 | `  }else{` |
|      - |  872 | `	  char zBuf[256];` |
|      - |  873 | `	  sxu32 nLen;` |
|      3 |  874 | `	  nLen = SyBufferFormat(zBuf,sizeof(zBuf),"%.*f",n,r);` |
|      - |  875 | `	  /* Convert the string to real number */` |
|      3 |  876 | `	  SyStrToReal(zBuf,nLen,(void *)&r,0);` |
|      - |  877 | `  }` |
|      - |  878 | `  /* Return thr rounded value */` |
|     17 |  879 | `  ph7_result_double(pCtx,r);` |
|     17 |  880 | `  return PH7_OK;` |
|     11 |  881 |  |
|      - |  882 | `/*` |
|      - |  883 | ` * string dechex(int $number)` |
|      - |  884 | ` *  Decimal to hexadecimal.` |
|      - |  885 | ` * Parameters` |
|      - |  886 | ` *  $number` |
|      - |  887 | ` *   Decimal value to convert` |
|      - |  888 | ` * Return` |
|      - |  889 | ` *  Hexadecimal string representation of number` |
|      - |  890 | ` */` |
|      6 |  891 | `static int PH7_builtin_dechex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  892 |  |
|      - |  893 | `	int iVal;` |
|      7 |  894 | `	if( nArg < 1 ){` |
|      - |  895 | `		/* Missing arguments,return null */` |
|      5 |  896 | `		ph7_result_null(pCtx);` |
|      5 |  897 | `		return PH7_OK;` |
|      - |  898 | `	}` |
|      - |  899 | `	/* Extract the given number */` |
|      3 |  900 | `	iVal = ph7_value_to_int(apArg[0]);` |
|      - |  901 | `	/* Format */` |
|      3 |  902 | `	ph7_result_string_format(pCtx,"%x",iVal);` |
|      3 |  903 | `	return PH7_OK;` |
|      4 |  904 |  |
|      - |  905 | `/*` |
|      - |  906 | ` * string decoct(int $number)` |
|      - |  907 | ` *  Decimal to Octal.` |
|      - |  908 | ` * Parameters` |
|      - |  909 | ` *  $number` |
|      - |  910 | ` *   Decimal value to convert` |
|      - |  911 | ` * Return` |
|      - |  912 | ` *  Octal string representation of number` |
|      - |  913 | ` */` |
|      8 |  914 | `static int PH7_builtin_decoct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  915 |  |
|      - |  916 | `	int iVal;` |
|      9 |  917 | `	if( nArg < 1 ){` |
|      - |  918 | `		/* Missing arguments,return null */` |
|      3 |  919 | `		ph7_result_null(pCtx);` |
|      3 |  920 | `		return PH7_OK;` |
|      - |  921 | `	}` |
|      - |  922 | `	/* Extract the given number */` |
|      7 |  923 | `	iVal = ph7_value_to_int(apArg[0]);` |
|      - |  924 | `	/* Format */` |
|      7 |  925 | `	ph7_result_string_format(pCtx,"%o",iVal);` |
|      7 |  926 | `	return PH7_OK;` |
|      5 |  927 |  |
|      - |  928 | `/*` |
|      - |  929 | ` * string decbin(int $number)` |
|      - |  930 | ` *  Decimal to binary.` |
|      - |  931 | ` * Parameters` |
|      - |  932 | ` *  $number` |
|      - |  933 | ` *   Decimal value to convert` |
|      - |  934 | ` * Return` |
|      - |  935 | ` *  Binary string representation of number` |
|      - |  936 | ` */` |
|      4 |  937 | `static int PH7_builtin_decbin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  938 |  |
|      - |  939 | `	int iVal;` |
|      5 |  940 | `	if( nArg < 1 ){` |
|      - |  941 | `		/* Missing arguments,return null */` |
|      3 |  942 | `		ph7_result_null(pCtx);` |
|      3 |  943 | `		return PH7_OK;` |
|      - |  944 | `	}` |
|      - |  945 | `	/* Extract the given number */` |
|      3 |  946 | `	iVal = ph7_value_to_int(apArg[0]);` |
|      - |  947 | `	/* Format */` |
|      3 |  948 | `	ph7_result_string_format(pCtx,"%B",iVal);` |
|      3 |  949 | `	return PH7_OK;` |
|      3 |  950 |  |
|      - |  951 | `/*` |
|      - |  952 | ` * int64 hexdec(string $hex_string)` |
|      - |  953 | ` *  Hexadecimal to decimal.` |
|      - |  954 | ` * Parameters` |
|      - |  955 | ` *  $hex_string` |
|      - |  956 | ` *   The hexadecimal string to convert` |
|      - |  957 | ` * Return` |
|      - |  958 | ` *  The decimal representation of hex_string` |
|      - |  959 | ` */` |
|     24 |  960 | `static int PH7_builtin_hexdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  961 |  |
|      - |  962 | `	const char *zString,*zEnd;` |
|      - |  963 | `	ph7_int64 iVal;` |
|      - |  964 | `	int nLen;` |
|     25 |  965 | `	if( nArg < 1 ){` |
|      - |  966 | `		/* Missing arguments,return -1 */` |
|      5 |  967 | `		ph7_result_int(pCtx,-1);` |
|      5 |  968 | `		return PH7_OK;` |
|      - |  969 | `	}` |
|     21 |  970 | `	iVal = 0;` |
|     21 |  971 | `	if( ph7_value_is_string(apArg[0]) ){` |
|      - |  972 | `		/* Extract the given string */` |
|     15 |  973 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|      - |  974 | `		/* Delimit the string */` |
|     15 |  975 | `		zEnd = &zString[nLen];` |
|      - |  976 | `		/* Ignore non hex-stream */` |
|     21 |  977 | `		while( zString < zEnd ){` |
|     21 |  978 | `			if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - |  979 | `				/* UTF-8 stream */` |
|      5 |  980 | `				zString++;` |
|      9 |  981 | `				while( zString < zEnd && (((unsigned char)zString[0] & 0xc0) == 0x80) ){` |
|      5 |  982 | `					zString++;` |
|      1 |  983 | `				}` |
|      3 |  984 | `			}else{` |
|     17 |  985 | `				if( SyisHex(zString[0]) ){` |
|     15 |  986 | `					break;` |
|      - |  987 | `				}` |
|      - |  988 | `				/* Ignore */` |
|      3 |  989 | `				zString++;` |
|      - |  990 | `			}` |
|      1 |  991 | `		}` |
|     15 |  992 | `		if( zString < zEnd ){` |
|      - |  993 | `			/* Cast */` |
|     15 |  994 | `			SyHexStrToInt64(zString,(sxu32)(zEnd-zString),(void *)&iVal,0);` |
|      7 |  995 | `		}` |
|      8 |  996 | `	}else{` |
|      - |  997 | `		/* Extract as a 64-bit integer */` |
|      7 |  998 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|      - |  999 | `	}` |
|      - | 1000 | `	/* Return the number */` |
|     21 | 1001 | `	ph7_result_int64(pCtx,iVal);` |
|     21 | 1002 | `	return PH7_OK;` |
|     13 | 1003 |  |
|      - | 1004 | `/*` |
|      - | 1005 | ` * int64 bindec(string $bin_string)` |
|      - | 1006 | ` *  Binary to decimal.` |
|      - | 1007 | ` * Parameters` |
|      - | 1008 | ` *  $bin_string` |
|      - | 1009 | ` *   The binary string to convert` |
|      - | 1010 | ` * Return` |
|      - | 1011 | ` *  Returns the decimal equivalent of the binary number represented by the binary_string argument.` |
|      - | 1012 | ` */` |
|     12 | 1013 | `static int PH7_builtin_bindec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1014 |  |
|      - | 1015 | `	const char *zString;` |
|      - | 1016 | `	ph7_int64 iVal;` |
|      - | 1017 | `	int nLen;` |
|     13 | 1018 | `	if( nArg < 1 ){` |
|      - | 1019 | `		/* Missing arguments,return -1 */` |
|      5 | 1020 | `		ph7_result_int(pCtx,-1);` |
|      5 | 1021 | `		return PH7_OK;` |
|      - | 1022 | `	}` |
|      9 | 1023 | `	iVal = 0;` |
|      9 | 1024 | `	if( ph7_value_is_string(apArg[0]) ){` |
|      - | 1025 | `		/* Extract the given string */` |
|      5 | 1026 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 1027 | `		if( nLen > 0 ){` |
|      - | 1028 | `			/* Perform a binary cast */` |
|      5 | 1029 | `			SyBinaryStrToInt64(zString,(sxu32)nLen,(void *)&iVal,0);` |
|      2 | 1030 | `		}` |
|      3 | 1031 | `	}else{` |
|      - | 1032 | `		/* Extract as a 64-bit integer */` |
|      5 | 1033 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|      - | 1034 | `	}` |
|      - | 1035 | `	/* Return the number */` |
|      9 | 1036 | `	ph7_result_int64(pCtx,iVal);` |
|      9 | 1037 | `	return PH7_OK;` |
|      7 | 1038 |  |
|      - | 1039 | `/*` |
|      - | 1040 | ` * int64 octdec(string $oct_string)` |
|      - | 1041 | ` *  Octal to decimal.` |
|      - | 1042 | ` * Parameters` |
|      - | 1043 | ` *  $oct_string` |
|      - | 1044 | ` *   The octal string to convert` |
|      - | 1045 | ` * Return` |
|      - | 1046 | ` *  Returns the decimal equivalent of the octal number represented by the octal_string argument.` |
|      - | 1047 | ` */` |
|      6 | 1048 | `static int PH7_builtin_octdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1049 |  |
|      - | 1050 | `	const char *zString;` |
|      - | 1051 | `	ph7_int64 iVal;` |
|      - | 1052 | `	int nLen;` |
|      7 | 1053 | `	if( nArg < 1 ){` |
|      - | 1054 | `		/* Missing arguments,return -1 */` |
|      3 | 1055 | `		ph7_result_int(pCtx,-1);` |
|      3 | 1056 | `		return PH7_OK;` |
|      - | 1057 | `	}` |
|      5 | 1058 | `	iVal = 0;` |
|      5 | 1059 | `	if( ph7_value_is_string(apArg[0]) ){` |
|      - | 1060 | `		/* Extract the given string */` |
|      3 | 1061 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 1062 | `		if( nLen > 0 ){` |
|      - | 1063 | `			/* Perform the cast */` |
|      3 | 1064 | `			SyOctalStrToInt64(zString,(sxu32)nLen,(void *)&iVal,0);` |
|      1 | 1065 | `		}` |
|      2 | 1066 | `	}else{` |
|      - | 1067 | `		/* Extract as a 64-bit integer */` |
|      3 | 1068 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|      - | 1069 | `	}` |
|      - | 1070 | `	/* Return the number */` |
|      5 | 1071 | `	ph7_result_int64(pCtx,iVal);` |
|      5 | 1072 | `	return PH7_OK;` |
|      4 | 1073 |  |
|      - | 1074 | `/*` |
|      - | 1075 | ` * srand([int $seed])` |
|      - | 1076 | ` * mt_srand([int $seed])` |
|      - | 1077 | ` *  Seed the random number generator.` |
|      - | 1078 | ` * Parameters` |
|      - | 1079 | ` * $seed` |
|      - | 1080 | ` *  Optional seed value` |
|      - | 1081 | ` * Return` |
|      - | 1082 | ` *  null.` |
|      - | 1083 | ` * Note:` |
|      - | 1084 | ` *  THIS FUNCTION IS A NO-OP.` |
|      - | 1085 | ` *  THE PH7 PRNG IS AUTOMATICALLY SEEDED WHEN THE VM IS CREATED.` |
|      - | 1086 | ` */` |
|     20 | 1087 | `static int PH7_builtin_srand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1088 |  |
|     10 | 1089 | `	SXUNUSED(nArg);` |
|     10 | 1090 | `	SXUNUSED(apArg);` |
|     21 | 1091 | `	ph7_result_null(pCtx);` |
|     21 | 1092 | `	return PH7_OK;` |
|      1 | 1093 |  |
|      - | 1094 | `/*` |
|      - | 1095 | ` * string base_convert(string $number,int $frombase,int $tobase)` |
|      - | 1096 | ` *  Convert a number between arbitrary bases.` |
|      - | 1097 | ` * Parameters` |
|      - | 1098 | ` * $number` |
|      - | 1099 | ` *  The number to convert` |
|      - | 1100 | ` * $frombase` |
|      - | 1101 | ` *  The base number is in` |
|      - | 1102 | ` * $tobase` |
|      - | 1103 | ` *  The base to convert number to` |
|      - | 1104 | ` * Return` |
|      - | 1105 | ` *  Number converted to base tobase` |
|      - | 1106 | ` */` |
|      - | 1107 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 1108 | `#ifdef PH7_NEED_FMT_AND_INI` |
|     48 | 1109 | `static int PH7_builtin_base_convert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      - | 1110 |  |
|      - | 1111 |  |
|      1 | 1112 |  |
|      - | 1113 | `	int nLen,iFbase,iTobase;` |
|      - | 1114 | `	const char *zNum;` |
|      - | 1115 | `	ph7_int64 iNum;` |
|     49 | 1116 | `	if( nArg < 3 ){` |
|      - | 1117 | `		/* Return the empty string*/` |
|     13 | 1118 | `		ph7_result_string(pCtx,"",0);` |
|     13 | 1119 | `		return PH7_OK;` |
|      - | 1120 | `	}` |
|      - | 1121 | `	/* Base numbers */` |
|     37 | 1122 | `	iFbase  = ph7_value_to_int(apArg[1]);` |
|     37 | 1123 | `	iTobase = ph7_value_to_int(apArg[2]);` |
|     37 | 1124 | `	if( ph7_value_is_string(apArg[0]) ){` |
|      - | 1125 | `		/* Extract the target number */` |
|     29 | 1126 | `		zNum = ph7_value_to_string(apArg[0],&nLen);` |
|     29 | 1127 | `		if( nLen < 1 ){` |
|      - | 1128 | `			/* Return the empty string*/` |
|    ! 0 | 1129 | `			ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1130 | `			return PH7_OK;` |
|      - | 1131 | `		}` |
|      - | 1132 | `		/* Base conversion */` |
|     29 | 1133 | `		switch(iFbase){` |
|      5 | 1134 | `		case 16:` |
|      - | 1135 | `			/* Hex */` |
|     11 | 1136 | `			SyHexStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|     11 | 1137 | `			break;` |
|      3 | 1138 | `		case 8:` |
|      - | 1139 | `			/* Octal */` |
|      7 | 1140 | `			SyOctalStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|      7 | 1141 | `			break;` |
|      2 | 1142 | `		case 2:` |
|      - | 1143 | `			/* Binary */` |
|      5 | 1144 | `			SyBinaryStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|      5 | 1145 | `			break;` |
|      4 | 1146 | `		default:` |
|      - | 1147 | `			/* Decimal */` |
|      9 | 1148 | `			SyStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|      8 | 1149 | `			break;` |
|      - | 1150 | `		}` |
|     15 | 1151 | `	}else{` |
|      9 | 1152 | `		iNum = ph7_value_to_int64(apArg[0]);` |
|      - | 1153 | `	}` |
|     37 | 1154 | `	switch(iTobase){` |
|      4 | 1155 | `	case 16:` |
|      - | 1156 | `		/* Hex */` |
|      9 | 1157 | `		ph7_result_string_format(pCtx,"%qx",iNum); /* Quad hex */` |
|      9 | 1158 | `		break;` |
|      1 | 1159 | `	case 8:` |
|      - | 1160 | `		/* Octal */` |
|      3 | 1161 | `		ph7_result_string_format(pCtx,"%qo",iNum); /* Quad octal */` |
|      3 | 1162 | `		break;` |
|      1 | 1163 | `	case 2:` |
|      - | 1164 | `		/* Binary */` |
|      3 | 1165 | `		ph7_result_string_format(pCtx,"%qB",iNum); /* Quad binary */` |
|      3 | 1166 | `		break;` |
|     12 | 1167 | `	default:` |
|      - | 1168 | `		/* Decimal */` |
|     25 | 1169 | `		ph7_result_string_format(pCtx,"%qd",iNum); /* Quad decimal */` |
|     24 | 1170 | `		break;` |
|      - | 1171 | `	}` |
|     37 | 1172 | `	return PH7_OK;` |
|     25 | 1173 |  |
|      - | 1174 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 1175 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 1176 | `/*` |
|      - | 1177 | ` * Section:` |
|      - | 1178 | ` *    String handling Functions.` |
|      - | 1179 | ` * Status:` |
|      - | 1180 | ` *    Stable.` |
|      - | 1181 | ` */` |
|      - | 1182 | `/*` |
|      - | 1183 | ` * string substr(string $string,int $start[, int $length ])` |
|      - | 1184 | ` *  Return part of a string.` |
|      - | 1185 | ` * Parameters` |
|      - | 1186 | ` *  $string` |
|      - | 1187 | ` *   The input string. Must be one character or longer.` |
|      - | 1188 | ` * $start` |
|      - | 1189 | ` *   If start is non-negative, the returned string will start at the start'th position` |
|      - | 1190 | ` *   in string, counting from zero. For instance, in the string 'abcdef', the character` |
|      - | 1191 | ` *   at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 1192 | ` *   If start is negative, the returned string will start at the start'th character` |
|      - | 1193 | ` *   from the end of string.` |
|      - | 1194 | ` *   If string is less than or equal to start characters long, FALSE will be returned.` |
|      - | 1195 | ` * $length` |
|      - | 1196 | ` *   If length is given and is positive, the string returned will contain at most length` |
|      - | 1197 | ` *   characters beginning from start (depending on the length of string).` |
|      - | 1198 | ` *   If length is given and is negative, then that many characters will be omitted from` |
|      - | 1199 | ` *   the end of string (after the start position has been calculated when a start is negative).` |
|      - | 1200 | ` *   If start denotes the position of this truncation or beyond, false will be returned.` |
|      - | 1201 | ` *   If length is given and is 0, FALSE or NULL an empty string will be returned.` |
|      - | 1202 | ` *   If length is omitted, the substring starting from start until the end of the string` |
|      - | 1203 | ` *   will be returned.` |
|      - | 1204 | ` * Return` |
|      - | 1205 | ` *  Returns the extracted part of string, or FALSE on failure or an empty string.` |
|      - | 1206 | ` */` |
| 109020 | 1207 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1208 |  |
|      - | 1209 | `	const char *zSource,*zOfft;` |
|      - | 1210 | `	int nOfft,nLen,nSrcLen;` |
| 109022 | 1211 | `	if( nArg < 2 ){` |
|      - | 1212 | `		/* return FALSE */` |
|      5 | 1213 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1214 | `		return PH7_OK;` |
|      - | 1215 | `	}` |
|      - | 1216 | `	/* Extract the target string */` |
| 109018 | 1217 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 109018 | 1218 | `	if( nSrcLen < 1 ){` |
|      - | 1219 | `		/* Empty string,return FALSE */` |
|   6920 | 1220 | `		ph7_result_bool(pCtx,0);` |
|   6920 | 1221 | `		return PH7_OK;` |
|      - | 1222 | `	}` |
| 102100 | 1223 | `	nLen = nSrcLen; /* cc warning */` |
|      - | 1224 | `	/* Extract the offset */` |
| 102100 | 1225 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 102100 | 1226 | `	if( nOfft < 0 ){` |
|  16858 | 1227 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  16858 | 1228 | `		if( zOfft < zSource ){` |
|      - | 1229 | `			/* Invalid offset */` |
|      5 | 1230 | `			ph7_result_bool(pCtx,0);` |
|      5 | 1231 | `			return PH7_OK;` |
|      - | 1232 | `		}` |
|  16854 | 1233 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  16854 | 1234 | `		nOfft = (int)(zOfft-zSource);` |
|  93670 | 1235 | `	}else if( nOfft >= nSrcLen ){` |
|      - | 1236 | `		/* Invalid offset */` |
|      7 | 1237 | `		ph7_result_bool(pCtx,0);` |
|      7 | 1238 | `		return PH7_OK;` |
|    ! 0 | 1239 | `	}else{` |
|  85238 | 1240 | `		zOfft = &zSource[nOfft];` |
|  85238 | 1241 | `		nLen = nSrcLen - nOfft;` |
|      - | 1242 | `	}` |
| 102090 | 1243 | `	if( nArg > 2 ){` |
|      - | 1244 | `		/* Extract the length */` |
|  85236 | 1245 | `		nLen = ph7_value_to_int(apArg[2]);` |
|  85236 | 1246 | `		if( nLen == 0 ){` |
|      - | 1247 | `			/* Invalid length,return an empty string */` |
|      5 | 1248 | `			ph7_result_string(pCtx,"",0);` |
|      5 | 1249 | `			return PH7_OK;` |
|  85232 | 1250 | `		}else if( nLen < 0 ){` |
|  16856 | 1251 | `			nLen = nSrcLen + nLen - nOfft;` |
|  16856 | 1252 | `			if( nLen < 1 ){` |
|      - | 1253 | `				/* Invalid  length */` |
|      3 | 1254 | `				nLen = nSrcLen - nOfft;` |
|      1 | 1255 | `			}` |
|   8427 | 1256 | `		}` |
|  85232 | 1257 | `		if( nLen + nOfft > nSrcLen ){` |
|      - | 1258 | `			/* Invalid length */` |
|   2136 | 1259 | `			nLen = nSrcLen - nOfft;` |
|   1067 | 1260 | `		}` |
|  42615 | 1261 | `	}` |
|      - | 1262 | `	/* Return the substring */` |
| 102086 | 1263 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 102086 | 1264 | `	return PH7_OK;` |
|  54512 | 1265 |  |
|      - | 1266 | `/*` |
|      - | 1267 | ` * int substr_compare(string $main_str,string $str ,int $offset[,int $length[,bool $case_insensitivity = false ]])` |
|      - | 1268 | ` *  Binary safe comparison of two strings from an offset, up to length characters.` |
|      - | 1269 | ` * Parameters` |
|      - | 1270 | ` *  $main_str` |
|      - | 1271 | ` *  The main string being compared.` |
|      - | 1272 | ` *  $str` |
|      - | 1273 | ` *   The secondary string being compared.` |
|      - | 1274 | ` * $offset` |
|      - | 1275 | ` *  The start position for the comparison. If negative, it starts counting from` |
|      - | 1276 | ` *  the end of the string.` |
|      - | 1277 | ` * $length` |
|      - | 1278 | ` *  The length of the comparison. The default value is the largest of the length` |
|      - | 1279 | ` *  of the str compared to the length of main_str less the offset.` |
|      - | 1280 | ` * $case_insensitivity` |
|      - | 1281 | ` *  If case_insensitivity is TRUE, comparison is case insensitive.` |
|      - | 1282 | ` * Return` |
|      - | 1283 | ` *  Returns < 0 if main_str from position offset is less than str, > 0 if it is greater than` |
|      - | 1284 | ` *  str, and 0 if they are equal. If offset is equal to or greater than the length of main_str` |
|      - | 1285 | ` *  or length is set and is less than 1, substr_compare() prints a warning and returns FALSE.` |
|      - | 1286 | ` */` |
|     26 | 1287 | `static int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1288 |  |
|      - | 1289 | `	const char *zSource,*zOfft,*zSub;` |
|      - | 1290 | `	int nOfft,nLen,nSrcLen,nSublen;` |
|     27 | 1291 | `	int iCase = 0;` |
|      - | 1292 | `	int rc;` |
|     27 | 1293 | `	if( nArg < 3 ){` |
|      - | 1294 | `		/* Missing arguments,return FALSE */` |
|      5 | 1295 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1296 | `		return PH7_OK;` |
|      - | 1297 | `	}` |
|      - | 1298 | `	/* Extract the target string */` |
|     23 | 1299 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|     23 | 1300 | `	if( nSrcLen < 1 ){` |
|      - | 1301 | `		/* Empty string,return FALSE */` |
|      3 | 1302 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1303 | `		return PH7_OK;` |
|      - | 1304 | `	}` |
|     21 | 1305 | `	nLen = nSrcLen; /* cc warning */` |
|      - | 1306 | `	/* Extract the substring */` |
|     21 | 1307 | `	zSub = ph7_value_to_string(apArg[1],&nSublen);` |
|     21 | 1308 | `	if( nSublen < 1 \|\| nSublen > nSrcLen){` |
|      - | 1309 | `		/* Empty string,return FALSE */` |
|      3 | 1310 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1311 | `		return PH7_OK;` |
|      - | 1312 | `	}` |
|      - | 1313 | `	/* Extract the offset */` |
|     19 | 1314 | `	nOfft = ph7_value_to_int(apArg[2]);` |
|     19 | 1315 | `	if( nOfft < 0 ){` |
|      5 | 1316 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|      5 | 1317 | `		if( zOfft < zSource ){` |
|      - | 1318 | `			/* Invalid offset */` |
|      3 | 1319 | `			ph7_result_bool(pCtx,0);` |
|      3 | 1320 | `			return PH7_OK;` |
|      - | 1321 | `		}` |
|      3 | 1322 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|      3 | 1323 | `		nOfft = (int)(zOfft-zSource);` |
|     16 | 1324 | `	}else if( nOfft >= nSrcLen ){` |
|      - | 1325 | `		/* Invalid offset */` |
|      3 | 1326 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1327 | `		return PH7_OK;` |
|    ! 0 | 1328 | `	}else{` |
|     13 | 1329 | `		zOfft = &zSource[nOfft];` |
|     13 | 1330 | `		nLen = nSrcLen - nOfft;` |
|      - | 1331 | `	}` |
|     15 | 1332 | `	if( nArg > 3 ){` |
|      - | 1333 | `		/* Extract the length */` |
|     13 | 1334 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     13 | 1335 | `		if( nLen < 1 ){` |
|      - | 1336 | `			/* Invalid  length */` |
|      5 | 1337 | `			ph7_result_int(pCtx,1);` |
|      5 | 1338 | `			return PH7_OK;` |
|      9 | 1339 | `		}else if( nLen + nOfft > nSrcLen ){` |
|      - | 1340 | `			/* Invalid length */` |
|      3 | 1341 | `			nLen = nSrcLen - nOfft;` |
|      1 | 1342 | `		}` |
|      9 | 1343 | `		if( nArg > 4 ){` |
|      - | 1344 | `			/* Case-sensitive or not */` |
|      5 | 1345 | `			iCase = ph7_value_to_bool(apArg[4]);` |
|      2 | 1346 | `		}` |
|      4 | 1347 | `	}` |
|      - | 1348 | `	/* Perform the comparison */` |
|     11 | 1349 | `	if( iCase ){` |
|      3 | 1350 | `		rc = SyStrnicmp(zOfft,zSub,(sxu32)nLen);` |
|      2 | 1351 | `	}else{` |
|      9 | 1352 | `		rc = SyStrncmp(zOfft,zSub,(sxu32)nLen);` |
|      - | 1353 | `	}` |
|      - | 1354 | `	/* Comparison result */` |
|     11 | 1355 | `	ph7_result_int(pCtx,rc);` |
|     11 | 1356 | `	return PH7_OK;` |
|     14 | 1357 |  |
|      - | 1358 | `/*` |
|      - | 1359 | ` * int substr_count(string $haystack,string $needle[,int $offset = 0 [,int $length ]])` |
|      - | 1360 | ` *  Count the number of substring occurrences.` |
|      - | 1361 | ` * Parameters` |
|      - | 1362 | ` * $haystack` |
|      - | 1363 | ` *   The string to search in` |
|      - | 1364 | ` * $needle` |
|      - | 1365 | ` *   The substring to search for` |
|      - | 1366 | ` * $offset` |
|      - | 1367 | ` *  The offset where to start counting` |
|      - | 1368 | ` * $length (NOT USED)` |
|      - | 1369 | ` *  The maximum length after the specified offset to search for the substring.` |
|      - | 1370 | ` *  It outputs a warning if the offset plus the length is greater than the haystack length.` |
|      - | 1371 | ` * Return` |
|      - | 1372 | ` *  Toral number of substring occurrences.` |
|      - | 1373 | ` */` |
|     24 | 1374 | `static int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1375 |  |
|      - | 1376 | `	const char *zText,*zPattern,*zEnd;` |
|      - | 1377 | `	int nTextlen,nPatlen;` |
|     25 | 1378 | `	int iCount = 0;` |
|      - | 1379 | `	sxu32 nOfft;` |
|      - | 1380 | `	sxi32 rc;` |
|     25 | 1381 | `	if( nArg < 2 ){` |
|      - | 1382 | `		/* Missing arguments */` |
|      5 | 1383 | `		ph7_result_int(pCtx,0);` |
|      5 | 1384 | `		return PH7_OK;` |
|      - | 1385 | `	}` |
|      - | 1386 | `	/* Point to the haystack */` |
|     21 | 1387 | `	zText = ph7_value_to_string(apArg[0],&nTextlen);` |
|      - | 1388 | `	/* Point to the neddle */` |
|     21 | 1389 | `	zPattern = ph7_value_to_string(apArg[1],&nPatlen);` |
|     21 | 1390 | `	if( nTextlen < 1 \|\| nPatlen < 1 \|\| nPatlen > nTextlen ){` |
|      - | 1391 | `		/* NOOP,return zero */` |
|      3 | 1392 | `		ph7_result_int(pCtx,0);` |
|      3 | 1393 | `		return PH7_OK;` |
|      - | 1394 | `	}` |
|     19 | 1395 | `	if( nArg > 2 ){` |
|      - | 1396 | `		int iOfft;` |
|      - | 1397 | `		/* Extract the offset */` |
|     15 | 1398 | `		iOfft = ph7_value_to_int(apArg[2]);` |
|     15 | 1399 | `		if( iOfft < 0 \|\| iOfft > nTextlen ){` |
|      - | 1400 | `			/* Invalid offset,return zero */` |
|      3 | 1401 | `			ph7_result_int(pCtx,0);` |
|      3 | 1402 | `			return PH7_OK;` |
|      - | 1403 | `		}` |
|      - | 1404 | `		/* Point to the desired offset */` |
|     13 | 1405 | `		zText = &zText[iOfft];` |
|      - | 1406 | `		/* Adjust length */` |
|     13 | 1407 | `		nTextlen -= iOfft;` |
|      6 | 1408 | `	}` |
|      - | 1409 | `	/* Point to the end of the string */` |
|     17 | 1410 | `	zEnd = &zText[nTextlen];` |
|     17 | 1411 | `	if( nArg > 3 ){` |
|      - | 1412 | `		int nLen;` |
|      - | 1413 | `		/* Extract the length */` |
|     13 | 1414 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     13 | 1415 | `		if( nLen < 0 \|\| nLen > nTextlen ){` |
|      - | 1416 | `			/* Invalid length,return 0 */` |
|      7 | 1417 | `			ph7_result_int(pCtx,0);` |
|      7 | 1418 | `			return PH7_OK;` |
|      - | 1419 | `		}` |
|      - | 1420 | `		/* Adjust pointer */` |
|      7 | 1421 | `		nTextlen = nLen;` |
|      7 | 1422 | `		zEnd = &zText[nTextlen];` |
|      3 | 1423 | `	}` |
|      - | 1424 | `	/* Perform the search */` |
|     12 | 1425 | `	for(;;){` |
|     25 | 1426 | `		rc = SyBlobSearch((const void *)zText,(sxu32)(zEnd-zText),(const void *)zPattern,nPatlen,&nOfft);` |
|     25 | 1427 | `		if( rc != SXRET_OK ){` |
|      - | 1428 | `			/* Pattern not found,break immediately */` |
|      9 | 1429 | `			break;` |
|      - | 1430 | `		}` |
|      - | 1431 | `		/* Increment counter and update the offset */` |
|     17 | 1432 | `		iCount++;` |
|     17 | 1433 | `		zText += nOfft + nPatlen;` |
|     17 | 1434 | `		if( zText >= zEnd ){` |
|      3 | 1435 | `			break;` |
|      - | 1436 | `		}` |
|      1 | 1437 | `	}` |
|      - | 1438 | `	/* Pattern count */` |
|     11 | 1439 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 1440 | `	return PH7_OK;` |
|     13 | 1441 |  |
|      - | 1442 | `/*` |
|      - | 1443 | ` * string chunk_split(string $body[,int $chunklen = 76 [, string $end = "\r\n" ]])` |
|      - | 1444 | ` *   Split a string into smaller chunks.` |
|      - | 1445 | ` * Parameters` |
|      - | 1446 | ` *  $body` |
|      - | 1447 | ` *   The string to be chunked.` |
|      - | 1448 | ` * $chunklen` |
|      - | 1449 | ` *   The chunk length.` |
|      - | 1450 | ` * $end` |
|      - | 1451 | ` *   The line ending sequence.` |
|      - | 1452 | ` * Return` |
|      - | 1453 | ` *  The chunked string or NULL on failure.` |
|      - | 1454 | ` */` |
|     16 | 1455 | `static int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1456 |  |
|     17 | 1457 | `	const char *zIn,*zEnd,*zSep = "\r\n";` |
|      - | 1458 | `	int nSepLen,nChunkLen,nLen;` |
|     17 | 1459 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1460 | `		/* Nothing to split,return null */` |
|      5 | 1461 | `		ph7_result_null(pCtx);` |
|      5 | 1462 | `		return PH7_OK;` |
|      - | 1463 | `	}` |
|      - | 1464 | `	/* initialize/Extract arguments */` |
|     13 | 1465 | `	nSepLen = (int)sizeof("\r\n") - 1;` |
|     13 | 1466 | `	nChunkLen = 76;` |
|     13 | 1467 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 1468 | `	zEnd = &zIn[nLen];` |
|     13 | 1469 | `	if( nArg > 1 ){` |
|      - | 1470 | `		/* Chunk length */` |
|     13 | 1471 | `		nChunkLen = ph7_value_to_int(apArg[1]);` |
|     13 | 1472 | `		if( nChunkLen < 1 ){` |
|      - | 1473 | `			/* Switch back to the default length */` |
|      3 | 1474 | `			nChunkLen = 76;` |
|      1 | 1475 | `		}` |
|     13 | 1476 | `		if( nArg > 2 ){` |
|      - | 1477 | `			/* Separator */` |
|      9 | 1478 | `			zSep = ph7_value_to_string(apArg[2],&nSepLen);` |
|      9 | 1479 | `			if( nSepLen < 1 ){` |
|      - | 1480 | `				/* Switch back to the default separator */` |
|      3 | 1481 | `				zSep = "\r\n";` |
|      3 | 1482 | `				nSepLen = (int)sizeof("\r\n") - 1;` |
|      1 | 1483 | `			}` |
|      4 | 1484 | `		}` |
|      6 | 1485 | `	}` |
|      - | 1486 | `	/* Perform the requested operation */` |
|     13 | 1487 | `	if( nChunkLen > nLen ){` |
|      - | 1488 | `		/* Nothing to split,return the string and the separator */` |
|      9 | 1489 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);` |
|      9 | 1490 | `		return PH7_OK;` |
|      - | 1491 | `	}` |
|     17 | 1492 | `	while( zIn < zEnd ){` |
|     13 | 1493 | `		if( nChunkLen > (int)(zEnd-zIn) ){` |
|      3 | 1494 | `			nChunkLen = (int)(zEnd - zIn);` |
|      1 | 1495 | `		}` |
|      - | 1496 | `		/* Append the chunk and the separator */` |
|     13 | 1497 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);` |
|      - | 1498 | `		/* Point beyond the chunk */` |
|     13 | 1499 | `		zIn += nChunkLen;` |
|      1 | 1500 | `	}` |
|      5 | 1501 | `	return PH7_OK;` |
|      9 | 1502 |  |
|      - | 1503 | `/*` |
|      - | 1504 | ` * string addslashes(string $str)` |
|      - | 1505 | ` *  Quote string with slashes.` |
|      - | 1506 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1507 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1508 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1509 | ` * Parameter` |
|      - | 1510 | ` *  str: The string to be escaped.` |
|      - | 1511 | ` * Return` |
|      - | 1512 | ` *  Returns the escaped string` |
|      - | 1513 | ` */` |
|     10 | 1514 | `static int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1515 |  |
|      - | 1516 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1517 | `	int nLen;` |
|     11 | 1518 | `	if( nArg < 1 ){` |
|      - | 1519 | `		/* Nothing to process,retun NULL */` |
|      5 | 1520 | `		ph7_result_null(pCtx);` |
|      5 | 1521 | `		return PH7_OK;` |
|      - | 1522 | `	}` |
|      - | 1523 | `	/* Extract the string to process */` |
|      7 | 1524 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 1525 | `	if( nLen < 1 ){` |
|      - | 1526 | `		/* Return the empty string */` |
|      5 | 1527 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1528 | `		return PH7_OK;` |
|      - | 1529 | `	}` |
|      3 | 1530 | `	zEnd = &zIn[nLen];` |
|      3 | 1531 | `	zCur = 0; /* cc warning */` |
|      3 | 1532 | `	for(;;){` |
|      7 | 1533 | `		if( zIn >= zEnd ){` |
|      - | 1534 | `			/* No more input */` |
|      3 | 1535 | `			break;` |
|      - | 1536 | `		}` |
|      5 | 1537 | `		zCur = zIn;` |
|     15 | 1538 | `		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' ){` |
|     11 | 1539 | `			zIn++;` |
|      1 | 1540 | `		}` |
|      5 | 1541 | `		if( zIn > zCur ){` |
|      - | 1542 | `			/* Append raw contents */` |
|      5 | 1543 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 1544 | `		}` |
|      5 | 1545 | `		if( zIn < zEnd ){` |
|      3 | 1546 | `			int c = zIn[0];` |
|      3 | 1547 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|      1 | 1548 | `		}` |
|      5 | 1549 | `		zIn++;` |
|      1 | 1550 | `	}` |
|      3 | 1551 | `	return PH7_OK;` |
|      6 | 1552 |  |
|      - | 1553 | `/*` |
|      - | 1554 | ` * Check if the given character is present in the given mask.` |
|      - | 1555 | ` * Return TRUE if present. FALSE otherwise.` |
|      - | 1556 | ` */` |
|     76 | 1557 | `static int cSlashCheckMask(int c,const char *zMask,int nLen)` |
|      1 | 1558 |  |
|     77 | 1559 | `	const char *zEnd = &zMask[nLen];` |
|    495 | 1560 | `	while( zMask < zEnd ){` |
|    449 | 1561 | `		if( zMask[0] == c ){` |
|      - | 1562 | `			/* Character present,return TRUE */` |
|     31 | 1563 | `			return 1;` |
|      - | 1564 | `		}` |
|      - | 1565 | `		/* Advance the pointer */` |
|    419 | 1566 | `		zMask++;` |
|      1 | 1567 | `	}` |
|      - | 1568 | `	/* Not present */` |
|     47 | 1569 | `	return 0;` |
|     39 | 1570 |  |
|      - | 1571 | `/*` |
|      - | 1572 | ` * string addcslashes(string $str,string $charlist)` |
|      - | 1573 | ` *  Quote string with slashes in a C style.` |
|      - | 1574 | ` * Parameter` |
|      - | 1575 | ` *  $str:` |
|      - | 1576 | ` *    The string to be escaped.` |
|      - | 1577 | ` *  $charlist:` |
|      - | 1578 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|      - | 1579 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|      - | 1580 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|      - | 1581 | ` * Return` |
|      - | 1582 | ` *  Returns the escaped string.` |
|      - | 1583 | ` * Note:` |
|      - | 1584 | ` *  Range characters [i.e: 'A..Z'] is not implemented in the current release.` |
|      - | 1585 | ` */` |
|     12 | 1586 | `static int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1587 |  |
|      - | 1588 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - | 1589 | `	int nLen,nMask;` |
|     13 | 1590 | `	if( nArg < 1 ){` |
|      - | 1591 | `		/* Nothing to process,retun NULL */` |
|      3 | 1592 | `		ph7_result_null(pCtx);` |
|      3 | 1593 | `		return PH7_OK;` |
|      - | 1594 | `	}` |
|      - | 1595 | `	/* Extract the string to process */` |
|     11 | 1596 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 1597 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 1598 | `		/* Return the string untouched */` |
|      5 | 1599 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      5 | 1600 | `		return PH7_OK;` |
|      - | 1601 | `	}` |
|      - | 1602 | `	/* Extract the desired mask */` |
|      7 | 1603 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|      7 | 1604 | `	zEnd = &zIn[nLen];` |
|      7 | 1605 | `	zCur = 0; /* cc warning */` |
|      8 | 1606 | `	for(;;){` |
|     17 | 1607 | `		if( zIn >= zEnd ){` |
|      - | 1608 | `			/* No more input */` |
|      7 | 1609 | `			break;` |
|      - | 1610 | `		}` |
|     11 | 1611 | `		zCur = zIn;` |
|     31 | 1612 | `		while( zIn < zEnd && !cSlashCheckMask(zIn[0],zMask,nMask) ){` |
|     21 | 1613 | `			zIn++;` |
|      1 | 1614 | `		}` |
|     11 | 1615 | `		if( zIn > zCur ){` |
|      - | 1616 | `			/* Append raw contents */` |
|     11 | 1617 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      5 | 1618 | `		}` |
|     11 | 1619 | `		if( zIn < zEnd ){` |
|      5 | 1620 | `			int c = zIn[0];` |
|      5 | 1621 | `			if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|      - | 1622 | `				/* Convert to octal */` |
|      3 | 1623 | `				ph7_result_string_format(pCtx,"\\%o",c);` |
|      2 | 1624 | `			}else{` |
|      3 | 1625 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1626 | `			}` |
|      2 | 1627 | `		}` |
|     11 | 1628 | `		zIn++;` |
|      1 | 1629 | `	}` |
|      7 | 1630 | `	return PH7_OK;` |
|      7 | 1631 |  |
|      - | 1632 | `/*` |
|      - | 1633 | ` * string quotemeta(string $str)` |
|      - | 1634 | ` *  Quote meta characters.` |
|      - | 1635 | ` * Parameter` |
|      - | 1636 | ` *  $str:` |
|      - | 1637 | ` *    The string to be escaped.` |
|      - | 1638 | ` * Return` |
|      - | 1639 | ` *  Returns the escaped string.` |
|      - | 1640 | `*/` |
|     10 | 1641 | `static int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1642 |  |
|      - | 1643 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1644 | `	int nLen;` |
|     11 | 1645 | `	if( nArg < 1 ){` |
|      - | 1646 | `		/* Nothing to process,retun NULL */` |
|      3 | 1647 | `		ph7_result_null(pCtx);` |
|      3 | 1648 | `		return PH7_OK;` |
|      - | 1649 | `	}` |
|      - | 1650 | `	/* Extract the string to process */` |
|      9 | 1651 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 1652 | `	if( nLen < 1 ){` |
|      - | 1653 | `		/* Return the empty string */` |
|      3 | 1654 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1655 | `		return PH7_OK;` |
|      - | 1656 | `	}` |
|      7 | 1657 | `	zEnd = &zIn[nLen];` |
|      7 | 1658 | `	zCur = 0; /* cc warning */` |
|     17 | 1659 | `	for(;;){` |
|     35 | 1660 | `		if( zIn >= zEnd ){` |
|      - | 1661 | `			/* No more input */` |
|      7 | 1662 | `			break;` |
|      - | 1663 | `		}` |
|     29 | 1664 | `		zCur = zIn;` |
|     55 | 1665 | `		while( zIn < zEnd && !cSlashCheckMask(zIn[0],".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1) ){` |
|     27 | 1666 | `			zIn++;` |
|      1 | 1667 | `		}` |
|     29 | 1668 | `		if( zIn > zCur ){` |
|      - | 1669 | `			/* Append raw contents */` |
|     11 | 1670 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      5 | 1671 | `		}` |
|     29 | 1672 | `		if( zIn < zEnd ){` |
|     27 | 1673 | `			int c = zIn[0];` |
|     27 | 1674 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|     13 | 1675 | `		}` |
|     29 | 1676 | `		zIn++;` |
|      1 | 1677 | `	}` |
|      7 | 1678 | `	return PH7_OK;` |
|      6 | 1679 |  |
|      - | 1680 | `/*` |
|      - | 1681 | ` * string stripslashes(string $str)` |
|      - | 1682 | ` *  Un-quotes a quoted string.` |
|      - | 1683 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1684 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1685 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1686 | ` * Parameter` |
|      - | 1687 | ` *  $str` |
|      - | 1688 | ` *   The input string.` |
|      - | 1689 | ` * Return` |
|      - | 1690 | ` *  Returns a string with backslashes stripped off.` |
|      - | 1691 | ` */` |
|      8 | 1692 | `static int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1693 |  |
|      - | 1694 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1695 | `	int nLen;` |
|      9 | 1696 | `	if( nArg < 1 ){` |
|      - | 1697 | `		/* Nothing to process,retun NULL */` |
|      3 | 1698 | `		ph7_result_null(pCtx);` |
|      3 | 1699 | `		return PH7_OK;` |
|      - | 1700 | `	}` |
|      - | 1701 | `	/* Extract the string to process */` |
|      7 | 1702 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 1703 | `	if( zIn == 0 ){` |
|    ! 0 | 1704 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1705 | `		return PH7_OK;` |
|      - | 1706 | `	}` |
|      7 | 1707 | `	zEnd = &zIn[nLen];` |
|      7 | 1708 | `	zCur = 0; /* cc warning */` |
|      - | 1709 | `	/* Encode the string */` |
|      4 | 1710 | `	for(;;){` |
|      9 | 1711 | `		if( zIn >= zEnd ){` |
|      - | 1712 | `			/* No more input */` |
|      5 | 1713 | `			break;` |
|      - | 1714 | `		}` |
|      5 | 1715 | `		zCur = zIn;` |
|     17 | 1716 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 | 1717 | `			zIn++;` |
|      1 | 1718 | `		}` |
|      5 | 1719 | `		if( zIn > zCur ){` |
|      - | 1720 | `			/* Append raw contents */` |
|      5 | 1721 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 1722 | `		}` |
|      5 | 1723 | `		if( &zIn[1] < zEnd ){` |
|      3 | 1724 | `			int c = zIn[1];` |
|      3 | 1725 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - | 1726 | `				/* Ignore the backslash */` |
|      3 | 1727 | `				zIn++;` |
|      1 | 1728 | `			}` |
|      2 | 1729 | `		}else{` |
|      3 | 1730 | `			break;` |
|      - | 1731 | `		}` |
|      1 | 1732 | `	}` |
|      7 | 1733 | `	return PH7_OK;` |
|      5 | 1734 |  |
|      - | 1735 | `/*` |
|      - | 1736 | ` * string htmlspecialchars(string $string [, int $flags = ENT_COMPAT \| ENT_HTML401 [, string $charset]])` |
|      - | 1737 | ` *  HTML escaping of special characters.` |
|      - | 1738 | ` *  The translations performed are:` |
|      - | 1739 | ` *   '&' (ampersand) ==> '&amp;'` |
|      - | 1740 | ` *   '"' (double quote) ==> '&quot;' when ENT_NOQUOTES is not set.` |
|      - | 1741 | ` *   "'" (single quote) ==> '&#039;' only when ENT_QUOTES is set.` |
|      - | 1742 | ` *   '<' (less than) ==> '&lt;'` |
|      - | 1743 | ` *   '>' (greater than) ==> '&gt;'` |
|      - | 1744 | ` * Parameters` |
|      - | 1745 | ` *  $string` |
|      - | 1746 | ` *   The string being converted.` |
|      - | 1747 | ` * $flags` |
|      - | 1748 | ` *   A bitmask of one or more of the following flags, which specify how to handle quotes.` |
|      - | 1749 | ` *   The default is ENT_COMPAT \| ENT_HTML401.` |
|      - | 1750 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone.` |
|      - | 1751 | ` *   ENT_QUOTES 	Will convert both double and single quotes.` |
|      - | 1752 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted.` |
|      - | 1753 | ` *   ENT_IGNORE 	Silently discard invalid code unit sequences instead of returning an empty string.` |
|      - | 1754 | ` * $charset` |
|      - | 1755 | ` *  Defines character set used in conversion. The default character set is ISO-8859-1. (Not used)` |
|      - | 1756 | ` * Return` |
|      - | 1757 | ` *  The escaped string or NULL on failure.` |
|      - | 1758 | ` */` |
|     20 | 1759 | `static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1760 |  |
|      - | 1761 | `	const char *zCur,*zIn,*zEnd;` |
|     21 | 1762 | `	int iFlags = 0x01\|0x40; /* ENT_COMPAT \| ENT_HTML401 */` |
|      - | 1763 | `	int nLen,c;` |
|     21 | 1764 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1765 | `		/* Missing/Invalid arguments,return NULL */` |
|     11 | 1766 | `		ph7_result_null(pCtx);` |
|     11 | 1767 | `		return PH7_OK;` |
|      - | 1768 | `	}` |
|      - | 1769 | `	/* Extract the target string */` |
|     11 | 1770 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 1771 | `	zEnd = &zIn[nLen];` |
|      - | 1772 | `	/* Extract the flags if available */` |
|     11 | 1773 | `	if( nArg > 1 ){` |
|      9 | 1774 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 | 1775 | `		if( iFlags < 0 ){` |
|      3 | 1776 | `			iFlags = 0x01\|0x40;` |
|      1 | 1777 | `		}` |
|      4 | 1778 | `	}` |
|      - | 1779 | `	/* Perform the requested operation */` |
|     23 | 1780 | `	for(;;){` |
|     47 | 1781 | `		if( zIn >= zEnd ){` |
|      9 | 1782 | `			break;` |
|      - | 1783 | `		}` |
|     39 | 1784 | `		zCur = zIn;` |
|     83 | 1785 | `		while( zIn < zEnd && zIn[0] != '&' && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '<' && zIn[0] != '>' ){` |
|     45 | 1786 | `			zIn++;` |
|      1 | 1787 | `		}` |
|     39 | 1788 | `		if( zCur < zIn ){` |
|      - | 1789 | `			/* Append the raw string verbatim */` |
|     17 | 1790 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      8 | 1791 | `		}` |
|     39 | 1792 | `		if( zIn >= zEnd ){` |
|      3 | 1793 | `			break;` |
|      - | 1794 | `		}` |
|     37 | 1795 | `		c = zIn[0];` |
|     37 | 1796 | `		if( c == '&' ){` |
|      - | 1797 | `			/* Expand '&amp;' */` |
|      9 | 1798 | `			ph7_result_string(pCtx,"&amp;",(int)sizeof("&amp;")-1);` |
|     33 | 1799 | `		}else if( c == '<' ){` |
|      - | 1800 | `			/* Expand '&lt;' */` |
|      7 | 1801 | `			ph7_result_string(pCtx,"&lt;",(int)sizeof("&lt;")-1);` |
|     26 | 1802 | `		}else if( c == '>' ){` |
|      - | 1803 | `			/* Expand '&gt;' */` |
|      9 | 1804 | `			ph7_result_string(pCtx,"&gt;",(int)sizeof("&gt;")-1);` |
|     19 | 1805 | `		}else if( c == '\'' ){` |
|      5 | 1806 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 1807 | `				/* Expand '&#039;' */` |
|      5 | 1808 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      3 | 1809 | `			}else{` |
|      - | 1810 | `				/* Leave the single quote untouched */` |
|    ! 0 | 1811 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      1 | 1812 | `			}` |
|     13 | 1813 | `		}else if( c == '"' ){` |
|     11 | 1814 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      - | 1815 | `				/* Expand '&quot;' */` |
|      7 | 1816 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      4 | 1817 | `			}else{` |
|      - | 1818 | `				/* Leave the double quote untouched */` |
|      5 | 1819 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      - | 1820 | `			}` |
|      5 | 1821 | `		}` |
|      - | 1822 | `		/* Ignore the unsafe HTML character */` |
|     37 | 1823 | `		zIn++;` |
|      1 | 1824 | `	}` |
|     11 | 1825 | `	return PH7_OK;` |
|     11 | 1826 |  |
|      - | 1827 | `/*` |
|      - | 1828 | ` * string htmlspecialchars_decode(string $string[,int $quote_style = ENT_COMPAT ])` |
|      - | 1829 | ` *  Unescape HTML entities.` |
|      - | 1830 | ` * Parameters` |
|      - | 1831 | ` *  $string` |
|      - | 1832 | ` *   The string to decode` |
|      - | 1833 | ` *  $quote_style` |
|      - | 1834 | ` *    The quote style. One of the following constants:` |
|      - | 1835 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone (default)` |
|      - | 1836 | ` *   ENT_QUOTES 	Will convert both double and single quotes` |
|      - | 1837 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted` |
|      - | 1838 | ` * Return` |
|      - | 1839 | ` *  The unescaped string or NULL on failure.` |
|      - | 1840 | ` */` |
|     16 | 1841 | `static int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1842 |  |
|      - | 1843 | `	const char *zCur,*zIn,*zEnd;` |
|     17 | 1844 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 1845 | `	int nLen,nJump;` |
|     17 | 1846 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1847 | `		/* Missing/Invalid arguments,return NULL */` |
|      7 | 1848 | `		ph7_result_null(pCtx);` |
|      7 | 1849 | `		return PH7_OK;` |
|      - | 1850 | `	}` |
|      - | 1851 | `	/* Extract the target string */` |
|     11 | 1852 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 1853 | `	zEnd = &zIn[nLen];` |
|      - | 1854 | `	/* Extract the flags if available */` |
|     11 | 1855 | `	if( nArg > 1 ){` |
|      7 | 1856 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      7 | 1857 | `		if( iFlags < 0 ){` |
|      3 | 1858 | `			iFlags = 0x01;` |
|      1 | 1859 | `		}` |
|      3 | 1860 | `	}` |
|      - | 1861 | `	/* Perform the requested operation */` |
|     15 | 1862 | `	for(;;){` |
|     31 | 1863 | `		if( zIn >= zEnd ){` |
|     11 | 1864 | `			break;` |
|      - | 1865 | `		}` |
|     21 | 1866 | `		zCur = zIn;` |
|     51 | 1867 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|     31 | 1868 | `			zIn++;` |
|      1 | 1869 | `		}` |
|     21 | 1870 | `		if( zCur < zIn ){` |
|      - | 1871 | `			/* Append the raw string verbatim */` |
|      9 | 1872 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 1873 | `		}` |
|     21 | 1874 | `		nLen = (int)(zEnd-zIn);` |
|     21 | 1875 | `		nJump = (int)sizeof(char);` |
|     21 | 1876 | `		if( nLen >= (int)sizeof("&amp;")-1 && SyStrnicmp(zIn,"&amp;",sizeof("&amp;")-1) == 0 ){` |
|      - | 1877 | `			/* &amp; ==> '&' */` |
|      3 | 1878 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|      3 | 1879 | `			nJump = (int)sizeof("&amp;")-1;` |
|     20 | 1880 | `		}else if( nLen >= (int)sizeof("&lt;")-1 && SyStrnicmp(zIn,"&lt;",sizeof("&lt;")-1) == 0 ){` |
|      - | 1881 | `			/* &lt; ==> < */` |
|      3 | 1882 | `			ph7_result_string(pCtx,"<",(int)sizeof(char));` |
|      3 | 1883 | `			nJump = (int)sizeof("&lt;")-1;` |
|     18 | 1884 | `		}else if( nLen >= (int)sizeof("&gt;")-1 && SyStrnicmp(zIn,"&gt;",sizeof("&gt;")-1) == 0 ){` |
|      - | 1885 | `			/* &gt; ==> '>' */` |
|      3 | 1886 | `			ph7_result_string(pCtx,">",(int)sizeof(char));` |
|      3 | 1887 | `			nJump = (int)sizeof("&gt;")-1;` |
|     16 | 1888 | `		}else if( nLen >= (int)sizeof("&quot;")-1 && SyStrnicmp(zIn,"&quot;",sizeof("&quot;")-1) == 0 ){` |
|      - | 1889 | `			/* &quot; ==> '"' */` |
|     13 | 1890 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      9 | 1891 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      5 | 1892 | `			}else{` |
|      - | 1893 | `				/* Leave untouched */` |
|      5 | 1894 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      - | 1895 | `			}` |
|     13 | 1896 | `			nJump = (int)sizeof("&quot;")-1;` |
|      9 | 1897 | `		}else if( nLen >= (int)sizeof("&#039;")-1 && SyStrnicmp(zIn,"&#039;",sizeof("&#039;")-1) == 0 ){` |
|      - | 1898 | `			/* &#039; ==> ''' */` |
|      3 | 1899 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 1900 | `				/* Expand ''' */` |
|      3 | 1901 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      2 | 1902 | `			}else{` |
|      - | 1903 | `				/* Leave untouched */` |
|    ! 0 | 1904 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      - | 1905 | `			}` |
|      3 | 1906 | `			nJump = (int)sizeof("&#039;")-1;` |
|      1 | 1907 | `		}else if( nLen >= (int)sizeof(char) ){` |
|      - | 1908 | `			/* expand '&' */` |
|    ! 0 | 1909 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 1910 | `		}else{` |
|      - | 1911 | `			/* No more input to process */` |
|    ! 0 | 1912 | `			break;` |
|      - | 1913 | `		}` |
|     21 | 1914 | `		zIn += nJump;` |
|      1 | 1915 | `	}` |
|     11 | 1916 | `	return PH7_OK;` |
|      9 | 1917 |  |
|      - | 1918 | `/* HTML encoding/Decoding table` |
|      - | 1919 | ` * Source: Symisc RunTime API.[chm@symisc.net]` |
|      - | 1920 | ` */` |
|      - | 1921 | `static const char *azHtmlEscape[] = {` |
|      - | 1922 | ` 	"&lt;","<","&gt;",">","&amp;","&","&quot;","\"","&#39;","'",` |
|      - | 1923 | `	"&#33;","!","&#36;","$","&#35;","#","&#37;","%","&#40;","(",` |
|      - | 1924 | `	"&#41;",")","&#123;","{","&#125;","}","&#61;","=","&#43;","+",` |
|      - | 1925 | `	"&#63;","?","&#91;","[","&#93;","]","&#64;","@","&#44;",","` |
|      - | 1926 | ` };` |
|      - | 1927 | `/*` |
|      - | 1928 | ` * array get_html_translation_table(void)` |
|      - | 1929 | ` *  Returns the translation table used by htmlspecialchars() and htmlentities().` |
|      - | 1930 | ` * Parameters` |
|      - | 1931 | ` *  None` |
|      - | 1932 | ` * Return` |
|      - | 1933 | ` *  The translation table as an array or NULL on failure.` |
|      - | 1934 | ` */` |
|      4 | 1935 | `static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1936 |  |
|      - | 1937 | `	ph7_value *pArray,*pValue;` |
|      - | 1938 | `	sxu32 n;` |
|      - | 1939 | `	/* Element value */` |
|      5 | 1940 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      5 | 1941 | `	if( pValue == 0 ){` |
|    ! 0 | 1942 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 1943 | `		SXUNUSED(apArg);` |
|      - | 1944 | `		/* Return NULL */` |
|    ! 0 | 1945 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1946 | `		return PH7_OK;` |
|      - | 1947 | `	}` |
|      - | 1948 | `	/* Create a new array */` |
|      5 | 1949 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 | 1950 | `	if( pArray == 0 ){` |
|      - | 1951 | `		/* Return NULL */` |
|    ! 0 | 1952 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1953 | `		return PH7_OK;` |
|      - | 1954 | `	}` |
|      - | 1955 | `	/* Make the table */` |
|     85 | 1956 | `	for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|      - | 1957 | `		/* Prepare the value */` |
|     81 | 1958 | `		ph7_value_string(pValue,azHtmlEscape[n],-1 /* Compute length automatically */);` |
|      - | 1959 | `		/* Insert the value */` |
|     81 | 1960 | `		ph7_array_add_strkey_elem(pArray,azHtmlEscape[n+1],pValue);` |
|      - | 1961 | `		/* Reset the string cursor */` |
|     81 | 1962 | `		ph7_value_reset_string_cursor(pValue);` |
|     41 | 1963 | `	}` |
|      - | 1964 | `	/*` |
|      - | 1965 | `	 * Return the array.` |
|      - | 1966 | `	 * Don't worry about freeing memory, everything will be automatically` |
|      - | 1967 | `	 * released upon we return from this function.` |
|      - | 1968 | `	 */` |
|      5 | 1969 | `	ph7_result_value(pCtx,pArray);` |
|      5 | 1970 | `	return PH7_OK;` |
|      3 | 1971 |  |
|      - | 1972 | `/*` |
|      - | 1973 | ` * string htmlentities( string $string [, int $flags = ENT_COMPAT \| ENT_HTML401]);` |
|      - | 1974 | ` *   Convert all applicable characters to HTML entities` |
|      - | 1975 | ` * Parameters` |
|      - | 1976 | ` * $string` |
|      - | 1977 | ` *   The input string.` |
|      - | 1978 | ` * $flags` |
|      - | 1979 | ` *  A bitmask of one or more of the flags (see block-comment on PH7_builtin_htmlspecialchars())` |
|      - | 1980 | ` * Return` |
|      - | 1981 | ` * The encoded string.` |
|      - | 1982 | ` */` |
|     10 | 1983 | `static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1984 |  |
|     11 | 1985 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 1986 | `	const char *zIn,*zEnd;` |
|      - | 1987 | `	int nLen,c;` |
|      - | 1988 | `	sxu32 n;` |
|     11 | 1989 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1990 | `		/* Missing/Invalid arguments,return NULL */` |
|      7 | 1991 | `		ph7_result_null(pCtx);` |
|      7 | 1992 | `		return PH7_OK;` |
|      - | 1993 | `	}` |
|      - | 1994 | `	/* Extract the target string */` |
|      5 | 1995 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 1996 | `	zEnd = &zIn[nLen];` |
|      - | 1997 | `	/* Extract the flags if available */` |
|      5 | 1998 | `	if( nArg > 1 ){` |
|      3 | 1999 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      3 | 2000 | `		if( iFlags < 0 ){` |
|      3 | 2001 | `			iFlags = 0x01;` |
|      1 | 2002 | `		}` |
|      1 | 2003 | `	}` |
|      - | 2004 | `	/* Perform the requested operation */` |
|     11 | 2005 | `	for(;;){` |
|     23 | 2006 | `		if( zIn >= zEnd ){` |
|      - | 2007 | `			/* No more input to process */` |
|      5 | 2008 | `			break;` |
|      - | 2009 | `		}` |
|     19 | 2010 | `		c = zIn[0];` |
|      - | 2011 | `		/* Perform a linear lookup on the decoding table */` |
|    233 | 2012 | `		for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    223 | 2013 | `			if( azHtmlEscape[n+1][0] == c ){` |
|      - | 2014 | `				/* Got one */` |
|      9 | 2015 | `				break;` |
|      - | 2016 | `			}` |
|    108 | 2017 | `		}` |
|     19 | 2018 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|      - | 2019 | `			/* Output the safe sequence [i.e: '<' ==> '&lt;"] */` |
|      9 | 2020 | `			if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 2021 | `				/* Expand the double quote verbatim */` |
|    ! 0 | 2022 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      9 | 2023 | `			}else if(c == '\'' && ((iFlags & 0x02 /*ENT_QUOTES*/) == 0 \|\| (iFlags & 0x04) /*ENT_NOQUOTES*/) ){` |
|      - | 2024 | `				/* expand single quote verbatim */` |
|    ! 0 | 2025 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|    ! 0 | 2026 | `			}else{` |
|      9 | 2027 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1/*Compute length automatically */);` |
|      - | 2028 | `			}` |
|      5 | 2029 | `		}else{` |
|      - | 2030 | `			/* Output character verbatim */` |
|     11 | 2031 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2032 | `		}` |
|     19 | 2033 | `		zIn++;` |
|      1 | 2034 | `	}` |
|      5 | 2035 | `	return PH7_OK;` |
|      6 | 2036 |  |
|      - | 2037 | `/*` |
|      - | 2038 | ` * string html_entity_decode(string $string [, int $quote_style = ENT_COMPAT [, string $charset = 'UTF-8' ]])` |
|      - | 2039 | ` *   Perform the reverse operation of html_entity_decode().` |
|      - | 2040 | ` * Parameters` |
|      - | 2041 | ` * $string` |
|      - | 2042 | ` *   The input string.` |
|      - | 2043 | ` * $flags` |
|      - | 2044 | ` *  A bitmask of one or more of the flags (see comment on PH7_builtin_htmlspecialchars())` |
|      - | 2045 | ` * Return` |
|      - | 2046 | ` * The decoded string.` |
|      - | 2047 | ` */` |
|     28 | 2048 | `static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2049 |  |
|      - | 2050 | `	const char *zCur,*zIn,*zEnd;` |
|     29 | 2051 | `	int iFlags = 0x01; /* ENT_COMPAT  */` |
|      - | 2052 | `	int nLen;` |
|      - | 2053 | `	sxu32 n;` |
|     29 | 2054 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2055 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 2056 | `		ph7_result_null(pCtx);` |
|      5 | 2057 | `		return PH7_OK;` |
|      - | 2058 | `	}` |
|      - | 2059 | `	/* Extract the target string */` |
|     25 | 2060 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 2061 | `	zEnd = &zIn[nLen];` |
|      - | 2062 | `	/* Extract the flags if available */` |
|     25 | 2063 | `	if( nArg > 1 ){` |
|     15 | 2064 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     15 | 2065 | `		if( iFlags < 0 ){` |
|      3 | 2066 | `			iFlags = 0x01;` |
|      1 | 2067 | `		}` |
|      7 | 2068 | `	}` |
|      - | 2069 | `	/* Perform the requested operation */` |
|     27 | 2070 | `	for(;;){` |
|     55 | 2071 | `		if( zIn >= zEnd ){` |
|      - | 2072 | `			/* No more input to process */` |
|     13 | 2073 | `			break;` |
|      - | 2074 | `		}` |
|     43 | 2075 | `		zCur = zIn;` |
|    173 | 2076 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|    131 | 2077 | `			zIn++;` |
|      1 | 2078 | `		}` |
|     43 | 2079 | `		if( zCur < zIn ){` |
|      - | 2080 | `			/* Append raw string verbatim */` |
|     27 | 2081 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     13 | 2082 | `		}` |
|     43 | 2083 | `		if( zIn >= zEnd ){` |
|     13 | 2084 | `			break;` |
|      - | 2085 | `		}` |
|     31 | 2086 | `		nLen = (int)(zEnd-zIn);` |
|      - | 2087 | `		/* Find an encoded sequence */` |
|    113 | 2088 | `		for(n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    113 | 2089 | `			int iLen = (int)SyStrlen(azHtmlEscape[n]);` |
|    113 | 2090 | `			if( nLen >= iLen && SyStrnicmp(zIn,azHtmlEscape[n],(sxu32)iLen) == 0 ){` |
|      - | 2091 | `				/* Got one */` |
|     31 | 2092 | `				zIn += iLen;` |
|     31 | 2093 | `				break;` |
|      - | 2094 | `			}` |
|     42 | 2095 | `		}` |
|     31 | 2096 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|     31 | 2097 | `			int c = azHtmlEscape[n+1][0];` |
|      - | 2098 | `			/* Output the decoded character */` |
|     31 | 2099 | `			if( c == '\'' && ((iFlags & 0x02) == 0 /*ENT_QUOTES*/\|\| (iFlags & 0x04) /*ENT_NOQUOTES*/)  ){` |
|      - | 2100 | `				/* Do not process single quotes */` |
|      9 | 2101 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|     27 | 2102 | `			}else if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 2103 | `				/* Do not process double quotes */` |
|      5 | 2104 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|      3 | 2105 | `			}else{` |
|     19 | 2106 | `				ph7_result_string(pCtx,azHtmlEscape[n+1],-1); /* Compute length automatically */` |
|      - | 2107 | `			}` |
|     16 | 2108 | `		}else{` |
|      - | 2109 | `			/* Append '&' */` |
|    ! 0 | 2110 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 2111 | `			zIn++;` |
|      - | 2112 | `		}` |
|      1 | 2113 | `	}` |
|     25 | 2114 | `	return PH7_OK;` |
|     15 | 2115 |  |
|      - | 2116 | `/*` |
|      - | 2117 | ` * int strlen($string)` |
|      - | 2118 | ` *  return the length of the given string.` |
|      - | 2119 | ` * Parameter` |
|      - | 2120 | ` *  string: The string being measured for length.` |
|      - | 2121 | ` * Return` |
|      - | 2122 | ` *  length of the given string.` |
|      - | 2123 | ` */` |
|   1508 | 2124 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2125 |  |
|   1510 | 2126 | `	int iLen = 0;` |
|   1510 | 2127 | `	if( nArg > 0 ){` |
|   1508 | 2128 | `		ph7_value_to_string(apArg[0],&iLen);` |
|    753 | 2129 | `	}` |
|      - | 2130 | `	/* String length */` |
|   1510 | 2131 | `	ph7_result_int(pCtx,iLen);` |
|   1510 | 2132 | `	return PH7_OK;` |
|      2 | 2133 |  |
|      - | 2134 | `/*` |
|      - | 2135 | ` * int strcmp(string $str1,string $str2)` |
|      - | 2136 | ` *  Perform a binary safe string comparison.` |
|      - | 2137 | ` * Parameter` |
|      - | 2138 | ` *  str1: The first string` |
|      - | 2139 | ` *  str2: The second string` |
|      - | 2140 | ` * Return` |
|      - | 2141 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2142 | ` *  than str2, and 0 if they are equal.` |
|      - | 2143 | ` */` |
|     50 | 2144 | `static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2145 |  |
|      - | 2146 | `	const char *z1,*z2;` |
|      - | 2147 | `	int n1,n2;` |
|      - | 2148 | `	int res;` |
|     51 | 2149 | `	if( nArg < 2 ){` |
|      5 | 2150 | `		res = nArg == 0 ? 0 : 1;` |
|      5 | 2151 | `		ph7_result_int(pCtx,res);` |
|      5 | 2152 | `		return PH7_OK;` |
|      - | 2153 | `	}` |
|      - | 2154 | `	/* Perform the comparison */` |
|     47 | 2155 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     47 | 2156 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     47 | 2157 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2158 | `	/* Comparison result */` |
|     47 | 2159 | `	ph7_result_int(pCtx,res);` |
|     47 | 2160 | `	return PH7_OK;` |
|     26 | 2161 |  |
|      - | 2162 | `/*` |
|      - | 2163 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 2164 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 2165 | ` * Parameter` |
|      - | 2166 | ` *  str1: The first string` |
|      - | 2167 | ` *  str2: The second string` |
|      - | 2168 | ` * Return` |
|      - | 2169 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2170 | ` *  than str2, and 0 if they are equal.` |
|      - | 2171 | ` */` |
|     20 | 2172 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2173 |  |
|      - | 2174 | `	const char *z1,*z2;` |
|      - | 2175 | `	int res;` |
|      - | 2176 | `	int n;` |
|     21 | 2177 | `	if( nArg < 3 ){` |
|      - | 2178 | `		/* Perform a standard comparison */` |
|      5 | 2179 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 2180 | `	}` |
|      - | 2181 | `	/* Desired comparison length */` |
|     17 | 2182 | `	n  = ph7_value_to_int(apArg[2]);` |
|     17 | 2183 | `	if( n < 0 ){` |
|      - | 2184 | `		/* Invalid length */` |
|      3 | 2185 | `		ph7_result_int(pCtx,-1);` |
|      3 | 2186 | `		return PH7_OK;` |
|      - | 2187 | `	}` |
|      - | 2188 | `	/* Perform the comparison */` |
|     15 | 2189 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     15 | 2190 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     15 | 2191 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 2192 | `	/* Comparison result */` |
|     15 | 2193 | `	ph7_result_int(pCtx,res);` |
|     15 | 2194 | `	return PH7_OK;` |
|     11 | 2195 |  |
|      - | 2196 | `/*` |
|      - | 2197 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 2198 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 2199 | ` * Parameter` |
|      - | 2200 | ` *  str1: The first string` |
|      - | 2201 | ` *  str2: The second string` |
|      - | 2202 | ` * Return` |
|      - | 2203 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2204 | ` *  than str2, and 0 if they are equal.` |
|      - | 2205 | ` */` |
|     18 | 2206 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2207 |  |
|      - | 2208 | `	const char *z1,*z2;` |
|      - | 2209 | `	int n1,n2;` |
|      - | 2210 | `	int res;` |
|     19 | 2211 | `	if( nArg < 2 ){` |
|      9 | 2212 | `		res = nArg == 0 ? 0 : 1;` |
|      9 | 2213 | `		ph7_result_int(pCtx,res);` |
|      9 | 2214 | `		return PH7_OK;` |
|      - | 2215 | `	}` |
|      - | 2216 | `	/* Perform the comparison */` |
|     11 | 2217 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     11 | 2218 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     11 | 2219 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2220 | `	/* Comparison result */` |
|     11 | 2221 | `	ph7_result_int(pCtx,res);` |
|     11 | 2222 | `	return PH7_OK;` |
|     10 | 2223 |  |
|      - | 2224 | `/*` |
|      - | 2225 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 2226 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 2227 | ` * Parameter` |
|      - | 2228 | ` *  $str1: The first string` |
|      - | 2229 | ` *  $str2: The second string` |
|      - | 2230 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 2231 | ` * Return` |
|      - | 2232 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2233 | ` *  than str2, and 0 if they are equal.` |
|      - | 2234 | ` */` |
|      8 | 2235 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2236 |  |
|      - | 2237 | `	const char *z1,*z2;` |
|      - | 2238 | `	int res;` |
|      - | 2239 | `	int n;` |
|      9 | 2240 | `	if( nArg < 3 ){` |
|      - | 2241 | `		/* Perform a standard comparison */` |
|      5 | 2242 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 2243 | `	}` |
|      - | 2244 | `	/* Desired comparison length */` |
|      5 | 2245 | `	n  = ph7_value_to_int(apArg[2]);` |
|      5 | 2246 | `	if( n < 0 ){` |
|      - | 2247 | `		/* Invalid length */` |
|    ! 0 | 2248 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2249 | `		return PH7_OK;` |
|      - | 2250 | `	}` |
|      - | 2251 | `	/* Perform the comparison */` |
|      5 | 2252 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|      5 | 2253 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|      5 | 2254 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 2255 | `	/* Comparison result */` |
|      5 | 2256 | `	ph7_result_int(pCtx,res);` |
|      5 | 2257 | `	return PH7_OK;` |
|      5 | 2258 |  |
|      - | 2259 | `/*` |
|      - | 2260 | ` * Implode context [i.e: it's private data].` |
|      - | 2261 | ` * A pointer to the following structure is forwarded` |
|      - | 2262 | ` * verbatim to the array walker callback defined below.` |
|      - | 2263 | ` */` |
|      - | 2264 | `struct implode_data {` |
|      - | 2265 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 2266 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 2267 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 2268 | `	int nSeplen;          /* Separator length */` |
|      - | 2269 | `	int bFirst;           /* TRUE if first call */` |
|      - | 2270 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 2271 | `};` |
|      - | 2272 | `/*` |
|      - | 2273 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 2274 | ` * The following routine is invoked for each array entry passed` |
|      - | 2275 | ` * to the implode() function.` |
|      - | 2276 | ` */` |
|  78794 | 2277 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      2 | 2278 |  |
|  39397 | 2279 | `	SXUNUSED(pKey);` |
|  78796 | 2280 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 2281 | `	const char *zData;` |
|      - | 2282 | `	int nLen;` |
|  78796 | 2283 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 2284 | `		if( pData->nSeplen > 0 ){` |
|      3 | 2285 | `			if( !pData->bFirst ){` |
|      - | 2286 | `				/* append the separator first */` |
|      3 | 2287 | `				ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|      2 | 2288 | `			}else{` |
|    ! 0 | 2289 | `				pData->bFirst = 0;` |
|      - | 2290 | `			}` |
|      1 | 2291 | `		}` |
|      - | 2292 | `		/* Recurse */` |
|      3 | 2293 | `		pData->bFirst = 1;` |
|      3 | 2294 | `		pData->nRecCount++;` |
|      3 | 2295 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 2296 | `		pData->nRecCount--;` |
|      3 | 2297 | `		return PH7_OK;` |
|      - | 2298 | `	}` |
|      - | 2299 | `	/* Extract the string representation of the entry value */` |
|  78794 | 2300 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 2301 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
|  78794 | 2302 | `	if( pData->bFirst ){` |
|  16968 | 2303 | `		pData->bFirst = 0;` |
|  70311 | 2304 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 2305 | `		/* append the separator first */` |
|  61816 | 2306 | `		ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|  30907 | 2307 | `	}` |
|      - | 2308 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
|  78794 | 2309 | `	if( nLen > 0 ){` |
|  71876 | 2310 | `		ph7_result_string(pData->pCtx,zData,nLen);` |
|  35937 | 2311 | `	}` |
|  78794 | 2312 | `	return PH7_OK;` |
|  39399 | 2313 |  |
|      - | 2314 | `/*` |
|      - | 2315 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 2316 | ` * string implode(array $pieces,...)` |
|      - | 2317 | ` *  Join array elements with a string.` |
|      - | 2318 | ` * $glue` |
|      - | 2319 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 2320 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 2321 | ` * $pieces` |
|      - | 2322 | ` *   The array of strings to implode.` |
|      - | 2323 | ` * Return` |
|      - | 2324 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 2325 | ` *  order, with the glue string between each element.` |
|      - | 2326 | ` */` |
|  16994 | 2327 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2328 |  |
|      - | 2329 | `	struct implode_data imp_data;` |
|  16996 | 2330 | `	int i = 1;` |
|  16996 | 2331 | `	if( nArg < 1 ){` |
|      - | 2332 | `		/* Missing argument,return NULL */` |
|    ! 0 | 2333 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2334 | `		return PH7_OK;` |
|      - | 2335 | `	}` |
|      - | 2336 | `	/* Prepare the implode context */` |
|  16996 | 2337 | `	imp_data.pCtx = pCtx;` |
|  16996 | 2338 | `	imp_data.bRecursive = 0;` |
|  16996 | 2339 | `	imp_data.bFirst = 1;` |
|  16996 | 2340 | `	imp_data.nRecCount = 0;` |
|  16996 | 2341 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  16994 | 2342 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|   8498 | 2343 | `	}else{` |
|      3 | 2344 | `		imp_data.zSep = 0;` |
|      3 | 2345 | `		imp_data.nSeplen = 0;` |
|      3 | 2346 | `		i = 0;` |
|      - | 2347 | `	}` |
|  16996 | 2348 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 2349 | `	/* Start the 'join' process */` |
|  33990 | 2350 | `	while( i < nArg ){` |
|  16996 | 2351 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2352 | `			/* Iterate throw array entries */` |
|  16996 | 2353 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|   8499 | 2354 | `		}else{` |
|      - | 2355 | `			const char *zData;` |
|      - | 2356 | `			int nLen;` |
|      - | 2357 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 2358 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2359 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 2360 | `			if( imp_data.bFirst ){` |
|    ! 0 | 2361 | `				imp_data.bFirst = 0;` |
|    ! 0 | 2362 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2363 | `				ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen);` |
|    ! 0 | 2364 | `			}` |
|      - | 2365 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 2366 | `			if( nLen > 0 ){` |
|    ! 0 | 2367 | `				ph7_result_string(pCtx,zData,nLen);` |
|    ! 0 | 2368 | `			}` |
|      - | 2369 | `		}` |
|  16996 | 2370 | `		i++;` |
|      2 | 2371 | `	}` |
|  16996 | 2372 | `	return PH7_OK;` |
|   8499 | 2373 |  |
|      - | 2374 | `/*` |
|      - | 2375 | ` * Symisc eXtension:` |
|      - | 2376 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 2377 | ` * Purpose` |
|      - | 2378 | ` *  Same as implode() but recurse on arrays.` |
|      - | 2379 | ` * Example:` |
|      - | 2380 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 2381 | ` *   echo implode_recursive("/",$a);` |
|      - | 2382 | ` *   Will output` |
|      - | 2383 | ` *     usr/home/dean.` |
|      - | 2384 | ` *   While the standard implode would produce.` |
|      - | 2385 | ` *    usr/Array.` |
|      - | 2386 | ` * Parameter` |
|      - | 2387 | ` *  Refer to implode().` |
|      - | 2388 | ` * Return` |
|      - | 2389 | ` *  Refer to implode().` |
|      - | 2390 | ` */` |
|     12 | 2391 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2392 |  |
|      - | 2393 | `	struct implode_data imp_data;` |
|     13 | 2394 | `	int i = 1;` |
|     13 | 2395 | `	if( nArg < 1 ){` |
|      - | 2396 | `		/* Missing argument,return NULL */` |
|      3 | 2397 | `		ph7_result_null(pCtx);` |
|      3 | 2398 | `		return PH7_OK;` |
|      - | 2399 | `	}` |
|      - | 2400 | `	/* Prepare the implode context */` |
|     11 | 2401 | `	imp_data.pCtx = pCtx;` |
|     11 | 2402 | `	imp_data.bRecursive = 1;` |
|     11 | 2403 | `	imp_data.bFirst = 1;` |
|     11 | 2404 | `	imp_data.nRecCount = 0;` |
|     11 | 2405 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 2406 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 2407 | `	}else{` |
|    ! 0 | 2408 | `		imp_data.zSep = 0;` |
|    ! 0 | 2409 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 2410 | `		i = 0;` |
|      - | 2411 | `	}` |
|     11 | 2412 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 2413 | `	/* Start the 'join' process */` |
|     21 | 2414 | `	while( i < nArg ){` |
|     11 | 2415 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2416 | `			/* Iterate throw array entries */` |
|      3 | 2417 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      2 | 2418 | `		}else{` |
|      - | 2419 | `			const char *zData;` |
|      - | 2420 | `			int nLen;` |
|      - | 2421 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 2422 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2423 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 2424 | `			if( imp_data.bFirst ){` |
|      9 | 2425 | `				imp_data.bFirst = 0;` |
|      4 | 2426 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2427 | `				ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen);` |
|    ! 0 | 2428 | `			}` |
|      - | 2429 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 2430 | `			if( nLen > 0 ){` |
|      9 | 2431 | `				ph7_result_string(pCtx,zData,nLen);` |
|      4 | 2432 | `			}` |
|      - | 2433 | `		}` |
|     11 | 2434 | `		i++;` |
|      1 | 2435 | `	}` |
|     11 | 2436 | `	return PH7_OK;` |
|      7 | 2437 |  |
|      - | 2438 | `/*` |
|      - | 2439 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 2440 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 2441 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 2442 | ` * Parameters` |
|      - | 2443 | ` *  $delimiter` |
|      - | 2444 | ` *   The boundary string.` |
|      - | 2445 | ` * $string` |
|      - | 2446 | ` *   The input string.` |
|      - | 2447 | ` * $limit` |
|      - | 2448 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 2449 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 2450 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 2451 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 2452 | ` * Returns` |
|      - | 2453 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 2454 | ` *  on boundaries formed by the delimiter.` |
|      - | 2455 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 2456 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 2457 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 2458 | ` *  will be returned.` |
|      - | 2459 | ` * NOTE:` |
|      - | 2460 | ` *  Negative limit is not supported.` |
|      - | 2461 | ` */` |
|   3068 | 2462 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2463 |  |
|      - | 2464 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 2465 | `	int nDelim,nStrlen,iLimit;` |
|      - | 2466 | `	ph7_value *pArray;` |
|      - | 2467 | `	ph7_value *pValue;` |
|      - | 2468 | `	sxu32 nOfft;` |
|      - | 2469 | `	sxi32 rc;` |
|   3070 | 2470 | `	if( nArg < 2 ){` |
|      - | 2471 | `		/* Missing arguments,return FALSE */` |
|      9 | 2472 | `		ph7_result_bool(pCtx,0);` |
|      9 | 2473 | `		return PH7_OK;` |
|      - | 2474 | `	}` |
|      - | 2475 | `	/* Extract the delimiter */` |
|   3062 | 2476 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   3062 | 2477 | `	if( nDelim < 1 ){` |
|      - | 2478 | `		/* Empty delimiter,return FALSE */` |
|      3 | 2479 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2480 | `		return PH7_OK;` |
|      - | 2481 | `	}` |
|      - | 2482 | `	/* Extract the string */` |
|   3060 | 2483 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   3060 | 2484 | `	if( nStrlen < 1 ){` |
|      - | 2485 | `		/* Empty string: return an array with a single empty element (PHP behavior) */` |
|      3 | 2486 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      3 | 2487 | `		ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      3 | 2488 | `		if( pArrayTmp == 0 \|\| pValueTmp == 0 ){` |
|      - | 2489 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 2490 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2491 | `			return PH7_OK;` |
|      - | 2492 | `		}` |
|      3 | 2493 | `		ph7_value_string(pValueTmp, "", 0);` |
|      3 | 2494 | `		ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp);` |
|      3 | 2495 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      3 | 2496 | `		return PH7_OK;` |
|      - | 2497 | `	}` |
|      - | 2498 | `	/* Point to the end of the string */` |
|   3058 | 2499 | `	zEnd = &zString[nStrlen];` |
|      - | 2500 | `	/* Create the array */` |
|   3058 | 2501 | `	pArray =  ph7_context_new_array(pCtx);` |
|   3058 | 2502 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   3058 | 2503 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2504 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 2505 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2506 | `		return PH7_OK;` |
|      - | 2507 | `	}` |
|      - | 2508 | `	/* Set a defualt limit */` |
|   3058 | 2509 | `	iLimit = SXI32_HIGH;` |
|   3058 | 2510 | `	if( nArg > 2 ){` |
|      9 | 2511 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|      9 | 2512 | `		 if( iLimit < 0 ){` |
|      3 | 2513 | `			iLimit = -iLimit;` |
|      1 | 2514 | `		}` |
|      9 | 2515 | `		if( iLimit == 0 ){` |
|      3 | 2516 | `			iLimit = 1;` |
|      1 | 2517 | `		}` |
|      9 | 2518 | `		iLimit--;` |
|      4 | 2519 | `	}` |
|      - | 2520 | `	/* Start exploding */` |
|  37668 | 2521 | `	for(;;){` |
|  75338 | 2522 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
|  75338 | 2523 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 2524 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   3058 | 2525 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   3058 | 2526 | `			ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|   3058 | 2527 | `			break;` |
|      - | 2528 | `		}` |
|      - | 2529 | `		/* Point to the desired offset */` |
|  72282 | 2530 | `		zCur = &zString[nOfft];` |
|      - | 2531 | `		/* Perform the store operation (may be empty) */` |
|  72282 | 2532 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|  72282 | 2533 | `		ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|      - | 2534 | `		/* Point beyond the delimiter */` |
|  72282 | 2535 | `		zString = &zCur[nDelim];` |
|      - | 2536 | `		/* Reset the cursor */` |
|  72282 | 2537 | `		ph7_value_reset_string_cursor(pValue);` |
|      2 | 2538 | `	}` |
|      - | 2539 | `	/* Return the freshly created array */` |
|   3058 | 2540 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2541 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 2542 | `	 * released as soon we return from this foregin function.` |
|      - | 2543 | `	 */` |
|   3058 | 2544 | `	return PH7_OK;` |
|   1536 | 2545 |  |
|      - | 2546 | `/*` |
|      - | 2547 | ` * string trim(string $str[,string $charlist ])` |
|      - | 2548 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2549 | ` * Parameters` |
|      - | 2550 | ` *  $str` |
|      - | 2551 | ` *   The string that will be trimmed.` |
|      - | 2552 | ` * $charlist` |
|      - | 2553 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2554 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2555 | ` *   With .. you can specify a range of characters.` |
|      - | 2556 | ` * Returns.` |
|      - | 2557 | ` *  Thr processed string.` |
|      - | 2558 | ` * NOTE:` |
|      - | 2559 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 2560 | ` */` |
|   7746 | 2561 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2562 |  |
|      - | 2563 | `	const char *zString;` |
|      - | 2564 | `	int nLen;` |
|   7748 | 2565 | `	if( nArg < 1 ){` |
|      - | 2566 | `		/* Missing arguments,return null */` |
|      3 | 2567 | `		ph7_result_null(pCtx);` |
|      3 | 2568 | `		return PH7_OK;` |
|      - | 2569 | `	}` |
|      - | 2570 | `	/* Extract the target string */` |
|   7746 | 2571 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|   7746 | 2572 | `	if( nLen < 1 ){` |
|      - | 2573 | `		/* Empty string,return */` |
|   1668 | 2574 | `		ph7_result_string(pCtx,"",0);` |
|   1668 | 2575 | `		return PH7_OK;` |
|      - | 2576 | `	}` |
|      - | 2577 | `	/* Start the trim process */` |
|   6080 | 2578 | `	if( nArg < 2 ){` |
|      - | 2579 | `		SyString sStr;` |
|      - | 2580 | `		/* Remove white spaces and NUL bytes */` |
|   6076 | 2581 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  14752 | 2582 | `		SyStringFullTrimSafe(&sStr);` |
|   6076 | 2583 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   3039 | 2584 | `	}else{` |
|      - | 2585 | `		/* Char list */` |
|      - | 2586 | `		const char *zList;` |
|      - | 2587 | `		int nListlen;` |
|      5 | 2588 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 2589 | `		if( nListlen < 1 ){` |
|      - | 2590 | `			/* Return the string unchanged */` |
|      3 | 2591 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2592 | `		}else{` |
|      3 | 2593 | `			const char *zEnd = &zString[nLen];` |
|      3 | 2594 | `			const char *zCur = zString;` |
|      - | 2595 | `			const char *zPtr;` |
|      - | 2596 | `			int i;` |
|      - | 2597 | `			/* Left trim */` |
|      4 | 2598 | `			for(;;){` |
|      9 | 2599 | `				if( zCur >= zEnd ){` |
|    ! 0 | 2600 | `					break;` |
|      - | 2601 | `				}` |
|      9 | 2602 | `				zPtr = zCur;` |
|     17 | 2603 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 2604 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|      7 | 2605 | `						zCur++;` |
|      3 | 2606 | `					}` |
|      5 | 2607 | `				}` |
|      9 | 2608 | `				if( zCur == zPtr ){` |
|      - | 2609 | `					/* No match,break immediately */` |
|      3 | 2610 | `					break;` |
|      - | 2611 | `				}` |
|      1 | 2612 | `			}` |
|      - | 2613 | `			/* Right trim */` |
|      3 | 2614 | `			zEnd--;` |
|      4 | 2615 | `			for(;;){` |
|      9 | 2616 | `				if( zEnd <= zCur ){` |
|    ! 0 | 2617 | `					break;` |
|      - | 2618 | `				}` |
|      9 | 2619 | `				zPtr = zEnd;` |
|     17 | 2620 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 2621 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      7 | 2622 | `						zEnd--;` |
|      3 | 2623 | `					}` |
|      5 | 2624 | `				}` |
|      9 | 2625 | `				if( zEnd == zPtr ){` |
|      3 | 2626 | `					break;` |
|      - | 2627 | `				}` |
|      1 | 2628 | `			}` |
|      3 | 2629 | `			if( zCur >= zEnd ){` |
|      - | 2630 | `				/* Return the empty string */` |
|    ! 0 | 2631 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2632 | `			}else{` |
|      3 | 2633 | `				zEnd++;` |
|      3 | 2634 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2635 | `			}` |
|      - | 2636 | `		}` |
|      - | 2637 | `	}` |
|   6080 | 2638 | `	return PH7_OK;` |
|   3875 | 2639 |  |
|      - | 2640 | `/*` |
|      - | 2641 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 2642 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 2643 | ` * Parameters` |
|      - | 2644 | ` *  $str` |
|      - | 2645 | ` *   The string that will be trimmed.` |
|      - | 2646 | ` * $charlist` |
|      - | 2647 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2648 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2649 | ` *   With .. you can specify a range of characters.` |
|      - | 2650 | ` * Returns.` |
|      - | 2651 | ` *  Thr processed string.` |
|      - | 2652 | ` * NOTE:` |
|      - | 2653 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 2654 | ` */` |
|     26 | 2655 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2656 |  |
|      - | 2657 | `	const char *zString;` |
|      - | 2658 | `	int nLen;` |
|     27 | 2659 | `	if( nArg < 1 ){` |
|      - | 2660 | `		/* Missing arguments,return null */` |
|      3 | 2661 | `		ph7_result_null(pCtx);` |
|      3 | 2662 | `		return PH7_OK;` |
|      - | 2663 | `	}` |
|      - | 2664 | `	/* Extract the target string */` |
|     25 | 2665 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 2666 | `	if( nLen < 1 ){` |
|      - | 2667 | `		/* Empty string,return */` |
|      5 | 2668 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2669 | `		return PH7_OK;` |
|      - | 2670 | `	}` |
|      - | 2671 | `	/* Start the trim process */` |
|     21 | 2672 | `	if( nArg < 2 ){` |
|      - | 2673 | `		SyString sStr;` |
|      - | 2674 | `		/* Remove white spaces and NUL bytes*/` |
|     17 | 2675 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     41 | 2676 | `		SyStringRightTrimSafe(&sStr);` |
|     17 | 2677 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      9 | 2678 | `	}else{` |
|      - | 2679 | `		/* Char list */` |
|      - | 2680 | `		const char *zList;` |
|      - | 2681 | `		int nListlen;` |
|      5 | 2682 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 2683 | `		if( nListlen < 1 ){` |
|      - | 2684 | `			/* Return the string unchanged */` |
|    ! 0 | 2685 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 2686 | `		}else{` |
|      5 | 2687 | `			const char *zEnd = &zString[nLen - 1];` |
|      5 | 2688 | `			const char *zCur = zString;` |
|      - | 2689 | `			const char *zPtr;` |
|      - | 2690 | `			int i;` |
|      - | 2691 | `			/* Right trim */` |
|      6 | 2692 | `			for(;;){` |
|     13 | 2693 | `				if( zEnd <= zCur ){` |
|    ! 0 | 2694 | `					break;` |
|      - | 2695 | `				}` |
|     13 | 2696 | `				zPtr = zEnd;` |
|     25 | 2697 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     13 | 2698 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      9 | 2699 | `						zEnd--;` |
|      4 | 2700 | `					}` |
|      7 | 2701 | `				}` |
|     13 | 2702 | `				if( zEnd == zPtr ){` |
|      5 | 2703 | `					break;` |
|      - | 2704 | `				}` |
|      1 | 2705 | `			}` |
|      5 | 2706 | `			if( zEnd <= zCur ){` |
|      - | 2707 | `				/* Return the empty string */` |
|    ! 0 | 2708 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2709 | `			}else{` |
|      5 | 2710 | `				zEnd++;` |
|      5 | 2711 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2712 | `			}` |
|      - | 2713 | `		}` |
|      - | 2714 | `	}` |
|     21 | 2715 | `	return PH7_OK;` |
|     14 | 2716 |  |
|      - | 2717 | `/*` |
|      - | 2718 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 2719 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2720 | ` * Parameters` |
|      - | 2721 | ` *  $str` |
|      - | 2722 | ` *   The string that will be trimmed.` |
|      - | 2723 | ` * $charlist` |
|      - | 2724 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2725 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2726 | ` *   With .. you can specify a range of characters.` |
|      - | 2727 | ` * Returns.` |
|      - | 2728 | ` *  Thr processed string.` |
|      - | 2729 | ` * NOTE:` |
|      - | 2730 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 2731 | ` */` |
|     12 | 2732 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2733 |  |
|      - | 2734 | `	const char *zString;` |
|      - | 2735 | `	int nLen;` |
|     13 | 2736 | `	if( nArg < 1 ){` |
|      - | 2737 | `		/* Missing arguments,return null */` |
|      3 | 2738 | `		ph7_result_null(pCtx);` |
|      3 | 2739 | `		return PH7_OK;` |
|      - | 2740 | `	}` |
|      - | 2741 | `	/* Extract the target string */` |
|     11 | 2742 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 2743 | `	if( nLen < 1 ){` |
|      - | 2744 | `		/* Empty string,return */` |
|    ! 0 | 2745 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2746 | `		return PH7_OK;` |
|      - | 2747 | `	}` |
|      - | 2748 | `	/* Start the trim process */` |
|     11 | 2749 | `	if( nArg < 2 ){` |
|      - | 2750 | `		SyString sStr;` |
|      - | 2751 | `		/* Remove white spaces and NUL byte */` |
|      3 | 2752 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|      8 | 2753 | `		SyStringLeftTrimSafe(&sStr);` |
|      3 | 2754 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      2 | 2755 | `	}else{` |
|      - | 2756 | `		/* Char list */` |
|      - | 2757 | `		const char *zList;` |
|      - | 2758 | `		int nListlen;` |
|      9 | 2759 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      9 | 2760 | `		if( nListlen < 1 ){` |
|      - | 2761 | `			/* Return the string unchanged */` |
|      3 | 2762 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2763 | `		}else{` |
|      7 | 2764 | `			const char *zEnd = &zString[nLen];` |
|      7 | 2765 | `			const char *zCur = zString;` |
|      - | 2766 | `			const char *zPtr;` |
|      - | 2767 | `			int i;` |
|      - | 2768 | `			/* Left trim */` |
|      7 | 2769 | `			for(;;){` |
|     15 | 2770 | `				if( zCur >= zEnd ){` |
|    ! 0 | 2771 | `					break;` |
|      - | 2772 | `				}` |
|     15 | 2773 | `				zPtr = zCur;` |
|     41 | 2774 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     27 | 2775 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|     13 | 2776 | `						zCur++;` |
|      6 | 2777 | `					}` |
|     14 | 2778 | `				}` |
|     15 | 2779 | `				if( zCur == zPtr ){` |
|      - | 2780 | `					/* No match,break immediately */` |
|      7 | 2781 | `					break;` |
|      - | 2782 | `				}` |
|      1 | 2783 | `			}` |
|      7 | 2784 | `			if( zCur >= zEnd ){` |
|      - | 2785 | `				/* Return the empty string */` |
|    ! 0 | 2786 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2787 | `			}else{` |
|      7 | 2788 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2789 | `			}` |
|      - | 2790 | `		}` |
|      - | 2791 | `	}` |
|     11 | 2792 | `	return PH7_OK;` |
|      7 | 2793 |  |
|      - | 2794 | `/*` |
|      - | 2795 | ` * string strtolower(string $str)` |
|      - | 2796 | ` *  Make a string lowercase.` |
|      - | 2797 | ` * Parameters` |
|      - | 2798 | ` *  $str` |
|      - | 2799 | ` *   The input string.` |
|      - | 2800 | ` * Returns.` |
|      - | 2801 | ` *  The lowercased string.` |
|      - | 2802 | ` */` |
|  16856 | 2803 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2804 |  |
|      - | 2805 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2806 | `	int nLen;` |
|  16858 | 2807 | `	if( nArg < 1 ){` |
|      - | 2808 | `		/* Missing arguments,return null */` |
|      3 | 2809 | `		ph7_result_null(pCtx);` |
|      3 | 2810 | `		return PH7_OK;` |
|      - | 2811 | `	}` |
|      - | 2812 | `	/* Extract the target string */` |
|  16856 | 2813 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  16856 | 2814 | `	if( nLen < 1 ){` |
|      - | 2815 | `		/* Empty string,return */` |
|      3 | 2816 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2817 | `		return PH7_OK;` |
|      - | 2818 | `	}` |
|      - | 2819 | `	/* Perform the requested operation */` |
|  16854 | 2820 | `	zEnd = &zString[nLen];` |
|  53297 | 2821 | `	for(;;){` |
| 106596 | 2822 | `		if( zString >= zEnd ){` |
|      - | 2823 | `			/* No more input,break immediately */` |
|  16854 | 2824 | `			break;` |
|      - | 2825 | `		}` |
|  89744 | 2826 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2827 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2828 | `			zCur = zString;` |
|    ! 0 | 2829 | `			zString++;` |
|    ! 0 | 2830 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2831 | `				zString++;` |
|    ! 0 | 2832 | `			}` |
|      - | 2833 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2834 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2835 | `		}else{` |
|  89744 | 2836 | `			int c = zString[0];` |
|  89744 | 2837 | `			if( SyisUpper(c) ){` |
|  89742 | 2838 | `				c = SyToLower(zString[0]);` |
|  44870 | 2839 | `			}` |
|      - | 2840 | `			/* Append character */` |
|  89744 | 2841 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2842 | `			/* Advance the cursor */` |
|  89744 | 2843 | `			zString++;` |
|      - | 2844 | `		}` |
|      2 | 2845 | `	}` |
|  16854 | 2846 | `	return PH7_OK;` |
|   8430 | 2847 |  |
|      - | 2848 | `/*` |
|      - | 2849 | ` * string strtolower(string $str)` |
|      - | 2850 | ` *  Make a string uppercase.` |
|      - | 2851 | ` * Parameters` |
|      - | 2852 | ` *  $str` |
|      - | 2853 | ` *   The input string.` |
|      - | 2854 | ` * Returns.` |
|      - | 2855 | ` *  The uppercased string.` |
|      - | 2856 | ` */` |
|     10 | 2857 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2858 |  |
|      - | 2859 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2860 | `	int nLen;` |
|     11 | 2861 | `	if( nArg < 1 ){` |
|      - | 2862 | `		/* Missing arguments,return null */` |
|      3 | 2863 | `		ph7_result_null(pCtx);` |
|      3 | 2864 | `		return PH7_OK;` |
|      - | 2865 | `	}` |
|      - | 2866 | `	/* Extract the target string */` |
|      9 | 2867 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 2868 | `	if( nLen < 1 ){` |
|      - | 2869 | `		/* Empty string,return */` |
|      3 | 2870 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2871 | `		return PH7_OK;` |
|      - | 2872 | `	}` |
|      - | 2873 | `	/* Perform the requested operation */` |
|      7 | 2874 | `	zEnd = &zString[nLen];` |
|     19 | 2875 | `	for(;;){` |
|     39 | 2876 | `		if( zString >= zEnd ){` |
|      - | 2877 | `			/* No more input,break immediately */` |
|      7 | 2878 | `			break;` |
|      - | 2879 | `		}` |
|     33 | 2880 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2881 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2882 | `			zCur = zString;` |
|    ! 0 | 2883 | `			zString++;` |
|    ! 0 | 2884 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2885 | `				zString++;` |
|    ! 0 | 2886 | `			}` |
|      - | 2887 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2888 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2889 | `		}else{` |
|     33 | 2890 | `			int c = zString[0];` |
|     33 | 2891 | `			if( SyisLower(c) ){` |
|     27 | 2892 | `				c = SyToUpper(zString[0]);` |
|     13 | 2893 | `			}` |
|      - | 2894 | `			/* Append character */` |
|     33 | 2895 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2896 | `			/* Advance the cursor */` |
|     33 | 2897 | `			zString++;` |
|      - | 2898 | `		}` |
|      1 | 2899 | `	}` |
|      7 | 2900 | `	return PH7_OK;` |
|      6 | 2901 |  |
|      - | 2902 | `/*` |
|      - | 2903 | ` * string ucfirst(string $str)` |
|      - | 2904 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2905 | ` *  character is alphabetic.` |
|      - | 2906 | ` * Parameters` |
|      - | 2907 | ` *  $str` |
|      - | 2908 | ` *   The input string.` |
|      - | 2909 | ` * Returns.` |
|      - | 2910 | ` *  The processed string.` |
|      - | 2911 | ` */` |
|      6 | 2912 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2913 |  |
|      - | 2914 | `	const char *zString,*zEnd;` |
|      - | 2915 | `	int nLen,c;` |
|      7 | 2916 | `	if( nArg < 1 ){` |
|      - | 2917 | `		/* Missing arguments,return null */` |
|      3 | 2918 | `		ph7_result_null(pCtx);` |
|      3 | 2919 | `		return PH7_OK;` |
|      - | 2920 | `	}` |
|      - | 2921 | `	/* Extract the target string */` |
|      5 | 2922 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2923 | `	if( nLen < 1 ){` |
|      - | 2924 | `		/* Empty string,return */` |
|      3 | 2925 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2926 | `		return PH7_OK;` |
|      - | 2927 | `	}` |
|      - | 2928 | `	/* Perform the requested operation */` |
|      3 | 2929 | `	zEnd = &zString[nLen];` |
|      3 | 2930 | `	c = zString[0];` |
|      3 | 2931 | `	if( SyisLower(c) ){` |
|      3 | 2932 | `		c = SyToUpper(c);` |
|      1 | 2933 | `	}` |
|      - | 2934 | `	/* Append the first character */` |
|      3 | 2935 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2936 | `	zString++;` |
|      3 | 2937 | `	if( zString < zEnd ){` |
|      - | 2938 | `		/* Append the rest of the input verbatim */` |
|      3 | 2939 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2940 | `	}` |
|      3 | 2941 | `	return PH7_OK;` |
|      4 | 2942 |  |
|      - | 2943 | `/*` |
|      - | 2944 | ` * string lcfirst(string $str)` |
|      - | 2945 | ` *  Make a string's first character lowercase.` |
|      - | 2946 | ` * Parameters` |
|      - | 2947 | ` *  $str` |
|      - | 2948 | ` *   The input string.` |
|      - | 2949 | ` * Returns.` |
|      - | 2950 | ` *  The processed string.` |
|      - | 2951 | ` */` |
|      6 | 2952 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2953 |  |
|      - | 2954 | `	const char *zString,*zEnd;` |
|      - | 2955 | `	int nLen,c;` |
|      7 | 2956 | `	if( nArg < 1 ){` |
|      - | 2957 | `		/* Missing arguments,return null */` |
|      3 | 2958 | `		ph7_result_null(pCtx);` |
|      3 | 2959 | `		return PH7_OK;` |
|      - | 2960 | `	}` |
|      - | 2961 | `	/* Extract the target string */` |
|      5 | 2962 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2963 | `	if( nLen < 1 ){` |
|      - | 2964 | `		/* Empty string,return */` |
|      3 | 2965 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2966 | `		return PH7_OK;` |
|      - | 2967 | `	}` |
|      - | 2968 | `	/* Perform the requested operation */` |
|      3 | 2969 | `	zEnd = &zString[nLen];` |
|      3 | 2970 | `	c = zString[0];` |
|      3 | 2971 | `	if( SyisUpper(c) ){` |
|      3 | 2972 | `		c = SyToLower(c);` |
|      1 | 2973 | `	}` |
|      - | 2974 | `	/* Append the first character */` |
|      3 | 2975 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2976 | `	zString++;` |
|      3 | 2977 | `	if( zString < zEnd ){` |
|      - | 2978 | `		/* Append the rest of the input verbatim */` |
|      3 | 2979 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2980 | `	}` |
|      3 | 2981 | `	return PH7_OK;` |
|      4 | 2982 |  |
|      - | 2983 | `/*` |
|      - | 2984 | ` * int ord(string $string)` |
|      - | 2985 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 2986 | ` * Parameters` |
|      - | 2987 | ` *  $str` |
|      - | 2988 | ` *   The input string.` |
|      - | 2989 | ` * Returns.` |
|      - | 2990 | ` *  The ASCII value as an integer.` |
|      - | 2991 | ` */` |
|     32 | 2992 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2993 |  |
|      - | 2994 | `	const char *zString;` |
|      - | 2995 | `	int nLen,c;` |
|     33 | 2996 | `	if( nArg < 1 ){` |
|      - | 2997 | `		/* Missing arguments,return -1 */` |
|      3 | 2998 | `		ph7_result_int(pCtx,-1);` |
|      3 | 2999 | `		return PH7_OK;` |
|      - | 3000 | `	}` |
|      - | 3001 | `	/* Extract the target string */` |
|     31 | 3002 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 3003 | `	if( nLen < 1 ){` |
|      - | 3004 | `		/* Empty string,return -1 */` |
|      3 | 3005 | `		ph7_result_int(pCtx,-1);` |
|      3 | 3006 | `		return PH7_OK;` |
|      - | 3007 | `	}` |
|      - | 3008 | `	/* Extract the ASCII value of the first character */` |
|     29 | 3009 | `	c = zString[0];` |
|      - | 3010 | `	/* Return that value */` |
|     29 | 3011 | `	ph7_result_int(pCtx,c);` |
|     29 | 3012 | `	return PH7_OK;` |
|     17 | 3013 |  |
|      - | 3014 | `/*` |
|      - | 3015 | ` * string chr(int $ascii)` |
|      - | 3016 | ` *  Returns a one-character string containing the character specified by ascii.` |
|      - | 3017 | ` * Parameters` |
|      - | 3018 | ` *  $ascii` |
|      - | 3019 | ` *   The ascii code.` |
|      - | 3020 | ` * Returns.` |
|      - | 3021 | ` *  The specified character.` |
|      - | 3022 | ` */` |
|     28 | 3023 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3024 |  |
|      - | 3025 | `	int c;` |
|     29 | 3026 | `	if( nArg < 1 ){` |
|      - | 3027 | `		/* Missing arguments,return null */` |
|      3 | 3028 | `		ph7_result_null(pCtx);` |
|      3 | 3029 | `		return PH7_OK;` |
|      - | 3030 | `	}` |
|      - | 3031 | `	/* Extract the ASCII value */` |
|     27 | 3032 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 3033 | `	/* Return the specified character */` |
|     27 | 3034 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     27 | 3035 | `	return PH7_OK;` |
|     15 | 3036 |  |
|      - | 3037 | `/*` |
|      - | 3038 | ` * Binary to hex consumer callback.` |
|      - | 3039 | ` * This callback is the default consumer used by the hash functions` |
|      - | 3040 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 3041 | ` */` |
|    226 | 3042 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 3043 |  |
|      - | 3044 | `	/* Append hex chunk verbatim */` |
|    227 | 3045 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|    227 | 3046 | `	return SXRET_OK;` |
|      1 | 3047 |  |
|      - | 3048 |  |
|      - | 3049 | `/*` |
|      - | 3050 | ` * string bin2hex(string $str)` |
|      - | 3051 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 3052 | ` * Parameters` |
|      - | 3053 | ` *  $str` |
|      - | 3054 | ` *   The input string.` |
|      - | 3055 | ` * Returns.` |
|      - | 3056 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 3057 | ` */` |
|     12 | 3058 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3059 |  |
|      - | 3060 | `	const char *zString;` |
|      - | 3061 | `	int nLen;` |
|     13 | 3062 | `	if( nArg < 1 ){` |
|      - | 3063 | `		/* Missing arguments,return null */` |
|      3 | 3064 | `		ph7_result_null(pCtx);` |
|      3 | 3065 | `		return PH7_OK;` |
|      - | 3066 | `	}` |
|      - | 3067 | `	/* Extract the target string */` |
|     11 | 3068 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 3069 | `	if( nLen < 1 ){` |
|      - | 3070 | `		/* Empty string,return */` |
|      3 | 3071 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3072 | `		return PH7_OK;` |
|      - | 3073 | `	}` |
|      - | 3074 | `	/* Perform the requested operation */` |
|      9 | 3075 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|      9 | 3076 | `	return PH7_OK;` |
|      7 | 3077 |  |
|      - | 3078 |  |
|      - | 3079 | `/* Search callback signature */` |
|      - | 3080 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 3081 | `/*` |
|      - | 3082 | ` * Case-insensitive pattern match.` |
|      - | 3083 | ` * Brute force is the default search method used here.` |
|      - | 3084 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 3085 | ` * well for short/medium texts on modern hardware.` |
|      - | 3086 | ` */` |
|    118 | 3087 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      1 | 3088 |  |
|    119 | 3089 | `	const char *zpIn = (const char *)pPattern;` |
|    119 | 3090 | `	const char *zIn = (const char *)pText;` |
|    119 | 3091 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    119 | 3092 | `	const char *zEnd = &zIn[nLen];` |
|      - | 3093 | `	const char *zPtr,*zPtr2;` |
|      - | 3094 | `	int c,d;` |
|    119 | 3095 | `	if( iPatLen > nLen ){` |
|      - | 3096 | `		/* Don't bother processing */` |
|     33 | 3097 | `		return SXERR_NOTFOUND;` |
|      - | 3098 | `	}` |
|    244 | 3099 | `	for(;;){` |
|    489 | 3100 | `		if( zIn >= zEnd ){` |
|     47 | 3101 | `			break;` |
|      - | 3102 | `		}` |
|    443 | 3103 | `		c = SyToLower(zIn[0]);` |
|    443 | 3104 | `		d = SyToLower(zpIn[0]);` |
|    443 | 3105 | `		if( c == d ){` |
|     41 | 3106 | `			zPtr   = &zIn[1];` |
|     41 | 3107 | `			zPtr2  = &zpIn[1];` |
|     71 | 3108 | `			for(;;){` |
|    143 | 3109 | `				if( zPtr2 >= zpEnd ){` |
|      - | 3110 | `					/* Pattern found */` |
|     41 | 3111 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 3112 | `					return SXRET_OK;` |
|      - | 3113 | `				}` |
|    103 | 3114 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 3115 | `					break;` |
|      - | 3116 | `				}` |
|    103 | 3117 | `				c = SyToLower(zPtr[0]);` |
|    103 | 3118 | `				d = SyToLower(zPtr2[0]);` |
|    103 | 3119 | `				if( c != d ){` |
|    ! 0 | 3120 | `					break;` |
|      - | 3121 | `				}` |
|    103 | 3122 | `				zPtr++; zPtr2++;` |
|      1 | 3123 | `			}` |
|    ! 0 | 3124 | `		}` |
|    403 | 3125 | `		zIn++;` |
|      1 | 3126 | `	}` |
|      - | 3127 | `	/* Pattern not found */` |
|     47 | 3128 | `	return SXERR_NOTFOUND;` |
|     60 | 3129 |  |
|      - | 3130 | `/*` |
|      - | 3131 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3132 | ` *  Find the first occurrence of a string.` |
|      - | 3133 | ` * Parameters` |
|      - | 3134 | ` *  $haystack` |
|      - | 3135 | ` *   The input string.` |
|      - | 3136 | ` * $needle` |
|      - | 3137 | ` *   Search pattern (must be a string).` |
|      - | 3138 | ` * $before_needle` |
|      - | 3139 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3140 | ` *   of the needle (excluding the needle).` |
|      - | 3141 | ` * Return` |
|      - | 3142 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3143 | ` */` |
|     10 | 3144 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3145 |  |
|     11 | 3146 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3147 | `	const char *zBlob,*zPattern;` |
|      - | 3148 | `	int nLen,nPatLen;` |
|      - | 3149 | `	sxu32 nOfft;` |
|      - | 3150 | `	sxi32 rc;` |
|     11 | 3151 | `	if( nArg < 2 ){` |
|      - | 3152 | `		/* Missing arguments,return FALSE */` |
|      5 | 3153 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3154 | `		return PH7_OK;` |
|      - | 3155 | `	}` |
|      - | 3156 | `	/* Extract the needle and the haystack */` |
|      7 | 3157 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 3158 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 3159 | `	nOfft = 0; /* cc warning */` |
|      9 | 3160 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3161 | `		int before = 0;` |
|      - | 3162 | `		/* Perform the lookup */` |
|      5 | 3163 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3164 | `		if( rc != SXRET_OK ){` |
|      - | 3165 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3166 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3167 | `			return PH7_OK;` |
|      - | 3168 | `		}` |
|      - | 3169 | `		/* Return the portion of the string */` |
|      5 | 3170 | `		if( nArg > 2 ){` |
|      3 | 3171 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3172 | `		}` |
|      5 | 3173 | `		if( before ){` |
|      3 | 3174 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3175 | `		}else{` |
|      3 | 3176 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3177 | `		}` |
|      3 | 3178 | `	}else{` |
|      3 | 3179 | `		ph7_result_bool(pCtx,0);` |
|      - | 3180 | `	}` |
|      7 | 3181 | `	return PH7_OK;` |
|      6 | 3182 |  |
|      - | 3183 | `/*` |
|      - | 3184 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3185 | ` *  Case-insensitive strstr().` |
|      - | 3186 | ` * Parameters` |
|      - | 3187 | ` *  $haystack` |
|      - | 3188 | ` *   The input string.` |
|      - | 3189 | ` * $needle` |
|      - | 3190 | ` *   Search pattern (must be a string).` |
|      - | 3191 | ` * $before_needle` |
|      - | 3192 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3193 | ` *   of the needle (excluding the needle).` |
|      - | 3194 | ` * Return` |
|      - | 3195 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3196 | ` */` |
|      6 | 3197 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3198 |  |
|      7 | 3199 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3200 | `	const char *zBlob,*zPattern;` |
|      - | 3201 | `	int nLen,nPatLen;` |
|      - | 3202 | `	sxu32 nOfft;` |
|      - | 3203 | `	sxi32 rc;` |
|      7 | 3204 | `	if( nArg < 2 ){` |
|      - | 3205 | `		/* Missing arguments,return FALSE */` |
|      3 | 3206 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3207 | `		return PH7_OK;` |
|      - | 3208 | `	}` |
|      - | 3209 | `	/* Extract the needle and the haystack */` |
|      5 | 3210 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3211 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 3212 | `	nOfft = 0; /* cc warning */` |
|      7 | 3213 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3214 | `		int before = 0;` |
|      - | 3215 | `		/* Perform the lookup */` |
|      5 | 3216 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3217 | `		if( rc != SXRET_OK ){` |
|      - | 3218 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3219 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3220 | `			return PH7_OK;` |
|      - | 3221 | `		}` |
|      - | 3222 | `		/* Return the portion of the string */` |
|      5 | 3223 | `		if( nArg > 2 ){` |
|      3 | 3224 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3225 | `		}` |
|      5 | 3226 | `		if( before ){` |
|      3 | 3227 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3228 | `		}else{` |
|      3 | 3229 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3230 | `		}` |
|      3 | 3231 | `	}else{` |
|    ! 0 | 3232 | `		ph7_result_bool(pCtx,0);` |
|      - | 3233 | `	}` |
|      5 | 3234 | `	return PH7_OK;` |
|      4 | 3235 |  |
|      - | 3236 | `/*` |
|      - | 3237 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3238 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 3239 | ` * Parameters` |
|      - | 3240 | ` *  $haystack` |
|      - | 3241 | ` *   The input string.` |
|      - | 3242 | ` * $needle` |
|      - | 3243 | ` *   Search pattern (must be a string).` |
|      - | 3244 | ` * $offset` |
|      - | 3245 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3246 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3247 | ` *   of haystack.` |
|      - | 3248 | ` * Return` |
|      - | 3249 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3250 | ` */` |
|     80 | 3251 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3252 |  |
|     82 | 3253 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3254 | `	const char *zBlob,*zPattern;` |
|      - | 3255 | `	int nLen,nPatLen,nStart;` |
|      - | 3256 | `	sxu32 nOfft;` |
|      - | 3257 | `	sxi32 rc;` |
|     82 | 3258 | `	if( nArg < 2 ){` |
|      - | 3259 | `		/* Missing arguments,return FALSE */` |
|      7 | 3260 | `		ph7_result_bool(pCtx,0);` |
|      7 | 3261 | `		return PH7_OK;` |
|      - | 3262 | `	}` |
|      - | 3263 | `	/* Extract the needle and the haystack */` |
|     76 | 3264 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     76 | 3265 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     76 | 3266 | `	nOfft = 0; /* cc warning */` |
|     76 | 3267 | `	nStart = 0;` |
|      - | 3268 | `	/* Peek the starting offset if available */` |
|     76 | 3269 | `	if( nArg > 2 ){` |
|    ! 0 | 3270 | `		nStart = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 3271 | `		if( nStart < 0 ){` |
|    ! 0 | 3272 | `			nStart = -nStart;` |
|    ! 0 | 3273 | `		}` |
|    ! 0 | 3274 | `		if( nStart >= nLen ){` |
|      - | 3275 | `			/* Invalid offset */` |
|    ! 0 | 3276 | `			nStart = 0;` |
|    ! 0 | 3277 | `		}else{` |
|    ! 0 | 3278 | `			zBlob += nStart;` |
|    ! 0 | 3279 | `			nLen -= nStart;` |
|      - | 3280 | `		}` |
|    ! 0 | 3281 | `	}` |
|     76 | 3282 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3283 | `		/* Perform the lookup */` |
|     74 | 3284 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     74 | 3285 | `		if( rc != SXRET_OK ){` |
|      - | 3286 | `			/* Pattern not found,return FALSE */` |
|      3 | 3287 | `			ph7_result_bool(pCtx,0);` |
|      3 | 3288 | `			return PH7_OK;` |
|      - | 3289 | `		}` |
|      - | 3290 | `		/* Return the pattern position */` |
|     72 | 3291 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|     37 | 3292 | `	}else{` |
|      3 | 3293 | `		ph7_result_bool(pCtx,0);` |
|      - | 3294 | `	}` |
|     74 | 3295 | `	return PH7_OK;` |
|     42 | 3296 |  |
|      - | 3297 | `/*` |
|      - | 3298 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3299 | ` *  Case-insensitive strpos.` |
|      - | 3300 | ` * Parameters` |
|      - | 3301 | ` *  $haystack` |
|      - | 3302 | ` *   The input string.` |
|      - | 3303 | ` * $needle` |
|      - | 3304 | ` *   Search pattern (must be a string).` |
|      - | 3305 | ` * $offset` |
|      - | 3306 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3307 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3308 | ` *   of haystack.` |
|      - | 3309 | ` * Return` |
|      - | 3310 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3311 | ` */` |
|     18 | 3312 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3313 |  |
|     19 | 3314 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3315 | `	const char *zBlob,*zPattern;` |
|      - | 3316 | `	int nLen,nPatLen,nStart;` |
|      - | 3317 | `	sxu32 nOfft;` |
|      - | 3318 | `	sxi32 rc;` |
|     19 | 3319 | `	if( nArg < 2 ){` |
|      - | 3320 | `		/* Missing arguments,return FALSE */` |
|      3 | 3321 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3322 | `		return PH7_OK;` |
|      - | 3323 | `	}` |
|      - | 3324 | `	/* Extract the needle and the haystack */` |
|     17 | 3325 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 3326 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     17 | 3327 | `	nOfft = 0; /* cc warning */` |
|     17 | 3328 | `	nStart = 0;` |
|      - | 3329 | `	/* Peek the starting offset if available */` |
|     17 | 3330 | `	if( nArg > 2 ){` |
|      5 | 3331 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 3332 | `		if( nStart < 0 ){` |
|      3 | 3333 | `			nStart = -nStart;` |
|      1 | 3334 | `		}` |
|      5 | 3335 | `		if( nStart >= nLen ){` |
|      - | 3336 | `			/* Invalid offset */` |
|    ! 0 | 3337 | `			nStart = 0;` |
|    ! 0 | 3338 | `		}else{` |
|      5 | 3339 | `			zBlob += nStart;` |
|      5 | 3340 | `			nLen -= nStart;` |
|      - | 3341 | `		}` |
|      2 | 3342 | `	}` |
|     17 | 3343 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3344 | `		/* Perform the lookup */` |
|     17 | 3345 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     17 | 3346 | `		if( rc != SXRET_OK ){` |
|      - | 3347 | `			/* Pattern not found,return FALSE */` |
|      3 | 3348 | `			ph7_result_bool(pCtx,0);` |
|      3 | 3349 | `			return PH7_OK;` |
|      - | 3350 | `		}` |
|      - | 3351 | `		/* Return the pattern position */` |
|     15 | 3352 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 3353 | `	}else{` |
|    ! 0 | 3354 | `		ph7_result_bool(pCtx,0);` |
|      - | 3355 | `	}` |
|     15 | 3356 | `	return PH7_OK;` |
|     10 | 3357 |  |
|      - | 3358 | `/*` |
|      - | 3359 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3360 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 3361 | ` * Parameters` |
|      - | 3362 | ` *  $haystack` |
|      - | 3363 | ` *   The input string.` |
|      - | 3364 | ` * $needle` |
|      - | 3365 | ` *   Search pattern (must be a string).` |
|      - | 3366 | ` * $offset` |
|      - | 3367 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3368 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3369 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3370 | ` * Return` |
|      - | 3371 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3372 | ` */` |
|     32 | 3373 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3374 |  |
|      - | 3375 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     33 | 3376 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3377 | `	int nLen,nPatLen;` |
|      - | 3378 | `	sxu32 nOfft;` |
|      - | 3379 | `	sxi32 rc;` |
|     33 | 3380 | `	if( nArg < 2 ){` |
|      - | 3381 | `		/* Missing arguments,return FALSE */` |
|      3 | 3382 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3383 | `		return PH7_OK;` |
|      - | 3384 | `	}` |
|      - | 3385 | `	/* Extract the needle and the haystack */` |
|     31 | 3386 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 3387 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3388 | `	/* Point to the end of the pattern */` |
|     31 | 3389 | `	zPtr = &zBlob[nLen - 1];` |
|     31 | 3390 | `	zEnd = &zBlob[nLen];` |
|      - | 3391 | `	/* Save the starting posistion */` |
|     31 | 3392 | `	zStart = zBlob;` |
|     31 | 3393 | `	nOfft = 0; /* cc warning */` |
|      - | 3394 | `	/* Peek the starting offset if available */` |
|     31 | 3395 | `	if( nArg > 2 ){` |
|      - | 3396 | `		int nStart;` |
|     21 | 3397 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 3398 | `		if( nStart < 0 ){` |
|     11 | 3399 | `			nStart = -nStart;` |
|     11 | 3400 | `			if( nStart >= nLen ){` |
|      - | 3401 | `				/* Invalid offset */` |
|      3 | 3402 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3403 | `				return PH7_OK;` |
|    ! 0 | 3404 | `			}else{` |
|      9 | 3405 | `				nLen -= nStart;` |
|      9 | 3406 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 3407 | `				zEnd = &zBlob[nLen];` |
|      - | 3408 | `			}` |
|      5 | 3409 | `		}else{` |
|     11 | 3410 | `			if( nStart >= nLen ){` |
|      - | 3411 | `				/* Invalid offset */` |
|      5 | 3412 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3413 | `				return PH7_OK;` |
|    ! 0 | 3414 | `			}else{` |
|      7 | 3415 | `				zBlob += nStart;` |
|      7 | 3416 | `				nLen -= nStart;` |
|      - | 3417 | `			}` |
|      - | 3418 | `		}` |
|      7 | 3419 | `	}` |
|     25 | 3420 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3421 | `		/* Perform the lookup */` |
|     57 | 3422 | `		for(;;){` |
|    115 | 3423 | `			if( zBlob >= zPtr ){` |
|     11 | 3424 | `				break;` |
|      - | 3425 | `			}` |
|    105 | 3426 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    105 | 3427 | `			if( rc == SXRET_OK ){` |
|      - | 3428 | `				/* Pattern found,return it's position */` |
|     13 | 3429 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 3430 | `				return PH7_OK;` |
|      - | 3431 | `			}` |
|     93 | 3432 | `			zPtr--;` |
|      1 | 3433 | `		}` |
|      - | 3434 | `		/* Pattern not found,return FALSE */` |
|     11 | 3435 | `		ph7_result_bool(pCtx,0);` |
|      6 | 3436 | `	}else{` |
|      3 | 3437 | `		ph7_result_bool(pCtx,0);` |
|      - | 3438 | `	}` |
|     13 | 3439 | `	return PH7_OK;` |
|     17 | 3440 |  |
|      - | 3441 | `/*` |
|      - | 3442 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3443 | ` *  Case-insensitive strrpos.` |
|      - | 3444 | ` * Parameters` |
|      - | 3445 | ` *  $haystack` |
|      - | 3446 | ` *   The input string.` |
|      - | 3447 | ` * $needle` |
|      - | 3448 | ` *   Search pattern (must be a string).` |
|      - | 3449 | ` * $offset` |
|      - | 3450 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3451 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3452 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3453 | ` * Return` |
|      - | 3454 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3455 | ` */` |
|     28 | 3456 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3457 |  |
|      - | 3458 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     29 | 3459 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3460 | `	int nLen,nPatLen;` |
|      - | 3461 | `	sxu32 nOfft;` |
|      - | 3462 | `	sxi32 rc;` |
|     29 | 3463 | `	if( nArg < 2 ){` |
|      - | 3464 | `		/* Missing arguments,return FALSE */` |
|      3 | 3465 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3466 | `		return PH7_OK;` |
|      - | 3467 | `	}` |
|      - | 3468 | `	/* Extract the needle and the haystack */` |
|     27 | 3469 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 3470 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3471 | `	/* Point to the end of the pattern */` |
|     27 | 3472 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 3473 | `	zEnd = &zBlob[nLen];` |
|      - | 3474 | `	/* Save the starting posistion */` |
|     27 | 3475 | `	zStart = zBlob;` |
|     27 | 3476 | `	nOfft = 0; /* cc warning */` |
|      - | 3477 | `	/* Peek the starting offset if available */` |
|     27 | 3478 | `	if( nArg > 2 ){` |
|      - | 3479 | `		int nStart;` |
|     15 | 3480 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3481 | `		if( nStart < 0 ){` |
|      7 | 3482 | `			nStart = -nStart;` |
|      7 | 3483 | `			if( nStart >= nLen ){` |
|      - | 3484 | `				/* Invalid offset */` |
|      3 | 3485 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3486 | `				return PH7_OK;` |
|    ! 0 | 3487 | `			}else{` |
|      5 | 3488 | `				nLen -= nStart;` |
|      5 | 3489 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 3490 | `				zEnd = &zBlob[nLen];` |
|      - | 3491 | `			}` |
|      3 | 3492 | `		}else{` |
|      9 | 3493 | `			if( nStart >= nLen ){` |
|      - | 3494 | `				/* Invalid offset */` |
|      5 | 3495 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3496 | `				return PH7_OK;` |
|    ! 0 | 3497 | `			}else{` |
|      5 | 3498 | `				zBlob += nStart;` |
|      5 | 3499 | `				nLen -= nStart;` |
|      - | 3500 | `			}` |
|      - | 3501 | `		}` |
|      4 | 3502 | `	}` |
|     21 | 3503 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3504 | `		/* Perform the lookup */` |
|     44 | 3505 | `		for(;;){` |
|     89 | 3506 | `			if( zBlob >= zPtr ){` |
|      9 | 3507 | `				break;` |
|      - | 3508 | `			}` |
|     81 | 3509 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 3510 | `			if( rc == SXRET_OK ){` |
|      - | 3511 | `				/* Pattern found,return it's position */` |
|     11 | 3512 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 3513 | `				return PH7_OK;` |
|      - | 3514 | `			}` |
|     71 | 3515 | `			zPtr--;` |
|      1 | 3516 | `		}` |
|      - | 3517 | `		/* Pattern not found,return FALSE */` |
|      9 | 3518 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3519 | `	}else{` |
|      3 | 3520 | `		ph7_result_bool(pCtx,0);` |
|      - | 3521 | `	}` |
|     11 | 3522 | `	return PH7_OK;` |
|     15 | 3523 |  |
|      - | 3524 | `/*` |
|      - | 3525 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3526 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3527 | ` * Parameters` |
|      - | 3528 | ` *  $haystack` |
|      - | 3529 | ` *   The input string.` |
|      - | 3530 | ` * $needle` |
|      - | 3531 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3532 | ` *  This behavior is different from that of strstr().` |
|      - | 3533 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3534 | ` *  as the ordinal value of a character.` |
|      - | 3535 | ` * Return` |
|      - | 3536 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3537 | ` */` |
|     24 | 3538 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3539 |  |
|      - | 3540 | `	const char *zBlob;` |
|      - | 3541 | `	int nLen,c;` |
|     25 | 3542 | `	if( nArg < 2 ){` |
|      - | 3543 | `		/* Missing arguments,return FALSE */` |
|      3 | 3544 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3545 | `		return PH7_OK;` |
|      - | 3546 | `	}` |
|      - | 3547 | `	/* Extract the haystack */` |
|     23 | 3548 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3549 | `	c = 0; /* cc warning */` |
|     23 | 3550 | `	if( nLen > 0 ){` |
|      - | 3551 | `		sxu32 nOfft;` |
|      - | 3552 | `		sxi32 rc;` |
|     21 | 3553 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 3554 | `			const char *zPattern;` |
|     11 | 3555 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 3556 | `														 * for NULL pointer.` |
|      - | 3557 | `														 */` |
|     11 | 3558 | `			c = zPattern[0];` |
|      6 | 3559 | `		}else{` |
|      - | 3560 | `			/* Int cast */` |
|     11 | 3561 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 3562 | `		}` |
|      - | 3563 | `		/* Perform the lookup */` |
|     21 | 3564 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3565 | `		if( rc != SXRET_OK ){` |
|      - | 3566 | `			/* No such entry,return FALSE */` |
|      7 | 3567 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3568 | `			return PH7_OK;` |
|      - | 3569 | `		}` |
|      - | 3570 | `		/* Return the string portion */` |
|     15 | 3571 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 3572 | `	}else{` |
|      3 | 3573 | `		ph7_result_bool(pCtx,0);` |
|      - | 3574 | `	}` |
|     17 | 3575 | `	return PH7_OK;` |
|     13 | 3576 |  |
|      - | 3577 | `/*` |
|      - | 3578 | ` * string strrev(string $string)` |
|      - | 3579 | ` *  Reverse a string.` |
|      - | 3580 | ` * Parameters` |
|      - | 3581 | ` *  $string` |
|      - | 3582 | ` *   String to be reversed.` |
|      - | 3583 | ` * Return` |
|      - | 3584 | ` *  The reversed string.` |
|      - | 3585 | ` */` |
|      4 | 3586 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3587 |  |
|      - | 3588 | `	const char *zIn,*zEnd;` |
|      - | 3589 | `	int nLen,c;` |
|      5 | 3590 | `	if( nArg < 1 ){` |
|      - | 3591 | `		/* Missing arguments,return NULL */` |
|      3 | 3592 | `		ph7_result_null(pCtx);` |
|      3 | 3593 | `		return PH7_OK;` |
|      - | 3594 | `	}` |
|      - | 3595 | `	/* Extract the target string */` |
|      3 | 3596 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3597 | `	if( nLen < 1 ){` |
|      - | 3598 | `		/* Empty string Return null */` |
|    ! 0 | 3599 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3600 | `		return PH7_OK;` |
|      - | 3601 | `	}` |
|      - | 3602 | `	/* Perform the requested operation */` |
|      3 | 3603 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3604 | `	for(;;){` |
|      9 | 3605 | `		if( zEnd < zIn ){` |
|      - | 3606 | `			/* No more input to process */` |
|      3 | 3607 | `			break;` |
|      - | 3608 | `		}` |
|      - | 3609 | `		/* Append current character */` |
|      7 | 3610 | `		c = zEnd[0];` |
|      7 | 3611 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3612 | `		zEnd--;` |
|      1 | 3613 | `	}` |
|      3 | 3614 | `	return PH7_OK;` |
|      3 | 3615 |  |
|      - | 3616 | `/*` |
|      - | 3617 | ` * string ucwords(string $string)` |
|      - | 3618 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3619 | ` *  The definition of a word is any string of characters that is immediately after` |
|      - | 3620 | ` *  a whitespace (These are: space, form-feed, newline, carriage return, horizontal tab, and vertical tab).` |
|      - | 3621 | ` * Parameters` |
|      - | 3622 | ` *  $string` |
|      - | 3623 | ` *   The input string.` |
|      - | 3624 | ` * Return` |
|      - | 3625 | ` *  The modified string..` |
|      - | 3626 | ` */` |
|     14 | 3627 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3628 |  |
|      - | 3629 | `	const char *zIn,*zCur,*zEnd;` |
|      - | 3630 | `	int nLen,c;` |
|     15 | 3631 | `	if( nArg < 1 ){` |
|      - | 3632 | `		/* Missing arguments,return NULL */` |
|      3 | 3633 | `		ph7_result_null(pCtx);` |
|      3 | 3634 | `		return PH7_OK;` |
|      - | 3635 | `	}` |
|      - | 3636 | `	/* Extract the target string */` |
|     13 | 3637 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 3638 | `	if( nLen < 1 ){` |
|      - | 3639 | `		/* Empty string Return null */` |
|      3 | 3640 | `		ph7_result_null(pCtx);` |
|      3 | 3641 | `		return PH7_OK;` |
|      - | 3642 | `	}` |
|      - | 3643 | `	/* Perform the requested operation */` |
|     11 | 3644 | `	zEnd = &zIn[nLen];` |
|     21 | 3645 | `	for(;;){` |
|      - | 3646 | `		/* Jump leading white spaces */` |
|     43 | 3647 | `		zCur = zIn;` |
|     65 | 3648 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0x80 && SyisSpace(zIn[0]) ){` |
|     23 | 3649 | `			zIn++;` |
|      1 | 3650 | `		}` |
|     43 | 3651 | `		if( zCur < zIn ){` |
|      - | 3652 | `			/* Append white space stream */` |
|     23 | 3653 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 3654 | `		}` |
|     43 | 3655 | `		if( zIn >= zEnd ){` |
|      - | 3656 | `			/* No more input to process */` |
|     11 | 3657 | `			break;` |
|      - | 3658 | `		}` |
|     33 | 3659 | `		c = zIn[0];` |
|     33 | 3660 | `		if( c < 0x80 && SyisLower(c) ){` |
|     29 | 3661 | `			c = SyToUpper(c);` |
|     14 | 3662 | `		}` |
|      - | 3663 | `		/* Append the upper-cased character */` |
|     33 | 3664 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     33 | 3665 | `		zIn++;` |
|     33 | 3666 | `		zCur = zIn;` |
|      - | 3667 | `		/* Append the word varbatim */` |
|    149 | 3668 | `		while( zIn < zEnd ){` |
|    139 | 3669 | `			if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 3670 | `				/* UTF-8 stream */` |
|    ! 0 | 3671 | `				zIn++;` |
|    ! 0 | 3672 | `				SX_JMP_UTF8(zIn,zEnd);` |
|    138 | 3673 | `			}else if( !SyisSpace(zIn[0]) ){` |
|    117 | 3674 | `				zIn++;` |
|     59 | 3675 | `			}else{` |
|     23 | 3676 | `				break;` |
|      - | 3677 | `			}` |
|      1 | 3678 | `		}` |
|     33 | 3679 | `		if( zCur < zIn ){` |
|     33 | 3680 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 | 3681 | `		}` |
|      1 | 3682 | `	}` |
|     11 | 3683 | `	return PH7_OK;` |
|      8 | 3684 |  |
|      - | 3685 | `/*` |
|      - | 3686 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3687 | ` *  Returns input repeated multiplier times.` |
|      - | 3688 | ` * Parameters` |
|      - | 3689 | ` *  $string` |
|      - | 3690 | ` *   String to be repeated.` |
|      - | 3691 | ` * $multiplier` |
|      - | 3692 | ` *  Number of time the input string should be repeated.` |
|      - | 3693 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3694 | ` *  to 0, the function will return an empty string.` |
|      - | 3695 | ` * Return` |
|      - | 3696 | ` *  The repeated string.` |
|      - | 3697 | ` */` |
|  20212 | 3698 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3699 |  |
|      - | 3700 | `	const char *zIn;` |
|      - | 3701 | `	int nLen,nMul;` |
|      - | 3702 | `	int rc;` |
|  20213 | 3703 | `	if( nArg < 2 ){` |
|      - | 3704 | `		/* Missing arguments,return NULL */` |
|      3 | 3705 | `		ph7_result_null(pCtx);` |
|      3 | 3706 | `		return PH7_OK;` |
|      - | 3707 | `	}` |
|      - | 3708 | `	/* Extract the target string */` |
|  20211 | 3709 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|  20211 | 3710 | `	if( nLen < 1 ){` |
|      - | 3711 | `		/* Empty string.Return null */` |
|    ! 0 | 3712 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3713 | `		return PH7_OK;` |
|      - | 3714 | `	}` |
|      - | 3715 | `	/* Extract the multiplier */` |
|  20211 | 3716 | `	nMul = ph7_value_to_int(apArg[1]);` |
|  20211 | 3717 | `	if( nMul < 1 ){` |
|      - | 3718 | `		/* Return the empty string */` |
|      3 | 3719 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3720 | `		return PH7_OK;` |
|      - | 3721 | `	}` |
|      - | 3722 | `	/* Perform the requested operation */` |
| 120220 | 3723 | `	for(;;){` |
| 240441 | 3724 | `		if( !nMul ){` |
|  20209 | 3725 | `			break;` |
|      - | 3726 | `		}` |
|      - | 3727 | `		/* Append the copy */` |
| 220233 | 3728 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 220233 | 3729 | `		if( rc != PH7_OK ){` |
|      - | 3730 | `			/* Out of memory,break immediately */` |
|    ! 0 | 3731 | `			break;` |
|      - | 3732 | `		}` |
| 220233 | 3733 | `		nMul--;` |
|      1 | 3734 | `	}` |
|  20209 | 3735 | `	return PH7_OK;` |
|  10107 | 3736 |  |
|      - | 3737 | `/*` |
|      - | 3738 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3739 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3740 | ` * Parameters` |
|      - | 3741 | ` *  $string` |
|      - | 3742 | ` *   The input string.` |
|      - | 3743 | ` * $is_xhtml` |
|      - | 3744 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3745 | ` * Return` |
|      - | 3746 | ` *  The processed string.` |
|      - | 3747 | ` */` |
|      6 | 3748 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3749 |  |
|      - | 3750 | `	const char *zIn,*zCur,*zEnd;` |
|      7 | 3751 | `	int is_xhtml = 1; /* Default to XHTML-style '<br/>' like PHP */` |
|      - | 3752 | `	int nLen;` |
|      7 | 3753 | `	if( nArg < 1 ){` |
|      - | 3754 | `		/* Missing arguments,return the empty string */` |
|      3 | 3755 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3756 | `		return PH7_OK;` |
|      - | 3757 | `	}` |
|      - | 3758 | `	/* Extract the target string */` |
|      5 | 3759 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3760 | `	if( nLen < 1 ){` |
|      - | 3761 | `		/* Empty string,return null */` |
|    ! 0 | 3762 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3763 | `		return PH7_OK;` |
|      - | 3764 | `	}` |
|      5 | 3765 | `	if( nArg > 1 ){` |
|      3 | 3766 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3767 | `	}` |
|      5 | 3768 | `	zEnd = &zIn[nLen];` |
|      - | 3769 | `	/* Perform the requested operation */` |
|      4 | 3770 | `	for(;;){` |
|      9 | 3771 | `		zCur = zIn;` |
|      - | 3772 | `		/* Delimit the string */` |
|     21 | 3773 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3774 | `			zIn++;` |
|      1 | 3775 | `		}` |
|      9 | 3776 | `		if( zCur < zIn ){` |
|      - | 3777 | `			/* Output chunk verbatim */` |
|      9 | 3778 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3779 | `		}` |
|      9 | 3780 | `		if( zIn >= zEnd ){` |
|      - | 3781 | `			/* No more input to process */` |
|      5 | 3782 | `			break;` |
|      - | 3783 | `		}` |
|      - | 3784 | `		/* Output the HTML line break */` |
|      - | 3785 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br/>' (legacy without space), otherwise use '<br>' */` |
|      5 | 3786 | `		if( is_xhtml ){` |
|      3 | 3787 | `			ph7_result_string(pCtx,"<br/>",(int)sizeof("<br/>")-1);` |
|      2 | 3788 | `		}else{` |
|      3 | 3789 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3790 | `		}` |
|      5 | 3791 | `		zCur = zIn;` |
|      - | 3792 | `		/* Append trailing line */` |
|     11 | 3793 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3794 | `			zIn++;` |
|      1 | 3795 | `		}` |
|      5 | 3796 | `		if( zCur < zIn ){` |
|      - | 3797 | `			/* Output chunk verbatim */` |
|      5 | 3798 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 3799 | `		}` |
|      1 | 3800 | `	}` |
|      5 | 3801 | `	return PH7_OK;` |
|      4 | 3802 |  |
|      - | 3803 | `/*` |
|      - | 3804 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3805 | ` *  According to the PHP reference manual.` |
|      - | 3806 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3807 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3808 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 3809 | ` * This applies to both sprintf() and printf().` |
|      - | 3810 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 3811 | ` * or more of these elements, in order:` |
|      - | 3812 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 3813 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 3814 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 3815 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 3816 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 3817 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 3818 | ` *   it with a single quote ('). See the examples below.` |
|      - | 3819 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 3820 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 3821 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 3822 | ` *   should result in.` |
|      - | 3823 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 3824 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 3825 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 3826 | ` *   limit to the string.` |
|      - | 3827 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 3828 | ` *       % - a literal percent character. No argument is required.` |
|      - | 3829 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 3830 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 3831 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 3832 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 3833 | ` * 	     for the number of digits after the decimal point.` |
|      - | 3834 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 3835 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 3836 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 3837 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 3838 | ` *       g - shorter of %e and %f.` |
|      - | 3839 | ` *       G - shorter of %E and %f.` |
|      - | 3840 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 3841 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 3842 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 3843 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 3844 | ` */` |
|      - | 3845 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3846 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 3847 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 3848 | `/*` |
|      - | 3849 | `** Conversion types fall into various categories as defined by the` |
|      - | 3850 | `** following enumeration.` |
|      - | 3851 | `*/` |
|      - | 3852 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 3853 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 3854 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 3855 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 3856 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 3857 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 3858 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 3859 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 3860 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 3861 |  |
|      - | 3862 | `/*` |
|      - | 3863 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 3864 | `*/` |
|      - | 3865 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 3866 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 3867 | `/*` |
|      - | 3868 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 3869 | `** by an instance of the following structure` |
|      - | 3870 | `*/` |
|      - | 3871 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 3872 | `struct ph7_fmt_info` |
|      - | 3873 |  |
|      - | 3874 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 3875 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 3876 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 3877 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 3878 | `  char *charset; /* The character set for conversion */` |
|      - | 3879 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 3880 | `};` |
|      - | 3881 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3882 | `/*` |
|      - | 3883 | `** "*val" is a double such that 0.1 <= *val < 10.0` |
|      - | 3884 | `** Return the ascii code for the leading digit of *val, then` |
|      - | 3885 | `** multiply "*val" by 10.0 to renormalize.` |
|      - | 3886 | `**` |
|      - | 3887 | `** Example:` |
|      - | 3888 | `**     input:     *val = 3.14159` |
|      - | 3889 | `**     output:    *val = 1.4159    function return = '3'` |
|      - | 3890 | `**` |
|      - | 3891 | `** The counter *cnt is incremented each time.  After counter exceeds` |
|      - | 3892 | `** 16 (the number of significant digits in a 64-bit float) '0' is` |
|      - | 3893 | `** always returned.` |
|      - | 3894 | `*/` |
|    404 | 3895 | `static int vxGetdigit(sxlongreal *val,int *cnt)` |
|      1 | 3896 |  |
|      - | 3897 | `  sxlongreal d;` |
|      - | 3898 | `  int digit;` |
|      - | 3899 |  |
|    405 | 3900 | `  if( (*cnt)++ >= 16 ){` |
|    ! 0 | 3901 | `	  return '0';` |
|      - | 3902 | `  }` |
|    405 | 3903 | `  digit = (int)*val;` |
|    405 | 3904 | `  d = digit;` |
|    405 | 3905 | `   *val = (*val - d)*10.0;` |
|    405 | 3906 | `  return digit + '0' ;` |
|    203 | 3907 |  |
|      - | 3908 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      - | 3909 | `/*` |
|      - | 3910 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 3911 | ` * used conversion types first.` |
|      - | 3912 | ` */` |
|      - | 3913 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 3914 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 3915 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 3916 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 3917 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 3918 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 3919 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 3920 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 3921 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 3922 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3923 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3924 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 3925 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 3926 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 3927 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 3928 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 3929 | `};` |
|      - | 3930 | `/*` |
|      - | 3931 | ` * Format a given string.` |
|      - | 3932 | ` * The root program.  All variations call this core.` |
|      - | 3933 | ` * INPUTS:` |
|      - | 3934 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 3935 | ` *            1. A pointer to the call context.` |
|      - | 3936 | ` *            2. A pointer to the list of characters to be output` |
|      - | 3937 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 3938 | ` *            3. An integer number of characters to be output.` |
|      - | 3939 | ` *               (Note: This number might be zero.)` |
|      - | 3940 | ` *            4. Upper layer private data.` |
|      - | 3941 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 3942 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 3943 | ` */` |
|    120 | 3944 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 3945 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 3946 | `	ph7_context *pCtx,  /* call context */` |
|      - | 3947 | `	const char *zIn,    /* Format string */` |
|      - | 3948 | `	int nByte,          /* Format string length */` |
|      - | 3949 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 3950 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 3951 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 3952 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 3953 | `	)` |
|      1 | 3954 |  |
|    121 | 3955 | `	char spaces[] = "                                                  ";` |
|      - | 3956 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    121 | 3957 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 3958 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 3959 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 3960 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 3961 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 3962 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 3963 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 3964 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 3965 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 3966 | `	ph7_int64 iVal;` |
|      - | 3967 | `	int precision;           /* Precision of the current field */` |
|      - | 3968 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 3969 | `	int c,rc,n;` |
|      - | 3970 | `	int length;              /* Length of the field */` |
|      - | 3971 | `	int prefix;` |
|      - | 3972 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 3973 | `	int width;               /* Width of the current field */` |
|      - | 3974 | `	int idx;` |
|    121 | 3975 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 3976 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 3977 | `	/* Start the format process */` |
|    123 | 3978 | `	for(;;){` |
|    247 | 3979 | `		zCur = zIn;` |
|    697 | 3980 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|    451 | 3981 | `			zIn++;` |
|      1 | 3982 | `		}` |
|    247 | 3983 | `		if( zCur < zIn ){` |
|      - | 3984 | `			/* Consume chunk verbatim */` |
|     95 | 3985 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|     95 | 3986 | `			if( rc == SXERR_ABORT ){` |
|      - | 3987 | `				/* Callback request an operation abort */` |
|    ! 0 | 3988 | `				break;` |
|      - | 3989 | `			}` |
|     47 | 3990 | `		}` |
|    247 | 3991 | `		if( zIn >= zEnd ){` |
|      - | 3992 | `			/* No more input to process,break immediately */` |
|    119 | 3993 | `			break;` |
|      - | 3994 | `		}` |
|      - | 3995 | `		/* Find out what flags are present */` |
|    129 | 3996 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    128 | 3997 | `			flag_alternateform = flag_zeropad = 0;` |
|    129 | 3998 | `		zIn++; /* Jump the precent sign */` |
|     64 | 3999 | `		do{` |
|    157 | 4000 | `			c = zIn[0];` |
|    157 | 4001 | `			switch( c ){` |
|      9 | 4002 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      3 | 4003 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 4004 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|      9 | 4005 | `			case '#':   flag_alternateform = 1;   c = 0;   break;` |
|      5 | 4006 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 4007 | `			case '\'':` |
|    ! 0 | 4008 | `				zIn++;` |
|    ! 0 | 4009 | `				if( zIn < zEnd ){` |
|      - | 4010 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 4011 | `					c = zIn[0];` |
|    ! 0 | 4012 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4013 | `						spaces[idx] = (char)c;` |
|    ! 0 | 4014 | `					}` |
|    ! 0 | 4015 | `					c = 0;` |
|    ! 0 | 4016 | `				}` |
|    ! 0 | 4017 | `				break;` |
|    128 | 4018 | `			default:                                       break;` |
|      - | 4019 | `			}` |
|    157 | 4020 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 4021 | `		/* Get the field width */` |
|    129 | 4022 | `		width = 0;` |
|    223 | 4023 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     31 | 4024 | `			width = width*10 + (zIn[0] - '0');` |
|     31 | 4025 | `			zIn++;` |
|      1 | 4026 | `		}` |
|    129 | 4027 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 4028 | `			/* Position specifer */` |
|    ! 0 | 4029 | `			if( width > 0 ){` |
|    ! 0 | 4030 | `				n = width;` |
|    ! 0 | 4031 | `				if( vf && n > 0 ){` |
|    ! 0 | 4032 | `					n--;` |
|    ! 0 | 4033 | `				}` |
|    ! 0 | 4034 | `			}` |
|    ! 0 | 4035 | `			zIn++;` |
|    ! 0 | 4036 | `			width = 0;` |
|    ! 0 | 4037 | `			if( zIn < zEnd && zIn[0] == '0' ){` |
|    ! 0 | 4038 | `				flag_zeropad = 1;` |
|    ! 0 | 4039 | `				zIn++;` |
|    ! 0 | 4040 | `			}` |
|    ! 0 | 4041 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 4042 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 4043 | `				zIn++;` |
|    ! 0 | 4044 | `			}` |
|    ! 0 | 4045 | `		}` |
|    129 | 4046 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 4047 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 4048 | `		}` |
|      - | 4049 | `		/* Get the precision */` |
|    129 | 4050 | `		precision = -1;` |
|    129 | 4051 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     57 | 4052 | `			precision = 0;` |
|     57 | 4053 | `			zIn++;` |
|    145 | 4054 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     61 | 4055 | `				precision = precision*10 + (zIn[0] - '0');` |
|     61 | 4056 | `				zIn++;` |
|      1 | 4057 | `			}` |
|     28 | 4058 | `		}` |
|    129 | 4059 | `		if( zIn >= zEnd ){` |
|      - | 4060 | `			/* No more input */` |
|      3 | 4061 | `			break;` |
|      - | 4062 | `		}` |
|      - | 4063 | `		/* Fetch the info entry for the field */` |
|    127 | 4064 | `		pInfo = 0;` |
|    127 | 4065 | `		xtype = PH7_FMT_ERROR;` |
|    127 | 4066 | `		c = zIn[0];` |
|    127 | 4067 | `		zIn++; /* Jump the format specifer */` |
|    699 | 4068 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|    697 | 4069 | `			if( c==aFmt[idx].fmttype ){` |
|    125 | 4070 | `				pInfo = &aFmt[idx];` |
|    125 | 4071 | `				xtype = pInfo->type;` |
|    125 | 4072 | `				break;` |
|      - | 4073 | `			}` |
|    287 | 4074 | `		}` |
|    127 | 4075 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    127 | 4076 | `		length = 0;` |
|      - | 4077 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 4078 | `		 /*` |
|      - | 4079 | `		  ** At this point, variables are initialized as follows:` |
|      - | 4080 | `		  **` |
|      - | 4081 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 4082 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 4083 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 4084 | `		  **                               field width was negative.` |
|      - | 4085 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 4086 | `		  **                               the conversion character.` |
|      - | 4087 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 4088 | `		  **   width                       The specified field width.  This is` |
|      - | 4089 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 4090 | `		  **   precision                   The specified precision.  The default` |
|      - | 4091 | `		  **                               is -1.` |
|      - | 4092 | `		  */` |
|    127 | 4093 | `		switch(xtype){` |
|    ! 0 | 4094 | `		case PH7_FMT_PERCENT:` |
|      - | 4095 | `			/* A literal percent character */` |
|    ! 0 | 4096 | `			zWorker[0] = '%';` |
|    ! 0 | 4097 | `			length = (int)sizeof(char);` |
|    ! 0 | 4098 | `			break;` |
|      3 | 4099 | `		case PH7_FMT_CHARX:` |
|      - | 4100 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 4101 | `			 * with that ASCII value` |
|      - | 4102 | `			 */` |
|      7 | 4103 | `			pArg = NEXT_ARG;` |
|      7 | 4104 | `			if( pArg == 0 ){` |
|      3 | 4105 | `				c = 0;` |
|      2 | 4106 | `			}else{` |
|      5 | 4107 | `				c = ph7_value_to_int(pArg);` |
|      - | 4108 | `			}` |
|      - | 4109 | `			/* NUL byte is an acceptable value */` |
|      7 | 4110 | `			zWorker[0] = (char)c;` |
|      7 | 4111 | `			length = (int)sizeof(char);` |
|      7 | 4112 | `			break;` |
|     12 | 4113 | `		case PH7_FMT_STRING:` |
|      - | 4114 | `			/* the argument is treated as and presented as a string */` |
|     25 | 4115 | `			pArg = NEXT_ARG;` |
|     25 | 4116 | `			if( pArg == 0 ){` |
|    ! 0 | 4117 | `				length = 0;` |
|    ! 0 | 4118 | `			}else{` |
|     25 | 4119 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 4120 | `			}` |
|     25 | 4121 | `			if( length < 1 ){` |
|    ! 0 | 4122 | `				zBuf = " ";` |
|    ! 0 | 4123 | `				length = (int)sizeof(char);` |
|    ! 0 | 4124 | `			}` |
|     25 | 4125 | `			if( precision>=0 && precision<length ){` |
|      3 | 4126 | `				length = precision;` |
|      1 | 4127 | `			}` |
|     25 | 4128 | `			if( flag_zeropad ){` |
|      - | 4129 | `				/* zero-padding works on strings too */` |
|    ! 0 | 4130 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4131 | `					spaces[idx] = '0';` |
|    ! 0 | 4132 | `				}` |
|    ! 0 | 4133 | `			}` |
|     25 | 4134 | `			break;` |
|     20 | 4135 | `		case PH7_FMT_RADIX:` |
|     41 | 4136 | `			pArg = NEXT_ARG;` |
|     41 | 4137 | `			if( pArg == 0 ){` |
|    ! 0 | 4138 | `				iVal = 0;` |
|    ! 0 | 4139 | `			}else{` |
|     41 | 4140 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 4141 | `			}` |
|      - | 4142 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|     41 | 4143 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 4144 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 4145 | `			}` |
|      - | 4146 | `#if 1` |
|      - | 4147 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 4148 | `        ** I think this is stupid.*/` |
|     41 | 4149 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 4150 | `#else` |
|      - | 4151 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 4152 | `        ** but leave the prefix for hex.*/` |
|      - | 4153 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 4154 | `#endif` |
|     41 | 4155 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|     23 | 4156 | `          if( iVal<0 ){` |
|      3 | 4157 | `            iVal = -iVal;` |
|      - | 4158 | `			/* Ticket 1433-003 */` |
|      3 | 4159 | `			if( iVal < 0 ){` |
|      - | 4160 | `				/* Overflow */` |
|    ! 0 | 4161 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4162 | `			}` |
|      3 | 4163 | `            prefix = '-';` |
|     22 | 4164 | `          }else if( flag_plussign )  prefix = '+';` |
|     19 | 4165 | `          else if( flag_blanksign )  prefix = ' ';` |
|     17 | 4166 | `          else                       prefix = 0;` |
|     12 | 4167 | `        }else{` |
|     19 | 4168 | `			if( iVal<0 ){` |
|    ! 0 | 4169 | `				iVal = -iVal;` |
|      - | 4170 | `				/* Ticket 1433-003 */` |
|    ! 0 | 4171 | `				if( iVal < 0 ){` |
|      - | 4172 | `					/* Overflow */` |
|    ! 0 | 4173 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4174 | `				}` |
|    ! 0 | 4175 | `			}` |
|     19 | 4176 | `			prefix = 0;` |
|      - | 4177 | `		}` |
|     41 | 4178 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      3 | 4179 | `          precision = width-(prefix!=0);` |
|      1 | 4180 | `        }` |
|     41 | 4181 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 4182 | `        {` |
|      - | 4183 | `          register char *cset;      /* Use registers for speed */` |
|      - | 4184 | `          register int base;` |
|     41 | 4185 | `          cset = pInfo->charset;` |
|     41 | 4186 | `          base = pInfo->base;` |
|     20 | 4187 | `          do{                                           /* Convert to ascii */` |
|     79 | 4188 | `            *(--zBuf) = cset[iVal%base];` |
|     79 | 4189 | `            iVal = iVal/base;` |
|     79 | 4190 | `          }while( iVal>0 );` |
|      - | 4191 | `        }` |
|     41 | 4192 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     55 | 4193 | `        for(idx=precision-length; idx>0; idx--){` |
|     15 | 4194 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|      8 | 4195 | `        }` |
|     41 | 4196 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|     41 | 4197 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 4198 | `          char *pre, x;` |
|      9 | 4199 | `          pre = pInfo->prefix;` |
|      9 | 4200 | `          if( *zBuf!=pre[0] ){` |
|     23 | 4201 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|      4 | 4202 | `          }` |
|      4 | 4203 | `        }` |
|     41 | 4204 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     41 | 4205 | `		break;` |
|     27 | 4206 | `		case PH7_FMT_FLOAT:` |
|      - | 4207 | `		case PH7_FMT_EXP:` |
|      - | 4208 | `		case PH7_FMT_GENERIC:{` |
|      - | 4209 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 4210 | `		long double realvalue;` |
|      - | 4211 | `		int  exp;                /* exponent of real numbers */` |
|      - | 4212 | `		double rounder;          /* Used for rounding floating point values */` |
|      - | 4213 | `		int flag_dp;            /* True if decimal point should be shown */` |
|      - | 4214 | `		int flag_rtz;           /* True if trailing zeros should be removed */` |
|      - | 4215 | `		int flag_exp;           /* True to force display of the exponent */` |
|      - | 4216 | `		int nsd;                 /* Number of significant digits returned */` |
|     55 | 4217 | `		pArg = NEXT_ARG;` |
|     55 | 4218 | `		if( pArg == 0 ){` |
|    ! 0 | 4219 | `			realvalue = 0;` |
|    ! 0 | 4220 | `		}else{` |
|     55 | 4221 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 4222 | `		}` |
|     55 | 4223 | `        if( precision<0 ) precision = 6;         /* Set default precision */` |
|     55 | 4224 | `        if( precision>PH7_FMT_BUFSIZ-40) precision = PH7_FMT_BUFSIZ-40;` |
|     55 | 4225 | `        if( realvalue<0.0 ){` |
|    ! 0 | 4226 | `          realvalue = -realvalue;` |
|    ! 0 | 4227 | `          prefix = '-';` |
|    ! 0 | 4228 | `        }else{` |
|     55 | 4229 | `          if( flag_plussign )          prefix = '+';` |
|     55 | 4230 | `          else if( flag_blanksign )    prefix = ' ';` |
|     55 | 4231 | `          else                         prefix = 0;` |
|      - | 4232 | `        }` |
|     55 | 4233 | `        if( pInfo->type==PH7_FMT_GENERIC && precision>0 ) precision--;` |
|     55 | 4234 | `        rounder = 0.0;` |
|      - | 4235 | `#if 0` |
|      - | 4236 | `        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */` |
|      - | 4237 | `        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);` |
|      - | 4238 | `#else` |
|      - | 4239 | `        /* It makes more sense to use 0.5 */` |
|    387 | 4240 | `        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);` |
|      - | 4241 | `#endif` |
|     55 | 4242 | `        if( pInfo->type==PH7_FMT_FLOAT ) realvalue += rounder;` |
|      - | 4243 | `        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */` |
|     55 | 4244 | `        exp = 0;` |
|     55 | 4245 | `        if( realvalue>0.0 ){` |
|     59 | 4246 | `          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }` |
|     87 | 4247 | `          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }` |
|     59 | 4248 | `          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }` |
|     69 | 4249 | `          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }` |
|     55 | 4250 | `          if( exp>350 \|\| exp<-350 ){` |
|    ! 0 | 4251 | `            zBuf = "NaN";` |
|    ! 0 | 4252 | `            length = 3;` |
|    ! 0 | 4253 | `            break;` |
|      - | 4254 | `          }` |
|     27 | 4255 | `        }` |
|     55 | 4256 | `        zBuf = zWorker;` |
|      - | 4257 | `        /*` |
|      - | 4258 | `        ** If the field type is etGENERIC, then convert to either etEXP` |
|      - | 4259 | `        ** or etFLOAT, as appropriate.` |
|      - | 4260 | `        */` |
|     55 | 4261 | `        flag_exp = xtype==PH7_FMT_EXP;` |
|     55 | 4262 | `        if( xtype!=PH7_FMT_FLOAT ){` |
|    ! 0 | 4263 | `          realvalue += rounder;` |
|    ! 0 | 4264 | `          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }` |
|    ! 0 | 4265 | `        }` |
|     55 | 4266 | `        if( xtype==PH7_FMT_GENERIC ){` |
|    ! 0 | 4267 | `          flag_rtz = !flag_alternateform;` |
|    ! 0 | 4268 | `          if( exp<-4 \|\| exp>precision ){` |
|    ! 0 | 4269 | `            xtype = PH7_FMT_EXP;` |
|    ! 0 | 4270 | `          }else{` |
|    ! 0 | 4271 | `            precision = precision - exp;` |
|    ! 0 | 4272 | `            xtype = PH7_FMT_FLOAT;` |
|      - | 4273 | `          }` |
|    ! 0 | 4274 | `        }else{` |
|     55 | 4275 | `          flag_rtz = 0;` |
|      - | 4276 | `        }` |
|      - | 4277 | `        /*` |
|      - | 4278 | `        ** The "exp+precision" test causes output to be of type etEXP if` |
|      - | 4279 | `        ** the precision is too large to fit in buf[].` |
|      - | 4280 | `        */` |
|     55 | 4281 | `        nsd = 0;` |
|     55 | 4282 | `        if( xtype==PH7_FMT_FLOAT && exp+precision<PH7_FMT_BUFSIZ-30 ){` |
|     55 | 4283 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|     55 | 4284 | `          if( prefix ) *(zBuf++) = (char)prefix;         /* Sign */` |
|     55 | 4285 | `          if( exp<0 )  *(zBuf++) = '0';            /* Digits before "." */` |
|    145 | 4286 | `          else for(; exp>=0; exp--) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     55 | 4287 | `          if( flag_dp ) *(zBuf++) = '.';           /* The decimal point */` |
|     87 | 4288 | `          for(exp++; exp<0 && precision>0; precision--, exp++){` |
|     33 | 4289 | `            *(zBuf++) = '0';` |
|     17 | 4290 | `          }` |
|    355 | 4291 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     55 | 4292 | `          *(zBuf--) = 0;                           /* Null terminate */` |
|     55 | 4293 | `          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */` |
|    ! 0 | 4294 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 4295 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 4296 | `          }` |
|     55 | 4297 | `          zBuf++;                            /* point to next free slot */` |
|     28 | 4298 | `        }else{    /* etEXP or etGENERIC */` |
|    ! 0 | 4299 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|    ! 0 | 4300 | `          if( prefix ) *(zBuf++) = (char)prefix;   /* Sign */` |
|    ! 0 | 4301 | `          *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);  /* First digit */` |
|    ! 0 | 4302 | `          if( flag_dp ) *(zBuf++) = '.';     /* Decimal point */` |
|    ! 0 | 4303 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|    ! 0 | 4304 | `          zBuf--;                            /* point to last digit */` |
|    ! 0 | 4305 | `          if( flag_rtz && flag_dp ){          /* Remove tail zeros */` |
|    ! 0 | 4306 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 4307 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 4308 | `          }` |
|    ! 0 | 4309 | `          zBuf++;                            /* point to next free slot */` |
|    ! 0 | 4310 | `          if( exp \|\| flag_exp ){` |
|    ! 0 | 4311 | `            *(zBuf++) = pInfo->charset[0];` |
|    ! 0 | 4312 | `            if( exp<0 ){ *(zBuf++) = '-'; exp = -exp; } /* sign of exp */` |
|    ! 0 | 4313 | `            else       { *(zBuf++) = '+'; }` |
|    ! 0 | 4314 | `            if( exp>=100 ){` |
|    ! 0 | 4315 | `              *(zBuf++) = (char)((exp/100)+'0');                /* 100's digit */` |
|    ! 0 | 4316 | `              exp %= 100;` |
|    ! 0 | 4317 | `            }` |
|    ! 0 | 4318 | `            *(zBuf++) = (char)(exp/10+'0');                     /* 10's digit */` |
|    ! 0 | 4319 | `            *(zBuf++) = (char)(exp%10+'0');                     /* 1's digit */` |
|    ! 0 | 4320 | `          }` |
|      - | 4321 | `        }` |
|      - | 4322 | `        /* The converted number is in buf[] and zero terminated.Output it.` |
|      - | 4323 | `        ** Note that the number is in the usual order, not reversed as with` |
|      - | 4324 | `        ** integer conversions.*/` |
|     55 | 4325 | `        length = (int)(zBuf-zWorker);` |
|     55 | 4326 | `        zBuf = zWorker;` |
|      - | 4327 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 4328 | `        ** set and we are not left justified */` |
|     55 | 4329 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 4330 | `          int i;` |
|      3 | 4331 | `          int nPad = width - length;` |
|     13 | 4332 | `          for(i=width; i>=nPad; i--){` |
|     11 | 4333 | `            zBuf[i] = zBuf[i-nPad];` |
|      6 | 4334 | `          }` |
|      3 | 4335 | `          i = prefix!=0;` |
|      5 | 4336 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      3 | 4337 | `          length = width;` |
|      1 | 4338 | `        }` |
|      - | 4339 | `#else` |
|      - | 4340 | `         zBuf = " ";` |
|      - | 4341 | `		 length = (int)sizeof(char);` |
|      - | 4342 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|     55 | 4343 | `		 break;` |
|      - | 4344 | `							 }` |
|      1 | 4345 | `		default:` |
|      - | 4346 | `			/* Invalid format specifer */` |
|      3 | 4347 | `			zWorker[0] = '?';` |
|      3 | 4348 | `			length = (int)sizeof(char);` |
|      2 | 4349 | `			break;` |
|      - | 4350 | `		}` |
|      - | 4351 | `		 /*` |
|      - | 4352 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 4353 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 4354 | `		 ** the output.` |
|      - | 4355 | `		 */` |
|    127 | 4356 | `    if( !flag_leftjustify ){` |
|      - | 4357 | `      register int nspace;` |
|    119 | 4358 | `      nspace = width-length;` |
|    119 | 4359 | `      if( nspace>0 ){` |
|      5 | 4360 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4361 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4362 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4363 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4364 | `			}` |
|    ! 0 | 4365 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4366 | `        }` |
|      5 | 4367 | `        if( nspace>0 ){` |
|      5 | 4368 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      5 | 4369 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4370 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4371 | `			}` |
|      2 | 4372 | `		}` |
|      2 | 4373 | `      }` |
|     59 | 4374 | `    }` |
|    127 | 4375 | `    if( length>0 ){` |
|    127 | 4376 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    127 | 4377 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4378 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4379 | `		}` |
|     63 | 4380 | `    }` |
|    127 | 4381 | `    if( flag_leftjustify ){` |
|      - | 4382 | `      register int nspace;` |
|      9 | 4383 | `      nspace = width-length;` |
|      9 | 4384 | `      if( nspace>0 ){` |
|      9 | 4385 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4386 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4387 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4388 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4389 | `			}` |
|    ! 0 | 4390 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4391 | `        }` |
|      9 | 4392 | `        if( nspace>0 ){` |
|      9 | 4393 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      9 | 4394 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4395 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4396 | `			}` |
|      4 | 4397 | `		}` |
|      4 | 4398 | `      }` |
|      4 | 4399 | `    }` |
|      1 | 4400 | ` }/* for(;;) */` |
|    121 | 4401 | `	return SXRET_OK;` |
|     61 | 4402 |  |
|      - | 4403 | `/*` |
|      - | 4404 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 4405 | ` */` |
|     84 | 4406 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4407 |  |
|      - | 4408 | `	/* Consume directly */` |
|     85 | 4409 | `	ph7_result_string(pCtx,zInput,nLen);` |
|     42 | 4410 | `	SXUNUSED(pUserData); /* cc warning */` |
|     85 | 4411 | `	return PH7_OK;` |
|      1 | 4412 |  |
|      - | 4413 | `/*` |
|      - | 4414 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 4415 | ` *  Return a formatted string.` |
|      - | 4416 | ` * Parameters` |
|      - | 4417 | ` *  $format` |
|      - | 4418 | ` *    The format string (see block comment above)` |
|      - | 4419 | ` * Return` |
|      - | 4420 | ` *  A string produced according to the formatting string format.` |
|      - | 4421 | ` */` |
|     56 | 4422 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4423 |  |
|      - | 4424 | `	const char *zFormat;` |
|      - | 4425 | `	int nLen;` |
|     57 | 4426 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4427 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 4428 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4429 | `		return PH7_OK;` |
|      - | 4430 | `	}` |
|      - | 4431 | `	/* Extract the string format */` |
|     55 | 4432 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     55 | 4433 | `	if( nLen < 1 ){` |
|      - | 4434 | `		/* Empty string */` |
|    ! 0 | 4435 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4436 | `		return PH7_OK;` |
|      - | 4437 | `	}` |
|      - | 4438 | `	/* Format the string */` |
|     55 | 4439 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,0,FALSE);` |
|     55 | 4440 | `	return PH7_OK;` |
|     29 | 4441 |  |
|      - | 4442 | `/*` |
|      - | 4443 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 4444 | ` */` |
|    110 | 4445 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4446 |  |
|    111 | 4447 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 4448 | `	/* Call the VM output consumer directly */` |
|    111 | 4449 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 4450 | `	/* Increment counter */` |
|    111 | 4451 | `	*pCounter += nLen;` |
|    111 | 4452 | `	return PH7_OK;` |
|      1 | 4453 |  |
|      - | 4454 | `/*` |
|      - | 4455 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 4456 | ` *  Output a formatted string.` |
|      - | 4457 | ` * Parameters` |
|      - | 4458 | ` *  $format` |
|      - | 4459 | ` *   See sprintf() for a description of format.` |
|      - | 4460 | ` * Return` |
|      - | 4461 | ` *  The length of the outputted string.` |
|      - | 4462 | ` */` |
|     42 | 4463 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4464 |  |
|     43 | 4465 | `	ph7_int64 nCounter = 0;` |
|      - | 4466 | `	const char *zFormat;` |
|      - | 4467 | `	int nLen;` |
|     43 | 4468 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4469 | `		/* Missing/Invalid arguments,return 0 */` |
|      3 | 4470 | `		ph7_result_int(pCtx,0);` |
|      3 | 4471 | `		return PH7_OK;` |
|      - | 4472 | `	}` |
|      - | 4473 | `	/* Extract the string format */` |
|     41 | 4474 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 4475 | `	if( nLen < 1 ){` |
|      - | 4476 | `		/* Empty string */` |
|    ! 0 | 4477 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4478 | `		return PH7_OK;` |
|      - | 4479 | `	}` |
|      - | 4480 | `	/* Format the string */` |
|     41 | 4481 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 4482 | `	/* Return the length of the outputted string */` |
|     41 | 4483 | `	ph7_result_int64(pCtx,nCounter);` |
|     41 | 4484 | `	return PH7_OK;` |
|     22 | 4485 |  |
|      - | 4486 | `/*` |
|      - | 4487 | ` * int vprintf(string $format,array $args)` |
|      - | 4488 | ` *  Output a formatted string.` |
|      - | 4489 | ` * Parameters` |
|      - | 4490 | ` *  $format` |
|      - | 4491 | ` *   See sprintf() for a description of format.` |
|      - | 4492 | ` * Return` |
|      - | 4493 | ` *  The length of the outputted string.` |
|      - | 4494 | ` */` |
|      2 | 4495 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4496 |  |
|      3 | 4497 | `	ph7_int64 nCounter = 0;` |
|      - | 4498 | `	const char *zFormat;` |
|      - | 4499 | `	ph7_hashmap *pMap;` |
|      - | 4500 | `	SySet sArg;` |
|      - | 4501 | `	int nLen,n;` |
|      3 | 4502 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4503 | `		/* Missing/Invalid arguments,return 0 */` |
|    ! 0 | 4504 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4505 | `		return PH7_OK;` |
|      - | 4506 | `	}` |
|      - | 4507 | `	/* Extract the string format */` |
|      3 | 4508 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4509 | `	if( nLen < 1 ){` |
|      - | 4510 | `		/* Empty string */` |
|    ! 0 | 4511 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4512 | `		return PH7_OK;` |
|      - | 4513 | `	}` |
|      - | 4514 | `	/* Point to the hashmap */` |
|      3 | 4515 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4516 | `	/* Extract arguments from the hashmap */` |
|      3 | 4517 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4518 | `	/* Format the string */` |
|      3 | 4519 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 4520 | `	/* Return the length of the outputted string */` |
|      3 | 4521 | `	ph7_result_int64(pCtx,nCounter);` |
|      - | 4522 | `	/* Release the container */` |
|      3 | 4523 | `	SySetRelease(&sArg);` |
|      3 | 4524 | `	return PH7_OK;` |
|      2 | 4525 |  |
|      - | 4526 | `/*` |
|      - | 4527 | ` * int vsprintf(string $format,array $args)` |
|      - | 4528 | ` *  Output a formatted string.` |
|      - | 4529 | ` * Parameters` |
|      - | 4530 | ` *  $format` |
|      - | 4531 | ` *   See sprintf() for a description of format.` |
|      - | 4532 | ` * Return` |
|      - | 4533 | ` *  A string produced according to the formatting string format.` |
|      - | 4534 | ` */` |
|     10 | 4535 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4536 |  |
|      - | 4537 | `	const char *zFormat;` |
|      - | 4538 | `	ph7_hashmap *pMap;` |
|      - | 4539 | `	SySet sArg;` |
|      - | 4540 | `	int nLen,n;` |
|     11 | 4541 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4542 | `		/* Missing/Invalid arguments,return the empty string */` |
|      5 | 4543 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 4544 | `		return PH7_OK;` |
|      - | 4545 | `	}` |
|      - | 4546 | `	/* Extract the string format */` |
|      7 | 4547 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4548 | `	if( nLen < 1 ){` |
|      - | 4549 | `		/* Empty string */` |
|    ! 0 | 4550 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4551 | `		return PH7_OK;` |
|      - | 4552 | `	}` |
|      - | 4553 | `	/* Point to hashmap */` |
|      7 | 4554 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4555 | `	/* Extract arguments from the hashmap */` |
|      7 | 4556 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4557 | `	/* Format the string */` |
|      7 | 4558 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),0,TRUE);` |
|      - | 4559 | `	/* Release the container */` |
|      7 | 4560 | `	SySetRelease(&sArg);` |
|      7 | 4561 | `	return PH7_OK;` |
|      6 | 4562 |  |
|      - | 4563 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 4564 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4565 | `/*` |
|      - | 4566 | ` * Symisc eXtension.` |
|      - | 4567 | ` * string size_format(int64 $size)` |
|      - | 4568 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 4569 | ` *  Example:` |
|      - | 4570 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 4571 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 4572 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 4573 | ` * Parameter` |
|      - | 4574 | ` *  $size` |
|      - | 4575 | ` *    Entity size in bytes.` |
|      - | 4576 | ` * Return` |
|      - | 4577 | ` *   Formatted string representation of the given size.` |
|      - | 4578 | ` */` |
|     24 | 4579 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4580 |  |
|      - | 4581 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 4582 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 4583 | `	sxi32 nRest,i_32;` |
|      - | 4584 | `	ph7_int64 iSize;` |
|     25 | 4585 | `	int c = -1; /* index in zUnit[] */` |
|      - | 4586 |  |
|     25 | 4587 | `	if( nArg < 1 ){` |
|      - | 4588 | `		/* Missing argument,return the empty string */` |
|      3 | 4589 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4590 | `		return PH7_OK;` |
|      - | 4591 | `	}` |
|      - | 4592 | `	/* Extract the given size */` |
|     23 | 4593 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 4594 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 4595 | `		/* Don't bother formatting,return immediately */` |
|      5 | 4596 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 4597 | `		return PH7_OK;` |
|      - | 4598 | `	}` |
|     19 | 4599 | `	for(;;){` |
|     39 | 4600 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 4601 | `		iSize >>= 10;` |
|     39 | 4602 | `		c++;` |
|     39 | 4603 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 4604 | `			break;` |
|      - | 4605 | `		}` |
|      1 | 4606 | `	}` |
|     19 | 4607 | `	nRest /= 100;` |
|     19 | 4608 | `	if( nRest > 9 ){` |
|    ! 0 | 4609 | `		nRest = 9;` |
|    ! 0 | 4610 | `	}` |
|     19 | 4611 | `	if( iSize > 999 ){` |
|    ! 0 | 4612 | `		c++;` |
|    ! 0 | 4613 | `		nRest = 9;` |
|    ! 0 | 4614 | `		iSize = 0;` |
|    ! 0 | 4615 | `	}` |
|     19 | 4616 | `	i_32 = (sxi32)iSize;` |
|      - | 4617 | `	/* Format */` |
|     19 | 4618 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 4619 | `	return PH7_OK;` |
|     13 | 4620 |  |
|      - | 4621 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4622 | `/*` |
|      - | 4623 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 4624 | ` *   Calculate the md5 hash of a string.` |
|      - | 4625 | ` * Parameter` |
|      - | 4626 | ` *  $str` |
|      - | 4627 | ` *   Input string` |
|      - | 4628 | ` * $raw_output` |
|      - | 4629 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4630 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4631 | ` * Return` |
|      - | 4632 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 4633 | ` */` |
|     10 | 4634 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4635 |  |
|      - | 4636 | `	unsigned char zDigest[16];` |
|     11 | 4637 | `	int raw_output = FALSE;` |
|      - | 4638 | `	const void *pIn;` |
|      - | 4639 | `	int nLen;` |
|     11 | 4640 | `	if( nArg < 1 ){` |
|      - | 4641 | `		/* Missing arguments,return the empty string */` |
|      3 | 4642 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4643 | `		return PH7_OK;` |
|      - | 4644 | `	}` |
|      - | 4645 | `	/* Extract the input string */` |
|      9 | 4646 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 4647 | `	if( nLen < 1 ){` |
|      - | 4648 | `		/* Empty string */` |
|    ! 0 | 4649 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4650 | `		return PH7_OK;` |
|      - | 4651 | `	}` |
|      9 | 4652 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      3 | 4653 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 | 4654 | `	}` |
|      - | 4655 | `	/* Compute the MD5 digest */` |
|      9 | 4656 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|      9 | 4657 | `	if( raw_output ){` |
|      - | 4658 | `		/* Output raw digest */` |
|      3 | 4659 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      2 | 4660 | `	}else{` |
|      - | 4661 | `		/* Perform a binary to hex conversion */` |
|      7 | 4662 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4663 | `	}` |
|      9 | 4664 | `	return PH7_OK;` |
|      6 | 4665 |  |
|      - | 4666 | `/*` |
|      - | 4667 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 4668 | ` *   Calculate the sha1 hash of a string.` |
|      - | 4669 | ` * Parameter` |
|      - | 4670 | ` *  $str` |
|      - | 4671 | ` *   Input string` |
|      - | 4672 | ` * $raw_output` |
|      - | 4673 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4674 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4675 | ` * Return` |
|      - | 4676 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 4677 | ` */` |
|      8 | 4678 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4679 |  |
|      - | 4680 | `	unsigned char zDigest[20];` |
|      9 | 4681 | `	int raw_output = FALSE;` |
|      - | 4682 | `	const void *pIn;` |
|      - | 4683 | `	int nLen;` |
|      9 | 4684 | `	if( nArg < 1 ){` |
|      - | 4685 | `		/* Missing arguments,return the empty string */` |
|      3 | 4686 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4687 | `		return PH7_OK;` |
|      - | 4688 | `	}` |
|      - | 4689 | `	/* Extract the input string */` |
|      7 | 4690 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4691 | `	if( nLen < 1 ){` |
|      - | 4692 | `		/* Empty string */` |
|    ! 0 | 4693 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4694 | `		return PH7_OK;` |
|      - | 4695 | `	}` |
|      7 | 4696 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      3 | 4697 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 | 4698 | `	}` |
|      - | 4699 | `	/* Compute the SHA1 digest */` |
|      7 | 4700 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|      7 | 4701 | `	if( raw_output ){` |
|      - | 4702 | `		/* Output raw digest */` |
|      3 | 4703 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      2 | 4704 | `	}else{` |
|      - | 4705 | `		/* Perform a binary to hex conversion */` |
|      5 | 4706 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4707 | `	}` |
|      7 | 4708 | `	return PH7_OK;` |
|      5 | 4709 |  |
|      - | 4710 | `/*` |
|      - | 4711 | ` * int64 crc32(string $str)` |
|      - | 4712 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 4713 | ` * Parameter` |
|      - | 4714 | ` *  $str` |
|      - | 4715 | ` *   Input string` |
|      - | 4716 | ` * Return` |
|      - | 4717 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 4718 | ` */` |
|      4 | 4719 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4720 |  |
|      - | 4721 | `	const void *pIn;` |
|      - | 4722 | `	sxu32 nCRC;` |
|      - | 4723 | `	int nLen;` |
|      5 | 4724 | `	if( nArg < 1 ){` |
|      - | 4725 | `		/* Missing arguments,return 0 */` |
|      3 | 4726 | `		ph7_result_int(pCtx,0);` |
|      3 | 4727 | `		return PH7_OK;` |
|      - | 4728 | `	}` |
|      - | 4729 | `	/* Extract the input string */` |
|      3 | 4730 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4731 | `	if( nLen < 1 ){` |
|      - | 4732 | `		/* Empty string */` |
|    ! 0 | 4733 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4734 | `		return PH7_OK;` |
|      - | 4735 | `	}` |
|      - | 4736 | `	/* Calculate the sum */` |
|      3 | 4737 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 4738 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 4739 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 4740 | `	return PH7_OK;` |
|      3 | 4741 |  |
|      - | 4742 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 4743 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4744 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 4745 | `/*` |
|      - | 4746 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 4747 |  |
|      - | 4748 | ` */` |
|      4 | 4749 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 4750 | `	const char *zInput, /* Raw input */` |
|      - | 4751 | `	int nByte,  /* Input length */` |
|      - | 4752 | `	int delim,  /* Delimiter */` |
|      - | 4753 | `	int encl,   /* Enclosure */` |
|      - | 4754 | `	int escape,  /* Escape character */` |
|      - | 4755 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 4756 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 4757 | `	)` |
|      1 | 4758 |  |
|      5 | 4759 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 4760 | `	const char *zIn = zInput;` |
|      - | 4761 | `	const char *zPtr;` |
|      - | 4762 | `	int isEnc;` |
|      - | 4763 | `	/* Start processing */` |
|      8 | 4764 | `	for(;;){` |
|     17 | 4765 | `		if( zIn >= zEnd ){` |
|      - | 4766 | `			/* No more input to process */` |
|      5 | 4767 | `			break;` |
|      - | 4768 | `		}` |
|     13 | 4769 | `		isEnc = 0;` |
|     13 | 4770 | `		zPtr = zIn;` |
|      - | 4771 | `		/* Find the first delimiter */` |
|     27 | 4772 | `		while( zIn < zEnd ){` |
|     23 | 4773 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 4774 | `				/* Delimiter found,break imediately */` |
|      5 | 4775 | `				break;` |
|     15 | 4776 | `			}else if( zIn[0] == encl ){` |
|      - | 4777 | `				/* Inside enclosure? */` |
|    ! 0 | 4778 | `				isEnc = !isEnc;` |
|     15 | 4779 | `			}else if( zIn[0] == escape ){` |
|      - | 4780 | `				/* Escape sequence */` |
|    ! 0 | 4781 | `				zIn++;` |
|    ! 0 | 4782 | `			}` |
|      - | 4783 | `			/* Advance the cursor */` |
|     15 | 4784 | `			zIn++;` |
|      1 | 4785 | `		}` |
|     13 | 4786 | `		if( zIn > zPtr ){` |
|     13 | 4787 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 4788 | `			sxi32 rc;` |
|      - | 4789 | `			/* Invoke the supllied callback */` |
|     13 | 4790 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 4791 | `				zPtr++;` |
|    ! 0 | 4792 | `				nByteChunk-=2;` |
|    ! 0 | 4793 | `			}` |
|     13 | 4794 | `			if( nByteChunk > 0 ){` |
|     13 | 4795 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 4796 | `				if( rc == SXERR_ABORT ){` |
|      - | 4797 | `					/* User callback request an operation abort */` |
|    ! 0 | 4798 | `					break;` |
|      - | 4799 | `				}` |
|      6 | 4800 | `			}` |
|      6 | 4801 | `		}` |
|      - | 4802 | `		/* Ignore trailing delimiter */` |
|     21 | 4803 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 4804 | `			zIn++;` |
|      1 | 4805 | `		}` |
|      1 | 4806 | `	}` |
|      5 | 4807 | `	return SXRET_OK;` |
|      1 | 4808 |  |
|      - | 4809 | `/*` |
|      - | 4810 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 4811 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 4812 | ` * argument to this callback.` |
|      - | 4813 | ` */` |
|     12 | 4814 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 4815 |  |
|     13 | 4816 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 4817 | `	ph7_value sEntry;` |
|      - | 4818 | `	SyString sToken;` |
|      - | 4819 | `	/* Insert the token in the given array */` |
|     13 | 4820 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 4821 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 4822 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 4823 | `	if( sToken.nByte < 1){` |
|    ! 0 | 4824 | `		return SXRET_OK;` |
|      - | 4825 | `	}` |
|     13 | 4826 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 4827 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 4828 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 4829 | `	return SXRET_OK;` |
|      7 | 4830 |  |
|      - | 4831 | `/*` |
|      - | 4832 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 4833 | ` *  Parse a CSV string into an array.` |
|      - | 4834 | ` * Parameters` |
|      - | 4835 | ` *  $input` |
|      - | 4836 | ` *   The string to parse.` |
|      - | 4837 | ` *  $delimiter` |
|      - | 4838 | ` *   Set the field delimiter (one character only).` |
|      - | 4839 | ` *  $enclosure` |
|      - | 4840 | ` *   Set the field enclosure character (one character only).` |
|      - | 4841 | ` *  $escape` |
|      - | 4842 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 4843 | ` * Return` |
|      - | 4844 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 4845 | ` */` |
|      4 | 4846 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4847 |  |
|      - | 4848 | `	const char *zInput,*zPtr;` |
|      - | 4849 | `	ph7_value *pArray;` |
|      5 | 4850 | `	int delim  = ',';   /* Delimiter */` |
|      5 | 4851 | `	int encl   = '"' ;  /* Enclosure */` |
|      5 | 4852 | `	int escape = '\\';  /* Escape character */` |
|      - | 4853 | `	int nLen;` |
|      5 | 4854 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4855 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 4856 | `		ph7_result_null(pCtx);` |
|      3 | 4857 | `		return PH7_OK;` |
|      - | 4858 | `	}` |
|      - | 4859 | `	/* Extract the raw input */` |
|      3 | 4860 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4861 | `	if( nArg > 1 ){` |
|      - | 4862 | `		int i;` |
|      3 | 4863 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 4864 | `			/* Extract the delimiter */` |
|      3 | 4865 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 4866 | `			if( i > 0 ){` |
|      3 | 4867 | `				delim = zPtr[0];` |
|      1 | 4868 | `			}` |
|      1 | 4869 | `		}` |
|      3 | 4870 | `		if( nArg > 2 ){` |
|      3 | 4871 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 4872 | `				/* Extract the enclosure */` |
|      3 | 4873 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 4874 | `				if( i > 0 ){` |
|      3 | 4875 | `					encl = zPtr[0];` |
|      1 | 4876 | `				}` |
|      1 | 4877 | `			}` |
|      3 | 4878 | `			if( nArg > 3 ){` |
|      3 | 4879 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 4880 | `					/* Extract the escape character */` |
|      3 | 4881 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 4882 | `					if( i > 0 ){` |
|      3 | 4883 | `						escape = zPtr[0];` |
|      1 | 4884 | `					}` |
|      1 | 4885 | `				}` |
|      1 | 4886 | `			}` |
|      1 | 4887 | `		}` |
|      1 | 4888 | `	}` |
|      - | 4889 | `	/* Create our array */` |
|      3 | 4890 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4891 | `	if( pArray == 0 ){` |
|    ! 0 | 4892 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4893 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4894 | `		return PH7_OK;` |
|      - | 4895 | `	}` |
|      - | 4896 | `	/* Parse the raw input */` |
|      3 | 4897 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 4898 | `	/* Return the freshly created array */` |
|      3 | 4899 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4900 | `	return PH7_OK;` |
|      3 | 4901 |  |
|      - | 4902 | `/*` |
|      - | 4903 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 4904 | ` * container.` |
|      - | 4905 | ` * Refer to [strip_tags()].` |
|      - | 4906 | ` */` |
|     10 | 4907 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 4908 |  |
|     11 | 4909 | `	const char *zEnd = &zTag[nByte];` |
|      - | 4910 | `	const char *zPtr;` |
|      - | 4911 | `	SyString sEntry;` |
|      - | 4912 | `	/* Strip tags */` |
|     10 | 4913 | `	for(;;){` |
|     45 | 4914 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 4915 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 4916 | `				zTag++;` |
|      1 | 4917 | `		}` |
|     21 | 4918 | `		if( zTag >= zEnd ){` |
|     11 | 4919 | `			break;` |
|      - | 4920 | `		}` |
|     11 | 4921 | `		zPtr = zTag;` |
|      - | 4922 | `		/* Delimit the tag */` |
|     25 | 4923 | `		while(zTag < zEnd ){` |
|     25 | 4924 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 4925 | `				/* UTF-8 stream */` |
|      3 | 4926 | `				zTag++;` |
|      5 | 4927 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 4928 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 4929 | `				break;` |
|    ! 0 | 4930 | `			}else{` |
|     13 | 4931 | `				zTag++;` |
|      - | 4932 | `			}` |
|      1 | 4933 | `		}` |
|     11 | 4934 | `		if( zTag > zPtr ){` |
|      - | 4935 | `			/* Perform the insertion */` |
|     11 | 4936 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 4937 | `			SyStringFullTrim(&sEntry);` |
|     11 | 4938 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 4939 | `		}` |
|      - | 4940 | `		/* Jump the trailing '>' */` |
|     11 | 4941 | `		zTag++;` |
|      1 | 4942 | `	}` |
|     11 | 4943 | `	return SXRET_OK;` |
|      1 | 4944 |  |
|      - | 4945 | `/*` |
|      - | 4946 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 4947 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 4948 | ` * Refer to [strip_tags()].` |
|      - | 4949 | ` */` |
|     36 | 4950 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 4951 |  |
|     37 | 4952 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 4953 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 4954 | `		SyString sTag;` |
|     85 | 4955 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 4956 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 4957 | `			zTag++;` |
|      1 | 4958 | `		}` |
|      - | 4959 | `		/* Delimit the tag */` |
|     25 | 4960 | `		zCur = zTag;` |
|     77 | 4961 | `		while(zTag < zEnd ){` |
|     77 | 4962 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 4963 | `				/* UTF-8 stream */` |
|      5 | 4964 | `				zTag++;` |
|      9 | 4965 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 4966 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 4967 | `				break;` |
|    ! 0 | 4968 | `			}else{` |
|     49 | 4969 | `				zTag++;` |
|      - | 4970 | `			}` |
|      1 | 4971 | `		}` |
|     25 | 4972 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 4973 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 4974 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 4975 | `		if( sTag.nByte > 0 ){` |
|      - | 4976 | `			SyString *aEntry,*pEntry;` |
|      - | 4977 | `			sxi32 rc;` |
|      - | 4978 | `			sxu32 n;` |
|      - | 4979 | `			/* Perform the lookup */` |
|     25 | 4980 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 4981 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 4982 | `				pEntry = &aEntry[n];` |
|      - | 4983 | `				/* Do the comparison */` |
|     25 | 4984 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 4985 | `				if( !rc ){` |
|     21 | 4986 | `					return SXRET_OK;` |
|      - | 4987 | `				}` |
|      3 | 4988 | `			}` |
|      2 | 4989 | `		}` |
|      2 | 4990 | `	}` |
|      - | 4991 | `	/* No such tag */` |
|     17 | 4992 | `	return SXERR_NOTFOUND;` |
|     19 | 4993 |  |
|      - | 4994 | `/*` |
|      - | 4995 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 4996 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 4997 | ` * Refer to [strip_tags()].` |
|      - | 4998 | ` */` |
|     16 | 4999 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 5000 |  |
|     17 | 5001 | `	const char *zEnd = &zIn[nByte];` |
|      - | 5002 | `	const char *zPtr,*zTag;` |
|      - | 5003 | `	SySet sSet;` |
|      - | 5004 | `	/* initialize the set of allowed tags */` |
|     17 | 5005 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 5006 | `	if( nTaglen > 0 ){` |
|      - | 5007 | `		/* Set of allowed tags */` |
|     11 | 5008 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 5009 | `	}` |
|      - | 5010 | `	/* Set the empty string */` |
|     17 | 5011 | `	ph7_result_string(pCtx,"",0);` |
|      - | 5012 | `	/* Start processing */` |
|     26 | 5013 | `	for(;;){` |
|     53 | 5014 | `		if(zIn >= zEnd){` |
|      - | 5015 | `			/* No more input to process */` |
|     15 | 5016 | `			break;` |
|      - | 5017 | `		}` |
|     39 | 5018 | `		zPtr = zIn;` |
|      - | 5019 | `		/* Find a tag */` |
|    133 | 5020 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 5021 | `			zIn++;` |
|      1 | 5022 | `		}` |
|     39 | 5023 | `		if( zIn > zPtr ){` |
|      - | 5024 | `			/* Consume raw input */` |
|     21 | 5025 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 5026 | `		}` |
|      - | 5027 | `		/* Ignore trailing null bytes */` |
|     39 | 5028 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 5029 | `			zIn++;` |
|    ! 0 | 5030 | `		}` |
|     39 | 5031 | `		if(zIn >= zEnd){` |
|      - | 5032 | `			/* No more input to process */` |
|      3 | 5033 | `			break;` |
|      - | 5034 | `		}` |
|     37 | 5035 | `		if( zIn[0] == '<' ){` |
|      - | 5036 | `			sxi32 rc;` |
|     37 | 5037 | `			zTag = zIn++;` |
|      - | 5038 | `			/* Delimit the tag */` |
|    127 | 5039 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 5040 | `				zIn++;` |
|      1 | 5041 | `			}` |
|     37 | 5042 | `			if( zIn < zEnd ){` |
|     37 | 5043 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 5044 | `			}` |
|      - | 5045 | `			/* Query the set */` |
|     37 | 5046 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 5047 | `			if( rc == SXRET_OK ){` |
|      - | 5048 | `				/* Keep the tag */` |
|     21 | 5049 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 5050 | `			}` |
|     18 | 5051 | `		}` |
|      1 | 5052 | `	}` |
|      - | 5053 | `	/* Cleanup */` |
|     17 | 5054 | `	SySetRelease(&sSet);` |
|     17 | 5055 | `	return SXRET_OK;` |
|      1 | 5056 |  |
|      - | 5057 | `/*` |
|      - | 5058 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 5059 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 5060 | ` * Parameters` |
|      - | 5061 | ` *  $str` |
|      - | 5062 | ` *  The input string.` |
|      - | 5063 | ` * $allowable_tags` |
|      - | 5064 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 5065 | ` * Return` |
|      - | 5066 | ` *  Returns the stripped string.` |
|      - | 5067 | ` */` |
|     16 | 5068 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5069 |  |
|     17 | 5070 | `	const char *zTaglist = 0;` |
|      - | 5071 | `	const char *zString;` |
|     17 | 5072 | `	int nTaglen = 0;` |
|      - | 5073 | `	int nLen;` |
|     17 | 5074 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5075 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 5076 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5077 | `		return PH7_OK;` |
|      - | 5078 | `	}` |
|      - | 5079 | `	/* Point to the raw string */` |
|     15 | 5080 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 5081 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 5082 | `		/* Allowed tag */` |
|     11 | 5083 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 5084 | `	}` |
|      - | 5085 | `	/* Process input */` |
|     15 | 5086 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 5087 | `	return PH7_OK;` |
|      9 | 5088 |  |
|      - | 5089 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5090 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5091 | `/*` |
|      - | 5092 | ` * string str_shuffle(string $str)` |
|      - | 5093 |  |
|      - | 5094 | ` *  Randomly shuffles a string.` |
|      - | 5095 | ` * Parameters` |
|      - | 5096 | ` *  $str` |
|      - | 5097 | ` *   The input string.` |
|      - | 5098 | ` * Return` |
|      - | 5099 | ` *  Returns the shuffled string.` |
|      - | 5100 | ` */` |
|     12 | 5101 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5102 |  |
|      - | 5103 | `	const char *zString;` |
|      - | 5104 | `	int nLen,i,c;` |
|      - | 5105 | `	sxu32 iR;` |
|     13 | 5106 | `	if( nArg < 1 ){` |
|      - | 5107 | `		/* Missing arguments,return the empty string */` |
|      3 | 5108 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5109 | `		return PH7_OK;` |
|      - | 5110 | `	}` |
|      - | 5111 | `	/* Extract the target string */` |
|     11 | 5112 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5113 | `	if( nLen < 1 ){` |
|      - | 5114 | `		/* Nothing to shuffle */` |
|      3 | 5115 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5116 | `		return PH7_OK;` |
|      - | 5117 | `	}` |
|      - | 5118 | `	/* Shuffle the string */` |
|     43 | 5119 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 5120 | `		/* Generate a random number first */` |
|     35 | 5121 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 5122 | `		/* Extract a random offset */` |
|     35 | 5123 | `		c = zString[iR % nLen];` |
|      - | 5124 | `		/* Append it */` |
|     35 | 5125 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 5126 | `	}` |
|      9 | 5127 | `	return PH7_OK;` |
|      7 | 5128 |  |
|      - | 5129 | `/*` |
|      - | 5130 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 5131 | ` *  Convert a string to an array.` |
|      - | 5132 | ` * Parameters` |
|      - | 5133 | ` * $str` |
|      - | 5134 | ` *  The input string.` |
|      - | 5135 | ` * $split_length` |
|      - | 5136 | ` *  Maximum length of the chunk.` |
|      - | 5137 | ` * Return` |
|      - | 5138 | ` *  If the optional split_length parameter is specified, the returned array` |
|      - | 5139 | ` *  will be broken down into chunks with each being split_length in length, otherwise` |
|      - | 5140 | ` *  each chunk will be one character in length. FALSE is returned if split_length is less than 1.` |
|      - | 5141 | ` *  If the split_length length exceeds the length of string, the entire string is returned` |
|      - | 5142 | ` *  as the first (and only) array element.` |
|      - | 5143 | ` */` |
|      8 | 5144 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5145 |  |
|      - | 5146 | `	const char *zString,*zEnd;` |
|      - | 5147 | `	ph7_value *pArray,*pValue;` |
|      - | 5148 | `	int split_len;` |
|      - | 5149 | `	int nLen;` |
|      9 | 5150 | `	if( nArg < 1 ){` |
|      - | 5151 | `		/* Missing arguments,return FALSE */` |
|      5 | 5152 | `		ph7_result_bool(pCtx,0);` |
|      5 | 5153 | `		return PH7_OK;` |
|      - | 5154 | `	}` |
|      - | 5155 | `	/* Point to the target string */` |
|      5 | 5156 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 5157 | `	if( nLen < 1 ){` |
|      - | 5158 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 5159 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5160 | `		return PH7_OK;` |
|      - | 5161 | `	}` |
|      5 | 5162 | `	split_len = (int)sizeof(char);` |
|      5 | 5163 | `	if( nArg > 1 ){` |
|      - | 5164 | `		/* Split length */` |
|      5 | 5165 | `		split_len = ph7_value_to_int(apArg[1]);` |
|      5 | 5166 | `		if( split_len < 1 ){` |
|      - | 5167 | `			/* Invalid length,return FALSE */` |
|      3 | 5168 | `			ph7_result_bool(pCtx,0);` |
|      3 | 5169 | `			return PH7_OK;` |
|      - | 5170 | `		}` |
|      3 | 5171 | `		if( split_len > nLen ){` |
|    ! 0 | 5172 | `			split_len = nLen;` |
|    ! 0 | 5173 | `		}` |
|      1 | 5174 | `	}` |
|      - | 5175 | `	/* Create the array and the scalar value */` |
|      3 | 5176 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 5177 | `	/*Chunk value */` |
|      3 | 5178 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 5179 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 5180 | `		/* Return FALSE */` |
|    ! 0 | 5181 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5182 | `		return PH7_OK;` |
|      - | 5183 | `	}` |
|      - | 5184 | `	/* Point to the end of the string */` |
|      3 | 5185 | `	zEnd = &zString[nLen];` |
|      - | 5186 | `	/* Perform the requested operation */` |
|      7 | 5187 | `	for(;;){` |
|      - | 5188 | `		int nMax;` |
|      9 | 5189 | `		if( zString >= zEnd ){` |
|      - | 5190 | `			/* No more input to process */` |
|      3 | 5191 | `			break;` |
|      - | 5192 | `		}` |
|      7 | 5193 | `		nMax = (int)(zEnd-zString);` |
|      7 | 5194 | `		if( nMax < split_len ){` |
|    ! 0 | 5195 | `			split_len = nMax;` |
|    ! 0 | 5196 | `		}` |
|      - | 5197 | `		/* Copy the current chunk */` |
|      7 | 5198 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 5199 | `		/* Insert it */` |
|      7 | 5200 | `		ph7_array_add_elem(pArray,0,pValue); /* Will make it's own copy */` |
|      - | 5201 | `		/* reset the string cursor */` |
|      7 | 5202 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 5203 | `		/* Update position */` |
|      7 | 5204 | `		zString += split_len;` |
|      1 | 5205 | `	}` |
|      - | 5206 | `	/*` |
|      - | 5207 | `	 * Return the array.` |
|      - | 5208 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 5209 | `	 * upon we return from this function.` |
|      - | 5210 | `	 */` |
|      3 | 5211 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5212 | `	return PH7_OK;` |
|      5 | 5213 |  |
|      - | 5214 | `/*` |
|      - | 5215 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 5216 | ` * Refer to [strspn()].` |
|      - | 5217 | ` */` |
|     28 | 5218 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 5219 |  |
|     29 | 5220 | `	const char *zIn = *pzIn;` |
|      - | 5221 | `	const char *zPtr;` |
|      - | 5222 | `	/* Ignore leading white spaces */` |
|     29 | 5223 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 5224 | `		zIn++;` |
|    ! 0 | 5225 | `	}` |
|     29 | 5226 | `	if( zIn >= zEnd ){` |
|      - | 5227 | `		/* End of input */` |
|    ! 0 | 5228 | `		return SXERR_EOF;` |
|      - | 5229 | `	}` |
|     29 | 5230 | `	zPtr = zIn;` |
|      - | 5231 | `	/* Extract the token */` |
|    201 | 5232 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 5233 | `		zIn++;` |
|      1 | 5234 | `	}` |
|     29 | 5235 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5236 | `	/* Synchronize pointers */` |
|     29 | 5237 | `	*pzIn = zIn;` |
|      - | 5238 | `	/* Return to the caller */` |
|     29 | 5239 | `	return SXRET_OK;` |
|     15 | 5240 |  |
|      - | 5241 | `/*` |
|      - | 5242 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 5243 | ` * return the longest match.` |
|      - | 5244 | ` * Refer to [strspn()].` |
|      - | 5245 | ` */` |
|     18 | 5246 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5247 |  |
|     19 | 5248 | `	const char *zEnd = &zString[nLen];` |
|     19 | 5249 | `	const char *zIn = zString;` |
|      - | 5250 | `	int i,c;` |
|     45 | 5251 | `	for(;;){` |
|     91 | 5252 | `		if( zString >= zEnd ){` |
|      7 | 5253 | `			break;` |
|      - | 5254 | `		}` |
|      - | 5255 | `		/* Extract current character */` |
|     85 | 5256 | `		c = zString[0];` |
|      - | 5257 | `		/* Perform the lookup */` |
|    383 | 5258 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 5259 | `			if( c == zMask[i] ){` |
|      - | 5260 | `				/* Character found */` |
|     73 | 5261 | `				break;` |
|      - | 5262 | `			}` |
|    150 | 5263 | `		}` |
|     85 | 5264 | `		if( i >= nMaskLen ){` |
|      - | 5265 | `			/* Character not in the current mask,break immediately */` |
|     13 | 5266 | `			break;` |
|      - | 5267 | `		}` |
|      - | 5268 | `		/* Advance cursor */` |
|     73 | 5269 | `		zString++;` |
|      1 | 5270 | `	}` |
|      - | 5271 | `	/* Longest match */` |
|     19 | 5272 | `	return (int)(zString-zIn);` |
|      1 | 5273 |  |
|      - | 5274 | `/*` |
|      - | 5275 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 5276 | ` * Refer to [strcspn()].` |
|      - | 5277 | ` */` |
|     10 | 5278 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5279 |  |
|     11 | 5280 | `	const char *zEnd = &zString[nLen];` |
|     11 | 5281 | `	const char *zIn = zString;` |
|      - | 5282 | `	int i,c;` |
|     12 | 5283 | `	for(;;){` |
|     25 | 5284 | `		if( zString >= zEnd ){` |
|      3 | 5285 | `			break;` |
|      - | 5286 | `		}` |
|      - | 5287 | `		/* Extract current character */` |
|     23 | 5288 | `		c = zString[0];` |
|      - | 5289 | `		/* Perform the lookup */` |
|     51 | 5290 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 5291 | `			if( c == zMask[i] ){` |
|      9 | 5292 | `				break;` |
|      - | 5293 | `			}` |
|     15 | 5294 | `		}` |
|     23 | 5295 | `		if( i < nMaskLen ){` |
|      - | 5296 | `			/* Character in the current mask,break immediately */` |
|      9 | 5297 | `			break;` |
|      - | 5298 | `		}` |
|      - | 5299 | `		/* Advance cursor */` |
|     15 | 5300 | `		zString++;` |
|      1 | 5301 | `	}` |
|      - | 5302 | `	/* Longest match */` |
|     11 | 5303 | `	return (int)(zString-zIn);` |
|      1 | 5304 |  |
|      - | 5305 | `/*` |
|      - | 5306 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5307 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 5308 | ` *  of characters contained within a given mask.` |
|      - | 5309 | ` * Parameters` |
|      - | 5310 | ` * $str` |
|      - | 5311 | ` *  The input string.` |
|      - | 5312 | ` * $mask` |
|      - | 5313 | ` *  The list of allowable characters.` |
|      - | 5314 | ` * $start` |
|      - | 5315 | ` *  The position in subject to start searching.` |
|      - | 5316 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5317 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5318 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5319 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5320 | ` *  start'th position from the end of subject.` |
|      - | 5321 | ` * $length` |
|      - | 5322 | ` *  The length of the segment from subject to examine.` |
|      - | 5323 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5324 | ` *  characters after the starting position.` |
|      - | 5325 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5326 | ` *  position up to length characters from the end of subject.` |
|      - | 5327 | ` * Return` |
|      - | 5328 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 5329 | ` * in mask.` |
|      - | 5330 | ` */` |
|     26 | 5331 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5332 |  |
|      - | 5333 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5334 | `	int iMasklen,iLen;` |
|      - | 5335 | `	SyString sToken;` |
|     27 | 5336 | `	int iCount = 0;` |
|      - | 5337 | `	int rc;` |
|     27 | 5338 | `	if( nArg < 2 ){` |
|      - | 5339 | `		/* Missing agruments,return zero */` |
|      3 | 5340 | `		ph7_result_int(pCtx,0);` |
|      3 | 5341 | `		return PH7_OK;` |
|      - | 5342 | `	}` |
|      - | 5343 | `	/* Extract the target string */` |
|     25 | 5344 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5345 | `	/* Extract the mask */` |
|     25 | 5346 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 5347 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 5348 | `		/* Nothing to process,return zero */` |
|      7 | 5349 | `		ph7_result_int(pCtx,0);` |
|      7 | 5350 | `		return PH7_OK;` |
|      - | 5351 | `	}` |
|     19 | 5352 | `	if( nArg > 2 ){` |
|      - | 5353 | `		int nOfft;` |
|      - | 5354 | `		/* Extract the offset */` |
|      9 | 5355 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 5356 | `		if( nOfft < 0 ){` |
|    ! 0 | 5357 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5358 | `			if( zBase > zString ){` |
|    ! 0 | 5359 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5360 | `				zString = zBase;` |
|    ! 0 | 5361 | `			}else{` |
|      - | 5362 | `				/* Invalid offset */` |
|    ! 0 | 5363 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5364 | `				return PH7_OK;` |
|      - | 5365 | `			}` |
|    ! 0 | 5366 | `		}else{` |
|      9 | 5367 | `			if( nOfft >= iLen ){` |
|      - | 5368 | `				/* Invalid offset */` |
|    ! 0 | 5369 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5370 | `				return PH7_OK;` |
|    ! 0 | 5371 | `			}else{` |
|      - | 5372 | `				/* Update offset */` |
|      9 | 5373 | `				zString += nOfft;` |
|      9 | 5374 | `				iLen -= nOfft;` |
|      - | 5375 | `			}` |
|      - | 5376 | `		}` |
|      9 | 5377 | `		if( nArg > 3 ){` |
|      - | 5378 | `			int iUserlen;` |
|      - | 5379 | `			/* Extract the desired length */` |
|      9 | 5380 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 5381 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 5382 | `				iLen = iUserlen;` |
|      2 | 5383 | `			}` |
|      4 | 5384 | `		}` |
|      4 | 5385 | `	}` |
|      - | 5386 | `	/* Point to the end of the string */` |
|     19 | 5387 | `	zEnd = &zString[iLen];` |
|      - | 5388 | `	/* Extract the first non-space token */` |
|     19 | 5389 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 5390 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5391 | `		/* Compare against the current mask */` |
|     19 | 5392 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 5393 | `	}` |
|      - | 5394 | `	/* Longest match */` |
|     19 | 5395 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 5396 | `	return PH7_OK;` |
|     14 | 5397 |  |
|      - | 5398 | `/*` |
|      - | 5399 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5400 | ` *  Find length of initial segment not matching mask.` |
|      - | 5401 | ` * Parameters` |
|      - | 5402 | ` * $str` |
|      - | 5403 | ` *  The input string.` |
|      - | 5404 | ` * $mask` |
|      - | 5405 | ` *  The list of not allowed characters.` |
|      - | 5406 | ` * $start` |
|      - | 5407 | ` *  The position in subject to start searching.` |
|      - | 5408 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5409 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5410 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5411 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5412 | ` *  start'th position from the end of subject.` |
|      - | 5413 | ` * $length` |
|      - | 5414 | ` *  The length of the segment from subject to examine.` |
|      - | 5415 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5416 | ` *  characters after the starting position.` |
|      - | 5417 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5418 | ` *  position up to length characters from the end of subject.` |
|      - | 5419 | ` * Return` |
|      - | 5420 | ` *  Returns the length of the segment as an integer.` |
|      - | 5421 | ` */` |
|     16 | 5422 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5423 |  |
|      - | 5424 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5425 | `	int iMasklen,iLen;` |
|      - | 5426 | `	SyString sToken;` |
|     17 | 5427 | `	int iCount = 0;` |
|      - | 5428 | `	int rc;` |
|     17 | 5429 | `	if( nArg < 2 ){` |
|      - | 5430 | `		/* Missing agruments,return zero */` |
|      3 | 5431 | `		ph7_result_int(pCtx,0);` |
|      3 | 5432 | `		return PH7_OK;` |
|      - | 5433 | `	}` |
|      - | 5434 | `	/* Extract the target string */` |
|     15 | 5435 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5436 | `	/* Extract the mask */` |
|     15 | 5437 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 5438 | `	if( iLen < 1 ){` |
|      - | 5439 | `		/* Nothing to process,return zero */` |
|    ! 0 | 5440 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5441 | `		return PH7_OK;` |
|      - | 5442 | `	}` |
|     15 | 5443 | `	if( iMasklen < 1 ){` |
|      - | 5444 | `		/* No given mask,return the string length */` |
|      3 | 5445 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 5446 | `		return PH7_OK;` |
|      - | 5447 | `	}` |
|     13 | 5448 | `	if( nArg > 2 ){` |
|      - | 5449 | `		int nOfft;` |
|      - | 5450 | `		/* Extract the offset */` |
|     11 | 5451 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 5452 | `		if( nOfft < 0 ){` |
|    ! 0 | 5453 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5454 | `			if( zBase > zString ){` |
|    ! 0 | 5455 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5456 | `				zString = zBase;` |
|    ! 0 | 5457 | `			}else{` |
|      - | 5458 | `				/* Invalid offset */` |
|    ! 0 | 5459 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5460 | `				return PH7_OK;` |
|      - | 5461 | `			}` |
|    ! 0 | 5462 | `		}else{` |
|     11 | 5463 | `			if( nOfft >= iLen ){` |
|      - | 5464 | `				/* Invalid offset */` |
|      3 | 5465 | `				ph7_result_int(pCtx,0);` |
|      3 | 5466 | `				return PH7_OK;` |
|    ! 0 | 5467 | `			}else{` |
|      - | 5468 | `				/* Update offset */` |
|      9 | 5469 | `				zString += nOfft;` |
|      9 | 5470 | `				iLen -= nOfft;` |
|      - | 5471 | `			}` |
|      - | 5472 | `		}` |
|      9 | 5473 | `		if( nArg > 3 ){` |
|      - | 5474 | `			int iUserlen;` |
|      - | 5475 | `			/* Extract the desired length */` |
|    ! 0 | 5476 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 5477 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 5478 | `				iLen = iUserlen;` |
|    ! 0 | 5479 | `			}` |
|    ! 0 | 5480 | `		}` |
|      4 | 5481 | `	}` |
|      - | 5482 | `	/* Point to the end of the string */` |
|     11 | 5483 | `	zEnd = &zString[iLen];` |
|      - | 5484 | `	/* Extract the first non-space token */` |
|     11 | 5485 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 5486 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5487 | `		/* Compare against the current mask */` |
|     11 | 5488 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 5489 | `	}` |
|      - | 5490 | `	/* Longest match */` |
|     11 | 5491 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 5492 | `	return PH7_OK;` |
|      9 | 5493 |  |
|      - | 5494 | `/*` |
|      - | 5495 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 5496 | ` *  Search a string for any of a set of characters.` |
|      - | 5497 | ` * Parameters` |
|      - | 5498 | ` *  $haystack` |
|      - | 5499 | ` *   The string where char_list is looked for.` |
|      - | 5500 | ` *  $char_list` |
|      - | 5501 | ` *   This parameter is case sensitive.` |
|      - | 5502 | ` * Return` |
|      - | 5503 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 5504 | ` */` |
|      6 | 5505 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5506 |  |
|      - | 5507 | `	const char *zString,*zList,*zEnd;` |
|      - | 5508 | `	int iLen,iListLen,i,c;` |
|      - | 5509 | `	sxu32 nOfft,nMax;` |
|      - | 5510 | `	sxi32 rc;` |
|      7 | 5511 | `	if( nArg < 2 ){` |
|      - | 5512 | `		/* Missing arguments,return FALSE */` |
|      3 | 5513 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5514 | `		return PH7_OK;` |
|      - | 5515 | `	}` |
|      - | 5516 | `	/* Extract the haystack and the char list */` |
|      5 | 5517 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 5518 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 5519 | `	if( iLen < 1 ){` |
|      - | 5520 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 5521 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5522 | `		return PH7_OK;` |
|      - | 5523 | `	}` |
|      - | 5524 | `	/* Point to the end of the string */` |
|      5 | 5525 | `	zEnd = &zString[iLen];` |
|      5 | 5526 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 5527 | `	/* perform the requested operation */` |
|     15 | 5528 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 5529 | `		c = zList[i];` |
|     11 | 5530 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 5531 | `		if( rc == SXRET_OK ){` |
|      5 | 5532 | `			if( nMax < nOfft ){` |
|      3 | 5533 | `				nOfft = nMax;` |
|      1 | 5534 | `			}` |
|      2 | 5535 | `		}` |
|      6 | 5536 | `	}` |
|      5 | 5537 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 5538 | `		/* No such substring,return FALSE */` |
|      3 | 5539 | `		ph7_result_bool(pCtx,0);` |
|      2 | 5540 | `	}else{` |
|      - | 5541 | `		/* Return the substring */` |
|      3 | 5542 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 5543 | `	}` |
|      5 | 5544 | `	return PH7_OK;` |
|      4 | 5545 |  |
|      - | 5546 | `/*` |
|      - | 5547 | ` * string soundex(string $str)` |
|      - | 5548 | ` *  Calculate the soundex key of a string.` |
|      - | 5549 | ` * Parameters` |
|      - | 5550 | ` *  $str` |
|      - | 5551 | ` *   The input string.` |
|      - | 5552 | ` * Return` |
|      - | 5553 | ` *  Returns the soundex key as a string.` |
|      - | 5554 | ` * Note:` |
|      - | 5555 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 5556 | ` * source tree.` |
|      - | 5557 | ` */` |
|     20 | 5558 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5559 |  |
|      - | 5560 | `	const unsigned char *zIn;` |
|      - | 5561 | `	char zResult[8];` |
|      - | 5562 | `	int i, j;` |
|      - | 5563 | `	static const unsigned char iCode[] = {` |
|      - | 5564 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 5565 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 5566 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 5567 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 5568 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 5569 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5570 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 5571 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5572 | `	};` |
|     21 | 5573 | `	if( nArg < 1 ){` |
|      - | 5574 | `		/* Missing arguments,return the empty string */` |
|      3 | 5575 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5576 | `		return PH7_OK;` |
|      - | 5577 | `	}` |
|     19 | 5578 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     19 | 5579 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     19 | 5580 | `	if( zIn[i] ){` |
|     17 | 5581 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 5582 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 5583 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 5584 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 5585 | `			if( code>0 ){` |
|     45 | 5586 | `				if( code!=prevcode ){` |
|     33 | 5587 | `					prevcode = (unsigned char)code;` |
|     33 | 5588 | `					zResult[j++] = (char)code + '0';` |
|     16 | 5589 | `				}` |
|     23 | 5590 | `			}else{` |
|     49 | 5591 | `				prevcode = 0;` |
|      - | 5592 | `			}` |
|     47 | 5593 | `		}` |
|     33 | 5594 | `		while( j<4 ){` |
|     17 | 5595 | `			zResult[j++] = '0';` |
|      1 | 5596 | `		}` |
|     17 | 5597 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 5598 | `	}else{` |
|      3 | 5599 | `	  ph7_result_string(pCtx,"?000",4);` |
|      - | 5600 | `	}` |
|     19 | 5601 | `	return PH7_OK;` |
|     11 | 5602 |  |
|      - | 5603 | `/*` |
|      - | 5604 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 5605 | ` *  Wraps a string to a given number of characters.` |
|      - | 5606 | ` * Parameters` |
|      - | 5607 | ` *  $str` |
|      - | 5608 | ` *   The input string.` |
|      - | 5609 | ` * $width` |
|      - | 5610 | ` *  The column width.` |
|      - | 5611 | ` * $break` |
|      - | 5612 | ` *  The line is broken using the optional break parameter.` |
|      - | 5613 | ` * Return` |
|      - | 5614 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 5615 | ` */` |
|     14 | 5616 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5617 |  |
|      - | 5618 | `	const char *zIn,*zEnd,*zBreak;` |
|      - | 5619 | `	int iLen,iBreaklen,iChunk;` |
|     15 | 5620 | `	if( nArg < 1 ){` |
|      - | 5621 | `		/* Missing arguments,return the empty string */` |
|      3 | 5622 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5623 | `		return PH7_OK;` |
|      - | 5624 | `	}` |
|      - | 5625 | `	/* Extract the input string */` |
|     13 | 5626 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|     13 | 5627 | `	if( iLen < 1 ){` |
|      - | 5628 | `		/* Nothing to process,return the empty string */` |
|      3 | 5629 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5630 | `		return PH7_OK;` |
|      - | 5631 | `	}` |
|      - | 5632 | `	/* Chunk length */` |
|     11 | 5633 | `	iChunk = 75;` |
|     11 | 5634 | `	iBreaklen = 0;` |
|     11 | 5635 | `	zBreak = ""; /* cc warning */` |
|     11 | 5636 | `	if( nArg > 1 ){` |
|     11 | 5637 | `		iChunk = ph7_value_to_int(apArg[1]);` |
|     11 | 5638 | `		if( iChunk < 1 ){` |
|    ! 0 | 5639 | `			iChunk = 75;` |
|    ! 0 | 5640 | `		}` |
|     11 | 5641 | `		if( nArg > 2 ){` |
|      3 | 5642 | `			zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      1 | 5643 | `		}` |
|      5 | 5644 | `	}` |
|     11 | 5645 | `	if( iBreaklen < 1 ){` |
|      - | 5646 | `		/* Set a default column break */` |
|      - | 5647 | `#ifdef __WINNT__` |
|      1 | 5648 | `		zBreak = "\r\n";` |
|      1 | 5649 | `		iBreaklen = (int)sizeof("\r\n")-1;` |
|      - | 5650 | `#else` |
|      8 | 5651 | `		zBreak = "\n";` |
|      8 | 5652 | `		iBreaklen = (int)sizeof(char);` |
|      - | 5653 | `#endif` |
|      4 | 5654 | `	}` |
|      - | 5655 | `	/* Perform the requested operation */` |
|     11 | 5656 | `	zEnd = &zIn[iLen];` |
|     41 | 5657 | `	for(;;){` |
|      - | 5658 | `		int nMax;` |
|     47 | 5659 | `		if( zIn >= zEnd ){` |
|      - | 5660 | `			/* No more input to process */` |
|     11 | 5661 | `			break;` |
|      - | 5662 | `		}` |
|     37 | 5663 | `		nMax = (int)(zEnd-zIn);` |
|     37 | 5664 | `		if( iChunk > nMax ){` |
|     11 | 5665 | `			iChunk = nMax;` |
|      5 | 5666 | `		}` |
|      - | 5667 | `		/* Append the column first */` |
|     37 | 5668 | `		ph7_result_string(pCtx,zIn,iChunk); /* Will make it's own copy */` |
|      - | 5669 | `		/* Advance the cursor */` |
|     37 | 5670 | `		zIn += iChunk;` |
|     37 | 5671 | `		if( zIn < zEnd ){` |
|      - | 5672 | `			/* Append the line break */` |
|     27 | 5673 | `			ph7_result_string(pCtx,zBreak,iBreaklen);` |
|     13 | 5674 | `		}` |
|      1 | 5675 | `	}` |
|     11 | 5676 | `	return PH7_OK;` |
|      8 | 5677 |  |
|      - | 5678 | `/*` |
|      - | 5679 | ` * Check if the given character is a member of the given mask.` |
|      - | 5680 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 5681 | ` * Refer to [strtok()].` |
|      - | 5682 | ` */` |
|     30 | 5683 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 5684 |  |
|      - | 5685 | `	int i;` |
|     57 | 5686 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 5687 | `		if( c == zMask[i] ){` |
|     13 | 5688 | `			if( pOfft ){` |
|      5 | 5689 | `				*pOfft = i;` |
|      2 | 5690 | `			}` |
|     13 | 5691 | `			return TRUE;` |
|      - | 5692 | `		}` |
|     14 | 5693 | `	}` |
|     19 | 5694 | `	return FALSE;` |
|     16 | 5695 |  |
|      - | 5696 | `/*` |
|      - | 5697 | ` * Extract a single token from the input stream.` |
|      - | 5698 | ` * Refer to [strtok()].` |
|      - | 5699 | ` */` |
|      6 | 5700 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 5701 |  |
|      7 | 5702 | `	const char *zIn = *pzIn;` |
|      - | 5703 | `	const char *zPtr;` |
|      - | 5704 | `	/* Ignore leading delimiter */` |
|     11 | 5705 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5706 | `		zIn++;` |
|      1 | 5707 | `	}` |
|      7 | 5708 | `	if( zIn >= zEnd ){` |
|      - | 5709 | `		/* End of input */` |
|    ! 0 | 5710 | `		return SXERR_EOF;` |
|      - | 5711 | `	}` |
|      7 | 5712 | `	zPtr = zIn;` |
|      - | 5713 | `	/* Extract the token */` |
|     13 | 5714 | `	while( zIn < zEnd ){` |
|     11 | 5715 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 5716 | `			/* UTF-8 stream */` |
|    ! 0 | 5717 | `			zIn++;` |
|    ! 0 | 5718 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 5719 | `		}else{` |
|     11 | 5720 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5721 | `				break;` |
|      - | 5722 | `			}` |
|      7 | 5723 | `			zIn++;` |
|      - | 5724 | `		}` |
|      1 | 5725 | `	}` |
|      7 | 5726 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5727 | `	/* Update the cursor */` |
|      7 | 5728 | `	*pzIn = zIn;` |
|      - | 5729 | `	/* Return to the caller */` |
|      7 | 5730 | `	return SXRET_OK;` |
|      4 | 5731 |  |
|      - | 5732 | `/* strtok auxiliary private data */` |
|      - | 5733 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 5734 | `struct strtok_aux_data` |
|      - | 5735 |  |
|      - | 5736 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 5737 | `	const char *zIn;   /* Current input stream */` |
|      - | 5738 | `	const char *zEnd;  /* End of input */` |
|      - | 5739 | `};` |
|      - | 5740 | `/*` |
|      - | 5741 | ` * string strtok(string $str,string $token)` |
|      - | 5742 | ` * string strtok(string $token)` |
|      - | 5743 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 5744 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 5745 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 5746 | ` *  words by using the space character as the token.` |
|      - | 5747 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 5748 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 5749 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 5750 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 5751 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 5752 | ` *  the argument are found.` |
|      - | 5753 | ` * Parameters` |
|      - | 5754 | ` *  $str` |
|      - | 5755 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 5756 | ` * $token` |
|      - | 5757 | ` *  The delimiter used when splitting up str.` |
|      - | 5758 | ` * Return` |
|      - | 5759 | ` *   Current token or FALSE on EOF.` |
|      - | 5760 | ` */` |
|      8 | 5761 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5762 |  |
|      - | 5763 | `	strtok_aux_data *pAux;` |
|      - | 5764 | `	const char *zMask;` |
|      - | 5765 | `	SyString sToken;` |
|      - | 5766 | `	int nMasklen;` |
|      - | 5767 | `	sxi32 rc;` |
|      9 | 5768 | `	if( nArg < 2 ){` |
|      - | 5769 | `		/* Extract top aux data */` |
|      7 | 5770 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      7 | 5771 | `		if( pAux == 0 ){` |
|      - | 5772 | `			/* No aux data,return FALSE */` |
|    ! 0 | 5773 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5774 | `			return PH7_OK;` |
|      - | 5775 | `		}` |
|      7 | 5776 | `		nMasklen = 0;` |
|      7 | 5777 | `		zMask = ""; /* cc warning */` |
|      7 | 5778 | `		if( nArg > 0 ){` |
|      - | 5779 | `			/* Extract the mask */` |
|      5 | 5780 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 5781 | `		}` |
|      7 | 5782 | `		if( nMasklen < 1 ){` |
|      - | 5783 | `			/* Invalid mask,return FALSE */` |
|      3 | 5784 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|      3 | 5785 | `			ph7_context_free_chunk(pCtx,pAux);` |
|      3 | 5786 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|      3 | 5787 | `			ph7_result_bool(pCtx,0);` |
|      3 | 5788 | `			return PH7_OK;` |
|      - | 5789 | `		}` |
|      - | 5790 | `		/* Extract the token */` |
|      5 | 5791 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 5792 | `		if( rc != SXRET_OK ){` |
|      - | 5793 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 5794 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 5795 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5796 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 5797 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5798 | `		}else{` |
|      - | 5799 | `			/* Return the extracted token */` |
|      5 | 5800 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 5801 | `		}` |
|      3 | 5802 | `	}else{` |
|      - | 5803 | `		const char *zInput,*zCur;` |
|      - | 5804 | `		char *zDup;` |
|      - | 5805 | `		int nLen;` |
|      - | 5806 | `		/* Extract the raw input */` |
|      3 | 5807 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5808 | `		if( nLen < 1 ){` |
|      - | 5809 | `			/* Empty input,return FALSE */` |
|    ! 0 | 5810 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5811 | `			return PH7_OK;` |
|      - | 5812 | `		}` |
|      - | 5813 | `		/* Extract the mask */` |
|      3 | 5814 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 5815 | `		if( nMasklen < 1 ){` |
|      - | 5816 | `			/* Set a default mask */` |
|      - | 5817 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 5818 | `			zMask = TOK_MASK;` |
|    ! 0 | 5819 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 5820 | `#undef TOK_MASK` |
|    ! 0 | 5821 | `		}` |
|      - | 5822 | `		/* Extract a single token */` |
|      3 | 5823 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 5824 | `		if( rc != SXRET_OK ){` |
|      - | 5825 | `			/* Empty input */` |
|    ! 0 | 5826 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5827 | `			return PH7_OK;` |
|    ! 0 | 5828 | `		}else{` |
|      - | 5829 | `			/* Return the extracted token */` |
|      3 | 5830 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 5831 | `		}` |
|      - | 5832 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 5833 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 5834 | `		if( pAux ){` |
|      3 | 5835 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 5836 | `			if( nLen < 1 ){` |
|    ! 0 | 5837 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5838 | `				return PH7_OK;` |
|      - | 5839 | `			}` |
|      - | 5840 | `			/* Duplicate input */` |
|      3 | 5841 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 5842 | `			if( zDup  ){` |
|      3 | 5843 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 5844 | `				/* Register the aux data */` |
|      3 | 5845 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 5846 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 5847 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 5848 | `			}` |
|      1 | 5849 | `		}` |
|      - | 5850 | `	}` |
|      7 | 5851 | `	return PH7_OK;` |
|      5 | 5852 |  |
|      - | 5853 | `/*` |
|      - | 5854 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 5855 | ` *  Pad a string to a certain length with another string` |
|      - | 5856 | ` * Parameters` |
|      - | 5857 | ` *  $input` |
|      - | 5858 | ` *   The input string.` |
|      - | 5859 | ` * $pad_length` |
|      - | 5860 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 5861 | ` *   string, no padding takes place.` |
|      - | 5862 | ` * $pad_string` |
|      - | 5863 | ` *   Note:` |
|      - | 5864 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 5865 | ` *    divided by the pad_string's length.` |
|      - | 5866 | ` * $pad_type` |
|      - | 5867 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 5868 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 5869 | ` * Return` |
|      - | 5870 | ` *  The padded string.` |
|      - | 5871 | ` */` |
|     10 | 5872 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5873 |  |
|      - | 5874 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 5875 | `	const char *zIn,*zPad;` |
|     11 | 5876 | `	if( nArg < 2 ){` |
|      - | 5877 | `		/* Missing arguments,return the empty string */` |
|      5 | 5878 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 5879 | `		return PH7_OK;` |
|      - | 5880 | `	}` |
|      - | 5881 | `	/* Extract the target string */` |
|      7 | 5882 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5883 | `	/* Padding length */` |
|      7 | 5884 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|      7 | 5885 | `	if( iPadlen > 0 ){` |
|      5 | 5886 | `		iPadlen -= iLen;` |
|      2 | 5887 | `	}` |
|      7 | 5888 | `	if( iPadlen < 1  ){` |
|      - | 5889 | `		/* Return the string verbatim */` |
|      3 | 5890 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      3 | 5891 | `		return PH7_OK;` |
|      - | 5892 | `	}` |
|      5 | 5893 | `	zPad = " "; /* Whitespace padding */` |
|      5 | 5894 | `	iStrpad = (int)sizeof(char);` |
|      5 | 5895 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      5 | 5896 | `	if( nArg > 2 ){` |
|      - | 5897 | `		/* Padding string */` |
|      5 | 5898 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      5 | 5899 | `		if( iStrpad < 1 ){` |
|      - | 5900 | `			/* Empty string */` |
|    ! 0 | 5901 | `			zPad = " "; /* Whitespace padding */` |
|    ! 0 | 5902 | `			iStrpad = (int)sizeof(char);` |
|    ! 0 | 5903 | `		}` |
|      5 | 5904 | `		if( nArg > 3 ){` |
|      - | 5905 | `			/* Padd type */` |
|      5 | 5906 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 5907 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 5908 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 5909 | `			}` |
|      2 | 5910 | `		}` |
|      2 | 5911 | `	}` |
|      5 | 5912 | `	iDiv = 1;` |
|      5 | 5913 | `	if( iType == 2 ){` |
|    ! 0 | 5914 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 5915 | `	}` |
|      - | 5916 | `	/* Perform the requested operation */` |
|      5 | 5917 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 5918 | `		jPad = iStrpad;` |
|      5 | 5919 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 5920 | `			/* Padding */` |
|      5 | 5921 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 5922 | `				break;` |
|      - | 5923 | `			}` |
|      3 | 5924 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      2 | 5925 | `		}` |
|      3 | 5926 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 5927 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 5928 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 5929 | `				if( jPad > iStrpad ){` |
|    ! 0 | 5930 | `					jPad = iStrpad;` |
|    ! 0 | 5931 | `				}` |
|      3 | 5932 | `				if( jPad < 1){` |
|    ! 0 | 5933 | `					break;` |
|      - | 5934 | `				}` |
|      3 | 5935 | `				ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 5936 | `			}` |
|      1 | 5937 | `		}` |
|      1 | 5938 | `	}` |
|      5 | 5939 | `	if( iLen > 0 ){` |
|      - | 5940 | `		/* Append the input string */` |
|      5 | 5941 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      2 | 5942 | `	}` |
|      5 | 5943 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 5944 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 5945 | `			/* Padding */` |
|      5 | 5946 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 5947 | `				break;` |
|      - | 5948 | `			}` |
|      3 | 5949 | `			ph7_result_string(pCtx,zPad,iStrpad);` |
|      2 | 5950 | `		}` |
|      5 | 5951 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 5952 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 5953 | `			if( jPad > iStrpad ){` |
|    ! 0 | 5954 | `				jPad = iStrpad;` |
|    ! 0 | 5955 | `			}` |
|      3 | 5956 | `			if( jPad < 1){` |
|    ! 0 | 5957 | `				break;` |
|      - | 5958 | `			}` |
|      3 | 5959 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 5960 | `		}` |
|      1 | 5961 | `	}` |
|      5 | 5962 | `	return PH7_OK;` |
|      6 | 5963 |  |
|      - | 5964 | `/*` |
|      - | 5965 | ` * String replacement private data.` |
|      - | 5966 | ` */` |
|      - | 5967 | `typedef struct str_replace_data str_replace_data;` |
|      - | 5968 | `struct str_replace_data` |
|      - | 5969 |  |
|      - | 5970 | `	/* The following two fields are only used by the strtr function */` |
|      - | 5971 | `	SyBlob *pWorker;         /* Working buffer */` |
|      - | 5972 | `	ProcStringMatch xMatch;  /* Pattern match routine */` |
|      - | 5973 | `	/* The following two fields are only used by the str_replace function */` |
|      - | 5974 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 5975 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 5976 | `};` |
|      - | 5977 | `/*` |
|      - | 5978 | ` * Remove a substring.` |
|      - | 5979 | ` */` |
|      - | 5980 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 5981 | `	for(;;){\` |
|      - | 5982 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 5983 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 5984 | `		++OFFT;\` |
|      - | 5985 | `	}\` |
|      - | 5986 |  |
|      - | 5987 | `/*` |
|      - | 5988 | ` * Shift right and insert algorithm.` |
|      - | 5989 | ` */` |
|      - | 5990 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 5991 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 5992 | `		for(;;){\` |
|      - | 5993 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 5994 | `			if(INLEN < 1 ) { break; }\` |
|      - | 5995 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 5996 | `			--INLEN; \` |
|      - | 5997 | `		}\` |
|      - | 5998 | `		for(;;){\` |
|      - | 5999 | `				if(ELEN < 1) { break; }\` |
|      - | 6000 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 6001 | `				OFFT++;\` |
|      - | 6002 | `				ENTRY++;\` |
|      - | 6003 | `				--ELEN;\` |
|      - | 6004 | `		}\` |
|      - | 6005 |  |
|      - | 6006 | `/*` |
|      - | 6007 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 6008 | ` * replacement string [i.e: zReplace].` |
|      - | 6009 | ` */` |
|     38 | 6010 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 6011 |  |
|     39 | 6012 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 6013 | `	sxu32 n,m;` |
|     39 | 6014 | `	n = SyBlobLength(pWorker);` |
|     39 | 6015 | `	m = nOfft;` |
|      - | 6016 | `	/* Delete the old entry */` |
|    475 | 6017 | `	STRDEL(zInput,n,m,nLen);` |
|     39 | 6018 | `	SyBlobLength(pWorker) -= nLen;` |
|     39 | 6019 | `	if( nReplen > 0 ){` |
|     33 | 6020 | `		sxi32 iRep = nReplen;` |
|      - | 6021 | `		sxi32 rc;` |
|      - | 6022 | `		/*` |
|      - | 6023 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 6024 | `		 * string.` |
|      - | 6025 | `		 */` |
|     33 | 6026 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     33 | 6027 | `		if( rc != SXRET_OK ){` |
|      - | 6028 | `			/* Simply ignore any memory failure problem */` |
|    ! 0 | 6029 | `			return SXRET_OK;` |
|      - | 6030 | `		}` |
|      - | 6031 | `		/* Perform the insertion now */` |
|     33 | 6032 | `		zInput = (char *)SyBlobData(pWorker);` |
|     33 | 6033 | `		n = SyBlobLength(pWorker);` |
|    163 | 6034 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     33 | 6035 | `		SyBlobLength(pWorker) += nReplen;` |
|     16 | 6036 | `	}` |
|     39 | 6037 | `	return SXRET_OK;` |
|     20 | 6038 |  |
|      - | 6039 | `/*` |
|      - | 6040 | ` * String replacement walker callback.` |
|      - | 6041 | ` * The following callback is invoked for each array entry that hold` |
|      - | 6042 | ` * the replace string.` |
|      - | 6043 | ` * Refer to the strtr() implementation for more information.` |
|      - | 6044 | ` */` |
|      8 | 6045 | `static int StringReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6046 |  |
|      9 | 6047 | `	str_replace_data *pRepData = (str_replace_data *)pUserData;` |
|      - | 6048 | `	const char *zTarget,*zReplace;` |
|      - | 6049 | `	SyBlob *pWorker;` |
|      - | 6050 | `	int tLen,nLen;` |
|      - | 6051 | `	sxu32 nOfft;` |
|      - | 6052 | `	sxi32 rc;` |
|      - | 6053 | `	/* Point to the working buffer */` |
|      9 | 6054 | `	pWorker = pRepData->pWorker;` |
|      9 | 6055 | `	if( !ph7_value_is_string(pKey) ){` |
|      - | 6056 | `		/* Target and replace must be a string */` |
|      3 | 6057 | `		return PH7_OK;` |
|      - | 6058 | `	}` |
|      - | 6059 | `	/* Extract the target and the replace */` |
|      7 | 6060 | `	zTarget = ph7_value_to_string(pKey,&tLen);` |
|      7 | 6061 | `	if( tLen < 1 ){` |
|      - | 6062 | `		/* Empty target,return immediately */` |
|    ! 0 | 6063 | `		return PH7_OK;` |
|      - | 6064 | `	}` |
|      - | 6065 | `	/* Perform a pattern search */` |
|      7 | 6066 | `	rc = pRepData->xMatch(SyBlobData(pWorker),SyBlobLength(pWorker),(const void *)zTarget,(sxu32)tLen,&nOfft);` |
|      7 | 6067 | `	if( rc != SXRET_OK ){` |
|      - | 6068 | `		/* Pattern not found */` |
|    ! 0 | 6069 | `		return PH7_OK;` |
|      - | 6070 | `	}` |
|      - | 6071 | `	/* Extract the replace string */` |
|      7 | 6072 | `	zReplace = ph7_value_to_string(pData,&nLen);` |
|      - | 6073 | `	/* Perform the replace process */` |
|      7 | 6074 | `	StringReplace(pWorker,nOfft,tLen,zReplace,nLen);` |
|      - | 6075 | `	/* All done */` |
|      7 | 6076 | `	return PH7_OK;` |
|      5 | 6077 |  |
|      - | 6078 | `/*` |
|      - | 6079 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 6080 | ` * to collect search/replace string.` |
|      - | 6081 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 6082 | ` */` |
|     26 | 6083 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6084 |  |
|     27 | 6085 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 6086 | `	SyString sWorker;` |
|      - | 6087 | `	const char *zIn;` |
|      - | 6088 | `	int nByte;` |
|      - | 6089 | `	/* Extract a string representation of the given argument */` |
|     27 | 6090 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 6091 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 6092 | `	if( nByte > 0 ){` |
|      - | 6093 | `		char *zDup;` |
|      - | 6094 | `		/* Duplicate the chunk */` |
|     25 | 6095 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 6096 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 6097 | `			);` |
|     25 | 6098 | `		if( zDup == 0 ){` |
|      - | 6099 | `			/* Ignore any memory failure problem */` |
|    ! 0 | 6100 | `			ph7_context_throw_error(pRep->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 6101 | `			return PH7_OK;` |
|      - | 6102 | `		}` |
|     25 | 6103 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 6104 | `		/* Save the chunk */` |
|     25 | 6105 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 6106 | `	}` |
|      - | 6107 | `	/* Save for later processing */` |
|     27 | 6108 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 6109 | `	/* All done */` |
|     13 | 6110 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 6111 | `	return PH7_OK;` |
|     14 | 6112 |  |
|      - | 6113 | `/*` |
|      - | 6114 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6115 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6116 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 6117 | ` * Parameters` |
|      - | 6118 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 6119 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 6120 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 6121 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 6122 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 6123 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 6124 | ` * $search` |
|      - | 6125 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 6126 | ` *  to designate multiple needles.` |
|      - | 6127 | ` * $replace` |
|      - | 6128 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 6129 | ` *  to designate multiple replacements.` |
|      - | 6130 | ` * $subject` |
|      - | 6131 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 6132 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 6133 | ` *  of subject, and the return value is an array as well.` |
|      - | 6134 | ` * $count (Not used)` |
|      - | 6135 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 6136 | ` * Return` |
|      - | 6137 | ` * This function returns a string or an array with the replaced values.` |
|      - | 6138 | ` */` |
|  12122 | 6139 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6140 |  |
|      - | 6141 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 6142 | `	ProcStringMatch xMatch;` |
|      - | 6143 | `	const char *zIn,*zFunc;` |
|      - | 6144 | `	str_replace_data sRep;` |
|      - | 6145 | `	SyBlob sWorker;` |
|      - | 6146 | `	SySet sReplace;` |
|      - | 6147 | `	SySet sSearch;` |
|      - | 6148 | `	int rep_str;` |
|      - | 6149 | `	int nByte;` |
|      - | 6150 | `	sxi32 rc;` |
|  12124 | 6151 | `	if( nArg < 3 ){` |
|      - | 6152 | `		/* Missing/Invalid arguments,return null */` |
|      7 | 6153 | `		ph7_result_null(pCtx);` |
|      7 | 6154 | `		return PH7_OK;` |
|      - | 6155 | `	}` |
|      - | 6156 | `	/* Initialize fields */` |
|  12118 | 6157 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  12118 | 6158 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  12118 | 6159 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  12118 | 6160 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  12118 | 6161 | `	sRep.pCtx = pCtx;` |
|  12118 | 6162 | `	sRep.pCollector = &sSearch;` |
|  12118 | 6163 | `	rep_str = 0;` |
|      - | 6164 | `	/* Extract the subject */` |
|  12118 | 6165 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  12118 | 6166 | `	if( nByte < 1 ){` |
|      - | 6167 | `		/* Nothing to replace,return the empty string */` |
|     38 | 6168 | `		ph7_result_string(pCtx,"",0);` |
|     38 | 6169 | `		return PH7_OK;` |
|      - | 6170 | `	}` |
|      - | 6171 | `	/* Copy the subject */` |
|  12082 | 6172 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 6173 | `	/* Search string */` |
|  12082 | 6174 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 6175 | `		/* Collect search string */` |
|      9 | 6176 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 6177 | `	}else{` |
|      - | 6178 | `		/* Single pattern */` |
|  12074 | 6179 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  12074 | 6180 | `		if( nByte < 1 ){` |
|      - | 6181 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 6182 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 6183 | `			return PH7_OK;` |
|      - | 6184 | `		}` |
|  12070 | 6185 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6186 | `		/* Save for later processing */` |
|  12070 | 6187 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 6188 | `	}` |
|      - | 6189 | `	/* Replace string */` |
|  12078 | 6190 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 6191 | `		/* Collect replace string */` |
|      7 | 6192 | `		sRep.pCollector = &sReplace;` |
|      7 | 6193 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 6194 | `	}else{` |
|      - | 6195 | `		/* Single needle */` |
|  12072 | 6196 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  12072 | 6197 | `		rep_str = 1;` |
|  12072 | 6198 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6199 | `		/* Save for later processing */` |
|  12072 | 6200 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 6201 | `	}` |
|      - | 6202 | `	/* Reset loop cursors */` |
|  12078 | 6203 | `	SySetResetCursor(&sSearch);` |
|  12078 | 6204 | `	SySetResetCursor(&sReplace);` |
|  12078 | 6205 | `	pReplace = pSearch = 0; /* cc warning */` |
|  12078 | 6206 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 6207 | `	/* Extract function name */` |
|  12078 | 6208 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 6209 | `	/* Set the default pattern match routine */` |
|  12078 | 6210 | `	xMatch = SyBlobSearch;` |
|  12078 | 6211 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 6212 | `		/* Case insensitive pattern match */` |
|     11 | 6213 | `		xMatch = iPatternMatch;` |
|      5 | 6214 | `	}` |
|      - | 6215 | `	/* Start the replace process */` |
|  24162 | 6216 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 6217 | `		sxu32 nCount,nOfft;` |
|  12086 | 6218 | `		if( pSearch->nByte <  1 ){` |
|      - | 6219 | `			/* Empty string,ignore */` |
|      3 | 6220 | `			continue;` |
|      - | 6221 | `		}` |
|      - | 6222 | `		/* Extract the replace string */` |
|  12084 | 6223 | `		if( rep_str ){` |
|  12074 | 6224 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|   6038 | 6225 | `		}else{` |
|     11 | 6226 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 6227 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 6228 | `				 * An empty string is used for the rest of replacement values` |
|      - | 6229 | `				 */` |
|      3 | 6230 | `				pReplace = 0;` |
|      1 | 6231 | `			}` |
|      - | 6232 | `		}` |
|  12084 | 6233 | `		if( pReplace == 0 ){` |
|      - | 6234 | `			/* Use an empty string instead */` |
|      3 | 6235 | `			pReplace = &sTemp;` |
|      1 | 6236 | `		}` |
|  12084 | 6237 | `		nOfft = nCount = 0;` |
|   6057 | 6238 | `		for(;;){` |
|  12116 | 6239 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 6240 | `				break;` |
|      - | 6241 | `			}` |
|      - | 6242 | `			/* Perform a pattern lookup */` |
|  18155 | 6243 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  12102 | 6244 | `				pSearch->nByte,&nOfft);` |
|  12104 | 6245 | `			if( rc != SXRET_OK ){` |
|      - | 6246 | `				/* Pattern not found */` |
|  12072 | 6247 | `				break;` |
|      - | 6248 | `			}` |
|      - | 6249 | `			/* Perform the replace operation */` |
|     33 | 6250 | `			StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|      - | 6251 | `			/* Increment offset counter */` |
|     33 | 6252 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 6253 | `		}` |
|      2 | 6254 | `	}` |
|      - | 6255 | `	/* All done,clean-up the mess left behind */` |
|  12078 | 6256 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  12078 | 6257 | `	SySetRelease(&sSearch);` |
|  12078 | 6258 | `	SySetRelease(&sReplace);` |
|  12078 | 6259 | `	SyBlobRelease(&sWorker);` |
|  12078 | 6260 | `	return PH7_OK;` |
|   6063 | 6261 |  |
|      - | 6262 | `/*` |
|      - | 6263 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 6264 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 6265 | ` *  Translate characters or replace substrings.` |
|      - | 6266 | ` * Parameters` |
|      - | 6267 | ` *  $str` |
|      - | 6268 | ` *  The string being translated.` |
|      - | 6269 | ` * $from` |
|      - | 6270 | ` *  The string being translated to to.` |
|      - | 6271 | ` * $to` |
|      - | 6272 | ` *  The string replacing from.` |
|      - | 6273 | ` * $replace_pairs` |
|      - | 6274 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 6275 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 6276 | ` * Return` |
|      - | 6277 | ` *  The translated string.` |
|      - | 6278 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 6279 | ` */` |
|     12 | 6280 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6281 |  |
|      - | 6282 | `	const char *zIn;` |
|      - | 6283 | `	int nLen;` |
|     13 | 6284 | `	if( nArg < 1 ){` |
|      - | 6285 | `		/* Nothing to replace,return FALSE */` |
|      7 | 6286 | `		ph7_result_bool(pCtx,0);` |
|      7 | 6287 | `		return PH7_OK;` |
|      - | 6288 | `	}` |
|      7 | 6289 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 6290 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 6291 | `		/* Invalid arguments */` |
|    ! 0 | 6292 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6293 | `		return PH7_OK;` |
|      - | 6294 | `	}` |
|      9 | 6295 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 6296 | `		str_replace_data sRepData;` |
|      - | 6297 | `		SyBlob sWorker;` |
|      - | 6298 | `		/* Initilaize the working buffer */` |
|      5 | 6299 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|      - | 6300 | `		/* Copy raw string */` |
|      5 | 6301 | `		SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nLen);` |
|      - | 6302 | `		/* Init our replace data instance */` |
|      5 | 6303 | `		sRepData.pWorker = &sWorker;` |
|      5 | 6304 | `		sRepData.xMatch = SyBlobSearch;` |
|      - | 6305 | `		/* Iterate throw array entries and perform the replace operation.*/` |
|      5 | 6306 | `		ph7_array_walk(apArg[1],StringReplaceWalker,&sRepData);` |
|      - | 6307 | `		/* All done, return the result string */` |
|      7 | 6308 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|      4 | 6309 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 6310 | `		/* Clean-up */` |
|      5 | 6311 | `		SyBlobRelease(&sWorker);` |
|      3 | 6312 | `	}else{` |
|      - | 6313 | `		int i,flen,tlen,c,iOfft;` |
|      - | 6314 | `		const char *zFrom,*zTo;` |
|      3 | 6315 | `		if( nArg < 3 ){` |
|      - | 6316 | `			/* Nothing to replace */` |
|    ! 0 | 6317 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6318 | `			return PH7_OK;` |
|      - | 6319 | `		}` |
|      - | 6320 | `		/* Extract given arguments */` |
|      3 | 6321 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 6322 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 6323 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 6324 | `			/* Nothing to replace */` |
|    ! 0 | 6325 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6326 | `			return PH7_OK;` |
|      - | 6327 | `		}` |
|      - | 6328 | `		/* Start the replace process */` |
|     13 | 6329 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 6330 | `			c = zIn[i];` |
|     11 | 6331 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 6332 | `				if ( iOfft < tlen ){` |
|      5 | 6333 | `					c = zTo[iOfft];` |
|      2 | 6334 | `				}` |
|      2 | 6335 | `			}` |
|     11 | 6336 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 6337 |  |
|      6 | 6338 | `		}` |
|      - | 6339 | `	}` |
|      7 | 6340 | `	return PH7_OK;` |
|      7 | 6341 |  |
|      - | 6342 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6343 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6344 | `/*` |
|      - | 6345 | ` * Parse an INI string.` |
|      - | 6346 |  |
|      - | 6347 | ` * According to wikipedia` |
|      - | 6348 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 6349 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 6350 | ` *  Format` |
|      - | 6351 | `*    Properties` |
|      - | 6352 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 6353 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 6354 | `*     Example:` |
|      - | 6355 | `*      name=value` |
|      - | 6356 | `*    Sections` |
|      - | 6357 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 6358 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 6359 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 6360 | `*     or the end of the file. Sections may not be nested.` |
|      - | 6361 | `*     Example:` |
|      - | 6362 | `*      [section]` |
|      - | 6363 | `*   Comments` |
|      - | 6364 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 6365 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 6366 | `*/` |
|     10 | 6367 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 6368 |  |
|      - | 6369 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     11 | 6370 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 6371 | `	SyHashEntry *pEntry;` |
|      - | 6372 | `	SyString sEntry;` |
|      - | 6373 | `	SyHash sHash;` |
|      - | 6374 | `	int c;` |
|      - | 6375 | `	/* Create an empty array and worker variables */` |
|     11 | 6376 | `	pArray = ph7_context_new_array(pCtx);` |
|     11 | 6377 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     11 | 6378 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     11 | 6379 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 6380 | `		/* Out of memory */` |
|    ! 0 | 6381 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      - | 6382 | `		/* Return FALSE */` |
|    ! 0 | 6383 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6384 | `		return PH7_OK;` |
|      - | 6385 | `	}` |
|     11 | 6386 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     11 | 6387 | `	pCur = pArray;` |
|      - | 6388 | `	/* Start the parse process */` |
|     20 | 6389 | `	for(;;){` |
|      - | 6390 | `		/* Ignore leading white spaces */` |
|     67 | 6391 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 6392 | `			zIn++;` |
|      1 | 6393 | `		}` |
|     41 | 6394 | `		if( zIn >= zEnd ){` |
|      - | 6395 | `			/* No more input to process */` |
|     11 | 6396 | `			break;` |
|      - | 6397 | `		}` |
|     31 | 6398 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6399 | `			/* Comment til the end of line */` |
|    ! 0 | 6400 | `			zIn++;` |
|    ! 0 | 6401 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 6402 | `				zIn++;` |
|    ! 0 | 6403 | `			}` |
|    ! 0 | 6404 | `			continue;` |
|      - | 6405 | `		}` |
|      - | 6406 | `		/* Reset the string cursor of the working variable */` |
|     31 | 6407 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 6408 | `		if( zIn[0] == '[' ){` |
|      - | 6409 | `			/* Section: Extract the section name */` |
|      9 | 6410 | `			zIn++;` |
|      9 | 6411 | `			zCur = zIn;` |
|     73 | 6412 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 6413 | `				zIn++;` |
|      1 | 6414 | `			}` |
|      9 | 6415 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 6416 | `				/* Save the section name */` |
|      5 | 6417 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 6418 | `				SyStringFullTrim(&sEntry);` |
|      5 | 6419 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 6420 | `				if( sEntry.nByte > 0 ){` |
|      - | 6421 | `					/* Associate an array with the section */` |
|      5 | 6422 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 6423 | `					if( pSection ){` |
|      5 | 6424 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 6425 | `						pCur = pSection;` |
|      2 | 6426 | `					}` |
|      2 | 6427 | `				}` |
|      2 | 6428 | `			}` |
|      9 | 6429 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 6430 | `		}else{` |
|      - | 6431 | `			ph7_value *pOldCur;` |
|      - | 6432 | `			int is_array;` |
|      - | 6433 | `			int iLen;` |
|      - | 6434 | `			/* Properties */` |
|     23 | 6435 | `			is_array = 0;` |
|     23 | 6436 | `			zCur = zIn;` |
|     23 | 6437 | `			iLen = 0; /* cc warning */` |
|     23 | 6438 | `			pOldCur = pCur;` |
|    155 | 6439 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 6440 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 6441 | `					/* Array */` |
|    ! 0 | 6442 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 6443 | `					is_array = 1;` |
|    ! 0 | 6444 | `					if( iLen > 0 ){` |
|    ! 0 | 6445 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 6446 | `						/* Query the hashtable */` |
|    ! 0 | 6447 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 6448 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 6449 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 6450 | `						if( pEntry ){` |
|    ! 0 | 6451 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 6452 | `						}else{` |
|      - | 6453 | `							/* Create an empty array */` |
|    ! 0 | 6454 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 6455 | `							if( pvArr ){` |
|      - | 6456 | `								/* Save the entry */` |
|    ! 0 | 6457 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 6458 | `								/* Insert the entry */` |
|    ! 0 | 6459 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6460 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 6461 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 6462 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6463 | `							}` |
|      - | 6464 | `						}` |
|    ! 0 | 6465 | `						if( pvArr ){` |
|    ! 0 | 6466 | `							pCur = pvArr;` |
|    ! 0 | 6467 | `						}` |
|    ! 0 | 6468 | `					}` |
|    ! 0 | 6469 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 6470 | `						zIn++;` |
|    ! 0 | 6471 | `					}` |
|    ! 0 | 6472 | `				}` |
|    133 | 6473 | `				zIn++;` |
|      1 | 6474 | `			}` |
|     23 | 6475 | `			if( !is_array ){` |
|     23 | 6476 | `				iLen = (int)(zIn-zCur);` |
|     11 | 6477 | `			}` |
|      - | 6478 | `			/* Trim the key */` |
|     23 | 6479 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 6480 | `			SyStringFullTrim(&sEntry);` |
|     23 | 6481 | `			if( sEntry.nByte > 0 ){` |
|     23 | 6482 | `				if( !is_array ){` |
|      - | 6483 | `					/* Save the key name */` |
|     23 | 6484 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 6485 | `				}` |
|      - | 6486 | `				/* extract key value */` |
|     23 | 6487 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 6488 | `				zIn++; /* '=' */` |
|     39 | 6489 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 6490 | `					zIn++;` |
|      1 | 6491 | `				}` |
|     23 | 6492 | `				if( zIn < zEnd ){` |
|     21 | 6493 | `					zCur = zIn;` |
|     21 | 6494 | `					c = zIn[0];` |
|     21 | 6495 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6496 | `						zIn++;` |
|      - | 6497 | `						/* Delimit the value */` |
|    ! 0 | 6498 | `						while( zIn < zEnd ){` |
|    ! 0 | 6499 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 6500 | `								break;` |
|      - | 6501 | `							}` |
|    ! 0 | 6502 | `							zIn++;` |
|    ! 0 | 6503 | `						}` |
|    ! 0 | 6504 | `						if( zIn < zEnd ){` |
|    ! 0 | 6505 | `							zIn++;` |
|    ! 0 | 6506 | `						}` |
|    ! 0 | 6507 | `					}else{` |
|    125 | 6508 | `						while( zIn < zEnd ){` |
|    123 | 6509 | `							if( zIn[0] == '\n' ){` |
|     19 | 6510 | `								if( zIn[-1] != '\\' ){` |
|     19 | 6511 | `									break;` |
|    ! 0 | 6512 | `								}` |
|    105 | 6513 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6514 | `								/* Inline comments */` |
|    ! 0 | 6515 | `								break;` |
|      - | 6516 | `							}` |
|    105 | 6517 | `							zIn++;` |
|      1 | 6518 | `						}` |
|      - | 6519 | `					}` |
|      - | 6520 | `					/* Trim the value */` |
|     21 | 6521 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 6522 | `					SyStringFullTrim(&sEntry);` |
|     21 | 6523 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6524 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 6525 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 6526 | `					}` |
|     21 | 6527 | `					if( sEntry.nByte > 0 ){` |
|     21 | 6528 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 6529 | `					}` |
|      - | 6530 | `					/* Insert the key and it's value */` |
|     21 | 6531 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 6532 | `				}` |
|     12 | 6533 | `			}else{` |
|    ! 0 | 6534 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 6535 | `					zIn++;` |
|    ! 0 | 6536 | `				}` |
|      - | 6537 | `			}` |
|     23 | 6538 | `			pCur = pOldCur;` |
|      - | 6539 | `		}` |
|      1 | 6540 | `	}` |
|     11 | 6541 | `	SyHashRelease(&sHash);` |
|      - | 6542 | `	/* Return the parse of the INI string */` |
|     11 | 6543 | `	ph7_result_value(pCtx,pArray);` |
|     11 | 6544 | `	return SXRET_OK;` |
|      6 | 6545 |  |
|      - | 6546 | `/*` |
|      - | 6547 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 6548 | ` *  Parse a configuration string.` |
|      - | 6549 | ` * Parameters` |
|      - | 6550 | ` *  $ini` |
|      - | 6551 | ` *   The contents of the ini file being parsed.` |
|      - | 6552 | ` *  $process_sections` |
|      - | 6553 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 6554 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 6555 | ` *  $scanner_mode (Not used)` |
|      - | 6556 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 6557 | ` *   then option values will not be parsed.` |
|      - | 6558 | ` * Return` |
|      - | 6559 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 6560 | ` */` |
|     10 | 6561 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6562 |  |
|      - | 6563 | `	const char *zIni;` |
|      - | 6564 | `	int nByte;` |
|     11 | 6565 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6566 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      3 | 6567 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6568 | `		return PH7_OK;` |
|      - | 6569 | `	}` |
|      - | 6570 | `	/* Extract the raw INI buffer */` |
|      9 | 6571 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 6572 | `	/* Process the INI buffer*/` |
|      9 | 6573 | `	PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      9 | 6574 | `	return PH7_OK;` |
|      6 | 6575 |  |
|      - | 6576 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6577 |  |
|      - | 6578 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6579 |  |
|      - | 6580 | `/*` |
|      - | 6581 | ` * Ctype Functions.` |
|      - | 6582 | ` * Status:` |
|      - | 6583 | ` *    Stable.` |
|      - | 6584 | ` */` |
|      - | 6585 | `/*` |
|      - | 6586 | ` * bool ctype_alnum(string $text)` |
|      - | 6587 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 6588 | ` * Parameters` |
|      - | 6589 | ` *  $text` |
|      - | 6590 | ` *   The tested string.` |
|      - | 6591 | ` * Return` |
|      - | 6592 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 6593 | ` */` |
|     16 | 6594 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6595 |  |
|      - | 6596 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6597 | `	int nLen;` |
|     17 | 6598 | `	if( nArg < 1 ){` |
|      - | 6599 | `		/* Missing arguments,return FALSE */` |
|      3 | 6600 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6601 | `		return PH7_OK;` |
|      - | 6602 | `	}` |
|      - | 6603 | `	/* Extract the target string */` |
|     15 | 6604 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 6605 | `	zEnd = &zIn[nLen];` |
|     15 | 6606 | `	if( nLen < 1 ){` |
|      - | 6607 | `		/* Empty string,return FALSE */` |
|      3 | 6608 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6609 | `		return PH7_OK;` |
|      - | 6610 | `	}` |
|      - | 6611 | `	/* Perform the requested operation */` |
|     32 | 6612 | `	for(;;){` |
|     65 | 6613 | `		if( zIn >= zEnd ){` |
|      - | 6614 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6615 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6616 | `			return PH7_OK;` |
|      - | 6617 | `		}` |
|     57 | 6618 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 6619 | `			break;` |
|      - | 6620 | `		}` |
|      - | 6621 | `		/* Point to the next character */` |
|     53 | 6622 | `		zIn++;` |
|      1 | 6623 | `	}` |
|      - | 6624 | `	/* The test failed,return FALSE */` |
|      5 | 6625 | `	ph7_result_bool(pCtx,0);` |
|      5 | 6626 | `	return PH7_OK;` |
|      9 | 6627 |  |
|      - | 6628 | `/*` |
|      - | 6629 | ` * bool ctype_alpha(string $text)` |
|      - | 6630 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 6631 | ` * Parameters` |
|      - | 6632 | ` *  $text` |
|      - | 6633 | ` *   The tested string.` |
|      - | 6634 | ` * Return` |
|      - | 6635 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 6636 | ` */` |
|     18 | 6637 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6638 |  |
|      - | 6639 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6640 | `	int nLen;` |
|     19 | 6641 | `	if( nArg < 1 ){` |
|      - | 6642 | `		/* Missing arguments,return FALSE */` |
|      3 | 6643 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6644 | `		return PH7_OK;` |
|      - | 6645 | `	}` |
|      - | 6646 | `	/* Extract the target string */` |
|     17 | 6647 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6648 | `	zEnd = &zIn[nLen];` |
|     17 | 6649 | `	if( nLen < 1 ){` |
|      - | 6650 | `		/* Empty string,return FALSE */` |
|      3 | 6651 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6652 | `		return PH7_OK;` |
|      - | 6653 | `	}` |
|      - | 6654 | `	/* Perform the requested operation */` |
|     42 | 6655 | `	for(;;){` |
|     85 | 6656 | `		if( zIn >= zEnd ){` |
|      - | 6657 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6658 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6659 | `			return PH7_OK;` |
|      - | 6660 | `		}` |
|     77 | 6661 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 6662 | `			break;` |
|      - | 6663 | `		}` |
|      - | 6664 | `		/* Point to the next character */` |
|     71 | 6665 | `		zIn++;` |
|      1 | 6666 | `	}` |
|      - | 6667 | `	/* The test failed,return FALSE */` |
|      7 | 6668 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6669 | `	return PH7_OK;` |
|     10 | 6670 |  |
|      - | 6671 | `/*` |
|      - | 6672 | ` * bool ctype_cntrl(string $text)` |
|      - | 6673 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 6674 | ` * Parameters` |
|      - | 6675 | ` *  $text` |
|      - | 6676 | ` *   The tested string.` |
|      - | 6677 | ` * Return` |
|      - | 6678 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 6679 | ` */` |
|     18 | 6680 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6681 |  |
|      - | 6682 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6683 | `	int nLen;` |
|     19 | 6684 | `	if( nArg < 1 ){` |
|      - | 6685 | `		/* Missing arguments,return FALSE */` |
|      3 | 6686 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6687 | `		return PH7_OK;` |
|      - | 6688 | `	}` |
|      - | 6689 | `	/* Extract the target string */` |
|     17 | 6690 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6691 | `	zEnd = &zIn[nLen];` |
|     17 | 6692 | `	if( nLen < 1 ){` |
|      - | 6693 | `		/* Empty string,return FALSE */` |
|      3 | 6694 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6695 | `		return PH7_OK;` |
|      - | 6696 | `	}` |
|      - | 6697 | `	/* Perform the requested operation */` |
|     14 | 6698 | `	for(;;){` |
|     29 | 6699 | `		if( zIn >= zEnd ){` |
|      - | 6700 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6701 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6702 | `			return PH7_OK;` |
|      - | 6703 | `		}` |
|     21 | 6704 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6705 | `			/* UTF-8 stream  */` |
|    ! 0 | 6706 | `			break;` |
|      - | 6707 | `		}` |
|     21 | 6708 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 6709 | `			break;` |
|      - | 6710 | `		}` |
|      - | 6711 | `		/* Point to the next character */` |
|     15 | 6712 | `		zIn++;` |
|      1 | 6713 | `	}` |
|      - | 6714 | `	/* The test failed,return FALSE */` |
|      7 | 6715 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6716 | `	return PH7_OK;` |
|     10 | 6717 |  |
|      - | 6718 | `/*` |
|      - | 6719 | ` * bool ctype_digit(string $text)` |
|      - | 6720 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 6721 | ` * Parameters` |
|      - | 6722 | ` *  $text` |
|      - | 6723 | ` *   The tested string.` |
|      - | 6724 | ` * Return` |
|      - | 6725 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 6726 | ` */` |
|   1482 | 6727 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6728 |  |
|      - | 6729 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6730 | `	int nLen;` |
|   1484 | 6731 | `	if( nArg < 1 ){` |
|      - | 6732 | `		/* Missing arguments,return FALSE */` |
|      3 | 6733 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6734 | `		return PH7_OK;` |
|      - | 6735 | `	}` |
|      - | 6736 | `	/* Extract the target string */` |
|   1482 | 6737 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1482 | 6738 | `	zEnd = &zIn[nLen];` |
|   1482 | 6739 | `	if( nLen < 1 ){` |
|      - | 6740 | `		/* Empty string,return FALSE */` |
|      3 | 6741 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6742 | `		return PH7_OK;` |
|      - | 6743 | `	}` |
|      - | 6744 | `	/* Perform the requested operation */` |
|   1389 | 6745 | `	for(;;){` |
|   2780 | 6746 | `		if( zIn >= zEnd ){` |
|      - | 6747 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1270 | 6748 | `			ph7_result_bool(pCtx,1);` |
|   1270 | 6749 | `			return PH7_OK;` |
|      - | 6750 | `		}` |
|   1512 | 6751 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6752 | `			/* UTF-8 stream  */` |
|    ! 0 | 6753 | `			break;` |
|      - | 6754 | `		}` |
|   1512 | 6755 | `		if( !SyisDigit(zIn[0]) ){` |
|    212 | 6756 | `			break;` |
|      - | 6757 | `		}` |
|      - | 6758 | `		/* Point to the next character */` |
|   1302 | 6759 | `		zIn++;` |
|      2 | 6760 | `	}` |
|      - | 6761 | `	/* The test failed,return FALSE */` |
|    212 | 6762 | `	ph7_result_bool(pCtx,0);` |
|    212 | 6763 | `	return PH7_OK;` |
|    743 | 6764 |  |
|      - | 6765 | `/*` |
|      - | 6766 | ` * bool ctype_xdigit(string $text)` |
|      - | 6767 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 6768 | ` * Parameters` |
|      - | 6769 | ` *  $text` |
|      - | 6770 | ` *   The tested string.` |
|      - | 6771 | ` * Return` |
|      - | 6772 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 6773 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 6774 | ` */` |
|     20 | 6775 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6776 |  |
|      - | 6777 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6778 | `	int nLen;` |
|     21 | 6779 | `	if( nArg < 1 ){` |
|      - | 6780 | `		/* Missing arguments,return FALSE */` |
|      3 | 6781 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6782 | `		return PH7_OK;` |
|      - | 6783 | `	}` |
|      - | 6784 | `	/* Extract the target string */` |
|     19 | 6785 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 6786 | `	zEnd = &zIn[nLen];` |
|     19 | 6787 | `	if( nLen < 1 ){` |
|      - | 6788 | `		/* Empty string,return FALSE */` |
|      3 | 6789 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6790 | `		return PH7_OK;` |
|      - | 6791 | `	}` |
|      - | 6792 | `	/* Perform the requested operation */` |
|     46 | 6793 | `	for(;;){` |
|     93 | 6794 | `		if( zIn >= zEnd ){` |
|      - | 6795 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 6796 | `			ph7_result_bool(pCtx,1);` |
|     11 | 6797 | `			return PH7_OK;` |
|      - | 6798 | `		}` |
|     83 | 6799 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6800 | `			/* UTF-8 stream  */` |
|    ! 0 | 6801 | `			break;` |
|      - | 6802 | `		}` |
|     83 | 6803 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 6804 | `			break;` |
|      - | 6805 | `		}` |
|      - | 6806 | `		/* Point to the next character */` |
|     77 | 6807 | `		zIn++;` |
|      1 | 6808 | `	}` |
|      - | 6809 | `	/* The test failed,return FALSE */` |
|      7 | 6810 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6811 | `	return PH7_OK;` |
|     11 | 6812 |  |
|      - | 6813 | `/*` |
|      - | 6814 | ` * bool ctype_graph(string $text)` |
|      - | 6815 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 6816 | ` * Parameters` |
|      - | 6817 | ` *  $text` |
|      - | 6818 | ` *   The tested string.` |
|      - | 6819 | ` * Return` |
|      - | 6820 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 6821 | ` * (no white space), FALSE otherwise.` |
|      - | 6822 | ` */` |
|     18 | 6823 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6824 |  |
|      - | 6825 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6826 | `	int nLen;` |
|     19 | 6827 | `	if( nArg < 1 ){` |
|      - | 6828 | `		/* Missing arguments,return FALSE */` |
|      3 | 6829 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6830 | `		return PH7_OK;` |
|      - | 6831 | `	}` |
|      - | 6832 | `	/* Extract the target string */` |
|     17 | 6833 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6834 | `	zEnd = &zIn[nLen];` |
|     17 | 6835 | `	if( nLen < 1 ){` |
|      - | 6836 | `		/* Empty string,return FALSE */` |
|      3 | 6837 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6838 | `		return PH7_OK;` |
|      - | 6839 | `	}` |
|      - | 6840 | `	/* Perform the requested operation */` |
|     57 | 6841 | `	for(;;){` |
|    115 | 6842 | `		if( zIn >= zEnd ){` |
|      - | 6843 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6844 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6845 | `			return PH7_OK;` |
|      - | 6846 | `		}` |
|    107 | 6847 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6848 | `			/* UTF-8 stream  */` |
|    ! 0 | 6849 | `			break;` |
|      - | 6850 | `		}` |
|    107 | 6851 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 6852 | `			break;` |
|      - | 6853 | `		}` |
|      - | 6854 | `		/* Point to the next character */` |
|    101 | 6855 | `		zIn++;` |
|      1 | 6856 | `	}` |
|      - | 6857 | `	/* The test failed,return FALSE */` |
|      7 | 6858 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6859 | `	return PH7_OK;` |
|     10 | 6860 |  |
|      - | 6861 | `/*` |
|      - | 6862 | ` * bool ctype_print(string $text)` |
|      - | 6863 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 6864 | ` * Parameters` |
|      - | 6865 | ` *  $text` |
|      - | 6866 | ` *   The tested string.` |
|      - | 6867 | ` * Return` |
|      - | 6868 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 6869 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 6870 | ` *  or control function at all.` |
|      - | 6871 | ` */` |
|     18 | 6872 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6873 |  |
|      - | 6874 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6875 | `	int nLen;` |
|     19 | 6876 | `	if( nArg < 1 ){` |
|      - | 6877 | `		/* Missing arguments,return FALSE */` |
|      3 | 6878 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6879 | `		return PH7_OK;` |
|      - | 6880 | `	}` |
|      - | 6881 | `	/* Extract the target string */` |
|     17 | 6882 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6883 | `	zEnd = &zIn[nLen];` |
|     17 | 6884 | `	if( nLen < 1 ){` |
|      - | 6885 | `		/* Empty string,return FALSE */` |
|      3 | 6886 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6887 | `		return PH7_OK;` |
|      - | 6888 | `	}` |
|      - | 6889 | `	/* Perform the requested operation */` |
|     63 | 6890 | `	for(;;){` |
|    127 | 6891 | `		if( zIn >= zEnd ){` |
|      - | 6892 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6893 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6894 | `			return PH7_OK;` |
|      - | 6895 | `		}` |
|    119 | 6896 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6897 | `			/* UTF-8 stream  */` |
|    ! 0 | 6898 | `			break;` |
|      - | 6899 | `		}` |
|    119 | 6900 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 6901 | `			break;` |
|      - | 6902 | `		}` |
|      - | 6903 | `		/* Point to the next character */` |
|    113 | 6904 | `		zIn++;` |
|      1 | 6905 | `	}` |
|      - | 6906 | `	/* The test failed,return FALSE */` |
|      7 | 6907 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6908 | `	return PH7_OK;` |
|     10 | 6909 |  |
|      - | 6910 | `/*` |
|      - | 6911 | ` * bool ctype_punct(string $text)` |
|      - | 6912 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 6913 | ` * Parameters` |
|      - | 6914 | ` *  $text` |
|      - | 6915 | ` *   The tested string.` |
|      - | 6916 | ` * Return` |
|      - | 6917 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 6918 | ` *  digit or blank, FALSE otherwise.` |
|      - | 6919 | ` */` |
|     20 | 6920 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6921 |  |
|      - | 6922 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6923 | `	int nLen;` |
|     21 | 6924 | `	if( nArg < 1 ){` |
|      - | 6925 | `		/* Missing arguments,return FALSE */` |
|      3 | 6926 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6927 | `		return PH7_OK;` |
|      - | 6928 | `	}` |
|      - | 6929 | `	/* Extract the target string */` |
|     19 | 6930 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 6931 | `	zEnd = &zIn[nLen];` |
|     19 | 6932 | `	if( nLen < 1 ){` |
|      - | 6933 | `		/* Empty string,return FALSE */` |
|      3 | 6934 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6935 | `		return PH7_OK;` |
|      - | 6936 | `	}` |
|      - | 6937 | `	/* Perform the requested operation */` |
|     38 | 6938 | `	for(;;){` |
|     77 | 6939 | `		if( zIn >= zEnd ){` |
|      - | 6940 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6941 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6942 | `			return PH7_OK;` |
|      - | 6943 | `		}` |
|     69 | 6944 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6945 | `			/* UTF-8 stream  */` |
|    ! 0 | 6946 | `			break;` |
|      - | 6947 | `		}` |
|     69 | 6948 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 6949 | `			break;` |
|      - | 6950 | `		}` |
|      - | 6951 | `		/* Point to the next character */` |
|     61 | 6952 | `		zIn++;` |
|      1 | 6953 | `	}` |
|      - | 6954 | `	/* The test failed,return FALSE */` |
|      9 | 6955 | `	ph7_result_bool(pCtx,0);` |
|      9 | 6956 | `	return PH7_OK;` |
|     11 | 6957 |  |
|      - | 6958 | `/*` |
|      - | 6959 | ` * bool ctype_space(string $text)` |
|      - | 6960 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 6961 | ` * Parameters` |
|      - | 6962 | ` *  $text` |
|      - | 6963 | ` *   The tested string.` |
|      - | 6964 | ` * Return` |
|      - | 6965 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 6966 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 6967 | ` *  and form feed characters.` |
|      - | 6968 | ` */` |
|  35520 | 6969 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6970 |  |
|      - | 6971 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6972 | `	int nLen;` |
|  35522 | 6973 | `	if( nArg < 1 ){` |
|      - | 6974 | `		/* Missing arguments,return FALSE */` |
|      3 | 6975 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6976 | `		return PH7_OK;` |
|      - | 6977 | `	}` |
|      - | 6978 | `	/* Extract the target string */` |
|  35520 | 6979 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  35520 | 6980 | `	zEnd = &zIn[nLen];` |
|  35520 | 6981 | `	if( nLen < 1 ){` |
|      - | 6982 | `		/* Empty string,return FALSE */` |
|      3 | 6983 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6984 | `		return PH7_OK;` |
|      - | 6985 | `	}` |
|      - | 6986 | `	/* Perform the requested operation */` |
|  18075 | 6987 | `	for(;;){` |
|  36108 | 6988 | `		if( zIn >= zEnd ){` |
|      - | 6989 | `			/* If we reach the end of the string,then the test succeeded. */` |
|    568 | 6990 | `			ph7_result_bool(pCtx,1);` |
|    568 | 6991 | `			return PH7_OK;` |
|      - | 6992 | `		}` |
|  35542 | 6993 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6994 | `			/* UTF-8 stream  */` |
|    ! 0 | 6995 | `			break;` |
|      - | 6996 | `		}` |
|  35542 | 6997 | `		if( !SyisSpace(zIn[0]) ){` |
|  34952 | 6998 | `			break;` |
|      - | 6999 | `		}` |
|      - | 7000 | `		/* Point to the next character */` |
|    592 | 7001 | `		zIn++;` |
|      2 | 7002 | `	}` |
|      - | 7003 | `	/* The test failed,return FALSE */` |
|  34952 | 7004 | `	ph7_result_bool(pCtx,0);` |
|  34952 | 7005 | `	return PH7_OK;` |
|  17784 | 7006 |  |
|      - | 7007 | `/*` |
|      - | 7008 | ` * bool ctype_lower(string $text)` |
|      - | 7009 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 7010 | ` * Parameters` |
|      - | 7011 | ` *  $text` |
|      - | 7012 | ` *   The tested string.` |
|      - | 7013 | ` * Return` |
|      - | 7014 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 7015 | ` */` |
|     18 | 7016 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7017 |  |
|      - | 7018 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7019 | `	int nLen;` |
|     19 | 7020 | `	if( nArg < 1 ){` |
|      - | 7021 | `		/* Missing arguments,return FALSE */` |
|      3 | 7022 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7023 | `		return PH7_OK;` |
|      - | 7024 | `	}` |
|      - | 7025 | `	/* Extract the target string */` |
|     17 | 7026 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7027 | `	zEnd = &zIn[nLen];` |
|     17 | 7028 | `	if( nLen < 1 ){` |
|      - | 7029 | `		/* Empty string,return FALSE */` |
|      3 | 7030 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7031 | `		return PH7_OK;` |
|      - | 7032 | `	}` |
|      - | 7033 | `	/* Perform the requested operation */` |
|     27 | 7034 | `	for(;;){` |
|     55 | 7035 | `		if( zIn >= zEnd ){` |
|      - | 7036 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7037 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7038 | `			return PH7_OK;` |
|      - | 7039 | `		}` |
|     51 | 7040 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 7041 | `			break;` |
|      - | 7042 | `		}` |
|      - | 7043 | `		/* Point to the next character */` |
|     41 | 7044 | `		zIn++;` |
|      1 | 7045 | `	}` |
|      - | 7046 | `	/* The test failed,return FALSE */` |
|     11 | 7047 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7048 | `	return PH7_OK;` |
|     10 | 7049 |  |
|      - | 7050 | `/*` |
|      - | 7051 | ` * bool ctype_upper(string $text)` |
|      - | 7052 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 7053 | ` * Parameters` |
|      - | 7054 | ` *  $text` |
|      - | 7055 | ` *   The tested string.` |
|      - | 7056 | ` * Return` |
|      - | 7057 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 7058 | ` */` |
|     18 | 7059 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7060 |  |
|      - | 7061 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7062 | `	int nLen;` |
|     19 | 7063 | `	if( nArg < 1 ){` |
|      - | 7064 | `		/* Missing arguments,return FALSE */` |
|      3 | 7065 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7066 | `		return PH7_OK;` |
|      - | 7067 | `	}` |
|      - | 7068 | `	/* Extract the target string */` |
|     17 | 7069 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7070 | `	zEnd = &zIn[nLen];` |
|     17 | 7071 | `	if( nLen < 1 ){` |
|      - | 7072 | `		/* Empty string,return FALSE */` |
|      3 | 7073 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7074 | `		return PH7_OK;` |
|      - | 7075 | `	}` |
|      - | 7076 | `	/* Perform the requested operation */` |
|     28 | 7077 | `	for(;;){` |
|     57 | 7078 | `		if( zIn >= zEnd ){` |
|      - | 7079 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7080 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7081 | `			return PH7_OK;` |
|      - | 7082 | `		}` |
|     53 | 7083 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 7084 | `			break;` |
|      - | 7085 | `		}` |
|      - | 7086 | `		/* Point to the next character */` |
|     43 | 7087 | `		zIn++;` |
|      1 | 7088 | `	}` |
|      - | 7089 | `	/* The test failed,return FALSE */` |
|     11 | 7090 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7091 | `	return PH7_OK;` |
|     10 | 7092 |  |
|      - | 7093 | `/*` |
|      - | 7094 | ` * Date/Time functions` |
|      - | 7095 | ` * Status:` |
|      - | 7096 | ` *    Devel.` |
|      - | 7097 | ` */` |
|      - | 7098 | `#include <time.h>` |
|      - | 7099 | `#ifdef __WINNT__` |
|      - | 7100 | `/* GetSystemTime() */` |
|      - | 7101 | `#include <Windows.h>` |
|      - | 7102 | `#ifdef _WIN32_WCE` |
|      - | 7103 | `/*` |
|      - | 7104 | `** WindowsCE does not have a localtime() function.  So create a` |
|      - | 7105 | `** substitute.` |
|      - | 7106 | `** Taken from the SQLite3 source tree.` |
|      - | 7107 | `** Status: Public domain` |
|      - | 7108 | `*/` |
|      - | 7109 | `struct tm *__cdecl localtime(const time_t *t)` |
|      - | 7110 |  |
|      - | 7111 | `  static struct tm y;` |
|      - | 7112 | `  FILETIME uTm, lTm;` |
|      - | 7113 | `  SYSTEMTIME pTm;` |
|      - | 7114 | `  ph7_int64 t64;` |
|      - | 7115 | `  t64 = *t;` |
|      - | 7116 | `  t64 = (t64 + 11644473600)*10000000;` |
|      - | 7117 | `  uTm.dwLowDateTime = (DWORD)(t64 & 0xFFFFFFFF);` |
|      - | 7118 | `  uTm.dwHighDateTime= (DWORD)(t64 >> 32);` |
|      - | 7119 | `  FileTimeToLocalFileTime(&uTm,&lTm);` |
|      - | 7120 | `  FileTimeToSystemTime(&lTm,&pTm);` |
|      - | 7121 | `  y.tm_year = pTm.wYear - 1900;` |
|      - | 7122 | `  y.tm_mon = pTm.wMonth - 1;` |
|      - | 7123 | `  y.tm_wday = pTm.wDayOfWeek;` |
|      - | 7124 | `  y.tm_mday = pTm.wDay;` |
|      - | 7125 | `  y.tm_hour = pTm.wHour;` |
|      - | 7126 | `  y.tm_min = pTm.wMinute;` |
|      - | 7127 | `  y.tm_sec = pTm.wSecond;` |
|      - | 7128 | `  return &y;` |
|      - | 7129 |  |
|      - | 7130 | `#endif /*_WIN32_WCE */` |
|      - | 7131 | `#elif defined(__UNIXES__)` |
|      - | 7132 | `#include <sys/time.h>` |
|      - | 7133 | `#endif /* __WINNT__*/` |
|      - | 7134 | ` /*` |
|      - | 7135 | `  * int64 time(void)` |
|      - | 7136 | `  *  Current Unix timestamp` |
|      - | 7137 | `  * Parameters` |
|      - | 7138 | `  *  None.` |
|      - | 7139 | `  * Return` |
|      - | 7140 | `  *  Returns the current time measured in the number of seconds` |
|      - | 7141 | `  *  since the Unix Epoch (January 1 1970 00:00:00 GMT).` |
|      - | 7142 | `  */` |
|      8 | 7143 | `static int PH7_builtin_time(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7144 |  |
|      - | 7145 | `	time_t tt;` |
|      4 | 7146 | `	SXUNUSED(nArg); /* cc warning */` |
|      4 | 7147 | `	SXUNUSED(apArg);` |
|      - | 7148 | `	/* Extract the current time */` |
|      9 | 7149 | `	time(&tt);` |
|      - | 7150 | `	/* Return as 64-bit integer */` |
|      9 | 7151 | `	ph7_result_int64(pCtx,(ph7_int64)tt);` |
|      9 | 7152 | `	return  PH7_OK;` |
|      1 | 7153 |  |
|      - | 7154 | `/*` |
|      - | 7155 | `  * string/float microtime([ bool $get_as_float = false ])` |
|      - | 7156 | `  *  microtime() returns the current Unix timestamp with microseconds.` |
|      - | 7157 | `  * Parameters` |
|      - | 7158 | `  *  $get_as_float` |
|      - | 7159 | `  *   If used and set to TRUE, microtime() will return a float instead of a string` |
|      - | 7160 | `  *   as described in the return values section below.` |
|      - | 7161 | `  * Return` |
|      - | 7162 | `  *  By default, microtime() returns a string in the form "msec sec", where sec` |
|      - | 7163 | `  *  is the current time measured in the number of seconds since the Unix` |
|      - | 7164 | `  *  epoch (0:00:00 January 1, 1970 GMT), and msec is the number of microseconds` |
|      - | 7165 | `  *  that have elapsed since sec expressed in seconds.` |
|      - | 7166 | `  *  If get_as_float is set to TRUE, then microtime() returns a float, which represents` |
|      - | 7167 | `  *  the current time in seconds since the Unix epoch accurate to the nearest microsecond.` |
|      - | 7168 | `  */` |
|     20 | 7169 | `static int PH7_builtin_microtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7170 |  |
|     21 | 7171 | `	int bFloat = 0;` |
|      - | 7172 | `	sytime sTime;` |
|      - | 7173 | `#if defined(__UNIXES__)` |
|      - | 7174 | `	struct timeval tv;` |
|     20 | 7175 | `	gettimeofday(&tv,0);` |
|     20 | 7176 | `	sTime.tm_sec  = (long)tv.tv_sec;` |
|     20 | 7177 | `	sTime.tm_usec = (long)tv.tv_usec;` |
|      - | 7178 | `#else` |
|      - | 7179 | `	time_t tt;` |
|      1 | 7180 | `	time(&tt);` |
|      1 | 7181 | `	sTime.tm_sec  = (long)tt;` |
|      1 | 7182 | `	sTime.tm_usec = (long)(tt%SX_USEC_PER_SEC);` |
|      - | 7183 | `#endif /* __UNIXES__ */` |
|     21 | 7184 | `	if( nArg > 0 ){` |
|     17 | 7185 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|      8 | 7186 | `	}` |
|     21 | 7187 | `	if( bFloat ){` |
|      - | 7188 | `		/* Return as float */` |
|     17 | 7189 | `		ph7_result_double(pCtx,(double)sTime.tm_sec);` |
|      9 | 7190 | `	}else{` |
|      - | 7191 | `		/* Return as string */` |
|      5 | 7192 | `		ph7_result_string_format(pCtx,"%ld %ld",sTime.tm_usec,sTime.tm_sec);` |
|      - | 7193 | `	}` |
|     21 | 7194 | `	return PH7_OK;` |
|      1 | 7195 |  |
|      - | 7196 | `/*` |
|      - | 7197 | ` * array getdate ([ int $timestamp = time() ])` |
|      - | 7198 | ` *  Get date/time information.` |
|      - | 7199 | ` * Parameter` |
|      - | 7200 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|      - | 7201 | ` *     that defaults to the current local time if a timestamp is not given.` |
|      - | 7202 | ` *     In other words, it defaults to the value of time().` |
|      - | 7203 | ` * Returns` |
|      - | 7204 | ` *  Returns an associative array of information related to the timestamp.` |
|      - | 7205 | ` *  Elements from the returned associative array are as follows:` |
|      - | 7206 | ` *   KEY                                                         VALUE` |
|      - | 7207 | ` * ---------                                                    -------` |
|      - | 7208 | ` * "seconds" 	Numeric representation of seconds 	            0 to 59` |
|      - | 7209 | ` * "minutes" 	Numeric representation of minutes 	            0 to 59` |
|      - | 7210 | ` * "hours" 	    Numeric representation of hours 	            0 to 23` |
|      - | 7211 | ` * "mday" 	    Numeric representation of the day of the month 	1 to 31` |
|      - | 7212 | ` * "wday" 	    Numeric representation of the day of the week 	0 (for Sunday) through 6 (for Saturday)` |
|      - | 7213 | ` * "mon" 	    Numeric representation of a month 	            1 through 12` |
|      - | 7214 | ` * "year" 	    A full numeric representation of a year,        4 digits 	Examples: 1999 or 2003` |
|      - | 7215 | ` * "yday" 	    Numeric representation of the day of the year   0 through 365` |
|      - | 7216 | ` * "weekday" 	A full textual representation of the day of the week 	Sunday through Saturday` |
|      - | 7217 | ` * "month" 	    A full textual representation of a month, such as January or March 	January through December` |
|      - | 7218 | ` * 0 	        Seconds since the Unix Epoch, similar to the values returned by time() and used by date().` |
|      - | 7219 | ` * NOTE:` |
|      - | 7220 | ` *   NULL is returned on failure.` |
|      - | 7221 | ` */` |
|      8 | 7222 | `static int PH7_builtin_getdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7223 |  |
|      - | 7224 | `	ph7_value *pValue,*pArray;` |
|      - | 7225 | `	Sytm sTm;` |
|      9 | 7226 | `	if( nArg < 1 ){` |
|      - | 7227 | `#ifdef __WINNT__` |
|      - | 7228 | `		SYSTEMTIME sOS;` |
|      1 | 7229 | `		GetSystemTime(&sOS);` |
|      1 | 7230 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 7231 | `#else` |
|      - | 7232 | `		struct tm *pTm;` |
|      - | 7233 | `		time_t t;` |
|      4 | 7234 | `		time(&t);` |
|      4 | 7235 | `		pTm = localtime(&t);` |
|      4 | 7236 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7237 | `#endif` |
|      3 | 7238 | `	}else{` |
|      - | 7239 | `		/* Use the given timestamp */` |
|      - | 7240 | `		time_t t;` |
|      - | 7241 | `		struct tm *pTm;` |
|      - | 7242 | `#ifdef __WINNT__` |
|      - | 7243 | `#ifdef _MSC_VER` |
|      - | 7244 | `#if _MSC_VER >= 1400 /* Visual Studio 2005 and up */` |
|      - | 7245 | `#pragma warning(disable:4996) /* _CRT_SECURE...*/` |
|      - | 7246 | `#endif` |
|      - | 7247 | `#endif` |
|      - | 7248 | `#endif` |
|      5 | 7249 | `		if( ph7_value_is_int(apArg[0]) ){` |
|      5 | 7250 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|      5 | 7251 | `			pTm = localtime(&t);` |
|      5 | 7252 | `			if( pTm == 0 ){` |
|    ! 0 | 7253 | `				time(&t);` |
|    ! 0 | 7254 | `			}` |
|      3 | 7255 | `		}else{` |
|    ! 0 | 7256 | `			time(&t);` |
|      - | 7257 | `		}` |
|      5 | 7258 | `		pTm = localtime(&t);` |
|      5 | 7259 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7260 | `	}` |
|      - | 7261 | `	/* Element value */` |
|      9 | 7262 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      9 | 7263 | `	if( pValue == 0 ){` |
|      - | 7264 | `		/* Return NULL */` |
|    ! 0 | 7265 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7266 | `		return PH7_OK;` |
|      - | 7267 | `	}` |
|      - | 7268 | `	/* Create a new array */` |
|      9 | 7269 | `	pArray = ph7_context_new_array(pCtx);` |
|      9 | 7270 | `	if( pArray == 0 ){` |
|      - | 7271 | `		/* Return NULL */` |
|    ! 0 | 7272 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7273 | `		return PH7_OK;` |
|      - | 7274 | `	}` |
|      - | 7275 | `	/* Fill the array */` |
|      - | 7276 | `	/* Seconds */` |
|      9 | 7277 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|      9 | 7278 | `	ph7_array_add_strkey_elem(pArray,"seconds",pValue);` |
|      - | 7279 | `	/* Minutes */` |
|      9 | 7280 | `	ph7_value_int(pValue,sTm.tm_min);` |
|      9 | 7281 | `	ph7_array_add_strkey_elem(pArray,"minutes",pValue);` |
|      - | 7282 | `	/* Hours */` |
|      9 | 7283 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|      9 | 7284 | `	ph7_array_add_strkey_elem(pArray,"hours",pValue);` |
|      - | 7285 | `	/* mday */` |
|      9 | 7286 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|      9 | 7287 | `	ph7_array_add_strkey_elem(pArray,"mday",pValue);` |
|      - | 7288 | `	/* wday */` |
|      9 | 7289 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|      9 | 7290 | `	ph7_array_add_strkey_elem(pArray,"wday",pValue);` |
|      - | 7291 | `	/* mon */` |
|      9 | 7292 | `	ph7_value_int(pValue,sTm.tm_mon+1);` |
|      9 | 7293 | `	ph7_array_add_strkey_elem(pArray,"mon",pValue);` |
|      - | 7294 | `	/* year */` |
|      9 | 7295 | `	ph7_value_int(pValue,sTm.tm_year);` |
|      9 | 7296 | `	ph7_array_add_strkey_elem(pArray,"year",pValue);` |
|      - | 7297 | `	/* yday */` |
|      9 | 7298 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|      9 | 7299 | `	ph7_array_add_strkey_elem(pArray,"yday",pValue);` |
|      - | 7300 | `	/* Weekday */` |
|      9 | 7301 | `	ph7_value_string(pValue,SyTimeGetDay(sTm.tm_wday),-1);` |
|      9 | 7302 | `	ph7_array_add_strkey_elem(pArray,"weekday",pValue);` |
|      - | 7303 | `	/* Month */` |
|      9 | 7304 | `	ph7_value_reset_string_cursor(pValue);` |
|      9 | 7305 | `	ph7_value_string(pValue,SyTimeGetMonth(sTm.tm_mon),-1);` |
|      9 | 7306 | `	ph7_array_add_strkey_elem(pArray,"month",pValue);` |
|      - | 7307 | `	/* Seconds since the epoch */` |
|      9 | 7308 | `	ph7_value_int64(pValue,(ph7_int64)time(0));` |
|      9 | 7309 | `	ph7_array_add_intkey_elem(pArray,0 /* Index zero */,pValue);` |
|      - | 7310 | `	/* Return the freshly created array */` |
|      9 | 7311 | `	ph7_result_value(pCtx,pArray);` |
|      9 | 7312 | `	return PH7_OK;` |
|      5 | 7313 |  |
|      - | 7314 | `/*` |
|      - | 7315 | ` * mixed gettimeofday([ bool $return_float = false ] )` |
|      - | 7316 | ` *   Returns an associative array containing the data returned from the system call.` |
|      - | 7317 | ` * Parameters` |
|      - | 7318 | ` *  $return_float` |
|      - | 7319 | ` *   When set to TRUE, a float instead of an array is returned.` |
|      - | 7320 | ` * Return` |
|      - | 7321 | ` *   By default an array is returned. If return_float is set, then` |
|      - | 7322 | ` *   a float is returned.` |
|      - | 7323 | ` */` |
|      4 | 7324 | `static int PH7_builtin_gettimeofday(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7325 |  |
|      5 | 7326 | `	int bFloat = 0;` |
|      - | 7327 | `	sytime sTime;` |
|      - | 7328 | `#if defined(__UNIXES__)` |
|      - | 7329 | `	struct timeval tv;` |
|      4 | 7330 | `	gettimeofday(&tv,0);` |
|      4 | 7331 | `	sTime.tm_sec  = (long)tv.tv_sec;` |
|      4 | 7332 | `	sTime.tm_usec = (long)tv.tv_usec;` |
|      - | 7333 | `#else` |
|      - | 7334 | `	time_t tt;` |
|      1 | 7335 | `	time(&tt);` |
|      1 | 7336 | `	sTime.tm_sec  = (long)tt;` |
|      1 | 7337 | `	sTime.tm_usec = (long)(tt%SX_USEC_PER_SEC);` |
|      - | 7338 | `#endif /* __UNIXES__ */` |
|      5 | 7339 | `	if( nArg > 0 ){` |
|      5 | 7340 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|      2 | 7341 | `	}` |
|      5 | 7342 | `	if( bFloat ){` |
|      - | 7343 | `		/* Return as float */` |
|      3 | 7344 | `		ph7_result_double(pCtx,(double)sTime.tm_sec);` |
|      2 | 7345 | `	}else{` |
|      - | 7346 | `		/* Return an associative array */` |
|      - | 7347 | `		ph7_value *pValue,*pArray;` |
|      - | 7348 | `		/* Create a new array */` |
|      3 | 7349 | `		pArray = ph7_context_new_array(pCtx);` |
|      - | 7350 | `		/* Element value */` |
|      3 | 7351 | `		pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 7352 | `		if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 7353 | `			/* Return NULL */` |
|    ! 0 | 7354 | `			ph7_result_null(pCtx);` |
|    ! 0 | 7355 | `			return PH7_OK;` |
|      - | 7356 | `		}` |
|      - | 7357 | `		/* Fill the array */` |
|      - | 7358 | `		/* sec */` |
|      3 | 7359 | `		ph7_value_int64(pValue,sTime.tm_sec);` |
|      3 | 7360 | `		ph7_array_add_strkey_elem(pArray,"sec",pValue);` |
|      - | 7361 | `		/* usec */` |
|      3 | 7362 | `		ph7_value_int64(pValue,sTime.tm_usec);` |
|      3 | 7363 | `		ph7_array_add_strkey_elem(pArray,"usec",pValue);` |
|      - | 7364 | `		/* Return the array */` |
|      3 | 7365 | `		ph7_result_value(pCtx,pArray);` |
|      - | 7366 | `	}` |
|      5 | 7367 | `	return PH7_OK;` |
|      3 | 7368 |  |
|      - | 7369 | `/* Check if the given year is leap or not */` |
|      - | 7370 | `#define IS_LEAP_YEAR(YEAR)	(YEAR % 400 ? ( YEAR % 100 ? ( YEAR % 4 ? 0 : 1 ) : 0 ) : 1)` |
|      - | 7371 | `/* ISO-8601 numeric representation of the day of the week */` |
|      - | 7372 | `static const int aISO8601[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|      - | 7373 | `/*` |
|      - | 7374 | ` * Format a given date string.` |
|      - | 7375 | ` * Supported format: (Taken from PHP online docs)` |
|      - | 7376 | ` * character 	Description` |
|      - | 7377 | ` * d          Day of the month` |
|      - | 7378 | ` * D          A textual representation of a days` |
|      - | 7379 | ` * j          Day of the month without leading zeros` |
|      - | 7380 | ` * l          A full textual representation of the day of the week` |
|      - | 7381 | ` * N          ISO-8601 numeric representation of the day of the week` |
|      - | 7382 | ` * w          Numeric representation of the day of the week` |
|      - | 7383 | ` * z          The day of the year (starting from 0)` |
|      - | 7384 | ` * F          A full textual representation of a month, such as January or March` |
|      - | 7385 | ` * m          Numeric representation of a month, with leading zeros 	01 through 12` |
|      - | 7386 | ` * M          A short textual representation of a month, three letters 	Jan through Dec` |
|      - | 7387 | ` * n          Numeric representation of a month, without leading zeros 	1 through 12` |
|      - | 7388 | ` * t          Number of days in the given month 	28 through 31` |
|      - | 7389 | ` * L          Whether it's a leap year 	1 if it is a leap year, 0 otherwise.` |
|      - | 7390 | ` * o          ISO-8601 year number. This has the same value as Y, except that if the ISO week number` |
|      - | 7391 | ` *            (W) belongs to the previous or next year, that year is used instead. (added in PHP 5.1.0) Examples: 1999 or 2003` |
|      - | 7392 | ` * Y          A full numeric representation of a year, 4 digits 	Examples: 1999 or 2003` |
|      - | 7393 | ` * y          A two digit representation of a year 	Examples: 99 or 03` |
|      - | 7394 | ` * a          Lowercase Ante meridiem and Post meridiem 	am or pm` |
|      - | 7395 | ` * A          Uppercase Ante meridiem and Post meridiem 	AM or PM` |
|      - | 7396 | ` * g          12-hour format of an hour without leading zeros 	1 through 12` |
|      - | 7397 | ` * G          24-hour format of an hour without leading zeros 	0 through 23` |
|      - | 7398 | ` * h          12-hour format of an hour with leading zeros 	01 through 12` |
|      - | 7399 | ` * H          24-hour format of an hour with leading zeros 	00 through 23` |
|      - | 7400 | ` * i          Minutes with leading zeros 	00 to 59` |
|      - | 7401 | ` * s          Seconds, with leading zeros 	00 through 59` |
|      - | 7402 | ` * u          Microseconds Example: 654321` |
|      - | 7403 | ` * e          Timezone identifier 	Examples: UTC, GMT, Atlantic/Azores` |
|      - | 7404 | ` * I          (capital i) Whether or not the date is in daylight saving time 	1 if Daylight Saving Time, 0 otherwise.` |
|      - | 7405 | ` * r          RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 +0200` |
|      - | 7406 | ` * U          Seconds since the Unix Epoch (January 1 1970 00:00:00 GMT)` |
|      - | 7407 | ` * S          English ordinal suffix for the day of the month, 2 characters` |
|      - | 7408 | ` * O          Difference to Greenwich time (GMT) in hours` |
|      - | 7409 | ` * Z          Timezone offset in seconds. The offset for timezones west of UTC is always negative, and for those` |
|      - | 7410 | ` *            east of UTC is always positive.` |
|      - | 7411 | ` * c         ISO 8601 date` |
|      - | 7412 | ` */` |
|     46 | 7413 | `static sxi32 DateFormat(ph7_context *pCtx,const char *zIn,int nLen,Sytm *pTm)` |
|      1 | 7414 |  |
|     47 | 7415 | `	const char *zEnd = &zIn[nLen];` |
|      - | 7416 | `	const char *zCur;` |
|      - | 7417 | `	/* Start the format process */` |
|     78 | 7418 | `	for(;;){` |
|    157 | 7419 | `		if( zIn >= zEnd ){` |
|      - | 7420 | `			/* No more input to process */` |
|     47 | 7421 | `			break;` |
|      - | 7422 | `		}` |
|    111 | 7423 | `		switch(zIn[0]){` |
|      7 | 7424 | `		case 'd':` |
|      - | 7425 | `			/* Day of the month, 2 digits with leading zeros */` |
|     15 | 7426 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mday);` |
|     15 | 7427 | `			break;` |
|    ! 0 | 7428 | `		case 'D':` |
|      - | 7429 | `			/*A textual representation of a day, three letters*/` |
|    ! 0 | 7430 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|    ! 0 | 7431 | `			ph7_result_string(pCtx,zCur,3);` |
|    ! 0 | 7432 | `			break;` |
|    ! 0 | 7433 | `		case 'j':` |
|      - | 7434 | `			/*	Day of the month without leading zeros */` |
|    ! 0 | 7435 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mday);` |
|    ! 0 | 7436 | `			break;` |
|      2 | 7437 | `		case 'l':` |
|      - | 7438 | `			/* A full textual representation of the day of the week */` |
|      5 | 7439 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|      5 | 7440 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|      5 | 7441 | `			break;` |
|    ! 0 | 7442 | `		case 'N':{` |
|      - | 7443 | `			/* ISO-8601 numeric representation of the day of the week */` |
|    ! 0 | 7444 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|    ! 0 | 7445 | `			break;` |
|      - | 7446 | `				 }` |
|    ! 0 | 7447 | `		case 'w':` |
|      - | 7448 | `			/*Numeric representation of the day of the week*/` |
|    ! 0 | 7449 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|    ! 0 | 7450 | `			break;` |
|    ! 0 | 7451 | `		case 'z':` |
|      - | 7452 | `			/*The day of the year*/` |
|    ! 0 | 7453 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_yday);` |
|    ! 0 | 7454 | `			break;` |
|      2 | 7455 | `		case 'F':` |
|      - | 7456 | `			/*A full textual representation of a month, such as January or March*/` |
|      5 | 7457 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|      5 | 7458 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|      5 | 7459 | `			break;` |
|      7 | 7460 | `		case 'm':` |
|      - | 7461 | `			/*Numeric representation of a month, with leading zeros*/` |
|     15 | 7462 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|     15 | 7463 | `			break;` |
|    ! 0 | 7464 | `		case 'M':` |
|      - | 7465 | `			/*A short textual representation of a month, three letters*/` |
|    ! 0 | 7466 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|    ! 0 | 7467 | `			ph7_result_string(pCtx,zCur,3);` |
|    ! 0 | 7468 | `			break;` |
|    ! 0 | 7469 | `		case 'n':` |
|      - | 7470 | `			/*Numeric representation of a month, without leading zeros*/` |
|    ! 0 | 7471 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mon + 1);` |
|    ! 0 | 7472 | `			break;` |
|    ! 0 | 7473 | `		case 't':{` |
|      - | 7474 | `			static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|    ! 0 | 7475 | `			int nDays = aMonDays[pTm->tm_mon % 12 ];` |
|    ! 0 | 7476 | `			if( pTm->tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(pTm->tm_year) ){` |
|    ! 0 | 7477 | `				nDays = 28;` |
|    ! 0 | 7478 | `			}` |
|      - | 7479 | `			/*Number of days in the given month*/` |
|    ! 0 | 7480 | `			ph7_result_string_format(pCtx,"%d",nDays);` |
|    ! 0 | 7481 | `			break;` |
|      - | 7482 | `				 }` |
|    ! 0 | 7483 | `		case 'L':{` |
|    ! 0 | 7484 | `			int isLeap = IS_LEAP_YEAR(pTm->tm_year);` |
|      - | 7485 | `			/* Whether it's a leap year */` |
|    ! 0 | 7486 | `			ph7_result_string_format(pCtx,"%d",isLeap);` |
|    ! 0 | 7487 | `			break;` |
|      - | 7488 | `				 }` |
|    ! 0 | 7489 | `		case 'o':` |
|      - | 7490 | `			/* ISO-8601 year number.*/` |
|    ! 0 | 7491 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|    ! 0 | 7492 | `			break;` |
|      9 | 7493 | `		case 'Y':` |
|      - | 7494 | `			/*	A full numeric representation of a year, 4 digits */` |
|     19 | 7495 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|     19 | 7496 | `			break;` |
|    ! 0 | 7497 | `		case 'y':` |
|      - | 7498 | `			/*A two digit representation of a year*/` |
|    ! 0 | 7499 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_year%100);` |
|    ! 0 | 7500 | `			break;` |
|    ! 0 | 7501 | `		case 'a':` |
|      - | 7502 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7503 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",2);` |
|    ! 0 | 7504 | `			break;` |
|    ! 0 | 7505 | `		case 'A':` |
|      - | 7506 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7507 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",2);` |
|    ! 0 | 7508 | `			break;` |
|    ! 0 | 7509 | `		case 'g':` |
|      - | 7510 | `			/*	12-hour format of an hour without leading zeros*/` |
|    ! 0 | 7511 | `			ph7_result_string_format(pCtx,"%d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7512 | `			break;` |
|    ! 0 | 7513 | `		case 'G':` |
|      - | 7514 | `			/* 24-hour format of an hour without leading zeros */` |
|    ! 0 | 7515 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_hour);` |
|    ! 0 | 7516 | `			break;` |
|    ! 0 | 7517 | `		case 'h':` |
|      - | 7518 | `			/* 12-hour format of an hour with leading zeros */` |
|    ! 0 | 7519 | `			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7520 | `			break;` |
|      3 | 7521 | `		case 'H':` |
|      - | 7522 | `			/*	24-hour format of an hour with leading zeros */` |
|      7 | 7523 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|      7 | 7524 | `			break;` |
|      3 | 7525 | `		case 'i':` |
|      - | 7526 | `			/* 	Minutes with leading zeros */` |
|      7 | 7527 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|      7 | 7528 | `			break;` |
|      3 | 7529 | `		case 's':` |
|      - | 7530 | `			/* 	second with leading zeros */` |
|      7 | 7531 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|      7 | 7532 | `			break;` |
|    ! 0 | 7533 | `		case 'u':` |
|      - | 7534 | `			/* 	Microseconds */` |
|    ! 0 | 7535 | `			ph7_result_string_format(pCtx,"%u",pTm->tm_sec * SX_USEC_PER_SEC);` |
|    ! 0 | 7536 | `			break;` |
|    ! 0 | 7537 | `		case 'S':{` |
|      - | 7538 | `			/* English ordinal suffix for the day of the month, 2 characters */` |
|      - | 7539 | `			static const char zSuffix[] = "thstndrdthththththth";` |
|    ! 0 | 7540 | `			int v = pTm->tm_mday;` |
|    ! 0 | 7541 | `			ph7_result_string(pCtx,&zSuffix[2 * (int)(v / 10 % 10 != 1 ? v % 10 : 0)],(int)sizeof(char) * 2);` |
|    ! 0 | 7542 | `			break;` |
|      - | 7543 | `				 }` |
|    ! 0 | 7544 | `		case 'e':` |
|      - | 7545 | `			/* 	Timezone identifier */` |
|    ! 0 | 7546 | `			zCur = pTm->tm_zone;` |
|    ! 0 | 7547 | `			if( zCur == 0 ){` |
|      - | 7548 | `				/* Assume GMT */` |
|    ! 0 | 7549 | `				zCur = "GMT";` |
|    ! 0 | 7550 | `			}` |
|    ! 0 | 7551 | `			ph7_result_string(pCtx,zCur,-1);` |
|    ! 0 | 7552 | `			break;` |
|    ! 0 | 7553 | `		case 'I':` |
|      - | 7554 | `			/* Whether or not the date is in daylight saving time */` |
|      - | 7555 | `#ifdef __WINNT__` |
|      - | 7556 | `#ifdef _MSC_VER` |
|      - | 7557 | `#ifndef _WIN32_WCE` |
|    ! 0 | 7558 | `			_get_daylight(&pTm->tm_isdst);` |
|      - | 7559 | `#endif` |
|      - | 7560 | `#endif` |
|      - | 7561 | `#endif` |
|    ! 0 | 7562 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_isdst == 1);` |
|    ! 0 | 7563 | `			break;` |
|    ! 0 | 7564 | `		case 'r':` |
|      - | 7565 | `			/* RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 */` |
|    ! 0 | 7566 | `			ph7_result_string_format(pCtx,"%.3s, %02d %.3s %4d %02d:%02d:%02d",` |
|    ! 0 | 7567 | `				SyTimeGetDay(pTm->tm_wday),` |
|    ! 0 | 7568 | `				pTm->tm_mday,` |
|    ! 0 | 7569 | `				SyTimeGetMonth(pTm->tm_mon),` |
|    ! 0 | 7570 | `				pTm->tm_year,` |
|    ! 0 | 7571 | `				pTm->tm_hour,` |
|    ! 0 | 7572 | `				pTm->tm_min,` |
|    ! 0 | 7573 | `				pTm->tm_sec` |
|      - | 7574 | `				);` |
|    ! 0 | 7575 | `			break;` |
|    ! 0 | 7576 | `		case 'U':{` |
|      - | 7577 | `			time_t tt;` |
|      - | 7578 | `			/* Seconds since the Unix Epoch */` |
|    ! 0 | 7579 | `			time(&tt);` |
|    ! 0 | 7580 | `			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);` |
|    ! 0 | 7581 | `			break;` |
|      - | 7582 | `				 }` |
|    ! 0 | 7583 | `		case 'O':` |
|      - | 7584 | `		case 'P':` |
|      - | 7585 | `			/* Difference to Greenwich time (GMT) in hours */` |
|    ! 0 | 7586 | `			ph7_result_string_format(pCtx,"%+05d",pTm->tm_gmtoff);` |
|    ! 0 | 7587 | `			break;` |
|    ! 0 | 7588 | `		case 'Z':` |
|      - | 7589 | `			/* Timezone offset in seconds. The offset for timezones west of UTC` |
|      - | 7590 | `			 * is always negative, and for those east of UTC is always positive.` |
|      - | 7591 | `			 */` |
|    ! 0 | 7592 | `			ph7_result_string_format(pCtx,"%+05d",pTm->tm_gmtoff);` |
|    ! 0 | 7593 | `			break;` |
|      1 | 7594 | `		case 'c':` |
|      - | 7595 | `			/* 	ISO 8601 date */` |
|      4 | 7596 | `			ph7_result_string_format(pCtx,"%4d-%02d-%02dT%02d:%02d:%02d%+05d",` |
|      1 | 7597 | `				pTm->tm_year,` |
|      2 | 7598 | `				pTm->tm_mon+1,` |
|      1 | 7599 | `				pTm->tm_mday,` |
|      1 | 7600 | `				pTm->tm_hour,` |
|      1 | 7601 | `				pTm->tm_min,` |
|      1 | 7602 | `				pTm->tm_sec,` |
|      1 | 7603 | `				pTm->tm_gmtoff` |
|      - | 7604 | `				);` |
|      3 | 7605 | `			break;` |
|      1 | 7606 | `		case '\\':` |
|      3 | 7607 | `			zIn++;` |
|      - | 7608 | `			/* Expand verbatim */` |
|      3 | 7609 | `			if( zIn < zEnd ){` |
|      3 | 7610 | `				ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|      1 | 7611 | `			}` |
|      3 | 7612 | `			break;` |
|     17 | 7613 | `		default:` |
|      - | 7614 | `			/* Unknown format specifer,expand verbatim */` |
|     35 | 7615 | `			ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|     34 | 7616 | `			break;` |
|      - | 7617 | `		}` |
|      - | 7618 | `		/* Point to the next character */` |
|    111 | 7619 | `		zIn++;` |
|      1 | 7620 | `	}` |
|     47 | 7621 | `	return SXRET_OK;` |
|      1 | 7622 |  |
|      - | 7623 | `/*` |
|      - | 7624 | ` * PH7 implementation of the strftime() function.` |
|      - | 7625 | ` * The following formats are supported:` |
|      - | 7626 | ` * %a 	An abbreviated textual representation of the day` |
|      - | 7627 | ` * %A 	A full textual representation of the day` |
|      - | 7628 | ` * %d 	Two-digit day of the month (with leading zeros)` |
|      - | 7629 | ` * %e 	Day of the month, with a space preceding single digits.` |
|      - | 7630 | ` * %j 	Day of the year, 3 digits with leading zeros` |
|      - | 7631 | ` * %u 	ISO-8601 numeric representation of the day of the week 	1 (for Monday) though 7 (for Sunday)` |
|      - | 7632 | ` * %w 	Numeric representation of the day of the week 0 (for Sunday) through 6 (for Saturday)` |
|      - | 7633 | ` * %U 	Week number of the given year, starting with the first Sunday as the first week` |
|      - | 7634 | ` * %V 	ISO-8601:1988 week number of the given year, starting with the first week of the year with at least` |
|      - | 7635 | ` *   4 weekdays, with Monday being the start of the week.` |
|      - | 7636 | ` * %W 	A numeric representation of the week of the year` |
|      - | 7637 | ` * %b 	Abbreviated month name, based on the locale` |
|      - | 7638 | ` * %B 	Full month name, based on the locale` |
|      - | 7639 | ` * %h 	Abbreviated month name, based on the locale (an alias of %b)` |
|      - | 7640 | ` * %m 	Two digit representation of the month` |
|      - | 7641 | ` * %C 	Two digit representation of the century (year divided by 100, truncated to an integer)` |
|      - | 7642 | ` * %g 	Two digit representation of the year going by ISO-8601:1988 standards (see %V)` |
|      - | 7643 | ` * %G 	The full four-digit version of %g` |
|      - | 7644 | ` * %y 	Two digit representation of the year` |
|      - | 7645 | ` * %Y 	Four digit representation for the year` |
|      - | 7646 | ` * %H 	Two digit representation of the hour in 24-hour format` |
|      - | 7647 | ` * %I 	Two digit representation of the hour in 12-hour format` |
|      - | 7648 | ` * %l (lower-case 'L') 	Hour in 12-hour format, with a space preceeding single digits` |
|      - | 7649 | ` * %M 	Two digit representation of the minute` |
|      - | 7650 | ` * %p 	UPPER-CASE 'AM' or 'PM' based on the given time` |
|      - | 7651 | ` * %P 	lower-case 'am' or 'pm' based on the given time` |
|      - | 7652 | ` * %r 	Same as "%I:%M:%S %p"` |
|      - | 7653 | ` * %R 	Same as "%H:%M"` |
|      - | 7654 | ` * %S 	Two digit representation of the second` |
|      - | 7655 | ` * %T 	Same as "%H:%M:%S"` |
|      - | 7656 | ` * %X 	Preferred time representation based on locale, without the date` |
|      - | 7657 | ` * %z 	Either the time zone offset from UTC or the abbreviation` |
|      - | 7658 | ` * %Z 	The time zone offset/abbreviation option NOT given by %z` |
|      - | 7659 | ` * %c 	Preferred date and time stamp based on local` |
|      - | 7660 | ` * %D 	Same as "%m/%d/%y"` |
|      - | 7661 | ` * %F 	Same as "%Y-%m-%d"` |
|      - | 7662 | ` * %s 	Unix Epoch Time timestamp (same as the time() function)` |
|      - | 7663 | ` * %x 	Preferred date representation based on locale, without the time` |
|      - | 7664 | ` * %n 	A newline character ("\n")` |
|      - | 7665 | ` * %t 	A Tab character ("\t")` |
|      - | 7666 | ` * %% 	A literal percentage character ("%")` |
|      - | 7667 | ` */` |
|     16 | 7668 | `static int PH7_Strftime(` |
|      - | 7669 | `	ph7_context *pCtx,  /* Call context */` |
|      - | 7670 | `	const char *zIn,    /* Input string */` |
|      - | 7671 | `	int nLen,           /* Input length */` |
|      - | 7672 | `	Sytm *pTm           /* Parse of the given time */` |
|      - | 7673 | `	)` |
|      1 | 7674 |  |
|     17 | 7675 | `	const char *zCur,*zEnd = &zIn[nLen];` |
|      - | 7676 | `	int c;` |
|      - | 7677 | `	/* Start the format process */` |
|     18 | 7678 | `	for(;;){` |
|     37 | 7679 | `		zCur = zIn;` |
|     41 | 7680 | `		while(zIn < zEnd && zIn[0] != '%' ){` |
|      5 | 7681 | `			zIn++;` |
|      1 | 7682 | `		}` |
|     37 | 7683 | `		if( zIn > zCur ){` |
|      - | 7684 | `			/* Consume input verbatim */` |
|      5 | 7685 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 7686 | `		}` |
|     37 | 7687 | `		zIn++; /* Jump the percent sign */` |
|     37 | 7688 | `		if( zIn >= zEnd ){` |
|      - | 7689 | `			/* No more input to process */` |
|     17 | 7690 | `			break;` |
|      - | 7691 | `		}` |
|     21 | 7692 | `		c = zIn[0];` |
|      - | 7693 | `		/* Act according to the current specifer */` |
|     21 | 7694 | `		switch(c){` |
|    ! 0 | 7695 | `		case '%':` |
|      - | 7696 | `			/* A literal percentage character ("%") */` |
|    ! 0 | 7697 | `			ph7_result_string(pCtx,"%",(int)sizeof(char));` |
|    ! 0 | 7698 | `			break;` |
|    ! 0 | 7699 | `		case 't':` |
|      - | 7700 | `			/* A Tab character */` |
|    ! 0 | 7701 | `			ph7_result_string(pCtx,"\t",(int)sizeof(char));` |
|    ! 0 | 7702 | `			break;` |
|    ! 0 | 7703 | `		case 'n':` |
|      - | 7704 | `			/* A newline character */` |
|    ! 0 | 7705 | `			ph7_result_string(pCtx,"\n",(int)sizeof(char));` |
|    ! 0 | 7706 | `			break;` |
|      1 | 7707 | `		case 'a':` |
|      - | 7708 | `			/* An abbreviated textual representation of the day */` |
|      3 | 7709 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),(int)sizeof(char)*3);` |
|      3 | 7710 | `			break;` |
|    ! 0 | 7711 | `		case 'A':` |
|      - | 7712 | `			/* A full textual representation of the day */` |
|    ! 0 | 7713 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),-1/*Compute length automatically*/);` |
|    ! 0 | 7714 | `			break;` |
|    ! 0 | 7715 | `		case 'e':` |
|      - | 7716 | `			/* Day of the month, 2 digits with leading space for single digit*/` |
|    ! 0 | 7717 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_mday);` |
|    ! 0 | 7718 | `			break;` |
|      2 | 7719 | `		case 'd':` |
|      - | 7720 | `			/* Two-digit day of the month (with leading zeros) */` |
|      5 | 7721 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon+1);` |
|      5 | 7722 | `			break;` |
|    ! 0 | 7723 | `		case 'j':` |
|      - | 7724 | `			/*The day of the year,3 digits with leading zeros*/` |
|    ! 0 | 7725 | `			ph7_result_string_format(pCtx,"%03d",pTm->tm_yday);` |
|    ! 0 | 7726 | `			break;` |
|    ! 0 | 7727 | `		case 'u':` |
|      - | 7728 | `			/* ISO-8601 numeric representation of the day of the week */` |
|    ! 0 | 7729 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|    ! 0 | 7730 | `			break;` |
|    ! 0 | 7731 | `		case 'w':` |
|      - | 7732 | `			/* Numeric representation of the day of the week */` |
|    ! 0 | 7733 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|    ! 0 | 7734 | `			break;` |
|    ! 0 | 7735 | `		case 'b':` |
|      - | 7736 | `		case 'h':` |
|      - | 7737 | `			/*A short textual representation of a month, three letters (Not based on locale)*/` |
|    ! 0 | 7738 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),(int)sizeof(char)*3);` |
|    ! 0 | 7739 | `			break;` |
|    ! 0 | 7740 | `		case 'B':` |
|      - | 7741 | `			/* Full month name (Not based on locale) */` |
|    ! 0 | 7742 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),-1/*Compute length automatically*/);` |
|    ! 0 | 7743 | `			break;` |
|      2 | 7744 | `		case 'm':` |
|      - | 7745 | `			/*Numeric representation of a month, with leading zeros*/` |
|      5 | 7746 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|      5 | 7747 | `			break;` |
|    ! 0 | 7748 | `		case 'C':` |
|      - | 7749 | `			/* Two digit representation of the century */` |
|    ! 0 | 7750 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year/100);` |
|    ! 0 | 7751 | `			break;` |
|    ! 0 | 7752 | `		case 'y':` |
|      - | 7753 | `		case 'g':` |
|      - | 7754 | `			/* Two digit representation of the year */` |
|    ! 0 | 7755 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year%100);` |
|    ! 0 | 7756 | `			break;` |
|      2 | 7757 | `		case 'Y':` |
|      - | 7758 | `		case 'G':` |
|      - | 7759 | `			/* Four digit representation of the year */` |
|      5 | 7760 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|      5 | 7761 | `			break;` |
|    ! 0 | 7762 | `		case 'I':` |
|      - | 7763 | `			/* 12-hour format of an hour with leading zeros */` |
|    ! 0 | 7764 | `			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7765 | `			break;` |
|    ! 0 | 7766 | `		case 'l':` |
|      - | 7767 | `			/* 12-hour format of an hour with leading space */` |
|    ! 0 | 7768 | `			ph7_result_string_format(pCtx,"%2d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7769 | `			break;` |
|      1 | 7770 | `		case 'H':` |
|      - | 7771 | `			/* 24-hour format of an hour with leading zeros */` |
|      3 | 7772 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|      3 | 7773 | `			break;` |
|      1 | 7774 | `		case 'M':` |
|      - | 7775 | `			/* Minutes with leading zeros */` |
|      3 | 7776 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|      3 | 7777 | `			break;` |
|    ! 0 | 7778 | `		case 'S':` |
|      - | 7779 | `			/* Seconds with leading zeros */` |
|    ! 0 | 7780 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|    ! 0 | 7781 | `			break;` |
|    ! 0 | 7782 | `		case 'z':` |
|      - | 7783 | `		case 'Z':` |
|      - | 7784 | `			/* 	Timezone identifier */` |
|    ! 0 | 7785 | `			zCur = pTm->tm_zone;` |
|    ! 0 | 7786 | `			if( zCur == 0 ){` |
|      - | 7787 | `				/* Assume GMT */` |
|    ! 0 | 7788 | `				zCur = "GMT";` |
|    ! 0 | 7789 | `			}` |
|    ! 0 | 7790 | `			ph7_result_string(pCtx,zCur,-1);` |
|    ! 0 | 7791 | `			break;` |
|    ! 0 | 7792 | `		case 'T':` |
|      - | 7793 | `		case 'X':` |
|      - | 7794 | `			/* Same as "%H:%M:%S" */` |
|    ! 0 | 7795 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d",pTm->tm_hour,pTm->tm_min,pTm->tm_sec);` |
|    ! 0 | 7796 | `			break;` |
|    ! 0 | 7797 | `		case 'R':` |
|      - | 7798 | `			/* Same as "%H:%M" */` |
|    ! 0 | 7799 | `			ph7_result_string_format(pCtx,"%02d:%02d",pTm->tm_hour,pTm->tm_min);` |
|    ! 0 | 7800 | `			break;` |
|    ! 0 | 7801 | `		case 'P':` |
|      - | 7802 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7803 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",(int)sizeof(char)*2);` |
|    ! 0 | 7804 | `			break;` |
|    ! 0 | 7805 | `		case 'p':` |
|      - | 7806 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7807 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",(int)sizeof(char)*2);` |
|    ! 0 | 7808 | `			break;` |
|    ! 0 | 7809 | `		case 'r':` |
|      - | 7810 | `			/* Same as "%I:%M:%S %p" */` |
|    ! 0 | 7811 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d %s",` |
|    ! 0 | 7812 | `				1+(pTm->tm_hour%12),` |
|    ! 0 | 7813 | `				pTm->tm_min,` |
|    ! 0 | 7814 | `				pTm->tm_sec,` |
|    ! 0 | 7815 | `				pTm->tm_hour > 12 ? "PM" : "AM"` |
|      - | 7816 | `				);` |
|    ! 0 | 7817 | `			break;` |
|      1 | 7818 | `		case 'D':` |
|      - | 7819 | `		case 'x':` |
|      - | 7820 | `			/* Same as "%m/%d/%y" */` |
|      4 | 7821 | `			ph7_result_string_format(pCtx,"%02d/%02d/%02d",` |
|      2 | 7822 | `				pTm->tm_mon+1,` |
|      1 | 7823 | `				pTm->tm_mday,` |
|      2 | 7824 | `				pTm->tm_year%100` |
|      - | 7825 | `				);` |
|      3 | 7826 | `			break;` |
|    ! 0 | 7827 | `		case 'F':` |
|      - | 7828 | `			/* Same as "%Y-%m-%d" */` |
|    ! 0 | 7829 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d",` |
|    ! 0 | 7830 | `				pTm->tm_year,` |
|    ! 0 | 7831 | `				pTm->tm_mon+1,` |
|    ! 0 | 7832 | `				pTm->tm_mday` |
|      - | 7833 | `				);` |
|    ! 0 | 7834 | `			break;` |
|    ! 0 | 7835 | `		case 'c':` |
|    ! 0 | 7836 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d %02d:%02d:%02d",` |
|    ! 0 | 7837 | `				pTm->tm_year,` |
|    ! 0 | 7838 | `				pTm->tm_mon+1,` |
|    ! 0 | 7839 | `				pTm->tm_mday,` |
|    ! 0 | 7840 | `				pTm->tm_hour,` |
|    ! 0 | 7841 | `				pTm->tm_min,` |
|    ! 0 | 7842 | `				pTm->tm_sec` |
|      - | 7843 | `				);` |
|    ! 0 | 7844 | `			break;` |
|    ! 0 | 7845 | `		case 's':{` |
|      - | 7846 | `			time_t tt;` |
|      - | 7847 | `			/* Seconds since the Unix Epoch */` |
|    ! 0 | 7848 | `			time(&tt);` |
|    ! 0 | 7849 | `			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);` |
|    ! 0 | 7850 | `			break;` |
|      - | 7851 | `				 }` |
|    ! 0 | 7852 | `		default:` |
|      - | 7853 | `			/* unknown specifer,simply ignore*/` |
|    ! 0 | 7854 | `			break;` |
|      - | 7855 | `		}` |
|      - | 7856 | `		/* Advance the cursor */` |
|     21 | 7857 | `		zIn++;` |
|      1 | 7858 | `	}` |
|     17 | 7859 | `	return SXRET_OK;` |
|      1 | 7860 |  |
|      - | 7861 | `/*` |
|      - | 7862 | ` * string date(string $format [, int $timestamp = time() ] )` |
|      - | 7863 | ` *  Returns a string formatted according to the given format string using` |
|      - | 7864 | ` *  the given integer timestamp or the current time if no timestamp is given.` |
|      - | 7865 | ` *  In other words, timestamp is optional and defaults to the value of time().` |
|      - | 7866 | ` * Parameters` |
|      - | 7867 | ` *  $format` |
|      - | 7868 | ` *   The format of the outputted date string (See code above)` |
|      - | 7869 | ` * $timestamp` |
|      - | 7870 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|      - | 7871 | ` *   that defaults to the current local time if a timestamp is not given.` |
|      - | 7872 | ` *   In other words, it defaults to the value of time().` |
|      - | 7873 | ` * Return` |
|      - | 7874 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|      - | 7875 | ` */` |
|     36 | 7876 | `static int PH7_builtin_date(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7877 |  |
|      - | 7878 | `	const char *zFormat;` |
|      - | 7879 | `	int nLen;` |
|      - | 7880 | `	Sytm sTm;` |
|     37 | 7881 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7882 | `		/* Missing/Invalid argument,return FALSE */` |
|      5 | 7883 | `		ph7_result_bool(pCtx,0);` |
|      5 | 7884 | `		return PH7_OK;` |
|      - | 7885 | `	}` |
|     33 | 7886 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     33 | 7887 | `	if( nLen < 1 ){` |
|      - | 7888 | `		/* Don't bother processing return the empty string */` |
|    ! 0 | 7889 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7890 | `	}` |
|     33 | 7891 | `	if( nArg < 2 ){` |
|      - | 7892 | `#ifdef __WINNT__` |
|      - | 7893 | `		SYSTEMTIME sOS;` |
|      1 | 7894 | `		GetSystemTime(&sOS);` |
|      1 | 7895 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 7896 | `#else` |
|      - | 7897 | `		struct tm *pTm;` |
|      - | 7898 | `		time_t t;` |
|     30 | 7899 | `		time(&t);` |
|     30 | 7900 | `		pTm = localtime(&t);` |
|     30 | 7901 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7902 | `#endif` |
|     16 | 7903 | `	}else{` |
|      - | 7904 | `		/* Use the given timestamp */` |
|      - | 7905 | `		time_t t;` |
|      - | 7906 | `		struct tm *pTm;` |
|      3 | 7907 | `		if( ph7_value_is_int(apArg[1]) ){` |
|      3 | 7908 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|      3 | 7909 | `			pTm = localtime(&t);` |
|      3 | 7910 | `			if( pTm == 0 ){` |
|    ! 0 | 7911 | `				time(&t);` |
|    ! 0 | 7912 | `			}` |
|      2 | 7913 | `		}else{` |
|    ! 0 | 7914 | `			time(&t);` |
|      - | 7915 | `		}` |
|      3 | 7916 | `		pTm = localtime(&t);` |
|      3 | 7917 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7918 | `	}` |
|      - | 7919 | `	/* Format the given string */` |
|     33 | 7920 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|     33 | 7921 | `	return PH7_OK;` |
|     19 | 7922 |  |
|      - | 7923 | `/*` |
|      - | 7924 | ` * string strftime(string $format [, int $timestamp = time() ] )` |
|      - | 7925 | ` *  Format a local time/date (PLATFORM INDEPENDANT IMPLEENTATION NOT BASED ON LOCALE)` |
|      - | 7926 | ` * Parameters` |
|      - | 7927 | ` *  $format` |
|      - | 7928 | ` *   The format of the outputted date string (See code above)` |
|      - | 7929 | ` * $timestamp` |
|      - | 7930 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|      - | 7931 | ` *   that defaults to the current local time if a timestamp is not given.` |
|      - | 7932 | ` *   In other words, it defaults to the value of time().` |
|      - | 7933 | ` * Return` |
|      - | 7934 | ` * Returns a string formatted according format using the given timestamp` |
|      - | 7935 | ` * or the current local time if no timestamp is given.` |
|      - | 7936 | ` */` |
|     20 | 7937 | `static int PH7_builtin_strftime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7938 |  |
|      - | 7939 | `	const char *zFormat;` |
|      - | 7940 | `	int nLen;` |
|      - | 7941 | `	Sytm sTm;` |
|     21 | 7942 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7943 | `		/* Missing/Invalid argument,return FALSE */` |
|      5 | 7944 | `		ph7_result_bool(pCtx,0);` |
|      5 | 7945 | `		return PH7_OK;` |
|      - | 7946 | `	}` |
|     17 | 7947 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7948 | `	if( nLen < 1 ){` |
|      - | 7949 | `		/* Don't bother processing return FALSE */` |
|    ! 0 | 7950 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7951 | `	}` |
|     17 | 7952 | `	if( nArg < 2 ){` |
|      - | 7953 | `#ifdef __WINNT__` |
|      - | 7954 | `		SYSTEMTIME sOS;` |
|      1 | 7955 | `		GetSystemTime(&sOS);` |
|      1 | 7956 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 7957 | `#else` |
|      - | 7958 | `		struct tm *pTm;` |
|      - | 7959 | `		time_t t;` |
|     14 | 7960 | `		time(&t);` |
|     14 | 7961 | `		pTm = localtime(&t);` |
|     14 | 7962 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7963 | `#endif` |
|      8 | 7964 | `	}else{` |
|      - | 7965 | `		/* Use the given timestamp */` |
|      - | 7966 | `		time_t t;` |
|      - | 7967 | `		struct tm *pTm;` |
|      3 | 7968 | `		if( ph7_value_is_int(apArg[1]) ){` |
|      3 | 7969 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|      3 | 7970 | `			pTm = localtime(&t);` |
|      3 | 7971 | `			if( pTm == 0 ){` |
|    ! 0 | 7972 | `				time(&t);` |
|    ! 0 | 7973 | `			}` |
|      2 | 7974 | `		}else{` |
|    ! 0 | 7975 | `			time(&t);` |
|      - | 7976 | `		}` |
|      3 | 7977 | `		pTm = localtime(&t);` |
|      3 | 7978 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7979 | `	}` |
|      - | 7980 | `	/* Format the given string */` |
|     17 | 7981 | `	PH7_Strftime(pCtx,zFormat,nLen,&sTm);` |
|     17 | 7982 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|      - | 7983 | `		/* Nothing was formatted,return FALSE */` |
|    ! 0 | 7984 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7985 | `	}` |
|     17 | 7986 | `	return PH7_OK;` |
|     11 | 7987 |  |
|      - | 7988 | `/*` |
|      - | 7989 | ` * string gmdate(string $format [, int $timestamp = time() ] )` |
|      - | 7990 | ` *  Identical to the date() function except that the time returned` |
|      - | 7991 | ` *  is Greenwich Mean Time (GMT).` |
|      - | 7992 | ` * Parameters` |
|      - | 7993 | ` *  $format` |
|      - | 7994 | ` *  The format of the outputted date string (See code above)` |
|      - | 7995 | ` *  $timestamp` |
|      - | 7996 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|      - | 7997 | ` *   that defaults to the current local time if a timestamp is not given.` |
|      - | 7998 | ` *   In other words, it defaults to the value of time().` |
|      - | 7999 | ` * Return` |
|      - | 8000 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|      - | 8001 | ` */` |
|     16 | 8002 | `static int PH7_builtin_gmdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8003 |  |
|      - | 8004 | `	const char *zFormat;` |
|      - | 8005 | `	int nLen;` |
|      - | 8006 | `	Sytm sTm;` |
|     17 | 8007 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8008 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 | 8009 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8010 | `		return PH7_OK;` |
|      - | 8011 | `	}` |
|     15 | 8012 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 8013 | `	if( nLen < 1 ){` |
|      - | 8014 | `		/* Don't bother processing return the empty string */` |
|    ! 0 | 8015 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 8016 | `	}` |
|     15 | 8017 | `	if( nArg < 2 ){` |
|      - | 8018 | `#ifdef __WINNT__` |
|      - | 8019 | `		SYSTEMTIME sOS;` |
|      1 | 8020 | `		GetSystemTime(&sOS);` |
|      1 | 8021 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8022 | `#else` |
|      - | 8023 | `		struct tm *pTm;` |
|      - | 8024 | `		time_t t;` |
|     12 | 8025 | `		time(&t);` |
|     12 | 8026 | `		pTm = gmtime(&t);` |
|     12 | 8027 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8028 | `#endif` |
|      7 | 8029 | `	}else{` |
|      - | 8030 | `		/* Use the given timestamp */` |
|      - | 8031 | `		time_t t;` |
|      - | 8032 | `		struct tm *pTm;` |
|      3 | 8033 | `		if( ph7_value_is_int(apArg[1]) ){` |
|      3 | 8034 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|      3 | 8035 | `			pTm = gmtime(&t);` |
|      3 | 8036 | `			if( pTm == 0 ){` |
|    ! 0 | 8037 | `				time(&t);` |
|    ! 0 | 8038 | `			}` |
|      2 | 8039 | `		}else{` |
|    ! 0 | 8040 | `			time(&t);` |
|      - | 8041 | `		}` |
|      3 | 8042 | `		pTm = gmtime(&t);` |
|      3 | 8043 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8044 | `	}` |
|      - | 8045 | `	/* Format the given string */` |
|     15 | 8046 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|     15 | 8047 | `	return PH7_OK;` |
|      9 | 8048 |  |
|      - | 8049 | `/*` |
|      - | 8050 | ` * array localtime([ int $timestamp = time() [, bool $is_associative = false ]])` |
|      - | 8051 | ` *  Return the local time.` |
|      - | 8052 | ` * Parameter` |
|      - | 8053 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|      - | 8054 | ` *     that defaults to the current local time if a timestamp is not given.` |
|      - | 8055 | ` *     In other words, it defaults to the value of time().` |
|      - | 8056 | ` * $is_associative` |
|      - | 8057 | ` *   If set to FALSE or not supplied then the array is returned as a regular, numerically` |
|      - | 8058 | ` *   indexed array. If the argument is set to TRUE then localtime() returns an associative` |
|      - | 8059 | ` *   array containing all the different elements of the structure returned by the C function` |
|      - | 8060 | ` *   call to localtime. The names of the different keys of the associative array are as follows:` |
|      - | 8061 | ` *      "tm_sec" - seconds, 0 to 59` |
|      - | 8062 | ` *      "tm_min" - minutes, 0 to 59` |
|      - | 8063 | ` *      "tm_hour" - hours, 0 to 23` |
|      - | 8064 | ` *      "tm_mday" - day of the month, 1 to 31` |
|      - | 8065 | ` *      "tm_mon" - month of the year, 0 (Jan) to 11 (Dec)` |
|      - | 8066 | ` *      "tm_year" - years since 1900` |
|      - | 8067 | ` *      "tm_wday" - day of the week, 0 (Sun) to 6 (Sat)` |
|      - | 8068 | ` *      "tm_yday" - day of the year, 0 to 365` |
|      - | 8069 | ` *      "tm_isdst" - is daylight savings time in effect? Positive if yes, 0 if not, negative if unknown.` |
|      - | 8070 | ` * Returns` |
|      - | 8071 | ` *  An associative array of information related to the timestamp.` |
|      - | 8072 | ` */` |
|      8 | 8073 | `static int PH7_builtin_localtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8074 |  |
|      - | 8075 | `	ph7_value *pValue,*pArray;` |
|      9 | 8076 | `	int isAssoc = 0;` |
|      - | 8077 | `	Sytm sTm;` |
|      9 | 8078 | `	if( nArg < 1 ){` |
|      - | 8079 | `#ifdef __WINNT__` |
|      - | 8080 | `		SYSTEMTIME sOS;` |
|      1 | 8081 | `		GetSystemTime(&sOS); /* TODO(chems): GMT not local */` |
|      1 | 8082 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8083 | `#else` |
|      - | 8084 | `		struct tm *pTm;` |
|      - | 8085 | `		time_t t;` |
|      4 | 8086 | `		time(&t);` |
|      4 | 8087 | `		pTm = localtime(&t);` |
|      4 | 8088 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8089 | `#endif` |
|      3 | 8090 | `	}else{` |
|      - | 8091 | `		/* Use the given timestamp */` |
|      - | 8092 | `		time_t t;` |
|      - | 8093 | `		struct tm *pTm;` |
|      5 | 8094 | `		if( ph7_value_is_int(apArg[0]) ){` |
|      5 | 8095 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|      5 | 8096 | `			pTm = localtime(&t);` |
|      5 | 8097 | `			if( pTm == 0 ){` |
|    ! 0 | 8098 | `				time(&t);` |
|    ! 0 | 8099 | `			}` |
|      3 | 8100 | `		}else{` |
|    ! 0 | 8101 | `			time(&t);` |
|      - | 8102 | `		}` |
|      5 | 8103 | `		pTm = localtime(&t);` |
|      5 | 8104 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8105 | `	}` |
|      - | 8106 | `	/* Element value */` |
|      9 | 8107 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      9 | 8108 | `	if( pValue == 0 ){` |
|      - | 8109 | `		/* Return NULL */` |
|    ! 0 | 8110 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8111 | `		return PH7_OK;` |
|      - | 8112 | `	}` |
|      - | 8113 | `	/* Create a new array */` |
|      9 | 8114 | `	pArray = ph7_context_new_array(pCtx);` |
|      9 | 8115 | `	if( pArray == 0 ){` |
|      - | 8116 | `		/* Return NULL */` |
|    ! 0 | 8117 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8118 | `		return PH7_OK;` |
|      - | 8119 | `	}` |
|      9 | 8120 | `	if( nArg > 1 ){` |
|      3 | 8121 | `		isAssoc = ph7_value_to_bool(apArg[1]);` |
|      1 | 8122 | `	}` |
|      - | 8123 | `	/* Fill the array */` |
|      - | 8124 | `	/* Seconds */` |
|      9 | 8125 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|      9 | 8126 | `	if( isAssoc ){` |
|      3 | 8127 | `		ph7_array_add_strkey_elem(pArray,"tm_sec",pValue);` |
|      2 | 8128 | `	}else{` |
|      7 | 8129 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8130 | `	}` |
|      - | 8131 | `	/* Minutes */` |
|      9 | 8132 | `	ph7_value_int(pValue,sTm.tm_min);` |
|      9 | 8133 | `	if( isAssoc ){` |
|      3 | 8134 | `		ph7_array_add_strkey_elem(pArray,"tm_min",pValue);` |
|      2 | 8135 | `	}else{` |
|      7 | 8136 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8137 | `	}` |
|      - | 8138 | `	/* Hours */` |
|      9 | 8139 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|      9 | 8140 | `	if( isAssoc ){` |
|      3 | 8141 | `		ph7_array_add_strkey_elem(pArray,"tm_hour",pValue);` |
|      2 | 8142 | `	}else{` |
|      7 | 8143 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8144 | `	}` |
|      - | 8145 | `	/* mday */` |
|      9 | 8146 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|      9 | 8147 | `	if( isAssoc ){` |
|      3 | 8148 | `		ph7_array_add_strkey_elem(pArray,"tm_mday",pValue);` |
|      2 | 8149 | `	}else{` |
|      7 | 8150 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8151 | `	}` |
|      - | 8152 | `	/* mon */` |
|      9 | 8153 | `	ph7_value_int(pValue,sTm.tm_mon);` |
|      9 | 8154 | `	if( isAssoc ){` |
|      3 | 8155 | `		ph7_array_add_strkey_elem(pArray,"tm_mon",pValue);` |
|      2 | 8156 | `	}else{` |
|      7 | 8157 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8158 | `	}` |
|      - | 8159 | `	/* year since 1900 */` |
|      9 | 8160 | `	ph7_value_int(pValue,sTm.tm_year-1900);` |
|      9 | 8161 | `	if( isAssoc ){` |
|      3 | 8162 | `		ph7_array_add_strkey_elem(pArray,"tm_year",pValue);` |
|      2 | 8163 | `	}else{` |
|      7 | 8164 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8165 | `	}` |
|      - | 8166 | `	/* wday */` |
|      9 | 8167 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|      9 | 8168 | `	if( isAssoc ){` |
|      3 | 8169 | `		ph7_array_add_strkey_elem(pArray,"tm_wday",pValue);` |
|      2 | 8170 | `	}else{` |
|      7 | 8171 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8172 | `	}` |
|      - | 8173 | `	/* yday */` |
|      9 | 8174 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|      9 | 8175 | `	if( isAssoc ){` |
|      3 | 8176 | `		ph7_array_add_strkey_elem(pArray,"tm_yday",pValue);` |
|      2 | 8177 | `	}else{` |
|      7 | 8178 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8179 | `	}` |
|      - | 8180 | `	/* isdst */` |
|      - | 8181 | `#ifdef __WINNT__` |
|      - | 8182 | `#ifdef _MSC_VER` |
|      - | 8183 | `#ifndef _WIN32_WCE` |
|      1 | 8184 | `			_get_daylight(&sTm.tm_isdst);` |
|      - | 8185 | `#endif` |
|      - | 8186 | `#endif` |
|      - | 8187 | `#endif` |
|      9 | 8188 | `	ph7_value_int(pValue,sTm.tm_isdst);` |
|      9 | 8189 | `	if( isAssoc ){` |
|      3 | 8190 | `		ph7_array_add_strkey_elem(pArray,"tm_isdst",pValue);` |
|      2 | 8191 | `	}else{` |
|      7 | 8192 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8193 | `	}` |
|      - | 8194 | `	/* Return the array */` |
|      9 | 8195 | `	ph7_result_value(pCtx,pArray);` |
|      9 | 8196 | `	return PH7_OK;` |
|      5 | 8197 |  |
|      - | 8198 | `/*` |
|      - | 8199 | ` * int idate(string $format [, int $timestamp = time() ])` |
|      - | 8200 | ` *  Returns a number formatted according to the given format string` |
|      - | 8201 | ` *  using the given integer timestamp or the current local time if` |
|      - | 8202 | ` *  no timestamp is given. In other words, timestamp is optional and defaults` |
|      - | 8203 | ` *  to the value of time().` |
|      - | 8204 | ` *  Unlike the function date(), idate() accepts just one char in the format` |
|      - | 8205 | ` *  parameter.` |
|      - | 8206 | ` * $Parameters` |
|      - | 8207 | ` *  Supported format` |
|      - | 8208 | ` *   d 	Day of the month` |
|      - | 8209 | ` *   h 	Hour (12 hour format)` |
|      - | 8210 | ` *   H 	Hour (24 hour format)` |
|      - | 8211 | ` *   i 	Minutes` |
|      - | 8212 | ` *   I (uppercase i)1 if DST is activated, 0 otherwise` |
|      - | 8213 | ` *   L (uppercase l) returns 1 for leap year, 0 otherwise` |
|      - | 8214 | ` *   m 	Month number` |
|      - | 8215 | ` *   s 	Seconds` |
|      - | 8216 | ` *   t 	Days in current month` |
|      - | 8217 | ` *   U 	Seconds since the Unix Epoch - January 1 1970 00:00:00 UTC - this is the same as time()` |
|      - | 8218 | ` *   w 	Day of the week (0 on Sunday)` |
|      - | 8219 | ` *   W 	ISO-8601 week number of year, weeks starting on Monday` |
|      - | 8220 | ` *   y 	Year (1 or 2 digits - check note below)` |
|      - | 8221 | ` *   Y 	Year (4 digits)` |
|      - | 8222 | ` *   z 	Day of the year` |
|      - | 8223 | ` *   Z 	Timezone offset in seconds` |
|      - | 8224 | ` * $timestamp` |
|      - | 8225 | ` *  The optional timestamp parameter is an integer Unix timestamp that defaults` |
|      - | 8226 | ` *  to the current local time if a timestamp is not given. In other words, it defaults` |
|      - | 8227 | ` *  to the value of time().` |
|      - | 8228 | ` * Return` |
|      - | 8229 | ` *  An integer.` |
|      - | 8230 | ` */` |
|     40 | 8231 | `static int PH7_builtin_idate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 8232 |  |
|      - | 8233 | `	const char *zFormat;` |
|     42 | 8234 | `	ph7_int64 iVal = 0;` |
|      - | 8235 | `	int nLen;` |
|      - | 8236 | `	Sytm sTm;` |
|     42 | 8237 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8238 | `		/* Missing/Invalid argument,return -1 */` |
|      5 | 8239 | `		ph7_result_int(pCtx,-1);` |
|      5 | 8240 | `		return PH7_OK;` |
|      - | 8241 | `	}` |
|     42 | 8242 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     42 | 8243 | `	if( nLen < 1 ){` |
|      - | 8244 | `		/* Don't bother processing return -1*/` |
|    ! 0 | 8245 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 8246 | `	}` |
|     42 | 8247 | `	if( nArg < 2 ){` |
|      - | 8248 | `#ifdef __WINNT__` |
|      - | 8249 | `		SYSTEMTIME sOS;` |
|      2 | 8250 | `		GetSystemTime(&sOS);` |
|      2 | 8251 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8252 | `#else` |
|      - | 8253 | `		struct tm *pTm;` |
|      - | 8254 | `		time_t t;` |
|     30 | 8255 | `		time(&t);` |
|     30 | 8256 | `		pTm = localtime(&t);` |
|     30 | 8257 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8258 | `#endif` |
|     18 | 8259 | `	}else{` |
|      - | 8260 | `		/* Use the given timestamp */` |
|      - | 8261 | `		time_t t;` |
|      - | 8262 | `		struct tm *pTm;` |
|     11 | 8263 | `		if( ph7_value_is_int(apArg[1]) ){` |
|     11 | 8264 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|     11 | 8265 | `			pTm = localtime(&t);` |
|     11 | 8266 | `			if( pTm == 0 ){` |
|    ! 0 | 8267 | `				time(&t);` |
|    ! 0 | 8268 | `			}` |
|      6 | 8269 | `		}else{` |
|    ! 0 | 8270 | `			time(&t);` |
|      - | 8271 | `		}` |
|     11 | 8272 | `		pTm = localtime(&t);` |
|     11 | 8273 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8274 | `	}` |
|      - | 8275 | `	/* Perform the requested operation */` |
|     42 | 8276 | `	switch(zFormat[0]){` |
|      2 | 8277 | `	case 'd':` |
|      - | 8278 | `		/* Day of the month */` |
|      5 | 8279 | `		iVal = sTm.tm_mday;` |
|      5 | 8280 | `		break;` |
|    ! 0 | 8281 | `	case 'h':` |
|      - | 8282 | `		/*	Hour (12 hour format)*/` |
|    ! 0 | 8283 | `		iVal = 1 + (sTm.tm_hour % 12);` |
|    ! 0 | 8284 | `		break;` |
|      1 | 8285 | `	case 'H':` |
|      - | 8286 | `		/* Hour (24 hour format)*/` |
|      3 | 8287 | `		iVal = sTm.tm_hour;` |
|      3 | 8288 | `		break;` |
|      1 | 8289 | `	case 'i':` |
|      - | 8290 | `		/*Minutes*/` |
|      3 | 8291 | `		iVal = sTm.tm_min;` |
|      3 | 8292 | `		break;` |
|      1 | 8293 | `	case 'I':` |
|      - | 8294 | `		/*	returns 1 if DST is activated, 0 otherwise */` |
|      - | 8295 | `#ifdef __WINNT__` |
|      - | 8296 | `#ifdef _MSC_VER` |
|      - | 8297 | `#ifndef _WIN32_WCE` |
|      1 | 8298 | `			_get_daylight(&sTm.tm_isdst);` |
|      - | 8299 | `#endif` |
|      - | 8300 | `#endif` |
|      - | 8301 | `#endif` |
|      3 | 8302 | `		iVal = sTm.tm_isdst;` |
|      3 | 8303 | `		break;` |
|      1 | 8304 | `	case 'L':` |
|      - | 8305 | `		/* 	returns 1 for leap year, 0 otherwise */` |
|      3 | 8306 | `		iVal = IS_LEAP_YEAR(sTm.tm_year);` |
|      3 | 8307 | `		break;` |
|      2 | 8308 | `	case 'm':` |
|      - | 8309 | `		/* Month number*/` |
|      5 | 8310 | `		iVal = sTm.tm_mon;` |
|      5 | 8311 | `		break;` |
|      1 | 8312 | `	case 's':` |
|      - | 8313 | `		/*Seconds*/` |
|      3 | 8314 | `		iVal = sTm.tm_sec;` |
|      3 | 8315 | `		break;` |
|      1 | 8316 | `	case 't':{` |
|      - | 8317 | `		/*Days in current month*/` |
|      - | 8318 | `		static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|      5 | 8319 | `		int nDays = aMonDays[sTm.tm_mon % 12 ];` |
|      5 | 8320 | `		if( sTm.tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(sTm.tm_year) ){` |
|      3 | 8321 | `			nDays = 28;` |
|      1 | 8322 | `		}` |
|      7 | 8323 | `		iVal = nDays;` |
|      7 | 8324 | `		break;` |
|      - | 8325 | `			 }` |
|      1 | 8326 | `	case 'U':` |
|      - | 8327 | `		/*Seconds since the Unix Epoch*/` |
|      3 | 8328 | `		iVal = (ph7_int64)time(0);` |
|      3 | 8329 | `		break;` |
|      1 | 8330 | `	case 'w':` |
|      - | 8331 | `		/*	Day of the week (0 on Sunday) */` |
|      3 | 8332 | `		iVal = sTm.tm_wday;` |
|      3 | 8333 | `		break;` |
|      1 | 8334 | `	case 'W': {` |
|      - | 8335 | `		/* ISO-8601 week number of year, weeks starting on Monday */` |
|      - | 8336 | `		static const int aISO8601_local[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|      3 | 8337 | `		iVal = aISO8601_local[sTm.tm_wday % 7 ];` |
|      3 | 8338 | `		break;` |
|      - | 8339 | `			  }` |
|    ! 0 | 8340 | `	case 'y':` |
|      - | 8341 | `		/* Year (2 digits) */` |
|    ! 0 | 8342 | `		iVal = sTm.tm_year % 100;` |
|    ! 0 | 8343 | `		break;` |
|      3 | 8344 | `	case 'Y':` |
|      - | 8345 | `		/* Year (4 digits) */` |
|      7 | 8346 | `		iVal = sTm.tm_year;` |
|      7 | 8347 | `		break;` |
|      1 | 8348 | `	case 'z':` |
|      - | 8349 | `		/* Day of the year */` |
|      3 | 8350 | `		iVal = sTm.tm_yday;` |
|      3 | 8351 | `		break;` |
|      1 | 8352 | `	case 'Z':` |
|      - | 8353 | `		/*Timezone offset in seconds*/` |
|      3 | 8354 | `		iVal = sTm.tm_gmtoff;` |
|      3 | 8355 | `		break;` |
|      1 | 8356 | `	default:` |
|      - | 8357 | `		/* unknown format,throw a warning */` |
|      3 | 8358 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Unknown date format token");` |
|      2 | 8359 | `		break;` |
|      - | 8360 | `	}` |
|      - | 8361 | `	/* Return the time value */` |
|     40 | 8362 | `	ph7_result_int64(pCtx,iVal);` |
|     40 | 8363 | `	return PH7_OK;` |
|     23 | 8364 |  |
|      - | 8365 | `/*` |
|      - | 8366 | ` * int mktime/gmmktime([ int $hour = date("H") [, int $minute = date("i") [, int $second = date("s")` |
|      - | 8367 | ` *  [, int $month = date("n") [, int $day = date("j") [, int $year = date("Y") [, int $is_dst = -1 ]]]]]]] )` |
|      - | 8368 | ` *  Returns the Unix timestamp corresponding to the arguments given. This timestamp is a 64bit integer` |
|      - | 8369 | ` *  containing the number of seconds between the Unix Epoch (January 1 1970 00:00:00 GMT) and the time` |
|      - | 8370 | ` *  specified.` |
|      - | 8371 | ` *  Arguments may be left out in order from right to left; any arguments thus omitted will be set to` |
|      - | 8372 | ` *  the current value according to the local date and time.` |
|      - | 8373 | ` * Parameters` |
|      - | 8374 | ` * $hour` |
|      - | 8375 | ` *  The number of the hour relevant to the start of the day determined by month, day and year.` |
|      - | 8376 | ` *  Negative values reference the hour before midnight of the day in question. Values greater` |
|      - | 8377 | ` *  than 23 reference the appropriate hour in the following day(s).` |
|      - | 8378 | ` * $minute` |
|      - | 8379 | ` *  The number of the minute relevant to the start of the hour. Negative values reference` |
|      - | 8380 | ` *  the minute in the previous hour. Values greater than 59 reference the appropriate minute` |
|      - | 8381 | ` *  in the following hour(s).` |
|      - | 8382 | ` * $second` |
|      - | 8383 | ` *  The number of seconds relevant to the start of the minute. Negative values reference` |
|      - | 8384 | ` *  the second in the previous minute. Values greater than 59 reference the appropriate` |
|      - | 8385 | ` * second in the following minute(s).` |
|      - | 8386 | ` * $month` |
|      - | 8387 | ` *  The number of the month relevant to the end of the previous year. Values 1 to 12 reference` |
|      - | 8388 | ` *  the normal calendar months of the year in question. Values less than 1 (including negative values)` |
|      - | 8389 | ` *  reference the months in the previous year in reverse order, so 0 is December, -1 is November)...` |
|      - | 8390 | ` * $day` |
|      - | 8391 | ` *  The number of the day relevant to the end of the previous month. Values 1 to 28, 29, 30 or 31` |
|      - | 8392 | ` *  (depending upon the month) reference the normal days in the relevant month. Values less than 1` |
|      - | 8393 | ` *  (including negative values) reference the days in the previous month, so 0 is the last day` |
|      - | 8394 | ` *  of the previous month, -1 is the day before that, etc. Values greater than the number of days` |
|      - | 8395 | ` *  in the relevant month reference the appropriate day in the following month(s).` |
|      - | 8396 | ` * $year` |
|      - | 8397 | ` *  The number of the year, may be a two or four digit value, with values between 0-69 mapping` |
|      - | 8398 | ` *  to 2000-2069 and 70-100 to 1970-2000. On systems where time_t is a 32bit signed integer, as` |
|      - | 8399 | ` *  most common today, the valid range for year is somewhere between 1901 and 2038.` |
|      - | 8400 | ` * $is_dst` |
|      - | 8401 | ` *  This parameter can be set to 1 if the time is during daylight savings time (DST), 0 if it is not,` |
|      - | 8402 | ` *  or -1 (the default) if it is unknown whether the time is within daylight savings time or not.` |
|      - | 8403 | ` * Return` |
|      - | 8404 | ` *   mktime() returns the Unix timestamp of the arguments given.` |
|      - | 8405 | ` *   If the arguments are invalid, the function returns FALSE` |
|      - | 8406 | ` */` |
|      8 | 8407 | `static int PH7_builtin_mktime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8408 |  |
|      - | 8409 | `	const char *zFunction;` |
|      9 | 8410 | `	ph7_int64 iVal = 0;` |
|      - | 8411 | `	struct tm *pTm;` |
|      - | 8412 | `	time_t t;` |
|      - | 8413 | `	/* Extract function name */` |
|      9 | 8414 | `	zFunction = ph7_function_name(pCtx);` |
|      - | 8415 | `	/* Get the current time */` |
|      9 | 8416 | `	time(&t);` |
|      9 | 8417 | `	if( zFunction[0] == 'g' /* gmmktime */ ){` |
|      3 | 8418 | `		pTm = gmtime(&t);` |
|      2 | 8419 | `	}else{` |
|      - | 8420 | `		/* localtime */` |
|      7 | 8421 | `		pTm = localtime(&t);` |
|      - | 8422 | `	}` |
|      9 | 8423 | `	if( nArg > 0 ){` |
|      - | 8424 | `		int iTmp;` |
|      - | 8425 | `		/* Hour */` |
|      9 | 8426 | `		iTmp = ph7_value_to_int(apArg[0]);` |
|      9 | 8427 | `		pTm->tm_hour = iTmp;` |
|      9 | 8428 | `		if( nArg > 1 ){` |
|      - | 8429 | `			/* Minutes */` |
|      9 | 8430 | `			iTmp = ph7_value_to_int(apArg[1]);` |
|      9 | 8431 | `			pTm->tm_min = iTmp;` |
|      9 | 8432 | `			if( nArg > 2 ){` |
|      - | 8433 | `				/* Seconds */` |
|      9 | 8434 | `				iTmp = ph7_value_to_int(apArg[2]);` |
|      9 | 8435 | `				pTm->tm_sec = iTmp;` |
|      9 | 8436 | `				if( nArg > 3 ){` |
|      - | 8437 | `					/* Month */` |
|      9 | 8438 | `					iTmp = ph7_value_to_int(apArg[3]);` |
|      9 | 8439 | `					pTm->tm_mon = iTmp - 1;` |
|      9 | 8440 | `					if( nArg > 4 ){` |
|      - | 8441 | `						/* mday */` |
|      9 | 8442 | `						iTmp = ph7_value_to_int(apArg[4]);` |
|      9 | 8443 | `						pTm->tm_mday = iTmp;` |
|      9 | 8444 | `						if( nArg > 5 ){` |
|      - | 8445 | `							/* Year */` |
|      9 | 8446 | `							iTmp = ph7_value_to_int(apArg[5]);` |
|      9 | 8447 | `							if( iTmp > 1900 ){` |
|      9 | 8448 | `								iTmp -= 1900;` |
|      4 | 8449 | `							}` |
|      9 | 8450 | `							pTm->tm_year = iTmp;` |
|      9 | 8451 | `							if( nArg > 6 ){` |
|      - | 8452 | `								/* is_dst */` |
|    ! 0 | 8453 | `								iTmp = ph7_value_to_bool(apArg[6]);` |
|    ! 0 | 8454 | `								pTm->tm_isdst = iTmp;` |
|    ! 0 | 8455 | `							}` |
|      4 | 8456 | `						}` |
|      4 | 8457 | `					}` |
|      4 | 8458 | `				}` |
|      4 | 8459 | `			}` |
|      4 | 8460 | `		}` |
|      4 | 8461 | `	}` |
|      - | 8462 | `	/* Make the time */` |
|      9 | 8463 | `	iVal = (ph7_int64)mktime(pTm);` |
|      - | 8464 | `	/* Return the timesatmp as a 64bit integer */` |
|      9 | 8465 | `	ph7_result_int64(pCtx,iVal);` |
|      9 | 8466 | `	return PH7_OK;` |
|      1 | 8467 |  |
|      - | 8468 | `/*` |
|      - | 8469 | ` * Section:` |
|      - | 8470 | ` *    URL handling Functions.` |
|      - | 8471 | ` * Status:` |
|      - | 8472 | ` *    Stable.` |
|      - | 8473 | ` */` |
|      - | 8474 | `/*` |
|      - | 8475 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 8476 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 8477 | ` */` |
|   1026 | 8478 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 8479 |  |
|      - | 8480 | `	/* Store in the call context result buffer */` |
|   1028 | 8481 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 8482 | `	return SXRET_OK;` |
|      2 | 8483 |  |
|      - | 8484 | `/*` |
|      - | 8485 | ` * string base64_encode(string $data)` |
|      - | 8486 | ` * string convert_uuencode(string $data)` |
|      - | 8487 | ` *  Encodes data with MIME base64` |
|      - | 8488 | ` * Parameter` |
|      - | 8489 | ` *  $data` |
|      - | 8490 | ` *    Data to encode` |
|      - | 8491 | ` * Return` |
|      - | 8492 | ` *  Encoded data or FALSE on failure.` |
|      - | 8493 | ` */` |
|     10 | 8494 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8495 |  |
|      - | 8496 | `	const char *zIn;` |
|      - | 8497 | `	int nLen;` |
|     11 | 8498 | `	if( nArg < 1 ){` |
|      - | 8499 | `		/* Missing arguments,return FALSE */` |
|      5 | 8500 | `		ph7_result_bool(pCtx,0);` |
|      5 | 8501 | `		return PH7_OK;` |
|      - | 8502 | `	}` |
|      - | 8503 | `	/* Extract the input string */` |
|      7 | 8504 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8505 | `	if( nLen < 1 ){` |
|      - | 8506 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8507 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8508 | `		return PH7_OK;` |
|      - | 8509 | `	}` |
|      - | 8510 | `	/* Perform the BASE64 encoding */` |
|      7 | 8511 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 8512 | `	return PH7_OK;` |
|      6 | 8513 |  |
|      - | 8514 | `/*` |
|      - | 8515 | ` * string base64_decode(string $data)` |
|      - | 8516 | ` * string convert_uudecode(string $data)` |
|      - | 8517 | ` *  Decodes data encoded with MIME base64` |
|      - | 8518 | ` * Parameter` |
|      - | 8519 | ` *  $data` |
|      - | 8520 | ` *    Encoded data.` |
|      - | 8521 | ` * Return` |
|      - | 8522 | ` *  Returns the original data or FALSE on failure.` |
|      - | 8523 | ` */` |
|     36 | 8524 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 8525 |  |
|      - | 8526 | `	const char *zIn;` |
|      - | 8527 | `	int nLen;` |
|     38 | 8528 | `	if( nArg < 1 ){` |
|      - | 8529 | `		/* Missing arguments,return FALSE */` |
|      3 | 8530 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8531 | `		return PH7_OK;` |
|      - | 8532 | `	}` |
|      - | 8533 | `	/* Extract the input string */` |
|     36 | 8534 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 8535 | `	if( nLen < 1 ){` |
|      - | 8536 | `		/* Nothing to process,return FALSE */` |
|      3 | 8537 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8538 | `		return PH7_OK;` |
|      - | 8539 | `	}` |
|      - | 8540 | `	/* Perform the BASE64 decoding */` |
|     34 | 8541 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 8542 | `	return PH7_OK;` |
|     20 | 8543 |  |
|      - | 8544 | `/*` |
|      - | 8545 | ` * string urlencode(string $str)` |
|      - | 8546 | ` *  URL encoding` |
|      - | 8547 | ` * Parameter` |
|      - | 8548 | ` *  $data` |
|      - | 8549 | ` *   Input string.` |
|      - | 8550 | ` * Return` |
|      - | 8551 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 8552 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 8553 | ` *  encoded as plus (+) signs.` |
|      - | 8554 | ` */` |
|      6 | 8555 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8556 |  |
|      - | 8557 | `	const char *zIn;` |
|      - | 8558 | `	int nLen;` |
|      7 | 8559 | `	if( nArg < 1 ){` |
|      - | 8560 | `		/* Missing arguments,return FALSE */` |
|      3 | 8561 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8562 | `		return PH7_OK;` |
|      - | 8563 | `	}` |
|      - | 8564 | `	/* Extract the input string */` |
|      5 | 8565 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 8566 | `	if( nLen < 1 ){` |
|      - | 8567 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8568 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8569 | `		return PH7_OK;` |
|      - | 8570 | `	}` |
|      - | 8571 | `	/* Perform the URL encoding */` |
|      5 | 8572 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 8573 | `	return PH7_OK;` |
|      4 | 8574 |  |
|      - | 8575 | `/*` |
|      - | 8576 | ` * string urldecode(string $str)` |
|      - | 8577 | ` *  Decodes any %## encoding in the given string.` |
|      - | 8578 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 8579 | ` * Parameter` |
|      - | 8580 | ` *  $data` |
|      - | 8581 | ` *    Input string.` |
|      - | 8582 | ` * Return` |
|      - | 8583 | ` *  Decoded URL or FALSE on failure.` |
|      - | 8584 | ` */` |
|      8 | 8585 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8586 |  |
|      - | 8587 | `	const char *zIn;` |
|      - | 8588 | `	int nLen;` |
|      9 | 8589 | `	if( nArg < 1 ){` |
|      - | 8590 | `		/* Missing arguments,return FALSE */` |
|      3 | 8591 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8592 | `		return PH7_OK;` |
|      - | 8593 | `	}` |
|      - | 8594 | `	/* Extract the input string */` |
|      7 | 8595 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8596 | `	if( nLen < 1 ){` |
|      - | 8597 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8598 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8599 | `		return PH7_OK;` |
|      - | 8600 | `	}` |
|      - | 8601 | `	/* Perform the URL decoding */` |
|      7 | 8602 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 8603 | `	return PH7_OK;` |
|      5 | 8604 |  |
|      - | 8605 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8606 | `/* Table of the built-in functions */` |
|      - | 8607 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 8608 | `	   /* Variable handling functions */` |
|      - | 8609 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 8610 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 8611 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 8612 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 8613 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 8614 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 8615 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 8616 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 8617 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 8618 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 8619 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 8620 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 8621 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 8622 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 8623 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 8624 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 8625 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 8626 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 8627 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 8628 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8629 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 8630 | `	   /* Math functions */` |
|      - | 8631 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 8632 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 8633 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 8634 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 8635 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 8636 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 8637 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 8638 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 8639 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 8640 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 8641 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 8642 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 8643 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 8644 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 8645 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 8646 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 8647 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 8648 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 8649 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 8650 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 8651 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 8652 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 8653 | `	{ "round",    PH7_builtin_round        },` |
|      - | 8654 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 8655 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 8656 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 8657 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 8658 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 8659 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 8660 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 8661 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 8662 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8663 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8664 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 8665 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8666 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8667 | `	   /* String handling functions */` |
|      - | 8668 |  |
|      - | 8669 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 8670 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 8671 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 8672 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 8673 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 8674 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 8675 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 8676 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 8677 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 8678 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 8679 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 8680 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 8681 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 8682 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 8683 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 8684 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 8685 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 8686 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 8687 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 8688 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 8689 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 8690 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 8691 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 8692 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 8693 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 8694 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 8695 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 8696 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 8697 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 8698 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 8699 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 8700 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 8701 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 8702 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 8703 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 8704 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 8705 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 8706 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 8707 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 8708 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 8709 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 8710 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 8711 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 8712 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 8713 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 8714 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 8715 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 8716 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 8717 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 8718 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8719 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8720 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 8721 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 8722 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 8723 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 8724 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8725 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8726 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 8727 |  |
|      - | 8728 |  |
|      - | 8729 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 8730 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 8731 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 8732 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 8733 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 8734 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8735 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8736 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 8737 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 8738 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8739 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8740 |  |
|      - | 8741 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 8742 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 8743 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 8744 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 8745 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 8746 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 8747 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 8748 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 8749 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 8750 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 8751 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 8752 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 8753 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8754 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8755 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 8756 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8757 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8758 |  |
|      - | 8759 | `	         /* Ctype functions */` |
|      - | 8760 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 8761 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 8762 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 8763 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 8764 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 8765 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 8766 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 8767 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 8768 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 8769 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 8770 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 8771 | `	         /* Time functions */` |
|      - | 8772 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 8773 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 8774 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 8775 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 8776 | `	{ "date",        PH7_builtin_date         },` |
|      - | 8777 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 8778 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 8779 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 8780 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 8781 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 8782 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 8783 | `	        /* URL functions */` |
|      - | 8784 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 8785 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 8786 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 8787 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 8788 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 8789 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 8790 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 8791 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 8792 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8793 | `};` |
|      - | 8794 | `/*` |
|      - | 8795 | ` * Register the built-in functions defined above,the array functions` |
|      - | 8796 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 8797 | ` */` |
|    944 | 8798 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      2 | 8799 |  |
|      - | 8800 | `	sxu32 n;` |
| 144434 | 8801 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 143490 | 8802 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
|  71746 | 8803 | `	}` |
|      - | 8804 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|    946 | 8805 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 8806 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|    946 | 8807 | `	PH7_RegisterIORoutine(&(*pVm));` |
|    946 | 8808 |  |
|      - | 8809 |  |
