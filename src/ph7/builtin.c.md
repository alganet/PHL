# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3781/4250 lines (88.96%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `/* filter_var(FILTER_VALIDATE_FLOAT) parses with libc strtod for overflow/underflow` |
|      - |    8 | ` * detection + a correctly-rounded value; SyStrToReal clamps the exponent and saturates,` |
|      - |    9 | ` * so it cannot reject out-of-range magnitudes. Same rationale as builtin_math.c's round(). */` |
|      - |   10 | `#include <stdlib.h>  /* strtod */` |
|      - |   11 | `#include <math.h>    /* HUGE_VAL */` |
|      - |   12 | `#include <errno.h>   /* ERANGE (strtod range-error signal) */` |
|      - |   13 | `/* This file implement built-in 'foreign' functions for the PH7 engine */` |
|      - |   14 | `/*` |
|      - |   15 | ` * Section:` |
|      - |   16 | ` *    Variable handling Functions.` |
|      - |   17 | ` * Status:` |
|      - |   18 | ` *    Stable.` |
|      - |   19 | ` */` |
|      - |   20 | `/*` |
|      - |   21 | ` * bool is_bool($var)` |
|      - |   22 | ` *  Finds out whether a variable is a boolean.` |
|      - |   23 | ` * Parameters` |
|      - |   24 | ` *   $var: The variable being evaluated.` |
|      - |   25 | ` * Return` |
|      - |   26 | ` *  TRUE if var is a boolean. False otherwise.` |
|      - |   27 | ` */` |
|     32 |   28 | `static int PH7_builtin_is_bool(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   29 | `{` |
|     33 |   30 | `	int res = 0; /* Assume false by default */` |
|     33 |   31 | `	if( nArg > 0 ){` |
|     29 |   32 | `		res = ph7_value_is_bool(apArg[0]);` |
|     14 |   33 | `	}` |
|      - |   34 | `	/* Query result */` |
|     33 |   35 | `	ph7_result_bool(pCtx,res);` |
|     33 |   36 | `	return PH7_OK;` |
|      1 |   37 | `}` |
|      - |   38 | `/*` |
|      - |   39 | ` * bool is_float($var)` |
|      - |   40 | ` * bool is_real($var)` |
|      - |   41 | ` * bool is_double($var)` |
|      - |   42 | ` *  Finds out whether a variable is a float.` |
|      - |   43 | ` * Parameters` |
|      - |   44 | ` *   $var: The variable being evaluated.` |
|      - |   45 | ` * Return` |
|      - |   46 | ` *  TRUE if var is a float. False otherwise.` |
|      - |   47 | ` */` |
|    210 |   48 | `static int PH7_builtin_is_float(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   49 | `{` |
|    211 |   50 | `	int res = 0; /* Assume false by default */` |
|    211 |   51 | `	if( nArg > 0 ){` |
|    209 |   52 | `		res = ph7_value_is_float(apArg[0]);` |
|    104 |   53 | `	}` |
|      - |   54 | `	/* Query result */` |
|    211 |   55 | `	ph7_result_bool(pCtx,res);` |
|    211 |   56 | `	return PH7_OK;` |
|      1 |   57 | `}` |
|      - |   58 | `/*` |
|      - |   59 | ` * bool is_int($var)` |
|      - |   60 | ` * bool is_integer($var)` |
|      - |   61 | ` * bool is_long($var)` |
|      - |   62 | ` *  Finds out whether a variable is an integer.` |
|      - |   63 | ` * Parameters` |
|      - |   64 | ` *   $var: The variable being evaluated.` |
|      - |   65 | ` * Return` |
|      - |   66 | ` *  TRUE if var is an integer. False otherwise.` |
|      - |   67 | ` */` |
|    632 |   68 | `static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |   69 | `{` |
|    634 |   70 | `	int res = 0; /* Assume false by default */` |
|    634 |   71 | `	if( nArg > 0 ){` |
|      - |   72 | `		/* Strict PHP identity: a float is never an int, even when it holds an` |
|      - |   73 | `		 * integer value (1.0). An integer-valued real carries both MEMOBJ_INT` |
|      - |   74 | `		 * (cached) and MEMOBJ_REAL, so REAL must be excluded here. */` |
|    632 |   75 | `		res = ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]);` |
|    315 |   76 | `	}` |
|      - |   77 | `	/* Query result */` |
|    634 |   78 | `	ph7_result_bool(pCtx,res);` |
|    634 |   79 | `	return PH7_OK;` |
|      2 |   80 | `}` |
|      - |   81 | `/*` |
|      - |   82 | ` * bool is_string($var)` |
|      - |   83 | ` *  Finds out whether a variable is a string.` |
|      - |   84 | ` * Parameters` |
|      - |   85 | ` *   $var: The variable being evaluated.` |
|      - |   86 | ` * Return` |
|      - |   87 | ` *  TRUE if var is string. False otherwise.` |
|      - |   88 | ` */` |
|    126 |   89 | `static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   90 | `{` |
|    127 |   91 | `	int res = 0; /* Assume false by default */` |
|    127 |   92 | `	if( nArg > 0 ){` |
|    125 |   93 | `		res = ph7_value_is_string(apArg[0]);` |
|     62 |   94 | `	}` |
|      - |   95 | `	/* Query result */` |
|    127 |   96 | `	ph7_result_bool(pCtx,res);` |
|    127 |   97 | `	return PH7_OK;` |
|      1 |   98 | `}` |
|      - |   99 | `/*` |
|      - |  100 | ` * bool is_null($var)` |
|      - |  101 | ` *  Finds out whether a variable is NULL.` |
|      - |  102 | ` * Parameters` |
|      - |  103 | ` *   $var: The variable being evaluated.` |
|      - |  104 | ` * Return` |
|      - |  105 | ` *  TRUE if var is NULL. False otherwise.` |
|      - |  106 | ` */` |
|     92 |  107 | `static int PH7_builtin_is_null(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  108 | `{` |
|     96 |  109 | `	int res = 0; /* Assume false by default */` |
|     96 |  110 | `	if( nArg > 0 ){` |
|     94 |  111 | `		res = ph7_value_is_null(apArg[0]);` |
|     45 |  112 | `	}` |
|      - |  113 | `	/* Query result */` |
|     96 |  114 | `	ph7_result_bool(pCtx,res);` |
|     96 |  115 | `	return PH7_OK;` |
|      4 |  116 | `}` |
|      - |  117 | `/*` |
|      - |  118 | ` * bool is_numeric($var)` |
|      - |  119 | ` *  Find out whether a variable is NULL.` |
|      - |  120 | ` * Parameters` |
|      - |  121 | ` *  $var: The variable being evaluated.` |
|      - |  122 | ` * Return` |
|      - |  123 | ` *  True if var is numeric. False otherwise.` |
|      - |  124 | ` */` |
|     38 |  125 | `static int PH7_builtin_is_numeric(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  126 | `{` |
|     43 |  127 | `	int res = 0; /* Assume false by default */` |
|     43 |  128 | `	if( nArg > 0 ){` |
|     41 |  129 | `		res = ph7_value_is_numeric(apArg[0]);` |
|     18 |  130 | `	}` |
|      - |  131 | `	/* Query result */` |
|     43 |  132 | `	ph7_result_bool(pCtx,res);` |
|     43 |  133 | `	return PH7_OK;` |
|      5 |  134 | `}` |
|      - |  135 | `/*` |
|      - |  136 | ` * bool is_scalar($var)` |
|      - |  137 | ` *  Find out whether a variable is a scalar.` |
|      - |  138 | ` * Parameters` |
|      - |  139 | ` *  $var: The variable being evaluated.` |
|      - |  140 | ` * Return` |
|      - |  141 | ` *  True if var is scalar. False otherwise.` |
|      - |  142 | ` */` |
|     14 |  143 | `static int PH7_builtin_is_scalar(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  144 | `{` |
|     15 |  145 | `	int res = 0; /* Assume false by default */` |
|     15 |  146 | `	if( nArg > 0 ){` |
|     13 |  147 | `		res = ph7_value_is_scalar(apArg[0]);` |
|      6 |  148 | `	}` |
|      - |  149 | `	/* Query result */` |
|     15 |  150 | `	ph7_result_bool(pCtx,res);` |
|     15 |  151 | `	return PH7_OK;` |
|      1 |  152 | `}` |
|      - |  153 | `/*` |
|      - |  154 | ` * bool is_array($var)` |
|      - |  155 | ` *  Find out whether a variable is an array.` |
|      - |  156 | ` * Parameters` |
|      - |  157 | ` *  $var: The variable being evaluated.` |
|      - |  158 | ` * Return` |
|      - |  159 | ` *  True if var is an array. False otherwise.` |
|      - |  160 | ` */` |
|    242 |  161 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  162 | `{` |
|    247 |  163 | `	int res = 0; /* Assume false by default */` |
|    247 |  164 | `	if( nArg > 0 ){` |
|    245 |  165 | `		res = ph7_value_is_array(apArg[0]);` |
|    120 |  166 | `	}` |
|      - |  167 | `	/* Query result */` |
|    247 |  168 | `	ph7_result_bool(pCtx,res);` |
|    247 |  169 | `	return PH7_OK;` |
|      5 |  170 | `}` |
|      - |  171 | `/*` |
|      - |  172 | ` * bool is_object($var)` |
|      - |  173 | ` *  Find out whether a variable is an object.` |
|      - |  174 | ` * Parameters` |
|      - |  175 | ` *  $var: The variable being evaluated.` |
|      - |  176 | ` * Return` |
|      - |  177 | ` *  True if var is an object. False otherwise.` |
|      - |  178 | ` */` |
|     22 |  179 | `static int PH7_builtin_is_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  180 | `{` |
|     23 |  181 | `	int res = 0; /* Assume false by default */` |
|     23 |  182 | `	if( nArg > 0 ){` |
|     21 |  183 | `		res = ph7_value_is_object(apArg[0]);` |
|     10 |  184 | `	}` |
|      - |  185 | `	/* Query result */` |
|     23 |  186 | `	ph7_result_bool(pCtx,res);` |
|     23 |  187 | `	return PH7_OK;` |
|      1 |  188 | `}` |
|      - |  189 | `/*` |
|      - |  190 | ` * bool is_resource($var)` |
|      - |  191 | ` *  Find out whether a variable is a resource.` |
|      - |  192 | ` * Parameters` |
|      - |  193 | ` *  $var: The variable being evaluated.` |
|      - |  194 | ` * Return` |
|      - |  195 | ` *  True if a resource. False otherwise.` |
|      - |  196 | ` */` |
|     60 |  197 | `static int PH7_builtin_is_resource(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  198 | `{` |
|     63 |  199 | `	int res = 0; /* Assume false by default */` |
|     63 |  200 | `	if( nArg > 0 ){` |
|     61 |  201 | `		res = ph7_value_is_resource(apArg[0]);` |
|     29 |  202 | `	}` |
|     63 |  203 | `	ph7_result_bool(pCtx,res);` |
|     63 |  204 | `	return PH7_OK;` |
|      3 |  205 | `}` |
|      - |  206 | `/*` |
|      - |  207 | ` * float floatval($var)` |
|      - |  208 | ` *  Get float value of a variable.` |
|      - |  209 | ` * Parameter` |
|      - |  210 | ` *  $var: The variable being processed.` |
|      - |  211 | ` * Return` |
|      - |  212 | ` *  the float value of a variable.` |
|      - |  213 | ` */` |
|      6 |  214 | `static int PH7_builtin_floatval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  215 | `{` |
|      7 |  216 | `	if( nArg < 1 ){` |
|      - |  217 | `		/* return 0.0 */` |
|      3 |  218 | `		ph7_result_double(pCtx,0);` |
|      2 |  219 | `	}else{` |
|      - |  220 | `		double dval;` |
|      - |  221 | `		/* Perform the cast */` |
|      5 |  222 | `		dval = ph7_value_to_double(apArg[0]);` |
|      5 |  223 | `		ph7_result_double(pCtx,dval);` |
|      - |  224 | `	}` |
|      7 |  225 | `	return PH7_OK;` |
|      1 |  226 | `}` |
|      - |  227 | `/*` |
|      - |  228 | ` * int intval($var)` |
|      - |  229 | ` *  Get integer value of a variable.` |
|      - |  230 | ` * Parameter` |
|      - |  231 | ` *  $var: The variable being processed.` |
|      - |  232 | ` * Return` |
|      - |  233 | ` *  the int value of a variable.` |
|      - |  234 | ` */` |
|     26 |  235 | `static int PH7_builtin_intval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  236 | `{` |
|     27 |  237 | `	if( nArg < 1 ){` |
|      - |  238 | `		/* return 0 */` |
|      3 |  239 | `		ph7_result_int(pCtx,0);` |
|      2 |  240 | `	}else{` |
|      - |  241 | `		sxi64 iVal;` |
|      - |  242 | `		/* Perform the cast */` |
|     25 |  243 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|     25 |  244 | `		ph7_result_int64(pCtx,iVal);` |
|      - |  245 | `	}` |
|     27 |  246 | `	return PH7_OK;` |
|      1 |  247 | `}` |
|      - |  248 | `/*` |
|      - |  249 | ` * string strval($var)` |
|      - |  250 | ` *  Get the string representation of a variable.` |
|      - |  251 | ` * Parameter` |
|      - |  252 | ` *  $var: The variable being processed.` |
|      - |  253 | ` * Return` |
|      - |  254 | ` *  the string value of a variable.` |
|      - |  255 | ` */` |
|      4 |  256 | `static int PH7_builtin_strval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  257 | `{` |
|      5 |  258 | `	if( nArg < 1 ){` |
|      - |  259 | `		/* return NULL */` |
|      3 |  260 | `		ph7_result_null(pCtx);` |
|      2 |  261 | `	}else{` |
|      - |  262 | `		const char *zVal;` |
|      3 |  263 | `		int iLen = 0; /* cc -O6 warning */` |
|      - |  264 | `		/* Perform the cast */` |
|      3 |  265 | `		zVal = ph7_value_to_string(apArg[0],&iLen);` |
|      3 |  266 | `		ph7_result_string(pCtx,zVal,iLen);` |
|      - |  267 | `	}` |
|      5 |  268 | `	return PH7_OK;` |
|      1 |  269 | `}` |
|      - |  270 | `/*` |
|      - |  271 | ` * bool boolval($var)` |
|      - |  272 | ` *  Get the boolean value of a variable.` |
|      - |  273 | ` * Parameter` |
|      - |  274 | ` *  $var: The variable being processed.` |
|      - |  275 | ` * Return` |
|      - |  276 | ` *  the bool value of a variable.` |
|      - |  277 | ` */` |
|     16 |  278 | `static int PH7_builtin_boolval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  279 | `{` |
|      - |  280 | `	int bVal;` |
|     18 |  281 | `	if( nArg != 1 ){` |
|      4 |  282 | `		return PH7_VmThrowException(pCtx,` |
|      - |  283 | `			"ArgumentCountError",` |
|      - |  284 | `			"boolval() expects exactly 1 argument, %d given",` |
|      1 |  285 | `			nArg` |
|      - |  286 | `			);` |
|      - |  287 | `	}` |
|      - |  288 | `	/* Perform the cast */` |
|     15 |  289 | `	bVal = ph7_value_to_bool(apArg[0]);` |
|     15 |  290 | `	ph7_result_bool(pCtx,bVal);` |
|     15 |  291 | `	return PH7_OK;` |
|     10 |  292 | `}` |
|      - |  293 | `/*` |
|      - |  294 | ` * bool empty($var)` |
|      - |  295 | ` *  Determine whether a variable is empty.` |
|      - |  296 | ` * Parameters` |
|      - |  297 | ` *   $var: The variable being checked.` |
|      - |  298 | ` * Return` |
|      - |  299 | ` *  0 if var has a non-empty and non-zero value.1 otherwise.` |
|      - |  300 | ` */` |
|  27692 |  301 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  302 | `{` |
|  27697 |  303 | `	int res = 1; /* Assume empty by default */` |
|  27697 |  304 | `	if( nArg > 0 ){` |
|  27695 |  305 | `		res = ph7_value_is_empty(apArg[0]);` |
|  13845 |  306 | `	}` |
|  27697 |  307 | `	ph7_result_bool(pCtx,res);` |
|  27697 |  308 | `	return PH7_OK;` |
|      - |  309 |  |
|      5 |  310 | `}` |
|      - |  311 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |  312 | `#define PH7_NEED_BUILTIN_REG 1` |
|      - |  313 | `#endif` |
|      - |  314 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - |  315 | `#define PH7_NEED_FMT_AND_INI 1` |
|      - |  316 | `#endif` |
|      - |  317 |  |
|      - |  318 | `/* Math functions moved to builtin_math.c */` |
|      - |  319 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - |  320 | `/*` |
|      - |  321 | ` * Section:` |
|      - |  322 | ` *    String handling Functions.` |
|      - |  323 | ` * Status:` |
|      - |  324 | ` *    Stable.` |
|      - |  325 | ` */` |
|      - |  326 | `/*` |
|      - |  327 | ` * string substr(string $string,int $start[, int $length ])` |
|      - |  328 | ` *  Return part of a string.` |
|      - |  329 | ` * Parameters` |
|      - |  330 | ` *  $string` |
|      - |  331 | ` *   The input string. Must be one character or longer.` |
|      - |  332 | ` * $start` |
|      - |  333 | ` *   If start is non-negative, the returned string will start at the start'th position` |
|      - |  334 | ` *   in string, counting from zero. For instance, in the string 'abcdef', the character` |
|      - |  335 | ` *   at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - |  336 | ` *   If start is negative, the returned string will start at the start'th character` |
|      - |  337 | ` *   from the end of string.` |
|      - |  338 | ` *   If string is less than or equal to start characters long, FALSE will be returned.` |
|      - |  339 | ` * $length` |
|      - |  340 | ` *   If length is given and is positive, the string returned will contain at most length` |
|      - |  341 | ` *   characters beginning from start (depending on the length of string).` |
|      - |  342 | ` *   If length is given and is negative, then that many characters will be omitted from` |
|      - |  343 | ` *   the end of string (after the start position has been calculated when a start is negative).` |
|      - |  344 | ` *   If start denotes the position of this truncation or beyond, false will be returned.` |
|      - |  345 | ` *   If length is given and is 0, FALSE or NULL an empty string will be returned.` |
|      - |  346 | ` *   If length is omitted, the substring starting from start until the end of the string` |
|      - |  347 | ` *   will be returned.` |
|      - |  348 | ` * Return` |
|      - |  349 | ` *  Returns the extracted part of string, or FALSE on failure or an empty string.` |
|      - |  350 | ` */` |
| 210452 |  351 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  352 | `{` |
|      - |  353 | `	const char *zSource,*zOfft;` |
|      - |  354 | `	int nOfft,nLen,nSrcLen;` |
| 210457 |  355 | `	if( nArg < 2 ){` |
|      - |  356 | `		/* return FALSE */` |
|      5 |  357 | `		ph7_result_bool(pCtx,0);` |
|      5 |  358 | `		return PH7_OK;` |
|      - |  359 | `	}` |
|      - |  360 | `	/* Extract the target string */` |
| 210453 |  361 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 210453 |  362 | `	if( nSrcLen < 1 ){` |
|      - |  363 | `		/* Empty string,return FALSE */` |
|  11871 |  364 | `		ph7_result_bool(pCtx,0);` |
|  11871 |  365 | `		return PH7_OK;` |
|      - |  366 | `	}` |
| 198587 |  367 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  368 | `	/* Extract the offset */` |
| 198587 |  369 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 198587 |  370 | `	if( nOfft < 0 ){` |
|  32187 |  371 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  32187 |  372 | `		if( zOfft < zSource ){` |
|      - |  373 | `			/* Invalid offset */` |
|      5 |  374 | `			ph7_result_bool(pCtx,0);` |
|      5 |  375 | `			return PH7_OK;` |
|      - |  376 | `		}` |
|  32183 |  377 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  32183 |  378 | `		nOfft = (int)(zOfft-zSource);` |
| 182494 |  379 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  380 | `		/* Invalid offset */` |
|    187 |  381 | `		ph7_result_bool(pCtx,0);` |
|    187 |  382 | `		return PH7_OK;` |
|    ! 0 |  383 | `	}else{` |
| 166223 |  384 | `		zOfft = &zSource[nOfft];` |
| 166223 |  385 | `		nLen = nSrcLen - nOfft;` |
|      - |  386 | `	}` |
| 198401 |  387 | `	if( nArg > 2 ){` |
|      - |  388 | `		/* Extract the length */` |
| 163485 |  389 | `		nLen = ph7_value_to_int(apArg[2]);` |
| 163485 |  390 | `		if( nLen == 0 ){` |
|      - |  391 | `			/* Invalid length,return an empty string */` |
|      5 |  392 | `			ph7_result_string(pCtx,"",0);` |
|      5 |  393 | `			return PH7_OK;` |
| 163481 |  394 | `		}else if( nLen < 0 ){` |
|  32175 |  395 | `			nLen = nSrcLen + nLen - nOfft;` |
|  32175 |  396 | `			if( nLen < 1 ){` |
|      - |  397 | `				/* Invalid  length */` |
|      3 |  398 | `				nLen = nSrcLen - nOfft;` |
|      1 |  399 | `			}` |
|  16085 |  400 | `		}` |
| 163481 |  401 | `		if( nLen + nOfft > nSrcLen ){` |
|      - |  402 | `			/* Invalid length */` |
|   4873 |  403 | `			nLen = nSrcLen - nOfft;` |
|   2434 |  404 | `		}` |
|  81738 |  405 | `	}` |
|      - |  406 | `	/* Return the substring */` |
| 198397 |  407 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 198397 |  408 | `	return PH7_OK;` |
| 105231 |  409 | `}` |
|      - |  410 | `/*` |
|      - |  411 | ` * int substr_compare(string $main_str,string $str ,int $offset[,int $length[,bool $case_insensitivity = false ]])` |
|      - |  412 | ` *  Binary safe comparison of two strings from an offset, up to length characters.` |
|      - |  413 | ` * Parameters` |
|      - |  414 | ` *  $main_str` |
|      - |  415 | ` *  The main string being compared.` |
|      - |  416 | ` *  $str` |
|      - |  417 | ` *   The secondary string being compared.` |
|      - |  418 | ` * $offset` |
|      - |  419 | ` *  The start position for the comparison. If negative, it starts counting from` |
|      - |  420 | ` *  the end of the string.` |
|      - |  421 | ` * $length` |
|      - |  422 | ` *  The length of the comparison. The default value is the largest of the length` |
|      - |  423 | ` *  of the str compared to the length of main_str less the offset.` |
|      - |  424 | ` * $case_insensitivity` |
|      - |  425 | ` *  If case_insensitivity is TRUE, comparison is case insensitive.` |
|      - |  426 | ` * Return` |
|      - |  427 | ` *  Returns < 0 if main_str from position offset is less than str, > 0 if it is greater than` |
|      - |  428 | ` *  str, and 0 if they are equal. If offset is equal to or greater than the length of main_str` |
|      - |  429 | ` *  or length is set and is less than 1, substr_compare() prints a warning and returns FALSE.` |
|      - |  430 | ` */` |
|     26 |  431 | `static int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  432 | `{` |
|      - |  433 | `	const char *zSource,*zOfft,*zSub;` |
|      - |  434 | `	int nOfft,nLen,nSrcLen,nSublen;` |
|     27 |  435 | `	int iCase = 0;` |
|      - |  436 | `	int rc;` |
|     27 |  437 | `	if( nArg < 3 ){` |
|      - |  438 | `		/* Missing arguments,return FALSE */` |
|      5 |  439 | `		ph7_result_bool(pCtx,0);` |
|      5 |  440 | `		return PH7_OK;` |
|      - |  441 | `	}` |
|      - |  442 | `	/* Extract the target string */` |
|     23 |  443 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|     23 |  444 | `	if( nSrcLen < 1 ){` |
|      - |  445 | `		/* Empty string,return FALSE */` |
|      3 |  446 | `		ph7_result_bool(pCtx,0);` |
|      3 |  447 | `		return PH7_OK;` |
|      - |  448 | `	}` |
|     21 |  449 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  450 | `	/* Extract the substring */` |
|     21 |  451 | `	zSub = ph7_value_to_string(apArg[1],&nSublen);` |
|     21 |  452 | `	if( nSublen < 1 \|\| nSublen > nSrcLen){` |
|      - |  453 | `		/* Empty string,return FALSE */` |
|      3 |  454 | `		ph7_result_bool(pCtx,0);` |
|      3 |  455 | `		return PH7_OK;` |
|      - |  456 | `	}` |
|      - |  457 | `	/* Extract the offset */` |
|     19 |  458 | `	nOfft = ph7_value_to_int(apArg[2]);` |
|     19 |  459 | `	if( nOfft < 0 ){` |
|      5 |  460 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|      5 |  461 | `		if( zOfft < zSource ){` |
|      - |  462 | `			/* Invalid offset */` |
|      3 |  463 | `			ph7_result_bool(pCtx,0);` |
|      3 |  464 | `			return PH7_OK;` |
|      - |  465 | `		}` |
|      3 |  466 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|      3 |  467 | `		nOfft = (int)(zOfft-zSource);` |
|     16 |  468 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  469 | `		/* Invalid offset */` |
|      3 |  470 | `		ph7_result_bool(pCtx,0);` |
|      3 |  471 | `		return PH7_OK;` |
|    ! 0 |  472 | `	}else{` |
|     13 |  473 | `		zOfft = &zSource[nOfft];` |
|     13 |  474 | `		nLen = nSrcLen - nOfft;` |
|      - |  475 | `	}` |
|     15 |  476 | `	if( nArg > 3 ){` |
|      - |  477 | `		/* Extract the length */` |
|     13 |  478 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     13 |  479 | `		if( nLen < 1 ){` |
|      - |  480 | `			/* Invalid  length */` |
|      5 |  481 | `			ph7_result_int(pCtx,1);` |
|      5 |  482 | `			return PH7_OK;` |
|      9 |  483 | `		}else if( nLen + nOfft > nSrcLen ){` |
|      - |  484 | `			/* Invalid length */` |
|      3 |  485 | `			nLen = nSrcLen - nOfft;` |
|      1 |  486 | `		}` |
|      9 |  487 | `		if( nArg > 4 ){` |
|      - |  488 | `			/* Case-sensitive or not */` |
|      5 |  489 | `			iCase = ph7_value_to_bool(apArg[4]);` |
|      2 |  490 | `		}` |
|      4 |  491 | `	}` |
|      - |  492 | `	/* Perform the comparison */` |
|     11 |  493 | `	if( iCase ){` |
|      3 |  494 | `		rc = SyStrnicmp(zOfft,zSub,(sxu32)nLen);` |
|      2 |  495 | `	}else{` |
|      9 |  496 | `		rc = SyStrncmp(zOfft,zSub,(sxu32)nLen);` |
|      - |  497 | `	}` |
|      - |  498 | `	/* Comparison result */` |
|     11 |  499 | `	ph7_result_int(pCtx,rc);` |
|     11 |  500 | `	return PH7_OK;` |
|     14 |  501 | `}` |
|      - |  502 | `/*` |
|      - |  503 | ` * int substr_count(string $haystack,string $needle[,int $offset = 0 [,int $length ]])` |
|      - |  504 | ` *  Count the number of substring occurrences.` |
|      - |  505 | ` * Parameters` |
|      - |  506 | ` * $haystack` |
|      - |  507 | ` *   The string to search in` |
|      - |  508 | ` * $needle` |
|      - |  509 | ` *   The substring to search for` |
|      - |  510 | ` * $offset` |
|      - |  511 | ` *  The offset where to start counting` |
|      - |  512 | ` * $length (NOT USED)` |
|      - |  513 | ` *  The maximum length after the specified offset to search for the substring.` |
|      - |  514 | ` *  It outputs a warning if the offset plus the length is greater than the haystack length.` |
|      - |  515 | ` * Return` |
|      - |  516 | ` *  Toral number of substring occurrences.` |
|      - |  517 | ` */` |
|     24 |  518 | `static int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  519 | `{` |
|      - |  520 | `	const char *zText,*zPattern,*zEnd;` |
|      - |  521 | `	int nTextlen,nPatlen;` |
|     25 |  522 | `	int iCount = 0;` |
|      - |  523 | `	sxu32 nOfft;` |
|      - |  524 | `	sxi32 rc;` |
|     25 |  525 | `	if( nArg < 2 ){` |
|      - |  526 | `		/* Missing arguments */` |
|      5 |  527 | `		ph7_result_int(pCtx,0);` |
|      5 |  528 | `		return PH7_OK;` |
|      - |  529 | `	}` |
|      - |  530 | `	/* Point to the haystack */` |
|     21 |  531 | `	zText = ph7_value_to_string(apArg[0],&nTextlen);` |
|      - |  532 | `	/* Point to the neddle */` |
|     21 |  533 | `	zPattern = ph7_value_to_string(apArg[1],&nPatlen);` |
|     21 |  534 | `	if( nTextlen < 1 \|\| nPatlen < 1 \|\| nPatlen > nTextlen ){` |
|      - |  535 | `		/* NOOP,return zero */` |
|      3 |  536 | `		ph7_result_int(pCtx,0);` |
|      3 |  537 | `		return PH7_OK;` |
|      - |  538 | `	}` |
|     19 |  539 | `	if( nArg > 2 ){` |
|      - |  540 | `		int iOfft;` |
|      - |  541 | `		/* Extract the offset */` |
|     15 |  542 | `		iOfft = ph7_value_to_int(apArg[2]);` |
|     15 |  543 | `		if( iOfft < 0 \|\| iOfft > nTextlen ){` |
|      - |  544 | `			/* Invalid offset,return zero */` |
|      3 |  545 | `			ph7_result_int(pCtx,0);` |
|      3 |  546 | `			return PH7_OK;` |
|      - |  547 | `		}` |
|      - |  548 | `		/* Point to the desired offset */` |
|     13 |  549 | `		zText = &zText[iOfft];` |
|      - |  550 | `		/* Adjust length */` |
|     13 |  551 | `		nTextlen -= iOfft;` |
|      6 |  552 | `	}` |
|      - |  553 | `	/* Point to the end of the string */` |
|     17 |  554 | `	zEnd = &zText[nTextlen];` |
|     17 |  555 | `	if( nArg > 3 ){` |
|      - |  556 | `		int nLen;` |
|      - |  557 | `		/* Extract the length */` |
|     13 |  558 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     13 |  559 | `		if( nLen < 0 \|\| nLen > nTextlen ){` |
|      - |  560 | `			/* Invalid length,return 0 */` |
|      7 |  561 | `			ph7_result_int(pCtx,0);` |
|      7 |  562 | `			return PH7_OK;` |
|      - |  563 | `		}` |
|      - |  564 | `		/* Adjust pointer */` |
|      7 |  565 | `		nTextlen = nLen;` |
|      7 |  566 | `		zEnd = &zText[nTextlen];` |
|      3 |  567 | `	}` |
|      - |  568 | `	/* Perform the search */` |
|     12 |  569 | `	for(;;){` |
|     25 |  570 | `		rc = SyBlobSearch((const void *)zText,(sxu32)(zEnd-zText),(const void *)zPattern,nPatlen,&nOfft);` |
|     25 |  571 | `		if( rc != SXRET_OK ){` |
|      - |  572 | `			/* Pattern not found,break immediately */` |
|      9 |  573 | `			break;` |
|      - |  574 | `		}` |
|      - |  575 | `		/* Increment counter and update the offset */` |
|     17 |  576 | `		iCount++;` |
|     17 |  577 | `		zText += nOfft + nPatlen;` |
|     17 |  578 | `		if( zText >= zEnd ){` |
|      3 |  579 | `			break;` |
|      - |  580 | `		}` |
|      1 |  581 | `	}` |
|      - |  582 | `	/* Pattern count */` |
|     11 |  583 | `	ph7_result_int(pCtx,iCount);` |
|     11 |  584 | `	return PH7_OK;` |
|     13 |  585 | `}` |
|      - |  586 | `/*` |
|      - |  587 | ` * string chunk_split(string $body[,int $chunklen = 76 [, string $end = "\r\n" ]])` |
|      - |  588 | ` *   Split a string into smaller chunks.` |
|      - |  589 | ` * Parameters` |
|      - |  590 | ` *  $body` |
|      - |  591 | ` *   The string to be chunked.` |
|      - |  592 | ` * $chunklen` |
|      - |  593 | ` *   The chunk length.` |
|      - |  594 | ` * $end` |
|      - |  595 | ` *   The line ending sequence.` |
|      - |  596 | ` * Return` |
|      - |  597 | ` *  The chunked string or NULL on failure.` |
|      - |  598 | ` */` |
|     16 |  599 | `static int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  600 | `{` |
|     17 |  601 | `	const char *zIn,*zEnd,*zSep = "\r\n";` |
|      - |  602 | `	int nSepLen,nChunkLen,nLen;` |
|     17 |  603 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  604 | `		/* Nothing to split,return null */` |
|      5 |  605 | `		ph7_result_null(pCtx);` |
|      5 |  606 | `		return PH7_OK;` |
|      - |  607 | `	}` |
|      - |  608 | `	/* initialize/Extract arguments */` |
|     13 |  609 | `	nSepLen = (int)sizeof("\r\n") - 1;` |
|     13 |  610 | `	nChunkLen = 76;` |
|     13 |  611 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 |  612 | `	zEnd = &zIn[nLen];` |
|     13 |  613 | `	if( nArg > 1 ){` |
|      - |  614 | `		/* Chunk length */` |
|     13 |  615 | `		nChunkLen = ph7_value_to_int(apArg[1]);` |
|     13 |  616 | `		if( nChunkLen < 1 ){` |
|      - |  617 | `			/* Switch back to the default length */` |
|      3 |  618 | `			nChunkLen = 76;` |
|      1 |  619 | `		}` |
|     13 |  620 | `		if( nArg > 2 ){` |
|      - |  621 | `			/* Separator */` |
|      9 |  622 | `			zSep = ph7_value_to_string(apArg[2],&nSepLen);` |
|      9 |  623 | `			if( nSepLen < 1 ){` |
|      - |  624 | `				/* Switch back to the default separator */` |
|      3 |  625 | `				zSep = "\r\n";` |
|      3 |  626 | `				nSepLen = (int)sizeof("\r\n") - 1;` |
|      1 |  627 | `			}` |
|      4 |  628 | `		}` |
|      6 |  629 | `	}` |
|      - |  630 | `	/* Perform the requested operation */` |
|     13 |  631 | `	if( nChunkLen > nLen ){` |
|      - |  632 | `		/* Nothing to split,return the string and the separator */` |
|      9 |  633 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);` |
|      9 |  634 | `		return PH7_OK;` |
|      - |  635 | `	}` |
|     17 |  636 | `	while( zIn < zEnd ){` |
|     13 |  637 | `		if( nChunkLen > (int)(zEnd-zIn) ){` |
|      3 |  638 | `			nChunkLen = (int)(zEnd - zIn);` |
|      1 |  639 | `		}` |
|      - |  640 | `		/* Append the chunk and the separator */` |
|     13 |  641 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);` |
|      - |  642 | `		/* Point beyond the chunk */` |
|     13 |  643 | `		zIn += nChunkLen;` |
|      1 |  644 | `	}` |
|      5 |  645 | `	return PH7_OK;` |
|      9 |  646 | `}` |
|      - |  647 | `/*` |
|      - |  648 | ` * string addslashes(string $str)` |
|      - |  649 | ` *  Quote string with slashes.` |
|      - |  650 | ` *  Returns a string with backslashes before characters that need` |
|      - |  651 | ` *  to be quoted in database queries etc. These characters are single` |
|      - |  652 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - |  653 | ` * Parameter` |
|      - |  654 | ` *  str: The string to be escaped.` |
|      - |  655 | ` * Return` |
|      - |  656 | ` *  Returns the escaped string` |
|      - |  657 | ` */` |
|     24 |  658 | `static int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  659 | `{` |
|      - |  660 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  661 | `	int nLen;` |
|      - |  662 | `	/* PHP enforces exactly one argument. */` |
|     28 |  663 | `	if( nArg != 1 ){` |
|      8 |  664 | `		return PH7_VmThrowException(pCtx,` |
|      - |  665 | `			"ArgumentCountError",` |
|      - |  666 | `			"addslashes() expects exactly 1 argument, %d given",` |
|      2 |  667 | `			nArg` |
|      - |  668 | `			);` |
|      - |  669 | `	}` |
|      - |  670 | `	/* NULL is deprecated and treated as an empty string; other invalid` |
|      - |  671 | `	 * types still produce a TypeError. */` |
|     22 |  672 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 |  673 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - |  674 | `			E_DEPRECATED,` |
|      - |  675 | `			"addslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - |  676 | `			);` |
|      - |  677 | `		/* fall through so conversion below yields empty string */` |
|      1 |  678 | `	}` |
|      - |  679 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     29 |  680 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     29 |  681 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     18 |  682 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 |  683 | `		return PH7_VmThrowException(pCtx,` |
|      - |  684 | `			"TypeError",` |
|      - |  685 | `			"addslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 |  686 | `			ph7_type_name(apArg[0])` |
|      - |  687 | `			);` |
|      - |  688 | `	}` |
|      - |  689 | `	/* Convert to string representation first and obtain length. */` |
|     19 |  690 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     19 |  691 | `	if( nLen < 1 ){` |
|      - |  692 | `		/* Return the empty string */` |
|      5 |  693 | `		ph7_result_string(pCtx,"",0);` |
|      5 |  694 | `		return PH7_OK;` |
|      - |  695 | `	}` |
|     15 |  696 | `	zEnd = &zIn[nLen];` |
|     15 |  697 | `	zCur = 0; /* cc warning */` |
|     20 |  698 | `	for(;;){` |
|     41 |  699 | `		if( zIn >= zEnd ){` |
|      - |  700 | `			/* No more input */` |
|     15 |  701 | `			break;` |
|      - |  702 | `		}` |
|     27 |  703 | `		zCur = zIn;` |
|      - |  704 | `		/* scan until a character that needs escaping (', ", \\, or NUL) */` |
|     89 |  705 | `		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' && zIn[0] != '\0' ){` |
|     63 |  706 | `			zIn++;` |
|      1 |  707 | `		}` |
|     27 |  708 | `		if( zIn > zCur ){` |
|      - |  709 | `			/* Append raw contents */` |
|     23 |  710 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 |  711 | `		}` |
|     27 |  712 | `		if( zIn < zEnd ){` |
|     17 |  713 | `			int c = zIn[0];` |
|     17 |  714 | `			if( c == '\0' ){` |
|      - |  715 | `				/* PHP escapes NUL as "\\0" (two characters) */` |
|      3 |  716 | `				ph7_result_string(pCtx,"\\0",2);` |
|      2 |  717 | `			}else{` |
|     15 |  718 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - |  719 | `			}` |
|      8 |  720 | `		}` |
|     27 |  721 | `		zIn++;` |
|      1 |  722 | `	}` |
|     15 |  723 | `	return PH7_OK;` |
|     16 |  724 | `}` |
|      - |  725 | `/*` |
|      - |  726 | ``  * Build a 256-entry membership mask from a PHP charlist, expanding `a..z` `` |
|      - |  727 | ` * byte ranges exactly like PHP's php_charmask(). On return aMask[c] != 0 iff` |
|      - |  728 | ` * the byte c belongs to the set. Emits the PHP-exact warnings for the three` |
|      - |  729 | ` * malformed-range shapes (ph7_context_throw_error_format prepends the active` |
|      - |  730 | ` * function name, so the messages omit it); on a bad range the surrounding` |
|      - |  731 | ` * bytes are still added and the scan never aborts. Reads only within` |
|      - |  732 | ` * [zList, zList+nLen).` |
|      - |  733 | ` *` |
|      - |  734 | ` * Use ONLY for the builtins whose charlist expands ranges the way PHP's` |
|      - |  735 | ` * php_charmask() does: trim/ltrim/rtrim/addcslashes (and quotemeta, whose set` |
|      - |  736 | ` * is a fixed literal with no ".."). Do NOT route strspn/strcspn/strtok/strpbrk` |
|      - |  737 | ` * through this — PHP treats their charlists literally, so expanding "a..z" here` |
|      - |  738 | ` * would be a behavior regression plus spurious "Invalid '..'-range" warnings.` |
|      - |  739 | ` */` |
|     78 |  740 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256])` |
|      3 |  741 | `{` |
|     81 |  742 | `	const unsigned char *zIn  = (const unsigned char *)zList;` |
|     81 |  743 | `	const unsigned char *zEnd = zIn + (nLen > 0 ? nLen : 0);` |
|     81 |  744 | `	SyZero(aMask,256);` |
|    291 |  745 | `	for( ; zIn < zEnd ; zIn++ ){` |
|    213 |  746 | `		int c = zIn[0];` |
|    213 |  747 | `		if( zIn + 3 < zEnd && zIn[1] == '.' && zIn[2] == '.' && zIn[3] >= c ){` |
|      - |  748 | `			/* Valid incrementing range c..zIn[3] */` |
|     20 |  749 | `			int hi = zIn[3],k;` |
|    364 |  750 | `			for( k = c ; k <= hi ; k++ ){` |
|    346 |  751 | `				aMask[k] = 1;` |
|    174 |  752 | `			}` |
|     20 |  753 | `			zIn += 3; /* the loop's ++ then steps past the range end */` |
|    213 |  754 | `		}else if( zIn + 1 < zEnd && zIn[0] == '.' && zIn[1] == '.' ){` |
|      - |  755 | `			/* Malformed range: mirror php_charmask's three diagnostics. */` |
|      - |  756 | `			const char *zMsg;` |
|     20 |  757 | `			if( (const unsigned char *)zList >= zIn ){` |
|      6 |  758 | `				zMsg = "no character to the left of '..'";` |
|     18 |  759 | `			}else if( zIn + 2 >= zEnd ){` |
|      6 |  760 | `				zMsg = "no character to the right of '..'";` |
|     14 |  761 | `			}else if( zIn[-1] > zIn[2] ){` |
|     12 |  762 | `				zMsg = "'..'-range needs to be incrementing";` |
|      7 |  763 | `			}else{` |
|    ! 0 |  764 | `				zMsg = 0; /* catch-all (e.g. a..b..c) */` |
|      - |  765 | `			}` |
|     20 |  766 | `			if( zMsg ){` |
|     29 |  767 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      9 |  768 | `					"Invalid '..'-range, %s",zMsg);` |
|     11 |  769 | `			}else{` |
|    ! 0 |  770 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  771 | `					"Invalid '..'-range");` |
|      - |  772 | `			}` |
|      - |  773 | `			/* Do not consume the dots: the loop's ++ steps one byte so the` |
|      - |  774 | `			 * dots are re-scanned as literals, exactly like php_charmask. */` |
|     11 |  775 | `		}else{` |
|    177 |  776 | `			aMask[c] = 1;` |
|      - |  777 | `		}` |
|    108 |  778 | `	}` |
|     81 |  779 | `}` |
|      - |  780 | `/*` |
|      - |  781 | ` * string addcslashes(string $str,string $charlist)` |
|      - |  782 | ` *  Quote string with slashes in a C style.` |
|      - |  783 | ` * Parameter` |
|      - |  784 | ` *  $str:` |
|      - |  785 | ` *    The string to be escaped.` |
|      - |  786 | ` *  $charlist:` |
|      - |  787 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|      - |  788 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|      - |  789 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|      - |  790 | ` * Return` |
|      - |  791 | ` *  Returns the escaped string.` |
|      - |  792 | ` * Note:` |
|      - |  793 | ` *  Character ranges [i.e: 'A..Z'] are supported (see PH7_BuildCharMask).` |
|      - |  794 | ` */` |
|     40 |  795 | `static int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  796 | `{` |
|      - |  797 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - |  798 | `	char aMask[256];` |
|      - |  799 | `	int nLen,nMask;` |
|      - |  800 | `	/* PHP enforces exactly two arguments. */` |
|     45 |  801 | `	if( nArg != 2 ){` |
|      8 |  802 | `		return PH7_VmThrowException(pCtx,` |
|      - |  803 | `			"ArgumentCountError",` |
|      - |  804 | `			"addcslashes() expects exactly 2 arguments, %d given",` |
|      2 |  805 | `			nArg` |
|      - |  806 | `			);` |
|      - |  807 | `	}` |
|      - |  808 | `	/* First argument must be a string-ish value.  NULL is deprecated and` |
|      - |  809 | `	 * treated as the empty string (PHP 8.1). */` |
|     40 |  810 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      - |  811 | `		/* Emit deprecation only once, similar to PHP behaviour. */` |
|      3 |  812 | `		PH7_VmThrowError(pCtx->pVm,0,/* iErr will be patched to 8192 below */` |
|      - |  813 | `			E_DEPRECATED,` |
|      - |  814 | `			"addcslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - |  815 | `			);` |
|      - |  816 | `		/* treat as empty string; fall through to conversion logic */` |
|     52 |  817 | `	} else if( ph7_value_is_array(apArg[0]) \|\|` |
|     52 |  818 | `	          ph7_value_is_object(apArg[0]) \|\|` |
|     32 |  819 | `	          ph7_value_is_resource(apArg[0]) ){` |
|      4 |  820 | `		return PH7_VmThrowException(pCtx,` |
|      - |  821 | `			"TypeError",` |
|      - |  822 | `			"addcslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 |  823 | `			ph7_type_name(apArg[0])` |
|      - |  824 | `			);` |
|      - |  825 | `	}` |
|      - |  826 | `	/* Second argument must be a string.  NULL is deprecated and treated as` |
|      - |  827 | `	 * an empty mask per PHP semantics.  Arrays/objects/resources still` |
|      - |  828 | `	 * trigger a TypeError. */` |
|     37 |  829 | `	if( ph7_value_is_null(apArg[1]) ){` |
|      3 |  830 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - |  831 | `			E_DEPRECATED,` |
|      - |  832 | `			"addcslashes(): Passing null to parameter #2 ($characters) of type string is deprecated"` |
|      - |  833 | `			);` |
|      - |  834 | `		/* allow through so it becomes empty string below */` |
|     49 |  835 | `	} else if( ph7_value_is_array(apArg[1]) \|\|` |
|     48 |  836 | `	          ph7_value_is_object(apArg[1]) \|\|` |
|     30 |  837 | `	          ph7_value_is_resource(apArg[1]) ){` |
|      4 |  838 | `		return PH7_VmThrowException(pCtx,` |
|      - |  839 | `			"TypeError",` |
|      - |  840 | `			"addcslashes(): Argument #2 ($characters) must be of type string, %s given",` |
|      2 |  841 | `			ph7_type_name(apArg[1])` |
|      - |  842 | `			);` |
|      - |  843 | `	}` |
|      - |  844 | `	/* Extract the string to process */` |
|     35 |  845 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      - |  846 | `	/* NULL would never reach here due to the check above. */` |
|     35 |  847 | `	if( nLen < 1 ){` |
|      - |  848 | `		/* Empty string returns itself. */` |
|      5 |  849 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      5 |  850 | `		return PH7_OK;` |
|      - |  851 | `	}` |
|      - |  852 | ``	/* Extract the desired mask and expand any `a..z` ranges into a lookup. */`` |
|     31 |  853 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|     31 |  854 | `	PH7_BuildCharMask(pCtx,zMask,nMask,aMask);` |
|     31 |  855 | `	zEnd = &zIn[nLen];` |
|     31 |  856 | `	zCur = 0; /* cc warning */` |
|     37 |  857 | `	for(;;){` |
|     77 |  858 | `		if( zIn >= zEnd ){` |
|      - |  859 | `			/* No more input */` |
|     31 |  860 | `			break;` |
|      - |  861 | `		}` |
|     49 |  862 | `		zCur = zIn;` |
|    125 |  863 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     79 |  864 | `			zIn++;` |
|      3 |  865 | `		}` |
|     49 |  866 | `		if( zIn > zCur ){` |
|      - |  867 | `			/* Append raw contents */` |
|     43 |  868 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     20 |  869 | `		}` |
|     49 |  870 | `		if( zIn < zEnd ){` |
|      - |  871 | `			/* Make sure we treat the byte as unsigned to avoid negative values` |
|      - |  872 | `			 * on platforms where char is signed. */` |
|     29 |  873 | `			int c = (unsigned char)zIn[0];` |
|      - |  874 | `			/* Handle special C-like escapes for common control characters first.` |
|      - |  875 | `			 * PHP outputs "\n" "\r" "\t" "\v" "\f" when those chars are` |
|      - |  876 | `			 * in the mask. NUL is left to the octal conversion below. */` |
|     29 |  877 | `			if( c == '\n' ){` |
|      3 |  878 | `				ph7_result_string(pCtx,"\\n",2);` |
|     28 |  879 | `			}else if( c == '\r' ){` |
|      3 |  880 | `				ph7_result_string(pCtx,"\\r",2);` |
|     26 |  881 | `			}else if( c == '\t' ){` |
|      3 |  882 | `				ph7_result_string(pCtx,"\\t",2);` |
|     24 |  883 | `			}else if( c == '\v' ){` |
|      3 |  884 | `				ph7_result_string(pCtx,"\\v",2);` |
|     22 |  885 | `			}else if( c == '\f' ){` |
|      3 |  886 | `				ph7_result_string(pCtx,"\\f",2);` |
|     20 |  887 | `			}else if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|      - |  888 | `				/* Convert to octal.  PHP always emits three-digit zero-padded` |
|      - |  889 | `				 * octal escapes (\001 not \1). */` |
|      7 |  890 | `				ph7_result_string_format(pCtx,"\\%03o",c);` |
|      4 |  891 | `			}else{` |
|     13 |  892 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - |  893 | `			}` |
|     13 |  894 | `		}` |
|     49 |  895 | `		zIn++;` |
|      3 |  896 | `	}` |
|     31 |  897 | `	return PH7_OK;` |
|     25 |  898 | `}` |
|      - |  899 | `/*` |
|      - |  900 | ` * string quotemeta(string $str)` |
|      - |  901 | ` *  Quote meta characters.` |
|      - |  902 | ` * Parameter` |
|      - |  903 | ` *  $str:` |
|      - |  904 | ` *    The string to be escaped.` |
|      - |  905 | ` * Return` |
|      - |  906 | ` *  Returns the escaped string.` |
|      - |  907 | `*/` |
|     12 |  908 | `static int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  909 | `{` |
|      - |  910 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  911 | `	char aMask[256];` |
|      - |  912 | `	int nLen;` |
|     14 |  913 | `	if( nArg < 1 ){` |
|      - |  914 | `		/* Nothing to process,retun NULL */` |
|      3 |  915 | `		ph7_result_null(pCtx);` |
|      3 |  916 | `		return PH7_OK;` |
|      - |  917 | `	}` |
|      - |  918 | `	/* Extract the string to process */` |
|     12 |  919 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     12 |  920 | `	if( nLen < 1 ){` |
|      - |  921 | `		/* Return the empty string */` |
|      3 |  922 | `		ph7_result_string(pCtx,"",0);` |
|      3 |  923 | `		return PH7_OK;` |
|      - |  924 | `	}` |
|      - |  925 | `	/* Fixed meta-character set (no ranges); build the lookup once. */` |
|     10 |  926 | `	PH7_BuildCharMask(pCtx,".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1,aMask);` |
|     10 |  927 | `	zEnd = &zIn[nLen];` |
|     10 |  928 | `	zCur = 0; /* cc warning */` |
|     22 |  929 | `	for(;;){` |
|     46 |  930 | `		if( zIn >= zEnd ){` |
|      - |  931 | `			/* No more input */` |
|     10 |  932 | `			break;` |
|      - |  933 | `		}` |
|     38 |  934 | `		zCur = zIn;` |
|     76 |  935 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     40 |  936 | `			zIn++;` |
|      2 |  937 | `		}` |
|     38 |  938 | `		if( zIn > zCur ){` |
|      - |  939 | `			/* Append raw contents */` |
|     20 |  940 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      9 |  941 | `		}` |
|     38 |  942 | `		if( zIn < zEnd ){` |
|     36 |  943 | `			int c = zIn[0];` |
|     36 |  944 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|     17 |  945 | `		}` |
|     38 |  946 | `		zIn++;` |
|      2 |  947 | `	}` |
|     10 |  948 | `	return PH7_OK;` |
|      8 |  949 | `}` |
|      - |  950 | `/*` |
|      - |  951 | ` * string stripslashes(string $str)` |
|      - |  952 | ` *  Un-quotes a quoted string.` |
|      - |  953 | ` *  Returns a string with backslashes before characters that need` |
|      - |  954 | ` *  to be quoted in database queries etc. These characters are single` |
|      - |  955 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - |  956 | ` * Parameter` |
|      - |  957 | ` *  $str` |
|      - |  958 | ` *   The input string.` |
|      - |  959 | ` * Return` |
|      - |  960 | ` *  Returns a string with backslashes stripped off.` |
|      - |  961 | ` */` |
|      8 |  962 | `static int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  963 | `{` |
|      - |  964 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  965 | `	int nLen;` |
|      9 |  966 | `	if( nArg < 1 ){` |
|      - |  967 | `		/* Nothing to process,retun NULL */` |
|      3 |  968 | `		ph7_result_null(pCtx);` |
|      3 |  969 | `		return PH7_OK;` |
|      - |  970 | `	}` |
|      - |  971 | `	/* Extract the string to process */` |
|      7 |  972 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 |  973 | `	if( zIn == 0 ){` |
|    ! 0 |  974 | `		ph7_result_null(pCtx);` |
|    ! 0 |  975 | `		return PH7_OK;` |
|      - |  976 | `	}` |
|      7 |  977 | `	zEnd = &zIn[nLen];` |
|      7 |  978 | `	zCur = 0; /* cc warning */` |
|      - |  979 | `	/* Encode the string */` |
|      4 |  980 | `	for(;;){` |
|      9 |  981 | `		if( zIn >= zEnd ){` |
|      - |  982 | `			/* No more input */` |
|      5 |  983 | `			break;` |
|      - |  984 | `		}` |
|      5 |  985 | `		zCur = zIn;` |
|     17 |  986 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 |  987 | `			zIn++;` |
|      1 |  988 | `		}` |
|      5 |  989 | `		if( zIn > zCur ){` |
|      - |  990 | `			/* Append raw contents */` |
|      5 |  991 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 |  992 | `		}` |
|      5 |  993 | `		if( &zIn[1] < zEnd ){` |
|      3 |  994 | `			int c = zIn[1];` |
|      3 |  995 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - |  996 | `				/* Ignore the backslash */` |
|      3 |  997 | `				zIn++;` |
|      1 |  998 | `			}` |
|      2 |  999 | `		}else{` |
|      3 | 1000 | `			break;` |
|      - | 1001 | `		}` |
|      1 | 1002 | `	}` |
|      7 | 1003 | `	return PH7_OK;` |
|      5 | 1004 | `}` |
|      - | 1005 | `/*` |
|      - | 1006 | ` * string htmlspecialchars(string $string [, int $flags = ENT_COMPAT \| ENT_HTML401 [, string $charset]])` |
|      - | 1007 | ` *  HTML escaping of special characters.` |
|      - | 1008 | ` *  The translations performed are:` |
|      - | 1009 | ` *   '&' (ampersand) ==> '&amp;'` |
|      - | 1010 | ` *   '"' (double quote) ==> '&quot;' when ENT_NOQUOTES is not set.` |
|      - | 1011 | ` *   "'" (single quote) ==> '&#039;' only when ENT_QUOTES is set.` |
|      - | 1012 | ` *   '<' (less than) ==> '&lt;'` |
|      - | 1013 | ` *   '>' (greater than) ==> '&gt;'` |
|      - | 1014 | ` * Parameters` |
|      - | 1015 | ` *  $string` |
|      - | 1016 | ` *   The string being converted.` |
|      - | 1017 | ` * $flags` |
|      - | 1018 | ` *   A bitmask of one or more of the following flags, which specify how to handle quotes.` |
|      - | 1019 | ` *   The default is ENT_COMPAT \| ENT_HTML401.` |
|      - | 1020 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone.` |
|      - | 1021 | ` *   ENT_QUOTES 	Will convert both double and single quotes.` |
|      - | 1022 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted.` |
|      - | 1023 | ` *   ENT_IGNORE 	Silently discard invalid code unit sequences instead of returning an empty string.` |
|      - | 1024 | ` * $charset` |
|      - | 1025 | ` *  Defines character set used in conversion. The default character set is ISO-8859-1. (Not used)` |
|      - | 1026 | ` * Return` |
|      - | 1027 | ` *  The escaped string or NULL on failure.` |
|      - | 1028 | ` */` |
|     20 | 1029 | `static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1030 | `{` |
|      - | 1031 | `	const char *zCur,*zIn,*zEnd;` |
|     21 | 1032 | `	int iFlags = 0x01\|0x40; /* ENT_COMPAT \| ENT_HTML401 */` |
|      - | 1033 | `	int nLen,c;` |
|     21 | 1034 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1035 | `		/* Missing/Invalid arguments,return NULL */` |
|      9 | 1036 | `		ph7_result_null(pCtx);` |
|      9 | 1037 | `		return PH7_OK;` |
|      - | 1038 | `	}` |
|      - | 1039 | `	/* Extract the target string */` |
|     13 | 1040 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1041 | `	/* Return early when the input is empty, mirroring PHP's behavior. */` |
|     13 | 1042 | `	if( nLen == 0 ){` |
|      3 | 1043 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1044 | `		return PH7_OK;` |
|      - | 1045 | `	}` |
|     11 | 1046 | `	zEnd = &zIn[nLen];` |
|      - | 1047 | `	/* Extract the flags if available */` |
|     11 | 1048 | `	if( nArg > 1 ){` |
|      9 | 1049 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 | 1050 | `		if( iFlags < 0 ){` |
|      3 | 1051 | `			iFlags = 0x01\|0x40;` |
|      1 | 1052 | `		}` |
|      4 | 1053 | `	}` |
|      - | 1054 | `	/* Perform the requested operation */` |
|     23 | 1055 | `	for(;;){` |
|     47 | 1056 | `		if( zIn >= zEnd ){` |
|      9 | 1057 | `			break;` |
|      - | 1058 | `		}` |
|     39 | 1059 | `		zCur = zIn;` |
|     83 | 1060 | `		while( zIn < zEnd && zIn[0] != '&' && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '<' && zIn[0] != '>' ){` |
|     45 | 1061 | `			zIn++;` |
|      1 | 1062 | `		}` |
|     39 | 1063 | `		if( zCur < zIn ){` |
|      - | 1064 | `			/* Append the raw string verbatim */` |
|     17 | 1065 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      8 | 1066 | `		}` |
|     39 | 1067 | `		if( zIn >= zEnd ){` |
|      3 | 1068 | `			break;` |
|      - | 1069 | `		}` |
|     37 | 1070 | `		c = zIn[0];` |
|     37 | 1071 | `		if( c == '&' ){` |
|      - | 1072 | `			/* Expand '&amp;' */` |
|      9 | 1073 | `			ph7_result_string(pCtx,"&amp;",(int)sizeof("&amp;")-1);` |
|     33 | 1074 | `		}else if( c == '<' ){` |
|      - | 1075 | `			/* Expand '&lt;' */` |
|      7 | 1076 | `			ph7_result_string(pCtx,"&lt;",(int)sizeof("&lt;")-1);` |
|     26 | 1077 | `		}else if( c == '>' ){` |
|      - | 1078 | `			/* Expand '&gt;' */` |
|      9 | 1079 | `			ph7_result_string(pCtx,"&gt;",(int)sizeof("&gt;")-1);` |
|     19 | 1080 | `		}else if( c == '\'' ){` |
|      5 | 1081 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 1082 | `				/* Expand '&#039;' */` |
|      5 | 1083 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      3 | 1084 | `			}else{` |
|      - | 1085 | `				/* Leave the single quote untouched */` |
|    ! 0 | 1086 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      1 | 1087 | `			}` |
|     13 | 1088 | `		}else if( c == '"' ){` |
|     11 | 1089 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      - | 1090 | `				/* Expand '&quot;' */` |
|      7 | 1091 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      4 | 1092 | `			}else{` |
|      - | 1093 | `				/* Leave the double quote untouched */` |
|      5 | 1094 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      - | 1095 | `			}` |
|      5 | 1096 | `		}` |
|      - | 1097 | `		/* Ignore the unsafe HTML character */` |
|     37 | 1098 | `		zIn++;` |
|      1 | 1099 | `	}` |
|     11 | 1100 | `	return PH7_OK;` |
|     11 | 1101 | `}` |
|      - | 1102 | `/*` |
|      - | 1103 | ` * string htmlspecialchars_decode(string $string[,int $quote_style = ENT_COMPAT ])` |
|      - | 1104 | ` *  Unescape HTML entities.` |
|      - | 1105 | ` * Parameters` |
|      - | 1106 | ` *  $string` |
|      - | 1107 | ` *   The string to decode` |
|      - | 1108 | ` *  $quote_style` |
|      - | 1109 | ` *    The quote style. One of the following constants:` |
|      - | 1110 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone (default)` |
|      - | 1111 | ` *   ENT_QUOTES 	Will convert both double and single quotes` |
|      - | 1112 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted` |
|      - | 1113 | ` * Return` |
|      - | 1114 | ` *  The unescaped string or NULL on failure.` |
|      - | 1115 | ` */` |
|     16 | 1116 | `static int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1117 | `{` |
|      - | 1118 | `	const char *zCur,*zIn,*zEnd;` |
|     17 | 1119 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 1120 | `	int nLen,nJump;` |
|     17 | 1121 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1122 | `		/* Missing/Invalid arguments,return NULL */` |
|      7 | 1123 | `		ph7_result_null(pCtx);` |
|      7 | 1124 | `		return PH7_OK;` |
|      - | 1125 | `	}` |
|      - | 1126 | `	/* Extract the target string */` |
|     11 | 1127 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 1128 | `	zEnd = &zIn[nLen];` |
|      - | 1129 | `	/* Extract the flags if available */` |
|     11 | 1130 | `	if( nArg > 1 ){` |
|      7 | 1131 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      7 | 1132 | `		if( iFlags < 0 ){` |
|      3 | 1133 | `			iFlags = 0x01;` |
|      1 | 1134 | `		}` |
|      3 | 1135 | `	}` |
|      - | 1136 | `	/* Perform the requested operation */` |
|     15 | 1137 | `	for(;;){` |
|     31 | 1138 | `		if( zIn >= zEnd ){` |
|     11 | 1139 | `			break;` |
|      - | 1140 | `		}` |
|     21 | 1141 | `		zCur = zIn;` |
|     51 | 1142 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|     31 | 1143 | `			zIn++;` |
|      1 | 1144 | `		}` |
|     21 | 1145 | `		if( zCur < zIn ){` |
|      - | 1146 | `			/* Append the raw string verbatim */` |
|      9 | 1147 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 1148 | `		}` |
|     21 | 1149 | `		nLen = (int)(zEnd-zIn);` |
|     21 | 1150 | `		nJump = (int)sizeof(char);` |
|     21 | 1151 | `		if( nLen >= (int)sizeof("&amp;")-1 && SyStrnicmp(zIn,"&amp;",sizeof("&amp;")-1) == 0 ){` |
|      - | 1152 | `			/* &amp; ==> '&' */` |
|      3 | 1153 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|      3 | 1154 | `			nJump = (int)sizeof("&amp;")-1;` |
|     20 | 1155 | `		}else if( nLen >= (int)sizeof("&lt;")-1 && SyStrnicmp(zIn,"&lt;",sizeof("&lt;")-1) == 0 ){` |
|      - | 1156 | `			/* &lt; ==> < */` |
|      3 | 1157 | `			ph7_result_string(pCtx,"<",(int)sizeof(char));` |
|      3 | 1158 | `			nJump = (int)sizeof("&lt;")-1;` |
|     18 | 1159 | `		}else if( nLen >= (int)sizeof("&gt;")-1 && SyStrnicmp(zIn,"&gt;",sizeof("&gt;")-1) == 0 ){` |
|      - | 1160 | `			/* &gt; ==> '>' */` |
|      3 | 1161 | `			ph7_result_string(pCtx,">",(int)sizeof(char));` |
|      3 | 1162 | `			nJump = (int)sizeof("&gt;")-1;` |
|     16 | 1163 | `		}else if( nLen >= (int)sizeof("&quot;")-1 && SyStrnicmp(zIn,"&quot;",sizeof("&quot;")-1) == 0 ){` |
|      - | 1164 | `			/* &quot; ==> '"' */` |
|     13 | 1165 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      9 | 1166 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      5 | 1167 | `			}else{` |
|      - | 1168 | `				/* Leave untouched */` |
|      5 | 1169 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      - | 1170 | `			}` |
|     13 | 1171 | `			nJump = (int)sizeof("&quot;")-1;` |
|      9 | 1172 | `		}else if( nLen >= (int)sizeof("&#039;")-1 && SyStrnicmp(zIn,"&#039;",sizeof("&#039;")-1) == 0 ){` |
|      - | 1173 | `			/* &#039; ==> ''' */` |
|      3 | 1174 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 1175 | `				/* Expand ''' */` |
|      3 | 1176 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      2 | 1177 | `			}else{` |
|      - | 1178 | `				/* Leave untouched */` |
|    ! 0 | 1179 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      - | 1180 | `			}` |
|      3 | 1181 | `			nJump = (int)sizeof("&#039;")-1;` |
|      1 | 1182 | `		}else if( nLen >= (int)sizeof(char) ){` |
|      - | 1183 | `			/* expand '&' */` |
|    ! 0 | 1184 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 1185 | `		}else{` |
|      - | 1186 | `			/* No more input to process */` |
|    ! 0 | 1187 | `			break;` |
|      - | 1188 | `		}` |
|     21 | 1189 | `		zIn += nJump;` |
|      1 | 1190 | `	}` |
|     11 | 1191 | `	return PH7_OK;` |
|      9 | 1192 | `}` |
|      - | 1193 | `/* HTML encoding/Decoding table` |
|      - | 1194 | ` * Source: Symisc RunTime API.[chm@symisc.net]` |
|      - | 1195 | ` */` |
|      - | 1196 | `static const char *azHtmlEscape[] = {` |
|      - | 1197 | ` 	"&lt;","<","&gt;",">","&amp;","&","&quot;","\"","&#39;","'",` |
|      - | 1198 | `	"&#33;","!","&#36;","$","&#35;","#","&#37;","%","&#40;","(",` |
|      - | 1199 | `	"&#41;",")","&#123;","{","&#125;","}","&#61;","=","&#43;","+",` |
|      - | 1200 | `	"&#63;","?","&#91;","[","&#93;","]","&#64;","@","&#44;",","` |
|      - | 1201 | ` };` |
|      - | 1202 | `/*` |
|      - | 1203 | ` * array get_html_translation_table(void)` |
|      - | 1204 | ` *  Returns the translation table used by htmlspecialchars() and htmlentities().` |
|      - | 1205 | ` * Parameters` |
|      - | 1206 | ` *  None` |
|      - | 1207 | ` * Return` |
|      - | 1208 | ` *  The translation table as an array or NULL on failure.` |
|      - | 1209 | ` */` |
|      4 | 1210 | `static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1211 | `{` |
|      - | 1212 | `	ph7_value *pArray,*pValue;` |
|      - | 1213 | `	sxu32 n;` |
|      - | 1214 | `	/* Element value */` |
|      5 | 1215 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      5 | 1216 | `	if( pValue == 0 ){` |
|    ! 0 | 1217 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 1218 | `		SXUNUSED(apArg);` |
|      - | 1219 | `		/* Return NULL */` |
|    ! 0 | 1220 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1221 | `		return PH7_OK;` |
|      - | 1222 | `	}` |
|      - | 1223 | `	/* Create a new array */` |
|      5 | 1224 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 | 1225 | `	if( pArray == 0 ){` |
|      - | 1226 | `		/* Return NULL */` |
|    ! 0 | 1227 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1228 | `		return PH7_OK;` |
|      - | 1229 | `	}` |
|      - | 1230 | `	/* Make the table */` |
|     85 | 1231 | `	for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|      - | 1232 | `		/* Prepare the value */` |
|     81 | 1233 | `		ph7_value_string(pValue,azHtmlEscape[n],-1 /* Compute length automatically */);` |
|      - | 1234 | `		/* Insert the value */` |
|     81 | 1235 | `		ph7_array_add_strkey_elem(pArray,azHtmlEscape[n+1],pValue);` |
|      - | 1236 | `		/* Reset the string cursor */` |
|     81 | 1237 | `		ph7_value_reset_string_cursor(pValue);` |
|     41 | 1238 | `	}` |
|      - | 1239 | `	/*` |
|      - | 1240 | `	 * Return the array.` |
|      - | 1241 | `	 * Don't worry about freeing memory, everything will be automatically` |
|      - | 1242 | `	 * released upon we return from this function.` |
|      - | 1243 | `	 */` |
|      5 | 1244 | `	ph7_result_value(pCtx,pArray);` |
|      5 | 1245 | `	return PH7_OK;` |
|      3 | 1246 | `}` |
|      - | 1247 | `/*` |
|      - | 1248 | ` * string htmlentities( string $string [, int $flags = ENT_COMPAT \| ENT_HTML401]);` |
|      - | 1249 | ` *   Convert all applicable characters to HTML entities` |
|      - | 1250 | ` * Parameters` |
|      - | 1251 | ` * $string` |
|      - | 1252 | ` *   The input string.` |
|      - | 1253 | ` * $flags` |
|      - | 1254 | ` *  A bitmask of one or more of the flags (see block-comment on PH7_builtin_htmlspecialchars())` |
|      - | 1255 | ` * Return` |
|      - | 1256 | ` * The encoded string.` |
|      - | 1257 | ` */` |
|     10 | 1258 | `static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1259 | `{` |
|     11 | 1260 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 1261 | `	const char *zIn,*zEnd;` |
|      - | 1262 | `	int nLen,c;` |
|      - | 1263 | `	sxu32 n;` |
|     11 | 1264 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1265 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 1266 | `		ph7_result_null(pCtx);` |
|      5 | 1267 | `		return PH7_OK;` |
|      - | 1268 | `	}` |
|      - | 1269 | `	/* Extract the target string */` |
|      7 | 1270 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1271 | `	/* Handle empty string up front */` |
|      7 | 1272 | `	if( nLen == 0 ){` |
|      3 | 1273 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1274 | `		return PH7_OK;` |
|      - | 1275 | `	}` |
|      5 | 1276 | `	zEnd = &zIn[nLen];` |
|      - | 1277 | `	/* Extract the flags if available */` |
|      5 | 1278 | `	if( nArg > 1 ){` |
|      3 | 1279 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      3 | 1280 | `		if( iFlags < 0 ){` |
|      3 | 1281 | `			iFlags = 0x01;` |
|      1 | 1282 | `		}` |
|      1 | 1283 | `	}` |
|      - | 1284 | `	/* Perform the requested operation */` |
|     11 | 1285 | `	for(;;){` |
|     23 | 1286 | `		if( zIn >= zEnd ){` |
|      - | 1287 | `			/* No more input to process */` |
|      5 | 1288 | `			break;` |
|      - | 1289 | `		}` |
|     19 | 1290 | `		c = zIn[0];` |
|      - | 1291 | `		/* Perform a linear lookup on the decoding table */` |
|    233 | 1292 | `		for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    223 | 1293 | `			if( azHtmlEscape[n+1][0] == c ){` |
|      - | 1294 | `				/* Got one */` |
|      9 | 1295 | `				break;` |
|      - | 1296 | `			}` |
|    108 | 1297 | `		}` |
|     19 | 1298 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|      - | 1299 | `			/* Output the safe sequence [i.e: '<' ==> '&lt;"] */` |
|      9 | 1300 | `			if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 1301 | `				/* Expand the double quote verbatim */` |
|    ! 0 | 1302 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      9 | 1303 | `			}else if(c == '\'' && ((iFlags & 0x02 /*ENT_QUOTES*/) == 0 \|\| (iFlags & 0x04) /*ENT_NOQUOTES*/) ){` |
|      - | 1304 | `				/* expand single quote verbatim */` |
|    ! 0 | 1305 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|    ! 0 | 1306 | `			}else{` |
|      9 | 1307 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1/*Compute length automatically */);` |
|      - | 1308 | `			}` |
|      5 | 1309 | `		}else{` |
|      - | 1310 | `			/* Output character verbatim */` |
|     11 | 1311 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 1312 | `		}` |
|     19 | 1313 | `		zIn++;` |
|      1 | 1314 | `	}` |
|      5 | 1315 | `	return PH7_OK;` |
|      6 | 1316 | `}` |
|      - | 1317 | `/*` |
|      - | 1318 | ` * string html_entity_decode(string $string [, int $quote_style = ENT_COMPAT [, string $charset = 'UTF-8' ]])` |
|      - | 1319 | ` *   Perform the reverse operation of html_entity_decode().` |
|      - | 1320 | ` * Parameters` |
|      - | 1321 | ` * $string` |
|      - | 1322 | ` *   The input string.` |
|      - | 1323 | ` * $flags` |
|      - | 1324 | ` *  A bitmask of one or more of the flags (see comment on PH7_builtin_htmlspecialchars())` |
|      - | 1325 | ` * Return` |
|      - | 1326 | ` * The decoded string.` |
|      - | 1327 | ` */` |
|     28 | 1328 | `static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1329 | `{` |
|      - | 1330 | `	const char *zCur,*zIn,*zEnd;` |
|     29 | 1331 | `	int iFlags = 0x01; /* ENT_COMPAT  */` |
|      - | 1332 | `	int nLen;` |
|      - | 1333 | `	sxu32 n;` |
|     29 | 1334 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1335 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 1336 | `		ph7_result_null(pCtx);` |
|      5 | 1337 | `		return PH7_OK;` |
|      - | 1338 | `	}` |
|      - | 1339 | `	/* Extract the target string */` |
|     25 | 1340 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 1341 | `	zEnd = &zIn[nLen];` |
|      - | 1342 | `	/* Extract the flags if available */` |
|     25 | 1343 | `	if( nArg > 1 ){` |
|     15 | 1344 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     15 | 1345 | `		if( iFlags < 0 ){` |
|      3 | 1346 | `			iFlags = 0x01;` |
|      1 | 1347 | `		}` |
|      7 | 1348 | `	}` |
|      - | 1349 | `	/* Perform the requested operation */` |
|     27 | 1350 | `	for(;;){` |
|     55 | 1351 | `		if( zIn >= zEnd ){` |
|      - | 1352 | `			/* No more input to process */` |
|     13 | 1353 | `			break;` |
|      - | 1354 | `		}` |
|     43 | 1355 | `		zCur = zIn;` |
|    173 | 1356 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|    131 | 1357 | `			zIn++;` |
|      1 | 1358 | `		}` |
|     43 | 1359 | `		if( zCur < zIn ){` |
|      - | 1360 | `			/* Append raw string verbatim */` |
|     27 | 1361 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     13 | 1362 | `		}` |
|     43 | 1363 | `		if( zIn >= zEnd ){` |
|     13 | 1364 | `			break;` |
|      - | 1365 | `		}` |
|     31 | 1366 | `		nLen = (int)(zEnd-zIn);` |
|      - | 1367 | `		/* Find an encoded sequence */` |
|    113 | 1368 | `		for(n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    113 | 1369 | `			int iLen = (int)SyStrlen(azHtmlEscape[n]);` |
|    113 | 1370 | `			if( nLen >= iLen && SyStrnicmp(zIn,azHtmlEscape[n],(sxu32)iLen) == 0 ){` |
|      - | 1371 | `				/* Got one */` |
|     31 | 1372 | `				zIn += iLen;` |
|     31 | 1373 | `				break;` |
|      - | 1374 | `			}` |
|     42 | 1375 | `		}` |
|     31 | 1376 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|     31 | 1377 | `			int c = azHtmlEscape[n+1][0];` |
|      - | 1378 | `			/* Output the decoded character */` |
|     31 | 1379 | `			if( c == '\'' && ((iFlags & 0x02) == 0 /*ENT_QUOTES*/\|\| (iFlags & 0x04) /*ENT_NOQUOTES*/)  ){` |
|      - | 1380 | `				/* Do not process single quotes */` |
|      9 | 1381 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|     27 | 1382 | `			}else if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 1383 | `				/* Do not process double quotes */` |
|      5 | 1384 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|      3 | 1385 | `			}else{` |
|     19 | 1386 | `				ph7_result_string(pCtx,azHtmlEscape[n+1],-1); /* Compute length automatically */` |
|      - | 1387 | `			}` |
|     16 | 1388 | `		}else{` |
|      - | 1389 | `			/* Append '&' */` |
|    ! 0 | 1390 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 1391 | `			zIn++;` |
|      - | 1392 | `		}` |
|      1 | 1393 | `	}` |
|     25 | 1394 | `	return PH7_OK;` |
|     15 | 1395 | `}` |
|      - | 1396 | `/*` |
|      - | 1397 | ` * int strlen($string)` |
|      - | 1398 | ` *  return the length of the given string.` |
|      - | 1399 | ` * Parameter` |
|      - | 1400 | ` *  string: The string being measured for length.` |
|      - | 1401 | ` * Return` |
|      - | 1402 | ` *  length of the given string.` |
|      - | 1403 | ` */` |
|   8534 | 1404 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1405 | `{` |
|   8539 | 1406 | `	int iLen = 0;` |
|   8539 | 1407 | `	if( nArg > 0 ){` |
|   8537 | 1408 | `		ph7_value_to_string(apArg[0],&iLen);` |
|   4266 | 1409 | `	}` |
|      - | 1410 | `	/* String length */` |
|   8539 | 1411 | `	ph7_result_int(pCtx,iLen);` |
|   8539 | 1412 | `	return PH7_OK;` |
|      5 | 1413 | `}` |
|      - | 1414 | `/*` |
|      - | 1415 | ` * int strcmp(string $str1,string $str2)` |
|      - | 1416 | ` *  Perform a binary safe string comparison.` |
|      - | 1417 | ` * Parameter` |
|      - | 1418 | ` *  str1: The first string` |
|      - | 1419 | ` *  str2: The second string` |
|      - | 1420 | ` * Return` |
|      - | 1421 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1422 | ` *  than str2, and 0 if they are equal.` |
|      - | 1423 | ` */` |
|     80 | 1424 | `static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1425 | `{` |
|      - | 1426 | `	const char *z1,*z2;` |
|      - | 1427 | `	int n1,n2;` |
|      - | 1428 | `	int res;` |
|     81 | 1429 | `	if( nArg < 2 ){` |
|      5 | 1430 | `		res = nArg == 0 ? 0 : 1;` |
|      5 | 1431 | `		ph7_result_int(pCtx,res);` |
|      5 | 1432 | `		return PH7_OK;` |
|      - | 1433 | `	}` |
|      - | 1434 | `	/* Perform the comparison */` |
|     77 | 1435 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     77 | 1436 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     77 | 1437 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1438 | `	/* Comparison result */` |
|     77 | 1439 | `	ph7_result_int(pCtx,res);` |
|     77 | 1440 | `	return PH7_OK;` |
|     41 | 1441 | `}` |
|      - | 1442 | `/*` |
|      - | 1443 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 1444 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 1445 | ` * Parameter` |
|      - | 1446 | ` *  str1: The first string` |
|      - | 1447 | ` *  str2: The second string` |
|      - | 1448 | ` * Return` |
|      - | 1449 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1450 | ` *  than str2, and 0 if they are equal.` |
|      - | 1451 | ` */` |
|     20 | 1452 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1453 | `{` |
|      - | 1454 | `	const char *z1,*z2;` |
|      - | 1455 | `	int res;` |
|      - | 1456 | `	int n;` |
|     21 | 1457 | `	if( nArg < 3 ){` |
|      - | 1458 | `		/* Perform a standard comparison */` |
|      5 | 1459 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 1460 | `	}` |
|      - | 1461 | `	/* Desired comparison length */` |
|     17 | 1462 | `	n  = ph7_value_to_int(apArg[2]);` |
|     17 | 1463 | `	if( n < 0 ){` |
|      - | 1464 | `		/* Invalid length */` |
|      3 | 1465 | `		ph7_result_int(pCtx,-1);` |
|      3 | 1466 | `		return PH7_OK;` |
|      - | 1467 | `	}` |
|      - | 1468 | `	/* Perform the comparison */` |
|     15 | 1469 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     15 | 1470 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     15 | 1471 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 1472 | `	/* Comparison result */` |
|     15 | 1473 | `	ph7_result_int(pCtx,res);` |
|     15 | 1474 | `	return PH7_OK;` |
|     11 | 1475 | `}` |
|      - | 1476 | `/*` |
|      - | 1477 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 1478 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 1479 | ` * Parameter` |
|      - | 1480 | ` *  str1: The first string` |
|      - | 1481 | ` *  str2: The second string` |
|      - | 1482 | ` * Return` |
|      - | 1483 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1484 | ` *  than str2, and 0 if they are equal.` |
|      - | 1485 | ` */` |
|     22 | 1486 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1487 | `{` |
|      - | 1488 | `	const char *z1,*z2;` |
|      - | 1489 | `	int n1,n2;` |
|      - | 1490 | `	int res;` |
|     23 | 1491 | `	if( nArg < 2 ){` |
|      9 | 1492 | `		res = nArg == 0 ? 0 : 1;` |
|      9 | 1493 | `		ph7_result_int(pCtx,res);` |
|      9 | 1494 | `		return PH7_OK;` |
|      - | 1495 | `	}` |
|      - | 1496 | `	/* Perform the comparison */` |
|     15 | 1497 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     15 | 1498 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     15 | 1499 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1500 | `	/* Comparison result */` |
|     15 | 1501 | `	ph7_result_int(pCtx,res);` |
|     15 | 1502 | `	return PH7_OK;` |
|     12 | 1503 | `}` |
|      - | 1504 | `/*` |
|      - | 1505 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 1506 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 1507 | ` * Parameter` |
|      - | 1508 | ` *  $str1: The first string` |
|      - | 1509 | ` *  $str2: The second string` |
|      - | 1510 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 1511 | ` * Return` |
|      - | 1512 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1513 | ` *  than str2, and 0 if they are equal.` |
|      - | 1514 | ` */` |
|      8 | 1515 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1516 | `{` |
|      - | 1517 | `	const char *z1,*z2;` |
|      - | 1518 | `	int res;` |
|      - | 1519 | `	int n;` |
|      9 | 1520 | `	if( nArg < 3 ){` |
|      - | 1521 | `		/* Perform a standard comparison */` |
|      5 | 1522 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 1523 | `	}` |
|      - | 1524 | `	/* Desired comparison length */` |
|      5 | 1525 | `	n  = ph7_value_to_int(apArg[2]);` |
|      5 | 1526 | `	if( n < 0 ){` |
|      - | 1527 | `		/* Invalid length */` |
|    ! 0 | 1528 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 1529 | `		return PH7_OK;` |
|      - | 1530 | `	}` |
|      - | 1531 | `	/* Perform the comparison */` |
|      5 | 1532 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|      5 | 1533 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|      5 | 1534 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 1535 | `	/* Comparison result */` |
|      5 | 1536 | `	ph7_result_int(pCtx,res);` |
|      5 | 1537 | `	return PH7_OK;` |
|      5 | 1538 | `}` |
|      - | 1539 | `/*` |
|      - | 1540 | ` * Implode context [i.e: it's private data].` |
|      - | 1541 | ` * A pointer to the following structure is forwarded` |
|      - | 1542 | ` * verbatim to the array walker callback defined below.` |
|      - | 1543 | ` */` |
|      - | 1544 | `struct implode_data {` |
|      - | 1545 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 1546 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 1547 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 1548 | `	int nSeplen;          /* Separator length */` |
|      - | 1549 | `	int bFirst;           /* TRUE if first call */` |
|      - | 1550 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 1551 | `	sxi32 rc;             /* Captured allocation rc; SXERR_MEM => the builtin raises an OOM fatal */` |
|      - | 1552 | `};` |
|      - | 1553 | `/*` |
|      - | 1554 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 1555 | ` * The following routine is invoked for each array entry passed` |
|      - | 1556 | ` * to the implode() function.` |
|      - | 1557 | ` */` |
| 131926 | 1558 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 1559 | `{` |
|  65963 | 1560 | `	SXUNUSED(pKey);` |
| 131931 | 1561 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 1562 | `	const char *zData;` |
|      - | 1563 | `	int nLen;` |
| 131931 | 1564 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 1565 | `		if( pData->nSeplen > 0 ){` |
|      3 | 1566 | `			if( !pData->bFirst ){` |
|      - | 1567 | `				/* append the separator first */` |
|      3 | 1568 | `				if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 1569 | `					pData->rc = SXERR_MEM;` |
|    ! 0 | 1570 | `					return PH7_ABORT;` |
|      - | 1571 | `				}` |
|      2 | 1572 | `			}else{` |
|    ! 0 | 1573 | `				pData->bFirst = 0;` |
|      - | 1574 | `			}` |
|      1 | 1575 | `		}` |
|      - | 1576 | `		/* Recurse */` |
|      3 | 1577 | `		pData->bFirst = 1;` |
|      3 | 1578 | `		pData->nRecCount++;` |
|      3 | 1579 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 1580 | `		pData->nRecCount--;` |
|      - | 1581 | `		/* Propagate an allocation failure surfaced deeper in the recursion. */` |
|      3 | 1582 | `		if( pData->rc != SXRET_OK ){` |
|    ! 0 | 1583 | `			return PH7_ABORT;` |
|      - | 1584 | `		}` |
|      3 | 1585 | `		return PH7_OK;` |
|      - | 1586 | `	}` |
|      - | 1587 | `	/* Extract the string representation of the entry value */` |
| 131929 | 1588 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 1589 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 131929 | 1590 | `	if( pData->bFirst ){` |
|  32519 | 1591 | `		pData->bFirst = 0;` |
| 115672 | 1592 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 1593 | `		/* append the separator first */` |
|  99403 | 1594 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 1595 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1596 | `			return PH7_ABORT;` |
|      - | 1597 | `		}` |
|  49699 | 1598 | `	}` |
|      - | 1599 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 131929 | 1600 | `	if( nLen > 0 ){` |
| 120063 | 1601 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1602 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1603 | `			return PH7_ABORT;` |
|      - | 1604 | `		}` |
|  60029 | 1605 | `	}` |
| 131929 | 1606 | `	return PH7_OK;` |
|  65968 | 1607 | `}` |
|      - | 1608 | `/*` |
|      - | 1609 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 1610 | ` * string implode(array $pieces,...)` |
|      - | 1611 | ` *  Join array elements with a string.` |
|      - | 1612 | ` * $glue` |
|      - | 1613 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 1614 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 1615 | ` * $pieces` |
|      - | 1616 | ` *   The array of strings to implode.` |
|      - | 1617 | ` * Return` |
|      - | 1618 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 1619 | ` *  order, with the glue string between each element.` |
|      - | 1620 | ` */` |
|  32540 | 1621 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1622 | `{` |
|      - | 1623 | `	struct implode_data imp_data;` |
|  32545 | 1624 | `	int i = 1;` |
|  32545 | 1625 | `	if( nArg < 1 ){` |
|      - | 1626 | `		/* Missing argument,return NULL */` |
|    ! 0 | 1627 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1628 | `		return PH7_OK;` |
|      - | 1629 | `	}` |
|      - | 1630 | `	/* Prepare the implode context */` |
|  32545 | 1631 | `	imp_data.pCtx = pCtx;` |
|  32545 | 1632 | `	imp_data.bRecursive = 0;` |
|  32545 | 1633 | `	imp_data.bFirst = 1;` |
|  32545 | 1634 | `	imp_data.nRecCount = 0;` |
|  32545 | 1635 | `	imp_data.rc = SXRET_OK;` |
|  32545 | 1636 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  32543 | 1637 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  16274 | 1638 | `	}else{` |
|      3 | 1639 | `		imp_data.zSep = 0;` |
|      3 | 1640 | `		imp_data.nSeplen = 0;` |
|      3 | 1641 | `		i = 0;` |
|      - | 1642 | `	}` |
|  32545 | 1643 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 1644 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1645 | `	}` |
|      - | 1646 | `	/* Start the 'join' process */` |
|  65085 | 1647 | `	while( i < nArg ){` |
|  32545 | 1648 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1649 | `			/* Iterate throw array entries */` |
|  32545 | 1650 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 1651 | `			/* Surface a callback allocation failure as a fatal */` |
|  32545 | 1652 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 1653 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1654 | `			}` |
|  16275 | 1655 | `		}else{` |
|      - | 1656 | `			const char *zData;` |
|      - | 1657 | `			int nLen;` |
|      - | 1658 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 1659 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1660 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 1661 | `			if( imp_data.bFirst ){` |
|    ! 0 | 1662 | `				imp_data.bFirst = 0;` |
|    ! 0 | 1663 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1664 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 1665 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1666 | `				}` |
|    ! 0 | 1667 | `			}` |
|      - | 1668 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 1669 | `			if( nLen > 0 ){` |
|    ! 0 | 1670 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1671 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1672 | `				}` |
|    ! 0 | 1673 | `			}` |
|      - | 1674 | `		}` |
|  32545 | 1675 | `		i++;` |
|      5 | 1676 | `	}` |
|  32545 | 1677 | `	return PH7_OK;` |
|  16275 | 1678 | `}` |
|      - | 1679 | `/*` |
|      - | 1680 | ` * Symisc eXtension:` |
|      - | 1681 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 1682 | ` * Purpose` |
|      - | 1683 | ` *  Same as implode() but recurse on arrays.` |
|      - | 1684 | ` * Example:` |
|      - | 1685 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 1686 | ` *   echo implode_recursive("/",$a);` |
|      - | 1687 | ` *   Will output` |
|      - | 1688 | ` *     usr/home/dean.` |
|      - | 1689 | ` *   While the standard implode would produce.` |
|      - | 1690 | ` *    usr/Array.` |
|      - | 1691 | ` * Parameter` |
|      - | 1692 | ` *  Refer to implode().` |
|      - | 1693 | ` * Return` |
|      - | 1694 | ` *  Refer to implode().` |
|      - | 1695 | ` */` |
|     12 | 1696 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1697 | `{` |
|      - | 1698 | `	struct implode_data imp_data;` |
|     13 | 1699 | `	int i = 1;` |
|     13 | 1700 | `	if( nArg < 1 ){` |
|      - | 1701 | `		/* Missing argument,return NULL */` |
|      3 | 1702 | `		ph7_result_null(pCtx);` |
|      3 | 1703 | `		return PH7_OK;` |
|      - | 1704 | `	}` |
|      - | 1705 | `	/* Prepare the implode context */` |
|     11 | 1706 | `	imp_data.pCtx = pCtx;` |
|     11 | 1707 | `	imp_data.bRecursive = 1;` |
|     11 | 1708 | `	imp_data.bFirst = 1;` |
|     11 | 1709 | `	imp_data.nRecCount = 0;` |
|     11 | 1710 | `	imp_data.rc = SXRET_OK;` |
|     11 | 1711 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 1712 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 1713 | `	}else{` |
|    ! 0 | 1714 | `		imp_data.zSep = 0;` |
|    ! 0 | 1715 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 1716 | `		i = 0;` |
|      - | 1717 | `	}` |
|     11 | 1718 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 1719 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1720 | `	}` |
|      - | 1721 | `	/* Start the 'join' process */` |
|     21 | 1722 | `	while( i < nArg ){` |
|     11 | 1723 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1724 | `			/* Iterate throw array entries */` |
|      3 | 1725 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 1726 | `			/* Surface a callback allocation failure as a fatal */` |
|      3 | 1727 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 1728 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1729 | `			}` |
|      2 | 1730 | `		}else{` |
|      - | 1731 | `			const char *zData;` |
|      - | 1732 | `			int nLen;` |
|      - | 1733 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 1734 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1735 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 1736 | `			if( imp_data.bFirst ){` |
|      9 | 1737 | `				imp_data.bFirst = 0;` |
|      4 | 1738 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1739 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 1740 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1741 | `				}` |
|    ! 0 | 1742 | `			}` |
|      - | 1743 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 1744 | `			if( nLen > 0 ){` |
|      9 | 1745 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1746 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1747 | `				}` |
|      4 | 1748 | `			}` |
|      - | 1749 | `		}` |
|     11 | 1750 | `		i++;` |
|      1 | 1751 | `	}` |
|     11 | 1752 | `	return PH7_OK;` |
|      7 | 1753 | `}` |
|      - | 1754 | `/*` |
|      - | 1755 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 1756 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 1757 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 1758 | ` * Parameters` |
|      - | 1759 | ` *  $delimiter` |
|      - | 1760 | ` *   The boundary string.` |
|      - | 1761 | ` * $string` |
|      - | 1762 | ` *   The input string.` |
|      - | 1763 | ` * $limit` |
|      - | 1764 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 1765 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 1766 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 1767 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 1768 | ` * Returns` |
|      - | 1769 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 1770 | ` *  on boundaries formed by the delimiter.` |
|      - | 1771 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 1772 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 1773 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 1774 | ` *  will be returned.` |
|      - | 1775 | ` * NOTE:` |
|      - | 1776 | ` *  Negative limit is not supported.` |
|      - | 1777 | ` */` |
|   6168 | 1778 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1779 | `{` |
|      - | 1780 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 1781 | `	int nDelim,nStrlen,iLimit;` |
|      - | 1782 | `	ph7_value *pArray;` |
|      - | 1783 | `	ph7_value *pValue;` |
|      - | 1784 | `	sxu32 nOfft;` |
|      - | 1785 | `	sxi32 rc;` |
|   6173 | 1786 | `	if( nArg < 2 ){` |
|      - | 1787 | `		/* Missing arguments,return FALSE */` |
|      9 | 1788 | `		ph7_result_bool(pCtx,0);` |
|      9 | 1789 | `		return PH7_OK;` |
|      - | 1790 | `	}` |
|      - | 1791 | `	/* Extract the delimiter */` |
|   6165 | 1792 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   6165 | 1793 | `	if( nDelim < 1 ){` |
|      - | 1794 | `		/* Empty delimiter,return FALSE */` |
|      3 | 1795 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1796 | `		return PH7_OK;` |
|      - | 1797 | `	}` |
|      - | 1798 | `	/* Extract the string */` |
|   6163 | 1799 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   6163 | 1800 | `	if( nStrlen < 1 ){` |
|      - | 1801 | `		/* Empty string: normally an array with a single empty element (PHP behavior).` |
|      - | 1802 | `		 * A negative limit drops the last -limit components, so the sole empty` |
|      - | 1803 | `		 * component is dropped and the result is an empty array. */` |
|      7 | 1804 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      7 | 1805 | `		if( pArrayTmp == 0 ){` |
|      - | 1806 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 1807 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 1808 | `			return PH7_OK;` |
|      - | 1809 | `		}` |
|      7 | 1810 | `		if( !(nArg > 2 && ph7_value_to_int(apArg[2]) < 0) ){` |
|      5 | 1811 | `			ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      5 | 1812 | `			if( pValueTmp == 0 ){` |
|      - | 1813 | `				/* Out of memory,return FALSE */` |
|    ! 0 | 1814 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 1815 | `				return PH7_OK;` |
|      - | 1816 | `			}` |
|      5 | 1817 | `			ph7_value_string(pValueTmp, "", 0);` |
|      5 | 1818 | `			if( ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp) != SXRET_OK ){` |
|    ! 0 | 1819 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1820 | `			}` |
|      2 | 1821 | `		}` |
|      7 | 1822 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      7 | 1823 | `		return PH7_OK;` |
|      - | 1824 | `	}` |
|      - | 1825 | `	/* Point to the end of the string */` |
|   6157 | 1826 | `	zEnd = &zString[nStrlen];` |
|      - | 1827 | `	/* Create the array */` |
|   6157 | 1828 | `	pArray =  ph7_context_new_array(pCtx);` |
|   6157 | 1829 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   6157 | 1830 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 1831 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 1832 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1833 | `		return PH7_OK;` |
|      - | 1834 | `	}` |
|      - | 1835 | `	/* Set a defualt limit */` |
|   6157 | 1836 | `	iLimit = SXI32_HIGH;` |
|   6157 | 1837 | `	if( nArg > 2 ){` |
|     29 | 1838 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     29 | 1839 | `		if( iLimit < 0 ){` |
|      - | 1840 | `			/* Negative limit: keep all components except the last -iLimit (PHP).` |
|      - | 1841 | `			 * Pre-count the components (delimiters + 1), then emit only the first` |
|      - | 1842 | `			 * nKeep CLEAN components — no trailing-remainder merge (the difference` |
|      - | 1843 | `			 * from the positive path). nKeep <= 0 drops everything -> empty array. */` |
|     17 | 1844 | `			int nTotal = 1,nKeep;` |
|     17 | 1845 | `			const char *zScan = zString;` |
|      - | 1846 | `			sxu32 nScanOfft;` |
|     57 | 1847 | `			while( SyBlobSearch(zScan,(sxu32)(zEnd - zScan),zDelim,nDelim,&nScanOfft) == SXRET_OK ){` |
|     41 | 1848 | `				nTotal++;` |
|     41 | 1849 | `				zScan = &zScan[nScanOfft + nDelim];` |
|      1 | 1850 | `			}` |
|     17 | 1851 | `			nKeep = nTotal + iLimit; /* iLimit < 0, so this is nTotal - (-iLimit) */` |
|     49 | 1852 | `			while( nKeep > (int)ph7_array_count(pArray)` |
|     39 | 1853 | `				&& SyBlobSearch(zString,(sxu32)(zEnd - zString),zDelim,nDelim,&nOfft) == SXRET_OK ){` |
|      - | 1854 | `				/* Emit the next clean component */` |
|     23 | 1855 | `				zCur = &zString[nOfft];` |
|     23 | 1856 | `				ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|     23 | 1857 | `				if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 1858 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1859 | `				}` |
|     23 | 1860 | `				zString = &zCur[nDelim];` |
|     23 | 1861 | `				ph7_value_reset_string_cursor(pValue);` |
|      1 | 1862 | `			}` |
|     17 | 1863 | `			ph7_result_value(pCtx,pArray);` |
|     17 | 1864 | `			return PH7_OK;` |
|      - | 1865 | `		}` |
|     13 | 1866 | `		if( iLimit == 0 ){` |
|      5 | 1867 | `			iLimit = 1;` |
|      2 | 1868 | `		}` |
|     13 | 1869 | `		iLimit--;` |
|      6 | 1870 | `	}` |
|      - | 1871 | `	/* Start exploding */` |
|  71609 | 1872 | `	for(;;){` |
| 143223 | 1873 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 143223 | 1874 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 1875 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   6141 | 1876 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   6141 | 1877 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 1878 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1879 | `			}` |
|   6141 | 1880 | `			break;` |
|      - | 1881 | `		}` |
|      - | 1882 | `		/* Point to the desired offset */` |
| 137087 | 1883 | `		zCur = &zString[nOfft];` |
|      - | 1884 | `		/* Perform the store operation (may be empty) */` |
| 137087 | 1885 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 137087 | 1886 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 1887 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 1888 | `		}` |
|      - | 1889 | `		/* Point beyond the delimiter */` |
| 137087 | 1890 | `		zString = &zCur[nDelim];` |
|      - | 1891 | `		/* Reset the cursor */` |
| 137087 | 1892 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 1893 | `	}` |
|      - | 1894 | `	/* Return the freshly created array */` |
|   6141 | 1895 | `	ph7_result_value(pCtx,pArray);` |
|      - | 1896 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 1897 | `	 * released as soon we return from this foregin function.` |
|      - | 1898 | `	 */` |
|   6141 | 1899 | `	return PH7_OK;` |
|   3089 | 1900 | `}` |
|      - | 1901 | `/*` |
|      - | 1902 | ` * string trim(string $str[,string $charlist ])` |
|      - | 1903 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 1904 | ` * Parameters` |
|      - | 1905 | ` *  $str` |
|      - | 1906 | ` *   The string that will be trimmed.` |
|      - | 1907 | ` * $charlist` |
|      - | 1908 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1909 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1910 | ` *   With .. you can specify a range of characters.` |
|      - | 1911 | ` * Returns.` |
|      - | 1912 | ` *  Thr processed string.` |
|      - | 1913 | ` * NOTE:` |
|      - | 1914 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 1915 | ` */` |
|  14040 | 1916 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1917 | `{` |
|      - | 1918 | `	const char *zString;` |
|      - | 1919 | `	int nLen;` |
|  14045 | 1920 | `	if( nArg < 1 ){` |
|      - | 1921 | `		/* Missing arguments,return null */` |
|      3 | 1922 | `		ph7_result_null(pCtx);` |
|      3 | 1923 | `		return PH7_OK;` |
|      - | 1924 | `	}` |
|      - | 1925 | `	/* Extract the target string */` |
|  14043 | 1926 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  14043 | 1927 | `	if( nLen < 1 ){` |
|      - | 1928 | `		/* Empty string,return */` |
|   1739 | 1929 | `		ph7_result_string(pCtx,"",0);` |
|   1739 | 1930 | `		return PH7_OK;` |
|      - | 1931 | `	}` |
|      - | 1932 | `	/* Start the trim process */` |
|  12309 | 1933 | `	if( nArg < 2 ){` |
|      - | 1934 | `		SyString sStr;` |
|      - | 1935 | `		/* Remove white spaces and NUL bytes */` |
|  12279 | 1936 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  30105 | 1937 | `		SyStringFullTrimSafe(&sStr);` |
|  12279 | 1938 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   6142 | 1939 | `	}else{` |
|      - | 1940 | `		/* Char list */` |
|      - | 1941 | `		const char *zList;` |
|      - | 1942 | `		int nListlen;` |
|     33 | 1943 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     33 | 1944 | `		if( nListlen < 1 ){` |
|      - | 1945 | `			/* Return the string unchanged */` |
|      6 | 1946 | `			ph7_result_string(pCtx,zString,nLen);` |
|      4 | 1947 | `		}else{` |
|      - | 1948 | `			char aMask[256];` |
|     29 | 1949 | `			const char *zEnd = &zString[nLen];` |
|     29 | 1950 | `			const char *zCur = zString;` |
|     29 | 1951 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 1952 | `			/* Left trim */` |
|     79 | 1953 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     53 | 1954 | `				zCur++;` |
|      3 | 1955 | `			}` |
|      - | 1956 | `			/* Right trim */` |
|     79 | 1957 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     53 | 1958 | `				zEnd--;` |
|      3 | 1959 | `			}` |
|     29 | 1960 | `			if( zCur >= zEnd ){` |
|      - | 1961 | `				/* Return the empty string */` |
|    ! 0 | 1962 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1963 | `			}else{` |
|     29 | 1964 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 1965 | `			}` |
|      - | 1966 | `		}` |
|      - | 1967 | `	}` |
|  12309 | 1968 | `	return PH7_OK;` |
|   7025 | 1969 | `}` |
|      - | 1970 | `/*` |
|      - | 1971 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 1972 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 1973 | ` * Parameters` |
|      - | 1974 | ` *  $str` |
|      - | 1975 | ` *   The string that will be trimmed.` |
|      - | 1976 | ` * $charlist` |
|      - | 1977 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1978 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1979 | ` *   With .. you can specify a range of characters.` |
|      - | 1980 | ` * Returns.` |
|      - | 1981 | ` *  Thr processed string.` |
|      - | 1982 | ` * NOTE:` |
|      - | 1983 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 1984 | ` */` |
|     30 | 1985 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1986 | `{` |
|      - | 1987 | `	const char *zString;` |
|      - | 1988 | `	int nLen;` |
|     33 | 1989 | `	if( nArg < 1 ){` |
|      - | 1990 | `		/* Missing arguments,return null */` |
|      3 | 1991 | `		ph7_result_null(pCtx);` |
|      3 | 1992 | `		return PH7_OK;` |
|      - | 1993 | `	}` |
|      - | 1994 | `	/* Extract the target string */` |
|     31 | 1995 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 1996 | `	if( nLen < 1 ){` |
|      - | 1997 | `		/* Empty string,return */` |
|      5 | 1998 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1999 | `		return PH7_OK;` |
|      - | 2000 | `	}` |
|      - | 2001 | `	/* Start the trim process */` |
|     27 | 2002 | `	if( nArg < 2 ){` |
|      - | 2003 | `		SyString sStr;` |
|      - | 2004 | `		/* Remove white spaces and NUL bytes*/` |
|     17 | 2005 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     41 | 2006 | `		SyStringRightTrimSafe(&sStr);` |
|     17 | 2007 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      9 | 2008 | `	}else{` |
|      - | 2009 | `		/* Char list */` |
|      - | 2010 | `		const char *zList;` |
|      - | 2011 | `		int nListlen;` |
|     11 | 2012 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     11 | 2013 | `		if( nListlen < 1 ){` |
|      - | 2014 | `			/* Return the string unchanged */` |
|    ! 0 | 2015 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 2016 | `		}else{` |
|      - | 2017 | `			char aMask[256];` |
|     11 | 2018 | `			const char *zEnd = &zString[nLen];` |
|     11 | 2019 | `			const char *zCur = zString;` |
|     11 | 2020 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2021 | `			/* Right trim */` |
|     29 | 2022 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     20 | 2023 | `				zEnd--;` |
|      2 | 2024 | `			}` |
|     11 | 2025 | `			if( zEnd <= zCur ){` |
|      - | 2026 | `				/* Return the empty string */` |
|    ! 0 | 2027 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2028 | `			}else{` |
|     11 | 2029 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2030 | `			}` |
|      - | 2031 | `		}` |
|      - | 2032 | `	}` |
|     27 | 2033 | `	return PH7_OK;` |
|     18 | 2034 | `}` |
|      - | 2035 | `/*` |
|      - | 2036 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 2037 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2038 | ` * Parameters` |
|      - | 2039 | ` *  $str` |
|      - | 2040 | ` *   The string that will be trimmed.` |
|      - | 2041 | ` * $charlist` |
|      - | 2042 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2043 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2044 | ` *   With .. you can specify a range of characters.` |
|      - | 2045 | ` * Returns.` |
|      - | 2046 | ` *  Thr processed string.` |
|      - | 2047 | ` * NOTE:` |
|      - | 2048 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2049 | ` */` |
|     14 | 2050 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2051 | `{` |
|      - | 2052 | `	const char *zString;` |
|      - | 2053 | `	int nLen;` |
|     16 | 2054 | `	if( nArg < 1 ){` |
|      - | 2055 | `		/* Missing arguments,return null */` |
|      3 | 2056 | `		ph7_result_null(pCtx);` |
|      3 | 2057 | `		return PH7_OK;` |
|      - | 2058 | `	}` |
|      - | 2059 | `	/* Extract the target string */` |
|     14 | 2060 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     14 | 2061 | `	if( nLen < 1 ){` |
|      - | 2062 | `		/* Empty string,return */` |
|    ! 0 | 2063 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2064 | `		return PH7_OK;` |
|      - | 2065 | `	}` |
|      - | 2066 | `	/* Start the trim process */` |
|     14 | 2067 | `	if( nArg < 2 ){` |
|      - | 2068 | `		SyString sStr;` |
|      - | 2069 | `		/* Remove white spaces and NUL byte */` |
|      3 | 2070 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|      8 | 2071 | `		SyStringLeftTrimSafe(&sStr);` |
|      3 | 2072 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      2 | 2073 | `	}else{` |
|      - | 2074 | `		/* Char list */` |
|      - | 2075 | `		const char *zList;` |
|      - | 2076 | `		int nListlen;` |
|     12 | 2077 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     12 | 2078 | `		if( nListlen < 1 ){` |
|      - | 2079 | `			/* Return the string unchanged */` |
|      3 | 2080 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2081 | `		}else{` |
|      - | 2082 | `			char aMask[256];` |
|     10 | 2083 | `			const char *zEnd = &zString[nLen];` |
|     10 | 2084 | `			const char *zCur = zString;` |
|     10 | 2085 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2086 | `			/* Left trim */` |
|     28 | 2087 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     20 | 2088 | `				zCur++;` |
|      2 | 2089 | `			}` |
|     10 | 2090 | `			if( zCur >= zEnd ){` |
|      - | 2091 | `				/* Return the empty string */` |
|    ! 0 | 2092 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2093 | `			}else{` |
|     10 | 2094 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2095 | `			}` |
|      - | 2096 | `		}` |
|      - | 2097 | `	}` |
|     14 | 2098 | `	return PH7_OK;` |
|      9 | 2099 | `}` |
|      - | 2100 | `/*` |
|      - | 2101 | ` * string strtolower(string $str)` |
|      - | 2102 | ` *  Make a string lowercase.` |
|      - | 2103 | ` * Parameters` |
|      - | 2104 | ` *  $str` |
|      - | 2105 | ` *   The input string.` |
|      - | 2106 | ` * Returns.` |
|      - | 2107 | ` *  The lowercased string.` |
|      - | 2108 | ` */` |
|  32172 | 2109 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2110 | `{` |
|      - | 2111 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2112 | `	int nLen;` |
|  32177 | 2113 | `	if( nArg < 1 ){` |
|      - | 2114 | `		/* Missing arguments,return null */` |
|      3 | 2115 | `		ph7_result_null(pCtx);` |
|      3 | 2116 | `		return PH7_OK;` |
|      - | 2117 | `	}` |
|      - | 2118 | `	/* Extract the target string */` |
|  32175 | 2119 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  32175 | 2120 | `	if( nLen < 1 ){` |
|      - | 2121 | `		/* Empty string,return */` |
|      3 | 2122 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2123 | `		return PH7_OK;` |
|      - | 2124 | `	}` |
|      - | 2125 | `	/* Perform the requested operation */` |
|  32173 | 2126 | `	zEnd = &zString[nLen];` |
| 101352 | 2127 | `	for(;;){` |
| 202709 | 2128 | `		if( zString >= zEnd ){` |
|      - | 2129 | `			/* No more input,break immediately */` |
|  32173 | 2130 | `			break;` |
|      - | 2131 | `		}` |
| 170541 | 2132 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2133 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2134 | `			zCur = zString;` |
|    ! 0 | 2135 | `			zString++;` |
|    ! 0 | 2136 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2137 | `				zString++;` |
|    ! 0 | 2138 | `			}` |
|      - | 2139 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2140 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2141 | `		}else{` |
| 170541 | 2142 | `			int c = zString[0];` |
| 170541 | 2143 | `			if( SyisUpper(c) ){` |
| 170539 | 2144 | `				c = SyToLower(zString[0]);` |
|  85267 | 2145 | `			}` |
|      - | 2146 | `			/* Append character */` |
| 170541 | 2147 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2148 | `			/* Advance the cursor */` |
| 170541 | 2149 | `			zString++;` |
|      - | 2150 | `		}` |
|      5 | 2151 | `	}` |
|  32173 | 2152 | `	return PH7_OK;` |
|  16091 | 2153 | `}` |
|      - | 2154 | `/*` |
|      - | 2155 | ` * string strtolower(string $str)` |
|      - | 2156 | ` *  Make a string uppercase.` |
|      - | 2157 | ` * Parameters` |
|      - | 2158 | ` *  $str` |
|      - | 2159 | ` *   The input string.` |
|      - | 2160 | ` * Returns.` |
|      - | 2161 | ` *  The uppercased string.` |
|      - | 2162 | ` */` |
|     42 | 2163 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2164 | `{` |
|      - | 2165 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2166 | `	int nLen;` |
|     47 | 2167 | `	if( nArg < 1 ){` |
|      - | 2168 | `		/* Missing arguments,return null */` |
|      3 | 2169 | `		ph7_result_null(pCtx);` |
|      3 | 2170 | `		return PH7_OK;` |
|      - | 2171 | `	}` |
|      - | 2172 | `	/* Extract the target string */` |
|     45 | 2173 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     45 | 2174 | `	if( nLen < 1 ){` |
|      - | 2175 | `		/* Empty string,return */` |
|      3 | 2176 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2177 | `		return PH7_OK;` |
|      - | 2178 | `	}` |
|      - | 2179 | `	/* Perform the requested operation */` |
|     43 | 2180 | `	zEnd = &zString[nLen];` |
|     98 | 2181 | `	for(;;){` |
|    201 | 2182 | `		if( zString >= zEnd ){` |
|      - | 2183 | `			/* No more input,break immediately */` |
|     43 | 2184 | `			break;` |
|      - | 2185 | `		}` |
|    163 | 2186 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2187 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2188 | `			zCur = zString;` |
|    ! 0 | 2189 | `			zString++;` |
|    ! 0 | 2190 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2191 | `				zString++;` |
|    ! 0 | 2192 | `			}` |
|      - | 2193 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2194 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2195 | `		}else{` |
|    163 | 2196 | `			int c = zString[0];` |
|    163 | 2197 | `			if( SyisLower(c) ){` |
|    157 | 2198 | `				c = SyToUpper(zString[0]);` |
|     76 | 2199 | `			}` |
|      - | 2200 | `			/* Append character */` |
|    163 | 2201 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2202 | `			/* Advance the cursor */` |
|    163 | 2203 | `			zString++;` |
|      - | 2204 | `		}` |
|      5 | 2205 | `	}` |
|     43 | 2206 | `	return PH7_OK;` |
|     26 | 2207 | `}` |
|      - | 2208 | `/*` |
|      - | 2209 | ` * string ucfirst(string $str)` |
|      - | 2210 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2211 | ` *  character is alphabetic.` |
|      - | 2212 | ` * Parameters` |
|      - | 2213 | ` *  $str` |
|      - | 2214 | ` *   The input string.` |
|      - | 2215 | ` * Returns.` |
|      - | 2216 | ` *  The processed string.` |
|      - | 2217 | ` */` |
|      6 | 2218 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2219 | `{` |
|      - | 2220 | `	const char *zString,*zEnd;` |
|      - | 2221 | `	int nLen,c;` |
|      7 | 2222 | `	if( nArg < 1 ){` |
|      - | 2223 | `		/* Missing arguments,return null */` |
|      3 | 2224 | `		ph7_result_null(pCtx);` |
|      3 | 2225 | `		return PH7_OK;` |
|      - | 2226 | `	}` |
|      - | 2227 | `	/* Extract the target string */` |
|      5 | 2228 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2229 | `	if( nLen < 1 ){` |
|      - | 2230 | `		/* Empty string,return */` |
|      3 | 2231 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2232 | `		return PH7_OK;` |
|      - | 2233 | `	}` |
|      - | 2234 | `	/* Perform the requested operation */` |
|      3 | 2235 | `	zEnd = &zString[nLen];` |
|      3 | 2236 | `	c = zString[0];` |
|      3 | 2237 | `	if( SyisLower(c) ){` |
|      3 | 2238 | `		c = SyToUpper(c);` |
|      1 | 2239 | `	}` |
|      - | 2240 | `	/* Append the first character */` |
|      3 | 2241 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2242 | `	zString++;` |
|      3 | 2243 | `	if( zString < zEnd ){` |
|      - | 2244 | `		/* Append the rest of the input verbatim */` |
|      3 | 2245 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2246 | `	}` |
|      3 | 2247 | `	return PH7_OK;` |
|      4 | 2248 | `}` |
|      - | 2249 | `/*` |
|      - | 2250 | ` * string lcfirst(string $str)` |
|      - | 2251 | ` *  Make a string's first character lowercase.` |
|      - | 2252 | ` * Parameters` |
|      - | 2253 | ` *  $str` |
|      - | 2254 | ` *   The input string.` |
|      - | 2255 | ` * Returns.` |
|      - | 2256 | ` *  The processed string.` |
|      - | 2257 | ` */` |
|      6 | 2258 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2259 | `{` |
|      - | 2260 | `	const char *zString,*zEnd;` |
|      - | 2261 | `	int nLen,c;` |
|      7 | 2262 | `	if( nArg < 1 ){` |
|      - | 2263 | `		/* Missing arguments,return null */` |
|      3 | 2264 | `		ph7_result_null(pCtx);` |
|      3 | 2265 | `		return PH7_OK;` |
|      - | 2266 | `	}` |
|      - | 2267 | `	/* Extract the target string */` |
|      5 | 2268 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2269 | `	if( nLen < 1 ){` |
|      - | 2270 | `		/* Empty string,return */` |
|      3 | 2271 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2272 | `		return PH7_OK;` |
|      - | 2273 | `	}` |
|      - | 2274 | `	/* Perform the requested operation */` |
|      3 | 2275 | `	zEnd = &zString[nLen];` |
|      3 | 2276 | `	c = zString[0];` |
|      3 | 2277 | `	if( SyisUpper(c) ){` |
|      3 | 2278 | `		c = SyToLower(c);` |
|      1 | 2279 | `	}` |
|      - | 2280 | `	/* Append the first character */` |
|      3 | 2281 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2282 | `	zString++;` |
|      3 | 2283 | `	if( zString < zEnd ){` |
|      - | 2284 | `		/* Append the rest of the input verbatim */` |
|      3 | 2285 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2286 | `	}` |
|      3 | 2287 | `	return PH7_OK;` |
|      4 | 2288 | `}` |
|      - | 2289 | `/*` |
|      - | 2290 | ` * int ord(string $string)` |
|      - | 2291 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 2292 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 2293 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 2294 | ` * Parameters` |
|      - | 2295 | ` *  $string` |
|      - | 2296 | ` *   The input string.` |
|      - | 2297 | ` * Returns` |
|      - | 2298 | ` *  The ASCII value as an integer.` |
|      - | 2299 | ` */` |
|     62 | 2300 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2301 | `{` |
|      - | 2302 | `	const char *zString;` |
|      - | 2303 | `	int nLen,c;` |
|      - | 2304 | `	/* PHP requires exactly one argument. */` |
|     65 | 2305 | `	if( nArg != 1 ){` |
|      8 | 2306 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2307 | `			"ArgumentCountError",` |
|      - | 2308 | `			"ord() expects exactly 1 argument, %d given",` |
|      2 | 2309 | `			nArg` |
|      - | 2310 | `			);` |
|      - | 2311 | `	}` |
|      - | 2312 | `	/* Passing null is deprecated (E_DEPRECATED).  PHP emits this before` |
|      - | 2313 | `	 * the empty-string deprecation, so we check null first. */` |
|     59 | 2314 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 2315 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2316 | `			"ord(): Passing null to parameter #1 ($character) "` |
|      - | 2317 | `			"of type string is deprecated"` |
|      - | 2318 | `			);` |
|      1 | 2319 | `	}` |
|      - | 2320 | `	/* Extract the target string */` |
|     59 | 2321 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     59 | 2322 | `	if( nLen < 1 ){` |
|      - | 2323 | `		/* Empty string is deprecated (E_DEPRECATED). */` |
|      5 | 2324 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2325 | `			"ord(): Providing an empty string is deprecated"` |
|      - | 2326 | `			);` |
|      5 | 2327 | `		ph7_result_int(pCtx,0);` |
|      5 | 2328 | `		return PH7_OK;` |
|      - | 2329 | `	}` |
|      - | 2330 | `	/* A string longer than one byte is deprecated (E_DEPRECATED). */` |
|     55 | 2331 | `	if( nLen > 1 ){` |
|      7 | 2332 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2333 | `			"ord(): Providing a string that is not one byte long "` |
|      - | 2334 | `			"is deprecated. Use ord($str[0]) instead"` |
|      - | 2335 | `			);` |
|      3 | 2336 | `	}` |
|      - | 2337 | `	/* Extract the ASCII value of the first character */` |
|     55 | 2338 | `	c = (unsigned char)zString[0];` |
|      - | 2339 | `	/* Return that value */` |
|     55 | 2340 | `	ph7_result_int(pCtx,c);` |
|     55 | 2341 | `	return PH7_OK;` |
|     34 | 2342 | `}` |
|      - | 2343 | `/*` |
|      - | 2344 | ` * string chr(int $codepoint)` |
|      - | 2345 | ` *  Returns a one-character string containing the character specified` |
|      - | 2346 | ` *  by the given codepoint.  Any integer is accepted; values outside` |
|      - | 2347 | ` *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.` |
|      - | 2348 | ` * Parameters` |
|      - | 2349 | ` *  $codepoint` |
|      - | 2350 | ` *   An integer codepoint.  Values outside 0-255 are deprecated and` |
|      - | 2351 | ` *   will be constrained to a single byte.` |
|      - | 2352 | ` * Returns` |
|      - | 2353 | ` *  A single-character string.` |
|      - | 2354 | ` */` |
|     48 | 2355 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2356 | `{` |
|      - | 2357 | `	int c;` |
|      - | 2358 | `	unsigned char ch;` |
|      - | 2359 | `	/* PHP requires exactly one argument. */` |
|     51 | 2360 | `	if( nArg != 1 ){` |
|      8 | 2361 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2362 | `			"ArgumentCountError",` |
|      - | 2363 | `			"chr() expects exactly 1 argument, %d given",` |
|      2 | 2364 | `			nArg` |
|      - | 2365 | `			);` |
|      - | 2366 | `	}` |
|      - | 2367 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 2368 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 2369 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 2370 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|     45 | 2371 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      - | 2372 | `		char zBuf[120];` |
|      4 | 2373 | `		SyBufferFormat(zBuf,sizeof(zBuf),` |
|      - | 2374 | `			"Implicit conversion from float %g to int loses precision",` |
|      1 | 2375 | `			ph7_value_to_double(apArg[0])` |
|      - | 2376 | `			);` |
|      3 | 2377 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zBuf);` |
|      1 | 2378 | `	}` |
|      - | 2379 | `	/* Extract the codepoint. */` |
|     45 | 2380 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 2381 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 2382 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 2383 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 2384 | `	 * name to avoid the API double-prefixing it. */` |
|     45 | 2385 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 2386 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 2387 | `			E_DEPRECATED,` |
|      - | 2388 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 2389 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 2390 | `			"The value used will be constrained using % 256"` |
|      - | 2391 | `			);` |
|      2 | 2392 | `	}` |
|      - | 2393 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 2394 | `	 * when taking the address of a wider int. */` |
|     45 | 2395 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 2396 | `	/* Return the specified character */` |
|     45 | 2397 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|     45 | 2398 | `	return PH7_OK;` |
|     27 | 2399 | `}` |
|      - | 2400 | `/*` |
|      - | 2401 | ` * Binary to hex consumer callback.` |
|      - | 2402 | ` * This callback is the default consumer used by the hash functions` |
|      - | 2403 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 2404 | ` */` |
|   2924 | 2405 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 2406 | `{` |
|      - | 2407 | `	/* Append hex chunk verbatim */` |
|   2925 | 2408 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   2925 | 2409 | `	return SXRET_OK;` |
|      1 | 2410 | `}` |
|      - | 2411 |  |
|      - | 2412 | `/*` |
|      - | 2413 | ` * string bin2hex(string $str)` |
|      - | 2414 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 2415 | ` * Parameters` |
|      - | 2416 | ` *  $str` |
|      - | 2417 | ` *   The input string.` |
|      - | 2418 | ` * Returns.` |
|      - | 2419 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 2420 | ` */` |
|     72 | 2421 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2422 | `{` |
|      - | 2423 | `	const char *zString;` |
|      - | 2424 | `	int nLen;` |
|      - | 2425 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|     77 | 2426 | `	if( nArg != 1 ){` |
|      8 | 2427 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2428 | `			"ArgumentCountError",` |
|      - | 2429 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|      2 | 2430 | `			nArg` |
|      - | 2431 | `			);` |
|      - | 2432 | `	}` |
|      - | 2433 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 2434 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 2435 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 2436 | `	 */` |
|    105 | 2437 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|     64 | 2438 | `		( ph7_value_is_object(apArg[0]) &&` |
|      3 | 2439 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|      2 | 2440 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|      1 | 2441 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 2442 | `		)` |
|      - | 2443 | `	){` |
|      9 | 2444 | `		const char *zType = ph7_type_name(apArg[0]);` |
|      9 | 2445 | `		if( ph7_value_is_object(apArg[0]) ){` |
|      3 | 2446 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      3 | 2447 | `			if( pInst && pInst->pClass ){` |
|      3 | 2448 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      1 | 2449 | `			}` |
|      1 | 2450 | `		}` |
|     12 | 2451 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2452 | `			"TypeError",` |
|      - | 2453 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|      3 | 2454 | `			zType` |
|      - | 2455 | `			);` |
|      - | 2456 | `	}` |
|      - | 2457 | `	/* Extract the target string */` |
|     63 | 2458 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     63 | 2459 | `	if( nLen < 1 ){` |
|      - | 2460 | `		/* Empty string,return */` |
|      9 | 2461 | `		ph7_result_string(pCtx,"",0);` |
|      9 | 2462 | `		return PH7_OK;` |
|      - | 2463 | `	}` |
|      - | 2464 | `	/* Perform the requested operation */` |
|     55 | 2465 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|     55 | 2466 | `	return PH7_OK;` |
|     41 | 2467 | `}` |
|      - | 2468 |  |
|      - | 2469 | `/* Search callback signature */` |
|      - | 2470 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 2471 | `/*` |
|      - | 2472 | ` * Case-insensitive pattern match.` |
|      - | 2473 | ` * Brute force is the default search method used here.` |
|      - | 2474 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 2475 | ` * well for short/medium texts on modern hardware.` |
|      - | 2476 | ` */` |
|    118 | 2477 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      1 | 2478 | `{` |
|    119 | 2479 | `	const char *zpIn = (const char *)pPattern;` |
|    119 | 2480 | `	const char *zIn = (const char *)pText;` |
|    119 | 2481 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    119 | 2482 | `	const char *zEnd = &zIn[nLen];` |
|      - | 2483 | `	const char *zPtr,*zPtr2;` |
|      - | 2484 | `	int c,d;` |
|    119 | 2485 | `	if( iPatLen > nLen ){` |
|      - | 2486 | `		/* Don't bother processing */` |
|     33 | 2487 | `		return SXERR_NOTFOUND;` |
|      - | 2488 | `	}` |
|    242 | 2489 | `	for(;;){` |
|    485 | 2490 | `		if( zIn >= zEnd ){` |
|     47 | 2491 | `			break;` |
|      - | 2492 | `		}` |
|    439 | 2493 | `		c = SyToLower(zIn[0]);` |
|    439 | 2494 | `		d = SyToLower(zpIn[0]);` |
|    439 | 2495 | `		if( c == d ){` |
|     41 | 2496 | `			zPtr   = &zIn[1];` |
|     41 | 2497 | `			zPtr2  = &zpIn[1];` |
|     71 | 2498 | `			for(;;){` |
|    143 | 2499 | `				if( zPtr2 >= zpEnd ){` |
|      - | 2500 | `					/* Pattern found */` |
|     41 | 2501 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 2502 | `					return SXRET_OK;` |
|      - | 2503 | `				}` |
|    103 | 2504 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 2505 | `					break;` |
|      - | 2506 | `				}` |
|    103 | 2507 | `				c = SyToLower(zPtr[0]);` |
|    103 | 2508 | `				d = SyToLower(zPtr2[0]);` |
|    103 | 2509 | `				if( c != d ){` |
|    ! 0 | 2510 | `					break;` |
|      - | 2511 | `				}` |
|    103 | 2512 | `				zPtr++; zPtr2++;` |
|      1 | 2513 | `			}` |
|    ! 0 | 2514 | `		}` |
|    399 | 2515 | `		zIn++;` |
|      1 | 2516 | `	}` |
|      - | 2517 | `	/* Pattern not found */` |
|     47 | 2518 | `	return SXERR_NOTFOUND;` |
|     60 | 2519 | `}` |
|      - | 2520 | `/*` |
|      - | 2521 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2522 | ` *  Find the first occurrence of a string.` |
|      - | 2523 | ` * Parameters` |
|      - | 2524 | ` *  $haystack` |
|      - | 2525 | ` *   The input string.` |
|      - | 2526 | ` * $needle` |
|      - | 2527 | ` *   Search pattern (must be a string).` |
|      - | 2528 | ` * $before_needle` |
|      - | 2529 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2530 | ` *   of the needle (excluding the needle).` |
|      - | 2531 | ` * Return` |
|      - | 2532 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2533 | ` */` |
|     10 | 2534 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2535 | `{` |
|     11 | 2536 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2537 | `	const char *zBlob,*zPattern;` |
|      - | 2538 | `	int nLen,nPatLen;` |
|      - | 2539 | `	sxu32 nOfft;` |
|      - | 2540 | `	sxi32 rc;` |
|     11 | 2541 | `	if( nArg < 2 ){` |
|      - | 2542 | `		/* Missing arguments,return FALSE */` |
|      5 | 2543 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2544 | `		return PH7_OK;` |
|      - | 2545 | `	}` |
|      - | 2546 | `	/* Extract the needle and the haystack */` |
|      7 | 2547 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 2548 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 2549 | `	nOfft = 0; /* cc warning */` |
|      9 | 2550 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 2551 | `		int before = 0;` |
|      - | 2552 | `		/* Perform the lookup */` |
|      5 | 2553 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2554 | `		if( rc != SXRET_OK ){` |
|      - | 2555 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2556 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2557 | `			return PH7_OK;` |
|      - | 2558 | `		}` |
|      - | 2559 | `		/* Return the portion of the string */` |
|      5 | 2560 | `		if( nArg > 2 ){` |
|      3 | 2561 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2562 | `		}` |
|      5 | 2563 | `		if( before ){` |
|      3 | 2564 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2565 | `		}else{` |
|      3 | 2566 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2567 | `		}` |
|      3 | 2568 | `	}else{` |
|      3 | 2569 | `		ph7_result_bool(pCtx,0);` |
|      - | 2570 | `	}` |
|      7 | 2571 | `	return PH7_OK;` |
|      6 | 2572 | `}` |
|      - | 2573 | `/*` |
|      - | 2574 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2575 | ` *  Case-insensitive strstr().` |
|      - | 2576 | ` * Parameters` |
|      - | 2577 | ` *  $haystack` |
|      - | 2578 | ` *   The input string.` |
|      - | 2579 | ` * $needle` |
|      - | 2580 | ` *   Search pattern (must be a string).` |
|      - | 2581 | ` * $before_needle` |
|      - | 2582 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2583 | ` *   of the needle (excluding the needle).` |
|      - | 2584 | ` * Return` |
|      - | 2585 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2586 | ` */` |
|      6 | 2587 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2588 | `{` |
|      7 | 2589 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2590 | `	const char *zBlob,*zPattern;` |
|      - | 2591 | `	int nLen,nPatLen;` |
|      - | 2592 | `	sxu32 nOfft;` |
|      - | 2593 | `	sxi32 rc;` |
|      7 | 2594 | `	if( nArg < 2 ){` |
|      - | 2595 | `		/* Missing arguments,return FALSE */` |
|      3 | 2596 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2597 | `		return PH7_OK;` |
|      - | 2598 | `	}` |
|      - | 2599 | `	/* Extract the needle and the haystack */` |
|      5 | 2600 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2601 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 2602 | `	nOfft = 0; /* cc warning */` |
|      7 | 2603 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 2604 | `		int before = 0;` |
|      - | 2605 | `		/* Perform the lookup */` |
|      5 | 2606 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2607 | `		if( rc != SXRET_OK ){` |
|      - | 2608 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2609 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2610 | `			return PH7_OK;` |
|      - | 2611 | `		}` |
|      - | 2612 | `		/* Return the portion of the string */` |
|      5 | 2613 | `		if( nArg > 2 ){` |
|      3 | 2614 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2615 | `		}` |
|      5 | 2616 | `		if( before ){` |
|      3 | 2617 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2618 | `		}else{` |
|      3 | 2619 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2620 | `		}` |
|      3 | 2621 | `	}else{` |
|    ! 0 | 2622 | `		ph7_result_bool(pCtx,0);` |
|      - | 2623 | `	}` |
|      5 | 2624 | `	return PH7_OK;` |
|      4 | 2625 | `}` |
|      - | 2626 | `/*` |
|      - | 2627 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2628 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 2629 | ` * Parameters` |
|      - | 2630 | ` *  $haystack` |
|      - | 2631 | ` *   The input string.` |
|      - | 2632 | ` * $needle` |
|      - | 2633 | ` *   Search pattern (must be a string).` |
|      - | 2634 | ` * $offset` |
|      - | 2635 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 2636 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 2637 | ` *   of haystack.` |
|      - | 2638 | ` * Return` |
|      - | 2639 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 2640 | ` */` |
|    124 | 2641 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2642 | `{` |
|    129 | 2643 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2644 | `	const char *zBlob,*zPattern;` |
|      - | 2645 | `	int nLen,nPatLen,nStart;` |
|      - | 2646 | `	sxu32 nOfft;` |
|      - | 2647 | `	sxi32 rc;` |
|    129 | 2648 | `	if( nArg < 2 ){` |
|      - | 2649 | `		/* Missing arguments,return FALSE */` |
|      7 | 2650 | `		ph7_result_bool(pCtx,0);` |
|      7 | 2651 | `		return PH7_OK;` |
|      - | 2652 | `	}` |
|      - | 2653 | `	/* Extract the needle and the haystack */` |
|    123 | 2654 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    123 | 2655 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    123 | 2656 | `	nOfft = 0; /* cc warning */` |
|    123 | 2657 | `	nStart = 0;` |
|      - | 2658 | `	/* Peek the starting offset if available */` |
|    123 | 2659 | `	if( nArg > 2 ){` |
|    ! 0 | 2660 | `		nStart = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 2661 | `		if( nStart < 0 ){` |
|    ! 0 | 2662 | `			nStart = -nStart;` |
|    ! 0 | 2663 | `		}` |
|    ! 0 | 2664 | `		if( nStart >= nLen ){` |
|      - | 2665 | `			/* Invalid offset */` |
|    ! 0 | 2666 | `			nStart = 0;` |
|    ! 0 | 2667 | `		}else{` |
|    ! 0 | 2668 | `			zBlob += nStart;` |
|    ! 0 | 2669 | `			nLen -= nStart;` |
|      - | 2670 | `		}` |
|    ! 0 | 2671 | `	}` |
|    123 | 2672 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2673 | `		/* Perform the lookup */` |
|    121 | 2674 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    121 | 2675 | `		if( rc != SXRET_OK ){` |
|      - | 2676 | `			/* Pattern not found,return FALSE */` |
|     33 | 2677 | `			ph7_result_bool(pCtx,0);` |
|     33 | 2678 | `			return PH7_OK;` |
|      - | 2679 | `		}` |
|      - | 2680 | `		/* Return the pattern position */` |
|     90 | 2681 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|     46 | 2682 | `	}else{` |
|      3 | 2683 | `		ph7_result_bool(pCtx,0);` |
|      - | 2684 | `	}` |
|     92 | 2685 | `	return PH7_OK;` |
|     67 | 2686 | `}` |
|      - | 2687 | `/*` |
|      - | 2688 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|      - | 2689 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|      - | 2690 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|      - | 2691 | ` * TypeError for arrays, resources, and objects without __toString.` |
|      - | 2692 | ` *` |
|      - | 2693 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|      - | 2694 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|      - | 2695 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|      - | 2696 | ` *` |
|      - | 2697 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|      - | 2698 | ` * is valid until pTmp is released or pArg is mutated.` |
|      - | 2699 | ` */` |
|    418 | 2700 | `static sxi32 StrPredicateResolveArg(` |
|      - | 2701 | `	ph7_context *pCtx,` |
|      - | 2702 | `	ph7_value *pArg,` |
|      - | 2703 | `	const char *zFunc,` |
|      - | 2704 | `	int iArgNum,` |
|      - | 2705 | `	const char *zParamName,` |
|      - | 2706 | `	const char *zNullMsg,` |
|      - | 2707 | `	ph7_value *pTmp,` |
|      - | 2708 | `	const char **pzOut,` |
|      - | 2709 | `	int *pnOut` |
|      4 | 2710 | `){` |
|    422 | 2711 | `	if( ph7_value_is_null(pArg) ){` |
|     13 | 2712 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zNullMsg);` |
|     13 | 2713 | `		*pzOut = "";` |
|     13 | 2714 | `		*pnOut = 0;` |
|     13 | 2715 | `		return PH7_OK;` |
|      - | 2716 | `	}` |
|    628 | 2717 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|    388 | 2718 | `	    ( ph7_value_is_object(pArg) &&` |
|     72 | 2719 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|     48 | 2720 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|     24 | 2721 | `	        "__toString",sizeof("__toString")-1) == 0` |
|      - | 2722 | `	    )` |
|      - | 2723 | `	){` |
|     34 | 2724 | `		const char *zType = ph7_type_name(pArg);` |
|     34 | 2725 | `		if( ph7_value_is_object(pArg) ){` |
|     13 | 2726 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     13 | 2727 | `			if( pInst && pInst->pClass ){` |
|     13 | 2728 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      6 | 2729 | `			}` |
|      6 | 2730 | `		}` |
|     49 | 2731 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2732 | `			"TypeError",` |
|      - | 2733 | `			"%s(): Argument #%d (%s) must be of type string, %s given",` |
|     15 | 2734 | `			zFunc, iArgNum, zParamName, zType` |
|      - | 2735 | `			);` |
|      - | 2736 | `	}` |
|    377 | 2737 | `	if( ph7_value_is_object(pArg) ){` |
|     37 | 2738 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     37 | 2739 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 2740 | `			"__toString",sizeof("__toString")-1);` |
|     37 | 2741 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|     37 | 2742 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|     37 | 2743 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|     37 | 2744 | `		return PH7_OK;` |
|      - | 2745 | `	}` |
|    341 | 2746 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|    341 | 2747 | `	return PH7_OK;` |
|    213 | 2748 | `}` |
|      - | 2749 | `/*` |
|      - | 2750 | ` * bool str_contains(string $haystack, string $needle)` |
|      - | 2751 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|      - | 2752 | ` * Return` |
|      - | 2753 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|      - | 2754 | ` */` |
|     92 | 2755 | `static int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2756 | `{` |
|      - | 2757 | `	const char *zHaystack,*zNeedle;` |
|      - | 2758 | `	int nHayLen,nNeedleLen;` |
|      - | 2759 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2760 | `	sxi32 rc;` |
|     96 | 2761 | `	if( nArg != 2 ){` |
|     18 | 2762 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2763 | `			"ArgumentCountError",` |
|      - | 2764 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|      5 | 2765 | `			nArg` |
|      - | 2766 | `			);` |
|      - | 2767 | `	}` |
|     84 | 2768 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     84 | 2769 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     84 | 2770 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack",` |
|      - | 2771 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|      - | 2772 | `		"of type string is deprecated",` |
|      - | 2773 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     84 | 2774 | `	if( rc != PH7_OK ) goto out;` |
|     77 | 2775 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle",` |
|      - | 2776 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|      - | 2777 | `		"of type string is deprecated",` |
|      - | 2778 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     77 | 2779 | `	if( rc != PH7_OK ) goto out;` |
|     73 | 2780 | `	if( nNeedleLen < 1 ){` |
|     13 | 2781 | `		ph7_result_bool(pCtx,1);` |
|     67 | 2782 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2783 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2784 | `	}else{` |
|     79 | 2785 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|     26 | 2786 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|     53 | 2787 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|      - | 2788 | `	}` |
|     73 | 2789 | `	rc = PH7_OK;` |
|     41 | 2790 | `out:` |
|     84 | 2791 | `	PH7_MemObjRelease(&sHayTmp);` |
|     84 | 2792 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     84 | 2793 | `	return rc;` |
|     50 | 2794 | `}` |
|      - | 2795 | `/*` |
|      - | 2796 | ` * bool str_starts_with(string $haystack, string $needle)` |
|      - | 2797 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|      - | 2798 | ` * Return` |
|      - | 2799 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|      - | 2800 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 2801 | ` */` |
|     78 | 2802 | `static int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2803 | `{` |
|      - | 2804 | `	const char *zHaystack,*zNeedle;` |
|      - | 2805 | `	int nHayLen,nNeedleLen;` |
|      - | 2806 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2807 | `	sxi32 rc;` |
|     82 | 2808 | `	if( nArg != 2 ){` |
|     18 | 2809 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2810 | `			"ArgumentCountError",` |
|      - | 2811 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|      5 | 2812 | `			nArg` |
|      - | 2813 | `			);` |
|      - | 2814 | `	}` |
|     70 | 2815 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 2816 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 2817 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack",` |
|      - | 2818 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 2819 | `		"of type string is deprecated",` |
|      - | 2820 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 2821 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 2822 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle",` |
|      - | 2823 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|      - | 2824 | `		"of type string is deprecated",` |
|      - | 2825 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 2826 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 2827 | `	if( nNeedleLen < 1 ){` |
|     13 | 2828 | `		ph7_result_bool(pCtx,1);` |
|     53 | 2829 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2830 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2831 | `	}else{` |
|     58 | 2832 | `		ph7_result_bool(pCtx,` |
|     38 | 2833 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 2834 | `	}` |
|     59 | 2835 | `	rc = PH7_OK;` |
|     34 | 2836 | `out:` |
|     70 | 2837 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 2838 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 2839 | `	return rc;` |
|     43 | 2840 | `}` |
|      - | 2841 | `/*` |
|      - | 2842 | ` * bool str_ends_with(string $haystack, string $needle)` |
|      - | 2843 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|      - | 2844 | ` * Return` |
|      - | 2845 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|      - | 2846 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 2847 | ` */` |
|     78 | 2848 | `static int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2849 | `{` |
|      - | 2850 | `	const char *zHaystack,*zNeedle;` |
|      - | 2851 | `	int nHayLen,nNeedleLen;` |
|      - | 2852 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2853 | `	sxi32 rc;` |
|     82 | 2854 | `	if( nArg != 2 ){` |
|     18 | 2855 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2856 | `			"ArgumentCountError",` |
|      - | 2857 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|      5 | 2858 | `			nArg` |
|      - | 2859 | `			);` |
|      - | 2860 | `	}` |
|     70 | 2861 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 2862 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 2863 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack",` |
|      - | 2864 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 2865 | `		"of type string is deprecated",` |
|      - | 2866 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 2867 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 2868 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle",` |
|      - | 2869 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|      - | 2870 | `		"of type string is deprecated",` |
|      - | 2871 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 2872 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 2873 | `	if( nNeedleLen < 1 ){` |
|     13 | 2874 | `		ph7_result_bool(pCtx,1);` |
|     53 | 2875 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2876 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2877 | `	}else{` |
|     58 | 2878 | `		ph7_result_bool(pCtx,` |
|     38 | 2879 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 2880 | `	}` |
|     59 | 2881 | `	rc = PH7_OK;` |
|     34 | 2882 | `out:` |
|     70 | 2883 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 2884 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 2885 | `	return rc;` |
|     43 | 2886 | `}` |
|      - | 2887 | `/*` |
|      - | 2888 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2889 | ` *  Case-insensitive strpos.` |
|      - | 2890 | ` * Parameters` |
|      - | 2891 | ` *  $haystack` |
|      - | 2892 | ` *   The input string.` |
|      - | 2893 | ` * $needle` |
|      - | 2894 | ` *   Search pattern (must be a string).` |
|      - | 2895 | ` * $offset` |
|      - | 2896 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 2897 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 2898 | ` *   of haystack.` |
|      - | 2899 | ` * Return` |
|      - | 2900 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 2901 | ` */` |
|     18 | 2902 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2903 | `{` |
|     19 | 2904 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2905 | `	const char *zBlob,*zPattern;` |
|      - | 2906 | `	int nLen,nPatLen,nStart;` |
|      - | 2907 | `	sxu32 nOfft;` |
|      - | 2908 | `	sxi32 rc;` |
|     19 | 2909 | `	if( nArg < 2 ){` |
|      - | 2910 | `		/* Missing arguments,return FALSE */` |
|      3 | 2911 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2912 | `		return PH7_OK;` |
|      - | 2913 | `	}` |
|      - | 2914 | `	/* Extract the needle and the haystack */` |
|     17 | 2915 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 2916 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     17 | 2917 | `	nOfft = 0; /* cc warning */` |
|     17 | 2918 | `	nStart = 0;` |
|      - | 2919 | `	/* Peek the starting offset if available */` |
|     17 | 2920 | `	if( nArg > 2 ){` |
|      5 | 2921 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 2922 | `		if( nStart < 0 ){` |
|      3 | 2923 | `			nStart = -nStart;` |
|      1 | 2924 | `		}` |
|      5 | 2925 | `		if( nStart >= nLen ){` |
|      - | 2926 | `			/* Invalid offset */` |
|    ! 0 | 2927 | `			nStart = 0;` |
|    ! 0 | 2928 | `		}else{` |
|      5 | 2929 | `			zBlob += nStart;` |
|      5 | 2930 | `			nLen -= nStart;` |
|      - | 2931 | `		}` |
|      2 | 2932 | `	}` |
|     17 | 2933 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2934 | `		/* Perform the lookup */` |
|     17 | 2935 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     17 | 2936 | `		if( rc != SXRET_OK ){` |
|      - | 2937 | `			/* Pattern not found,return FALSE */` |
|      3 | 2938 | `			ph7_result_bool(pCtx,0);` |
|      3 | 2939 | `			return PH7_OK;` |
|      - | 2940 | `		}` |
|      - | 2941 | `		/* Return the pattern position */` |
|     15 | 2942 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 2943 | `	}else{` |
|    ! 0 | 2944 | `		ph7_result_bool(pCtx,0);` |
|      - | 2945 | `	}` |
|     15 | 2946 | `	return PH7_OK;` |
|     10 | 2947 | `}` |
|      - | 2948 | `/*` |
|      - | 2949 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2950 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 2951 | ` * Parameters` |
|      - | 2952 | ` *  $haystack` |
|      - | 2953 | ` *   The input string.` |
|      - | 2954 | ` * $needle` |
|      - | 2955 | ` *   Search pattern (must be a string).` |
|      - | 2956 | ` * $offset` |
|      - | 2957 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 2958 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 2959 | ` *   characters from the end of the string, searching backwards.` |
|      - | 2960 | ` * Return` |
|      - | 2961 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 2962 | ` */` |
|     32 | 2963 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2964 | `{` |
|      - | 2965 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     33 | 2966 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2967 | `	int nLen,nPatLen;` |
|      - | 2968 | `	sxu32 nOfft;` |
|      - | 2969 | `	sxi32 rc;` |
|     33 | 2970 | `	if( nArg < 2 ){` |
|      - | 2971 | `		/* Missing arguments,return FALSE */` |
|      3 | 2972 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2973 | `		return PH7_OK;` |
|      - | 2974 | `	}` |
|      - | 2975 | `	/* Extract the needle and the haystack */` |
|     31 | 2976 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 2977 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 2978 | `	/* Point to the end of the pattern */` |
|     31 | 2979 | `	zPtr = &zBlob[nLen - 1];` |
|     31 | 2980 | `	zEnd = &zBlob[nLen];` |
|      - | 2981 | `	/* Save the starting posistion */` |
|     31 | 2982 | `	zStart = zBlob;` |
|     31 | 2983 | `	nOfft = 0; /* cc warning */` |
|      - | 2984 | `	/* Peek the starting offset if available */` |
|     31 | 2985 | `	if( nArg > 2 ){` |
|      - | 2986 | `		int nStart;` |
|     21 | 2987 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 2988 | `		if( nStart < 0 ){` |
|     11 | 2989 | `			nStart = -nStart;` |
|     11 | 2990 | `			if( nStart >= nLen ){` |
|      - | 2991 | `				/* Invalid offset */` |
|      3 | 2992 | `				ph7_result_bool(pCtx,0);` |
|      3 | 2993 | `				return PH7_OK;` |
|    ! 0 | 2994 | `			}else{` |
|      9 | 2995 | `				nLen -= nStart;` |
|      9 | 2996 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 2997 | `				zEnd = &zBlob[nLen];` |
|      - | 2998 | `			}` |
|      5 | 2999 | `		}else{` |
|     11 | 3000 | `			if( nStart >= nLen ){` |
|      - | 3001 | `				/* Invalid offset */` |
|      5 | 3002 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3003 | `				return PH7_OK;` |
|    ! 0 | 3004 | `			}else{` |
|      7 | 3005 | `				zBlob += nStart;` |
|      7 | 3006 | `				nLen -= nStart;` |
|      - | 3007 | `			}` |
|      - | 3008 | `		}` |
|      7 | 3009 | `	}` |
|     25 | 3010 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3011 | `		/* Perform the lookup */` |
|     57 | 3012 | `		for(;;){` |
|    115 | 3013 | `			if( zBlob >= zPtr ){` |
|     11 | 3014 | `				break;` |
|      - | 3015 | `			}` |
|    105 | 3016 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    105 | 3017 | `			if( rc == SXRET_OK ){` |
|      - | 3018 | `				/* Pattern found,return it's position */` |
|     13 | 3019 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 3020 | `				return PH7_OK;` |
|      - | 3021 | `			}` |
|     93 | 3022 | `			zPtr--;` |
|      1 | 3023 | `		}` |
|      - | 3024 | `		/* Pattern not found,return FALSE */` |
|     11 | 3025 | `		ph7_result_bool(pCtx,0);` |
|      6 | 3026 | `	}else{` |
|      3 | 3027 | `		ph7_result_bool(pCtx,0);` |
|      - | 3028 | `	}` |
|     13 | 3029 | `	return PH7_OK;` |
|     17 | 3030 | `}` |
|      - | 3031 | `/*` |
|      - | 3032 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3033 | ` *  Case-insensitive strrpos.` |
|      - | 3034 | ` * Parameters` |
|      - | 3035 | ` *  $haystack` |
|      - | 3036 | ` *   The input string.` |
|      - | 3037 | ` * $needle` |
|      - | 3038 | ` *   Search pattern (must be a string).` |
|      - | 3039 | ` * $offset` |
|      - | 3040 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3041 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3042 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3043 | ` * Return` |
|      - | 3044 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3045 | ` */` |
|     28 | 3046 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3047 | `{` |
|      - | 3048 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     29 | 3049 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3050 | `	int nLen,nPatLen;` |
|      - | 3051 | `	sxu32 nOfft;` |
|      - | 3052 | `	sxi32 rc;` |
|     29 | 3053 | `	if( nArg < 2 ){` |
|      - | 3054 | `		/* Missing arguments,return FALSE */` |
|      3 | 3055 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3056 | `		return PH7_OK;` |
|      - | 3057 | `	}` |
|      - | 3058 | `	/* Extract the needle and the haystack */` |
|     27 | 3059 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 3060 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3061 | `	/* Point to the end of the pattern */` |
|     27 | 3062 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 3063 | `	zEnd = &zBlob[nLen];` |
|      - | 3064 | `	/* Save the starting posistion */` |
|     27 | 3065 | `	zStart = zBlob;` |
|     27 | 3066 | `	nOfft = 0; /* cc warning */` |
|      - | 3067 | `	/* Peek the starting offset if available */` |
|     27 | 3068 | `	if( nArg > 2 ){` |
|      - | 3069 | `		int nStart;` |
|     15 | 3070 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3071 | `		if( nStart < 0 ){` |
|      7 | 3072 | `			nStart = -nStart;` |
|      7 | 3073 | `			if( nStart >= nLen ){` |
|      - | 3074 | `				/* Invalid offset */` |
|      3 | 3075 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3076 | `				return PH7_OK;` |
|    ! 0 | 3077 | `			}else{` |
|      5 | 3078 | `				nLen -= nStart;` |
|      5 | 3079 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 3080 | `				zEnd = &zBlob[nLen];` |
|      - | 3081 | `			}` |
|      3 | 3082 | `		}else{` |
|      9 | 3083 | `			if( nStart >= nLen ){` |
|      - | 3084 | `				/* Invalid offset */` |
|      5 | 3085 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3086 | `				return PH7_OK;` |
|    ! 0 | 3087 | `			}else{` |
|      5 | 3088 | `				zBlob += nStart;` |
|      5 | 3089 | `				nLen -= nStart;` |
|      - | 3090 | `			}` |
|      - | 3091 | `		}` |
|      4 | 3092 | `	}` |
|     21 | 3093 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3094 | `		/* Perform the lookup */` |
|     44 | 3095 | `		for(;;){` |
|     89 | 3096 | `			if( zBlob >= zPtr ){` |
|      9 | 3097 | `				break;` |
|      - | 3098 | `			}` |
|     81 | 3099 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 3100 | `			if( rc == SXRET_OK ){` |
|      - | 3101 | `				/* Pattern found,return it's position */` |
|     11 | 3102 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 3103 | `				return PH7_OK;` |
|      - | 3104 | `			}` |
|     71 | 3105 | `			zPtr--;` |
|      1 | 3106 | `		}` |
|      - | 3107 | `		/* Pattern not found,return FALSE */` |
|      9 | 3108 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3109 | `	}else{` |
|      3 | 3110 | `		ph7_result_bool(pCtx,0);` |
|      - | 3111 | `	}` |
|     11 | 3112 | `	return PH7_OK;` |
|     15 | 3113 | `}` |
|      - | 3114 | `/*` |
|      - | 3115 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3116 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3117 | ` * Parameters` |
|      - | 3118 | ` *  $haystack` |
|      - | 3119 | ` *   The input string.` |
|      - | 3120 | ` * $needle` |
|      - | 3121 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3122 | ` *  This behavior is different from that of strstr().` |
|      - | 3123 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3124 | ` *  as the ordinal value of a character.` |
|      - | 3125 | ` * Return` |
|      - | 3126 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3127 | ` */` |
|     24 | 3128 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3129 | `{` |
|      - | 3130 | `	const char *zBlob;` |
|      - | 3131 | `	int nLen,c;` |
|     25 | 3132 | `	if( nArg < 2 ){` |
|      - | 3133 | `		/* Missing arguments,return FALSE */` |
|      3 | 3134 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3135 | `		return PH7_OK;` |
|      - | 3136 | `	}` |
|      - | 3137 | `	/* Extract the haystack */` |
|     23 | 3138 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3139 | `	c = 0; /* cc warning */` |
|     23 | 3140 | `	if( nLen > 0 ){` |
|      - | 3141 | `		sxu32 nOfft;` |
|      - | 3142 | `		sxi32 rc;` |
|     21 | 3143 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 3144 | `			const char *zPattern;` |
|     11 | 3145 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 3146 | `														 * for NULL pointer.` |
|      - | 3147 | `														 */` |
|     11 | 3148 | `			c = zPattern[0];` |
|      6 | 3149 | `		}else{` |
|      - | 3150 | `			/* Int cast */` |
|     11 | 3151 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 3152 | `		}` |
|      - | 3153 | `		/* Perform the lookup */` |
|     21 | 3154 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3155 | `		if( rc != SXRET_OK ){` |
|      - | 3156 | `			/* No such entry,return FALSE */` |
|      7 | 3157 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3158 | `			return PH7_OK;` |
|      - | 3159 | `		}` |
|      - | 3160 | `		/* Return the string portion */` |
|     15 | 3161 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 3162 | `	}else{` |
|      3 | 3163 | `		ph7_result_bool(pCtx,0);` |
|      - | 3164 | `	}` |
|     17 | 3165 | `	return PH7_OK;` |
|     13 | 3166 | `}` |
|      - | 3167 | `/*` |
|      - | 3168 | ` * string strrev(string $string)` |
|      - | 3169 | ` *  Reverse a string.` |
|      - | 3170 | ` * Parameters` |
|      - | 3171 | ` *  $string` |
|      - | 3172 | ` *   String to be reversed.` |
|      - | 3173 | ` * Return` |
|      - | 3174 | ` *  The reversed string.` |
|      - | 3175 | ` */` |
|      4 | 3176 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3177 | `{` |
|      - | 3178 | `	const char *zIn,*zEnd;` |
|      - | 3179 | `	int nLen,c;` |
|      5 | 3180 | `	if( nArg < 1 ){` |
|      - | 3181 | `		/* Missing arguments,return NULL */` |
|      3 | 3182 | `		ph7_result_null(pCtx);` |
|      3 | 3183 | `		return PH7_OK;` |
|      - | 3184 | `	}` |
|      - | 3185 | `	/* Extract the target string */` |
|      3 | 3186 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3187 | `	if( nLen < 1 ){` |
|      - | 3188 | `		/* Empty string Return null */` |
|    ! 0 | 3189 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3190 | `		return PH7_OK;` |
|      - | 3191 | `	}` |
|      - | 3192 | `	/* Perform the requested operation */` |
|      3 | 3193 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3194 | `	for(;;){` |
|      9 | 3195 | `		if( zEnd < zIn ){` |
|      - | 3196 | `			/* No more input to process */` |
|      3 | 3197 | `			break;` |
|      - | 3198 | `		}` |
|      - | 3199 | `		/* Append current character */` |
|      7 | 3200 | `		c = zEnd[0];` |
|      7 | 3201 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3202 | `		zEnd--;` |
|      1 | 3203 | `	}` |
|      3 | 3204 | `	return PH7_OK;` |
|      3 | 3205 | `}` |
|      - | 3206 | `/*` |
|      - | 3207 | ` * string ucwords(string $string)` |
|      - | 3208 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3209 | ` *  The definition of a word is any string of characters that is immediately after` |
|      - | 3210 | ` *  a whitespace (These are: space, form-feed, newline, carriage return, horizontal tab, and vertical tab).` |
|      - | 3211 | ` * Parameters` |
|      - | 3212 | ` *  $string` |
|      - | 3213 | ` *   The input string.` |
|      - | 3214 | ` * Return` |
|      - | 3215 | ` *  The modified string..` |
|      - | 3216 | ` */` |
|     14 | 3217 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3218 | `{` |
|      - | 3219 | `	const char *zIn,*zCur,*zEnd;` |
|      - | 3220 | `	int nLen,c;` |
|     15 | 3221 | `	if( nArg < 1 ){` |
|      - | 3222 | `		/* Missing arguments,return NULL */` |
|      3 | 3223 | `		ph7_result_null(pCtx);` |
|      3 | 3224 | `		return PH7_OK;` |
|      - | 3225 | `	}` |
|      - | 3226 | `	/* Extract the target string */` |
|     13 | 3227 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 3228 | `	if( nLen < 1 ){` |
|      - | 3229 | `		/* Empty string – match PHP semantics */` |
|      3 | 3230 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3231 | `		return PH7_OK;` |
|      - | 3232 | `	}` |
|      - | 3233 | `	/* Perform the requested operation */` |
|     11 | 3234 | `	zEnd = &zIn[nLen];` |
|     21 | 3235 | `	for(;;){` |
|      - | 3236 | `		/* Jump leading white spaces */` |
|     43 | 3237 | `		zCur = zIn;` |
|     65 | 3238 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0x80 && SyisSpace(zIn[0]) ){` |
|     23 | 3239 | `			zIn++;` |
|      1 | 3240 | `		}` |
|     43 | 3241 | `		if( zCur < zIn ){` |
|      - | 3242 | `			/* Append white space stream */` |
|     23 | 3243 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 3244 | `		}` |
|     43 | 3245 | `		if( zIn >= zEnd ){` |
|      - | 3246 | `			/* No more input to process */` |
|     11 | 3247 | `			break;` |
|      - | 3248 | `		}` |
|     33 | 3249 | `		c = zIn[0];` |
|     33 | 3250 | `		if( c < 0x80 && SyisLower(c) ){` |
|     29 | 3251 | `			c = SyToUpper(c);` |
|     14 | 3252 | `		}` |
|      - | 3253 | `		/* Append the upper-cased character */` |
|     33 | 3254 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     33 | 3255 | `		zIn++;` |
|     33 | 3256 | `		zCur = zIn;` |
|      - | 3257 | `		/* Append the word varbatim */` |
|    149 | 3258 | `		while( zIn < zEnd ){` |
|    139 | 3259 | `			if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 3260 | `				/* UTF-8 stream */` |
|    ! 0 | 3261 | `				zIn++;` |
|    ! 0 | 3262 | `				SX_JMP_UTF8(zIn,zEnd);` |
|    138 | 3263 | `			}else if( !SyisSpace(zIn[0]) ){` |
|    117 | 3264 | `				zIn++;` |
|     59 | 3265 | `			}else{` |
|     23 | 3266 | `				break;` |
|      - | 3267 | `			}` |
|      1 | 3268 | `		}` |
|     33 | 3269 | `		if( zCur < zIn ){` |
|     33 | 3270 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 | 3271 | `		}` |
|      1 | 3272 | `	}` |
|     11 | 3273 | `	return PH7_OK;` |
|      8 | 3274 | `}` |
|      - | 3275 | `/*` |
|      - | 3276 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3277 | ` *  Returns input repeated multiplier times.` |
|      - | 3278 | ` * Parameters` |
|      - | 3279 | ` *  $string` |
|      - | 3280 | ` *   String to be repeated.` |
|      - | 3281 | ` * $multiplier` |
|      - | 3282 | ` *  Number of time the input string should be repeated.` |
|      - | 3283 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3284 | ` *  to 0, the function will return an empty string.` |
|      - | 3285 | ` * Return` |
|      - | 3286 | ` *  The repeated string.` |
|      - | 3287 | ` */` |
|  20226 | 3288 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3289 | `{` |
|      - | 3290 | `	const char *zIn;` |
|      - | 3291 | `	int nLen,nMul;` |
|      - | 3292 | `	int rc;` |
|  20227 | 3293 | `	if( nArg < 2 ){` |
|      - | 3294 | `		/* Missing arguments,return NULL */` |
|      3 | 3295 | `		ph7_result_null(pCtx);` |
|      3 | 3296 | `		return PH7_OK;` |
|      - | 3297 | `	}` |
|      - | 3298 | `	/* Extract the target string */` |
|  20225 | 3299 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|  20225 | 3300 | `	if( nLen < 1 ){` |
|      - | 3301 | `		/* Empty string.Return null */` |
|    ! 0 | 3302 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3303 | `		return PH7_OK;` |
|      - | 3304 | `	}` |
|      - | 3305 | `	/* Extract the multiplier */` |
|  20225 | 3306 | `	nMul = ph7_value_to_int(apArg[1]);` |
|  20225 | 3307 | `	if( nMul < 1 ){` |
|      - | 3308 | `		/* Return the empty string */` |
|      3 | 3309 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3310 | `		return PH7_OK;` |
|      - | 3311 | `	}` |
|      - | 3312 | `	/* Perform the requested operation */` |
| 120878 | 3313 | `	for(;;){` |
| 241757 | 3314 | `		if( !nMul ){` |
|  20223 | 3315 | `			break;` |
|      - | 3316 | `		}` |
|      - | 3317 | `		/* Append the copy */` |
| 221535 | 3318 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 221535 | 3319 | `		if( rc != PH7_OK ){` |
|      - | 3320 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 3321 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 3322 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 3323 | `		}` |
| 221535 | 3324 | `		nMul--;` |
|      1 | 3325 | `	}` |
|  20223 | 3326 | `	return PH7_OK;` |
|  10114 | 3327 | `}` |
|      - | 3328 | `/*` |
|      - | 3329 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3330 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3331 | ` * Parameters` |
|      - | 3332 | ` *  $string` |
|      - | 3333 | ` *   The input string.` |
|      - | 3334 | ` * $is_xhtml` |
|      - | 3335 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3336 | ` * Return` |
|      - | 3337 | ` *  The processed string.` |
|      - | 3338 | ` */` |
|      6 | 3339 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3340 | `{` |
|      - | 3341 | `	const char *zIn,*zCur,*zEnd;` |
|      7 | 3342 | `	int is_xhtml = 1; /* Default to XHTML-style '<br/>' like PHP */` |
|      - | 3343 | `	int nLen;` |
|      7 | 3344 | `	if( nArg < 1 ){` |
|      - | 3345 | `		/* Missing arguments,return the empty string */` |
|      3 | 3346 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3347 | `		return PH7_OK;` |
|      - | 3348 | `	}` |
|      - | 3349 | `	/* Extract the target string */` |
|      5 | 3350 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3351 | `	if( nLen < 1 ){` |
|      - | 3352 | `		/* Empty string,return null */` |
|    ! 0 | 3353 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3354 | `		return PH7_OK;` |
|      - | 3355 | `	}` |
|      5 | 3356 | `	if( nArg > 1 ){` |
|      3 | 3357 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3358 | `	}` |
|      5 | 3359 | `	zEnd = &zIn[nLen];` |
|      - | 3360 | `	/* Perform the requested operation */` |
|      4 | 3361 | `	for(;;){` |
|      9 | 3362 | `		zCur = zIn;` |
|      - | 3363 | `		/* Delimit the string */` |
|     21 | 3364 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3365 | `			zIn++;` |
|      1 | 3366 | `		}` |
|      9 | 3367 | `		if( zCur < zIn ){` |
|      - | 3368 | `			/* Output chunk verbatim */` |
|      9 | 3369 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3370 | `		}` |
|      9 | 3371 | `		if( zIn >= zEnd ){` |
|      - | 3372 | `			/* No more input to process */` |
|      5 | 3373 | `			break;` |
|      - | 3374 | `		}` |
|      - | 3375 | `		/* Output the HTML line break */` |
|      - | 3376 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br/>' (legacy without space), otherwise use '<br>' */` |
|      5 | 3377 | `		if( is_xhtml ){` |
|      3 | 3378 | `			ph7_result_string(pCtx,"<br/>",(int)sizeof("<br/>")-1);` |
|      2 | 3379 | `		}else{` |
|      3 | 3380 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3381 | `		}` |
|      5 | 3382 | `		zCur = zIn;` |
|      - | 3383 | `		/* Append trailing line */` |
|     11 | 3384 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3385 | `			zIn++;` |
|      1 | 3386 | `		}` |
|      5 | 3387 | `		if( zCur < zIn ){` |
|      - | 3388 | `			/* Output chunk verbatim */` |
|      5 | 3389 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 3390 | `		}` |
|      1 | 3391 | `	}` |
|      5 | 3392 | `	return PH7_OK;` |
|      4 | 3393 | `}` |
|      - | 3394 | `/*` |
|      - | 3395 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3396 | ` *  According to the PHP reference manual.` |
|      - | 3397 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3398 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3399 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 3400 | ` * This applies to both sprintf() and printf().` |
|      - | 3401 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 3402 | ` * or more of these elements, in order:` |
|      - | 3403 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 3404 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 3405 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 3406 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 3407 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 3408 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 3409 | ` *   it with a single quote ('). See the examples below.` |
|      - | 3410 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 3411 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 3412 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 3413 | ` *   should result in.` |
|      - | 3414 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 3415 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 3416 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 3417 | ` *   limit to the string.` |
|      - | 3418 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 3419 | ` *       % - a literal percent character. No argument is required.` |
|      - | 3420 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 3421 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 3422 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 3423 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 3424 | ` * 	     for the number of digits after the decimal point.` |
|      - | 3425 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 3426 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 3427 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 3428 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 3429 | ` *       g - shorter of %e and %f.` |
|      - | 3430 | ` *       G - shorter of %E and %f.` |
|      - | 3431 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 3432 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 3433 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 3434 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 3435 | ` */` |
|      - | 3436 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3437 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 3438 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 3439 | `/*` |
|      - | 3440 | `** Conversion types fall into various categories as defined by the` |
|      - | 3441 | `** following enumeration.` |
|      - | 3442 | `*/` |
|      - | 3443 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 3444 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 3445 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 3446 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 3447 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 3448 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 3449 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 3450 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 3451 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 3452 |  |
|      - | 3453 | `/*` |
|      - | 3454 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 3455 | `*/` |
|      - | 3456 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 3457 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 3458 | `/*` |
|      - | 3459 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 3460 | `** by an instance of the following structure` |
|      - | 3461 | `*/` |
|      - | 3462 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 3463 | `struct ph7_fmt_info` |
|      - | 3464 | `{` |
|      - | 3465 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 3466 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 3467 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 3468 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 3469 | `  char *charset; /* The character set for conversion */` |
|      - | 3470 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 3471 | `};` |
|      - | 3472 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3473 | `/*` |
|      - | 3474 | `** "*val" is a double such that 0.1 <= *val < 10.0` |
|      - | 3475 | `** Return the ascii code for the leading digit of *val, then` |
|      - | 3476 | `** multiply "*val" by 10.0 to renormalize.` |
|      - | 3477 | `**` |
|      - | 3478 | `** Example:` |
|      - | 3479 | `**     input:     *val = 3.14159` |
|      - | 3480 | `**     output:    *val = 1.4159    function return = '3'` |
|      - | 3481 | `**` |
|      - | 3482 | `** The counter *cnt is incremented each time.  After counter exceeds` |
|      - | 3483 | `** 16 (the number of significant digits in a 64-bit float) '0' is` |
|      - | 3484 | `** always returned.` |
|      - | 3485 | `*/` |
|    422 | 3486 | `static int vxGetdigit(sxlongreal *val,int *cnt)` |
|      1 | 3487 | `{` |
|      - | 3488 | `  sxlongreal d;` |
|      - | 3489 | `  int digit;` |
|      - | 3490 |  |
|    423 | 3491 | `  if( (*cnt)++ >= 16 ){` |
|    ! 0 | 3492 | `	  return '0';` |
|      - | 3493 | `  }` |
|    423 | 3494 | `  digit = (int)*val;` |
|    423 | 3495 | `  d = digit;` |
|    423 | 3496 | `   *val = (*val - d)*10.0;` |
|    423 | 3497 | `  return digit + '0' ;` |
|    212 | 3498 | `}` |
|      - | 3499 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      - | 3500 | `/*` |
|      - | 3501 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 3502 | ` * used conversion types first.` |
|      - | 3503 | ` */` |
|      - | 3504 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 3505 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 3506 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 3507 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 3508 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 3509 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 3510 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 3511 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 3512 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 3513 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3514 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3515 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 3516 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 3517 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 3518 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 3519 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 3520 | `};` |
|      - | 3521 | `/*` |
|      - | 3522 | ` * Format a given string.` |
|      - | 3523 | ` * The root program.  All variations call this core.` |
|      - | 3524 | ` * INPUTS:` |
|      - | 3525 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 3526 | ` *            1. A pointer to the call context.` |
|      - | 3527 | ` *            2. A pointer to the list of characters to be output` |
|      - | 3528 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 3529 | ` *            3. An integer number of characters to be output.` |
|      - | 3530 | ` *               (Note: This number might be zero.)` |
|      - | 3531 | ` *            4. Upper layer private data.` |
|      - | 3532 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 3533 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 3534 | ` */` |
|    260 | 3535 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 3536 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 3537 | `	ph7_context *pCtx,  /* call context */` |
|      - | 3538 | `	const char *zIn,    /* Format string */` |
|      - | 3539 | `	int nByte,          /* Format string length */` |
|      - | 3540 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 3541 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 3542 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 3543 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 3544 | `	)` |
|      1 | 3545 | `{` |
|    261 | 3546 | `	char spaces[] = "                                                  ";` |
|      - | 3547 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    261 | 3548 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 3549 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 3550 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 3551 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 3552 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 3553 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 3554 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 3555 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 3556 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 3557 | `	ph7_int64 iVal;` |
|      - | 3558 | `	int precision;           /* Precision of the current field */` |
|      - | 3559 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 3560 | `	int c,rc,n;` |
|      - | 3561 | `	int length;              /* Length of the field */` |
|      - | 3562 | `	int prefix;` |
|      - | 3563 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 3564 | `	int width;               /* Width of the current field */` |
|      - | 3565 | `	int idx;` |
|    261 | 3566 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 3567 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 3568 | `	/* Start the format process */` |
|    380 | 3569 | `	for(;;){` |
|    761 | 3570 | `		zCur = zIn;` |
|   2785 | 3571 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|   2025 | 3572 | `			zIn++;` |
|      1 | 3573 | `		}` |
|    761 | 3574 | `		if( zCur < zIn ){` |
|      - | 3575 | `			/* Consume chunk verbatim */` |
|    539 | 3576 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|    539 | 3577 | `			if( rc != SXRET_OK ){` |
|      - | 3578 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|    ! 0 | 3579 | `				break;` |
|      - | 3580 | `			}` |
|    269 | 3581 | `		}` |
|    761 | 3582 | `		if( zIn >= zEnd ){` |
|      - | 3583 | `			/* No more input to process,break immediately */` |
|    259 | 3584 | `			break;` |
|      - | 3585 | `		}` |
|      - | 3586 | `		/* Find out what flags are present */` |
|    503 | 3587 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    502 | 3588 | `			flag_alternateform = flag_zeropad = 0;` |
|    503 | 3589 | `		zIn++; /* Jump the precent sign */` |
|    251 | 3590 | `		do{` |
|    535 | 3591 | `			c = zIn[0];` |
|    535 | 3592 | `			switch( c ){` |
|      9 | 3593 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      3 | 3594 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 3595 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|      9 | 3596 | `			case '#':   flag_alternateform = 1;   c = 0;   break;` |
|      9 | 3597 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 3598 | `			case '\'':` |
|    ! 0 | 3599 | `				zIn++;` |
|    ! 0 | 3600 | `				if( zIn < zEnd ){` |
|      - | 3601 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 3602 | `					c = zIn[0];` |
|    ! 0 | 3603 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3604 | `						spaces[idx] = (char)c;` |
|    ! 0 | 3605 | `					}` |
|    ! 0 | 3606 | `					c = 0;` |
|    ! 0 | 3607 | `				}` |
|    ! 0 | 3608 | `				break;` |
|    502 | 3609 | `			default:                                       break;` |
|      - | 3610 | `			}` |
|    535 | 3611 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 3612 | `		/* Get the field width */` |
|    503 | 3613 | `		width = 0;` |
|    788 | 3614 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     35 | 3615 | `			width = width*10 + (zIn[0] - '0');` |
|     35 | 3616 | `			zIn++;` |
|      1 | 3617 | `		}` |
|    503 | 3618 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 3619 | `			/* Position specifer */` |
|    ! 0 | 3620 | `			if( width > 0 ){` |
|    ! 0 | 3621 | `				n = width;` |
|    ! 0 | 3622 | `				if( vf && n > 0 ){` |
|    ! 0 | 3623 | `					n--;` |
|    ! 0 | 3624 | `				}` |
|    ! 0 | 3625 | `			}` |
|    ! 0 | 3626 | `			zIn++;` |
|    ! 0 | 3627 | `			width = 0;` |
|    ! 0 | 3628 | `			if( zIn < zEnd && zIn[0] == '0' ){` |
|    ! 0 | 3629 | `				flag_zeropad = 1;` |
|    ! 0 | 3630 | `				zIn++;` |
|    ! 0 | 3631 | `			}` |
|    ! 0 | 3632 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 3633 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 3634 | `				zIn++;` |
|    ! 0 | 3635 | `			}` |
|    ! 0 | 3636 | `		}` |
|    503 | 3637 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 3638 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 3639 | `		}` |
|      - | 3640 | `		/* Get the precision */` |
|    503 | 3641 | `		precision = -1;` |
|    503 | 3642 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     59 | 3643 | `			precision = 0;` |
|     59 | 3644 | `			zIn++;` |
|    150 | 3645 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     63 | 3646 | `				precision = precision*10 + (zIn[0] - '0');` |
|     63 | 3647 | `				zIn++;` |
|      1 | 3648 | `			}` |
|     29 | 3649 | `		}` |
|    503 | 3650 | `		if( zIn >= zEnd ){` |
|      - | 3651 | `			/* No more input */` |
|      3 | 3652 | `			break;` |
|      - | 3653 | `		}` |
|      - | 3654 | `		/* Fetch the info entry for the field */` |
|    501 | 3655 | `		pInfo = 0;` |
|    501 | 3656 | `		xtype = PH7_FMT_ERROR;` |
|    501 | 3657 | `		c = zIn[0];` |
|    501 | 3658 | `		zIn++; /* Jump the format specifer */` |
|   1439 | 3659 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|   1437 | 3660 | `			if( c==aFmt[idx].fmttype ){` |
|    499 | 3661 | `				pInfo = &aFmt[idx];` |
|    499 | 3662 | `				xtype = pInfo->type;` |
|    499 | 3663 | `				break;` |
|      - | 3664 | `			}` |
|    470 | 3665 | `		}` |
|    501 | 3666 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    501 | 3667 | `		length = 0;` |
|      - | 3668 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 3669 | `		 /*` |
|      - | 3670 | `		  ** At this point, variables are initialized as follows:` |
|      - | 3671 | `		  **` |
|      - | 3672 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 3673 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 3674 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 3675 | `		  **                               field width was negative.` |
|      - | 3676 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 3677 | `		  **                               the conversion character.` |
|      - | 3678 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 3679 | `		  **   width                       The specified field width.  This is` |
|      - | 3680 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 3681 | `		  **   precision                   The specified precision.  The default` |
|      - | 3682 | `		  **                               is -1.` |
|      - | 3683 | `		  */` |
|    501 | 3684 | `		switch(xtype){` |
|    ! 0 | 3685 | `		case PH7_FMT_PERCENT:` |
|      - | 3686 | `			/* A literal percent character */` |
|    ! 0 | 3687 | `			zWorker[0] = '%';` |
|    ! 0 | 3688 | `			length = (int)sizeof(char);` |
|    ! 0 | 3689 | `			break;` |
|      3 | 3690 | `		case PH7_FMT_CHARX:` |
|      - | 3691 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 3692 | `			 * with that ASCII value` |
|      - | 3693 | `			 */` |
|      7 | 3694 | `			pArg = NEXT_ARG;` |
|      7 | 3695 | `			if( pArg == 0 ){` |
|      3 | 3696 | `				c = 0;` |
|      2 | 3697 | `			}else{` |
|      5 | 3698 | `				c = ph7_value_to_int(pArg);` |
|      - | 3699 | `			}` |
|      - | 3700 | `			/* NUL byte is an acceptable value */` |
|      7 | 3701 | `			zWorker[0] = (char)c;` |
|      7 | 3702 | `			length = (int)sizeof(char);` |
|      7 | 3703 | `			break;` |
|    159 | 3704 | `		case PH7_FMT_STRING:` |
|      - | 3705 | `			/* the argument is treated as and presented as a string */` |
|    319 | 3706 | `			pArg = NEXT_ARG;` |
|    319 | 3707 | `			if( pArg == 0 ){` |
|    ! 0 | 3708 | `				length = 0;` |
|    ! 0 | 3709 | `			}else{` |
|    319 | 3710 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 3711 | `			}` |
|    319 | 3712 | `			if( length < 1 ){` |
|    ! 0 | 3713 | `				zBuf = " ";` |
|    ! 0 | 3714 | `				length = (int)sizeof(char);` |
|    ! 0 | 3715 | `			}` |
|    319 | 3716 | `			if( precision>=0 && precision<length ){` |
|      3 | 3717 | `				length = precision;` |
|      1 | 3718 | `			}` |
|    319 | 3719 | `			if( flag_zeropad ){` |
|      - | 3720 | `				/* zero-padding works on strings too */` |
|    ! 0 | 3721 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3722 | `					spaces[idx] = '0';` |
|    ! 0 | 3723 | `				}` |
|    ! 0 | 3724 | `			}` |
|    319 | 3725 | `			break;` |
|     59 | 3726 | `		case PH7_FMT_RADIX:` |
|    119 | 3727 | `			pArg = NEXT_ARG;` |
|    119 | 3728 | `			if( pArg == 0 ){` |
|    ! 0 | 3729 | `				iVal = 0;` |
|    ! 0 | 3730 | `			}else{` |
|    119 | 3731 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 3732 | `			}` |
|      - | 3733 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|    119 | 3734 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 3735 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 3736 | `			}` |
|      - | 3737 | `#if 1` |
|      - | 3738 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 3739 | `        ** I think this is stupid.*/` |
|    119 | 3740 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 3741 | `#else` |
|      - | 3742 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 3743 | `        ** but leave the prefix for hex.*/` |
|      - | 3744 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 3745 | `#endif` |
|    119 | 3746 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|     89 | 3747 | `          if( iVal<0 ){` |
|     25 | 3748 | `            iVal = -iVal;` |
|      - | 3749 | `			/* Ticket 1433-003 */` |
|     25 | 3750 | `			if( iVal < 0 ){` |
|      - | 3751 | `				/* Overflow */` |
|    ! 0 | 3752 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3753 | `			}` |
|     25 | 3754 | `            prefix = '-';` |
|     77 | 3755 | `          }else if( flag_plussign )  prefix = '+';` |
|     63 | 3756 | `          else if( flag_blanksign )  prefix = ' ';` |
|     61 | 3757 | `          else                       prefix = 0;` |
|     45 | 3758 | `        }else{` |
|     31 | 3759 | `			if( iVal<0 ){` |
|    ! 0 | 3760 | `				iVal = -iVal;` |
|      - | 3761 | `				/* Ticket 1433-003 */` |
|    ! 0 | 3762 | `				if( iVal < 0 ){` |
|      - | 3763 | `					/* Overflow */` |
|    ! 0 | 3764 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3765 | `				}` |
|    ! 0 | 3766 | `			}` |
|     31 | 3767 | `			prefix = 0;` |
|      - | 3768 | `		}` |
|    119 | 3769 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      7 | 3770 | `          precision = width-(prefix!=0);` |
|      3 | 3771 | `        }` |
|    119 | 3772 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 3773 | `        {` |
|      - | 3774 | `          register char *cset;      /* Use registers for speed */` |
|      - | 3775 | `          register int base;` |
|    119 | 3776 | `          cset = pInfo->charset;` |
|    119 | 3777 | `          base = pInfo->base;` |
|     59 | 3778 | `          do{                                           /* Convert to ascii */` |
|    187 | 3779 | `            *(--zBuf) = cset[iVal%base];` |
|    187 | 3780 | `            iVal = iVal/base;` |
|    187 | 3781 | `          }while( iVal>0 );` |
|      - | 3782 | `        }` |
|    119 | 3783 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    141 | 3784 | `        for(idx=precision-length; idx>0; idx--){` |
|     23 | 3785 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|     12 | 3786 | `        }` |
|    119 | 3787 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|    119 | 3788 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 3789 | `          char *pre, x;` |
|      9 | 3790 | `          pre = pInfo->prefix;` |
|      9 | 3791 | `          if( *zBuf!=pre[0] ){` |
|     23 | 3792 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|      4 | 3793 | `          }` |
|      4 | 3794 | `        }` |
|    119 | 3795 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    119 | 3796 | `		break;` |
|     28 | 3797 | `		case PH7_FMT_FLOAT:` |
|      - | 3798 | `		case PH7_FMT_EXP:` |
|      - | 3799 | `		case PH7_FMT_GENERIC:{` |
|      - | 3800 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3801 | `		long double realvalue;` |
|      - | 3802 | `		int  exp;                /* exponent of real numbers */` |
|      - | 3803 | `		double rounder;          /* Used for rounding floating point values */` |
|      - | 3804 | `		int flag_dp;            /* True if decimal point should be shown */` |
|      - | 3805 | `		int flag_rtz;           /* True if trailing zeros should be removed */` |
|      - | 3806 | `		int flag_exp;           /* True to force display of the exponent */` |
|      - | 3807 | `		int nsd;                 /* Number of significant digits returned */` |
|     57 | 3808 | `		pArg = NEXT_ARG;` |
|     57 | 3809 | `		if( pArg == 0 ){` |
|    ! 0 | 3810 | `			realvalue = 0;` |
|    ! 0 | 3811 | `		}else{` |
|     57 | 3812 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 3813 | `		}` |
|      - | 3814 | `		/* Special-case NaN and infinities since the normal formatting logic` |
|      - | 3815 | `		 * below assumes a finite positive realvalue. */` |
|     57 | 3816 | `		if( PH7_IS_NAN(realvalue) ){` |
|    ! 0 | 3817 | `			zBuf = "NAN";` |
|    ! 0 | 3818 | `			length = 3;` |
|    ! 0 | 3819 | `			break;` |
|      - | 3820 | `		}` |
|     57 | 3821 | `		if( PH7_IS_INF(realvalue) ){` |
|      - | 3822 | `			/* Infinity prints as INF or -INF depending on sign. */` |
|    ! 0 | 3823 | `			if( realvalue < 0.0 ){` |
|    ! 0 | 3824 | `				zBuf = "-INF";` |
|    ! 0 | 3825 | `				length = 4;` |
|    ! 0 | 3826 | `			}else{` |
|    ! 0 | 3827 | `				zBuf = "INF";` |
|    ! 0 | 3828 | `				length = 3;` |
|      - | 3829 | `			}` |
|    ! 0 | 3830 | `			break;` |
|      - | 3831 | `		}` |
|     57 | 3832 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|     57 | 3833 | `		if( precision>PH7_FMT_BUFSIZ-40) precision = PH7_FMT_BUFSIZ-40;` |
|     57 | 3834 | `        if( realvalue<0.0 ){` |
|      3 | 3835 | `          realvalue = -realvalue;` |
|      3 | 3836 | `          prefix = '-';` |
|      2 | 3837 | `        }else{` |
|     55 | 3838 | `          if( flag_plussign )          prefix = '+';` |
|     55 | 3839 | `          else if( flag_blanksign )    prefix = ' ';` |
|     55 | 3840 | `          else                         prefix = 0;` |
|      - | 3841 | `        }` |
|     57 | 3842 | `        if( pInfo->type==PH7_FMT_GENERIC && precision>0 ) precision--;` |
|     57 | 3843 | `        rounder = 0.0;` |
|      - | 3844 | `#if 0` |
|      - | 3845 | `        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */` |
|      - | 3846 | `        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);` |
|      - | 3847 | `#else` |
|      - | 3848 | `        /* It makes more sense to use 0.5 */` |
|    405 | 3849 | `        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);` |
|      - | 3850 | `#endif` |
|     57 | 3851 | `        if( pInfo->type==PH7_FMT_FLOAT ) realvalue += rounder;` |
|      - | 3852 | `        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */` |
|     57 | 3853 | `        exp = 0;` |
|     57 | 3854 | `        if( realvalue>0.0 ){` |
|     61 | 3855 | `          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }` |
|     89 | 3856 | `          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }` |
|     61 | 3857 | `          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }` |
|     71 | 3858 | `          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }` |
|     57 | 3859 | `          if( exp>350 \|\| exp<-350 ){` |
|    ! 0 | 3860 | `            zBuf = "NaN";` |
|    ! 0 | 3861 | `            length = 3;` |
|    ! 0 | 3862 | `            break;` |
|      - | 3863 | `          }` |
|     28 | 3864 | `        }` |
|     57 | 3865 | `        zBuf = zWorker;` |
|      - | 3866 | `        /*` |
|      - | 3867 | `        ** If the field type is etGENERIC, then convert to either etEXP` |
|      - | 3868 | `        ** or etFLOAT, as appropriate.` |
|      - | 3869 | `        */` |
|     57 | 3870 | `        flag_exp = xtype==PH7_FMT_EXP;` |
|     57 | 3871 | `        if( xtype!=PH7_FMT_FLOAT ){` |
|    ! 0 | 3872 | `          realvalue += rounder;` |
|    ! 0 | 3873 | `          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }` |
|    ! 0 | 3874 | `        }` |
|     57 | 3875 | `        if( xtype==PH7_FMT_GENERIC ){` |
|    ! 0 | 3876 | `          flag_rtz = !flag_alternateform;` |
|    ! 0 | 3877 | `          if( exp<-4 \|\| exp>precision ){` |
|    ! 0 | 3878 | `            xtype = PH7_FMT_EXP;` |
|    ! 0 | 3879 | `          }else{` |
|    ! 0 | 3880 | `            precision = precision - exp;` |
|    ! 0 | 3881 | `            xtype = PH7_FMT_FLOAT;` |
|      - | 3882 | `          }` |
|    ! 0 | 3883 | `        }else{` |
|     57 | 3884 | `          flag_rtz = 0;` |
|      - | 3885 | `        }` |
|      - | 3886 | `        /*` |
|      - | 3887 | `        ** The "exp+precision" test causes output to be of type etEXP if` |
|      - | 3888 | `        ** the precision is too large to fit in buf[].` |
|      - | 3889 | `        */` |
|     57 | 3890 | `        nsd = 0;` |
|     57 | 3891 | `        if( xtype==PH7_FMT_FLOAT && exp+precision<PH7_FMT_BUFSIZ-30 ){` |
|     57 | 3892 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|     57 | 3893 | `          if( prefix ) *(zBuf++) = (char)prefix;         /* Sign */` |
|     57 | 3894 | `          if( exp<0 )  *(zBuf++) = '0';            /* Digits before "." */` |
|    149 | 3895 | `          else for(; exp>=0; exp--) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     57 | 3896 | `          if( flag_dp ) *(zBuf++) = '.';           /* The decimal point */` |
|     89 | 3897 | `          for(exp++; exp<0 && precision>0; precision--, exp++){` |
|     33 | 3898 | `            *(zBuf++) = '0';` |
|     17 | 3899 | `          }` |
|    373 | 3900 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     57 | 3901 | `          *(zBuf--) = 0;                           /* Null terminate */` |
|     57 | 3902 | `          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */` |
|    ! 0 | 3903 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 3904 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 3905 | `          }` |
|     57 | 3906 | `          zBuf++;                            /* point to next free slot */` |
|     29 | 3907 | `        }else{    /* etEXP or etGENERIC */` |
|    ! 0 | 3908 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|    ! 0 | 3909 | `          if( prefix ) *(zBuf++) = (char)prefix;   /* Sign */` |
|    ! 0 | 3910 | `          *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);  /* First digit */` |
|    ! 0 | 3911 | `          if( flag_dp ) *(zBuf++) = '.';     /* Decimal point */` |
|    ! 0 | 3912 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|    ! 0 | 3913 | `          zBuf--;                            /* point to last digit */` |
|    ! 0 | 3914 | `          if( flag_rtz && flag_dp ){          /* Remove tail zeros */` |
|    ! 0 | 3915 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 3916 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 3917 | `          }` |
|    ! 0 | 3918 | `          zBuf++;                            /* point to next free slot */` |
|    ! 0 | 3919 | `          if( exp \|\| flag_exp ){` |
|    ! 0 | 3920 | `            *(zBuf++) = pInfo->charset[0];` |
|    ! 0 | 3921 | `            if( exp<0 ){ *(zBuf++) = '-'; exp = -exp; } /* sign of exp */` |
|    ! 0 | 3922 | `            else       { *(zBuf++) = '+'; }` |
|    ! 0 | 3923 | `            if( exp>=100 ){` |
|    ! 0 | 3924 | `              *(zBuf++) = (char)((exp/100)+'0');                /* 100's digit */` |
|    ! 0 | 3925 | `              exp %= 100;` |
|    ! 0 | 3926 | `            }` |
|    ! 0 | 3927 | `            *(zBuf++) = (char)(exp/10+'0');                     /* 10's digit */` |
|    ! 0 | 3928 | `            *(zBuf++) = (char)(exp%10+'0');                     /* 1's digit */` |
|    ! 0 | 3929 | `          }` |
|      - | 3930 | `        }` |
|      - | 3931 | `        /* The converted number is in buf[] and zero terminated.Output it.` |
|      - | 3932 | `        ** Note that the number is in the usual order, not reversed as with` |
|      - | 3933 | `        ** integer conversions.*/` |
|     57 | 3934 | `        length = (int)(zBuf-zWorker);` |
|     57 | 3935 | `        zBuf = zWorker;` |
|      - | 3936 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 3937 | `        ** set and we are not left justified */` |
|     57 | 3938 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 3939 | `          int i;` |
|      3 | 3940 | `          int nPad = width - length;` |
|     13 | 3941 | `          for(i=width; i>=nPad; i--){` |
|     11 | 3942 | `            zBuf[i] = zBuf[i-nPad];` |
|      6 | 3943 | `          }` |
|      3 | 3944 | `          i = prefix!=0;` |
|      5 | 3945 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      3 | 3946 | `          length = width;` |
|      1 | 3947 | `        }` |
|      - | 3948 | `#else` |
|      - | 3949 | `         zBuf = " ";` |
|      - | 3950 | `		 length = (int)sizeof(char);` |
|      - | 3951 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|     57 | 3952 | `		 break;` |
|      - | 3953 | `							 }` |
|      1 | 3954 | `		default:` |
|      - | 3955 | `			/* Invalid format specifer */` |
|      3 | 3956 | `			zWorker[0] = '?';` |
|      3 | 3957 | `			length = (int)sizeof(char);` |
|      2 | 3958 | `			break;` |
|      - | 3959 | `		}` |
|      - | 3960 | `		 /*` |
|      - | 3961 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 3962 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 3963 | `		 ** the output.` |
|      - | 3964 | `		 */` |
|    501 | 3965 | `    if( !flag_leftjustify ){` |
|      - | 3966 | `      register int nspace;` |
|    493 | 3967 | `      nspace = width-length;` |
|    493 | 3968 | `      if( nspace>0 ){` |
|      5 | 3969 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3970 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3971 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3972 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3973 | `			}` |
|    ! 0 | 3974 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3975 | `        }` |
|      5 | 3976 | `        if( nspace>0 ){` |
|      5 | 3977 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      5 | 3978 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3979 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3980 | `			}` |
|      2 | 3981 | `		}` |
|      2 | 3982 | `      }` |
|    246 | 3983 | `    }` |
|    501 | 3984 | `    if( length>0 ){` |
|    501 | 3985 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    501 | 3986 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3987 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3988 | `		}` |
|    250 | 3989 | `    }` |
|    501 | 3990 | `    if( flag_leftjustify ){` |
|      - | 3991 | `      register int nspace;` |
|      9 | 3992 | `      nspace = width-length;` |
|      9 | 3993 | `      if( nspace>0 ){` |
|      9 | 3994 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3995 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3996 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3997 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3998 | `			}` |
|    ! 0 | 3999 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4000 | `        }` |
|      9 | 4001 | `        if( nspace>0 ){` |
|      9 | 4002 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      9 | 4003 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4004 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4005 | `			}` |
|      4 | 4006 | `		}` |
|      4 | 4007 | `      }` |
|      4 | 4008 | `    }` |
|      1 | 4009 | ` }/* for(;;) */` |
|    261 | 4010 | `	return SXRET_OK;` |
|    131 | 4011 | `}` |
|      - | 4012 | `/*` |
|      - | 4013 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 4014 | ` */` |
|     90 | 4015 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4016 | `{` |
|      - | 4017 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|      - | 4018 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|      - | 4019 | `	 * non-OK rc also stops the format loop. */` |
|     91 | 4020 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|     91 | 4021 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|     91 | 4022 | `	return *pRc;` |
|      1 | 4023 | `}` |
|      - | 4024 | `/*` |
|      - | 4025 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 4026 | ` *  Return a formatted string.` |
|      - | 4027 | ` * Parameters` |
|      - | 4028 | ` *  $format` |
|      - | 4029 | ` *    The format string (see block comment above)` |
|      - | 4030 | ` * Return` |
|      - | 4031 | ` *  A string produced according to the formatting string format.` |
|      - | 4032 | ` */` |
|     62 | 4033 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4034 | `{` |
|      - | 4035 | `	const char *zFormat;` |
|     63 | 4036 | `	sxi32 rc = SXRET_OK;` |
|      - | 4037 | `	int nLen;` |
|     63 | 4038 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4039 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 4040 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4041 | `		return PH7_OK;` |
|      - | 4042 | `	}` |
|      - | 4043 | `	/* Extract the string format */` |
|     61 | 4044 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     61 | 4045 | `	if( nLen < 1 ){` |
|      - | 4046 | `		/* Empty string */` |
|    ! 0 | 4047 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4048 | `		return PH7_OK;` |
|      - | 4049 | `	}` |
|      - | 4050 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|     61 | 4051 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|     61 | 4052 | `	if( rc != SXRET_OK ){` |
|      - | 4053 | `		/* The result append ran out of memory: raise a fatal rather than` |
|      - | 4054 | `		 * returning a silently-truncated string. */` |
|    ! 0 | 4055 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4056 | `	}` |
|     61 | 4057 | `	return PH7_OK;` |
|     32 | 4058 | `}` |
|      - | 4059 | `/*` |
|      - | 4060 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 4061 | ` */` |
|    922 | 4062 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4063 | `{` |
|    923 | 4064 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 4065 | `	/* Call the VM output consumer directly */` |
|    923 | 4066 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 4067 | `	/* Increment counter */` |
|    923 | 4068 | `	*pCounter += nLen;` |
|    923 | 4069 | `	return PH7_OK;` |
|      1 | 4070 | `}` |
|      - | 4071 | `/*` |
|      - | 4072 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 4073 | ` *  Output a formatted string.` |
|      - | 4074 | ` * Parameters` |
|      - | 4075 | ` *  $format` |
|      - | 4076 | ` *   See sprintf() for a description of format.` |
|      - | 4077 | ` * Return` |
|      - | 4078 | ` *  The length of the outputted string.` |
|      - | 4079 | ` */` |
|    176 | 4080 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4081 | `{` |
|    177 | 4082 | `	ph7_int64 nCounter = 0;` |
|      - | 4083 | `	const char *zFormat;` |
|      - | 4084 | `	int nLen;` |
|    177 | 4085 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4086 | `		/* Missing/Invalid arguments,return 0 */` |
|      3 | 4087 | `		ph7_result_int(pCtx,0);` |
|      3 | 4088 | `		return PH7_OK;` |
|      - | 4089 | `	}` |
|      - | 4090 | `	/* Extract the string format */` |
|    175 | 4091 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    175 | 4092 | `	if( nLen < 1 ){` |
|      - | 4093 | `		/* Empty string */` |
|    ! 0 | 4094 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4095 | `		return PH7_OK;` |
|      - | 4096 | `	}` |
|      - | 4097 | `	/* Format the string */` |
|    175 | 4098 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 4099 | `	/* Return the length of the outputted string */` |
|    175 | 4100 | `	ph7_result_int64(pCtx,nCounter);` |
|    175 | 4101 | `	return PH7_OK;` |
|     89 | 4102 | `}` |
|      - | 4103 | `/*` |
|      - | 4104 | ` * int vprintf(string $format,array $args)` |
|      - | 4105 | ` *  Output a formatted string.` |
|      - | 4106 | ` * Parameters` |
|      - | 4107 | ` *  $format` |
|      - | 4108 | ` *   See sprintf() for a description of format.` |
|      - | 4109 | ` * Return` |
|      - | 4110 | ` *  The length of the outputted string.` |
|      - | 4111 | ` */` |
|      2 | 4112 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4113 | `{` |
|      3 | 4114 | `	ph7_int64 nCounter = 0;` |
|      - | 4115 | `	const char *zFormat;` |
|      - | 4116 | `	ph7_hashmap *pMap;` |
|      - | 4117 | `	SySet sArg;` |
|      - | 4118 | `	int nLen,n;` |
|      3 | 4119 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4120 | `		/* Missing/Invalid arguments,return 0 */` |
|    ! 0 | 4121 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4122 | `		return PH7_OK;` |
|      - | 4123 | `	}` |
|      - | 4124 | `	/* Extract the string format */` |
|      3 | 4125 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4126 | `	if( nLen < 1 ){` |
|      - | 4127 | `		/* Empty string */` |
|    ! 0 | 4128 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4129 | `		return PH7_OK;` |
|      - | 4130 | `	}` |
|      - | 4131 | `	/* Point to the hashmap */` |
|      3 | 4132 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4133 | `	/* Extract arguments from the hashmap */` |
|      3 | 4134 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4135 | `	/* Format the string */` |
|      3 | 4136 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 4137 | `	/* Return the length of the outputted string */` |
|      3 | 4138 | `	ph7_result_int64(pCtx,nCounter);` |
|      - | 4139 | `	/* Release the container */` |
|      3 | 4140 | `	SySetRelease(&sArg);` |
|      3 | 4141 | `	return PH7_OK;` |
|      2 | 4142 | `}` |
|      - | 4143 | `/*` |
|      - | 4144 | ` * int vsprintf(string $format,array $args)` |
|      - | 4145 | ` *  Output a formatted string.` |
|      - | 4146 | ` * Parameters` |
|      - | 4147 | ` *  $format` |
|      - | 4148 | ` *   See sprintf() for a description of format.` |
|      - | 4149 | ` * Return` |
|      - | 4150 | ` *  A string produced according to the formatting string format.` |
|      - | 4151 | ` */` |
|     10 | 4152 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4153 | `{` |
|      - | 4154 | `	const char *zFormat;` |
|      - | 4155 | `	ph7_hashmap *pMap;` |
|      - | 4156 | `	SySet sArg;` |
|     11 | 4157 | `	sxi32 rc = SXRET_OK;` |
|      - | 4158 | `	int nLen,n;` |
|     11 | 4159 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4160 | `		/* Missing/Invalid arguments,return the empty string */` |
|      5 | 4161 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 4162 | `		return PH7_OK;` |
|      - | 4163 | `	}` |
|      - | 4164 | `	/* Extract the string format */` |
|      7 | 4165 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4166 | `	if( nLen < 1 ){` |
|      - | 4167 | `		/* Empty string */` |
|    ! 0 | 4168 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4169 | `		return PH7_OK;` |
|      - | 4170 | `	}` |
|      - | 4171 | `	/* Point to hashmap */` |
|      7 | 4172 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4173 | `	/* Extract arguments from the hashmap */` |
|      7 | 4174 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4175 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|      7 | 4176 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|      - | 4177 | `	/* Release the container */` |
|      7 | 4178 | `	SySetRelease(&sArg);` |
|      7 | 4179 | `	if( rc != SXRET_OK ){` |
|      - | 4180 | `		/* The result append ran out of memory: raise a fatal. */` |
|    ! 0 | 4181 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4182 | `	}` |
|      7 | 4183 | `	return PH7_OK;` |
|      6 | 4184 | `}` |
|      - | 4185 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 4186 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4187 | `/*` |
|      - | 4188 | ` * Symisc eXtension.` |
|      - | 4189 | ` * string size_format(int64 $size)` |
|      - | 4190 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 4191 | ` *  Example:` |
|      - | 4192 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 4193 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 4194 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 4195 | ` * Parameter` |
|      - | 4196 | ` *  $size` |
|      - | 4197 | ` *    Entity size in bytes.` |
|      - | 4198 | ` * Return` |
|      - | 4199 | ` *   Formatted string representation of the given size.` |
|      - | 4200 | ` */` |
|     24 | 4201 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4202 | `{` |
|      - | 4203 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 4204 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 4205 | `	sxi32 nRest,i_32;` |
|      - | 4206 | `	ph7_int64 iSize;` |
|     25 | 4207 | `	int c = -1; /* index in zUnit[] */` |
|      - | 4208 |  |
|     25 | 4209 | `	if( nArg < 1 ){` |
|      - | 4210 | `		/* Missing argument,return the empty string */` |
|      3 | 4211 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4212 | `		return PH7_OK;` |
|      - | 4213 | `	}` |
|      - | 4214 | `	/* Extract the given size */` |
|     23 | 4215 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 4216 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 4217 | `		/* Don't bother formatting,return immediately */` |
|      5 | 4218 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 4219 | `		return PH7_OK;` |
|      - | 4220 | `	}` |
|     19 | 4221 | `	for(;;){` |
|     39 | 4222 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 4223 | `		iSize >>= 10;` |
|     39 | 4224 | `		c++;` |
|     39 | 4225 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 4226 | `			break;` |
|      - | 4227 | `		}` |
|      1 | 4228 | `	}` |
|     19 | 4229 | `	nRest /= 100;` |
|     19 | 4230 | `	if( nRest > 9 ){` |
|    ! 0 | 4231 | `		nRest = 9;` |
|    ! 0 | 4232 | `	}` |
|     19 | 4233 | `	if( iSize > 999 ){` |
|    ! 0 | 4234 | `		c++;` |
|    ! 0 | 4235 | `		nRest = 9;` |
|    ! 0 | 4236 | `		iSize = 0;` |
|    ! 0 | 4237 | `	}` |
|     19 | 4238 | `	i_32 = (sxi32)iSize;` |
|      - | 4239 | `	/* Format */` |
|     19 | 4240 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 4241 | `	return PH7_OK;` |
|     13 | 4242 | `}` |
|      - | 4243 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4244 | `/*` |
|      - | 4245 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 4246 | ` *   Calculate the md5 hash of a string.` |
|      - | 4247 | ` * Parameter` |
|      - | 4248 | ` *  $str` |
|      - | 4249 | ` *   Input string` |
|      - | 4250 | ` * $raw_output` |
|      - | 4251 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4252 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4253 | ` * Return` |
|      - | 4254 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 4255 | ` */` |
|     14 | 4256 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4257 | `{` |
|      - | 4258 | `	unsigned char zDigest[16];` |
|     15 | 4259 | `	int raw_output = FALSE;` |
|      - | 4260 | `	const void *pIn;` |
|      - | 4261 | `	int nLen;` |
|     15 | 4262 | `	if( nArg < 1 ){` |
|      - | 4263 | `		/* Missing arguments,return the empty string */` |
|      3 | 4264 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4265 | `		return PH7_OK;` |
|      - | 4266 | `	}` |
|      - | 4267 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 4268 | `	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */` |
|     13 | 4269 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 4270 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 4271 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 4272 | `	}` |
|      - | 4273 | `	/* Compute the MD5 digest */` |
|     13 | 4274 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|     13 | 4275 | `	if( raw_output ){` |
|      - | 4276 | `		/* Output raw digest */` |
|      5 | 4277 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 4278 | `	}else{` |
|      - | 4279 | `		/* Perform a binary to hex conversion */` |
|      9 | 4280 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4281 | `	}` |
|     13 | 4282 | `	return PH7_OK;` |
|      8 | 4283 | `}` |
|      - | 4284 | `/*` |
|      - | 4285 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 4286 | ` *   Calculate the sha1 hash of a string.` |
|      - | 4287 | ` * Parameter` |
|      - | 4288 | ` *  $str` |
|      - | 4289 | ` *   Input string` |
|      - | 4290 | ` * $raw_output` |
|      - | 4291 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4292 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4293 | ` * Return` |
|      - | 4294 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 4295 | ` */` |
|     12 | 4296 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4297 | `{` |
|      - | 4298 | `	unsigned char zDigest[20];` |
|     13 | 4299 | `	int raw_output = FALSE;` |
|      - | 4300 | `	const void *pIn;` |
|      - | 4301 | `	int nLen;` |
|     13 | 4302 | `	if( nArg < 1 ){` |
|      - | 4303 | `		/* Missing arguments,return the empty string */` |
|      3 | 4304 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4305 | `		return PH7_OK;` |
|      - | 4306 | `	}` |
|      - | 4307 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 4308 | `	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */` |
|     11 | 4309 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 4310 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 4311 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 4312 | `	}` |
|      - | 4313 | `	/* Compute the SHA1 digest */` |
|     11 | 4314 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|     11 | 4315 | `	if( raw_output ){` |
|      - | 4316 | `		/* Output raw digest */` |
|      5 | 4317 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 4318 | `	}else{` |
|      - | 4319 | `		/* Perform a binary to hex conversion */` |
|      7 | 4320 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4321 | `	}` |
|     11 | 4322 | `	return PH7_OK;` |
|      7 | 4323 | `}` |
|      - | 4324 | `/*` |
|      - | 4325 | ` * int64 crc32(string $str)` |
|      - | 4326 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 4327 | ` * Parameter` |
|      - | 4328 | ` *  $str` |
|      - | 4329 | ` *   Input string` |
|      - | 4330 | ` * Return` |
|      - | 4331 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 4332 | ` */` |
|      4 | 4333 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4334 | `{` |
|      - | 4335 | `	const void *pIn;` |
|      - | 4336 | `	sxu32 nCRC;` |
|      - | 4337 | `	int nLen;` |
|      5 | 4338 | `	if( nArg < 1 ){` |
|      - | 4339 | `		/* Missing arguments,return 0 */` |
|      3 | 4340 | `		ph7_result_int(pCtx,0);` |
|      3 | 4341 | `		return PH7_OK;` |
|      - | 4342 | `	}` |
|      - | 4343 | `	/* Extract the input string */` |
|      3 | 4344 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4345 | `	if( nLen < 1 ){` |
|      - | 4346 | `		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike` |
|      - | 4347 | `		 * md5()/sha1(), whose empty-string digests are non-zero. */` |
|    ! 0 | 4348 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4349 | `		return PH7_OK;` |
|      - | 4350 | `	}` |
|      - | 4351 | `	/* Calculate the sum */` |
|      3 | 4352 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 4353 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 4354 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 4355 | `	return PH7_OK;` |
|      3 | 4356 | `}` |
|      - | 4357 | `/*` |
|      - | 4358 | ` * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is` |
|      - | 4359 | ` * described by a small record so one dispatch (and one generic HMAC) serves them` |
|      - | 4360 | ` * all. Thin adapters normalize the differing context types and the reversed` |
|      - | 4361 | ` * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.` |
|      - | 4362 | ` */` |
|     11 | 4363 | `static void HashMd5Init(HashCtx *c){ MD5Init(&c->md5); }` |
|     15 | 4364 | `static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }` |
|     11 | 4365 | `static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }` |
|     11 | 4366 | `static void HashSha1Init(HashCtx *c){ SHA1Init(&c->sha1); }` |
|     15 | 4367 | `static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }` |
|     11 | 4368 | `static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }` |
|      9 | 4369 | `static void HashSha224Init(HashCtx *c){ SHA224Init(&c->sha256); }` |
|     33 | 4370 | `static void HashSha256Init(HashCtx *c){ SHA256Init(&c->sha256); }` |
|     57 | 4371 | `static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }` |
|     41 | 4372 | `static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }` |
|      9 | 4373 | `static void HashSha384Init(HashCtx *c){ SHA384Init(&c->sha512); }` |
|     15 | 4374 | `static void HashSha512Init(HashCtx *c){ SHA512Init(&c->sha512); }` |
|     27 | 4375 | `static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }` |
|     23 | 4376 | `static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }` |
|      - | 4377 | `typedef struct HashAlgo HashAlgo;` |
|      - | 4378 | `struct HashAlgo {` |
|      - | 4379 | `	const char *zName;   /* lowercase canonical name */` |
|      - | 4380 | `	int nDigestLen;      /* output bytes: 16/20/28/32/48/64 */` |
|      - | 4381 | `	int nBlockLen;       /* internal block bytes (for HMAC): 64 or 128 */` |
|      - | 4382 | `	void (*xInit)(HashCtx *);` |
|      - | 4383 | `	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);` |
|      - | 4384 | `	void (*xFinal)(HashCtx *,unsigned char *);` |
|      - | 4385 | `};` |
|      - | 4386 | `static const HashAlgo aHashAlgo[] = {` |
|      - | 4387 | `	{ "md5",    16, 64,  HashMd5Init,    HashMd5Update,    HashMd5Final    },` |
|      - | 4388 | `	{ "sha1",   20, 64,  HashSha1Init,   HashSha1Update,   HashSha1Final   },` |
|      - | 4389 | `	{ "sha224", 28, 64,  HashSha224Init, HashSha256Update, HashSha256Final },` |
|      - | 4390 | `	{ "sha256", 32, 64,  HashSha256Init, HashSha256Update, HashSha256Final },` |
|      - | 4391 | `	{ "sha384", 48, 128, HashSha384Init, HashSha512Update, HashSha512Final },` |
|      - | 4392 | `	{ "sha512", 64, 128, HashSha512Init, HashSha512Update, HashSha512Final },` |
|      - | 4393 | `};` |
|      - | 4394 | `/* Case-insensitive algorithm lookup (PHP accepts 'SHA256' etc.). */` |
|     73 | 4395 | `static const HashAlgo * HashFindAlgo(const char *zName,int nLen){` |
|      - | 4396 | `	sxu32 i;` |
|    279 | 4397 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|    272 | 4398 | `		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen` |
|    211 | 4399 | `			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){` |
|     67 | 4400 | `			return &aHashAlgo[i];` |
|      - | 4401 | `		}` |
|    106 | 4402 | `	}` |
|      6 | 4403 | `	return 0;` |
|     38 | 4404 | `}` |
|      - | 4405 | `/*` |
|      - | 4406 | ` * string hash(string $algo,string $data[,bool $binary = false])` |
|      - | 4407 | ` *   Generate a hash value (message digest).` |
|      - | 4408 | ` */` |
|     54 | 4409 | `static int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4410 | `{` |
|      - | 4411 | `	const HashAlgo *pAlgo;` |
|      - | 4412 | `	const char *zAlgo,*zData;` |
|     56 | 4413 | `	int nAlgoLen,nDataLen,raw_output = FALSE;` |
|      - | 4414 | `	HashCtx sCtx;` |
|      - | 4415 | `	unsigned char zDigest[64];` |
|     56 | 4416 | `	if( nArg < 2 ){` |
|    ! 0 | 4417 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4418 | `			"hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4419 | `	}` |
|     56 | 4420 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     56 | 4421 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     56 | 4422 | `	if( pAlgo == 0 ){` |
|      3 | 4423 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4424 | `			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - | 4425 | `	}` |
|     53 | 4426 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     53 | 4427 | `	if( nArg > 2 ){` |
|      9 | 4428 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|      4 | 4429 | `	}` |
|     53 | 4430 | `	pAlgo->xInit(&sCtx);` |
|     53 | 4431 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     53 | 4432 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     53 | 4433 | `	if( raw_output ){` |
|      9 | 4434 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|      5 | 4435 | `	}else{` |
|     45 | 4436 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - | 4437 | `	}` |
|     53 | 4438 | `	return PH7_OK;` |
|     29 | 4439 | `}` |
|      - | 4440 | `/*` |
|      - | 4441 | ` * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])` |
|      - | 4442 | ` *   Generate a keyed hash value using the HMAC method (RFC 2104).` |
|      - | 4443 | ` */` |
|     16 | 4444 | `static int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4445 | `{` |
|      - | 4446 | `	const HashAlgo *pAlgo;` |
|      - | 4447 | `	const char *zAlgo,*zData,*zKey;` |
|     18 | 4448 | `	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;` |
|      - | 4449 | `	HashCtx sCtx;` |
|      - | 4450 | `	unsigned char zKeyBlock[128],zIpad[128],zOpad[128],zInner[64],zDigest[64];` |
|      - | 4451 | `	int i,nBlock,nDigest;` |
|     18 | 4452 | `	if( nArg < 3 ){` |
|    ! 0 | 4453 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4454 | `			"hash_hmac() expects at least 3 arguments, %d given",nArg);` |
|      - | 4455 | `	}` |
|     18 | 4456 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     18 | 4457 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     18 | 4458 | `	if( pAlgo == 0 ){` |
|      3 | 4459 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4460 | `			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 4461 | `	}` |
|     15 | 4462 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     15 | 4463 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|     15 | 4464 | `	if( nArg > 3 ){` |
|      3 | 4465 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|      1 | 4466 | `	}` |
|     15 | 4467 | `	nBlock = pAlgo->nBlockLen;` |
|     15 | 4468 | `	nDigest = pAlgo->nDigestLen;` |
|      - | 4469 | `	/* Reduce the key to a single block: hash it if longer than the block, then` |
|      - | 4470 | `	 * zero-pad (a short or empty key is just zero-padded). */` |
|     15 | 4471 | `	SyZero(zKeyBlock,sizeof(zKeyBlock));` |
|     15 | 4472 | `	if( nKeyLen > nBlock ){` |
|      3 | 4473 | `		pAlgo->xInit(&sCtx);` |
|      3 | 4474 | `		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);` |
|      3 | 4475 | `		pAlgo->xFinal(&sCtx,zKeyBlock);` |
|     14 | 4476 | `	}else if( nKeyLen > 0 ){` |
|     11 | 4477 | `		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);` |
|      5 | 4478 | `	}` |
|   1039 | 4479 | `	for( i = 0; i < nBlock; i++ ){` |
|   1025 | 4480 | `		zIpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);` |
|   1025 | 4481 | `		zOpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);` |
|    513 | 4482 | `	}` |
|      - | 4483 | `	/* inner = H((key ^ ipad) \|\| data) */` |
|     15 | 4484 | `	pAlgo->xInit(&sCtx);` |
|     15 | 4485 | `	pAlgo->xUpdate(&sCtx,zIpad,(unsigned int)nBlock);` |
|     15 | 4486 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     15 | 4487 | `	pAlgo->xFinal(&sCtx,zInner);` |
|      - | 4488 | `	/* out = H((key ^ opad) \|\| inner) */` |
|     15 | 4489 | `	pAlgo->xInit(&sCtx);` |
|     15 | 4490 | `	pAlgo->xUpdate(&sCtx,zOpad,(unsigned int)nBlock);` |
|     15 | 4491 | `	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)nDigest);` |
|     15 | 4492 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     15 | 4493 | `	if( raw_output ){` |
|      3 | 4494 | `		ph7_result_string(pCtx,(const char *)zDigest,nDigest);` |
|      2 | 4495 | `	}else{` |
|     13 | 4496 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);` |
|      - | 4497 | `	}` |
|     15 | 4498 | `	return PH7_OK;` |
|     10 | 4499 | `}` |
|      - | 4500 | `/*` |
|      - | 4501 | ` * bool hash_equals(string $known_string,string $user_string)` |
|      - | 4502 | ` *   Timing-attack-safe string comparison.` |
|      - | 4503 | ` */` |
|     14 | 4504 | `static int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4505 | `{` |
|      - | 4506 | `	const char *zKnown,*zUser;` |
|      - | 4507 | `	int nKnown,nUser,i;` |
|     17 | 4508 | `	volatile unsigned char vDiff = 0;` |
|     17 | 4509 | `	if( nArg < 2 ){` |
|    ! 0 | 4510 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4511 | `			"hash_equals() expects exactly 2 arguments, %d given",nArg);` |
|      - | 4512 | `	}` |
|     17 | 4513 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      4 | 4514 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4515 | `			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",` |
|      1 | 4516 | `			ph7_type_name(apArg[0]));` |
|      - | 4517 | `	}` |
|     14 | 4518 | `	if( !ph7_value_is_string(apArg[1]) ){` |
|      4 | 4519 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4520 | `			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",` |
|      2 | 4521 | `			ph7_type_name(apArg[1]));` |
|      - | 4522 | `	}` |
|     11 | 4523 | `	zKnown = ph7_value_to_string(apArg[0],&nKnown);` |
|     11 | 4524 | `	zUser = ph7_value_to_string(apArg[1],&nUser);` |
|     11 | 4525 | `	if( nKnown != nUser ){` |
|      5 | 4526 | `		ph7_result_bool(pCtx,0);` |
|      5 | 4527 | `		return PH7_OK;` |
|      - | 4528 | `	}` |
|      - | 4529 | `	/* Constant-time: read every byte, never short-circuit. */` |
|     19 | 4530 | `	for( i = 0; i < nKnown; i++ ){` |
|     13 | 4531 | `		vDiff \|= (unsigned char)(zKnown[i] ^ zUser[i]);` |
|      7 | 4532 | `	}` |
|      7 | 4533 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|      7 | 4534 | `	return PH7_OK;` |
|     10 | 4535 | `}` |
|      - | 4536 | `/*` |
|      - | 4537 | ` * array hash_algos(void)` |
|      - | 4538 | ` *   Return a list of the registered hashing algorithms.` |
|      - | 4539 | ` */` |
|      2 | 4540 | `static int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4541 | `{` |
|      - | 4542 | `	ph7_value *pArray,*pValue;` |
|      - | 4543 | `	sxu32 i;` |
|      1 | 4544 | `	SXUNUSED(nArg);` |
|      1 | 4545 | `	SXUNUSED(apArg);` |
|      3 | 4546 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4547 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 4548 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 4549 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4550 | `		return PH7_OK;` |
|      - | 4551 | `	}` |
|     15 | 4552 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|     13 | 4553 | `		ph7_value_string(pValue,aHashAlgo[i].zName,-1);` |
|     13 | 4554 | `		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);` |
|     13 | 4555 | `		ph7_value_reset_string_cursor(pValue);` |
|      7 | 4556 | `	}` |
|      3 | 4557 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4558 | `	return PH7_OK;` |
|      2 | 4559 | `}` |
|      - | 4560 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 4561 | `/*` |
|      - | 4562 | ` * password_* (bcrypt). These live in ext/standard in real PHP — outside the` |
|      - | 4563 | ` * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.` |
|      - | 4564 | ` */` |
|      - | 4565 | `/*` |
|      - | 4566 | ` * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a` |
|      - | 4567 | ` * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).` |
|      - | 4568 | ` */` |
|     40 | 4569 | `static int BcryptParseHash(const char *zHash,int nHash,int *piCost)` |
|      1 | 4570 | `{` |
|      - | 4571 | `	int iCost;` |
|     40 | 4572 | `	if( nHash != 60 \|\| zHash[0] != '$' \|\| zHash[1] != '2' \|\| zHash[3] != '$'` |
|     29 | 4573 | `		\|\| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){` |
|     13 | 4574 | `		return FALSE;` |
|      - | 4575 | `	}` |
|     29 | 4576 | `	if( zHash[4] < '0' \|\| zHash[4] > '9' \|\| zHash[5] < '0' \|\| zHash[5] > '9' \|\| zHash[6] != '$' ){` |
|    ! 0 | 4577 | `		return FALSE;` |
|      - | 4578 | `	}` |
|     29 | 4579 | `	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');` |
|     29 | 4580 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      3 | 4581 | `		return FALSE;` |
|      - | 4582 | `	}` |
|     27 | 4583 | `	if( piCost ){ *piCost = iCost; }` |
|     27 | 4584 | `	return TRUE;` |
|     21 | 4585 | `}` |
|      - | 4586 | `/*` |
|      - | 4587 | ` * TRUE if the $algo argument selects bcrypt: null (PASSWORD_DEFAULT) or the` |
|      - | 4588 | ` * "2y" id (PASSWORD_BCRYPT/PASSWORD_DEFAULT). bcrypt is the only supported algo.` |
|      - | 4589 | ` */` |
|     20 | 4590 | `static int BcryptIsBcryptAlgo(ph7_value *pAlgo)` |
|      3 | 4591 | `{` |
|     23 | 4592 | `	if( ph7_value_is_null(pAlgo) ){` |
|    ! 0 | 4593 | `		return TRUE;` |
|      - | 4594 | `	}` |
|     23 | 4595 | `	if( ph7_value_is_string(pAlgo) ){` |
|      - | 4596 | `		int nAlgo;` |
|     23 | 4597 | `		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);` |
|     23 | 4598 | `		return ( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' );` |
|      - | 4599 | `	}` |
|    ! 0 | 4600 | `	return FALSE;` |
|     13 | 4601 | `}` |
|      - | 4602 | `/*` |
|      - | 4603 | ` * bool\|string password_hash(string $password,string\|int\|null $algo[,array $options])` |
|      - | 4604 | ` *  Create a bcrypt hash of the password.` |
|      - | 4605 | ` */` |
|     16 | 4606 | `static int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4607 | `{` |
|      - | 4608 | `	const char *zPwd;` |
|     19 | 4609 | `	int nPwd,iCost = 12;` |
|      - | 4610 | `	unsigned char aSalt[16];` |
|      - | 4611 | `	char zHash[60];` |
|     19 | 4612 | `	if( nArg < 2 ){` |
|    ! 0 | 4613 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4614 | `			"password_hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4615 | `	}` |
|     19 | 4616 | `	if( !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      3 | 4617 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4618 | `			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");` |
|      - | 4619 | `	}` |
|      - | 4620 | `	/* cost from $options['cost'] (default 12). */` |
|     16 | 4621 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|     14 | 4622 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|     14 | 4623 | `		if( pCost ){ iCost = ph7_value_to_int(pCost); }` |
|      6 | 4624 | `	}` |
|     16 | 4625 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      4 | 4626 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 4627 | `			"Invalid bcrypt cost parameter specified: %d",iCost);` |
|      - | 4628 | `	}` |
|     13 | 4629 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     13 | 4630 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 4631 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - | 4632 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|      - | 4633 | `	}` |
|     13 | 4634 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){` |
|    ! 0 | 4635 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4636 | `		return PH7_OK;` |
|      - | 4637 | `	}` |
|     13 | 4638 | `	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));` |
|     13 | 4639 | `	return PH7_OK;` |
|     11 | 4640 | `}` |
|      - | 4641 | `/*` |
|      - | 4642 | ` * bool password_verify(string $password,string $hash)` |
|      - | 4643 | ` *  Verify a password against a bcrypt hash. Never throws on a malformed hash.` |
|      - | 4644 | ` */` |
|     28 | 4645 | `static int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4646 | `{` |
|      - | 4647 | `	const char *zPwd,*zHash;` |
|      - | 4648 | `	int nPwd,nHash,iCost,i;` |
|      - | 4649 | `	unsigned char aSalt[16];` |
|      - | 4650 | `	char zComputed[60];` |
|     29 | 4651 | `	volatile unsigned char vDiff = 0;` |
|     29 | 4652 | `	if( nArg < 2 ){` |
|    ! 0 | 4653 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4654 | `			"password_verify() expects exactly 2 arguments, %d given",nArg);` |
|      - | 4655 | `	}` |
|     29 | 4656 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     29 | 4657 | `	zHash = ph7_value_to_string(apArg[1],&nHash);` |
|     29 | 4658 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|     11 | 4659 | `		ph7_result_bool(pCtx,0);` |
|     11 | 4660 | `		return PH7_OK;` |
|      - | 4661 | `	}` |
|      - | 4662 | `	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */` |
|     19 | 4663 | `	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 4664 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4665 | `		return PH7_OK;` |
|      - | 4666 | `	}` |
|     19 | 4667 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){` |
|    ! 0 | 4668 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4669 | `		return PH7_OK;` |
|      - | 4670 | `	}` |
|      - | 4671 | `	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps` |
|      - | 4672 | `	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */` |
|    577 | 4673 | `	for( i = 29; i < 60; i++ ){` |
|    559 | 4674 | `		vDiff \|= (unsigned char)(zComputed[i] ^ zHash[i]);` |
|    280 | 4675 | `	}` |
|     19 | 4676 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|     19 | 4677 | `	return PH7_OK;` |
|     15 | 4678 | `}` |
|      - | 4679 | `/*` |
|      - | 4680 | ` * array password_get_info(string $hash)` |
|      - | 4681 | ` *  Return ["algo"=>id\|null, "algoName"=>name, "options"=>[...]].` |
|      - | 4682 | ` */` |
|      6 | 4683 | `static int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4684 | `{` |
|      7 | 4685 | `	const char *zHash = "";` |
|      7 | 4686 | `	int nHash,iCost = 0,bBcrypt = 0;` |
|      - | 4687 | `	ph7_value *pArray,*pOptions,*pVal;` |
|      7 | 4688 | `	if( nArg > 0 ){` |
|      7 | 4689 | `		zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 4690 | `		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);` |
|      3 | 4691 | `	}` |
|      7 | 4692 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 4693 | `	pOptions = ph7_context_new_array(pCtx);` |
|      7 | 4694 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      7 | 4695 | `	if( pArray == 0 \|\| pOptions == 0 \|\| pVal == 0 ){` |
|    ! 0 | 4696 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4697 | `		return PH7_OK;` |
|      - | 4698 | `	}` |
|      7 | 4699 | `	if( bBcrypt ){` |
|      5 | 4700 | `		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */` |
|      5 | 4701 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 4702 | `		ph7_value_reset_string_cursor(pVal);` |
|      5 | 4703 | `		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);` |
|      5 | 4704 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      5 | 4705 | `		ph7_value_int(pVal,iCost);` |
|      5 | 4706 | `		ph7_array_add_strkey_elem(pOptions,"cost",pVal);` |
|      3 | 4707 | `	}else{` |
|      3 | 4708 | `		ph7_value_null(pVal);                          /* algo => null */` |
|      3 | 4709 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      3 | 4710 | `		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);` |
|      3 | 4711 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      - | 4712 | `	}` |
|      7 | 4713 | `	ph7_array_add_strkey_elem(pArray,"options",pOptions);` |
|      7 | 4714 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 4715 | `	return PH7_OK;` |
|      4 | 4716 | `}` |
|      - | 4717 | `/*` |
|      - | 4718 | ` * bool password_needs_rehash(string $hash,string\|int\|null $algo[,array $options])` |
|      - | 4719 | ` *  True if the hash was not made with the given algo/options.` |
|      - | 4720 | ` */` |
|      6 | 4721 | `static int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4722 | `{` |
|      - | 4723 | `	const char *zHash;` |
|      7 | 4724 | `	int nHash,iCost = 0,iWantCost = 12;` |
|      7 | 4725 | `	if( nArg < 2 ){` |
|    ! 0 | 4726 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4727 | `			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4728 | `	}` |
|      7 | 4729 | `	zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 4730 | `	if( !BcryptParseHash(zHash,nHash,&iCost) \|\| !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      - | 4731 | `		/* A non-bcrypt hash, or a request for a different algo → needs rehash. */` |
|      3 | 4732 | `		ph7_result_bool(pCtx,1);` |
|      3 | 4733 | `		return PH7_OK;` |
|      - | 4734 | `	}` |
|      5 | 4735 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|      5 | 4736 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|      5 | 4737 | `		if( pCost ){ iWantCost = ph7_value_to_int(pCost); }` |
|      2 | 4738 | `	}` |
|      5 | 4739 | `	ph7_result_bool(pCtx,iCost != iWantCost);` |
|      5 | 4740 | `	return PH7_OK;` |
|      4 | 4741 | `}` |
|      - | 4742 | `/*` |
|      - | 4743 | ` * filter_var() — input validation and sanitization (the ext/filter API).` |
|      - | 4744 | ` *` |
|      - | 4745 | ` * Filter and flag identifiers (values match PHP 8.5; the constants themselves` |
|      - | 4746 | ` * are registered in constant.c). The validate filters are hand-rolled rather` |
|      - | 4747 | ` * than delegating to SyStrToInt64/SyStrToReal: the former silently skips leading` |
|      - | 4748 | ` * zeros and cannot signal overflow, and the latter treats ',' as a decimal point` |
|      - | 4749 | ` * unconditionally — neither matches PHP's filter semantics.` |
|      - | 4750 | ` */` |
|      - | 4751 | `#define FV_VALIDATE_INT     257` |
|      - | 4752 | `#define FV_VALIDATE_BOOLEAN 258` |
|      - | 4753 | `#define FV_VALIDATE_FLOAT   259` |
|      - | 4754 | `#define FV_VALIDATE_REGEXP  272` |
|      - | 4755 | `#define FV_VALIDATE_URL     273` |
|      - | 4756 | `#define FV_VALIDATE_EMAIL   274` |
|      - | 4757 | `#define FV_VALIDATE_IP      275` |
|      - | 4758 | `#define FV_VALIDATE_MAC     276` |
|      - | 4759 | `#define FV_VALIDATE_DOMAIN  277` |
|      - | 4760 | `#define FV_SANITIZE_SPECIAL_CHARS      515` |
|      - | 4761 | `#define FV_DEFAULT          516 /* == FILTER_UNSAFE_RAW: pass the value through */` |
|      - | 4762 | `#define FV_SANITIZE_EMAIL   517` |
|      - | 4763 | `#define FV_SANITIZE_URL     518` |
|      - | 4764 | `#define FV_SANITIZE_NUMBER_INT   519` |
|      - | 4765 | `#define FV_SANITIZE_NUMBER_FLOAT 520` |
|      - | 4766 | `#define FV_SANITIZE_FULL_SPECIAL_CHARS 522` |
|      - | 4767 | `#define FV_FLAG_ALLOW_OCTAL  1` |
|      - | 4768 | `#define FV_FLAG_ALLOW_HEX    2` |
|      - | 4769 | `#define FV_FLAG_STRIP_LOW    4` |
|      - | 4770 | `#define FV_FLAG_STRIP_HIGH   8` |
|      - | 4771 | `#define FV_FLAG_ENCODE_LOW   16` |
|      - | 4772 | `#define FV_FLAG_ENCODE_HIGH  32` |
|      - | 4773 | `#define FV_FLAG_ENCODE_AMP   64` |
|      - | 4774 | `#define FV_FLAG_NO_ENCODE_QUOTES 128` |
|      - | 4775 | `#define FV_FLAG_STRIP_BACKTICK   512` |
|      - | 4776 | `#define FV_FLAG_ALLOW_FRACTION   4096` |
|      - | 4777 | `#define FV_FLAG_ALLOW_THOUSAND   8192` |
|      - | 4778 | `#define FV_FLAG_ALLOW_SCIENTIFIC 16384` |
|      - | 4779 | `#define FV_FLAG_IPV4  1048576` |
|      - | 4780 | `#define FV_FLAG_IPV6  2097152` |
|      - | 4781 | `#define FV_NULL_ON_FAILURE 134217728` |
|      - | 4782 | `/* The subset of flags the UNSAFE_RAW/DEFAULT string filter (FvSanitizeString)` |
|      - | 4783 | ` * acts on: when none are set the filter is a verbatim pass-through, so FV_DEFAULT` |
|      - | 4784 | ` * can shortcut. Keep this in sync with FvSanitizeString's flag handling. */` |
|      - | 4785 | `#define FV_FLAG_STRING_MASK (FV_FLAG_STRIP_LOW\|FV_FLAG_STRIP_HIGH\|FV_FLAG_STRIP_BACKTICK \` |
|      - | 4786 | `                            \|FV_FLAG_ENCODE_LOW\|FV_FLAG_ENCODE_HIGH\|FV_FLAG_ENCODE_AMP)` |
|      - | 4787 |  |
|      - | 4788 | `/* Trim leading/trailing PHP whitespace, adjusting the (*pz,*pn) view in place.` |
|      - | 4789 | ` * SyisSpace (isspace) matches PHP's filter whitespace set " \t\n\r\v\f". */` |
|    153 | 4790 | `static void FvTrim(const char **pz,int *pn){` |
|    153 | 4791 | `	const char *z = *pz;` |
|    153 | 4792 | `	int n = *pn;` |
|    157 | 4793 | `	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }` |
|    161 | 4794 | `	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }` |
|    153 | 4795 | `	*pz = z; *pn = n;` |
|    153 | 4796 | `}` |
|      - | 4797 | `/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     57 | 4798 | `static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){` |
|     57 | 4799 | `	int neg = 0, i;` |
|     57 | 4800 | `	sxu64 u = 0;` |
|     57 | 4801 | `	FvTrim(&z,&n);` |
|     57 | 4802 | `	if( n==0 ){ return 0; }` |
|     51 | 4803 | `	if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }` |
|     51 | 4804 | `	if( n==0 ){ return 0; }` |
|     49 | 4805 | `	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'\|\|z[1]=='X') ){` |
|      3 | 4806 | `		z += 2; n -= 2;` |
|      3 | 4807 | `		if( n==0 ){ return 0; }` |
|      7 | 4808 | `		for( i=0; i<n; i++ ){` |
|      5 | 4809 | `			int h = SyHexToint((unsigned char)z[i]);` |
|      5 | 4810 | `			if( h<0 ){ return 0; }` |
|      5 | 4811 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }` |
|      5 | 4812 | `			u = u*16 + (sxu64)h;` |
|      3 | 4813 | `		}` |
|     48 | 4814 | `	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){` |
|      9 | 4815 | `		for( i=0; i<n; i++ ){` |
|      7 | 4816 | `			if( z[i]<'0' \|\| z[i]>'7' ){ return 0; }` |
|      7 | 4817 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }` |
|      7 | 4818 | `			u = u*8 + (sxu64)(z[i]-'0');` |
|      4 | 4819 | `		}` |
|      2 | 4820 | `	}else{` |
|     45 | 4821 | `		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */` |
|    201 | 4822 | `		for( i=0; i<n; i++ ){` |
|    173 | 4823 | `			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|    161 | 4824 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }` |
|    161 | 4825 | `			u = u*10 + (sxu64)(z[i]-'0');` |
|     81 | 4826 | `		}` |
|      - | 4827 | `	}` |
|     33 | 4828 | `	if( neg ){` |
|      5 | 4829 | `		if( u > 0x8000000000000000ULL ){ return 0; }` |
|      5 | 4830 | `		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */` |
|      3 | 4831 | `	}else{` |
|     29 | 4832 | `		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }` |
|     27 | 4833 | `		*pOut = (ph7_int64)u;` |
|      - | 4834 | `	}` |
|     31 | 4835 | `	return 1;` |
|     29 | 4836 | `}` |
|      - | 4837 | `/* FILTER_VALIDATE_FLOAT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     69 | 4838 | `static int FvValidateFloat(const char *z,int n,int flags,double *pOut){` |
|      - | 4839 | `	char zBuf[512];` |
|     69 | 4840 | `	int i, m = 0, seenDigit = 0;` |
|     69 | 4841 | `	const char *zv; int nv; double d = 0;` |
|     69 | 4842 | `	FvTrim(&z,&n);` |
|      - | 4843 | `	/* Bound the input: zBuf[512] holds the thousand-separator-stripped copy, and` |
|      - | 4844 | `	 * the cap also rejects the pathological 500+ digit floats PHP refuses. */` |
|     69 | 4845 | `	if( n==0 \|\| n>500 ){ return 0; }` |
|     69 | 4846 | `	if( flags & FV_FLAG_ALLOW_THOUSAND ){` |
|      - | 4847 | `		/* Commas are optional, but when present they must group the integer part` |
|      - | 4848 | `		 * into a leading run of 1..3 digits followed by groups of exactly 3` |
|      - | 4849 | `		 * ("1,000" ok, "1,5"/"1234,567" rejected). Strip them into zBuf and reject` |
|      - | 4850 | `		 * a comma anywhere in the fractional/exponent tail. */` |
|     25 | 4851 | `		int s = 0, intEnd, segStart, segIdx, hasComma = 0;` |
|     25 | 4852 | `		if( s<n && (z[s]=='+'\|\|z[s]=='-') ){ zBuf[m++] = z[s]; s++; }` |
|     25 | 4853 | `		intEnd = s;` |
|    167 | 4854 | `		while( intEnd<n && z[intEnd]!='.' && z[intEnd]!='e' && z[intEnd]!='E' ){` |
|    143 | 4855 | `			if( z[intEnd]==',' ){ hasComma = 1; }` |
|    143 | 4856 | `			intEnd++;` |
|      1 | 4857 | `		}` |
|     25 | 4858 | `		if( hasComma ){` |
|     25 | 4859 | `			segStart = s; segIdx = 0;` |
|    165 | 4860 | `			for( i=s; i<=intEnd; i++ ){` |
|    151 | 4861 | `				if( i==intEnd \|\| z[i]==',' ){` |
|     49 | 4862 | `					int segLen = i - segStart, k;` |
|     49 | 4863 | `					if( segIdx==0 ){ if( segLen<1 \|\| segLen>3 ){ return 0; } }` |
|     25 | 4864 | `					else if( segLen!=3 ){ return 0; }` |
|    119 | 4865 | `					for( k=segStart; k<i; k++ ){` |
|     81 | 4866 | `						if( !SyisDigit((unsigned char)z[k]) ){ return 0; }` |
|     81 | 4867 | `						zBuf[m++] = z[k];` |
|     41 | 4868 | `					}` |
|     39 | 4869 | `					segStart = i+1; segIdx++;` |
|     19 | 4870 | `				}` |
|     71 | 4871 | `			}` |
|      8 | 4872 | `		}else{` |
|    ! 0 | 4873 | `			for( i=s; i<intEnd; i++ ){ zBuf[m++] = z[i]; }` |
|      - | 4874 | `		}` |
|     27 | 4875 | `		for( i=intEnd; i<n; i++ ){` |
|     13 | 4876 | `			if( z[i]==',' ){ return 0; }` |
|     13 | 4877 | `			zBuf[m++] = z[i];` |
|      7 | 4878 | `		}` |
|     15 | 4879 | `		zv = zBuf; nv = m;` |
|      8 | 4880 | `	}else{` |
|     45 | 4881 | `		zv = z; nv = n;` |
|      - | 4882 | `	}` |
|     59 | 4883 | `	i = 0;` |
|     59 | 4884 | `	if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|    167 | 4885 | `	while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     59 | 4886 | `	if( i<nv && zv[i]=='.' ){` |
|     21 | 4887 | `		i++;` |
|     39 | 4888 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     10 | 4889 | `	}` |
|     59 | 4890 | `	if( !seenDigit ){ return 0; }` |
|     57 | 4891 | `	if( i<nv && (zv[i]=='e'\|\|zv[i]=='E') ){` |
|     29 | 4892 | `		i++;` |
|     29 | 4893 | `		if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|     29 | 4894 | `		if( i>=nv \|\| !SyisDigit((unsigned char)zv[i]) ){ return 0; }` |
|    105 | 4895 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; }` |
|     14 | 4896 | `	}` |
|     57 | 4897 | `	if( i!=nv ){ return 0; } /* trailing junk */` |
|      - | 4898 | `	/* The grammar above guarantees zv[0..nv) is a clean ASCII decimal float (no hex /` |
|      - | 4899 | `	 * inf / nan / trailing junk), so it is safe to hand to libc strtod, which — unlike` |
|      - | 4900 | `	 * SyStrToReal (15 sig-digits + exponent clamped to 308, so it silently saturates` |
|      - | 4901 | `	 * overflowing magnitudes to a finite value) — is overflow/underflow-aware and` |
|      - | 4902 | `	 * correctly rounded. strtod needs a NUL-terminated string: the ALLOW_THOUSAND path` |
|      - | 4903 | `	 * already built the span in zBuf (zv==zBuf); the plain path must copy it there (z is` |
|      - | 4904 | `	 * const + not NUL-terminated). nv <= n <= 500 < sizeof(zBuf) by the cap above.` |
|      - | 4905 | `	 * Matches PHP 8.5 byte-for-byte: reject overflow (-> +/-INF) and total underflow` |
|      - | 4906 | `	 * (-> 0.0), keep subnormals (nonzero, errno==ERANGE) and a genuine "0" (errno==0). */` |
|     53 | 4907 | `	if( zv != zBuf ){ SyMemcpy(zv,zBuf,(sxu32)nv); }` |
|     53 | 4908 | `	zBuf[nv] = 0;` |
|     53 | 4909 | `	errno = 0;` |
|     53 | 4910 | `	d = strtod(zBuf,0);` |
|     53 | 4911 | `	if( errno == ERANGE && (d == HUGE_VAL \|\| d == -HUGE_VAL \|\| d == 0.0) ){` |
|     15 | 4912 | `		return 0;` |
|      - | 4913 | `	}` |
|     39 | 4914 | `	*pOut = d;` |
|     39 | 4915 | `	return 1;` |
|     35 | 4916 | `}` |
|      - | 4917 | `/* FILTER_VALIDATE_BOOLEAN. Returns 1 if the string is recognized (sets *pBool),` |
|      - | 4918 | ` * 0 if it is unrecognized (the failure path). "0"/"false"/"" are recognized as` |
|      - | 4919 | ` * false, NOT failures. */` |
|     33 | 4920 | `static int FvValidateBool(const char *z,int n,int *pBool){` |
|     33 | 4921 | `	FvTrim(&z,&n);` |
|     32 | 4922 | `	if( (n==1 && z[0]=='1') \|\| (n==4 && SyStrnicmp(z,"true",4)==0)` |
|     25 | 4923 | `	    \|\| (n==2 && SyStrnicmp(z,"on",2)==0) \|\| (n==3 && SyStrnicmp(z,"yes",3)==0) ){` |
|     11 | 4924 | `		*pBool = 1; return 1;` |
|      - | 4925 | `	}` |
|     22 | 4926 | `	if( n==0 \|\| (n==1 && z[0]=='0') \|\| (n==5 && SyStrnicmp(z,"false",5)==0)` |
|     11 | 4927 | `	    \|\| (n==3 && SyStrnicmp(z,"off",3)==0) \|\| (n==2 && SyStrnicmp(z,"no",2)==0) ){` |
|     11 | 4928 | `		*pBool = 0; return 1;` |
|      - | 4929 | `	}` |
|      9 | 4930 | `	return 0;` |
|     15 | 4931 | `}` |
|      - | 4932 | `/* IPv4 dotted-quad: exactly 4 octets 0..255, no leading zeros. */` |
|     33 | 4933 | `static int FvValidateIp4(const char *z,int n){` |
|     33 | 4934 | `	int i = 0, parts = 0;` |
|     77 | 4935 | `	while( i<n ){` |
|     65 | 4936 | `		int val = 0, digits = 0, start = i;` |
|    143 | 4937 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){` |
|     85 | 4938 | `			val = val*10 + (z[i]-'0');` |
|     85 | 4939 | `			if( val>255 ){ return 0; }` |
|     79 | 4940 | `			digits++; i++;` |
|      1 | 4941 | `		}` |
|     59 | 4942 | `		if( digits==0 \|\| digits>3 ){ return 0; }` |
|     49 | 4943 | `		if( digits>1 && z[start]=='0' ){ return 0; } /* leading zero */` |
|     45 | 4944 | `		parts++;` |
|     45 | 4945 | `		if( parts>4 ){ return 0; }` |
|     45 | 4946 | `		if( i<n ){` |
|     33 | 4947 | `			if( z[i]!='.' ){ return 0; }` |
|     33 | 4948 | `			i++;` |
|     33 | 4949 | `			if( i>=n ){ return 0; } /* trailing dot */` |
|     16 | 4950 | `		}` |
|      1 | 4951 | `	}` |
|     13 | 4952 | `	return parts==4;` |
|     17 | 4953 | `}` |
|      - | 4954 | `/* A colon-separated run of IPv6 hextets with no "::" (n may be 0 -> 0 groups),` |
|      - | 4955 | ` * allowing a trailing embedded IPv4. Returns the 16-bit group count or -1. */` |
|     19 | 4956 | `static int FvIp6Hextets(const char *z,int n){` |
|     19 | 4957 | `	int i = 0, segStart = 0, groups = 0;` |
|     19 | 4958 | `	if( n==0 ){ return 0; }` |
|    145 | 4959 | `	while( i<=n ){` |
|    133 | 4960 | `		if( i==n \|\| z[i]==':' ){` |
|     23 | 4961 | `			int segLen = i - segStart, j, isV4 = 0;` |
|     23 | 4962 | `			if( segLen==0 ){ return -1; } /* an empty hextet (stray ':') */` |
|     77 | 4963 | `			for( j=segStart; j<i; j++ ){ if( z[j]=='.' ){ isV4 = 1; break; } }` |
|     23 | 4964 | `			if( isV4 ){` |
|     11 | 4965 | `				if( i!=n ){ return -1; } /* IPv4 only as the final token */` |
|     11 | 4966 | `				if( !FvValidateIp4(z+segStart,segLen) ){ return -1; }` |
|      5 | 4967 | `				groups += 2;` |
|      3 | 4968 | `			}else{` |
|     13 | 4969 | `				if( segLen>4 ){ return -1; }` |
|     47 | 4970 | `				for( j=segStart; j<i; j++ ){ if( SyHexToint((unsigned char)z[j])<0 ){ return -1; } }` |
|     13 | 4971 | `				groups++;` |
|      - | 4972 | `			}` |
|     17 | 4973 | `			segStart = i+1;` |
|      8 | 4974 | `		}` |
|    127 | 4975 | `		i++;` |
|      1 | 4976 | `	}` |
|     13 | 4977 | `	return groups;` |
|     10 | 4978 | `}` |
|      - | 4979 | `/* IPv6: at most one "::" zero-run; 8 groups exactly, or fewer when "::" present. */` |
|     19 | 4980 | `static int FvValidateIp6(const char *z,int n){` |
|     19 | 4981 | `	const char *zDbl = 0;` |
|      - | 4982 | `	int i, ga, gb;` |
|    139 | 4983 | `	for( i=0; i+1<n; i++ ){` |
|    123 | 4984 | `		if( z[i]==':' && z[i+1]==':' ){` |
|     13 | 4985 | `			if( zDbl ){ return 0; } /* a second "::" is invalid */` |
|     11 | 4986 | `			zDbl = z+i;` |
|      5 | 4987 | `		}` |
|     61 | 4988 | `	}` |
|     17 | 4989 | `	if( zDbl==0 ){` |
|      9 | 4990 | `		return FvIp6Hextets(z,n)==8;` |
|    ! 0 | 4991 | `	}else{` |
|      9 | 4992 | `		int lenA = (int)(zDbl - z);` |
|      9 | 4993 | `		int lenB = n - lenA - 2;` |
|      9 | 4994 | `		ga = (lenA==0) ? 0 : FvIp6Hextets(z,lenA);` |
|      9 | 4995 | `		gb = (lenB==0) ? 0 : FvIp6Hextets(zDbl+2,lenB);` |
|      9 | 4996 | `		if( ga<0 \|\| gb<0 ){ return 0; }` |
|      9 | 4997 | `		return (ga+gb)<=7; /* "::" stands for at least one zero group */` |
|      - | 4998 | `	}` |
|     10 | 4999 | `}` |
|     25 | 5000 | `static int FvValidateIp(const char *z,int n,int flags){` |
|     25 | 5001 | `	int v4 = (flags & FV_FLAG_IPV4), v6 = (flags & FV_FLAG_IPV6);` |
|     25 | 5002 | `	if( !v4 && !v6 ){ v4 = v6 = 1; } /* default accepts either family */` |
|     25 | 5003 | `	if( v4 && FvValidateIp4(z,n) ){ return 1; }` |
|     21 | 5004 | `	if( v6 && FvValidateIp6(z,n) ){ return 1; }` |
|     13 | 5005 | `	return 0;` |
|     13 | 5006 | `}` |
|      - | 5007 | `/* FILTER_VALIDATE_MAC: 17-char colon- or dash-separated hex (XX:XX:..:XX). */` |
|     11 | 5008 | `static int FvValidateMac(const char *z,int n){` |
|      - | 5009 | `	char sep;` |
|      - | 5010 | `	int i;` |
|     11 | 5011 | `	if( n!=17 ){ return 0; }` |
|      7 | 5012 | `	sep = z[2];` |
|      7 | 5013 | `	if( sep!=':' && sep!='-' ){ return 0; }` |
|    105 | 5014 | `	for( i=0; i<17; i++ ){` |
|    101 | 5015 | `		if( (i%3)==2 ){ if( z[i]!=sep ){ return 0; } }` |
|     71 | 5016 | `		else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }` |
|     50 | 5017 | `	}` |
|      5 | 5018 | `	return 1;` |
|      6 | 5019 | `}` |
|      - | 5020 | `/* FILTER_VALIDATE_EMAIL (best-effort: covers the common cases, not quoted local` |
|      - | 5021 | ` * parts or IP-literal domains). */` |
|     28 | 5022 | `static int FvValidateEmail(const char *z,int n){` |
|     28 | 5023 | `	int at = -1, i, localLen, domLen, labelStart, dotCount = 0;` |
|      - | 5024 | `	const char *zDom;` |
|     28 | 5025 | `	if( n==0 \|\| n>320 ){ return 0; }` |
|    201 | 5026 | `	for( i=0; i<n; i++ ){` |
|    181 | 5027 | `		if( z[i]=='@' ){ if( at>=0 ){ return 0; } at = i; }` |
|     91 | 5028 | `	}` |
|     21 | 5029 | `	if( at<=0 \|\| at==n-1 ){ return 0; } /* one '@', non-empty local and domain */` |
|     21 | 5030 | `	localLen = at;` |
|     21 | 5031 | `	zDom = z + at + 1;` |
|     21 | 5032 | `	domLen = n - at - 1;` |
|     21 | 5033 | `	if( z[0]=='.' \|\| z[at-1]=='.' ){ return 0; }` |
|     57 | 5034 | `	for( i=0; i<localLen; i++ ){` |
|     43 | 5035 | `		unsigned char c = (unsigned char)z[i];` |
|     43 | 5036 | `		if( c<=' ' ){ return 0; }` |
|     41 | 5037 | `		if( c=='.' && i+1<localLen && z[i+1]=='.' ){ return 0; }` |
|     20 | 5038 | `	}` |
|     15 | 5039 | `	if( zDom[0]=='.' \|\| zDom[domLen-1]=='.' ){ return 0; }` |
|     13 | 5040 | `	labelStart = 0;` |
|     85 | 5041 | `	for( i=0; i<=domLen; i++ ){` |
|     75 | 5042 | `		if( i==domLen \|\| zDom[i]=='.' ){` |
|     25 | 5043 | `			int ll = i - labelStart;` |
|     25 | 5044 | `			if( ll==0 ){ return 0; } /* consecutive dots */` |
|     23 | 5045 | `			if( zDom[labelStart]=='-' \|\| zDom[i-1]=='-' ){ return 0; }` |
|     23 | 5046 | `			if( i<domLen ){ dotCount++; }` |
|     23 | 5047 | `			labelStart = i+1;` |
|     12 | 5048 | `		}else{` |
|     51 | 5049 | `			unsigned char c = (unsigned char)zDom[i];` |
|     51 | 5050 | `			if( !((c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9')\|\|c=='-') ){ return 0; }` |
|      - | 5051 | `		}` |
|     37 | 5052 | `	}` |
|     11 | 5053 | `	if( dotCount<1 ){ return 0; } /* PHP requires a dot in the domain (any TLD length) */` |
|      9 | 5054 | `	return 1;` |
|     15 | 5055 | `}` |
|      - | 5056 | `/* FILTER_VALIDATE_DOMAIN (lenient, matching PHP without FILTER_FLAG_HOSTNAME). */` |
|     11 | 5057 | `static int FvValidateDomain(const char *z,int n){` |
|      - | 5058 | `	int i;` |
|     11 | 5059 | `	if( n<1 \|\| n>253 \|\| z[0]=='.' ){ return 0; }` |
|     81 | 5060 | `	for( i=0; i<n; i++ ){` |
|     75 | 5061 | `		unsigned char c = (unsigned char)z[i];` |
|     75 | 5062 | `		if( c<=' ' ){ return 0; }` |
|     75 | 5063 | `		if( c=='.' && i+1<n && z[i+1]=='.' ){ return 0; }` |
|     37 | 5064 | `	}` |
|      7 | 5065 | `	return 1;` |
|      6 | 5066 | `}` |
|      - | 5067 | `/* FILTER_VALIDATE_URL: require a scheme and a host (PHP's filter is itself` |
|      - | 5068 | ` * parse_url-based, so PH7_VmHttpSplitURI tracks it closely). */` |
|     15 | 5069 | `static int FvValidateUrl(const char *z,int n){` |
|      - | 5070 | `	SyhttpUri sUri;` |
|     15 | 5071 | `	if( n==0 ){ return 0; }` |
|     15 | 5072 | `	SyZero(&sUri,(sxu32)sizeof(sUri));` |
|     15 | 5073 | `	if( PH7_VmHttpSplitURI(&sUri,z,(sxu32)n)!=SXRET_OK ){ return 0; }` |
|     15 | 5074 | `	return sUri.sScheme.nByte!=0 && sUri.sHost.nByte!=0;` |
|      8 | 5075 | `}` |
|      - | 5076 | `/* The Fv sanitizers build their result by appending directly to the call` |
|      - | 5077 | ` * context (ph7_result_string accumulates, like htmlspecialchars), emitting each` |
|      - | 5078 | ` * kept run in one call and seeding "" so an all-stripped input yields "". */` |
|      - | 5079 | `/* SANITIZE_NUMBER_INT (isFloat=0) / SANITIZE_NUMBER_FLOAT (isFloat=1). */` |
|     37 | 5080 | `static void FvSanitizeNumber(ph7_context *pCtx,const char *z,int n,int isFloat,int flags){` |
|     37 | 5081 | `	int i, runStart = 0;` |
|     37 | 5082 | `	ph7_result_string(pCtx,"",0);` |
|     97 | 5083 | `	for( i=0; i<n; i++ ){` |
|     91 | 5084 | `		char c = z[i];` |
|     91 | 5085 | `		int keep = (c>='0'&&c<='9') \|\| c=='+' \|\| c=='-';` |
|     91 | 5086 | `		if( !keep && isFloat ){` |
|     38 | 5087 | `			keep = (c=='.' && (flags & FV_FLAG_ALLOW_FRACTION))` |
|     23 | 5088 | `			    \|\| (c==',' && (flags & FV_FLAG_ALLOW_THOUSAND))` |
|     36 | 5089 | `			    \|\| ((c=='e'\|\|c=='E') && (flags & FV_FLAG_ALLOW_SCIENTIFIC));` |
|     12 | 5090 | `		}` |
|     61 | 5091 | `		if( !keep ){` |
|     33 | 5092 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     33 | 5093 | `			runStart = i+1;` |
|     16 | 5094 | `		}` |
|     31 | 5095 | `	}` |
|      7 | 5096 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 | 5097 | `}` |
|      - | 5098 | `/* Return non-zero when byte c must be stripped under the STRIP_* flags. Shared` |
|      - | 5099 | ` * by the UNSAFE_RAW string filter and SANITIZE_SPECIAL_CHARS. STRIP_LOW drops` |
|      - | 5100 | `` * bytes <32, STRIP_HIGH drops bytes >=127 (incl. DEL), STRIP_BACKTICK drops '`'.`` |
|      - | 5101 | ` * Matches php_filter_strip(); verified byte-exact vs php 8.5.7. */` |
|    287 | 5102 | `static int FvStripByte(unsigned char c,int flags){` |
|    287 | 5103 | `	if( (flags & FV_FLAG_STRIP_LOW)      && c<32 )    { return 1; }` |
|    281 | 5104 | `	if( (flags & FV_FLAG_STRIP_HIGH)     && c>=127 )  { return 1; }` |
|    269 | 5105 | `	if( (flags & FV_FLAG_STRIP_BACKTICK) && c==0x60 ) { return 1; }` |
|    267 | 5106 | `	return 0;` |
|    144 | 5107 | `}` |
|      - | 5108 | `/* FILTER_UNSAFE_RAW / FILTER_DEFAULT with flags: no default transform, but the` |
|      - | 5109 | ` * STRIP/ENCODE flags apply. Precedence (per php_filter_unsafe_raw, verified` |
|      - | 5110 | ` * vs php 8.5.7): a byte is first tested for stripping; a surviving byte is then` |
|      - | 5111 | ` * encoded as a decimal numeric entity if ENCODE_LOW (<32) / ENCODE_HIGH (>=127)` |
|      - | 5112 | ` * is set, and '&' becomes "&#38;" under ENCODE_AMP. So STRIP_LOW\|ENCODE_LOW` |
|      - | 5113 | ` * strips (nothing left to encode). Bytes are treated individually — ENCODE_HIGH` |
|      - | 5114 | ` * numeric-encodes each byte of a multibyte sequence separately, not the codepoint. */` |
|     25 | 5115 | `static void FvSanitizeString(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 5116 | `	int i, runStart = 0;` |
|     25 | 5117 | `	ph7_result_string(pCtx,"",0);` |
|    193 | 5118 | `	for( i=0; i<n; i++ ){` |
|    179 | 5119 | `		unsigned char c = (unsigned char)z[i];` |
|    179 | 5120 | `		if( FvStripByte(c,flags) ){` |
|     13 | 5121 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     13 | 5122 | `			runStart = i+1;` |
|     13 | 5123 | `			continue;` |
|      - | 5124 | `		}` |
|    167 | 5125 | `		if( c=='&' && (flags & FV_FLAG_ENCODE_AMP) ){` |
|      3 | 5126 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      3 | 5127 | `			ph7_result_string(pCtx,"&#38;",-1);` |
|      3 | 5128 | `			runStart = i+1;` |
|    166 | 5129 | `		}else if( (c<32 && (flags & FV_FLAG_ENCODE_LOW))` |
|    164 | 5130 | `		       \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     37 | 5131 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 5132 | `			ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|      9 | 5133 | `			runStart = i+1;` |
|      4 | 5134 | `		}` |
|     79 | 5135 | `	}` |
|     15 | 5136 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     15 | 5137 | `}` |
|      - | 5138 | `/* FILTER_SANITIZE_SPECIAL_CHARS: encode <>&"' and every control byte <32 as a` |
|      - | 5139 | ` * decimal numeric entity (&#60; &#38; &#34; ...). The STRIP_* flags remove bytes` |
|      - | 5140 | ` * before encoding; ENCODE_HIGH numeric-encodes surviving bytes >=127. Bytes >=128` |
|      - | 5141 | ` * are otherwise passed through verbatim (this filter is NOT UTF-8-aware — only the` |
|      - | 5142 | ` * FULL variant is). Byte-exact vs php 8.5.7. */` |
|     13 | 5143 | `static void FvSanitizeSpecial(ph7_context *pCtx,const char *z,int n,int flags){` |
|     13 | 5144 | `	int i, runStart = 0;` |
|      - | 5145 | `	const char *zEnt;` |
|     13 | 5146 | `	ph7_result_string(pCtx,"",0);` |
|    131 | 5147 | `	for( i=0; i<n; i++ ){` |
|    119 | 5148 | `		unsigned char c = (unsigned char)z[i];` |
|    119 | 5149 | `		if( FvStripByte(c,flags) ){` |
|      9 | 5150 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 5151 | `			runStart = i+1;` |
|      9 | 5152 | `			continue;` |
|      - | 5153 | `		}` |
|    111 | 5154 | `		switch( c ){` |
|      3 | 5155 | `		case '<':  zEnt = "&#60;"; break;` |
|      3 | 5156 | `		case '>':  zEnt = "&#62;"; break;` |
|     11 | 5157 | `		case '&':  zEnt = "&#38;"; break;` |
|      3 | 5158 | `		case '"':  zEnt = "&#34;"; break;` |
|      3 | 5159 | `		case '\'': zEnt = "&#39;"; break;` |
|     46 | 5160 | `		default:` |
|      - | 5161 | `			/* Control bytes <32 are always numeric-encoded; bytes >=127 only when` |
|      - | 5162 | `			 * ENCODE_HIGH is set. Everything else stays in the current run. */` |
|     93 | 5163 | `			if( c<32 \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     17 | 5164 | `				if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     17 | 5165 | `				ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|     17 | 5166 | `				runStart = i+1;` |
|      8 | 5167 | `			}` |
|     93 | 5168 | `			continue; /* keep in the current run */` |
|      - | 5169 | `		}` |
|     19 | 5170 | `		if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     19 | 5171 | `		ph7_result_string(pCtx,zEnt,-1); /* -1: length from strlen */` |
|     19 | 5172 | `		runStart = i+1;` |
|     10 | 5173 | `	}` |
|     13 | 5174 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     13 | 5175 | `}` |
|      - | 5176 | `/* HTML 4.01 named-entity table (codepoint -> "&name;") used by the UTF-8-aware` |
|      - | 5177 | ` * FULL_SPECIAL_CHARS filter, sorted ascending by codepoint for binary search.` |
|      - | 5178 | ` * Generated from php 8.5.7 (the exact set php_escape_html_entities emits for the` |
|      - | 5179 | ` * default document type); the five inline specials <>&"' are handled separately,` |
|      - | 5180 | ` * so every entry here is a codepoint >=0xA0. 248 rows. */` |
|      - | 5181 | `static const struct { sxu32 cp; const char *zEnt; } aHtml401Ent[] = {` |
|      - | 5182 | `	{0x00A0,"&nbsp;"},{0x00A1,"&iexcl;"},{0x00A2,"&cent;"},{0x00A3,"&pound;"},` |
|      - | 5183 | `	{0x00A4,"&curren;"},{0x00A5,"&yen;"},{0x00A6,"&brvbar;"},{0x00A7,"&sect;"},` |
|      - | 5184 | `	{0x00A8,"&uml;"},{0x00A9,"&copy;"},{0x00AA,"&ordf;"},{0x00AB,"&laquo;"},` |
|      - | 5185 | `	{0x00AC,"&not;"},{0x00AD,"&shy;"},{0x00AE,"&reg;"},{0x00AF,"&macr;"},` |
|      - | 5186 | `	{0x00B0,"&deg;"},{0x00B1,"&plusmn;"},{0x00B2,"&sup2;"},{0x00B3,"&sup3;"},` |
|      - | 5187 | `	{0x00B4,"&acute;"},{0x00B5,"&micro;"},{0x00B6,"&para;"},{0x00B7,"&middot;"},` |
|      - | 5188 | `	{0x00B8,"&cedil;"},{0x00B9,"&sup1;"},{0x00BA,"&ordm;"},{0x00BB,"&raquo;"},` |
|      - | 5189 | `	{0x00BC,"&frac14;"},{0x00BD,"&frac12;"},{0x00BE,"&frac34;"},{0x00BF,"&iquest;"},` |
|      - | 5190 | `	{0x00C0,"&Agrave;"},{0x00C1,"&Aacute;"},{0x00C2,"&Acirc;"},{0x00C3,"&Atilde;"},` |
|      - | 5191 | `	{0x00C4,"&Auml;"},{0x00C5,"&Aring;"},{0x00C6,"&AElig;"},{0x00C7,"&Ccedil;"},` |
|      - | 5192 | `	{0x00C8,"&Egrave;"},{0x00C9,"&Eacute;"},{0x00CA,"&Ecirc;"},{0x00CB,"&Euml;"},` |
|      - | 5193 | `	{0x00CC,"&Igrave;"},{0x00CD,"&Iacute;"},{0x00CE,"&Icirc;"},{0x00CF,"&Iuml;"},` |
|      - | 5194 | `	{0x00D0,"&ETH;"},{0x00D1,"&Ntilde;"},{0x00D2,"&Ograve;"},{0x00D3,"&Oacute;"},` |
|      - | 5195 | `	{0x00D4,"&Ocirc;"},{0x00D5,"&Otilde;"},{0x00D6,"&Ouml;"},{0x00D7,"&times;"},` |
|      - | 5196 | `	{0x00D8,"&Oslash;"},{0x00D9,"&Ugrave;"},{0x00DA,"&Uacute;"},{0x00DB,"&Ucirc;"},` |
|      - | 5197 | `	{0x00DC,"&Uuml;"},{0x00DD,"&Yacute;"},{0x00DE,"&THORN;"},{0x00DF,"&szlig;"},` |
|      - | 5198 | `	{0x00E0,"&agrave;"},{0x00E1,"&aacute;"},{0x00E2,"&acirc;"},{0x00E3,"&atilde;"},` |
|      - | 5199 | `	{0x00E4,"&auml;"},{0x00E5,"&aring;"},{0x00E6,"&aelig;"},{0x00E7,"&ccedil;"},` |
|      - | 5200 | `	{0x00E8,"&egrave;"},{0x00E9,"&eacute;"},{0x00EA,"&ecirc;"},{0x00EB,"&euml;"},` |
|      - | 5201 | `	{0x00EC,"&igrave;"},{0x00ED,"&iacute;"},{0x00EE,"&icirc;"},{0x00EF,"&iuml;"},` |
|      - | 5202 | `	{0x00F0,"&eth;"},{0x00F1,"&ntilde;"},{0x00F2,"&ograve;"},{0x00F3,"&oacute;"},` |
|      - | 5203 | `	{0x00F4,"&ocirc;"},{0x00F5,"&otilde;"},{0x00F6,"&ouml;"},{0x00F7,"&divide;"},` |
|      - | 5204 | `	{0x00F8,"&oslash;"},{0x00F9,"&ugrave;"},{0x00FA,"&uacute;"},{0x00FB,"&ucirc;"},` |
|      - | 5205 | `	{0x00FC,"&uuml;"},{0x00FD,"&yacute;"},{0x00FE,"&thorn;"},{0x00FF,"&yuml;"},` |
|      - | 5206 | `	{0x0152,"&OElig;"},{0x0153,"&oelig;"},{0x0160,"&Scaron;"},{0x0161,"&scaron;"},` |
|      - | 5207 | `	{0x0178,"&Yuml;"},{0x0192,"&fnof;"},{0x02C6,"&circ;"},{0x02DC,"&tilde;"},` |
|      - | 5208 | `	{0x0391,"&Alpha;"},{0x0392,"&Beta;"},{0x0393,"&Gamma;"},{0x0394,"&Delta;"},` |
|      - | 5209 | `	{0x0395,"&Epsilon;"},{0x0396,"&Zeta;"},{0x0397,"&Eta;"},{0x0398,"&Theta;"},` |
|      - | 5210 | `	{0x0399,"&Iota;"},{0x039A,"&Kappa;"},{0x039B,"&Lambda;"},{0x039C,"&Mu;"},` |
|      - | 5211 | `	{0x039D,"&Nu;"},{0x039E,"&Xi;"},{0x039F,"&Omicron;"},{0x03A0,"&Pi;"},` |
|      - | 5212 | `	{0x03A1,"&Rho;"},{0x03A3,"&Sigma;"},{0x03A4,"&Tau;"},{0x03A5,"&Upsilon;"},` |
|      - | 5213 | `	{0x03A6,"&Phi;"},{0x03A7,"&Chi;"},{0x03A8,"&Psi;"},{0x03A9,"&Omega;"},` |
|      - | 5214 | `	{0x03B1,"&alpha;"},{0x03B2,"&beta;"},{0x03B3,"&gamma;"},{0x03B4,"&delta;"},` |
|      - | 5215 | `	{0x03B5,"&epsilon;"},{0x03B6,"&zeta;"},{0x03B7,"&eta;"},{0x03B8,"&theta;"},` |
|      - | 5216 | `	{0x03B9,"&iota;"},{0x03BA,"&kappa;"},{0x03BB,"&lambda;"},{0x03BC,"&mu;"},` |
|      - | 5217 | `	{0x03BD,"&nu;"},{0x03BE,"&xi;"},{0x03BF,"&omicron;"},{0x03C0,"&pi;"},` |
|      - | 5218 | `	{0x03C1,"&rho;"},{0x03C2,"&sigmaf;"},{0x03C3,"&sigma;"},{0x03C4,"&tau;"},` |
|      - | 5219 | `	{0x03C5,"&upsilon;"},{0x03C6,"&phi;"},{0x03C7,"&chi;"},{0x03C8,"&psi;"},` |
|      - | 5220 | `	{0x03C9,"&omega;"},{0x03D1,"&thetasym;"},{0x03D2,"&upsih;"},{0x03D6,"&piv;"},` |
|      - | 5221 | `	{0x2002,"&ensp;"},{0x2003,"&emsp;"},{0x2009,"&thinsp;"},{0x200C,"&zwnj;"},` |
|      - | 5222 | `	{0x200D,"&zwj;"},{0x200E,"&lrm;"},{0x200F,"&rlm;"},{0x2013,"&ndash;"},` |
|      - | 5223 | `	{0x2014,"&mdash;"},{0x2018,"&lsquo;"},{0x2019,"&rsquo;"},{0x201A,"&sbquo;"},` |
|      - | 5224 | `	{0x201C,"&ldquo;"},{0x201D,"&rdquo;"},{0x201E,"&bdquo;"},{0x2020,"&dagger;"},` |
|      - | 5225 | `	{0x2021,"&Dagger;"},{0x2022,"&bull;"},{0x2026,"&hellip;"},{0x2030,"&permil;"},` |
|      - | 5226 | `	{0x2032,"&prime;"},{0x2033,"&Prime;"},{0x2039,"&lsaquo;"},{0x203A,"&rsaquo;"},` |
|      - | 5227 | `	{0x203E,"&oline;"},{0x2044,"&frasl;"},{0x20AC,"&euro;"},{0x2111,"&image;"},` |
|      - | 5228 | `	{0x2118,"&weierp;"},{0x211C,"&real;"},{0x2122,"&trade;"},{0x2135,"&alefsym;"},` |
|      - | 5229 | `	{0x2190,"&larr;"},{0x2191,"&uarr;"},{0x2192,"&rarr;"},{0x2193,"&darr;"},` |
|      - | 5230 | `	{0x2194,"&harr;"},{0x21B5,"&crarr;"},{0x21D0,"&lArr;"},{0x21D1,"&uArr;"},` |
|      - | 5231 | `	{0x21D2,"&rArr;"},{0x21D3,"&dArr;"},{0x21D4,"&hArr;"},{0x2200,"&forall;"},` |
|      - | 5232 | `	{0x2202,"&part;"},{0x2203,"&exist;"},{0x2205,"&empty;"},{0x2207,"&nabla;"},` |
|      - | 5233 | `	{0x2208,"&isin;"},{0x2209,"&notin;"},{0x220B,"&ni;"},{0x220F,"&prod;"},` |
|      - | 5234 | `	{0x2211,"&sum;"},{0x2212,"&minus;"},{0x2217,"&lowast;"},{0x221A,"&radic;"},` |
|      - | 5235 | `	{0x221D,"&prop;"},{0x221E,"&infin;"},{0x2220,"&ang;"},{0x2227,"&and;"},` |
|      - | 5236 | `	{0x2228,"&or;"},{0x2229,"&cap;"},{0x222A,"&cup;"},{0x222B,"&int;"},` |
|      - | 5237 | `	{0x2234,"&there4;"},{0x223C,"&sim;"},{0x2245,"&cong;"},{0x2248,"&asymp;"},` |
|      - | 5238 | `	{0x2260,"&ne;"},{0x2261,"&equiv;"},{0x2264,"&le;"},{0x2265,"&ge;"},` |
|      - | 5239 | `	{0x2282,"&sub;"},{0x2283,"&sup;"},{0x2284,"&nsub;"},{0x2286,"&sube;"},` |
|      - | 5240 | `	{0x2287,"&supe;"},{0x2295,"&oplus;"},{0x2297,"&otimes;"},{0x22A5,"&perp;"},` |
|      - | 5241 | `	{0x22C5,"&sdot;"},{0x2308,"&lceil;"},{0x2309,"&rceil;"},{0x230A,"&lfloor;"},` |
|      - | 5242 | `	{0x230B,"&rfloor;"},{0x2329,"&lang;"},{0x232A,"&rang;"},{0x25CA,"&loz;"},` |
|      - | 5243 | `	{0x2660,"&spades;"},{0x2663,"&clubs;"},{0x2665,"&hearts;"},{0x2666,"&diams;"}` |
|      - | 5244 | `};` |
|      - | 5245 | `/* Binary-search aHtml401Ent[] for cp; return its "&name;" entity or 0. */` |
|     17 | 5246 | `static const char *FvHtml401Lookup(sxu32 cp){` |
|     17 | 5247 | `	int lo = 0, hi = (int)SX_ARRAYSIZE(aHtml401Ent) - 1;` |
|    133 | 5248 | `	while( lo <= hi ){` |
|    127 | 5249 | `		int mid = (lo + hi) / 2;` |
|    127 | 5250 | `		sxu32 c = aHtml401Ent[mid].cp;` |
|    127 | 5251 | `		if( c == cp ){ return aHtml401Ent[mid].zEnt; }` |
|    117 | 5252 | `		if( c < cp ){ lo = mid + 1; } else { hi = mid - 1; }` |
|      1 | 5253 | `	}` |
|      7 | 5254 | `	return 0;` |
|      9 | 5255 | `}` |
|      - | 5256 | `/* Decode one strict-UTF-8 sequence at p (< zEnd). On success returns its byte` |
|      - | 5257 | ` * length (1..4) and sets *pCp to the codepoint; on any malformed, overlong,` |
|      - | 5258 | ` * surrogate, truncated or out-of-range (>U+10FFFF) sequence returns 0. Matches` |
|      - | 5259 | ` * PHP's UTF-8 validation used by FULL_SPECIAL_CHARS (verified vs php 8.5.7). */` |
|     95 | 5260 | `static int FvUtf8Next(const unsigned char *p,const unsigned char *zEnd,sxu32 *pCp){` |
|     95 | 5261 | `	unsigned char c = p[0];` |
|     95 | 5262 | `	if( c < 0x80 ){ *pCp = c; return 1; }` |
|     39 | 5263 | `	if( c < 0xC2 ){ return 0; }              /* 0x80-0xBF stray cont / 0xC0-0xC1 overlong */` |
|     39 | 5264 | `	if( c < 0xE0 ){                          /* 2-byte: U+0080..U+07FF */` |
|     17 | 5265 | `		if( zEnd-p < 2 \|\| (p[1]&0xC0)!=0x80 ){ return 0; }` |
|     17 | 5266 | `		*pCp = ((sxu32)(c&0x1F)<<6) \| (p[1]&0x3F);` |
|     17 | 5267 | `		return 2;` |
|      - | 5268 | `	}` |
|     23 | 5269 | `	if( c < 0xF0 ){                          /* 3-byte: U+0800..U+FFFF minus surrogates */` |
|      - | 5270 | `		sxu32 cp;` |
|     17 | 5271 | `		if( zEnd-p < 3 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 ){ return 0; }` |
|     15 | 5272 | `		cp = ((sxu32)(c&0x0F)<<12) \| ((sxu32)(p[1]&0x3F)<<6) \| (p[2]&0x3F);` |
|     15 | 5273 | `		if( cp < 0x800 \|\| (cp>=0xD800 && cp<=0xDFFF) ){ return 0; }` |
|     13 | 5274 | `		*pCp = cp;` |
|     13 | 5275 | `		return 3;` |
|      - | 5276 | `	}` |
|      7 | 5277 | `	if( c < 0xF5 ){                          /* 4-byte: U+10000..U+10FFFF */` |
|      - | 5278 | `		sxu32 cp;` |
|      5 | 5279 | `		if( zEnd-p < 4 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 \|\| (p[3]&0xC0)!=0x80 ){ return 0; }` |
|      5 | 5280 | `		cp = ((sxu32)(c&0x07)<<18) \| ((sxu32)(p[1]&0x3F)<<12) \| ((sxu32)(p[2]&0x3F)<<6) \| (p[3]&0x3F);` |
|      5 | 5281 | `		if( cp < 0x10000 \|\| cp > 0x10FFFF ){ return 0; }` |
|      5 | 5282 | `		*pCp = cp;` |
|      5 | 5283 | `		return 4;` |
|      - | 5284 | `	}` |
|      3 | 5285 | `	return 0;                                /* 0xF5-0xFF */` |
|     48 | 5286 | `}` |
|      - | 5287 | `/* FILTER_SANITIZE_FULL_SPECIAL_CHARS: htmlentities-style, UTF-8-aware. Encodes` |
|      - | 5288 | ` * <>&"' as named entities ("'" -> &#039;; quotes suppressed under NO_ENCODE_QUOTES),` |
|      - | 5289 | ` * and every valid UTF-8 codepoint with an HTML 4.01 named entity as that entity;` |
|      - | 5290 | ` * valid codepoints without a named entity (and low control bytes) pass through` |
|      - | 5291 | ` * verbatim. If the input contains ANY invalid UTF-8 the whole result is "".` |
|      - | 5292 | ` * The STRIP/ENCODE flags do NOT apply to this filter (only NO_ENCODE_QUOTES).` |
|      - | 5293 | ` * Byte-exact vs php 8.5.7. */` |
|     25 | 5294 | `static void FvSanitizeFull(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 5295 | `	const unsigned char *zEnd = (const unsigned char *)(z + n);` |
|     25 | 5296 | `	const unsigned char *p = (const unsigned char *)z;` |
|      - | 5297 | `	const unsigned char *runStart;` |
|     25 | 5298 | `	int bNoQuotes = (flags & FV_FLAG_NO_ENCODE_QUOTES) ? 1 : 0;` |
|      - | 5299 | `	sxu32 cp;` |
|      - | 5300 | `	/* Pass 1: reject the entire input on the first malformed UTF-8 sequence. */` |
|     97 | 5301 | `	while( p < zEnd ){` |
|     79 | 5302 | `		int len = FvUtf8Next(p,zEnd,&cp);` |
|     79 | 5303 | `		if( len==0 ){ ph7_result_string(pCtx,"",0); return; }` |
|     73 | 5304 | `		p += len;` |
|      1 | 5305 | `	}` |
|      - | 5306 | `	/* Pass 2: emit (input is now known to be valid UTF-8). */` |
|     19 | 5307 | `	p = (const unsigned char *)z;` |
|     19 | 5308 | `	runStart = p;` |
|     19 | 5309 | `	ph7_result_string(pCtx,"",0);` |
|     85 | 5310 | `	while( p < zEnd ){` |
|     67 | 5311 | `		const char *zEnt = 0;` |
|      - | 5312 | `		int len;` |
|     67 | 5313 | `		if( *p < 0x80 ){` |
|     51 | 5314 | `			switch( *p ){` |
|      7 | 5315 | `			case '<':  zEnt = "&lt;";  break;` |
|      5 | 5316 | `			case '>':  zEnt = "&gt;";  break;` |
|      7 | 5317 | `			case '&':  zEnt = "&amp;"; break;` |
|      7 | 5318 | `			case '"':  if( !bNoQuotes ){ zEnt = "&quot;"; } break;` |
|      7 | 5319 | `			case '\'': if( !bNoQuotes ){ zEnt = "&#039;"; } break;` |
|      - | 5320 | `			}` |
|     51 | 5321 | `			len = 1;` |
|     26 | 5322 | `		}else{` |
|     17 | 5323 | `			len = FvUtf8Next(p,zEnd,&cp);   /* len>0: validated in pass 1 */` |
|     17 | 5324 | `			zEnt = FvHtml401Lookup(cp);` |
|      - | 5325 | `		}` |
|     67 | 5326 | `		if( zEnt ){` |
|     35 | 5327 | `			if( p>runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|     35 | 5328 | `			ph7_result_string(pCtx,zEnt,-1);` |
|     35 | 5329 | `			runStart = p + len;` |
|     17 | 5330 | `		}` |
|     67 | 5331 | `		p += len;` |
|      1 | 5332 | `	}` |
|     19 | 5333 | `	if( zEnd>runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     13 | 5334 | `}` |
|     25 | 5335 | `static int FvEmailAllowed(unsigned char c){` |
|     25 | 5336 | `	if( (c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9') ){ return 1; }` |
|     16 | 5337 | `	return c=='!'\|\|c=='#'\|\|c=='$'\|\|c=='%'\|\|c=='&'\|\|c=='\''\|\|c=='*'\|\|c=='+'` |
|     10 | 5338 | ``	    \|\| c=='-'\|\|c=='='\|\|c=='?'\|\|c=='^'\|\|c=='_'\|\|c=='`'\|\|c=='{'\|\|c=='\|'`` |
|     15 | 5339 | `	    \|\| c=='}'\|\|c=='~'\|\|c=='@'\|\|c=='.'\|\|c=='['\|\|c==']';` |
|     13 | 5340 | `}` |
|     23 | 5341 | `static int FvUrlAllowed(unsigned char c){` |
|     23 | 5342 | `	return c>=33 && c<=126; /* PHP keeps every printable ASCII byte except space */` |
|      1 | 5343 | `}` |
|      - | 5344 | `/* SANITIZE_EMAIL (isUrl=0) / SANITIZE_URL (isUrl=1): strip disallowed bytes. */` |
|      5 | 5345 | `static void FvSanitizeChars(ph7_context *pCtx,const char *z,int n,int isUrl){` |
|      5 | 5346 | `	int i, runStart = 0;` |
|      5 | 5347 | `	ph7_result_string(pCtx,"",0);` |
|     51 | 5348 | `	for( i=0; i<n; i++ ){` |
|     47 | 5349 | `		unsigned char c = (unsigned char)z[i];` |
|     47 | 5350 | `		if( !(isUrl ? FvUrlAllowed(c) : FvEmailAllowed(c)) ){` |
|     11 | 5351 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     11 | 5352 | `			runStart = i+1;` |
|      5 | 5353 | `		}` |
|     24 | 5354 | `	}` |
|      5 | 5355 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      5 | 5356 | `}` |
|      - | 5357 | `/*` |
|      - | 5358 | ` * Apply the selected filter to one already-resolved input value and write the` |
|      - | 5359 | ` * result into pCtx. Shared by filter_var() and filter_input(): the caller has` |
|      - | 5360 | ` * already parsed $filter/$flags/$options. On validation failure the 'default'` |
|      - | 5361 | ` * option (if any) is returned, else null when FILTER_NULL_ON_FAILURE is set,` |
|      - | 5362 | ` * else false. A validating filter that passes returns the (string) input` |
|      - | 5363 | ` * unchanged; a sanitizer writes its transformed output directly.` |
|      - | 5364 | ` */` |
|    316 | 5365 | `static int FvApplyFilter(ph7_context *pCtx,ph7_value *pInput,` |
|      - | 5366 | `                         int iFilter,int iFlags,ph7_value *pOpts,` |
|      - | 5367 | `                         ph7_value *pDefault)` |
|      3 | 5368 | `{` |
|    319 | 5369 | `	int bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|      - | 5370 | `	const char *zVal; int nVal;` |
|      - | 5371 | `	/* An array/object input fails every scalar filter. */` |
|    319 | 5372 | `	if( ph7_value_is_array(pInput) ){ goto fail; }` |
|    317 | 5373 | `	zVal = ph7_value_to_string(pInput,&nVal);` |
|    317 | 5374 | `	switch( iFilter ){` |
|     28 | 5375 | `	case FV_VALIDATE_INT: {` |
|      - | 5376 | `		ph7_int64 v;` |
|     58 | 5377 | `		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }` |
|     31 | 5378 | `		if( pOpts ){` |
|      7 | 5379 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|      7 | 5380 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|      7 | 5381 | `			if( pMin && v<ph7_value_to_int64(pMin) ){ goto fail; }` |
|      7 | 5382 | `			if( pMax && v>ph7_value_to_int64(pMax) ){ goto fail; }` |
|      2 | 5383 | `		}` |
|     29 | 5384 | `		ph7_result_int64(pCtx,v);` |
|     29 | 5385 | `		return PH7_OK;` |
|      - | 5386 | `	}` |
|     34 | 5387 | `	case FV_VALIDATE_FLOAT: {` |
|      - | 5388 | `		double d;` |
|     69 | 5389 | `		if( !FvValidateFloat(zVal,nVal,iFlags,&d) ){ goto fail; }` |
|     39 | 5390 | `		ph7_result_double(pCtx,d);` |
|     39 | 5391 | `		return PH7_OK;` |
|      - | 5392 | `	}` |
|     14 | 5393 | `	case FV_VALIDATE_BOOLEAN: {` |
|      - | 5394 | `		int b;` |
|     29 | 5395 | `		if( !FvValidateBool(zVal,nVal,&b) ){ goto fail; }` |
|     21 | 5396 | `		ph7_result_bool(pCtx,b);` |
|     21 | 5397 | `		return PH7_OK;` |
|      - | 5398 | `	}` |
|     25 | 5399 | `	case FV_VALIDATE_IP:     if( !FvValidateIp(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|     11 | 5400 | `	case FV_VALIDATE_MAC:    if( !FvValidateMac(zVal,nVal) ){ goto fail; }       goto pass;` |
|     28 | 5401 | `	case FV_VALIDATE_EMAIL:  if( !FvValidateEmail(zVal,nVal) ){ goto fail; }     goto pass;` |
|     11 | 5402 | `	case FV_VALIDATE_DOMAIN: if( !FvValidateDomain(zVal,nVal) ){ goto fail; }    goto pass;` |
|     15 | 5403 | `	case FV_VALIDATE_URL:    if( !FvValidateUrl(zVal,nVal) ){ goto fail; }       goto pass;` |
|      3 | 5404 | `	case FV_VALIDATE_REGEXP: {` |
|      - | 5405 | `#ifdef PH7_ENABLE_PCRE` |
|      8 | 5406 | `		ph7_value *pRe = pOpts ? ph7_array_fetch(pOpts,"regexp",(int)sizeof("regexp")-1) : 0;` |
|      8 | 5407 | `		const char *zRe; int nRe, matched = 0;` |
|      8 | 5408 | `		if( pRe==0 ){` |
|      3 | 5409 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5410 | `				"filter_var(): \"regexp\" option is missing");` |
|      - | 5411 | `		}` |
|      5 | 5412 | `		zRe = ph7_value_to_string(pRe,&nRe);` |
|      5 | 5413 | `		if( PH7_PcreMatchQuiet(pCtx,zRe,nRe,zVal,nVal,&matched)!=SXRET_OK \|\| !matched ){ goto fail; }` |
|      3 | 5414 | `		goto pass;` |
|      - | 5415 | `#else` |
|      - | 5416 | `		goto fail;` |
|      - | 5417 | `#endif` |
|      - | 5418 | `	}` |
|      3 | 5419 | `	case FV_SANITIZE_NUMBER_INT:   FvSanitizeNumber(pCtx,zVal,nVal,0,0);      return PH7_OK;` |
|      5 | 5420 | `	case FV_SANITIZE_NUMBER_FLOAT: FvSanitizeNumber(pCtx,zVal,nVal,1,iFlags); return PH7_OK;` |
|     13 | 5421 | `	case FV_SANITIZE_SPECIAL_CHARS:      FvSanitizeSpecial(pCtx,zVal,nVal,iFlags); return PH7_OK;` |
|     25 | 5422 | `	case FV_SANITIZE_FULL_SPECIAL_CHARS: FvSanitizeFull(pCtx,zVal,nVal,iFlags);    return PH7_OK;` |
|      3 | 5423 | `	case FV_SANITIZE_EMAIL: FvSanitizeChars(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 5424 | `	case FV_SANITIZE_URL:   FvSanitizeChars(pCtx,zVal,nVal,1); return PH7_OK;` |
|     13 | 5425 | `	case FV_DEFAULT:` |
|      - | 5426 | `		/* FILTER_UNSAFE_RAW / FILTER_DEFAULT: pass through unchanged unless a` |
|      - | 5427 | `		 * STRIP/ENCODE flag is set, in which case apply the string filter. */` |
|     28 | 5428 | `		if( iFlags & FV_FLAG_STRING_MASK ){` |
|     15 | 5429 | `			FvSanitizeString(pCtx,zVal,nVal,iFlags);` |
|     15 | 5430 | `			return PH7_OK;` |
|      - | 5431 | `		}` |
|     14 | 5432 | `		goto pass;` |
|    ! 0 | 5433 | `	default:` |
|    ! 0 | 5434 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5435 | `			"Unknown filter with ID %d",iFilter);` |
|    ! 0 | 5436 | `		break; /* unknown filter id -> fail */` |
|    ! 0 | 5437 | `	}` |
|     58 | 5438 | `fail:` |
|    118 | 5439 | `	if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|    114 | 5440 | `	else if( bNull ){ ph7_result_null(pCtx); }` |
|    108 | 5441 | `	else { ph7_result_bool(pCtx,0); }` |
|    118 | 5442 | `	return PH7_OK;` |
|     26 | 5443 | `pass: /* validation passed: return the (string) input unchanged */` |
|     54 | 5444 | `	ph7_result_string(pCtx,zVal,nVal);` |
|     54 | 5445 | `	return PH7_OK;` |
|    161 | 5446 | `}` |
|      - | 5447 | `/*` |
|      - | 5448 | ` * Parse the ($filter, $options) pair shared by filter_var()/filter_input() out` |
|      - | 5449 | ` * of apArg[iBase] ($filter) and apArg[iBase+1] ($options): $options is either a` |
|      - | 5450 | ` * plain flags int, or an array with 'flags' and an 'options' sub-array (whose` |
|      - | 5451 | ` * 'default' entry is the fallback value). Fills the four output pointers;` |
|      - | 5452 | ` * unset outputs keep the caller-provided defaults.` |
|      - | 5453 | ` */` |
|    328 | 5454 | `static void FvParseFilterArgs(int nArg,ph7_value **apArg,int iBase,` |
|      - | 5455 | `                              int *piFilter,int *piFlags,` |
|      - | 5456 | `                              ph7_value **ppOpts,ph7_value **ppDefault)` |
|      3 | 5457 | `{` |
|    331 | 5458 | `	if( nArg>iBase ){ *piFilter = ph7_value_to_int(apArg[iBase]); }` |
|    331 | 5459 | `	if( nArg>iBase+1 ){` |
|     88 | 5460 | `		if( ph7_value_is_array(apArg[iBase+1]) ){` |
|     42 | 5461 | `			ph7_value *pF = ph7_array_fetch(apArg[iBase+1],"flags",(int)sizeof("flags")-1);` |
|     42 | 5462 | `			if( pF ){ *piFlags = ph7_value_to_int(pF); }` |
|     42 | 5463 | `			*ppOpts = ph7_array_fetch(apArg[iBase+1],"options",(int)sizeof("options")-1);` |
|     42 | 5464 | `			if( *ppOpts && !ph7_value_is_array(*ppOpts) ){ *ppOpts = 0; }` |
|     42 | 5465 | `			if( *ppOpts ){ *ppDefault = ph7_array_fetch(*ppOpts,"default",(int)sizeof("default")-1); }` |
|     22 | 5466 | `		}else{` |
|     48 | 5467 | `			*piFlags = ph7_value_to_int(apArg[iBase+1]);` |
|      - | 5468 | `		}` |
|     43 | 5469 | `	}` |
|    331 | 5470 | `}` |
|      - | 5471 | `/*` |
|      - | 5472 | ` * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 5473 | ` *  Validate or sanitize a value; see FvApplyFilter for the failure semantics.` |
|      - | 5474 | ` */` |
|    306 | 5475 | `static int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5476 | `{` |
|    308 | 5477 | `	int iFilter = FV_DEFAULT, iFlags = 0;` |
|    308 | 5478 | `	ph7_value *pOpts = 0, *pDefault = 0;` |
|    308 | 5479 | `	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    308 | 5480 | `	FvParseFilterArgs(nArg,apArg,1,&iFilter,&iFlags,&pOpts,&pDefault);` |
|    308 | 5481 | `	return FvApplyFilter(pCtx,apArg[0],iFilter,iFlags,pOpts,pDefault);` |
|    155 | 5482 | `}` |
|      - | 5483 | `/*` |
|      - | 5484 | ` * filter_input($type, $var_name, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 5485 | ` *  Look up $var_name in the requested INPUT_* superglobal, then apply the` |
|      - | 5486 | ` *  filter. Semantics verified byte-for-byte against php 8.5:` |
|      - | 5487 | ` *   - variable NOT set: 'default' option wins, else false when` |
|      - | 5488 | ` *     FILTER_NULL_ON_FAILURE is set, else null. (Note the null/false roles are` |
|      - | 5489 | ` *     INVERTED relative to a present value that fails validation, which yields` |
|      - | 5490 | ` *     default > null-if-NULL_ON_FAILURE > false via FvApplyFilter.)` |
|      - | 5491 | ` *   - variable present: delegate to FvApplyFilter.` |
|      - | 5492 | ` *  Divergence: php reads a SAPI snapshot of the original request variables` |
|      - | 5493 | ` *  captured at startup; PHL reads the live superglobal. In CLI they match for` |
|      - | 5494 | ` *  the SAPI-registered keys (SCRIPT_NAME/PHP_SELF/DOCUMENT_ROOT); keys added` |
|      - | 5495 | ` *  only to the live $_SERVER (REQUEST_TIME/PWD/…) are visible here but not in` |
|      - | 5496 | ` *  php's snapshot.` |
|      - | 5497 | ` */` |
|     28 | 5498 | `static int PH7_builtin_filter_input(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5499 | `{` |
|     30 | 5500 | `	int iType, iFilter = FV_DEFAULT, iFlags = 0;` |
|     30 | 5501 | `	ph7_value *pOpts = 0, *pDefault = 0, *pSuper, *pElem;` |
|      - | 5502 | `	const char *zVar, *zSuper; int nVar; sxu32 nSuper;` |
|     30 | 5503 | `	if( nArg<2 ){` |
|      7 | 5504 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|      2 | 5505 | `			"filter_input() expects at least 2 arguments, %d given",nArg);` |
|      - | 5506 | `	}` |
|     26 | 5507 | `	iType = ph7_value_to_int(apArg[0]);` |
|     26 | 5508 | `	switch( iType ){` |
|      3 | 5509 | `	case 0: zSuper = "_POST";   nSuper = (sxu32)sizeof("_POST")-1;   break; /* INPUT_POST */` |
|      3 | 5510 | `	case 1: zSuper = "_GET";    nSuper = (sxu32)sizeof("_GET")-1;    break; /* INPUT_GET */` |
|    ! 0 | 5511 | `	case 2: zSuper = "_COOKIE"; nSuper = (sxu32)sizeof("_COOKIE")-1; break; /* INPUT_COOKIE */` |
|    ! 0 | 5512 | `	case 4: zSuper = "_ENV";    nSuper = (sxu32)sizeof("_ENV")-1;    break; /* INPUT_ENV */` |
|     19 | 5513 | `	case 5: zSuper = "_SERVER"; nSuper = (sxu32)sizeof("_SERVER")-1; break; /* INPUT_SERVER */` |
|      1 | 5514 | `	default:` |
|      3 | 5515 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5516 | `			"filter_input(): Argument #1 ($type) must be an INPUT_* constant");` |
|      - | 5517 | `	}` |
|     23 | 5518 | `	zVar = ph7_value_to_string(apArg[1],&nVar);` |
|     23 | 5519 | `	FvParseFilterArgs(nArg,apArg,2,&iFilter,&iFlags,&pOpts,&pDefault);` |
|      - | 5520 | `	/* Resolve the variable from the superglobal (missing/non-array -> not set). */` |
|     23 | 5521 | `	pSuper = PH7_VmExtractSuper(pCtx->pVm,zSuper,nSuper);` |
|     23 | 5522 | `	pElem = (pSuper && ph7_value_is_array(pSuper))` |
|     33 | 5523 | `		? ph7_array_fetch(pSuper,zVar,nVar) : 0;` |
|     23 | 5524 | `	if( pElem==0 ){` |
|      - | 5525 | `		/* Variable not set: default > false(if NULL_ON_FAILURE) > null. Note the` |
|      - | 5526 | `		 * false/null roles are inverted vs FvApplyFilter's present-but-fails path. */` |
|     13 | 5527 | `		if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|      9 | 5528 | `		else if( iFlags & FV_NULL_ON_FAILURE ){ ph7_result_bool(pCtx,0); }` |
|      7 | 5529 | `		else { ph7_result_null(pCtx); }` |
|     13 | 5530 | `		return PH7_OK;` |
|      - | 5531 | `	}` |
|     11 | 5532 | `	return FvApplyFilter(pCtx,pElem,iFilter,iFlags,pOpts,pDefault);` |
|     16 | 5533 | `}` |
|      - | 5534 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 5535 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 5536 | `/*` |
|      - | 5537 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 5538 |  |
|      - | 5539 | ` */` |
|      4 | 5540 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 5541 | `	const char *zInput, /* Raw input */` |
|      - | 5542 | `	int nByte,  /* Input length */` |
|      - | 5543 | `	int delim,  /* Delimiter */` |
|      - | 5544 | `	int encl,   /* Enclosure */` |
|      - | 5545 | `	int escape,  /* Escape character */` |
|      - | 5546 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 5547 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 5548 | `	)` |
|      1 | 5549 | `{` |
|      5 | 5550 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 5551 | `	const char *zIn = zInput;` |
|      - | 5552 | `	const char *zPtr;` |
|      - | 5553 | `	int isEnc;` |
|      - | 5554 | `	/* Start processing */` |
|      8 | 5555 | `	for(;;){` |
|     17 | 5556 | `		if( zIn >= zEnd ){` |
|      - | 5557 | `			/* No more input to process */` |
|      5 | 5558 | `			break;` |
|      - | 5559 | `		}` |
|     13 | 5560 | `		isEnc = 0;` |
|     13 | 5561 | `		zPtr = zIn;` |
|      - | 5562 | `		/* Find the first delimiter */` |
|     27 | 5563 | `		while( zIn < zEnd ){` |
|     23 | 5564 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 5565 | `				/* Delimiter found,break imediately */` |
|      5 | 5566 | `				break;` |
|     15 | 5567 | `			}else if( zIn[0] == encl ){` |
|      - | 5568 | `				/* Inside enclosure? */` |
|    ! 0 | 5569 | `				isEnc = !isEnc;` |
|     15 | 5570 | `			}else if( zIn[0] == escape ){` |
|      - | 5571 | `				/* Escape sequence */` |
|    ! 0 | 5572 | `				zIn++;` |
|    ! 0 | 5573 | `			}` |
|      - | 5574 | `			/* Advance the cursor */` |
|     15 | 5575 | `			zIn++;` |
|      1 | 5576 | `		}` |
|     13 | 5577 | `		if( zIn > zPtr ){` |
|     13 | 5578 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 5579 | `			sxi32 rc;` |
|      - | 5580 | `			/* Invoke the supllied callback */` |
|     13 | 5581 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 5582 | `				zPtr++;` |
|    ! 0 | 5583 | `				nByteChunk-=2;` |
|    ! 0 | 5584 | `			}` |
|     13 | 5585 | `			if( nByteChunk > 0 ){` |
|     13 | 5586 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 5587 | `				if( rc == SXERR_ABORT ){` |
|      - | 5588 | `					/* User callback request an operation abort */` |
|    ! 0 | 5589 | `					break;` |
|      - | 5590 | `				}` |
|      6 | 5591 | `			}` |
|      6 | 5592 | `		}` |
|      - | 5593 | `		/* Ignore trailing delimiter */` |
|     21 | 5594 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 5595 | `			zIn++;` |
|      1 | 5596 | `		}` |
|      1 | 5597 | `	}` |
|      5 | 5598 | `	return SXRET_OK;` |
|      1 | 5599 | `}` |
|      - | 5600 | `/*` |
|      - | 5601 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 5602 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 5603 | ` * argument to this callback.` |
|      - | 5604 | ` */` |
|     12 | 5605 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 5606 | `{` |
|     13 | 5607 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 5608 | `	ph7_value sEntry;` |
|      - | 5609 | `	SyString sToken;` |
|      - | 5610 | `	/* Insert the token in the given array */` |
|     13 | 5611 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 5612 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 5613 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 5614 | `	if( sToken.nByte < 1){` |
|    ! 0 | 5615 | `		return SXRET_OK;` |
|      - | 5616 | `	}` |
|     13 | 5617 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 5618 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 5619 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 5620 | `	return SXRET_OK;` |
|      7 | 5621 | `}` |
|      - | 5622 | `/*` |
|      - | 5623 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 5624 | ` *  Parse a CSV string into an array.` |
|      - | 5625 | ` * Parameters` |
|      - | 5626 | ` *  $input` |
|      - | 5627 | ` *   The string to parse.` |
|      - | 5628 | ` *  $delimiter` |
|      - | 5629 | ` *   Set the field delimiter (one character only).` |
|      - | 5630 | ` *  $enclosure` |
|      - | 5631 | ` *   Set the field enclosure character (one character only).` |
|      - | 5632 | ` *  $escape` |
|      - | 5633 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 5634 | ` * Return` |
|      - | 5635 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 5636 | ` */` |
|      4 | 5637 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5638 | `{` |
|      - | 5639 | `	const char *zInput,*zPtr;` |
|      - | 5640 | `	ph7_value *pArray;` |
|      5 | 5641 | `	int delim  = ',';   /* Delimiter */` |
|      5 | 5642 | `	int encl   = '"' ;  /* Enclosure */` |
|      5 | 5643 | `	int escape = '\\';  /* Escape character */` |
|      - | 5644 | `	int nLen;` |
|      5 | 5645 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5646 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 5647 | `		ph7_result_null(pCtx);` |
|      3 | 5648 | `		return PH7_OK;` |
|      - | 5649 | `	}` |
|      - | 5650 | `	/* Extract the raw input */` |
|      3 | 5651 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5652 | `	if( nArg > 1 ){` |
|      - | 5653 | `		int i;` |
|      3 | 5654 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 5655 | `			/* Extract the delimiter */` |
|      3 | 5656 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 5657 | `			if( i > 0 ){` |
|      3 | 5658 | `				delim = zPtr[0];` |
|      1 | 5659 | `			}` |
|      1 | 5660 | `		}` |
|      3 | 5661 | `		if( nArg > 2 ){` |
|      3 | 5662 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 5663 | `				/* Extract the enclosure */` |
|      3 | 5664 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 5665 | `				if( i > 0 ){` |
|      3 | 5666 | `					encl = zPtr[0];` |
|      1 | 5667 | `				}` |
|      1 | 5668 | `			}` |
|      3 | 5669 | `			if( nArg > 3 ){` |
|      3 | 5670 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 5671 | `					/* Extract the escape character */` |
|      3 | 5672 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 5673 | `					if( i > 0 ){` |
|      3 | 5674 | `						escape = zPtr[0];` |
|      1 | 5675 | `					}` |
|      1 | 5676 | `				}` |
|      1 | 5677 | `			}` |
|      1 | 5678 | `		}` |
|      1 | 5679 | `	}` |
|      - | 5680 | `	/* Create our array */` |
|      3 | 5681 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5682 | `	if( pArray == 0 ){` |
|      - | 5683 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|    ! 0 | 5684 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5685 | `	}` |
|      - | 5686 | `	/* Parse the raw input */` |
|      3 | 5687 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 5688 | `	/* Return the freshly created array */` |
|      3 | 5689 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5690 | `	return PH7_OK;` |
|      3 | 5691 | `}` |
|      - | 5692 | `/*` |
|      - | 5693 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 5694 | ` * container.` |
|      - | 5695 | ` * Refer to [strip_tags()].` |
|      - | 5696 | ` */` |
|     10 | 5697 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5698 | `{` |
|     11 | 5699 | `	const char *zEnd = &zTag[nByte];` |
|      - | 5700 | `	const char *zPtr;` |
|      - | 5701 | `	SyString sEntry;` |
|      - | 5702 | `	/* Strip tags */` |
|     10 | 5703 | `	for(;;){` |
|     45 | 5704 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 5705 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 5706 | `				zTag++;` |
|      1 | 5707 | `		}` |
|     21 | 5708 | `		if( zTag >= zEnd ){` |
|     11 | 5709 | `			break;` |
|      - | 5710 | `		}` |
|     11 | 5711 | `		zPtr = zTag;` |
|      - | 5712 | `		/* Delimit the tag */` |
|     25 | 5713 | `		while(zTag < zEnd ){` |
|     25 | 5714 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5715 | `				/* UTF-8 stream */` |
|      3 | 5716 | `				zTag++;` |
|      5 | 5717 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 5718 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 5719 | `				break;` |
|    ! 0 | 5720 | `			}else{` |
|     13 | 5721 | `				zTag++;` |
|      - | 5722 | `			}` |
|      1 | 5723 | `		}` |
|     11 | 5724 | `		if( zTag > zPtr ){` |
|      - | 5725 | `			/* Perform the insertion */` |
|     11 | 5726 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 5727 | `			SyStringFullTrim(&sEntry);` |
|     11 | 5728 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 5729 | `		}` |
|      - | 5730 | `		/* Jump the trailing '>' */` |
|     11 | 5731 | `		zTag++;` |
|      1 | 5732 | `	}` |
|     11 | 5733 | `	return SXRET_OK;` |
|      1 | 5734 | `}` |
|      - | 5735 | `/*` |
|      - | 5736 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 5737 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 5738 | ` * Refer to [strip_tags()].` |
|      - | 5739 | ` */` |
|     36 | 5740 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5741 | `{` |
|     37 | 5742 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 5743 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 5744 | `		SyString sTag;` |
|     85 | 5745 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 5746 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 5747 | `			zTag++;` |
|      1 | 5748 | `		}` |
|      - | 5749 | `		/* Delimit the tag */` |
|     25 | 5750 | `		zCur = zTag;` |
|     77 | 5751 | `		while(zTag < zEnd ){` |
|     77 | 5752 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5753 | `				/* UTF-8 stream */` |
|      5 | 5754 | `				zTag++;` |
|      9 | 5755 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 5756 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 5757 | `				break;` |
|    ! 0 | 5758 | `			}else{` |
|     49 | 5759 | `				zTag++;` |
|      - | 5760 | `			}` |
|      1 | 5761 | `		}` |
|     25 | 5762 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 5763 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 5764 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 5765 | `		if( sTag.nByte > 0 ){` |
|      - | 5766 | `			SyString *aEntry,*pEntry;` |
|      - | 5767 | `			sxi32 rc;` |
|      - | 5768 | `			sxu32 n;` |
|      - | 5769 | `			/* Perform the lookup */` |
|     25 | 5770 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 5771 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 5772 | `				pEntry = &aEntry[n];` |
|      - | 5773 | `				/* Do the comparison */` |
|     25 | 5774 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 5775 | `				if( !rc ){` |
|     21 | 5776 | `					return SXRET_OK;` |
|      - | 5777 | `				}` |
|      3 | 5778 | `			}` |
|      2 | 5779 | `		}` |
|      2 | 5780 | `	}` |
|      - | 5781 | `	/* No such tag */` |
|     17 | 5782 | `	return SXERR_NOTFOUND;` |
|     19 | 5783 | `}` |
|      - | 5784 | `/*` |
|      - | 5785 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 5786 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 5787 | ` * Refer to [strip_tags()].` |
|      - | 5788 | ` */` |
|     16 | 5789 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 5790 | `{` |
|     17 | 5791 | `	const char *zEnd = &zIn[nByte];` |
|      - | 5792 | `	const char *zPtr,*zTag;` |
|      - | 5793 | `	SySet sSet;` |
|      - | 5794 | `	/* initialize the set of allowed tags */` |
|     17 | 5795 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 5796 | `	if( nTaglen > 0 ){` |
|      - | 5797 | `		/* Set of allowed tags */` |
|     11 | 5798 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 5799 | `	}` |
|      - | 5800 | `	/* Set the empty string */` |
|     17 | 5801 | `	ph7_result_string(pCtx,"",0);` |
|      - | 5802 | `	/* Start processing */` |
|     26 | 5803 | `	for(;;){` |
|     53 | 5804 | `		if(zIn >= zEnd){` |
|      - | 5805 | `			/* No more input to process */` |
|     15 | 5806 | `			break;` |
|      - | 5807 | `		}` |
|     39 | 5808 | `		zPtr = zIn;` |
|      - | 5809 | `		/* Find a tag */` |
|    133 | 5810 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 5811 | `			zIn++;` |
|      1 | 5812 | `		}` |
|     39 | 5813 | `		if( zIn > zPtr ){` |
|      - | 5814 | `			/* Consume raw input */` |
|     21 | 5815 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 5816 | `		}` |
|      - | 5817 | `		/* Ignore trailing null bytes */` |
|     39 | 5818 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 5819 | `			zIn++;` |
|    ! 0 | 5820 | `		}` |
|     39 | 5821 | `		if(zIn >= zEnd){` |
|      - | 5822 | `			/* No more input to process */` |
|      3 | 5823 | `			break;` |
|      - | 5824 | `		}` |
|     37 | 5825 | `		if( zIn[0] == '<' ){` |
|      - | 5826 | `			sxi32 rc;` |
|     37 | 5827 | `			zTag = zIn++;` |
|      - | 5828 | `			/* Delimit the tag */` |
|    127 | 5829 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 5830 | `				zIn++;` |
|      1 | 5831 | `			}` |
|     37 | 5832 | `			if( zIn < zEnd ){` |
|     37 | 5833 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 5834 | `			}` |
|      - | 5835 | `			/* Query the set */` |
|     37 | 5836 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 5837 | `			if( rc == SXRET_OK ){` |
|      - | 5838 | `				/* Keep the tag */` |
|     21 | 5839 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 5840 | `			}` |
|     18 | 5841 | `		}` |
|      1 | 5842 | `	}` |
|      - | 5843 | `	/* Cleanup */` |
|     17 | 5844 | `	SySetRelease(&sSet);` |
|     17 | 5845 | `	return SXRET_OK;` |
|      1 | 5846 | `}` |
|      - | 5847 | `/*` |
|      - | 5848 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 5849 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 5850 | ` * Parameters` |
|      - | 5851 | ` *  $str` |
|      - | 5852 | ` *  The input string.` |
|      - | 5853 | ` * $allowable_tags` |
|      - | 5854 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 5855 | ` * Return` |
|      - | 5856 | ` *  Returns the stripped string.` |
|      - | 5857 | ` */` |
|     16 | 5858 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5859 | `{` |
|     17 | 5860 | `	const char *zTaglist = 0;` |
|      - | 5861 | `	const char *zString;` |
|     17 | 5862 | `	int nTaglen = 0;` |
|      - | 5863 | `	int nLen;` |
|     17 | 5864 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5865 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 5866 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5867 | `		return PH7_OK;` |
|      - | 5868 | `	}` |
|      - | 5869 | `	/* Point to the raw string */` |
|     15 | 5870 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 5871 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 5872 | `		/* Allowed tag */` |
|     11 | 5873 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 5874 | `	}` |
|      - | 5875 | `	/* Process input */` |
|     15 | 5876 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 5877 | `	return PH7_OK;` |
|      9 | 5878 | `}` |
|      - | 5879 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5880 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5881 | `/*` |
|      - | 5882 | ` * string str_shuffle(string $str)` |
|      - | 5883 |  |
|      - | 5884 | ` *  Randomly shuffles a string.` |
|      - | 5885 | ` * Parameters` |
|      - | 5886 | ` *  $str` |
|      - | 5887 | ` *   The input string.` |
|      - | 5888 | ` * Return` |
|      - | 5889 | ` *  Returns the shuffled string.` |
|      - | 5890 | ` */` |
|     12 | 5891 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5892 | `{` |
|      - | 5893 | `	const char *zString;` |
|      - | 5894 | `	int nLen,i,c;` |
|      - | 5895 | `	sxu32 iR;` |
|     13 | 5896 | `	if( nArg < 1 ){` |
|      - | 5897 | `		/* Missing arguments,return the empty string */` |
|      3 | 5898 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5899 | `		return PH7_OK;` |
|      - | 5900 | `	}` |
|      - | 5901 | `	/* Extract the target string */` |
|     11 | 5902 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5903 | `	if( nLen < 1 ){` |
|      - | 5904 | `		/* Nothing to shuffle */` |
|      3 | 5905 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5906 | `		return PH7_OK;` |
|      - | 5907 | `	}` |
|      - | 5908 | `	/* Shuffle the string */` |
|     43 | 5909 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 5910 | `		/* Generate a random number first */` |
|     35 | 5911 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 5912 | `		/* Extract a random offset */` |
|     35 | 5913 | `		c = zString[iR % nLen];` |
|      - | 5914 | `		/* Append it */` |
|     35 | 5915 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 5916 | `	}` |
|      9 | 5917 | `	return PH7_OK;` |
|      7 | 5918 | `}` |
|      - | 5919 | `/*` |
|      - | 5920 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 5921 | ` *  Convert a string to an array.` |
|      - | 5922 | ` * Parameters` |
|      - | 5923 | ` * $string` |
|      - | 5924 | ` *  The input string.` |
|      - | 5925 | ` * $split_length` |
|      - | 5926 | ` *  Maximum length of the chunk.` |
|      - | 5927 | ` * Return` |
|      - | 5928 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 5929 | ` *  except possibly the last one which may be shorter.` |
|      - | 5930 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 5931 | ` *  as the first (and only) array element.` |
|      - | 5932 | ` *  An empty string returns an empty array.` |
|      - | 5933 | ` * Errors` |
|      - | 5934 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 5935 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 5936 | ` *  ValueError if $split_length is less than 1.` |
|      - | 5937 | ` */` |
|     28 | 5938 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 5939 | `{` |
|      - | 5940 | `	const char *zString,*zEnd;` |
|      - | 5941 | `	ph7_value *pArray,*pValue;` |
|      - | 5942 | `	int split_len;` |
|      - | 5943 | `	int nLen;` |
|     33 | 5944 | `	if( nArg < 1 ){` |
|      4 | 5945 | `		return PH7_VmThrowException(pCtx,` |
|      - | 5946 | `			"ArgumentCountError",` |
|      - | 5947 | `			"str_split() expects at least 1 argument, %d given",` |
|      1 | 5948 | `			nArg` |
|      - | 5949 | `			);` |
|      - | 5950 | `	}` |
|      - | 5951 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     38 | 5952 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     40 | 5953 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 5954 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 5955 | `		return PH7_VmThrowException(pCtx,` |
|      - | 5956 | `			"TypeError",` |
|      - | 5957 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 5958 | `			ph7_type_name(apArg[0])` |
|      - | 5959 | `			);` |
|      - | 5960 | `	}` |
|      - | 5961 | `	/* Point to the target string */` |
|     27 | 5962 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 5963 | `	split_len = (int)sizeof(char);` |
|     27 | 5964 | `	if( nArg > 1 ){` |
|      - | 5965 | `		/* Split length */` |
|     17 | 5966 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 5967 | `		if( split_len < 1 ){` |
|      6 | 5968 | `			return PH7_VmThrowException(pCtx,` |
|      - | 5969 | `				"ValueError",` |
|      - | 5970 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 5971 | `				);` |
|      - | 5972 | `		}` |
|     11 | 5973 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 5974 | `			split_len = nLen;` |
|      1 | 5975 | `		}` |
|      5 | 5976 | `	}` |
|      - | 5977 | `	/* Create the array and the scalar value */` |
|     21 | 5978 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 5979 | `	/*Chunk value */` |
|     21 | 5980 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 5981 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 5982 | `		/* Return FALSE */` |
|    ! 0 | 5983 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5984 | `		return PH7_OK;` |
|      - | 5985 | `	}` |
|      - | 5986 | `	/* Point to the end of the string */` |
|     21 | 5987 | `	zEnd = &zString[nLen];` |
|      - | 5988 | `	/* Perform the requested operation */` |
|     48 | 5989 | `	for(;;){` |
|      - | 5990 | `		int nMax;` |
|     59 | 5991 | `		if( zString >= zEnd ){` |
|      - | 5992 | `			/* No more input to process */` |
|     21 | 5993 | `			break;` |
|      - | 5994 | `		}` |
|     39 | 5995 | `		nMax = (int)(zEnd-zString);` |
|     39 | 5996 | `		if( nMax < split_len ){` |
|      3 | 5997 | `			split_len = nMax;` |
|      1 | 5998 | `		}` |
|      - | 5999 | `		/* Copy the current chunk */` |
|     39 | 6000 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 6001 | `		/* Insert it */` |
|     39 | 6002 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|    ! 0 | 6003 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6004 | `		}` |
|      - | 6005 | `		/* reset the string cursor */` |
|     39 | 6006 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 6007 | `		/* Update position */` |
|     39 | 6008 | `		zString += split_len;` |
|      1 | 6009 | `	}` |
|      - | 6010 | `	/*` |
|      - | 6011 | `	 * Return the array.` |
|      - | 6012 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 6013 | `	 * upon we return from this function.` |
|      - | 6014 | `	 */` |
|     21 | 6015 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 6016 | `	return PH7_OK;` |
|     19 | 6017 | `}` |
|      - | 6018 | `/*` |
|      - | 6019 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 6020 | ` * Refer to [strspn()].` |
|      - | 6021 | ` */` |
|     28 | 6022 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 6023 | `{` |
|     29 | 6024 | `	const char *zIn = *pzIn;` |
|      - | 6025 | `	const char *zPtr;` |
|      - | 6026 | `	/* Ignore leading white spaces */` |
|     29 | 6027 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 6028 | `		zIn++;` |
|    ! 0 | 6029 | `	}` |
|     29 | 6030 | `	if( zIn >= zEnd ){` |
|      - | 6031 | `		/* End of input */` |
|    ! 0 | 6032 | `		return SXERR_EOF;` |
|      - | 6033 | `	}` |
|     29 | 6034 | `	zPtr = zIn;` |
|      - | 6035 | `	/* Extract the token */` |
|    201 | 6036 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 6037 | `		zIn++;` |
|      1 | 6038 | `	}` |
|     29 | 6039 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 6040 | `	/* Synchronize pointers */` |
|     29 | 6041 | `	*pzIn = zIn;` |
|      - | 6042 | `	/* Return to the caller */` |
|     29 | 6043 | `	return SXRET_OK;` |
|     15 | 6044 | `}` |
|      - | 6045 | `/*` |
|      - | 6046 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 6047 | ` * return the longest match.` |
|      - | 6048 | ` * Refer to [strspn()].` |
|      - | 6049 | ` */` |
|     18 | 6050 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 6051 | `{` |
|     19 | 6052 | `	const char *zEnd = &zString[nLen];` |
|     19 | 6053 | `	const char *zIn = zString;` |
|      - | 6054 | `	int i,c;` |
|     45 | 6055 | `	for(;;){` |
|     91 | 6056 | `		if( zString >= zEnd ){` |
|      7 | 6057 | `			break;` |
|      - | 6058 | `		}` |
|      - | 6059 | `		/* Extract current character */` |
|     85 | 6060 | `		c = zString[0];` |
|      - | 6061 | `		/* Perform the lookup */` |
|    383 | 6062 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 6063 | `			if( c == zMask[i] ){` |
|      - | 6064 | `				/* Character found */` |
|     73 | 6065 | `				break;` |
|      - | 6066 | `			}` |
|    150 | 6067 | `		}` |
|     85 | 6068 | `		if( i >= nMaskLen ){` |
|      - | 6069 | `			/* Character not in the current mask,break immediately */` |
|     13 | 6070 | `			break;` |
|      - | 6071 | `		}` |
|      - | 6072 | `		/* Advance cursor */` |
|     73 | 6073 | `		zString++;` |
|      1 | 6074 | `	}` |
|      - | 6075 | `	/* Longest match */` |
|     19 | 6076 | `	return (int)(zString-zIn);` |
|      1 | 6077 | `}` |
|      - | 6078 | `/*` |
|      - | 6079 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 6080 | ` * Refer to [strcspn()].` |
|      - | 6081 | ` */` |
|     10 | 6082 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 6083 | `{` |
|     11 | 6084 | `	const char *zEnd = &zString[nLen];` |
|     11 | 6085 | `	const char *zIn = zString;` |
|      - | 6086 | `	int i,c;` |
|     12 | 6087 | `	for(;;){` |
|     25 | 6088 | `		if( zString >= zEnd ){` |
|      3 | 6089 | `			break;` |
|      - | 6090 | `		}` |
|      - | 6091 | `		/* Extract current character */` |
|     23 | 6092 | `		c = zString[0];` |
|      - | 6093 | `		/* Perform the lookup */` |
|     51 | 6094 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 6095 | `			if( c == zMask[i] ){` |
|      9 | 6096 | `				break;` |
|      - | 6097 | `			}` |
|     15 | 6098 | `		}` |
|     23 | 6099 | `		if( i < nMaskLen ){` |
|      - | 6100 | `			/* Character in the current mask,break immediately */` |
|      9 | 6101 | `			break;` |
|      - | 6102 | `		}` |
|      - | 6103 | `		/* Advance cursor */` |
|     15 | 6104 | `		zString++;` |
|      1 | 6105 | `	}` |
|      - | 6106 | `	/* Longest match */` |
|     11 | 6107 | `	return (int)(zString-zIn);` |
|      1 | 6108 | `}` |
|      - | 6109 | `/*` |
|      - | 6110 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 6111 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 6112 | ` *  of characters contained within a given mask.` |
|      - | 6113 | ` * Parameters` |
|      - | 6114 | ` * $str` |
|      - | 6115 | ` *  The input string.` |
|      - | 6116 | ` * $mask` |
|      - | 6117 | ` *  The list of allowable characters.` |
|      - | 6118 | ` * $start` |
|      - | 6119 | ` *  The position in subject to start searching.` |
|      - | 6120 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 6121 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 6122 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 6123 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 6124 | ` *  start'th position from the end of subject.` |
|      - | 6125 | ` * $length` |
|      - | 6126 | ` *  The length of the segment from subject to examine.` |
|      - | 6127 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 6128 | ` *  characters after the starting position.` |
|      - | 6129 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 6130 | ` *  position up to length characters from the end of subject.` |
|      - | 6131 | ` * Return` |
|      - | 6132 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 6133 | ` * in mask.` |
|      - | 6134 | ` */` |
|     26 | 6135 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6136 | `{` |
|      - | 6137 | `	const char *zString,*zMask,*zEnd;` |
|      - | 6138 | `	int iMasklen,iLen;` |
|      - | 6139 | `	SyString sToken;` |
|     27 | 6140 | `	int iCount = 0;` |
|      - | 6141 | `	int rc;` |
|     27 | 6142 | `	if( nArg < 2 ){` |
|      - | 6143 | `		/* Missing agruments,return zero */` |
|      3 | 6144 | `		ph7_result_int(pCtx,0);` |
|      3 | 6145 | `		return PH7_OK;` |
|      - | 6146 | `	}` |
|      - | 6147 | `	/* Extract the target string */` |
|     25 | 6148 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6149 | `	/* Extract the mask */` |
|     25 | 6150 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 6151 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 6152 | `		/* Nothing to process,return zero */` |
|      7 | 6153 | `		ph7_result_int(pCtx,0);` |
|      7 | 6154 | `		return PH7_OK;` |
|      - | 6155 | `	}` |
|     19 | 6156 | `	if( nArg > 2 ){` |
|      - | 6157 | `		int nOfft;` |
|      - | 6158 | `		/* Extract the offset */` |
|      9 | 6159 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 6160 | `		if( nOfft < 0 ){` |
|    ! 0 | 6161 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 6162 | `			if( zBase > zString ){` |
|    ! 0 | 6163 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 6164 | `				zString = zBase;` |
|    ! 0 | 6165 | `			}else{` |
|      - | 6166 | `				/* Invalid offset */` |
|    ! 0 | 6167 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 6168 | `				return PH7_OK;` |
|      - | 6169 | `			}` |
|    ! 0 | 6170 | `		}else{` |
|      9 | 6171 | `			if( nOfft >= iLen ){` |
|      - | 6172 | `				/* Invalid offset */` |
|    ! 0 | 6173 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 6174 | `				return PH7_OK;` |
|    ! 0 | 6175 | `			}else{` |
|      - | 6176 | `				/* Update offset */` |
|      9 | 6177 | `				zString += nOfft;` |
|      9 | 6178 | `				iLen -= nOfft;` |
|      - | 6179 | `			}` |
|      - | 6180 | `		}` |
|      9 | 6181 | `		if( nArg > 3 ){` |
|      - | 6182 | `			int iUserlen;` |
|      - | 6183 | `			/* Extract the desired length */` |
|      9 | 6184 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 6185 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 6186 | `				iLen = iUserlen;` |
|      2 | 6187 | `			}` |
|      4 | 6188 | `		}` |
|      4 | 6189 | `	}` |
|      - | 6190 | `	/* Point to the end of the string */` |
|     19 | 6191 | `	zEnd = &zString[iLen];` |
|      - | 6192 | `	/* Extract the first non-space token */` |
|     19 | 6193 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 6194 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 6195 | `		/* Compare against the current mask */` |
|     19 | 6196 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 6197 | `	}` |
|      - | 6198 | `	/* Longest match */` |
|     19 | 6199 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 6200 | `	return PH7_OK;` |
|     14 | 6201 | `}` |
|      - | 6202 | `/*` |
|      - | 6203 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 6204 | ` *  Find length of initial segment not matching mask.` |
|      - | 6205 | ` * Parameters` |
|      - | 6206 | ` * $str` |
|      - | 6207 | ` *  The input string.` |
|      - | 6208 | ` * $mask` |
|      - | 6209 | ` *  The list of not allowed characters.` |
|      - | 6210 | ` * $start` |
|      - | 6211 | ` *  The position in subject to start searching.` |
|      - | 6212 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 6213 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 6214 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 6215 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 6216 | ` *  start'th position from the end of subject.` |
|      - | 6217 | ` * $length` |
|      - | 6218 | ` *  The length of the segment from subject to examine.` |
|      - | 6219 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 6220 | ` *  characters after the starting position.` |
|      - | 6221 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 6222 | ` *  position up to length characters from the end of subject.` |
|      - | 6223 | ` * Return` |
|      - | 6224 | ` *  Returns the length of the segment as an integer.` |
|      - | 6225 | ` */` |
|     16 | 6226 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6227 | `{` |
|      - | 6228 | `	const char *zString,*zMask,*zEnd;` |
|      - | 6229 | `	int iMasklen,iLen;` |
|      - | 6230 | `	SyString sToken;` |
|     17 | 6231 | `	int iCount = 0;` |
|      - | 6232 | `	int rc;` |
|     17 | 6233 | `	if( nArg < 2 ){` |
|      - | 6234 | `		/* Missing agruments,return zero */` |
|      3 | 6235 | `		ph7_result_int(pCtx,0);` |
|      3 | 6236 | `		return PH7_OK;` |
|      - | 6237 | `	}` |
|      - | 6238 | `	/* Extract the target string */` |
|     15 | 6239 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6240 | `	/* Extract the mask */` |
|     15 | 6241 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 6242 | `	if( iLen < 1 ){` |
|      - | 6243 | `		/* Nothing to process,return zero */` |
|    ! 0 | 6244 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 6245 | `		return PH7_OK;` |
|      - | 6246 | `	}` |
|     15 | 6247 | `	if( iMasklen < 1 ){` |
|      - | 6248 | `		/* No given mask,return the string length */` |
|      3 | 6249 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 6250 | `		return PH7_OK;` |
|      - | 6251 | `	}` |
|     13 | 6252 | `	if( nArg > 2 ){` |
|      - | 6253 | `		int nOfft;` |
|      - | 6254 | `		/* Extract the offset */` |
|     11 | 6255 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 6256 | `		if( nOfft < 0 ){` |
|    ! 0 | 6257 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 6258 | `			if( zBase > zString ){` |
|    ! 0 | 6259 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 6260 | `				zString = zBase;` |
|    ! 0 | 6261 | `			}else{` |
|      - | 6262 | `				/* Invalid offset */` |
|    ! 0 | 6263 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 6264 | `				return PH7_OK;` |
|      - | 6265 | `			}` |
|    ! 0 | 6266 | `		}else{` |
|     11 | 6267 | `			if( nOfft >= iLen ){` |
|      - | 6268 | `				/* Invalid offset */` |
|      3 | 6269 | `				ph7_result_int(pCtx,0);` |
|      3 | 6270 | `				return PH7_OK;` |
|    ! 0 | 6271 | `			}else{` |
|      - | 6272 | `				/* Update offset */` |
|      9 | 6273 | `				zString += nOfft;` |
|      9 | 6274 | `				iLen -= nOfft;` |
|      - | 6275 | `			}` |
|      - | 6276 | `		}` |
|      9 | 6277 | `		if( nArg > 3 ){` |
|      - | 6278 | `			int iUserlen;` |
|      - | 6279 | `			/* Extract the desired length */` |
|    ! 0 | 6280 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 6281 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 6282 | `				iLen = iUserlen;` |
|    ! 0 | 6283 | `			}` |
|    ! 0 | 6284 | `		}` |
|      4 | 6285 | `	}` |
|      - | 6286 | `	/* Point to the end of the string */` |
|     11 | 6287 | `	zEnd = &zString[iLen];` |
|      - | 6288 | `	/* Extract the first non-space token */` |
|     11 | 6289 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 6290 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 6291 | `		/* Compare against the current mask */` |
|     11 | 6292 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 6293 | `	}` |
|      - | 6294 | `	/* Longest match */` |
|     11 | 6295 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 6296 | `	return PH7_OK;` |
|      9 | 6297 | `}` |
|      - | 6298 | `/*` |
|      - | 6299 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 6300 | ` *  Search a string for any of a set of characters.` |
|      - | 6301 | ` * Parameters` |
|      - | 6302 | ` *  $haystack` |
|      - | 6303 | ` *   The string where char_list is looked for.` |
|      - | 6304 | ` *  $char_list` |
|      - | 6305 | ` *   This parameter is case sensitive.` |
|      - | 6306 | ` * Return` |
|      - | 6307 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 6308 | ` */` |
|      6 | 6309 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6310 | `{` |
|      - | 6311 | `	const char *zString,*zList,*zEnd;` |
|      - | 6312 | `	int iLen,iListLen,i,c;` |
|      - | 6313 | `	sxu32 nOfft,nMax;` |
|      - | 6314 | `	sxi32 rc;` |
|      7 | 6315 | `	if( nArg < 2 ){` |
|      - | 6316 | `		/* Missing arguments,return FALSE */` |
|      3 | 6317 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6318 | `		return PH7_OK;` |
|      - | 6319 | `	}` |
|      - | 6320 | `	/* Extract the haystack and the char list */` |
|      5 | 6321 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 6322 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 6323 | `	if( iLen < 1 ){` |
|      - | 6324 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 6325 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6326 | `		return PH7_OK;` |
|      - | 6327 | `	}` |
|      - | 6328 | `	/* Point to the end of the string */` |
|      5 | 6329 | `	zEnd = &zString[iLen];` |
|      5 | 6330 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 6331 | `	/* perform the requested operation */` |
|     15 | 6332 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 6333 | `		c = zList[i];` |
|     11 | 6334 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 6335 | `		if( rc == SXRET_OK ){` |
|      5 | 6336 | `			if( nMax < nOfft ){` |
|      3 | 6337 | `				nOfft = nMax;` |
|      1 | 6338 | `			}` |
|      2 | 6339 | `		}` |
|      6 | 6340 | `	}` |
|      5 | 6341 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 6342 | `		/* No such substring,return FALSE */` |
|      3 | 6343 | `		ph7_result_bool(pCtx,0);` |
|      2 | 6344 | `	}else{` |
|      - | 6345 | `		/* Return the substring */` |
|      3 | 6346 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 6347 | `	}` |
|      5 | 6348 | `	return PH7_OK;` |
|      4 | 6349 | `}` |
|      - | 6350 | `/* SPDX-SnippetBegin */` |
|      - | 6351 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 6352 | `/* SPDX-License-Identifier: blessing */` |
|      - | 6353 | `/*` |
|      - | 6354 | ` * string soundex(string $str)` |
|      - | 6355 | ` *  Calculate the soundex key of a string.` |
|      - | 6356 | ` * Parameters` |
|      - | 6357 | ` *  $str` |
|      - | 6358 | ` *   The input string.` |
|      - | 6359 | ` * Return` |
|      - | 6360 | ` *  Returns the soundex key as a string.` |
|      - | 6361 | ` * Note:` |
|      - | 6362 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 6363 | ` * source tree.` |
|      - | 6364 | ` */` |
|     20 | 6365 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6366 | `{` |
|      - | 6367 | `	const unsigned char *zIn;` |
|      - | 6368 | `	char zResult[8];` |
|      - | 6369 | `	int i, j;` |
|      - | 6370 | `	static const unsigned char iCode[] = {` |
|      - | 6371 |  |
|      - | 6372 |  |
|      - | 6373 |  |
|      - | 6374 |  |
|      - | 6375 |  |
|      - | 6376 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 6377 |  |
|      - | 6378 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 6379 | `	};` |
|     21 | 6380 | `	if( nArg < 1 ){` |
|      - | 6381 | `		/* Missing arguments,return the empty string */` |
|      3 | 6382 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 6383 | `		return PH7_OK;` |
|      - | 6384 | `	}` |
|     19 | 6385 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     19 | 6386 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     19 | 6387 | `	if( zIn[i] ){` |
|     17 | 6388 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 6389 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 6390 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 6391 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 6392 | `			if( code>0 ){` |
|     45 | 6393 | `				if( code!=prevcode ){` |
|     33 | 6394 | `					prevcode = (unsigned char)code;` |
|     33 | 6395 | `					zResult[j++] = (char)code + '0';` |
|     16 | 6396 | `				}` |
|     23 | 6397 | `			}else{` |
|     49 | 6398 | `				prevcode = 0;` |
|      - | 6399 | `			}` |
|     47 | 6400 | `		}` |
|     33 | 6401 | `		while( j<4 ){` |
|     17 | 6402 | `			zResult[j++] = '0';` |
|      1 | 6403 | `		}` |
|     17 | 6404 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 6405 | `	}else{` |
|      3 | 6406 | `	  ph7_result_string(pCtx,"?000",4);` |
|      - | 6407 | `	}` |
|     19 | 6408 | `	return PH7_OK;` |
|     11 | 6409 | `}` |
|      - | 6410 | `/* SPDX-SnippetEnd */` |
|      - | 6411 | `/*` |
|      - | 6412 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 6413 | ` *  Wraps a string to a given number of characters.` |
|      - | 6414 | ` * Parameters` |
|      - | 6415 | ` *  $str` |
|      - | 6416 | ` *   The input string.` |
|      - | 6417 | ` * $width` |
|      - | 6418 | ` *  The column width.` |
|      - | 6419 | ` * $break` |
|      - | 6420 | ` *  The line is broken using the optional break parameter.` |
|      - | 6421 | ` * Return` |
|      - | 6422 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 6423 | ` */` |
|     14 | 6424 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6425 | `{` |
|      - | 6426 | `	const char *zIn,*zEnd,*zBreak;` |
|      - | 6427 | `	int iLen,iBreaklen,iChunk;` |
|     15 | 6428 | `	if( nArg < 1 ){` |
|      - | 6429 | `		/* Missing arguments,return the empty string */` |
|      3 | 6430 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 6431 | `		return PH7_OK;` |
|      - | 6432 | `	}` |
|      - | 6433 | `	/* Extract the input string */` |
|     13 | 6434 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|     13 | 6435 | `	if( iLen < 1 ){` |
|      - | 6436 | `		/* Nothing to process,return the empty string */` |
|      3 | 6437 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 6438 | `		return PH7_OK;` |
|      - | 6439 | `	}` |
|      - | 6440 | `	/* Chunk length */` |
|     11 | 6441 | `	iChunk = 75;` |
|     11 | 6442 | `	iBreaklen = 0;` |
|     11 | 6443 | `	zBreak = ""; /* cc warning */` |
|     11 | 6444 | `	if( nArg > 1 ){` |
|     11 | 6445 | `		iChunk = ph7_value_to_int(apArg[1]);` |
|     11 | 6446 | `		if( iChunk < 1 ){` |
|    ! 0 | 6447 | `			iChunk = 75;` |
|    ! 0 | 6448 | `		}` |
|     11 | 6449 | `		if( nArg > 2 ){` |
|      3 | 6450 | `			zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      1 | 6451 | `		}` |
|      5 | 6452 | `	}` |
|     11 | 6453 | `	if( iBreaklen < 1 ){` |
|      - | 6454 | `		/* Set a default column break */` |
|      - | 6455 | `#ifdef __WINNT__` |
|      1 | 6456 | `		zBreak = "\r\n";` |
|      1 | 6457 | `		iBreaklen = (int)sizeof("\r\n")-1;` |
|      - | 6458 | `#else` |
|      8 | 6459 | `		zBreak = "\n";` |
|      8 | 6460 | `		iBreaklen = (int)sizeof(char);` |
|      - | 6461 | `#endif` |
|      4 | 6462 | `	}` |
|      - | 6463 | `	/* Perform the requested operation */` |
|     11 | 6464 | `	zEnd = &zIn[iLen];` |
|     41 | 6465 | `	for(;;){` |
|      - | 6466 | `		int nMax;` |
|     47 | 6467 | `		if( zIn >= zEnd ){` |
|      - | 6468 | `			/* No more input to process */` |
|     11 | 6469 | `			break;` |
|      - | 6470 | `		}` |
|     37 | 6471 | `		nMax = (int)(zEnd-zIn);` |
|     37 | 6472 | `		if( iChunk > nMax ){` |
|     11 | 6473 | `			iChunk = nMax;` |
|      5 | 6474 | `		}` |
|      - | 6475 | `		/* Append the column first */` |
|     37 | 6476 | `		ph7_result_string(pCtx,zIn,iChunk); /* Will make it's own copy */` |
|      - | 6477 | `		/* Advance the cursor */` |
|     37 | 6478 | `		zIn += iChunk;` |
|     37 | 6479 | `		if( zIn < zEnd ){` |
|      - | 6480 | `			/* Append the line break */` |
|     27 | 6481 | `			ph7_result_string(pCtx,zBreak,iBreaklen);` |
|     13 | 6482 | `		}` |
|      1 | 6483 | `	}` |
|     11 | 6484 | `	return PH7_OK;` |
|      8 | 6485 | `}` |
|      - | 6486 | `/*` |
|      - | 6487 | ` * Check if the given character is a member of the given mask.` |
|      - | 6488 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 6489 | ` * Refer to [strtok()].` |
|      - | 6490 | ` */` |
|     30 | 6491 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 6492 | `{` |
|      - | 6493 | `	int i;` |
|     57 | 6494 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 6495 | `		if( c == zMask[i] ){` |
|     13 | 6496 | `			if( pOfft ){` |
|      5 | 6497 | `				*pOfft = i;` |
|      2 | 6498 | `			}` |
|     13 | 6499 | `			return TRUE;` |
|      - | 6500 | `		}` |
|     14 | 6501 | `	}` |
|     19 | 6502 | `	return FALSE;` |
|     16 | 6503 | `}` |
|      - | 6504 | `/*` |
|      - | 6505 | ` * Extract a single token from the input stream.` |
|      - | 6506 | ` * Refer to [strtok()].` |
|      - | 6507 | ` */` |
|      6 | 6508 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 6509 | `{` |
|      7 | 6510 | `	const char *zIn = *pzIn;` |
|      - | 6511 | `	const char *zPtr;` |
|      - | 6512 | `	/* Ignore leading delimiter */` |
|     11 | 6513 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 6514 | `		zIn++;` |
|      1 | 6515 | `	}` |
|      7 | 6516 | `	if( zIn >= zEnd ){` |
|      - | 6517 | `		/* End of input */` |
|    ! 0 | 6518 | `		return SXERR_EOF;` |
|      - | 6519 | `	}` |
|      7 | 6520 | `	zPtr = zIn;` |
|      - | 6521 | `	/* Extract the token */` |
|     13 | 6522 | `	while( zIn < zEnd ){` |
|     11 | 6523 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 6524 | `			/* UTF-8 stream */` |
|    ! 0 | 6525 | `			zIn++;` |
|    ! 0 | 6526 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 6527 | `		}else{` |
|     11 | 6528 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 6529 | `				break;` |
|      - | 6530 | `			}` |
|      7 | 6531 | `			zIn++;` |
|      - | 6532 | `		}` |
|      1 | 6533 | `	}` |
|      7 | 6534 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 6535 | `	/* Update the cursor */` |
|      7 | 6536 | `	*pzIn = zIn;` |
|      - | 6537 | `	/* Return to the caller */` |
|      7 | 6538 | `	return SXRET_OK;` |
|      4 | 6539 | `}` |
|      - | 6540 | `/* strtok auxiliary private data */` |
|      - | 6541 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 6542 | `struct strtok_aux_data` |
|      - | 6543 | `{` |
|      - | 6544 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 6545 | `	const char *zIn;   /* Current input stream */` |
|      - | 6546 | `	const char *zEnd;  /* End of input */` |
|      - | 6547 | `};` |
|      - | 6548 | `/*` |
|      - | 6549 | ` * string strtok(string $str,string $token)` |
|      - | 6550 | ` * string strtok(string $token)` |
|      - | 6551 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 6552 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 6553 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 6554 | ` *  words by using the space character as the token.` |
|      - | 6555 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 6556 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 6557 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 6558 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 6559 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 6560 | ` *  the argument are found.` |
|      - | 6561 | ` * Parameters` |
|      - | 6562 | ` *  $str` |
|      - | 6563 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 6564 | ` * $token` |
|      - | 6565 | ` *  The delimiter used when splitting up str.` |
|      - | 6566 | ` * Return` |
|      - | 6567 | ` *   Current token or FALSE on EOF.` |
|      - | 6568 | ` */` |
|      8 | 6569 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6570 | `{` |
|      - | 6571 | `	strtok_aux_data *pAux;` |
|      - | 6572 | `	const char *zMask;` |
|      - | 6573 | `	SyString sToken;` |
|      - | 6574 | `	int nMasklen;` |
|      - | 6575 | `	sxi32 rc;` |
|      9 | 6576 | `	if( nArg < 2 ){` |
|      - | 6577 | `		/* Extract top aux data */` |
|      7 | 6578 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      7 | 6579 | `		if( pAux == 0 ){` |
|      - | 6580 | `			/* No aux data,return FALSE */` |
|    ! 0 | 6581 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6582 | `			return PH7_OK;` |
|      - | 6583 | `		}` |
|      7 | 6584 | `		nMasklen = 0;` |
|      7 | 6585 | `		zMask = ""; /* cc warning */` |
|      7 | 6586 | `		if( nArg > 0 ){` |
|      - | 6587 | `			/* Extract the mask */` |
|      5 | 6588 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 6589 | `		}` |
|      7 | 6590 | `		if( nMasklen < 1 ){` |
|      - | 6591 | `			/* Invalid mask,return FALSE */` |
|      3 | 6592 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|      3 | 6593 | `			ph7_context_free_chunk(pCtx,pAux);` |
|      3 | 6594 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|      3 | 6595 | `			ph7_result_bool(pCtx,0);` |
|      3 | 6596 | `			return PH7_OK;` |
|      - | 6597 | `		}` |
|      - | 6598 | `		/* Extract the token */` |
|      5 | 6599 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 6600 | `		if( rc != SXRET_OK ){` |
|      - | 6601 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 6602 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 6603 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6604 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 6605 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6606 | `		}else{` |
|      - | 6607 | `			/* Return the extracted token */` |
|      5 | 6608 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6609 | `		}` |
|      3 | 6610 | `	}else{` |
|      - | 6611 | `		const char *zInput,*zCur;` |
|      - | 6612 | `		char *zDup;` |
|      - | 6613 | `		int nLen;` |
|      - | 6614 | `		/* Extract the raw input */` |
|      3 | 6615 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 6616 | `		if( nLen < 1 ){` |
|      - | 6617 | `			/* Empty input,return FALSE */` |
|    ! 0 | 6618 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6619 | `			return PH7_OK;` |
|      - | 6620 | `		}` |
|      - | 6621 | `		/* Extract the mask */` |
|      3 | 6622 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 6623 | `		if( nMasklen < 1 ){` |
|      - | 6624 | `			/* Set a default mask */` |
|      - | 6625 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 6626 | `			zMask = TOK_MASK;` |
|    ! 0 | 6627 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 6628 | `#undef TOK_MASK` |
|    ! 0 | 6629 | `		}` |
|      - | 6630 | `		/* Extract a single token */` |
|      3 | 6631 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 6632 | `		if( rc != SXRET_OK ){` |
|      - | 6633 | `			/* Empty input */` |
|    ! 0 | 6634 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6635 | `			return PH7_OK;` |
|    ! 0 | 6636 | `		}else{` |
|      - | 6637 | `			/* Return the extracted token */` |
|      3 | 6638 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6639 | `		}` |
|      - | 6640 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 6641 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 6642 | `		if( pAux ){` |
|      3 | 6643 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 6644 | `			if( nLen < 1 ){` |
|    ! 0 | 6645 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6646 | `				return PH7_OK;` |
|      - | 6647 | `			}` |
|      - | 6648 | `			/* Duplicate input */` |
|      3 | 6649 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 6650 | `			if( zDup  ){` |
|      3 | 6651 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 6652 | `				/* Register the aux data */` |
|      3 | 6653 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 6654 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 6655 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 6656 | `			}` |
|      1 | 6657 | `		}` |
|      - | 6658 | `	}` |
|      7 | 6659 | `	return PH7_OK;` |
|      5 | 6660 | `}` |
|      - | 6661 | `/*` |
|      - | 6662 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 6663 | ` *  Pad a string to a certain length with another string` |
|      - | 6664 | ` * Parameters` |
|      - | 6665 | ` *  $input` |
|      - | 6666 | ` *   The input string.` |
|      - | 6667 | ` * $pad_length` |
|      - | 6668 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 6669 | ` *   string, no padding takes place.` |
|      - | 6670 | ` * $pad_string` |
|      - | 6671 | ` *   Note:` |
|      - | 6672 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 6673 | ` *    divided by the pad_string's length.` |
|      - | 6674 | ` * $pad_type` |
|      - | 6675 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 6676 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 6677 | ` * Return` |
|      - | 6678 | ` *  The padded string.` |
|      - | 6679 | ` */` |
|     10 | 6680 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6681 | `{` |
|      - | 6682 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 6683 | `	const char *zIn,*zPad;` |
|     11 | 6684 | `	if( nArg < 2 ){` |
|      - | 6685 | `		/* Missing arguments,return the empty string */` |
|      5 | 6686 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 6687 | `		return PH7_OK;` |
|      - | 6688 | `	}` |
|      - | 6689 | `	/* Extract the target string */` |
|      7 | 6690 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6691 | `	/* Padding length */` |
|      7 | 6692 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|      7 | 6693 | `	if( iPadlen > 0 ){` |
|      5 | 6694 | `		iPadlen -= iLen;` |
|      2 | 6695 | `	}` |
|      7 | 6696 | `	if( iPadlen < 1  ){` |
|      - | 6697 | `		/* Return the string verbatim */` |
|      3 | 6698 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      3 | 6699 | `		return PH7_OK;` |
|      - | 6700 | `	}` |
|      5 | 6701 | `	zPad = " "; /* Whitespace padding */` |
|      5 | 6702 | `	iStrpad = (int)sizeof(char);` |
|      5 | 6703 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      5 | 6704 | `	if( nArg > 2 ){` |
|      - | 6705 | `		/* Padding string */` |
|      5 | 6706 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      5 | 6707 | `		if( iStrpad < 1 ){` |
|      - | 6708 | `			/* Empty string */` |
|    ! 0 | 6709 | `			zPad = " "; /* Whitespace padding */` |
|    ! 0 | 6710 | `			iStrpad = (int)sizeof(char);` |
|    ! 0 | 6711 | `		}` |
|      5 | 6712 | `		if( nArg > 3 ){` |
|      - | 6713 | `			/* Padd type */` |
|      5 | 6714 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 6715 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 6716 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 6717 | `			}` |
|      2 | 6718 | `		}` |
|      2 | 6719 | `	}` |
|      5 | 6720 | `	iDiv = 1;` |
|      5 | 6721 | `	if( iType == 2 ){` |
|    ! 0 | 6722 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 6723 | `	}` |
|      - | 6724 | `	/* Perform the requested operation */` |
|      5 | 6725 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 6726 | `		jPad = iStrpad;` |
|      5 | 6727 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 6728 | `			/* Padding */` |
|      5 | 6729 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 6730 | `				break;` |
|      - | 6731 | `			}` |
|      3 | 6732 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 6733 | `		}` |
|      3 | 6734 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 6735 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 6736 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 6737 | `				if( jPad > iStrpad ){` |
|    ! 0 | 6738 | `					jPad = iStrpad;` |
|    ! 0 | 6739 | `				}` |
|      3 | 6740 | `				if( jPad < 1){` |
|    ! 0 | 6741 | `					break;` |
|      - | 6742 | `				}` |
|      3 | 6743 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 6744 | `			}` |
|      1 | 6745 | `		}` |
|      1 | 6746 | `	}` |
|      5 | 6747 | `	if( iLen > 0 ){` |
|      - | 6748 | `		/* Append the input string */` |
|      5 | 6749 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 6750 | `	}` |
|      5 | 6751 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 6752 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 6753 | `			/* Padding */` |
|      5 | 6754 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 6755 | `				break;` |
|      - | 6756 | `			}` |
|      3 | 6757 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 6758 | `		}` |
|      5 | 6759 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 6760 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 6761 | `			if( jPad > iStrpad ){` |
|    ! 0 | 6762 | `				jPad = iStrpad;` |
|    ! 0 | 6763 | `			}` |
|      3 | 6764 | `			if( jPad < 1){` |
|    ! 0 | 6765 | `				break;` |
|      - | 6766 | `			}` |
|      3 | 6767 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 6768 | `		}` |
|      1 | 6769 | `	}` |
|      5 | 6770 | `	return PH7_OK;` |
|      6 | 6771 | `}` |
|      - | 6772 | `/*` |
|      - | 6773 | ` * String replacement private data.` |
|      - | 6774 | ` */` |
|      - | 6775 | `typedef struct str_replace_data str_replace_data;` |
|      - | 6776 | `struct str_replace_data` |
|      - | 6777 | `{` |
|      - | 6778 | `	/* The following two fields are only used by the strtr function */` |
|      - | 6779 | `	SyBlob *pWorker;         /* Working buffer */` |
|      - | 6780 | `	ProcStringMatch xMatch;  /* Pattern match routine */` |
|      - | 6781 | `	/* The following two fields are only used by the str_replace function */` |
|      - | 6782 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 6783 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 6784 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 6785 | `};` |
|      - | 6786 | `/*` |
|      - | 6787 | ` * Remove a substring.` |
|      - | 6788 | ` */` |
|      - | 6789 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 6790 | `	for(;;){\` |
|      - | 6791 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 6792 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 6793 | `		++OFFT;\` |
|      - | 6794 | `	}\` |
|      - | 6795 | `}` |
|      - | 6796 | `/*` |
|      - | 6797 | ` * Shift right and insert algorithm.` |
|      - | 6798 | ` */` |
|      - | 6799 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 6800 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 6801 | `		for(;;){\` |
|      - | 6802 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 6803 | `			if(INLEN < 1 ) { break; }\` |
|      - | 6804 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 6805 | `			--INLEN; \` |
|      - | 6806 | `		}\` |
|      - | 6807 | `		for(;;){\` |
|      - | 6808 | `				if(ELEN < 1) { break; }\` |
|      - | 6809 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 6810 | `				OFFT++;\` |
|      - | 6811 | `				ENTRY++;\` |
|      - | 6812 | `				--ELEN;\` |
|      - | 6813 | `		}\` |
|      - | 6814 | `}` |
|      - | 6815 | `/*` |
|      - | 6816 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 6817 | ` * replacement string [i.e: zReplace].` |
|      - | 6818 | ` */` |
|     38 | 6819 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 6820 | `{` |
|     39 | 6821 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 6822 | `	sxu32 n,m;` |
|     39 | 6823 | `	n = SyBlobLength(pWorker);` |
|     39 | 6824 | `	m = nOfft;` |
|      - | 6825 | `	/* Delete the old entry */` |
|    475 | 6826 | `	STRDEL(zInput,n,m,nLen);` |
|     39 | 6827 | `	SyBlobLength(pWorker) -= nLen;` |
|     39 | 6828 | `	if( nReplen > 0 ){` |
|     33 | 6829 | `		sxi32 iRep = nReplen;` |
|      - | 6830 | `		sxi32 rc;` |
|      - | 6831 | `		/*` |
|      - | 6832 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 6833 | `		 * string.` |
|      - | 6834 | `		 */` |
|     33 | 6835 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     33 | 6836 | `		if( rc != SXRET_OK ){` |
|      - | 6837 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 6838 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 6839 | `			return rc;` |
|      - | 6840 | `		}` |
|      - | 6841 | `		/* Perform the insertion now */` |
|     33 | 6842 | `		zInput = (char *)SyBlobData(pWorker);` |
|     33 | 6843 | `		n = SyBlobLength(pWorker);` |
|    163 | 6844 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     33 | 6845 | `		SyBlobLength(pWorker) += nReplen;` |
|     16 | 6846 | `	}` |
|     39 | 6847 | `	return SXRET_OK;` |
|     20 | 6848 | `}` |
|      - | 6849 | `/*` |
|      - | 6850 | ` * String replacement walker callback.` |
|      - | 6851 | ` * The following callback is invoked for each array entry that hold` |
|      - | 6852 | ` * the replace string.` |
|      - | 6853 | ` * Refer to the strtr() implementation for more information.` |
|      - | 6854 | ` */` |
|      8 | 6855 | `static int StringReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6856 | `{` |
|      9 | 6857 | `	str_replace_data *pRepData = (str_replace_data *)pUserData;` |
|      - | 6858 | `	const char *zTarget,*zReplace;` |
|      - | 6859 | `	SyBlob *pWorker;` |
|      - | 6860 | `	int tLen,nLen;` |
|      - | 6861 | `	sxu32 nOfft;` |
|      - | 6862 | `	sxi32 rc;` |
|      - | 6863 | `	/* Point to the working buffer */` |
|      9 | 6864 | `	pWorker = pRepData->pWorker;` |
|      9 | 6865 | `	if( !ph7_value_is_string(pKey) ){` |
|      - | 6866 | `		/* Target and replace must be a string */` |
|      3 | 6867 | `		return PH7_OK;` |
|      - | 6868 | `	}` |
|      - | 6869 | `	/* Extract the target and the replace */` |
|      7 | 6870 | `	zTarget = ph7_value_to_string(pKey,&tLen);` |
|      7 | 6871 | `	if( tLen < 1 ){` |
|      - | 6872 | `		/* Empty target,return immediately */` |
|    ! 0 | 6873 | `		return PH7_OK;` |
|      - | 6874 | `	}` |
|      - | 6875 | `	/* Perform a pattern search */` |
|      7 | 6876 | `	rc = pRepData->xMatch(SyBlobData(pWorker),SyBlobLength(pWorker),(const void *)zTarget,(sxu32)tLen,&nOfft);` |
|      7 | 6877 | `	if( rc != SXRET_OK ){` |
|      - | 6878 | `		/* Pattern not found */` |
|    ! 0 | 6879 | `		return PH7_OK;` |
|      - | 6880 | `	}` |
|      - | 6881 | `	/* Extract the replace string */` |
|      7 | 6882 | `	zReplace = ph7_value_to_string(pData,&nLen);` |
|      - | 6883 | `	/* Perform the replace process */` |
|      7 | 6884 | `	rc = StringReplace(pWorker,nOfft,tLen,zReplace,nLen);` |
|      7 | 6885 | `	if( rc != SXRET_OK ){` |
|      - | 6886 | `		/* Allocation failure: carry it out and stop the walk */` |
|    ! 0 | 6887 | `		pRepData->rc = rc;` |
|    ! 0 | 6888 | `		return rc;` |
|      - | 6889 | `	}` |
|      - | 6890 | `	/* All done */` |
|      7 | 6891 | `	return PH7_OK;` |
|      5 | 6892 | `}` |
|      - | 6893 | `/*` |
|      - | 6894 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 6895 | ` * to collect search/replace string.` |
|      - | 6896 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 6897 | ` */` |
|     26 | 6898 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6899 | `{` |
|     27 | 6900 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 6901 | `	SyString sWorker;` |
|      - | 6902 | `	const char *zIn;` |
|      - | 6903 | `	int nByte;` |
|      - | 6904 | `	/* Extract a string representation of the given argument */` |
|     27 | 6905 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 6906 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 6907 | `	if( nByte > 0 ){` |
|      - | 6908 | `		char *zDup;` |
|      - | 6909 | `		/* Duplicate the chunk */` |
|     25 | 6910 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 6911 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 6912 | `			);` |
|     25 | 6913 | `		if( zDup == 0 ){` |
|      - | 6914 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 6915 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 6916 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 6917 | `			return SXERR_MEM;` |
|      - | 6918 | `		}` |
|     25 | 6919 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 6920 | `		/* Save the chunk */` |
|     25 | 6921 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 6922 | `	}` |
|      - | 6923 | `	/* Save for later processing */` |
|     27 | 6924 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 6925 | `	/* All done */` |
|     13 | 6926 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 6927 | `	return PH7_OK;` |
|     14 | 6928 | `}` |
|      - | 6929 | `/*` |
|      - | 6930 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6931 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6932 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 6933 | ` * Parameters` |
|      - | 6934 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 6935 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 6936 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 6937 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 6938 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 6939 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 6940 | ` * $search` |
|      - | 6941 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 6942 | ` *  to designate multiple needles.` |
|      - | 6943 | ` * $replace` |
|      - | 6944 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 6945 | ` *  to designate multiple replacements.` |
|      - | 6946 | ` * $subject` |
|      - | 6947 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 6948 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 6949 | ` *  of subject, and the return value is an array as well.` |
|      - | 6950 | ` * $count (Not used)` |
|      - | 6951 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 6952 | ` * Return` |
|      - | 6953 | ` * This function returns a string or an array with the replaced values.` |
|      - | 6954 | ` */` |
|  24410 | 6955 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 6956 | `{` |
|      - | 6957 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 6958 | `	ProcStringMatch xMatch;` |
|      - | 6959 | `	const char *zIn,*zFunc;` |
|      - | 6960 | `	str_replace_data sRep;` |
|      - | 6961 | `	SyBlob sWorker;` |
|      - | 6962 | `	SySet sReplace;` |
|      - | 6963 | `	SySet sSearch;` |
|      - | 6964 | `	int rep_str;` |
|      - | 6965 | `	int nByte;` |
|      - | 6966 | `	sxi32 rc;` |
|  24415 | 6967 | `	if( nArg < 3 ){` |
|      - | 6968 | `		/* Missing/Invalid arguments,return null */` |
|      7 | 6969 | `		ph7_result_null(pCtx);` |
|      7 | 6970 | `		return PH7_OK;` |
|      - | 6971 | `	}` |
|      - | 6972 | `	/* Initialize fields */` |
|  24409 | 6973 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  24409 | 6974 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  24409 | 6975 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  24409 | 6976 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  24409 | 6977 | `	sRep.pCtx = pCtx;` |
|  24409 | 6978 | `	sRep.pCollector = &sSearch;` |
|  24409 | 6979 | `	rep_str = 0;` |
|      - | 6980 | `	/* Extract the subject */` |
|  24409 | 6981 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  24409 | 6982 | `	if( nByte < 1 ){` |
|      - | 6983 | `		/* Nothing to replace,return the empty string */` |
|     29 | 6984 | `		ph7_result_string(pCtx,"",0);` |
|     29 | 6985 | `		return PH7_OK;` |
|      - | 6986 | `	}` |
|      - | 6987 | `	/* Copy the subject */` |
|  24381 | 6988 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 6989 | `	/* Search string */` |
|  24381 | 6990 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 6991 | `		/* Collect search string */` |
|      9 | 6992 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 6993 | `	}else{` |
|      - | 6994 | `		/* Single pattern */` |
|  24373 | 6995 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  24373 | 6996 | `		if( nByte < 1 ){` |
|      - | 6997 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 6998 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 6999 | `			return PH7_OK;` |
|      - | 7000 | `		}` |
|  24369 | 7001 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 7002 | `		/* Save for later processing */` |
|  24369 | 7003 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 7004 | `	}` |
|      - | 7005 | `	/* Replace string */` |
|  24377 | 7006 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 7007 | `		/* Collect replace string */` |
|      7 | 7008 | `		sRep.pCollector = &sReplace;` |
|      7 | 7009 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 7010 | `	}else{` |
|      - | 7011 | `		/* Single needle */` |
|  24371 | 7012 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  24371 | 7013 | `		rep_str = 1;` |
|  24371 | 7014 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 7015 | `		/* Save for later processing */` |
|  24371 | 7016 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 7017 | `	}` |
|      - | 7018 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  24377 | 7019 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 7020 | `		SySetRelease(&sSearch);` |
|    ! 0 | 7021 | `		SySetRelease(&sReplace);` |
|    ! 0 | 7022 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 7023 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7024 | `	}` |
|      - | 7025 | `	/* Reset loop cursors */` |
|  24377 | 7026 | `	SySetResetCursor(&sSearch);` |
|  24377 | 7027 | `	SySetResetCursor(&sReplace);` |
|  24377 | 7028 | `	pReplace = pSearch = 0; /* cc warning */` |
|  24377 | 7029 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 7030 | `	/* Extract function name */` |
|  24377 | 7031 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 7032 | `	/* Set the default pattern match routine */` |
|  24377 | 7033 | `	xMatch = SyBlobSearch;` |
|  24377 | 7034 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 7035 | `		/* Case insensitive pattern match */` |
|     11 | 7036 | `		xMatch = iPatternMatch;` |
|      5 | 7037 | `	}` |
|      - | 7038 | `	/* Start the replace process */` |
|  48757 | 7039 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 7040 | `		sxu32 nCount,nOfft;` |
|  24385 | 7041 | `		if( pSearch->nByte <  1 ){` |
|      - | 7042 | `			/* Empty string,ignore */` |
|      3 | 7043 | `			continue;` |
|      - | 7044 | `		}` |
|      - | 7045 | `		/* Extract the replace string */` |
|  24383 | 7046 | `		if( rep_str ){` |
|  24373 | 7047 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  12189 | 7048 | `		}else{` |
|     11 | 7049 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 7050 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 7051 | `				 * An empty string is used for the rest of replacement values` |
|      - | 7052 | `				 */` |
|      3 | 7053 | `				pReplace = 0;` |
|      1 | 7054 | `			}` |
|      - | 7055 | `		}` |
|  24383 | 7056 | `		if( pReplace == 0 ){` |
|      - | 7057 | `			/* Use an empty string instead */` |
|      3 | 7058 | `			pReplace = &sTemp;` |
|      1 | 7059 | `		}` |
|  24383 | 7060 | `		nOfft = nCount = 0;` |
|  12205 | 7061 | `		for(;;){` |
|  24415 | 7062 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 7063 | `				break;` |
|      - | 7064 | `			}` |
|      - | 7065 | `			/* Perform a pattern lookup */` |
|  36602 | 7066 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  24398 | 7067 | `				pSearch->nByte,&nOfft);` |
|  24403 | 7068 | `			if( rc != SXRET_OK ){` |
|      - | 7069 | `				/* Pattern not found */` |
|  24371 | 7070 | `				break;` |
|      - | 7071 | `			}` |
|      - | 7072 | `			/* Perform the replace operation */` |
|     33 | 7073 | `			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|     33 | 7074 | `			if( rc != SXRET_OK ){` |
|      - | 7075 | `				/* Allocation failure: surface a fatal instead of a partial result */` |
|    ! 0 | 7076 | `				SySetRelease(&sSearch);` |
|    ! 0 | 7077 | `				SySetRelease(&sReplace);` |
|    ! 0 | 7078 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 7079 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 7080 | `			}` |
|      - | 7081 | `			/* Increment offset counter */` |
|     33 | 7082 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 7083 | `		}` |
|      5 | 7084 | `	}` |
|      - | 7085 | `	/* All done,clean-up the mess left behind */` |
|  24377 | 7086 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  24377 | 7087 | `	SySetRelease(&sSearch);` |
|  24377 | 7088 | `	SySetRelease(&sReplace);` |
|  24377 | 7089 | `	SyBlobRelease(&sWorker);` |
|  24377 | 7090 | `	if( rc != PH7_OK ){` |
|    ! 0 | 7091 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7092 | `	}` |
|  24377 | 7093 | `	return PH7_OK;` |
|  12210 | 7094 | `}` |
|      - | 7095 | `/*` |
|      - | 7096 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 7097 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 7098 | ` *  Translate characters or replace substrings.` |
|      - | 7099 | ` * Parameters` |
|      - | 7100 | ` *  $str` |
|      - | 7101 | ` *  The string being translated.` |
|      - | 7102 | ` * $from` |
|      - | 7103 | ` *  The string being translated to to.` |
|      - | 7104 | ` * $to` |
|      - | 7105 | ` *  The string replacing from.` |
|      - | 7106 | ` * $replace_pairs` |
|      - | 7107 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 7108 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 7109 | ` * Return` |
|      - | 7110 | ` *  The translated string.` |
|      - | 7111 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 7112 | ` */` |
|     12 | 7113 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7114 | `{` |
|      - | 7115 | `	const char *zIn;` |
|      - | 7116 | `	int nLen;` |
|     13 | 7117 | `	if( nArg < 1 ){` |
|      - | 7118 | `		/* Nothing to replace,return FALSE */` |
|      7 | 7119 | `		ph7_result_bool(pCtx,0);` |
|      7 | 7120 | `		return PH7_OK;` |
|      - | 7121 | `	}` |
|      7 | 7122 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 7123 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 7124 | `		/* Invalid arguments */` |
|    ! 0 | 7125 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 7126 | `		return PH7_OK;` |
|      - | 7127 | `	}` |
|      9 | 7128 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 7129 | `		str_replace_data sRepData;` |
|      - | 7130 | `		SyBlob sWorker;` |
|      - | 7131 | `		sxi32 rc;` |
|      - | 7132 | `		/* Initilaize the working buffer */` |
|      5 | 7133 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|      - | 7134 | `		/* Copy raw string */` |
|      5 | 7135 | `		SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nLen);` |
|      - | 7136 | `		/* Init our replace data instance */` |
|      5 | 7137 | `		sRepData.pWorker = &sWorker;` |
|      5 | 7138 | `		sRepData.xMatch = SyBlobSearch;` |
|      5 | 7139 | `		sRepData.rc = SXRET_OK;` |
|      - | 7140 | `		/* Iterate throw array entries and perform the replace operation.*/` |
|      5 | 7141 | `		ph7_array_walk(apArg[1],StringReplaceWalker,&sRepData);` |
|      5 | 7142 | `		if( sRepData.rc != SXRET_OK ){` |
|      - | 7143 | `			/* Allocation failure during replacement: surface a fatal */` |
|    ! 0 | 7144 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 7145 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 7146 | `		}` |
|      - | 7147 | `		/* All done, return the result string */` |
|      7 | 7148 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|      4 | 7149 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 7150 | `		/* Clean-up */` |
|      5 | 7151 | `		SyBlobRelease(&sWorker);` |
|      5 | 7152 | `		if( rc != PH7_OK ){` |
|    ! 0 | 7153 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 7154 | `		}` |
|      3 | 7155 | `	}else{` |
|      - | 7156 | `		int i,flen,tlen,c,iOfft;` |
|      - | 7157 | `		const char *zFrom,*zTo;` |
|      3 | 7158 | `		if( nArg < 3 ){` |
|      - | 7159 | `			/* Nothing to replace */` |
|    ! 0 | 7160 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 7161 | `			return PH7_OK;` |
|      - | 7162 | `		}` |
|      - | 7163 | `		/* Extract given arguments */` |
|      3 | 7164 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 7165 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 7166 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 7167 | `			/* Nothing to replace */` |
|    ! 0 | 7168 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 7169 | `			return PH7_OK;` |
|      - | 7170 | `		}` |
|      - | 7171 | `		/* Start the replace process */` |
|     13 | 7172 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 7173 | `			c = zIn[i];` |
|     11 | 7174 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 7175 | `				if ( iOfft < tlen ){` |
|      5 | 7176 | `					c = zTo[iOfft];` |
|      2 | 7177 | `				}` |
|      2 | 7178 | `			}` |
|     11 | 7179 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 7180 |  |
|      6 | 7181 | `		}` |
|      - | 7182 | `	}` |
|      7 | 7183 | `	return PH7_OK;` |
|      7 | 7184 | `}` |
|      - | 7185 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7186 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7187 | `/*` |
|      - | 7188 | ` * Parse an INI string.` |
|      - | 7189 |  |
|      - | 7190 | ` * According to wikipedia` |
|      - | 7191 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 7192 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 7193 | ` *  Format` |
|      - | 7194 | `*    Properties` |
|      - | 7195 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 7196 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 7197 | `*     Example:` |
|      - | 7198 | `*      name=value` |
|      - | 7199 | `*    Sections` |
|      - | 7200 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 7201 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 7202 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 7203 | `*     or the end of the file. Sections may not be nested.` |
|      - | 7204 | `*     Example:` |
|      - | 7205 | `*      [section]` |
|      - | 7206 | `*   Comments` |
|      - | 7207 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 7208 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 7209 | `*/` |
|     12 | 7210 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 7211 | `{` |
|      - | 7212 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 7213 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 7214 | `	SyHashEntry *pEntry;` |
|      - | 7215 | `	SyString sEntry;` |
|      - | 7216 | `	SyHash sHash;` |
|      - | 7217 | `	int c;` |
|      - | 7218 | `	/* Create an empty array and worker variables */` |
|     13 | 7219 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 7220 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 7221 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 7222 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 7223 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|    ! 0 | 7224 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7225 | `	}` |
|     13 | 7226 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 7227 | `	pCur = pArray;` |
|      - | 7228 | `	/* Start the parse process */` |
|     21 | 7229 | `	for(;;){` |
|      - | 7230 | `		/* Ignore leading white spaces */` |
|     69 | 7231 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 7232 | `			zIn++;` |
|      1 | 7233 | `		}` |
|     43 | 7234 | `		if( zIn >= zEnd ){` |
|      - | 7235 | `			/* No more input to process */` |
|     13 | 7236 | `			break;` |
|      - | 7237 | `		}` |
|     31 | 7238 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 7239 | `			/* Comment til the end of line */` |
|    ! 0 | 7240 | `			zIn++;` |
|    ! 0 | 7241 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 7242 | `				zIn++;` |
|    ! 0 | 7243 | `			}` |
|    ! 0 | 7244 | `			continue;` |
|      - | 7245 | `		}` |
|      - | 7246 | `		/* Reset the string cursor of the working variable */` |
|     31 | 7247 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 7248 | `		if( zIn[0] == '[' ){` |
|      - | 7249 | `			/* Section: Extract the section name */` |
|      9 | 7250 | `			zIn++;` |
|      9 | 7251 | `			zCur = zIn;` |
|     73 | 7252 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 7253 | `				zIn++;` |
|      1 | 7254 | `			}` |
|      9 | 7255 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 7256 | `				/* Save the section name */` |
|      5 | 7257 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 7258 | `				SyStringFullTrim(&sEntry);` |
|      5 | 7259 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 7260 | `				if( sEntry.nByte > 0 ){` |
|      - | 7261 | `					/* Associate an array with the section */` |
|      5 | 7262 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 7263 | `					if( pSection ){` |
|      5 | 7264 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 7265 | `						pCur = pSection;` |
|      2 | 7266 | `					}` |
|      2 | 7267 | `				}` |
|      2 | 7268 | `			}` |
|      9 | 7269 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 7270 | `		}else{` |
|      - | 7271 | `			ph7_value *pOldCur;` |
|      - | 7272 | `			int is_array;` |
|      - | 7273 | `			int iLen;` |
|      - | 7274 | `			/* Properties */` |
|     23 | 7275 | `			is_array = 0;` |
|     23 | 7276 | `			zCur = zIn;` |
|     23 | 7277 | `			iLen = 0; /* cc warning */` |
|     23 | 7278 | `			pOldCur = pCur;` |
|    155 | 7279 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 7280 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 7281 | `					/* Array */` |
|    ! 0 | 7282 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 7283 | `					is_array = 1;` |
|    ! 0 | 7284 | `					if( iLen > 0 ){` |
|    ! 0 | 7285 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 7286 | `						/* Query the hashtable */` |
|    ! 0 | 7287 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 7288 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 7289 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 7290 | `						if( pEntry ){` |
|    ! 0 | 7291 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 7292 | `						}else{` |
|      - | 7293 | `							/* Create an empty array */` |
|    ! 0 | 7294 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 7295 | `							if( pvArr ){` |
|      - | 7296 | `								/* Save the entry */` |
|    ! 0 | 7297 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 7298 | `								/* Insert the entry */` |
|    ! 0 | 7299 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 7300 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 7301 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 7302 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 7303 | `							}` |
|      - | 7304 | `						}` |
|    ! 0 | 7305 | `						if( pvArr ){` |
|    ! 0 | 7306 | `							pCur = pvArr;` |
|    ! 0 | 7307 | `						}` |
|    ! 0 | 7308 | `					}` |
|    ! 0 | 7309 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 7310 | `						zIn++;` |
|    ! 0 | 7311 | `					}` |
|    ! 0 | 7312 | `				}` |
|    133 | 7313 | `				zIn++;` |
|      1 | 7314 | `			}` |
|     23 | 7315 | `			if( !is_array ){` |
|     23 | 7316 | `				iLen = (int)(zIn-zCur);` |
|     11 | 7317 | `			}` |
|      - | 7318 | `			/* Trim the key */` |
|     23 | 7319 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 7320 | `			SyStringFullTrim(&sEntry);` |
|     23 | 7321 | `			if( sEntry.nByte > 0 ){` |
|     23 | 7322 | `				if( !is_array ){` |
|      - | 7323 | `					/* Save the key name */` |
|     23 | 7324 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 7325 | `				}` |
|      - | 7326 | `				/* extract key value */` |
|     23 | 7327 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 7328 | `				zIn++; /* '=' */` |
|     39 | 7329 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 7330 | `					zIn++;` |
|      1 | 7331 | `				}` |
|     23 | 7332 | `				if( zIn < zEnd ){` |
|     21 | 7333 | `					zCur = zIn;` |
|     21 | 7334 | `					c = zIn[0];` |
|     21 | 7335 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 7336 | `						zIn++;` |
|      - | 7337 | `						/* Delimit the value */` |
|    ! 0 | 7338 | `						while( zIn < zEnd ){` |
|    ! 0 | 7339 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 7340 | `								break;` |
|      - | 7341 | `							}` |
|    ! 0 | 7342 | `							zIn++;` |
|    ! 0 | 7343 | `						}` |
|    ! 0 | 7344 | `						if( zIn < zEnd ){` |
|    ! 0 | 7345 | `							zIn++;` |
|    ! 0 | 7346 | `						}` |
|    ! 0 | 7347 | `					}else{` |
|    125 | 7348 | `						while( zIn < zEnd ){` |
|    123 | 7349 | `							if( zIn[0] == '\n' ){` |
|     19 | 7350 | `								if( zIn[-1] != '\\' ){` |
|     19 | 7351 | `									break;` |
|    ! 0 | 7352 | `								}` |
|    105 | 7353 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 7354 | `								/* Inline comments */` |
|    ! 0 | 7355 | `								break;` |
|      - | 7356 | `							}` |
|    105 | 7357 | `							zIn++;` |
|      1 | 7358 | `						}` |
|      - | 7359 | `					}` |
|      - | 7360 | `					/* Trim the value */` |
|     21 | 7361 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 7362 | `					SyStringFullTrim(&sEntry);` |
|     21 | 7363 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 7364 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 7365 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 7366 | `					}` |
|     21 | 7367 | `					if( sEntry.nByte > 0 ){` |
|     21 | 7368 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 7369 | `					}` |
|      - | 7370 | `					/* Insert the key and it's value */` |
|     21 | 7371 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 7372 | `				}` |
|     12 | 7373 | `			}else{` |
|    ! 0 | 7374 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 7375 | `					zIn++;` |
|    ! 0 | 7376 | `				}` |
|      - | 7377 | `			}` |
|     23 | 7378 | `			pCur = pOldCur;` |
|      - | 7379 | `		}` |
|      1 | 7380 | `	}` |
|     13 | 7381 | `	SyHashRelease(&sHash);` |
|      - | 7382 | `	/* Return the parse of the INI string */` |
|     13 | 7383 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 7384 | `	return SXRET_OK;` |
|      7 | 7385 | `}` |
|      - | 7386 | `/*` |
|      - | 7387 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 7388 | ` *  Parse a configuration string.` |
|      - | 7389 | ` * Parameters` |
|      - | 7390 | ` *  $ini` |
|      - | 7391 | ` *   The contents of the ini file being parsed.` |
|      - | 7392 | ` *  $process_sections` |
|      - | 7393 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 7394 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 7395 | ` *  $scanner_mode (Not used)` |
|      - | 7396 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 7397 | ` *   then option values will not be parsed.` |
|      - | 7398 | ` * Return` |
|      - | 7399 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 7400 | ` */` |
|     10 | 7401 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7402 | `{` |
|      - | 7403 | `	const char *zIni;` |
|      - | 7404 | `	int nByte;` |
|     11 | 7405 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7406 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 7407 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7408 | `		return PH7_OK;` |
|      - | 7409 | `	}` |
|      - | 7410 | `	/* Extract the raw INI buffer */` |
|     11 | 7411 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 7412 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|     11 | 7413 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      6 | 7414 | `}` |
|      - | 7415 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7416 |  |
|      - | 7417 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7418 |  |
|      - | 7419 | `/*` |
|      - | 7420 | ` * Ctype Functions.` |
|      - | 7421 | ` * Status:` |
|      - | 7422 | ` *    Stable.` |
|      - | 7423 | ` */` |
|      - | 7424 | `/*` |
|      - | 7425 | ` * bool ctype_alnum(string $text)` |
|      - | 7426 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 7427 | ` * Parameters` |
|      - | 7428 | ` *  $text` |
|      - | 7429 | ` *   The tested string.` |
|      - | 7430 | ` * Return` |
|      - | 7431 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 7432 | ` */` |
|     16 | 7433 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7434 | `{` |
|      - | 7435 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7436 | `	int nLen;` |
|     17 | 7437 | `	if( nArg < 1 ){` |
|      - | 7438 | `		/* Missing arguments,return FALSE */` |
|      3 | 7439 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7440 | `		return PH7_OK;` |
|      - | 7441 | `	}` |
|      - | 7442 | `	/* Extract the target string */` |
|     15 | 7443 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 7444 | `	zEnd = &zIn[nLen];` |
|     15 | 7445 | `	if( nLen < 1 ){` |
|      - | 7446 | `		/* Empty string,return FALSE */` |
|      3 | 7447 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7448 | `		return PH7_OK;` |
|      - | 7449 | `	}` |
|      - | 7450 | `	/* Perform the requested operation */` |
|     32 | 7451 | `	for(;;){` |
|     65 | 7452 | `		if( zIn >= zEnd ){` |
|      - | 7453 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7454 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7455 | `			return PH7_OK;` |
|      - | 7456 | `		}` |
|     57 | 7457 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 7458 | `			break;` |
|      - | 7459 | `		}` |
|      - | 7460 | `		/* Point to the next character */` |
|     53 | 7461 | `		zIn++;` |
|      1 | 7462 | `	}` |
|      - | 7463 | `	/* The test failed,return FALSE */` |
|      5 | 7464 | `	ph7_result_bool(pCtx,0);` |
|      5 | 7465 | `	return PH7_OK;` |
|      9 | 7466 | `}` |
|      - | 7467 | `/*` |
|      - | 7468 | ` * bool ctype_alpha(string $text)` |
|      - | 7469 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 7470 | ` * Parameters` |
|      - | 7471 | ` *  $text` |
|      - | 7472 | ` *   The tested string.` |
|      - | 7473 | ` * Return` |
|      - | 7474 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 7475 | ` */` |
|     18 | 7476 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7477 | `{` |
|      - | 7478 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7479 | `	int nLen;` |
|     19 | 7480 | `	if( nArg < 1 ){` |
|      - | 7481 | `		/* Missing arguments,return FALSE */` |
|      3 | 7482 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7483 | `		return PH7_OK;` |
|      - | 7484 | `	}` |
|      - | 7485 | `	/* Extract the target string */` |
|     17 | 7486 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7487 | `	zEnd = &zIn[nLen];` |
|     17 | 7488 | `	if( nLen < 1 ){` |
|      - | 7489 | `		/* Empty string,return FALSE */` |
|      3 | 7490 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7491 | `		return PH7_OK;` |
|      - | 7492 | `	}` |
|      - | 7493 | `	/* Perform the requested operation */` |
|     42 | 7494 | `	for(;;){` |
|     85 | 7495 | `		if( zIn >= zEnd ){` |
|      - | 7496 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7497 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7498 | `			return PH7_OK;` |
|      - | 7499 | `		}` |
|     77 | 7500 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 7501 | `			break;` |
|      - | 7502 | `		}` |
|      - | 7503 | `		/* Point to the next character */` |
|     71 | 7504 | `		zIn++;` |
|      1 | 7505 | `	}` |
|      - | 7506 | `	/* The test failed,return FALSE */` |
|      7 | 7507 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7508 | `	return PH7_OK;` |
|     10 | 7509 | `}` |
|      - | 7510 | `/*` |
|      - | 7511 | ` * bool ctype_cntrl(string $text)` |
|      - | 7512 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 7513 | ` * Parameters` |
|      - | 7514 | ` *  $text` |
|      - | 7515 | ` *   The tested string.` |
|      - | 7516 | ` * Return` |
|      - | 7517 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 7518 | ` */` |
|     18 | 7519 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7520 | `{` |
|      - | 7521 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7522 | `	int nLen;` |
|     19 | 7523 | `	if( nArg < 1 ){` |
|      - | 7524 | `		/* Missing arguments,return FALSE */` |
|      3 | 7525 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7526 | `		return PH7_OK;` |
|      - | 7527 | `	}` |
|      - | 7528 | `	/* Extract the target string */` |
|     17 | 7529 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7530 | `	zEnd = &zIn[nLen];` |
|     17 | 7531 | `	if( nLen < 1 ){` |
|      - | 7532 | `		/* Empty string,return FALSE */` |
|      3 | 7533 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7534 | `		return PH7_OK;` |
|      - | 7535 | `	}` |
|      - | 7536 | `	/* Perform the requested operation */` |
|     14 | 7537 | `	for(;;){` |
|     29 | 7538 | `		if( zIn >= zEnd ){` |
|      - | 7539 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7540 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7541 | `			return PH7_OK;` |
|      - | 7542 | `		}` |
|     21 | 7543 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7544 | `			/* UTF-8 stream  */` |
|    ! 0 | 7545 | `			break;` |
|      - | 7546 | `		}` |
|     21 | 7547 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 7548 | `			break;` |
|      - | 7549 | `		}` |
|      - | 7550 | `		/* Point to the next character */` |
|     15 | 7551 | `		zIn++;` |
|      1 | 7552 | `	}` |
|      - | 7553 | `	/* The test failed,return FALSE */` |
|      7 | 7554 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7555 | `	return PH7_OK;` |
|     10 | 7556 | `}` |
|      - | 7557 | `/*` |
|      - | 7558 | ` * bool ctype_digit(string $text)` |
|      - | 7559 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 7560 | ` * Parameters` |
|      - | 7561 | ` *  $text` |
|      - | 7562 | ` *   The tested string.` |
|      - | 7563 | ` * Return` |
|      - | 7564 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 7565 | ` */` |
|   1638 | 7566 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7567 | `{` |
|      - | 7568 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7569 | `	int nLen;` |
|   1643 | 7570 | `	if( nArg < 1 ){` |
|      - | 7571 | `		/* Missing arguments,return FALSE */` |
|      3 | 7572 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7573 | `		return PH7_OK;` |
|      - | 7574 | `	}` |
|      - | 7575 | `	/* Extract the target string */` |
|   1641 | 7576 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1641 | 7577 | `	zEnd = &zIn[nLen];` |
|   1641 | 7578 | `	if( nLen < 1 ){` |
|      - | 7579 | `		/* Empty string,return FALSE */` |
|      3 | 7580 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7581 | `		return PH7_OK;` |
|      - | 7582 | `	}` |
|      - | 7583 | `	/* Perform the requested operation */` |
|   1538 | 7584 | `	for(;;){` |
|   3081 | 7585 | `		if( zIn >= zEnd ){` |
|      - | 7586 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1397 | 7587 | `			ph7_result_bool(pCtx,1);` |
|   1397 | 7588 | `			return PH7_OK;` |
|      - | 7589 | `		}` |
|   1689 | 7590 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7591 | `			/* UTF-8 stream  */` |
|    ! 0 | 7592 | `			break;` |
|      - | 7593 | `		}` |
|   1689 | 7594 | `		if( !SyisDigit(zIn[0]) ){` |
|    247 | 7595 | `			break;` |
|      - | 7596 | `		}` |
|      - | 7597 | `		/* Point to the next character */` |
|   1447 | 7598 | `		zIn++;` |
|      5 | 7599 | `	}` |
|      - | 7600 | `	/* The test failed,return FALSE */` |
|    247 | 7601 | `	ph7_result_bool(pCtx,0);` |
|    247 | 7602 | `	return PH7_OK;` |
|    824 | 7603 | `}` |
|      - | 7604 | `/*` |
|      - | 7605 | ` * bool ctype_xdigit(string $text)` |
|      - | 7606 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 7607 | ` * Parameters` |
|      - | 7608 | ` *  $text` |
|      - | 7609 | ` *   The tested string.` |
|      - | 7610 | ` * Return` |
|      - | 7611 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 7612 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 7613 | ` */` |
|     20 | 7614 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7615 | `{` |
|      - | 7616 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7617 | `	int nLen;` |
|     21 | 7618 | `	if( nArg < 1 ){` |
|      - | 7619 | `		/* Missing arguments,return FALSE */` |
|      3 | 7620 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7621 | `		return PH7_OK;` |
|      - | 7622 | `	}` |
|      - | 7623 | `	/* Extract the target string */` |
|     19 | 7624 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 7625 | `	zEnd = &zIn[nLen];` |
|     19 | 7626 | `	if( nLen < 1 ){` |
|      - | 7627 | `		/* Empty string,return FALSE */` |
|      3 | 7628 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7629 | `		return PH7_OK;` |
|      - | 7630 | `	}` |
|      - | 7631 | `	/* Perform the requested operation */` |
|     46 | 7632 | `	for(;;){` |
|     93 | 7633 | `		if( zIn >= zEnd ){` |
|      - | 7634 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 7635 | `			ph7_result_bool(pCtx,1);` |
|     11 | 7636 | `			return PH7_OK;` |
|      - | 7637 | `		}` |
|     83 | 7638 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7639 | `			/* UTF-8 stream  */` |
|    ! 0 | 7640 | `			break;` |
|      - | 7641 | `		}` |
|     83 | 7642 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 7643 | `			break;` |
|      - | 7644 | `		}` |
|      - | 7645 | `		/* Point to the next character */` |
|     77 | 7646 | `		zIn++;` |
|      1 | 7647 | `	}` |
|      - | 7648 | `	/* The test failed,return FALSE */` |
|      7 | 7649 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7650 | `	return PH7_OK;` |
|     11 | 7651 | `}` |
|      - | 7652 | `/*` |
|      - | 7653 | ` * bool ctype_graph(string $text)` |
|      - | 7654 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 7655 | ` * Parameters` |
|      - | 7656 | ` *  $text` |
|      - | 7657 | ` *   The tested string.` |
|      - | 7658 | ` * Return` |
|      - | 7659 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 7660 | ` * (no white space), FALSE otherwise.` |
|      - | 7661 | ` */` |
|     18 | 7662 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7663 | `{` |
|      - | 7664 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7665 | `	int nLen;` |
|     19 | 7666 | `	if( nArg < 1 ){` |
|      - | 7667 | `		/* Missing arguments,return FALSE */` |
|      3 | 7668 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7669 | `		return PH7_OK;` |
|      - | 7670 | `	}` |
|      - | 7671 | `	/* Extract the target string */` |
|     17 | 7672 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7673 | `	zEnd = &zIn[nLen];` |
|     17 | 7674 | `	if( nLen < 1 ){` |
|      - | 7675 | `		/* Empty string,return FALSE */` |
|      3 | 7676 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7677 | `		return PH7_OK;` |
|      - | 7678 | `	}` |
|      - | 7679 | `	/* Perform the requested operation */` |
|     57 | 7680 | `	for(;;){` |
|    115 | 7681 | `		if( zIn >= zEnd ){` |
|      - | 7682 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7683 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7684 | `			return PH7_OK;` |
|      - | 7685 | `		}` |
|    107 | 7686 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7687 | `			/* UTF-8 stream  */` |
|    ! 0 | 7688 | `			break;` |
|      - | 7689 | `		}` |
|    107 | 7690 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 7691 | `			break;` |
|      - | 7692 | `		}` |
|      - | 7693 | `		/* Point to the next character */` |
|    101 | 7694 | `		zIn++;` |
|      1 | 7695 | `	}` |
|      - | 7696 | `	/* The test failed,return FALSE */` |
|      7 | 7697 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7698 | `	return PH7_OK;` |
|     10 | 7699 | `}` |
|      - | 7700 | `/*` |
|      - | 7701 | ` * bool ctype_print(string $text)` |
|      - | 7702 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 7703 | ` * Parameters` |
|      - | 7704 | ` *  $text` |
|      - | 7705 | ` *   The tested string.` |
|      - | 7706 | ` * Return` |
|      - | 7707 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 7708 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 7709 | ` *  or control function at all.` |
|      - | 7710 | ` */` |
|     18 | 7711 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7712 | `{` |
|      - | 7713 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7714 | `	int nLen;` |
|     19 | 7715 | `	if( nArg < 1 ){` |
|      - | 7716 | `		/* Missing arguments,return FALSE */` |
|      3 | 7717 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7718 | `		return PH7_OK;` |
|      - | 7719 | `	}` |
|      - | 7720 | `	/* Extract the target string */` |
|     17 | 7721 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7722 | `	zEnd = &zIn[nLen];` |
|     17 | 7723 | `	if( nLen < 1 ){` |
|      - | 7724 | `		/* Empty string,return FALSE */` |
|      3 | 7725 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7726 | `		return PH7_OK;` |
|      - | 7727 | `	}` |
|      - | 7728 | `	/* Perform the requested operation */` |
|     63 | 7729 | `	for(;;){` |
|    127 | 7730 | `		if( zIn >= zEnd ){` |
|      - | 7731 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7732 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7733 | `			return PH7_OK;` |
|      - | 7734 | `		}` |
|    119 | 7735 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7736 | `			/* UTF-8 stream  */` |
|    ! 0 | 7737 | `			break;` |
|      - | 7738 | `		}` |
|    119 | 7739 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 7740 | `			break;` |
|      - | 7741 | `		}` |
|      - | 7742 | `		/* Point to the next character */` |
|    113 | 7743 | `		zIn++;` |
|      1 | 7744 | `	}` |
|      - | 7745 | `	/* The test failed,return FALSE */` |
|      7 | 7746 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7747 | `	return PH7_OK;` |
|     10 | 7748 | `}` |
|      - | 7749 | `/*` |
|      - | 7750 | ` * bool ctype_punct(string $text)` |
|      - | 7751 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 7752 | ` * Parameters` |
|      - | 7753 | ` *  $text` |
|      - | 7754 | ` *   The tested string.` |
|      - | 7755 | ` * Return` |
|      - | 7756 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 7757 | ` *  digit or blank, FALSE otherwise.` |
|      - | 7758 | ` */` |
|     20 | 7759 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7760 | `{` |
|      - | 7761 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7762 | `	int nLen;` |
|     21 | 7763 | `	if( nArg < 1 ){` |
|      - | 7764 | `		/* Missing arguments,return FALSE */` |
|      3 | 7765 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7766 | `		return PH7_OK;` |
|      - | 7767 | `	}` |
|      - | 7768 | `	/* Extract the target string */` |
|     19 | 7769 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 7770 | `	zEnd = &zIn[nLen];` |
|     19 | 7771 | `	if( nLen < 1 ){` |
|      - | 7772 | `		/* Empty string,return FALSE */` |
|      3 | 7773 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7774 | `		return PH7_OK;` |
|      - | 7775 | `	}` |
|      - | 7776 | `	/* Perform the requested operation */` |
|     38 | 7777 | `	for(;;){` |
|     77 | 7778 | `		if( zIn >= zEnd ){` |
|      - | 7779 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7780 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7781 | `			return PH7_OK;` |
|      - | 7782 | `		}` |
|     69 | 7783 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7784 | `			/* UTF-8 stream  */` |
|    ! 0 | 7785 | `			break;` |
|      - | 7786 | `		}` |
|     69 | 7787 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 7788 | `			break;` |
|      - | 7789 | `		}` |
|      - | 7790 | `		/* Point to the next character */` |
|     61 | 7791 | `		zIn++;` |
|      1 | 7792 | `	}` |
|      - | 7793 | `	/* The test failed,return FALSE */` |
|      9 | 7794 | `	ph7_result_bool(pCtx,0);` |
|      9 | 7795 | `	return PH7_OK;` |
|     11 | 7796 | `}` |
|      - | 7797 | `/*` |
|      - | 7798 | ` * bool ctype_space(string $text)` |
|      - | 7799 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 7800 | ` * Parameters` |
|      - | 7801 | ` *  $text` |
|      - | 7802 | ` *   The tested string.` |
|      - | 7803 | ` * Return` |
|      - | 7804 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 7805 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 7806 | ` *  and form feed characters.` |
|      - | 7807 | ` */` |
|  62045 | 7808 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7809 | `{` |
|      - | 7810 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7811 | `	int nLen;` |
|  62050 | 7812 | `	if( nArg < 1 ){` |
|      - | 7813 | `		/* Missing arguments,return FALSE */` |
|      3 | 7814 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7815 | `		return PH7_OK;` |
|      - | 7816 | `	}` |
|      - | 7817 | `	/* Extract the target string */` |
|  62048 | 7818 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  62048 | 7819 | `	zEnd = &zIn[nLen];` |
|  62048 | 7820 | `	if( nLen < 1 ){` |
|      - | 7821 | `		/* Empty string,return FALSE */` |
|      3 | 7822 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7823 | `		return PH7_OK;` |
|      - | 7824 | `	}` |
|      - | 7825 | `	/* Perform the requested operation */` |
|  32128 | 7826 | `	for(;;){` |
|  64176 | 7827 | `		if( zIn >= zEnd ){` |
|      - | 7828 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   2111 | 7829 | `			ph7_result_bool(pCtx,1);` |
|   2111 | 7830 | `			return PH7_OK;` |
|      - | 7831 | `		}` |
|  62070 | 7832 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7833 | `			/* UTF-8 stream  */` |
|    ! 0 | 7834 | `			break;` |
|      - | 7835 | `		}` |
|  62070 | 7836 | `		if( !SyisSpace(zIn[0]) ){` |
|  59940 | 7837 | `			break;` |
|      - | 7838 | `		}` |
|      - | 7839 | `		/* Point to the next character */` |
|   2135 | 7840 | `		zIn++;` |
|      5 | 7841 | `	}` |
|      - | 7842 | `	/* The test failed,return FALSE */` |
|  59940 | 7843 | `	ph7_result_bool(pCtx,0);` |
|  59940 | 7844 | `	return PH7_OK;` |
|  31070 | 7845 | `}` |
|      - | 7846 | `/*` |
|      - | 7847 | ` * bool ctype_lower(string $text)` |
|      - | 7848 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 7849 | ` * Parameters` |
|      - | 7850 | ` *  $text` |
|      - | 7851 | ` *   The tested string.` |
|      - | 7852 | ` * Return` |
|      - | 7853 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 7854 | ` */` |
|     18 | 7855 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7856 | `{` |
|      - | 7857 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7858 | `	int nLen;` |
|     19 | 7859 | `	if( nArg < 1 ){` |
|      - | 7860 | `		/* Missing arguments,return FALSE */` |
|      3 | 7861 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7862 | `		return PH7_OK;` |
|      - | 7863 | `	}` |
|      - | 7864 | `	/* Extract the target string */` |
|     17 | 7865 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7866 | `	zEnd = &zIn[nLen];` |
|     17 | 7867 | `	if( nLen < 1 ){` |
|      - | 7868 | `		/* Empty string,return FALSE */` |
|      3 | 7869 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7870 | `		return PH7_OK;` |
|      - | 7871 | `	}` |
|      - | 7872 | `	/* Perform the requested operation */` |
|     27 | 7873 | `	for(;;){` |
|     55 | 7874 | `		if( zIn >= zEnd ){` |
|      - | 7875 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7876 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7877 | `			return PH7_OK;` |
|      - | 7878 | `		}` |
|     51 | 7879 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 7880 | `			break;` |
|      - | 7881 | `		}` |
|      - | 7882 | `		/* Point to the next character */` |
|     41 | 7883 | `		zIn++;` |
|      1 | 7884 | `	}` |
|      - | 7885 | `	/* The test failed,return FALSE */` |
|     11 | 7886 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7887 | `	return PH7_OK;` |
|     10 | 7888 | `}` |
|      - | 7889 | `/*` |
|      - | 7890 | ` * bool ctype_upper(string $text)` |
|      - | 7891 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 7892 | ` * Parameters` |
|      - | 7893 | ` *  $text` |
|      - | 7894 | ` *   The tested string.` |
|      - | 7895 | ` * Return` |
|      - | 7896 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 7897 | ` */` |
|     18 | 7898 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7899 | `{` |
|      - | 7900 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7901 | `	int nLen;` |
|     19 | 7902 | `	if( nArg < 1 ){` |
|      - | 7903 | `		/* Missing arguments,return FALSE */` |
|      3 | 7904 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7905 | `		return PH7_OK;` |
|      - | 7906 | `	}` |
|      - | 7907 | `	/* Extract the target string */` |
|     17 | 7908 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7909 | `	zEnd = &zIn[nLen];` |
|     17 | 7910 | `	if( nLen < 1 ){` |
|      - | 7911 | `		/* Empty string,return FALSE */` |
|      3 | 7912 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7913 | `		return PH7_OK;` |
|      - | 7914 | `	}` |
|      - | 7915 | `	/* Perform the requested operation */` |
|     28 | 7916 | `	for(;;){` |
|     57 | 7917 | `		if( zIn >= zEnd ){` |
|      - | 7918 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7919 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7920 | `			return PH7_OK;` |
|      - | 7921 | `		}` |
|     53 | 7922 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 7923 | `			break;` |
|      - | 7924 | `		}` |
|      - | 7925 | `		/* Point to the next character */` |
|     43 | 7926 | `		zIn++;` |
|      1 | 7927 | `	}` |
|      - | 7928 | `	/* The test failed,return FALSE */` |
|     11 | 7929 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7930 | `	return PH7_OK;` |
|     10 | 7931 | `}` |
|      - | 7932 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 7933 | `/*` |
|      - | 7934 | ` * Section:` |
|      - | 7935 | ` *    URL handling Functions.` |
|      - | 7936 | ` * Status:` |
|      - | 7937 | ` *    Stable.` |
|      - | 7938 | ` */` |
|      - | 7939 | `/*` |
|      - | 7940 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 7941 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 7942 | ` */` |
|   1026 | 7943 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 7944 | `{` |
|      - | 7945 | `	/* Store in the call context result buffer */` |
|   1028 | 7946 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 7947 | `	return SXRET_OK;` |
|      2 | 7948 | `}` |
|      - | 7949 | `/*` |
|      - | 7950 | ` * string base64_encode(string $data)` |
|      - | 7951 | ` * string convert_uuencode(string $data)` |
|      - | 7952 | ` *  Encodes data with MIME base64` |
|      - | 7953 | ` * Parameter` |
|      - | 7954 | ` *  $data` |
|      - | 7955 | ` *    Data to encode` |
|      - | 7956 | ` * Return` |
|      - | 7957 | ` *  Encoded data or FALSE on failure.` |
|      - | 7958 | ` */` |
|     10 | 7959 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7960 | `{` |
|      - | 7961 | `	const char *zIn;` |
|      - | 7962 | `	int nLen;` |
|     11 | 7963 | `	if( nArg < 1 ){` |
|      - | 7964 | `		/* Missing arguments,return FALSE */` |
|      5 | 7965 | `		ph7_result_bool(pCtx,0);` |
|      5 | 7966 | `		return PH7_OK;` |
|      - | 7967 | `	}` |
|      - | 7968 | `	/* Extract the input string */` |
|      7 | 7969 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 7970 | `	if( nLen < 1 ){` |
|      - | 7971 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7972 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7973 | `		return PH7_OK;` |
|      - | 7974 | `	}` |
|      - | 7975 | `	/* Perform the BASE64 encoding */` |
|      7 | 7976 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 7977 | `	return PH7_OK;` |
|      6 | 7978 | `}` |
|      - | 7979 | `/*` |
|      - | 7980 | ` * string base64_decode(string $data)` |
|      - | 7981 | ` * string convert_uudecode(string $data)` |
|      - | 7982 | ` *  Decodes data encoded with MIME base64` |
|      - | 7983 | ` * Parameter` |
|      - | 7984 | ` *  $data` |
|      - | 7985 | ` *    Encoded data.` |
|      - | 7986 | ` * Return` |
|      - | 7987 | ` *  Returns the original data or FALSE on failure.` |
|      - | 7988 | ` */` |
|     36 | 7989 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 7990 | `{` |
|      - | 7991 | `	const char *zIn;` |
|      - | 7992 | `	int nLen;` |
|     38 | 7993 | `	if( nArg < 1 ){` |
|      - | 7994 | `		/* Missing arguments,return FALSE */` |
|      3 | 7995 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7996 | `		return PH7_OK;` |
|      - | 7997 | `	}` |
|      - | 7998 | `	/* Extract the input string */` |
|     36 | 7999 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 8000 | `	if( nLen < 1 ){` |
|      - | 8001 | `		/* Nothing to process,return FALSE */` |
|      3 | 8002 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8003 | `		return PH7_OK;` |
|      - | 8004 | `	}` |
|      - | 8005 | `	/* Perform the BASE64 decoding */` |
|     34 | 8006 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 8007 | `	return PH7_OK;` |
|     20 | 8008 | `}` |
|      - | 8009 | `/*` |
|      - | 8010 | ` * string urlencode(string $str)` |
|      - | 8011 | ` *  URL encoding` |
|      - | 8012 | ` * Parameter` |
|      - | 8013 | ` *  $data` |
|      - | 8014 | ` *   Input string.` |
|      - | 8015 | ` * Return` |
|      - | 8016 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 8017 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 8018 | ` *  encoded as plus (+) signs.` |
|      - | 8019 | ` */` |
|      6 | 8020 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8021 | `{` |
|      - | 8022 | `	const char *zIn;` |
|      - | 8023 | `	int nLen;` |
|      7 | 8024 | `	if( nArg < 1 ){` |
|      - | 8025 | `		/* Missing arguments,return FALSE */` |
|      3 | 8026 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8027 | `		return PH7_OK;` |
|      - | 8028 | `	}` |
|      - | 8029 | `	/* Extract the input string */` |
|      5 | 8030 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 8031 | `	if( nLen < 1 ){` |
|      - | 8032 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8033 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8034 | `		return PH7_OK;` |
|      - | 8035 | `	}` |
|      - | 8036 | `	/* Perform the URL encoding */` |
|      5 | 8037 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 8038 | `	return PH7_OK;` |
|      4 | 8039 | `}` |
|      - | 8040 | `/*` |
|      - | 8041 | ` * string urldecode(string $str)` |
|      - | 8042 | ` *  Decodes any %## encoding in the given string.` |
|      - | 8043 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 8044 | ` * Parameter` |
|      - | 8045 | ` *  $data` |
|      - | 8046 | ` *    Input string.` |
|      - | 8047 | ` * Return` |
|      - | 8048 | ` *  Decoded URL or FALSE on failure.` |
|      - | 8049 | ` */` |
|      8 | 8050 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8051 | `{` |
|      - | 8052 | `	const char *zIn;` |
|      - | 8053 | `	int nLen;` |
|      9 | 8054 | `	if( nArg < 1 ){` |
|      - | 8055 | `		/* Missing arguments,return FALSE */` |
|      3 | 8056 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8057 | `		return PH7_OK;` |
|      - | 8058 | `	}` |
|      - | 8059 | `	/* Extract the input string */` |
|      7 | 8060 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8061 | `	if( nLen < 1 ){` |
|      - | 8062 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8063 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8064 | `		return PH7_OK;` |
|      - | 8065 | `	}` |
|      - | 8066 | `	/* Perform the URL decoding */` |
|      7 | 8067 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 8068 | `	return PH7_OK;` |
|      5 | 8069 | `}` |
|      - | 8070 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8071 | `/* Table of the built-in functions */` |
|      - | 8072 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 8073 | `	   /* Variable handling functions */` |
|      - | 8074 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 8075 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 8076 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 8077 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 8078 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 8079 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 8080 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 8081 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 8082 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 8083 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 8084 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 8085 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 8086 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 8087 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 8088 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 8089 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 8090 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 8091 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 8092 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 8093 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 8094 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8095 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 8096 | `	   /* Math functions */` |
|      - | 8097 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 8098 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 8099 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 8100 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 8101 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 8102 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 8103 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 8104 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 8105 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 8106 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 8107 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 8108 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 8109 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 8110 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 8111 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 8112 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 8113 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 8114 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 8115 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 8116 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 8117 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 8118 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 8119 | `	{ "round",    PH7_builtin_round        },` |
|      - | 8120 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 8121 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 8122 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 8123 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 8124 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 8125 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 8126 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 8127 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 8128 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 8129 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8130 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8131 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 8132 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8133 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8134 | `	   /* String handling functions */` |
|      - | 8135 |  |
|      - | 8136 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 8137 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 8138 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 8139 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 8140 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 8141 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 8142 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 8143 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 8144 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 8145 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 8146 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 8147 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 8148 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 8149 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 8150 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 8151 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 8152 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 8153 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 8154 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 8155 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 8156 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 8157 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 8158 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 8159 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 8160 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 8161 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 8162 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 8163 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 8164 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 8165 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 8166 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 8167 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 8168 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 8169 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 8170 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 8171 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 8172 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 8173 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 8174 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 8175 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 8176 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 8177 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 8178 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 8179 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 8180 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 8181 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 8182 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 8183 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 8184 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - | 8185 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - | 8186 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - | 8187 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 8188 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8189 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8190 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 8191 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 8192 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 8193 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 8194 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8195 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8196 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 8197 |  |
|      - | 8198 |  |
|      - | 8199 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 8200 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 8201 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 8202 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 8203 | `	{ "hash",         PH7_builtin_hash      },` |
|      - | 8204 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|      - | 8205 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|      - | 8206 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|      - | 8207 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 8208 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|      - | 8209 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|      - | 8210 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|      - | 8211 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|      - | 8212 | `	{ "filter_var",            PH7_builtin_filter_var },` |
|      - | 8213 | `	{ "filter_input",          PH7_builtin_filter_input },` |
|      - | 8214 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8215 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8216 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 8217 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 8218 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8219 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8220 |  |
|      - | 8221 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 8222 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 8223 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 8224 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 8225 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 8226 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 8227 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 8228 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 8229 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 8230 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 8231 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 8232 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 8233 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8234 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8235 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 8236 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8237 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8238 |  |
|      - | 8239 | `	         /* Ctype functions */` |
|      - | 8240 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 8241 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 8242 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 8243 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 8244 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 8245 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 8246 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 8247 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 8248 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 8249 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 8250 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 8251 | `	         /* Time functions */` |
|      - | 8252 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 8253 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 8254 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 8255 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 8256 | `	{ "date",        PH7_builtin_date         },` |
|      - | 8257 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 8258 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 8259 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 8260 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 8261 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 8262 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 8263 | `	        /* URL functions */` |
|      - | 8264 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 8265 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 8266 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 8267 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 8268 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 8269 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 8270 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 8271 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 8272 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8273 | `};` |
|      - | 8274 | `/*` |
|      - | 8275 | ` * Register the built-in functions defined above,the array functions` |
|      - | 8276 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 8277 | ` */` |
|   3310 | 8278 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      5 | 8279 | `{` |
|      - | 8280 | `	sxu32 n;` |
| 556085 | 8281 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 552775 | 8282 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 276390 | 8283 | `	}` |
|      - | 8284 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   3315 | 8285 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 8286 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   3315 | 8287 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   3315 | 8288 | `}` |
|      - | 8289 |  |
