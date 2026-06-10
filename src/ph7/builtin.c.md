# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3045/3455 lines (88.13%)

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
|    510 |   62 | `static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |   63 |  |
|    512 |   64 | `	int res = 0; /* Assume false by default */` |
|    512 |   65 | `	if( nArg > 0 ){` |
|      - |   66 | `		/* Strict PHP identity: a float is never an int, even when it holds an` |
|      - |   67 | `		 * integer value (1.0). An integer-valued real carries both MEMOBJ_INT` |
|      - |   68 | `		 * (cached) and MEMOBJ_REAL, so REAL must be excluded here. */` |
|    510 |   69 | `		res = ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]);` |
|    254 |   70 | `	}` |
|      - |   71 | `	/* Query result */` |
|    512 |   72 | `	ph7_result_bool(pCtx,res);` |
|    512 |   73 | `	return PH7_OK;` |
|      2 |   74 |  |
|      - |   75 | `/*` |
|      - |   76 | ` * bool is_string($var)` |
|      - |   77 | ` *  Finds out whether a variable is a string.` |
|      - |   78 | ` * Parameters` |
|      - |   79 | ` *   $var: The variable being evaluated.` |
|      - |   80 | ` * Return` |
|      - |   81 | ` *  TRUE if var is string. False otherwise.` |
|      - |   82 | ` */` |
|    118 |   83 | `static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   84 |  |
|    119 |   85 | `	int res = 0; /* Assume false by default */` |
|    119 |   86 | `	if( nArg > 0 ){` |
|    117 |   87 | `		res = ph7_value_is_string(apArg[0]);` |
|     58 |   88 | `	}` |
|      - |   89 | `	/* Query result */` |
|    119 |   90 | `	ph7_result_bool(pCtx,res);` |
|    119 |   91 | `	return PH7_OK;` |
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
|      2 |  102 |  |
|     88 |  103 | `	int res = 0; /* Assume false by default */` |
|     88 |  104 | `	if( nArg > 0 ){` |
|     86 |  105 | `		res = ph7_value_is_null(apArg[0]);` |
|     42 |  106 | `	}` |
|      - |  107 | `	/* Query result */` |
|     88 |  108 | `	ph7_result_bool(pCtx,res);` |
|     88 |  109 | `	return PH7_OK;` |
|      2 |  110 |  |
|      - |  111 | `/*` |
|      - |  112 | ` * bool is_numeric($var)` |
|      - |  113 | ` *  Find out whether a variable is NULL.` |
|      - |  114 | ` * Parameters` |
|      - |  115 | ` *  $var: The variable being evaluated.` |
|      - |  116 | ` * Return` |
|      - |  117 | ` *  True if var is numeric. False otherwise.` |
|      - |  118 | ` */` |
|     38 |  119 | `static int PH7_builtin_is_numeric(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  120 |  |
|     40 |  121 | `	int res = 0; /* Assume false by default */` |
|     40 |  122 | `	if( nArg > 0 ){` |
|     38 |  123 | `		res = ph7_value_is_numeric(apArg[0]);` |
|     18 |  124 | `	}` |
|      - |  125 | `	/* Query result */` |
|     40 |  126 | `	ph7_result_bool(pCtx,res);` |
|     40 |  127 | `	return PH7_OK;` |
|      2 |  128 |  |
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
|    194 |  155 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  156 |  |
|    196 |  157 | `	int res = 0; /* Assume false by default */` |
|    196 |  158 | `	if( nArg > 0 ){` |
|    194 |  159 | `		res = ph7_value_is_array(apArg[0]);` |
|     96 |  160 | `	}` |
|      - |  161 | `	/* Query result */` |
|    196 |  162 | `	ph7_result_bool(pCtx,res);` |
|    196 |  163 | `	return PH7_OK;` |
|      2 |  164 |  |
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
|      2 |  192 |  |
|     62 |  193 | `	int res = 0; /* Assume false by default */` |
|     62 |  194 | `	if( nArg > 0 ){` |
|     60 |  195 | `		res = ph7_value_is_resource(apArg[0]);` |
|     29 |  196 | `	}` |
|     62 |  197 | `	ph7_result_bool(pCtx,res);` |
|     62 |  198 | `	return PH7_OK;` |
|      2 |  199 |  |
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
|  25200 |  295 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  296 |  |
|  25202 |  297 | `	int res = 1; /* Assume empty by default */` |
|  25202 |  298 | `	if( nArg > 0 ){` |
|  25200 |  299 | `		res = ph7_value_is_empty(apArg[0]);` |
|  12599 |  300 | `	}` |
|  25202 |  301 | `	ph7_result_bool(pCtx,res);` |
|  25202 |  302 | `	return PH7_OK;` |
|      - |  303 |  |
|      2 |  304 |  |
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
| 186092 |  345 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  346 |  |
|      - |  347 | `	const char *zSource,*zOfft;` |
|      - |  348 | `	int nOfft,nLen,nSrcLen;` |
| 186094 |  349 | `	if( nArg < 2 ){` |
|      - |  350 | `		/* return FALSE */` |
|      5 |  351 | `		ph7_result_bool(pCtx,0);` |
|      5 |  352 | `		return PH7_OK;` |
|      - |  353 | `	}` |
|      - |  354 | `	/* Extract the target string */` |
| 186090 |  355 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 186090 |  356 | `	if( nSrcLen < 1 ){` |
|      - |  357 | `		/* Empty string,return FALSE */` |
|  10892 |  358 | `		ph7_result_bool(pCtx,0);` |
|  10892 |  359 | `		return PH7_OK;` |
|      - |  360 | `	}` |
| 175200 |  361 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  362 | `	/* Extract the offset */` |
| 175200 |  363 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 175200 |  364 | `	if( nOfft < 0 ){` |
|  29184 |  365 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  29184 |  366 | `		if( zOfft < zSource ){` |
|      - |  367 | `			/* Invalid offset */` |
|      5 |  368 | `			ph7_result_bool(pCtx,0);` |
|      5 |  369 | `			return PH7_OK;` |
|      - |  370 | `		}` |
|  29180 |  371 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  29180 |  372 | `		nOfft = (int)(zOfft-zSource);` |
| 160607 |  373 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  374 | `		/* Invalid offset */` |
|    116 |  375 | `		ph7_result_bool(pCtx,0);` |
|    116 |  376 | `		return PH7_OK;` |
|    ! 0 |  377 | `	}else{` |
| 145904 |  378 | `		zOfft = &zSource[nOfft];` |
| 145904 |  379 | `		nLen = nSrcLen - nOfft;` |
|      - |  380 | `	}` |
| 175082 |  381 | `	if( nArg > 2 ){` |
|      - |  382 | `		/* Extract the length */` |
| 144722 |  383 | `		nLen = ph7_value_to_int(apArg[2]);` |
| 144722 |  384 | `		if( nLen == 0 ){` |
|      - |  385 | `			/* Invalid length,return an empty string */` |
|      5 |  386 | `			ph7_result_string(pCtx,"",0);` |
|      5 |  387 | `			return PH7_OK;` |
| 144718 |  388 | `		}else if( nLen < 0 ){` |
|  29174 |  389 | `			nLen = nSrcLen + nLen - nOfft;` |
|  29174 |  390 | `			if( nLen < 1 ){` |
|      - |  391 | `				/* Invalid  length */` |
|      3 |  392 | `				nLen = nSrcLen - nOfft;` |
|      1 |  393 | `			}` |
|  14586 |  394 | `		}` |
| 144718 |  395 | `		if( nLen + nOfft > nSrcLen ){` |
|      - |  396 | `			/* Invalid length */` |
|   4118 |  397 | `			nLen = nSrcLen - nOfft;` |
|   2058 |  398 | `		}` |
|  72358 |  399 | `	}` |
|      - |  400 | `	/* Return the substring */` |
| 175078 |  401 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 175078 |  402 | `	return PH7_OK;` |
|  93048 |  403 |  |
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
|      2 |  653 |  |
|      - |  654 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  655 | `	int nLen;` |
|      - |  656 | `	/* PHP enforces exactly one argument. */` |
|     26 |  657 | `	if( nArg != 1 ){` |
|      7 |  658 | `		return PH7_VmThrowException(pCtx,` |
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
|     14 |  718 |  |
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
|      2 |  769 |  |
|      - |  770 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - |  771 | `	int nLen,nMask;` |
|      - |  772 | `	/* PHP enforces exactly two arguments. */` |
|     36 |  773 | `	if( nArg != 2 ){` |
|      7 |  774 | `		return PH7_VmThrowException(pCtx,` |
|      - |  775 | `			"ArgumentCountError",` |
|      - |  776 | `			"addcslashes() expects exactly 2 arguments, %d given",` |
|      2 |  777 | `			nArg` |
|      - |  778 | `			);` |
|      - |  779 | `	}` |
|      - |  780 | `	/* First argument must be a string-ish value.  NULL is deprecated and` |
|      - |  781 | `	 * treated as the empty string (PHP 8.1). */` |
|     32 |  782 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      - |  783 | `		/* Emit deprecation only once, similar to PHP behaviour. */` |
|      3 |  784 | `		PH7_VmThrowError(pCtx->pVm,0,/* iErr will be patched to 8192 below */` |
|      - |  785 | `			E_DEPRECATED,` |
|      - |  786 | `			"addcslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - |  787 | `			);` |
|      - |  788 | `		/* treat as empty string; fall through to conversion logic */` |
|     56 |  789 | `	} else if( ph7_value_is_array(apArg[0]) \|\|` |
|     41 |  790 | `	          ph7_value_is_object(apArg[0]) \|\|` |
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
|     19 |  869 |  |
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
|   5244 | 1372 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1373 |  |
|   5246 | 1374 | `	int iLen = 0;` |
|   5246 | 1375 | `	if( nArg > 0 ){` |
|   5244 | 1376 | `		ph7_value_to_string(apArg[0],&iLen);` |
|   2621 | 1377 | `	}` |
|      - | 1378 | `	/* String length */` |
|   5246 | 1379 | `	ph7_result_int(pCtx,iLen);` |
|   5246 | 1380 | `	return PH7_OK;` |
|      2 | 1381 |  |
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
| 118026 | 1525 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      2 | 1526 |  |
|  59013 | 1527 | `	SXUNUSED(pKey);` |
| 118028 | 1528 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 1529 | `	const char *zData;` |
|      - | 1530 | `	int nLen;` |
| 118028 | 1531 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
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
| 118026 | 1548 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 1549 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 118026 | 1550 | `	if( pData->bFirst ){` |
|  29456 | 1551 | `		pData->bFirst = 0;` |
| 103299 | 1552 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 1553 | `		/* append the separator first */` |
|  88560 | 1554 | `		ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|  44279 | 1555 | `	}` |
|      - | 1556 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 118026 | 1557 | `	if( nLen > 0 ){` |
| 107136 | 1558 | `		ph7_result_string(pData->pCtx,zData,nLen);` |
|  53567 | 1559 | `	}` |
| 118026 | 1560 | `	return PH7_OK;` |
|  59015 | 1561 |  |
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
|  29478 | 1575 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1576 |  |
|      - | 1577 | `	struct implode_data imp_data;` |
|  29480 | 1578 | `	int i = 1;` |
|  29480 | 1579 | `	if( nArg < 1 ){` |
|      - | 1580 | `		/* Missing argument,return NULL */` |
|    ! 0 | 1581 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1582 | `		return PH7_OK;` |
|      - | 1583 | `	}` |
|      - | 1584 | `	/* Prepare the implode context */` |
|  29480 | 1585 | `	imp_data.pCtx = pCtx;` |
|  29480 | 1586 | `	imp_data.bRecursive = 0;` |
|  29480 | 1587 | `	imp_data.bFirst = 1;` |
|  29480 | 1588 | `	imp_data.nRecCount = 0;` |
|  29480 | 1589 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  29478 | 1590 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  14740 | 1591 | `	}else{` |
|      3 | 1592 | `		imp_data.zSep = 0;` |
|      3 | 1593 | `		imp_data.nSeplen = 0;` |
|      3 | 1594 | `		i = 0;` |
|      - | 1595 | `	}` |
|  29480 | 1596 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 1597 | `	/* Start the 'join' process */` |
|  58958 | 1598 | `	while( i < nArg ){` |
|  29480 | 1599 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1600 | `			/* Iterate throw array entries */` |
|  29480 | 1601 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|  14741 | 1602 | `		}else{` |
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
|  29480 | 1618 | `		i++;` |
|      2 | 1619 | `	}` |
|  29480 | 1620 | `	return PH7_OK;` |
|  14741 | 1621 |  |
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
|   5542 | 1710 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1711 |  |
|      - | 1712 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 1713 | `	int nDelim,nStrlen,iLimit;` |
|      - | 1714 | `	ph7_value *pArray;` |
|      - | 1715 | `	ph7_value *pValue;` |
|      - | 1716 | `	sxu32 nOfft;` |
|      - | 1717 | `	sxi32 rc;` |
|   5544 | 1718 | `	if( nArg < 2 ){` |
|      - | 1719 | `		/* Missing arguments,return FALSE */` |
|      9 | 1720 | `		ph7_result_bool(pCtx,0);` |
|      9 | 1721 | `		return PH7_OK;` |
|      - | 1722 | `	}` |
|      - | 1723 | `	/* Extract the delimiter */` |
|   5536 | 1724 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   5536 | 1725 | `	if( nDelim < 1 ){` |
|      - | 1726 | `		/* Empty delimiter,return FALSE */` |
|      3 | 1727 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1728 | `		return PH7_OK;` |
|      - | 1729 | `	}` |
|      - | 1730 | `	/* Extract the string */` |
|   5534 | 1731 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   5534 | 1732 | `	if( nStrlen < 1 ){` |
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
|   5532 | 1747 | `	zEnd = &zString[nStrlen];` |
|      - | 1748 | `	/* Create the array */` |
|   5532 | 1749 | `	pArray =  ph7_context_new_array(pCtx);` |
|   5532 | 1750 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   5532 | 1751 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 1752 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 1753 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1754 | `		return PH7_OK;` |
|      - | 1755 | `	}` |
|      - | 1756 | `	/* Set a defualt limit */` |
|   5532 | 1757 | `	iLimit = SXI32_HIGH;` |
|   5532 | 1758 | `	if( nArg > 2 ){` |
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
|  63240 | 1769 | `	for(;;){` |
| 126482 | 1770 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 126482 | 1771 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 1772 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   5532 | 1773 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   5532 | 1774 | `			ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|   5532 | 1775 | `			break;` |
|      - | 1776 | `		}` |
|      - | 1777 | `		/* Point to the desired offset */` |
| 120952 | 1778 | `		zCur = &zString[nOfft];` |
|      - | 1779 | `		/* Perform the store operation (may be empty) */` |
| 120952 | 1780 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 120952 | 1781 | `		ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|      - | 1782 | `		/* Point beyond the delimiter */` |
| 120952 | 1783 | `		zString = &zCur[nDelim];` |
|      - | 1784 | `		/* Reset the cursor */` |
| 120952 | 1785 | `		ph7_value_reset_string_cursor(pValue);` |
|      2 | 1786 | `	}` |
|      - | 1787 | `	/* Return the freshly created array */` |
|   5532 | 1788 | `	ph7_result_value(pCtx,pArray);` |
|      - | 1789 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 1790 | `	 * released as soon we return from this foregin function.` |
|      - | 1791 | `	 */` |
|   5532 | 1792 | `	return PH7_OK;` |
|   2773 | 1793 |  |
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
|  12712 | 1809 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1810 |  |
|      - | 1811 | `	const char *zString;` |
|      - | 1812 | `	int nLen;` |
|  12714 | 1813 | `	if( nArg < 1 ){` |
|      - | 1814 | `		/* Missing arguments,return null */` |
|      3 | 1815 | `		ph7_result_null(pCtx);` |
|      3 | 1816 | `		return PH7_OK;` |
|      - | 1817 | `	}` |
|      - | 1818 | `	/* Extract the target string */` |
|  12712 | 1819 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  12712 | 1820 | `	if( nLen < 1 ){` |
|      - | 1821 | `		/* Empty string,return */` |
|   1644 | 1822 | `		ph7_result_string(pCtx,"",0);` |
|   1644 | 1823 | `		return PH7_OK;` |
|      - | 1824 | `	}` |
|      - | 1825 | `	/* Start the trim process */` |
|  11070 | 1826 | `	if( nArg < 2 ){` |
|      - | 1827 | `		SyString sStr;` |
|      - | 1828 | `		/* Remove white spaces and NUL bytes */` |
|  11066 | 1829 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  26960 | 1830 | `		SyStringFullTrimSafe(&sStr);` |
|  11066 | 1831 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   5534 | 1832 | `	}else{` |
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
|  11070 | 1886 | `	return PH7_OK;` |
|   6358 | 1887 |  |
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
|  29174 | 2051 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2052 |  |
|      - | 2053 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2054 | `	int nLen;` |
|  29176 | 2055 | `	if( nArg < 1 ){` |
|      - | 2056 | `		/* Missing arguments,return null */` |
|      3 | 2057 | `		ph7_result_null(pCtx);` |
|      3 | 2058 | `		return PH7_OK;` |
|      - | 2059 | `	}` |
|      - | 2060 | `	/* Extract the target string */` |
|  29174 | 2061 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  29174 | 2062 | `	if( nLen < 1 ){` |
|      - | 2063 | `		/* Empty string,return */` |
|      3 | 2064 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2065 | `		return PH7_OK;` |
|      - | 2066 | `	}` |
|      - | 2067 | `	/* Perform the requested operation */` |
|  29172 | 2068 | `	zEnd = &zString[nLen];` |
|  91943 | 2069 | `	for(;;){` |
| 183888 | 2070 | `		if( zString >= zEnd ){` |
|      - | 2071 | `			/* No more input,break immediately */` |
|  29172 | 2072 | `			break;` |
|      - | 2073 | `		}` |
| 154718 | 2074 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2075 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2076 | `			zCur = zString;` |
|    ! 0 | 2077 | `			zString++;` |
|    ! 0 | 2078 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2079 | `				zString++;` |
|    ! 0 | 2080 | `			}` |
|      - | 2081 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2082 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2083 | `		}else{` |
| 154718 | 2084 | `			int c = zString[0];` |
| 154718 | 2085 | `			if( SyisUpper(c) ){` |
| 154716 | 2086 | `				c = SyToLower(zString[0]);` |
|  77357 | 2087 | `			}` |
|      - | 2088 | `			/* Append character */` |
| 154718 | 2089 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2090 | `			/* Advance the cursor */` |
| 154718 | 2091 | `			zString++;` |
|      - | 2092 | `		}` |
|      2 | 2093 | `	}` |
|  29172 | 2094 | `	return PH7_OK;` |
|  14589 | 2095 |  |
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
|      2 | 2106 |  |
|      - | 2107 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2108 | `	int nLen;` |
|     36 | 2109 | `	if( nArg < 1 ){` |
|      - | 2110 | `		/* Missing arguments,return null */` |
|      3 | 2111 | `		ph7_result_null(pCtx);` |
|      3 | 2112 | `		return PH7_OK;` |
|      - | 2113 | `	}` |
|      - | 2114 | `	/* Extract the target string */` |
|     34 | 2115 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     34 | 2116 | `	if( nLen < 1 ){` |
|      - | 2117 | `		/* Empty string,return */` |
|      3 | 2118 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2119 | `		return PH7_OK;` |
|      - | 2120 | `	}` |
|      - | 2121 | `	/* Perform the requested operation */` |
|     32 | 2122 | `	zEnd = &zString[nLen];` |
|     88 | 2123 | `	for(;;){` |
|    178 | 2124 | `		if( zString >= zEnd ){` |
|      - | 2125 | `			/* No more input,break immediately */` |
|     32 | 2126 | `			break;` |
|      - | 2127 | `		}` |
|    148 | 2128 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2129 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2130 | `			zCur = zString;` |
|    ! 0 | 2131 | `			zString++;` |
|    ! 0 | 2132 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2133 | `				zString++;` |
|    ! 0 | 2134 | `			}` |
|      - | 2135 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2136 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2137 | `		}else{` |
|    148 | 2138 | `			int c = zString[0];` |
|    148 | 2139 | `			if( SyisLower(c) ){` |
|    142 | 2140 | `				c = SyToUpper(zString[0]);` |
|     70 | 2141 | `			}` |
|      - | 2142 | `			/* Append character */` |
|    148 | 2143 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2144 | `			/* Advance the cursor */` |
|    148 | 2145 | `			zString++;` |
|      - | 2146 | `		}` |
|      2 | 2147 | `	}` |
|     32 | 2148 | `	return PH7_OK;` |
|     19 | 2149 |  |
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
|      2 | 2243 |  |
|      - | 2244 | `	const char *zString;` |
|      - | 2245 | `	int nLen,c;` |
|      - | 2246 | `	/* PHP requires exactly one argument. */` |
|     64 | 2247 | `	if( nArg != 1 ){` |
|      7 | 2248 | `		return PH7_VmThrowException(pCtx,` |
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
|     33 | 2284 |  |
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
|      2 | 2298 |  |
|      - | 2299 | `	int c;` |
|      - | 2300 | `	unsigned char ch;` |
|      - | 2301 | `	/* PHP requires exactly one argument. */` |
|     46 | 2302 | `	if( nArg != 1 ){` |
|      7 | 2303 | `		return PH7_VmThrowException(pCtx,` |
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
|     24 | 2341 |  |
|      - | 2342 | `/*` |
|      - | 2343 | ` * Binary to hex consumer callback.` |
|      - | 2344 | ` * This callback is the default consumer used by the hash functions` |
|      - | 2345 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 2346 | ` */` |
|    226 | 2347 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 2348 |  |
|      - | 2349 | `	/* Append hex chunk verbatim */` |
|    227 | 2350 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|    227 | 2351 | `	return SXRET_OK;` |
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
|     20 | 2363 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2364 |  |
|      - | 2365 | `	const char *zString;` |
|      - | 2366 | `	int nLen;` |
|      - | 2367 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|     22 | 2368 | `	if( nArg != 1 ){` |
|      7 | 2369 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2370 | `			"ArgumentCountError",` |
|      - | 2371 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|      2 | 2372 | `			nArg` |
|      - | 2373 | `			);` |
|      - | 2374 | `	}` |
|      - | 2375 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 2376 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 2377 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 2378 | `	 */` |
|     25 | 2379 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|     12 | 2380 | `		( ph7_value_is_object(apArg[0]) &&` |
|      3 | 2381 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|      2 | 2382 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|      1 | 2383 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 2384 | `		)` |
|      - | 2385 | `	){` |
|      7 | 2386 | `		const char *zType = ph7_type_name(apArg[0]);` |
|      7 | 2387 | `		if( ph7_value_is_object(apArg[0]) ){` |
|      3 | 2388 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      3 | 2389 | `			if( pInst && pInst->pClass ){` |
|      3 | 2390 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      1 | 2391 | `			}` |
|      1 | 2392 | `		}` |
|     10 | 2393 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2394 | `			"TypeError",` |
|      - | 2395 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|      3 | 2396 | `			zType` |
|      - | 2397 | `			);` |
|      - | 2398 | `	}` |
|      - | 2399 | `	/* Extract the target string */` |
|     11 | 2400 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 2401 | `	if( nLen < 1 ){` |
|      - | 2402 | `		/* Empty string,return */` |
|      3 | 2403 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2404 | `		return PH7_OK;` |
|      - | 2405 | `	}` |
|      - | 2406 | `	/* Perform the requested operation */` |
|      9 | 2407 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|      9 | 2408 | `	return PH7_OK;` |
|     12 | 2409 |  |
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
|    120 | 2583 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2584 |  |
|    122 | 2585 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2586 | `	const char *zBlob,*zPattern;` |
|      - | 2587 | `	int nLen,nPatLen,nStart;` |
|      - | 2588 | `	sxu32 nOfft;` |
|      - | 2589 | `	sxi32 rc;` |
|    122 | 2590 | `	if( nArg < 2 ){` |
|      - | 2591 | `		/* Missing arguments,return FALSE */` |
|      7 | 2592 | `		ph7_result_bool(pCtx,0);` |
|      7 | 2593 | `		return PH7_OK;` |
|      - | 2594 | `	}` |
|      - | 2595 | `	/* Extract the needle and the haystack */` |
|    116 | 2596 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    116 | 2597 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    116 | 2598 | `	nOfft = 0; /* cc warning */` |
|    116 | 2599 | `	nStart = 0;` |
|      - | 2600 | `	/* Peek the starting offset if available */` |
|    116 | 2601 | `	if( nArg > 2 ){` |
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
|    116 | 2614 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2615 | `		/* Perform the lookup */` |
|    114 | 2616 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    114 | 2617 | `		if( rc != SXRET_OK ){` |
|      - | 2618 | `			/* Pattern not found,return FALSE */` |
|     26 | 2619 | `			ph7_result_bool(pCtx,0);` |
|     26 | 2620 | `			return PH7_OK;` |
|      - | 2621 | `		}` |
|      - | 2622 | `		/* Return the pattern position */` |
|     90 | 2623 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|     46 | 2624 | `	}else{` |
|      3 | 2625 | `		ph7_result_bool(pCtx,0);` |
|      - | 2626 | `	}` |
|     92 | 2627 | `	return PH7_OK;` |
|     62 | 2628 |  |
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
|      2 | 2652 | `){` |
|    388 | 2653 | `	if( ph7_value_is_null(pArg) ){` |
|     13 | 2654 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zNullMsg);` |
|     13 | 2655 | `		*pzOut = "";` |
|     13 | 2656 | `		*pnOut = 0;` |
|     13 | 2657 | `		return PH7_OK;` |
|      - | 2658 | `	}` |
|    578 | 2659 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|    356 | 2660 | `	    ( ph7_value_is_object(pArg) &&` |
|     72 | 2661 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|     48 | 2662 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|     24 | 2663 | `	        "__toString",sizeof("__toString")-1) == 0` |
|      - | 2664 | `	    )` |
|      - | 2665 | `	){` |
|     32 | 2666 | `		const char *zType = ph7_type_name(pArg);` |
|     32 | 2667 | `		if( ph7_value_is_object(pArg) ){` |
|     13 | 2668 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     13 | 2669 | `			if( pInst && pInst->pClass ){` |
|     13 | 2670 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      6 | 2671 | `			}` |
|      6 | 2672 | `		}` |
|     47 | 2673 | `		return PH7_VmThrowException(pCtx,` |
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
|    195 | 2690 |  |
|      - | 2691 | `/*` |
|      - | 2692 | ` * bool str_contains(string $haystack, string $needle)` |
|      - | 2693 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|      - | 2694 | ` * Return` |
|      - | 2695 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|      - | 2696 | ` */` |
|     76 | 2697 | `static int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2698 |  |
|      - | 2699 | `	const char *zHaystack,*zNeedle;` |
|      - | 2700 | `	int nHayLen,nNeedleLen;` |
|      - | 2701 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2702 | `	sxi32 rc;` |
|     78 | 2703 | `	if( nArg != 2 ){` |
|     17 | 2704 | `		return PH7_VmThrowException(pCtx,` |
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
|     40 | 2736 |  |
|      - | 2737 | `/*` |
|      - | 2738 | ` * bool str_starts_with(string $haystack, string $needle)` |
|      - | 2739 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|      - | 2740 | ` * Return` |
|      - | 2741 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|      - | 2742 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 2743 | ` */` |
|     78 | 2744 | `static int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2745 |  |
|      - | 2746 | `	const char *zHaystack,*zNeedle;` |
|      - | 2747 | `	int nHayLen,nNeedleLen;` |
|      - | 2748 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2749 | `	sxi32 rc;` |
|     80 | 2750 | `	if( nArg != 2 ){` |
|     17 | 2751 | `		return PH7_VmThrowException(pCtx,` |
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
|     41 | 2782 |  |
|      - | 2783 | `/*` |
|      - | 2784 | ` * bool str_ends_with(string $haystack, string $needle)` |
|      - | 2785 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|      - | 2786 | ` * Return` |
|      - | 2787 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|      - | 2788 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 2789 | ` */` |
|     78 | 2790 | `static int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2791 |  |
|      - | 2792 | `	const char *zHaystack,*zNeedle;` |
|      - | 2793 | `	int nHayLen,nNeedleLen;` |
|      - | 2794 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2795 | `	sxi32 rc;` |
|     80 | 2796 | `	if( nArg != 2 ){` |
|     17 | 2797 | `		return PH7_VmThrowException(pCtx,` |
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
|     41 | 2828 |  |
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
|  20216 | 3230 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3231 |  |
|      - | 3232 | `	const char *zIn;` |
|      - | 3233 | `	int nLen,nMul;` |
|      - | 3234 | `	int rc;` |
|  20217 | 3235 | `	if( nArg < 2 ){` |
|      - | 3236 | `		/* Missing arguments,return NULL */` |
|      3 | 3237 | `		ph7_result_null(pCtx);` |
|      3 | 3238 | `		return PH7_OK;` |
|      - | 3239 | `	}` |
|      - | 3240 | `	/* Extract the target string */` |
|  20215 | 3241 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|  20215 | 3242 | `	if( nLen < 1 ){` |
|      - | 3243 | `		/* Empty string.Return null */` |
|    ! 0 | 3244 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3245 | `		return PH7_OK;` |
|      - | 3246 | `	}` |
|      - | 3247 | `	/* Extract the multiplier */` |
|  20215 | 3248 | `	nMul = ph7_value_to_int(apArg[1]);` |
|  20215 | 3249 | `	if( nMul < 1 ){` |
|      - | 3250 | `		/* Return the empty string */` |
|      3 | 3251 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3252 | `		return PH7_OK;` |
|      - | 3253 | `	}` |
|      - | 3254 | `	/* Perform the requested operation */` |
| 120289 | 3255 | `	for(;;){` |
| 240579 | 3256 | `		if( !nMul ){` |
|  20213 | 3257 | `			break;` |
|      - | 3258 | `		}` |
|      - | 3259 | `		/* Append the copy */` |
| 220367 | 3260 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 220367 | 3261 | `		if( rc != PH7_OK ){` |
|      - | 3262 | `			/* Out of memory,break immediately */` |
|    ! 0 | 3263 | `			break;` |
|      - | 3264 | `		}` |
| 220367 | 3265 | `		nMul--;` |
|      1 | 3266 | `	}` |
|  20213 | 3267 | `	return PH7_OK;` |
|  10109 | 3268 |  |
|      - | 3269 | `/*` |
|      - | 3270 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3271 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3272 | ` * Parameters` |
|      - | 3273 | ` *  $string` |
|      - | 3274 | ` *   The input string.` |
|      - | 3275 | ` * $is_xhtml` |
|      - | 3276 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3277 | ` * Return` |
|      - | 3278 | ` *  The processed string.` |
|      - | 3279 | ` */` |
|      6 | 3280 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3281 |  |
|      - | 3282 | `	const char *zIn,*zCur,*zEnd;` |
|      7 | 3283 | `	int is_xhtml = 1; /* Default to XHTML-style '<br/>' like PHP */` |
|      - | 3284 | `	int nLen;` |
|      7 | 3285 | `	if( nArg < 1 ){` |
|      - | 3286 | `		/* Missing arguments,return the empty string */` |
|      3 | 3287 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3288 | `		return PH7_OK;` |
|      - | 3289 | `	}` |
|      - | 3290 | `	/* Extract the target string */` |
|      5 | 3291 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3292 | `	if( nLen < 1 ){` |
|      - | 3293 | `		/* Empty string,return null */` |
|    ! 0 | 3294 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3295 | `		return PH7_OK;` |
|      - | 3296 | `	}` |
|      5 | 3297 | `	if( nArg > 1 ){` |
|      3 | 3298 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3299 | `	}` |
|      5 | 3300 | `	zEnd = &zIn[nLen];` |
|      - | 3301 | `	/* Perform the requested operation */` |
|      4 | 3302 | `	for(;;){` |
|      9 | 3303 | `		zCur = zIn;` |
|      - | 3304 | `		/* Delimit the string */` |
|     21 | 3305 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3306 | `			zIn++;` |
|      1 | 3307 | `		}` |
|      9 | 3308 | `		if( zCur < zIn ){` |
|      - | 3309 | `			/* Output chunk verbatim */` |
|      9 | 3310 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3311 | `		}` |
|      9 | 3312 | `		if( zIn >= zEnd ){` |
|      - | 3313 | `			/* No more input to process */` |
|      5 | 3314 | `			break;` |
|      - | 3315 | `		}` |
|      - | 3316 | `		/* Output the HTML line break */` |
|      - | 3317 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br/>' (legacy without space), otherwise use '<br>' */` |
|      5 | 3318 | `		if( is_xhtml ){` |
|      3 | 3319 | `			ph7_result_string(pCtx,"<br/>",(int)sizeof("<br/>")-1);` |
|      2 | 3320 | `		}else{` |
|      3 | 3321 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3322 | `		}` |
|      5 | 3323 | `		zCur = zIn;` |
|      - | 3324 | `		/* Append trailing line */` |
|     11 | 3325 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3326 | `			zIn++;` |
|      1 | 3327 | `		}` |
|      5 | 3328 | `		if( zCur < zIn ){` |
|      - | 3329 | `			/* Output chunk verbatim */` |
|      5 | 3330 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 3331 | `		}` |
|      1 | 3332 | `	}` |
|      5 | 3333 | `	return PH7_OK;` |
|      4 | 3334 |  |
|      - | 3335 | `/*` |
|      - | 3336 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3337 | ` *  According to the PHP reference manual.` |
|      - | 3338 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3339 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3340 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 3341 | ` * This applies to both sprintf() and printf().` |
|      - | 3342 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 3343 | ` * or more of these elements, in order:` |
|      - | 3344 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 3345 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 3346 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 3347 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 3348 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 3349 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 3350 | ` *   it with a single quote ('). See the examples below.` |
|      - | 3351 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 3352 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 3353 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 3354 | ` *   should result in.` |
|      - | 3355 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 3356 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 3357 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 3358 | ` *   limit to the string.` |
|      - | 3359 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 3360 | ` *       % - a literal percent character. No argument is required.` |
|      - | 3361 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 3362 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 3363 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 3364 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 3365 | ` * 	     for the number of digits after the decimal point.` |
|      - | 3366 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 3367 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 3368 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 3369 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 3370 | ` *       g - shorter of %e and %f.` |
|      - | 3371 | ` *       G - shorter of %E and %f.` |
|      - | 3372 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 3373 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 3374 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 3375 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 3376 | ` */` |
|      - | 3377 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3378 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 3379 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 3380 | `/*` |
|      - | 3381 | `** Conversion types fall into various categories as defined by the` |
|      - | 3382 | `** following enumeration.` |
|      - | 3383 | `*/` |
|      - | 3384 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 3385 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 3386 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 3387 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 3388 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 3389 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 3390 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 3391 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 3392 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 3393 |  |
|      - | 3394 | `/*` |
|      - | 3395 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 3396 | `*/` |
|      - | 3397 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 3398 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 3399 | `/*` |
|      - | 3400 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 3401 | `** by an instance of the following structure` |
|      - | 3402 | `*/` |
|      - | 3403 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 3404 | `struct ph7_fmt_info` |
|      - | 3405 |  |
|      - | 3406 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 3407 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 3408 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 3409 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 3410 | `  char *charset; /* The character set for conversion */` |
|      - | 3411 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 3412 | `};` |
|      - | 3413 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3414 | `/*` |
|      - | 3415 | `** "*val" is a double such that 0.1 <= *val < 10.0` |
|      - | 3416 | `** Return the ascii code for the leading digit of *val, then` |
|      - | 3417 | `** multiply "*val" by 10.0 to renormalize.` |
|      - | 3418 | `**` |
|      - | 3419 | `** Example:` |
|      - | 3420 | `**     input:     *val = 3.14159` |
|      - | 3421 | `**     output:    *val = 1.4159    function return = '3'` |
|      - | 3422 | `**` |
|      - | 3423 | `** The counter *cnt is incremented each time.  After counter exceeds` |
|      - | 3424 | `** 16 (the number of significant digits in a 64-bit float) '0' is` |
|      - | 3425 | `** always returned.` |
|      - | 3426 | `*/` |
|    422 | 3427 | `static int vxGetdigit(sxlongreal *val,int *cnt)` |
|      1 | 3428 |  |
|      - | 3429 | `  sxlongreal d;` |
|      - | 3430 | `  int digit;` |
|      - | 3431 |  |
|    423 | 3432 | `  if( (*cnt)++ >= 16 ){` |
|    ! 0 | 3433 | `	  return '0';` |
|      - | 3434 | `  }` |
|    423 | 3435 | `  digit = (int)*val;` |
|    423 | 3436 | `  d = digit;` |
|    423 | 3437 | `   *val = (*val - d)*10.0;` |
|    423 | 3438 | `  return digit + '0' ;` |
|    212 | 3439 |  |
|      - | 3440 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      - | 3441 | `/*` |
|      - | 3442 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 3443 | ` * used conversion types first.` |
|      - | 3444 | ` */` |
|      - | 3445 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 3446 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 3447 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 3448 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 3449 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 3450 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 3451 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 3452 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 3453 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 3454 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3455 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3456 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 3457 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 3458 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 3459 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 3460 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 3461 | `};` |
|      - | 3462 | `/*` |
|      - | 3463 | ` * Format a given string.` |
|      - | 3464 | ` * The root program.  All variations call this core.` |
|      - | 3465 | ` * INPUTS:` |
|      - | 3466 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 3467 | ` *            1. A pointer to the call context.` |
|      - | 3468 | ` *            2. A pointer to the list of characters to be output` |
|      - | 3469 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 3470 | ` *            3. An integer number of characters to be output.` |
|      - | 3471 | ` *               (Note: This number might be zero.)` |
|      - | 3472 | ` *            4. Upper layer private data.` |
|      - | 3473 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 3474 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 3475 | ` */` |
|    136 | 3476 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 3477 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 3478 | `	ph7_context *pCtx,  /* call context */` |
|      - | 3479 | `	const char *zIn,    /* Format string */` |
|      - | 3480 | `	int nByte,          /* Format string length */` |
|      - | 3481 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 3482 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 3483 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 3484 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 3485 | `	)` |
|      1 | 3486 |  |
|    137 | 3487 | `	char spaces[] = "                                                  ";` |
|      - | 3488 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    137 | 3489 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 3490 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 3491 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 3492 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 3493 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 3494 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 3495 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 3496 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 3497 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 3498 | `	ph7_int64 iVal;` |
|      - | 3499 | `	int precision;           /* Precision of the current field */` |
|      - | 3500 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 3501 | `	int c,rc,n;` |
|      - | 3502 | `	int length;              /* Length of the field */` |
|      - | 3503 | `	int prefix;` |
|      - | 3504 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 3505 | `	int width;               /* Width of the current field */` |
|      - | 3506 | `	int idx;` |
|    137 | 3507 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 3508 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 3509 | `	/* Start the format process */` |
|    139 | 3510 | `	for(;;){` |
|    279 | 3511 | `		zCur = zIn;` |
|    739 | 3512 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|    461 | 3513 | `			zIn++;` |
|      1 | 3514 | `		}` |
|    279 | 3515 | `		if( zCur < zIn ){` |
|      - | 3516 | `			/* Consume chunk verbatim */` |
|    105 | 3517 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|    105 | 3518 | `			if( rc == SXERR_ABORT ){` |
|      - | 3519 | `				/* Callback request an operation abort */` |
|    ! 0 | 3520 | `				break;` |
|      - | 3521 | `			}` |
|     52 | 3522 | `		}` |
|    279 | 3523 | `		if( zIn >= zEnd ){` |
|      - | 3524 | `			/* No more input to process,break immediately */` |
|    135 | 3525 | `			break;` |
|      - | 3526 | `		}` |
|      - | 3527 | `		/* Find out what flags are present */` |
|    145 | 3528 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    144 | 3529 | `			flag_alternateform = flag_zeropad = 0;` |
|    145 | 3530 | `		zIn++; /* Jump the precent sign */` |
|     72 | 3531 | `		do{` |
|    177 | 3532 | `			c = zIn[0];` |
|    177 | 3533 | `			switch( c ){` |
|      9 | 3534 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      3 | 3535 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 3536 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|      9 | 3537 | `			case '#':   flag_alternateform = 1;   c = 0;   break;` |
|      9 | 3538 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 3539 | `			case '\'':` |
|    ! 0 | 3540 | `				zIn++;` |
|    ! 0 | 3541 | `				if( zIn < zEnd ){` |
|      - | 3542 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 3543 | `					c = zIn[0];` |
|    ! 0 | 3544 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3545 | `						spaces[idx] = (char)c;` |
|    ! 0 | 3546 | `					}` |
|    ! 0 | 3547 | `					c = 0;` |
|    ! 0 | 3548 | `				}` |
|    ! 0 | 3549 | `				break;` |
|    144 | 3550 | `			default:                                       break;` |
|      - | 3551 | `			}` |
|    177 | 3552 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 3553 | `		/* Get the field width */` |
|    145 | 3554 | `		width = 0;` |
|    251 | 3555 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     35 | 3556 | `			width = width*10 + (zIn[0] - '0');` |
|     35 | 3557 | `			zIn++;` |
|      1 | 3558 | `		}` |
|    145 | 3559 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 3560 | `			/* Position specifer */` |
|    ! 0 | 3561 | `			if( width > 0 ){` |
|    ! 0 | 3562 | `				n = width;` |
|    ! 0 | 3563 | `				if( vf && n > 0 ){` |
|    ! 0 | 3564 | `					n--;` |
|    ! 0 | 3565 | `				}` |
|    ! 0 | 3566 | `			}` |
|    ! 0 | 3567 | `			zIn++;` |
|    ! 0 | 3568 | `			width = 0;` |
|    ! 0 | 3569 | `			if( zIn < zEnd && zIn[0] == '0' ){` |
|    ! 0 | 3570 | `				flag_zeropad = 1;` |
|    ! 0 | 3571 | `				zIn++;` |
|    ! 0 | 3572 | `			}` |
|    ! 0 | 3573 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 3574 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 3575 | `				zIn++;` |
|    ! 0 | 3576 | `			}` |
|    ! 0 | 3577 | `		}` |
|    145 | 3578 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 3579 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 3580 | `		}` |
|      - | 3581 | `		/* Get the precision */` |
|    145 | 3582 | `		precision = -1;` |
|    145 | 3583 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     59 | 3584 | `			precision = 0;` |
|     59 | 3585 | `			zIn++;` |
|    150 | 3586 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     63 | 3587 | `				precision = precision*10 + (zIn[0] - '0');` |
|     63 | 3588 | `				zIn++;` |
|      1 | 3589 | `			}` |
|     29 | 3590 | `		}` |
|    145 | 3591 | `		if( zIn >= zEnd ){` |
|      - | 3592 | `			/* No more input */` |
|      3 | 3593 | `			break;` |
|      - | 3594 | `		}` |
|      - | 3595 | `		/* Fetch the info entry for the field */` |
|    143 | 3596 | `		pInfo = 0;` |
|    143 | 3597 | `		xtype = PH7_FMT_ERROR;` |
|    143 | 3598 | `		c = zIn[0];` |
|    143 | 3599 | `		zIn++; /* Jump the format specifer */` |
|    787 | 3600 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|    785 | 3601 | `			if( c==aFmt[idx].fmttype ){` |
|    141 | 3602 | `				pInfo = &aFmt[idx];` |
|    141 | 3603 | `				xtype = pInfo->type;` |
|    141 | 3604 | `				break;` |
|      - | 3605 | `			}` |
|    323 | 3606 | `		}` |
|    143 | 3607 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    143 | 3608 | `		length = 0;` |
|      - | 3609 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 3610 | `		 /*` |
|      - | 3611 | `		  ** At this point, variables are initialized as follows:` |
|      - | 3612 | `		  **` |
|      - | 3613 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 3614 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 3615 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 3616 | `		  **                               field width was negative.` |
|      - | 3617 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 3618 | `		  **                               the conversion character.` |
|      - | 3619 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 3620 | `		  **   width                       The specified field width.  This is` |
|      - | 3621 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 3622 | `		  **   precision                   The specified precision.  The default` |
|      - | 3623 | `		  **                               is -1.` |
|      - | 3624 | `		  */` |
|    143 | 3625 | `		switch(xtype){` |
|    ! 0 | 3626 | `		case PH7_FMT_PERCENT:` |
|      - | 3627 | `			/* A literal percent character */` |
|    ! 0 | 3628 | `			zWorker[0] = '%';` |
|    ! 0 | 3629 | `			length = (int)sizeof(char);` |
|    ! 0 | 3630 | `			break;` |
|      3 | 3631 | `		case PH7_FMT_CHARX:` |
|      - | 3632 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 3633 | `			 * with that ASCII value` |
|      - | 3634 | `			 */` |
|      7 | 3635 | `			pArg = NEXT_ARG;` |
|      7 | 3636 | `			if( pArg == 0 ){` |
|      3 | 3637 | `				c = 0;` |
|      2 | 3638 | `			}else{` |
|      5 | 3639 | `				c = ph7_value_to_int(pArg);` |
|      - | 3640 | `			}` |
|      - | 3641 | `			/* NUL byte is an acceptable value */` |
|      7 | 3642 | `			zWorker[0] = (char)c;` |
|      7 | 3643 | `			length = (int)sizeof(char);` |
|      7 | 3644 | `			break;` |
|     12 | 3645 | `		case PH7_FMT_STRING:` |
|      - | 3646 | `			/* the argument is treated as and presented as a string */` |
|     25 | 3647 | `			pArg = NEXT_ARG;` |
|     25 | 3648 | `			if( pArg == 0 ){` |
|    ! 0 | 3649 | `				length = 0;` |
|    ! 0 | 3650 | `			}else{` |
|     25 | 3651 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 3652 | `			}` |
|     25 | 3653 | `			if( length < 1 ){` |
|    ! 0 | 3654 | `				zBuf = " ";` |
|    ! 0 | 3655 | `				length = (int)sizeof(char);` |
|    ! 0 | 3656 | `			}` |
|     25 | 3657 | `			if( precision>=0 && precision<length ){` |
|      3 | 3658 | `				length = precision;` |
|      1 | 3659 | `			}` |
|     25 | 3660 | `			if( flag_zeropad ){` |
|      - | 3661 | `				/* zero-padding works on strings too */` |
|    ! 0 | 3662 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3663 | `					spaces[idx] = '0';` |
|    ! 0 | 3664 | `				}` |
|    ! 0 | 3665 | `			}` |
|     25 | 3666 | `			break;` |
|     27 | 3667 | `		case PH7_FMT_RADIX:` |
|     55 | 3668 | `			pArg = NEXT_ARG;` |
|     55 | 3669 | `			if( pArg == 0 ){` |
|    ! 0 | 3670 | `				iVal = 0;` |
|    ! 0 | 3671 | `			}else{` |
|     55 | 3672 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 3673 | `			}` |
|      - | 3674 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|     55 | 3675 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 3676 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 3677 | `			}` |
|      - | 3678 | `#if 1` |
|      - | 3679 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 3680 | `        ** I think this is stupid.*/` |
|     55 | 3681 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 3682 | `#else` |
|      - | 3683 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 3684 | `        ** but leave the prefix for hex.*/` |
|      - | 3685 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 3686 | `#endif` |
|     55 | 3687 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|     25 | 3688 | `          if( iVal<0 ){` |
|      3 | 3689 | `            iVal = -iVal;` |
|      - | 3690 | `			/* Ticket 1433-003 */` |
|      3 | 3691 | `			if( iVal < 0 ){` |
|      - | 3692 | `				/* Overflow */` |
|    ! 0 | 3693 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3694 | `			}` |
|      3 | 3695 | `            prefix = '-';` |
|     24 | 3696 | `          }else if( flag_plussign )  prefix = '+';` |
|     21 | 3697 | `          else if( flag_blanksign )  prefix = ' ';` |
|     19 | 3698 | `          else                       prefix = 0;` |
|     13 | 3699 | `        }else{` |
|     31 | 3700 | `			if( iVal<0 ){` |
|    ! 0 | 3701 | `				iVal = -iVal;` |
|      - | 3702 | `				/* Ticket 1433-003 */` |
|    ! 0 | 3703 | `				if( iVal < 0 ){` |
|      - | 3704 | `					/* Overflow */` |
|    ! 0 | 3705 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3706 | `				}` |
|    ! 0 | 3707 | `			}` |
|     31 | 3708 | `			prefix = 0;` |
|      - | 3709 | `		}` |
|     55 | 3710 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      7 | 3711 | `          precision = width-(prefix!=0);` |
|      3 | 3712 | `        }` |
|     55 | 3713 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 3714 | `        {` |
|      - | 3715 | `          register char *cset;      /* Use registers for speed */` |
|      - | 3716 | `          register int base;` |
|     55 | 3717 | `          cset = pInfo->charset;` |
|     55 | 3718 | `          base = pInfo->base;` |
|     27 | 3719 | `          do{                                           /* Convert to ascii */` |
|    123 | 3720 | `            *(--zBuf) = cset[iVal%base];` |
|    123 | 3721 | `            iVal = iVal/base;` |
|    123 | 3722 | `          }while( iVal>0 );` |
|      - | 3723 | `        }` |
|     55 | 3724 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     77 | 3725 | `        for(idx=precision-length; idx>0; idx--){` |
|     23 | 3726 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|     12 | 3727 | `        }` |
|     55 | 3728 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|     55 | 3729 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 3730 | `          char *pre, x;` |
|      9 | 3731 | `          pre = pInfo->prefix;` |
|      9 | 3732 | `          if( *zBuf!=pre[0] ){` |
|     23 | 3733 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|      4 | 3734 | `          }` |
|      4 | 3735 | `        }` |
|     55 | 3736 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     55 | 3737 | `		break;` |
|     28 | 3738 | `		case PH7_FMT_FLOAT:` |
|      - | 3739 | `		case PH7_FMT_EXP:` |
|      - | 3740 | `		case PH7_FMT_GENERIC:{` |
|      - | 3741 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3742 | `		long double realvalue;` |
|      - | 3743 | `		int  exp;                /* exponent of real numbers */` |
|      - | 3744 | `		double rounder;          /* Used for rounding floating point values */` |
|      - | 3745 | `		int flag_dp;            /* True if decimal point should be shown */` |
|      - | 3746 | `		int flag_rtz;           /* True if trailing zeros should be removed */` |
|      - | 3747 | `		int flag_exp;           /* True to force display of the exponent */` |
|      - | 3748 | `		int nsd;                 /* Number of significant digits returned */` |
|     57 | 3749 | `		pArg = NEXT_ARG;` |
|     57 | 3750 | `		if( pArg == 0 ){` |
|    ! 0 | 3751 | `			realvalue = 0;` |
|    ! 0 | 3752 | `		}else{` |
|     57 | 3753 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 3754 | `		}` |
|      - | 3755 | `		/* Special-case NaN and infinities since the normal formatting logic` |
|      - | 3756 | `		 * below assumes a finite positive realvalue. */` |
|     57 | 3757 | `		if( PH7_IS_NAN(realvalue) ){` |
|    ! 0 | 3758 | `			zBuf = "NAN";` |
|    ! 0 | 3759 | `			length = 3;` |
|    ! 0 | 3760 | `			break;` |
|      - | 3761 | `		}` |
|     57 | 3762 | `		if( PH7_IS_INF(realvalue) ){` |
|      - | 3763 | `			/* Infinity prints as INF or -INF depending on sign. */` |
|    ! 0 | 3764 | `			if( realvalue < 0.0 ){` |
|    ! 0 | 3765 | `				zBuf = "-INF";` |
|    ! 0 | 3766 | `				length = 4;` |
|    ! 0 | 3767 | `			}else{` |
|    ! 0 | 3768 | `				zBuf = "INF";` |
|    ! 0 | 3769 | `				length = 3;` |
|      - | 3770 | `			}` |
|    ! 0 | 3771 | `			break;` |
|      - | 3772 | `		}` |
|     57 | 3773 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|     57 | 3774 | `		if( precision>PH7_FMT_BUFSIZ-40) precision = PH7_FMT_BUFSIZ-40;` |
|     57 | 3775 | `        if( realvalue<0.0 ){` |
|      3 | 3776 | `          realvalue = -realvalue;` |
|      3 | 3777 | `          prefix = '-';` |
|      2 | 3778 | `        }else{` |
|     55 | 3779 | `          if( flag_plussign )          prefix = '+';` |
|     55 | 3780 | `          else if( flag_blanksign )    prefix = ' ';` |
|     55 | 3781 | `          else                         prefix = 0;` |
|      - | 3782 | `        }` |
|     57 | 3783 | `        if( pInfo->type==PH7_FMT_GENERIC && precision>0 ) precision--;` |
|     57 | 3784 | `        rounder = 0.0;` |
|      - | 3785 | `#if 0` |
|      - | 3786 | `        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */` |
|      - | 3787 | `        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);` |
|      - | 3788 | `#else` |
|      - | 3789 | `        /* It makes more sense to use 0.5 */` |
|    405 | 3790 | `        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);` |
|      - | 3791 | `#endif` |
|     57 | 3792 | `        if( pInfo->type==PH7_FMT_FLOAT ) realvalue += rounder;` |
|      - | 3793 | `        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */` |
|     57 | 3794 | `        exp = 0;` |
|     57 | 3795 | `        if( realvalue>0.0 ){` |
|     61 | 3796 | `          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }` |
|     89 | 3797 | `          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }` |
|     61 | 3798 | `          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }` |
|     71 | 3799 | `          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }` |
|     57 | 3800 | `          if( exp>350 \|\| exp<-350 ){` |
|    ! 0 | 3801 | `            zBuf = "NaN";` |
|    ! 0 | 3802 | `            length = 3;` |
|    ! 0 | 3803 | `            break;` |
|      - | 3804 | `          }` |
|     28 | 3805 | `        }` |
|     57 | 3806 | `        zBuf = zWorker;` |
|      - | 3807 | `        /*` |
|      - | 3808 | `        ** If the field type is etGENERIC, then convert to either etEXP` |
|      - | 3809 | `        ** or etFLOAT, as appropriate.` |
|      - | 3810 | `        */` |
|     57 | 3811 | `        flag_exp = xtype==PH7_FMT_EXP;` |
|     57 | 3812 | `        if( xtype!=PH7_FMT_FLOAT ){` |
|    ! 0 | 3813 | `          realvalue += rounder;` |
|    ! 0 | 3814 | `          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }` |
|    ! 0 | 3815 | `        }` |
|     57 | 3816 | `        if( xtype==PH7_FMT_GENERIC ){` |
|    ! 0 | 3817 | `          flag_rtz = !flag_alternateform;` |
|    ! 0 | 3818 | `          if( exp<-4 \|\| exp>precision ){` |
|    ! 0 | 3819 | `            xtype = PH7_FMT_EXP;` |
|    ! 0 | 3820 | `          }else{` |
|    ! 0 | 3821 | `            precision = precision - exp;` |
|    ! 0 | 3822 | `            xtype = PH7_FMT_FLOAT;` |
|      - | 3823 | `          }` |
|    ! 0 | 3824 | `        }else{` |
|     57 | 3825 | `          flag_rtz = 0;` |
|      - | 3826 | `        }` |
|      - | 3827 | `        /*` |
|      - | 3828 | `        ** The "exp+precision" test causes output to be of type etEXP if` |
|      - | 3829 | `        ** the precision is too large to fit in buf[].` |
|      - | 3830 | `        */` |
|     57 | 3831 | `        nsd = 0;` |
|     57 | 3832 | `        if( xtype==PH7_FMT_FLOAT && exp+precision<PH7_FMT_BUFSIZ-30 ){` |
|     57 | 3833 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|     57 | 3834 | `          if( prefix ) *(zBuf++) = (char)prefix;         /* Sign */` |
|     57 | 3835 | `          if( exp<0 )  *(zBuf++) = '0';            /* Digits before "." */` |
|    149 | 3836 | `          else for(; exp>=0; exp--) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     57 | 3837 | `          if( flag_dp ) *(zBuf++) = '.';           /* The decimal point */` |
|     89 | 3838 | `          for(exp++; exp<0 && precision>0; precision--, exp++){` |
|     33 | 3839 | `            *(zBuf++) = '0';` |
|     17 | 3840 | `          }` |
|    373 | 3841 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     57 | 3842 | `          *(zBuf--) = 0;                           /* Null terminate */` |
|     57 | 3843 | `          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */` |
|    ! 0 | 3844 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 3845 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 3846 | `          }` |
|     57 | 3847 | `          zBuf++;                            /* point to next free slot */` |
|     29 | 3848 | `        }else{    /* etEXP or etGENERIC */` |
|    ! 0 | 3849 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|    ! 0 | 3850 | `          if( prefix ) *(zBuf++) = (char)prefix;   /* Sign */` |
|    ! 0 | 3851 | `          *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);  /* First digit */` |
|    ! 0 | 3852 | `          if( flag_dp ) *(zBuf++) = '.';     /* Decimal point */` |
|    ! 0 | 3853 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|    ! 0 | 3854 | `          zBuf--;                            /* point to last digit */` |
|    ! 0 | 3855 | `          if( flag_rtz && flag_dp ){          /* Remove tail zeros */` |
|    ! 0 | 3856 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 3857 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 3858 | `          }` |
|    ! 0 | 3859 | `          zBuf++;                            /* point to next free slot */` |
|    ! 0 | 3860 | `          if( exp \|\| flag_exp ){` |
|    ! 0 | 3861 | `            *(zBuf++) = pInfo->charset[0];` |
|    ! 0 | 3862 | `            if( exp<0 ){ *(zBuf++) = '-'; exp = -exp; } /* sign of exp */` |
|    ! 0 | 3863 | `            else       { *(zBuf++) = '+'; }` |
|    ! 0 | 3864 | `            if( exp>=100 ){` |
|    ! 0 | 3865 | `              *(zBuf++) = (char)((exp/100)+'0');                /* 100's digit */` |
|    ! 0 | 3866 | `              exp %= 100;` |
|    ! 0 | 3867 | `            }` |
|    ! 0 | 3868 | `            *(zBuf++) = (char)(exp/10+'0');                     /* 10's digit */` |
|    ! 0 | 3869 | `            *(zBuf++) = (char)(exp%10+'0');                     /* 1's digit */` |
|    ! 0 | 3870 | `          }` |
|      - | 3871 | `        }` |
|      - | 3872 | `        /* The converted number is in buf[] and zero terminated.Output it.` |
|      - | 3873 | `        ** Note that the number is in the usual order, not reversed as with` |
|      - | 3874 | `        ** integer conversions.*/` |
|     57 | 3875 | `        length = (int)(zBuf-zWorker);` |
|     57 | 3876 | `        zBuf = zWorker;` |
|      - | 3877 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 3878 | `        ** set and we are not left justified */` |
|     57 | 3879 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 3880 | `          int i;` |
|      3 | 3881 | `          int nPad = width - length;` |
|     13 | 3882 | `          for(i=width; i>=nPad; i--){` |
|     11 | 3883 | `            zBuf[i] = zBuf[i-nPad];` |
|      6 | 3884 | `          }` |
|      3 | 3885 | `          i = prefix!=0;` |
|      5 | 3886 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      3 | 3887 | `          length = width;` |
|      1 | 3888 | `        }` |
|      - | 3889 | `#else` |
|      - | 3890 | `         zBuf = " ";` |
|      - | 3891 | `		 length = (int)sizeof(char);` |
|      - | 3892 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|     57 | 3893 | `		 break;` |
|      - | 3894 | `							 }` |
|      1 | 3895 | `		default:` |
|      - | 3896 | `			/* Invalid format specifer */` |
|      3 | 3897 | `			zWorker[0] = '?';` |
|      3 | 3898 | `			length = (int)sizeof(char);` |
|      2 | 3899 | `			break;` |
|      - | 3900 | `		}` |
|      - | 3901 | `		 /*` |
|      - | 3902 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 3903 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 3904 | `		 ** the output.` |
|      - | 3905 | `		 */` |
|    143 | 3906 | `    if( !flag_leftjustify ){` |
|      - | 3907 | `      register int nspace;` |
|    135 | 3908 | `      nspace = width-length;` |
|    135 | 3909 | `      if( nspace>0 ){` |
|      5 | 3910 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3911 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3912 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3913 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3914 | `			}` |
|    ! 0 | 3915 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3916 | `        }` |
|      5 | 3917 | `        if( nspace>0 ){` |
|      5 | 3918 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      5 | 3919 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3920 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3921 | `			}` |
|      2 | 3922 | `		}` |
|      2 | 3923 | `      }` |
|     67 | 3924 | `    }` |
|    143 | 3925 | `    if( length>0 ){` |
|    143 | 3926 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    143 | 3927 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3928 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3929 | `		}` |
|     71 | 3930 | `    }` |
|    143 | 3931 | `    if( flag_leftjustify ){` |
|      - | 3932 | `      register int nspace;` |
|      9 | 3933 | `      nspace = width-length;` |
|      9 | 3934 | `      if( nspace>0 ){` |
|      9 | 3935 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3936 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3937 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3938 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3939 | `			}` |
|    ! 0 | 3940 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3941 | `        }` |
|      9 | 3942 | `        if( nspace>0 ){` |
|      9 | 3943 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      9 | 3944 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3945 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3946 | `			}` |
|      4 | 3947 | `		}` |
|      4 | 3948 | `      }` |
|      4 | 3949 | `    }` |
|      1 | 3950 | ` }/* for(;;) */` |
|    137 | 3951 | `	return SXRET_OK;` |
|     69 | 3952 |  |
|      - | 3953 | `/*` |
|      - | 3954 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 3955 | ` */` |
|     90 | 3956 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 3957 |  |
|      - | 3958 | `	/* Consume directly */` |
|     91 | 3959 | `	ph7_result_string(pCtx,zInput,nLen);` |
|     45 | 3960 | `	SXUNUSED(pUserData); /* cc warning */` |
|     91 | 3961 | `	return PH7_OK;` |
|      1 | 3962 |  |
|      - | 3963 | `/*` |
|      - | 3964 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 3965 | ` *  Return a formatted string.` |
|      - | 3966 | ` * Parameters` |
|      - | 3967 | ` *  $format` |
|      - | 3968 | ` *    The format string (see block comment above)` |
|      - | 3969 | ` * Return` |
|      - | 3970 | ` *  A string produced according to the formatting string format.` |
|      - | 3971 | ` */` |
|     62 | 3972 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3973 |  |
|      - | 3974 | `	const char *zFormat;` |
|      - | 3975 | `	int nLen;` |
|     63 | 3976 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3977 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 3978 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3979 | `		return PH7_OK;` |
|      - | 3980 | `	}` |
|      - | 3981 | `	/* Extract the string format */` |
|     61 | 3982 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     61 | 3983 | `	if( nLen < 1 ){` |
|      - | 3984 | `		/* Empty string */` |
|    ! 0 | 3985 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3986 | `		return PH7_OK;` |
|      - | 3987 | `	}` |
|      - | 3988 | `	/* Format the string */` |
|     61 | 3989 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,0,FALSE);` |
|     61 | 3990 | `	return PH7_OK;` |
|     32 | 3991 |  |
|      - | 3992 | `/*` |
|      - | 3993 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 3994 | ` */` |
|    130 | 3995 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 3996 |  |
|    131 | 3997 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 3998 | `	/* Call the VM output consumer directly */` |
|    131 | 3999 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 4000 | `	/* Increment counter */` |
|    131 | 4001 | `	*pCounter += nLen;` |
|    131 | 4002 | `	return PH7_OK;` |
|      1 | 4003 |  |
|      - | 4004 | `/*` |
|      - | 4005 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 4006 | ` *  Output a formatted string.` |
|      - | 4007 | ` * Parameters` |
|      - | 4008 | ` *  $format` |
|      - | 4009 | ` *   See sprintf() for a description of format.` |
|      - | 4010 | ` * Return` |
|      - | 4011 | ` *  The length of the outputted string.` |
|      - | 4012 | ` */` |
|     52 | 4013 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4014 |  |
|     53 | 4015 | `	ph7_int64 nCounter = 0;` |
|      - | 4016 | `	const char *zFormat;` |
|      - | 4017 | `	int nLen;` |
|     53 | 4018 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4019 | `		/* Missing/Invalid arguments,return 0 */` |
|      3 | 4020 | `		ph7_result_int(pCtx,0);` |
|      3 | 4021 | `		return PH7_OK;` |
|      - | 4022 | `	}` |
|      - | 4023 | `	/* Extract the string format */` |
|     51 | 4024 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     51 | 4025 | `	if( nLen < 1 ){` |
|      - | 4026 | `		/* Empty string */` |
|    ! 0 | 4027 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4028 | `		return PH7_OK;` |
|      - | 4029 | `	}` |
|      - | 4030 | `	/* Format the string */` |
|     51 | 4031 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 4032 | `	/* Return the length of the outputted string */` |
|     51 | 4033 | `	ph7_result_int64(pCtx,nCounter);` |
|     51 | 4034 | `	return PH7_OK;` |
|     27 | 4035 |  |
|      - | 4036 | `/*` |
|      - | 4037 | ` * int vprintf(string $format,array $args)` |
|      - | 4038 | ` *  Output a formatted string.` |
|      - | 4039 | ` * Parameters` |
|      - | 4040 | ` *  $format` |
|      - | 4041 | ` *   See sprintf() for a description of format.` |
|      - | 4042 | ` * Return` |
|      - | 4043 | ` *  The length of the outputted string.` |
|      - | 4044 | ` */` |
|      2 | 4045 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4046 |  |
|      3 | 4047 | `	ph7_int64 nCounter = 0;` |
|      - | 4048 | `	const char *zFormat;` |
|      - | 4049 | `	ph7_hashmap *pMap;` |
|      - | 4050 | `	SySet sArg;` |
|      - | 4051 | `	int nLen,n;` |
|      3 | 4052 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4053 | `		/* Missing/Invalid arguments,return 0 */` |
|    ! 0 | 4054 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4055 | `		return PH7_OK;` |
|      - | 4056 | `	}` |
|      - | 4057 | `	/* Extract the string format */` |
|      3 | 4058 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4059 | `	if( nLen < 1 ){` |
|      - | 4060 | `		/* Empty string */` |
|    ! 0 | 4061 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4062 | `		return PH7_OK;` |
|      - | 4063 | `	}` |
|      - | 4064 | `	/* Point to the hashmap */` |
|      3 | 4065 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4066 | `	/* Extract arguments from the hashmap */` |
|      3 | 4067 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4068 | `	/* Format the string */` |
|      3 | 4069 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 4070 | `	/* Return the length of the outputted string */` |
|      3 | 4071 | `	ph7_result_int64(pCtx,nCounter);` |
|      - | 4072 | `	/* Release the container */` |
|      3 | 4073 | `	SySetRelease(&sArg);` |
|      3 | 4074 | `	return PH7_OK;` |
|      2 | 4075 |  |
|      - | 4076 | `/*` |
|      - | 4077 | ` * int vsprintf(string $format,array $args)` |
|      - | 4078 | ` *  Output a formatted string.` |
|      - | 4079 | ` * Parameters` |
|      - | 4080 | ` *  $format` |
|      - | 4081 | ` *   See sprintf() for a description of format.` |
|      - | 4082 | ` * Return` |
|      - | 4083 | ` *  A string produced according to the formatting string format.` |
|      - | 4084 | ` */` |
|     10 | 4085 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4086 |  |
|      - | 4087 | `	const char *zFormat;` |
|      - | 4088 | `	ph7_hashmap *pMap;` |
|      - | 4089 | `	SySet sArg;` |
|      - | 4090 | `	int nLen,n;` |
|     11 | 4091 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4092 | `		/* Missing/Invalid arguments,return the empty string */` |
|      5 | 4093 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 4094 | `		return PH7_OK;` |
|      - | 4095 | `	}` |
|      - | 4096 | `	/* Extract the string format */` |
|      7 | 4097 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4098 | `	if( nLen < 1 ){` |
|      - | 4099 | `		/* Empty string */` |
|    ! 0 | 4100 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4101 | `		return PH7_OK;` |
|      - | 4102 | `	}` |
|      - | 4103 | `	/* Point to hashmap */` |
|      7 | 4104 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4105 | `	/* Extract arguments from the hashmap */` |
|      7 | 4106 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4107 | `	/* Format the string */` |
|      7 | 4108 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),0,TRUE);` |
|      - | 4109 | `	/* Release the container */` |
|      7 | 4110 | `	SySetRelease(&sArg);` |
|      7 | 4111 | `	return PH7_OK;` |
|      6 | 4112 |  |
|      - | 4113 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 4114 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4115 | `/*` |
|      - | 4116 | ` * Symisc eXtension.` |
|      - | 4117 | ` * string size_format(int64 $size)` |
|      - | 4118 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 4119 | ` *  Example:` |
|      - | 4120 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 4121 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 4122 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 4123 | ` * Parameter` |
|      - | 4124 | ` *  $size` |
|      - | 4125 | ` *    Entity size in bytes.` |
|      - | 4126 | ` * Return` |
|      - | 4127 | ` *   Formatted string representation of the given size.` |
|      - | 4128 | ` */` |
|     24 | 4129 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4130 |  |
|      - | 4131 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 4132 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 4133 | `	sxi32 nRest,i_32;` |
|      - | 4134 | `	ph7_int64 iSize;` |
|     25 | 4135 | `	int c = -1; /* index in zUnit[] */` |
|      - | 4136 |  |
|     25 | 4137 | `	if( nArg < 1 ){` |
|      - | 4138 | `		/* Missing argument,return the empty string */` |
|      3 | 4139 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4140 | `		return PH7_OK;` |
|      - | 4141 | `	}` |
|      - | 4142 | `	/* Extract the given size */` |
|     23 | 4143 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 4144 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 4145 | `		/* Don't bother formatting,return immediately */` |
|      5 | 4146 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 4147 | `		return PH7_OK;` |
|      - | 4148 | `	}` |
|     19 | 4149 | `	for(;;){` |
|     39 | 4150 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 4151 | `		iSize >>= 10;` |
|     39 | 4152 | `		c++;` |
|     39 | 4153 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 4154 | `			break;` |
|      - | 4155 | `		}` |
|      1 | 4156 | `	}` |
|     19 | 4157 | `	nRest /= 100;` |
|     19 | 4158 | `	if( nRest > 9 ){` |
|    ! 0 | 4159 | `		nRest = 9;` |
|    ! 0 | 4160 | `	}` |
|     19 | 4161 | `	if( iSize > 999 ){` |
|    ! 0 | 4162 | `		c++;` |
|    ! 0 | 4163 | `		nRest = 9;` |
|    ! 0 | 4164 | `		iSize = 0;` |
|    ! 0 | 4165 | `	}` |
|     19 | 4166 | `	i_32 = (sxi32)iSize;` |
|      - | 4167 | `	/* Format */` |
|     19 | 4168 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 4169 | `	return PH7_OK;` |
|     13 | 4170 |  |
|      - | 4171 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4172 | `/*` |
|      - | 4173 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 4174 | ` *   Calculate the md5 hash of a string.` |
|      - | 4175 | ` * Parameter` |
|      - | 4176 | ` *  $str` |
|      - | 4177 | ` *   Input string` |
|      - | 4178 | ` * $raw_output` |
|      - | 4179 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4180 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4181 | ` * Return` |
|      - | 4182 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 4183 | ` */` |
|     10 | 4184 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4185 |  |
|      - | 4186 | `	unsigned char zDigest[16];` |
|     11 | 4187 | `	int raw_output = FALSE;` |
|      - | 4188 | `	const void *pIn;` |
|      - | 4189 | `	int nLen;` |
|     11 | 4190 | `	if( nArg < 1 ){` |
|      - | 4191 | `		/* Missing arguments,return the empty string */` |
|      3 | 4192 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4193 | `		return PH7_OK;` |
|      - | 4194 | `	}` |
|      - | 4195 | `	/* Extract the input string */` |
|      9 | 4196 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 4197 | `	if( nLen < 1 ){` |
|      - | 4198 | `		/* Empty string */` |
|    ! 0 | 4199 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4200 | `		return PH7_OK;` |
|      - | 4201 | `	}` |
|      9 | 4202 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      3 | 4203 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 | 4204 | `	}` |
|      - | 4205 | `	/* Compute the MD5 digest */` |
|      9 | 4206 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|      9 | 4207 | `	if( raw_output ){` |
|      - | 4208 | `		/* Output raw digest */` |
|      3 | 4209 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      2 | 4210 | `	}else{` |
|      - | 4211 | `		/* Perform a binary to hex conversion */` |
|      7 | 4212 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4213 | `	}` |
|      9 | 4214 | `	return PH7_OK;` |
|      6 | 4215 |  |
|      - | 4216 | `/*` |
|      - | 4217 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 4218 | ` *   Calculate the sha1 hash of a string.` |
|      - | 4219 | ` * Parameter` |
|      - | 4220 | ` *  $str` |
|      - | 4221 | ` *   Input string` |
|      - | 4222 | ` * $raw_output` |
|      - | 4223 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4224 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4225 | ` * Return` |
|      - | 4226 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 4227 | ` */` |
|      8 | 4228 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4229 |  |
|      - | 4230 | `	unsigned char zDigest[20];` |
|      9 | 4231 | `	int raw_output = FALSE;` |
|      - | 4232 | `	const void *pIn;` |
|      - | 4233 | `	int nLen;` |
|      9 | 4234 | `	if( nArg < 1 ){` |
|      - | 4235 | `		/* Missing arguments,return the empty string */` |
|      3 | 4236 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4237 | `		return PH7_OK;` |
|      - | 4238 | `	}` |
|      - | 4239 | `	/* Extract the input string */` |
|      7 | 4240 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4241 | `	if( nLen < 1 ){` |
|      - | 4242 | `		/* Empty string */` |
|    ! 0 | 4243 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4244 | `		return PH7_OK;` |
|      - | 4245 | `	}` |
|      7 | 4246 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      3 | 4247 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 | 4248 | `	}` |
|      - | 4249 | `	/* Compute the SHA1 digest */` |
|      7 | 4250 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|      7 | 4251 | `	if( raw_output ){` |
|      - | 4252 | `		/* Output raw digest */` |
|      3 | 4253 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      2 | 4254 | `	}else{` |
|      - | 4255 | `		/* Perform a binary to hex conversion */` |
|      5 | 4256 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4257 | `	}` |
|      7 | 4258 | `	return PH7_OK;` |
|      5 | 4259 |  |
|      - | 4260 | `/*` |
|      - | 4261 | ` * int64 crc32(string $str)` |
|      - | 4262 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 4263 | ` * Parameter` |
|      - | 4264 | ` *  $str` |
|      - | 4265 | ` *   Input string` |
|      - | 4266 | ` * Return` |
|      - | 4267 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 4268 | ` */` |
|      4 | 4269 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4270 |  |
|      - | 4271 | `	const void *pIn;` |
|      - | 4272 | `	sxu32 nCRC;` |
|      - | 4273 | `	int nLen;` |
|      5 | 4274 | `	if( nArg < 1 ){` |
|      - | 4275 | `		/* Missing arguments,return 0 */` |
|      3 | 4276 | `		ph7_result_int(pCtx,0);` |
|      3 | 4277 | `		return PH7_OK;` |
|      - | 4278 | `	}` |
|      - | 4279 | `	/* Extract the input string */` |
|      3 | 4280 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4281 | `	if( nLen < 1 ){` |
|      - | 4282 | `		/* Empty string */` |
|    ! 0 | 4283 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4284 | `		return PH7_OK;` |
|      - | 4285 | `	}` |
|      - | 4286 | `	/* Calculate the sum */` |
|      3 | 4287 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 4288 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 4289 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 4290 | `	return PH7_OK;` |
|      3 | 4291 |  |
|      - | 4292 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 4293 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4294 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 4295 | `/*` |
|      - | 4296 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 4297 |  |
|      - | 4298 | ` */` |
|      4 | 4299 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 4300 | `	const char *zInput, /* Raw input */` |
|      - | 4301 | `	int nByte,  /* Input length */` |
|      - | 4302 | `	int delim,  /* Delimiter */` |
|      - | 4303 | `	int encl,   /* Enclosure */` |
|      - | 4304 | `	int escape,  /* Escape character */` |
|      - | 4305 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 4306 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 4307 | `	)` |
|      1 | 4308 |  |
|      5 | 4309 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 4310 | `	const char *zIn = zInput;` |
|      - | 4311 | `	const char *zPtr;` |
|      - | 4312 | `	int isEnc;` |
|      - | 4313 | `	/* Start processing */` |
|      8 | 4314 | `	for(;;){` |
|     17 | 4315 | `		if( zIn >= zEnd ){` |
|      - | 4316 | `			/* No more input to process */` |
|      5 | 4317 | `			break;` |
|      - | 4318 | `		}` |
|     13 | 4319 | `		isEnc = 0;` |
|     13 | 4320 | `		zPtr = zIn;` |
|      - | 4321 | `		/* Find the first delimiter */` |
|     27 | 4322 | `		while( zIn < zEnd ){` |
|     23 | 4323 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 4324 | `				/* Delimiter found,break imediately */` |
|      5 | 4325 | `				break;` |
|     15 | 4326 | `			}else if( zIn[0] == encl ){` |
|      - | 4327 | `				/* Inside enclosure? */` |
|    ! 0 | 4328 | `				isEnc = !isEnc;` |
|     15 | 4329 | `			}else if( zIn[0] == escape ){` |
|      - | 4330 | `				/* Escape sequence */` |
|    ! 0 | 4331 | `				zIn++;` |
|    ! 0 | 4332 | `			}` |
|      - | 4333 | `			/* Advance the cursor */` |
|     15 | 4334 | `			zIn++;` |
|      1 | 4335 | `		}` |
|     13 | 4336 | `		if( zIn > zPtr ){` |
|     13 | 4337 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 4338 | `			sxi32 rc;` |
|      - | 4339 | `			/* Invoke the supllied callback */` |
|     13 | 4340 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 4341 | `				zPtr++;` |
|    ! 0 | 4342 | `				nByteChunk-=2;` |
|    ! 0 | 4343 | `			}` |
|     13 | 4344 | `			if( nByteChunk > 0 ){` |
|     13 | 4345 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 4346 | `				if( rc == SXERR_ABORT ){` |
|      - | 4347 | `					/* User callback request an operation abort */` |
|    ! 0 | 4348 | `					break;` |
|      - | 4349 | `				}` |
|      6 | 4350 | `			}` |
|      6 | 4351 | `		}` |
|      - | 4352 | `		/* Ignore trailing delimiter */` |
|     21 | 4353 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 4354 | `			zIn++;` |
|      1 | 4355 | `		}` |
|      1 | 4356 | `	}` |
|      5 | 4357 | `	return SXRET_OK;` |
|      1 | 4358 |  |
|      - | 4359 | `/*` |
|      - | 4360 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 4361 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 4362 | ` * argument to this callback.` |
|      - | 4363 | ` */` |
|     12 | 4364 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 4365 |  |
|     13 | 4366 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 4367 | `	ph7_value sEntry;` |
|      - | 4368 | `	SyString sToken;` |
|      - | 4369 | `	/* Insert the token in the given array */` |
|     13 | 4370 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 4371 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 4372 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 4373 | `	if( sToken.nByte < 1){` |
|    ! 0 | 4374 | `		return SXRET_OK;` |
|      - | 4375 | `	}` |
|     13 | 4376 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 4377 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 4378 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 4379 | `	return SXRET_OK;` |
|      7 | 4380 |  |
|      - | 4381 | `/*` |
|      - | 4382 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 4383 | ` *  Parse a CSV string into an array.` |
|      - | 4384 | ` * Parameters` |
|      - | 4385 | ` *  $input` |
|      - | 4386 | ` *   The string to parse.` |
|      - | 4387 | ` *  $delimiter` |
|      - | 4388 | ` *   Set the field delimiter (one character only).` |
|      - | 4389 | ` *  $enclosure` |
|      - | 4390 | ` *   Set the field enclosure character (one character only).` |
|      - | 4391 | ` *  $escape` |
|      - | 4392 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 4393 | ` * Return` |
|      - | 4394 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 4395 | ` */` |
|      4 | 4396 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4397 |  |
|      - | 4398 | `	const char *zInput,*zPtr;` |
|      - | 4399 | `	ph7_value *pArray;` |
|      5 | 4400 | `	int delim  = ',';   /* Delimiter */` |
|      5 | 4401 | `	int encl   = '"' ;  /* Enclosure */` |
|      5 | 4402 | `	int escape = '\\';  /* Escape character */` |
|      - | 4403 | `	int nLen;` |
|      5 | 4404 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4405 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 4406 | `		ph7_result_null(pCtx);` |
|      3 | 4407 | `		return PH7_OK;` |
|      - | 4408 | `	}` |
|      - | 4409 | `	/* Extract the raw input */` |
|      3 | 4410 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4411 | `	if( nArg > 1 ){` |
|      - | 4412 | `		int i;` |
|      3 | 4413 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 4414 | `			/* Extract the delimiter */` |
|      3 | 4415 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 4416 | `			if( i > 0 ){` |
|      3 | 4417 | `				delim = zPtr[0];` |
|      1 | 4418 | `			}` |
|      1 | 4419 | `		}` |
|      3 | 4420 | `		if( nArg > 2 ){` |
|      3 | 4421 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 4422 | `				/* Extract the enclosure */` |
|      3 | 4423 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 4424 | `				if( i > 0 ){` |
|      3 | 4425 | `					encl = zPtr[0];` |
|      1 | 4426 | `				}` |
|      1 | 4427 | `			}` |
|      3 | 4428 | `			if( nArg > 3 ){` |
|      3 | 4429 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 4430 | `					/* Extract the escape character */` |
|      3 | 4431 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 4432 | `					if( i > 0 ){` |
|      3 | 4433 | `						escape = zPtr[0];` |
|      1 | 4434 | `					}` |
|      1 | 4435 | `				}` |
|      1 | 4436 | `			}` |
|      1 | 4437 | `		}` |
|      1 | 4438 | `	}` |
|      - | 4439 | `	/* Create our array */` |
|      3 | 4440 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4441 | `	if( pArray == 0 ){` |
|    ! 0 | 4442 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4443 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4444 | `		return PH7_OK;` |
|      - | 4445 | `	}` |
|      - | 4446 | `	/* Parse the raw input */` |
|      3 | 4447 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 4448 | `	/* Return the freshly created array */` |
|      3 | 4449 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4450 | `	return PH7_OK;` |
|      3 | 4451 |  |
|      - | 4452 | `/*` |
|      - | 4453 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 4454 | ` * container.` |
|      - | 4455 | ` * Refer to [strip_tags()].` |
|      - | 4456 | ` */` |
|     10 | 4457 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 4458 |  |
|     11 | 4459 | `	const char *zEnd = &zTag[nByte];` |
|      - | 4460 | `	const char *zPtr;` |
|      - | 4461 | `	SyString sEntry;` |
|      - | 4462 | `	/* Strip tags */` |
|     10 | 4463 | `	for(;;){` |
|     45 | 4464 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 4465 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 4466 | `				zTag++;` |
|      1 | 4467 | `		}` |
|     21 | 4468 | `		if( zTag >= zEnd ){` |
|     11 | 4469 | `			break;` |
|      - | 4470 | `		}` |
|     11 | 4471 | `		zPtr = zTag;` |
|      - | 4472 | `		/* Delimit the tag */` |
|     25 | 4473 | `		while(zTag < zEnd ){` |
|     25 | 4474 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 4475 | `				/* UTF-8 stream */` |
|      3 | 4476 | `				zTag++;` |
|      5 | 4477 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 4478 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 4479 | `				break;` |
|    ! 0 | 4480 | `			}else{` |
|     13 | 4481 | `				zTag++;` |
|      - | 4482 | `			}` |
|      1 | 4483 | `		}` |
|     11 | 4484 | `		if( zTag > zPtr ){` |
|      - | 4485 | `			/* Perform the insertion */` |
|     11 | 4486 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 4487 | `			SyStringFullTrim(&sEntry);` |
|     11 | 4488 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 4489 | `		}` |
|      - | 4490 | `		/* Jump the trailing '>' */` |
|     11 | 4491 | `		zTag++;` |
|      1 | 4492 | `	}` |
|     11 | 4493 | `	return SXRET_OK;` |
|      1 | 4494 |  |
|      - | 4495 | `/*` |
|      - | 4496 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 4497 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 4498 | ` * Refer to [strip_tags()].` |
|      - | 4499 | ` */` |
|     36 | 4500 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 4501 |  |
|     37 | 4502 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 4503 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 4504 | `		SyString sTag;` |
|     85 | 4505 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 4506 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 4507 | `			zTag++;` |
|      1 | 4508 | `		}` |
|      - | 4509 | `		/* Delimit the tag */` |
|     25 | 4510 | `		zCur = zTag;` |
|     77 | 4511 | `		while(zTag < zEnd ){` |
|     77 | 4512 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 4513 | `				/* UTF-8 stream */` |
|      5 | 4514 | `				zTag++;` |
|      9 | 4515 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 4516 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 4517 | `				break;` |
|    ! 0 | 4518 | `			}else{` |
|     49 | 4519 | `				zTag++;` |
|      - | 4520 | `			}` |
|      1 | 4521 | `		}` |
|     25 | 4522 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 4523 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 4524 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 4525 | `		if( sTag.nByte > 0 ){` |
|      - | 4526 | `			SyString *aEntry,*pEntry;` |
|      - | 4527 | `			sxi32 rc;` |
|      - | 4528 | `			sxu32 n;` |
|      - | 4529 | `			/* Perform the lookup */` |
|     25 | 4530 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 4531 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 4532 | `				pEntry = &aEntry[n];` |
|      - | 4533 | `				/* Do the comparison */` |
|     25 | 4534 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 4535 | `				if( !rc ){` |
|     21 | 4536 | `					return SXRET_OK;` |
|      - | 4537 | `				}` |
|      3 | 4538 | `			}` |
|      2 | 4539 | `		}` |
|      2 | 4540 | `	}` |
|      - | 4541 | `	/* No such tag */` |
|     17 | 4542 | `	return SXERR_NOTFOUND;` |
|     19 | 4543 |  |
|      - | 4544 | `/*` |
|      - | 4545 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 4546 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 4547 | ` * Refer to [strip_tags()].` |
|      - | 4548 | ` */` |
|     16 | 4549 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 4550 |  |
|     17 | 4551 | `	const char *zEnd = &zIn[nByte];` |
|      - | 4552 | `	const char *zPtr,*zTag;` |
|      - | 4553 | `	SySet sSet;` |
|      - | 4554 | `	/* initialize the set of allowed tags */` |
|     17 | 4555 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 4556 | `	if( nTaglen > 0 ){` |
|      - | 4557 | `		/* Set of allowed tags */` |
|     11 | 4558 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 4559 | `	}` |
|      - | 4560 | `	/* Set the empty string */` |
|     17 | 4561 | `	ph7_result_string(pCtx,"",0);` |
|      - | 4562 | `	/* Start processing */` |
|     26 | 4563 | `	for(;;){` |
|     53 | 4564 | `		if(zIn >= zEnd){` |
|      - | 4565 | `			/* No more input to process */` |
|     15 | 4566 | `			break;` |
|      - | 4567 | `		}` |
|     39 | 4568 | `		zPtr = zIn;` |
|      - | 4569 | `		/* Find a tag */` |
|    133 | 4570 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 4571 | `			zIn++;` |
|      1 | 4572 | `		}` |
|     39 | 4573 | `		if( zIn > zPtr ){` |
|      - | 4574 | `			/* Consume raw input */` |
|     21 | 4575 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 4576 | `		}` |
|      - | 4577 | `		/* Ignore trailing null bytes */` |
|     39 | 4578 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 4579 | `			zIn++;` |
|    ! 0 | 4580 | `		}` |
|     39 | 4581 | `		if(zIn >= zEnd){` |
|      - | 4582 | `			/* No more input to process */` |
|      3 | 4583 | `			break;` |
|      - | 4584 | `		}` |
|     37 | 4585 | `		if( zIn[0] == '<' ){` |
|      - | 4586 | `			sxi32 rc;` |
|     37 | 4587 | `			zTag = zIn++;` |
|      - | 4588 | `			/* Delimit the tag */` |
|    127 | 4589 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 4590 | `				zIn++;` |
|      1 | 4591 | `			}` |
|     37 | 4592 | `			if( zIn < zEnd ){` |
|     37 | 4593 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 4594 | `			}` |
|      - | 4595 | `			/* Query the set */` |
|     37 | 4596 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 4597 | `			if( rc == SXRET_OK ){` |
|      - | 4598 | `				/* Keep the tag */` |
|     21 | 4599 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 4600 | `			}` |
|     18 | 4601 | `		}` |
|      1 | 4602 | `	}` |
|      - | 4603 | `	/* Cleanup */` |
|     17 | 4604 | `	SySetRelease(&sSet);` |
|     17 | 4605 | `	return SXRET_OK;` |
|      1 | 4606 |  |
|      - | 4607 | `/*` |
|      - | 4608 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 4609 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 4610 | ` * Parameters` |
|      - | 4611 | ` *  $str` |
|      - | 4612 | ` *  The input string.` |
|      - | 4613 | ` * $allowable_tags` |
|      - | 4614 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 4615 | ` * Return` |
|      - | 4616 | ` *  Returns the stripped string.` |
|      - | 4617 | ` */` |
|     16 | 4618 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4619 |  |
|     17 | 4620 | `	const char *zTaglist = 0;` |
|      - | 4621 | `	const char *zString;` |
|     17 | 4622 | `	int nTaglen = 0;` |
|      - | 4623 | `	int nLen;` |
|     17 | 4624 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4625 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 4626 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4627 | `		return PH7_OK;` |
|      - | 4628 | `	}` |
|      - | 4629 | `	/* Point to the raw string */` |
|     15 | 4630 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 4631 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 4632 | `		/* Allowed tag */` |
|     11 | 4633 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 4634 | `	}` |
|      - | 4635 | `	/* Process input */` |
|     15 | 4636 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 4637 | `	return PH7_OK;` |
|      9 | 4638 |  |
|      - | 4639 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 4640 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4641 | `/*` |
|      - | 4642 | ` * string str_shuffle(string $str)` |
|      - | 4643 |  |
|      - | 4644 | ` *  Randomly shuffles a string.` |
|      - | 4645 | ` * Parameters` |
|      - | 4646 | ` *  $str` |
|      - | 4647 | ` *   The input string.` |
|      - | 4648 | ` * Return` |
|      - | 4649 | ` *  Returns the shuffled string.` |
|      - | 4650 | ` */` |
|     12 | 4651 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4652 |  |
|      - | 4653 | `	const char *zString;` |
|      - | 4654 | `	int nLen,i,c;` |
|      - | 4655 | `	sxu32 iR;` |
|     13 | 4656 | `	if( nArg < 1 ){` |
|      - | 4657 | `		/* Missing arguments,return the empty string */` |
|      3 | 4658 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4659 | `		return PH7_OK;` |
|      - | 4660 | `	}` |
|      - | 4661 | `	/* Extract the target string */` |
|     11 | 4662 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 4663 | `	if( nLen < 1 ){` |
|      - | 4664 | `		/* Nothing to shuffle */` |
|      3 | 4665 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4666 | `		return PH7_OK;` |
|      - | 4667 | `	}` |
|      - | 4668 | `	/* Shuffle the string */` |
|     43 | 4669 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 4670 | `		/* Generate a random number first */` |
|     35 | 4671 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 4672 | `		/* Extract a random offset */` |
|     35 | 4673 | `		c = zString[iR % nLen];` |
|      - | 4674 | `		/* Append it */` |
|     35 | 4675 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 4676 | `	}` |
|      9 | 4677 | `	return PH7_OK;` |
|      7 | 4678 |  |
|      - | 4679 | `/*` |
|      - | 4680 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 4681 | ` *  Convert a string to an array.` |
|      - | 4682 | ` * Parameters` |
|      - | 4683 | ` * $string` |
|      - | 4684 | ` *  The input string.` |
|      - | 4685 | ` * $split_length` |
|      - | 4686 | ` *  Maximum length of the chunk.` |
|      - | 4687 | ` * Return` |
|      - | 4688 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 4689 | ` *  except possibly the last one which may be shorter.` |
|      - | 4690 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 4691 | ` *  as the first (and only) array element.` |
|      - | 4692 | ` *  An empty string returns an empty array.` |
|      - | 4693 | ` * Errors` |
|      - | 4694 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 4695 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 4696 | ` *  ValueError if $split_length is less than 1.` |
|      - | 4697 | ` */` |
|     28 | 4698 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4699 |  |
|      - | 4700 | `	const char *zString,*zEnd;` |
|      - | 4701 | `	ph7_value *pArray,*pValue;` |
|      - | 4702 | `	int split_len;` |
|      - | 4703 | `	int nLen;` |
|     30 | 4704 | `	if( nArg < 1 ){` |
|      4 | 4705 | `		return PH7_VmThrowException(pCtx,` |
|      - | 4706 | `			"ArgumentCountError",` |
|      - | 4707 | `			"str_split() expects at least 1 argument, %d given",` |
|      1 | 4708 | `			nArg` |
|      - | 4709 | `			);` |
|      - | 4710 | `	}` |
|      - | 4711 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     50 | 4712 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     38 | 4713 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 4714 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 4715 | `		return PH7_VmThrowException(pCtx,` |
|      - | 4716 | `			"TypeError",` |
|      - | 4717 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 4718 | `			ph7_type_name(apArg[0])` |
|      - | 4719 | `			);` |
|      - | 4720 | `	}` |
|      - | 4721 | `	/* Point to the target string */` |
|     26 | 4722 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     26 | 4723 | `	split_len = (int)sizeof(char);` |
|     26 | 4724 | `	if( nArg > 1 ){` |
|      - | 4725 | `		/* Split length */` |
|     16 | 4726 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     16 | 4727 | `		if( split_len < 1 ){` |
|      5 | 4728 | `			return PH7_VmThrowException(pCtx,` |
|      - | 4729 | `				"ValueError",` |
|      - | 4730 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 4731 | `				);` |
|      - | 4732 | `		}` |
|     11 | 4733 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 4734 | `			split_len = nLen;` |
|      1 | 4735 | `		}` |
|      5 | 4736 | `	}` |
|      - | 4737 | `	/* Create the array and the scalar value */` |
|     21 | 4738 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 4739 | `	/*Chunk value */` |
|     21 | 4740 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 4741 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 4742 | `		/* Return FALSE */` |
|    ! 0 | 4743 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4744 | `		return PH7_OK;` |
|      - | 4745 | `	}` |
|      - | 4746 | `	/* Point to the end of the string */` |
|     21 | 4747 | `	zEnd = &zString[nLen];` |
|      - | 4748 | `	/* Perform the requested operation */` |
|     48 | 4749 | `	for(;;){` |
|      - | 4750 | `		int nMax;` |
|     59 | 4751 | `		if( zString >= zEnd ){` |
|      - | 4752 | `			/* No more input to process */` |
|     21 | 4753 | `			break;` |
|      - | 4754 | `		}` |
|     39 | 4755 | `		nMax = (int)(zEnd-zString);` |
|     39 | 4756 | `		if( nMax < split_len ){` |
|      3 | 4757 | `			split_len = nMax;` |
|      1 | 4758 | `		}` |
|      - | 4759 | `		/* Copy the current chunk */` |
|     39 | 4760 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 4761 | `		/* Insert it */` |
|     39 | 4762 | `		ph7_array_add_elem(pArray,0,pValue); /* Will make it's own copy */` |
|      - | 4763 | `		/* reset the string cursor */` |
|     39 | 4764 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 4765 | `		/* Update position */` |
|     39 | 4766 | `		zString += split_len;` |
|      1 | 4767 | `	}` |
|      - | 4768 | `	/*` |
|      - | 4769 | `	 * Return the array.` |
|      - | 4770 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 4771 | `	 * upon we return from this function.` |
|      - | 4772 | `	 */` |
|     21 | 4773 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 4774 | `	return PH7_OK;` |
|     16 | 4775 |  |
|      - | 4776 | `/*` |
|      - | 4777 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 4778 | ` * Refer to [strspn()].` |
|      - | 4779 | ` */` |
|     28 | 4780 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 4781 |  |
|     29 | 4782 | `	const char *zIn = *pzIn;` |
|      - | 4783 | `	const char *zPtr;` |
|      - | 4784 | `	/* Ignore leading white spaces */` |
|     29 | 4785 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 4786 | `		zIn++;` |
|    ! 0 | 4787 | `	}` |
|     29 | 4788 | `	if( zIn >= zEnd ){` |
|      - | 4789 | `		/* End of input */` |
|    ! 0 | 4790 | `		return SXERR_EOF;` |
|      - | 4791 | `	}` |
|     29 | 4792 | `	zPtr = zIn;` |
|      - | 4793 | `	/* Extract the token */` |
|    201 | 4794 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 4795 | `		zIn++;` |
|      1 | 4796 | `	}` |
|     29 | 4797 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 4798 | `	/* Synchronize pointers */` |
|     29 | 4799 | `	*pzIn = zIn;` |
|      - | 4800 | `	/* Return to the caller */` |
|     29 | 4801 | `	return SXRET_OK;` |
|     15 | 4802 |  |
|      - | 4803 | `/*` |
|      - | 4804 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 4805 | ` * return the longest match.` |
|      - | 4806 | ` * Refer to [strspn()].` |
|      - | 4807 | ` */` |
|     18 | 4808 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 4809 |  |
|     19 | 4810 | `	const char *zEnd = &zString[nLen];` |
|     19 | 4811 | `	const char *zIn = zString;` |
|      - | 4812 | `	int i,c;` |
|     45 | 4813 | `	for(;;){` |
|     91 | 4814 | `		if( zString >= zEnd ){` |
|      7 | 4815 | `			break;` |
|      - | 4816 | `		}` |
|      - | 4817 | `		/* Extract current character */` |
|     85 | 4818 | `		c = zString[0];` |
|      - | 4819 | `		/* Perform the lookup */` |
|    383 | 4820 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 4821 | `			if( c == zMask[i] ){` |
|      - | 4822 | `				/* Character found */` |
|     73 | 4823 | `				break;` |
|      - | 4824 | `			}` |
|    150 | 4825 | `		}` |
|     85 | 4826 | `		if( i >= nMaskLen ){` |
|      - | 4827 | `			/* Character not in the current mask,break immediately */` |
|     13 | 4828 | `			break;` |
|      - | 4829 | `		}` |
|      - | 4830 | `		/* Advance cursor */` |
|     73 | 4831 | `		zString++;` |
|      1 | 4832 | `	}` |
|      - | 4833 | `	/* Longest match */` |
|     19 | 4834 | `	return (int)(zString-zIn);` |
|      1 | 4835 |  |
|      - | 4836 | `/*` |
|      - | 4837 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 4838 | ` * Refer to [strcspn()].` |
|      - | 4839 | ` */` |
|     10 | 4840 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 4841 |  |
|     11 | 4842 | `	const char *zEnd = &zString[nLen];` |
|     11 | 4843 | `	const char *zIn = zString;` |
|      - | 4844 | `	int i,c;` |
|     12 | 4845 | `	for(;;){` |
|     25 | 4846 | `		if( zString >= zEnd ){` |
|      3 | 4847 | `			break;` |
|      - | 4848 | `		}` |
|      - | 4849 | `		/* Extract current character */` |
|     23 | 4850 | `		c = zString[0];` |
|      - | 4851 | `		/* Perform the lookup */` |
|     51 | 4852 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 4853 | `			if( c == zMask[i] ){` |
|      9 | 4854 | `				break;` |
|      - | 4855 | `			}` |
|     15 | 4856 | `		}` |
|     23 | 4857 | `		if( i < nMaskLen ){` |
|      - | 4858 | `			/* Character in the current mask,break immediately */` |
|      9 | 4859 | `			break;` |
|      - | 4860 | `		}` |
|      - | 4861 | `		/* Advance cursor */` |
|     15 | 4862 | `		zString++;` |
|      1 | 4863 | `	}` |
|      - | 4864 | `	/* Longest match */` |
|     11 | 4865 | `	return (int)(zString-zIn);` |
|      1 | 4866 |  |
|      - | 4867 | `/*` |
|      - | 4868 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 4869 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 4870 | ` *  of characters contained within a given mask.` |
|      - | 4871 | ` * Parameters` |
|      - | 4872 | ` * $str` |
|      - | 4873 | ` *  The input string.` |
|      - | 4874 | ` * $mask` |
|      - | 4875 | ` *  The list of allowable characters.` |
|      - | 4876 | ` * $start` |
|      - | 4877 | ` *  The position in subject to start searching.` |
|      - | 4878 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 4879 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 4880 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 4881 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 4882 | ` *  start'th position from the end of subject.` |
|      - | 4883 | ` * $length` |
|      - | 4884 | ` *  The length of the segment from subject to examine.` |
|      - | 4885 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 4886 | ` *  characters after the starting position.` |
|      - | 4887 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 4888 | ` *  position up to length characters from the end of subject.` |
|      - | 4889 | ` * Return` |
|      - | 4890 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 4891 | ` * in mask.` |
|      - | 4892 | ` */` |
|     26 | 4893 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4894 |  |
|      - | 4895 | `	const char *zString,*zMask,*zEnd;` |
|      - | 4896 | `	int iMasklen,iLen;` |
|      - | 4897 | `	SyString sToken;` |
|     27 | 4898 | `	int iCount = 0;` |
|      - | 4899 | `	int rc;` |
|     27 | 4900 | `	if( nArg < 2 ){` |
|      - | 4901 | `		/* Missing agruments,return zero */` |
|      3 | 4902 | `		ph7_result_int(pCtx,0);` |
|      3 | 4903 | `		return PH7_OK;` |
|      - | 4904 | `	}` |
|      - | 4905 | `	/* Extract the target string */` |
|     25 | 4906 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 4907 | `	/* Extract the mask */` |
|     25 | 4908 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 4909 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 4910 | `		/* Nothing to process,return zero */` |
|      7 | 4911 | `		ph7_result_int(pCtx,0);` |
|      7 | 4912 | `		return PH7_OK;` |
|      - | 4913 | `	}` |
|     19 | 4914 | `	if( nArg > 2 ){` |
|      - | 4915 | `		int nOfft;` |
|      - | 4916 | `		/* Extract the offset */` |
|      9 | 4917 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 4918 | `		if( nOfft < 0 ){` |
|    ! 0 | 4919 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 4920 | `			if( zBase > zString ){` |
|    ! 0 | 4921 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 4922 | `				zString = zBase;` |
|    ! 0 | 4923 | `			}else{` |
|      - | 4924 | `				/* Invalid offset */` |
|    ! 0 | 4925 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 4926 | `				return PH7_OK;` |
|      - | 4927 | `			}` |
|    ! 0 | 4928 | `		}else{` |
|      9 | 4929 | `			if( nOfft >= iLen ){` |
|      - | 4930 | `				/* Invalid offset */` |
|    ! 0 | 4931 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 4932 | `				return PH7_OK;` |
|    ! 0 | 4933 | `			}else{` |
|      - | 4934 | `				/* Update offset */` |
|      9 | 4935 | `				zString += nOfft;` |
|      9 | 4936 | `				iLen -= nOfft;` |
|      - | 4937 | `			}` |
|      - | 4938 | `		}` |
|      9 | 4939 | `		if( nArg > 3 ){` |
|      - | 4940 | `			int iUserlen;` |
|      - | 4941 | `			/* Extract the desired length */` |
|      9 | 4942 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 4943 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 4944 | `				iLen = iUserlen;` |
|      2 | 4945 | `			}` |
|      4 | 4946 | `		}` |
|      4 | 4947 | `	}` |
|      - | 4948 | `	/* Point to the end of the string */` |
|     19 | 4949 | `	zEnd = &zString[iLen];` |
|      - | 4950 | `	/* Extract the first non-space token */` |
|     19 | 4951 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 4952 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 4953 | `		/* Compare against the current mask */` |
|     19 | 4954 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 4955 | `	}` |
|      - | 4956 | `	/* Longest match */` |
|     19 | 4957 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 4958 | `	return PH7_OK;` |
|     14 | 4959 |  |
|      - | 4960 | `/*` |
|      - | 4961 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 4962 | ` *  Find length of initial segment not matching mask.` |
|      - | 4963 | ` * Parameters` |
|      - | 4964 | ` * $str` |
|      - | 4965 | ` *  The input string.` |
|      - | 4966 | ` * $mask` |
|      - | 4967 | ` *  The list of not allowed characters.` |
|      - | 4968 | ` * $start` |
|      - | 4969 | ` *  The position in subject to start searching.` |
|      - | 4970 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 4971 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 4972 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 4973 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 4974 | ` *  start'th position from the end of subject.` |
|      - | 4975 | ` * $length` |
|      - | 4976 | ` *  The length of the segment from subject to examine.` |
|      - | 4977 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 4978 | ` *  characters after the starting position.` |
|      - | 4979 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 4980 | ` *  position up to length characters from the end of subject.` |
|      - | 4981 | ` * Return` |
|      - | 4982 | ` *  Returns the length of the segment as an integer.` |
|      - | 4983 | ` */` |
|     16 | 4984 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4985 |  |
|      - | 4986 | `	const char *zString,*zMask,*zEnd;` |
|      - | 4987 | `	int iMasklen,iLen;` |
|      - | 4988 | `	SyString sToken;` |
|     17 | 4989 | `	int iCount = 0;` |
|      - | 4990 | `	int rc;` |
|     17 | 4991 | `	if( nArg < 2 ){` |
|      - | 4992 | `		/* Missing agruments,return zero */` |
|      3 | 4993 | `		ph7_result_int(pCtx,0);` |
|      3 | 4994 | `		return PH7_OK;` |
|      - | 4995 | `	}` |
|      - | 4996 | `	/* Extract the target string */` |
|     15 | 4997 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 4998 | `	/* Extract the mask */` |
|     15 | 4999 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 5000 | `	if( iLen < 1 ){` |
|      - | 5001 | `		/* Nothing to process,return zero */` |
|    ! 0 | 5002 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5003 | `		return PH7_OK;` |
|      - | 5004 | `	}` |
|     15 | 5005 | `	if( iMasklen < 1 ){` |
|      - | 5006 | `		/* No given mask,return the string length */` |
|      3 | 5007 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 5008 | `		return PH7_OK;` |
|      - | 5009 | `	}` |
|     13 | 5010 | `	if( nArg > 2 ){` |
|      - | 5011 | `		int nOfft;` |
|      - | 5012 | `		/* Extract the offset */` |
|     11 | 5013 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 5014 | `		if( nOfft < 0 ){` |
|    ! 0 | 5015 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5016 | `			if( zBase > zString ){` |
|    ! 0 | 5017 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5018 | `				zString = zBase;` |
|    ! 0 | 5019 | `			}else{` |
|      - | 5020 | `				/* Invalid offset */` |
|    ! 0 | 5021 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5022 | `				return PH7_OK;` |
|      - | 5023 | `			}` |
|    ! 0 | 5024 | `		}else{` |
|     11 | 5025 | `			if( nOfft >= iLen ){` |
|      - | 5026 | `				/* Invalid offset */` |
|      3 | 5027 | `				ph7_result_int(pCtx,0);` |
|      3 | 5028 | `				return PH7_OK;` |
|    ! 0 | 5029 | `			}else{` |
|      - | 5030 | `				/* Update offset */` |
|      9 | 5031 | `				zString += nOfft;` |
|      9 | 5032 | `				iLen -= nOfft;` |
|      - | 5033 | `			}` |
|      - | 5034 | `		}` |
|      9 | 5035 | `		if( nArg > 3 ){` |
|      - | 5036 | `			int iUserlen;` |
|      - | 5037 | `			/* Extract the desired length */` |
|    ! 0 | 5038 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 5039 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 5040 | `				iLen = iUserlen;` |
|    ! 0 | 5041 | `			}` |
|    ! 0 | 5042 | `		}` |
|      4 | 5043 | `	}` |
|      - | 5044 | `	/* Point to the end of the string */` |
|     11 | 5045 | `	zEnd = &zString[iLen];` |
|      - | 5046 | `	/* Extract the first non-space token */` |
|     11 | 5047 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 5048 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5049 | `		/* Compare against the current mask */` |
|     11 | 5050 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 5051 | `	}` |
|      - | 5052 | `	/* Longest match */` |
|     11 | 5053 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 5054 | `	return PH7_OK;` |
|      9 | 5055 |  |
|      - | 5056 | `/*` |
|      - | 5057 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 5058 | ` *  Search a string for any of a set of characters.` |
|      - | 5059 | ` * Parameters` |
|      - | 5060 | ` *  $haystack` |
|      - | 5061 | ` *   The string where char_list is looked for.` |
|      - | 5062 | ` *  $char_list` |
|      - | 5063 | ` *   This parameter is case sensitive.` |
|      - | 5064 | ` * Return` |
|      - | 5065 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 5066 | ` */` |
|      6 | 5067 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5068 |  |
|      - | 5069 | `	const char *zString,*zList,*zEnd;` |
|      - | 5070 | `	int iLen,iListLen,i,c;` |
|      - | 5071 | `	sxu32 nOfft,nMax;` |
|      - | 5072 | `	sxi32 rc;` |
|      7 | 5073 | `	if( nArg < 2 ){` |
|      - | 5074 | `		/* Missing arguments,return FALSE */` |
|      3 | 5075 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5076 | `		return PH7_OK;` |
|      - | 5077 | `	}` |
|      - | 5078 | `	/* Extract the haystack and the char list */` |
|      5 | 5079 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 5080 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 5081 | `	if( iLen < 1 ){` |
|      - | 5082 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 5083 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5084 | `		return PH7_OK;` |
|      - | 5085 | `	}` |
|      - | 5086 | `	/* Point to the end of the string */` |
|      5 | 5087 | `	zEnd = &zString[iLen];` |
|      5 | 5088 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 5089 | `	/* perform the requested operation */` |
|     15 | 5090 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 5091 | `		c = zList[i];` |
|     11 | 5092 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 5093 | `		if( rc == SXRET_OK ){` |
|      5 | 5094 | `			if( nMax < nOfft ){` |
|      3 | 5095 | `				nOfft = nMax;` |
|      1 | 5096 | `			}` |
|      2 | 5097 | `		}` |
|      6 | 5098 | `	}` |
|      5 | 5099 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 5100 | `		/* No such substring,return FALSE */` |
|      3 | 5101 | `		ph7_result_bool(pCtx,0);` |
|      2 | 5102 | `	}else{` |
|      - | 5103 | `		/* Return the substring */` |
|      3 | 5104 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 5105 | `	}` |
|      5 | 5106 | `	return PH7_OK;` |
|      4 | 5107 |  |
|      - | 5108 | `/*` |
|      - | 5109 | ` * string soundex(string $str)` |
|      - | 5110 | ` *  Calculate the soundex key of a string.` |
|      - | 5111 | ` * Parameters` |
|      - | 5112 | ` *  $str` |
|      - | 5113 | ` *   The input string.` |
|      - | 5114 | ` * Return` |
|      - | 5115 | ` *  Returns the soundex key as a string.` |
|      - | 5116 | ` * Note:` |
|      - | 5117 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 5118 | ` * source tree.` |
|      - | 5119 | ` */` |
|     20 | 5120 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5121 |  |
|      - | 5122 | `	const unsigned char *zIn;` |
|      - | 5123 | `	char zResult[8];` |
|      - | 5124 | `	int i, j;` |
|      - | 5125 | `	static const unsigned char iCode[] = {` |
|      - | 5126 |  |
|      - | 5127 |  |
|      - | 5128 |  |
|      - | 5129 |  |
|      - | 5130 |  |
|      - | 5131 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5132 |  |
|      - | 5133 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5134 | `	};` |
|     21 | 5135 | `	if( nArg < 1 ){` |
|      - | 5136 | `		/* Missing arguments,return the empty string */` |
|      3 | 5137 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5138 | `		return PH7_OK;` |
|      - | 5139 | `	}` |
|     19 | 5140 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     19 | 5141 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     19 | 5142 | `	if( zIn[i] ){` |
|     17 | 5143 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 5144 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 5145 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 5146 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 5147 | `			if( code>0 ){` |
|     45 | 5148 | `				if( code!=prevcode ){` |
|     33 | 5149 | `					prevcode = (unsigned char)code;` |
|     33 | 5150 | `					zResult[j++] = (char)code + '0';` |
|     16 | 5151 | `				}` |
|     23 | 5152 | `			}else{` |
|     49 | 5153 | `				prevcode = 0;` |
|      - | 5154 | `			}` |
|     47 | 5155 | `		}` |
|     33 | 5156 | `		while( j<4 ){` |
|     17 | 5157 | `			zResult[j++] = '0';` |
|      1 | 5158 | `		}` |
|     17 | 5159 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 5160 | `	}else{` |
|      3 | 5161 | `	  ph7_result_string(pCtx,"?000",4);` |
|      - | 5162 | `	}` |
|     19 | 5163 | `	return PH7_OK;` |
|     11 | 5164 |  |
|      - | 5165 | `/*` |
|      - | 5166 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 5167 | ` *  Wraps a string to a given number of characters.` |
|      - | 5168 | ` * Parameters` |
|      - | 5169 | ` *  $str` |
|      - | 5170 | ` *   The input string.` |
|      - | 5171 | ` * $width` |
|      - | 5172 | ` *  The column width.` |
|      - | 5173 | ` * $break` |
|      - | 5174 | ` *  The line is broken using the optional break parameter.` |
|      - | 5175 | ` * Return` |
|      - | 5176 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 5177 | ` */` |
|     14 | 5178 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5179 |  |
|      - | 5180 | `	const char *zIn,*zEnd,*zBreak;` |
|      - | 5181 | `	int iLen,iBreaklen,iChunk;` |
|     15 | 5182 | `	if( nArg < 1 ){` |
|      - | 5183 | `		/* Missing arguments,return the empty string */` |
|      3 | 5184 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5185 | `		return PH7_OK;` |
|      - | 5186 | `	}` |
|      - | 5187 | `	/* Extract the input string */` |
|     13 | 5188 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|     13 | 5189 | `	if( iLen < 1 ){` |
|      - | 5190 | `		/* Nothing to process,return the empty string */` |
|      3 | 5191 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5192 | `		return PH7_OK;` |
|      - | 5193 | `	}` |
|      - | 5194 | `	/* Chunk length */` |
|     11 | 5195 | `	iChunk = 75;` |
|     11 | 5196 | `	iBreaklen = 0;` |
|     11 | 5197 | `	zBreak = ""; /* cc warning */` |
|     11 | 5198 | `	if( nArg > 1 ){` |
|     11 | 5199 | `		iChunk = ph7_value_to_int(apArg[1]);` |
|     11 | 5200 | `		if( iChunk < 1 ){` |
|    ! 0 | 5201 | `			iChunk = 75;` |
|    ! 0 | 5202 | `		}` |
|     11 | 5203 | `		if( nArg > 2 ){` |
|      3 | 5204 | `			zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      1 | 5205 | `		}` |
|      5 | 5206 | `	}` |
|     11 | 5207 | `	if( iBreaklen < 1 ){` |
|      - | 5208 | `		/* Set a default column break */` |
|      - | 5209 | `#ifdef __WINNT__` |
|      1 | 5210 | `		zBreak = "\r\n";` |
|      1 | 5211 | `		iBreaklen = (int)sizeof("\r\n")-1;` |
|      - | 5212 | `#else` |
|      8 | 5213 | `		zBreak = "\n";` |
|      8 | 5214 | `		iBreaklen = (int)sizeof(char);` |
|      - | 5215 | `#endif` |
|      4 | 5216 | `	}` |
|      - | 5217 | `	/* Perform the requested operation */` |
|     11 | 5218 | `	zEnd = &zIn[iLen];` |
|     41 | 5219 | `	for(;;){` |
|      - | 5220 | `		int nMax;` |
|     47 | 5221 | `		if( zIn >= zEnd ){` |
|      - | 5222 | `			/* No more input to process */` |
|     11 | 5223 | `			break;` |
|      - | 5224 | `		}` |
|     37 | 5225 | `		nMax = (int)(zEnd-zIn);` |
|     37 | 5226 | `		if( iChunk > nMax ){` |
|     11 | 5227 | `			iChunk = nMax;` |
|      5 | 5228 | `		}` |
|      - | 5229 | `		/* Append the column first */` |
|     37 | 5230 | `		ph7_result_string(pCtx,zIn,iChunk); /* Will make it's own copy */` |
|      - | 5231 | `		/* Advance the cursor */` |
|     37 | 5232 | `		zIn += iChunk;` |
|     37 | 5233 | `		if( zIn < zEnd ){` |
|      - | 5234 | `			/* Append the line break */` |
|     27 | 5235 | `			ph7_result_string(pCtx,zBreak,iBreaklen);` |
|     13 | 5236 | `		}` |
|      1 | 5237 | `	}` |
|     11 | 5238 | `	return PH7_OK;` |
|      8 | 5239 |  |
|      - | 5240 | `/*` |
|      - | 5241 | ` * Check if the given character is a member of the given mask.` |
|      - | 5242 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 5243 | ` * Refer to [strtok()].` |
|      - | 5244 | ` */` |
|     30 | 5245 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 5246 |  |
|      - | 5247 | `	int i;` |
|     57 | 5248 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 5249 | `		if( c == zMask[i] ){` |
|     13 | 5250 | `			if( pOfft ){` |
|      5 | 5251 | `				*pOfft = i;` |
|      2 | 5252 | `			}` |
|     13 | 5253 | `			return TRUE;` |
|      - | 5254 | `		}` |
|     14 | 5255 | `	}` |
|     19 | 5256 | `	return FALSE;` |
|     16 | 5257 |  |
|      - | 5258 | `/*` |
|      - | 5259 | ` * Extract a single token from the input stream.` |
|      - | 5260 | ` * Refer to [strtok()].` |
|      - | 5261 | ` */` |
|      6 | 5262 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 5263 |  |
|      7 | 5264 | `	const char *zIn = *pzIn;` |
|      - | 5265 | `	const char *zPtr;` |
|      - | 5266 | `	/* Ignore leading delimiter */` |
|     11 | 5267 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5268 | `		zIn++;` |
|      1 | 5269 | `	}` |
|      7 | 5270 | `	if( zIn >= zEnd ){` |
|      - | 5271 | `		/* End of input */` |
|    ! 0 | 5272 | `		return SXERR_EOF;` |
|      - | 5273 | `	}` |
|      7 | 5274 | `	zPtr = zIn;` |
|      - | 5275 | `	/* Extract the token */` |
|     13 | 5276 | `	while( zIn < zEnd ){` |
|     11 | 5277 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 5278 | `			/* UTF-8 stream */` |
|    ! 0 | 5279 | `			zIn++;` |
|    ! 0 | 5280 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 5281 | `		}else{` |
|     11 | 5282 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5283 | `				break;` |
|      - | 5284 | `			}` |
|      7 | 5285 | `			zIn++;` |
|      - | 5286 | `		}` |
|      1 | 5287 | `	}` |
|      7 | 5288 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5289 | `	/* Update the cursor */` |
|      7 | 5290 | `	*pzIn = zIn;` |
|      - | 5291 | `	/* Return to the caller */` |
|      7 | 5292 | `	return SXRET_OK;` |
|      4 | 5293 |  |
|      - | 5294 | `/* strtok auxiliary private data */` |
|      - | 5295 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 5296 | `struct strtok_aux_data` |
|      - | 5297 |  |
|      - | 5298 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 5299 | `	const char *zIn;   /* Current input stream */` |
|      - | 5300 | `	const char *zEnd;  /* End of input */` |
|      - | 5301 | `};` |
|      - | 5302 | `/*` |
|      - | 5303 | ` * string strtok(string $str,string $token)` |
|      - | 5304 | ` * string strtok(string $token)` |
|      - | 5305 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 5306 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 5307 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 5308 | ` *  words by using the space character as the token.` |
|      - | 5309 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 5310 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 5311 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 5312 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 5313 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 5314 | ` *  the argument are found.` |
|      - | 5315 | ` * Parameters` |
|      - | 5316 | ` *  $str` |
|      - | 5317 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 5318 | ` * $token` |
|      - | 5319 | ` *  The delimiter used when splitting up str.` |
|      - | 5320 | ` * Return` |
|      - | 5321 | ` *   Current token or FALSE on EOF.` |
|      - | 5322 | ` */` |
|      8 | 5323 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5324 |  |
|      - | 5325 | `	strtok_aux_data *pAux;` |
|      - | 5326 | `	const char *zMask;` |
|      - | 5327 | `	SyString sToken;` |
|      - | 5328 | `	int nMasklen;` |
|      - | 5329 | `	sxi32 rc;` |
|      9 | 5330 | `	if( nArg < 2 ){` |
|      - | 5331 | `		/* Extract top aux data */` |
|      7 | 5332 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      7 | 5333 | `		if( pAux == 0 ){` |
|      - | 5334 | `			/* No aux data,return FALSE */` |
|    ! 0 | 5335 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5336 | `			return PH7_OK;` |
|      - | 5337 | `		}` |
|      7 | 5338 | `		nMasklen = 0;` |
|      7 | 5339 | `		zMask = ""; /* cc warning */` |
|      7 | 5340 | `		if( nArg > 0 ){` |
|      - | 5341 | `			/* Extract the mask */` |
|      5 | 5342 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 5343 | `		}` |
|      7 | 5344 | `		if( nMasklen < 1 ){` |
|      - | 5345 | `			/* Invalid mask,return FALSE */` |
|      3 | 5346 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|      3 | 5347 | `			ph7_context_free_chunk(pCtx,pAux);` |
|      3 | 5348 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|      3 | 5349 | `			ph7_result_bool(pCtx,0);` |
|      3 | 5350 | `			return PH7_OK;` |
|      - | 5351 | `		}` |
|      - | 5352 | `		/* Extract the token */` |
|      5 | 5353 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 5354 | `		if( rc != SXRET_OK ){` |
|      - | 5355 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 5356 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 5357 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5358 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 5359 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5360 | `		}else{` |
|      - | 5361 | `			/* Return the extracted token */` |
|      5 | 5362 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 5363 | `		}` |
|      3 | 5364 | `	}else{` |
|      - | 5365 | `		const char *zInput,*zCur;` |
|      - | 5366 | `		char *zDup;` |
|      - | 5367 | `		int nLen;` |
|      - | 5368 | `		/* Extract the raw input */` |
|      3 | 5369 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5370 | `		if( nLen < 1 ){` |
|      - | 5371 | `			/* Empty input,return FALSE */` |
|    ! 0 | 5372 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5373 | `			return PH7_OK;` |
|      - | 5374 | `		}` |
|      - | 5375 | `		/* Extract the mask */` |
|      3 | 5376 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 5377 | `		if( nMasklen < 1 ){` |
|      - | 5378 | `			/* Set a default mask */` |
|      - | 5379 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 5380 | `			zMask = TOK_MASK;` |
|    ! 0 | 5381 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 5382 | `#undef TOK_MASK` |
|    ! 0 | 5383 | `		}` |
|      - | 5384 | `		/* Extract a single token */` |
|      3 | 5385 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 5386 | `		if( rc != SXRET_OK ){` |
|      - | 5387 | `			/* Empty input */` |
|    ! 0 | 5388 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5389 | `			return PH7_OK;` |
|    ! 0 | 5390 | `		}else{` |
|      - | 5391 | `			/* Return the extracted token */` |
|      3 | 5392 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 5393 | `		}` |
|      - | 5394 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 5395 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 5396 | `		if( pAux ){` |
|      3 | 5397 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 5398 | `			if( nLen < 1 ){` |
|    ! 0 | 5399 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5400 | `				return PH7_OK;` |
|      - | 5401 | `			}` |
|      - | 5402 | `			/* Duplicate input */` |
|      3 | 5403 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 5404 | `			if( zDup  ){` |
|      3 | 5405 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 5406 | `				/* Register the aux data */` |
|      3 | 5407 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 5408 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 5409 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 5410 | `			}` |
|      1 | 5411 | `		}` |
|      - | 5412 | `	}` |
|      7 | 5413 | `	return PH7_OK;` |
|      5 | 5414 |  |
|      - | 5415 | `/*` |
|      - | 5416 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 5417 | ` *  Pad a string to a certain length with another string` |
|      - | 5418 | ` * Parameters` |
|      - | 5419 | ` *  $input` |
|      - | 5420 | ` *   The input string.` |
|      - | 5421 | ` * $pad_length` |
|      - | 5422 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 5423 | ` *   string, no padding takes place.` |
|      - | 5424 | ` * $pad_string` |
|      - | 5425 | ` *   Note:` |
|      - | 5426 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 5427 | ` *    divided by the pad_string's length.` |
|      - | 5428 | ` * $pad_type` |
|      - | 5429 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 5430 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 5431 | ` * Return` |
|      - | 5432 | ` *  The padded string.` |
|      - | 5433 | ` */` |
|     10 | 5434 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5435 |  |
|      - | 5436 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 5437 | `	const char *zIn,*zPad;` |
|     11 | 5438 | `	if( nArg < 2 ){` |
|      - | 5439 | `		/* Missing arguments,return the empty string */` |
|      5 | 5440 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 5441 | `		return PH7_OK;` |
|      - | 5442 | `	}` |
|      - | 5443 | `	/* Extract the target string */` |
|      7 | 5444 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5445 | `	/* Padding length */` |
|      7 | 5446 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|      7 | 5447 | `	if( iPadlen > 0 ){` |
|      5 | 5448 | `		iPadlen -= iLen;` |
|      2 | 5449 | `	}` |
|      7 | 5450 | `	if( iPadlen < 1  ){` |
|      - | 5451 | `		/* Return the string verbatim */` |
|      3 | 5452 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      3 | 5453 | `		return PH7_OK;` |
|      - | 5454 | `	}` |
|      5 | 5455 | `	zPad = " "; /* Whitespace padding */` |
|      5 | 5456 | `	iStrpad = (int)sizeof(char);` |
|      5 | 5457 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      5 | 5458 | `	if( nArg > 2 ){` |
|      - | 5459 | `		/* Padding string */` |
|      5 | 5460 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      5 | 5461 | `		if( iStrpad < 1 ){` |
|      - | 5462 | `			/* Empty string */` |
|    ! 0 | 5463 | `			zPad = " "; /* Whitespace padding */` |
|    ! 0 | 5464 | `			iStrpad = (int)sizeof(char);` |
|    ! 0 | 5465 | `		}` |
|      5 | 5466 | `		if( nArg > 3 ){` |
|      - | 5467 | `			/* Padd type */` |
|      5 | 5468 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 5469 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 5470 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 5471 | `			}` |
|      2 | 5472 | `		}` |
|      2 | 5473 | `	}` |
|      5 | 5474 | `	iDiv = 1;` |
|      5 | 5475 | `	if( iType == 2 ){` |
|    ! 0 | 5476 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 5477 | `	}` |
|      - | 5478 | `	/* Perform the requested operation */` |
|      5 | 5479 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 5480 | `		jPad = iStrpad;` |
|      5 | 5481 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 5482 | `			/* Padding */` |
|      5 | 5483 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 5484 | `				break;` |
|      - | 5485 | `			}` |
|      3 | 5486 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      2 | 5487 | `		}` |
|      3 | 5488 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 5489 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 5490 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 5491 | `				if( jPad > iStrpad ){` |
|    ! 0 | 5492 | `					jPad = iStrpad;` |
|    ! 0 | 5493 | `				}` |
|      3 | 5494 | `				if( jPad < 1){` |
|    ! 0 | 5495 | `					break;` |
|      - | 5496 | `				}` |
|      3 | 5497 | `				ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 5498 | `			}` |
|      1 | 5499 | `		}` |
|      1 | 5500 | `	}` |
|      5 | 5501 | `	if( iLen > 0 ){` |
|      - | 5502 | `		/* Append the input string */` |
|      5 | 5503 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      2 | 5504 | `	}` |
|      5 | 5505 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 5506 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 5507 | `			/* Padding */` |
|      5 | 5508 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 5509 | `				break;` |
|      - | 5510 | `			}` |
|      3 | 5511 | `			ph7_result_string(pCtx,zPad,iStrpad);` |
|      2 | 5512 | `		}` |
|      5 | 5513 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 5514 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 5515 | `			if( jPad > iStrpad ){` |
|    ! 0 | 5516 | `				jPad = iStrpad;` |
|    ! 0 | 5517 | `			}` |
|      3 | 5518 | `			if( jPad < 1){` |
|    ! 0 | 5519 | `				break;` |
|      - | 5520 | `			}` |
|      3 | 5521 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 5522 | `		}` |
|      1 | 5523 | `	}` |
|      5 | 5524 | `	return PH7_OK;` |
|      6 | 5525 |  |
|      - | 5526 | `/*` |
|      - | 5527 | ` * String replacement private data.` |
|      - | 5528 | ` */` |
|      - | 5529 | `typedef struct str_replace_data str_replace_data;` |
|      - | 5530 | `struct str_replace_data` |
|      - | 5531 |  |
|      - | 5532 | `	/* The following two fields are only used by the strtr function */` |
|      - | 5533 | `	SyBlob *pWorker;         /* Working buffer */` |
|      - | 5534 | `	ProcStringMatch xMatch;  /* Pattern match routine */` |
|      - | 5535 | `	/* The following two fields are only used by the str_replace function */` |
|      - | 5536 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 5537 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 5538 | `};` |
|      - | 5539 | `/*` |
|      - | 5540 | ` * Remove a substring.` |
|      - | 5541 | ` */` |
|      - | 5542 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 5543 | `	for(;;){\` |
|      - | 5544 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 5545 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 5546 | `		++OFFT;\` |
|      - | 5547 | `	}\` |
|      - | 5548 |  |
|      - | 5549 | `/*` |
|      - | 5550 | ` * Shift right and insert algorithm.` |
|      - | 5551 | ` */` |
|      - | 5552 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 5553 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 5554 | `		for(;;){\` |
|      - | 5555 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 5556 | `			if(INLEN < 1 ) { break; }\` |
|      - | 5557 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 5558 | `			--INLEN; \` |
|      - | 5559 | `		}\` |
|      - | 5560 | `		for(;;){\` |
|      - | 5561 | `				if(ELEN < 1) { break; }\` |
|      - | 5562 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 5563 | `				OFFT++;\` |
|      - | 5564 | `				ENTRY++;\` |
|      - | 5565 | `				--ELEN;\` |
|      - | 5566 | `		}\` |
|      - | 5567 |  |
|      - | 5568 | `/*` |
|      - | 5569 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 5570 | ` * replacement string [i.e: zReplace].` |
|      - | 5571 | ` */` |
|     38 | 5572 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 5573 |  |
|     39 | 5574 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 5575 | `	sxu32 n,m;` |
|     39 | 5576 | `	n = SyBlobLength(pWorker);` |
|     39 | 5577 | `	m = nOfft;` |
|      - | 5578 | `	/* Delete the old entry */` |
|    475 | 5579 | `	STRDEL(zInput,n,m,nLen);` |
|     39 | 5580 | `	SyBlobLength(pWorker) -= nLen;` |
|     39 | 5581 | `	if( nReplen > 0 ){` |
|     33 | 5582 | `		sxi32 iRep = nReplen;` |
|      - | 5583 | `		sxi32 rc;` |
|      - | 5584 | `		/*` |
|      - | 5585 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 5586 | `		 * string.` |
|      - | 5587 | `		 */` |
|     33 | 5588 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     33 | 5589 | `		if( rc != SXRET_OK ){` |
|      - | 5590 | `			/* Simply ignore any memory failure problem */` |
|    ! 0 | 5591 | `			return SXRET_OK;` |
|      - | 5592 | `		}` |
|      - | 5593 | `		/* Perform the insertion now */` |
|     33 | 5594 | `		zInput = (char *)SyBlobData(pWorker);` |
|     33 | 5595 | `		n = SyBlobLength(pWorker);` |
|    163 | 5596 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     33 | 5597 | `		SyBlobLength(pWorker) += nReplen;` |
|     16 | 5598 | `	}` |
|     39 | 5599 | `	return SXRET_OK;` |
|     20 | 5600 |  |
|      - | 5601 | `/*` |
|      - | 5602 | ` * String replacement walker callback.` |
|      - | 5603 | ` * The following callback is invoked for each array entry that hold` |
|      - | 5604 | ` * the replace string.` |
|      - | 5605 | ` * Refer to the strtr() implementation for more information.` |
|      - | 5606 | ` */` |
|      8 | 5607 | `static int StringReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 5608 |  |
|      9 | 5609 | `	str_replace_data *pRepData = (str_replace_data *)pUserData;` |
|      - | 5610 | `	const char *zTarget,*zReplace;` |
|      - | 5611 | `	SyBlob *pWorker;` |
|      - | 5612 | `	int tLen,nLen;` |
|      - | 5613 | `	sxu32 nOfft;` |
|      - | 5614 | `	sxi32 rc;` |
|      - | 5615 | `	/* Point to the working buffer */` |
|      9 | 5616 | `	pWorker = pRepData->pWorker;` |
|      9 | 5617 | `	if( !ph7_value_is_string(pKey) ){` |
|      - | 5618 | `		/* Target and replace must be a string */` |
|      3 | 5619 | `		return PH7_OK;` |
|      - | 5620 | `	}` |
|      - | 5621 | `	/* Extract the target and the replace */` |
|      7 | 5622 | `	zTarget = ph7_value_to_string(pKey,&tLen);` |
|      7 | 5623 | `	if( tLen < 1 ){` |
|      - | 5624 | `		/* Empty target,return immediately */` |
|    ! 0 | 5625 | `		return PH7_OK;` |
|      - | 5626 | `	}` |
|      - | 5627 | `	/* Perform a pattern search */` |
|      7 | 5628 | `	rc = pRepData->xMatch(SyBlobData(pWorker),SyBlobLength(pWorker),(const void *)zTarget,(sxu32)tLen,&nOfft);` |
|      7 | 5629 | `	if( rc != SXRET_OK ){` |
|      - | 5630 | `		/* Pattern not found */` |
|    ! 0 | 5631 | `		return PH7_OK;` |
|      - | 5632 | `	}` |
|      - | 5633 | `	/* Extract the replace string */` |
|      7 | 5634 | `	zReplace = ph7_value_to_string(pData,&nLen);` |
|      - | 5635 | `	/* Perform the replace process */` |
|      7 | 5636 | `	StringReplace(pWorker,nOfft,tLen,zReplace,nLen);` |
|      - | 5637 | `	/* All done */` |
|      7 | 5638 | `	return PH7_OK;` |
|      5 | 5639 |  |
|      - | 5640 | `/*` |
|      - | 5641 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 5642 | ` * to collect search/replace string.` |
|      - | 5643 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 5644 | ` */` |
|     26 | 5645 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 5646 |  |
|     27 | 5647 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 5648 | `	SyString sWorker;` |
|      - | 5649 | `	const char *zIn;` |
|      - | 5650 | `	int nByte;` |
|      - | 5651 | `	/* Extract a string representation of the given argument */` |
|     27 | 5652 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 5653 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 5654 | `	if( nByte > 0 ){` |
|      - | 5655 | `		char *zDup;` |
|      - | 5656 | `		/* Duplicate the chunk */` |
|     25 | 5657 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 5658 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 5659 | `			);` |
|     25 | 5660 | `		if( zDup == 0 ){` |
|      - | 5661 | `			/* Ignore any memory failure problem */` |
|    ! 0 | 5662 | `			ph7_context_throw_error(pRep->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 5663 | `			return PH7_OK;` |
|      - | 5664 | `		}` |
|     25 | 5665 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 5666 | `		/* Save the chunk */` |
|     25 | 5667 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 5668 | `	}` |
|      - | 5669 | `	/* Save for later processing */` |
|     27 | 5670 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 5671 | `	/* All done */` |
|     13 | 5672 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 5673 | `	return PH7_OK;` |
|     14 | 5674 |  |
|      - | 5675 | `/*` |
|      - | 5676 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 5677 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 5678 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 5679 | ` * Parameters` |
|      - | 5680 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 5681 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 5682 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 5683 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 5684 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 5685 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 5686 | ` * $search` |
|      - | 5687 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 5688 | ` *  to designate multiple needles.` |
|      - | 5689 | ` * $replace` |
|      - | 5690 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 5691 | ` *  to designate multiple replacements.` |
|      - | 5692 | ` * $subject` |
|      - | 5693 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 5694 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 5695 | ` *  of subject, and the return value is an array as well.` |
|      - | 5696 | ` * $count (Not used)` |
|      - | 5697 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 5698 | ` * Return` |
|      - | 5699 | ` * This function returns a string or an array with the replaced values.` |
|      - | 5700 | ` */` |
|  22002 | 5701 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5702 |  |
|      - | 5703 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 5704 | `	ProcStringMatch xMatch;` |
|      - | 5705 | `	const char *zIn,*zFunc;` |
|      - | 5706 | `	str_replace_data sRep;` |
|      - | 5707 | `	SyBlob sWorker;` |
|      - | 5708 | `	SySet sReplace;` |
|      - | 5709 | `	SySet sSearch;` |
|      - | 5710 | `	int rep_str;` |
|      - | 5711 | `	int nByte;` |
|      - | 5712 | `	sxi32 rc;` |
|  22004 | 5713 | `	if( nArg < 3 ){` |
|      - | 5714 | `		/* Missing/Invalid arguments,return null */` |
|      7 | 5715 | `		ph7_result_null(pCtx);` |
|      7 | 5716 | `		return PH7_OK;` |
|      - | 5717 | `	}` |
|      - | 5718 | `	/* Initialize fields */` |
|  21998 | 5719 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  21998 | 5720 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  21998 | 5721 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  21998 | 5722 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  21998 | 5723 | `	sRep.pCtx = pCtx;` |
|  21998 | 5724 | `	sRep.pCollector = &sSearch;` |
|  21998 | 5725 | `	rep_str = 0;` |
|      - | 5726 | `	/* Extract the subject */` |
|  21998 | 5727 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  21998 | 5728 | `	if( nByte < 1 ){` |
|      - | 5729 | `		/* Nothing to replace,return the empty string */` |
|     29 | 5730 | `		ph7_result_string(pCtx,"",0);` |
|     29 | 5731 | `		return PH7_OK;` |
|      - | 5732 | `	}` |
|      - | 5733 | `	/* Copy the subject */` |
|  21970 | 5734 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 5735 | `	/* Search string */` |
|  21970 | 5736 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 5737 | `		/* Collect search string */` |
|      9 | 5738 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 5739 | `	}else{` |
|      - | 5740 | `		/* Single pattern */` |
|  21962 | 5741 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  21962 | 5742 | `		if( nByte < 1 ){` |
|      - | 5743 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 5744 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 5745 | `			return PH7_OK;` |
|      - | 5746 | `		}` |
|  21958 | 5747 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 5748 | `		/* Save for later processing */` |
|  21958 | 5749 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 5750 | `	}` |
|      - | 5751 | `	/* Replace string */` |
|  21966 | 5752 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 5753 | `		/* Collect replace string */` |
|      7 | 5754 | `		sRep.pCollector = &sReplace;` |
|      7 | 5755 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 5756 | `	}else{` |
|      - | 5757 | `		/* Single needle */` |
|  21960 | 5758 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  21960 | 5759 | `		rep_str = 1;` |
|  21960 | 5760 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 5761 | `		/* Save for later processing */` |
|  21960 | 5762 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 5763 | `	}` |
|      - | 5764 | `	/* Reset loop cursors */` |
|  21966 | 5765 | `	SySetResetCursor(&sSearch);` |
|  21966 | 5766 | `	SySetResetCursor(&sReplace);` |
|  21966 | 5767 | `	pReplace = pSearch = 0; /* cc warning */` |
|  21966 | 5768 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 5769 | `	/* Extract function name */` |
|  21966 | 5770 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 5771 | `	/* Set the default pattern match routine */` |
|  21966 | 5772 | `	xMatch = SyBlobSearch;` |
|  21966 | 5773 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 5774 | `		/* Case insensitive pattern match */` |
|     11 | 5775 | `		xMatch = iPatternMatch;` |
|      5 | 5776 | `	}` |
|      - | 5777 | `	/* Start the replace process */` |
|  43938 | 5778 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 5779 | `		sxu32 nCount,nOfft;` |
|  21974 | 5780 | `		if( pSearch->nByte <  1 ){` |
|      - | 5781 | `			/* Empty string,ignore */` |
|      3 | 5782 | `			continue;` |
|      - | 5783 | `		}` |
|      - | 5784 | `		/* Extract the replace string */` |
|  21972 | 5785 | `		if( rep_str ){` |
|  21962 | 5786 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  10982 | 5787 | `		}else{` |
|     11 | 5788 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 5789 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 5790 | `				 * An empty string is used for the rest of replacement values` |
|      - | 5791 | `				 */` |
|      3 | 5792 | `				pReplace = 0;` |
|      1 | 5793 | `			}` |
|      - | 5794 | `		}` |
|  21972 | 5795 | `		if( pReplace == 0 ){` |
|      - | 5796 | `			/* Use an empty string instead */` |
|      3 | 5797 | `			pReplace = &sTemp;` |
|      1 | 5798 | `		}` |
|  21972 | 5799 | `		nOfft = nCount = 0;` |
|  11001 | 5800 | `		for(;;){` |
|  22004 | 5801 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 5802 | `				break;` |
|      - | 5803 | `			}` |
|      - | 5804 | `			/* Perform a pattern lookup */` |
|  32987 | 5805 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  21990 | 5806 | `				pSearch->nByte,&nOfft);` |
|  21992 | 5807 | `			if( rc != SXRET_OK ){` |
|      - | 5808 | `				/* Pattern not found */` |
|  21960 | 5809 | `				break;` |
|      - | 5810 | `			}` |
|      - | 5811 | `			/* Perform the replace operation */` |
|     33 | 5812 | `			StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|      - | 5813 | `			/* Increment offset counter */` |
|     33 | 5814 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 5815 | `		}` |
|      2 | 5816 | `	}` |
|      - | 5817 | `	/* All done,clean-up the mess left behind */` |
|  21966 | 5818 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  21966 | 5819 | `	SySetRelease(&sSearch);` |
|  21966 | 5820 | `	SySetRelease(&sReplace);` |
|  21966 | 5821 | `	SyBlobRelease(&sWorker);` |
|  21966 | 5822 | `	return PH7_OK;` |
|  11003 | 5823 |  |
|      - | 5824 | `/*` |
|      - | 5825 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 5826 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 5827 | ` *  Translate characters or replace substrings.` |
|      - | 5828 | ` * Parameters` |
|      - | 5829 | ` *  $str` |
|      - | 5830 | ` *  The string being translated.` |
|      - | 5831 | ` * $from` |
|      - | 5832 | ` *  The string being translated to to.` |
|      - | 5833 | ` * $to` |
|      - | 5834 | ` *  The string replacing from.` |
|      - | 5835 | ` * $replace_pairs` |
|      - | 5836 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 5837 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 5838 | ` * Return` |
|      - | 5839 | ` *  The translated string.` |
|      - | 5840 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 5841 | ` */` |
|     12 | 5842 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5843 |  |
|      - | 5844 | `	const char *zIn;` |
|      - | 5845 | `	int nLen;` |
|     13 | 5846 | `	if( nArg < 1 ){` |
|      - | 5847 | `		/* Nothing to replace,return FALSE */` |
|      7 | 5848 | `		ph7_result_bool(pCtx,0);` |
|      7 | 5849 | `		return PH7_OK;` |
|      - | 5850 | `	}` |
|      7 | 5851 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 5852 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 5853 | `		/* Invalid arguments */` |
|    ! 0 | 5854 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 5855 | `		return PH7_OK;` |
|      - | 5856 | `	}` |
|      9 | 5857 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 5858 | `		str_replace_data sRepData;` |
|      - | 5859 | `		SyBlob sWorker;` |
|      - | 5860 | `		/* Initilaize the working buffer */` |
|      5 | 5861 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|      - | 5862 | `		/* Copy raw string */` |
|      5 | 5863 | `		SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nLen);` |
|      - | 5864 | `		/* Init our replace data instance */` |
|      5 | 5865 | `		sRepData.pWorker = &sWorker;` |
|      5 | 5866 | `		sRepData.xMatch = SyBlobSearch;` |
|      - | 5867 | `		/* Iterate throw array entries and perform the replace operation.*/` |
|      5 | 5868 | `		ph7_array_walk(apArg[1],StringReplaceWalker,&sRepData);` |
|      - | 5869 | `		/* All done, return the result string */` |
|      7 | 5870 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|      4 | 5871 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 5872 | `		/* Clean-up */` |
|      5 | 5873 | `		SyBlobRelease(&sWorker);` |
|      3 | 5874 | `	}else{` |
|      - | 5875 | `		int i,flen,tlen,c,iOfft;` |
|      - | 5876 | `		const char *zFrom,*zTo;` |
|      3 | 5877 | `		if( nArg < 3 ){` |
|      - | 5878 | `			/* Nothing to replace */` |
|    ! 0 | 5879 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 5880 | `			return PH7_OK;` |
|      - | 5881 | `		}` |
|      - | 5882 | `		/* Extract given arguments */` |
|      3 | 5883 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 5884 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 5885 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 5886 | `			/* Nothing to replace */` |
|    ! 0 | 5887 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 5888 | `			return PH7_OK;` |
|      - | 5889 | `		}` |
|      - | 5890 | `		/* Start the replace process */` |
|     13 | 5891 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 5892 | `			c = zIn[i];` |
|     11 | 5893 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 5894 | `				if ( iOfft < tlen ){` |
|      5 | 5895 | `					c = zTo[iOfft];` |
|      2 | 5896 | `				}` |
|      2 | 5897 | `			}` |
|     11 | 5898 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 5899 |  |
|      6 | 5900 | `		}` |
|      - | 5901 | `	}` |
|      7 | 5902 | `	return PH7_OK;` |
|      7 | 5903 |  |
|      - | 5904 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 5905 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 5906 | `/*` |
|      - | 5907 | ` * Parse an INI string.` |
|      - | 5908 |  |
|      - | 5909 | ` * According to wikipedia` |
|      - | 5910 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 5911 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 5912 | ` *  Format` |
|      - | 5913 | `*    Properties` |
|      - | 5914 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 5915 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 5916 | `*     Example:` |
|      - | 5917 | `*      name=value` |
|      - | 5918 | `*    Sections` |
|      - | 5919 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 5920 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 5921 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 5922 | `*     or the end of the file. Sections may not be nested.` |
|      - | 5923 | `*     Example:` |
|      - | 5924 | `*      [section]` |
|      - | 5925 | `*   Comments` |
|      - | 5926 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 5927 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 5928 | `*/` |
|     12 | 5929 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 5930 |  |
|      - | 5931 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 5932 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 5933 | `	SyHashEntry *pEntry;` |
|      - | 5934 | `	SyString sEntry;` |
|      - | 5935 | `	SyHash sHash;` |
|      - | 5936 | `	int c;` |
|      - | 5937 | `	/* Create an empty array and worker variables */` |
|     13 | 5938 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 5939 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 5940 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 5941 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 5942 | `		/* Out of memory */` |
|    ! 0 | 5943 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      - | 5944 | `		/* Return FALSE */` |
|    ! 0 | 5945 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5946 | `		return PH7_OK;` |
|      - | 5947 | `	}` |
|     13 | 5948 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 5949 | `	pCur = pArray;` |
|      - | 5950 | `	/* Start the parse process */` |
|     21 | 5951 | `	for(;;){` |
|      - | 5952 | `		/* Ignore leading white spaces */` |
|     69 | 5953 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 5954 | `			zIn++;` |
|      1 | 5955 | `		}` |
|     43 | 5956 | `		if( zIn >= zEnd ){` |
|      - | 5957 | `			/* No more input to process */` |
|     13 | 5958 | `			break;` |
|      - | 5959 | `		}` |
|     31 | 5960 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 5961 | `			/* Comment til the end of line */` |
|    ! 0 | 5962 | `			zIn++;` |
|    ! 0 | 5963 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 5964 | `				zIn++;` |
|    ! 0 | 5965 | `			}` |
|    ! 0 | 5966 | `			continue;` |
|      - | 5967 | `		}` |
|      - | 5968 | `		/* Reset the string cursor of the working variable */` |
|     31 | 5969 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 5970 | `		if( zIn[0] == '[' ){` |
|      - | 5971 | `			/* Section: Extract the section name */` |
|      9 | 5972 | `			zIn++;` |
|      9 | 5973 | `			zCur = zIn;` |
|     73 | 5974 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 5975 | `				zIn++;` |
|      1 | 5976 | `			}` |
|      9 | 5977 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 5978 | `				/* Save the section name */` |
|      5 | 5979 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 5980 | `				SyStringFullTrim(&sEntry);` |
|      5 | 5981 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 5982 | `				if( sEntry.nByte > 0 ){` |
|      - | 5983 | `					/* Associate an array with the section */` |
|      5 | 5984 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 5985 | `					if( pSection ){` |
|      5 | 5986 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 5987 | `						pCur = pSection;` |
|      2 | 5988 | `					}` |
|      2 | 5989 | `				}` |
|      2 | 5990 | `			}` |
|      9 | 5991 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 5992 | `		}else{` |
|      - | 5993 | `			ph7_value *pOldCur;` |
|      - | 5994 | `			int is_array;` |
|      - | 5995 | `			int iLen;` |
|      - | 5996 | `			/* Properties */` |
|     23 | 5997 | `			is_array = 0;` |
|     23 | 5998 | `			zCur = zIn;` |
|     23 | 5999 | `			iLen = 0; /* cc warning */` |
|     23 | 6000 | `			pOldCur = pCur;` |
|    155 | 6001 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 6002 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 6003 | `					/* Array */` |
|    ! 0 | 6004 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 6005 | `					is_array = 1;` |
|    ! 0 | 6006 | `					if( iLen > 0 ){` |
|    ! 0 | 6007 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 6008 | `						/* Query the hashtable */` |
|    ! 0 | 6009 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 6010 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 6011 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 6012 | `						if( pEntry ){` |
|    ! 0 | 6013 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 6014 | `						}else{` |
|      - | 6015 | `							/* Create an empty array */` |
|    ! 0 | 6016 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 6017 | `							if( pvArr ){` |
|      - | 6018 | `								/* Save the entry */` |
|    ! 0 | 6019 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 6020 | `								/* Insert the entry */` |
|    ! 0 | 6021 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6022 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 6023 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 6024 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6025 | `							}` |
|      - | 6026 | `						}` |
|    ! 0 | 6027 | `						if( pvArr ){` |
|    ! 0 | 6028 | `							pCur = pvArr;` |
|    ! 0 | 6029 | `						}` |
|    ! 0 | 6030 | `					}` |
|    ! 0 | 6031 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 6032 | `						zIn++;` |
|    ! 0 | 6033 | `					}` |
|    ! 0 | 6034 | `				}` |
|    133 | 6035 | `				zIn++;` |
|      1 | 6036 | `			}` |
|     23 | 6037 | `			if( !is_array ){` |
|     23 | 6038 | `				iLen = (int)(zIn-zCur);` |
|     11 | 6039 | `			}` |
|      - | 6040 | `			/* Trim the key */` |
|     23 | 6041 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 6042 | `			SyStringFullTrim(&sEntry);` |
|     23 | 6043 | `			if( sEntry.nByte > 0 ){` |
|     23 | 6044 | `				if( !is_array ){` |
|      - | 6045 | `					/* Save the key name */` |
|     23 | 6046 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 6047 | `				}` |
|      - | 6048 | `				/* extract key value */` |
|     23 | 6049 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 6050 | `				zIn++; /* '=' */` |
|     39 | 6051 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 6052 | `					zIn++;` |
|      1 | 6053 | `				}` |
|     23 | 6054 | `				if( zIn < zEnd ){` |
|     21 | 6055 | `					zCur = zIn;` |
|     21 | 6056 | `					c = zIn[0];` |
|     21 | 6057 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6058 | `						zIn++;` |
|      - | 6059 | `						/* Delimit the value */` |
|    ! 0 | 6060 | `						while( zIn < zEnd ){` |
|    ! 0 | 6061 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 6062 | `								break;` |
|      - | 6063 | `							}` |
|    ! 0 | 6064 | `							zIn++;` |
|    ! 0 | 6065 | `						}` |
|    ! 0 | 6066 | `						if( zIn < zEnd ){` |
|    ! 0 | 6067 | `							zIn++;` |
|    ! 0 | 6068 | `						}` |
|    ! 0 | 6069 | `					}else{` |
|    125 | 6070 | `						while( zIn < zEnd ){` |
|    123 | 6071 | `							if( zIn[0] == '\n' ){` |
|     19 | 6072 | `								if( zIn[-1] != '\\' ){` |
|     19 | 6073 | `									break;` |
|    ! 0 | 6074 | `								}` |
|    105 | 6075 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6076 | `								/* Inline comments */` |
|    ! 0 | 6077 | `								break;` |
|      - | 6078 | `							}` |
|    105 | 6079 | `							zIn++;` |
|      1 | 6080 | `						}` |
|      - | 6081 | `					}` |
|      - | 6082 | `					/* Trim the value */` |
|     21 | 6083 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 6084 | `					SyStringFullTrim(&sEntry);` |
|     21 | 6085 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6086 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 6087 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 6088 | `					}` |
|     21 | 6089 | `					if( sEntry.nByte > 0 ){` |
|     21 | 6090 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 6091 | `					}` |
|      - | 6092 | `					/* Insert the key and it's value */` |
|     21 | 6093 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 6094 | `				}` |
|     12 | 6095 | `			}else{` |
|    ! 0 | 6096 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 6097 | `					zIn++;` |
|    ! 0 | 6098 | `				}` |
|      - | 6099 | `			}` |
|     23 | 6100 | `			pCur = pOldCur;` |
|      - | 6101 | `		}` |
|      1 | 6102 | `	}` |
|     13 | 6103 | `	SyHashRelease(&sHash);` |
|      - | 6104 | `	/* Return the parse of the INI string */` |
|     13 | 6105 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 6106 | `	return SXRET_OK;` |
|      7 | 6107 |  |
|      - | 6108 | `/*` |
|      - | 6109 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 6110 | ` *  Parse a configuration string.` |
|      - | 6111 | ` * Parameters` |
|      - | 6112 | ` *  $ini` |
|      - | 6113 | ` *   The contents of the ini file being parsed.` |
|      - | 6114 | ` *  $process_sections` |
|      - | 6115 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 6116 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 6117 | ` *  $scanner_mode (Not used)` |
|      - | 6118 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 6119 | ` *   then option values will not be parsed.` |
|      - | 6120 | ` * Return` |
|      - | 6121 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 6122 | ` */` |
|     10 | 6123 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6124 |  |
|      - | 6125 | `	const char *zIni;` |
|      - | 6126 | `	int nByte;` |
|     11 | 6127 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6128 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 6129 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6130 | `		return PH7_OK;` |
|      - | 6131 | `	}` |
|      - | 6132 | `	/* Extract the raw INI buffer */` |
|     11 | 6133 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 6134 | `	/* Process the INI buffer*/` |
|     11 | 6135 | `	PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|     11 | 6136 | `	return PH7_OK;` |
|      6 | 6137 |  |
|      - | 6138 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6139 |  |
|      - | 6140 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6141 |  |
|      - | 6142 | `/*` |
|      - | 6143 | ` * Ctype Functions.` |
|      - | 6144 | ` * Status:` |
|      - | 6145 | ` *    Stable.` |
|      - | 6146 | ` */` |
|      - | 6147 | `/*` |
|      - | 6148 | ` * bool ctype_alnum(string $text)` |
|      - | 6149 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 6150 | ` * Parameters` |
|      - | 6151 | ` *  $text` |
|      - | 6152 | ` *   The tested string.` |
|      - | 6153 | ` * Return` |
|      - | 6154 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 6155 | ` */` |
|     16 | 6156 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6157 |  |
|      - | 6158 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6159 | `	int nLen;` |
|     17 | 6160 | `	if( nArg < 1 ){` |
|      - | 6161 | `		/* Missing arguments,return FALSE */` |
|      3 | 6162 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6163 | `		return PH7_OK;` |
|      - | 6164 | `	}` |
|      - | 6165 | `	/* Extract the target string */` |
|     15 | 6166 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 6167 | `	zEnd = &zIn[nLen];` |
|     15 | 6168 | `	if( nLen < 1 ){` |
|      - | 6169 | `		/* Empty string,return FALSE */` |
|      3 | 6170 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6171 | `		return PH7_OK;` |
|      - | 6172 | `	}` |
|      - | 6173 | `	/* Perform the requested operation */` |
|     32 | 6174 | `	for(;;){` |
|     65 | 6175 | `		if( zIn >= zEnd ){` |
|      - | 6176 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6177 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6178 | `			return PH7_OK;` |
|      - | 6179 | `		}` |
|     57 | 6180 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 6181 | `			break;` |
|      - | 6182 | `		}` |
|      - | 6183 | `		/* Point to the next character */` |
|     53 | 6184 | `		zIn++;` |
|      1 | 6185 | `	}` |
|      - | 6186 | `	/* The test failed,return FALSE */` |
|      5 | 6187 | `	ph7_result_bool(pCtx,0);` |
|      5 | 6188 | `	return PH7_OK;` |
|      9 | 6189 |  |
|      - | 6190 | `/*` |
|      - | 6191 | ` * bool ctype_alpha(string $text)` |
|      - | 6192 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 6193 | ` * Parameters` |
|      - | 6194 | ` *  $text` |
|      - | 6195 | ` *   The tested string.` |
|      - | 6196 | ` * Return` |
|      - | 6197 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 6198 | ` */` |
|     18 | 6199 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6200 |  |
|      - | 6201 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6202 | `	int nLen;` |
|     19 | 6203 | `	if( nArg < 1 ){` |
|      - | 6204 | `		/* Missing arguments,return FALSE */` |
|      3 | 6205 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6206 | `		return PH7_OK;` |
|      - | 6207 | `	}` |
|      - | 6208 | `	/* Extract the target string */` |
|     17 | 6209 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6210 | `	zEnd = &zIn[nLen];` |
|     17 | 6211 | `	if( nLen < 1 ){` |
|      - | 6212 | `		/* Empty string,return FALSE */` |
|      3 | 6213 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6214 | `		return PH7_OK;` |
|      - | 6215 | `	}` |
|      - | 6216 | `	/* Perform the requested operation */` |
|     42 | 6217 | `	for(;;){` |
|     85 | 6218 | `		if( zIn >= zEnd ){` |
|      - | 6219 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6220 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6221 | `			return PH7_OK;` |
|      - | 6222 | `		}` |
|     77 | 6223 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 6224 | `			break;` |
|      - | 6225 | `		}` |
|      - | 6226 | `		/* Point to the next character */` |
|     71 | 6227 | `		zIn++;` |
|      1 | 6228 | `	}` |
|      - | 6229 | `	/* The test failed,return FALSE */` |
|      7 | 6230 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6231 | `	return PH7_OK;` |
|     10 | 6232 |  |
|      - | 6233 | `/*` |
|      - | 6234 | ` * bool ctype_cntrl(string $text)` |
|      - | 6235 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 6236 | ` * Parameters` |
|      - | 6237 | ` *  $text` |
|      - | 6238 | ` *   The tested string.` |
|      - | 6239 | ` * Return` |
|      - | 6240 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 6241 | ` */` |
|     18 | 6242 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6243 |  |
|      - | 6244 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6245 | `	int nLen;` |
|     19 | 6246 | `	if( nArg < 1 ){` |
|      - | 6247 | `		/* Missing arguments,return FALSE */` |
|      3 | 6248 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6249 | `		return PH7_OK;` |
|      - | 6250 | `	}` |
|      - | 6251 | `	/* Extract the target string */` |
|     17 | 6252 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6253 | `	zEnd = &zIn[nLen];` |
|     17 | 6254 | `	if( nLen < 1 ){` |
|      - | 6255 | `		/* Empty string,return FALSE */` |
|      3 | 6256 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6257 | `		return PH7_OK;` |
|      - | 6258 | `	}` |
|      - | 6259 | `	/* Perform the requested operation */` |
|     14 | 6260 | `	for(;;){` |
|     29 | 6261 | `		if( zIn >= zEnd ){` |
|      - | 6262 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6263 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6264 | `			return PH7_OK;` |
|      - | 6265 | `		}` |
|     21 | 6266 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6267 | `			/* UTF-8 stream  */` |
|    ! 0 | 6268 | `			break;` |
|      - | 6269 | `		}` |
|     21 | 6270 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 6271 | `			break;` |
|      - | 6272 | `		}` |
|      - | 6273 | `		/* Point to the next character */` |
|     15 | 6274 | `		zIn++;` |
|      1 | 6275 | `	}` |
|      - | 6276 | `	/* The test failed,return FALSE */` |
|      7 | 6277 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6278 | `	return PH7_OK;` |
|     10 | 6279 |  |
|      - | 6280 | `/*` |
|      - | 6281 | ` * bool ctype_digit(string $text)` |
|      - | 6282 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 6283 | ` * Parameters` |
|      - | 6284 | ` *  $text` |
|      - | 6285 | ` *   The tested string.` |
|      - | 6286 | ` * Return` |
|      - | 6287 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 6288 | ` */` |
|   1546 | 6289 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6290 |  |
|      - | 6291 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6292 | `	int nLen;` |
|   1548 | 6293 | `	if( nArg < 1 ){` |
|      - | 6294 | `		/* Missing arguments,return FALSE */` |
|      3 | 6295 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6296 | `		return PH7_OK;` |
|      - | 6297 | `	}` |
|      - | 6298 | `	/* Extract the target string */` |
|   1546 | 6299 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1546 | 6300 | `	zEnd = &zIn[nLen];` |
|   1546 | 6301 | `	if( nLen < 1 ){` |
|      - | 6302 | `		/* Empty string,return FALSE */` |
|      3 | 6303 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6304 | `		return PH7_OK;` |
|      - | 6305 | `	}` |
|      - | 6306 | `	/* Perform the requested operation */` |
|   1448 | 6307 | `	for(;;){` |
|   2898 | 6308 | `		if( zIn >= zEnd ){` |
|      - | 6309 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1324 | 6310 | `			ph7_result_bool(pCtx,1);` |
|   1324 | 6311 | `			return PH7_OK;` |
|      - | 6312 | `		}` |
|   1576 | 6313 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6314 | `			/* UTF-8 stream  */` |
|    ! 0 | 6315 | `			break;` |
|      - | 6316 | `		}` |
|   1576 | 6317 | `		if( !SyisDigit(zIn[0]) ){` |
|    222 | 6318 | `			break;` |
|      - | 6319 | `		}` |
|      - | 6320 | `		/* Point to the next character */` |
|   1356 | 6321 | `		zIn++;` |
|      2 | 6322 | `	}` |
|      - | 6323 | `	/* The test failed,return FALSE */` |
|    222 | 6324 | `	ph7_result_bool(pCtx,0);` |
|    222 | 6325 | `	return PH7_OK;` |
|    775 | 6326 |  |
|      - | 6327 | `/*` |
|      - | 6328 | ` * bool ctype_xdigit(string $text)` |
|      - | 6329 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 6330 | ` * Parameters` |
|      - | 6331 | ` *  $text` |
|      - | 6332 | ` *   The tested string.` |
|      - | 6333 | ` * Return` |
|      - | 6334 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 6335 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 6336 | ` */` |
|     20 | 6337 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6338 |  |
|      - | 6339 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6340 | `	int nLen;` |
|     21 | 6341 | `	if( nArg < 1 ){` |
|      - | 6342 | `		/* Missing arguments,return FALSE */` |
|      3 | 6343 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6344 | `		return PH7_OK;` |
|      - | 6345 | `	}` |
|      - | 6346 | `	/* Extract the target string */` |
|     19 | 6347 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 6348 | `	zEnd = &zIn[nLen];` |
|     19 | 6349 | `	if( nLen < 1 ){` |
|      - | 6350 | `		/* Empty string,return FALSE */` |
|      3 | 6351 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6352 | `		return PH7_OK;` |
|      - | 6353 | `	}` |
|      - | 6354 | `	/* Perform the requested operation */` |
|     46 | 6355 | `	for(;;){` |
|     93 | 6356 | `		if( zIn >= zEnd ){` |
|      - | 6357 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 6358 | `			ph7_result_bool(pCtx,1);` |
|     11 | 6359 | `			return PH7_OK;` |
|      - | 6360 | `		}` |
|     83 | 6361 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6362 | `			/* UTF-8 stream  */` |
|    ! 0 | 6363 | `			break;` |
|      - | 6364 | `		}` |
|     83 | 6365 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 6366 | `			break;` |
|      - | 6367 | `		}` |
|      - | 6368 | `		/* Point to the next character */` |
|     77 | 6369 | `		zIn++;` |
|      1 | 6370 | `	}` |
|      - | 6371 | `	/* The test failed,return FALSE */` |
|      7 | 6372 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6373 | `	return PH7_OK;` |
|     11 | 6374 |  |
|      - | 6375 | `/*` |
|      - | 6376 | ` * bool ctype_graph(string $text)` |
|      - | 6377 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 6378 | ` * Parameters` |
|      - | 6379 | ` *  $text` |
|      - | 6380 | ` *   The tested string.` |
|      - | 6381 | ` * Return` |
|      - | 6382 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 6383 | ` * (no white space), FALSE otherwise.` |
|      - | 6384 | ` */` |
|     18 | 6385 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6386 |  |
|      - | 6387 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6388 | `	int nLen;` |
|     19 | 6389 | `	if( nArg < 1 ){` |
|      - | 6390 | `		/* Missing arguments,return FALSE */` |
|      3 | 6391 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6392 | `		return PH7_OK;` |
|      - | 6393 | `	}` |
|      - | 6394 | `	/* Extract the target string */` |
|     17 | 6395 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6396 | `	zEnd = &zIn[nLen];` |
|     17 | 6397 | `	if( nLen < 1 ){` |
|      - | 6398 | `		/* Empty string,return FALSE */` |
|      3 | 6399 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6400 | `		return PH7_OK;` |
|      - | 6401 | `	}` |
|      - | 6402 | `	/* Perform the requested operation */` |
|     57 | 6403 | `	for(;;){` |
|    115 | 6404 | `		if( zIn >= zEnd ){` |
|      - | 6405 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6406 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6407 | `			return PH7_OK;` |
|      - | 6408 | `		}` |
|    107 | 6409 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6410 | `			/* UTF-8 stream  */` |
|    ! 0 | 6411 | `			break;` |
|      - | 6412 | `		}` |
|    107 | 6413 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 6414 | `			break;` |
|      - | 6415 | `		}` |
|      - | 6416 | `		/* Point to the next character */` |
|    101 | 6417 | `		zIn++;` |
|      1 | 6418 | `	}` |
|      - | 6419 | `	/* The test failed,return FALSE */` |
|      7 | 6420 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6421 | `	return PH7_OK;` |
|     10 | 6422 |  |
|      - | 6423 | `/*` |
|      - | 6424 | ` * bool ctype_print(string $text)` |
|      - | 6425 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 6426 | ` * Parameters` |
|      - | 6427 | ` *  $text` |
|      - | 6428 | ` *   The tested string.` |
|      - | 6429 | ` * Return` |
|      - | 6430 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 6431 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 6432 | ` *  or control function at all.` |
|      - | 6433 | ` */` |
|     18 | 6434 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6435 |  |
|      - | 6436 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6437 | `	int nLen;` |
|     19 | 6438 | `	if( nArg < 1 ){` |
|      - | 6439 | `		/* Missing arguments,return FALSE */` |
|      3 | 6440 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6441 | `		return PH7_OK;` |
|      - | 6442 | `	}` |
|      - | 6443 | `	/* Extract the target string */` |
|     17 | 6444 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6445 | `	zEnd = &zIn[nLen];` |
|     17 | 6446 | `	if( nLen < 1 ){` |
|      - | 6447 | `		/* Empty string,return FALSE */` |
|      3 | 6448 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6449 | `		return PH7_OK;` |
|      - | 6450 | `	}` |
|      - | 6451 | `	/* Perform the requested operation */` |
|     63 | 6452 | `	for(;;){` |
|    127 | 6453 | `		if( zIn >= zEnd ){` |
|      - | 6454 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6455 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6456 | `			return PH7_OK;` |
|      - | 6457 | `		}` |
|    119 | 6458 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6459 | `			/* UTF-8 stream  */` |
|    ! 0 | 6460 | `			break;` |
|      - | 6461 | `		}` |
|    119 | 6462 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 6463 | `			break;` |
|      - | 6464 | `		}` |
|      - | 6465 | `		/* Point to the next character */` |
|    113 | 6466 | `		zIn++;` |
|      1 | 6467 | `	}` |
|      - | 6468 | `	/* The test failed,return FALSE */` |
|      7 | 6469 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6470 | `	return PH7_OK;` |
|     10 | 6471 |  |
|      - | 6472 | `/*` |
|      - | 6473 | ` * bool ctype_punct(string $text)` |
|      - | 6474 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 6475 | ` * Parameters` |
|      - | 6476 | ` *  $text` |
|      - | 6477 | ` *   The tested string.` |
|      - | 6478 | ` * Return` |
|      - | 6479 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 6480 | ` *  digit or blank, FALSE otherwise.` |
|      - | 6481 | ` */` |
|     20 | 6482 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6483 |  |
|      - | 6484 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6485 | `	int nLen;` |
|     21 | 6486 | `	if( nArg < 1 ){` |
|      - | 6487 | `		/* Missing arguments,return FALSE */` |
|      3 | 6488 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6489 | `		return PH7_OK;` |
|      - | 6490 | `	}` |
|      - | 6491 | `	/* Extract the target string */` |
|     19 | 6492 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 6493 | `	zEnd = &zIn[nLen];` |
|     19 | 6494 | `	if( nLen < 1 ){` |
|      - | 6495 | `		/* Empty string,return FALSE */` |
|      3 | 6496 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6497 | `		return PH7_OK;` |
|      - | 6498 | `	}` |
|      - | 6499 | `	/* Perform the requested operation */` |
|     38 | 6500 | `	for(;;){` |
|     77 | 6501 | `		if( zIn >= zEnd ){` |
|      - | 6502 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6503 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6504 | `			return PH7_OK;` |
|      - | 6505 | `		}` |
|     69 | 6506 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6507 | `			/* UTF-8 stream  */` |
|    ! 0 | 6508 | `			break;` |
|      - | 6509 | `		}` |
|     69 | 6510 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 6511 | `			break;` |
|      - | 6512 | `		}` |
|      - | 6513 | `		/* Point to the next character */` |
|     61 | 6514 | `		zIn++;` |
|      1 | 6515 | `	}` |
|      - | 6516 | `	/* The test failed,return FALSE */` |
|      9 | 6517 | `	ph7_result_bool(pCtx,0);` |
|      9 | 6518 | `	return PH7_OK;` |
|     11 | 6519 |  |
|      - | 6520 | `/*` |
|      - | 6521 | ` * bool ctype_space(string $text)` |
|      - | 6522 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 6523 | ` * Parameters` |
|      - | 6524 | ` *  $text` |
|      - | 6525 | ` *   The tested string.` |
|      - | 6526 | ` * Return` |
|      - | 6527 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 6528 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 6529 | ` *  and form feed characters.` |
|      - | 6530 | ` */` |
|  59528 | 6531 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6532 |  |
|      - | 6533 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6534 | `	int nLen;` |
|  59530 | 6535 | `	if( nArg < 1 ){` |
|      - | 6536 | `		/* Missing arguments,return FALSE */` |
|      3 | 6537 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6538 | `		return PH7_OK;` |
|      - | 6539 | `	}` |
|      - | 6540 | `	/* Extract the target string */` |
|  59528 | 6541 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  59528 | 6542 | `	zEnd = &zIn[nLen];` |
|  59528 | 6543 | `	if( nLen < 1 ){` |
|      - | 6544 | `		/* Empty string,return FALSE */` |
|      3 | 6545 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6546 | `		return PH7_OK;` |
|      - | 6547 | `	}` |
|      - | 6548 | `	/* Perform the requested operation */` |
|  30792 | 6549 | `	for(;;){` |
|  61542 | 6550 | `		if( zIn >= zEnd ){` |
|      - | 6551 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1994 | 6552 | `			ph7_result_bool(pCtx,1);` |
|   1994 | 6553 | `			return PH7_OK;` |
|      - | 6554 | `		}` |
|  59550 | 6555 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6556 | `			/* UTF-8 stream  */` |
|    ! 0 | 6557 | `			break;` |
|      - | 6558 | `		}` |
|  59550 | 6559 | `		if( !SyisSpace(zIn[0]) ){` |
|  57534 | 6560 | `			break;` |
|      - | 6561 | `		}` |
|      - | 6562 | `		/* Point to the next character */` |
|   2018 | 6563 | `		zIn++;` |
|      2 | 6564 | `	}` |
|      - | 6565 | `	/* The test failed,return FALSE */` |
|  57534 | 6566 | `	ph7_result_bool(pCtx,0);` |
|  57534 | 6567 | `	return PH7_OK;` |
|  29788 | 6568 |  |
|      - | 6569 | `/*` |
|      - | 6570 | ` * bool ctype_lower(string $text)` |
|      - | 6571 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 6572 | ` * Parameters` |
|      - | 6573 | ` *  $text` |
|      - | 6574 | ` *   The tested string.` |
|      - | 6575 | ` * Return` |
|      - | 6576 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 6577 | ` */` |
|     18 | 6578 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6579 |  |
|      - | 6580 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6581 | `	int nLen;` |
|     19 | 6582 | `	if( nArg < 1 ){` |
|      - | 6583 | `		/* Missing arguments,return FALSE */` |
|      3 | 6584 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6585 | `		return PH7_OK;` |
|      - | 6586 | `	}` |
|      - | 6587 | `	/* Extract the target string */` |
|     17 | 6588 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6589 | `	zEnd = &zIn[nLen];` |
|     17 | 6590 | `	if( nLen < 1 ){` |
|      - | 6591 | `		/* Empty string,return FALSE */` |
|      3 | 6592 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6593 | `		return PH7_OK;` |
|      - | 6594 | `	}` |
|      - | 6595 | `	/* Perform the requested operation */` |
|     27 | 6596 | `	for(;;){` |
|     55 | 6597 | `		if( zIn >= zEnd ){` |
|      - | 6598 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 6599 | `			ph7_result_bool(pCtx,1);` |
|      5 | 6600 | `			return PH7_OK;` |
|      - | 6601 | `		}` |
|     51 | 6602 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 6603 | `			break;` |
|      - | 6604 | `		}` |
|      - | 6605 | `		/* Point to the next character */` |
|     41 | 6606 | `		zIn++;` |
|      1 | 6607 | `	}` |
|      - | 6608 | `	/* The test failed,return FALSE */` |
|     11 | 6609 | `	ph7_result_bool(pCtx,0);` |
|     11 | 6610 | `	return PH7_OK;` |
|     10 | 6611 |  |
|      - | 6612 | `/*` |
|      - | 6613 | ` * bool ctype_upper(string $text)` |
|      - | 6614 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 6615 | ` * Parameters` |
|      - | 6616 | ` *  $text` |
|      - | 6617 | ` *   The tested string.` |
|      - | 6618 | ` * Return` |
|      - | 6619 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 6620 | ` */` |
|     18 | 6621 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6622 |  |
|      - | 6623 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6624 | `	int nLen;` |
|     19 | 6625 | `	if( nArg < 1 ){` |
|      - | 6626 | `		/* Missing arguments,return FALSE */` |
|      3 | 6627 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6628 | `		return PH7_OK;` |
|      - | 6629 | `	}` |
|      - | 6630 | `	/* Extract the target string */` |
|     17 | 6631 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6632 | `	zEnd = &zIn[nLen];` |
|     17 | 6633 | `	if( nLen < 1 ){` |
|      - | 6634 | `		/* Empty string,return FALSE */` |
|      3 | 6635 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6636 | `		return PH7_OK;` |
|      - | 6637 | `	}` |
|      - | 6638 | `	/* Perform the requested operation */` |
|     28 | 6639 | `	for(;;){` |
|     57 | 6640 | `		if( zIn >= zEnd ){` |
|      - | 6641 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 6642 | `			ph7_result_bool(pCtx,1);` |
|      5 | 6643 | `			return PH7_OK;` |
|      - | 6644 | `		}` |
|     53 | 6645 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 6646 | `			break;` |
|      - | 6647 | `		}` |
|      - | 6648 | `		/* Point to the next character */` |
|     43 | 6649 | `		zIn++;` |
|      1 | 6650 | `	}` |
|      - | 6651 | `	/* The test failed,return FALSE */` |
|     11 | 6652 | `	ph7_result_bool(pCtx,0);` |
|     11 | 6653 | `	return PH7_OK;` |
|     10 | 6654 |  |
|      - | 6655 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 6656 | `/*` |
|      - | 6657 | ` * Section:` |
|      - | 6658 | ` *    URL handling Functions.` |
|      - | 6659 | ` * Status:` |
|      - | 6660 | ` *    Stable.` |
|      - | 6661 | ` */` |
|      - | 6662 | `/*` |
|      - | 6663 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 6664 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 6665 | ` */` |
|   1026 | 6666 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 6667 |  |
|      - | 6668 | `	/* Store in the call context result buffer */` |
|   1028 | 6669 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 6670 | `	return SXRET_OK;` |
|      2 | 6671 |  |
|      - | 6672 | `/*` |
|      - | 6673 | ` * string base64_encode(string $data)` |
|      - | 6674 | ` * string convert_uuencode(string $data)` |
|      - | 6675 | ` *  Encodes data with MIME base64` |
|      - | 6676 | ` * Parameter` |
|      - | 6677 | ` *  $data` |
|      - | 6678 | ` *    Data to encode` |
|      - | 6679 | ` * Return` |
|      - | 6680 | ` *  Encoded data or FALSE on failure.` |
|      - | 6681 | ` */` |
|     10 | 6682 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6683 |  |
|      - | 6684 | `	const char *zIn;` |
|      - | 6685 | `	int nLen;` |
|     11 | 6686 | `	if( nArg < 1 ){` |
|      - | 6687 | `		/* Missing arguments,return FALSE */` |
|      5 | 6688 | `		ph7_result_bool(pCtx,0);` |
|      5 | 6689 | `		return PH7_OK;` |
|      - | 6690 | `	}` |
|      - | 6691 | `	/* Extract the input string */` |
|      7 | 6692 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 6693 | `	if( nLen < 1 ){` |
|      - | 6694 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 6695 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6696 | `		return PH7_OK;` |
|      - | 6697 | `	}` |
|      - | 6698 | `	/* Perform the BASE64 encoding */` |
|      7 | 6699 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 6700 | `	return PH7_OK;` |
|      6 | 6701 |  |
|      - | 6702 | `/*` |
|      - | 6703 | ` * string base64_decode(string $data)` |
|      - | 6704 | ` * string convert_uudecode(string $data)` |
|      - | 6705 | ` *  Decodes data encoded with MIME base64` |
|      - | 6706 | ` * Parameter` |
|      - | 6707 | ` *  $data` |
|      - | 6708 | ` *    Encoded data.` |
|      - | 6709 | ` * Return` |
|      - | 6710 | ` *  Returns the original data or FALSE on failure.` |
|      - | 6711 | ` */` |
|     36 | 6712 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6713 |  |
|      - | 6714 | `	const char *zIn;` |
|      - | 6715 | `	int nLen;` |
|     38 | 6716 | `	if( nArg < 1 ){` |
|      - | 6717 | `		/* Missing arguments,return FALSE */` |
|      3 | 6718 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6719 | `		return PH7_OK;` |
|      - | 6720 | `	}` |
|      - | 6721 | `	/* Extract the input string */` |
|     36 | 6722 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 6723 | `	if( nLen < 1 ){` |
|      - | 6724 | `		/* Nothing to process,return FALSE */` |
|      3 | 6725 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6726 | `		return PH7_OK;` |
|      - | 6727 | `	}` |
|      - | 6728 | `	/* Perform the BASE64 decoding */` |
|     34 | 6729 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 6730 | `	return PH7_OK;` |
|     20 | 6731 |  |
|      - | 6732 | `/*` |
|      - | 6733 | ` * string urlencode(string $str)` |
|      - | 6734 | ` *  URL encoding` |
|      - | 6735 | ` * Parameter` |
|      - | 6736 | ` *  $data` |
|      - | 6737 | ` *   Input string.` |
|      - | 6738 | ` * Return` |
|      - | 6739 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 6740 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 6741 | ` *  encoded as plus (+) signs.` |
|      - | 6742 | ` */` |
|      6 | 6743 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6744 |  |
|      - | 6745 | `	const char *zIn;` |
|      - | 6746 | `	int nLen;` |
|      7 | 6747 | `	if( nArg < 1 ){` |
|      - | 6748 | `		/* Missing arguments,return FALSE */` |
|      3 | 6749 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6750 | `		return PH7_OK;` |
|      - | 6751 | `	}` |
|      - | 6752 | `	/* Extract the input string */` |
|      5 | 6753 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 6754 | `	if( nLen < 1 ){` |
|      - | 6755 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 6756 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6757 | `		return PH7_OK;` |
|      - | 6758 | `	}` |
|      - | 6759 | `	/* Perform the URL encoding */` |
|      5 | 6760 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 6761 | `	return PH7_OK;` |
|      4 | 6762 |  |
|      - | 6763 | `/*` |
|      - | 6764 | ` * string urldecode(string $str)` |
|      - | 6765 | ` *  Decodes any %## encoding in the given string.` |
|      - | 6766 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 6767 | ` * Parameter` |
|      - | 6768 | ` *  $data` |
|      - | 6769 | ` *    Input string.` |
|      - | 6770 | ` * Return` |
|      - | 6771 | ` *  Decoded URL or FALSE on failure.` |
|      - | 6772 | ` */` |
|      8 | 6773 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6774 |  |
|      - | 6775 | `	const char *zIn;` |
|      - | 6776 | `	int nLen;` |
|      9 | 6777 | `	if( nArg < 1 ){` |
|      - | 6778 | `		/* Missing arguments,return FALSE */` |
|      3 | 6779 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6780 | `		return PH7_OK;` |
|      - | 6781 | `	}` |
|      - | 6782 | `	/* Extract the input string */` |
|      7 | 6783 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 6784 | `	if( nLen < 1 ){` |
|      - | 6785 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 6786 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6787 | `		return PH7_OK;` |
|      - | 6788 | `	}` |
|      - | 6789 | `	/* Perform the URL decoding */` |
|      7 | 6790 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 6791 | `	return PH7_OK;` |
|      5 | 6792 |  |
|      - | 6793 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6794 | `/* Table of the built-in functions */` |
|      - | 6795 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 6796 | `	   /* Variable handling functions */` |
|      - | 6797 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 6798 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 6799 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 6800 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 6801 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 6802 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 6803 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 6804 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 6805 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 6806 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 6807 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 6808 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 6809 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 6810 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 6811 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 6812 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 6813 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 6814 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 6815 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 6816 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 6817 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6818 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 6819 | `	   /* Math functions */` |
|      - | 6820 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 6821 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 6822 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 6823 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 6824 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 6825 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 6826 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 6827 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 6828 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 6829 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 6830 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 6831 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 6832 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 6833 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 6834 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 6835 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 6836 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 6837 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 6838 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 6839 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 6840 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 6841 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 6842 | `	{ "round",    PH7_builtin_round        },` |
|      - | 6843 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 6844 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 6845 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 6846 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 6847 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 6848 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 6849 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 6850 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 6851 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 6852 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6853 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6854 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 6855 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6856 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6857 | `	   /* String handling functions */` |
|      - | 6858 |  |
|      - | 6859 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 6860 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 6861 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 6862 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 6863 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 6864 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 6865 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 6866 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 6867 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 6868 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 6869 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 6870 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 6871 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 6872 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 6873 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 6874 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 6875 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 6876 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 6877 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 6878 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 6879 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 6880 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 6881 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 6882 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 6883 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 6884 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 6885 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 6886 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 6887 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 6888 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 6889 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 6890 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 6891 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 6892 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 6893 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 6894 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 6895 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 6896 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 6897 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 6898 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 6899 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 6900 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 6901 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 6902 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 6903 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 6904 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 6905 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 6906 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 6907 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - | 6908 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - | 6909 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - | 6910 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 6911 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6912 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6913 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 6914 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 6915 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 6916 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 6917 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6918 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6919 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 6920 |  |
|      - | 6921 |  |
|      - | 6922 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 6923 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 6924 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 6925 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 6926 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 6927 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6928 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6929 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 6930 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 6931 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6932 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6933 |  |
|      - | 6934 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 6935 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 6936 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 6937 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 6938 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 6939 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 6940 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 6941 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 6942 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 6943 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 6944 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 6945 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 6946 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6947 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6948 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 6949 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6950 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6951 |  |
|      - | 6952 | `	         /* Ctype functions */` |
|      - | 6953 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 6954 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 6955 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 6956 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 6957 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 6958 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 6959 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 6960 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 6961 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 6962 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 6963 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 6964 | `	         /* Time functions */` |
|      - | 6965 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 6966 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 6967 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 6968 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 6969 | `	{ "date",        PH7_builtin_date         },` |
|      - | 6970 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 6971 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 6972 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 6973 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 6974 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 6975 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 6976 | `	        /* URL functions */` |
|      - | 6977 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 6978 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 6979 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 6980 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 6981 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 6982 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 6983 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 6984 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 6985 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6986 | `};` |
|      - | 6987 | `/*` |
|      - | 6988 | ` * Register the built-in functions defined above,the array functions` |
|      - | 6989 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 6990 | ` */` |
|   2820 | 6991 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      2 | 6992 |  |
|      - | 6993 | `	sxu32 n;` |
| 445562 | 6994 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 442742 | 6995 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 221372 | 6996 | `	}` |
|      - | 6997 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   2822 | 6998 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 6999 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   2822 | 7000 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   2822 | 7001 |  |
|      - | 7002 |  |
