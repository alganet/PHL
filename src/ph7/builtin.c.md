# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3268/3710 lines (88.09%)

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
|    112 |   42 | `static int PH7_builtin_is_float(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   43 |  |
|    113 |   44 | `	int res = 0; /* Assume false by default */` |
|    113 |   45 | `	if( nArg > 0 ){` |
|    111 |   46 | `		res = ph7_value_is_float(apArg[0]);` |
|     55 |   47 | `	}` |
|      - |   48 | `	/* Query result */` |
|    113 |   49 | `	ph7_result_bool(pCtx,res);` |
|    113 |   50 | `	return PH7_OK;` |
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
|    514 |   62 | `static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |   63 |  |
|    516 |   64 | `	int res = 0; /* Assume false by default */` |
|    516 |   65 | `	if( nArg > 0 ){` |
|      - |   66 | `		/* Strict PHP identity: a float is never an int, even when it holds an` |
|      - |   67 | `		 * integer value (1.0). An integer-valued real carries both MEMOBJ_INT` |
|      - |   68 | `		 * (cached) and MEMOBJ_REAL, so REAL must be excluded here. */` |
|    514 |   69 | `		res = ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]);` |
|    256 |   70 | `	}` |
|      - |   71 | `	/* Query result */` |
|    516 |   72 | `	ph7_result_bool(pCtx,res);` |
|    516 |   73 | `	return PH7_OK;` |
|      2 |   74 |  |
|      - |   75 | `/*` |
|      - |   76 | ` * bool is_string($var)` |
|      - |   77 | ` *  Finds out whether a variable is a string.` |
|      - |   78 | ` * Parameters` |
|      - |   79 | ` *   $var: The variable being evaluated.` |
|      - |   80 | ` * Return` |
|      - |   81 | ` *  TRUE if var is string. False otherwise.` |
|      - |   82 | ` */` |
|    124 |   83 | `static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   84 |  |
|    125 |   85 | `	int res = 0; /* Assume false by default */` |
|    125 |   86 | `	if( nArg > 0 ){` |
|    123 |   87 | `		res = ph7_value_is_string(apArg[0]);` |
|     61 |   88 | `	}` |
|      - |   89 | `	/* Query result */` |
|    125 |   90 | `	ph7_result_bool(pCtx,res);` |
|    125 |   91 | `	return PH7_OK;` |
|      1 |   92 |  |
|      - |   93 | `/*` |
|      - |   94 | ` * bool is_null($var)` |
|      - |   95 | ` *  Finds out whether a variable is NULL.` |
|      - |   96 | ` * Parameters` |
|      - |   97 | ` *   $var: The variable being evaluated.` |
|      - |   98 | ` * Return` |
|      - |   99 | ` *  TRUE if var is NULL. False otherwise.` |
|      - |  100 | ` */` |
|     86 |  101 | `static int PH7_builtin_is_null(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  102 |  |
|     89 |  103 | `	int res = 0; /* Assume false by default */` |
|     89 |  104 | `	if( nArg > 0 ){` |
|     87 |  105 | `		res = ph7_value_is_null(apArg[0]);` |
|     42 |  106 | `	}` |
|      - |  107 | `	/* Query result */` |
|     89 |  108 | `	ph7_result_bool(pCtx,res);` |
|     89 |  109 | `	return PH7_OK;` |
|      3 |  110 |  |
|      - |  111 | `/*` |
|      - |  112 | ` * bool is_numeric($var)` |
|      - |  113 | ` *  Find out whether a variable is NULL.` |
|      - |  114 | ` * Parameters` |
|      - |  115 | ` *  $var: The variable being evaluated.` |
|      - |  116 | ` * Return` |
|      - |  117 | ` *  True if var is numeric. False otherwise.` |
|      - |  118 | ` */` |
|     38 |  119 | `static int PH7_builtin_is_numeric(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  120 |  |
|     43 |  121 | `	int res = 0; /* Assume false by default */` |
|     43 |  122 | `	if( nArg > 0 ){` |
|     41 |  123 | `		res = ph7_value_is_numeric(apArg[0]);` |
|     18 |  124 | `	}` |
|      - |  125 | `	/* Query result */` |
|     43 |  126 | `	ph7_result_bool(pCtx,res);` |
|     43 |  127 | `	return PH7_OK;` |
|      5 |  128 |  |
|      - |  129 | `/*` |
|      - |  130 | ` * bool is_scalar($var)` |
|      - |  131 | ` *  Find out whether a variable is a scalar.` |
|      - |  132 | ` * Parameters` |
|      - |  133 | ` *  $var: The variable being evaluated.` |
|      - |  134 | ` * Return` |
|      - |  135 | ` *  True if var is scalar. False otherwise.` |
|      - |  136 | ` */` |
|     14 |  137 | `static int PH7_builtin_is_scalar(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  138 |  |
|     15 |  139 | `	int res = 0; /* Assume false by default */` |
|     15 |  140 | `	if( nArg > 0 ){` |
|     13 |  141 | `		res = ph7_value_is_scalar(apArg[0]);` |
|      6 |  142 | `	}` |
|      - |  143 | `	/* Query result */` |
|     15 |  144 | `	ph7_result_bool(pCtx,res);` |
|     15 |  145 | `	return PH7_OK;` |
|      1 |  146 |  |
|      - |  147 | `/*` |
|      - |  148 | ` * bool is_array($var)` |
|      - |  149 | ` *  Find out whether a variable is an array.` |
|      - |  150 | ` * Parameters` |
|      - |  151 | ` *  $var: The variable being evaluated.` |
|      - |  152 | ` * Return` |
|      - |  153 | ` *  True if var is an array. False otherwise.` |
|      - |  154 | ` */` |
|    240 |  155 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  156 |  |
|    244 |  157 | `	int res = 0; /* Assume false by default */` |
|    244 |  158 | `	if( nArg > 0 ){` |
|    242 |  159 | `		res = ph7_value_is_array(apArg[0]);` |
|    119 |  160 | `	}` |
|      - |  161 | `	/* Query result */` |
|    244 |  162 | `	ph7_result_bool(pCtx,res);` |
|    244 |  163 | `	return PH7_OK;` |
|      4 |  164 |  |
|      - |  165 | `/*` |
|      - |  166 | ` * bool is_object($var)` |
|      - |  167 | ` *  Find out whether a variable is an object.` |
|      - |  168 | ` * Parameters` |
|      - |  169 | ` *  $var: The variable being evaluated.` |
|      - |  170 | ` * Return` |
|      - |  171 | ` *  True if var is an object. False otherwise.` |
|      - |  172 | ` */` |
|     20 |  173 | `static int PH7_builtin_is_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  174 |  |
|     21 |  175 | `	int res = 0; /* Assume false by default */` |
|     21 |  176 | `	if( nArg > 0 ){` |
|     19 |  177 | `		res = ph7_value_is_object(apArg[0]);` |
|      9 |  178 | `	}` |
|      - |  179 | `	/* Query result */` |
|     21 |  180 | `	ph7_result_bool(pCtx,res);` |
|     21 |  181 | `	return PH7_OK;` |
|      1 |  182 |  |
|      - |  183 | `/*` |
|      - |  184 | ` * bool is_resource($var)` |
|      - |  185 | ` *  Find out whether a variable is a resource.` |
|      - |  186 | ` * Parameters` |
|      - |  187 | ` *  $var: The variable being evaluated.` |
|      - |  188 | ` * Return` |
|      - |  189 | ` *  True if a resource. False otherwise.` |
|      - |  190 | ` */` |
|     60 |  191 | `static int PH7_builtin_is_resource(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  192 |  |
|     64 |  193 | `	int res = 0; /* Assume false by default */` |
|     64 |  194 | `	if( nArg > 0 ){` |
|     62 |  195 | `		res = ph7_value_is_resource(apArg[0]);` |
|     29 |  196 | `	}` |
|     64 |  197 | `	ph7_result_bool(pCtx,res);` |
|     64 |  198 | `	return PH7_OK;` |
|      4 |  199 |  |
|      - |  200 | `/*` |
|      - |  201 | ` * float floatval($var)` |
|      - |  202 | ` *  Get float value of a variable.` |
|      - |  203 | ` * Parameter` |
|      - |  204 | ` *  $var: The variable being processed.` |
|      - |  205 | ` * Return` |
|      - |  206 | ` *  the float value of a variable.` |
|      - |  207 | ` */` |
|      6 |  208 | `static int PH7_builtin_floatval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  209 |  |
|      7 |  210 | `	if( nArg < 1 ){` |
|      - |  211 | `		/* return 0.0 */` |
|      3 |  212 | `		ph7_result_double(pCtx,0);` |
|      2 |  213 | `	}else{` |
|      - |  214 | `		double dval;` |
|      - |  215 | `		/* Perform the cast */` |
|      5 |  216 | `		dval = ph7_value_to_double(apArg[0]);` |
|      5 |  217 | `		ph7_result_double(pCtx,dval);` |
|      - |  218 | `	}` |
|      7 |  219 | `	return PH7_OK;` |
|      1 |  220 |  |
|      - |  221 | `/*` |
|      - |  222 | ` * int intval($var)` |
|      - |  223 | ` *  Get integer value of a variable.` |
|      - |  224 | ` * Parameter` |
|      - |  225 | ` *  $var: The variable being processed.` |
|      - |  226 | ` * Return` |
|      - |  227 | ` *  the int value of a variable.` |
|      - |  228 | ` */` |
|     26 |  229 | `static int PH7_builtin_intval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  230 |  |
|     27 |  231 | `	if( nArg < 1 ){` |
|      - |  232 | `		/* return 0 */` |
|      3 |  233 | `		ph7_result_int(pCtx,0);` |
|      2 |  234 | `	}else{` |
|      - |  235 | `		sxi64 iVal;` |
|      - |  236 | `		/* Perform the cast */` |
|     25 |  237 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|     25 |  238 | `		ph7_result_int64(pCtx,iVal);` |
|      - |  239 | `	}` |
|     27 |  240 | `	return PH7_OK;` |
|      1 |  241 |  |
|      - |  242 | `/*` |
|      - |  243 | ` * string strval($var)` |
|      - |  244 | ` *  Get the string representation of a variable.` |
|      - |  245 | ` * Parameter` |
|      - |  246 | ` *  $var: The variable being processed.` |
|      - |  247 | ` * Return` |
|      - |  248 | ` *  the string value of a variable.` |
|      - |  249 | ` */` |
|      4 |  250 | `static int PH7_builtin_strval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  251 |  |
|      5 |  252 | `	if( nArg < 1 ){` |
|      - |  253 | `		/* return NULL */` |
|      3 |  254 | `		ph7_result_null(pCtx);` |
|      2 |  255 | `	}else{` |
|      - |  256 | `		const char *zVal;` |
|      3 |  257 | `		int iLen = 0; /* cc -O6 warning */` |
|      - |  258 | `		/* Perform the cast */` |
|      3 |  259 | `		zVal = ph7_value_to_string(apArg[0],&iLen);` |
|      3 |  260 | `		ph7_result_string(pCtx,zVal,iLen);` |
|      - |  261 | `	}` |
|      5 |  262 | `	return PH7_OK;` |
|      1 |  263 |  |
|      - |  264 | `/*` |
|      - |  265 | ` * bool boolval($var)` |
|      - |  266 | ` *  Get the boolean value of a variable.` |
|      - |  267 | ` * Parameter` |
|      - |  268 | ` *  $var: The variable being processed.` |
|      - |  269 | ` * Return` |
|      - |  270 | ` *  the bool value of a variable.` |
|      - |  271 | ` */` |
|     16 |  272 | `static int PH7_builtin_boolval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  273 |  |
|      - |  274 | `	int bVal;` |
|     18 |  275 | `	if( nArg != 1 ){` |
|      4 |  276 | `		return PH7_VmThrowException(pCtx,` |
|      - |  277 | `			"ArgumentCountError",` |
|      - |  278 | `			"boolval() expects exactly 1 argument, %d given",` |
|      1 |  279 | `			nArg` |
|      - |  280 | `			);` |
|      - |  281 | `	}` |
|      - |  282 | `	/* Perform the cast */` |
|     15 |  283 | `	bVal = ph7_value_to_bool(apArg[0]);` |
|     15 |  284 | `	ph7_result_bool(pCtx,bVal);` |
|     15 |  285 | `	return PH7_OK;` |
|     10 |  286 |  |
|      - |  287 | `/*` |
|      - |  288 | ` * bool empty($var)` |
|      - |  289 | ` *  Determine whether a variable is empty.` |
|      - |  290 | ` * Parameters` |
|      - |  291 | ` *   $var: The variable being checked.` |
|      - |  292 | ` * Return` |
|      - |  293 | ` *  0 if var has a non-empty and non-zero value.1 otherwise.` |
|      - |  294 | ` */` |
|  26214 |  295 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  296 |  |
|  26219 |  297 | `	int res = 1; /* Assume empty by default */` |
|  26219 |  298 | `	if( nArg > 0 ){` |
|  26217 |  299 | `		res = ph7_value_is_empty(apArg[0]);` |
|  13106 |  300 | `	}` |
|  26219 |  301 | `	ph7_result_bool(pCtx,res);` |
|  26219 |  302 | `	return PH7_OK;` |
|      - |  303 |  |
|      5 |  304 |  |
|      - |  305 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |  306 | `#define PH7_NEED_BUILTIN_REG 1` |
|      - |  307 | `#endif` |
|      - |  308 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - |  309 | `#define PH7_NEED_FMT_AND_INI 1` |
|      - |  310 | `#endif` |
|      - |  311 |  |
|      - |  312 | `/* Math functions moved to builtin_math.c */` |
|      - |  313 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - |  314 | `/*` |
|      - |  315 | ` * Section:` |
|      - |  316 | ` *    String handling Functions.` |
|      - |  317 | ` * Status:` |
|      - |  318 | ` *    Stable.` |
|      - |  319 | ` */` |
|      - |  320 | `/*` |
|      - |  321 | ` * string substr(string $string,int $start[, int $length ])` |
|      - |  322 | ` *  Return part of a string.` |
|      - |  323 | ` * Parameters` |
|      - |  324 | ` *  $string` |
|      - |  325 | ` *   The input string. Must be one character or longer.` |
|      - |  326 | ` * $start` |
|      - |  327 | ` *   If start is non-negative, the returned string will start at the start'th position` |
|      - |  328 | ` *   in string, counting from zero. For instance, in the string 'abcdef', the character` |
|      - |  329 | ` *   at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - |  330 | ` *   If start is negative, the returned string will start at the start'th character` |
|      - |  331 | ` *   from the end of string.` |
|      - |  332 | ` *   If string is less than or equal to start characters long, FALSE will be returned.` |
|      - |  333 | ` * $length` |
|      - |  334 | ` *   If length is given and is positive, the string returned will contain at most length` |
|      - |  335 | ` *   characters beginning from start (depending on the length of string).` |
|      - |  336 | ` *   If length is given and is negative, then that many characters will be omitted from` |
|      - |  337 | ` *   the end of string (after the start position has been calculated when a start is negative).` |
|      - |  338 | ` *   If start denotes the position of this truncation or beyond, false will be returned.` |
|      - |  339 | ` *   If length is given and is 0, FALSE or NULL an empty string will be returned.` |
|      - |  340 | ` *   If length is omitted, the substring starting from start until the end of the string` |
|      - |  341 | ` *   will be returned.` |
|      - |  342 | ` * Return` |
|      - |  343 | ` *  Returns the extracted part of string, or FALSE on failure or an empty string.` |
|      - |  344 | ` */` |
| 194764 |  345 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  346 |  |
|      - |  347 | `	const char *zSource,*zOfft;` |
|      - |  348 | `	int nOfft,nLen,nSrcLen;` |
| 194769 |  349 | `	if( nArg < 2 ){` |
|      - |  350 | `		/* return FALSE */` |
|      5 |  351 | `		ph7_result_bool(pCtx,0);` |
|      5 |  352 | `		return PH7_OK;` |
|      - |  353 | `	}` |
|      - |  354 | `	/* Extract the target string */` |
| 194765 |  355 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 194765 |  356 | `	if( nSrcLen < 1 ){` |
|      - |  357 | `		/* Empty string,return FALSE */` |
|  11213 |  358 | `		ph7_result_bool(pCtx,0);` |
|  11213 |  359 | `		return PH7_OK;` |
|      - |  360 | `	}` |
| 183557 |  361 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  362 | `	/* Extract the offset */` |
| 183557 |  363 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 183557 |  364 | `	if( nOfft < 0 ){` |
|  30395 |  365 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  30395 |  366 | `		if( zOfft < zSource ){` |
|      - |  367 | `			/* Invalid offset */` |
|      5 |  368 | `			ph7_result_bool(pCtx,0);` |
|      5 |  369 | `			return PH7_OK;` |
|      - |  370 | `		}` |
|  30391 |  371 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  30391 |  372 | `		nOfft = (int)(zOfft-zSource);` |
| 168360 |  373 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  374 | `		/* Invalid offset */` |
|    169 |  375 | `		ph7_result_bool(pCtx,0);` |
|    169 |  376 | `		return PH7_OK;` |
|    ! 0 |  377 | `	}else{` |
| 153003 |  378 | `		zOfft = &zSource[nOfft];` |
| 153003 |  379 | `		nLen = nSrcLen - nOfft;` |
|      - |  380 | `	}` |
| 183389 |  381 | `	if( nArg > 2 ){` |
|      - |  382 | `		/* Extract the length */` |
| 151495 |  383 | `		nLen = ph7_value_to_int(apArg[2]);` |
| 151495 |  384 | `		if( nLen == 0 ){` |
|      - |  385 | `			/* Invalid length,return an empty string */` |
|      5 |  386 | `			ph7_result_string(pCtx,"",0);` |
|      5 |  387 | `			return PH7_OK;` |
| 151491 |  388 | `		}else if( nLen < 0 ){` |
|  30383 |  389 | `			nLen = nSrcLen + nLen - nOfft;` |
|  30383 |  390 | `			if( nLen < 1 ){` |
|      - |  391 | `				/* Invalid  length */` |
|      3 |  392 | `				nLen = nSrcLen - nOfft;` |
|      1 |  393 | `			}` |
|  15189 |  394 | `		}` |
| 151491 |  395 | `		if( nLen + nOfft > nSrcLen ){` |
|      - |  396 | `			/* Invalid length */` |
|   4367 |  397 | `			nLen = nSrcLen - nOfft;` |
|   2181 |  398 | `		}` |
|  75743 |  399 | `	}` |
|      - |  400 | `	/* Return the substring */` |
| 183385 |  401 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 183385 |  402 | `	return PH7_OK;` |
|  97387 |  403 |  |
|      - |  404 | `/*` |
|      - |  405 | ` * int substr_compare(string $main_str,string $str ,int $offset[,int $length[,bool $case_insensitivity = false ]])` |
|      - |  406 | ` *  Binary safe comparison of two strings from an offset, up to length characters.` |
|      - |  407 | ` * Parameters` |
|      - |  408 | ` *  $main_str` |
|      - |  409 | ` *  The main string being compared.` |
|      - |  410 | ` *  $str` |
|      - |  411 | ` *   The secondary string being compared.` |
|      - |  412 | ` * $offset` |
|      - |  413 | ` *  The start position for the comparison. If negative, it starts counting from` |
|      - |  414 | ` *  the end of the string.` |
|      - |  415 | ` * $length` |
|      - |  416 | ` *  The length of the comparison. The default value is the largest of the length` |
|      - |  417 | ` *  of the str compared to the length of main_str less the offset.` |
|      - |  418 | ` * $case_insensitivity` |
|      - |  419 | ` *  If case_insensitivity is TRUE, comparison is case insensitive.` |
|      - |  420 | ` * Return` |
|      - |  421 | ` *  Returns < 0 if main_str from position offset is less than str, > 0 if it is greater than` |
|      - |  422 | ` *  str, and 0 if they are equal. If offset is equal to or greater than the length of main_str` |
|      - |  423 | ` *  or length is set and is less than 1, substr_compare() prints a warning and returns FALSE.` |
|      - |  424 | ` */` |
|     26 |  425 | `static int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  426 |  |
|      - |  427 | `	const char *zSource,*zOfft,*zSub;` |
|      - |  428 | `	int nOfft,nLen,nSrcLen,nSublen;` |
|     27 |  429 | `	int iCase = 0;` |
|      - |  430 | `	int rc;` |
|     27 |  431 | `	if( nArg < 3 ){` |
|      - |  432 | `		/* Missing arguments,return FALSE */` |
|      5 |  433 | `		ph7_result_bool(pCtx,0);` |
|      5 |  434 | `		return PH7_OK;` |
|      - |  435 | `	}` |
|      - |  436 | `	/* Extract the target string */` |
|     23 |  437 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|     23 |  438 | `	if( nSrcLen < 1 ){` |
|      - |  439 | `		/* Empty string,return FALSE */` |
|      3 |  440 | `		ph7_result_bool(pCtx,0);` |
|      3 |  441 | `		return PH7_OK;` |
|      - |  442 | `	}` |
|     21 |  443 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  444 | `	/* Extract the substring */` |
|     21 |  445 | `	zSub = ph7_value_to_string(apArg[1],&nSublen);` |
|     21 |  446 | `	if( nSublen < 1 \|\| nSublen > nSrcLen){` |
|      - |  447 | `		/* Empty string,return FALSE */` |
|      3 |  448 | `		ph7_result_bool(pCtx,0);` |
|      3 |  449 | `		return PH7_OK;` |
|      - |  450 | `	}` |
|      - |  451 | `	/* Extract the offset */` |
|     19 |  452 | `	nOfft = ph7_value_to_int(apArg[2]);` |
|     19 |  453 | `	if( nOfft < 0 ){` |
|      5 |  454 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|      5 |  455 | `		if( zOfft < zSource ){` |
|      - |  456 | `			/* Invalid offset */` |
|      3 |  457 | `			ph7_result_bool(pCtx,0);` |
|      3 |  458 | `			return PH7_OK;` |
|      - |  459 | `		}` |
|      3 |  460 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|      3 |  461 | `		nOfft = (int)(zOfft-zSource);` |
|     16 |  462 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  463 | `		/* Invalid offset */` |
|      3 |  464 | `		ph7_result_bool(pCtx,0);` |
|      3 |  465 | `		return PH7_OK;` |
|    ! 0 |  466 | `	}else{` |
|     13 |  467 | `		zOfft = &zSource[nOfft];` |
|     13 |  468 | `		nLen = nSrcLen - nOfft;` |
|      - |  469 | `	}` |
|     15 |  470 | `	if( nArg > 3 ){` |
|      - |  471 | `		/* Extract the length */` |
|     13 |  472 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     13 |  473 | `		if( nLen < 1 ){` |
|      - |  474 | `			/* Invalid  length */` |
|      5 |  475 | `			ph7_result_int(pCtx,1);` |
|      5 |  476 | `			return PH7_OK;` |
|      9 |  477 | `		}else if( nLen + nOfft > nSrcLen ){` |
|      - |  478 | `			/* Invalid length */` |
|      3 |  479 | `			nLen = nSrcLen - nOfft;` |
|      1 |  480 | `		}` |
|      9 |  481 | `		if( nArg > 4 ){` |
|      - |  482 | `			/* Case-sensitive or not */` |
|      5 |  483 | `			iCase = ph7_value_to_bool(apArg[4]);` |
|      2 |  484 | `		}` |
|      4 |  485 | `	}` |
|      - |  486 | `	/* Perform the comparison */` |
|     11 |  487 | `	if( iCase ){` |
|      3 |  488 | `		rc = SyStrnicmp(zOfft,zSub,(sxu32)nLen);` |
|      2 |  489 | `	}else{` |
|      9 |  490 | `		rc = SyStrncmp(zOfft,zSub,(sxu32)nLen);` |
|      - |  491 | `	}` |
|      - |  492 | `	/* Comparison result */` |
|     11 |  493 | `	ph7_result_int(pCtx,rc);` |
|     11 |  494 | `	return PH7_OK;` |
|     14 |  495 |  |
|      - |  496 | `/*` |
|      - |  497 | ` * int substr_count(string $haystack,string $needle[,int $offset = 0 [,int $length ]])` |
|      - |  498 | ` *  Count the number of substring occurrences.` |
|      - |  499 | ` * Parameters` |
|      - |  500 | ` * $haystack` |
|      - |  501 | ` *   The string to search in` |
|      - |  502 | ` * $needle` |
|      - |  503 | ` *   The substring to search for` |
|      - |  504 | ` * $offset` |
|      - |  505 | ` *  The offset where to start counting` |
|      - |  506 | ` * $length (NOT USED)` |
|      - |  507 | ` *  The maximum length after the specified offset to search for the substring.` |
|      - |  508 | ` *  It outputs a warning if the offset plus the length is greater than the haystack length.` |
|      - |  509 | ` * Return` |
|      - |  510 | ` *  Toral number of substring occurrences.` |
|      - |  511 | ` */` |
|     24 |  512 | `static int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  513 |  |
|      - |  514 | `	const char *zText,*zPattern,*zEnd;` |
|      - |  515 | `	int nTextlen,nPatlen;` |
|     25 |  516 | `	int iCount = 0;` |
|      - |  517 | `	sxu32 nOfft;` |
|      - |  518 | `	sxi32 rc;` |
|     25 |  519 | `	if( nArg < 2 ){` |
|      - |  520 | `		/* Missing arguments */` |
|      5 |  521 | `		ph7_result_int(pCtx,0);` |
|      5 |  522 | `		return PH7_OK;` |
|      - |  523 | `	}` |
|      - |  524 | `	/* Point to the haystack */` |
|     21 |  525 | `	zText = ph7_value_to_string(apArg[0],&nTextlen);` |
|      - |  526 | `	/* Point to the neddle */` |
|     21 |  527 | `	zPattern = ph7_value_to_string(apArg[1],&nPatlen);` |
|     21 |  528 | `	if( nTextlen < 1 \|\| nPatlen < 1 \|\| nPatlen > nTextlen ){` |
|      - |  529 | `		/* NOOP,return zero */` |
|      3 |  530 | `		ph7_result_int(pCtx,0);` |
|      3 |  531 | `		return PH7_OK;` |
|      - |  532 | `	}` |
|     19 |  533 | `	if( nArg > 2 ){` |
|      - |  534 | `		int iOfft;` |
|      - |  535 | `		/* Extract the offset */` |
|     15 |  536 | `		iOfft = ph7_value_to_int(apArg[2]);` |
|     15 |  537 | `		if( iOfft < 0 \|\| iOfft > nTextlen ){` |
|      - |  538 | `			/* Invalid offset,return zero */` |
|      3 |  539 | `			ph7_result_int(pCtx,0);` |
|      3 |  540 | `			return PH7_OK;` |
|      - |  541 | `		}` |
|      - |  542 | `		/* Point to the desired offset */` |
|     13 |  543 | `		zText = &zText[iOfft];` |
|      - |  544 | `		/* Adjust length */` |
|     13 |  545 | `		nTextlen -= iOfft;` |
|      6 |  546 | `	}` |
|      - |  547 | `	/* Point to the end of the string */` |
|     17 |  548 | `	zEnd = &zText[nTextlen];` |
|     17 |  549 | `	if( nArg > 3 ){` |
|      - |  550 | `		int nLen;` |
|      - |  551 | `		/* Extract the length */` |
|     13 |  552 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     13 |  553 | `		if( nLen < 0 \|\| nLen > nTextlen ){` |
|      - |  554 | `			/* Invalid length,return 0 */` |
|      7 |  555 | `			ph7_result_int(pCtx,0);` |
|      7 |  556 | `			return PH7_OK;` |
|      - |  557 | `		}` |
|      - |  558 | `		/* Adjust pointer */` |
|      7 |  559 | `		nTextlen = nLen;` |
|      7 |  560 | `		zEnd = &zText[nTextlen];` |
|      3 |  561 | `	}` |
|      - |  562 | `	/* Perform the search */` |
|     12 |  563 | `	for(;;){` |
|     25 |  564 | `		rc = SyBlobSearch((const void *)zText,(sxu32)(zEnd-zText),(const void *)zPattern,nPatlen,&nOfft);` |
|     25 |  565 | `		if( rc != SXRET_OK ){` |
|      - |  566 | `			/* Pattern not found,break immediately */` |
|      9 |  567 | `			break;` |
|      - |  568 | `		}` |
|      - |  569 | `		/* Increment counter and update the offset */` |
|     17 |  570 | `		iCount++;` |
|     17 |  571 | `		zText += nOfft + nPatlen;` |
|     17 |  572 | `		if( zText >= zEnd ){` |
|      3 |  573 | `			break;` |
|      - |  574 | `		}` |
|      1 |  575 | `	}` |
|      - |  576 | `	/* Pattern count */` |
|     11 |  577 | `	ph7_result_int(pCtx,iCount);` |
|     11 |  578 | `	return PH7_OK;` |
|     13 |  579 |  |
|      - |  580 | `/*` |
|      - |  581 | ` * string chunk_split(string $body[,int $chunklen = 76 [, string $end = "\r\n" ]])` |
|      - |  582 | ` *   Split a string into smaller chunks.` |
|      - |  583 | ` * Parameters` |
|      - |  584 | ` *  $body` |
|      - |  585 | ` *   The string to be chunked.` |
|      - |  586 | ` * $chunklen` |
|      - |  587 | ` *   The chunk length.` |
|      - |  588 | ` * $end` |
|      - |  589 | ` *   The line ending sequence.` |
|      - |  590 | ` * Return` |
|      - |  591 | ` *  The chunked string or NULL on failure.` |
|      - |  592 | ` */` |
|     16 |  593 | `static int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  594 |  |
|     17 |  595 | `	const char *zIn,*zEnd,*zSep = "\r\n";` |
|      - |  596 | `	int nSepLen,nChunkLen,nLen;` |
|     17 |  597 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  598 | `		/* Nothing to split,return null */` |
|      5 |  599 | `		ph7_result_null(pCtx);` |
|      5 |  600 | `		return PH7_OK;` |
|      - |  601 | `	}` |
|      - |  602 | `	/* initialize/Extract arguments */` |
|     13 |  603 | `	nSepLen = (int)sizeof("\r\n") - 1;` |
|     13 |  604 | `	nChunkLen = 76;` |
|     13 |  605 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 |  606 | `	zEnd = &zIn[nLen];` |
|     13 |  607 | `	if( nArg > 1 ){` |
|      - |  608 | `		/* Chunk length */` |
|     13 |  609 | `		nChunkLen = ph7_value_to_int(apArg[1]);` |
|     13 |  610 | `		if( nChunkLen < 1 ){` |
|      - |  611 | `			/* Switch back to the default length */` |
|      3 |  612 | `			nChunkLen = 76;` |
|      1 |  613 | `		}` |
|     13 |  614 | `		if( nArg > 2 ){` |
|      - |  615 | `			/* Separator */` |
|      9 |  616 | `			zSep = ph7_value_to_string(apArg[2],&nSepLen);` |
|      9 |  617 | `			if( nSepLen < 1 ){` |
|      - |  618 | `				/* Switch back to the default separator */` |
|      3 |  619 | `				zSep = "\r\n";` |
|      3 |  620 | `				nSepLen = (int)sizeof("\r\n") - 1;` |
|      1 |  621 | `			}` |
|      4 |  622 | `		}` |
|      6 |  623 | `	}` |
|      - |  624 | `	/* Perform the requested operation */` |
|     13 |  625 | `	if( nChunkLen > nLen ){` |
|      - |  626 | `		/* Nothing to split,return the string and the separator */` |
|      9 |  627 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);` |
|      9 |  628 | `		return PH7_OK;` |
|      - |  629 | `	}` |
|     17 |  630 | `	while( zIn < zEnd ){` |
|     13 |  631 | `		if( nChunkLen > (int)(zEnd-zIn) ){` |
|      3 |  632 | `			nChunkLen = (int)(zEnd - zIn);` |
|      1 |  633 | `		}` |
|      - |  634 | `		/* Append the chunk and the separator */` |
|     13 |  635 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);` |
|      - |  636 | `		/* Point beyond the chunk */` |
|     13 |  637 | `		zIn += nChunkLen;` |
|      1 |  638 | `	}` |
|      5 |  639 | `	return PH7_OK;` |
|      9 |  640 |  |
|      - |  641 | `/*` |
|      - |  642 | ` * string addslashes(string $str)` |
|      - |  643 | ` *  Quote string with slashes.` |
|      - |  644 | ` *  Returns a string with backslashes before characters that need` |
|      - |  645 | ` *  to be quoted in database queries etc. These characters are single` |
|      - |  646 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - |  647 | ` * Parameter` |
|      - |  648 | ` *  str: The string to be escaped.` |
|      - |  649 | ` * Return` |
|      - |  650 | ` *  Returns the escaped string` |
|      - |  651 | ` */` |
|     24 |  652 | `static int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  653 |  |
|      - |  654 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  655 | `	int nLen;` |
|      - |  656 | `	/* PHP enforces exactly one argument. */` |
|     28 |  657 | `	if( nArg != 1 ){` |
|      8 |  658 | `		return PH7_VmThrowException(pCtx,` |
|      - |  659 | `			"ArgumentCountError",` |
|      - |  660 | `			"addslashes() expects exactly 1 argument, %d given",` |
|      2 |  661 | `			nArg` |
|      - |  662 | `			);` |
|      - |  663 | `	}` |
|      - |  664 | `	/* NULL is deprecated and treated as an empty string; other invalid` |
|      - |  665 | `	 * types still produce a TypeError. */` |
|     22 |  666 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 |  667 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - |  668 | `			E_DEPRECATED,` |
|      - |  669 | `			"addslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - |  670 | `			);` |
|      - |  671 | `		/* fall through so conversion below yields empty string */` |
|      1 |  672 | `	}` |
|      - |  673 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     38 |  674 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     29 |  675 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     18 |  676 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 |  677 | `		return PH7_VmThrowException(pCtx,` |
|      - |  678 | `			"TypeError",` |
|      - |  679 | `			"addslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 |  680 | `			ph7_type_name(apArg[0])` |
|      - |  681 | `			);` |
|      - |  682 | `	}` |
|      - |  683 | `	/* Convert to string representation first and obtain length. */` |
|     19 |  684 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     19 |  685 | `	if( nLen < 1 ){` |
|      - |  686 | `		/* Return the empty string */` |
|      5 |  687 | `		ph7_result_string(pCtx,"",0);` |
|      5 |  688 | `		return PH7_OK;` |
|      - |  689 | `	}` |
|     15 |  690 | `	zEnd = &zIn[nLen];` |
|     15 |  691 | `	zCur = 0; /* cc warning */` |
|     20 |  692 | `	for(;;){` |
|     41 |  693 | `		if( zIn >= zEnd ){` |
|      - |  694 | `			/* No more input */` |
|     15 |  695 | `			break;` |
|      - |  696 | `		}` |
|     27 |  697 | `		zCur = zIn;` |
|      - |  698 | `		/* scan until a character that needs escaping (', ", \\, or NUL) */` |
|     89 |  699 | `		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' && zIn[0] != '\0' ){` |
|     63 |  700 | `			zIn++;` |
|      1 |  701 | `		}` |
|     27 |  702 | `		if( zIn > zCur ){` |
|      - |  703 | `			/* Append raw contents */` |
|     23 |  704 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 |  705 | `		}` |
|     27 |  706 | `		if( zIn < zEnd ){` |
|     17 |  707 | `			int c = zIn[0];` |
|     17 |  708 | `			if( c == '\0' ){` |
|      - |  709 | `				/* PHP escapes NUL as "\\0" (two characters) */` |
|      3 |  710 | `				ph7_result_string(pCtx,"\\0",2);` |
|      2 |  711 | `			}else{` |
|     15 |  712 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - |  713 | `			}` |
|      8 |  714 | `		}` |
|     27 |  715 | `		zIn++;` |
|      1 |  716 | `	}` |
|     15 |  717 | `	return PH7_OK;` |
|     16 |  718 |  |
|      - |  719 | `/*` |
|      - |  720 | ` * Check if the given character is present in the given mask.` |
|      - |  721 | ` * Return TRUE if present. FALSE otherwise.` |
|      - |  722 | ` */` |
|    124 |  723 | `static int cSlashCheckMask(int c,const char *zMask,int nLen)` |
|      1 |  724 |  |
|    125 |  725 | `	const char *zEnd = &zMask[nLen];` |
|    555 |  726 | `	while( zMask < zEnd ){` |
|      - |  727 | `		/* Support range syntax A..Z where A and Z are literal bytes.  The` |
|      - |  728 | `		 * original PH7 implementation ignored ranges; tests rely on them so` |
|      - |  729 | `		 * provide a simple on-the-fly check here. */` |
|    475 |  730 | `		if( zMask + 3 < zEnd && zMask[1] == '.' && zMask[2] == '.' ){` |
|      3 |  731 | `			int lo = (unsigned char)zMask[0];` |
|      3 |  732 | `			int hi = (unsigned char)zMask[3];` |
|      3 |  733 | `			if( lo > hi ){` |
|    ! 0 |  734 | `				int tmp = lo; lo = hi; hi = tmp;` |
|    ! 0 |  735 | `			}` |
|      3 |  736 | `			if( c >= lo && c <= hi ){` |
|      3 |  737 | `				return 1;` |
|      - |  738 | `			}` |
|      - |  739 | `			/* consume the range specifier */` |
|    ! 0 |  740 | `			zMask += 4;` |
|    ! 0 |  741 | `			continue;` |
|      - |  742 | `		}` |
|    473 |  743 | `		if( zMask[0] == c ){` |
|      - |  744 | `			/* Character present,return TRUE */` |
|     43 |  745 | `			return 1;` |
|      - |  746 | `		}` |
|      - |  747 | `		/* Advance the pointer */` |
|    431 |  748 | `		zMask++;` |
|      1 |  749 | `	}` |
|      - |  750 | `	/* Not present */` |
|     81 |  751 | `	return 0;` |
|     63 |  752 |  |
|      - |  753 | `/*` |
|      - |  754 | ` * string addcslashes(string $str,string $charlist)` |
|      - |  755 | ` *  Quote string with slashes in a C style.` |
|      - |  756 | ` * Parameter` |
|      - |  757 | ` *  $str:` |
|      - |  758 | ` *    The string to be escaped.` |
|      - |  759 | ` *  $charlist:` |
|      - |  760 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|      - |  761 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|      - |  762 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|      - |  763 | ` * Return` |
|      - |  764 | ` *  Returns the escaped string.` |
|      - |  765 | ` * Note:` |
|      - |  766 | ` *  Range characters [i.e: 'A..Z'] is not implemented in the current release.` |
|      - |  767 | ` */` |
|     34 |  768 | `static int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  769 |  |
|      - |  770 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - |  771 | `	int nLen,nMask;` |
|      - |  772 | `	/* PHP enforces exactly two arguments. */` |
|     39 |  773 | `	if( nArg != 2 ){` |
|      8 |  774 | `		return PH7_VmThrowException(pCtx,` |
|      - |  775 | `			"ArgumentCountError",` |
|      - |  776 | `			"addcslashes() expects exactly 2 arguments, %d given",` |
|      2 |  777 | `			nArg` |
|      - |  778 | `			);` |
|      - |  779 | `	}` |
|      - |  780 | `	/* First argument must be a string-ish value.  NULL is deprecated and` |
|      - |  781 | `	 * treated as the empty string (PHP 8.1). */` |
|     33 |  782 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      - |  783 | `		/* Emit deprecation only once, similar to PHP behaviour. */` |
|      3 |  784 | `		PH7_VmThrowError(pCtx->pVm,0,/* iErr will be patched to 8192 below */` |
|      - |  785 | `			E_DEPRECATED,` |
|      - |  786 | `			"addcslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - |  787 | `			);` |
|      - |  788 | `		/* treat as empty string; fall through to conversion logic */` |
|     56 |  789 | `	} else if( ph7_value_is_array(apArg[0]) \|\|` |
|     42 |  790 | `	          ph7_value_is_object(apArg[0]) \|\|` |
|     26 |  791 | `	          ph7_value_is_resource(apArg[0]) ){` |
|      4 |  792 | `		return PH7_VmThrowException(pCtx,` |
|      - |  793 | `			"TypeError",` |
|      - |  794 | `			"addcslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 |  795 | `			ph7_type_name(apArg[0])` |
|      - |  796 | `			);` |
|      - |  797 | `	}` |
|      - |  798 | `	/* Second argument must be a string.  NULL is deprecated and treated as` |
|      - |  799 | `	 * an empty mask per PHP semantics.  Arrays/objects/resources still` |
|      - |  800 | `	 * trigger a TypeError. */` |
|     30 |  801 | `	if( ph7_value_is_null(apArg[1]) ){` |
|      3 |  802 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - |  803 | `			E_DEPRECATED,` |
|      - |  804 | `			"addcslashes(): Passing null to parameter #2 ($characters) of type string is deprecated"` |
|      - |  805 | `			);` |
|      - |  806 | `		/* allow through so it becomes empty string below */` |
|     52 |  807 | `	} else if( ph7_value_is_array(apArg[1]) \|\|` |
|     38 |  808 | `	          ph7_value_is_object(apArg[1]) \|\|` |
|     24 |  809 | `	          ph7_value_is_resource(apArg[1]) ){` |
|      4 |  810 | `		return PH7_VmThrowException(pCtx,` |
|      - |  811 | `			"TypeError",` |
|      - |  812 | `			"addcslashes(): Argument #2 ($characters) must be of type string, %s given",` |
|      2 |  813 | `			ph7_type_name(apArg[1])` |
|      - |  814 | `			);` |
|      - |  815 | `	}` |
|      - |  816 | `	/* Extract the string to process */` |
|     27 |  817 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      - |  818 | `	/* NULL would never reach here due to the check above. */` |
|     27 |  819 | `	if( nLen < 1 ){` |
|      - |  820 | `		/* Empty string returns itself. */` |
|      5 |  821 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      5 |  822 | `		return PH7_OK;` |
|      - |  823 | `	}` |
|      - |  824 | `	/* Extract the desired mask */` |
|     23 |  825 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|     23 |  826 | `	zEnd = &zIn[nLen];` |
|     23 |  827 | `	zCur = 0; /* cc warning */` |
|     29 |  828 | `	for(;;){` |
|     59 |  829 | `		if( zIn >= zEnd ){` |
|      - |  830 | `			/* No more input */` |
|     23 |  831 | `			break;` |
|      - |  832 | `		}` |
|     37 |  833 | `		zCur = zIn;` |
|     91 |  834 | `		while( zIn < zEnd && !cSlashCheckMask(zIn[0],zMask,nMask) ){` |
|     55 |  835 | `			zIn++;` |
|      1 |  836 | `		}` |
|     37 |  837 | `		if( zIn > zCur ){` |
|      - |  838 | `			/* Append raw contents */` |
|     33 |  839 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 |  840 | `		}` |
|     37 |  841 | `		if( zIn < zEnd ){` |
|      - |  842 | `			/* Make sure we treat the byte as unsigned to avoid negative values` |
|      - |  843 | `			 * on platforms where char is signed. */` |
|     19 |  844 | `			int c = (unsigned char)zIn[0];` |
|      - |  845 | `			/* Handle special C-like escapes for common control characters first.` |
|      - |  846 | `			 * PHP outputs "\n" "\r" "\t" "\v" "\f" when those chars are` |
|      - |  847 | `			 * in the mask. NUL is left to the octal conversion below. */` |
|     19 |  848 | `			if( c == '\n' ){` |
|      3 |  849 | `				ph7_result_string(pCtx,"\\n",2);` |
|     18 |  850 | `			}else if( c == '\r' ){` |
|      3 |  851 | `				ph7_result_string(pCtx,"\\r",2);` |
|     16 |  852 | `			}else if( c == '\t' ){` |
|      3 |  853 | `				ph7_result_string(pCtx,"\\t",2);` |
|     14 |  854 | `			}else if( c == '\v' ){` |
|      3 |  855 | `				ph7_result_string(pCtx,"\\v",2);` |
|     12 |  856 | `			}else if( c == '\f' ){` |
|      3 |  857 | `				ph7_result_string(pCtx,"\\f",2);` |
|     10 |  858 | `			}else if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|      - |  859 | `				/* Convert to octal.  PHP always emits three-digit zero-padded` |
|      - |  860 | `				 * octal escapes (\001 not \1). */` |
|      7 |  861 | `				ph7_result_string_format(pCtx,"\\%03o",c);` |
|      4 |  862 | `			}else{` |
|      3 |  863 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - |  864 | `			}` |
|      9 |  865 | `		}` |
|     37 |  866 | `		zIn++;` |
|      1 |  867 | `	}` |
|     23 |  868 | `	return PH7_OK;` |
|     22 |  869 |  |
|      - |  870 | `/*` |
|      - |  871 | ` * string quotemeta(string $str)` |
|      - |  872 | ` *  Quote meta characters.` |
|      - |  873 | ` * Parameter` |
|      - |  874 | ` *  $str:` |
|      - |  875 | ` *    The string to be escaped.` |
|      - |  876 | ` * Return` |
|      - |  877 | ` *  Returns the escaped string.` |
|      - |  878 | `*/` |
|     10 |  879 | `static int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  880 |  |
|      - |  881 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  882 | `	int nLen;` |
|     11 |  883 | `	if( nArg < 1 ){` |
|      - |  884 | `		/* Nothing to process,retun NULL */` |
|      3 |  885 | `		ph7_result_null(pCtx);` |
|      3 |  886 | `		return PH7_OK;` |
|      - |  887 | `	}` |
|      - |  888 | `	/* Extract the string to process */` |
|      9 |  889 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      9 |  890 | `	if( nLen < 1 ){` |
|      - |  891 | `		/* Return the empty string */` |
|      3 |  892 | `		ph7_result_string(pCtx,"",0);` |
|      3 |  893 | `		return PH7_OK;` |
|      - |  894 | `	}` |
|      7 |  895 | `	zEnd = &zIn[nLen];` |
|      7 |  896 | `	zCur = 0; /* cc warning */` |
|     17 |  897 | `	for(;;){` |
|     35 |  898 | `		if( zIn >= zEnd ){` |
|      - |  899 | `			/* No more input */` |
|      7 |  900 | `			break;` |
|      - |  901 | `		}` |
|     29 |  902 | `		zCur = zIn;` |
|     55 |  903 | `		while( zIn < zEnd && !cSlashCheckMask(zIn[0],".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1) ){` |
|     27 |  904 | `			zIn++;` |
|      1 |  905 | `		}` |
|     29 |  906 | `		if( zIn > zCur ){` |
|      - |  907 | `			/* Append raw contents */` |
|     11 |  908 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      5 |  909 | `		}` |
|     29 |  910 | `		if( zIn < zEnd ){` |
|     27 |  911 | `			int c = zIn[0];` |
|     27 |  912 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|     13 |  913 | `		}` |
|     29 |  914 | `		zIn++;` |
|      1 |  915 | `	}` |
|      7 |  916 | `	return PH7_OK;` |
|      6 |  917 |  |
|      - |  918 | `/*` |
|      - |  919 | ` * string stripslashes(string $str)` |
|      - |  920 | ` *  Un-quotes a quoted string.` |
|      - |  921 | ` *  Returns a string with backslashes before characters that need` |
|      - |  922 | ` *  to be quoted in database queries etc. These characters are single` |
|      - |  923 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - |  924 | ` * Parameter` |
|      - |  925 | ` *  $str` |
|      - |  926 | ` *   The input string.` |
|      - |  927 | ` * Return` |
|      - |  928 | ` *  Returns a string with backslashes stripped off.` |
|      - |  929 | ` */` |
|      8 |  930 | `static int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  931 |  |
|      - |  932 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  933 | `	int nLen;` |
|      9 |  934 | `	if( nArg < 1 ){` |
|      - |  935 | `		/* Nothing to process,retun NULL */` |
|      3 |  936 | `		ph7_result_null(pCtx);` |
|      3 |  937 | `		return PH7_OK;` |
|      - |  938 | `	}` |
|      - |  939 | `	/* Extract the string to process */` |
|      7 |  940 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 |  941 | `	if( zIn == 0 ){` |
|    ! 0 |  942 | `		ph7_result_null(pCtx);` |
|    ! 0 |  943 | `		return PH7_OK;` |
|      - |  944 | `	}` |
|      7 |  945 | `	zEnd = &zIn[nLen];` |
|      7 |  946 | `	zCur = 0; /* cc warning */` |
|      - |  947 | `	/* Encode the string */` |
|      4 |  948 | `	for(;;){` |
|      9 |  949 | `		if( zIn >= zEnd ){` |
|      - |  950 | `			/* No more input */` |
|      5 |  951 | `			break;` |
|      - |  952 | `		}` |
|      5 |  953 | `		zCur = zIn;` |
|     17 |  954 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 |  955 | `			zIn++;` |
|      1 |  956 | `		}` |
|      5 |  957 | `		if( zIn > zCur ){` |
|      - |  958 | `			/* Append raw contents */` |
|      5 |  959 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 |  960 | `		}` |
|      5 |  961 | `		if( &zIn[1] < zEnd ){` |
|      3 |  962 | `			int c = zIn[1];` |
|      3 |  963 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - |  964 | `				/* Ignore the backslash */` |
|      3 |  965 | `				zIn++;` |
|      1 |  966 | `			}` |
|      2 |  967 | `		}else{` |
|      3 |  968 | `			break;` |
|      - |  969 | `		}` |
|      1 |  970 | `	}` |
|      7 |  971 | `	return PH7_OK;` |
|      5 |  972 |  |
|      - |  973 | `/*` |
|      - |  974 | ` * string htmlspecialchars(string $string [, int $flags = ENT_COMPAT \| ENT_HTML401 [, string $charset]])` |
|      - |  975 | ` *  HTML escaping of special characters.` |
|      - |  976 | ` *  The translations performed are:` |
|      - |  977 | ` *   '&' (ampersand) ==> '&amp;'` |
|      - |  978 | ` *   '"' (double quote) ==> '&quot;' when ENT_NOQUOTES is not set.` |
|      - |  979 | ` *   "'" (single quote) ==> '&#039;' only when ENT_QUOTES is set.` |
|      - |  980 | ` *   '<' (less than) ==> '&lt;'` |
|      - |  981 | ` *   '>' (greater than) ==> '&gt;'` |
|      - |  982 | ` * Parameters` |
|      - |  983 | ` *  $string` |
|      - |  984 | ` *   The string being converted.` |
|      - |  985 | ` * $flags` |
|      - |  986 | ` *   A bitmask of one or more of the following flags, which specify how to handle quotes.` |
|      - |  987 | ` *   The default is ENT_COMPAT \| ENT_HTML401.` |
|      - |  988 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone.` |
|      - |  989 | ` *   ENT_QUOTES 	Will convert both double and single quotes.` |
|      - |  990 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted.` |
|      - |  991 | ` *   ENT_IGNORE 	Silently discard invalid code unit sequences instead of returning an empty string.` |
|      - |  992 | ` * $charset` |
|      - |  993 | ` *  Defines character set used in conversion. The default character set is ISO-8859-1. (Not used)` |
|      - |  994 | ` * Return` |
|      - |  995 | ` *  The escaped string or NULL on failure.` |
|      - |  996 | ` */` |
|     20 |  997 | `static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  998 |  |
|      - |  999 | `	const char *zCur,*zIn,*zEnd;` |
|     21 | 1000 | `	int iFlags = 0x01\|0x40; /* ENT_COMPAT \| ENT_HTML401 */` |
|      - | 1001 | `	int nLen,c;` |
|     21 | 1002 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1003 | `		/* Missing/Invalid arguments,return NULL */` |
|      9 | 1004 | `		ph7_result_null(pCtx);` |
|      9 | 1005 | `		return PH7_OK;` |
|      - | 1006 | `	}` |
|      - | 1007 | `	/* Extract the target string */` |
|     13 | 1008 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1009 | `	/* Return early when the input is empty, mirroring PHP's behavior. */` |
|     13 | 1010 | `	if( nLen == 0 ){` |
|      3 | 1011 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1012 | `		return PH7_OK;` |
|      - | 1013 | `	}` |
|     11 | 1014 | `	zEnd = &zIn[nLen];` |
|      - | 1015 | `	/* Extract the flags if available */` |
|     11 | 1016 | `	if( nArg > 1 ){` |
|      9 | 1017 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 | 1018 | `		if( iFlags < 0 ){` |
|      3 | 1019 | `			iFlags = 0x01\|0x40;` |
|      1 | 1020 | `		}` |
|      4 | 1021 | `	}` |
|      - | 1022 | `	/* Perform the requested operation */` |
|     23 | 1023 | `	for(;;){` |
|     47 | 1024 | `		if( zIn >= zEnd ){` |
|      9 | 1025 | `			break;` |
|      - | 1026 | `		}` |
|     39 | 1027 | `		zCur = zIn;` |
|     83 | 1028 | `		while( zIn < zEnd && zIn[0] != '&' && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '<' && zIn[0] != '>' ){` |
|     45 | 1029 | `			zIn++;` |
|      1 | 1030 | `		}` |
|     39 | 1031 | `		if( zCur < zIn ){` |
|      - | 1032 | `			/* Append the raw string verbatim */` |
|     17 | 1033 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      8 | 1034 | `		}` |
|     39 | 1035 | `		if( zIn >= zEnd ){` |
|      3 | 1036 | `			break;` |
|      - | 1037 | `		}` |
|     37 | 1038 | `		c = zIn[0];` |
|     37 | 1039 | `		if( c == '&' ){` |
|      - | 1040 | `			/* Expand '&amp;' */` |
|      9 | 1041 | `			ph7_result_string(pCtx,"&amp;",(int)sizeof("&amp;")-1);` |
|     33 | 1042 | `		}else if( c == '<' ){` |
|      - | 1043 | `			/* Expand '&lt;' */` |
|      7 | 1044 | `			ph7_result_string(pCtx,"&lt;",(int)sizeof("&lt;")-1);` |
|     26 | 1045 | `		}else if( c == '>' ){` |
|      - | 1046 | `			/* Expand '&gt;' */` |
|      9 | 1047 | `			ph7_result_string(pCtx,"&gt;",(int)sizeof("&gt;")-1);` |
|     19 | 1048 | `		}else if( c == '\'' ){` |
|      5 | 1049 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 1050 | `				/* Expand '&#039;' */` |
|      5 | 1051 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      3 | 1052 | `			}else{` |
|      - | 1053 | `				/* Leave the single quote untouched */` |
|    ! 0 | 1054 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      1 | 1055 | `			}` |
|     13 | 1056 | `		}else if( c == '"' ){` |
|     11 | 1057 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      - | 1058 | `				/* Expand '&quot;' */` |
|      7 | 1059 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      4 | 1060 | `			}else{` |
|      - | 1061 | `				/* Leave the double quote untouched */` |
|      5 | 1062 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      - | 1063 | `			}` |
|      5 | 1064 | `		}` |
|      - | 1065 | `		/* Ignore the unsafe HTML character */` |
|     37 | 1066 | `		zIn++;` |
|      1 | 1067 | `	}` |
|     11 | 1068 | `	return PH7_OK;` |
|     11 | 1069 |  |
|      - | 1070 | `/*` |
|      - | 1071 | ` * string htmlspecialchars_decode(string $string[,int $quote_style = ENT_COMPAT ])` |
|      - | 1072 | ` *  Unescape HTML entities.` |
|      - | 1073 | ` * Parameters` |
|      - | 1074 | ` *  $string` |
|      - | 1075 | ` *   The string to decode` |
|      - | 1076 | ` *  $quote_style` |
|      - | 1077 | ` *    The quote style. One of the following constants:` |
|      - | 1078 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone (default)` |
|      - | 1079 | ` *   ENT_QUOTES 	Will convert both double and single quotes` |
|      - | 1080 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted` |
|      - | 1081 | ` * Return` |
|      - | 1082 | ` *  The unescaped string or NULL on failure.` |
|      - | 1083 | ` */` |
|     16 | 1084 | `static int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1085 |  |
|      - | 1086 | `	const char *zCur,*zIn,*zEnd;` |
|     17 | 1087 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 1088 | `	int nLen,nJump;` |
|     17 | 1089 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1090 | `		/* Missing/Invalid arguments,return NULL */` |
|      7 | 1091 | `		ph7_result_null(pCtx);` |
|      7 | 1092 | `		return PH7_OK;` |
|      - | 1093 | `	}` |
|      - | 1094 | `	/* Extract the target string */` |
|     11 | 1095 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 1096 | `	zEnd = &zIn[nLen];` |
|      - | 1097 | `	/* Extract the flags if available */` |
|     11 | 1098 | `	if( nArg > 1 ){` |
|      7 | 1099 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      7 | 1100 | `		if( iFlags < 0 ){` |
|      3 | 1101 | `			iFlags = 0x01;` |
|      1 | 1102 | `		}` |
|      3 | 1103 | `	}` |
|      - | 1104 | `	/* Perform the requested operation */` |
|     15 | 1105 | `	for(;;){` |
|     31 | 1106 | `		if( zIn >= zEnd ){` |
|     11 | 1107 | `			break;` |
|      - | 1108 | `		}` |
|     21 | 1109 | `		zCur = zIn;` |
|     51 | 1110 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|     31 | 1111 | `			zIn++;` |
|      1 | 1112 | `		}` |
|     21 | 1113 | `		if( zCur < zIn ){` |
|      - | 1114 | `			/* Append the raw string verbatim */` |
|      9 | 1115 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 1116 | `		}` |
|     21 | 1117 | `		nLen = (int)(zEnd-zIn);` |
|     21 | 1118 | `		nJump = (int)sizeof(char);` |
|     21 | 1119 | `		if( nLen >= (int)sizeof("&amp;")-1 && SyStrnicmp(zIn,"&amp;",sizeof("&amp;")-1) == 0 ){` |
|      - | 1120 | `			/* &amp; ==> '&' */` |
|      3 | 1121 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|      3 | 1122 | `			nJump = (int)sizeof("&amp;")-1;` |
|     20 | 1123 | `		}else if( nLen >= (int)sizeof("&lt;")-1 && SyStrnicmp(zIn,"&lt;",sizeof("&lt;")-1) == 0 ){` |
|      - | 1124 | `			/* &lt; ==> < */` |
|      3 | 1125 | `			ph7_result_string(pCtx,"<",(int)sizeof(char));` |
|      3 | 1126 | `			nJump = (int)sizeof("&lt;")-1;` |
|     18 | 1127 | `		}else if( nLen >= (int)sizeof("&gt;")-1 && SyStrnicmp(zIn,"&gt;",sizeof("&gt;")-1) == 0 ){` |
|      - | 1128 | `			/* &gt; ==> '>' */` |
|      3 | 1129 | `			ph7_result_string(pCtx,">",(int)sizeof(char));` |
|      3 | 1130 | `			nJump = (int)sizeof("&gt;")-1;` |
|     16 | 1131 | `		}else if( nLen >= (int)sizeof("&quot;")-1 && SyStrnicmp(zIn,"&quot;",sizeof("&quot;")-1) == 0 ){` |
|      - | 1132 | `			/* &quot; ==> '"' */` |
|     13 | 1133 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      9 | 1134 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      5 | 1135 | `			}else{` |
|      - | 1136 | `				/* Leave untouched */` |
|      5 | 1137 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      - | 1138 | `			}` |
|     13 | 1139 | `			nJump = (int)sizeof("&quot;")-1;` |
|      9 | 1140 | `		}else if( nLen >= (int)sizeof("&#039;")-1 && SyStrnicmp(zIn,"&#039;",sizeof("&#039;")-1) == 0 ){` |
|      - | 1141 | `			/* &#039; ==> ''' */` |
|      3 | 1142 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 1143 | `				/* Expand ''' */` |
|      3 | 1144 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      2 | 1145 | `			}else{` |
|      - | 1146 | `				/* Leave untouched */` |
|    ! 0 | 1147 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      - | 1148 | `			}` |
|      3 | 1149 | `			nJump = (int)sizeof("&#039;")-1;` |
|      1 | 1150 | `		}else if( nLen >= (int)sizeof(char) ){` |
|      - | 1151 | `			/* expand '&' */` |
|    ! 0 | 1152 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 1153 | `		}else{` |
|      - | 1154 | `			/* No more input to process */` |
|    ! 0 | 1155 | `			break;` |
|      - | 1156 | `		}` |
|     21 | 1157 | `		zIn += nJump;` |
|      1 | 1158 | `	}` |
|     11 | 1159 | `	return PH7_OK;` |
|      9 | 1160 |  |
|      - | 1161 | `/* HTML encoding/Decoding table` |
|      - | 1162 | ` * Source: Symisc RunTime API.[chm@symisc.net]` |
|      - | 1163 | ` */` |
|      - | 1164 | `static const char *azHtmlEscape[] = {` |
|      - | 1165 | ` 	"&lt;","<","&gt;",">","&amp;","&","&quot;","\"","&#39;","'",` |
|      - | 1166 | `	"&#33;","!","&#36;","$","&#35;","#","&#37;","%","&#40;","(",` |
|      - | 1167 | `	"&#41;",")","&#123;","{","&#125;","}","&#61;","=","&#43;","+",` |
|      - | 1168 | `	"&#63;","?","&#91;","[","&#93;","]","&#64;","@","&#44;",","` |
|      - | 1169 | ` };` |
|      - | 1170 | `/*` |
|      - | 1171 | ` * array get_html_translation_table(void)` |
|      - | 1172 | ` *  Returns the translation table used by htmlspecialchars() and htmlentities().` |
|      - | 1173 | ` * Parameters` |
|      - | 1174 | ` *  None` |
|      - | 1175 | ` * Return` |
|      - | 1176 | ` *  The translation table as an array or NULL on failure.` |
|      - | 1177 | ` */` |
|      4 | 1178 | `static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1179 |  |
|      - | 1180 | `	ph7_value *pArray,*pValue;` |
|      - | 1181 | `	sxu32 n;` |
|      - | 1182 | `	/* Element value */` |
|      5 | 1183 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      5 | 1184 | `	if( pValue == 0 ){` |
|    ! 0 | 1185 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 1186 | `		SXUNUSED(apArg);` |
|      - | 1187 | `		/* Return NULL */` |
|    ! 0 | 1188 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1189 | `		return PH7_OK;` |
|      - | 1190 | `	}` |
|      - | 1191 | `	/* Create a new array */` |
|      5 | 1192 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 | 1193 | `	if( pArray == 0 ){` |
|      - | 1194 | `		/* Return NULL */` |
|    ! 0 | 1195 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1196 | `		return PH7_OK;` |
|      - | 1197 | `	}` |
|      - | 1198 | `	/* Make the table */` |
|     85 | 1199 | `	for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|      - | 1200 | `		/* Prepare the value */` |
|     81 | 1201 | `		ph7_value_string(pValue,azHtmlEscape[n],-1 /* Compute length automatically */);` |
|      - | 1202 | `		/* Insert the value */` |
|     81 | 1203 | `		ph7_array_add_strkey_elem(pArray,azHtmlEscape[n+1],pValue);` |
|      - | 1204 | `		/* Reset the string cursor */` |
|     81 | 1205 | `		ph7_value_reset_string_cursor(pValue);` |
|     41 | 1206 | `	}` |
|      - | 1207 | `	/*` |
|      - | 1208 | `	 * Return the array.` |
|      - | 1209 | `	 * Don't worry about freeing memory, everything will be automatically` |
|      - | 1210 | `	 * released upon we return from this function.` |
|      - | 1211 | `	 */` |
|      5 | 1212 | `	ph7_result_value(pCtx,pArray);` |
|      5 | 1213 | `	return PH7_OK;` |
|      3 | 1214 |  |
|      - | 1215 | `/*` |
|      - | 1216 | ` * string htmlentities( string $string [, int $flags = ENT_COMPAT \| ENT_HTML401]);` |
|      - | 1217 | ` *   Convert all applicable characters to HTML entities` |
|      - | 1218 | ` * Parameters` |
|      - | 1219 | ` * $string` |
|      - | 1220 | ` *   The input string.` |
|      - | 1221 | ` * $flags` |
|      - | 1222 | ` *  A bitmask of one or more of the flags (see block-comment on PH7_builtin_htmlspecialchars())` |
|      - | 1223 | ` * Return` |
|      - | 1224 | ` * The encoded string.` |
|      - | 1225 | ` */` |
|     10 | 1226 | `static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1227 |  |
|     11 | 1228 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 1229 | `	const char *zIn,*zEnd;` |
|      - | 1230 | `	int nLen,c;` |
|      - | 1231 | `	sxu32 n;` |
|     11 | 1232 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1233 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 1234 | `		ph7_result_null(pCtx);` |
|      5 | 1235 | `		return PH7_OK;` |
|      - | 1236 | `	}` |
|      - | 1237 | `	/* Extract the target string */` |
|      7 | 1238 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1239 | `	/* Handle empty string up front */` |
|      7 | 1240 | `	if( nLen == 0 ){` |
|      3 | 1241 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1242 | `		return PH7_OK;` |
|      - | 1243 | `	}` |
|      5 | 1244 | `	zEnd = &zIn[nLen];` |
|      - | 1245 | `	/* Extract the flags if available */` |
|      5 | 1246 | `	if( nArg > 1 ){` |
|      3 | 1247 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      3 | 1248 | `		if( iFlags < 0 ){` |
|      3 | 1249 | `			iFlags = 0x01;` |
|      1 | 1250 | `		}` |
|      1 | 1251 | `	}` |
|      - | 1252 | `	/* Perform the requested operation */` |
|     11 | 1253 | `	for(;;){` |
|     23 | 1254 | `		if( zIn >= zEnd ){` |
|      - | 1255 | `			/* No more input to process */` |
|      5 | 1256 | `			break;` |
|      - | 1257 | `		}` |
|     19 | 1258 | `		c = zIn[0];` |
|      - | 1259 | `		/* Perform a linear lookup on the decoding table */` |
|    233 | 1260 | `		for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    223 | 1261 | `			if( azHtmlEscape[n+1][0] == c ){` |
|      - | 1262 | `				/* Got one */` |
|      9 | 1263 | `				break;` |
|      - | 1264 | `			}` |
|    108 | 1265 | `		}` |
|     19 | 1266 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|      - | 1267 | `			/* Output the safe sequence [i.e: '<' ==> '&lt;"] */` |
|      9 | 1268 | `			if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 1269 | `				/* Expand the double quote verbatim */` |
|    ! 0 | 1270 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      9 | 1271 | `			}else if(c == '\'' && ((iFlags & 0x02 /*ENT_QUOTES*/) == 0 \|\| (iFlags & 0x04) /*ENT_NOQUOTES*/) ){` |
|      - | 1272 | `				/* expand single quote verbatim */` |
|    ! 0 | 1273 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|    ! 0 | 1274 | `			}else{` |
|      9 | 1275 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1/*Compute length automatically */);` |
|      - | 1276 | `			}` |
|      5 | 1277 | `		}else{` |
|      - | 1278 | `			/* Output character verbatim */` |
|     11 | 1279 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 1280 | `		}` |
|     19 | 1281 | `		zIn++;` |
|      1 | 1282 | `	}` |
|      5 | 1283 | `	return PH7_OK;` |
|      6 | 1284 |  |
|      - | 1285 | `/*` |
|      - | 1286 | ` * string html_entity_decode(string $string [, int $quote_style = ENT_COMPAT [, string $charset = 'UTF-8' ]])` |
|      - | 1287 | ` *   Perform the reverse operation of html_entity_decode().` |
|      - | 1288 | ` * Parameters` |
|      - | 1289 | ` * $string` |
|      - | 1290 | ` *   The input string.` |
|      - | 1291 | ` * $flags` |
|      - | 1292 | ` *  A bitmask of one or more of the flags (see comment on PH7_builtin_htmlspecialchars())` |
|      - | 1293 | ` * Return` |
|      - | 1294 | ` * The decoded string.` |
|      - | 1295 | ` */` |
|     28 | 1296 | `static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1297 |  |
|      - | 1298 | `	const char *zCur,*zIn,*zEnd;` |
|     29 | 1299 | `	int iFlags = 0x01; /* ENT_COMPAT  */` |
|      - | 1300 | `	int nLen;` |
|      - | 1301 | `	sxu32 n;` |
|     29 | 1302 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1303 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 1304 | `		ph7_result_null(pCtx);` |
|      5 | 1305 | `		return PH7_OK;` |
|      - | 1306 | `	}` |
|      - | 1307 | `	/* Extract the target string */` |
|     25 | 1308 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 1309 | `	zEnd = &zIn[nLen];` |
|      - | 1310 | `	/* Extract the flags if available */` |
|     25 | 1311 | `	if( nArg > 1 ){` |
|     15 | 1312 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     15 | 1313 | `		if( iFlags < 0 ){` |
|      3 | 1314 | `			iFlags = 0x01;` |
|      1 | 1315 | `		}` |
|      7 | 1316 | `	}` |
|      - | 1317 | `	/* Perform the requested operation */` |
|     27 | 1318 | `	for(;;){` |
|     55 | 1319 | `		if( zIn >= zEnd ){` |
|      - | 1320 | `			/* No more input to process */` |
|     13 | 1321 | `			break;` |
|      - | 1322 | `		}` |
|     43 | 1323 | `		zCur = zIn;` |
|    173 | 1324 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|    131 | 1325 | `			zIn++;` |
|      1 | 1326 | `		}` |
|     43 | 1327 | `		if( zCur < zIn ){` |
|      - | 1328 | `			/* Append raw string verbatim */` |
|     27 | 1329 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     13 | 1330 | `		}` |
|     43 | 1331 | `		if( zIn >= zEnd ){` |
|     13 | 1332 | `			break;` |
|      - | 1333 | `		}` |
|     31 | 1334 | `		nLen = (int)(zEnd-zIn);` |
|      - | 1335 | `		/* Find an encoded sequence */` |
|    113 | 1336 | `		for(n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    113 | 1337 | `			int iLen = (int)SyStrlen(azHtmlEscape[n]);` |
|    113 | 1338 | `			if( nLen >= iLen && SyStrnicmp(zIn,azHtmlEscape[n],(sxu32)iLen) == 0 ){` |
|      - | 1339 | `				/* Got one */` |
|     31 | 1340 | `				zIn += iLen;` |
|     31 | 1341 | `				break;` |
|      - | 1342 | `			}` |
|     42 | 1343 | `		}` |
|     31 | 1344 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|     31 | 1345 | `			int c = azHtmlEscape[n+1][0];` |
|      - | 1346 | `			/* Output the decoded character */` |
|     31 | 1347 | `			if( c == '\'' && ((iFlags & 0x02) == 0 /*ENT_QUOTES*/\|\| (iFlags & 0x04) /*ENT_NOQUOTES*/)  ){` |
|      - | 1348 | `				/* Do not process single quotes */` |
|      9 | 1349 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|     27 | 1350 | `			}else if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 1351 | `				/* Do not process double quotes */` |
|      5 | 1352 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|      3 | 1353 | `			}else{` |
|     19 | 1354 | `				ph7_result_string(pCtx,azHtmlEscape[n+1],-1); /* Compute length automatically */` |
|      - | 1355 | `			}` |
|     16 | 1356 | `		}else{` |
|      - | 1357 | `			/* Append '&' */` |
|    ! 0 | 1358 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 1359 | `			zIn++;` |
|      - | 1360 | `		}` |
|      1 | 1361 | `	}` |
|     25 | 1362 | `	return PH7_OK;` |
|     15 | 1363 |  |
|      - | 1364 | `/*` |
|      - | 1365 | ` * int strlen($string)` |
|      - | 1366 | ` *  return the length of the given string.` |
|      - | 1367 | ` * Parameter` |
|      - | 1368 | ` *  string: The string being measured for length.` |
|      - | 1369 | ` * Return` |
|      - | 1370 | ` *  length of the given string.` |
|      - | 1371 | ` */` |
|   6046 | 1372 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1373 |  |
|   6051 | 1374 | `	int iLen = 0;` |
|   6051 | 1375 | `	if( nArg > 0 ){` |
|   6049 | 1376 | `		ph7_value_to_string(apArg[0],&iLen);` |
|   3022 | 1377 | `	}` |
|      - | 1378 | `	/* String length */` |
|   6051 | 1379 | `	ph7_result_int(pCtx,iLen);` |
|   6051 | 1380 | `	return PH7_OK;` |
|      5 | 1381 |  |
|      - | 1382 | `/*` |
|      - | 1383 | ` * int strcmp(string $str1,string $str2)` |
|      - | 1384 | ` *  Perform a binary safe string comparison.` |
|      - | 1385 | ` * Parameter` |
|      - | 1386 | ` *  str1: The first string` |
|      - | 1387 | ` *  str2: The second string` |
|      - | 1388 | ` * Return` |
|      - | 1389 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1390 | ` *  than str2, and 0 if they are equal.` |
|      - | 1391 | ` */` |
|     80 | 1392 | `static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1393 |  |
|      - | 1394 | `	const char *z1,*z2;` |
|      - | 1395 | `	int n1,n2;` |
|      - | 1396 | `	int res;` |
|     81 | 1397 | `	if( nArg < 2 ){` |
|      5 | 1398 | `		res = nArg == 0 ? 0 : 1;` |
|      5 | 1399 | `		ph7_result_int(pCtx,res);` |
|      5 | 1400 | `		return PH7_OK;` |
|      - | 1401 | `	}` |
|      - | 1402 | `	/* Perform the comparison */` |
|     77 | 1403 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     77 | 1404 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     77 | 1405 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1406 | `	/* Comparison result */` |
|     77 | 1407 | `	ph7_result_int(pCtx,res);` |
|     77 | 1408 | `	return PH7_OK;` |
|     41 | 1409 |  |
|      - | 1410 | `/*` |
|      - | 1411 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 1412 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 1413 | ` * Parameter` |
|      - | 1414 | ` *  str1: The first string` |
|      - | 1415 | ` *  str2: The second string` |
|      - | 1416 | ` * Return` |
|      - | 1417 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1418 | ` *  than str2, and 0 if they are equal.` |
|      - | 1419 | ` */` |
|     20 | 1420 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1421 |  |
|      - | 1422 | `	const char *z1,*z2;` |
|      - | 1423 | `	int res;` |
|      - | 1424 | `	int n;` |
|     21 | 1425 | `	if( nArg < 3 ){` |
|      - | 1426 | `		/* Perform a standard comparison */` |
|      5 | 1427 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 1428 | `	}` |
|      - | 1429 | `	/* Desired comparison length */` |
|     17 | 1430 | `	n  = ph7_value_to_int(apArg[2]);` |
|     17 | 1431 | `	if( n < 0 ){` |
|      - | 1432 | `		/* Invalid length */` |
|      3 | 1433 | `		ph7_result_int(pCtx,-1);` |
|      3 | 1434 | `		return PH7_OK;` |
|      - | 1435 | `	}` |
|      - | 1436 | `	/* Perform the comparison */` |
|     15 | 1437 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     15 | 1438 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     15 | 1439 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 1440 | `	/* Comparison result */` |
|     15 | 1441 | `	ph7_result_int(pCtx,res);` |
|     15 | 1442 | `	return PH7_OK;` |
|     11 | 1443 |  |
|      - | 1444 | `/*` |
|      - | 1445 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 1446 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 1447 | ` * Parameter` |
|      - | 1448 | ` *  str1: The first string` |
|      - | 1449 | ` *  str2: The second string` |
|      - | 1450 | ` * Return` |
|      - | 1451 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1452 | ` *  than str2, and 0 if they are equal.` |
|      - | 1453 | ` */` |
|     22 | 1454 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1455 |  |
|      - | 1456 | `	const char *z1,*z2;` |
|      - | 1457 | `	int n1,n2;` |
|      - | 1458 | `	int res;` |
|     23 | 1459 | `	if( nArg < 2 ){` |
|      9 | 1460 | `		res = nArg == 0 ? 0 : 1;` |
|      9 | 1461 | `		ph7_result_int(pCtx,res);` |
|      9 | 1462 | `		return PH7_OK;` |
|      - | 1463 | `	}` |
|      - | 1464 | `	/* Perform the comparison */` |
|     15 | 1465 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     15 | 1466 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     15 | 1467 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1468 | `	/* Comparison result */` |
|     15 | 1469 | `	ph7_result_int(pCtx,res);` |
|     15 | 1470 | `	return PH7_OK;` |
|     12 | 1471 |  |
|      - | 1472 | `/*` |
|      - | 1473 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 1474 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 1475 | ` * Parameter` |
|      - | 1476 | ` *  $str1: The first string` |
|      - | 1477 | ` *  $str2: The second string` |
|      - | 1478 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 1479 | ` * Return` |
|      - | 1480 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1481 | ` *  than str2, and 0 if they are equal.` |
|      - | 1482 | ` */` |
|      8 | 1483 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1484 |  |
|      - | 1485 | `	const char *z1,*z2;` |
|      - | 1486 | `	int res;` |
|      - | 1487 | `	int n;` |
|      9 | 1488 | `	if( nArg < 3 ){` |
|      - | 1489 | `		/* Perform a standard comparison */` |
|      5 | 1490 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 1491 | `	}` |
|      - | 1492 | `	/* Desired comparison length */` |
|      5 | 1493 | `	n  = ph7_value_to_int(apArg[2]);` |
|      5 | 1494 | `	if( n < 0 ){` |
|      - | 1495 | `		/* Invalid length */` |
|    ! 0 | 1496 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 1497 | `		return PH7_OK;` |
|      - | 1498 | `	}` |
|      - | 1499 | `	/* Perform the comparison */` |
|      5 | 1500 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|      5 | 1501 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|      5 | 1502 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 1503 | `	/* Comparison result */` |
|      5 | 1504 | `	ph7_result_int(pCtx,res);` |
|      5 | 1505 | `	return PH7_OK;` |
|      5 | 1506 |  |
|      - | 1507 | `/*` |
|      - | 1508 | ` * Implode context [i.e: it's private data].` |
|      - | 1509 | ` * A pointer to the following structure is forwarded` |
|      - | 1510 | ` * verbatim to the array walker callback defined below.` |
|      - | 1511 | ` */` |
|      - | 1512 | `struct implode_data {` |
|      - | 1513 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 1514 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 1515 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 1516 | `	int nSeplen;          /* Separator length */` |
|      - | 1517 | `	int bFirst;           /* TRUE if first call */` |
|      - | 1518 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 1519 | `};` |
|      - | 1520 | `/*` |
|      - | 1521 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 1522 | ` * The following routine is invoked for each array entry passed` |
|      - | 1523 | ` * to the implode() function.` |
|      - | 1524 | ` */` |
| 122770 | 1525 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 1526 |  |
|  61385 | 1527 | `	SXUNUSED(pKey);` |
| 122775 | 1528 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 1529 | `	const char *zData;` |
|      - | 1530 | `	int nLen;` |
| 122775 | 1531 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 1532 | `		if( pData->nSeplen > 0 ){` |
|      3 | 1533 | `			if( !pData->bFirst ){` |
|      - | 1534 | `				/* append the separator first */` |
|      3 | 1535 | `				ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|      2 | 1536 | `			}else{` |
|    ! 0 | 1537 | `				pData->bFirst = 0;` |
|      - | 1538 | `			}` |
|      1 | 1539 | `		}` |
|      - | 1540 | `		/* Recurse */` |
|      3 | 1541 | `		pData->bFirst = 1;` |
|      3 | 1542 | `		pData->nRecCount++;` |
|      3 | 1543 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 1544 | `		pData->nRecCount--;` |
|      3 | 1545 | `		return PH7_OK;` |
|      - | 1546 | `	}` |
|      - | 1547 | `	/* Extract the string representation of the entry value */` |
| 122773 | 1548 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 1549 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 122773 | 1550 | `	if( pData->bFirst ){` |
|  30693 | 1551 | `		pData->bFirst = 0;` |
| 107429 | 1552 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 1553 | `		/* append the separator first */` |
|  92073 | 1554 | `		ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|  46034 | 1555 | `	}` |
|      - | 1556 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 122773 | 1557 | `	if( nLen > 0 ){` |
| 111565 | 1558 | `		ph7_result_string(pData->pCtx,zData,nLen);` |
|  55780 | 1559 | `	}` |
| 122773 | 1560 | `	return PH7_OK;` |
|  61390 | 1561 |  |
|      - | 1562 | `/*` |
|      - | 1563 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 1564 | ` * string implode(array $pieces,...)` |
|      - | 1565 | ` *  Join array elements with a string.` |
|      - | 1566 | ` * $glue` |
|      - | 1567 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 1568 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 1569 | ` * $pieces` |
|      - | 1570 | ` *   The array of strings to implode.` |
|      - | 1571 | ` * Return` |
|      - | 1572 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 1573 | ` *  order, with the glue string between each element.` |
|      - | 1574 | ` */` |
|  30714 | 1575 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1576 |  |
|      - | 1577 | `	struct implode_data imp_data;` |
|  30719 | 1578 | `	int i = 1;` |
|  30719 | 1579 | `	if( nArg < 1 ){` |
|      - | 1580 | `		/* Missing argument,return NULL */` |
|    ! 0 | 1581 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1582 | `		return PH7_OK;` |
|      - | 1583 | `	}` |
|      - | 1584 | `	/* Prepare the implode context */` |
|  30719 | 1585 | `	imp_data.pCtx = pCtx;` |
|  30719 | 1586 | `	imp_data.bRecursive = 0;` |
|  30719 | 1587 | `	imp_data.bFirst = 1;` |
|  30719 | 1588 | `	imp_data.nRecCount = 0;` |
|  30719 | 1589 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  30717 | 1590 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  15361 | 1591 | `	}else{` |
|      3 | 1592 | `		imp_data.zSep = 0;` |
|      3 | 1593 | `		imp_data.nSeplen = 0;` |
|      3 | 1594 | `		i = 0;` |
|      - | 1595 | `	}` |
|  30719 | 1596 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 1597 | `	/* Start the 'join' process */` |
|  61433 | 1598 | `	while( i < nArg ){` |
|  30719 | 1599 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1600 | `			/* Iterate throw array entries */` |
|  30719 | 1601 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|  15362 | 1602 | `		}else{` |
|      - | 1603 | `			const char *zData;` |
|      - | 1604 | `			int nLen;` |
|      - | 1605 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 1606 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1607 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 1608 | `			if( imp_data.bFirst ){` |
|    ! 0 | 1609 | `				imp_data.bFirst = 0;` |
|    ! 0 | 1610 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1611 | `				ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen);` |
|    ! 0 | 1612 | `			}` |
|      - | 1613 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 1614 | `			if( nLen > 0 ){` |
|    ! 0 | 1615 | `				ph7_result_string(pCtx,zData,nLen);` |
|    ! 0 | 1616 | `			}` |
|      - | 1617 | `		}` |
|  30719 | 1618 | `		i++;` |
|      5 | 1619 | `	}` |
|  30719 | 1620 | `	return PH7_OK;` |
|  15362 | 1621 |  |
|      - | 1622 | `/*` |
|      - | 1623 | ` * Symisc eXtension:` |
|      - | 1624 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 1625 | ` * Purpose` |
|      - | 1626 | ` *  Same as implode() but recurse on arrays.` |
|      - | 1627 | ` * Example:` |
|      - | 1628 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 1629 | ` *   echo implode_recursive("/",$a);` |
|      - | 1630 | ` *   Will output` |
|      - | 1631 | ` *     usr/home/dean.` |
|      - | 1632 | ` *   While the standard implode would produce.` |
|      - | 1633 | ` *    usr/Array.` |
|      - | 1634 | ` * Parameter` |
|      - | 1635 | ` *  Refer to implode().` |
|      - | 1636 | ` * Return` |
|      - | 1637 | ` *  Refer to implode().` |
|      - | 1638 | ` */` |
|     12 | 1639 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1640 |  |
|      - | 1641 | `	struct implode_data imp_data;` |
|     13 | 1642 | `	int i = 1;` |
|     13 | 1643 | `	if( nArg < 1 ){` |
|      - | 1644 | `		/* Missing argument,return NULL */` |
|      3 | 1645 | `		ph7_result_null(pCtx);` |
|      3 | 1646 | `		return PH7_OK;` |
|      - | 1647 | `	}` |
|      - | 1648 | `	/* Prepare the implode context */` |
|     11 | 1649 | `	imp_data.pCtx = pCtx;` |
|     11 | 1650 | `	imp_data.bRecursive = 1;` |
|     11 | 1651 | `	imp_data.bFirst = 1;` |
|     11 | 1652 | `	imp_data.nRecCount = 0;` |
|     11 | 1653 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 1654 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 1655 | `	}else{` |
|    ! 0 | 1656 | `		imp_data.zSep = 0;` |
|    ! 0 | 1657 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 1658 | `		i = 0;` |
|      - | 1659 | `	}` |
|     11 | 1660 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 1661 | `	/* Start the 'join' process */` |
|     21 | 1662 | `	while( i < nArg ){` |
|     11 | 1663 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1664 | `			/* Iterate throw array entries */` |
|      3 | 1665 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      2 | 1666 | `		}else{` |
|      - | 1667 | `			const char *zData;` |
|      - | 1668 | `			int nLen;` |
|      - | 1669 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 1670 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1671 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 1672 | `			if( imp_data.bFirst ){` |
|      9 | 1673 | `				imp_data.bFirst = 0;` |
|      4 | 1674 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1675 | `				ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen);` |
|    ! 0 | 1676 | `			}` |
|      - | 1677 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 1678 | `			if( nLen > 0 ){` |
|      9 | 1679 | `				ph7_result_string(pCtx,zData,nLen);` |
|      4 | 1680 | `			}` |
|      - | 1681 | `		}` |
|     11 | 1682 | `		i++;` |
|      1 | 1683 | `	}` |
|     11 | 1684 | `	return PH7_OK;` |
|      7 | 1685 |  |
|      - | 1686 | `/*` |
|      - | 1687 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 1688 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 1689 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 1690 | ` * Parameters` |
|      - | 1691 | ` *  $delimiter` |
|      - | 1692 | ` *   The boundary string.` |
|      - | 1693 | ` * $string` |
|      - | 1694 | ` *   The input string.` |
|      - | 1695 | ` * $limit` |
|      - | 1696 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 1697 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 1698 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 1699 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 1700 | ` * Returns` |
|      - | 1701 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 1702 | ` *  on boundaries formed by the delimiter.` |
|      - | 1703 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 1704 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 1705 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 1706 | ` *  will be returned.` |
|      - | 1707 | ` * NOTE:` |
|      - | 1708 | ` *  Negative limit is not supported.` |
|      - | 1709 | ` */` |
|   5778 | 1710 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1711 |  |
|      - | 1712 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 1713 | `	int nDelim,nStrlen,iLimit;` |
|      - | 1714 | `	ph7_value *pArray;` |
|      - | 1715 | `	ph7_value *pValue;` |
|      - | 1716 | `	sxu32 nOfft;` |
|      - | 1717 | `	sxi32 rc;` |
|   5783 | 1718 | `	if( nArg < 2 ){` |
|      - | 1719 | `		/* Missing arguments,return FALSE */` |
|      9 | 1720 | `		ph7_result_bool(pCtx,0);` |
|      9 | 1721 | `		return PH7_OK;` |
|      - | 1722 | `	}` |
|      - | 1723 | `	/* Extract the delimiter */` |
|   5775 | 1724 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   5775 | 1725 | `	if( nDelim < 1 ){` |
|      - | 1726 | `		/* Empty delimiter,return FALSE */` |
|      3 | 1727 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1728 | `		return PH7_OK;` |
|      - | 1729 | `	}` |
|      - | 1730 | `	/* Extract the string */` |
|   5773 | 1731 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   5773 | 1732 | `	if( nStrlen < 1 ){` |
|      - | 1733 | `		/* Empty string: return an array with a single empty element (PHP behavior) */` |
|      3 | 1734 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      3 | 1735 | `		ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      3 | 1736 | `		if( pArrayTmp == 0 \|\| pValueTmp == 0 ){` |
|      - | 1737 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 1738 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 1739 | `			return PH7_OK;` |
|      - | 1740 | `		}` |
|      3 | 1741 | `		ph7_value_string(pValueTmp, "", 0);` |
|      3 | 1742 | `		ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp);` |
|      3 | 1743 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      3 | 1744 | `		return PH7_OK;` |
|      - | 1745 | `	}` |
|      - | 1746 | `	/* Point to the end of the string */` |
|   5771 | 1747 | `	zEnd = &zString[nStrlen];` |
|      - | 1748 | `	/* Create the array */` |
|   5771 | 1749 | `	pArray =  ph7_context_new_array(pCtx);` |
|   5771 | 1750 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   5771 | 1751 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 1752 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 1753 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1754 | `		return PH7_OK;` |
|      - | 1755 | `	}` |
|      - | 1756 | `	/* Set a defualt limit */` |
|   5771 | 1757 | `	iLimit = SXI32_HIGH;` |
|   5771 | 1758 | `	if( nArg > 2 ){` |
|     11 | 1759 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     11 | 1760 | `		 if( iLimit < 0 ){` |
|      3 | 1761 | `			iLimit = -iLimit;` |
|      1 | 1762 | `		}` |
|     11 | 1763 | `		if( iLimit == 0 ){` |
|      3 | 1764 | `			iLimit = 1;` |
|      1 | 1765 | `		}` |
|     11 | 1766 | `		iLimit--;` |
|      5 | 1767 | `	}` |
|      - | 1768 | `	/* Start exploding */` |
|  66182 | 1769 | `	for(;;){` |
| 132369 | 1770 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 132369 | 1771 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 1772 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   5771 | 1773 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   5771 | 1774 | `			ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|   5771 | 1775 | `			break;` |
|      - | 1776 | `		}` |
|      - | 1777 | `		/* Point to the desired offset */` |
| 126603 | 1778 | `		zCur = &zString[nOfft];` |
|      - | 1779 | `		/* Perform the store operation (may be empty) */` |
| 126603 | 1780 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 126603 | 1781 | `		ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|      - | 1782 | `		/* Point beyond the delimiter */` |
| 126603 | 1783 | `		zString = &zCur[nDelim];` |
|      - | 1784 | `		/* Reset the cursor */` |
| 126603 | 1785 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 1786 | `	}` |
|      - | 1787 | `	/* Return the freshly created array */` |
|   5771 | 1788 | `	ph7_result_value(pCtx,pArray);` |
|      - | 1789 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 1790 | `	 * released as soon we return from this foregin function.` |
|      - | 1791 | `	 */` |
|   5771 | 1792 | `	return PH7_OK;` |
|   2894 | 1793 |  |
|      - | 1794 | `/*` |
|      - | 1795 | ` * string trim(string $str[,string $charlist ])` |
|      - | 1796 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 1797 | ` * Parameters` |
|      - | 1798 | ` *  $str` |
|      - | 1799 | ` *   The string that will be trimmed.` |
|      - | 1800 | ` * $charlist` |
|      - | 1801 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1802 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1803 | ` *   With .. you can specify a range of characters.` |
|      - | 1804 | ` * Returns.` |
|      - | 1805 | ` *  Thr processed string.` |
|      - | 1806 | ` * NOTE:` |
|      - | 1807 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 1808 | ` */` |
|  13228 | 1809 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1810 |  |
|      - | 1811 | `	const char *zString;` |
|      - | 1812 | `	int nLen;` |
|  13233 | 1813 | `	if( nArg < 1 ){` |
|      - | 1814 | `		/* Missing arguments,return null */` |
|      3 | 1815 | `		ph7_result_null(pCtx);` |
|      3 | 1816 | `		return PH7_OK;` |
|      - | 1817 | `	}` |
|      - | 1818 | `	/* Extract the target string */` |
|  13231 | 1819 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  13231 | 1820 | `	if( nLen < 1 ){` |
|      - | 1821 | `		/* Empty string,return */` |
|   1691 | 1822 | `		ph7_result_string(pCtx,"",0);` |
|   1691 | 1823 | `		return PH7_OK;` |
|      - | 1824 | `	}` |
|      - | 1825 | `	/* Start the trim process */` |
|  11545 | 1826 | `	if( nArg < 2 ){` |
|      - | 1827 | `		SyString sStr;` |
|      - | 1828 | `		/* Remove white spaces and NUL bytes */` |
|  11541 | 1829 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  28155 | 1830 | `		SyStringFullTrimSafe(&sStr);` |
|  11541 | 1831 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   5773 | 1832 | `	}else{` |
|      - | 1833 | `		/* Char list */` |
|      - | 1834 | `		const char *zList;` |
|      - | 1835 | `		int nListlen;` |
|      5 | 1836 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 1837 | `		if( nListlen < 1 ){` |
|      - | 1838 | `			/* Return the string unchanged */` |
|      3 | 1839 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 1840 | `		}else{` |
|      3 | 1841 | `			const char *zEnd = &zString[nLen];` |
|      3 | 1842 | `			const char *zCur = zString;` |
|      - | 1843 | `			const char *zPtr;` |
|      - | 1844 | `			int i;` |
|      - | 1845 | `			/* Left trim */` |
|      4 | 1846 | `			for(;;){` |
|      9 | 1847 | `				if( zCur >= zEnd ){` |
|    ! 0 | 1848 | `					break;` |
|      - | 1849 | `				}` |
|      9 | 1850 | `				zPtr = zCur;` |
|     17 | 1851 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 1852 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|      7 | 1853 | `						zCur++;` |
|      3 | 1854 | `					}` |
|      5 | 1855 | `				}` |
|      9 | 1856 | `				if( zCur == zPtr ){` |
|      - | 1857 | `					/* No match,break immediately */` |
|      3 | 1858 | `					break;` |
|      - | 1859 | `				}` |
|      1 | 1860 | `			}` |
|      - | 1861 | `			/* Right trim */` |
|      3 | 1862 | `			zEnd--;` |
|      4 | 1863 | `			for(;;){` |
|      9 | 1864 | `				if( zEnd <= zCur ){` |
|    ! 0 | 1865 | `					break;` |
|      - | 1866 | `				}` |
|      9 | 1867 | `				zPtr = zEnd;` |
|     17 | 1868 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 1869 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      7 | 1870 | `						zEnd--;` |
|      3 | 1871 | `					}` |
|      5 | 1872 | `				}` |
|      9 | 1873 | `				if( zEnd == zPtr ){` |
|      3 | 1874 | `					break;` |
|      - | 1875 | `				}` |
|      1 | 1876 | `			}` |
|      3 | 1877 | `			if( zCur >= zEnd ){` |
|      - | 1878 | `				/* Return the empty string */` |
|    ! 0 | 1879 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1880 | `			}else{` |
|      3 | 1881 | `				zEnd++;` |
|      3 | 1882 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 1883 | `			}` |
|      - | 1884 | `		}` |
|      - | 1885 | `	}` |
|  11545 | 1886 | `	return PH7_OK;` |
|   6619 | 1887 |  |
|      - | 1888 | `/*` |
|      - | 1889 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 1890 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 1891 | ` * Parameters` |
|      - | 1892 | ` *  $str` |
|      - | 1893 | ` *   The string that will be trimmed.` |
|      - | 1894 | ` * $charlist` |
|      - | 1895 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1896 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1897 | ` *   With .. you can specify a range of characters.` |
|      - | 1898 | ` * Returns.` |
|      - | 1899 | ` *  Thr processed string.` |
|      - | 1900 | ` * NOTE:` |
|      - | 1901 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 1902 | ` */` |
|     26 | 1903 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1904 |  |
|      - | 1905 | `	const char *zString;` |
|      - | 1906 | `	int nLen;` |
|     27 | 1907 | `	if( nArg < 1 ){` |
|      - | 1908 | `		/* Missing arguments,return null */` |
|      3 | 1909 | `		ph7_result_null(pCtx);` |
|      3 | 1910 | `		return PH7_OK;` |
|      - | 1911 | `	}` |
|      - | 1912 | `	/* Extract the target string */` |
|     25 | 1913 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 1914 | `	if( nLen < 1 ){` |
|      - | 1915 | `		/* Empty string,return */` |
|      5 | 1916 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1917 | `		return PH7_OK;` |
|      - | 1918 | `	}` |
|      - | 1919 | `	/* Start the trim process */` |
|     21 | 1920 | `	if( nArg < 2 ){` |
|      - | 1921 | `		SyString sStr;` |
|      - | 1922 | `		/* Remove white spaces and NUL bytes*/` |
|     17 | 1923 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     41 | 1924 | `		SyStringRightTrimSafe(&sStr);` |
|     17 | 1925 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      9 | 1926 | `	}else{` |
|      - | 1927 | `		/* Char list */` |
|      - | 1928 | `		const char *zList;` |
|      - | 1929 | `		int nListlen;` |
|      5 | 1930 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 1931 | `		if( nListlen < 1 ){` |
|      - | 1932 | `			/* Return the string unchanged */` |
|    ! 0 | 1933 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 1934 | `		}else{` |
|      5 | 1935 | `			const char *zEnd = &zString[nLen - 1];` |
|      5 | 1936 | `			const char *zCur = zString;` |
|      - | 1937 | `			const char *zPtr;` |
|      - | 1938 | `			int i;` |
|      - | 1939 | `			/* Right trim */` |
|      6 | 1940 | `			for(;;){` |
|     13 | 1941 | `				if( zEnd <= zCur ){` |
|    ! 0 | 1942 | `					break;` |
|      - | 1943 | `				}` |
|     13 | 1944 | `				zPtr = zEnd;` |
|     25 | 1945 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     13 | 1946 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      9 | 1947 | `						zEnd--;` |
|      4 | 1948 | `					}` |
|      7 | 1949 | `				}` |
|     13 | 1950 | `				if( zEnd == zPtr ){` |
|      5 | 1951 | `					break;` |
|      - | 1952 | `				}` |
|      1 | 1953 | `			}` |
|      5 | 1954 | `			if( zEnd <= zCur ){` |
|      - | 1955 | `				/* Return the empty string */` |
|    ! 0 | 1956 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1957 | `			}else{` |
|      5 | 1958 | `				zEnd++;` |
|      5 | 1959 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 1960 | `			}` |
|      - | 1961 | `		}` |
|      - | 1962 | `	}` |
|     21 | 1963 | `	return PH7_OK;` |
|     14 | 1964 |  |
|      - | 1965 | `/*` |
|      - | 1966 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 1967 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 1968 | ` * Parameters` |
|      - | 1969 | ` *  $str` |
|      - | 1970 | ` *   The string that will be trimmed.` |
|      - | 1971 | ` * $charlist` |
|      - | 1972 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1973 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1974 | ` *   With .. you can specify a range of characters.` |
|      - | 1975 | ` * Returns.` |
|      - | 1976 | ` *  Thr processed string.` |
|      - | 1977 | ` * NOTE:` |
|      - | 1978 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 1979 | ` */` |
|     12 | 1980 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1981 |  |
|      - | 1982 | `	const char *zString;` |
|      - | 1983 | `	int nLen;` |
|     13 | 1984 | `	if( nArg < 1 ){` |
|      - | 1985 | `		/* Missing arguments,return null */` |
|      3 | 1986 | `		ph7_result_null(pCtx);` |
|      3 | 1987 | `		return PH7_OK;` |
|      - | 1988 | `	}` |
|      - | 1989 | `	/* Extract the target string */` |
|     11 | 1990 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 1991 | `	if( nLen < 1 ){` |
|      - | 1992 | `		/* Empty string,return */` |
|    ! 0 | 1993 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1994 | `		return PH7_OK;` |
|      - | 1995 | `	}` |
|      - | 1996 | `	/* Start the trim process */` |
|     11 | 1997 | `	if( nArg < 2 ){` |
|      - | 1998 | `		SyString sStr;` |
|      - | 1999 | `		/* Remove white spaces and NUL byte */` |
|      3 | 2000 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|      8 | 2001 | `		SyStringLeftTrimSafe(&sStr);` |
|      3 | 2002 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      2 | 2003 | `	}else{` |
|      - | 2004 | `		/* Char list */` |
|      - | 2005 | `		const char *zList;` |
|      - | 2006 | `		int nListlen;` |
|      9 | 2007 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      9 | 2008 | `		if( nListlen < 1 ){` |
|      - | 2009 | `			/* Return the string unchanged */` |
|      3 | 2010 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2011 | `		}else{` |
|      7 | 2012 | `			const char *zEnd = &zString[nLen];` |
|      7 | 2013 | `			const char *zCur = zString;` |
|      - | 2014 | `			const char *zPtr;` |
|      - | 2015 | `			int i;` |
|      - | 2016 | `			/* Left trim */` |
|      7 | 2017 | `			for(;;){` |
|     15 | 2018 | `				if( zCur >= zEnd ){` |
|    ! 0 | 2019 | `					break;` |
|      - | 2020 | `				}` |
|     15 | 2021 | `				zPtr = zCur;` |
|     41 | 2022 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     27 | 2023 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|     13 | 2024 | `						zCur++;` |
|      6 | 2025 | `					}` |
|     14 | 2026 | `				}` |
|     15 | 2027 | `				if( zCur == zPtr ){` |
|      - | 2028 | `					/* No match,break immediately */` |
|      7 | 2029 | `					break;` |
|      - | 2030 | `				}` |
|      1 | 2031 | `			}` |
|      7 | 2032 | `			if( zCur >= zEnd ){` |
|      - | 2033 | `				/* Return the empty string */` |
|    ! 0 | 2034 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2035 | `			}else{` |
|      7 | 2036 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2037 | `			}` |
|      - | 2038 | `		}` |
|      - | 2039 | `	}` |
|     11 | 2040 | `	return PH7_OK;` |
|      7 | 2041 |  |
|      - | 2042 | `/*` |
|      - | 2043 | ` * string strtolower(string $str)` |
|      - | 2044 | ` *  Make a string lowercase.` |
|      - | 2045 | ` * Parameters` |
|      - | 2046 | ` *  $str` |
|      - | 2047 | ` *   The input string.` |
|      - | 2048 | ` * Returns.` |
|      - | 2049 | ` *  The lowercased string.` |
|      - | 2050 | ` */` |
|  30380 | 2051 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2052 |  |
|      - | 2053 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2054 | `	int nLen;` |
|  30385 | 2055 | `	if( nArg < 1 ){` |
|      - | 2056 | `		/* Missing arguments,return null */` |
|      3 | 2057 | `		ph7_result_null(pCtx);` |
|      3 | 2058 | `		return PH7_OK;` |
|      - | 2059 | `	}` |
|      - | 2060 | `	/* Extract the target string */` |
|  30383 | 2061 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  30383 | 2062 | `	if( nLen < 1 ){` |
|      - | 2063 | `		/* Empty string,return */` |
|      3 | 2064 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2065 | `		return PH7_OK;` |
|      - | 2066 | `	}` |
|      - | 2067 | `	/* Perform the requested operation */` |
|  30381 | 2068 | `	zEnd = &zString[nLen];` |
|  95738 | 2069 | `	for(;;){` |
| 191481 | 2070 | `		if( zString >= zEnd ){` |
|      - | 2071 | `			/* No more input,break immediately */` |
|  30381 | 2072 | `			break;` |
|      - | 2073 | `		}` |
| 161105 | 2074 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2075 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2076 | `			zCur = zString;` |
|    ! 0 | 2077 | `			zString++;` |
|    ! 0 | 2078 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2079 | `				zString++;` |
|    ! 0 | 2080 | `			}` |
|      - | 2081 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2082 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2083 | `		}else{` |
| 161105 | 2084 | `			int c = zString[0];` |
| 161105 | 2085 | `			if( SyisUpper(c) ){` |
| 161103 | 2086 | `				c = SyToLower(zString[0]);` |
|  80549 | 2087 | `			}` |
|      - | 2088 | `			/* Append character */` |
| 161105 | 2089 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2090 | `			/* Advance the cursor */` |
| 161105 | 2091 | `			zString++;` |
|      - | 2092 | `		}` |
|      5 | 2093 | `	}` |
|  30381 | 2094 | `	return PH7_OK;` |
|  15195 | 2095 |  |
|      - | 2096 | `/*` |
|      - | 2097 | ` * string strtolower(string $str)` |
|      - | 2098 | ` *  Make a string uppercase.` |
|      - | 2099 | ` * Parameters` |
|      - | 2100 | ` *  $str` |
|      - | 2101 | ` *   The input string.` |
|      - | 2102 | ` * Returns.` |
|      - | 2103 | ` *  The uppercased string.` |
|      - | 2104 | ` */` |
|     34 | 2105 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2106 |  |
|      - | 2107 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2108 | `	int nLen;` |
|     39 | 2109 | `	if( nArg < 1 ){` |
|      - | 2110 | `		/* Missing arguments,return null */` |
|      3 | 2111 | `		ph7_result_null(pCtx);` |
|      3 | 2112 | `		return PH7_OK;` |
|      - | 2113 | `	}` |
|      - | 2114 | `	/* Extract the target string */` |
|     37 | 2115 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     37 | 2116 | `	if( nLen < 1 ){` |
|      - | 2117 | `		/* Empty string,return */` |
|      3 | 2118 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2119 | `		return PH7_OK;` |
|      - | 2120 | `	}` |
|      - | 2121 | `	/* Perform the requested operation */` |
|     35 | 2122 | `	zEnd = &zString[nLen];` |
|     88 | 2123 | `	for(;;){` |
|    181 | 2124 | `		if( zString >= zEnd ){` |
|      - | 2125 | `			/* No more input,break immediately */` |
|     35 | 2126 | `			break;` |
|      - | 2127 | `		}` |
|    151 | 2128 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2129 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2130 | `			zCur = zString;` |
|    ! 0 | 2131 | `			zString++;` |
|    ! 0 | 2132 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2133 | `				zString++;` |
|    ! 0 | 2134 | `			}` |
|      - | 2135 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2136 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2137 | `		}else{` |
|    151 | 2138 | `			int c = zString[0];` |
|    151 | 2139 | `			if( SyisLower(c) ){` |
|    145 | 2140 | `				c = SyToUpper(zString[0]);` |
|     70 | 2141 | `			}` |
|      - | 2142 | `			/* Append character */` |
|    151 | 2143 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2144 | `			/* Advance the cursor */` |
|    151 | 2145 | `			zString++;` |
|      - | 2146 | `		}` |
|      5 | 2147 | `	}` |
|     35 | 2148 | `	return PH7_OK;` |
|     22 | 2149 |  |
|      - | 2150 | `/*` |
|      - | 2151 | ` * string ucfirst(string $str)` |
|      - | 2152 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2153 | ` *  character is alphabetic.` |
|      - | 2154 | ` * Parameters` |
|      - | 2155 | ` *  $str` |
|      - | 2156 | ` *   The input string.` |
|      - | 2157 | ` * Returns.` |
|      - | 2158 | ` *  The processed string.` |
|      - | 2159 | ` */` |
|      6 | 2160 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2161 |  |
|      - | 2162 | `	const char *zString,*zEnd;` |
|      - | 2163 | `	int nLen,c;` |
|      7 | 2164 | `	if( nArg < 1 ){` |
|      - | 2165 | `		/* Missing arguments,return null */` |
|      3 | 2166 | `		ph7_result_null(pCtx);` |
|      3 | 2167 | `		return PH7_OK;` |
|      - | 2168 | `	}` |
|      - | 2169 | `	/* Extract the target string */` |
|      5 | 2170 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2171 | `	if( nLen < 1 ){` |
|      - | 2172 | `		/* Empty string,return */` |
|      3 | 2173 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2174 | `		return PH7_OK;` |
|      - | 2175 | `	}` |
|      - | 2176 | `	/* Perform the requested operation */` |
|      3 | 2177 | `	zEnd = &zString[nLen];` |
|      3 | 2178 | `	c = zString[0];` |
|      3 | 2179 | `	if( SyisLower(c) ){` |
|      3 | 2180 | `		c = SyToUpper(c);` |
|      1 | 2181 | `	}` |
|      - | 2182 | `	/* Append the first character */` |
|      3 | 2183 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2184 | `	zString++;` |
|      3 | 2185 | `	if( zString < zEnd ){` |
|      - | 2186 | `		/* Append the rest of the input verbatim */` |
|      3 | 2187 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2188 | `	}` |
|      3 | 2189 | `	return PH7_OK;` |
|      4 | 2190 |  |
|      - | 2191 | `/*` |
|      - | 2192 | ` * string lcfirst(string $str)` |
|      - | 2193 | ` *  Make a string's first character lowercase.` |
|      - | 2194 | ` * Parameters` |
|      - | 2195 | ` *  $str` |
|      - | 2196 | ` *   The input string.` |
|      - | 2197 | ` * Returns.` |
|      - | 2198 | ` *  The processed string.` |
|      - | 2199 | ` */` |
|      6 | 2200 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2201 |  |
|      - | 2202 | `	const char *zString,*zEnd;` |
|      - | 2203 | `	int nLen,c;` |
|      7 | 2204 | `	if( nArg < 1 ){` |
|      - | 2205 | `		/* Missing arguments,return null */` |
|      3 | 2206 | `		ph7_result_null(pCtx);` |
|      3 | 2207 | `		return PH7_OK;` |
|      - | 2208 | `	}` |
|      - | 2209 | `	/* Extract the target string */` |
|      5 | 2210 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2211 | `	if( nLen < 1 ){` |
|      - | 2212 | `		/* Empty string,return */` |
|      3 | 2213 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2214 | `		return PH7_OK;` |
|      - | 2215 | `	}` |
|      - | 2216 | `	/* Perform the requested operation */` |
|      3 | 2217 | `	zEnd = &zString[nLen];` |
|      3 | 2218 | `	c = zString[0];` |
|      3 | 2219 | `	if( SyisUpper(c) ){` |
|      3 | 2220 | `		c = SyToLower(c);` |
|      1 | 2221 | `	}` |
|      - | 2222 | `	/* Append the first character */` |
|      3 | 2223 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2224 | `	zString++;` |
|      3 | 2225 | `	if( zString < zEnd ){` |
|      - | 2226 | `		/* Append the rest of the input verbatim */` |
|      3 | 2227 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2228 | `	}` |
|      3 | 2229 | `	return PH7_OK;` |
|      4 | 2230 |  |
|      - | 2231 | `/*` |
|      - | 2232 | ` * int ord(string $string)` |
|      - | 2233 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 2234 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 2235 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 2236 | ` * Parameters` |
|      - | 2237 | ` *  $string` |
|      - | 2238 | ` *   The input string.` |
|      - | 2239 | ` * Returns` |
|      - | 2240 | ` *  The ASCII value as an integer.` |
|      - | 2241 | ` */` |
|     62 | 2242 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2243 |  |
|      - | 2244 | `	const char *zString;` |
|      - | 2245 | `	int nLen,c;` |
|      - | 2246 | `	/* PHP requires exactly one argument. */` |
|     65 | 2247 | `	if( nArg != 1 ){` |
|      8 | 2248 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2249 | `			"ArgumentCountError",` |
|      - | 2250 | `			"ord() expects exactly 1 argument, %d given",` |
|      2 | 2251 | `			nArg` |
|      - | 2252 | `			);` |
|      - | 2253 | `	}` |
|      - | 2254 | `	/* Passing null is deprecated (E_DEPRECATED).  PHP emits this before` |
|      - | 2255 | `	 * the empty-string deprecation, so we check null first. */` |
|     59 | 2256 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 2257 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2258 | `			"ord(): Passing null to parameter #1 ($character) "` |
|      - | 2259 | `			"of type string is deprecated"` |
|      - | 2260 | `			);` |
|      1 | 2261 | `	}` |
|      - | 2262 | `	/* Extract the target string */` |
|     59 | 2263 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     59 | 2264 | `	if( nLen < 1 ){` |
|      - | 2265 | `		/* Empty string is deprecated (E_DEPRECATED). */` |
|      5 | 2266 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2267 | `			"ord(): Providing an empty string is deprecated"` |
|      - | 2268 | `			);` |
|      5 | 2269 | `		ph7_result_int(pCtx,0);` |
|      5 | 2270 | `		return PH7_OK;` |
|      - | 2271 | `	}` |
|      - | 2272 | `	/* A string longer than one byte is deprecated (E_DEPRECATED). */` |
|     55 | 2273 | `	if( nLen > 1 ){` |
|      7 | 2274 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2275 | `			"ord(): Providing a string that is not one byte long "` |
|      - | 2276 | `			"is deprecated. Use ord($str[0]) instead"` |
|      - | 2277 | `			);` |
|      3 | 2278 | `	}` |
|      - | 2279 | `	/* Extract the ASCII value of the first character */` |
|     55 | 2280 | `	c = (unsigned char)zString[0];` |
|      - | 2281 | `	/* Return that value */` |
|     55 | 2282 | `	ph7_result_int(pCtx,c);` |
|     55 | 2283 | `	return PH7_OK;` |
|     34 | 2284 |  |
|      - | 2285 | `/*` |
|      - | 2286 | ` * string chr(int $codepoint)` |
|      - | 2287 | ` *  Returns a one-character string containing the character specified` |
|      - | 2288 | ` *  by the given codepoint.  Any integer is accepted; values outside` |
|      - | 2289 | ` *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.` |
|      - | 2290 | ` * Parameters` |
|      - | 2291 | ` *  $codepoint` |
|      - | 2292 | ` *   An integer codepoint.  Values outside 0-255 are deprecated and` |
|      - | 2293 | ` *   will be constrained to a single byte.` |
|      - | 2294 | ` * Returns` |
|      - | 2295 | ` *  A single-character string.` |
|      - | 2296 | ` */` |
|     44 | 2297 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2298 |  |
|      - | 2299 | `	int c;` |
|      - | 2300 | `	unsigned char ch;` |
|      - | 2301 | `	/* PHP requires exactly one argument. */` |
|     47 | 2302 | `	if( nArg != 1 ){` |
|      8 | 2303 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2304 | `			"ArgumentCountError",` |
|      - | 2305 | `			"chr() expects exactly 1 argument, %d given",` |
|      2 | 2306 | `			nArg` |
|      - | 2307 | `			);` |
|      - | 2308 | `	}` |
|      - | 2309 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 2310 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 2311 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 2312 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|     41 | 2313 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      - | 2314 | `		char zBuf[120];` |
|      4 | 2315 | `		SyBufferFormat(zBuf,sizeof(zBuf),` |
|      - | 2316 | `			"Implicit conversion from float %g to int loses precision",` |
|      1 | 2317 | `			ph7_value_to_double(apArg[0])` |
|      - | 2318 | `			);` |
|      3 | 2319 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zBuf);` |
|      1 | 2320 | `	}` |
|      - | 2321 | `	/* Extract the codepoint. */` |
|     41 | 2322 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 2323 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 2324 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 2325 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 2326 | `	 * name to avoid the API double-prefixing it. */` |
|     41 | 2327 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 2328 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 2329 | `			E_DEPRECATED,` |
|      - | 2330 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 2331 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 2332 | `			"The value used will be constrained using % 256"` |
|      - | 2333 | `			);` |
|      2 | 2334 | `	}` |
|      - | 2335 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 2336 | `	 * when taking the address of a wider int. */` |
|     41 | 2337 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 2338 | `	/* Return the specified character */` |
|     41 | 2339 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|     41 | 2340 | `	return PH7_OK;` |
|     25 | 2341 |  |
|      - | 2342 | `/*` |
|      - | 2343 | ` * Binary to hex consumer callback.` |
|      - | 2344 | ` * This callback is the default consumer used by the hash functions` |
|      - | 2345 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 2346 | ` */` |
|   2330 | 2347 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 2348 |  |
|      - | 2349 | `	/* Append hex chunk verbatim */` |
|   2331 | 2350 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   2331 | 2351 | `	return SXRET_OK;` |
|      1 | 2352 |  |
|      - | 2353 |  |
|      - | 2354 | `/*` |
|      - | 2355 | ` * string bin2hex(string $str)` |
|      - | 2356 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 2357 | ` * Parameters` |
|      - | 2358 | ` *  $str` |
|      - | 2359 | ` *   The input string.` |
|      - | 2360 | ` * Returns.` |
|      - | 2361 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 2362 | ` */` |
|     24 | 2363 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2364 |  |
|      - | 2365 | `	const char *zString;` |
|      - | 2366 | `	int nLen;` |
|      - | 2367 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|     29 | 2368 | `	if( nArg != 1 ){` |
|      8 | 2369 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2370 | `			"ArgumentCountError",` |
|      - | 2371 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|      2 | 2372 | `			nArg` |
|      - | 2373 | `			);` |
|      - | 2374 | `	}` |
|      - | 2375 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 2376 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 2377 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 2378 | `	 */` |
|     33 | 2379 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|     16 | 2380 | `		( ph7_value_is_object(apArg[0]) &&` |
|      3 | 2381 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|      2 | 2382 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|      1 | 2383 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 2384 | `		)` |
|      - | 2385 | `	){` |
|      9 | 2386 | `		const char *zType = ph7_type_name(apArg[0]);` |
|      9 | 2387 | `		if( ph7_value_is_object(apArg[0]) ){` |
|      3 | 2388 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      3 | 2389 | `			if( pInst && pInst->pClass ){` |
|      3 | 2390 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      1 | 2391 | `			}` |
|      1 | 2392 | `		}` |
|     12 | 2393 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2394 | `			"TypeError",` |
|      - | 2395 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|      3 | 2396 | `			zType` |
|      - | 2397 | `			);` |
|      - | 2398 | `	}` |
|      - | 2399 | `	/* Extract the target string */` |
|     15 | 2400 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 2401 | `	if( nLen < 1 ){` |
|      - | 2402 | `		/* Empty string,return */` |
|      3 | 2403 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2404 | `		return PH7_OK;` |
|      - | 2405 | `	}` |
|      - | 2406 | `	/* Perform the requested operation */` |
|     13 | 2407 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|     13 | 2408 | `	return PH7_OK;` |
|     17 | 2409 |  |
|      - | 2410 |  |
|      - | 2411 | `/* Search callback signature */` |
|      - | 2412 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 2413 | `/*` |
|      - | 2414 | ` * Case-insensitive pattern match.` |
|      - | 2415 | ` * Brute force is the default search method used here.` |
|      - | 2416 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 2417 | ` * well for short/medium texts on modern hardware.` |
|      - | 2418 | ` */` |
|    118 | 2419 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      1 | 2420 |  |
|    119 | 2421 | `	const char *zpIn = (const char *)pPattern;` |
|    119 | 2422 | `	const char *zIn = (const char *)pText;` |
|    119 | 2423 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    119 | 2424 | `	const char *zEnd = &zIn[nLen];` |
|      - | 2425 | `	const char *zPtr,*zPtr2;` |
|      - | 2426 | `	int c,d;` |
|    119 | 2427 | `	if( iPatLen > nLen ){` |
|      - | 2428 | `		/* Don't bother processing */` |
|     33 | 2429 | `		return SXERR_NOTFOUND;` |
|      - | 2430 | `	}` |
|    244 | 2431 | `	for(;;){` |
|    489 | 2432 | `		if( zIn >= zEnd ){` |
|     47 | 2433 | `			break;` |
|      - | 2434 | `		}` |
|    443 | 2435 | `		c = SyToLower(zIn[0]);` |
|    443 | 2436 | `		d = SyToLower(zpIn[0]);` |
|    443 | 2437 | `		if( c == d ){` |
|     41 | 2438 | `			zPtr   = &zIn[1];` |
|     41 | 2439 | `			zPtr2  = &zpIn[1];` |
|     71 | 2440 | `			for(;;){` |
|    143 | 2441 | `				if( zPtr2 >= zpEnd ){` |
|      - | 2442 | `					/* Pattern found */` |
|     41 | 2443 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 2444 | `					return SXRET_OK;` |
|      - | 2445 | `				}` |
|    103 | 2446 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 2447 | `					break;` |
|      - | 2448 | `				}` |
|    103 | 2449 | `				c = SyToLower(zPtr[0]);` |
|    103 | 2450 | `				d = SyToLower(zPtr2[0]);` |
|    103 | 2451 | `				if( c != d ){` |
|    ! 0 | 2452 | `					break;` |
|      - | 2453 | `				}` |
|    103 | 2454 | `				zPtr++; zPtr2++;` |
|      1 | 2455 | `			}` |
|    ! 0 | 2456 | `		}` |
|    403 | 2457 | `		zIn++;` |
|      1 | 2458 | `	}` |
|      - | 2459 | `	/* Pattern not found */` |
|     47 | 2460 | `	return SXERR_NOTFOUND;` |
|     60 | 2461 |  |
|      - | 2462 | `/*` |
|      - | 2463 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2464 | ` *  Find the first occurrence of a string.` |
|      - | 2465 | ` * Parameters` |
|      - | 2466 | ` *  $haystack` |
|      - | 2467 | ` *   The input string.` |
|      - | 2468 | ` * $needle` |
|      - | 2469 | ` *   Search pattern (must be a string).` |
|      - | 2470 | ` * $before_needle` |
|      - | 2471 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2472 | ` *   of the needle (excluding the needle).` |
|      - | 2473 | ` * Return` |
|      - | 2474 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2475 | ` */` |
|     10 | 2476 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2477 |  |
|     11 | 2478 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2479 | `	const char *zBlob,*zPattern;` |
|      - | 2480 | `	int nLen,nPatLen;` |
|      - | 2481 | `	sxu32 nOfft;` |
|      - | 2482 | `	sxi32 rc;` |
|     11 | 2483 | `	if( nArg < 2 ){` |
|      - | 2484 | `		/* Missing arguments,return FALSE */` |
|      5 | 2485 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2486 | `		return PH7_OK;` |
|      - | 2487 | `	}` |
|      - | 2488 | `	/* Extract the needle and the haystack */` |
|      7 | 2489 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 2490 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 2491 | `	nOfft = 0; /* cc warning */` |
|      9 | 2492 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 2493 | `		int before = 0;` |
|      - | 2494 | `		/* Perform the lookup */` |
|      5 | 2495 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2496 | `		if( rc != SXRET_OK ){` |
|      - | 2497 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2498 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2499 | `			return PH7_OK;` |
|      - | 2500 | `		}` |
|      - | 2501 | `		/* Return the portion of the string */` |
|      5 | 2502 | `		if( nArg > 2 ){` |
|      3 | 2503 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2504 | `		}` |
|      5 | 2505 | `		if( before ){` |
|      3 | 2506 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2507 | `		}else{` |
|      3 | 2508 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2509 | `		}` |
|      3 | 2510 | `	}else{` |
|      3 | 2511 | `		ph7_result_bool(pCtx,0);` |
|      - | 2512 | `	}` |
|      7 | 2513 | `	return PH7_OK;` |
|      6 | 2514 |  |
|      - | 2515 | `/*` |
|      - | 2516 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2517 | ` *  Case-insensitive strstr().` |
|      - | 2518 | ` * Parameters` |
|      - | 2519 | ` *  $haystack` |
|      - | 2520 | ` *   The input string.` |
|      - | 2521 | ` * $needle` |
|      - | 2522 | ` *   Search pattern (must be a string).` |
|      - | 2523 | ` * $before_needle` |
|      - | 2524 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2525 | ` *   of the needle (excluding the needle).` |
|      - | 2526 | ` * Return` |
|      - | 2527 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2528 | ` */` |
|      6 | 2529 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2530 |  |
|      7 | 2531 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2532 | `	const char *zBlob,*zPattern;` |
|      - | 2533 | `	int nLen,nPatLen;` |
|      - | 2534 | `	sxu32 nOfft;` |
|      - | 2535 | `	sxi32 rc;` |
|      7 | 2536 | `	if( nArg < 2 ){` |
|      - | 2537 | `		/* Missing arguments,return FALSE */` |
|      3 | 2538 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2539 | `		return PH7_OK;` |
|      - | 2540 | `	}` |
|      - | 2541 | `	/* Extract the needle and the haystack */` |
|      5 | 2542 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2543 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 2544 | `	nOfft = 0; /* cc warning */` |
|      7 | 2545 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 2546 | `		int before = 0;` |
|      - | 2547 | `		/* Perform the lookup */` |
|      5 | 2548 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2549 | `		if( rc != SXRET_OK ){` |
|      - | 2550 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2551 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2552 | `			return PH7_OK;` |
|      - | 2553 | `		}` |
|      - | 2554 | `		/* Return the portion of the string */` |
|      5 | 2555 | `		if( nArg > 2 ){` |
|      3 | 2556 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2557 | `		}` |
|      5 | 2558 | `		if( before ){` |
|      3 | 2559 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2560 | `		}else{` |
|      3 | 2561 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2562 | `		}` |
|      3 | 2563 | `	}else{` |
|    ! 0 | 2564 | `		ph7_result_bool(pCtx,0);` |
|      - | 2565 | `	}` |
|      5 | 2566 | `	return PH7_OK;` |
|      4 | 2567 |  |
|      - | 2568 | `/*` |
|      - | 2569 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2570 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 2571 | ` * Parameters` |
|      - | 2572 | ` *  $haystack` |
|      - | 2573 | ` *   The input string.` |
|      - | 2574 | ` * $needle` |
|      - | 2575 | ` *   Search pattern (must be a string).` |
|      - | 2576 | ` * $offset` |
|      - | 2577 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 2578 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 2579 | ` *   of haystack.` |
|      - | 2580 | ` * Return` |
|      - | 2581 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 2582 | ` */` |
|    122 | 2583 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2584 |  |
|    127 | 2585 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2586 | `	const char *zBlob,*zPattern;` |
|      - | 2587 | `	int nLen,nPatLen,nStart;` |
|      - | 2588 | `	sxu32 nOfft;` |
|      - | 2589 | `	sxi32 rc;` |
|    127 | 2590 | `	if( nArg < 2 ){` |
|      - | 2591 | `		/* Missing arguments,return FALSE */` |
|      7 | 2592 | `		ph7_result_bool(pCtx,0);` |
|      7 | 2593 | `		return PH7_OK;` |
|      - | 2594 | `	}` |
|      - | 2595 | `	/* Extract the needle and the haystack */` |
|    121 | 2596 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    121 | 2597 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    121 | 2598 | `	nOfft = 0; /* cc warning */` |
|    121 | 2599 | `	nStart = 0;` |
|      - | 2600 | `	/* Peek the starting offset if available */` |
|    121 | 2601 | `	if( nArg > 2 ){` |
|    ! 0 | 2602 | `		nStart = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 2603 | `		if( nStart < 0 ){` |
|    ! 0 | 2604 | `			nStart = -nStart;` |
|    ! 0 | 2605 | `		}` |
|    ! 0 | 2606 | `		if( nStart >= nLen ){` |
|      - | 2607 | `			/* Invalid offset */` |
|    ! 0 | 2608 | `			nStart = 0;` |
|    ! 0 | 2609 | `		}else{` |
|    ! 0 | 2610 | `			zBlob += nStart;` |
|    ! 0 | 2611 | `			nLen -= nStart;` |
|      - | 2612 | `		}` |
|    ! 0 | 2613 | `	}` |
|    121 | 2614 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2615 | `		/* Perform the lookup */` |
|    119 | 2616 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    119 | 2617 | `		if( rc != SXRET_OK ){` |
|      - | 2618 | `			/* Pattern not found,return FALSE */` |
|     33 | 2619 | `			ph7_result_bool(pCtx,0);` |
|     33 | 2620 | `			return PH7_OK;` |
|      - | 2621 | `		}` |
|      - | 2622 | `		/* Return the pattern position */` |
|     88 | 2623 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|     45 | 2624 | `	}else{` |
|      3 | 2625 | `		ph7_result_bool(pCtx,0);` |
|      - | 2626 | `	}` |
|     90 | 2627 | `	return PH7_OK;` |
|     66 | 2628 |  |
|      - | 2629 | `/*` |
|      - | 2630 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|      - | 2631 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|      - | 2632 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|      - | 2633 | ` * TypeError for arrays, resources, and objects without __toString.` |
|      - | 2634 | ` *` |
|      - | 2635 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|      - | 2636 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|      - | 2637 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|      - | 2638 | ` *` |
|      - | 2639 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|      - | 2640 | ` * is valid until pTmp is released or pArg is mutated.` |
|      - | 2641 | ` */` |
|    386 | 2642 | `static sxi32 StrPredicateResolveArg(` |
|      - | 2643 | `	ph7_context *pCtx,` |
|      - | 2644 | `	ph7_value *pArg,` |
|      - | 2645 | `	const char *zFunc,` |
|      - | 2646 | `	int iArgNum,` |
|      - | 2647 | `	const char *zParamName,` |
|      - | 2648 | `	const char *zNullMsg,` |
|      - | 2649 | `	ph7_value *pTmp,` |
|      - | 2650 | `	const char **pzOut,` |
|      - | 2651 | `	int *pnOut` |
|      4 | 2652 | `){` |
|    390 | 2653 | `	if( ph7_value_is_null(pArg) ){` |
|     13 | 2654 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zNullMsg);` |
|     13 | 2655 | `		*pzOut = "";` |
|     13 | 2656 | `		*pnOut = 0;` |
|     13 | 2657 | `		return PH7_OK;` |
|      - | 2658 | `	}` |
|    580 | 2659 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|    356 | 2660 | `	    ( ph7_value_is_object(pArg) &&` |
|     72 | 2661 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|     48 | 2662 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|     24 | 2663 | `	        "__toString",sizeof("__toString")-1) == 0` |
|      - | 2664 | `	    )` |
|      - | 2665 | `	){` |
|     34 | 2666 | `		const char *zType = ph7_type_name(pArg);` |
|     34 | 2667 | `		if( ph7_value_is_object(pArg) ){` |
|     13 | 2668 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     13 | 2669 | `			if( pInst && pInst->pClass ){` |
|     13 | 2670 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      6 | 2671 | `			}` |
|      6 | 2672 | `		}` |
|     49 | 2673 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2674 | `			"TypeError",` |
|      - | 2675 | `			"%s(): Argument #%d (%s) must be of type string, %s given",` |
|     15 | 2676 | `			zFunc, iArgNum, zParamName, zType` |
|      - | 2677 | `			);` |
|      - | 2678 | `	}` |
|    345 | 2679 | `	if( ph7_value_is_object(pArg) ){` |
|     37 | 2680 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     37 | 2681 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 2682 | `			"__toString",sizeof("__toString")-1);` |
|     37 | 2683 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|     37 | 2684 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|     37 | 2685 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|     37 | 2686 | `		return PH7_OK;` |
|      - | 2687 | `	}` |
|    309 | 2688 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|    309 | 2689 | `	return PH7_OK;` |
|    197 | 2690 |  |
|      - | 2691 | `/*` |
|      - | 2692 | ` * bool str_contains(string $haystack, string $needle)` |
|      - | 2693 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|      - | 2694 | ` * Return` |
|      - | 2695 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|      - | 2696 | ` */` |
|     76 | 2697 | `static int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2698 |  |
|      - | 2699 | `	const char *zHaystack,*zNeedle;` |
|      - | 2700 | `	int nHayLen,nNeedleLen;` |
|      - | 2701 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2702 | `	sxi32 rc;` |
|     80 | 2703 | `	if( nArg != 2 ){` |
|     18 | 2704 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2705 | `			"ArgumentCountError",` |
|      - | 2706 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|      5 | 2707 | `			nArg` |
|      - | 2708 | `			);` |
|      - | 2709 | `	}` |
|     68 | 2710 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     68 | 2711 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     68 | 2712 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack",` |
|      - | 2713 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|      - | 2714 | `		"of type string is deprecated",` |
|      - | 2715 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     68 | 2716 | `	if( rc != PH7_OK ) goto out;` |
|     61 | 2717 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle",` |
|      - | 2718 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|      - | 2719 | `		"of type string is deprecated",` |
|      - | 2720 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     61 | 2721 | `	if( rc != PH7_OK ) goto out;` |
|     57 | 2722 | `	if( nNeedleLen < 1 ){` |
|     13 | 2723 | `		ph7_result_bool(pCtx,1);` |
|     51 | 2724 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2725 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2726 | `	}else{` |
|     55 | 2727 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|     18 | 2728 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|     37 | 2729 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|      - | 2730 | `	}` |
|     57 | 2731 | `	rc = PH7_OK;` |
|     33 | 2732 | `out:` |
|     68 | 2733 | `	PH7_MemObjRelease(&sHayTmp);` |
|     68 | 2734 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     68 | 2735 | `	return rc;` |
|     42 | 2736 |  |
|      - | 2737 | `/*` |
|      - | 2738 | ` * bool str_starts_with(string $haystack, string $needle)` |
|      - | 2739 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|      - | 2740 | ` * Return` |
|      - | 2741 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|      - | 2742 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 2743 | ` */` |
|     78 | 2744 | `static int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2745 |  |
|      - | 2746 | `	const char *zHaystack,*zNeedle;` |
|      - | 2747 | `	int nHayLen,nNeedleLen;` |
|      - | 2748 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2749 | `	sxi32 rc;` |
|     82 | 2750 | `	if( nArg != 2 ){` |
|     18 | 2751 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2752 | `			"ArgumentCountError",` |
|      - | 2753 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|      5 | 2754 | `			nArg` |
|      - | 2755 | `			);` |
|      - | 2756 | `	}` |
|     70 | 2757 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 2758 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 2759 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack",` |
|      - | 2760 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 2761 | `		"of type string is deprecated",` |
|      - | 2762 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 2763 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 2764 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle",` |
|      - | 2765 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|      - | 2766 | `		"of type string is deprecated",` |
|      - | 2767 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 2768 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 2769 | `	if( nNeedleLen < 1 ){` |
|     13 | 2770 | `		ph7_result_bool(pCtx,1);` |
|     53 | 2771 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2772 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2773 | `	}else{` |
|     58 | 2774 | `		ph7_result_bool(pCtx,` |
|     38 | 2775 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 2776 | `	}` |
|     59 | 2777 | `	rc = PH7_OK;` |
|     34 | 2778 | `out:` |
|     70 | 2779 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 2780 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 2781 | `	return rc;` |
|     43 | 2782 |  |
|      - | 2783 | `/*` |
|      - | 2784 | ` * bool str_ends_with(string $haystack, string $needle)` |
|      - | 2785 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|      - | 2786 | ` * Return` |
|      - | 2787 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|      - | 2788 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 2789 | ` */` |
|     78 | 2790 | `static int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2791 |  |
|      - | 2792 | `	const char *zHaystack,*zNeedle;` |
|      - | 2793 | `	int nHayLen,nNeedleLen;` |
|      - | 2794 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2795 | `	sxi32 rc;` |
|     82 | 2796 | `	if( nArg != 2 ){` |
|     18 | 2797 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2798 | `			"ArgumentCountError",` |
|      - | 2799 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|      5 | 2800 | `			nArg` |
|      - | 2801 | `			);` |
|      - | 2802 | `	}` |
|     70 | 2803 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 2804 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 2805 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack",` |
|      - | 2806 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 2807 | `		"of type string is deprecated",` |
|      - | 2808 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 2809 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 2810 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle",` |
|      - | 2811 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|      - | 2812 | `		"of type string is deprecated",` |
|      - | 2813 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 2814 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 2815 | `	if( nNeedleLen < 1 ){` |
|     13 | 2816 | `		ph7_result_bool(pCtx,1);` |
|     53 | 2817 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2818 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2819 | `	}else{` |
|     58 | 2820 | `		ph7_result_bool(pCtx,` |
|     38 | 2821 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 2822 | `	}` |
|     59 | 2823 | `	rc = PH7_OK;` |
|     34 | 2824 | `out:` |
|     70 | 2825 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 2826 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 2827 | `	return rc;` |
|     43 | 2828 |  |
|      - | 2829 | `/*` |
|      - | 2830 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2831 | ` *  Case-insensitive strpos.` |
|      - | 2832 | ` * Parameters` |
|      - | 2833 | ` *  $haystack` |
|      - | 2834 | ` *   The input string.` |
|      - | 2835 | ` * $needle` |
|      - | 2836 | ` *   Search pattern (must be a string).` |
|      - | 2837 | ` * $offset` |
|      - | 2838 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 2839 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 2840 | ` *   of haystack.` |
|      - | 2841 | ` * Return` |
|      - | 2842 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 2843 | ` */` |
|     18 | 2844 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2845 |  |
|     19 | 2846 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2847 | `	const char *zBlob,*zPattern;` |
|      - | 2848 | `	int nLen,nPatLen,nStart;` |
|      - | 2849 | `	sxu32 nOfft;` |
|      - | 2850 | `	sxi32 rc;` |
|     19 | 2851 | `	if( nArg < 2 ){` |
|      - | 2852 | `		/* Missing arguments,return FALSE */` |
|      3 | 2853 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2854 | `		return PH7_OK;` |
|      - | 2855 | `	}` |
|      - | 2856 | `	/* Extract the needle and the haystack */` |
|     17 | 2857 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 2858 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     17 | 2859 | `	nOfft = 0; /* cc warning */` |
|     17 | 2860 | `	nStart = 0;` |
|      - | 2861 | `	/* Peek the starting offset if available */` |
|     17 | 2862 | `	if( nArg > 2 ){` |
|      5 | 2863 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 2864 | `		if( nStart < 0 ){` |
|      3 | 2865 | `			nStart = -nStart;` |
|      1 | 2866 | `		}` |
|      5 | 2867 | `		if( nStart >= nLen ){` |
|      - | 2868 | `			/* Invalid offset */` |
|    ! 0 | 2869 | `			nStart = 0;` |
|    ! 0 | 2870 | `		}else{` |
|      5 | 2871 | `			zBlob += nStart;` |
|      5 | 2872 | `			nLen -= nStart;` |
|      - | 2873 | `		}` |
|      2 | 2874 | `	}` |
|     17 | 2875 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2876 | `		/* Perform the lookup */` |
|     17 | 2877 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     17 | 2878 | `		if( rc != SXRET_OK ){` |
|      - | 2879 | `			/* Pattern not found,return FALSE */` |
|      3 | 2880 | `			ph7_result_bool(pCtx,0);` |
|      3 | 2881 | `			return PH7_OK;` |
|      - | 2882 | `		}` |
|      - | 2883 | `		/* Return the pattern position */` |
|     15 | 2884 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 2885 | `	}else{` |
|    ! 0 | 2886 | `		ph7_result_bool(pCtx,0);` |
|      - | 2887 | `	}` |
|     15 | 2888 | `	return PH7_OK;` |
|     10 | 2889 |  |
|      - | 2890 | `/*` |
|      - | 2891 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2892 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 2893 | ` * Parameters` |
|      - | 2894 | ` *  $haystack` |
|      - | 2895 | ` *   The input string.` |
|      - | 2896 | ` * $needle` |
|      - | 2897 | ` *   Search pattern (must be a string).` |
|      - | 2898 | ` * $offset` |
|      - | 2899 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 2900 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 2901 | ` *   characters from the end of the string, searching backwards.` |
|      - | 2902 | ` * Return` |
|      - | 2903 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 2904 | ` */` |
|     32 | 2905 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2906 |  |
|      - | 2907 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     33 | 2908 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2909 | `	int nLen,nPatLen;` |
|      - | 2910 | `	sxu32 nOfft;` |
|      - | 2911 | `	sxi32 rc;` |
|     33 | 2912 | `	if( nArg < 2 ){` |
|      - | 2913 | `		/* Missing arguments,return FALSE */` |
|      3 | 2914 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2915 | `		return PH7_OK;` |
|      - | 2916 | `	}` |
|      - | 2917 | `	/* Extract the needle and the haystack */` |
|     31 | 2918 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 2919 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 2920 | `	/* Point to the end of the pattern */` |
|     31 | 2921 | `	zPtr = &zBlob[nLen - 1];` |
|     31 | 2922 | `	zEnd = &zBlob[nLen];` |
|      - | 2923 | `	/* Save the starting posistion */` |
|     31 | 2924 | `	zStart = zBlob;` |
|     31 | 2925 | `	nOfft = 0; /* cc warning */` |
|      - | 2926 | `	/* Peek the starting offset if available */` |
|     31 | 2927 | `	if( nArg > 2 ){` |
|      - | 2928 | `		int nStart;` |
|     21 | 2929 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 2930 | `		if( nStart < 0 ){` |
|     11 | 2931 | `			nStart = -nStart;` |
|     11 | 2932 | `			if( nStart >= nLen ){` |
|      - | 2933 | `				/* Invalid offset */` |
|      3 | 2934 | `				ph7_result_bool(pCtx,0);` |
|      3 | 2935 | `				return PH7_OK;` |
|    ! 0 | 2936 | `			}else{` |
|      9 | 2937 | `				nLen -= nStart;` |
|      9 | 2938 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 2939 | `				zEnd = &zBlob[nLen];` |
|      - | 2940 | `			}` |
|      5 | 2941 | `		}else{` |
|     11 | 2942 | `			if( nStart >= nLen ){` |
|      - | 2943 | `				/* Invalid offset */` |
|      5 | 2944 | `				ph7_result_bool(pCtx,0);` |
|      5 | 2945 | `				return PH7_OK;` |
|    ! 0 | 2946 | `			}else{` |
|      7 | 2947 | `				zBlob += nStart;` |
|      7 | 2948 | `				nLen -= nStart;` |
|      - | 2949 | `			}` |
|      - | 2950 | `		}` |
|      7 | 2951 | `	}` |
|     25 | 2952 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2953 | `		/* Perform the lookup */` |
|     57 | 2954 | `		for(;;){` |
|    115 | 2955 | `			if( zBlob >= zPtr ){` |
|     11 | 2956 | `				break;` |
|      - | 2957 | `			}` |
|    105 | 2958 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    105 | 2959 | `			if( rc == SXRET_OK ){` |
|      - | 2960 | `				/* Pattern found,return it's position */` |
|     13 | 2961 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 2962 | `				return PH7_OK;` |
|      - | 2963 | `			}` |
|     93 | 2964 | `			zPtr--;` |
|      1 | 2965 | `		}` |
|      - | 2966 | `		/* Pattern not found,return FALSE */` |
|     11 | 2967 | `		ph7_result_bool(pCtx,0);` |
|      6 | 2968 | `	}else{` |
|      3 | 2969 | `		ph7_result_bool(pCtx,0);` |
|      - | 2970 | `	}` |
|     13 | 2971 | `	return PH7_OK;` |
|     17 | 2972 |  |
|      - | 2973 | `/*` |
|      - | 2974 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2975 | ` *  Case-insensitive strrpos.` |
|      - | 2976 | ` * Parameters` |
|      - | 2977 | ` *  $haystack` |
|      - | 2978 | ` *   The input string.` |
|      - | 2979 | ` * $needle` |
|      - | 2980 | ` *   Search pattern (must be a string).` |
|      - | 2981 | ` * $offset` |
|      - | 2982 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 2983 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 2984 | ` *   characters from the end of the string, searching backwards.` |
|      - | 2985 | ` * Return` |
|      - | 2986 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 2987 | ` */` |
|     28 | 2988 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2989 |  |
|      - | 2990 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     29 | 2991 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2992 | `	int nLen,nPatLen;` |
|      - | 2993 | `	sxu32 nOfft;` |
|      - | 2994 | `	sxi32 rc;` |
|     29 | 2995 | `	if( nArg < 2 ){` |
|      - | 2996 | `		/* Missing arguments,return FALSE */` |
|      3 | 2997 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2998 | `		return PH7_OK;` |
|      - | 2999 | `	}` |
|      - | 3000 | `	/* Extract the needle and the haystack */` |
|     27 | 3001 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 3002 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3003 | `	/* Point to the end of the pattern */` |
|     27 | 3004 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 3005 | `	zEnd = &zBlob[nLen];` |
|      - | 3006 | `	/* Save the starting posistion */` |
|     27 | 3007 | `	zStart = zBlob;` |
|     27 | 3008 | `	nOfft = 0; /* cc warning */` |
|      - | 3009 | `	/* Peek the starting offset if available */` |
|     27 | 3010 | `	if( nArg > 2 ){` |
|      - | 3011 | `		int nStart;` |
|     15 | 3012 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3013 | `		if( nStart < 0 ){` |
|      7 | 3014 | `			nStart = -nStart;` |
|      7 | 3015 | `			if( nStart >= nLen ){` |
|      - | 3016 | `				/* Invalid offset */` |
|      3 | 3017 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3018 | `				return PH7_OK;` |
|    ! 0 | 3019 | `			}else{` |
|      5 | 3020 | `				nLen -= nStart;` |
|      5 | 3021 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 3022 | `				zEnd = &zBlob[nLen];` |
|      - | 3023 | `			}` |
|      3 | 3024 | `		}else{` |
|      9 | 3025 | `			if( nStart >= nLen ){` |
|      - | 3026 | `				/* Invalid offset */` |
|      5 | 3027 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3028 | `				return PH7_OK;` |
|    ! 0 | 3029 | `			}else{` |
|      5 | 3030 | `				zBlob += nStart;` |
|      5 | 3031 | `				nLen -= nStart;` |
|      - | 3032 | `			}` |
|      - | 3033 | `		}` |
|      4 | 3034 | `	}` |
|     21 | 3035 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3036 | `		/* Perform the lookup */` |
|     44 | 3037 | `		for(;;){` |
|     89 | 3038 | `			if( zBlob >= zPtr ){` |
|      9 | 3039 | `				break;` |
|      - | 3040 | `			}` |
|     81 | 3041 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 3042 | `			if( rc == SXRET_OK ){` |
|      - | 3043 | `				/* Pattern found,return it's position */` |
|     11 | 3044 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 3045 | `				return PH7_OK;` |
|      - | 3046 | `			}` |
|     71 | 3047 | `			zPtr--;` |
|      1 | 3048 | `		}` |
|      - | 3049 | `		/* Pattern not found,return FALSE */` |
|      9 | 3050 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3051 | `	}else{` |
|      3 | 3052 | `		ph7_result_bool(pCtx,0);` |
|      - | 3053 | `	}` |
|     11 | 3054 | `	return PH7_OK;` |
|     15 | 3055 |  |
|      - | 3056 | `/*` |
|      - | 3057 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3058 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3059 | ` * Parameters` |
|      - | 3060 | ` *  $haystack` |
|      - | 3061 | ` *   The input string.` |
|      - | 3062 | ` * $needle` |
|      - | 3063 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3064 | ` *  This behavior is different from that of strstr().` |
|      - | 3065 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3066 | ` *  as the ordinal value of a character.` |
|      - | 3067 | ` * Return` |
|      - | 3068 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3069 | ` */` |
|     24 | 3070 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3071 |  |
|      - | 3072 | `	const char *zBlob;` |
|      - | 3073 | `	int nLen,c;` |
|     25 | 3074 | `	if( nArg < 2 ){` |
|      - | 3075 | `		/* Missing arguments,return FALSE */` |
|      3 | 3076 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3077 | `		return PH7_OK;` |
|      - | 3078 | `	}` |
|      - | 3079 | `	/* Extract the haystack */` |
|     23 | 3080 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3081 | `	c = 0; /* cc warning */` |
|     23 | 3082 | `	if( nLen > 0 ){` |
|      - | 3083 | `		sxu32 nOfft;` |
|      - | 3084 | `		sxi32 rc;` |
|     21 | 3085 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 3086 | `			const char *zPattern;` |
|     11 | 3087 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 3088 | `														 * for NULL pointer.` |
|      - | 3089 | `														 */` |
|     11 | 3090 | `			c = zPattern[0];` |
|      6 | 3091 | `		}else{` |
|      - | 3092 | `			/* Int cast */` |
|     11 | 3093 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 3094 | `		}` |
|      - | 3095 | `		/* Perform the lookup */` |
|     21 | 3096 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3097 | `		if( rc != SXRET_OK ){` |
|      - | 3098 | `			/* No such entry,return FALSE */` |
|      7 | 3099 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3100 | `			return PH7_OK;` |
|      - | 3101 | `		}` |
|      - | 3102 | `		/* Return the string portion */` |
|     15 | 3103 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 3104 | `	}else{` |
|      3 | 3105 | `		ph7_result_bool(pCtx,0);` |
|      - | 3106 | `	}` |
|     17 | 3107 | `	return PH7_OK;` |
|     13 | 3108 |  |
|      - | 3109 | `/*` |
|      - | 3110 | ` * string strrev(string $string)` |
|      - | 3111 | ` *  Reverse a string.` |
|      - | 3112 | ` * Parameters` |
|      - | 3113 | ` *  $string` |
|      - | 3114 | ` *   String to be reversed.` |
|      - | 3115 | ` * Return` |
|      - | 3116 | ` *  The reversed string.` |
|      - | 3117 | ` */` |
|      4 | 3118 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3119 |  |
|      - | 3120 | `	const char *zIn,*zEnd;` |
|      - | 3121 | `	int nLen,c;` |
|      5 | 3122 | `	if( nArg < 1 ){` |
|      - | 3123 | `		/* Missing arguments,return NULL */` |
|      3 | 3124 | `		ph7_result_null(pCtx);` |
|      3 | 3125 | `		return PH7_OK;` |
|      - | 3126 | `	}` |
|      - | 3127 | `	/* Extract the target string */` |
|      3 | 3128 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3129 | `	if( nLen < 1 ){` |
|      - | 3130 | `		/* Empty string Return null */` |
|    ! 0 | 3131 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3132 | `		return PH7_OK;` |
|      - | 3133 | `	}` |
|      - | 3134 | `	/* Perform the requested operation */` |
|      3 | 3135 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3136 | `	for(;;){` |
|      9 | 3137 | `		if( zEnd < zIn ){` |
|      - | 3138 | `			/* No more input to process */` |
|      3 | 3139 | `			break;` |
|      - | 3140 | `		}` |
|      - | 3141 | `		/* Append current character */` |
|      7 | 3142 | `		c = zEnd[0];` |
|      7 | 3143 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3144 | `		zEnd--;` |
|      1 | 3145 | `	}` |
|      3 | 3146 | `	return PH7_OK;` |
|      3 | 3147 |  |
|      - | 3148 | `/*` |
|      - | 3149 | ` * string ucwords(string $string)` |
|      - | 3150 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3151 | ` *  The definition of a word is any string of characters that is immediately after` |
|      - | 3152 | ` *  a whitespace (These are: space, form-feed, newline, carriage return, horizontal tab, and vertical tab).` |
|      - | 3153 | ` * Parameters` |
|      - | 3154 | ` *  $string` |
|      - | 3155 | ` *   The input string.` |
|      - | 3156 | ` * Return` |
|      - | 3157 | ` *  The modified string..` |
|      - | 3158 | ` */` |
|     14 | 3159 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3160 |  |
|      - | 3161 | `	const char *zIn,*zCur,*zEnd;` |
|      - | 3162 | `	int nLen,c;` |
|     15 | 3163 | `	if( nArg < 1 ){` |
|      - | 3164 | `		/* Missing arguments,return NULL */` |
|      3 | 3165 | `		ph7_result_null(pCtx);` |
|      3 | 3166 | `		return PH7_OK;` |
|      - | 3167 | `	}` |
|      - | 3168 | `	/* Extract the target string */` |
|     13 | 3169 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 3170 | `	if( nLen < 1 ){` |
|      - | 3171 | `		/* Empty string – match PHP semantics */` |
|      3 | 3172 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3173 | `		return PH7_OK;` |
|      - | 3174 | `	}` |
|      - | 3175 | `	/* Perform the requested operation */` |
|     11 | 3176 | `	zEnd = &zIn[nLen];` |
|     21 | 3177 | `	for(;;){` |
|      - | 3178 | `		/* Jump leading white spaces */` |
|     43 | 3179 | `		zCur = zIn;` |
|     65 | 3180 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0x80 && SyisSpace(zIn[0]) ){` |
|     23 | 3181 | `			zIn++;` |
|      1 | 3182 | `		}` |
|     43 | 3183 | `		if( zCur < zIn ){` |
|      - | 3184 | `			/* Append white space stream */` |
|     23 | 3185 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 3186 | `		}` |
|     43 | 3187 | `		if( zIn >= zEnd ){` |
|      - | 3188 | `			/* No more input to process */` |
|     11 | 3189 | `			break;` |
|      - | 3190 | `		}` |
|     33 | 3191 | `		c = zIn[0];` |
|     33 | 3192 | `		if( c < 0x80 && SyisLower(c) ){` |
|     29 | 3193 | `			c = SyToUpper(c);` |
|     14 | 3194 | `		}` |
|      - | 3195 | `		/* Append the upper-cased character */` |
|     33 | 3196 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     33 | 3197 | `		zIn++;` |
|     33 | 3198 | `		zCur = zIn;` |
|      - | 3199 | `		/* Append the word varbatim */` |
|    149 | 3200 | `		while( zIn < zEnd ){` |
|    139 | 3201 | `			if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 3202 | `				/* UTF-8 stream */` |
|    ! 0 | 3203 | `				zIn++;` |
|    ! 0 | 3204 | `				SX_JMP_UTF8(zIn,zEnd);` |
|    138 | 3205 | `			}else if( !SyisSpace(zIn[0]) ){` |
|    117 | 3206 | `				zIn++;` |
|     59 | 3207 | `			}else{` |
|     23 | 3208 | `				break;` |
|      - | 3209 | `			}` |
|      1 | 3210 | `		}` |
|     33 | 3211 | `		if( zCur < zIn ){` |
|     33 | 3212 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 | 3213 | `		}` |
|      1 | 3214 | `	}` |
|     11 | 3215 | `	return PH7_OK;` |
|      8 | 3216 |  |
|      - | 3217 | `/*` |
|      - | 3218 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3219 | ` *  Returns input repeated multiplier times.` |
|      - | 3220 | ` * Parameters` |
|      - | 3221 | ` *  $string` |
|      - | 3222 | ` *   String to be repeated.` |
|      - | 3223 | ` * $multiplier` |
|      - | 3224 | ` *  Number of time the input string should be repeated.` |
|      - | 3225 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3226 | ` *  to 0, the function will return an empty string.` |
|      - | 3227 | ` * Return` |
|      - | 3228 | ` *  The repeated string.` |
|      - | 3229 | ` */` |
|  20226 | 3230 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3231 |  |
|      - | 3232 | `	const char *zIn;` |
|      - | 3233 | `	int nLen,nMul;` |
|      - | 3234 | `	int rc;` |
|  20227 | 3235 | `	if( nArg < 2 ){` |
|      - | 3236 | `		/* Missing arguments,return NULL */` |
|      3 | 3237 | `		ph7_result_null(pCtx);` |
|      3 | 3238 | `		return PH7_OK;` |
|      - | 3239 | `	}` |
|      - | 3240 | `	/* Extract the target string */` |
|  20225 | 3241 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|  20225 | 3242 | `	if( nLen < 1 ){` |
|      - | 3243 | `		/* Empty string.Return null */` |
|    ! 0 | 3244 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3245 | `		return PH7_OK;` |
|      - | 3246 | `	}` |
|      - | 3247 | `	/* Extract the multiplier */` |
|  20225 | 3248 | `	nMul = ph7_value_to_int(apArg[1]);` |
|  20225 | 3249 | `	if( nMul < 1 ){` |
|      - | 3250 | `		/* Return the empty string */` |
|      3 | 3251 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3252 | `		return PH7_OK;` |
|      - | 3253 | `	}` |
|      - | 3254 | `	/* Perform the requested operation */` |
| 120878 | 3255 | `	for(;;){` |
| 241757 | 3256 | `		if( !nMul ){` |
|  20223 | 3257 | `			break;` |
|      - | 3258 | `		}` |
|      - | 3259 | `		/* Append the copy */` |
| 221535 | 3260 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 221535 | 3261 | `		if( rc != PH7_OK ){` |
|      - | 3262 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 3263 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 3264 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 3265 | `		}` |
| 221535 | 3266 | `		nMul--;` |
|      1 | 3267 | `	}` |
|  20223 | 3268 | `	return PH7_OK;` |
|  10114 | 3269 |  |
|      - | 3270 | `/*` |
|      - | 3271 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3272 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3273 | ` * Parameters` |
|      - | 3274 | ` *  $string` |
|      - | 3275 | ` *   The input string.` |
|      - | 3276 | ` * $is_xhtml` |
|      - | 3277 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3278 | ` * Return` |
|      - | 3279 | ` *  The processed string.` |
|      - | 3280 | ` */` |
|      6 | 3281 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3282 |  |
|      - | 3283 | `	const char *zIn,*zCur,*zEnd;` |
|      7 | 3284 | `	int is_xhtml = 1; /* Default to XHTML-style '<br/>' like PHP */` |
|      - | 3285 | `	int nLen;` |
|      7 | 3286 | `	if( nArg < 1 ){` |
|      - | 3287 | `		/* Missing arguments,return the empty string */` |
|      3 | 3288 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3289 | `		return PH7_OK;` |
|      - | 3290 | `	}` |
|      - | 3291 | `	/* Extract the target string */` |
|      5 | 3292 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3293 | `	if( nLen < 1 ){` |
|      - | 3294 | `		/* Empty string,return null */` |
|    ! 0 | 3295 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3296 | `		return PH7_OK;` |
|      - | 3297 | `	}` |
|      5 | 3298 | `	if( nArg > 1 ){` |
|      3 | 3299 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3300 | `	}` |
|      5 | 3301 | `	zEnd = &zIn[nLen];` |
|      - | 3302 | `	/* Perform the requested operation */` |
|      4 | 3303 | `	for(;;){` |
|      9 | 3304 | `		zCur = zIn;` |
|      - | 3305 | `		/* Delimit the string */` |
|     21 | 3306 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3307 | `			zIn++;` |
|      1 | 3308 | `		}` |
|      9 | 3309 | `		if( zCur < zIn ){` |
|      - | 3310 | `			/* Output chunk verbatim */` |
|      9 | 3311 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3312 | `		}` |
|      9 | 3313 | `		if( zIn >= zEnd ){` |
|      - | 3314 | `			/* No more input to process */` |
|      5 | 3315 | `			break;` |
|      - | 3316 | `		}` |
|      - | 3317 | `		/* Output the HTML line break */` |
|      - | 3318 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br/>' (legacy without space), otherwise use '<br>' */` |
|      5 | 3319 | `		if( is_xhtml ){` |
|      3 | 3320 | `			ph7_result_string(pCtx,"<br/>",(int)sizeof("<br/>")-1);` |
|      2 | 3321 | `		}else{` |
|      3 | 3322 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3323 | `		}` |
|      5 | 3324 | `		zCur = zIn;` |
|      - | 3325 | `		/* Append trailing line */` |
|     11 | 3326 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3327 | `			zIn++;` |
|      1 | 3328 | `		}` |
|      5 | 3329 | `		if( zCur < zIn ){` |
|      - | 3330 | `			/* Output chunk verbatim */` |
|      5 | 3331 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 3332 | `		}` |
|      1 | 3333 | `	}` |
|      5 | 3334 | `	return PH7_OK;` |
|      4 | 3335 |  |
|      - | 3336 | `/*` |
|      - | 3337 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3338 | ` *  According to the PHP reference manual.` |
|      - | 3339 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3340 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3341 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 3342 | ` * This applies to both sprintf() and printf().` |
|      - | 3343 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 3344 | ` * or more of these elements, in order:` |
|      - | 3345 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 3346 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 3347 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 3348 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 3349 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 3350 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 3351 | ` *   it with a single quote ('). See the examples below.` |
|      - | 3352 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 3353 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 3354 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 3355 | ` *   should result in.` |
|      - | 3356 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 3357 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 3358 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 3359 | ` *   limit to the string.` |
|      - | 3360 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 3361 | ` *       % - a literal percent character. No argument is required.` |
|      - | 3362 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 3363 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 3364 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 3365 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 3366 | ` * 	     for the number of digits after the decimal point.` |
|      - | 3367 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 3368 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 3369 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 3370 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 3371 | ` *       g - shorter of %e and %f.` |
|      - | 3372 | ` *       G - shorter of %E and %f.` |
|      - | 3373 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 3374 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 3375 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 3376 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 3377 | ` */` |
|      - | 3378 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3379 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 3380 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 3381 | `/*` |
|      - | 3382 | `** Conversion types fall into various categories as defined by the` |
|      - | 3383 | `** following enumeration.` |
|      - | 3384 | `*/` |
|      - | 3385 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 3386 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 3387 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 3388 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 3389 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 3390 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 3391 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 3392 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 3393 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 3394 |  |
|      - | 3395 | `/*` |
|      - | 3396 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 3397 | `*/` |
|      - | 3398 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 3399 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 3400 | `/*` |
|      - | 3401 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 3402 | `** by an instance of the following structure` |
|      - | 3403 | `*/` |
|      - | 3404 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 3405 | `struct ph7_fmt_info` |
|      - | 3406 |  |
|      - | 3407 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 3408 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 3409 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 3410 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 3411 | `  char *charset; /* The character set for conversion */` |
|      - | 3412 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 3413 | `};` |
|      - | 3414 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3415 | `/*` |
|      - | 3416 | `** "*val" is a double such that 0.1 <= *val < 10.0` |
|      - | 3417 | `** Return the ascii code for the leading digit of *val, then` |
|      - | 3418 | `** multiply "*val" by 10.0 to renormalize.` |
|      - | 3419 | `**` |
|      - | 3420 | `** Example:` |
|      - | 3421 | `**     input:     *val = 3.14159` |
|      - | 3422 | `**     output:    *val = 1.4159    function return = '3'` |
|      - | 3423 | `**` |
|      - | 3424 | `** The counter *cnt is incremented each time.  After counter exceeds` |
|      - | 3425 | `** 16 (the number of significant digits in a 64-bit float) '0' is` |
|      - | 3426 | `** always returned.` |
|      - | 3427 | `*/` |
|    422 | 3428 | `static int vxGetdigit(sxlongreal *val,int *cnt)` |
|      1 | 3429 |  |
|      - | 3430 | `  sxlongreal d;` |
|      - | 3431 | `  int digit;` |
|      - | 3432 |  |
|    423 | 3433 | `  if( (*cnt)++ >= 16 ){` |
|    ! 0 | 3434 | `	  return '0';` |
|      - | 3435 | `  }` |
|    423 | 3436 | `  digit = (int)*val;` |
|    423 | 3437 | `  d = digit;` |
|    423 | 3438 | `   *val = (*val - d)*10.0;` |
|    423 | 3439 | `  return digit + '0' ;` |
|    212 | 3440 |  |
|      - | 3441 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      - | 3442 | `/*` |
|      - | 3443 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 3444 | ` * used conversion types first.` |
|      - | 3445 | ` */` |
|      - | 3446 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 3447 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 3448 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 3449 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 3450 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 3451 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 3452 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 3453 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 3454 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 3455 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3456 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3457 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 3458 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 3459 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 3460 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 3461 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 3462 | `};` |
|      - | 3463 | `/*` |
|      - | 3464 | ` * Format a given string.` |
|      - | 3465 | ` * The root program.  All variations call this core.` |
|      - | 3466 | ` * INPUTS:` |
|      - | 3467 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 3468 | ` *            1. A pointer to the call context.` |
|      - | 3469 | ` *            2. A pointer to the list of characters to be output` |
|      - | 3470 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 3471 | ` *            3. An integer number of characters to be output.` |
|      - | 3472 | ` *               (Note: This number might be zero.)` |
|      - | 3473 | ` *            4. Upper layer private data.` |
|      - | 3474 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 3475 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 3476 | ` */` |
|    136 | 3477 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 3478 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 3479 | `	ph7_context *pCtx,  /* call context */` |
|      - | 3480 | `	const char *zIn,    /* Format string */` |
|      - | 3481 | `	int nByte,          /* Format string length */` |
|      - | 3482 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 3483 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 3484 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 3485 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 3486 | `	)` |
|      1 | 3487 |  |
|    137 | 3488 | `	char spaces[] = "                                                  ";` |
|      - | 3489 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    137 | 3490 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 3491 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 3492 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 3493 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 3494 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 3495 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 3496 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 3497 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 3498 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 3499 | `	ph7_int64 iVal;` |
|      - | 3500 | `	int precision;           /* Precision of the current field */` |
|      - | 3501 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 3502 | `	int c,rc,n;` |
|      - | 3503 | `	int length;              /* Length of the field */` |
|      - | 3504 | `	int prefix;` |
|      - | 3505 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 3506 | `	int width;               /* Width of the current field */` |
|      - | 3507 | `	int idx;` |
|    137 | 3508 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 3509 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 3510 | `	/* Start the format process */` |
|    139 | 3511 | `	for(;;){` |
|    279 | 3512 | `		zCur = zIn;` |
|    739 | 3513 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|    461 | 3514 | `			zIn++;` |
|      1 | 3515 | `		}` |
|    279 | 3516 | `		if( zCur < zIn ){` |
|      - | 3517 | `			/* Consume chunk verbatim */` |
|    105 | 3518 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|    105 | 3519 | `			if( rc == SXERR_ABORT ){` |
|      - | 3520 | `				/* Callback request an operation abort */` |
|    ! 0 | 3521 | `				break;` |
|      - | 3522 | `			}` |
|     52 | 3523 | `		}` |
|    279 | 3524 | `		if( zIn >= zEnd ){` |
|      - | 3525 | `			/* No more input to process,break immediately */` |
|    135 | 3526 | `			break;` |
|      - | 3527 | `		}` |
|      - | 3528 | `		/* Find out what flags are present */` |
|    145 | 3529 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    144 | 3530 | `			flag_alternateform = flag_zeropad = 0;` |
|    145 | 3531 | `		zIn++; /* Jump the precent sign */` |
|     72 | 3532 | `		do{` |
|    177 | 3533 | `			c = zIn[0];` |
|    177 | 3534 | `			switch( c ){` |
|      9 | 3535 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      3 | 3536 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 3537 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|      9 | 3538 | `			case '#':   flag_alternateform = 1;   c = 0;   break;` |
|      9 | 3539 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 3540 | `			case '\'':` |
|    ! 0 | 3541 | `				zIn++;` |
|    ! 0 | 3542 | `				if( zIn < zEnd ){` |
|      - | 3543 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 3544 | `					c = zIn[0];` |
|    ! 0 | 3545 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3546 | `						spaces[idx] = (char)c;` |
|    ! 0 | 3547 | `					}` |
|    ! 0 | 3548 | `					c = 0;` |
|    ! 0 | 3549 | `				}` |
|    ! 0 | 3550 | `				break;` |
|    144 | 3551 | `			default:                                       break;` |
|      - | 3552 | `			}` |
|    177 | 3553 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 3554 | `		/* Get the field width */` |
|    145 | 3555 | `		width = 0;` |
|    251 | 3556 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     35 | 3557 | `			width = width*10 + (zIn[0] - '0');` |
|     35 | 3558 | `			zIn++;` |
|      1 | 3559 | `		}` |
|    145 | 3560 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 3561 | `			/* Position specifer */` |
|    ! 0 | 3562 | `			if( width > 0 ){` |
|    ! 0 | 3563 | `				n = width;` |
|    ! 0 | 3564 | `				if( vf && n > 0 ){` |
|    ! 0 | 3565 | `					n--;` |
|    ! 0 | 3566 | `				}` |
|    ! 0 | 3567 | `			}` |
|    ! 0 | 3568 | `			zIn++;` |
|    ! 0 | 3569 | `			width = 0;` |
|    ! 0 | 3570 | `			if( zIn < zEnd && zIn[0] == '0' ){` |
|    ! 0 | 3571 | `				flag_zeropad = 1;` |
|    ! 0 | 3572 | `				zIn++;` |
|    ! 0 | 3573 | `			}` |
|    ! 0 | 3574 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 3575 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 3576 | `				zIn++;` |
|    ! 0 | 3577 | `			}` |
|    ! 0 | 3578 | `		}` |
|    145 | 3579 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 3580 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 3581 | `		}` |
|      - | 3582 | `		/* Get the precision */` |
|    145 | 3583 | `		precision = -1;` |
|    145 | 3584 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     59 | 3585 | `			precision = 0;` |
|     59 | 3586 | `			zIn++;` |
|    150 | 3587 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     63 | 3588 | `				precision = precision*10 + (zIn[0] - '0');` |
|     63 | 3589 | `				zIn++;` |
|      1 | 3590 | `			}` |
|     29 | 3591 | `		}` |
|    145 | 3592 | `		if( zIn >= zEnd ){` |
|      - | 3593 | `			/* No more input */` |
|      3 | 3594 | `			break;` |
|      - | 3595 | `		}` |
|      - | 3596 | `		/* Fetch the info entry for the field */` |
|    143 | 3597 | `		pInfo = 0;` |
|    143 | 3598 | `		xtype = PH7_FMT_ERROR;` |
|    143 | 3599 | `		c = zIn[0];` |
|    143 | 3600 | `		zIn++; /* Jump the format specifer */` |
|    787 | 3601 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|    785 | 3602 | `			if( c==aFmt[idx].fmttype ){` |
|    141 | 3603 | `				pInfo = &aFmt[idx];` |
|    141 | 3604 | `				xtype = pInfo->type;` |
|    141 | 3605 | `				break;` |
|      - | 3606 | `			}` |
|    323 | 3607 | `		}` |
|    143 | 3608 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    143 | 3609 | `		length = 0;` |
|      - | 3610 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 3611 | `		 /*` |
|      - | 3612 | `		  ** At this point, variables are initialized as follows:` |
|      - | 3613 | `		  **` |
|      - | 3614 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 3615 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 3616 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 3617 | `		  **                               field width was negative.` |
|      - | 3618 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 3619 | `		  **                               the conversion character.` |
|      - | 3620 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 3621 | `		  **   width                       The specified field width.  This is` |
|      - | 3622 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 3623 | `		  **   precision                   The specified precision.  The default` |
|      - | 3624 | `		  **                               is -1.` |
|      - | 3625 | `		  */` |
|    143 | 3626 | `		switch(xtype){` |
|    ! 0 | 3627 | `		case PH7_FMT_PERCENT:` |
|      - | 3628 | `			/* A literal percent character */` |
|    ! 0 | 3629 | `			zWorker[0] = '%';` |
|    ! 0 | 3630 | `			length = (int)sizeof(char);` |
|    ! 0 | 3631 | `			break;` |
|      3 | 3632 | `		case PH7_FMT_CHARX:` |
|      - | 3633 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 3634 | `			 * with that ASCII value` |
|      - | 3635 | `			 */` |
|      7 | 3636 | `			pArg = NEXT_ARG;` |
|      7 | 3637 | `			if( pArg == 0 ){` |
|      3 | 3638 | `				c = 0;` |
|      2 | 3639 | `			}else{` |
|      5 | 3640 | `				c = ph7_value_to_int(pArg);` |
|      - | 3641 | `			}` |
|      - | 3642 | `			/* NUL byte is an acceptable value */` |
|      7 | 3643 | `			zWorker[0] = (char)c;` |
|      7 | 3644 | `			length = (int)sizeof(char);` |
|      7 | 3645 | `			break;` |
|     12 | 3646 | `		case PH7_FMT_STRING:` |
|      - | 3647 | `			/* the argument is treated as and presented as a string */` |
|     25 | 3648 | `			pArg = NEXT_ARG;` |
|     25 | 3649 | `			if( pArg == 0 ){` |
|    ! 0 | 3650 | `				length = 0;` |
|    ! 0 | 3651 | `			}else{` |
|     25 | 3652 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 3653 | `			}` |
|     25 | 3654 | `			if( length < 1 ){` |
|    ! 0 | 3655 | `				zBuf = " ";` |
|    ! 0 | 3656 | `				length = (int)sizeof(char);` |
|    ! 0 | 3657 | `			}` |
|     25 | 3658 | `			if( precision>=0 && precision<length ){` |
|      3 | 3659 | `				length = precision;` |
|      1 | 3660 | `			}` |
|     25 | 3661 | `			if( flag_zeropad ){` |
|      - | 3662 | `				/* zero-padding works on strings too */` |
|    ! 0 | 3663 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3664 | `					spaces[idx] = '0';` |
|    ! 0 | 3665 | `				}` |
|    ! 0 | 3666 | `			}` |
|     25 | 3667 | `			break;` |
|     27 | 3668 | `		case PH7_FMT_RADIX:` |
|     55 | 3669 | `			pArg = NEXT_ARG;` |
|     55 | 3670 | `			if( pArg == 0 ){` |
|    ! 0 | 3671 | `				iVal = 0;` |
|    ! 0 | 3672 | `			}else{` |
|     55 | 3673 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 3674 | `			}` |
|      - | 3675 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|     55 | 3676 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 3677 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 3678 | `			}` |
|      - | 3679 | `#if 1` |
|      - | 3680 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 3681 | `        ** I think this is stupid.*/` |
|     55 | 3682 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 3683 | `#else` |
|      - | 3684 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 3685 | `        ** but leave the prefix for hex.*/` |
|      - | 3686 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 3687 | `#endif` |
|     55 | 3688 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|     25 | 3689 | `          if( iVal<0 ){` |
|      3 | 3690 | `            iVal = -iVal;` |
|      - | 3691 | `			/* Ticket 1433-003 */` |
|      3 | 3692 | `			if( iVal < 0 ){` |
|      - | 3693 | `				/* Overflow */` |
|    ! 0 | 3694 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3695 | `			}` |
|      3 | 3696 | `            prefix = '-';` |
|     24 | 3697 | `          }else if( flag_plussign )  prefix = '+';` |
|     21 | 3698 | `          else if( flag_blanksign )  prefix = ' ';` |
|     19 | 3699 | `          else                       prefix = 0;` |
|     13 | 3700 | `        }else{` |
|     31 | 3701 | `			if( iVal<0 ){` |
|    ! 0 | 3702 | `				iVal = -iVal;` |
|      - | 3703 | `				/* Ticket 1433-003 */` |
|    ! 0 | 3704 | `				if( iVal < 0 ){` |
|      - | 3705 | `					/* Overflow */` |
|    ! 0 | 3706 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3707 | `				}` |
|    ! 0 | 3708 | `			}` |
|     31 | 3709 | `			prefix = 0;` |
|      - | 3710 | `		}` |
|     55 | 3711 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      7 | 3712 | `          precision = width-(prefix!=0);` |
|      3 | 3713 | `        }` |
|     55 | 3714 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 3715 | `        {` |
|      - | 3716 | `          register char *cset;      /* Use registers for speed */` |
|      - | 3717 | `          register int base;` |
|     55 | 3718 | `          cset = pInfo->charset;` |
|     55 | 3719 | `          base = pInfo->base;` |
|     27 | 3720 | `          do{                                           /* Convert to ascii */` |
|    123 | 3721 | `            *(--zBuf) = cset[iVal%base];` |
|    123 | 3722 | `            iVal = iVal/base;` |
|    123 | 3723 | `          }while( iVal>0 );` |
|      - | 3724 | `        }` |
|     55 | 3725 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     77 | 3726 | `        for(idx=precision-length; idx>0; idx--){` |
|     23 | 3727 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|     12 | 3728 | `        }` |
|     55 | 3729 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|     55 | 3730 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 3731 | `          char *pre, x;` |
|      9 | 3732 | `          pre = pInfo->prefix;` |
|      9 | 3733 | `          if( *zBuf!=pre[0] ){` |
|     23 | 3734 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|      4 | 3735 | `          }` |
|      4 | 3736 | `        }` |
|     55 | 3737 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     55 | 3738 | `		break;` |
|     28 | 3739 | `		case PH7_FMT_FLOAT:` |
|      - | 3740 | `		case PH7_FMT_EXP:` |
|      - | 3741 | `		case PH7_FMT_GENERIC:{` |
|      - | 3742 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3743 | `		long double realvalue;` |
|      - | 3744 | `		int  exp;                /* exponent of real numbers */` |
|      - | 3745 | `		double rounder;          /* Used for rounding floating point values */` |
|      - | 3746 | `		int flag_dp;            /* True if decimal point should be shown */` |
|      - | 3747 | `		int flag_rtz;           /* True if trailing zeros should be removed */` |
|      - | 3748 | `		int flag_exp;           /* True to force display of the exponent */` |
|      - | 3749 | `		int nsd;                 /* Number of significant digits returned */` |
|     57 | 3750 | `		pArg = NEXT_ARG;` |
|     57 | 3751 | `		if( pArg == 0 ){` |
|    ! 0 | 3752 | `			realvalue = 0;` |
|    ! 0 | 3753 | `		}else{` |
|     57 | 3754 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 3755 | `		}` |
|      - | 3756 | `		/* Special-case NaN and infinities since the normal formatting logic` |
|      - | 3757 | `		 * below assumes a finite positive realvalue. */` |
|     57 | 3758 | `		if( PH7_IS_NAN(realvalue) ){` |
|    ! 0 | 3759 | `			zBuf = "NAN";` |
|    ! 0 | 3760 | `			length = 3;` |
|    ! 0 | 3761 | `			break;` |
|      - | 3762 | `		}` |
|     57 | 3763 | `		if( PH7_IS_INF(realvalue) ){` |
|      - | 3764 | `			/* Infinity prints as INF or -INF depending on sign. */` |
|    ! 0 | 3765 | `			if( realvalue < 0.0 ){` |
|    ! 0 | 3766 | `				zBuf = "-INF";` |
|    ! 0 | 3767 | `				length = 4;` |
|    ! 0 | 3768 | `			}else{` |
|    ! 0 | 3769 | `				zBuf = "INF";` |
|    ! 0 | 3770 | `				length = 3;` |
|      - | 3771 | `			}` |
|    ! 0 | 3772 | `			break;` |
|      - | 3773 | `		}` |
|     57 | 3774 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|     57 | 3775 | `		if( precision>PH7_FMT_BUFSIZ-40) precision = PH7_FMT_BUFSIZ-40;` |
|     57 | 3776 | `        if( realvalue<0.0 ){` |
|      3 | 3777 | `          realvalue = -realvalue;` |
|      3 | 3778 | `          prefix = '-';` |
|      2 | 3779 | `        }else{` |
|     55 | 3780 | `          if( flag_plussign )          prefix = '+';` |
|     55 | 3781 | `          else if( flag_blanksign )    prefix = ' ';` |
|     55 | 3782 | `          else                         prefix = 0;` |
|      - | 3783 | `        }` |
|     57 | 3784 | `        if( pInfo->type==PH7_FMT_GENERIC && precision>0 ) precision--;` |
|     57 | 3785 | `        rounder = 0.0;` |
|      - | 3786 | `#if 0` |
|      - | 3787 | `        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */` |
|      - | 3788 | `        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);` |
|      - | 3789 | `#else` |
|      - | 3790 | `        /* It makes more sense to use 0.5 */` |
|    405 | 3791 | `        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);` |
|      - | 3792 | `#endif` |
|     57 | 3793 | `        if( pInfo->type==PH7_FMT_FLOAT ) realvalue += rounder;` |
|      - | 3794 | `        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */` |
|     57 | 3795 | `        exp = 0;` |
|     57 | 3796 | `        if( realvalue>0.0 ){` |
|     61 | 3797 | `          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }` |
|     89 | 3798 | `          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }` |
|     61 | 3799 | `          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }` |
|     71 | 3800 | `          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }` |
|     57 | 3801 | `          if( exp>350 \|\| exp<-350 ){` |
|    ! 0 | 3802 | `            zBuf = "NaN";` |
|    ! 0 | 3803 | `            length = 3;` |
|    ! 0 | 3804 | `            break;` |
|      - | 3805 | `          }` |
|     28 | 3806 | `        }` |
|     57 | 3807 | `        zBuf = zWorker;` |
|      - | 3808 | `        /*` |
|      - | 3809 | `        ** If the field type is etGENERIC, then convert to either etEXP` |
|      - | 3810 | `        ** or etFLOAT, as appropriate.` |
|      - | 3811 | `        */` |
|     57 | 3812 | `        flag_exp = xtype==PH7_FMT_EXP;` |
|     57 | 3813 | `        if( xtype!=PH7_FMT_FLOAT ){` |
|    ! 0 | 3814 | `          realvalue += rounder;` |
|    ! 0 | 3815 | `          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }` |
|    ! 0 | 3816 | `        }` |
|     57 | 3817 | `        if( xtype==PH7_FMT_GENERIC ){` |
|    ! 0 | 3818 | `          flag_rtz = !flag_alternateform;` |
|    ! 0 | 3819 | `          if( exp<-4 \|\| exp>precision ){` |
|    ! 0 | 3820 | `            xtype = PH7_FMT_EXP;` |
|    ! 0 | 3821 | `          }else{` |
|    ! 0 | 3822 | `            precision = precision - exp;` |
|    ! 0 | 3823 | `            xtype = PH7_FMT_FLOAT;` |
|      - | 3824 | `          }` |
|    ! 0 | 3825 | `        }else{` |
|     57 | 3826 | `          flag_rtz = 0;` |
|      - | 3827 | `        }` |
|      - | 3828 | `        /*` |
|      - | 3829 | `        ** The "exp+precision" test causes output to be of type etEXP if` |
|      - | 3830 | `        ** the precision is too large to fit in buf[].` |
|      - | 3831 | `        */` |
|     57 | 3832 | `        nsd = 0;` |
|     57 | 3833 | `        if( xtype==PH7_FMT_FLOAT && exp+precision<PH7_FMT_BUFSIZ-30 ){` |
|     57 | 3834 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|     57 | 3835 | `          if( prefix ) *(zBuf++) = (char)prefix;         /* Sign */` |
|     57 | 3836 | `          if( exp<0 )  *(zBuf++) = '0';            /* Digits before "." */` |
|    149 | 3837 | `          else for(; exp>=0; exp--) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     57 | 3838 | `          if( flag_dp ) *(zBuf++) = '.';           /* The decimal point */` |
|     89 | 3839 | `          for(exp++; exp<0 && precision>0; precision--, exp++){` |
|     33 | 3840 | `            *(zBuf++) = '0';` |
|     17 | 3841 | `          }` |
|    373 | 3842 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     57 | 3843 | `          *(zBuf--) = 0;                           /* Null terminate */` |
|     57 | 3844 | `          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */` |
|    ! 0 | 3845 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 3846 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 3847 | `          }` |
|     57 | 3848 | `          zBuf++;                            /* point to next free slot */` |
|     29 | 3849 | `        }else{    /* etEXP or etGENERIC */` |
|    ! 0 | 3850 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|    ! 0 | 3851 | `          if( prefix ) *(zBuf++) = (char)prefix;   /* Sign */` |
|    ! 0 | 3852 | `          *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);  /* First digit */` |
|    ! 0 | 3853 | `          if( flag_dp ) *(zBuf++) = '.';     /* Decimal point */` |
|    ! 0 | 3854 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|    ! 0 | 3855 | `          zBuf--;                            /* point to last digit */` |
|    ! 0 | 3856 | `          if( flag_rtz && flag_dp ){          /* Remove tail zeros */` |
|    ! 0 | 3857 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 3858 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 3859 | `          }` |
|    ! 0 | 3860 | `          zBuf++;                            /* point to next free slot */` |
|    ! 0 | 3861 | `          if( exp \|\| flag_exp ){` |
|    ! 0 | 3862 | `            *(zBuf++) = pInfo->charset[0];` |
|    ! 0 | 3863 | `            if( exp<0 ){ *(zBuf++) = '-'; exp = -exp; } /* sign of exp */` |
|    ! 0 | 3864 | `            else       { *(zBuf++) = '+'; }` |
|    ! 0 | 3865 | `            if( exp>=100 ){` |
|    ! 0 | 3866 | `              *(zBuf++) = (char)((exp/100)+'0');                /* 100's digit */` |
|    ! 0 | 3867 | `              exp %= 100;` |
|    ! 0 | 3868 | `            }` |
|    ! 0 | 3869 | `            *(zBuf++) = (char)(exp/10+'0');                     /* 10's digit */` |
|    ! 0 | 3870 | `            *(zBuf++) = (char)(exp%10+'0');                     /* 1's digit */` |
|    ! 0 | 3871 | `          }` |
|      - | 3872 | `        }` |
|      - | 3873 | `        /* The converted number is in buf[] and zero terminated.Output it.` |
|      - | 3874 | `        ** Note that the number is in the usual order, not reversed as with` |
|      - | 3875 | `        ** integer conversions.*/` |
|     57 | 3876 | `        length = (int)(zBuf-zWorker);` |
|     57 | 3877 | `        zBuf = zWorker;` |
|      - | 3878 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 3879 | `        ** set and we are not left justified */` |
|     57 | 3880 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 3881 | `          int i;` |
|      3 | 3882 | `          int nPad = width - length;` |
|     13 | 3883 | `          for(i=width; i>=nPad; i--){` |
|     11 | 3884 | `            zBuf[i] = zBuf[i-nPad];` |
|      6 | 3885 | `          }` |
|      3 | 3886 | `          i = prefix!=0;` |
|      5 | 3887 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      3 | 3888 | `          length = width;` |
|      1 | 3889 | `        }` |
|      - | 3890 | `#else` |
|      - | 3891 | `         zBuf = " ";` |
|      - | 3892 | `		 length = (int)sizeof(char);` |
|      - | 3893 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|     57 | 3894 | `		 break;` |
|      - | 3895 | `							 }` |
|      1 | 3896 | `		default:` |
|      - | 3897 | `			/* Invalid format specifer */` |
|      3 | 3898 | `			zWorker[0] = '?';` |
|      3 | 3899 | `			length = (int)sizeof(char);` |
|      2 | 3900 | `			break;` |
|      - | 3901 | `		}` |
|      - | 3902 | `		 /*` |
|      - | 3903 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 3904 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 3905 | `		 ** the output.` |
|      - | 3906 | `		 */` |
|    143 | 3907 | `    if( !flag_leftjustify ){` |
|      - | 3908 | `      register int nspace;` |
|    135 | 3909 | `      nspace = width-length;` |
|    135 | 3910 | `      if( nspace>0 ){` |
|      5 | 3911 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3912 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3913 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3914 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3915 | `			}` |
|    ! 0 | 3916 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3917 | `        }` |
|      5 | 3918 | `        if( nspace>0 ){` |
|      5 | 3919 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      5 | 3920 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3921 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3922 | `			}` |
|      2 | 3923 | `		}` |
|      2 | 3924 | `      }` |
|     67 | 3925 | `    }` |
|    143 | 3926 | `    if( length>0 ){` |
|    143 | 3927 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    143 | 3928 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3929 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3930 | `		}` |
|     71 | 3931 | `    }` |
|    143 | 3932 | `    if( flag_leftjustify ){` |
|      - | 3933 | `      register int nspace;` |
|      9 | 3934 | `      nspace = width-length;` |
|      9 | 3935 | `      if( nspace>0 ){` |
|      9 | 3936 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3937 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3938 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3939 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3940 | `			}` |
|    ! 0 | 3941 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3942 | `        }` |
|      9 | 3943 | `        if( nspace>0 ){` |
|      9 | 3944 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      9 | 3945 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3946 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3947 | `			}` |
|      4 | 3948 | `		}` |
|      4 | 3949 | `      }` |
|      4 | 3950 | `    }` |
|      1 | 3951 | ` }/* for(;;) */` |
|    137 | 3952 | `	return SXRET_OK;` |
|     69 | 3953 |  |
|      - | 3954 | `/*` |
|      - | 3955 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 3956 | ` */` |
|     90 | 3957 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 3958 |  |
|      - | 3959 | `	/* Consume directly */` |
|     91 | 3960 | `	ph7_result_string(pCtx,zInput,nLen);` |
|     45 | 3961 | `	SXUNUSED(pUserData); /* cc warning */` |
|     91 | 3962 | `	return PH7_OK;` |
|      1 | 3963 |  |
|      - | 3964 | `/*` |
|      - | 3965 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 3966 | ` *  Return a formatted string.` |
|      - | 3967 | ` * Parameters` |
|      - | 3968 | ` *  $format` |
|      - | 3969 | ` *    The format string (see block comment above)` |
|      - | 3970 | ` * Return` |
|      - | 3971 | ` *  A string produced according to the formatting string format.` |
|      - | 3972 | ` */` |
|     62 | 3973 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3974 |  |
|      - | 3975 | `	const char *zFormat;` |
|      - | 3976 | `	int nLen;` |
|     63 | 3977 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3978 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 3979 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3980 | `		return PH7_OK;` |
|      - | 3981 | `	}` |
|      - | 3982 | `	/* Extract the string format */` |
|     61 | 3983 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     61 | 3984 | `	if( nLen < 1 ){` |
|      - | 3985 | `		/* Empty string */` |
|    ! 0 | 3986 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3987 | `		return PH7_OK;` |
|      - | 3988 | `	}` |
|      - | 3989 | `	/* Format the string */` |
|     61 | 3990 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,0,FALSE);` |
|     61 | 3991 | `	return PH7_OK;` |
|     32 | 3992 |  |
|      - | 3993 | `/*` |
|      - | 3994 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 3995 | ` */` |
|    130 | 3996 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 3997 |  |
|    131 | 3998 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 3999 | `	/* Call the VM output consumer directly */` |
|    131 | 4000 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 4001 | `	/* Increment counter */` |
|    131 | 4002 | `	*pCounter += nLen;` |
|    131 | 4003 | `	return PH7_OK;` |
|      1 | 4004 |  |
|      - | 4005 | `/*` |
|      - | 4006 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 4007 | ` *  Output a formatted string.` |
|      - | 4008 | ` * Parameters` |
|      - | 4009 | ` *  $format` |
|      - | 4010 | ` *   See sprintf() for a description of format.` |
|      - | 4011 | ` * Return` |
|      - | 4012 | ` *  The length of the outputted string.` |
|      - | 4013 | ` */` |
|     52 | 4014 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4015 |  |
|     53 | 4016 | `	ph7_int64 nCounter = 0;` |
|      - | 4017 | `	const char *zFormat;` |
|      - | 4018 | `	int nLen;` |
|     53 | 4019 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4020 | `		/* Missing/Invalid arguments,return 0 */` |
|      3 | 4021 | `		ph7_result_int(pCtx,0);` |
|      3 | 4022 | `		return PH7_OK;` |
|      - | 4023 | `	}` |
|      - | 4024 | `	/* Extract the string format */` |
|     51 | 4025 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     51 | 4026 | `	if( nLen < 1 ){` |
|      - | 4027 | `		/* Empty string */` |
|    ! 0 | 4028 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4029 | `		return PH7_OK;` |
|      - | 4030 | `	}` |
|      - | 4031 | `	/* Format the string */` |
|     51 | 4032 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 4033 | `	/* Return the length of the outputted string */` |
|     51 | 4034 | `	ph7_result_int64(pCtx,nCounter);` |
|     51 | 4035 | `	return PH7_OK;` |
|     27 | 4036 |  |
|      - | 4037 | `/*` |
|      - | 4038 | ` * int vprintf(string $format,array $args)` |
|      - | 4039 | ` *  Output a formatted string.` |
|      - | 4040 | ` * Parameters` |
|      - | 4041 | ` *  $format` |
|      - | 4042 | ` *   See sprintf() for a description of format.` |
|      - | 4043 | ` * Return` |
|      - | 4044 | ` *  The length of the outputted string.` |
|      - | 4045 | ` */` |
|      2 | 4046 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4047 |  |
|      3 | 4048 | `	ph7_int64 nCounter = 0;` |
|      - | 4049 | `	const char *zFormat;` |
|      - | 4050 | `	ph7_hashmap *pMap;` |
|      - | 4051 | `	SySet sArg;` |
|      - | 4052 | `	int nLen,n;` |
|      3 | 4053 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4054 | `		/* Missing/Invalid arguments,return 0 */` |
|    ! 0 | 4055 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4056 | `		return PH7_OK;` |
|      - | 4057 | `	}` |
|      - | 4058 | `	/* Extract the string format */` |
|      3 | 4059 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4060 | `	if( nLen < 1 ){` |
|      - | 4061 | `		/* Empty string */` |
|    ! 0 | 4062 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4063 | `		return PH7_OK;` |
|      - | 4064 | `	}` |
|      - | 4065 | `	/* Point to the hashmap */` |
|      3 | 4066 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4067 | `	/* Extract arguments from the hashmap */` |
|      3 | 4068 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4069 | `	/* Format the string */` |
|      3 | 4070 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 4071 | `	/* Return the length of the outputted string */` |
|      3 | 4072 | `	ph7_result_int64(pCtx,nCounter);` |
|      - | 4073 | `	/* Release the container */` |
|      3 | 4074 | `	SySetRelease(&sArg);` |
|      3 | 4075 | `	return PH7_OK;` |
|      2 | 4076 |  |
|      - | 4077 | `/*` |
|      - | 4078 | ` * int vsprintf(string $format,array $args)` |
|      - | 4079 | ` *  Output a formatted string.` |
|      - | 4080 | ` * Parameters` |
|      - | 4081 | ` *  $format` |
|      - | 4082 | ` *   See sprintf() for a description of format.` |
|      - | 4083 | ` * Return` |
|      - | 4084 | ` *  A string produced according to the formatting string format.` |
|      - | 4085 | ` */` |
|     10 | 4086 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4087 |  |
|      - | 4088 | `	const char *zFormat;` |
|      - | 4089 | `	ph7_hashmap *pMap;` |
|      - | 4090 | `	SySet sArg;` |
|      - | 4091 | `	int nLen,n;` |
|     11 | 4092 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4093 | `		/* Missing/Invalid arguments,return the empty string */` |
|      5 | 4094 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 4095 | `		return PH7_OK;` |
|      - | 4096 | `	}` |
|      - | 4097 | `	/* Extract the string format */` |
|      7 | 4098 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4099 | `	if( nLen < 1 ){` |
|      - | 4100 | `		/* Empty string */` |
|    ! 0 | 4101 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4102 | `		return PH7_OK;` |
|      - | 4103 | `	}` |
|      - | 4104 | `	/* Point to hashmap */` |
|      7 | 4105 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4106 | `	/* Extract arguments from the hashmap */` |
|      7 | 4107 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4108 | `	/* Format the string */` |
|      7 | 4109 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),0,TRUE);` |
|      - | 4110 | `	/* Release the container */` |
|      7 | 4111 | `	SySetRelease(&sArg);` |
|      7 | 4112 | `	return PH7_OK;` |
|      6 | 4113 |  |
|      - | 4114 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 4115 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4116 | `/*` |
|      - | 4117 | ` * Symisc eXtension.` |
|      - | 4118 | ` * string size_format(int64 $size)` |
|      - | 4119 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 4120 | ` *  Example:` |
|      - | 4121 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 4122 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 4123 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 4124 | ` * Parameter` |
|      - | 4125 | ` *  $size` |
|      - | 4126 | ` *    Entity size in bytes.` |
|      - | 4127 | ` * Return` |
|      - | 4128 | ` *   Formatted string representation of the given size.` |
|      - | 4129 | ` */` |
|     24 | 4130 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4131 |  |
|      - | 4132 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 4133 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 4134 | `	sxi32 nRest,i_32;` |
|      - | 4135 | `	ph7_int64 iSize;` |
|     25 | 4136 | `	int c = -1; /* index in zUnit[] */` |
|      - | 4137 |  |
|     25 | 4138 | `	if( nArg < 1 ){` |
|      - | 4139 | `		/* Missing argument,return the empty string */` |
|      3 | 4140 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4141 | `		return PH7_OK;` |
|      - | 4142 | `	}` |
|      - | 4143 | `	/* Extract the given size */` |
|     23 | 4144 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 4145 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 4146 | `		/* Don't bother formatting,return immediately */` |
|      5 | 4147 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 4148 | `		return PH7_OK;` |
|      - | 4149 | `	}` |
|     19 | 4150 | `	for(;;){` |
|     39 | 4151 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 4152 | `		iSize >>= 10;` |
|     39 | 4153 | `		c++;` |
|     39 | 4154 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 4155 | `			break;` |
|      - | 4156 | `		}` |
|      1 | 4157 | `	}` |
|     19 | 4158 | `	nRest /= 100;` |
|     19 | 4159 | `	if( nRest > 9 ){` |
|    ! 0 | 4160 | `		nRest = 9;` |
|    ! 0 | 4161 | `	}` |
|     19 | 4162 | `	if( iSize > 999 ){` |
|    ! 0 | 4163 | `		c++;` |
|    ! 0 | 4164 | `		nRest = 9;` |
|    ! 0 | 4165 | `		iSize = 0;` |
|    ! 0 | 4166 | `	}` |
|     19 | 4167 | `	i_32 = (sxi32)iSize;` |
|      - | 4168 | `	/* Format */` |
|     19 | 4169 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 4170 | `	return PH7_OK;` |
|     13 | 4171 |  |
|      - | 4172 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4173 | `/*` |
|      - | 4174 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 4175 | ` *   Calculate the md5 hash of a string.` |
|      - | 4176 | ` * Parameter` |
|      - | 4177 | ` *  $str` |
|      - | 4178 | ` *   Input string` |
|      - | 4179 | ` * $raw_output` |
|      - | 4180 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4181 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4182 | ` * Return` |
|      - | 4183 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 4184 | ` */` |
|     14 | 4185 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4186 |  |
|      - | 4187 | `	unsigned char zDigest[16];` |
|     15 | 4188 | `	int raw_output = FALSE;` |
|      - | 4189 | `	const void *pIn;` |
|      - | 4190 | `	int nLen;` |
|     15 | 4191 | `	if( nArg < 1 ){` |
|      - | 4192 | `		/* Missing arguments,return the empty string */` |
|      3 | 4193 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4194 | `		return PH7_OK;` |
|      - | 4195 | `	}` |
|      - | 4196 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 4197 | `	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */` |
|     13 | 4198 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 4199 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 4200 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 4201 | `	}` |
|      - | 4202 | `	/* Compute the MD5 digest */` |
|     13 | 4203 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|     13 | 4204 | `	if( raw_output ){` |
|      - | 4205 | `		/* Output raw digest */` |
|      5 | 4206 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 4207 | `	}else{` |
|      - | 4208 | `		/* Perform a binary to hex conversion */` |
|      9 | 4209 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4210 | `	}` |
|     13 | 4211 | `	return PH7_OK;` |
|      8 | 4212 |  |
|      - | 4213 | `/*` |
|      - | 4214 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 4215 | ` *   Calculate the sha1 hash of a string.` |
|      - | 4216 | ` * Parameter` |
|      - | 4217 | ` *  $str` |
|      - | 4218 | ` *   Input string` |
|      - | 4219 | ` * $raw_output` |
|      - | 4220 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4221 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4222 | ` * Return` |
|      - | 4223 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 4224 | ` */` |
|     12 | 4225 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4226 |  |
|      - | 4227 | `	unsigned char zDigest[20];` |
|     13 | 4228 | `	int raw_output = FALSE;` |
|      - | 4229 | `	const void *pIn;` |
|      - | 4230 | `	int nLen;` |
|     13 | 4231 | `	if( nArg < 1 ){` |
|      - | 4232 | `		/* Missing arguments,return the empty string */` |
|      3 | 4233 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4234 | `		return PH7_OK;` |
|      - | 4235 | `	}` |
|      - | 4236 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 4237 | `	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */` |
|     11 | 4238 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 4239 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 4240 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 4241 | `	}` |
|      - | 4242 | `	/* Compute the SHA1 digest */` |
|     11 | 4243 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|     11 | 4244 | `	if( raw_output ){` |
|      - | 4245 | `		/* Output raw digest */` |
|      5 | 4246 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 4247 | `	}else{` |
|      - | 4248 | `		/* Perform a binary to hex conversion */` |
|      7 | 4249 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4250 | `	}` |
|     11 | 4251 | `	return PH7_OK;` |
|      7 | 4252 |  |
|      - | 4253 | `/*` |
|      - | 4254 | ` * int64 crc32(string $str)` |
|      - | 4255 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 4256 | ` * Parameter` |
|      - | 4257 | ` *  $str` |
|      - | 4258 | ` *   Input string` |
|      - | 4259 | ` * Return` |
|      - | 4260 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 4261 | ` */` |
|      4 | 4262 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4263 |  |
|      - | 4264 | `	const void *pIn;` |
|      - | 4265 | `	sxu32 nCRC;` |
|      - | 4266 | `	int nLen;` |
|      5 | 4267 | `	if( nArg < 1 ){` |
|      - | 4268 | `		/* Missing arguments,return 0 */` |
|      3 | 4269 | `		ph7_result_int(pCtx,0);` |
|      3 | 4270 | `		return PH7_OK;` |
|      - | 4271 | `	}` |
|      - | 4272 | `	/* Extract the input string */` |
|      3 | 4273 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4274 | `	if( nLen < 1 ){` |
|      - | 4275 | `		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike` |
|      - | 4276 | `		 * md5()/sha1(), whose empty-string digests are non-zero. */` |
|    ! 0 | 4277 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4278 | `		return PH7_OK;` |
|      - | 4279 | `	}` |
|      - | 4280 | `	/* Calculate the sum */` |
|      3 | 4281 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 4282 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 4283 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 4284 | `	return PH7_OK;` |
|      3 | 4285 |  |
|      - | 4286 | `/*` |
|      - | 4287 | ` * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is` |
|      - | 4288 | ` * described by a small record so one dispatch (and one generic HMAC) serves them` |
|      - | 4289 | ` * all. Thin adapters normalize the differing context types and the reversed` |
|      - | 4290 | ` * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.` |
|      - | 4291 | ` */` |
|     11 | 4292 | `static void HashMd5Init(HashCtx *c){ MD5Init(&c->md5); }` |
|     15 | 4293 | `static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }` |
|     11 | 4294 | `static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }` |
|     11 | 4295 | `static void HashSha1Init(HashCtx *c){ SHA1Init(&c->sha1); }` |
|     15 | 4296 | `static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }` |
|     11 | 4297 | `static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }` |
|      9 | 4298 | `static void HashSha224Init(HashCtx *c){ SHA224Init(&c->sha256); }` |
|     33 | 4299 | `static void HashSha256Init(HashCtx *c){ SHA256Init(&c->sha256); }` |
|     57 | 4300 | `static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }` |
|     41 | 4301 | `static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }` |
|      9 | 4302 | `static void HashSha384Init(HashCtx *c){ SHA384Init(&c->sha512); }` |
|     15 | 4303 | `static void HashSha512Init(HashCtx *c){ SHA512Init(&c->sha512); }` |
|     27 | 4304 | `static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }` |
|     23 | 4305 | `static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }` |
|      - | 4306 | `typedef struct HashAlgo HashAlgo;` |
|      - | 4307 | `struct HashAlgo {` |
|      - | 4308 | `	const char *zName;   /* lowercase canonical name */` |
|      - | 4309 | `	int nDigestLen;      /* output bytes: 16/20/28/32/48/64 */` |
|      - | 4310 | `	int nBlockLen;       /* internal block bytes (for HMAC): 64 or 128 */` |
|      - | 4311 | `	void (*xInit)(HashCtx *);` |
|      - | 4312 | `	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);` |
|      - | 4313 | `	void (*xFinal)(HashCtx *,unsigned char *);` |
|      - | 4314 | `};` |
|      - | 4315 | `static const HashAlgo aHashAlgo[] = {` |
|      - | 4316 | `	{ "md5",    16, 64,  HashMd5Init,    HashMd5Update,    HashMd5Final    },` |
|      - | 4317 | `	{ "sha1",   20, 64,  HashSha1Init,   HashSha1Update,   HashSha1Final   },` |
|      - | 4318 | `	{ "sha224", 28, 64,  HashSha224Init, HashSha256Update, HashSha256Final },` |
|      - | 4319 | `	{ "sha256", 32, 64,  HashSha256Init, HashSha256Update, HashSha256Final },` |
|      - | 4320 | `	{ "sha384", 48, 128, HashSha384Init, HashSha512Update, HashSha512Final },` |
|      - | 4321 | `	{ "sha512", 64, 128, HashSha512Init, HashSha512Update, HashSha512Final },` |
|      - | 4322 | `};` |
|      - | 4323 | `/* Case-insensitive algorithm lookup (PHP accepts 'SHA256' etc.). */` |
|     73 | 4324 | `static const HashAlgo * HashFindAlgo(const char *zName,int nLen){` |
|      - | 4325 | `	sxu32 i;` |
|    279 | 4326 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|    272 | 4327 | `		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen` |
|    211 | 4328 | `			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){` |
|     67 | 4329 | `			return &aHashAlgo[i];` |
|      - | 4330 | `		}` |
|    106 | 4331 | `	}` |
|      6 | 4332 | `	return 0;` |
|     38 | 4333 |  |
|      - | 4334 | `/*` |
|      - | 4335 | ` * string hash(string $algo,string $data[,bool $binary = false])` |
|      - | 4336 | ` *   Generate a hash value (message digest).` |
|      - | 4337 | ` */` |
|     54 | 4338 | `static int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4339 |  |
|      - | 4340 | `	const HashAlgo *pAlgo;` |
|      - | 4341 | `	const char *zAlgo,*zData;` |
|     56 | 4342 | `	int nAlgoLen,nDataLen,raw_output = FALSE;` |
|      - | 4343 | `	HashCtx sCtx;` |
|      - | 4344 | `	unsigned char zDigest[64];` |
|     56 | 4345 | `	if( nArg < 2 ){` |
|    ! 0 | 4346 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4347 | `			"hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4348 | `	}` |
|     56 | 4349 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     56 | 4350 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     56 | 4351 | `	if( pAlgo == 0 ){` |
|      3 | 4352 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4353 | `			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - | 4354 | `	}` |
|     53 | 4355 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     53 | 4356 | `	if( nArg > 2 ){` |
|      9 | 4357 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|      4 | 4358 | `	}` |
|     53 | 4359 | `	pAlgo->xInit(&sCtx);` |
|     53 | 4360 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     53 | 4361 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     53 | 4362 | `	if( raw_output ){` |
|      9 | 4363 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|      5 | 4364 | `	}else{` |
|     45 | 4365 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - | 4366 | `	}` |
|     53 | 4367 | `	return PH7_OK;` |
|     29 | 4368 |  |
|      - | 4369 | `/*` |
|      - | 4370 | ` * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])` |
|      - | 4371 | ` *   Generate a keyed hash value using the HMAC method (RFC 2104).` |
|      - | 4372 | ` */` |
|     16 | 4373 | `static int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4374 |  |
|      - | 4375 | `	const HashAlgo *pAlgo;` |
|      - | 4376 | `	const char *zAlgo,*zData,*zKey;` |
|     18 | 4377 | `	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;` |
|      - | 4378 | `	HashCtx sCtx;` |
|      - | 4379 | `	unsigned char zKeyBlock[128],zIpad[128],zOpad[128],zInner[64],zDigest[64];` |
|      - | 4380 | `	int i,nBlock,nDigest;` |
|     18 | 4381 | `	if( nArg < 3 ){` |
|    ! 0 | 4382 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4383 | `			"hash_hmac() expects at least 3 arguments, %d given",nArg);` |
|      - | 4384 | `	}` |
|     18 | 4385 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     18 | 4386 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     18 | 4387 | `	if( pAlgo == 0 ){` |
|      3 | 4388 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4389 | `			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 4390 | `	}` |
|     15 | 4391 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     15 | 4392 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|     15 | 4393 | `	if( nArg > 3 ){` |
|      3 | 4394 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|      1 | 4395 | `	}` |
|     15 | 4396 | `	nBlock = pAlgo->nBlockLen;` |
|     15 | 4397 | `	nDigest = pAlgo->nDigestLen;` |
|      - | 4398 | `	/* Reduce the key to a single block: hash it if longer than the block, then` |
|      - | 4399 | `	 * zero-pad (a short or empty key is just zero-padded). */` |
|     15 | 4400 | `	SyZero(zKeyBlock,sizeof(zKeyBlock));` |
|     15 | 4401 | `	if( nKeyLen > nBlock ){` |
|      3 | 4402 | `		pAlgo->xInit(&sCtx);` |
|      3 | 4403 | `		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);` |
|      3 | 4404 | `		pAlgo->xFinal(&sCtx,zKeyBlock);` |
|     14 | 4405 | `	}else if( nKeyLen > 0 ){` |
|     11 | 4406 | `		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);` |
|      5 | 4407 | `	}` |
|   1039 | 4408 | `	for( i = 0; i < nBlock; i++ ){` |
|   1025 | 4409 | `		zIpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);` |
|   1025 | 4410 | `		zOpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);` |
|    513 | 4411 | `	}` |
|      - | 4412 | `	/* inner = H((key ^ ipad) \|\| data) */` |
|     15 | 4413 | `	pAlgo->xInit(&sCtx);` |
|     15 | 4414 | `	pAlgo->xUpdate(&sCtx,zIpad,(unsigned int)nBlock);` |
|     15 | 4415 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     15 | 4416 | `	pAlgo->xFinal(&sCtx,zInner);` |
|      - | 4417 | `	/* out = H((key ^ opad) \|\| inner) */` |
|     15 | 4418 | `	pAlgo->xInit(&sCtx);` |
|     15 | 4419 | `	pAlgo->xUpdate(&sCtx,zOpad,(unsigned int)nBlock);` |
|     15 | 4420 | `	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)nDigest);` |
|     15 | 4421 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     15 | 4422 | `	if( raw_output ){` |
|      3 | 4423 | `		ph7_result_string(pCtx,(const char *)zDigest,nDigest);` |
|      2 | 4424 | `	}else{` |
|     13 | 4425 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);` |
|      - | 4426 | `	}` |
|     15 | 4427 | `	return PH7_OK;` |
|     10 | 4428 |  |
|      - | 4429 | `/*` |
|      - | 4430 | ` * bool hash_equals(string $known_string,string $user_string)` |
|      - | 4431 | ` *   Timing-attack-safe string comparison.` |
|      - | 4432 | ` */` |
|     14 | 4433 | `static int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4434 |  |
|      - | 4435 | `	const char *zKnown,*zUser;` |
|      - | 4436 | `	int nKnown,nUser,i;` |
|     17 | 4437 | `	volatile unsigned char vDiff = 0;` |
|     17 | 4438 | `	if( nArg < 2 ){` |
|    ! 0 | 4439 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4440 | `			"hash_equals() expects exactly 2 arguments, %d given",nArg);` |
|      - | 4441 | `	}` |
|     17 | 4442 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      4 | 4443 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4444 | `			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",` |
|      1 | 4445 | `			ph7_type_name(apArg[0]));` |
|      - | 4446 | `	}` |
|     14 | 4447 | `	if( !ph7_value_is_string(apArg[1]) ){` |
|      4 | 4448 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4449 | `			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",` |
|      2 | 4450 | `			ph7_type_name(apArg[1]));` |
|      - | 4451 | `	}` |
|     11 | 4452 | `	zKnown = ph7_value_to_string(apArg[0],&nKnown);` |
|     11 | 4453 | `	zUser = ph7_value_to_string(apArg[1],&nUser);` |
|     11 | 4454 | `	if( nKnown != nUser ){` |
|      5 | 4455 | `		ph7_result_bool(pCtx,0);` |
|      5 | 4456 | `		return PH7_OK;` |
|      - | 4457 | `	}` |
|      - | 4458 | `	/* Constant-time: read every byte, never short-circuit. */` |
|     19 | 4459 | `	for( i = 0; i < nKnown; i++ ){` |
|     13 | 4460 | `		vDiff \|= (unsigned char)(zKnown[i] ^ zUser[i]);` |
|      7 | 4461 | `	}` |
|      7 | 4462 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|      7 | 4463 | `	return PH7_OK;` |
|     10 | 4464 |  |
|      - | 4465 | `/*` |
|      - | 4466 | ` * array hash_algos(void)` |
|      - | 4467 | ` *   Return a list of the registered hashing algorithms.` |
|      - | 4468 | ` */` |
|      2 | 4469 | `static int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4470 |  |
|      - | 4471 | `	ph7_value *pArray,*pValue;` |
|      - | 4472 | `	sxu32 i;` |
|      1 | 4473 | `	SXUNUSED(nArg);` |
|      1 | 4474 | `	SXUNUSED(apArg);` |
|      3 | 4475 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4476 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 4477 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 4478 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4479 | `		return PH7_OK;` |
|      - | 4480 | `	}` |
|     15 | 4481 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|     13 | 4482 | `		ph7_value_string(pValue,aHashAlgo[i].zName,-1);` |
|     13 | 4483 | `		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);` |
|     13 | 4484 | `		ph7_value_reset_string_cursor(pValue);` |
|      7 | 4485 | `	}` |
|      3 | 4486 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4487 | `	return PH7_OK;` |
|      2 | 4488 |  |
|      - | 4489 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 4490 | `/*` |
|      - | 4491 | ` * password_* (bcrypt). These live in ext/standard in real PHP — outside the` |
|      - | 4492 | ` * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.` |
|      - | 4493 | ` */` |
|      - | 4494 | `/*` |
|      - | 4495 | ` * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a` |
|      - | 4496 | ` * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).` |
|      - | 4497 | ` */` |
|     40 | 4498 | `static int BcryptParseHash(const char *zHash,int nHash,int *piCost)` |
|      1 | 4499 |  |
|      - | 4500 | `	int iCost;` |
|     51 | 4501 | `	if( nHash != 60 \|\| zHash[0] != '$' \|\| zHash[1] != '2' \|\| zHash[3] != '$'` |
|     29 | 4502 | `		\|\| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){` |
|     13 | 4503 | `		return FALSE;` |
|      - | 4504 | `	}` |
|     29 | 4505 | `	if( zHash[4] < '0' \|\| zHash[4] > '9' \|\| zHash[5] < '0' \|\| zHash[5] > '9' \|\| zHash[6] != '$' ){` |
|    ! 0 | 4506 | `		return FALSE;` |
|      - | 4507 | `	}` |
|     29 | 4508 | `	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');` |
|     29 | 4509 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      3 | 4510 | `		return FALSE;` |
|      - | 4511 | `	}` |
|     27 | 4512 | `	if( piCost ){ *piCost = iCost; }` |
|     27 | 4513 | `	return TRUE;` |
|     21 | 4514 |  |
|      - | 4515 | `/*` |
|      - | 4516 | ` * TRUE if the $algo argument selects bcrypt: null (PASSWORD_DEFAULT) or the` |
|      - | 4517 | ` * "2y" id (PASSWORD_BCRYPT/PASSWORD_DEFAULT). bcrypt is the only supported algo.` |
|      - | 4518 | ` */` |
|     20 | 4519 | `static int BcryptIsBcryptAlgo(ph7_value *pAlgo)` |
|      3 | 4520 |  |
|     23 | 4521 | `	if( ph7_value_is_null(pAlgo) ){` |
|    ! 0 | 4522 | `		return TRUE;` |
|      - | 4523 | `	}` |
|     23 | 4524 | `	if( ph7_value_is_string(pAlgo) ){` |
|      - | 4525 | `		int nAlgo;` |
|     23 | 4526 | `		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);` |
|     23 | 4527 | `		return ( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' );` |
|      - | 4528 | `	}` |
|    ! 0 | 4529 | `	return FALSE;` |
|     13 | 4530 |  |
|      - | 4531 | `/*` |
|      - | 4532 | ` * bool\|string password_hash(string $password,string\|int\|null $algo[,array $options])` |
|      - | 4533 | ` *  Create a bcrypt hash of the password.` |
|      - | 4534 | ` */` |
|     16 | 4535 | `static int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4536 |  |
|      - | 4537 | `	const char *zPwd;` |
|     19 | 4538 | `	int nPwd,iCost = 12;` |
|      - | 4539 | `	unsigned char aSalt[16];` |
|      - | 4540 | `	char zHash[60];` |
|     19 | 4541 | `	if( nArg < 2 ){` |
|    ! 0 | 4542 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4543 | `			"password_hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4544 | `	}` |
|     19 | 4545 | `	if( !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      3 | 4546 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4547 | `			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");` |
|      - | 4548 | `	}` |
|      - | 4549 | `	/* cost from $options['cost'] (default 12). */` |
|     16 | 4550 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|     14 | 4551 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|     14 | 4552 | `		if( pCost ){ iCost = ph7_value_to_int(pCost); }` |
|      6 | 4553 | `	}` |
|     16 | 4554 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      4 | 4555 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 4556 | `			"Invalid bcrypt cost parameter specified: %d",iCost);` |
|      - | 4557 | `	}` |
|     13 | 4558 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     13 | 4559 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 4560 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - | 4561 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|      - | 4562 | `	}` |
|     13 | 4563 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){` |
|    ! 0 | 4564 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4565 | `		return PH7_OK;` |
|      - | 4566 | `	}` |
|     13 | 4567 | `	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));` |
|     13 | 4568 | `	return PH7_OK;` |
|     11 | 4569 |  |
|      - | 4570 | `/*` |
|      - | 4571 | ` * bool password_verify(string $password,string $hash)` |
|      - | 4572 | ` *  Verify a password against a bcrypt hash. Never throws on a malformed hash.` |
|      - | 4573 | ` */` |
|     28 | 4574 | `static int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4575 |  |
|      - | 4576 | `	const char *zPwd,*zHash;` |
|      - | 4577 | `	int nPwd,nHash,iCost,i;` |
|      - | 4578 | `	unsigned char aSalt[16];` |
|      - | 4579 | `	char zComputed[60];` |
|     29 | 4580 | `	volatile unsigned char vDiff = 0;` |
|     29 | 4581 | `	if( nArg < 2 ){` |
|    ! 0 | 4582 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4583 | `			"password_verify() expects exactly 2 arguments, %d given",nArg);` |
|      - | 4584 | `	}` |
|     29 | 4585 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     29 | 4586 | `	zHash = ph7_value_to_string(apArg[1],&nHash);` |
|     29 | 4587 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|     11 | 4588 | `		ph7_result_bool(pCtx,0);` |
|     11 | 4589 | `		return PH7_OK;` |
|      - | 4590 | `	}` |
|      - | 4591 | `	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */` |
|     19 | 4592 | `	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 4593 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4594 | `		return PH7_OK;` |
|      - | 4595 | `	}` |
|     19 | 4596 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){` |
|    ! 0 | 4597 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4598 | `		return PH7_OK;` |
|      - | 4599 | `	}` |
|      - | 4600 | `	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps` |
|      - | 4601 | `	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */` |
|    577 | 4602 | `	for( i = 29; i < 60; i++ ){` |
|    559 | 4603 | `		vDiff \|= (unsigned char)(zComputed[i] ^ zHash[i]);` |
|    280 | 4604 | `	}` |
|     19 | 4605 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|     19 | 4606 | `	return PH7_OK;` |
|     15 | 4607 |  |
|      - | 4608 | `/*` |
|      - | 4609 | ` * array password_get_info(string $hash)` |
|      - | 4610 | ` *  Return ["algo"=>id\|null, "algoName"=>name, "options"=>[...]].` |
|      - | 4611 | ` */` |
|      6 | 4612 | `static int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4613 |  |
|      7 | 4614 | `	const char *zHash = "";` |
|      7 | 4615 | `	int nHash,iCost = 0,bBcrypt = 0;` |
|      - | 4616 | `	ph7_value *pArray,*pOptions,*pVal;` |
|      7 | 4617 | `	if( nArg > 0 ){` |
|      7 | 4618 | `		zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 4619 | `		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);` |
|      3 | 4620 | `	}` |
|      7 | 4621 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 4622 | `	pOptions = ph7_context_new_array(pCtx);` |
|      7 | 4623 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      7 | 4624 | `	if( pArray == 0 \|\| pOptions == 0 \|\| pVal == 0 ){` |
|    ! 0 | 4625 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4626 | `		return PH7_OK;` |
|      - | 4627 | `	}` |
|      7 | 4628 | `	if( bBcrypt ){` |
|      5 | 4629 | `		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */` |
|      5 | 4630 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 4631 | `		ph7_value_reset_string_cursor(pVal);` |
|      5 | 4632 | `		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);` |
|      5 | 4633 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      5 | 4634 | `		ph7_value_int(pVal,iCost);` |
|      5 | 4635 | `		ph7_array_add_strkey_elem(pOptions,"cost",pVal);` |
|      3 | 4636 | `	}else{` |
|      3 | 4637 | `		ph7_value_null(pVal);                          /* algo => null */` |
|      3 | 4638 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      3 | 4639 | `		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);` |
|      3 | 4640 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      - | 4641 | `	}` |
|      7 | 4642 | `	ph7_array_add_strkey_elem(pArray,"options",pOptions);` |
|      7 | 4643 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 4644 | `	return PH7_OK;` |
|      4 | 4645 |  |
|      - | 4646 | `/*` |
|      - | 4647 | ` * bool password_needs_rehash(string $hash,string\|int\|null $algo[,array $options])` |
|      - | 4648 | ` *  True if the hash was not made with the given algo/options.` |
|      - | 4649 | ` */` |
|      6 | 4650 | `static int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4651 |  |
|      - | 4652 | `	const char *zHash;` |
|      7 | 4653 | `	int nHash,iCost = 0,iWantCost = 12;` |
|      7 | 4654 | `	if( nArg < 2 ){` |
|    ! 0 | 4655 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4656 | `			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4657 | `	}` |
|      7 | 4658 | `	zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 4659 | `	if( !BcryptParseHash(zHash,nHash,&iCost) \|\| !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      - | 4660 | `		/* A non-bcrypt hash, or a request for a different algo → needs rehash. */` |
|      3 | 4661 | `		ph7_result_bool(pCtx,1);` |
|      3 | 4662 | `		return PH7_OK;` |
|      - | 4663 | `	}` |
|      5 | 4664 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|      5 | 4665 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|      5 | 4666 | `		if( pCost ){ iWantCost = ph7_value_to_int(pCost); }` |
|      2 | 4667 | `	}` |
|      5 | 4668 | `	ph7_result_bool(pCtx,iCost != iWantCost);` |
|      5 | 4669 | `	return PH7_OK;` |
|      4 | 4670 |  |
|      - | 4671 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4672 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 4673 | `/*` |
|      - | 4674 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 4675 |  |
|      - | 4676 | ` */` |
|      4 | 4677 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 4678 | `	const char *zInput, /* Raw input */` |
|      - | 4679 | `	int nByte,  /* Input length */` |
|      - | 4680 | `	int delim,  /* Delimiter */` |
|      - | 4681 | `	int encl,   /* Enclosure */` |
|      - | 4682 | `	int escape,  /* Escape character */` |
|      - | 4683 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 4684 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 4685 | `	)` |
|      1 | 4686 |  |
|      5 | 4687 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 4688 | `	const char *zIn = zInput;` |
|      - | 4689 | `	const char *zPtr;` |
|      - | 4690 | `	int isEnc;` |
|      - | 4691 | `	/* Start processing */` |
|      8 | 4692 | `	for(;;){` |
|     17 | 4693 | `		if( zIn >= zEnd ){` |
|      - | 4694 | `			/* No more input to process */` |
|      5 | 4695 | `			break;` |
|      - | 4696 | `		}` |
|     13 | 4697 | `		isEnc = 0;` |
|     13 | 4698 | `		zPtr = zIn;` |
|      - | 4699 | `		/* Find the first delimiter */` |
|     27 | 4700 | `		while( zIn < zEnd ){` |
|     23 | 4701 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 4702 | `				/* Delimiter found,break imediately */` |
|      5 | 4703 | `				break;` |
|     15 | 4704 | `			}else if( zIn[0] == encl ){` |
|      - | 4705 | `				/* Inside enclosure? */` |
|    ! 0 | 4706 | `				isEnc = !isEnc;` |
|     15 | 4707 | `			}else if( zIn[0] == escape ){` |
|      - | 4708 | `				/* Escape sequence */` |
|    ! 0 | 4709 | `				zIn++;` |
|    ! 0 | 4710 | `			}` |
|      - | 4711 | `			/* Advance the cursor */` |
|     15 | 4712 | `			zIn++;` |
|      1 | 4713 | `		}` |
|     13 | 4714 | `		if( zIn > zPtr ){` |
|     13 | 4715 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 4716 | `			sxi32 rc;` |
|      - | 4717 | `			/* Invoke the supllied callback */` |
|     13 | 4718 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 4719 | `				zPtr++;` |
|    ! 0 | 4720 | `				nByteChunk-=2;` |
|    ! 0 | 4721 | `			}` |
|     13 | 4722 | `			if( nByteChunk > 0 ){` |
|     13 | 4723 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 4724 | `				if( rc == SXERR_ABORT ){` |
|      - | 4725 | `					/* User callback request an operation abort */` |
|    ! 0 | 4726 | `					break;` |
|      - | 4727 | `				}` |
|      6 | 4728 | `			}` |
|      6 | 4729 | `		}` |
|      - | 4730 | `		/* Ignore trailing delimiter */` |
|     21 | 4731 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 4732 | `			zIn++;` |
|      1 | 4733 | `		}` |
|      1 | 4734 | `	}` |
|      5 | 4735 | `	return SXRET_OK;` |
|      1 | 4736 |  |
|      - | 4737 | `/*` |
|      - | 4738 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 4739 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 4740 | ` * argument to this callback.` |
|      - | 4741 | ` */` |
|     12 | 4742 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 4743 |  |
|     13 | 4744 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 4745 | `	ph7_value sEntry;` |
|      - | 4746 | `	SyString sToken;` |
|      - | 4747 | `	/* Insert the token in the given array */` |
|     13 | 4748 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 4749 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 4750 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 4751 | `	if( sToken.nByte < 1){` |
|    ! 0 | 4752 | `		return SXRET_OK;` |
|      - | 4753 | `	}` |
|     13 | 4754 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 4755 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 4756 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 4757 | `	return SXRET_OK;` |
|      7 | 4758 |  |
|      - | 4759 | `/*` |
|      - | 4760 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 4761 | ` *  Parse a CSV string into an array.` |
|      - | 4762 | ` * Parameters` |
|      - | 4763 | ` *  $input` |
|      - | 4764 | ` *   The string to parse.` |
|      - | 4765 | ` *  $delimiter` |
|      - | 4766 | ` *   Set the field delimiter (one character only).` |
|      - | 4767 | ` *  $enclosure` |
|      - | 4768 | ` *   Set the field enclosure character (one character only).` |
|      - | 4769 | ` *  $escape` |
|      - | 4770 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 4771 | ` * Return` |
|      - | 4772 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 4773 | ` */` |
|      4 | 4774 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4775 |  |
|      - | 4776 | `	const char *zInput,*zPtr;` |
|      - | 4777 | `	ph7_value *pArray;` |
|      5 | 4778 | `	int delim  = ',';   /* Delimiter */` |
|      5 | 4779 | `	int encl   = '"' ;  /* Enclosure */` |
|      5 | 4780 | `	int escape = '\\';  /* Escape character */` |
|      - | 4781 | `	int nLen;` |
|      5 | 4782 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4783 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 4784 | `		ph7_result_null(pCtx);` |
|      3 | 4785 | `		return PH7_OK;` |
|      - | 4786 | `	}` |
|      - | 4787 | `	/* Extract the raw input */` |
|      3 | 4788 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4789 | `	if( nArg > 1 ){` |
|      - | 4790 | `		int i;` |
|      3 | 4791 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 4792 | `			/* Extract the delimiter */` |
|      3 | 4793 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 4794 | `			if( i > 0 ){` |
|      3 | 4795 | `				delim = zPtr[0];` |
|      1 | 4796 | `			}` |
|      1 | 4797 | `		}` |
|      3 | 4798 | `		if( nArg > 2 ){` |
|      3 | 4799 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 4800 | `				/* Extract the enclosure */` |
|      3 | 4801 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 4802 | `				if( i > 0 ){` |
|      3 | 4803 | `					encl = zPtr[0];` |
|      1 | 4804 | `				}` |
|      1 | 4805 | `			}` |
|      3 | 4806 | `			if( nArg > 3 ){` |
|      3 | 4807 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 4808 | `					/* Extract the escape character */` |
|      3 | 4809 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 4810 | `					if( i > 0 ){` |
|      3 | 4811 | `						escape = zPtr[0];` |
|      1 | 4812 | `					}` |
|      1 | 4813 | `				}` |
|      1 | 4814 | `			}` |
|      1 | 4815 | `		}` |
|      1 | 4816 | `	}` |
|      - | 4817 | `	/* Create our array */` |
|      3 | 4818 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4819 | `	if( pArray == 0 ){` |
|      - | 4820 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|    ! 0 | 4821 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4822 | `	}` |
|      - | 4823 | `	/* Parse the raw input */` |
|      3 | 4824 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 4825 | `	/* Return the freshly created array */` |
|      3 | 4826 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4827 | `	return PH7_OK;` |
|      3 | 4828 |  |
|      - | 4829 | `/*` |
|      - | 4830 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 4831 | ` * container.` |
|      - | 4832 | ` * Refer to [strip_tags()].` |
|      - | 4833 | ` */` |
|     10 | 4834 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 4835 |  |
|     11 | 4836 | `	const char *zEnd = &zTag[nByte];` |
|      - | 4837 | `	const char *zPtr;` |
|      - | 4838 | `	SyString sEntry;` |
|      - | 4839 | `	/* Strip tags */` |
|     10 | 4840 | `	for(;;){` |
|     45 | 4841 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 4842 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 4843 | `				zTag++;` |
|      1 | 4844 | `		}` |
|     21 | 4845 | `		if( zTag >= zEnd ){` |
|     11 | 4846 | `			break;` |
|      - | 4847 | `		}` |
|     11 | 4848 | `		zPtr = zTag;` |
|      - | 4849 | `		/* Delimit the tag */` |
|     25 | 4850 | `		while(zTag < zEnd ){` |
|     25 | 4851 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 4852 | `				/* UTF-8 stream */` |
|      3 | 4853 | `				zTag++;` |
|      5 | 4854 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 4855 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 4856 | `				break;` |
|    ! 0 | 4857 | `			}else{` |
|     13 | 4858 | `				zTag++;` |
|      - | 4859 | `			}` |
|      1 | 4860 | `		}` |
|     11 | 4861 | `		if( zTag > zPtr ){` |
|      - | 4862 | `			/* Perform the insertion */` |
|     11 | 4863 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 4864 | `			SyStringFullTrim(&sEntry);` |
|     11 | 4865 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 4866 | `		}` |
|      - | 4867 | `		/* Jump the trailing '>' */` |
|     11 | 4868 | `		zTag++;` |
|      1 | 4869 | `	}` |
|     11 | 4870 | `	return SXRET_OK;` |
|      1 | 4871 |  |
|      - | 4872 | `/*` |
|      - | 4873 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 4874 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 4875 | ` * Refer to [strip_tags()].` |
|      - | 4876 | ` */` |
|     36 | 4877 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 4878 |  |
|     37 | 4879 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 4880 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 4881 | `		SyString sTag;` |
|     85 | 4882 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 4883 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 4884 | `			zTag++;` |
|      1 | 4885 | `		}` |
|      - | 4886 | `		/* Delimit the tag */` |
|     25 | 4887 | `		zCur = zTag;` |
|     77 | 4888 | `		while(zTag < zEnd ){` |
|     77 | 4889 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 4890 | `				/* UTF-8 stream */` |
|      5 | 4891 | `				zTag++;` |
|      9 | 4892 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 4893 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 4894 | `				break;` |
|    ! 0 | 4895 | `			}else{` |
|     49 | 4896 | `				zTag++;` |
|      - | 4897 | `			}` |
|      1 | 4898 | `		}` |
|     25 | 4899 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 4900 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 4901 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 4902 | `		if( sTag.nByte > 0 ){` |
|      - | 4903 | `			SyString *aEntry,*pEntry;` |
|      - | 4904 | `			sxi32 rc;` |
|      - | 4905 | `			sxu32 n;` |
|      - | 4906 | `			/* Perform the lookup */` |
|     25 | 4907 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 4908 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 4909 | `				pEntry = &aEntry[n];` |
|      - | 4910 | `				/* Do the comparison */` |
|     25 | 4911 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 4912 | `				if( !rc ){` |
|     21 | 4913 | `					return SXRET_OK;` |
|      - | 4914 | `				}` |
|      3 | 4915 | `			}` |
|      2 | 4916 | `		}` |
|      2 | 4917 | `	}` |
|      - | 4918 | `	/* No such tag */` |
|     17 | 4919 | `	return SXERR_NOTFOUND;` |
|     19 | 4920 |  |
|      - | 4921 | `/*` |
|      - | 4922 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 4923 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 4924 | ` * Refer to [strip_tags()].` |
|      - | 4925 | ` */` |
|     16 | 4926 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 4927 |  |
|     17 | 4928 | `	const char *zEnd = &zIn[nByte];` |
|      - | 4929 | `	const char *zPtr,*zTag;` |
|      - | 4930 | `	SySet sSet;` |
|      - | 4931 | `	/* initialize the set of allowed tags */` |
|     17 | 4932 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 4933 | `	if( nTaglen > 0 ){` |
|      - | 4934 | `		/* Set of allowed tags */` |
|     11 | 4935 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 4936 | `	}` |
|      - | 4937 | `	/* Set the empty string */` |
|     17 | 4938 | `	ph7_result_string(pCtx,"",0);` |
|      - | 4939 | `	/* Start processing */` |
|     26 | 4940 | `	for(;;){` |
|     53 | 4941 | `		if(zIn >= zEnd){` |
|      - | 4942 | `			/* No more input to process */` |
|     15 | 4943 | `			break;` |
|      - | 4944 | `		}` |
|     39 | 4945 | `		zPtr = zIn;` |
|      - | 4946 | `		/* Find a tag */` |
|    133 | 4947 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 4948 | `			zIn++;` |
|      1 | 4949 | `		}` |
|     39 | 4950 | `		if( zIn > zPtr ){` |
|      - | 4951 | `			/* Consume raw input */` |
|     21 | 4952 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 4953 | `		}` |
|      - | 4954 | `		/* Ignore trailing null bytes */` |
|     39 | 4955 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 4956 | `			zIn++;` |
|    ! 0 | 4957 | `		}` |
|     39 | 4958 | `		if(zIn >= zEnd){` |
|      - | 4959 | `			/* No more input to process */` |
|      3 | 4960 | `			break;` |
|      - | 4961 | `		}` |
|     37 | 4962 | `		if( zIn[0] == '<' ){` |
|      - | 4963 | `			sxi32 rc;` |
|     37 | 4964 | `			zTag = zIn++;` |
|      - | 4965 | `			/* Delimit the tag */` |
|    127 | 4966 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 4967 | `				zIn++;` |
|      1 | 4968 | `			}` |
|     37 | 4969 | `			if( zIn < zEnd ){` |
|     37 | 4970 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 4971 | `			}` |
|      - | 4972 | `			/* Query the set */` |
|     37 | 4973 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 4974 | `			if( rc == SXRET_OK ){` |
|      - | 4975 | `				/* Keep the tag */` |
|     21 | 4976 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 4977 | `			}` |
|     18 | 4978 | `		}` |
|      1 | 4979 | `	}` |
|      - | 4980 | `	/* Cleanup */` |
|     17 | 4981 | `	SySetRelease(&sSet);` |
|     17 | 4982 | `	return SXRET_OK;` |
|      1 | 4983 |  |
|      - | 4984 | `/*` |
|      - | 4985 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 4986 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 4987 | ` * Parameters` |
|      - | 4988 | ` *  $str` |
|      - | 4989 | ` *  The input string.` |
|      - | 4990 | ` * $allowable_tags` |
|      - | 4991 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 4992 | ` * Return` |
|      - | 4993 | ` *  Returns the stripped string.` |
|      - | 4994 | ` */` |
|     16 | 4995 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4996 |  |
|     17 | 4997 | `	const char *zTaglist = 0;` |
|      - | 4998 | `	const char *zString;` |
|     17 | 4999 | `	int nTaglen = 0;` |
|      - | 5000 | `	int nLen;` |
|     17 | 5001 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5002 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 5003 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5004 | `		return PH7_OK;` |
|      - | 5005 | `	}` |
|      - | 5006 | `	/* Point to the raw string */` |
|     15 | 5007 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 5008 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 5009 | `		/* Allowed tag */` |
|     11 | 5010 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 5011 | `	}` |
|      - | 5012 | `	/* Process input */` |
|     15 | 5013 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 5014 | `	return PH7_OK;` |
|      9 | 5015 |  |
|      - | 5016 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5017 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5018 | `/*` |
|      - | 5019 | ` * string str_shuffle(string $str)` |
|      - | 5020 |  |
|      - | 5021 | ` *  Randomly shuffles a string.` |
|      - | 5022 | ` * Parameters` |
|      - | 5023 | ` *  $str` |
|      - | 5024 | ` *   The input string.` |
|      - | 5025 | ` * Return` |
|      - | 5026 | ` *  Returns the shuffled string.` |
|      - | 5027 | ` */` |
|     12 | 5028 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5029 |  |
|      - | 5030 | `	const char *zString;` |
|      - | 5031 | `	int nLen,i,c;` |
|      - | 5032 | `	sxu32 iR;` |
|     13 | 5033 | `	if( nArg < 1 ){` |
|      - | 5034 | `		/* Missing arguments,return the empty string */` |
|      3 | 5035 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5036 | `		return PH7_OK;` |
|      - | 5037 | `	}` |
|      - | 5038 | `	/* Extract the target string */` |
|     11 | 5039 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5040 | `	if( nLen < 1 ){` |
|      - | 5041 | `		/* Nothing to shuffle */` |
|      3 | 5042 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5043 | `		return PH7_OK;` |
|      - | 5044 | `	}` |
|      - | 5045 | `	/* Shuffle the string */` |
|     43 | 5046 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 5047 | `		/* Generate a random number first */` |
|     35 | 5048 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 5049 | `		/* Extract a random offset */` |
|     35 | 5050 | `		c = zString[iR % nLen];` |
|      - | 5051 | `		/* Append it */` |
|     35 | 5052 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 5053 | `	}` |
|      9 | 5054 | `	return PH7_OK;` |
|      7 | 5055 |  |
|      - | 5056 | `/*` |
|      - | 5057 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 5058 | ` *  Convert a string to an array.` |
|      - | 5059 | ` * Parameters` |
|      - | 5060 | ` * $string` |
|      - | 5061 | ` *  The input string.` |
|      - | 5062 | ` * $split_length` |
|      - | 5063 | ` *  Maximum length of the chunk.` |
|      - | 5064 | ` * Return` |
|      - | 5065 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 5066 | ` *  except possibly the last one which may be shorter.` |
|      - | 5067 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 5068 | ` *  as the first (and only) array element.` |
|      - | 5069 | ` *  An empty string returns an empty array.` |
|      - | 5070 | ` * Errors` |
|      - | 5071 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 5072 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 5073 | ` *  ValueError if $split_length is less than 1.` |
|      - | 5074 | ` */` |
|     28 | 5075 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 5076 |  |
|      - | 5077 | `	const char *zString,*zEnd;` |
|      - | 5078 | `	ph7_value *pArray,*pValue;` |
|      - | 5079 | `	int split_len;` |
|      - | 5080 | `	int nLen;` |
|     33 | 5081 | `	if( nArg < 1 ){` |
|      4 | 5082 | `		return PH7_VmThrowException(pCtx,` |
|      - | 5083 | `			"ArgumentCountError",` |
|      - | 5084 | `			"str_split() expects at least 1 argument, %d given",` |
|      1 | 5085 | `			nArg` |
|      - | 5086 | `			);` |
|      - | 5087 | `	}` |
|      - | 5088 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     50 | 5089 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     40 | 5090 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 5091 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 5092 | `		return PH7_VmThrowException(pCtx,` |
|      - | 5093 | `			"TypeError",` |
|      - | 5094 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 5095 | `			ph7_type_name(apArg[0])` |
|      - | 5096 | `			);` |
|      - | 5097 | `	}` |
|      - | 5098 | `	/* Point to the target string */` |
|     27 | 5099 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 5100 | `	split_len = (int)sizeof(char);` |
|     27 | 5101 | `	if( nArg > 1 ){` |
|      - | 5102 | `		/* Split length */` |
|     17 | 5103 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 5104 | `		if( split_len < 1 ){` |
|      6 | 5105 | `			return PH7_VmThrowException(pCtx,` |
|      - | 5106 | `				"ValueError",` |
|      - | 5107 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 5108 | `				);` |
|      - | 5109 | `		}` |
|     11 | 5110 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 5111 | `			split_len = nLen;` |
|      1 | 5112 | `		}` |
|      5 | 5113 | `	}` |
|      - | 5114 | `	/* Create the array and the scalar value */` |
|     21 | 5115 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 5116 | `	/*Chunk value */` |
|     21 | 5117 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 5118 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 5119 | `		/* Return FALSE */` |
|    ! 0 | 5120 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5121 | `		return PH7_OK;` |
|      - | 5122 | `	}` |
|      - | 5123 | `	/* Point to the end of the string */` |
|     21 | 5124 | `	zEnd = &zString[nLen];` |
|      - | 5125 | `	/* Perform the requested operation */` |
|     48 | 5126 | `	for(;;){` |
|      - | 5127 | `		int nMax;` |
|     59 | 5128 | `		if( zString >= zEnd ){` |
|      - | 5129 | `			/* No more input to process */` |
|     21 | 5130 | `			break;` |
|      - | 5131 | `		}` |
|     39 | 5132 | `		nMax = (int)(zEnd-zString);` |
|     39 | 5133 | `		if( nMax < split_len ){` |
|      3 | 5134 | `			split_len = nMax;` |
|      1 | 5135 | `		}` |
|      - | 5136 | `		/* Copy the current chunk */` |
|     39 | 5137 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 5138 | `		/* Insert it */` |
|     39 | 5139 | `		ph7_array_add_elem(pArray,0,pValue); /* Will make it's own copy */` |
|      - | 5140 | `		/* reset the string cursor */` |
|     39 | 5141 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 5142 | `		/* Update position */` |
|     39 | 5143 | `		zString += split_len;` |
|      1 | 5144 | `	}` |
|      - | 5145 | `	/*` |
|      - | 5146 | `	 * Return the array.` |
|      - | 5147 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 5148 | `	 * upon we return from this function.` |
|      - | 5149 | `	 */` |
|     21 | 5150 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 5151 | `	return PH7_OK;` |
|     19 | 5152 |  |
|      - | 5153 | `/*` |
|      - | 5154 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 5155 | ` * Refer to [strspn()].` |
|      - | 5156 | ` */` |
|     28 | 5157 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 5158 |  |
|     29 | 5159 | `	const char *zIn = *pzIn;` |
|      - | 5160 | `	const char *zPtr;` |
|      - | 5161 | `	/* Ignore leading white spaces */` |
|     29 | 5162 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 5163 | `		zIn++;` |
|    ! 0 | 5164 | `	}` |
|     29 | 5165 | `	if( zIn >= zEnd ){` |
|      - | 5166 | `		/* End of input */` |
|    ! 0 | 5167 | `		return SXERR_EOF;` |
|      - | 5168 | `	}` |
|     29 | 5169 | `	zPtr = zIn;` |
|      - | 5170 | `	/* Extract the token */` |
|    201 | 5171 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 5172 | `		zIn++;` |
|      1 | 5173 | `	}` |
|     29 | 5174 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5175 | `	/* Synchronize pointers */` |
|     29 | 5176 | `	*pzIn = zIn;` |
|      - | 5177 | `	/* Return to the caller */` |
|     29 | 5178 | `	return SXRET_OK;` |
|     15 | 5179 |  |
|      - | 5180 | `/*` |
|      - | 5181 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 5182 | ` * return the longest match.` |
|      - | 5183 | ` * Refer to [strspn()].` |
|      - | 5184 | ` */` |
|     18 | 5185 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5186 |  |
|     19 | 5187 | `	const char *zEnd = &zString[nLen];` |
|     19 | 5188 | `	const char *zIn = zString;` |
|      - | 5189 | `	int i,c;` |
|     45 | 5190 | `	for(;;){` |
|     91 | 5191 | `		if( zString >= zEnd ){` |
|      7 | 5192 | `			break;` |
|      - | 5193 | `		}` |
|      - | 5194 | `		/* Extract current character */` |
|     85 | 5195 | `		c = zString[0];` |
|      - | 5196 | `		/* Perform the lookup */` |
|    383 | 5197 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 5198 | `			if( c == zMask[i] ){` |
|      - | 5199 | `				/* Character found */` |
|     73 | 5200 | `				break;` |
|      - | 5201 | `			}` |
|    150 | 5202 | `		}` |
|     85 | 5203 | `		if( i >= nMaskLen ){` |
|      - | 5204 | `			/* Character not in the current mask,break immediately */` |
|     13 | 5205 | `			break;` |
|      - | 5206 | `		}` |
|      - | 5207 | `		/* Advance cursor */` |
|     73 | 5208 | `		zString++;` |
|      1 | 5209 | `	}` |
|      - | 5210 | `	/* Longest match */` |
|     19 | 5211 | `	return (int)(zString-zIn);` |
|      1 | 5212 |  |
|      - | 5213 | `/*` |
|      - | 5214 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 5215 | ` * Refer to [strcspn()].` |
|      - | 5216 | ` */` |
|     10 | 5217 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5218 |  |
|     11 | 5219 | `	const char *zEnd = &zString[nLen];` |
|     11 | 5220 | `	const char *zIn = zString;` |
|      - | 5221 | `	int i,c;` |
|     12 | 5222 | `	for(;;){` |
|     25 | 5223 | `		if( zString >= zEnd ){` |
|      3 | 5224 | `			break;` |
|      - | 5225 | `		}` |
|      - | 5226 | `		/* Extract current character */` |
|     23 | 5227 | `		c = zString[0];` |
|      - | 5228 | `		/* Perform the lookup */` |
|     51 | 5229 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 5230 | `			if( c == zMask[i] ){` |
|      9 | 5231 | `				break;` |
|      - | 5232 | `			}` |
|     15 | 5233 | `		}` |
|     23 | 5234 | `		if( i < nMaskLen ){` |
|      - | 5235 | `			/* Character in the current mask,break immediately */` |
|      9 | 5236 | `			break;` |
|      - | 5237 | `		}` |
|      - | 5238 | `		/* Advance cursor */` |
|     15 | 5239 | `		zString++;` |
|      1 | 5240 | `	}` |
|      - | 5241 | `	/* Longest match */` |
|     11 | 5242 | `	return (int)(zString-zIn);` |
|      1 | 5243 |  |
|      - | 5244 | `/*` |
|      - | 5245 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5246 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 5247 | ` *  of characters contained within a given mask.` |
|      - | 5248 | ` * Parameters` |
|      - | 5249 | ` * $str` |
|      - | 5250 | ` *  The input string.` |
|      - | 5251 | ` * $mask` |
|      - | 5252 | ` *  The list of allowable characters.` |
|      - | 5253 | ` * $start` |
|      - | 5254 | ` *  The position in subject to start searching.` |
|      - | 5255 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5256 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5257 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5258 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5259 | ` *  start'th position from the end of subject.` |
|      - | 5260 | ` * $length` |
|      - | 5261 | ` *  The length of the segment from subject to examine.` |
|      - | 5262 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5263 | ` *  characters after the starting position.` |
|      - | 5264 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5265 | ` *  position up to length characters from the end of subject.` |
|      - | 5266 | ` * Return` |
|      - | 5267 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 5268 | ` * in mask.` |
|      - | 5269 | ` */` |
|     26 | 5270 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5271 |  |
|      - | 5272 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5273 | `	int iMasklen,iLen;` |
|      - | 5274 | `	SyString sToken;` |
|     27 | 5275 | `	int iCount = 0;` |
|      - | 5276 | `	int rc;` |
|     27 | 5277 | `	if( nArg < 2 ){` |
|      - | 5278 | `		/* Missing agruments,return zero */` |
|      3 | 5279 | `		ph7_result_int(pCtx,0);` |
|      3 | 5280 | `		return PH7_OK;` |
|      - | 5281 | `	}` |
|      - | 5282 | `	/* Extract the target string */` |
|     25 | 5283 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5284 | `	/* Extract the mask */` |
|     25 | 5285 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 5286 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 5287 | `		/* Nothing to process,return zero */` |
|      7 | 5288 | `		ph7_result_int(pCtx,0);` |
|      7 | 5289 | `		return PH7_OK;` |
|      - | 5290 | `	}` |
|     19 | 5291 | `	if( nArg > 2 ){` |
|      - | 5292 | `		int nOfft;` |
|      - | 5293 | `		/* Extract the offset */` |
|      9 | 5294 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 5295 | `		if( nOfft < 0 ){` |
|    ! 0 | 5296 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5297 | `			if( zBase > zString ){` |
|    ! 0 | 5298 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5299 | `				zString = zBase;` |
|    ! 0 | 5300 | `			}else{` |
|      - | 5301 | `				/* Invalid offset */` |
|    ! 0 | 5302 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5303 | `				return PH7_OK;` |
|      - | 5304 | `			}` |
|    ! 0 | 5305 | `		}else{` |
|      9 | 5306 | `			if( nOfft >= iLen ){` |
|      - | 5307 | `				/* Invalid offset */` |
|    ! 0 | 5308 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5309 | `				return PH7_OK;` |
|    ! 0 | 5310 | `			}else{` |
|      - | 5311 | `				/* Update offset */` |
|      9 | 5312 | `				zString += nOfft;` |
|      9 | 5313 | `				iLen -= nOfft;` |
|      - | 5314 | `			}` |
|      - | 5315 | `		}` |
|      9 | 5316 | `		if( nArg > 3 ){` |
|      - | 5317 | `			int iUserlen;` |
|      - | 5318 | `			/* Extract the desired length */` |
|      9 | 5319 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 5320 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 5321 | `				iLen = iUserlen;` |
|      2 | 5322 | `			}` |
|      4 | 5323 | `		}` |
|      4 | 5324 | `	}` |
|      - | 5325 | `	/* Point to the end of the string */` |
|     19 | 5326 | `	zEnd = &zString[iLen];` |
|      - | 5327 | `	/* Extract the first non-space token */` |
|     19 | 5328 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 5329 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5330 | `		/* Compare against the current mask */` |
|     19 | 5331 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 5332 | `	}` |
|      - | 5333 | `	/* Longest match */` |
|     19 | 5334 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 5335 | `	return PH7_OK;` |
|     14 | 5336 |  |
|      - | 5337 | `/*` |
|      - | 5338 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5339 | ` *  Find length of initial segment not matching mask.` |
|      - | 5340 | ` * Parameters` |
|      - | 5341 | ` * $str` |
|      - | 5342 | ` *  The input string.` |
|      - | 5343 | ` * $mask` |
|      - | 5344 | ` *  The list of not allowed characters.` |
|      - | 5345 | ` * $start` |
|      - | 5346 | ` *  The position in subject to start searching.` |
|      - | 5347 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5348 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5349 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5350 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5351 | ` *  start'th position from the end of subject.` |
|      - | 5352 | ` * $length` |
|      - | 5353 | ` *  The length of the segment from subject to examine.` |
|      - | 5354 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5355 | ` *  characters after the starting position.` |
|      - | 5356 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5357 | ` *  position up to length characters from the end of subject.` |
|      - | 5358 | ` * Return` |
|      - | 5359 | ` *  Returns the length of the segment as an integer.` |
|      - | 5360 | ` */` |
|     16 | 5361 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5362 |  |
|      - | 5363 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5364 | `	int iMasklen,iLen;` |
|      - | 5365 | `	SyString sToken;` |
|     17 | 5366 | `	int iCount = 0;` |
|      - | 5367 | `	int rc;` |
|     17 | 5368 | `	if( nArg < 2 ){` |
|      - | 5369 | `		/* Missing agruments,return zero */` |
|      3 | 5370 | `		ph7_result_int(pCtx,0);` |
|      3 | 5371 | `		return PH7_OK;` |
|      - | 5372 | `	}` |
|      - | 5373 | `	/* Extract the target string */` |
|     15 | 5374 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5375 | `	/* Extract the mask */` |
|     15 | 5376 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 5377 | `	if( iLen < 1 ){` |
|      - | 5378 | `		/* Nothing to process,return zero */` |
|    ! 0 | 5379 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5380 | `		return PH7_OK;` |
|      - | 5381 | `	}` |
|     15 | 5382 | `	if( iMasklen < 1 ){` |
|      - | 5383 | `		/* No given mask,return the string length */` |
|      3 | 5384 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 5385 | `		return PH7_OK;` |
|      - | 5386 | `	}` |
|     13 | 5387 | `	if( nArg > 2 ){` |
|      - | 5388 | `		int nOfft;` |
|      - | 5389 | `		/* Extract the offset */` |
|     11 | 5390 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 5391 | `		if( nOfft < 0 ){` |
|    ! 0 | 5392 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5393 | `			if( zBase > zString ){` |
|    ! 0 | 5394 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5395 | `				zString = zBase;` |
|    ! 0 | 5396 | `			}else{` |
|      - | 5397 | `				/* Invalid offset */` |
|    ! 0 | 5398 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5399 | `				return PH7_OK;` |
|      - | 5400 | `			}` |
|    ! 0 | 5401 | `		}else{` |
|     11 | 5402 | `			if( nOfft >= iLen ){` |
|      - | 5403 | `				/* Invalid offset */` |
|      3 | 5404 | `				ph7_result_int(pCtx,0);` |
|      3 | 5405 | `				return PH7_OK;` |
|    ! 0 | 5406 | `			}else{` |
|      - | 5407 | `				/* Update offset */` |
|      9 | 5408 | `				zString += nOfft;` |
|      9 | 5409 | `				iLen -= nOfft;` |
|      - | 5410 | `			}` |
|      - | 5411 | `		}` |
|      9 | 5412 | `		if( nArg > 3 ){` |
|      - | 5413 | `			int iUserlen;` |
|      - | 5414 | `			/* Extract the desired length */` |
|    ! 0 | 5415 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 5416 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 5417 | `				iLen = iUserlen;` |
|    ! 0 | 5418 | `			}` |
|    ! 0 | 5419 | `		}` |
|      4 | 5420 | `	}` |
|      - | 5421 | `	/* Point to the end of the string */` |
|     11 | 5422 | `	zEnd = &zString[iLen];` |
|      - | 5423 | `	/* Extract the first non-space token */` |
|     11 | 5424 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 5425 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5426 | `		/* Compare against the current mask */` |
|     11 | 5427 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 5428 | `	}` |
|      - | 5429 | `	/* Longest match */` |
|     11 | 5430 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 5431 | `	return PH7_OK;` |
|      9 | 5432 |  |
|      - | 5433 | `/*` |
|      - | 5434 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 5435 | ` *  Search a string for any of a set of characters.` |
|      - | 5436 | ` * Parameters` |
|      - | 5437 | ` *  $haystack` |
|      - | 5438 | ` *   The string where char_list is looked for.` |
|      - | 5439 | ` *  $char_list` |
|      - | 5440 | ` *   This parameter is case sensitive.` |
|      - | 5441 | ` * Return` |
|      - | 5442 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 5443 | ` */` |
|      6 | 5444 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5445 |  |
|      - | 5446 | `	const char *zString,*zList,*zEnd;` |
|      - | 5447 | `	int iLen,iListLen,i,c;` |
|      - | 5448 | `	sxu32 nOfft,nMax;` |
|      - | 5449 | `	sxi32 rc;` |
|      7 | 5450 | `	if( nArg < 2 ){` |
|      - | 5451 | `		/* Missing arguments,return FALSE */` |
|      3 | 5452 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5453 | `		return PH7_OK;` |
|      - | 5454 | `	}` |
|      - | 5455 | `	/* Extract the haystack and the char list */` |
|      5 | 5456 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 5457 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 5458 | `	if( iLen < 1 ){` |
|      - | 5459 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 5460 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5461 | `		return PH7_OK;` |
|      - | 5462 | `	}` |
|      - | 5463 | `	/* Point to the end of the string */` |
|      5 | 5464 | `	zEnd = &zString[iLen];` |
|      5 | 5465 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 5466 | `	/* perform the requested operation */` |
|     15 | 5467 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 5468 | `		c = zList[i];` |
|     11 | 5469 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 5470 | `		if( rc == SXRET_OK ){` |
|      5 | 5471 | `			if( nMax < nOfft ){` |
|      3 | 5472 | `				nOfft = nMax;` |
|      1 | 5473 | `			}` |
|      2 | 5474 | `		}` |
|      6 | 5475 | `	}` |
|      5 | 5476 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 5477 | `		/* No such substring,return FALSE */` |
|      3 | 5478 | `		ph7_result_bool(pCtx,0);` |
|      2 | 5479 | `	}else{` |
|      - | 5480 | `		/* Return the substring */` |
|      3 | 5481 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 5482 | `	}` |
|      5 | 5483 | `	return PH7_OK;` |
|      4 | 5484 |  |
|      - | 5485 | `/* SPDX-SnippetBegin */` |
|      - | 5486 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 5487 | `/* SPDX-License-Identifier: blessing */` |
|      - | 5488 | `/*` |
|      - | 5489 | ` * string soundex(string $str)` |
|      - | 5490 | ` *  Calculate the soundex key of a string.` |
|      - | 5491 | ` * Parameters` |
|      - | 5492 | ` *  $str` |
|      - | 5493 | ` *   The input string.` |
|      - | 5494 | ` * Return` |
|      - | 5495 | ` *  Returns the soundex key as a string.` |
|      - | 5496 | ` * Note:` |
|      - | 5497 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 5498 | ` * source tree.` |
|      - | 5499 | ` */` |
|     20 | 5500 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5501 |  |
|      - | 5502 | `	const unsigned char *zIn;` |
|      - | 5503 | `	char zResult[8];` |
|      - | 5504 | `	int i, j;` |
|      - | 5505 | `	static const unsigned char iCode[] = {` |
|      - | 5506 |  |
|      - | 5507 |  |
|      - | 5508 |  |
|      - | 5509 |  |
|      - | 5510 |  |
|      - | 5511 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5512 |  |
|      - | 5513 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5514 | `	};` |
|     21 | 5515 | `	if( nArg < 1 ){` |
|      - | 5516 | `		/* Missing arguments,return the empty string */` |
|      3 | 5517 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5518 | `		return PH7_OK;` |
|      - | 5519 | `	}` |
|     19 | 5520 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     19 | 5521 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     19 | 5522 | `	if( zIn[i] ){` |
|     17 | 5523 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 5524 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 5525 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 5526 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 5527 | `			if( code>0 ){` |
|     45 | 5528 | `				if( code!=prevcode ){` |
|     33 | 5529 | `					prevcode = (unsigned char)code;` |
|     33 | 5530 | `					zResult[j++] = (char)code + '0';` |
|     16 | 5531 | `				}` |
|     23 | 5532 | `			}else{` |
|     49 | 5533 | `				prevcode = 0;` |
|      - | 5534 | `			}` |
|     47 | 5535 | `		}` |
|     33 | 5536 | `		while( j<4 ){` |
|     17 | 5537 | `			zResult[j++] = '0';` |
|      1 | 5538 | `		}` |
|     17 | 5539 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 5540 | `	}else{` |
|      3 | 5541 | `	  ph7_result_string(pCtx,"?000",4);` |
|      - | 5542 | `	}` |
|     19 | 5543 | `	return PH7_OK;` |
|     11 | 5544 |  |
|      - | 5545 | `/* SPDX-SnippetEnd */` |
|      - | 5546 | `/*` |
|      - | 5547 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 5548 | ` *  Wraps a string to a given number of characters.` |
|      - | 5549 | ` * Parameters` |
|      - | 5550 | ` *  $str` |
|      - | 5551 | ` *   The input string.` |
|      - | 5552 | ` * $width` |
|      - | 5553 | ` *  The column width.` |
|      - | 5554 | ` * $break` |
|      - | 5555 | ` *  The line is broken using the optional break parameter.` |
|      - | 5556 | ` * Return` |
|      - | 5557 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 5558 | ` */` |
|     14 | 5559 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5560 |  |
|      - | 5561 | `	const char *zIn,*zEnd,*zBreak;` |
|      - | 5562 | `	int iLen,iBreaklen,iChunk;` |
|     15 | 5563 | `	if( nArg < 1 ){` |
|      - | 5564 | `		/* Missing arguments,return the empty string */` |
|      3 | 5565 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5566 | `		return PH7_OK;` |
|      - | 5567 | `	}` |
|      - | 5568 | `	/* Extract the input string */` |
|     13 | 5569 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|     13 | 5570 | `	if( iLen < 1 ){` |
|      - | 5571 | `		/* Nothing to process,return the empty string */` |
|      3 | 5572 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5573 | `		return PH7_OK;` |
|      - | 5574 | `	}` |
|      - | 5575 | `	/* Chunk length */` |
|     11 | 5576 | `	iChunk = 75;` |
|     11 | 5577 | `	iBreaklen = 0;` |
|     11 | 5578 | `	zBreak = ""; /* cc warning */` |
|     11 | 5579 | `	if( nArg > 1 ){` |
|     11 | 5580 | `		iChunk = ph7_value_to_int(apArg[1]);` |
|     11 | 5581 | `		if( iChunk < 1 ){` |
|    ! 0 | 5582 | `			iChunk = 75;` |
|    ! 0 | 5583 | `		}` |
|     11 | 5584 | `		if( nArg > 2 ){` |
|      3 | 5585 | `			zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      1 | 5586 | `		}` |
|      5 | 5587 | `	}` |
|     11 | 5588 | `	if( iBreaklen < 1 ){` |
|      - | 5589 | `		/* Set a default column break */` |
|      - | 5590 | `#ifdef __WINNT__` |
|      1 | 5591 | `		zBreak = "\r\n";` |
|      1 | 5592 | `		iBreaklen = (int)sizeof("\r\n")-1;` |
|      - | 5593 | `#else` |
|      8 | 5594 | `		zBreak = "\n";` |
|      8 | 5595 | `		iBreaklen = (int)sizeof(char);` |
|      - | 5596 | `#endif` |
|      4 | 5597 | `	}` |
|      - | 5598 | `	/* Perform the requested operation */` |
|     11 | 5599 | `	zEnd = &zIn[iLen];` |
|     41 | 5600 | `	for(;;){` |
|      - | 5601 | `		int nMax;` |
|     47 | 5602 | `		if( zIn >= zEnd ){` |
|      - | 5603 | `			/* No more input to process */` |
|     11 | 5604 | `			break;` |
|      - | 5605 | `		}` |
|     37 | 5606 | `		nMax = (int)(zEnd-zIn);` |
|     37 | 5607 | `		if( iChunk > nMax ){` |
|     11 | 5608 | `			iChunk = nMax;` |
|      5 | 5609 | `		}` |
|      - | 5610 | `		/* Append the column first */` |
|     37 | 5611 | `		ph7_result_string(pCtx,zIn,iChunk); /* Will make it's own copy */` |
|      - | 5612 | `		/* Advance the cursor */` |
|     37 | 5613 | `		zIn += iChunk;` |
|     37 | 5614 | `		if( zIn < zEnd ){` |
|      - | 5615 | `			/* Append the line break */` |
|     27 | 5616 | `			ph7_result_string(pCtx,zBreak,iBreaklen);` |
|     13 | 5617 | `		}` |
|      1 | 5618 | `	}` |
|     11 | 5619 | `	return PH7_OK;` |
|      8 | 5620 |  |
|      - | 5621 | `/*` |
|      - | 5622 | ` * Check if the given character is a member of the given mask.` |
|      - | 5623 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 5624 | ` * Refer to [strtok()].` |
|      - | 5625 | ` */` |
|     30 | 5626 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 5627 |  |
|      - | 5628 | `	int i;` |
|     57 | 5629 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 5630 | `		if( c == zMask[i] ){` |
|     13 | 5631 | `			if( pOfft ){` |
|      5 | 5632 | `				*pOfft = i;` |
|      2 | 5633 | `			}` |
|     13 | 5634 | `			return TRUE;` |
|      - | 5635 | `		}` |
|     14 | 5636 | `	}` |
|     19 | 5637 | `	return FALSE;` |
|     16 | 5638 |  |
|      - | 5639 | `/*` |
|      - | 5640 | ` * Extract a single token from the input stream.` |
|      - | 5641 | ` * Refer to [strtok()].` |
|      - | 5642 | ` */` |
|      6 | 5643 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 5644 |  |
|      7 | 5645 | `	const char *zIn = *pzIn;` |
|      - | 5646 | `	const char *zPtr;` |
|      - | 5647 | `	/* Ignore leading delimiter */` |
|     11 | 5648 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5649 | `		zIn++;` |
|      1 | 5650 | `	}` |
|      7 | 5651 | `	if( zIn >= zEnd ){` |
|      - | 5652 | `		/* End of input */` |
|    ! 0 | 5653 | `		return SXERR_EOF;` |
|      - | 5654 | `	}` |
|      7 | 5655 | `	zPtr = zIn;` |
|      - | 5656 | `	/* Extract the token */` |
|     13 | 5657 | `	while( zIn < zEnd ){` |
|     11 | 5658 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 5659 | `			/* UTF-8 stream */` |
|    ! 0 | 5660 | `			zIn++;` |
|    ! 0 | 5661 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 5662 | `		}else{` |
|     11 | 5663 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5664 | `				break;` |
|      - | 5665 | `			}` |
|      7 | 5666 | `			zIn++;` |
|      - | 5667 | `		}` |
|      1 | 5668 | `	}` |
|      7 | 5669 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5670 | `	/* Update the cursor */` |
|      7 | 5671 | `	*pzIn = zIn;` |
|      - | 5672 | `	/* Return to the caller */` |
|      7 | 5673 | `	return SXRET_OK;` |
|      4 | 5674 |  |
|      - | 5675 | `/* strtok auxiliary private data */` |
|      - | 5676 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 5677 | `struct strtok_aux_data` |
|      - | 5678 |  |
|      - | 5679 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 5680 | `	const char *zIn;   /* Current input stream */` |
|      - | 5681 | `	const char *zEnd;  /* End of input */` |
|      - | 5682 | `};` |
|      - | 5683 | `/*` |
|      - | 5684 | ` * string strtok(string $str,string $token)` |
|      - | 5685 | ` * string strtok(string $token)` |
|      - | 5686 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 5687 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 5688 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 5689 | ` *  words by using the space character as the token.` |
|      - | 5690 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 5691 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 5692 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 5693 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 5694 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 5695 | ` *  the argument are found.` |
|      - | 5696 | ` * Parameters` |
|      - | 5697 | ` *  $str` |
|      - | 5698 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 5699 | ` * $token` |
|      - | 5700 | ` *  The delimiter used when splitting up str.` |
|      - | 5701 | ` * Return` |
|      - | 5702 | ` *   Current token or FALSE on EOF.` |
|      - | 5703 | ` */` |
|      8 | 5704 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5705 |  |
|      - | 5706 | `	strtok_aux_data *pAux;` |
|      - | 5707 | `	const char *zMask;` |
|      - | 5708 | `	SyString sToken;` |
|      - | 5709 | `	int nMasklen;` |
|      - | 5710 | `	sxi32 rc;` |
|      9 | 5711 | `	if( nArg < 2 ){` |
|      - | 5712 | `		/* Extract top aux data */` |
|      7 | 5713 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      7 | 5714 | `		if( pAux == 0 ){` |
|      - | 5715 | `			/* No aux data,return FALSE */` |
|    ! 0 | 5716 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5717 | `			return PH7_OK;` |
|      - | 5718 | `		}` |
|      7 | 5719 | `		nMasklen = 0;` |
|      7 | 5720 | `		zMask = ""; /* cc warning */` |
|      7 | 5721 | `		if( nArg > 0 ){` |
|      - | 5722 | `			/* Extract the mask */` |
|      5 | 5723 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 5724 | `		}` |
|      7 | 5725 | `		if( nMasklen < 1 ){` |
|      - | 5726 | `			/* Invalid mask,return FALSE */` |
|      3 | 5727 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|      3 | 5728 | `			ph7_context_free_chunk(pCtx,pAux);` |
|      3 | 5729 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|      3 | 5730 | `			ph7_result_bool(pCtx,0);` |
|      3 | 5731 | `			return PH7_OK;` |
|      - | 5732 | `		}` |
|      - | 5733 | `		/* Extract the token */` |
|      5 | 5734 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 5735 | `		if( rc != SXRET_OK ){` |
|      - | 5736 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 5737 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 5738 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5739 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 5740 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5741 | `		}else{` |
|      - | 5742 | `			/* Return the extracted token */` |
|      5 | 5743 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 5744 | `		}` |
|      3 | 5745 | `	}else{` |
|      - | 5746 | `		const char *zInput,*zCur;` |
|      - | 5747 | `		char *zDup;` |
|      - | 5748 | `		int nLen;` |
|      - | 5749 | `		/* Extract the raw input */` |
|      3 | 5750 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5751 | `		if( nLen < 1 ){` |
|      - | 5752 | `			/* Empty input,return FALSE */` |
|    ! 0 | 5753 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5754 | `			return PH7_OK;` |
|      - | 5755 | `		}` |
|      - | 5756 | `		/* Extract the mask */` |
|      3 | 5757 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 5758 | `		if( nMasklen < 1 ){` |
|      - | 5759 | `			/* Set a default mask */` |
|      - | 5760 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 5761 | `			zMask = TOK_MASK;` |
|    ! 0 | 5762 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 5763 | `#undef TOK_MASK` |
|    ! 0 | 5764 | `		}` |
|      - | 5765 | `		/* Extract a single token */` |
|      3 | 5766 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 5767 | `		if( rc != SXRET_OK ){` |
|      - | 5768 | `			/* Empty input */` |
|    ! 0 | 5769 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5770 | `			return PH7_OK;` |
|    ! 0 | 5771 | `		}else{` |
|      - | 5772 | `			/* Return the extracted token */` |
|      3 | 5773 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 5774 | `		}` |
|      - | 5775 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 5776 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 5777 | `		if( pAux ){` |
|      3 | 5778 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 5779 | `			if( nLen < 1 ){` |
|    ! 0 | 5780 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5781 | `				return PH7_OK;` |
|      - | 5782 | `			}` |
|      - | 5783 | `			/* Duplicate input */` |
|      3 | 5784 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 5785 | `			if( zDup  ){` |
|      3 | 5786 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 5787 | `				/* Register the aux data */` |
|      3 | 5788 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 5789 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 5790 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 5791 | `			}` |
|      1 | 5792 | `		}` |
|      - | 5793 | `	}` |
|      7 | 5794 | `	return PH7_OK;` |
|      5 | 5795 |  |
|      - | 5796 | `/*` |
|      - | 5797 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 5798 | ` *  Pad a string to a certain length with another string` |
|      - | 5799 | ` * Parameters` |
|      - | 5800 | ` *  $input` |
|      - | 5801 | ` *   The input string.` |
|      - | 5802 | ` * $pad_length` |
|      - | 5803 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 5804 | ` *   string, no padding takes place.` |
|      - | 5805 | ` * $pad_string` |
|      - | 5806 | ` *   Note:` |
|      - | 5807 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 5808 | ` *    divided by the pad_string's length.` |
|      - | 5809 | ` * $pad_type` |
|      - | 5810 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 5811 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 5812 | ` * Return` |
|      - | 5813 | ` *  The padded string.` |
|      - | 5814 | ` */` |
|     10 | 5815 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5816 |  |
|      - | 5817 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 5818 | `	const char *zIn,*zPad;` |
|     11 | 5819 | `	if( nArg < 2 ){` |
|      - | 5820 | `		/* Missing arguments,return the empty string */` |
|      5 | 5821 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 5822 | `		return PH7_OK;` |
|      - | 5823 | `	}` |
|      - | 5824 | `	/* Extract the target string */` |
|      7 | 5825 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5826 | `	/* Padding length */` |
|      7 | 5827 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|      7 | 5828 | `	if( iPadlen > 0 ){` |
|      5 | 5829 | `		iPadlen -= iLen;` |
|      2 | 5830 | `	}` |
|      7 | 5831 | `	if( iPadlen < 1  ){` |
|      - | 5832 | `		/* Return the string verbatim */` |
|      3 | 5833 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      3 | 5834 | `		return PH7_OK;` |
|      - | 5835 | `	}` |
|      5 | 5836 | `	zPad = " "; /* Whitespace padding */` |
|      5 | 5837 | `	iStrpad = (int)sizeof(char);` |
|      5 | 5838 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      5 | 5839 | `	if( nArg > 2 ){` |
|      - | 5840 | `		/* Padding string */` |
|      5 | 5841 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      5 | 5842 | `		if( iStrpad < 1 ){` |
|      - | 5843 | `			/* Empty string */` |
|    ! 0 | 5844 | `			zPad = " "; /* Whitespace padding */` |
|    ! 0 | 5845 | `			iStrpad = (int)sizeof(char);` |
|    ! 0 | 5846 | `		}` |
|      5 | 5847 | `		if( nArg > 3 ){` |
|      - | 5848 | `			/* Padd type */` |
|      5 | 5849 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 5850 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 5851 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 5852 | `			}` |
|      2 | 5853 | `		}` |
|      2 | 5854 | `	}` |
|      5 | 5855 | `	iDiv = 1;` |
|      5 | 5856 | `	if( iType == 2 ){` |
|    ! 0 | 5857 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 5858 | `	}` |
|      - | 5859 | `	/* Perform the requested operation */` |
|      5 | 5860 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 5861 | `		jPad = iStrpad;` |
|      5 | 5862 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 5863 | `			/* Padding */` |
|      5 | 5864 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 5865 | `				break;` |
|      - | 5866 | `			}` |
|      3 | 5867 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      2 | 5868 | `		}` |
|      3 | 5869 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 5870 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 5871 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 5872 | `				if( jPad > iStrpad ){` |
|    ! 0 | 5873 | `					jPad = iStrpad;` |
|    ! 0 | 5874 | `				}` |
|      3 | 5875 | `				if( jPad < 1){` |
|    ! 0 | 5876 | `					break;` |
|      - | 5877 | `				}` |
|      3 | 5878 | `				ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 5879 | `			}` |
|      1 | 5880 | `		}` |
|      1 | 5881 | `	}` |
|      5 | 5882 | `	if( iLen > 0 ){` |
|      - | 5883 | `		/* Append the input string */` |
|      5 | 5884 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      2 | 5885 | `	}` |
|      5 | 5886 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 5887 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 5888 | `			/* Padding */` |
|      5 | 5889 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 5890 | `				break;` |
|      - | 5891 | `			}` |
|      3 | 5892 | `			ph7_result_string(pCtx,zPad,iStrpad);` |
|      2 | 5893 | `		}` |
|      5 | 5894 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 5895 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 5896 | `			if( jPad > iStrpad ){` |
|    ! 0 | 5897 | `				jPad = iStrpad;` |
|    ! 0 | 5898 | `			}` |
|      3 | 5899 | `			if( jPad < 1){` |
|    ! 0 | 5900 | `				break;` |
|      - | 5901 | `			}` |
|      3 | 5902 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 5903 | `		}` |
|      1 | 5904 | `	}` |
|      5 | 5905 | `	return PH7_OK;` |
|      6 | 5906 |  |
|      - | 5907 | `/*` |
|      - | 5908 | ` * String replacement private data.` |
|      - | 5909 | ` */` |
|      - | 5910 | `typedef struct str_replace_data str_replace_data;` |
|      - | 5911 | `struct str_replace_data` |
|      - | 5912 |  |
|      - | 5913 | `	/* The following two fields are only used by the strtr function */` |
|      - | 5914 | `	SyBlob *pWorker;         /* Working buffer */` |
|      - | 5915 | `	ProcStringMatch xMatch;  /* Pattern match routine */` |
|      - | 5916 | `	/* The following two fields are only used by the str_replace function */` |
|      - | 5917 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 5918 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 5919 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 5920 | `};` |
|      - | 5921 | `/*` |
|      - | 5922 | ` * Remove a substring.` |
|      - | 5923 | ` */` |
|      - | 5924 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 5925 | `	for(;;){\` |
|      - | 5926 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 5927 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 5928 | `		++OFFT;\` |
|      - | 5929 | `	}\` |
|      - | 5930 |  |
|      - | 5931 | `/*` |
|      - | 5932 | ` * Shift right and insert algorithm.` |
|      - | 5933 | ` */` |
|      - | 5934 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 5935 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 5936 | `		for(;;){\` |
|      - | 5937 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 5938 | `			if(INLEN < 1 ) { break; }\` |
|      - | 5939 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 5940 | `			--INLEN; \` |
|      - | 5941 | `		}\` |
|      - | 5942 | `		for(;;){\` |
|      - | 5943 | `				if(ELEN < 1) { break; }\` |
|      - | 5944 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 5945 | `				OFFT++;\` |
|      - | 5946 | `				ENTRY++;\` |
|      - | 5947 | `				--ELEN;\` |
|      - | 5948 | `		}\` |
|      - | 5949 |  |
|      - | 5950 | `/*` |
|      - | 5951 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 5952 | ` * replacement string [i.e: zReplace].` |
|      - | 5953 | ` */` |
|     38 | 5954 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 5955 |  |
|     39 | 5956 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 5957 | `	sxu32 n,m;` |
|     39 | 5958 | `	n = SyBlobLength(pWorker);` |
|     39 | 5959 | `	m = nOfft;` |
|      - | 5960 | `	/* Delete the old entry */` |
|    475 | 5961 | `	STRDEL(zInput,n,m,nLen);` |
|     39 | 5962 | `	SyBlobLength(pWorker) -= nLen;` |
|     39 | 5963 | `	if( nReplen > 0 ){` |
|     33 | 5964 | `		sxi32 iRep = nReplen;` |
|      - | 5965 | `		sxi32 rc;` |
|      - | 5966 | `		/*` |
|      - | 5967 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 5968 | `		 * string.` |
|      - | 5969 | `		 */` |
|     33 | 5970 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     33 | 5971 | `		if( rc != SXRET_OK ){` |
|      - | 5972 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 5973 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 5974 | `			return rc;` |
|      - | 5975 | `		}` |
|      - | 5976 | `		/* Perform the insertion now */` |
|     33 | 5977 | `		zInput = (char *)SyBlobData(pWorker);` |
|     33 | 5978 | `		n = SyBlobLength(pWorker);` |
|    163 | 5979 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     33 | 5980 | `		SyBlobLength(pWorker) += nReplen;` |
|     16 | 5981 | `	}` |
|     39 | 5982 | `	return SXRET_OK;` |
|     20 | 5983 |  |
|      - | 5984 | `/*` |
|      - | 5985 | ` * String replacement walker callback.` |
|      - | 5986 | ` * The following callback is invoked for each array entry that hold` |
|      - | 5987 | ` * the replace string.` |
|      - | 5988 | ` * Refer to the strtr() implementation for more information.` |
|      - | 5989 | ` */` |
|      8 | 5990 | `static int StringReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 5991 |  |
|      9 | 5992 | `	str_replace_data *pRepData = (str_replace_data *)pUserData;` |
|      - | 5993 | `	const char *zTarget,*zReplace;` |
|      - | 5994 | `	SyBlob *pWorker;` |
|      - | 5995 | `	int tLen,nLen;` |
|      - | 5996 | `	sxu32 nOfft;` |
|      - | 5997 | `	sxi32 rc;` |
|      - | 5998 | `	/* Point to the working buffer */` |
|      9 | 5999 | `	pWorker = pRepData->pWorker;` |
|      9 | 6000 | `	if( !ph7_value_is_string(pKey) ){` |
|      - | 6001 | `		/* Target and replace must be a string */` |
|      3 | 6002 | `		return PH7_OK;` |
|      - | 6003 | `	}` |
|      - | 6004 | `	/* Extract the target and the replace */` |
|      7 | 6005 | `	zTarget = ph7_value_to_string(pKey,&tLen);` |
|      7 | 6006 | `	if( tLen < 1 ){` |
|      - | 6007 | `		/* Empty target,return immediately */` |
|    ! 0 | 6008 | `		return PH7_OK;` |
|      - | 6009 | `	}` |
|      - | 6010 | `	/* Perform a pattern search */` |
|      7 | 6011 | `	rc = pRepData->xMatch(SyBlobData(pWorker),SyBlobLength(pWorker),(const void *)zTarget,(sxu32)tLen,&nOfft);` |
|      7 | 6012 | `	if( rc != SXRET_OK ){` |
|      - | 6013 | `		/* Pattern not found */` |
|    ! 0 | 6014 | `		return PH7_OK;` |
|      - | 6015 | `	}` |
|      - | 6016 | `	/* Extract the replace string */` |
|      7 | 6017 | `	zReplace = ph7_value_to_string(pData,&nLen);` |
|      - | 6018 | `	/* Perform the replace process */` |
|      7 | 6019 | `	rc = StringReplace(pWorker,nOfft,tLen,zReplace,nLen);` |
|      7 | 6020 | `	if( rc != SXRET_OK ){` |
|      - | 6021 | `		/* Allocation failure: carry it out and stop the walk */` |
|    ! 0 | 6022 | `		pRepData->rc = rc;` |
|    ! 0 | 6023 | `		return rc;` |
|      - | 6024 | `	}` |
|      - | 6025 | `	/* All done */` |
|      7 | 6026 | `	return PH7_OK;` |
|      5 | 6027 |  |
|      - | 6028 | `/*` |
|      - | 6029 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 6030 | ` * to collect search/replace string.` |
|      - | 6031 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 6032 | ` */` |
|     26 | 6033 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6034 |  |
|     27 | 6035 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 6036 | `	SyString sWorker;` |
|      - | 6037 | `	const char *zIn;` |
|      - | 6038 | `	int nByte;` |
|      - | 6039 | `	/* Extract a string representation of the given argument */` |
|     27 | 6040 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 6041 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 6042 | `	if( nByte > 0 ){` |
|      - | 6043 | `		char *zDup;` |
|      - | 6044 | `		/* Duplicate the chunk */` |
|     25 | 6045 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 6046 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 6047 | `			);` |
|     25 | 6048 | `		if( zDup == 0 ){` |
|      - | 6049 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 6050 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 6051 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 6052 | `			return SXERR_MEM;` |
|      - | 6053 | `		}` |
|     25 | 6054 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 6055 | `		/* Save the chunk */` |
|     25 | 6056 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 6057 | `	}` |
|      - | 6058 | `	/* Save for later processing */` |
|     27 | 6059 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 6060 | `	/* All done */` |
|     13 | 6061 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 6062 | `	return PH7_OK;` |
|     14 | 6063 |  |
|      - | 6064 | `/*` |
|      - | 6065 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6066 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6067 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 6068 | ` * Parameters` |
|      - | 6069 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 6070 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 6071 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 6072 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 6073 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 6074 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 6075 | ` * $search` |
|      - | 6076 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 6077 | ` *  to designate multiple needles.` |
|      - | 6078 | ` * $replace` |
|      - | 6079 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 6080 | ` *  to designate multiple replacements.` |
|      - | 6081 | ` * $subject` |
|      - | 6082 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 6083 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 6084 | ` *  of subject, and the return value is an array as well.` |
|      - | 6085 | ` * $count (Not used)` |
|      - | 6086 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 6087 | ` * Return` |
|      - | 6088 | ` * This function returns a string or an array with the replaced values.` |
|      - | 6089 | ` */` |
|  22938 | 6090 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 6091 |  |
|      - | 6092 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 6093 | `	ProcStringMatch xMatch;` |
|      - | 6094 | `	const char *zIn,*zFunc;` |
|      - | 6095 | `	str_replace_data sRep;` |
|      - | 6096 | `	SyBlob sWorker;` |
|      - | 6097 | `	SySet sReplace;` |
|      - | 6098 | `	SySet sSearch;` |
|      - | 6099 | `	int rep_str;` |
|      - | 6100 | `	int nByte;` |
|      - | 6101 | `	sxi32 rc;` |
|  22943 | 6102 | `	if( nArg < 3 ){` |
|      - | 6103 | `		/* Missing/Invalid arguments,return null */` |
|      7 | 6104 | `		ph7_result_null(pCtx);` |
|      7 | 6105 | `		return PH7_OK;` |
|      - | 6106 | `	}` |
|      - | 6107 | `	/* Initialize fields */` |
|  22937 | 6108 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  22937 | 6109 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  22937 | 6110 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  22937 | 6111 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  22937 | 6112 | `	sRep.pCtx = pCtx;` |
|  22937 | 6113 | `	sRep.pCollector = &sSearch;` |
|  22937 | 6114 | `	rep_str = 0;` |
|      - | 6115 | `	/* Extract the subject */` |
|  22937 | 6116 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  22937 | 6117 | `	if( nByte < 1 ){` |
|      - | 6118 | `		/* Nothing to replace,return the empty string */` |
|     29 | 6119 | `		ph7_result_string(pCtx,"",0);` |
|     29 | 6120 | `		return PH7_OK;` |
|      - | 6121 | `	}` |
|      - | 6122 | `	/* Copy the subject */` |
|  22909 | 6123 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 6124 | `	/* Search string */` |
|  22909 | 6125 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 6126 | `		/* Collect search string */` |
|      9 | 6127 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 6128 | `	}else{` |
|      - | 6129 | `		/* Single pattern */` |
|  22901 | 6130 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  22901 | 6131 | `		if( nByte < 1 ){` |
|      - | 6132 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 6133 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 6134 | `			return PH7_OK;` |
|      - | 6135 | `		}` |
|  22897 | 6136 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6137 | `		/* Save for later processing */` |
|  22897 | 6138 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 6139 | `	}` |
|      - | 6140 | `	/* Replace string */` |
|  22905 | 6141 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 6142 | `		/* Collect replace string */` |
|      7 | 6143 | `		sRep.pCollector = &sReplace;` |
|      7 | 6144 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 6145 | `	}else{` |
|      - | 6146 | `		/* Single needle */` |
|  22899 | 6147 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  22899 | 6148 | `		rep_str = 1;` |
|  22899 | 6149 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6150 | `		/* Save for later processing */` |
|  22899 | 6151 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 6152 | `	}` |
|      - | 6153 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  22905 | 6154 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 6155 | `		SySetRelease(&sSearch);` |
|    ! 0 | 6156 | `		SySetRelease(&sReplace);` |
|    ! 0 | 6157 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 6158 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6159 | `	}` |
|      - | 6160 | `	/* Reset loop cursors */` |
|  22905 | 6161 | `	SySetResetCursor(&sSearch);` |
|  22905 | 6162 | `	SySetResetCursor(&sReplace);` |
|  22905 | 6163 | `	pReplace = pSearch = 0; /* cc warning */` |
|  22905 | 6164 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 6165 | `	/* Extract function name */` |
|  22905 | 6166 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 6167 | `	/* Set the default pattern match routine */` |
|  22905 | 6168 | `	xMatch = SyBlobSearch;` |
|  22905 | 6169 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 6170 | `		/* Case insensitive pattern match */` |
|     11 | 6171 | `		xMatch = iPatternMatch;` |
|      5 | 6172 | `	}` |
|      - | 6173 | `	/* Start the replace process */` |
|  45813 | 6174 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 6175 | `		sxu32 nCount,nOfft;` |
|  22913 | 6176 | `		if( pSearch->nByte <  1 ){` |
|      - | 6177 | `			/* Empty string,ignore */` |
|      3 | 6178 | `			continue;` |
|      - | 6179 | `		}` |
|      - | 6180 | `		/* Extract the replace string */` |
|  22911 | 6181 | `		if( rep_str ){` |
|  22901 | 6182 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  11453 | 6183 | `		}else{` |
|     11 | 6184 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 6185 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 6186 | `				 * An empty string is used for the rest of replacement values` |
|      - | 6187 | `				 */` |
|      3 | 6188 | `				pReplace = 0;` |
|      1 | 6189 | `			}` |
|      - | 6190 | `		}` |
|  22911 | 6191 | `		if( pReplace == 0 ){` |
|      - | 6192 | `			/* Use an empty string instead */` |
|      3 | 6193 | `			pReplace = &sTemp;` |
|      1 | 6194 | `		}` |
|  22911 | 6195 | `		nOfft = nCount = 0;` |
|  11469 | 6196 | `		for(;;){` |
|  22943 | 6197 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 6198 | `				break;` |
|      - | 6199 | `			}` |
|      - | 6200 | `			/* Perform a pattern lookup */` |
|  34394 | 6201 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  22926 | 6202 | `				pSearch->nByte,&nOfft);` |
|  22931 | 6203 | `			if( rc != SXRET_OK ){` |
|      - | 6204 | `				/* Pattern not found */` |
|  22899 | 6205 | `				break;` |
|      - | 6206 | `			}` |
|      - | 6207 | `			/* Perform the replace operation */` |
|     33 | 6208 | `			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|     33 | 6209 | `			if( rc != SXRET_OK ){` |
|      - | 6210 | `				/* Allocation failure: surface a fatal instead of a partial result */` |
|    ! 0 | 6211 | `				SySetRelease(&sSearch);` |
|    ! 0 | 6212 | `				SySetRelease(&sReplace);` |
|    ! 0 | 6213 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 6214 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 6215 | `			}` |
|      - | 6216 | `			/* Increment offset counter */` |
|     33 | 6217 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 6218 | `		}` |
|      5 | 6219 | `	}` |
|      - | 6220 | `	/* All done,clean-up the mess left behind */` |
|  22905 | 6221 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  22905 | 6222 | `	SySetRelease(&sSearch);` |
|  22905 | 6223 | `	SySetRelease(&sReplace);` |
|  22905 | 6224 | `	SyBlobRelease(&sWorker);` |
|  22905 | 6225 | `	if( rc != PH7_OK ){` |
|    ! 0 | 6226 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6227 | `	}` |
|  22905 | 6228 | `	return PH7_OK;` |
|  11474 | 6229 |  |
|      - | 6230 | `/*` |
|      - | 6231 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 6232 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 6233 | ` *  Translate characters or replace substrings.` |
|      - | 6234 | ` * Parameters` |
|      - | 6235 | ` *  $str` |
|      - | 6236 | ` *  The string being translated.` |
|      - | 6237 | ` * $from` |
|      - | 6238 | ` *  The string being translated to to.` |
|      - | 6239 | ` * $to` |
|      - | 6240 | ` *  The string replacing from.` |
|      - | 6241 | ` * $replace_pairs` |
|      - | 6242 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 6243 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 6244 | ` * Return` |
|      - | 6245 | ` *  The translated string.` |
|      - | 6246 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 6247 | ` */` |
|     12 | 6248 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6249 |  |
|      - | 6250 | `	const char *zIn;` |
|      - | 6251 | `	int nLen;` |
|     13 | 6252 | `	if( nArg < 1 ){` |
|      - | 6253 | `		/* Nothing to replace,return FALSE */` |
|      7 | 6254 | `		ph7_result_bool(pCtx,0);` |
|      7 | 6255 | `		return PH7_OK;` |
|      - | 6256 | `	}` |
|      7 | 6257 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 6258 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 6259 | `		/* Invalid arguments */` |
|    ! 0 | 6260 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6261 | `		return PH7_OK;` |
|      - | 6262 | `	}` |
|      9 | 6263 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 6264 | `		str_replace_data sRepData;` |
|      - | 6265 | `		SyBlob sWorker;` |
|      - | 6266 | `		sxi32 rc;` |
|      - | 6267 | `		/* Initilaize the working buffer */` |
|      5 | 6268 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|      - | 6269 | `		/* Copy raw string */` |
|      5 | 6270 | `		SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nLen);` |
|      - | 6271 | `		/* Init our replace data instance */` |
|      5 | 6272 | `		sRepData.pWorker = &sWorker;` |
|      5 | 6273 | `		sRepData.xMatch = SyBlobSearch;` |
|      5 | 6274 | `		sRepData.rc = SXRET_OK;` |
|      - | 6275 | `		/* Iterate throw array entries and perform the replace operation.*/` |
|      5 | 6276 | `		ph7_array_walk(apArg[1],StringReplaceWalker,&sRepData);` |
|      5 | 6277 | `		if( sRepData.rc != SXRET_OK ){` |
|      - | 6278 | `			/* Allocation failure during replacement: surface a fatal */` |
|    ! 0 | 6279 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 6280 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6281 | `		}` |
|      - | 6282 | `		/* All done, return the result string */` |
|      7 | 6283 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|      4 | 6284 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 6285 | `		/* Clean-up */` |
|      5 | 6286 | `		SyBlobRelease(&sWorker);` |
|      5 | 6287 | `		if( rc != PH7_OK ){` |
|    ! 0 | 6288 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6289 | `		}` |
|      3 | 6290 | `	}else{` |
|      - | 6291 | `		int i,flen,tlen,c,iOfft;` |
|      - | 6292 | `		const char *zFrom,*zTo;` |
|      3 | 6293 | `		if( nArg < 3 ){` |
|      - | 6294 | `			/* Nothing to replace */` |
|    ! 0 | 6295 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6296 | `			return PH7_OK;` |
|      - | 6297 | `		}` |
|      - | 6298 | `		/* Extract given arguments */` |
|      3 | 6299 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 6300 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 6301 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 6302 | `			/* Nothing to replace */` |
|    ! 0 | 6303 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6304 | `			return PH7_OK;` |
|      - | 6305 | `		}` |
|      - | 6306 | `		/* Start the replace process */` |
|     13 | 6307 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 6308 | `			c = zIn[i];` |
|     11 | 6309 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 6310 | `				if ( iOfft < tlen ){` |
|      5 | 6311 | `					c = zTo[iOfft];` |
|      2 | 6312 | `				}` |
|      2 | 6313 | `			}` |
|     11 | 6314 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 6315 |  |
|      6 | 6316 | `		}` |
|      - | 6317 | `	}` |
|      7 | 6318 | `	return PH7_OK;` |
|      7 | 6319 |  |
|      - | 6320 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6321 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6322 | `/*` |
|      - | 6323 | ` * Parse an INI string.` |
|      - | 6324 |  |
|      - | 6325 | ` * According to wikipedia` |
|      - | 6326 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 6327 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 6328 | ` *  Format` |
|      - | 6329 | `*    Properties` |
|      - | 6330 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 6331 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 6332 | `*     Example:` |
|      - | 6333 | `*      name=value` |
|      - | 6334 | `*    Sections` |
|      - | 6335 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 6336 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 6337 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 6338 | `*     or the end of the file. Sections may not be nested.` |
|      - | 6339 | `*     Example:` |
|      - | 6340 | `*      [section]` |
|      - | 6341 | `*   Comments` |
|      - | 6342 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 6343 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 6344 | `*/` |
|     12 | 6345 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 6346 |  |
|      - | 6347 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 6348 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 6349 | `	SyHashEntry *pEntry;` |
|      - | 6350 | `	SyString sEntry;` |
|      - | 6351 | `	SyHash sHash;` |
|      - | 6352 | `	int c;` |
|      - | 6353 | `	/* Create an empty array and worker variables */` |
|     13 | 6354 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 6355 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 6356 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 6357 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 6358 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|    ! 0 | 6359 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6360 | `	}` |
|     13 | 6361 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 6362 | `	pCur = pArray;` |
|      - | 6363 | `	/* Start the parse process */` |
|     21 | 6364 | `	for(;;){` |
|      - | 6365 | `		/* Ignore leading white spaces */` |
|     69 | 6366 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 6367 | `			zIn++;` |
|      1 | 6368 | `		}` |
|     43 | 6369 | `		if( zIn >= zEnd ){` |
|      - | 6370 | `			/* No more input to process */` |
|     13 | 6371 | `			break;` |
|      - | 6372 | `		}` |
|     31 | 6373 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6374 | `			/* Comment til the end of line */` |
|    ! 0 | 6375 | `			zIn++;` |
|    ! 0 | 6376 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 6377 | `				zIn++;` |
|    ! 0 | 6378 | `			}` |
|    ! 0 | 6379 | `			continue;` |
|      - | 6380 | `		}` |
|      - | 6381 | `		/* Reset the string cursor of the working variable */` |
|     31 | 6382 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 6383 | `		if( zIn[0] == '[' ){` |
|      - | 6384 | `			/* Section: Extract the section name */` |
|      9 | 6385 | `			zIn++;` |
|      9 | 6386 | `			zCur = zIn;` |
|     73 | 6387 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 6388 | `				zIn++;` |
|      1 | 6389 | `			}` |
|      9 | 6390 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 6391 | `				/* Save the section name */` |
|      5 | 6392 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 6393 | `				SyStringFullTrim(&sEntry);` |
|      5 | 6394 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 6395 | `				if( sEntry.nByte > 0 ){` |
|      - | 6396 | `					/* Associate an array with the section */` |
|      5 | 6397 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 6398 | `					if( pSection ){` |
|      5 | 6399 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 6400 | `						pCur = pSection;` |
|      2 | 6401 | `					}` |
|      2 | 6402 | `				}` |
|      2 | 6403 | `			}` |
|      9 | 6404 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 6405 | `		}else{` |
|      - | 6406 | `			ph7_value *pOldCur;` |
|      - | 6407 | `			int is_array;` |
|      - | 6408 | `			int iLen;` |
|      - | 6409 | `			/* Properties */` |
|     23 | 6410 | `			is_array = 0;` |
|     23 | 6411 | `			zCur = zIn;` |
|     23 | 6412 | `			iLen = 0; /* cc warning */` |
|     23 | 6413 | `			pOldCur = pCur;` |
|    155 | 6414 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 6415 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 6416 | `					/* Array */` |
|    ! 0 | 6417 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 6418 | `					is_array = 1;` |
|    ! 0 | 6419 | `					if( iLen > 0 ){` |
|    ! 0 | 6420 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 6421 | `						/* Query the hashtable */` |
|    ! 0 | 6422 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 6423 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 6424 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 6425 | `						if( pEntry ){` |
|    ! 0 | 6426 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 6427 | `						}else{` |
|      - | 6428 | `							/* Create an empty array */` |
|    ! 0 | 6429 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 6430 | `							if( pvArr ){` |
|      - | 6431 | `								/* Save the entry */` |
|    ! 0 | 6432 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 6433 | `								/* Insert the entry */` |
|    ! 0 | 6434 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6435 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 6436 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 6437 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6438 | `							}` |
|      - | 6439 | `						}` |
|    ! 0 | 6440 | `						if( pvArr ){` |
|    ! 0 | 6441 | `							pCur = pvArr;` |
|    ! 0 | 6442 | `						}` |
|    ! 0 | 6443 | `					}` |
|    ! 0 | 6444 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 6445 | `						zIn++;` |
|    ! 0 | 6446 | `					}` |
|    ! 0 | 6447 | `				}` |
|    133 | 6448 | `				zIn++;` |
|      1 | 6449 | `			}` |
|     23 | 6450 | `			if( !is_array ){` |
|     23 | 6451 | `				iLen = (int)(zIn-zCur);` |
|     11 | 6452 | `			}` |
|      - | 6453 | `			/* Trim the key */` |
|     23 | 6454 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 6455 | `			SyStringFullTrim(&sEntry);` |
|     23 | 6456 | `			if( sEntry.nByte > 0 ){` |
|     23 | 6457 | `				if( !is_array ){` |
|      - | 6458 | `					/* Save the key name */` |
|     23 | 6459 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 6460 | `				}` |
|      - | 6461 | `				/* extract key value */` |
|     23 | 6462 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 6463 | `				zIn++; /* '=' */` |
|     39 | 6464 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 6465 | `					zIn++;` |
|      1 | 6466 | `				}` |
|     23 | 6467 | `				if( zIn < zEnd ){` |
|     21 | 6468 | `					zCur = zIn;` |
|     21 | 6469 | `					c = zIn[0];` |
|     21 | 6470 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6471 | `						zIn++;` |
|      - | 6472 | `						/* Delimit the value */` |
|    ! 0 | 6473 | `						while( zIn < zEnd ){` |
|    ! 0 | 6474 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 6475 | `								break;` |
|      - | 6476 | `							}` |
|    ! 0 | 6477 | `							zIn++;` |
|    ! 0 | 6478 | `						}` |
|    ! 0 | 6479 | `						if( zIn < zEnd ){` |
|    ! 0 | 6480 | `							zIn++;` |
|    ! 0 | 6481 | `						}` |
|    ! 0 | 6482 | `					}else{` |
|    125 | 6483 | `						while( zIn < zEnd ){` |
|    123 | 6484 | `							if( zIn[0] == '\n' ){` |
|     19 | 6485 | `								if( zIn[-1] != '\\' ){` |
|     19 | 6486 | `									break;` |
|    ! 0 | 6487 | `								}` |
|    105 | 6488 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6489 | `								/* Inline comments */` |
|    ! 0 | 6490 | `								break;` |
|      - | 6491 | `							}` |
|    105 | 6492 | `							zIn++;` |
|      1 | 6493 | `						}` |
|      - | 6494 | `					}` |
|      - | 6495 | `					/* Trim the value */` |
|     21 | 6496 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 6497 | `					SyStringFullTrim(&sEntry);` |
|     21 | 6498 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6499 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 6500 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 6501 | `					}` |
|     21 | 6502 | `					if( sEntry.nByte > 0 ){` |
|     21 | 6503 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 6504 | `					}` |
|      - | 6505 | `					/* Insert the key and it's value */` |
|     21 | 6506 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 6507 | `				}` |
|     12 | 6508 | `			}else{` |
|    ! 0 | 6509 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 6510 | `					zIn++;` |
|    ! 0 | 6511 | `				}` |
|      - | 6512 | `			}` |
|     23 | 6513 | `			pCur = pOldCur;` |
|      - | 6514 | `		}` |
|      1 | 6515 | `	}` |
|     13 | 6516 | `	SyHashRelease(&sHash);` |
|      - | 6517 | `	/* Return the parse of the INI string */` |
|     13 | 6518 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 6519 | `	return SXRET_OK;` |
|      7 | 6520 |  |
|      - | 6521 | `/*` |
|      - | 6522 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 6523 | ` *  Parse a configuration string.` |
|      - | 6524 | ` * Parameters` |
|      - | 6525 | ` *  $ini` |
|      - | 6526 | ` *   The contents of the ini file being parsed.` |
|      - | 6527 | ` *  $process_sections` |
|      - | 6528 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 6529 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 6530 | ` *  $scanner_mode (Not used)` |
|      - | 6531 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 6532 | ` *   then option values will not be parsed.` |
|      - | 6533 | ` * Return` |
|      - | 6534 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 6535 | ` */` |
|     10 | 6536 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6537 |  |
|      - | 6538 | `	const char *zIni;` |
|      - | 6539 | `	int nByte;` |
|     11 | 6540 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6541 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 6542 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6543 | `		return PH7_OK;` |
|      - | 6544 | `	}` |
|      - | 6545 | `	/* Extract the raw INI buffer */` |
|     11 | 6546 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 6547 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|     11 | 6548 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      6 | 6549 |  |
|      - | 6550 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6551 |  |
|      - | 6552 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6553 |  |
|      - | 6554 | `/*` |
|      - | 6555 | ` * Ctype Functions.` |
|      - | 6556 | ` * Status:` |
|      - | 6557 | ` *    Stable.` |
|      - | 6558 | ` */` |
|      - | 6559 | `/*` |
|      - | 6560 | ` * bool ctype_alnum(string $text)` |
|      - | 6561 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 6562 | ` * Parameters` |
|      - | 6563 | ` *  $text` |
|      - | 6564 | ` *   The tested string.` |
|      - | 6565 | ` * Return` |
|      - | 6566 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 6567 | ` */` |
|     16 | 6568 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6569 |  |
|      - | 6570 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6571 | `	int nLen;` |
|     17 | 6572 | `	if( nArg < 1 ){` |
|      - | 6573 | `		/* Missing arguments,return FALSE */` |
|      3 | 6574 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6575 | `		return PH7_OK;` |
|      - | 6576 | `	}` |
|      - | 6577 | `	/* Extract the target string */` |
|     15 | 6578 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 6579 | `	zEnd = &zIn[nLen];` |
|     15 | 6580 | `	if( nLen < 1 ){` |
|      - | 6581 | `		/* Empty string,return FALSE */` |
|      3 | 6582 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6583 | `		return PH7_OK;` |
|      - | 6584 | `	}` |
|      - | 6585 | `	/* Perform the requested operation */` |
|     32 | 6586 | `	for(;;){` |
|     65 | 6587 | `		if( zIn >= zEnd ){` |
|      - | 6588 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6589 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6590 | `			return PH7_OK;` |
|      - | 6591 | `		}` |
|     57 | 6592 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 6593 | `			break;` |
|      - | 6594 | `		}` |
|      - | 6595 | `		/* Point to the next character */` |
|     53 | 6596 | `		zIn++;` |
|      1 | 6597 | `	}` |
|      - | 6598 | `	/* The test failed,return FALSE */` |
|      5 | 6599 | `	ph7_result_bool(pCtx,0);` |
|      5 | 6600 | `	return PH7_OK;` |
|      9 | 6601 |  |
|      - | 6602 | `/*` |
|      - | 6603 | ` * bool ctype_alpha(string $text)` |
|      - | 6604 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 6605 | ` * Parameters` |
|      - | 6606 | ` *  $text` |
|      - | 6607 | ` *   The tested string.` |
|      - | 6608 | ` * Return` |
|      - | 6609 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 6610 | ` */` |
|     18 | 6611 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6612 |  |
|      - | 6613 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6614 | `	int nLen;` |
|     19 | 6615 | `	if( nArg < 1 ){` |
|      - | 6616 | `		/* Missing arguments,return FALSE */` |
|      3 | 6617 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6618 | `		return PH7_OK;` |
|      - | 6619 | `	}` |
|      - | 6620 | `	/* Extract the target string */` |
|     17 | 6621 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6622 | `	zEnd = &zIn[nLen];` |
|     17 | 6623 | `	if( nLen < 1 ){` |
|      - | 6624 | `		/* Empty string,return FALSE */` |
|      3 | 6625 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6626 | `		return PH7_OK;` |
|      - | 6627 | `	}` |
|      - | 6628 | `	/* Perform the requested operation */` |
|     42 | 6629 | `	for(;;){` |
|     85 | 6630 | `		if( zIn >= zEnd ){` |
|      - | 6631 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6632 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6633 | `			return PH7_OK;` |
|      - | 6634 | `		}` |
|     77 | 6635 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 6636 | `			break;` |
|      - | 6637 | `		}` |
|      - | 6638 | `		/* Point to the next character */` |
|     71 | 6639 | `		zIn++;` |
|      1 | 6640 | `	}` |
|      - | 6641 | `	/* The test failed,return FALSE */` |
|      7 | 6642 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6643 | `	return PH7_OK;` |
|     10 | 6644 |  |
|      - | 6645 | `/*` |
|      - | 6646 | ` * bool ctype_cntrl(string $text)` |
|      - | 6647 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 6648 | ` * Parameters` |
|      - | 6649 | ` *  $text` |
|      - | 6650 | ` *   The tested string.` |
|      - | 6651 | ` * Return` |
|      - | 6652 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 6653 | ` */` |
|     18 | 6654 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6655 |  |
|      - | 6656 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6657 | `	int nLen;` |
|     19 | 6658 | `	if( nArg < 1 ){` |
|      - | 6659 | `		/* Missing arguments,return FALSE */` |
|      3 | 6660 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6661 | `		return PH7_OK;` |
|      - | 6662 | `	}` |
|      - | 6663 | `	/* Extract the target string */` |
|     17 | 6664 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6665 | `	zEnd = &zIn[nLen];` |
|     17 | 6666 | `	if( nLen < 1 ){` |
|      - | 6667 | `		/* Empty string,return FALSE */` |
|      3 | 6668 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6669 | `		return PH7_OK;` |
|      - | 6670 | `	}` |
|      - | 6671 | `	/* Perform the requested operation */` |
|     14 | 6672 | `	for(;;){` |
|     29 | 6673 | `		if( zIn >= zEnd ){` |
|      - | 6674 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6675 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6676 | `			return PH7_OK;` |
|      - | 6677 | `		}` |
|     21 | 6678 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6679 | `			/* UTF-8 stream  */` |
|    ! 0 | 6680 | `			break;` |
|      - | 6681 | `		}` |
|     21 | 6682 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 6683 | `			break;` |
|      - | 6684 | `		}` |
|      - | 6685 | `		/* Point to the next character */` |
|     15 | 6686 | `		zIn++;` |
|      1 | 6687 | `	}` |
|      - | 6688 | `	/* The test failed,return FALSE */` |
|      7 | 6689 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6690 | `	return PH7_OK;` |
|     10 | 6691 |  |
|      - | 6692 | `/*` |
|      - | 6693 | ` * bool ctype_digit(string $text)` |
|      - | 6694 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 6695 | ` * Parameters` |
|      - | 6696 | ` *  $text` |
|      - | 6697 | ` *   The tested string.` |
|      - | 6698 | ` * Return` |
|      - | 6699 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 6700 | ` */` |
|   1620 | 6701 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 6702 |  |
|      - | 6703 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6704 | `	int nLen;` |
|   1625 | 6705 | `	if( nArg < 1 ){` |
|      - | 6706 | `		/* Missing arguments,return FALSE */` |
|      3 | 6707 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6708 | `		return PH7_OK;` |
|      - | 6709 | `	}` |
|      - | 6710 | `	/* Extract the target string */` |
|   1623 | 6711 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1623 | 6712 | `	zEnd = &zIn[nLen];` |
|   1623 | 6713 | `	if( nLen < 1 ){` |
|      - | 6714 | `		/* Empty string,return FALSE */` |
|      3 | 6715 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6716 | `		return PH7_OK;` |
|      - | 6717 | `	}` |
|      - | 6718 | `	/* Perform the requested operation */` |
|   1521 | 6719 | `	for(;;){` |
|   3047 | 6720 | `		if( zIn >= zEnd ){` |
|      - | 6721 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1381 | 6722 | `			ph7_result_bool(pCtx,1);` |
|   1381 | 6723 | `			return PH7_OK;` |
|      - | 6724 | `		}` |
|   1671 | 6725 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6726 | `			/* UTF-8 stream  */` |
|    ! 0 | 6727 | `			break;` |
|      - | 6728 | `		}` |
|   1671 | 6729 | `		if( !SyisDigit(zIn[0]) ){` |
|    245 | 6730 | `			break;` |
|      - | 6731 | `		}` |
|      - | 6732 | `		/* Point to the next character */` |
|   1431 | 6733 | `		zIn++;` |
|      5 | 6734 | `	}` |
|      - | 6735 | `	/* The test failed,return FALSE */` |
|    245 | 6736 | `	ph7_result_bool(pCtx,0);` |
|    245 | 6737 | `	return PH7_OK;` |
|    815 | 6738 |  |
|      - | 6739 | `/*` |
|      - | 6740 | ` * bool ctype_xdigit(string $text)` |
|      - | 6741 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 6742 | ` * Parameters` |
|      - | 6743 | ` *  $text` |
|      - | 6744 | ` *   The tested string.` |
|      - | 6745 | ` * Return` |
|      - | 6746 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 6747 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 6748 | ` */` |
|     20 | 6749 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6750 |  |
|      - | 6751 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6752 | `	int nLen;` |
|     21 | 6753 | `	if( nArg < 1 ){` |
|      - | 6754 | `		/* Missing arguments,return FALSE */` |
|      3 | 6755 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6756 | `		return PH7_OK;` |
|      - | 6757 | `	}` |
|      - | 6758 | `	/* Extract the target string */` |
|     19 | 6759 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 6760 | `	zEnd = &zIn[nLen];` |
|     19 | 6761 | `	if( nLen < 1 ){` |
|      - | 6762 | `		/* Empty string,return FALSE */` |
|      3 | 6763 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6764 | `		return PH7_OK;` |
|      - | 6765 | `	}` |
|      - | 6766 | `	/* Perform the requested operation */` |
|     46 | 6767 | `	for(;;){` |
|     93 | 6768 | `		if( zIn >= zEnd ){` |
|      - | 6769 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 6770 | `			ph7_result_bool(pCtx,1);` |
|     11 | 6771 | `			return PH7_OK;` |
|      - | 6772 | `		}` |
|     83 | 6773 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6774 | `			/* UTF-8 stream  */` |
|    ! 0 | 6775 | `			break;` |
|      - | 6776 | `		}` |
|     83 | 6777 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 6778 | `			break;` |
|      - | 6779 | `		}` |
|      - | 6780 | `		/* Point to the next character */` |
|     77 | 6781 | `		zIn++;` |
|      1 | 6782 | `	}` |
|      - | 6783 | `	/* The test failed,return FALSE */` |
|      7 | 6784 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6785 | `	return PH7_OK;` |
|     11 | 6786 |  |
|      - | 6787 | `/*` |
|      - | 6788 | ` * bool ctype_graph(string $text)` |
|      - | 6789 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 6790 | ` * Parameters` |
|      - | 6791 | ` *  $text` |
|      - | 6792 | ` *   The tested string.` |
|      - | 6793 | ` * Return` |
|      - | 6794 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 6795 | ` * (no white space), FALSE otherwise.` |
|      - | 6796 | ` */` |
|     18 | 6797 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6798 |  |
|      - | 6799 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6800 | `	int nLen;` |
|     19 | 6801 | `	if( nArg < 1 ){` |
|      - | 6802 | `		/* Missing arguments,return FALSE */` |
|      3 | 6803 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6804 | `		return PH7_OK;` |
|      - | 6805 | `	}` |
|      - | 6806 | `	/* Extract the target string */` |
|     17 | 6807 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6808 | `	zEnd = &zIn[nLen];` |
|     17 | 6809 | `	if( nLen < 1 ){` |
|      - | 6810 | `		/* Empty string,return FALSE */` |
|      3 | 6811 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6812 | `		return PH7_OK;` |
|      - | 6813 | `	}` |
|      - | 6814 | `	/* Perform the requested operation */` |
|     57 | 6815 | `	for(;;){` |
|    115 | 6816 | `		if( zIn >= zEnd ){` |
|      - | 6817 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6818 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6819 | `			return PH7_OK;` |
|      - | 6820 | `		}` |
|    107 | 6821 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6822 | `			/* UTF-8 stream  */` |
|    ! 0 | 6823 | `			break;` |
|      - | 6824 | `		}` |
|    107 | 6825 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 6826 | `			break;` |
|      - | 6827 | `		}` |
|      - | 6828 | `		/* Point to the next character */` |
|    101 | 6829 | `		zIn++;` |
|      1 | 6830 | `	}` |
|      - | 6831 | `	/* The test failed,return FALSE */` |
|      7 | 6832 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6833 | `	return PH7_OK;` |
|     10 | 6834 |  |
|      - | 6835 | `/*` |
|      - | 6836 | ` * bool ctype_print(string $text)` |
|      - | 6837 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 6838 | ` * Parameters` |
|      - | 6839 | ` *  $text` |
|      - | 6840 | ` *   The tested string.` |
|      - | 6841 | ` * Return` |
|      - | 6842 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 6843 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 6844 | ` *  or control function at all.` |
|      - | 6845 | ` */` |
|     18 | 6846 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6847 |  |
|      - | 6848 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6849 | `	int nLen;` |
|     19 | 6850 | `	if( nArg < 1 ){` |
|      - | 6851 | `		/* Missing arguments,return FALSE */` |
|      3 | 6852 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6853 | `		return PH7_OK;` |
|      - | 6854 | `	}` |
|      - | 6855 | `	/* Extract the target string */` |
|     17 | 6856 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6857 | `	zEnd = &zIn[nLen];` |
|     17 | 6858 | `	if( nLen < 1 ){` |
|      - | 6859 | `		/* Empty string,return FALSE */` |
|      3 | 6860 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6861 | `		return PH7_OK;` |
|      - | 6862 | `	}` |
|      - | 6863 | `	/* Perform the requested operation */` |
|     63 | 6864 | `	for(;;){` |
|    127 | 6865 | `		if( zIn >= zEnd ){` |
|      - | 6866 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6867 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6868 | `			return PH7_OK;` |
|      - | 6869 | `		}` |
|    119 | 6870 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6871 | `			/* UTF-8 stream  */` |
|    ! 0 | 6872 | `			break;` |
|      - | 6873 | `		}` |
|    119 | 6874 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 6875 | `			break;` |
|      - | 6876 | `		}` |
|      - | 6877 | `		/* Point to the next character */` |
|    113 | 6878 | `		zIn++;` |
|      1 | 6879 | `	}` |
|      - | 6880 | `	/* The test failed,return FALSE */` |
|      7 | 6881 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6882 | `	return PH7_OK;` |
|     10 | 6883 |  |
|      - | 6884 | `/*` |
|      - | 6885 | ` * bool ctype_punct(string $text)` |
|      - | 6886 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 6887 | ` * Parameters` |
|      - | 6888 | ` *  $text` |
|      - | 6889 | ` *   The tested string.` |
|      - | 6890 | ` * Return` |
|      - | 6891 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 6892 | ` *  digit or blank, FALSE otherwise.` |
|      - | 6893 | ` */` |
|     20 | 6894 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6895 |  |
|      - | 6896 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6897 | `	int nLen;` |
|     21 | 6898 | `	if( nArg < 1 ){` |
|      - | 6899 | `		/* Missing arguments,return FALSE */` |
|      3 | 6900 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6901 | `		return PH7_OK;` |
|      - | 6902 | `	}` |
|      - | 6903 | `	/* Extract the target string */` |
|     19 | 6904 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 6905 | `	zEnd = &zIn[nLen];` |
|     19 | 6906 | `	if( nLen < 1 ){` |
|      - | 6907 | `		/* Empty string,return FALSE */` |
|      3 | 6908 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6909 | `		return PH7_OK;` |
|      - | 6910 | `	}` |
|      - | 6911 | `	/* Perform the requested operation */` |
|     38 | 6912 | `	for(;;){` |
|     77 | 6913 | `		if( zIn >= zEnd ){` |
|      - | 6914 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6915 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6916 | `			return PH7_OK;` |
|      - | 6917 | `		}` |
|     69 | 6918 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6919 | `			/* UTF-8 stream  */` |
|    ! 0 | 6920 | `			break;` |
|      - | 6921 | `		}` |
|     69 | 6922 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 6923 | `			break;` |
|      - | 6924 | `		}` |
|      - | 6925 | `		/* Point to the next character */` |
|     61 | 6926 | `		zIn++;` |
|      1 | 6927 | `	}` |
|      - | 6928 | `	/* The test failed,return FALSE */` |
|      9 | 6929 | `	ph7_result_bool(pCtx,0);` |
|      9 | 6930 | `	return PH7_OK;` |
|     11 | 6931 |  |
|      - | 6932 | `/*` |
|      - | 6933 | ` * bool ctype_space(string $text)` |
|      - | 6934 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 6935 | ` * Parameters` |
|      - | 6936 | ` *  $text` |
|      - | 6937 | ` *   The tested string.` |
|      - | 6938 | ` * Return` |
|      - | 6939 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 6940 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 6941 | ` *  and form feed characters.` |
|      - | 6942 | ` */` |
|  60181 | 6943 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 6944 |  |
|      - | 6945 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6946 | `	int nLen;` |
|  60186 | 6947 | `	if( nArg < 1 ){` |
|      - | 6948 | `		/* Missing arguments,return FALSE */` |
|      3 | 6949 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6950 | `		return PH7_OK;` |
|      - | 6951 | `	}` |
|      - | 6952 | `	/* Extract the target string */` |
|  60184 | 6953 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  60184 | 6954 | `	zEnd = &zIn[nLen];` |
|  60184 | 6955 | `	if( nLen < 1 ){` |
|      - | 6956 | `		/* Empty string,return FALSE */` |
|      3 | 6957 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6958 | `		return PH7_OK;` |
|      - | 6959 | `	}` |
|      - | 6960 | `	/* Perform the requested operation */` |
|  31171 | 6961 | `	for(;;){` |
|  62262 | 6962 | `		if( zIn >= zEnd ){` |
|      - | 6963 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   2061 | 6964 | `			ph7_result_bool(pCtx,1);` |
|   2061 | 6965 | `			return PH7_OK;` |
|      - | 6966 | `		}` |
|  60206 | 6967 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6968 | `			/* UTF-8 stream  */` |
|    ! 0 | 6969 | `			break;` |
|      - | 6970 | `		}` |
|  60206 | 6971 | `		if( !SyisSpace(zIn[0]) ){` |
|  58126 | 6972 | `			break;` |
|      - | 6973 | `		}` |
|      - | 6974 | `		/* Point to the next character */` |
|   2085 | 6975 | `		zIn++;` |
|      5 | 6976 | `	}` |
|      - | 6977 | `	/* The test failed,return FALSE */` |
|  58126 | 6978 | `	ph7_result_bool(pCtx,0);` |
|  58126 | 6979 | `	return PH7_OK;` |
|  30138 | 6980 |  |
|      - | 6981 | `/*` |
|      - | 6982 | ` * bool ctype_lower(string $text)` |
|      - | 6983 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 6984 | ` * Parameters` |
|      - | 6985 | ` *  $text` |
|      - | 6986 | ` *   The tested string.` |
|      - | 6987 | ` * Return` |
|      - | 6988 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 6989 | ` */` |
|     18 | 6990 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6991 |  |
|      - | 6992 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6993 | `	int nLen;` |
|     19 | 6994 | `	if( nArg < 1 ){` |
|      - | 6995 | `		/* Missing arguments,return FALSE */` |
|      3 | 6996 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6997 | `		return PH7_OK;` |
|      - | 6998 | `	}` |
|      - | 6999 | `	/* Extract the target string */` |
|     17 | 7000 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7001 | `	zEnd = &zIn[nLen];` |
|     17 | 7002 | `	if( nLen < 1 ){` |
|      - | 7003 | `		/* Empty string,return FALSE */` |
|      3 | 7004 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7005 | `		return PH7_OK;` |
|      - | 7006 | `	}` |
|      - | 7007 | `	/* Perform the requested operation */` |
|     27 | 7008 | `	for(;;){` |
|     55 | 7009 | `		if( zIn >= zEnd ){` |
|      - | 7010 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7011 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7012 | `			return PH7_OK;` |
|      - | 7013 | `		}` |
|     51 | 7014 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 7015 | `			break;` |
|      - | 7016 | `		}` |
|      - | 7017 | `		/* Point to the next character */` |
|     41 | 7018 | `		zIn++;` |
|      1 | 7019 | `	}` |
|      - | 7020 | `	/* The test failed,return FALSE */` |
|     11 | 7021 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7022 | `	return PH7_OK;` |
|     10 | 7023 |  |
|      - | 7024 | `/*` |
|      - | 7025 | ` * bool ctype_upper(string $text)` |
|      - | 7026 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 7027 | ` * Parameters` |
|      - | 7028 | ` *  $text` |
|      - | 7029 | ` *   The tested string.` |
|      - | 7030 | ` * Return` |
|      - | 7031 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 7032 | ` */` |
|     18 | 7033 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7034 |  |
|      - | 7035 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7036 | `	int nLen;` |
|     19 | 7037 | `	if( nArg < 1 ){` |
|      - | 7038 | `		/* Missing arguments,return FALSE */` |
|      3 | 7039 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7040 | `		return PH7_OK;` |
|      - | 7041 | `	}` |
|      - | 7042 | `	/* Extract the target string */` |
|     17 | 7043 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7044 | `	zEnd = &zIn[nLen];` |
|     17 | 7045 | `	if( nLen < 1 ){` |
|      - | 7046 | `		/* Empty string,return FALSE */` |
|      3 | 7047 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7048 | `		return PH7_OK;` |
|      - | 7049 | `	}` |
|      - | 7050 | `	/* Perform the requested operation */` |
|     28 | 7051 | `	for(;;){` |
|     57 | 7052 | `		if( zIn >= zEnd ){` |
|      - | 7053 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7054 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7055 | `			return PH7_OK;` |
|      - | 7056 | `		}` |
|     53 | 7057 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 7058 | `			break;` |
|      - | 7059 | `		}` |
|      - | 7060 | `		/* Point to the next character */` |
|     43 | 7061 | `		zIn++;` |
|      1 | 7062 | `	}` |
|      - | 7063 | `	/* The test failed,return FALSE */` |
|     11 | 7064 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7065 | `	return PH7_OK;` |
|     10 | 7066 |  |
|      - | 7067 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 7068 | `/*` |
|      - | 7069 | ` * Section:` |
|      - | 7070 | ` *    URL handling Functions.` |
|      - | 7071 | ` * Status:` |
|      - | 7072 | ` *    Stable.` |
|      - | 7073 | ` */` |
|      - | 7074 | `/*` |
|      - | 7075 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 7076 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 7077 | ` */` |
|   1026 | 7078 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 7079 |  |
|      - | 7080 | `	/* Store in the call context result buffer */` |
|   1028 | 7081 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 7082 | `	return SXRET_OK;` |
|      2 | 7083 |  |
|      - | 7084 | `/*` |
|      - | 7085 | ` * string base64_encode(string $data)` |
|      - | 7086 | ` * string convert_uuencode(string $data)` |
|      - | 7087 | ` *  Encodes data with MIME base64` |
|      - | 7088 | ` * Parameter` |
|      - | 7089 | ` *  $data` |
|      - | 7090 | ` *    Data to encode` |
|      - | 7091 | ` * Return` |
|      - | 7092 | ` *  Encoded data or FALSE on failure.` |
|      - | 7093 | ` */` |
|     10 | 7094 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7095 |  |
|      - | 7096 | `	const char *zIn;` |
|      - | 7097 | `	int nLen;` |
|     11 | 7098 | `	if( nArg < 1 ){` |
|      - | 7099 | `		/* Missing arguments,return FALSE */` |
|      5 | 7100 | `		ph7_result_bool(pCtx,0);` |
|      5 | 7101 | `		return PH7_OK;` |
|      - | 7102 | `	}` |
|      - | 7103 | `	/* Extract the input string */` |
|      7 | 7104 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 7105 | `	if( nLen < 1 ){` |
|      - | 7106 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7107 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7108 | `		return PH7_OK;` |
|      - | 7109 | `	}` |
|      - | 7110 | `	/* Perform the BASE64 encoding */` |
|      7 | 7111 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 7112 | `	return PH7_OK;` |
|      6 | 7113 |  |
|      - | 7114 | `/*` |
|      - | 7115 | ` * string base64_decode(string $data)` |
|      - | 7116 | ` * string convert_uudecode(string $data)` |
|      - | 7117 | ` *  Decodes data encoded with MIME base64` |
|      - | 7118 | ` * Parameter` |
|      - | 7119 | ` *  $data` |
|      - | 7120 | ` *    Encoded data.` |
|      - | 7121 | ` * Return` |
|      - | 7122 | ` *  Returns the original data or FALSE on failure.` |
|      - | 7123 | ` */` |
|     36 | 7124 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 7125 |  |
|      - | 7126 | `	const char *zIn;` |
|      - | 7127 | `	int nLen;` |
|     38 | 7128 | `	if( nArg < 1 ){` |
|      - | 7129 | `		/* Missing arguments,return FALSE */` |
|      3 | 7130 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7131 | `		return PH7_OK;` |
|      - | 7132 | `	}` |
|      - | 7133 | `	/* Extract the input string */` |
|     36 | 7134 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 7135 | `	if( nLen < 1 ){` |
|      - | 7136 | `		/* Nothing to process,return FALSE */` |
|      3 | 7137 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7138 | `		return PH7_OK;` |
|      - | 7139 | `	}` |
|      - | 7140 | `	/* Perform the BASE64 decoding */` |
|     34 | 7141 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 7142 | `	return PH7_OK;` |
|     20 | 7143 |  |
|      - | 7144 | `/*` |
|      - | 7145 | ` * string urlencode(string $str)` |
|      - | 7146 | ` *  URL encoding` |
|      - | 7147 | ` * Parameter` |
|      - | 7148 | ` *  $data` |
|      - | 7149 | ` *   Input string.` |
|      - | 7150 | ` * Return` |
|      - | 7151 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 7152 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 7153 | ` *  encoded as plus (+) signs.` |
|      - | 7154 | ` */` |
|      6 | 7155 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7156 |  |
|      - | 7157 | `	const char *zIn;` |
|      - | 7158 | `	int nLen;` |
|      7 | 7159 | `	if( nArg < 1 ){` |
|      - | 7160 | `		/* Missing arguments,return FALSE */` |
|      3 | 7161 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7162 | `		return PH7_OK;` |
|      - | 7163 | `	}` |
|      - | 7164 | `	/* Extract the input string */` |
|      5 | 7165 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 7166 | `	if( nLen < 1 ){` |
|      - | 7167 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7168 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7169 | `		return PH7_OK;` |
|      - | 7170 | `	}` |
|      - | 7171 | `	/* Perform the URL encoding */` |
|      5 | 7172 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 7173 | `	return PH7_OK;` |
|      4 | 7174 |  |
|      - | 7175 | `/*` |
|      - | 7176 | ` * string urldecode(string $str)` |
|      - | 7177 | ` *  Decodes any %## encoding in the given string.` |
|      - | 7178 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 7179 | ` * Parameter` |
|      - | 7180 | ` *  $data` |
|      - | 7181 | ` *    Input string.` |
|      - | 7182 | ` * Return` |
|      - | 7183 | ` *  Decoded URL or FALSE on failure.` |
|      - | 7184 | ` */` |
|      8 | 7185 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7186 |  |
|      - | 7187 | `	const char *zIn;` |
|      - | 7188 | `	int nLen;` |
|      9 | 7189 | `	if( nArg < 1 ){` |
|      - | 7190 | `		/* Missing arguments,return FALSE */` |
|      3 | 7191 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7192 | `		return PH7_OK;` |
|      - | 7193 | `	}` |
|      - | 7194 | `	/* Extract the input string */` |
|      7 | 7195 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 7196 | `	if( nLen < 1 ){` |
|      - | 7197 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7198 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7199 | `		return PH7_OK;` |
|      - | 7200 | `	}` |
|      - | 7201 | `	/* Perform the URL decoding */` |
|      7 | 7202 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 7203 | `	return PH7_OK;` |
|      5 | 7204 |  |
|      - | 7205 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7206 | `/* Table of the built-in functions */` |
|      - | 7207 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 7208 | `	   /* Variable handling functions */` |
|      - | 7209 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 7210 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 7211 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 7212 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 7213 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 7214 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 7215 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 7216 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 7217 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 7218 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 7219 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 7220 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 7221 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 7222 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 7223 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 7224 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 7225 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 7226 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 7227 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 7228 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 7229 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7230 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 7231 | `	   /* Math functions */` |
|      - | 7232 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 7233 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 7234 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 7235 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 7236 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 7237 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 7238 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 7239 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 7240 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 7241 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 7242 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 7243 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 7244 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 7245 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 7246 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 7247 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 7248 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 7249 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 7250 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 7251 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 7252 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 7253 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 7254 | `	{ "round",    PH7_builtin_round        },` |
|      - | 7255 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 7256 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 7257 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 7258 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 7259 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 7260 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 7261 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 7262 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 7263 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 7264 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7265 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7266 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 7267 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7268 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7269 | `	   /* String handling functions */` |
|      - | 7270 |  |
|      - | 7271 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 7272 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 7273 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 7274 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 7275 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 7276 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 7277 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 7278 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 7279 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 7280 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 7281 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 7282 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 7283 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 7284 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 7285 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 7286 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 7287 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 7288 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 7289 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 7290 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 7291 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 7292 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 7293 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 7294 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 7295 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 7296 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 7297 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 7298 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 7299 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 7300 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 7301 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 7302 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 7303 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 7304 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 7305 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 7306 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 7307 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 7308 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 7309 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 7310 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 7311 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 7312 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 7313 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 7314 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 7315 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 7316 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 7317 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 7318 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 7319 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - | 7320 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - | 7321 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - | 7322 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 7323 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7324 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7325 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 7326 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 7327 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 7328 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 7329 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7330 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7331 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 7332 |  |
|      - | 7333 |  |
|      - | 7334 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 7335 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 7336 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 7337 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 7338 | `	{ "hash",         PH7_builtin_hash      },` |
|      - | 7339 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|      - | 7340 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|      - | 7341 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|      - | 7342 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 7343 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|      - | 7344 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|      - | 7345 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|      - | 7346 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|      - | 7347 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7348 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7349 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 7350 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 7351 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7352 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7353 |  |
|      - | 7354 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 7355 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 7356 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 7357 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 7358 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 7359 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 7360 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 7361 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 7362 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 7363 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 7364 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 7365 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 7366 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7367 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7368 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 7369 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7370 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7371 |  |
|      - | 7372 | `	         /* Ctype functions */` |
|      - | 7373 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 7374 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 7375 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 7376 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 7377 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 7378 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 7379 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 7380 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 7381 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 7382 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 7383 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 7384 | `	         /* Time functions */` |
|      - | 7385 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 7386 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 7387 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 7388 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 7389 | `	{ "date",        PH7_builtin_date         },` |
|      - | 7390 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 7391 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 7392 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 7393 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 7394 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 7395 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 7396 | `	        /* URL functions */` |
|      - | 7397 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 7398 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 7399 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 7400 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 7401 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 7402 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 7403 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 7404 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 7405 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7406 | `};` |
|      - | 7407 | `/*` |
|      - | 7408 | ` * Register the built-in functions defined above,the array functions` |
|      - | 7409 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 7410 | ` */` |
|   2964 | 7411 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      5 | 7412 |  |
|      - | 7413 | `	sxu32 n;` |
| 492029 | 7414 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 489065 | 7415 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 244535 | 7416 | `	}` |
|      - | 7417 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   2969 | 7418 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 7419 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   2969 | 7420 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   2969 | 7421 |  |
|      - | 7422 |  |
