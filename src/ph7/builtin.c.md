# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3848/4323 lines (89.01%)

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
|      3 |   69 | `{` |
|    635 |   70 | `	int res = 0; /* Assume false by default */` |
|    635 |   71 | `	if( nArg > 0 ){` |
|      - |   72 | `		/* Strict PHP identity: a float is never an int, even when it holds an` |
|      - |   73 | `		 * integer value (1.0). An integer-valued real carries both MEMOBJ_INT` |
|      - |   74 | `		 * (cached) and MEMOBJ_REAL, so REAL must be excluded here. */` |
|    633 |   75 | `		res = ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]);` |
|    315 |   76 | `	}` |
|      - |   77 | `	/* Query result */` |
|    635 |   78 | `	ph7_result_bool(pCtx,res);` |
|    635 |   79 | `	return PH7_OK;` |
|      3 |   80 | `}` |
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
|      2 |  108 | `{` |
|     94 |  109 | `	int res = 0; /* Assume false by default */` |
|     94 |  110 | `	if( nArg > 0 ){` |
|     92 |  111 | `		res = ph7_value_is_null(apArg[0]);` |
|     45 |  112 | `	}` |
|      - |  113 | `	/* Query result */` |
|     94 |  114 | `	ph7_result_bool(pCtx,res);` |
|     94 |  115 | `	return PH7_OK;` |
|      2 |  116 | `}` |
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
|  33896 |  301 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  302 | `{` |
|  33901 |  303 | `	int res = 1; /* Assume empty by default */` |
|  33901 |  304 | `	if( nArg > 0 ){` |
|  33899 |  305 | `		res = ph7_value_is_empty(apArg[0]);` |
|  16947 |  306 | `	}` |
|  33901 |  307 | `	ph7_result_bool(pCtx,res);` |
|  33901 |  308 | `	return PH7_OK;` |
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
| 212972 |  351 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  352 | `{` |
|      - |  353 | `	const char *zSource,*zOfft;` |
|      - |  354 | `	int nOfft,nLen,nSrcLen;` |
| 212977 |  355 | `	if( nArg < 2 ){` |
|      - |  356 | `		/* return FALSE */` |
|      5 |  357 | `		ph7_result_bool(pCtx,0);` |
|      5 |  358 | `		return PH7_OK;` |
|      - |  359 | `	}` |
|      - |  360 | `	/* Extract the target string */` |
| 212973 |  361 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 212973 |  362 | `	if( nSrcLen < 1 ){` |
|      - |  363 | `		/* Empty string,return FALSE */` |
|  11839 |  364 | `		ph7_result_bool(pCtx,0);` |
|  11839 |  365 | `		return PH7_OK;` |
|      - |  366 | `	}` |
| 201139 |  367 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  368 | `	/* Extract the offset */` |
| 201139 |  369 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 201139 |  370 | `	if( nOfft < 0 ){` |
|  32373 |  371 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  32373 |  372 | `		if( zOfft < zSource ){` |
|      - |  373 | `			/* Invalid offset */` |
|      5 |  374 | `			ph7_result_bool(pCtx,0);` |
|      5 |  375 | `			return PH7_OK;` |
|      - |  376 | `		}` |
|  32369 |  377 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  32369 |  378 | `		nOfft = (int)(zOfft-zSource);` |
| 184953 |  379 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  380 | `		/* Invalid offset */` |
|    195 |  381 | `		ph7_result_bool(pCtx,0);` |
|    195 |  382 | `		return PH7_OK;` |
|    ! 0 |  383 | `	}else{` |
| 168581 |  384 | `		zOfft = &zSource[nOfft];` |
| 168581 |  385 | `		nLen = nSrcLen - nOfft;` |
|      - |  386 | `	}` |
| 200945 |  387 | `	if( nArg > 2 ){` |
|      - |  388 | `		/* Extract the length */` |
| 165267 |  389 | `		nLen = ph7_value_to_int(apArg[2]);` |
| 165267 |  390 | `		if( nLen == 0 ){` |
|      - |  391 | `			/* Invalid length,return an empty string */` |
|      5 |  392 | `			ph7_result_string(pCtx,"",0);` |
|      5 |  393 | `			return PH7_OK;` |
| 165263 |  394 | `		}else if( nLen < 0 ){` |
|  32361 |  395 | `			nLen = nSrcLen + nLen - nOfft;` |
|  32361 |  396 | `			if( nLen < 1 ){` |
|      - |  397 | `				/* Invalid  length */` |
|      3 |  398 | `				nLen = nSrcLen - nOfft;` |
|      1 |  399 | `			}` |
|  16178 |  400 | `		}` |
| 165263 |  401 | `		if( nLen + nOfft > nSrcLen ){` |
|      - |  402 | `			/* Invalid length */` |
|   4987 |  403 | `			nLen = nSrcLen - nOfft;` |
|   2491 |  404 | `		}` |
|  82629 |  405 | `	}` |
|      - |  406 | `	/* Return the substring */` |
| 200941 |  407 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 200941 |  408 | `	return PH7_OK;` |
| 106491 |  409 | `}` |
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
|     38 |  680 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
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
|     68 |  817 | `	} else if( ph7_value_is_array(apArg[0]) \|\|` |
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
|     64 |  835 | `	} else if( ph7_value_is_array(apArg[1]) \|\|` |
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
|      - | 1006 | ` * UTF-8-aware HTML entity machinery, shared by htmlspecialchars/htmlentities/` |
|      - | 1007 | ` * htmlspecialchars_decode/html_entity_decode/get_html_translation_table.` |
|      - | 1008 | ` * The implementations live further down in this file, next to the filter_var` |
|      - | 1009 | ` * FULL_SPECIAL_CHARS machinery they reuse (aHtml401Ent[]/FvHtml401Lookup()/` |
|      - | 1010 | ` * FvUtf8Next()). Semantics are byte-exact vs php 8.5.7; PHL is UTF-8-only` |
|      - | 1011 | ` * (PLAN.md §6) so every charset argument other than a UTF-8 alias gets PHP's` |
|      - | 1012 | ` * unsupported-charset warning and is treated as UTF-8.` |
|      - | 1013 | ` *` |
|      - | 1014 | ` * Flag model (the PHP-exact ENT_* values, see constant.c): bit 1 = encode/` |
|      - | 1015 | ` * decode single quotes, bit 2 = double quotes (ENT_QUOTES=3, ENT_COMPAT=2,` |
|      - | 1016 | ` * ENT_NOQUOTES=0); bits 16\|32 select the doctype (0=HTML401, 16=XML1,` |
|      - | 1017 | ` * 32=XHTML, 48=HTML5); ENT_IGNORE=4 drops invalid UTF-8 bytes (wins over` |
|      - | 1018 | ` * ENT_SUBSTITUTE=8, which replaces each with U+FFFD; with neither set the` |
|      - | 1019 | ` * whole result collapses to ""); ENT_DISALLOWED=128 substitutes valid but` |
|      - | 1020 | ` * doctype-disallowed codepoints. The shared default is` |
|      - | 1021 | ` * ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 = 11.` |
|      - | 1022 | ` */` |
|      - | 1023 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bAll,int bDoubleEncode);` |
|      - | 1024 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bFull);` |
|      - | 1025 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx);` |
|      - | 1026 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags);` |
|      - | 1027 | `/*` |
|      - | 1028 | ` * string htmlspecialchars(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1029 | ` *                         [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1030 | ` *  Convert the special characters & < > " ' to HTML entities.` |
|      - | 1031 | ` * Return` |
|      - | 1032 | ` *  The escaped string or NULL on failure.` |
|      - | 1033 | ` */` |
|     46 | 1034 | `static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1035 | `{` |
|     47 | 1036 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1037 | `	const char *zIn;` |
|     47 | 1038 | `	int nLen,bDouble = 1;` |
|     47 | 1039 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1040 | `		/* Missing/Invalid arguments,return NULL */` |
|      7 | 1041 | `		ph7_result_null(pCtx);` |
|      7 | 1042 | `		return PH7_OK;` |
|      - | 1043 | `	}` |
|      - | 1044 | `	/* Extract the target string */` |
|     41 | 1045 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 1046 | `	if( nArg > 1 ){` |
|     35 | 1047 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     17 | 1048 | `	}` |
|     41 | 1049 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     41 | 1050 | `	if( nArg > 3 ){` |
|      7 | 1051 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      3 | 1052 | `	}` |
|     41 | 1053 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,0,bDouble);` |
|     41 | 1054 | `	return PH7_OK;` |
|     24 | 1055 | `}` |
|      - | 1056 | `/*` |
|      - | 1057 | ` * string htmlspecialchars_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401])` |
|      - | 1058 | ` *  Convert the special HTML entities (&amp; &lt; &gt; &quot; and the` |
|      - | 1059 | ` *  numeric/doctype forms of the two quotes) back to characters.` |
|      - | 1060 | ` * Return` |
|      - | 1061 | ` *  The unescaped string or NULL on failure.` |
|      - | 1062 | ` */` |
|     24 | 1063 | `static int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1064 | `{` |
|     25 | 1065 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1066 | `	const char *zIn;` |
|      - | 1067 | `	int nLen;` |
|     25 | 1068 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1069 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 1070 | `		ph7_result_null(pCtx);` |
|      5 | 1071 | `		return PH7_OK;` |
|      - | 1072 | `	}` |
|      - | 1073 | `	/* Extract the target string */` |
|     21 | 1074 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     21 | 1075 | `	if( nArg > 1 ){` |
|      9 | 1076 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1077 | `	}` |
|     21 | 1078 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,0);` |
|     21 | 1079 | `	return PH7_OK;` |
|     13 | 1080 | `}` |
|      - | 1081 | `/*` |
|      - | 1082 | ` * array get_html_translation_table(int $table = HTML_SPECIALCHARS` |
|      - | 1083 | ` *      [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 [, string $encoding = "UTF-8"]])` |
|      - | 1084 | ` *  Return the translation table used by htmlspecialchars() (HTML_SPECIALCHARS)` |
|      - | 1085 | ` *  or htmlentities() (HTML_ENTITIES) as character => entity pairs.` |
|      - | 1086 | ` * Return` |
|      - | 1087 | ` *  The translation table as an array or NULL on failure.` |
|      - | 1088 | ` */` |
|     12 | 1089 | `static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1090 | `{` |
|     13 | 1091 | `	int iTable = 0; /* HTML_SPECIALCHARS */` |
|     13 | 1092 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|     13 | 1093 | `	if( nArg > 0 ){` |
|     11 | 1094 | `		iTable = ph7_value_to_int(apArg[0]);` |
|      5 | 1095 | `	}` |
|     13 | 1096 | `	if( nArg > 1 ){` |
|      9 | 1097 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1098 | `	}` |
|     13 | 1099 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     13 | 1100 | `	HtmlTranslationTable(pCtx,iTable,iFlags);` |
|     13 | 1101 | `	return PH7_OK;` |
|      1 | 1102 | `}` |
|      - | 1103 | `/*` |
|      - | 1104 | ` * string htmlentities(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1105 | ` *                     [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1106 | ` *  Convert all applicable characters to HTML entities: the specials plus` |
|      - | 1107 | ` *  every codepoint with an HTML 4.01 named entity (aHtml401Ent[]).` |
|      - | 1108 | ` * Return` |
|      - | 1109 | ` *  The encoded string or NULL on failure.` |
|      - | 1110 | ` */` |
|     34 | 1111 | `static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1112 | `{` |
|     35 | 1113 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1114 | `	const char *zIn;` |
|     35 | 1115 | `	int nLen,bDouble = 1;` |
|     35 | 1116 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1117 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 1118 | `		ph7_result_null(pCtx);` |
|      5 | 1119 | `		return PH7_OK;` |
|      - | 1120 | `	}` |
|      - | 1121 | `	/* Extract the target string */` |
|     31 | 1122 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 1123 | `	if( nArg > 1 ){` |
|     19 | 1124 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 | 1125 | `	}` |
|     31 | 1126 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     31 | 1127 | `	if( nArg > 3 ){` |
|      3 | 1128 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      1 | 1129 | `	}` |
|     31 | 1130 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,1,bDouble);` |
|     31 | 1131 | `	return PH7_OK;` |
|     18 | 1132 | `}` |
|      - | 1133 | `/*` |
|      - | 1134 | ` * string html_entity_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1135 | ` *                           [, string $encoding = "UTF-8"]])` |
|      - | 1136 | ` *  Convert HTML entities (named — case-sensitive — and numeric, decimal or` |
|      - | 1137 | ` *  hex) back to their UTF-8 characters. The reverse of htmlentities().` |
|      - | 1138 | ` * Return` |
|      - | 1139 | ` *  The decoded string or NULL on failure.` |
|      - | 1140 | ` */` |
|     62 | 1141 | `static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1142 | `{` |
|     63 | 1143 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1144 | `	const char *zIn;` |
|      - | 1145 | `	int nLen;` |
|     63 | 1146 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1147 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 1148 | `		ph7_result_null(pCtx);` |
|      5 | 1149 | `		return PH7_OK;` |
|      - | 1150 | `	}` |
|      - | 1151 | `	/* Extract the target string */` |
|     59 | 1152 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     59 | 1153 | `	if( nArg > 1 ){` |
|     27 | 1154 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     13 | 1155 | `	}` |
|     59 | 1156 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     59 | 1157 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,1);` |
|     59 | 1158 | `	return PH7_OK;` |
|     32 | 1159 | `}` |
|      - | 1160 | `/*` |
|      - | 1161 | ` * int strlen($string)` |
|      - | 1162 | ` *  return the length of the given string.` |
|      - | 1163 | ` * Parameter` |
|      - | 1164 | ` *  string: The string being measured for length.` |
|      - | 1165 | ` * Return` |
|      - | 1166 | ` *  length of the given string.` |
|      - | 1167 | ` */` |
|   9642 | 1168 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1169 | `{` |
|   9647 | 1170 | `	int iLen = 0;` |
|   9647 | 1171 | `	if( nArg > 0 ){` |
|   9645 | 1172 | `		ph7_value_to_string(apArg[0],&iLen);` |
|   4820 | 1173 | `	}` |
|      - | 1174 | `	/* String length */` |
|   9647 | 1175 | `	ph7_result_int(pCtx,iLen);` |
|   9647 | 1176 | `	return PH7_OK;` |
|      5 | 1177 | `}` |
|      - | 1178 | `/*` |
|      - | 1179 | ` * int strcmp(string $str1,string $str2)` |
|      - | 1180 | ` *  Perform a binary safe string comparison.` |
|      - | 1181 | ` * Parameter` |
|      - | 1182 | ` *  str1: The first string` |
|      - | 1183 | ` *  str2: The second string` |
|      - | 1184 | ` * Return` |
|      - | 1185 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1186 | ` *  than str2, and 0 if they are equal.` |
|      - | 1187 | ` */` |
|     80 | 1188 | `static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1189 | `{` |
|      - | 1190 | `	const char *z1,*z2;` |
|      - | 1191 | `	int n1,n2;` |
|      - | 1192 | `	int res;` |
|     81 | 1193 | `	if( nArg < 2 ){` |
|      5 | 1194 | `		res = nArg == 0 ? 0 : 1;` |
|      5 | 1195 | `		ph7_result_int(pCtx,res);` |
|      5 | 1196 | `		return PH7_OK;` |
|      - | 1197 | `	}` |
|      - | 1198 | `	/* Perform the comparison */` |
|     77 | 1199 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     77 | 1200 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     77 | 1201 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1202 | `	/* Comparison result */` |
|     77 | 1203 | `	ph7_result_int(pCtx,res);` |
|     77 | 1204 | `	return PH7_OK;` |
|     41 | 1205 | `}` |
|      - | 1206 | `/*` |
|      - | 1207 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 1208 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 1209 | ` * Parameter` |
|      - | 1210 | ` *  str1: The first string` |
|      - | 1211 | ` *  str2: The second string` |
|      - | 1212 | ` * Return` |
|      - | 1213 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1214 | ` *  than str2, and 0 if they are equal.` |
|      - | 1215 | ` */` |
|     20 | 1216 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1217 | `{` |
|      - | 1218 | `	const char *z1,*z2;` |
|      - | 1219 | `	int res;` |
|      - | 1220 | `	int n;` |
|     21 | 1221 | `	if( nArg < 3 ){` |
|      - | 1222 | `		/* Perform a standard comparison */` |
|      5 | 1223 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 1224 | `	}` |
|      - | 1225 | `	/* Desired comparison length */` |
|     17 | 1226 | `	n  = ph7_value_to_int(apArg[2]);` |
|     17 | 1227 | `	if( n < 0 ){` |
|      - | 1228 | `		/* Invalid length */` |
|      3 | 1229 | `		ph7_result_int(pCtx,-1);` |
|      3 | 1230 | `		return PH7_OK;` |
|      - | 1231 | `	}` |
|      - | 1232 | `	/* Perform the comparison */` |
|     15 | 1233 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     15 | 1234 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     15 | 1235 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 1236 | `	/* Comparison result */` |
|     15 | 1237 | `	ph7_result_int(pCtx,res);` |
|     15 | 1238 | `	return PH7_OK;` |
|     11 | 1239 | `}` |
|      - | 1240 | `/*` |
|      - | 1241 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 1242 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 1243 | ` * Parameter` |
|      - | 1244 | ` *  str1: The first string` |
|      - | 1245 | ` *  str2: The second string` |
|      - | 1246 | ` * Return` |
|      - | 1247 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1248 | ` *  than str2, and 0 if they are equal.` |
|      - | 1249 | ` */` |
|     22 | 1250 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1251 | `{` |
|      - | 1252 | `	const char *z1,*z2;` |
|      - | 1253 | `	int n1,n2;` |
|      - | 1254 | `	int res;` |
|     23 | 1255 | `	if( nArg < 2 ){` |
|      9 | 1256 | `		res = nArg == 0 ? 0 : 1;` |
|      9 | 1257 | `		ph7_result_int(pCtx,res);` |
|      9 | 1258 | `		return PH7_OK;` |
|      - | 1259 | `	}` |
|      - | 1260 | `	/* Perform the comparison */` |
|     15 | 1261 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     15 | 1262 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     15 | 1263 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1264 | `	/* Comparison result */` |
|     15 | 1265 | `	ph7_result_int(pCtx,res);` |
|     15 | 1266 | `	return PH7_OK;` |
|     12 | 1267 | `}` |
|      - | 1268 | `/*` |
|      - | 1269 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 1270 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 1271 | ` * Parameter` |
|      - | 1272 | ` *  $str1: The first string` |
|      - | 1273 | ` *  $str2: The second string` |
|      - | 1274 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 1275 | ` * Return` |
|      - | 1276 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1277 | ` *  than str2, and 0 if they are equal.` |
|      - | 1278 | ` */` |
|      8 | 1279 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1280 | `{` |
|      - | 1281 | `	const char *z1,*z2;` |
|      - | 1282 | `	int res;` |
|      - | 1283 | `	int n;` |
|      9 | 1284 | `	if( nArg < 3 ){` |
|      - | 1285 | `		/* Perform a standard comparison */` |
|      5 | 1286 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 1287 | `	}` |
|      - | 1288 | `	/* Desired comparison length */` |
|      5 | 1289 | `	n  = ph7_value_to_int(apArg[2]);` |
|      5 | 1290 | `	if( n < 0 ){` |
|      - | 1291 | `		/* Invalid length */` |
|    ! 0 | 1292 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 1293 | `		return PH7_OK;` |
|      - | 1294 | `	}` |
|      - | 1295 | `	/* Perform the comparison */` |
|      5 | 1296 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|      5 | 1297 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|      5 | 1298 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 1299 | `	/* Comparison result */` |
|      5 | 1300 | `	ph7_result_int(pCtx,res);` |
|      5 | 1301 | `	return PH7_OK;` |
|      5 | 1302 | `}` |
|      - | 1303 | `/*` |
|      - | 1304 | ` * Implode context [i.e: it's private data].` |
|      - | 1305 | ` * A pointer to the following structure is forwarded` |
|      - | 1306 | ` * verbatim to the array walker callback defined below.` |
|      - | 1307 | ` */` |
|      - | 1308 | `struct implode_data {` |
|      - | 1309 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 1310 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 1311 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 1312 | `	int nSeplen;          /* Separator length */` |
|      - | 1313 | `	int bFirst;           /* TRUE if first call */` |
|      - | 1314 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 1315 | `	sxi32 rc;             /* Captured allocation rc; SXERR_MEM => the builtin raises an OOM fatal */` |
|      - | 1316 | `};` |
|      - | 1317 | `/*` |
|      - | 1318 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 1319 | ` * The following routine is invoked for each array entry passed` |
|      - | 1320 | ` * to the implode() function.` |
|      - | 1321 | ` */` |
| 133304 | 1322 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 1323 | `{` |
|  66652 | 1324 | `	SXUNUSED(pKey);` |
| 133309 | 1325 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 1326 | `	const char *zData;` |
|      - | 1327 | `	int nLen;` |
| 133309 | 1328 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 1329 | `		if( pData->nSeplen > 0 ){` |
|      3 | 1330 | `			if( !pData->bFirst ){` |
|      - | 1331 | `				/* append the separator first */` |
|      3 | 1332 | `				if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 1333 | `					pData->rc = SXERR_MEM;` |
|    ! 0 | 1334 | `					return PH7_ABORT;` |
|      - | 1335 | `				}` |
|      2 | 1336 | `			}else{` |
|    ! 0 | 1337 | `				pData->bFirst = 0;` |
|      - | 1338 | `			}` |
|      1 | 1339 | `		}` |
|      - | 1340 | `		/* Recurse */` |
|      3 | 1341 | `		pData->bFirst = 1;` |
|      3 | 1342 | `		pData->nRecCount++;` |
|      3 | 1343 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 1344 | `		pData->nRecCount--;` |
|      - | 1345 | `		/* Propagate an allocation failure surfaced deeper in the recursion. */` |
|      3 | 1346 | `		if( pData->rc != SXRET_OK ){` |
|    ! 0 | 1347 | `			return PH7_ABORT;` |
|      - | 1348 | `		}` |
|      3 | 1349 | `		return PH7_OK;` |
|      - | 1350 | `	}` |
|      - | 1351 | `	/* Extract the string representation of the entry value */` |
| 133307 | 1352 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 1353 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 133307 | 1354 | `	if( pData->bFirst ){` |
|  32707 | 1355 | `		pData->bFirst = 0;` |
| 116956 | 1356 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 1357 | `		/* append the separator first */` |
| 100593 | 1358 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 1359 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1360 | `			return PH7_ABORT;` |
|      - | 1361 | `		}` |
|  50294 | 1362 | `	}` |
|      - | 1363 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 133307 | 1364 | `	if( nLen > 0 ){` |
| 121473 | 1365 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1366 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1367 | `			return PH7_ABORT;` |
|      - | 1368 | `		}` |
|  60734 | 1369 | `	}` |
| 133307 | 1370 | `	return PH7_OK;` |
|  66657 | 1371 | `}` |
|      - | 1372 | `/*` |
|      - | 1373 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 1374 | ` * string implode(array $pieces,...)` |
|      - | 1375 | ` *  Join array elements with a string.` |
|      - | 1376 | ` * $glue` |
|      - | 1377 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 1378 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 1379 | ` * $pieces` |
|      - | 1380 | ` *   The array of strings to implode.` |
|      - | 1381 | ` * Return` |
|      - | 1382 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 1383 | ` *  order, with the glue string between each element.` |
|      - | 1384 | ` */` |
|  32726 | 1385 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1386 | `{` |
|      - | 1387 | `	struct implode_data imp_data;` |
|  32731 | 1388 | `	int i = 1;` |
|  32731 | 1389 | `	if( nArg < 1 ){` |
|      - | 1390 | `		/* Missing argument,return NULL */` |
|    ! 0 | 1391 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1392 | `		return PH7_OK;` |
|      - | 1393 | `	}` |
|      - | 1394 | `	/* Prepare the implode context */` |
|  32731 | 1395 | `	imp_data.pCtx = pCtx;` |
|  32731 | 1396 | `	imp_data.bRecursive = 0;` |
|  32731 | 1397 | `	imp_data.bFirst = 1;` |
|  32731 | 1398 | `	imp_data.nRecCount = 0;` |
|  32731 | 1399 | `	imp_data.rc = SXRET_OK;` |
|  32731 | 1400 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  32729 | 1401 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  16367 | 1402 | `	}else{` |
|      3 | 1403 | `		imp_data.zSep = 0;` |
|      3 | 1404 | `		imp_data.nSeplen = 0;` |
|      3 | 1405 | `		i = 0;` |
|      - | 1406 | `	}` |
|  32731 | 1407 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 1408 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1409 | `	}` |
|      - | 1410 | `	/* Start the 'join' process */` |
|  65457 | 1411 | `	while( i < nArg ){` |
|  32731 | 1412 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1413 | `			/* Iterate throw array entries */` |
|  32731 | 1414 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 1415 | `			/* Surface a callback allocation failure as a fatal */` |
|  32731 | 1416 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 1417 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1418 | `			}` |
|  16368 | 1419 | `		}else{` |
|      - | 1420 | `			const char *zData;` |
|      - | 1421 | `			int nLen;` |
|      - | 1422 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 1423 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1424 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 1425 | `			if( imp_data.bFirst ){` |
|    ! 0 | 1426 | `				imp_data.bFirst = 0;` |
|    ! 0 | 1427 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1428 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 1429 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1430 | `				}` |
|    ! 0 | 1431 | `			}` |
|      - | 1432 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 1433 | `			if( nLen > 0 ){` |
|    ! 0 | 1434 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1435 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1436 | `				}` |
|    ! 0 | 1437 | `			}` |
|      - | 1438 | `		}` |
|  32731 | 1439 | `		i++;` |
|      5 | 1440 | `	}` |
|  32731 | 1441 | `	return PH7_OK;` |
|  16368 | 1442 | `}` |
|      - | 1443 | `/*` |
|      - | 1444 | ` * Symisc eXtension:` |
|      - | 1445 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 1446 | ` * Purpose` |
|      - | 1447 | ` *  Same as implode() but recurse on arrays.` |
|      - | 1448 | ` * Example:` |
|      - | 1449 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 1450 | ` *   echo implode_recursive("/",$a);` |
|      - | 1451 | ` *   Will output` |
|      - | 1452 | ` *     usr/home/dean.` |
|      - | 1453 | ` *   While the standard implode would produce.` |
|      - | 1454 | ` *    usr/Array.` |
|      - | 1455 | ` * Parameter` |
|      - | 1456 | ` *  Refer to implode().` |
|      - | 1457 | ` * Return` |
|      - | 1458 | ` *  Refer to implode().` |
|      - | 1459 | ` */` |
|     12 | 1460 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1461 | `{` |
|      - | 1462 | `	struct implode_data imp_data;` |
|     13 | 1463 | `	int i = 1;` |
|     13 | 1464 | `	if( nArg < 1 ){` |
|      - | 1465 | `		/* Missing argument,return NULL */` |
|      3 | 1466 | `		ph7_result_null(pCtx);` |
|      3 | 1467 | `		return PH7_OK;` |
|      - | 1468 | `	}` |
|      - | 1469 | `	/* Prepare the implode context */` |
|     11 | 1470 | `	imp_data.pCtx = pCtx;` |
|     11 | 1471 | `	imp_data.bRecursive = 1;` |
|     11 | 1472 | `	imp_data.bFirst = 1;` |
|     11 | 1473 | `	imp_data.nRecCount = 0;` |
|     11 | 1474 | `	imp_data.rc = SXRET_OK;` |
|     11 | 1475 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 1476 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 1477 | `	}else{` |
|    ! 0 | 1478 | `		imp_data.zSep = 0;` |
|    ! 0 | 1479 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 1480 | `		i = 0;` |
|      - | 1481 | `	}` |
|     11 | 1482 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 1483 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1484 | `	}` |
|      - | 1485 | `	/* Start the 'join' process */` |
|     21 | 1486 | `	while( i < nArg ){` |
|     11 | 1487 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1488 | `			/* Iterate throw array entries */` |
|      3 | 1489 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 1490 | `			/* Surface a callback allocation failure as a fatal */` |
|      3 | 1491 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 1492 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1493 | `			}` |
|      2 | 1494 | `		}else{` |
|      - | 1495 | `			const char *zData;` |
|      - | 1496 | `			int nLen;` |
|      - | 1497 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 1498 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1499 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 1500 | `			if( imp_data.bFirst ){` |
|      9 | 1501 | `				imp_data.bFirst = 0;` |
|      4 | 1502 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1503 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 1504 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1505 | `				}` |
|    ! 0 | 1506 | `			}` |
|      - | 1507 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 1508 | `			if( nLen > 0 ){` |
|      9 | 1509 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1510 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1511 | `				}` |
|      4 | 1512 | `			}` |
|      - | 1513 | `		}` |
|     11 | 1514 | `		i++;` |
|      1 | 1515 | `	}` |
|     11 | 1516 | `	return PH7_OK;` |
|      7 | 1517 | `}` |
|      - | 1518 | `/*` |
|      - | 1519 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 1520 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 1521 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 1522 | ` * Parameters` |
|      - | 1523 | ` *  $delimiter` |
|      - | 1524 | ` *   The boundary string.` |
|      - | 1525 | ` * $string` |
|      - | 1526 | ` *   The input string.` |
|      - | 1527 | ` * $limit` |
|      - | 1528 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 1529 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 1530 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 1531 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 1532 | ` * Returns` |
|      - | 1533 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 1534 | ` *  on boundaries formed by the delimiter.` |
|      - | 1535 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 1536 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 1537 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 1538 | ` *  will be returned.` |
|      - | 1539 | ` * NOTE:` |
|      - | 1540 | ` *  Negative limit is not supported.` |
|      - | 1541 | ` */` |
|   6230 | 1542 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1543 | `{` |
|      - | 1544 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 1545 | `	int nDelim,nStrlen,iLimit;` |
|      - | 1546 | `	ph7_value *pArray;` |
|      - | 1547 | `	ph7_value *pValue;` |
|      - | 1548 | `	sxu32 nOfft;` |
|      - | 1549 | `	sxi32 rc;` |
|   6235 | 1550 | `	if( nArg < 2 ){` |
|      - | 1551 | `		/* Missing arguments,return FALSE */` |
|      9 | 1552 | `		ph7_result_bool(pCtx,0);` |
|      9 | 1553 | `		return PH7_OK;` |
|      - | 1554 | `	}` |
|      - | 1555 | `	/* Extract the delimiter */` |
|   6227 | 1556 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   6227 | 1557 | `	if( nDelim < 1 ){` |
|      - | 1558 | `		/* Empty delimiter,return FALSE */` |
|      3 | 1559 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1560 | `		return PH7_OK;` |
|      - | 1561 | `	}` |
|      - | 1562 | `	/* Extract the string */` |
|   6225 | 1563 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   6225 | 1564 | `	if( nStrlen < 1 ){` |
|      - | 1565 | `		/* Empty string: normally an array with a single empty element (PHP behavior).` |
|      - | 1566 | `		 * A negative limit drops the last -limit components, so the sole empty` |
|      - | 1567 | `		 * component is dropped and the result is an empty array. */` |
|      7 | 1568 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      7 | 1569 | `		if( pArrayTmp == 0 ){` |
|      - | 1570 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 1571 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 1572 | `			return PH7_OK;` |
|      - | 1573 | `		}` |
|      7 | 1574 | `		if( !(nArg > 2 && ph7_value_to_int(apArg[2]) < 0) ){` |
|      5 | 1575 | `			ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      5 | 1576 | `			if( pValueTmp == 0 ){` |
|      - | 1577 | `				/* Out of memory,return FALSE */` |
|    ! 0 | 1578 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 1579 | `				return PH7_OK;` |
|      - | 1580 | `			}` |
|      5 | 1581 | `			ph7_value_string(pValueTmp, "", 0);` |
|      5 | 1582 | `			if( ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp) != SXRET_OK ){` |
|    ! 0 | 1583 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1584 | `			}` |
|      2 | 1585 | `		}` |
|      7 | 1586 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      7 | 1587 | `		return PH7_OK;` |
|      - | 1588 | `	}` |
|      - | 1589 | `	/* Point to the end of the string */` |
|   6219 | 1590 | `	zEnd = &zString[nStrlen];` |
|      - | 1591 | `	/* Create the array */` |
|   6219 | 1592 | `	pArray =  ph7_context_new_array(pCtx);` |
|   6219 | 1593 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   6219 | 1594 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 1595 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 1596 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1597 | `		return PH7_OK;` |
|      - | 1598 | `	}` |
|      - | 1599 | `	/* Set a defualt limit */` |
|   6219 | 1600 | `	iLimit = SXI32_HIGH;` |
|   6219 | 1601 | `	if( nArg > 2 ){` |
|     37 | 1602 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     37 | 1603 | `		if( iLimit < 0 ){` |
|      - | 1604 | `			/* Negative limit: keep all components except the last -iLimit (PHP).` |
|      - | 1605 | `			 * Pre-count the components (delimiters + 1), then emit only the first` |
|      - | 1606 | `			 * nKeep CLEAN components — no trailing-remainder merge (the difference` |
|      - | 1607 | `			 * from the positive path). nKeep <= 0 drops everything -> empty array. */` |
|     17 | 1608 | `			int nTotal = 1,nKeep;` |
|     17 | 1609 | `			const char *zScan = zString;` |
|      - | 1610 | `			sxu32 nScanOfft;` |
|     57 | 1611 | `			while( SyBlobSearch(zScan,(sxu32)(zEnd - zScan),zDelim,nDelim,&nScanOfft) == SXRET_OK ){` |
|     41 | 1612 | `				nTotal++;` |
|     41 | 1613 | `				zScan = &zScan[nScanOfft + nDelim];` |
|      1 | 1614 | `			}` |
|     17 | 1615 | `			nKeep = nTotal + iLimit; /* iLimit < 0, so this is nTotal - (-iLimit) */` |
|     49 | 1616 | `			while( nKeep > (int)ph7_array_count(pArray)` |
|     39 | 1617 | `				&& SyBlobSearch(zString,(sxu32)(zEnd - zString),zDelim,nDelim,&nOfft) == SXRET_OK ){` |
|      - | 1618 | `				/* Emit the next clean component */` |
|     23 | 1619 | `				zCur = &zString[nOfft];` |
|     23 | 1620 | `				ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|     23 | 1621 | `				if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 1622 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1623 | `				}` |
|     23 | 1624 | `				zString = &zCur[nDelim];` |
|     23 | 1625 | `				ph7_value_reset_string_cursor(pValue);` |
|      1 | 1626 | `			}` |
|     17 | 1627 | `			ph7_result_value(pCtx,pArray);` |
|     17 | 1628 | `			return PH7_OK;` |
|      - | 1629 | `		}` |
|     21 | 1630 | `		if( iLimit == 0 ){` |
|      5 | 1631 | `			iLimit = 1;` |
|      2 | 1632 | `		}` |
|     21 | 1633 | `		iLimit--;` |
|      9 | 1634 | `	}` |
|      - | 1635 | `	/* Start exploding */` |
|  72400 | 1636 | `	for(;;){` |
| 144805 | 1637 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 144805 | 1638 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 1639 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   6203 | 1640 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   6203 | 1641 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 1642 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1643 | `			}` |
|   6203 | 1644 | `			break;` |
|      - | 1645 | `		}` |
|      - | 1646 | `		/* Point to the desired offset */` |
| 138607 | 1647 | `		zCur = &zString[nOfft];` |
|      - | 1648 | `		/* Perform the store operation (may be empty) */` |
| 138607 | 1649 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 138607 | 1650 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 1651 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 1652 | `		}` |
|      - | 1653 | `		/* Point beyond the delimiter */` |
| 138607 | 1654 | `		zString = &zCur[nDelim];` |
|      - | 1655 | `		/* Reset the cursor */` |
| 138607 | 1656 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 1657 | `	}` |
|      - | 1658 | `	/* Return the freshly created array */` |
|   6203 | 1659 | `	ph7_result_value(pCtx,pArray);` |
|      - | 1660 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 1661 | `	 * released as soon we return from this foregin function.` |
|      - | 1662 | `	 */` |
|   6203 | 1663 | `	return PH7_OK;` |
|   3120 | 1664 | `}` |
|      - | 1665 | `/*` |
|      - | 1666 | ` * string trim(string $str[,string $charlist ])` |
|      - | 1667 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 1668 | ` * Parameters` |
|      - | 1669 | ` *  $str` |
|      - | 1670 | ` *   The string that will be trimmed.` |
|      - | 1671 | ` * $charlist` |
|      - | 1672 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1673 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1674 | ` *   With .. you can specify a range of characters.` |
|      - | 1675 | ` * Returns.` |
|      - | 1676 | ` *  Thr processed string.` |
|      - | 1677 | ` * NOTE:` |
|      - | 1678 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 1679 | ` */` |
|  14074 | 1680 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1681 | `{` |
|      - | 1682 | `	const char *zString;` |
|      - | 1683 | `	int nLen;` |
|  14079 | 1684 | `	if( nArg < 1 ){` |
|      - | 1685 | `		/* Missing arguments,return null */` |
|      3 | 1686 | `		ph7_result_null(pCtx);` |
|      3 | 1687 | `		return PH7_OK;` |
|      - | 1688 | `	}` |
|      - | 1689 | `	/* Extract the target string */` |
|  14077 | 1690 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  14077 | 1691 | `	if( nLen < 1 ){` |
|      - | 1692 | `		/* Empty string,return */` |
|   1657 | 1693 | `		ph7_result_string(pCtx,"",0);` |
|   1657 | 1694 | `		return PH7_OK;` |
|      - | 1695 | `	}` |
|      - | 1696 | `	/* Start the trim process */` |
|  12425 | 1697 | `	if( nArg < 2 ){` |
|      - | 1698 | `		SyString sStr;` |
|      - | 1699 | `		/* Remove white spaces and NUL bytes */` |
|  12395 | 1700 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  30401 | 1701 | `		SyStringFullTrimSafe(&sStr);` |
|  12395 | 1702 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   6200 | 1703 | `	}else{` |
|      - | 1704 | `		/* Char list */` |
|      - | 1705 | `		const char *zList;` |
|      - | 1706 | `		int nListlen;` |
|     33 | 1707 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     33 | 1708 | `		if( nListlen < 1 ){` |
|      - | 1709 | `			/* Return the string unchanged */` |
|      6 | 1710 | `			ph7_result_string(pCtx,zString,nLen);` |
|      4 | 1711 | `		}else{` |
|      - | 1712 | `			char aMask[256];` |
|     29 | 1713 | `			const char *zEnd = &zString[nLen];` |
|     29 | 1714 | `			const char *zCur = zString;` |
|     29 | 1715 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 1716 | `			/* Left trim */` |
|     79 | 1717 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     53 | 1718 | `				zCur++;` |
|      3 | 1719 | `			}` |
|      - | 1720 | `			/* Right trim */` |
|     79 | 1721 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     53 | 1722 | `				zEnd--;` |
|      3 | 1723 | `			}` |
|     29 | 1724 | `			if( zCur >= zEnd ){` |
|      - | 1725 | `				/* Return the empty string */` |
|    ! 0 | 1726 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1727 | `			}else{` |
|     29 | 1728 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 1729 | `			}` |
|      - | 1730 | `		}` |
|      - | 1731 | `	}` |
|  12425 | 1732 | `	return PH7_OK;` |
|   7042 | 1733 | `}` |
|      - | 1734 | `/*` |
|      - | 1735 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 1736 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 1737 | ` * Parameters` |
|      - | 1738 | ` *  $str` |
|      - | 1739 | ` *   The string that will be trimmed.` |
|      - | 1740 | ` * $charlist` |
|      - | 1741 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1742 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1743 | ` *   With .. you can specify a range of characters.` |
|      - | 1744 | ` * Returns.` |
|      - | 1745 | ` *  Thr processed string.` |
|      - | 1746 | ` * NOTE:` |
|      - | 1747 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 1748 | ` */` |
|     30 | 1749 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1750 | `{` |
|      - | 1751 | `	const char *zString;` |
|      - | 1752 | `	int nLen;` |
|     33 | 1753 | `	if( nArg < 1 ){` |
|      - | 1754 | `		/* Missing arguments,return null */` |
|      3 | 1755 | `		ph7_result_null(pCtx);` |
|      3 | 1756 | `		return PH7_OK;` |
|      - | 1757 | `	}` |
|      - | 1758 | `	/* Extract the target string */` |
|     31 | 1759 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 1760 | `	if( nLen < 1 ){` |
|      - | 1761 | `		/* Empty string,return */` |
|      5 | 1762 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1763 | `		return PH7_OK;` |
|      - | 1764 | `	}` |
|      - | 1765 | `	/* Start the trim process */` |
|     27 | 1766 | `	if( nArg < 2 ){` |
|      - | 1767 | `		SyString sStr;` |
|      - | 1768 | `		/* Remove white spaces and NUL bytes*/` |
|     17 | 1769 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     41 | 1770 | `		SyStringRightTrimSafe(&sStr);` |
|     17 | 1771 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      9 | 1772 | `	}else{` |
|      - | 1773 | `		/* Char list */` |
|      - | 1774 | `		const char *zList;` |
|      - | 1775 | `		int nListlen;` |
|     11 | 1776 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     11 | 1777 | `		if( nListlen < 1 ){` |
|      - | 1778 | `			/* Return the string unchanged */` |
|    ! 0 | 1779 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 1780 | `		}else{` |
|      - | 1781 | `			char aMask[256];` |
|     11 | 1782 | `			const char *zEnd = &zString[nLen];` |
|     11 | 1783 | `			const char *zCur = zString;` |
|     11 | 1784 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 1785 | `			/* Right trim */` |
|     29 | 1786 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     20 | 1787 | `				zEnd--;` |
|      2 | 1788 | `			}` |
|     11 | 1789 | `			if( zEnd <= zCur ){` |
|      - | 1790 | `				/* Return the empty string */` |
|    ! 0 | 1791 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1792 | `			}else{` |
|     11 | 1793 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 1794 | `			}` |
|      - | 1795 | `		}` |
|      - | 1796 | `	}` |
|     27 | 1797 | `	return PH7_OK;` |
|     18 | 1798 | `}` |
|      - | 1799 | `/*` |
|      - | 1800 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 1801 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 1802 | ` * Parameters` |
|      - | 1803 | ` *  $str` |
|      - | 1804 | ` *   The string that will be trimmed.` |
|      - | 1805 | ` * $charlist` |
|      - | 1806 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1807 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1808 | ` *   With .. you can specify a range of characters.` |
|      - | 1809 | ` * Returns.` |
|      - | 1810 | ` *  Thr processed string.` |
|      - | 1811 | ` * NOTE:` |
|      - | 1812 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 1813 | ` */` |
|     14 | 1814 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1815 | `{` |
|      - | 1816 | `	const char *zString;` |
|      - | 1817 | `	int nLen;` |
|     16 | 1818 | `	if( nArg < 1 ){` |
|      - | 1819 | `		/* Missing arguments,return null */` |
|      3 | 1820 | `		ph7_result_null(pCtx);` |
|      3 | 1821 | `		return PH7_OK;` |
|      - | 1822 | `	}` |
|      - | 1823 | `	/* Extract the target string */` |
|     14 | 1824 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     14 | 1825 | `	if( nLen < 1 ){` |
|      - | 1826 | `		/* Empty string,return */` |
|    ! 0 | 1827 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1828 | `		return PH7_OK;` |
|      - | 1829 | `	}` |
|      - | 1830 | `	/* Start the trim process */` |
|     14 | 1831 | `	if( nArg < 2 ){` |
|      - | 1832 | `		SyString sStr;` |
|      - | 1833 | `		/* Remove white spaces and NUL byte */` |
|      3 | 1834 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|      8 | 1835 | `		SyStringLeftTrimSafe(&sStr);` |
|      3 | 1836 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      2 | 1837 | `	}else{` |
|      - | 1838 | `		/* Char list */` |
|      - | 1839 | `		const char *zList;` |
|      - | 1840 | `		int nListlen;` |
|     12 | 1841 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     12 | 1842 | `		if( nListlen < 1 ){` |
|      - | 1843 | `			/* Return the string unchanged */` |
|      3 | 1844 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 1845 | `		}else{` |
|      - | 1846 | `			char aMask[256];` |
|     10 | 1847 | `			const char *zEnd = &zString[nLen];` |
|     10 | 1848 | `			const char *zCur = zString;` |
|     10 | 1849 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 1850 | `			/* Left trim */` |
|     28 | 1851 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     20 | 1852 | `				zCur++;` |
|      2 | 1853 | `			}` |
|     10 | 1854 | `			if( zCur >= zEnd ){` |
|      - | 1855 | `				/* Return the empty string */` |
|    ! 0 | 1856 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1857 | `			}else{` |
|     10 | 1858 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 1859 | `			}` |
|      - | 1860 | `		}` |
|      - | 1861 | `	}` |
|     14 | 1862 | `	return PH7_OK;` |
|      9 | 1863 | `}` |
|      - | 1864 | `/*` |
|      - | 1865 | ` * string strtolower(string $str)` |
|      - | 1866 | ` *  Make a string lowercase.` |
|      - | 1867 | ` * Parameters` |
|      - | 1868 | ` *  $str` |
|      - | 1869 | ` *   The input string.` |
|      - | 1870 | ` * Returns.` |
|      - | 1871 | ` *  The lowercased string.` |
|      - | 1872 | ` */` |
|  32358 | 1873 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1874 | `{` |
|      - | 1875 | `	const char *zString,*zCur,*zEnd;` |
|      - | 1876 | `	int nLen;` |
|  32363 | 1877 | `	if( nArg < 1 ){` |
|      - | 1878 | `		/* Missing arguments,return null */` |
|      3 | 1879 | `		ph7_result_null(pCtx);` |
|      3 | 1880 | `		return PH7_OK;` |
|      - | 1881 | `	}` |
|      - | 1882 | `	/* Extract the target string */` |
|  32361 | 1883 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  32361 | 1884 | `	if( nLen < 1 ){` |
|      - | 1885 | `		/* Empty string,return */` |
|      3 | 1886 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1887 | `		return PH7_OK;` |
|      - | 1888 | `	}` |
|      - | 1889 | `	/* Perform the requested operation */` |
|  32359 | 1890 | `	zEnd = &zString[nLen];` |
| 101911 | 1891 | `	for(;;){` |
| 203827 | 1892 | `		if( zString >= zEnd ){` |
|      - | 1893 | `			/* No more input,break immediately */` |
|  32359 | 1894 | `			break;` |
|      - | 1895 | `		}` |
| 171473 | 1896 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 1897 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 1898 | `			zCur = zString;` |
|    ! 0 | 1899 | `			zString++;` |
|    ! 0 | 1900 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 1901 | `				zString++;` |
|    ! 0 | 1902 | `			}` |
|      - | 1903 | `			/* Append UTF-8 stream */` |
|    ! 0 | 1904 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 1905 | `		}else{` |
| 171473 | 1906 | `			int c = zString[0];` |
| 171473 | 1907 | `			if( SyisUpper(c) ){` |
| 171471 | 1908 | `				c = SyToLower(zString[0]);` |
|  85733 | 1909 | `			}` |
|      - | 1910 | `			/* Append character */` |
| 171473 | 1911 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 1912 | `			/* Advance the cursor */` |
| 171473 | 1913 | `			zString++;` |
|      - | 1914 | `		}` |
|      5 | 1915 | `	}` |
|  32359 | 1916 | `	return PH7_OK;` |
|  16184 | 1917 | `}` |
|      - | 1918 | `/*` |
|      - | 1919 | ` * string strtolower(string $str)` |
|      - | 1920 | ` *  Make a string uppercase.` |
|      - | 1921 | ` * Parameters` |
|      - | 1922 | ` *  $str` |
|      - | 1923 | ` *   The input string.` |
|      - | 1924 | ` * Returns.` |
|      - | 1925 | ` *  The uppercased string.` |
|      - | 1926 | ` */` |
|     42 | 1927 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1928 | `{` |
|      - | 1929 | `	const char *zString,*zCur,*zEnd;` |
|      - | 1930 | `	int nLen;` |
|     46 | 1931 | `	if( nArg < 1 ){` |
|      - | 1932 | `		/* Missing arguments,return null */` |
|      3 | 1933 | `		ph7_result_null(pCtx);` |
|      3 | 1934 | `		return PH7_OK;` |
|      - | 1935 | `	}` |
|      - | 1936 | `	/* Extract the target string */` |
|     44 | 1937 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     44 | 1938 | `	if( nLen < 1 ){` |
|      - | 1939 | `		/* Empty string,return */` |
|      3 | 1940 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1941 | `		return PH7_OK;` |
|      - | 1942 | `	}` |
|      - | 1943 | `	/* Perform the requested operation */` |
|     42 | 1944 | `	zEnd = &zString[nLen];` |
|     98 | 1945 | `	for(;;){` |
|    200 | 1946 | `		if( zString >= zEnd ){` |
|      - | 1947 | `			/* No more input,break immediately */` |
|     42 | 1948 | `			break;` |
|      - | 1949 | `		}` |
|    162 | 1950 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 1951 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 1952 | `			zCur = zString;` |
|    ! 0 | 1953 | `			zString++;` |
|    ! 0 | 1954 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 1955 | `				zString++;` |
|    ! 0 | 1956 | `			}` |
|      - | 1957 | `			/* Append UTF-8 stream */` |
|    ! 0 | 1958 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 1959 | `		}else{` |
|    162 | 1960 | `			int c = zString[0];` |
|    162 | 1961 | `			if( SyisLower(c) ){` |
|    156 | 1962 | `				c = SyToUpper(zString[0]);` |
|     76 | 1963 | `			}` |
|      - | 1964 | `			/* Append character */` |
|    162 | 1965 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 1966 | `			/* Advance the cursor */` |
|    162 | 1967 | `			zString++;` |
|      - | 1968 | `		}` |
|      4 | 1969 | `	}` |
|     42 | 1970 | `	return PH7_OK;` |
|     25 | 1971 | `}` |
|      - | 1972 | `/*` |
|      - | 1973 | ` * string ucfirst(string $str)` |
|      - | 1974 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 1975 | ` *  character is alphabetic.` |
|      - | 1976 | ` * Parameters` |
|      - | 1977 | ` *  $str` |
|      - | 1978 | ` *   The input string.` |
|      - | 1979 | ` * Returns.` |
|      - | 1980 | ` *  The processed string.` |
|      - | 1981 | ` */` |
|      6 | 1982 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1983 | `{` |
|      - | 1984 | `	const char *zString,*zEnd;` |
|      - | 1985 | `	int nLen,c;` |
|      7 | 1986 | `	if( nArg < 1 ){` |
|      - | 1987 | `		/* Missing arguments,return null */` |
|      3 | 1988 | `		ph7_result_null(pCtx);` |
|      3 | 1989 | `		return PH7_OK;` |
|      - | 1990 | `	}` |
|      - | 1991 | `	/* Extract the target string */` |
|      5 | 1992 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 1993 | `	if( nLen < 1 ){` |
|      - | 1994 | `		/* Empty string,return */` |
|      3 | 1995 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1996 | `		return PH7_OK;` |
|      - | 1997 | `	}` |
|      - | 1998 | `	/* Perform the requested operation */` |
|      3 | 1999 | `	zEnd = &zString[nLen];` |
|      3 | 2000 | `	c = zString[0];` |
|      3 | 2001 | `	if( SyisLower(c) ){` |
|      3 | 2002 | `		c = SyToUpper(c);` |
|      1 | 2003 | `	}` |
|      - | 2004 | `	/* Append the first character */` |
|      3 | 2005 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2006 | `	zString++;` |
|      3 | 2007 | `	if( zString < zEnd ){` |
|      - | 2008 | `		/* Append the rest of the input verbatim */` |
|      3 | 2009 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2010 | `	}` |
|      3 | 2011 | `	return PH7_OK;` |
|      4 | 2012 | `}` |
|      - | 2013 | `/*` |
|      - | 2014 | ` * string lcfirst(string $str)` |
|      - | 2015 | ` *  Make a string's first character lowercase.` |
|      - | 2016 | ` * Parameters` |
|      - | 2017 | ` *  $str` |
|      - | 2018 | ` *   The input string.` |
|      - | 2019 | ` * Returns.` |
|      - | 2020 | ` *  The processed string.` |
|      - | 2021 | ` */` |
|      6 | 2022 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2023 | `{` |
|      - | 2024 | `	const char *zString,*zEnd;` |
|      - | 2025 | `	int nLen,c;` |
|      7 | 2026 | `	if( nArg < 1 ){` |
|      - | 2027 | `		/* Missing arguments,return null */` |
|      3 | 2028 | `		ph7_result_null(pCtx);` |
|      3 | 2029 | `		return PH7_OK;` |
|      - | 2030 | `	}` |
|      - | 2031 | `	/* Extract the target string */` |
|      5 | 2032 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2033 | `	if( nLen < 1 ){` |
|      - | 2034 | `		/* Empty string,return */` |
|      3 | 2035 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2036 | `		return PH7_OK;` |
|      - | 2037 | `	}` |
|      - | 2038 | `	/* Perform the requested operation */` |
|      3 | 2039 | `	zEnd = &zString[nLen];` |
|      3 | 2040 | `	c = zString[0];` |
|      3 | 2041 | `	if( SyisUpper(c) ){` |
|      3 | 2042 | `		c = SyToLower(c);` |
|      1 | 2043 | `	}` |
|      - | 2044 | `	/* Append the first character */` |
|      3 | 2045 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2046 | `	zString++;` |
|      3 | 2047 | `	if( zString < zEnd ){` |
|      - | 2048 | `		/* Append the rest of the input verbatim */` |
|      3 | 2049 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2050 | `	}` |
|      3 | 2051 | `	return PH7_OK;` |
|      4 | 2052 | `}` |
|      - | 2053 | `/*` |
|      - | 2054 | ` * int ord(string $string)` |
|      - | 2055 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 2056 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 2057 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 2058 | ` * Parameters` |
|      - | 2059 | ` *  $string` |
|      - | 2060 | ` *   The input string.` |
|      - | 2061 | ` * Returns` |
|      - | 2062 | ` *  The ASCII value as an integer.` |
|      - | 2063 | ` */` |
|     56 | 2064 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2065 | `{` |
|      - | 2066 | `	const char *zString;` |
|      - | 2067 | `	int nLen,c;` |
|      - | 2068 | `	/* PHP requires exactly one argument. */` |
|     59 | 2069 | `	if( nArg != 1 ){` |
|      8 | 2070 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2071 | `			"ArgumentCountError",` |
|      - | 2072 | `			"ord() expects exactly 1 argument, %d given",` |
|      2 | 2073 | `			nArg` |
|      - | 2074 | `			);` |
|      - | 2075 | `	}` |
|      - | 2076 | `	/* Passing null is deprecated (E_DEPRECATED).  PHP emits this before` |
|      - | 2077 | `	 * the empty-string deprecation, so we check null first. */` |
|     53 | 2078 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 2079 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2080 | `			"ord(): Passing null to parameter #1 ($character) "` |
|      - | 2081 | `			"of type string is deprecated"` |
|      - | 2082 | `			);` |
|      1 | 2083 | `	}` |
|      - | 2084 | `	/* Extract the target string */` |
|     53 | 2085 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     53 | 2086 | `	if( nLen < 1 ){` |
|      - | 2087 | `		/* Empty string is deprecated (E_DEPRECATED). */` |
|      5 | 2088 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2089 | `			"ord(): Providing an empty string is deprecated"` |
|      - | 2090 | `			);` |
|      5 | 2091 | `		ph7_result_int(pCtx,0);` |
|      5 | 2092 | `		return PH7_OK;` |
|      - | 2093 | `	}` |
|      - | 2094 | `	/* A string longer than one byte is deprecated (E_DEPRECATED). */` |
|     49 | 2095 | `	if( nLen > 1 ){` |
|      7 | 2096 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2097 | `			"ord(): Providing a string that is not one byte long "` |
|      - | 2098 | `			"is deprecated. Use ord($str[0]) instead"` |
|      - | 2099 | `			);` |
|      3 | 2100 | `	}` |
|      - | 2101 | `	/* Extract the ASCII value of the first character */` |
|     49 | 2102 | `	c = (unsigned char)zString[0];` |
|      - | 2103 | `	/* Return that value */` |
|     49 | 2104 | `	ph7_result_int(pCtx,c);` |
|     49 | 2105 | `	return PH7_OK;` |
|     31 | 2106 | `}` |
|      - | 2107 | `/*` |
|      - | 2108 | ` * string chr(int $codepoint)` |
|      - | 2109 | ` *  Returns a one-character string containing the character specified` |
|      - | 2110 | ` *  by the given codepoint.  Any integer is accepted; values outside` |
|      - | 2111 | ` *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.` |
|      - | 2112 | ` * Parameters` |
|      - | 2113 | ` *  $codepoint` |
|      - | 2114 | ` *   An integer codepoint.  Values outside 0-255 are deprecated and` |
|      - | 2115 | ` *   will be constrained to a single byte.` |
|      - | 2116 | ` * Returns` |
|      - | 2117 | ` *  A single-character string.` |
|      - | 2118 | ` */` |
|     48 | 2119 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2120 | `{` |
|      - | 2121 | `	int c;` |
|      - | 2122 | `	unsigned char ch;` |
|      - | 2123 | `	/* PHP requires exactly one argument. */` |
|     51 | 2124 | `	if( nArg != 1 ){` |
|      8 | 2125 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2126 | `			"ArgumentCountError",` |
|      - | 2127 | `			"chr() expects exactly 1 argument, %d given",` |
|      2 | 2128 | `			nArg` |
|      - | 2129 | `			);` |
|      - | 2130 | `	}` |
|      - | 2131 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 2132 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 2133 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 2134 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|     45 | 2135 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      - | 2136 | `		char zBuf[120];` |
|      4 | 2137 | `		SyBufferFormat(zBuf,sizeof(zBuf),` |
|      - | 2138 | `			"Implicit conversion from float %g to int loses precision",` |
|      1 | 2139 | `			ph7_value_to_double(apArg[0])` |
|      - | 2140 | `			);` |
|      3 | 2141 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zBuf);` |
|      1 | 2142 | `	}` |
|      - | 2143 | `	/* Extract the codepoint. */` |
|     45 | 2144 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 2145 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 2146 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 2147 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 2148 | `	 * name to avoid the API double-prefixing it. */` |
|     45 | 2149 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 2150 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 2151 | `			E_DEPRECATED,` |
|      - | 2152 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 2153 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 2154 | `			"The value used will be constrained using % 256"` |
|      - | 2155 | `			);` |
|      2 | 2156 | `	}` |
|      - | 2157 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 2158 | `	 * when taking the address of a wider int. */` |
|     45 | 2159 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 2160 | `	/* Return the specified character */` |
|     45 | 2161 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|     45 | 2162 | `	return PH7_OK;` |
|     27 | 2163 | `}` |
|      - | 2164 | `/*` |
|      - | 2165 | ` * Binary to hex consumer callback.` |
|      - | 2166 | ` * This callback is the default consumer used by the hash functions` |
|      - | 2167 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 2168 | ` */` |
|   3118 | 2169 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 2170 | `{` |
|      - | 2171 | `	/* Append hex chunk verbatim */` |
|   3120 | 2172 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   3120 | 2173 | `	return SXRET_OK;` |
|      2 | 2174 | `}` |
|      - | 2175 |  |
|      - | 2176 | `/*` |
|      - | 2177 | ` * string bin2hex(string $str)` |
|      - | 2178 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 2179 | ` * Parameters` |
|      - | 2180 | ` *  $str` |
|      - | 2181 | ` *   The input string.` |
|      - | 2182 | ` * Returns.` |
|      - | 2183 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 2184 | ` */` |
|    138 | 2185 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2186 | `{` |
|      - | 2187 | `	const char *zString;` |
|      - | 2188 | `	int nLen;` |
|      - | 2189 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|    143 | 2190 | `	if( nArg != 1 ){` |
|      8 | 2191 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2192 | `			"ArgumentCountError",` |
|      - | 2193 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|      2 | 2194 | `			nArg` |
|      - | 2195 | `			);` |
|      - | 2196 | `	}` |
|      - | 2197 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 2198 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 2199 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 2200 | `	 */` |
|    204 | 2201 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|    130 | 2202 | `		( ph7_value_is_object(apArg[0]) &&` |
|      3 | 2203 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|      2 | 2204 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|      1 | 2205 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 2206 | `		)` |
|      - | 2207 | `	){` |
|      9 | 2208 | `		const char *zType = ph7_type_name(apArg[0]);` |
|      9 | 2209 | `		if( ph7_value_is_object(apArg[0]) ){` |
|      3 | 2210 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      3 | 2211 | `			if( pInst && pInst->pClass ){` |
|      3 | 2212 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      1 | 2213 | `			}` |
|      1 | 2214 | `		}` |
|     12 | 2215 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2216 | `			"TypeError",` |
|      - | 2217 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|      3 | 2218 | `			zType` |
|      - | 2219 | `			);` |
|      - | 2220 | `	}` |
|      - | 2221 | `	/* Extract the target string */` |
|    130 | 2222 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    130 | 2223 | `	if( nLen < 1 ){` |
|      - | 2224 | `		/* Empty string,return */` |
|     13 | 2225 | `		ph7_result_string(pCtx,"",0);` |
|     13 | 2226 | `		return PH7_OK;` |
|      - | 2227 | `	}` |
|      - | 2228 | `	/* Perform the requested operation */` |
|    118 | 2229 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|    118 | 2230 | `	return PH7_OK;` |
|     74 | 2231 | `}` |
|      - | 2232 |  |
|      - | 2233 | `/* Search callback signature */` |
|      - | 2234 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 2235 | `/*` |
|      - | 2236 | ` * Case-insensitive pattern match.` |
|      - | 2237 | ` * Brute force is the default search method used here.` |
|      - | 2238 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 2239 | ` * well for short/medium texts on modern hardware.` |
|      - | 2240 | ` */` |
|    118 | 2241 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      1 | 2242 | `{` |
|    119 | 2243 | `	const char *zpIn = (const char *)pPattern;` |
|    119 | 2244 | `	const char *zIn = (const char *)pText;` |
|    119 | 2245 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    119 | 2246 | `	const char *zEnd = &zIn[nLen];` |
|      - | 2247 | `	const char *zPtr,*zPtr2;` |
|      - | 2248 | `	int c,d;` |
|    119 | 2249 | `	if( iPatLen > nLen ){` |
|      - | 2250 | `		/* Don't bother processing */` |
|     33 | 2251 | `		return SXERR_NOTFOUND;` |
|      - | 2252 | `	}` |
|    242 | 2253 | `	for(;;){` |
|    485 | 2254 | `		if( zIn >= zEnd ){` |
|     47 | 2255 | `			break;` |
|      - | 2256 | `		}` |
|    439 | 2257 | `		c = SyToLower(zIn[0]);` |
|    439 | 2258 | `		d = SyToLower(zpIn[0]);` |
|    439 | 2259 | `		if( c == d ){` |
|     41 | 2260 | `			zPtr   = &zIn[1];` |
|     41 | 2261 | `			zPtr2  = &zpIn[1];` |
|     71 | 2262 | `			for(;;){` |
|    143 | 2263 | `				if( zPtr2 >= zpEnd ){` |
|      - | 2264 | `					/* Pattern found */` |
|     41 | 2265 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 2266 | `					return SXRET_OK;` |
|      - | 2267 | `				}` |
|    103 | 2268 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 2269 | `					break;` |
|      - | 2270 | `				}` |
|    103 | 2271 | `				c = SyToLower(zPtr[0]);` |
|    103 | 2272 | `				d = SyToLower(zPtr2[0]);` |
|    103 | 2273 | `				if( c != d ){` |
|    ! 0 | 2274 | `					break;` |
|      - | 2275 | `				}` |
|    103 | 2276 | `				zPtr++; zPtr2++;` |
|      1 | 2277 | `			}` |
|    ! 0 | 2278 | `		}` |
|    399 | 2279 | `		zIn++;` |
|      1 | 2280 | `	}` |
|      - | 2281 | `	/* Pattern not found */` |
|     47 | 2282 | `	return SXERR_NOTFOUND;` |
|     60 | 2283 | `}` |
|      - | 2284 | `/*` |
|      - | 2285 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2286 | ` *  Find the first occurrence of a string.` |
|      - | 2287 | ` * Parameters` |
|      - | 2288 | ` *  $haystack` |
|      - | 2289 | ` *   The input string.` |
|      - | 2290 | ` * $needle` |
|      - | 2291 | ` *   Search pattern (must be a string).` |
|      - | 2292 | ` * $before_needle` |
|      - | 2293 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2294 | ` *   of the needle (excluding the needle).` |
|      - | 2295 | ` * Return` |
|      - | 2296 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2297 | ` */` |
|     10 | 2298 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2299 | `{` |
|     11 | 2300 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2301 | `	const char *zBlob,*zPattern;` |
|      - | 2302 | `	int nLen,nPatLen;` |
|      - | 2303 | `	sxu32 nOfft;` |
|      - | 2304 | `	sxi32 rc;` |
|     11 | 2305 | `	if( nArg < 2 ){` |
|      - | 2306 | `		/* Missing arguments,return FALSE */` |
|      5 | 2307 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2308 | `		return PH7_OK;` |
|      - | 2309 | `	}` |
|      - | 2310 | `	/* Extract the needle and the haystack */` |
|      7 | 2311 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 2312 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 2313 | `	nOfft = 0; /* cc warning */` |
|      9 | 2314 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 2315 | `		int before = 0;` |
|      - | 2316 | `		/* Perform the lookup */` |
|      5 | 2317 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2318 | `		if( rc != SXRET_OK ){` |
|      - | 2319 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2320 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2321 | `			return PH7_OK;` |
|      - | 2322 | `		}` |
|      - | 2323 | `		/* Return the portion of the string */` |
|      5 | 2324 | `		if( nArg > 2 ){` |
|      3 | 2325 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2326 | `		}` |
|      5 | 2327 | `		if( before ){` |
|      3 | 2328 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2329 | `		}else{` |
|      3 | 2330 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2331 | `		}` |
|      3 | 2332 | `	}else{` |
|      3 | 2333 | `		ph7_result_bool(pCtx,0);` |
|      - | 2334 | `	}` |
|      7 | 2335 | `	return PH7_OK;` |
|      6 | 2336 | `}` |
|      - | 2337 | `/*` |
|      - | 2338 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2339 | ` *  Case-insensitive strstr().` |
|      - | 2340 | ` * Parameters` |
|      - | 2341 | ` *  $haystack` |
|      - | 2342 | ` *   The input string.` |
|      - | 2343 | ` * $needle` |
|      - | 2344 | ` *   Search pattern (must be a string).` |
|      - | 2345 | ` * $before_needle` |
|      - | 2346 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2347 | ` *   of the needle (excluding the needle).` |
|      - | 2348 | ` * Return` |
|      - | 2349 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2350 | ` */` |
|      6 | 2351 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2352 | `{` |
|      7 | 2353 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2354 | `	const char *zBlob,*zPattern;` |
|      - | 2355 | `	int nLen,nPatLen;` |
|      - | 2356 | `	sxu32 nOfft;` |
|      - | 2357 | `	sxi32 rc;` |
|      7 | 2358 | `	if( nArg < 2 ){` |
|      - | 2359 | `		/* Missing arguments,return FALSE */` |
|      3 | 2360 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2361 | `		return PH7_OK;` |
|      - | 2362 | `	}` |
|      - | 2363 | `	/* Extract the needle and the haystack */` |
|      5 | 2364 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2365 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 2366 | `	nOfft = 0; /* cc warning */` |
|      7 | 2367 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 2368 | `		int before = 0;` |
|      - | 2369 | `		/* Perform the lookup */` |
|      5 | 2370 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2371 | `		if( rc != SXRET_OK ){` |
|      - | 2372 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2373 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2374 | `			return PH7_OK;` |
|      - | 2375 | `		}` |
|      - | 2376 | `		/* Return the portion of the string */` |
|      5 | 2377 | `		if( nArg > 2 ){` |
|      3 | 2378 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2379 | `		}` |
|      5 | 2380 | `		if( before ){` |
|      3 | 2381 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2382 | `		}else{` |
|      3 | 2383 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2384 | `		}` |
|      3 | 2385 | `	}else{` |
|    ! 0 | 2386 | `		ph7_result_bool(pCtx,0);` |
|      - | 2387 | `	}` |
|      5 | 2388 | `	return PH7_OK;` |
|      4 | 2389 | `}` |
|      - | 2390 | `/*` |
|      - | 2391 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2392 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 2393 | ` * Parameters` |
|      - | 2394 | ` *  $haystack` |
|      - | 2395 | ` *   The input string.` |
|      - | 2396 | ` * $needle` |
|      - | 2397 | ` *   Search pattern (must be a string).` |
|      - | 2398 | ` * $offset` |
|      - | 2399 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 2400 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 2401 | ` *   of haystack.` |
|      - | 2402 | ` * Return` |
|      - | 2403 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 2404 | ` */` |
|    128 | 2405 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2406 | `{` |
|    133 | 2407 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2408 | `	const char *zBlob,*zPattern;` |
|      - | 2409 | `	int nLen,nPatLen,nStart;` |
|      - | 2410 | `	sxu32 nOfft;` |
|      - | 2411 | `	sxi32 rc;` |
|    133 | 2412 | `	if( nArg < 2 ){` |
|      - | 2413 | `		/* Missing arguments,return FALSE */` |
|      7 | 2414 | `		ph7_result_bool(pCtx,0);` |
|      7 | 2415 | `		return PH7_OK;` |
|      - | 2416 | `	}` |
|      - | 2417 | `	/* Extract the needle and the haystack */` |
|    127 | 2418 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    127 | 2419 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    127 | 2420 | `	nOfft = 0; /* cc warning */` |
|    127 | 2421 | `	nStart = 0;` |
|      - | 2422 | `	/* Peek the starting offset if available */` |
|    127 | 2423 | `	if( nArg > 2 ){` |
|    ! 0 | 2424 | `		nStart = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 2425 | `		if( nStart < 0 ){` |
|    ! 0 | 2426 | `			nStart = -nStart;` |
|    ! 0 | 2427 | `		}` |
|    ! 0 | 2428 | `		if( nStart >= nLen ){` |
|      - | 2429 | `			/* Invalid offset */` |
|    ! 0 | 2430 | `			nStart = 0;` |
|    ! 0 | 2431 | `		}else{` |
|    ! 0 | 2432 | `			zBlob += nStart;` |
|    ! 0 | 2433 | `			nLen -= nStart;` |
|      - | 2434 | `		}` |
|    ! 0 | 2435 | `	}` |
|    127 | 2436 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2437 | `		/* Perform the lookup */` |
|    125 | 2438 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    125 | 2439 | `		if( rc != SXRET_OK ){` |
|      - | 2440 | `			/* Pattern not found,return FALSE */` |
|     33 | 2441 | `			ph7_result_bool(pCtx,0);` |
|     33 | 2442 | `			return PH7_OK;` |
|      - | 2443 | `		}` |
|      - | 2444 | `		/* Return the pattern position */` |
|     96 | 2445 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|     50 | 2446 | `	}else{` |
|      3 | 2447 | `		ph7_result_bool(pCtx,0);` |
|      - | 2448 | `	}` |
|     98 | 2449 | `	return PH7_OK;` |
|     69 | 2450 | `}` |
|      - | 2451 | `/*` |
|      - | 2452 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|      - | 2453 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|      - | 2454 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|      - | 2455 | ` * TypeError for arrays, resources, and objects without __toString.` |
|      - | 2456 | ` *` |
|      - | 2457 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|      - | 2458 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|      - | 2459 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|      - | 2460 | ` *` |
|      - | 2461 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|      - | 2462 | ` * is valid until pTmp is released or pArg is mutated.` |
|      - | 2463 | ` */` |
|    418 | 2464 | `static sxi32 StrPredicateResolveArg(` |
|      - | 2465 | `	ph7_context *pCtx,` |
|      - | 2466 | `	ph7_value *pArg,` |
|      - | 2467 | `	const char *zFunc,` |
|      - | 2468 | `	int iArgNum,` |
|      - | 2469 | `	const char *zParamName,` |
|      - | 2470 | `	const char *zNullMsg,` |
|      - | 2471 | `	ph7_value *pTmp,` |
|      - | 2472 | `	const char **pzOut,` |
|      - | 2473 | `	int *pnOut` |
|      4 | 2474 | `){` |
|    422 | 2475 | `	if( ph7_value_is_null(pArg) ){` |
|     13 | 2476 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zNullMsg);` |
|     13 | 2477 | `		*pzOut = "";` |
|     13 | 2478 | `		*pnOut = 0;` |
|     13 | 2479 | `		return PH7_OK;` |
|      - | 2480 | `	}` |
|    628 | 2481 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|    388 | 2482 | `	    ( ph7_value_is_object(pArg) &&` |
|     72 | 2483 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|     48 | 2484 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|     24 | 2485 | `	        "__toString",sizeof("__toString")-1) == 0` |
|      - | 2486 | `	    )` |
|      - | 2487 | `	){` |
|     34 | 2488 | `		const char *zType = ph7_type_name(pArg);` |
|     34 | 2489 | `		if( ph7_value_is_object(pArg) ){` |
|     13 | 2490 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     13 | 2491 | `			if( pInst && pInst->pClass ){` |
|     13 | 2492 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      6 | 2493 | `			}` |
|      6 | 2494 | `		}` |
|     49 | 2495 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2496 | `			"TypeError",` |
|      - | 2497 | `			"%s(): Argument #%d (%s) must be of type string, %s given",` |
|     15 | 2498 | `			zFunc, iArgNum, zParamName, zType` |
|      - | 2499 | `			);` |
|      - | 2500 | `	}` |
|    377 | 2501 | `	if( ph7_value_is_object(pArg) ){` |
|     37 | 2502 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     37 | 2503 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 2504 | `			"__toString",sizeof("__toString")-1);` |
|     37 | 2505 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|     37 | 2506 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|     37 | 2507 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|     37 | 2508 | `		return PH7_OK;` |
|      - | 2509 | `	}` |
|    341 | 2510 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|    341 | 2511 | `	return PH7_OK;` |
|    213 | 2512 | `}` |
|      - | 2513 | `/*` |
|      - | 2514 | ` * bool str_contains(string $haystack, string $needle)` |
|      - | 2515 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|      - | 2516 | ` * Return` |
|      - | 2517 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|      - | 2518 | ` */` |
|     92 | 2519 | `static int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2520 | `{` |
|      - | 2521 | `	const char *zHaystack,*zNeedle;` |
|      - | 2522 | `	int nHayLen,nNeedleLen;` |
|      - | 2523 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2524 | `	sxi32 rc;` |
|     96 | 2525 | `	if( nArg != 2 ){` |
|     18 | 2526 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2527 | `			"ArgumentCountError",` |
|      - | 2528 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|      5 | 2529 | `			nArg` |
|      - | 2530 | `			);` |
|      - | 2531 | `	}` |
|     84 | 2532 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     84 | 2533 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     84 | 2534 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack",` |
|      - | 2535 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|      - | 2536 | `		"of type string is deprecated",` |
|      - | 2537 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     84 | 2538 | `	if( rc != PH7_OK ) goto out;` |
|     77 | 2539 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle",` |
|      - | 2540 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|      - | 2541 | `		"of type string is deprecated",` |
|      - | 2542 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     77 | 2543 | `	if( rc != PH7_OK ) goto out;` |
|     73 | 2544 | `	if( nNeedleLen < 1 ){` |
|     13 | 2545 | `		ph7_result_bool(pCtx,1);` |
|     67 | 2546 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2547 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2548 | `	}else{` |
|     79 | 2549 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|     26 | 2550 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|     53 | 2551 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|      - | 2552 | `	}` |
|     73 | 2553 | `	rc = PH7_OK;` |
|     41 | 2554 | `out:` |
|     84 | 2555 | `	PH7_MemObjRelease(&sHayTmp);` |
|     84 | 2556 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     84 | 2557 | `	return rc;` |
|     50 | 2558 | `}` |
|      - | 2559 | `/*` |
|      - | 2560 | ` * bool str_starts_with(string $haystack, string $needle)` |
|      - | 2561 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|      - | 2562 | ` * Return` |
|      - | 2563 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|      - | 2564 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 2565 | ` */` |
|     78 | 2566 | `static int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2567 | `{` |
|      - | 2568 | `	const char *zHaystack,*zNeedle;` |
|      - | 2569 | `	int nHayLen,nNeedleLen;` |
|      - | 2570 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2571 | `	sxi32 rc;` |
|     82 | 2572 | `	if( nArg != 2 ){` |
|     18 | 2573 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2574 | `			"ArgumentCountError",` |
|      - | 2575 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|      5 | 2576 | `			nArg` |
|      - | 2577 | `			);` |
|      - | 2578 | `	}` |
|     70 | 2579 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 2580 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 2581 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack",` |
|      - | 2582 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 2583 | `		"of type string is deprecated",` |
|      - | 2584 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 2585 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 2586 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle",` |
|      - | 2587 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|      - | 2588 | `		"of type string is deprecated",` |
|      - | 2589 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 2590 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 2591 | `	if( nNeedleLen < 1 ){` |
|     13 | 2592 | `		ph7_result_bool(pCtx,1);` |
|     53 | 2593 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2594 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2595 | `	}else{` |
|     58 | 2596 | `		ph7_result_bool(pCtx,` |
|     38 | 2597 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 2598 | `	}` |
|     59 | 2599 | `	rc = PH7_OK;` |
|     34 | 2600 | `out:` |
|     70 | 2601 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 2602 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 2603 | `	return rc;` |
|     43 | 2604 | `}` |
|      - | 2605 | `/*` |
|      - | 2606 | ` * bool str_ends_with(string $haystack, string $needle)` |
|      - | 2607 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|      - | 2608 | ` * Return` |
|      - | 2609 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|      - | 2610 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 2611 | ` */` |
|     78 | 2612 | `static int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2613 | `{` |
|      - | 2614 | `	const char *zHaystack,*zNeedle;` |
|      - | 2615 | `	int nHayLen,nNeedleLen;` |
|      - | 2616 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2617 | `	sxi32 rc;` |
|     82 | 2618 | `	if( nArg != 2 ){` |
|     18 | 2619 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2620 | `			"ArgumentCountError",` |
|      - | 2621 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|      5 | 2622 | `			nArg` |
|      - | 2623 | `			);` |
|      - | 2624 | `	}` |
|     70 | 2625 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 2626 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 2627 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack",` |
|      - | 2628 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 2629 | `		"of type string is deprecated",` |
|      - | 2630 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 2631 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 2632 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle",` |
|      - | 2633 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|      - | 2634 | `		"of type string is deprecated",` |
|      - | 2635 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 2636 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 2637 | `	if( nNeedleLen < 1 ){` |
|     13 | 2638 | `		ph7_result_bool(pCtx,1);` |
|     53 | 2639 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2640 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2641 | `	}else{` |
|     58 | 2642 | `		ph7_result_bool(pCtx,` |
|     38 | 2643 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 2644 | `	}` |
|     59 | 2645 | `	rc = PH7_OK;` |
|     34 | 2646 | `out:` |
|     70 | 2647 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 2648 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 2649 | `	return rc;` |
|     43 | 2650 | `}` |
|      - | 2651 | `/*` |
|      - | 2652 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2653 | ` *  Case-insensitive strpos.` |
|      - | 2654 | ` * Parameters` |
|      - | 2655 | ` *  $haystack` |
|      - | 2656 | ` *   The input string.` |
|      - | 2657 | ` * $needle` |
|      - | 2658 | ` *   Search pattern (must be a string).` |
|      - | 2659 | ` * $offset` |
|      - | 2660 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 2661 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 2662 | ` *   of haystack.` |
|      - | 2663 | ` * Return` |
|      - | 2664 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 2665 | ` */` |
|     18 | 2666 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2667 | `{` |
|     19 | 2668 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2669 | `	const char *zBlob,*zPattern;` |
|      - | 2670 | `	int nLen,nPatLen,nStart;` |
|      - | 2671 | `	sxu32 nOfft;` |
|      - | 2672 | `	sxi32 rc;` |
|     19 | 2673 | `	if( nArg < 2 ){` |
|      - | 2674 | `		/* Missing arguments,return FALSE */` |
|      3 | 2675 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2676 | `		return PH7_OK;` |
|      - | 2677 | `	}` |
|      - | 2678 | `	/* Extract the needle and the haystack */` |
|     17 | 2679 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 2680 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     17 | 2681 | `	nOfft = 0; /* cc warning */` |
|     17 | 2682 | `	nStart = 0;` |
|      - | 2683 | `	/* Peek the starting offset if available */` |
|     17 | 2684 | `	if( nArg > 2 ){` |
|      5 | 2685 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 2686 | `		if( nStart < 0 ){` |
|      3 | 2687 | `			nStart = -nStart;` |
|      1 | 2688 | `		}` |
|      5 | 2689 | `		if( nStart >= nLen ){` |
|      - | 2690 | `			/* Invalid offset */` |
|    ! 0 | 2691 | `			nStart = 0;` |
|    ! 0 | 2692 | `		}else{` |
|      5 | 2693 | `			zBlob += nStart;` |
|      5 | 2694 | `			nLen -= nStart;` |
|      - | 2695 | `		}` |
|      2 | 2696 | `	}` |
|     17 | 2697 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2698 | `		/* Perform the lookup */` |
|     17 | 2699 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     17 | 2700 | `		if( rc != SXRET_OK ){` |
|      - | 2701 | `			/* Pattern not found,return FALSE */` |
|      3 | 2702 | `			ph7_result_bool(pCtx,0);` |
|      3 | 2703 | `			return PH7_OK;` |
|      - | 2704 | `		}` |
|      - | 2705 | `		/* Return the pattern position */` |
|     15 | 2706 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 2707 | `	}else{` |
|    ! 0 | 2708 | `		ph7_result_bool(pCtx,0);` |
|      - | 2709 | `	}` |
|     15 | 2710 | `	return PH7_OK;` |
|     10 | 2711 | `}` |
|      - | 2712 | `/*` |
|      - | 2713 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2714 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 2715 | ` * Parameters` |
|      - | 2716 | ` *  $haystack` |
|      - | 2717 | ` *   The input string.` |
|      - | 2718 | ` * $needle` |
|      - | 2719 | ` *   Search pattern (must be a string).` |
|      - | 2720 | ` * $offset` |
|      - | 2721 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 2722 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 2723 | ` *   characters from the end of the string, searching backwards.` |
|      - | 2724 | ` * Return` |
|      - | 2725 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 2726 | ` */` |
|     32 | 2727 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2728 | `{` |
|      - | 2729 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     33 | 2730 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2731 | `	int nLen,nPatLen;` |
|      - | 2732 | `	sxu32 nOfft;` |
|      - | 2733 | `	sxi32 rc;` |
|     33 | 2734 | `	if( nArg < 2 ){` |
|      - | 2735 | `		/* Missing arguments,return FALSE */` |
|      3 | 2736 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2737 | `		return PH7_OK;` |
|      - | 2738 | `	}` |
|      - | 2739 | `	/* Extract the needle and the haystack */` |
|     31 | 2740 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 2741 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 2742 | `	/* Point to the end of the pattern */` |
|     31 | 2743 | `	zPtr = &zBlob[nLen - 1];` |
|     31 | 2744 | `	zEnd = &zBlob[nLen];` |
|      - | 2745 | `	/* Save the starting posistion */` |
|     31 | 2746 | `	zStart = zBlob;` |
|     31 | 2747 | `	nOfft = 0; /* cc warning */` |
|      - | 2748 | `	/* Peek the starting offset if available */` |
|     31 | 2749 | `	if( nArg > 2 ){` |
|      - | 2750 | `		int nStart;` |
|     21 | 2751 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 2752 | `		if( nStart < 0 ){` |
|     11 | 2753 | `			nStart = -nStart;` |
|     11 | 2754 | `			if( nStart >= nLen ){` |
|      - | 2755 | `				/* Invalid offset */` |
|      3 | 2756 | `				ph7_result_bool(pCtx,0);` |
|      3 | 2757 | `				return PH7_OK;` |
|    ! 0 | 2758 | `			}else{` |
|      9 | 2759 | `				nLen -= nStart;` |
|      9 | 2760 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 2761 | `				zEnd = &zBlob[nLen];` |
|      - | 2762 | `			}` |
|      5 | 2763 | `		}else{` |
|     11 | 2764 | `			if( nStart >= nLen ){` |
|      - | 2765 | `				/* Invalid offset */` |
|      5 | 2766 | `				ph7_result_bool(pCtx,0);` |
|      5 | 2767 | `				return PH7_OK;` |
|    ! 0 | 2768 | `			}else{` |
|      7 | 2769 | `				zBlob += nStart;` |
|      7 | 2770 | `				nLen -= nStart;` |
|      - | 2771 | `			}` |
|      - | 2772 | `		}` |
|      7 | 2773 | `	}` |
|     25 | 2774 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2775 | `		/* Perform the lookup */` |
|     57 | 2776 | `		for(;;){` |
|    115 | 2777 | `			if( zBlob >= zPtr ){` |
|     11 | 2778 | `				break;` |
|      - | 2779 | `			}` |
|    105 | 2780 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    105 | 2781 | `			if( rc == SXRET_OK ){` |
|      - | 2782 | `				/* Pattern found,return it's position */` |
|     13 | 2783 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 2784 | `				return PH7_OK;` |
|      - | 2785 | `			}` |
|     93 | 2786 | `			zPtr--;` |
|      1 | 2787 | `		}` |
|      - | 2788 | `		/* Pattern not found,return FALSE */` |
|     11 | 2789 | `		ph7_result_bool(pCtx,0);` |
|      6 | 2790 | `	}else{` |
|      3 | 2791 | `		ph7_result_bool(pCtx,0);` |
|      - | 2792 | `	}` |
|     13 | 2793 | `	return PH7_OK;` |
|     17 | 2794 | `}` |
|      - | 2795 | `/*` |
|      - | 2796 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2797 | ` *  Case-insensitive strrpos.` |
|      - | 2798 | ` * Parameters` |
|      - | 2799 | ` *  $haystack` |
|      - | 2800 | ` *   The input string.` |
|      - | 2801 | ` * $needle` |
|      - | 2802 | ` *   Search pattern (must be a string).` |
|      - | 2803 | ` * $offset` |
|      - | 2804 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 2805 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 2806 | ` *   characters from the end of the string, searching backwards.` |
|      - | 2807 | ` * Return` |
|      - | 2808 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 2809 | ` */` |
|     28 | 2810 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2811 | `{` |
|      - | 2812 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     29 | 2813 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2814 | `	int nLen,nPatLen;` |
|      - | 2815 | `	sxu32 nOfft;` |
|      - | 2816 | `	sxi32 rc;` |
|     29 | 2817 | `	if( nArg < 2 ){` |
|      - | 2818 | `		/* Missing arguments,return FALSE */` |
|      3 | 2819 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2820 | `		return PH7_OK;` |
|      - | 2821 | `	}` |
|      - | 2822 | `	/* Extract the needle and the haystack */` |
|     27 | 2823 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 2824 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 2825 | `	/* Point to the end of the pattern */` |
|     27 | 2826 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 2827 | `	zEnd = &zBlob[nLen];` |
|      - | 2828 | `	/* Save the starting posistion */` |
|     27 | 2829 | `	zStart = zBlob;` |
|     27 | 2830 | `	nOfft = 0; /* cc warning */` |
|      - | 2831 | `	/* Peek the starting offset if available */` |
|     27 | 2832 | `	if( nArg > 2 ){` |
|      - | 2833 | `		int nStart;` |
|     15 | 2834 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 2835 | `		if( nStart < 0 ){` |
|      7 | 2836 | `			nStart = -nStart;` |
|      7 | 2837 | `			if( nStart >= nLen ){` |
|      - | 2838 | `				/* Invalid offset */` |
|      3 | 2839 | `				ph7_result_bool(pCtx,0);` |
|      3 | 2840 | `				return PH7_OK;` |
|    ! 0 | 2841 | `			}else{` |
|      5 | 2842 | `				nLen -= nStart;` |
|      5 | 2843 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 2844 | `				zEnd = &zBlob[nLen];` |
|      - | 2845 | `			}` |
|      3 | 2846 | `		}else{` |
|      9 | 2847 | `			if( nStart >= nLen ){` |
|      - | 2848 | `				/* Invalid offset */` |
|      5 | 2849 | `				ph7_result_bool(pCtx,0);` |
|      5 | 2850 | `				return PH7_OK;` |
|    ! 0 | 2851 | `			}else{` |
|      5 | 2852 | `				zBlob += nStart;` |
|      5 | 2853 | `				nLen -= nStart;` |
|      - | 2854 | `			}` |
|      - | 2855 | `		}` |
|      4 | 2856 | `	}` |
|     21 | 2857 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2858 | `		/* Perform the lookup */` |
|     44 | 2859 | `		for(;;){` |
|     89 | 2860 | `			if( zBlob >= zPtr ){` |
|      9 | 2861 | `				break;` |
|      - | 2862 | `			}` |
|     81 | 2863 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 2864 | `			if( rc == SXRET_OK ){` |
|      - | 2865 | `				/* Pattern found,return it's position */` |
|     11 | 2866 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 2867 | `				return PH7_OK;` |
|      - | 2868 | `			}` |
|     71 | 2869 | `			zPtr--;` |
|      1 | 2870 | `		}` |
|      - | 2871 | `		/* Pattern not found,return FALSE */` |
|      9 | 2872 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2873 | `	}else{` |
|      3 | 2874 | `		ph7_result_bool(pCtx,0);` |
|      - | 2875 | `	}` |
|     11 | 2876 | `	return PH7_OK;` |
|     15 | 2877 | `}` |
|      - | 2878 | `/*` |
|      - | 2879 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 2880 | ` *  Find the last occurrence of a character in a string.` |
|      - | 2881 | ` * Parameters` |
|      - | 2882 | ` *  $haystack` |
|      - | 2883 | ` *   The input string.` |
|      - | 2884 | ` * $needle` |
|      - | 2885 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 2886 | ` *  This behavior is different from that of strstr().` |
|      - | 2887 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 2888 | ` *  as the ordinal value of a character.` |
|      - | 2889 | ` * Return` |
|      - | 2890 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 2891 | ` */` |
|     24 | 2892 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2893 | `{` |
|      - | 2894 | `	const char *zBlob;` |
|      - | 2895 | `	int nLen,c;` |
|     25 | 2896 | `	if( nArg < 2 ){` |
|      - | 2897 | `		/* Missing arguments,return FALSE */` |
|      3 | 2898 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2899 | `		return PH7_OK;` |
|      - | 2900 | `	}` |
|      - | 2901 | `	/* Extract the haystack */` |
|     23 | 2902 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 2903 | `	c = 0; /* cc warning */` |
|     23 | 2904 | `	if( nLen > 0 ){` |
|      - | 2905 | `		sxu32 nOfft;` |
|      - | 2906 | `		sxi32 rc;` |
|     21 | 2907 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 2908 | `			const char *zPattern;` |
|     11 | 2909 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 2910 | `														 * for NULL pointer.` |
|      - | 2911 | `														 */` |
|     11 | 2912 | `			c = zPattern[0];` |
|      6 | 2913 | `		}else{` |
|      - | 2914 | `			/* Int cast */` |
|     11 | 2915 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 2916 | `		}` |
|      - | 2917 | `		/* Perform the lookup */` |
|     21 | 2918 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 2919 | `		if( rc != SXRET_OK ){` |
|      - | 2920 | `			/* No such entry,return FALSE */` |
|      7 | 2921 | `			ph7_result_bool(pCtx,0);` |
|      7 | 2922 | `			return PH7_OK;` |
|      - | 2923 | `		}` |
|      - | 2924 | `		/* Return the string portion */` |
|     15 | 2925 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 2926 | `	}else{` |
|      3 | 2927 | `		ph7_result_bool(pCtx,0);` |
|      - | 2928 | `	}` |
|     17 | 2929 | `	return PH7_OK;` |
|     13 | 2930 | `}` |
|      - | 2931 | `/*` |
|      - | 2932 | ` * string strrev(string $string)` |
|      - | 2933 | ` *  Reverse a string.` |
|      - | 2934 | ` * Parameters` |
|      - | 2935 | ` *  $string` |
|      - | 2936 | ` *   String to be reversed.` |
|      - | 2937 | ` * Return` |
|      - | 2938 | ` *  The reversed string.` |
|      - | 2939 | ` */` |
|      4 | 2940 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2941 | `{` |
|      - | 2942 | `	const char *zIn,*zEnd;` |
|      - | 2943 | `	int nLen,c;` |
|      5 | 2944 | `	if( nArg < 1 ){` |
|      - | 2945 | `		/* Missing arguments,return NULL */` |
|      3 | 2946 | `		ph7_result_null(pCtx);` |
|      3 | 2947 | `		return PH7_OK;` |
|      - | 2948 | `	}` |
|      - | 2949 | `	/* Extract the target string */` |
|      3 | 2950 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 2951 | `	if( nLen < 1 ){` |
|      - | 2952 | `		/* Empty string Return null */` |
|    ! 0 | 2953 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2954 | `		return PH7_OK;` |
|      - | 2955 | `	}` |
|      - | 2956 | `	/* Perform the requested operation */` |
|      3 | 2957 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 2958 | `	for(;;){` |
|      9 | 2959 | `		if( zEnd < zIn ){` |
|      - | 2960 | `			/* No more input to process */` |
|      3 | 2961 | `			break;` |
|      - | 2962 | `		}` |
|      - | 2963 | `		/* Append current character */` |
|      7 | 2964 | `		c = zEnd[0];` |
|      7 | 2965 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 2966 | `		zEnd--;` |
|      1 | 2967 | `	}` |
|      3 | 2968 | `	return PH7_OK;` |
|      3 | 2969 | `}` |
|      - | 2970 | `/*` |
|      - | 2971 | ` * string ucwords(string $string)` |
|      - | 2972 | ` *  Uppercase the first character of each word in a string.` |
|      - | 2973 | ` *  The definition of a word is any string of characters that is immediately after` |
|      - | 2974 | ` *  a whitespace (These are: space, form-feed, newline, carriage return, horizontal tab, and vertical tab).` |
|      - | 2975 | ` * Parameters` |
|      - | 2976 | ` *  $string` |
|      - | 2977 | ` *   The input string.` |
|      - | 2978 | ` * Return` |
|      - | 2979 | ` *  The modified string..` |
|      - | 2980 | ` */` |
|     14 | 2981 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2982 | `{` |
|      - | 2983 | `	const char *zIn,*zCur,*zEnd;` |
|      - | 2984 | `	int nLen,c;` |
|     15 | 2985 | `	if( nArg < 1 ){` |
|      - | 2986 | `		/* Missing arguments,return NULL */` |
|      3 | 2987 | `		ph7_result_null(pCtx);` |
|      3 | 2988 | `		return PH7_OK;` |
|      - | 2989 | `	}` |
|      - | 2990 | `	/* Extract the target string */` |
|     13 | 2991 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 2992 | `	if( nLen < 1 ){` |
|      - | 2993 | `		/* Empty string – match PHP semantics */` |
|      3 | 2994 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2995 | `		return PH7_OK;` |
|      - | 2996 | `	}` |
|      - | 2997 | `	/* Perform the requested operation */` |
|     11 | 2998 | `	zEnd = &zIn[nLen];` |
|     21 | 2999 | `	for(;;){` |
|      - | 3000 | `		/* Jump leading white spaces */` |
|     43 | 3001 | `		zCur = zIn;` |
|     65 | 3002 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0x80 && SyisSpace(zIn[0]) ){` |
|     23 | 3003 | `			zIn++;` |
|      1 | 3004 | `		}` |
|     43 | 3005 | `		if( zCur < zIn ){` |
|      - | 3006 | `			/* Append white space stream */` |
|     23 | 3007 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 3008 | `		}` |
|     43 | 3009 | `		if( zIn >= zEnd ){` |
|      - | 3010 | `			/* No more input to process */` |
|     11 | 3011 | `			break;` |
|      - | 3012 | `		}` |
|     33 | 3013 | `		c = zIn[0];` |
|     33 | 3014 | `		if( c < 0x80 && SyisLower(c) ){` |
|     29 | 3015 | `			c = SyToUpper(c);` |
|     14 | 3016 | `		}` |
|      - | 3017 | `		/* Append the upper-cased character */` |
|     33 | 3018 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     33 | 3019 | `		zIn++;` |
|     33 | 3020 | `		zCur = zIn;` |
|      - | 3021 | `		/* Append the word varbatim */` |
|    149 | 3022 | `		while( zIn < zEnd ){` |
|    139 | 3023 | `			if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 3024 | `				/* UTF-8 stream */` |
|    ! 0 | 3025 | `				zIn++;` |
|    ! 0 | 3026 | `				SX_JMP_UTF8(zIn,zEnd);` |
|    138 | 3027 | `			}else if( !SyisSpace(zIn[0]) ){` |
|    117 | 3028 | `				zIn++;` |
|     59 | 3029 | `			}else{` |
|     23 | 3030 | `				break;` |
|      - | 3031 | `			}` |
|      1 | 3032 | `		}` |
|     33 | 3033 | `		if( zCur < zIn ){` |
|     33 | 3034 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 | 3035 | `		}` |
|      1 | 3036 | `	}` |
|     11 | 3037 | `	return PH7_OK;` |
|      8 | 3038 | `}` |
|      - | 3039 | `/*` |
|      - | 3040 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3041 | ` *  Returns input repeated multiplier times.` |
|      - | 3042 | ` * Parameters` |
|      - | 3043 | ` *  $string` |
|      - | 3044 | ` *   String to be repeated.` |
|      - | 3045 | ` * $multiplier` |
|      - | 3046 | ` *  Number of time the input string should be repeated.` |
|      - | 3047 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3048 | ` *  to 0, the function will return an empty string.` |
|      - | 3049 | ` * Return` |
|      - | 3050 | ` *  The repeated string.` |
|      - | 3051 | ` */` |
|  20426 | 3052 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3053 | `{` |
|      - | 3054 | `	const char *zIn;` |
|      - | 3055 | `	int nLen,nMul;` |
|      - | 3056 | `	int rc;` |
|  20428 | 3057 | `	if( nArg < 2 ){` |
|      - | 3058 | `		/* Missing arguments,return NULL */` |
|      3 | 3059 | `		ph7_result_null(pCtx);` |
|      3 | 3060 | `		return PH7_OK;` |
|      - | 3061 | `	}` |
|      - | 3062 | `	/* Extract the target string */` |
|  20426 | 3063 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|  20426 | 3064 | `	if( nLen < 1 ){` |
|      - | 3065 | `		/* Empty string.Return null */` |
|    ! 0 | 3066 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3067 | `		return PH7_OK;` |
|      - | 3068 | `	}` |
|      - | 3069 | `	/* Extract the multiplier */` |
|  20426 | 3070 | `	nMul = ph7_value_to_int(apArg[1]);` |
|  20426 | 3071 | `	if( nMul < 1 ){` |
|      - | 3072 | `		/* Return the empty string */` |
|      3 | 3073 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3074 | `		return PH7_OK;` |
|      - | 3075 | `	}` |
|      - | 3076 | `	/* Perform the requested operation */` |
| 220978 | 3077 | `	for(;;){` |
| 441958 | 3078 | `		if( !nMul ){` |
|  20424 | 3079 | `			break;` |
|      - | 3080 | `		}` |
|      - | 3081 | `		/* Append the copy */` |
| 421536 | 3082 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 421536 | 3083 | `		if( rc != PH7_OK ){` |
|      - | 3084 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 3085 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 3086 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 3087 | `		}` |
| 421536 | 3088 | `		nMul--;` |
|      2 | 3089 | `	}` |
|  20424 | 3090 | `	return PH7_OK;` |
|  10215 | 3091 | `}` |
|      - | 3092 | `/*` |
|      - | 3093 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3094 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3095 | ` * Parameters` |
|      - | 3096 | ` *  $string` |
|      - | 3097 | ` *   The input string.` |
|      - | 3098 | ` * $is_xhtml` |
|      - | 3099 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3100 | ` * Return` |
|      - | 3101 | ` *  The processed string.` |
|      - | 3102 | ` */` |
|      6 | 3103 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3104 | `{` |
|      - | 3105 | `	const char *zIn,*zCur,*zEnd;` |
|      7 | 3106 | `	int is_xhtml = 1; /* Default to XHTML-style '<br/>' like PHP */` |
|      - | 3107 | `	int nLen;` |
|      7 | 3108 | `	if( nArg < 1 ){` |
|      - | 3109 | `		/* Missing arguments,return the empty string */` |
|      3 | 3110 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3111 | `		return PH7_OK;` |
|      - | 3112 | `	}` |
|      - | 3113 | `	/* Extract the target string */` |
|      5 | 3114 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3115 | `	if( nLen < 1 ){` |
|      - | 3116 | `		/* Empty string,return null */` |
|    ! 0 | 3117 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3118 | `		return PH7_OK;` |
|      - | 3119 | `	}` |
|      5 | 3120 | `	if( nArg > 1 ){` |
|      3 | 3121 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3122 | `	}` |
|      5 | 3123 | `	zEnd = &zIn[nLen];` |
|      - | 3124 | `	/* Perform the requested operation */` |
|      4 | 3125 | `	for(;;){` |
|      9 | 3126 | `		zCur = zIn;` |
|      - | 3127 | `		/* Delimit the string */` |
|     21 | 3128 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3129 | `			zIn++;` |
|      1 | 3130 | `		}` |
|      9 | 3131 | `		if( zCur < zIn ){` |
|      - | 3132 | `			/* Output chunk verbatim */` |
|      9 | 3133 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3134 | `		}` |
|      9 | 3135 | `		if( zIn >= zEnd ){` |
|      - | 3136 | `			/* No more input to process */` |
|      5 | 3137 | `			break;` |
|      - | 3138 | `		}` |
|      - | 3139 | `		/* Output the HTML line break */` |
|      - | 3140 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br/>' (legacy without space), otherwise use '<br>' */` |
|      5 | 3141 | `		if( is_xhtml ){` |
|      3 | 3142 | `			ph7_result_string(pCtx,"<br/>",(int)sizeof("<br/>")-1);` |
|      2 | 3143 | `		}else{` |
|      3 | 3144 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3145 | `		}` |
|      5 | 3146 | `		zCur = zIn;` |
|      - | 3147 | `		/* Append trailing line */` |
|     11 | 3148 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3149 | `			zIn++;` |
|      1 | 3150 | `		}` |
|      5 | 3151 | `		if( zCur < zIn ){` |
|      - | 3152 | `			/* Output chunk verbatim */` |
|      5 | 3153 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 3154 | `		}` |
|      1 | 3155 | `	}` |
|      5 | 3156 | `	return PH7_OK;` |
|      4 | 3157 | `}` |
|      - | 3158 | `/*` |
|      - | 3159 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3160 | ` *  According to the PHP reference manual.` |
|      - | 3161 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3162 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3163 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 3164 | ` * This applies to both sprintf() and printf().` |
|      - | 3165 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 3166 | ` * or more of these elements, in order:` |
|      - | 3167 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 3168 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 3169 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 3170 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 3171 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 3172 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 3173 | ` *   it with a single quote ('). See the examples below.` |
|      - | 3174 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 3175 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 3176 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 3177 | ` *   should result in.` |
|      - | 3178 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 3179 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 3180 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 3181 | ` *   limit to the string.` |
|      - | 3182 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 3183 | ` *       % - a literal percent character. No argument is required.` |
|      - | 3184 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 3185 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 3186 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 3187 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 3188 | ` * 	     for the number of digits after the decimal point.` |
|      - | 3189 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 3190 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 3191 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 3192 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 3193 | ` *       g - shorter of %e and %f.` |
|      - | 3194 | ` *       G - shorter of %E and %f.` |
|      - | 3195 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 3196 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 3197 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 3198 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 3199 | ` */` |
|      - | 3200 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3201 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 3202 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 3203 | `/*` |
|      - | 3204 | `** Conversion types fall into various categories as defined by the` |
|      - | 3205 | `** following enumeration.` |
|      - | 3206 | `*/` |
|      - | 3207 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 3208 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 3209 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 3210 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 3211 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 3212 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 3213 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 3214 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 3215 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 3216 |  |
|      - | 3217 | `/*` |
|      - | 3218 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 3219 | `*/` |
|      - | 3220 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 3221 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 3222 | `/*` |
|      - | 3223 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 3224 | `** by an instance of the following structure` |
|      - | 3225 | `*/` |
|      - | 3226 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 3227 | `struct ph7_fmt_info` |
|      - | 3228 | `{` |
|      - | 3229 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 3230 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 3231 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 3232 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 3233 | `  char *charset; /* The character set for conversion */` |
|      - | 3234 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 3235 | `};` |
|      - | 3236 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3237 | `/*` |
|      - | 3238 | `** "*val" is a double such that 0.1 <= *val < 10.0` |
|      - | 3239 | `** Return the ascii code for the leading digit of *val, then` |
|      - | 3240 | `** multiply "*val" by 10.0 to renormalize.` |
|      - | 3241 | `**` |
|      - | 3242 | `** Example:` |
|      - | 3243 | `**     input:     *val = 3.14159` |
|      - | 3244 | `**     output:    *val = 1.4159    function return = '3'` |
|      - | 3245 | `**` |
|      - | 3246 | `** The counter *cnt is incremented each time.  After counter exceeds` |
|      - | 3247 | `** 16 (the number of significant digits in a 64-bit float) '0' is` |
|      - | 3248 | `** always returned.` |
|      - | 3249 | `*/` |
|    422 | 3250 | `static int vxGetdigit(sxlongreal *val,int *cnt)` |
|      1 | 3251 | `{` |
|      - | 3252 | `  sxlongreal d;` |
|      - | 3253 | `  int digit;` |
|      - | 3254 |  |
|    423 | 3255 | `  if( (*cnt)++ >= 16 ){` |
|    ! 0 | 3256 | `	  return '0';` |
|      - | 3257 | `  }` |
|    423 | 3258 | `  digit = (int)*val;` |
|    423 | 3259 | `  d = digit;` |
|    423 | 3260 | `   *val = (*val - d)*10.0;` |
|    423 | 3261 | `  return digit + '0' ;` |
|    212 | 3262 | `}` |
|      - | 3263 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      - | 3264 | `/*` |
|      - | 3265 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 3266 | ` * used conversion types first.` |
|      - | 3267 | ` */` |
|      - | 3268 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 3269 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 3270 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 3271 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 3272 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 3273 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 3274 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 3275 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 3276 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 3277 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3278 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3279 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 3280 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 3281 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 3282 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 3283 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 3284 | `};` |
|      - | 3285 | `/*` |
|      - | 3286 | ` * Format a given string.` |
|      - | 3287 | ` * The root program.  All variations call this core.` |
|      - | 3288 | ` * INPUTS:` |
|      - | 3289 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 3290 | ` *            1. A pointer to the call context.` |
|      - | 3291 | ` *            2. A pointer to the list of characters to be output` |
|      - | 3292 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 3293 | ` *            3. An integer number of characters to be output.` |
|      - | 3294 | ` *               (Note: This number might be zero.)` |
|      - | 3295 | ` *            4. Upper layer private data.` |
|      - | 3296 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 3297 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 3298 | ` */` |
|    260 | 3299 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 3300 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 3301 | `	ph7_context *pCtx,  /* call context */` |
|      - | 3302 | `	const char *zIn,    /* Format string */` |
|      - | 3303 | `	int nByte,          /* Format string length */` |
|      - | 3304 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 3305 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 3306 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 3307 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 3308 | `	)` |
|      1 | 3309 | `{` |
|    261 | 3310 | `	char spaces[] = "                                                  ";` |
|      - | 3311 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    261 | 3312 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 3313 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 3314 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 3315 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 3316 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 3317 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 3318 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 3319 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 3320 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 3321 | `	ph7_int64 iVal;` |
|      - | 3322 | `	int precision;           /* Precision of the current field */` |
|      - | 3323 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 3324 | `	int c,rc,n;` |
|      - | 3325 | `	int length;              /* Length of the field */` |
|      - | 3326 | `	int prefix;` |
|      - | 3327 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 3328 | `	int width;               /* Width of the current field */` |
|      - | 3329 | `	int idx;` |
|    261 | 3330 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 3331 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 3332 | `	/* Start the format process */` |
|    380 | 3333 | `	for(;;){` |
|    761 | 3334 | `		zCur = zIn;` |
|   2785 | 3335 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|   2025 | 3336 | `			zIn++;` |
|      1 | 3337 | `		}` |
|    761 | 3338 | `		if( zCur < zIn ){` |
|      - | 3339 | `			/* Consume chunk verbatim */` |
|    539 | 3340 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|    539 | 3341 | `			if( rc != SXRET_OK ){` |
|      - | 3342 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|    ! 0 | 3343 | `				break;` |
|      - | 3344 | `			}` |
|    269 | 3345 | `		}` |
|    761 | 3346 | `		if( zIn >= zEnd ){` |
|      - | 3347 | `			/* No more input to process,break immediately */` |
|    259 | 3348 | `			break;` |
|      - | 3349 | `		}` |
|      - | 3350 | `		/* Find out what flags are present */` |
|    503 | 3351 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    502 | 3352 | `			flag_alternateform = flag_zeropad = 0;` |
|    503 | 3353 | `		zIn++; /* Jump the precent sign */` |
|    251 | 3354 | `		do{` |
|    535 | 3355 | `			c = zIn[0];` |
|    535 | 3356 | `			switch( c ){` |
|      9 | 3357 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      3 | 3358 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 3359 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|      9 | 3360 | `			case '#':   flag_alternateform = 1;   c = 0;   break;` |
|      9 | 3361 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 3362 | `			case '\'':` |
|    ! 0 | 3363 | `				zIn++;` |
|    ! 0 | 3364 | `				if( zIn < zEnd ){` |
|      - | 3365 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 3366 | `					c = zIn[0];` |
|    ! 0 | 3367 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3368 | `						spaces[idx] = (char)c;` |
|    ! 0 | 3369 | `					}` |
|    ! 0 | 3370 | `					c = 0;` |
|    ! 0 | 3371 | `				}` |
|    ! 0 | 3372 | `				break;` |
|    502 | 3373 | `			default:                                       break;` |
|      - | 3374 | `			}` |
|    535 | 3375 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 3376 | `		/* Get the field width */` |
|    503 | 3377 | `		width = 0;` |
|    788 | 3378 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     35 | 3379 | `			width = width*10 + (zIn[0] - '0');` |
|     35 | 3380 | `			zIn++;` |
|      1 | 3381 | `		}` |
|    503 | 3382 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 3383 | `			/* Position specifer */` |
|    ! 0 | 3384 | `			if( width > 0 ){` |
|    ! 0 | 3385 | `				n = width;` |
|    ! 0 | 3386 | `				if( vf && n > 0 ){` |
|    ! 0 | 3387 | `					n--;` |
|    ! 0 | 3388 | `				}` |
|    ! 0 | 3389 | `			}` |
|    ! 0 | 3390 | `			zIn++;` |
|    ! 0 | 3391 | `			width = 0;` |
|    ! 0 | 3392 | `			if( zIn < zEnd && zIn[0] == '0' ){` |
|    ! 0 | 3393 | `				flag_zeropad = 1;` |
|    ! 0 | 3394 | `				zIn++;` |
|    ! 0 | 3395 | `			}` |
|    ! 0 | 3396 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 3397 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 3398 | `				zIn++;` |
|    ! 0 | 3399 | `			}` |
|    ! 0 | 3400 | `		}` |
|    503 | 3401 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 3402 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 3403 | `		}` |
|      - | 3404 | `		/* Get the precision */` |
|    503 | 3405 | `		precision = -1;` |
|    503 | 3406 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     59 | 3407 | `			precision = 0;` |
|     59 | 3408 | `			zIn++;` |
|    150 | 3409 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     63 | 3410 | `				precision = precision*10 + (zIn[0] - '0');` |
|     63 | 3411 | `				zIn++;` |
|      1 | 3412 | `			}` |
|     29 | 3413 | `		}` |
|    503 | 3414 | `		if( zIn >= zEnd ){` |
|      - | 3415 | `			/* No more input */` |
|      3 | 3416 | `			break;` |
|      - | 3417 | `		}` |
|      - | 3418 | `		/* Fetch the info entry for the field */` |
|    501 | 3419 | `		pInfo = 0;` |
|    501 | 3420 | `		xtype = PH7_FMT_ERROR;` |
|    501 | 3421 | `		c = zIn[0];` |
|    501 | 3422 | `		zIn++; /* Jump the format specifer */` |
|   1439 | 3423 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|   1437 | 3424 | `			if( c==aFmt[idx].fmttype ){` |
|    499 | 3425 | `				pInfo = &aFmt[idx];` |
|    499 | 3426 | `				xtype = pInfo->type;` |
|    499 | 3427 | `				break;` |
|      - | 3428 | `			}` |
|    470 | 3429 | `		}` |
|    501 | 3430 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    501 | 3431 | `		length = 0;` |
|      - | 3432 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 3433 | `		 /*` |
|      - | 3434 | `		  ** At this point, variables are initialized as follows:` |
|      - | 3435 | `		  **` |
|      - | 3436 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 3437 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 3438 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 3439 | `		  **                               field width was negative.` |
|      - | 3440 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 3441 | `		  **                               the conversion character.` |
|      - | 3442 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 3443 | `		  **   width                       The specified field width.  This is` |
|      - | 3444 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 3445 | `		  **   precision                   The specified precision.  The default` |
|      - | 3446 | `		  **                               is -1.` |
|      - | 3447 | `		  */` |
|    501 | 3448 | `		switch(xtype){` |
|    ! 0 | 3449 | `		case PH7_FMT_PERCENT:` |
|      - | 3450 | `			/* A literal percent character */` |
|    ! 0 | 3451 | `			zWorker[0] = '%';` |
|    ! 0 | 3452 | `			length = (int)sizeof(char);` |
|    ! 0 | 3453 | `			break;` |
|      3 | 3454 | `		case PH7_FMT_CHARX:` |
|      - | 3455 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 3456 | `			 * with that ASCII value` |
|      - | 3457 | `			 */` |
|      7 | 3458 | `			pArg = NEXT_ARG;` |
|      7 | 3459 | `			if( pArg == 0 ){` |
|      3 | 3460 | `				c = 0;` |
|      2 | 3461 | `			}else{` |
|      5 | 3462 | `				c = ph7_value_to_int(pArg);` |
|      - | 3463 | `			}` |
|      - | 3464 | `			/* NUL byte is an acceptable value */` |
|      7 | 3465 | `			zWorker[0] = (char)c;` |
|      7 | 3466 | `			length = (int)sizeof(char);` |
|      7 | 3467 | `			break;` |
|    159 | 3468 | `		case PH7_FMT_STRING:` |
|      - | 3469 | `			/* the argument is treated as and presented as a string */` |
|    319 | 3470 | `			pArg = NEXT_ARG;` |
|    319 | 3471 | `			if( pArg == 0 ){` |
|    ! 0 | 3472 | `				length = 0;` |
|    ! 0 | 3473 | `			}else{` |
|    319 | 3474 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 3475 | `			}` |
|    319 | 3476 | `			if( length < 1 ){` |
|    ! 0 | 3477 | `				zBuf = " ";` |
|    ! 0 | 3478 | `				length = (int)sizeof(char);` |
|    ! 0 | 3479 | `			}` |
|    319 | 3480 | `			if( precision>=0 && precision<length ){` |
|      3 | 3481 | `				length = precision;` |
|      1 | 3482 | `			}` |
|    319 | 3483 | `			if( flag_zeropad ){` |
|      - | 3484 | `				/* zero-padding works on strings too */` |
|    ! 0 | 3485 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3486 | `					spaces[idx] = '0';` |
|    ! 0 | 3487 | `				}` |
|    ! 0 | 3488 | `			}` |
|    319 | 3489 | `			break;` |
|     59 | 3490 | `		case PH7_FMT_RADIX:` |
|    119 | 3491 | `			pArg = NEXT_ARG;` |
|    119 | 3492 | `			if( pArg == 0 ){` |
|    ! 0 | 3493 | `				iVal = 0;` |
|    ! 0 | 3494 | `			}else{` |
|    119 | 3495 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 3496 | `			}` |
|      - | 3497 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|    119 | 3498 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 3499 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 3500 | `			}` |
|      - | 3501 | `#if 1` |
|      - | 3502 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 3503 | `        ** I think this is stupid.*/` |
|    119 | 3504 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 3505 | `#else` |
|      - | 3506 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 3507 | `        ** but leave the prefix for hex.*/` |
|      - | 3508 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 3509 | `#endif` |
|    119 | 3510 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|     89 | 3511 | `          if( iVal<0 ){` |
|     25 | 3512 | `            iVal = -iVal;` |
|      - | 3513 | `			/* Ticket 1433-003 */` |
|     25 | 3514 | `			if( iVal < 0 ){` |
|      - | 3515 | `				/* Overflow */` |
|    ! 0 | 3516 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3517 | `			}` |
|     25 | 3518 | `            prefix = '-';` |
|     77 | 3519 | `          }else if( flag_plussign )  prefix = '+';` |
|     63 | 3520 | `          else if( flag_blanksign )  prefix = ' ';` |
|     61 | 3521 | `          else                       prefix = 0;` |
|     45 | 3522 | `        }else{` |
|     31 | 3523 | `			if( iVal<0 ){` |
|    ! 0 | 3524 | `				iVal = -iVal;` |
|      - | 3525 | `				/* Ticket 1433-003 */` |
|    ! 0 | 3526 | `				if( iVal < 0 ){` |
|      - | 3527 | `					/* Overflow */` |
|    ! 0 | 3528 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3529 | `				}` |
|    ! 0 | 3530 | `			}` |
|     31 | 3531 | `			prefix = 0;` |
|      - | 3532 | `		}` |
|    119 | 3533 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      7 | 3534 | `          precision = width-(prefix!=0);` |
|      3 | 3535 | `        }` |
|    119 | 3536 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 3537 | `        {` |
|      - | 3538 | `          register char *cset;      /* Use registers for speed */` |
|      - | 3539 | `          register int base;` |
|    119 | 3540 | `          cset = pInfo->charset;` |
|    119 | 3541 | `          base = pInfo->base;` |
|     59 | 3542 | `          do{                                           /* Convert to ascii */` |
|    187 | 3543 | `            *(--zBuf) = cset[iVal%base];` |
|    187 | 3544 | `            iVal = iVal/base;` |
|    187 | 3545 | `          }while( iVal>0 );` |
|      - | 3546 | `        }` |
|    119 | 3547 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    141 | 3548 | `        for(idx=precision-length; idx>0; idx--){` |
|     23 | 3549 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|     12 | 3550 | `        }` |
|    119 | 3551 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|    119 | 3552 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 3553 | `          char *pre, x;` |
|      9 | 3554 | `          pre = pInfo->prefix;` |
|      9 | 3555 | `          if( *zBuf!=pre[0] ){` |
|     23 | 3556 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|      4 | 3557 | `          }` |
|      4 | 3558 | `        }` |
|    119 | 3559 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    119 | 3560 | `		break;` |
|     28 | 3561 | `		case PH7_FMT_FLOAT:` |
|      - | 3562 | `		case PH7_FMT_EXP:` |
|      - | 3563 | `		case PH7_FMT_GENERIC:{` |
|      - | 3564 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3565 | `		long double realvalue;` |
|      - | 3566 | `		int  exp;                /* exponent of real numbers */` |
|      - | 3567 | `		double rounder;          /* Used for rounding floating point values */` |
|      - | 3568 | `		int flag_dp;            /* True if decimal point should be shown */` |
|      - | 3569 | `		int flag_rtz;           /* True if trailing zeros should be removed */` |
|      - | 3570 | `		int flag_exp;           /* True to force display of the exponent */` |
|      - | 3571 | `		int nsd;                 /* Number of significant digits returned */` |
|     57 | 3572 | `		pArg = NEXT_ARG;` |
|     57 | 3573 | `		if( pArg == 0 ){` |
|    ! 0 | 3574 | `			realvalue = 0;` |
|    ! 0 | 3575 | `		}else{` |
|     57 | 3576 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 3577 | `		}` |
|      - | 3578 | `		/* Special-case NaN and infinities since the normal formatting logic` |
|      - | 3579 | `		 * below assumes a finite positive realvalue. */` |
|     57 | 3580 | `		if( PH7_IS_NAN(realvalue) ){` |
|    ! 0 | 3581 | `			zBuf = "NAN";` |
|    ! 0 | 3582 | `			length = 3;` |
|    ! 0 | 3583 | `			break;` |
|      - | 3584 | `		}` |
|     57 | 3585 | `		if( PH7_IS_INF(realvalue) ){` |
|      - | 3586 | `			/* Infinity prints as INF or -INF depending on sign. */` |
|    ! 0 | 3587 | `			if( realvalue < 0.0 ){` |
|    ! 0 | 3588 | `				zBuf = "-INF";` |
|    ! 0 | 3589 | `				length = 4;` |
|    ! 0 | 3590 | `			}else{` |
|    ! 0 | 3591 | `				zBuf = "INF";` |
|    ! 0 | 3592 | `				length = 3;` |
|      - | 3593 | `			}` |
|    ! 0 | 3594 | `			break;` |
|      - | 3595 | `		}` |
|     57 | 3596 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|     57 | 3597 | `		if( precision>PH7_FMT_BUFSIZ-40) precision = PH7_FMT_BUFSIZ-40;` |
|     57 | 3598 | `        if( realvalue<0.0 ){` |
|      3 | 3599 | `          realvalue = -realvalue;` |
|      3 | 3600 | `          prefix = '-';` |
|      2 | 3601 | `        }else{` |
|     55 | 3602 | `          if( flag_plussign )          prefix = '+';` |
|     55 | 3603 | `          else if( flag_blanksign )    prefix = ' ';` |
|     55 | 3604 | `          else                         prefix = 0;` |
|      - | 3605 | `        }` |
|     57 | 3606 | `        if( pInfo->type==PH7_FMT_GENERIC && precision>0 ) precision--;` |
|     57 | 3607 | `        rounder = 0.0;` |
|      - | 3608 | `#if 0` |
|      - | 3609 | `        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */` |
|      - | 3610 | `        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);` |
|      - | 3611 | `#else` |
|      - | 3612 | `        /* It makes more sense to use 0.5 */` |
|    405 | 3613 | `        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);` |
|      - | 3614 | `#endif` |
|     57 | 3615 | `        if( pInfo->type==PH7_FMT_FLOAT ) realvalue += rounder;` |
|      - | 3616 | `        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */` |
|     57 | 3617 | `        exp = 0;` |
|     57 | 3618 | `        if( realvalue>0.0 ){` |
|     61 | 3619 | `          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }` |
|     89 | 3620 | `          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }` |
|     61 | 3621 | `          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }` |
|     71 | 3622 | `          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }` |
|     57 | 3623 | `          if( exp>350 \|\| exp<-350 ){` |
|    ! 0 | 3624 | `            zBuf = "NaN";` |
|    ! 0 | 3625 | `            length = 3;` |
|    ! 0 | 3626 | `            break;` |
|      - | 3627 | `          }` |
|     28 | 3628 | `        }` |
|     57 | 3629 | `        zBuf = zWorker;` |
|      - | 3630 | `        /*` |
|      - | 3631 | `        ** If the field type is etGENERIC, then convert to either etEXP` |
|      - | 3632 | `        ** or etFLOAT, as appropriate.` |
|      - | 3633 | `        */` |
|     57 | 3634 | `        flag_exp = xtype==PH7_FMT_EXP;` |
|     57 | 3635 | `        if( xtype!=PH7_FMT_FLOAT ){` |
|    ! 0 | 3636 | `          realvalue += rounder;` |
|    ! 0 | 3637 | `          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }` |
|    ! 0 | 3638 | `        }` |
|     57 | 3639 | `        if( xtype==PH7_FMT_GENERIC ){` |
|    ! 0 | 3640 | `          flag_rtz = !flag_alternateform;` |
|    ! 0 | 3641 | `          if( exp<-4 \|\| exp>precision ){` |
|    ! 0 | 3642 | `            xtype = PH7_FMT_EXP;` |
|    ! 0 | 3643 | `          }else{` |
|    ! 0 | 3644 | `            precision = precision - exp;` |
|    ! 0 | 3645 | `            xtype = PH7_FMT_FLOAT;` |
|      - | 3646 | `          }` |
|    ! 0 | 3647 | `        }else{` |
|     57 | 3648 | `          flag_rtz = 0;` |
|      - | 3649 | `        }` |
|      - | 3650 | `        /*` |
|      - | 3651 | `        ** The "exp+precision" test causes output to be of type etEXP if` |
|      - | 3652 | `        ** the precision is too large to fit in buf[].` |
|      - | 3653 | `        */` |
|     57 | 3654 | `        nsd = 0;` |
|     57 | 3655 | `        if( xtype==PH7_FMT_FLOAT && exp+precision<PH7_FMT_BUFSIZ-30 ){` |
|     57 | 3656 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|     57 | 3657 | `          if( prefix ) *(zBuf++) = (char)prefix;         /* Sign */` |
|     57 | 3658 | `          if( exp<0 )  *(zBuf++) = '0';            /* Digits before "." */` |
|    149 | 3659 | `          else for(; exp>=0; exp--) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     57 | 3660 | `          if( flag_dp ) *(zBuf++) = '.';           /* The decimal point */` |
|     89 | 3661 | `          for(exp++; exp<0 && precision>0; precision--, exp++){` |
|     33 | 3662 | `            *(zBuf++) = '0';` |
|     17 | 3663 | `          }` |
|    373 | 3664 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     57 | 3665 | `          *(zBuf--) = 0;                           /* Null terminate */` |
|     57 | 3666 | `          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */` |
|    ! 0 | 3667 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 3668 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 3669 | `          }` |
|     57 | 3670 | `          zBuf++;                            /* point to next free slot */` |
|     29 | 3671 | `        }else{    /* etEXP or etGENERIC */` |
|    ! 0 | 3672 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|    ! 0 | 3673 | `          if( prefix ) *(zBuf++) = (char)prefix;   /* Sign */` |
|    ! 0 | 3674 | `          *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);  /* First digit */` |
|    ! 0 | 3675 | `          if( flag_dp ) *(zBuf++) = '.';     /* Decimal point */` |
|    ! 0 | 3676 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|    ! 0 | 3677 | `          zBuf--;                            /* point to last digit */` |
|    ! 0 | 3678 | `          if( flag_rtz && flag_dp ){          /* Remove tail zeros */` |
|    ! 0 | 3679 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 3680 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 3681 | `          }` |
|    ! 0 | 3682 | `          zBuf++;                            /* point to next free slot */` |
|    ! 0 | 3683 | `          if( exp \|\| flag_exp ){` |
|    ! 0 | 3684 | `            *(zBuf++) = pInfo->charset[0];` |
|    ! 0 | 3685 | `            if( exp<0 ){ *(zBuf++) = '-'; exp = -exp; } /* sign of exp */` |
|    ! 0 | 3686 | `            else       { *(zBuf++) = '+'; }` |
|    ! 0 | 3687 | `            if( exp>=100 ){` |
|    ! 0 | 3688 | `              *(zBuf++) = (char)((exp/100)+'0');                /* 100's digit */` |
|    ! 0 | 3689 | `              exp %= 100;` |
|    ! 0 | 3690 | `            }` |
|    ! 0 | 3691 | `            *(zBuf++) = (char)(exp/10+'0');                     /* 10's digit */` |
|    ! 0 | 3692 | `            *(zBuf++) = (char)(exp%10+'0');                     /* 1's digit */` |
|    ! 0 | 3693 | `          }` |
|      - | 3694 | `        }` |
|      - | 3695 | `        /* The converted number is in buf[] and zero terminated.Output it.` |
|      - | 3696 | `        ** Note that the number is in the usual order, not reversed as with` |
|      - | 3697 | `        ** integer conversions.*/` |
|     57 | 3698 | `        length = (int)(zBuf-zWorker);` |
|     57 | 3699 | `        zBuf = zWorker;` |
|      - | 3700 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 3701 | `        ** set and we are not left justified */` |
|     57 | 3702 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 3703 | `          int i;` |
|      3 | 3704 | `          int nPad = width - length;` |
|     13 | 3705 | `          for(i=width; i>=nPad; i--){` |
|     11 | 3706 | `            zBuf[i] = zBuf[i-nPad];` |
|      6 | 3707 | `          }` |
|      3 | 3708 | `          i = prefix!=0;` |
|      5 | 3709 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      3 | 3710 | `          length = width;` |
|      1 | 3711 | `        }` |
|      - | 3712 | `#else` |
|      - | 3713 | `         zBuf = " ";` |
|      - | 3714 | `		 length = (int)sizeof(char);` |
|      - | 3715 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|     57 | 3716 | `		 break;` |
|      - | 3717 | `							 }` |
|      1 | 3718 | `		default:` |
|      - | 3719 | `			/* Invalid format specifer */` |
|      3 | 3720 | `			zWorker[0] = '?';` |
|      3 | 3721 | `			length = (int)sizeof(char);` |
|      2 | 3722 | `			break;` |
|      - | 3723 | `		}` |
|      - | 3724 | `		 /*` |
|      - | 3725 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 3726 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 3727 | `		 ** the output.` |
|      - | 3728 | `		 */` |
|    501 | 3729 | `    if( !flag_leftjustify ){` |
|      - | 3730 | `      register int nspace;` |
|    493 | 3731 | `      nspace = width-length;` |
|    493 | 3732 | `      if( nspace>0 ){` |
|      5 | 3733 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3734 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3735 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3736 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3737 | `			}` |
|    ! 0 | 3738 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3739 | `        }` |
|      5 | 3740 | `        if( nspace>0 ){` |
|      5 | 3741 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      5 | 3742 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3743 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3744 | `			}` |
|      2 | 3745 | `		}` |
|      2 | 3746 | `      }` |
|    246 | 3747 | `    }` |
|    501 | 3748 | `    if( length>0 ){` |
|    501 | 3749 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    501 | 3750 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3751 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3752 | `		}` |
|    250 | 3753 | `    }` |
|    501 | 3754 | `    if( flag_leftjustify ){` |
|      - | 3755 | `      register int nspace;` |
|      9 | 3756 | `      nspace = width-length;` |
|      9 | 3757 | `      if( nspace>0 ){` |
|      9 | 3758 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3759 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3760 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3761 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3762 | `			}` |
|    ! 0 | 3763 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3764 | `        }` |
|      9 | 3765 | `        if( nspace>0 ){` |
|      9 | 3766 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      9 | 3767 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3768 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3769 | `			}` |
|      4 | 3770 | `		}` |
|      4 | 3771 | `      }` |
|      4 | 3772 | `    }` |
|      1 | 3773 | ` }/* for(;;) */` |
|    261 | 3774 | `	return SXRET_OK;` |
|    131 | 3775 | `}` |
|      - | 3776 | `/*` |
|      - | 3777 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 3778 | ` */` |
|     90 | 3779 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 3780 | `{` |
|      - | 3781 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|      - | 3782 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|      - | 3783 | `	 * non-OK rc also stops the format loop. */` |
|     91 | 3784 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|     91 | 3785 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|     91 | 3786 | `	return *pRc;` |
|      1 | 3787 | `}` |
|      - | 3788 | `/*` |
|      - | 3789 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 3790 | ` *  Return a formatted string.` |
|      - | 3791 | ` * Parameters` |
|      - | 3792 | ` *  $format` |
|      - | 3793 | ` *    The format string (see block comment above)` |
|      - | 3794 | ` * Return` |
|      - | 3795 | ` *  A string produced according to the formatting string format.` |
|      - | 3796 | ` */` |
|     62 | 3797 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3798 | `{` |
|      - | 3799 | `	const char *zFormat;` |
|     63 | 3800 | `	sxi32 rc = SXRET_OK;` |
|      - | 3801 | `	int nLen;` |
|     63 | 3802 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3803 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 3804 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3805 | `		return PH7_OK;` |
|      - | 3806 | `	}` |
|      - | 3807 | `	/* Extract the string format */` |
|     61 | 3808 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     61 | 3809 | `	if( nLen < 1 ){` |
|      - | 3810 | `		/* Empty string */` |
|    ! 0 | 3811 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3812 | `		return PH7_OK;` |
|      - | 3813 | `	}` |
|      - | 3814 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|     61 | 3815 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|     61 | 3816 | `	if( rc != SXRET_OK ){` |
|      - | 3817 | `		/* The result append ran out of memory: raise a fatal rather than` |
|      - | 3818 | `		 * returning a silently-truncated string. */` |
|    ! 0 | 3819 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 3820 | `	}` |
|     61 | 3821 | `	return PH7_OK;` |
|     32 | 3822 | `}` |
|      - | 3823 | `/*` |
|      - | 3824 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 3825 | ` */` |
|    922 | 3826 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 3827 | `{` |
|    923 | 3828 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 3829 | `	/* Call the VM output consumer directly */` |
|    923 | 3830 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 3831 | `	/* Increment counter */` |
|    923 | 3832 | `	*pCounter += nLen;` |
|    923 | 3833 | `	return PH7_OK;` |
|      1 | 3834 | `}` |
|      - | 3835 | `/*` |
|      - | 3836 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 3837 | ` *  Output a formatted string.` |
|      - | 3838 | ` * Parameters` |
|      - | 3839 | ` *  $format` |
|      - | 3840 | ` *   See sprintf() for a description of format.` |
|      - | 3841 | ` * Return` |
|      - | 3842 | ` *  The length of the outputted string.` |
|      - | 3843 | ` */` |
|    176 | 3844 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3845 | `{` |
|    177 | 3846 | `	ph7_int64 nCounter = 0;` |
|      - | 3847 | `	const char *zFormat;` |
|      - | 3848 | `	int nLen;` |
|    177 | 3849 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3850 | `		/* Missing/Invalid arguments,return 0 */` |
|      3 | 3851 | `		ph7_result_int(pCtx,0);` |
|      3 | 3852 | `		return PH7_OK;` |
|      - | 3853 | `	}` |
|      - | 3854 | `	/* Extract the string format */` |
|    175 | 3855 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    175 | 3856 | `	if( nLen < 1 ){` |
|      - | 3857 | `		/* Empty string */` |
|    ! 0 | 3858 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3859 | `		return PH7_OK;` |
|      - | 3860 | `	}` |
|      - | 3861 | `	/* Format the string */` |
|    175 | 3862 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 3863 | `	/* Return the length of the outputted string */` |
|    175 | 3864 | `	ph7_result_int64(pCtx,nCounter);` |
|    175 | 3865 | `	return PH7_OK;` |
|     89 | 3866 | `}` |
|      - | 3867 | `/*` |
|      - | 3868 | ` * int vprintf(string $format,array $args)` |
|      - | 3869 | ` *  Output a formatted string.` |
|      - | 3870 | ` * Parameters` |
|      - | 3871 | ` *  $format` |
|      - | 3872 | ` *   See sprintf() for a description of format.` |
|      - | 3873 | ` * Return` |
|      - | 3874 | ` *  The length of the outputted string.` |
|      - | 3875 | ` */` |
|      2 | 3876 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3877 | `{` |
|      3 | 3878 | `	ph7_int64 nCounter = 0;` |
|      - | 3879 | `	const char *zFormat;` |
|      - | 3880 | `	ph7_hashmap *pMap;` |
|      - | 3881 | `	SySet sArg;` |
|      - | 3882 | `	int nLen,n;` |
|      3 | 3883 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 3884 | `		/* Missing/Invalid arguments,return 0 */` |
|    ! 0 | 3885 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3886 | `		return PH7_OK;` |
|      - | 3887 | `	}` |
|      - | 3888 | `	/* Extract the string format */` |
|      3 | 3889 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3890 | `	if( nLen < 1 ){` |
|      - | 3891 | `		/* Empty string */` |
|    ! 0 | 3892 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3893 | `		return PH7_OK;` |
|      - | 3894 | `	}` |
|      - | 3895 | `	/* Point to the hashmap */` |
|      3 | 3896 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 3897 | `	/* Extract arguments from the hashmap */` |
|      3 | 3898 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 3899 | `	/* Format the string */` |
|      3 | 3900 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 3901 | `	/* Return the length of the outputted string */` |
|      3 | 3902 | `	ph7_result_int64(pCtx,nCounter);` |
|      - | 3903 | `	/* Release the container */` |
|      3 | 3904 | `	SySetRelease(&sArg);` |
|      3 | 3905 | `	return PH7_OK;` |
|      2 | 3906 | `}` |
|      - | 3907 | `/*` |
|      - | 3908 | ` * int vsprintf(string $format,array $args)` |
|      - | 3909 | ` *  Output a formatted string.` |
|      - | 3910 | ` * Parameters` |
|      - | 3911 | ` *  $format` |
|      - | 3912 | ` *   See sprintf() for a description of format.` |
|      - | 3913 | ` * Return` |
|      - | 3914 | ` *  A string produced according to the formatting string format.` |
|      - | 3915 | ` */` |
|     10 | 3916 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3917 | `{` |
|      - | 3918 | `	const char *zFormat;` |
|      - | 3919 | `	ph7_hashmap *pMap;` |
|      - | 3920 | `	SySet sArg;` |
|     11 | 3921 | `	sxi32 rc = SXRET_OK;` |
|      - | 3922 | `	int nLen,n;` |
|     11 | 3923 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 3924 | `		/* Missing/Invalid arguments,return the empty string */` |
|      5 | 3925 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 3926 | `		return PH7_OK;` |
|      - | 3927 | `	}` |
|      - | 3928 | `	/* Extract the string format */` |
|      7 | 3929 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 3930 | `	if( nLen < 1 ){` |
|      - | 3931 | `		/* Empty string */` |
|    ! 0 | 3932 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3933 | `		return PH7_OK;` |
|      - | 3934 | `	}` |
|      - | 3935 | `	/* Point to hashmap */` |
|      7 | 3936 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 3937 | `	/* Extract arguments from the hashmap */` |
|      7 | 3938 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 3939 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|      7 | 3940 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|      - | 3941 | `	/* Release the container */` |
|      7 | 3942 | `	SySetRelease(&sArg);` |
|      7 | 3943 | `	if( rc != SXRET_OK ){` |
|      - | 3944 | `		/* The result append ran out of memory: raise a fatal. */` |
|    ! 0 | 3945 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 3946 | `	}` |
|      7 | 3947 | `	return PH7_OK;` |
|      6 | 3948 | `}` |
|      - | 3949 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 3950 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 3951 | `/*` |
|      - | 3952 | ` * Symisc eXtension.` |
|      - | 3953 | ` * string size_format(int64 $size)` |
|      - | 3954 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 3955 | ` *  Example:` |
|      - | 3956 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 3957 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 3958 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 3959 | ` * Parameter` |
|      - | 3960 | ` *  $size` |
|      - | 3961 | ` *    Entity size in bytes.` |
|      - | 3962 | ` * Return` |
|      - | 3963 | ` *   Formatted string representation of the given size.` |
|      - | 3964 | ` */` |
|     24 | 3965 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3966 | `{` |
|      - | 3967 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 3968 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 3969 | `	sxi32 nRest,i_32;` |
|      - | 3970 | `	ph7_int64 iSize;` |
|     25 | 3971 | `	int c = -1; /* index in zUnit[] */` |
|      - | 3972 |  |
|     25 | 3973 | `	if( nArg < 1 ){` |
|      - | 3974 | `		/* Missing argument,return the empty string */` |
|      3 | 3975 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3976 | `		return PH7_OK;` |
|      - | 3977 | `	}` |
|      - | 3978 | `	/* Extract the given size */` |
|     23 | 3979 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 3980 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 3981 | `		/* Don't bother formatting,return immediately */` |
|      5 | 3982 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 3983 | `		return PH7_OK;` |
|      - | 3984 | `	}` |
|     19 | 3985 | `	for(;;){` |
|     39 | 3986 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 3987 | `		iSize >>= 10;` |
|     39 | 3988 | `		c++;` |
|     39 | 3989 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 3990 | `			break;` |
|      - | 3991 | `		}` |
|      1 | 3992 | `	}` |
|     19 | 3993 | `	nRest /= 100;` |
|     19 | 3994 | `	if( nRest > 9 ){` |
|    ! 0 | 3995 | `		nRest = 9;` |
|    ! 0 | 3996 | `	}` |
|     19 | 3997 | `	if( iSize > 999 ){` |
|    ! 0 | 3998 | `		c++;` |
|    ! 0 | 3999 | `		nRest = 9;` |
|    ! 0 | 4000 | `		iSize = 0;` |
|    ! 0 | 4001 | `	}` |
|     19 | 4002 | `	i_32 = (sxi32)iSize;` |
|      - | 4003 | `	/* Format */` |
|     19 | 4004 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 4005 | `	return PH7_OK;` |
|     13 | 4006 | `}` |
|      - | 4007 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4008 | `/*` |
|      - | 4009 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 4010 | ` *   Calculate the md5 hash of a string.` |
|      - | 4011 | ` * Parameter` |
|      - | 4012 | ` *  $str` |
|      - | 4013 | ` *   Input string` |
|      - | 4014 | ` * $raw_output` |
|      - | 4015 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4016 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4017 | ` * Return` |
|      - | 4018 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 4019 | ` */` |
|     14 | 4020 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4021 | `{` |
|      - | 4022 | `	unsigned char zDigest[16];` |
|     15 | 4023 | `	int raw_output = FALSE;` |
|      - | 4024 | `	const void *pIn;` |
|      - | 4025 | `	int nLen;` |
|     15 | 4026 | `	if( nArg < 1 ){` |
|      - | 4027 | `		/* Missing arguments,return the empty string */` |
|      3 | 4028 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4029 | `		return PH7_OK;` |
|      - | 4030 | `	}` |
|      - | 4031 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 4032 | `	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */` |
|     13 | 4033 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 4034 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 4035 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 4036 | `	}` |
|      - | 4037 | `	/* Compute the MD5 digest */` |
|     13 | 4038 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|     13 | 4039 | `	if( raw_output ){` |
|      - | 4040 | `		/* Output raw digest */` |
|      5 | 4041 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 4042 | `	}else{` |
|      - | 4043 | `		/* Perform a binary to hex conversion */` |
|      9 | 4044 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4045 | `	}` |
|     13 | 4046 | `	return PH7_OK;` |
|      8 | 4047 | `}` |
|      - | 4048 | `/*` |
|      - | 4049 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 4050 | ` *   Calculate the sha1 hash of a string.` |
|      - | 4051 | ` * Parameter` |
|      - | 4052 | ` *  $str` |
|      - | 4053 | ` *   Input string` |
|      - | 4054 | ` * $raw_output` |
|      - | 4055 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4056 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4057 | ` * Return` |
|      - | 4058 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 4059 | ` */` |
|     12 | 4060 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4061 | `{` |
|      - | 4062 | `	unsigned char zDigest[20];` |
|     13 | 4063 | `	int raw_output = FALSE;` |
|      - | 4064 | `	const void *pIn;` |
|      - | 4065 | `	int nLen;` |
|     13 | 4066 | `	if( nArg < 1 ){` |
|      - | 4067 | `		/* Missing arguments,return the empty string */` |
|      3 | 4068 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4069 | `		return PH7_OK;` |
|      - | 4070 | `	}` |
|      - | 4071 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 4072 | `	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */` |
|     11 | 4073 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 4074 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 4075 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 4076 | `	}` |
|      - | 4077 | `	/* Compute the SHA1 digest */` |
|     11 | 4078 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|     11 | 4079 | `	if( raw_output ){` |
|      - | 4080 | `		/* Output raw digest */` |
|      5 | 4081 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 4082 | `	}else{` |
|      - | 4083 | `		/* Perform a binary to hex conversion */` |
|      7 | 4084 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4085 | `	}` |
|     11 | 4086 | `	return PH7_OK;` |
|      7 | 4087 | `}` |
|      - | 4088 | `/*` |
|      - | 4089 | ` * int64 crc32(string $str)` |
|      - | 4090 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 4091 | ` * Parameter` |
|      - | 4092 | ` *  $str` |
|      - | 4093 | ` *   Input string` |
|      - | 4094 | ` * Return` |
|      - | 4095 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 4096 | ` */` |
|      4 | 4097 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4098 | `{` |
|      - | 4099 | `	const void *pIn;` |
|      - | 4100 | `	sxu32 nCRC;` |
|      - | 4101 | `	int nLen;` |
|      5 | 4102 | `	if( nArg < 1 ){` |
|      - | 4103 | `		/* Missing arguments,return 0 */` |
|      3 | 4104 | `		ph7_result_int(pCtx,0);` |
|      3 | 4105 | `		return PH7_OK;` |
|      - | 4106 | `	}` |
|      - | 4107 | `	/* Extract the input string */` |
|      3 | 4108 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4109 | `	if( nLen < 1 ){` |
|      - | 4110 | `		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike` |
|      - | 4111 | `		 * md5()/sha1(), whose empty-string digests are non-zero. */` |
|    ! 0 | 4112 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4113 | `		return PH7_OK;` |
|      - | 4114 | `	}` |
|      - | 4115 | `	/* Calculate the sum */` |
|      3 | 4116 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 4117 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 4118 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 4119 | `	return PH7_OK;` |
|      3 | 4120 | `}` |
|      - | 4121 | `/*` |
|      - | 4122 | ` * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is` |
|      - | 4123 | ` * described by a small record so one dispatch (and one generic HMAC) serves them` |
|      - | 4124 | ` * all. Thin adapters normalize the differing context types and the reversed` |
|      - | 4125 | ` * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.` |
|      - | 4126 | ` */` |
|     11 | 4127 | `static void HashMd5Init(HashCtx *c){ MD5Init(&c->md5); }` |
|     15 | 4128 | `static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }` |
|     11 | 4129 | `static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }` |
|     11 | 4130 | `static void HashSha1Init(HashCtx *c){ SHA1Init(&c->sha1); }` |
|     15 | 4131 | `static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }` |
|     11 | 4132 | `static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }` |
|      9 | 4133 | `static void HashSha224Init(HashCtx *c){ SHA224Init(&c->sha256); }` |
|     33 | 4134 | `static void HashSha256Init(HashCtx *c){ SHA256Init(&c->sha256); }` |
|     57 | 4135 | `static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }` |
|     41 | 4136 | `static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }` |
|      9 | 4137 | `static void HashSha384Init(HashCtx *c){ SHA384Init(&c->sha512); }` |
|     15 | 4138 | `static void HashSha512Init(HashCtx *c){ SHA512Init(&c->sha512); }` |
|     27 | 4139 | `static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }` |
|     23 | 4140 | `static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }` |
|      - | 4141 | `typedef struct HashAlgo HashAlgo;` |
|      - | 4142 | `struct HashAlgo {` |
|      - | 4143 | `	const char *zName;   /* lowercase canonical name */` |
|      - | 4144 | `	int nDigestLen;      /* output bytes: 16/20/28/32/48/64 */` |
|      - | 4145 | `	int nBlockLen;       /* internal block bytes (for HMAC): 64 or 128 */` |
|      - | 4146 | `	void (*xInit)(HashCtx *);` |
|      - | 4147 | `	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);` |
|      - | 4148 | `	void (*xFinal)(HashCtx *,unsigned char *);` |
|      - | 4149 | `};` |
|      - | 4150 | `static const HashAlgo aHashAlgo[] = {` |
|      - | 4151 | `	{ "md5",    16, 64,  HashMd5Init,    HashMd5Update,    HashMd5Final    },` |
|      - | 4152 | `	{ "sha1",   20, 64,  HashSha1Init,   HashSha1Update,   HashSha1Final   },` |
|      - | 4153 | `	{ "sha224", 28, 64,  HashSha224Init, HashSha256Update, HashSha256Final },` |
|      - | 4154 | `	{ "sha256", 32, 64,  HashSha256Init, HashSha256Update, HashSha256Final },` |
|      - | 4155 | `	{ "sha384", 48, 128, HashSha384Init, HashSha512Update, HashSha512Final },` |
|      - | 4156 | `	{ "sha512", 64, 128, HashSha512Init, HashSha512Update, HashSha512Final },` |
|      - | 4157 | `};` |
|      - | 4158 | `/* Case-insensitive algorithm lookup (PHP accepts 'SHA256' etc.). */` |
|     73 | 4159 | `static const HashAlgo * HashFindAlgo(const char *zName,int nLen){` |
|      - | 4160 | `	sxu32 i;` |
|    279 | 4161 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|    272 | 4162 | `		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen` |
|    211 | 4163 | `			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){` |
|     67 | 4164 | `			return &aHashAlgo[i];` |
|      - | 4165 | `		}` |
|    106 | 4166 | `	}` |
|      6 | 4167 | `	return 0;` |
|     38 | 4168 | `}` |
|      - | 4169 | `/*` |
|      - | 4170 | ` * string hash(string $algo,string $data[,bool $binary = false])` |
|      - | 4171 | ` *   Generate a hash value (message digest).` |
|      - | 4172 | ` */` |
|     54 | 4173 | `static int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4174 | `{` |
|      - | 4175 | `	const HashAlgo *pAlgo;` |
|      - | 4176 | `	const char *zAlgo,*zData;` |
|     56 | 4177 | `	int nAlgoLen,nDataLen,raw_output = FALSE;` |
|      - | 4178 | `	HashCtx sCtx;` |
|      - | 4179 | `	unsigned char zDigest[64];` |
|     56 | 4180 | `	if( nArg < 2 ){` |
|    ! 0 | 4181 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4182 | `			"hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4183 | `	}` |
|     56 | 4184 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     56 | 4185 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     56 | 4186 | `	if( pAlgo == 0 ){` |
|      3 | 4187 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4188 | `			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - | 4189 | `	}` |
|     53 | 4190 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     53 | 4191 | `	if( nArg > 2 ){` |
|      9 | 4192 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|      4 | 4193 | `	}` |
|     53 | 4194 | `	pAlgo->xInit(&sCtx);` |
|     53 | 4195 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     53 | 4196 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     53 | 4197 | `	if( raw_output ){` |
|      9 | 4198 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|      5 | 4199 | `	}else{` |
|     45 | 4200 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - | 4201 | `	}` |
|     53 | 4202 | `	return PH7_OK;` |
|     29 | 4203 | `}` |
|      - | 4204 | `/*` |
|      - | 4205 | ` * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])` |
|      - | 4206 | ` *   Generate a keyed hash value using the HMAC method (RFC 2104).` |
|      - | 4207 | ` */` |
|     16 | 4208 | `static int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4209 | `{` |
|      - | 4210 | `	const HashAlgo *pAlgo;` |
|      - | 4211 | `	const char *zAlgo,*zData,*zKey;` |
|     18 | 4212 | `	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;` |
|      - | 4213 | `	HashCtx sCtx;` |
|      - | 4214 | `	unsigned char zKeyBlock[128],zIpad[128],zOpad[128],zInner[64],zDigest[64];` |
|      - | 4215 | `	int i,nBlock,nDigest;` |
|     18 | 4216 | `	if( nArg < 3 ){` |
|    ! 0 | 4217 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4218 | `			"hash_hmac() expects at least 3 arguments, %d given",nArg);` |
|      - | 4219 | `	}` |
|     18 | 4220 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     18 | 4221 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     18 | 4222 | `	if( pAlgo == 0 ){` |
|      3 | 4223 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4224 | `			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 4225 | `	}` |
|     15 | 4226 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     15 | 4227 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|     15 | 4228 | `	if( nArg > 3 ){` |
|      3 | 4229 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|      1 | 4230 | `	}` |
|     15 | 4231 | `	nBlock = pAlgo->nBlockLen;` |
|     15 | 4232 | `	nDigest = pAlgo->nDigestLen;` |
|      - | 4233 | `	/* Reduce the key to a single block: hash it if longer than the block, then` |
|      - | 4234 | `	 * zero-pad (a short or empty key is just zero-padded). */` |
|     15 | 4235 | `	SyZero(zKeyBlock,sizeof(zKeyBlock));` |
|     15 | 4236 | `	if( nKeyLen > nBlock ){` |
|      3 | 4237 | `		pAlgo->xInit(&sCtx);` |
|      3 | 4238 | `		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);` |
|      3 | 4239 | `		pAlgo->xFinal(&sCtx,zKeyBlock);` |
|     14 | 4240 | `	}else if( nKeyLen > 0 ){` |
|     11 | 4241 | `		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);` |
|      5 | 4242 | `	}` |
|   1039 | 4243 | `	for( i = 0; i < nBlock; i++ ){` |
|   1025 | 4244 | `		zIpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);` |
|   1025 | 4245 | `		zOpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);` |
|    513 | 4246 | `	}` |
|      - | 4247 | `	/* inner = H((key ^ ipad) \|\| data) */` |
|     15 | 4248 | `	pAlgo->xInit(&sCtx);` |
|     15 | 4249 | `	pAlgo->xUpdate(&sCtx,zIpad,(unsigned int)nBlock);` |
|     15 | 4250 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     15 | 4251 | `	pAlgo->xFinal(&sCtx,zInner);` |
|      - | 4252 | `	/* out = H((key ^ opad) \|\| inner) */` |
|     15 | 4253 | `	pAlgo->xInit(&sCtx);` |
|     15 | 4254 | `	pAlgo->xUpdate(&sCtx,zOpad,(unsigned int)nBlock);` |
|     15 | 4255 | `	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)nDigest);` |
|     15 | 4256 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     15 | 4257 | `	if( raw_output ){` |
|      3 | 4258 | `		ph7_result_string(pCtx,(const char *)zDigest,nDigest);` |
|      2 | 4259 | `	}else{` |
|     13 | 4260 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);` |
|      - | 4261 | `	}` |
|     15 | 4262 | `	return PH7_OK;` |
|     10 | 4263 | `}` |
|      - | 4264 | `/*` |
|      - | 4265 | ` * bool hash_equals(string $known_string,string $user_string)` |
|      - | 4266 | ` *   Timing-attack-safe string comparison.` |
|      - | 4267 | ` */` |
|     14 | 4268 | `static int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4269 | `{` |
|      - | 4270 | `	const char *zKnown,*zUser;` |
|      - | 4271 | `	int nKnown,nUser,i;` |
|     17 | 4272 | `	volatile unsigned char vDiff = 0;` |
|     17 | 4273 | `	if( nArg < 2 ){` |
|    ! 0 | 4274 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4275 | `			"hash_equals() expects exactly 2 arguments, %d given",nArg);` |
|      - | 4276 | `	}` |
|     17 | 4277 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      4 | 4278 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4279 | `			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",` |
|      1 | 4280 | `			ph7_type_name(apArg[0]));` |
|      - | 4281 | `	}` |
|     14 | 4282 | `	if( !ph7_value_is_string(apArg[1]) ){` |
|      4 | 4283 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4284 | `			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",` |
|      2 | 4285 | `			ph7_type_name(apArg[1]));` |
|      - | 4286 | `	}` |
|     11 | 4287 | `	zKnown = ph7_value_to_string(apArg[0],&nKnown);` |
|     11 | 4288 | `	zUser = ph7_value_to_string(apArg[1],&nUser);` |
|     11 | 4289 | `	if( nKnown != nUser ){` |
|      5 | 4290 | `		ph7_result_bool(pCtx,0);` |
|      5 | 4291 | `		return PH7_OK;` |
|      - | 4292 | `	}` |
|      - | 4293 | `	/* Constant-time: read every byte, never short-circuit. */` |
|     19 | 4294 | `	for( i = 0; i < nKnown; i++ ){` |
|     13 | 4295 | `		vDiff \|= (unsigned char)(zKnown[i] ^ zUser[i]);` |
|      7 | 4296 | `	}` |
|      7 | 4297 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|      7 | 4298 | `	return PH7_OK;` |
|     10 | 4299 | `}` |
|      - | 4300 | `/*` |
|      - | 4301 | ` * array hash_algos(void)` |
|      - | 4302 | ` *   Return a list of the registered hashing algorithms.` |
|      - | 4303 | ` */` |
|      2 | 4304 | `static int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4305 | `{` |
|      - | 4306 | `	ph7_value *pArray,*pValue;` |
|      - | 4307 | `	sxu32 i;` |
|      1 | 4308 | `	SXUNUSED(nArg);` |
|      1 | 4309 | `	SXUNUSED(apArg);` |
|      3 | 4310 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4311 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 4312 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 4313 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4314 | `		return PH7_OK;` |
|      - | 4315 | `	}` |
|     15 | 4316 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|     13 | 4317 | `		ph7_value_string(pValue,aHashAlgo[i].zName,-1);` |
|     13 | 4318 | `		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);` |
|     13 | 4319 | `		ph7_value_reset_string_cursor(pValue);` |
|      7 | 4320 | `	}` |
|      3 | 4321 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4322 | `	return PH7_OK;` |
|      2 | 4323 | `}` |
|      - | 4324 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 4325 | `/*` |
|      - | 4326 | ` * password_* (bcrypt). These live in ext/standard in real PHP — outside the` |
|      - | 4327 | ` * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.` |
|      - | 4328 | ` */` |
|      - | 4329 | `/*` |
|      - | 4330 | ` * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a` |
|      - | 4331 | ` * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).` |
|      - | 4332 | ` */` |
|     40 | 4333 | `static int BcryptParseHash(const char *zHash,int nHash,int *piCost)` |
|      1 | 4334 | `{` |
|      - | 4335 | `	int iCost;` |
|     51 | 4336 | `	if( nHash != 60 \|\| zHash[0] != '$' \|\| zHash[1] != '2' \|\| zHash[3] != '$'` |
|     29 | 4337 | `		\|\| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){` |
|     13 | 4338 | `		return FALSE;` |
|      - | 4339 | `	}` |
|     29 | 4340 | `	if( zHash[4] < '0' \|\| zHash[4] > '9' \|\| zHash[5] < '0' \|\| zHash[5] > '9' \|\| zHash[6] != '$' ){` |
|    ! 0 | 4341 | `		return FALSE;` |
|      - | 4342 | `	}` |
|     29 | 4343 | `	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');` |
|     29 | 4344 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      3 | 4345 | `		return FALSE;` |
|      - | 4346 | `	}` |
|     27 | 4347 | `	if( piCost ){ *piCost = iCost; }` |
|     27 | 4348 | `	return TRUE;` |
|     21 | 4349 | `}` |
|      - | 4350 | `/*` |
|      - | 4351 | ` * TRUE if the $algo argument selects bcrypt: null (PASSWORD_DEFAULT) or the` |
|      - | 4352 | ` * "2y" id (PASSWORD_BCRYPT/PASSWORD_DEFAULT). bcrypt is the only supported algo.` |
|      - | 4353 | ` */` |
|     20 | 4354 | `static int BcryptIsBcryptAlgo(ph7_value *pAlgo)` |
|      3 | 4355 | `{` |
|     23 | 4356 | `	if( ph7_value_is_null(pAlgo) ){` |
|    ! 0 | 4357 | `		return TRUE;` |
|      - | 4358 | `	}` |
|     23 | 4359 | `	if( ph7_value_is_string(pAlgo) ){` |
|      - | 4360 | `		int nAlgo;` |
|     23 | 4361 | `		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);` |
|     23 | 4362 | `		return ( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' );` |
|      - | 4363 | `	}` |
|    ! 0 | 4364 | `	return FALSE;` |
|     13 | 4365 | `}` |
|      - | 4366 | `/*` |
|      - | 4367 | ` * bool\|string password_hash(string $password,string\|int\|null $algo[,array $options])` |
|      - | 4368 | ` *  Create a bcrypt hash of the password.` |
|      - | 4369 | ` */` |
|     16 | 4370 | `static int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4371 | `{` |
|      - | 4372 | `	const char *zPwd;` |
|     19 | 4373 | `	int nPwd,iCost = 12;` |
|      - | 4374 | `	unsigned char aSalt[16];` |
|      - | 4375 | `	char zHash[60];` |
|     19 | 4376 | `	if( nArg < 2 ){` |
|    ! 0 | 4377 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4378 | `			"password_hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4379 | `	}` |
|     19 | 4380 | `	if( !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      3 | 4381 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4382 | `			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");` |
|      - | 4383 | `	}` |
|      - | 4384 | `	/* cost from $options['cost'] (default 12). */` |
|     16 | 4385 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|     14 | 4386 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|     14 | 4387 | `		if( pCost ){ iCost = ph7_value_to_int(pCost); }` |
|      6 | 4388 | `	}` |
|     16 | 4389 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      4 | 4390 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 4391 | `			"Invalid bcrypt cost parameter specified: %d",iCost);` |
|      - | 4392 | `	}` |
|     13 | 4393 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     13 | 4394 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 4395 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - | 4396 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|      - | 4397 | `	}` |
|     13 | 4398 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){` |
|    ! 0 | 4399 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4400 | `		return PH7_OK;` |
|      - | 4401 | `	}` |
|     13 | 4402 | `	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));` |
|     13 | 4403 | `	return PH7_OK;` |
|     11 | 4404 | `}` |
|      - | 4405 | `/*` |
|      - | 4406 | ` * bool password_verify(string $password,string $hash)` |
|      - | 4407 | ` *  Verify a password against a bcrypt hash. Never throws on a malformed hash.` |
|      - | 4408 | ` */` |
|     28 | 4409 | `static int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4410 | `{` |
|      - | 4411 | `	const char *zPwd,*zHash;` |
|      - | 4412 | `	int nPwd,nHash,iCost,i;` |
|      - | 4413 | `	unsigned char aSalt[16];` |
|      - | 4414 | `	char zComputed[60];` |
|     29 | 4415 | `	volatile unsigned char vDiff = 0;` |
|     29 | 4416 | `	if( nArg < 2 ){` |
|    ! 0 | 4417 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4418 | `			"password_verify() expects exactly 2 arguments, %d given",nArg);` |
|      - | 4419 | `	}` |
|     29 | 4420 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     29 | 4421 | `	zHash = ph7_value_to_string(apArg[1],&nHash);` |
|     29 | 4422 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|     11 | 4423 | `		ph7_result_bool(pCtx,0);` |
|     11 | 4424 | `		return PH7_OK;` |
|      - | 4425 | `	}` |
|      - | 4426 | `	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */` |
|     19 | 4427 | `	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 4428 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4429 | `		return PH7_OK;` |
|      - | 4430 | `	}` |
|     19 | 4431 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){` |
|    ! 0 | 4432 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4433 | `		return PH7_OK;` |
|      - | 4434 | `	}` |
|      - | 4435 | `	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps` |
|      - | 4436 | `	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */` |
|    577 | 4437 | `	for( i = 29; i < 60; i++ ){` |
|    559 | 4438 | `		vDiff \|= (unsigned char)(zComputed[i] ^ zHash[i]);` |
|    280 | 4439 | `	}` |
|     19 | 4440 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|     19 | 4441 | `	return PH7_OK;` |
|     15 | 4442 | `}` |
|      - | 4443 | `/*` |
|      - | 4444 | ` * array password_get_info(string $hash)` |
|      - | 4445 | ` *  Return ["algo"=>id\|null, "algoName"=>name, "options"=>[...]].` |
|      - | 4446 | ` */` |
|      6 | 4447 | `static int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4448 | `{` |
|      7 | 4449 | `	const char *zHash = "";` |
|      7 | 4450 | `	int nHash,iCost = 0,bBcrypt = 0;` |
|      - | 4451 | `	ph7_value *pArray,*pOptions,*pVal;` |
|      7 | 4452 | `	if( nArg > 0 ){` |
|      7 | 4453 | `		zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 4454 | `		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);` |
|      3 | 4455 | `	}` |
|      7 | 4456 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 4457 | `	pOptions = ph7_context_new_array(pCtx);` |
|      7 | 4458 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      7 | 4459 | `	if( pArray == 0 \|\| pOptions == 0 \|\| pVal == 0 ){` |
|    ! 0 | 4460 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4461 | `		return PH7_OK;` |
|      - | 4462 | `	}` |
|      7 | 4463 | `	if( bBcrypt ){` |
|      5 | 4464 | `		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */` |
|      5 | 4465 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 4466 | `		ph7_value_reset_string_cursor(pVal);` |
|      5 | 4467 | `		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);` |
|      5 | 4468 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      5 | 4469 | `		ph7_value_int(pVal,iCost);` |
|      5 | 4470 | `		ph7_array_add_strkey_elem(pOptions,"cost",pVal);` |
|      3 | 4471 | `	}else{` |
|      3 | 4472 | `		ph7_value_null(pVal);                          /* algo => null */` |
|      3 | 4473 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      3 | 4474 | `		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);` |
|      3 | 4475 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      - | 4476 | `	}` |
|      7 | 4477 | `	ph7_array_add_strkey_elem(pArray,"options",pOptions);` |
|      7 | 4478 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 4479 | `	return PH7_OK;` |
|      4 | 4480 | `}` |
|      - | 4481 | `/*` |
|      - | 4482 | ` * bool password_needs_rehash(string $hash,string\|int\|null $algo[,array $options])` |
|      - | 4483 | ` *  True if the hash was not made with the given algo/options.` |
|      - | 4484 | ` */` |
|      6 | 4485 | `static int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4486 | `{` |
|      - | 4487 | `	const char *zHash;` |
|      7 | 4488 | `	int nHash,iCost = 0,iWantCost = 12;` |
|      7 | 4489 | `	if( nArg < 2 ){` |
|    ! 0 | 4490 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4491 | `			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4492 | `	}` |
|      7 | 4493 | `	zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 4494 | `	if( !BcryptParseHash(zHash,nHash,&iCost) \|\| !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      - | 4495 | `		/* A non-bcrypt hash, or a request for a different algo → needs rehash. */` |
|      3 | 4496 | `		ph7_result_bool(pCtx,1);` |
|      3 | 4497 | `		return PH7_OK;` |
|      - | 4498 | `	}` |
|      5 | 4499 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|      5 | 4500 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|      5 | 4501 | `		if( pCost ){ iWantCost = ph7_value_to_int(pCost); }` |
|      2 | 4502 | `	}` |
|      5 | 4503 | `	ph7_result_bool(pCtx,iCost != iWantCost);` |
|      5 | 4504 | `	return PH7_OK;` |
|      4 | 4505 | `}` |
|      - | 4506 | `/*` |
|      - | 4507 | ` * filter_var() — input validation and sanitization (the ext/filter API).` |
|      - | 4508 | ` *` |
|      - | 4509 | ` * Filter and flag identifiers (values match PHP 8.5; the constants themselves` |
|      - | 4510 | ` * are registered in constant.c). The validate filters are hand-rolled rather` |
|      - | 4511 | ` * than delegating to SyStrToInt64/SyStrToReal: the former silently skips leading` |
|      - | 4512 | ` * zeros and cannot signal overflow, and the latter treats ',' as a decimal point` |
|      - | 4513 | ` * unconditionally — neither matches PHP's filter semantics.` |
|      - | 4514 | ` */` |
|      - | 4515 | `#define FV_VALIDATE_INT     257` |
|      - | 4516 | `#define FV_VALIDATE_BOOLEAN 258` |
|      - | 4517 | `#define FV_VALIDATE_FLOAT   259` |
|      - | 4518 | `#define FV_VALIDATE_REGEXP  272` |
|      - | 4519 | `#define FV_VALIDATE_URL     273` |
|      - | 4520 | `#define FV_VALIDATE_EMAIL   274` |
|      - | 4521 | `#define FV_VALIDATE_IP      275` |
|      - | 4522 | `#define FV_VALIDATE_MAC     276` |
|      - | 4523 | `#define FV_VALIDATE_DOMAIN  277` |
|      - | 4524 | `#define FV_SANITIZE_SPECIAL_CHARS      515` |
|      - | 4525 | `#define FV_DEFAULT          516 /* == FILTER_UNSAFE_RAW: pass the value through */` |
|      - | 4526 | `#define FV_SANITIZE_EMAIL   517` |
|      - | 4527 | `#define FV_SANITIZE_URL     518` |
|      - | 4528 | `#define FV_SANITIZE_NUMBER_INT   519` |
|      - | 4529 | `#define FV_SANITIZE_NUMBER_FLOAT 520` |
|      - | 4530 | `#define FV_SANITIZE_FULL_SPECIAL_CHARS 522` |
|      - | 4531 | `#define FV_FLAG_ALLOW_OCTAL  1` |
|      - | 4532 | `#define FV_FLAG_ALLOW_HEX    2` |
|      - | 4533 | `#define FV_FLAG_STRIP_LOW    4` |
|      - | 4534 | `#define FV_FLAG_STRIP_HIGH   8` |
|      - | 4535 | `#define FV_FLAG_ENCODE_LOW   16` |
|      - | 4536 | `#define FV_FLAG_ENCODE_HIGH  32` |
|      - | 4537 | `#define FV_FLAG_ENCODE_AMP   64` |
|      - | 4538 | `#define FV_FLAG_NO_ENCODE_QUOTES 128` |
|      - | 4539 | `#define FV_FLAG_STRIP_BACKTICK   512` |
|      - | 4540 | `#define FV_FLAG_ALLOW_FRACTION   4096` |
|      - | 4541 | `#define FV_FLAG_ALLOW_THOUSAND   8192` |
|      - | 4542 | `#define FV_FLAG_ALLOW_SCIENTIFIC 16384` |
|      - | 4543 | `#define FV_FLAG_IPV4  1048576` |
|      - | 4544 | `#define FV_FLAG_IPV6  2097152` |
|      - | 4545 | `#define FV_NULL_ON_FAILURE 134217728` |
|      - | 4546 | `/* The subset of flags the UNSAFE_RAW/DEFAULT string filter (FvSanitizeString)` |
|      - | 4547 | ` * acts on: when none are set the filter is a verbatim pass-through, so FV_DEFAULT` |
|      - | 4548 | ` * can shortcut. Keep this in sync with FvSanitizeString's flag handling. */` |
|      - | 4549 | `#define FV_FLAG_STRING_MASK (FV_FLAG_STRIP_LOW\|FV_FLAG_STRIP_HIGH\|FV_FLAG_STRIP_BACKTICK \` |
|      - | 4550 | `                            \|FV_FLAG_ENCODE_LOW\|FV_FLAG_ENCODE_HIGH\|FV_FLAG_ENCODE_AMP)` |
|      - | 4551 |  |
|      - | 4552 | `/* Trim leading/trailing PHP whitespace, adjusting the (*pz,*pn) view in place.` |
|      - | 4553 | ` * SyisSpace (isspace) matches PHP's filter whitespace set " \t\n\r\v\f". */` |
|    153 | 4554 | `static void FvTrim(const char **pz,int *pn){` |
|    153 | 4555 | `	const char *z = *pz;` |
|    153 | 4556 | `	int n = *pn;` |
|    157 | 4557 | `	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }` |
|    161 | 4558 | `	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }` |
|    153 | 4559 | `	*pz = z; *pn = n;` |
|    153 | 4560 | `}` |
|      - | 4561 | `/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     57 | 4562 | `static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){` |
|     57 | 4563 | `	int neg = 0, i;` |
|     57 | 4564 | `	sxu64 u = 0;` |
|     57 | 4565 | `	FvTrim(&z,&n);` |
|     57 | 4566 | `	if( n==0 ){ return 0; }` |
|     51 | 4567 | `	if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }` |
|     51 | 4568 | `	if( n==0 ){ return 0; }` |
|     49 | 4569 | `	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'\|\|z[1]=='X') ){` |
|      3 | 4570 | `		z += 2; n -= 2;` |
|      3 | 4571 | `		if( n==0 ){ return 0; }` |
|      7 | 4572 | `		for( i=0; i<n; i++ ){` |
|      5 | 4573 | `			int h = SyHexToint((unsigned char)z[i]);` |
|      5 | 4574 | `			if( h<0 ){ return 0; }` |
|      5 | 4575 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }` |
|      5 | 4576 | `			u = u*16 + (sxu64)h;` |
|      3 | 4577 | `		}` |
|     48 | 4578 | `	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){` |
|      9 | 4579 | `		for( i=0; i<n; i++ ){` |
|      7 | 4580 | `			if( z[i]<'0' \|\| z[i]>'7' ){ return 0; }` |
|      7 | 4581 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }` |
|      7 | 4582 | `			u = u*8 + (sxu64)(z[i]-'0');` |
|      4 | 4583 | `		}` |
|      2 | 4584 | `	}else{` |
|     45 | 4585 | `		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */` |
|    201 | 4586 | `		for( i=0; i<n; i++ ){` |
|    173 | 4587 | `			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|    161 | 4588 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }` |
|    161 | 4589 | `			u = u*10 + (sxu64)(z[i]-'0');` |
|     81 | 4590 | `		}` |
|      - | 4591 | `	}` |
|     33 | 4592 | `	if( neg ){` |
|      5 | 4593 | `		if( u > 0x8000000000000000ULL ){ return 0; }` |
|      5 | 4594 | `		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */` |
|      3 | 4595 | `	}else{` |
|     29 | 4596 | `		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }` |
|     27 | 4597 | `		*pOut = (ph7_int64)u;` |
|      - | 4598 | `	}` |
|     31 | 4599 | `	return 1;` |
|     29 | 4600 | `}` |
|      - | 4601 | `/* FILTER_VALIDATE_FLOAT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     69 | 4602 | `static int FvValidateFloat(const char *z,int n,int flags,double *pOut){` |
|      - | 4603 | `	char zBuf[512];` |
|     69 | 4604 | `	int i, m = 0, seenDigit = 0;` |
|     69 | 4605 | `	const char *zv; int nv; double d = 0;` |
|     69 | 4606 | `	FvTrim(&z,&n);` |
|      - | 4607 | `	/* Bound the input: zBuf[512] holds the thousand-separator-stripped copy, and` |
|      - | 4608 | `	 * the cap also rejects the pathological 500+ digit floats PHP refuses. */` |
|     69 | 4609 | `	if( n==0 \|\| n>500 ){ return 0; }` |
|     69 | 4610 | `	if( flags & FV_FLAG_ALLOW_THOUSAND ){` |
|      - | 4611 | `		/* Commas are optional, but when present they must group the integer part` |
|      - | 4612 | `		 * into a leading run of 1..3 digits followed by groups of exactly 3` |
|      - | 4613 | `		 * ("1,000" ok, "1,5"/"1234,567" rejected). Strip them into zBuf and reject` |
|      - | 4614 | `		 * a comma anywhere in the fractional/exponent tail. */` |
|     25 | 4615 | `		int s = 0, intEnd, segStart, segIdx, hasComma = 0;` |
|     25 | 4616 | `		if( s<n && (z[s]=='+'\|\|z[s]=='-') ){ zBuf[m++] = z[s]; s++; }` |
|     25 | 4617 | `		intEnd = s;` |
|    167 | 4618 | `		while( intEnd<n && z[intEnd]!='.' && z[intEnd]!='e' && z[intEnd]!='E' ){` |
|    143 | 4619 | `			if( z[intEnd]==',' ){ hasComma = 1; }` |
|    143 | 4620 | `			intEnd++;` |
|      1 | 4621 | `		}` |
|     25 | 4622 | `		if( hasComma ){` |
|     25 | 4623 | `			segStart = s; segIdx = 0;` |
|    165 | 4624 | `			for( i=s; i<=intEnd; i++ ){` |
|    151 | 4625 | `				if( i==intEnd \|\| z[i]==',' ){` |
|     49 | 4626 | `					int segLen = i - segStart, k;` |
|     49 | 4627 | `					if( segIdx==0 ){ if( segLen<1 \|\| segLen>3 ){ return 0; } }` |
|     25 | 4628 | `					else if( segLen!=3 ){ return 0; }` |
|    119 | 4629 | `					for( k=segStart; k<i; k++ ){` |
|     81 | 4630 | `						if( !SyisDigit((unsigned char)z[k]) ){ return 0; }` |
|     81 | 4631 | `						zBuf[m++] = z[k];` |
|     41 | 4632 | `					}` |
|     39 | 4633 | `					segStart = i+1; segIdx++;` |
|     19 | 4634 | `				}` |
|     71 | 4635 | `			}` |
|      8 | 4636 | `		}else{` |
|    ! 0 | 4637 | `			for( i=s; i<intEnd; i++ ){ zBuf[m++] = z[i]; }` |
|      - | 4638 | `		}` |
|     27 | 4639 | `		for( i=intEnd; i<n; i++ ){` |
|     13 | 4640 | `			if( z[i]==',' ){ return 0; }` |
|     13 | 4641 | `			zBuf[m++] = z[i];` |
|      7 | 4642 | `		}` |
|     15 | 4643 | `		zv = zBuf; nv = m;` |
|      8 | 4644 | `	}else{` |
|     45 | 4645 | `		zv = z; nv = n;` |
|      - | 4646 | `	}` |
|     59 | 4647 | `	i = 0;` |
|     59 | 4648 | `	if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|    167 | 4649 | `	while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     59 | 4650 | `	if( i<nv && zv[i]=='.' ){` |
|     21 | 4651 | `		i++;` |
|     39 | 4652 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     10 | 4653 | `	}` |
|     59 | 4654 | `	if( !seenDigit ){ return 0; }` |
|     57 | 4655 | `	if( i<nv && (zv[i]=='e'\|\|zv[i]=='E') ){` |
|     29 | 4656 | `		i++;` |
|     29 | 4657 | `		if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|     29 | 4658 | `		if( i>=nv \|\| !SyisDigit((unsigned char)zv[i]) ){ return 0; }` |
|    105 | 4659 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; }` |
|     14 | 4660 | `	}` |
|     57 | 4661 | `	if( i!=nv ){ return 0; } /* trailing junk */` |
|      - | 4662 | `	/* The grammar above guarantees zv[0..nv) is a clean ASCII decimal float (no hex /` |
|      - | 4663 | `	 * inf / nan / trailing junk), so it is safe to hand to libc strtod, which — unlike` |
|      - | 4664 | `	 * SyStrToReal (15 sig-digits + exponent clamped to 308, so it silently saturates` |
|      - | 4665 | `	 * overflowing magnitudes to a finite value) — is overflow/underflow-aware and` |
|      - | 4666 | `	 * correctly rounded. strtod needs a NUL-terminated string: the ALLOW_THOUSAND path` |
|      - | 4667 | `	 * already built the span in zBuf (zv==zBuf); the plain path must copy it there (z is` |
|      - | 4668 | `	 * const + not NUL-terminated). nv <= n <= 500 < sizeof(zBuf) by the cap above.` |
|      - | 4669 | `	 * Matches PHP 8.5 byte-for-byte: reject overflow (-> +/-INF) and total underflow` |
|      - | 4670 | `	 * (-> 0.0), keep subnormals (nonzero, errno==ERANGE) and a genuine "0" (errno==0). */` |
|     53 | 4671 | `	if( zv != zBuf ){ SyMemcpy(zv,zBuf,(sxu32)nv); }` |
|     53 | 4672 | `	zBuf[nv] = 0;` |
|     53 | 4673 | `	errno = 0;` |
|     53 | 4674 | `	d = strtod(zBuf,0);` |
|     53 | 4675 | `	if( errno == ERANGE && (d == HUGE_VAL \|\| d == -HUGE_VAL \|\| d == 0.0) ){` |
|     15 | 4676 | `		return 0;` |
|      - | 4677 | `	}` |
|     39 | 4678 | `	*pOut = d;` |
|     39 | 4679 | `	return 1;` |
|     35 | 4680 | `}` |
|      - | 4681 | `/* FILTER_VALIDATE_BOOLEAN. Returns 1 if the string is recognized (sets *pBool),` |
|      - | 4682 | ` * 0 if it is unrecognized (the failure path). "0"/"false"/"" are recognized as` |
|      - | 4683 | ` * false, NOT failures. */` |
|     33 | 4684 | `static int FvValidateBool(const char *z,int n,int *pBool){` |
|     33 | 4685 | `	FvTrim(&z,&n);` |
|     35 | 4686 | `	if( (n==1 && z[0]=='1') \|\| (n==4 && SyStrnicmp(z,"true",4)==0)` |
|     25 | 4687 | `	    \|\| (n==2 && SyStrnicmp(z,"on",2)==0) \|\| (n==3 && SyStrnicmp(z,"yes",3)==0) ){` |
|     11 | 4688 | `		*pBool = 1; return 1;` |
|      - | 4689 | `	}` |
|     23 | 4690 | `	if( n==0 \|\| (n==1 && z[0]=='0') \|\| (n==5 && SyStrnicmp(z,"false",5)==0)` |
|     11 | 4691 | `	    \|\| (n==3 && SyStrnicmp(z,"off",3)==0) \|\| (n==2 && SyStrnicmp(z,"no",2)==0) ){` |
|     11 | 4692 | `		*pBool = 0; return 1;` |
|      - | 4693 | `	}` |
|      9 | 4694 | `	return 0;` |
|     15 | 4695 | `}` |
|      - | 4696 | `/* IPv4 dotted-quad: exactly 4 octets 0..255, no leading zeros. */` |
|     33 | 4697 | `static int FvValidateIp4(const char *z,int n){` |
|     33 | 4698 | `	int i = 0, parts = 0;` |
|     77 | 4699 | `	while( i<n ){` |
|     65 | 4700 | `		int val = 0, digits = 0, start = i;` |
|    143 | 4701 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){` |
|     85 | 4702 | `			val = val*10 + (z[i]-'0');` |
|     85 | 4703 | `			if( val>255 ){ return 0; }` |
|     79 | 4704 | `			digits++; i++;` |
|      1 | 4705 | `		}` |
|     59 | 4706 | `		if( digits==0 \|\| digits>3 ){ return 0; }` |
|     49 | 4707 | `		if( digits>1 && z[start]=='0' ){ return 0; } /* leading zero */` |
|     45 | 4708 | `		parts++;` |
|     45 | 4709 | `		if( parts>4 ){ return 0; }` |
|     45 | 4710 | `		if( i<n ){` |
|     33 | 4711 | `			if( z[i]!='.' ){ return 0; }` |
|     33 | 4712 | `			i++;` |
|     33 | 4713 | `			if( i>=n ){ return 0; } /* trailing dot */` |
|     16 | 4714 | `		}` |
|      1 | 4715 | `	}` |
|     13 | 4716 | `	return parts==4;` |
|     17 | 4717 | `}` |
|      - | 4718 | `/* A colon-separated run of IPv6 hextets with no "::" (n may be 0 -> 0 groups),` |
|      - | 4719 | ` * allowing a trailing embedded IPv4. Returns the 16-bit group count or -1. */` |
|     19 | 4720 | `static int FvIp6Hextets(const char *z,int n){` |
|     19 | 4721 | `	int i = 0, segStart = 0, groups = 0;` |
|     19 | 4722 | `	if( n==0 ){ return 0; }` |
|    145 | 4723 | `	while( i<=n ){` |
|    133 | 4724 | `		if( i==n \|\| z[i]==':' ){` |
|     23 | 4725 | `			int segLen = i - segStart, j, isV4 = 0;` |
|     23 | 4726 | `			if( segLen==0 ){ return -1; } /* an empty hextet (stray ':') */` |
|     77 | 4727 | `			for( j=segStart; j<i; j++ ){ if( z[j]=='.' ){ isV4 = 1; break; } }` |
|     23 | 4728 | `			if( isV4 ){` |
|     11 | 4729 | `				if( i!=n ){ return -1; } /* IPv4 only as the final token */` |
|     11 | 4730 | `				if( !FvValidateIp4(z+segStart,segLen) ){ return -1; }` |
|      5 | 4731 | `				groups += 2;` |
|      3 | 4732 | `			}else{` |
|     13 | 4733 | `				if( segLen>4 ){ return -1; }` |
|     47 | 4734 | `				for( j=segStart; j<i; j++ ){ if( SyHexToint((unsigned char)z[j])<0 ){ return -1; } }` |
|     13 | 4735 | `				groups++;` |
|      - | 4736 | `			}` |
|     17 | 4737 | `			segStart = i+1;` |
|      8 | 4738 | `		}` |
|    127 | 4739 | `		i++;` |
|      1 | 4740 | `	}` |
|     13 | 4741 | `	return groups;` |
|     10 | 4742 | `}` |
|      - | 4743 | `/* IPv6: at most one "::" zero-run; 8 groups exactly, or fewer when "::" present. */` |
|     19 | 4744 | `static int FvValidateIp6(const char *z,int n){` |
|     19 | 4745 | `	const char *zDbl = 0;` |
|      - | 4746 | `	int i, ga, gb;` |
|    139 | 4747 | `	for( i=0; i+1<n; i++ ){` |
|    123 | 4748 | `		if( z[i]==':' && z[i+1]==':' ){` |
|     13 | 4749 | `			if( zDbl ){ return 0; } /* a second "::" is invalid */` |
|     11 | 4750 | `			zDbl = z+i;` |
|      5 | 4751 | `		}` |
|     61 | 4752 | `	}` |
|     17 | 4753 | `	if( zDbl==0 ){` |
|      9 | 4754 | `		return FvIp6Hextets(z,n)==8;` |
|    ! 0 | 4755 | `	}else{` |
|      9 | 4756 | `		int lenA = (int)(zDbl - z);` |
|      9 | 4757 | `		int lenB = n - lenA - 2;` |
|      9 | 4758 | `		ga = (lenA==0) ? 0 : FvIp6Hextets(z,lenA);` |
|      9 | 4759 | `		gb = (lenB==0) ? 0 : FvIp6Hextets(zDbl+2,lenB);` |
|      9 | 4760 | `		if( ga<0 \|\| gb<0 ){ return 0; }` |
|      9 | 4761 | `		return (ga+gb)<=7; /* "::" stands for at least one zero group */` |
|      - | 4762 | `	}` |
|     10 | 4763 | `}` |
|     25 | 4764 | `static int FvValidateIp(const char *z,int n,int flags){` |
|     25 | 4765 | `	int v4 = (flags & FV_FLAG_IPV4), v6 = (flags & FV_FLAG_IPV6);` |
|     25 | 4766 | `	if( !v4 && !v6 ){ v4 = v6 = 1; } /* default accepts either family */` |
|     25 | 4767 | `	if( v4 && FvValidateIp4(z,n) ){ return 1; }` |
|     21 | 4768 | `	if( v6 && FvValidateIp6(z,n) ){ return 1; }` |
|     13 | 4769 | `	return 0;` |
|     13 | 4770 | `}` |
|      - | 4771 | `/* FILTER_VALIDATE_MAC: 17-char colon- or dash-separated hex (XX:XX:..:XX). */` |
|     11 | 4772 | `static int FvValidateMac(const char *z,int n){` |
|      - | 4773 | `	char sep;` |
|      - | 4774 | `	int i;` |
|     11 | 4775 | `	if( n!=17 ){ return 0; }` |
|      7 | 4776 | `	sep = z[2];` |
|      7 | 4777 | `	if( sep!=':' && sep!='-' ){ return 0; }` |
|    105 | 4778 | `	for( i=0; i<17; i++ ){` |
|    101 | 4779 | `		if( (i%3)==2 ){ if( z[i]!=sep ){ return 0; } }` |
|     71 | 4780 | `		else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }` |
|     50 | 4781 | `	}` |
|      5 | 4782 | `	return 1;` |
|      6 | 4783 | `}` |
|      - | 4784 | `/* FILTER_VALIDATE_EMAIL (best-effort: covers the common cases, not quoted local` |
|      - | 4785 | ` * parts or IP-literal domains). */` |
|     28 | 4786 | `static int FvValidateEmail(const char *z,int n){` |
|     28 | 4787 | `	int at = -1, i, localLen, domLen, labelStart, dotCount = 0;` |
|      - | 4788 | `	const char *zDom;` |
|     28 | 4789 | `	if( n==0 \|\| n>320 ){ return 0; }` |
|    201 | 4790 | `	for( i=0; i<n; i++ ){` |
|    181 | 4791 | `		if( z[i]=='@' ){ if( at>=0 ){ return 0; } at = i; }` |
|     91 | 4792 | `	}` |
|     21 | 4793 | `	if( at<=0 \|\| at==n-1 ){ return 0; } /* one '@', non-empty local and domain */` |
|     21 | 4794 | `	localLen = at;` |
|     21 | 4795 | `	zDom = z + at + 1;` |
|     21 | 4796 | `	domLen = n - at - 1;` |
|     21 | 4797 | `	if( z[0]=='.' \|\| z[at-1]=='.' ){ return 0; }` |
|     57 | 4798 | `	for( i=0; i<localLen; i++ ){` |
|     43 | 4799 | `		unsigned char c = (unsigned char)z[i];` |
|     43 | 4800 | `		if( c<=' ' ){ return 0; }` |
|     41 | 4801 | `		if( c=='.' && i+1<localLen && z[i+1]=='.' ){ return 0; }` |
|     20 | 4802 | `	}` |
|     15 | 4803 | `	if( zDom[0]=='.' \|\| zDom[domLen-1]=='.' ){ return 0; }` |
|     13 | 4804 | `	labelStart = 0;` |
|     85 | 4805 | `	for( i=0; i<=domLen; i++ ){` |
|     75 | 4806 | `		if( i==domLen \|\| zDom[i]=='.' ){` |
|     25 | 4807 | `			int ll = i - labelStart;` |
|     25 | 4808 | `			if( ll==0 ){ return 0; } /* consecutive dots */` |
|     23 | 4809 | `			if( zDom[labelStart]=='-' \|\| zDom[i-1]=='-' ){ return 0; }` |
|     23 | 4810 | `			if( i<domLen ){ dotCount++; }` |
|     23 | 4811 | `			labelStart = i+1;` |
|     12 | 4812 | `		}else{` |
|     51 | 4813 | `			unsigned char c = (unsigned char)zDom[i];` |
|     51 | 4814 | `			if( !((c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9')\|\|c=='-') ){ return 0; }` |
|      - | 4815 | `		}` |
|     37 | 4816 | `	}` |
|     11 | 4817 | `	if( dotCount<1 ){ return 0; } /* PHP requires a dot in the domain (any TLD length) */` |
|      9 | 4818 | `	return 1;` |
|     15 | 4819 | `}` |
|      - | 4820 | `/* FILTER_VALIDATE_DOMAIN (lenient, matching PHP without FILTER_FLAG_HOSTNAME). */` |
|     11 | 4821 | `static int FvValidateDomain(const char *z,int n){` |
|      - | 4822 | `	int i;` |
|     11 | 4823 | `	if( n<1 \|\| n>253 \|\| z[0]=='.' ){ return 0; }` |
|     81 | 4824 | `	for( i=0; i<n; i++ ){` |
|     75 | 4825 | `		unsigned char c = (unsigned char)z[i];` |
|     75 | 4826 | `		if( c<=' ' ){ return 0; }` |
|     75 | 4827 | `		if( c=='.' && i+1<n && z[i+1]=='.' ){ return 0; }` |
|     37 | 4828 | `	}` |
|      7 | 4829 | `	return 1;` |
|      6 | 4830 | `}` |
|      - | 4831 | `/* FILTER_VALIDATE_URL: require a scheme and a host (PHP's filter is itself` |
|      - | 4832 | ` * parse_url-based, so PH7_VmHttpSplitURI tracks it closely). */` |
|     15 | 4833 | `static int FvValidateUrl(const char *z,int n){` |
|      - | 4834 | `	SyhttpUri sUri;` |
|     15 | 4835 | `	if( n==0 ){ return 0; }` |
|     15 | 4836 | `	SyZero(&sUri,(sxu32)sizeof(sUri));` |
|     15 | 4837 | `	if( PH7_VmHttpSplitURI(&sUri,z,(sxu32)n)!=SXRET_OK ){ return 0; }` |
|     15 | 4838 | `	return sUri.sScheme.nByte!=0 && sUri.sHost.nByte!=0;` |
|      8 | 4839 | `}` |
|      - | 4840 | `/* The Fv sanitizers build their result by appending directly to the call` |
|      - | 4841 | ` * context (ph7_result_string accumulates, like htmlspecialchars), emitting each` |
|      - | 4842 | ` * kept run in one call and seeding "" so an all-stripped input yields "". */` |
|      - | 4843 | `/* SANITIZE_NUMBER_INT (isFloat=0) / SANITIZE_NUMBER_FLOAT (isFloat=1). */` |
|     37 | 4844 | `static void FvSanitizeNumber(ph7_context *pCtx,const char *z,int n,int isFloat,int flags){` |
|     37 | 4845 | `	int i, runStart = 0;` |
|     37 | 4846 | `	ph7_result_string(pCtx,"",0);` |
|     97 | 4847 | `	for( i=0; i<n; i++ ){` |
|     91 | 4848 | `		char c = z[i];` |
|     91 | 4849 | `		int keep = (c>='0'&&c<='9') \|\| c=='+' \|\| c=='-';` |
|     91 | 4850 | `		if( !keep && isFloat ){` |
|     38 | 4851 | `			keep = (c=='.' && (flags & FV_FLAG_ALLOW_FRACTION))` |
|     23 | 4852 | `			    \|\| (c==',' && (flags & FV_FLAG_ALLOW_THOUSAND))` |
|     36 | 4853 | `			    \|\| ((c=='e'\|\|c=='E') && (flags & FV_FLAG_ALLOW_SCIENTIFIC));` |
|     12 | 4854 | `		}` |
|     61 | 4855 | `		if( !keep ){` |
|     33 | 4856 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     33 | 4857 | `			runStart = i+1;` |
|     16 | 4858 | `		}` |
|     31 | 4859 | `	}` |
|      7 | 4860 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 | 4861 | `}` |
|      - | 4862 | `/* Return non-zero when byte c must be stripped under the STRIP_* flags. Shared` |
|      - | 4863 | ` * by the UNSAFE_RAW string filter and SANITIZE_SPECIAL_CHARS. STRIP_LOW drops` |
|      - | 4864 | `` * bytes <32, STRIP_HIGH drops bytes >=127 (incl. DEL), STRIP_BACKTICK drops '`'.`` |
|      - | 4865 | ` * Matches php_filter_strip(); verified byte-exact vs php 8.5.7. */` |
|    287 | 4866 | `static int FvStripByte(unsigned char c,int flags){` |
|    287 | 4867 | `	if( (flags & FV_FLAG_STRIP_LOW)      && c<32 )    { return 1; }` |
|    281 | 4868 | `	if( (flags & FV_FLAG_STRIP_HIGH)     && c>=127 )  { return 1; }` |
|    269 | 4869 | `	if( (flags & FV_FLAG_STRIP_BACKTICK) && c==0x60 ) { return 1; }` |
|    267 | 4870 | `	return 0;` |
|    144 | 4871 | `}` |
|      - | 4872 | `/* FILTER_UNSAFE_RAW / FILTER_DEFAULT with flags: no default transform, but the` |
|      - | 4873 | ` * STRIP/ENCODE flags apply. Precedence (per php_filter_unsafe_raw, verified` |
|      - | 4874 | ` * vs php 8.5.7): a byte is first tested for stripping; a surviving byte is then` |
|      - | 4875 | ` * encoded as a decimal numeric entity if ENCODE_LOW (<32) / ENCODE_HIGH (>=127)` |
|      - | 4876 | ` * is set, and '&' becomes "&#38;" under ENCODE_AMP. So STRIP_LOW\|ENCODE_LOW` |
|      - | 4877 | ` * strips (nothing left to encode). Bytes are treated individually — ENCODE_HIGH` |
|      - | 4878 | ` * numeric-encodes each byte of a multibyte sequence separately, not the codepoint. */` |
|     25 | 4879 | `static void FvSanitizeString(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 4880 | `	int i, runStart = 0;` |
|     25 | 4881 | `	ph7_result_string(pCtx,"",0);` |
|    193 | 4882 | `	for( i=0; i<n; i++ ){` |
|    179 | 4883 | `		unsigned char c = (unsigned char)z[i];` |
|    179 | 4884 | `		if( FvStripByte(c,flags) ){` |
|     13 | 4885 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     13 | 4886 | `			runStart = i+1;` |
|     13 | 4887 | `			continue;` |
|      - | 4888 | `		}` |
|    167 | 4889 | `		if( c=='&' && (flags & FV_FLAG_ENCODE_AMP) ){` |
|      3 | 4890 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      3 | 4891 | `			ph7_result_string(pCtx,"&#38;",-1);` |
|      3 | 4892 | `			runStart = i+1;` |
|    184 | 4893 | `		}else if( (c<32 && (flags & FV_FLAG_ENCODE_LOW))` |
|    164 | 4894 | `		       \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     37 | 4895 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 4896 | `			ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|      9 | 4897 | `			runStart = i+1;` |
|      4 | 4898 | `		}` |
|     79 | 4899 | `	}` |
|     15 | 4900 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     15 | 4901 | `}` |
|      - | 4902 | `/* FILTER_SANITIZE_SPECIAL_CHARS: encode <>&"' and every control byte <32 as a` |
|      - | 4903 | ` * decimal numeric entity (&#60; &#38; &#34; ...). The STRIP_* flags remove bytes` |
|      - | 4904 | ` * before encoding; ENCODE_HIGH numeric-encodes surviving bytes >=127. Bytes >=128` |
|      - | 4905 | ` * are otherwise passed through verbatim (this filter is NOT UTF-8-aware — only the` |
|      - | 4906 | ` * FULL variant is). Byte-exact vs php 8.5.7. */` |
|     13 | 4907 | `static void FvSanitizeSpecial(ph7_context *pCtx,const char *z,int n,int flags){` |
|     13 | 4908 | `	int i, runStart = 0;` |
|      - | 4909 | `	const char *zEnt;` |
|     13 | 4910 | `	ph7_result_string(pCtx,"",0);` |
|    131 | 4911 | `	for( i=0; i<n; i++ ){` |
|    119 | 4912 | `		unsigned char c = (unsigned char)z[i];` |
|    119 | 4913 | `		if( FvStripByte(c,flags) ){` |
|      9 | 4914 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 4915 | `			runStart = i+1;` |
|      9 | 4916 | `			continue;` |
|      - | 4917 | `		}` |
|    111 | 4918 | `		switch( c ){` |
|      3 | 4919 | `		case '<':  zEnt = "&#60;"; break;` |
|      3 | 4920 | `		case '>':  zEnt = "&#62;"; break;` |
|     11 | 4921 | `		case '&':  zEnt = "&#38;"; break;` |
|      3 | 4922 | `		case '"':  zEnt = "&#34;"; break;` |
|      3 | 4923 | `		case '\'': zEnt = "&#39;"; break;` |
|     46 | 4924 | `		default:` |
|      - | 4925 | `			/* Control bytes <32 are always numeric-encoded; bytes >=127 only when` |
|      - | 4926 | `			 * ENCODE_HIGH is set. Everything else stays in the current run. */` |
|     93 | 4927 | `			if( c<32 \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     17 | 4928 | `				if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     17 | 4929 | `				ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|     17 | 4930 | `				runStart = i+1;` |
|      8 | 4931 | `			}` |
|     93 | 4932 | `			continue; /* keep in the current run */` |
|      - | 4933 | `		}` |
|     19 | 4934 | `		if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     19 | 4935 | `		ph7_result_string(pCtx,zEnt,-1); /* -1: length from strlen */` |
|     19 | 4936 | `		runStart = i+1;` |
|     10 | 4937 | `	}` |
|     13 | 4938 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     13 | 4939 | `}` |
|      - | 4940 | `/* HTML 4.01 named-entity table (codepoint -> "&name;") used by the UTF-8-aware` |
|      - | 4941 | ` * FULL_SPECIAL_CHARS filter, sorted ascending by codepoint for binary search.` |
|      - | 4942 | ` * Generated from php 8.5.7 (the exact set php_escape_html_entities emits for the` |
|      - | 4943 | ` * default document type); the five inline specials <>&"' are handled separately,` |
|      - | 4944 | ` * so every entry here is a codepoint >=0xA0. 248 rows. */` |
|      - | 4945 | `static const struct { sxu32 cp; const char *zEnt; } aHtml401Ent[] = {` |
|      - | 4946 | `	{0x00A0,"&nbsp;"},{0x00A1,"&iexcl;"},{0x00A2,"&cent;"},{0x00A3,"&pound;"},` |
|      - | 4947 | `	{0x00A4,"&curren;"},{0x00A5,"&yen;"},{0x00A6,"&brvbar;"},{0x00A7,"&sect;"},` |
|      - | 4948 | `	{0x00A8,"&uml;"},{0x00A9,"&copy;"},{0x00AA,"&ordf;"},{0x00AB,"&laquo;"},` |
|      - | 4949 | `	{0x00AC,"&not;"},{0x00AD,"&shy;"},{0x00AE,"&reg;"},{0x00AF,"&macr;"},` |
|      - | 4950 | `	{0x00B0,"&deg;"},{0x00B1,"&plusmn;"},{0x00B2,"&sup2;"},{0x00B3,"&sup3;"},` |
|      - | 4951 | `	{0x00B4,"&acute;"},{0x00B5,"&micro;"},{0x00B6,"&para;"},{0x00B7,"&middot;"},` |
|      - | 4952 | `	{0x00B8,"&cedil;"},{0x00B9,"&sup1;"},{0x00BA,"&ordm;"},{0x00BB,"&raquo;"},` |
|      - | 4953 | `	{0x00BC,"&frac14;"},{0x00BD,"&frac12;"},{0x00BE,"&frac34;"},{0x00BF,"&iquest;"},` |
|      - | 4954 | `	{0x00C0,"&Agrave;"},{0x00C1,"&Aacute;"},{0x00C2,"&Acirc;"},{0x00C3,"&Atilde;"},` |
|      - | 4955 | `	{0x00C4,"&Auml;"},{0x00C5,"&Aring;"},{0x00C6,"&AElig;"},{0x00C7,"&Ccedil;"},` |
|      - | 4956 | `	{0x00C8,"&Egrave;"},{0x00C9,"&Eacute;"},{0x00CA,"&Ecirc;"},{0x00CB,"&Euml;"},` |
|      - | 4957 | `	{0x00CC,"&Igrave;"},{0x00CD,"&Iacute;"},{0x00CE,"&Icirc;"},{0x00CF,"&Iuml;"},` |
|      - | 4958 | `	{0x00D0,"&ETH;"},{0x00D1,"&Ntilde;"},{0x00D2,"&Ograve;"},{0x00D3,"&Oacute;"},` |
|      - | 4959 | `	{0x00D4,"&Ocirc;"},{0x00D5,"&Otilde;"},{0x00D6,"&Ouml;"},{0x00D7,"&times;"},` |
|      - | 4960 | `	{0x00D8,"&Oslash;"},{0x00D9,"&Ugrave;"},{0x00DA,"&Uacute;"},{0x00DB,"&Ucirc;"},` |
|      - | 4961 | `	{0x00DC,"&Uuml;"},{0x00DD,"&Yacute;"},{0x00DE,"&THORN;"},{0x00DF,"&szlig;"},` |
|      - | 4962 | `	{0x00E0,"&agrave;"},{0x00E1,"&aacute;"},{0x00E2,"&acirc;"},{0x00E3,"&atilde;"},` |
|      - | 4963 | `	{0x00E4,"&auml;"},{0x00E5,"&aring;"},{0x00E6,"&aelig;"},{0x00E7,"&ccedil;"},` |
|      - | 4964 | `	{0x00E8,"&egrave;"},{0x00E9,"&eacute;"},{0x00EA,"&ecirc;"},{0x00EB,"&euml;"},` |
|      - | 4965 | `	{0x00EC,"&igrave;"},{0x00ED,"&iacute;"},{0x00EE,"&icirc;"},{0x00EF,"&iuml;"},` |
|      - | 4966 | `	{0x00F0,"&eth;"},{0x00F1,"&ntilde;"},{0x00F2,"&ograve;"},{0x00F3,"&oacute;"},` |
|      - | 4967 | `	{0x00F4,"&ocirc;"},{0x00F5,"&otilde;"},{0x00F6,"&ouml;"},{0x00F7,"&divide;"},` |
|      - | 4968 | `	{0x00F8,"&oslash;"},{0x00F9,"&ugrave;"},{0x00FA,"&uacute;"},{0x00FB,"&ucirc;"},` |
|      - | 4969 | `	{0x00FC,"&uuml;"},{0x00FD,"&yacute;"},{0x00FE,"&thorn;"},{0x00FF,"&yuml;"},` |
|      - | 4970 | `	{0x0152,"&OElig;"},{0x0153,"&oelig;"},{0x0160,"&Scaron;"},{0x0161,"&scaron;"},` |
|      - | 4971 | `	{0x0178,"&Yuml;"},{0x0192,"&fnof;"},{0x02C6,"&circ;"},{0x02DC,"&tilde;"},` |
|      - | 4972 | `	{0x0391,"&Alpha;"},{0x0392,"&Beta;"},{0x0393,"&Gamma;"},{0x0394,"&Delta;"},` |
|      - | 4973 | `	{0x0395,"&Epsilon;"},{0x0396,"&Zeta;"},{0x0397,"&Eta;"},{0x0398,"&Theta;"},` |
|      - | 4974 | `	{0x0399,"&Iota;"},{0x039A,"&Kappa;"},{0x039B,"&Lambda;"},{0x039C,"&Mu;"},` |
|      - | 4975 | `	{0x039D,"&Nu;"},{0x039E,"&Xi;"},{0x039F,"&Omicron;"},{0x03A0,"&Pi;"},` |
|      - | 4976 | `	{0x03A1,"&Rho;"},{0x03A3,"&Sigma;"},{0x03A4,"&Tau;"},{0x03A5,"&Upsilon;"},` |
|      - | 4977 | `	{0x03A6,"&Phi;"},{0x03A7,"&Chi;"},{0x03A8,"&Psi;"},{0x03A9,"&Omega;"},` |
|      - | 4978 | `	{0x03B1,"&alpha;"},{0x03B2,"&beta;"},{0x03B3,"&gamma;"},{0x03B4,"&delta;"},` |
|      - | 4979 | `	{0x03B5,"&epsilon;"},{0x03B6,"&zeta;"},{0x03B7,"&eta;"},{0x03B8,"&theta;"},` |
|      - | 4980 | `	{0x03B9,"&iota;"},{0x03BA,"&kappa;"},{0x03BB,"&lambda;"},{0x03BC,"&mu;"},` |
|      - | 4981 | `	{0x03BD,"&nu;"},{0x03BE,"&xi;"},{0x03BF,"&omicron;"},{0x03C0,"&pi;"},` |
|      - | 4982 | `	{0x03C1,"&rho;"},{0x03C2,"&sigmaf;"},{0x03C3,"&sigma;"},{0x03C4,"&tau;"},` |
|      - | 4983 | `	{0x03C5,"&upsilon;"},{0x03C6,"&phi;"},{0x03C7,"&chi;"},{0x03C8,"&psi;"},` |
|      - | 4984 | `	{0x03C9,"&omega;"},{0x03D1,"&thetasym;"},{0x03D2,"&upsih;"},{0x03D6,"&piv;"},` |
|      - | 4985 | `	{0x2002,"&ensp;"},{0x2003,"&emsp;"},{0x2009,"&thinsp;"},{0x200C,"&zwnj;"},` |
|      - | 4986 | `	{0x200D,"&zwj;"},{0x200E,"&lrm;"},{0x200F,"&rlm;"},{0x2013,"&ndash;"},` |
|      - | 4987 | `	{0x2014,"&mdash;"},{0x2018,"&lsquo;"},{0x2019,"&rsquo;"},{0x201A,"&sbquo;"},` |
|      - | 4988 | `	{0x201C,"&ldquo;"},{0x201D,"&rdquo;"},{0x201E,"&bdquo;"},{0x2020,"&dagger;"},` |
|      - | 4989 | `	{0x2021,"&Dagger;"},{0x2022,"&bull;"},{0x2026,"&hellip;"},{0x2030,"&permil;"},` |
|      - | 4990 | `	{0x2032,"&prime;"},{0x2033,"&Prime;"},{0x2039,"&lsaquo;"},{0x203A,"&rsaquo;"},` |
|      - | 4991 | `	{0x203E,"&oline;"},{0x2044,"&frasl;"},{0x20AC,"&euro;"},{0x2111,"&image;"},` |
|      - | 4992 | `	{0x2118,"&weierp;"},{0x211C,"&real;"},{0x2122,"&trade;"},{0x2135,"&alefsym;"},` |
|      - | 4993 | `	{0x2190,"&larr;"},{0x2191,"&uarr;"},{0x2192,"&rarr;"},{0x2193,"&darr;"},` |
|      - | 4994 | `	{0x2194,"&harr;"},{0x21B5,"&crarr;"},{0x21D0,"&lArr;"},{0x21D1,"&uArr;"},` |
|      - | 4995 | `	{0x21D2,"&rArr;"},{0x21D3,"&dArr;"},{0x21D4,"&hArr;"},{0x2200,"&forall;"},` |
|      - | 4996 | `	{0x2202,"&part;"},{0x2203,"&exist;"},{0x2205,"&empty;"},{0x2207,"&nabla;"},` |
|      - | 4997 | `	{0x2208,"&isin;"},{0x2209,"&notin;"},{0x220B,"&ni;"},{0x220F,"&prod;"},` |
|      - | 4998 | `	{0x2211,"&sum;"},{0x2212,"&minus;"},{0x2217,"&lowast;"},{0x221A,"&radic;"},` |
|      - | 4999 | `	{0x221D,"&prop;"},{0x221E,"&infin;"},{0x2220,"&ang;"},{0x2227,"&and;"},` |
|      - | 5000 | `	{0x2228,"&or;"},{0x2229,"&cap;"},{0x222A,"&cup;"},{0x222B,"&int;"},` |
|      - | 5001 | `	{0x2234,"&there4;"},{0x223C,"&sim;"},{0x2245,"&cong;"},{0x2248,"&asymp;"},` |
|      - | 5002 | `	{0x2260,"&ne;"},{0x2261,"&equiv;"},{0x2264,"&le;"},{0x2265,"&ge;"},` |
|      - | 5003 | `	{0x2282,"&sub;"},{0x2283,"&sup;"},{0x2284,"&nsub;"},{0x2286,"&sube;"},` |
|      - | 5004 | `	{0x2287,"&supe;"},{0x2295,"&oplus;"},{0x2297,"&otimes;"},{0x22A5,"&perp;"},` |
|      - | 5005 | `	{0x22C5,"&sdot;"},{0x2308,"&lceil;"},{0x2309,"&rceil;"},{0x230A,"&lfloor;"},` |
|      - | 5006 | `	{0x230B,"&rfloor;"},{0x2329,"&lang;"},{0x232A,"&rang;"},{0x25CA,"&loz;"},` |
|      - | 5007 | `	{0x2660,"&spades;"},{0x2663,"&clubs;"},{0x2665,"&hearts;"},{0x2666,"&diams;"}` |
|      - | 5008 | `};` |
|      - | 5009 | `/* Binary-search aHtml401Ent[] for cp; return its "&name;" entity or 0. */` |
|     41 | 5010 | `static const char *FvHtml401Lookup(sxu32 cp){` |
|     41 | 5011 | `	int lo = 0, hi = (int)SX_ARRAYSIZE(aHtml401Ent) - 1;` |
|    323 | 5012 | `	while( lo <= hi ){` |
|    309 | 5013 | `		int mid = (lo + hi) / 2;` |
|    309 | 5014 | `		sxu32 c = aHtml401Ent[mid].cp;` |
|    309 | 5015 | `		if( c == cp ){ return aHtml401Ent[mid].zEnt; }` |
|    283 | 5016 | `		if( c < cp ){ lo = mid + 1; } else { hi = mid - 1; }` |
|      1 | 5017 | `	}` |
|     15 | 5018 | `	return 0;` |
|     21 | 5019 | `}` |
|      - | 5020 | `/* Decode one strict-UTF-8 sequence at p (< zEnd). On success returns its byte` |
|      - | 5021 | ` * length (1..4) and sets *pCp to the codepoint; on any malformed, overlong,` |
|      - | 5022 | ` * surrogate, truncated or out-of-range (>U+10FFFF) sequence returns 0. Matches` |
|      - | 5023 | ` * PHP's UTF-8 validation used by FULL_SPECIAL_CHARS (verified vs php 8.5.7). */` |
|    101 | 5024 | `static int FvUtf8Next(const unsigned char *p,const unsigned char *zEnd,sxu32 *pCp){` |
|    101 | 5025 | `	unsigned char c = p[0];` |
|    101 | 5026 | `	if( c < 0x80 ){ *pCp = c; return 1; }` |
|    101 | 5027 | `	if( c < 0xC2 ){ return 0; }              /* 0x80-0xBF stray cont / 0xC0-0xC1 overlong */` |
|     99 | 5028 | `	if( c < 0xE0 ){                          /* 2-byte: U+0080..U+07FF */` |
|     47 | 5029 | `		if( zEnd-p < 2 \|\| (p[1]&0xC0)!=0x80 ){ return 0; }` |
|     45 | 5030 | `		*pCp = ((sxu32)(c&0x1F)<<6) \| (p[1]&0x3F);` |
|     45 | 5031 | `		return 2;` |
|      - | 5032 | `	}` |
|     53 | 5033 | `	if( c < 0xF0 ){                          /* 3-byte: U+0800..U+FFFF minus surrogates */` |
|      - | 5034 | `		sxu32 cp;` |
|     47 | 5035 | `		if( zEnd-p < 3 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 ){ return 0; }` |
|     33 | 5036 | `		cp = ((sxu32)(c&0x0F)<<12) \| ((sxu32)(p[1]&0x3F)<<6) \| (p[2]&0x3F);` |
|     33 | 5037 | `		if( cp < 0x800 \|\| (cp>=0xD800 && cp<=0xDFFF) ){ return 0; }` |
|     29 | 5038 | `		*pCp = cp;` |
|     29 | 5039 | `		return 3;` |
|      - | 5040 | `	}` |
|      7 | 5041 | `	if( c < 0xF5 ){                          /* 4-byte: U+10000..U+10FFFF */` |
|      - | 5042 | `		sxu32 cp;` |
|      5 | 5043 | `		if( zEnd-p < 4 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 \|\| (p[3]&0xC0)!=0x80 ){ return 0; }` |
|      5 | 5044 | `		cp = ((sxu32)(c&0x07)<<18) \| ((sxu32)(p[1]&0x3F)<<12) \| ((sxu32)(p[2]&0x3F)<<6) \| (p[3]&0x3F);` |
|      5 | 5045 | `		if( cp < 0x10000 \|\| cp > 0x10FFFF ){ return 0; }` |
|      5 | 5046 | `		*pCp = cp;` |
|      5 | 5047 | `		return 4;` |
|      - | 5048 | `	}` |
|      3 | 5049 | `	return 0;                                /* 0xF5-0xFF */` |
|     51 | 5050 | `}` |
|      - | 5051 | `/* FILTER_SANITIZE_FULL_SPECIAL_CHARS: htmlentities-style, UTF-8-aware. Encodes` |
|      - | 5052 | ` * <>&"' as named entities ("'" -> &#039;; quotes suppressed under NO_ENCODE_QUOTES),` |
|      - | 5053 | ` * and every valid UTF-8 codepoint with an HTML 4.01 named entity as that entity;` |
|      - | 5054 | ` * valid codepoints without a named entity (and low control bytes) pass through` |
|      - | 5055 | ` * verbatim. If the input contains ANY invalid UTF-8 the whole result is "".` |
|      - | 5056 | ` * The STRIP/ENCODE flags do NOT apply to this filter (only NO_ENCODE_QUOTES).` |
|      - | 5057 | ` * php's filter does NOT re-encode valid pre-existing entities ("&amp;" stays,` |
|      - | 5058 | ` * "&bogus;" becomes "&amp;bogus;"), i.e. double_encode=false semantics —` |
|      - | 5059 | ` * exactly htmlentities(ENT_QUOTES\|ENT_HTML401, double_encode: false), so this` |
|      - | 5060 | ` * delegates to the shared encoder. Byte-exact vs php 8.5.7. */` |
|     25 | 5061 | `static void FvSanitizeFull(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 5062 | `	int iEntFlags = (flags & FV_FLAG_NO_ENCODE_QUOTES) ? 0 : PH7_ENT_QUOTES;` |
|     25 | 5063 | `	HtmlEscape(pCtx,z,n,iEntFlags,1/*bAll*/,0/*bDoubleEncode*/);` |
|     25 | 5064 | `}` |
|      - | 5065 | `/* ---------------------------------------------------------------------------` |
|      - | 5066 | ` * UTF-8-aware HTML entity core (htmlspecialchars/htmlentities family).` |
|      - | 5067 | ` * Prototyped next to the five builtins earlier in this file; lives here so it` |
|      - | 5068 | ` * can share aHtml401Ent[]/FvHtml401Lookup()/FvUtf8Next() with the filter_var` |
|      - | 5069 | ` * FULL_SPECIAL_CHARS filter above. Byte-exact vs php 8.5.7 (oracle-swept).` |
|      - | 5070 | ` * ------------------------------------------------------------------------ */` |
|      - | 5071 | `/* Encode cp as UTF-8 into zBuf (>= 4 bytes); return the byte length 1..4.` |
|      - | 5072 | ` * Thin wrapper over the engine-wide SX_WRITE_UTF8 (sxmacros.h). */` |
|    585 | 5073 | `static int HtmlCpUtf8(sxu32 cp,char *zBuf){` |
|    585 | 5074 | `	sxu8 *z = (sxu8 *)zBuf;` |
|    585 | 5075 | `	SX_WRITE_UTF8(z,cp);` |
|    585 | 5076 | `	return (int)(z - (sxu8 *)zBuf);` |
|      1 | 5077 | `}` |
|      - | 5078 | `/* Doctype-allowed codepoint test (php's unicode_cp_is_allowed) — gates what a` |
|      - | 5079 | ` * numeric reference may DECODE to. Oracle-pinned per doctype: HTML401` |
|      - | 5080 | ` * disallows C0 (except TAB/LF/CR) and DEL..U+009F; XML1 and XHTML share the` |
|      - | 5081 | ` * XML rules — DEL..U+009F allowed, U+FFFE/U+FFFF excluded; HTML5 swaps CR` |
|      - | 5082 | ` * for FF (0x0C) and excludes the noncharacters (U+FDD0..U+FDEF and every` |
|      - | 5083 | ` * U+xFFFE/U+xFFFF). Surrogates are disallowed everywhere. */` |
|     91 | 5084 | `static int HtmlCpAllowed(sxu32 cp,int iFlags){` |
|     91 | 5085 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|     91 | 5086 | `	if( cp==0x09 \|\| cp==0x0A ){ return 1; }` |
|     87 | 5087 | `	if( cp==0x0D ){ return iDoc != PH7_ENT_DOC_HTML5; }` |
|     85 | 5088 | `	if( cp==0x0C ){ return iDoc == PH7_ENT_DOC_HTML5; }` |
|     85 | 5089 | `	if( cp < 0x20 \|\| cp > 0x10FFFF ){ return 0; }` |
|     79 | 5090 | `	if( cp>=0xD800 && cp<=0xDFFF ){ return 0; }` |
|     77 | 5091 | `	if( cp>=0x7F && cp<=0x9F ){ return iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML; }` |
|     71 | 5092 | `	if( iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML ){` |
|    ! 0 | 5093 | `		return cp!=0xFFFE && cp!=0xFFFF;` |
|      - | 5094 | `	}` |
|     71 | 5095 | `	if( iDoc == PH7_ENT_DOC_HTML5 ){` |
|      9 | 5096 | `		if( cp>=0xFDD0 && cp<=0xFDEF ){ return 0; }` |
|      9 | 5097 | `		if( (cp & 0xFFFF) >= 0xFFFE ){ return 0; }` |
|      4 | 5098 | `	}` |
|     71 | 5099 | `	return 1;` |
|     46 | 5100 | `}` |
|      - | 5101 | `/* The ENT_DISALLOWED gate for RAW characters on the ENCODE side. Same as the` |
|      - | 5102 | ` * decode gate except CR under HTML5: php's encode-side unicode_cp_is_allowed` |
|      - | 5103 | ` * keeps a literal "\r" verbatim under ENT_HTML5\|ENT_DISALLOWED while the` |
|      - | 5104 | ` * decode side leaves "&#13;" un-decoded (oracle-pinned at flags 176). */` |
|      9 | 5105 | `static int HtmlCpAllowedEncode(sxu32 cp,int iFlags){` |
|      9 | 5106 | `	if( cp==0x0D && (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 ){ return 1; }` |
|      9 | 5107 | `	return HtmlCpAllowed(cp,iFlags);` |
|      5 | 5108 | `}` |
|      - | 5109 | `/* Numeric-reference validity for the double_encode=false "is this already a` |
|      - | 5110 | ` * valid entity" test — a MUCH looser predicate than the decode gate above:` |
|      - | 5111 | ` * any codepoint <= U+10FFFF is valid (controls and surrogates included, every` |
|      - | 5112 | ` * doctype). ENT_DISALLOWED re-tightens non-HTML401 doctypes to the decode` |
|      - | 5113 | ` * gate, except that HTML5 exempts surrogates. All oracle-pinned: &#0; and` |
|      - | 5114 | ` * &#xD800; stay verbatim at flags 11 and 139; flags -1 (HTML5+DISALLOWED)` |
|      - | 5115 | ` * re-encodes &#0; and &#x10FFFF; but still keeps &#xD800;; flags 144` |
|      - | 5116 | ` * (XML1+DISALLOWED) re-encodes &#xD800;. */` |
|      9 | 5117 | `static int HtmlNumericAllowed(sxu32 cp,int iFlags){` |
|      9 | 5118 | `	if( cp > 0x10FFFF ){ return 0; }` |
|      7 | 5119 | `	if( (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML401 ){ return 1; /* never tightened */ }` |
|    ! 0 | 5120 | `	if( (iFlags & PH7_ENT_DISALLOWED)` |
|    ! 0 | 5121 | `	 && !((iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 && cp>=0xD800 && cp<=0xDFFF)` |
|    ! 0 | 5122 | `	 && !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|    ! 0 | 5123 | `	return 1;` |
|      5 | 5124 | `}` |
|      - | 5125 | `/* How many bytes the malformed UTF-8 sequence at p consumes — php's` |
|      - | 5126 | ` * get_next_char failure step (one U+FFFD substitution / one ENT_IGNORE drop` |
|      - | 5127 | ` * per MAXIMAL invalid subpart, not per byte): a prefix-valid sequence eats` |
|      - | 5128 | ` * its continuation bytes ("\xE0\x80\xAF" is ONE unit) while a byte that could` |
|      - | 5129 | ` * start a new sequence is left for the next round. */` |
|      5 | 5130 | `static int HtmlUtf8Trail(unsigned char c){ return c>=0x80 && c<=0xBF; }` |
|     11 | 5131 | `static int HtmlUtf8Lead(unsigned char c){ return c<0x80 \|\| (c>=0xC2 && c<=0xF4); }` |
|     15 | 5132 | `static int HtmlUtf8FailAdvance(const unsigned char *p,const unsigned char *zEnd){` |
|     15 | 5133 | `	unsigned char c = p[0];` |
|     15 | 5134 | `	int nAvail = (int)(zEnd - p);` |
|     15 | 5135 | `	if( c < 0xC2 \|\| c > 0xF4 ){ return 1; } /* stray trail / C0-C1 / F5-FF */` |
|     13 | 5136 | `	if( c < 0xE0 ){` |
|      3 | 5137 | `		if( nAvail < 2 ){ return 1; }` |
|      3 | 5138 | `		return HtmlUtf8Lead(p[1]) ? 1 : 2;` |
|      - | 5139 | `	}` |
|     11 | 5140 | `	if( c < 0xF0 ){` |
|     11 | 5141 | `		if( nAvail >= 3 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) ){` |
|      3 | 5142 | `			return 3; /* complete but overlong/surrogate */` |
|      - | 5143 | `		}` |
|      9 | 5144 | `		if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 5145 | `		if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 5146 | `		return 3;` |
|      - | 5147 | `	}` |
|    ! 0 | 5148 | `	if( nAvail >= 4 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) && HtmlUtf8Trail(p[3]) ){` |
|    ! 0 | 5149 | `		return 4; /* complete but overlong / > U+10FFFF */` |
|      - | 5150 | `	}` |
|    ! 0 | 5151 | `	if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 5152 | `	if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 5153 | `	if( nAvail < 4 \|\| HtmlUtf8Lead(p[3]) ){ return 3; }` |
|    ! 0 | 5154 | `	return 4;` |
|      8 | 5155 | `}` |
|      - | 5156 | `/* The basic special entities, shared by named matching, the hsc_decode` |
|      - | 5157 | ` * numeric whitelist and the translation-table builder so the sets can never` |
|      - | 5158 | ` * drift apart. (&apos; is not an HTML 4.01 entity — doctype-gated below.) */` |
|      - | 5159 | `static const struct { const char *zEnt; int n; sxu32 cp; } aHtmlSpecEnt[] = {` |
|      - | 5160 | `	{"&amp;",5,38},{"&lt;",4,60},{"&gt;",4,62},{"&quot;",6,34},{"&apos;",6,39}` |
|      - | 5161 | `};` |
|      - | 5162 | `/* Does this doctype consult the named-entity table (aHtml401Ent)? XML 1.0 has` |
|      - | 5163 | ` * no named entities beyond the specials; XHTML/HTML5 are approximated by the` |
|      - | 5164 | ` * HTML 4.01 table (documented divergence, PLAN.md §3.9). */` |
|     63 | 5165 | `static int HtmlDocHasNamedTable(int iDoc){` |
|     63 | 5166 | `	return iDoc != PH7_ENT_DOC_XML1;` |
|      1 | 5167 | `}` |
|      - | 5168 | `/* The single-quote entity per doctype. Oracle-pinned asymmetry: for every` |
|      - | 5169 | ` * non-HTML401 doctype htmlspecialchars emits &apos; while htmlentities` |
|      - | 5170 | ` * (bEntities) keeps &#039; under XHTML too. The translation table mirrors` |
|      - | 5171 | ` * whichever function the requested table belongs to. */` |
|     29 | 5172 | `static const char *HtmlAposEntity(int iDoc,int bEntities){` |
|     29 | 5173 | `	if( iDoc == PH7_ENT_DOC_HTML401 \|\| (bEntities && iDoc == PH7_ENT_DOC_XHTML) ){` |
|     21 | 5174 | `		return "&#039;";` |
|      - | 5175 | `	}` |
|      9 | 5176 | `	return "&apos;";` |
|     15 | 5177 | `}` |
|      - | 5178 | `/* Try to parse one HTML entity at z (z[0]=='&', z < zEnd). bFull selects the` |
|      - | 5179 | ` * html_entity_decode set (doctype named table + any allowed numeric ref) vs` |
|      - | 5180 | ` * the htmlspecialchars_decode set (the basic specials + quote numerics only).` |
|      - | 5181 | ` * Named matching is case-SENSITIVE and the ';' is required (both PHP-exact);` |
|      - | 5182 | ` * numeric refs accept dec/hex (x or X) with any number of leading zeros but` |
|      - | 5183 | ` * reject out-of-range, surrogate and doctype-disallowed codepoints (the` |
|      - | 5184 | ` * caller then leaves the source verbatim). Quote-flag gating is NOT applied` |
|      - | 5185 | ` * here — the same routine doubles as the "is this a valid entity" test for` |
|      - | 5186 | ` * double_encode=false, which ignores the quote bits (oracle-pinned).` |
|      - | 5187 | ` * bEncodeCheck selects the looser HtmlNumericAllowed predicate used by that` |
|      - | 5188 | ` * double_encode test; decode callers pass 0 for the HtmlCpAllowed gate.` |
|      - | 5189 | ` * On success sets *pCp / *pnConsumed and returns 1. */` |
|    172 | 5190 | `static int HtmlParseEntity(const unsigned char *z,const unsigned char *zEnd,` |
|      1 | 5191 | `                           int iFlags,int bFull,int bEncodeCheck,sxu32 *pCp,int *pnConsumed){` |
|    173 | 5192 | `	int nAvail = (int)(zEnd - z);` |
|    173 | 5193 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 5194 | `	sxu32 n;` |
|    173 | 5195 | `	if( nAvail < 4 ){ return 0; } /* shortest entities: &lt; &#9; */` |
|    169 | 5196 | `	if( z[1] == '#' ){` |
|      - | 5197 | `		/* Numeric reference */` |
|     89 | 5198 | `		sxu32 cp = 0;` |
|     89 | 5199 | `		int i = 2, bHex = 0, nDig = 0;` |
|     89 | 5200 | `		if( z[i]=='x' \|\| z[i]=='X' ){ bHex = 1; i++; }` |
|    317 | 5201 | `		for( ; i < nAvail && z[i] != ';' ; i++ ){` |
|      - | 5202 | `			int v;` |
|    221 | 5203 | `			unsigned char c = z[i];` |
|    221 | 5204 | `			if( c>='0' && c<='9' ){ v = c - '0'; }` |
|     17 | 5205 | `			else if( bHex && c>='a' && c<='f' ){ v = c - 'a' + 10; }` |
|     17 | 5206 | `			else if( bHex && c>='A' && c<='F' ){ v = c - 'A' + 10; }` |
|    ! 0 | 5207 | `			else { return 0; }` |
|      - | 5208 | `			/* Stop accumulating once out of range (keeps validating the shape;` |
|      - | 5209 | `			 * max intermediate is 0x10FFFF*16+15, no sxu32 overflow). */` |
|    221 | 5210 | `			if( cp <= 0x10FFFF ){ cp = cp * (bHex ? 16 : 10) + (sxu32)v; }` |
|    221 | 5211 | `			nDig++;` |
|    111 | 5212 | `		}` |
|     97 | 5213 | `		if( nDig == 0 \|\| i >= nAvail ){ return 0; } /* no digits / no ';' */` |
|     97 | 5214 | `		if( bEncodeCheck ? !HtmlNumericAllowed(cp,iFlags) : !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|     83 | 5215 | `		if( !bFull ){` |
|      - | 5216 | `			/* hsc_decode: numeric refs to the five specials only. */` |
|     99 | 5217 | `			for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) && aHtmlSpecEnt[n].cp != cp ; n++ ){}` |
|     25 | 5218 | `			if( n >= SX_ARRAYSIZE(aHtmlSpecEnt) ){ return 0; }` |
|     11 | 5219 | `		}` |
|     75 | 5220 | `		*pCp = cp;` |
|     75 | 5221 | `		*pnConsumed = i + 1;` |
|     75 | 5222 | `		return 1;` |
|      - | 5223 | `	}` |
|      - | 5224 | `	/* Named reference — every entity name starts with a letter, so anything` |
|      - | 5225 | `	 * else can bail out before touching the tables. */` |
|     81 | 5226 | `	if( !((z[1]>='a' && z[1]<='z') \|\| (z[1]>='A' && z[1]<='Z')) ){ return 0; }` |
|    287 | 5227 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) ; n++ ){` |
|    265 | 5228 | `		if( aHtmlSpecEnt[n].cp == 39 && iDoc == PH7_ENT_DOC_HTML401 ){ continue; }` |
|    243 | 5229 | `		if( nAvail >= aHtmlSpecEnt[n].n && SyMemcmp(z,aHtmlSpecEnt[n].zEnt,(sxu32)aHtmlSpecEnt[n].n) == 0 ){` |
|     53 | 5230 | `			*pCp = aHtmlSpecEnt[n].cp;` |
|     53 | 5231 | `			*pnConsumed = aHtmlSpecEnt[n].n;` |
|     53 | 5232 | `			return 1;` |
|      - | 5233 | `		}` |
|     96 | 5234 | `	}` |
|     23 | 5235 | `	if( bFull && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 5236 | `		/* Linear scan of the 248-row table: runs only at '&'-then-letter` |
|      - | 5237 | `		 * positions and guarantees the decode set can never drift from the` |
|      - | 5238 | `		 * encode table. The first-letter guard skips the SyStrlen/SyMemcmp` |
|      - | 5239 | `		 * for ~96% of rows. */` |
|   3369 | 5240 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|      - | 5241 | `			sxu32 nEnt;` |
|   3357 | 5242 | `			if( z[1] != (unsigned char)aHtml401Ent[n].zEnt[1] ){ continue; }` |
|    121 | 5243 | `			nEnt = SyStrlen(aHtml401Ent[n].zEnt);` |
|    121 | 5244 | `			if( (sxu32)nAvail >= nEnt && SyMemcmp(z,aHtml401Ent[n].zEnt,nEnt) == 0 ){` |
|      7 | 5245 | `				*pCp = aHtml401Ent[n].cp;` |
|      7 | 5246 | `				*pnConsumed = (int)nEnt;` |
|      7 | 5247 | `				return 1;` |
|      - | 5248 | `			}` |
|     58 | 5249 | `		}` |
|      6 | 5250 | `	}` |
|     17 | 5251 | `	return 0;` |
|     88 | 5252 | `}` |
|      - | 5253 | `/* Shared encoder for htmlspecialchars (bAll=0) and htmlentities (bAll=1).` |
|      - | 5254 | ` * Invalid UTF-8 policy: ENT_IGNORE drops the byte (and wins over SUBSTITUTE),` |
|      - | 5255 | ` * ENT_SUBSTITUTE emits one U+FFFD per invalid byte, neither -> the whole` |
|      - | 5256 | ` * result is "" (pre-validated in a first pass: the accumulating result API` |
|      - | 5257 | ` * cannot roll back — same reason FvSanitizeFull is two-pass). */` |
|     94 | 5258 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 5259 | `                       int iFlags,int bAll,int bDoubleEncode){` |
|     95 | 5260 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     95 | 5261 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|      - | 5262 | `	const unsigned char *runStart;` |
|     95 | 5263 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 5264 | `	sxu32 cp;` |
|     95 | 5265 | `	if( (iFlags & (PH7_ENT_IGNORE\|PH7_ENT_SUBSTITUTE)) == 0 ){` |
|      - | 5266 | `		/* Pass 1: any malformed sequence rejects the entire input. ASCII` |
|      - | 5267 | `		 * bytes cannot be malformed, so skip them without the decoder. */` |
|    381 | 5268 | `		while( p < zEnd ){` |
|      - | 5269 | `			int len;` |
|    323 | 5270 | `			if( *p < 0x80 ){ p++; continue; }` |
|     37 | 5271 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     37 | 5272 | `			if( len == 0 ){ ph7_result_string(pCtx,"",0); return; }` |
|     27 | 5273 | `			p += len;` |
|      1 | 5274 | `		}` |
|     59 | 5275 | `		p = (const unsigned char *)zIn;` |
|     29 | 5276 | `	}` |
|     85 | 5277 | `	runStart = p;` |
|     85 | 5278 | `	ph7_result_string(pCtx,"",0);` |
|    455 | 5279 | `	while( p < zEnd ){` |
|    371 | 5280 | `		const char *zEnt = 0;` |
|      - | 5281 | `		int len;` |
|    371 | 5282 | `		if( *p < 0x80 ){` |
|    307 | 5283 | `			len = 1;` |
|    307 | 5284 | `			switch( *p ){` |
|     25 | 5285 | `			case '<': zEnt = "&lt;"; break;` |
|     25 | 5286 | `			case '>': zEnt = "&gt;"; break;` |
|     18 | 5287 | `			case '&':` |
|     37 | 5288 | `				zEnt = "&amp;";` |
|     37 | 5289 | `				if( !bDoubleEncode ){` |
|      - | 5290 | `					sxu32 eCp; int nEat;` |
|     25 | 5291 | `					if( HtmlParseEntity(p,zEnd,iFlags,1,1,&eCp,&nEat) ){` |
|      - | 5292 | `						/* A valid existing entity: keep it verbatim. */` |
|     13 | 5293 | `						zEnt = 0;` |
|     13 | 5294 | `						len = nEat;` |
|      6 | 5295 | `					}` |
|     12 | 5296 | `				}` |
|     37 | 5297 | `				break;` |
|     10 | 5298 | `			case '"':` |
|     21 | 5299 | `				if( iFlags & PH7_ENT_QUOTE_DOUBLE ){ zEnt = "&quot;"; }` |
|     21 | 5300 | `				break;` |
|     12 | 5301 | `			case '\'':` |
|     25 | 5302 | `				if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|     23 | 5303 | `					zEnt = HtmlAposEntity(iDoc,bAll);` |
|     11 | 5304 | `				}` |
|     25 | 5305 | `				break;` |
|     89 | 5306 | `			default:` |
|    179 | 5307 | `				if( (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode((sxu32)*p,iFlags) ){` |
|    ! 0 | 5308 | `					zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 5309 | `				}` |
|    178 | 5310 | `				break;` |
|      - | 5311 | `			}` |
|    154 | 5312 | `		}else{` |
|     65 | 5313 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     65 | 5314 | `			if( len == 0 ){` |
|      - | 5315 | `				/* Malformed subpart (IGNORE or SUBSTITUTE is set, else pass 1` |
|      - | 5316 | `				 * would have rejected): drop it or emit ONE U+FFFD for the` |
|      - | 5317 | `				 * whole unit (php substitutes per maximal invalid subpart). */` |
|     15 | 5318 | `				if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|     15 | 5319 | `				if( (iFlags & PH7_ENT_IGNORE) == 0 ){ ph7_result_string(pCtx,"\xEF\xBF\xBD",3); }` |
|     15 | 5320 | `				p += HtmlUtf8FailAdvance(p,zEnd);` |
|     15 | 5321 | `				runStart = p;` |
|     15 | 5322 | `				continue;` |
|      - | 5323 | `			}` |
|     51 | 5324 | `			if( bAll && HtmlDocHasNamedTable(iDoc) ){` |
|     41 | 5325 | `				zEnt = FvHtml401Lookup(cp);` |
|     20 | 5326 | `			}` |
|     51 | 5327 | `			if( zEnt == 0 && (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode(cp,iFlags) ){` |
|    ! 0 | 5328 | `				zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 5329 | `			}` |
|      - | 5330 | `		}` |
|    357 | 5331 | `		if( zEnt ){` |
|    135 | 5332 | `			if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|    135 | 5333 | `			ph7_result_string(pCtx,zEnt,-1);` |
|    135 | 5334 | `			runStart = p + len;` |
|     67 | 5335 | `		}` |
|    357 | 5336 | `		p += len;` |
|      1 | 5337 | `	}` |
|     85 | 5338 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     48 | 5339 | `}` |
|      - | 5340 | `/* Shared decoder for html_entity_decode (bFull=1) and htmlspecialchars_decode` |
|      - | 5341 | ` * (bFull=0). Quote refs (cp 34/39, named or numeric) are gated by the quote` |
|      - | 5342 | ` * bits and left verbatim when suppressed; an invalid entity leaves its '&'` |
|      - | 5343 | ` * verbatim and rescans right after it, which also yields PHP's no-double-` |
|      - | 5344 | ` * decode behavior ("&amp;lt;" -> "&lt;"). */` |
|     82 | 5345 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 5346 | `                         int iFlags,int bFull){` |
|     83 | 5347 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     83 | 5348 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|     83 | 5349 | `	const unsigned char *runStart = p;` |
|     83 | 5350 | `	ph7_result_string(pCtx,"",0);` |
|    557 | 5351 | `	while( p < zEnd ){` |
|      - | 5352 | `		sxu32 cp;` |
|      - | 5353 | `		int nEat;` |
|    510 | 5354 | `		if( *p != '&' ){ p++; continue; }` |
|    155 | 5355 | `		if( !HtmlParseEntity(p,zEnd,iFlags,bFull,0,&cp,&nEat) ){ p++; continue; }` |
|    142 | 5356 | `		if( (cp == 34 && (iFlags & PH7_ENT_QUOTE_DOUBLE) == 0)` |
|    117 | 5357 | `		 \|\| (cp == 39 && (iFlags & PH7_ENT_QUOTE_SINGLE) == 0) ){` |
|      - | 5358 | `			/* Suppressed quote: leave the entity source verbatim. */` |
|     37 | 5359 | `			p += nEat;` |
|     37 | 5360 | `			continue;` |
|      - | 5361 | `		}` |
|     89 | 5362 | `		if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|      - | 5363 | `		{` |
|      - | 5364 | `			char zBuf[4];` |
|     89 | 5365 | `			int n = HtmlCpUtf8(cp,zBuf);` |
|     89 | 5366 | `			ph7_result_string(pCtx,zBuf,n);` |
|      - | 5367 | `		}` |
|     89 | 5368 | `		p += nEat;` |
|     89 | 5369 | `		runStart = p;` |
|      1 | 5370 | `	}` |
|     79 | 5371 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     79 | 5372 | `}` |
|      - | 5373 | `/* Validate the optional charset argument at apArg[idx]: UTF-8 aliases (and` |
|      - | 5374 | ` * ""/NULL meaning the default) are accepted; anything else — including` |
|      - | 5375 | ` * php-supported single-byte charsets like ISO-8859-1, PHL is UTF-8-only per` |
|      - | 5376 | ` * PLAN.md §6 — raises PHP's unsupported-charset warning and is treated as` |
|      - | 5377 | ` * UTF-8 (ph7_context_throw_error_format prepends the function name). */` |
|    141 | 5378 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx){` |
|      - | 5379 | `	const char *zCs;` |
|      - | 5380 | `	int nCs;` |
|    148 | 5381 | `	if( nArg <= idx \|\| ph7_value_is_null(apArg[idx]) ){ return; }` |
|     15 | 5382 | `	zCs = ph7_value_to_string(apArg[idx],&nCs);` |
|     15 | 5383 | `	if( nCs == 0 ){ return; } /* "" selects the default charset (UTF-8) */` |
|     13 | 5384 | `	if( nCs == 5 && SyStrnicmp(zCs,"UTF-8",5) == 0 ){` |
|     13 | 5385 | `		return; /* php accepts only "UTF-8" (any case) silently — "UTF8" warns */` |
|      - | 5386 | `	}` |
|    ! 0 | 5387 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5388 | `		"Charset \"%.*s\" is not supported, assuming UTF-8",nCs,zCs);` |
|     71 | 5389 | `}` |
|      - | 5390 | `/* get_html_translation_table() worker: character (UTF-8 bytes) => entity.` |
|      - | 5391 | ` * The five specials come first in byte order, then — for HTML_ENTITIES with a` |
|      - | 5392 | ` * named-table doctype — the 248 aHtml401Ent rows ascending (oracle-pinned` |
|      - | 5393 | ` * ordering; 253 entries under the defaults). */` |
|    549 | 5394 | `static void HtmlTableAdd(ph7_value *pArray,ph7_value *pValue,const char *zKey,const char *zEnt){` |
|    549 | 5395 | `	ph7_value_string(pValue,zEnt,-1);` |
|    549 | 5396 | `	ph7_array_add_strkey_elem(pArray,zKey,pValue);` |
|    549 | 5397 | `	ph7_value_reset_string_cursor(pValue);` |
|    549 | 5398 | `}` |
|     13 | 5399 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags){` |
|      - | 5400 | `	ph7_value *pArray,*pValue;` |
|     13 | 5401 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 5402 | `	sxu32 n;` |
|     13 | 5403 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 5404 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 5405 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|    ! 0 | 5406 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5407 | `		return;` |
|      - | 5408 | `	}` |
|     13 | 5409 | `	if( iFlags & PH7_ENT_QUOTE_DOUBLE ){` |
|     11 | 5410 | `		HtmlTableAdd(pArray,pValue,"\"","&quot;");` |
|      5 | 5411 | `	}` |
|     13 | 5412 | `	HtmlTableAdd(pArray,pValue,"&","&amp;");` |
|     13 | 5413 | `	if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|      - | 5414 | `		/* The apostrophe row mirrors the function each table belongs to:` |
|      - | 5415 | `		 * SPECIALCHARS follows htmlspecialchars, ENTITIES follows` |
|      - | 5416 | `		 * htmlentities (oracle-pinned at flags 35). */` |
|      7 | 5417 | `		HtmlTableAdd(pArray,pValue,"'",HtmlAposEntity(iDoc,iTable != 0));` |
|      3 | 5418 | `	}` |
|     13 | 5419 | `	HtmlTableAdd(pArray,pValue,"<","&lt;");` |
|     13 | 5420 | `	HtmlTableAdd(pArray,pValue,">","&gt;");` |
|     13 | 5421 | `	if( iTable != 0 /*php: any non-HTML_SPECIALCHARS table => entities*/ && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 5422 | `		char zKey[8];` |
|    499 | 5423 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|    497 | 5424 | `			int nK = HtmlCpUtf8(aHtml401Ent[n].cp,zKey);` |
|    497 | 5425 | `			zKey[nK] = 0;` |
|    497 | 5426 | `			HtmlTableAdd(pArray,pValue,zKey,aHtml401Ent[n].zEnt);` |
|    249 | 5427 | `		}` |
|      1 | 5428 | `	}` |
|     13 | 5429 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 5430 | `}` |
|     25 | 5431 | `static int FvEmailAllowed(unsigned char c){` |
|     25 | 5432 | `	if( (c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9') ){ return 1; }` |
|     16 | 5433 | `	return c=='!'\|\|c=='#'\|\|c=='$'\|\|c=='%'\|\|c=='&'\|\|c=='\''\|\|c=='*'\|\|c=='+'` |
|     10 | 5434 | ``	    \|\| c=='-'\|\|c=='='\|\|c=='?'\|\|c=='^'\|\|c=='_'\|\|c=='`'\|\|c=='{'\|\|c=='\|'`` |
|     15 | 5435 | `	    \|\| c=='}'\|\|c=='~'\|\|c=='@'\|\|c=='.'\|\|c=='['\|\|c==']';` |
|     13 | 5436 | `}` |
|     23 | 5437 | `static int FvUrlAllowed(unsigned char c){` |
|     23 | 5438 | `	return c>=33 && c<=126; /* PHP keeps every printable ASCII byte except space */` |
|      1 | 5439 | `}` |
|      - | 5440 | `/* SANITIZE_EMAIL (isUrl=0) / SANITIZE_URL (isUrl=1): strip disallowed bytes. */` |
|      5 | 5441 | `static void FvSanitizeChars(ph7_context *pCtx,const char *z,int n,int isUrl){` |
|      5 | 5442 | `	int i, runStart = 0;` |
|      5 | 5443 | `	ph7_result_string(pCtx,"",0);` |
|     51 | 5444 | `	for( i=0; i<n; i++ ){` |
|     47 | 5445 | `		unsigned char c = (unsigned char)z[i];` |
|     47 | 5446 | `		if( !(isUrl ? FvUrlAllowed(c) : FvEmailAllowed(c)) ){` |
|     11 | 5447 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     11 | 5448 | `			runStart = i+1;` |
|      5 | 5449 | `		}` |
|     24 | 5450 | `	}` |
|      5 | 5451 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      5 | 5452 | `}` |
|      - | 5453 | `/*` |
|      - | 5454 | ` * Apply the selected filter to one already-resolved input value and write the` |
|      - | 5455 | ` * result into pCtx. Shared by filter_var() and filter_input(): the caller has` |
|      - | 5456 | ` * already parsed $filter/$flags/$options. On validation failure the 'default'` |
|      - | 5457 | ` * option (if any) is returned, else null when FILTER_NULL_ON_FAILURE is set,` |
|      - | 5458 | ` * else false. A validating filter that passes returns the (string) input` |
|      - | 5459 | ` * unchanged; a sanitizer writes its transformed output directly.` |
|      - | 5460 | ` */` |
|    316 | 5461 | `static int FvApplyFilter(ph7_context *pCtx,ph7_value *pInput,` |
|      - | 5462 | `                         int iFilter,int iFlags,ph7_value *pOpts,` |
|      - | 5463 | `                         ph7_value *pDefault)` |
|      3 | 5464 | `{` |
|    319 | 5465 | `	int bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|      - | 5466 | `	const char *zVal; int nVal;` |
|      - | 5467 | `	/* An array/object input fails every scalar filter. */` |
|    319 | 5468 | `	if( ph7_value_is_array(pInput) ){ goto fail; }` |
|    317 | 5469 | `	zVal = ph7_value_to_string(pInput,&nVal);` |
|    317 | 5470 | `	switch( iFilter ){` |
|     28 | 5471 | `	case FV_VALIDATE_INT: {` |
|      - | 5472 | `		ph7_int64 v;` |
|     58 | 5473 | `		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }` |
|     31 | 5474 | `		if( pOpts ){` |
|      7 | 5475 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|      7 | 5476 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|      7 | 5477 | `			if( pMin && v<ph7_value_to_int64(pMin) ){ goto fail; }` |
|      7 | 5478 | `			if( pMax && v>ph7_value_to_int64(pMax) ){ goto fail; }` |
|      2 | 5479 | `		}` |
|     29 | 5480 | `		ph7_result_int64(pCtx,v);` |
|     29 | 5481 | `		return PH7_OK;` |
|      - | 5482 | `	}` |
|     34 | 5483 | `	case FV_VALIDATE_FLOAT: {` |
|      - | 5484 | `		double d;` |
|     69 | 5485 | `		if( !FvValidateFloat(zVal,nVal,iFlags,&d) ){ goto fail; }` |
|     39 | 5486 | `		ph7_result_double(pCtx,d);` |
|     39 | 5487 | `		return PH7_OK;` |
|      - | 5488 | `	}` |
|     14 | 5489 | `	case FV_VALIDATE_BOOLEAN: {` |
|      - | 5490 | `		int b;` |
|     29 | 5491 | `		if( !FvValidateBool(zVal,nVal,&b) ){ goto fail; }` |
|     21 | 5492 | `		ph7_result_bool(pCtx,b);` |
|     21 | 5493 | `		return PH7_OK;` |
|      - | 5494 | `	}` |
|     25 | 5495 | `	case FV_VALIDATE_IP:     if( !FvValidateIp(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|     11 | 5496 | `	case FV_VALIDATE_MAC:    if( !FvValidateMac(zVal,nVal) ){ goto fail; }       goto pass;` |
|     28 | 5497 | `	case FV_VALIDATE_EMAIL:  if( !FvValidateEmail(zVal,nVal) ){ goto fail; }     goto pass;` |
|     11 | 5498 | `	case FV_VALIDATE_DOMAIN: if( !FvValidateDomain(zVal,nVal) ){ goto fail; }    goto pass;` |
|     15 | 5499 | `	case FV_VALIDATE_URL:    if( !FvValidateUrl(zVal,nVal) ){ goto fail; }       goto pass;` |
|      3 | 5500 | `	case FV_VALIDATE_REGEXP: {` |
|      - | 5501 | `#ifdef PH7_ENABLE_PCRE` |
|      8 | 5502 | `		ph7_value *pRe = pOpts ? ph7_array_fetch(pOpts,"regexp",(int)sizeof("regexp")-1) : 0;` |
|      8 | 5503 | `		const char *zRe; int nRe, matched = 0;` |
|      8 | 5504 | `		if( pRe==0 ){` |
|      3 | 5505 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5506 | `				"filter_var(): \"regexp\" option is missing");` |
|      - | 5507 | `		}` |
|      5 | 5508 | `		zRe = ph7_value_to_string(pRe,&nRe);` |
|      5 | 5509 | `		if( PH7_PcreMatchQuiet(pCtx,zRe,nRe,zVal,nVal,&matched)!=SXRET_OK \|\| !matched ){ goto fail; }` |
|      3 | 5510 | `		goto pass;` |
|      - | 5511 | `#else` |
|      - | 5512 | `		goto fail;` |
|      - | 5513 | `#endif` |
|      - | 5514 | `	}` |
|      3 | 5515 | `	case FV_SANITIZE_NUMBER_INT:   FvSanitizeNumber(pCtx,zVal,nVal,0,0);      return PH7_OK;` |
|      5 | 5516 | `	case FV_SANITIZE_NUMBER_FLOAT: FvSanitizeNumber(pCtx,zVal,nVal,1,iFlags); return PH7_OK;` |
|     13 | 5517 | `	case FV_SANITIZE_SPECIAL_CHARS:      FvSanitizeSpecial(pCtx,zVal,nVal,iFlags); return PH7_OK;` |
|     25 | 5518 | `	case FV_SANITIZE_FULL_SPECIAL_CHARS: FvSanitizeFull(pCtx,zVal,nVal,iFlags);    return PH7_OK;` |
|      3 | 5519 | `	case FV_SANITIZE_EMAIL: FvSanitizeChars(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 5520 | `	case FV_SANITIZE_URL:   FvSanitizeChars(pCtx,zVal,nVal,1); return PH7_OK;` |
|     13 | 5521 | `	case FV_DEFAULT:` |
|      - | 5522 | `		/* FILTER_UNSAFE_RAW / FILTER_DEFAULT: pass through unchanged unless a` |
|      - | 5523 | `		 * STRIP/ENCODE flag is set, in which case apply the string filter. */` |
|     28 | 5524 | `		if( iFlags & FV_FLAG_STRING_MASK ){` |
|     15 | 5525 | `			FvSanitizeString(pCtx,zVal,nVal,iFlags);` |
|     15 | 5526 | `			return PH7_OK;` |
|      - | 5527 | `		}` |
|     14 | 5528 | `		goto pass;` |
|    ! 0 | 5529 | `	default:` |
|    ! 0 | 5530 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5531 | `			"Unknown filter with ID %d",iFilter);` |
|    ! 0 | 5532 | `		break; /* unknown filter id -> fail */` |
|    ! 0 | 5533 | `	}` |
|     58 | 5534 | `fail:` |
|    118 | 5535 | `	if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|    114 | 5536 | `	else if( bNull ){ ph7_result_null(pCtx); }` |
|    108 | 5537 | `	else { ph7_result_bool(pCtx,0); }` |
|    118 | 5538 | `	return PH7_OK;` |
|     26 | 5539 | `pass: /* validation passed: return the (string) input unchanged */` |
|     54 | 5540 | `	ph7_result_string(pCtx,zVal,nVal);` |
|     54 | 5541 | `	return PH7_OK;` |
|    161 | 5542 | `}` |
|      - | 5543 | `/*` |
|      - | 5544 | ` * Parse the ($filter, $options) pair shared by filter_var()/filter_input() out` |
|      - | 5545 | ` * of apArg[iBase] ($filter) and apArg[iBase+1] ($options): $options is either a` |
|      - | 5546 | ` * plain flags int, or an array with 'flags' and an 'options' sub-array (whose` |
|      - | 5547 | ` * 'default' entry is the fallback value). Fills the four output pointers;` |
|      - | 5548 | ` * unset outputs keep the caller-provided defaults.` |
|      - | 5549 | ` */` |
|    328 | 5550 | `static void FvParseFilterArgs(int nArg,ph7_value **apArg,int iBase,` |
|      - | 5551 | `                              int *piFilter,int *piFlags,` |
|      - | 5552 | `                              ph7_value **ppOpts,ph7_value **ppDefault)` |
|      3 | 5553 | `{` |
|    331 | 5554 | `	if( nArg>iBase ){ *piFilter = ph7_value_to_int(apArg[iBase]); }` |
|    331 | 5555 | `	if( nArg>iBase+1 ){` |
|     88 | 5556 | `		if( ph7_value_is_array(apArg[iBase+1]) ){` |
|     42 | 5557 | `			ph7_value *pF = ph7_array_fetch(apArg[iBase+1],"flags",(int)sizeof("flags")-1);` |
|     42 | 5558 | `			if( pF ){ *piFlags = ph7_value_to_int(pF); }` |
|     42 | 5559 | `			*ppOpts = ph7_array_fetch(apArg[iBase+1],"options",(int)sizeof("options")-1);` |
|     42 | 5560 | `			if( *ppOpts && !ph7_value_is_array(*ppOpts) ){ *ppOpts = 0; }` |
|     42 | 5561 | `			if( *ppOpts ){ *ppDefault = ph7_array_fetch(*ppOpts,"default",(int)sizeof("default")-1); }` |
|     22 | 5562 | `		}else{` |
|     48 | 5563 | `			*piFlags = ph7_value_to_int(apArg[iBase+1]);` |
|      - | 5564 | `		}` |
|     43 | 5565 | `	}` |
|    331 | 5566 | `}` |
|      - | 5567 | `/*` |
|      - | 5568 | ` * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 5569 | ` *  Validate or sanitize a value; see FvApplyFilter for the failure semantics.` |
|      - | 5570 | ` */` |
|    306 | 5571 | `static int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5572 | `{` |
|    308 | 5573 | `	int iFilter = FV_DEFAULT, iFlags = 0;` |
|    308 | 5574 | `	ph7_value *pOpts = 0, *pDefault = 0;` |
|    308 | 5575 | `	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    308 | 5576 | `	FvParseFilterArgs(nArg,apArg,1,&iFilter,&iFlags,&pOpts,&pDefault);` |
|    308 | 5577 | `	return FvApplyFilter(pCtx,apArg[0],iFilter,iFlags,pOpts,pDefault);` |
|    155 | 5578 | `}` |
|      - | 5579 | `/*` |
|      - | 5580 | ` * filter_input($type, $var_name, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 5581 | ` *  Look up $var_name in the requested INPUT_* superglobal, then apply the` |
|      - | 5582 | ` *  filter. Semantics verified byte-for-byte against php 8.5:` |
|      - | 5583 | ` *   - variable NOT set: 'default' option wins, else false when` |
|      - | 5584 | ` *     FILTER_NULL_ON_FAILURE is set, else null. (Note the null/false roles are` |
|      - | 5585 | ` *     INVERTED relative to a present value that fails validation, which yields` |
|      - | 5586 | ` *     default > null-if-NULL_ON_FAILURE > false via FvApplyFilter.)` |
|      - | 5587 | ` *   - variable present: delegate to FvApplyFilter.` |
|      - | 5588 | ` *  Divergence: php reads a SAPI snapshot of the original request variables` |
|      - | 5589 | ` *  captured at startup; PHL reads the live superglobal. In CLI they match for` |
|      - | 5590 | ` *  the SAPI-registered keys (SCRIPT_NAME/PHP_SELF/DOCUMENT_ROOT); keys added` |
|      - | 5591 | ` *  only to the live $_SERVER (REQUEST_TIME/PWD/…) are visible here but not in` |
|      - | 5592 | ` *  php's snapshot.` |
|      - | 5593 | ` */` |
|     28 | 5594 | `static int PH7_builtin_filter_input(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5595 | `{` |
|     30 | 5596 | `	int iType, iFilter = FV_DEFAULT, iFlags = 0;` |
|     30 | 5597 | `	ph7_value *pOpts = 0, *pDefault = 0, *pSuper, *pElem;` |
|      - | 5598 | `	const char *zVar, *zSuper; int nVar; sxu32 nSuper;` |
|     30 | 5599 | `	if( nArg<2 ){` |
|      7 | 5600 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|      2 | 5601 | `			"filter_input() expects at least 2 arguments, %d given",nArg);` |
|      - | 5602 | `	}` |
|     26 | 5603 | `	iType = ph7_value_to_int(apArg[0]);` |
|     26 | 5604 | `	switch( iType ){` |
|      3 | 5605 | `	case 0: zSuper = "_POST";   nSuper = (sxu32)sizeof("_POST")-1;   break; /* INPUT_POST */` |
|      3 | 5606 | `	case 1: zSuper = "_GET";    nSuper = (sxu32)sizeof("_GET")-1;    break; /* INPUT_GET */` |
|    ! 0 | 5607 | `	case 2: zSuper = "_COOKIE"; nSuper = (sxu32)sizeof("_COOKIE")-1; break; /* INPUT_COOKIE */` |
|    ! 0 | 5608 | `	case 4: zSuper = "_ENV";    nSuper = (sxu32)sizeof("_ENV")-1;    break; /* INPUT_ENV */` |
|     19 | 5609 | `	case 5: zSuper = "_SERVER"; nSuper = (sxu32)sizeof("_SERVER")-1; break; /* INPUT_SERVER */` |
|      1 | 5610 | `	default:` |
|      3 | 5611 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5612 | `			"filter_input(): Argument #1 ($type) must be an INPUT_* constant");` |
|      - | 5613 | `	}` |
|     23 | 5614 | `	zVar = ph7_value_to_string(apArg[1],&nVar);` |
|     23 | 5615 | `	FvParseFilterArgs(nArg,apArg,2,&iFilter,&iFlags,&pOpts,&pDefault);` |
|      - | 5616 | `	/* Resolve the variable from the superglobal (missing/non-array -> not set). */` |
|     23 | 5617 | `	pSuper = PH7_VmExtractSuper(pCtx->pVm,zSuper,nSuper);` |
|     23 | 5618 | `	pElem = (pSuper && ph7_value_is_array(pSuper))` |
|     33 | 5619 | `		? ph7_array_fetch(pSuper,zVar,nVar) : 0;` |
|     23 | 5620 | `	if( pElem==0 ){` |
|      - | 5621 | `		/* Variable not set: default > false(if NULL_ON_FAILURE) > null. Note the` |
|      - | 5622 | `		 * false/null roles are inverted vs FvApplyFilter's present-but-fails path. */` |
|     13 | 5623 | `		if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|      9 | 5624 | `		else if( iFlags & FV_NULL_ON_FAILURE ){ ph7_result_bool(pCtx,0); }` |
|      7 | 5625 | `		else { ph7_result_null(pCtx); }` |
|     13 | 5626 | `		return PH7_OK;` |
|      - | 5627 | `	}` |
|     11 | 5628 | `	return FvApplyFilter(pCtx,pElem,iFilter,iFlags,pOpts,pDefault);` |
|     16 | 5629 | `}` |
|      - | 5630 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 5631 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 5632 | `/*` |
|      - | 5633 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 5634 |  |
|      - | 5635 | ` */` |
|      4 | 5636 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 5637 | `	const char *zInput, /* Raw input */` |
|      - | 5638 | `	int nByte,  /* Input length */` |
|      - | 5639 | `	int delim,  /* Delimiter */` |
|      - | 5640 | `	int encl,   /* Enclosure */` |
|      - | 5641 | `	int escape,  /* Escape character */` |
|      - | 5642 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 5643 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 5644 | `	)` |
|      1 | 5645 | `{` |
|      5 | 5646 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 5647 | `	const char *zIn = zInput;` |
|      - | 5648 | `	const char *zPtr;` |
|      - | 5649 | `	int isEnc;` |
|      - | 5650 | `	/* Start processing */` |
|      8 | 5651 | `	for(;;){` |
|     17 | 5652 | `		if( zIn >= zEnd ){` |
|      - | 5653 | `			/* No more input to process */` |
|      5 | 5654 | `			break;` |
|      - | 5655 | `		}` |
|     13 | 5656 | `		isEnc = 0;` |
|     13 | 5657 | `		zPtr = zIn;` |
|      - | 5658 | `		/* Find the first delimiter */` |
|     27 | 5659 | `		while( zIn < zEnd ){` |
|     23 | 5660 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 5661 | `				/* Delimiter found,break imediately */` |
|      5 | 5662 | `				break;` |
|     15 | 5663 | `			}else if( zIn[0] == encl ){` |
|      - | 5664 | `				/* Inside enclosure? */` |
|    ! 0 | 5665 | `				isEnc = !isEnc;` |
|     15 | 5666 | `			}else if( zIn[0] == escape ){` |
|      - | 5667 | `				/* Escape sequence */` |
|    ! 0 | 5668 | `				zIn++;` |
|    ! 0 | 5669 | `			}` |
|      - | 5670 | `			/* Advance the cursor */` |
|     15 | 5671 | `			zIn++;` |
|      1 | 5672 | `		}` |
|     13 | 5673 | `		if( zIn > zPtr ){` |
|     13 | 5674 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 5675 | `			sxi32 rc;` |
|      - | 5676 | `			/* Invoke the supllied callback */` |
|     13 | 5677 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 5678 | `				zPtr++;` |
|    ! 0 | 5679 | `				nByteChunk-=2;` |
|    ! 0 | 5680 | `			}` |
|     13 | 5681 | `			if( nByteChunk > 0 ){` |
|     13 | 5682 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 5683 | `				if( rc == SXERR_ABORT ){` |
|      - | 5684 | `					/* User callback request an operation abort */` |
|    ! 0 | 5685 | `					break;` |
|      - | 5686 | `				}` |
|      6 | 5687 | `			}` |
|      6 | 5688 | `		}` |
|      - | 5689 | `		/* Ignore trailing delimiter */` |
|     21 | 5690 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 5691 | `			zIn++;` |
|      1 | 5692 | `		}` |
|      1 | 5693 | `	}` |
|      5 | 5694 | `	return SXRET_OK;` |
|      1 | 5695 | `}` |
|      - | 5696 | `/*` |
|      - | 5697 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 5698 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 5699 | ` * argument to this callback.` |
|      - | 5700 | ` */` |
|     12 | 5701 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 5702 | `{` |
|     13 | 5703 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 5704 | `	ph7_value sEntry;` |
|      - | 5705 | `	SyString sToken;` |
|      - | 5706 | `	/* Insert the token in the given array */` |
|     13 | 5707 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 5708 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 5709 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 5710 | `	if( sToken.nByte < 1){` |
|    ! 0 | 5711 | `		return SXRET_OK;` |
|      - | 5712 | `	}` |
|     13 | 5713 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 5714 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 5715 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 5716 | `	return SXRET_OK;` |
|      7 | 5717 | `}` |
|      - | 5718 | `/*` |
|      - | 5719 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 5720 | ` *  Parse a CSV string into an array.` |
|      - | 5721 | ` * Parameters` |
|      - | 5722 | ` *  $input` |
|      - | 5723 | ` *   The string to parse.` |
|      - | 5724 | ` *  $delimiter` |
|      - | 5725 | ` *   Set the field delimiter (one character only).` |
|      - | 5726 | ` *  $enclosure` |
|      - | 5727 | ` *   Set the field enclosure character (one character only).` |
|      - | 5728 | ` *  $escape` |
|      - | 5729 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 5730 | ` * Return` |
|      - | 5731 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 5732 | ` */` |
|      4 | 5733 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5734 | `{` |
|      - | 5735 | `	const char *zInput,*zPtr;` |
|      - | 5736 | `	ph7_value *pArray;` |
|      5 | 5737 | `	int delim  = ',';   /* Delimiter */` |
|      5 | 5738 | `	int encl   = '"' ;  /* Enclosure */` |
|      5 | 5739 | `	int escape = '\\';  /* Escape character */` |
|      - | 5740 | `	int nLen;` |
|      5 | 5741 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5742 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 5743 | `		ph7_result_null(pCtx);` |
|      3 | 5744 | `		return PH7_OK;` |
|      - | 5745 | `	}` |
|      - | 5746 | `	/* Extract the raw input */` |
|      3 | 5747 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5748 | `	if( nArg > 1 ){` |
|      - | 5749 | `		int i;` |
|      3 | 5750 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 5751 | `			/* Extract the delimiter */` |
|      3 | 5752 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 5753 | `			if( i > 0 ){` |
|      3 | 5754 | `				delim = zPtr[0];` |
|      1 | 5755 | `			}` |
|      1 | 5756 | `		}` |
|      3 | 5757 | `		if( nArg > 2 ){` |
|      3 | 5758 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 5759 | `				/* Extract the enclosure */` |
|      3 | 5760 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 5761 | `				if( i > 0 ){` |
|      3 | 5762 | `					encl = zPtr[0];` |
|      1 | 5763 | `				}` |
|      1 | 5764 | `			}` |
|      3 | 5765 | `			if( nArg > 3 ){` |
|      3 | 5766 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 5767 | `					/* Extract the escape character */` |
|      3 | 5768 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 5769 | `					if( i > 0 ){` |
|      3 | 5770 | `						escape = zPtr[0];` |
|      1 | 5771 | `					}` |
|      1 | 5772 | `				}` |
|      1 | 5773 | `			}` |
|      1 | 5774 | `		}` |
|      1 | 5775 | `	}` |
|      - | 5776 | `	/* Create our array */` |
|      3 | 5777 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5778 | `	if( pArray == 0 ){` |
|      - | 5779 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|    ! 0 | 5780 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5781 | `	}` |
|      - | 5782 | `	/* Parse the raw input */` |
|      3 | 5783 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 5784 | `	/* Return the freshly created array */` |
|      3 | 5785 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5786 | `	return PH7_OK;` |
|      3 | 5787 | `}` |
|      - | 5788 | `/*` |
|      - | 5789 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 5790 | ` * container.` |
|      - | 5791 | ` * Refer to [strip_tags()].` |
|      - | 5792 | ` */` |
|     10 | 5793 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5794 | `{` |
|     11 | 5795 | `	const char *zEnd = &zTag[nByte];` |
|      - | 5796 | `	const char *zPtr;` |
|      - | 5797 | `	SyString sEntry;` |
|      - | 5798 | `	/* Strip tags */` |
|     10 | 5799 | `	for(;;){` |
|     45 | 5800 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 5801 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 5802 | `				zTag++;` |
|      1 | 5803 | `		}` |
|     21 | 5804 | `		if( zTag >= zEnd ){` |
|     11 | 5805 | `			break;` |
|      - | 5806 | `		}` |
|     11 | 5807 | `		zPtr = zTag;` |
|      - | 5808 | `		/* Delimit the tag */` |
|     25 | 5809 | `		while(zTag < zEnd ){` |
|     25 | 5810 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5811 | `				/* UTF-8 stream */` |
|      3 | 5812 | `				zTag++;` |
|      5 | 5813 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 5814 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 5815 | `				break;` |
|    ! 0 | 5816 | `			}else{` |
|     13 | 5817 | `				zTag++;` |
|      - | 5818 | `			}` |
|      1 | 5819 | `		}` |
|     11 | 5820 | `		if( zTag > zPtr ){` |
|      - | 5821 | `			/* Perform the insertion */` |
|     11 | 5822 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 5823 | `			SyStringFullTrim(&sEntry);` |
|     11 | 5824 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 5825 | `		}` |
|      - | 5826 | `		/* Jump the trailing '>' */` |
|     11 | 5827 | `		zTag++;` |
|      1 | 5828 | `	}` |
|     11 | 5829 | `	return SXRET_OK;` |
|      1 | 5830 | `}` |
|      - | 5831 | `/*` |
|      - | 5832 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 5833 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 5834 | ` * Refer to [strip_tags()].` |
|      - | 5835 | ` */` |
|     36 | 5836 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5837 | `{` |
|     37 | 5838 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 5839 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 5840 | `		SyString sTag;` |
|     85 | 5841 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 5842 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 5843 | `			zTag++;` |
|      1 | 5844 | `		}` |
|      - | 5845 | `		/* Delimit the tag */` |
|     25 | 5846 | `		zCur = zTag;` |
|     77 | 5847 | `		while(zTag < zEnd ){` |
|     77 | 5848 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5849 | `				/* UTF-8 stream */` |
|      5 | 5850 | `				zTag++;` |
|      9 | 5851 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 5852 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 5853 | `				break;` |
|    ! 0 | 5854 | `			}else{` |
|     49 | 5855 | `				zTag++;` |
|      - | 5856 | `			}` |
|      1 | 5857 | `		}` |
|     25 | 5858 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 5859 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 5860 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 5861 | `		if( sTag.nByte > 0 ){` |
|      - | 5862 | `			SyString *aEntry,*pEntry;` |
|      - | 5863 | `			sxi32 rc;` |
|      - | 5864 | `			sxu32 n;` |
|      - | 5865 | `			/* Perform the lookup */` |
|     25 | 5866 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 5867 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 5868 | `				pEntry = &aEntry[n];` |
|      - | 5869 | `				/* Do the comparison */` |
|     25 | 5870 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 5871 | `				if( !rc ){` |
|     21 | 5872 | `					return SXRET_OK;` |
|      - | 5873 | `				}` |
|      3 | 5874 | `			}` |
|      2 | 5875 | `		}` |
|      2 | 5876 | `	}` |
|      - | 5877 | `	/* No such tag */` |
|     17 | 5878 | `	return SXERR_NOTFOUND;` |
|     19 | 5879 | `}` |
|      - | 5880 | `/*` |
|      - | 5881 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 5882 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 5883 | ` * Refer to [strip_tags()].` |
|      - | 5884 | ` */` |
|     16 | 5885 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 5886 | `{` |
|     17 | 5887 | `	const char *zEnd = &zIn[nByte];` |
|      - | 5888 | `	const char *zPtr,*zTag;` |
|      - | 5889 | `	SySet sSet;` |
|      - | 5890 | `	/* initialize the set of allowed tags */` |
|     17 | 5891 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 5892 | `	if( nTaglen > 0 ){` |
|      - | 5893 | `		/* Set of allowed tags */` |
|     11 | 5894 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 5895 | `	}` |
|      - | 5896 | `	/* Set the empty string */` |
|     17 | 5897 | `	ph7_result_string(pCtx,"",0);` |
|      - | 5898 | `	/* Start processing */` |
|     26 | 5899 | `	for(;;){` |
|     53 | 5900 | `		if(zIn >= zEnd){` |
|      - | 5901 | `			/* No more input to process */` |
|     15 | 5902 | `			break;` |
|      - | 5903 | `		}` |
|     39 | 5904 | `		zPtr = zIn;` |
|      - | 5905 | `		/* Find a tag */` |
|    133 | 5906 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 5907 | `			zIn++;` |
|      1 | 5908 | `		}` |
|     39 | 5909 | `		if( zIn > zPtr ){` |
|      - | 5910 | `			/* Consume raw input */` |
|     21 | 5911 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 5912 | `		}` |
|      - | 5913 | `		/* Ignore trailing null bytes */` |
|     39 | 5914 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 5915 | `			zIn++;` |
|    ! 0 | 5916 | `		}` |
|     39 | 5917 | `		if(zIn >= zEnd){` |
|      - | 5918 | `			/* No more input to process */` |
|      3 | 5919 | `			break;` |
|      - | 5920 | `		}` |
|     37 | 5921 | `		if( zIn[0] == '<' ){` |
|      - | 5922 | `			sxi32 rc;` |
|     37 | 5923 | `			zTag = zIn++;` |
|      - | 5924 | `			/* Delimit the tag */` |
|    127 | 5925 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 5926 | `				zIn++;` |
|      1 | 5927 | `			}` |
|     37 | 5928 | `			if( zIn < zEnd ){` |
|     37 | 5929 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 5930 | `			}` |
|      - | 5931 | `			/* Query the set */` |
|     37 | 5932 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 5933 | `			if( rc == SXRET_OK ){` |
|      - | 5934 | `				/* Keep the tag */` |
|     21 | 5935 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 5936 | `			}` |
|     18 | 5937 | `		}` |
|      1 | 5938 | `	}` |
|      - | 5939 | `	/* Cleanup */` |
|     17 | 5940 | `	SySetRelease(&sSet);` |
|     17 | 5941 | `	return SXRET_OK;` |
|      1 | 5942 | `}` |
|      - | 5943 | `/*` |
|      - | 5944 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 5945 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 5946 | ` * Parameters` |
|      - | 5947 | ` *  $str` |
|      - | 5948 | ` *  The input string.` |
|      - | 5949 | ` * $allowable_tags` |
|      - | 5950 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 5951 | ` * Return` |
|      - | 5952 | ` *  Returns the stripped string.` |
|      - | 5953 | ` */` |
|     16 | 5954 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5955 | `{` |
|     17 | 5956 | `	const char *zTaglist = 0;` |
|      - | 5957 | `	const char *zString;` |
|     17 | 5958 | `	int nTaglen = 0;` |
|      - | 5959 | `	int nLen;` |
|     17 | 5960 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5961 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 5962 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5963 | `		return PH7_OK;` |
|      - | 5964 | `	}` |
|      - | 5965 | `	/* Point to the raw string */` |
|     15 | 5966 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 5967 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 5968 | `		/* Allowed tag */` |
|     11 | 5969 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 5970 | `	}` |
|      - | 5971 | `	/* Process input */` |
|     15 | 5972 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 5973 | `	return PH7_OK;` |
|      9 | 5974 | `}` |
|      - | 5975 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5976 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5977 | `/*` |
|      - | 5978 | ` * string str_shuffle(string $str)` |
|      - | 5979 |  |
|      - | 5980 | ` *  Randomly shuffles a string.` |
|      - | 5981 | ` * Parameters` |
|      - | 5982 | ` *  $str` |
|      - | 5983 | ` *   The input string.` |
|      - | 5984 | ` * Return` |
|      - | 5985 | ` *  Returns the shuffled string.` |
|      - | 5986 | ` */` |
|     12 | 5987 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5988 | `{` |
|      - | 5989 | `	const char *zString;` |
|      - | 5990 | `	int nLen,i,c;` |
|      - | 5991 | `	sxu32 iR;` |
|     13 | 5992 | `	if( nArg < 1 ){` |
|      - | 5993 | `		/* Missing arguments,return the empty string */` |
|      3 | 5994 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5995 | `		return PH7_OK;` |
|      - | 5996 | `	}` |
|      - | 5997 | `	/* Extract the target string */` |
|     11 | 5998 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5999 | `	if( nLen < 1 ){` |
|      - | 6000 | `		/* Nothing to shuffle */` |
|      3 | 6001 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 6002 | `		return PH7_OK;` |
|      - | 6003 | `	}` |
|      - | 6004 | `	/* Shuffle the string */` |
|     43 | 6005 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 6006 | `		/* Generate a random number first */` |
|     35 | 6007 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 6008 | `		/* Extract a random offset */` |
|     35 | 6009 | `		c = zString[iR % nLen];` |
|      - | 6010 | `		/* Append it */` |
|     35 | 6011 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 6012 | `	}` |
|      9 | 6013 | `	return PH7_OK;` |
|      7 | 6014 | `}` |
|      - | 6015 | `/*` |
|      - | 6016 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 6017 | ` *  Convert a string to an array.` |
|      - | 6018 | ` * Parameters` |
|      - | 6019 | ` * $string` |
|      - | 6020 | ` *  The input string.` |
|      - | 6021 | ` * $split_length` |
|      - | 6022 | ` *  Maximum length of the chunk.` |
|      - | 6023 | ` * Return` |
|      - | 6024 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 6025 | ` *  except possibly the last one which may be shorter.` |
|      - | 6026 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 6027 | ` *  as the first (and only) array element.` |
|      - | 6028 | ` *  An empty string returns an empty array.` |
|      - | 6029 | ` * Errors` |
|      - | 6030 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 6031 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 6032 | ` *  ValueError if $split_length is less than 1.` |
|      - | 6033 | ` */` |
|     28 | 6034 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 6035 | `{` |
|      - | 6036 | `	const char *zString,*zEnd;` |
|      - | 6037 | `	ph7_value *pArray,*pValue;` |
|      - | 6038 | `	int split_len;` |
|      - | 6039 | `	int nLen;` |
|     33 | 6040 | `	if( nArg < 1 ){` |
|      4 | 6041 | `		return PH7_VmThrowException(pCtx,` |
|      - | 6042 | `			"ArgumentCountError",` |
|      - | 6043 | `			"str_split() expects at least 1 argument, %d given",` |
|      1 | 6044 | `			nArg` |
|      - | 6045 | `			);` |
|      - | 6046 | `	}` |
|      - | 6047 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     50 | 6048 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     40 | 6049 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 6050 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 6051 | `		return PH7_VmThrowException(pCtx,` |
|      - | 6052 | `			"TypeError",` |
|      - | 6053 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 6054 | `			ph7_type_name(apArg[0])` |
|      - | 6055 | `			);` |
|      - | 6056 | `	}` |
|      - | 6057 | `	/* Point to the target string */` |
|     27 | 6058 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 6059 | `	split_len = (int)sizeof(char);` |
|     27 | 6060 | `	if( nArg > 1 ){` |
|      - | 6061 | `		/* Split length */` |
|     17 | 6062 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 6063 | `		if( split_len < 1 ){` |
|      6 | 6064 | `			return PH7_VmThrowException(pCtx,` |
|      - | 6065 | `				"ValueError",` |
|      - | 6066 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 6067 | `				);` |
|      - | 6068 | `		}` |
|     11 | 6069 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 6070 | `			split_len = nLen;` |
|      1 | 6071 | `		}` |
|      5 | 6072 | `	}` |
|      - | 6073 | `	/* Create the array and the scalar value */` |
|     21 | 6074 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 6075 | `	/*Chunk value */` |
|     21 | 6076 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 6077 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 6078 | `		/* Return FALSE */` |
|    ! 0 | 6079 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6080 | `		return PH7_OK;` |
|      - | 6081 | `	}` |
|      - | 6082 | `	/* Point to the end of the string */` |
|     21 | 6083 | `	zEnd = &zString[nLen];` |
|      - | 6084 | `	/* Perform the requested operation */` |
|     48 | 6085 | `	for(;;){` |
|      - | 6086 | `		int nMax;` |
|     59 | 6087 | `		if( zString >= zEnd ){` |
|      - | 6088 | `			/* No more input to process */` |
|     21 | 6089 | `			break;` |
|      - | 6090 | `		}` |
|     39 | 6091 | `		nMax = (int)(zEnd-zString);` |
|     39 | 6092 | `		if( nMax < split_len ){` |
|      3 | 6093 | `			split_len = nMax;` |
|      1 | 6094 | `		}` |
|      - | 6095 | `		/* Copy the current chunk */` |
|     39 | 6096 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 6097 | `		/* Insert it */` |
|     39 | 6098 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|    ! 0 | 6099 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6100 | `		}` |
|      - | 6101 | `		/* reset the string cursor */` |
|     39 | 6102 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 6103 | `		/* Update position */` |
|     39 | 6104 | `		zString += split_len;` |
|      1 | 6105 | `	}` |
|      - | 6106 | `	/*` |
|      - | 6107 | `	 * Return the array.` |
|      - | 6108 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 6109 | `	 * upon we return from this function.` |
|      - | 6110 | `	 */` |
|     21 | 6111 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 6112 | `	return PH7_OK;` |
|     19 | 6113 | `}` |
|      - | 6114 | `/*` |
|      - | 6115 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 6116 | ` * Refer to [strspn()].` |
|      - | 6117 | ` */` |
|     28 | 6118 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 6119 | `{` |
|     29 | 6120 | `	const char *zIn = *pzIn;` |
|      - | 6121 | `	const char *zPtr;` |
|      - | 6122 | `	/* Ignore leading white spaces */` |
|     29 | 6123 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 6124 | `		zIn++;` |
|    ! 0 | 6125 | `	}` |
|     29 | 6126 | `	if( zIn >= zEnd ){` |
|      - | 6127 | `		/* End of input */` |
|    ! 0 | 6128 | `		return SXERR_EOF;` |
|      - | 6129 | `	}` |
|     29 | 6130 | `	zPtr = zIn;` |
|      - | 6131 | `	/* Extract the token */` |
|    201 | 6132 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 6133 | `		zIn++;` |
|      1 | 6134 | `	}` |
|     29 | 6135 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 6136 | `	/* Synchronize pointers */` |
|     29 | 6137 | `	*pzIn = zIn;` |
|      - | 6138 | `	/* Return to the caller */` |
|     29 | 6139 | `	return SXRET_OK;` |
|     15 | 6140 | `}` |
|      - | 6141 | `/*` |
|      - | 6142 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 6143 | ` * return the longest match.` |
|      - | 6144 | ` * Refer to [strspn()].` |
|      - | 6145 | ` */` |
|     18 | 6146 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 6147 | `{` |
|     19 | 6148 | `	const char *zEnd = &zString[nLen];` |
|     19 | 6149 | `	const char *zIn = zString;` |
|      - | 6150 | `	int i,c;` |
|     45 | 6151 | `	for(;;){` |
|     91 | 6152 | `		if( zString >= zEnd ){` |
|      7 | 6153 | `			break;` |
|      - | 6154 | `		}` |
|      - | 6155 | `		/* Extract current character */` |
|     85 | 6156 | `		c = zString[0];` |
|      - | 6157 | `		/* Perform the lookup */` |
|    383 | 6158 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 6159 | `			if( c == zMask[i] ){` |
|      - | 6160 | `				/* Character found */` |
|     73 | 6161 | `				break;` |
|      - | 6162 | `			}` |
|    150 | 6163 | `		}` |
|     85 | 6164 | `		if( i >= nMaskLen ){` |
|      - | 6165 | `			/* Character not in the current mask,break immediately */` |
|     13 | 6166 | `			break;` |
|      - | 6167 | `		}` |
|      - | 6168 | `		/* Advance cursor */` |
|     73 | 6169 | `		zString++;` |
|      1 | 6170 | `	}` |
|      - | 6171 | `	/* Longest match */` |
|     19 | 6172 | `	return (int)(zString-zIn);` |
|      1 | 6173 | `}` |
|      - | 6174 | `/*` |
|      - | 6175 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 6176 | ` * Refer to [strcspn()].` |
|      - | 6177 | ` */` |
|     10 | 6178 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 6179 | `{` |
|     11 | 6180 | `	const char *zEnd = &zString[nLen];` |
|     11 | 6181 | `	const char *zIn = zString;` |
|      - | 6182 | `	int i,c;` |
|     12 | 6183 | `	for(;;){` |
|     25 | 6184 | `		if( zString >= zEnd ){` |
|      3 | 6185 | `			break;` |
|      - | 6186 | `		}` |
|      - | 6187 | `		/* Extract current character */` |
|     23 | 6188 | `		c = zString[0];` |
|      - | 6189 | `		/* Perform the lookup */` |
|     51 | 6190 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 6191 | `			if( c == zMask[i] ){` |
|      9 | 6192 | `				break;` |
|      - | 6193 | `			}` |
|     15 | 6194 | `		}` |
|     23 | 6195 | `		if( i < nMaskLen ){` |
|      - | 6196 | `			/* Character in the current mask,break immediately */` |
|      9 | 6197 | `			break;` |
|      - | 6198 | `		}` |
|      - | 6199 | `		/* Advance cursor */` |
|     15 | 6200 | `		zString++;` |
|      1 | 6201 | `	}` |
|      - | 6202 | `	/* Longest match */` |
|     11 | 6203 | `	return (int)(zString-zIn);` |
|      1 | 6204 | `}` |
|      - | 6205 | `/*` |
|      - | 6206 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 6207 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 6208 | ` *  of characters contained within a given mask.` |
|      - | 6209 | ` * Parameters` |
|      - | 6210 | ` * $str` |
|      - | 6211 | ` *  The input string.` |
|      - | 6212 | ` * $mask` |
|      - | 6213 | ` *  The list of allowable characters.` |
|      - | 6214 | ` * $start` |
|      - | 6215 | ` *  The position in subject to start searching.` |
|      - | 6216 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 6217 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 6218 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 6219 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 6220 | ` *  start'th position from the end of subject.` |
|      - | 6221 | ` * $length` |
|      - | 6222 | ` *  The length of the segment from subject to examine.` |
|      - | 6223 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 6224 | ` *  characters after the starting position.` |
|      - | 6225 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 6226 | ` *  position up to length characters from the end of subject.` |
|      - | 6227 | ` * Return` |
|      - | 6228 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 6229 | ` * in mask.` |
|      - | 6230 | ` */` |
|     26 | 6231 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6232 | `{` |
|      - | 6233 | `	const char *zString,*zMask,*zEnd;` |
|      - | 6234 | `	int iMasklen,iLen;` |
|      - | 6235 | `	SyString sToken;` |
|     27 | 6236 | `	int iCount = 0;` |
|      - | 6237 | `	int rc;` |
|     27 | 6238 | `	if( nArg < 2 ){` |
|      - | 6239 | `		/* Missing agruments,return zero */` |
|      3 | 6240 | `		ph7_result_int(pCtx,0);` |
|      3 | 6241 | `		return PH7_OK;` |
|      - | 6242 | `	}` |
|      - | 6243 | `	/* Extract the target string */` |
|     25 | 6244 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6245 | `	/* Extract the mask */` |
|     25 | 6246 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 6247 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 6248 | `		/* Nothing to process,return zero */` |
|      7 | 6249 | `		ph7_result_int(pCtx,0);` |
|      7 | 6250 | `		return PH7_OK;` |
|      - | 6251 | `	}` |
|     19 | 6252 | `	if( nArg > 2 ){` |
|      - | 6253 | `		int nOfft;` |
|      - | 6254 | `		/* Extract the offset */` |
|      9 | 6255 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 6256 | `		if( nOfft < 0 ){` |
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
|      9 | 6267 | `			if( nOfft >= iLen ){` |
|      - | 6268 | `				/* Invalid offset */` |
|    ! 0 | 6269 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 6270 | `				return PH7_OK;` |
|    ! 0 | 6271 | `			}else{` |
|      - | 6272 | `				/* Update offset */` |
|      9 | 6273 | `				zString += nOfft;` |
|      9 | 6274 | `				iLen -= nOfft;` |
|      - | 6275 | `			}` |
|      - | 6276 | `		}` |
|      9 | 6277 | `		if( nArg > 3 ){` |
|      - | 6278 | `			int iUserlen;` |
|      - | 6279 | `			/* Extract the desired length */` |
|      9 | 6280 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 6281 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 6282 | `				iLen = iUserlen;` |
|      2 | 6283 | `			}` |
|      4 | 6284 | `		}` |
|      4 | 6285 | `	}` |
|      - | 6286 | `	/* Point to the end of the string */` |
|     19 | 6287 | `	zEnd = &zString[iLen];` |
|      - | 6288 | `	/* Extract the first non-space token */` |
|     19 | 6289 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 6290 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 6291 | `		/* Compare against the current mask */` |
|     19 | 6292 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 6293 | `	}` |
|      - | 6294 | `	/* Longest match */` |
|     19 | 6295 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 6296 | `	return PH7_OK;` |
|     14 | 6297 | `}` |
|      - | 6298 | `/*` |
|      - | 6299 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 6300 | ` *  Find length of initial segment not matching mask.` |
|      - | 6301 | ` * Parameters` |
|      - | 6302 | ` * $str` |
|      - | 6303 | ` *  The input string.` |
|      - | 6304 | ` * $mask` |
|      - | 6305 | ` *  The list of not allowed characters.` |
|      - | 6306 | ` * $start` |
|      - | 6307 | ` *  The position in subject to start searching.` |
|      - | 6308 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 6309 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 6310 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 6311 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 6312 | ` *  start'th position from the end of subject.` |
|      - | 6313 | ` * $length` |
|      - | 6314 | ` *  The length of the segment from subject to examine.` |
|      - | 6315 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 6316 | ` *  characters after the starting position.` |
|      - | 6317 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 6318 | ` *  position up to length characters from the end of subject.` |
|      - | 6319 | ` * Return` |
|      - | 6320 | ` *  Returns the length of the segment as an integer.` |
|      - | 6321 | ` */` |
|     16 | 6322 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6323 | `{` |
|      - | 6324 | `	const char *zString,*zMask,*zEnd;` |
|      - | 6325 | `	int iMasklen,iLen;` |
|      - | 6326 | `	SyString sToken;` |
|     17 | 6327 | `	int iCount = 0;` |
|      - | 6328 | `	int rc;` |
|     17 | 6329 | `	if( nArg < 2 ){` |
|      - | 6330 | `		/* Missing agruments,return zero */` |
|      3 | 6331 | `		ph7_result_int(pCtx,0);` |
|      3 | 6332 | `		return PH7_OK;` |
|      - | 6333 | `	}` |
|      - | 6334 | `	/* Extract the target string */` |
|     15 | 6335 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6336 | `	/* Extract the mask */` |
|     15 | 6337 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 6338 | `	if( iLen < 1 ){` |
|      - | 6339 | `		/* Nothing to process,return zero */` |
|    ! 0 | 6340 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 6341 | `		return PH7_OK;` |
|      - | 6342 | `	}` |
|     15 | 6343 | `	if( iMasklen < 1 ){` |
|      - | 6344 | `		/* No given mask,return the string length */` |
|      3 | 6345 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 6346 | `		return PH7_OK;` |
|      - | 6347 | `	}` |
|     13 | 6348 | `	if( nArg > 2 ){` |
|      - | 6349 | `		int nOfft;` |
|      - | 6350 | `		/* Extract the offset */` |
|     11 | 6351 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 6352 | `		if( nOfft < 0 ){` |
|    ! 0 | 6353 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 6354 | `			if( zBase > zString ){` |
|    ! 0 | 6355 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 6356 | `				zString = zBase;` |
|    ! 0 | 6357 | `			}else{` |
|      - | 6358 | `				/* Invalid offset */` |
|    ! 0 | 6359 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 6360 | `				return PH7_OK;` |
|      - | 6361 | `			}` |
|    ! 0 | 6362 | `		}else{` |
|     11 | 6363 | `			if( nOfft >= iLen ){` |
|      - | 6364 | `				/* Invalid offset */` |
|      3 | 6365 | `				ph7_result_int(pCtx,0);` |
|      3 | 6366 | `				return PH7_OK;` |
|    ! 0 | 6367 | `			}else{` |
|      - | 6368 | `				/* Update offset */` |
|      9 | 6369 | `				zString += nOfft;` |
|      9 | 6370 | `				iLen -= nOfft;` |
|      - | 6371 | `			}` |
|      - | 6372 | `		}` |
|      9 | 6373 | `		if( nArg > 3 ){` |
|      - | 6374 | `			int iUserlen;` |
|      - | 6375 | `			/* Extract the desired length */` |
|    ! 0 | 6376 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 6377 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 6378 | `				iLen = iUserlen;` |
|    ! 0 | 6379 | `			}` |
|    ! 0 | 6380 | `		}` |
|      4 | 6381 | `	}` |
|      - | 6382 | `	/* Point to the end of the string */` |
|     11 | 6383 | `	zEnd = &zString[iLen];` |
|      - | 6384 | `	/* Extract the first non-space token */` |
|     11 | 6385 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 6386 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 6387 | `		/* Compare against the current mask */` |
|     11 | 6388 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 6389 | `	}` |
|      - | 6390 | `	/* Longest match */` |
|     11 | 6391 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 6392 | `	return PH7_OK;` |
|      9 | 6393 | `}` |
|      - | 6394 | `/*` |
|      - | 6395 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 6396 | ` *  Search a string for any of a set of characters.` |
|      - | 6397 | ` * Parameters` |
|      - | 6398 | ` *  $haystack` |
|      - | 6399 | ` *   The string where char_list is looked for.` |
|      - | 6400 | ` *  $char_list` |
|      - | 6401 | ` *   This parameter is case sensitive.` |
|      - | 6402 | ` * Return` |
|      - | 6403 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 6404 | ` */` |
|      6 | 6405 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6406 | `{` |
|      - | 6407 | `	const char *zString,*zList,*zEnd;` |
|      - | 6408 | `	int iLen,iListLen,i,c;` |
|      - | 6409 | `	sxu32 nOfft,nMax;` |
|      - | 6410 | `	sxi32 rc;` |
|      7 | 6411 | `	if( nArg < 2 ){` |
|      - | 6412 | `		/* Missing arguments,return FALSE */` |
|      3 | 6413 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6414 | `		return PH7_OK;` |
|      - | 6415 | `	}` |
|      - | 6416 | `	/* Extract the haystack and the char list */` |
|      5 | 6417 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 6418 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 6419 | `	if( iLen < 1 ){` |
|      - | 6420 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 6421 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6422 | `		return PH7_OK;` |
|      - | 6423 | `	}` |
|      - | 6424 | `	/* Point to the end of the string */` |
|      5 | 6425 | `	zEnd = &zString[iLen];` |
|      5 | 6426 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 6427 | `	/* perform the requested operation */` |
|     15 | 6428 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 6429 | `		c = zList[i];` |
|     11 | 6430 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 6431 | `		if( rc == SXRET_OK ){` |
|      5 | 6432 | `			if( nMax < nOfft ){` |
|      3 | 6433 | `				nOfft = nMax;` |
|      1 | 6434 | `			}` |
|      2 | 6435 | `		}` |
|      6 | 6436 | `	}` |
|      5 | 6437 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 6438 | `		/* No such substring,return FALSE */` |
|      3 | 6439 | `		ph7_result_bool(pCtx,0);` |
|      2 | 6440 | `	}else{` |
|      - | 6441 | `		/* Return the substring */` |
|      3 | 6442 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 6443 | `	}` |
|      5 | 6444 | `	return PH7_OK;` |
|      4 | 6445 | `}` |
|      - | 6446 | `/* SPDX-SnippetBegin */` |
|      - | 6447 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 6448 | `/* SPDX-License-Identifier: blessing */` |
|      - | 6449 | `/*` |
|      - | 6450 | ` * string soundex(string $str)` |
|      - | 6451 | ` *  Calculate the soundex key of a string.` |
|      - | 6452 | ` * Parameters` |
|      - | 6453 | ` *  $str` |
|      - | 6454 | ` *   The input string.` |
|      - | 6455 | ` * Return` |
|      - | 6456 | ` *  Returns the soundex key as a string.` |
|      - | 6457 | ` * Note:` |
|      - | 6458 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 6459 | ` * source tree.` |
|      - | 6460 | ` */` |
|     20 | 6461 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6462 | `{` |
|      - | 6463 | `	const unsigned char *zIn;` |
|      - | 6464 | `	char zResult[8];` |
|      - | 6465 | `	int i, j;` |
|      - | 6466 | `	static const unsigned char iCode[] = {` |
|      - | 6467 |  |
|      - | 6468 |  |
|      - | 6469 |  |
|      - | 6470 |  |
|      - | 6471 |  |
|      - | 6472 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 6473 |  |
|      - | 6474 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 6475 | `	};` |
|     21 | 6476 | `	if( nArg < 1 ){` |
|      - | 6477 | `		/* Missing arguments,return the empty string */` |
|      3 | 6478 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 6479 | `		return PH7_OK;` |
|      - | 6480 | `	}` |
|     19 | 6481 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     19 | 6482 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     19 | 6483 | `	if( zIn[i] ){` |
|     17 | 6484 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 6485 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 6486 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 6487 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 6488 | `			if( code>0 ){` |
|     45 | 6489 | `				if( code!=prevcode ){` |
|     33 | 6490 | `					prevcode = (unsigned char)code;` |
|     33 | 6491 | `					zResult[j++] = (char)code + '0';` |
|     16 | 6492 | `				}` |
|     23 | 6493 | `			}else{` |
|     49 | 6494 | `				prevcode = 0;` |
|      - | 6495 | `			}` |
|     47 | 6496 | `		}` |
|     33 | 6497 | `		while( j<4 ){` |
|     17 | 6498 | `			zResult[j++] = '0';` |
|      1 | 6499 | `		}` |
|     17 | 6500 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 6501 | `	}else{` |
|      3 | 6502 | `	  ph7_result_string(pCtx,"?000",4);` |
|      - | 6503 | `	}` |
|     19 | 6504 | `	return PH7_OK;` |
|     11 | 6505 | `}` |
|      - | 6506 | `/* SPDX-SnippetEnd */` |
|      - | 6507 | `/*` |
|      - | 6508 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 6509 | ` *  Wraps a string to a given number of characters.` |
|      - | 6510 | ` * Parameters` |
|      - | 6511 | ` *  $str` |
|      - | 6512 | ` *   The input string.` |
|      - | 6513 | ` * $width` |
|      - | 6514 | ` *  The column width.` |
|      - | 6515 | ` * $break` |
|      - | 6516 | ` *  The line is broken using the optional break parameter.` |
|      - | 6517 | ` * Return` |
|      - | 6518 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 6519 | ` */` |
|     14 | 6520 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6521 | `{` |
|      - | 6522 | `	const char *zIn,*zEnd,*zBreak;` |
|      - | 6523 | `	int iLen,iBreaklen,iChunk;` |
|     15 | 6524 | `	if( nArg < 1 ){` |
|      - | 6525 | `		/* Missing arguments,return the empty string */` |
|      3 | 6526 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 6527 | `		return PH7_OK;` |
|      - | 6528 | `	}` |
|      - | 6529 | `	/* Extract the input string */` |
|     13 | 6530 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|     13 | 6531 | `	if( iLen < 1 ){` |
|      - | 6532 | `		/* Nothing to process,return the empty string */` |
|      3 | 6533 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 6534 | `		return PH7_OK;` |
|      - | 6535 | `	}` |
|      - | 6536 | `	/* Chunk length */` |
|     11 | 6537 | `	iChunk = 75;` |
|     11 | 6538 | `	iBreaklen = 0;` |
|     11 | 6539 | `	zBreak = ""; /* cc warning */` |
|     11 | 6540 | `	if( nArg > 1 ){` |
|     11 | 6541 | `		iChunk = ph7_value_to_int(apArg[1]);` |
|     11 | 6542 | `		if( iChunk < 1 ){` |
|    ! 0 | 6543 | `			iChunk = 75;` |
|    ! 0 | 6544 | `		}` |
|     11 | 6545 | `		if( nArg > 2 ){` |
|      3 | 6546 | `			zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      1 | 6547 | `		}` |
|      5 | 6548 | `	}` |
|     11 | 6549 | `	if( iBreaklen < 1 ){` |
|      - | 6550 | `		/* Set a default column break */` |
|      - | 6551 | `#ifdef __WINNT__` |
|      1 | 6552 | `		zBreak = "\r\n";` |
|      1 | 6553 | `		iBreaklen = (int)sizeof("\r\n")-1;` |
|      - | 6554 | `#else` |
|      8 | 6555 | `		zBreak = "\n";` |
|      8 | 6556 | `		iBreaklen = (int)sizeof(char);` |
|      - | 6557 | `#endif` |
|      4 | 6558 | `	}` |
|      - | 6559 | `	/* Perform the requested operation */` |
|     11 | 6560 | `	zEnd = &zIn[iLen];` |
|     41 | 6561 | `	for(;;){` |
|      - | 6562 | `		int nMax;` |
|     47 | 6563 | `		if( zIn >= zEnd ){` |
|      - | 6564 | `			/* No more input to process */` |
|     11 | 6565 | `			break;` |
|      - | 6566 | `		}` |
|     37 | 6567 | `		nMax = (int)(zEnd-zIn);` |
|     37 | 6568 | `		if( iChunk > nMax ){` |
|     11 | 6569 | `			iChunk = nMax;` |
|      5 | 6570 | `		}` |
|      - | 6571 | `		/* Append the column first */` |
|     37 | 6572 | `		ph7_result_string(pCtx,zIn,iChunk); /* Will make it's own copy */` |
|      - | 6573 | `		/* Advance the cursor */` |
|     37 | 6574 | `		zIn += iChunk;` |
|     37 | 6575 | `		if( zIn < zEnd ){` |
|      - | 6576 | `			/* Append the line break */` |
|     27 | 6577 | `			ph7_result_string(pCtx,zBreak,iBreaklen);` |
|     13 | 6578 | `		}` |
|      1 | 6579 | `	}` |
|     11 | 6580 | `	return PH7_OK;` |
|      8 | 6581 | `}` |
|      - | 6582 | `/*` |
|      - | 6583 | ` * Check if the given character is a member of the given mask.` |
|      - | 6584 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 6585 | ` * Refer to [strtok()].` |
|      - | 6586 | ` */` |
|     30 | 6587 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 6588 | `{` |
|      - | 6589 | `	int i;` |
|     57 | 6590 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 6591 | `		if( c == zMask[i] ){` |
|     13 | 6592 | `			if( pOfft ){` |
|      5 | 6593 | `				*pOfft = i;` |
|      2 | 6594 | `			}` |
|     13 | 6595 | `			return TRUE;` |
|      - | 6596 | `		}` |
|     14 | 6597 | `	}` |
|     19 | 6598 | `	return FALSE;` |
|     16 | 6599 | `}` |
|      - | 6600 | `/*` |
|      - | 6601 | ` * Extract a single token from the input stream.` |
|      - | 6602 | ` * Refer to [strtok()].` |
|      - | 6603 | ` */` |
|      6 | 6604 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 6605 | `{` |
|      7 | 6606 | `	const char *zIn = *pzIn;` |
|      - | 6607 | `	const char *zPtr;` |
|      - | 6608 | `	/* Ignore leading delimiter */` |
|     11 | 6609 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 6610 | `		zIn++;` |
|      1 | 6611 | `	}` |
|      7 | 6612 | `	if( zIn >= zEnd ){` |
|      - | 6613 | `		/* End of input */` |
|    ! 0 | 6614 | `		return SXERR_EOF;` |
|      - | 6615 | `	}` |
|      7 | 6616 | `	zPtr = zIn;` |
|      - | 6617 | `	/* Extract the token */` |
|     13 | 6618 | `	while( zIn < zEnd ){` |
|     11 | 6619 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 6620 | `			/* UTF-8 stream */` |
|    ! 0 | 6621 | `			zIn++;` |
|    ! 0 | 6622 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 6623 | `		}else{` |
|     11 | 6624 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 6625 | `				break;` |
|      - | 6626 | `			}` |
|      7 | 6627 | `			zIn++;` |
|      - | 6628 | `		}` |
|      1 | 6629 | `	}` |
|      7 | 6630 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 6631 | `	/* Update the cursor */` |
|      7 | 6632 | `	*pzIn = zIn;` |
|      - | 6633 | `	/* Return to the caller */` |
|      7 | 6634 | `	return SXRET_OK;` |
|      4 | 6635 | `}` |
|      - | 6636 | `/* strtok auxiliary private data */` |
|      - | 6637 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 6638 | `struct strtok_aux_data` |
|      - | 6639 | `{` |
|      - | 6640 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 6641 | `	const char *zIn;   /* Current input stream */` |
|      - | 6642 | `	const char *zEnd;  /* End of input */` |
|      - | 6643 | `};` |
|      - | 6644 | `/*` |
|      - | 6645 | ` * string strtok(string $str,string $token)` |
|      - | 6646 | ` * string strtok(string $token)` |
|      - | 6647 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 6648 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 6649 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 6650 | ` *  words by using the space character as the token.` |
|      - | 6651 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 6652 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 6653 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 6654 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 6655 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 6656 | ` *  the argument are found.` |
|      - | 6657 | ` * Parameters` |
|      - | 6658 | ` *  $str` |
|      - | 6659 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 6660 | ` * $token` |
|      - | 6661 | ` *  The delimiter used when splitting up str.` |
|      - | 6662 | ` * Return` |
|      - | 6663 | ` *   Current token or FALSE on EOF.` |
|      - | 6664 | ` */` |
|      8 | 6665 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6666 | `{` |
|      - | 6667 | `	strtok_aux_data *pAux;` |
|      - | 6668 | `	const char *zMask;` |
|      - | 6669 | `	SyString sToken;` |
|      - | 6670 | `	int nMasklen;` |
|      - | 6671 | `	sxi32 rc;` |
|      9 | 6672 | `	if( nArg < 2 ){` |
|      - | 6673 | `		/* Extract top aux data */` |
|      7 | 6674 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      7 | 6675 | `		if( pAux == 0 ){` |
|      - | 6676 | `			/* No aux data,return FALSE */` |
|    ! 0 | 6677 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6678 | `			return PH7_OK;` |
|      - | 6679 | `		}` |
|      7 | 6680 | `		nMasklen = 0;` |
|      7 | 6681 | `		zMask = ""; /* cc warning */` |
|      7 | 6682 | `		if( nArg > 0 ){` |
|      - | 6683 | `			/* Extract the mask */` |
|      5 | 6684 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 6685 | `		}` |
|      7 | 6686 | `		if( nMasklen < 1 ){` |
|      - | 6687 | `			/* Invalid mask,return FALSE */` |
|      3 | 6688 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|      3 | 6689 | `			ph7_context_free_chunk(pCtx,pAux);` |
|      3 | 6690 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|      3 | 6691 | `			ph7_result_bool(pCtx,0);` |
|      3 | 6692 | `			return PH7_OK;` |
|      - | 6693 | `		}` |
|      - | 6694 | `		/* Extract the token */` |
|      5 | 6695 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 6696 | `		if( rc != SXRET_OK ){` |
|      - | 6697 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 6698 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 6699 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6700 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 6701 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6702 | `		}else{` |
|      - | 6703 | `			/* Return the extracted token */` |
|      5 | 6704 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6705 | `		}` |
|      3 | 6706 | `	}else{` |
|      - | 6707 | `		const char *zInput,*zCur;` |
|      - | 6708 | `		char *zDup;` |
|      - | 6709 | `		int nLen;` |
|      - | 6710 | `		/* Extract the raw input */` |
|      3 | 6711 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 6712 | `		if( nLen < 1 ){` |
|      - | 6713 | `			/* Empty input,return FALSE */` |
|    ! 0 | 6714 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6715 | `			return PH7_OK;` |
|      - | 6716 | `		}` |
|      - | 6717 | `		/* Extract the mask */` |
|      3 | 6718 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 6719 | `		if( nMasklen < 1 ){` |
|      - | 6720 | `			/* Set a default mask */` |
|      - | 6721 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 6722 | `			zMask = TOK_MASK;` |
|    ! 0 | 6723 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 6724 | `#undef TOK_MASK` |
|    ! 0 | 6725 | `		}` |
|      - | 6726 | `		/* Extract a single token */` |
|      3 | 6727 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 6728 | `		if( rc != SXRET_OK ){` |
|      - | 6729 | `			/* Empty input */` |
|    ! 0 | 6730 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6731 | `			return PH7_OK;` |
|    ! 0 | 6732 | `		}else{` |
|      - | 6733 | `			/* Return the extracted token */` |
|      3 | 6734 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6735 | `		}` |
|      - | 6736 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 6737 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 6738 | `		if( pAux ){` |
|      3 | 6739 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 6740 | `			if( nLen < 1 ){` |
|    ! 0 | 6741 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6742 | `				return PH7_OK;` |
|      - | 6743 | `			}` |
|      - | 6744 | `			/* Duplicate input */` |
|      3 | 6745 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 6746 | `			if( zDup  ){` |
|      3 | 6747 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 6748 | `				/* Register the aux data */` |
|      3 | 6749 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 6750 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 6751 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 6752 | `			}` |
|      1 | 6753 | `		}` |
|      - | 6754 | `	}` |
|      7 | 6755 | `	return PH7_OK;` |
|      5 | 6756 | `}` |
|      - | 6757 | `/*` |
|      - | 6758 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 6759 | ` *  Pad a string to a certain length with another string` |
|      - | 6760 | ` * Parameters` |
|      - | 6761 | ` *  $input` |
|      - | 6762 | ` *   The input string.` |
|      - | 6763 | ` * $pad_length` |
|      - | 6764 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 6765 | ` *   string, no padding takes place.` |
|      - | 6766 | ` * $pad_string` |
|      - | 6767 | ` *   Note:` |
|      - | 6768 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 6769 | ` *    divided by the pad_string's length.` |
|      - | 6770 | ` * $pad_type` |
|      - | 6771 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 6772 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 6773 | ` * Return` |
|      - | 6774 | ` *  The padded string.` |
|      - | 6775 | ` */` |
|     10 | 6776 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6777 | `{` |
|      - | 6778 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 6779 | `	const char *zIn,*zPad;` |
|     11 | 6780 | `	if( nArg < 2 ){` |
|      - | 6781 | `		/* Missing arguments,return the empty string */` |
|      5 | 6782 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 6783 | `		return PH7_OK;` |
|      - | 6784 | `	}` |
|      - | 6785 | `	/* Extract the target string */` |
|      7 | 6786 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6787 | `	/* Padding length */` |
|      7 | 6788 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|      7 | 6789 | `	if( iPadlen > 0 ){` |
|      5 | 6790 | `		iPadlen -= iLen;` |
|      2 | 6791 | `	}` |
|      7 | 6792 | `	if( iPadlen < 1  ){` |
|      - | 6793 | `		/* Return the string verbatim */` |
|      3 | 6794 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      3 | 6795 | `		return PH7_OK;` |
|      - | 6796 | `	}` |
|      5 | 6797 | `	zPad = " "; /* Whitespace padding */` |
|      5 | 6798 | `	iStrpad = (int)sizeof(char);` |
|      5 | 6799 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      5 | 6800 | `	if( nArg > 2 ){` |
|      - | 6801 | `		/* Padding string */` |
|      5 | 6802 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      5 | 6803 | `		if( iStrpad < 1 ){` |
|      - | 6804 | `			/* Empty string */` |
|    ! 0 | 6805 | `			zPad = " "; /* Whitespace padding */` |
|    ! 0 | 6806 | `			iStrpad = (int)sizeof(char);` |
|    ! 0 | 6807 | `		}` |
|      5 | 6808 | `		if( nArg > 3 ){` |
|      - | 6809 | `			/* Padd type */` |
|      5 | 6810 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 6811 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 6812 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 6813 | `			}` |
|      2 | 6814 | `		}` |
|      2 | 6815 | `	}` |
|      5 | 6816 | `	iDiv = 1;` |
|      5 | 6817 | `	if( iType == 2 ){` |
|    ! 0 | 6818 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 6819 | `	}` |
|      - | 6820 | `	/* Perform the requested operation */` |
|      5 | 6821 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 6822 | `		jPad = iStrpad;` |
|      5 | 6823 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 6824 | `			/* Padding */` |
|      5 | 6825 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 6826 | `				break;` |
|      - | 6827 | `			}` |
|      3 | 6828 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 6829 | `		}` |
|      3 | 6830 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 6831 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 6832 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 6833 | `				if( jPad > iStrpad ){` |
|    ! 0 | 6834 | `					jPad = iStrpad;` |
|    ! 0 | 6835 | `				}` |
|      3 | 6836 | `				if( jPad < 1){` |
|    ! 0 | 6837 | `					break;` |
|      - | 6838 | `				}` |
|      3 | 6839 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 6840 | `			}` |
|      1 | 6841 | `		}` |
|      1 | 6842 | `	}` |
|      5 | 6843 | `	if( iLen > 0 ){` |
|      - | 6844 | `		/* Append the input string */` |
|      5 | 6845 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 6846 | `	}` |
|      5 | 6847 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 6848 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 6849 | `			/* Padding */` |
|      5 | 6850 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 6851 | `				break;` |
|      - | 6852 | `			}` |
|      3 | 6853 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 6854 | `		}` |
|      5 | 6855 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 6856 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 6857 | `			if( jPad > iStrpad ){` |
|    ! 0 | 6858 | `				jPad = iStrpad;` |
|    ! 0 | 6859 | `			}` |
|      3 | 6860 | `			if( jPad < 1){` |
|    ! 0 | 6861 | `				break;` |
|      - | 6862 | `			}` |
|      3 | 6863 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 6864 | `		}` |
|      1 | 6865 | `	}` |
|      5 | 6866 | `	return PH7_OK;` |
|      6 | 6867 | `}` |
|      - | 6868 | `/*` |
|      - | 6869 | ` * String replacement private data.` |
|      - | 6870 | ` */` |
|      - | 6871 | `typedef struct str_replace_data str_replace_data;` |
|      - | 6872 | `struct str_replace_data` |
|      - | 6873 | `{` |
|      - | 6874 | `	/* The following two fields are only used by the strtr function */` |
|      - | 6875 | `	SyBlob *pWorker;         /* Working buffer */` |
|      - | 6876 | `	ProcStringMatch xMatch;  /* Pattern match routine */` |
|      - | 6877 | `	/* The following two fields are only used by the str_replace function */` |
|      - | 6878 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 6879 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 6880 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 6881 | `};` |
|      - | 6882 | `/*` |
|      - | 6883 | ` * Remove a substring.` |
|      - | 6884 | ` */` |
|      - | 6885 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 6886 | `	for(;;){\` |
|      - | 6887 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 6888 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 6889 | `		++OFFT;\` |
|      - | 6890 | `	}\` |
|      - | 6891 | `}` |
|      - | 6892 | `/*` |
|      - | 6893 | ` * Shift right and insert algorithm.` |
|      - | 6894 | ` */` |
|      - | 6895 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 6896 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 6897 | `		for(;;){\` |
|      - | 6898 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 6899 | `			if(INLEN < 1 ) { break; }\` |
|      - | 6900 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 6901 | `			--INLEN; \` |
|      - | 6902 | `		}\` |
|      - | 6903 | `		for(;;){\` |
|      - | 6904 | `				if(ELEN < 1) { break; }\` |
|      - | 6905 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 6906 | `				OFFT++;\` |
|      - | 6907 | `				ENTRY++;\` |
|      - | 6908 | `				--ELEN;\` |
|      - | 6909 | `		}\` |
|      - | 6910 | `}` |
|      - | 6911 | `/*` |
|      - | 6912 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 6913 | ` * replacement string [i.e: zReplace].` |
|      - | 6914 | ` */` |
|     38 | 6915 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 6916 | `{` |
|     39 | 6917 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 6918 | `	sxu32 n,m;` |
|     39 | 6919 | `	n = SyBlobLength(pWorker);` |
|     39 | 6920 | `	m = nOfft;` |
|      - | 6921 | `	/* Delete the old entry */` |
|    457 | 6922 | `	STRDEL(zInput,n,m,nLen);` |
|     39 | 6923 | `	SyBlobLength(pWorker) -= nLen;` |
|     39 | 6924 | `	if( nReplen > 0 ){` |
|     33 | 6925 | `		sxi32 iRep = nReplen;` |
|      - | 6926 | `		sxi32 rc;` |
|      - | 6927 | `		/*` |
|      - | 6928 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 6929 | `		 * string.` |
|      - | 6930 | `		 */` |
|     33 | 6931 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     33 | 6932 | `		if( rc != SXRET_OK ){` |
|      - | 6933 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 6934 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 6935 | `			return rc;` |
|      - | 6936 | `		}` |
|      - | 6937 | `		/* Perform the insertion now */` |
|     33 | 6938 | `		zInput = (char *)SyBlobData(pWorker);` |
|     33 | 6939 | `		n = SyBlobLength(pWorker);` |
|    163 | 6940 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     33 | 6941 | `		SyBlobLength(pWorker) += nReplen;` |
|     16 | 6942 | `	}` |
|     39 | 6943 | `	return SXRET_OK;` |
|     20 | 6944 | `}` |
|      - | 6945 | `/*` |
|      - | 6946 | ` * String replacement walker callback.` |
|      - | 6947 | ` * The following callback is invoked for each array entry that hold` |
|      - | 6948 | ` * the replace string.` |
|      - | 6949 | ` * Refer to the strtr() implementation for more information.` |
|      - | 6950 | ` */` |
|      8 | 6951 | `static int StringReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6952 | `{` |
|      9 | 6953 | `	str_replace_data *pRepData = (str_replace_data *)pUserData;` |
|      - | 6954 | `	const char *zTarget,*zReplace;` |
|      - | 6955 | `	SyBlob *pWorker;` |
|      - | 6956 | `	int tLen,nLen;` |
|      - | 6957 | `	sxu32 nOfft;` |
|      - | 6958 | `	sxi32 rc;` |
|      - | 6959 | `	/* Point to the working buffer */` |
|      9 | 6960 | `	pWorker = pRepData->pWorker;` |
|      9 | 6961 | `	if( !ph7_value_is_string(pKey) ){` |
|      - | 6962 | `		/* Target and replace must be a string */` |
|      3 | 6963 | `		return PH7_OK;` |
|      - | 6964 | `	}` |
|      - | 6965 | `	/* Extract the target and the replace */` |
|      7 | 6966 | `	zTarget = ph7_value_to_string(pKey,&tLen);` |
|      7 | 6967 | `	if( tLen < 1 ){` |
|      - | 6968 | `		/* Empty target,return immediately */` |
|    ! 0 | 6969 | `		return PH7_OK;` |
|      - | 6970 | `	}` |
|      - | 6971 | `	/* Perform a pattern search */` |
|      7 | 6972 | `	rc = pRepData->xMatch(SyBlobData(pWorker),SyBlobLength(pWorker),(const void *)zTarget,(sxu32)tLen,&nOfft);` |
|      7 | 6973 | `	if( rc != SXRET_OK ){` |
|      - | 6974 | `		/* Pattern not found */` |
|    ! 0 | 6975 | `		return PH7_OK;` |
|      - | 6976 | `	}` |
|      - | 6977 | `	/* Extract the replace string */` |
|      7 | 6978 | `	zReplace = ph7_value_to_string(pData,&nLen);` |
|      - | 6979 | `	/* Perform the replace process */` |
|      7 | 6980 | `	rc = StringReplace(pWorker,nOfft,tLen,zReplace,nLen);` |
|      7 | 6981 | `	if( rc != SXRET_OK ){` |
|      - | 6982 | `		/* Allocation failure: carry it out and stop the walk */` |
|    ! 0 | 6983 | `		pRepData->rc = rc;` |
|    ! 0 | 6984 | `		return rc;` |
|      - | 6985 | `	}` |
|      - | 6986 | `	/* All done */` |
|      7 | 6987 | `	return PH7_OK;` |
|      5 | 6988 | `}` |
|      - | 6989 | `/*` |
|      - | 6990 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 6991 | ` * to collect search/replace string.` |
|      - | 6992 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 6993 | ` */` |
|     26 | 6994 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6995 | `{` |
|     27 | 6996 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 6997 | `	SyString sWorker;` |
|      - | 6998 | `	const char *zIn;` |
|      - | 6999 | `	int nByte;` |
|      - | 7000 | `	/* Extract a string representation of the given argument */` |
|     27 | 7001 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 7002 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 7003 | `	if( nByte > 0 ){` |
|      - | 7004 | `		char *zDup;` |
|      - | 7005 | `		/* Duplicate the chunk */` |
|     25 | 7006 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 7007 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 7008 | `			);` |
|     25 | 7009 | `		if( zDup == 0 ){` |
|      - | 7010 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 7011 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 7012 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 7013 | `			return SXERR_MEM;` |
|      - | 7014 | `		}` |
|     25 | 7015 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 7016 | `		/* Save the chunk */` |
|     25 | 7017 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 7018 | `	}` |
|      - | 7019 | `	/* Save for later processing */` |
|     27 | 7020 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 7021 | `	/* All done */` |
|     13 | 7022 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 7023 | `	return PH7_OK;` |
|     14 | 7024 | `}` |
|      - | 7025 | `/*` |
|      - | 7026 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 7027 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 7028 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 7029 | ` * Parameters` |
|      - | 7030 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 7031 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 7032 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 7033 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 7034 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 7035 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 7036 | ` * $search` |
|      - | 7037 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 7038 | ` *  to designate multiple needles.` |
|      - | 7039 | ` * $replace` |
|      - | 7040 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 7041 | ` *  to designate multiple replacements.` |
|      - | 7042 | ` * $subject` |
|      - | 7043 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 7044 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 7045 | ` *  of subject, and the return value is an array as well.` |
|      - | 7046 | ` * $count (Not used)` |
|      - | 7047 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 7048 | ` * Return` |
|      - | 7049 | ` * This function returns a string or an array with the replaced values.` |
|      - | 7050 | ` */` |
|  28394 | 7051 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7052 | `{` |
|      - | 7053 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 7054 | `	ProcStringMatch xMatch;` |
|      - | 7055 | `	const char *zIn,*zFunc;` |
|      - | 7056 | `	str_replace_data sRep;` |
|      - | 7057 | `	SyBlob sWorker;` |
|      - | 7058 | `	SySet sReplace;` |
|      - | 7059 | `	SySet sSearch;` |
|      - | 7060 | `	int rep_str;` |
|      - | 7061 | `	int nByte;` |
|      - | 7062 | `	sxi32 rc;` |
|  28399 | 7063 | `	if( nArg < 3 ){` |
|      - | 7064 | `		/* Missing/Invalid arguments,return null */` |
|      7 | 7065 | `		ph7_result_null(pCtx);` |
|      7 | 7066 | `		return PH7_OK;` |
|      - | 7067 | `	}` |
|      - | 7068 | `	/* Initialize fields */` |
|  28393 | 7069 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  28393 | 7070 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  28393 | 7071 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  28393 | 7072 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  28393 | 7073 | `	sRep.pCtx = pCtx;` |
|  28393 | 7074 | `	sRep.pCollector = &sSearch;` |
|  28393 | 7075 | `	rep_str = 0;` |
|      - | 7076 | `	/* Extract the subject */` |
|  28393 | 7077 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  28393 | 7078 | `	if( nByte < 1 ){` |
|      - | 7079 | `		/* Nothing to replace,return the empty string */` |
|     29 | 7080 | `		ph7_result_string(pCtx,"",0);` |
|     29 | 7081 | `		return PH7_OK;` |
|      - | 7082 | `	}` |
|      - | 7083 | `	/* Copy the subject */` |
|  28365 | 7084 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 7085 | `	/* Search string */` |
|  28365 | 7086 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 7087 | `		/* Collect search string */` |
|      9 | 7088 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 7089 | `	}else{` |
|      - | 7090 | `		/* Single pattern */` |
|  28357 | 7091 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  28357 | 7092 | `		if( nByte < 1 ){` |
|      - | 7093 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 7094 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 7095 | `			return PH7_OK;` |
|      - | 7096 | `		}` |
|  28353 | 7097 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 7098 | `		/* Save for later processing */` |
|  28353 | 7099 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 7100 | `	}` |
|      - | 7101 | `	/* Replace string */` |
|  28361 | 7102 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 7103 | `		/* Collect replace string */` |
|      7 | 7104 | `		sRep.pCollector = &sReplace;` |
|      7 | 7105 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 7106 | `	}else{` |
|      - | 7107 | `		/* Single needle */` |
|  28355 | 7108 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  28355 | 7109 | `		rep_str = 1;` |
|  28355 | 7110 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 7111 | `		/* Save for later processing */` |
|  28355 | 7112 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 7113 | `	}` |
|      - | 7114 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  28361 | 7115 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 7116 | `		SySetRelease(&sSearch);` |
|    ! 0 | 7117 | `		SySetRelease(&sReplace);` |
|    ! 0 | 7118 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 7119 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7120 | `	}` |
|      - | 7121 | `	/* Reset loop cursors */` |
|  28361 | 7122 | `	SySetResetCursor(&sSearch);` |
|  28361 | 7123 | `	SySetResetCursor(&sReplace);` |
|  28361 | 7124 | `	pReplace = pSearch = 0; /* cc warning */` |
|  28361 | 7125 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 7126 | `	/* Extract function name */` |
|  28361 | 7127 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 7128 | `	/* Set the default pattern match routine */` |
|  28361 | 7129 | `	xMatch = SyBlobSearch;` |
|  28361 | 7130 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 7131 | `		/* Case insensitive pattern match */` |
|     11 | 7132 | `		xMatch = iPatternMatch;` |
|      5 | 7133 | `	}` |
|      - | 7134 | `	/* Start the replace process */` |
|  56725 | 7135 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 7136 | `		sxu32 nCount,nOfft;` |
|  28369 | 7137 | `		if( pSearch->nByte <  1 ){` |
|      - | 7138 | `			/* Empty string,ignore */` |
|      3 | 7139 | `			continue;` |
|      - | 7140 | `		}` |
|      - | 7141 | `		/* Extract the replace string */` |
|  28367 | 7142 | `		if( rep_str ){` |
|  28357 | 7143 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  14181 | 7144 | `		}else{` |
|     11 | 7145 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 7146 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 7147 | `				 * An empty string is used for the rest of replacement values` |
|      - | 7148 | `				 */` |
|      3 | 7149 | `				pReplace = 0;` |
|      1 | 7150 | `			}` |
|      - | 7151 | `		}` |
|  28367 | 7152 | `		if( pReplace == 0 ){` |
|      - | 7153 | `			/* Use an empty string instead */` |
|      3 | 7154 | `			pReplace = &sTemp;` |
|      1 | 7155 | `		}` |
|  28367 | 7156 | `		nOfft = nCount = 0;` |
|  14197 | 7157 | `		for(;;){` |
|  28399 | 7158 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 7159 | `				break;` |
|      - | 7160 | `			}` |
|      - | 7161 | `			/* Perform a pattern lookup */` |
|  42578 | 7162 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  28382 | 7163 | `				pSearch->nByte,&nOfft);` |
|  28387 | 7164 | `			if( rc != SXRET_OK ){` |
|      - | 7165 | `				/* Pattern not found */` |
|  28355 | 7166 | `				break;` |
|      - | 7167 | `			}` |
|      - | 7168 | `			/* Perform the replace operation */` |
|     33 | 7169 | `			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|     33 | 7170 | `			if( rc != SXRET_OK ){` |
|      - | 7171 | `				/* Allocation failure: surface a fatal instead of a partial result */` |
|    ! 0 | 7172 | `				SySetRelease(&sSearch);` |
|    ! 0 | 7173 | `				SySetRelease(&sReplace);` |
|    ! 0 | 7174 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 7175 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 7176 | `			}` |
|      - | 7177 | `			/* Increment offset counter */` |
|     33 | 7178 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 7179 | `		}` |
|      5 | 7180 | `	}` |
|      - | 7181 | `	/* All done,clean-up the mess left behind */` |
|  28361 | 7182 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  28361 | 7183 | `	SySetRelease(&sSearch);` |
|  28361 | 7184 | `	SySetRelease(&sReplace);` |
|  28361 | 7185 | `	SyBlobRelease(&sWorker);` |
|  28361 | 7186 | `	if( rc != PH7_OK ){` |
|    ! 0 | 7187 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7188 | `	}` |
|  28361 | 7189 | `	return PH7_OK;` |
|  14202 | 7190 | `}` |
|      - | 7191 | `/*` |
|      - | 7192 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 7193 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 7194 | ` *  Translate characters or replace substrings.` |
|      - | 7195 | ` * Parameters` |
|      - | 7196 | ` *  $str` |
|      - | 7197 | ` *  The string being translated.` |
|      - | 7198 | ` * $from` |
|      - | 7199 | ` *  The string being translated to to.` |
|      - | 7200 | ` * $to` |
|      - | 7201 | ` *  The string replacing from.` |
|      - | 7202 | ` * $replace_pairs` |
|      - | 7203 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 7204 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 7205 | ` * Return` |
|      - | 7206 | ` *  The translated string.` |
|      - | 7207 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 7208 | ` */` |
|     12 | 7209 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7210 | `{` |
|      - | 7211 | `	const char *zIn;` |
|      - | 7212 | `	int nLen;` |
|     13 | 7213 | `	if( nArg < 1 ){` |
|      - | 7214 | `		/* Nothing to replace,return FALSE */` |
|      7 | 7215 | `		ph7_result_bool(pCtx,0);` |
|      7 | 7216 | `		return PH7_OK;` |
|      - | 7217 | `	}` |
|      7 | 7218 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 7219 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 7220 | `		/* Invalid arguments */` |
|    ! 0 | 7221 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 7222 | `		return PH7_OK;` |
|      - | 7223 | `	}` |
|      9 | 7224 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 7225 | `		str_replace_data sRepData;` |
|      - | 7226 | `		SyBlob sWorker;` |
|      - | 7227 | `		sxi32 rc;` |
|      - | 7228 | `		/* Initilaize the working buffer */` |
|      5 | 7229 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|      - | 7230 | `		/* Copy raw string */` |
|      5 | 7231 | `		SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nLen);` |
|      - | 7232 | `		/* Init our replace data instance */` |
|      5 | 7233 | `		sRepData.pWorker = &sWorker;` |
|      5 | 7234 | `		sRepData.xMatch = SyBlobSearch;` |
|      5 | 7235 | `		sRepData.rc = SXRET_OK;` |
|      - | 7236 | `		/* Iterate throw array entries and perform the replace operation.*/` |
|      5 | 7237 | `		ph7_array_walk(apArg[1],StringReplaceWalker,&sRepData);` |
|      5 | 7238 | `		if( sRepData.rc != SXRET_OK ){` |
|      - | 7239 | `			/* Allocation failure during replacement: surface a fatal */` |
|    ! 0 | 7240 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 7241 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 7242 | `		}` |
|      - | 7243 | `		/* All done, return the result string */` |
|      7 | 7244 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|      4 | 7245 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 7246 | `		/* Clean-up */` |
|      5 | 7247 | `		SyBlobRelease(&sWorker);` |
|      5 | 7248 | `		if( rc != PH7_OK ){` |
|    ! 0 | 7249 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 7250 | `		}` |
|      3 | 7251 | `	}else{` |
|      - | 7252 | `		int i,flen,tlen,c,iOfft;` |
|      - | 7253 | `		const char *zFrom,*zTo;` |
|      3 | 7254 | `		if( nArg < 3 ){` |
|      - | 7255 | `			/* Nothing to replace */` |
|    ! 0 | 7256 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 7257 | `			return PH7_OK;` |
|      - | 7258 | `		}` |
|      - | 7259 | `		/* Extract given arguments */` |
|      3 | 7260 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 7261 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 7262 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 7263 | `			/* Nothing to replace */` |
|    ! 0 | 7264 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 7265 | `			return PH7_OK;` |
|      - | 7266 | `		}` |
|      - | 7267 | `		/* Start the replace process */` |
|     13 | 7268 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 7269 | `			c = zIn[i];` |
|     11 | 7270 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 7271 | `				if ( iOfft < tlen ){` |
|      5 | 7272 | `					c = zTo[iOfft];` |
|      2 | 7273 | `				}` |
|      2 | 7274 | `			}` |
|     11 | 7275 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 7276 |  |
|      6 | 7277 | `		}` |
|      - | 7278 | `	}` |
|      7 | 7279 | `	return PH7_OK;` |
|      7 | 7280 | `}` |
|      - | 7281 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7282 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7283 | `/*` |
|      - | 7284 | ` * Parse an INI string.` |
|      - | 7285 |  |
|      - | 7286 | ` * According to wikipedia` |
|      - | 7287 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 7288 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 7289 | ` *  Format` |
|      - | 7290 | `*    Properties` |
|      - | 7291 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 7292 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 7293 | `*     Example:` |
|      - | 7294 | `*      name=value` |
|      - | 7295 | `*    Sections` |
|      - | 7296 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 7297 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 7298 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 7299 | `*     or the end of the file. Sections may not be nested.` |
|      - | 7300 | `*     Example:` |
|      - | 7301 | `*      [section]` |
|      - | 7302 | `*   Comments` |
|      - | 7303 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 7304 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 7305 | `*/` |
|     12 | 7306 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 7307 | `{` |
|      - | 7308 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 7309 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 7310 | `	SyHashEntry *pEntry;` |
|      - | 7311 | `	SyString sEntry;` |
|      - | 7312 | `	SyHash sHash;` |
|      - | 7313 | `	int c;` |
|      - | 7314 | `	/* Create an empty array and worker variables */` |
|     13 | 7315 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 7316 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 7317 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 7318 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 7319 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|    ! 0 | 7320 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7321 | `	}` |
|     13 | 7322 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 7323 | `	pCur = pArray;` |
|      - | 7324 | `	/* Start the parse process */` |
|     21 | 7325 | `	for(;;){` |
|      - | 7326 | `		/* Ignore leading white spaces */` |
|     69 | 7327 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 7328 | `			zIn++;` |
|      1 | 7329 | `		}` |
|     43 | 7330 | `		if( zIn >= zEnd ){` |
|      - | 7331 | `			/* No more input to process */` |
|     13 | 7332 | `			break;` |
|      - | 7333 | `		}` |
|     31 | 7334 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 7335 | `			/* Comment til the end of line */` |
|    ! 0 | 7336 | `			zIn++;` |
|    ! 0 | 7337 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 7338 | `				zIn++;` |
|    ! 0 | 7339 | `			}` |
|    ! 0 | 7340 | `			continue;` |
|      - | 7341 | `		}` |
|      - | 7342 | `		/* Reset the string cursor of the working variable */` |
|     31 | 7343 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 7344 | `		if( zIn[0] == '[' ){` |
|      - | 7345 | `			/* Section: Extract the section name */` |
|      9 | 7346 | `			zIn++;` |
|      9 | 7347 | `			zCur = zIn;` |
|     73 | 7348 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 7349 | `				zIn++;` |
|      1 | 7350 | `			}` |
|      9 | 7351 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 7352 | `				/* Save the section name */` |
|      5 | 7353 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 7354 | `				SyStringFullTrim(&sEntry);` |
|      5 | 7355 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 7356 | `				if( sEntry.nByte > 0 ){` |
|      - | 7357 | `					/* Associate an array with the section */` |
|      5 | 7358 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 7359 | `					if( pSection ){` |
|      5 | 7360 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 7361 | `						pCur = pSection;` |
|      2 | 7362 | `					}` |
|      2 | 7363 | `				}` |
|      2 | 7364 | `			}` |
|      9 | 7365 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 7366 | `		}else{` |
|      - | 7367 | `			ph7_value *pOldCur;` |
|      - | 7368 | `			int is_array;` |
|      - | 7369 | `			int iLen;` |
|      - | 7370 | `			/* Properties */` |
|     23 | 7371 | `			is_array = 0;` |
|     23 | 7372 | `			zCur = zIn;` |
|     23 | 7373 | `			iLen = 0; /* cc warning */` |
|     23 | 7374 | `			pOldCur = pCur;` |
|    155 | 7375 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 7376 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 7377 | `					/* Array */` |
|    ! 0 | 7378 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 7379 | `					is_array = 1;` |
|    ! 0 | 7380 | `					if( iLen > 0 ){` |
|    ! 0 | 7381 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 7382 | `						/* Query the hashtable */` |
|    ! 0 | 7383 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 7384 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 7385 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 7386 | `						if( pEntry ){` |
|    ! 0 | 7387 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 7388 | `						}else{` |
|      - | 7389 | `							/* Create an empty array */` |
|    ! 0 | 7390 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 7391 | `							if( pvArr ){` |
|      - | 7392 | `								/* Save the entry */` |
|    ! 0 | 7393 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 7394 | `								/* Insert the entry */` |
|    ! 0 | 7395 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 7396 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 7397 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 7398 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 7399 | `							}` |
|      - | 7400 | `						}` |
|    ! 0 | 7401 | `						if( pvArr ){` |
|    ! 0 | 7402 | `							pCur = pvArr;` |
|    ! 0 | 7403 | `						}` |
|    ! 0 | 7404 | `					}` |
|    ! 0 | 7405 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 7406 | `						zIn++;` |
|    ! 0 | 7407 | `					}` |
|    ! 0 | 7408 | `				}` |
|    133 | 7409 | `				zIn++;` |
|      1 | 7410 | `			}` |
|     23 | 7411 | `			if( !is_array ){` |
|     23 | 7412 | `				iLen = (int)(zIn-zCur);` |
|     11 | 7413 | `			}` |
|      - | 7414 | `			/* Trim the key */` |
|     23 | 7415 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 7416 | `			SyStringFullTrim(&sEntry);` |
|     23 | 7417 | `			if( sEntry.nByte > 0 ){` |
|     23 | 7418 | `				if( !is_array ){` |
|      - | 7419 | `					/* Save the key name */` |
|     23 | 7420 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 7421 | `				}` |
|      - | 7422 | `				/* extract key value */` |
|     23 | 7423 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 7424 | `				zIn++; /* '=' */` |
|     39 | 7425 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 7426 | `					zIn++;` |
|      1 | 7427 | `				}` |
|     23 | 7428 | `				if( zIn < zEnd ){` |
|     21 | 7429 | `					zCur = zIn;` |
|     21 | 7430 | `					c = zIn[0];` |
|     21 | 7431 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 7432 | `						zIn++;` |
|      - | 7433 | `						/* Delimit the value */` |
|    ! 0 | 7434 | `						while( zIn < zEnd ){` |
|    ! 0 | 7435 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 7436 | `								break;` |
|      - | 7437 | `							}` |
|    ! 0 | 7438 | `							zIn++;` |
|    ! 0 | 7439 | `						}` |
|    ! 0 | 7440 | `						if( zIn < zEnd ){` |
|    ! 0 | 7441 | `							zIn++;` |
|    ! 0 | 7442 | `						}` |
|    ! 0 | 7443 | `					}else{` |
|    125 | 7444 | `						while( zIn < zEnd ){` |
|    123 | 7445 | `							if( zIn[0] == '\n' ){` |
|     19 | 7446 | `								if( zIn[-1] != '\\' ){` |
|     19 | 7447 | `									break;` |
|    ! 0 | 7448 | `								}` |
|    105 | 7449 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 7450 | `								/* Inline comments */` |
|    ! 0 | 7451 | `								break;` |
|      - | 7452 | `							}` |
|    105 | 7453 | `							zIn++;` |
|      1 | 7454 | `						}` |
|      - | 7455 | `					}` |
|      - | 7456 | `					/* Trim the value */` |
|     21 | 7457 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 7458 | `					SyStringFullTrim(&sEntry);` |
|     21 | 7459 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 7460 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 7461 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 7462 | `					}` |
|     21 | 7463 | `					if( sEntry.nByte > 0 ){` |
|     21 | 7464 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 7465 | `					}` |
|      - | 7466 | `					/* Insert the key and it's value */` |
|     21 | 7467 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 7468 | `				}` |
|     12 | 7469 | `			}else{` |
|    ! 0 | 7470 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 7471 | `					zIn++;` |
|    ! 0 | 7472 | `				}` |
|      - | 7473 | `			}` |
|     23 | 7474 | `			pCur = pOldCur;` |
|      - | 7475 | `		}` |
|      1 | 7476 | `	}` |
|     13 | 7477 | `	SyHashRelease(&sHash);` |
|      - | 7478 | `	/* Return the parse of the INI string */` |
|     13 | 7479 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 7480 | `	return SXRET_OK;` |
|      7 | 7481 | `}` |
|      - | 7482 | `/*` |
|      - | 7483 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 7484 | ` *  Parse a configuration string.` |
|      - | 7485 | ` * Parameters` |
|      - | 7486 | ` *  $ini` |
|      - | 7487 | ` *   The contents of the ini file being parsed.` |
|      - | 7488 | ` *  $process_sections` |
|      - | 7489 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 7490 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 7491 | ` *  $scanner_mode (Not used)` |
|      - | 7492 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 7493 | ` *   then option values will not be parsed.` |
|      - | 7494 | ` * Return` |
|      - | 7495 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 7496 | ` */` |
|     10 | 7497 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7498 | `{` |
|      - | 7499 | `	const char *zIni;` |
|      - | 7500 | `	int nByte;` |
|     11 | 7501 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7502 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 7503 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7504 | `		return PH7_OK;` |
|      - | 7505 | `	}` |
|      - | 7506 | `	/* Extract the raw INI buffer */` |
|     11 | 7507 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 7508 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|     11 | 7509 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      6 | 7510 | `}` |
|      - | 7511 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7512 |  |
|      - | 7513 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7514 |  |
|      - | 7515 | `/*` |
|      - | 7516 | ` * Ctype Functions.` |
|      - | 7517 | ` * Status:` |
|      - | 7518 | ` *    Stable.` |
|      - | 7519 | ` */` |
|      - | 7520 | `/*` |
|      - | 7521 | ` * bool ctype_alnum(string $text)` |
|      - | 7522 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 7523 | ` * Parameters` |
|      - | 7524 | ` *  $text` |
|      - | 7525 | ` *   The tested string.` |
|      - | 7526 | ` * Return` |
|      - | 7527 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 7528 | ` */` |
|     16 | 7529 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7530 | `{` |
|      - | 7531 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7532 | `	int nLen;` |
|     17 | 7533 | `	if( nArg < 1 ){` |
|      - | 7534 | `		/* Missing arguments,return FALSE */` |
|      3 | 7535 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7536 | `		return PH7_OK;` |
|      - | 7537 | `	}` |
|      - | 7538 | `	/* Extract the target string */` |
|     15 | 7539 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 7540 | `	zEnd = &zIn[nLen];` |
|     15 | 7541 | `	if( nLen < 1 ){` |
|      - | 7542 | `		/* Empty string,return FALSE */` |
|      3 | 7543 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7544 | `		return PH7_OK;` |
|      - | 7545 | `	}` |
|      - | 7546 | `	/* Perform the requested operation */` |
|     32 | 7547 | `	for(;;){` |
|     65 | 7548 | `		if( zIn >= zEnd ){` |
|      - | 7549 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7550 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7551 | `			return PH7_OK;` |
|      - | 7552 | `		}` |
|     57 | 7553 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 7554 | `			break;` |
|      - | 7555 | `		}` |
|      - | 7556 | `		/* Point to the next character */` |
|     53 | 7557 | `		zIn++;` |
|      1 | 7558 | `	}` |
|      - | 7559 | `	/* The test failed,return FALSE */` |
|      5 | 7560 | `	ph7_result_bool(pCtx,0);` |
|      5 | 7561 | `	return PH7_OK;` |
|      9 | 7562 | `}` |
|      - | 7563 | `/*` |
|      - | 7564 | ` * bool ctype_alpha(string $text)` |
|      - | 7565 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 7566 | ` * Parameters` |
|      - | 7567 | ` *  $text` |
|      - | 7568 | ` *   The tested string.` |
|      - | 7569 | ` * Return` |
|      - | 7570 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 7571 | ` */` |
|     18 | 7572 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7573 | `{` |
|      - | 7574 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7575 | `	int nLen;` |
|     19 | 7576 | `	if( nArg < 1 ){` |
|      - | 7577 | `		/* Missing arguments,return FALSE */` |
|      3 | 7578 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7579 | `		return PH7_OK;` |
|      - | 7580 | `	}` |
|      - | 7581 | `	/* Extract the target string */` |
|     17 | 7582 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7583 | `	zEnd = &zIn[nLen];` |
|     17 | 7584 | `	if( nLen < 1 ){` |
|      - | 7585 | `		/* Empty string,return FALSE */` |
|      3 | 7586 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7587 | `		return PH7_OK;` |
|      - | 7588 | `	}` |
|      - | 7589 | `	/* Perform the requested operation */` |
|     42 | 7590 | `	for(;;){` |
|     85 | 7591 | `		if( zIn >= zEnd ){` |
|      - | 7592 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7593 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7594 | `			return PH7_OK;` |
|      - | 7595 | `		}` |
|     77 | 7596 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 7597 | `			break;` |
|      - | 7598 | `		}` |
|      - | 7599 | `		/* Point to the next character */` |
|     71 | 7600 | `		zIn++;` |
|      1 | 7601 | `	}` |
|      - | 7602 | `	/* The test failed,return FALSE */` |
|      7 | 7603 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7604 | `	return PH7_OK;` |
|     10 | 7605 | `}` |
|      - | 7606 | `/*` |
|      - | 7607 | ` * bool ctype_cntrl(string $text)` |
|      - | 7608 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 7609 | ` * Parameters` |
|      - | 7610 | ` *  $text` |
|      - | 7611 | ` *   The tested string.` |
|      - | 7612 | ` * Return` |
|      - | 7613 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 7614 | ` */` |
|     18 | 7615 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7616 | `{` |
|      - | 7617 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7618 | `	int nLen;` |
|     19 | 7619 | `	if( nArg < 1 ){` |
|      - | 7620 | `		/* Missing arguments,return FALSE */` |
|      3 | 7621 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7622 | `		return PH7_OK;` |
|      - | 7623 | `	}` |
|      - | 7624 | `	/* Extract the target string */` |
|     17 | 7625 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7626 | `	zEnd = &zIn[nLen];` |
|     17 | 7627 | `	if( nLen < 1 ){` |
|      - | 7628 | `		/* Empty string,return FALSE */` |
|      3 | 7629 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7630 | `		return PH7_OK;` |
|      - | 7631 | `	}` |
|      - | 7632 | `	/* Perform the requested operation */` |
|     14 | 7633 | `	for(;;){` |
|     29 | 7634 | `		if( zIn >= zEnd ){` |
|      - | 7635 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7636 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7637 | `			return PH7_OK;` |
|      - | 7638 | `		}` |
|     21 | 7639 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7640 | `			/* UTF-8 stream  */` |
|    ! 0 | 7641 | `			break;` |
|      - | 7642 | `		}` |
|     21 | 7643 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 7644 | `			break;` |
|      - | 7645 | `		}` |
|      - | 7646 | `		/* Point to the next character */` |
|     15 | 7647 | `		zIn++;` |
|      1 | 7648 | `	}` |
|      - | 7649 | `	/* The test failed,return FALSE */` |
|      7 | 7650 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7651 | `	return PH7_OK;` |
|     10 | 7652 | `}` |
|      - | 7653 | `/*` |
|      - | 7654 | ` * bool ctype_digit(string $text)` |
|      - | 7655 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 7656 | ` * Parameters` |
|      - | 7657 | ` *  $text` |
|      - | 7658 | ` *   The tested string.` |
|      - | 7659 | ` * Return` |
|      - | 7660 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 7661 | ` */` |
|   1623 | 7662 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7663 | `{` |
|      - | 7664 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7665 | `	int nLen;` |
|   1628 | 7666 | `	if( nArg < 1 ){` |
|      - | 7667 | `		/* Missing arguments,return FALSE */` |
|      3 | 7668 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7669 | `		return PH7_OK;` |
|      - | 7670 | `	}` |
|      - | 7671 | `	/* Extract the target string */` |
|   1626 | 7672 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1626 | 7673 | `	zEnd = &zIn[nLen];` |
|   1626 | 7674 | `	if( nLen < 1 ){` |
|      - | 7675 | `		/* Empty string,return FALSE */` |
|      3 | 7676 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7677 | `		return PH7_OK;` |
|      - | 7678 | `	}` |
|      - | 7679 | `	/* Perform the requested operation */` |
|   1523 | 7680 | `	for(;;){` |
|   3049 | 7681 | `		if( zIn >= zEnd ){` |
|      - | 7682 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1380 | 7683 | `			ph7_result_bool(pCtx,1);` |
|   1380 | 7684 | `			return PH7_OK;` |
|      - | 7685 | `		}` |
|   1674 | 7686 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7687 | `			/* UTF-8 stream  */` |
|    ! 0 | 7688 | `			break;` |
|      - | 7689 | `		}` |
|   1674 | 7690 | `		if( !SyisDigit(zIn[0]) ){` |
|    249 | 7691 | `			break;` |
|      - | 7692 | `		}` |
|      - | 7693 | `		/* Point to the next character */` |
|   1430 | 7694 | `		zIn++;` |
|      5 | 7695 | `	}` |
|      - | 7696 | `	/* The test failed,return FALSE */` |
|    249 | 7697 | `	ph7_result_bool(pCtx,0);` |
|    249 | 7698 | `	return PH7_OK;` |
|    817 | 7699 | `}` |
|      - | 7700 | `/*` |
|      - | 7701 | ` * bool ctype_xdigit(string $text)` |
|      - | 7702 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 7703 | ` * Parameters` |
|      - | 7704 | ` *  $text` |
|      - | 7705 | ` *   The tested string.` |
|      - | 7706 | ` * Return` |
|      - | 7707 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 7708 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 7709 | ` */` |
|     20 | 7710 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7711 | `{` |
|      - | 7712 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7713 | `	int nLen;` |
|     21 | 7714 | `	if( nArg < 1 ){` |
|      - | 7715 | `		/* Missing arguments,return FALSE */` |
|      3 | 7716 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7717 | `		return PH7_OK;` |
|      - | 7718 | `	}` |
|      - | 7719 | `	/* Extract the target string */` |
|     19 | 7720 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 7721 | `	zEnd = &zIn[nLen];` |
|     19 | 7722 | `	if( nLen < 1 ){` |
|      - | 7723 | `		/* Empty string,return FALSE */` |
|      3 | 7724 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7725 | `		return PH7_OK;` |
|      - | 7726 | `	}` |
|      - | 7727 | `	/* Perform the requested operation */` |
|     46 | 7728 | `	for(;;){` |
|     93 | 7729 | `		if( zIn >= zEnd ){` |
|      - | 7730 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 7731 | `			ph7_result_bool(pCtx,1);` |
|     11 | 7732 | `			return PH7_OK;` |
|      - | 7733 | `		}` |
|     83 | 7734 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7735 | `			/* UTF-8 stream  */` |
|    ! 0 | 7736 | `			break;` |
|      - | 7737 | `		}` |
|     83 | 7738 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 7739 | `			break;` |
|      - | 7740 | `		}` |
|      - | 7741 | `		/* Point to the next character */` |
|     77 | 7742 | `		zIn++;` |
|      1 | 7743 | `	}` |
|      - | 7744 | `	/* The test failed,return FALSE */` |
|      7 | 7745 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7746 | `	return PH7_OK;` |
|     11 | 7747 | `}` |
|      - | 7748 | `/*` |
|      - | 7749 | ` * bool ctype_graph(string $text)` |
|      - | 7750 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 7751 | ` * Parameters` |
|      - | 7752 | ` *  $text` |
|      - | 7753 | ` *   The tested string.` |
|      - | 7754 | ` * Return` |
|      - | 7755 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 7756 | ` * (no white space), FALSE otherwise.` |
|      - | 7757 | ` */` |
|     18 | 7758 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7759 | `{` |
|      - | 7760 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7761 | `	int nLen;` |
|     19 | 7762 | `	if( nArg < 1 ){` |
|      - | 7763 | `		/* Missing arguments,return FALSE */` |
|      3 | 7764 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7765 | `		return PH7_OK;` |
|      - | 7766 | `	}` |
|      - | 7767 | `	/* Extract the target string */` |
|     17 | 7768 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7769 | `	zEnd = &zIn[nLen];` |
|     17 | 7770 | `	if( nLen < 1 ){` |
|      - | 7771 | `		/* Empty string,return FALSE */` |
|      3 | 7772 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7773 | `		return PH7_OK;` |
|      - | 7774 | `	}` |
|      - | 7775 | `	/* Perform the requested operation */` |
|     57 | 7776 | `	for(;;){` |
|    115 | 7777 | `		if( zIn >= zEnd ){` |
|      - | 7778 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7779 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7780 | `			return PH7_OK;` |
|      - | 7781 | `		}` |
|    107 | 7782 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7783 | `			/* UTF-8 stream  */` |
|    ! 0 | 7784 | `			break;` |
|      - | 7785 | `		}` |
|    107 | 7786 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 7787 | `			break;` |
|      - | 7788 | `		}` |
|      - | 7789 | `		/* Point to the next character */` |
|    101 | 7790 | `		zIn++;` |
|      1 | 7791 | `	}` |
|      - | 7792 | `	/* The test failed,return FALSE */` |
|      7 | 7793 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7794 | `	return PH7_OK;` |
|     10 | 7795 | `}` |
|      - | 7796 | `/*` |
|      - | 7797 | ` * bool ctype_print(string $text)` |
|      - | 7798 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 7799 | ` * Parameters` |
|      - | 7800 | ` *  $text` |
|      - | 7801 | ` *   The tested string.` |
|      - | 7802 | ` * Return` |
|      - | 7803 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 7804 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 7805 | ` *  or control function at all.` |
|      - | 7806 | ` */` |
|     18 | 7807 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7808 | `{` |
|      - | 7809 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7810 | `	int nLen;` |
|     19 | 7811 | `	if( nArg < 1 ){` |
|      - | 7812 | `		/* Missing arguments,return FALSE */` |
|      3 | 7813 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7814 | `		return PH7_OK;` |
|      - | 7815 | `	}` |
|      - | 7816 | `	/* Extract the target string */` |
|     17 | 7817 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7818 | `	zEnd = &zIn[nLen];` |
|     17 | 7819 | `	if( nLen < 1 ){` |
|      - | 7820 | `		/* Empty string,return FALSE */` |
|      3 | 7821 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7822 | `		return PH7_OK;` |
|      - | 7823 | `	}` |
|      - | 7824 | `	/* Perform the requested operation */` |
|     63 | 7825 | `	for(;;){` |
|    127 | 7826 | `		if( zIn >= zEnd ){` |
|      - | 7827 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7828 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7829 | `			return PH7_OK;` |
|      - | 7830 | `		}` |
|    119 | 7831 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7832 | `			/* UTF-8 stream  */` |
|    ! 0 | 7833 | `			break;` |
|      - | 7834 | `		}` |
|    119 | 7835 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 7836 | `			break;` |
|      - | 7837 | `		}` |
|      - | 7838 | `		/* Point to the next character */` |
|    113 | 7839 | `		zIn++;` |
|      1 | 7840 | `	}` |
|      - | 7841 | `	/* The test failed,return FALSE */` |
|      7 | 7842 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7843 | `	return PH7_OK;` |
|     10 | 7844 | `}` |
|      - | 7845 | `/*` |
|      - | 7846 | ` * bool ctype_punct(string $text)` |
|      - | 7847 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 7848 | ` * Parameters` |
|      - | 7849 | ` *  $text` |
|      - | 7850 | ` *   The tested string.` |
|      - | 7851 | ` * Return` |
|      - | 7852 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 7853 | ` *  digit or blank, FALSE otherwise.` |
|      - | 7854 | ` */` |
|     20 | 7855 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7856 | `{` |
|      - | 7857 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7858 | `	int nLen;` |
|     21 | 7859 | `	if( nArg < 1 ){` |
|      - | 7860 | `		/* Missing arguments,return FALSE */` |
|      3 | 7861 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7862 | `		return PH7_OK;` |
|      - | 7863 | `	}` |
|      - | 7864 | `	/* Extract the target string */` |
|     19 | 7865 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 7866 | `	zEnd = &zIn[nLen];` |
|     19 | 7867 | `	if( nLen < 1 ){` |
|      - | 7868 | `		/* Empty string,return FALSE */` |
|      3 | 7869 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7870 | `		return PH7_OK;` |
|      - | 7871 | `	}` |
|      - | 7872 | `	/* Perform the requested operation */` |
|     38 | 7873 | `	for(;;){` |
|     77 | 7874 | `		if( zIn >= zEnd ){` |
|      - | 7875 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7876 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7877 | `			return PH7_OK;` |
|      - | 7878 | `		}` |
|     69 | 7879 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7880 | `			/* UTF-8 stream  */` |
|    ! 0 | 7881 | `			break;` |
|      - | 7882 | `		}` |
|     69 | 7883 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 7884 | `			break;` |
|      - | 7885 | `		}` |
|      - | 7886 | `		/* Point to the next character */` |
|     61 | 7887 | `		zIn++;` |
|      1 | 7888 | `	}` |
|      - | 7889 | `	/* The test failed,return FALSE */` |
|      9 | 7890 | `	ph7_result_bool(pCtx,0);` |
|      9 | 7891 | `	return PH7_OK;` |
|     11 | 7892 | `}` |
|      - | 7893 | `/*` |
|      - | 7894 | ` * bool ctype_space(string $text)` |
|      - | 7895 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 7896 | ` * Parameters` |
|      - | 7897 | ` *  $text` |
|      - | 7898 | ` *   The tested string.` |
|      - | 7899 | ` * Return` |
|      - | 7900 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 7901 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 7902 | ` *  and form feed characters.` |
|      - | 7903 | ` */` |
|  62710 | 7904 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7905 | `{` |
|      - | 7906 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7907 | `	int nLen;` |
|  62715 | 7908 | `	if( nArg < 1 ){` |
|      - | 7909 | `		/* Missing arguments,return FALSE */` |
|      3 | 7910 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7911 | `		return PH7_OK;` |
|      - | 7912 | `	}` |
|      - | 7913 | `	/* Extract the target string */` |
|  62713 | 7914 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  62713 | 7915 | `	zEnd = &zIn[nLen];` |
|  62713 | 7916 | `	if( nLen < 1 ){` |
|      - | 7917 | `		/* Empty string,return FALSE */` |
|      3 | 7918 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7919 | `		return PH7_OK;` |
|      - | 7920 | `	}` |
|      - | 7921 | `	/* Perform the requested operation */` |
|  32466 | 7922 | `	for(;;){` |
|  64851 | 7923 | `		if( zIn >= zEnd ){` |
|      - | 7924 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   2121 | 7925 | `			ph7_result_bool(pCtx,1);` |
|   2121 | 7926 | `			return PH7_OK;` |
|      - | 7927 | `		}` |
|  62735 | 7928 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7929 | `			/* UTF-8 stream  */` |
|    ! 0 | 7930 | `			break;` |
|      - | 7931 | `		}` |
|  62735 | 7932 | `		if( !SyisSpace(zIn[0]) ){` |
|  60595 | 7933 | `			break;` |
|      - | 7934 | `		}` |
|      - | 7935 | `		/* Point to the next character */` |
|   2145 | 7936 | `		zIn++;` |
|      5 | 7937 | `	}` |
|      - | 7938 | `	/* The test failed,return FALSE */` |
|  60595 | 7939 | `	ph7_result_bool(pCtx,0);` |
|  60595 | 7940 | `	return PH7_OK;` |
|  31403 | 7941 | `}` |
|      - | 7942 | `/*` |
|      - | 7943 | ` * bool ctype_lower(string $text)` |
|      - | 7944 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 7945 | ` * Parameters` |
|      - | 7946 | ` *  $text` |
|      - | 7947 | ` *   The tested string.` |
|      - | 7948 | ` * Return` |
|      - | 7949 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 7950 | ` */` |
|     18 | 7951 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7952 | `{` |
|      - | 7953 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7954 | `	int nLen;` |
|     19 | 7955 | `	if( nArg < 1 ){` |
|      - | 7956 | `		/* Missing arguments,return FALSE */` |
|      3 | 7957 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7958 | `		return PH7_OK;` |
|      - | 7959 | `	}` |
|      - | 7960 | `	/* Extract the target string */` |
|     17 | 7961 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7962 | `	zEnd = &zIn[nLen];` |
|     17 | 7963 | `	if( nLen < 1 ){` |
|      - | 7964 | `		/* Empty string,return FALSE */` |
|      3 | 7965 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7966 | `		return PH7_OK;` |
|      - | 7967 | `	}` |
|      - | 7968 | `	/* Perform the requested operation */` |
|     27 | 7969 | `	for(;;){` |
|     55 | 7970 | `		if( zIn >= zEnd ){` |
|      - | 7971 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7972 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7973 | `			return PH7_OK;` |
|      - | 7974 | `		}` |
|     51 | 7975 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 7976 | `			break;` |
|      - | 7977 | `		}` |
|      - | 7978 | `		/* Point to the next character */` |
|     41 | 7979 | `		zIn++;` |
|      1 | 7980 | `	}` |
|      - | 7981 | `	/* The test failed,return FALSE */` |
|     11 | 7982 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7983 | `	return PH7_OK;` |
|     10 | 7984 | `}` |
|      - | 7985 | `/*` |
|      - | 7986 | ` * bool ctype_upper(string $text)` |
|      - | 7987 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 7988 | ` * Parameters` |
|      - | 7989 | ` *  $text` |
|      - | 7990 | ` *   The tested string.` |
|      - | 7991 | ` * Return` |
|      - | 7992 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 7993 | ` */` |
|     18 | 7994 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7995 | `{` |
|      - | 7996 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7997 | `	int nLen;` |
|     19 | 7998 | `	if( nArg < 1 ){` |
|      - | 7999 | `		/* Missing arguments,return FALSE */` |
|      3 | 8000 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8001 | `		return PH7_OK;` |
|      - | 8002 | `	}` |
|      - | 8003 | `	/* Extract the target string */` |
|     17 | 8004 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8005 | `	zEnd = &zIn[nLen];` |
|     17 | 8006 | `	if( nLen < 1 ){` |
|      - | 8007 | `		/* Empty string,return FALSE */` |
|      3 | 8008 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8009 | `		return PH7_OK;` |
|      - | 8010 | `	}` |
|      - | 8011 | `	/* Perform the requested operation */` |
|     28 | 8012 | `	for(;;){` |
|     57 | 8013 | `		if( zIn >= zEnd ){` |
|      - | 8014 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 8015 | `			ph7_result_bool(pCtx,1);` |
|      5 | 8016 | `			return PH7_OK;` |
|      - | 8017 | `		}` |
|     53 | 8018 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 8019 | `			break;` |
|      - | 8020 | `		}` |
|      - | 8021 | `		/* Point to the next character */` |
|     43 | 8022 | `		zIn++;` |
|      1 | 8023 | `	}` |
|      - | 8024 | `	/* The test failed,return FALSE */` |
|     11 | 8025 | `	ph7_result_bool(pCtx,0);` |
|     11 | 8026 | `	return PH7_OK;` |
|     10 | 8027 | `}` |
|      - | 8028 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 8029 | `/*` |
|      - | 8030 | ` * Section:` |
|      - | 8031 | ` *    URL handling Functions.` |
|      - | 8032 | ` * Status:` |
|      - | 8033 | ` *    Stable.` |
|      - | 8034 | ` */` |
|      - | 8035 | `/*` |
|      - | 8036 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 8037 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 8038 | ` */` |
|   1026 | 8039 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 8040 | `{` |
|      - | 8041 | `	/* Store in the call context result buffer */` |
|   1028 | 8042 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 8043 | `	return SXRET_OK;` |
|      2 | 8044 | `}` |
|      - | 8045 | `/*` |
|      - | 8046 | ` * string base64_encode(string $data)` |
|      - | 8047 | ` * string convert_uuencode(string $data)` |
|      - | 8048 | ` *  Encodes data with MIME base64` |
|      - | 8049 | ` * Parameter` |
|      - | 8050 | ` *  $data` |
|      - | 8051 | ` *    Data to encode` |
|      - | 8052 | ` * Return` |
|      - | 8053 | ` *  Encoded data or FALSE on failure.` |
|      - | 8054 | ` */` |
|     10 | 8055 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8056 | `{` |
|      - | 8057 | `	const char *zIn;` |
|      - | 8058 | `	int nLen;` |
|     11 | 8059 | `	if( nArg < 1 ){` |
|      - | 8060 | `		/* Missing arguments,return FALSE */` |
|      5 | 8061 | `		ph7_result_bool(pCtx,0);` |
|      5 | 8062 | `		return PH7_OK;` |
|      - | 8063 | `	}` |
|      - | 8064 | `	/* Extract the input string */` |
|      7 | 8065 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8066 | `	if( nLen < 1 ){` |
|      - | 8067 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8068 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8069 | `		return PH7_OK;` |
|      - | 8070 | `	}` |
|      - | 8071 | `	/* Perform the BASE64 encoding */` |
|      7 | 8072 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 8073 | `	return PH7_OK;` |
|      6 | 8074 | `}` |
|      - | 8075 | `/*` |
|      - | 8076 | ` * string base64_decode(string $data)` |
|      - | 8077 | ` * string convert_uudecode(string $data)` |
|      - | 8078 | ` *  Decodes data encoded with MIME base64` |
|      - | 8079 | ` * Parameter` |
|      - | 8080 | ` *  $data` |
|      - | 8081 | ` *    Encoded data.` |
|      - | 8082 | ` * Return` |
|      - | 8083 | ` *  Returns the original data or FALSE on failure.` |
|      - | 8084 | ` */` |
|     36 | 8085 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 8086 | `{` |
|      - | 8087 | `	const char *zIn;` |
|      - | 8088 | `	int nLen;` |
|     38 | 8089 | `	if( nArg < 1 ){` |
|      - | 8090 | `		/* Missing arguments,return FALSE */` |
|      3 | 8091 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8092 | `		return PH7_OK;` |
|      - | 8093 | `	}` |
|      - | 8094 | `	/* Extract the input string */` |
|     36 | 8095 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 8096 | `	if( nLen < 1 ){` |
|      - | 8097 | `		/* Nothing to process,return FALSE */` |
|      3 | 8098 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8099 | `		return PH7_OK;` |
|      - | 8100 | `	}` |
|      - | 8101 | `	/* Perform the BASE64 decoding */` |
|     34 | 8102 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 8103 | `	return PH7_OK;` |
|     20 | 8104 | `}` |
|      - | 8105 | `/*` |
|      - | 8106 | ` * string urlencode(string $str)` |
|      - | 8107 | ` *  URL encoding` |
|      - | 8108 | ` * Parameter` |
|      - | 8109 | ` *  $data` |
|      - | 8110 | ` *   Input string.` |
|      - | 8111 | ` * Return` |
|      - | 8112 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 8113 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 8114 | ` *  encoded as plus (+) signs.` |
|      - | 8115 | ` */` |
|      6 | 8116 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8117 | `{` |
|      - | 8118 | `	const char *zIn;` |
|      - | 8119 | `	int nLen;` |
|      7 | 8120 | `	if( nArg < 1 ){` |
|      - | 8121 | `		/* Missing arguments,return FALSE */` |
|      3 | 8122 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8123 | `		return PH7_OK;` |
|      - | 8124 | `	}` |
|      - | 8125 | `	/* Extract the input string */` |
|      5 | 8126 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 8127 | `	if( nLen < 1 ){` |
|      - | 8128 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8129 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8130 | `		return PH7_OK;` |
|      - | 8131 | `	}` |
|      - | 8132 | `	/* Perform the URL encoding */` |
|      5 | 8133 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 8134 | `	return PH7_OK;` |
|      4 | 8135 | `}` |
|      - | 8136 | `/*` |
|      - | 8137 | ` * string urldecode(string $str)` |
|      - | 8138 | ` *  Decodes any %## encoding in the given string.` |
|      - | 8139 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 8140 | ` * Parameter` |
|      - | 8141 | ` *  $data` |
|      - | 8142 | ` *    Input string.` |
|      - | 8143 | ` * Return` |
|      - | 8144 | ` *  Decoded URL or FALSE on failure.` |
|      - | 8145 | ` */` |
|      8 | 8146 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8147 | `{` |
|      - | 8148 | `	const char *zIn;` |
|      - | 8149 | `	int nLen;` |
|      9 | 8150 | `	if( nArg < 1 ){` |
|      - | 8151 | `		/* Missing arguments,return FALSE */` |
|      3 | 8152 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8153 | `		return PH7_OK;` |
|      - | 8154 | `	}` |
|      - | 8155 | `	/* Extract the input string */` |
|      7 | 8156 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8157 | `	if( nLen < 1 ){` |
|      - | 8158 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8159 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8160 | `		return PH7_OK;` |
|      - | 8161 | `	}` |
|      - | 8162 | `	/* Perform the URL decoding */` |
|      7 | 8163 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 8164 | `	return PH7_OK;` |
|      5 | 8165 | `}` |
|      - | 8166 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8167 | `/* Table of the built-in functions */` |
|      - | 8168 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 8169 | `	   /* Variable handling functions */` |
|      - | 8170 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 8171 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 8172 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 8173 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 8174 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 8175 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 8176 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 8177 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 8178 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 8179 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 8180 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 8181 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 8182 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 8183 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 8184 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 8185 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 8186 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 8187 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 8188 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 8189 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 8190 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8191 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 8192 | `	   /* Math functions */` |
|      - | 8193 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 8194 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 8195 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 8196 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 8197 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 8198 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 8199 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 8200 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 8201 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 8202 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 8203 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 8204 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 8205 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 8206 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 8207 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 8208 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 8209 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 8210 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 8211 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 8212 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 8213 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 8214 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 8215 | `	{ "round",    PH7_builtin_round        },` |
|      - | 8216 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 8217 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 8218 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 8219 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 8220 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 8221 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 8222 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 8223 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 8224 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 8225 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8226 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8227 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 8228 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8229 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8230 | `	   /* String handling functions */` |
|      - | 8231 |  |
|      - | 8232 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 8233 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 8234 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 8235 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 8236 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 8237 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 8238 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 8239 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 8240 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 8241 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 8242 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 8243 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 8244 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 8245 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 8246 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 8247 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 8248 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 8249 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 8250 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 8251 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 8252 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 8253 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 8254 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 8255 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 8256 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 8257 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 8258 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 8259 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 8260 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 8261 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 8262 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 8263 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 8264 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 8265 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 8266 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 8267 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 8268 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 8269 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 8270 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 8271 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 8272 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 8273 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 8274 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 8275 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 8276 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 8277 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 8278 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 8279 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 8280 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - | 8281 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - | 8282 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - | 8283 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 8284 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8285 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8286 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 8287 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 8288 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 8289 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 8290 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8291 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8292 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 8293 |  |
|      - | 8294 |  |
|      - | 8295 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 8296 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 8297 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 8298 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 8299 | `	{ "hash",         PH7_builtin_hash      },` |
|      - | 8300 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|      - | 8301 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|      - | 8302 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|      - | 8303 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 8304 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|      - | 8305 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|      - | 8306 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|      - | 8307 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|      - | 8308 | `	{ "filter_var",            PH7_builtin_filter_var },` |
|      - | 8309 | `	{ "filter_input",          PH7_builtin_filter_input },` |
|      - | 8310 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8311 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8312 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 8313 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 8314 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8315 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8316 |  |
|      - | 8317 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 8318 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 8319 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 8320 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 8321 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 8322 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 8323 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 8324 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 8325 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 8326 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 8327 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 8328 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 8329 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8330 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8331 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 8332 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8333 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8334 |  |
|      - | 8335 | `	         /* Ctype functions */` |
|      - | 8336 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 8337 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 8338 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 8339 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 8340 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 8341 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 8342 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 8343 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 8344 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 8345 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 8346 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 8347 | `	         /* Time functions */` |
|      - | 8348 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 8349 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 8350 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 8351 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 8352 | `	{ "date",        PH7_builtin_date         },` |
|      - | 8353 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 8354 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 8355 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 8356 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 8357 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 8358 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 8359 | `	        /* URL functions */` |
|      - | 8360 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 8361 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 8362 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 8363 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 8364 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 8365 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 8366 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 8367 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 8368 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8369 | `};` |
|      - | 8370 | `/*` |
|      - | 8371 | ` * Register the built-in functions defined above,the array functions` |
|      - | 8372 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 8373 | ` */` |
|   3446 | 8374 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      5 | 8375 | `{` |
|      - | 8376 | `	sxu32 n;` |
| 578933 | 8377 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 575487 | 8378 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 287746 | 8379 | `	}` |
|      - | 8380 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   3451 | 8381 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 8382 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   3451 | 8383 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   3451 | 8384 | `}` |
|      - | 8385 |  |
