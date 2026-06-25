# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3638/4108 lines (88.56%)

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
|    196 |   42 | `static int PH7_builtin_is_float(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   43 |  |
|    197 |   44 | `	int res = 0; /* Assume false by default */` |
|    197 |   45 | `	if( nArg > 0 ){` |
|    195 |   46 | `		res = ph7_value_is_float(apArg[0]);` |
|     97 |   47 | `	}` |
|      - |   48 | `	/* Query result */` |
|    197 |   49 | `	ph7_result_bool(pCtx,res);` |
|    197 |   50 | `	return PH7_OK;` |
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
|    628 |   62 | `static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |   63 |  |
|    630 |   64 | `	int res = 0; /* Assume false by default */` |
|    630 |   65 | `	if( nArg > 0 ){` |
|      - |   66 | `		/* Strict PHP identity: a float is never an int, even when it holds an` |
|      - |   67 | `		 * integer value (1.0). An integer-valued real carries both MEMOBJ_INT` |
|      - |   68 | `		 * (cached) and MEMOBJ_REAL, so REAL must be excluded here. */` |
|    628 |   69 | `		res = ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]);` |
|    313 |   70 | `	}` |
|      - |   71 | `	/* Query result */` |
|    630 |   72 | `	ph7_result_bool(pCtx,res);` |
|    630 |   73 | `	return PH7_OK;` |
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
|    242 |  155 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  156 |  |
|    246 |  157 | `	int res = 0; /* Assume false by default */` |
|    246 |  158 | `	if( nArg > 0 ){` |
|    244 |  159 | `		res = ph7_value_is_array(apArg[0]);` |
|    120 |  160 | `	}` |
|      - |  161 | `	/* Query result */` |
|    246 |  162 | `	ph7_result_bool(pCtx,res);` |
|    246 |  163 | `	return PH7_OK;` |
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
|  26876 |  295 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  296 |  |
|  26881 |  297 | `	int res = 1; /* Assume empty by default */` |
|  26881 |  298 | `	if( nArg > 0 ){` |
|  26879 |  299 | `		res = ph7_value_is_empty(apArg[0]);` |
|  13437 |  300 | `	}` |
|  26881 |  301 | `	ph7_result_bool(pCtx,res);` |
|  26881 |  302 | `	return PH7_OK;` |
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
| 200300 |  345 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  346 |  |
|      - |  347 | `	const char *zSource,*zOfft;` |
|      - |  348 | `	int nOfft,nLen,nSrcLen;` |
| 200305 |  349 | `	if( nArg < 2 ){` |
|      - |  350 | `		/* return FALSE */` |
|      5 |  351 | `		ph7_result_bool(pCtx,0);` |
|      5 |  352 | `		return PH7_OK;` |
|      - |  353 | `	}` |
|      - |  354 | `	/* Extract the target string */` |
| 200301 |  355 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 200301 |  356 | `	if( nSrcLen < 1 ){` |
|      - |  357 | `		/* Empty string,return FALSE */` |
|  11429 |  358 | `		ph7_result_bool(pCtx,0);` |
|  11429 |  359 | `		return PH7_OK;` |
|      - |  360 | `	}` |
| 188877 |  361 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  362 | `	/* Extract the offset */` |
| 188877 |  363 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 188877 |  364 | `	if( nOfft < 0 ){` |
|  31193 |  365 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  31193 |  366 | `		if( zOfft < zSource ){` |
|      - |  367 | `			/* Invalid offset */` |
|      5 |  368 | `			ph7_result_bool(pCtx,0);` |
|      5 |  369 | `			return PH7_OK;` |
|      - |  370 | `		}` |
|  31189 |  371 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  31189 |  372 | `		nOfft = (int)(zOfft-zSource);` |
| 173281 |  373 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  374 | `		/* Invalid offset */` |
|    169 |  375 | `		ph7_result_bool(pCtx,0);` |
|    169 |  376 | `		return PH7_OK;` |
|    ! 0 |  377 | `	}else{` |
| 157525 |  378 | `		zOfft = &zSource[nOfft];` |
| 157525 |  379 | `		nLen = nSrcLen - nOfft;` |
|      - |  380 | `	}` |
| 188709 |  381 | `	if( nArg > 2 ){` |
|      - |  382 | `		/* Extract the length */` |
| 156017 |  383 | `		nLen = ph7_value_to_int(apArg[2]);` |
| 156017 |  384 | `		if( nLen == 0 ){` |
|      - |  385 | `			/* Invalid length,return an empty string */` |
|      5 |  386 | `			ph7_result_string(pCtx,"",0);` |
|      5 |  387 | `			return PH7_OK;` |
| 156013 |  388 | `		}else if( nLen < 0 ){` |
|  31181 |  389 | `			nLen = nSrcLen + nLen - nOfft;` |
|  31181 |  390 | `			if( nLen < 1 ){` |
|      - |  391 | `				/* Invalid  length */` |
|      3 |  392 | `				nLen = nSrcLen - nOfft;` |
|      1 |  393 | `			}` |
|  15588 |  394 | `		}` |
| 156013 |  395 | `		if( nLen + nOfft > nSrcLen ){` |
|      - |  396 | `			/* Invalid length */` |
|   4633 |  397 | `			nLen = nSrcLen - nOfft;` |
|   2314 |  398 | `		}` |
|  78004 |  399 | `	}` |
|      - |  400 | `	/* Return the substring */` |
| 188705 |  401 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 188705 |  402 | `	return PH7_OK;` |
| 100155 |  403 |  |
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
|     29 |  674 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
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
|     43 |  789 | `	} else if( ph7_value_is_array(apArg[0]) \|\|` |
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
|     40 |  807 | `	} else if( ph7_value_is_array(apArg[1]) \|\|` |
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
|   6048 | 1372 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1373 |  |
|   6053 | 1374 | `	int iLen = 0;` |
|   6053 | 1375 | `	if( nArg > 0 ){` |
|   6051 | 1376 | `		ph7_value_to_string(apArg[0],&iLen);` |
|   3023 | 1377 | `	}` |
|      - | 1378 | `	/* String length */` |
|   6053 | 1379 | `	ph7_result_int(pCtx,iLen);` |
|   6053 | 1380 | `	return PH7_OK;` |
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
|      - | 1519 | `	sxi32 rc;             /* Captured allocation rc; SXERR_MEM => the builtin raises an OOM fatal */` |
|      - | 1520 | `};` |
|      - | 1521 | `/*` |
|      - | 1522 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 1523 | ` * The following routine is invoked for each array entry passed` |
|      - | 1524 | ` * to the implode() function.` |
|      - | 1525 | ` */` |
| 125954 | 1526 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 1527 |  |
|  62977 | 1528 | `	SXUNUSED(pKey);` |
| 125959 | 1529 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 1530 | `	const char *zData;` |
|      - | 1531 | `	int nLen;` |
| 125959 | 1532 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 1533 | `		if( pData->nSeplen > 0 ){` |
|      3 | 1534 | `			if( !pData->bFirst ){` |
|      - | 1535 | `				/* append the separator first */` |
|      3 | 1536 | `				if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 1537 | `					pData->rc = SXERR_MEM;` |
|    ! 0 | 1538 | `					return PH7_ABORT;` |
|      - | 1539 | `				}` |
|      2 | 1540 | `			}else{` |
|    ! 0 | 1541 | `				pData->bFirst = 0;` |
|      - | 1542 | `			}` |
|      1 | 1543 | `		}` |
|      - | 1544 | `		/* Recurse */` |
|      3 | 1545 | `		pData->bFirst = 1;` |
|      3 | 1546 | `		pData->nRecCount++;` |
|      3 | 1547 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 1548 | `		pData->nRecCount--;` |
|      - | 1549 | `		/* Propagate an allocation failure surfaced deeper in the recursion. */` |
|      3 | 1550 | `		if( pData->rc != SXRET_OK ){` |
|    ! 0 | 1551 | `			return PH7_ABORT;` |
|      - | 1552 | `		}` |
|      3 | 1553 | `		return PH7_OK;` |
|      - | 1554 | `	}` |
|      - | 1555 | `	/* Extract the string representation of the entry value */` |
| 125957 | 1556 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 1557 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 125957 | 1558 | `	if( pData->bFirst ){` |
|  31505 | 1559 | `		pData->bFirst = 0;` |
| 110207 | 1560 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 1561 | `		/* append the separator first */` |
|  94445 | 1562 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 1563 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1564 | `			return PH7_ABORT;` |
|      - | 1565 | `		}` |
|  47220 | 1566 | `	}` |
|      - | 1567 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 125957 | 1568 | `	if( nLen > 0 ){` |
| 114533 | 1569 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1570 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1571 | `			return PH7_ABORT;` |
|      - | 1572 | `		}` |
|  57264 | 1573 | `	}` |
| 125957 | 1574 | `	return PH7_OK;` |
|  62982 | 1575 |  |
|      - | 1576 | `/*` |
|      - | 1577 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 1578 | ` * string implode(array $pieces,...)` |
|      - | 1579 | ` *  Join array elements with a string.` |
|      - | 1580 | ` * $glue` |
|      - | 1581 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 1582 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 1583 | ` * $pieces` |
|      - | 1584 | ` *   The array of strings to implode.` |
|      - | 1585 | ` * Return` |
|      - | 1586 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 1587 | ` *  order, with the glue string between each element.` |
|      - | 1588 | ` */` |
|  31526 | 1589 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1590 |  |
|      - | 1591 | `	struct implode_data imp_data;` |
|  31531 | 1592 | `	int i = 1;` |
|  31531 | 1593 | `	if( nArg < 1 ){` |
|      - | 1594 | `		/* Missing argument,return NULL */` |
|    ! 0 | 1595 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1596 | `		return PH7_OK;` |
|      - | 1597 | `	}` |
|      - | 1598 | `	/* Prepare the implode context */` |
|  31531 | 1599 | `	imp_data.pCtx = pCtx;` |
|  31531 | 1600 | `	imp_data.bRecursive = 0;` |
|  31531 | 1601 | `	imp_data.bFirst = 1;` |
|  31531 | 1602 | `	imp_data.nRecCount = 0;` |
|  31531 | 1603 | `	imp_data.rc = SXRET_OK;` |
|  31531 | 1604 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  31529 | 1605 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  15767 | 1606 | `	}else{` |
|      3 | 1607 | `		imp_data.zSep = 0;` |
|      3 | 1608 | `		imp_data.nSeplen = 0;` |
|      3 | 1609 | `		i = 0;` |
|      - | 1610 | `	}` |
|  31531 | 1611 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 1612 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1613 | `	}` |
|      - | 1614 | `	/* Start the 'join' process */` |
|  63057 | 1615 | `	while( i < nArg ){` |
|  31531 | 1616 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1617 | `			/* Iterate throw array entries */` |
|  31531 | 1618 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 1619 | `			/* Surface a callback allocation failure as a fatal */` |
|  31531 | 1620 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 1621 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1622 | `			}` |
|  15768 | 1623 | `		}else{` |
|      - | 1624 | `			const char *zData;` |
|      - | 1625 | `			int nLen;` |
|      - | 1626 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 1627 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1628 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 1629 | `			if( imp_data.bFirst ){` |
|    ! 0 | 1630 | `				imp_data.bFirst = 0;` |
|    ! 0 | 1631 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1632 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 1633 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1634 | `				}` |
|    ! 0 | 1635 | `			}` |
|      - | 1636 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 1637 | `			if( nLen > 0 ){` |
|    ! 0 | 1638 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1639 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1640 | `				}` |
|    ! 0 | 1641 | `			}` |
|      - | 1642 | `		}` |
|  31531 | 1643 | `		i++;` |
|      5 | 1644 | `	}` |
|  31531 | 1645 | `	return PH7_OK;` |
|  15768 | 1646 |  |
|      - | 1647 | `/*` |
|      - | 1648 | ` * Symisc eXtension:` |
|      - | 1649 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 1650 | ` * Purpose` |
|      - | 1651 | ` *  Same as implode() but recurse on arrays.` |
|      - | 1652 | ` * Example:` |
|      - | 1653 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 1654 | ` *   echo implode_recursive("/",$a);` |
|      - | 1655 | ` *   Will output` |
|      - | 1656 | ` *     usr/home/dean.` |
|      - | 1657 | ` *   While the standard implode would produce.` |
|      - | 1658 | ` *    usr/Array.` |
|      - | 1659 | ` * Parameter` |
|      - | 1660 | ` *  Refer to implode().` |
|      - | 1661 | ` * Return` |
|      - | 1662 | ` *  Refer to implode().` |
|      - | 1663 | ` */` |
|     12 | 1664 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1665 |  |
|      - | 1666 | `	struct implode_data imp_data;` |
|     13 | 1667 | `	int i = 1;` |
|     13 | 1668 | `	if( nArg < 1 ){` |
|      - | 1669 | `		/* Missing argument,return NULL */` |
|      3 | 1670 | `		ph7_result_null(pCtx);` |
|      3 | 1671 | `		return PH7_OK;` |
|      - | 1672 | `	}` |
|      - | 1673 | `	/* Prepare the implode context */` |
|     11 | 1674 | `	imp_data.pCtx = pCtx;` |
|     11 | 1675 | `	imp_data.bRecursive = 1;` |
|     11 | 1676 | `	imp_data.bFirst = 1;` |
|     11 | 1677 | `	imp_data.nRecCount = 0;` |
|     11 | 1678 | `	imp_data.rc = SXRET_OK;` |
|     11 | 1679 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 1680 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 1681 | `	}else{` |
|    ! 0 | 1682 | `		imp_data.zSep = 0;` |
|    ! 0 | 1683 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 1684 | `		i = 0;` |
|      - | 1685 | `	}` |
|     11 | 1686 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 1687 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1688 | `	}` |
|      - | 1689 | `	/* Start the 'join' process */` |
|     21 | 1690 | `	while( i < nArg ){` |
|     11 | 1691 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1692 | `			/* Iterate throw array entries */` |
|      3 | 1693 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 1694 | `			/* Surface a callback allocation failure as a fatal */` |
|      3 | 1695 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 1696 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1697 | `			}` |
|      2 | 1698 | `		}else{` |
|      - | 1699 | `			const char *zData;` |
|      - | 1700 | `			int nLen;` |
|      - | 1701 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 1702 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1703 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 1704 | `			if( imp_data.bFirst ){` |
|      9 | 1705 | `				imp_data.bFirst = 0;` |
|      4 | 1706 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1707 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 1708 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1709 | `				}` |
|    ! 0 | 1710 | `			}` |
|      - | 1711 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 1712 | `			if( nLen > 0 ){` |
|      9 | 1713 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1714 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1715 | `				}` |
|      4 | 1716 | `			}` |
|      - | 1717 | `		}` |
|     11 | 1718 | `		i++;` |
|      1 | 1719 | `	}` |
|     11 | 1720 | `	return PH7_OK;` |
|      7 | 1721 |  |
|      - | 1722 | `/*` |
|      - | 1723 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 1724 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 1725 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 1726 | ` * Parameters` |
|      - | 1727 | ` *  $delimiter` |
|      - | 1728 | ` *   The boundary string.` |
|      - | 1729 | ` * $string` |
|      - | 1730 | ` *   The input string.` |
|      - | 1731 | ` * $limit` |
|      - | 1732 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 1733 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 1734 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 1735 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 1736 | ` * Returns` |
|      - | 1737 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 1738 | ` *  on boundaries formed by the delimiter.` |
|      - | 1739 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 1740 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 1741 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 1742 | ` *  will be returned.` |
|      - | 1743 | ` * NOTE:` |
|      - | 1744 | ` *  Negative limit is not supported.` |
|      - | 1745 | ` */` |
|   5940 | 1746 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1747 |  |
|      - | 1748 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 1749 | `	int nDelim,nStrlen,iLimit;` |
|      - | 1750 | `	ph7_value *pArray;` |
|      - | 1751 | `	ph7_value *pValue;` |
|      - | 1752 | `	sxu32 nOfft;` |
|      - | 1753 | `	sxi32 rc;` |
|   5945 | 1754 | `	if( nArg < 2 ){` |
|      - | 1755 | `		/* Missing arguments,return FALSE */` |
|      9 | 1756 | `		ph7_result_bool(pCtx,0);` |
|      9 | 1757 | `		return PH7_OK;` |
|      - | 1758 | `	}` |
|      - | 1759 | `	/* Extract the delimiter */` |
|   5937 | 1760 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   5937 | 1761 | `	if( nDelim < 1 ){` |
|      - | 1762 | `		/* Empty delimiter,return FALSE */` |
|      3 | 1763 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1764 | `		return PH7_OK;` |
|      - | 1765 | `	}` |
|      - | 1766 | `	/* Extract the string */` |
|   5935 | 1767 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   5935 | 1768 | `	if( nStrlen < 1 ){` |
|      - | 1769 | `		/* Empty string: return an array with a single empty element (PHP behavior) */` |
|      3 | 1770 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      3 | 1771 | `		ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      3 | 1772 | `		if( pArrayTmp == 0 \|\| pValueTmp == 0 ){` |
|      - | 1773 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 1774 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 1775 | `			return PH7_OK;` |
|      - | 1776 | `		}` |
|      3 | 1777 | `		ph7_value_string(pValueTmp, "", 0);` |
|      3 | 1778 | `		if( ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp) != SXRET_OK ){` |
|    ! 0 | 1779 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 1780 | `		}` |
|      3 | 1781 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      3 | 1782 | `		return PH7_OK;` |
|      - | 1783 | `	}` |
|      - | 1784 | `	/* Point to the end of the string */` |
|   5933 | 1785 | `	zEnd = &zString[nStrlen];` |
|      - | 1786 | `	/* Create the array */` |
|   5933 | 1787 | `	pArray =  ph7_context_new_array(pCtx);` |
|   5933 | 1788 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   5933 | 1789 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 1790 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 1791 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1792 | `		return PH7_OK;` |
|      - | 1793 | `	}` |
|      - | 1794 | `	/* Set a defualt limit */` |
|   5933 | 1795 | `	iLimit = SXI32_HIGH;` |
|   5933 | 1796 | `	if( nArg > 2 ){` |
|     11 | 1797 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     11 | 1798 | `		 if( iLimit < 0 ){` |
|      3 | 1799 | `			iLimit = -iLimit;` |
|      1 | 1800 | `		}` |
|     11 | 1801 | `		if( iLimit == 0 ){` |
|      3 | 1802 | `			iLimit = 1;` |
|      1 | 1803 | `		}` |
|     11 | 1804 | `		iLimit--;` |
|      5 | 1805 | `	}` |
|      - | 1806 | `	/* Start exploding */` |
|  68151 | 1807 | `	for(;;){` |
| 136307 | 1808 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 136307 | 1809 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 1810 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   5933 | 1811 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   5933 | 1812 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 1813 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1814 | `			}` |
|   5933 | 1815 | `			break;` |
|      - | 1816 | `		}` |
|      - | 1817 | `		/* Point to the desired offset */` |
| 130379 | 1818 | `		zCur = &zString[nOfft];` |
|      - | 1819 | `		/* Perform the store operation (may be empty) */` |
| 130379 | 1820 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 130379 | 1821 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 1822 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 1823 | `		}` |
|      - | 1824 | `		/* Point beyond the delimiter */` |
| 130379 | 1825 | `		zString = &zCur[nDelim];` |
|      - | 1826 | `		/* Reset the cursor */` |
| 130379 | 1827 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 1828 | `	}` |
|      - | 1829 | `	/* Return the freshly created array */` |
|   5933 | 1830 | `	ph7_result_value(pCtx,pArray);` |
|      - | 1831 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 1832 | `	 * released as soon we return from this foregin function.` |
|      - | 1833 | `	 */` |
|   5933 | 1834 | `	return PH7_OK;` |
|   2975 | 1835 |  |
|      - | 1836 | `/*` |
|      - | 1837 | ` * string trim(string $str[,string $charlist ])` |
|      - | 1838 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 1839 | ` * Parameters` |
|      - | 1840 | ` *  $str` |
|      - | 1841 | ` *   The string that will be trimmed.` |
|      - | 1842 | ` * $charlist` |
|      - | 1843 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1844 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1845 | ` *   With .. you can specify a range of characters.` |
|      - | 1846 | ` * Returns.` |
|      - | 1847 | ` *  Thr processed string.` |
|      - | 1848 | ` * NOTE:` |
|      - | 1849 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 1850 | ` */` |
|  13584 | 1851 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1852 |  |
|      - | 1853 | `	const char *zString;` |
|      - | 1854 | `	int nLen;` |
|  13589 | 1855 | `	if( nArg < 1 ){` |
|      - | 1856 | `		/* Missing arguments,return null */` |
|      3 | 1857 | `		ph7_result_null(pCtx);` |
|      3 | 1858 | `		return PH7_OK;` |
|      - | 1859 | `	}` |
|      - | 1860 | `	/* Extract the target string */` |
|  13587 | 1861 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  13587 | 1862 | `	if( nLen < 1 ){` |
|      - | 1863 | `		/* Empty string,return */` |
|   1723 | 1864 | `		ph7_result_string(pCtx,"",0);` |
|   1723 | 1865 | `		return PH7_OK;` |
|      - | 1866 | `	}` |
|      - | 1867 | `	/* Start the trim process */` |
|  11869 | 1868 | `	if( nArg < 2 ){` |
|      - | 1869 | `		SyString sStr;` |
|      - | 1870 | `		/* Remove white spaces and NUL bytes */` |
|  11865 | 1871 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  29015 | 1872 | `		SyStringFullTrimSafe(&sStr);` |
|  11865 | 1873 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   5935 | 1874 | `	}else{` |
|      - | 1875 | `		/* Char list */` |
|      - | 1876 | `		const char *zList;` |
|      - | 1877 | `		int nListlen;` |
|      5 | 1878 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 1879 | `		if( nListlen < 1 ){` |
|      - | 1880 | `			/* Return the string unchanged */` |
|      3 | 1881 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 1882 | `		}else{` |
|      3 | 1883 | `			const char *zEnd = &zString[nLen];` |
|      3 | 1884 | `			const char *zCur = zString;` |
|      - | 1885 | `			const char *zPtr;` |
|      - | 1886 | `			int i;` |
|      - | 1887 | `			/* Left trim */` |
|      4 | 1888 | `			for(;;){` |
|      9 | 1889 | `				if( zCur >= zEnd ){` |
|    ! 0 | 1890 | `					break;` |
|      - | 1891 | `				}` |
|      9 | 1892 | `				zPtr = zCur;` |
|     17 | 1893 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 1894 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|      7 | 1895 | `						zCur++;` |
|      3 | 1896 | `					}` |
|      5 | 1897 | `				}` |
|      9 | 1898 | `				if( zCur == zPtr ){` |
|      - | 1899 | `					/* No match,break immediately */` |
|      3 | 1900 | `					break;` |
|      - | 1901 | `				}` |
|      1 | 1902 | `			}` |
|      - | 1903 | `			/* Right trim */` |
|      3 | 1904 | `			zEnd--;` |
|      4 | 1905 | `			for(;;){` |
|      9 | 1906 | `				if( zEnd <= zCur ){` |
|    ! 0 | 1907 | `					break;` |
|      - | 1908 | `				}` |
|      9 | 1909 | `				zPtr = zEnd;` |
|     17 | 1910 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 1911 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      7 | 1912 | `						zEnd--;` |
|      3 | 1913 | `					}` |
|      5 | 1914 | `				}` |
|      9 | 1915 | `				if( zEnd == zPtr ){` |
|      3 | 1916 | `					break;` |
|      - | 1917 | `				}` |
|      1 | 1918 | `			}` |
|      3 | 1919 | `			if( zCur >= zEnd ){` |
|      - | 1920 | `				/* Return the empty string */` |
|    ! 0 | 1921 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1922 | `			}else{` |
|      3 | 1923 | `				zEnd++;` |
|      3 | 1924 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 1925 | `			}` |
|      - | 1926 | `		}` |
|      - | 1927 | `	}` |
|  11869 | 1928 | `	return PH7_OK;` |
|   6797 | 1929 |  |
|      - | 1930 | `/*` |
|      - | 1931 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 1932 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 1933 | ` * Parameters` |
|      - | 1934 | ` *  $str` |
|      - | 1935 | ` *   The string that will be trimmed.` |
|      - | 1936 | ` * $charlist` |
|      - | 1937 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1938 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1939 | ` *   With .. you can specify a range of characters.` |
|      - | 1940 | ` * Returns.` |
|      - | 1941 | ` *  Thr processed string.` |
|      - | 1942 | ` * NOTE:` |
|      - | 1943 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 1944 | ` */` |
|     26 | 1945 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1946 |  |
|      - | 1947 | `	const char *zString;` |
|      - | 1948 | `	int nLen;` |
|     27 | 1949 | `	if( nArg < 1 ){` |
|      - | 1950 | `		/* Missing arguments,return null */` |
|      3 | 1951 | `		ph7_result_null(pCtx);` |
|      3 | 1952 | `		return PH7_OK;` |
|      - | 1953 | `	}` |
|      - | 1954 | `	/* Extract the target string */` |
|     25 | 1955 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 1956 | `	if( nLen < 1 ){` |
|      - | 1957 | `		/* Empty string,return */` |
|      5 | 1958 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1959 | `		return PH7_OK;` |
|      - | 1960 | `	}` |
|      - | 1961 | `	/* Start the trim process */` |
|     21 | 1962 | `	if( nArg < 2 ){` |
|      - | 1963 | `		SyString sStr;` |
|      - | 1964 | `		/* Remove white spaces and NUL bytes*/` |
|     17 | 1965 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     41 | 1966 | `		SyStringRightTrimSafe(&sStr);` |
|     17 | 1967 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      9 | 1968 | `	}else{` |
|      - | 1969 | `		/* Char list */` |
|      - | 1970 | `		const char *zList;` |
|      - | 1971 | `		int nListlen;` |
|      5 | 1972 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 1973 | `		if( nListlen < 1 ){` |
|      - | 1974 | `			/* Return the string unchanged */` |
|    ! 0 | 1975 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 1976 | `		}else{` |
|      5 | 1977 | `			const char *zEnd = &zString[nLen - 1];` |
|      5 | 1978 | `			const char *zCur = zString;` |
|      - | 1979 | `			const char *zPtr;` |
|      - | 1980 | `			int i;` |
|      - | 1981 | `			/* Right trim */` |
|      6 | 1982 | `			for(;;){` |
|     13 | 1983 | `				if( zEnd <= zCur ){` |
|    ! 0 | 1984 | `					break;` |
|      - | 1985 | `				}` |
|     13 | 1986 | `				zPtr = zEnd;` |
|     25 | 1987 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     13 | 1988 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      9 | 1989 | `						zEnd--;` |
|      4 | 1990 | `					}` |
|      7 | 1991 | `				}` |
|     13 | 1992 | `				if( zEnd == zPtr ){` |
|      5 | 1993 | `					break;` |
|      - | 1994 | `				}` |
|      1 | 1995 | `			}` |
|      5 | 1996 | `			if( zEnd <= zCur ){` |
|      - | 1997 | `				/* Return the empty string */` |
|    ! 0 | 1998 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1999 | `			}else{` |
|      5 | 2000 | `				zEnd++;` |
|      5 | 2001 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2002 | `			}` |
|      - | 2003 | `		}` |
|      - | 2004 | `	}` |
|     21 | 2005 | `	return PH7_OK;` |
|     14 | 2006 |  |
|      - | 2007 | `/*` |
|      - | 2008 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 2009 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2010 | ` * Parameters` |
|      - | 2011 | ` *  $str` |
|      - | 2012 | ` *   The string that will be trimmed.` |
|      - | 2013 | ` * $charlist` |
|      - | 2014 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2015 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2016 | ` *   With .. you can specify a range of characters.` |
|      - | 2017 | ` * Returns.` |
|      - | 2018 | ` *  Thr processed string.` |
|      - | 2019 | ` * NOTE:` |
|      - | 2020 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 2021 | ` */` |
|     12 | 2022 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2023 |  |
|      - | 2024 | `	const char *zString;` |
|      - | 2025 | `	int nLen;` |
|     13 | 2026 | `	if( nArg < 1 ){` |
|      - | 2027 | `		/* Missing arguments,return null */` |
|      3 | 2028 | `		ph7_result_null(pCtx);` |
|      3 | 2029 | `		return PH7_OK;` |
|      - | 2030 | `	}` |
|      - | 2031 | `	/* Extract the target string */` |
|     11 | 2032 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 2033 | `	if( nLen < 1 ){` |
|      - | 2034 | `		/* Empty string,return */` |
|    ! 0 | 2035 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2036 | `		return PH7_OK;` |
|      - | 2037 | `	}` |
|      - | 2038 | `	/* Start the trim process */` |
|     11 | 2039 | `	if( nArg < 2 ){` |
|      - | 2040 | `		SyString sStr;` |
|      - | 2041 | `		/* Remove white spaces and NUL byte */` |
|      3 | 2042 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|      8 | 2043 | `		SyStringLeftTrimSafe(&sStr);` |
|      3 | 2044 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      2 | 2045 | `	}else{` |
|      - | 2046 | `		/* Char list */` |
|      - | 2047 | `		const char *zList;` |
|      - | 2048 | `		int nListlen;` |
|      9 | 2049 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      9 | 2050 | `		if( nListlen < 1 ){` |
|      - | 2051 | `			/* Return the string unchanged */` |
|      3 | 2052 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2053 | `		}else{` |
|      7 | 2054 | `			const char *zEnd = &zString[nLen];` |
|      7 | 2055 | `			const char *zCur = zString;` |
|      - | 2056 | `			const char *zPtr;` |
|      - | 2057 | `			int i;` |
|      - | 2058 | `			/* Left trim */` |
|      7 | 2059 | `			for(;;){` |
|     15 | 2060 | `				if( zCur >= zEnd ){` |
|    ! 0 | 2061 | `					break;` |
|      - | 2062 | `				}` |
|     15 | 2063 | `				zPtr = zCur;` |
|     41 | 2064 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     27 | 2065 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|     13 | 2066 | `						zCur++;` |
|      6 | 2067 | `					}` |
|     14 | 2068 | `				}` |
|     15 | 2069 | `				if( zCur == zPtr ){` |
|      - | 2070 | `					/* No match,break immediately */` |
|      7 | 2071 | `					break;` |
|      - | 2072 | `				}` |
|      1 | 2073 | `			}` |
|      7 | 2074 | `			if( zCur >= zEnd ){` |
|      - | 2075 | `				/* Return the empty string */` |
|    ! 0 | 2076 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2077 | `			}else{` |
|      7 | 2078 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2079 | `			}` |
|      - | 2080 | `		}` |
|      - | 2081 | `	}` |
|     11 | 2082 | `	return PH7_OK;` |
|      7 | 2083 |  |
|      - | 2084 | `/*` |
|      - | 2085 | ` * string strtolower(string $str)` |
|      - | 2086 | ` *  Make a string lowercase.` |
|      - | 2087 | ` * Parameters` |
|      - | 2088 | ` *  $str` |
|      - | 2089 | ` *   The input string.` |
|      - | 2090 | ` * Returns.` |
|      - | 2091 | ` *  The lowercased string.` |
|      - | 2092 | ` */` |
|  31178 | 2093 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2094 |  |
|      - | 2095 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2096 | `	int nLen;` |
|  31183 | 2097 | `	if( nArg < 1 ){` |
|      - | 2098 | `		/* Missing arguments,return null */` |
|      3 | 2099 | `		ph7_result_null(pCtx);` |
|      3 | 2100 | `		return PH7_OK;` |
|      - | 2101 | `	}` |
|      - | 2102 | `	/* Extract the target string */` |
|  31181 | 2103 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  31181 | 2104 | `	if( nLen < 1 ){` |
|      - | 2105 | `		/* Empty string,return */` |
|      3 | 2106 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2107 | `		return PH7_OK;` |
|      - | 2108 | `	}` |
|      - | 2109 | `	/* Perform the requested operation */` |
|  31179 | 2110 | `	zEnd = &zString[nLen];` |
|  98247 | 2111 | `	for(;;){` |
| 196499 | 2112 | `		if( zString >= zEnd ){` |
|      - | 2113 | `			/* No more input,break immediately */` |
|  31179 | 2114 | `			break;` |
|      - | 2115 | `		}` |
| 165325 | 2116 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2117 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2118 | `			zCur = zString;` |
|    ! 0 | 2119 | `			zString++;` |
|    ! 0 | 2120 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2121 | `				zString++;` |
|    ! 0 | 2122 | `			}` |
|      - | 2123 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2124 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2125 | `		}else{` |
| 165325 | 2126 | `			int c = zString[0];` |
| 165325 | 2127 | `			if( SyisUpper(c) ){` |
| 165323 | 2128 | `				c = SyToLower(zString[0]);` |
|  82659 | 2129 | `			}` |
|      - | 2130 | `			/* Append character */` |
| 165325 | 2131 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2132 | `			/* Advance the cursor */` |
| 165325 | 2133 | `			zString++;` |
|      - | 2134 | `		}` |
|      5 | 2135 | `	}` |
|  31179 | 2136 | `	return PH7_OK;` |
|  15594 | 2137 |  |
|      - | 2138 | `/*` |
|      - | 2139 | ` * string strtolower(string $str)` |
|      - | 2140 | ` *  Make a string uppercase.` |
|      - | 2141 | ` * Parameters` |
|      - | 2142 | ` *  $str` |
|      - | 2143 | ` *   The input string.` |
|      - | 2144 | ` * Returns.` |
|      - | 2145 | ` *  The uppercased string.` |
|      - | 2146 | ` */` |
|     34 | 2147 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2148 |  |
|      - | 2149 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2150 | `	int nLen;` |
|     38 | 2151 | `	if( nArg < 1 ){` |
|      - | 2152 | `		/* Missing arguments,return null */` |
|      3 | 2153 | `		ph7_result_null(pCtx);` |
|      3 | 2154 | `		return PH7_OK;` |
|      - | 2155 | `	}` |
|      - | 2156 | `	/* Extract the target string */` |
|     36 | 2157 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 2158 | `	if( nLen < 1 ){` |
|      - | 2159 | `		/* Empty string,return */` |
|      3 | 2160 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2161 | `		return PH7_OK;` |
|      - | 2162 | `	}` |
|      - | 2163 | `	/* Perform the requested operation */` |
|     34 | 2164 | `	zEnd = &zString[nLen];` |
|     88 | 2165 | `	for(;;){` |
|    180 | 2166 | `		if( zString >= zEnd ){` |
|      - | 2167 | `			/* No more input,break immediately */` |
|     34 | 2168 | `			break;` |
|      - | 2169 | `		}` |
|    150 | 2170 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2171 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2172 | `			zCur = zString;` |
|    ! 0 | 2173 | `			zString++;` |
|    ! 0 | 2174 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2175 | `				zString++;` |
|    ! 0 | 2176 | `			}` |
|      - | 2177 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2178 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2179 | `		}else{` |
|    150 | 2180 | `			int c = zString[0];` |
|    150 | 2181 | `			if( SyisLower(c) ){` |
|    144 | 2182 | `				c = SyToUpper(zString[0]);` |
|     70 | 2183 | `			}` |
|      - | 2184 | `			/* Append character */` |
|    150 | 2185 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2186 | `			/* Advance the cursor */` |
|    150 | 2187 | `			zString++;` |
|      - | 2188 | `		}` |
|      4 | 2189 | `	}` |
|     34 | 2190 | `	return PH7_OK;` |
|     21 | 2191 |  |
|      - | 2192 | `/*` |
|      - | 2193 | ` * string ucfirst(string $str)` |
|      - | 2194 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2195 | ` *  character is alphabetic.` |
|      - | 2196 | ` * Parameters` |
|      - | 2197 | ` *  $str` |
|      - | 2198 | ` *   The input string.` |
|      - | 2199 | ` * Returns.` |
|      - | 2200 | ` *  The processed string.` |
|      - | 2201 | ` */` |
|      6 | 2202 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2203 |  |
|      - | 2204 | `	const char *zString,*zEnd;` |
|      - | 2205 | `	int nLen,c;` |
|      7 | 2206 | `	if( nArg < 1 ){` |
|      - | 2207 | `		/* Missing arguments,return null */` |
|      3 | 2208 | `		ph7_result_null(pCtx);` |
|      3 | 2209 | `		return PH7_OK;` |
|      - | 2210 | `	}` |
|      - | 2211 | `	/* Extract the target string */` |
|      5 | 2212 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2213 | `	if( nLen < 1 ){` |
|      - | 2214 | `		/* Empty string,return */` |
|      3 | 2215 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2216 | `		return PH7_OK;` |
|      - | 2217 | `	}` |
|      - | 2218 | `	/* Perform the requested operation */` |
|      3 | 2219 | `	zEnd = &zString[nLen];` |
|      3 | 2220 | `	c = zString[0];` |
|      3 | 2221 | `	if( SyisLower(c) ){` |
|      3 | 2222 | `		c = SyToUpper(c);` |
|      1 | 2223 | `	}` |
|      - | 2224 | `	/* Append the first character */` |
|      3 | 2225 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2226 | `	zString++;` |
|      3 | 2227 | `	if( zString < zEnd ){` |
|      - | 2228 | `		/* Append the rest of the input verbatim */` |
|      3 | 2229 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2230 | `	}` |
|      3 | 2231 | `	return PH7_OK;` |
|      4 | 2232 |  |
|      - | 2233 | `/*` |
|      - | 2234 | ` * string lcfirst(string $str)` |
|      - | 2235 | ` *  Make a string's first character lowercase.` |
|      - | 2236 | ` * Parameters` |
|      - | 2237 | ` *  $str` |
|      - | 2238 | ` *   The input string.` |
|      - | 2239 | ` * Returns.` |
|      - | 2240 | ` *  The processed string.` |
|      - | 2241 | ` */` |
|      6 | 2242 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2243 |  |
|      - | 2244 | `	const char *zString,*zEnd;` |
|      - | 2245 | `	int nLen,c;` |
|      7 | 2246 | `	if( nArg < 1 ){` |
|      - | 2247 | `		/* Missing arguments,return null */` |
|      3 | 2248 | `		ph7_result_null(pCtx);` |
|      3 | 2249 | `		return PH7_OK;` |
|      - | 2250 | `	}` |
|      - | 2251 | `	/* Extract the target string */` |
|      5 | 2252 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2253 | `	if( nLen < 1 ){` |
|      - | 2254 | `		/* Empty string,return */` |
|      3 | 2255 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2256 | `		return PH7_OK;` |
|      - | 2257 | `	}` |
|      - | 2258 | `	/* Perform the requested operation */` |
|      3 | 2259 | `	zEnd = &zString[nLen];` |
|      3 | 2260 | `	c = zString[0];` |
|      3 | 2261 | `	if( SyisUpper(c) ){` |
|      3 | 2262 | `		c = SyToLower(c);` |
|      1 | 2263 | `	}` |
|      - | 2264 | `	/* Append the first character */` |
|      3 | 2265 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2266 | `	zString++;` |
|      3 | 2267 | `	if( zString < zEnd ){` |
|      - | 2268 | `		/* Append the rest of the input verbatim */` |
|      3 | 2269 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2270 | `	}` |
|      3 | 2271 | `	return PH7_OK;` |
|      4 | 2272 |  |
|      - | 2273 | `/*` |
|      - | 2274 | ` * int ord(string $string)` |
|      - | 2275 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 2276 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 2277 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 2278 | ` * Parameters` |
|      - | 2279 | ` *  $string` |
|      - | 2280 | ` *   The input string.` |
|      - | 2281 | ` * Returns` |
|      - | 2282 | ` *  The ASCII value as an integer.` |
|      - | 2283 | ` */` |
|     62 | 2284 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2285 |  |
|      - | 2286 | `	const char *zString;` |
|      - | 2287 | `	int nLen,c;` |
|      - | 2288 | `	/* PHP requires exactly one argument. */` |
|     65 | 2289 | `	if( nArg != 1 ){` |
|      8 | 2290 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2291 | `			"ArgumentCountError",` |
|      - | 2292 | `			"ord() expects exactly 1 argument, %d given",` |
|      2 | 2293 | `			nArg` |
|      - | 2294 | `			);` |
|      - | 2295 | `	}` |
|      - | 2296 | `	/* Passing null is deprecated (E_DEPRECATED).  PHP emits this before` |
|      - | 2297 | `	 * the empty-string deprecation, so we check null first. */` |
|     59 | 2298 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 2299 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2300 | `			"ord(): Passing null to parameter #1 ($character) "` |
|      - | 2301 | `			"of type string is deprecated"` |
|      - | 2302 | `			);` |
|      1 | 2303 | `	}` |
|      - | 2304 | `	/* Extract the target string */` |
|     59 | 2305 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     59 | 2306 | `	if( nLen < 1 ){` |
|      - | 2307 | `		/* Empty string is deprecated (E_DEPRECATED). */` |
|      5 | 2308 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2309 | `			"ord(): Providing an empty string is deprecated"` |
|      - | 2310 | `			);` |
|      5 | 2311 | `		ph7_result_int(pCtx,0);` |
|      5 | 2312 | `		return PH7_OK;` |
|      - | 2313 | `	}` |
|      - | 2314 | `	/* A string longer than one byte is deprecated (E_DEPRECATED). */` |
|     55 | 2315 | `	if( nLen > 1 ){` |
|      7 | 2316 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2317 | `			"ord(): Providing a string that is not one byte long "` |
|      - | 2318 | `			"is deprecated. Use ord($str[0]) instead"` |
|      - | 2319 | `			);` |
|      3 | 2320 | `	}` |
|      - | 2321 | `	/* Extract the ASCII value of the first character */` |
|     55 | 2322 | `	c = (unsigned char)zString[0];` |
|      - | 2323 | `	/* Return that value */` |
|     55 | 2324 | `	ph7_result_int(pCtx,c);` |
|     55 | 2325 | `	return PH7_OK;` |
|     34 | 2326 |  |
|      - | 2327 | `/*` |
|      - | 2328 | ` * string chr(int $codepoint)` |
|      - | 2329 | ` *  Returns a one-character string containing the character specified` |
|      - | 2330 | ` *  by the given codepoint.  Any integer is accepted; values outside` |
|      - | 2331 | ` *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.` |
|      - | 2332 | ` * Parameters` |
|      - | 2333 | ` *  $codepoint` |
|      - | 2334 | ` *   An integer codepoint.  Values outside 0-255 are deprecated and` |
|      - | 2335 | ` *   will be constrained to a single byte.` |
|      - | 2336 | ` * Returns` |
|      - | 2337 | ` *  A single-character string.` |
|      - | 2338 | ` */` |
|     48 | 2339 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2340 |  |
|      - | 2341 | `	int c;` |
|      - | 2342 | `	unsigned char ch;` |
|      - | 2343 | `	/* PHP requires exactly one argument. */` |
|     51 | 2344 | `	if( nArg != 1 ){` |
|      8 | 2345 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2346 | `			"ArgumentCountError",` |
|      - | 2347 | `			"chr() expects exactly 1 argument, %d given",` |
|      2 | 2348 | `			nArg` |
|      - | 2349 | `			);` |
|      - | 2350 | `	}` |
|      - | 2351 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 2352 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 2353 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 2354 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|     45 | 2355 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      - | 2356 | `		char zBuf[120];` |
|      4 | 2357 | `		SyBufferFormat(zBuf,sizeof(zBuf),` |
|      - | 2358 | `			"Implicit conversion from float %g to int loses precision",` |
|      1 | 2359 | `			ph7_value_to_double(apArg[0])` |
|      - | 2360 | `			);` |
|      3 | 2361 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zBuf);` |
|      1 | 2362 | `	}` |
|      - | 2363 | `	/* Extract the codepoint. */` |
|     45 | 2364 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 2365 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 2366 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 2367 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 2368 | `	 * name to avoid the API double-prefixing it. */` |
|     45 | 2369 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 2370 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 2371 | `			E_DEPRECATED,` |
|      - | 2372 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 2373 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 2374 | `			"The value used will be constrained using % 256"` |
|      - | 2375 | `			);` |
|      2 | 2376 | `	}` |
|      - | 2377 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 2378 | `	 * when taking the address of a wider int. */` |
|     45 | 2379 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 2380 | `	/* Return the specified character */` |
|     45 | 2381 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|     45 | 2382 | `	return PH7_OK;` |
|     27 | 2383 |  |
|      - | 2384 | `/*` |
|      - | 2385 | ` * Binary to hex consumer callback.` |
|      - | 2386 | ` * This callback is the default consumer used by the hash functions` |
|      - | 2387 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 2388 | ` */` |
|   2330 | 2389 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 2390 |  |
|      - | 2391 | `	/* Append hex chunk verbatim */` |
|   2331 | 2392 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   2331 | 2393 | `	return SXRET_OK;` |
|      1 | 2394 |  |
|      - | 2395 |  |
|      - | 2396 | `/*` |
|      - | 2397 | ` * string bin2hex(string $str)` |
|      - | 2398 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 2399 | ` * Parameters` |
|      - | 2400 | ` *  $str` |
|      - | 2401 | ` *   The input string.` |
|      - | 2402 | ` * Returns.` |
|      - | 2403 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 2404 | ` */` |
|     24 | 2405 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2406 |  |
|      - | 2407 | `	const char *zString;` |
|      - | 2408 | `	int nLen;` |
|      - | 2409 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|     29 | 2410 | `	if( nArg != 1 ){` |
|      8 | 2411 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2412 | `			"ArgumentCountError",` |
|      - | 2413 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|      2 | 2414 | `			nArg` |
|      - | 2415 | `			);` |
|      - | 2416 | `	}` |
|      - | 2417 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 2418 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 2419 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 2420 | `	 */` |
|     33 | 2421 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|     16 | 2422 | `		( ph7_value_is_object(apArg[0]) &&` |
|      3 | 2423 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|      2 | 2424 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|      1 | 2425 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 2426 | `		)` |
|      - | 2427 | `	){` |
|      9 | 2428 | `		const char *zType = ph7_type_name(apArg[0]);` |
|      9 | 2429 | `		if( ph7_value_is_object(apArg[0]) ){` |
|      3 | 2430 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      3 | 2431 | `			if( pInst && pInst->pClass ){` |
|      3 | 2432 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      1 | 2433 | `			}` |
|      1 | 2434 | `		}` |
|     12 | 2435 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2436 | `			"TypeError",` |
|      - | 2437 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|      3 | 2438 | `			zType` |
|      - | 2439 | `			);` |
|      - | 2440 | `	}` |
|      - | 2441 | `	/* Extract the target string */` |
|     15 | 2442 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 2443 | `	if( nLen < 1 ){` |
|      - | 2444 | `		/* Empty string,return */` |
|      3 | 2445 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2446 | `		return PH7_OK;` |
|      - | 2447 | `	}` |
|      - | 2448 | `	/* Perform the requested operation */` |
|     13 | 2449 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|     13 | 2450 | `	return PH7_OK;` |
|     17 | 2451 |  |
|      - | 2452 |  |
|      - | 2453 | `/* Search callback signature */` |
|      - | 2454 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 2455 | `/*` |
|      - | 2456 | ` * Case-insensitive pattern match.` |
|      - | 2457 | ` * Brute force is the default search method used here.` |
|      - | 2458 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 2459 | ` * well for short/medium texts on modern hardware.` |
|      - | 2460 | ` */` |
|    118 | 2461 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      1 | 2462 |  |
|    119 | 2463 | `	const char *zpIn = (const char *)pPattern;` |
|    119 | 2464 | `	const char *zIn = (const char *)pText;` |
|    119 | 2465 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    119 | 2466 | `	const char *zEnd = &zIn[nLen];` |
|      - | 2467 | `	const char *zPtr,*zPtr2;` |
|      - | 2468 | `	int c,d;` |
|    119 | 2469 | `	if( iPatLen > nLen ){` |
|      - | 2470 | `		/* Don't bother processing */` |
|     33 | 2471 | `		return SXERR_NOTFOUND;` |
|      - | 2472 | `	}` |
|    242 | 2473 | `	for(;;){` |
|    485 | 2474 | `		if( zIn >= zEnd ){` |
|     47 | 2475 | `			break;` |
|      - | 2476 | `		}` |
|    439 | 2477 | `		c = SyToLower(zIn[0]);` |
|    439 | 2478 | `		d = SyToLower(zpIn[0]);` |
|    439 | 2479 | `		if( c == d ){` |
|     41 | 2480 | `			zPtr   = &zIn[1];` |
|     41 | 2481 | `			zPtr2  = &zpIn[1];` |
|     71 | 2482 | `			for(;;){` |
|    143 | 2483 | `				if( zPtr2 >= zpEnd ){` |
|      - | 2484 | `					/* Pattern found */` |
|     41 | 2485 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 2486 | `					return SXRET_OK;` |
|      - | 2487 | `				}` |
|    103 | 2488 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 2489 | `					break;` |
|      - | 2490 | `				}` |
|    103 | 2491 | `				c = SyToLower(zPtr[0]);` |
|    103 | 2492 | `				d = SyToLower(zPtr2[0]);` |
|    103 | 2493 | `				if( c != d ){` |
|    ! 0 | 2494 | `					break;` |
|      - | 2495 | `				}` |
|    103 | 2496 | `				zPtr++; zPtr2++;` |
|      1 | 2497 | `			}` |
|    ! 0 | 2498 | `		}` |
|    399 | 2499 | `		zIn++;` |
|      1 | 2500 | `	}` |
|      - | 2501 | `	/* Pattern not found */` |
|     47 | 2502 | `	return SXERR_NOTFOUND;` |
|     60 | 2503 |  |
|      - | 2504 | `/*` |
|      - | 2505 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2506 | ` *  Find the first occurrence of a string.` |
|      - | 2507 | ` * Parameters` |
|      - | 2508 | ` *  $haystack` |
|      - | 2509 | ` *   The input string.` |
|      - | 2510 | ` * $needle` |
|      - | 2511 | ` *   Search pattern (must be a string).` |
|      - | 2512 | ` * $before_needle` |
|      - | 2513 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2514 | ` *   of the needle (excluding the needle).` |
|      - | 2515 | ` * Return` |
|      - | 2516 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2517 | ` */` |
|     10 | 2518 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2519 |  |
|     11 | 2520 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2521 | `	const char *zBlob,*zPattern;` |
|      - | 2522 | `	int nLen,nPatLen;` |
|      - | 2523 | `	sxu32 nOfft;` |
|      - | 2524 | `	sxi32 rc;` |
|     11 | 2525 | `	if( nArg < 2 ){` |
|      - | 2526 | `		/* Missing arguments,return FALSE */` |
|      5 | 2527 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2528 | `		return PH7_OK;` |
|      - | 2529 | `	}` |
|      - | 2530 | `	/* Extract the needle and the haystack */` |
|      7 | 2531 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 2532 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 2533 | `	nOfft = 0; /* cc warning */` |
|      9 | 2534 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 2535 | `		int before = 0;` |
|      - | 2536 | `		/* Perform the lookup */` |
|      5 | 2537 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2538 | `		if( rc != SXRET_OK ){` |
|      - | 2539 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2540 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2541 | `			return PH7_OK;` |
|      - | 2542 | `		}` |
|      - | 2543 | `		/* Return the portion of the string */` |
|      5 | 2544 | `		if( nArg > 2 ){` |
|      3 | 2545 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2546 | `		}` |
|      5 | 2547 | `		if( before ){` |
|      3 | 2548 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2549 | `		}else{` |
|      3 | 2550 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2551 | `		}` |
|      3 | 2552 | `	}else{` |
|      3 | 2553 | `		ph7_result_bool(pCtx,0);` |
|      - | 2554 | `	}` |
|      7 | 2555 | `	return PH7_OK;` |
|      6 | 2556 |  |
|      - | 2557 | `/*` |
|      - | 2558 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2559 | ` *  Case-insensitive strstr().` |
|      - | 2560 | ` * Parameters` |
|      - | 2561 | ` *  $haystack` |
|      - | 2562 | ` *   The input string.` |
|      - | 2563 | ` * $needle` |
|      - | 2564 | ` *   Search pattern (must be a string).` |
|      - | 2565 | ` * $before_needle` |
|      - | 2566 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2567 | ` *   of the needle (excluding the needle).` |
|      - | 2568 | ` * Return` |
|      - | 2569 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2570 | ` */` |
|      6 | 2571 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2572 |  |
|      7 | 2573 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2574 | `	const char *zBlob,*zPattern;` |
|      - | 2575 | `	int nLen,nPatLen;` |
|      - | 2576 | `	sxu32 nOfft;` |
|      - | 2577 | `	sxi32 rc;` |
|      7 | 2578 | `	if( nArg < 2 ){` |
|      - | 2579 | `		/* Missing arguments,return FALSE */` |
|      3 | 2580 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2581 | `		return PH7_OK;` |
|      - | 2582 | `	}` |
|      - | 2583 | `	/* Extract the needle and the haystack */` |
|      5 | 2584 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2585 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 2586 | `	nOfft = 0; /* cc warning */` |
|      7 | 2587 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 2588 | `		int before = 0;` |
|      - | 2589 | `		/* Perform the lookup */` |
|      5 | 2590 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2591 | `		if( rc != SXRET_OK ){` |
|      - | 2592 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2593 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2594 | `			return PH7_OK;` |
|      - | 2595 | `		}` |
|      - | 2596 | `		/* Return the portion of the string */` |
|      5 | 2597 | `		if( nArg > 2 ){` |
|      3 | 2598 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2599 | `		}` |
|      5 | 2600 | `		if( before ){` |
|      3 | 2601 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2602 | `		}else{` |
|      3 | 2603 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2604 | `		}` |
|      3 | 2605 | `	}else{` |
|    ! 0 | 2606 | `		ph7_result_bool(pCtx,0);` |
|      - | 2607 | `	}` |
|      5 | 2608 | `	return PH7_OK;` |
|      4 | 2609 |  |
|      - | 2610 | `/*` |
|      - | 2611 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2612 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 2613 | ` * Parameters` |
|      - | 2614 | ` *  $haystack` |
|      - | 2615 | ` *   The input string.` |
|      - | 2616 | ` * $needle` |
|      - | 2617 | ` *   Search pattern (must be a string).` |
|      - | 2618 | ` * $offset` |
|      - | 2619 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 2620 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 2621 | ` *   of haystack.` |
|      - | 2622 | ` * Return` |
|      - | 2623 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 2624 | ` */` |
|    122 | 2625 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2626 |  |
|    127 | 2627 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2628 | `	const char *zBlob,*zPattern;` |
|      - | 2629 | `	int nLen,nPatLen,nStart;` |
|      - | 2630 | `	sxu32 nOfft;` |
|      - | 2631 | `	sxi32 rc;` |
|    127 | 2632 | `	if( nArg < 2 ){` |
|      - | 2633 | `		/* Missing arguments,return FALSE */` |
|      7 | 2634 | `		ph7_result_bool(pCtx,0);` |
|      7 | 2635 | `		return PH7_OK;` |
|      - | 2636 | `	}` |
|      - | 2637 | `	/* Extract the needle and the haystack */` |
|    121 | 2638 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    121 | 2639 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    121 | 2640 | `	nOfft = 0; /* cc warning */` |
|    121 | 2641 | `	nStart = 0;` |
|      - | 2642 | `	/* Peek the starting offset if available */` |
|    121 | 2643 | `	if( nArg > 2 ){` |
|    ! 0 | 2644 | `		nStart = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 2645 | `		if( nStart < 0 ){` |
|    ! 0 | 2646 | `			nStart = -nStart;` |
|    ! 0 | 2647 | `		}` |
|    ! 0 | 2648 | `		if( nStart >= nLen ){` |
|      - | 2649 | `			/* Invalid offset */` |
|    ! 0 | 2650 | `			nStart = 0;` |
|    ! 0 | 2651 | `		}else{` |
|    ! 0 | 2652 | `			zBlob += nStart;` |
|    ! 0 | 2653 | `			nLen -= nStart;` |
|      - | 2654 | `		}` |
|    ! 0 | 2655 | `	}` |
|    121 | 2656 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2657 | `		/* Perform the lookup */` |
|    119 | 2658 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    119 | 2659 | `		if( rc != SXRET_OK ){` |
|      - | 2660 | `			/* Pattern not found,return FALSE */` |
|     33 | 2661 | `			ph7_result_bool(pCtx,0);` |
|     33 | 2662 | `			return PH7_OK;` |
|      - | 2663 | `		}` |
|      - | 2664 | `		/* Return the pattern position */` |
|     88 | 2665 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|     45 | 2666 | `	}else{` |
|      3 | 2667 | `		ph7_result_bool(pCtx,0);` |
|      - | 2668 | `	}` |
|     90 | 2669 | `	return PH7_OK;` |
|     66 | 2670 |  |
|      - | 2671 | `/*` |
|      - | 2672 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|      - | 2673 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|      - | 2674 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|      - | 2675 | ` * TypeError for arrays, resources, and objects without __toString.` |
|      - | 2676 | ` *` |
|      - | 2677 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|      - | 2678 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|      - | 2679 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|      - | 2680 | ` *` |
|      - | 2681 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|      - | 2682 | ` * is valid until pTmp is released or pArg is mutated.` |
|      - | 2683 | ` */` |
|    386 | 2684 | `static sxi32 StrPredicateResolveArg(` |
|      - | 2685 | `	ph7_context *pCtx,` |
|      - | 2686 | `	ph7_value *pArg,` |
|      - | 2687 | `	const char *zFunc,` |
|      - | 2688 | `	int iArgNum,` |
|      - | 2689 | `	const char *zParamName,` |
|      - | 2690 | `	const char *zNullMsg,` |
|      - | 2691 | `	ph7_value *pTmp,` |
|      - | 2692 | `	const char **pzOut,` |
|      - | 2693 | `	int *pnOut` |
|      4 | 2694 | `){` |
|    390 | 2695 | `	if( ph7_value_is_null(pArg) ){` |
|     13 | 2696 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zNullMsg);` |
|     13 | 2697 | `		*pzOut = "";` |
|     13 | 2698 | `		*pnOut = 0;` |
|     13 | 2699 | `		return PH7_OK;` |
|      - | 2700 | `	}` |
|    580 | 2701 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|    356 | 2702 | `	    ( ph7_value_is_object(pArg) &&` |
|     72 | 2703 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|     48 | 2704 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|     24 | 2705 | `	        "__toString",sizeof("__toString")-1) == 0` |
|      - | 2706 | `	    )` |
|      - | 2707 | `	){` |
|     34 | 2708 | `		const char *zType = ph7_type_name(pArg);` |
|     34 | 2709 | `		if( ph7_value_is_object(pArg) ){` |
|     13 | 2710 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     13 | 2711 | `			if( pInst && pInst->pClass ){` |
|     13 | 2712 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      6 | 2713 | `			}` |
|      6 | 2714 | `		}` |
|     49 | 2715 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2716 | `			"TypeError",` |
|      - | 2717 | `			"%s(): Argument #%d (%s) must be of type string, %s given",` |
|     15 | 2718 | `			zFunc, iArgNum, zParamName, zType` |
|      - | 2719 | `			);` |
|      - | 2720 | `	}` |
|    345 | 2721 | `	if( ph7_value_is_object(pArg) ){` |
|     37 | 2722 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     37 | 2723 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 2724 | `			"__toString",sizeof("__toString")-1);` |
|     37 | 2725 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|     37 | 2726 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|     37 | 2727 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|     37 | 2728 | `		return PH7_OK;` |
|      - | 2729 | `	}` |
|    309 | 2730 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|    309 | 2731 | `	return PH7_OK;` |
|    197 | 2732 |  |
|      - | 2733 | `/*` |
|      - | 2734 | ` * bool str_contains(string $haystack, string $needle)` |
|      - | 2735 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|      - | 2736 | ` * Return` |
|      - | 2737 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|      - | 2738 | ` */` |
|     76 | 2739 | `static int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2740 |  |
|      - | 2741 | `	const char *zHaystack,*zNeedle;` |
|      - | 2742 | `	int nHayLen,nNeedleLen;` |
|      - | 2743 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2744 | `	sxi32 rc;` |
|     80 | 2745 | `	if( nArg != 2 ){` |
|     18 | 2746 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2747 | `			"ArgumentCountError",` |
|      - | 2748 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|      5 | 2749 | `			nArg` |
|      - | 2750 | `			);` |
|      - | 2751 | `	}` |
|     68 | 2752 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     68 | 2753 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     68 | 2754 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack",` |
|      - | 2755 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|      - | 2756 | `		"of type string is deprecated",` |
|      - | 2757 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     68 | 2758 | `	if( rc != PH7_OK ) goto out;` |
|     61 | 2759 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle",` |
|      - | 2760 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|      - | 2761 | `		"of type string is deprecated",` |
|      - | 2762 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     61 | 2763 | `	if( rc != PH7_OK ) goto out;` |
|     57 | 2764 | `	if( nNeedleLen < 1 ){` |
|     13 | 2765 | `		ph7_result_bool(pCtx,1);` |
|     51 | 2766 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2767 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2768 | `	}else{` |
|     55 | 2769 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|     18 | 2770 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|     37 | 2771 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|      - | 2772 | `	}` |
|     57 | 2773 | `	rc = PH7_OK;` |
|     33 | 2774 | `out:` |
|     68 | 2775 | `	PH7_MemObjRelease(&sHayTmp);` |
|     68 | 2776 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     68 | 2777 | `	return rc;` |
|     42 | 2778 |  |
|      - | 2779 | `/*` |
|      - | 2780 | ` * bool str_starts_with(string $haystack, string $needle)` |
|      - | 2781 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|      - | 2782 | ` * Return` |
|      - | 2783 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|      - | 2784 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 2785 | ` */` |
|     78 | 2786 | `static int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2787 |  |
|      - | 2788 | `	const char *zHaystack,*zNeedle;` |
|      - | 2789 | `	int nHayLen,nNeedleLen;` |
|      - | 2790 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2791 | `	sxi32 rc;` |
|     82 | 2792 | `	if( nArg != 2 ){` |
|     18 | 2793 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2794 | `			"ArgumentCountError",` |
|      - | 2795 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|      5 | 2796 | `			nArg` |
|      - | 2797 | `			);` |
|      - | 2798 | `	}` |
|     70 | 2799 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 2800 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 2801 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack",` |
|      - | 2802 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 2803 | `		"of type string is deprecated",` |
|      - | 2804 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 2805 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 2806 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle",` |
|      - | 2807 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|      - | 2808 | `		"of type string is deprecated",` |
|      - | 2809 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 2810 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 2811 | `	if( nNeedleLen < 1 ){` |
|     13 | 2812 | `		ph7_result_bool(pCtx,1);` |
|     53 | 2813 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2814 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2815 | `	}else{` |
|     58 | 2816 | `		ph7_result_bool(pCtx,` |
|     38 | 2817 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 2818 | `	}` |
|     59 | 2819 | `	rc = PH7_OK;` |
|     34 | 2820 | `out:` |
|     70 | 2821 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 2822 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 2823 | `	return rc;` |
|     43 | 2824 |  |
|      - | 2825 | `/*` |
|      - | 2826 | ` * bool str_ends_with(string $haystack, string $needle)` |
|      - | 2827 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|      - | 2828 | ` * Return` |
|      - | 2829 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|      - | 2830 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 2831 | ` */` |
|     78 | 2832 | `static int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2833 |  |
|      - | 2834 | `	const char *zHaystack,*zNeedle;` |
|      - | 2835 | `	int nHayLen,nNeedleLen;` |
|      - | 2836 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2837 | `	sxi32 rc;` |
|     82 | 2838 | `	if( nArg != 2 ){` |
|     18 | 2839 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2840 | `			"ArgumentCountError",` |
|      - | 2841 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|      5 | 2842 | `			nArg` |
|      - | 2843 | `			);` |
|      - | 2844 | `	}` |
|     70 | 2845 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 2846 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 2847 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack",` |
|      - | 2848 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 2849 | `		"of type string is deprecated",` |
|      - | 2850 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 2851 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 2852 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle",` |
|      - | 2853 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|      - | 2854 | `		"of type string is deprecated",` |
|      - | 2855 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 2856 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 2857 | `	if( nNeedleLen < 1 ){` |
|     13 | 2858 | `		ph7_result_bool(pCtx,1);` |
|     53 | 2859 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2860 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2861 | `	}else{` |
|     58 | 2862 | `		ph7_result_bool(pCtx,` |
|     38 | 2863 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 2864 | `	}` |
|     59 | 2865 | `	rc = PH7_OK;` |
|     34 | 2866 | `out:` |
|     70 | 2867 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 2868 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 2869 | `	return rc;` |
|     43 | 2870 |  |
|      - | 2871 | `/*` |
|      - | 2872 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2873 | ` *  Case-insensitive strpos.` |
|      - | 2874 | ` * Parameters` |
|      - | 2875 | ` *  $haystack` |
|      - | 2876 | ` *   The input string.` |
|      - | 2877 | ` * $needle` |
|      - | 2878 | ` *   Search pattern (must be a string).` |
|      - | 2879 | ` * $offset` |
|      - | 2880 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 2881 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 2882 | ` *   of haystack.` |
|      - | 2883 | ` * Return` |
|      - | 2884 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 2885 | ` */` |
|     18 | 2886 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2887 |  |
|     19 | 2888 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2889 | `	const char *zBlob,*zPattern;` |
|      - | 2890 | `	int nLen,nPatLen,nStart;` |
|      - | 2891 | `	sxu32 nOfft;` |
|      - | 2892 | `	sxi32 rc;` |
|     19 | 2893 | `	if( nArg < 2 ){` |
|      - | 2894 | `		/* Missing arguments,return FALSE */` |
|      3 | 2895 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2896 | `		return PH7_OK;` |
|      - | 2897 | `	}` |
|      - | 2898 | `	/* Extract the needle and the haystack */` |
|     17 | 2899 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 2900 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     17 | 2901 | `	nOfft = 0; /* cc warning */` |
|     17 | 2902 | `	nStart = 0;` |
|      - | 2903 | `	/* Peek the starting offset if available */` |
|     17 | 2904 | `	if( nArg > 2 ){` |
|      5 | 2905 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 2906 | `		if( nStart < 0 ){` |
|      3 | 2907 | `			nStart = -nStart;` |
|      1 | 2908 | `		}` |
|      5 | 2909 | `		if( nStart >= nLen ){` |
|      - | 2910 | `			/* Invalid offset */` |
|    ! 0 | 2911 | `			nStart = 0;` |
|    ! 0 | 2912 | `		}else{` |
|      5 | 2913 | `			zBlob += nStart;` |
|      5 | 2914 | `			nLen -= nStart;` |
|      - | 2915 | `		}` |
|      2 | 2916 | `	}` |
|     17 | 2917 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2918 | `		/* Perform the lookup */` |
|     17 | 2919 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     17 | 2920 | `		if( rc != SXRET_OK ){` |
|      - | 2921 | `			/* Pattern not found,return FALSE */` |
|      3 | 2922 | `			ph7_result_bool(pCtx,0);` |
|      3 | 2923 | `			return PH7_OK;` |
|      - | 2924 | `		}` |
|      - | 2925 | `		/* Return the pattern position */` |
|     15 | 2926 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 2927 | `	}else{` |
|    ! 0 | 2928 | `		ph7_result_bool(pCtx,0);` |
|      - | 2929 | `	}` |
|     15 | 2930 | `	return PH7_OK;` |
|     10 | 2931 |  |
|      - | 2932 | `/*` |
|      - | 2933 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2934 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 2935 | ` * Parameters` |
|      - | 2936 | ` *  $haystack` |
|      - | 2937 | ` *   The input string.` |
|      - | 2938 | ` * $needle` |
|      - | 2939 | ` *   Search pattern (must be a string).` |
|      - | 2940 | ` * $offset` |
|      - | 2941 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 2942 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 2943 | ` *   characters from the end of the string, searching backwards.` |
|      - | 2944 | ` * Return` |
|      - | 2945 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 2946 | ` */` |
|     32 | 2947 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2948 |  |
|      - | 2949 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     33 | 2950 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2951 | `	int nLen,nPatLen;` |
|      - | 2952 | `	sxu32 nOfft;` |
|      - | 2953 | `	sxi32 rc;` |
|     33 | 2954 | `	if( nArg < 2 ){` |
|      - | 2955 | `		/* Missing arguments,return FALSE */` |
|      3 | 2956 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2957 | `		return PH7_OK;` |
|      - | 2958 | `	}` |
|      - | 2959 | `	/* Extract the needle and the haystack */` |
|     31 | 2960 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 2961 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 2962 | `	/* Point to the end of the pattern */` |
|     31 | 2963 | `	zPtr = &zBlob[nLen - 1];` |
|     31 | 2964 | `	zEnd = &zBlob[nLen];` |
|      - | 2965 | `	/* Save the starting posistion */` |
|     31 | 2966 | `	zStart = zBlob;` |
|     31 | 2967 | `	nOfft = 0; /* cc warning */` |
|      - | 2968 | `	/* Peek the starting offset if available */` |
|     31 | 2969 | `	if( nArg > 2 ){` |
|      - | 2970 | `		int nStart;` |
|     21 | 2971 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 2972 | `		if( nStart < 0 ){` |
|     11 | 2973 | `			nStart = -nStart;` |
|     11 | 2974 | `			if( nStart >= nLen ){` |
|      - | 2975 | `				/* Invalid offset */` |
|      3 | 2976 | `				ph7_result_bool(pCtx,0);` |
|      3 | 2977 | `				return PH7_OK;` |
|    ! 0 | 2978 | `			}else{` |
|      9 | 2979 | `				nLen -= nStart;` |
|      9 | 2980 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 2981 | `				zEnd = &zBlob[nLen];` |
|      - | 2982 | `			}` |
|      5 | 2983 | `		}else{` |
|     11 | 2984 | `			if( nStart >= nLen ){` |
|      - | 2985 | `				/* Invalid offset */` |
|      5 | 2986 | `				ph7_result_bool(pCtx,0);` |
|      5 | 2987 | `				return PH7_OK;` |
|    ! 0 | 2988 | `			}else{` |
|      7 | 2989 | `				zBlob += nStart;` |
|      7 | 2990 | `				nLen -= nStart;` |
|      - | 2991 | `			}` |
|      - | 2992 | `		}` |
|      7 | 2993 | `	}` |
|     25 | 2994 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2995 | `		/* Perform the lookup */` |
|     57 | 2996 | `		for(;;){` |
|    115 | 2997 | `			if( zBlob >= zPtr ){` |
|     11 | 2998 | `				break;` |
|      - | 2999 | `			}` |
|    105 | 3000 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    105 | 3001 | `			if( rc == SXRET_OK ){` |
|      - | 3002 | `				/* Pattern found,return it's position */` |
|     13 | 3003 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 3004 | `				return PH7_OK;` |
|      - | 3005 | `			}` |
|     93 | 3006 | `			zPtr--;` |
|      1 | 3007 | `		}` |
|      - | 3008 | `		/* Pattern not found,return FALSE */` |
|     11 | 3009 | `		ph7_result_bool(pCtx,0);` |
|      6 | 3010 | `	}else{` |
|      3 | 3011 | `		ph7_result_bool(pCtx,0);` |
|      - | 3012 | `	}` |
|     13 | 3013 | `	return PH7_OK;` |
|     17 | 3014 |  |
|      - | 3015 | `/*` |
|      - | 3016 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3017 | ` *  Case-insensitive strrpos.` |
|      - | 3018 | ` * Parameters` |
|      - | 3019 | ` *  $haystack` |
|      - | 3020 | ` *   The input string.` |
|      - | 3021 | ` * $needle` |
|      - | 3022 | ` *   Search pattern (must be a string).` |
|      - | 3023 | ` * $offset` |
|      - | 3024 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3025 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3026 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3027 | ` * Return` |
|      - | 3028 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3029 | ` */` |
|     28 | 3030 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3031 |  |
|      - | 3032 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     29 | 3033 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3034 | `	int nLen,nPatLen;` |
|      - | 3035 | `	sxu32 nOfft;` |
|      - | 3036 | `	sxi32 rc;` |
|     29 | 3037 | `	if( nArg < 2 ){` |
|      - | 3038 | `		/* Missing arguments,return FALSE */` |
|      3 | 3039 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3040 | `		return PH7_OK;` |
|      - | 3041 | `	}` |
|      - | 3042 | `	/* Extract the needle and the haystack */` |
|     27 | 3043 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 3044 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3045 | `	/* Point to the end of the pattern */` |
|     27 | 3046 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 3047 | `	zEnd = &zBlob[nLen];` |
|      - | 3048 | `	/* Save the starting posistion */` |
|     27 | 3049 | `	zStart = zBlob;` |
|     27 | 3050 | `	nOfft = 0; /* cc warning */` |
|      - | 3051 | `	/* Peek the starting offset if available */` |
|     27 | 3052 | `	if( nArg > 2 ){` |
|      - | 3053 | `		int nStart;` |
|     15 | 3054 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3055 | `		if( nStart < 0 ){` |
|      7 | 3056 | `			nStart = -nStart;` |
|      7 | 3057 | `			if( nStart >= nLen ){` |
|      - | 3058 | `				/* Invalid offset */` |
|      3 | 3059 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3060 | `				return PH7_OK;` |
|    ! 0 | 3061 | `			}else{` |
|      5 | 3062 | `				nLen -= nStart;` |
|      5 | 3063 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 3064 | `				zEnd = &zBlob[nLen];` |
|      - | 3065 | `			}` |
|      3 | 3066 | `		}else{` |
|      9 | 3067 | `			if( nStart >= nLen ){` |
|      - | 3068 | `				/* Invalid offset */` |
|      5 | 3069 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3070 | `				return PH7_OK;` |
|    ! 0 | 3071 | `			}else{` |
|      5 | 3072 | `				zBlob += nStart;` |
|      5 | 3073 | `				nLen -= nStart;` |
|      - | 3074 | `			}` |
|      - | 3075 | `		}` |
|      4 | 3076 | `	}` |
|     21 | 3077 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3078 | `		/* Perform the lookup */` |
|     44 | 3079 | `		for(;;){` |
|     89 | 3080 | `			if( zBlob >= zPtr ){` |
|      9 | 3081 | `				break;` |
|      - | 3082 | `			}` |
|     81 | 3083 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 3084 | `			if( rc == SXRET_OK ){` |
|      - | 3085 | `				/* Pattern found,return it's position */` |
|     11 | 3086 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 3087 | `				return PH7_OK;` |
|      - | 3088 | `			}` |
|     71 | 3089 | `			zPtr--;` |
|      1 | 3090 | `		}` |
|      - | 3091 | `		/* Pattern not found,return FALSE */` |
|      9 | 3092 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3093 | `	}else{` |
|      3 | 3094 | `		ph7_result_bool(pCtx,0);` |
|      - | 3095 | `	}` |
|     11 | 3096 | `	return PH7_OK;` |
|     15 | 3097 |  |
|      - | 3098 | `/*` |
|      - | 3099 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3100 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3101 | ` * Parameters` |
|      - | 3102 | ` *  $haystack` |
|      - | 3103 | ` *   The input string.` |
|      - | 3104 | ` * $needle` |
|      - | 3105 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3106 | ` *  This behavior is different from that of strstr().` |
|      - | 3107 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3108 | ` *  as the ordinal value of a character.` |
|      - | 3109 | ` * Return` |
|      - | 3110 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3111 | ` */` |
|     24 | 3112 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3113 |  |
|      - | 3114 | `	const char *zBlob;` |
|      - | 3115 | `	int nLen,c;` |
|     25 | 3116 | `	if( nArg < 2 ){` |
|      - | 3117 | `		/* Missing arguments,return FALSE */` |
|      3 | 3118 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3119 | `		return PH7_OK;` |
|      - | 3120 | `	}` |
|      - | 3121 | `	/* Extract the haystack */` |
|     23 | 3122 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3123 | `	c = 0; /* cc warning */` |
|     23 | 3124 | `	if( nLen > 0 ){` |
|      - | 3125 | `		sxu32 nOfft;` |
|      - | 3126 | `		sxi32 rc;` |
|     21 | 3127 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 3128 | `			const char *zPattern;` |
|     11 | 3129 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 3130 | `														 * for NULL pointer.` |
|      - | 3131 | `														 */` |
|     11 | 3132 | `			c = zPattern[0];` |
|      6 | 3133 | `		}else{` |
|      - | 3134 | `			/* Int cast */` |
|     11 | 3135 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 3136 | `		}` |
|      - | 3137 | `		/* Perform the lookup */` |
|     21 | 3138 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3139 | `		if( rc != SXRET_OK ){` |
|      - | 3140 | `			/* No such entry,return FALSE */` |
|      7 | 3141 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3142 | `			return PH7_OK;` |
|      - | 3143 | `		}` |
|      - | 3144 | `		/* Return the string portion */` |
|     15 | 3145 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 3146 | `	}else{` |
|      3 | 3147 | `		ph7_result_bool(pCtx,0);` |
|      - | 3148 | `	}` |
|     17 | 3149 | `	return PH7_OK;` |
|     13 | 3150 |  |
|      - | 3151 | `/*` |
|      - | 3152 | ` * string strrev(string $string)` |
|      - | 3153 | ` *  Reverse a string.` |
|      - | 3154 | ` * Parameters` |
|      - | 3155 | ` *  $string` |
|      - | 3156 | ` *   String to be reversed.` |
|      - | 3157 | ` * Return` |
|      - | 3158 | ` *  The reversed string.` |
|      - | 3159 | ` */` |
|      4 | 3160 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3161 |  |
|      - | 3162 | `	const char *zIn,*zEnd;` |
|      - | 3163 | `	int nLen,c;` |
|      5 | 3164 | `	if( nArg < 1 ){` |
|      - | 3165 | `		/* Missing arguments,return NULL */` |
|      3 | 3166 | `		ph7_result_null(pCtx);` |
|      3 | 3167 | `		return PH7_OK;` |
|      - | 3168 | `	}` |
|      - | 3169 | `	/* Extract the target string */` |
|      3 | 3170 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3171 | `	if( nLen < 1 ){` |
|      - | 3172 | `		/* Empty string Return null */` |
|    ! 0 | 3173 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3174 | `		return PH7_OK;` |
|      - | 3175 | `	}` |
|      - | 3176 | `	/* Perform the requested operation */` |
|      3 | 3177 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3178 | `	for(;;){` |
|      9 | 3179 | `		if( zEnd < zIn ){` |
|      - | 3180 | `			/* No more input to process */` |
|      3 | 3181 | `			break;` |
|      - | 3182 | `		}` |
|      - | 3183 | `		/* Append current character */` |
|      7 | 3184 | `		c = zEnd[0];` |
|      7 | 3185 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3186 | `		zEnd--;` |
|      1 | 3187 | `	}` |
|      3 | 3188 | `	return PH7_OK;` |
|      3 | 3189 |  |
|      - | 3190 | `/*` |
|      - | 3191 | ` * string ucwords(string $string)` |
|      - | 3192 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3193 | ` *  The definition of a word is any string of characters that is immediately after` |
|      - | 3194 | ` *  a whitespace (These are: space, form-feed, newline, carriage return, horizontal tab, and vertical tab).` |
|      - | 3195 | ` * Parameters` |
|      - | 3196 | ` *  $string` |
|      - | 3197 | ` *   The input string.` |
|      - | 3198 | ` * Return` |
|      - | 3199 | ` *  The modified string..` |
|      - | 3200 | ` */` |
|     14 | 3201 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3202 |  |
|      - | 3203 | `	const char *zIn,*zCur,*zEnd;` |
|      - | 3204 | `	int nLen,c;` |
|     15 | 3205 | `	if( nArg < 1 ){` |
|      - | 3206 | `		/* Missing arguments,return NULL */` |
|      3 | 3207 | `		ph7_result_null(pCtx);` |
|      3 | 3208 | `		return PH7_OK;` |
|      - | 3209 | `	}` |
|      - | 3210 | `	/* Extract the target string */` |
|     13 | 3211 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 3212 | `	if( nLen < 1 ){` |
|      - | 3213 | `		/* Empty string – match PHP semantics */` |
|      3 | 3214 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3215 | `		return PH7_OK;` |
|      - | 3216 | `	}` |
|      - | 3217 | `	/* Perform the requested operation */` |
|     11 | 3218 | `	zEnd = &zIn[nLen];` |
|     21 | 3219 | `	for(;;){` |
|      - | 3220 | `		/* Jump leading white spaces */` |
|     43 | 3221 | `		zCur = zIn;` |
|     65 | 3222 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0x80 && SyisSpace(zIn[0]) ){` |
|     23 | 3223 | `			zIn++;` |
|      1 | 3224 | `		}` |
|     43 | 3225 | `		if( zCur < zIn ){` |
|      - | 3226 | `			/* Append white space stream */` |
|     23 | 3227 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 3228 | `		}` |
|     43 | 3229 | `		if( zIn >= zEnd ){` |
|      - | 3230 | `			/* No more input to process */` |
|     11 | 3231 | `			break;` |
|      - | 3232 | `		}` |
|     33 | 3233 | `		c = zIn[0];` |
|     33 | 3234 | `		if( c < 0x80 && SyisLower(c) ){` |
|     29 | 3235 | `			c = SyToUpper(c);` |
|     14 | 3236 | `		}` |
|      - | 3237 | `		/* Append the upper-cased character */` |
|     33 | 3238 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     33 | 3239 | `		zIn++;` |
|     33 | 3240 | `		zCur = zIn;` |
|      - | 3241 | `		/* Append the word varbatim */` |
|    149 | 3242 | `		while( zIn < zEnd ){` |
|    139 | 3243 | `			if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 3244 | `				/* UTF-8 stream */` |
|    ! 0 | 3245 | `				zIn++;` |
|    ! 0 | 3246 | `				SX_JMP_UTF8(zIn,zEnd);` |
|    138 | 3247 | `			}else if( !SyisSpace(zIn[0]) ){` |
|    117 | 3248 | `				zIn++;` |
|     59 | 3249 | `			}else{` |
|     23 | 3250 | `				break;` |
|      - | 3251 | `			}` |
|      1 | 3252 | `		}` |
|     33 | 3253 | `		if( zCur < zIn ){` |
|     33 | 3254 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 | 3255 | `		}` |
|      1 | 3256 | `	}` |
|     11 | 3257 | `	return PH7_OK;` |
|      8 | 3258 |  |
|      - | 3259 | `/*` |
|      - | 3260 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3261 | ` *  Returns input repeated multiplier times.` |
|      - | 3262 | ` * Parameters` |
|      - | 3263 | ` *  $string` |
|      - | 3264 | ` *   String to be repeated.` |
|      - | 3265 | ` * $multiplier` |
|      - | 3266 | ` *  Number of time the input string should be repeated.` |
|      - | 3267 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3268 | ` *  to 0, the function will return an empty string.` |
|      - | 3269 | ` * Return` |
|      - | 3270 | ` *  The repeated string.` |
|      - | 3271 | ` */` |
|  20226 | 3272 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3273 |  |
|      - | 3274 | `	const char *zIn;` |
|      - | 3275 | `	int nLen,nMul;` |
|      - | 3276 | `	int rc;` |
|  20227 | 3277 | `	if( nArg < 2 ){` |
|      - | 3278 | `		/* Missing arguments,return NULL */` |
|      3 | 3279 | `		ph7_result_null(pCtx);` |
|      3 | 3280 | `		return PH7_OK;` |
|      - | 3281 | `	}` |
|      - | 3282 | `	/* Extract the target string */` |
|  20225 | 3283 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|  20225 | 3284 | `	if( nLen < 1 ){` |
|      - | 3285 | `		/* Empty string.Return null */` |
|    ! 0 | 3286 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3287 | `		return PH7_OK;` |
|      - | 3288 | `	}` |
|      - | 3289 | `	/* Extract the multiplier */` |
|  20225 | 3290 | `	nMul = ph7_value_to_int(apArg[1]);` |
|  20225 | 3291 | `	if( nMul < 1 ){` |
|      - | 3292 | `		/* Return the empty string */` |
|      3 | 3293 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3294 | `		return PH7_OK;` |
|      - | 3295 | `	}` |
|      - | 3296 | `	/* Perform the requested operation */` |
| 120878 | 3297 | `	for(;;){` |
| 241757 | 3298 | `		if( !nMul ){` |
|  20223 | 3299 | `			break;` |
|      - | 3300 | `		}` |
|      - | 3301 | `		/* Append the copy */` |
| 221535 | 3302 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 221535 | 3303 | `		if( rc != PH7_OK ){` |
|      - | 3304 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 3305 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 3306 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 3307 | `		}` |
| 221535 | 3308 | `		nMul--;` |
|      1 | 3309 | `	}` |
|  20223 | 3310 | `	return PH7_OK;` |
|  10114 | 3311 |  |
|      - | 3312 | `/*` |
|      - | 3313 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3314 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3315 | ` * Parameters` |
|      - | 3316 | ` *  $string` |
|      - | 3317 | ` *   The input string.` |
|      - | 3318 | ` * $is_xhtml` |
|      - | 3319 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3320 | ` * Return` |
|      - | 3321 | ` *  The processed string.` |
|      - | 3322 | ` */` |
|      6 | 3323 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3324 |  |
|      - | 3325 | `	const char *zIn,*zCur,*zEnd;` |
|      7 | 3326 | `	int is_xhtml = 1; /* Default to XHTML-style '<br/>' like PHP */` |
|      - | 3327 | `	int nLen;` |
|      7 | 3328 | `	if( nArg < 1 ){` |
|      - | 3329 | `		/* Missing arguments,return the empty string */` |
|      3 | 3330 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3331 | `		return PH7_OK;` |
|      - | 3332 | `	}` |
|      - | 3333 | `	/* Extract the target string */` |
|      5 | 3334 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3335 | `	if( nLen < 1 ){` |
|      - | 3336 | `		/* Empty string,return null */` |
|    ! 0 | 3337 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3338 | `		return PH7_OK;` |
|      - | 3339 | `	}` |
|      5 | 3340 | `	if( nArg > 1 ){` |
|      3 | 3341 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3342 | `	}` |
|      5 | 3343 | `	zEnd = &zIn[nLen];` |
|      - | 3344 | `	/* Perform the requested operation */` |
|      4 | 3345 | `	for(;;){` |
|      9 | 3346 | `		zCur = zIn;` |
|      - | 3347 | `		/* Delimit the string */` |
|     21 | 3348 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3349 | `			zIn++;` |
|      1 | 3350 | `		}` |
|      9 | 3351 | `		if( zCur < zIn ){` |
|      - | 3352 | `			/* Output chunk verbatim */` |
|      9 | 3353 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3354 | `		}` |
|      9 | 3355 | `		if( zIn >= zEnd ){` |
|      - | 3356 | `			/* No more input to process */` |
|      5 | 3357 | `			break;` |
|      - | 3358 | `		}` |
|      - | 3359 | `		/* Output the HTML line break */` |
|      - | 3360 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br/>' (legacy without space), otherwise use '<br>' */` |
|      5 | 3361 | `		if( is_xhtml ){` |
|      3 | 3362 | `			ph7_result_string(pCtx,"<br/>",(int)sizeof("<br/>")-1);` |
|      2 | 3363 | `		}else{` |
|      3 | 3364 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3365 | `		}` |
|      5 | 3366 | `		zCur = zIn;` |
|      - | 3367 | `		/* Append trailing line */` |
|     11 | 3368 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3369 | `			zIn++;` |
|      1 | 3370 | `		}` |
|      5 | 3371 | `		if( zCur < zIn ){` |
|      - | 3372 | `			/* Output chunk verbatim */` |
|      5 | 3373 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 3374 | `		}` |
|      1 | 3375 | `	}` |
|      5 | 3376 | `	return PH7_OK;` |
|      4 | 3377 |  |
|      - | 3378 | `/*` |
|      - | 3379 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3380 | ` *  According to the PHP reference manual.` |
|      - | 3381 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3382 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3383 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 3384 | ` * This applies to both sprintf() and printf().` |
|      - | 3385 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 3386 | ` * or more of these elements, in order:` |
|      - | 3387 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 3388 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 3389 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 3390 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 3391 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 3392 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 3393 | ` *   it with a single quote ('). See the examples below.` |
|      - | 3394 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 3395 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 3396 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 3397 | ` *   should result in.` |
|      - | 3398 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 3399 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 3400 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 3401 | ` *   limit to the string.` |
|      - | 3402 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 3403 | ` *       % - a literal percent character. No argument is required.` |
|      - | 3404 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 3405 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 3406 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 3407 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 3408 | ` * 	     for the number of digits after the decimal point.` |
|      - | 3409 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 3410 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 3411 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 3412 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 3413 | ` *       g - shorter of %e and %f.` |
|      - | 3414 | ` *       G - shorter of %E and %f.` |
|      - | 3415 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 3416 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 3417 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 3418 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 3419 | ` */` |
|      - | 3420 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3421 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 3422 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 3423 | `/*` |
|      - | 3424 | `** Conversion types fall into various categories as defined by the` |
|      - | 3425 | `** following enumeration.` |
|      - | 3426 | `*/` |
|      - | 3427 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 3428 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 3429 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 3430 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 3431 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 3432 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 3433 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 3434 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 3435 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 3436 |  |
|      - | 3437 | `/*` |
|      - | 3438 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 3439 | `*/` |
|      - | 3440 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 3441 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 3442 | `/*` |
|      - | 3443 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 3444 | `** by an instance of the following structure` |
|      - | 3445 | `*/` |
|      - | 3446 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 3447 | `struct ph7_fmt_info` |
|      - | 3448 |  |
|      - | 3449 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 3450 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 3451 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 3452 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 3453 | `  char *charset; /* The character set for conversion */` |
|      - | 3454 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 3455 | `};` |
|      - | 3456 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3457 | `/*` |
|      - | 3458 | `** "*val" is a double such that 0.1 <= *val < 10.0` |
|      - | 3459 | `** Return the ascii code for the leading digit of *val, then` |
|      - | 3460 | `** multiply "*val" by 10.0 to renormalize.` |
|      - | 3461 | `**` |
|      - | 3462 | `** Example:` |
|      - | 3463 | `**     input:     *val = 3.14159` |
|      - | 3464 | `**     output:    *val = 1.4159    function return = '3'` |
|      - | 3465 | `**` |
|      - | 3466 | `** The counter *cnt is incremented each time.  After counter exceeds` |
|      - | 3467 | `** 16 (the number of significant digits in a 64-bit float) '0' is` |
|      - | 3468 | `** always returned.` |
|      - | 3469 | `*/` |
|    422 | 3470 | `static int vxGetdigit(sxlongreal *val,int *cnt)` |
|      1 | 3471 |  |
|      - | 3472 | `  sxlongreal d;` |
|      - | 3473 | `  int digit;` |
|      - | 3474 |  |
|    423 | 3475 | `  if( (*cnt)++ >= 16 ){` |
|    ! 0 | 3476 | `	  return '0';` |
|      - | 3477 | `  }` |
|    423 | 3478 | `  digit = (int)*val;` |
|    423 | 3479 | `  d = digit;` |
|    423 | 3480 | `   *val = (*val - d)*10.0;` |
|    423 | 3481 | `  return digit + '0' ;` |
|    212 | 3482 |  |
|      - | 3483 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      - | 3484 | `/*` |
|      - | 3485 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 3486 | ` * used conversion types first.` |
|      - | 3487 | ` */` |
|      - | 3488 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 3489 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 3490 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 3491 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 3492 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 3493 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 3494 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 3495 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 3496 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 3497 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3498 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3499 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 3500 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 3501 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 3502 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 3503 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 3504 | `};` |
|      - | 3505 | `/*` |
|      - | 3506 | ` * Format a given string.` |
|      - | 3507 | ` * The root program.  All variations call this core.` |
|      - | 3508 | ` * INPUTS:` |
|      - | 3509 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 3510 | ` *            1. A pointer to the call context.` |
|      - | 3511 | ` *            2. A pointer to the list of characters to be output` |
|      - | 3512 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 3513 | ` *            3. An integer number of characters to be output.` |
|      - | 3514 | ` *               (Note: This number might be zero.)` |
|      - | 3515 | ` *            4. Upper layer private data.` |
|      - | 3516 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 3517 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 3518 | ` */` |
|    136 | 3519 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 3520 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 3521 | `	ph7_context *pCtx,  /* call context */` |
|      - | 3522 | `	const char *zIn,    /* Format string */` |
|      - | 3523 | `	int nByte,          /* Format string length */` |
|      - | 3524 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 3525 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 3526 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 3527 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 3528 | `	)` |
|      1 | 3529 |  |
|    137 | 3530 | `	char spaces[] = "                                                  ";` |
|      - | 3531 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    137 | 3532 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 3533 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 3534 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 3535 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 3536 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 3537 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 3538 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 3539 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 3540 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 3541 | `	ph7_int64 iVal;` |
|      - | 3542 | `	int precision;           /* Precision of the current field */` |
|      - | 3543 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 3544 | `	int c,rc,n;` |
|      - | 3545 | `	int length;              /* Length of the field */` |
|      - | 3546 | `	int prefix;` |
|      - | 3547 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 3548 | `	int width;               /* Width of the current field */` |
|      - | 3549 | `	int idx;` |
|    137 | 3550 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 3551 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 3552 | `	/* Start the format process */` |
|    139 | 3553 | `	for(;;){` |
|    279 | 3554 | `		zCur = zIn;` |
|    739 | 3555 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|    461 | 3556 | `			zIn++;` |
|      1 | 3557 | `		}` |
|    279 | 3558 | `		if( zCur < zIn ){` |
|      - | 3559 | `			/* Consume chunk verbatim */` |
|    105 | 3560 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|    105 | 3561 | `			if( rc != SXRET_OK ){` |
|      - | 3562 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|    ! 0 | 3563 | `				break;` |
|      - | 3564 | `			}` |
|     52 | 3565 | `		}` |
|    279 | 3566 | `		if( zIn >= zEnd ){` |
|      - | 3567 | `			/* No more input to process,break immediately */` |
|    135 | 3568 | `			break;` |
|      - | 3569 | `		}` |
|      - | 3570 | `		/* Find out what flags are present */` |
|    145 | 3571 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    144 | 3572 | `			flag_alternateform = flag_zeropad = 0;` |
|    145 | 3573 | `		zIn++; /* Jump the precent sign */` |
|     72 | 3574 | `		do{` |
|    177 | 3575 | `			c = zIn[0];` |
|    177 | 3576 | `			switch( c ){` |
|      9 | 3577 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      3 | 3578 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 3579 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|      9 | 3580 | `			case '#':   flag_alternateform = 1;   c = 0;   break;` |
|      9 | 3581 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 3582 | `			case '\'':` |
|    ! 0 | 3583 | `				zIn++;` |
|    ! 0 | 3584 | `				if( zIn < zEnd ){` |
|      - | 3585 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 3586 | `					c = zIn[0];` |
|    ! 0 | 3587 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3588 | `						spaces[idx] = (char)c;` |
|    ! 0 | 3589 | `					}` |
|    ! 0 | 3590 | `					c = 0;` |
|    ! 0 | 3591 | `				}` |
|    ! 0 | 3592 | `				break;` |
|    144 | 3593 | `			default:                                       break;` |
|      - | 3594 | `			}` |
|    177 | 3595 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 3596 | `		/* Get the field width */` |
|    145 | 3597 | `		width = 0;` |
|    251 | 3598 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     35 | 3599 | `			width = width*10 + (zIn[0] - '0');` |
|     35 | 3600 | `			zIn++;` |
|      1 | 3601 | `		}` |
|    145 | 3602 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 3603 | `			/* Position specifer */` |
|    ! 0 | 3604 | `			if( width > 0 ){` |
|    ! 0 | 3605 | `				n = width;` |
|    ! 0 | 3606 | `				if( vf && n > 0 ){` |
|    ! 0 | 3607 | `					n--;` |
|    ! 0 | 3608 | `				}` |
|    ! 0 | 3609 | `			}` |
|    ! 0 | 3610 | `			zIn++;` |
|    ! 0 | 3611 | `			width = 0;` |
|    ! 0 | 3612 | `			if( zIn < zEnd && zIn[0] == '0' ){` |
|    ! 0 | 3613 | `				flag_zeropad = 1;` |
|    ! 0 | 3614 | `				zIn++;` |
|    ! 0 | 3615 | `			}` |
|    ! 0 | 3616 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 3617 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 3618 | `				zIn++;` |
|    ! 0 | 3619 | `			}` |
|    ! 0 | 3620 | `		}` |
|    145 | 3621 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 3622 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 3623 | `		}` |
|      - | 3624 | `		/* Get the precision */` |
|    145 | 3625 | `		precision = -1;` |
|    145 | 3626 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     59 | 3627 | `			precision = 0;` |
|     59 | 3628 | `			zIn++;` |
|    150 | 3629 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     63 | 3630 | `				precision = precision*10 + (zIn[0] - '0');` |
|     63 | 3631 | `				zIn++;` |
|      1 | 3632 | `			}` |
|     29 | 3633 | `		}` |
|    145 | 3634 | `		if( zIn >= zEnd ){` |
|      - | 3635 | `			/* No more input */` |
|      3 | 3636 | `			break;` |
|      - | 3637 | `		}` |
|      - | 3638 | `		/* Fetch the info entry for the field */` |
|    143 | 3639 | `		pInfo = 0;` |
|    143 | 3640 | `		xtype = PH7_FMT_ERROR;` |
|    143 | 3641 | `		c = zIn[0];` |
|    143 | 3642 | `		zIn++; /* Jump the format specifer */` |
|    787 | 3643 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|    785 | 3644 | `			if( c==aFmt[idx].fmttype ){` |
|    141 | 3645 | `				pInfo = &aFmt[idx];` |
|    141 | 3646 | `				xtype = pInfo->type;` |
|    141 | 3647 | `				break;` |
|      - | 3648 | `			}` |
|    323 | 3649 | `		}` |
|    143 | 3650 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    143 | 3651 | `		length = 0;` |
|      - | 3652 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 3653 | `		 /*` |
|      - | 3654 | `		  ** At this point, variables are initialized as follows:` |
|      - | 3655 | `		  **` |
|      - | 3656 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 3657 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 3658 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 3659 | `		  **                               field width was negative.` |
|      - | 3660 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 3661 | `		  **                               the conversion character.` |
|      - | 3662 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 3663 | `		  **   width                       The specified field width.  This is` |
|      - | 3664 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 3665 | `		  **   precision                   The specified precision.  The default` |
|      - | 3666 | `		  **                               is -1.` |
|      - | 3667 | `		  */` |
|    143 | 3668 | `		switch(xtype){` |
|    ! 0 | 3669 | `		case PH7_FMT_PERCENT:` |
|      - | 3670 | `			/* A literal percent character */` |
|    ! 0 | 3671 | `			zWorker[0] = '%';` |
|    ! 0 | 3672 | `			length = (int)sizeof(char);` |
|    ! 0 | 3673 | `			break;` |
|      3 | 3674 | `		case PH7_FMT_CHARX:` |
|      - | 3675 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 3676 | `			 * with that ASCII value` |
|      - | 3677 | `			 */` |
|      7 | 3678 | `			pArg = NEXT_ARG;` |
|      7 | 3679 | `			if( pArg == 0 ){` |
|      3 | 3680 | `				c = 0;` |
|      2 | 3681 | `			}else{` |
|      5 | 3682 | `				c = ph7_value_to_int(pArg);` |
|      - | 3683 | `			}` |
|      - | 3684 | `			/* NUL byte is an acceptable value */` |
|      7 | 3685 | `			zWorker[0] = (char)c;` |
|      7 | 3686 | `			length = (int)sizeof(char);` |
|      7 | 3687 | `			break;` |
|     12 | 3688 | `		case PH7_FMT_STRING:` |
|      - | 3689 | `			/* the argument is treated as and presented as a string */` |
|     25 | 3690 | `			pArg = NEXT_ARG;` |
|     25 | 3691 | `			if( pArg == 0 ){` |
|    ! 0 | 3692 | `				length = 0;` |
|    ! 0 | 3693 | `			}else{` |
|     25 | 3694 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 3695 | `			}` |
|     25 | 3696 | `			if( length < 1 ){` |
|    ! 0 | 3697 | `				zBuf = " ";` |
|    ! 0 | 3698 | `				length = (int)sizeof(char);` |
|    ! 0 | 3699 | `			}` |
|     25 | 3700 | `			if( precision>=0 && precision<length ){` |
|      3 | 3701 | `				length = precision;` |
|      1 | 3702 | `			}` |
|     25 | 3703 | `			if( flag_zeropad ){` |
|      - | 3704 | `				/* zero-padding works on strings too */` |
|    ! 0 | 3705 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3706 | `					spaces[idx] = '0';` |
|    ! 0 | 3707 | `				}` |
|    ! 0 | 3708 | `			}` |
|     25 | 3709 | `			break;` |
|     27 | 3710 | `		case PH7_FMT_RADIX:` |
|     55 | 3711 | `			pArg = NEXT_ARG;` |
|     55 | 3712 | `			if( pArg == 0 ){` |
|    ! 0 | 3713 | `				iVal = 0;` |
|    ! 0 | 3714 | `			}else{` |
|     55 | 3715 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 3716 | `			}` |
|      - | 3717 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|     55 | 3718 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 3719 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 3720 | `			}` |
|      - | 3721 | `#if 1` |
|      - | 3722 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 3723 | `        ** I think this is stupid.*/` |
|     55 | 3724 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 3725 | `#else` |
|      - | 3726 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 3727 | `        ** but leave the prefix for hex.*/` |
|      - | 3728 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 3729 | `#endif` |
|     55 | 3730 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|     25 | 3731 | `          if( iVal<0 ){` |
|      3 | 3732 | `            iVal = -iVal;` |
|      - | 3733 | `			/* Ticket 1433-003 */` |
|      3 | 3734 | `			if( iVal < 0 ){` |
|      - | 3735 | `				/* Overflow */` |
|    ! 0 | 3736 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3737 | `			}` |
|      3 | 3738 | `            prefix = '-';` |
|     24 | 3739 | `          }else if( flag_plussign )  prefix = '+';` |
|     21 | 3740 | `          else if( flag_blanksign )  prefix = ' ';` |
|     19 | 3741 | `          else                       prefix = 0;` |
|     13 | 3742 | `        }else{` |
|     31 | 3743 | `			if( iVal<0 ){` |
|    ! 0 | 3744 | `				iVal = -iVal;` |
|      - | 3745 | `				/* Ticket 1433-003 */` |
|    ! 0 | 3746 | `				if( iVal < 0 ){` |
|      - | 3747 | `					/* Overflow */` |
|    ! 0 | 3748 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3749 | `				}` |
|    ! 0 | 3750 | `			}` |
|     31 | 3751 | `			prefix = 0;` |
|      - | 3752 | `		}` |
|     55 | 3753 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      7 | 3754 | `          precision = width-(prefix!=0);` |
|      3 | 3755 | `        }` |
|     55 | 3756 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 3757 | `        {` |
|      - | 3758 | `          register char *cset;      /* Use registers for speed */` |
|      - | 3759 | `          register int base;` |
|     55 | 3760 | `          cset = pInfo->charset;` |
|     55 | 3761 | `          base = pInfo->base;` |
|     27 | 3762 | `          do{                                           /* Convert to ascii */` |
|    123 | 3763 | `            *(--zBuf) = cset[iVal%base];` |
|    123 | 3764 | `            iVal = iVal/base;` |
|    123 | 3765 | `          }while( iVal>0 );` |
|      - | 3766 | `        }` |
|     55 | 3767 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     77 | 3768 | `        for(idx=precision-length; idx>0; idx--){` |
|     23 | 3769 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|     12 | 3770 | `        }` |
|     55 | 3771 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|     55 | 3772 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 3773 | `          char *pre, x;` |
|      9 | 3774 | `          pre = pInfo->prefix;` |
|      9 | 3775 | `          if( *zBuf!=pre[0] ){` |
|     23 | 3776 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|      4 | 3777 | `          }` |
|      4 | 3778 | `        }` |
|     55 | 3779 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     55 | 3780 | `		break;` |
|     28 | 3781 | `		case PH7_FMT_FLOAT:` |
|      - | 3782 | `		case PH7_FMT_EXP:` |
|      - | 3783 | `		case PH7_FMT_GENERIC:{` |
|      - | 3784 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3785 | `		long double realvalue;` |
|      - | 3786 | `		int  exp;                /* exponent of real numbers */` |
|      - | 3787 | `		double rounder;          /* Used for rounding floating point values */` |
|      - | 3788 | `		int flag_dp;            /* True if decimal point should be shown */` |
|      - | 3789 | `		int flag_rtz;           /* True if trailing zeros should be removed */` |
|      - | 3790 | `		int flag_exp;           /* True to force display of the exponent */` |
|      - | 3791 | `		int nsd;                 /* Number of significant digits returned */` |
|     57 | 3792 | `		pArg = NEXT_ARG;` |
|     57 | 3793 | `		if( pArg == 0 ){` |
|    ! 0 | 3794 | `			realvalue = 0;` |
|    ! 0 | 3795 | `		}else{` |
|     57 | 3796 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 3797 | `		}` |
|      - | 3798 | `		/* Special-case NaN and infinities since the normal formatting logic` |
|      - | 3799 | `		 * below assumes a finite positive realvalue. */` |
|     57 | 3800 | `		if( PH7_IS_NAN(realvalue) ){` |
|    ! 0 | 3801 | `			zBuf = "NAN";` |
|    ! 0 | 3802 | `			length = 3;` |
|    ! 0 | 3803 | `			break;` |
|      - | 3804 | `		}` |
|     57 | 3805 | `		if( PH7_IS_INF(realvalue) ){` |
|      - | 3806 | `			/* Infinity prints as INF or -INF depending on sign. */` |
|    ! 0 | 3807 | `			if( realvalue < 0.0 ){` |
|    ! 0 | 3808 | `				zBuf = "-INF";` |
|    ! 0 | 3809 | `				length = 4;` |
|    ! 0 | 3810 | `			}else{` |
|    ! 0 | 3811 | `				zBuf = "INF";` |
|    ! 0 | 3812 | `				length = 3;` |
|      - | 3813 | `			}` |
|    ! 0 | 3814 | `			break;` |
|      - | 3815 | `		}` |
|     57 | 3816 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|     57 | 3817 | `		if( precision>PH7_FMT_BUFSIZ-40) precision = PH7_FMT_BUFSIZ-40;` |
|     57 | 3818 | `        if( realvalue<0.0 ){` |
|      3 | 3819 | `          realvalue = -realvalue;` |
|      3 | 3820 | `          prefix = '-';` |
|      2 | 3821 | `        }else{` |
|     55 | 3822 | `          if( flag_plussign )          prefix = '+';` |
|     55 | 3823 | `          else if( flag_blanksign )    prefix = ' ';` |
|     55 | 3824 | `          else                         prefix = 0;` |
|      - | 3825 | `        }` |
|     57 | 3826 | `        if( pInfo->type==PH7_FMT_GENERIC && precision>0 ) precision--;` |
|     57 | 3827 | `        rounder = 0.0;` |
|      - | 3828 | `#if 0` |
|      - | 3829 | `        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */` |
|      - | 3830 | `        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);` |
|      - | 3831 | `#else` |
|      - | 3832 | `        /* It makes more sense to use 0.5 */` |
|    405 | 3833 | `        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);` |
|      - | 3834 | `#endif` |
|     57 | 3835 | `        if( pInfo->type==PH7_FMT_FLOAT ) realvalue += rounder;` |
|      - | 3836 | `        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */` |
|     57 | 3837 | `        exp = 0;` |
|     57 | 3838 | `        if( realvalue>0.0 ){` |
|     61 | 3839 | `          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }` |
|     89 | 3840 | `          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }` |
|     61 | 3841 | `          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }` |
|     71 | 3842 | `          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }` |
|     57 | 3843 | `          if( exp>350 \|\| exp<-350 ){` |
|    ! 0 | 3844 | `            zBuf = "NaN";` |
|    ! 0 | 3845 | `            length = 3;` |
|    ! 0 | 3846 | `            break;` |
|      - | 3847 | `          }` |
|     28 | 3848 | `        }` |
|     57 | 3849 | `        zBuf = zWorker;` |
|      - | 3850 | `        /*` |
|      - | 3851 | `        ** If the field type is etGENERIC, then convert to either etEXP` |
|      - | 3852 | `        ** or etFLOAT, as appropriate.` |
|      - | 3853 | `        */` |
|     57 | 3854 | `        flag_exp = xtype==PH7_FMT_EXP;` |
|     57 | 3855 | `        if( xtype!=PH7_FMT_FLOAT ){` |
|    ! 0 | 3856 | `          realvalue += rounder;` |
|    ! 0 | 3857 | `          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }` |
|    ! 0 | 3858 | `        }` |
|     57 | 3859 | `        if( xtype==PH7_FMT_GENERIC ){` |
|    ! 0 | 3860 | `          flag_rtz = !flag_alternateform;` |
|    ! 0 | 3861 | `          if( exp<-4 \|\| exp>precision ){` |
|    ! 0 | 3862 | `            xtype = PH7_FMT_EXP;` |
|    ! 0 | 3863 | `          }else{` |
|    ! 0 | 3864 | `            precision = precision - exp;` |
|    ! 0 | 3865 | `            xtype = PH7_FMT_FLOAT;` |
|      - | 3866 | `          }` |
|    ! 0 | 3867 | `        }else{` |
|     57 | 3868 | `          flag_rtz = 0;` |
|      - | 3869 | `        }` |
|      - | 3870 | `        /*` |
|      - | 3871 | `        ** The "exp+precision" test causes output to be of type etEXP if` |
|      - | 3872 | `        ** the precision is too large to fit in buf[].` |
|      - | 3873 | `        */` |
|     57 | 3874 | `        nsd = 0;` |
|     57 | 3875 | `        if( xtype==PH7_FMT_FLOAT && exp+precision<PH7_FMT_BUFSIZ-30 ){` |
|     57 | 3876 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|     57 | 3877 | `          if( prefix ) *(zBuf++) = (char)prefix;         /* Sign */` |
|     57 | 3878 | `          if( exp<0 )  *(zBuf++) = '0';            /* Digits before "." */` |
|    149 | 3879 | `          else for(; exp>=0; exp--) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     57 | 3880 | `          if( flag_dp ) *(zBuf++) = '.';           /* The decimal point */` |
|     89 | 3881 | `          for(exp++; exp<0 && precision>0; precision--, exp++){` |
|     33 | 3882 | `            *(zBuf++) = '0';` |
|     17 | 3883 | `          }` |
|    373 | 3884 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     57 | 3885 | `          *(zBuf--) = 0;                           /* Null terminate */` |
|     57 | 3886 | `          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */` |
|    ! 0 | 3887 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 3888 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 3889 | `          }` |
|     57 | 3890 | `          zBuf++;                            /* point to next free slot */` |
|     29 | 3891 | `        }else{    /* etEXP or etGENERIC */` |
|    ! 0 | 3892 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|    ! 0 | 3893 | `          if( prefix ) *(zBuf++) = (char)prefix;   /* Sign */` |
|    ! 0 | 3894 | `          *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);  /* First digit */` |
|    ! 0 | 3895 | `          if( flag_dp ) *(zBuf++) = '.';     /* Decimal point */` |
|    ! 0 | 3896 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|    ! 0 | 3897 | `          zBuf--;                            /* point to last digit */` |
|    ! 0 | 3898 | `          if( flag_rtz && flag_dp ){          /* Remove tail zeros */` |
|    ! 0 | 3899 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 3900 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 3901 | `          }` |
|    ! 0 | 3902 | `          zBuf++;                            /* point to next free slot */` |
|    ! 0 | 3903 | `          if( exp \|\| flag_exp ){` |
|    ! 0 | 3904 | `            *(zBuf++) = pInfo->charset[0];` |
|    ! 0 | 3905 | `            if( exp<0 ){ *(zBuf++) = '-'; exp = -exp; } /* sign of exp */` |
|    ! 0 | 3906 | `            else       { *(zBuf++) = '+'; }` |
|    ! 0 | 3907 | `            if( exp>=100 ){` |
|    ! 0 | 3908 | `              *(zBuf++) = (char)((exp/100)+'0');                /* 100's digit */` |
|    ! 0 | 3909 | `              exp %= 100;` |
|    ! 0 | 3910 | `            }` |
|    ! 0 | 3911 | `            *(zBuf++) = (char)(exp/10+'0');                     /* 10's digit */` |
|    ! 0 | 3912 | `            *(zBuf++) = (char)(exp%10+'0');                     /* 1's digit */` |
|    ! 0 | 3913 | `          }` |
|      - | 3914 | `        }` |
|      - | 3915 | `        /* The converted number is in buf[] and zero terminated.Output it.` |
|      - | 3916 | `        ** Note that the number is in the usual order, not reversed as with` |
|      - | 3917 | `        ** integer conversions.*/` |
|     57 | 3918 | `        length = (int)(zBuf-zWorker);` |
|     57 | 3919 | `        zBuf = zWorker;` |
|      - | 3920 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 3921 | `        ** set and we are not left justified */` |
|     57 | 3922 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 3923 | `          int i;` |
|      3 | 3924 | `          int nPad = width - length;` |
|     13 | 3925 | `          for(i=width; i>=nPad; i--){` |
|     11 | 3926 | `            zBuf[i] = zBuf[i-nPad];` |
|      6 | 3927 | `          }` |
|      3 | 3928 | `          i = prefix!=0;` |
|      5 | 3929 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      3 | 3930 | `          length = width;` |
|      1 | 3931 | `        }` |
|      - | 3932 | `#else` |
|      - | 3933 | `         zBuf = " ";` |
|      - | 3934 | `		 length = (int)sizeof(char);` |
|      - | 3935 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|     57 | 3936 | `		 break;` |
|      - | 3937 | `							 }` |
|      1 | 3938 | `		default:` |
|      - | 3939 | `			/* Invalid format specifer */` |
|      3 | 3940 | `			zWorker[0] = '?';` |
|      3 | 3941 | `			length = (int)sizeof(char);` |
|      2 | 3942 | `			break;` |
|      - | 3943 | `		}` |
|      - | 3944 | `		 /*` |
|      - | 3945 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 3946 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 3947 | `		 ** the output.` |
|      - | 3948 | `		 */` |
|    143 | 3949 | `    if( !flag_leftjustify ){` |
|      - | 3950 | `      register int nspace;` |
|    135 | 3951 | `      nspace = width-length;` |
|    135 | 3952 | `      if( nspace>0 ){` |
|      5 | 3953 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3954 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3955 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3956 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3957 | `			}` |
|    ! 0 | 3958 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3959 | `        }` |
|      5 | 3960 | `        if( nspace>0 ){` |
|      5 | 3961 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      5 | 3962 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3963 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3964 | `			}` |
|      2 | 3965 | `		}` |
|      2 | 3966 | `      }` |
|     67 | 3967 | `    }` |
|    143 | 3968 | `    if( length>0 ){` |
|    143 | 3969 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    143 | 3970 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3971 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3972 | `		}` |
|     71 | 3973 | `    }` |
|    143 | 3974 | `    if( flag_leftjustify ){` |
|      - | 3975 | `      register int nspace;` |
|      9 | 3976 | `      nspace = width-length;` |
|      9 | 3977 | `      if( nspace>0 ){` |
|      9 | 3978 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3979 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3980 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3981 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3982 | `			}` |
|    ! 0 | 3983 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3984 | `        }` |
|      9 | 3985 | `        if( nspace>0 ){` |
|      9 | 3986 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      9 | 3987 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3988 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3989 | `			}` |
|      4 | 3990 | `		}` |
|      4 | 3991 | `      }` |
|      4 | 3992 | `    }` |
|      1 | 3993 | ` }/* for(;;) */` |
|    137 | 3994 | `	return SXRET_OK;` |
|     69 | 3995 |  |
|      - | 3996 | `/*` |
|      - | 3997 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 3998 | ` */` |
|     90 | 3999 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4000 |  |
|      - | 4001 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|      - | 4002 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|      - | 4003 | `	 * non-OK rc also stops the format loop. */` |
|     91 | 4004 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|     91 | 4005 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|     91 | 4006 | `	return *pRc;` |
|      1 | 4007 |  |
|      - | 4008 | `/*` |
|      - | 4009 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 4010 | ` *  Return a formatted string.` |
|      - | 4011 | ` * Parameters` |
|      - | 4012 | ` *  $format` |
|      - | 4013 | ` *    The format string (see block comment above)` |
|      - | 4014 | ` * Return` |
|      - | 4015 | ` *  A string produced according to the formatting string format.` |
|      - | 4016 | ` */` |
|     62 | 4017 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4018 |  |
|      - | 4019 | `	const char *zFormat;` |
|     63 | 4020 | `	sxi32 rc = SXRET_OK;` |
|      - | 4021 | `	int nLen;` |
|     63 | 4022 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4023 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 4024 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4025 | `		return PH7_OK;` |
|      - | 4026 | `	}` |
|      - | 4027 | `	/* Extract the string format */` |
|     61 | 4028 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     61 | 4029 | `	if( nLen < 1 ){` |
|      - | 4030 | `		/* Empty string */` |
|    ! 0 | 4031 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4032 | `		return PH7_OK;` |
|      - | 4033 | `	}` |
|      - | 4034 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|     61 | 4035 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|     61 | 4036 | `	if( rc != SXRET_OK ){` |
|      - | 4037 | `		/* The result append ran out of memory: raise a fatal rather than` |
|      - | 4038 | `		 * returning a silently-truncated string. */` |
|    ! 0 | 4039 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4040 | `	}` |
|     61 | 4041 | `	return PH7_OK;` |
|     32 | 4042 |  |
|      - | 4043 | `/*` |
|      - | 4044 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 4045 | ` */` |
|    130 | 4046 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4047 |  |
|    131 | 4048 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 4049 | `	/* Call the VM output consumer directly */` |
|    131 | 4050 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 4051 | `	/* Increment counter */` |
|    131 | 4052 | `	*pCounter += nLen;` |
|    131 | 4053 | `	return PH7_OK;` |
|      1 | 4054 |  |
|      - | 4055 | `/*` |
|      - | 4056 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 4057 | ` *  Output a formatted string.` |
|      - | 4058 | ` * Parameters` |
|      - | 4059 | ` *  $format` |
|      - | 4060 | ` *   See sprintf() for a description of format.` |
|      - | 4061 | ` * Return` |
|      - | 4062 | ` *  The length of the outputted string.` |
|      - | 4063 | ` */` |
|     52 | 4064 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4065 |  |
|     53 | 4066 | `	ph7_int64 nCounter = 0;` |
|      - | 4067 | `	const char *zFormat;` |
|      - | 4068 | `	int nLen;` |
|     53 | 4069 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4070 | `		/* Missing/Invalid arguments,return 0 */` |
|      3 | 4071 | `		ph7_result_int(pCtx,0);` |
|      3 | 4072 | `		return PH7_OK;` |
|      - | 4073 | `	}` |
|      - | 4074 | `	/* Extract the string format */` |
|     51 | 4075 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     51 | 4076 | `	if( nLen < 1 ){` |
|      - | 4077 | `		/* Empty string */` |
|    ! 0 | 4078 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4079 | `		return PH7_OK;` |
|      - | 4080 | `	}` |
|      - | 4081 | `	/* Format the string */` |
|     51 | 4082 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 4083 | `	/* Return the length of the outputted string */` |
|     51 | 4084 | `	ph7_result_int64(pCtx,nCounter);` |
|     51 | 4085 | `	return PH7_OK;` |
|     27 | 4086 |  |
|      - | 4087 | `/*` |
|      - | 4088 | ` * int vprintf(string $format,array $args)` |
|      - | 4089 | ` *  Output a formatted string.` |
|      - | 4090 | ` * Parameters` |
|      - | 4091 | ` *  $format` |
|      - | 4092 | ` *   See sprintf() for a description of format.` |
|      - | 4093 | ` * Return` |
|      - | 4094 | ` *  The length of the outputted string.` |
|      - | 4095 | ` */` |
|      2 | 4096 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4097 |  |
|      3 | 4098 | `	ph7_int64 nCounter = 0;` |
|      - | 4099 | `	const char *zFormat;` |
|      - | 4100 | `	ph7_hashmap *pMap;` |
|      - | 4101 | `	SySet sArg;` |
|      - | 4102 | `	int nLen,n;` |
|      3 | 4103 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4104 | `		/* Missing/Invalid arguments,return 0 */` |
|    ! 0 | 4105 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4106 | `		return PH7_OK;` |
|      - | 4107 | `	}` |
|      - | 4108 | `	/* Extract the string format */` |
|      3 | 4109 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4110 | `	if( nLen < 1 ){` |
|      - | 4111 | `		/* Empty string */` |
|    ! 0 | 4112 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4113 | `		return PH7_OK;` |
|      - | 4114 | `	}` |
|      - | 4115 | `	/* Point to the hashmap */` |
|      3 | 4116 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4117 | `	/* Extract arguments from the hashmap */` |
|      3 | 4118 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4119 | `	/* Format the string */` |
|      3 | 4120 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 4121 | `	/* Return the length of the outputted string */` |
|      3 | 4122 | `	ph7_result_int64(pCtx,nCounter);` |
|      - | 4123 | `	/* Release the container */` |
|      3 | 4124 | `	SySetRelease(&sArg);` |
|      3 | 4125 | `	return PH7_OK;` |
|      2 | 4126 |  |
|      - | 4127 | `/*` |
|      - | 4128 | ` * int vsprintf(string $format,array $args)` |
|      - | 4129 | ` *  Output a formatted string.` |
|      - | 4130 | ` * Parameters` |
|      - | 4131 | ` *  $format` |
|      - | 4132 | ` *   See sprintf() for a description of format.` |
|      - | 4133 | ` * Return` |
|      - | 4134 | ` *  A string produced according to the formatting string format.` |
|      - | 4135 | ` */` |
|     10 | 4136 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4137 |  |
|      - | 4138 | `	const char *zFormat;` |
|      - | 4139 | `	ph7_hashmap *pMap;` |
|      - | 4140 | `	SySet sArg;` |
|     11 | 4141 | `	sxi32 rc = SXRET_OK;` |
|      - | 4142 | `	int nLen,n;` |
|     11 | 4143 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4144 | `		/* Missing/Invalid arguments,return the empty string */` |
|      5 | 4145 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 4146 | `		return PH7_OK;` |
|      - | 4147 | `	}` |
|      - | 4148 | `	/* Extract the string format */` |
|      7 | 4149 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4150 | `	if( nLen < 1 ){` |
|      - | 4151 | `		/* Empty string */` |
|    ! 0 | 4152 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4153 | `		return PH7_OK;` |
|      - | 4154 | `	}` |
|      - | 4155 | `	/* Point to hashmap */` |
|      7 | 4156 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4157 | `	/* Extract arguments from the hashmap */` |
|      7 | 4158 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4159 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|      7 | 4160 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|      - | 4161 | `	/* Release the container */` |
|      7 | 4162 | `	SySetRelease(&sArg);` |
|      7 | 4163 | `	if( rc != SXRET_OK ){` |
|      - | 4164 | `		/* The result append ran out of memory: raise a fatal. */` |
|    ! 0 | 4165 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4166 | `	}` |
|      7 | 4167 | `	return PH7_OK;` |
|      6 | 4168 |  |
|      - | 4169 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 4170 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4171 | `/*` |
|      - | 4172 | ` * Symisc eXtension.` |
|      - | 4173 | ` * string size_format(int64 $size)` |
|      - | 4174 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 4175 | ` *  Example:` |
|      - | 4176 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 4177 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 4178 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 4179 | ` * Parameter` |
|      - | 4180 | ` *  $size` |
|      - | 4181 | ` *    Entity size in bytes.` |
|      - | 4182 | ` * Return` |
|      - | 4183 | ` *   Formatted string representation of the given size.` |
|      - | 4184 | ` */` |
|     24 | 4185 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4186 |  |
|      - | 4187 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 4188 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 4189 | `	sxi32 nRest,i_32;` |
|      - | 4190 | `	ph7_int64 iSize;` |
|     25 | 4191 | `	int c = -1; /* index in zUnit[] */` |
|      - | 4192 |  |
|     25 | 4193 | `	if( nArg < 1 ){` |
|      - | 4194 | `		/* Missing argument,return the empty string */` |
|      3 | 4195 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4196 | `		return PH7_OK;` |
|      - | 4197 | `	}` |
|      - | 4198 | `	/* Extract the given size */` |
|     23 | 4199 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 4200 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 4201 | `		/* Don't bother formatting,return immediately */` |
|      5 | 4202 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 4203 | `		return PH7_OK;` |
|      - | 4204 | `	}` |
|     19 | 4205 | `	for(;;){` |
|     39 | 4206 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 4207 | `		iSize >>= 10;` |
|     39 | 4208 | `		c++;` |
|     39 | 4209 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 4210 | `			break;` |
|      - | 4211 | `		}` |
|      1 | 4212 | `	}` |
|     19 | 4213 | `	nRest /= 100;` |
|     19 | 4214 | `	if( nRest > 9 ){` |
|    ! 0 | 4215 | `		nRest = 9;` |
|    ! 0 | 4216 | `	}` |
|     19 | 4217 | `	if( iSize > 999 ){` |
|    ! 0 | 4218 | `		c++;` |
|    ! 0 | 4219 | `		nRest = 9;` |
|    ! 0 | 4220 | `		iSize = 0;` |
|    ! 0 | 4221 | `	}` |
|     19 | 4222 | `	i_32 = (sxi32)iSize;` |
|      - | 4223 | `	/* Format */` |
|     19 | 4224 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 4225 | `	return PH7_OK;` |
|     13 | 4226 |  |
|      - | 4227 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4228 | `/*` |
|      - | 4229 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 4230 | ` *   Calculate the md5 hash of a string.` |
|      - | 4231 | ` * Parameter` |
|      - | 4232 | ` *  $str` |
|      - | 4233 | ` *   Input string` |
|      - | 4234 | ` * $raw_output` |
|      - | 4235 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4236 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4237 | ` * Return` |
|      - | 4238 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 4239 | ` */` |
|     14 | 4240 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4241 |  |
|      - | 4242 | `	unsigned char zDigest[16];` |
|     15 | 4243 | `	int raw_output = FALSE;` |
|      - | 4244 | `	const void *pIn;` |
|      - | 4245 | `	int nLen;` |
|     15 | 4246 | `	if( nArg < 1 ){` |
|      - | 4247 | `		/* Missing arguments,return the empty string */` |
|      3 | 4248 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4249 | `		return PH7_OK;` |
|      - | 4250 | `	}` |
|      - | 4251 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 4252 | `	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */` |
|     13 | 4253 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 4254 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 4255 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 4256 | `	}` |
|      - | 4257 | `	/* Compute the MD5 digest */` |
|     13 | 4258 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|     13 | 4259 | `	if( raw_output ){` |
|      - | 4260 | `		/* Output raw digest */` |
|      5 | 4261 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 4262 | `	}else{` |
|      - | 4263 | `		/* Perform a binary to hex conversion */` |
|      9 | 4264 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4265 | `	}` |
|     13 | 4266 | `	return PH7_OK;` |
|      8 | 4267 |  |
|      - | 4268 | `/*` |
|      - | 4269 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 4270 | ` *   Calculate the sha1 hash of a string.` |
|      - | 4271 | ` * Parameter` |
|      - | 4272 | ` *  $str` |
|      - | 4273 | ` *   Input string` |
|      - | 4274 | ` * $raw_output` |
|      - | 4275 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4276 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4277 | ` * Return` |
|      - | 4278 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 4279 | ` */` |
|     12 | 4280 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4281 |  |
|      - | 4282 | `	unsigned char zDigest[20];` |
|     13 | 4283 | `	int raw_output = FALSE;` |
|      - | 4284 | `	const void *pIn;` |
|      - | 4285 | `	int nLen;` |
|     13 | 4286 | `	if( nArg < 1 ){` |
|      - | 4287 | `		/* Missing arguments,return the empty string */` |
|      3 | 4288 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4289 | `		return PH7_OK;` |
|      - | 4290 | `	}` |
|      - | 4291 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 4292 | `	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */` |
|     11 | 4293 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 4294 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 4295 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 4296 | `	}` |
|      - | 4297 | `	/* Compute the SHA1 digest */` |
|     11 | 4298 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|     11 | 4299 | `	if( raw_output ){` |
|      - | 4300 | `		/* Output raw digest */` |
|      5 | 4301 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 4302 | `	}else{` |
|      - | 4303 | `		/* Perform a binary to hex conversion */` |
|      7 | 4304 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4305 | `	}` |
|     11 | 4306 | `	return PH7_OK;` |
|      7 | 4307 |  |
|      - | 4308 | `/*` |
|      - | 4309 | ` * int64 crc32(string $str)` |
|      - | 4310 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 4311 | ` * Parameter` |
|      - | 4312 | ` *  $str` |
|      - | 4313 | ` *   Input string` |
|      - | 4314 | ` * Return` |
|      - | 4315 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 4316 | ` */` |
|      4 | 4317 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4318 |  |
|      - | 4319 | `	const void *pIn;` |
|      - | 4320 | `	sxu32 nCRC;` |
|      - | 4321 | `	int nLen;` |
|      5 | 4322 | `	if( nArg < 1 ){` |
|      - | 4323 | `		/* Missing arguments,return 0 */` |
|      3 | 4324 | `		ph7_result_int(pCtx,0);` |
|      3 | 4325 | `		return PH7_OK;` |
|      - | 4326 | `	}` |
|      - | 4327 | `	/* Extract the input string */` |
|      3 | 4328 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4329 | `	if( nLen < 1 ){` |
|      - | 4330 | `		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike` |
|      - | 4331 | `		 * md5()/sha1(), whose empty-string digests are non-zero. */` |
|    ! 0 | 4332 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4333 | `		return PH7_OK;` |
|      - | 4334 | `	}` |
|      - | 4335 | `	/* Calculate the sum */` |
|      3 | 4336 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 4337 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 4338 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 4339 | `	return PH7_OK;` |
|      3 | 4340 |  |
|      - | 4341 | `/*` |
|      - | 4342 | ` * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is` |
|      - | 4343 | ` * described by a small record so one dispatch (and one generic HMAC) serves them` |
|      - | 4344 | ` * all. Thin adapters normalize the differing context types and the reversed` |
|      - | 4345 | ` * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.` |
|      - | 4346 | ` */` |
|     11 | 4347 | `static void HashMd5Init(HashCtx *c){ MD5Init(&c->md5); }` |
|     15 | 4348 | `static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }` |
|     11 | 4349 | `static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }` |
|     11 | 4350 | `static void HashSha1Init(HashCtx *c){ SHA1Init(&c->sha1); }` |
|     15 | 4351 | `static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }` |
|     11 | 4352 | `static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }` |
|      9 | 4353 | `static void HashSha224Init(HashCtx *c){ SHA224Init(&c->sha256); }` |
|     33 | 4354 | `static void HashSha256Init(HashCtx *c){ SHA256Init(&c->sha256); }` |
|     57 | 4355 | `static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }` |
|     41 | 4356 | `static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }` |
|      9 | 4357 | `static void HashSha384Init(HashCtx *c){ SHA384Init(&c->sha512); }` |
|     15 | 4358 | `static void HashSha512Init(HashCtx *c){ SHA512Init(&c->sha512); }` |
|     27 | 4359 | `static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }` |
|     23 | 4360 | `static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }` |
|      - | 4361 | `typedef struct HashAlgo HashAlgo;` |
|      - | 4362 | `struct HashAlgo {` |
|      - | 4363 | `	const char *zName;   /* lowercase canonical name */` |
|      - | 4364 | `	int nDigestLen;      /* output bytes: 16/20/28/32/48/64 */` |
|      - | 4365 | `	int nBlockLen;       /* internal block bytes (for HMAC): 64 or 128 */` |
|      - | 4366 | `	void (*xInit)(HashCtx *);` |
|      - | 4367 | `	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);` |
|      - | 4368 | `	void (*xFinal)(HashCtx *,unsigned char *);` |
|      - | 4369 | `};` |
|      - | 4370 | `static const HashAlgo aHashAlgo[] = {` |
|      - | 4371 | `	{ "md5",    16, 64,  HashMd5Init,    HashMd5Update,    HashMd5Final    },` |
|      - | 4372 | `	{ "sha1",   20, 64,  HashSha1Init,   HashSha1Update,   HashSha1Final   },` |
|      - | 4373 | `	{ "sha224", 28, 64,  HashSha224Init, HashSha256Update, HashSha256Final },` |
|      - | 4374 | `	{ "sha256", 32, 64,  HashSha256Init, HashSha256Update, HashSha256Final },` |
|      - | 4375 | `	{ "sha384", 48, 128, HashSha384Init, HashSha512Update, HashSha512Final },` |
|      - | 4376 | `	{ "sha512", 64, 128, HashSha512Init, HashSha512Update, HashSha512Final },` |
|      - | 4377 | `};` |
|      - | 4378 | `/* Case-insensitive algorithm lookup (PHP accepts 'SHA256' etc.). */` |
|     73 | 4379 | `static const HashAlgo * HashFindAlgo(const char *zName,int nLen){` |
|      - | 4380 | `	sxu32 i;` |
|    279 | 4381 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|    272 | 4382 | `		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen` |
|    211 | 4383 | `			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){` |
|     67 | 4384 | `			return &aHashAlgo[i];` |
|      - | 4385 | `		}` |
|    106 | 4386 | `	}` |
|      6 | 4387 | `	return 0;` |
|     38 | 4388 |  |
|      - | 4389 | `/*` |
|      - | 4390 | ` * string hash(string $algo,string $data[,bool $binary = false])` |
|      - | 4391 | ` *   Generate a hash value (message digest).` |
|      - | 4392 | ` */` |
|     54 | 4393 | `static int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4394 |  |
|      - | 4395 | `	const HashAlgo *pAlgo;` |
|      - | 4396 | `	const char *zAlgo,*zData;` |
|     56 | 4397 | `	int nAlgoLen,nDataLen,raw_output = FALSE;` |
|      - | 4398 | `	HashCtx sCtx;` |
|      - | 4399 | `	unsigned char zDigest[64];` |
|     56 | 4400 | `	if( nArg < 2 ){` |
|    ! 0 | 4401 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4402 | `			"hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4403 | `	}` |
|     56 | 4404 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     56 | 4405 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     56 | 4406 | `	if( pAlgo == 0 ){` |
|      3 | 4407 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4408 | `			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - | 4409 | `	}` |
|     53 | 4410 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     53 | 4411 | `	if( nArg > 2 ){` |
|      9 | 4412 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|      4 | 4413 | `	}` |
|     53 | 4414 | `	pAlgo->xInit(&sCtx);` |
|     53 | 4415 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     53 | 4416 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     53 | 4417 | `	if( raw_output ){` |
|      9 | 4418 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|      5 | 4419 | `	}else{` |
|     45 | 4420 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - | 4421 | `	}` |
|     53 | 4422 | `	return PH7_OK;` |
|     29 | 4423 |  |
|      - | 4424 | `/*` |
|      - | 4425 | ` * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])` |
|      - | 4426 | ` *   Generate a keyed hash value using the HMAC method (RFC 2104).` |
|      - | 4427 | ` */` |
|     16 | 4428 | `static int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4429 |  |
|      - | 4430 | `	const HashAlgo *pAlgo;` |
|      - | 4431 | `	const char *zAlgo,*zData,*zKey;` |
|     18 | 4432 | `	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;` |
|      - | 4433 | `	HashCtx sCtx;` |
|      - | 4434 | `	unsigned char zKeyBlock[128],zIpad[128],zOpad[128],zInner[64],zDigest[64];` |
|      - | 4435 | `	int i,nBlock,nDigest;` |
|     18 | 4436 | `	if( nArg < 3 ){` |
|    ! 0 | 4437 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4438 | `			"hash_hmac() expects at least 3 arguments, %d given",nArg);` |
|      - | 4439 | `	}` |
|     18 | 4440 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     18 | 4441 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     18 | 4442 | `	if( pAlgo == 0 ){` |
|      3 | 4443 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4444 | `			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 4445 | `	}` |
|     15 | 4446 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     15 | 4447 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|     15 | 4448 | `	if( nArg > 3 ){` |
|      3 | 4449 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|      1 | 4450 | `	}` |
|     15 | 4451 | `	nBlock = pAlgo->nBlockLen;` |
|     15 | 4452 | `	nDigest = pAlgo->nDigestLen;` |
|      - | 4453 | `	/* Reduce the key to a single block: hash it if longer than the block, then` |
|      - | 4454 | `	 * zero-pad (a short or empty key is just zero-padded). */` |
|     15 | 4455 | `	SyZero(zKeyBlock,sizeof(zKeyBlock));` |
|     15 | 4456 | `	if( nKeyLen > nBlock ){` |
|      3 | 4457 | `		pAlgo->xInit(&sCtx);` |
|      3 | 4458 | `		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);` |
|      3 | 4459 | `		pAlgo->xFinal(&sCtx,zKeyBlock);` |
|     14 | 4460 | `	}else if( nKeyLen > 0 ){` |
|     11 | 4461 | `		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);` |
|      5 | 4462 | `	}` |
|   1039 | 4463 | `	for( i = 0; i < nBlock; i++ ){` |
|   1025 | 4464 | `		zIpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);` |
|   1025 | 4465 | `		zOpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);` |
|    513 | 4466 | `	}` |
|      - | 4467 | `	/* inner = H((key ^ ipad) \|\| data) */` |
|     15 | 4468 | `	pAlgo->xInit(&sCtx);` |
|     15 | 4469 | `	pAlgo->xUpdate(&sCtx,zIpad,(unsigned int)nBlock);` |
|     15 | 4470 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     15 | 4471 | `	pAlgo->xFinal(&sCtx,zInner);` |
|      - | 4472 | `	/* out = H((key ^ opad) \|\| inner) */` |
|     15 | 4473 | `	pAlgo->xInit(&sCtx);` |
|     15 | 4474 | `	pAlgo->xUpdate(&sCtx,zOpad,(unsigned int)nBlock);` |
|     15 | 4475 | `	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)nDigest);` |
|     15 | 4476 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     15 | 4477 | `	if( raw_output ){` |
|      3 | 4478 | `		ph7_result_string(pCtx,(const char *)zDigest,nDigest);` |
|      2 | 4479 | `	}else{` |
|     13 | 4480 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);` |
|      - | 4481 | `	}` |
|     15 | 4482 | `	return PH7_OK;` |
|     10 | 4483 |  |
|      - | 4484 | `/*` |
|      - | 4485 | ` * bool hash_equals(string $known_string,string $user_string)` |
|      - | 4486 | ` *   Timing-attack-safe string comparison.` |
|      - | 4487 | ` */` |
|     14 | 4488 | `static int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4489 |  |
|      - | 4490 | `	const char *zKnown,*zUser;` |
|      - | 4491 | `	int nKnown,nUser,i;` |
|     17 | 4492 | `	volatile unsigned char vDiff = 0;` |
|     17 | 4493 | `	if( nArg < 2 ){` |
|    ! 0 | 4494 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4495 | `			"hash_equals() expects exactly 2 arguments, %d given",nArg);` |
|      - | 4496 | `	}` |
|     17 | 4497 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      4 | 4498 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4499 | `			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",` |
|      1 | 4500 | `			ph7_type_name(apArg[0]));` |
|      - | 4501 | `	}` |
|     14 | 4502 | `	if( !ph7_value_is_string(apArg[1]) ){` |
|      4 | 4503 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4504 | `			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",` |
|      2 | 4505 | `			ph7_type_name(apArg[1]));` |
|      - | 4506 | `	}` |
|     11 | 4507 | `	zKnown = ph7_value_to_string(apArg[0],&nKnown);` |
|     11 | 4508 | `	zUser = ph7_value_to_string(apArg[1],&nUser);` |
|     11 | 4509 | `	if( nKnown != nUser ){` |
|      5 | 4510 | `		ph7_result_bool(pCtx,0);` |
|      5 | 4511 | `		return PH7_OK;` |
|      - | 4512 | `	}` |
|      - | 4513 | `	/* Constant-time: read every byte, never short-circuit. */` |
|     19 | 4514 | `	for( i = 0; i < nKnown; i++ ){` |
|     13 | 4515 | `		vDiff \|= (unsigned char)(zKnown[i] ^ zUser[i]);` |
|      7 | 4516 | `	}` |
|      7 | 4517 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|      7 | 4518 | `	return PH7_OK;` |
|     10 | 4519 |  |
|      - | 4520 | `/*` |
|      - | 4521 | ` * array hash_algos(void)` |
|      - | 4522 | ` *   Return a list of the registered hashing algorithms.` |
|      - | 4523 | ` */` |
|      2 | 4524 | `static int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4525 |  |
|      - | 4526 | `	ph7_value *pArray,*pValue;` |
|      - | 4527 | `	sxu32 i;` |
|      1 | 4528 | `	SXUNUSED(nArg);` |
|      1 | 4529 | `	SXUNUSED(apArg);` |
|      3 | 4530 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4531 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 4532 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 4533 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4534 | `		return PH7_OK;` |
|      - | 4535 | `	}` |
|     15 | 4536 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|     13 | 4537 | `		ph7_value_string(pValue,aHashAlgo[i].zName,-1);` |
|     13 | 4538 | `		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);` |
|     13 | 4539 | `		ph7_value_reset_string_cursor(pValue);` |
|      7 | 4540 | `	}` |
|      3 | 4541 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4542 | `	return PH7_OK;` |
|      2 | 4543 |  |
|      - | 4544 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 4545 | `/*` |
|      - | 4546 | ` * password_* (bcrypt). These live in ext/standard in real PHP — outside the` |
|      - | 4547 | ` * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.` |
|      - | 4548 | ` */` |
|      - | 4549 | `/*` |
|      - | 4550 | ` * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a` |
|      - | 4551 | ` * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).` |
|      - | 4552 | ` */` |
|     40 | 4553 | `static int BcryptParseHash(const char *zHash,int nHash,int *piCost)` |
|      1 | 4554 |  |
|      - | 4555 | `	int iCost;` |
|     40 | 4556 | `	if( nHash != 60 \|\| zHash[0] != '$' \|\| zHash[1] != '2' \|\| zHash[3] != '$'` |
|     29 | 4557 | `		\|\| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){` |
|     13 | 4558 | `		return FALSE;` |
|      - | 4559 | `	}` |
|     29 | 4560 | `	if( zHash[4] < '0' \|\| zHash[4] > '9' \|\| zHash[5] < '0' \|\| zHash[5] > '9' \|\| zHash[6] != '$' ){` |
|    ! 0 | 4561 | `		return FALSE;` |
|      - | 4562 | `	}` |
|     29 | 4563 | `	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');` |
|     29 | 4564 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      3 | 4565 | `		return FALSE;` |
|      - | 4566 | `	}` |
|     27 | 4567 | `	if( piCost ){ *piCost = iCost; }` |
|     27 | 4568 | `	return TRUE;` |
|     21 | 4569 |  |
|      - | 4570 | `/*` |
|      - | 4571 | ` * TRUE if the $algo argument selects bcrypt: null (PASSWORD_DEFAULT) or the` |
|      - | 4572 | ` * "2y" id (PASSWORD_BCRYPT/PASSWORD_DEFAULT). bcrypt is the only supported algo.` |
|      - | 4573 | ` */` |
|     20 | 4574 | `static int BcryptIsBcryptAlgo(ph7_value *pAlgo)` |
|      3 | 4575 |  |
|     23 | 4576 | `	if( ph7_value_is_null(pAlgo) ){` |
|    ! 0 | 4577 | `		return TRUE;` |
|      - | 4578 | `	}` |
|     23 | 4579 | `	if( ph7_value_is_string(pAlgo) ){` |
|      - | 4580 | `		int nAlgo;` |
|     23 | 4581 | `		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);` |
|     23 | 4582 | `		return ( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' );` |
|      - | 4583 | `	}` |
|    ! 0 | 4584 | `	return FALSE;` |
|     13 | 4585 |  |
|      - | 4586 | `/*` |
|      - | 4587 | ` * bool\|string password_hash(string $password,string\|int\|null $algo[,array $options])` |
|      - | 4588 | ` *  Create a bcrypt hash of the password.` |
|      - | 4589 | ` */` |
|     16 | 4590 | `static int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4591 |  |
|      - | 4592 | `	const char *zPwd;` |
|     19 | 4593 | `	int nPwd,iCost = 12;` |
|      - | 4594 | `	unsigned char aSalt[16];` |
|      - | 4595 | `	char zHash[60];` |
|     19 | 4596 | `	if( nArg < 2 ){` |
|    ! 0 | 4597 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4598 | `			"password_hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4599 | `	}` |
|     19 | 4600 | `	if( !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      3 | 4601 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4602 | `			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");` |
|      - | 4603 | `	}` |
|      - | 4604 | `	/* cost from $options['cost'] (default 12). */` |
|     16 | 4605 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|     14 | 4606 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|     14 | 4607 | `		if( pCost ){ iCost = ph7_value_to_int(pCost); }` |
|      6 | 4608 | `	}` |
|     16 | 4609 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      4 | 4610 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 4611 | `			"Invalid bcrypt cost parameter specified: %d",iCost);` |
|      - | 4612 | `	}` |
|     13 | 4613 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     13 | 4614 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 4615 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - | 4616 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|      - | 4617 | `	}` |
|     13 | 4618 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){` |
|    ! 0 | 4619 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4620 | `		return PH7_OK;` |
|      - | 4621 | `	}` |
|     13 | 4622 | `	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));` |
|     13 | 4623 | `	return PH7_OK;` |
|     11 | 4624 |  |
|      - | 4625 | `/*` |
|      - | 4626 | ` * bool password_verify(string $password,string $hash)` |
|      - | 4627 | ` *  Verify a password against a bcrypt hash. Never throws on a malformed hash.` |
|      - | 4628 | ` */` |
|     28 | 4629 | `static int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4630 |  |
|      - | 4631 | `	const char *zPwd,*zHash;` |
|      - | 4632 | `	int nPwd,nHash,iCost,i;` |
|      - | 4633 | `	unsigned char aSalt[16];` |
|      - | 4634 | `	char zComputed[60];` |
|     29 | 4635 | `	volatile unsigned char vDiff = 0;` |
|     29 | 4636 | `	if( nArg < 2 ){` |
|    ! 0 | 4637 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4638 | `			"password_verify() expects exactly 2 arguments, %d given",nArg);` |
|      - | 4639 | `	}` |
|     29 | 4640 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     29 | 4641 | `	zHash = ph7_value_to_string(apArg[1],&nHash);` |
|     29 | 4642 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|     11 | 4643 | `		ph7_result_bool(pCtx,0);` |
|     11 | 4644 | `		return PH7_OK;` |
|      - | 4645 | `	}` |
|      - | 4646 | `	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */` |
|     19 | 4647 | `	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 4648 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4649 | `		return PH7_OK;` |
|      - | 4650 | `	}` |
|     19 | 4651 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){` |
|    ! 0 | 4652 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4653 | `		return PH7_OK;` |
|      - | 4654 | `	}` |
|      - | 4655 | `	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps` |
|      - | 4656 | `	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */` |
|    577 | 4657 | `	for( i = 29; i < 60; i++ ){` |
|    559 | 4658 | `		vDiff \|= (unsigned char)(zComputed[i] ^ zHash[i]);` |
|    280 | 4659 | `	}` |
|     19 | 4660 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|     19 | 4661 | `	return PH7_OK;` |
|     15 | 4662 |  |
|      - | 4663 | `/*` |
|      - | 4664 | ` * array password_get_info(string $hash)` |
|      - | 4665 | ` *  Return ["algo"=>id\|null, "algoName"=>name, "options"=>[...]].` |
|      - | 4666 | ` */` |
|      6 | 4667 | `static int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4668 |  |
|      7 | 4669 | `	const char *zHash = "";` |
|      7 | 4670 | `	int nHash,iCost = 0,bBcrypt = 0;` |
|      - | 4671 | `	ph7_value *pArray,*pOptions,*pVal;` |
|      7 | 4672 | `	if( nArg > 0 ){` |
|      7 | 4673 | `		zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 4674 | `		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);` |
|      3 | 4675 | `	}` |
|      7 | 4676 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 4677 | `	pOptions = ph7_context_new_array(pCtx);` |
|      7 | 4678 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      7 | 4679 | `	if( pArray == 0 \|\| pOptions == 0 \|\| pVal == 0 ){` |
|    ! 0 | 4680 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4681 | `		return PH7_OK;` |
|      - | 4682 | `	}` |
|      7 | 4683 | `	if( bBcrypt ){` |
|      5 | 4684 | `		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */` |
|      5 | 4685 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 4686 | `		ph7_value_reset_string_cursor(pVal);` |
|      5 | 4687 | `		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);` |
|      5 | 4688 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      5 | 4689 | `		ph7_value_int(pVal,iCost);` |
|      5 | 4690 | `		ph7_array_add_strkey_elem(pOptions,"cost",pVal);` |
|      3 | 4691 | `	}else{` |
|      3 | 4692 | `		ph7_value_null(pVal);                          /* algo => null */` |
|      3 | 4693 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      3 | 4694 | `		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);` |
|      3 | 4695 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      - | 4696 | `	}` |
|      7 | 4697 | `	ph7_array_add_strkey_elem(pArray,"options",pOptions);` |
|      7 | 4698 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 4699 | `	return PH7_OK;` |
|      4 | 4700 |  |
|      - | 4701 | `/*` |
|      - | 4702 | ` * bool password_needs_rehash(string $hash,string\|int\|null $algo[,array $options])` |
|      - | 4703 | ` *  True if the hash was not made with the given algo/options.` |
|      - | 4704 | ` */` |
|      6 | 4705 | `static int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4706 |  |
|      - | 4707 | `	const char *zHash;` |
|      7 | 4708 | `	int nHash,iCost = 0,iWantCost = 12;` |
|      7 | 4709 | `	if( nArg < 2 ){` |
|    ! 0 | 4710 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4711 | `			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4712 | `	}` |
|      7 | 4713 | `	zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 4714 | `	if( !BcryptParseHash(zHash,nHash,&iCost) \|\| !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      - | 4715 | `		/* A non-bcrypt hash, or a request for a different algo → needs rehash. */` |
|      3 | 4716 | `		ph7_result_bool(pCtx,1);` |
|      3 | 4717 | `		return PH7_OK;` |
|      - | 4718 | `	}` |
|      5 | 4719 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|      5 | 4720 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|      5 | 4721 | `		if( pCost ){ iWantCost = ph7_value_to_int(pCost); }` |
|      2 | 4722 | `	}` |
|      5 | 4723 | `	ph7_result_bool(pCtx,iCost != iWantCost);` |
|      5 | 4724 | `	return PH7_OK;` |
|      4 | 4725 |  |
|      - | 4726 | `/*` |
|      - | 4727 | ` * filter_var() — input validation and sanitization (the ext/filter API).` |
|      - | 4728 | ` *` |
|      - | 4729 | ` * Filter and flag identifiers (values match PHP 8.5; the constants themselves` |
|      - | 4730 | ` * are registered in constant.c). The validate filters are hand-rolled rather` |
|      - | 4731 | ` * than delegating to SyStrToInt64/SyStrToReal: the former silently skips leading` |
|      - | 4732 | ` * zeros and cannot signal overflow, and the latter treats ',' as a decimal point` |
|      - | 4733 | ` * unconditionally — neither matches PHP's filter semantics.` |
|      - | 4734 | ` */` |
|      - | 4735 | `#define FV_VALIDATE_INT     257` |
|      - | 4736 | `#define FV_VALIDATE_BOOLEAN 258` |
|      - | 4737 | `#define FV_VALIDATE_FLOAT   259` |
|      - | 4738 | `#define FV_VALIDATE_REGEXP  272` |
|      - | 4739 | `#define FV_VALIDATE_URL     273` |
|      - | 4740 | `#define FV_VALIDATE_EMAIL   274` |
|      - | 4741 | `#define FV_VALIDATE_IP      275` |
|      - | 4742 | `#define FV_VALIDATE_MAC     276` |
|      - | 4743 | `#define FV_VALIDATE_DOMAIN  277` |
|      - | 4744 | `#define FV_SANITIZE_SPECIAL_CHARS      515` |
|      - | 4745 | `#define FV_DEFAULT          516 /* == FILTER_UNSAFE_RAW: pass the value through */` |
|      - | 4746 | `#define FV_SANITIZE_EMAIL   517` |
|      - | 4747 | `#define FV_SANITIZE_URL     518` |
|      - | 4748 | `#define FV_SANITIZE_NUMBER_INT   519` |
|      - | 4749 | `#define FV_SANITIZE_NUMBER_FLOAT 520` |
|      - | 4750 | `#define FV_SANITIZE_FULL_SPECIAL_CHARS 522` |
|      - | 4751 | `#define FV_FLAG_ALLOW_OCTAL  1` |
|      - | 4752 | `#define FV_FLAG_ALLOW_HEX    2` |
|      - | 4753 | `#define FV_FLAG_ALLOW_FRACTION   4096` |
|      - | 4754 | `#define FV_FLAG_ALLOW_THOUSAND   8192` |
|      - | 4755 | `#define FV_FLAG_ALLOW_SCIENTIFIC 16384` |
|      - | 4756 | `#define FV_FLAG_IPV4  1048576` |
|      - | 4757 | `#define FV_FLAG_IPV6  2097152` |
|      - | 4758 | `#define FV_NULL_ON_FAILURE 134217728` |
|      - | 4759 |  |
|      - | 4760 | `/* Trim leading/trailing PHP whitespace, adjusting the (*pz,*pn) view in place.` |
|      - | 4761 | ` * SyisSpace (isspace) matches PHP's filter whitespace set " \t\n\r\v\f". */` |
|    125 | 4762 | `static void FvTrim(const char **pz,int *pn){` |
|    125 | 4763 | `	const char *z = *pz;` |
|    125 | 4764 | `	int n = *pn;` |
|    129 | 4765 | `	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }` |
|    133 | 4766 | `	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }` |
|    125 | 4767 | `	*pz = z; *pn = n;` |
|    125 | 4768 |  |
|      - | 4769 | `/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     57 | 4770 | `static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){` |
|     57 | 4771 | `	int neg = 0, i;` |
|     57 | 4772 | `	sxu64 u = 0;` |
|     57 | 4773 | `	FvTrim(&z,&n);` |
|     57 | 4774 | `	if( n==0 ){ return 0; }` |
|     51 | 4775 | `	if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }` |
|     51 | 4776 | `	if( n==0 ){ return 0; }` |
|     49 | 4777 | `	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'\|\|z[1]=='X') ){` |
|      3 | 4778 | `		z += 2; n -= 2;` |
|      3 | 4779 | `		if( n==0 ){ return 0; }` |
|      7 | 4780 | `		for( i=0; i<n; i++ ){` |
|      5 | 4781 | `			int h = SyHexToint((unsigned char)z[i]);` |
|      5 | 4782 | `			if( h<0 ){ return 0; }` |
|      5 | 4783 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }` |
|      5 | 4784 | `			u = u*16 + (sxu64)h;` |
|      3 | 4785 | `		}` |
|     48 | 4786 | `	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){` |
|      9 | 4787 | `		for( i=0; i<n; i++ ){` |
|      7 | 4788 | `			if( z[i]<'0' \|\| z[i]>'7' ){ return 0; }` |
|      7 | 4789 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }` |
|      7 | 4790 | `			u = u*8 + (sxu64)(z[i]-'0');` |
|      4 | 4791 | `		}` |
|      2 | 4792 | `	}else{` |
|     45 | 4793 | `		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */` |
|    201 | 4794 | `		for( i=0; i<n; i++ ){` |
|    173 | 4795 | `			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|    161 | 4796 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }` |
|    161 | 4797 | `			u = u*10 + (sxu64)(z[i]-'0');` |
|     81 | 4798 | `		}` |
|      - | 4799 | `	}` |
|     33 | 4800 | `	if( neg ){` |
|      5 | 4801 | `		if( u > 0x8000000000000000ULL ){ return 0; }` |
|      5 | 4802 | `		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */` |
|      3 | 4803 | `	}else{` |
|     29 | 4804 | `		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }` |
|     27 | 4805 | `		*pOut = (ph7_int64)u;` |
|      - | 4806 | `	}` |
|     31 | 4807 | `	return 1;` |
|     29 | 4808 |  |
|      - | 4809 | `/* FILTER_VALIDATE_FLOAT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     41 | 4810 | `static int FvValidateFloat(const char *z,int n,int flags,double *pOut){` |
|      - | 4811 | `	char zBuf[512];` |
|     41 | 4812 | `	int i, m = 0, seenDigit = 0;` |
|     41 | 4813 | `	const char *zv; int nv; double d = 0; const char *zRest = 0;` |
|     41 | 4814 | `	FvTrim(&z,&n);` |
|      - | 4815 | `	/* Bound the input: zBuf[512] holds the thousand-separator-stripped copy, and` |
|      - | 4816 | `	 * the cap also rejects the pathological 500+ digit floats PHP refuses. */` |
|     41 | 4817 | `	if( n==0 \|\| n>500 ){ return 0; }` |
|     41 | 4818 | `	if( flags & FV_FLAG_ALLOW_THOUSAND ){` |
|      - | 4819 | `		/* Commas are optional, but when present they must group the integer part` |
|      - | 4820 | `		 * into a leading run of 1..3 digits followed by groups of exactly 3` |
|      - | 4821 | `		 * ("1,000" ok, "1,5"/"1234,567" rejected). Strip them into zBuf and reject` |
|      - | 4822 | `		 * a comma anywhere in the fractional/exponent tail. */` |
|     23 | 4823 | `		int s = 0, intEnd, segStart, segIdx, hasComma = 0;` |
|     23 | 4824 | `		if( s<n && (z[s]=='+'\|\|z[s]=='-') ){ zBuf[m++] = z[s]; s++; }` |
|     23 | 4825 | `		intEnd = s;` |
|    155 | 4826 | `		while( intEnd<n && z[intEnd]!='.' && z[intEnd]!='e' && z[intEnd]!='E' ){` |
|    133 | 4827 | `			if( z[intEnd]==',' ){ hasComma = 1; }` |
|    133 | 4828 | `			intEnd++;` |
|      1 | 4829 | `		}` |
|     23 | 4830 | `		if( hasComma ){` |
|     23 | 4831 | `			segStart = s; segIdx = 0;` |
|    151 | 4832 | `			for( i=s; i<=intEnd; i++ ){` |
|    139 | 4833 | `				if( i==intEnd \|\| z[i]==',' ){` |
|     45 | 4834 | `					int segLen = i - segStart, k;` |
|     45 | 4835 | `					if( segIdx==0 ){ if( segLen<1 \|\| segLen>3 ){ return 0; } }` |
|     23 | 4836 | `					else if( segLen!=3 ){ return 0; }` |
|    107 | 4837 | `					for( k=segStart; k<i; k++ ){` |
|     73 | 4838 | `						if( !SyisDigit((unsigned char)z[k]) ){ return 0; }` |
|     73 | 4839 | `						zBuf[m++] = z[k];` |
|     37 | 4840 | `					}` |
|     35 | 4841 | `					segStart = i+1; segIdx++;` |
|     17 | 4842 | `				}` |
|     65 | 4843 | `			}` |
|      7 | 4844 | `		}else{` |
|    ! 0 | 4845 | `			for( i=s; i<intEnd; i++ ){ zBuf[m++] = z[i]; }` |
|      - | 4846 | `		}` |
|     17 | 4847 | `		for( i=intEnd; i<n; i++ ){` |
|      5 | 4848 | `			if( z[i]==',' ){ return 0; }` |
|      5 | 4849 | `			zBuf[m++] = z[i];` |
|      3 | 4850 | `		}` |
|     13 | 4851 | `		zv = zBuf; nv = m;` |
|      7 | 4852 | `	}else{` |
|     19 | 4853 | `		zv = z; nv = n;` |
|      - | 4854 | `	}` |
|     31 | 4855 | `	i = 0;` |
|     31 | 4856 | `	if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|    105 | 4857 | `	while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     31 | 4858 | `	if( i<nv && zv[i]=='.' ){` |
|     13 | 4859 | `		i++;` |
|     23 | 4860 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|      6 | 4861 | `	}` |
|     31 | 4862 | `	if( !seenDigit ){ return 0; }` |
|     29 | 4863 | `	if( i<nv && (zv[i]=='e'\|\|zv[i]=='E') ){` |
|      5 | 4864 | `		i++;` |
|      5 | 4865 | `		if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|      5 | 4866 | `		if( i>=nv \|\| !SyisDigit((unsigned char)zv[i]) ){ return 0; }` |
|      9 | 4867 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; }` |
|      2 | 4868 | `	}` |
|     29 | 4869 | `	if( i!=nv ){ return 0; } /* trailing junk */` |
|      - | 4870 | `	/* Divergence: PHP rejects magnitudes beyond the double range ("1e400" ->` |
|      - | 4871 | `	 * false), but SyStrToReal (the engine-wide float parser, also behind` |
|      - | 4872 | `	 * floatval/(float)) saturates them to a finite value, so they validate here. */` |
|     25 | 4873 | `	SyStrToReal(zv,(sxu32)nv,(void *)&d,&zRest);` |
|     25 | 4874 | `	*pOut = d;` |
|     25 | 4875 | `	return 1;` |
|     21 | 4876 |  |
|      - | 4877 | `/* FILTER_VALIDATE_BOOLEAN. Returns 1 if the string is recognized (sets *pBool),` |
|      - | 4878 | ` * 0 if it is unrecognized (the failure path). "0"/"false"/"" are recognized as` |
|      - | 4879 | ` * false, NOT failures. */` |
|     33 | 4880 | `static int FvValidateBool(const char *z,int n,int *pBool){` |
|     33 | 4881 | `	FvTrim(&z,&n);` |
|     32 | 4882 | `	if( (n==1 && z[0]=='1') \|\| (n==4 && SyStrnicmp(z,"true",4)==0)` |
|     25 | 4883 | `	    \|\| (n==2 && SyStrnicmp(z,"on",2)==0) \|\| (n==3 && SyStrnicmp(z,"yes",3)==0) ){` |
|     11 | 4884 | `		*pBool = 1; return 1;` |
|      - | 4885 | `	}` |
|     22 | 4886 | `	if( n==0 \|\| (n==1 && z[0]=='0') \|\| (n==5 && SyStrnicmp(z,"false",5)==0)` |
|     11 | 4887 | `	    \|\| (n==3 && SyStrnicmp(z,"off",3)==0) \|\| (n==2 && SyStrnicmp(z,"no",2)==0) ){` |
|     11 | 4888 | `		*pBool = 0; return 1;` |
|      - | 4889 | `	}` |
|      9 | 4890 | `	return 0;` |
|     15 | 4891 |  |
|      - | 4892 | `/* IPv4 dotted-quad: exactly 4 octets 0..255, no leading zeros. */` |
|     33 | 4893 | `static int FvValidateIp4(const char *z,int n){` |
|     33 | 4894 | `	int i = 0, parts = 0;` |
|     77 | 4895 | `	while( i<n ){` |
|     65 | 4896 | `		int val = 0, digits = 0, start = i;` |
|    143 | 4897 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){` |
|     85 | 4898 | `			val = val*10 + (z[i]-'0');` |
|     85 | 4899 | `			if( val>255 ){ return 0; }` |
|     79 | 4900 | `			digits++; i++;` |
|      1 | 4901 | `		}` |
|     59 | 4902 | `		if( digits==0 \|\| digits>3 ){ return 0; }` |
|     49 | 4903 | `		if( digits>1 && z[start]=='0' ){ return 0; } /* leading zero */` |
|     45 | 4904 | `		parts++;` |
|     45 | 4905 | `		if( parts>4 ){ return 0; }` |
|     45 | 4906 | `		if( i<n ){` |
|     33 | 4907 | `			if( z[i]!='.' ){ return 0; }` |
|     33 | 4908 | `			i++;` |
|     33 | 4909 | `			if( i>=n ){ return 0; } /* trailing dot */` |
|     16 | 4910 | `		}` |
|      1 | 4911 | `	}` |
|     13 | 4912 | `	return parts==4;` |
|     17 | 4913 |  |
|      - | 4914 | `/* A colon-separated run of IPv6 hextets with no "::" (n may be 0 -> 0 groups),` |
|      - | 4915 | ` * allowing a trailing embedded IPv4. Returns the 16-bit group count or -1. */` |
|     19 | 4916 | `static int FvIp6Hextets(const char *z,int n){` |
|     19 | 4917 | `	int i = 0, segStart = 0, groups = 0;` |
|     19 | 4918 | `	if( n==0 ){ return 0; }` |
|    145 | 4919 | `	while( i<=n ){` |
|    133 | 4920 | `		if( i==n \|\| z[i]==':' ){` |
|     23 | 4921 | `			int segLen = i - segStart, j, isV4 = 0;` |
|     23 | 4922 | `			if( segLen==0 ){ return -1; } /* an empty hextet (stray ':') */` |
|     77 | 4923 | `			for( j=segStart; j<i; j++ ){ if( z[j]=='.' ){ isV4 = 1; break; } }` |
|     23 | 4924 | `			if( isV4 ){` |
|     11 | 4925 | `				if( i!=n ){ return -1; } /* IPv4 only as the final token */` |
|     11 | 4926 | `				if( !FvValidateIp4(z+segStart,segLen) ){ return -1; }` |
|      5 | 4927 | `				groups += 2;` |
|      3 | 4928 | `			}else{` |
|     13 | 4929 | `				if( segLen>4 ){ return -1; }` |
|     47 | 4930 | `				for( j=segStart; j<i; j++ ){ if( SyHexToint((unsigned char)z[j])<0 ){ return -1; } }` |
|     13 | 4931 | `				groups++;` |
|      - | 4932 | `			}` |
|     17 | 4933 | `			segStart = i+1;` |
|      8 | 4934 | `		}` |
|    127 | 4935 | `		i++;` |
|      1 | 4936 | `	}` |
|     13 | 4937 | `	return groups;` |
|     10 | 4938 |  |
|      - | 4939 | `/* IPv6: at most one "::" zero-run; 8 groups exactly, or fewer when "::" present. */` |
|     19 | 4940 | `static int FvValidateIp6(const char *z,int n){` |
|     19 | 4941 | `	const char *zDbl = 0;` |
|      - | 4942 | `	int i, ga, gb;` |
|    139 | 4943 | `	for( i=0; i+1<n; i++ ){` |
|    123 | 4944 | `		if( z[i]==':' && z[i+1]==':' ){` |
|     13 | 4945 | `			if( zDbl ){ return 0; } /* a second "::" is invalid */` |
|     11 | 4946 | `			zDbl = z+i;` |
|      5 | 4947 | `		}` |
|     61 | 4948 | `	}` |
|     17 | 4949 | `	if( zDbl==0 ){` |
|      9 | 4950 | `		return FvIp6Hextets(z,n)==8;` |
|    ! 0 | 4951 | `	}else{` |
|      9 | 4952 | `		int lenA = (int)(zDbl - z);` |
|      9 | 4953 | `		int lenB = n - lenA - 2;` |
|      9 | 4954 | `		ga = (lenA==0) ? 0 : FvIp6Hextets(z,lenA);` |
|      9 | 4955 | `		gb = (lenB==0) ? 0 : FvIp6Hextets(zDbl+2,lenB);` |
|      9 | 4956 | `		if( ga<0 \|\| gb<0 ){ return 0; }` |
|      9 | 4957 | `		return (ga+gb)<=7; /* "::" stands for at least one zero group */` |
|      - | 4958 | `	}` |
|     10 | 4959 |  |
|     25 | 4960 | `static int FvValidateIp(const char *z,int n,int flags){` |
|     25 | 4961 | `	int v4 = (flags & FV_FLAG_IPV4), v6 = (flags & FV_FLAG_IPV6);` |
|     25 | 4962 | `	if( !v4 && !v6 ){ v4 = v6 = 1; } /* default accepts either family */` |
|     25 | 4963 | `	if( v4 && FvValidateIp4(z,n) ){ return 1; }` |
|     21 | 4964 | `	if( v6 && FvValidateIp6(z,n) ){ return 1; }` |
|     13 | 4965 | `	return 0;` |
|     13 | 4966 |  |
|      - | 4967 | `/* FILTER_VALIDATE_MAC: 17-char colon- or dash-separated hex (XX:XX:..:XX). */` |
|     11 | 4968 | `static int FvValidateMac(const char *z,int n){` |
|      - | 4969 | `	char sep;` |
|      - | 4970 | `	int i;` |
|     11 | 4971 | `	if( n!=17 ){ return 0; }` |
|      7 | 4972 | `	sep = z[2];` |
|      7 | 4973 | `	if( sep!=':' && sep!='-' ){ return 0; }` |
|    105 | 4974 | `	for( i=0; i<17; i++ ){` |
|    101 | 4975 | `		if( (i%3)==2 ){ if( z[i]!=sep ){ return 0; } }` |
|     71 | 4976 | `		else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }` |
|     50 | 4977 | `	}` |
|      5 | 4978 | `	return 1;` |
|      6 | 4979 |  |
|      - | 4980 | `/* FILTER_VALIDATE_EMAIL (best-effort: covers the common cases, not quoted local` |
|      - | 4981 | ` * parts or IP-literal domains). */` |
|     21 | 4982 | `static int FvValidateEmail(const char *z,int n){` |
|     21 | 4983 | `	int at = -1, i, localLen, domLen, labelStart, dotCount = 0;` |
|      - | 4984 | `	const char *zDom;` |
|     21 | 4985 | `	if( n==0 \|\| n>320 ){ return 0; }` |
|    201 | 4986 | `	for( i=0; i<n; i++ ){` |
|    181 | 4987 | `		if( z[i]=='@' ){ if( at>=0 ){ return 0; } at = i; }` |
|     91 | 4988 | `	}` |
|     21 | 4989 | `	if( at<=0 \|\| at==n-1 ){ return 0; } /* one '@', non-empty local and domain */` |
|     21 | 4990 | `	localLen = at;` |
|     21 | 4991 | `	zDom = z + at + 1;` |
|     21 | 4992 | `	domLen = n - at - 1;` |
|     21 | 4993 | `	if( z[0]=='.' \|\| z[at-1]=='.' ){ return 0; }` |
|     57 | 4994 | `	for( i=0; i<localLen; i++ ){` |
|     43 | 4995 | `		unsigned char c = (unsigned char)z[i];` |
|     43 | 4996 | `		if( c<=' ' ){ return 0; }` |
|     41 | 4997 | `		if( c=='.' && i+1<localLen && z[i+1]=='.' ){ return 0; }` |
|     20 | 4998 | `	}` |
|     15 | 4999 | `	if( zDom[0]=='.' \|\| zDom[domLen-1]=='.' ){ return 0; }` |
|     13 | 5000 | `	labelStart = 0;` |
|     85 | 5001 | `	for( i=0; i<=domLen; i++ ){` |
|     75 | 5002 | `		if( i==domLen \|\| zDom[i]=='.' ){` |
|     25 | 5003 | `			int ll = i - labelStart;` |
|     25 | 5004 | `			if( ll==0 ){ return 0; } /* consecutive dots */` |
|     23 | 5005 | `			if( zDom[labelStart]=='-' \|\| zDom[i-1]=='-' ){ return 0; }` |
|     23 | 5006 | `			if( i<domLen ){ dotCount++; }` |
|     23 | 5007 | `			labelStart = i+1;` |
|     12 | 5008 | `		}else{` |
|     51 | 5009 | `			unsigned char c = (unsigned char)zDom[i];` |
|     51 | 5010 | `			if( !((c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9')\|\|c=='-') ){ return 0; }` |
|      - | 5011 | `		}` |
|     37 | 5012 | `	}` |
|     11 | 5013 | `	if( dotCount<1 ){ return 0; } /* PHP requires a dot in the domain (any TLD length) */` |
|      9 | 5014 | `	return 1;` |
|     11 | 5015 |  |
|      - | 5016 | `/* FILTER_VALIDATE_DOMAIN (lenient, matching PHP without FILTER_FLAG_HOSTNAME). */` |
|     11 | 5017 | `static int FvValidateDomain(const char *z,int n){` |
|      - | 5018 | `	int i;` |
|     11 | 5019 | `	if( n<1 \|\| n>253 \|\| z[0]=='.' ){ return 0; }` |
|     81 | 5020 | `	for( i=0; i<n; i++ ){` |
|     75 | 5021 | `		unsigned char c = (unsigned char)z[i];` |
|     75 | 5022 | `		if( c<=' ' ){ return 0; }` |
|     75 | 5023 | `		if( c=='.' && i+1<n && z[i+1]=='.' ){ return 0; }` |
|     37 | 5024 | `	}` |
|      7 | 5025 | `	return 1;` |
|      6 | 5026 |  |
|      - | 5027 | `/* FILTER_VALIDATE_URL: require a scheme and a host (PHP's filter is itself` |
|      - | 5028 | ` * parse_url-based, so PH7_VmHttpSplitURI tracks it closely). */` |
|     15 | 5029 | `static int FvValidateUrl(const char *z,int n){` |
|      - | 5030 | `	SyhttpUri sUri;` |
|     15 | 5031 | `	if( n==0 ){ return 0; }` |
|     15 | 5032 | `	SyZero(&sUri,(sxu32)sizeof(sUri));` |
|     15 | 5033 | `	if( PH7_VmHttpSplitURI(&sUri,z,(sxu32)n)!=SXRET_OK ){ return 0; }` |
|     15 | 5034 | `	return sUri.sScheme.nByte!=0 && sUri.sHost.nByte!=0;` |
|      8 | 5035 |  |
|      - | 5036 | `/* The Fv sanitizers build their result by appending directly to the call` |
|      - | 5037 | ` * context (ph7_result_string accumulates, like htmlspecialchars), emitting each` |
|      - | 5038 | ` * kept run in one call and seeding "" so an all-stripped input yields "". */` |
|      - | 5039 | `/* SANITIZE_NUMBER_INT (isFloat=0) / SANITIZE_NUMBER_FLOAT (isFloat=1). */` |
|     37 | 5040 | `static void FvSanitizeNumber(ph7_context *pCtx,const char *z,int n,int isFloat,int flags){` |
|     37 | 5041 | `	int i, runStart = 0;` |
|     37 | 5042 | `	ph7_result_string(pCtx,"",0);` |
|     97 | 5043 | `	for( i=0; i<n; i++ ){` |
|     91 | 5044 | `		char c = z[i];` |
|     91 | 5045 | `		int keep = (c>='0'&&c<='9') \|\| c=='+' \|\| c=='-';` |
|     91 | 5046 | `		if( !keep && isFloat ){` |
|     38 | 5047 | `			keep = (c=='.' && (flags & FV_FLAG_ALLOW_FRACTION))` |
|     23 | 5048 | `			    \|\| (c==',' && (flags & FV_FLAG_ALLOW_THOUSAND))` |
|     36 | 5049 | `			    \|\| ((c=='e'\|\|c=='E') && (flags & FV_FLAG_ALLOW_SCIENTIFIC));` |
|     12 | 5050 | `		}` |
|     61 | 5051 | `		if( !keep ){` |
|     33 | 5052 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     33 | 5053 | `			runStart = i+1;` |
|     16 | 5054 | `		}` |
|     31 | 5055 | `	}` |
|      7 | 5056 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 | 5057 |  |
|      - | 5058 | `/* SANITIZE_SPECIAL_CHARS (full=0, numeric entities; also encodes control bytes` |
|      - | 5059 | ` * <32 as &#N;) / FULL_SPECIAL_CHARS (full=1, named entities for <>&"').` |
|      - | 5060 | ` * Divergence on bytes >=128: PHP's FULL filter is UTF-8-aware — it named-entity` |
|      - | 5061 | ` * encodes valid sequences ("\xC3\xA9" -> "&eacute;") and drops invalid ones; we` |
|      - | 5062 | ` * pass every byte >=128 through verbatim (the engine has no UTF-8 entity table,` |
|      - | 5063 | ` * and PH7_builtin_htmlspecialchars behaves the same way). Bytes 0-127 match. */` |
|      7 | 5064 | `static void FvSanitizeSpecial(ph7_context *pCtx,const char *z,int n,int full){` |
|      7 | 5065 | `	int i, runStart = 0;` |
|      - | 5066 | `	const char *zEnt;` |
|      7 | 5067 | `	ph7_result_string(pCtx,"",0);` |
|     43 | 5068 | `	for( i=0; i<n; i++ ){` |
|     37 | 5069 | `		unsigned char c = (unsigned char)z[i];` |
|     37 | 5070 | `		switch( c ){` |
|      5 | 5071 | `		case '<':  zEnt = full?"&lt;":"&#60;";   break;` |
|      5 | 5072 | `		case '>':  zEnt = full?"&gt;":"&#62;";   break;` |
|      5 | 5073 | `		case '&':  zEnt = full?"&amp;":"&#38;";  break;` |
|      5 | 5074 | `		case '"':  zEnt = full?"&quot;":"&#34;"; break;` |
|      5 | 5075 | `		case '\'': zEnt = full?"&#039;":"&#39;"; break;` |
|      8 | 5076 | `		default:` |
|     17 | 5077 | `			if( full \|\| c>=32 ){ continue; } /* keep in the current run */` |
|      - | 5078 | `			/* SPECIAL_CHARS encodes a control byte as a numeric entity. */` |
|      5 | 5079 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      5 | 5080 | `			ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|      5 | 5081 | `			runStart = i+1;` |
|      5 | 5082 | `			continue;` |
|      - | 5083 | `		}` |
|     21 | 5084 | `		if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     21 | 5085 | `		ph7_result_string(pCtx,zEnt,-1); /* -1: length from strlen */` |
|     21 | 5086 | `		runStart = i+1;` |
|     11 | 5087 | `	}` |
|      7 | 5088 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 | 5089 |  |
|     25 | 5090 | `static int FvEmailAllowed(unsigned char c){` |
|     25 | 5091 | `	if( (c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9') ){ return 1; }` |
|     16 | 5092 | `	return c=='!'\|\|c=='#'\|\|c=='$'\|\|c=='%'\|\|c=='&'\|\|c=='\''\|\|c=='*'\|\|c=='+'` |
|     10 | 5093 | ``	    \|\| c=='-'\|\|c=='='\|\|c=='?'\|\|c=='^'\|\|c=='_'\|\|c=='`'\|\|c=='{'\|\|c=='\|'`` |
|     15 | 5094 | `	    \|\| c=='}'\|\|c=='~'\|\|c=='@'\|\|c=='.'\|\|c=='['\|\|c==']';` |
|     13 | 5095 |  |
|     23 | 5096 | `static int FvUrlAllowed(unsigned char c){` |
|     23 | 5097 | `	return c>=33 && c<=126; /* PHP keeps every printable ASCII byte except space */` |
|      1 | 5098 |  |
|      - | 5099 | `/* SANITIZE_EMAIL (isUrl=0) / SANITIZE_URL (isUrl=1): strip disallowed bytes. */` |
|      5 | 5100 | `static void FvSanitizeChars(ph7_context *pCtx,const char *z,int n,int isUrl){` |
|      5 | 5101 | `	int i, runStart = 0;` |
|      5 | 5102 | `	ph7_result_string(pCtx,"",0);` |
|     51 | 5103 | `	for( i=0; i<n; i++ ){` |
|     47 | 5104 | `		unsigned char c = (unsigned char)z[i];` |
|     47 | 5105 | `		if( !(isUrl ? FvUrlAllowed(c) : FvEmailAllowed(c)) ){` |
|     11 | 5106 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     11 | 5107 | `			runStart = i+1;` |
|      5 | 5108 | `		}` |
|     24 | 5109 | `	}` |
|      5 | 5110 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      5 | 5111 |  |
|      - | 5112 | `/*` |
|      - | 5113 | ` * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 5114 | ` *  Validate or sanitize a value. The scalar input is coerced to a string and the` |
|      - | 5115 | ` *  selected filter applied; on validation failure the 'default' option (if any)` |
|      - | 5116 | ` *  is returned, else null when FILTER_NULL_ON_FAILURE is set, else false.` |
|      - | 5117 | ` */` |
|    230 | 5118 | `static int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5119 |  |
|    232 | 5120 | `	int iFilter = FV_DEFAULT, iFlags = 0, bNull;` |
|    232 | 5121 | `	ph7_value *pOpts = 0, *pDefault = 0;` |
|      - | 5122 | `	const char *zVal; int nVal;` |
|    232 | 5123 | `	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    232 | 5124 | `	if( nArg>1 ){ iFilter = ph7_value_to_int(apArg[1]); }` |
|    232 | 5125 | `	if( nArg>2 ){` |
|     53 | 5126 | `		if( ph7_value_is_array(apArg[2]) ){` |
|     13 | 5127 | `			ph7_value *pF = ph7_array_fetch(apArg[2],"flags",(int)sizeof("flags")-1);` |
|     13 | 5128 | `			if( pF ){ iFlags = ph7_value_to_int(pF); }` |
|     13 | 5129 | `			pOpts = ph7_array_fetch(apArg[2],"options",(int)sizeof("options")-1);` |
|     13 | 5130 | `			if( pOpts && !ph7_value_is_array(pOpts) ){ pOpts = 0; }` |
|     13 | 5131 | `			if( pOpts ){ pDefault = ph7_array_fetch(pOpts,"default",(int)sizeof("default")-1); }` |
|      7 | 5132 | `		}else{` |
|     41 | 5133 | `			iFlags = ph7_value_to_int(apArg[2]);` |
|      - | 5134 | `		}` |
|     26 | 5135 | `	}` |
|    232 | 5136 | `	bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|      - | 5137 | `	/* An array/object input fails every scalar filter. */` |
|    232 | 5138 | `	if( ph7_value_is_array(apArg[0]) ){ goto fail; }` |
|    230 | 5139 | `	zVal = ph7_value_to_string(apArg[0],&nVal);` |
|    230 | 5140 | `	switch( iFilter ){` |
|     28 | 5141 | `	case FV_VALIDATE_INT: {` |
|      - | 5142 | `		ph7_int64 v;` |
|     58 | 5143 | `		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }` |
|     31 | 5144 | `		if( pOpts ){` |
|      7 | 5145 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|      7 | 5146 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|      7 | 5147 | `			if( pMin && v<ph7_value_to_int64(pMin) ){ goto fail; }` |
|      7 | 5148 | `			if( pMax && v>ph7_value_to_int64(pMax) ){ goto fail; }` |
|      2 | 5149 | `		}` |
|     29 | 5150 | `		ph7_result_int64(pCtx,v);` |
|     29 | 5151 | `		return PH7_OK;` |
|      - | 5152 | `	}` |
|     20 | 5153 | `	case FV_VALIDATE_FLOAT: {` |
|      - | 5154 | `		double d;` |
|     41 | 5155 | `		if( !FvValidateFloat(zVal,nVal,iFlags,&d) ){ goto fail; }` |
|     25 | 5156 | `		ph7_result_double(pCtx,d);` |
|     25 | 5157 | `		return PH7_OK;` |
|      - | 5158 | `	}` |
|     14 | 5159 | `	case FV_VALIDATE_BOOLEAN: {` |
|      - | 5160 | `		int b;` |
|     29 | 5161 | `		if( !FvValidateBool(zVal,nVal,&b) ){ goto fail; }` |
|     21 | 5162 | `		ph7_result_bool(pCtx,b);` |
|     21 | 5163 | `		return PH7_OK;` |
|      - | 5164 | `	}` |
|     25 | 5165 | `	case FV_VALIDATE_IP:     if( !FvValidateIp(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|     11 | 5166 | `	case FV_VALIDATE_MAC:    if( !FvValidateMac(zVal,nVal) ){ goto fail; }       goto pass;` |
|     21 | 5167 | `	case FV_VALIDATE_EMAIL:  if( !FvValidateEmail(zVal,nVal) ){ goto fail; }     goto pass;` |
|     11 | 5168 | `	case FV_VALIDATE_DOMAIN: if( !FvValidateDomain(zVal,nVal) ){ goto fail; }    goto pass;` |
|     15 | 5169 | `	case FV_VALIDATE_URL:    if( !FvValidateUrl(zVal,nVal) ){ goto fail; }       goto pass;` |
|      3 | 5170 | `	case FV_VALIDATE_REGEXP: {` |
|      - | 5171 | `#ifdef PH7_ENABLE_PCRE` |
|      8 | 5172 | `		ph7_value *pRe = pOpts ? ph7_array_fetch(pOpts,"regexp",(int)sizeof("regexp")-1) : 0;` |
|      8 | 5173 | `		const char *zRe; int nRe, matched = 0;` |
|      8 | 5174 | `		if( pRe==0 ){` |
|      3 | 5175 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5176 | `				"filter_var(): \"regexp\" option is missing");` |
|      - | 5177 | `		}` |
|      5 | 5178 | `		zRe = ph7_value_to_string(pRe,&nRe);` |
|      5 | 5179 | `		if( PH7_PcreMatchQuiet(pCtx,zRe,nRe,zVal,nVal,&matched)!=SXRET_OK \|\| !matched ){ goto fail; }` |
|      3 | 5180 | `		goto pass;` |
|      - | 5181 | `#else` |
|      - | 5182 | `		goto fail;` |
|      - | 5183 | `#endif` |
|      - | 5184 | `	}` |
|      3 | 5185 | `	case FV_SANITIZE_NUMBER_INT:   FvSanitizeNumber(pCtx,zVal,nVal,0,0);      return PH7_OK;` |
|      5 | 5186 | `	case FV_SANITIZE_NUMBER_FLOAT: FvSanitizeNumber(pCtx,zVal,nVal,1,iFlags); return PH7_OK;` |
|      5 | 5187 | `	case FV_SANITIZE_SPECIAL_CHARS:      FvSanitizeSpecial(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 5188 | `	case FV_SANITIZE_FULL_SPECIAL_CHARS: FvSanitizeSpecial(pCtx,zVal,nVal,1); return PH7_OK;` |
|      3 | 5189 | `	case FV_SANITIZE_EMAIL: FvSanitizeChars(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 5190 | `	case FV_SANITIZE_URL:   FvSanitizeChars(pCtx,zVal,nVal,1); return PH7_OK;` |
|      5 | 5191 | `	case FV_DEFAULT: goto pass; /* FILTER_UNSAFE_RAW: pass through unchanged */` |
|    ! 0 | 5192 | `	default:` |
|    ! 0 | 5193 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5194 | `			"Unknown filter with ID %d",iFilter);` |
|    ! 0 | 5195 | `		break; /* unknown filter id -> fail */` |
|    ! 0 | 5196 | `	}` |
|     48 | 5197 | `fail:` |
|     97 | 5198 | `	if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|     95 | 5199 | `	else if( bNull ){ ph7_result_null(pCtx); }` |
|     91 | 5200 | `	else { ph7_result_bool(pCtx,0); }` |
|     97 | 5201 | `	return PH7_OK;` |
|     22 | 5202 | `pass: /* validation passed: return the (string) input unchanged */` |
|     45 | 5203 | `	ph7_result_string(pCtx,zVal,nVal);` |
|     45 | 5204 | `	return PH7_OK;` |
|    117 | 5205 |  |
|      - | 5206 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 5207 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 5208 | `/*` |
|      - | 5209 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 5210 |  |
|      - | 5211 | ` */` |
|      4 | 5212 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 5213 | `	const char *zInput, /* Raw input */` |
|      - | 5214 | `	int nByte,  /* Input length */` |
|      - | 5215 | `	int delim,  /* Delimiter */` |
|      - | 5216 | `	int encl,   /* Enclosure */` |
|      - | 5217 | `	int escape,  /* Escape character */` |
|      - | 5218 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 5219 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 5220 | `	)` |
|      1 | 5221 |  |
|      5 | 5222 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 5223 | `	const char *zIn = zInput;` |
|      - | 5224 | `	const char *zPtr;` |
|      - | 5225 | `	int isEnc;` |
|      - | 5226 | `	/* Start processing */` |
|      8 | 5227 | `	for(;;){` |
|     17 | 5228 | `		if( zIn >= zEnd ){` |
|      - | 5229 | `			/* No more input to process */` |
|      5 | 5230 | `			break;` |
|      - | 5231 | `		}` |
|     13 | 5232 | `		isEnc = 0;` |
|     13 | 5233 | `		zPtr = zIn;` |
|      - | 5234 | `		/* Find the first delimiter */` |
|     27 | 5235 | `		while( zIn < zEnd ){` |
|     23 | 5236 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 5237 | `				/* Delimiter found,break imediately */` |
|      5 | 5238 | `				break;` |
|     15 | 5239 | `			}else if( zIn[0] == encl ){` |
|      - | 5240 | `				/* Inside enclosure? */` |
|    ! 0 | 5241 | `				isEnc = !isEnc;` |
|     15 | 5242 | `			}else if( zIn[0] == escape ){` |
|      - | 5243 | `				/* Escape sequence */` |
|    ! 0 | 5244 | `				zIn++;` |
|    ! 0 | 5245 | `			}` |
|      - | 5246 | `			/* Advance the cursor */` |
|     15 | 5247 | `			zIn++;` |
|      1 | 5248 | `		}` |
|     13 | 5249 | `		if( zIn > zPtr ){` |
|     13 | 5250 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 5251 | `			sxi32 rc;` |
|      - | 5252 | `			/* Invoke the supllied callback */` |
|     13 | 5253 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 5254 | `				zPtr++;` |
|    ! 0 | 5255 | `				nByteChunk-=2;` |
|    ! 0 | 5256 | `			}` |
|     13 | 5257 | `			if( nByteChunk > 0 ){` |
|     13 | 5258 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 5259 | `				if( rc == SXERR_ABORT ){` |
|      - | 5260 | `					/* User callback request an operation abort */` |
|    ! 0 | 5261 | `					break;` |
|      - | 5262 | `				}` |
|      6 | 5263 | `			}` |
|      6 | 5264 | `		}` |
|      - | 5265 | `		/* Ignore trailing delimiter */` |
|     21 | 5266 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 5267 | `			zIn++;` |
|      1 | 5268 | `		}` |
|      1 | 5269 | `	}` |
|      5 | 5270 | `	return SXRET_OK;` |
|      1 | 5271 |  |
|      - | 5272 | `/*` |
|      - | 5273 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 5274 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 5275 | ` * argument to this callback.` |
|      - | 5276 | ` */` |
|     12 | 5277 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 5278 |  |
|     13 | 5279 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 5280 | `	ph7_value sEntry;` |
|      - | 5281 | `	SyString sToken;` |
|      - | 5282 | `	/* Insert the token in the given array */` |
|     13 | 5283 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 5284 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 5285 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 5286 | `	if( sToken.nByte < 1){` |
|    ! 0 | 5287 | `		return SXRET_OK;` |
|      - | 5288 | `	}` |
|     13 | 5289 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 5290 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 5291 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 5292 | `	return SXRET_OK;` |
|      7 | 5293 |  |
|      - | 5294 | `/*` |
|      - | 5295 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 5296 | ` *  Parse a CSV string into an array.` |
|      - | 5297 | ` * Parameters` |
|      - | 5298 | ` *  $input` |
|      - | 5299 | ` *   The string to parse.` |
|      - | 5300 | ` *  $delimiter` |
|      - | 5301 | ` *   Set the field delimiter (one character only).` |
|      - | 5302 | ` *  $enclosure` |
|      - | 5303 | ` *   Set the field enclosure character (one character only).` |
|      - | 5304 | ` *  $escape` |
|      - | 5305 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 5306 | ` * Return` |
|      - | 5307 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 5308 | ` */` |
|      4 | 5309 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5310 |  |
|      - | 5311 | `	const char *zInput,*zPtr;` |
|      - | 5312 | `	ph7_value *pArray;` |
|      5 | 5313 | `	int delim  = ',';   /* Delimiter */` |
|      5 | 5314 | `	int encl   = '"' ;  /* Enclosure */` |
|      5 | 5315 | `	int escape = '\\';  /* Escape character */` |
|      - | 5316 | `	int nLen;` |
|      5 | 5317 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5318 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 5319 | `		ph7_result_null(pCtx);` |
|      3 | 5320 | `		return PH7_OK;` |
|      - | 5321 | `	}` |
|      - | 5322 | `	/* Extract the raw input */` |
|      3 | 5323 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5324 | `	if( nArg > 1 ){` |
|      - | 5325 | `		int i;` |
|      3 | 5326 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 5327 | `			/* Extract the delimiter */` |
|      3 | 5328 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 5329 | `			if( i > 0 ){` |
|      3 | 5330 | `				delim = zPtr[0];` |
|      1 | 5331 | `			}` |
|      1 | 5332 | `		}` |
|      3 | 5333 | `		if( nArg > 2 ){` |
|      3 | 5334 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 5335 | `				/* Extract the enclosure */` |
|      3 | 5336 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 5337 | `				if( i > 0 ){` |
|      3 | 5338 | `					encl = zPtr[0];` |
|      1 | 5339 | `				}` |
|      1 | 5340 | `			}` |
|      3 | 5341 | `			if( nArg > 3 ){` |
|      3 | 5342 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 5343 | `					/* Extract the escape character */` |
|      3 | 5344 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 5345 | `					if( i > 0 ){` |
|      3 | 5346 | `						escape = zPtr[0];` |
|      1 | 5347 | `					}` |
|      1 | 5348 | `				}` |
|      1 | 5349 | `			}` |
|      1 | 5350 | `		}` |
|      1 | 5351 | `	}` |
|      - | 5352 | `	/* Create our array */` |
|      3 | 5353 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5354 | `	if( pArray == 0 ){` |
|      - | 5355 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|    ! 0 | 5356 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5357 | `	}` |
|      - | 5358 | `	/* Parse the raw input */` |
|      3 | 5359 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 5360 | `	/* Return the freshly created array */` |
|      3 | 5361 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5362 | `	return PH7_OK;` |
|      3 | 5363 |  |
|      - | 5364 | `/*` |
|      - | 5365 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 5366 | ` * container.` |
|      - | 5367 | ` * Refer to [strip_tags()].` |
|      - | 5368 | ` */` |
|     10 | 5369 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5370 |  |
|     11 | 5371 | `	const char *zEnd = &zTag[nByte];` |
|      - | 5372 | `	const char *zPtr;` |
|      - | 5373 | `	SyString sEntry;` |
|      - | 5374 | `	/* Strip tags */` |
|     10 | 5375 | `	for(;;){` |
|     45 | 5376 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 5377 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 5378 | `				zTag++;` |
|      1 | 5379 | `		}` |
|     21 | 5380 | `		if( zTag >= zEnd ){` |
|     11 | 5381 | `			break;` |
|      - | 5382 | `		}` |
|     11 | 5383 | `		zPtr = zTag;` |
|      - | 5384 | `		/* Delimit the tag */` |
|     25 | 5385 | `		while(zTag < zEnd ){` |
|     25 | 5386 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5387 | `				/* UTF-8 stream */` |
|      3 | 5388 | `				zTag++;` |
|      5 | 5389 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 5390 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 5391 | `				break;` |
|    ! 0 | 5392 | `			}else{` |
|     13 | 5393 | `				zTag++;` |
|      - | 5394 | `			}` |
|      1 | 5395 | `		}` |
|     11 | 5396 | `		if( zTag > zPtr ){` |
|      - | 5397 | `			/* Perform the insertion */` |
|     11 | 5398 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 5399 | `			SyStringFullTrim(&sEntry);` |
|     11 | 5400 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 5401 | `		}` |
|      - | 5402 | `		/* Jump the trailing '>' */` |
|     11 | 5403 | `		zTag++;` |
|      1 | 5404 | `	}` |
|     11 | 5405 | `	return SXRET_OK;` |
|      1 | 5406 |  |
|      - | 5407 | `/*` |
|      - | 5408 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 5409 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 5410 | ` * Refer to [strip_tags()].` |
|      - | 5411 | ` */` |
|     36 | 5412 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5413 |  |
|     37 | 5414 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 5415 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 5416 | `		SyString sTag;` |
|     85 | 5417 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 5418 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 5419 | `			zTag++;` |
|      1 | 5420 | `		}` |
|      - | 5421 | `		/* Delimit the tag */` |
|     25 | 5422 | `		zCur = zTag;` |
|     77 | 5423 | `		while(zTag < zEnd ){` |
|     77 | 5424 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5425 | `				/* UTF-8 stream */` |
|      5 | 5426 | `				zTag++;` |
|      9 | 5427 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 5428 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 5429 | `				break;` |
|    ! 0 | 5430 | `			}else{` |
|     49 | 5431 | `				zTag++;` |
|      - | 5432 | `			}` |
|      1 | 5433 | `		}` |
|     25 | 5434 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 5435 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 5436 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 5437 | `		if( sTag.nByte > 0 ){` |
|      - | 5438 | `			SyString *aEntry,*pEntry;` |
|      - | 5439 | `			sxi32 rc;` |
|      - | 5440 | `			sxu32 n;` |
|      - | 5441 | `			/* Perform the lookup */` |
|     25 | 5442 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 5443 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 5444 | `				pEntry = &aEntry[n];` |
|      - | 5445 | `				/* Do the comparison */` |
|     25 | 5446 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 5447 | `				if( !rc ){` |
|     21 | 5448 | `					return SXRET_OK;` |
|      - | 5449 | `				}` |
|      3 | 5450 | `			}` |
|      2 | 5451 | `		}` |
|      2 | 5452 | `	}` |
|      - | 5453 | `	/* No such tag */` |
|     17 | 5454 | `	return SXERR_NOTFOUND;` |
|     19 | 5455 |  |
|      - | 5456 | `/*` |
|      - | 5457 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 5458 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 5459 | ` * Refer to [strip_tags()].` |
|      - | 5460 | ` */` |
|     16 | 5461 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 5462 |  |
|     17 | 5463 | `	const char *zEnd = &zIn[nByte];` |
|      - | 5464 | `	const char *zPtr,*zTag;` |
|      - | 5465 | `	SySet sSet;` |
|      - | 5466 | `	/* initialize the set of allowed tags */` |
|     17 | 5467 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 5468 | `	if( nTaglen > 0 ){` |
|      - | 5469 | `		/* Set of allowed tags */` |
|     11 | 5470 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 5471 | `	}` |
|      - | 5472 | `	/* Set the empty string */` |
|     17 | 5473 | `	ph7_result_string(pCtx,"",0);` |
|      - | 5474 | `	/* Start processing */` |
|     26 | 5475 | `	for(;;){` |
|     53 | 5476 | `		if(zIn >= zEnd){` |
|      - | 5477 | `			/* No more input to process */` |
|     15 | 5478 | `			break;` |
|      - | 5479 | `		}` |
|     39 | 5480 | `		zPtr = zIn;` |
|      - | 5481 | `		/* Find a tag */` |
|    133 | 5482 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 5483 | `			zIn++;` |
|      1 | 5484 | `		}` |
|     39 | 5485 | `		if( zIn > zPtr ){` |
|      - | 5486 | `			/* Consume raw input */` |
|     21 | 5487 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 5488 | `		}` |
|      - | 5489 | `		/* Ignore trailing null bytes */` |
|     39 | 5490 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 5491 | `			zIn++;` |
|    ! 0 | 5492 | `		}` |
|     39 | 5493 | `		if(zIn >= zEnd){` |
|      - | 5494 | `			/* No more input to process */` |
|      3 | 5495 | `			break;` |
|      - | 5496 | `		}` |
|     37 | 5497 | `		if( zIn[0] == '<' ){` |
|      - | 5498 | `			sxi32 rc;` |
|     37 | 5499 | `			zTag = zIn++;` |
|      - | 5500 | `			/* Delimit the tag */` |
|    127 | 5501 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 5502 | `				zIn++;` |
|      1 | 5503 | `			}` |
|     37 | 5504 | `			if( zIn < zEnd ){` |
|     37 | 5505 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 5506 | `			}` |
|      - | 5507 | `			/* Query the set */` |
|     37 | 5508 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 5509 | `			if( rc == SXRET_OK ){` |
|      - | 5510 | `				/* Keep the tag */` |
|     21 | 5511 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 5512 | `			}` |
|     18 | 5513 | `		}` |
|      1 | 5514 | `	}` |
|      - | 5515 | `	/* Cleanup */` |
|     17 | 5516 | `	SySetRelease(&sSet);` |
|     17 | 5517 | `	return SXRET_OK;` |
|      1 | 5518 |  |
|      - | 5519 | `/*` |
|      - | 5520 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 5521 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 5522 | ` * Parameters` |
|      - | 5523 | ` *  $str` |
|      - | 5524 | ` *  The input string.` |
|      - | 5525 | ` * $allowable_tags` |
|      - | 5526 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 5527 | ` * Return` |
|      - | 5528 | ` *  Returns the stripped string.` |
|      - | 5529 | ` */` |
|     16 | 5530 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5531 |  |
|     17 | 5532 | `	const char *zTaglist = 0;` |
|      - | 5533 | `	const char *zString;` |
|     17 | 5534 | `	int nTaglen = 0;` |
|      - | 5535 | `	int nLen;` |
|     17 | 5536 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5537 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 5538 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5539 | `		return PH7_OK;` |
|      - | 5540 | `	}` |
|      - | 5541 | `	/* Point to the raw string */` |
|     15 | 5542 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 5543 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 5544 | `		/* Allowed tag */` |
|     11 | 5545 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 5546 | `	}` |
|      - | 5547 | `	/* Process input */` |
|     15 | 5548 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 5549 | `	return PH7_OK;` |
|      9 | 5550 |  |
|      - | 5551 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5552 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5553 | `/*` |
|      - | 5554 | ` * string str_shuffle(string $str)` |
|      - | 5555 |  |
|      - | 5556 | ` *  Randomly shuffles a string.` |
|      - | 5557 | ` * Parameters` |
|      - | 5558 | ` *  $str` |
|      - | 5559 | ` *   The input string.` |
|      - | 5560 | ` * Return` |
|      - | 5561 | ` *  Returns the shuffled string.` |
|      - | 5562 | ` */` |
|     12 | 5563 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5564 |  |
|      - | 5565 | `	const char *zString;` |
|      - | 5566 | `	int nLen,i,c;` |
|      - | 5567 | `	sxu32 iR;` |
|     13 | 5568 | `	if( nArg < 1 ){` |
|      - | 5569 | `		/* Missing arguments,return the empty string */` |
|      3 | 5570 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5571 | `		return PH7_OK;` |
|      - | 5572 | `	}` |
|      - | 5573 | `	/* Extract the target string */` |
|     11 | 5574 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5575 | `	if( nLen < 1 ){` |
|      - | 5576 | `		/* Nothing to shuffle */` |
|      3 | 5577 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5578 | `		return PH7_OK;` |
|      - | 5579 | `	}` |
|      - | 5580 | `	/* Shuffle the string */` |
|     43 | 5581 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 5582 | `		/* Generate a random number first */` |
|     35 | 5583 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 5584 | `		/* Extract a random offset */` |
|     35 | 5585 | `		c = zString[iR % nLen];` |
|      - | 5586 | `		/* Append it */` |
|     35 | 5587 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 5588 | `	}` |
|      9 | 5589 | `	return PH7_OK;` |
|      7 | 5590 |  |
|      - | 5591 | `/*` |
|      - | 5592 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 5593 | ` *  Convert a string to an array.` |
|      - | 5594 | ` * Parameters` |
|      - | 5595 | ` * $string` |
|      - | 5596 | ` *  The input string.` |
|      - | 5597 | ` * $split_length` |
|      - | 5598 | ` *  Maximum length of the chunk.` |
|      - | 5599 | ` * Return` |
|      - | 5600 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 5601 | ` *  except possibly the last one which may be shorter.` |
|      - | 5602 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 5603 | ` *  as the first (and only) array element.` |
|      - | 5604 | ` *  An empty string returns an empty array.` |
|      - | 5605 | ` * Errors` |
|      - | 5606 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 5607 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 5608 | ` *  ValueError if $split_length is less than 1.` |
|      - | 5609 | ` */` |
|     28 | 5610 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 5611 |  |
|      - | 5612 | `	const char *zString,*zEnd;` |
|      - | 5613 | `	ph7_value *pArray,*pValue;` |
|      - | 5614 | `	int split_len;` |
|      - | 5615 | `	int nLen;` |
|     33 | 5616 | `	if( nArg < 1 ){` |
|      4 | 5617 | `		return PH7_VmThrowException(pCtx,` |
|      - | 5618 | `			"ArgumentCountError",` |
|      - | 5619 | `			"str_split() expects at least 1 argument, %d given",` |
|      1 | 5620 | `			nArg` |
|      - | 5621 | `			);` |
|      - | 5622 | `	}` |
|      - | 5623 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     38 | 5624 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     40 | 5625 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 5626 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 5627 | `		return PH7_VmThrowException(pCtx,` |
|      - | 5628 | `			"TypeError",` |
|      - | 5629 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 5630 | `			ph7_type_name(apArg[0])` |
|      - | 5631 | `			);` |
|      - | 5632 | `	}` |
|      - | 5633 | `	/* Point to the target string */` |
|     27 | 5634 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 5635 | `	split_len = (int)sizeof(char);` |
|     27 | 5636 | `	if( nArg > 1 ){` |
|      - | 5637 | `		/* Split length */` |
|     17 | 5638 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 5639 | `		if( split_len < 1 ){` |
|      6 | 5640 | `			return PH7_VmThrowException(pCtx,` |
|      - | 5641 | `				"ValueError",` |
|      - | 5642 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 5643 | `				);` |
|      - | 5644 | `		}` |
|     11 | 5645 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 5646 | `			split_len = nLen;` |
|      1 | 5647 | `		}` |
|      5 | 5648 | `	}` |
|      - | 5649 | `	/* Create the array and the scalar value */` |
|     21 | 5650 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 5651 | `	/*Chunk value */` |
|     21 | 5652 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 5653 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 5654 | `		/* Return FALSE */` |
|    ! 0 | 5655 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5656 | `		return PH7_OK;` |
|      - | 5657 | `	}` |
|      - | 5658 | `	/* Point to the end of the string */` |
|     21 | 5659 | `	zEnd = &zString[nLen];` |
|      - | 5660 | `	/* Perform the requested operation */` |
|     48 | 5661 | `	for(;;){` |
|      - | 5662 | `		int nMax;` |
|     59 | 5663 | `		if( zString >= zEnd ){` |
|      - | 5664 | `			/* No more input to process */` |
|     21 | 5665 | `			break;` |
|      - | 5666 | `		}` |
|     39 | 5667 | `		nMax = (int)(zEnd-zString);` |
|     39 | 5668 | `		if( nMax < split_len ){` |
|      3 | 5669 | `			split_len = nMax;` |
|      1 | 5670 | `		}` |
|      - | 5671 | `		/* Copy the current chunk */` |
|     39 | 5672 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 5673 | `		/* Insert it */` |
|     39 | 5674 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|    ! 0 | 5675 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 5676 | `		}` |
|      - | 5677 | `		/* reset the string cursor */` |
|     39 | 5678 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 5679 | `		/* Update position */` |
|     39 | 5680 | `		zString += split_len;` |
|      1 | 5681 | `	}` |
|      - | 5682 | `	/*` |
|      - | 5683 | `	 * Return the array.` |
|      - | 5684 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 5685 | `	 * upon we return from this function.` |
|      - | 5686 | `	 */` |
|     21 | 5687 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 5688 | `	return PH7_OK;` |
|     19 | 5689 |  |
|      - | 5690 | `/*` |
|      - | 5691 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 5692 | ` * Refer to [strspn()].` |
|      - | 5693 | ` */` |
|     28 | 5694 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 5695 |  |
|     29 | 5696 | `	const char *zIn = *pzIn;` |
|      - | 5697 | `	const char *zPtr;` |
|      - | 5698 | `	/* Ignore leading white spaces */` |
|     29 | 5699 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 5700 | `		zIn++;` |
|    ! 0 | 5701 | `	}` |
|     29 | 5702 | `	if( zIn >= zEnd ){` |
|      - | 5703 | `		/* End of input */` |
|    ! 0 | 5704 | `		return SXERR_EOF;` |
|      - | 5705 | `	}` |
|     29 | 5706 | `	zPtr = zIn;` |
|      - | 5707 | `	/* Extract the token */` |
|    201 | 5708 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 5709 | `		zIn++;` |
|      1 | 5710 | `	}` |
|     29 | 5711 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5712 | `	/* Synchronize pointers */` |
|     29 | 5713 | `	*pzIn = zIn;` |
|      - | 5714 | `	/* Return to the caller */` |
|     29 | 5715 | `	return SXRET_OK;` |
|     15 | 5716 |  |
|      - | 5717 | `/*` |
|      - | 5718 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 5719 | ` * return the longest match.` |
|      - | 5720 | ` * Refer to [strspn()].` |
|      - | 5721 | ` */` |
|     18 | 5722 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5723 |  |
|     19 | 5724 | `	const char *zEnd = &zString[nLen];` |
|     19 | 5725 | `	const char *zIn = zString;` |
|      - | 5726 | `	int i,c;` |
|     45 | 5727 | `	for(;;){` |
|     91 | 5728 | `		if( zString >= zEnd ){` |
|      7 | 5729 | `			break;` |
|      - | 5730 | `		}` |
|      - | 5731 | `		/* Extract current character */` |
|     85 | 5732 | `		c = zString[0];` |
|      - | 5733 | `		/* Perform the lookup */` |
|    383 | 5734 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 5735 | `			if( c == zMask[i] ){` |
|      - | 5736 | `				/* Character found */` |
|     73 | 5737 | `				break;` |
|      - | 5738 | `			}` |
|    150 | 5739 | `		}` |
|     85 | 5740 | `		if( i >= nMaskLen ){` |
|      - | 5741 | `			/* Character not in the current mask,break immediately */` |
|     13 | 5742 | `			break;` |
|      - | 5743 | `		}` |
|      - | 5744 | `		/* Advance cursor */` |
|     73 | 5745 | `		zString++;` |
|      1 | 5746 | `	}` |
|      - | 5747 | `	/* Longest match */` |
|     19 | 5748 | `	return (int)(zString-zIn);` |
|      1 | 5749 |  |
|      - | 5750 | `/*` |
|      - | 5751 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 5752 | ` * Refer to [strcspn()].` |
|      - | 5753 | ` */` |
|     10 | 5754 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5755 |  |
|     11 | 5756 | `	const char *zEnd = &zString[nLen];` |
|     11 | 5757 | `	const char *zIn = zString;` |
|      - | 5758 | `	int i,c;` |
|     12 | 5759 | `	for(;;){` |
|     25 | 5760 | `		if( zString >= zEnd ){` |
|      3 | 5761 | `			break;` |
|      - | 5762 | `		}` |
|      - | 5763 | `		/* Extract current character */` |
|     23 | 5764 | `		c = zString[0];` |
|      - | 5765 | `		/* Perform the lookup */` |
|     51 | 5766 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 5767 | `			if( c == zMask[i] ){` |
|      9 | 5768 | `				break;` |
|      - | 5769 | `			}` |
|     15 | 5770 | `		}` |
|     23 | 5771 | `		if( i < nMaskLen ){` |
|      - | 5772 | `			/* Character in the current mask,break immediately */` |
|      9 | 5773 | `			break;` |
|      - | 5774 | `		}` |
|      - | 5775 | `		/* Advance cursor */` |
|     15 | 5776 | `		zString++;` |
|      1 | 5777 | `	}` |
|      - | 5778 | `	/* Longest match */` |
|     11 | 5779 | `	return (int)(zString-zIn);` |
|      1 | 5780 |  |
|      - | 5781 | `/*` |
|      - | 5782 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5783 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 5784 | ` *  of characters contained within a given mask.` |
|      - | 5785 | ` * Parameters` |
|      - | 5786 | ` * $str` |
|      - | 5787 | ` *  The input string.` |
|      - | 5788 | ` * $mask` |
|      - | 5789 | ` *  The list of allowable characters.` |
|      - | 5790 | ` * $start` |
|      - | 5791 | ` *  The position in subject to start searching.` |
|      - | 5792 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5793 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5794 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5795 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5796 | ` *  start'th position from the end of subject.` |
|      - | 5797 | ` * $length` |
|      - | 5798 | ` *  The length of the segment from subject to examine.` |
|      - | 5799 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5800 | ` *  characters after the starting position.` |
|      - | 5801 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5802 | ` *  position up to length characters from the end of subject.` |
|      - | 5803 | ` * Return` |
|      - | 5804 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 5805 | ` * in mask.` |
|      - | 5806 | ` */` |
|     26 | 5807 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5808 |  |
|      - | 5809 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5810 | `	int iMasklen,iLen;` |
|      - | 5811 | `	SyString sToken;` |
|     27 | 5812 | `	int iCount = 0;` |
|      - | 5813 | `	int rc;` |
|     27 | 5814 | `	if( nArg < 2 ){` |
|      - | 5815 | `		/* Missing agruments,return zero */` |
|      3 | 5816 | `		ph7_result_int(pCtx,0);` |
|      3 | 5817 | `		return PH7_OK;` |
|      - | 5818 | `	}` |
|      - | 5819 | `	/* Extract the target string */` |
|     25 | 5820 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5821 | `	/* Extract the mask */` |
|     25 | 5822 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 5823 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 5824 | `		/* Nothing to process,return zero */` |
|      7 | 5825 | `		ph7_result_int(pCtx,0);` |
|      7 | 5826 | `		return PH7_OK;` |
|      - | 5827 | `	}` |
|     19 | 5828 | `	if( nArg > 2 ){` |
|      - | 5829 | `		int nOfft;` |
|      - | 5830 | `		/* Extract the offset */` |
|      9 | 5831 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 5832 | `		if( nOfft < 0 ){` |
|    ! 0 | 5833 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5834 | `			if( zBase > zString ){` |
|    ! 0 | 5835 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5836 | `				zString = zBase;` |
|    ! 0 | 5837 | `			}else{` |
|      - | 5838 | `				/* Invalid offset */` |
|    ! 0 | 5839 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5840 | `				return PH7_OK;` |
|      - | 5841 | `			}` |
|    ! 0 | 5842 | `		}else{` |
|      9 | 5843 | `			if( nOfft >= iLen ){` |
|      - | 5844 | `				/* Invalid offset */` |
|    ! 0 | 5845 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5846 | `				return PH7_OK;` |
|    ! 0 | 5847 | `			}else{` |
|      - | 5848 | `				/* Update offset */` |
|      9 | 5849 | `				zString += nOfft;` |
|      9 | 5850 | `				iLen -= nOfft;` |
|      - | 5851 | `			}` |
|      - | 5852 | `		}` |
|      9 | 5853 | `		if( nArg > 3 ){` |
|      - | 5854 | `			int iUserlen;` |
|      - | 5855 | `			/* Extract the desired length */` |
|      9 | 5856 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 5857 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 5858 | `				iLen = iUserlen;` |
|      2 | 5859 | `			}` |
|      4 | 5860 | `		}` |
|      4 | 5861 | `	}` |
|      - | 5862 | `	/* Point to the end of the string */` |
|     19 | 5863 | `	zEnd = &zString[iLen];` |
|      - | 5864 | `	/* Extract the first non-space token */` |
|     19 | 5865 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 5866 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5867 | `		/* Compare against the current mask */` |
|     19 | 5868 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 5869 | `	}` |
|      - | 5870 | `	/* Longest match */` |
|     19 | 5871 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 5872 | `	return PH7_OK;` |
|     14 | 5873 |  |
|      - | 5874 | `/*` |
|      - | 5875 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5876 | ` *  Find length of initial segment not matching mask.` |
|      - | 5877 | ` * Parameters` |
|      - | 5878 | ` * $str` |
|      - | 5879 | ` *  The input string.` |
|      - | 5880 | ` * $mask` |
|      - | 5881 | ` *  The list of not allowed characters.` |
|      - | 5882 | ` * $start` |
|      - | 5883 | ` *  The position in subject to start searching.` |
|      - | 5884 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5885 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5886 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5887 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5888 | ` *  start'th position from the end of subject.` |
|      - | 5889 | ` * $length` |
|      - | 5890 | ` *  The length of the segment from subject to examine.` |
|      - | 5891 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5892 | ` *  characters after the starting position.` |
|      - | 5893 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5894 | ` *  position up to length characters from the end of subject.` |
|      - | 5895 | ` * Return` |
|      - | 5896 | ` *  Returns the length of the segment as an integer.` |
|      - | 5897 | ` */` |
|     16 | 5898 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5899 |  |
|      - | 5900 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5901 | `	int iMasklen,iLen;` |
|      - | 5902 | `	SyString sToken;` |
|     17 | 5903 | `	int iCount = 0;` |
|      - | 5904 | `	int rc;` |
|     17 | 5905 | `	if( nArg < 2 ){` |
|      - | 5906 | `		/* Missing agruments,return zero */` |
|      3 | 5907 | `		ph7_result_int(pCtx,0);` |
|      3 | 5908 | `		return PH7_OK;` |
|      - | 5909 | `	}` |
|      - | 5910 | `	/* Extract the target string */` |
|     15 | 5911 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5912 | `	/* Extract the mask */` |
|     15 | 5913 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 5914 | `	if( iLen < 1 ){` |
|      - | 5915 | `		/* Nothing to process,return zero */` |
|    ! 0 | 5916 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5917 | `		return PH7_OK;` |
|      - | 5918 | `	}` |
|     15 | 5919 | `	if( iMasklen < 1 ){` |
|      - | 5920 | `		/* No given mask,return the string length */` |
|      3 | 5921 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 5922 | `		return PH7_OK;` |
|      - | 5923 | `	}` |
|     13 | 5924 | `	if( nArg > 2 ){` |
|      - | 5925 | `		int nOfft;` |
|      - | 5926 | `		/* Extract the offset */` |
|     11 | 5927 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 5928 | `		if( nOfft < 0 ){` |
|    ! 0 | 5929 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5930 | `			if( zBase > zString ){` |
|    ! 0 | 5931 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5932 | `				zString = zBase;` |
|    ! 0 | 5933 | `			}else{` |
|      - | 5934 | `				/* Invalid offset */` |
|    ! 0 | 5935 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5936 | `				return PH7_OK;` |
|      - | 5937 | `			}` |
|    ! 0 | 5938 | `		}else{` |
|     11 | 5939 | `			if( nOfft >= iLen ){` |
|      - | 5940 | `				/* Invalid offset */` |
|      3 | 5941 | `				ph7_result_int(pCtx,0);` |
|      3 | 5942 | `				return PH7_OK;` |
|    ! 0 | 5943 | `			}else{` |
|      - | 5944 | `				/* Update offset */` |
|      9 | 5945 | `				zString += nOfft;` |
|      9 | 5946 | `				iLen -= nOfft;` |
|      - | 5947 | `			}` |
|      - | 5948 | `		}` |
|      9 | 5949 | `		if( nArg > 3 ){` |
|      - | 5950 | `			int iUserlen;` |
|      - | 5951 | `			/* Extract the desired length */` |
|    ! 0 | 5952 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 5953 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 5954 | `				iLen = iUserlen;` |
|    ! 0 | 5955 | `			}` |
|    ! 0 | 5956 | `		}` |
|      4 | 5957 | `	}` |
|      - | 5958 | `	/* Point to the end of the string */` |
|     11 | 5959 | `	zEnd = &zString[iLen];` |
|      - | 5960 | `	/* Extract the first non-space token */` |
|     11 | 5961 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 5962 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5963 | `		/* Compare against the current mask */` |
|     11 | 5964 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 5965 | `	}` |
|      - | 5966 | `	/* Longest match */` |
|     11 | 5967 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 5968 | `	return PH7_OK;` |
|      9 | 5969 |  |
|      - | 5970 | `/*` |
|      - | 5971 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 5972 | ` *  Search a string for any of a set of characters.` |
|      - | 5973 | ` * Parameters` |
|      - | 5974 | ` *  $haystack` |
|      - | 5975 | ` *   The string where char_list is looked for.` |
|      - | 5976 | ` *  $char_list` |
|      - | 5977 | ` *   This parameter is case sensitive.` |
|      - | 5978 | ` * Return` |
|      - | 5979 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 5980 | ` */` |
|      6 | 5981 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5982 |  |
|      - | 5983 | `	const char *zString,*zList,*zEnd;` |
|      - | 5984 | `	int iLen,iListLen,i,c;` |
|      - | 5985 | `	sxu32 nOfft,nMax;` |
|      - | 5986 | `	sxi32 rc;` |
|      7 | 5987 | `	if( nArg < 2 ){` |
|      - | 5988 | `		/* Missing arguments,return FALSE */` |
|      3 | 5989 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5990 | `		return PH7_OK;` |
|      - | 5991 | `	}` |
|      - | 5992 | `	/* Extract the haystack and the char list */` |
|      5 | 5993 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 5994 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 5995 | `	if( iLen < 1 ){` |
|      - | 5996 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 5997 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5998 | `		return PH7_OK;` |
|      - | 5999 | `	}` |
|      - | 6000 | `	/* Point to the end of the string */` |
|      5 | 6001 | `	zEnd = &zString[iLen];` |
|      5 | 6002 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 6003 | `	/* perform the requested operation */` |
|     15 | 6004 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 6005 | `		c = zList[i];` |
|     11 | 6006 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 6007 | `		if( rc == SXRET_OK ){` |
|      5 | 6008 | `			if( nMax < nOfft ){` |
|      3 | 6009 | `				nOfft = nMax;` |
|      1 | 6010 | `			}` |
|      2 | 6011 | `		}` |
|      6 | 6012 | `	}` |
|      5 | 6013 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 6014 | `		/* No such substring,return FALSE */` |
|      3 | 6015 | `		ph7_result_bool(pCtx,0);` |
|      2 | 6016 | `	}else{` |
|      - | 6017 | `		/* Return the substring */` |
|      3 | 6018 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 6019 | `	}` |
|      5 | 6020 | `	return PH7_OK;` |
|      4 | 6021 |  |
|      - | 6022 | `/* SPDX-SnippetBegin */` |
|      - | 6023 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 6024 | `/* SPDX-License-Identifier: blessing */` |
|      - | 6025 | `/*` |
|      - | 6026 | ` * string soundex(string $str)` |
|      - | 6027 | ` *  Calculate the soundex key of a string.` |
|      - | 6028 | ` * Parameters` |
|      - | 6029 | ` *  $str` |
|      - | 6030 | ` *   The input string.` |
|      - | 6031 | ` * Return` |
|      - | 6032 | ` *  Returns the soundex key as a string.` |
|      - | 6033 | ` * Note:` |
|      - | 6034 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 6035 | ` * source tree.` |
|      - | 6036 | ` */` |
|     20 | 6037 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6038 |  |
|      - | 6039 | `	const unsigned char *zIn;` |
|      - | 6040 | `	char zResult[8];` |
|      - | 6041 | `	int i, j;` |
|      - | 6042 | `	static const unsigned char iCode[] = {` |
|      - | 6043 |  |
|      - | 6044 |  |
|      - | 6045 |  |
|      - | 6046 |  |
|      - | 6047 |  |
|      - | 6048 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 6049 |  |
|      - | 6050 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 6051 | `	};` |
|     21 | 6052 | `	if( nArg < 1 ){` |
|      - | 6053 | `		/* Missing arguments,return the empty string */` |
|      3 | 6054 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 6055 | `		return PH7_OK;` |
|      - | 6056 | `	}` |
|     19 | 6057 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     19 | 6058 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     19 | 6059 | `	if( zIn[i] ){` |
|     17 | 6060 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 6061 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 6062 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 6063 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 6064 | `			if( code>0 ){` |
|     45 | 6065 | `				if( code!=prevcode ){` |
|     33 | 6066 | `					prevcode = (unsigned char)code;` |
|     33 | 6067 | `					zResult[j++] = (char)code + '0';` |
|     16 | 6068 | `				}` |
|     23 | 6069 | `			}else{` |
|     49 | 6070 | `				prevcode = 0;` |
|      - | 6071 | `			}` |
|     47 | 6072 | `		}` |
|     33 | 6073 | `		while( j<4 ){` |
|     17 | 6074 | `			zResult[j++] = '0';` |
|      1 | 6075 | `		}` |
|     17 | 6076 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 6077 | `	}else{` |
|      3 | 6078 | `	  ph7_result_string(pCtx,"?000",4);` |
|      - | 6079 | `	}` |
|     19 | 6080 | `	return PH7_OK;` |
|     11 | 6081 |  |
|      - | 6082 | `/* SPDX-SnippetEnd */` |
|      - | 6083 | `/*` |
|      - | 6084 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 6085 | ` *  Wraps a string to a given number of characters.` |
|      - | 6086 | ` * Parameters` |
|      - | 6087 | ` *  $str` |
|      - | 6088 | ` *   The input string.` |
|      - | 6089 | ` * $width` |
|      - | 6090 | ` *  The column width.` |
|      - | 6091 | ` * $break` |
|      - | 6092 | ` *  The line is broken using the optional break parameter.` |
|      - | 6093 | ` * Return` |
|      - | 6094 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 6095 | ` */` |
|     14 | 6096 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6097 |  |
|      - | 6098 | `	const char *zIn,*zEnd,*zBreak;` |
|      - | 6099 | `	int iLen,iBreaklen,iChunk;` |
|     15 | 6100 | `	if( nArg < 1 ){` |
|      - | 6101 | `		/* Missing arguments,return the empty string */` |
|      3 | 6102 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 6103 | `		return PH7_OK;` |
|      - | 6104 | `	}` |
|      - | 6105 | `	/* Extract the input string */` |
|     13 | 6106 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|     13 | 6107 | `	if( iLen < 1 ){` |
|      - | 6108 | `		/* Nothing to process,return the empty string */` |
|      3 | 6109 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 6110 | `		return PH7_OK;` |
|      - | 6111 | `	}` |
|      - | 6112 | `	/* Chunk length */` |
|     11 | 6113 | `	iChunk = 75;` |
|     11 | 6114 | `	iBreaklen = 0;` |
|     11 | 6115 | `	zBreak = ""; /* cc warning */` |
|     11 | 6116 | `	if( nArg > 1 ){` |
|     11 | 6117 | `		iChunk = ph7_value_to_int(apArg[1]);` |
|     11 | 6118 | `		if( iChunk < 1 ){` |
|    ! 0 | 6119 | `			iChunk = 75;` |
|    ! 0 | 6120 | `		}` |
|     11 | 6121 | `		if( nArg > 2 ){` |
|      3 | 6122 | `			zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      1 | 6123 | `		}` |
|      5 | 6124 | `	}` |
|     11 | 6125 | `	if( iBreaklen < 1 ){` |
|      - | 6126 | `		/* Set a default column break */` |
|      - | 6127 | `#ifdef __WINNT__` |
|      1 | 6128 | `		zBreak = "\r\n";` |
|      1 | 6129 | `		iBreaklen = (int)sizeof("\r\n")-1;` |
|      - | 6130 | `#else` |
|      8 | 6131 | `		zBreak = "\n";` |
|      8 | 6132 | `		iBreaklen = (int)sizeof(char);` |
|      - | 6133 | `#endif` |
|      4 | 6134 | `	}` |
|      - | 6135 | `	/* Perform the requested operation */` |
|     11 | 6136 | `	zEnd = &zIn[iLen];` |
|     41 | 6137 | `	for(;;){` |
|      - | 6138 | `		int nMax;` |
|     47 | 6139 | `		if( zIn >= zEnd ){` |
|      - | 6140 | `			/* No more input to process */` |
|     11 | 6141 | `			break;` |
|      - | 6142 | `		}` |
|     37 | 6143 | `		nMax = (int)(zEnd-zIn);` |
|     37 | 6144 | `		if( iChunk > nMax ){` |
|     11 | 6145 | `			iChunk = nMax;` |
|      5 | 6146 | `		}` |
|      - | 6147 | `		/* Append the column first */` |
|     37 | 6148 | `		ph7_result_string(pCtx,zIn,iChunk); /* Will make it's own copy */` |
|      - | 6149 | `		/* Advance the cursor */` |
|     37 | 6150 | `		zIn += iChunk;` |
|     37 | 6151 | `		if( zIn < zEnd ){` |
|      - | 6152 | `			/* Append the line break */` |
|     27 | 6153 | `			ph7_result_string(pCtx,zBreak,iBreaklen);` |
|     13 | 6154 | `		}` |
|      1 | 6155 | `	}` |
|     11 | 6156 | `	return PH7_OK;` |
|      8 | 6157 |  |
|      - | 6158 | `/*` |
|      - | 6159 | ` * Check if the given character is a member of the given mask.` |
|      - | 6160 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 6161 | ` * Refer to [strtok()].` |
|      - | 6162 | ` */` |
|     30 | 6163 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 6164 |  |
|      - | 6165 | `	int i;` |
|     57 | 6166 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 6167 | `		if( c == zMask[i] ){` |
|     13 | 6168 | `			if( pOfft ){` |
|      5 | 6169 | `				*pOfft = i;` |
|      2 | 6170 | `			}` |
|     13 | 6171 | `			return TRUE;` |
|      - | 6172 | `		}` |
|     14 | 6173 | `	}` |
|     19 | 6174 | `	return FALSE;` |
|     16 | 6175 |  |
|      - | 6176 | `/*` |
|      - | 6177 | ` * Extract a single token from the input stream.` |
|      - | 6178 | ` * Refer to [strtok()].` |
|      - | 6179 | ` */` |
|      6 | 6180 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 6181 |  |
|      7 | 6182 | `	const char *zIn = *pzIn;` |
|      - | 6183 | `	const char *zPtr;` |
|      - | 6184 | `	/* Ignore leading delimiter */` |
|     11 | 6185 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 6186 | `		zIn++;` |
|      1 | 6187 | `	}` |
|      7 | 6188 | `	if( zIn >= zEnd ){` |
|      - | 6189 | `		/* End of input */` |
|    ! 0 | 6190 | `		return SXERR_EOF;` |
|      - | 6191 | `	}` |
|      7 | 6192 | `	zPtr = zIn;` |
|      - | 6193 | `	/* Extract the token */` |
|     13 | 6194 | `	while( zIn < zEnd ){` |
|     11 | 6195 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 6196 | `			/* UTF-8 stream */` |
|    ! 0 | 6197 | `			zIn++;` |
|    ! 0 | 6198 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 6199 | `		}else{` |
|     11 | 6200 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 6201 | `				break;` |
|      - | 6202 | `			}` |
|      7 | 6203 | `			zIn++;` |
|      - | 6204 | `		}` |
|      1 | 6205 | `	}` |
|      7 | 6206 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 6207 | `	/* Update the cursor */` |
|      7 | 6208 | `	*pzIn = zIn;` |
|      - | 6209 | `	/* Return to the caller */` |
|      7 | 6210 | `	return SXRET_OK;` |
|      4 | 6211 |  |
|      - | 6212 | `/* strtok auxiliary private data */` |
|      - | 6213 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 6214 | `struct strtok_aux_data` |
|      - | 6215 |  |
|      - | 6216 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 6217 | `	const char *zIn;   /* Current input stream */` |
|      - | 6218 | `	const char *zEnd;  /* End of input */` |
|      - | 6219 | `};` |
|      - | 6220 | `/*` |
|      - | 6221 | ` * string strtok(string $str,string $token)` |
|      - | 6222 | ` * string strtok(string $token)` |
|      - | 6223 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 6224 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 6225 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 6226 | ` *  words by using the space character as the token.` |
|      - | 6227 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 6228 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 6229 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 6230 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 6231 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 6232 | ` *  the argument are found.` |
|      - | 6233 | ` * Parameters` |
|      - | 6234 | ` *  $str` |
|      - | 6235 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 6236 | ` * $token` |
|      - | 6237 | ` *  The delimiter used when splitting up str.` |
|      - | 6238 | ` * Return` |
|      - | 6239 | ` *   Current token or FALSE on EOF.` |
|      - | 6240 | ` */` |
|      8 | 6241 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6242 |  |
|      - | 6243 | `	strtok_aux_data *pAux;` |
|      - | 6244 | `	const char *zMask;` |
|      - | 6245 | `	SyString sToken;` |
|      - | 6246 | `	int nMasklen;` |
|      - | 6247 | `	sxi32 rc;` |
|      9 | 6248 | `	if( nArg < 2 ){` |
|      - | 6249 | `		/* Extract top aux data */` |
|      7 | 6250 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      7 | 6251 | `		if( pAux == 0 ){` |
|      - | 6252 | `			/* No aux data,return FALSE */` |
|    ! 0 | 6253 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6254 | `			return PH7_OK;` |
|      - | 6255 | `		}` |
|      7 | 6256 | `		nMasklen = 0;` |
|      7 | 6257 | `		zMask = ""; /* cc warning */` |
|      7 | 6258 | `		if( nArg > 0 ){` |
|      - | 6259 | `			/* Extract the mask */` |
|      5 | 6260 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 6261 | `		}` |
|      7 | 6262 | `		if( nMasklen < 1 ){` |
|      - | 6263 | `			/* Invalid mask,return FALSE */` |
|      3 | 6264 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|      3 | 6265 | `			ph7_context_free_chunk(pCtx,pAux);` |
|      3 | 6266 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|      3 | 6267 | `			ph7_result_bool(pCtx,0);` |
|      3 | 6268 | `			return PH7_OK;` |
|      - | 6269 | `		}` |
|      - | 6270 | `		/* Extract the token */` |
|      5 | 6271 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 6272 | `		if( rc != SXRET_OK ){` |
|      - | 6273 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 6274 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 6275 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6276 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 6277 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6278 | `		}else{` |
|      - | 6279 | `			/* Return the extracted token */` |
|      5 | 6280 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6281 | `		}` |
|      3 | 6282 | `	}else{` |
|      - | 6283 | `		const char *zInput,*zCur;` |
|      - | 6284 | `		char *zDup;` |
|      - | 6285 | `		int nLen;` |
|      - | 6286 | `		/* Extract the raw input */` |
|      3 | 6287 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 6288 | `		if( nLen < 1 ){` |
|      - | 6289 | `			/* Empty input,return FALSE */` |
|    ! 0 | 6290 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6291 | `			return PH7_OK;` |
|      - | 6292 | `		}` |
|      - | 6293 | `		/* Extract the mask */` |
|      3 | 6294 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 6295 | `		if( nMasklen < 1 ){` |
|      - | 6296 | `			/* Set a default mask */` |
|      - | 6297 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 6298 | `			zMask = TOK_MASK;` |
|    ! 0 | 6299 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 6300 | `#undef TOK_MASK` |
|    ! 0 | 6301 | `		}` |
|      - | 6302 | `		/* Extract a single token */` |
|      3 | 6303 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 6304 | `		if( rc != SXRET_OK ){` |
|      - | 6305 | `			/* Empty input */` |
|    ! 0 | 6306 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6307 | `			return PH7_OK;` |
|    ! 0 | 6308 | `		}else{` |
|      - | 6309 | `			/* Return the extracted token */` |
|      3 | 6310 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6311 | `		}` |
|      - | 6312 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 6313 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 6314 | `		if( pAux ){` |
|      3 | 6315 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 6316 | `			if( nLen < 1 ){` |
|    ! 0 | 6317 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6318 | `				return PH7_OK;` |
|      - | 6319 | `			}` |
|      - | 6320 | `			/* Duplicate input */` |
|      3 | 6321 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 6322 | `			if( zDup  ){` |
|      3 | 6323 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 6324 | `				/* Register the aux data */` |
|      3 | 6325 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 6326 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 6327 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 6328 | `			}` |
|      1 | 6329 | `		}` |
|      - | 6330 | `	}` |
|      7 | 6331 | `	return PH7_OK;` |
|      5 | 6332 |  |
|      - | 6333 | `/*` |
|      - | 6334 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 6335 | ` *  Pad a string to a certain length with another string` |
|      - | 6336 | ` * Parameters` |
|      - | 6337 | ` *  $input` |
|      - | 6338 | ` *   The input string.` |
|      - | 6339 | ` * $pad_length` |
|      - | 6340 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 6341 | ` *   string, no padding takes place.` |
|      - | 6342 | ` * $pad_string` |
|      - | 6343 | ` *   Note:` |
|      - | 6344 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 6345 | ` *    divided by the pad_string's length.` |
|      - | 6346 | ` * $pad_type` |
|      - | 6347 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 6348 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 6349 | ` * Return` |
|      - | 6350 | ` *  The padded string.` |
|      - | 6351 | ` */` |
|     10 | 6352 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6353 |  |
|      - | 6354 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 6355 | `	const char *zIn,*zPad;` |
|     11 | 6356 | `	if( nArg < 2 ){` |
|      - | 6357 | `		/* Missing arguments,return the empty string */` |
|      5 | 6358 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 6359 | `		return PH7_OK;` |
|      - | 6360 | `	}` |
|      - | 6361 | `	/* Extract the target string */` |
|      7 | 6362 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6363 | `	/* Padding length */` |
|      7 | 6364 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|      7 | 6365 | `	if( iPadlen > 0 ){` |
|      5 | 6366 | `		iPadlen -= iLen;` |
|      2 | 6367 | `	}` |
|      7 | 6368 | `	if( iPadlen < 1  ){` |
|      - | 6369 | `		/* Return the string verbatim */` |
|      3 | 6370 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      3 | 6371 | `		return PH7_OK;` |
|      - | 6372 | `	}` |
|      5 | 6373 | `	zPad = " "; /* Whitespace padding */` |
|      5 | 6374 | `	iStrpad = (int)sizeof(char);` |
|      5 | 6375 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      5 | 6376 | `	if( nArg > 2 ){` |
|      - | 6377 | `		/* Padding string */` |
|      5 | 6378 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      5 | 6379 | `		if( iStrpad < 1 ){` |
|      - | 6380 | `			/* Empty string */` |
|    ! 0 | 6381 | `			zPad = " "; /* Whitespace padding */` |
|    ! 0 | 6382 | `			iStrpad = (int)sizeof(char);` |
|    ! 0 | 6383 | `		}` |
|      5 | 6384 | `		if( nArg > 3 ){` |
|      - | 6385 | `			/* Padd type */` |
|      5 | 6386 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 6387 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 6388 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 6389 | `			}` |
|      2 | 6390 | `		}` |
|      2 | 6391 | `	}` |
|      5 | 6392 | `	iDiv = 1;` |
|      5 | 6393 | `	if( iType == 2 ){` |
|    ! 0 | 6394 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 6395 | `	}` |
|      - | 6396 | `	/* Perform the requested operation */` |
|      5 | 6397 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 6398 | `		jPad = iStrpad;` |
|      5 | 6399 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 6400 | `			/* Padding */` |
|      5 | 6401 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 6402 | `				break;` |
|      - | 6403 | `			}` |
|      3 | 6404 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 6405 | `		}` |
|      3 | 6406 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 6407 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 6408 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 6409 | `				if( jPad > iStrpad ){` |
|    ! 0 | 6410 | `					jPad = iStrpad;` |
|    ! 0 | 6411 | `				}` |
|      3 | 6412 | `				if( jPad < 1){` |
|    ! 0 | 6413 | `					break;` |
|      - | 6414 | `				}` |
|      3 | 6415 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 6416 | `			}` |
|      1 | 6417 | `		}` |
|      1 | 6418 | `	}` |
|      5 | 6419 | `	if( iLen > 0 ){` |
|      - | 6420 | `		/* Append the input string */` |
|      5 | 6421 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 6422 | `	}` |
|      5 | 6423 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 6424 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 6425 | `			/* Padding */` |
|      5 | 6426 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 6427 | `				break;` |
|      - | 6428 | `			}` |
|      3 | 6429 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 6430 | `		}` |
|      5 | 6431 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 6432 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 6433 | `			if( jPad > iStrpad ){` |
|    ! 0 | 6434 | `				jPad = iStrpad;` |
|    ! 0 | 6435 | `			}` |
|      3 | 6436 | `			if( jPad < 1){` |
|    ! 0 | 6437 | `				break;` |
|      - | 6438 | `			}` |
|      3 | 6439 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 6440 | `		}` |
|      1 | 6441 | `	}` |
|      5 | 6442 | `	return PH7_OK;` |
|      6 | 6443 |  |
|      - | 6444 | `/*` |
|      - | 6445 | ` * String replacement private data.` |
|      - | 6446 | ` */` |
|      - | 6447 | `typedef struct str_replace_data str_replace_data;` |
|      - | 6448 | `struct str_replace_data` |
|      - | 6449 |  |
|      - | 6450 | `	/* The following two fields are only used by the strtr function */` |
|      - | 6451 | `	SyBlob *pWorker;         /* Working buffer */` |
|      - | 6452 | `	ProcStringMatch xMatch;  /* Pattern match routine */` |
|      - | 6453 | `	/* The following two fields are only used by the str_replace function */` |
|      - | 6454 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 6455 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 6456 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 6457 | `};` |
|      - | 6458 | `/*` |
|      - | 6459 | ` * Remove a substring.` |
|      - | 6460 | ` */` |
|      - | 6461 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 6462 | `	for(;;){\` |
|      - | 6463 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 6464 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 6465 | `		++OFFT;\` |
|      - | 6466 | `	}\` |
|      - | 6467 |  |
|      - | 6468 | `/*` |
|      - | 6469 | ` * Shift right and insert algorithm.` |
|      - | 6470 | ` */` |
|      - | 6471 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 6472 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 6473 | `		for(;;){\` |
|      - | 6474 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 6475 | `			if(INLEN < 1 ) { break; }\` |
|      - | 6476 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 6477 | `			--INLEN; \` |
|      - | 6478 | `		}\` |
|      - | 6479 | `		for(;;){\` |
|      - | 6480 | `				if(ELEN < 1) { break; }\` |
|      - | 6481 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 6482 | `				OFFT++;\` |
|      - | 6483 | `				ENTRY++;\` |
|      - | 6484 | `				--ELEN;\` |
|      - | 6485 | `		}\` |
|      - | 6486 |  |
|      - | 6487 | `/*` |
|      - | 6488 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 6489 | ` * replacement string [i.e: zReplace].` |
|      - | 6490 | ` */` |
|     38 | 6491 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 6492 |  |
|     39 | 6493 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 6494 | `	sxu32 n,m;` |
|     39 | 6495 | `	n = SyBlobLength(pWorker);` |
|     39 | 6496 | `	m = nOfft;` |
|      - | 6497 | `	/* Delete the old entry */` |
|    475 | 6498 | `	STRDEL(zInput,n,m,nLen);` |
|     39 | 6499 | `	SyBlobLength(pWorker) -= nLen;` |
|     39 | 6500 | `	if( nReplen > 0 ){` |
|     33 | 6501 | `		sxi32 iRep = nReplen;` |
|      - | 6502 | `		sxi32 rc;` |
|      - | 6503 | `		/*` |
|      - | 6504 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 6505 | `		 * string.` |
|      - | 6506 | `		 */` |
|     33 | 6507 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     33 | 6508 | `		if( rc != SXRET_OK ){` |
|      - | 6509 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 6510 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 6511 | `			return rc;` |
|      - | 6512 | `		}` |
|      - | 6513 | `		/* Perform the insertion now */` |
|     33 | 6514 | `		zInput = (char *)SyBlobData(pWorker);` |
|     33 | 6515 | `		n = SyBlobLength(pWorker);` |
|    163 | 6516 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     33 | 6517 | `		SyBlobLength(pWorker) += nReplen;` |
|     16 | 6518 | `	}` |
|     39 | 6519 | `	return SXRET_OK;` |
|     20 | 6520 |  |
|      - | 6521 | `/*` |
|      - | 6522 | ` * String replacement walker callback.` |
|      - | 6523 | ` * The following callback is invoked for each array entry that hold` |
|      - | 6524 | ` * the replace string.` |
|      - | 6525 | ` * Refer to the strtr() implementation for more information.` |
|      - | 6526 | ` */` |
|      8 | 6527 | `static int StringReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6528 |  |
|      9 | 6529 | `	str_replace_data *pRepData = (str_replace_data *)pUserData;` |
|      - | 6530 | `	const char *zTarget,*zReplace;` |
|      - | 6531 | `	SyBlob *pWorker;` |
|      - | 6532 | `	int tLen,nLen;` |
|      - | 6533 | `	sxu32 nOfft;` |
|      - | 6534 | `	sxi32 rc;` |
|      - | 6535 | `	/* Point to the working buffer */` |
|      9 | 6536 | `	pWorker = pRepData->pWorker;` |
|      9 | 6537 | `	if( !ph7_value_is_string(pKey) ){` |
|      - | 6538 | `		/* Target and replace must be a string */` |
|      3 | 6539 | `		return PH7_OK;` |
|      - | 6540 | `	}` |
|      - | 6541 | `	/* Extract the target and the replace */` |
|      7 | 6542 | `	zTarget = ph7_value_to_string(pKey,&tLen);` |
|      7 | 6543 | `	if( tLen < 1 ){` |
|      - | 6544 | `		/* Empty target,return immediately */` |
|    ! 0 | 6545 | `		return PH7_OK;` |
|      - | 6546 | `	}` |
|      - | 6547 | `	/* Perform a pattern search */` |
|      7 | 6548 | `	rc = pRepData->xMatch(SyBlobData(pWorker),SyBlobLength(pWorker),(const void *)zTarget,(sxu32)tLen,&nOfft);` |
|      7 | 6549 | `	if( rc != SXRET_OK ){` |
|      - | 6550 | `		/* Pattern not found */` |
|    ! 0 | 6551 | `		return PH7_OK;` |
|      - | 6552 | `	}` |
|      - | 6553 | `	/* Extract the replace string */` |
|      7 | 6554 | `	zReplace = ph7_value_to_string(pData,&nLen);` |
|      - | 6555 | `	/* Perform the replace process */` |
|      7 | 6556 | `	rc = StringReplace(pWorker,nOfft,tLen,zReplace,nLen);` |
|      7 | 6557 | `	if( rc != SXRET_OK ){` |
|      - | 6558 | `		/* Allocation failure: carry it out and stop the walk */` |
|    ! 0 | 6559 | `		pRepData->rc = rc;` |
|    ! 0 | 6560 | `		return rc;` |
|      - | 6561 | `	}` |
|      - | 6562 | `	/* All done */` |
|      7 | 6563 | `	return PH7_OK;` |
|      5 | 6564 |  |
|      - | 6565 | `/*` |
|      - | 6566 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 6567 | ` * to collect search/replace string.` |
|      - | 6568 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 6569 | ` */` |
|     26 | 6570 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6571 |  |
|     27 | 6572 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 6573 | `	SyString sWorker;` |
|      - | 6574 | `	const char *zIn;` |
|      - | 6575 | `	int nByte;` |
|      - | 6576 | `	/* Extract a string representation of the given argument */` |
|     27 | 6577 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 6578 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 6579 | `	if( nByte > 0 ){` |
|      - | 6580 | `		char *zDup;` |
|      - | 6581 | `		/* Duplicate the chunk */` |
|     25 | 6582 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 6583 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 6584 | `			);` |
|     25 | 6585 | `		if( zDup == 0 ){` |
|      - | 6586 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 6587 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 6588 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 6589 | `			return SXERR_MEM;` |
|      - | 6590 | `		}` |
|     25 | 6591 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 6592 | `		/* Save the chunk */` |
|     25 | 6593 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 6594 | `	}` |
|      - | 6595 | `	/* Save for later processing */` |
|     27 | 6596 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 6597 | `	/* All done */` |
|     13 | 6598 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 6599 | `	return PH7_OK;` |
|     14 | 6600 |  |
|      - | 6601 | `/*` |
|      - | 6602 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6603 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6604 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 6605 | ` * Parameters` |
|      - | 6606 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 6607 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 6608 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 6609 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 6610 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 6611 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 6612 | ` * $search` |
|      - | 6613 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 6614 | ` *  to designate multiple needles.` |
|      - | 6615 | ` * $replace` |
|      - | 6616 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 6617 | ` *  to designate multiple replacements.` |
|      - | 6618 | ` * $subject` |
|      - | 6619 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 6620 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 6621 | ` *  of subject, and the return value is an array as well.` |
|      - | 6622 | ` * $count (Not used)` |
|      - | 6623 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 6624 | ` * Return` |
|      - | 6625 | ` * This function returns a string or an array with the replaced values.` |
|      - | 6626 | ` */` |
|  23586 | 6627 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 6628 |  |
|      - | 6629 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 6630 | `	ProcStringMatch xMatch;` |
|      - | 6631 | `	const char *zIn,*zFunc;` |
|      - | 6632 | `	str_replace_data sRep;` |
|      - | 6633 | `	SyBlob sWorker;` |
|      - | 6634 | `	SySet sReplace;` |
|      - | 6635 | `	SySet sSearch;` |
|      - | 6636 | `	int rep_str;` |
|      - | 6637 | `	int nByte;` |
|      - | 6638 | `	sxi32 rc;` |
|  23591 | 6639 | `	if( nArg < 3 ){` |
|      - | 6640 | `		/* Missing/Invalid arguments,return null */` |
|      7 | 6641 | `		ph7_result_null(pCtx);` |
|      7 | 6642 | `		return PH7_OK;` |
|      - | 6643 | `	}` |
|      - | 6644 | `	/* Initialize fields */` |
|  23585 | 6645 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  23585 | 6646 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  23585 | 6647 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  23585 | 6648 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  23585 | 6649 | `	sRep.pCtx = pCtx;` |
|  23585 | 6650 | `	sRep.pCollector = &sSearch;` |
|  23585 | 6651 | `	rep_str = 0;` |
|      - | 6652 | `	/* Extract the subject */` |
|  23585 | 6653 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  23585 | 6654 | `	if( nByte < 1 ){` |
|      - | 6655 | `		/* Nothing to replace,return the empty string */` |
|     29 | 6656 | `		ph7_result_string(pCtx,"",0);` |
|     29 | 6657 | `		return PH7_OK;` |
|      - | 6658 | `	}` |
|      - | 6659 | `	/* Copy the subject */` |
|  23557 | 6660 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 6661 | `	/* Search string */` |
|  23557 | 6662 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 6663 | `		/* Collect search string */` |
|      9 | 6664 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 6665 | `	}else{` |
|      - | 6666 | `		/* Single pattern */` |
|  23549 | 6667 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  23549 | 6668 | `		if( nByte < 1 ){` |
|      - | 6669 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 6670 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 6671 | `			return PH7_OK;` |
|      - | 6672 | `		}` |
|  23545 | 6673 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6674 | `		/* Save for later processing */` |
|  23545 | 6675 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 6676 | `	}` |
|      - | 6677 | `	/* Replace string */` |
|  23553 | 6678 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 6679 | `		/* Collect replace string */` |
|      7 | 6680 | `		sRep.pCollector = &sReplace;` |
|      7 | 6681 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 6682 | `	}else{` |
|      - | 6683 | `		/* Single needle */` |
|  23547 | 6684 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  23547 | 6685 | `		rep_str = 1;` |
|  23547 | 6686 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6687 | `		/* Save for later processing */` |
|  23547 | 6688 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 6689 | `	}` |
|      - | 6690 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  23553 | 6691 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 6692 | `		SySetRelease(&sSearch);` |
|    ! 0 | 6693 | `		SySetRelease(&sReplace);` |
|    ! 0 | 6694 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 6695 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6696 | `	}` |
|      - | 6697 | `	/* Reset loop cursors */` |
|  23553 | 6698 | `	SySetResetCursor(&sSearch);` |
|  23553 | 6699 | `	SySetResetCursor(&sReplace);` |
|  23553 | 6700 | `	pReplace = pSearch = 0; /* cc warning */` |
|  23553 | 6701 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 6702 | `	/* Extract function name */` |
|  23553 | 6703 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 6704 | `	/* Set the default pattern match routine */` |
|  23553 | 6705 | `	xMatch = SyBlobSearch;` |
|  23553 | 6706 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 6707 | `		/* Case insensitive pattern match */` |
|     11 | 6708 | `		xMatch = iPatternMatch;` |
|      5 | 6709 | `	}` |
|      - | 6710 | `	/* Start the replace process */` |
|  47109 | 6711 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 6712 | `		sxu32 nCount,nOfft;` |
|  23561 | 6713 | `		if( pSearch->nByte <  1 ){` |
|      - | 6714 | `			/* Empty string,ignore */` |
|      3 | 6715 | `			continue;` |
|      - | 6716 | `		}` |
|      - | 6717 | `		/* Extract the replace string */` |
|  23559 | 6718 | `		if( rep_str ){` |
|  23549 | 6719 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  11777 | 6720 | `		}else{` |
|     11 | 6721 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 6722 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 6723 | `				 * An empty string is used for the rest of replacement values` |
|      - | 6724 | `				 */` |
|      3 | 6725 | `				pReplace = 0;` |
|      1 | 6726 | `			}` |
|      - | 6727 | `		}` |
|  23559 | 6728 | `		if( pReplace == 0 ){` |
|      - | 6729 | `			/* Use an empty string instead */` |
|      3 | 6730 | `			pReplace = &sTemp;` |
|      1 | 6731 | `		}` |
|  23559 | 6732 | `		nOfft = nCount = 0;` |
|  11793 | 6733 | `		for(;;){` |
|  23591 | 6734 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 6735 | `				break;` |
|      - | 6736 | `			}` |
|      - | 6737 | `			/* Perform a pattern lookup */` |
|  35366 | 6738 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  23574 | 6739 | `				pSearch->nByte,&nOfft);` |
|  23579 | 6740 | `			if( rc != SXRET_OK ){` |
|      - | 6741 | `				/* Pattern not found */` |
|  23547 | 6742 | `				break;` |
|      - | 6743 | `			}` |
|      - | 6744 | `			/* Perform the replace operation */` |
|     33 | 6745 | `			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|     33 | 6746 | `			if( rc != SXRET_OK ){` |
|      - | 6747 | `				/* Allocation failure: surface a fatal instead of a partial result */` |
|    ! 0 | 6748 | `				SySetRelease(&sSearch);` |
|    ! 0 | 6749 | `				SySetRelease(&sReplace);` |
|    ! 0 | 6750 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 6751 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 6752 | `			}` |
|      - | 6753 | `			/* Increment offset counter */` |
|     33 | 6754 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 6755 | `		}` |
|      5 | 6756 | `	}` |
|      - | 6757 | `	/* All done,clean-up the mess left behind */` |
|  23553 | 6758 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  23553 | 6759 | `	SySetRelease(&sSearch);` |
|  23553 | 6760 | `	SySetRelease(&sReplace);` |
|  23553 | 6761 | `	SyBlobRelease(&sWorker);` |
|  23553 | 6762 | `	if( rc != PH7_OK ){` |
|    ! 0 | 6763 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6764 | `	}` |
|  23553 | 6765 | `	return PH7_OK;` |
|  11798 | 6766 |  |
|      - | 6767 | `/*` |
|      - | 6768 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 6769 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 6770 | ` *  Translate characters or replace substrings.` |
|      - | 6771 | ` * Parameters` |
|      - | 6772 | ` *  $str` |
|      - | 6773 | ` *  The string being translated.` |
|      - | 6774 | ` * $from` |
|      - | 6775 | ` *  The string being translated to to.` |
|      - | 6776 | ` * $to` |
|      - | 6777 | ` *  The string replacing from.` |
|      - | 6778 | ` * $replace_pairs` |
|      - | 6779 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 6780 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 6781 | ` * Return` |
|      - | 6782 | ` *  The translated string.` |
|      - | 6783 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 6784 | ` */` |
|     12 | 6785 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6786 |  |
|      - | 6787 | `	const char *zIn;` |
|      - | 6788 | `	int nLen;` |
|     13 | 6789 | `	if( nArg < 1 ){` |
|      - | 6790 | `		/* Nothing to replace,return FALSE */` |
|      7 | 6791 | `		ph7_result_bool(pCtx,0);` |
|      7 | 6792 | `		return PH7_OK;` |
|      - | 6793 | `	}` |
|      7 | 6794 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 6795 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 6796 | `		/* Invalid arguments */` |
|    ! 0 | 6797 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6798 | `		return PH7_OK;` |
|      - | 6799 | `	}` |
|      9 | 6800 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 6801 | `		str_replace_data sRepData;` |
|      - | 6802 | `		SyBlob sWorker;` |
|      - | 6803 | `		sxi32 rc;` |
|      - | 6804 | `		/* Initilaize the working buffer */` |
|      5 | 6805 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|      - | 6806 | `		/* Copy raw string */` |
|      5 | 6807 | `		SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nLen);` |
|      - | 6808 | `		/* Init our replace data instance */` |
|      5 | 6809 | `		sRepData.pWorker = &sWorker;` |
|      5 | 6810 | `		sRepData.xMatch = SyBlobSearch;` |
|      5 | 6811 | `		sRepData.rc = SXRET_OK;` |
|      - | 6812 | `		/* Iterate throw array entries and perform the replace operation.*/` |
|      5 | 6813 | `		ph7_array_walk(apArg[1],StringReplaceWalker,&sRepData);` |
|      5 | 6814 | `		if( sRepData.rc != SXRET_OK ){` |
|      - | 6815 | `			/* Allocation failure during replacement: surface a fatal */` |
|    ! 0 | 6816 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 6817 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6818 | `		}` |
|      - | 6819 | `		/* All done, return the result string */` |
|      7 | 6820 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|      4 | 6821 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 6822 | `		/* Clean-up */` |
|      5 | 6823 | `		SyBlobRelease(&sWorker);` |
|      5 | 6824 | `		if( rc != PH7_OK ){` |
|    ! 0 | 6825 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6826 | `		}` |
|      3 | 6827 | `	}else{` |
|      - | 6828 | `		int i,flen,tlen,c,iOfft;` |
|      - | 6829 | `		const char *zFrom,*zTo;` |
|      3 | 6830 | `		if( nArg < 3 ){` |
|      - | 6831 | `			/* Nothing to replace */` |
|    ! 0 | 6832 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6833 | `			return PH7_OK;` |
|      - | 6834 | `		}` |
|      - | 6835 | `		/* Extract given arguments */` |
|      3 | 6836 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 6837 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 6838 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 6839 | `			/* Nothing to replace */` |
|    ! 0 | 6840 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6841 | `			return PH7_OK;` |
|      - | 6842 | `		}` |
|      - | 6843 | `		/* Start the replace process */` |
|     13 | 6844 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 6845 | `			c = zIn[i];` |
|     11 | 6846 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 6847 | `				if ( iOfft < tlen ){` |
|      5 | 6848 | `					c = zTo[iOfft];` |
|      2 | 6849 | `				}` |
|      2 | 6850 | `			}` |
|     11 | 6851 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 6852 |  |
|      6 | 6853 | `		}` |
|      - | 6854 | `	}` |
|      7 | 6855 | `	return PH7_OK;` |
|      7 | 6856 |  |
|      - | 6857 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6858 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6859 | `/*` |
|      - | 6860 | ` * Parse an INI string.` |
|      - | 6861 |  |
|      - | 6862 | ` * According to wikipedia` |
|      - | 6863 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 6864 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 6865 | ` *  Format` |
|      - | 6866 | `*    Properties` |
|      - | 6867 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 6868 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 6869 | `*     Example:` |
|      - | 6870 | `*      name=value` |
|      - | 6871 | `*    Sections` |
|      - | 6872 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 6873 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 6874 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 6875 | `*     or the end of the file. Sections may not be nested.` |
|      - | 6876 | `*     Example:` |
|      - | 6877 | `*      [section]` |
|      - | 6878 | `*   Comments` |
|      - | 6879 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 6880 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 6881 | `*/` |
|     12 | 6882 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 6883 |  |
|      - | 6884 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 6885 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 6886 | `	SyHashEntry *pEntry;` |
|      - | 6887 | `	SyString sEntry;` |
|      - | 6888 | `	SyHash sHash;` |
|      - | 6889 | `	int c;` |
|      - | 6890 | `	/* Create an empty array and worker variables */` |
|     13 | 6891 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 6892 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 6893 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 6894 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 6895 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|    ! 0 | 6896 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6897 | `	}` |
|     13 | 6898 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 6899 | `	pCur = pArray;` |
|      - | 6900 | `	/* Start the parse process */` |
|     21 | 6901 | `	for(;;){` |
|      - | 6902 | `		/* Ignore leading white spaces */` |
|     69 | 6903 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 6904 | `			zIn++;` |
|      1 | 6905 | `		}` |
|     43 | 6906 | `		if( zIn >= zEnd ){` |
|      - | 6907 | `			/* No more input to process */` |
|     13 | 6908 | `			break;` |
|      - | 6909 | `		}` |
|     31 | 6910 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6911 | `			/* Comment til the end of line */` |
|    ! 0 | 6912 | `			zIn++;` |
|    ! 0 | 6913 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 6914 | `				zIn++;` |
|    ! 0 | 6915 | `			}` |
|    ! 0 | 6916 | `			continue;` |
|      - | 6917 | `		}` |
|      - | 6918 | `		/* Reset the string cursor of the working variable */` |
|     31 | 6919 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 6920 | `		if( zIn[0] == '[' ){` |
|      - | 6921 | `			/* Section: Extract the section name */` |
|      9 | 6922 | `			zIn++;` |
|      9 | 6923 | `			zCur = zIn;` |
|     73 | 6924 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 6925 | `				zIn++;` |
|      1 | 6926 | `			}` |
|      9 | 6927 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 6928 | `				/* Save the section name */` |
|      5 | 6929 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 6930 | `				SyStringFullTrim(&sEntry);` |
|      5 | 6931 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 6932 | `				if( sEntry.nByte > 0 ){` |
|      - | 6933 | `					/* Associate an array with the section */` |
|      5 | 6934 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 6935 | `					if( pSection ){` |
|      5 | 6936 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 6937 | `						pCur = pSection;` |
|      2 | 6938 | `					}` |
|      2 | 6939 | `				}` |
|      2 | 6940 | `			}` |
|      9 | 6941 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 6942 | `		}else{` |
|      - | 6943 | `			ph7_value *pOldCur;` |
|      - | 6944 | `			int is_array;` |
|      - | 6945 | `			int iLen;` |
|      - | 6946 | `			/* Properties */` |
|     23 | 6947 | `			is_array = 0;` |
|     23 | 6948 | `			zCur = zIn;` |
|     23 | 6949 | `			iLen = 0; /* cc warning */` |
|     23 | 6950 | `			pOldCur = pCur;` |
|    155 | 6951 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 6952 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 6953 | `					/* Array */` |
|    ! 0 | 6954 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 6955 | `					is_array = 1;` |
|    ! 0 | 6956 | `					if( iLen > 0 ){` |
|    ! 0 | 6957 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 6958 | `						/* Query the hashtable */` |
|    ! 0 | 6959 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 6960 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 6961 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 6962 | `						if( pEntry ){` |
|    ! 0 | 6963 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 6964 | `						}else{` |
|      - | 6965 | `							/* Create an empty array */` |
|    ! 0 | 6966 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 6967 | `							if( pvArr ){` |
|      - | 6968 | `								/* Save the entry */` |
|    ! 0 | 6969 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 6970 | `								/* Insert the entry */` |
|    ! 0 | 6971 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6972 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 6973 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 6974 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6975 | `							}` |
|      - | 6976 | `						}` |
|    ! 0 | 6977 | `						if( pvArr ){` |
|    ! 0 | 6978 | `							pCur = pvArr;` |
|    ! 0 | 6979 | `						}` |
|    ! 0 | 6980 | `					}` |
|    ! 0 | 6981 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 6982 | `						zIn++;` |
|    ! 0 | 6983 | `					}` |
|    ! 0 | 6984 | `				}` |
|    133 | 6985 | `				zIn++;` |
|      1 | 6986 | `			}` |
|     23 | 6987 | `			if( !is_array ){` |
|     23 | 6988 | `				iLen = (int)(zIn-zCur);` |
|     11 | 6989 | `			}` |
|      - | 6990 | `			/* Trim the key */` |
|     23 | 6991 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 6992 | `			SyStringFullTrim(&sEntry);` |
|     23 | 6993 | `			if( sEntry.nByte > 0 ){` |
|     23 | 6994 | `				if( !is_array ){` |
|      - | 6995 | `					/* Save the key name */` |
|     23 | 6996 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 6997 | `				}` |
|      - | 6998 | `				/* extract key value */` |
|     23 | 6999 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 7000 | `				zIn++; /* '=' */` |
|     39 | 7001 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 7002 | `					zIn++;` |
|      1 | 7003 | `				}` |
|     23 | 7004 | `				if( zIn < zEnd ){` |
|     21 | 7005 | `					zCur = zIn;` |
|     21 | 7006 | `					c = zIn[0];` |
|     21 | 7007 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 7008 | `						zIn++;` |
|      - | 7009 | `						/* Delimit the value */` |
|    ! 0 | 7010 | `						while( zIn < zEnd ){` |
|    ! 0 | 7011 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 7012 | `								break;` |
|      - | 7013 | `							}` |
|    ! 0 | 7014 | `							zIn++;` |
|    ! 0 | 7015 | `						}` |
|    ! 0 | 7016 | `						if( zIn < zEnd ){` |
|    ! 0 | 7017 | `							zIn++;` |
|    ! 0 | 7018 | `						}` |
|    ! 0 | 7019 | `					}else{` |
|    125 | 7020 | `						while( zIn < zEnd ){` |
|    123 | 7021 | `							if( zIn[0] == '\n' ){` |
|     19 | 7022 | `								if( zIn[-1] != '\\' ){` |
|     19 | 7023 | `									break;` |
|    ! 0 | 7024 | `								}` |
|    105 | 7025 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 7026 | `								/* Inline comments */` |
|    ! 0 | 7027 | `								break;` |
|      - | 7028 | `							}` |
|    105 | 7029 | `							zIn++;` |
|      1 | 7030 | `						}` |
|      - | 7031 | `					}` |
|      - | 7032 | `					/* Trim the value */` |
|     21 | 7033 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 7034 | `					SyStringFullTrim(&sEntry);` |
|     21 | 7035 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 7036 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 7037 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 7038 | `					}` |
|     21 | 7039 | `					if( sEntry.nByte > 0 ){` |
|     21 | 7040 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 7041 | `					}` |
|      - | 7042 | `					/* Insert the key and it's value */` |
|     21 | 7043 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 7044 | `				}` |
|     12 | 7045 | `			}else{` |
|    ! 0 | 7046 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 7047 | `					zIn++;` |
|    ! 0 | 7048 | `				}` |
|      - | 7049 | `			}` |
|     23 | 7050 | `			pCur = pOldCur;` |
|      - | 7051 | `		}` |
|      1 | 7052 | `	}` |
|     13 | 7053 | `	SyHashRelease(&sHash);` |
|      - | 7054 | `	/* Return the parse of the INI string */` |
|     13 | 7055 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 7056 | `	return SXRET_OK;` |
|      7 | 7057 |  |
|      - | 7058 | `/*` |
|      - | 7059 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 7060 | ` *  Parse a configuration string.` |
|      - | 7061 | ` * Parameters` |
|      - | 7062 | ` *  $ini` |
|      - | 7063 | ` *   The contents of the ini file being parsed.` |
|      - | 7064 | ` *  $process_sections` |
|      - | 7065 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 7066 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 7067 | ` *  $scanner_mode (Not used)` |
|      - | 7068 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 7069 | ` *   then option values will not be parsed.` |
|      - | 7070 | ` * Return` |
|      - | 7071 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 7072 | ` */` |
|     10 | 7073 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7074 |  |
|      - | 7075 | `	const char *zIni;` |
|      - | 7076 | `	int nByte;` |
|     11 | 7077 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7078 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 7079 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7080 | `		return PH7_OK;` |
|      - | 7081 | `	}` |
|      - | 7082 | `	/* Extract the raw INI buffer */` |
|     11 | 7083 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 7084 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|     11 | 7085 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      6 | 7086 |  |
|      - | 7087 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7088 |  |
|      - | 7089 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7090 |  |
|      - | 7091 | `/*` |
|      - | 7092 | ` * Ctype Functions.` |
|      - | 7093 | ` * Status:` |
|      - | 7094 | ` *    Stable.` |
|      - | 7095 | ` */` |
|      - | 7096 | `/*` |
|      - | 7097 | ` * bool ctype_alnum(string $text)` |
|      - | 7098 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 7099 | ` * Parameters` |
|      - | 7100 | ` *  $text` |
|      - | 7101 | ` *   The tested string.` |
|      - | 7102 | ` * Return` |
|      - | 7103 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 7104 | ` */` |
|     16 | 7105 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7106 |  |
|      - | 7107 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7108 | `	int nLen;` |
|     17 | 7109 | `	if( nArg < 1 ){` |
|      - | 7110 | `		/* Missing arguments,return FALSE */` |
|      3 | 7111 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7112 | `		return PH7_OK;` |
|      - | 7113 | `	}` |
|      - | 7114 | `	/* Extract the target string */` |
|     15 | 7115 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 7116 | `	zEnd = &zIn[nLen];` |
|     15 | 7117 | `	if( nLen < 1 ){` |
|      - | 7118 | `		/* Empty string,return FALSE */` |
|      3 | 7119 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7120 | `		return PH7_OK;` |
|      - | 7121 | `	}` |
|      - | 7122 | `	/* Perform the requested operation */` |
|     32 | 7123 | `	for(;;){` |
|     65 | 7124 | `		if( zIn >= zEnd ){` |
|      - | 7125 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7126 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7127 | `			return PH7_OK;` |
|      - | 7128 | `		}` |
|     57 | 7129 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 7130 | `			break;` |
|      - | 7131 | `		}` |
|      - | 7132 | `		/* Point to the next character */` |
|     53 | 7133 | `		zIn++;` |
|      1 | 7134 | `	}` |
|      - | 7135 | `	/* The test failed,return FALSE */` |
|      5 | 7136 | `	ph7_result_bool(pCtx,0);` |
|      5 | 7137 | `	return PH7_OK;` |
|      9 | 7138 |  |
|      - | 7139 | `/*` |
|      - | 7140 | ` * bool ctype_alpha(string $text)` |
|      - | 7141 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 7142 | ` * Parameters` |
|      - | 7143 | ` *  $text` |
|      - | 7144 | ` *   The tested string.` |
|      - | 7145 | ` * Return` |
|      - | 7146 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 7147 | ` */` |
|     18 | 7148 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7149 |  |
|      - | 7150 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7151 | `	int nLen;` |
|     19 | 7152 | `	if( nArg < 1 ){` |
|      - | 7153 | `		/* Missing arguments,return FALSE */` |
|      3 | 7154 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7155 | `		return PH7_OK;` |
|      - | 7156 | `	}` |
|      - | 7157 | `	/* Extract the target string */` |
|     17 | 7158 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7159 | `	zEnd = &zIn[nLen];` |
|     17 | 7160 | `	if( nLen < 1 ){` |
|      - | 7161 | `		/* Empty string,return FALSE */` |
|      3 | 7162 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7163 | `		return PH7_OK;` |
|      - | 7164 | `	}` |
|      - | 7165 | `	/* Perform the requested operation */` |
|     42 | 7166 | `	for(;;){` |
|     85 | 7167 | `		if( zIn >= zEnd ){` |
|      - | 7168 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7169 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7170 | `			return PH7_OK;` |
|      - | 7171 | `		}` |
|     77 | 7172 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 7173 | `			break;` |
|      - | 7174 | `		}` |
|      - | 7175 | `		/* Point to the next character */` |
|     71 | 7176 | `		zIn++;` |
|      1 | 7177 | `	}` |
|      - | 7178 | `	/* The test failed,return FALSE */` |
|      7 | 7179 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7180 | `	return PH7_OK;` |
|     10 | 7181 |  |
|      - | 7182 | `/*` |
|      - | 7183 | ` * bool ctype_cntrl(string $text)` |
|      - | 7184 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 7185 | ` * Parameters` |
|      - | 7186 | ` *  $text` |
|      - | 7187 | ` *   The tested string.` |
|      - | 7188 | ` * Return` |
|      - | 7189 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 7190 | ` */` |
|     18 | 7191 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7192 |  |
|      - | 7193 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7194 | `	int nLen;` |
|     19 | 7195 | `	if( nArg < 1 ){` |
|      - | 7196 | `		/* Missing arguments,return FALSE */` |
|      3 | 7197 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7198 | `		return PH7_OK;` |
|      - | 7199 | `	}` |
|      - | 7200 | `	/* Extract the target string */` |
|     17 | 7201 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7202 | `	zEnd = &zIn[nLen];` |
|     17 | 7203 | `	if( nLen < 1 ){` |
|      - | 7204 | `		/* Empty string,return FALSE */` |
|      3 | 7205 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7206 | `		return PH7_OK;` |
|      - | 7207 | `	}` |
|      - | 7208 | `	/* Perform the requested operation */` |
|     14 | 7209 | `	for(;;){` |
|     29 | 7210 | `		if( zIn >= zEnd ){` |
|      - | 7211 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7212 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7213 | `			return PH7_OK;` |
|      - | 7214 | `		}` |
|     21 | 7215 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7216 | `			/* UTF-8 stream  */` |
|    ! 0 | 7217 | `			break;` |
|      - | 7218 | `		}` |
|     21 | 7219 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 7220 | `			break;` |
|      - | 7221 | `		}` |
|      - | 7222 | `		/* Point to the next character */` |
|     15 | 7223 | `		zIn++;` |
|      1 | 7224 | `	}` |
|      - | 7225 | `	/* The test failed,return FALSE */` |
|      7 | 7226 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7227 | `	return PH7_OK;` |
|     10 | 7228 |  |
|      - | 7229 | `/*` |
|      - | 7230 | ` * bool ctype_digit(string $text)` |
|      - | 7231 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 7232 | ` * Parameters` |
|      - | 7233 | ` *  $text` |
|      - | 7234 | ` *   The tested string.` |
|      - | 7235 | ` * Return` |
|      - | 7236 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 7237 | ` */` |
|   1630 | 7238 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7239 |  |
|      - | 7240 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7241 | `	int nLen;` |
|   1635 | 7242 | `	if( nArg < 1 ){` |
|      - | 7243 | `		/* Missing arguments,return FALSE */` |
|      3 | 7244 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7245 | `		return PH7_OK;` |
|      - | 7246 | `	}` |
|      - | 7247 | `	/* Extract the target string */` |
|   1633 | 7248 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1633 | 7249 | `	zEnd = &zIn[nLen];` |
|   1633 | 7250 | `	if( nLen < 1 ){` |
|      - | 7251 | `		/* Empty string,return FALSE */` |
|      3 | 7252 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7253 | `		return PH7_OK;` |
|      - | 7254 | `	}` |
|      - | 7255 | `	/* Perform the requested operation */` |
|   1530 | 7256 | `	for(;;){` |
|   3065 | 7257 | `		if( zIn >= zEnd ){` |
|      - | 7258 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1389 | 7259 | `			ph7_result_bool(pCtx,1);` |
|   1389 | 7260 | `			return PH7_OK;` |
|      - | 7261 | `		}` |
|   1681 | 7262 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7263 | `			/* UTF-8 stream  */` |
|    ! 0 | 7264 | `			break;` |
|      - | 7265 | `		}` |
|   1681 | 7266 | `		if( !SyisDigit(zIn[0]) ){` |
|    247 | 7267 | `			break;` |
|      - | 7268 | `		}` |
|      - | 7269 | `		/* Point to the next character */` |
|   1439 | 7270 | `		zIn++;` |
|      5 | 7271 | `	}` |
|      - | 7272 | `	/* The test failed,return FALSE */` |
|    247 | 7273 | `	ph7_result_bool(pCtx,0);` |
|    247 | 7274 | `	return PH7_OK;` |
|    820 | 7275 |  |
|      - | 7276 | `/*` |
|      - | 7277 | ` * bool ctype_xdigit(string $text)` |
|      - | 7278 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 7279 | ` * Parameters` |
|      - | 7280 | ` *  $text` |
|      - | 7281 | ` *   The tested string.` |
|      - | 7282 | ` * Return` |
|      - | 7283 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 7284 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 7285 | ` */` |
|     20 | 7286 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7287 |  |
|      - | 7288 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7289 | `	int nLen;` |
|     21 | 7290 | `	if( nArg < 1 ){` |
|      - | 7291 | `		/* Missing arguments,return FALSE */` |
|      3 | 7292 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7293 | `		return PH7_OK;` |
|      - | 7294 | `	}` |
|      - | 7295 | `	/* Extract the target string */` |
|     19 | 7296 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 7297 | `	zEnd = &zIn[nLen];` |
|     19 | 7298 | `	if( nLen < 1 ){` |
|      - | 7299 | `		/* Empty string,return FALSE */` |
|      3 | 7300 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7301 | `		return PH7_OK;` |
|      - | 7302 | `	}` |
|      - | 7303 | `	/* Perform the requested operation */` |
|     46 | 7304 | `	for(;;){` |
|     93 | 7305 | `		if( zIn >= zEnd ){` |
|      - | 7306 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 7307 | `			ph7_result_bool(pCtx,1);` |
|     11 | 7308 | `			return PH7_OK;` |
|      - | 7309 | `		}` |
|     83 | 7310 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7311 | `			/* UTF-8 stream  */` |
|    ! 0 | 7312 | `			break;` |
|      - | 7313 | `		}` |
|     83 | 7314 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 7315 | `			break;` |
|      - | 7316 | `		}` |
|      - | 7317 | `		/* Point to the next character */` |
|     77 | 7318 | `		zIn++;` |
|      1 | 7319 | `	}` |
|      - | 7320 | `	/* The test failed,return FALSE */` |
|      7 | 7321 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7322 | `	return PH7_OK;` |
|     11 | 7323 |  |
|      - | 7324 | `/*` |
|      - | 7325 | ` * bool ctype_graph(string $text)` |
|      - | 7326 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 7327 | ` * Parameters` |
|      - | 7328 | ` *  $text` |
|      - | 7329 | ` *   The tested string.` |
|      - | 7330 | ` * Return` |
|      - | 7331 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 7332 | ` * (no white space), FALSE otherwise.` |
|      - | 7333 | ` */` |
|     18 | 7334 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7335 |  |
|      - | 7336 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7337 | `	int nLen;` |
|     19 | 7338 | `	if( nArg < 1 ){` |
|      - | 7339 | `		/* Missing arguments,return FALSE */` |
|      3 | 7340 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7341 | `		return PH7_OK;` |
|      - | 7342 | `	}` |
|      - | 7343 | `	/* Extract the target string */` |
|     17 | 7344 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7345 | `	zEnd = &zIn[nLen];` |
|     17 | 7346 | `	if( nLen < 1 ){` |
|      - | 7347 | `		/* Empty string,return FALSE */` |
|      3 | 7348 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7349 | `		return PH7_OK;` |
|      - | 7350 | `	}` |
|      - | 7351 | `	/* Perform the requested operation */` |
|     57 | 7352 | `	for(;;){` |
|    115 | 7353 | `		if( zIn >= zEnd ){` |
|      - | 7354 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7355 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7356 | `			return PH7_OK;` |
|      - | 7357 | `		}` |
|    107 | 7358 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7359 | `			/* UTF-8 stream  */` |
|    ! 0 | 7360 | `			break;` |
|      - | 7361 | `		}` |
|    107 | 7362 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 7363 | `			break;` |
|      - | 7364 | `		}` |
|      - | 7365 | `		/* Point to the next character */` |
|    101 | 7366 | `		zIn++;` |
|      1 | 7367 | `	}` |
|      - | 7368 | `	/* The test failed,return FALSE */` |
|      7 | 7369 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7370 | `	return PH7_OK;` |
|     10 | 7371 |  |
|      - | 7372 | `/*` |
|      - | 7373 | ` * bool ctype_print(string $text)` |
|      - | 7374 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 7375 | ` * Parameters` |
|      - | 7376 | ` *  $text` |
|      - | 7377 | ` *   The tested string.` |
|      - | 7378 | ` * Return` |
|      - | 7379 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 7380 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 7381 | ` *  or control function at all.` |
|      - | 7382 | ` */` |
|     18 | 7383 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7384 |  |
|      - | 7385 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7386 | `	int nLen;` |
|     19 | 7387 | `	if( nArg < 1 ){` |
|      - | 7388 | `		/* Missing arguments,return FALSE */` |
|      3 | 7389 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7390 | `		return PH7_OK;` |
|      - | 7391 | `	}` |
|      - | 7392 | `	/* Extract the target string */` |
|     17 | 7393 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7394 | `	zEnd = &zIn[nLen];` |
|     17 | 7395 | `	if( nLen < 1 ){` |
|      - | 7396 | `		/* Empty string,return FALSE */` |
|      3 | 7397 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7398 | `		return PH7_OK;` |
|      - | 7399 | `	}` |
|      - | 7400 | `	/* Perform the requested operation */` |
|     63 | 7401 | `	for(;;){` |
|    127 | 7402 | `		if( zIn >= zEnd ){` |
|      - | 7403 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7404 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7405 | `			return PH7_OK;` |
|      - | 7406 | `		}` |
|    119 | 7407 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7408 | `			/* UTF-8 stream  */` |
|    ! 0 | 7409 | `			break;` |
|      - | 7410 | `		}` |
|    119 | 7411 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 7412 | `			break;` |
|      - | 7413 | `		}` |
|      - | 7414 | `		/* Point to the next character */` |
|    113 | 7415 | `		zIn++;` |
|      1 | 7416 | `	}` |
|      - | 7417 | `	/* The test failed,return FALSE */` |
|      7 | 7418 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7419 | `	return PH7_OK;` |
|     10 | 7420 |  |
|      - | 7421 | `/*` |
|      - | 7422 | ` * bool ctype_punct(string $text)` |
|      - | 7423 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 7424 | ` * Parameters` |
|      - | 7425 | ` *  $text` |
|      - | 7426 | ` *   The tested string.` |
|      - | 7427 | ` * Return` |
|      - | 7428 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 7429 | ` *  digit or blank, FALSE otherwise.` |
|      - | 7430 | ` */` |
|     20 | 7431 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7432 |  |
|      - | 7433 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7434 | `	int nLen;` |
|     21 | 7435 | `	if( nArg < 1 ){` |
|      - | 7436 | `		/* Missing arguments,return FALSE */` |
|      3 | 7437 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7438 | `		return PH7_OK;` |
|      - | 7439 | `	}` |
|      - | 7440 | `	/* Extract the target string */` |
|     19 | 7441 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 7442 | `	zEnd = &zIn[nLen];` |
|     19 | 7443 | `	if( nLen < 1 ){` |
|      - | 7444 | `		/* Empty string,return FALSE */` |
|      3 | 7445 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7446 | `		return PH7_OK;` |
|      - | 7447 | `	}` |
|      - | 7448 | `	/* Perform the requested operation */` |
|     38 | 7449 | `	for(;;){` |
|     77 | 7450 | `		if( zIn >= zEnd ){` |
|      - | 7451 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7452 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7453 | `			return PH7_OK;` |
|      - | 7454 | `		}` |
|     69 | 7455 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7456 | `			/* UTF-8 stream  */` |
|    ! 0 | 7457 | `			break;` |
|      - | 7458 | `		}` |
|     69 | 7459 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 7460 | `			break;` |
|      - | 7461 | `		}` |
|      - | 7462 | `		/* Point to the next character */` |
|     61 | 7463 | `		zIn++;` |
|      1 | 7464 | `	}` |
|      - | 7465 | `	/* The test failed,return FALSE */` |
|      9 | 7466 | `	ph7_result_bool(pCtx,0);` |
|      9 | 7467 | `	return PH7_OK;` |
|     11 | 7468 |  |
|      - | 7469 | `/*` |
|      - | 7470 | ` * bool ctype_space(string $text)` |
|      - | 7471 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 7472 | ` * Parameters` |
|      - | 7473 | ` *  $text` |
|      - | 7474 | ` *   The tested string.` |
|      - | 7475 | ` * Return` |
|      - | 7476 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 7477 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 7478 | ` *  and form feed characters.` |
|      - | 7479 | ` */` |
|  60181 | 7480 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7481 |  |
|      - | 7482 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7483 | `	int nLen;` |
|  60186 | 7484 | `	if( nArg < 1 ){` |
|      - | 7485 | `		/* Missing arguments,return FALSE */` |
|      3 | 7486 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7487 | `		return PH7_OK;` |
|      - | 7488 | `	}` |
|      - | 7489 | `	/* Extract the target string */` |
|  60184 | 7490 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  60184 | 7491 | `	zEnd = &zIn[nLen];` |
|  60184 | 7492 | `	if( nLen < 1 ){` |
|      - | 7493 | `		/* Empty string,return FALSE */` |
|      3 | 7494 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7495 | `		return PH7_OK;` |
|      - | 7496 | `	}` |
|      - | 7497 | `	/* Perform the requested operation */` |
|  31171 | 7498 | `	for(;;){` |
|  62262 | 7499 | `		if( zIn >= zEnd ){` |
|      - | 7500 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   2061 | 7501 | `			ph7_result_bool(pCtx,1);` |
|   2061 | 7502 | `			return PH7_OK;` |
|      - | 7503 | `		}` |
|  60206 | 7504 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7505 | `			/* UTF-8 stream  */` |
|    ! 0 | 7506 | `			break;` |
|      - | 7507 | `		}` |
|  60206 | 7508 | `		if( !SyisSpace(zIn[0]) ){` |
|  58126 | 7509 | `			break;` |
|      - | 7510 | `		}` |
|      - | 7511 | `		/* Point to the next character */` |
|   2085 | 7512 | `		zIn++;` |
|      5 | 7513 | `	}` |
|      - | 7514 | `	/* The test failed,return FALSE */` |
|  58126 | 7515 | `	ph7_result_bool(pCtx,0);` |
|  58126 | 7516 | `	return PH7_OK;` |
|  30138 | 7517 |  |
|      - | 7518 | `/*` |
|      - | 7519 | ` * bool ctype_lower(string $text)` |
|      - | 7520 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 7521 | ` * Parameters` |
|      - | 7522 | ` *  $text` |
|      - | 7523 | ` *   The tested string.` |
|      - | 7524 | ` * Return` |
|      - | 7525 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 7526 | ` */` |
|     18 | 7527 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7528 |  |
|      - | 7529 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7530 | `	int nLen;` |
|     19 | 7531 | `	if( nArg < 1 ){` |
|      - | 7532 | `		/* Missing arguments,return FALSE */` |
|      3 | 7533 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7534 | `		return PH7_OK;` |
|      - | 7535 | `	}` |
|      - | 7536 | `	/* Extract the target string */` |
|     17 | 7537 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7538 | `	zEnd = &zIn[nLen];` |
|     17 | 7539 | `	if( nLen < 1 ){` |
|      - | 7540 | `		/* Empty string,return FALSE */` |
|      3 | 7541 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7542 | `		return PH7_OK;` |
|      - | 7543 | `	}` |
|      - | 7544 | `	/* Perform the requested operation */` |
|     27 | 7545 | `	for(;;){` |
|     55 | 7546 | `		if( zIn >= zEnd ){` |
|      - | 7547 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7548 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7549 | `			return PH7_OK;` |
|      - | 7550 | `		}` |
|     51 | 7551 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 7552 | `			break;` |
|      - | 7553 | `		}` |
|      - | 7554 | `		/* Point to the next character */` |
|     41 | 7555 | `		zIn++;` |
|      1 | 7556 | `	}` |
|      - | 7557 | `	/* The test failed,return FALSE */` |
|     11 | 7558 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7559 | `	return PH7_OK;` |
|     10 | 7560 |  |
|      - | 7561 | `/*` |
|      - | 7562 | ` * bool ctype_upper(string $text)` |
|      - | 7563 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 7564 | ` * Parameters` |
|      - | 7565 | ` *  $text` |
|      - | 7566 | ` *   The tested string.` |
|      - | 7567 | ` * Return` |
|      - | 7568 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 7569 | ` */` |
|     18 | 7570 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7571 |  |
|      - | 7572 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7573 | `	int nLen;` |
|     19 | 7574 | `	if( nArg < 1 ){` |
|      - | 7575 | `		/* Missing arguments,return FALSE */` |
|      3 | 7576 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7577 | `		return PH7_OK;` |
|      - | 7578 | `	}` |
|      - | 7579 | `	/* Extract the target string */` |
|     17 | 7580 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7581 | `	zEnd = &zIn[nLen];` |
|     17 | 7582 | `	if( nLen < 1 ){` |
|      - | 7583 | `		/* Empty string,return FALSE */` |
|      3 | 7584 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7585 | `		return PH7_OK;` |
|      - | 7586 | `	}` |
|      - | 7587 | `	/* Perform the requested operation */` |
|     28 | 7588 | `	for(;;){` |
|     57 | 7589 | `		if( zIn >= zEnd ){` |
|      - | 7590 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7591 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7592 | `			return PH7_OK;` |
|      - | 7593 | `		}` |
|     53 | 7594 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 7595 | `			break;` |
|      - | 7596 | `		}` |
|      - | 7597 | `		/* Point to the next character */` |
|     43 | 7598 | `		zIn++;` |
|      1 | 7599 | `	}` |
|      - | 7600 | `	/* The test failed,return FALSE */` |
|     11 | 7601 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7602 | `	return PH7_OK;` |
|     10 | 7603 |  |
|      - | 7604 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 7605 | `/*` |
|      - | 7606 | ` * Section:` |
|      - | 7607 | ` *    URL handling Functions.` |
|      - | 7608 | ` * Status:` |
|      - | 7609 | ` *    Stable.` |
|      - | 7610 | ` */` |
|      - | 7611 | `/*` |
|      - | 7612 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 7613 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 7614 | ` */` |
|   1026 | 7615 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 7616 |  |
|      - | 7617 | `	/* Store in the call context result buffer */` |
|   1028 | 7618 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 7619 | `	return SXRET_OK;` |
|      2 | 7620 |  |
|      - | 7621 | `/*` |
|      - | 7622 | ` * string base64_encode(string $data)` |
|      - | 7623 | ` * string convert_uuencode(string $data)` |
|      - | 7624 | ` *  Encodes data with MIME base64` |
|      - | 7625 | ` * Parameter` |
|      - | 7626 | ` *  $data` |
|      - | 7627 | ` *    Data to encode` |
|      - | 7628 | ` * Return` |
|      - | 7629 | ` *  Encoded data or FALSE on failure.` |
|      - | 7630 | ` */` |
|     10 | 7631 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7632 |  |
|      - | 7633 | `	const char *zIn;` |
|      - | 7634 | `	int nLen;` |
|     11 | 7635 | `	if( nArg < 1 ){` |
|      - | 7636 | `		/* Missing arguments,return FALSE */` |
|      5 | 7637 | `		ph7_result_bool(pCtx,0);` |
|      5 | 7638 | `		return PH7_OK;` |
|      - | 7639 | `	}` |
|      - | 7640 | `	/* Extract the input string */` |
|      7 | 7641 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 7642 | `	if( nLen < 1 ){` |
|      - | 7643 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7644 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7645 | `		return PH7_OK;` |
|      - | 7646 | `	}` |
|      - | 7647 | `	/* Perform the BASE64 encoding */` |
|      7 | 7648 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 7649 | `	return PH7_OK;` |
|      6 | 7650 |  |
|      - | 7651 | `/*` |
|      - | 7652 | ` * string base64_decode(string $data)` |
|      - | 7653 | ` * string convert_uudecode(string $data)` |
|      - | 7654 | ` *  Decodes data encoded with MIME base64` |
|      - | 7655 | ` * Parameter` |
|      - | 7656 | ` *  $data` |
|      - | 7657 | ` *    Encoded data.` |
|      - | 7658 | ` * Return` |
|      - | 7659 | ` *  Returns the original data or FALSE on failure.` |
|      - | 7660 | ` */` |
|     36 | 7661 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 7662 |  |
|      - | 7663 | `	const char *zIn;` |
|      - | 7664 | `	int nLen;` |
|     38 | 7665 | `	if( nArg < 1 ){` |
|      - | 7666 | `		/* Missing arguments,return FALSE */` |
|      3 | 7667 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7668 | `		return PH7_OK;` |
|      - | 7669 | `	}` |
|      - | 7670 | `	/* Extract the input string */` |
|     36 | 7671 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 7672 | `	if( nLen < 1 ){` |
|      - | 7673 | `		/* Nothing to process,return FALSE */` |
|      3 | 7674 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7675 | `		return PH7_OK;` |
|      - | 7676 | `	}` |
|      - | 7677 | `	/* Perform the BASE64 decoding */` |
|     34 | 7678 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 7679 | `	return PH7_OK;` |
|     20 | 7680 |  |
|      - | 7681 | `/*` |
|      - | 7682 | ` * string urlencode(string $str)` |
|      - | 7683 | ` *  URL encoding` |
|      - | 7684 | ` * Parameter` |
|      - | 7685 | ` *  $data` |
|      - | 7686 | ` *   Input string.` |
|      - | 7687 | ` * Return` |
|      - | 7688 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 7689 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 7690 | ` *  encoded as plus (+) signs.` |
|      - | 7691 | ` */` |
|      6 | 7692 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7693 |  |
|      - | 7694 | `	const char *zIn;` |
|      - | 7695 | `	int nLen;` |
|      7 | 7696 | `	if( nArg < 1 ){` |
|      - | 7697 | `		/* Missing arguments,return FALSE */` |
|      3 | 7698 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7699 | `		return PH7_OK;` |
|      - | 7700 | `	}` |
|      - | 7701 | `	/* Extract the input string */` |
|      5 | 7702 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 7703 | `	if( nLen < 1 ){` |
|      - | 7704 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7705 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7706 | `		return PH7_OK;` |
|      - | 7707 | `	}` |
|      - | 7708 | `	/* Perform the URL encoding */` |
|      5 | 7709 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 7710 | `	return PH7_OK;` |
|      4 | 7711 |  |
|      - | 7712 | `/*` |
|      - | 7713 | ` * string urldecode(string $str)` |
|      - | 7714 | ` *  Decodes any %## encoding in the given string.` |
|      - | 7715 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 7716 | ` * Parameter` |
|      - | 7717 | ` *  $data` |
|      - | 7718 | ` *    Input string.` |
|      - | 7719 | ` * Return` |
|      - | 7720 | ` *  Decoded URL or FALSE on failure.` |
|      - | 7721 | ` */` |
|      8 | 7722 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7723 |  |
|      - | 7724 | `	const char *zIn;` |
|      - | 7725 | `	int nLen;` |
|      9 | 7726 | `	if( nArg < 1 ){` |
|      - | 7727 | `		/* Missing arguments,return FALSE */` |
|      3 | 7728 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7729 | `		return PH7_OK;` |
|      - | 7730 | `	}` |
|      - | 7731 | `	/* Extract the input string */` |
|      7 | 7732 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 7733 | `	if( nLen < 1 ){` |
|      - | 7734 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7735 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7736 | `		return PH7_OK;` |
|      - | 7737 | `	}` |
|      - | 7738 | `	/* Perform the URL decoding */` |
|      7 | 7739 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 7740 | `	return PH7_OK;` |
|      5 | 7741 |  |
|      - | 7742 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7743 | `/* Table of the built-in functions */` |
|      - | 7744 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 7745 | `	   /* Variable handling functions */` |
|      - | 7746 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 7747 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 7748 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 7749 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 7750 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 7751 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 7752 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 7753 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 7754 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 7755 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 7756 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 7757 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 7758 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 7759 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 7760 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 7761 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 7762 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 7763 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 7764 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 7765 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 7766 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7767 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 7768 | `	   /* Math functions */` |
|      - | 7769 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 7770 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 7771 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 7772 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 7773 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 7774 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 7775 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 7776 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 7777 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 7778 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 7779 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 7780 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 7781 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 7782 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 7783 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 7784 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 7785 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 7786 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 7787 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 7788 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 7789 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 7790 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 7791 | `	{ "round",    PH7_builtin_round        },` |
|      - | 7792 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 7793 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 7794 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 7795 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 7796 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 7797 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 7798 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 7799 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 7800 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 7801 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7802 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7803 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 7804 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7805 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7806 | `	   /* String handling functions */` |
|      - | 7807 |  |
|      - | 7808 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 7809 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 7810 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 7811 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 7812 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 7813 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 7814 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 7815 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 7816 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 7817 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 7818 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 7819 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 7820 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 7821 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 7822 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 7823 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 7824 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 7825 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 7826 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 7827 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 7828 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 7829 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 7830 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 7831 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 7832 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 7833 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 7834 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 7835 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 7836 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 7837 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 7838 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 7839 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 7840 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 7841 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 7842 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 7843 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 7844 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 7845 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 7846 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 7847 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 7848 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 7849 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 7850 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 7851 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 7852 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 7853 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 7854 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 7855 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 7856 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - | 7857 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - | 7858 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - | 7859 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 7860 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7861 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7862 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 7863 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 7864 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 7865 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 7866 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7867 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7868 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 7869 |  |
|      - | 7870 |  |
|      - | 7871 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 7872 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 7873 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 7874 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 7875 | `	{ "hash",         PH7_builtin_hash      },` |
|      - | 7876 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|      - | 7877 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|      - | 7878 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|      - | 7879 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 7880 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|      - | 7881 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|      - | 7882 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|      - | 7883 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|      - | 7884 | `	{ "filter_var",            PH7_builtin_filter_var },` |
|      - | 7885 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7886 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7887 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 7888 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 7889 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7890 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7891 |  |
|      - | 7892 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 7893 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 7894 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 7895 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 7896 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 7897 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 7898 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 7899 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 7900 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 7901 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 7902 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 7903 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 7904 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7905 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7906 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 7907 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7908 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7909 |  |
|      - | 7910 | `	         /* Ctype functions */` |
|      - | 7911 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 7912 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 7913 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 7914 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 7915 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 7916 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 7917 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 7918 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 7919 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 7920 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 7921 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 7922 | `	         /* Time functions */` |
|      - | 7923 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 7924 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 7925 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 7926 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 7927 | `	{ "date",        PH7_builtin_date         },` |
|      - | 7928 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 7929 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 7930 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 7931 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 7932 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 7933 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 7934 | `	        /* URL functions */` |
|      - | 7935 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 7936 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 7937 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 7938 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 7939 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 7940 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 7941 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 7942 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 7943 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7944 | `};` |
|      - | 7945 | `/*` |
|      - | 7946 | ` * Register the built-in functions defined above,the array functions` |
|      - | 7947 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 7948 | ` */` |
|   3110 | 7949 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      5 | 7950 |  |
|      - | 7951 | `	sxu32 n;` |
| 519375 | 7952 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 516265 | 7953 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 258135 | 7954 | `	}` |
|      - | 7955 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   3115 | 7956 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 7957 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   3115 | 7958 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   3115 | 7959 |  |
|      - | 7960 |  |
