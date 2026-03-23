# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2929/3342 lines (87.64%)

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
|    128 |   62 | `static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |   63 |  |
|    130 |   64 | `	int res = 0; /* Assume false by default */` |
|    130 |   65 | `	if( nArg > 0 ){` |
|    128 |   66 | `		res = ph7_value_is_int(apArg[0]);` |
|     63 |   67 | `	}` |
|      - |   68 | `	/* Query result */` |
|    130 |   69 | `	ph7_result_bool(pCtx,res);` |
|    130 |   70 | `	return PH7_OK;` |
|      2 |   71 |  |
|      - |   72 | `/*` |
|      - |   73 | ` * bool is_string($var)` |
|      - |   74 | ` *  Finds out whether a variable is a string.` |
|      - |   75 | ` * Parameters` |
|      - |   76 | ` *   $var: The variable being evaluated.` |
|      - |   77 | ` * Return` |
|      - |   78 | ` *  TRUE if var is string. False otherwise.` |
|      - |   79 | ` */` |
|     76 |   80 | `static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   81 |  |
|     77 |   82 | `	int res = 0; /* Assume false by default */` |
|     77 |   83 | `	if( nArg > 0 ){` |
|     75 |   84 | `		res = ph7_value_is_string(apArg[0]);` |
|     37 |   85 | `	}` |
|      - |   86 | `	/* Query result */` |
|     77 |   87 | `	ph7_result_bool(pCtx,res);` |
|     77 |   88 | `	return PH7_OK;` |
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
|    186 |  152 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  153 |  |
|    188 |  154 | `	int res = 0; /* Assume false by default */` |
|    188 |  155 | `	if( nArg > 0 ){` |
|    186 |  156 | `		res = ph7_value_is_array(apArg[0]);` |
|     92 |  157 | `	}` |
|      - |  158 | `	/* Query result */` |
|    188 |  159 | `	ph7_result_bool(pCtx,res);` |
|    188 |  160 | `	return PH7_OK;` |
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
|      - |  262 | ` * bool empty($var)` |
|      - |  263 | ` *  Determine whether a variable is empty.` |
|      - |  264 | ` * Parameters` |
|      - |  265 | ` *   $var: The variable being checked.` |
|      - |  266 | ` * Return` |
|      - |  267 | ` *  0 if var has a non-empty and non-zero value.1 otherwise.` |
|      - |  268 | ` */` |
|  19414 |  269 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  270 |  |
|  19416 |  271 | `	int res = 1; /* Assume empty by default */` |
|  19416 |  272 | `	if( nArg > 0 ){` |
|  19414 |  273 | `		res = ph7_value_is_empty(apArg[0]);` |
|   9706 |  274 | `	}` |
|  19416 |  275 | `	ph7_result_bool(pCtx,res);` |
|  19416 |  276 | `	return PH7_OK;` |
|      - |  277 |  |
|      2 |  278 |  |
|      - |  279 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |  280 | `#define PH7_NEED_BUILTIN_REG 1` |
|      - |  281 | `#endif` |
|      - |  282 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - |  283 | `#define PH7_NEED_FMT_AND_INI 1` |
|      - |  284 | `#endif` |
|      - |  285 |  |
|      - |  286 | `/* Math functions moved to builtin_math.c */` |
|      - |  287 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - |  288 | `/*` |
|      - |  289 | ` * Section:` |
|      - |  290 | ` *    String handling Functions.` |
|      - |  291 | ` * Status:` |
|      - |  292 | ` *    Stable.` |
|      - |  293 | ` */` |
|      - |  294 | `/*` |
|      - |  295 | ` * string substr(string $string,int $start[, int $length ])` |
|      - |  296 | ` *  Return part of a string.` |
|      - |  297 | ` * Parameters` |
|      - |  298 | ` *  $string` |
|      - |  299 | ` *   The input string. Must be one character or longer.` |
|      - |  300 | ` * $start` |
|      - |  301 | ` *   If start is non-negative, the returned string will start at the start'th position` |
|      - |  302 | ` *   in string, counting from zero. For instance, in the string 'abcdef', the character` |
|      - |  303 | ` *   at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - |  304 | ` *   If start is negative, the returned string will start at the start'th character` |
|      - |  305 | ` *   from the end of string.` |
|      - |  306 | ` *   If string is less than or equal to start characters long, FALSE will be returned.` |
|      - |  307 | ` * $length` |
|      - |  308 | ` *   If length is given and is positive, the string returned will contain at most length` |
|      - |  309 | ` *   characters beginning from start (depending on the length of string).` |
|      - |  310 | ` *   If length is given and is negative, then that many characters will be omitted from` |
|      - |  311 | ` *   the end of string (after the start position has been calculated when a start is negative).` |
|      - |  312 | ` *   If start denotes the position of this truncation or beyond, false will be returned.` |
|      - |  313 | ` *   If length is given and is 0, FALSE or NULL an empty string will be returned.` |
|      - |  314 | ` *   If length is omitted, the substring starting from start until the end of the string` |
|      - |  315 | ` *   will be returned.` |
|      - |  316 | ` * Return` |
|      - |  317 | ` *  Returns the extracted part of string, or FALSE on failure or an empty string.` |
|      - |  318 | ` */` |
| 135756 |  319 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  320 |  |
|      - |  321 | `	const char *zSource,*zOfft;` |
|      - |  322 | `	int nOfft,nLen,nSrcLen;` |
| 135758 |  323 | `	if( nArg < 2 ){` |
|      - |  324 | `		/* return FALSE */` |
|      5 |  325 | `		ph7_result_bool(pCtx,0);` |
|      5 |  326 | `		return PH7_OK;` |
|      - |  327 | `	}` |
|      - |  328 | `	/* Extract the target string */` |
| 135754 |  329 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 135754 |  330 | `	if( nSrcLen < 1 ){` |
|      - |  331 | `		/* Empty string,return FALSE */` |
|   8276 |  332 | `		ph7_result_bool(pCtx,0);` |
|   8276 |  333 | `		return PH7_OK;` |
|      - |  334 | `	}` |
| 127480 |  335 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  336 | `	/* Extract the offset */` |
| 127480 |  337 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 127480 |  338 | `	if( nOfft < 0 ){` |
|  21980 |  339 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  21980 |  340 | `		if( zOfft < zSource ){` |
|      - |  341 | `			/* Invalid offset */` |
|      5 |  342 | `			ph7_result_bool(pCtx,0);` |
|      5 |  343 | `			return PH7_OK;` |
|      - |  344 | `		}` |
|  21976 |  345 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  21976 |  346 | `		nOfft = (int)(zOfft-zSource);` |
| 116489 |  347 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  348 | `		/* Invalid offset */` |
|      7 |  349 | `		ph7_result_bool(pCtx,0);` |
|      7 |  350 | `		return PH7_OK;` |
|    ! 0 |  351 | `	}else{` |
| 105496 |  352 | `		zOfft = &zSource[nOfft];` |
| 105496 |  353 | `		nLen = nSrcLen - nOfft;` |
|      - |  354 | `	}` |
| 127470 |  355 | `	if( nArg > 2 ){` |
|      - |  356 | `		/* Extract the length */` |
| 105494 |  357 | `		nLen = ph7_value_to_int(apArg[2]);` |
| 105494 |  358 | `		if( nLen == 0 ){` |
|      - |  359 | `			/* Invalid length,return an empty string */` |
|      5 |  360 | `			ph7_result_string(pCtx,"",0);` |
|      5 |  361 | `			return PH7_OK;` |
| 105490 |  362 | `		}else if( nLen < 0 ){` |
|  21978 |  363 | `			nLen = nSrcLen + nLen - nOfft;` |
|  21978 |  364 | `			if( nLen < 1 ){` |
|      - |  365 | `				/* Invalid  length */` |
|      3 |  366 | `				nLen = nSrcLen - nOfft;` |
|      1 |  367 | `			}` |
|  10988 |  368 | `		}` |
| 105490 |  369 | `		if( nLen + nOfft > nSrcLen ){` |
|      - |  370 | `			/* Invalid length */` |
|   2534 |  371 | `			nLen = nSrcLen - nOfft;` |
|   1266 |  372 | `		}` |
|  52744 |  373 | `	}` |
|      - |  374 | `	/* Return the substring */` |
| 127466 |  375 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 127466 |  376 | `	return PH7_OK;` |
|  67880 |  377 |  |
|      - |  378 | `/*` |
|      - |  379 | ` * int substr_compare(string $main_str,string $str ,int $offset[,int $length[,bool $case_insensitivity = false ]])` |
|      - |  380 | ` *  Binary safe comparison of two strings from an offset, up to length characters.` |
|      - |  381 | ` * Parameters` |
|      - |  382 | ` *  $main_str` |
|      - |  383 | ` *  The main string being compared.` |
|      - |  384 | ` *  $str` |
|      - |  385 | ` *   The secondary string being compared.` |
|      - |  386 | ` * $offset` |
|      - |  387 | ` *  The start position for the comparison. If negative, it starts counting from` |
|      - |  388 | ` *  the end of the string.` |
|      - |  389 | ` * $length` |
|      - |  390 | ` *  The length of the comparison. The default value is the largest of the length` |
|      - |  391 | ` *  of the str compared to the length of main_str less the offset.` |
|      - |  392 | ` * $case_insensitivity` |
|      - |  393 | ` *  If case_insensitivity is TRUE, comparison is case insensitive.` |
|      - |  394 | ` * Return` |
|      - |  395 | ` *  Returns < 0 if main_str from position offset is less than str, > 0 if it is greater than` |
|      - |  396 | ` *  str, and 0 if they are equal. If offset is equal to or greater than the length of main_str` |
|      - |  397 | ` *  or length is set and is less than 1, substr_compare() prints a warning and returns FALSE.` |
|      - |  398 | ` */` |
|     26 |  399 | `static int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  400 |  |
|      - |  401 | `	const char *zSource,*zOfft,*zSub;` |
|      - |  402 | `	int nOfft,nLen,nSrcLen,nSublen;` |
|     27 |  403 | `	int iCase = 0;` |
|      - |  404 | `	int rc;` |
|     27 |  405 | `	if( nArg < 3 ){` |
|      - |  406 | `		/* Missing arguments,return FALSE */` |
|      5 |  407 | `		ph7_result_bool(pCtx,0);` |
|      5 |  408 | `		return PH7_OK;` |
|      - |  409 | `	}` |
|      - |  410 | `	/* Extract the target string */` |
|     23 |  411 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|     23 |  412 | `	if( nSrcLen < 1 ){` |
|      - |  413 | `		/* Empty string,return FALSE */` |
|      3 |  414 | `		ph7_result_bool(pCtx,0);` |
|      3 |  415 | `		return PH7_OK;` |
|      - |  416 | `	}` |
|     21 |  417 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  418 | `	/* Extract the substring */` |
|     21 |  419 | `	zSub = ph7_value_to_string(apArg[1],&nSublen);` |
|     21 |  420 | `	if( nSublen < 1 \|\| nSublen > nSrcLen){` |
|      - |  421 | `		/* Empty string,return FALSE */` |
|      3 |  422 | `		ph7_result_bool(pCtx,0);` |
|      3 |  423 | `		return PH7_OK;` |
|      - |  424 | `	}` |
|      - |  425 | `	/* Extract the offset */` |
|     19 |  426 | `	nOfft = ph7_value_to_int(apArg[2]);` |
|     19 |  427 | `	if( nOfft < 0 ){` |
|      5 |  428 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|      5 |  429 | `		if( zOfft < zSource ){` |
|      - |  430 | `			/* Invalid offset */` |
|      3 |  431 | `			ph7_result_bool(pCtx,0);` |
|      3 |  432 | `			return PH7_OK;` |
|      - |  433 | `		}` |
|      3 |  434 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|      3 |  435 | `		nOfft = (int)(zOfft-zSource);` |
|     16 |  436 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  437 | `		/* Invalid offset */` |
|      3 |  438 | `		ph7_result_bool(pCtx,0);` |
|      3 |  439 | `		return PH7_OK;` |
|    ! 0 |  440 | `	}else{` |
|     13 |  441 | `		zOfft = &zSource[nOfft];` |
|     13 |  442 | `		nLen = nSrcLen - nOfft;` |
|      - |  443 | `	}` |
|     15 |  444 | `	if( nArg > 3 ){` |
|      - |  445 | `		/* Extract the length */` |
|     13 |  446 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     13 |  447 | `		if( nLen < 1 ){` |
|      - |  448 | `			/* Invalid  length */` |
|      5 |  449 | `			ph7_result_int(pCtx,1);` |
|      5 |  450 | `			return PH7_OK;` |
|      9 |  451 | `		}else if( nLen + nOfft > nSrcLen ){` |
|      - |  452 | `			/* Invalid length */` |
|      3 |  453 | `			nLen = nSrcLen - nOfft;` |
|      1 |  454 | `		}` |
|      9 |  455 | `		if( nArg > 4 ){` |
|      - |  456 | `			/* Case-sensitive or not */` |
|      5 |  457 | `			iCase = ph7_value_to_bool(apArg[4]);` |
|      2 |  458 | `		}` |
|      4 |  459 | `	}` |
|      - |  460 | `	/* Perform the comparison */` |
|     11 |  461 | `	if( iCase ){` |
|      3 |  462 | `		rc = SyStrnicmp(zOfft,zSub,(sxu32)nLen);` |
|      2 |  463 | `	}else{` |
|      9 |  464 | `		rc = SyStrncmp(zOfft,zSub,(sxu32)nLen);` |
|      - |  465 | `	}` |
|      - |  466 | `	/* Comparison result */` |
|     11 |  467 | `	ph7_result_int(pCtx,rc);` |
|     11 |  468 | `	return PH7_OK;` |
|     14 |  469 |  |
|      - |  470 | `/*` |
|      - |  471 | ` * int substr_count(string $haystack,string $needle[,int $offset = 0 [,int $length ]])` |
|      - |  472 | ` *  Count the number of substring occurrences.` |
|      - |  473 | ` * Parameters` |
|      - |  474 | ` * $haystack` |
|      - |  475 | ` *   The string to search in` |
|      - |  476 | ` * $needle` |
|      - |  477 | ` *   The substring to search for` |
|      - |  478 | ` * $offset` |
|      - |  479 | ` *  The offset where to start counting` |
|      - |  480 | ` * $length (NOT USED)` |
|      - |  481 | ` *  The maximum length after the specified offset to search for the substring.` |
|      - |  482 | ` *  It outputs a warning if the offset plus the length is greater than the haystack length.` |
|      - |  483 | ` * Return` |
|      - |  484 | ` *  Toral number of substring occurrences.` |
|      - |  485 | ` */` |
|     24 |  486 | `static int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  487 |  |
|      - |  488 | `	const char *zText,*zPattern,*zEnd;` |
|      - |  489 | `	int nTextlen,nPatlen;` |
|     25 |  490 | `	int iCount = 0;` |
|      - |  491 | `	sxu32 nOfft;` |
|      - |  492 | `	sxi32 rc;` |
|     25 |  493 | `	if( nArg < 2 ){` |
|      - |  494 | `		/* Missing arguments */` |
|      5 |  495 | `		ph7_result_int(pCtx,0);` |
|      5 |  496 | `		return PH7_OK;` |
|      - |  497 | `	}` |
|      - |  498 | `	/* Point to the haystack */` |
|     21 |  499 | `	zText = ph7_value_to_string(apArg[0],&nTextlen);` |
|      - |  500 | `	/* Point to the neddle */` |
|     21 |  501 | `	zPattern = ph7_value_to_string(apArg[1],&nPatlen);` |
|     21 |  502 | `	if( nTextlen < 1 \|\| nPatlen < 1 \|\| nPatlen > nTextlen ){` |
|      - |  503 | `		/* NOOP,return zero */` |
|      3 |  504 | `		ph7_result_int(pCtx,0);` |
|      3 |  505 | `		return PH7_OK;` |
|      - |  506 | `	}` |
|     19 |  507 | `	if( nArg > 2 ){` |
|      - |  508 | `		int iOfft;` |
|      - |  509 | `		/* Extract the offset */` |
|     15 |  510 | `		iOfft = ph7_value_to_int(apArg[2]);` |
|     15 |  511 | `		if( iOfft < 0 \|\| iOfft > nTextlen ){` |
|      - |  512 | `			/* Invalid offset,return zero */` |
|      3 |  513 | `			ph7_result_int(pCtx,0);` |
|      3 |  514 | `			return PH7_OK;` |
|      - |  515 | `		}` |
|      - |  516 | `		/* Point to the desired offset */` |
|     13 |  517 | `		zText = &zText[iOfft];` |
|      - |  518 | `		/* Adjust length */` |
|     13 |  519 | `		nTextlen -= iOfft;` |
|      6 |  520 | `	}` |
|      - |  521 | `	/* Point to the end of the string */` |
|     17 |  522 | `	zEnd = &zText[nTextlen];` |
|     17 |  523 | `	if( nArg > 3 ){` |
|      - |  524 | `		int nLen;` |
|      - |  525 | `		/* Extract the length */` |
|     13 |  526 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     13 |  527 | `		if( nLen < 0 \|\| nLen > nTextlen ){` |
|      - |  528 | `			/* Invalid length,return 0 */` |
|      7 |  529 | `			ph7_result_int(pCtx,0);` |
|      7 |  530 | `			return PH7_OK;` |
|      - |  531 | `		}` |
|      - |  532 | `		/* Adjust pointer */` |
|      7 |  533 | `		nTextlen = nLen;` |
|      7 |  534 | `		zEnd = &zText[nTextlen];` |
|      3 |  535 | `	}` |
|      - |  536 | `	/* Perform the search */` |
|     12 |  537 | `	for(;;){` |
|     25 |  538 | `		rc = SyBlobSearch((const void *)zText,(sxu32)(zEnd-zText),(const void *)zPattern,nPatlen,&nOfft);` |
|     25 |  539 | `		if( rc != SXRET_OK ){` |
|      - |  540 | `			/* Pattern not found,break immediately */` |
|      9 |  541 | `			break;` |
|      - |  542 | `		}` |
|      - |  543 | `		/* Increment counter and update the offset */` |
|     17 |  544 | `		iCount++;` |
|     17 |  545 | `		zText += nOfft + nPatlen;` |
|     17 |  546 | `		if( zText >= zEnd ){` |
|      3 |  547 | `			break;` |
|      - |  548 | `		}` |
|      1 |  549 | `	}` |
|      - |  550 | `	/* Pattern count */` |
|     11 |  551 | `	ph7_result_int(pCtx,iCount);` |
|     11 |  552 | `	return PH7_OK;` |
|     13 |  553 |  |
|      - |  554 | `/*` |
|      - |  555 | ` * string chunk_split(string $body[,int $chunklen = 76 [, string $end = "\r\n" ]])` |
|      - |  556 | ` *   Split a string into smaller chunks.` |
|      - |  557 | ` * Parameters` |
|      - |  558 | ` *  $body` |
|      - |  559 | ` *   The string to be chunked.` |
|      - |  560 | ` * $chunklen` |
|      - |  561 | ` *   The chunk length.` |
|      - |  562 | ` * $end` |
|      - |  563 | ` *   The line ending sequence.` |
|      - |  564 | ` * Return` |
|      - |  565 | ` *  The chunked string or NULL on failure.` |
|      - |  566 | ` */` |
|     16 |  567 | `static int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  568 |  |
|     17 |  569 | `	const char *zIn,*zEnd,*zSep = "\r\n";` |
|      - |  570 | `	int nSepLen,nChunkLen,nLen;` |
|     17 |  571 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  572 | `		/* Nothing to split,return null */` |
|      5 |  573 | `		ph7_result_null(pCtx);` |
|      5 |  574 | `		return PH7_OK;` |
|      - |  575 | `	}` |
|      - |  576 | `	/* initialize/Extract arguments */` |
|     13 |  577 | `	nSepLen = (int)sizeof("\r\n") - 1;` |
|     13 |  578 | `	nChunkLen = 76;` |
|     13 |  579 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 |  580 | `	zEnd = &zIn[nLen];` |
|     13 |  581 | `	if( nArg > 1 ){` |
|      - |  582 | `		/* Chunk length */` |
|     13 |  583 | `		nChunkLen = ph7_value_to_int(apArg[1]);` |
|     13 |  584 | `		if( nChunkLen < 1 ){` |
|      - |  585 | `			/* Switch back to the default length */` |
|      3 |  586 | `			nChunkLen = 76;` |
|      1 |  587 | `		}` |
|     13 |  588 | `		if( nArg > 2 ){` |
|      - |  589 | `			/* Separator */` |
|      9 |  590 | `			zSep = ph7_value_to_string(apArg[2],&nSepLen);` |
|      9 |  591 | `			if( nSepLen < 1 ){` |
|      - |  592 | `				/* Switch back to the default separator */` |
|      3 |  593 | `				zSep = "\r\n";` |
|      3 |  594 | `				nSepLen = (int)sizeof("\r\n") - 1;` |
|      1 |  595 | `			}` |
|      4 |  596 | `		}` |
|      6 |  597 | `	}` |
|      - |  598 | `	/* Perform the requested operation */` |
|     13 |  599 | `	if( nChunkLen > nLen ){` |
|      - |  600 | `		/* Nothing to split,return the string and the separator */` |
|      9 |  601 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);` |
|      9 |  602 | `		return PH7_OK;` |
|      - |  603 | `	}` |
|     17 |  604 | `	while( zIn < zEnd ){` |
|     13 |  605 | `		if( nChunkLen > (int)(zEnd-zIn) ){` |
|      3 |  606 | `			nChunkLen = (int)(zEnd - zIn);` |
|      1 |  607 | `		}` |
|      - |  608 | `		/* Append the chunk and the separator */` |
|     13 |  609 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);` |
|      - |  610 | `		/* Point beyond the chunk */` |
|     13 |  611 | `		zIn += nChunkLen;` |
|      1 |  612 | `	}` |
|      5 |  613 | `	return PH7_OK;` |
|      9 |  614 |  |
|      - |  615 | `/*` |
|      - |  616 | ` * string addslashes(string $str)` |
|      - |  617 | ` *  Quote string with slashes.` |
|      - |  618 | ` *  Returns a string with backslashes before characters that need` |
|      - |  619 | ` *  to be quoted in database queries etc. These characters are single` |
|      - |  620 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - |  621 | ` * Parameter` |
|      - |  622 | ` *  str: The string to be escaped.` |
|      - |  623 | ` * Return` |
|      - |  624 | ` *  Returns the escaped string` |
|      - |  625 | ` */` |
|     24 |  626 | `static int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  627 |  |
|      - |  628 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  629 | `	int nLen;` |
|      - |  630 | `	/* PHP enforces exactly one argument. */` |
|     26 |  631 | `	if( nArg != 1 ){` |
|      7 |  632 | `		return PH7_VmThrowException(pCtx,` |
|      - |  633 | `			"ArgumentCountError",` |
|      - |  634 | `			"addslashes() expects exactly 1 argument, %d given",` |
|      2 |  635 | `			nArg` |
|      - |  636 | `			);` |
|      - |  637 | `	}` |
|      - |  638 | `	/* NULL is deprecated and treated as an empty string; other invalid` |
|      - |  639 | `	 * types still produce a TypeError. */` |
|     22 |  640 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 |  641 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - |  642 | `			E_DEPRECATED,` |
|      - |  643 | `			"addslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - |  644 | `			);` |
|      - |  645 | `		/* fall through so conversion below yields empty string */` |
|      1 |  646 | `	}` |
|      - |  647 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     38 |  648 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     29 |  649 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     18 |  650 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 |  651 | `		return PH7_VmThrowException(pCtx,` |
|      - |  652 | `			"TypeError",` |
|      - |  653 | `			"addslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 |  654 | `			ph7_type_name(apArg[0])` |
|      - |  655 | `			);` |
|      - |  656 | `	}` |
|      - |  657 | `	/* Convert to string representation first and obtain length. */` |
|     19 |  658 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     19 |  659 | `	if( nLen < 1 ){` |
|      - |  660 | `		/* Return the empty string */` |
|      5 |  661 | `		ph7_result_string(pCtx,"",0);` |
|      5 |  662 | `		return PH7_OK;` |
|      - |  663 | `	}` |
|     15 |  664 | `	zEnd = &zIn[nLen];` |
|     15 |  665 | `	zCur = 0; /* cc warning */` |
|     20 |  666 | `	for(;;){` |
|     41 |  667 | `		if( zIn >= zEnd ){` |
|      - |  668 | `			/* No more input */` |
|     15 |  669 | `			break;` |
|      - |  670 | `		}` |
|     27 |  671 | `		zCur = zIn;` |
|      - |  672 | `		/* scan until a character that needs escaping (', ", \\, or NUL) */` |
|     89 |  673 | `		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' && zIn[0] != '\0' ){` |
|     63 |  674 | `			zIn++;` |
|      1 |  675 | `		}` |
|     27 |  676 | `		if( zIn > zCur ){` |
|      - |  677 | `			/* Append raw contents */` |
|     23 |  678 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 |  679 | `		}` |
|     27 |  680 | `		if( zIn < zEnd ){` |
|     17 |  681 | `			int c = zIn[0];` |
|     17 |  682 | `			if( c == '\0' ){` |
|      - |  683 | `				/* PHP escapes NUL as "\\0" (two characters) */` |
|      3 |  684 | `				ph7_result_string(pCtx,"\\0",2);` |
|      2 |  685 | `			}else{` |
|     15 |  686 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - |  687 | `			}` |
|      8 |  688 | `		}` |
|     27 |  689 | `		zIn++;` |
|      1 |  690 | `	}` |
|     15 |  691 | `	return PH7_OK;` |
|     14 |  692 |  |
|      - |  693 | `/*` |
|      - |  694 | ` * Check if the given character is present in the given mask.` |
|      - |  695 | ` * Return TRUE if present. FALSE otherwise.` |
|      - |  696 | ` */` |
|    124 |  697 | `static int cSlashCheckMask(int c,const char *zMask,int nLen)` |
|      1 |  698 |  |
|    125 |  699 | `	const char *zEnd = &zMask[nLen];` |
|    555 |  700 | `	while( zMask < zEnd ){` |
|      - |  701 | `		/* Support range syntax A..Z where A and Z are literal bytes.  The` |
|      - |  702 | `		 * original PH7 implementation ignored ranges; tests rely on them so` |
|      - |  703 | `		 * provide a simple on-the-fly check here. */` |
|    475 |  704 | `		if( zMask + 3 < zEnd && zMask[1] == '.' && zMask[2] == '.' ){` |
|      3 |  705 | `			int lo = (unsigned char)zMask[0];` |
|      3 |  706 | `			int hi = (unsigned char)zMask[3];` |
|      3 |  707 | `			if( lo > hi ){` |
|    ! 0 |  708 | `				int tmp = lo; lo = hi; hi = tmp;` |
|    ! 0 |  709 | `			}` |
|      3 |  710 | `			if( c >= lo && c <= hi ){` |
|      3 |  711 | `				return 1;` |
|      - |  712 | `			}` |
|      - |  713 | `			/* consume the range specifier */` |
|    ! 0 |  714 | `			zMask += 4;` |
|    ! 0 |  715 | `			continue;` |
|      - |  716 | `		}` |
|    473 |  717 | `		if( zMask[0] == c ){` |
|      - |  718 | `			/* Character present,return TRUE */` |
|     43 |  719 | `			return 1;` |
|      - |  720 | `		}` |
|      - |  721 | `		/* Advance the pointer */` |
|    431 |  722 | `		zMask++;` |
|      1 |  723 | `	}` |
|      - |  724 | `	/* Not present */` |
|     81 |  725 | `	return 0;` |
|     63 |  726 |  |
|      - |  727 | `/*` |
|      - |  728 | ` * string addcslashes(string $str,string $charlist)` |
|      - |  729 | ` *  Quote string with slashes in a C style.` |
|      - |  730 | ` * Parameter` |
|      - |  731 | ` *  $str:` |
|      - |  732 | ` *    The string to be escaped.` |
|      - |  733 | ` *  $charlist:` |
|      - |  734 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|      - |  735 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|      - |  736 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|      - |  737 | ` * Return` |
|      - |  738 | ` *  Returns the escaped string.` |
|      - |  739 | ` * Note:` |
|      - |  740 | ` *  Range characters [i.e: 'A..Z'] is not implemented in the current release.` |
|      - |  741 | ` */` |
|     34 |  742 | `static int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  743 |  |
|      - |  744 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - |  745 | `	int nLen,nMask;` |
|      - |  746 | `	/* PHP enforces exactly two arguments. */` |
|     36 |  747 | `	if( nArg != 2 ){` |
|      7 |  748 | `		return PH7_VmThrowException(pCtx,` |
|      - |  749 | `			"ArgumentCountError",` |
|      - |  750 | `			"addcslashes() expects exactly 2 arguments, %d given",` |
|      2 |  751 | `			nArg` |
|      - |  752 | `			);` |
|      - |  753 | `	}` |
|      - |  754 | `	/* First argument must be a string-ish value.  NULL is deprecated and` |
|      - |  755 | `	 * treated as the empty string (PHP 8.1). */` |
|     32 |  756 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      - |  757 | `		/* Emit deprecation only once, similar to PHP behaviour. */` |
|      3 |  758 | `		PH7_VmThrowError(pCtx->pVm,0,/* iErr will be patched to 8192 below */` |
|      - |  759 | `			E_DEPRECATED,` |
|      - |  760 | `			"addcslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - |  761 | `			);` |
|      - |  762 | `		/* treat as empty string; fall through to conversion logic */` |
|     56 |  763 | `	} else if( ph7_value_is_array(apArg[0]) \|\|` |
|     41 |  764 | `	          ph7_value_is_object(apArg[0]) \|\|` |
|     26 |  765 | `	          ph7_value_is_resource(apArg[0]) ){` |
|      4 |  766 | `		return PH7_VmThrowException(pCtx,` |
|      - |  767 | `			"TypeError",` |
|      - |  768 | `			"addcslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 |  769 | `			ph7_type_name(apArg[0])` |
|      - |  770 | `			);` |
|      - |  771 | `	}` |
|      - |  772 | `	/* Second argument must be a string.  NULL is deprecated and treated as` |
|      - |  773 | `	 * an empty mask per PHP semantics.  Arrays/objects/resources still` |
|      - |  774 | `	 * trigger a TypeError. */` |
|     30 |  775 | `	if( ph7_value_is_null(apArg[1]) ){` |
|      3 |  776 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - |  777 | `			E_DEPRECATED,` |
|      - |  778 | `			"addcslashes(): Passing null to parameter #2 ($characters) of type string is deprecated"` |
|      - |  779 | `			);` |
|      - |  780 | `		/* allow through so it becomes empty string below */` |
|     52 |  781 | `	} else if( ph7_value_is_array(apArg[1]) \|\|` |
|     38 |  782 | `	          ph7_value_is_object(apArg[1]) \|\|` |
|     24 |  783 | `	          ph7_value_is_resource(apArg[1]) ){` |
|      4 |  784 | `		return PH7_VmThrowException(pCtx,` |
|      - |  785 | `			"TypeError",` |
|      - |  786 | `			"addcslashes(): Argument #2 ($characters) must be of type string, %s given",` |
|      2 |  787 | `			ph7_type_name(apArg[1])` |
|      - |  788 | `			);` |
|      - |  789 | `	}` |
|      - |  790 | `	/* Extract the string to process */` |
|     27 |  791 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      - |  792 | `	/* NULL would never reach here due to the check above. */` |
|     27 |  793 | `	if( nLen < 1 ){` |
|      - |  794 | `		/* Empty string returns itself. */` |
|      5 |  795 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      5 |  796 | `		return PH7_OK;` |
|      - |  797 | `	}` |
|      - |  798 | `	/* Extract the desired mask */` |
|     23 |  799 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|     23 |  800 | `	zEnd = &zIn[nLen];` |
|     23 |  801 | `	zCur = 0; /* cc warning */` |
|     29 |  802 | `	for(;;){` |
|     59 |  803 | `		if( zIn >= zEnd ){` |
|      - |  804 | `			/* No more input */` |
|     23 |  805 | `			break;` |
|      - |  806 | `		}` |
|     37 |  807 | `		zCur = zIn;` |
|     91 |  808 | `		while( zIn < zEnd && !cSlashCheckMask(zIn[0],zMask,nMask) ){` |
|     55 |  809 | `			zIn++;` |
|      1 |  810 | `		}` |
|     37 |  811 | `		if( zIn > zCur ){` |
|      - |  812 | `			/* Append raw contents */` |
|     33 |  813 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 |  814 | `		}` |
|     37 |  815 | `		if( zIn < zEnd ){` |
|      - |  816 | `			/* Make sure we treat the byte as unsigned to avoid negative values` |
|      - |  817 | `			 * on platforms where char is signed. */` |
|     19 |  818 | `			int c = (unsigned char)zIn[0];` |
|      - |  819 | `			/* Handle special C-like escapes for common control characters first.` |
|      - |  820 | `			 * PHP outputs "\n" "\r" "\t" "\v" "\f" when those chars are` |
|      - |  821 | `			 * in the mask. NUL is left to the octal conversion below. */` |
|     19 |  822 | `			if( c == '\n' ){` |
|      3 |  823 | `				ph7_result_string(pCtx,"\\n",2);` |
|     18 |  824 | `			}else if( c == '\r' ){` |
|      3 |  825 | `				ph7_result_string(pCtx,"\\r",2);` |
|     16 |  826 | `			}else if( c == '\t' ){` |
|      3 |  827 | `				ph7_result_string(pCtx,"\\t",2);` |
|     14 |  828 | `			}else if( c == '\v' ){` |
|      3 |  829 | `				ph7_result_string(pCtx,"\\v",2);` |
|     12 |  830 | `			}else if( c == '\f' ){` |
|      3 |  831 | `				ph7_result_string(pCtx,"\\f",2);` |
|     10 |  832 | `			}else if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|      - |  833 | `				/* Convert to octal.  PHP always emits three-digit zero-padded` |
|      - |  834 | `				 * octal escapes (\001 not \1). */` |
|      7 |  835 | `				ph7_result_string_format(pCtx,"\\%03o",c);` |
|      4 |  836 | `			}else{` |
|      3 |  837 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - |  838 | `			}` |
|      9 |  839 | `		}` |
|     37 |  840 | `		zIn++;` |
|      1 |  841 | `	}` |
|     23 |  842 | `	return PH7_OK;` |
|     19 |  843 |  |
|      - |  844 | `/*` |
|      - |  845 | ` * string quotemeta(string $str)` |
|      - |  846 | ` *  Quote meta characters.` |
|      - |  847 | ` * Parameter` |
|      - |  848 | ` *  $str:` |
|      - |  849 | ` *    The string to be escaped.` |
|      - |  850 | ` * Return` |
|      - |  851 | ` *  Returns the escaped string.` |
|      - |  852 | `*/` |
|     10 |  853 | `static int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  854 |  |
|      - |  855 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  856 | `	int nLen;` |
|     11 |  857 | `	if( nArg < 1 ){` |
|      - |  858 | `		/* Nothing to process,retun NULL */` |
|      3 |  859 | `		ph7_result_null(pCtx);` |
|      3 |  860 | `		return PH7_OK;` |
|      - |  861 | `	}` |
|      - |  862 | `	/* Extract the string to process */` |
|      9 |  863 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      9 |  864 | `	if( nLen < 1 ){` |
|      - |  865 | `		/* Return the empty string */` |
|      3 |  866 | `		ph7_result_string(pCtx,"",0);` |
|      3 |  867 | `		return PH7_OK;` |
|      - |  868 | `	}` |
|      7 |  869 | `	zEnd = &zIn[nLen];` |
|      7 |  870 | `	zCur = 0; /* cc warning */` |
|     17 |  871 | `	for(;;){` |
|     35 |  872 | `		if( zIn >= zEnd ){` |
|      - |  873 | `			/* No more input */` |
|      7 |  874 | `			break;` |
|      - |  875 | `		}` |
|     29 |  876 | `		zCur = zIn;` |
|     55 |  877 | `		while( zIn < zEnd && !cSlashCheckMask(zIn[0],".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1) ){` |
|     27 |  878 | `			zIn++;` |
|      1 |  879 | `		}` |
|     29 |  880 | `		if( zIn > zCur ){` |
|      - |  881 | `			/* Append raw contents */` |
|     11 |  882 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      5 |  883 | `		}` |
|     29 |  884 | `		if( zIn < zEnd ){` |
|     27 |  885 | `			int c = zIn[0];` |
|     27 |  886 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|     13 |  887 | `		}` |
|     29 |  888 | `		zIn++;` |
|      1 |  889 | `	}` |
|      7 |  890 | `	return PH7_OK;` |
|      6 |  891 |  |
|      - |  892 | `/*` |
|      - |  893 | ` * string stripslashes(string $str)` |
|      - |  894 | ` *  Un-quotes a quoted string.` |
|      - |  895 | ` *  Returns a string with backslashes before characters that need` |
|      - |  896 | ` *  to be quoted in database queries etc. These characters are single` |
|      - |  897 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - |  898 | ` * Parameter` |
|      - |  899 | ` *  $str` |
|      - |  900 | ` *   The input string.` |
|      - |  901 | ` * Return` |
|      - |  902 | ` *  Returns a string with backslashes stripped off.` |
|      - |  903 | ` */` |
|      8 |  904 | `static int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  905 |  |
|      - |  906 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  907 | `	int nLen;` |
|      9 |  908 | `	if( nArg < 1 ){` |
|      - |  909 | `		/* Nothing to process,retun NULL */` |
|      3 |  910 | `		ph7_result_null(pCtx);` |
|      3 |  911 | `		return PH7_OK;` |
|      - |  912 | `	}` |
|      - |  913 | `	/* Extract the string to process */` |
|      7 |  914 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 |  915 | `	if( zIn == 0 ){` |
|    ! 0 |  916 | `		ph7_result_null(pCtx);` |
|    ! 0 |  917 | `		return PH7_OK;` |
|      - |  918 | `	}` |
|      7 |  919 | `	zEnd = &zIn[nLen];` |
|      7 |  920 | `	zCur = 0; /* cc warning */` |
|      - |  921 | `	/* Encode the string */` |
|      4 |  922 | `	for(;;){` |
|      9 |  923 | `		if( zIn >= zEnd ){` |
|      - |  924 | `			/* No more input */` |
|      5 |  925 | `			break;` |
|      - |  926 | `		}` |
|      5 |  927 | `		zCur = zIn;` |
|     17 |  928 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 |  929 | `			zIn++;` |
|      1 |  930 | `		}` |
|      5 |  931 | `		if( zIn > zCur ){` |
|      - |  932 | `			/* Append raw contents */` |
|      5 |  933 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 |  934 | `		}` |
|      5 |  935 | `		if( &zIn[1] < zEnd ){` |
|      3 |  936 | `			int c = zIn[1];` |
|      3 |  937 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - |  938 | `				/* Ignore the backslash */` |
|      3 |  939 | `				zIn++;` |
|      1 |  940 | `			}` |
|      2 |  941 | `		}else{` |
|      3 |  942 | `			break;` |
|      - |  943 | `		}` |
|      1 |  944 | `	}` |
|      7 |  945 | `	return PH7_OK;` |
|      5 |  946 |  |
|      - |  947 | `/*` |
|      - |  948 | ` * string htmlspecialchars(string $string [, int $flags = ENT_COMPAT \| ENT_HTML401 [, string $charset]])` |
|      - |  949 | ` *  HTML escaping of special characters.` |
|      - |  950 | ` *  The translations performed are:` |
|      - |  951 | ` *   '&' (ampersand) ==> '&amp;'` |
|      - |  952 | ` *   '"' (double quote) ==> '&quot;' when ENT_NOQUOTES is not set.` |
|      - |  953 | ` *   "'" (single quote) ==> '&#039;' only when ENT_QUOTES is set.` |
|      - |  954 | ` *   '<' (less than) ==> '&lt;'` |
|      - |  955 | ` *   '>' (greater than) ==> '&gt;'` |
|      - |  956 | ` * Parameters` |
|      - |  957 | ` *  $string` |
|      - |  958 | ` *   The string being converted.` |
|      - |  959 | ` * $flags` |
|      - |  960 | ` *   A bitmask of one or more of the following flags, which specify how to handle quotes.` |
|      - |  961 | ` *   The default is ENT_COMPAT \| ENT_HTML401.` |
|      - |  962 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone.` |
|      - |  963 | ` *   ENT_QUOTES 	Will convert both double and single quotes.` |
|      - |  964 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted.` |
|      - |  965 | ` *   ENT_IGNORE 	Silently discard invalid code unit sequences instead of returning an empty string.` |
|      - |  966 | ` * $charset` |
|      - |  967 | ` *  Defines character set used in conversion. The default character set is ISO-8859-1. (Not used)` |
|      - |  968 | ` * Return` |
|      - |  969 | ` *  The escaped string or NULL on failure.` |
|      - |  970 | ` */` |
|     20 |  971 | `static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  972 |  |
|      - |  973 | `	const char *zCur,*zIn,*zEnd;` |
|     21 |  974 | `	int iFlags = 0x01\|0x40; /* ENT_COMPAT \| ENT_HTML401 */` |
|      - |  975 | `	int nLen,c;` |
|     21 |  976 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  977 | `		/* Missing/Invalid arguments,return NULL */` |
|      9 |  978 | `		ph7_result_null(pCtx);` |
|      9 |  979 | `		return PH7_OK;` |
|      - |  980 | `	}` |
|      - |  981 | `	/* Extract the target string */` |
|     13 |  982 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - |  983 | `	/* Return early when the input is empty, mirroring PHP's behavior. */` |
|     13 |  984 | `	if( nLen == 0 ){` |
|      3 |  985 | `		ph7_result_string(pCtx,"",0);` |
|      3 |  986 | `		return PH7_OK;` |
|      - |  987 | `	}` |
|     11 |  988 | `	zEnd = &zIn[nLen];` |
|      - |  989 | `	/* Extract the flags if available */` |
|     11 |  990 | `	if( nArg > 1 ){` |
|      9 |  991 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 |  992 | `		if( iFlags < 0 ){` |
|      3 |  993 | `			iFlags = 0x01\|0x40;` |
|      1 |  994 | `		}` |
|      4 |  995 | `	}` |
|      - |  996 | `	/* Perform the requested operation */` |
|     23 |  997 | `	for(;;){` |
|     47 |  998 | `		if( zIn >= zEnd ){` |
|      9 |  999 | `			break;` |
|      - | 1000 | `		}` |
|     39 | 1001 | `		zCur = zIn;` |
|     83 | 1002 | `		while( zIn < zEnd && zIn[0] != '&' && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '<' && zIn[0] != '>' ){` |
|     45 | 1003 | `			zIn++;` |
|      1 | 1004 | `		}` |
|     39 | 1005 | `		if( zCur < zIn ){` |
|      - | 1006 | `			/* Append the raw string verbatim */` |
|     17 | 1007 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      8 | 1008 | `		}` |
|     39 | 1009 | `		if( zIn >= zEnd ){` |
|      3 | 1010 | `			break;` |
|      - | 1011 | `		}` |
|     37 | 1012 | `		c = zIn[0];` |
|     37 | 1013 | `		if( c == '&' ){` |
|      - | 1014 | `			/* Expand '&amp;' */` |
|      9 | 1015 | `			ph7_result_string(pCtx,"&amp;",(int)sizeof("&amp;")-1);` |
|     33 | 1016 | `		}else if( c == '<' ){` |
|      - | 1017 | `			/* Expand '&lt;' */` |
|      7 | 1018 | `			ph7_result_string(pCtx,"&lt;",(int)sizeof("&lt;")-1);` |
|     26 | 1019 | `		}else if( c == '>' ){` |
|      - | 1020 | `			/* Expand '&gt;' */` |
|      9 | 1021 | `			ph7_result_string(pCtx,"&gt;",(int)sizeof("&gt;")-1);` |
|     19 | 1022 | `		}else if( c == '\'' ){` |
|      5 | 1023 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 1024 | `				/* Expand '&#039;' */` |
|      5 | 1025 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      3 | 1026 | `			}else{` |
|      - | 1027 | `				/* Leave the single quote untouched */` |
|    ! 0 | 1028 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      1 | 1029 | `			}` |
|     13 | 1030 | `		}else if( c == '"' ){` |
|     11 | 1031 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      - | 1032 | `				/* Expand '&quot;' */` |
|      7 | 1033 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      4 | 1034 | `			}else{` |
|      - | 1035 | `				/* Leave the double quote untouched */` |
|      5 | 1036 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      - | 1037 | `			}` |
|      5 | 1038 | `		}` |
|      - | 1039 | `		/* Ignore the unsafe HTML character */` |
|     37 | 1040 | `		zIn++;` |
|      1 | 1041 | `	}` |
|     11 | 1042 | `	return PH7_OK;` |
|     11 | 1043 |  |
|      - | 1044 | `/*` |
|      - | 1045 | ` * string htmlspecialchars_decode(string $string[,int $quote_style = ENT_COMPAT ])` |
|      - | 1046 | ` *  Unescape HTML entities.` |
|      - | 1047 | ` * Parameters` |
|      - | 1048 | ` *  $string` |
|      - | 1049 | ` *   The string to decode` |
|      - | 1050 | ` *  $quote_style` |
|      - | 1051 | ` *    The quote style. One of the following constants:` |
|      - | 1052 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone (default)` |
|      - | 1053 | ` *   ENT_QUOTES 	Will convert both double and single quotes` |
|      - | 1054 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted` |
|      - | 1055 | ` * Return` |
|      - | 1056 | ` *  The unescaped string or NULL on failure.` |
|      - | 1057 | ` */` |
|     16 | 1058 | `static int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1059 |  |
|      - | 1060 | `	const char *zCur,*zIn,*zEnd;` |
|     17 | 1061 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 1062 | `	int nLen,nJump;` |
|     17 | 1063 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1064 | `		/* Missing/Invalid arguments,return NULL */` |
|      7 | 1065 | `		ph7_result_null(pCtx);` |
|      7 | 1066 | `		return PH7_OK;` |
|      - | 1067 | `	}` |
|      - | 1068 | `	/* Extract the target string */` |
|     11 | 1069 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 1070 | `	zEnd = &zIn[nLen];` |
|      - | 1071 | `	/* Extract the flags if available */` |
|     11 | 1072 | `	if( nArg > 1 ){` |
|      7 | 1073 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      7 | 1074 | `		if( iFlags < 0 ){` |
|      3 | 1075 | `			iFlags = 0x01;` |
|      1 | 1076 | `		}` |
|      3 | 1077 | `	}` |
|      - | 1078 | `	/* Perform the requested operation */` |
|     15 | 1079 | `	for(;;){` |
|     31 | 1080 | `		if( zIn >= zEnd ){` |
|     11 | 1081 | `			break;` |
|      - | 1082 | `		}` |
|     21 | 1083 | `		zCur = zIn;` |
|     51 | 1084 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|     31 | 1085 | `			zIn++;` |
|      1 | 1086 | `		}` |
|     21 | 1087 | `		if( zCur < zIn ){` |
|      - | 1088 | `			/* Append the raw string verbatim */` |
|      9 | 1089 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 1090 | `		}` |
|     21 | 1091 | `		nLen = (int)(zEnd-zIn);` |
|     21 | 1092 | `		nJump = (int)sizeof(char);` |
|     21 | 1093 | `		if( nLen >= (int)sizeof("&amp;")-1 && SyStrnicmp(zIn,"&amp;",sizeof("&amp;")-1) == 0 ){` |
|      - | 1094 | `			/* &amp; ==> '&' */` |
|      3 | 1095 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|      3 | 1096 | `			nJump = (int)sizeof("&amp;")-1;` |
|     20 | 1097 | `		}else if( nLen >= (int)sizeof("&lt;")-1 && SyStrnicmp(zIn,"&lt;",sizeof("&lt;")-1) == 0 ){` |
|      - | 1098 | `			/* &lt; ==> < */` |
|      3 | 1099 | `			ph7_result_string(pCtx,"<",(int)sizeof(char));` |
|      3 | 1100 | `			nJump = (int)sizeof("&lt;")-1;` |
|     18 | 1101 | `		}else if( nLen >= (int)sizeof("&gt;")-1 && SyStrnicmp(zIn,"&gt;",sizeof("&gt;")-1) == 0 ){` |
|      - | 1102 | `			/* &gt; ==> '>' */` |
|      3 | 1103 | `			ph7_result_string(pCtx,">",(int)sizeof(char));` |
|      3 | 1104 | `			nJump = (int)sizeof("&gt;")-1;` |
|     16 | 1105 | `		}else if( nLen >= (int)sizeof("&quot;")-1 && SyStrnicmp(zIn,"&quot;",sizeof("&quot;")-1) == 0 ){` |
|      - | 1106 | `			/* &quot; ==> '"' */` |
|     13 | 1107 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      9 | 1108 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      5 | 1109 | `			}else{` |
|      - | 1110 | `				/* Leave untouched */` |
|      5 | 1111 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      - | 1112 | `			}` |
|     13 | 1113 | `			nJump = (int)sizeof("&quot;")-1;` |
|      9 | 1114 | `		}else if( nLen >= (int)sizeof("&#039;")-1 && SyStrnicmp(zIn,"&#039;",sizeof("&#039;")-1) == 0 ){` |
|      - | 1115 | `			/* &#039; ==> ''' */` |
|      3 | 1116 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 1117 | `				/* Expand ''' */` |
|      3 | 1118 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      2 | 1119 | `			}else{` |
|      - | 1120 | `				/* Leave untouched */` |
|    ! 0 | 1121 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      - | 1122 | `			}` |
|      3 | 1123 | `			nJump = (int)sizeof("&#039;")-1;` |
|      1 | 1124 | `		}else if( nLen >= (int)sizeof(char) ){` |
|      - | 1125 | `			/* expand '&' */` |
|    ! 0 | 1126 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 1127 | `		}else{` |
|      - | 1128 | `			/* No more input to process */` |
|    ! 0 | 1129 | `			break;` |
|      - | 1130 | `		}` |
|     21 | 1131 | `		zIn += nJump;` |
|      1 | 1132 | `	}` |
|     11 | 1133 | `	return PH7_OK;` |
|      9 | 1134 |  |
|      - | 1135 | `/* HTML encoding/Decoding table` |
|      - | 1136 | ` * Source: Symisc RunTime API.[chm@symisc.net]` |
|      - | 1137 | ` */` |
|      - | 1138 | `static const char *azHtmlEscape[] = {` |
|      - | 1139 | ` 	"&lt;","<","&gt;",">","&amp;","&","&quot;","\"","&#39;","'",` |
|      - | 1140 | `	"&#33;","!","&#36;","$","&#35;","#","&#37;","%","&#40;","(",` |
|      - | 1141 | `	"&#41;",")","&#123;","{","&#125;","}","&#61;","=","&#43;","+",` |
|      - | 1142 | `	"&#63;","?","&#91;","[","&#93;","]","&#64;","@","&#44;",","` |
|      - | 1143 | ` };` |
|      - | 1144 | `/*` |
|      - | 1145 | ` * array get_html_translation_table(void)` |
|      - | 1146 | ` *  Returns the translation table used by htmlspecialchars() and htmlentities().` |
|      - | 1147 | ` * Parameters` |
|      - | 1148 | ` *  None` |
|      - | 1149 | ` * Return` |
|      - | 1150 | ` *  The translation table as an array or NULL on failure.` |
|      - | 1151 | ` */` |
|      4 | 1152 | `static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1153 |  |
|      - | 1154 | `	ph7_value *pArray,*pValue;` |
|      - | 1155 | `	sxu32 n;` |
|      - | 1156 | `	/* Element value */` |
|      5 | 1157 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      5 | 1158 | `	if( pValue == 0 ){` |
|    ! 0 | 1159 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 1160 | `		SXUNUSED(apArg);` |
|      - | 1161 | `		/* Return NULL */` |
|    ! 0 | 1162 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1163 | `		return PH7_OK;` |
|      - | 1164 | `	}` |
|      - | 1165 | `	/* Create a new array */` |
|      5 | 1166 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 | 1167 | `	if( pArray == 0 ){` |
|      - | 1168 | `		/* Return NULL */` |
|    ! 0 | 1169 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1170 | `		return PH7_OK;` |
|      - | 1171 | `	}` |
|      - | 1172 | `	/* Make the table */` |
|     85 | 1173 | `	for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|      - | 1174 | `		/* Prepare the value */` |
|     81 | 1175 | `		ph7_value_string(pValue,azHtmlEscape[n],-1 /* Compute length automatically */);` |
|      - | 1176 | `		/* Insert the value */` |
|     81 | 1177 | `		ph7_array_add_strkey_elem(pArray,azHtmlEscape[n+1],pValue);` |
|      - | 1178 | `		/* Reset the string cursor */` |
|     81 | 1179 | `		ph7_value_reset_string_cursor(pValue);` |
|     41 | 1180 | `	}` |
|      - | 1181 | `	/*` |
|      - | 1182 | `	 * Return the array.` |
|      - | 1183 | `	 * Don't worry about freeing memory, everything will be automatically` |
|      - | 1184 | `	 * released upon we return from this function.` |
|      - | 1185 | `	 */` |
|      5 | 1186 | `	ph7_result_value(pCtx,pArray);` |
|      5 | 1187 | `	return PH7_OK;` |
|      3 | 1188 |  |
|      - | 1189 | `/*` |
|      - | 1190 | ` * string htmlentities( string $string [, int $flags = ENT_COMPAT \| ENT_HTML401]);` |
|      - | 1191 | ` *   Convert all applicable characters to HTML entities` |
|      - | 1192 | ` * Parameters` |
|      - | 1193 | ` * $string` |
|      - | 1194 | ` *   The input string.` |
|      - | 1195 | ` * $flags` |
|      - | 1196 | ` *  A bitmask of one or more of the flags (see block-comment on PH7_builtin_htmlspecialchars())` |
|      - | 1197 | ` * Return` |
|      - | 1198 | ` * The encoded string.` |
|      - | 1199 | ` */` |
|     10 | 1200 | `static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1201 |  |
|     11 | 1202 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 1203 | `	const char *zIn,*zEnd;` |
|      - | 1204 | `	int nLen,c;` |
|      - | 1205 | `	sxu32 n;` |
|     11 | 1206 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1207 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 1208 | `		ph7_result_null(pCtx);` |
|      5 | 1209 | `		return PH7_OK;` |
|      - | 1210 | `	}` |
|      - | 1211 | `	/* Extract the target string */` |
|      7 | 1212 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1213 | `	/* Handle empty string up front */` |
|      7 | 1214 | `	if( nLen == 0 ){` |
|      3 | 1215 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1216 | `		return PH7_OK;` |
|      - | 1217 | `	}` |
|      5 | 1218 | `	zEnd = &zIn[nLen];` |
|      - | 1219 | `	/* Extract the flags if available */` |
|      5 | 1220 | `	if( nArg > 1 ){` |
|      3 | 1221 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      3 | 1222 | `		if( iFlags < 0 ){` |
|      3 | 1223 | `			iFlags = 0x01;` |
|      1 | 1224 | `		}` |
|      1 | 1225 | `	}` |
|      - | 1226 | `	/* Perform the requested operation */` |
|     11 | 1227 | `	for(;;){` |
|     23 | 1228 | `		if( zIn >= zEnd ){` |
|      - | 1229 | `			/* No more input to process */` |
|      5 | 1230 | `			break;` |
|      - | 1231 | `		}` |
|     19 | 1232 | `		c = zIn[0];` |
|      - | 1233 | `		/* Perform a linear lookup on the decoding table */` |
|    233 | 1234 | `		for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    223 | 1235 | `			if( azHtmlEscape[n+1][0] == c ){` |
|      - | 1236 | `				/* Got one */` |
|      9 | 1237 | `				break;` |
|      - | 1238 | `			}` |
|    108 | 1239 | `		}` |
|     19 | 1240 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|      - | 1241 | `			/* Output the safe sequence [i.e: '<' ==> '&lt;"] */` |
|      9 | 1242 | `			if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 1243 | `				/* Expand the double quote verbatim */` |
|    ! 0 | 1244 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      9 | 1245 | `			}else if(c == '\'' && ((iFlags & 0x02 /*ENT_QUOTES*/) == 0 \|\| (iFlags & 0x04) /*ENT_NOQUOTES*/) ){` |
|      - | 1246 | `				/* expand single quote verbatim */` |
|    ! 0 | 1247 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|    ! 0 | 1248 | `			}else{` |
|      9 | 1249 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1/*Compute length automatically */);` |
|      - | 1250 | `			}` |
|      5 | 1251 | `		}else{` |
|      - | 1252 | `			/* Output character verbatim */` |
|     11 | 1253 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 1254 | `		}` |
|     19 | 1255 | `		zIn++;` |
|      1 | 1256 | `	}` |
|      5 | 1257 | `	return PH7_OK;` |
|      6 | 1258 |  |
|      - | 1259 | `/*` |
|      - | 1260 | ` * string html_entity_decode(string $string [, int $quote_style = ENT_COMPAT [, string $charset = 'UTF-8' ]])` |
|      - | 1261 | ` *   Perform the reverse operation of html_entity_decode().` |
|      - | 1262 | ` * Parameters` |
|      - | 1263 | ` * $string` |
|      - | 1264 | ` *   The input string.` |
|      - | 1265 | ` * $flags` |
|      - | 1266 | ` *  A bitmask of one or more of the flags (see comment on PH7_builtin_htmlspecialchars())` |
|      - | 1267 | ` * Return` |
|      - | 1268 | ` * The decoded string.` |
|      - | 1269 | ` */` |
|     28 | 1270 | `static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1271 |  |
|      - | 1272 | `	const char *zCur,*zIn,*zEnd;` |
|     29 | 1273 | `	int iFlags = 0x01; /* ENT_COMPAT  */` |
|      - | 1274 | `	int nLen;` |
|      - | 1275 | `	sxu32 n;` |
|     29 | 1276 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1277 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 1278 | `		ph7_result_null(pCtx);` |
|      5 | 1279 | `		return PH7_OK;` |
|      - | 1280 | `	}` |
|      - | 1281 | `	/* Extract the target string */` |
|     25 | 1282 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 1283 | `	zEnd = &zIn[nLen];` |
|      - | 1284 | `	/* Extract the flags if available */` |
|     25 | 1285 | `	if( nArg > 1 ){` |
|     15 | 1286 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     15 | 1287 | `		if( iFlags < 0 ){` |
|      3 | 1288 | `			iFlags = 0x01;` |
|      1 | 1289 | `		}` |
|      7 | 1290 | `	}` |
|      - | 1291 | `	/* Perform the requested operation */` |
|     27 | 1292 | `	for(;;){` |
|     55 | 1293 | `		if( zIn >= zEnd ){` |
|      - | 1294 | `			/* No more input to process */` |
|     13 | 1295 | `			break;` |
|      - | 1296 | `		}` |
|     43 | 1297 | `		zCur = zIn;` |
|    173 | 1298 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|    131 | 1299 | `			zIn++;` |
|      1 | 1300 | `		}` |
|     43 | 1301 | `		if( zCur < zIn ){` |
|      - | 1302 | `			/* Append raw string verbatim */` |
|     27 | 1303 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     13 | 1304 | `		}` |
|     43 | 1305 | `		if( zIn >= zEnd ){` |
|     13 | 1306 | `			break;` |
|      - | 1307 | `		}` |
|     31 | 1308 | `		nLen = (int)(zEnd-zIn);` |
|      - | 1309 | `		/* Find an encoded sequence */` |
|    113 | 1310 | `		for(n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    113 | 1311 | `			int iLen = (int)SyStrlen(azHtmlEscape[n]);` |
|    113 | 1312 | `			if( nLen >= iLen && SyStrnicmp(zIn,azHtmlEscape[n],(sxu32)iLen) == 0 ){` |
|      - | 1313 | `				/* Got one */` |
|     31 | 1314 | `				zIn += iLen;` |
|     31 | 1315 | `				break;` |
|      - | 1316 | `			}` |
|     42 | 1317 | `		}` |
|     31 | 1318 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|     31 | 1319 | `			int c = azHtmlEscape[n+1][0];` |
|      - | 1320 | `			/* Output the decoded character */` |
|     31 | 1321 | `			if( c == '\'' && ((iFlags & 0x02) == 0 /*ENT_QUOTES*/\|\| (iFlags & 0x04) /*ENT_NOQUOTES*/)  ){` |
|      - | 1322 | `				/* Do not process single quotes */` |
|      9 | 1323 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|     27 | 1324 | `			}else if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 1325 | `				/* Do not process double quotes */` |
|      5 | 1326 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|      3 | 1327 | `			}else{` |
|     19 | 1328 | `				ph7_result_string(pCtx,azHtmlEscape[n+1],-1); /* Compute length automatically */` |
|      - | 1329 | `			}` |
|     16 | 1330 | `		}else{` |
|      - | 1331 | `			/* Append '&' */` |
|    ! 0 | 1332 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 1333 | `			zIn++;` |
|      - | 1334 | `		}` |
|      1 | 1335 | `	}` |
|     25 | 1336 | `	return PH7_OK;` |
|     15 | 1337 |  |
|      - | 1338 | `/*` |
|      - | 1339 | ` * int strlen($string)` |
|      - | 1340 | ` *  return the length of the given string.` |
|      - | 1341 | ` * Parameter` |
|      - | 1342 | ` *  string: The string being measured for length.` |
|      - | 1343 | ` * Return` |
|      - | 1344 | ` *  length of the given string.` |
|      - | 1345 | ` */` |
|   2464 | 1346 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1347 |  |
|   2466 | 1348 | `	int iLen = 0;` |
|   2466 | 1349 | `	if( nArg > 0 ){` |
|   2464 | 1350 | `		ph7_value_to_string(apArg[0],&iLen);` |
|   1231 | 1351 | `	}` |
|      - | 1352 | `	/* String length */` |
|   2466 | 1353 | `	ph7_result_int(pCtx,iLen);` |
|   2466 | 1354 | `	return PH7_OK;` |
|      2 | 1355 |  |
|      - | 1356 | `/*` |
|      - | 1357 | ` * int strcmp(string $str1,string $str2)` |
|      - | 1358 | ` *  Perform a binary safe string comparison.` |
|      - | 1359 | ` * Parameter` |
|      - | 1360 | ` *  str1: The first string` |
|      - | 1361 | ` *  str2: The second string` |
|      - | 1362 | ` * Return` |
|      - | 1363 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1364 | ` *  than str2, and 0 if they are equal.` |
|      - | 1365 | ` */` |
|     80 | 1366 | `static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1367 |  |
|      - | 1368 | `	const char *z1,*z2;` |
|      - | 1369 | `	int n1,n2;` |
|      - | 1370 | `	int res;` |
|     81 | 1371 | `	if( nArg < 2 ){` |
|      5 | 1372 | `		res = nArg == 0 ? 0 : 1;` |
|      5 | 1373 | `		ph7_result_int(pCtx,res);` |
|      5 | 1374 | `		return PH7_OK;` |
|      - | 1375 | `	}` |
|      - | 1376 | `	/* Perform the comparison */` |
|     77 | 1377 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     77 | 1378 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     77 | 1379 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1380 | `	/* Comparison result */` |
|     77 | 1381 | `	ph7_result_int(pCtx,res);` |
|     77 | 1382 | `	return PH7_OK;` |
|     41 | 1383 |  |
|      - | 1384 | `/*` |
|      - | 1385 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 1386 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 1387 | ` * Parameter` |
|      - | 1388 | ` *  str1: The first string` |
|      - | 1389 | ` *  str2: The second string` |
|      - | 1390 | ` * Return` |
|      - | 1391 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1392 | ` *  than str2, and 0 if they are equal.` |
|      - | 1393 | ` */` |
|     20 | 1394 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1395 |  |
|      - | 1396 | `	const char *z1,*z2;` |
|      - | 1397 | `	int res;` |
|      - | 1398 | `	int n;` |
|     21 | 1399 | `	if( nArg < 3 ){` |
|      - | 1400 | `		/* Perform a standard comparison */` |
|      5 | 1401 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 1402 | `	}` |
|      - | 1403 | `	/* Desired comparison length */` |
|     17 | 1404 | `	n  = ph7_value_to_int(apArg[2]);` |
|     17 | 1405 | `	if( n < 0 ){` |
|      - | 1406 | `		/* Invalid length */` |
|      3 | 1407 | `		ph7_result_int(pCtx,-1);` |
|      3 | 1408 | `		return PH7_OK;` |
|      - | 1409 | `	}` |
|      - | 1410 | `	/* Perform the comparison */` |
|     15 | 1411 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     15 | 1412 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     15 | 1413 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 1414 | `	/* Comparison result */` |
|     15 | 1415 | `	ph7_result_int(pCtx,res);` |
|     15 | 1416 | `	return PH7_OK;` |
|     11 | 1417 |  |
|      - | 1418 | `/*` |
|      - | 1419 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 1420 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 1421 | ` * Parameter` |
|      - | 1422 | ` *  str1: The first string` |
|      - | 1423 | ` *  str2: The second string` |
|      - | 1424 | ` * Return` |
|      - | 1425 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1426 | ` *  than str2, and 0 if they are equal.` |
|      - | 1427 | ` */` |
|     22 | 1428 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1429 |  |
|      - | 1430 | `	const char *z1,*z2;` |
|      - | 1431 | `	int n1,n2;` |
|      - | 1432 | `	int res;` |
|     23 | 1433 | `	if( nArg < 2 ){` |
|      9 | 1434 | `		res = nArg == 0 ? 0 : 1;` |
|      9 | 1435 | `		ph7_result_int(pCtx,res);` |
|      9 | 1436 | `		return PH7_OK;` |
|      - | 1437 | `	}` |
|      - | 1438 | `	/* Perform the comparison */` |
|     15 | 1439 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     15 | 1440 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     15 | 1441 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1442 | `	/* Comparison result */` |
|     15 | 1443 | `	ph7_result_int(pCtx,res);` |
|     15 | 1444 | `	return PH7_OK;` |
|     12 | 1445 |  |
|      - | 1446 | `/*` |
|      - | 1447 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 1448 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 1449 | ` * Parameter` |
|      - | 1450 | ` *  $str1: The first string` |
|      - | 1451 | ` *  $str2: The second string` |
|      - | 1452 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 1453 | ` * Return` |
|      - | 1454 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1455 | ` *  than str2, and 0 if they are equal.` |
|      - | 1456 | ` */` |
|      8 | 1457 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1458 |  |
|      - | 1459 | `	const char *z1,*z2;` |
|      - | 1460 | `	int res;` |
|      - | 1461 | `	int n;` |
|      9 | 1462 | `	if( nArg < 3 ){` |
|      - | 1463 | `		/* Perform a standard comparison */` |
|      5 | 1464 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 1465 | `	}` |
|      - | 1466 | `	/* Desired comparison length */` |
|      5 | 1467 | `	n  = ph7_value_to_int(apArg[2]);` |
|      5 | 1468 | `	if( n < 0 ){` |
|      - | 1469 | `		/* Invalid length */` |
|    ! 0 | 1470 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 1471 | `		return PH7_OK;` |
|      - | 1472 | `	}` |
|      - | 1473 | `	/* Perform the comparison */` |
|      5 | 1474 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|      5 | 1475 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|      5 | 1476 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 1477 | `	/* Comparison result */` |
|      5 | 1478 | `	ph7_result_int(pCtx,res);` |
|      5 | 1479 | `	return PH7_OK;` |
|      5 | 1480 |  |
|      - | 1481 | `/*` |
|      - | 1482 | ` * Implode context [i.e: it's private data].` |
|      - | 1483 | ` * A pointer to the following structure is forwarded` |
|      - | 1484 | ` * verbatim to the array walker callback defined below.` |
|      - | 1485 | ` */` |
|      - | 1486 | `struct implode_data {` |
|      - | 1487 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 1488 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 1489 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 1490 | `	int nSeplen;          /* Separator length */` |
|      - | 1491 | `	int bFirst;           /* TRUE if first call */` |
|      - | 1492 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 1493 | `};` |
|      - | 1494 | `/*` |
|      - | 1495 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 1496 | ` * The following routine is invoked for each array entry passed` |
|      - | 1497 | ` * to the implode() function.` |
|      - | 1498 | ` */` |
|  90402 | 1499 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      2 | 1500 |  |
|  45201 | 1501 | `	SXUNUSED(pKey);` |
|  90404 | 1502 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 1503 | `	const char *zData;` |
|      - | 1504 | `	int nLen;` |
|  90404 | 1505 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 1506 | `		if( pData->nSeplen > 0 ){` |
|      3 | 1507 | `			if( !pData->bFirst ){` |
|      - | 1508 | `				/* append the separator first */` |
|      3 | 1509 | `				ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|      2 | 1510 | `			}else{` |
|    ! 0 | 1511 | `				pData->bFirst = 0;` |
|      - | 1512 | `			}` |
|      1 | 1513 | `		}` |
|      - | 1514 | `		/* Recurse */` |
|      3 | 1515 | `		pData->bFirst = 1;` |
|      3 | 1516 | `		pData->nRecCount++;` |
|      3 | 1517 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 1518 | `		pData->nRecCount--;` |
|      3 | 1519 | `		return PH7_OK;` |
|      - | 1520 | `	}` |
|      - | 1521 | `	/* Extract the string representation of the entry value */` |
|  90402 | 1522 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 1523 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
|  90402 | 1524 | `	if( pData->bFirst ){` |
|  22196 | 1525 | `		pData->bFirst = 0;` |
|  79305 | 1526 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 1527 | `		/* append the separator first */` |
|  68196 | 1528 | `		ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|  34097 | 1529 | `	}` |
|      - | 1530 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
|  90402 | 1531 | `	if( nLen > 0 ){` |
|  82128 | 1532 | `		ph7_result_string(pData->pCtx,zData,nLen);` |
|  41063 | 1533 | `	}` |
|  90402 | 1534 | `	return PH7_OK;` |
|  45203 | 1535 |  |
|      - | 1536 | `/*` |
|      - | 1537 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 1538 | ` * string implode(array $pieces,...)` |
|      - | 1539 | ` *  Join array elements with a string.` |
|      - | 1540 | ` * $glue` |
|      - | 1541 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 1542 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 1543 | ` * $pieces` |
|      - | 1544 | ` *   The array of strings to implode.` |
|      - | 1545 | ` * Return` |
|      - | 1546 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 1547 | ` *  order, with the glue string between each element.` |
|      - | 1548 | ` */` |
|  22222 | 1549 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1550 |  |
|      - | 1551 | `	struct implode_data imp_data;` |
|  22224 | 1552 | `	int i = 1;` |
|  22224 | 1553 | `	if( nArg < 1 ){` |
|      - | 1554 | `		/* Missing argument,return NULL */` |
|    ! 0 | 1555 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1556 | `		return PH7_OK;` |
|      - | 1557 | `	}` |
|      - | 1558 | `	/* Prepare the implode context */` |
|  22224 | 1559 | `	imp_data.pCtx = pCtx;` |
|  22224 | 1560 | `	imp_data.bRecursive = 0;` |
|  22224 | 1561 | `	imp_data.bFirst = 1;` |
|  22224 | 1562 | `	imp_data.nRecCount = 0;` |
|  22224 | 1563 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  22222 | 1564 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  11112 | 1565 | `	}else{` |
|      3 | 1566 | `		imp_data.zSep = 0;` |
|      3 | 1567 | `		imp_data.nSeplen = 0;` |
|      3 | 1568 | `		i = 0;` |
|      - | 1569 | `	}` |
|  22224 | 1570 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 1571 | `	/* Start the 'join' process */` |
|  44446 | 1572 | `	while( i < nArg ){` |
|  22224 | 1573 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1574 | `			/* Iterate throw array entries */` |
|  22224 | 1575 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|  11113 | 1576 | `		}else{` |
|      - | 1577 | `			const char *zData;` |
|      - | 1578 | `			int nLen;` |
|      - | 1579 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 1580 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1581 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 1582 | `			if( imp_data.bFirst ){` |
|    ! 0 | 1583 | `				imp_data.bFirst = 0;` |
|    ! 0 | 1584 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1585 | `				ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen);` |
|    ! 0 | 1586 | `			}` |
|      - | 1587 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 1588 | `			if( nLen > 0 ){` |
|    ! 0 | 1589 | `				ph7_result_string(pCtx,zData,nLen);` |
|    ! 0 | 1590 | `			}` |
|      - | 1591 | `		}` |
|  22224 | 1592 | `		i++;` |
|      2 | 1593 | `	}` |
|  22224 | 1594 | `	return PH7_OK;` |
|  11113 | 1595 |  |
|      - | 1596 | `/*` |
|      - | 1597 | ` * Symisc eXtension:` |
|      - | 1598 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 1599 | ` * Purpose` |
|      - | 1600 | ` *  Same as implode() but recurse on arrays.` |
|      - | 1601 | ` * Example:` |
|      - | 1602 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 1603 | ` *   echo implode_recursive("/",$a);` |
|      - | 1604 | ` *   Will output` |
|      - | 1605 | ` *     usr/home/dean.` |
|      - | 1606 | ` *   While the standard implode would produce.` |
|      - | 1607 | ` *    usr/Array.` |
|      - | 1608 | ` * Parameter` |
|      - | 1609 | ` *  Refer to implode().` |
|      - | 1610 | ` * Return` |
|      - | 1611 | ` *  Refer to implode().` |
|      - | 1612 | ` */` |
|     12 | 1613 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1614 |  |
|      - | 1615 | `	struct implode_data imp_data;` |
|     13 | 1616 | `	int i = 1;` |
|     13 | 1617 | `	if( nArg < 1 ){` |
|      - | 1618 | `		/* Missing argument,return NULL */` |
|      3 | 1619 | `		ph7_result_null(pCtx);` |
|      3 | 1620 | `		return PH7_OK;` |
|      - | 1621 | `	}` |
|      - | 1622 | `	/* Prepare the implode context */` |
|     11 | 1623 | `	imp_data.pCtx = pCtx;` |
|     11 | 1624 | `	imp_data.bRecursive = 1;` |
|     11 | 1625 | `	imp_data.bFirst = 1;` |
|     11 | 1626 | `	imp_data.nRecCount = 0;` |
|     11 | 1627 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 1628 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 1629 | `	}else{` |
|    ! 0 | 1630 | `		imp_data.zSep = 0;` |
|    ! 0 | 1631 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 1632 | `		i = 0;` |
|      - | 1633 | `	}` |
|     11 | 1634 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 1635 | `	/* Start the 'join' process */` |
|     21 | 1636 | `	while( i < nArg ){` |
|     11 | 1637 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1638 | `			/* Iterate throw array entries */` |
|      3 | 1639 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      2 | 1640 | `		}else{` |
|      - | 1641 | `			const char *zData;` |
|      - | 1642 | `			int nLen;` |
|      - | 1643 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 1644 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1645 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 1646 | `			if( imp_data.bFirst ){` |
|      9 | 1647 | `				imp_data.bFirst = 0;` |
|      4 | 1648 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1649 | `				ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen);` |
|    ! 0 | 1650 | `			}` |
|      - | 1651 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 1652 | `			if( nLen > 0 ){` |
|      9 | 1653 | `				ph7_result_string(pCtx,zData,nLen);` |
|      4 | 1654 | `			}` |
|      - | 1655 | `		}` |
|     11 | 1656 | `		i++;` |
|      1 | 1657 | `	}` |
|     11 | 1658 | `	return PH7_OK;` |
|      7 | 1659 |  |
|      - | 1660 | `/*` |
|      - | 1661 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 1662 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 1663 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 1664 | ` * Parameters` |
|      - | 1665 | ` *  $delimiter` |
|      - | 1666 | ` *   The boundary string.` |
|      - | 1667 | ` * $string` |
|      - | 1668 | ` *   The input string.` |
|      - | 1669 | ` * $limit` |
|      - | 1670 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 1671 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 1672 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 1673 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 1674 | ` * Returns` |
|      - | 1675 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 1676 | ` *  on boundaries formed by the delimiter.` |
|      - | 1677 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 1678 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 1679 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 1680 | ` *  will be returned.` |
|      - | 1681 | ` * NOTE:` |
|      - | 1682 | ` *  Negative limit is not supported.` |
|      - | 1683 | ` */` |
|   4106 | 1684 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1685 |  |
|      - | 1686 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 1687 | `	int nDelim,nStrlen,iLimit;` |
|      - | 1688 | `	ph7_value *pArray;` |
|      - | 1689 | `	ph7_value *pValue;` |
|      - | 1690 | `	sxu32 nOfft;` |
|      - | 1691 | `	sxi32 rc;` |
|   4108 | 1692 | `	if( nArg < 2 ){` |
|      - | 1693 | `		/* Missing arguments,return FALSE */` |
|      9 | 1694 | `		ph7_result_bool(pCtx,0);` |
|      9 | 1695 | `		return PH7_OK;` |
|      - | 1696 | `	}` |
|      - | 1697 | `	/* Extract the delimiter */` |
|   4100 | 1698 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   4100 | 1699 | `	if( nDelim < 1 ){` |
|      - | 1700 | `		/* Empty delimiter,return FALSE */` |
|      3 | 1701 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1702 | `		return PH7_OK;` |
|      - | 1703 | `	}` |
|      - | 1704 | `	/* Extract the string */` |
|   4098 | 1705 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   4098 | 1706 | `	if( nStrlen < 1 ){` |
|      - | 1707 | `		/* Empty string: return an array with a single empty element (PHP behavior) */` |
|      3 | 1708 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      3 | 1709 | `		ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      3 | 1710 | `		if( pArrayTmp == 0 \|\| pValueTmp == 0 ){` |
|      - | 1711 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 1712 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 1713 | `			return PH7_OK;` |
|      - | 1714 | `		}` |
|      3 | 1715 | `		ph7_value_string(pValueTmp, "", 0);` |
|      3 | 1716 | `		ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp);` |
|      3 | 1717 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      3 | 1718 | `		return PH7_OK;` |
|      - | 1719 | `	}` |
|      - | 1720 | `	/* Point to the end of the string */` |
|   4096 | 1721 | `	zEnd = &zString[nStrlen];` |
|      - | 1722 | `	/* Create the array */` |
|   4096 | 1723 | `	pArray =  ph7_context_new_array(pCtx);` |
|   4096 | 1724 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   4096 | 1725 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 1726 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 1727 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1728 | `		return PH7_OK;` |
|      - | 1729 | `	}` |
|      - | 1730 | `	/* Set a defualt limit */` |
|   4096 | 1731 | `	iLimit = SXI32_HIGH;` |
|   4096 | 1732 | `	if( nArg > 2 ){` |
|      9 | 1733 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|      9 | 1734 | `		 if( iLimit < 0 ){` |
|      3 | 1735 | `			iLimit = -iLimit;` |
|      1 | 1736 | `		}` |
|      9 | 1737 | `		if( iLimit == 0 ){` |
|      3 | 1738 | `			iLimit = 1;` |
|      1 | 1739 | `		}` |
|      9 | 1740 | `		iLimit--;` |
|      4 | 1741 | `	}` |
|      - | 1742 | `	/* Start exploding */` |
|  45914 | 1743 | `	for(;;){` |
|  91830 | 1744 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
|  91830 | 1745 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 1746 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   4096 | 1747 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   4096 | 1748 | `			ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|   4096 | 1749 | `			break;` |
|      - | 1750 | `		}` |
|      - | 1751 | `		/* Point to the desired offset */` |
|  87736 | 1752 | `		zCur = &zString[nOfft];` |
|      - | 1753 | `		/* Perform the store operation (may be empty) */` |
|  87736 | 1754 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|  87736 | 1755 | `		ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|      - | 1756 | `		/* Point beyond the delimiter */` |
|  87736 | 1757 | `		zString = &zCur[nDelim];` |
|      - | 1758 | `		/* Reset the cursor */` |
|  87736 | 1759 | `		ph7_value_reset_string_cursor(pValue);` |
|      2 | 1760 | `	}` |
|      - | 1761 | `	/* Return the freshly created array */` |
|   4096 | 1762 | `	ph7_result_value(pCtx,pArray);` |
|      - | 1763 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 1764 | `	 * released as soon we return from this foregin function.` |
|      - | 1765 | `	 */` |
|   4096 | 1766 | `	return PH7_OK;` |
|   2055 | 1767 |  |
|      - | 1768 | `/*` |
|      - | 1769 | ` * string trim(string $str[,string $charlist ])` |
|      - | 1770 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 1771 | ` * Parameters` |
|      - | 1772 | ` *  $str` |
|      - | 1773 | ` *   The string that will be trimmed.` |
|      - | 1774 | ` * $charlist` |
|      - | 1775 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1776 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1777 | ` *   With .. you can specify a range of characters.` |
|      - | 1778 | ` * Returns.` |
|      - | 1779 | ` *  Thr processed string.` |
|      - | 1780 | ` * NOTE:` |
|      - | 1781 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 1782 | ` */` |
|   9760 | 1783 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1784 |  |
|      - | 1785 | `	const char *zString;` |
|      - | 1786 | `	int nLen;` |
|   9762 | 1787 | `	if( nArg < 1 ){` |
|      - | 1788 | `		/* Missing arguments,return null */` |
|      3 | 1789 | `		ph7_result_null(pCtx);` |
|      3 | 1790 | `		return PH7_OK;` |
|      - | 1791 | `	}` |
|      - | 1792 | `	/* Extract the target string */` |
|   9760 | 1793 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|   9760 | 1794 | `	if( nLen < 1 ){` |
|      - | 1795 | `		/* Empty string,return */` |
|   1600 | 1796 | `		ph7_result_string(pCtx,"",0);` |
|   1600 | 1797 | `		return PH7_OK;` |
|      - | 1798 | `	}` |
|      - | 1799 | `	/* Start the trim process */` |
|   8162 | 1800 | `	if( nArg < 2 ){` |
|      - | 1801 | `		SyString sStr;` |
|      - | 1802 | `		/* Remove white spaces and NUL bytes */` |
|   8158 | 1803 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  19654 | 1804 | `		SyStringFullTrimSafe(&sStr);` |
|   8158 | 1805 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   4080 | 1806 | `	}else{` |
|      - | 1807 | `		/* Char list */` |
|      - | 1808 | `		const char *zList;` |
|      - | 1809 | `		int nListlen;` |
|      5 | 1810 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 1811 | `		if( nListlen < 1 ){` |
|      - | 1812 | `			/* Return the string unchanged */` |
|      3 | 1813 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 1814 | `		}else{` |
|      3 | 1815 | `			const char *zEnd = &zString[nLen];` |
|      3 | 1816 | `			const char *zCur = zString;` |
|      - | 1817 | `			const char *zPtr;` |
|      - | 1818 | `			int i;` |
|      - | 1819 | `			/* Left trim */` |
|      4 | 1820 | `			for(;;){` |
|      9 | 1821 | `				if( zCur >= zEnd ){` |
|    ! 0 | 1822 | `					break;` |
|      - | 1823 | `				}` |
|      9 | 1824 | `				zPtr = zCur;` |
|     17 | 1825 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 1826 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|      7 | 1827 | `						zCur++;` |
|      3 | 1828 | `					}` |
|      5 | 1829 | `				}` |
|      9 | 1830 | `				if( zCur == zPtr ){` |
|      - | 1831 | `					/* No match,break immediately */` |
|      3 | 1832 | `					break;` |
|      - | 1833 | `				}` |
|      1 | 1834 | `			}` |
|      - | 1835 | `			/* Right trim */` |
|      3 | 1836 | `			zEnd--;` |
|      4 | 1837 | `			for(;;){` |
|      9 | 1838 | `				if( zEnd <= zCur ){` |
|    ! 0 | 1839 | `					break;` |
|      - | 1840 | `				}` |
|      9 | 1841 | `				zPtr = zEnd;` |
|     17 | 1842 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 1843 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      7 | 1844 | `						zEnd--;` |
|      3 | 1845 | `					}` |
|      5 | 1846 | `				}` |
|      9 | 1847 | `				if( zEnd == zPtr ){` |
|      3 | 1848 | `					break;` |
|      - | 1849 | `				}` |
|      1 | 1850 | `			}` |
|      3 | 1851 | `			if( zCur >= zEnd ){` |
|      - | 1852 | `				/* Return the empty string */` |
|    ! 0 | 1853 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1854 | `			}else{` |
|      3 | 1855 | `				zEnd++;` |
|      3 | 1856 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 1857 | `			}` |
|      - | 1858 | `		}` |
|      - | 1859 | `	}` |
|   8162 | 1860 | `	return PH7_OK;` |
|   4882 | 1861 |  |
|      - | 1862 | `/*` |
|      - | 1863 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 1864 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 1865 | ` * Parameters` |
|      - | 1866 | ` *  $str` |
|      - | 1867 | ` *   The string that will be trimmed.` |
|      - | 1868 | ` * $charlist` |
|      - | 1869 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1870 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1871 | ` *   With .. you can specify a range of characters.` |
|      - | 1872 | ` * Returns.` |
|      - | 1873 | ` *  Thr processed string.` |
|      - | 1874 | ` * NOTE:` |
|      - | 1875 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 1876 | ` */` |
|     26 | 1877 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1878 |  |
|      - | 1879 | `	const char *zString;` |
|      - | 1880 | `	int nLen;` |
|     27 | 1881 | `	if( nArg < 1 ){` |
|      - | 1882 | `		/* Missing arguments,return null */` |
|      3 | 1883 | `		ph7_result_null(pCtx);` |
|      3 | 1884 | `		return PH7_OK;` |
|      - | 1885 | `	}` |
|      - | 1886 | `	/* Extract the target string */` |
|     25 | 1887 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 1888 | `	if( nLen < 1 ){` |
|      - | 1889 | `		/* Empty string,return */` |
|      5 | 1890 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1891 | `		return PH7_OK;` |
|      - | 1892 | `	}` |
|      - | 1893 | `	/* Start the trim process */` |
|     21 | 1894 | `	if( nArg < 2 ){` |
|      - | 1895 | `		SyString sStr;` |
|      - | 1896 | `		/* Remove white spaces and NUL bytes*/` |
|     17 | 1897 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     41 | 1898 | `		SyStringRightTrimSafe(&sStr);` |
|     17 | 1899 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      9 | 1900 | `	}else{` |
|      - | 1901 | `		/* Char list */` |
|      - | 1902 | `		const char *zList;` |
|      - | 1903 | `		int nListlen;` |
|      5 | 1904 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 1905 | `		if( nListlen < 1 ){` |
|      - | 1906 | `			/* Return the string unchanged */` |
|    ! 0 | 1907 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 1908 | `		}else{` |
|      5 | 1909 | `			const char *zEnd = &zString[nLen - 1];` |
|      5 | 1910 | `			const char *zCur = zString;` |
|      - | 1911 | `			const char *zPtr;` |
|      - | 1912 | `			int i;` |
|      - | 1913 | `			/* Right trim */` |
|      6 | 1914 | `			for(;;){` |
|     13 | 1915 | `				if( zEnd <= zCur ){` |
|    ! 0 | 1916 | `					break;` |
|      - | 1917 | `				}` |
|     13 | 1918 | `				zPtr = zEnd;` |
|     25 | 1919 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     13 | 1920 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      9 | 1921 | `						zEnd--;` |
|      4 | 1922 | `					}` |
|      7 | 1923 | `				}` |
|     13 | 1924 | `				if( zEnd == zPtr ){` |
|      5 | 1925 | `					break;` |
|      - | 1926 | `				}` |
|      1 | 1927 | `			}` |
|      5 | 1928 | `			if( zEnd <= zCur ){` |
|      - | 1929 | `				/* Return the empty string */` |
|    ! 0 | 1930 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1931 | `			}else{` |
|      5 | 1932 | `				zEnd++;` |
|      5 | 1933 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 1934 | `			}` |
|      - | 1935 | `		}` |
|      - | 1936 | `	}` |
|     21 | 1937 | `	return PH7_OK;` |
|     14 | 1938 |  |
|      - | 1939 | `/*` |
|      - | 1940 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 1941 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 1942 | ` * Parameters` |
|      - | 1943 | ` *  $str` |
|      - | 1944 | ` *   The string that will be trimmed.` |
|      - | 1945 | ` * $charlist` |
|      - | 1946 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1947 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1948 | ` *   With .. you can specify a range of characters.` |
|      - | 1949 | ` * Returns.` |
|      - | 1950 | ` *  Thr processed string.` |
|      - | 1951 | ` * NOTE:` |
|      - | 1952 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 1953 | ` */` |
|     12 | 1954 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1955 |  |
|      - | 1956 | `	const char *zString;` |
|      - | 1957 | `	int nLen;` |
|     13 | 1958 | `	if( nArg < 1 ){` |
|      - | 1959 | `		/* Missing arguments,return null */` |
|      3 | 1960 | `		ph7_result_null(pCtx);` |
|      3 | 1961 | `		return PH7_OK;` |
|      - | 1962 | `	}` |
|      - | 1963 | `	/* Extract the target string */` |
|     11 | 1964 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 1965 | `	if( nLen < 1 ){` |
|      - | 1966 | `		/* Empty string,return */` |
|    ! 0 | 1967 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1968 | `		return PH7_OK;` |
|      - | 1969 | `	}` |
|      - | 1970 | `	/* Start the trim process */` |
|     11 | 1971 | `	if( nArg < 2 ){` |
|      - | 1972 | `		SyString sStr;` |
|      - | 1973 | `		/* Remove white spaces and NUL byte */` |
|      3 | 1974 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|      8 | 1975 | `		SyStringLeftTrimSafe(&sStr);` |
|      3 | 1976 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      2 | 1977 | `	}else{` |
|      - | 1978 | `		/* Char list */` |
|      - | 1979 | `		const char *zList;` |
|      - | 1980 | `		int nListlen;` |
|      9 | 1981 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      9 | 1982 | `		if( nListlen < 1 ){` |
|      - | 1983 | `			/* Return the string unchanged */` |
|      3 | 1984 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 1985 | `		}else{` |
|      7 | 1986 | `			const char *zEnd = &zString[nLen];` |
|      7 | 1987 | `			const char *zCur = zString;` |
|      - | 1988 | `			const char *zPtr;` |
|      - | 1989 | `			int i;` |
|      - | 1990 | `			/* Left trim */` |
|      7 | 1991 | `			for(;;){` |
|     15 | 1992 | `				if( zCur >= zEnd ){` |
|    ! 0 | 1993 | `					break;` |
|      - | 1994 | `				}` |
|     15 | 1995 | `				zPtr = zCur;` |
|     41 | 1996 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     27 | 1997 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|     13 | 1998 | `						zCur++;` |
|      6 | 1999 | `					}` |
|     14 | 2000 | `				}` |
|     15 | 2001 | `				if( zCur == zPtr ){` |
|      - | 2002 | `					/* No match,break immediately */` |
|      7 | 2003 | `					break;` |
|      - | 2004 | `				}` |
|      1 | 2005 | `			}` |
|      7 | 2006 | `			if( zCur >= zEnd ){` |
|      - | 2007 | `				/* Return the empty string */` |
|    ! 0 | 2008 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2009 | `			}else{` |
|      7 | 2010 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2011 | `			}` |
|      - | 2012 | `		}` |
|      - | 2013 | `	}` |
|     11 | 2014 | `	return PH7_OK;` |
|      7 | 2015 |  |
|      - | 2016 | `/*` |
|      - | 2017 | ` * string strtolower(string $str)` |
|      - | 2018 | ` *  Make a string lowercase.` |
|      - | 2019 | ` * Parameters` |
|      - | 2020 | ` *  $str` |
|      - | 2021 | ` *   The input string.` |
|      - | 2022 | ` * Returns.` |
|      - | 2023 | ` *  The lowercased string.` |
|      - | 2024 | ` */` |
|  21978 | 2025 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2026 |  |
|      - | 2027 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2028 | `	int nLen;` |
|  21980 | 2029 | `	if( nArg < 1 ){` |
|      - | 2030 | `		/* Missing arguments,return null */` |
|      3 | 2031 | `		ph7_result_null(pCtx);` |
|      3 | 2032 | `		return PH7_OK;` |
|      - | 2033 | `	}` |
|      - | 2034 | `	/* Extract the target string */` |
|  21978 | 2035 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  21978 | 2036 | `	if( nLen < 1 ){` |
|      - | 2037 | `		/* Empty string,return */` |
|      3 | 2038 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2039 | `		return PH7_OK;` |
|      - | 2040 | `	}` |
|      - | 2041 | `	/* Perform the requested operation */` |
|  21976 | 2042 | `	zEnd = &zString[nLen];` |
|  69386 | 2043 | `	for(;;){` |
| 138774 | 2044 | `		if( zString >= zEnd ){` |
|      - | 2045 | `			/* No more input,break immediately */` |
|  21976 | 2046 | `			break;` |
|      - | 2047 | `		}` |
| 116800 | 2048 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2049 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2050 | `			zCur = zString;` |
|    ! 0 | 2051 | `			zString++;` |
|    ! 0 | 2052 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2053 | `				zString++;` |
|    ! 0 | 2054 | `			}` |
|      - | 2055 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2056 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2057 | `		}else{` |
| 116800 | 2058 | `			int c = zString[0];` |
| 116800 | 2059 | `			if( SyisUpper(c) ){` |
| 116798 | 2060 | `				c = SyToLower(zString[0]);` |
|  58398 | 2061 | `			}` |
|      - | 2062 | `			/* Append character */` |
| 116800 | 2063 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2064 | `			/* Advance the cursor */` |
| 116800 | 2065 | `			zString++;` |
|      - | 2066 | `		}` |
|      2 | 2067 | `	}` |
|  21976 | 2068 | `	return PH7_OK;` |
|  10991 | 2069 |  |
|      - | 2070 | `/*` |
|      - | 2071 | ` * string strtolower(string $str)` |
|      - | 2072 | ` *  Make a string uppercase.` |
|      - | 2073 | ` * Parameters` |
|      - | 2074 | ` *  $str` |
|      - | 2075 | ` *   The input string.` |
|      - | 2076 | ` * Returns.` |
|      - | 2077 | ` *  The uppercased string.` |
|      - | 2078 | ` */` |
|     14 | 2079 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2080 |  |
|      - | 2081 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2082 | `	int nLen;` |
|     15 | 2083 | `	if( nArg < 1 ){` |
|      - | 2084 | `		/* Missing arguments,return null */` |
|      3 | 2085 | `		ph7_result_null(pCtx);` |
|      3 | 2086 | `		return PH7_OK;` |
|      - | 2087 | `	}` |
|      - | 2088 | `	/* Extract the target string */` |
|     13 | 2089 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 2090 | `	if( nLen < 1 ){` |
|      - | 2091 | `		/* Empty string,return */` |
|      3 | 2092 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2093 | `		return PH7_OK;` |
|      - | 2094 | `	}` |
|      - | 2095 | `	/* Perform the requested operation */` |
|     11 | 2096 | `	zEnd = &zString[nLen];` |
|     31 | 2097 | `	for(;;){` |
|     63 | 2098 | `		if( zString >= zEnd ){` |
|      - | 2099 | `			/* No more input,break immediately */` |
|     11 | 2100 | `			break;` |
|      - | 2101 | `		}` |
|     53 | 2102 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2103 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2104 | `			zCur = zString;` |
|    ! 0 | 2105 | `			zString++;` |
|    ! 0 | 2106 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2107 | `				zString++;` |
|    ! 0 | 2108 | `			}` |
|      - | 2109 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2110 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2111 | `		}else{` |
|     53 | 2112 | `			int c = zString[0];` |
|     53 | 2113 | `			if( SyisLower(c) ){` |
|     47 | 2114 | `				c = SyToUpper(zString[0]);` |
|     23 | 2115 | `			}` |
|      - | 2116 | `			/* Append character */` |
|     53 | 2117 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2118 | `			/* Advance the cursor */` |
|     53 | 2119 | `			zString++;` |
|      - | 2120 | `		}` |
|      1 | 2121 | `	}` |
|     11 | 2122 | `	return PH7_OK;` |
|      8 | 2123 |  |
|      - | 2124 | `/*` |
|      - | 2125 | ` * string ucfirst(string $str)` |
|      - | 2126 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2127 | ` *  character is alphabetic.` |
|      - | 2128 | ` * Parameters` |
|      - | 2129 | ` *  $str` |
|      - | 2130 | ` *   The input string.` |
|      - | 2131 | ` * Returns.` |
|      - | 2132 | ` *  The processed string.` |
|      - | 2133 | ` */` |
|      6 | 2134 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2135 |  |
|      - | 2136 | `	const char *zString,*zEnd;` |
|      - | 2137 | `	int nLen,c;` |
|      7 | 2138 | `	if( nArg < 1 ){` |
|      - | 2139 | `		/* Missing arguments,return null */` |
|      3 | 2140 | `		ph7_result_null(pCtx);` |
|      3 | 2141 | `		return PH7_OK;` |
|      - | 2142 | `	}` |
|      - | 2143 | `	/* Extract the target string */` |
|      5 | 2144 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2145 | `	if( nLen < 1 ){` |
|      - | 2146 | `		/* Empty string,return */` |
|      3 | 2147 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2148 | `		return PH7_OK;` |
|      - | 2149 | `	}` |
|      - | 2150 | `	/* Perform the requested operation */` |
|      3 | 2151 | `	zEnd = &zString[nLen];` |
|      3 | 2152 | `	c = zString[0];` |
|      3 | 2153 | `	if( SyisLower(c) ){` |
|      3 | 2154 | `		c = SyToUpper(c);` |
|      1 | 2155 | `	}` |
|      - | 2156 | `	/* Append the first character */` |
|      3 | 2157 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2158 | `	zString++;` |
|      3 | 2159 | `	if( zString < zEnd ){` |
|      - | 2160 | `		/* Append the rest of the input verbatim */` |
|      3 | 2161 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2162 | `	}` |
|      3 | 2163 | `	return PH7_OK;` |
|      4 | 2164 |  |
|      - | 2165 | `/*` |
|      - | 2166 | ` * string lcfirst(string $str)` |
|      - | 2167 | ` *  Make a string's first character lowercase.` |
|      - | 2168 | ` * Parameters` |
|      - | 2169 | ` *  $str` |
|      - | 2170 | ` *   The input string.` |
|      - | 2171 | ` * Returns.` |
|      - | 2172 | ` *  The processed string.` |
|      - | 2173 | ` */` |
|      6 | 2174 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2175 |  |
|      - | 2176 | `	const char *zString,*zEnd;` |
|      - | 2177 | `	int nLen,c;` |
|      7 | 2178 | `	if( nArg < 1 ){` |
|      - | 2179 | `		/* Missing arguments,return null */` |
|      3 | 2180 | `		ph7_result_null(pCtx);` |
|      3 | 2181 | `		return PH7_OK;` |
|      - | 2182 | `	}` |
|      - | 2183 | `	/* Extract the target string */` |
|      5 | 2184 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2185 | `	if( nLen < 1 ){` |
|      - | 2186 | `		/* Empty string,return */` |
|      3 | 2187 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2188 | `		return PH7_OK;` |
|      - | 2189 | `	}` |
|      - | 2190 | `	/* Perform the requested operation */` |
|      3 | 2191 | `	zEnd = &zString[nLen];` |
|      3 | 2192 | `	c = zString[0];` |
|      3 | 2193 | `	if( SyisUpper(c) ){` |
|      3 | 2194 | `		c = SyToLower(c);` |
|      1 | 2195 | `	}` |
|      - | 2196 | `	/* Append the first character */` |
|      3 | 2197 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2198 | `	zString++;` |
|      3 | 2199 | `	if( zString < zEnd ){` |
|      - | 2200 | `		/* Append the rest of the input verbatim */` |
|      3 | 2201 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2202 | `	}` |
|      3 | 2203 | `	return PH7_OK;` |
|      4 | 2204 |  |
|      - | 2205 | `/*` |
|      - | 2206 | ` * int ord(string $string)` |
|      - | 2207 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 2208 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 2209 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 2210 | ` * Parameters` |
|      - | 2211 | ` *  $string` |
|      - | 2212 | ` *   The input string.` |
|      - | 2213 | ` * Returns` |
|      - | 2214 | ` *  The ASCII value as an integer.` |
|      - | 2215 | ` */` |
|     62 | 2216 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2217 |  |
|      - | 2218 | `	const char *zString;` |
|      - | 2219 | `	int nLen,c;` |
|      - | 2220 | `	/* PHP requires exactly one argument. */` |
|     64 | 2221 | `	if( nArg != 1 ){` |
|      7 | 2222 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2223 | `			"ArgumentCountError",` |
|      - | 2224 | `			"ord() expects exactly 1 argument, %d given",` |
|      2 | 2225 | `			nArg` |
|      - | 2226 | `			);` |
|      - | 2227 | `	}` |
|      - | 2228 | `	/* Passing null is deprecated (E_DEPRECATED).  PHP emits this before` |
|      - | 2229 | `	 * the empty-string deprecation, so we check null first. */` |
|     59 | 2230 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 2231 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2232 | `			"ord(): Passing null to parameter #1 ($character) "` |
|      - | 2233 | `			"of type string is deprecated"` |
|      - | 2234 | `			);` |
|      1 | 2235 | `	}` |
|      - | 2236 | `	/* Extract the target string */` |
|     59 | 2237 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     59 | 2238 | `	if( nLen < 1 ){` |
|      - | 2239 | `		/* Empty string is deprecated (E_DEPRECATED). */` |
|      5 | 2240 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2241 | `			"ord(): Providing an empty string is deprecated"` |
|      - | 2242 | `			);` |
|      5 | 2243 | `		ph7_result_int(pCtx,0);` |
|      5 | 2244 | `		return PH7_OK;` |
|      - | 2245 | `	}` |
|      - | 2246 | `	/* A string longer than one byte is deprecated (E_DEPRECATED). */` |
|     55 | 2247 | `	if( nLen > 1 ){` |
|      7 | 2248 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2249 | `			"ord(): Providing a string that is not one byte long "` |
|      - | 2250 | `			"is deprecated. Use ord($str[0]) instead"` |
|      - | 2251 | `			);` |
|      3 | 2252 | `	}` |
|      - | 2253 | `	/* Extract the ASCII value of the first character */` |
|     55 | 2254 | `	c = (unsigned char)zString[0];` |
|      - | 2255 | `	/* Return that value */` |
|     55 | 2256 | `	ph7_result_int(pCtx,c);` |
|     55 | 2257 | `	return PH7_OK;` |
|     33 | 2258 |  |
|      - | 2259 | `/*` |
|      - | 2260 | ` * string chr(int $codepoint)` |
|      - | 2261 | ` *  Returns a one-character string containing the character specified` |
|      - | 2262 | ` *  by the given codepoint.  Any integer is accepted; values outside` |
|      - | 2263 | ` *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.` |
|      - | 2264 | ` * Parameters` |
|      - | 2265 | ` *  $codepoint` |
|      - | 2266 | ` *   An integer codepoint.  Values outside 0-255 are deprecated and` |
|      - | 2267 | ` *   will be constrained to a single byte.` |
|      - | 2268 | ` * Returns` |
|      - | 2269 | ` *  A single-character string.` |
|      - | 2270 | ` */` |
|     44 | 2271 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2272 |  |
|      - | 2273 | `	int c;` |
|      - | 2274 | `	unsigned char ch;` |
|      - | 2275 | `	/* PHP requires exactly one argument. */` |
|     46 | 2276 | `	if( nArg != 1 ){` |
|      7 | 2277 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2278 | `			"ArgumentCountError",` |
|      - | 2279 | `			"chr() expects exactly 1 argument, %d given",` |
|      2 | 2280 | `			nArg` |
|      - | 2281 | `			);` |
|      - | 2282 | `	}` |
|      - | 2283 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 2284 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 2285 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 2286 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|     41 | 2287 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      - | 2288 | `		char zBuf[120];` |
|      4 | 2289 | `		SyBufferFormat(zBuf,sizeof(zBuf),` |
|      - | 2290 | `			"Implicit conversion from float %g to int loses precision",` |
|      1 | 2291 | `			ph7_value_to_double(apArg[0])` |
|      - | 2292 | `			);` |
|      3 | 2293 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zBuf);` |
|      1 | 2294 | `	}` |
|      - | 2295 | `	/* Extract the codepoint. */` |
|     41 | 2296 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 2297 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 2298 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 2299 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 2300 | `	 * name to avoid the API double-prefixing it. */` |
|     41 | 2301 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 2302 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 2303 | `			E_DEPRECATED,` |
|      - | 2304 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 2305 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 2306 | `			"The value used will be constrained using % 256"` |
|      - | 2307 | `			);` |
|      2 | 2308 | `	}` |
|      - | 2309 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 2310 | `	 * when taking the address of a wider int. */` |
|     41 | 2311 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 2312 | `	/* Return the specified character */` |
|     41 | 2313 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|     41 | 2314 | `	return PH7_OK;` |
|     24 | 2315 |  |
|      - | 2316 | `/*` |
|      - | 2317 | ` * Binary to hex consumer callback.` |
|      - | 2318 | ` * This callback is the default consumer used by the hash functions` |
|      - | 2319 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 2320 | ` */` |
|    226 | 2321 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 2322 |  |
|      - | 2323 | `	/* Append hex chunk verbatim */` |
|    227 | 2324 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|    227 | 2325 | `	return SXRET_OK;` |
|      1 | 2326 |  |
|      - | 2327 |  |
|      - | 2328 | `/*` |
|      - | 2329 | ` * string bin2hex(string $str)` |
|      - | 2330 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 2331 | ` * Parameters` |
|      - | 2332 | ` *  $str` |
|      - | 2333 | ` *   The input string.` |
|      - | 2334 | ` * Returns.` |
|      - | 2335 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 2336 | ` */` |
|     20 | 2337 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2338 |  |
|      - | 2339 | `	const char *zString;` |
|      - | 2340 | `	int nLen;` |
|      - | 2341 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|     22 | 2342 | `	if( nArg != 1 ){` |
|      7 | 2343 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2344 | `			"ArgumentCountError",` |
|      - | 2345 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|      2 | 2346 | `			nArg` |
|      - | 2347 | `			);` |
|      - | 2348 | `	}` |
|      - | 2349 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 2350 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 2351 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 2352 | `	 */` |
|     25 | 2353 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|     12 | 2354 | `		( ph7_value_is_object(apArg[0]) &&` |
|      3 | 2355 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|      2 | 2356 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|      1 | 2357 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 2358 | `		)` |
|      - | 2359 | `	){` |
|      7 | 2360 | `		const char *zType = ph7_type_name(apArg[0]);` |
|      7 | 2361 | `		if( ph7_value_is_object(apArg[0]) ){` |
|      3 | 2362 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      3 | 2363 | `			if( pInst && pInst->pClass ){` |
|      3 | 2364 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      1 | 2365 | `			}` |
|      1 | 2366 | `		}` |
|     10 | 2367 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2368 | `			"TypeError",` |
|      - | 2369 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|      3 | 2370 | `			zType` |
|      - | 2371 | `			);` |
|      - | 2372 | `	}` |
|      - | 2373 | `	/* Extract the target string */` |
|     11 | 2374 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 2375 | `	if( nLen < 1 ){` |
|      - | 2376 | `		/* Empty string,return */` |
|      3 | 2377 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2378 | `		return PH7_OK;` |
|      - | 2379 | `	}` |
|      - | 2380 | `	/* Perform the requested operation */` |
|      9 | 2381 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|      9 | 2382 | `	return PH7_OK;` |
|     12 | 2383 |  |
|      - | 2384 |  |
|      - | 2385 | `/* Search callback signature */` |
|      - | 2386 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 2387 | `/*` |
|      - | 2388 | ` * Case-insensitive pattern match.` |
|      - | 2389 | ` * Brute force is the default search method used here.` |
|      - | 2390 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 2391 | ` * well for short/medium texts on modern hardware.` |
|      - | 2392 | ` */` |
|    118 | 2393 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      1 | 2394 |  |
|    119 | 2395 | `	const char *zpIn = (const char *)pPattern;` |
|    119 | 2396 | `	const char *zIn = (const char *)pText;` |
|    119 | 2397 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    119 | 2398 | `	const char *zEnd = &zIn[nLen];` |
|      - | 2399 | `	const char *zPtr,*zPtr2;` |
|      - | 2400 | `	int c,d;` |
|    119 | 2401 | `	if( iPatLen > nLen ){` |
|      - | 2402 | `		/* Don't bother processing */` |
|     33 | 2403 | `		return SXERR_NOTFOUND;` |
|      - | 2404 | `	}` |
|    244 | 2405 | `	for(;;){` |
|    489 | 2406 | `		if( zIn >= zEnd ){` |
|     47 | 2407 | `			break;` |
|      - | 2408 | `		}` |
|    443 | 2409 | `		c = SyToLower(zIn[0]);` |
|    443 | 2410 | `		d = SyToLower(zpIn[0]);` |
|    443 | 2411 | `		if( c == d ){` |
|     41 | 2412 | `			zPtr   = &zIn[1];` |
|     41 | 2413 | `			zPtr2  = &zpIn[1];` |
|     71 | 2414 | `			for(;;){` |
|    143 | 2415 | `				if( zPtr2 >= zpEnd ){` |
|      - | 2416 | `					/* Pattern found */` |
|     41 | 2417 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 2418 | `					return SXRET_OK;` |
|      - | 2419 | `				}` |
|    103 | 2420 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 2421 | `					break;` |
|      - | 2422 | `				}` |
|    103 | 2423 | `				c = SyToLower(zPtr[0]);` |
|    103 | 2424 | `				d = SyToLower(zPtr2[0]);` |
|    103 | 2425 | `				if( c != d ){` |
|    ! 0 | 2426 | `					break;` |
|      - | 2427 | `				}` |
|    103 | 2428 | `				zPtr++; zPtr2++;` |
|      1 | 2429 | `			}` |
|    ! 0 | 2430 | `		}` |
|    403 | 2431 | `		zIn++;` |
|      1 | 2432 | `	}` |
|      - | 2433 | `	/* Pattern not found */` |
|     47 | 2434 | `	return SXERR_NOTFOUND;` |
|     60 | 2435 |  |
|      - | 2436 | `/*` |
|      - | 2437 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2438 | ` *  Find the first occurrence of a string.` |
|      - | 2439 | ` * Parameters` |
|      - | 2440 | ` *  $haystack` |
|      - | 2441 | ` *   The input string.` |
|      - | 2442 | ` * $needle` |
|      - | 2443 | ` *   Search pattern (must be a string).` |
|      - | 2444 | ` * $before_needle` |
|      - | 2445 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2446 | ` *   of the needle (excluding the needle).` |
|      - | 2447 | ` * Return` |
|      - | 2448 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2449 | ` */` |
|     10 | 2450 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2451 |  |
|     11 | 2452 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2453 | `	const char *zBlob,*zPattern;` |
|      - | 2454 | `	int nLen,nPatLen;` |
|      - | 2455 | `	sxu32 nOfft;` |
|      - | 2456 | `	sxi32 rc;` |
|     11 | 2457 | `	if( nArg < 2 ){` |
|      - | 2458 | `		/* Missing arguments,return FALSE */` |
|      5 | 2459 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2460 | `		return PH7_OK;` |
|      - | 2461 | `	}` |
|      - | 2462 | `	/* Extract the needle and the haystack */` |
|      7 | 2463 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 2464 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 2465 | `	nOfft = 0; /* cc warning */` |
|      9 | 2466 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 2467 | `		int before = 0;` |
|      - | 2468 | `		/* Perform the lookup */` |
|      5 | 2469 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2470 | `		if( rc != SXRET_OK ){` |
|      - | 2471 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2472 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2473 | `			return PH7_OK;` |
|      - | 2474 | `		}` |
|      - | 2475 | `		/* Return the portion of the string */` |
|      5 | 2476 | `		if( nArg > 2 ){` |
|      3 | 2477 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2478 | `		}` |
|      5 | 2479 | `		if( before ){` |
|      3 | 2480 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2481 | `		}else{` |
|      3 | 2482 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2483 | `		}` |
|      3 | 2484 | `	}else{` |
|      3 | 2485 | `		ph7_result_bool(pCtx,0);` |
|      - | 2486 | `	}` |
|      7 | 2487 | `	return PH7_OK;` |
|      6 | 2488 |  |
|      - | 2489 | `/*` |
|      - | 2490 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2491 | ` *  Case-insensitive strstr().` |
|      - | 2492 | ` * Parameters` |
|      - | 2493 | ` *  $haystack` |
|      - | 2494 | ` *   The input string.` |
|      - | 2495 | ` * $needle` |
|      - | 2496 | ` *   Search pattern (must be a string).` |
|      - | 2497 | ` * $before_needle` |
|      - | 2498 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2499 | ` *   of the needle (excluding the needle).` |
|      - | 2500 | ` * Return` |
|      - | 2501 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2502 | ` */` |
|      6 | 2503 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2504 |  |
|      7 | 2505 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2506 | `	const char *zBlob,*zPattern;` |
|      - | 2507 | `	int nLen,nPatLen;` |
|      - | 2508 | `	sxu32 nOfft;` |
|      - | 2509 | `	sxi32 rc;` |
|      7 | 2510 | `	if( nArg < 2 ){` |
|      - | 2511 | `		/* Missing arguments,return FALSE */` |
|      3 | 2512 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2513 | `		return PH7_OK;` |
|      - | 2514 | `	}` |
|      - | 2515 | `	/* Extract the needle and the haystack */` |
|      5 | 2516 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2517 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 2518 | `	nOfft = 0; /* cc warning */` |
|      7 | 2519 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 2520 | `		int before = 0;` |
|      - | 2521 | `		/* Perform the lookup */` |
|      5 | 2522 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2523 | `		if( rc != SXRET_OK ){` |
|      - | 2524 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2525 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2526 | `			return PH7_OK;` |
|      - | 2527 | `		}` |
|      - | 2528 | `		/* Return the portion of the string */` |
|      5 | 2529 | `		if( nArg > 2 ){` |
|      3 | 2530 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2531 | `		}` |
|      5 | 2532 | `		if( before ){` |
|      3 | 2533 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2534 | `		}else{` |
|      3 | 2535 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2536 | `		}` |
|      3 | 2537 | `	}else{` |
|    ! 0 | 2538 | `		ph7_result_bool(pCtx,0);` |
|      - | 2539 | `	}` |
|      5 | 2540 | `	return PH7_OK;` |
|      4 | 2541 |  |
|      - | 2542 | `/*` |
|      - | 2543 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2544 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 2545 | ` * Parameters` |
|      - | 2546 | ` *  $haystack` |
|      - | 2547 | ` *   The input string.` |
|      - | 2548 | ` * $needle` |
|      - | 2549 | ` *   Search pattern (must be a string).` |
|      - | 2550 | ` * $offset` |
|      - | 2551 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 2552 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 2553 | ` *   of haystack.` |
|      - | 2554 | ` * Return` |
|      - | 2555 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 2556 | ` */` |
|     80 | 2557 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2558 |  |
|     82 | 2559 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2560 | `	const char *zBlob,*zPattern;` |
|      - | 2561 | `	int nLen,nPatLen,nStart;` |
|      - | 2562 | `	sxu32 nOfft;` |
|      - | 2563 | `	sxi32 rc;` |
|     82 | 2564 | `	if( nArg < 2 ){` |
|      - | 2565 | `		/* Missing arguments,return FALSE */` |
|      7 | 2566 | `		ph7_result_bool(pCtx,0);` |
|      7 | 2567 | `		return PH7_OK;` |
|      - | 2568 | `	}` |
|      - | 2569 | `	/* Extract the needle and the haystack */` |
|     76 | 2570 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     76 | 2571 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     76 | 2572 | `	nOfft = 0; /* cc warning */` |
|     76 | 2573 | `	nStart = 0;` |
|      - | 2574 | `	/* Peek the starting offset if available */` |
|     76 | 2575 | `	if( nArg > 2 ){` |
|    ! 0 | 2576 | `		nStart = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 2577 | `		if( nStart < 0 ){` |
|    ! 0 | 2578 | `			nStart = -nStart;` |
|    ! 0 | 2579 | `		}` |
|    ! 0 | 2580 | `		if( nStart >= nLen ){` |
|      - | 2581 | `			/* Invalid offset */` |
|    ! 0 | 2582 | `			nStart = 0;` |
|    ! 0 | 2583 | `		}else{` |
|    ! 0 | 2584 | `			zBlob += nStart;` |
|    ! 0 | 2585 | `			nLen -= nStart;` |
|      - | 2586 | `		}` |
|    ! 0 | 2587 | `	}` |
|     76 | 2588 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2589 | `		/* Perform the lookup */` |
|     74 | 2590 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     74 | 2591 | `		if( rc != SXRET_OK ){` |
|      - | 2592 | `			/* Pattern not found,return FALSE */` |
|      5 | 2593 | `			ph7_result_bool(pCtx,0);` |
|      5 | 2594 | `			return PH7_OK;` |
|      - | 2595 | `		}` |
|      - | 2596 | `		/* Return the pattern position */` |
|     70 | 2597 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|     36 | 2598 | `	}else{` |
|      3 | 2599 | `		ph7_result_bool(pCtx,0);` |
|      - | 2600 | `	}` |
|     72 | 2601 | `	return PH7_OK;` |
|     42 | 2602 |  |
|      - | 2603 | `/*` |
|      - | 2604 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2605 | ` *  Case-insensitive strpos.` |
|      - | 2606 | ` * Parameters` |
|      - | 2607 | ` *  $haystack` |
|      - | 2608 | ` *   The input string.` |
|      - | 2609 | ` * $needle` |
|      - | 2610 | ` *   Search pattern (must be a string).` |
|      - | 2611 | ` * $offset` |
|      - | 2612 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 2613 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 2614 | ` *   of haystack.` |
|      - | 2615 | ` * Return` |
|      - | 2616 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 2617 | ` */` |
|     18 | 2618 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2619 |  |
|     19 | 2620 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2621 | `	const char *zBlob,*zPattern;` |
|      - | 2622 | `	int nLen,nPatLen,nStart;` |
|      - | 2623 | `	sxu32 nOfft;` |
|      - | 2624 | `	sxi32 rc;` |
|     19 | 2625 | `	if( nArg < 2 ){` |
|      - | 2626 | `		/* Missing arguments,return FALSE */` |
|      3 | 2627 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2628 | `		return PH7_OK;` |
|      - | 2629 | `	}` |
|      - | 2630 | `	/* Extract the needle and the haystack */` |
|     17 | 2631 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 2632 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     17 | 2633 | `	nOfft = 0; /* cc warning */` |
|     17 | 2634 | `	nStart = 0;` |
|      - | 2635 | `	/* Peek the starting offset if available */` |
|     17 | 2636 | `	if( nArg > 2 ){` |
|      5 | 2637 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 2638 | `		if( nStart < 0 ){` |
|      3 | 2639 | `			nStart = -nStart;` |
|      1 | 2640 | `		}` |
|      5 | 2641 | `		if( nStart >= nLen ){` |
|      - | 2642 | `			/* Invalid offset */` |
|    ! 0 | 2643 | `			nStart = 0;` |
|    ! 0 | 2644 | `		}else{` |
|      5 | 2645 | `			zBlob += nStart;` |
|      5 | 2646 | `			nLen -= nStart;` |
|      - | 2647 | `		}` |
|      2 | 2648 | `	}` |
|     17 | 2649 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2650 | `		/* Perform the lookup */` |
|     17 | 2651 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     17 | 2652 | `		if( rc != SXRET_OK ){` |
|      - | 2653 | `			/* Pattern not found,return FALSE */` |
|      3 | 2654 | `			ph7_result_bool(pCtx,0);` |
|      3 | 2655 | `			return PH7_OK;` |
|      - | 2656 | `		}` |
|      - | 2657 | `		/* Return the pattern position */` |
|     15 | 2658 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 2659 | `	}else{` |
|    ! 0 | 2660 | `		ph7_result_bool(pCtx,0);` |
|      - | 2661 | `	}` |
|     15 | 2662 | `	return PH7_OK;` |
|     10 | 2663 |  |
|      - | 2664 | `/*` |
|      - | 2665 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2666 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 2667 | ` * Parameters` |
|      - | 2668 | ` *  $haystack` |
|      - | 2669 | ` *   The input string.` |
|      - | 2670 | ` * $needle` |
|      - | 2671 | ` *   Search pattern (must be a string).` |
|      - | 2672 | ` * $offset` |
|      - | 2673 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 2674 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 2675 | ` *   characters from the end of the string, searching backwards.` |
|      - | 2676 | ` * Return` |
|      - | 2677 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 2678 | ` */` |
|     32 | 2679 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2680 |  |
|      - | 2681 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     33 | 2682 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2683 | `	int nLen,nPatLen;` |
|      - | 2684 | `	sxu32 nOfft;` |
|      - | 2685 | `	sxi32 rc;` |
|     33 | 2686 | `	if( nArg < 2 ){` |
|      - | 2687 | `		/* Missing arguments,return FALSE */` |
|      3 | 2688 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2689 | `		return PH7_OK;` |
|      - | 2690 | `	}` |
|      - | 2691 | `	/* Extract the needle and the haystack */` |
|     31 | 2692 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 2693 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 2694 | `	/* Point to the end of the pattern */` |
|     31 | 2695 | `	zPtr = &zBlob[nLen - 1];` |
|     31 | 2696 | `	zEnd = &zBlob[nLen];` |
|      - | 2697 | `	/* Save the starting posistion */` |
|     31 | 2698 | `	zStart = zBlob;` |
|     31 | 2699 | `	nOfft = 0; /* cc warning */` |
|      - | 2700 | `	/* Peek the starting offset if available */` |
|     31 | 2701 | `	if( nArg > 2 ){` |
|      - | 2702 | `		int nStart;` |
|     21 | 2703 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 2704 | `		if( nStart < 0 ){` |
|     11 | 2705 | `			nStart = -nStart;` |
|     11 | 2706 | `			if( nStart >= nLen ){` |
|      - | 2707 | `				/* Invalid offset */` |
|      3 | 2708 | `				ph7_result_bool(pCtx,0);` |
|      3 | 2709 | `				return PH7_OK;` |
|    ! 0 | 2710 | `			}else{` |
|      9 | 2711 | `				nLen -= nStart;` |
|      9 | 2712 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 2713 | `				zEnd = &zBlob[nLen];` |
|      - | 2714 | `			}` |
|      5 | 2715 | `		}else{` |
|     11 | 2716 | `			if( nStart >= nLen ){` |
|      - | 2717 | `				/* Invalid offset */` |
|      5 | 2718 | `				ph7_result_bool(pCtx,0);` |
|      5 | 2719 | `				return PH7_OK;` |
|    ! 0 | 2720 | `			}else{` |
|      7 | 2721 | `				zBlob += nStart;` |
|      7 | 2722 | `				nLen -= nStart;` |
|      - | 2723 | `			}` |
|      - | 2724 | `		}` |
|      7 | 2725 | `	}` |
|     25 | 2726 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2727 | `		/* Perform the lookup */` |
|     57 | 2728 | `		for(;;){` |
|    115 | 2729 | `			if( zBlob >= zPtr ){` |
|     11 | 2730 | `				break;` |
|      - | 2731 | `			}` |
|    105 | 2732 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    105 | 2733 | `			if( rc == SXRET_OK ){` |
|      - | 2734 | `				/* Pattern found,return it's position */` |
|     13 | 2735 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 2736 | `				return PH7_OK;` |
|      - | 2737 | `			}` |
|     93 | 2738 | `			zPtr--;` |
|      1 | 2739 | `		}` |
|      - | 2740 | `		/* Pattern not found,return FALSE */` |
|     11 | 2741 | `		ph7_result_bool(pCtx,0);` |
|      6 | 2742 | `	}else{` |
|      3 | 2743 | `		ph7_result_bool(pCtx,0);` |
|      - | 2744 | `	}` |
|     13 | 2745 | `	return PH7_OK;` |
|     17 | 2746 |  |
|      - | 2747 | `/*` |
|      - | 2748 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2749 | ` *  Case-insensitive strrpos.` |
|      - | 2750 | ` * Parameters` |
|      - | 2751 | ` *  $haystack` |
|      - | 2752 | ` *   The input string.` |
|      - | 2753 | ` * $needle` |
|      - | 2754 | ` *   Search pattern (must be a string).` |
|      - | 2755 | ` * $offset` |
|      - | 2756 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 2757 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 2758 | ` *   characters from the end of the string, searching backwards.` |
|      - | 2759 | ` * Return` |
|      - | 2760 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 2761 | ` */` |
|     28 | 2762 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2763 |  |
|      - | 2764 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     29 | 2765 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2766 | `	int nLen,nPatLen;` |
|      - | 2767 | `	sxu32 nOfft;` |
|      - | 2768 | `	sxi32 rc;` |
|     29 | 2769 | `	if( nArg < 2 ){` |
|      - | 2770 | `		/* Missing arguments,return FALSE */` |
|      3 | 2771 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2772 | `		return PH7_OK;` |
|      - | 2773 | `	}` |
|      - | 2774 | `	/* Extract the needle and the haystack */` |
|     27 | 2775 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 2776 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 2777 | `	/* Point to the end of the pattern */` |
|     27 | 2778 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 2779 | `	zEnd = &zBlob[nLen];` |
|      - | 2780 | `	/* Save the starting posistion */` |
|     27 | 2781 | `	zStart = zBlob;` |
|     27 | 2782 | `	nOfft = 0; /* cc warning */` |
|      - | 2783 | `	/* Peek the starting offset if available */` |
|     27 | 2784 | `	if( nArg > 2 ){` |
|      - | 2785 | `		int nStart;` |
|     15 | 2786 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 2787 | `		if( nStart < 0 ){` |
|      7 | 2788 | `			nStart = -nStart;` |
|      7 | 2789 | `			if( nStart >= nLen ){` |
|      - | 2790 | `				/* Invalid offset */` |
|      3 | 2791 | `				ph7_result_bool(pCtx,0);` |
|      3 | 2792 | `				return PH7_OK;` |
|    ! 0 | 2793 | `			}else{` |
|      5 | 2794 | `				nLen -= nStart;` |
|      5 | 2795 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 2796 | `				zEnd = &zBlob[nLen];` |
|      - | 2797 | `			}` |
|      3 | 2798 | `		}else{` |
|      9 | 2799 | `			if( nStart >= nLen ){` |
|      - | 2800 | `				/* Invalid offset */` |
|      5 | 2801 | `				ph7_result_bool(pCtx,0);` |
|      5 | 2802 | `				return PH7_OK;` |
|    ! 0 | 2803 | `			}else{` |
|      5 | 2804 | `				zBlob += nStart;` |
|      5 | 2805 | `				nLen -= nStart;` |
|      - | 2806 | `			}` |
|      - | 2807 | `		}` |
|      4 | 2808 | `	}` |
|     21 | 2809 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2810 | `		/* Perform the lookup */` |
|     44 | 2811 | `		for(;;){` |
|     89 | 2812 | `			if( zBlob >= zPtr ){` |
|      9 | 2813 | `				break;` |
|      - | 2814 | `			}` |
|     81 | 2815 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 2816 | `			if( rc == SXRET_OK ){` |
|      - | 2817 | `				/* Pattern found,return it's position */` |
|     11 | 2818 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 2819 | `				return PH7_OK;` |
|      - | 2820 | `			}` |
|     71 | 2821 | `			zPtr--;` |
|      1 | 2822 | `		}` |
|      - | 2823 | `		/* Pattern not found,return FALSE */` |
|      9 | 2824 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2825 | `	}else{` |
|      3 | 2826 | `		ph7_result_bool(pCtx,0);` |
|      - | 2827 | `	}` |
|     11 | 2828 | `	return PH7_OK;` |
|     15 | 2829 |  |
|      - | 2830 | `/*` |
|      - | 2831 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 2832 | ` *  Find the last occurrence of a character in a string.` |
|      - | 2833 | ` * Parameters` |
|      - | 2834 | ` *  $haystack` |
|      - | 2835 | ` *   The input string.` |
|      - | 2836 | ` * $needle` |
|      - | 2837 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 2838 | ` *  This behavior is different from that of strstr().` |
|      - | 2839 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 2840 | ` *  as the ordinal value of a character.` |
|      - | 2841 | ` * Return` |
|      - | 2842 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 2843 | ` */` |
|     24 | 2844 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2845 |  |
|      - | 2846 | `	const char *zBlob;` |
|      - | 2847 | `	int nLen,c;` |
|     25 | 2848 | `	if( nArg < 2 ){` |
|      - | 2849 | `		/* Missing arguments,return FALSE */` |
|      3 | 2850 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2851 | `		return PH7_OK;` |
|      - | 2852 | `	}` |
|      - | 2853 | `	/* Extract the haystack */` |
|     23 | 2854 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 2855 | `	c = 0; /* cc warning */` |
|     23 | 2856 | `	if( nLen > 0 ){` |
|      - | 2857 | `		sxu32 nOfft;` |
|      - | 2858 | `		sxi32 rc;` |
|     21 | 2859 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 2860 | `			const char *zPattern;` |
|     11 | 2861 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 2862 | `														 * for NULL pointer.` |
|      - | 2863 | `														 */` |
|     11 | 2864 | `			c = zPattern[0];` |
|      6 | 2865 | `		}else{` |
|      - | 2866 | `			/* Int cast */` |
|     11 | 2867 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 2868 | `		}` |
|      - | 2869 | `		/* Perform the lookup */` |
|     21 | 2870 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 2871 | `		if( rc != SXRET_OK ){` |
|      - | 2872 | `			/* No such entry,return FALSE */` |
|      7 | 2873 | `			ph7_result_bool(pCtx,0);` |
|      7 | 2874 | `			return PH7_OK;` |
|      - | 2875 | `		}` |
|      - | 2876 | `		/* Return the string portion */` |
|     15 | 2877 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 2878 | `	}else{` |
|      3 | 2879 | `		ph7_result_bool(pCtx,0);` |
|      - | 2880 | `	}` |
|     17 | 2881 | `	return PH7_OK;` |
|     13 | 2882 |  |
|      - | 2883 | `/*` |
|      - | 2884 | ` * string strrev(string $string)` |
|      - | 2885 | ` *  Reverse a string.` |
|      - | 2886 | ` * Parameters` |
|      - | 2887 | ` *  $string` |
|      - | 2888 | ` *   String to be reversed.` |
|      - | 2889 | ` * Return` |
|      - | 2890 | ` *  The reversed string.` |
|      - | 2891 | ` */` |
|      4 | 2892 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2893 |  |
|      - | 2894 | `	const char *zIn,*zEnd;` |
|      - | 2895 | `	int nLen,c;` |
|      5 | 2896 | `	if( nArg < 1 ){` |
|      - | 2897 | `		/* Missing arguments,return NULL */` |
|      3 | 2898 | `		ph7_result_null(pCtx);` |
|      3 | 2899 | `		return PH7_OK;` |
|      - | 2900 | `	}` |
|      - | 2901 | `	/* Extract the target string */` |
|      3 | 2902 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 2903 | `	if( nLen < 1 ){` |
|      - | 2904 | `		/* Empty string Return null */` |
|    ! 0 | 2905 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2906 | `		return PH7_OK;` |
|      - | 2907 | `	}` |
|      - | 2908 | `	/* Perform the requested operation */` |
|      3 | 2909 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 2910 | `	for(;;){` |
|      9 | 2911 | `		if( zEnd < zIn ){` |
|      - | 2912 | `			/* No more input to process */` |
|      3 | 2913 | `			break;` |
|      - | 2914 | `		}` |
|      - | 2915 | `		/* Append current character */` |
|      7 | 2916 | `		c = zEnd[0];` |
|      7 | 2917 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 2918 | `		zEnd--;` |
|      1 | 2919 | `	}` |
|      3 | 2920 | `	return PH7_OK;` |
|      3 | 2921 |  |
|      - | 2922 | `/*` |
|      - | 2923 | ` * string ucwords(string $string)` |
|      - | 2924 | ` *  Uppercase the first character of each word in a string.` |
|      - | 2925 | ` *  The definition of a word is any string of characters that is immediately after` |
|      - | 2926 | ` *  a whitespace (These are: space, form-feed, newline, carriage return, horizontal tab, and vertical tab).` |
|      - | 2927 | ` * Parameters` |
|      - | 2928 | ` *  $string` |
|      - | 2929 | ` *   The input string.` |
|      - | 2930 | ` * Return` |
|      - | 2931 | ` *  The modified string..` |
|      - | 2932 | ` */` |
|     14 | 2933 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2934 |  |
|      - | 2935 | `	const char *zIn,*zCur,*zEnd;` |
|      - | 2936 | `	int nLen,c;` |
|     15 | 2937 | `	if( nArg < 1 ){` |
|      - | 2938 | `		/* Missing arguments,return NULL */` |
|      3 | 2939 | `		ph7_result_null(pCtx);` |
|      3 | 2940 | `		return PH7_OK;` |
|      - | 2941 | `	}` |
|      - | 2942 | `	/* Extract the target string */` |
|     13 | 2943 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 2944 | `	if( nLen < 1 ){` |
|      - | 2945 | `		/* Empty string – match PHP semantics */` |
|      3 | 2946 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2947 | `		return PH7_OK;` |
|      - | 2948 | `	}` |
|      - | 2949 | `	/* Perform the requested operation */` |
|     11 | 2950 | `	zEnd = &zIn[nLen];` |
|     21 | 2951 | `	for(;;){` |
|      - | 2952 | `		/* Jump leading white spaces */` |
|     43 | 2953 | `		zCur = zIn;` |
|     65 | 2954 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0x80 && SyisSpace(zIn[0]) ){` |
|     23 | 2955 | `			zIn++;` |
|      1 | 2956 | `		}` |
|     43 | 2957 | `		if( zCur < zIn ){` |
|      - | 2958 | `			/* Append white space stream */` |
|     23 | 2959 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 2960 | `		}` |
|     43 | 2961 | `		if( zIn >= zEnd ){` |
|      - | 2962 | `			/* No more input to process */` |
|     11 | 2963 | `			break;` |
|      - | 2964 | `		}` |
|     33 | 2965 | `		c = zIn[0];` |
|     33 | 2966 | `		if( c < 0x80 && SyisLower(c) ){` |
|     29 | 2967 | `			c = SyToUpper(c);` |
|     14 | 2968 | `		}` |
|      - | 2969 | `		/* Append the upper-cased character */` |
|     33 | 2970 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     33 | 2971 | `		zIn++;` |
|     33 | 2972 | `		zCur = zIn;` |
|      - | 2973 | `		/* Append the word varbatim */` |
|    149 | 2974 | `		while( zIn < zEnd ){` |
|    139 | 2975 | `			if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 2976 | `				/* UTF-8 stream */` |
|    ! 0 | 2977 | `				zIn++;` |
|    ! 0 | 2978 | `				SX_JMP_UTF8(zIn,zEnd);` |
|    138 | 2979 | `			}else if( !SyisSpace(zIn[0]) ){` |
|    117 | 2980 | `				zIn++;` |
|     59 | 2981 | `			}else{` |
|     23 | 2982 | `				break;` |
|      - | 2983 | `			}` |
|      1 | 2984 | `		}` |
|     33 | 2985 | `		if( zCur < zIn ){` |
|     33 | 2986 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 | 2987 | `		}` |
|      1 | 2988 | `	}` |
|     11 | 2989 | `	return PH7_OK;` |
|      8 | 2990 |  |
|      - | 2991 | `/*` |
|      - | 2992 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 2993 | ` *  Returns input repeated multiplier times.` |
|      - | 2994 | ` * Parameters` |
|      - | 2995 | ` *  $string` |
|      - | 2996 | ` *   String to be repeated.` |
|      - | 2997 | ` * $multiplier` |
|      - | 2998 | ` *  Number of time the input string should be repeated.` |
|      - | 2999 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3000 | ` *  to 0, the function will return an empty string.` |
|      - | 3001 | ` * Return` |
|      - | 3002 | ` *  The repeated string.` |
|      - | 3003 | ` */` |
|  20212 | 3004 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3005 |  |
|      - | 3006 | `	const char *zIn;` |
|      - | 3007 | `	int nLen,nMul;` |
|      - | 3008 | `	int rc;` |
|  20213 | 3009 | `	if( nArg < 2 ){` |
|      - | 3010 | `		/* Missing arguments,return NULL */` |
|      3 | 3011 | `		ph7_result_null(pCtx);` |
|      3 | 3012 | `		return PH7_OK;` |
|      - | 3013 | `	}` |
|      - | 3014 | `	/* Extract the target string */` |
|  20211 | 3015 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|  20211 | 3016 | `	if( nLen < 1 ){` |
|      - | 3017 | `		/* Empty string.Return null */` |
|    ! 0 | 3018 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3019 | `		return PH7_OK;` |
|      - | 3020 | `	}` |
|      - | 3021 | `	/* Extract the multiplier */` |
|  20211 | 3022 | `	nMul = ph7_value_to_int(apArg[1]);` |
|  20211 | 3023 | `	if( nMul < 1 ){` |
|      - | 3024 | `		/* Return the empty string */` |
|      3 | 3025 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3026 | `		return PH7_OK;` |
|      - | 3027 | `	}` |
|      - | 3028 | `	/* Perform the requested operation */` |
| 120220 | 3029 | `	for(;;){` |
| 240441 | 3030 | `		if( !nMul ){` |
|  20209 | 3031 | `			break;` |
|      - | 3032 | `		}` |
|      - | 3033 | `		/* Append the copy */` |
| 220233 | 3034 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 220233 | 3035 | `		if( rc != PH7_OK ){` |
|      - | 3036 | `			/* Out of memory,break immediately */` |
|    ! 0 | 3037 | `			break;` |
|      - | 3038 | `		}` |
| 220233 | 3039 | `		nMul--;` |
|      1 | 3040 | `	}` |
|  20209 | 3041 | `	return PH7_OK;` |
|  10107 | 3042 |  |
|      - | 3043 | `/*` |
|      - | 3044 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3045 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3046 | ` * Parameters` |
|      - | 3047 | ` *  $string` |
|      - | 3048 | ` *   The input string.` |
|      - | 3049 | ` * $is_xhtml` |
|      - | 3050 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3051 | ` * Return` |
|      - | 3052 | ` *  The processed string.` |
|      - | 3053 | ` */` |
|      6 | 3054 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3055 |  |
|      - | 3056 | `	const char *zIn,*zCur,*zEnd;` |
|      7 | 3057 | `	int is_xhtml = 1; /* Default to XHTML-style '<br/>' like PHP */` |
|      - | 3058 | `	int nLen;` |
|      7 | 3059 | `	if( nArg < 1 ){` |
|      - | 3060 | `		/* Missing arguments,return the empty string */` |
|      3 | 3061 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3062 | `		return PH7_OK;` |
|      - | 3063 | `	}` |
|      - | 3064 | `	/* Extract the target string */` |
|      5 | 3065 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3066 | `	if( nLen < 1 ){` |
|      - | 3067 | `		/* Empty string,return null */` |
|    ! 0 | 3068 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3069 | `		return PH7_OK;` |
|      - | 3070 | `	}` |
|      5 | 3071 | `	if( nArg > 1 ){` |
|      3 | 3072 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3073 | `	}` |
|      5 | 3074 | `	zEnd = &zIn[nLen];` |
|      - | 3075 | `	/* Perform the requested operation */` |
|      4 | 3076 | `	for(;;){` |
|      9 | 3077 | `		zCur = zIn;` |
|      - | 3078 | `		/* Delimit the string */` |
|     21 | 3079 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3080 | `			zIn++;` |
|      1 | 3081 | `		}` |
|      9 | 3082 | `		if( zCur < zIn ){` |
|      - | 3083 | `			/* Output chunk verbatim */` |
|      9 | 3084 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3085 | `		}` |
|      9 | 3086 | `		if( zIn >= zEnd ){` |
|      - | 3087 | `			/* No more input to process */` |
|      5 | 3088 | `			break;` |
|      - | 3089 | `		}` |
|      - | 3090 | `		/* Output the HTML line break */` |
|      - | 3091 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br/>' (legacy without space), otherwise use '<br>' */` |
|      5 | 3092 | `		if( is_xhtml ){` |
|      3 | 3093 | `			ph7_result_string(pCtx,"<br/>",(int)sizeof("<br/>")-1);` |
|      2 | 3094 | `		}else{` |
|      3 | 3095 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3096 | `		}` |
|      5 | 3097 | `		zCur = zIn;` |
|      - | 3098 | `		/* Append trailing line */` |
|     11 | 3099 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3100 | `			zIn++;` |
|      1 | 3101 | `		}` |
|      5 | 3102 | `		if( zCur < zIn ){` |
|      - | 3103 | `			/* Output chunk verbatim */` |
|      5 | 3104 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 3105 | `		}` |
|      1 | 3106 | `	}` |
|      5 | 3107 | `	return PH7_OK;` |
|      4 | 3108 |  |
|      - | 3109 | `/*` |
|      - | 3110 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3111 | ` *  According to the PHP reference manual.` |
|      - | 3112 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3113 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3114 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 3115 | ` * This applies to both sprintf() and printf().` |
|      - | 3116 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 3117 | ` * or more of these elements, in order:` |
|      - | 3118 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 3119 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 3120 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 3121 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 3122 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 3123 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 3124 | ` *   it with a single quote ('). See the examples below.` |
|      - | 3125 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 3126 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 3127 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 3128 | ` *   should result in.` |
|      - | 3129 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 3130 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 3131 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 3132 | ` *   limit to the string.` |
|      - | 3133 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 3134 | ` *       % - a literal percent character. No argument is required.` |
|      - | 3135 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 3136 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 3137 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 3138 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 3139 | ` * 	     for the number of digits after the decimal point.` |
|      - | 3140 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 3141 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 3142 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 3143 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 3144 | ` *       g - shorter of %e and %f.` |
|      - | 3145 | ` *       G - shorter of %E and %f.` |
|      - | 3146 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 3147 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 3148 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 3149 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 3150 | ` */` |
|      - | 3151 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3152 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 3153 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 3154 | `/*` |
|      - | 3155 | `** Conversion types fall into various categories as defined by the` |
|      - | 3156 | `** following enumeration.` |
|      - | 3157 | `*/` |
|      - | 3158 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 3159 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 3160 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 3161 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 3162 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 3163 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 3164 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 3165 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 3166 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 3167 |  |
|      - | 3168 | `/*` |
|      - | 3169 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 3170 | `*/` |
|      - | 3171 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 3172 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 3173 | `/*` |
|      - | 3174 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 3175 | `** by an instance of the following structure` |
|      - | 3176 | `*/` |
|      - | 3177 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 3178 | `struct ph7_fmt_info` |
|      - | 3179 |  |
|      - | 3180 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 3181 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 3182 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 3183 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 3184 | `  char *charset; /* The character set for conversion */` |
|      - | 3185 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 3186 | `};` |
|      - | 3187 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3188 | `/*` |
|      - | 3189 | `** "*val" is a double such that 0.1 <= *val < 10.0` |
|      - | 3190 | `** Return the ascii code for the leading digit of *val, then` |
|      - | 3191 | `** multiply "*val" by 10.0 to renormalize.` |
|      - | 3192 | `**` |
|      - | 3193 | `** Example:` |
|      - | 3194 | `**     input:     *val = 3.14159` |
|      - | 3195 | `**     output:    *val = 1.4159    function return = '3'` |
|      - | 3196 | `**` |
|      - | 3197 | `** The counter *cnt is incremented each time.  After counter exceeds` |
|      - | 3198 | `** 16 (the number of significant digits in a 64-bit float) '0' is` |
|      - | 3199 | `** always returned.` |
|      - | 3200 | `*/` |
|    404 | 3201 | `static int vxGetdigit(sxlongreal *val,int *cnt)` |
|      1 | 3202 |  |
|      - | 3203 | `  sxlongreal d;` |
|      - | 3204 | `  int digit;` |
|      - | 3205 |  |
|    405 | 3206 | `  if( (*cnt)++ >= 16 ){` |
|    ! 0 | 3207 | `	  return '0';` |
|      - | 3208 | `  }` |
|    405 | 3209 | `  digit = (int)*val;` |
|    405 | 3210 | `  d = digit;` |
|    405 | 3211 | `   *val = (*val - d)*10.0;` |
|    405 | 3212 | `  return digit + '0' ;` |
|    203 | 3213 |  |
|      - | 3214 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      - | 3215 | `/*` |
|      - | 3216 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 3217 | ` * used conversion types first.` |
|      - | 3218 | ` */` |
|      - | 3219 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 3220 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 3221 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 3222 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 3223 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 3224 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 3225 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 3226 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 3227 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 3228 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3229 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3230 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 3231 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 3232 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 3233 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 3234 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 3235 | `};` |
|      - | 3236 | `/*` |
|      - | 3237 | ` * Format a given string.` |
|      - | 3238 | ` * The root program.  All variations call this core.` |
|      - | 3239 | ` * INPUTS:` |
|      - | 3240 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 3241 | ` *            1. A pointer to the call context.` |
|      - | 3242 | ` *            2. A pointer to the list of characters to be output` |
|      - | 3243 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 3244 | ` *            3. An integer number of characters to be output.` |
|      - | 3245 | ` *               (Note: This number might be zero.)` |
|      - | 3246 | ` *            4. Upper layer private data.` |
|      - | 3247 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 3248 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 3249 | ` */` |
|    120 | 3250 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 3251 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 3252 | `	ph7_context *pCtx,  /* call context */` |
|      - | 3253 | `	const char *zIn,    /* Format string */` |
|      - | 3254 | `	int nByte,          /* Format string length */` |
|      - | 3255 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 3256 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 3257 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 3258 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 3259 | `	)` |
|      1 | 3260 |  |
|    121 | 3261 | `	char spaces[] = "                                                  ";` |
|      - | 3262 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    121 | 3263 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 3264 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 3265 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 3266 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 3267 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 3268 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 3269 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 3270 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 3271 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 3272 | `	ph7_int64 iVal;` |
|      - | 3273 | `	int precision;           /* Precision of the current field */` |
|      - | 3274 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 3275 | `	int c,rc,n;` |
|      - | 3276 | `	int length;              /* Length of the field */` |
|      - | 3277 | `	int prefix;` |
|      - | 3278 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 3279 | `	int width;               /* Width of the current field */` |
|      - | 3280 | `	int idx;` |
|    121 | 3281 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 3282 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 3283 | `	/* Start the format process */` |
|    123 | 3284 | `	for(;;){` |
|    247 | 3285 | `		zCur = zIn;` |
|    697 | 3286 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|    451 | 3287 | `			zIn++;` |
|      1 | 3288 | `		}` |
|    247 | 3289 | `		if( zCur < zIn ){` |
|      - | 3290 | `			/* Consume chunk verbatim */` |
|     95 | 3291 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|     95 | 3292 | `			if( rc == SXERR_ABORT ){` |
|      - | 3293 | `				/* Callback request an operation abort */` |
|    ! 0 | 3294 | `				break;` |
|      - | 3295 | `			}` |
|     47 | 3296 | `		}` |
|    247 | 3297 | `		if( zIn >= zEnd ){` |
|      - | 3298 | `			/* No more input to process,break immediately */` |
|    119 | 3299 | `			break;` |
|      - | 3300 | `		}` |
|      - | 3301 | `		/* Find out what flags are present */` |
|    129 | 3302 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    128 | 3303 | `			flag_alternateform = flag_zeropad = 0;` |
|    129 | 3304 | `		zIn++; /* Jump the precent sign */` |
|     64 | 3305 | `		do{` |
|    157 | 3306 | `			c = zIn[0];` |
|    157 | 3307 | `			switch( c ){` |
|      9 | 3308 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      3 | 3309 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 3310 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|      9 | 3311 | `			case '#':   flag_alternateform = 1;   c = 0;   break;` |
|      5 | 3312 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 3313 | `			case '\'':` |
|    ! 0 | 3314 | `				zIn++;` |
|    ! 0 | 3315 | `				if( zIn < zEnd ){` |
|      - | 3316 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 3317 | `					c = zIn[0];` |
|    ! 0 | 3318 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3319 | `						spaces[idx] = (char)c;` |
|    ! 0 | 3320 | `					}` |
|    ! 0 | 3321 | `					c = 0;` |
|    ! 0 | 3322 | `				}` |
|    ! 0 | 3323 | `				break;` |
|    128 | 3324 | `			default:                                       break;` |
|      - | 3325 | `			}` |
|    157 | 3326 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 3327 | `		/* Get the field width */` |
|    129 | 3328 | `		width = 0;` |
|    223 | 3329 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     31 | 3330 | `			width = width*10 + (zIn[0] - '0');` |
|     31 | 3331 | `			zIn++;` |
|      1 | 3332 | `		}` |
|    129 | 3333 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 3334 | `			/* Position specifer */` |
|    ! 0 | 3335 | `			if( width > 0 ){` |
|    ! 0 | 3336 | `				n = width;` |
|    ! 0 | 3337 | `				if( vf && n > 0 ){` |
|    ! 0 | 3338 | `					n--;` |
|    ! 0 | 3339 | `				}` |
|    ! 0 | 3340 | `			}` |
|    ! 0 | 3341 | `			zIn++;` |
|    ! 0 | 3342 | `			width = 0;` |
|    ! 0 | 3343 | `			if( zIn < zEnd && zIn[0] == '0' ){` |
|    ! 0 | 3344 | `				flag_zeropad = 1;` |
|    ! 0 | 3345 | `				zIn++;` |
|    ! 0 | 3346 | `			}` |
|    ! 0 | 3347 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 3348 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 3349 | `				zIn++;` |
|    ! 0 | 3350 | `			}` |
|    ! 0 | 3351 | `		}` |
|    129 | 3352 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 3353 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 3354 | `		}` |
|      - | 3355 | `		/* Get the precision */` |
|    129 | 3356 | `		precision = -1;` |
|    129 | 3357 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     57 | 3358 | `			precision = 0;` |
|     57 | 3359 | `			zIn++;` |
|    145 | 3360 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     61 | 3361 | `				precision = precision*10 + (zIn[0] - '0');` |
|     61 | 3362 | `				zIn++;` |
|      1 | 3363 | `			}` |
|     28 | 3364 | `		}` |
|    129 | 3365 | `		if( zIn >= zEnd ){` |
|      - | 3366 | `			/* No more input */` |
|      3 | 3367 | `			break;` |
|      - | 3368 | `		}` |
|      - | 3369 | `		/* Fetch the info entry for the field */` |
|    127 | 3370 | `		pInfo = 0;` |
|    127 | 3371 | `		xtype = PH7_FMT_ERROR;` |
|    127 | 3372 | `		c = zIn[0];` |
|    127 | 3373 | `		zIn++; /* Jump the format specifer */` |
|    699 | 3374 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|    697 | 3375 | `			if( c==aFmt[idx].fmttype ){` |
|    125 | 3376 | `				pInfo = &aFmt[idx];` |
|    125 | 3377 | `				xtype = pInfo->type;` |
|    125 | 3378 | `				break;` |
|      - | 3379 | `			}` |
|    287 | 3380 | `		}` |
|    127 | 3381 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    127 | 3382 | `		length = 0;` |
|      - | 3383 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 3384 | `		 /*` |
|      - | 3385 | `		  ** At this point, variables are initialized as follows:` |
|      - | 3386 | `		  **` |
|      - | 3387 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 3388 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 3389 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 3390 | `		  **                               field width was negative.` |
|      - | 3391 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 3392 | `		  **                               the conversion character.` |
|      - | 3393 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 3394 | `		  **   width                       The specified field width.  This is` |
|      - | 3395 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 3396 | `		  **   precision                   The specified precision.  The default` |
|      - | 3397 | `		  **                               is -1.` |
|      - | 3398 | `		  */` |
|    127 | 3399 | `		switch(xtype){` |
|    ! 0 | 3400 | `		case PH7_FMT_PERCENT:` |
|      - | 3401 | `			/* A literal percent character */` |
|    ! 0 | 3402 | `			zWorker[0] = '%';` |
|    ! 0 | 3403 | `			length = (int)sizeof(char);` |
|    ! 0 | 3404 | `			break;` |
|      3 | 3405 | `		case PH7_FMT_CHARX:` |
|      - | 3406 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 3407 | `			 * with that ASCII value` |
|      - | 3408 | `			 */` |
|      7 | 3409 | `			pArg = NEXT_ARG;` |
|      7 | 3410 | `			if( pArg == 0 ){` |
|      3 | 3411 | `				c = 0;` |
|      2 | 3412 | `			}else{` |
|      5 | 3413 | `				c = ph7_value_to_int(pArg);` |
|      - | 3414 | `			}` |
|      - | 3415 | `			/* NUL byte is an acceptable value */` |
|      7 | 3416 | `			zWorker[0] = (char)c;` |
|      7 | 3417 | `			length = (int)sizeof(char);` |
|      7 | 3418 | `			break;` |
|     12 | 3419 | `		case PH7_FMT_STRING:` |
|      - | 3420 | `			/* the argument is treated as and presented as a string */` |
|     25 | 3421 | `			pArg = NEXT_ARG;` |
|     25 | 3422 | `			if( pArg == 0 ){` |
|    ! 0 | 3423 | `				length = 0;` |
|    ! 0 | 3424 | `			}else{` |
|     25 | 3425 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 3426 | `			}` |
|     25 | 3427 | `			if( length < 1 ){` |
|    ! 0 | 3428 | `				zBuf = " ";` |
|    ! 0 | 3429 | `				length = (int)sizeof(char);` |
|    ! 0 | 3430 | `			}` |
|     25 | 3431 | `			if( precision>=0 && precision<length ){` |
|      3 | 3432 | `				length = precision;` |
|      1 | 3433 | `			}` |
|     25 | 3434 | `			if( flag_zeropad ){` |
|      - | 3435 | `				/* zero-padding works on strings too */` |
|    ! 0 | 3436 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3437 | `					spaces[idx] = '0';` |
|    ! 0 | 3438 | `				}` |
|    ! 0 | 3439 | `			}` |
|     25 | 3440 | `			break;` |
|     20 | 3441 | `		case PH7_FMT_RADIX:` |
|     41 | 3442 | `			pArg = NEXT_ARG;` |
|     41 | 3443 | `			if( pArg == 0 ){` |
|    ! 0 | 3444 | `				iVal = 0;` |
|    ! 0 | 3445 | `			}else{` |
|     41 | 3446 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 3447 | `			}` |
|      - | 3448 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|     41 | 3449 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 3450 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 3451 | `			}` |
|      - | 3452 | `#if 1` |
|      - | 3453 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 3454 | `        ** I think this is stupid.*/` |
|     41 | 3455 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 3456 | `#else` |
|      - | 3457 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 3458 | `        ** but leave the prefix for hex.*/` |
|      - | 3459 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 3460 | `#endif` |
|     41 | 3461 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|     23 | 3462 | `          if( iVal<0 ){` |
|      3 | 3463 | `            iVal = -iVal;` |
|      - | 3464 | `			/* Ticket 1433-003 */` |
|      3 | 3465 | `			if( iVal < 0 ){` |
|      - | 3466 | `				/* Overflow */` |
|    ! 0 | 3467 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3468 | `			}` |
|      3 | 3469 | `            prefix = '-';` |
|     22 | 3470 | `          }else if( flag_plussign )  prefix = '+';` |
|     19 | 3471 | `          else if( flag_blanksign )  prefix = ' ';` |
|     17 | 3472 | `          else                       prefix = 0;` |
|     12 | 3473 | `        }else{` |
|     19 | 3474 | `			if( iVal<0 ){` |
|    ! 0 | 3475 | `				iVal = -iVal;` |
|      - | 3476 | `				/* Ticket 1433-003 */` |
|    ! 0 | 3477 | `				if( iVal < 0 ){` |
|      - | 3478 | `					/* Overflow */` |
|    ! 0 | 3479 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3480 | `				}` |
|    ! 0 | 3481 | `			}` |
|     19 | 3482 | `			prefix = 0;` |
|      - | 3483 | `		}` |
|     41 | 3484 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      3 | 3485 | `          precision = width-(prefix!=0);` |
|      1 | 3486 | `        }` |
|     41 | 3487 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 3488 | `        {` |
|      - | 3489 | `          register char *cset;      /* Use registers for speed */` |
|      - | 3490 | `          register int base;` |
|     41 | 3491 | `          cset = pInfo->charset;` |
|     41 | 3492 | `          base = pInfo->base;` |
|     20 | 3493 | `          do{                                           /* Convert to ascii */` |
|     79 | 3494 | `            *(--zBuf) = cset[iVal%base];` |
|     79 | 3495 | `            iVal = iVal/base;` |
|     79 | 3496 | `          }while( iVal>0 );` |
|      - | 3497 | `        }` |
|     41 | 3498 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     55 | 3499 | `        for(idx=precision-length; idx>0; idx--){` |
|     15 | 3500 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|      8 | 3501 | `        }` |
|     41 | 3502 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|     41 | 3503 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 3504 | `          char *pre, x;` |
|      9 | 3505 | `          pre = pInfo->prefix;` |
|      9 | 3506 | `          if( *zBuf!=pre[0] ){` |
|     23 | 3507 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|      4 | 3508 | `          }` |
|      4 | 3509 | `        }` |
|     41 | 3510 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     41 | 3511 | `		break;` |
|     27 | 3512 | `		case PH7_FMT_FLOAT:` |
|      - | 3513 | `		case PH7_FMT_EXP:` |
|      - | 3514 | `		case PH7_FMT_GENERIC:{` |
|      - | 3515 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3516 | `		long double realvalue;` |
|      - | 3517 | `		int  exp;                /* exponent of real numbers */` |
|      - | 3518 | `		double rounder;          /* Used for rounding floating point values */` |
|      - | 3519 | `		int flag_dp;            /* True if decimal point should be shown */` |
|      - | 3520 | `		int flag_rtz;           /* True if trailing zeros should be removed */` |
|      - | 3521 | `		int flag_exp;           /* True to force display of the exponent */` |
|      - | 3522 | `		int nsd;                 /* Number of significant digits returned */` |
|     55 | 3523 | `		pArg = NEXT_ARG;` |
|     55 | 3524 | `		if( pArg == 0 ){` |
|    ! 0 | 3525 | `			realvalue = 0;` |
|    ! 0 | 3526 | `		}else{` |
|     55 | 3527 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 3528 | `		}` |
|      - | 3529 | `		/* Special-case NaN and infinities since the normal formatting logic` |
|      - | 3530 | `		 * below assumes a finite positive realvalue. */` |
|     55 | 3531 | `		if( PH7_IS_NAN(realvalue) ){` |
|    ! 0 | 3532 | `			zBuf = "NAN";` |
|    ! 0 | 3533 | `			length = 3;` |
|    ! 0 | 3534 | `			break;` |
|      - | 3535 | `		}` |
|     55 | 3536 | `		if( PH7_IS_INF(realvalue) ){` |
|      - | 3537 | `			/* Infinity prints as INF or -INF depending on sign. */` |
|    ! 0 | 3538 | `			if( realvalue < 0.0 ){` |
|    ! 0 | 3539 | `				zBuf = "-INF";` |
|    ! 0 | 3540 | `				length = 4;` |
|    ! 0 | 3541 | `			}else{` |
|    ! 0 | 3542 | `				zBuf = "INF";` |
|    ! 0 | 3543 | `				length = 3;` |
|      - | 3544 | `			}` |
|    ! 0 | 3545 | `			break;` |
|      - | 3546 | `		}` |
|     55 | 3547 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|     55 | 3548 | `		if( precision>PH7_FMT_BUFSIZ-40) precision = PH7_FMT_BUFSIZ-40;` |
|     55 | 3549 | `        if( realvalue<0.0 ){` |
|    ! 0 | 3550 | `          realvalue = -realvalue;` |
|    ! 0 | 3551 | `          prefix = '-';` |
|    ! 0 | 3552 | `        }else{` |
|     55 | 3553 | `          if( flag_plussign )          prefix = '+';` |
|     55 | 3554 | `          else if( flag_blanksign )    prefix = ' ';` |
|     55 | 3555 | `          else                         prefix = 0;` |
|      - | 3556 | `        }` |
|     55 | 3557 | `        if( pInfo->type==PH7_FMT_GENERIC && precision>0 ) precision--;` |
|     55 | 3558 | `        rounder = 0.0;` |
|      - | 3559 | `#if 0` |
|      - | 3560 | `        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */` |
|      - | 3561 | `        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);` |
|      - | 3562 | `#else` |
|      - | 3563 | `        /* It makes more sense to use 0.5 */` |
|    387 | 3564 | `        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);` |
|      - | 3565 | `#endif` |
|     55 | 3566 | `        if( pInfo->type==PH7_FMT_FLOAT ) realvalue += rounder;` |
|      - | 3567 | `        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */` |
|     55 | 3568 | `        exp = 0;` |
|     55 | 3569 | `        if( realvalue>0.0 ){` |
|     59 | 3570 | `          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }` |
|     87 | 3571 | `          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }` |
|     59 | 3572 | `          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }` |
|     69 | 3573 | `          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }` |
|     55 | 3574 | `          if( exp>350 \|\| exp<-350 ){` |
|    ! 0 | 3575 | `            zBuf = "NaN";` |
|    ! 0 | 3576 | `            length = 3;` |
|    ! 0 | 3577 | `            break;` |
|      - | 3578 | `          }` |
|     27 | 3579 | `        }` |
|     55 | 3580 | `        zBuf = zWorker;` |
|      - | 3581 | `        /*` |
|      - | 3582 | `        ** If the field type is etGENERIC, then convert to either etEXP` |
|      - | 3583 | `        ** or etFLOAT, as appropriate.` |
|      - | 3584 | `        */` |
|     55 | 3585 | `        flag_exp = xtype==PH7_FMT_EXP;` |
|     55 | 3586 | `        if( xtype!=PH7_FMT_FLOAT ){` |
|    ! 0 | 3587 | `          realvalue += rounder;` |
|    ! 0 | 3588 | `          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }` |
|    ! 0 | 3589 | `        }` |
|     55 | 3590 | `        if( xtype==PH7_FMT_GENERIC ){` |
|    ! 0 | 3591 | `          flag_rtz = !flag_alternateform;` |
|    ! 0 | 3592 | `          if( exp<-4 \|\| exp>precision ){` |
|    ! 0 | 3593 | `            xtype = PH7_FMT_EXP;` |
|    ! 0 | 3594 | `          }else{` |
|    ! 0 | 3595 | `            precision = precision - exp;` |
|    ! 0 | 3596 | `            xtype = PH7_FMT_FLOAT;` |
|      - | 3597 | `          }` |
|    ! 0 | 3598 | `        }else{` |
|     55 | 3599 | `          flag_rtz = 0;` |
|      - | 3600 | `        }` |
|      - | 3601 | `        /*` |
|      - | 3602 | `        ** The "exp+precision" test causes output to be of type etEXP if` |
|      - | 3603 | `        ** the precision is too large to fit in buf[].` |
|      - | 3604 | `        */` |
|     55 | 3605 | `        nsd = 0;` |
|     55 | 3606 | `        if( xtype==PH7_FMT_FLOAT && exp+precision<PH7_FMT_BUFSIZ-30 ){` |
|     55 | 3607 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|     55 | 3608 | `          if( prefix ) *(zBuf++) = (char)prefix;         /* Sign */` |
|     55 | 3609 | `          if( exp<0 )  *(zBuf++) = '0';            /* Digits before "." */` |
|    145 | 3610 | `          else for(; exp>=0; exp--) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     55 | 3611 | `          if( flag_dp ) *(zBuf++) = '.';           /* The decimal point */` |
|     87 | 3612 | `          for(exp++; exp<0 && precision>0; precision--, exp++){` |
|     33 | 3613 | `            *(zBuf++) = '0';` |
|     17 | 3614 | `          }` |
|    355 | 3615 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     55 | 3616 | `          *(zBuf--) = 0;                           /* Null terminate */` |
|     55 | 3617 | `          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */` |
|    ! 0 | 3618 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 3619 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 3620 | `          }` |
|     55 | 3621 | `          zBuf++;                            /* point to next free slot */` |
|     28 | 3622 | `        }else{    /* etEXP or etGENERIC */` |
|    ! 0 | 3623 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|    ! 0 | 3624 | `          if( prefix ) *(zBuf++) = (char)prefix;   /* Sign */` |
|    ! 0 | 3625 | `          *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);  /* First digit */` |
|    ! 0 | 3626 | `          if( flag_dp ) *(zBuf++) = '.';     /* Decimal point */` |
|    ! 0 | 3627 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|    ! 0 | 3628 | `          zBuf--;                            /* point to last digit */` |
|    ! 0 | 3629 | `          if( flag_rtz && flag_dp ){          /* Remove tail zeros */` |
|    ! 0 | 3630 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 3631 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 3632 | `          }` |
|    ! 0 | 3633 | `          zBuf++;                            /* point to next free slot */` |
|    ! 0 | 3634 | `          if( exp \|\| flag_exp ){` |
|    ! 0 | 3635 | `            *(zBuf++) = pInfo->charset[0];` |
|    ! 0 | 3636 | `            if( exp<0 ){ *(zBuf++) = '-'; exp = -exp; } /* sign of exp */` |
|    ! 0 | 3637 | `            else       { *(zBuf++) = '+'; }` |
|    ! 0 | 3638 | `            if( exp>=100 ){` |
|    ! 0 | 3639 | `              *(zBuf++) = (char)((exp/100)+'0');                /* 100's digit */` |
|    ! 0 | 3640 | `              exp %= 100;` |
|    ! 0 | 3641 | `            }` |
|    ! 0 | 3642 | `            *(zBuf++) = (char)(exp/10+'0');                     /* 10's digit */` |
|    ! 0 | 3643 | `            *(zBuf++) = (char)(exp%10+'0');                     /* 1's digit */` |
|    ! 0 | 3644 | `          }` |
|      - | 3645 | `        }` |
|      - | 3646 | `        /* The converted number is in buf[] and zero terminated.Output it.` |
|      - | 3647 | `        ** Note that the number is in the usual order, not reversed as with` |
|      - | 3648 | `        ** integer conversions.*/` |
|     55 | 3649 | `        length = (int)(zBuf-zWorker);` |
|     55 | 3650 | `        zBuf = zWorker;` |
|      - | 3651 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 3652 | `        ** set and we are not left justified */` |
|     55 | 3653 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 3654 | `          int i;` |
|      3 | 3655 | `          int nPad = width - length;` |
|     13 | 3656 | `          for(i=width; i>=nPad; i--){` |
|     11 | 3657 | `            zBuf[i] = zBuf[i-nPad];` |
|      6 | 3658 | `          }` |
|      3 | 3659 | `          i = prefix!=0;` |
|      5 | 3660 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      3 | 3661 | `          length = width;` |
|      1 | 3662 | `        }` |
|      - | 3663 | `#else` |
|      - | 3664 | `         zBuf = " ";` |
|      - | 3665 | `		 length = (int)sizeof(char);` |
|      - | 3666 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|     55 | 3667 | `		 break;` |
|      - | 3668 | `							 }` |
|      1 | 3669 | `		default:` |
|      - | 3670 | `			/* Invalid format specifer */` |
|      3 | 3671 | `			zWorker[0] = '?';` |
|      3 | 3672 | `			length = (int)sizeof(char);` |
|      2 | 3673 | `			break;` |
|      - | 3674 | `		}` |
|      - | 3675 | `		 /*` |
|      - | 3676 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 3677 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 3678 | `		 ** the output.` |
|      - | 3679 | `		 */` |
|    127 | 3680 | `    if( !flag_leftjustify ){` |
|      - | 3681 | `      register int nspace;` |
|    119 | 3682 | `      nspace = width-length;` |
|    119 | 3683 | `      if( nspace>0 ){` |
|      5 | 3684 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3685 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3686 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3687 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3688 | `			}` |
|    ! 0 | 3689 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3690 | `        }` |
|      5 | 3691 | `        if( nspace>0 ){` |
|      5 | 3692 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      5 | 3693 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3694 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3695 | `			}` |
|      2 | 3696 | `		}` |
|      2 | 3697 | `      }` |
|     59 | 3698 | `    }` |
|    127 | 3699 | `    if( length>0 ){` |
|    127 | 3700 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    127 | 3701 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3702 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3703 | `		}` |
|     63 | 3704 | `    }` |
|    127 | 3705 | `    if( flag_leftjustify ){` |
|      - | 3706 | `      register int nspace;` |
|      9 | 3707 | `      nspace = width-length;` |
|      9 | 3708 | `      if( nspace>0 ){` |
|      9 | 3709 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3710 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3711 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3712 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3713 | `			}` |
|    ! 0 | 3714 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3715 | `        }` |
|      9 | 3716 | `        if( nspace>0 ){` |
|      9 | 3717 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      9 | 3718 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3719 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3720 | `			}` |
|      4 | 3721 | `		}` |
|      4 | 3722 | `      }` |
|      4 | 3723 | `    }` |
|      1 | 3724 | ` }/* for(;;) */` |
|    121 | 3725 | `	return SXRET_OK;` |
|     61 | 3726 |  |
|      - | 3727 | `/*` |
|      - | 3728 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 3729 | ` */` |
|     84 | 3730 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 3731 |  |
|      - | 3732 | `	/* Consume directly */` |
|     85 | 3733 | `	ph7_result_string(pCtx,zInput,nLen);` |
|     42 | 3734 | `	SXUNUSED(pUserData); /* cc warning */` |
|     85 | 3735 | `	return PH7_OK;` |
|      1 | 3736 |  |
|      - | 3737 | `/*` |
|      - | 3738 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 3739 | ` *  Return a formatted string.` |
|      - | 3740 | ` * Parameters` |
|      - | 3741 | ` *  $format` |
|      - | 3742 | ` *    The format string (see block comment above)` |
|      - | 3743 | ` * Return` |
|      - | 3744 | ` *  A string produced according to the formatting string format.` |
|      - | 3745 | ` */` |
|     56 | 3746 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3747 |  |
|      - | 3748 | `	const char *zFormat;` |
|      - | 3749 | `	int nLen;` |
|     57 | 3750 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3751 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 3752 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3753 | `		return PH7_OK;` |
|      - | 3754 | `	}` |
|      - | 3755 | `	/* Extract the string format */` |
|     55 | 3756 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     55 | 3757 | `	if( nLen < 1 ){` |
|      - | 3758 | `		/* Empty string */` |
|    ! 0 | 3759 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3760 | `		return PH7_OK;` |
|      - | 3761 | `	}` |
|      - | 3762 | `	/* Format the string */` |
|     55 | 3763 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,0,FALSE);` |
|     55 | 3764 | `	return PH7_OK;` |
|     29 | 3765 |  |
|      - | 3766 | `/*` |
|      - | 3767 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 3768 | ` */` |
|    110 | 3769 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 3770 |  |
|    111 | 3771 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 3772 | `	/* Call the VM output consumer directly */` |
|    111 | 3773 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 3774 | `	/* Increment counter */` |
|    111 | 3775 | `	*pCounter += nLen;` |
|    111 | 3776 | `	return PH7_OK;` |
|      1 | 3777 |  |
|      - | 3778 | `/*` |
|      - | 3779 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 3780 | ` *  Output a formatted string.` |
|      - | 3781 | ` * Parameters` |
|      - | 3782 | ` *  $format` |
|      - | 3783 | ` *   See sprintf() for a description of format.` |
|      - | 3784 | ` * Return` |
|      - | 3785 | ` *  The length of the outputted string.` |
|      - | 3786 | ` */` |
|     42 | 3787 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3788 |  |
|     43 | 3789 | `	ph7_int64 nCounter = 0;` |
|      - | 3790 | `	const char *zFormat;` |
|      - | 3791 | `	int nLen;` |
|     43 | 3792 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3793 | `		/* Missing/Invalid arguments,return 0 */` |
|      3 | 3794 | `		ph7_result_int(pCtx,0);` |
|      3 | 3795 | `		return PH7_OK;` |
|      - | 3796 | `	}` |
|      - | 3797 | `	/* Extract the string format */` |
|     41 | 3798 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 3799 | `	if( nLen < 1 ){` |
|      - | 3800 | `		/* Empty string */` |
|    ! 0 | 3801 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3802 | `		return PH7_OK;` |
|      - | 3803 | `	}` |
|      - | 3804 | `	/* Format the string */` |
|     41 | 3805 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 3806 | `	/* Return the length of the outputted string */` |
|     41 | 3807 | `	ph7_result_int64(pCtx,nCounter);` |
|     41 | 3808 | `	return PH7_OK;` |
|     22 | 3809 |  |
|      - | 3810 | `/*` |
|      - | 3811 | ` * int vprintf(string $format,array $args)` |
|      - | 3812 | ` *  Output a formatted string.` |
|      - | 3813 | ` * Parameters` |
|      - | 3814 | ` *  $format` |
|      - | 3815 | ` *   See sprintf() for a description of format.` |
|      - | 3816 | ` * Return` |
|      - | 3817 | ` *  The length of the outputted string.` |
|      - | 3818 | ` */` |
|      2 | 3819 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3820 |  |
|      3 | 3821 | `	ph7_int64 nCounter = 0;` |
|      - | 3822 | `	const char *zFormat;` |
|      - | 3823 | `	ph7_hashmap *pMap;` |
|      - | 3824 | `	SySet sArg;` |
|      - | 3825 | `	int nLen,n;` |
|      3 | 3826 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 3827 | `		/* Missing/Invalid arguments,return 0 */` |
|    ! 0 | 3828 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3829 | `		return PH7_OK;` |
|      - | 3830 | `	}` |
|      - | 3831 | `	/* Extract the string format */` |
|      3 | 3832 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3833 | `	if( nLen < 1 ){` |
|      - | 3834 | `		/* Empty string */` |
|    ! 0 | 3835 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3836 | `		return PH7_OK;` |
|      - | 3837 | `	}` |
|      - | 3838 | `	/* Point to the hashmap */` |
|      3 | 3839 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 3840 | `	/* Extract arguments from the hashmap */` |
|      3 | 3841 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 3842 | `	/* Format the string */` |
|      3 | 3843 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 3844 | `	/* Return the length of the outputted string */` |
|      3 | 3845 | `	ph7_result_int64(pCtx,nCounter);` |
|      - | 3846 | `	/* Release the container */` |
|      3 | 3847 | `	SySetRelease(&sArg);` |
|      3 | 3848 | `	return PH7_OK;` |
|      2 | 3849 |  |
|      - | 3850 | `/*` |
|      - | 3851 | ` * int vsprintf(string $format,array $args)` |
|      - | 3852 | ` *  Output a formatted string.` |
|      - | 3853 | ` * Parameters` |
|      - | 3854 | ` *  $format` |
|      - | 3855 | ` *   See sprintf() for a description of format.` |
|      - | 3856 | ` * Return` |
|      - | 3857 | ` *  A string produced according to the formatting string format.` |
|      - | 3858 | ` */` |
|     10 | 3859 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3860 |  |
|      - | 3861 | `	const char *zFormat;` |
|      - | 3862 | `	ph7_hashmap *pMap;` |
|      - | 3863 | `	SySet sArg;` |
|      - | 3864 | `	int nLen,n;` |
|     11 | 3865 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 3866 | `		/* Missing/Invalid arguments,return the empty string */` |
|      5 | 3867 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 3868 | `		return PH7_OK;` |
|      - | 3869 | `	}` |
|      - | 3870 | `	/* Extract the string format */` |
|      7 | 3871 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 3872 | `	if( nLen < 1 ){` |
|      - | 3873 | `		/* Empty string */` |
|    ! 0 | 3874 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3875 | `		return PH7_OK;` |
|      - | 3876 | `	}` |
|      - | 3877 | `	/* Point to hashmap */` |
|      7 | 3878 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 3879 | `	/* Extract arguments from the hashmap */` |
|      7 | 3880 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 3881 | `	/* Format the string */` |
|      7 | 3882 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),0,TRUE);` |
|      - | 3883 | `	/* Release the container */` |
|      7 | 3884 | `	SySetRelease(&sArg);` |
|      7 | 3885 | `	return PH7_OK;` |
|      6 | 3886 |  |
|      - | 3887 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 3888 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 3889 | `/*` |
|      - | 3890 | ` * Symisc eXtension.` |
|      - | 3891 | ` * string size_format(int64 $size)` |
|      - | 3892 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 3893 | ` *  Example:` |
|      - | 3894 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 3895 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 3896 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 3897 | ` * Parameter` |
|      - | 3898 | ` *  $size` |
|      - | 3899 | ` *    Entity size in bytes.` |
|      - | 3900 | ` * Return` |
|      - | 3901 | ` *   Formatted string representation of the given size.` |
|      - | 3902 | ` */` |
|     24 | 3903 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3904 |  |
|      - | 3905 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 3906 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 3907 | `	sxi32 nRest,i_32;` |
|      - | 3908 | `	ph7_int64 iSize;` |
|     25 | 3909 | `	int c = -1; /* index in zUnit[] */` |
|      - | 3910 |  |
|     25 | 3911 | `	if( nArg < 1 ){` |
|      - | 3912 | `		/* Missing argument,return the empty string */` |
|      3 | 3913 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3914 | `		return PH7_OK;` |
|      - | 3915 | `	}` |
|      - | 3916 | `	/* Extract the given size */` |
|     23 | 3917 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 3918 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 3919 | `		/* Don't bother formatting,return immediately */` |
|      5 | 3920 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 3921 | `		return PH7_OK;` |
|      - | 3922 | `	}` |
|     19 | 3923 | `	for(;;){` |
|     39 | 3924 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 3925 | `		iSize >>= 10;` |
|     39 | 3926 | `		c++;` |
|     39 | 3927 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 3928 | `			break;` |
|      - | 3929 | `		}` |
|      1 | 3930 | `	}` |
|     19 | 3931 | `	nRest /= 100;` |
|     19 | 3932 | `	if( nRest > 9 ){` |
|    ! 0 | 3933 | `		nRest = 9;` |
|    ! 0 | 3934 | `	}` |
|     19 | 3935 | `	if( iSize > 999 ){` |
|    ! 0 | 3936 | `		c++;` |
|    ! 0 | 3937 | `		nRest = 9;` |
|    ! 0 | 3938 | `		iSize = 0;` |
|    ! 0 | 3939 | `	}` |
|     19 | 3940 | `	i_32 = (sxi32)iSize;` |
|      - | 3941 | `	/* Format */` |
|     19 | 3942 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 3943 | `	return PH7_OK;` |
|     13 | 3944 |  |
|      - | 3945 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 3946 | `/*` |
|      - | 3947 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 3948 | ` *   Calculate the md5 hash of a string.` |
|      - | 3949 | ` * Parameter` |
|      - | 3950 | ` *  $str` |
|      - | 3951 | ` *   Input string` |
|      - | 3952 | ` * $raw_output` |
|      - | 3953 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 3954 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 3955 | ` * Return` |
|      - | 3956 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 3957 | ` */` |
|     10 | 3958 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3959 |  |
|      - | 3960 | `	unsigned char zDigest[16];` |
|     11 | 3961 | `	int raw_output = FALSE;` |
|      - | 3962 | `	const void *pIn;` |
|      - | 3963 | `	int nLen;` |
|     11 | 3964 | `	if( nArg < 1 ){` |
|      - | 3965 | `		/* Missing arguments,return the empty string */` |
|      3 | 3966 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3967 | `		return PH7_OK;` |
|      - | 3968 | `	}` |
|      - | 3969 | `	/* Extract the input string */` |
|      9 | 3970 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 3971 | `	if( nLen < 1 ){` |
|      - | 3972 | `		/* Empty string */` |
|    ! 0 | 3973 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3974 | `		return PH7_OK;` |
|      - | 3975 | `	}` |
|      9 | 3976 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      3 | 3977 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 | 3978 | `	}` |
|      - | 3979 | `	/* Compute the MD5 digest */` |
|      9 | 3980 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|      9 | 3981 | `	if( raw_output ){` |
|      - | 3982 | `		/* Output raw digest */` |
|      3 | 3983 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      2 | 3984 | `	}else{` |
|      - | 3985 | `		/* Perform a binary to hex conversion */` |
|      7 | 3986 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 3987 | `	}` |
|      9 | 3988 | `	return PH7_OK;` |
|      6 | 3989 |  |
|      - | 3990 | `/*` |
|      - | 3991 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 3992 | ` *   Calculate the sha1 hash of a string.` |
|      - | 3993 | ` * Parameter` |
|      - | 3994 | ` *  $str` |
|      - | 3995 | ` *   Input string` |
|      - | 3996 | ` * $raw_output` |
|      - | 3997 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 3998 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 3999 | ` * Return` |
|      - | 4000 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 4001 | ` */` |
|      8 | 4002 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4003 |  |
|      - | 4004 | `	unsigned char zDigest[20];` |
|      9 | 4005 | `	int raw_output = FALSE;` |
|      - | 4006 | `	const void *pIn;` |
|      - | 4007 | `	int nLen;` |
|      9 | 4008 | `	if( nArg < 1 ){` |
|      - | 4009 | `		/* Missing arguments,return the empty string */` |
|      3 | 4010 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4011 | `		return PH7_OK;` |
|      - | 4012 | `	}` |
|      - | 4013 | `	/* Extract the input string */` |
|      7 | 4014 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4015 | `	if( nLen < 1 ){` |
|      - | 4016 | `		/* Empty string */` |
|    ! 0 | 4017 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4018 | `		return PH7_OK;` |
|      - | 4019 | `	}` |
|      7 | 4020 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      3 | 4021 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 | 4022 | `	}` |
|      - | 4023 | `	/* Compute the SHA1 digest */` |
|      7 | 4024 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|      7 | 4025 | `	if( raw_output ){` |
|      - | 4026 | `		/* Output raw digest */` |
|      3 | 4027 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      2 | 4028 | `	}else{` |
|      - | 4029 | `		/* Perform a binary to hex conversion */` |
|      5 | 4030 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4031 | `	}` |
|      7 | 4032 | `	return PH7_OK;` |
|      5 | 4033 |  |
|      - | 4034 | `/*` |
|      - | 4035 | ` * int64 crc32(string $str)` |
|      - | 4036 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 4037 | ` * Parameter` |
|      - | 4038 | ` *  $str` |
|      - | 4039 | ` *   Input string` |
|      - | 4040 | ` * Return` |
|      - | 4041 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 4042 | ` */` |
|      4 | 4043 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4044 |  |
|      - | 4045 | `	const void *pIn;` |
|      - | 4046 | `	sxu32 nCRC;` |
|      - | 4047 | `	int nLen;` |
|      5 | 4048 | `	if( nArg < 1 ){` |
|      - | 4049 | `		/* Missing arguments,return 0 */` |
|      3 | 4050 | `		ph7_result_int(pCtx,0);` |
|      3 | 4051 | `		return PH7_OK;` |
|      - | 4052 | `	}` |
|      - | 4053 | `	/* Extract the input string */` |
|      3 | 4054 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4055 | `	if( nLen < 1 ){` |
|      - | 4056 | `		/* Empty string */` |
|    ! 0 | 4057 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4058 | `		return PH7_OK;` |
|      - | 4059 | `	}` |
|      - | 4060 | `	/* Calculate the sum */` |
|      3 | 4061 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 4062 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 4063 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 4064 | `	return PH7_OK;` |
|      3 | 4065 |  |
|      - | 4066 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 4067 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4068 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 4069 | `/*` |
|      - | 4070 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 4071 |  |
|      - | 4072 | ` */` |
|      4 | 4073 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 4074 | `	const char *zInput, /* Raw input */` |
|      - | 4075 | `	int nByte,  /* Input length */` |
|      - | 4076 | `	int delim,  /* Delimiter */` |
|      - | 4077 | `	int encl,   /* Enclosure */` |
|      - | 4078 | `	int escape,  /* Escape character */` |
|      - | 4079 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 4080 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 4081 | `	)` |
|      1 | 4082 |  |
|      5 | 4083 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 4084 | `	const char *zIn = zInput;` |
|      - | 4085 | `	const char *zPtr;` |
|      - | 4086 | `	int isEnc;` |
|      - | 4087 | `	/* Start processing */` |
|      8 | 4088 | `	for(;;){` |
|     17 | 4089 | `		if( zIn >= zEnd ){` |
|      - | 4090 | `			/* No more input to process */` |
|      5 | 4091 | `			break;` |
|      - | 4092 | `		}` |
|     13 | 4093 | `		isEnc = 0;` |
|     13 | 4094 | `		zPtr = zIn;` |
|      - | 4095 | `		/* Find the first delimiter */` |
|     27 | 4096 | `		while( zIn < zEnd ){` |
|     23 | 4097 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 4098 | `				/* Delimiter found,break imediately */` |
|      5 | 4099 | `				break;` |
|     15 | 4100 | `			}else if( zIn[0] == encl ){` |
|      - | 4101 | `				/* Inside enclosure? */` |
|    ! 0 | 4102 | `				isEnc = !isEnc;` |
|     15 | 4103 | `			}else if( zIn[0] == escape ){` |
|      - | 4104 | `				/* Escape sequence */` |
|    ! 0 | 4105 | `				zIn++;` |
|    ! 0 | 4106 | `			}` |
|      - | 4107 | `			/* Advance the cursor */` |
|     15 | 4108 | `			zIn++;` |
|      1 | 4109 | `		}` |
|     13 | 4110 | `		if( zIn > zPtr ){` |
|     13 | 4111 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 4112 | `			sxi32 rc;` |
|      - | 4113 | `			/* Invoke the supllied callback */` |
|     13 | 4114 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 4115 | `				zPtr++;` |
|    ! 0 | 4116 | `				nByteChunk-=2;` |
|    ! 0 | 4117 | `			}` |
|     13 | 4118 | `			if( nByteChunk > 0 ){` |
|     13 | 4119 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 4120 | `				if( rc == SXERR_ABORT ){` |
|      - | 4121 | `					/* User callback request an operation abort */` |
|    ! 0 | 4122 | `					break;` |
|      - | 4123 | `				}` |
|      6 | 4124 | `			}` |
|      6 | 4125 | `		}` |
|      - | 4126 | `		/* Ignore trailing delimiter */` |
|     21 | 4127 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 4128 | `			zIn++;` |
|      1 | 4129 | `		}` |
|      1 | 4130 | `	}` |
|      5 | 4131 | `	return SXRET_OK;` |
|      1 | 4132 |  |
|      - | 4133 | `/*` |
|      - | 4134 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 4135 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 4136 | ` * argument to this callback.` |
|      - | 4137 | ` */` |
|     12 | 4138 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 4139 |  |
|     13 | 4140 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 4141 | `	ph7_value sEntry;` |
|      - | 4142 | `	SyString sToken;` |
|      - | 4143 | `	/* Insert the token in the given array */` |
|     13 | 4144 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 4145 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 4146 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 4147 | `	if( sToken.nByte < 1){` |
|    ! 0 | 4148 | `		return SXRET_OK;` |
|      - | 4149 | `	}` |
|     13 | 4150 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 4151 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 4152 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 4153 | `	return SXRET_OK;` |
|      7 | 4154 |  |
|      - | 4155 | `/*` |
|      - | 4156 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 4157 | ` *  Parse a CSV string into an array.` |
|      - | 4158 | ` * Parameters` |
|      - | 4159 | ` *  $input` |
|      - | 4160 | ` *   The string to parse.` |
|      - | 4161 | ` *  $delimiter` |
|      - | 4162 | ` *   Set the field delimiter (one character only).` |
|      - | 4163 | ` *  $enclosure` |
|      - | 4164 | ` *   Set the field enclosure character (one character only).` |
|      - | 4165 | ` *  $escape` |
|      - | 4166 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 4167 | ` * Return` |
|      - | 4168 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 4169 | ` */` |
|      4 | 4170 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4171 |  |
|      - | 4172 | `	const char *zInput,*zPtr;` |
|      - | 4173 | `	ph7_value *pArray;` |
|      5 | 4174 | `	int delim  = ',';   /* Delimiter */` |
|      5 | 4175 | `	int encl   = '"' ;  /* Enclosure */` |
|      5 | 4176 | `	int escape = '\\';  /* Escape character */` |
|      - | 4177 | `	int nLen;` |
|      5 | 4178 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4179 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 4180 | `		ph7_result_null(pCtx);` |
|      3 | 4181 | `		return PH7_OK;` |
|      - | 4182 | `	}` |
|      - | 4183 | `	/* Extract the raw input */` |
|      3 | 4184 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4185 | `	if( nArg > 1 ){` |
|      - | 4186 | `		int i;` |
|      3 | 4187 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 4188 | `			/* Extract the delimiter */` |
|      3 | 4189 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 4190 | `			if( i > 0 ){` |
|      3 | 4191 | `				delim = zPtr[0];` |
|      1 | 4192 | `			}` |
|      1 | 4193 | `		}` |
|      3 | 4194 | `		if( nArg > 2 ){` |
|      3 | 4195 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 4196 | `				/* Extract the enclosure */` |
|      3 | 4197 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 4198 | `				if( i > 0 ){` |
|      3 | 4199 | `					encl = zPtr[0];` |
|      1 | 4200 | `				}` |
|      1 | 4201 | `			}` |
|      3 | 4202 | `			if( nArg > 3 ){` |
|      3 | 4203 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 4204 | `					/* Extract the escape character */` |
|      3 | 4205 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 4206 | `					if( i > 0 ){` |
|      3 | 4207 | `						escape = zPtr[0];` |
|      1 | 4208 | `					}` |
|      1 | 4209 | `				}` |
|      1 | 4210 | `			}` |
|      1 | 4211 | `		}` |
|      1 | 4212 | `	}` |
|      - | 4213 | `	/* Create our array */` |
|      3 | 4214 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4215 | `	if( pArray == 0 ){` |
|    ! 0 | 4216 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4217 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4218 | `		return PH7_OK;` |
|      - | 4219 | `	}` |
|      - | 4220 | `	/* Parse the raw input */` |
|      3 | 4221 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 4222 | `	/* Return the freshly created array */` |
|      3 | 4223 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4224 | `	return PH7_OK;` |
|      3 | 4225 |  |
|      - | 4226 | `/*` |
|      - | 4227 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 4228 | ` * container.` |
|      - | 4229 | ` * Refer to [strip_tags()].` |
|      - | 4230 | ` */` |
|     10 | 4231 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 4232 |  |
|     11 | 4233 | `	const char *zEnd = &zTag[nByte];` |
|      - | 4234 | `	const char *zPtr;` |
|      - | 4235 | `	SyString sEntry;` |
|      - | 4236 | `	/* Strip tags */` |
|     10 | 4237 | `	for(;;){` |
|     45 | 4238 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 4239 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 4240 | `				zTag++;` |
|      1 | 4241 | `		}` |
|     21 | 4242 | `		if( zTag >= zEnd ){` |
|     11 | 4243 | `			break;` |
|      - | 4244 | `		}` |
|     11 | 4245 | `		zPtr = zTag;` |
|      - | 4246 | `		/* Delimit the tag */` |
|     25 | 4247 | `		while(zTag < zEnd ){` |
|     25 | 4248 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 4249 | `				/* UTF-8 stream */` |
|      3 | 4250 | `				zTag++;` |
|      5 | 4251 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 4252 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 4253 | `				break;` |
|    ! 0 | 4254 | `			}else{` |
|     13 | 4255 | `				zTag++;` |
|      - | 4256 | `			}` |
|      1 | 4257 | `		}` |
|     11 | 4258 | `		if( zTag > zPtr ){` |
|      - | 4259 | `			/* Perform the insertion */` |
|     11 | 4260 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 4261 | `			SyStringFullTrim(&sEntry);` |
|     11 | 4262 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 4263 | `		}` |
|      - | 4264 | `		/* Jump the trailing '>' */` |
|     11 | 4265 | `		zTag++;` |
|      1 | 4266 | `	}` |
|     11 | 4267 | `	return SXRET_OK;` |
|      1 | 4268 |  |
|      - | 4269 | `/*` |
|      - | 4270 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 4271 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 4272 | ` * Refer to [strip_tags()].` |
|      - | 4273 | ` */` |
|     36 | 4274 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 4275 |  |
|     37 | 4276 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 4277 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 4278 | `		SyString sTag;` |
|     85 | 4279 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 4280 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 4281 | `			zTag++;` |
|      1 | 4282 | `		}` |
|      - | 4283 | `		/* Delimit the tag */` |
|     25 | 4284 | `		zCur = zTag;` |
|     77 | 4285 | `		while(zTag < zEnd ){` |
|     77 | 4286 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 4287 | `				/* UTF-8 stream */` |
|      5 | 4288 | `				zTag++;` |
|      9 | 4289 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 4290 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 4291 | `				break;` |
|    ! 0 | 4292 | `			}else{` |
|     49 | 4293 | `				zTag++;` |
|      - | 4294 | `			}` |
|      1 | 4295 | `		}` |
|     25 | 4296 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 4297 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 4298 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 4299 | `		if( sTag.nByte > 0 ){` |
|      - | 4300 | `			SyString *aEntry,*pEntry;` |
|      - | 4301 | `			sxi32 rc;` |
|      - | 4302 | `			sxu32 n;` |
|      - | 4303 | `			/* Perform the lookup */` |
|     25 | 4304 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 4305 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 4306 | `				pEntry = &aEntry[n];` |
|      - | 4307 | `				/* Do the comparison */` |
|     25 | 4308 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 4309 | `				if( !rc ){` |
|     21 | 4310 | `					return SXRET_OK;` |
|      - | 4311 | `				}` |
|      3 | 4312 | `			}` |
|      2 | 4313 | `		}` |
|      2 | 4314 | `	}` |
|      - | 4315 | `	/* No such tag */` |
|     17 | 4316 | `	return SXERR_NOTFOUND;` |
|     19 | 4317 |  |
|      - | 4318 | `/*` |
|      - | 4319 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 4320 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 4321 | ` * Refer to [strip_tags()].` |
|      - | 4322 | ` */` |
|     16 | 4323 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 4324 |  |
|     17 | 4325 | `	const char *zEnd = &zIn[nByte];` |
|      - | 4326 | `	const char *zPtr,*zTag;` |
|      - | 4327 | `	SySet sSet;` |
|      - | 4328 | `	/* initialize the set of allowed tags */` |
|     17 | 4329 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 4330 | `	if( nTaglen > 0 ){` |
|      - | 4331 | `		/* Set of allowed tags */` |
|     11 | 4332 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 4333 | `	}` |
|      - | 4334 | `	/* Set the empty string */` |
|     17 | 4335 | `	ph7_result_string(pCtx,"",0);` |
|      - | 4336 | `	/* Start processing */` |
|     26 | 4337 | `	for(;;){` |
|     53 | 4338 | `		if(zIn >= zEnd){` |
|      - | 4339 | `			/* No more input to process */` |
|     15 | 4340 | `			break;` |
|      - | 4341 | `		}` |
|     39 | 4342 | `		zPtr = zIn;` |
|      - | 4343 | `		/* Find a tag */` |
|    133 | 4344 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 4345 | `			zIn++;` |
|      1 | 4346 | `		}` |
|     39 | 4347 | `		if( zIn > zPtr ){` |
|      - | 4348 | `			/* Consume raw input */` |
|     21 | 4349 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 4350 | `		}` |
|      - | 4351 | `		/* Ignore trailing null bytes */` |
|     39 | 4352 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 4353 | `			zIn++;` |
|    ! 0 | 4354 | `		}` |
|     39 | 4355 | `		if(zIn >= zEnd){` |
|      - | 4356 | `			/* No more input to process */` |
|      3 | 4357 | `			break;` |
|      - | 4358 | `		}` |
|     37 | 4359 | `		if( zIn[0] == '<' ){` |
|      - | 4360 | `			sxi32 rc;` |
|     37 | 4361 | `			zTag = zIn++;` |
|      - | 4362 | `			/* Delimit the tag */` |
|    127 | 4363 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 4364 | `				zIn++;` |
|      1 | 4365 | `			}` |
|     37 | 4366 | `			if( zIn < zEnd ){` |
|     37 | 4367 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 4368 | `			}` |
|      - | 4369 | `			/* Query the set */` |
|     37 | 4370 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 4371 | `			if( rc == SXRET_OK ){` |
|      - | 4372 | `				/* Keep the tag */` |
|     21 | 4373 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 4374 | `			}` |
|     18 | 4375 | `		}` |
|      1 | 4376 | `	}` |
|      - | 4377 | `	/* Cleanup */` |
|     17 | 4378 | `	SySetRelease(&sSet);` |
|     17 | 4379 | `	return SXRET_OK;` |
|      1 | 4380 |  |
|      - | 4381 | `/*` |
|      - | 4382 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 4383 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 4384 | ` * Parameters` |
|      - | 4385 | ` *  $str` |
|      - | 4386 | ` *  The input string.` |
|      - | 4387 | ` * $allowable_tags` |
|      - | 4388 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 4389 | ` * Return` |
|      - | 4390 | ` *  Returns the stripped string.` |
|      - | 4391 | ` */` |
|     16 | 4392 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4393 |  |
|     17 | 4394 | `	const char *zTaglist = 0;` |
|      - | 4395 | `	const char *zString;` |
|     17 | 4396 | `	int nTaglen = 0;` |
|      - | 4397 | `	int nLen;` |
|     17 | 4398 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4399 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 4400 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4401 | `		return PH7_OK;` |
|      - | 4402 | `	}` |
|      - | 4403 | `	/* Point to the raw string */` |
|     15 | 4404 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 4405 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 4406 | `		/* Allowed tag */` |
|     11 | 4407 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 4408 | `	}` |
|      - | 4409 | `	/* Process input */` |
|     15 | 4410 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 4411 | `	return PH7_OK;` |
|      9 | 4412 |  |
|      - | 4413 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 4414 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4415 | `/*` |
|      - | 4416 | ` * string str_shuffle(string $str)` |
|      - | 4417 |  |
|      - | 4418 | ` *  Randomly shuffles a string.` |
|      - | 4419 | ` * Parameters` |
|      - | 4420 | ` *  $str` |
|      - | 4421 | ` *   The input string.` |
|      - | 4422 | ` * Return` |
|      - | 4423 | ` *  Returns the shuffled string.` |
|      - | 4424 | ` */` |
|     12 | 4425 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4426 |  |
|      - | 4427 | `	const char *zString;` |
|      - | 4428 | `	int nLen,i,c;` |
|      - | 4429 | `	sxu32 iR;` |
|     13 | 4430 | `	if( nArg < 1 ){` |
|      - | 4431 | `		/* Missing arguments,return the empty string */` |
|      3 | 4432 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4433 | `		return PH7_OK;` |
|      - | 4434 | `	}` |
|      - | 4435 | `	/* Extract the target string */` |
|     11 | 4436 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 4437 | `	if( nLen < 1 ){` |
|      - | 4438 | `		/* Nothing to shuffle */` |
|      3 | 4439 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4440 | `		return PH7_OK;` |
|      - | 4441 | `	}` |
|      - | 4442 | `	/* Shuffle the string */` |
|     43 | 4443 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 4444 | `		/* Generate a random number first */` |
|     35 | 4445 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 4446 | `		/* Extract a random offset */` |
|     35 | 4447 | `		c = zString[iR % nLen];` |
|      - | 4448 | `		/* Append it */` |
|     35 | 4449 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 4450 | `	}` |
|      9 | 4451 | `	return PH7_OK;` |
|      7 | 4452 |  |
|      - | 4453 | `/*` |
|      - | 4454 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 4455 | ` *  Convert a string to an array.` |
|      - | 4456 | ` * Parameters` |
|      - | 4457 | ` * $string` |
|      - | 4458 | ` *  The input string.` |
|      - | 4459 | ` * $split_length` |
|      - | 4460 | ` *  Maximum length of the chunk.` |
|      - | 4461 | ` * Return` |
|      - | 4462 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 4463 | ` *  except possibly the last one which may be shorter.` |
|      - | 4464 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 4465 | ` *  as the first (and only) array element.` |
|      - | 4466 | ` *  An empty string returns an empty array.` |
|      - | 4467 | ` * Errors` |
|      - | 4468 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 4469 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 4470 | ` *  ValueError if $split_length is less than 1.` |
|      - | 4471 | ` */` |
|     28 | 4472 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4473 |  |
|      - | 4474 | `	const char *zString,*zEnd;` |
|      - | 4475 | `	ph7_value *pArray,*pValue;` |
|      - | 4476 | `	int split_len;` |
|      - | 4477 | `	int nLen;` |
|     30 | 4478 | `	if( nArg < 1 ){` |
|      4 | 4479 | `		return PH7_VmThrowException(pCtx,` |
|      - | 4480 | `			"ArgumentCountError",` |
|      - | 4481 | `			"str_split() expects at least 1 argument, %d given",` |
|      1 | 4482 | `			nArg` |
|      - | 4483 | `			);` |
|      - | 4484 | `	}` |
|      - | 4485 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     50 | 4486 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     38 | 4487 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 4488 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 4489 | `		return PH7_VmThrowException(pCtx,` |
|      - | 4490 | `			"TypeError",` |
|      - | 4491 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 4492 | `			ph7_type_name(apArg[0])` |
|      - | 4493 | `			);` |
|      - | 4494 | `	}` |
|      - | 4495 | `	/* Point to the target string */` |
|     26 | 4496 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     26 | 4497 | `	split_len = (int)sizeof(char);` |
|     26 | 4498 | `	if( nArg > 1 ){` |
|      - | 4499 | `		/* Split length */` |
|     16 | 4500 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     16 | 4501 | `		if( split_len < 1 ){` |
|      5 | 4502 | `			return PH7_VmThrowException(pCtx,` |
|      - | 4503 | `				"ValueError",` |
|      - | 4504 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 4505 | `				);` |
|      - | 4506 | `		}` |
|     11 | 4507 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 4508 | `			split_len = nLen;` |
|      1 | 4509 | `		}` |
|      5 | 4510 | `	}` |
|      - | 4511 | `	/* Create the array and the scalar value */` |
|     21 | 4512 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 4513 | `	/*Chunk value */` |
|     21 | 4514 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 4515 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 4516 | `		/* Return FALSE */` |
|    ! 0 | 4517 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4518 | `		return PH7_OK;` |
|      - | 4519 | `	}` |
|      - | 4520 | `	/* Point to the end of the string */` |
|     21 | 4521 | `	zEnd = &zString[nLen];` |
|      - | 4522 | `	/* Perform the requested operation */` |
|     48 | 4523 | `	for(;;){` |
|      - | 4524 | `		int nMax;` |
|     59 | 4525 | `		if( zString >= zEnd ){` |
|      - | 4526 | `			/* No more input to process */` |
|     21 | 4527 | `			break;` |
|      - | 4528 | `		}` |
|     39 | 4529 | `		nMax = (int)(zEnd-zString);` |
|     39 | 4530 | `		if( nMax < split_len ){` |
|      3 | 4531 | `			split_len = nMax;` |
|      1 | 4532 | `		}` |
|      - | 4533 | `		/* Copy the current chunk */` |
|     39 | 4534 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 4535 | `		/* Insert it */` |
|     39 | 4536 | `		ph7_array_add_elem(pArray,0,pValue); /* Will make it's own copy */` |
|      - | 4537 | `		/* reset the string cursor */` |
|     39 | 4538 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 4539 | `		/* Update position */` |
|     39 | 4540 | `		zString += split_len;` |
|      1 | 4541 | `	}` |
|      - | 4542 | `	/*` |
|      - | 4543 | `	 * Return the array.` |
|      - | 4544 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 4545 | `	 * upon we return from this function.` |
|      - | 4546 | `	 */` |
|     21 | 4547 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 4548 | `	return PH7_OK;` |
|     16 | 4549 |  |
|      - | 4550 | `/*` |
|      - | 4551 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 4552 | ` * Refer to [strspn()].` |
|      - | 4553 | ` */` |
|     28 | 4554 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 4555 |  |
|     29 | 4556 | `	const char *zIn = *pzIn;` |
|      - | 4557 | `	const char *zPtr;` |
|      - | 4558 | `	/* Ignore leading white spaces */` |
|     29 | 4559 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 4560 | `		zIn++;` |
|    ! 0 | 4561 | `	}` |
|     29 | 4562 | `	if( zIn >= zEnd ){` |
|      - | 4563 | `		/* End of input */` |
|    ! 0 | 4564 | `		return SXERR_EOF;` |
|      - | 4565 | `	}` |
|     29 | 4566 | `	zPtr = zIn;` |
|      - | 4567 | `	/* Extract the token */` |
|    201 | 4568 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 4569 | `		zIn++;` |
|      1 | 4570 | `	}` |
|     29 | 4571 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 4572 | `	/* Synchronize pointers */` |
|     29 | 4573 | `	*pzIn = zIn;` |
|      - | 4574 | `	/* Return to the caller */` |
|     29 | 4575 | `	return SXRET_OK;` |
|     15 | 4576 |  |
|      - | 4577 | `/*` |
|      - | 4578 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 4579 | ` * return the longest match.` |
|      - | 4580 | ` * Refer to [strspn()].` |
|      - | 4581 | ` */` |
|     18 | 4582 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 4583 |  |
|     19 | 4584 | `	const char *zEnd = &zString[nLen];` |
|     19 | 4585 | `	const char *zIn = zString;` |
|      - | 4586 | `	int i,c;` |
|     45 | 4587 | `	for(;;){` |
|     91 | 4588 | `		if( zString >= zEnd ){` |
|      7 | 4589 | `			break;` |
|      - | 4590 | `		}` |
|      - | 4591 | `		/* Extract current character */` |
|     85 | 4592 | `		c = zString[0];` |
|      - | 4593 | `		/* Perform the lookup */` |
|    383 | 4594 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 4595 | `			if( c == zMask[i] ){` |
|      - | 4596 | `				/* Character found */` |
|     73 | 4597 | `				break;` |
|      - | 4598 | `			}` |
|    150 | 4599 | `		}` |
|     85 | 4600 | `		if( i >= nMaskLen ){` |
|      - | 4601 | `			/* Character not in the current mask,break immediately */` |
|     13 | 4602 | `			break;` |
|      - | 4603 | `		}` |
|      - | 4604 | `		/* Advance cursor */` |
|     73 | 4605 | `		zString++;` |
|      1 | 4606 | `	}` |
|      - | 4607 | `	/* Longest match */` |
|     19 | 4608 | `	return (int)(zString-zIn);` |
|      1 | 4609 |  |
|      - | 4610 | `/*` |
|      - | 4611 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 4612 | ` * Refer to [strcspn()].` |
|      - | 4613 | ` */` |
|     10 | 4614 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 4615 |  |
|     11 | 4616 | `	const char *zEnd = &zString[nLen];` |
|     11 | 4617 | `	const char *zIn = zString;` |
|      - | 4618 | `	int i,c;` |
|     12 | 4619 | `	for(;;){` |
|     25 | 4620 | `		if( zString >= zEnd ){` |
|      3 | 4621 | `			break;` |
|      - | 4622 | `		}` |
|      - | 4623 | `		/* Extract current character */` |
|     23 | 4624 | `		c = zString[0];` |
|      - | 4625 | `		/* Perform the lookup */` |
|     51 | 4626 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 4627 | `			if( c == zMask[i] ){` |
|      9 | 4628 | `				break;` |
|      - | 4629 | `			}` |
|     15 | 4630 | `		}` |
|     23 | 4631 | `		if( i < nMaskLen ){` |
|      - | 4632 | `			/* Character in the current mask,break immediately */` |
|      9 | 4633 | `			break;` |
|      - | 4634 | `		}` |
|      - | 4635 | `		/* Advance cursor */` |
|     15 | 4636 | `		zString++;` |
|      1 | 4637 | `	}` |
|      - | 4638 | `	/* Longest match */` |
|     11 | 4639 | `	return (int)(zString-zIn);` |
|      1 | 4640 |  |
|      - | 4641 | `/*` |
|      - | 4642 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 4643 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 4644 | ` *  of characters contained within a given mask.` |
|      - | 4645 | ` * Parameters` |
|      - | 4646 | ` * $str` |
|      - | 4647 | ` *  The input string.` |
|      - | 4648 | ` * $mask` |
|      - | 4649 | ` *  The list of allowable characters.` |
|      - | 4650 | ` * $start` |
|      - | 4651 | ` *  The position in subject to start searching.` |
|      - | 4652 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 4653 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 4654 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 4655 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 4656 | ` *  start'th position from the end of subject.` |
|      - | 4657 | ` * $length` |
|      - | 4658 | ` *  The length of the segment from subject to examine.` |
|      - | 4659 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 4660 | ` *  characters after the starting position.` |
|      - | 4661 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 4662 | ` *  position up to length characters from the end of subject.` |
|      - | 4663 | ` * Return` |
|      - | 4664 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 4665 | ` * in mask.` |
|      - | 4666 | ` */` |
|     26 | 4667 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4668 |  |
|      - | 4669 | `	const char *zString,*zMask,*zEnd;` |
|      - | 4670 | `	int iMasklen,iLen;` |
|      - | 4671 | `	SyString sToken;` |
|     27 | 4672 | `	int iCount = 0;` |
|      - | 4673 | `	int rc;` |
|     27 | 4674 | `	if( nArg < 2 ){` |
|      - | 4675 | `		/* Missing agruments,return zero */` |
|      3 | 4676 | `		ph7_result_int(pCtx,0);` |
|      3 | 4677 | `		return PH7_OK;` |
|      - | 4678 | `	}` |
|      - | 4679 | `	/* Extract the target string */` |
|     25 | 4680 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 4681 | `	/* Extract the mask */` |
|     25 | 4682 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 4683 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 4684 | `		/* Nothing to process,return zero */` |
|      7 | 4685 | `		ph7_result_int(pCtx,0);` |
|      7 | 4686 | `		return PH7_OK;` |
|      - | 4687 | `	}` |
|     19 | 4688 | `	if( nArg > 2 ){` |
|      - | 4689 | `		int nOfft;` |
|      - | 4690 | `		/* Extract the offset */` |
|      9 | 4691 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 4692 | `		if( nOfft < 0 ){` |
|    ! 0 | 4693 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 4694 | `			if( zBase > zString ){` |
|    ! 0 | 4695 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 4696 | `				zString = zBase;` |
|    ! 0 | 4697 | `			}else{` |
|      - | 4698 | `				/* Invalid offset */` |
|    ! 0 | 4699 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 4700 | `				return PH7_OK;` |
|      - | 4701 | `			}` |
|    ! 0 | 4702 | `		}else{` |
|      9 | 4703 | `			if( nOfft >= iLen ){` |
|      - | 4704 | `				/* Invalid offset */` |
|    ! 0 | 4705 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 4706 | `				return PH7_OK;` |
|    ! 0 | 4707 | `			}else{` |
|      - | 4708 | `				/* Update offset */` |
|      9 | 4709 | `				zString += nOfft;` |
|      9 | 4710 | `				iLen -= nOfft;` |
|      - | 4711 | `			}` |
|      - | 4712 | `		}` |
|      9 | 4713 | `		if( nArg > 3 ){` |
|      - | 4714 | `			int iUserlen;` |
|      - | 4715 | `			/* Extract the desired length */` |
|      9 | 4716 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 4717 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 4718 | `				iLen = iUserlen;` |
|      2 | 4719 | `			}` |
|      4 | 4720 | `		}` |
|      4 | 4721 | `	}` |
|      - | 4722 | `	/* Point to the end of the string */` |
|     19 | 4723 | `	zEnd = &zString[iLen];` |
|      - | 4724 | `	/* Extract the first non-space token */` |
|     19 | 4725 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 4726 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 4727 | `		/* Compare against the current mask */` |
|     19 | 4728 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 4729 | `	}` |
|      - | 4730 | `	/* Longest match */` |
|     19 | 4731 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 4732 | `	return PH7_OK;` |
|     14 | 4733 |  |
|      - | 4734 | `/*` |
|      - | 4735 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 4736 | ` *  Find length of initial segment not matching mask.` |
|      - | 4737 | ` * Parameters` |
|      - | 4738 | ` * $str` |
|      - | 4739 | ` *  The input string.` |
|      - | 4740 | ` * $mask` |
|      - | 4741 | ` *  The list of not allowed characters.` |
|      - | 4742 | ` * $start` |
|      - | 4743 | ` *  The position in subject to start searching.` |
|      - | 4744 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 4745 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 4746 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 4747 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 4748 | ` *  start'th position from the end of subject.` |
|      - | 4749 | ` * $length` |
|      - | 4750 | ` *  The length of the segment from subject to examine.` |
|      - | 4751 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 4752 | ` *  characters after the starting position.` |
|      - | 4753 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 4754 | ` *  position up to length characters from the end of subject.` |
|      - | 4755 | ` * Return` |
|      - | 4756 | ` *  Returns the length of the segment as an integer.` |
|      - | 4757 | ` */` |
|     16 | 4758 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4759 |  |
|      - | 4760 | `	const char *zString,*zMask,*zEnd;` |
|      - | 4761 | `	int iMasklen,iLen;` |
|      - | 4762 | `	SyString sToken;` |
|     17 | 4763 | `	int iCount = 0;` |
|      - | 4764 | `	int rc;` |
|     17 | 4765 | `	if( nArg < 2 ){` |
|      - | 4766 | `		/* Missing agruments,return zero */` |
|      3 | 4767 | `		ph7_result_int(pCtx,0);` |
|      3 | 4768 | `		return PH7_OK;` |
|      - | 4769 | `	}` |
|      - | 4770 | `	/* Extract the target string */` |
|     15 | 4771 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 4772 | `	/* Extract the mask */` |
|     15 | 4773 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 4774 | `	if( iLen < 1 ){` |
|      - | 4775 | `		/* Nothing to process,return zero */` |
|    ! 0 | 4776 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4777 | `		return PH7_OK;` |
|      - | 4778 | `	}` |
|     15 | 4779 | `	if( iMasklen < 1 ){` |
|      - | 4780 | `		/* No given mask,return the string length */` |
|      3 | 4781 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 4782 | `		return PH7_OK;` |
|      - | 4783 | `	}` |
|     13 | 4784 | `	if( nArg > 2 ){` |
|      - | 4785 | `		int nOfft;` |
|      - | 4786 | `		/* Extract the offset */` |
|     11 | 4787 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 4788 | `		if( nOfft < 0 ){` |
|    ! 0 | 4789 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 4790 | `			if( zBase > zString ){` |
|    ! 0 | 4791 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 4792 | `				zString = zBase;` |
|    ! 0 | 4793 | `			}else{` |
|      - | 4794 | `				/* Invalid offset */` |
|    ! 0 | 4795 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 4796 | `				return PH7_OK;` |
|      - | 4797 | `			}` |
|    ! 0 | 4798 | `		}else{` |
|     11 | 4799 | `			if( nOfft >= iLen ){` |
|      - | 4800 | `				/* Invalid offset */` |
|      3 | 4801 | `				ph7_result_int(pCtx,0);` |
|      3 | 4802 | `				return PH7_OK;` |
|    ! 0 | 4803 | `			}else{` |
|      - | 4804 | `				/* Update offset */` |
|      9 | 4805 | `				zString += nOfft;` |
|      9 | 4806 | `				iLen -= nOfft;` |
|      - | 4807 | `			}` |
|      - | 4808 | `		}` |
|      9 | 4809 | `		if( nArg > 3 ){` |
|      - | 4810 | `			int iUserlen;` |
|      - | 4811 | `			/* Extract the desired length */` |
|    ! 0 | 4812 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 4813 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 4814 | `				iLen = iUserlen;` |
|    ! 0 | 4815 | `			}` |
|    ! 0 | 4816 | `		}` |
|      4 | 4817 | `	}` |
|      - | 4818 | `	/* Point to the end of the string */` |
|     11 | 4819 | `	zEnd = &zString[iLen];` |
|      - | 4820 | `	/* Extract the first non-space token */` |
|     11 | 4821 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 4822 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 4823 | `		/* Compare against the current mask */` |
|     11 | 4824 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 4825 | `	}` |
|      - | 4826 | `	/* Longest match */` |
|     11 | 4827 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 4828 | `	return PH7_OK;` |
|      9 | 4829 |  |
|      - | 4830 | `/*` |
|      - | 4831 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 4832 | ` *  Search a string for any of a set of characters.` |
|      - | 4833 | ` * Parameters` |
|      - | 4834 | ` *  $haystack` |
|      - | 4835 | ` *   The string where char_list is looked for.` |
|      - | 4836 | ` *  $char_list` |
|      - | 4837 | ` *   This parameter is case sensitive.` |
|      - | 4838 | ` * Return` |
|      - | 4839 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 4840 | ` */` |
|      6 | 4841 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4842 |  |
|      - | 4843 | `	const char *zString,*zList,*zEnd;` |
|      - | 4844 | `	int iLen,iListLen,i,c;` |
|      - | 4845 | `	sxu32 nOfft,nMax;` |
|      - | 4846 | `	sxi32 rc;` |
|      7 | 4847 | `	if( nArg < 2 ){` |
|      - | 4848 | `		/* Missing arguments,return FALSE */` |
|      3 | 4849 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4850 | `		return PH7_OK;` |
|      - | 4851 | `	}` |
|      - | 4852 | `	/* Extract the haystack and the char list */` |
|      5 | 4853 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 4854 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 4855 | `	if( iLen < 1 ){` |
|      - | 4856 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 4857 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4858 | `		return PH7_OK;` |
|      - | 4859 | `	}` |
|      - | 4860 | `	/* Point to the end of the string */` |
|      5 | 4861 | `	zEnd = &zString[iLen];` |
|      5 | 4862 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 4863 | `	/* perform the requested operation */` |
|     15 | 4864 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 4865 | `		c = zList[i];` |
|     11 | 4866 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 4867 | `		if( rc == SXRET_OK ){` |
|      5 | 4868 | `			if( nMax < nOfft ){` |
|      3 | 4869 | `				nOfft = nMax;` |
|      1 | 4870 | `			}` |
|      2 | 4871 | `		}` |
|      6 | 4872 | `	}` |
|      5 | 4873 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 4874 | `		/* No such substring,return FALSE */` |
|      3 | 4875 | `		ph7_result_bool(pCtx,0);` |
|      2 | 4876 | `	}else{` |
|      - | 4877 | `		/* Return the substring */` |
|      3 | 4878 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 4879 | `	}` |
|      5 | 4880 | `	return PH7_OK;` |
|      4 | 4881 |  |
|      - | 4882 | `/*` |
|      - | 4883 | ` * string soundex(string $str)` |
|      - | 4884 | ` *  Calculate the soundex key of a string.` |
|      - | 4885 | ` * Parameters` |
|      - | 4886 | ` *  $str` |
|      - | 4887 | ` *   The input string.` |
|      - | 4888 | ` * Return` |
|      - | 4889 | ` *  Returns the soundex key as a string.` |
|      - | 4890 | ` * Note:` |
|      - | 4891 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 4892 | ` * source tree.` |
|      - | 4893 | ` */` |
|     20 | 4894 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4895 |  |
|      - | 4896 | `	const unsigned char *zIn;` |
|      - | 4897 | `	char zResult[8];` |
|      - | 4898 | `	int i, j;` |
|      - | 4899 | `	static const unsigned char iCode[] = {` |
|      - | 4900 |  |
|      - | 4901 |  |
|      - | 4902 |  |
|      - | 4903 |  |
|      - | 4904 |  |
|      - | 4905 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 4906 |  |
|      - | 4907 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 4908 | `	};` |
|     21 | 4909 | `	if( nArg < 1 ){` |
|      - | 4910 | `		/* Missing arguments,return the empty string */` |
|      3 | 4911 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4912 | `		return PH7_OK;` |
|      - | 4913 | `	}` |
|     19 | 4914 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     19 | 4915 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     19 | 4916 | `	if( zIn[i] ){` |
|     17 | 4917 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 4918 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 4919 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 4920 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 4921 | `			if( code>0 ){` |
|     45 | 4922 | `				if( code!=prevcode ){` |
|     33 | 4923 | `					prevcode = (unsigned char)code;` |
|     33 | 4924 | `					zResult[j++] = (char)code + '0';` |
|     16 | 4925 | `				}` |
|     23 | 4926 | `			}else{` |
|     49 | 4927 | `				prevcode = 0;` |
|      - | 4928 | `			}` |
|     47 | 4929 | `		}` |
|     33 | 4930 | `		while( j<4 ){` |
|     17 | 4931 | `			zResult[j++] = '0';` |
|      1 | 4932 | `		}` |
|     17 | 4933 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 4934 | `	}else{` |
|      3 | 4935 | `	  ph7_result_string(pCtx,"?000",4);` |
|      - | 4936 | `	}` |
|     19 | 4937 | `	return PH7_OK;` |
|     11 | 4938 |  |
|      - | 4939 | `/*` |
|      - | 4940 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 4941 | ` *  Wraps a string to a given number of characters.` |
|      - | 4942 | ` * Parameters` |
|      - | 4943 | ` *  $str` |
|      - | 4944 | ` *   The input string.` |
|      - | 4945 | ` * $width` |
|      - | 4946 | ` *  The column width.` |
|      - | 4947 | ` * $break` |
|      - | 4948 | ` *  The line is broken using the optional break parameter.` |
|      - | 4949 | ` * Return` |
|      - | 4950 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 4951 | ` */` |
|     14 | 4952 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4953 |  |
|      - | 4954 | `	const char *zIn,*zEnd,*zBreak;` |
|      - | 4955 | `	int iLen,iBreaklen,iChunk;` |
|     15 | 4956 | `	if( nArg < 1 ){` |
|      - | 4957 | `		/* Missing arguments,return the empty string */` |
|      3 | 4958 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4959 | `		return PH7_OK;` |
|      - | 4960 | `	}` |
|      - | 4961 | `	/* Extract the input string */` |
|     13 | 4962 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|     13 | 4963 | `	if( iLen < 1 ){` |
|      - | 4964 | `		/* Nothing to process,return the empty string */` |
|      3 | 4965 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4966 | `		return PH7_OK;` |
|      - | 4967 | `	}` |
|      - | 4968 | `	/* Chunk length */` |
|     11 | 4969 | `	iChunk = 75;` |
|     11 | 4970 | `	iBreaklen = 0;` |
|     11 | 4971 | `	zBreak = ""; /* cc warning */` |
|     11 | 4972 | `	if( nArg > 1 ){` |
|     11 | 4973 | `		iChunk = ph7_value_to_int(apArg[1]);` |
|     11 | 4974 | `		if( iChunk < 1 ){` |
|    ! 0 | 4975 | `			iChunk = 75;` |
|    ! 0 | 4976 | `		}` |
|     11 | 4977 | `		if( nArg > 2 ){` |
|      3 | 4978 | `			zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      1 | 4979 | `		}` |
|      5 | 4980 | `	}` |
|     11 | 4981 | `	if( iBreaklen < 1 ){` |
|      - | 4982 | `		/* Set a default column break */` |
|      - | 4983 | `#ifdef __WINNT__` |
|      1 | 4984 | `		zBreak = "\r\n";` |
|      1 | 4985 | `		iBreaklen = (int)sizeof("\r\n")-1;` |
|      - | 4986 | `#else` |
|      8 | 4987 | `		zBreak = "\n";` |
|      8 | 4988 | `		iBreaklen = (int)sizeof(char);` |
|      - | 4989 | `#endif` |
|      4 | 4990 | `	}` |
|      - | 4991 | `	/* Perform the requested operation */` |
|     11 | 4992 | `	zEnd = &zIn[iLen];` |
|     41 | 4993 | `	for(;;){` |
|      - | 4994 | `		int nMax;` |
|     47 | 4995 | `		if( zIn >= zEnd ){` |
|      - | 4996 | `			/* No more input to process */` |
|     11 | 4997 | `			break;` |
|      - | 4998 | `		}` |
|     37 | 4999 | `		nMax = (int)(zEnd-zIn);` |
|     37 | 5000 | `		if( iChunk > nMax ){` |
|     11 | 5001 | `			iChunk = nMax;` |
|      5 | 5002 | `		}` |
|      - | 5003 | `		/* Append the column first */` |
|     37 | 5004 | `		ph7_result_string(pCtx,zIn,iChunk); /* Will make it's own copy */` |
|      - | 5005 | `		/* Advance the cursor */` |
|     37 | 5006 | `		zIn += iChunk;` |
|     37 | 5007 | `		if( zIn < zEnd ){` |
|      - | 5008 | `			/* Append the line break */` |
|     27 | 5009 | `			ph7_result_string(pCtx,zBreak,iBreaklen);` |
|     13 | 5010 | `		}` |
|      1 | 5011 | `	}` |
|     11 | 5012 | `	return PH7_OK;` |
|      8 | 5013 |  |
|      - | 5014 | `/*` |
|      - | 5015 | ` * Check if the given character is a member of the given mask.` |
|      - | 5016 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 5017 | ` * Refer to [strtok()].` |
|      - | 5018 | ` */` |
|     30 | 5019 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 5020 |  |
|      - | 5021 | `	int i;` |
|     57 | 5022 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 5023 | `		if( c == zMask[i] ){` |
|     13 | 5024 | `			if( pOfft ){` |
|      5 | 5025 | `				*pOfft = i;` |
|      2 | 5026 | `			}` |
|     13 | 5027 | `			return TRUE;` |
|      - | 5028 | `		}` |
|     14 | 5029 | `	}` |
|     19 | 5030 | `	return FALSE;` |
|     16 | 5031 |  |
|      - | 5032 | `/*` |
|      - | 5033 | ` * Extract a single token from the input stream.` |
|      - | 5034 | ` * Refer to [strtok()].` |
|      - | 5035 | ` */` |
|      6 | 5036 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 5037 |  |
|      7 | 5038 | `	const char *zIn = *pzIn;` |
|      - | 5039 | `	const char *zPtr;` |
|      - | 5040 | `	/* Ignore leading delimiter */` |
|     11 | 5041 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5042 | `		zIn++;` |
|      1 | 5043 | `	}` |
|      7 | 5044 | `	if( zIn >= zEnd ){` |
|      - | 5045 | `		/* End of input */` |
|    ! 0 | 5046 | `		return SXERR_EOF;` |
|      - | 5047 | `	}` |
|      7 | 5048 | `	zPtr = zIn;` |
|      - | 5049 | `	/* Extract the token */` |
|     13 | 5050 | `	while( zIn < zEnd ){` |
|     11 | 5051 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 5052 | `			/* UTF-8 stream */` |
|    ! 0 | 5053 | `			zIn++;` |
|    ! 0 | 5054 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 5055 | `		}else{` |
|     11 | 5056 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5057 | `				break;` |
|      - | 5058 | `			}` |
|      7 | 5059 | `			zIn++;` |
|      - | 5060 | `		}` |
|      1 | 5061 | `	}` |
|      7 | 5062 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5063 | `	/* Update the cursor */` |
|      7 | 5064 | `	*pzIn = zIn;` |
|      - | 5065 | `	/* Return to the caller */` |
|      7 | 5066 | `	return SXRET_OK;` |
|      4 | 5067 |  |
|      - | 5068 | `/* strtok auxiliary private data */` |
|      - | 5069 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 5070 | `struct strtok_aux_data` |
|      - | 5071 |  |
|      - | 5072 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 5073 | `	const char *zIn;   /* Current input stream */` |
|      - | 5074 | `	const char *zEnd;  /* End of input */` |
|      - | 5075 | `};` |
|      - | 5076 | `/*` |
|      - | 5077 | ` * string strtok(string $str,string $token)` |
|      - | 5078 | ` * string strtok(string $token)` |
|      - | 5079 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 5080 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 5081 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 5082 | ` *  words by using the space character as the token.` |
|      - | 5083 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 5084 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 5085 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 5086 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 5087 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 5088 | ` *  the argument are found.` |
|      - | 5089 | ` * Parameters` |
|      - | 5090 | ` *  $str` |
|      - | 5091 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 5092 | ` * $token` |
|      - | 5093 | ` *  The delimiter used when splitting up str.` |
|      - | 5094 | ` * Return` |
|      - | 5095 | ` *   Current token or FALSE on EOF.` |
|      - | 5096 | ` */` |
|      8 | 5097 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5098 |  |
|      - | 5099 | `	strtok_aux_data *pAux;` |
|      - | 5100 | `	const char *zMask;` |
|      - | 5101 | `	SyString sToken;` |
|      - | 5102 | `	int nMasklen;` |
|      - | 5103 | `	sxi32 rc;` |
|      9 | 5104 | `	if( nArg < 2 ){` |
|      - | 5105 | `		/* Extract top aux data */` |
|      7 | 5106 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      7 | 5107 | `		if( pAux == 0 ){` |
|      - | 5108 | `			/* No aux data,return FALSE */` |
|    ! 0 | 5109 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5110 | `			return PH7_OK;` |
|      - | 5111 | `		}` |
|      7 | 5112 | `		nMasklen = 0;` |
|      7 | 5113 | `		zMask = ""; /* cc warning */` |
|      7 | 5114 | `		if( nArg > 0 ){` |
|      - | 5115 | `			/* Extract the mask */` |
|      5 | 5116 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 5117 | `		}` |
|      7 | 5118 | `		if( nMasklen < 1 ){` |
|      - | 5119 | `			/* Invalid mask,return FALSE */` |
|      3 | 5120 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|      3 | 5121 | `			ph7_context_free_chunk(pCtx,pAux);` |
|      3 | 5122 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|      3 | 5123 | `			ph7_result_bool(pCtx,0);` |
|      3 | 5124 | `			return PH7_OK;` |
|      - | 5125 | `		}` |
|      - | 5126 | `		/* Extract the token */` |
|      5 | 5127 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 5128 | `		if( rc != SXRET_OK ){` |
|      - | 5129 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 5130 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 5131 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5132 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 5133 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5134 | `		}else{` |
|      - | 5135 | `			/* Return the extracted token */` |
|      5 | 5136 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 5137 | `		}` |
|      3 | 5138 | `	}else{` |
|      - | 5139 | `		const char *zInput,*zCur;` |
|      - | 5140 | `		char *zDup;` |
|      - | 5141 | `		int nLen;` |
|      - | 5142 | `		/* Extract the raw input */` |
|      3 | 5143 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5144 | `		if( nLen < 1 ){` |
|      - | 5145 | `			/* Empty input,return FALSE */` |
|    ! 0 | 5146 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5147 | `			return PH7_OK;` |
|      - | 5148 | `		}` |
|      - | 5149 | `		/* Extract the mask */` |
|      3 | 5150 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 5151 | `		if( nMasklen < 1 ){` |
|      - | 5152 | `			/* Set a default mask */` |
|      - | 5153 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 5154 | `			zMask = TOK_MASK;` |
|    ! 0 | 5155 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 5156 | `#undef TOK_MASK` |
|    ! 0 | 5157 | `		}` |
|      - | 5158 | `		/* Extract a single token */` |
|      3 | 5159 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 5160 | `		if( rc != SXRET_OK ){` |
|      - | 5161 | `			/* Empty input */` |
|    ! 0 | 5162 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5163 | `			return PH7_OK;` |
|    ! 0 | 5164 | `		}else{` |
|      - | 5165 | `			/* Return the extracted token */` |
|      3 | 5166 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 5167 | `		}` |
|      - | 5168 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 5169 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 5170 | `		if( pAux ){` |
|      3 | 5171 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 5172 | `			if( nLen < 1 ){` |
|    ! 0 | 5173 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5174 | `				return PH7_OK;` |
|      - | 5175 | `			}` |
|      - | 5176 | `			/* Duplicate input */` |
|      3 | 5177 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 5178 | `			if( zDup  ){` |
|      3 | 5179 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 5180 | `				/* Register the aux data */` |
|      3 | 5181 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 5182 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 5183 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 5184 | `			}` |
|      1 | 5185 | `		}` |
|      - | 5186 | `	}` |
|      7 | 5187 | `	return PH7_OK;` |
|      5 | 5188 |  |
|      - | 5189 | `/*` |
|      - | 5190 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 5191 | ` *  Pad a string to a certain length with another string` |
|      - | 5192 | ` * Parameters` |
|      - | 5193 | ` *  $input` |
|      - | 5194 | ` *   The input string.` |
|      - | 5195 | ` * $pad_length` |
|      - | 5196 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 5197 | ` *   string, no padding takes place.` |
|      - | 5198 | ` * $pad_string` |
|      - | 5199 | ` *   Note:` |
|      - | 5200 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 5201 | ` *    divided by the pad_string's length.` |
|      - | 5202 | ` * $pad_type` |
|      - | 5203 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 5204 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 5205 | ` * Return` |
|      - | 5206 | ` *  The padded string.` |
|      - | 5207 | ` */` |
|     10 | 5208 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5209 |  |
|      - | 5210 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 5211 | `	const char *zIn,*zPad;` |
|     11 | 5212 | `	if( nArg < 2 ){` |
|      - | 5213 | `		/* Missing arguments,return the empty string */` |
|      5 | 5214 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 5215 | `		return PH7_OK;` |
|      - | 5216 | `	}` |
|      - | 5217 | `	/* Extract the target string */` |
|      7 | 5218 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5219 | `	/* Padding length */` |
|      7 | 5220 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|      7 | 5221 | `	if( iPadlen > 0 ){` |
|      5 | 5222 | `		iPadlen -= iLen;` |
|      2 | 5223 | `	}` |
|      7 | 5224 | `	if( iPadlen < 1  ){` |
|      - | 5225 | `		/* Return the string verbatim */` |
|      3 | 5226 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      3 | 5227 | `		return PH7_OK;` |
|      - | 5228 | `	}` |
|      5 | 5229 | `	zPad = " "; /* Whitespace padding */` |
|      5 | 5230 | `	iStrpad = (int)sizeof(char);` |
|      5 | 5231 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      5 | 5232 | `	if( nArg > 2 ){` |
|      - | 5233 | `		/* Padding string */` |
|      5 | 5234 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      5 | 5235 | `		if( iStrpad < 1 ){` |
|      - | 5236 | `			/* Empty string */` |
|    ! 0 | 5237 | `			zPad = " "; /* Whitespace padding */` |
|    ! 0 | 5238 | `			iStrpad = (int)sizeof(char);` |
|    ! 0 | 5239 | `		}` |
|      5 | 5240 | `		if( nArg > 3 ){` |
|      - | 5241 | `			/* Padd type */` |
|      5 | 5242 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 5243 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 5244 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 5245 | `			}` |
|      2 | 5246 | `		}` |
|      2 | 5247 | `	}` |
|      5 | 5248 | `	iDiv = 1;` |
|      5 | 5249 | `	if( iType == 2 ){` |
|    ! 0 | 5250 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 5251 | `	}` |
|      - | 5252 | `	/* Perform the requested operation */` |
|      5 | 5253 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 5254 | `		jPad = iStrpad;` |
|      5 | 5255 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 5256 | `			/* Padding */` |
|      5 | 5257 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 5258 | `				break;` |
|      - | 5259 | `			}` |
|      3 | 5260 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      2 | 5261 | `		}` |
|      3 | 5262 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 5263 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 5264 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 5265 | `				if( jPad > iStrpad ){` |
|    ! 0 | 5266 | `					jPad = iStrpad;` |
|    ! 0 | 5267 | `				}` |
|      3 | 5268 | `				if( jPad < 1){` |
|    ! 0 | 5269 | `					break;` |
|      - | 5270 | `				}` |
|      3 | 5271 | `				ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 5272 | `			}` |
|      1 | 5273 | `		}` |
|      1 | 5274 | `	}` |
|      5 | 5275 | `	if( iLen > 0 ){` |
|      - | 5276 | `		/* Append the input string */` |
|      5 | 5277 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      2 | 5278 | `	}` |
|      5 | 5279 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 5280 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 5281 | `			/* Padding */` |
|      5 | 5282 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 5283 | `				break;` |
|      - | 5284 | `			}` |
|      3 | 5285 | `			ph7_result_string(pCtx,zPad,iStrpad);` |
|      2 | 5286 | `		}` |
|      5 | 5287 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 5288 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 5289 | `			if( jPad > iStrpad ){` |
|    ! 0 | 5290 | `				jPad = iStrpad;` |
|    ! 0 | 5291 | `			}` |
|      3 | 5292 | `			if( jPad < 1){` |
|    ! 0 | 5293 | `				break;` |
|      - | 5294 | `			}` |
|      3 | 5295 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 5296 | `		}` |
|      1 | 5297 | `	}` |
|      5 | 5298 | `	return PH7_OK;` |
|      6 | 5299 |  |
|      - | 5300 | `/*` |
|      - | 5301 | ` * String replacement private data.` |
|      - | 5302 | ` */` |
|      - | 5303 | `typedef struct str_replace_data str_replace_data;` |
|      - | 5304 | `struct str_replace_data` |
|      - | 5305 |  |
|      - | 5306 | `	/* The following two fields are only used by the strtr function */` |
|      - | 5307 | `	SyBlob *pWorker;         /* Working buffer */` |
|      - | 5308 | `	ProcStringMatch xMatch;  /* Pattern match routine */` |
|      - | 5309 | `	/* The following two fields are only used by the str_replace function */` |
|      - | 5310 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 5311 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 5312 | `};` |
|      - | 5313 | `/*` |
|      - | 5314 | ` * Remove a substring.` |
|      - | 5315 | ` */` |
|      - | 5316 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 5317 | `	for(;;){\` |
|      - | 5318 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 5319 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 5320 | `		++OFFT;\` |
|      - | 5321 | `	}\` |
|      - | 5322 |  |
|      - | 5323 | `/*` |
|      - | 5324 | ` * Shift right and insert algorithm.` |
|      - | 5325 | ` */` |
|      - | 5326 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 5327 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 5328 | `		for(;;){\` |
|      - | 5329 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 5330 | `			if(INLEN < 1 ) { break; }\` |
|      - | 5331 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 5332 | `			--INLEN; \` |
|      - | 5333 | `		}\` |
|      - | 5334 | `		for(;;){\` |
|      - | 5335 | `				if(ELEN < 1) { break; }\` |
|      - | 5336 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 5337 | `				OFFT++;\` |
|      - | 5338 | `				ENTRY++;\` |
|      - | 5339 | `				--ELEN;\` |
|      - | 5340 | `		}\` |
|      - | 5341 |  |
|      - | 5342 | `/*` |
|      - | 5343 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 5344 | ` * replacement string [i.e: zReplace].` |
|      - | 5345 | ` */` |
|     38 | 5346 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 5347 |  |
|     39 | 5348 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 5349 | `	sxu32 n,m;` |
|     39 | 5350 | `	n = SyBlobLength(pWorker);` |
|     39 | 5351 | `	m = nOfft;` |
|      - | 5352 | `	/* Delete the old entry */` |
|    475 | 5353 | `	STRDEL(zInput,n,m,nLen);` |
|     39 | 5354 | `	SyBlobLength(pWorker) -= nLen;` |
|     39 | 5355 | `	if( nReplen > 0 ){` |
|     33 | 5356 | `		sxi32 iRep = nReplen;` |
|      - | 5357 | `		sxi32 rc;` |
|      - | 5358 | `		/*` |
|      - | 5359 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 5360 | `		 * string.` |
|      - | 5361 | `		 */` |
|     33 | 5362 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     33 | 5363 | `		if( rc != SXRET_OK ){` |
|      - | 5364 | `			/* Simply ignore any memory failure problem */` |
|    ! 0 | 5365 | `			return SXRET_OK;` |
|      - | 5366 | `		}` |
|      - | 5367 | `		/* Perform the insertion now */` |
|     33 | 5368 | `		zInput = (char *)SyBlobData(pWorker);` |
|     33 | 5369 | `		n = SyBlobLength(pWorker);` |
|    163 | 5370 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     33 | 5371 | `		SyBlobLength(pWorker) += nReplen;` |
|     16 | 5372 | `	}` |
|     39 | 5373 | `	return SXRET_OK;` |
|     20 | 5374 |  |
|      - | 5375 | `/*` |
|      - | 5376 | ` * String replacement walker callback.` |
|      - | 5377 | ` * The following callback is invoked for each array entry that hold` |
|      - | 5378 | ` * the replace string.` |
|      - | 5379 | ` * Refer to the strtr() implementation for more information.` |
|      - | 5380 | ` */` |
|      8 | 5381 | `static int StringReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 5382 |  |
|      9 | 5383 | `	str_replace_data *pRepData = (str_replace_data *)pUserData;` |
|      - | 5384 | `	const char *zTarget,*zReplace;` |
|      - | 5385 | `	SyBlob *pWorker;` |
|      - | 5386 | `	int tLen,nLen;` |
|      - | 5387 | `	sxu32 nOfft;` |
|      - | 5388 | `	sxi32 rc;` |
|      - | 5389 | `	/* Point to the working buffer */` |
|      9 | 5390 | `	pWorker = pRepData->pWorker;` |
|      9 | 5391 | `	if( !ph7_value_is_string(pKey) ){` |
|      - | 5392 | `		/* Target and replace must be a string */` |
|      3 | 5393 | `		return PH7_OK;` |
|      - | 5394 | `	}` |
|      - | 5395 | `	/* Extract the target and the replace */` |
|      7 | 5396 | `	zTarget = ph7_value_to_string(pKey,&tLen);` |
|      7 | 5397 | `	if( tLen < 1 ){` |
|      - | 5398 | `		/* Empty target,return immediately */` |
|    ! 0 | 5399 | `		return PH7_OK;` |
|      - | 5400 | `	}` |
|      - | 5401 | `	/* Perform a pattern search */` |
|      7 | 5402 | `	rc = pRepData->xMatch(SyBlobData(pWorker),SyBlobLength(pWorker),(const void *)zTarget,(sxu32)tLen,&nOfft);` |
|      7 | 5403 | `	if( rc != SXRET_OK ){` |
|      - | 5404 | `		/* Pattern not found */` |
|    ! 0 | 5405 | `		return PH7_OK;` |
|      - | 5406 | `	}` |
|      - | 5407 | `	/* Extract the replace string */` |
|      7 | 5408 | `	zReplace = ph7_value_to_string(pData,&nLen);` |
|      - | 5409 | `	/* Perform the replace process */` |
|      7 | 5410 | `	StringReplace(pWorker,nOfft,tLen,zReplace,nLen);` |
|      - | 5411 | `	/* All done */` |
|      7 | 5412 | `	return PH7_OK;` |
|      5 | 5413 |  |
|      - | 5414 | `/*` |
|      - | 5415 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 5416 | ` * to collect search/replace string.` |
|      - | 5417 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 5418 | ` */` |
|     26 | 5419 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 5420 |  |
|     27 | 5421 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 5422 | `	SyString sWorker;` |
|      - | 5423 | `	const char *zIn;` |
|      - | 5424 | `	int nByte;` |
|      - | 5425 | `	/* Extract a string representation of the given argument */` |
|     27 | 5426 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 5427 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 5428 | `	if( nByte > 0 ){` |
|      - | 5429 | `		char *zDup;` |
|      - | 5430 | `		/* Duplicate the chunk */` |
|     25 | 5431 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 5432 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 5433 | `			);` |
|     25 | 5434 | `		if( zDup == 0 ){` |
|      - | 5435 | `			/* Ignore any memory failure problem */` |
|    ! 0 | 5436 | `			ph7_context_throw_error(pRep->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 5437 | `			return PH7_OK;` |
|      - | 5438 | `		}` |
|     25 | 5439 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 5440 | `		/* Save the chunk */` |
|     25 | 5441 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 5442 | `	}` |
|      - | 5443 | `	/* Save for later processing */` |
|     27 | 5444 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 5445 | `	/* All done */` |
|     13 | 5446 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 5447 | `	return PH7_OK;` |
|     14 | 5448 |  |
|      - | 5449 | `/*` |
|      - | 5450 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 5451 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 5452 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 5453 | ` * Parameters` |
|      - | 5454 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 5455 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 5456 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 5457 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 5458 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 5459 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 5460 | ` * $search` |
|      - | 5461 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 5462 | ` *  to designate multiple needles.` |
|      - | 5463 | ` * $replace` |
|      - | 5464 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 5465 | ` *  to designate multiple replacements.` |
|      - | 5466 | ` * $subject` |
|      - | 5467 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 5468 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 5469 | ` *  of subject, and the return value is an array as well.` |
|      - | 5470 | ` * $count (Not used)` |
|      - | 5471 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 5472 | ` * Return` |
|      - | 5473 | ` * This function returns a string or an array with the replaced values.` |
|      - | 5474 | ` */` |
|  16274 | 5475 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5476 |  |
|      - | 5477 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 5478 | `	ProcStringMatch xMatch;` |
|      - | 5479 | `	const char *zIn,*zFunc;` |
|      - | 5480 | `	str_replace_data sRep;` |
|      - | 5481 | `	SyBlob sWorker;` |
|      - | 5482 | `	SySet sReplace;` |
|      - | 5483 | `	SySet sSearch;` |
|      - | 5484 | `	int rep_str;` |
|      - | 5485 | `	int nByte;` |
|      - | 5486 | `	sxi32 rc;` |
|  16276 | 5487 | `	if( nArg < 3 ){` |
|      - | 5488 | `		/* Missing/Invalid arguments,return null */` |
|      7 | 5489 | `		ph7_result_null(pCtx);` |
|      7 | 5490 | `		return PH7_OK;` |
|      - | 5491 | `	}` |
|      - | 5492 | `	/* Initialize fields */` |
|  16270 | 5493 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  16270 | 5494 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  16270 | 5495 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  16270 | 5496 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  16270 | 5497 | `	sRep.pCtx = pCtx;` |
|  16270 | 5498 | `	sRep.pCollector = &sSearch;` |
|  16270 | 5499 | `	rep_str = 0;` |
|      - | 5500 | `	/* Extract the subject */` |
|  16270 | 5501 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  16270 | 5502 | `	if( nByte < 1 ){` |
|      - | 5503 | `		/* Nothing to replace,return the empty string */` |
|     38 | 5504 | `		ph7_result_string(pCtx,"",0);` |
|     38 | 5505 | `		return PH7_OK;` |
|      - | 5506 | `	}` |
|      - | 5507 | `	/* Copy the subject */` |
|  16234 | 5508 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 5509 | `	/* Search string */` |
|  16234 | 5510 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 5511 | `		/* Collect search string */` |
|      9 | 5512 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 5513 | `	}else{` |
|      - | 5514 | `		/* Single pattern */` |
|  16226 | 5515 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  16226 | 5516 | `		if( nByte < 1 ){` |
|      - | 5517 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 5518 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 5519 | `			return PH7_OK;` |
|      - | 5520 | `		}` |
|  16222 | 5521 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 5522 | `		/* Save for later processing */` |
|  16222 | 5523 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 5524 | `	}` |
|      - | 5525 | `	/* Replace string */` |
|  16230 | 5526 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 5527 | `		/* Collect replace string */` |
|      7 | 5528 | `		sRep.pCollector = &sReplace;` |
|      7 | 5529 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 5530 | `	}else{` |
|      - | 5531 | `		/* Single needle */` |
|  16224 | 5532 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  16224 | 5533 | `		rep_str = 1;` |
|  16224 | 5534 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 5535 | `		/* Save for later processing */` |
|  16224 | 5536 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 5537 | `	}` |
|      - | 5538 | `	/* Reset loop cursors */` |
|  16230 | 5539 | `	SySetResetCursor(&sSearch);` |
|  16230 | 5540 | `	SySetResetCursor(&sReplace);` |
|  16230 | 5541 | `	pReplace = pSearch = 0; /* cc warning */` |
|  16230 | 5542 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 5543 | `	/* Extract function name */` |
|  16230 | 5544 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 5545 | `	/* Set the default pattern match routine */` |
|  16230 | 5546 | `	xMatch = SyBlobSearch;` |
|  16230 | 5547 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 5548 | `		/* Case insensitive pattern match */` |
|     11 | 5549 | `		xMatch = iPatternMatch;` |
|      5 | 5550 | `	}` |
|      - | 5551 | `	/* Start the replace process */` |
|  32466 | 5552 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 5553 | `		sxu32 nCount,nOfft;` |
|  16238 | 5554 | `		if( pSearch->nByte <  1 ){` |
|      - | 5555 | `			/* Empty string,ignore */` |
|      3 | 5556 | `			continue;` |
|      - | 5557 | `		}` |
|      - | 5558 | `		/* Extract the replace string */` |
|  16236 | 5559 | `		if( rep_str ){` |
|  16226 | 5560 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|   8114 | 5561 | `		}else{` |
|     11 | 5562 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 5563 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 5564 | `				 * An empty string is used for the rest of replacement values` |
|      - | 5565 | `				 */` |
|      3 | 5566 | `				pReplace = 0;` |
|      1 | 5567 | `			}` |
|      - | 5568 | `		}` |
|  16236 | 5569 | `		if( pReplace == 0 ){` |
|      - | 5570 | `			/* Use an empty string instead */` |
|      3 | 5571 | `			pReplace = &sTemp;` |
|      1 | 5572 | `		}` |
|  16236 | 5573 | `		nOfft = nCount = 0;` |
|   8133 | 5574 | `		for(;;){` |
|  16268 | 5575 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 5576 | `				break;` |
|      - | 5577 | `			}` |
|      - | 5578 | `			/* Perform a pattern lookup */` |
|  24383 | 5579 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  16254 | 5580 | `				pSearch->nByte,&nOfft);` |
|  16256 | 5581 | `			if( rc != SXRET_OK ){` |
|      - | 5582 | `				/* Pattern not found */` |
|  16224 | 5583 | `				break;` |
|      - | 5584 | `			}` |
|      - | 5585 | `			/* Perform the replace operation */` |
|     33 | 5586 | `			StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|      - | 5587 | `			/* Increment offset counter */` |
|     33 | 5588 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 5589 | `		}` |
|      2 | 5590 | `	}` |
|      - | 5591 | `	/* All done,clean-up the mess left behind */` |
|  16230 | 5592 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  16230 | 5593 | `	SySetRelease(&sSearch);` |
|  16230 | 5594 | `	SySetRelease(&sReplace);` |
|  16230 | 5595 | `	SyBlobRelease(&sWorker);` |
|  16230 | 5596 | `	return PH7_OK;` |
|   8139 | 5597 |  |
|      - | 5598 | `/*` |
|      - | 5599 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 5600 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 5601 | ` *  Translate characters or replace substrings.` |
|      - | 5602 | ` * Parameters` |
|      - | 5603 | ` *  $str` |
|      - | 5604 | ` *  The string being translated.` |
|      - | 5605 | ` * $from` |
|      - | 5606 | ` *  The string being translated to to.` |
|      - | 5607 | ` * $to` |
|      - | 5608 | ` *  The string replacing from.` |
|      - | 5609 | ` * $replace_pairs` |
|      - | 5610 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 5611 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 5612 | ` * Return` |
|      - | 5613 | ` *  The translated string.` |
|      - | 5614 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 5615 | ` */` |
|     12 | 5616 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5617 |  |
|      - | 5618 | `	const char *zIn;` |
|      - | 5619 | `	int nLen;` |
|     13 | 5620 | `	if( nArg < 1 ){` |
|      - | 5621 | `		/* Nothing to replace,return FALSE */` |
|      7 | 5622 | `		ph7_result_bool(pCtx,0);` |
|      7 | 5623 | `		return PH7_OK;` |
|      - | 5624 | `	}` |
|      7 | 5625 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 5626 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 5627 | `		/* Invalid arguments */` |
|    ! 0 | 5628 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 5629 | `		return PH7_OK;` |
|      - | 5630 | `	}` |
|      9 | 5631 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 5632 | `		str_replace_data sRepData;` |
|      - | 5633 | `		SyBlob sWorker;` |
|      - | 5634 | `		/* Initilaize the working buffer */` |
|      5 | 5635 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|      - | 5636 | `		/* Copy raw string */` |
|      5 | 5637 | `		SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nLen);` |
|      - | 5638 | `		/* Init our replace data instance */` |
|      5 | 5639 | `		sRepData.pWorker = &sWorker;` |
|      5 | 5640 | `		sRepData.xMatch = SyBlobSearch;` |
|      - | 5641 | `		/* Iterate throw array entries and perform the replace operation.*/` |
|      5 | 5642 | `		ph7_array_walk(apArg[1],StringReplaceWalker,&sRepData);` |
|      - | 5643 | `		/* All done, return the result string */` |
|      7 | 5644 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|      4 | 5645 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 5646 | `		/* Clean-up */` |
|      5 | 5647 | `		SyBlobRelease(&sWorker);` |
|      3 | 5648 | `	}else{` |
|      - | 5649 | `		int i,flen,tlen,c,iOfft;` |
|      - | 5650 | `		const char *zFrom,*zTo;` |
|      3 | 5651 | `		if( nArg < 3 ){` |
|      - | 5652 | `			/* Nothing to replace */` |
|    ! 0 | 5653 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 5654 | `			return PH7_OK;` |
|      - | 5655 | `		}` |
|      - | 5656 | `		/* Extract given arguments */` |
|      3 | 5657 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 5658 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 5659 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 5660 | `			/* Nothing to replace */` |
|    ! 0 | 5661 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 5662 | `			return PH7_OK;` |
|      - | 5663 | `		}` |
|      - | 5664 | `		/* Start the replace process */` |
|     13 | 5665 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 5666 | `			c = zIn[i];` |
|     11 | 5667 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 5668 | `				if ( iOfft < tlen ){` |
|      5 | 5669 | `					c = zTo[iOfft];` |
|      2 | 5670 | `				}` |
|      2 | 5671 | `			}` |
|     11 | 5672 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 5673 |  |
|      6 | 5674 | `		}` |
|      - | 5675 | `	}` |
|      7 | 5676 | `	return PH7_OK;` |
|      7 | 5677 |  |
|      - | 5678 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 5679 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 5680 | `/*` |
|      - | 5681 | ` * Parse an INI string.` |
|      - | 5682 |  |
|      - | 5683 | ` * According to wikipedia` |
|      - | 5684 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 5685 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 5686 | ` *  Format` |
|      - | 5687 | `*    Properties` |
|      - | 5688 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 5689 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 5690 | `*     Example:` |
|      - | 5691 | `*      name=value` |
|      - | 5692 | `*    Sections` |
|      - | 5693 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 5694 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 5695 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 5696 | `*     or the end of the file. Sections may not be nested.` |
|      - | 5697 | `*     Example:` |
|      - | 5698 | `*      [section]` |
|      - | 5699 | `*   Comments` |
|      - | 5700 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 5701 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 5702 | `*/` |
|     12 | 5703 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 5704 |  |
|      - | 5705 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 5706 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 5707 | `	SyHashEntry *pEntry;` |
|      - | 5708 | `	SyString sEntry;` |
|      - | 5709 | `	SyHash sHash;` |
|      - | 5710 | `	int c;` |
|      - | 5711 | `	/* Create an empty array and worker variables */` |
|     13 | 5712 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 5713 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 5714 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 5715 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 5716 | `		/* Out of memory */` |
|    ! 0 | 5717 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      - | 5718 | `		/* Return FALSE */` |
|    ! 0 | 5719 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5720 | `		return PH7_OK;` |
|      - | 5721 | `	}` |
|     13 | 5722 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 5723 | `	pCur = pArray;` |
|      - | 5724 | `	/* Start the parse process */` |
|     21 | 5725 | `	for(;;){` |
|      - | 5726 | `		/* Ignore leading white spaces */` |
|     69 | 5727 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 5728 | `			zIn++;` |
|      1 | 5729 | `		}` |
|     43 | 5730 | `		if( zIn >= zEnd ){` |
|      - | 5731 | `			/* No more input to process */` |
|     13 | 5732 | `			break;` |
|      - | 5733 | `		}` |
|     31 | 5734 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 5735 | `			/* Comment til the end of line */` |
|    ! 0 | 5736 | `			zIn++;` |
|    ! 0 | 5737 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 5738 | `				zIn++;` |
|    ! 0 | 5739 | `			}` |
|    ! 0 | 5740 | `			continue;` |
|      - | 5741 | `		}` |
|      - | 5742 | `		/* Reset the string cursor of the working variable */` |
|     31 | 5743 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 5744 | `		if( zIn[0] == '[' ){` |
|      - | 5745 | `			/* Section: Extract the section name */` |
|      9 | 5746 | `			zIn++;` |
|      9 | 5747 | `			zCur = zIn;` |
|     73 | 5748 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 5749 | `				zIn++;` |
|      1 | 5750 | `			}` |
|      9 | 5751 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 5752 | `				/* Save the section name */` |
|      5 | 5753 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 5754 | `				SyStringFullTrim(&sEntry);` |
|      5 | 5755 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 5756 | `				if( sEntry.nByte > 0 ){` |
|      - | 5757 | `					/* Associate an array with the section */` |
|      5 | 5758 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 5759 | `					if( pSection ){` |
|      5 | 5760 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 5761 | `						pCur = pSection;` |
|      2 | 5762 | `					}` |
|      2 | 5763 | `				}` |
|      2 | 5764 | `			}` |
|      9 | 5765 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 5766 | `		}else{` |
|      - | 5767 | `			ph7_value *pOldCur;` |
|      - | 5768 | `			int is_array;` |
|      - | 5769 | `			int iLen;` |
|      - | 5770 | `			/* Properties */` |
|     23 | 5771 | `			is_array = 0;` |
|     23 | 5772 | `			zCur = zIn;` |
|     23 | 5773 | `			iLen = 0; /* cc warning */` |
|     23 | 5774 | `			pOldCur = pCur;` |
|    155 | 5775 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 5776 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 5777 | `					/* Array */` |
|    ! 0 | 5778 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 5779 | `					is_array = 1;` |
|    ! 0 | 5780 | `					if( iLen > 0 ){` |
|    ! 0 | 5781 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 5782 | `						/* Query the hashtable */` |
|    ! 0 | 5783 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 5784 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 5785 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 5786 | `						if( pEntry ){` |
|    ! 0 | 5787 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 5788 | `						}else{` |
|      - | 5789 | `							/* Create an empty array */` |
|    ! 0 | 5790 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 5791 | `							if( pvArr ){` |
|      - | 5792 | `								/* Save the entry */` |
|    ! 0 | 5793 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 5794 | `								/* Insert the entry */` |
|    ! 0 | 5795 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 5796 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 5797 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 5798 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 5799 | `							}` |
|      - | 5800 | `						}` |
|    ! 0 | 5801 | `						if( pvArr ){` |
|    ! 0 | 5802 | `							pCur = pvArr;` |
|    ! 0 | 5803 | `						}` |
|    ! 0 | 5804 | `					}` |
|    ! 0 | 5805 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 5806 | `						zIn++;` |
|    ! 0 | 5807 | `					}` |
|    ! 0 | 5808 | `				}` |
|    133 | 5809 | `				zIn++;` |
|      1 | 5810 | `			}` |
|     23 | 5811 | `			if( !is_array ){` |
|     23 | 5812 | `				iLen = (int)(zIn-zCur);` |
|     11 | 5813 | `			}` |
|      - | 5814 | `			/* Trim the key */` |
|     23 | 5815 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 5816 | `			SyStringFullTrim(&sEntry);` |
|     23 | 5817 | `			if( sEntry.nByte > 0 ){` |
|     23 | 5818 | `				if( !is_array ){` |
|      - | 5819 | `					/* Save the key name */` |
|     23 | 5820 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 5821 | `				}` |
|      - | 5822 | `				/* extract key value */` |
|     23 | 5823 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 5824 | `				zIn++; /* '=' */` |
|     39 | 5825 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 5826 | `					zIn++;` |
|      1 | 5827 | `				}` |
|     23 | 5828 | `				if( zIn < zEnd ){` |
|     21 | 5829 | `					zCur = zIn;` |
|     21 | 5830 | `					c = zIn[0];` |
|     21 | 5831 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 5832 | `						zIn++;` |
|      - | 5833 | `						/* Delimit the value */` |
|    ! 0 | 5834 | `						while( zIn < zEnd ){` |
|    ! 0 | 5835 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 5836 | `								break;` |
|      - | 5837 | `							}` |
|    ! 0 | 5838 | `							zIn++;` |
|    ! 0 | 5839 | `						}` |
|    ! 0 | 5840 | `						if( zIn < zEnd ){` |
|    ! 0 | 5841 | `							zIn++;` |
|    ! 0 | 5842 | `						}` |
|    ! 0 | 5843 | `					}else{` |
|    125 | 5844 | `						while( zIn < zEnd ){` |
|    123 | 5845 | `							if( zIn[0] == '\n' ){` |
|     19 | 5846 | `								if( zIn[-1] != '\\' ){` |
|     19 | 5847 | `									break;` |
|    ! 0 | 5848 | `								}` |
|    105 | 5849 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 5850 | `								/* Inline comments */` |
|    ! 0 | 5851 | `								break;` |
|      - | 5852 | `							}` |
|    105 | 5853 | `							zIn++;` |
|      1 | 5854 | `						}` |
|      - | 5855 | `					}` |
|      - | 5856 | `					/* Trim the value */` |
|     21 | 5857 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 5858 | `					SyStringFullTrim(&sEntry);` |
|     21 | 5859 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 5860 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 5861 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 5862 | `					}` |
|     21 | 5863 | `					if( sEntry.nByte > 0 ){` |
|     21 | 5864 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 5865 | `					}` |
|      - | 5866 | `					/* Insert the key and it's value */` |
|     21 | 5867 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 5868 | `				}` |
|     12 | 5869 | `			}else{` |
|    ! 0 | 5870 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 5871 | `					zIn++;` |
|    ! 0 | 5872 | `				}` |
|      - | 5873 | `			}` |
|     23 | 5874 | `			pCur = pOldCur;` |
|      - | 5875 | `		}` |
|      1 | 5876 | `	}` |
|     13 | 5877 | `	SyHashRelease(&sHash);` |
|      - | 5878 | `	/* Return the parse of the INI string */` |
|     13 | 5879 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 5880 | `	return SXRET_OK;` |
|      7 | 5881 |  |
|      - | 5882 | `/*` |
|      - | 5883 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 5884 | ` *  Parse a configuration string.` |
|      - | 5885 | ` * Parameters` |
|      - | 5886 | ` *  $ini` |
|      - | 5887 | ` *   The contents of the ini file being parsed.` |
|      - | 5888 | ` *  $process_sections` |
|      - | 5889 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 5890 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 5891 | ` *  $scanner_mode (Not used)` |
|      - | 5892 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 5893 | ` *   then option values will not be parsed.` |
|      - | 5894 | ` * Return` |
|      - | 5895 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 5896 | ` */` |
|     10 | 5897 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5898 |  |
|      - | 5899 | `	const char *zIni;` |
|      - | 5900 | `	int nByte;` |
|     11 | 5901 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5902 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 5903 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5904 | `		return PH7_OK;` |
|      - | 5905 | `	}` |
|      - | 5906 | `	/* Extract the raw INI buffer */` |
|     11 | 5907 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 5908 | `	/* Process the INI buffer*/` |
|     11 | 5909 | `	PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|     11 | 5910 | `	return PH7_OK;` |
|      6 | 5911 |  |
|      - | 5912 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5913 |  |
|      - | 5914 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5915 |  |
|      - | 5916 | `/*` |
|      - | 5917 | ` * Ctype Functions.` |
|      - | 5918 | ` * Status:` |
|      - | 5919 | ` *    Stable.` |
|      - | 5920 | ` */` |
|      - | 5921 | `/*` |
|      - | 5922 | ` * bool ctype_alnum(string $text)` |
|      - | 5923 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 5924 | ` * Parameters` |
|      - | 5925 | ` *  $text` |
|      - | 5926 | ` *   The tested string.` |
|      - | 5927 | ` * Return` |
|      - | 5928 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 5929 | ` */` |
|     16 | 5930 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5931 |  |
|      - | 5932 | `	const unsigned char *zIn,*zEnd;` |
|      - | 5933 | `	int nLen;` |
|     17 | 5934 | `	if( nArg < 1 ){` |
|      - | 5935 | `		/* Missing arguments,return FALSE */` |
|      3 | 5936 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5937 | `		return PH7_OK;` |
|      - | 5938 | `	}` |
|      - | 5939 | `	/* Extract the target string */` |
|     15 | 5940 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 5941 | `	zEnd = &zIn[nLen];` |
|     15 | 5942 | `	if( nLen < 1 ){` |
|      - | 5943 | `		/* Empty string,return FALSE */` |
|      3 | 5944 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5945 | `		return PH7_OK;` |
|      - | 5946 | `	}` |
|      - | 5947 | `	/* Perform the requested operation */` |
|     32 | 5948 | `	for(;;){` |
|     65 | 5949 | `		if( zIn >= zEnd ){` |
|      - | 5950 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 5951 | `			ph7_result_bool(pCtx,1);` |
|      9 | 5952 | `			return PH7_OK;` |
|      - | 5953 | `		}` |
|     57 | 5954 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 5955 | `			break;` |
|      - | 5956 | `		}` |
|      - | 5957 | `		/* Point to the next character */` |
|     53 | 5958 | `		zIn++;` |
|      1 | 5959 | `	}` |
|      - | 5960 | `	/* The test failed,return FALSE */` |
|      5 | 5961 | `	ph7_result_bool(pCtx,0);` |
|      5 | 5962 | `	return PH7_OK;` |
|      9 | 5963 |  |
|      - | 5964 | `/*` |
|      - | 5965 | ` * bool ctype_alpha(string $text)` |
|      - | 5966 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 5967 | ` * Parameters` |
|      - | 5968 | ` *  $text` |
|      - | 5969 | ` *   The tested string.` |
|      - | 5970 | ` * Return` |
|      - | 5971 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 5972 | ` */` |
|     18 | 5973 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5974 |  |
|      - | 5975 | `	const unsigned char *zIn,*zEnd;` |
|      - | 5976 | `	int nLen;` |
|     19 | 5977 | `	if( nArg < 1 ){` |
|      - | 5978 | `		/* Missing arguments,return FALSE */` |
|      3 | 5979 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5980 | `		return PH7_OK;` |
|      - | 5981 | `	}` |
|      - | 5982 | `	/* Extract the target string */` |
|     17 | 5983 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 5984 | `	zEnd = &zIn[nLen];` |
|     17 | 5985 | `	if( nLen < 1 ){` |
|      - | 5986 | `		/* Empty string,return FALSE */` |
|      3 | 5987 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5988 | `		return PH7_OK;` |
|      - | 5989 | `	}` |
|      - | 5990 | `	/* Perform the requested operation */` |
|     42 | 5991 | `	for(;;){` |
|     85 | 5992 | `		if( zIn >= zEnd ){` |
|      - | 5993 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 5994 | `			ph7_result_bool(pCtx,1);` |
|      9 | 5995 | `			return PH7_OK;` |
|      - | 5996 | `		}` |
|     77 | 5997 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 5998 | `			break;` |
|      - | 5999 | `		}` |
|      - | 6000 | `		/* Point to the next character */` |
|     71 | 6001 | `		zIn++;` |
|      1 | 6002 | `	}` |
|      - | 6003 | `	/* The test failed,return FALSE */` |
|      7 | 6004 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6005 | `	return PH7_OK;` |
|     10 | 6006 |  |
|      - | 6007 | `/*` |
|      - | 6008 | ` * bool ctype_cntrl(string $text)` |
|      - | 6009 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 6010 | ` * Parameters` |
|      - | 6011 | ` *  $text` |
|      - | 6012 | ` *   The tested string.` |
|      - | 6013 | ` * Return` |
|      - | 6014 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 6015 | ` */` |
|     18 | 6016 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6017 |  |
|      - | 6018 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6019 | `	int nLen;` |
|     19 | 6020 | `	if( nArg < 1 ){` |
|      - | 6021 | `		/* Missing arguments,return FALSE */` |
|      3 | 6022 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6023 | `		return PH7_OK;` |
|      - | 6024 | `	}` |
|      - | 6025 | `	/* Extract the target string */` |
|     17 | 6026 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6027 | `	zEnd = &zIn[nLen];` |
|     17 | 6028 | `	if( nLen < 1 ){` |
|      - | 6029 | `		/* Empty string,return FALSE */` |
|      3 | 6030 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6031 | `		return PH7_OK;` |
|      - | 6032 | `	}` |
|      - | 6033 | `	/* Perform the requested operation */` |
|     14 | 6034 | `	for(;;){` |
|     29 | 6035 | `		if( zIn >= zEnd ){` |
|      - | 6036 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6037 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6038 | `			return PH7_OK;` |
|      - | 6039 | `		}` |
|     21 | 6040 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6041 | `			/* UTF-8 stream  */` |
|    ! 0 | 6042 | `			break;` |
|      - | 6043 | `		}` |
|     21 | 6044 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 6045 | `			break;` |
|      - | 6046 | `		}` |
|      - | 6047 | `		/* Point to the next character */` |
|     15 | 6048 | `		zIn++;` |
|      1 | 6049 | `	}` |
|      - | 6050 | `	/* The test failed,return FALSE */` |
|      7 | 6051 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6052 | `	return PH7_OK;` |
|     10 | 6053 |  |
|      - | 6054 | `/*` |
|      - | 6055 | ` * bool ctype_digit(string $text)` |
|      - | 6056 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 6057 | ` * Parameters` |
|      - | 6058 | ` *  $text` |
|      - | 6059 | ` *   The tested string.` |
|      - | 6060 | ` * Return` |
|      - | 6061 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 6062 | ` */` |
|   1962 | 6063 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6064 |  |
|      - | 6065 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6066 | `	int nLen;` |
|   1964 | 6067 | `	if( nArg < 1 ){` |
|      - | 6068 | `		/* Missing arguments,return FALSE */` |
|      3 | 6069 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6070 | `		return PH7_OK;` |
|      - | 6071 | `	}` |
|      - | 6072 | `	/* Extract the target string */` |
|   1962 | 6073 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1962 | 6074 | `	zEnd = &zIn[nLen];` |
|   1962 | 6075 | `	if( nLen < 1 ){` |
|      - | 6076 | `		/* Empty string,return FALSE */` |
|      3 | 6077 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6078 | `		return PH7_OK;` |
|      - | 6079 | `	}` |
|      - | 6080 | `	/* Perform the requested operation */` |
|   1801 | 6081 | `	for(;;){` |
|   3604 | 6082 | `		if( zIn >= zEnd ){` |
|      - | 6083 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1614 | 6084 | `			ph7_result_bool(pCtx,1);` |
|   1614 | 6085 | `			return PH7_OK;` |
|      - | 6086 | `		}` |
|   1992 | 6087 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6088 | `			/* UTF-8 stream  */` |
|    ! 0 | 6089 | `			break;` |
|      - | 6090 | `		}` |
|   1992 | 6091 | `		if( !SyisDigit(zIn[0]) ){` |
|    348 | 6092 | `			break;` |
|      - | 6093 | `		}` |
|      - | 6094 | `		/* Point to the next character */` |
|   1646 | 6095 | `		zIn++;` |
|      2 | 6096 | `	}` |
|      - | 6097 | `	/* The test failed,return FALSE */` |
|    348 | 6098 | `	ph7_result_bool(pCtx,0);` |
|    348 | 6099 | `	return PH7_OK;` |
|    983 | 6100 |  |
|      - | 6101 | `/*` |
|      - | 6102 | ` * bool ctype_xdigit(string $text)` |
|      - | 6103 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 6104 | ` * Parameters` |
|      - | 6105 | ` *  $text` |
|      - | 6106 | ` *   The tested string.` |
|      - | 6107 | ` * Return` |
|      - | 6108 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 6109 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 6110 | ` */` |
|     20 | 6111 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6112 |  |
|      - | 6113 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6114 | `	int nLen;` |
|     21 | 6115 | `	if( nArg < 1 ){` |
|      - | 6116 | `		/* Missing arguments,return FALSE */` |
|      3 | 6117 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6118 | `		return PH7_OK;` |
|      - | 6119 | `	}` |
|      - | 6120 | `	/* Extract the target string */` |
|     19 | 6121 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 6122 | `	zEnd = &zIn[nLen];` |
|     19 | 6123 | `	if( nLen < 1 ){` |
|      - | 6124 | `		/* Empty string,return FALSE */` |
|      3 | 6125 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6126 | `		return PH7_OK;` |
|      - | 6127 | `	}` |
|      - | 6128 | `	/* Perform the requested operation */` |
|     46 | 6129 | `	for(;;){` |
|     93 | 6130 | `		if( zIn >= zEnd ){` |
|      - | 6131 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 6132 | `			ph7_result_bool(pCtx,1);` |
|     11 | 6133 | `			return PH7_OK;` |
|      - | 6134 | `		}` |
|     83 | 6135 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6136 | `			/* UTF-8 stream  */` |
|    ! 0 | 6137 | `			break;` |
|      - | 6138 | `		}` |
|     83 | 6139 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 6140 | `			break;` |
|      - | 6141 | `		}` |
|      - | 6142 | `		/* Point to the next character */` |
|     77 | 6143 | `		zIn++;` |
|      1 | 6144 | `	}` |
|      - | 6145 | `	/* The test failed,return FALSE */` |
|      7 | 6146 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6147 | `	return PH7_OK;` |
|     11 | 6148 |  |
|      - | 6149 | `/*` |
|      - | 6150 | ` * bool ctype_graph(string $text)` |
|      - | 6151 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 6152 | ` * Parameters` |
|      - | 6153 | ` *  $text` |
|      - | 6154 | ` *   The tested string.` |
|      - | 6155 | ` * Return` |
|      - | 6156 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 6157 | ` * (no white space), FALSE otherwise.` |
|      - | 6158 | ` */` |
|     18 | 6159 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6160 |  |
|      - | 6161 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6162 | `	int nLen;` |
|     19 | 6163 | `	if( nArg < 1 ){` |
|      - | 6164 | `		/* Missing arguments,return FALSE */` |
|      3 | 6165 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6166 | `		return PH7_OK;` |
|      - | 6167 | `	}` |
|      - | 6168 | `	/* Extract the target string */` |
|     17 | 6169 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6170 | `	zEnd = &zIn[nLen];` |
|     17 | 6171 | `	if( nLen < 1 ){` |
|      - | 6172 | `		/* Empty string,return FALSE */` |
|      3 | 6173 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6174 | `		return PH7_OK;` |
|      - | 6175 | `	}` |
|      - | 6176 | `	/* Perform the requested operation */` |
|     57 | 6177 | `	for(;;){` |
|    115 | 6178 | `		if( zIn >= zEnd ){` |
|      - | 6179 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6180 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6181 | `			return PH7_OK;` |
|      - | 6182 | `		}` |
|    107 | 6183 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6184 | `			/* UTF-8 stream  */` |
|    ! 0 | 6185 | `			break;` |
|      - | 6186 | `		}` |
|    107 | 6187 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 6188 | `			break;` |
|      - | 6189 | `		}` |
|      - | 6190 | `		/* Point to the next character */` |
|    101 | 6191 | `		zIn++;` |
|      1 | 6192 | `	}` |
|      - | 6193 | `	/* The test failed,return FALSE */` |
|      7 | 6194 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6195 | `	return PH7_OK;` |
|     10 | 6196 |  |
|      - | 6197 | `/*` |
|      - | 6198 | ` * bool ctype_print(string $text)` |
|      - | 6199 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 6200 | ` * Parameters` |
|      - | 6201 | ` *  $text` |
|      - | 6202 | ` *   The tested string.` |
|      - | 6203 | ` * Return` |
|      - | 6204 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 6205 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 6206 | ` *  or control function at all.` |
|      - | 6207 | ` */` |
|     18 | 6208 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6209 |  |
|      - | 6210 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6211 | `	int nLen;` |
|     19 | 6212 | `	if( nArg < 1 ){` |
|      - | 6213 | `		/* Missing arguments,return FALSE */` |
|      3 | 6214 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6215 | `		return PH7_OK;` |
|      - | 6216 | `	}` |
|      - | 6217 | `	/* Extract the target string */` |
|     17 | 6218 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6219 | `	zEnd = &zIn[nLen];` |
|     17 | 6220 | `	if( nLen < 1 ){` |
|      - | 6221 | `		/* Empty string,return FALSE */` |
|      3 | 6222 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6223 | `		return PH7_OK;` |
|      - | 6224 | `	}` |
|      - | 6225 | `	/* Perform the requested operation */` |
|     63 | 6226 | `	for(;;){` |
|    127 | 6227 | `		if( zIn >= zEnd ){` |
|      - | 6228 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6229 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6230 | `			return PH7_OK;` |
|      - | 6231 | `		}` |
|    119 | 6232 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6233 | `			/* UTF-8 stream  */` |
|    ! 0 | 6234 | `			break;` |
|      - | 6235 | `		}` |
|    119 | 6236 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 6237 | `			break;` |
|      - | 6238 | `		}` |
|      - | 6239 | `		/* Point to the next character */` |
|    113 | 6240 | `		zIn++;` |
|      1 | 6241 | `	}` |
|      - | 6242 | `	/* The test failed,return FALSE */` |
|      7 | 6243 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6244 | `	return PH7_OK;` |
|     10 | 6245 |  |
|      - | 6246 | `/*` |
|      - | 6247 | ` * bool ctype_punct(string $text)` |
|      - | 6248 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 6249 | ` * Parameters` |
|      - | 6250 | ` *  $text` |
|      - | 6251 | ` *   The tested string.` |
|      - | 6252 | ` * Return` |
|      - | 6253 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 6254 | ` *  digit or blank, FALSE otherwise.` |
|      - | 6255 | ` */` |
|     20 | 6256 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6257 |  |
|      - | 6258 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6259 | `	int nLen;` |
|     21 | 6260 | `	if( nArg < 1 ){` |
|      - | 6261 | `		/* Missing arguments,return FALSE */` |
|      3 | 6262 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6263 | `		return PH7_OK;` |
|      - | 6264 | `	}` |
|      - | 6265 | `	/* Extract the target string */` |
|     19 | 6266 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 6267 | `	zEnd = &zIn[nLen];` |
|     19 | 6268 | `	if( nLen < 1 ){` |
|      - | 6269 | `		/* Empty string,return FALSE */` |
|      3 | 6270 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6271 | `		return PH7_OK;` |
|      - | 6272 | `	}` |
|      - | 6273 | `	/* Perform the requested operation */` |
|     38 | 6274 | `	for(;;){` |
|     77 | 6275 | `		if( zIn >= zEnd ){` |
|      - | 6276 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6277 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6278 | `			return PH7_OK;` |
|      - | 6279 | `		}` |
|     69 | 6280 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6281 | `			/* UTF-8 stream  */` |
|    ! 0 | 6282 | `			break;` |
|      - | 6283 | `		}` |
|     69 | 6284 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 6285 | `			break;` |
|      - | 6286 | `		}` |
|      - | 6287 | `		/* Point to the next character */` |
|     61 | 6288 | `		zIn++;` |
|      1 | 6289 | `	}` |
|      - | 6290 | `	/* The test failed,return FALSE */` |
|      9 | 6291 | `	ph7_result_bool(pCtx,0);` |
|      9 | 6292 | `	return PH7_OK;` |
|     11 | 6293 |  |
|      - | 6294 | `/*` |
|      - | 6295 | ` * bool ctype_space(string $text)` |
|      - | 6296 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 6297 | ` * Parameters` |
|      - | 6298 | ` *  $text` |
|      - | 6299 | ` *   The tested string.` |
|      - | 6300 | ` * Return` |
|      - | 6301 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 6302 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 6303 | ` *  and form feed characters.` |
|      - | 6304 | ` */` |
|  77978 | 6305 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6306 |  |
|      - | 6307 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6308 | `	int nLen;` |
|  77980 | 6309 | `	if( nArg < 1 ){` |
|      - | 6310 | `		/* Missing arguments,return FALSE */` |
|      3 | 6311 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6312 | `		return PH7_OK;` |
|      - | 6313 | `	}` |
|      - | 6314 | `	/* Extract the target string */` |
|  77978 | 6315 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  77978 | 6316 | `	zEnd = &zIn[nLen];` |
|  77978 | 6317 | `	if( nLen < 1 ){` |
|      - | 6318 | `		/* Empty string,return FALSE */` |
|      3 | 6319 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6320 | `		return PH7_OK;` |
|      - | 6321 | `	}` |
|      - | 6322 | `	/* Perform the requested operation */` |
|  39769 | 6323 | `	for(;;){` |
|  79496 | 6324 | `		if( zIn >= zEnd ){` |
|      - | 6325 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1498 | 6326 | `			ph7_result_bool(pCtx,1);` |
|   1498 | 6327 | `			return PH7_OK;` |
|      - | 6328 | `		}` |
|  78000 | 6329 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6330 | `			/* UTF-8 stream  */` |
|    ! 0 | 6331 | `			break;` |
|      - | 6332 | `		}` |
|  78000 | 6333 | `		if( !SyisSpace(zIn[0]) ){` |
|  76480 | 6334 | `			break;` |
|      - | 6335 | `		}` |
|      - | 6336 | `		/* Point to the next character */` |
|   1522 | 6337 | `		zIn++;` |
|      2 | 6338 | `	}` |
|      - | 6339 | `	/* The test failed,return FALSE */` |
|  76480 | 6340 | `	ph7_result_bool(pCtx,0);` |
|  76480 | 6341 | `	return PH7_OK;` |
|  39013 | 6342 |  |
|      - | 6343 | `/*` |
|      - | 6344 | ` * bool ctype_lower(string $text)` |
|      - | 6345 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 6346 | ` * Parameters` |
|      - | 6347 | ` *  $text` |
|      - | 6348 | ` *   The tested string.` |
|      - | 6349 | ` * Return` |
|      - | 6350 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 6351 | ` */` |
|     18 | 6352 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6353 |  |
|      - | 6354 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6355 | `	int nLen;` |
|     19 | 6356 | `	if( nArg < 1 ){` |
|      - | 6357 | `		/* Missing arguments,return FALSE */` |
|      3 | 6358 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6359 | `		return PH7_OK;` |
|      - | 6360 | `	}` |
|      - | 6361 | `	/* Extract the target string */` |
|     17 | 6362 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6363 | `	zEnd = &zIn[nLen];` |
|     17 | 6364 | `	if( nLen < 1 ){` |
|      - | 6365 | `		/* Empty string,return FALSE */` |
|      3 | 6366 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6367 | `		return PH7_OK;` |
|      - | 6368 | `	}` |
|      - | 6369 | `	/* Perform the requested operation */` |
|     27 | 6370 | `	for(;;){` |
|     55 | 6371 | `		if( zIn >= zEnd ){` |
|      - | 6372 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 6373 | `			ph7_result_bool(pCtx,1);` |
|      5 | 6374 | `			return PH7_OK;` |
|      - | 6375 | `		}` |
|     51 | 6376 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 6377 | `			break;` |
|      - | 6378 | `		}` |
|      - | 6379 | `		/* Point to the next character */` |
|     41 | 6380 | `		zIn++;` |
|      1 | 6381 | `	}` |
|      - | 6382 | `	/* The test failed,return FALSE */` |
|     11 | 6383 | `	ph7_result_bool(pCtx,0);` |
|     11 | 6384 | `	return PH7_OK;` |
|     10 | 6385 |  |
|      - | 6386 | `/*` |
|      - | 6387 | ` * bool ctype_upper(string $text)` |
|      - | 6388 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 6389 | ` * Parameters` |
|      - | 6390 | ` *  $text` |
|      - | 6391 | ` *   The tested string.` |
|      - | 6392 | ` * Return` |
|      - | 6393 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 6394 | ` */` |
|     18 | 6395 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6396 |  |
|      - | 6397 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6398 | `	int nLen;` |
|     19 | 6399 | `	if( nArg < 1 ){` |
|      - | 6400 | `		/* Missing arguments,return FALSE */` |
|      3 | 6401 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6402 | `		return PH7_OK;` |
|      - | 6403 | `	}` |
|      - | 6404 | `	/* Extract the target string */` |
|     17 | 6405 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6406 | `	zEnd = &zIn[nLen];` |
|     17 | 6407 | `	if( nLen < 1 ){` |
|      - | 6408 | `		/* Empty string,return FALSE */` |
|      3 | 6409 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6410 | `		return PH7_OK;` |
|      - | 6411 | `	}` |
|      - | 6412 | `	/* Perform the requested operation */` |
|     28 | 6413 | `	for(;;){` |
|     57 | 6414 | `		if( zIn >= zEnd ){` |
|      - | 6415 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 6416 | `			ph7_result_bool(pCtx,1);` |
|      5 | 6417 | `			return PH7_OK;` |
|      - | 6418 | `		}` |
|     53 | 6419 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 6420 | `			break;` |
|      - | 6421 | `		}` |
|      - | 6422 | `		/* Point to the next character */` |
|     43 | 6423 | `		zIn++;` |
|      1 | 6424 | `	}` |
|      - | 6425 | `	/* The test failed,return FALSE */` |
|     11 | 6426 | `	ph7_result_bool(pCtx,0);` |
|     11 | 6427 | `	return PH7_OK;` |
|     10 | 6428 |  |
|      - | 6429 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 6430 | `/*` |
|      - | 6431 | ` * Section:` |
|      - | 6432 | ` *    URL handling Functions.` |
|      - | 6433 | ` * Status:` |
|      - | 6434 | ` *    Stable.` |
|      - | 6435 | ` */` |
|      - | 6436 | `/*` |
|      - | 6437 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 6438 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 6439 | ` */` |
|   1026 | 6440 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 6441 |  |
|      - | 6442 | `	/* Store in the call context result buffer */` |
|   1028 | 6443 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 6444 | `	return SXRET_OK;` |
|      2 | 6445 |  |
|      - | 6446 | `/*` |
|      - | 6447 | ` * string base64_encode(string $data)` |
|      - | 6448 | ` * string convert_uuencode(string $data)` |
|      - | 6449 | ` *  Encodes data with MIME base64` |
|      - | 6450 | ` * Parameter` |
|      - | 6451 | ` *  $data` |
|      - | 6452 | ` *    Data to encode` |
|      - | 6453 | ` * Return` |
|      - | 6454 | ` *  Encoded data or FALSE on failure.` |
|      - | 6455 | ` */` |
|     10 | 6456 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6457 |  |
|      - | 6458 | `	const char *zIn;` |
|      - | 6459 | `	int nLen;` |
|     11 | 6460 | `	if( nArg < 1 ){` |
|      - | 6461 | `		/* Missing arguments,return FALSE */` |
|      5 | 6462 | `		ph7_result_bool(pCtx,0);` |
|      5 | 6463 | `		return PH7_OK;` |
|      - | 6464 | `	}` |
|      - | 6465 | `	/* Extract the input string */` |
|      7 | 6466 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 6467 | `	if( nLen < 1 ){` |
|      - | 6468 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 6469 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6470 | `		return PH7_OK;` |
|      - | 6471 | `	}` |
|      - | 6472 | `	/* Perform the BASE64 encoding */` |
|      7 | 6473 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 6474 | `	return PH7_OK;` |
|      6 | 6475 |  |
|      - | 6476 | `/*` |
|      - | 6477 | ` * string base64_decode(string $data)` |
|      - | 6478 | ` * string convert_uudecode(string $data)` |
|      - | 6479 | ` *  Decodes data encoded with MIME base64` |
|      - | 6480 | ` * Parameter` |
|      - | 6481 | ` *  $data` |
|      - | 6482 | ` *    Encoded data.` |
|      - | 6483 | ` * Return` |
|      - | 6484 | ` *  Returns the original data or FALSE on failure.` |
|      - | 6485 | ` */` |
|     36 | 6486 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6487 |  |
|      - | 6488 | `	const char *zIn;` |
|      - | 6489 | `	int nLen;` |
|     38 | 6490 | `	if( nArg < 1 ){` |
|      - | 6491 | `		/* Missing arguments,return FALSE */` |
|      3 | 6492 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6493 | `		return PH7_OK;` |
|      - | 6494 | `	}` |
|      - | 6495 | `	/* Extract the input string */` |
|     36 | 6496 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 6497 | `	if( nLen < 1 ){` |
|      - | 6498 | `		/* Nothing to process,return FALSE */` |
|      3 | 6499 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6500 | `		return PH7_OK;` |
|      - | 6501 | `	}` |
|      - | 6502 | `	/* Perform the BASE64 decoding */` |
|     34 | 6503 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 6504 | `	return PH7_OK;` |
|     20 | 6505 |  |
|      - | 6506 | `/*` |
|      - | 6507 | ` * string urlencode(string $str)` |
|      - | 6508 | ` *  URL encoding` |
|      - | 6509 | ` * Parameter` |
|      - | 6510 | ` *  $data` |
|      - | 6511 | ` *   Input string.` |
|      - | 6512 | ` * Return` |
|      - | 6513 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 6514 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 6515 | ` *  encoded as plus (+) signs.` |
|      - | 6516 | ` */` |
|      6 | 6517 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6518 |  |
|      - | 6519 | `	const char *zIn;` |
|      - | 6520 | `	int nLen;` |
|      7 | 6521 | `	if( nArg < 1 ){` |
|      - | 6522 | `		/* Missing arguments,return FALSE */` |
|      3 | 6523 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6524 | `		return PH7_OK;` |
|      - | 6525 | `	}` |
|      - | 6526 | `	/* Extract the input string */` |
|      5 | 6527 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 6528 | `	if( nLen < 1 ){` |
|      - | 6529 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 6530 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6531 | `		return PH7_OK;` |
|      - | 6532 | `	}` |
|      - | 6533 | `	/* Perform the URL encoding */` |
|      5 | 6534 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 6535 | `	return PH7_OK;` |
|      4 | 6536 |  |
|      - | 6537 | `/*` |
|      - | 6538 | ` * string urldecode(string $str)` |
|      - | 6539 | ` *  Decodes any %## encoding in the given string.` |
|      - | 6540 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 6541 | ` * Parameter` |
|      - | 6542 | ` *  $data` |
|      - | 6543 | ` *    Input string.` |
|      - | 6544 | ` * Return` |
|      - | 6545 | ` *  Decoded URL or FALSE on failure.` |
|      - | 6546 | ` */` |
|      8 | 6547 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6548 |  |
|      - | 6549 | `	const char *zIn;` |
|      - | 6550 | `	int nLen;` |
|      9 | 6551 | `	if( nArg < 1 ){` |
|      - | 6552 | `		/* Missing arguments,return FALSE */` |
|      3 | 6553 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6554 | `		return PH7_OK;` |
|      - | 6555 | `	}` |
|      - | 6556 | `	/* Extract the input string */` |
|      7 | 6557 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 6558 | `	if( nLen < 1 ){` |
|      - | 6559 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 6560 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6561 | `		return PH7_OK;` |
|      - | 6562 | `	}` |
|      - | 6563 | `	/* Perform the URL decoding */` |
|      7 | 6564 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 6565 | `	return PH7_OK;` |
|      5 | 6566 |  |
|      - | 6567 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6568 | `/* Table of the built-in functions */` |
|      - | 6569 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 6570 | `	   /* Variable handling functions */` |
|      - | 6571 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 6572 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 6573 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 6574 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 6575 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 6576 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 6577 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 6578 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 6579 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 6580 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 6581 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 6582 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 6583 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 6584 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 6585 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 6586 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 6587 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 6588 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 6589 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 6590 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6591 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 6592 | `	   /* Math functions */` |
|      - | 6593 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 6594 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 6595 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 6596 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 6597 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 6598 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 6599 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 6600 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 6601 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 6602 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 6603 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 6604 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 6605 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 6606 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 6607 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 6608 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 6609 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 6610 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 6611 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 6612 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 6613 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 6614 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 6615 | `	{ "round",    PH7_builtin_round        },` |
|      - | 6616 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 6617 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 6618 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 6619 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 6620 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 6621 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 6622 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 6623 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 6624 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6625 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6626 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 6627 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6628 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6629 | `	   /* String handling functions */` |
|      - | 6630 |  |
|      - | 6631 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 6632 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 6633 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 6634 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 6635 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 6636 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 6637 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 6638 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 6639 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 6640 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 6641 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 6642 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 6643 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 6644 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 6645 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 6646 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 6647 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 6648 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 6649 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 6650 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 6651 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 6652 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 6653 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 6654 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 6655 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 6656 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 6657 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 6658 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 6659 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 6660 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 6661 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 6662 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 6663 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 6664 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 6665 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 6666 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 6667 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 6668 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 6669 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 6670 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 6671 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 6672 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 6673 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 6674 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 6675 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 6676 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 6677 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 6678 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 6679 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 6680 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6681 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6682 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 6683 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 6684 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 6685 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 6686 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6687 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6688 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 6689 |  |
|      - | 6690 |  |
|      - | 6691 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 6692 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 6693 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 6694 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 6695 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 6696 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6697 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6698 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 6699 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 6700 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6701 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6702 |  |
|      - | 6703 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 6704 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 6705 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 6706 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 6707 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 6708 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 6709 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 6710 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 6711 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 6712 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 6713 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 6714 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 6715 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6716 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6717 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 6718 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6719 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6720 |  |
|      - | 6721 | `	         /* Ctype functions */` |
|      - | 6722 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 6723 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 6724 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 6725 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 6726 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 6727 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 6728 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 6729 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 6730 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 6731 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 6732 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 6733 | `	         /* Time functions */` |
|      - | 6734 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 6735 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 6736 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 6737 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 6738 | `	{ "date",        PH7_builtin_date         },` |
|      - | 6739 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 6740 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 6741 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 6742 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 6743 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 6744 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 6745 | `	        /* URL functions */` |
|      - | 6746 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 6747 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 6748 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 6749 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 6750 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 6751 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 6752 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 6753 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 6754 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6755 | `};` |
|      - | 6756 | `/*` |
|      - | 6757 | ` * Register the built-in functions defined above,the array functions` |
|      - | 6758 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 6759 | ` */` |
|   1934 | 6760 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      2 | 6761 |  |
|      - | 6762 | `	sxu32 n;` |
| 295904 | 6763 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 293970 | 6764 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 146986 | 6765 | `	}` |
|      - | 6766 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   1936 | 6767 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 6768 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   1936 | 6769 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   1936 | 6770 |  |
|      - | 6771 |  |
