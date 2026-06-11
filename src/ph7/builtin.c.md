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
|    122 |   83 | `static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   84 |  |
|    123 |   85 | `	int res = 0; /* Assume false by default */` |
|    123 |   86 | `	if( nArg > 0 ){` |
|    121 |   87 | `		res = ph7_value_is_string(apArg[0]);` |
|     60 |   88 | `	}` |
|      - |   89 | `	/* Query result */` |
|    123 |   90 | `	ph7_result_bool(pCtx,res);` |
|    123 |   91 | `	return PH7_OK;` |
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
|    228 |  155 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  156 |  |
|    230 |  157 | `	int res = 0; /* Assume false by default */` |
|    230 |  158 | `	if( nArg > 0 ){` |
|    228 |  159 | `		res = ph7_value_is_array(apArg[0]);` |
|    113 |  160 | `	}` |
|      - |  161 | `	/* Query result */` |
|    230 |  162 | `	ph7_result_bool(pCtx,res);` |
|    230 |  163 | `	return PH7_OK;` |
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
|  25358 |  295 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  296 |  |
|  25360 |  297 | `	int res = 1; /* Assume empty by default */` |
|  25360 |  298 | `	if( nArg > 0 ){` |
|  25358 |  299 | `		res = ph7_value_is_empty(apArg[0]);` |
|  12678 |  300 | `	}` |
|  25360 |  301 | `	ph7_result_bool(pCtx,res);` |
|  25360 |  302 | `	return PH7_OK;` |
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
| 187214 |  345 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  346 |  |
|      - |  347 | `	const char *zSource,*zOfft;` |
|      - |  348 | `	int nOfft,nLen,nSrcLen;` |
| 187216 |  349 | `	if( nArg < 2 ){` |
|      - |  350 | `		/* return FALSE */` |
|      5 |  351 | `		ph7_result_bool(pCtx,0);` |
|      5 |  352 | `		return PH7_OK;` |
|      - |  353 | `	}` |
|      - |  354 | `	/* Extract the target string */` |
| 187212 |  355 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 187212 |  356 | `	if( nSrcLen < 1 ){` |
|      - |  357 | `		/* Empty string,return FALSE */` |
|  10938 |  358 | `		ph7_result_bool(pCtx,0);` |
|  10938 |  359 | `		return PH7_OK;` |
|      - |  360 | `	}` |
| 176276 |  361 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  362 | `	/* Extract the offset */` |
| 176276 |  363 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 176276 |  364 | `	if( nOfft < 0 ){` |
|  29344 |  365 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  29344 |  366 | `		if( zOfft < zSource ){` |
|      - |  367 | `			/* Invalid offset */` |
|      5 |  368 | `			ph7_result_bool(pCtx,0);` |
|      5 |  369 | `			return PH7_OK;` |
|      - |  370 | `		}` |
|  29340 |  371 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  29340 |  372 | `		nOfft = (int)(zOfft-zSource);` |
| 161603 |  373 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  374 | `		/* Invalid offset */` |
|    116 |  375 | `		ph7_result_bool(pCtx,0);` |
|    116 |  376 | `		return PH7_OK;` |
|    ! 0 |  377 | `	}else{` |
| 146820 |  378 | `		zOfft = &zSource[nOfft];` |
| 146820 |  379 | `		nLen = nSrcLen - nOfft;` |
|      - |  380 | `	}` |
| 176158 |  381 | `	if( nArg > 2 ){` |
|      - |  382 | `		/* Extract the length */` |
| 145638 |  383 | `		nLen = ph7_value_to_int(apArg[2]);` |
| 145638 |  384 | `		if( nLen == 0 ){` |
|      - |  385 | `			/* Invalid length,return an empty string */` |
|      5 |  386 | `			ph7_result_string(pCtx,"",0);` |
|      5 |  387 | `			return PH7_OK;` |
| 145634 |  388 | `		}else if( nLen < 0 ){` |
|  29334 |  389 | `			nLen = nSrcLen + nLen - nOfft;` |
|  29334 |  390 | `			if( nLen < 1 ){` |
|      - |  391 | `				/* Invalid  length */` |
|      3 |  392 | `				nLen = nSrcLen - nOfft;` |
|      1 |  393 | `			}` |
|  14666 |  394 | `		}` |
| 145634 |  395 | `		if( nLen + nOfft > nSrcLen ){` |
|      - |  396 | `			/* Invalid length */` |
|   4148 |  397 | `			nLen = nSrcLen - nOfft;` |
|   2073 |  398 | `		}` |
|  72816 |  399 | `	}` |
|      - |  400 | `	/* Return the substring */` |
| 176154 |  401 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 176154 |  402 | `	return PH7_OK;` |
|  93609 |  403 |  |
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
| 118694 | 1525 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      2 | 1526 |  |
|  59347 | 1527 | `	SXUNUSED(pKey);` |
| 118696 | 1528 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 1529 | `	const char *zData;` |
|      - | 1530 | `	int nLen;` |
| 118696 | 1531 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
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
| 118694 | 1548 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 1549 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 118694 | 1550 | `	if( pData->bFirst ){` |
|  29630 | 1551 | `		pData->bFirst = 0;` |
| 103880 | 1552 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 1553 | `		/* append the separator first */` |
|  89054 | 1554 | `		ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|  44526 | 1555 | `	}` |
|      - | 1556 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 118694 | 1557 | `	if( nLen > 0 ){` |
| 107758 | 1558 | `		ph7_result_string(pData->pCtx,zData,nLen);` |
|  53878 | 1559 | `	}` |
| 118694 | 1560 | `	return PH7_OK;` |
|  59349 | 1561 |  |
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
|  29654 | 1575 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1576 |  |
|      - | 1577 | `	struct implode_data imp_data;` |
|  29656 | 1578 | `	int i = 1;` |
|  29656 | 1579 | `	if( nArg < 1 ){` |
|      - | 1580 | `		/* Missing argument,return NULL */` |
|    ! 0 | 1581 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1582 | `		return PH7_OK;` |
|      - | 1583 | `	}` |
|      - | 1584 | `	/* Prepare the implode context */` |
|  29656 | 1585 | `	imp_data.pCtx = pCtx;` |
|  29656 | 1586 | `	imp_data.bRecursive = 0;` |
|  29656 | 1587 | `	imp_data.bFirst = 1;` |
|  29656 | 1588 | `	imp_data.nRecCount = 0;` |
|  29656 | 1589 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  29654 | 1590 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  14828 | 1591 | `	}else{` |
|      3 | 1592 | `		imp_data.zSep = 0;` |
|      3 | 1593 | `		imp_data.nSeplen = 0;` |
|      3 | 1594 | `		i = 0;` |
|      - | 1595 | `	}` |
|  29656 | 1596 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 1597 | `	/* Start the 'join' process */` |
|  59310 | 1598 | `	while( i < nArg ){` |
|  29656 | 1599 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1600 | `			/* Iterate throw array entries */` |
|  29656 | 1601 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|  14829 | 1602 | `		}else{` |
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
|  29656 | 1618 | `		i++;` |
|      2 | 1619 | `	}` |
|  29656 | 1620 | `	return PH7_OK;` |
|  14829 | 1621 |  |
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
|   5570 | 1710 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1711 |  |
|      - | 1712 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 1713 | `	int nDelim,nStrlen,iLimit;` |
|      - | 1714 | `	ph7_value *pArray;` |
|      - | 1715 | `	ph7_value *pValue;` |
|      - | 1716 | `	sxu32 nOfft;` |
|      - | 1717 | `	sxi32 rc;` |
|   5572 | 1718 | `	if( nArg < 2 ){` |
|      - | 1719 | `		/* Missing arguments,return FALSE */` |
|      9 | 1720 | `		ph7_result_bool(pCtx,0);` |
|      9 | 1721 | `		return PH7_OK;` |
|      - | 1722 | `	}` |
|      - | 1723 | `	/* Extract the delimiter */` |
|   5564 | 1724 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   5564 | 1725 | `	if( nDelim < 1 ){` |
|      - | 1726 | `		/* Empty delimiter,return FALSE */` |
|      3 | 1727 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1728 | `		return PH7_OK;` |
|      - | 1729 | `	}` |
|      - | 1730 | `	/* Extract the string */` |
|   5562 | 1731 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   5562 | 1732 | `	if( nStrlen < 1 ){` |
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
|   5560 | 1747 | `	zEnd = &zString[nStrlen];` |
|      - | 1748 | `	/* Create the array */` |
|   5560 | 1749 | `	pArray =  ph7_context_new_array(pCtx);` |
|   5560 | 1750 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   5560 | 1751 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 1752 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 1753 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1754 | `		return PH7_OK;` |
|      - | 1755 | `	}` |
|      - | 1756 | `	/* Set a defualt limit */` |
|   5560 | 1757 | `	iLimit = SXI32_HIGH;` |
|   5560 | 1758 | `	if( nArg > 2 ){` |
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
|  63641 | 1769 | `	for(;;){` |
| 127284 | 1770 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 127284 | 1771 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 1772 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   5560 | 1773 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   5560 | 1774 | `			ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|   5560 | 1775 | `			break;` |
|      - | 1776 | `		}` |
|      - | 1777 | `		/* Point to the desired offset */` |
| 121726 | 1778 | `		zCur = &zString[nOfft];` |
|      - | 1779 | `		/* Perform the store operation (may be empty) */` |
| 121726 | 1780 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 121726 | 1781 | `		ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|      - | 1782 | `		/* Point beyond the delimiter */` |
| 121726 | 1783 | `		zString = &zCur[nDelim];` |
|      - | 1784 | `		/* Reset the cursor */` |
| 121726 | 1785 | `		ph7_value_reset_string_cursor(pValue);` |
|      2 | 1786 | `	}` |
|      - | 1787 | `	/* Return the freshly created array */` |
|   5560 | 1788 | `	ph7_result_value(pCtx,pArray);` |
|      - | 1789 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 1790 | `	 * released as soon we return from this foregin function.` |
|      - | 1791 | `	 */` |
|   5560 | 1792 | `	return PH7_OK;` |
|   2787 | 1793 |  |
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
|  12794 | 1809 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1810 |  |
|      - | 1811 | `	const char *zString;` |
|      - | 1812 | `	int nLen;` |
|  12796 | 1813 | `	if( nArg < 1 ){` |
|      - | 1814 | `		/* Missing arguments,return null */` |
|      3 | 1815 | `		ph7_result_null(pCtx);` |
|      3 | 1816 | `		return PH7_OK;` |
|      - | 1817 | `	}` |
|      - | 1818 | `	/* Extract the target string */` |
|  12794 | 1819 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  12794 | 1820 | `	if( nLen < 1 ){` |
|      - | 1821 | `		/* Empty string,return */` |
|   1670 | 1822 | `		ph7_result_string(pCtx,"",0);` |
|   1670 | 1823 | `		return PH7_OK;` |
|      - | 1824 | `	}` |
|      - | 1825 | `	/* Start the trim process */` |
|  11126 | 1826 | `	if( nArg < 2 ){` |
|      - | 1827 | `		SyString sStr;` |
|      - | 1828 | `		/* Remove white spaces and NUL bytes */` |
|  11122 | 1829 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  27106 | 1830 | `		SyStringFullTrimSafe(&sStr);` |
|  11122 | 1831 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   5562 | 1832 | `	}else{` |
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
|  11126 | 1886 | `	return PH7_OK;` |
|   6399 | 1887 |  |
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
|  29334 | 2051 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2052 |  |
|      - | 2053 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2054 | `	int nLen;` |
|  29336 | 2055 | `	if( nArg < 1 ){` |
|      - | 2056 | `		/* Missing arguments,return null */` |
|      3 | 2057 | `		ph7_result_null(pCtx);` |
|      3 | 2058 | `		return PH7_OK;` |
|      - | 2059 | `	}` |
|      - | 2060 | `	/* Extract the target string */` |
|  29334 | 2061 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  29334 | 2062 | `	if( nLen < 1 ){` |
|      - | 2063 | `		/* Empty string,return */` |
|      3 | 2064 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2065 | `		return PH7_OK;` |
|      - | 2066 | `	}` |
|      - | 2067 | `	/* Perform the requested operation */` |
|  29332 | 2068 | `	zEnd = &zString[nLen];` |
|  92450 | 2069 | `	for(;;){` |
| 184902 | 2070 | `		if( zString >= zEnd ){` |
|      - | 2071 | `			/* No more input,break immediately */` |
|  29332 | 2072 | `			break;` |
|      - | 2073 | `		}` |
| 155572 | 2074 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2075 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2076 | `			zCur = zString;` |
|    ! 0 | 2077 | `			zString++;` |
|    ! 0 | 2078 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2079 | `				zString++;` |
|    ! 0 | 2080 | `			}` |
|      - | 2081 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2082 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2083 | `		}else{` |
| 155572 | 2084 | `			int c = zString[0];` |
| 155572 | 2085 | `			if( SyisUpper(c) ){` |
| 155570 | 2086 | `				c = SyToLower(zString[0]);` |
|  77784 | 2087 | `			}` |
|      - | 2088 | `			/* Append character */` |
| 155572 | 2089 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2090 | `			/* Advance the cursor */` |
| 155572 | 2091 | `			zString++;` |
|      - | 2092 | `		}` |
|      2 | 2093 | `	}` |
|  29332 | 2094 | `	return PH7_OK;` |
|  14669 | 2095 |  |
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
|      - | 5108 | `/* SPDX-SnippetBegin */` |
|      - | 5109 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 5110 | `/* SPDX-License-Identifier: blessing */` |
|      - | 5111 | `/*` |
|      - | 5112 | ` * string soundex(string $str)` |
|      - | 5113 | ` *  Calculate the soundex key of a string.` |
|      - | 5114 | ` * Parameters` |
|      - | 5115 | ` *  $str` |
|      - | 5116 | ` *   The input string.` |
|      - | 5117 | ` * Return` |
|      - | 5118 | ` *  Returns the soundex key as a string.` |
|      - | 5119 | ` * Note:` |
|      - | 5120 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 5121 | ` * source tree.` |
|      - | 5122 | ` */` |
|     20 | 5123 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5124 |  |
|      - | 5125 | `	const unsigned char *zIn;` |
|      - | 5126 | `	char zResult[8];` |
|      - | 5127 | `	int i, j;` |
|      - | 5128 | `	static const unsigned char iCode[] = {` |
|      - | 5129 |  |
|      - | 5130 |  |
|      - | 5131 |  |
|      - | 5132 |  |
|      - | 5133 |  |
|      - | 5134 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5135 |  |
|      - | 5136 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5137 | `	};` |
|     21 | 5138 | `	if( nArg < 1 ){` |
|      - | 5139 | `		/* Missing arguments,return the empty string */` |
|      3 | 5140 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5141 | `		return PH7_OK;` |
|      - | 5142 | `	}` |
|     19 | 5143 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     19 | 5144 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     19 | 5145 | `	if( zIn[i] ){` |
|     17 | 5146 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 5147 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 5148 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 5149 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 5150 | `			if( code>0 ){` |
|     45 | 5151 | `				if( code!=prevcode ){` |
|     33 | 5152 | `					prevcode = (unsigned char)code;` |
|     33 | 5153 | `					zResult[j++] = (char)code + '0';` |
|     16 | 5154 | `				}` |
|     23 | 5155 | `			}else{` |
|     49 | 5156 | `				prevcode = 0;` |
|      - | 5157 | `			}` |
|     47 | 5158 | `		}` |
|     33 | 5159 | `		while( j<4 ){` |
|     17 | 5160 | `			zResult[j++] = '0';` |
|      1 | 5161 | `		}` |
|     17 | 5162 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 5163 | `	}else{` |
|      3 | 5164 | `	  ph7_result_string(pCtx,"?000",4);` |
|      - | 5165 | `	}` |
|     19 | 5166 | `	return PH7_OK;` |
|     11 | 5167 |  |
|      - | 5168 | `/* SPDX-SnippetEnd */` |
|      - | 5169 | `/*` |
|      - | 5170 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 5171 | ` *  Wraps a string to a given number of characters.` |
|      - | 5172 | ` * Parameters` |
|      - | 5173 | ` *  $str` |
|      - | 5174 | ` *   The input string.` |
|      - | 5175 | ` * $width` |
|      - | 5176 | ` *  The column width.` |
|      - | 5177 | ` * $break` |
|      - | 5178 | ` *  The line is broken using the optional break parameter.` |
|      - | 5179 | ` * Return` |
|      - | 5180 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 5181 | ` */` |
|     14 | 5182 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5183 |  |
|      - | 5184 | `	const char *zIn,*zEnd,*zBreak;` |
|      - | 5185 | `	int iLen,iBreaklen,iChunk;` |
|     15 | 5186 | `	if( nArg < 1 ){` |
|      - | 5187 | `		/* Missing arguments,return the empty string */` |
|      3 | 5188 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5189 | `		return PH7_OK;` |
|      - | 5190 | `	}` |
|      - | 5191 | `	/* Extract the input string */` |
|     13 | 5192 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|     13 | 5193 | `	if( iLen < 1 ){` |
|      - | 5194 | `		/* Nothing to process,return the empty string */` |
|      3 | 5195 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5196 | `		return PH7_OK;` |
|      - | 5197 | `	}` |
|      - | 5198 | `	/* Chunk length */` |
|     11 | 5199 | `	iChunk = 75;` |
|     11 | 5200 | `	iBreaklen = 0;` |
|     11 | 5201 | `	zBreak = ""; /* cc warning */` |
|     11 | 5202 | `	if( nArg > 1 ){` |
|     11 | 5203 | `		iChunk = ph7_value_to_int(apArg[1]);` |
|     11 | 5204 | `		if( iChunk < 1 ){` |
|    ! 0 | 5205 | `			iChunk = 75;` |
|    ! 0 | 5206 | `		}` |
|     11 | 5207 | `		if( nArg > 2 ){` |
|      3 | 5208 | `			zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      1 | 5209 | `		}` |
|      5 | 5210 | `	}` |
|     11 | 5211 | `	if( iBreaklen < 1 ){` |
|      - | 5212 | `		/* Set a default column break */` |
|      - | 5213 | `#ifdef __WINNT__` |
|      1 | 5214 | `		zBreak = "\r\n";` |
|      1 | 5215 | `		iBreaklen = (int)sizeof("\r\n")-1;` |
|      - | 5216 | `#else` |
|      8 | 5217 | `		zBreak = "\n";` |
|      8 | 5218 | `		iBreaklen = (int)sizeof(char);` |
|      - | 5219 | `#endif` |
|      4 | 5220 | `	}` |
|      - | 5221 | `	/* Perform the requested operation */` |
|     11 | 5222 | `	zEnd = &zIn[iLen];` |
|     41 | 5223 | `	for(;;){` |
|      - | 5224 | `		int nMax;` |
|     47 | 5225 | `		if( zIn >= zEnd ){` |
|      - | 5226 | `			/* No more input to process */` |
|     11 | 5227 | `			break;` |
|      - | 5228 | `		}` |
|     37 | 5229 | `		nMax = (int)(zEnd-zIn);` |
|     37 | 5230 | `		if( iChunk > nMax ){` |
|     11 | 5231 | `			iChunk = nMax;` |
|      5 | 5232 | `		}` |
|      - | 5233 | `		/* Append the column first */` |
|     37 | 5234 | `		ph7_result_string(pCtx,zIn,iChunk); /* Will make it's own copy */` |
|      - | 5235 | `		/* Advance the cursor */` |
|     37 | 5236 | `		zIn += iChunk;` |
|     37 | 5237 | `		if( zIn < zEnd ){` |
|      - | 5238 | `			/* Append the line break */` |
|     27 | 5239 | `			ph7_result_string(pCtx,zBreak,iBreaklen);` |
|     13 | 5240 | `		}` |
|      1 | 5241 | `	}` |
|     11 | 5242 | `	return PH7_OK;` |
|      8 | 5243 |  |
|      - | 5244 | `/*` |
|      - | 5245 | ` * Check if the given character is a member of the given mask.` |
|      - | 5246 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 5247 | ` * Refer to [strtok()].` |
|      - | 5248 | ` */` |
|     30 | 5249 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 5250 |  |
|      - | 5251 | `	int i;` |
|     57 | 5252 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 5253 | `		if( c == zMask[i] ){` |
|     13 | 5254 | `			if( pOfft ){` |
|      5 | 5255 | `				*pOfft = i;` |
|      2 | 5256 | `			}` |
|     13 | 5257 | `			return TRUE;` |
|      - | 5258 | `		}` |
|     14 | 5259 | `	}` |
|     19 | 5260 | `	return FALSE;` |
|     16 | 5261 |  |
|      - | 5262 | `/*` |
|      - | 5263 | ` * Extract a single token from the input stream.` |
|      - | 5264 | ` * Refer to [strtok()].` |
|      - | 5265 | ` */` |
|      6 | 5266 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 5267 |  |
|      7 | 5268 | `	const char *zIn = *pzIn;` |
|      - | 5269 | `	const char *zPtr;` |
|      - | 5270 | `	/* Ignore leading delimiter */` |
|     11 | 5271 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5272 | `		zIn++;` |
|      1 | 5273 | `	}` |
|      7 | 5274 | `	if( zIn >= zEnd ){` |
|      - | 5275 | `		/* End of input */` |
|    ! 0 | 5276 | `		return SXERR_EOF;` |
|      - | 5277 | `	}` |
|      7 | 5278 | `	zPtr = zIn;` |
|      - | 5279 | `	/* Extract the token */` |
|     13 | 5280 | `	while( zIn < zEnd ){` |
|     11 | 5281 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 5282 | `			/* UTF-8 stream */` |
|    ! 0 | 5283 | `			zIn++;` |
|    ! 0 | 5284 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 5285 | `		}else{` |
|     11 | 5286 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5287 | `				break;` |
|      - | 5288 | `			}` |
|      7 | 5289 | `			zIn++;` |
|      - | 5290 | `		}` |
|      1 | 5291 | `	}` |
|      7 | 5292 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5293 | `	/* Update the cursor */` |
|      7 | 5294 | `	*pzIn = zIn;` |
|      - | 5295 | `	/* Return to the caller */` |
|      7 | 5296 | `	return SXRET_OK;` |
|      4 | 5297 |  |
|      - | 5298 | `/* strtok auxiliary private data */` |
|      - | 5299 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 5300 | `struct strtok_aux_data` |
|      - | 5301 |  |
|      - | 5302 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 5303 | `	const char *zIn;   /* Current input stream */` |
|      - | 5304 | `	const char *zEnd;  /* End of input */` |
|      - | 5305 | `};` |
|      - | 5306 | `/*` |
|      - | 5307 | ` * string strtok(string $str,string $token)` |
|      - | 5308 | ` * string strtok(string $token)` |
|      - | 5309 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 5310 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 5311 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 5312 | ` *  words by using the space character as the token.` |
|      - | 5313 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 5314 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 5315 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 5316 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 5317 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 5318 | ` *  the argument are found.` |
|      - | 5319 | ` * Parameters` |
|      - | 5320 | ` *  $str` |
|      - | 5321 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 5322 | ` * $token` |
|      - | 5323 | ` *  The delimiter used when splitting up str.` |
|      - | 5324 | ` * Return` |
|      - | 5325 | ` *   Current token or FALSE on EOF.` |
|      - | 5326 | ` */` |
|      8 | 5327 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5328 |  |
|      - | 5329 | `	strtok_aux_data *pAux;` |
|      - | 5330 | `	const char *zMask;` |
|      - | 5331 | `	SyString sToken;` |
|      - | 5332 | `	int nMasklen;` |
|      - | 5333 | `	sxi32 rc;` |
|      9 | 5334 | `	if( nArg < 2 ){` |
|      - | 5335 | `		/* Extract top aux data */` |
|      7 | 5336 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      7 | 5337 | `		if( pAux == 0 ){` |
|      - | 5338 | `			/* No aux data,return FALSE */` |
|    ! 0 | 5339 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5340 | `			return PH7_OK;` |
|      - | 5341 | `		}` |
|      7 | 5342 | `		nMasklen = 0;` |
|      7 | 5343 | `		zMask = ""; /* cc warning */` |
|      7 | 5344 | `		if( nArg > 0 ){` |
|      - | 5345 | `			/* Extract the mask */` |
|      5 | 5346 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 5347 | `		}` |
|      7 | 5348 | `		if( nMasklen < 1 ){` |
|      - | 5349 | `			/* Invalid mask,return FALSE */` |
|      3 | 5350 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|      3 | 5351 | `			ph7_context_free_chunk(pCtx,pAux);` |
|      3 | 5352 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|      3 | 5353 | `			ph7_result_bool(pCtx,0);` |
|      3 | 5354 | `			return PH7_OK;` |
|      - | 5355 | `		}` |
|      - | 5356 | `		/* Extract the token */` |
|      5 | 5357 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 5358 | `		if( rc != SXRET_OK ){` |
|      - | 5359 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 5360 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 5361 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5362 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 5363 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5364 | `		}else{` |
|      - | 5365 | `			/* Return the extracted token */` |
|      5 | 5366 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 5367 | `		}` |
|      3 | 5368 | `	}else{` |
|      - | 5369 | `		const char *zInput,*zCur;` |
|      - | 5370 | `		char *zDup;` |
|      - | 5371 | `		int nLen;` |
|      - | 5372 | `		/* Extract the raw input */` |
|      3 | 5373 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5374 | `		if( nLen < 1 ){` |
|      - | 5375 | `			/* Empty input,return FALSE */` |
|    ! 0 | 5376 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5377 | `			return PH7_OK;` |
|      - | 5378 | `		}` |
|      - | 5379 | `		/* Extract the mask */` |
|      3 | 5380 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 5381 | `		if( nMasklen < 1 ){` |
|      - | 5382 | `			/* Set a default mask */` |
|      - | 5383 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 5384 | `			zMask = TOK_MASK;` |
|    ! 0 | 5385 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 5386 | `#undef TOK_MASK` |
|    ! 0 | 5387 | `		}` |
|      - | 5388 | `		/* Extract a single token */` |
|      3 | 5389 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 5390 | `		if( rc != SXRET_OK ){` |
|      - | 5391 | `			/* Empty input */` |
|    ! 0 | 5392 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5393 | `			return PH7_OK;` |
|    ! 0 | 5394 | `		}else{` |
|      - | 5395 | `			/* Return the extracted token */` |
|      3 | 5396 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 5397 | `		}` |
|      - | 5398 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 5399 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 5400 | `		if( pAux ){` |
|      3 | 5401 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 5402 | `			if( nLen < 1 ){` |
|    ! 0 | 5403 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5404 | `				return PH7_OK;` |
|      - | 5405 | `			}` |
|      - | 5406 | `			/* Duplicate input */` |
|      3 | 5407 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 5408 | `			if( zDup  ){` |
|      3 | 5409 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 5410 | `				/* Register the aux data */` |
|      3 | 5411 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 5412 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 5413 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 5414 | `			}` |
|      1 | 5415 | `		}` |
|      - | 5416 | `	}` |
|      7 | 5417 | `	return PH7_OK;` |
|      5 | 5418 |  |
|      - | 5419 | `/*` |
|      - | 5420 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 5421 | ` *  Pad a string to a certain length with another string` |
|      - | 5422 | ` * Parameters` |
|      - | 5423 | ` *  $input` |
|      - | 5424 | ` *   The input string.` |
|      - | 5425 | ` * $pad_length` |
|      - | 5426 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 5427 | ` *   string, no padding takes place.` |
|      - | 5428 | ` * $pad_string` |
|      - | 5429 | ` *   Note:` |
|      - | 5430 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 5431 | ` *    divided by the pad_string's length.` |
|      - | 5432 | ` * $pad_type` |
|      - | 5433 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 5434 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 5435 | ` * Return` |
|      - | 5436 | ` *  The padded string.` |
|      - | 5437 | ` */` |
|     10 | 5438 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5439 |  |
|      - | 5440 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 5441 | `	const char *zIn,*zPad;` |
|     11 | 5442 | `	if( nArg < 2 ){` |
|      - | 5443 | `		/* Missing arguments,return the empty string */` |
|      5 | 5444 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 5445 | `		return PH7_OK;` |
|      - | 5446 | `	}` |
|      - | 5447 | `	/* Extract the target string */` |
|      7 | 5448 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5449 | `	/* Padding length */` |
|      7 | 5450 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|      7 | 5451 | `	if( iPadlen > 0 ){` |
|      5 | 5452 | `		iPadlen -= iLen;` |
|      2 | 5453 | `	}` |
|      7 | 5454 | `	if( iPadlen < 1  ){` |
|      - | 5455 | `		/* Return the string verbatim */` |
|      3 | 5456 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      3 | 5457 | `		return PH7_OK;` |
|      - | 5458 | `	}` |
|      5 | 5459 | `	zPad = " "; /* Whitespace padding */` |
|      5 | 5460 | `	iStrpad = (int)sizeof(char);` |
|      5 | 5461 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      5 | 5462 | `	if( nArg > 2 ){` |
|      - | 5463 | `		/* Padding string */` |
|      5 | 5464 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      5 | 5465 | `		if( iStrpad < 1 ){` |
|      - | 5466 | `			/* Empty string */` |
|    ! 0 | 5467 | `			zPad = " "; /* Whitespace padding */` |
|    ! 0 | 5468 | `			iStrpad = (int)sizeof(char);` |
|    ! 0 | 5469 | `		}` |
|      5 | 5470 | `		if( nArg > 3 ){` |
|      - | 5471 | `			/* Padd type */` |
|      5 | 5472 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 5473 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 5474 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 5475 | `			}` |
|      2 | 5476 | `		}` |
|      2 | 5477 | `	}` |
|      5 | 5478 | `	iDiv = 1;` |
|      5 | 5479 | `	if( iType == 2 ){` |
|    ! 0 | 5480 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 5481 | `	}` |
|      - | 5482 | `	/* Perform the requested operation */` |
|      5 | 5483 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 5484 | `		jPad = iStrpad;` |
|      5 | 5485 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 5486 | `			/* Padding */` |
|      5 | 5487 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 5488 | `				break;` |
|      - | 5489 | `			}` |
|      3 | 5490 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      2 | 5491 | `		}` |
|      3 | 5492 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 5493 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 5494 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 5495 | `				if( jPad > iStrpad ){` |
|    ! 0 | 5496 | `					jPad = iStrpad;` |
|    ! 0 | 5497 | `				}` |
|      3 | 5498 | `				if( jPad < 1){` |
|    ! 0 | 5499 | `					break;` |
|      - | 5500 | `				}` |
|      3 | 5501 | `				ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 5502 | `			}` |
|      1 | 5503 | `		}` |
|      1 | 5504 | `	}` |
|      5 | 5505 | `	if( iLen > 0 ){` |
|      - | 5506 | `		/* Append the input string */` |
|      5 | 5507 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      2 | 5508 | `	}` |
|      5 | 5509 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 5510 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 5511 | `			/* Padding */` |
|      5 | 5512 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 5513 | `				break;` |
|      - | 5514 | `			}` |
|      3 | 5515 | `			ph7_result_string(pCtx,zPad,iStrpad);` |
|      2 | 5516 | `		}` |
|      5 | 5517 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 5518 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 5519 | `			if( jPad > iStrpad ){` |
|    ! 0 | 5520 | `				jPad = iStrpad;` |
|    ! 0 | 5521 | `			}` |
|      3 | 5522 | `			if( jPad < 1){` |
|    ! 0 | 5523 | `				break;` |
|      - | 5524 | `			}` |
|      3 | 5525 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 5526 | `		}` |
|      1 | 5527 | `	}` |
|      5 | 5528 | `	return PH7_OK;` |
|      6 | 5529 |  |
|      - | 5530 | `/*` |
|      - | 5531 | ` * String replacement private data.` |
|      - | 5532 | ` */` |
|      - | 5533 | `typedef struct str_replace_data str_replace_data;` |
|      - | 5534 | `struct str_replace_data` |
|      - | 5535 |  |
|      - | 5536 | `	/* The following two fields are only used by the strtr function */` |
|      - | 5537 | `	SyBlob *pWorker;         /* Working buffer */` |
|      - | 5538 | `	ProcStringMatch xMatch;  /* Pattern match routine */` |
|      - | 5539 | `	/* The following two fields are only used by the str_replace function */` |
|      - | 5540 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 5541 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 5542 | `};` |
|      - | 5543 | `/*` |
|      - | 5544 | ` * Remove a substring.` |
|      - | 5545 | ` */` |
|      - | 5546 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 5547 | `	for(;;){\` |
|      - | 5548 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 5549 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 5550 | `		++OFFT;\` |
|      - | 5551 | `	}\` |
|      - | 5552 |  |
|      - | 5553 | `/*` |
|      - | 5554 | ` * Shift right and insert algorithm.` |
|      - | 5555 | ` */` |
|      - | 5556 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 5557 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 5558 | `		for(;;){\` |
|      - | 5559 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 5560 | `			if(INLEN < 1 ) { break; }\` |
|      - | 5561 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 5562 | `			--INLEN; \` |
|      - | 5563 | `		}\` |
|      - | 5564 | `		for(;;){\` |
|      - | 5565 | `				if(ELEN < 1) { break; }\` |
|      - | 5566 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 5567 | `				OFFT++;\` |
|      - | 5568 | `				ENTRY++;\` |
|      - | 5569 | `				--ELEN;\` |
|      - | 5570 | `		}\` |
|      - | 5571 |  |
|      - | 5572 | `/*` |
|      - | 5573 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 5574 | ` * replacement string [i.e: zReplace].` |
|      - | 5575 | ` */` |
|     38 | 5576 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 5577 |  |
|     39 | 5578 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 5579 | `	sxu32 n,m;` |
|     39 | 5580 | `	n = SyBlobLength(pWorker);` |
|     39 | 5581 | `	m = nOfft;` |
|      - | 5582 | `	/* Delete the old entry */` |
|    475 | 5583 | `	STRDEL(zInput,n,m,nLen);` |
|     39 | 5584 | `	SyBlobLength(pWorker) -= nLen;` |
|     39 | 5585 | `	if( nReplen > 0 ){` |
|     33 | 5586 | `		sxi32 iRep = nReplen;` |
|      - | 5587 | `		sxi32 rc;` |
|      - | 5588 | `		/*` |
|      - | 5589 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 5590 | `		 * string.` |
|      - | 5591 | `		 */` |
|     33 | 5592 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     33 | 5593 | `		if( rc != SXRET_OK ){` |
|      - | 5594 | `			/* Simply ignore any memory failure problem */` |
|    ! 0 | 5595 | `			return SXRET_OK;` |
|      - | 5596 | `		}` |
|      - | 5597 | `		/* Perform the insertion now */` |
|     33 | 5598 | `		zInput = (char *)SyBlobData(pWorker);` |
|     33 | 5599 | `		n = SyBlobLength(pWorker);` |
|    163 | 5600 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     33 | 5601 | `		SyBlobLength(pWorker) += nReplen;` |
|     16 | 5602 | `	}` |
|     39 | 5603 | `	return SXRET_OK;` |
|     20 | 5604 |  |
|      - | 5605 | `/*` |
|      - | 5606 | ` * String replacement walker callback.` |
|      - | 5607 | ` * The following callback is invoked for each array entry that hold` |
|      - | 5608 | ` * the replace string.` |
|      - | 5609 | ` * Refer to the strtr() implementation for more information.` |
|      - | 5610 | ` */` |
|      8 | 5611 | `static int StringReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 5612 |  |
|      9 | 5613 | `	str_replace_data *pRepData = (str_replace_data *)pUserData;` |
|      - | 5614 | `	const char *zTarget,*zReplace;` |
|      - | 5615 | `	SyBlob *pWorker;` |
|      - | 5616 | `	int tLen,nLen;` |
|      - | 5617 | `	sxu32 nOfft;` |
|      - | 5618 | `	sxi32 rc;` |
|      - | 5619 | `	/* Point to the working buffer */` |
|      9 | 5620 | `	pWorker = pRepData->pWorker;` |
|      9 | 5621 | `	if( !ph7_value_is_string(pKey) ){` |
|      - | 5622 | `		/* Target and replace must be a string */` |
|      3 | 5623 | `		return PH7_OK;` |
|      - | 5624 | `	}` |
|      - | 5625 | `	/* Extract the target and the replace */` |
|      7 | 5626 | `	zTarget = ph7_value_to_string(pKey,&tLen);` |
|      7 | 5627 | `	if( tLen < 1 ){` |
|      - | 5628 | `		/* Empty target,return immediately */` |
|    ! 0 | 5629 | `		return PH7_OK;` |
|      - | 5630 | `	}` |
|      - | 5631 | `	/* Perform a pattern search */` |
|      7 | 5632 | `	rc = pRepData->xMatch(SyBlobData(pWorker),SyBlobLength(pWorker),(const void *)zTarget,(sxu32)tLen,&nOfft);` |
|      7 | 5633 | `	if( rc != SXRET_OK ){` |
|      - | 5634 | `		/* Pattern not found */` |
|    ! 0 | 5635 | `		return PH7_OK;` |
|      - | 5636 | `	}` |
|      - | 5637 | `	/* Extract the replace string */` |
|      7 | 5638 | `	zReplace = ph7_value_to_string(pData,&nLen);` |
|      - | 5639 | `	/* Perform the replace process */` |
|      7 | 5640 | `	StringReplace(pWorker,nOfft,tLen,zReplace,nLen);` |
|      - | 5641 | `	/* All done */` |
|      7 | 5642 | `	return PH7_OK;` |
|      5 | 5643 |  |
|      - | 5644 | `/*` |
|      - | 5645 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 5646 | ` * to collect search/replace string.` |
|      - | 5647 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 5648 | ` */` |
|     26 | 5649 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 5650 |  |
|     27 | 5651 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 5652 | `	SyString sWorker;` |
|      - | 5653 | `	const char *zIn;` |
|      - | 5654 | `	int nByte;` |
|      - | 5655 | `	/* Extract a string representation of the given argument */` |
|     27 | 5656 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 5657 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 5658 | `	if( nByte > 0 ){` |
|      - | 5659 | `		char *zDup;` |
|      - | 5660 | `		/* Duplicate the chunk */` |
|     25 | 5661 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 5662 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 5663 | `			);` |
|     25 | 5664 | `		if( zDup == 0 ){` |
|      - | 5665 | `			/* Ignore any memory failure problem */` |
|    ! 0 | 5666 | `			ph7_context_throw_error(pRep->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 5667 | `			return PH7_OK;` |
|      - | 5668 | `		}` |
|     25 | 5669 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 5670 | `		/* Save the chunk */` |
|     25 | 5671 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 5672 | `	}` |
|      - | 5673 | `	/* Save for later processing */` |
|     27 | 5674 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 5675 | `	/* All done */` |
|     13 | 5676 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 5677 | `	return PH7_OK;` |
|     14 | 5678 |  |
|      - | 5679 | `/*` |
|      - | 5680 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 5681 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 5682 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 5683 | ` * Parameters` |
|      - | 5684 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 5685 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 5686 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 5687 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 5688 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 5689 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 5690 | ` * $search` |
|      - | 5691 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 5692 | ` *  to designate multiple needles.` |
|      - | 5693 | ` * $replace` |
|      - | 5694 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 5695 | ` *  to designate multiple replacements.` |
|      - | 5696 | ` * $subject` |
|      - | 5697 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 5698 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 5699 | ` *  of subject, and the return value is an array as well.` |
|      - | 5700 | ` * $count (Not used)` |
|      - | 5701 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 5702 | ` * Return` |
|      - | 5703 | ` * This function returns a string or an array with the replaced values.` |
|      - | 5704 | ` */` |
|  22114 | 5705 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5706 |  |
|      - | 5707 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 5708 | `	ProcStringMatch xMatch;` |
|      - | 5709 | `	const char *zIn,*zFunc;` |
|      - | 5710 | `	str_replace_data sRep;` |
|      - | 5711 | `	SyBlob sWorker;` |
|      - | 5712 | `	SySet sReplace;` |
|      - | 5713 | `	SySet sSearch;` |
|      - | 5714 | `	int rep_str;` |
|      - | 5715 | `	int nByte;` |
|      - | 5716 | `	sxi32 rc;` |
|  22116 | 5717 | `	if( nArg < 3 ){` |
|      - | 5718 | `		/* Missing/Invalid arguments,return null */` |
|      7 | 5719 | `		ph7_result_null(pCtx);` |
|      7 | 5720 | `		return PH7_OK;` |
|      - | 5721 | `	}` |
|      - | 5722 | `	/* Initialize fields */` |
|  22110 | 5723 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  22110 | 5724 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  22110 | 5725 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  22110 | 5726 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  22110 | 5727 | `	sRep.pCtx = pCtx;` |
|  22110 | 5728 | `	sRep.pCollector = &sSearch;` |
|  22110 | 5729 | `	rep_str = 0;` |
|      - | 5730 | `	/* Extract the subject */` |
|  22110 | 5731 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  22110 | 5732 | `	if( nByte < 1 ){` |
|      - | 5733 | `		/* Nothing to replace,return the empty string */` |
|     29 | 5734 | `		ph7_result_string(pCtx,"",0);` |
|     29 | 5735 | `		return PH7_OK;` |
|      - | 5736 | `	}` |
|      - | 5737 | `	/* Copy the subject */` |
|  22082 | 5738 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 5739 | `	/* Search string */` |
|  22082 | 5740 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 5741 | `		/* Collect search string */` |
|      9 | 5742 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 5743 | `	}else{` |
|      - | 5744 | `		/* Single pattern */` |
|  22074 | 5745 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  22074 | 5746 | `		if( nByte < 1 ){` |
|      - | 5747 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 5748 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 5749 | `			return PH7_OK;` |
|      - | 5750 | `		}` |
|  22070 | 5751 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 5752 | `		/* Save for later processing */` |
|  22070 | 5753 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 5754 | `	}` |
|      - | 5755 | `	/* Replace string */` |
|  22078 | 5756 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 5757 | `		/* Collect replace string */` |
|      7 | 5758 | `		sRep.pCollector = &sReplace;` |
|      7 | 5759 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 5760 | `	}else{` |
|      - | 5761 | `		/* Single needle */` |
|  22072 | 5762 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  22072 | 5763 | `		rep_str = 1;` |
|  22072 | 5764 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 5765 | `		/* Save for later processing */` |
|  22072 | 5766 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 5767 | `	}` |
|      - | 5768 | `	/* Reset loop cursors */` |
|  22078 | 5769 | `	SySetResetCursor(&sSearch);` |
|  22078 | 5770 | `	SySetResetCursor(&sReplace);` |
|  22078 | 5771 | `	pReplace = pSearch = 0; /* cc warning */` |
|  22078 | 5772 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 5773 | `	/* Extract function name */` |
|  22078 | 5774 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 5775 | `	/* Set the default pattern match routine */` |
|  22078 | 5776 | `	xMatch = SyBlobSearch;` |
|  22078 | 5777 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 5778 | `		/* Case insensitive pattern match */` |
|     11 | 5779 | `		xMatch = iPatternMatch;` |
|      5 | 5780 | `	}` |
|      - | 5781 | `	/* Start the replace process */` |
|  44162 | 5782 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 5783 | `		sxu32 nCount,nOfft;` |
|  22086 | 5784 | `		if( pSearch->nByte <  1 ){` |
|      - | 5785 | `			/* Empty string,ignore */` |
|      3 | 5786 | `			continue;` |
|      - | 5787 | `		}` |
|      - | 5788 | `		/* Extract the replace string */` |
|  22084 | 5789 | `		if( rep_str ){` |
|  22074 | 5790 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  11038 | 5791 | `		}else{` |
|     11 | 5792 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 5793 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 5794 | `				 * An empty string is used for the rest of replacement values` |
|      - | 5795 | `				 */` |
|      3 | 5796 | `				pReplace = 0;` |
|      1 | 5797 | `			}` |
|      - | 5798 | `		}` |
|  22084 | 5799 | `		if( pReplace == 0 ){` |
|      - | 5800 | `			/* Use an empty string instead */` |
|      3 | 5801 | `			pReplace = &sTemp;` |
|      1 | 5802 | `		}` |
|  22084 | 5803 | `		nOfft = nCount = 0;` |
|  11057 | 5804 | `		for(;;){` |
|  22116 | 5805 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 5806 | `				break;` |
|      - | 5807 | `			}` |
|      - | 5808 | `			/* Perform a pattern lookup */` |
|  33155 | 5809 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  22102 | 5810 | `				pSearch->nByte,&nOfft);` |
|  22104 | 5811 | `			if( rc != SXRET_OK ){` |
|      - | 5812 | `				/* Pattern not found */` |
|  22072 | 5813 | `				break;` |
|      - | 5814 | `			}` |
|      - | 5815 | `			/* Perform the replace operation */` |
|     33 | 5816 | `			StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|      - | 5817 | `			/* Increment offset counter */` |
|     33 | 5818 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 5819 | `		}` |
|      2 | 5820 | `	}` |
|      - | 5821 | `	/* All done,clean-up the mess left behind */` |
|  22078 | 5822 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  22078 | 5823 | `	SySetRelease(&sSearch);` |
|  22078 | 5824 | `	SySetRelease(&sReplace);` |
|  22078 | 5825 | `	SyBlobRelease(&sWorker);` |
|  22078 | 5826 | `	return PH7_OK;` |
|  11059 | 5827 |  |
|      - | 5828 | `/*` |
|      - | 5829 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 5830 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 5831 | ` *  Translate characters or replace substrings.` |
|      - | 5832 | ` * Parameters` |
|      - | 5833 | ` *  $str` |
|      - | 5834 | ` *  The string being translated.` |
|      - | 5835 | ` * $from` |
|      - | 5836 | ` *  The string being translated to to.` |
|      - | 5837 | ` * $to` |
|      - | 5838 | ` *  The string replacing from.` |
|      - | 5839 | ` * $replace_pairs` |
|      - | 5840 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 5841 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 5842 | ` * Return` |
|      - | 5843 | ` *  The translated string.` |
|      - | 5844 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 5845 | ` */` |
|     12 | 5846 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5847 |  |
|      - | 5848 | `	const char *zIn;` |
|      - | 5849 | `	int nLen;` |
|     13 | 5850 | `	if( nArg < 1 ){` |
|      - | 5851 | `		/* Nothing to replace,return FALSE */` |
|      7 | 5852 | `		ph7_result_bool(pCtx,0);` |
|      7 | 5853 | `		return PH7_OK;` |
|      - | 5854 | `	}` |
|      7 | 5855 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 5856 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 5857 | `		/* Invalid arguments */` |
|    ! 0 | 5858 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 5859 | `		return PH7_OK;` |
|      - | 5860 | `	}` |
|      9 | 5861 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 5862 | `		str_replace_data sRepData;` |
|      - | 5863 | `		SyBlob sWorker;` |
|      - | 5864 | `		/* Initilaize the working buffer */` |
|      5 | 5865 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|      - | 5866 | `		/* Copy raw string */` |
|      5 | 5867 | `		SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nLen);` |
|      - | 5868 | `		/* Init our replace data instance */` |
|      5 | 5869 | `		sRepData.pWorker = &sWorker;` |
|      5 | 5870 | `		sRepData.xMatch = SyBlobSearch;` |
|      - | 5871 | `		/* Iterate throw array entries and perform the replace operation.*/` |
|      5 | 5872 | `		ph7_array_walk(apArg[1],StringReplaceWalker,&sRepData);` |
|      - | 5873 | `		/* All done, return the result string */` |
|      7 | 5874 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|      4 | 5875 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 5876 | `		/* Clean-up */` |
|      5 | 5877 | `		SyBlobRelease(&sWorker);` |
|      3 | 5878 | `	}else{` |
|      - | 5879 | `		int i,flen,tlen,c,iOfft;` |
|      - | 5880 | `		const char *zFrom,*zTo;` |
|      3 | 5881 | `		if( nArg < 3 ){` |
|      - | 5882 | `			/* Nothing to replace */` |
|    ! 0 | 5883 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 5884 | `			return PH7_OK;` |
|      - | 5885 | `		}` |
|      - | 5886 | `		/* Extract given arguments */` |
|      3 | 5887 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 5888 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 5889 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 5890 | `			/* Nothing to replace */` |
|    ! 0 | 5891 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 5892 | `			return PH7_OK;` |
|      - | 5893 | `		}` |
|      - | 5894 | `		/* Start the replace process */` |
|     13 | 5895 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 5896 | `			c = zIn[i];` |
|     11 | 5897 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 5898 | `				if ( iOfft < tlen ){` |
|      5 | 5899 | `					c = zTo[iOfft];` |
|      2 | 5900 | `				}` |
|      2 | 5901 | `			}` |
|     11 | 5902 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 5903 |  |
|      6 | 5904 | `		}` |
|      - | 5905 | `	}` |
|      7 | 5906 | `	return PH7_OK;` |
|      7 | 5907 |  |
|      - | 5908 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 5909 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 5910 | `/*` |
|      - | 5911 | ` * Parse an INI string.` |
|      - | 5912 |  |
|      - | 5913 | ` * According to wikipedia` |
|      - | 5914 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 5915 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 5916 | ` *  Format` |
|      - | 5917 | `*    Properties` |
|      - | 5918 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 5919 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 5920 | `*     Example:` |
|      - | 5921 | `*      name=value` |
|      - | 5922 | `*    Sections` |
|      - | 5923 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 5924 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 5925 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 5926 | `*     or the end of the file. Sections may not be nested.` |
|      - | 5927 | `*     Example:` |
|      - | 5928 | `*      [section]` |
|      - | 5929 | `*   Comments` |
|      - | 5930 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 5931 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 5932 | `*/` |
|     12 | 5933 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 5934 |  |
|      - | 5935 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 5936 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 5937 | `	SyHashEntry *pEntry;` |
|      - | 5938 | `	SyString sEntry;` |
|      - | 5939 | `	SyHash sHash;` |
|      - | 5940 | `	int c;` |
|      - | 5941 | `	/* Create an empty array and worker variables */` |
|     13 | 5942 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 5943 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 5944 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 5945 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 5946 | `		/* Out of memory */` |
|    ! 0 | 5947 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      - | 5948 | `		/* Return FALSE */` |
|    ! 0 | 5949 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5950 | `		return PH7_OK;` |
|      - | 5951 | `	}` |
|     13 | 5952 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 5953 | `	pCur = pArray;` |
|      - | 5954 | `	/* Start the parse process */` |
|     21 | 5955 | `	for(;;){` |
|      - | 5956 | `		/* Ignore leading white spaces */` |
|     69 | 5957 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 5958 | `			zIn++;` |
|      1 | 5959 | `		}` |
|     43 | 5960 | `		if( zIn >= zEnd ){` |
|      - | 5961 | `			/* No more input to process */` |
|     13 | 5962 | `			break;` |
|      - | 5963 | `		}` |
|     31 | 5964 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 5965 | `			/* Comment til the end of line */` |
|    ! 0 | 5966 | `			zIn++;` |
|    ! 0 | 5967 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 5968 | `				zIn++;` |
|    ! 0 | 5969 | `			}` |
|    ! 0 | 5970 | `			continue;` |
|      - | 5971 | `		}` |
|      - | 5972 | `		/* Reset the string cursor of the working variable */` |
|     31 | 5973 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 5974 | `		if( zIn[0] == '[' ){` |
|      - | 5975 | `			/* Section: Extract the section name */` |
|      9 | 5976 | `			zIn++;` |
|      9 | 5977 | `			zCur = zIn;` |
|     73 | 5978 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 5979 | `				zIn++;` |
|      1 | 5980 | `			}` |
|      9 | 5981 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 5982 | `				/* Save the section name */` |
|      5 | 5983 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 5984 | `				SyStringFullTrim(&sEntry);` |
|      5 | 5985 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 5986 | `				if( sEntry.nByte > 0 ){` |
|      - | 5987 | `					/* Associate an array with the section */` |
|      5 | 5988 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 5989 | `					if( pSection ){` |
|      5 | 5990 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 5991 | `						pCur = pSection;` |
|      2 | 5992 | `					}` |
|      2 | 5993 | `				}` |
|      2 | 5994 | `			}` |
|      9 | 5995 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 5996 | `		}else{` |
|      - | 5997 | `			ph7_value *pOldCur;` |
|      - | 5998 | `			int is_array;` |
|      - | 5999 | `			int iLen;` |
|      - | 6000 | `			/* Properties */` |
|     23 | 6001 | `			is_array = 0;` |
|     23 | 6002 | `			zCur = zIn;` |
|     23 | 6003 | `			iLen = 0; /* cc warning */` |
|     23 | 6004 | `			pOldCur = pCur;` |
|    155 | 6005 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 6006 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 6007 | `					/* Array */` |
|    ! 0 | 6008 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 6009 | `					is_array = 1;` |
|    ! 0 | 6010 | `					if( iLen > 0 ){` |
|    ! 0 | 6011 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 6012 | `						/* Query the hashtable */` |
|    ! 0 | 6013 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 6014 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 6015 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 6016 | `						if( pEntry ){` |
|    ! 0 | 6017 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 6018 | `						}else{` |
|      - | 6019 | `							/* Create an empty array */` |
|    ! 0 | 6020 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 6021 | `							if( pvArr ){` |
|      - | 6022 | `								/* Save the entry */` |
|    ! 0 | 6023 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 6024 | `								/* Insert the entry */` |
|    ! 0 | 6025 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6026 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 6027 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 6028 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6029 | `							}` |
|      - | 6030 | `						}` |
|    ! 0 | 6031 | `						if( pvArr ){` |
|    ! 0 | 6032 | `							pCur = pvArr;` |
|    ! 0 | 6033 | `						}` |
|    ! 0 | 6034 | `					}` |
|    ! 0 | 6035 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 6036 | `						zIn++;` |
|    ! 0 | 6037 | `					}` |
|    ! 0 | 6038 | `				}` |
|    133 | 6039 | `				zIn++;` |
|      1 | 6040 | `			}` |
|     23 | 6041 | `			if( !is_array ){` |
|     23 | 6042 | `				iLen = (int)(zIn-zCur);` |
|     11 | 6043 | `			}` |
|      - | 6044 | `			/* Trim the key */` |
|     23 | 6045 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 6046 | `			SyStringFullTrim(&sEntry);` |
|     23 | 6047 | `			if( sEntry.nByte > 0 ){` |
|     23 | 6048 | `				if( !is_array ){` |
|      - | 6049 | `					/* Save the key name */` |
|     23 | 6050 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 6051 | `				}` |
|      - | 6052 | `				/* extract key value */` |
|     23 | 6053 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 6054 | `				zIn++; /* '=' */` |
|     39 | 6055 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 6056 | `					zIn++;` |
|      1 | 6057 | `				}` |
|     23 | 6058 | `				if( zIn < zEnd ){` |
|     21 | 6059 | `					zCur = zIn;` |
|     21 | 6060 | `					c = zIn[0];` |
|     21 | 6061 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6062 | `						zIn++;` |
|      - | 6063 | `						/* Delimit the value */` |
|    ! 0 | 6064 | `						while( zIn < zEnd ){` |
|    ! 0 | 6065 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 6066 | `								break;` |
|      - | 6067 | `							}` |
|    ! 0 | 6068 | `							zIn++;` |
|    ! 0 | 6069 | `						}` |
|    ! 0 | 6070 | `						if( zIn < zEnd ){` |
|    ! 0 | 6071 | `							zIn++;` |
|    ! 0 | 6072 | `						}` |
|    ! 0 | 6073 | `					}else{` |
|    125 | 6074 | `						while( zIn < zEnd ){` |
|    123 | 6075 | `							if( zIn[0] == '\n' ){` |
|     19 | 6076 | `								if( zIn[-1] != '\\' ){` |
|     19 | 6077 | `									break;` |
|    ! 0 | 6078 | `								}` |
|    105 | 6079 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6080 | `								/* Inline comments */` |
|    ! 0 | 6081 | `								break;` |
|      - | 6082 | `							}` |
|    105 | 6083 | `							zIn++;` |
|      1 | 6084 | `						}` |
|      - | 6085 | `					}` |
|      - | 6086 | `					/* Trim the value */` |
|     21 | 6087 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 6088 | `					SyStringFullTrim(&sEntry);` |
|     21 | 6089 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6090 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 6091 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 6092 | `					}` |
|     21 | 6093 | `					if( sEntry.nByte > 0 ){` |
|     21 | 6094 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 6095 | `					}` |
|      - | 6096 | `					/* Insert the key and it's value */` |
|     21 | 6097 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 6098 | `				}` |
|     12 | 6099 | `			}else{` |
|    ! 0 | 6100 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 6101 | `					zIn++;` |
|    ! 0 | 6102 | `				}` |
|      - | 6103 | `			}` |
|     23 | 6104 | `			pCur = pOldCur;` |
|      - | 6105 | `		}` |
|      1 | 6106 | `	}` |
|     13 | 6107 | `	SyHashRelease(&sHash);` |
|      - | 6108 | `	/* Return the parse of the INI string */` |
|     13 | 6109 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 6110 | `	return SXRET_OK;` |
|      7 | 6111 |  |
|      - | 6112 | `/*` |
|      - | 6113 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 6114 | ` *  Parse a configuration string.` |
|      - | 6115 | ` * Parameters` |
|      - | 6116 | ` *  $ini` |
|      - | 6117 | ` *   The contents of the ini file being parsed.` |
|      - | 6118 | ` *  $process_sections` |
|      - | 6119 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 6120 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 6121 | ` *  $scanner_mode (Not used)` |
|      - | 6122 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 6123 | ` *   then option values will not be parsed.` |
|      - | 6124 | ` * Return` |
|      - | 6125 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 6126 | ` */` |
|     10 | 6127 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6128 |  |
|      - | 6129 | `	const char *zIni;` |
|      - | 6130 | `	int nByte;` |
|     11 | 6131 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6132 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 6133 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6134 | `		return PH7_OK;` |
|      - | 6135 | `	}` |
|      - | 6136 | `	/* Extract the raw INI buffer */` |
|     11 | 6137 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 6138 | `	/* Process the INI buffer*/` |
|     11 | 6139 | `	PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|     11 | 6140 | `	return PH7_OK;` |
|      6 | 6141 |  |
|      - | 6142 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6143 |  |
|      - | 6144 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6145 |  |
|      - | 6146 | `/*` |
|      - | 6147 | ` * Ctype Functions.` |
|      - | 6148 | ` * Status:` |
|      - | 6149 | ` *    Stable.` |
|      - | 6150 | ` */` |
|      - | 6151 | `/*` |
|      - | 6152 | ` * bool ctype_alnum(string $text)` |
|      - | 6153 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 6154 | ` * Parameters` |
|      - | 6155 | ` *  $text` |
|      - | 6156 | ` *   The tested string.` |
|      - | 6157 | ` * Return` |
|      - | 6158 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 6159 | ` */` |
|     16 | 6160 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6161 |  |
|      - | 6162 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6163 | `	int nLen;` |
|     17 | 6164 | `	if( nArg < 1 ){` |
|      - | 6165 | `		/* Missing arguments,return FALSE */` |
|      3 | 6166 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6167 | `		return PH7_OK;` |
|      - | 6168 | `	}` |
|      - | 6169 | `	/* Extract the target string */` |
|     15 | 6170 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 6171 | `	zEnd = &zIn[nLen];` |
|     15 | 6172 | `	if( nLen < 1 ){` |
|      - | 6173 | `		/* Empty string,return FALSE */` |
|      3 | 6174 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6175 | `		return PH7_OK;` |
|      - | 6176 | `	}` |
|      - | 6177 | `	/* Perform the requested operation */` |
|     32 | 6178 | `	for(;;){` |
|     65 | 6179 | `		if( zIn >= zEnd ){` |
|      - | 6180 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6181 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6182 | `			return PH7_OK;` |
|      - | 6183 | `		}` |
|     57 | 6184 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 6185 | `			break;` |
|      - | 6186 | `		}` |
|      - | 6187 | `		/* Point to the next character */` |
|     53 | 6188 | `		zIn++;` |
|      1 | 6189 | `	}` |
|      - | 6190 | `	/* The test failed,return FALSE */` |
|      5 | 6191 | `	ph7_result_bool(pCtx,0);` |
|      5 | 6192 | `	return PH7_OK;` |
|      9 | 6193 |  |
|      - | 6194 | `/*` |
|      - | 6195 | ` * bool ctype_alpha(string $text)` |
|      - | 6196 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 6197 | ` * Parameters` |
|      - | 6198 | ` *  $text` |
|      - | 6199 | ` *   The tested string.` |
|      - | 6200 | ` * Return` |
|      - | 6201 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 6202 | ` */` |
|     18 | 6203 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6204 |  |
|      - | 6205 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6206 | `	int nLen;` |
|     19 | 6207 | `	if( nArg < 1 ){` |
|      - | 6208 | `		/* Missing arguments,return FALSE */` |
|      3 | 6209 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6210 | `		return PH7_OK;` |
|      - | 6211 | `	}` |
|      - | 6212 | `	/* Extract the target string */` |
|     17 | 6213 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6214 | `	zEnd = &zIn[nLen];` |
|     17 | 6215 | `	if( nLen < 1 ){` |
|      - | 6216 | `		/* Empty string,return FALSE */` |
|      3 | 6217 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6218 | `		return PH7_OK;` |
|      - | 6219 | `	}` |
|      - | 6220 | `	/* Perform the requested operation */` |
|     42 | 6221 | `	for(;;){` |
|     85 | 6222 | `		if( zIn >= zEnd ){` |
|      - | 6223 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6224 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6225 | `			return PH7_OK;` |
|      - | 6226 | `		}` |
|     77 | 6227 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 6228 | `			break;` |
|      - | 6229 | `		}` |
|      - | 6230 | `		/* Point to the next character */` |
|     71 | 6231 | `		zIn++;` |
|      1 | 6232 | `	}` |
|      - | 6233 | `	/* The test failed,return FALSE */` |
|      7 | 6234 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6235 | `	return PH7_OK;` |
|     10 | 6236 |  |
|      - | 6237 | `/*` |
|      - | 6238 | ` * bool ctype_cntrl(string $text)` |
|      - | 6239 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 6240 | ` * Parameters` |
|      - | 6241 | ` *  $text` |
|      - | 6242 | ` *   The tested string.` |
|      - | 6243 | ` * Return` |
|      - | 6244 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 6245 | ` */` |
|     18 | 6246 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6247 |  |
|      - | 6248 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6249 | `	int nLen;` |
|     19 | 6250 | `	if( nArg < 1 ){` |
|      - | 6251 | `		/* Missing arguments,return FALSE */` |
|      3 | 6252 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6253 | `		return PH7_OK;` |
|      - | 6254 | `	}` |
|      - | 6255 | `	/* Extract the target string */` |
|     17 | 6256 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6257 | `	zEnd = &zIn[nLen];` |
|     17 | 6258 | `	if( nLen < 1 ){` |
|      - | 6259 | `		/* Empty string,return FALSE */` |
|      3 | 6260 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6261 | `		return PH7_OK;` |
|      - | 6262 | `	}` |
|      - | 6263 | `	/* Perform the requested operation */` |
|     14 | 6264 | `	for(;;){` |
|     29 | 6265 | `		if( zIn >= zEnd ){` |
|      - | 6266 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6267 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6268 | `			return PH7_OK;` |
|      - | 6269 | `		}` |
|     21 | 6270 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6271 | `			/* UTF-8 stream  */` |
|    ! 0 | 6272 | `			break;` |
|      - | 6273 | `		}` |
|     21 | 6274 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 6275 | `			break;` |
|      - | 6276 | `		}` |
|      - | 6277 | `		/* Point to the next character */` |
|     15 | 6278 | `		zIn++;` |
|      1 | 6279 | `	}` |
|      - | 6280 | `	/* The test failed,return FALSE */` |
|      7 | 6281 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6282 | `	return PH7_OK;` |
|     10 | 6283 |  |
|      - | 6284 | `/*` |
|      - | 6285 | ` * bool ctype_digit(string $text)` |
|      - | 6286 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 6287 | ` * Parameters` |
|      - | 6288 | ` *  $text` |
|      - | 6289 | ` *   The tested string.` |
|      - | 6290 | ` * Return` |
|      - | 6291 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 6292 | ` */` |
|   1546 | 6293 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6294 |  |
|      - | 6295 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6296 | `	int nLen;` |
|   1548 | 6297 | `	if( nArg < 1 ){` |
|      - | 6298 | `		/* Missing arguments,return FALSE */` |
|      3 | 6299 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6300 | `		return PH7_OK;` |
|      - | 6301 | `	}` |
|      - | 6302 | `	/* Extract the target string */` |
|   1546 | 6303 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1546 | 6304 | `	zEnd = &zIn[nLen];` |
|   1546 | 6305 | `	if( nLen < 1 ){` |
|      - | 6306 | `		/* Empty string,return FALSE */` |
|      3 | 6307 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6308 | `		return PH7_OK;` |
|      - | 6309 | `	}` |
|      - | 6310 | `	/* Perform the requested operation */` |
|   1448 | 6311 | `	for(;;){` |
|   2898 | 6312 | `		if( zIn >= zEnd ){` |
|      - | 6313 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1324 | 6314 | `			ph7_result_bool(pCtx,1);` |
|   1324 | 6315 | `			return PH7_OK;` |
|      - | 6316 | `		}` |
|   1576 | 6317 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6318 | `			/* UTF-8 stream  */` |
|    ! 0 | 6319 | `			break;` |
|      - | 6320 | `		}` |
|   1576 | 6321 | `		if( !SyisDigit(zIn[0]) ){` |
|    222 | 6322 | `			break;` |
|      - | 6323 | `		}` |
|      - | 6324 | `		/* Point to the next character */` |
|   1356 | 6325 | `		zIn++;` |
|      2 | 6326 | `	}` |
|      - | 6327 | `	/* The test failed,return FALSE */` |
|    222 | 6328 | `	ph7_result_bool(pCtx,0);` |
|    222 | 6329 | `	return PH7_OK;` |
|    775 | 6330 |  |
|      - | 6331 | `/*` |
|      - | 6332 | ` * bool ctype_xdigit(string $text)` |
|      - | 6333 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 6334 | ` * Parameters` |
|      - | 6335 | ` *  $text` |
|      - | 6336 | ` *   The tested string.` |
|      - | 6337 | ` * Return` |
|      - | 6338 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 6339 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 6340 | ` */` |
|     20 | 6341 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6342 |  |
|      - | 6343 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6344 | `	int nLen;` |
|     21 | 6345 | `	if( nArg < 1 ){` |
|      - | 6346 | `		/* Missing arguments,return FALSE */` |
|      3 | 6347 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6348 | `		return PH7_OK;` |
|      - | 6349 | `	}` |
|      - | 6350 | `	/* Extract the target string */` |
|     19 | 6351 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 6352 | `	zEnd = &zIn[nLen];` |
|     19 | 6353 | `	if( nLen < 1 ){` |
|      - | 6354 | `		/* Empty string,return FALSE */` |
|      3 | 6355 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6356 | `		return PH7_OK;` |
|      - | 6357 | `	}` |
|      - | 6358 | `	/* Perform the requested operation */` |
|     46 | 6359 | `	for(;;){` |
|     93 | 6360 | `		if( zIn >= zEnd ){` |
|      - | 6361 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 6362 | `			ph7_result_bool(pCtx,1);` |
|     11 | 6363 | `			return PH7_OK;` |
|      - | 6364 | `		}` |
|     83 | 6365 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6366 | `			/* UTF-8 stream  */` |
|    ! 0 | 6367 | `			break;` |
|      - | 6368 | `		}` |
|     83 | 6369 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 6370 | `			break;` |
|      - | 6371 | `		}` |
|      - | 6372 | `		/* Point to the next character */` |
|     77 | 6373 | `		zIn++;` |
|      1 | 6374 | `	}` |
|      - | 6375 | `	/* The test failed,return FALSE */` |
|      7 | 6376 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6377 | `	return PH7_OK;` |
|     11 | 6378 |  |
|      - | 6379 | `/*` |
|      - | 6380 | ` * bool ctype_graph(string $text)` |
|      - | 6381 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 6382 | ` * Parameters` |
|      - | 6383 | ` *  $text` |
|      - | 6384 | ` *   The tested string.` |
|      - | 6385 | ` * Return` |
|      - | 6386 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 6387 | ` * (no white space), FALSE otherwise.` |
|      - | 6388 | ` */` |
|     18 | 6389 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6390 |  |
|      - | 6391 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6392 | `	int nLen;` |
|     19 | 6393 | `	if( nArg < 1 ){` |
|      - | 6394 | `		/* Missing arguments,return FALSE */` |
|      3 | 6395 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6396 | `		return PH7_OK;` |
|      - | 6397 | `	}` |
|      - | 6398 | `	/* Extract the target string */` |
|     17 | 6399 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6400 | `	zEnd = &zIn[nLen];` |
|     17 | 6401 | `	if( nLen < 1 ){` |
|      - | 6402 | `		/* Empty string,return FALSE */` |
|      3 | 6403 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6404 | `		return PH7_OK;` |
|      - | 6405 | `	}` |
|      - | 6406 | `	/* Perform the requested operation */` |
|     57 | 6407 | `	for(;;){` |
|    115 | 6408 | `		if( zIn >= zEnd ){` |
|      - | 6409 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6410 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6411 | `			return PH7_OK;` |
|      - | 6412 | `		}` |
|    107 | 6413 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6414 | `			/* UTF-8 stream  */` |
|    ! 0 | 6415 | `			break;` |
|      - | 6416 | `		}` |
|    107 | 6417 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 6418 | `			break;` |
|      - | 6419 | `		}` |
|      - | 6420 | `		/* Point to the next character */` |
|    101 | 6421 | `		zIn++;` |
|      1 | 6422 | `	}` |
|      - | 6423 | `	/* The test failed,return FALSE */` |
|      7 | 6424 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6425 | `	return PH7_OK;` |
|     10 | 6426 |  |
|      - | 6427 | `/*` |
|      - | 6428 | ` * bool ctype_print(string $text)` |
|      - | 6429 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 6430 | ` * Parameters` |
|      - | 6431 | ` *  $text` |
|      - | 6432 | ` *   The tested string.` |
|      - | 6433 | ` * Return` |
|      - | 6434 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 6435 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 6436 | ` *  or control function at all.` |
|      - | 6437 | ` */` |
|     18 | 6438 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6439 |  |
|      - | 6440 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6441 | `	int nLen;` |
|     19 | 6442 | `	if( nArg < 1 ){` |
|      - | 6443 | `		/* Missing arguments,return FALSE */` |
|      3 | 6444 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6445 | `		return PH7_OK;` |
|      - | 6446 | `	}` |
|      - | 6447 | `	/* Extract the target string */` |
|     17 | 6448 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6449 | `	zEnd = &zIn[nLen];` |
|     17 | 6450 | `	if( nLen < 1 ){` |
|      - | 6451 | `		/* Empty string,return FALSE */` |
|      3 | 6452 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6453 | `		return PH7_OK;` |
|      - | 6454 | `	}` |
|      - | 6455 | `	/* Perform the requested operation */` |
|     63 | 6456 | `	for(;;){` |
|    127 | 6457 | `		if( zIn >= zEnd ){` |
|      - | 6458 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6459 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6460 | `			return PH7_OK;` |
|      - | 6461 | `		}` |
|    119 | 6462 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6463 | `			/* UTF-8 stream  */` |
|    ! 0 | 6464 | `			break;` |
|      - | 6465 | `		}` |
|    119 | 6466 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 6467 | `			break;` |
|      - | 6468 | `		}` |
|      - | 6469 | `		/* Point to the next character */` |
|    113 | 6470 | `		zIn++;` |
|      1 | 6471 | `	}` |
|      - | 6472 | `	/* The test failed,return FALSE */` |
|      7 | 6473 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6474 | `	return PH7_OK;` |
|     10 | 6475 |  |
|      - | 6476 | `/*` |
|      - | 6477 | ` * bool ctype_punct(string $text)` |
|      - | 6478 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 6479 | ` * Parameters` |
|      - | 6480 | ` *  $text` |
|      - | 6481 | ` *   The tested string.` |
|      - | 6482 | ` * Return` |
|      - | 6483 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 6484 | ` *  digit or blank, FALSE otherwise.` |
|      - | 6485 | ` */` |
|     20 | 6486 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6487 |  |
|      - | 6488 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6489 | `	int nLen;` |
|     21 | 6490 | `	if( nArg < 1 ){` |
|      - | 6491 | `		/* Missing arguments,return FALSE */` |
|      3 | 6492 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6493 | `		return PH7_OK;` |
|      - | 6494 | `	}` |
|      - | 6495 | `	/* Extract the target string */` |
|     19 | 6496 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 6497 | `	zEnd = &zIn[nLen];` |
|     19 | 6498 | `	if( nLen < 1 ){` |
|      - | 6499 | `		/* Empty string,return FALSE */` |
|      3 | 6500 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6501 | `		return PH7_OK;` |
|      - | 6502 | `	}` |
|      - | 6503 | `	/* Perform the requested operation */` |
|     38 | 6504 | `	for(;;){` |
|     77 | 6505 | `		if( zIn >= zEnd ){` |
|      - | 6506 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6507 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6508 | `			return PH7_OK;` |
|      - | 6509 | `		}` |
|     69 | 6510 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6511 | `			/* UTF-8 stream  */` |
|    ! 0 | 6512 | `			break;` |
|      - | 6513 | `		}` |
|     69 | 6514 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 6515 | `			break;` |
|      - | 6516 | `		}` |
|      - | 6517 | `		/* Point to the next character */` |
|     61 | 6518 | `		zIn++;` |
|      1 | 6519 | `	}` |
|      - | 6520 | `	/* The test failed,return FALSE */` |
|      9 | 6521 | `	ph7_result_bool(pCtx,0);` |
|      9 | 6522 | `	return PH7_OK;` |
|     11 | 6523 |  |
|      - | 6524 | `/*` |
|      - | 6525 | ` * bool ctype_space(string $text)` |
|      - | 6526 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 6527 | ` * Parameters` |
|      - | 6528 | ` *  $text` |
|      - | 6529 | ` *   The tested string.` |
|      - | 6530 | ` * Return` |
|      - | 6531 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 6532 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 6533 | ` *  and form feed characters.` |
|      - | 6534 | ` */` |
|  59528 | 6535 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6536 |  |
|      - | 6537 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6538 | `	int nLen;` |
|  59530 | 6539 | `	if( nArg < 1 ){` |
|      - | 6540 | `		/* Missing arguments,return FALSE */` |
|      3 | 6541 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6542 | `		return PH7_OK;` |
|      - | 6543 | `	}` |
|      - | 6544 | `	/* Extract the target string */` |
|  59528 | 6545 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  59528 | 6546 | `	zEnd = &zIn[nLen];` |
|  59528 | 6547 | `	if( nLen < 1 ){` |
|      - | 6548 | `		/* Empty string,return FALSE */` |
|      3 | 6549 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6550 | `		return PH7_OK;` |
|      - | 6551 | `	}` |
|      - | 6552 | `	/* Perform the requested operation */` |
|  30792 | 6553 | `	for(;;){` |
|  61542 | 6554 | `		if( zIn >= zEnd ){` |
|      - | 6555 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1994 | 6556 | `			ph7_result_bool(pCtx,1);` |
|   1994 | 6557 | `			return PH7_OK;` |
|      - | 6558 | `		}` |
|  59550 | 6559 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6560 | `			/* UTF-8 stream  */` |
|    ! 0 | 6561 | `			break;` |
|      - | 6562 | `		}` |
|  59550 | 6563 | `		if( !SyisSpace(zIn[0]) ){` |
|  57534 | 6564 | `			break;` |
|      - | 6565 | `		}` |
|      - | 6566 | `		/* Point to the next character */` |
|   2018 | 6567 | `		zIn++;` |
|      2 | 6568 | `	}` |
|      - | 6569 | `	/* The test failed,return FALSE */` |
|  57534 | 6570 | `	ph7_result_bool(pCtx,0);` |
|  57534 | 6571 | `	return PH7_OK;` |
|  29788 | 6572 |  |
|      - | 6573 | `/*` |
|      - | 6574 | ` * bool ctype_lower(string $text)` |
|      - | 6575 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 6576 | ` * Parameters` |
|      - | 6577 | ` *  $text` |
|      - | 6578 | ` *   The tested string.` |
|      - | 6579 | ` * Return` |
|      - | 6580 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 6581 | ` */` |
|     18 | 6582 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6583 |  |
|      - | 6584 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6585 | `	int nLen;` |
|     19 | 6586 | `	if( nArg < 1 ){` |
|      - | 6587 | `		/* Missing arguments,return FALSE */` |
|      3 | 6588 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6589 | `		return PH7_OK;` |
|      - | 6590 | `	}` |
|      - | 6591 | `	/* Extract the target string */` |
|     17 | 6592 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6593 | `	zEnd = &zIn[nLen];` |
|     17 | 6594 | `	if( nLen < 1 ){` |
|      - | 6595 | `		/* Empty string,return FALSE */` |
|      3 | 6596 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6597 | `		return PH7_OK;` |
|      - | 6598 | `	}` |
|      - | 6599 | `	/* Perform the requested operation */` |
|     27 | 6600 | `	for(;;){` |
|     55 | 6601 | `		if( zIn >= zEnd ){` |
|      - | 6602 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 6603 | `			ph7_result_bool(pCtx,1);` |
|      5 | 6604 | `			return PH7_OK;` |
|      - | 6605 | `		}` |
|     51 | 6606 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 6607 | `			break;` |
|      - | 6608 | `		}` |
|      - | 6609 | `		/* Point to the next character */` |
|     41 | 6610 | `		zIn++;` |
|      1 | 6611 | `	}` |
|      - | 6612 | `	/* The test failed,return FALSE */` |
|     11 | 6613 | `	ph7_result_bool(pCtx,0);` |
|     11 | 6614 | `	return PH7_OK;` |
|     10 | 6615 |  |
|      - | 6616 | `/*` |
|      - | 6617 | ` * bool ctype_upper(string $text)` |
|      - | 6618 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 6619 | ` * Parameters` |
|      - | 6620 | ` *  $text` |
|      - | 6621 | ` *   The tested string.` |
|      - | 6622 | ` * Return` |
|      - | 6623 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 6624 | ` */` |
|     18 | 6625 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6626 |  |
|      - | 6627 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6628 | `	int nLen;` |
|     19 | 6629 | `	if( nArg < 1 ){` |
|      - | 6630 | `		/* Missing arguments,return FALSE */` |
|      3 | 6631 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6632 | `		return PH7_OK;` |
|      - | 6633 | `	}` |
|      - | 6634 | `	/* Extract the target string */` |
|     17 | 6635 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6636 | `	zEnd = &zIn[nLen];` |
|     17 | 6637 | `	if( nLen < 1 ){` |
|      - | 6638 | `		/* Empty string,return FALSE */` |
|      3 | 6639 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6640 | `		return PH7_OK;` |
|      - | 6641 | `	}` |
|      - | 6642 | `	/* Perform the requested operation */` |
|     28 | 6643 | `	for(;;){` |
|     57 | 6644 | `		if( zIn >= zEnd ){` |
|      - | 6645 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 6646 | `			ph7_result_bool(pCtx,1);` |
|      5 | 6647 | `			return PH7_OK;` |
|      - | 6648 | `		}` |
|     53 | 6649 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 6650 | `			break;` |
|      - | 6651 | `		}` |
|      - | 6652 | `		/* Point to the next character */` |
|     43 | 6653 | `		zIn++;` |
|      1 | 6654 | `	}` |
|      - | 6655 | `	/* The test failed,return FALSE */` |
|     11 | 6656 | `	ph7_result_bool(pCtx,0);` |
|     11 | 6657 | `	return PH7_OK;` |
|     10 | 6658 |  |
|      - | 6659 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 6660 | `/*` |
|      - | 6661 | ` * Section:` |
|      - | 6662 | ` *    URL handling Functions.` |
|      - | 6663 | ` * Status:` |
|      - | 6664 | ` *    Stable.` |
|      - | 6665 | ` */` |
|      - | 6666 | `/*` |
|      - | 6667 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 6668 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 6669 | ` */` |
|   1026 | 6670 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 6671 |  |
|      - | 6672 | `	/* Store in the call context result buffer */` |
|   1028 | 6673 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 6674 | `	return SXRET_OK;` |
|      2 | 6675 |  |
|      - | 6676 | `/*` |
|      - | 6677 | ` * string base64_encode(string $data)` |
|      - | 6678 | ` * string convert_uuencode(string $data)` |
|      - | 6679 | ` *  Encodes data with MIME base64` |
|      - | 6680 | ` * Parameter` |
|      - | 6681 | ` *  $data` |
|      - | 6682 | ` *    Data to encode` |
|      - | 6683 | ` * Return` |
|      - | 6684 | ` *  Encoded data or FALSE on failure.` |
|      - | 6685 | ` */` |
|     10 | 6686 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6687 |  |
|      - | 6688 | `	const char *zIn;` |
|      - | 6689 | `	int nLen;` |
|     11 | 6690 | `	if( nArg < 1 ){` |
|      - | 6691 | `		/* Missing arguments,return FALSE */` |
|      5 | 6692 | `		ph7_result_bool(pCtx,0);` |
|      5 | 6693 | `		return PH7_OK;` |
|      - | 6694 | `	}` |
|      - | 6695 | `	/* Extract the input string */` |
|      7 | 6696 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 6697 | `	if( nLen < 1 ){` |
|      - | 6698 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 6699 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6700 | `		return PH7_OK;` |
|      - | 6701 | `	}` |
|      - | 6702 | `	/* Perform the BASE64 encoding */` |
|      7 | 6703 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 6704 | `	return PH7_OK;` |
|      6 | 6705 |  |
|      - | 6706 | `/*` |
|      - | 6707 | ` * string base64_decode(string $data)` |
|      - | 6708 | ` * string convert_uudecode(string $data)` |
|      - | 6709 | ` *  Decodes data encoded with MIME base64` |
|      - | 6710 | ` * Parameter` |
|      - | 6711 | ` *  $data` |
|      - | 6712 | ` *    Encoded data.` |
|      - | 6713 | ` * Return` |
|      - | 6714 | ` *  Returns the original data or FALSE on failure.` |
|      - | 6715 | ` */` |
|     36 | 6716 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6717 |  |
|      - | 6718 | `	const char *zIn;` |
|      - | 6719 | `	int nLen;` |
|     38 | 6720 | `	if( nArg < 1 ){` |
|      - | 6721 | `		/* Missing arguments,return FALSE */` |
|      3 | 6722 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6723 | `		return PH7_OK;` |
|      - | 6724 | `	}` |
|      - | 6725 | `	/* Extract the input string */` |
|     36 | 6726 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 6727 | `	if( nLen < 1 ){` |
|      - | 6728 | `		/* Nothing to process,return FALSE */` |
|      3 | 6729 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6730 | `		return PH7_OK;` |
|      - | 6731 | `	}` |
|      - | 6732 | `	/* Perform the BASE64 decoding */` |
|     34 | 6733 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 6734 | `	return PH7_OK;` |
|     20 | 6735 |  |
|      - | 6736 | `/*` |
|      - | 6737 | ` * string urlencode(string $str)` |
|      - | 6738 | ` *  URL encoding` |
|      - | 6739 | ` * Parameter` |
|      - | 6740 | ` *  $data` |
|      - | 6741 | ` *   Input string.` |
|      - | 6742 | ` * Return` |
|      - | 6743 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 6744 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 6745 | ` *  encoded as plus (+) signs.` |
|      - | 6746 | ` */` |
|      6 | 6747 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6748 |  |
|      - | 6749 | `	const char *zIn;` |
|      - | 6750 | `	int nLen;` |
|      7 | 6751 | `	if( nArg < 1 ){` |
|      - | 6752 | `		/* Missing arguments,return FALSE */` |
|      3 | 6753 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6754 | `		return PH7_OK;` |
|      - | 6755 | `	}` |
|      - | 6756 | `	/* Extract the input string */` |
|      5 | 6757 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 6758 | `	if( nLen < 1 ){` |
|      - | 6759 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 6760 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6761 | `		return PH7_OK;` |
|      - | 6762 | `	}` |
|      - | 6763 | `	/* Perform the URL encoding */` |
|      5 | 6764 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 6765 | `	return PH7_OK;` |
|      4 | 6766 |  |
|      - | 6767 | `/*` |
|      - | 6768 | ` * string urldecode(string $str)` |
|      - | 6769 | ` *  Decodes any %## encoding in the given string.` |
|      - | 6770 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 6771 | ` * Parameter` |
|      - | 6772 | ` *  $data` |
|      - | 6773 | ` *    Input string.` |
|      - | 6774 | ` * Return` |
|      - | 6775 | ` *  Decoded URL or FALSE on failure.` |
|      - | 6776 | ` */` |
|      8 | 6777 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6778 |  |
|      - | 6779 | `	const char *zIn;` |
|      - | 6780 | `	int nLen;` |
|      9 | 6781 | `	if( nArg < 1 ){` |
|      - | 6782 | `		/* Missing arguments,return FALSE */` |
|      3 | 6783 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6784 | `		return PH7_OK;` |
|      - | 6785 | `	}` |
|      - | 6786 | `	/* Extract the input string */` |
|      7 | 6787 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 6788 | `	if( nLen < 1 ){` |
|      - | 6789 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 6790 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6791 | `		return PH7_OK;` |
|      - | 6792 | `	}` |
|      - | 6793 | `	/* Perform the URL decoding */` |
|      7 | 6794 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 6795 | `	return PH7_OK;` |
|      5 | 6796 |  |
|      - | 6797 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6798 | `/* Table of the built-in functions */` |
|      - | 6799 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 6800 | `	   /* Variable handling functions */` |
|      - | 6801 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 6802 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 6803 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 6804 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 6805 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 6806 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 6807 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 6808 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 6809 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 6810 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 6811 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 6812 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 6813 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 6814 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 6815 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 6816 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 6817 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 6818 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 6819 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 6820 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 6821 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6822 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 6823 | `	   /* Math functions */` |
|      - | 6824 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 6825 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 6826 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 6827 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 6828 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 6829 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 6830 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 6831 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 6832 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 6833 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 6834 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 6835 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 6836 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 6837 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 6838 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 6839 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 6840 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 6841 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 6842 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 6843 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 6844 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 6845 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 6846 | `	{ "round",    PH7_builtin_round        },` |
|      - | 6847 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 6848 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 6849 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 6850 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 6851 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 6852 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 6853 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 6854 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 6855 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 6856 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6857 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6858 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 6859 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6860 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6861 | `	   /* String handling functions */` |
|      - | 6862 |  |
|      - | 6863 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 6864 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 6865 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 6866 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 6867 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 6868 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 6869 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 6870 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 6871 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 6872 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 6873 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 6874 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 6875 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 6876 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 6877 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 6878 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 6879 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 6880 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 6881 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 6882 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 6883 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 6884 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 6885 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 6886 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 6887 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 6888 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 6889 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 6890 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 6891 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 6892 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 6893 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 6894 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 6895 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 6896 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 6897 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 6898 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 6899 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 6900 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 6901 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 6902 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 6903 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 6904 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 6905 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 6906 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 6907 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 6908 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 6909 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 6910 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 6911 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - | 6912 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - | 6913 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - | 6914 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 6915 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6916 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6917 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 6918 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 6919 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 6920 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 6921 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6922 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6923 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 6924 |  |
|      - | 6925 |  |
|      - | 6926 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 6927 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 6928 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 6929 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 6930 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 6931 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6932 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6933 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 6934 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 6935 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6936 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6937 |  |
|      - | 6938 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 6939 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 6940 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 6941 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 6942 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 6943 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 6944 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 6945 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 6946 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 6947 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 6948 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 6949 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 6950 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6951 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6952 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 6953 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6954 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6955 |  |
|      - | 6956 | `	         /* Ctype functions */` |
|      - | 6957 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 6958 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 6959 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 6960 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 6961 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 6962 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 6963 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 6964 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 6965 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 6966 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 6967 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 6968 | `	         /* Time functions */` |
|      - | 6969 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 6970 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 6971 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 6972 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 6973 | `	{ "date",        PH7_builtin_date         },` |
|      - | 6974 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 6975 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 6976 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 6977 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 6978 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 6979 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 6980 | `	        /* URL functions */` |
|      - | 6981 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 6982 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 6983 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 6984 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 6985 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 6986 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 6987 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 6988 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 6989 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6990 | `};` |
|      - | 6991 | `/*` |
|      - | 6992 | ` * Register the built-in functions defined above,the array functions` |
|      - | 6993 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 6994 | ` */` |
|   2820 | 6995 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      2 | 6996 |  |
|      - | 6997 | `	sxu32 n;` |
| 445562 | 6998 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 442742 | 6999 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 221372 | 7000 | `	}` |
|      - | 7001 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   2822 | 7002 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 7003 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   2822 | 7004 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   2822 | 7005 |  |
|      - | 7006 |  |
