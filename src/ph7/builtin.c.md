# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 229/339 lines (67.55%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `/* filter_var(FILTER_VALIDATE_FLOAT) parses with libc strtod directly because it` |
|      - |    8 | ` * needs errno==ERANGE to reject out-of-range magnitudes; SyStrToReal (also` |
|      - |    9 | ` * strtod-backed nowadays) exposes no range-error signal. */` |
|      - |   10 | `#include <stdlib.h>  /* strtod */` |
|      - |   11 | `#include <math.h>    /* HUGE_VAL */` |
|      - |   12 | `#include <errno.h>   /* ERANGE (strtod range-error signal) */` |
|      - |   13 | `#include <stdio.h>   /* snprintf (printf-family float conversions — correctly` |
|      - |   14 | `                      * rounded digits like php's zend_dtoa; see PH7_InputFormat) */` |
|      - |   15 | ``/* Shared ZPP helper for `int` parameters — defined OUTSIDE the`` |
|      - |   16 | ` * PH7_DISABLE_BUILTIN_FUNC guard because hashmap.c (array_slice) and` |
|      - |   17 | ` * builtin_math.c (intdiv) call it and both compile in the tiny build. */` |
| 498441 |   18 | `PH7_PRIVATE sxi32 PH7_IntArgResolve(` |
|      - |   19 | `	ph7_context *pCtx,` |
|      - |   20 | `	ph7_value *pArg,` |
|      - |   21 | `	const char *zFunc,` |
|      - |   22 | `	int iArgNum,` |
|      - |   23 | `	const char *zParamName,` |
|      - |   24 | `	const char *zTypeStr,` |
|      - |   25 | `	sxi64 *pOut` |
|      5 |   26 | `){` |
| 498446 |   27 | `	if( ph7_value_is_null(pArg) ){` |
|      - |   28 | `		/* php only DEPRECATES passing null to a non-nullable internal param; PHL` |
|      - |   29 | `		 * targets php's non-deprecated surface and rejects it with the TypeError` |
|      - |   30 | `		 * php will eventually raise. */` |
|    ! 0 |   31 | `		return PH7_VmThrowException(pCtx,` |
|      - |   32 | `			"TypeError",` |
|      - |   33 | `			"%s(): Argument #%d (%s) must be of type %s, null given",` |
|    ! 0 |   34 | `			zFunc,iArgNum,zParamName,zTypeStr` |
|      - |   35 | `			);` |
|      - |   36 | `	}` |
| 498446 |   37 | `	if( ph7_value_is_float(pArg) ){` |
|     16 |   38 | `		double dVal = ph7_value_to_double(pArg);` |
|      - |   39 | `		sxi64 iVal;` |
|      - |   40 | `		/* php: NAN/INF/out-of-int64-range floats fail ZPP outright */` |
|     16 |   41 | `		if( dVal != dVal \|\| dVal >= 9223372036854775808.0 \|\| dVal < -9223372036854775808.0 ){` |
|      7 |   42 | `			return PH7_VmThrowException(pCtx,` |
|      - |   43 | `				"TypeError",` |
|      - |   44 | `				"%s(): Argument #%d (%s) must be of type %s, float given",` |
|      2 |   45 | `				zFunc,iArgNum,zParamName,zTypeStr` |
|      - |   46 | `				);` |
|      - |   47 | `		}` |
|     12 |   48 | `		iVal = (sxi64)dVal;` |
|     12 |   49 | `		if( (double)iVal != dVal ){` |
|      - |   50 | `			/* php DEPRECATES a lossy float->int; PHL rejects it (the value is not` |
|      - |   51 | `			 * representable as int). */` |
|      7 |   52 | `			return PH7_VmThrowException(pCtx,` |
|      - |   53 | `				"TypeError",` |
|      - |   54 | `				"%s(): Argument #%d (%s) must be of type %s, float given",` |
|      2 |   55 | `				zFunc,iArgNum,zParamName,zTypeStr` |
|      - |   56 | `				);` |
|      - |   57 | `		}` |
|      8 |   58 | `		*pOut = iVal;` |
|      8 |   59 | `		return PH7_OK;` |
|      - |   60 | `	}` |
| 498432 |   61 | `	if( ph7_value_is_string(pArg) ){` |
|      - |   62 | `		const char *zNum;` |
|      - |   63 | `		int nSlen;` |
|     18 |   64 | `		int i,bFloat = 0;` |
|     18 |   65 | `		if( !PH7_MemObjStringIsNumeric(pArg) ){` |
|     16 |   66 | `			return PH7_VmThrowException(pCtx,` |
|      - |   67 | `				"TypeError",` |
|      - |   68 | `				"%s(): Argument #%d (%s) must be of type %s, string given",` |
|      5 |   69 | `				zFunc,iArgNum,zParamName,zTypeStr` |
|      - |   70 | `				);` |
|      - |   71 | `		}` |
|      8 |   72 | `		zNum = ph7_value_to_string(pArg,&nSlen);` |
|     14 |   73 | `		for( i = 0 ; i < nSlen ; i++ ){` |
|     10 |   74 | `			if( zNum[i] == '.' \|\| zNum[i] == 'e' \|\| zNum[i] == 'E' ){` |
|      3 |   75 | `				bFloat = 1;` |
|      3 |   76 | `				break;` |
|      - |   77 | `			}` |
|      5 |   78 | `		}` |
|      8 |   79 | `		if( bFloat ){` |
|      3 |   80 | `			double dVal = 0;` |
|      - |   81 | `			sxi64 iVal;` |
|      3 |   82 | `			SyStrToReal(zNum,(sxu32)nSlen,(void *)&dVal,0);` |
|      3 |   83 | `			if( dVal != dVal \|\| dVal >= 9223372036854775808.0 \|\| dVal < -9223372036854775808.0 ){` |
|    ! 0 |   84 | `				return PH7_VmThrowException(pCtx,` |
|      - |   85 | `					"TypeError",` |
|      - |   86 | `					"%s(): Argument #%d (%s) must be of type %s, string given",` |
|    ! 0 |   87 | `					zFunc,iArgNum,zParamName,zTypeStr` |
|      - |   88 | `					);` |
|      - |   89 | `			}` |
|      3 |   90 | `			iVal = (sxi64)dVal;` |
|      3 |   91 | `			if( (double)iVal != dVal ){` |
|      - |   92 | `				/* php DEPRECATES a lossy float-string->int; PHL rejects it. */` |
|      4 |   93 | `				return PH7_VmThrowException(pCtx,` |
|      - |   94 | `					"TypeError",` |
|      - |   95 | `					"%s(): Argument #%d (%s) must be of type %s, string given",` |
|      1 |   96 | `					zFunc,iArgNum,zParamName,zTypeStr` |
|      - |   97 | `					);` |
|      - |   98 | `			}` |
|    ! 0 |   99 | `			*pOut = iVal;` |
|    ! 0 |  100 | `			return PH7_OK;` |
|      - |  101 | `		}` |
|      5 |  102 | `		*pOut = ph7_value_to_int64(pArg);` |
|      5 |  103 | `		return PH7_OK;` |
|      - |  104 | `	}` |
| 498416 |  105 | `	if( !ph7_value_is_int(pArg) && !ph7_value_is_bool(pArg) ){` |
|      - |  106 | `		/* Arrays, resources and objects: php names the class for objects */` |
|    ! 0 |  107 | `		const char *zType = ph7_type_name(pArg);` |
|    ! 0 |  108 | `		if( ph7_value_is_object(pArg) ){` |
|    ! 0 |  109 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|    ! 0 |  110 | `			if( pInst && pInst->pClass ){` |
|    ! 0 |  111 | `				zType = SyStringData(&pInst->pClass->sName);` |
|    ! 0 |  112 | `			}` |
|    ! 0 |  113 | `		}` |
|    ! 0 |  114 | `		return PH7_VmThrowException(pCtx,` |
|      - |  115 | `			"TypeError",` |
|      - |  116 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|    ! 0 |  117 | `			zFunc,iArgNum,zParamName,zTypeStr,zType` |
|      - |  118 | `			);` |
|      - |  119 | `	}` |
| 498416 |  120 | `	*pOut = ph7_value_to_int64(pArg);` |
| 498416 |  121 | `	return PH7_OK;` |
| 249330 |  122 | `}` |
|      - |  123 |  |
|      - |  124 | `/* This file implement built-in 'foreign' functions for the PH7 engine */` |
|      - |  125 | `/*` |
|      - |  126 | ` * Section:` |
|      - |  127 | ` *    Variable handling Functions.` |
|      - |  128 | ` * Status:` |
|      - |  129 | ` *    Stable.` |
|      - |  130 | ` */` |
|      - |  131 | `/*` |
|      - |  132 | ` * bool is_bool($var)` |
|      - |  133 | ` *  Finds out whether a variable is a boolean.` |
|      - |  134 | ` * Parameters` |
|      - |  135 | ` *   $var: The variable being evaluated.` |
|      - |  136 | ` * Return` |
|      - |  137 | ` *  TRUE if var is a boolean. False otherwise.` |
|      - |  138 | ` */` |
|     76 |  139 | `static int PH7_builtin_is_bool(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  140 | `{` |
|     78 |  141 | `	int res = 0; /* Assume false by default */` |
|     78 |  142 | `	if( nArg > 0 ){` |
|     78 |  143 | `		res = ph7_value_is_bool(apArg[0]);` |
|     38 |  144 | `	}` |
|      - |  145 | `	/* Query result */` |
|     78 |  146 | `	ph7_result_bool(pCtx,res);` |
|     78 |  147 | `	return PH7_OK;` |
|      2 |  148 | `}` |
|      - |  149 | `/*` |
|      - |  150 | ` * bool is_float($var)` |
|      - |  151 | ` * bool is_real($var)` |
|      - |  152 | ` * bool is_double($var)` |
|      - |  153 | ` *  Finds out whether a variable is a float.` |
|      - |  154 | ` * Parameters` |
|      - |  155 | ` *   $var: The variable being evaluated.` |
|      - |  156 | ` * Return` |
|      - |  157 | ` *  TRUE if var is a float. False otherwise.` |
|      - |  158 | ` */` |
|    310 |  159 | `static int PH7_builtin_is_float(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  160 | `{` |
|    311 |  161 | `	int res = 0; /* Assume false by default */` |
|    311 |  162 | `	if( nArg > 0 ){` |
|    311 |  163 | `		res = ph7_value_is_float(apArg[0]);` |
|    155 |  164 | `	}` |
|      - |  165 | `	/* Query result */` |
|    311 |  166 | `	ph7_result_bool(pCtx,res);` |
|    311 |  167 | `	return PH7_OK;` |
|      1 |  168 | `}` |
|      - |  169 | `/*` |
|      - |  170 | ` * bool is_int($var)` |
|      - |  171 | ` * bool is_integer($var)` |
|      - |  172 | ` * bool is_long($var)` |
|      - |  173 | ` *  Finds out whether a variable is an integer.` |
|      - |  174 | ` * Parameters` |
|      - |  175 | ` *   $var: The variable being evaluated.` |
|      - |  176 | ` * Return` |
|      - |  177 | ` *  TRUE if var is an integer. False otherwise.` |
|      - |  178 | ` */` |
|    928 |  179 | `static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  180 | `{` |
|    932 |  181 | `	int res = 0; /* Assume false by default */` |
|    932 |  182 | `	if( nArg > 0 ){` |
|      - |  183 | `		/* Strict PHP identity: a float is never an int, even when it holds an` |
|      - |  184 | `		 * integer value (1.0). An integer-valued real carries both MEMOBJ_INT` |
|      - |  185 | `		 * (cached) and MEMOBJ_REAL, so REAL must be excluded here. */` |
|    932 |  186 | `		res = ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]);` |
|    464 |  187 | `	}` |
|      - |  188 | `	/* Query result */` |
|    932 |  189 | `	ph7_result_bool(pCtx,res);` |
|    932 |  190 | `	return PH7_OK;` |
|      4 |  191 | `}` |
|      - |  192 | `/*` |
|      - |  193 | ` * bool is_string($var)` |
|      - |  194 | ` *  Finds out whether a variable is a string.` |
|      - |  195 | ` * Parameters` |
|      - |  196 | ` *   $var: The variable being evaluated.` |
|      - |  197 | ` * Return` |
|      - |  198 | ` *  TRUE if var is string. False otherwise.` |
|      - |  199 | ` */` |
|   1104 |  200 | `static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  201 | `{` |
|   1108 |  202 | `	int res = 0; /* Assume false by default */` |
|   1108 |  203 | `	if( nArg > 0 ){` |
|   1108 |  204 | `		res = ph7_value_is_string(apArg[0]);` |
|    552 |  205 | `	}` |
|      - |  206 | `	/* Query result */` |
|   1108 |  207 | `	ph7_result_bool(pCtx,res);` |
|   1108 |  208 | `	return PH7_OK;` |
|      4 |  209 | `}` |
|      - |  210 | `/*` |
|      - |  211 | ` * bool is_null($var)` |
|      - |  212 | ` *  Finds out whether a variable is NULL.` |
|      - |  213 | ` * Parameters` |
|      - |  214 | ` *   $var: The variable being evaluated.` |
|      - |  215 | ` * Return` |
|      - |  216 | ` *  TRUE if var is NULL. False otherwise.` |
|      - |  217 | ` */` |
|     84 |  218 | `static int PH7_builtin_is_null(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  219 | `{` |
|     86 |  220 | `	int res = 0; /* Assume false by default */` |
|     86 |  221 | `	if( nArg > 0 ){` |
|     86 |  222 | `		res = ph7_value_is_null(apArg[0]);` |
|     42 |  223 | `	}` |
|      - |  224 | `	/* Query result */` |
|     86 |  225 | `	ph7_result_bool(pCtx,res);` |
|     86 |  226 | `	return PH7_OK;` |
|      2 |  227 | `}` |
|      - |  228 | `/*` |
|      - |  229 | ` * bool is_numeric($var)` |
|      - |  230 | ` *  Find out whether a variable is NULL.` |
|      - |  231 | ` * Parameters` |
|      - |  232 | ` *  $var: The variable being evaluated.` |
|      - |  233 | ` * Return` |
|      - |  234 | ` *  True if var is numeric. False otherwise.` |
|      - |  235 | ` */` |
|     94 |  236 | `static int PH7_builtin_is_numeric(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  237 | `{` |
|     99 |  238 | `	int res = 0; /* Assume false by default */` |
|     99 |  239 | `	if( nArg > 0 ){` |
|      - |  240 | `		/* Strict PHP semantics: only int/float and numeric strings are numeric.` |
|      - |  241 | `		 * PHL's lenient helper also reports booleans as numeric (they coerce for` |
|      - |  242 | `		 * arithmetic), but php's is_numeric() rejects true/false, so exclude` |
|      - |  243 | `		 * MEMOBJ_BOOL here. */` |
|     99 |  244 | `		res = ph7_value_is_numeric(apArg[0]) && !ph7_value_is_bool(apArg[0]);` |
|     47 |  245 | `	}` |
|      - |  246 | `	/* Query result */` |
|     99 |  247 | `	ph7_result_bool(pCtx,res);` |
|     99 |  248 | `	return PH7_OK;` |
|      5 |  249 | `}` |
|      - |  250 | `/*` |
|      - |  251 | ` * bool is_scalar($var)` |
|      - |  252 | ` *  Find out whether a variable is a scalar.` |
|      - |  253 | ` * Parameters` |
|      - |  254 | ` *  $var: The variable being evaluated.` |
|      - |  255 | ` * Return` |
|      - |  256 | ` *  True if var is scalar. False otherwise.` |
|      - |  257 | ` */` |
|     30 |  258 | `static int PH7_builtin_is_scalar(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  259 | `{` |
|     31 |  260 | `	int res = 0; /* Assume false by default */` |
|     31 |  261 | `	if( nArg > 0 ){` |
|      - |  262 | `		/* Strict PHP semantics: scalars are int/float/string/bool. PHL's` |
|      - |  263 | `		 * MEMOBJ_SCALAR bucket also includes NULL, but php's is_scalar(null) is` |
|      - |  264 | `		 * false, so exclude the NULL case. */` |
|     31 |  265 | `		res = ph7_value_is_scalar(apArg[0]) && !ph7_value_is_null(apArg[0]);` |
|     15 |  266 | `	}` |
|      - |  267 | `	/* Query result */` |
|     31 |  268 | `	ph7_result_bool(pCtx,res);` |
|     31 |  269 | `	return PH7_OK;` |
|      1 |  270 | `}` |
|      - |  271 | `/*` |
|      - |  272 | ` * bool is_array($var)` |
|      - |  273 | ` *  Find out whether a variable is an array.` |
|      - |  274 | ` * Parameters` |
|      - |  275 | ` *  $var: The variable being evaluated.` |
|      - |  276 | ` * Return` |
|      - |  277 | ` *  True if var is an array. False otherwise.` |
|      - |  278 | ` */` |
|   1388 |  279 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  280 | `{` |
|   1393 |  281 | `	int res = 0; /* Assume false by default */` |
|   1393 |  282 | `	if( nArg > 0 ){` |
|   1393 |  283 | `		res = ph7_value_is_array(apArg[0]);` |
|    694 |  284 | `	}` |
|      - |  285 | `	/* Query result */` |
|   1393 |  286 | `	ph7_result_bool(pCtx,res);` |
|   1393 |  287 | `	return PH7_OK;` |
|      5 |  288 | `}` |
|      - |  289 | `/*` |
|      - |  290 | ` * bool is_object($var)` |
|      - |  291 | ` *  Find out whether a variable is an object.` |
|      - |  292 | ` * Parameters` |
|      - |  293 | ` *  $var: The variable being evaluated.` |
|      - |  294 | ` * Return` |
|      - |  295 | ` *  True if var is an object. False otherwise.` |
|      - |  296 | ` */` |
|   1598 |  297 | `static int PH7_builtin_is_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  298 | `{` |
|   1602 |  299 | `	int res = 0; /* Assume false by default */` |
|   1602 |  300 | `	if( nArg > 0 ){` |
|   1602 |  301 | `		res = ph7_value_is_object(apArg[0]);` |
|    799 |  302 | `	}` |
|      - |  303 | `	/* Query result */` |
|   1602 |  304 | `	ph7_result_bool(pCtx,res);` |
|   1602 |  305 | `	return PH7_OK;` |
|      4 |  306 | `}` |
|      - |  307 | `/*` |
|      - |  308 | ` * bool is_resource($var)` |
|      - |  309 | ` *  Find out whether a variable is a resource.` |
|      - |  310 | ` * Parameters` |
|      - |  311 | ` *  $var: The variable being evaluated.` |
|      - |  312 | ` * Return` |
|      - |  313 | ` *  True if a resource. False otherwise.` |
|      - |  314 | ` */` |
|     30 |  315 | `static int PH7_builtin_is_resource(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  316 | `{` |
|     32 |  317 | `	int res = 0; /* Assume false by default */` |
|     32 |  318 | `	if( nArg > 0 && ph7_value_is_resource(apArg[0]) ){` |
|      - |  319 | `		/* A handle closed via fclose()/closedir()/pclose() is no longer a` |
|      - |  320 | `		 * live resource — php's is_resource() returns false for it. */` |
|     32 |  321 | `		res = !PH7_VfsResourceIsClosed(apArg[0]->x.pOther);` |
|     15 |  322 | `	}` |
|     32 |  323 | `	ph7_result_bool(pCtx,res);` |
|     32 |  324 | `	return PH7_OK;` |
|      2 |  325 | `}` |
|      - |  326 | `/*` |
|      - |  327 | ` * float floatval($var)` |
|      - |  328 | ` *  Get float value of a variable.` |
|      - |  329 | ` * Parameter` |
|      - |  330 | ` *  $var: The variable being processed.` |
|      - |  331 | ` * Return` |
|      - |  332 | ` *  the float value of a variable.` |
|      - |  333 | ` */` |
|      4 |  334 | `static int PH7_builtin_floatval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  335 | `{` |
|      5 |  336 | `	if( nArg < 1 ){` |
|      - |  337 | `		/* return 0.0 */` |
|    ! 0 |  338 | `		ph7_result_double(pCtx,0);` |
|    ! 0 |  339 | `	}else{` |
|      - |  340 | `		double dval;` |
|      - |  341 | `		/* Perform the cast */` |
|      5 |  342 | `		dval = ph7_value_to_double(apArg[0]);` |
|      5 |  343 | `		ph7_result_double(pCtx,dval);` |
|      - |  344 | `	}` |
|      5 |  345 | `	return PH7_OK;` |
|      1 |  346 | `}` |
|      - |  347 | `/*` |
|      - |  348 | ` * int intval($var)` |
|      - |  349 | ` *  Get integer value of a variable.` |
|      - |  350 | ` * Parameter` |
|      - |  351 | ` *  $var: The variable being processed.` |
|      - |  352 | ` * Return` |
|      - |  353 | ` *  the int value of a variable.` |
|      - |  354 | ` */` |
|     50 |  355 | `static int PH7_builtin_intval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  356 | `{` |
|     51 |  357 | `	if( nArg < 1 ){` |
|      - |  358 | `		/* return 0 */` |
|    ! 0 |  359 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  360 | `	}else{` |
|      - |  361 | `		sxi64 iVal;` |
|      - |  362 | `		/* Perform the cast */` |
|     51 |  363 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|     51 |  364 | `		ph7_result_int64(pCtx,iVal);` |
|      - |  365 | `	}` |
|     51 |  366 | `	return PH7_OK;` |
|      1 |  367 | `}` |
|      - |  368 | `/*` |
|      - |  369 | ` * string strval($var)` |
|      - |  370 | ` *  Get the string representation of a variable.` |
|      - |  371 | ` * Parameter` |
|      - |  372 | ` *  $var: The variable being processed.` |
|      - |  373 | ` * Return` |
|      - |  374 | ` *  the string value of a variable.` |
|      - |  375 | ` */` |
|      2 |  376 | `static int PH7_builtin_strval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  377 | `{` |
|      3 |  378 | `	if( nArg < 1 ){` |
|      - |  379 | `		/* return NULL */` |
|    ! 0 |  380 | `		ph7_result_null(pCtx);` |
|    ! 0 |  381 | `	}else{` |
|      - |  382 | `		const char *zVal;` |
|      3 |  383 | `		int iLen = 0; /* cc -O6 warning */` |
|      - |  384 | `		/* Perform the cast */` |
|      3 |  385 | `		zVal = ph7_value_to_string(apArg[0],&iLen);` |
|      3 |  386 | `		ph7_result_string(pCtx,zVal,iLen);` |
|      - |  387 | `	}` |
|      3 |  388 | `	return PH7_OK;` |
|      1 |  389 | `}` |
|      - |  390 | `/*` |
|      - |  391 | ` * bool boolval($var)` |
|      - |  392 | ` *  Get the boolean value of a variable.` |
|      - |  393 | ` * Parameter` |
|      - |  394 | ` *  $var: The variable being processed.` |
|      - |  395 | ` * Return` |
|      - |  396 | ` *  the bool value of a variable.` |
|      - |  397 | ` */` |
|     16 |  398 | `static int PH7_builtin_boolval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  399 | `{` |
|      - |  400 | `	int bVal;` |
|     17 |  401 | `	if( nArg != 1 ){` |
|    ! 0 |  402 | `		return PH7_VmThrowException(pCtx,` |
|      - |  403 | `			"ArgumentCountError",` |
|      - |  404 | `			"boolval() expects exactly 1 argument, %d given",` |
|    ! 0 |  405 | `			nArg` |
|      - |  406 | `			);` |
|      - |  407 | `	}` |
|      - |  408 | `	/* Perform the cast */` |
|     17 |  409 | `	bVal = ph7_value_to_bool(apArg[0]);` |
|     17 |  410 | `	ph7_result_bool(pCtx,bVal);` |
|     17 |  411 | `	return PH7_OK;` |
|      9 |  412 | `}` |
|      - |  413 | `/*` |
|      - |  414 | ` * bool empty($var)` |
|      - |  415 | ` *  Determine whether a variable is empty.` |
|      - |  416 | ` * Parameters` |
|      - |  417 | ` *   $var: The variable being checked.` |
|      - |  418 | ` * Return` |
|      - |  419 | ` *  0 if var has a non-empty and non-zero value.1 otherwise.` |
|      - |  420 | ` */` |
|  39344 |  421 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  422 | `{` |
|  39349 |  423 | `	int res = 1; /* Assume empty by default */` |
|  39349 |  424 | `	if( nArg > 0 ){` |
|  39349 |  425 | `		res = ph7_value_is_empty(apArg[0]);` |
|  19672 |  426 | `	}` |
|  39349 |  427 | `	ph7_result_bool(pCtx,res);` |
|  39349 |  428 | `	return PH7_OK;` |
|      - |  429 |  |
|      5 |  430 | `}` |
|      - |  431 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |  432 | `#define PH7_NEED_BUILTIN_REG 1` |
|      - |  433 | `#endif` |
|      - |  434 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - |  435 | `#define PH7_NEED_FMT_AND_INI 1` |
|      - |  436 | `#endif` |
|      - |  437 |  |
|      - |  438 | `/* Math functions moved to builtin_math.c */` |
|      - |  439 |  |
|      - |  440 | `/* Table of the built-in functions */` |
|      - |  441 | `/*` |
|      - |  442 | ` * int memory_get_usage([bool $real_usage = false])` |
|      - |  443 | ` *  Amount of memory, in bytes, currently allocated to the script through PHL's` |
|      - |  444 | ` *  memory backend. PHL tracks the backend's real allocated bytes, so the` |
|      - |  445 | ` *  $real_usage flag has no effect here (php's non-real figure would be smaller,` |
|      - |  446 | ` *  reflecting Zend's emalloc bookkeeping — recorded divergence).` |
|      - |  447 | ` */` |
|    ! 0 |  448 | `static int PH7_builtin_memory_get_usage(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  449 | `{` |
|    ! 0 |  450 | `	SXUNUSED(nArg);` |
|    ! 0 |  451 | `	SXUNUSED(apArg);` |
|    ! 0 |  452 | `	ph7_result_int64(pCtx,(ph7_int64)pCtx->pVm->sAllocator.nMemUsed);` |
|    ! 0 |  453 | `	return PH7_OK;` |
|    ! 0 |  454 | `}` |
|      - |  455 | `/*` |
|      - |  456 | ` * int memory_get_peak_usage([bool $real_usage = false])` |
|      - |  457 | ` *  High-water mark of memory_get_usage() over the script's lifetime.` |
|      - |  458 | ` */` |
|      4 |  459 | `static int PH7_builtin_memory_get_peak_usage(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  460 | `{` |
|      2 |  461 | `	SXUNUSED(nArg);` |
|      2 |  462 | `	SXUNUSED(apArg);` |
|      5 |  463 | `	ph7_result_int64(pCtx,(ph7_int64)pCtx->pVm->sAllocator.nMemPeak);` |
|      5 |  464 | `	return PH7_OK;` |
|      1 |  465 | `}` |
|      - |  466 | `/*` |
|      - |  467 | ` * void memory_reset_peak_usage()` |
|      - |  468 | ` *  Reset the peak memory usage (memory_get_peak_usage) back to the current` |
|      - |  469 | ` *  live usage — php 8.2. Frameworks call it between tests to measure per-test` |
|      - |  470 | ` *  peaks.` |
|      - |  471 | ` */` |
|      4 |  472 | `static int PH7_builtin_memory_reset_peak_usage(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  473 | `{` |
|      2 |  474 | `	SXUNUSED(nArg);` |
|      2 |  475 | `	SXUNUSED(apArg);` |
|      5 |  476 | `	pCtx->pVm->sAllocator.nMemPeak = pCtx->pVm->sAllocator.nMemUsed;` |
|      5 |  477 | `	return PH7_OK;` |
|      1 |  478 | `}` |
|      - |  479 | `/*` |
|      - |  480 | ` * PHL frees values by reference count as they go out of scope, so there is no` |
|      - |  481 | ` * mark-and-sweep cycle collector to drive. The gc_* family is provided for` |
|      - |  482 | ` * source compatibility (real frameworks call it around test runs): the state is` |
|      - |  483 | ` * observational and collection is a no-op. Recorded divergence from php, whose` |
|      - |  484 | ` * collector actually reclaims reference cycles.` |
|      - |  485 | ` */` |
|    ! 0 |  486 | `static int PH7_builtin_gc_enable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  487 | `{` |
|    ! 0 |  488 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 |  489 | `	pCtx->pVm->bGcEnabled = 1;` |
|    ! 0 |  490 | `	return PH7_OK;` |
|    ! 0 |  491 | `}` |
|    ! 0 |  492 | `static int PH7_builtin_gc_disable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  493 | `{` |
|    ! 0 |  494 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 |  495 | `	pCtx->pVm->bGcEnabled = 0;` |
|    ! 0 |  496 | `	return PH7_OK;` |
|    ! 0 |  497 | `}` |
|    ! 0 |  498 | `static int PH7_builtin_gc_enabled(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  499 | `{` |
|    ! 0 |  500 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 |  501 | `	ph7_result_bool(pCtx,pCtx->pVm->bGcEnabled);` |
|    ! 0 |  502 | `	return PH7_OK;` |
|    ! 0 |  503 | `}` |
|    ! 0 |  504 | `static int PH7_builtin_gc_collect_cycles(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  505 | `{` |
|      - |  506 | `	/* No cycle collector: nothing to reclaim. Returns the count collected (0). */` |
|    ! 0 |  507 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 |  508 | `	ph7_result_int(pCtx,0);` |
|    ! 0 |  509 | `	return PH7_OK;` |
|    ! 0 |  510 | `}` |
|    ! 0 |  511 | `static int PH7_builtin_gc_mem_caches(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  512 | `{` |
|    ! 0 |  513 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 |  514 | `	ph7_result_int(pCtx,0);` |
|    ! 0 |  515 | `	return PH7_OK;` |
|    ! 0 |  516 | `}` |
|      - |  517 | `/*` |
|      - |  518 | ` * array gc_status(void)` |
|      - |  519 | ` *  php 8.3 shape. PHL never runs a collection, so every counter is zero and the` |
|      - |  520 | ` *  timing fields are 0.0; 'running' reflects gc_enable()/gc_disable().` |
|      - |  521 | ` */` |
|    ! 0 |  522 | `static int PH7_builtin_gc_status(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  523 | `{` |
|      - |  524 | `	ph7_value *pArray,*pVal;` |
|    ! 0 |  525 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 |  526 | `	pArray = ph7_context_new_array(pCtx);` |
|    ! 0 |  527 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    ! 0 |  528 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|    ! 0 |  529 | `		ph7_result_null(pCtx);` |
|    ! 0 |  530 | `		return PH7_OK;` |
|      - |  531 | `	}` |
|      - |  532 | `	/* Key order matches php 8.3's gc_status(). */` |
|    ! 0 |  533 | `	ph7_value_bool(pVal,pCtx->pVm->bGcEnabled); ph7_array_add_strkey_elem(pArray,"running",pVal);` |
|    ! 0 |  534 | `	ph7_value_bool(pVal,0);      ph7_array_add_strkey_elem(pArray,"protected",pVal);` |
|    ! 0 |  535 | `	ph7_value_bool(pVal,0);      ph7_array_add_strkey_elem(pArray,"full",pVal);` |
|    ! 0 |  536 | `	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"runs",pVal);` |
|    ! 0 |  537 | `	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"collected",pVal);` |
|    ! 0 |  538 | `	ph7_value_int(pVal,1000);    ph7_array_add_strkey_elem(pArray,"threshold",pVal);` |
|    ! 0 |  539 | `	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"buffer_size",pVal);` |
|    ! 0 |  540 | `	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"roots",pVal);` |
|    ! 0 |  541 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"application_time",pVal);` |
|    ! 0 |  542 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"collector_time",pVal);` |
|    ! 0 |  543 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"destructor_time",pVal);` |
|    ! 0 |  544 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"free_time",pVal);` |
|    ! 0 |  545 | `	ph7_context_release_value(pCtx,pVal);` |
|    ! 0 |  546 | `	ph7_result_value(pCtx,pArray);` |
|    ! 0 |  547 | `	return PH7_OK;` |
|    ! 0 |  548 | `}` |
|      - |  549 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - |  550 | `	{ "memory_get_usage"     , PH7_builtin_memory_get_usage      },` |
|      - |  551 | `	{ "memory_get_peak_usage", PH7_builtin_memory_get_peak_usage },` |
|      - |  552 | `	{ "memory_reset_peak_usage", PH7_builtin_memory_reset_peak_usage },` |
|      - |  553 | `	{ "gc_enable"            , PH7_builtin_gc_enable             },` |
|      - |  554 | `	{ "gc_disable"           , PH7_builtin_gc_disable            },` |
|      - |  555 | `	{ "gc_enabled"           , PH7_builtin_gc_enabled            },` |
|      - |  556 | `	{ "gc_collect_cycles"    , PH7_builtin_gc_collect_cycles     },` |
|      - |  557 | `	{ "gc_mem_caches"        , PH7_builtin_gc_mem_caches         },` |
|      - |  558 | `	{ "gc_status"            , PH7_builtin_gc_status             },` |
|      - |  559 | `	   /* Variable handling functions */` |
|      - |  560 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - |  561 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - |  562 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - |  563 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - |  564 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - |  565 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - |  566 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - |  567 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - |  568 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - |  569 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - |  570 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - |  571 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - |  572 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - |  573 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - |  574 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - |  575 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - |  576 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - |  577 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - |  578 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - |  579 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - |  580 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - |  581 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - |  582 | `	   /* Math functions */` |
|      - |  583 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - |  584 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - |  585 | `	{ "acosh" ,   PH7_builtin_acosh        },` |
|      - |  586 | `	{ "asinh" ,   PH7_builtin_asinh        },` |
|      - |  587 | `	{ "atanh" ,   PH7_builtin_atanh        },` |
|      - |  588 | `	{ "expm1" ,   PH7_builtin_expm1        },` |
|      - |  589 | `	{ "log1p" ,   PH7_builtin_log1p        },` |
|      - |  590 | `	{ "deg2rad" , PH7_builtin_deg2rad      },` |
|      - |  591 | `	{ "rad2deg" , PH7_builtin_rad2deg      },` |
|      - |  592 | `	{ "fpow" ,    PH7_builtin_fpow         },` |
|      - |  593 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - |  594 | `	{ "floor",    PH7_builtin_floor        },` |
|      - |  595 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - |  596 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - |  597 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - |  598 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - |  599 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - |  600 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - |  601 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - |  602 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - |  603 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - |  604 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - |  605 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - |  606 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - |  607 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - |  608 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - |  609 | `	{ "pi",       PH7_builtin_pi           },` |
|      - |  610 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - |  611 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - |  612 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - |  613 | `	{ "round",    PH7_builtin_round        },` |
|      - |  614 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - |  615 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - |  616 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - |  617 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - |  618 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - |  619 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - |  620 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - |  621 | `	{ "srand",  PH7_builtin_srand          },` |
|      - |  622 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - |  623 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - |  624 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - |  625 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - |  626 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - |  627 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - |  628 | `	   /* String handling functions */` |
|      - |  629 |  |
|      - |  630 | `	{ "substr",          PH7_builtin_substr     },` |
|      - |  631 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - |  632 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - |  633 | `	{ "substr_replace",  PH7_builtin_substr_replace },` |
|      - |  634 | `	{ "levenshtein",     PH7_builtin_levenshtein },` |
|      - |  635 | `	{ "similar_text",    PH7_builtin_similar_text },` |
|      - |  636 | `	{ "str_word_count",  PH7_builtin_str_word_count },` |
|      - |  637 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - |  638 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - |  639 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - |  640 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - |  641 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - |  642 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - |  643 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - |  644 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - |  645 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - |  646 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - |  647 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - |  648 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - |  649 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - |  650 | `	{ "strnatcmp"  , PH7_builtin_strnatcmp  },` |
|      - |  651 | `	{ "strnatcasecmp", PH7_builtin_strnatcmp },` |
|      - |  652 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - |  653 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - |  654 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - |  655 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - |  656 | `	{ "join"       , PH7_builtin_implode    },` |
|      - |  657 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - |  658 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - |  659 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - |  660 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - |  661 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - |  662 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - |  663 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - |  664 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - |  665 | `	{ "mb_strtolower",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */` |
|      - |  666 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - |  667 | `	{ "mb_strtoupper",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */` |
|      - |  668 | `	{ "mb_strlen",    PH7_builtin_mb_strlen_f },` |
|      - |  669 | `	{ "mb_substr",    PH7_builtin_mb_substr_f },` |
|      - |  670 | `	{ "mb_convert_case", PH7_builtin_mb_convert_case_f },` |
|      - |  671 | `	{ "mb_strpos",    PH7_builtin_mb_strpos_f },` |
|      - |  672 | `	{ "mb_stripos",   PH7_builtin_mb_strpos_f },` |
|      - |  673 | `	{ "mb_strrpos",   PH7_builtin_mb_strpos_f },` |
|      - |  674 | `	{ "mb_str_split", PH7_builtin_mb_str_split_f },` |
|      - |  675 | `	{ "mb_internal_encoding", PH7_builtin_mb_internal_encoding_f },` |
|      - |  676 | `	{ "mb_check_encoding",    PH7_builtin_mb_check_encoding_f },` |
|      - |  677 | `	{ "mb_strwidth",  PH7_builtin_mb_strwidth_f },` |
|      - |  678 | `	{ "mb_chr",       PH7_builtin_mb_chr_f   },` |
|      - |  679 | `	{ "mb_ord",       PH7_builtin_mb_ord_f   },` |
|      - |  680 | `	{ "mb_detect_encoding", PH7_builtin_mb_detect_encoding_f },` |
|      - |  681 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - |  682 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - |  683 | `	{ "ord",          PH7_builtin_ord        },` |
|      - |  684 | `	{ "chr",          PH7_builtin_chr        },` |
|      - |  685 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - |  686 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - |  687 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - |  688 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - |  689 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - |  690 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - |  691 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - |  692 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - |  693 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - |  694 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - |  695 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - |  696 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - |  697 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - |  698 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - |  699 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - |  700 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - |  701 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - |  702 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - |  703 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - |  704 | `	{ "printf",       PH7_builtin_printf     },` |
|      - |  705 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - |  706 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - |  707 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - |  708 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - |  709 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - |  710 |  |
|      - |  711 |  |
|      - |  712 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - |  713 | `	{ "md5",          PH7_builtin_md5       },` |
|      - |  714 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - |  715 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - |  716 | `	{ "hash",         PH7_builtin_hash      },` |
|      - |  717 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|      - |  718 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|      - |  719 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|      - |  720 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - |  721 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|      - |  722 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|      - |  723 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|      - |  724 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|      - |  725 | `	{ "filter_var",            PH7_builtin_filter_var },` |
|      - |  726 | `	{ "filter_input",          PH7_builtin_filter_input },` |
|      - |  727 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - |  728 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - |  729 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - |  730 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - |  731 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - |  732 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - |  733 |  |
|      - |  734 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - |  735 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - |  736 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - |  737 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - |  738 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - |  739 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - |  740 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - |  741 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - |  742 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - |  743 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - |  744 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - |  745 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - |  746 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - |  747 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - |  748 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - |  749 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - |  750 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - |  751 |  |
|      - |  752 | `	         /* Ctype functions */` |
|      - |  753 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - |  754 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - |  755 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - |  756 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - |  757 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - |  758 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - |  759 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - |  760 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - |  761 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - |  762 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - |  763 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - |  764 | `	         /* Time functions */` |
|      - |  765 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - |  766 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - |  767 | `	{ "hrtime",      PH7_builtin_hrtime       },` |
|      - |  768 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - |  769 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - |  770 | `	{ "date",        PH7_builtin_date         },` |
|      - |  771 | `	{ "idate",       PH7_builtin_idate        },` |
|      - |  772 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - |  773 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - |  774 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - |  775 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - |  776 | `	{ "date_default_timezone_get", PH7_builtin_date_default_timezone_get },` |
|      - |  777 | `	{ "date_default_timezone_set", PH7_builtin_date_default_timezone_set },` |
|      - |  778 | `	        /* URL functions */` |
|      - |  779 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - |  780 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - |  781 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - |  782 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - |  783 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - |  784 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - |  785 | `	{ "rawurlencode", PH7_builtin_rawurlencode },` |
|      - |  786 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - |  787 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - |  788 | `};` |
|      - |  789 | `/*` |
|      - |  790 | ` * Register the built-in functions defined above,the array functions` |
|      - |  791 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - |  792 | ` */` |
|   3382 |  793 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      5 |  794 | `{` |
|      - |  795 | `	sxu32 n;` |
| 696697 |  796 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 693315 |  797 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 346660 |  798 | `	}` |
|      - |  799 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   3387 |  800 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - |  801 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   3387 |  802 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   3387 |  803 | `}` |
|      - |  804 |  |
|      - |  805 | `/*` |
|      - |  806 | ` * UTF-8 encode/decode builtins (registered from vm.c).  Relocated here` |
|      - |  807 | ` * from the removed vm_xml.c when the legacy xml_* API was dropped.` |
|      - |  808 | ` */` |
|      - |  809 | `/*` |
|      - |  810 | ` * int utf8_encode(string $input)` |
|      - |  811 | ` *  UTF-8 encoding.` |
|      - |  812 | ` *  This function encodes the string data to UTF-8, and returns the encoded version.` |
|      - |  813 | ` *  UTF-8 is a standard mechanism used by Unicode for encoding wide character values` |
|      - |  814 | ` * into a byte stream. UTF-8 is transparent to plain ASCII characters, is self-synchronized` |
|      - |  815 | ` * (meaning it is possible for a program to figure out where in the bytestream characters start)` |
|      - |  816 | ` * and can be used with normal string comparison functions for sorting and such.` |
|      - |  817 | ` *  Notes on UTF-8 (According to SQLite3 authors):` |
|      - |  818 | ` *  Byte-0    Byte-1    Byte-2    Byte-3    Value` |
|      - |  819 | ` *  0xxxxxxx                                 00000000 00000000 0xxxxxxx` |
|      - |  820 | ` *  110yyyyy  10xxxxxx                       00000000 00000yyy yyxxxxxx` |
|      - |  821 | ` *  1110zzzz  10yyyyyy  10xxxxxx             00000000 zzzzyyyy yyxxxxxx` |
|      - |  822 | ` *  11110uuu  10uuzzzz  10yyyyyy  10xxxxxx   000uuuuu zzzzyyyy yyxxxxxx` |
|      - |  823 | ` * Parameters` |
|      - |  824 | ` * $input` |
|      - |  825 | ` *   String to encode or NULL on failure.` |
|      - |  826 | ` * Return` |
|      - |  827 | ` *  An UTF-8 encoded string.` |
|      - |  828 | ` */` |
|      2 |  829 | `PH7_PRIVATE int vm_builtin_utf8_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  830 | `{` |
|      - |  831 | `	const unsigned char *zIn,*zEnd;` |
|      - |  832 | `	int nByte,c,e;` |
|      3 |  833 | `	if( nArg < 1 ){` |
|      - |  834 | `		/* Missing arguments,return null */` |
|    ! 0 |  835 | `		ph7_result_null(pCtx);` |
|    ! 0 |  836 | `		return PH7_OK;` |
|      - |  837 | `	}` |
|      - |  838 | `	/* Extract the target string */` |
|      3 |  839 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nByte);` |
|      3 |  840 | `	if( nByte < 1 ){` |
|      - |  841 | `		/* Empty string,return null */` |
|    ! 0 |  842 | `		ph7_result_null(pCtx);` |
|    ! 0 |  843 | `		return PH7_OK;` |
|      - |  844 | `	}` |
|      3 |  845 | `	zEnd = &zIn[nByte];` |
|      - |  846 | `	/* Start the encoding process */` |
|      2 |  847 | `	for(;;){` |
|      5 |  848 | `		if( zIn >= zEnd ){` |
|      - |  849 | `			/* End of input */` |
|      3 |  850 | `			break;` |
|      - |  851 | `		}` |
|      3 |  852 | `		c = zIn[0];` |
|      - |  853 | `		/* Advance the stream cursor */` |
|      3 |  854 | `		zIn++;` |
|      - |  855 | `		/* Encode */` |
|      3 |  856 | `		if( c<0x00080 ){` |
|    ! 0 |  857 | `			e = (c&0xFF);` |
|    ! 0 |  858 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      3 |  859 | `		}else if( c<0x00800 ){` |
|      3 |  860 | `			e = 0xC0 + ((c>>6)&0x1F);` |
|      3 |  861 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      3 |  862 | `			e = 0x80 + (c & 0x3F);` |
|      3 |  863 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      1 |  864 | `		}else if( c<0x10000 ){` |
|    ! 0 |  865 | `			e = 0xE0 + ((c>>12)&0x0F);` |
|    ! 0 |  866 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|    ! 0 |  867 | `			e = 0x80 + ((c>>6) & 0x3F);` |
|    ! 0 |  868 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|    ! 0 |  869 | `			e = 0x80 + (c & 0x3F);` |
|    ! 0 |  870 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|    ! 0 |  871 | `		}else{` |
|    ! 0 |  872 | `			e = 0xF0 + ((c>>18) & 0x07);` |
|    ! 0 |  873 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|    ! 0 |  874 | `			e = 0x80 + ((c>>12) & 0x3F);` |
|    ! 0 |  875 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|    ! 0 |  876 | `			e = 0x80 + ((c>>6) & 0x3F);` |
|    ! 0 |  877 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|    ! 0 |  878 | `			e = 0x80 + (c & 0x3F);` |
|    ! 0 |  879 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      - |  880 | `		}` |
|      1 |  881 | `	}` |
|      - |  882 | `	/* All done */` |
|      3 |  883 | `	return PH7_OK;` |
|      2 |  884 | `}` |
|      - |  885 | `/* SPDX-SnippetBegin */` |
|      - |  886 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - |  887 | `/* SPDX-License-Identifier: blessing */` |
|      - |  888 | `/*` |
|      - |  889 | ` * UTF-8 decoding routine extracted from the sqlite3 source tree.` |
|      - |  890 | ` * Original author: D. Richard Hipp (http://www.sqlite.org)` |
|      - |  891 | ` * Status: Public Domain` |
|      - |  892 | ` */` |
|      - |  893 | `/*` |
|      - |  894 | `** This lookup table is used to help decode the first byte of` |
|      - |  895 | `** a multi-byte UTF8 character.` |
|      - |  896 | `*/` |
|      - |  897 | `static const unsigned char UtfTrans1[] = {` |
|      - |  898 | `  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,` |
|      - |  899 | `  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,` |
|      - |  900 | `  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,` |
|      - |  901 | `  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,` |
|      - |  902 | `  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,` |
|      - |  903 | `  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,` |
|      - |  904 | `  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,` |
|      - |  905 | `  0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x00, 0x00,` |
|      - |  906 | `};` |
|      - |  907 | `/*` |
|      - |  908 | `** Translate a single UTF-8 character.  Return the unicode value.` |
|      - |  909 | `**` |
|      - |  910 | `** During translation, assume that the byte that zTerm points` |
|      - |  911 | `** is a 0x00.` |
|      - |  912 | `**` |
|      - |  913 | `** Write a pointer to the next unread byte back into *pzNext.` |
|      - |  914 | `**` |
|      - |  915 | `** Notes On Invalid UTF-8:` |
|      - |  916 | `**` |
|      - |  917 | `**  *  This routine never allows a 7-bit character (0x00 through 0x7f) to` |
|      - |  918 | `**     be encoded as a multi-byte character.  Any multi-byte character that` |
|      - |  919 | `**     attempts to encode a value between 0x00 and 0x7f is rendered as 0xfffd.` |
|      - |  920 | `**` |
|      - |  921 | `**  *  This routine never allows a UTF16 surrogate value to be encoded.` |
|      - |  922 | `**     If a multi-byte character attempts to encode a value between` |
|      - |  923 | `**     0xd800 and 0xe000 then it is rendered as 0xfffd.` |
|      - |  924 | `**` |
|      - |  925 | `**  *  Bytes in the range of 0x80 through 0xbf which occur as the first` |
|      - |  926 | `**     byte of a character are interpreted as single-byte characters` |
|      - |  927 | `**     and rendered as themselves even though they are technically` |
|      - |  928 | `**     invalid characters.` |
|      - |  929 | `**` |
|      - |  930 | `**  *  This routine accepts an infinite number of different UTF8 encodings` |
|      - |  931 | `**     for unicode values 0x80 and greater.  It do not change over-length` |
|      - |  932 | `**     encodings to 0xfffd as some systems recommend.` |
|      - |  933 | `*/` |
|      - |  934 | `#define READ_UTF8(zIn, zTerm, c)                           \` |
|      - |  935 | `  c = *(zIn++);                                            \` |
|      - |  936 | `  if( c>=0xc0 ){                                           \` |
|      - |  937 | `    c = UtfTrans1[c-0xc0];                                 \` |
|      - |  938 | `    while( zIn!=zTerm && (*zIn & 0xc0)==0x80 ){            \` |
|      - |  939 | `      c = (c<<6) + (0x3f & *(zIn++));                      \` |
|      - |  940 | `    }                                                      \` |
|      - |  941 | `    if( c<0x80                                             \` |
|      - |  942 | `        \|\| (c&0xFFFFF800)==0xD800                          \` |
|      - |  943 | `        \|\| (c&0xFFFFFFFE)==0xFFFE ){  c = 0xFFFD; }        \` |
|      - |  944 | `  }` |
|    210 |  945 | `PH7_PRIVATE int PH7_Utf8Read(` |
|      - |  946 | `  const unsigned char *z,         /* First byte of UTF-8 character */` |
|      - |  947 | `  const unsigned char *zTerm,     /* Pretend this byte is 0x00 */` |
|      - |  948 | `  const unsigned char **pzNext    /* Write first byte past UTF-8 char here */` |
|      1 |  949 | `){` |
|      - |  950 | `  int c;` |
|    213 |  951 | `  READ_UTF8(z, zTerm, c);` |
|    211 |  952 | `  *pzNext = z;` |
|    211 |  953 | `  return c;` |
|      1 |  954 | `}` |
|      - |  955 | `/* SPDX-SnippetEnd */` |
|      - |  956 | `/*` |
|      - |  957 | ` * string utf8_decode(string $data)` |
|      - |  958 | ` *  This function decodes data, assumed to be UTF-8 encoded, to unicode.` |
|      - |  959 | ` * Parameters` |
|      - |  960 | ` * data` |
|      - |  961 | ` *  An UTF-8 encoded string.` |
|      - |  962 | ` * Return` |
|      - |  963 | ` *  Unicode decoded string or NULL on failure.` |
|      - |  964 | ` */` |
|      2 |  965 | `PH7_PRIVATE int vm_builtin_utf8_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  966 | `{` |
|      - |  967 | `	const unsigned char *zIn,*zEnd;` |
|      - |  968 | `	int nByte,c;` |
|      3 |  969 | `	if( nArg < 1 ){` |
|      - |  970 | `		/* Missing arguments,return null */` |
|    ! 0 |  971 | `		ph7_result_null(pCtx);` |
|    ! 0 |  972 | `		return PH7_OK;` |
|      - |  973 | `	}` |
|      - |  974 | `	/* Extract the target string */` |
|      3 |  975 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nByte);` |
|      3 |  976 | `	if( nByte < 1 ){` |
|      - |  977 | `		/* Empty string,return null */` |
|    ! 0 |  978 | `		ph7_result_null(pCtx);` |
|    ! 0 |  979 | `		return PH7_OK;` |
|      - |  980 | `	}` |
|      3 |  981 | `	zEnd = &zIn[nByte];` |
|      - |  982 | `	/* Start the decoding process */` |
|      5 |  983 | `	while( zIn < zEnd ){` |
|      3 |  984 | `		c = PH7_Utf8Read(zIn,zEnd,&zIn);` |
|      3 |  985 | `		if( c == 0x0 ){` |
|    ! 0 |  986 | `			break;` |
|      - |  987 | `		}` |
|      3 |  988 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      1 |  989 | `	}` |
|      3 |  990 | `	return PH7_OK;` |
|      2 |  991 | `}` |
|      - |  992 |  |
