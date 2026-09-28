# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 312/400 lines (78.00%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `/* filter_var(FILTER_VALIDATE_FLOAT) parses with libc strtod directly because it` |
|       - |    8 | ` * needs errno==ERANGE to reject out-of-range magnitudes; SyStrToReal (also` |
|       - |    9 | ` * strtod-backed nowadays) exposes no range-error signal. */` |
|       - |   10 | `#include <stdlib.h>  /* strtod */` |
|       - |   11 | `#include <math.h>    /* HUGE_VAL */` |
|       - |   12 | `#include <errno.h>   /* ERANGE (strtod range-error signal) */` |
|       - |   13 | `#include <stdio.h>   /* snprintf (printf-family float conversions — correctly` |
|       - |   14 | `                      * rounded digits like php's zend_dtoa; see PH7_InputFormat) */` |
|       - |   15 | ``/* Shared ZPP helper for `int` parameters — defined OUTSIDE the`` |
|       - |   16 | ` * PH7_DISABLE_BUILTIN_FUNC guard because hashmap.c (array_slice) and` |
|       - |   17 | ` * builtin_math.c (intdiv) call it and both compile in the tiny build. */` |
|  979228 |   18 | `PH7_PRIVATE sxi32 PH7_IntArgResolve(` |
|       - |   19 | `	ph7_context *pCtx,` |
|       - |   20 | `	ph7_value *pArg,` |
|       - |   21 | `	const char *zFunc,` |
|       - |   22 | `	int iArgNum,` |
|       - |   23 | `	const char *zParamName,` |
|       - |   24 | `	const char *zTypeStr,` |
|       - |   25 | `	sxi64 *pOut` |
|       5 |   26 | `){` |
|  979233 |   27 | `	if( ph7_value_is_null(pArg) ){` |
|       - |   28 | `		/* php only DEPRECATES passing null to a non-nullable internal param; PHL` |
|       - |   29 | `		 * targets php's non-deprecated surface and rejects it with the TypeError` |
|       - |   30 | `		 * php will eventually raise. */` |
|     ! 0 |   31 | `		return PH7_VmThrowException(pCtx,` |
|       - |   32 | `			"TypeError",` |
|       - |   33 | `			"%s(): Argument #%d (%s) must be of type %s, null given",` |
|     ! 0 |   34 | `			zFunc,iArgNum,zParamName,zTypeStr` |
|       - |   35 | `			);` |
|       - |   36 | `	}` |
|  979233 |   37 | `	if( ph7_value_is_float(pArg) ){` |
|      27 |   38 | `		double dVal = ph7_value_to_double(pArg);` |
|       - |   39 | `		sxi64 iVal;` |
|       - |   40 | `		/* php: NAN/INF/out-of-int64-range floats fail ZPP outright */` |
|      27 |   41 | `		if( !PH7_RealFitsInt64(dVal) ){` |
|     ! 0 |   42 | `			return PH7_VmThrowException(pCtx,` |
|       - |   43 | `				"TypeError",` |
|       - |   44 | `				"%s(): Argument #%d (%s) must be of type %s, float given",` |
|     ! 0 |   45 | `				zFunc,iArgNum,zParamName,zTypeStr` |
|       - |   46 | `				);` |
|       - |   47 | `		}` |
|      27 |   48 | `		iVal = (sxi64)dVal;` |
|      27 |   49 | `		if( (double)iVal != dVal ){` |
|       - |   50 | `			/* php DEPRECATES a lossy float->int; PHL rejects it (the value is not` |
|       - |   51 | `			 * representable as int). */` |
|     ! 0 |   52 | `			return PH7_VmThrowException(pCtx,` |
|       - |   53 | `				"TypeError",` |
|       - |   54 | `				"%s(): Argument #%d (%s) must be of type %s, float given",` |
|     ! 0 |   55 | `				zFunc,iArgNum,zParamName,zTypeStr` |
|       - |   56 | `				);` |
|       - |   57 | `		}` |
|      27 |   58 | `		*pOut = iVal;` |
|      27 |   59 | `		return PH7_OK;` |
|       - |   60 | `	}` |
|  979209 |   61 | `	if( ph7_value_is_string(pArg) ){` |
|       - |   62 | `		const char *zNum;` |
|       - |   63 | `		int nSlen;` |
|      58 |   64 | `		int i,bFloat = 0;` |
|      58 |   65 | `		if( !PH7_MemObjStringIsNumeric(pArg) ){` |
|      31 |   66 | `			return PH7_VmThrowException(pCtx,` |
|       - |   67 | `				"TypeError",` |
|       - |   68 | `				"%s(): Argument #%d (%s) must be of type %s, string given",` |
|      10 |   69 | `				zFunc,iArgNum,zParamName,zTypeStr` |
|       - |   70 | `				);` |
|       - |   71 | `		}` |
|      38 |   72 | `		zNum = ph7_value_to_string(pArg,&nSlen);` |
|      88 |   73 | `		for( i = 0 ; i < nSlen ; i++ ){` |
|      56 |   74 | `			if( zNum[i] == '.' \|\| zNum[i] == 'e' \|\| zNum[i] == 'E' ){` |
|       5 |   75 | `				bFloat = 1;` |
|       5 |   76 | `				break;` |
|       - |   77 | `			}` |
|      27 |   78 | `		}` |
|      38 |   79 | `		if( bFloat ){` |
|       5 |   80 | `			double dVal = 0;` |
|       - |   81 | `			sxi64 iVal;` |
|       5 |   82 | `			SyStrToReal(zNum,(sxu32)nSlen,(void *)&dVal,0);` |
|       5 |   83 | `			if( !PH7_RealFitsInt64(dVal) ){` |
|     ! 0 |   84 | `				return PH7_VmThrowException(pCtx,` |
|       - |   85 | `					"TypeError",` |
|       - |   86 | `					"%s(): Argument #%d (%s) must be of type %s, string given",` |
|     ! 0 |   87 | `					zFunc,iArgNum,zParamName,zTypeStr` |
|       - |   88 | `					);` |
|       - |   89 | `			}` |
|       5 |   90 | `			iVal = (sxi64)dVal;` |
|       5 |   91 | `			if( (double)iVal != dVal ){` |
|       - |   92 | `				/* php DEPRECATES a lossy float-string->int; PHL rejects it. */` |
|     ! 0 |   93 | `				return PH7_VmThrowException(pCtx,` |
|       - |   94 | `					"TypeError",` |
|       - |   95 | `					"%s(): Argument #%d (%s) must be of type %s, string given",` |
|     ! 0 |   96 | `					zFunc,iArgNum,zParamName,zTypeStr` |
|       - |   97 | `					);` |
|       - |   98 | `			}` |
|       5 |   99 | `			*pOut = iVal;` |
|       5 |  100 | `			return PH7_OK;` |
|       - |  101 | `		}` |
|      34 |  102 | `		*pOut = ph7_value_to_int64(pArg);` |
|      34 |  103 | `		return PH7_OK;` |
|       - |  104 | `	}` |
|  979153 |  105 | `	if( !ph7_value_is_int(pArg) && !ph7_value_is_bool(pArg) ){` |
|       - |  106 | `		/* Arrays, resources and objects: php names the class for objects */` |
|     ! 0 |  107 | `		const char *zType = ph7_type_name(pArg);` |
|     ! 0 |  108 | `		if( ph7_value_is_object(pArg) ){` |
|     ! 0 |  109 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     ! 0 |  110 | `			if( pInst && pInst->pClass ){` |
|     ! 0 |  111 | `				zType = SyStringData(&pInst->pClass->sName);` |
|     ! 0 |  112 | `			}` |
|     ! 0 |  113 | `		}` |
|     ! 0 |  114 | `		return PH7_VmThrowException(pCtx,` |
|       - |  115 | `			"TypeError",` |
|       - |  116 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|     ! 0 |  117 | `			zFunc,iArgNum,zParamName,zTypeStr,zType` |
|       - |  118 | `			);` |
|       - |  119 | `	}` |
|  979153 |  120 | `	*pOut = ph7_value_to_int64(pArg);` |
|  979153 |  121 | `	return PH7_OK;` |
|  490199 |  122 | `}` |
|       - |  123 |  |
|       - |  124 | `/* This file implement built-in 'foreign' functions for the PH7 engine */` |
|       - |  125 | `/*` |
|       - |  126 | ` * Section:` |
|       - |  127 | ` *    Variable handling Functions.` |
|       - |  128 | ` * Status:` |
|       - |  129 | ` *    Stable.` |
|       - |  130 | ` */` |
|       - |  131 | `/*` |
|       - |  132 | ` * bool is_bool($var)` |
|       - |  133 | ` *  Finds out whether a variable is a boolean.` |
|       - |  134 | ` * Parameters` |
|       - |  135 | ` *   $var: The variable being evaluated.` |
|       - |  136 | ` * Return` |
|       - |  137 | ` *  TRUE if var is a boolean. False otherwise.` |
|       - |  138 | ` */` |
|     282 |  139 | `static int PH7_builtin_is_bool(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  140 | `{` |
|     284 |  141 | `	int res = 0; /* Assume false by default */` |
|     284 |  142 | `	if( nArg > 0 ){` |
|     284 |  143 | `		res = ph7_value_is_bool(apArg[0]);` |
|     141 |  144 | `	}` |
|       - |  145 | `	/* Query result */` |
|     284 |  146 | `	ph7_result_bool(pCtx,res);` |
|     284 |  147 | `	return PH7_OK;` |
|       2 |  148 | `}` |
|       - |  149 | `/*` |
|       - |  150 | ` * bool is_float($var)` |
|       - |  151 | ` * bool is_double($var)` |
|       - |  152 | ` *  Finds out whether a variable is a float.` |
|       - |  153 | ` * Parameters` |
|       - |  154 | ` *   $var: The variable being evaluated.` |
|       - |  155 | ` * Return` |
|       - |  156 | ` *  TRUE if var is a float. False otherwise.` |
|       - |  157 | ` */` |
|    6316 |  158 | `static int PH7_builtin_is_float(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  159 | `{` |
|    6317 |  160 | `	int res = 0; /* Assume false by default */` |
|    6317 |  161 | `	if( nArg > 0 ){` |
|    6317 |  162 | `		res = ph7_value_is_float(apArg[0]);` |
|    3158 |  163 | `	}` |
|       - |  164 | `	/* Query result */` |
|    6317 |  165 | `	ph7_result_bool(pCtx,res);` |
|    6317 |  166 | `	return PH7_OK;` |
|       1 |  167 | `}` |
|       - |  168 | `/*` |
|       - |  169 | ` * bool is_int($var)` |
|       - |  170 | ` * bool is_integer($var)` |
|       - |  171 | ` * bool is_long($var)` |
|       - |  172 | ` *  Finds out whether a variable is an integer.` |
|       - |  173 | ` * Parameters` |
|       - |  174 | ` *   $var: The variable being evaluated.` |
|       - |  175 | ` * Return` |
|       - |  176 | ` *  TRUE if var is an integer. False otherwise.` |
|       - |  177 | ` */` |
|     765 |  178 | `static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  179 | `{` |
|     768 |  180 | `	int res = 0; /* Assume false by default */` |
|     768 |  181 | `	if( nArg > 0 ){` |
|       - |  182 | `		/* Strict PHP identity: a float is never an int, even when it holds an` |
|       - |  183 | `		 * integer value (1.0). An integer-valued real carries both MEMOBJ_INT` |
|       - |  184 | `		 * (cached) and MEMOBJ_REAL, so REAL must be excluded here. */` |
|     768 |  185 | `		res = ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]);` |
|     379 |  186 | `	}` |
|       - |  187 | `	/* Query result */` |
|     768 |  188 | `	ph7_result_bool(pCtx,res);` |
|     768 |  189 | `	return PH7_OK;` |
|       3 |  190 | `}` |
|       - |  191 | `/*` |
|       - |  192 | ` * bool is_string($var)` |
|       - |  193 | ` *  Finds out whether a variable is a string.` |
|       - |  194 | ` * Parameters` |
|       - |  195 | ` *   $var: The variable being evaluated.` |
|       - |  196 | ` * Return` |
|       - |  197 | ` *  TRUE if var is string. False otherwise.` |
|       - |  198 | ` */` |
|     846 |  199 | `static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  200 | `{` |
|     849 |  201 | `	int res = 0; /* Assume false by default */` |
|     849 |  202 | `	if( nArg > 0 ){` |
|     849 |  203 | `		res = ph7_value_is_string(apArg[0]);` |
|     423 |  204 | `	}` |
|       - |  205 | `	/* Query result */` |
|     849 |  206 | `	ph7_result_bool(pCtx,res);` |
|     849 |  207 | `	return PH7_OK;` |
|       3 |  208 | `}` |
|       - |  209 | `/*` |
|       - |  210 | ` * bool is_null($var)` |
|       - |  211 | ` *  Finds out whether a variable is NULL.` |
|       - |  212 | ` * Parameters` |
|       - |  213 | ` *   $var: The variable being evaluated.` |
|       - |  214 | ` * Return` |
|       - |  215 | ` *  TRUE if var is NULL. False otherwise.` |
|       - |  216 | ` */` |
|      82 |  217 | `static int PH7_builtin_is_null(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  218 | `{` |
|      85 |  219 | `	int res = 0; /* Assume false by default */` |
|      85 |  220 | `	if( nArg > 0 ){` |
|      85 |  221 | `		res = ph7_value_is_null(apArg[0]);` |
|      41 |  222 | `	}` |
|       - |  223 | `	/* Query result */` |
|      85 |  224 | `	ph7_result_bool(pCtx,res);` |
|      85 |  225 | `	return PH7_OK;` |
|       3 |  226 | `}` |
|       - |  227 | `/*` |
|       - |  228 | ` * bool is_numeric($var)` |
|       - |  229 | ` *  Find out whether a variable is NULL.` |
|       - |  230 | ` * Parameters` |
|       - |  231 | ` *  $var: The variable being evaluated.` |
|       - |  232 | ` * Return` |
|       - |  233 | ` *  True if var is numeric. False otherwise.` |
|       - |  234 | ` */` |
|      94 |  235 | `static int PH7_builtin_is_numeric(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  236 | `{` |
|      99 |  237 | `	int res = 0; /* Assume false by default */` |
|      99 |  238 | `	if( nArg > 0 ){` |
|       - |  239 | `		/* Strict PHP semantics: only int/float and numeric strings are numeric.` |
|       - |  240 | `		 * PHL's lenient helper also reports booleans as numeric (they coerce for` |
|       - |  241 | `		 * arithmetic), but php's is_numeric() rejects true/false, so exclude` |
|       - |  242 | `		 * MEMOBJ_BOOL here. */` |
|      99 |  243 | `		res = ph7_value_is_numeric(apArg[0]) && !ph7_value_is_bool(apArg[0]);` |
|      47 |  244 | `	}` |
|       - |  245 | `	/* Query result */` |
|      99 |  246 | `	ph7_result_bool(pCtx,res);` |
|      99 |  247 | `	return PH7_OK;` |
|       5 |  248 | `}` |
|       - |  249 | `/*` |
|       - |  250 | ` * bool is_scalar($var)` |
|       - |  251 | ` *  Find out whether a variable is a scalar.` |
|       - |  252 | ` * Parameters` |
|       - |  253 | ` *  $var: The variable being evaluated.` |
|       - |  254 | ` * Return` |
|       - |  255 | ` *  True if var is scalar. False otherwise.` |
|       - |  256 | ` */` |
|      30 |  257 | `static int PH7_builtin_is_scalar(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  258 | `{` |
|      31 |  259 | `	int res = 0; /* Assume false by default */` |
|      31 |  260 | `	if( nArg > 0 ){` |
|       - |  261 | `		/* Strict PHP semantics: scalars are int/float/string/bool. PHL's` |
|       - |  262 | `		 * MEMOBJ_SCALAR bucket also includes NULL, but php's is_scalar(null) is` |
|       - |  263 | `		 * false, so exclude the NULL case. */` |
|      31 |  264 | `		res = ph7_value_is_scalar(apArg[0]) && !ph7_value_is_null(apArg[0]);` |
|      15 |  265 | `	}` |
|       - |  266 | `	/* Query result */` |
|      31 |  267 | `	ph7_result_bool(pCtx,res);` |
|      31 |  268 | `	return PH7_OK;` |
|       1 |  269 | `}` |
|       - |  270 | `/*` |
|       - |  271 | ` * bool is_array($var)` |
|       - |  272 | ` *  Find out whether a variable is an array.` |
|       - |  273 | ` * Parameters` |
|       - |  274 | ` *  $var: The variable being evaluated.` |
|       - |  275 | ` * Return` |
|       - |  276 | ` *  True if var is an array. False otherwise.` |
|       - |  277 | ` */` |
|    6652 |  278 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  279 | `{` |
|    6656 |  280 | `	int res = 0; /* Assume false by default */` |
|    6656 |  281 | `	if( nArg > 0 ){` |
|    6656 |  282 | `		res = ph7_value_is_array(apArg[0]);` |
|    3326 |  283 | `	}` |
|       - |  284 | `	/* Query result */` |
|    6656 |  285 | `	ph7_result_bool(pCtx,res);` |
|    6656 |  286 | `	return PH7_OK;` |
|       4 |  287 | `}` |
|       - |  288 | `/*` |
|       - |  289 | ` * bool is_object($var)` |
|       - |  290 | ` *  Find out whether a variable is an object.` |
|       - |  291 | ` * Parameters` |
|       - |  292 | ` *  $var: The variable being evaluated.` |
|       - |  293 | ` * Return` |
|       - |  294 | ` *  True if var is an object. False otherwise.` |
|       - |  295 | ` */` |
|     856 |  296 | `static int PH7_builtin_is_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  297 | `{` |
|     861 |  298 | `	int res = 0; /* Assume false by default */` |
|     861 |  299 | `	if( nArg > 0 ){` |
|     861 |  300 | `		res = ph7_value_is_object(apArg[0]);` |
|     428 |  301 | `	}` |
|       - |  302 | `	/* Query result */` |
|     861 |  303 | `	ph7_result_bool(pCtx,res);` |
|     861 |  304 | `	return PH7_OK;` |
|       5 |  305 | `}` |
|       - |  306 | `/*` |
|       - |  307 | ` * bool is_resource($var)` |
|       - |  308 | ` *  Find out whether a variable is a resource.` |
|       - |  309 | ` * Parameters` |
|       - |  310 | ` *  $var: The variable being evaluated.` |
|       - |  311 | ` * Return` |
|       - |  312 | ` *  True if a resource. False otherwise.` |
|       - |  313 | ` */` |
|     300 |  314 | `static int PH7_builtin_is_resource(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  315 | `{` |
|     305 |  316 | `	int res = 0; /* Assume false by default */` |
|     305 |  317 | `	if( nArg > 0 && ph7_value_is_resource(apArg[0]) ){` |
|       - |  318 | `		/* A handle closed via fclose()/closedir()/pclose() is no longer a` |
|       - |  319 | `		 * live resource — php's is_resource() returns false for it. */` |
|     253 |  320 | `		res = !PH7_VfsResourceIsClosed(apArg[0]->x.pOther);` |
|     124 |  321 | `	}` |
|     305 |  322 | `	ph7_result_bool(pCtx,res);` |
|     305 |  323 | `	return PH7_OK;` |
|       5 |  324 | `}` |
|       - |  325 | `/*` |
|       - |  326 | ` * float floatval($var)` |
|       - |  327 | ` *  Get float value of a variable.` |
|       - |  328 | ` * Parameter` |
|       - |  329 | ` *  $var: The variable being processed.` |
|       - |  330 | ` * Return` |
|       - |  331 | ` *  the float value of a variable.` |
|       - |  332 | ` */` |
|       4 |  333 | `static int PH7_builtin_floatval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  334 | `{` |
|       5 |  335 | `	if( nArg < 1 ){` |
|       - |  336 | `		/* return 0.0 */` |
|     ! 0 |  337 | `		ph7_result_double(pCtx,0);` |
|     ! 0 |  338 | `	}else{` |
|       - |  339 | `		double dval;` |
|       - |  340 | `		/* Perform the cast */` |
|       5 |  341 | `		dval = ph7_value_to_double(apArg[0]);` |
|       5 |  342 | `		ph7_result_double(pCtx,dval);` |
|       - |  343 | `	}` |
|       5 |  344 | `	return PH7_OK;` |
|       1 |  345 | `}` |
|       - |  346 | `/*` |
|       - |  347 | ` * One C strtol() run, which is what php's intval() calls (ZEND_STRTOL in` |
|       - |  348 | ` * ext/standard/type.c). The rules are strtol's, not php's own numeric-string` |
|       - |  349 | ` * ones, and every one of them is observable:` |
|       - |  350 | ` *  - leading whitespace, then at most ONE sign;` |
|       - |  351 | ` *  - base 16 skips an optional "0x"/"0X"; base 0 PICKS the base from the same` |
|       - |  352 | ` *    prefix ("0x" -> 16, a leading "0" -> 8, otherwise 10);` |
|       - |  353 | ` *  - the scan stops at the first byte the base cannot spell, so "12ag" in base` |
|       - |  354 | ` *    16 is 0x12a and "0x0x1" is 0;` |
|       - |  355 | ` *  - a base outside 2..36 makes strtol answer 0 -- php raises nothing for it;` |
|       - |  356 | ` *  - the result SATURATES at PHP_INT_MAX/PHP_INT_MIN instead of wrapping.` |
|       - |  357 | `` * `iPreSign` is the sign php pastes in FRONT of the string it hands over (see`` |
|       - |  358 | ` * IntvalStrToInt64): a sign already consumed, so neither whitespace nor a` |
|       - |  359 | ` * second sign may follow it.` |
|       - |  360 | ` */` |
|     178 |  361 | `static sxi64 IntvalStrtol(int iPreSign,const char *zIn,int nLen,int iBase)` |
|       1 |  362 | `{` |
|     179 |  363 | `	sxu64 uLimit,uCutoff,uAcc = 0;` |
|     179 |  364 | `	int iCutlim,iSign = 1,bAny = 0,bOvf = 0;` |
|     179 |  365 | `	int i = 0;` |
|     179 |  366 | `	if( iPreSign ){` |
|      15 |  367 | `		iSign = (iPreSign == '-') ? -1 : 1;` |
|       8 |  368 | `	}else{` |
|     187 |  369 | `		while( i < nLen && SyisSpace((unsigned char)zIn[i]) ){` |
|      23 |  370 | `			i++;` |
|       1 |  371 | `		}` |
|     165 |  372 | `		if( i < nLen && (zIn[i] == '-' \|\| zIn[i] == '+') ){` |
|      33 |  373 | `			iSign = (zIn[i] == '-') ? -1 : 1;` |
|      33 |  374 | `			i++;` |
|      16 |  375 | `		}` |
|       - |  376 | `	}` |
|     178 |  377 | `	if( (iBase == 0 \|\| iBase == 16) && i + 1 < nLen` |
|     127 |  378 | `	 && zIn[i] == '0' && (zIn[i+1] == 'x' \|\| zIn[i+1] == 'X') ){` |
|      15 |  379 | `		i += 2;` |
|      15 |  380 | `		iBase = 16;` |
|     172 |  381 | `	}else if( (iBase == 0 \|\| iBase == 2) && i + 2 < nLen` |
|     114 |  382 | `	 && zIn[i] == '0' && (zIn[i+1] == 'b' \|\| zIn[i+1] == 'B')` |
|      52 |  383 | `	 && (zIn[i+2] == '0' \|\| zIn[i+2] == '1') ){` |
|       - |  384 | `		/* The conversion accepts a binary prefix of its own, on TOP of the one` |
|       - |  385 | `		 * IntvalStrToInt64 strips -- which is why intval("0b0b1",2) is 1 and a` |
|       - |  386 | `		 * THIRD prefix stops the scan: intval("0b0b0b1",2) is 0. It is also the` |
|       - |  387 | `		 * only prefix reader a base that NARROWED to 0 or 2 gets, since the` |
|       - |  388 | `		 * strip upstream reads the base at full width. */` |
|      39 |  389 | `		i += 2;` |
|      39 |  390 | `		iBase = 2;` |
|     140 |  391 | `	}else if( iBase == 0 ){` |
|      15 |  392 | `		iBase = (i < nLen && zIn[i] == '0') ? 8 : 10;` |
|       7 |  393 | `	}` |
|     173 |  394 | `	if( iBase < 2 \|\| iBase > 36 ){` |
|       9 |  395 | `		return 0;` |
|       - |  396 | `	}` |
|       - |  397 | `	/* strtol's own overflow test: the magnitude a negative result may reach is` |
|       - |  398 | `	 * one larger than a positive one, so the cutoff is computed per sign. */` |
|     171 |  399 | `	uLimit  = (iSign < 0) ? (sxu64)SXI64_HIGH + 1 : (sxu64)SXI64_HIGH;` |
|     171 |  400 | `	uCutoff = uLimit / (sxu64)iBase;` |
|     171 |  401 | `	iCutlim = (int)(uLimit % (sxu64)iBase);` |
|     583 |  402 | `	for( ; i < nLen ; ++i ){` |
|     469 |  403 | `		int c = (unsigned char)zIn[i];` |
|     469 |  404 | `		if( c >= '0' && c <= '9' ){` |
|     269 |  405 | `			c -= '0';` |
|     335 |  406 | `		}else if( c >= 'A' && c <= 'Z' ){` |
|       5 |  407 | `			c -= 'A' - 10;` |
|     199 |  408 | `		}else if( c >= 'a' && c <= 'z' ){` |
|     183 |  409 | `			c -= 'a' - 10;` |
|      92 |  410 | `		}else{` |
|       8 |  411 | `			break;` |
|       - |  412 | `		}` |
|     455 |  413 | `		if( c >= iBase ){` |
|      43 |  414 | `			break;` |
|       - |  415 | `		}` |
|     413 |  416 | `		bAny = 1;` |
|     413 |  417 | `		if( bOvf \|\| uAcc > uCutoff \|\| (uAcc == uCutoff && c > iCutlim) ){` |
|      11 |  418 | `			bOvf = 1;   /* keep consuming digits, the answer is pinned */` |
|      11 |  419 | `			continue;` |
|       - |  420 | `		}` |
|     403 |  421 | `		uAcc = uAcc * (sxu64)iBase + (sxu64)c;` |
|     202 |  422 | `	}` |
|     171 |  423 | `	if( bOvf ){` |
|       7 |  424 | `		return (iSign < 0) ? (-(sxi64)SXI64_HIGH - 1) : (sxi64)SXI64_HIGH;` |
|       - |  425 | `	}` |
|     165 |  426 | `	if( !bAny ){` |
|      23 |  427 | `		return 0;` |
|       - |  428 | `	}` |
|     143 |  429 | `	if( iSign < 0 ){` |
|       - |  430 | `		/* uAcc may be exactly 2^63 here, which no sxi64 holds: PHP_INT_MIN is` |
|       - |  431 | `		 * its negation and negating the SIGNED value would be undefined. */` |
|      21 |  432 | `		return (uAcc == (sxu64)SXI64_HIGH + 1) ? (-(sxi64)SXI64_HIGH - 1) : -(sxi64)uAcc;` |
|       - |  433 | `	}` |
|     123 |  434 | `	return (sxi64)uAcc;` |
|      90 |  435 | `}` |
|       - |  436 | `/*` |
|       - |  437 | ` * php's intval() string path. strtol() knows "0x" but not "0b", so php strips a` |
|       - |  438 | ` * binary prefix ITSELF -- for base 2 and for base 0 -- by building a fresh` |
|       - |  439 | ` * string out of the sign it found and the bytes past the "0b", and running` |
|       - |  440 | ` * strtol over THAT. The rebuild is observable, because strtol then runs its` |
|       - |  441 | `` * whole prelude again over the remainder: `intval("0b-1",2)` is -1 and`` |
|       - |  442 | `` * `intval("0b 1",0)` is 1, while a sign BEFORE the prefix is already spent, so`` |
|       - |  443 | `` * `intval("-0b-1",2)` is 0. php hands strtol() the C string, so an embedded NUL`` |
|       - |  444 | ` * truncates: intval("12\0 34",16) is 0x12. That truncation is copied too.` |
|       - |  445 | ` */` |
|     178 |  446 | `static sxi64 IntvalStrToInt64(const char *zIn,int nLen,sxi64 iBase64,int iBase)` |
|       1 |  447 | `{` |
|     179 |  448 | `	int i = 0;` |
|       - |  449 | `	/* An embedded NUL ends the string for strtol(). */` |
|    1031 |  450 | `	while( i < nLen && zIn[i] != 0 ){` |
|     853 |  451 | `		i++;` |
|       1 |  452 | `	}` |
|     179 |  453 | `	nLen = i;` |
|     179 |  454 | `	i = 0;` |
|     189 |  455 | `	while( i < nLen && SyisSpace((unsigned char)zIn[i]) ){` |
|      11 |  456 | `		i++;` |
|       1 |  457 | `	}` |
|       - |  458 | `	/* php's own strip reads the base at FULL width, one step before the narrowing` |
|       - |  459 | `	 * cast the conversion below gets -- so base 2^32+2 does not strip here even` |
|       - |  460 | `	 * though it converts in base 2. Only when something FOLLOWS the prefix, too:` |
|       - |  461 | `	 * "0b" alone stays a base-2 zero. */` |
|     179 |  462 | `	if( (iBase64 == 0 \|\| iBase64 == 2) && nLen - i > 2 ){` |
|      83 |  463 | `		int off = (zIn[i] == '-' \|\| zIn[i] == '+') ? 1 : 0;` |
|      83 |  464 | `		if( zIn[i+off] == '0' && (zIn[i+off+1] == 'b' \|\| zIn[i+off+1] == 'B') ){` |
|      73 |  465 | `			int iPreSign = off ? (unsigned char)zIn[i] : 0;` |
|      73 |  466 | `			i += off + 2;` |
|      73 |  467 | `			return IntvalStrtol(iPreSign,&zIn[i],nLen - i,2);` |
|       - |  468 | `		}` |
|       5 |  469 | `	}` |
|     107 |  470 | `	return IntvalStrtol(0,zIn,nLen,iBase);` |
|      90 |  471 | `}` |
|       - |  472 | `/*` |
|       - |  473 | ` * int intval(mixed $value, int $base = 10)` |
|       - |  474 | ` *  Get integer value of a variable.` |
|       - |  475 | ` * Parameters` |
|       - |  476 | ` *  $value: The variable being processed.` |
|       - |  477 | ` *  $base: The base $value is written in -- read ONLY when $value is a string` |
|       - |  478 | ` *   and the base is not 10. Every other value takes the ordinary int cast and` |
|       - |  479 | ` *   ignores $base entirely, an out-of-range one included.` |
|       - |  480 | ` * Return` |
|       - |  481 | ` *  the int value of a variable.` |
|       - |  482 | ` */` |
| 1170942 |  483 | `static int PH7_builtin_intval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  484 | `{` |
| 1170944 |  485 | `	if( nArg < 1 ){` |
|       - |  486 | `		/* return 0 */` |
|     ! 0 |  487 | `		ph7_result_int(pCtx,0);` |
|     ! 0 |  488 | `	}else{` |
|       - |  489 | `		sxi64 iVal;` |
| 1170944 |  490 | `		sxi64 iBase = 10;` |
| 1170944 |  491 | `		if( nArg > 1 ){` |
|     203 |  492 | `			iBase = ph7_value_to_int64(apArg[1]);` |
|     101 |  493 | `		}` |
| 1171033 |  494 | `		if( iBase != 10 && ph7_value_is_string(apArg[0]) ){` |
|       - |  495 | `			/* The only path php reads $base on -- and the "is it 10?" test above is` |
|       - |  496 | `			 * the LAST thing to see the argument at full width. Everything past it` |
|       - |  497 | ``			 * is strtol's `int base` parameter, which php reaches through a plain`` |
|       - |  498 | `			 * narrowing cast, so a base of 2^32+16 really does read as 16 and` |
|       - |  499 | `			 * PHP_INT_MIN really does read as 0 (auto-detect). Spelled through` |
|       - |  500 | `			 * unsigned arithmetic because the two's-complement wrap of an` |
|       - |  501 | `			 * out-of-range signed conversion is implementation-defined. */` |
|       - |  502 | `			int nLen;` |
|     179 |  503 | `			const char *zVal = ph7_value_to_string(apArg[0],&nLen);` |
|     179 |  504 | `			sxu32 uB = (sxu32)((sxu64)iBase & 0xFFFFFFFF);` |
|     179 |  505 | `			int iB = (uB <= (sxu32)SXI32_HIGH)` |
|      89 |  506 | `				? (int)uB : -(int)(SXU32_HIGH - uB) - 1;` |
|     179 |  507 | `			iVal = IntvalStrToInt64(zVal,nLen,iBase,iB);` |
|      90 |  508 | `		}else{` |
|       - |  509 | ``			/* Perform the cast -- the same one the `(int)` operator performs, so`` |
|       - |  510 | `			 * a float no int can hold warns here too. */` |
| 1170766 |  511 | `			PH7_MemObjWarnIntCast(apArg[0]);` |
| 1170766 |  512 | `			iVal = ph7_value_to_int64(apArg[0]);` |
|       - |  513 | `		}` |
| 1170944 |  514 | `		ph7_result_int64(pCtx,iVal);` |
|       - |  515 | `	}` |
| 1170944 |  516 | `	return PH7_OK;` |
|       2 |  517 | `}` |
|       - |  518 | `/*` |
|       - |  519 | ` * string strval($var)` |
|       - |  520 | ` *  Get the string representation of a variable.` |
|       - |  521 | ` * Parameter` |
|       - |  522 | ` *  $var: The variable being processed.` |
|       - |  523 | ` * Return` |
|       - |  524 | ` *  the string value of a variable.` |
|       - |  525 | ` */` |
|       8 |  526 | `static int PH7_builtin_strval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  527 | `{` |
|      10 |  528 | `	if( nArg < 1 ){` |
|       - |  529 | `		/* return NULL */` |
|     ! 0 |  530 | `		ph7_result_null(pCtx);` |
|     ! 0 |  531 | `	}else{` |
|       - |  532 | `		const char *zVal;` |
|      10 |  533 | `		int iLen = 0; /* cc -O6 warning */` |
|       - |  534 | `		/* Perform the cast. It is the USER-VISIBLE one: strval() is php's` |
|       - |  535 | `		 * (string) cast spelled as a function, so an object with no` |
|       - |  536 | `		 * __toString() throws there too (it used to answer "Object"). */` |
|      10 |  537 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&zVal,&iLen);` |
|      10 |  538 | `		if( rcSv != SXRET_OK ){` |
|       3 |  539 | `			return rcSv;` |
|       - |  540 | `		}` |
|       8 |  541 | `		ph7_result_string(pCtx,zVal,iLen);` |
|       - |  542 | `	}` |
|       8 |  543 | `	return PH7_OK;` |
|       6 |  544 | `}` |
|       - |  545 | `/*` |
|       - |  546 | ` * bool boolval($var)` |
|       - |  547 | ` *  Get the boolean value of a variable.` |
|       - |  548 | ` * Parameter` |
|       - |  549 | ` *  $var: The variable being processed.` |
|       - |  550 | ` * Return` |
|       - |  551 | ` *  the bool value of a variable.` |
|       - |  552 | ` */` |
|      24 |  553 | `static int PH7_builtin_boolval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  554 | `{` |
|       - |  555 | `	int bVal;` |
|      25 |  556 | `	if( nArg != 1 ){` |
|     ! 0 |  557 | `		return PH7_VmThrowException(pCtx,` |
|       - |  558 | `			"ArgumentCountError",` |
|       - |  559 | `			"boolval() expects exactly 1 argument, %d given",` |
|     ! 0 |  560 | `			nArg` |
|       - |  561 | `			);` |
|       - |  562 | `	}` |
|       - |  563 | `	/* Perform the cast */` |
|      25 |  564 | `	bVal = ph7_value_to_bool(apArg[0]);` |
|      25 |  565 | `	ph7_result_bool(pCtx,bVal);` |
|      25 |  566 | `	return PH7_OK;` |
|      13 |  567 | `}` |
|       - |  568 | `/*` |
|       - |  569 | ` * bool empty($var)` |
|       - |  570 | ` *  Determine whether a variable is empty.` |
|       - |  571 | ` * Parameters` |
|       - |  572 | ` *   $var: The variable being checked.` |
|       - |  573 | ` * Return` |
|       - |  574 | ` *  0 if var has a non-empty and non-zero value.1 otherwise.` |
|       - |  575 | ` */` |
|   51600 |  576 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  577 | `{` |
|   51605 |  578 | `	int res = 1; /* Assume empty by default */` |
|   51605 |  579 | `	if( nArg > 0 ){` |
|   51605 |  580 | `		res = ph7_value_is_empty(apArg[0]);` |
|   25800 |  581 | `	}` |
|   51605 |  582 | `	ph7_result_bool(pCtx,res);` |
|   51605 |  583 | `	return PH7_OK;` |
|       - |  584 |  |
|       5 |  585 | `}` |
|       - |  586 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|       - |  587 | `#define PH7_NEED_BUILTIN_REG 1` |
|       - |  588 | `#endif` |
|       - |  589 | `#ifndef PH7_DISABLE_DISK_IO` |
|       - |  590 | `#define PH7_NEED_FMT_AND_INI 1` |
|       - |  591 | `#endif` |
|       - |  592 |  |
|       - |  593 | `/* Math functions moved to builtin_math.c */` |
|       - |  594 |  |
|       - |  595 | `/* Table of the built-in functions */` |
|       - |  596 | `/*` |
|       - |  597 | ` * int memory_get_usage([bool $real_usage = false])` |
|       - |  598 | ` *  Amount of memory, in bytes, currently allocated to the script through PHL's` |
|       - |  599 | ` *  memory backend. PHL tracks the backend's real allocated bytes, so the` |
|       - |  600 | ` *  $real_usage flag has no effect here (php's non-real figure would be smaller,` |
|       - |  601 | ` *  reflecting Zend's emalloc bookkeeping — recorded divergence).` |
|       - |  602 | ` */` |
|     ! 0 |  603 | `static int PH7_builtin_memory_get_usage(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 |  604 | `{` |
|     ! 0 |  605 | `	SXUNUSED(nArg);` |
|     ! 0 |  606 | `	SXUNUSED(apArg);` |
|     ! 0 |  607 | `	ph7_result_int64(pCtx,(ph7_int64)pCtx->pVm->sAllocator.nMemUsed);` |
|     ! 0 |  608 | `	return PH7_OK;` |
|     ! 0 |  609 | `}` |
|       - |  610 | `/*` |
|       - |  611 | ` * int memory_get_peak_usage([bool $real_usage = false])` |
|       - |  612 | ` *  High-water mark of memory_get_usage() over the script's lifetime.` |
|       - |  613 | ` */` |
|       4 |  614 | `static int PH7_builtin_memory_get_peak_usage(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  615 | `{` |
|       2 |  616 | `	SXUNUSED(nArg);` |
|       2 |  617 | `	SXUNUSED(apArg);` |
|       5 |  618 | `	ph7_result_int64(pCtx,(ph7_int64)pCtx->pVm->sAllocator.nMemPeak);` |
|       5 |  619 | `	return PH7_OK;` |
|       1 |  620 | `}` |
|       - |  621 | `/*` |
|       - |  622 | ` * void memory_reset_peak_usage()` |
|       - |  623 | ` *  Reset the peak memory usage (memory_get_peak_usage) back to the current` |
|       - |  624 | ` *  live usage — php 8.2. Frameworks call it between tests to measure per-test` |
|       - |  625 | ` *  peaks.` |
|       - |  626 | ` */` |
|       4 |  627 | `static int PH7_builtin_memory_reset_peak_usage(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  628 | `{` |
|       2 |  629 | `	SXUNUSED(nArg);` |
|       2 |  630 | `	SXUNUSED(apArg);` |
|       5 |  631 | `	pCtx->pVm->sAllocator.nMemPeak = pCtx->pVm->sAllocator.nMemUsed;` |
|       5 |  632 | `	return PH7_OK;` |
|       1 |  633 | `}` |
|       - |  634 | `/*` |
|       - |  635 | ` * PHL frees values by reference count as they go out of scope, so there is no` |
|       - |  636 | ` * mark-and-sweep cycle collector to drive. The gc_* family is provided for` |
|       - |  637 | ` * source compatibility (real frameworks call it around test runs): the state is` |
|       - |  638 | ` * observational and collection is a no-op. Recorded divergence from php, whose` |
|       - |  639 | ` * collector actually reclaims reference cycles.` |
|       - |  640 | ` */` |
|     ! 0 |  641 | `static int PH7_builtin_gc_enable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 |  642 | `{` |
|     ! 0 |  643 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     ! 0 |  644 | `	pCtx->pVm->bGcEnabled = 1;` |
|     ! 0 |  645 | `	return PH7_OK;` |
|     ! 0 |  646 | `}` |
|     ! 0 |  647 | `static int PH7_builtin_gc_disable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 |  648 | `{` |
|     ! 0 |  649 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     ! 0 |  650 | `	pCtx->pVm->bGcEnabled = 0;` |
|     ! 0 |  651 | `	return PH7_OK;` |
|     ! 0 |  652 | `}` |
|     ! 0 |  653 | `static int PH7_builtin_gc_enabled(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 |  654 | `{` |
|     ! 0 |  655 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     ! 0 |  656 | `	ph7_result_bool(pCtx,pCtx->pVm->bGcEnabled);` |
|     ! 0 |  657 | `	return PH7_OK;` |
|     ! 0 |  658 | `}` |
|     ! 0 |  659 | `static int PH7_builtin_gc_collect_cycles(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 |  660 | `{` |
|       - |  661 | `	/* No cycle collector: nothing to reclaim. Returns the count collected (0). */` |
|     ! 0 |  662 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     ! 0 |  663 | `	ph7_result_int(pCtx,0);` |
|     ! 0 |  664 | `	return PH7_OK;` |
|     ! 0 |  665 | `}` |
|     ! 0 |  666 | `static int PH7_builtin_gc_mem_caches(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 |  667 | `{` |
|     ! 0 |  668 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     ! 0 |  669 | `	ph7_result_int(pCtx,0);` |
|     ! 0 |  670 | `	return PH7_OK;` |
|     ! 0 |  671 | `}` |
|       - |  672 | `/*` |
|       - |  673 | ` * array gc_status(void)` |
|       - |  674 | ` *  php 8.3 shape. PHL never runs a collection, so every counter is zero and the` |
|       - |  675 | ` *  timing fields are 0.0; 'running' reflects gc_enable()/gc_disable().` |
|       - |  676 | ` */` |
|     ! 0 |  677 | `static int PH7_builtin_gc_status(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 |  678 | `{` |
|       - |  679 | `	ph7_value *pArray,*pVal;` |
|     ! 0 |  680 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     ! 0 |  681 | `	pArray = ph7_context_new_array(pCtx);` |
|     ! 0 |  682 | `	pVal = ph7_context_new_scalar(pCtx);` |
|     ! 0 |  683 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|     ! 0 |  684 | `		ph7_result_null(pCtx);` |
|     ! 0 |  685 | `		return PH7_OK;` |
|       - |  686 | `	}` |
|       - |  687 | `	/* Key order matches php 8.3's gc_status(). */` |
|     ! 0 |  688 | `	ph7_value_bool(pVal,pCtx->pVm->bGcEnabled); ph7_array_add_strkey_elem(pArray,"running",pVal);` |
|     ! 0 |  689 | `	ph7_value_bool(pVal,0);      ph7_array_add_strkey_elem(pArray,"protected",pVal);` |
|     ! 0 |  690 | `	ph7_value_bool(pVal,0);      ph7_array_add_strkey_elem(pArray,"full",pVal);` |
|     ! 0 |  691 | `	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"runs",pVal);` |
|     ! 0 |  692 | `	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"collected",pVal);` |
|     ! 0 |  693 | `	ph7_value_int(pVal,1000);    ph7_array_add_strkey_elem(pArray,"threshold",pVal);` |
|     ! 0 |  694 | `	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"buffer_size",pVal);` |
|     ! 0 |  695 | `	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"roots",pVal);` |
|     ! 0 |  696 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"application_time",pVal);` |
|     ! 0 |  697 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"collector_time",pVal);` |
|     ! 0 |  698 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"destructor_time",pVal);` |
|     ! 0 |  699 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"free_time",pVal);` |
|     ! 0 |  700 | `	ph7_context_release_value(pCtx,pVal);` |
|     ! 0 |  701 | `	ph7_result_value(pCtx,pArray);` |
|     ! 0 |  702 | `	return PH7_OK;` |
|     ! 0 |  703 | `}` |
|       - |  704 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|       - |  705 | `	{ "memory_get_usage"     , PH7_builtin_memory_get_usage      },` |
|       - |  706 | `	{ "memory_get_peak_usage", PH7_builtin_memory_get_peak_usage },` |
|       - |  707 | `	{ "memory_reset_peak_usage", PH7_builtin_memory_reset_peak_usage },` |
|       - |  708 | `	{ "gc_enable"            , PH7_builtin_gc_enable             },` |
|       - |  709 | `	{ "gc_disable"           , PH7_builtin_gc_disable            },` |
|       - |  710 | `	{ "gc_enabled"           , PH7_builtin_gc_enabled            },` |
|       - |  711 | `	{ "gc_collect_cycles"    , PH7_builtin_gc_collect_cycles     },` |
|       - |  712 | `	{ "gc_mem_caches"        , PH7_builtin_gc_mem_caches         },` |
|       - |  713 | `	{ "gc_status"            , PH7_builtin_gc_status             },` |
|       - |  714 | `	   /* Variable handling functions */` |
|       - |  715 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|       - |  716 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|       - |  717 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|       - |  718 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|       - |  719 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|       - |  720 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|       - |  721 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|       - |  722 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|       - |  723 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|       - |  724 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|       - |  725 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|       - |  726 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|       - |  727 | `	{ "is_resource", PH7_builtin_is_resource },` |
|       - |  728 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|       - |  729 | `	{ "intval"     , PH7_builtin_intval      },` |
|       - |  730 | `	{ "strval"     , PH7_builtin_strval      },` |
|       - |  731 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|       - |  732 | `	{ "empty"      , PH7_builtin_empty       },` |
|       - |  733 | `#ifdef PH7_NEED_BUILTIN_REG` |
|       - |  734 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|       - |  735 | `	   /* Math functions */` |
|       - |  736 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|       - |  737 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|       - |  738 | `	{ "acosh" ,   PH7_builtin_acosh        },` |
|       - |  739 | `	{ "asinh" ,   PH7_builtin_asinh        },` |
|       - |  740 | `	{ "atanh" ,   PH7_builtin_atanh        },` |
|       - |  741 | `	{ "expm1" ,   PH7_builtin_expm1        },` |
|       - |  742 | `	{ "log1p" ,   PH7_builtin_log1p        },` |
|       - |  743 | `	{ "deg2rad" , PH7_builtin_deg2rad      },` |
|       - |  744 | `	{ "rad2deg" , PH7_builtin_rad2deg      },` |
|       - |  745 | `	{ "fpow" ,    PH7_builtin_fpow         },` |
|       - |  746 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|       - |  747 | `	{ "floor",    PH7_builtin_floor        },` |
|       - |  748 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|       - |  749 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|       - |  750 | `	{ "acos" ,    PH7_builtin_acos         },` |
|       - |  751 | `	{ "asin" ,    PH7_builtin_asin         },` |
|       - |  752 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|       - |  753 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|       - |  754 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|       - |  755 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|       - |  756 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|       - |  757 | `	{ "atan" ,    PH7_builtin_atan         },` |
|       - |  758 | `	{ "atan2",    PH7_builtin_atan2        },` |
|       - |  759 | `	{ "log"  ,    PH7_builtin_log          },` |
|       - |  760 | `	{ "log10" ,   PH7_builtin_log10        },` |
|       - |  761 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|       - |  762 | `	{ "pi",       PH7_builtin_pi           },` |
|       - |  763 | `	{ "fmod",     PH7_builtin_fmod         },` |
|       - |  764 | `	{ "hypot",    PH7_builtin_hypot        },` |
|       - |  765 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|       - |  766 | `	{ "round",    PH7_builtin_round        },` |
|       - |  767 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|       - |  768 | `	{ "number_format", PH7_builtin_number_format },` |
|       - |  769 | `	{ "dechex", PH7_builtin_dechex         },` |
|       - |  770 | `	{ "decoct", PH7_builtin_decoct         },` |
|       - |  771 | `	{ "decbin", PH7_builtin_decbin         },` |
|       - |  772 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|       - |  773 | `	{ "bindec", PH7_builtin_bindec         },` |
|       - |  774 | `	{ "octdec", PH7_builtin_octdec         },` |
|       - |  775 | `	{ "srand",  PH7_builtin_srand          },` |
|       - |  776 | `	{ "mt_srand",PH7_builtin_srand         },` |
|       - |  777 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|       - |  778 | `#ifdef PH7_NEED_FMT_AND_INI` |
|       - |  779 | `	{ "base_convert", PH7_builtin_base_convert },` |
|       - |  780 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|       - |  781 | `#ifdef PH7_NEED_BUILTIN_REG` |
|       - |  782 | `	   /* String handling functions */` |
|       - |  783 |  |
|       - |  784 | `	{ "substr",          PH7_builtin_substr     },` |
|       - |  785 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|       - |  786 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|       - |  787 | `	{ "substr_replace",  PH7_builtin_substr_replace },` |
|       - |  788 | `	{ "levenshtein",     PH7_builtin_levenshtein },` |
|       - |  789 | `	{ "similar_text",    PH7_builtin_similar_text },` |
|       - |  790 | `	{ "str_word_count",  PH7_builtin_str_word_count },` |
|       - |  791 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|       - |  792 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|       - |  793 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|       - |  794 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|       - |  795 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|       - |  796 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|       - |  797 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|       - |  798 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|       - |  799 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|       - |  800 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|       - |  801 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|       - |  802 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|       - |  803 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|       - |  804 | `	{ "strnatcmp"  , PH7_builtin_strnatcmp  },` |
|       - |  805 | `	{ "strnatcasecmp", PH7_builtin_strnatcmp },` |
|       - |  806 | `	{ "version_compare", PH7_builtin_version_compare },` |
|       - |  807 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|       - |  808 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|       - |  809 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|       - |  810 | `	{ "implode"    , PH7_builtin_implode    },` |
|       - |  811 | `	{ "join"       , PH7_builtin_implode    },` |
|       - |  812 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|       - |  813 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|       - |  814 | `	{ "explode"     , PH7_builtin_explode    },` |
|       - |  815 | `	{ "trim"        , PH7_builtin_trim       },` |
|       - |  816 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|       - |  817 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|       - |  818 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|       - |  819 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|       - |  820 | `	{ "mb_strtolower",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */` |
|       - |  821 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|       - |  822 | `	{ "mb_strtoupper",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */` |
|       - |  823 | `	{ "mb_strlen",    PH7_builtin_mb_strlen_f },` |
|       - |  824 | `	{ "mb_substr",    PH7_builtin_mb_substr_f },` |
|       - |  825 | `	{ "mb_convert_case", PH7_builtin_mb_convert_case_f },` |
|       - |  826 | `	{ "mb_strpos",    PH7_builtin_mb_strpos_f },` |
|       - |  827 | `	{ "mb_stripos",   PH7_builtin_mb_strpos_f },` |
|       - |  828 | `	{ "mb_strrpos",   PH7_builtin_mb_strpos_f },` |
|       - |  829 | `	{ "mb_strripos",  PH7_builtin_mb_strpos_f },` |
|       - |  830 | `	{ "mb_strstr",    PH7_builtin_mb_strstr_f },` |
|       - |  831 | `	{ "mb_stristr",   PH7_builtin_mb_strstr_f },` |
|       - |  832 | `	{ "mb_strrchr",   PH7_builtin_mb_strstr_f },` |
|       - |  833 | `	{ "mb_strrichr",  PH7_builtin_mb_strstr_f },` |
|       - |  834 | `	{ "mb_substr_count", PH7_builtin_mb_substr_count_f },` |
|       - |  835 | `	{ "mb_str_pad",   PH7_builtin_mb_str_pad_f },` |
|       - |  836 | `	{ "mb_strcut",    PH7_builtin_mb_strcut_f },` |
|       - |  837 | `	{ "mb_strimwidth", PH7_builtin_mb_strimwidth_f },` |
|       - |  838 | `	{ "mb_str_split", PH7_builtin_mb_str_split_f },` |
|       - |  839 | `	{ "mb_trim",      PH7_builtin_mb_trim_f  },` |
|       - |  840 | `	{ "mb_ltrim",     PH7_builtin_mb_trim_f  },` |
|       - |  841 | `	{ "mb_rtrim",     PH7_builtin_mb_trim_f  },` |
|       - |  842 | `	{ "mb_internal_encoding", PH7_builtin_mb_internal_encoding_f },` |
|       - |  843 | `	{ "mb_substitute_character", PH7_builtin_mb_substitute_character_f },` |
|       - |  844 | `	{ "mb_scrub",     PH7_builtin_mb_scrub_f },` |
|       - |  845 | `	{ "mb_check_encoding",    PH7_builtin_mb_check_encoding_f },` |
|       - |  846 | `	{ "mb_strwidth",  PH7_builtin_mb_strwidth_f },` |
|       - |  847 | `	{ "mb_chr",       PH7_builtin_mb_chr_f   },` |
|       - |  848 | `	{ "mb_ord",       PH7_builtin_mb_ord_f   },` |
|       - |  849 | `	{ "mb_ucfirst",   PH7_builtin_mb_ucfirst_f },` |
|       - |  850 | `	{ "mb_lcfirst",   PH7_builtin_mb_ucfirst_f },` |
|       - |  851 | `	{ "mb_detect_encoding", PH7_builtin_mb_detect_encoding_f },` |
|       - |  852 | `	{ "mb_convert_encoding", PH7_builtin_mb_convert_encoding_f },` |
|       - |  853 | `	{ "iconv",        PH7_builtin_iconv_f    }, /* builtin_iconv.c */` |
|       - |  854 | `	{ "iconv_strlen", PH7_builtin_iconv_strlen_f },` |
|       - |  855 | `	{ "iconv_substr", PH7_builtin_iconv_substr_f },` |
|       - |  856 | `	{ "iconv_strpos", PH7_builtin_iconv_strpos_f },` |
|       - |  857 | `	{ "iconv_strrpos",PH7_builtin_iconv_strrpos_f },` |
|       - |  858 | `	{ "iconv_get_encoding", PH7_builtin_iconv_get_encoding_f },` |
|       - |  859 | `	{ "iconv_mime_encode", PH7_builtin_iconv_mime_encode_f },` |
|       - |  860 | `	{ "iconv_mime_decode", PH7_builtin_iconv_mime_decode_f },` |
|       - |  861 | `	{ "iconv_mime_decode_headers", PH7_builtin_iconv_mime_decode_headers_f },` |
|       - |  862 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|       - |  863 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|       - |  864 | `	{ "ord",          PH7_builtin_ord        },` |
|       - |  865 | `	{ "chr",          PH7_builtin_chr        },` |
|       - |  866 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|       - |  867 | `	{ "strstr",       PH7_builtin_strstr     },` |
|       - |  868 | `	{ "stristr",      PH7_builtin_stristr    },` |
|       - |  869 | `	{ "strchr",       PH7_builtin_strstr     },` |
|       - |  870 | `	{ "strpos",       PH7_builtin_strpos     },` |
|       - |  871 | `	{ "stripos",      PH7_builtin_stripos    },` |
|       - |  872 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|       - |  873 | `	{ "strripos",     PH7_builtin_strripos   },` |
|       - |  874 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|       - |  875 | `	{ "strrev",       PH7_builtin_strrev     },` |
|       - |  876 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|       - |  877 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|       - |  878 | `	{ "str_contains", PH7_builtin_str_contains },` |
|       - |  879 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|       - |  880 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|       - |  881 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|       - |  882 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|       - |  883 | `#ifdef PH7_NEED_FMT_AND_INI` |
|       - |  884 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|       - |  885 | `	{ "printf",       PH7_builtin_printf     },` |
|       - |  886 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|       - |  887 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|       - |  888 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|       - |  889 | `#ifdef PH7_NEED_BUILTIN_REG` |
|       - |  890 | `	{ "size_format",  PH7_builtin_size_format},` |
|       - |  891 |  |
|       - |  892 |  |
|       - |  893 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|       - |  894 | `	{ "md5",          PH7_builtin_md5       },` |
|       - |  895 | `	{ "sha1",         PH7_builtin_sha1      },` |
|       - |  896 | `	{ "crc32",        PH7_builtin_crc32     },` |
|       - |  897 | `	{ "hash",         PH7_builtin_hash      },` |
|       - |  898 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|       - |  899 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|       - |  900 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|       - |  901 | `	{ "hash_hmac_algos", PH7_builtin_hash_hmac_algos },` |
|       - |  902 | `	{ "hash_init",    PH7_builtin_hash_init },` |
|       - |  903 | `	{ "hash_update",  PH7_builtin_hash_update },` |
|       - |  904 | `	{ "hash_final",   PH7_builtin_hash_final },` |
|       - |  905 | `	{ "hash_copy",    PH7_builtin_hash_copy },` |
|       - |  906 | `	{ "hash_pbkdf2",  PH7_builtin_hash_pbkdf2 },` |
|       - |  907 | `	{ "hash_hkdf",    PH7_builtin_hash_hkdf },` |
|       - |  908 | `	{ "crypt",        PH7_builtin_crypt     },` |
|       - |  909 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|       - |  910 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|       - |  911 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|       - |  912 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|       - |  913 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|       - |  914 | `	{ "password_algos",        PH7_builtin_password_algos },` |
|       - |  915 | `	{ "filter_var",            PH7_builtin_filter_var },` |
|       - |  916 | `	{ "filter_input",          PH7_builtin_filter_input },` |
|       - |  917 | `	{ "filter_list",           PH7_builtin_filter_list },` |
|       - |  918 | `	{ "filter_id",             PH7_builtin_filter_id },` |
|       - |  919 | `	{ "filter_has_var",        PH7_builtin_filter_has_var },` |
|       - |  920 | `	{ "filter_var_array",      PH7_builtin_filter_var_array },` |
|       - |  921 | `	{ "filter_input_array",    PH7_builtin_filter_input_array },` |
|       - |  922 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|       - |  923 | `#ifdef PH7_NEED_FMT_AND_INI` |
|       - |  924 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|       - |  925 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|       - |  926 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|       - |  927 | `#ifdef PH7_NEED_BUILTIN_REG` |
|       - |  928 |  |
|       - |  929 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|       - |  930 | `	{ "str_split",    PH7_builtin_str_split  },` |
|       - |  931 | `	{ "count_chars",  PH7_builtin_count_chars},` |
|       - |  932 | `	{ "strspn",       PH7_builtin_strspn     },` |
|       - |  933 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|       - |  934 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|       - |  935 | `	{ "soundex",      PH7_builtin_soundex    },` |
|       - |  936 | `	{ "str_rot13",    PH7_builtin_str_rot13  },` |
|       - |  937 | `	{ "metaphone",    PH7_builtin_metaphone  },` |
|       - |  938 | `	{ "pack",         PH7_builtin_pack       },` |
|       - |  939 | `	{ "unpack",       PH7_builtin_unpack     },` |
|       - |  940 | `	{ "sscanf",       PH7_builtin_sscanf     },` |
|       - |  941 | `	     /* ext/bcmath: arbitrary-precision decimal arithmetic over strings */` |
|       - |  942 | `	{ "bcadd",        PH7_builtin_bcadd      },` |
|       - |  943 | `	{ "bcsub",        PH7_builtin_bcsub      },` |
|       - |  944 | `	{ "bcmul",        PH7_builtin_bcmul      },` |
|       - |  945 | `	{ "bccomp",       PH7_builtin_bccomp     },` |
|       - |  946 | `	{ "bcdiv",        PH7_builtin_bcdiv      },` |
|       - |  947 | `	{ "bcmod",        PH7_builtin_bcmod      },` |
|       - |  948 | `	{ "bcdivmod",     PH7_builtin_bcdivmod   },` |
|       - |  949 | `	{ "bcpow",        PH7_builtin_bcpow      },` |
|       - |  950 | `	{ "bcpowmod",     PH7_builtin_bcpowmod   },` |
|       - |  951 | `	{ "bcsqrt",       PH7_builtin_bcsqrt     },` |
|       - |  952 | `	{ "bcround",      PH7_builtin_bcround    },` |
|       - |  953 | `	{ "bcfloor",      PH7_builtin_bcfloor    },` |
|       - |  954 | `	{ "bcceil",       PH7_builtin_bcceil     },` |
|       - |  955 | `	{ "bcscale",      PH7_builtin_bcscale    },` |
|       - |  956 | `	     /* ext/calendar: the serial day number and its four calendars */` |
|       - |  957 | `	{ "gregoriantojd",PH7_builtin_gregoriantojd },` |
|       - |  958 | `	{ "jdtogregorian",PH7_builtin_jdtogregorian },` |
|       - |  959 | `	{ "juliantojd",   PH7_builtin_juliantojd    },` |
|       - |  960 | `	{ "jdtojulian",   PH7_builtin_jdtojulian    },` |
|       - |  961 | `	{ "frenchtojd",   PH7_builtin_frenchtojd    },` |
|       - |  962 | `	{ "jdtofrench",   PH7_builtin_jdtofrench    },` |
|       - |  963 | `	{ "jewishtojd",   PH7_builtin_jewishtojd    },` |
|       - |  964 | `	{ "jdtojewish",   PH7_builtin_jdtojewish    },` |
|       - |  965 | `	{ "cal_info",     PH7_builtin_cal_info      },` |
|       - |  966 | `	{ "cal_days_in_month", PH7_builtin_cal_days_in_month },` |
|       - |  967 | `	{ "cal_to_jd",    PH7_builtin_cal_to_jd     },` |
|       - |  968 | `	{ "cal_from_jd",  PH7_builtin_cal_from_jd   },` |
|       - |  969 | `	{ "jddayofweek",  PH7_builtin_jddayofweek   },` |
|       - |  970 | `	{ "jdmonthname",  PH7_builtin_jdmonthname   },` |
|       - |  971 | `	{ "unixtojd",     PH7_builtin_unixtojd      },` |
|       - |  972 | `	{ "jdtounix",     PH7_builtin_jdtounix      },` |
|       - |  973 | `	{ "easter_days",  PH7_builtin_easter_days   },` |
|       - |  974 | `	{ "easter_date",  PH7_builtin_easter_date   },` |
|       - |  975 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|       - |  976 | `	{ "strtok",       PH7_builtin_strtok     },` |
|       - |  977 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|       - |  978 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|       - |  979 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|       - |  980 | `	{ "strtr",        PH7_builtin_strtr      },` |
|       - |  981 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|       - |  982 | `#ifdef PH7_NEED_FMT_AND_INI` |
|       - |  983 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|       - |  984 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|       - |  985 | `#ifdef PH7_NEED_BUILTIN_REG` |
|       - |  986 |  |
|       - |  987 | `	         /* Ctype functions */` |
|       - |  988 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|       - |  989 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|       - |  990 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|       - |  991 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|       - |  992 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|       - |  993 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|       - |  994 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|       - |  995 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|       - |  996 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|       - |  997 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|       - |  998 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|       - |  999 | `	         /* Time functions */` |
|       - | 1000 | `	{ "time"    ,    PH7_builtin_time         },` |
|       - | 1001 | `	{ "microtime",   PH7_builtin_microtime    },` |
|       - | 1002 | `	{ "hrtime",      PH7_builtin_hrtime       },` |
|       - | 1003 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|       - | 1004 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|       - | 1005 | `	{ "date",        PH7_builtin_date         },` |
|       - | 1006 | `	{ "idate",       PH7_builtin_idate        },` |
|       - | 1007 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|       - | 1008 | `	{ "localtime",   PH7_builtin_localtime    },` |
|       - | 1009 | `	{ "mktime",      PH7_builtin_mktime       },` |
|       - | 1010 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|       - | 1011 | `	{ "date_default_timezone_get", PH7_builtin_date_default_timezone_get },` |
|       - | 1012 | `	{ "date_default_timezone_set", PH7_builtin_date_default_timezone_set },` |
|       - | 1013 | `	        /* URL functions */` |
|       - | 1014 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|       - | 1015 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|       - | 1016 | `	{ "convert_uuencode",PH7_builtin_convert_uuencode },` |
|       - | 1017 | `	{ "convert_uudecode",PH7_builtin_convert_uudecode },` |
|       - | 1018 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|       - | 1019 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|       - | 1020 | `	{ "rawurlencode", PH7_builtin_rawurlencode },` |
|       - | 1021 | `	{ "http_build_query", PH7_builtin_http_build_query },` |
|       - | 1022 | `	{ "parse_str",    PH7_builtin_parse_str  },` |
|       - | 1023 | `	{ "rawurldecode", PH7_builtin_rawurldecode },` |
|       - | 1024 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|       - | 1025 | `};` |
|       - | 1026 | `/*` |
|       - | 1027 | ` * Register the built-in functions defined above,the array functions` |
|       - | 1028 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|       - | 1029 | ` */` |
|    4964 | 1030 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|       5 | 1031 | `{` |
|       - | 1032 | `	sxu32 n;` |
| 1419709 | 1033 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 1414745 | 1034 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
|  707660 | 1035 | `	}` |
|       - | 1036 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|    4969 | 1037 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|       - | 1038 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|    4969 | 1039 | `	PH7_RegisterIORoutine(&(*pVm));` |
|    4969 | 1040 | `}` |
|       - | 1041 |  |
|       - | 1042 | `/*` |
|       - | 1043 | ` * UTF-8 codepoint reader shared by the glob/fnmatch matcher in vfs.c.` |
|       - | 1044 | ` * Relocated here from the removed vm_xml.c when the legacy xml_* API was` |
|       - | 1045 | ` * dropped; the utf8_encode()/utf8_decode() builtins it once served were` |
|       - | 1046 | ` * removed in turn (superseded by mb_convert_encoding()),` |
|       - | 1047 | ` * leaving only this public-domain SQLite reader.` |
|       - | 1048 | ` */` |
|       - | 1049 | `/* SPDX-SnippetBegin */` |
|       - | 1050 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|       - | 1051 | `/* SPDX-License-Identifier: blessing */` |
|       - | 1052 | `/*` |
|       - | 1053 | ` * UTF-8 decoding routine extracted from the sqlite3 source tree.` |
|       - | 1054 | ` * Original author: D. Richard Hipp (http://www.sqlite.org)` |
|       - | 1055 | ` * Status: Public Domain` |
|       - | 1056 | ` */` |
|       - | 1057 | `/*` |
|       - | 1058 | `** This lookup table is used to help decode the first byte of` |
|       - | 1059 | `** a multi-byte UTF8 character.` |
|       - | 1060 | `*/` |
|       - | 1061 | `static const unsigned char UtfTrans1[] = {` |
|       - | 1062 | `  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,` |
|       - | 1063 | `  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,` |
|       - | 1064 | `  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,` |
|       - | 1065 | `  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,` |
|       - | 1066 | `  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,` |
|       - | 1067 | `  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,` |
|       - | 1068 | `  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,` |
|       - | 1069 | `  0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x00, 0x00,` |
|       - | 1070 | `};` |
|       - | 1071 | `/*` |
|       - | 1072 | `** Translate a single UTF-8 character.  Return the unicode value.` |
|       - | 1073 | `**` |
|       - | 1074 | `** During translation, assume that the byte that zTerm points` |
|       - | 1075 | `** is a 0x00.` |
|       - | 1076 | `**` |
|       - | 1077 | `** Write a pointer to the next unread byte back into *pzNext.` |
|       - | 1078 | `**` |
|       - | 1079 | `** Notes On Invalid UTF-8:` |
|       - | 1080 | `**` |
|       - | 1081 | `**  *  This routine never allows a 7-bit character (0x00 through 0x7f) to` |
|       - | 1082 | `**     be encoded as a multi-byte character.  Any multi-byte character that` |
|       - | 1083 | `**     attempts to encode a value between 0x00 and 0x7f is rendered as 0xfffd.` |
|       - | 1084 | `**` |
|       - | 1085 | `**  *  This routine never allows a UTF16 surrogate value to be encoded.` |
|       - | 1086 | `**     If a multi-byte character attempts to encode a value between` |
|       - | 1087 | `**     0xd800 and 0xe000 then it is rendered as 0xfffd.` |
|       - | 1088 | `**` |
|       - | 1089 | `**  *  Bytes in the range of 0x80 through 0xbf which occur as the first` |
|       - | 1090 | `**     byte of a character are interpreted as single-byte characters` |
|       - | 1091 | `**     and rendered as themselves even though they are technically` |
|       - | 1092 | `**     invalid characters.` |
|       - | 1093 | `**` |
|       - | 1094 | `**  *  This routine accepts an infinite number of different UTF8 encodings` |
|       - | 1095 | `**     for unicode values 0x80 and greater.  It do not change over-length` |
|       - | 1096 | `**     encodings to 0xfffd as some systems recommend.` |
|       - | 1097 | `*/` |
|       - | 1098 | `#define READ_UTF8(zIn, zTerm, c)                           \` |
|       - | 1099 | `  c = *(zIn++);                                            \` |
|       - | 1100 | `  if( c>=0xc0 ){                                           \` |
|       - | 1101 | `    c = UtfTrans1[c-0xc0];                                 \` |
|       - | 1102 | `    while( zIn!=zTerm && (*zIn & 0xc0)==0x80 ){            \` |
|       - | 1103 | `      c = (c<<6) + (0x3f & *(zIn++));                      \` |
|       - | 1104 | `    }                                                      \` |
|       - | 1105 | `    if( c<0x80                                             \` |
|       - | 1106 | `        \|\| (c&0xFFFFF800)==0xD800                          \` |
|       - | 1107 | `        \|\| (c&0xFFFFFFFE)==0xFFFE ){  c = 0xFFFD; }        \` |
|       - | 1108 | `  }` |
|    9402 | 1109 | `PH7_PRIVATE int PH7_Utf8Read(` |
|       - | 1110 | `  const unsigned char *z,         /* First byte of UTF-8 character */` |
|       - | 1111 | `  const unsigned char *zTerm,     /* Pretend this byte is 0x00 */` |
|       - | 1112 | `  const unsigned char **pzNext    /* Write first byte past UTF-8 char here */` |
|       3 | 1113 | `){` |
|       - | 1114 | `  int c;` |
|    9405 | 1115 | `  READ_UTF8(z, zTerm, c);` |
|    9405 | 1116 | `  *pzNext = z;` |
|    9405 | 1117 | `  return c;` |
|       3 | 1118 | `}` |
|       - | 1119 | `/* SPDX-SnippetEnd */` |
|       - | 1120 | `/*` |
|       - | 1121 | ` * Read one STRICTLY well-formed UTF-8 sequence from z[0..n-1].` |
|       - | 1122 | ` *` |
|       - | 1123 | ` * Unlike PH7_Utf8Read above (the lenient SQLite reader, which renders anything` |
|       - | 1124 | ` * dubious as U+FFFD and happily accepts over-long forms), this one implements` |
|       - | 1125 | ` * the RFC 3629 / Unicode "Table 3-7 well-formed byte sequences" rule php uses` |
|       - | 1126 | ` * wherever it has to decide whether a php string really is UTF-8:` |
|       - | 1127 | ` *` |
|       - | 1128 | ` *   00..7F                          one byte` |
|       - | 1129 | ` *   C2..DF  80..BF                  (C0/C1 are over-long two-byte forms)` |
|       - | 1130 | ` *   E0      A0..BF  80..BF          (E0 80..9F is over-long)` |
|       - | 1131 | ` *   E1..EC  80..BF  80..BF` |
|       - | 1132 | ` *   ED      80..9F  80..BF          (ED A0..BF is a UTF-16 surrogate)` |
|       - | 1133 | ` *   EE..EF  80..BF  80..BF` |
|       - | 1134 | ` *   F0      90..BF  80..BF  80..BF  (F0 80..8F is over-long)` |
|       - | 1135 | ` *   F1..F3  80..BF  80..BF  80..BF` |
|       - | 1136 | ` *   F4      80..8F  80..BF  80..BF  (past U+10FFFF)` |
|       - | 1137 | ` *` |
|       - | 1138 | ` * Returns the code point and writes the sequence length to *pLen. On an` |
|       - | 1139 | ` * ill-formed sequence it returns -1 and writes 1, so a caller can apply its own` |
|       - | 1140 | ` * php policy to the single offending byte (json_encode: JSON_ERROR_UTF8 or the` |
|       - | 1141 | ` * JSON_INVALID_UTF8_* substitution; mb_strtolower: '?') and resume at the next` |
|       - | 1142 | ` * byte exactly like php does. n must be >= 1.` |
|       - | 1143 | ` */` |
|   46555 | 1144 | `PH7_PRIVATE sxi32 PH7_Utf8ReadStrict(const unsigned char *z,sxu32 n,sxu32 *pLen)` |
|       2 | 1145 | `{` |
|   46557 | 1146 | `	sxu32 c = z[0];` |
|   46557 | 1147 | `	*pLen = 1;` |
|   46557 | 1148 | `	if( c < 0x80 ){` |
|   44484 | 1149 | `		return (sxi32)c;` |
|       - | 1150 | `	}` |
|    2074 | 1151 | `	if( c >= 0xC2 && c <= 0xDF ){` |
|    1104 | 1152 | `		if( n < 2 \|\| (z[1] & 0xC0) != 0x80 ){` |
|      63 | 1153 | `			return -1;` |
|       - | 1154 | `		}` |
|    1042 | 1155 | `		*pLen = 2;` |
|    1042 | 1156 | `		return (sxi32)(((c & 0x1F) << 6) \| (z[1] & 0x3F));` |
|       - | 1157 | `	}` |
|     971 | 1158 | `	if( c >= 0xE0 && c <= 0xEF ){` |
|     381 | 1159 | `		sxu32 iLow = (c == 0xE0) ? 0xA0 : 0x80;` |
|     381 | 1160 | `		sxu32 iHigh = (c == 0xED) ? 0x9F : 0xBF;` |
|     381 | 1161 | `		if( n < 3 \|\| z[1] < iLow \|\| z[1] > iHigh \|\| (z[2] & 0xC0) != 0x80 ){` |
|      99 | 1162 | `			return -1;` |
|       - | 1163 | `		}` |
|     283 | 1164 | `		*pLen = 3;` |
|     283 | 1165 | `		return (sxi32)(((c & 0x0F) << 12) \| ((z[1] & 0x3F) << 6) \| (z[2] & 0x3F));` |
|       - | 1166 | `	}` |
|     591 | 1167 | `	if( c >= 0xF0 && c <= 0xF4 ){` |
|     103 | 1168 | `		sxu32 iLow = (c == 0xF0) ? 0x90 : 0x80;` |
|     103 | 1169 | `		sxu32 iHigh = (c == 0xF4) ? 0x8F : 0xBF;` |
|     102 | 1170 | `		if( n < 4 \|\| z[1] < iLow \|\| z[1] > iHigh` |
|      79 | 1171 | `		 \|\| (z[2] & 0xC0) != 0x80 \|\| (z[3] & 0xC0) != 0x80 ){` |
|      31 | 1172 | `			return -1;` |
|       - | 1173 | `		}` |
|      73 | 1174 | `		*pLen = 4;` |
|     109 | 1175 | `		return (sxi32)(((c & 0x07) << 18) \| ((z[1] & 0x3F) << 12)` |
|      72 | 1176 | `			\| ((z[2] & 0x3F) << 6) \| (z[3] & 0x3F));` |
|       - | 1177 | `	}` |
|     489 | 1178 | `	return -1; /* 80..C1 as a lead byte, or F5..FF */` |
|   27928 | 1179 | `}` |
|       - | 1180 |  |
