# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2941/3351 lines (87.76%)

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
|     74 |   42 | `static int PH7_builtin_is_float(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   43 |  |
|     75 |   44 | `	int res = 0; /* Assume false by default */` |
|     75 |   45 | `	if( nArg > 0 ){` |
|     73 |   46 | `		res = ph7_value_is_float(apArg[0]);` |
|     36 |   47 | `	}` |
|      - |   48 | `	/* Query result */` |
|     75 |   49 | `	ph7_result_bool(pCtx,res);` |
|     75 |   50 | `	return PH7_OK;` |
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
|    240 |   62 | `static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |   63 |  |
|    242 |   64 | `	int res = 0; /* Assume false by default */` |
|    242 |   65 | `	if( nArg > 0 ){` |
|    240 |   66 | `		res = ph7_value_is_int(apArg[0]);` |
|    119 |   67 | `	}` |
|      - |   68 | `	/* Query result */` |
|    242 |   69 | `	ph7_result_bool(pCtx,res);` |
|    242 |   70 | `	return PH7_OK;` |
|      2 |   71 |  |
|      - |   72 | `/*` |
|      - |   73 | ` * bool is_string($var)` |
|      - |   74 | ` *  Finds out whether a variable is a string.` |
|      - |   75 | ` * Parameters` |
|      - |   76 | ` *   $var: The variable being evaluated.` |
|      - |   77 | ` * Return` |
|      - |   78 | ` *  TRUE if var is string. False otherwise.` |
|      - |   79 | ` */` |
|     88 |   80 | `static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   81 |  |
|     89 |   82 | `	int res = 0; /* Assume false by default */` |
|     89 |   83 | `	if( nArg > 0 ){` |
|     87 |   84 | `		res = ph7_value_is_string(apArg[0]);` |
|     43 |   85 | `	}` |
|      - |   86 | `	/* Query result */` |
|     89 |   87 | `	ph7_result_bool(pCtx,res);` |
|     89 |   88 | `	return PH7_OK;` |
|      1 |   89 |  |
|      - |   90 | `/*` |
|      - |   91 | ` * bool is_null($var)` |
|      - |   92 | ` *  Finds out whether a variable is NULL.` |
|      - |   93 | ` * Parameters` |
|      - |   94 | ` *   $var: The variable being evaluated.` |
|      - |   95 | ` * Return` |
|      - |   96 | ` *  TRUE if var is NULL. False otherwise.` |
|      - |   97 | ` */` |
|     84 |   98 | `static int PH7_builtin_is_null(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |   99 |  |
|     86 |  100 | `	int res = 0; /* Assume false by default */` |
|     86 |  101 | `	if( nArg > 0 ){` |
|     84 |  102 | `		res = ph7_value_is_null(apArg[0]);` |
|     41 |  103 | `	}` |
|      - |  104 | `	/* Query result */` |
|     86 |  105 | `	ph7_result_bool(pCtx,res);` |
|     86 |  106 | `	return PH7_OK;` |
|      2 |  107 |  |
|      - |  108 | `/*` |
|      - |  109 | ` * bool is_numeric($var)` |
|      - |  110 | ` *  Find out whether a variable is NULL.` |
|      - |  111 | ` * Parameters` |
|      - |  112 | ` *  $var: The variable being evaluated.` |
|      - |  113 | ` * Return` |
|      - |  114 | ` *  True if var is numeric. False otherwise.` |
|      - |  115 | ` */` |
|     38 |  116 | `static int PH7_builtin_is_numeric(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  117 |  |
|     40 |  118 | `	int res = 0; /* Assume false by default */` |
|     40 |  119 | `	if( nArg > 0 ){` |
|     38 |  120 | `		res = ph7_value_is_numeric(apArg[0]);` |
|     18 |  121 | `	}` |
|      - |  122 | `	/* Query result */` |
|     40 |  123 | `	ph7_result_bool(pCtx,res);` |
|     40 |  124 | `	return PH7_OK;` |
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
|    194 |  152 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  153 |  |
|    196 |  154 | `	int res = 0; /* Assume false by default */` |
|    196 |  155 | `	if( nArg > 0 ){` |
|    194 |  156 | `		res = ph7_value_is_array(apArg[0]);` |
|     96 |  157 | `	}` |
|      - |  158 | `	/* Query result */` |
|    196 |  159 | `	ph7_result_bool(pCtx,res);` |
|    196 |  160 | `	return PH7_OK;` |
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
|     26 |  226 | `static int PH7_builtin_intval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  227 |  |
|     27 |  228 | `	if( nArg < 1 ){` |
|      - |  229 | `		/* return 0 */` |
|      3 |  230 | `		ph7_result_int(pCtx,0);` |
|      2 |  231 | `	}else{` |
|      - |  232 | `		sxi64 iVal;` |
|      - |  233 | `		/* Perform the cast */` |
|     25 |  234 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|     25 |  235 | `		ph7_result_int64(pCtx,iVal);` |
|      - |  236 | `	}` |
|     27 |  237 | `	return PH7_OK;` |
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
|      - |  262 | ` * bool boolval($var)` |
|      - |  263 | ` *  Get the boolean value of a variable.` |
|      - |  264 | ` * Parameter` |
|      - |  265 | ` *  $var: The variable being processed.` |
|      - |  266 | ` * Return` |
|      - |  267 | ` *  the bool value of a variable.` |
|      - |  268 | ` */` |
|     16 |  269 | `static int PH7_builtin_boolval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  270 |  |
|      - |  271 | `	int bVal;` |
|     18 |  272 | `	if( nArg != 1 ){` |
|      4 |  273 | `		return PH7_VmThrowException(pCtx,` |
|      - |  274 | `			"ArgumentCountError",` |
|      - |  275 | `			"boolval() expects exactly 1 argument, %d given",` |
|      1 |  276 | `			nArg` |
|      - |  277 | `			);` |
|      - |  278 | `	}` |
|      - |  279 | `	/* Perform the cast */` |
|     15 |  280 | `	bVal = ph7_value_to_bool(apArg[0]);` |
|     15 |  281 | `	ph7_result_bool(pCtx,bVal);` |
|     15 |  282 | `	return PH7_OK;` |
|     10 |  283 |  |
|      - |  284 | `/*` |
|      - |  285 | ` * bool empty($var)` |
|      - |  286 | ` *  Determine whether a variable is empty.` |
|      - |  287 | ` * Parameters` |
|      - |  288 | ` *   $var: The variable being checked.` |
|      - |  289 | ` * Return` |
|      - |  290 | ` *  0 if var has a non-empty and non-zero value.1 otherwise.` |
|      - |  291 | ` */` |
|  23780 |  292 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  293 |  |
|  23782 |  294 | `	int res = 1; /* Assume empty by default */` |
|  23782 |  295 | `	if( nArg > 0 ){` |
|  23780 |  296 | `		res = ph7_value_is_empty(apArg[0]);` |
|  11889 |  297 | `	}` |
|  23782 |  298 | `	ph7_result_bool(pCtx,res);` |
|  23782 |  299 | `	return PH7_OK;` |
|      - |  300 |  |
|      2 |  301 |  |
|      - |  302 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |  303 | `#define PH7_NEED_BUILTIN_REG 1` |
|      - |  304 | `#endif` |
|      - |  305 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - |  306 | `#define PH7_NEED_FMT_AND_INI 1` |
|      - |  307 | `#endif` |
|      - |  308 |  |
|      - |  309 | `/* Math functions moved to builtin_math.c */` |
|      - |  310 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - |  311 | `/*` |
|      - |  312 | ` * Section:` |
|      - |  313 | ` *    String handling Functions.` |
|      - |  314 | ` * Status:` |
|      - |  315 | ` *    Stable.` |
|      - |  316 | ` */` |
|      - |  317 | `/*` |
|      - |  318 | ` * string substr(string $string,int $start[, int $length ])` |
|      - |  319 | ` *  Return part of a string.` |
|      - |  320 | ` * Parameters` |
|      - |  321 | ` *  $string` |
|      - |  322 | ` *   The input string. Must be one character or longer.` |
|      - |  323 | ` * $start` |
|      - |  324 | ` *   If start is non-negative, the returned string will start at the start'th position` |
|      - |  325 | ` *   in string, counting from zero. For instance, in the string 'abcdef', the character` |
|      - |  326 | ` *   at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - |  327 | ` *   If start is negative, the returned string will start at the start'th character` |
|      - |  328 | ` *   from the end of string.` |
|      - |  329 | ` *   If string is less than or equal to start characters long, FALSE will be returned.` |
|      - |  330 | ` * $length` |
|      - |  331 | ` *   If length is given and is positive, the string returned will contain at most length` |
|      - |  332 | ` *   characters beginning from start (depending on the length of string).` |
|      - |  333 | ` *   If length is given and is negative, then that many characters will be omitted from` |
|      - |  334 | ` *   the end of string (after the start position has been calculated when a start is negative).` |
|      - |  335 | ` *   If start denotes the position of this truncation or beyond, false will be returned.` |
|      - |  336 | ` *   If length is given and is 0, FALSE or NULL an empty string will be returned.` |
|      - |  337 | ` *   If length is omitted, the substring starting from start until the end of the string` |
|      - |  338 | ` *   will be returned.` |
|      - |  339 | ` * Return` |
|      - |  340 | ` *  Returns the extracted part of string, or FALSE on failure or an empty string.` |
|      - |  341 | ` */` |
| 174146 |  342 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  343 |  |
|      - |  344 | `	const char *zSource,*zOfft;` |
|      - |  345 | `	int nOfft,nLen,nSrcLen;` |
| 174148 |  346 | `	if( nArg < 2 ){` |
|      - |  347 | `		/* return FALSE */` |
|      5 |  348 | `		ph7_result_bool(pCtx,0);` |
|      5 |  349 | `		return PH7_OK;` |
|      - |  350 | `	}` |
|      - |  351 | `	/* Extract the target string */` |
| 174144 |  352 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 174144 |  353 | `	if( nSrcLen < 1 ){` |
|      - |  354 | `		/* Empty string,return FALSE */` |
|  10302 |  355 | `		ph7_result_bool(pCtx,0);` |
|  10302 |  356 | `		return PH7_OK;` |
|      - |  357 | `	}` |
| 163844 |  358 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  359 | `	/* Extract the offset */` |
| 163844 |  360 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 163844 |  361 | `	if( nOfft < 0 ){` |
|  27420 |  362 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  27420 |  363 | `		if( zOfft < zSource ){` |
|      - |  364 | `			/* Invalid offset */` |
|      5 |  365 | `			ph7_result_bool(pCtx,0);` |
|      5 |  366 | `			return PH7_OK;` |
|      - |  367 | `		}` |
|  27416 |  368 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  27416 |  369 | `		nOfft = (int)(zOfft-zSource);` |
| 150133 |  370 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  371 | `		/* Invalid offset */` |
|    106 |  372 | `		ph7_result_bool(pCtx,0);` |
|    106 |  373 | `		return PH7_OK;` |
|    ! 0 |  374 | `	}else{` |
| 136322 |  375 | `		zOfft = &zSource[nOfft];` |
| 136322 |  376 | `		nLen = nSrcLen - nOfft;` |
|      - |  377 | `	}` |
| 163736 |  378 | `	if( nArg > 2 ){` |
|      - |  379 | `		/* Extract the length */` |
| 135142 |  380 | `		nLen = ph7_value_to_int(apArg[2]);` |
| 135142 |  381 | `		if( nLen == 0 ){` |
|      - |  382 | `			/* Invalid length,return an empty string */` |
|      5 |  383 | `			ph7_result_string(pCtx,"",0);` |
|      5 |  384 | `			return PH7_OK;` |
| 135138 |  385 | `		}else if( nLen < 0 ){` |
|  27418 |  386 | `			nLen = nSrcLen + nLen - nOfft;` |
|  27418 |  387 | `			if( nLen < 1 ){` |
|      - |  388 | `				/* Invalid  length */` |
|      3 |  389 | `				nLen = nSrcLen - nOfft;` |
|      1 |  390 | `			}` |
|  13708 |  391 | `		}` |
| 135138 |  392 | `		if( nLen + nOfft > nSrcLen ){` |
|      - |  393 | `			/* Invalid length */` |
|   3834 |  394 | `			nLen = nSrcLen - nOfft;` |
|   1916 |  395 | `		}` |
|  67568 |  396 | `	}` |
|      - |  397 | `	/* Return the substring */` |
| 163732 |  398 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 163732 |  399 | `	return PH7_OK;` |
|  87075 |  400 |  |
|      - |  401 | `/*` |
|      - |  402 | ` * int substr_compare(string $main_str,string $str ,int $offset[,int $length[,bool $case_insensitivity = false ]])` |
|      - |  403 | ` *  Binary safe comparison of two strings from an offset, up to length characters.` |
|      - |  404 | ` * Parameters` |
|      - |  405 | ` *  $main_str` |
|      - |  406 | ` *  The main string being compared.` |
|      - |  407 | ` *  $str` |
|      - |  408 | ` *   The secondary string being compared.` |
|      - |  409 | ` * $offset` |
|      - |  410 | ` *  The start position for the comparison. If negative, it starts counting from` |
|      - |  411 | ` *  the end of the string.` |
|      - |  412 | ` * $length` |
|      - |  413 | ` *  The length of the comparison. The default value is the largest of the length` |
|      - |  414 | ` *  of the str compared to the length of main_str less the offset.` |
|      - |  415 | ` * $case_insensitivity` |
|      - |  416 | ` *  If case_insensitivity is TRUE, comparison is case insensitive.` |
|      - |  417 | ` * Return` |
|      - |  418 | ` *  Returns < 0 if main_str from position offset is less than str, > 0 if it is greater than` |
|      - |  419 | ` *  str, and 0 if they are equal. If offset is equal to or greater than the length of main_str` |
|      - |  420 | ` *  or length is set and is less than 1, substr_compare() prints a warning and returns FALSE.` |
|      - |  421 | ` */` |
|     26 |  422 | `static int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  423 |  |
|      - |  424 | `	const char *zSource,*zOfft,*zSub;` |
|      - |  425 | `	int nOfft,nLen,nSrcLen,nSublen;` |
|     27 |  426 | `	int iCase = 0;` |
|      - |  427 | `	int rc;` |
|     27 |  428 | `	if( nArg < 3 ){` |
|      - |  429 | `		/* Missing arguments,return FALSE */` |
|      5 |  430 | `		ph7_result_bool(pCtx,0);` |
|      5 |  431 | `		return PH7_OK;` |
|      - |  432 | `	}` |
|      - |  433 | `	/* Extract the target string */` |
|     23 |  434 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|     23 |  435 | `	if( nSrcLen < 1 ){` |
|      - |  436 | `		/* Empty string,return FALSE */` |
|      3 |  437 | `		ph7_result_bool(pCtx,0);` |
|      3 |  438 | `		return PH7_OK;` |
|      - |  439 | `	}` |
|     21 |  440 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  441 | `	/* Extract the substring */` |
|     21 |  442 | `	zSub = ph7_value_to_string(apArg[1],&nSublen);` |
|     21 |  443 | `	if( nSublen < 1 \|\| nSublen > nSrcLen){` |
|      - |  444 | `		/* Empty string,return FALSE */` |
|      3 |  445 | `		ph7_result_bool(pCtx,0);` |
|      3 |  446 | `		return PH7_OK;` |
|      - |  447 | `	}` |
|      - |  448 | `	/* Extract the offset */` |
|     19 |  449 | `	nOfft = ph7_value_to_int(apArg[2]);` |
|     19 |  450 | `	if( nOfft < 0 ){` |
|      5 |  451 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|      5 |  452 | `		if( zOfft < zSource ){` |
|      - |  453 | `			/* Invalid offset */` |
|      3 |  454 | `			ph7_result_bool(pCtx,0);` |
|      3 |  455 | `			return PH7_OK;` |
|      - |  456 | `		}` |
|      3 |  457 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|      3 |  458 | `		nOfft = (int)(zOfft-zSource);` |
|     16 |  459 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  460 | `		/* Invalid offset */` |
|      3 |  461 | `		ph7_result_bool(pCtx,0);` |
|      3 |  462 | `		return PH7_OK;` |
|    ! 0 |  463 | `	}else{` |
|     13 |  464 | `		zOfft = &zSource[nOfft];` |
|     13 |  465 | `		nLen = nSrcLen - nOfft;` |
|      - |  466 | `	}` |
|     15 |  467 | `	if( nArg > 3 ){` |
|      - |  468 | `		/* Extract the length */` |
|     13 |  469 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     13 |  470 | `		if( nLen < 1 ){` |
|      - |  471 | `			/* Invalid  length */` |
|      5 |  472 | `			ph7_result_int(pCtx,1);` |
|      5 |  473 | `			return PH7_OK;` |
|      9 |  474 | `		}else if( nLen + nOfft > nSrcLen ){` |
|      - |  475 | `			/* Invalid length */` |
|      3 |  476 | `			nLen = nSrcLen - nOfft;` |
|      1 |  477 | `		}` |
|      9 |  478 | `		if( nArg > 4 ){` |
|      - |  479 | `			/* Case-sensitive or not */` |
|      5 |  480 | `			iCase = ph7_value_to_bool(apArg[4]);` |
|      2 |  481 | `		}` |
|      4 |  482 | `	}` |
|      - |  483 | `	/* Perform the comparison */` |
|     11 |  484 | `	if( iCase ){` |
|      3 |  485 | `		rc = SyStrnicmp(zOfft,zSub,(sxu32)nLen);` |
|      2 |  486 | `	}else{` |
|      9 |  487 | `		rc = SyStrncmp(zOfft,zSub,(sxu32)nLen);` |
|      - |  488 | `	}` |
|      - |  489 | `	/* Comparison result */` |
|     11 |  490 | `	ph7_result_int(pCtx,rc);` |
|     11 |  491 | `	return PH7_OK;` |
|     14 |  492 |  |
|      - |  493 | `/*` |
|      - |  494 | ` * int substr_count(string $haystack,string $needle[,int $offset = 0 [,int $length ]])` |
|      - |  495 | ` *  Count the number of substring occurrences.` |
|      - |  496 | ` * Parameters` |
|      - |  497 | ` * $haystack` |
|      - |  498 | ` *   The string to search in` |
|      - |  499 | ` * $needle` |
|      - |  500 | ` *   The substring to search for` |
|      - |  501 | ` * $offset` |
|      - |  502 | ` *  The offset where to start counting` |
|      - |  503 | ` * $length (NOT USED)` |
|      - |  504 | ` *  The maximum length after the specified offset to search for the substring.` |
|      - |  505 | ` *  It outputs a warning if the offset plus the length is greater than the haystack length.` |
|      - |  506 | ` * Return` |
|      - |  507 | ` *  Toral number of substring occurrences.` |
|      - |  508 | ` */` |
|     24 |  509 | `static int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  510 |  |
|      - |  511 | `	const char *zText,*zPattern,*zEnd;` |
|      - |  512 | `	int nTextlen,nPatlen;` |
|     25 |  513 | `	int iCount = 0;` |
|      - |  514 | `	sxu32 nOfft;` |
|      - |  515 | `	sxi32 rc;` |
|     25 |  516 | `	if( nArg < 2 ){` |
|      - |  517 | `		/* Missing arguments */` |
|      5 |  518 | `		ph7_result_int(pCtx,0);` |
|      5 |  519 | `		return PH7_OK;` |
|      - |  520 | `	}` |
|      - |  521 | `	/* Point to the haystack */` |
|     21 |  522 | `	zText = ph7_value_to_string(apArg[0],&nTextlen);` |
|      - |  523 | `	/* Point to the neddle */` |
|     21 |  524 | `	zPattern = ph7_value_to_string(apArg[1],&nPatlen);` |
|     21 |  525 | `	if( nTextlen < 1 \|\| nPatlen < 1 \|\| nPatlen > nTextlen ){` |
|      - |  526 | `		/* NOOP,return zero */` |
|      3 |  527 | `		ph7_result_int(pCtx,0);` |
|      3 |  528 | `		return PH7_OK;` |
|      - |  529 | `	}` |
|     19 |  530 | `	if( nArg > 2 ){` |
|      - |  531 | `		int iOfft;` |
|      - |  532 | `		/* Extract the offset */` |
|     15 |  533 | `		iOfft = ph7_value_to_int(apArg[2]);` |
|     15 |  534 | `		if( iOfft < 0 \|\| iOfft > nTextlen ){` |
|      - |  535 | `			/* Invalid offset,return zero */` |
|      3 |  536 | `			ph7_result_int(pCtx,0);` |
|      3 |  537 | `			return PH7_OK;` |
|      - |  538 | `		}` |
|      - |  539 | `		/* Point to the desired offset */` |
|     13 |  540 | `		zText = &zText[iOfft];` |
|      - |  541 | `		/* Adjust length */` |
|     13 |  542 | `		nTextlen -= iOfft;` |
|      6 |  543 | `	}` |
|      - |  544 | `	/* Point to the end of the string */` |
|     17 |  545 | `	zEnd = &zText[nTextlen];` |
|     17 |  546 | `	if( nArg > 3 ){` |
|      - |  547 | `		int nLen;` |
|      - |  548 | `		/* Extract the length */` |
|     13 |  549 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     13 |  550 | `		if( nLen < 0 \|\| nLen > nTextlen ){` |
|      - |  551 | `			/* Invalid length,return 0 */` |
|      7 |  552 | `			ph7_result_int(pCtx,0);` |
|      7 |  553 | `			return PH7_OK;` |
|      - |  554 | `		}` |
|      - |  555 | `		/* Adjust pointer */` |
|      7 |  556 | `		nTextlen = nLen;` |
|      7 |  557 | `		zEnd = &zText[nTextlen];` |
|      3 |  558 | `	}` |
|      - |  559 | `	/* Perform the search */` |
|     12 |  560 | `	for(;;){` |
|     25 |  561 | `		rc = SyBlobSearch((const void *)zText,(sxu32)(zEnd-zText),(const void *)zPattern,nPatlen,&nOfft);` |
|     25 |  562 | `		if( rc != SXRET_OK ){` |
|      - |  563 | `			/* Pattern not found,break immediately */` |
|      9 |  564 | `			break;` |
|      - |  565 | `		}` |
|      - |  566 | `		/* Increment counter and update the offset */` |
|     17 |  567 | `		iCount++;` |
|     17 |  568 | `		zText += nOfft + nPatlen;` |
|     17 |  569 | `		if( zText >= zEnd ){` |
|      3 |  570 | `			break;` |
|      - |  571 | `		}` |
|      1 |  572 | `	}` |
|      - |  573 | `	/* Pattern count */` |
|     11 |  574 | `	ph7_result_int(pCtx,iCount);` |
|     11 |  575 | `	return PH7_OK;` |
|     13 |  576 |  |
|      - |  577 | `/*` |
|      - |  578 | ` * string chunk_split(string $body[,int $chunklen = 76 [, string $end = "\r\n" ]])` |
|      - |  579 | ` *   Split a string into smaller chunks.` |
|      - |  580 | ` * Parameters` |
|      - |  581 | ` *  $body` |
|      - |  582 | ` *   The string to be chunked.` |
|      - |  583 | ` * $chunklen` |
|      - |  584 | ` *   The chunk length.` |
|      - |  585 | ` * $end` |
|      - |  586 | ` *   The line ending sequence.` |
|      - |  587 | ` * Return` |
|      - |  588 | ` *  The chunked string or NULL on failure.` |
|      - |  589 | ` */` |
|     16 |  590 | `static int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  591 |  |
|     17 |  592 | `	const char *zIn,*zEnd,*zSep = "\r\n";` |
|      - |  593 | `	int nSepLen,nChunkLen,nLen;` |
|     17 |  594 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  595 | `		/* Nothing to split,return null */` |
|      5 |  596 | `		ph7_result_null(pCtx);` |
|      5 |  597 | `		return PH7_OK;` |
|      - |  598 | `	}` |
|      - |  599 | `	/* initialize/Extract arguments */` |
|     13 |  600 | `	nSepLen = (int)sizeof("\r\n") - 1;` |
|     13 |  601 | `	nChunkLen = 76;` |
|     13 |  602 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 |  603 | `	zEnd = &zIn[nLen];` |
|     13 |  604 | `	if( nArg > 1 ){` |
|      - |  605 | `		/* Chunk length */` |
|     13 |  606 | `		nChunkLen = ph7_value_to_int(apArg[1]);` |
|     13 |  607 | `		if( nChunkLen < 1 ){` |
|      - |  608 | `			/* Switch back to the default length */` |
|      3 |  609 | `			nChunkLen = 76;` |
|      1 |  610 | `		}` |
|     13 |  611 | `		if( nArg > 2 ){` |
|      - |  612 | `			/* Separator */` |
|      9 |  613 | `			zSep = ph7_value_to_string(apArg[2],&nSepLen);` |
|      9 |  614 | `			if( nSepLen < 1 ){` |
|      - |  615 | `				/* Switch back to the default separator */` |
|      3 |  616 | `				zSep = "\r\n";` |
|      3 |  617 | `				nSepLen = (int)sizeof("\r\n") - 1;` |
|      1 |  618 | `			}` |
|      4 |  619 | `		}` |
|      6 |  620 | `	}` |
|      - |  621 | `	/* Perform the requested operation */` |
|     13 |  622 | `	if( nChunkLen > nLen ){` |
|      - |  623 | `		/* Nothing to split,return the string and the separator */` |
|      9 |  624 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);` |
|      9 |  625 | `		return PH7_OK;` |
|      - |  626 | `	}` |
|     17 |  627 | `	while( zIn < zEnd ){` |
|     13 |  628 | `		if( nChunkLen > (int)(zEnd-zIn) ){` |
|      3 |  629 | `			nChunkLen = (int)(zEnd - zIn);` |
|      1 |  630 | `		}` |
|      - |  631 | `		/* Append the chunk and the separator */` |
|     13 |  632 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);` |
|      - |  633 | `		/* Point beyond the chunk */` |
|     13 |  634 | `		zIn += nChunkLen;` |
|      1 |  635 | `	}` |
|      5 |  636 | `	return PH7_OK;` |
|      9 |  637 |  |
|      - |  638 | `/*` |
|      - |  639 | ` * string addslashes(string $str)` |
|      - |  640 | ` *  Quote string with slashes.` |
|      - |  641 | ` *  Returns a string with backslashes before characters that need` |
|      - |  642 | ` *  to be quoted in database queries etc. These characters are single` |
|      - |  643 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - |  644 | ` * Parameter` |
|      - |  645 | ` *  str: The string to be escaped.` |
|      - |  646 | ` * Return` |
|      - |  647 | ` *  Returns the escaped string` |
|      - |  648 | ` */` |
|     24 |  649 | `static int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  650 |  |
|      - |  651 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  652 | `	int nLen;` |
|      - |  653 | `	/* PHP enforces exactly one argument. */` |
|     26 |  654 | `	if( nArg != 1 ){` |
|      7 |  655 | `		return PH7_VmThrowException(pCtx,` |
|      - |  656 | `			"ArgumentCountError",` |
|      - |  657 | `			"addslashes() expects exactly 1 argument, %d given",` |
|      2 |  658 | `			nArg` |
|      - |  659 | `			);` |
|      - |  660 | `	}` |
|      - |  661 | `	/* NULL is deprecated and treated as an empty string; other invalid` |
|      - |  662 | `	 * types still produce a TypeError. */` |
|     22 |  663 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 |  664 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - |  665 | `			E_DEPRECATED,` |
|      - |  666 | `			"addslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - |  667 | `			);` |
|      - |  668 | `		/* fall through so conversion below yields empty string */` |
|      1 |  669 | `	}` |
|      - |  670 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     38 |  671 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     29 |  672 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     18 |  673 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 |  674 | `		return PH7_VmThrowException(pCtx,` |
|      - |  675 | `			"TypeError",` |
|      - |  676 | `			"addslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 |  677 | `			ph7_type_name(apArg[0])` |
|      - |  678 | `			);` |
|      - |  679 | `	}` |
|      - |  680 | `	/* Convert to string representation first and obtain length. */` |
|     19 |  681 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     19 |  682 | `	if( nLen < 1 ){` |
|      - |  683 | `		/* Return the empty string */` |
|      5 |  684 | `		ph7_result_string(pCtx,"",0);` |
|      5 |  685 | `		return PH7_OK;` |
|      - |  686 | `	}` |
|     15 |  687 | `	zEnd = &zIn[nLen];` |
|     15 |  688 | `	zCur = 0; /* cc warning */` |
|     20 |  689 | `	for(;;){` |
|     41 |  690 | `		if( zIn >= zEnd ){` |
|      - |  691 | `			/* No more input */` |
|     15 |  692 | `			break;` |
|      - |  693 | `		}` |
|     27 |  694 | `		zCur = zIn;` |
|      - |  695 | `		/* scan until a character that needs escaping (', ", \\, or NUL) */` |
|     89 |  696 | `		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' && zIn[0] != '\0' ){` |
|     63 |  697 | `			zIn++;` |
|      1 |  698 | `		}` |
|     27 |  699 | `		if( zIn > zCur ){` |
|      - |  700 | `			/* Append raw contents */` |
|     23 |  701 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 |  702 | `		}` |
|     27 |  703 | `		if( zIn < zEnd ){` |
|     17 |  704 | `			int c = zIn[0];` |
|     17 |  705 | `			if( c == '\0' ){` |
|      - |  706 | `				/* PHP escapes NUL as "\\0" (two characters) */` |
|      3 |  707 | `				ph7_result_string(pCtx,"\\0",2);` |
|      2 |  708 | `			}else{` |
|     15 |  709 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - |  710 | `			}` |
|      8 |  711 | `		}` |
|     27 |  712 | `		zIn++;` |
|      1 |  713 | `	}` |
|     15 |  714 | `	return PH7_OK;` |
|     14 |  715 |  |
|      - |  716 | `/*` |
|      - |  717 | ` * Check if the given character is present in the given mask.` |
|      - |  718 | ` * Return TRUE if present. FALSE otherwise.` |
|      - |  719 | ` */` |
|    124 |  720 | `static int cSlashCheckMask(int c,const char *zMask,int nLen)` |
|      1 |  721 |  |
|    125 |  722 | `	const char *zEnd = &zMask[nLen];` |
|    555 |  723 | `	while( zMask < zEnd ){` |
|      - |  724 | `		/* Support range syntax A..Z where A and Z are literal bytes.  The` |
|      - |  725 | `		 * original PH7 implementation ignored ranges; tests rely on them so` |
|      - |  726 | `		 * provide a simple on-the-fly check here. */` |
|    475 |  727 | `		if( zMask + 3 < zEnd && zMask[1] == '.' && zMask[2] == '.' ){` |
|      3 |  728 | `			int lo = (unsigned char)zMask[0];` |
|      3 |  729 | `			int hi = (unsigned char)zMask[3];` |
|      3 |  730 | `			if( lo > hi ){` |
|    ! 0 |  731 | `				int tmp = lo; lo = hi; hi = tmp;` |
|    ! 0 |  732 | `			}` |
|      3 |  733 | `			if( c >= lo && c <= hi ){` |
|      3 |  734 | `				return 1;` |
|      - |  735 | `			}` |
|      - |  736 | `			/* consume the range specifier */` |
|    ! 0 |  737 | `			zMask += 4;` |
|    ! 0 |  738 | `			continue;` |
|      - |  739 | `		}` |
|    473 |  740 | `		if( zMask[0] == c ){` |
|      - |  741 | `			/* Character present,return TRUE */` |
|     43 |  742 | `			return 1;` |
|      - |  743 | `		}` |
|      - |  744 | `		/* Advance the pointer */` |
|    431 |  745 | `		zMask++;` |
|      1 |  746 | `	}` |
|      - |  747 | `	/* Not present */` |
|     81 |  748 | `	return 0;` |
|     63 |  749 |  |
|      - |  750 | `/*` |
|      - |  751 | ` * string addcslashes(string $str,string $charlist)` |
|      - |  752 | ` *  Quote string with slashes in a C style.` |
|      - |  753 | ` * Parameter` |
|      - |  754 | ` *  $str:` |
|      - |  755 | ` *    The string to be escaped.` |
|      - |  756 | ` *  $charlist:` |
|      - |  757 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|      - |  758 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|      - |  759 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|      - |  760 | ` * Return` |
|      - |  761 | ` *  Returns the escaped string.` |
|      - |  762 | ` * Note:` |
|      - |  763 | ` *  Range characters [i.e: 'A..Z'] is not implemented in the current release.` |
|      - |  764 | ` */` |
|     34 |  765 | `static int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  766 |  |
|      - |  767 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - |  768 | `	int nLen,nMask;` |
|      - |  769 | `	/* PHP enforces exactly two arguments. */` |
|     36 |  770 | `	if( nArg != 2 ){` |
|      7 |  771 | `		return PH7_VmThrowException(pCtx,` |
|      - |  772 | `			"ArgumentCountError",` |
|      - |  773 | `			"addcslashes() expects exactly 2 arguments, %d given",` |
|      2 |  774 | `			nArg` |
|      - |  775 | `			);` |
|      - |  776 | `	}` |
|      - |  777 | `	/* First argument must be a string-ish value.  NULL is deprecated and` |
|      - |  778 | `	 * treated as the empty string (PHP 8.1). */` |
|     32 |  779 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      - |  780 | `		/* Emit deprecation only once, similar to PHP behaviour. */` |
|      3 |  781 | `		PH7_VmThrowError(pCtx->pVm,0,/* iErr will be patched to 8192 below */` |
|      - |  782 | `			E_DEPRECATED,` |
|      - |  783 | `			"addcslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - |  784 | `			);` |
|      - |  785 | `		/* treat as empty string; fall through to conversion logic */` |
|     56 |  786 | `	} else if( ph7_value_is_array(apArg[0]) \|\|` |
|     41 |  787 | `	          ph7_value_is_object(apArg[0]) \|\|` |
|     26 |  788 | `	          ph7_value_is_resource(apArg[0]) ){` |
|      4 |  789 | `		return PH7_VmThrowException(pCtx,` |
|      - |  790 | `			"TypeError",` |
|      - |  791 | `			"addcslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 |  792 | `			ph7_type_name(apArg[0])` |
|      - |  793 | `			);` |
|      - |  794 | `	}` |
|      - |  795 | `	/* Second argument must be a string.  NULL is deprecated and treated as` |
|      - |  796 | `	 * an empty mask per PHP semantics.  Arrays/objects/resources still` |
|      - |  797 | `	 * trigger a TypeError. */` |
|     30 |  798 | `	if( ph7_value_is_null(apArg[1]) ){` |
|      3 |  799 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - |  800 | `			E_DEPRECATED,` |
|      - |  801 | `			"addcslashes(): Passing null to parameter #2 ($characters) of type string is deprecated"` |
|      - |  802 | `			);` |
|      - |  803 | `		/* allow through so it becomes empty string below */` |
|     52 |  804 | `	} else if( ph7_value_is_array(apArg[1]) \|\|` |
|     38 |  805 | `	          ph7_value_is_object(apArg[1]) \|\|` |
|     24 |  806 | `	          ph7_value_is_resource(apArg[1]) ){` |
|      4 |  807 | `		return PH7_VmThrowException(pCtx,` |
|      - |  808 | `			"TypeError",` |
|      - |  809 | `			"addcslashes(): Argument #2 ($characters) must be of type string, %s given",` |
|      2 |  810 | `			ph7_type_name(apArg[1])` |
|      - |  811 | `			);` |
|      - |  812 | `	}` |
|      - |  813 | `	/* Extract the string to process */` |
|     27 |  814 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      - |  815 | `	/* NULL would never reach here due to the check above. */` |
|     27 |  816 | `	if( nLen < 1 ){` |
|      - |  817 | `		/* Empty string returns itself. */` |
|      5 |  818 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      5 |  819 | `		return PH7_OK;` |
|      - |  820 | `	}` |
|      - |  821 | `	/* Extract the desired mask */` |
|     23 |  822 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|     23 |  823 | `	zEnd = &zIn[nLen];` |
|     23 |  824 | `	zCur = 0; /* cc warning */` |
|     29 |  825 | `	for(;;){` |
|     59 |  826 | `		if( zIn >= zEnd ){` |
|      - |  827 | `			/* No more input */` |
|     23 |  828 | `			break;` |
|      - |  829 | `		}` |
|     37 |  830 | `		zCur = zIn;` |
|     91 |  831 | `		while( zIn < zEnd && !cSlashCheckMask(zIn[0],zMask,nMask) ){` |
|     55 |  832 | `			zIn++;` |
|      1 |  833 | `		}` |
|     37 |  834 | `		if( zIn > zCur ){` |
|      - |  835 | `			/* Append raw contents */` |
|     33 |  836 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 |  837 | `		}` |
|     37 |  838 | `		if( zIn < zEnd ){` |
|      - |  839 | `			/* Make sure we treat the byte as unsigned to avoid negative values` |
|      - |  840 | `			 * on platforms where char is signed. */` |
|     19 |  841 | `			int c = (unsigned char)zIn[0];` |
|      - |  842 | `			/* Handle special C-like escapes for common control characters first.` |
|      - |  843 | `			 * PHP outputs "\n" "\r" "\t" "\v" "\f" when those chars are` |
|      - |  844 | `			 * in the mask. NUL is left to the octal conversion below. */` |
|     19 |  845 | `			if( c == '\n' ){` |
|      3 |  846 | `				ph7_result_string(pCtx,"\\n",2);` |
|     18 |  847 | `			}else if( c == '\r' ){` |
|      3 |  848 | `				ph7_result_string(pCtx,"\\r",2);` |
|     16 |  849 | `			}else if( c == '\t' ){` |
|      3 |  850 | `				ph7_result_string(pCtx,"\\t",2);` |
|     14 |  851 | `			}else if( c == '\v' ){` |
|      3 |  852 | `				ph7_result_string(pCtx,"\\v",2);` |
|     12 |  853 | `			}else if( c == '\f' ){` |
|      3 |  854 | `				ph7_result_string(pCtx,"\\f",2);` |
|     10 |  855 | `			}else if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|      - |  856 | `				/* Convert to octal.  PHP always emits three-digit zero-padded` |
|      - |  857 | `				 * octal escapes (\001 not \1). */` |
|      7 |  858 | `				ph7_result_string_format(pCtx,"\\%03o",c);` |
|      4 |  859 | `			}else{` |
|      3 |  860 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - |  861 | `			}` |
|      9 |  862 | `		}` |
|     37 |  863 | `		zIn++;` |
|      1 |  864 | `	}` |
|     23 |  865 | `	return PH7_OK;` |
|     19 |  866 |  |
|      - |  867 | `/*` |
|      - |  868 | ` * string quotemeta(string $str)` |
|      - |  869 | ` *  Quote meta characters.` |
|      - |  870 | ` * Parameter` |
|      - |  871 | ` *  $str:` |
|      - |  872 | ` *    The string to be escaped.` |
|      - |  873 | ` * Return` |
|      - |  874 | ` *  Returns the escaped string.` |
|      - |  875 | `*/` |
|     10 |  876 | `static int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  877 |  |
|      - |  878 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  879 | `	int nLen;` |
|     11 |  880 | `	if( nArg < 1 ){` |
|      - |  881 | `		/* Nothing to process,retun NULL */` |
|      3 |  882 | `		ph7_result_null(pCtx);` |
|      3 |  883 | `		return PH7_OK;` |
|      - |  884 | `	}` |
|      - |  885 | `	/* Extract the string to process */` |
|      9 |  886 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      9 |  887 | `	if( nLen < 1 ){` |
|      - |  888 | `		/* Return the empty string */` |
|      3 |  889 | `		ph7_result_string(pCtx,"",0);` |
|      3 |  890 | `		return PH7_OK;` |
|      - |  891 | `	}` |
|      7 |  892 | `	zEnd = &zIn[nLen];` |
|      7 |  893 | `	zCur = 0; /* cc warning */` |
|     17 |  894 | `	for(;;){` |
|     35 |  895 | `		if( zIn >= zEnd ){` |
|      - |  896 | `			/* No more input */` |
|      7 |  897 | `			break;` |
|      - |  898 | `		}` |
|     29 |  899 | `		zCur = zIn;` |
|     55 |  900 | `		while( zIn < zEnd && !cSlashCheckMask(zIn[0],".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1) ){` |
|     27 |  901 | `			zIn++;` |
|      1 |  902 | `		}` |
|     29 |  903 | `		if( zIn > zCur ){` |
|      - |  904 | `			/* Append raw contents */` |
|     11 |  905 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      5 |  906 | `		}` |
|     29 |  907 | `		if( zIn < zEnd ){` |
|     27 |  908 | `			int c = zIn[0];` |
|     27 |  909 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|     13 |  910 | `		}` |
|     29 |  911 | `		zIn++;` |
|      1 |  912 | `	}` |
|      7 |  913 | `	return PH7_OK;` |
|      6 |  914 |  |
|      - |  915 | `/*` |
|      - |  916 | ` * string stripslashes(string $str)` |
|      - |  917 | ` *  Un-quotes a quoted string.` |
|      - |  918 | ` *  Returns a string with backslashes before characters that need` |
|      - |  919 | ` *  to be quoted in database queries etc. These characters are single` |
|      - |  920 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - |  921 | ` * Parameter` |
|      - |  922 | ` *  $str` |
|      - |  923 | ` *   The input string.` |
|      - |  924 | ` * Return` |
|      - |  925 | ` *  Returns a string with backslashes stripped off.` |
|      - |  926 | ` */` |
|      8 |  927 | `static int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  928 |  |
|      - |  929 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  930 | `	int nLen;` |
|      9 |  931 | `	if( nArg < 1 ){` |
|      - |  932 | `		/* Nothing to process,retun NULL */` |
|      3 |  933 | `		ph7_result_null(pCtx);` |
|      3 |  934 | `		return PH7_OK;` |
|      - |  935 | `	}` |
|      - |  936 | `	/* Extract the string to process */` |
|      7 |  937 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 |  938 | `	if( zIn == 0 ){` |
|    ! 0 |  939 | `		ph7_result_null(pCtx);` |
|    ! 0 |  940 | `		return PH7_OK;` |
|      - |  941 | `	}` |
|      7 |  942 | `	zEnd = &zIn[nLen];` |
|      7 |  943 | `	zCur = 0; /* cc warning */` |
|      - |  944 | `	/* Encode the string */` |
|      4 |  945 | `	for(;;){` |
|      9 |  946 | `		if( zIn >= zEnd ){` |
|      - |  947 | `			/* No more input */` |
|      5 |  948 | `			break;` |
|      - |  949 | `		}` |
|      5 |  950 | `		zCur = zIn;` |
|     17 |  951 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 |  952 | `			zIn++;` |
|      1 |  953 | `		}` |
|      5 |  954 | `		if( zIn > zCur ){` |
|      - |  955 | `			/* Append raw contents */` |
|      5 |  956 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 |  957 | `		}` |
|      5 |  958 | `		if( &zIn[1] < zEnd ){` |
|      3 |  959 | `			int c = zIn[1];` |
|      3 |  960 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - |  961 | `				/* Ignore the backslash */` |
|      3 |  962 | `				zIn++;` |
|      1 |  963 | `			}` |
|      2 |  964 | `		}else{` |
|      3 |  965 | `			break;` |
|      - |  966 | `		}` |
|      1 |  967 | `	}` |
|      7 |  968 | `	return PH7_OK;` |
|      5 |  969 |  |
|      - |  970 | `/*` |
|      - |  971 | ` * string htmlspecialchars(string $string [, int $flags = ENT_COMPAT \| ENT_HTML401 [, string $charset]])` |
|      - |  972 | ` *  HTML escaping of special characters.` |
|      - |  973 | ` *  The translations performed are:` |
|      - |  974 | ` *   '&' (ampersand) ==> '&amp;'` |
|      - |  975 | ` *   '"' (double quote) ==> '&quot;' when ENT_NOQUOTES is not set.` |
|      - |  976 | ` *   "'" (single quote) ==> '&#039;' only when ENT_QUOTES is set.` |
|      - |  977 | ` *   '<' (less than) ==> '&lt;'` |
|      - |  978 | ` *   '>' (greater than) ==> '&gt;'` |
|      - |  979 | ` * Parameters` |
|      - |  980 | ` *  $string` |
|      - |  981 | ` *   The string being converted.` |
|      - |  982 | ` * $flags` |
|      - |  983 | ` *   A bitmask of one or more of the following flags, which specify how to handle quotes.` |
|      - |  984 | ` *   The default is ENT_COMPAT \| ENT_HTML401.` |
|      - |  985 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone.` |
|      - |  986 | ` *   ENT_QUOTES 	Will convert both double and single quotes.` |
|      - |  987 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted.` |
|      - |  988 | ` *   ENT_IGNORE 	Silently discard invalid code unit sequences instead of returning an empty string.` |
|      - |  989 | ` * $charset` |
|      - |  990 | ` *  Defines character set used in conversion. The default character set is ISO-8859-1. (Not used)` |
|      - |  991 | ` * Return` |
|      - |  992 | ` *  The escaped string or NULL on failure.` |
|      - |  993 | ` */` |
|     20 |  994 | `static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  995 |  |
|      - |  996 | `	const char *zCur,*zIn,*zEnd;` |
|     21 |  997 | `	int iFlags = 0x01\|0x40; /* ENT_COMPAT \| ENT_HTML401 */` |
|      - |  998 | `	int nLen,c;` |
|     21 |  999 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1000 | `		/* Missing/Invalid arguments,return NULL */` |
|      9 | 1001 | `		ph7_result_null(pCtx);` |
|      9 | 1002 | `		return PH7_OK;` |
|      - | 1003 | `	}` |
|      - | 1004 | `	/* Extract the target string */` |
|     13 | 1005 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1006 | `	/* Return early when the input is empty, mirroring PHP's behavior. */` |
|     13 | 1007 | `	if( nLen == 0 ){` |
|      3 | 1008 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1009 | `		return PH7_OK;` |
|      - | 1010 | `	}` |
|     11 | 1011 | `	zEnd = &zIn[nLen];` |
|      - | 1012 | `	/* Extract the flags if available */` |
|     11 | 1013 | `	if( nArg > 1 ){` |
|      9 | 1014 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 | 1015 | `		if( iFlags < 0 ){` |
|      3 | 1016 | `			iFlags = 0x01\|0x40;` |
|      1 | 1017 | `		}` |
|      4 | 1018 | `	}` |
|      - | 1019 | `	/* Perform the requested operation */` |
|     23 | 1020 | `	for(;;){` |
|     47 | 1021 | `		if( zIn >= zEnd ){` |
|      9 | 1022 | `			break;` |
|      - | 1023 | `		}` |
|     39 | 1024 | `		zCur = zIn;` |
|     83 | 1025 | `		while( zIn < zEnd && zIn[0] != '&' && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '<' && zIn[0] != '>' ){` |
|     45 | 1026 | `			zIn++;` |
|      1 | 1027 | `		}` |
|     39 | 1028 | `		if( zCur < zIn ){` |
|      - | 1029 | `			/* Append the raw string verbatim */` |
|     17 | 1030 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      8 | 1031 | `		}` |
|     39 | 1032 | `		if( zIn >= zEnd ){` |
|      3 | 1033 | `			break;` |
|      - | 1034 | `		}` |
|     37 | 1035 | `		c = zIn[0];` |
|     37 | 1036 | `		if( c == '&' ){` |
|      - | 1037 | `			/* Expand '&amp;' */` |
|      9 | 1038 | `			ph7_result_string(pCtx,"&amp;",(int)sizeof("&amp;")-1);` |
|     33 | 1039 | `		}else if( c == '<' ){` |
|      - | 1040 | `			/* Expand '&lt;' */` |
|      7 | 1041 | `			ph7_result_string(pCtx,"&lt;",(int)sizeof("&lt;")-1);` |
|     26 | 1042 | `		}else if( c == '>' ){` |
|      - | 1043 | `			/* Expand '&gt;' */` |
|      9 | 1044 | `			ph7_result_string(pCtx,"&gt;",(int)sizeof("&gt;")-1);` |
|     19 | 1045 | `		}else if( c == '\'' ){` |
|      5 | 1046 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 1047 | `				/* Expand '&#039;' */` |
|      5 | 1048 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      3 | 1049 | `			}else{` |
|      - | 1050 | `				/* Leave the single quote untouched */` |
|    ! 0 | 1051 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      1 | 1052 | `			}` |
|     13 | 1053 | `		}else if( c == '"' ){` |
|     11 | 1054 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      - | 1055 | `				/* Expand '&quot;' */` |
|      7 | 1056 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      4 | 1057 | `			}else{` |
|      - | 1058 | `				/* Leave the double quote untouched */` |
|      5 | 1059 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      - | 1060 | `			}` |
|      5 | 1061 | `		}` |
|      - | 1062 | `		/* Ignore the unsafe HTML character */` |
|     37 | 1063 | `		zIn++;` |
|      1 | 1064 | `	}` |
|     11 | 1065 | `	return PH7_OK;` |
|     11 | 1066 |  |
|      - | 1067 | `/*` |
|      - | 1068 | ` * string htmlspecialchars_decode(string $string[,int $quote_style = ENT_COMPAT ])` |
|      - | 1069 | ` *  Unescape HTML entities.` |
|      - | 1070 | ` * Parameters` |
|      - | 1071 | ` *  $string` |
|      - | 1072 | ` *   The string to decode` |
|      - | 1073 | ` *  $quote_style` |
|      - | 1074 | ` *    The quote style. One of the following constants:` |
|      - | 1075 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone (default)` |
|      - | 1076 | ` *   ENT_QUOTES 	Will convert both double and single quotes` |
|      - | 1077 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted` |
|      - | 1078 | ` * Return` |
|      - | 1079 | ` *  The unescaped string or NULL on failure.` |
|      - | 1080 | ` */` |
|     16 | 1081 | `static int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1082 |  |
|      - | 1083 | `	const char *zCur,*zIn,*zEnd;` |
|     17 | 1084 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 1085 | `	int nLen,nJump;` |
|     17 | 1086 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1087 | `		/* Missing/Invalid arguments,return NULL */` |
|      7 | 1088 | `		ph7_result_null(pCtx);` |
|      7 | 1089 | `		return PH7_OK;` |
|      - | 1090 | `	}` |
|      - | 1091 | `	/* Extract the target string */` |
|     11 | 1092 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 1093 | `	zEnd = &zIn[nLen];` |
|      - | 1094 | `	/* Extract the flags if available */` |
|     11 | 1095 | `	if( nArg > 1 ){` |
|      7 | 1096 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      7 | 1097 | `		if( iFlags < 0 ){` |
|      3 | 1098 | `			iFlags = 0x01;` |
|      1 | 1099 | `		}` |
|      3 | 1100 | `	}` |
|      - | 1101 | `	/* Perform the requested operation */` |
|     15 | 1102 | `	for(;;){` |
|     31 | 1103 | `		if( zIn >= zEnd ){` |
|     11 | 1104 | `			break;` |
|      - | 1105 | `		}` |
|     21 | 1106 | `		zCur = zIn;` |
|     51 | 1107 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|     31 | 1108 | `			zIn++;` |
|      1 | 1109 | `		}` |
|     21 | 1110 | `		if( zCur < zIn ){` |
|      - | 1111 | `			/* Append the raw string verbatim */` |
|      9 | 1112 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 1113 | `		}` |
|     21 | 1114 | `		nLen = (int)(zEnd-zIn);` |
|     21 | 1115 | `		nJump = (int)sizeof(char);` |
|     21 | 1116 | `		if( nLen >= (int)sizeof("&amp;")-1 && SyStrnicmp(zIn,"&amp;",sizeof("&amp;")-1) == 0 ){` |
|      - | 1117 | `			/* &amp; ==> '&' */` |
|      3 | 1118 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|      3 | 1119 | `			nJump = (int)sizeof("&amp;")-1;` |
|     20 | 1120 | `		}else if( nLen >= (int)sizeof("&lt;")-1 && SyStrnicmp(zIn,"&lt;",sizeof("&lt;")-1) == 0 ){` |
|      - | 1121 | `			/* &lt; ==> < */` |
|      3 | 1122 | `			ph7_result_string(pCtx,"<",(int)sizeof(char));` |
|      3 | 1123 | `			nJump = (int)sizeof("&lt;")-1;` |
|     18 | 1124 | `		}else if( nLen >= (int)sizeof("&gt;")-1 && SyStrnicmp(zIn,"&gt;",sizeof("&gt;")-1) == 0 ){` |
|      - | 1125 | `			/* &gt; ==> '>' */` |
|      3 | 1126 | `			ph7_result_string(pCtx,">",(int)sizeof(char));` |
|      3 | 1127 | `			nJump = (int)sizeof("&gt;")-1;` |
|     16 | 1128 | `		}else if( nLen >= (int)sizeof("&quot;")-1 && SyStrnicmp(zIn,"&quot;",sizeof("&quot;")-1) == 0 ){` |
|      - | 1129 | `			/* &quot; ==> '"' */` |
|     13 | 1130 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      9 | 1131 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      5 | 1132 | `			}else{` |
|      - | 1133 | `				/* Leave untouched */` |
|      5 | 1134 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      - | 1135 | `			}` |
|     13 | 1136 | `			nJump = (int)sizeof("&quot;")-1;` |
|      9 | 1137 | `		}else if( nLen >= (int)sizeof("&#039;")-1 && SyStrnicmp(zIn,"&#039;",sizeof("&#039;")-1) == 0 ){` |
|      - | 1138 | `			/* &#039; ==> ''' */` |
|      3 | 1139 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 1140 | `				/* Expand ''' */` |
|      3 | 1141 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      2 | 1142 | `			}else{` |
|      - | 1143 | `				/* Leave untouched */` |
|    ! 0 | 1144 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      - | 1145 | `			}` |
|      3 | 1146 | `			nJump = (int)sizeof("&#039;")-1;` |
|      1 | 1147 | `		}else if( nLen >= (int)sizeof(char) ){` |
|      - | 1148 | `			/* expand '&' */` |
|    ! 0 | 1149 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 1150 | `		}else{` |
|      - | 1151 | `			/* No more input to process */` |
|    ! 0 | 1152 | `			break;` |
|      - | 1153 | `		}` |
|     21 | 1154 | `		zIn += nJump;` |
|      1 | 1155 | `	}` |
|     11 | 1156 | `	return PH7_OK;` |
|      9 | 1157 |  |
|      - | 1158 | `/* HTML encoding/Decoding table` |
|      - | 1159 | ` * Source: Symisc RunTime API.[chm@symisc.net]` |
|      - | 1160 | ` */` |
|      - | 1161 | `static const char *azHtmlEscape[] = {` |
|      - | 1162 | ` 	"&lt;","<","&gt;",">","&amp;","&","&quot;","\"","&#39;","'",` |
|      - | 1163 | `	"&#33;","!","&#36;","$","&#35;","#","&#37;","%","&#40;","(",` |
|      - | 1164 | `	"&#41;",")","&#123;","{","&#125;","}","&#61;","=","&#43;","+",` |
|      - | 1165 | `	"&#63;","?","&#91;","[","&#93;","]","&#64;","@","&#44;",","` |
|      - | 1166 | ` };` |
|      - | 1167 | `/*` |
|      - | 1168 | ` * array get_html_translation_table(void)` |
|      - | 1169 | ` *  Returns the translation table used by htmlspecialchars() and htmlentities().` |
|      - | 1170 | ` * Parameters` |
|      - | 1171 | ` *  None` |
|      - | 1172 | ` * Return` |
|      - | 1173 | ` *  The translation table as an array or NULL on failure.` |
|      - | 1174 | ` */` |
|      4 | 1175 | `static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1176 |  |
|      - | 1177 | `	ph7_value *pArray,*pValue;` |
|      - | 1178 | `	sxu32 n;` |
|      - | 1179 | `	/* Element value */` |
|      5 | 1180 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      5 | 1181 | `	if( pValue == 0 ){` |
|    ! 0 | 1182 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 1183 | `		SXUNUSED(apArg);` |
|      - | 1184 | `		/* Return NULL */` |
|    ! 0 | 1185 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1186 | `		return PH7_OK;` |
|      - | 1187 | `	}` |
|      - | 1188 | `	/* Create a new array */` |
|      5 | 1189 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 | 1190 | `	if( pArray == 0 ){` |
|      - | 1191 | `		/* Return NULL */` |
|    ! 0 | 1192 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1193 | `		return PH7_OK;` |
|      - | 1194 | `	}` |
|      - | 1195 | `	/* Make the table */` |
|     85 | 1196 | `	for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|      - | 1197 | `		/* Prepare the value */` |
|     81 | 1198 | `		ph7_value_string(pValue,azHtmlEscape[n],-1 /* Compute length automatically */);` |
|      - | 1199 | `		/* Insert the value */` |
|     81 | 1200 | `		ph7_array_add_strkey_elem(pArray,azHtmlEscape[n+1],pValue);` |
|      - | 1201 | `		/* Reset the string cursor */` |
|     81 | 1202 | `		ph7_value_reset_string_cursor(pValue);` |
|     41 | 1203 | `	}` |
|      - | 1204 | `	/*` |
|      - | 1205 | `	 * Return the array.` |
|      - | 1206 | `	 * Don't worry about freeing memory, everything will be automatically` |
|      - | 1207 | `	 * released upon we return from this function.` |
|      - | 1208 | `	 */` |
|      5 | 1209 | `	ph7_result_value(pCtx,pArray);` |
|      5 | 1210 | `	return PH7_OK;` |
|      3 | 1211 |  |
|      - | 1212 | `/*` |
|      - | 1213 | ` * string htmlentities( string $string [, int $flags = ENT_COMPAT \| ENT_HTML401]);` |
|      - | 1214 | ` *   Convert all applicable characters to HTML entities` |
|      - | 1215 | ` * Parameters` |
|      - | 1216 | ` * $string` |
|      - | 1217 | ` *   The input string.` |
|      - | 1218 | ` * $flags` |
|      - | 1219 | ` *  A bitmask of one or more of the flags (see block-comment on PH7_builtin_htmlspecialchars())` |
|      - | 1220 | ` * Return` |
|      - | 1221 | ` * The encoded string.` |
|      - | 1222 | ` */` |
|     10 | 1223 | `static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1224 |  |
|     11 | 1225 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 1226 | `	const char *zIn,*zEnd;` |
|      - | 1227 | `	int nLen,c;` |
|      - | 1228 | `	sxu32 n;` |
|     11 | 1229 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1230 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 1231 | `		ph7_result_null(pCtx);` |
|      5 | 1232 | `		return PH7_OK;` |
|      - | 1233 | `	}` |
|      - | 1234 | `	/* Extract the target string */` |
|      7 | 1235 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1236 | `	/* Handle empty string up front */` |
|      7 | 1237 | `	if( nLen == 0 ){` |
|      3 | 1238 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1239 | `		return PH7_OK;` |
|      - | 1240 | `	}` |
|      5 | 1241 | `	zEnd = &zIn[nLen];` |
|      - | 1242 | `	/* Extract the flags if available */` |
|      5 | 1243 | `	if( nArg > 1 ){` |
|      3 | 1244 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      3 | 1245 | `		if( iFlags < 0 ){` |
|      3 | 1246 | `			iFlags = 0x01;` |
|      1 | 1247 | `		}` |
|      1 | 1248 | `	}` |
|      - | 1249 | `	/* Perform the requested operation */` |
|     11 | 1250 | `	for(;;){` |
|     23 | 1251 | `		if( zIn >= zEnd ){` |
|      - | 1252 | `			/* No more input to process */` |
|      5 | 1253 | `			break;` |
|      - | 1254 | `		}` |
|     19 | 1255 | `		c = zIn[0];` |
|      - | 1256 | `		/* Perform a linear lookup on the decoding table */` |
|    233 | 1257 | `		for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    223 | 1258 | `			if( azHtmlEscape[n+1][0] == c ){` |
|      - | 1259 | `				/* Got one */` |
|      9 | 1260 | `				break;` |
|      - | 1261 | `			}` |
|    108 | 1262 | `		}` |
|     19 | 1263 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|      - | 1264 | `			/* Output the safe sequence [i.e: '<' ==> '&lt;"] */` |
|      9 | 1265 | `			if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 1266 | `				/* Expand the double quote verbatim */` |
|    ! 0 | 1267 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      9 | 1268 | `			}else if(c == '\'' && ((iFlags & 0x02 /*ENT_QUOTES*/) == 0 \|\| (iFlags & 0x04) /*ENT_NOQUOTES*/) ){` |
|      - | 1269 | `				/* expand single quote verbatim */` |
|    ! 0 | 1270 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|    ! 0 | 1271 | `			}else{` |
|      9 | 1272 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1/*Compute length automatically */);` |
|      - | 1273 | `			}` |
|      5 | 1274 | `		}else{` |
|      - | 1275 | `			/* Output character verbatim */` |
|     11 | 1276 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 1277 | `		}` |
|     19 | 1278 | `		zIn++;` |
|      1 | 1279 | `	}` |
|      5 | 1280 | `	return PH7_OK;` |
|      6 | 1281 |  |
|      - | 1282 | `/*` |
|      - | 1283 | ` * string html_entity_decode(string $string [, int $quote_style = ENT_COMPAT [, string $charset = 'UTF-8' ]])` |
|      - | 1284 | ` *   Perform the reverse operation of html_entity_decode().` |
|      - | 1285 | ` * Parameters` |
|      - | 1286 | ` * $string` |
|      - | 1287 | ` *   The input string.` |
|      - | 1288 | ` * $flags` |
|      - | 1289 | ` *  A bitmask of one or more of the flags (see comment on PH7_builtin_htmlspecialchars())` |
|      - | 1290 | ` * Return` |
|      - | 1291 | ` * The decoded string.` |
|      - | 1292 | ` */` |
|     28 | 1293 | `static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1294 |  |
|      - | 1295 | `	const char *zCur,*zIn,*zEnd;` |
|     29 | 1296 | `	int iFlags = 0x01; /* ENT_COMPAT  */` |
|      - | 1297 | `	int nLen;` |
|      - | 1298 | `	sxu32 n;` |
|     29 | 1299 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1300 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 1301 | `		ph7_result_null(pCtx);` |
|      5 | 1302 | `		return PH7_OK;` |
|      - | 1303 | `	}` |
|      - | 1304 | `	/* Extract the target string */` |
|     25 | 1305 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 1306 | `	zEnd = &zIn[nLen];` |
|      - | 1307 | `	/* Extract the flags if available */` |
|     25 | 1308 | `	if( nArg > 1 ){` |
|     15 | 1309 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     15 | 1310 | `		if( iFlags < 0 ){` |
|      3 | 1311 | `			iFlags = 0x01;` |
|      1 | 1312 | `		}` |
|      7 | 1313 | `	}` |
|      - | 1314 | `	/* Perform the requested operation */` |
|     27 | 1315 | `	for(;;){` |
|     55 | 1316 | `		if( zIn >= zEnd ){` |
|      - | 1317 | `			/* No more input to process */` |
|     13 | 1318 | `			break;` |
|      - | 1319 | `		}` |
|     43 | 1320 | `		zCur = zIn;` |
|    173 | 1321 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|    131 | 1322 | `			zIn++;` |
|      1 | 1323 | `		}` |
|     43 | 1324 | `		if( zCur < zIn ){` |
|      - | 1325 | `			/* Append raw string verbatim */` |
|     27 | 1326 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     13 | 1327 | `		}` |
|     43 | 1328 | `		if( zIn >= zEnd ){` |
|     13 | 1329 | `			break;` |
|      - | 1330 | `		}` |
|     31 | 1331 | `		nLen = (int)(zEnd-zIn);` |
|      - | 1332 | `		/* Find an encoded sequence */` |
|    113 | 1333 | `		for(n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    113 | 1334 | `			int iLen = (int)SyStrlen(azHtmlEscape[n]);` |
|    113 | 1335 | `			if( nLen >= iLen && SyStrnicmp(zIn,azHtmlEscape[n],(sxu32)iLen) == 0 ){` |
|      - | 1336 | `				/* Got one */` |
|     31 | 1337 | `				zIn += iLen;` |
|     31 | 1338 | `				break;` |
|      - | 1339 | `			}` |
|     42 | 1340 | `		}` |
|     31 | 1341 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|     31 | 1342 | `			int c = azHtmlEscape[n+1][0];` |
|      - | 1343 | `			/* Output the decoded character */` |
|     31 | 1344 | `			if( c == '\'' && ((iFlags & 0x02) == 0 /*ENT_QUOTES*/\|\| (iFlags & 0x04) /*ENT_NOQUOTES*/)  ){` |
|      - | 1345 | `				/* Do not process single quotes */` |
|      9 | 1346 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|     27 | 1347 | `			}else if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 1348 | `				/* Do not process double quotes */` |
|      5 | 1349 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|      3 | 1350 | `			}else{` |
|     19 | 1351 | `				ph7_result_string(pCtx,azHtmlEscape[n+1],-1); /* Compute length automatically */` |
|      - | 1352 | `			}` |
|     16 | 1353 | `		}else{` |
|      - | 1354 | `			/* Append '&' */` |
|    ! 0 | 1355 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 1356 | `			zIn++;` |
|      - | 1357 | `		}` |
|      1 | 1358 | `	}` |
|     25 | 1359 | `	return PH7_OK;` |
|     15 | 1360 |  |
|      - | 1361 | `/*` |
|      - | 1362 | ` * int strlen($string)` |
|      - | 1363 | ` *  return the length of the given string.` |
|      - | 1364 | ` * Parameter` |
|      - | 1365 | ` *  string: The string being measured for length.` |
|      - | 1366 | ` * Return` |
|      - | 1367 | ` *  length of the given string.` |
|      - | 1368 | ` */` |
|   5166 | 1369 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1370 |  |
|   5168 | 1371 | `	int iLen = 0;` |
|   5168 | 1372 | `	if( nArg > 0 ){` |
|   5166 | 1373 | `		ph7_value_to_string(apArg[0],&iLen);` |
|   2582 | 1374 | `	}` |
|      - | 1375 | `	/* String length */` |
|   5168 | 1376 | `	ph7_result_int(pCtx,iLen);` |
|   5168 | 1377 | `	return PH7_OK;` |
|      2 | 1378 |  |
|      - | 1379 | `/*` |
|      - | 1380 | ` * int strcmp(string $str1,string $str2)` |
|      - | 1381 | ` *  Perform a binary safe string comparison.` |
|      - | 1382 | ` * Parameter` |
|      - | 1383 | ` *  str1: The first string` |
|      - | 1384 | ` *  str2: The second string` |
|      - | 1385 | ` * Return` |
|      - | 1386 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1387 | ` *  than str2, and 0 if they are equal.` |
|      - | 1388 | ` */` |
|     80 | 1389 | `static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1390 |  |
|      - | 1391 | `	const char *z1,*z2;` |
|      - | 1392 | `	int n1,n2;` |
|      - | 1393 | `	int res;` |
|     81 | 1394 | `	if( nArg < 2 ){` |
|      5 | 1395 | `		res = nArg == 0 ? 0 : 1;` |
|      5 | 1396 | `		ph7_result_int(pCtx,res);` |
|      5 | 1397 | `		return PH7_OK;` |
|      - | 1398 | `	}` |
|      - | 1399 | `	/* Perform the comparison */` |
|     77 | 1400 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     77 | 1401 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     77 | 1402 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1403 | `	/* Comparison result */` |
|     77 | 1404 | `	ph7_result_int(pCtx,res);` |
|     77 | 1405 | `	return PH7_OK;` |
|     41 | 1406 |  |
|      - | 1407 | `/*` |
|      - | 1408 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 1409 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 1410 | ` * Parameter` |
|      - | 1411 | ` *  str1: The first string` |
|      - | 1412 | ` *  str2: The second string` |
|      - | 1413 | ` * Return` |
|      - | 1414 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1415 | ` *  than str2, and 0 if they are equal.` |
|      - | 1416 | ` */` |
|     20 | 1417 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1418 |  |
|      - | 1419 | `	const char *z1,*z2;` |
|      - | 1420 | `	int res;` |
|      - | 1421 | `	int n;` |
|     21 | 1422 | `	if( nArg < 3 ){` |
|      - | 1423 | `		/* Perform a standard comparison */` |
|      5 | 1424 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 1425 | `	}` |
|      - | 1426 | `	/* Desired comparison length */` |
|     17 | 1427 | `	n  = ph7_value_to_int(apArg[2]);` |
|     17 | 1428 | `	if( n < 0 ){` |
|      - | 1429 | `		/* Invalid length */` |
|      3 | 1430 | `		ph7_result_int(pCtx,-1);` |
|      3 | 1431 | `		return PH7_OK;` |
|      - | 1432 | `	}` |
|      - | 1433 | `	/* Perform the comparison */` |
|     15 | 1434 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     15 | 1435 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     15 | 1436 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 1437 | `	/* Comparison result */` |
|     15 | 1438 | `	ph7_result_int(pCtx,res);` |
|     15 | 1439 | `	return PH7_OK;` |
|     11 | 1440 |  |
|      - | 1441 | `/*` |
|      - | 1442 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 1443 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 1444 | ` * Parameter` |
|      - | 1445 | ` *  str1: The first string` |
|      - | 1446 | ` *  str2: The second string` |
|      - | 1447 | ` * Return` |
|      - | 1448 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1449 | ` *  than str2, and 0 if they are equal.` |
|      - | 1450 | ` */` |
|     22 | 1451 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1452 |  |
|      - | 1453 | `	const char *z1,*z2;` |
|      - | 1454 | `	int n1,n2;` |
|      - | 1455 | `	int res;` |
|     23 | 1456 | `	if( nArg < 2 ){` |
|      9 | 1457 | `		res = nArg == 0 ? 0 : 1;` |
|      9 | 1458 | `		ph7_result_int(pCtx,res);` |
|      9 | 1459 | `		return PH7_OK;` |
|      - | 1460 | `	}` |
|      - | 1461 | `	/* Perform the comparison */` |
|     15 | 1462 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     15 | 1463 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     15 | 1464 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1465 | `	/* Comparison result */` |
|     15 | 1466 | `	ph7_result_int(pCtx,res);` |
|     15 | 1467 | `	return PH7_OK;` |
|     12 | 1468 |  |
|      - | 1469 | `/*` |
|      - | 1470 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 1471 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 1472 | ` * Parameter` |
|      - | 1473 | ` *  $str1: The first string` |
|      - | 1474 | ` *  $str2: The second string` |
|      - | 1475 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 1476 | ` * Return` |
|      - | 1477 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1478 | ` *  than str2, and 0 if they are equal.` |
|      - | 1479 | ` */` |
|      8 | 1480 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1481 |  |
|      - | 1482 | `	const char *z1,*z2;` |
|      - | 1483 | `	int res;` |
|      - | 1484 | `	int n;` |
|      9 | 1485 | `	if( nArg < 3 ){` |
|      - | 1486 | `		/* Perform a standard comparison */` |
|      5 | 1487 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 1488 | `	}` |
|      - | 1489 | `	/* Desired comparison length */` |
|      5 | 1490 | `	n  = ph7_value_to_int(apArg[2]);` |
|      5 | 1491 | `	if( n < 0 ){` |
|      - | 1492 | `		/* Invalid length */` |
|    ! 0 | 1493 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 1494 | `		return PH7_OK;` |
|      - | 1495 | `	}` |
|      - | 1496 | `	/* Perform the comparison */` |
|      5 | 1497 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|      5 | 1498 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|      5 | 1499 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 1500 | `	/* Comparison result */` |
|      5 | 1501 | `	ph7_result_int(pCtx,res);` |
|      5 | 1502 | `	return PH7_OK;` |
|      5 | 1503 |  |
|      - | 1504 | `/*` |
|      - | 1505 | ` * Implode context [i.e: it's private data].` |
|      - | 1506 | ` * A pointer to the following structure is forwarded` |
|      - | 1507 | ` * verbatim to the array walker callback defined below.` |
|      - | 1508 | ` */` |
|      - | 1509 | `struct implode_data {` |
|      - | 1510 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 1511 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 1512 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 1513 | `	int nSeplen;          /* Separator length */` |
|      - | 1514 | `	int bFirst;           /* TRUE if first call */` |
|      - | 1515 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 1516 | `};` |
|      - | 1517 | `/*` |
|      - | 1518 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 1519 | ` * The following routine is invoked for each array entry passed` |
|      - | 1520 | ` * to the implode() function.` |
|      - | 1521 | ` */` |
| 111288 | 1522 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      2 | 1523 |  |
|  55644 | 1524 | `	SXUNUSED(pKey);` |
| 111290 | 1525 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 1526 | `	const char *zData;` |
|      - | 1527 | `	int nLen;` |
| 111290 | 1528 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 1529 | `		if( pData->nSeplen > 0 ){` |
|      3 | 1530 | `			if( !pData->bFirst ){` |
|      - | 1531 | `				/* append the separator first */` |
|      3 | 1532 | `				ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|      2 | 1533 | `			}else{` |
|    ! 0 | 1534 | `				pData->bFirst = 0;` |
|      - | 1535 | `			}` |
|      1 | 1536 | `		}` |
|      - | 1537 | `		/* Recurse */` |
|      3 | 1538 | `		pData->bFirst = 1;` |
|      3 | 1539 | `		pData->nRecCount++;` |
|      3 | 1540 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 1541 | `		pData->nRecCount--;` |
|      3 | 1542 | `		return PH7_OK;` |
|      - | 1543 | `	}` |
|      - | 1544 | `	/* Extract the string representation of the entry value */` |
| 111288 | 1545 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 1546 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 111288 | 1547 | `	if( pData->bFirst ){` |
|  27676 | 1548 | `		pData->bFirst = 0;` |
|  97451 | 1549 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 1550 | `		/* append the separator first */` |
|  83602 | 1551 | `		ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|  41800 | 1552 | `	}` |
|      - | 1553 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 111288 | 1554 | `	if( nLen > 0 ){` |
| 100988 | 1555 | `		ph7_result_string(pData->pCtx,zData,nLen);` |
|  50493 | 1556 | `	}` |
| 111288 | 1557 | `	return PH7_OK;` |
|  55646 | 1558 |  |
|      - | 1559 | `/*` |
|      - | 1560 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 1561 | ` * string implode(array $pieces,...)` |
|      - | 1562 | ` *  Join array elements with a string.` |
|      - | 1563 | ` * $glue` |
|      - | 1564 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 1565 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 1566 | ` * $pieces` |
|      - | 1567 | ` *   The array of strings to implode.` |
|      - | 1568 | ` * Return` |
|      - | 1569 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 1570 | ` *  order, with the glue string between each element.` |
|      - | 1571 | ` */` |
|  27698 | 1572 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1573 |  |
|      - | 1574 | `	struct implode_data imp_data;` |
|  27700 | 1575 | `	int i = 1;` |
|  27700 | 1576 | `	if( nArg < 1 ){` |
|      - | 1577 | `		/* Missing argument,return NULL */` |
|    ! 0 | 1578 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1579 | `		return PH7_OK;` |
|      - | 1580 | `	}` |
|      - | 1581 | `	/* Prepare the implode context */` |
|  27700 | 1582 | `	imp_data.pCtx = pCtx;` |
|  27700 | 1583 | `	imp_data.bRecursive = 0;` |
|  27700 | 1584 | `	imp_data.bFirst = 1;` |
|  27700 | 1585 | `	imp_data.nRecCount = 0;` |
|  27700 | 1586 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  27698 | 1587 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  13850 | 1588 | `	}else{` |
|      3 | 1589 | `		imp_data.zSep = 0;` |
|      3 | 1590 | `		imp_data.nSeplen = 0;` |
|      3 | 1591 | `		i = 0;` |
|      - | 1592 | `	}` |
|  27700 | 1593 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 1594 | `	/* Start the 'join' process */` |
|  55398 | 1595 | `	while( i < nArg ){` |
|  27700 | 1596 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1597 | `			/* Iterate throw array entries */` |
|  27700 | 1598 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|  13851 | 1599 | `		}else{` |
|      - | 1600 | `			const char *zData;` |
|      - | 1601 | `			int nLen;` |
|      - | 1602 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 1603 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1604 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 1605 | `			if( imp_data.bFirst ){` |
|    ! 0 | 1606 | `				imp_data.bFirst = 0;` |
|    ! 0 | 1607 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1608 | `				ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen);` |
|    ! 0 | 1609 | `			}` |
|      - | 1610 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 1611 | `			if( nLen > 0 ){` |
|    ! 0 | 1612 | `				ph7_result_string(pCtx,zData,nLen);` |
|    ! 0 | 1613 | `			}` |
|      - | 1614 | `		}` |
|  27700 | 1615 | `		i++;` |
|      2 | 1616 | `	}` |
|  27700 | 1617 | `	return PH7_OK;` |
|  13851 | 1618 |  |
|      - | 1619 | `/*` |
|      - | 1620 | ` * Symisc eXtension:` |
|      - | 1621 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 1622 | ` * Purpose` |
|      - | 1623 | ` *  Same as implode() but recurse on arrays.` |
|      - | 1624 | ` * Example:` |
|      - | 1625 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 1626 | ` *   echo implode_recursive("/",$a);` |
|      - | 1627 | ` *   Will output` |
|      - | 1628 | ` *     usr/home/dean.` |
|      - | 1629 | ` *   While the standard implode would produce.` |
|      - | 1630 | ` *    usr/Array.` |
|      - | 1631 | ` * Parameter` |
|      - | 1632 | ` *  Refer to implode().` |
|      - | 1633 | ` * Return` |
|      - | 1634 | ` *  Refer to implode().` |
|      - | 1635 | ` */` |
|     12 | 1636 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1637 |  |
|      - | 1638 | `	struct implode_data imp_data;` |
|     13 | 1639 | `	int i = 1;` |
|     13 | 1640 | `	if( nArg < 1 ){` |
|      - | 1641 | `		/* Missing argument,return NULL */` |
|      3 | 1642 | `		ph7_result_null(pCtx);` |
|      3 | 1643 | `		return PH7_OK;` |
|      - | 1644 | `	}` |
|      - | 1645 | `	/* Prepare the implode context */` |
|     11 | 1646 | `	imp_data.pCtx = pCtx;` |
|     11 | 1647 | `	imp_data.bRecursive = 1;` |
|     11 | 1648 | `	imp_data.bFirst = 1;` |
|     11 | 1649 | `	imp_data.nRecCount = 0;` |
|     11 | 1650 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 1651 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 1652 | `	}else{` |
|    ! 0 | 1653 | `		imp_data.zSep = 0;` |
|    ! 0 | 1654 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 1655 | `		i = 0;` |
|      - | 1656 | `	}` |
|     11 | 1657 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 1658 | `	/* Start the 'join' process */` |
|     21 | 1659 | `	while( i < nArg ){` |
|     11 | 1660 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1661 | `			/* Iterate throw array entries */` |
|      3 | 1662 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      2 | 1663 | `		}else{` |
|      - | 1664 | `			const char *zData;` |
|      - | 1665 | `			int nLen;` |
|      - | 1666 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 1667 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1668 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 1669 | `			if( imp_data.bFirst ){` |
|      9 | 1670 | `				imp_data.bFirst = 0;` |
|      4 | 1671 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1672 | `				ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen);` |
|    ! 0 | 1673 | `			}` |
|      - | 1674 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 1675 | `			if( nLen > 0 ){` |
|      9 | 1676 | `				ph7_result_string(pCtx,zData,nLen);` |
|      4 | 1677 | `			}` |
|      - | 1678 | `		}` |
|     11 | 1679 | `		i++;` |
|      1 | 1680 | `	}` |
|     11 | 1681 | `	return PH7_OK;` |
|      7 | 1682 |  |
|      - | 1683 | `/*` |
|      - | 1684 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 1685 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 1686 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 1687 | ` * Parameters` |
|      - | 1688 | ` *  $delimiter` |
|      - | 1689 | ` *   The boundary string.` |
|      - | 1690 | ` * $string` |
|      - | 1691 | ` *   The input string.` |
|      - | 1692 | ` * $limit` |
|      - | 1693 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 1694 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 1695 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 1696 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 1697 | ` * Returns` |
|      - | 1698 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 1699 | ` *  on boundaries formed by the delimiter.` |
|      - | 1700 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 1701 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 1702 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 1703 | ` *  will be returned.` |
|      - | 1704 | ` * NOTE:` |
|      - | 1705 | ` *  Negative limit is not supported.` |
|      - | 1706 | ` */` |
|   5202 | 1707 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1708 |  |
|      - | 1709 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 1710 | `	int nDelim,nStrlen,iLimit;` |
|      - | 1711 | `	ph7_value *pArray;` |
|      - | 1712 | `	ph7_value *pValue;` |
|      - | 1713 | `	sxu32 nOfft;` |
|      - | 1714 | `	sxi32 rc;` |
|   5204 | 1715 | `	if( nArg < 2 ){` |
|      - | 1716 | `		/* Missing arguments,return FALSE */` |
|      9 | 1717 | `		ph7_result_bool(pCtx,0);` |
|      9 | 1718 | `		return PH7_OK;` |
|      - | 1719 | `	}` |
|      - | 1720 | `	/* Extract the delimiter */` |
|   5196 | 1721 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   5196 | 1722 | `	if( nDelim < 1 ){` |
|      - | 1723 | `		/* Empty delimiter,return FALSE */` |
|      3 | 1724 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1725 | `		return PH7_OK;` |
|      - | 1726 | `	}` |
|      - | 1727 | `	/* Extract the string */` |
|   5194 | 1728 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   5194 | 1729 | `	if( nStrlen < 1 ){` |
|      - | 1730 | `		/* Empty string: return an array with a single empty element (PHP behavior) */` |
|      3 | 1731 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      3 | 1732 | `		ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      3 | 1733 | `		if( pArrayTmp == 0 \|\| pValueTmp == 0 ){` |
|      - | 1734 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 1735 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 1736 | `			return PH7_OK;` |
|      - | 1737 | `		}` |
|      3 | 1738 | `		ph7_value_string(pValueTmp, "", 0);` |
|      3 | 1739 | `		ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp);` |
|      3 | 1740 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      3 | 1741 | `		return PH7_OK;` |
|      - | 1742 | `	}` |
|      - | 1743 | `	/* Point to the end of the string */` |
|   5192 | 1744 | `	zEnd = &zString[nStrlen];` |
|      - | 1745 | `	/* Create the array */` |
|   5192 | 1746 | `	pArray =  ph7_context_new_array(pCtx);` |
|   5192 | 1747 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   5192 | 1748 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 1749 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 1750 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1751 | `		return PH7_OK;` |
|      - | 1752 | `	}` |
|      - | 1753 | `	/* Set a defualt limit */` |
|   5192 | 1754 | `	iLimit = SXI32_HIGH;` |
|   5192 | 1755 | `	if( nArg > 2 ){` |
|     11 | 1756 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     11 | 1757 | `		 if( iLimit < 0 ){` |
|      3 | 1758 | `			iLimit = -iLimit;` |
|      1 | 1759 | `		}` |
|     11 | 1760 | `		if( iLimit == 0 ){` |
|      3 | 1761 | `			iLimit = 1;` |
|      1 | 1762 | `		}` |
|     11 | 1763 | `		iLimit--;` |
|      5 | 1764 | `	}` |
|      - | 1765 | `	/* Start exploding */` |
|  59033 | 1766 | `	for(;;){` |
| 118068 | 1767 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 118068 | 1768 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 1769 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   5192 | 1770 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   5192 | 1771 | `			ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|   5192 | 1772 | `			break;` |
|      - | 1773 | `		}` |
|      - | 1774 | `		/* Point to the desired offset */` |
| 112878 | 1775 | `		zCur = &zString[nOfft];` |
|      - | 1776 | `		/* Perform the store operation (may be empty) */` |
| 112878 | 1777 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 112878 | 1778 | `		ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|      - | 1779 | `		/* Point beyond the delimiter */` |
| 112878 | 1780 | `		zString = &zCur[nDelim];` |
|      - | 1781 | `		/* Reset the cursor */` |
| 112878 | 1782 | `		ph7_value_reset_string_cursor(pValue);` |
|      2 | 1783 | `	}` |
|      - | 1784 | `	/* Return the freshly created array */` |
|   5192 | 1785 | `	ph7_result_value(pCtx,pArray);` |
|      - | 1786 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 1787 | `	 * released as soon we return from this foregin function.` |
|      - | 1788 | `	 */` |
|   5192 | 1789 | `	return PH7_OK;` |
|   2603 | 1790 |  |
|      - | 1791 | `/*` |
|      - | 1792 | ` * string trim(string $str[,string $charlist ])` |
|      - | 1793 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 1794 | ` * Parameters` |
|      - | 1795 | ` *  $str` |
|      - | 1796 | ` *   The string that will be trimmed.` |
|      - | 1797 | ` * $charlist` |
|      - | 1798 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1799 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1800 | ` *   With .. you can specify a range of characters.` |
|      - | 1801 | ` * Returns.` |
|      - | 1802 | ` *  Thr processed string.` |
|      - | 1803 | ` * NOTE:` |
|      - | 1804 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 1805 | ` */` |
|  12006 | 1806 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1807 |  |
|      - | 1808 | `	const char *zString;` |
|      - | 1809 | `	int nLen;` |
|  12008 | 1810 | `	if( nArg < 1 ){` |
|      - | 1811 | `		/* Missing arguments,return null */` |
|      3 | 1812 | `		ph7_result_null(pCtx);` |
|      3 | 1813 | `		return PH7_OK;` |
|      - | 1814 | `	}` |
|      - | 1815 | `	/* Extract the target string */` |
|  12006 | 1816 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  12006 | 1817 | `	if( nLen < 1 ){` |
|      - | 1818 | `		/* Empty string,return */` |
|   1614 | 1819 | `		ph7_result_string(pCtx,"",0);` |
|   1614 | 1820 | `		return PH7_OK;` |
|      - | 1821 | `	}` |
|      - | 1822 | `	/* Start the trim process */` |
|  10394 | 1823 | `	if( nArg < 2 ){` |
|      - | 1824 | `		SyString sStr;` |
|      - | 1825 | `		/* Remove white spaces and NUL bytes */` |
|  10390 | 1826 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  25266 | 1827 | `		SyStringFullTrimSafe(&sStr);` |
|  10390 | 1828 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   5196 | 1829 | `	}else{` |
|      - | 1830 | `		/* Char list */` |
|      - | 1831 | `		const char *zList;` |
|      - | 1832 | `		int nListlen;` |
|      5 | 1833 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 1834 | `		if( nListlen < 1 ){` |
|      - | 1835 | `			/* Return the string unchanged */` |
|      3 | 1836 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 1837 | `		}else{` |
|      3 | 1838 | `			const char *zEnd = &zString[nLen];` |
|      3 | 1839 | `			const char *zCur = zString;` |
|      - | 1840 | `			const char *zPtr;` |
|      - | 1841 | `			int i;` |
|      - | 1842 | `			/* Left trim */` |
|      4 | 1843 | `			for(;;){` |
|      9 | 1844 | `				if( zCur >= zEnd ){` |
|    ! 0 | 1845 | `					break;` |
|      - | 1846 | `				}` |
|      9 | 1847 | `				zPtr = zCur;` |
|     17 | 1848 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 1849 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|      7 | 1850 | `						zCur++;` |
|      3 | 1851 | `					}` |
|      5 | 1852 | `				}` |
|      9 | 1853 | `				if( zCur == zPtr ){` |
|      - | 1854 | `					/* No match,break immediately */` |
|      3 | 1855 | `					break;` |
|      - | 1856 | `				}` |
|      1 | 1857 | `			}` |
|      - | 1858 | `			/* Right trim */` |
|      3 | 1859 | `			zEnd--;` |
|      4 | 1860 | `			for(;;){` |
|      9 | 1861 | `				if( zEnd <= zCur ){` |
|    ! 0 | 1862 | `					break;` |
|      - | 1863 | `				}` |
|      9 | 1864 | `				zPtr = zEnd;` |
|     17 | 1865 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 1866 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      7 | 1867 | `						zEnd--;` |
|      3 | 1868 | `					}` |
|      5 | 1869 | `				}` |
|      9 | 1870 | `				if( zEnd == zPtr ){` |
|      3 | 1871 | `					break;` |
|      - | 1872 | `				}` |
|      1 | 1873 | `			}` |
|      3 | 1874 | `			if( zCur >= zEnd ){` |
|      - | 1875 | `				/* Return the empty string */` |
|    ! 0 | 1876 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1877 | `			}else{` |
|      3 | 1878 | `				zEnd++;` |
|      3 | 1879 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 1880 | `			}` |
|      - | 1881 | `		}` |
|      - | 1882 | `	}` |
|  10394 | 1883 | `	return PH7_OK;` |
|   6005 | 1884 |  |
|      - | 1885 | `/*` |
|      - | 1886 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 1887 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 1888 | ` * Parameters` |
|      - | 1889 | ` *  $str` |
|      - | 1890 | ` *   The string that will be trimmed.` |
|      - | 1891 | ` * $charlist` |
|      - | 1892 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1893 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1894 | ` *   With .. you can specify a range of characters.` |
|      - | 1895 | ` * Returns.` |
|      - | 1896 | ` *  Thr processed string.` |
|      - | 1897 | ` * NOTE:` |
|      - | 1898 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 1899 | ` */` |
|     26 | 1900 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1901 |  |
|      - | 1902 | `	const char *zString;` |
|      - | 1903 | `	int nLen;` |
|     27 | 1904 | `	if( nArg < 1 ){` |
|      - | 1905 | `		/* Missing arguments,return null */` |
|      3 | 1906 | `		ph7_result_null(pCtx);` |
|      3 | 1907 | `		return PH7_OK;` |
|      - | 1908 | `	}` |
|      - | 1909 | `	/* Extract the target string */` |
|     25 | 1910 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 1911 | `	if( nLen < 1 ){` |
|      - | 1912 | `		/* Empty string,return */` |
|      5 | 1913 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1914 | `		return PH7_OK;` |
|      - | 1915 | `	}` |
|      - | 1916 | `	/* Start the trim process */` |
|     21 | 1917 | `	if( nArg < 2 ){` |
|      - | 1918 | `		SyString sStr;` |
|      - | 1919 | `		/* Remove white spaces and NUL bytes*/` |
|     17 | 1920 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     41 | 1921 | `		SyStringRightTrimSafe(&sStr);` |
|     17 | 1922 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      9 | 1923 | `	}else{` |
|      - | 1924 | `		/* Char list */` |
|      - | 1925 | `		const char *zList;` |
|      - | 1926 | `		int nListlen;` |
|      5 | 1927 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 1928 | `		if( nListlen < 1 ){` |
|      - | 1929 | `			/* Return the string unchanged */` |
|    ! 0 | 1930 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 1931 | `		}else{` |
|      5 | 1932 | `			const char *zEnd = &zString[nLen - 1];` |
|      5 | 1933 | `			const char *zCur = zString;` |
|      - | 1934 | `			const char *zPtr;` |
|      - | 1935 | `			int i;` |
|      - | 1936 | `			/* Right trim */` |
|      6 | 1937 | `			for(;;){` |
|     13 | 1938 | `				if( zEnd <= zCur ){` |
|    ! 0 | 1939 | `					break;` |
|      - | 1940 | `				}` |
|     13 | 1941 | `				zPtr = zEnd;` |
|     25 | 1942 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     13 | 1943 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      9 | 1944 | `						zEnd--;` |
|      4 | 1945 | `					}` |
|      7 | 1946 | `				}` |
|     13 | 1947 | `				if( zEnd == zPtr ){` |
|      5 | 1948 | `					break;` |
|      - | 1949 | `				}` |
|      1 | 1950 | `			}` |
|      5 | 1951 | `			if( zEnd <= zCur ){` |
|      - | 1952 | `				/* Return the empty string */` |
|    ! 0 | 1953 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1954 | `			}else{` |
|      5 | 1955 | `				zEnd++;` |
|      5 | 1956 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 1957 | `			}` |
|      - | 1958 | `		}` |
|      - | 1959 | `	}` |
|     21 | 1960 | `	return PH7_OK;` |
|     14 | 1961 |  |
|      - | 1962 | `/*` |
|      - | 1963 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 1964 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 1965 | ` * Parameters` |
|      - | 1966 | ` *  $str` |
|      - | 1967 | ` *   The string that will be trimmed.` |
|      - | 1968 | ` * $charlist` |
|      - | 1969 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1970 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1971 | ` *   With .. you can specify a range of characters.` |
|      - | 1972 | ` * Returns.` |
|      - | 1973 | ` *  Thr processed string.` |
|      - | 1974 | ` * NOTE:` |
|      - | 1975 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 1976 | ` */` |
|     12 | 1977 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1978 |  |
|      - | 1979 | `	const char *zString;` |
|      - | 1980 | `	int nLen;` |
|     13 | 1981 | `	if( nArg < 1 ){` |
|      - | 1982 | `		/* Missing arguments,return null */` |
|      3 | 1983 | `		ph7_result_null(pCtx);` |
|      3 | 1984 | `		return PH7_OK;` |
|      - | 1985 | `	}` |
|      - | 1986 | `	/* Extract the target string */` |
|     11 | 1987 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 1988 | `	if( nLen < 1 ){` |
|      - | 1989 | `		/* Empty string,return */` |
|    ! 0 | 1990 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1991 | `		return PH7_OK;` |
|      - | 1992 | `	}` |
|      - | 1993 | `	/* Start the trim process */` |
|     11 | 1994 | `	if( nArg < 2 ){` |
|      - | 1995 | `		SyString sStr;` |
|      - | 1996 | `		/* Remove white spaces and NUL byte */` |
|      3 | 1997 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|      8 | 1998 | `		SyStringLeftTrimSafe(&sStr);` |
|      3 | 1999 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      2 | 2000 | `	}else{` |
|      - | 2001 | `		/* Char list */` |
|      - | 2002 | `		const char *zList;` |
|      - | 2003 | `		int nListlen;` |
|      9 | 2004 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      9 | 2005 | `		if( nListlen < 1 ){` |
|      - | 2006 | `			/* Return the string unchanged */` |
|      3 | 2007 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2008 | `		}else{` |
|      7 | 2009 | `			const char *zEnd = &zString[nLen];` |
|      7 | 2010 | `			const char *zCur = zString;` |
|      - | 2011 | `			const char *zPtr;` |
|      - | 2012 | `			int i;` |
|      - | 2013 | `			/* Left trim */` |
|      7 | 2014 | `			for(;;){` |
|     15 | 2015 | `				if( zCur >= zEnd ){` |
|    ! 0 | 2016 | `					break;` |
|      - | 2017 | `				}` |
|     15 | 2018 | `				zPtr = zCur;` |
|     41 | 2019 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     27 | 2020 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|     13 | 2021 | `						zCur++;` |
|      6 | 2022 | `					}` |
|     14 | 2023 | `				}` |
|     15 | 2024 | `				if( zCur == zPtr ){` |
|      - | 2025 | `					/* No match,break immediately */` |
|      7 | 2026 | `					break;` |
|      - | 2027 | `				}` |
|      1 | 2028 | `			}` |
|      7 | 2029 | `			if( zCur >= zEnd ){` |
|      - | 2030 | `				/* Return the empty string */` |
|    ! 0 | 2031 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2032 | `			}else{` |
|      7 | 2033 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2034 | `			}` |
|      - | 2035 | `		}` |
|      - | 2036 | `	}` |
|     11 | 2037 | `	return PH7_OK;` |
|      7 | 2038 |  |
|      - | 2039 | `/*` |
|      - | 2040 | ` * string strtolower(string $str)` |
|      - | 2041 | ` *  Make a string lowercase.` |
|      - | 2042 | ` * Parameters` |
|      - | 2043 | ` *  $str` |
|      - | 2044 | ` *   The input string.` |
|      - | 2045 | ` * Returns.` |
|      - | 2046 | ` *  The lowercased string.` |
|      - | 2047 | ` */` |
|  27418 | 2048 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2049 |  |
|      - | 2050 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2051 | `	int nLen;` |
|  27420 | 2052 | `	if( nArg < 1 ){` |
|      - | 2053 | `		/* Missing arguments,return null */` |
|      3 | 2054 | `		ph7_result_null(pCtx);` |
|      3 | 2055 | `		return PH7_OK;` |
|      - | 2056 | `	}` |
|      - | 2057 | `	/* Extract the target string */` |
|  27418 | 2058 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  27418 | 2059 | `	if( nLen < 1 ){` |
|      - | 2060 | `		/* Empty string,return */` |
|      3 | 2061 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2062 | `		return PH7_OK;` |
|      - | 2063 | `	}` |
|      - | 2064 | `	/* Perform the requested operation */` |
|  27416 | 2065 | `	zEnd = &zString[nLen];` |
|  86378 | 2066 | `	for(;;){` |
| 172758 | 2067 | `		if( zString >= zEnd ){` |
|      - | 2068 | `			/* No more input,break immediately */` |
|  27416 | 2069 | `			break;` |
|      - | 2070 | `		}` |
| 145344 | 2071 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2072 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2073 | `			zCur = zString;` |
|    ! 0 | 2074 | `			zString++;` |
|    ! 0 | 2075 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2076 | `				zString++;` |
|    ! 0 | 2077 | `			}` |
|      - | 2078 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2079 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2080 | `		}else{` |
| 145344 | 2081 | `			int c = zString[0];` |
| 145344 | 2082 | `			if( SyisUpper(c) ){` |
| 145342 | 2083 | `				c = SyToLower(zString[0]);` |
|  72670 | 2084 | `			}` |
|      - | 2085 | `			/* Append character */` |
| 145344 | 2086 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2087 | `			/* Advance the cursor */` |
| 145344 | 2088 | `			zString++;` |
|      - | 2089 | `		}` |
|      2 | 2090 | `	}` |
|  27416 | 2091 | `	return PH7_OK;` |
|  13711 | 2092 |  |
|      - | 2093 | `/*` |
|      - | 2094 | ` * string strtolower(string $str)` |
|      - | 2095 | ` *  Make a string uppercase.` |
|      - | 2096 | ` * Parameters` |
|      - | 2097 | ` *  $str` |
|      - | 2098 | ` *   The input string.` |
|      - | 2099 | ` * Returns.` |
|      - | 2100 | ` *  The uppercased string.` |
|      - | 2101 | ` */` |
|     30 | 2102 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2103 |  |
|      - | 2104 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2105 | `	int nLen;` |
|     32 | 2106 | `	if( nArg < 1 ){` |
|      - | 2107 | `		/* Missing arguments,return null */` |
|      3 | 2108 | `		ph7_result_null(pCtx);` |
|      3 | 2109 | `		return PH7_OK;` |
|      - | 2110 | `	}` |
|      - | 2111 | `	/* Extract the target string */` |
|     30 | 2112 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     30 | 2113 | `	if( nLen < 1 ){` |
|      - | 2114 | `		/* Empty string,return */` |
|      3 | 2115 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2116 | `		return PH7_OK;` |
|      - | 2117 | `	}` |
|      - | 2118 | `	/* Perform the requested operation */` |
|     28 | 2119 | `	zEnd = &zString[nLen];` |
|     76 | 2120 | `	for(;;){` |
|    154 | 2121 | `		if( zString >= zEnd ){` |
|      - | 2122 | `			/* No more input,break immediately */` |
|     28 | 2123 | `			break;` |
|      - | 2124 | `		}` |
|    128 | 2125 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2126 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2127 | `			zCur = zString;` |
|    ! 0 | 2128 | `			zString++;` |
|    ! 0 | 2129 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2130 | `				zString++;` |
|    ! 0 | 2131 | `			}` |
|      - | 2132 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2133 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2134 | `		}else{` |
|    128 | 2135 | `			int c = zString[0];` |
|    128 | 2136 | `			if( SyisLower(c) ){` |
|    122 | 2137 | `				c = SyToUpper(zString[0]);` |
|     60 | 2138 | `			}` |
|      - | 2139 | `			/* Append character */` |
|    128 | 2140 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2141 | `			/* Advance the cursor */` |
|    128 | 2142 | `			zString++;` |
|      - | 2143 | `		}` |
|      2 | 2144 | `	}` |
|     28 | 2145 | `	return PH7_OK;` |
|     17 | 2146 |  |
|      - | 2147 | `/*` |
|      - | 2148 | ` * string ucfirst(string $str)` |
|      - | 2149 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2150 | ` *  character is alphabetic.` |
|      - | 2151 | ` * Parameters` |
|      - | 2152 | ` *  $str` |
|      - | 2153 | ` *   The input string.` |
|      - | 2154 | ` * Returns.` |
|      - | 2155 | ` *  The processed string.` |
|      - | 2156 | ` */` |
|      6 | 2157 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2158 |  |
|      - | 2159 | `	const char *zString,*zEnd;` |
|      - | 2160 | `	int nLen,c;` |
|      7 | 2161 | `	if( nArg < 1 ){` |
|      - | 2162 | `		/* Missing arguments,return null */` |
|      3 | 2163 | `		ph7_result_null(pCtx);` |
|      3 | 2164 | `		return PH7_OK;` |
|      - | 2165 | `	}` |
|      - | 2166 | `	/* Extract the target string */` |
|      5 | 2167 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2168 | `	if( nLen < 1 ){` |
|      - | 2169 | `		/* Empty string,return */` |
|      3 | 2170 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2171 | `		return PH7_OK;` |
|      - | 2172 | `	}` |
|      - | 2173 | `	/* Perform the requested operation */` |
|      3 | 2174 | `	zEnd = &zString[nLen];` |
|      3 | 2175 | `	c = zString[0];` |
|      3 | 2176 | `	if( SyisLower(c) ){` |
|      3 | 2177 | `		c = SyToUpper(c);` |
|      1 | 2178 | `	}` |
|      - | 2179 | `	/* Append the first character */` |
|      3 | 2180 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2181 | `	zString++;` |
|      3 | 2182 | `	if( zString < zEnd ){` |
|      - | 2183 | `		/* Append the rest of the input verbatim */` |
|      3 | 2184 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2185 | `	}` |
|      3 | 2186 | `	return PH7_OK;` |
|      4 | 2187 |  |
|      - | 2188 | `/*` |
|      - | 2189 | ` * string lcfirst(string $str)` |
|      - | 2190 | ` *  Make a string's first character lowercase.` |
|      - | 2191 | ` * Parameters` |
|      - | 2192 | ` *  $str` |
|      - | 2193 | ` *   The input string.` |
|      - | 2194 | ` * Returns.` |
|      - | 2195 | ` *  The processed string.` |
|      - | 2196 | ` */` |
|      6 | 2197 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2198 |  |
|      - | 2199 | `	const char *zString,*zEnd;` |
|      - | 2200 | `	int nLen,c;` |
|      7 | 2201 | `	if( nArg < 1 ){` |
|      - | 2202 | `		/* Missing arguments,return null */` |
|      3 | 2203 | `		ph7_result_null(pCtx);` |
|      3 | 2204 | `		return PH7_OK;` |
|      - | 2205 | `	}` |
|      - | 2206 | `	/* Extract the target string */` |
|      5 | 2207 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2208 | `	if( nLen < 1 ){` |
|      - | 2209 | `		/* Empty string,return */` |
|      3 | 2210 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2211 | `		return PH7_OK;` |
|      - | 2212 | `	}` |
|      - | 2213 | `	/* Perform the requested operation */` |
|      3 | 2214 | `	zEnd = &zString[nLen];` |
|      3 | 2215 | `	c = zString[0];` |
|      3 | 2216 | `	if( SyisUpper(c) ){` |
|      3 | 2217 | `		c = SyToLower(c);` |
|      1 | 2218 | `	}` |
|      - | 2219 | `	/* Append the first character */` |
|      3 | 2220 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2221 | `	zString++;` |
|      3 | 2222 | `	if( zString < zEnd ){` |
|      - | 2223 | `		/* Append the rest of the input verbatim */` |
|      3 | 2224 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2225 | `	}` |
|      3 | 2226 | `	return PH7_OK;` |
|      4 | 2227 |  |
|      - | 2228 | `/*` |
|      - | 2229 | ` * int ord(string $string)` |
|      - | 2230 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 2231 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 2232 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 2233 | ` * Parameters` |
|      - | 2234 | ` *  $string` |
|      - | 2235 | ` *   The input string.` |
|      - | 2236 | ` * Returns` |
|      - | 2237 | ` *  The ASCII value as an integer.` |
|      - | 2238 | ` */` |
|     62 | 2239 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2240 |  |
|      - | 2241 | `	const char *zString;` |
|      - | 2242 | `	int nLen,c;` |
|      - | 2243 | `	/* PHP requires exactly one argument. */` |
|     64 | 2244 | `	if( nArg != 1 ){` |
|      7 | 2245 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2246 | `			"ArgumentCountError",` |
|      - | 2247 | `			"ord() expects exactly 1 argument, %d given",` |
|      2 | 2248 | `			nArg` |
|      - | 2249 | `			);` |
|      - | 2250 | `	}` |
|      - | 2251 | `	/* Passing null is deprecated (E_DEPRECATED).  PHP emits this before` |
|      - | 2252 | `	 * the empty-string deprecation, so we check null first. */` |
|     59 | 2253 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 2254 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2255 | `			"ord(): Passing null to parameter #1 ($character) "` |
|      - | 2256 | `			"of type string is deprecated"` |
|      - | 2257 | `			);` |
|      1 | 2258 | `	}` |
|      - | 2259 | `	/* Extract the target string */` |
|     59 | 2260 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     59 | 2261 | `	if( nLen < 1 ){` |
|      - | 2262 | `		/* Empty string is deprecated (E_DEPRECATED). */` |
|      5 | 2263 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2264 | `			"ord(): Providing an empty string is deprecated"` |
|      - | 2265 | `			);` |
|      5 | 2266 | `		ph7_result_int(pCtx,0);` |
|      5 | 2267 | `		return PH7_OK;` |
|      - | 2268 | `	}` |
|      - | 2269 | `	/* A string longer than one byte is deprecated (E_DEPRECATED). */` |
|     55 | 2270 | `	if( nLen > 1 ){` |
|      7 | 2271 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2272 | `			"ord(): Providing a string that is not one byte long "` |
|      - | 2273 | `			"is deprecated. Use ord($str[0]) instead"` |
|      - | 2274 | `			);` |
|      3 | 2275 | `	}` |
|      - | 2276 | `	/* Extract the ASCII value of the first character */` |
|     55 | 2277 | `	c = (unsigned char)zString[0];` |
|      - | 2278 | `	/* Return that value */` |
|     55 | 2279 | `	ph7_result_int(pCtx,c);` |
|     55 | 2280 | `	return PH7_OK;` |
|     33 | 2281 |  |
|      - | 2282 | `/*` |
|      - | 2283 | ` * string chr(int $codepoint)` |
|      - | 2284 | ` *  Returns a one-character string containing the character specified` |
|      - | 2285 | ` *  by the given codepoint.  Any integer is accepted; values outside` |
|      - | 2286 | ` *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.` |
|      - | 2287 | ` * Parameters` |
|      - | 2288 | ` *  $codepoint` |
|      - | 2289 | ` *   An integer codepoint.  Values outside 0-255 are deprecated and` |
|      - | 2290 | ` *   will be constrained to a single byte.` |
|      - | 2291 | ` * Returns` |
|      - | 2292 | ` *  A single-character string.` |
|      - | 2293 | ` */` |
|     44 | 2294 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2295 |  |
|      - | 2296 | `	int c;` |
|      - | 2297 | `	unsigned char ch;` |
|      - | 2298 | `	/* PHP requires exactly one argument. */` |
|     46 | 2299 | `	if( nArg != 1 ){` |
|      7 | 2300 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2301 | `			"ArgumentCountError",` |
|      - | 2302 | `			"chr() expects exactly 1 argument, %d given",` |
|      2 | 2303 | `			nArg` |
|      - | 2304 | `			);` |
|      - | 2305 | `	}` |
|      - | 2306 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 2307 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 2308 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 2309 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|     41 | 2310 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      - | 2311 | `		char zBuf[120];` |
|      4 | 2312 | `		SyBufferFormat(zBuf,sizeof(zBuf),` |
|      - | 2313 | `			"Implicit conversion from float %g to int loses precision",` |
|      1 | 2314 | `			ph7_value_to_double(apArg[0])` |
|      - | 2315 | `			);` |
|      3 | 2316 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zBuf);` |
|      1 | 2317 | `	}` |
|      - | 2318 | `	/* Extract the codepoint. */` |
|     41 | 2319 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 2320 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 2321 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 2322 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 2323 | `	 * name to avoid the API double-prefixing it. */` |
|     41 | 2324 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 2325 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 2326 | `			E_DEPRECATED,` |
|      - | 2327 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 2328 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 2329 | `			"The value used will be constrained using % 256"` |
|      - | 2330 | `			);` |
|      2 | 2331 | `	}` |
|      - | 2332 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 2333 | `	 * when taking the address of a wider int. */` |
|     41 | 2334 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 2335 | `	/* Return the specified character */` |
|     41 | 2336 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|     41 | 2337 | `	return PH7_OK;` |
|     24 | 2338 |  |
|      - | 2339 | `/*` |
|      - | 2340 | ` * Binary to hex consumer callback.` |
|      - | 2341 | ` * This callback is the default consumer used by the hash functions` |
|      - | 2342 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 2343 | ` */` |
|    226 | 2344 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 2345 |  |
|      - | 2346 | `	/* Append hex chunk verbatim */` |
|    227 | 2347 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|    227 | 2348 | `	return SXRET_OK;` |
|      1 | 2349 |  |
|      - | 2350 |  |
|      - | 2351 | `/*` |
|      - | 2352 | ` * string bin2hex(string $str)` |
|      - | 2353 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 2354 | ` * Parameters` |
|      - | 2355 | ` *  $str` |
|      - | 2356 | ` *   The input string.` |
|      - | 2357 | ` * Returns.` |
|      - | 2358 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 2359 | ` */` |
|     20 | 2360 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2361 |  |
|      - | 2362 | `	const char *zString;` |
|      - | 2363 | `	int nLen;` |
|      - | 2364 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|     22 | 2365 | `	if( nArg != 1 ){` |
|      7 | 2366 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2367 | `			"ArgumentCountError",` |
|      - | 2368 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|      2 | 2369 | `			nArg` |
|      - | 2370 | `			);` |
|      - | 2371 | `	}` |
|      - | 2372 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 2373 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 2374 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 2375 | `	 */` |
|     25 | 2376 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|     12 | 2377 | `		( ph7_value_is_object(apArg[0]) &&` |
|      3 | 2378 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|      2 | 2379 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|      1 | 2380 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 2381 | `		)` |
|      - | 2382 | `	){` |
|      7 | 2383 | `		const char *zType = ph7_type_name(apArg[0]);` |
|      7 | 2384 | `		if( ph7_value_is_object(apArg[0]) ){` |
|      3 | 2385 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      3 | 2386 | `			if( pInst && pInst->pClass ){` |
|      3 | 2387 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      1 | 2388 | `			}` |
|      1 | 2389 | `		}` |
|     10 | 2390 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2391 | `			"TypeError",` |
|      - | 2392 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|      3 | 2393 | `			zType` |
|      - | 2394 | `			);` |
|      - | 2395 | `	}` |
|      - | 2396 | `	/* Extract the target string */` |
|     11 | 2397 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 2398 | `	if( nLen < 1 ){` |
|      - | 2399 | `		/* Empty string,return */` |
|      3 | 2400 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2401 | `		return PH7_OK;` |
|      - | 2402 | `	}` |
|      - | 2403 | `	/* Perform the requested operation */` |
|      9 | 2404 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|      9 | 2405 | `	return PH7_OK;` |
|     12 | 2406 |  |
|      - | 2407 |  |
|      - | 2408 | `/* Search callback signature */` |
|      - | 2409 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 2410 | `/*` |
|      - | 2411 | ` * Case-insensitive pattern match.` |
|      - | 2412 | ` * Brute force is the default search method used here.` |
|      - | 2413 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 2414 | ` * well for short/medium texts on modern hardware.` |
|      - | 2415 | ` */` |
|    118 | 2416 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      1 | 2417 |  |
|    119 | 2418 | `	const char *zpIn = (const char *)pPattern;` |
|    119 | 2419 | `	const char *zIn = (const char *)pText;` |
|    119 | 2420 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    119 | 2421 | `	const char *zEnd = &zIn[nLen];` |
|      - | 2422 | `	const char *zPtr,*zPtr2;` |
|      - | 2423 | `	int c,d;` |
|    119 | 2424 | `	if( iPatLen > nLen ){` |
|      - | 2425 | `		/* Don't bother processing */` |
|     33 | 2426 | `		return SXERR_NOTFOUND;` |
|      - | 2427 | `	}` |
|    244 | 2428 | `	for(;;){` |
|    489 | 2429 | `		if( zIn >= zEnd ){` |
|     47 | 2430 | `			break;` |
|      - | 2431 | `		}` |
|    443 | 2432 | `		c = SyToLower(zIn[0]);` |
|    443 | 2433 | `		d = SyToLower(zpIn[0]);` |
|    443 | 2434 | `		if( c == d ){` |
|     41 | 2435 | `			zPtr   = &zIn[1];` |
|     41 | 2436 | `			zPtr2  = &zpIn[1];` |
|     71 | 2437 | `			for(;;){` |
|    143 | 2438 | `				if( zPtr2 >= zpEnd ){` |
|      - | 2439 | `					/* Pattern found */` |
|     41 | 2440 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 2441 | `					return SXRET_OK;` |
|      - | 2442 | `				}` |
|    103 | 2443 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 2444 | `					break;` |
|      - | 2445 | `				}` |
|    103 | 2446 | `				c = SyToLower(zPtr[0]);` |
|    103 | 2447 | `				d = SyToLower(zPtr2[0]);` |
|    103 | 2448 | `				if( c != d ){` |
|    ! 0 | 2449 | `					break;` |
|      - | 2450 | `				}` |
|    103 | 2451 | `				zPtr++; zPtr2++;` |
|      1 | 2452 | `			}` |
|    ! 0 | 2453 | `		}` |
|    403 | 2454 | `		zIn++;` |
|      1 | 2455 | `	}` |
|      - | 2456 | `	/* Pattern not found */` |
|     47 | 2457 | `	return SXERR_NOTFOUND;` |
|     60 | 2458 |  |
|      - | 2459 | `/*` |
|      - | 2460 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2461 | ` *  Find the first occurrence of a string.` |
|      - | 2462 | ` * Parameters` |
|      - | 2463 | ` *  $haystack` |
|      - | 2464 | ` *   The input string.` |
|      - | 2465 | ` * $needle` |
|      - | 2466 | ` *   Search pattern (must be a string).` |
|      - | 2467 | ` * $before_needle` |
|      - | 2468 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2469 | ` *   of the needle (excluding the needle).` |
|      - | 2470 | ` * Return` |
|      - | 2471 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2472 | ` */` |
|     10 | 2473 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2474 |  |
|     11 | 2475 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2476 | `	const char *zBlob,*zPattern;` |
|      - | 2477 | `	int nLen,nPatLen;` |
|      - | 2478 | `	sxu32 nOfft;` |
|      - | 2479 | `	sxi32 rc;` |
|     11 | 2480 | `	if( nArg < 2 ){` |
|      - | 2481 | `		/* Missing arguments,return FALSE */` |
|      5 | 2482 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2483 | `		return PH7_OK;` |
|      - | 2484 | `	}` |
|      - | 2485 | `	/* Extract the needle and the haystack */` |
|      7 | 2486 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 2487 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 2488 | `	nOfft = 0; /* cc warning */` |
|      9 | 2489 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 2490 | `		int before = 0;` |
|      - | 2491 | `		/* Perform the lookup */` |
|      5 | 2492 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2493 | `		if( rc != SXRET_OK ){` |
|      - | 2494 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2495 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2496 | `			return PH7_OK;` |
|      - | 2497 | `		}` |
|      - | 2498 | `		/* Return the portion of the string */` |
|      5 | 2499 | `		if( nArg > 2 ){` |
|      3 | 2500 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2501 | `		}` |
|      5 | 2502 | `		if( before ){` |
|      3 | 2503 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2504 | `		}else{` |
|      3 | 2505 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2506 | `		}` |
|      3 | 2507 | `	}else{` |
|      3 | 2508 | `		ph7_result_bool(pCtx,0);` |
|      - | 2509 | `	}` |
|      7 | 2510 | `	return PH7_OK;` |
|      6 | 2511 |  |
|      - | 2512 | `/*` |
|      - | 2513 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2514 | ` *  Case-insensitive strstr().` |
|      - | 2515 | ` * Parameters` |
|      - | 2516 | ` *  $haystack` |
|      - | 2517 | ` *   The input string.` |
|      - | 2518 | ` * $needle` |
|      - | 2519 | ` *   Search pattern (must be a string).` |
|      - | 2520 | ` * $before_needle` |
|      - | 2521 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2522 | ` *   of the needle (excluding the needle).` |
|      - | 2523 | ` * Return` |
|      - | 2524 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2525 | ` */` |
|      6 | 2526 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2527 |  |
|      7 | 2528 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2529 | `	const char *zBlob,*zPattern;` |
|      - | 2530 | `	int nLen,nPatLen;` |
|      - | 2531 | `	sxu32 nOfft;` |
|      - | 2532 | `	sxi32 rc;` |
|      7 | 2533 | `	if( nArg < 2 ){` |
|      - | 2534 | `		/* Missing arguments,return FALSE */` |
|      3 | 2535 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2536 | `		return PH7_OK;` |
|      - | 2537 | `	}` |
|      - | 2538 | `	/* Extract the needle and the haystack */` |
|      5 | 2539 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2540 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 2541 | `	nOfft = 0; /* cc warning */` |
|      7 | 2542 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 2543 | `		int before = 0;` |
|      - | 2544 | `		/* Perform the lookup */` |
|      5 | 2545 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2546 | `		if( rc != SXRET_OK ){` |
|      - | 2547 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2548 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2549 | `			return PH7_OK;` |
|      - | 2550 | `		}` |
|      - | 2551 | `		/* Return the portion of the string */` |
|      5 | 2552 | `		if( nArg > 2 ){` |
|      3 | 2553 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2554 | `		}` |
|      5 | 2555 | `		if( before ){` |
|      3 | 2556 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2557 | `		}else{` |
|      3 | 2558 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2559 | `		}` |
|      3 | 2560 | `	}else{` |
|    ! 0 | 2561 | `		ph7_result_bool(pCtx,0);` |
|      - | 2562 | `	}` |
|      5 | 2563 | `	return PH7_OK;` |
|      4 | 2564 |  |
|      - | 2565 | `/*` |
|      - | 2566 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2567 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 2568 | ` * Parameters` |
|      - | 2569 | ` *  $haystack` |
|      - | 2570 | ` *   The input string.` |
|      - | 2571 | ` * $needle` |
|      - | 2572 | ` *   Search pattern (must be a string).` |
|      - | 2573 | ` * $offset` |
|      - | 2574 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 2575 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 2576 | ` *   of haystack.` |
|      - | 2577 | ` * Return` |
|      - | 2578 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 2579 | ` */` |
|    120 | 2580 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2581 |  |
|    122 | 2582 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2583 | `	const char *zBlob,*zPattern;` |
|      - | 2584 | `	int nLen,nPatLen,nStart;` |
|      - | 2585 | `	sxu32 nOfft;` |
|      - | 2586 | `	sxi32 rc;` |
|    122 | 2587 | `	if( nArg < 2 ){` |
|      - | 2588 | `		/* Missing arguments,return FALSE */` |
|      7 | 2589 | `		ph7_result_bool(pCtx,0);` |
|      7 | 2590 | `		return PH7_OK;` |
|      - | 2591 | `	}` |
|      - | 2592 | `	/* Extract the needle and the haystack */` |
|    116 | 2593 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    116 | 2594 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    116 | 2595 | `	nOfft = 0; /* cc warning */` |
|    116 | 2596 | `	nStart = 0;` |
|      - | 2597 | `	/* Peek the starting offset if available */` |
|    116 | 2598 | `	if( nArg > 2 ){` |
|    ! 0 | 2599 | `		nStart = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 2600 | `		if( nStart < 0 ){` |
|    ! 0 | 2601 | `			nStart = -nStart;` |
|    ! 0 | 2602 | `		}` |
|    ! 0 | 2603 | `		if( nStart >= nLen ){` |
|      - | 2604 | `			/* Invalid offset */` |
|    ! 0 | 2605 | `			nStart = 0;` |
|    ! 0 | 2606 | `		}else{` |
|    ! 0 | 2607 | `			zBlob += nStart;` |
|    ! 0 | 2608 | `			nLen -= nStart;` |
|      - | 2609 | `		}` |
|    ! 0 | 2610 | `	}` |
|    116 | 2611 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2612 | `		/* Perform the lookup */` |
|    114 | 2613 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    114 | 2614 | `		if( rc != SXRET_OK ){` |
|      - | 2615 | `			/* Pattern not found,return FALSE */` |
|     26 | 2616 | `			ph7_result_bool(pCtx,0);` |
|     26 | 2617 | `			return PH7_OK;` |
|      - | 2618 | `		}` |
|      - | 2619 | `		/* Return the pattern position */` |
|     90 | 2620 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|     46 | 2621 | `	}else{` |
|      3 | 2622 | `		ph7_result_bool(pCtx,0);` |
|      - | 2623 | `	}` |
|     92 | 2624 | `	return PH7_OK;` |
|     62 | 2625 |  |
|      - | 2626 | `/*` |
|      - | 2627 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2628 | ` *  Case-insensitive strpos.` |
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
|     18 | 2641 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2642 |  |
|     19 | 2643 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2644 | `	const char *zBlob,*zPattern;` |
|      - | 2645 | `	int nLen,nPatLen,nStart;` |
|      - | 2646 | `	sxu32 nOfft;` |
|      - | 2647 | `	sxi32 rc;` |
|     19 | 2648 | `	if( nArg < 2 ){` |
|      - | 2649 | `		/* Missing arguments,return FALSE */` |
|      3 | 2650 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2651 | `		return PH7_OK;` |
|      - | 2652 | `	}` |
|      - | 2653 | `	/* Extract the needle and the haystack */` |
|     17 | 2654 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 2655 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     17 | 2656 | `	nOfft = 0; /* cc warning */` |
|     17 | 2657 | `	nStart = 0;` |
|      - | 2658 | `	/* Peek the starting offset if available */` |
|     17 | 2659 | `	if( nArg > 2 ){` |
|      5 | 2660 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 2661 | `		if( nStart < 0 ){` |
|      3 | 2662 | `			nStart = -nStart;` |
|      1 | 2663 | `		}` |
|      5 | 2664 | `		if( nStart >= nLen ){` |
|      - | 2665 | `			/* Invalid offset */` |
|    ! 0 | 2666 | `			nStart = 0;` |
|    ! 0 | 2667 | `		}else{` |
|      5 | 2668 | `			zBlob += nStart;` |
|      5 | 2669 | `			nLen -= nStart;` |
|      - | 2670 | `		}` |
|      2 | 2671 | `	}` |
|     17 | 2672 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2673 | `		/* Perform the lookup */` |
|     17 | 2674 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     17 | 2675 | `		if( rc != SXRET_OK ){` |
|      - | 2676 | `			/* Pattern not found,return FALSE */` |
|      3 | 2677 | `			ph7_result_bool(pCtx,0);` |
|      3 | 2678 | `			return PH7_OK;` |
|      - | 2679 | `		}` |
|      - | 2680 | `		/* Return the pattern position */` |
|     15 | 2681 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 2682 | `	}else{` |
|    ! 0 | 2683 | `		ph7_result_bool(pCtx,0);` |
|      - | 2684 | `	}` |
|     15 | 2685 | `	return PH7_OK;` |
|     10 | 2686 |  |
|      - | 2687 | `/*` |
|      - | 2688 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2689 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 2690 | ` * Parameters` |
|      - | 2691 | ` *  $haystack` |
|      - | 2692 | ` *   The input string.` |
|      - | 2693 | ` * $needle` |
|      - | 2694 | ` *   Search pattern (must be a string).` |
|      - | 2695 | ` * $offset` |
|      - | 2696 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 2697 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 2698 | ` *   characters from the end of the string, searching backwards.` |
|      - | 2699 | ` * Return` |
|      - | 2700 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 2701 | ` */` |
|     32 | 2702 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2703 |  |
|      - | 2704 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     33 | 2705 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2706 | `	int nLen,nPatLen;` |
|      - | 2707 | `	sxu32 nOfft;` |
|      - | 2708 | `	sxi32 rc;` |
|     33 | 2709 | `	if( nArg < 2 ){` |
|      - | 2710 | `		/* Missing arguments,return FALSE */` |
|      3 | 2711 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2712 | `		return PH7_OK;` |
|      - | 2713 | `	}` |
|      - | 2714 | `	/* Extract the needle and the haystack */` |
|     31 | 2715 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 2716 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 2717 | `	/* Point to the end of the pattern */` |
|     31 | 2718 | `	zPtr = &zBlob[nLen - 1];` |
|     31 | 2719 | `	zEnd = &zBlob[nLen];` |
|      - | 2720 | `	/* Save the starting posistion */` |
|     31 | 2721 | `	zStart = zBlob;` |
|     31 | 2722 | `	nOfft = 0; /* cc warning */` |
|      - | 2723 | `	/* Peek the starting offset if available */` |
|     31 | 2724 | `	if( nArg > 2 ){` |
|      - | 2725 | `		int nStart;` |
|     21 | 2726 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 2727 | `		if( nStart < 0 ){` |
|     11 | 2728 | `			nStart = -nStart;` |
|     11 | 2729 | `			if( nStart >= nLen ){` |
|      - | 2730 | `				/* Invalid offset */` |
|      3 | 2731 | `				ph7_result_bool(pCtx,0);` |
|      3 | 2732 | `				return PH7_OK;` |
|    ! 0 | 2733 | `			}else{` |
|      9 | 2734 | `				nLen -= nStart;` |
|      9 | 2735 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 2736 | `				zEnd = &zBlob[nLen];` |
|      - | 2737 | `			}` |
|      5 | 2738 | `		}else{` |
|     11 | 2739 | `			if( nStart >= nLen ){` |
|      - | 2740 | `				/* Invalid offset */` |
|      5 | 2741 | `				ph7_result_bool(pCtx,0);` |
|      5 | 2742 | `				return PH7_OK;` |
|    ! 0 | 2743 | `			}else{` |
|      7 | 2744 | `				zBlob += nStart;` |
|      7 | 2745 | `				nLen -= nStart;` |
|      - | 2746 | `			}` |
|      - | 2747 | `		}` |
|      7 | 2748 | `	}` |
|     25 | 2749 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2750 | `		/* Perform the lookup */` |
|     57 | 2751 | `		for(;;){` |
|    115 | 2752 | `			if( zBlob >= zPtr ){` |
|     11 | 2753 | `				break;` |
|      - | 2754 | `			}` |
|    105 | 2755 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    105 | 2756 | `			if( rc == SXRET_OK ){` |
|      - | 2757 | `				/* Pattern found,return it's position */` |
|     13 | 2758 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 2759 | `				return PH7_OK;` |
|      - | 2760 | `			}` |
|     93 | 2761 | `			zPtr--;` |
|      1 | 2762 | `		}` |
|      - | 2763 | `		/* Pattern not found,return FALSE */` |
|     11 | 2764 | `		ph7_result_bool(pCtx,0);` |
|      6 | 2765 | `	}else{` |
|      3 | 2766 | `		ph7_result_bool(pCtx,0);` |
|      - | 2767 | `	}` |
|     13 | 2768 | `	return PH7_OK;` |
|     17 | 2769 |  |
|      - | 2770 | `/*` |
|      - | 2771 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2772 | ` *  Case-insensitive strrpos.` |
|      - | 2773 | ` * Parameters` |
|      - | 2774 | ` *  $haystack` |
|      - | 2775 | ` *   The input string.` |
|      - | 2776 | ` * $needle` |
|      - | 2777 | ` *   Search pattern (must be a string).` |
|      - | 2778 | ` * $offset` |
|      - | 2779 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 2780 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 2781 | ` *   characters from the end of the string, searching backwards.` |
|      - | 2782 | ` * Return` |
|      - | 2783 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 2784 | ` */` |
|     28 | 2785 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2786 |  |
|      - | 2787 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     29 | 2788 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2789 | `	int nLen,nPatLen;` |
|      - | 2790 | `	sxu32 nOfft;` |
|      - | 2791 | `	sxi32 rc;` |
|     29 | 2792 | `	if( nArg < 2 ){` |
|      - | 2793 | `		/* Missing arguments,return FALSE */` |
|      3 | 2794 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2795 | `		return PH7_OK;` |
|      - | 2796 | `	}` |
|      - | 2797 | `	/* Extract the needle and the haystack */` |
|     27 | 2798 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 2799 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 2800 | `	/* Point to the end of the pattern */` |
|     27 | 2801 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 2802 | `	zEnd = &zBlob[nLen];` |
|      - | 2803 | `	/* Save the starting posistion */` |
|     27 | 2804 | `	zStart = zBlob;` |
|     27 | 2805 | `	nOfft = 0; /* cc warning */` |
|      - | 2806 | `	/* Peek the starting offset if available */` |
|     27 | 2807 | `	if( nArg > 2 ){` |
|      - | 2808 | `		int nStart;` |
|     15 | 2809 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 2810 | `		if( nStart < 0 ){` |
|      7 | 2811 | `			nStart = -nStart;` |
|      7 | 2812 | `			if( nStart >= nLen ){` |
|      - | 2813 | `				/* Invalid offset */` |
|      3 | 2814 | `				ph7_result_bool(pCtx,0);` |
|      3 | 2815 | `				return PH7_OK;` |
|    ! 0 | 2816 | `			}else{` |
|      5 | 2817 | `				nLen -= nStart;` |
|      5 | 2818 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 2819 | `				zEnd = &zBlob[nLen];` |
|      - | 2820 | `			}` |
|      3 | 2821 | `		}else{` |
|      9 | 2822 | `			if( nStart >= nLen ){` |
|      - | 2823 | `				/* Invalid offset */` |
|      5 | 2824 | `				ph7_result_bool(pCtx,0);` |
|      5 | 2825 | `				return PH7_OK;` |
|    ! 0 | 2826 | `			}else{` |
|      5 | 2827 | `				zBlob += nStart;` |
|      5 | 2828 | `				nLen -= nStart;` |
|      - | 2829 | `			}` |
|      - | 2830 | `		}` |
|      4 | 2831 | `	}` |
|     21 | 2832 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2833 | `		/* Perform the lookup */` |
|     44 | 2834 | `		for(;;){` |
|     89 | 2835 | `			if( zBlob >= zPtr ){` |
|      9 | 2836 | `				break;` |
|      - | 2837 | `			}` |
|     81 | 2838 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 2839 | `			if( rc == SXRET_OK ){` |
|      - | 2840 | `				/* Pattern found,return it's position */` |
|     11 | 2841 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 2842 | `				return PH7_OK;` |
|      - | 2843 | `			}` |
|     71 | 2844 | `			zPtr--;` |
|      1 | 2845 | `		}` |
|      - | 2846 | `		/* Pattern not found,return FALSE */` |
|      9 | 2847 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2848 | `	}else{` |
|      3 | 2849 | `		ph7_result_bool(pCtx,0);` |
|      - | 2850 | `	}` |
|     11 | 2851 | `	return PH7_OK;` |
|     15 | 2852 |  |
|      - | 2853 | `/*` |
|      - | 2854 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 2855 | ` *  Find the last occurrence of a character in a string.` |
|      - | 2856 | ` * Parameters` |
|      - | 2857 | ` *  $haystack` |
|      - | 2858 | ` *   The input string.` |
|      - | 2859 | ` * $needle` |
|      - | 2860 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 2861 | ` *  This behavior is different from that of strstr().` |
|      - | 2862 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 2863 | ` *  as the ordinal value of a character.` |
|      - | 2864 | ` * Return` |
|      - | 2865 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 2866 | ` */` |
|     24 | 2867 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2868 |  |
|      - | 2869 | `	const char *zBlob;` |
|      - | 2870 | `	int nLen,c;` |
|     25 | 2871 | `	if( nArg < 2 ){` |
|      - | 2872 | `		/* Missing arguments,return FALSE */` |
|      3 | 2873 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2874 | `		return PH7_OK;` |
|      - | 2875 | `	}` |
|      - | 2876 | `	/* Extract the haystack */` |
|     23 | 2877 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 2878 | `	c = 0; /* cc warning */` |
|     23 | 2879 | `	if( nLen > 0 ){` |
|      - | 2880 | `		sxu32 nOfft;` |
|      - | 2881 | `		sxi32 rc;` |
|     21 | 2882 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 2883 | `			const char *zPattern;` |
|     11 | 2884 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 2885 | `														 * for NULL pointer.` |
|      - | 2886 | `														 */` |
|     11 | 2887 | `			c = zPattern[0];` |
|      6 | 2888 | `		}else{` |
|      - | 2889 | `			/* Int cast */` |
|     11 | 2890 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 2891 | `		}` |
|      - | 2892 | `		/* Perform the lookup */` |
|     21 | 2893 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 2894 | `		if( rc != SXRET_OK ){` |
|      - | 2895 | `			/* No such entry,return FALSE */` |
|      7 | 2896 | `			ph7_result_bool(pCtx,0);` |
|      7 | 2897 | `			return PH7_OK;` |
|      - | 2898 | `		}` |
|      - | 2899 | `		/* Return the string portion */` |
|     15 | 2900 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 2901 | `	}else{` |
|      3 | 2902 | `		ph7_result_bool(pCtx,0);` |
|      - | 2903 | `	}` |
|     17 | 2904 | `	return PH7_OK;` |
|     13 | 2905 |  |
|      - | 2906 | `/*` |
|      - | 2907 | ` * string strrev(string $string)` |
|      - | 2908 | ` *  Reverse a string.` |
|      - | 2909 | ` * Parameters` |
|      - | 2910 | ` *  $string` |
|      - | 2911 | ` *   String to be reversed.` |
|      - | 2912 | ` * Return` |
|      - | 2913 | ` *  The reversed string.` |
|      - | 2914 | ` */` |
|      4 | 2915 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2916 |  |
|      - | 2917 | `	const char *zIn,*zEnd;` |
|      - | 2918 | `	int nLen,c;` |
|      5 | 2919 | `	if( nArg < 1 ){` |
|      - | 2920 | `		/* Missing arguments,return NULL */` |
|      3 | 2921 | `		ph7_result_null(pCtx);` |
|      3 | 2922 | `		return PH7_OK;` |
|      - | 2923 | `	}` |
|      - | 2924 | `	/* Extract the target string */` |
|      3 | 2925 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 2926 | `	if( nLen < 1 ){` |
|      - | 2927 | `		/* Empty string Return null */` |
|    ! 0 | 2928 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2929 | `		return PH7_OK;` |
|      - | 2930 | `	}` |
|      - | 2931 | `	/* Perform the requested operation */` |
|      3 | 2932 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 2933 | `	for(;;){` |
|      9 | 2934 | `		if( zEnd < zIn ){` |
|      - | 2935 | `			/* No more input to process */` |
|      3 | 2936 | `			break;` |
|      - | 2937 | `		}` |
|      - | 2938 | `		/* Append current character */` |
|      7 | 2939 | `		c = zEnd[0];` |
|      7 | 2940 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 2941 | `		zEnd--;` |
|      1 | 2942 | `	}` |
|      3 | 2943 | `	return PH7_OK;` |
|      3 | 2944 |  |
|      - | 2945 | `/*` |
|      - | 2946 | ` * string ucwords(string $string)` |
|      - | 2947 | ` *  Uppercase the first character of each word in a string.` |
|      - | 2948 | ` *  The definition of a word is any string of characters that is immediately after` |
|      - | 2949 | ` *  a whitespace (These are: space, form-feed, newline, carriage return, horizontal tab, and vertical tab).` |
|      - | 2950 | ` * Parameters` |
|      - | 2951 | ` *  $string` |
|      - | 2952 | ` *   The input string.` |
|      - | 2953 | ` * Return` |
|      - | 2954 | ` *  The modified string..` |
|      - | 2955 | ` */` |
|     14 | 2956 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2957 |  |
|      - | 2958 | `	const char *zIn,*zCur,*zEnd;` |
|      - | 2959 | `	int nLen,c;` |
|     15 | 2960 | `	if( nArg < 1 ){` |
|      - | 2961 | `		/* Missing arguments,return NULL */` |
|      3 | 2962 | `		ph7_result_null(pCtx);` |
|      3 | 2963 | `		return PH7_OK;` |
|      - | 2964 | `	}` |
|      - | 2965 | `	/* Extract the target string */` |
|     13 | 2966 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 2967 | `	if( nLen < 1 ){` |
|      - | 2968 | `		/* Empty string – match PHP semantics */` |
|      3 | 2969 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2970 | `		return PH7_OK;` |
|      - | 2971 | `	}` |
|      - | 2972 | `	/* Perform the requested operation */` |
|     11 | 2973 | `	zEnd = &zIn[nLen];` |
|     21 | 2974 | `	for(;;){` |
|      - | 2975 | `		/* Jump leading white spaces */` |
|     43 | 2976 | `		zCur = zIn;` |
|     65 | 2977 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0x80 && SyisSpace(zIn[0]) ){` |
|     23 | 2978 | `			zIn++;` |
|      1 | 2979 | `		}` |
|     43 | 2980 | `		if( zCur < zIn ){` |
|      - | 2981 | `			/* Append white space stream */` |
|     23 | 2982 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 2983 | `		}` |
|     43 | 2984 | `		if( zIn >= zEnd ){` |
|      - | 2985 | `			/* No more input to process */` |
|     11 | 2986 | `			break;` |
|      - | 2987 | `		}` |
|     33 | 2988 | `		c = zIn[0];` |
|     33 | 2989 | `		if( c < 0x80 && SyisLower(c) ){` |
|     29 | 2990 | `			c = SyToUpper(c);` |
|     14 | 2991 | `		}` |
|      - | 2992 | `		/* Append the upper-cased character */` |
|     33 | 2993 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     33 | 2994 | `		zIn++;` |
|     33 | 2995 | `		zCur = zIn;` |
|      - | 2996 | `		/* Append the word varbatim */` |
|    149 | 2997 | `		while( zIn < zEnd ){` |
|    139 | 2998 | `			if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 2999 | `				/* UTF-8 stream */` |
|    ! 0 | 3000 | `				zIn++;` |
|    ! 0 | 3001 | `				SX_JMP_UTF8(zIn,zEnd);` |
|    138 | 3002 | `			}else if( !SyisSpace(zIn[0]) ){` |
|    117 | 3003 | `				zIn++;` |
|     59 | 3004 | `			}else{` |
|     23 | 3005 | `				break;` |
|      - | 3006 | `			}` |
|      1 | 3007 | `		}` |
|     33 | 3008 | `		if( zCur < zIn ){` |
|     33 | 3009 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 | 3010 | `		}` |
|      1 | 3011 | `	}` |
|     11 | 3012 | `	return PH7_OK;` |
|      8 | 3013 |  |
|      - | 3014 | `/*` |
|      - | 3015 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3016 | ` *  Returns input repeated multiplier times.` |
|      - | 3017 | ` * Parameters` |
|      - | 3018 | ` *  $string` |
|      - | 3019 | ` *   String to be repeated.` |
|      - | 3020 | ` * $multiplier` |
|      - | 3021 | ` *  Number of time the input string should be repeated.` |
|      - | 3022 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3023 | ` *  to 0, the function will return an empty string.` |
|      - | 3024 | ` * Return` |
|      - | 3025 | ` *  The repeated string.` |
|      - | 3026 | ` */` |
|  20214 | 3027 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3028 |  |
|      - | 3029 | `	const char *zIn;` |
|      - | 3030 | `	int nLen,nMul;` |
|      - | 3031 | `	int rc;` |
|  20215 | 3032 | `	if( nArg < 2 ){` |
|      - | 3033 | `		/* Missing arguments,return NULL */` |
|      3 | 3034 | `		ph7_result_null(pCtx);` |
|      3 | 3035 | `		return PH7_OK;` |
|      - | 3036 | `	}` |
|      - | 3037 | `	/* Extract the target string */` |
|  20213 | 3038 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|  20213 | 3039 | `	if( nLen < 1 ){` |
|      - | 3040 | `		/* Empty string.Return null */` |
|    ! 0 | 3041 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3042 | `		return PH7_OK;` |
|      - | 3043 | `	}` |
|      - | 3044 | `	/* Extract the multiplier */` |
|  20213 | 3045 | `	nMul = ph7_value_to_int(apArg[1]);` |
|  20213 | 3046 | `	if( nMul < 1 ){` |
|      - | 3047 | `		/* Return the empty string */` |
|      3 | 3048 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3049 | `		return PH7_OK;` |
|      - | 3050 | `	}` |
|      - | 3051 | `	/* Perform the requested operation */` |
| 120224 | 3052 | `	for(;;){` |
| 240449 | 3053 | `		if( !nMul ){` |
|  20211 | 3054 | `			break;` |
|      - | 3055 | `		}` |
|      - | 3056 | `		/* Append the copy */` |
| 220239 | 3057 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 220239 | 3058 | `		if( rc != PH7_OK ){` |
|      - | 3059 | `			/* Out of memory,break immediately */` |
|    ! 0 | 3060 | `			break;` |
|      - | 3061 | `		}` |
| 220239 | 3062 | `		nMul--;` |
|      1 | 3063 | `	}` |
|  20211 | 3064 | `	return PH7_OK;` |
|  10108 | 3065 |  |
|      - | 3066 | `/*` |
|      - | 3067 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3068 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3069 | ` * Parameters` |
|      - | 3070 | ` *  $string` |
|      - | 3071 | ` *   The input string.` |
|      - | 3072 | ` * $is_xhtml` |
|      - | 3073 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3074 | ` * Return` |
|      - | 3075 | ` *  The processed string.` |
|      - | 3076 | ` */` |
|      6 | 3077 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3078 |  |
|      - | 3079 | `	const char *zIn,*zCur,*zEnd;` |
|      7 | 3080 | `	int is_xhtml = 1; /* Default to XHTML-style '<br/>' like PHP */` |
|      - | 3081 | `	int nLen;` |
|      7 | 3082 | `	if( nArg < 1 ){` |
|      - | 3083 | `		/* Missing arguments,return the empty string */` |
|      3 | 3084 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3085 | `		return PH7_OK;` |
|      - | 3086 | `	}` |
|      - | 3087 | `	/* Extract the target string */` |
|      5 | 3088 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3089 | `	if( nLen < 1 ){` |
|      - | 3090 | `		/* Empty string,return null */` |
|    ! 0 | 3091 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3092 | `		return PH7_OK;` |
|      - | 3093 | `	}` |
|      5 | 3094 | `	if( nArg > 1 ){` |
|      3 | 3095 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3096 | `	}` |
|      5 | 3097 | `	zEnd = &zIn[nLen];` |
|      - | 3098 | `	/* Perform the requested operation */` |
|      4 | 3099 | `	for(;;){` |
|      9 | 3100 | `		zCur = zIn;` |
|      - | 3101 | `		/* Delimit the string */` |
|     21 | 3102 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3103 | `			zIn++;` |
|      1 | 3104 | `		}` |
|      9 | 3105 | `		if( zCur < zIn ){` |
|      - | 3106 | `			/* Output chunk verbatim */` |
|      9 | 3107 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3108 | `		}` |
|      9 | 3109 | `		if( zIn >= zEnd ){` |
|      - | 3110 | `			/* No more input to process */` |
|      5 | 3111 | `			break;` |
|      - | 3112 | `		}` |
|      - | 3113 | `		/* Output the HTML line break */` |
|      - | 3114 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br/>' (legacy without space), otherwise use '<br>' */` |
|      5 | 3115 | `		if( is_xhtml ){` |
|      3 | 3116 | `			ph7_result_string(pCtx,"<br/>",(int)sizeof("<br/>")-1);` |
|      2 | 3117 | `		}else{` |
|      3 | 3118 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3119 | `		}` |
|      5 | 3120 | `		zCur = zIn;` |
|      - | 3121 | `		/* Append trailing line */` |
|     11 | 3122 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3123 | `			zIn++;` |
|      1 | 3124 | `		}` |
|      5 | 3125 | `		if( zCur < zIn ){` |
|      - | 3126 | `			/* Output chunk verbatim */` |
|      5 | 3127 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 3128 | `		}` |
|      1 | 3129 | `	}` |
|      5 | 3130 | `	return PH7_OK;` |
|      4 | 3131 |  |
|      - | 3132 | `/*` |
|      - | 3133 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3134 | ` *  According to the PHP reference manual.` |
|      - | 3135 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3136 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3137 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 3138 | ` * This applies to both sprintf() and printf().` |
|      - | 3139 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 3140 | ` * or more of these elements, in order:` |
|      - | 3141 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 3142 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 3143 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 3144 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 3145 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 3146 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 3147 | ` *   it with a single quote ('). See the examples below.` |
|      - | 3148 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 3149 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 3150 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 3151 | ` *   should result in.` |
|      - | 3152 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 3153 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 3154 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 3155 | ` *   limit to the string.` |
|      - | 3156 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 3157 | ` *       % - a literal percent character. No argument is required.` |
|      - | 3158 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 3159 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 3160 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 3161 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 3162 | ` * 	     for the number of digits after the decimal point.` |
|      - | 3163 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 3164 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 3165 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 3166 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 3167 | ` *       g - shorter of %e and %f.` |
|      - | 3168 | ` *       G - shorter of %E and %f.` |
|      - | 3169 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 3170 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 3171 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 3172 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 3173 | ` */` |
|      - | 3174 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3175 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 3176 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 3177 | `/*` |
|      - | 3178 | `** Conversion types fall into various categories as defined by the` |
|      - | 3179 | `** following enumeration.` |
|      - | 3180 | `*/` |
|      - | 3181 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 3182 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 3183 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 3184 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 3185 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 3186 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 3187 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 3188 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 3189 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 3190 |  |
|      - | 3191 | `/*` |
|      - | 3192 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 3193 | `*/` |
|      - | 3194 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 3195 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 3196 | `/*` |
|      - | 3197 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 3198 | `** by an instance of the following structure` |
|      - | 3199 | `*/` |
|      - | 3200 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 3201 | `struct ph7_fmt_info` |
|      - | 3202 |  |
|      - | 3203 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 3204 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 3205 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 3206 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 3207 | `  char *charset; /* The character set for conversion */` |
|      - | 3208 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 3209 | `};` |
|      - | 3210 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3211 | `/*` |
|      - | 3212 | `** "*val" is a double such that 0.1 <= *val < 10.0` |
|      - | 3213 | `** Return the ascii code for the leading digit of *val, then` |
|      - | 3214 | `** multiply "*val" by 10.0 to renormalize.` |
|      - | 3215 | `**` |
|      - | 3216 | `** Example:` |
|      - | 3217 | `**     input:     *val = 3.14159` |
|      - | 3218 | `**     output:    *val = 1.4159    function return = '3'` |
|      - | 3219 | `**` |
|      - | 3220 | `** The counter *cnt is incremented each time.  After counter exceeds` |
|      - | 3221 | `** 16 (the number of significant digits in a 64-bit float) '0' is` |
|      - | 3222 | `** always returned.` |
|      - | 3223 | `*/` |
|    422 | 3224 | `static int vxGetdigit(sxlongreal *val,int *cnt)` |
|      1 | 3225 |  |
|      - | 3226 | `  sxlongreal d;` |
|      - | 3227 | `  int digit;` |
|      - | 3228 |  |
|    423 | 3229 | `  if( (*cnt)++ >= 16 ){` |
|    ! 0 | 3230 | `	  return '0';` |
|      - | 3231 | `  }` |
|    423 | 3232 | `  digit = (int)*val;` |
|    423 | 3233 | `  d = digit;` |
|    423 | 3234 | `   *val = (*val - d)*10.0;` |
|    423 | 3235 | `  return digit + '0' ;` |
|    212 | 3236 |  |
|      - | 3237 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      - | 3238 | `/*` |
|      - | 3239 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 3240 | ` * used conversion types first.` |
|      - | 3241 | ` */` |
|      - | 3242 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 3243 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 3244 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 3245 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 3246 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 3247 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 3248 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 3249 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 3250 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 3251 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3252 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3253 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 3254 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 3255 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 3256 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 3257 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 3258 | `};` |
|      - | 3259 | `/*` |
|      - | 3260 | ` * Format a given string.` |
|      - | 3261 | ` * The root program.  All variations call this core.` |
|      - | 3262 | ` * INPUTS:` |
|      - | 3263 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 3264 | ` *            1. A pointer to the call context.` |
|      - | 3265 | ` *            2. A pointer to the list of characters to be output` |
|      - | 3266 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 3267 | ` *            3. An integer number of characters to be output.` |
|      - | 3268 | ` *               (Note: This number might be zero.)` |
|      - | 3269 | ` *            4. Upper layer private data.` |
|      - | 3270 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 3271 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 3272 | ` */` |
|    136 | 3273 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 3274 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 3275 | `	ph7_context *pCtx,  /* call context */` |
|      - | 3276 | `	const char *zIn,    /* Format string */` |
|      - | 3277 | `	int nByte,          /* Format string length */` |
|      - | 3278 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 3279 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 3280 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 3281 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 3282 | `	)` |
|      1 | 3283 |  |
|    137 | 3284 | `	char spaces[] = "                                                  ";` |
|      - | 3285 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    137 | 3286 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 3287 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 3288 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 3289 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 3290 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 3291 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 3292 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 3293 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 3294 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 3295 | `	ph7_int64 iVal;` |
|      - | 3296 | `	int precision;           /* Precision of the current field */` |
|      - | 3297 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 3298 | `	int c,rc,n;` |
|      - | 3299 | `	int length;              /* Length of the field */` |
|      - | 3300 | `	int prefix;` |
|      - | 3301 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 3302 | `	int width;               /* Width of the current field */` |
|      - | 3303 | `	int idx;` |
|    137 | 3304 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 3305 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 3306 | `	/* Start the format process */` |
|    139 | 3307 | `	for(;;){` |
|    279 | 3308 | `		zCur = zIn;` |
|    739 | 3309 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|    461 | 3310 | `			zIn++;` |
|      1 | 3311 | `		}` |
|    279 | 3312 | `		if( zCur < zIn ){` |
|      - | 3313 | `			/* Consume chunk verbatim */` |
|    105 | 3314 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|    105 | 3315 | `			if( rc == SXERR_ABORT ){` |
|      - | 3316 | `				/* Callback request an operation abort */` |
|    ! 0 | 3317 | `				break;` |
|      - | 3318 | `			}` |
|     52 | 3319 | `		}` |
|    279 | 3320 | `		if( zIn >= zEnd ){` |
|      - | 3321 | `			/* No more input to process,break immediately */` |
|    135 | 3322 | `			break;` |
|      - | 3323 | `		}` |
|      - | 3324 | `		/* Find out what flags are present */` |
|    145 | 3325 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    144 | 3326 | `			flag_alternateform = flag_zeropad = 0;` |
|    145 | 3327 | `		zIn++; /* Jump the precent sign */` |
|     72 | 3328 | `		do{` |
|    177 | 3329 | `			c = zIn[0];` |
|    177 | 3330 | `			switch( c ){` |
|      9 | 3331 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      3 | 3332 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 3333 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|      9 | 3334 | `			case '#':   flag_alternateform = 1;   c = 0;   break;` |
|      9 | 3335 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 3336 | `			case '\'':` |
|    ! 0 | 3337 | `				zIn++;` |
|    ! 0 | 3338 | `				if( zIn < zEnd ){` |
|      - | 3339 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 3340 | `					c = zIn[0];` |
|    ! 0 | 3341 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3342 | `						spaces[idx] = (char)c;` |
|    ! 0 | 3343 | `					}` |
|    ! 0 | 3344 | `					c = 0;` |
|    ! 0 | 3345 | `				}` |
|    ! 0 | 3346 | `				break;` |
|    144 | 3347 | `			default:                                       break;` |
|      - | 3348 | `			}` |
|    177 | 3349 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 3350 | `		/* Get the field width */` |
|    145 | 3351 | `		width = 0;` |
|    251 | 3352 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     35 | 3353 | `			width = width*10 + (zIn[0] - '0');` |
|     35 | 3354 | `			zIn++;` |
|      1 | 3355 | `		}` |
|    145 | 3356 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 3357 | `			/* Position specifer */` |
|    ! 0 | 3358 | `			if( width > 0 ){` |
|    ! 0 | 3359 | `				n = width;` |
|    ! 0 | 3360 | `				if( vf && n > 0 ){` |
|    ! 0 | 3361 | `					n--;` |
|    ! 0 | 3362 | `				}` |
|    ! 0 | 3363 | `			}` |
|    ! 0 | 3364 | `			zIn++;` |
|    ! 0 | 3365 | `			width = 0;` |
|    ! 0 | 3366 | `			if( zIn < zEnd && zIn[0] == '0' ){` |
|    ! 0 | 3367 | `				flag_zeropad = 1;` |
|    ! 0 | 3368 | `				zIn++;` |
|    ! 0 | 3369 | `			}` |
|    ! 0 | 3370 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 3371 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 3372 | `				zIn++;` |
|    ! 0 | 3373 | `			}` |
|    ! 0 | 3374 | `		}` |
|    145 | 3375 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 3376 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 3377 | `		}` |
|      - | 3378 | `		/* Get the precision */` |
|    145 | 3379 | `		precision = -1;` |
|    145 | 3380 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     59 | 3381 | `			precision = 0;` |
|     59 | 3382 | `			zIn++;` |
|    150 | 3383 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     63 | 3384 | `				precision = precision*10 + (zIn[0] - '0');` |
|     63 | 3385 | `				zIn++;` |
|      1 | 3386 | `			}` |
|     29 | 3387 | `		}` |
|    145 | 3388 | `		if( zIn >= zEnd ){` |
|      - | 3389 | `			/* No more input */` |
|      3 | 3390 | `			break;` |
|      - | 3391 | `		}` |
|      - | 3392 | `		/* Fetch the info entry for the field */` |
|    143 | 3393 | `		pInfo = 0;` |
|    143 | 3394 | `		xtype = PH7_FMT_ERROR;` |
|    143 | 3395 | `		c = zIn[0];` |
|    143 | 3396 | `		zIn++; /* Jump the format specifer */` |
|    787 | 3397 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|    785 | 3398 | `			if( c==aFmt[idx].fmttype ){` |
|    141 | 3399 | `				pInfo = &aFmt[idx];` |
|    141 | 3400 | `				xtype = pInfo->type;` |
|    141 | 3401 | `				break;` |
|      - | 3402 | `			}` |
|    323 | 3403 | `		}` |
|    143 | 3404 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    143 | 3405 | `		length = 0;` |
|      - | 3406 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 3407 | `		 /*` |
|      - | 3408 | `		  ** At this point, variables are initialized as follows:` |
|      - | 3409 | `		  **` |
|      - | 3410 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 3411 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 3412 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 3413 | `		  **                               field width was negative.` |
|      - | 3414 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 3415 | `		  **                               the conversion character.` |
|      - | 3416 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 3417 | `		  **   width                       The specified field width.  This is` |
|      - | 3418 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 3419 | `		  **   precision                   The specified precision.  The default` |
|      - | 3420 | `		  **                               is -1.` |
|      - | 3421 | `		  */` |
|    143 | 3422 | `		switch(xtype){` |
|    ! 0 | 3423 | `		case PH7_FMT_PERCENT:` |
|      - | 3424 | `			/* A literal percent character */` |
|    ! 0 | 3425 | `			zWorker[0] = '%';` |
|    ! 0 | 3426 | `			length = (int)sizeof(char);` |
|    ! 0 | 3427 | `			break;` |
|      3 | 3428 | `		case PH7_FMT_CHARX:` |
|      - | 3429 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 3430 | `			 * with that ASCII value` |
|      - | 3431 | `			 */` |
|      7 | 3432 | `			pArg = NEXT_ARG;` |
|      7 | 3433 | `			if( pArg == 0 ){` |
|      3 | 3434 | `				c = 0;` |
|      2 | 3435 | `			}else{` |
|      5 | 3436 | `				c = ph7_value_to_int(pArg);` |
|      - | 3437 | `			}` |
|      - | 3438 | `			/* NUL byte is an acceptable value */` |
|      7 | 3439 | `			zWorker[0] = (char)c;` |
|      7 | 3440 | `			length = (int)sizeof(char);` |
|      7 | 3441 | `			break;` |
|     12 | 3442 | `		case PH7_FMT_STRING:` |
|      - | 3443 | `			/* the argument is treated as and presented as a string */` |
|     25 | 3444 | `			pArg = NEXT_ARG;` |
|     25 | 3445 | `			if( pArg == 0 ){` |
|    ! 0 | 3446 | `				length = 0;` |
|    ! 0 | 3447 | `			}else{` |
|     25 | 3448 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 3449 | `			}` |
|     25 | 3450 | `			if( length < 1 ){` |
|    ! 0 | 3451 | `				zBuf = " ";` |
|    ! 0 | 3452 | `				length = (int)sizeof(char);` |
|    ! 0 | 3453 | `			}` |
|     25 | 3454 | `			if( precision>=0 && precision<length ){` |
|      3 | 3455 | `				length = precision;` |
|      1 | 3456 | `			}` |
|     25 | 3457 | `			if( flag_zeropad ){` |
|      - | 3458 | `				/* zero-padding works on strings too */` |
|    ! 0 | 3459 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3460 | `					spaces[idx] = '0';` |
|    ! 0 | 3461 | `				}` |
|    ! 0 | 3462 | `			}` |
|     25 | 3463 | `			break;` |
|     27 | 3464 | `		case PH7_FMT_RADIX:` |
|     55 | 3465 | `			pArg = NEXT_ARG;` |
|     55 | 3466 | `			if( pArg == 0 ){` |
|    ! 0 | 3467 | `				iVal = 0;` |
|    ! 0 | 3468 | `			}else{` |
|     55 | 3469 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 3470 | `			}` |
|      - | 3471 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|     55 | 3472 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 3473 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 3474 | `			}` |
|      - | 3475 | `#if 1` |
|      - | 3476 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 3477 | `        ** I think this is stupid.*/` |
|     55 | 3478 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 3479 | `#else` |
|      - | 3480 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 3481 | `        ** but leave the prefix for hex.*/` |
|      - | 3482 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 3483 | `#endif` |
|     55 | 3484 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|     25 | 3485 | `          if( iVal<0 ){` |
|      3 | 3486 | `            iVal = -iVal;` |
|      - | 3487 | `			/* Ticket 1433-003 */` |
|      3 | 3488 | `			if( iVal < 0 ){` |
|      - | 3489 | `				/* Overflow */` |
|    ! 0 | 3490 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3491 | `			}` |
|      3 | 3492 | `            prefix = '-';` |
|     24 | 3493 | `          }else if( flag_plussign )  prefix = '+';` |
|     21 | 3494 | `          else if( flag_blanksign )  prefix = ' ';` |
|     19 | 3495 | `          else                       prefix = 0;` |
|     13 | 3496 | `        }else{` |
|     31 | 3497 | `			if( iVal<0 ){` |
|    ! 0 | 3498 | `				iVal = -iVal;` |
|      - | 3499 | `				/* Ticket 1433-003 */` |
|    ! 0 | 3500 | `				if( iVal < 0 ){` |
|      - | 3501 | `					/* Overflow */` |
|    ! 0 | 3502 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3503 | `				}` |
|    ! 0 | 3504 | `			}` |
|     31 | 3505 | `			prefix = 0;` |
|      - | 3506 | `		}` |
|     55 | 3507 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      7 | 3508 | `          precision = width-(prefix!=0);` |
|      3 | 3509 | `        }` |
|     55 | 3510 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 3511 | `        {` |
|      - | 3512 | `          register char *cset;      /* Use registers for speed */` |
|      - | 3513 | `          register int base;` |
|     55 | 3514 | `          cset = pInfo->charset;` |
|     55 | 3515 | `          base = pInfo->base;` |
|     27 | 3516 | `          do{                                           /* Convert to ascii */` |
|    123 | 3517 | `            *(--zBuf) = cset[iVal%base];` |
|    123 | 3518 | `            iVal = iVal/base;` |
|    123 | 3519 | `          }while( iVal>0 );` |
|      - | 3520 | `        }` |
|     55 | 3521 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     77 | 3522 | `        for(idx=precision-length; idx>0; idx--){` |
|     23 | 3523 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|     12 | 3524 | `        }` |
|     55 | 3525 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|     55 | 3526 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 3527 | `          char *pre, x;` |
|      9 | 3528 | `          pre = pInfo->prefix;` |
|      9 | 3529 | `          if( *zBuf!=pre[0] ){` |
|     23 | 3530 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|      4 | 3531 | `          }` |
|      4 | 3532 | `        }` |
|     55 | 3533 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     55 | 3534 | `		break;` |
|     28 | 3535 | `		case PH7_FMT_FLOAT:` |
|      - | 3536 | `		case PH7_FMT_EXP:` |
|      - | 3537 | `		case PH7_FMT_GENERIC:{` |
|      - | 3538 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3539 | `		long double realvalue;` |
|      - | 3540 | `		int  exp;                /* exponent of real numbers */` |
|      - | 3541 | `		double rounder;          /* Used for rounding floating point values */` |
|      - | 3542 | `		int flag_dp;            /* True if decimal point should be shown */` |
|      - | 3543 | `		int flag_rtz;           /* True if trailing zeros should be removed */` |
|      - | 3544 | `		int flag_exp;           /* True to force display of the exponent */` |
|      - | 3545 | `		int nsd;                 /* Number of significant digits returned */` |
|     57 | 3546 | `		pArg = NEXT_ARG;` |
|     57 | 3547 | `		if( pArg == 0 ){` |
|    ! 0 | 3548 | `			realvalue = 0;` |
|    ! 0 | 3549 | `		}else{` |
|     57 | 3550 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 3551 | `		}` |
|      - | 3552 | `		/* Special-case NaN and infinities since the normal formatting logic` |
|      - | 3553 | `		 * below assumes a finite positive realvalue. */` |
|     57 | 3554 | `		if( PH7_IS_NAN(realvalue) ){` |
|    ! 0 | 3555 | `			zBuf = "NAN";` |
|    ! 0 | 3556 | `			length = 3;` |
|    ! 0 | 3557 | `			break;` |
|      - | 3558 | `		}` |
|     57 | 3559 | `		if( PH7_IS_INF(realvalue) ){` |
|      - | 3560 | `			/* Infinity prints as INF or -INF depending on sign. */` |
|    ! 0 | 3561 | `			if( realvalue < 0.0 ){` |
|    ! 0 | 3562 | `				zBuf = "-INF";` |
|    ! 0 | 3563 | `				length = 4;` |
|    ! 0 | 3564 | `			}else{` |
|    ! 0 | 3565 | `				zBuf = "INF";` |
|    ! 0 | 3566 | `				length = 3;` |
|      - | 3567 | `			}` |
|    ! 0 | 3568 | `			break;` |
|      - | 3569 | `		}` |
|     57 | 3570 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|     57 | 3571 | `		if( precision>PH7_FMT_BUFSIZ-40) precision = PH7_FMT_BUFSIZ-40;` |
|     57 | 3572 | `        if( realvalue<0.0 ){` |
|      3 | 3573 | `          realvalue = -realvalue;` |
|      3 | 3574 | `          prefix = '-';` |
|      2 | 3575 | `        }else{` |
|     55 | 3576 | `          if( flag_plussign )          prefix = '+';` |
|     55 | 3577 | `          else if( flag_blanksign )    prefix = ' ';` |
|     55 | 3578 | `          else                         prefix = 0;` |
|      - | 3579 | `        }` |
|     57 | 3580 | `        if( pInfo->type==PH7_FMT_GENERIC && precision>0 ) precision--;` |
|     57 | 3581 | `        rounder = 0.0;` |
|      - | 3582 | `#if 0` |
|      - | 3583 | `        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */` |
|      - | 3584 | `        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);` |
|      - | 3585 | `#else` |
|      - | 3586 | `        /* It makes more sense to use 0.5 */` |
|    405 | 3587 | `        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);` |
|      - | 3588 | `#endif` |
|     57 | 3589 | `        if( pInfo->type==PH7_FMT_FLOAT ) realvalue += rounder;` |
|      - | 3590 | `        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */` |
|     57 | 3591 | `        exp = 0;` |
|     57 | 3592 | `        if( realvalue>0.0 ){` |
|     61 | 3593 | `          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }` |
|     89 | 3594 | `          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }` |
|     61 | 3595 | `          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }` |
|     71 | 3596 | `          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }` |
|     57 | 3597 | `          if( exp>350 \|\| exp<-350 ){` |
|    ! 0 | 3598 | `            zBuf = "NaN";` |
|    ! 0 | 3599 | `            length = 3;` |
|    ! 0 | 3600 | `            break;` |
|      - | 3601 | `          }` |
|     28 | 3602 | `        }` |
|     57 | 3603 | `        zBuf = zWorker;` |
|      - | 3604 | `        /*` |
|      - | 3605 | `        ** If the field type is etGENERIC, then convert to either etEXP` |
|      - | 3606 | `        ** or etFLOAT, as appropriate.` |
|      - | 3607 | `        */` |
|     57 | 3608 | `        flag_exp = xtype==PH7_FMT_EXP;` |
|     57 | 3609 | `        if( xtype!=PH7_FMT_FLOAT ){` |
|    ! 0 | 3610 | `          realvalue += rounder;` |
|    ! 0 | 3611 | `          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }` |
|    ! 0 | 3612 | `        }` |
|     57 | 3613 | `        if( xtype==PH7_FMT_GENERIC ){` |
|    ! 0 | 3614 | `          flag_rtz = !flag_alternateform;` |
|    ! 0 | 3615 | `          if( exp<-4 \|\| exp>precision ){` |
|    ! 0 | 3616 | `            xtype = PH7_FMT_EXP;` |
|    ! 0 | 3617 | `          }else{` |
|    ! 0 | 3618 | `            precision = precision - exp;` |
|    ! 0 | 3619 | `            xtype = PH7_FMT_FLOAT;` |
|      - | 3620 | `          }` |
|    ! 0 | 3621 | `        }else{` |
|     57 | 3622 | `          flag_rtz = 0;` |
|      - | 3623 | `        }` |
|      - | 3624 | `        /*` |
|      - | 3625 | `        ** The "exp+precision" test causes output to be of type etEXP if` |
|      - | 3626 | `        ** the precision is too large to fit in buf[].` |
|      - | 3627 | `        */` |
|     57 | 3628 | `        nsd = 0;` |
|     57 | 3629 | `        if( xtype==PH7_FMT_FLOAT && exp+precision<PH7_FMT_BUFSIZ-30 ){` |
|     57 | 3630 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|     57 | 3631 | `          if( prefix ) *(zBuf++) = (char)prefix;         /* Sign */` |
|     57 | 3632 | `          if( exp<0 )  *(zBuf++) = '0';            /* Digits before "." */` |
|    149 | 3633 | `          else for(; exp>=0; exp--) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     57 | 3634 | `          if( flag_dp ) *(zBuf++) = '.';           /* The decimal point */` |
|     89 | 3635 | `          for(exp++; exp<0 && precision>0; precision--, exp++){` |
|     33 | 3636 | `            *(zBuf++) = '0';` |
|     17 | 3637 | `          }` |
|    373 | 3638 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     57 | 3639 | `          *(zBuf--) = 0;                           /* Null terminate */` |
|     57 | 3640 | `          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */` |
|    ! 0 | 3641 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 3642 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 3643 | `          }` |
|     57 | 3644 | `          zBuf++;                            /* point to next free slot */` |
|     29 | 3645 | `        }else{    /* etEXP or etGENERIC */` |
|    ! 0 | 3646 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|    ! 0 | 3647 | `          if( prefix ) *(zBuf++) = (char)prefix;   /* Sign */` |
|    ! 0 | 3648 | `          *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);  /* First digit */` |
|    ! 0 | 3649 | `          if( flag_dp ) *(zBuf++) = '.';     /* Decimal point */` |
|    ! 0 | 3650 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|    ! 0 | 3651 | `          zBuf--;                            /* point to last digit */` |
|    ! 0 | 3652 | `          if( flag_rtz && flag_dp ){          /* Remove tail zeros */` |
|    ! 0 | 3653 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 3654 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 3655 | `          }` |
|    ! 0 | 3656 | `          zBuf++;                            /* point to next free slot */` |
|    ! 0 | 3657 | `          if( exp \|\| flag_exp ){` |
|    ! 0 | 3658 | `            *(zBuf++) = pInfo->charset[0];` |
|    ! 0 | 3659 | `            if( exp<0 ){ *(zBuf++) = '-'; exp = -exp; } /* sign of exp */` |
|    ! 0 | 3660 | `            else       { *(zBuf++) = '+'; }` |
|    ! 0 | 3661 | `            if( exp>=100 ){` |
|    ! 0 | 3662 | `              *(zBuf++) = (char)((exp/100)+'0');                /* 100's digit */` |
|    ! 0 | 3663 | `              exp %= 100;` |
|    ! 0 | 3664 | `            }` |
|    ! 0 | 3665 | `            *(zBuf++) = (char)(exp/10+'0');                     /* 10's digit */` |
|    ! 0 | 3666 | `            *(zBuf++) = (char)(exp%10+'0');                     /* 1's digit */` |
|    ! 0 | 3667 | `          }` |
|      - | 3668 | `        }` |
|      - | 3669 | `        /* The converted number is in buf[] and zero terminated.Output it.` |
|      - | 3670 | `        ** Note that the number is in the usual order, not reversed as with` |
|      - | 3671 | `        ** integer conversions.*/` |
|     57 | 3672 | `        length = (int)(zBuf-zWorker);` |
|     57 | 3673 | `        zBuf = zWorker;` |
|      - | 3674 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 3675 | `        ** set and we are not left justified */` |
|     57 | 3676 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 3677 | `          int i;` |
|      3 | 3678 | `          int nPad = width - length;` |
|     13 | 3679 | `          for(i=width; i>=nPad; i--){` |
|     11 | 3680 | `            zBuf[i] = zBuf[i-nPad];` |
|      6 | 3681 | `          }` |
|      3 | 3682 | `          i = prefix!=0;` |
|      5 | 3683 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      3 | 3684 | `          length = width;` |
|      1 | 3685 | `        }` |
|      - | 3686 | `#else` |
|      - | 3687 | `         zBuf = " ";` |
|      - | 3688 | `		 length = (int)sizeof(char);` |
|      - | 3689 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|     57 | 3690 | `		 break;` |
|      - | 3691 | `							 }` |
|      1 | 3692 | `		default:` |
|      - | 3693 | `			/* Invalid format specifer */` |
|      3 | 3694 | `			zWorker[0] = '?';` |
|      3 | 3695 | `			length = (int)sizeof(char);` |
|      2 | 3696 | `			break;` |
|      - | 3697 | `		}` |
|      - | 3698 | `		 /*` |
|      - | 3699 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 3700 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 3701 | `		 ** the output.` |
|      - | 3702 | `		 */` |
|    143 | 3703 | `    if( !flag_leftjustify ){` |
|      - | 3704 | `      register int nspace;` |
|    135 | 3705 | `      nspace = width-length;` |
|    135 | 3706 | `      if( nspace>0 ){` |
|      5 | 3707 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3708 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3709 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3710 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3711 | `			}` |
|    ! 0 | 3712 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3713 | `        }` |
|      5 | 3714 | `        if( nspace>0 ){` |
|      5 | 3715 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      5 | 3716 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3717 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3718 | `			}` |
|      2 | 3719 | `		}` |
|      2 | 3720 | `      }` |
|     67 | 3721 | `    }` |
|    143 | 3722 | `    if( length>0 ){` |
|    143 | 3723 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    143 | 3724 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3725 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3726 | `		}` |
|     71 | 3727 | `    }` |
|    143 | 3728 | `    if( flag_leftjustify ){` |
|      - | 3729 | `      register int nspace;` |
|      9 | 3730 | `      nspace = width-length;` |
|      9 | 3731 | `      if( nspace>0 ){` |
|      9 | 3732 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3733 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3734 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3735 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3736 | `			}` |
|    ! 0 | 3737 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3738 | `        }` |
|      9 | 3739 | `        if( nspace>0 ){` |
|      9 | 3740 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      9 | 3741 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3742 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3743 | `			}` |
|      4 | 3744 | `		}` |
|      4 | 3745 | `      }` |
|      4 | 3746 | `    }` |
|      1 | 3747 | ` }/* for(;;) */` |
|    137 | 3748 | `	return SXRET_OK;` |
|     69 | 3749 |  |
|      - | 3750 | `/*` |
|      - | 3751 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 3752 | ` */` |
|     90 | 3753 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 3754 |  |
|      - | 3755 | `	/* Consume directly */` |
|     91 | 3756 | `	ph7_result_string(pCtx,zInput,nLen);` |
|     45 | 3757 | `	SXUNUSED(pUserData); /* cc warning */` |
|     91 | 3758 | `	return PH7_OK;` |
|      1 | 3759 |  |
|      - | 3760 | `/*` |
|      - | 3761 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 3762 | ` *  Return a formatted string.` |
|      - | 3763 | ` * Parameters` |
|      - | 3764 | ` *  $format` |
|      - | 3765 | ` *    The format string (see block comment above)` |
|      - | 3766 | ` * Return` |
|      - | 3767 | ` *  A string produced according to the formatting string format.` |
|      - | 3768 | ` */` |
|     62 | 3769 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3770 |  |
|      - | 3771 | `	const char *zFormat;` |
|      - | 3772 | `	int nLen;` |
|     63 | 3773 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3774 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 3775 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3776 | `		return PH7_OK;` |
|      - | 3777 | `	}` |
|      - | 3778 | `	/* Extract the string format */` |
|     61 | 3779 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     61 | 3780 | `	if( nLen < 1 ){` |
|      - | 3781 | `		/* Empty string */` |
|    ! 0 | 3782 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3783 | `		return PH7_OK;` |
|      - | 3784 | `	}` |
|      - | 3785 | `	/* Format the string */` |
|     61 | 3786 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,0,FALSE);` |
|     61 | 3787 | `	return PH7_OK;` |
|     32 | 3788 |  |
|      - | 3789 | `/*` |
|      - | 3790 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 3791 | ` */` |
|    130 | 3792 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 3793 |  |
|    131 | 3794 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 3795 | `	/* Call the VM output consumer directly */` |
|    131 | 3796 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 3797 | `	/* Increment counter */` |
|    131 | 3798 | `	*pCounter += nLen;` |
|    131 | 3799 | `	return PH7_OK;` |
|      1 | 3800 |  |
|      - | 3801 | `/*` |
|      - | 3802 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 3803 | ` *  Output a formatted string.` |
|      - | 3804 | ` * Parameters` |
|      - | 3805 | ` *  $format` |
|      - | 3806 | ` *   See sprintf() for a description of format.` |
|      - | 3807 | ` * Return` |
|      - | 3808 | ` *  The length of the outputted string.` |
|      - | 3809 | ` */` |
|     52 | 3810 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3811 |  |
|     53 | 3812 | `	ph7_int64 nCounter = 0;` |
|      - | 3813 | `	const char *zFormat;` |
|      - | 3814 | `	int nLen;` |
|     53 | 3815 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3816 | `		/* Missing/Invalid arguments,return 0 */` |
|      3 | 3817 | `		ph7_result_int(pCtx,0);` |
|      3 | 3818 | `		return PH7_OK;` |
|      - | 3819 | `	}` |
|      - | 3820 | `	/* Extract the string format */` |
|     51 | 3821 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     51 | 3822 | `	if( nLen < 1 ){` |
|      - | 3823 | `		/* Empty string */` |
|    ! 0 | 3824 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3825 | `		return PH7_OK;` |
|      - | 3826 | `	}` |
|      - | 3827 | `	/* Format the string */` |
|     51 | 3828 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 3829 | `	/* Return the length of the outputted string */` |
|     51 | 3830 | `	ph7_result_int64(pCtx,nCounter);` |
|     51 | 3831 | `	return PH7_OK;` |
|     27 | 3832 |  |
|      - | 3833 | `/*` |
|      - | 3834 | ` * int vprintf(string $format,array $args)` |
|      - | 3835 | ` *  Output a formatted string.` |
|      - | 3836 | ` * Parameters` |
|      - | 3837 | ` *  $format` |
|      - | 3838 | ` *   See sprintf() for a description of format.` |
|      - | 3839 | ` * Return` |
|      - | 3840 | ` *  The length of the outputted string.` |
|      - | 3841 | ` */` |
|      2 | 3842 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3843 |  |
|      3 | 3844 | `	ph7_int64 nCounter = 0;` |
|      - | 3845 | `	const char *zFormat;` |
|      - | 3846 | `	ph7_hashmap *pMap;` |
|      - | 3847 | `	SySet sArg;` |
|      - | 3848 | `	int nLen,n;` |
|      3 | 3849 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 3850 | `		/* Missing/Invalid arguments,return 0 */` |
|    ! 0 | 3851 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3852 | `		return PH7_OK;` |
|      - | 3853 | `	}` |
|      - | 3854 | `	/* Extract the string format */` |
|      3 | 3855 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3856 | `	if( nLen < 1 ){` |
|      - | 3857 | `		/* Empty string */` |
|    ! 0 | 3858 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3859 | `		return PH7_OK;` |
|      - | 3860 | `	}` |
|      - | 3861 | `	/* Point to the hashmap */` |
|      3 | 3862 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 3863 | `	/* Extract arguments from the hashmap */` |
|      3 | 3864 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 3865 | `	/* Format the string */` |
|      3 | 3866 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 3867 | `	/* Return the length of the outputted string */` |
|      3 | 3868 | `	ph7_result_int64(pCtx,nCounter);` |
|      - | 3869 | `	/* Release the container */` |
|      3 | 3870 | `	SySetRelease(&sArg);` |
|      3 | 3871 | `	return PH7_OK;` |
|      2 | 3872 |  |
|      - | 3873 | `/*` |
|      - | 3874 | ` * int vsprintf(string $format,array $args)` |
|      - | 3875 | ` *  Output a formatted string.` |
|      - | 3876 | ` * Parameters` |
|      - | 3877 | ` *  $format` |
|      - | 3878 | ` *   See sprintf() for a description of format.` |
|      - | 3879 | ` * Return` |
|      - | 3880 | ` *  A string produced according to the formatting string format.` |
|      - | 3881 | ` */` |
|     10 | 3882 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3883 |  |
|      - | 3884 | `	const char *zFormat;` |
|      - | 3885 | `	ph7_hashmap *pMap;` |
|      - | 3886 | `	SySet sArg;` |
|      - | 3887 | `	int nLen,n;` |
|     11 | 3888 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 3889 | `		/* Missing/Invalid arguments,return the empty string */` |
|      5 | 3890 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 3891 | `		return PH7_OK;` |
|      - | 3892 | `	}` |
|      - | 3893 | `	/* Extract the string format */` |
|      7 | 3894 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 3895 | `	if( nLen < 1 ){` |
|      - | 3896 | `		/* Empty string */` |
|    ! 0 | 3897 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3898 | `		return PH7_OK;` |
|      - | 3899 | `	}` |
|      - | 3900 | `	/* Point to hashmap */` |
|      7 | 3901 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 3902 | `	/* Extract arguments from the hashmap */` |
|      7 | 3903 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 3904 | `	/* Format the string */` |
|      7 | 3905 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),0,TRUE);` |
|      - | 3906 | `	/* Release the container */` |
|      7 | 3907 | `	SySetRelease(&sArg);` |
|      7 | 3908 | `	return PH7_OK;` |
|      6 | 3909 |  |
|      - | 3910 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 3911 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 3912 | `/*` |
|      - | 3913 | ` * Symisc eXtension.` |
|      - | 3914 | ` * string size_format(int64 $size)` |
|      - | 3915 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 3916 | ` *  Example:` |
|      - | 3917 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 3918 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 3919 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 3920 | ` * Parameter` |
|      - | 3921 | ` *  $size` |
|      - | 3922 | ` *    Entity size in bytes.` |
|      - | 3923 | ` * Return` |
|      - | 3924 | ` *   Formatted string representation of the given size.` |
|      - | 3925 | ` */` |
|     24 | 3926 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3927 |  |
|      - | 3928 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 3929 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 3930 | `	sxi32 nRest,i_32;` |
|      - | 3931 | `	ph7_int64 iSize;` |
|     25 | 3932 | `	int c = -1; /* index in zUnit[] */` |
|      - | 3933 |  |
|     25 | 3934 | `	if( nArg < 1 ){` |
|      - | 3935 | `		/* Missing argument,return the empty string */` |
|      3 | 3936 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3937 | `		return PH7_OK;` |
|      - | 3938 | `	}` |
|      - | 3939 | `	/* Extract the given size */` |
|     23 | 3940 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 3941 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 3942 | `		/* Don't bother formatting,return immediately */` |
|      5 | 3943 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 3944 | `		return PH7_OK;` |
|      - | 3945 | `	}` |
|     19 | 3946 | `	for(;;){` |
|     39 | 3947 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 3948 | `		iSize >>= 10;` |
|     39 | 3949 | `		c++;` |
|     39 | 3950 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 3951 | `			break;` |
|      - | 3952 | `		}` |
|      1 | 3953 | `	}` |
|     19 | 3954 | `	nRest /= 100;` |
|     19 | 3955 | `	if( nRest > 9 ){` |
|    ! 0 | 3956 | `		nRest = 9;` |
|    ! 0 | 3957 | `	}` |
|     19 | 3958 | `	if( iSize > 999 ){` |
|    ! 0 | 3959 | `		c++;` |
|    ! 0 | 3960 | `		nRest = 9;` |
|    ! 0 | 3961 | `		iSize = 0;` |
|    ! 0 | 3962 | `	}` |
|     19 | 3963 | `	i_32 = (sxi32)iSize;` |
|      - | 3964 | `	/* Format */` |
|     19 | 3965 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 3966 | `	return PH7_OK;` |
|     13 | 3967 |  |
|      - | 3968 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 3969 | `/*` |
|      - | 3970 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 3971 | ` *   Calculate the md5 hash of a string.` |
|      - | 3972 | ` * Parameter` |
|      - | 3973 | ` *  $str` |
|      - | 3974 | ` *   Input string` |
|      - | 3975 | ` * $raw_output` |
|      - | 3976 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 3977 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 3978 | ` * Return` |
|      - | 3979 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 3980 | ` */` |
|     10 | 3981 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3982 |  |
|      - | 3983 | `	unsigned char zDigest[16];` |
|     11 | 3984 | `	int raw_output = FALSE;` |
|      - | 3985 | `	const void *pIn;` |
|      - | 3986 | `	int nLen;` |
|     11 | 3987 | `	if( nArg < 1 ){` |
|      - | 3988 | `		/* Missing arguments,return the empty string */` |
|      3 | 3989 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3990 | `		return PH7_OK;` |
|      - | 3991 | `	}` |
|      - | 3992 | `	/* Extract the input string */` |
|      9 | 3993 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 3994 | `	if( nLen < 1 ){` |
|      - | 3995 | `		/* Empty string */` |
|    ! 0 | 3996 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3997 | `		return PH7_OK;` |
|      - | 3998 | `	}` |
|      9 | 3999 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      3 | 4000 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 | 4001 | `	}` |
|      - | 4002 | `	/* Compute the MD5 digest */` |
|      9 | 4003 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|      9 | 4004 | `	if( raw_output ){` |
|      - | 4005 | `		/* Output raw digest */` |
|      3 | 4006 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      2 | 4007 | `	}else{` |
|      - | 4008 | `		/* Perform a binary to hex conversion */` |
|      7 | 4009 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4010 | `	}` |
|      9 | 4011 | `	return PH7_OK;` |
|      6 | 4012 |  |
|      - | 4013 | `/*` |
|      - | 4014 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 4015 | ` *   Calculate the sha1 hash of a string.` |
|      - | 4016 | ` * Parameter` |
|      - | 4017 | ` *  $str` |
|      - | 4018 | ` *   Input string` |
|      - | 4019 | ` * $raw_output` |
|      - | 4020 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4021 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4022 | ` * Return` |
|      - | 4023 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 4024 | ` */` |
|      8 | 4025 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4026 |  |
|      - | 4027 | `	unsigned char zDigest[20];` |
|      9 | 4028 | `	int raw_output = FALSE;` |
|      - | 4029 | `	const void *pIn;` |
|      - | 4030 | `	int nLen;` |
|      9 | 4031 | `	if( nArg < 1 ){` |
|      - | 4032 | `		/* Missing arguments,return the empty string */` |
|      3 | 4033 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4034 | `		return PH7_OK;` |
|      - | 4035 | `	}` |
|      - | 4036 | `	/* Extract the input string */` |
|      7 | 4037 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4038 | `	if( nLen < 1 ){` |
|      - | 4039 | `		/* Empty string */` |
|    ! 0 | 4040 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4041 | `		return PH7_OK;` |
|      - | 4042 | `	}` |
|      7 | 4043 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      3 | 4044 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 | 4045 | `	}` |
|      - | 4046 | `	/* Compute the SHA1 digest */` |
|      7 | 4047 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|      7 | 4048 | `	if( raw_output ){` |
|      - | 4049 | `		/* Output raw digest */` |
|      3 | 4050 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      2 | 4051 | `	}else{` |
|      - | 4052 | `		/* Perform a binary to hex conversion */` |
|      5 | 4053 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4054 | `	}` |
|      7 | 4055 | `	return PH7_OK;` |
|      5 | 4056 |  |
|      - | 4057 | `/*` |
|      - | 4058 | ` * int64 crc32(string $str)` |
|      - | 4059 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 4060 | ` * Parameter` |
|      - | 4061 | ` *  $str` |
|      - | 4062 | ` *   Input string` |
|      - | 4063 | ` * Return` |
|      - | 4064 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 4065 | ` */` |
|      4 | 4066 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4067 |  |
|      - | 4068 | `	const void *pIn;` |
|      - | 4069 | `	sxu32 nCRC;` |
|      - | 4070 | `	int nLen;` |
|      5 | 4071 | `	if( nArg < 1 ){` |
|      - | 4072 | `		/* Missing arguments,return 0 */` |
|      3 | 4073 | `		ph7_result_int(pCtx,0);` |
|      3 | 4074 | `		return PH7_OK;` |
|      - | 4075 | `	}` |
|      - | 4076 | `	/* Extract the input string */` |
|      3 | 4077 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4078 | `	if( nLen < 1 ){` |
|      - | 4079 | `		/* Empty string */` |
|    ! 0 | 4080 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4081 | `		return PH7_OK;` |
|      - | 4082 | `	}` |
|      - | 4083 | `	/* Calculate the sum */` |
|      3 | 4084 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 4085 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 4086 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 4087 | `	return PH7_OK;` |
|      3 | 4088 |  |
|      - | 4089 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 4090 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4091 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 4092 | `/*` |
|      - | 4093 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 4094 |  |
|      - | 4095 | ` */` |
|      4 | 4096 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 4097 | `	const char *zInput, /* Raw input */` |
|      - | 4098 | `	int nByte,  /* Input length */` |
|      - | 4099 | `	int delim,  /* Delimiter */` |
|      - | 4100 | `	int encl,   /* Enclosure */` |
|      - | 4101 | `	int escape,  /* Escape character */` |
|      - | 4102 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 4103 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 4104 | `	)` |
|      1 | 4105 |  |
|      5 | 4106 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 4107 | `	const char *zIn = zInput;` |
|      - | 4108 | `	const char *zPtr;` |
|      - | 4109 | `	int isEnc;` |
|      - | 4110 | `	/* Start processing */` |
|      8 | 4111 | `	for(;;){` |
|     17 | 4112 | `		if( zIn >= zEnd ){` |
|      - | 4113 | `			/* No more input to process */` |
|      5 | 4114 | `			break;` |
|      - | 4115 | `		}` |
|     13 | 4116 | `		isEnc = 0;` |
|     13 | 4117 | `		zPtr = zIn;` |
|      - | 4118 | `		/* Find the first delimiter */` |
|     27 | 4119 | `		while( zIn < zEnd ){` |
|     23 | 4120 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 4121 | `				/* Delimiter found,break imediately */` |
|      5 | 4122 | `				break;` |
|     15 | 4123 | `			}else if( zIn[0] == encl ){` |
|      - | 4124 | `				/* Inside enclosure? */` |
|    ! 0 | 4125 | `				isEnc = !isEnc;` |
|     15 | 4126 | `			}else if( zIn[0] == escape ){` |
|      - | 4127 | `				/* Escape sequence */` |
|    ! 0 | 4128 | `				zIn++;` |
|    ! 0 | 4129 | `			}` |
|      - | 4130 | `			/* Advance the cursor */` |
|     15 | 4131 | `			zIn++;` |
|      1 | 4132 | `		}` |
|     13 | 4133 | `		if( zIn > zPtr ){` |
|     13 | 4134 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 4135 | `			sxi32 rc;` |
|      - | 4136 | `			/* Invoke the supllied callback */` |
|     13 | 4137 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 4138 | `				zPtr++;` |
|    ! 0 | 4139 | `				nByteChunk-=2;` |
|    ! 0 | 4140 | `			}` |
|     13 | 4141 | `			if( nByteChunk > 0 ){` |
|     13 | 4142 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 4143 | `				if( rc == SXERR_ABORT ){` |
|      - | 4144 | `					/* User callback request an operation abort */` |
|    ! 0 | 4145 | `					break;` |
|      - | 4146 | `				}` |
|      6 | 4147 | `			}` |
|      6 | 4148 | `		}` |
|      - | 4149 | `		/* Ignore trailing delimiter */` |
|     21 | 4150 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 4151 | `			zIn++;` |
|      1 | 4152 | `		}` |
|      1 | 4153 | `	}` |
|      5 | 4154 | `	return SXRET_OK;` |
|      1 | 4155 |  |
|      - | 4156 | `/*` |
|      - | 4157 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 4158 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 4159 | ` * argument to this callback.` |
|      - | 4160 | ` */` |
|     12 | 4161 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 4162 |  |
|     13 | 4163 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 4164 | `	ph7_value sEntry;` |
|      - | 4165 | `	SyString sToken;` |
|      - | 4166 | `	/* Insert the token in the given array */` |
|     13 | 4167 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 4168 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 4169 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 4170 | `	if( sToken.nByte < 1){` |
|    ! 0 | 4171 | `		return SXRET_OK;` |
|      - | 4172 | `	}` |
|     13 | 4173 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 4174 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 4175 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 4176 | `	return SXRET_OK;` |
|      7 | 4177 |  |
|      - | 4178 | `/*` |
|      - | 4179 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 4180 | ` *  Parse a CSV string into an array.` |
|      - | 4181 | ` * Parameters` |
|      - | 4182 | ` *  $input` |
|      - | 4183 | ` *   The string to parse.` |
|      - | 4184 | ` *  $delimiter` |
|      - | 4185 | ` *   Set the field delimiter (one character only).` |
|      - | 4186 | ` *  $enclosure` |
|      - | 4187 | ` *   Set the field enclosure character (one character only).` |
|      - | 4188 | ` *  $escape` |
|      - | 4189 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 4190 | ` * Return` |
|      - | 4191 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 4192 | ` */` |
|      4 | 4193 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4194 |  |
|      - | 4195 | `	const char *zInput,*zPtr;` |
|      - | 4196 | `	ph7_value *pArray;` |
|      5 | 4197 | `	int delim  = ',';   /* Delimiter */` |
|      5 | 4198 | `	int encl   = '"' ;  /* Enclosure */` |
|      5 | 4199 | `	int escape = '\\';  /* Escape character */` |
|      - | 4200 | `	int nLen;` |
|      5 | 4201 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4202 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 4203 | `		ph7_result_null(pCtx);` |
|      3 | 4204 | `		return PH7_OK;` |
|      - | 4205 | `	}` |
|      - | 4206 | `	/* Extract the raw input */` |
|      3 | 4207 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4208 | `	if( nArg > 1 ){` |
|      - | 4209 | `		int i;` |
|      3 | 4210 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 4211 | `			/* Extract the delimiter */` |
|      3 | 4212 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 4213 | `			if( i > 0 ){` |
|      3 | 4214 | `				delim = zPtr[0];` |
|      1 | 4215 | `			}` |
|      1 | 4216 | `		}` |
|      3 | 4217 | `		if( nArg > 2 ){` |
|      3 | 4218 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 4219 | `				/* Extract the enclosure */` |
|      3 | 4220 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 4221 | `				if( i > 0 ){` |
|      3 | 4222 | `					encl = zPtr[0];` |
|      1 | 4223 | `				}` |
|      1 | 4224 | `			}` |
|      3 | 4225 | `			if( nArg > 3 ){` |
|      3 | 4226 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 4227 | `					/* Extract the escape character */` |
|      3 | 4228 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 4229 | `					if( i > 0 ){` |
|      3 | 4230 | `						escape = zPtr[0];` |
|      1 | 4231 | `					}` |
|      1 | 4232 | `				}` |
|      1 | 4233 | `			}` |
|      1 | 4234 | `		}` |
|      1 | 4235 | `	}` |
|      - | 4236 | `	/* Create our array */` |
|      3 | 4237 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4238 | `	if( pArray == 0 ){` |
|    ! 0 | 4239 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4240 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4241 | `		return PH7_OK;` |
|      - | 4242 | `	}` |
|      - | 4243 | `	/* Parse the raw input */` |
|      3 | 4244 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 4245 | `	/* Return the freshly created array */` |
|      3 | 4246 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4247 | `	return PH7_OK;` |
|      3 | 4248 |  |
|      - | 4249 | `/*` |
|      - | 4250 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 4251 | ` * container.` |
|      - | 4252 | ` * Refer to [strip_tags()].` |
|      - | 4253 | ` */` |
|     10 | 4254 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 4255 |  |
|     11 | 4256 | `	const char *zEnd = &zTag[nByte];` |
|      - | 4257 | `	const char *zPtr;` |
|      - | 4258 | `	SyString sEntry;` |
|      - | 4259 | `	/* Strip tags */` |
|     10 | 4260 | `	for(;;){` |
|     45 | 4261 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 4262 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 4263 | `				zTag++;` |
|      1 | 4264 | `		}` |
|     21 | 4265 | `		if( zTag >= zEnd ){` |
|     11 | 4266 | `			break;` |
|      - | 4267 | `		}` |
|     11 | 4268 | `		zPtr = zTag;` |
|      - | 4269 | `		/* Delimit the tag */` |
|     25 | 4270 | `		while(zTag < zEnd ){` |
|     25 | 4271 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 4272 | `				/* UTF-8 stream */` |
|      3 | 4273 | `				zTag++;` |
|      5 | 4274 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 4275 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 4276 | `				break;` |
|    ! 0 | 4277 | `			}else{` |
|     13 | 4278 | `				zTag++;` |
|      - | 4279 | `			}` |
|      1 | 4280 | `		}` |
|     11 | 4281 | `		if( zTag > zPtr ){` |
|      - | 4282 | `			/* Perform the insertion */` |
|     11 | 4283 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 4284 | `			SyStringFullTrim(&sEntry);` |
|     11 | 4285 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 4286 | `		}` |
|      - | 4287 | `		/* Jump the trailing '>' */` |
|     11 | 4288 | `		zTag++;` |
|      1 | 4289 | `	}` |
|     11 | 4290 | `	return SXRET_OK;` |
|      1 | 4291 |  |
|      - | 4292 | `/*` |
|      - | 4293 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 4294 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 4295 | ` * Refer to [strip_tags()].` |
|      - | 4296 | ` */` |
|     36 | 4297 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 4298 |  |
|     37 | 4299 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 4300 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 4301 | `		SyString sTag;` |
|     85 | 4302 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 4303 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 4304 | `			zTag++;` |
|      1 | 4305 | `		}` |
|      - | 4306 | `		/* Delimit the tag */` |
|     25 | 4307 | `		zCur = zTag;` |
|     77 | 4308 | `		while(zTag < zEnd ){` |
|     77 | 4309 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 4310 | `				/* UTF-8 stream */` |
|      5 | 4311 | `				zTag++;` |
|      9 | 4312 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 4313 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 4314 | `				break;` |
|    ! 0 | 4315 | `			}else{` |
|     49 | 4316 | `				zTag++;` |
|      - | 4317 | `			}` |
|      1 | 4318 | `		}` |
|     25 | 4319 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 4320 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 4321 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 4322 | `		if( sTag.nByte > 0 ){` |
|      - | 4323 | `			SyString *aEntry,*pEntry;` |
|      - | 4324 | `			sxi32 rc;` |
|      - | 4325 | `			sxu32 n;` |
|      - | 4326 | `			/* Perform the lookup */` |
|     25 | 4327 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 4328 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 4329 | `				pEntry = &aEntry[n];` |
|      - | 4330 | `				/* Do the comparison */` |
|     25 | 4331 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 4332 | `				if( !rc ){` |
|     21 | 4333 | `					return SXRET_OK;` |
|      - | 4334 | `				}` |
|      3 | 4335 | `			}` |
|      2 | 4336 | `		}` |
|      2 | 4337 | `	}` |
|      - | 4338 | `	/* No such tag */` |
|     17 | 4339 | `	return SXERR_NOTFOUND;` |
|     19 | 4340 |  |
|      - | 4341 | `/*` |
|      - | 4342 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 4343 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 4344 | ` * Refer to [strip_tags()].` |
|      - | 4345 | ` */` |
|     16 | 4346 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 4347 |  |
|     17 | 4348 | `	const char *zEnd = &zIn[nByte];` |
|      - | 4349 | `	const char *zPtr,*zTag;` |
|      - | 4350 | `	SySet sSet;` |
|      - | 4351 | `	/* initialize the set of allowed tags */` |
|     17 | 4352 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 4353 | `	if( nTaglen > 0 ){` |
|      - | 4354 | `		/* Set of allowed tags */` |
|     11 | 4355 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 4356 | `	}` |
|      - | 4357 | `	/* Set the empty string */` |
|     17 | 4358 | `	ph7_result_string(pCtx,"",0);` |
|      - | 4359 | `	/* Start processing */` |
|     26 | 4360 | `	for(;;){` |
|     53 | 4361 | `		if(zIn >= zEnd){` |
|      - | 4362 | `			/* No more input to process */` |
|     15 | 4363 | `			break;` |
|      - | 4364 | `		}` |
|     39 | 4365 | `		zPtr = zIn;` |
|      - | 4366 | `		/* Find a tag */` |
|    133 | 4367 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 4368 | `			zIn++;` |
|      1 | 4369 | `		}` |
|     39 | 4370 | `		if( zIn > zPtr ){` |
|      - | 4371 | `			/* Consume raw input */` |
|     21 | 4372 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 4373 | `		}` |
|      - | 4374 | `		/* Ignore trailing null bytes */` |
|     39 | 4375 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 4376 | `			zIn++;` |
|    ! 0 | 4377 | `		}` |
|     39 | 4378 | `		if(zIn >= zEnd){` |
|      - | 4379 | `			/* No more input to process */` |
|      3 | 4380 | `			break;` |
|      - | 4381 | `		}` |
|     37 | 4382 | `		if( zIn[0] == '<' ){` |
|      - | 4383 | `			sxi32 rc;` |
|     37 | 4384 | `			zTag = zIn++;` |
|      - | 4385 | `			/* Delimit the tag */` |
|    127 | 4386 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 4387 | `				zIn++;` |
|      1 | 4388 | `			}` |
|     37 | 4389 | `			if( zIn < zEnd ){` |
|     37 | 4390 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 4391 | `			}` |
|      - | 4392 | `			/* Query the set */` |
|     37 | 4393 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 4394 | `			if( rc == SXRET_OK ){` |
|      - | 4395 | `				/* Keep the tag */` |
|     21 | 4396 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 4397 | `			}` |
|     18 | 4398 | `		}` |
|      1 | 4399 | `	}` |
|      - | 4400 | `	/* Cleanup */` |
|     17 | 4401 | `	SySetRelease(&sSet);` |
|     17 | 4402 | `	return SXRET_OK;` |
|      1 | 4403 |  |
|      - | 4404 | `/*` |
|      - | 4405 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 4406 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 4407 | ` * Parameters` |
|      - | 4408 | ` *  $str` |
|      - | 4409 | ` *  The input string.` |
|      - | 4410 | ` * $allowable_tags` |
|      - | 4411 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 4412 | ` * Return` |
|      - | 4413 | ` *  Returns the stripped string.` |
|      - | 4414 | ` */` |
|     16 | 4415 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4416 |  |
|     17 | 4417 | `	const char *zTaglist = 0;` |
|      - | 4418 | `	const char *zString;` |
|     17 | 4419 | `	int nTaglen = 0;` |
|      - | 4420 | `	int nLen;` |
|     17 | 4421 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4422 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 4423 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4424 | `		return PH7_OK;` |
|      - | 4425 | `	}` |
|      - | 4426 | `	/* Point to the raw string */` |
|     15 | 4427 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 4428 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 4429 | `		/* Allowed tag */` |
|     11 | 4430 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 4431 | `	}` |
|      - | 4432 | `	/* Process input */` |
|     15 | 4433 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 4434 | `	return PH7_OK;` |
|      9 | 4435 |  |
|      - | 4436 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 4437 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4438 | `/*` |
|      - | 4439 | ` * string str_shuffle(string $str)` |
|      - | 4440 |  |
|      - | 4441 | ` *  Randomly shuffles a string.` |
|      - | 4442 | ` * Parameters` |
|      - | 4443 | ` *  $str` |
|      - | 4444 | ` *   The input string.` |
|      - | 4445 | ` * Return` |
|      - | 4446 | ` *  Returns the shuffled string.` |
|      - | 4447 | ` */` |
|     12 | 4448 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4449 |  |
|      - | 4450 | `	const char *zString;` |
|      - | 4451 | `	int nLen,i,c;` |
|      - | 4452 | `	sxu32 iR;` |
|     13 | 4453 | `	if( nArg < 1 ){` |
|      - | 4454 | `		/* Missing arguments,return the empty string */` |
|      3 | 4455 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4456 | `		return PH7_OK;` |
|      - | 4457 | `	}` |
|      - | 4458 | `	/* Extract the target string */` |
|     11 | 4459 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 4460 | `	if( nLen < 1 ){` |
|      - | 4461 | `		/* Nothing to shuffle */` |
|      3 | 4462 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4463 | `		return PH7_OK;` |
|      - | 4464 | `	}` |
|      - | 4465 | `	/* Shuffle the string */` |
|     43 | 4466 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 4467 | `		/* Generate a random number first */` |
|     35 | 4468 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 4469 | `		/* Extract a random offset */` |
|     35 | 4470 | `		c = zString[iR % nLen];` |
|      - | 4471 | `		/* Append it */` |
|     35 | 4472 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 4473 | `	}` |
|      9 | 4474 | `	return PH7_OK;` |
|      7 | 4475 |  |
|      - | 4476 | `/*` |
|      - | 4477 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 4478 | ` *  Convert a string to an array.` |
|      - | 4479 | ` * Parameters` |
|      - | 4480 | ` * $string` |
|      - | 4481 | ` *  The input string.` |
|      - | 4482 | ` * $split_length` |
|      - | 4483 | ` *  Maximum length of the chunk.` |
|      - | 4484 | ` * Return` |
|      - | 4485 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 4486 | ` *  except possibly the last one which may be shorter.` |
|      - | 4487 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 4488 | ` *  as the first (and only) array element.` |
|      - | 4489 | ` *  An empty string returns an empty array.` |
|      - | 4490 | ` * Errors` |
|      - | 4491 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 4492 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 4493 | ` *  ValueError if $split_length is less than 1.` |
|      - | 4494 | ` */` |
|     28 | 4495 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4496 |  |
|      - | 4497 | `	const char *zString,*zEnd;` |
|      - | 4498 | `	ph7_value *pArray,*pValue;` |
|      - | 4499 | `	int split_len;` |
|      - | 4500 | `	int nLen;` |
|     30 | 4501 | `	if( nArg < 1 ){` |
|      4 | 4502 | `		return PH7_VmThrowException(pCtx,` |
|      - | 4503 | `			"ArgumentCountError",` |
|      - | 4504 | `			"str_split() expects at least 1 argument, %d given",` |
|      1 | 4505 | `			nArg` |
|      - | 4506 | `			);` |
|      - | 4507 | `	}` |
|      - | 4508 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     50 | 4509 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     38 | 4510 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 4511 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 4512 | `		return PH7_VmThrowException(pCtx,` |
|      - | 4513 | `			"TypeError",` |
|      - | 4514 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 4515 | `			ph7_type_name(apArg[0])` |
|      - | 4516 | `			);` |
|      - | 4517 | `	}` |
|      - | 4518 | `	/* Point to the target string */` |
|     26 | 4519 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     26 | 4520 | `	split_len = (int)sizeof(char);` |
|     26 | 4521 | `	if( nArg > 1 ){` |
|      - | 4522 | `		/* Split length */` |
|     16 | 4523 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     16 | 4524 | `		if( split_len < 1 ){` |
|      5 | 4525 | `			return PH7_VmThrowException(pCtx,` |
|      - | 4526 | `				"ValueError",` |
|      - | 4527 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 4528 | `				);` |
|      - | 4529 | `		}` |
|     11 | 4530 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 4531 | `			split_len = nLen;` |
|      1 | 4532 | `		}` |
|      5 | 4533 | `	}` |
|      - | 4534 | `	/* Create the array and the scalar value */` |
|     21 | 4535 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 4536 | `	/*Chunk value */` |
|     21 | 4537 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 4538 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 4539 | `		/* Return FALSE */` |
|    ! 0 | 4540 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4541 | `		return PH7_OK;` |
|      - | 4542 | `	}` |
|      - | 4543 | `	/* Point to the end of the string */` |
|     21 | 4544 | `	zEnd = &zString[nLen];` |
|      - | 4545 | `	/* Perform the requested operation */` |
|     48 | 4546 | `	for(;;){` |
|      - | 4547 | `		int nMax;` |
|     59 | 4548 | `		if( zString >= zEnd ){` |
|      - | 4549 | `			/* No more input to process */` |
|     21 | 4550 | `			break;` |
|      - | 4551 | `		}` |
|     39 | 4552 | `		nMax = (int)(zEnd-zString);` |
|     39 | 4553 | `		if( nMax < split_len ){` |
|      3 | 4554 | `			split_len = nMax;` |
|      1 | 4555 | `		}` |
|      - | 4556 | `		/* Copy the current chunk */` |
|     39 | 4557 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 4558 | `		/* Insert it */` |
|     39 | 4559 | `		ph7_array_add_elem(pArray,0,pValue); /* Will make it's own copy */` |
|      - | 4560 | `		/* reset the string cursor */` |
|     39 | 4561 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 4562 | `		/* Update position */` |
|     39 | 4563 | `		zString += split_len;` |
|      1 | 4564 | `	}` |
|      - | 4565 | `	/*` |
|      - | 4566 | `	 * Return the array.` |
|      - | 4567 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 4568 | `	 * upon we return from this function.` |
|      - | 4569 | `	 */` |
|     21 | 4570 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 4571 | `	return PH7_OK;` |
|     16 | 4572 |  |
|      - | 4573 | `/*` |
|      - | 4574 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 4575 | ` * Refer to [strspn()].` |
|      - | 4576 | ` */` |
|     28 | 4577 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 4578 |  |
|     29 | 4579 | `	const char *zIn = *pzIn;` |
|      - | 4580 | `	const char *zPtr;` |
|      - | 4581 | `	/* Ignore leading white spaces */` |
|     29 | 4582 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 4583 | `		zIn++;` |
|    ! 0 | 4584 | `	}` |
|     29 | 4585 | `	if( zIn >= zEnd ){` |
|      - | 4586 | `		/* End of input */` |
|    ! 0 | 4587 | `		return SXERR_EOF;` |
|      - | 4588 | `	}` |
|     29 | 4589 | `	zPtr = zIn;` |
|      - | 4590 | `	/* Extract the token */` |
|    201 | 4591 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 4592 | `		zIn++;` |
|      1 | 4593 | `	}` |
|     29 | 4594 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 4595 | `	/* Synchronize pointers */` |
|     29 | 4596 | `	*pzIn = zIn;` |
|      - | 4597 | `	/* Return to the caller */` |
|     29 | 4598 | `	return SXRET_OK;` |
|     15 | 4599 |  |
|      - | 4600 | `/*` |
|      - | 4601 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 4602 | ` * return the longest match.` |
|      - | 4603 | ` * Refer to [strspn()].` |
|      - | 4604 | ` */` |
|     18 | 4605 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 4606 |  |
|     19 | 4607 | `	const char *zEnd = &zString[nLen];` |
|     19 | 4608 | `	const char *zIn = zString;` |
|      - | 4609 | `	int i,c;` |
|     45 | 4610 | `	for(;;){` |
|     91 | 4611 | `		if( zString >= zEnd ){` |
|      7 | 4612 | `			break;` |
|      - | 4613 | `		}` |
|      - | 4614 | `		/* Extract current character */` |
|     85 | 4615 | `		c = zString[0];` |
|      - | 4616 | `		/* Perform the lookup */` |
|    383 | 4617 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 4618 | `			if( c == zMask[i] ){` |
|      - | 4619 | `				/* Character found */` |
|     73 | 4620 | `				break;` |
|      - | 4621 | `			}` |
|    150 | 4622 | `		}` |
|     85 | 4623 | `		if( i >= nMaskLen ){` |
|      - | 4624 | `			/* Character not in the current mask,break immediately */` |
|     13 | 4625 | `			break;` |
|      - | 4626 | `		}` |
|      - | 4627 | `		/* Advance cursor */` |
|     73 | 4628 | `		zString++;` |
|      1 | 4629 | `	}` |
|      - | 4630 | `	/* Longest match */` |
|     19 | 4631 | `	return (int)(zString-zIn);` |
|      1 | 4632 |  |
|      - | 4633 | `/*` |
|      - | 4634 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 4635 | ` * Refer to [strcspn()].` |
|      - | 4636 | ` */` |
|     10 | 4637 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 4638 |  |
|     11 | 4639 | `	const char *zEnd = &zString[nLen];` |
|     11 | 4640 | `	const char *zIn = zString;` |
|      - | 4641 | `	int i,c;` |
|     12 | 4642 | `	for(;;){` |
|     25 | 4643 | `		if( zString >= zEnd ){` |
|      3 | 4644 | `			break;` |
|      - | 4645 | `		}` |
|      - | 4646 | `		/* Extract current character */` |
|     23 | 4647 | `		c = zString[0];` |
|      - | 4648 | `		/* Perform the lookup */` |
|     51 | 4649 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 4650 | `			if( c == zMask[i] ){` |
|      9 | 4651 | `				break;` |
|      - | 4652 | `			}` |
|     15 | 4653 | `		}` |
|     23 | 4654 | `		if( i < nMaskLen ){` |
|      - | 4655 | `			/* Character in the current mask,break immediately */` |
|      9 | 4656 | `			break;` |
|      - | 4657 | `		}` |
|      - | 4658 | `		/* Advance cursor */` |
|     15 | 4659 | `		zString++;` |
|      1 | 4660 | `	}` |
|      - | 4661 | `	/* Longest match */` |
|     11 | 4662 | `	return (int)(zString-zIn);` |
|      1 | 4663 |  |
|      - | 4664 | `/*` |
|      - | 4665 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 4666 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 4667 | ` *  of characters contained within a given mask.` |
|      - | 4668 | ` * Parameters` |
|      - | 4669 | ` * $str` |
|      - | 4670 | ` *  The input string.` |
|      - | 4671 | ` * $mask` |
|      - | 4672 | ` *  The list of allowable characters.` |
|      - | 4673 | ` * $start` |
|      - | 4674 | ` *  The position in subject to start searching.` |
|      - | 4675 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 4676 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 4677 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 4678 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 4679 | ` *  start'th position from the end of subject.` |
|      - | 4680 | ` * $length` |
|      - | 4681 | ` *  The length of the segment from subject to examine.` |
|      - | 4682 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 4683 | ` *  characters after the starting position.` |
|      - | 4684 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 4685 | ` *  position up to length characters from the end of subject.` |
|      - | 4686 | ` * Return` |
|      - | 4687 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 4688 | ` * in mask.` |
|      - | 4689 | ` */` |
|     26 | 4690 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4691 |  |
|      - | 4692 | `	const char *zString,*zMask,*zEnd;` |
|      - | 4693 | `	int iMasklen,iLen;` |
|      - | 4694 | `	SyString sToken;` |
|     27 | 4695 | `	int iCount = 0;` |
|      - | 4696 | `	int rc;` |
|     27 | 4697 | `	if( nArg < 2 ){` |
|      - | 4698 | `		/* Missing agruments,return zero */` |
|      3 | 4699 | `		ph7_result_int(pCtx,0);` |
|      3 | 4700 | `		return PH7_OK;` |
|      - | 4701 | `	}` |
|      - | 4702 | `	/* Extract the target string */` |
|     25 | 4703 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 4704 | `	/* Extract the mask */` |
|     25 | 4705 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 4706 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 4707 | `		/* Nothing to process,return zero */` |
|      7 | 4708 | `		ph7_result_int(pCtx,0);` |
|      7 | 4709 | `		return PH7_OK;` |
|      - | 4710 | `	}` |
|     19 | 4711 | `	if( nArg > 2 ){` |
|      - | 4712 | `		int nOfft;` |
|      - | 4713 | `		/* Extract the offset */` |
|      9 | 4714 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 4715 | `		if( nOfft < 0 ){` |
|    ! 0 | 4716 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 4717 | `			if( zBase > zString ){` |
|    ! 0 | 4718 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 4719 | `				zString = zBase;` |
|    ! 0 | 4720 | `			}else{` |
|      - | 4721 | `				/* Invalid offset */` |
|    ! 0 | 4722 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 4723 | `				return PH7_OK;` |
|      - | 4724 | `			}` |
|    ! 0 | 4725 | `		}else{` |
|      9 | 4726 | `			if( nOfft >= iLen ){` |
|      - | 4727 | `				/* Invalid offset */` |
|    ! 0 | 4728 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 4729 | `				return PH7_OK;` |
|    ! 0 | 4730 | `			}else{` |
|      - | 4731 | `				/* Update offset */` |
|      9 | 4732 | `				zString += nOfft;` |
|      9 | 4733 | `				iLen -= nOfft;` |
|      - | 4734 | `			}` |
|      - | 4735 | `		}` |
|      9 | 4736 | `		if( nArg > 3 ){` |
|      - | 4737 | `			int iUserlen;` |
|      - | 4738 | `			/* Extract the desired length */` |
|      9 | 4739 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 4740 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 4741 | `				iLen = iUserlen;` |
|      2 | 4742 | `			}` |
|      4 | 4743 | `		}` |
|      4 | 4744 | `	}` |
|      - | 4745 | `	/* Point to the end of the string */` |
|     19 | 4746 | `	zEnd = &zString[iLen];` |
|      - | 4747 | `	/* Extract the first non-space token */` |
|     19 | 4748 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 4749 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 4750 | `		/* Compare against the current mask */` |
|     19 | 4751 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 4752 | `	}` |
|      - | 4753 | `	/* Longest match */` |
|     19 | 4754 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 4755 | `	return PH7_OK;` |
|     14 | 4756 |  |
|      - | 4757 | `/*` |
|      - | 4758 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 4759 | ` *  Find length of initial segment not matching mask.` |
|      - | 4760 | ` * Parameters` |
|      - | 4761 | ` * $str` |
|      - | 4762 | ` *  The input string.` |
|      - | 4763 | ` * $mask` |
|      - | 4764 | ` *  The list of not allowed characters.` |
|      - | 4765 | ` * $start` |
|      - | 4766 | ` *  The position in subject to start searching.` |
|      - | 4767 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 4768 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 4769 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 4770 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 4771 | ` *  start'th position from the end of subject.` |
|      - | 4772 | ` * $length` |
|      - | 4773 | ` *  The length of the segment from subject to examine.` |
|      - | 4774 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 4775 | ` *  characters after the starting position.` |
|      - | 4776 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 4777 | ` *  position up to length characters from the end of subject.` |
|      - | 4778 | ` * Return` |
|      - | 4779 | ` *  Returns the length of the segment as an integer.` |
|      - | 4780 | ` */` |
|     16 | 4781 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4782 |  |
|      - | 4783 | `	const char *zString,*zMask,*zEnd;` |
|      - | 4784 | `	int iMasklen,iLen;` |
|      - | 4785 | `	SyString sToken;` |
|     17 | 4786 | `	int iCount = 0;` |
|      - | 4787 | `	int rc;` |
|     17 | 4788 | `	if( nArg < 2 ){` |
|      - | 4789 | `		/* Missing agruments,return zero */` |
|      3 | 4790 | `		ph7_result_int(pCtx,0);` |
|      3 | 4791 | `		return PH7_OK;` |
|      - | 4792 | `	}` |
|      - | 4793 | `	/* Extract the target string */` |
|     15 | 4794 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 4795 | `	/* Extract the mask */` |
|     15 | 4796 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 4797 | `	if( iLen < 1 ){` |
|      - | 4798 | `		/* Nothing to process,return zero */` |
|    ! 0 | 4799 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4800 | `		return PH7_OK;` |
|      - | 4801 | `	}` |
|     15 | 4802 | `	if( iMasklen < 1 ){` |
|      - | 4803 | `		/* No given mask,return the string length */` |
|      3 | 4804 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 4805 | `		return PH7_OK;` |
|      - | 4806 | `	}` |
|     13 | 4807 | `	if( nArg > 2 ){` |
|      - | 4808 | `		int nOfft;` |
|      - | 4809 | `		/* Extract the offset */` |
|     11 | 4810 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 4811 | `		if( nOfft < 0 ){` |
|    ! 0 | 4812 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 4813 | `			if( zBase > zString ){` |
|    ! 0 | 4814 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 4815 | `				zString = zBase;` |
|    ! 0 | 4816 | `			}else{` |
|      - | 4817 | `				/* Invalid offset */` |
|    ! 0 | 4818 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 4819 | `				return PH7_OK;` |
|      - | 4820 | `			}` |
|    ! 0 | 4821 | `		}else{` |
|     11 | 4822 | `			if( nOfft >= iLen ){` |
|      - | 4823 | `				/* Invalid offset */` |
|      3 | 4824 | `				ph7_result_int(pCtx,0);` |
|      3 | 4825 | `				return PH7_OK;` |
|    ! 0 | 4826 | `			}else{` |
|      - | 4827 | `				/* Update offset */` |
|      9 | 4828 | `				zString += nOfft;` |
|      9 | 4829 | `				iLen -= nOfft;` |
|      - | 4830 | `			}` |
|      - | 4831 | `		}` |
|      9 | 4832 | `		if( nArg > 3 ){` |
|      - | 4833 | `			int iUserlen;` |
|      - | 4834 | `			/* Extract the desired length */` |
|    ! 0 | 4835 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 4836 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 4837 | `				iLen = iUserlen;` |
|    ! 0 | 4838 | `			}` |
|    ! 0 | 4839 | `		}` |
|      4 | 4840 | `	}` |
|      - | 4841 | `	/* Point to the end of the string */` |
|     11 | 4842 | `	zEnd = &zString[iLen];` |
|      - | 4843 | `	/* Extract the first non-space token */` |
|     11 | 4844 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 4845 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 4846 | `		/* Compare against the current mask */` |
|     11 | 4847 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 4848 | `	}` |
|      - | 4849 | `	/* Longest match */` |
|     11 | 4850 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 4851 | `	return PH7_OK;` |
|      9 | 4852 |  |
|      - | 4853 | `/*` |
|      - | 4854 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 4855 | ` *  Search a string for any of a set of characters.` |
|      - | 4856 | ` * Parameters` |
|      - | 4857 | ` *  $haystack` |
|      - | 4858 | ` *   The string where char_list is looked for.` |
|      - | 4859 | ` *  $char_list` |
|      - | 4860 | ` *   This parameter is case sensitive.` |
|      - | 4861 | ` * Return` |
|      - | 4862 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 4863 | ` */` |
|      6 | 4864 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4865 |  |
|      - | 4866 | `	const char *zString,*zList,*zEnd;` |
|      - | 4867 | `	int iLen,iListLen,i,c;` |
|      - | 4868 | `	sxu32 nOfft,nMax;` |
|      - | 4869 | `	sxi32 rc;` |
|      7 | 4870 | `	if( nArg < 2 ){` |
|      - | 4871 | `		/* Missing arguments,return FALSE */` |
|      3 | 4872 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4873 | `		return PH7_OK;` |
|      - | 4874 | `	}` |
|      - | 4875 | `	/* Extract the haystack and the char list */` |
|      5 | 4876 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 4877 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 4878 | `	if( iLen < 1 ){` |
|      - | 4879 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 4880 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4881 | `		return PH7_OK;` |
|      - | 4882 | `	}` |
|      - | 4883 | `	/* Point to the end of the string */` |
|      5 | 4884 | `	zEnd = &zString[iLen];` |
|      5 | 4885 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 4886 | `	/* perform the requested operation */` |
|     15 | 4887 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 4888 | `		c = zList[i];` |
|     11 | 4889 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 4890 | `		if( rc == SXRET_OK ){` |
|      5 | 4891 | `			if( nMax < nOfft ){` |
|      3 | 4892 | `				nOfft = nMax;` |
|      1 | 4893 | `			}` |
|      2 | 4894 | `		}` |
|      6 | 4895 | `	}` |
|      5 | 4896 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 4897 | `		/* No such substring,return FALSE */` |
|      3 | 4898 | `		ph7_result_bool(pCtx,0);` |
|      2 | 4899 | `	}else{` |
|      - | 4900 | `		/* Return the substring */` |
|      3 | 4901 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 4902 | `	}` |
|      5 | 4903 | `	return PH7_OK;` |
|      4 | 4904 |  |
|      - | 4905 | `/*` |
|      - | 4906 | ` * string soundex(string $str)` |
|      - | 4907 | ` *  Calculate the soundex key of a string.` |
|      - | 4908 | ` * Parameters` |
|      - | 4909 | ` *  $str` |
|      - | 4910 | ` *   The input string.` |
|      - | 4911 | ` * Return` |
|      - | 4912 | ` *  Returns the soundex key as a string.` |
|      - | 4913 | ` * Note:` |
|      - | 4914 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 4915 | ` * source tree.` |
|      - | 4916 | ` */` |
|     20 | 4917 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4918 |  |
|      - | 4919 | `	const unsigned char *zIn;` |
|      - | 4920 | `	char zResult[8];` |
|      - | 4921 | `	int i, j;` |
|      - | 4922 | `	static const unsigned char iCode[] = {` |
|      - | 4923 |  |
|      - | 4924 |  |
|      - | 4925 |  |
|      - | 4926 |  |
|      - | 4927 |  |
|      - | 4928 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 4929 |  |
|      - | 4930 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 4931 | `	};` |
|     21 | 4932 | `	if( nArg < 1 ){` |
|      - | 4933 | `		/* Missing arguments,return the empty string */` |
|      3 | 4934 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4935 | `		return PH7_OK;` |
|      - | 4936 | `	}` |
|     19 | 4937 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     19 | 4938 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     19 | 4939 | `	if( zIn[i] ){` |
|     17 | 4940 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 4941 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 4942 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 4943 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 4944 | `			if( code>0 ){` |
|     45 | 4945 | `				if( code!=prevcode ){` |
|     33 | 4946 | `					prevcode = (unsigned char)code;` |
|     33 | 4947 | `					zResult[j++] = (char)code + '0';` |
|     16 | 4948 | `				}` |
|     23 | 4949 | `			}else{` |
|     49 | 4950 | `				prevcode = 0;` |
|      - | 4951 | `			}` |
|     47 | 4952 | `		}` |
|     33 | 4953 | `		while( j<4 ){` |
|     17 | 4954 | `			zResult[j++] = '0';` |
|      1 | 4955 | `		}` |
|     17 | 4956 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 4957 | `	}else{` |
|      3 | 4958 | `	  ph7_result_string(pCtx,"?000",4);` |
|      - | 4959 | `	}` |
|     19 | 4960 | `	return PH7_OK;` |
|     11 | 4961 |  |
|      - | 4962 | `/*` |
|      - | 4963 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 4964 | ` *  Wraps a string to a given number of characters.` |
|      - | 4965 | ` * Parameters` |
|      - | 4966 | ` *  $str` |
|      - | 4967 | ` *   The input string.` |
|      - | 4968 | ` * $width` |
|      - | 4969 | ` *  The column width.` |
|      - | 4970 | ` * $break` |
|      - | 4971 | ` *  The line is broken using the optional break parameter.` |
|      - | 4972 | ` * Return` |
|      - | 4973 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 4974 | ` */` |
|     14 | 4975 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4976 |  |
|      - | 4977 | `	const char *zIn,*zEnd,*zBreak;` |
|      - | 4978 | `	int iLen,iBreaklen,iChunk;` |
|     15 | 4979 | `	if( nArg < 1 ){` |
|      - | 4980 | `		/* Missing arguments,return the empty string */` |
|      3 | 4981 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4982 | `		return PH7_OK;` |
|      - | 4983 | `	}` |
|      - | 4984 | `	/* Extract the input string */` |
|     13 | 4985 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|     13 | 4986 | `	if( iLen < 1 ){` |
|      - | 4987 | `		/* Nothing to process,return the empty string */` |
|      3 | 4988 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4989 | `		return PH7_OK;` |
|      - | 4990 | `	}` |
|      - | 4991 | `	/* Chunk length */` |
|     11 | 4992 | `	iChunk = 75;` |
|     11 | 4993 | `	iBreaklen = 0;` |
|     11 | 4994 | `	zBreak = ""; /* cc warning */` |
|     11 | 4995 | `	if( nArg > 1 ){` |
|     11 | 4996 | `		iChunk = ph7_value_to_int(apArg[1]);` |
|     11 | 4997 | `		if( iChunk < 1 ){` |
|    ! 0 | 4998 | `			iChunk = 75;` |
|    ! 0 | 4999 | `		}` |
|     11 | 5000 | `		if( nArg > 2 ){` |
|      3 | 5001 | `			zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      1 | 5002 | `		}` |
|      5 | 5003 | `	}` |
|     11 | 5004 | `	if( iBreaklen < 1 ){` |
|      - | 5005 | `		/* Set a default column break */` |
|      - | 5006 | `#ifdef __WINNT__` |
|      1 | 5007 | `		zBreak = "\r\n";` |
|      1 | 5008 | `		iBreaklen = (int)sizeof("\r\n")-1;` |
|      - | 5009 | `#else` |
|      8 | 5010 | `		zBreak = "\n";` |
|      8 | 5011 | `		iBreaklen = (int)sizeof(char);` |
|      - | 5012 | `#endif` |
|      4 | 5013 | `	}` |
|      - | 5014 | `	/* Perform the requested operation */` |
|     11 | 5015 | `	zEnd = &zIn[iLen];` |
|     41 | 5016 | `	for(;;){` |
|      - | 5017 | `		int nMax;` |
|     47 | 5018 | `		if( zIn >= zEnd ){` |
|      - | 5019 | `			/* No more input to process */` |
|     11 | 5020 | `			break;` |
|      - | 5021 | `		}` |
|     37 | 5022 | `		nMax = (int)(zEnd-zIn);` |
|     37 | 5023 | `		if( iChunk > nMax ){` |
|     11 | 5024 | `			iChunk = nMax;` |
|      5 | 5025 | `		}` |
|      - | 5026 | `		/* Append the column first */` |
|     37 | 5027 | `		ph7_result_string(pCtx,zIn,iChunk); /* Will make it's own copy */` |
|      - | 5028 | `		/* Advance the cursor */` |
|     37 | 5029 | `		zIn += iChunk;` |
|     37 | 5030 | `		if( zIn < zEnd ){` |
|      - | 5031 | `			/* Append the line break */` |
|     27 | 5032 | `			ph7_result_string(pCtx,zBreak,iBreaklen);` |
|     13 | 5033 | `		}` |
|      1 | 5034 | `	}` |
|     11 | 5035 | `	return PH7_OK;` |
|      8 | 5036 |  |
|      - | 5037 | `/*` |
|      - | 5038 | ` * Check if the given character is a member of the given mask.` |
|      - | 5039 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 5040 | ` * Refer to [strtok()].` |
|      - | 5041 | ` */` |
|     30 | 5042 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 5043 |  |
|      - | 5044 | `	int i;` |
|     57 | 5045 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 5046 | `		if( c == zMask[i] ){` |
|     13 | 5047 | `			if( pOfft ){` |
|      5 | 5048 | `				*pOfft = i;` |
|      2 | 5049 | `			}` |
|     13 | 5050 | `			return TRUE;` |
|      - | 5051 | `		}` |
|     14 | 5052 | `	}` |
|     19 | 5053 | `	return FALSE;` |
|     16 | 5054 |  |
|      - | 5055 | `/*` |
|      - | 5056 | ` * Extract a single token from the input stream.` |
|      - | 5057 | ` * Refer to [strtok()].` |
|      - | 5058 | ` */` |
|      6 | 5059 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 5060 |  |
|      7 | 5061 | `	const char *zIn = *pzIn;` |
|      - | 5062 | `	const char *zPtr;` |
|      - | 5063 | `	/* Ignore leading delimiter */` |
|     11 | 5064 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5065 | `		zIn++;` |
|      1 | 5066 | `	}` |
|      7 | 5067 | `	if( zIn >= zEnd ){` |
|      - | 5068 | `		/* End of input */` |
|    ! 0 | 5069 | `		return SXERR_EOF;` |
|      - | 5070 | `	}` |
|      7 | 5071 | `	zPtr = zIn;` |
|      - | 5072 | `	/* Extract the token */` |
|     13 | 5073 | `	while( zIn < zEnd ){` |
|     11 | 5074 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 5075 | `			/* UTF-8 stream */` |
|    ! 0 | 5076 | `			zIn++;` |
|    ! 0 | 5077 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 5078 | `		}else{` |
|     11 | 5079 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5080 | `				break;` |
|      - | 5081 | `			}` |
|      7 | 5082 | `			zIn++;` |
|      - | 5083 | `		}` |
|      1 | 5084 | `	}` |
|      7 | 5085 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5086 | `	/* Update the cursor */` |
|      7 | 5087 | `	*pzIn = zIn;` |
|      - | 5088 | `	/* Return to the caller */` |
|      7 | 5089 | `	return SXRET_OK;` |
|      4 | 5090 |  |
|      - | 5091 | `/* strtok auxiliary private data */` |
|      - | 5092 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 5093 | `struct strtok_aux_data` |
|      - | 5094 |  |
|      - | 5095 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 5096 | `	const char *zIn;   /* Current input stream */` |
|      - | 5097 | `	const char *zEnd;  /* End of input */` |
|      - | 5098 | `};` |
|      - | 5099 | `/*` |
|      - | 5100 | ` * string strtok(string $str,string $token)` |
|      - | 5101 | ` * string strtok(string $token)` |
|      - | 5102 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 5103 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 5104 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 5105 | ` *  words by using the space character as the token.` |
|      - | 5106 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 5107 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 5108 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 5109 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 5110 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 5111 | ` *  the argument are found.` |
|      - | 5112 | ` * Parameters` |
|      - | 5113 | ` *  $str` |
|      - | 5114 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 5115 | ` * $token` |
|      - | 5116 | ` *  The delimiter used when splitting up str.` |
|      - | 5117 | ` * Return` |
|      - | 5118 | ` *   Current token or FALSE on EOF.` |
|      - | 5119 | ` */` |
|      8 | 5120 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5121 |  |
|      - | 5122 | `	strtok_aux_data *pAux;` |
|      - | 5123 | `	const char *zMask;` |
|      - | 5124 | `	SyString sToken;` |
|      - | 5125 | `	int nMasklen;` |
|      - | 5126 | `	sxi32 rc;` |
|      9 | 5127 | `	if( nArg < 2 ){` |
|      - | 5128 | `		/* Extract top aux data */` |
|      7 | 5129 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      7 | 5130 | `		if( pAux == 0 ){` |
|      - | 5131 | `			/* No aux data,return FALSE */` |
|    ! 0 | 5132 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5133 | `			return PH7_OK;` |
|      - | 5134 | `		}` |
|      7 | 5135 | `		nMasklen = 0;` |
|      7 | 5136 | `		zMask = ""; /* cc warning */` |
|      7 | 5137 | `		if( nArg > 0 ){` |
|      - | 5138 | `			/* Extract the mask */` |
|      5 | 5139 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 5140 | `		}` |
|      7 | 5141 | `		if( nMasklen < 1 ){` |
|      - | 5142 | `			/* Invalid mask,return FALSE */` |
|      3 | 5143 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|      3 | 5144 | `			ph7_context_free_chunk(pCtx,pAux);` |
|      3 | 5145 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|      3 | 5146 | `			ph7_result_bool(pCtx,0);` |
|      3 | 5147 | `			return PH7_OK;` |
|      - | 5148 | `		}` |
|      - | 5149 | `		/* Extract the token */` |
|      5 | 5150 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 5151 | `		if( rc != SXRET_OK ){` |
|      - | 5152 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 5153 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 5154 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5155 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 5156 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5157 | `		}else{` |
|      - | 5158 | `			/* Return the extracted token */` |
|      5 | 5159 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 5160 | `		}` |
|      3 | 5161 | `	}else{` |
|      - | 5162 | `		const char *zInput,*zCur;` |
|      - | 5163 | `		char *zDup;` |
|      - | 5164 | `		int nLen;` |
|      - | 5165 | `		/* Extract the raw input */` |
|      3 | 5166 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5167 | `		if( nLen < 1 ){` |
|      - | 5168 | `			/* Empty input,return FALSE */` |
|    ! 0 | 5169 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5170 | `			return PH7_OK;` |
|      - | 5171 | `		}` |
|      - | 5172 | `		/* Extract the mask */` |
|      3 | 5173 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 5174 | `		if( nMasklen < 1 ){` |
|      - | 5175 | `			/* Set a default mask */` |
|      - | 5176 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 5177 | `			zMask = TOK_MASK;` |
|    ! 0 | 5178 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 5179 | `#undef TOK_MASK` |
|    ! 0 | 5180 | `		}` |
|      - | 5181 | `		/* Extract a single token */` |
|      3 | 5182 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 5183 | `		if( rc != SXRET_OK ){` |
|      - | 5184 | `			/* Empty input */` |
|    ! 0 | 5185 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5186 | `			return PH7_OK;` |
|    ! 0 | 5187 | `		}else{` |
|      - | 5188 | `			/* Return the extracted token */` |
|      3 | 5189 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 5190 | `		}` |
|      - | 5191 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 5192 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 5193 | `		if( pAux ){` |
|      3 | 5194 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 5195 | `			if( nLen < 1 ){` |
|    ! 0 | 5196 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5197 | `				return PH7_OK;` |
|      - | 5198 | `			}` |
|      - | 5199 | `			/* Duplicate input */` |
|      3 | 5200 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 5201 | `			if( zDup  ){` |
|      3 | 5202 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 5203 | `				/* Register the aux data */` |
|      3 | 5204 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 5205 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 5206 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 5207 | `			}` |
|      1 | 5208 | `		}` |
|      - | 5209 | `	}` |
|      7 | 5210 | `	return PH7_OK;` |
|      5 | 5211 |  |
|      - | 5212 | `/*` |
|      - | 5213 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 5214 | ` *  Pad a string to a certain length with another string` |
|      - | 5215 | ` * Parameters` |
|      - | 5216 | ` *  $input` |
|      - | 5217 | ` *   The input string.` |
|      - | 5218 | ` * $pad_length` |
|      - | 5219 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 5220 | ` *   string, no padding takes place.` |
|      - | 5221 | ` * $pad_string` |
|      - | 5222 | ` *   Note:` |
|      - | 5223 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 5224 | ` *    divided by the pad_string's length.` |
|      - | 5225 | ` * $pad_type` |
|      - | 5226 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 5227 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 5228 | ` * Return` |
|      - | 5229 | ` *  The padded string.` |
|      - | 5230 | ` */` |
|     10 | 5231 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5232 |  |
|      - | 5233 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 5234 | `	const char *zIn,*zPad;` |
|     11 | 5235 | `	if( nArg < 2 ){` |
|      - | 5236 | `		/* Missing arguments,return the empty string */` |
|      5 | 5237 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 5238 | `		return PH7_OK;` |
|      - | 5239 | `	}` |
|      - | 5240 | `	/* Extract the target string */` |
|      7 | 5241 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5242 | `	/* Padding length */` |
|      7 | 5243 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|      7 | 5244 | `	if( iPadlen > 0 ){` |
|      5 | 5245 | `		iPadlen -= iLen;` |
|      2 | 5246 | `	}` |
|      7 | 5247 | `	if( iPadlen < 1  ){` |
|      - | 5248 | `		/* Return the string verbatim */` |
|      3 | 5249 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      3 | 5250 | `		return PH7_OK;` |
|      - | 5251 | `	}` |
|      5 | 5252 | `	zPad = " "; /* Whitespace padding */` |
|      5 | 5253 | `	iStrpad = (int)sizeof(char);` |
|      5 | 5254 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      5 | 5255 | `	if( nArg > 2 ){` |
|      - | 5256 | `		/* Padding string */` |
|      5 | 5257 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      5 | 5258 | `		if( iStrpad < 1 ){` |
|      - | 5259 | `			/* Empty string */` |
|    ! 0 | 5260 | `			zPad = " "; /* Whitespace padding */` |
|    ! 0 | 5261 | `			iStrpad = (int)sizeof(char);` |
|    ! 0 | 5262 | `		}` |
|      5 | 5263 | `		if( nArg > 3 ){` |
|      - | 5264 | `			/* Padd type */` |
|      5 | 5265 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 5266 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 5267 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 5268 | `			}` |
|      2 | 5269 | `		}` |
|      2 | 5270 | `	}` |
|      5 | 5271 | `	iDiv = 1;` |
|      5 | 5272 | `	if( iType == 2 ){` |
|    ! 0 | 5273 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 5274 | `	}` |
|      - | 5275 | `	/* Perform the requested operation */` |
|      5 | 5276 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 5277 | `		jPad = iStrpad;` |
|      5 | 5278 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 5279 | `			/* Padding */` |
|      5 | 5280 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 5281 | `				break;` |
|      - | 5282 | `			}` |
|      3 | 5283 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      2 | 5284 | `		}` |
|      3 | 5285 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 5286 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 5287 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 5288 | `				if( jPad > iStrpad ){` |
|    ! 0 | 5289 | `					jPad = iStrpad;` |
|    ! 0 | 5290 | `				}` |
|      3 | 5291 | `				if( jPad < 1){` |
|    ! 0 | 5292 | `					break;` |
|      - | 5293 | `				}` |
|      3 | 5294 | `				ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 5295 | `			}` |
|      1 | 5296 | `		}` |
|      1 | 5297 | `	}` |
|      5 | 5298 | `	if( iLen > 0 ){` |
|      - | 5299 | `		/* Append the input string */` |
|      5 | 5300 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      2 | 5301 | `	}` |
|      5 | 5302 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 5303 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 5304 | `			/* Padding */` |
|      5 | 5305 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 5306 | `				break;` |
|      - | 5307 | `			}` |
|      3 | 5308 | `			ph7_result_string(pCtx,zPad,iStrpad);` |
|      2 | 5309 | `		}` |
|      5 | 5310 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 5311 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 5312 | `			if( jPad > iStrpad ){` |
|    ! 0 | 5313 | `				jPad = iStrpad;` |
|    ! 0 | 5314 | `			}` |
|      3 | 5315 | `			if( jPad < 1){` |
|    ! 0 | 5316 | `				break;` |
|      - | 5317 | `			}` |
|      3 | 5318 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 5319 | `		}` |
|      1 | 5320 | `	}` |
|      5 | 5321 | `	return PH7_OK;` |
|      6 | 5322 |  |
|      - | 5323 | `/*` |
|      - | 5324 | ` * String replacement private data.` |
|      - | 5325 | ` */` |
|      - | 5326 | `typedef struct str_replace_data str_replace_data;` |
|      - | 5327 | `struct str_replace_data` |
|      - | 5328 |  |
|      - | 5329 | `	/* The following two fields are only used by the strtr function */` |
|      - | 5330 | `	SyBlob *pWorker;         /* Working buffer */` |
|      - | 5331 | `	ProcStringMatch xMatch;  /* Pattern match routine */` |
|      - | 5332 | `	/* The following two fields are only used by the str_replace function */` |
|      - | 5333 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 5334 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 5335 | `};` |
|      - | 5336 | `/*` |
|      - | 5337 | ` * Remove a substring.` |
|      - | 5338 | ` */` |
|      - | 5339 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 5340 | `	for(;;){\` |
|      - | 5341 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 5342 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 5343 | `		++OFFT;\` |
|      - | 5344 | `	}\` |
|      - | 5345 |  |
|      - | 5346 | `/*` |
|      - | 5347 | ` * Shift right and insert algorithm.` |
|      - | 5348 | ` */` |
|      - | 5349 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 5350 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 5351 | `		for(;;){\` |
|      - | 5352 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 5353 | `			if(INLEN < 1 ) { break; }\` |
|      - | 5354 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 5355 | `			--INLEN; \` |
|      - | 5356 | `		}\` |
|      - | 5357 | `		for(;;){\` |
|      - | 5358 | `				if(ELEN < 1) { break; }\` |
|      - | 5359 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 5360 | `				OFFT++;\` |
|      - | 5361 | `				ENTRY++;\` |
|      - | 5362 | `				--ELEN;\` |
|      - | 5363 | `		}\` |
|      - | 5364 |  |
|      - | 5365 | `/*` |
|      - | 5366 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 5367 | ` * replacement string [i.e: zReplace].` |
|      - | 5368 | ` */` |
|     38 | 5369 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 5370 |  |
|     39 | 5371 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 5372 | `	sxu32 n,m;` |
|     39 | 5373 | `	n = SyBlobLength(pWorker);` |
|     39 | 5374 | `	m = nOfft;` |
|      - | 5375 | `	/* Delete the old entry */` |
|    475 | 5376 | `	STRDEL(zInput,n,m,nLen);` |
|     39 | 5377 | `	SyBlobLength(pWorker) -= nLen;` |
|     39 | 5378 | `	if( nReplen > 0 ){` |
|     33 | 5379 | `		sxi32 iRep = nReplen;` |
|      - | 5380 | `		sxi32 rc;` |
|      - | 5381 | `		/*` |
|      - | 5382 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 5383 | `		 * string.` |
|      - | 5384 | `		 */` |
|     33 | 5385 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     33 | 5386 | `		if( rc != SXRET_OK ){` |
|      - | 5387 | `			/* Simply ignore any memory failure problem */` |
|    ! 0 | 5388 | `			return SXRET_OK;` |
|      - | 5389 | `		}` |
|      - | 5390 | `		/* Perform the insertion now */` |
|     33 | 5391 | `		zInput = (char *)SyBlobData(pWorker);` |
|     33 | 5392 | `		n = SyBlobLength(pWorker);` |
|    163 | 5393 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     33 | 5394 | `		SyBlobLength(pWorker) += nReplen;` |
|     16 | 5395 | `	}` |
|     39 | 5396 | `	return SXRET_OK;` |
|     20 | 5397 |  |
|      - | 5398 | `/*` |
|      - | 5399 | ` * String replacement walker callback.` |
|      - | 5400 | ` * The following callback is invoked for each array entry that hold` |
|      - | 5401 | ` * the replace string.` |
|      - | 5402 | ` * Refer to the strtr() implementation for more information.` |
|      - | 5403 | ` */` |
|      8 | 5404 | `static int StringReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 5405 |  |
|      9 | 5406 | `	str_replace_data *pRepData = (str_replace_data *)pUserData;` |
|      - | 5407 | `	const char *zTarget,*zReplace;` |
|      - | 5408 | `	SyBlob *pWorker;` |
|      - | 5409 | `	int tLen,nLen;` |
|      - | 5410 | `	sxu32 nOfft;` |
|      - | 5411 | `	sxi32 rc;` |
|      - | 5412 | `	/* Point to the working buffer */` |
|      9 | 5413 | `	pWorker = pRepData->pWorker;` |
|      9 | 5414 | `	if( !ph7_value_is_string(pKey) ){` |
|      - | 5415 | `		/* Target and replace must be a string */` |
|      3 | 5416 | `		return PH7_OK;` |
|      - | 5417 | `	}` |
|      - | 5418 | `	/* Extract the target and the replace */` |
|      7 | 5419 | `	zTarget = ph7_value_to_string(pKey,&tLen);` |
|      7 | 5420 | `	if( tLen < 1 ){` |
|      - | 5421 | `		/* Empty target,return immediately */` |
|    ! 0 | 5422 | `		return PH7_OK;` |
|      - | 5423 | `	}` |
|      - | 5424 | `	/* Perform a pattern search */` |
|      7 | 5425 | `	rc = pRepData->xMatch(SyBlobData(pWorker),SyBlobLength(pWorker),(const void *)zTarget,(sxu32)tLen,&nOfft);` |
|      7 | 5426 | `	if( rc != SXRET_OK ){` |
|      - | 5427 | `		/* Pattern not found */` |
|    ! 0 | 5428 | `		return PH7_OK;` |
|      - | 5429 | `	}` |
|      - | 5430 | `	/* Extract the replace string */` |
|      7 | 5431 | `	zReplace = ph7_value_to_string(pData,&nLen);` |
|      - | 5432 | `	/* Perform the replace process */` |
|      7 | 5433 | `	StringReplace(pWorker,nOfft,tLen,zReplace,nLen);` |
|      - | 5434 | `	/* All done */` |
|      7 | 5435 | `	return PH7_OK;` |
|      5 | 5436 |  |
|      - | 5437 | `/*` |
|      - | 5438 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 5439 | ` * to collect search/replace string.` |
|      - | 5440 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 5441 | ` */` |
|     26 | 5442 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 5443 |  |
|     27 | 5444 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 5445 | `	SyString sWorker;` |
|      - | 5446 | `	const char *zIn;` |
|      - | 5447 | `	int nByte;` |
|      - | 5448 | `	/* Extract a string representation of the given argument */` |
|     27 | 5449 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 5450 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 5451 | `	if( nByte > 0 ){` |
|      - | 5452 | `		char *zDup;` |
|      - | 5453 | `		/* Duplicate the chunk */` |
|     25 | 5454 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 5455 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 5456 | `			);` |
|     25 | 5457 | `		if( zDup == 0 ){` |
|      - | 5458 | `			/* Ignore any memory failure problem */` |
|    ! 0 | 5459 | `			ph7_context_throw_error(pRep->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 5460 | `			return PH7_OK;` |
|      - | 5461 | `		}` |
|     25 | 5462 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 5463 | `		/* Save the chunk */` |
|     25 | 5464 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 5465 | `	}` |
|      - | 5466 | `	/* Save for later processing */` |
|     27 | 5467 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 5468 | `	/* All done */` |
|     13 | 5469 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 5470 | `	return PH7_OK;` |
|     14 | 5471 |  |
|      - | 5472 | `/*` |
|      - | 5473 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 5474 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 5475 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 5476 | ` * Parameters` |
|      - | 5477 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 5478 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 5479 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 5480 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 5481 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 5482 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 5483 | ` * $search` |
|      - | 5484 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 5485 | ` *  to designate multiple needles.` |
|      - | 5486 | ` * $replace` |
|      - | 5487 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 5488 | ` *  to designate multiple replacements.` |
|      - | 5489 | ` * $subject` |
|      - | 5490 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 5491 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 5492 | ` *  of subject, and the return value is an array as well.` |
|      - | 5493 | ` * $count (Not used)` |
|      - | 5494 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 5495 | ` * Return` |
|      - | 5496 | ` * This function returns a string or an array with the replaced values.` |
|      - | 5497 | ` */` |
|  20650 | 5498 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5499 |  |
|      - | 5500 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 5501 | `	ProcStringMatch xMatch;` |
|      - | 5502 | `	const char *zIn,*zFunc;` |
|      - | 5503 | `	str_replace_data sRep;` |
|      - | 5504 | `	SyBlob sWorker;` |
|      - | 5505 | `	SySet sReplace;` |
|      - | 5506 | `	SySet sSearch;` |
|      - | 5507 | `	int rep_str;` |
|      - | 5508 | `	int nByte;` |
|      - | 5509 | `	sxi32 rc;` |
|  20652 | 5510 | `	if( nArg < 3 ){` |
|      - | 5511 | `		/* Missing/Invalid arguments,return null */` |
|      7 | 5512 | `		ph7_result_null(pCtx);` |
|      7 | 5513 | `		return PH7_OK;` |
|      - | 5514 | `	}` |
|      - | 5515 | `	/* Initialize fields */` |
|  20646 | 5516 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  20646 | 5517 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  20646 | 5518 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  20646 | 5519 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  20646 | 5520 | `	sRep.pCtx = pCtx;` |
|  20646 | 5521 | `	sRep.pCollector = &sSearch;` |
|  20646 | 5522 | `	rep_str = 0;` |
|      - | 5523 | `	/* Extract the subject */` |
|  20646 | 5524 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  20646 | 5525 | `	if( nByte < 1 ){` |
|      - | 5526 | `		/* Nothing to replace,return the empty string */` |
|     29 | 5527 | `		ph7_result_string(pCtx,"",0);` |
|     29 | 5528 | `		return PH7_OK;` |
|      - | 5529 | `	}` |
|      - | 5530 | `	/* Copy the subject */` |
|  20618 | 5531 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 5532 | `	/* Search string */` |
|  20618 | 5533 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 5534 | `		/* Collect search string */` |
|      9 | 5535 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 5536 | `	}else{` |
|      - | 5537 | `		/* Single pattern */` |
|  20610 | 5538 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  20610 | 5539 | `		if( nByte < 1 ){` |
|      - | 5540 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 5541 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 5542 | `			return PH7_OK;` |
|      - | 5543 | `		}` |
|  20606 | 5544 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 5545 | `		/* Save for later processing */` |
|  20606 | 5546 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 5547 | `	}` |
|      - | 5548 | `	/* Replace string */` |
|  20614 | 5549 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 5550 | `		/* Collect replace string */` |
|      7 | 5551 | `		sRep.pCollector = &sReplace;` |
|      7 | 5552 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 5553 | `	}else{` |
|      - | 5554 | `		/* Single needle */` |
|  20608 | 5555 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  20608 | 5556 | `		rep_str = 1;` |
|  20608 | 5557 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 5558 | `		/* Save for later processing */` |
|  20608 | 5559 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 5560 | `	}` |
|      - | 5561 | `	/* Reset loop cursors */` |
|  20614 | 5562 | `	SySetResetCursor(&sSearch);` |
|  20614 | 5563 | `	SySetResetCursor(&sReplace);` |
|  20614 | 5564 | `	pReplace = pSearch = 0; /* cc warning */` |
|  20614 | 5565 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 5566 | `	/* Extract function name */` |
|  20614 | 5567 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 5568 | `	/* Set the default pattern match routine */` |
|  20614 | 5569 | `	xMatch = SyBlobSearch;` |
|  20614 | 5570 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 5571 | `		/* Case insensitive pattern match */` |
|     11 | 5572 | `		xMatch = iPatternMatch;` |
|      5 | 5573 | `	}` |
|      - | 5574 | `	/* Start the replace process */` |
|  41234 | 5575 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 5576 | `		sxu32 nCount,nOfft;` |
|  20622 | 5577 | `		if( pSearch->nByte <  1 ){` |
|      - | 5578 | `			/* Empty string,ignore */` |
|      3 | 5579 | `			continue;` |
|      - | 5580 | `		}` |
|      - | 5581 | `		/* Extract the replace string */` |
|  20620 | 5582 | `		if( rep_str ){` |
|  20610 | 5583 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  10306 | 5584 | `		}else{` |
|     11 | 5585 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 5586 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 5587 | `				 * An empty string is used for the rest of replacement values` |
|      - | 5588 | `				 */` |
|      3 | 5589 | `				pReplace = 0;` |
|      1 | 5590 | `			}` |
|      - | 5591 | `		}` |
|  20620 | 5592 | `		if( pReplace == 0 ){` |
|      - | 5593 | `			/* Use an empty string instead */` |
|      3 | 5594 | `			pReplace = &sTemp;` |
|      1 | 5595 | `		}` |
|  20620 | 5596 | `		nOfft = nCount = 0;` |
|  10325 | 5597 | `		for(;;){` |
|  20652 | 5598 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 5599 | `				break;` |
|      - | 5600 | `			}` |
|      - | 5601 | `			/* Perform a pattern lookup */` |
|  30959 | 5602 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  20638 | 5603 | `				pSearch->nByte,&nOfft);` |
|  20640 | 5604 | `			if( rc != SXRET_OK ){` |
|      - | 5605 | `				/* Pattern not found */` |
|  20608 | 5606 | `				break;` |
|      - | 5607 | `			}` |
|      - | 5608 | `			/* Perform the replace operation */` |
|     33 | 5609 | `			StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|      - | 5610 | `			/* Increment offset counter */` |
|     33 | 5611 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 5612 | `		}` |
|      2 | 5613 | `	}` |
|      - | 5614 | `	/* All done,clean-up the mess left behind */` |
|  20614 | 5615 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  20614 | 5616 | `	SySetRelease(&sSearch);` |
|  20614 | 5617 | `	SySetRelease(&sReplace);` |
|  20614 | 5618 | `	SyBlobRelease(&sWorker);` |
|  20614 | 5619 | `	return PH7_OK;` |
|  10327 | 5620 |  |
|      - | 5621 | `/*` |
|      - | 5622 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 5623 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 5624 | ` *  Translate characters or replace substrings.` |
|      - | 5625 | ` * Parameters` |
|      - | 5626 | ` *  $str` |
|      - | 5627 | ` *  The string being translated.` |
|      - | 5628 | ` * $from` |
|      - | 5629 | ` *  The string being translated to to.` |
|      - | 5630 | ` * $to` |
|      - | 5631 | ` *  The string replacing from.` |
|      - | 5632 | ` * $replace_pairs` |
|      - | 5633 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 5634 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 5635 | ` * Return` |
|      - | 5636 | ` *  The translated string.` |
|      - | 5637 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 5638 | ` */` |
|     12 | 5639 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5640 |  |
|      - | 5641 | `	const char *zIn;` |
|      - | 5642 | `	int nLen;` |
|     13 | 5643 | `	if( nArg < 1 ){` |
|      - | 5644 | `		/* Nothing to replace,return FALSE */` |
|      7 | 5645 | `		ph7_result_bool(pCtx,0);` |
|      7 | 5646 | `		return PH7_OK;` |
|      - | 5647 | `	}` |
|      7 | 5648 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 5649 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 5650 | `		/* Invalid arguments */` |
|    ! 0 | 5651 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 5652 | `		return PH7_OK;` |
|      - | 5653 | `	}` |
|      9 | 5654 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 5655 | `		str_replace_data sRepData;` |
|      - | 5656 | `		SyBlob sWorker;` |
|      - | 5657 | `		/* Initilaize the working buffer */` |
|      5 | 5658 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|      - | 5659 | `		/* Copy raw string */` |
|      5 | 5660 | `		SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nLen);` |
|      - | 5661 | `		/* Init our replace data instance */` |
|      5 | 5662 | `		sRepData.pWorker = &sWorker;` |
|      5 | 5663 | `		sRepData.xMatch = SyBlobSearch;` |
|      - | 5664 | `		/* Iterate throw array entries and perform the replace operation.*/` |
|      5 | 5665 | `		ph7_array_walk(apArg[1],StringReplaceWalker,&sRepData);` |
|      - | 5666 | `		/* All done, return the result string */` |
|      7 | 5667 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|      4 | 5668 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 5669 | `		/* Clean-up */` |
|      5 | 5670 | `		SyBlobRelease(&sWorker);` |
|      3 | 5671 | `	}else{` |
|      - | 5672 | `		int i,flen,tlen,c,iOfft;` |
|      - | 5673 | `		const char *zFrom,*zTo;` |
|      3 | 5674 | `		if( nArg < 3 ){` |
|      - | 5675 | `			/* Nothing to replace */` |
|    ! 0 | 5676 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 5677 | `			return PH7_OK;` |
|      - | 5678 | `		}` |
|      - | 5679 | `		/* Extract given arguments */` |
|      3 | 5680 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 5681 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 5682 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 5683 | `			/* Nothing to replace */` |
|    ! 0 | 5684 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 5685 | `			return PH7_OK;` |
|      - | 5686 | `		}` |
|      - | 5687 | `		/* Start the replace process */` |
|     13 | 5688 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 5689 | `			c = zIn[i];` |
|     11 | 5690 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 5691 | `				if ( iOfft < tlen ){` |
|      5 | 5692 | `					c = zTo[iOfft];` |
|      2 | 5693 | `				}` |
|      2 | 5694 | `			}` |
|     11 | 5695 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 5696 |  |
|      6 | 5697 | `		}` |
|      - | 5698 | `	}` |
|      7 | 5699 | `	return PH7_OK;` |
|      7 | 5700 |  |
|      - | 5701 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 5702 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 5703 | `/*` |
|      - | 5704 | ` * Parse an INI string.` |
|      - | 5705 |  |
|      - | 5706 | ` * According to wikipedia` |
|      - | 5707 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 5708 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 5709 | ` *  Format` |
|      - | 5710 | `*    Properties` |
|      - | 5711 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 5712 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 5713 | `*     Example:` |
|      - | 5714 | `*      name=value` |
|      - | 5715 | `*    Sections` |
|      - | 5716 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 5717 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 5718 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 5719 | `*     or the end of the file. Sections may not be nested.` |
|      - | 5720 | `*     Example:` |
|      - | 5721 | `*      [section]` |
|      - | 5722 | `*   Comments` |
|      - | 5723 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 5724 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 5725 | `*/` |
|     12 | 5726 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 5727 |  |
|      - | 5728 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 5729 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 5730 | `	SyHashEntry *pEntry;` |
|      - | 5731 | `	SyString sEntry;` |
|      - | 5732 | `	SyHash sHash;` |
|      - | 5733 | `	int c;` |
|      - | 5734 | `	/* Create an empty array and worker variables */` |
|     13 | 5735 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 5736 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 5737 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 5738 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 5739 | `		/* Out of memory */` |
|    ! 0 | 5740 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      - | 5741 | `		/* Return FALSE */` |
|    ! 0 | 5742 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5743 | `		return PH7_OK;` |
|      - | 5744 | `	}` |
|     13 | 5745 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 5746 | `	pCur = pArray;` |
|      - | 5747 | `	/* Start the parse process */` |
|     21 | 5748 | `	for(;;){` |
|      - | 5749 | `		/* Ignore leading white spaces */` |
|     69 | 5750 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 5751 | `			zIn++;` |
|      1 | 5752 | `		}` |
|     43 | 5753 | `		if( zIn >= zEnd ){` |
|      - | 5754 | `			/* No more input to process */` |
|     13 | 5755 | `			break;` |
|      - | 5756 | `		}` |
|     31 | 5757 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 5758 | `			/* Comment til the end of line */` |
|    ! 0 | 5759 | `			zIn++;` |
|    ! 0 | 5760 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 5761 | `				zIn++;` |
|    ! 0 | 5762 | `			}` |
|    ! 0 | 5763 | `			continue;` |
|      - | 5764 | `		}` |
|      - | 5765 | `		/* Reset the string cursor of the working variable */` |
|     31 | 5766 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 5767 | `		if( zIn[0] == '[' ){` |
|      - | 5768 | `			/* Section: Extract the section name */` |
|      9 | 5769 | `			zIn++;` |
|      9 | 5770 | `			zCur = zIn;` |
|     73 | 5771 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 5772 | `				zIn++;` |
|      1 | 5773 | `			}` |
|      9 | 5774 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 5775 | `				/* Save the section name */` |
|      5 | 5776 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 5777 | `				SyStringFullTrim(&sEntry);` |
|      5 | 5778 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 5779 | `				if( sEntry.nByte > 0 ){` |
|      - | 5780 | `					/* Associate an array with the section */` |
|      5 | 5781 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 5782 | `					if( pSection ){` |
|      5 | 5783 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 5784 | `						pCur = pSection;` |
|      2 | 5785 | `					}` |
|      2 | 5786 | `				}` |
|      2 | 5787 | `			}` |
|      9 | 5788 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 5789 | `		}else{` |
|      - | 5790 | `			ph7_value *pOldCur;` |
|      - | 5791 | `			int is_array;` |
|      - | 5792 | `			int iLen;` |
|      - | 5793 | `			/* Properties */` |
|     23 | 5794 | `			is_array = 0;` |
|     23 | 5795 | `			zCur = zIn;` |
|     23 | 5796 | `			iLen = 0; /* cc warning */` |
|     23 | 5797 | `			pOldCur = pCur;` |
|    155 | 5798 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 5799 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 5800 | `					/* Array */` |
|    ! 0 | 5801 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 5802 | `					is_array = 1;` |
|    ! 0 | 5803 | `					if( iLen > 0 ){` |
|    ! 0 | 5804 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 5805 | `						/* Query the hashtable */` |
|    ! 0 | 5806 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 5807 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 5808 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 5809 | `						if( pEntry ){` |
|    ! 0 | 5810 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 5811 | `						}else{` |
|      - | 5812 | `							/* Create an empty array */` |
|    ! 0 | 5813 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 5814 | `							if( pvArr ){` |
|      - | 5815 | `								/* Save the entry */` |
|    ! 0 | 5816 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 5817 | `								/* Insert the entry */` |
|    ! 0 | 5818 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 5819 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 5820 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 5821 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 5822 | `							}` |
|      - | 5823 | `						}` |
|    ! 0 | 5824 | `						if( pvArr ){` |
|    ! 0 | 5825 | `							pCur = pvArr;` |
|    ! 0 | 5826 | `						}` |
|    ! 0 | 5827 | `					}` |
|    ! 0 | 5828 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 5829 | `						zIn++;` |
|    ! 0 | 5830 | `					}` |
|    ! 0 | 5831 | `				}` |
|    133 | 5832 | `				zIn++;` |
|      1 | 5833 | `			}` |
|     23 | 5834 | `			if( !is_array ){` |
|     23 | 5835 | `				iLen = (int)(zIn-zCur);` |
|     11 | 5836 | `			}` |
|      - | 5837 | `			/* Trim the key */` |
|     23 | 5838 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 5839 | `			SyStringFullTrim(&sEntry);` |
|     23 | 5840 | `			if( sEntry.nByte > 0 ){` |
|     23 | 5841 | `				if( !is_array ){` |
|      - | 5842 | `					/* Save the key name */` |
|     23 | 5843 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 5844 | `				}` |
|      - | 5845 | `				/* extract key value */` |
|     23 | 5846 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 5847 | `				zIn++; /* '=' */` |
|     39 | 5848 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 5849 | `					zIn++;` |
|      1 | 5850 | `				}` |
|     23 | 5851 | `				if( zIn < zEnd ){` |
|     21 | 5852 | `					zCur = zIn;` |
|     21 | 5853 | `					c = zIn[0];` |
|     21 | 5854 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 5855 | `						zIn++;` |
|      - | 5856 | `						/* Delimit the value */` |
|    ! 0 | 5857 | `						while( zIn < zEnd ){` |
|    ! 0 | 5858 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 5859 | `								break;` |
|      - | 5860 | `							}` |
|    ! 0 | 5861 | `							zIn++;` |
|    ! 0 | 5862 | `						}` |
|    ! 0 | 5863 | `						if( zIn < zEnd ){` |
|    ! 0 | 5864 | `							zIn++;` |
|    ! 0 | 5865 | `						}` |
|    ! 0 | 5866 | `					}else{` |
|    125 | 5867 | `						while( zIn < zEnd ){` |
|    123 | 5868 | `							if( zIn[0] == '\n' ){` |
|     19 | 5869 | `								if( zIn[-1] != '\\' ){` |
|     19 | 5870 | `									break;` |
|    ! 0 | 5871 | `								}` |
|    105 | 5872 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 5873 | `								/* Inline comments */` |
|    ! 0 | 5874 | `								break;` |
|      - | 5875 | `							}` |
|    105 | 5876 | `							zIn++;` |
|      1 | 5877 | `						}` |
|      - | 5878 | `					}` |
|      - | 5879 | `					/* Trim the value */` |
|     21 | 5880 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 5881 | `					SyStringFullTrim(&sEntry);` |
|     21 | 5882 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 5883 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 5884 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 5885 | `					}` |
|     21 | 5886 | `					if( sEntry.nByte > 0 ){` |
|     21 | 5887 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 5888 | `					}` |
|      - | 5889 | `					/* Insert the key and it's value */` |
|     21 | 5890 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 5891 | `				}` |
|     12 | 5892 | `			}else{` |
|    ! 0 | 5893 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 5894 | `					zIn++;` |
|    ! 0 | 5895 | `				}` |
|      - | 5896 | `			}` |
|     23 | 5897 | `			pCur = pOldCur;` |
|      - | 5898 | `		}` |
|      1 | 5899 | `	}` |
|     13 | 5900 | `	SyHashRelease(&sHash);` |
|      - | 5901 | `	/* Return the parse of the INI string */` |
|     13 | 5902 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 5903 | `	return SXRET_OK;` |
|      7 | 5904 |  |
|      - | 5905 | `/*` |
|      - | 5906 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 5907 | ` *  Parse a configuration string.` |
|      - | 5908 | ` * Parameters` |
|      - | 5909 | ` *  $ini` |
|      - | 5910 | ` *   The contents of the ini file being parsed.` |
|      - | 5911 | ` *  $process_sections` |
|      - | 5912 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 5913 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 5914 | ` *  $scanner_mode (Not used)` |
|      - | 5915 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 5916 | ` *   then option values will not be parsed.` |
|      - | 5917 | ` * Return` |
|      - | 5918 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 5919 | ` */` |
|     10 | 5920 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5921 |  |
|      - | 5922 | `	const char *zIni;` |
|      - | 5923 | `	int nByte;` |
|     11 | 5924 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5925 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 5926 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5927 | `		return PH7_OK;` |
|      - | 5928 | `	}` |
|      - | 5929 | `	/* Extract the raw INI buffer */` |
|     11 | 5930 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 5931 | `	/* Process the INI buffer*/` |
|     11 | 5932 | `	PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|     11 | 5933 | `	return PH7_OK;` |
|      6 | 5934 |  |
|      - | 5935 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5936 |  |
|      - | 5937 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5938 |  |
|      - | 5939 | `/*` |
|      - | 5940 | ` * Ctype Functions.` |
|      - | 5941 | ` * Status:` |
|      - | 5942 | ` *    Stable.` |
|      - | 5943 | ` */` |
|      - | 5944 | `/*` |
|      - | 5945 | ` * bool ctype_alnum(string $text)` |
|      - | 5946 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 5947 | ` * Parameters` |
|      - | 5948 | ` *  $text` |
|      - | 5949 | ` *   The tested string.` |
|      - | 5950 | ` * Return` |
|      - | 5951 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 5952 | ` */` |
|     16 | 5953 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5954 |  |
|      - | 5955 | `	const unsigned char *zIn,*zEnd;` |
|      - | 5956 | `	int nLen;` |
|     17 | 5957 | `	if( nArg < 1 ){` |
|      - | 5958 | `		/* Missing arguments,return FALSE */` |
|      3 | 5959 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5960 | `		return PH7_OK;` |
|      - | 5961 | `	}` |
|      - | 5962 | `	/* Extract the target string */` |
|     15 | 5963 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 5964 | `	zEnd = &zIn[nLen];` |
|     15 | 5965 | `	if( nLen < 1 ){` |
|      - | 5966 | `		/* Empty string,return FALSE */` |
|      3 | 5967 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5968 | `		return PH7_OK;` |
|      - | 5969 | `	}` |
|      - | 5970 | `	/* Perform the requested operation */` |
|     32 | 5971 | `	for(;;){` |
|     65 | 5972 | `		if( zIn >= zEnd ){` |
|      - | 5973 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 5974 | `			ph7_result_bool(pCtx,1);` |
|      9 | 5975 | `			return PH7_OK;` |
|      - | 5976 | `		}` |
|     57 | 5977 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 5978 | `			break;` |
|      - | 5979 | `		}` |
|      - | 5980 | `		/* Point to the next character */` |
|     53 | 5981 | `		zIn++;` |
|      1 | 5982 | `	}` |
|      - | 5983 | `	/* The test failed,return FALSE */` |
|      5 | 5984 | `	ph7_result_bool(pCtx,0);` |
|      5 | 5985 | `	return PH7_OK;` |
|      9 | 5986 |  |
|      - | 5987 | `/*` |
|      - | 5988 | ` * bool ctype_alpha(string $text)` |
|      - | 5989 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 5990 | ` * Parameters` |
|      - | 5991 | ` *  $text` |
|      - | 5992 | ` *   The tested string.` |
|      - | 5993 | ` * Return` |
|      - | 5994 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 5995 | ` */` |
|     18 | 5996 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5997 |  |
|      - | 5998 | `	const unsigned char *zIn,*zEnd;` |
|      - | 5999 | `	int nLen;` |
|     19 | 6000 | `	if( nArg < 1 ){` |
|      - | 6001 | `		/* Missing arguments,return FALSE */` |
|      3 | 6002 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6003 | `		return PH7_OK;` |
|      - | 6004 | `	}` |
|      - | 6005 | `	/* Extract the target string */` |
|     17 | 6006 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6007 | `	zEnd = &zIn[nLen];` |
|     17 | 6008 | `	if( nLen < 1 ){` |
|      - | 6009 | `		/* Empty string,return FALSE */` |
|      3 | 6010 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6011 | `		return PH7_OK;` |
|      - | 6012 | `	}` |
|      - | 6013 | `	/* Perform the requested operation */` |
|     42 | 6014 | `	for(;;){` |
|     85 | 6015 | `		if( zIn >= zEnd ){` |
|      - | 6016 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6017 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6018 | `			return PH7_OK;` |
|      - | 6019 | `		}` |
|     77 | 6020 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 6021 | `			break;` |
|      - | 6022 | `		}` |
|      - | 6023 | `		/* Point to the next character */` |
|     71 | 6024 | `		zIn++;` |
|      1 | 6025 | `	}` |
|      - | 6026 | `	/* The test failed,return FALSE */` |
|      7 | 6027 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6028 | `	return PH7_OK;` |
|     10 | 6029 |  |
|      - | 6030 | `/*` |
|      - | 6031 | ` * bool ctype_cntrl(string $text)` |
|      - | 6032 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 6033 | ` * Parameters` |
|      - | 6034 | ` *  $text` |
|      - | 6035 | ` *   The tested string.` |
|      - | 6036 | ` * Return` |
|      - | 6037 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 6038 | ` */` |
|     18 | 6039 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6040 |  |
|      - | 6041 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6042 | `	int nLen;` |
|     19 | 6043 | `	if( nArg < 1 ){` |
|      - | 6044 | `		/* Missing arguments,return FALSE */` |
|      3 | 6045 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6046 | `		return PH7_OK;` |
|      - | 6047 | `	}` |
|      - | 6048 | `	/* Extract the target string */` |
|     17 | 6049 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6050 | `	zEnd = &zIn[nLen];` |
|     17 | 6051 | `	if( nLen < 1 ){` |
|      - | 6052 | `		/* Empty string,return FALSE */` |
|      3 | 6053 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6054 | `		return PH7_OK;` |
|      - | 6055 | `	}` |
|      - | 6056 | `	/* Perform the requested operation */` |
|     14 | 6057 | `	for(;;){` |
|     29 | 6058 | `		if( zIn >= zEnd ){` |
|      - | 6059 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6060 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6061 | `			return PH7_OK;` |
|      - | 6062 | `		}` |
|     21 | 6063 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6064 | `			/* UTF-8 stream  */` |
|    ! 0 | 6065 | `			break;` |
|      - | 6066 | `		}` |
|     21 | 6067 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 6068 | `			break;` |
|      - | 6069 | `		}` |
|      - | 6070 | `		/* Point to the next character */` |
|     15 | 6071 | `		zIn++;` |
|      1 | 6072 | `	}` |
|      - | 6073 | `	/* The test failed,return FALSE */` |
|      7 | 6074 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6075 | `	return PH7_OK;` |
|     10 | 6076 |  |
|      - | 6077 | `/*` |
|      - | 6078 | ` * bool ctype_digit(string $text)` |
|      - | 6079 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 6080 | ` * Parameters` |
|      - | 6081 | ` *  $text` |
|      - | 6082 | ` *   The tested string.` |
|      - | 6083 | ` * Return` |
|      - | 6084 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 6085 | ` */` |
|   1546 | 6086 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6087 |  |
|      - | 6088 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6089 | `	int nLen;` |
|   1548 | 6090 | `	if( nArg < 1 ){` |
|      - | 6091 | `		/* Missing arguments,return FALSE */` |
|      3 | 6092 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6093 | `		return PH7_OK;` |
|      - | 6094 | `	}` |
|      - | 6095 | `	/* Extract the target string */` |
|   1546 | 6096 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1546 | 6097 | `	zEnd = &zIn[nLen];` |
|   1546 | 6098 | `	if( nLen < 1 ){` |
|      - | 6099 | `		/* Empty string,return FALSE */` |
|      3 | 6100 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6101 | `		return PH7_OK;` |
|      - | 6102 | `	}` |
|      - | 6103 | `	/* Perform the requested operation */` |
|   1448 | 6104 | `	for(;;){` |
|   2898 | 6105 | `		if( zIn >= zEnd ){` |
|      - | 6106 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1324 | 6107 | `			ph7_result_bool(pCtx,1);` |
|   1324 | 6108 | `			return PH7_OK;` |
|      - | 6109 | `		}` |
|   1576 | 6110 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6111 | `			/* UTF-8 stream  */` |
|    ! 0 | 6112 | `			break;` |
|      - | 6113 | `		}` |
|   1576 | 6114 | `		if( !SyisDigit(zIn[0]) ){` |
|    222 | 6115 | `			break;` |
|      - | 6116 | `		}` |
|      - | 6117 | `		/* Point to the next character */` |
|   1356 | 6118 | `		zIn++;` |
|      2 | 6119 | `	}` |
|      - | 6120 | `	/* The test failed,return FALSE */` |
|    222 | 6121 | `	ph7_result_bool(pCtx,0);` |
|    222 | 6122 | `	return PH7_OK;` |
|    775 | 6123 |  |
|      - | 6124 | `/*` |
|      - | 6125 | ` * bool ctype_xdigit(string $text)` |
|      - | 6126 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 6127 | ` * Parameters` |
|      - | 6128 | ` *  $text` |
|      - | 6129 | ` *   The tested string.` |
|      - | 6130 | ` * Return` |
|      - | 6131 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 6132 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 6133 | ` */` |
|     20 | 6134 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6135 |  |
|      - | 6136 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6137 | `	int nLen;` |
|     21 | 6138 | `	if( nArg < 1 ){` |
|      - | 6139 | `		/* Missing arguments,return FALSE */` |
|      3 | 6140 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6141 | `		return PH7_OK;` |
|      - | 6142 | `	}` |
|      - | 6143 | `	/* Extract the target string */` |
|     19 | 6144 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 6145 | `	zEnd = &zIn[nLen];` |
|     19 | 6146 | `	if( nLen < 1 ){` |
|      - | 6147 | `		/* Empty string,return FALSE */` |
|      3 | 6148 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6149 | `		return PH7_OK;` |
|      - | 6150 | `	}` |
|      - | 6151 | `	/* Perform the requested operation */` |
|     46 | 6152 | `	for(;;){` |
|     93 | 6153 | `		if( zIn >= zEnd ){` |
|      - | 6154 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 6155 | `			ph7_result_bool(pCtx,1);` |
|     11 | 6156 | `			return PH7_OK;` |
|      - | 6157 | `		}` |
|     83 | 6158 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6159 | `			/* UTF-8 stream  */` |
|    ! 0 | 6160 | `			break;` |
|      - | 6161 | `		}` |
|     83 | 6162 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 6163 | `			break;` |
|      - | 6164 | `		}` |
|      - | 6165 | `		/* Point to the next character */` |
|     77 | 6166 | `		zIn++;` |
|      1 | 6167 | `	}` |
|      - | 6168 | `	/* The test failed,return FALSE */` |
|      7 | 6169 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6170 | `	return PH7_OK;` |
|     11 | 6171 |  |
|      - | 6172 | `/*` |
|      - | 6173 | ` * bool ctype_graph(string $text)` |
|      - | 6174 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 6175 | ` * Parameters` |
|      - | 6176 | ` *  $text` |
|      - | 6177 | ` *   The tested string.` |
|      - | 6178 | ` * Return` |
|      - | 6179 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 6180 | ` * (no white space), FALSE otherwise.` |
|      - | 6181 | ` */` |
|     18 | 6182 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6183 |  |
|      - | 6184 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6185 | `	int nLen;` |
|     19 | 6186 | `	if( nArg < 1 ){` |
|      - | 6187 | `		/* Missing arguments,return FALSE */` |
|      3 | 6188 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6189 | `		return PH7_OK;` |
|      - | 6190 | `	}` |
|      - | 6191 | `	/* Extract the target string */` |
|     17 | 6192 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6193 | `	zEnd = &zIn[nLen];` |
|     17 | 6194 | `	if( nLen < 1 ){` |
|      - | 6195 | `		/* Empty string,return FALSE */` |
|      3 | 6196 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6197 | `		return PH7_OK;` |
|      - | 6198 | `	}` |
|      - | 6199 | `	/* Perform the requested operation */` |
|     57 | 6200 | `	for(;;){` |
|    115 | 6201 | `		if( zIn >= zEnd ){` |
|      - | 6202 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6203 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6204 | `			return PH7_OK;` |
|      - | 6205 | `		}` |
|    107 | 6206 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6207 | `			/* UTF-8 stream  */` |
|    ! 0 | 6208 | `			break;` |
|      - | 6209 | `		}` |
|    107 | 6210 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 6211 | `			break;` |
|      - | 6212 | `		}` |
|      - | 6213 | `		/* Point to the next character */` |
|    101 | 6214 | `		zIn++;` |
|      1 | 6215 | `	}` |
|      - | 6216 | `	/* The test failed,return FALSE */` |
|      7 | 6217 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6218 | `	return PH7_OK;` |
|     10 | 6219 |  |
|      - | 6220 | `/*` |
|      - | 6221 | ` * bool ctype_print(string $text)` |
|      - | 6222 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 6223 | ` * Parameters` |
|      - | 6224 | ` *  $text` |
|      - | 6225 | ` *   The tested string.` |
|      - | 6226 | ` * Return` |
|      - | 6227 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 6228 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 6229 | ` *  or control function at all.` |
|      - | 6230 | ` */` |
|     18 | 6231 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6232 |  |
|      - | 6233 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6234 | `	int nLen;` |
|     19 | 6235 | `	if( nArg < 1 ){` |
|      - | 6236 | `		/* Missing arguments,return FALSE */` |
|      3 | 6237 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6238 | `		return PH7_OK;` |
|      - | 6239 | `	}` |
|      - | 6240 | `	/* Extract the target string */` |
|     17 | 6241 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6242 | `	zEnd = &zIn[nLen];` |
|     17 | 6243 | `	if( nLen < 1 ){` |
|      - | 6244 | `		/* Empty string,return FALSE */` |
|      3 | 6245 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6246 | `		return PH7_OK;` |
|      - | 6247 | `	}` |
|      - | 6248 | `	/* Perform the requested operation */` |
|     63 | 6249 | `	for(;;){` |
|    127 | 6250 | `		if( zIn >= zEnd ){` |
|      - | 6251 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6252 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6253 | `			return PH7_OK;` |
|      - | 6254 | `		}` |
|    119 | 6255 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6256 | `			/* UTF-8 stream  */` |
|    ! 0 | 6257 | `			break;` |
|      - | 6258 | `		}` |
|    119 | 6259 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 6260 | `			break;` |
|      - | 6261 | `		}` |
|      - | 6262 | `		/* Point to the next character */` |
|    113 | 6263 | `		zIn++;` |
|      1 | 6264 | `	}` |
|      - | 6265 | `	/* The test failed,return FALSE */` |
|      7 | 6266 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6267 | `	return PH7_OK;` |
|     10 | 6268 |  |
|      - | 6269 | `/*` |
|      - | 6270 | ` * bool ctype_punct(string $text)` |
|      - | 6271 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 6272 | ` * Parameters` |
|      - | 6273 | ` *  $text` |
|      - | 6274 | ` *   The tested string.` |
|      - | 6275 | ` * Return` |
|      - | 6276 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 6277 | ` *  digit or blank, FALSE otherwise.` |
|      - | 6278 | ` */` |
|     20 | 6279 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6280 |  |
|      - | 6281 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6282 | `	int nLen;` |
|     21 | 6283 | `	if( nArg < 1 ){` |
|      - | 6284 | `		/* Missing arguments,return FALSE */` |
|      3 | 6285 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6286 | `		return PH7_OK;` |
|      - | 6287 | `	}` |
|      - | 6288 | `	/* Extract the target string */` |
|     19 | 6289 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 6290 | `	zEnd = &zIn[nLen];` |
|     19 | 6291 | `	if( nLen < 1 ){` |
|      - | 6292 | `		/* Empty string,return FALSE */` |
|      3 | 6293 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6294 | `		return PH7_OK;` |
|      - | 6295 | `	}` |
|      - | 6296 | `	/* Perform the requested operation */` |
|     38 | 6297 | `	for(;;){` |
|     77 | 6298 | `		if( zIn >= zEnd ){` |
|      - | 6299 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6300 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6301 | `			return PH7_OK;` |
|      - | 6302 | `		}` |
|     69 | 6303 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6304 | `			/* UTF-8 stream  */` |
|    ! 0 | 6305 | `			break;` |
|      - | 6306 | `		}` |
|     69 | 6307 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 6308 | `			break;` |
|      - | 6309 | `		}` |
|      - | 6310 | `		/* Point to the next character */` |
|     61 | 6311 | `		zIn++;` |
|      1 | 6312 | `	}` |
|      - | 6313 | `	/* The test failed,return FALSE */` |
|      9 | 6314 | `	ph7_result_bool(pCtx,0);` |
|      9 | 6315 | `	return PH7_OK;` |
|     11 | 6316 |  |
|      - | 6317 | `/*` |
|      - | 6318 | ` * bool ctype_space(string $text)` |
|      - | 6319 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 6320 | ` * Parameters` |
|      - | 6321 | ` *  $text` |
|      - | 6322 | ` *   The tested string.` |
|      - | 6323 | ` * Return` |
|      - | 6324 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 6325 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 6326 | ` *  and form feed characters.` |
|      - | 6327 | ` */` |
|  57564 | 6328 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6329 |  |
|      - | 6330 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6331 | `	int nLen;` |
|  57566 | 6332 | `	if( nArg < 1 ){` |
|      - | 6333 | `		/* Missing arguments,return FALSE */` |
|      3 | 6334 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6335 | `		return PH7_OK;` |
|      - | 6336 | `	}` |
|      - | 6337 | `	/* Extract the target string */` |
|  57564 | 6338 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  57564 | 6339 | `	zEnd = &zIn[nLen];` |
|  57564 | 6340 | `	if( nLen < 1 ){` |
|      - | 6341 | `		/* Empty string,return FALSE */` |
|      3 | 6342 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6343 | `		return PH7_OK;` |
|      - | 6344 | `	}` |
|      - | 6345 | `	/* Perform the requested operation */` |
|  29787 | 6346 | `	for(;;){` |
|  59532 | 6347 | `		if( zIn >= zEnd ){` |
|      - | 6348 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1948 | 6349 | `			ph7_result_bool(pCtx,1);` |
|   1948 | 6350 | `			return PH7_OK;` |
|      - | 6351 | `		}` |
|  57586 | 6352 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6353 | `			/* UTF-8 stream  */` |
|    ! 0 | 6354 | `			break;` |
|      - | 6355 | `		}` |
|  57586 | 6356 | `		if( !SyisSpace(zIn[0]) ){` |
|  55616 | 6357 | `			break;` |
|      - | 6358 | `		}` |
|      - | 6359 | `		/* Point to the next character */` |
|   1972 | 6360 | `		zIn++;` |
|      2 | 6361 | `	}` |
|      - | 6362 | `	/* The test failed,return FALSE */` |
|  55616 | 6363 | `	ph7_result_bool(pCtx,0);` |
|  55616 | 6364 | `	return PH7_OK;` |
|  28806 | 6365 |  |
|      - | 6366 | `/*` |
|      - | 6367 | ` * bool ctype_lower(string $text)` |
|      - | 6368 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 6369 | ` * Parameters` |
|      - | 6370 | ` *  $text` |
|      - | 6371 | ` *   The tested string.` |
|      - | 6372 | ` * Return` |
|      - | 6373 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 6374 | ` */` |
|     18 | 6375 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6376 |  |
|      - | 6377 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6378 | `	int nLen;` |
|     19 | 6379 | `	if( nArg < 1 ){` |
|      - | 6380 | `		/* Missing arguments,return FALSE */` |
|      3 | 6381 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6382 | `		return PH7_OK;` |
|      - | 6383 | `	}` |
|      - | 6384 | `	/* Extract the target string */` |
|     17 | 6385 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6386 | `	zEnd = &zIn[nLen];` |
|     17 | 6387 | `	if( nLen < 1 ){` |
|      - | 6388 | `		/* Empty string,return FALSE */` |
|      3 | 6389 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6390 | `		return PH7_OK;` |
|      - | 6391 | `	}` |
|      - | 6392 | `	/* Perform the requested operation */` |
|     27 | 6393 | `	for(;;){` |
|     55 | 6394 | `		if( zIn >= zEnd ){` |
|      - | 6395 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 6396 | `			ph7_result_bool(pCtx,1);` |
|      5 | 6397 | `			return PH7_OK;` |
|      - | 6398 | `		}` |
|     51 | 6399 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 6400 | `			break;` |
|      - | 6401 | `		}` |
|      - | 6402 | `		/* Point to the next character */` |
|     41 | 6403 | `		zIn++;` |
|      1 | 6404 | `	}` |
|      - | 6405 | `	/* The test failed,return FALSE */` |
|     11 | 6406 | `	ph7_result_bool(pCtx,0);` |
|     11 | 6407 | `	return PH7_OK;` |
|     10 | 6408 |  |
|      - | 6409 | `/*` |
|      - | 6410 | ` * bool ctype_upper(string $text)` |
|      - | 6411 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 6412 | ` * Parameters` |
|      - | 6413 | ` *  $text` |
|      - | 6414 | ` *   The tested string.` |
|      - | 6415 | ` * Return` |
|      - | 6416 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 6417 | ` */` |
|     18 | 6418 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6419 |  |
|      - | 6420 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6421 | `	int nLen;` |
|     19 | 6422 | `	if( nArg < 1 ){` |
|      - | 6423 | `		/* Missing arguments,return FALSE */` |
|      3 | 6424 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6425 | `		return PH7_OK;` |
|      - | 6426 | `	}` |
|      - | 6427 | `	/* Extract the target string */` |
|     17 | 6428 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6429 | `	zEnd = &zIn[nLen];` |
|     17 | 6430 | `	if( nLen < 1 ){` |
|      - | 6431 | `		/* Empty string,return FALSE */` |
|      3 | 6432 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6433 | `		return PH7_OK;` |
|      - | 6434 | `	}` |
|      - | 6435 | `	/* Perform the requested operation */` |
|     28 | 6436 | `	for(;;){` |
|     57 | 6437 | `		if( zIn >= zEnd ){` |
|      - | 6438 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 6439 | `			ph7_result_bool(pCtx,1);` |
|      5 | 6440 | `			return PH7_OK;` |
|      - | 6441 | `		}` |
|     53 | 6442 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 6443 | `			break;` |
|      - | 6444 | `		}` |
|      - | 6445 | `		/* Point to the next character */` |
|     43 | 6446 | `		zIn++;` |
|      1 | 6447 | `	}` |
|      - | 6448 | `	/* The test failed,return FALSE */` |
|     11 | 6449 | `	ph7_result_bool(pCtx,0);` |
|     11 | 6450 | `	return PH7_OK;` |
|     10 | 6451 |  |
|      - | 6452 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 6453 | `/*` |
|      - | 6454 | ` * Section:` |
|      - | 6455 | ` *    URL handling Functions.` |
|      - | 6456 | ` * Status:` |
|      - | 6457 | ` *    Stable.` |
|      - | 6458 | ` */` |
|      - | 6459 | `/*` |
|      - | 6460 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 6461 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 6462 | ` */` |
|   1026 | 6463 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 6464 |  |
|      - | 6465 | `	/* Store in the call context result buffer */` |
|   1028 | 6466 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 6467 | `	return SXRET_OK;` |
|      2 | 6468 |  |
|      - | 6469 | `/*` |
|      - | 6470 | ` * string base64_encode(string $data)` |
|      - | 6471 | ` * string convert_uuencode(string $data)` |
|      - | 6472 | ` *  Encodes data with MIME base64` |
|      - | 6473 | ` * Parameter` |
|      - | 6474 | ` *  $data` |
|      - | 6475 | ` *    Data to encode` |
|      - | 6476 | ` * Return` |
|      - | 6477 | ` *  Encoded data or FALSE on failure.` |
|      - | 6478 | ` */` |
|     10 | 6479 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6480 |  |
|      - | 6481 | `	const char *zIn;` |
|      - | 6482 | `	int nLen;` |
|     11 | 6483 | `	if( nArg < 1 ){` |
|      - | 6484 | `		/* Missing arguments,return FALSE */` |
|      5 | 6485 | `		ph7_result_bool(pCtx,0);` |
|      5 | 6486 | `		return PH7_OK;` |
|      - | 6487 | `	}` |
|      - | 6488 | `	/* Extract the input string */` |
|      7 | 6489 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 6490 | `	if( nLen < 1 ){` |
|      - | 6491 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 6492 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6493 | `		return PH7_OK;` |
|      - | 6494 | `	}` |
|      - | 6495 | `	/* Perform the BASE64 encoding */` |
|      7 | 6496 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 6497 | `	return PH7_OK;` |
|      6 | 6498 |  |
|      - | 6499 | `/*` |
|      - | 6500 | ` * string base64_decode(string $data)` |
|      - | 6501 | ` * string convert_uudecode(string $data)` |
|      - | 6502 | ` *  Decodes data encoded with MIME base64` |
|      - | 6503 | ` * Parameter` |
|      - | 6504 | ` *  $data` |
|      - | 6505 | ` *    Encoded data.` |
|      - | 6506 | ` * Return` |
|      - | 6507 | ` *  Returns the original data or FALSE on failure.` |
|      - | 6508 | ` */` |
|     36 | 6509 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6510 |  |
|      - | 6511 | `	const char *zIn;` |
|      - | 6512 | `	int nLen;` |
|     38 | 6513 | `	if( nArg < 1 ){` |
|      - | 6514 | `		/* Missing arguments,return FALSE */` |
|      3 | 6515 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6516 | `		return PH7_OK;` |
|      - | 6517 | `	}` |
|      - | 6518 | `	/* Extract the input string */` |
|     36 | 6519 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 6520 | `	if( nLen < 1 ){` |
|      - | 6521 | `		/* Nothing to process,return FALSE */` |
|      3 | 6522 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6523 | `		return PH7_OK;` |
|      - | 6524 | `	}` |
|      - | 6525 | `	/* Perform the BASE64 decoding */` |
|     34 | 6526 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 6527 | `	return PH7_OK;` |
|     20 | 6528 |  |
|      - | 6529 | `/*` |
|      - | 6530 | ` * string urlencode(string $str)` |
|      - | 6531 | ` *  URL encoding` |
|      - | 6532 | ` * Parameter` |
|      - | 6533 | ` *  $data` |
|      - | 6534 | ` *   Input string.` |
|      - | 6535 | ` * Return` |
|      - | 6536 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 6537 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 6538 | ` *  encoded as plus (+) signs.` |
|      - | 6539 | ` */` |
|      6 | 6540 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6541 |  |
|      - | 6542 | `	const char *zIn;` |
|      - | 6543 | `	int nLen;` |
|      7 | 6544 | `	if( nArg < 1 ){` |
|      - | 6545 | `		/* Missing arguments,return FALSE */` |
|      3 | 6546 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6547 | `		return PH7_OK;` |
|      - | 6548 | `	}` |
|      - | 6549 | `	/* Extract the input string */` |
|      5 | 6550 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 6551 | `	if( nLen < 1 ){` |
|      - | 6552 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 6553 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6554 | `		return PH7_OK;` |
|      - | 6555 | `	}` |
|      - | 6556 | `	/* Perform the URL encoding */` |
|      5 | 6557 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 6558 | `	return PH7_OK;` |
|      4 | 6559 |  |
|      - | 6560 | `/*` |
|      - | 6561 | ` * string urldecode(string $str)` |
|      - | 6562 | ` *  Decodes any %## encoding in the given string.` |
|      - | 6563 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 6564 | ` * Parameter` |
|      - | 6565 | ` *  $data` |
|      - | 6566 | ` *    Input string.` |
|      - | 6567 | ` * Return` |
|      - | 6568 | ` *  Decoded URL or FALSE on failure.` |
|      - | 6569 | ` */` |
|      8 | 6570 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6571 |  |
|      - | 6572 | `	const char *zIn;` |
|      - | 6573 | `	int nLen;` |
|      9 | 6574 | `	if( nArg < 1 ){` |
|      - | 6575 | `		/* Missing arguments,return FALSE */` |
|      3 | 6576 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6577 | `		return PH7_OK;` |
|      - | 6578 | `	}` |
|      - | 6579 | `	/* Extract the input string */` |
|      7 | 6580 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 6581 | `	if( nLen < 1 ){` |
|      - | 6582 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 6583 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6584 | `		return PH7_OK;` |
|      - | 6585 | `	}` |
|      - | 6586 | `	/* Perform the URL decoding */` |
|      7 | 6587 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 6588 | `	return PH7_OK;` |
|      5 | 6589 |  |
|      - | 6590 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6591 | `/* Table of the built-in functions */` |
|      - | 6592 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 6593 | `	   /* Variable handling functions */` |
|      - | 6594 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 6595 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 6596 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 6597 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 6598 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 6599 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 6600 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 6601 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 6602 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 6603 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 6604 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 6605 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 6606 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 6607 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 6608 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 6609 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 6610 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 6611 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 6612 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 6613 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 6614 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6615 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 6616 | `	   /* Math functions */` |
|      - | 6617 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 6618 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 6619 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 6620 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 6621 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 6622 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 6623 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 6624 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 6625 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 6626 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 6627 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 6628 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 6629 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 6630 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 6631 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 6632 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 6633 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 6634 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 6635 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 6636 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 6637 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 6638 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 6639 | `	{ "round",    PH7_builtin_round        },` |
|      - | 6640 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 6641 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 6642 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 6643 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 6644 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 6645 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 6646 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 6647 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 6648 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 6649 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6650 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6651 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 6652 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6653 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6654 | `	   /* String handling functions */` |
|      - | 6655 |  |
|      - | 6656 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 6657 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 6658 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 6659 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 6660 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 6661 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 6662 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 6663 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 6664 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 6665 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 6666 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 6667 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 6668 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 6669 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 6670 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 6671 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 6672 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 6673 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 6674 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 6675 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 6676 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 6677 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 6678 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 6679 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 6680 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 6681 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 6682 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 6683 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 6684 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 6685 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 6686 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 6687 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 6688 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 6689 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 6690 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 6691 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 6692 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 6693 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 6694 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 6695 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 6696 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 6697 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 6698 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 6699 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 6700 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 6701 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 6702 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 6703 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 6704 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 6705 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6706 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6707 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 6708 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 6709 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 6710 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 6711 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6712 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6713 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 6714 |  |
|      - | 6715 |  |
|      - | 6716 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 6717 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 6718 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 6719 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 6720 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 6721 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6722 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6723 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 6724 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 6725 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6726 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6727 |  |
|      - | 6728 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 6729 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 6730 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 6731 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 6732 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 6733 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 6734 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 6735 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 6736 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 6737 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 6738 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 6739 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 6740 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6741 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6742 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 6743 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6744 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6745 |  |
|      - | 6746 | `	         /* Ctype functions */` |
|      - | 6747 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 6748 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 6749 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 6750 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 6751 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 6752 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 6753 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 6754 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 6755 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 6756 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 6757 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 6758 | `	         /* Time functions */` |
|      - | 6759 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 6760 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 6761 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 6762 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 6763 | `	{ "date",        PH7_builtin_date         },` |
|      - | 6764 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 6765 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 6766 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 6767 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 6768 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 6769 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 6770 | `	        /* URL functions */` |
|      - | 6771 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 6772 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 6773 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 6774 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 6775 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 6776 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 6777 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 6778 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 6779 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6780 | `};` |
|      - | 6781 | `/*` |
|      - | 6782 | ` * Register the built-in functions defined above,the array functions` |
|      - | 6783 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 6784 | ` */` |
|   2622 | 6785 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      2 | 6786 |  |
|      - | 6787 | `	sxu32 n;` |
| 406412 | 6788 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 403790 | 6789 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 201896 | 6790 | `	}` |
|      - | 6791 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   2624 | 6792 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 6793 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   2624 | 6794 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   2624 | 6795 |  |
|      - | 6796 |  |
