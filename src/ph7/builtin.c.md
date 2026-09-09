# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4380/5056 lines (86.63%)

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
| 482060 |   18 | `PH7_PRIVATE sxi32 PH7_IntArgResolve(` |
|      - |   19 | `	ph7_context *pCtx,` |
|      - |   20 | `	ph7_value *pArg,` |
|      - |   21 | `	const char *zFunc,` |
|      - |   22 | `	int iArgNum,` |
|      - |   23 | `	const char *zParamName,` |
|      - |   24 | `	const char *zTypeStr,` |
|      - |   25 | `	sxi64 *pOut` |
|      5 |   26 | `){` |
| 482065 |   27 | `	if( ph7_value_is_null(pArg) ){` |
|    ! 0 |   28 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |   29 | `			"%s(): Passing null to parameter #%d (%s) of type %s is deprecated",` |
|    ! 0 |   30 | `			zFunc,iArgNum,zParamName,zTypeStr` |
|      - |   31 | `			);` |
|    ! 0 |   32 | `		*pOut = 0;` |
|    ! 0 |   33 | `		return PH7_OK;` |
|      - |   34 | `	}` |
| 482065 |   35 | `	if( ph7_value_is_float(pArg) ){` |
|      7 |   36 | `		double dVal = ph7_value_to_double(pArg);` |
|      - |   37 | `		sxi64 iVal;` |
|      - |   38 | `		/* php: NAN/INF/out-of-int64-range floats fail ZPP outright */` |
|      7 |   39 | `		if( dVal != dVal \|\| dVal >= 9223372036854775808.0 \|\| dVal < -9223372036854775808.0 ){` |
|      7 |   40 | `			return PH7_VmThrowException(pCtx,` |
|      - |   41 | `				"TypeError",` |
|      - |   42 | `				"%s(): Argument #%d (%s) must be of type %s, float given",` |
|      2 |   43 | `				zFunc,iArgNum,zParamName,zTypeStr` |
|      - |   44 | `				);` |
|      - |   45 | `		}` |
|      3 |   46 | `		iVal = (sxi64)dVal;` |
|      3 |   47 | `		if( (double)iVal != dVal ){` |
|    ! 0 |   48 | `			PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |   49 | `				"Implicit conversion from float %s to int loses precision",` |
|    ! 0 |   50 | `				ph7_value_to_string(pArg,0)` |
|      - |   51 | `				);` |
|    ! 0 |   52 | `		}` |
|      3 |   53 | `		*pOut = iVal;` |
|      3 |   54 | `		return PH7_OK;` |
|      - |   55 | `	}` |
| 482059 |   56 | `	if( ph7_value_is_string(pArg) ){` |
|      - |   57 | `		const char *zNum;` |
|      - |   58 | `		int nSlen;` |
|     15 |   59 | `		int i,bFloat = 0;` |
|     15 |   60 | `		if( !PH7_MemObjStringIsNumeric(pArg) ){` |
|     16 |   61 | `			return PH7_VmThrowException(pCtx,` |
|      - |   62 | `				"TypeError",` |
|      - |   63 | `				"%s(): Argument #%d (%s) must be of type %s, string given",` |
|      5 |   64 | `				zFunc,iArgNum,zParamName,zTypeStr` |
|      - |   65 | `				);` |
|      - |   66 | `		}` |
|      5 |   67 | `		zNum = ph7_value_to_string(pArg,&nSlen);` |
|      9 |   68 | `		for( i = 0 ; i < nSlen ; i++ ){` |
|      5 |   69 | `			if( zNum[i] == '.' \|\| zNum[i] == 'e' \|\| zNum[i] == 'E' ){` |
|    ! 0 |   70 | `				bFloat = 1;` |
|    ! 0 |   71 | `				break;` |
|      - |   72 | `			}` |
|      3 |   73 | `		}` |
|      5 |   74 | `		if( bFloat ){` |
|    ! 0 |   75 | `			double dVal = 0;` |
|      - |   76 | `			sxi64 iVal;` |
|    ! 0 |   77 | `			SyStrToReal(zNum,(sxu32)nSlen,(void *)&dVal,0);` |
|    ! 0 |   78 | `			if( dVal != dVal \|\| dVal >= 9223372036854775808.0 \|\| dVal < -9223372036854775808.0 ){` |
|    ! 0 |   79 | `				return PH7_VmThrowException(pCtx,` |
|      - |   80 | `					"TypeError",` |
|      - |   81 | `					"%s(): Argument #%d (%s) must be of type %s, string given",` |
|    ! 0 |   82 | `					zFunc,iArgNum,zParamName,zTypeStr` |
|      - |   83 | `					);` |
|      - |   84 | `			}` |
|    ! 0 |   85 | `			iVal = (sxi64)dVal;` |
|    ! 0 |   86 | `			if( (double)iVal != dVal ){` |
|    ! 0 |   87 | `				PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |   88 | `					"Implicit conversion from float-string \"%s\" to int loses precision",` |
|    ! 0 |   89 | `					zNum` |
|      - |   90 | `					);` |
|    ! 0 |   91 | `			}` |
|    ! 0 |   92 | `			*pOut = iVal;` |
|    ! 0 |   93 | `			return PH7_OK;` |
|      - |   94 | `		}` |
|      5 |   95 | `		*pOut = ph7_value_to_int64(pArg);` |
|      5 |   96 | `		return PH7_OK;` |
|      - |   97 | `	}` |
| 482045 |   98 | `	if( !ph7_value_is_int(pArg) && !ph7_value_is_bool(pArg) ){` |
|      - |   99 | `		/* Arrays, resources and objects: php names the class for objects */` |
|    ! 0 |  100 | `		const char *zType = ph7_type_name(pArg);` |
|    ! 0 |  101 | `		if( ph7_value_is_object(pArg) ){` |
|    ! 0 |  102 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|    ! 0 |  103 | `			if( pInst && pInst->pClass ){` |
|    ! 0 |  104 | `				zType = SyStringData(&pInst->pClass->sName);` |
|    ! 0 |  105 | `			}` |
|    ! 0 |  106 | `		}` |
|    ! 0 |  107 | `		return PH7_VmThrowException(pCtx,` |
|      - |  108 | `			"TypeError",` |
|      - |  109 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|    ! 0 |  110 | `			zFunc,iArgNum,zParamName,zTypeStr,zType` |
|      - |  111 | `			);` |
|      - |  112 | `	}` |
| 482045 |  113 | `	*pOut = ph7_value_to_int64(pArg);` |
| 482045 |  114 | `	return PH7_OK;` |
| 241035 |  115 | `}` |
|      - |  116 |  |
|      - |  117 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |  118 | `/* Forward decl: null-to-string ZPP deprecation notice (defined near the ZPP` |
|      - |  119 | ` * helpers; both live inside the same DISABLE_BUILTIN_FUNC region as every` |
|      - |  120 | ` * caller — the tiny build compiles none of them). */` |
|      - |  121 | `static void StrNullArgNotice(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNum,const char *zParamName);` |
|      - |  122 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - |  123 | `/* This file implement built-in 'foreign' functions for the PH7 engine */` |
|      - |  124 | `/*` |
|      - |  125 | ` * Section:` |
|      - |  126 | ` *    Variable handling Functions.` |
|      - |  127 | ` * Status:` |
|      - |  128 | ` *    Stable.` |
|      - |  129 | ` */` |
|      - |  130 | `/*` |
|      - |  131 | ` * bool is_bool($var)` |
|      - |  132 | ` *  Finds out whether a variable is a boolean.` |
|      - |  133 | ` * Parameters` |
|      - |  134 | ` *   $var: The variable being evaluated.` |
|      - |  135 | ` * Return` |
|      - |  136 | ` *  TRUE if var is a boolean. False otherwise.` |
|      - |  137 | ` */` |
|     72 |  138 | `static int PH7_builtin_is_bool(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  139 | `{` |
|     74 |  140 | `	int res = 0; /* Assume false by default */` |
|     74 |  141 | `	if( nArg > 0 ){` |
|     74 |  142 | `		res = ph7_value_is_bool(apArg[0]);` |
|     36 |  143 | `	}` |
|      - |  144 | `	/* Query result */` |
|     74 |  145 | `	ph7_result_bool(pCtx,res);` |
|     74 |  146 | `	return PH7_OK;` |
|      2 |  147 | `}` |
|      - |  148 | `/*` |
|      - |  149 | ` * bool is_float($var)` |
|      - |  150 | ` * bool is_real($var)` |
|      - |  151 | ` * bool is_double($var)` |
|      - |  152 | ` *  Finds out whether a variable is a float.` |
|      - |  153 | ` * Parameters` |
|      - |  154 | ` *   $var: The variable being evaluated.` |
|      - |  155 | ` * Return` |
|      - |  156 | ` *  TRUE if var is a float. False otherwise.` |
|      - |  157 | ` */` |
|    308 |  158 | `static int PH7_builtin_is_float(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  159 | `{` |
|    309 |  160 | `	int res = 0; /* Assume false by default */` |
|    309 |  161 | `	if( nArg > 0 ){` |
|    309 |  162 | `		res = ph7_value_is_float(apArg[0]);` |
|    154 |  163 | `	}` |
|      - |  164 | `	/* Query result */` |
|    309 |  165 | `	ph7_result_bool(pCtx,res);` |
|    309 |  166 | `	return PH7_OK;` |
|      1 |  167 | `}` |
|      - |  168 | `/*` |
|      - |  169 | ` * bool is_int($var)` |
|      - |  170 | ` * bool is_integer($var)` |
|      - |  171 | ` * bool is_long($var)` |
|      - |  172 | ` *  Finds out whether a variable is an integer.` |
|      - |  173 | ` * Parameters` |
|      - |  174 | ` *   $var: The variable being evaluated.` |
|      - |  175 | ` * Return` |
|      - |  176 | ` *  TRUE if var is an integer. False otherwise.` |
|      - |  177 | ` */` |
|    922 |  178 | `static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  179 | `{` |
|    925 |  180 | `	int res = 0; /* Assume false by default */` |
|    925 |  181 | `	if( nArg > 0 ){` |
|      - |  182 | `		/* Strict PHP identity: a float is never an int, even when it holds an` |
|      - |  183 | `		 * integer value (1.0). An integer-valued real carries both MEMOBJ_INT` |
|      - |  184 | `		 * (cached) and MEMOBJ_REAL, so REAL must be excluded here. */` |
|    925 |  185 | `		res = ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]);` |
|    461 |  186 | `	}` |
|      - |  187 | `	/* Query result */` |
|    925 |  188 | `	ph7_result_bool(pCtx,res);` |
|    925 |  189 | `	return PH7_OK;` |
|      3 |  190 | `}` |
|      - |  191 | `/*` |
|      - |  192 | ` * bool is_string($var)` |
|      - |  193 | ` *  Finds out whether a variable is a string.` |
|      - |  194 | ` * Parameters` |
|      - |  195 | ` *   $var: The variable being evaluated.` |
|      - |  196 | ` * Return` |
|      - |  197 | ` *  TRUE if var is string. False otherwise.` |
|      - |  198 | ` */` |
|    772 |  199 | `static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  200 | `{` |
|    775 |  201 | `	int res = 0; /* Assume false by default */` |
|    775 |  202 | `	if( nArg > 0 ){` |
|    775 |  203 | `		res = ph7_value_is_string(apArg[0]);` |
|    386 |  204 | `	}` |
|      - |  205 | `	/* Query result */` |
|    775 |  206 | `	ph7_result_bool(pCtx,res);` |
|    775 |  207 | `	return PH7_OK;` |
|      3 |  208 | `}` |
|      - |  209 | `/*` |
|      - |  210 | ` * bool is_null($var)` |
|      - |  211 | ` *  Finds out whether a variable is NULL.` |
|      - |  212 | ` * Parameters` |
|      - |  213 | ` *   $var: The variable being evaluated.` |
|      - |  214 | ` * Return` |
|      - |  215 | ` *  TRUE if var is NULL. False otherwise.` |
|      - |  216 | ` */` |
|     86 |  217 | `static int PH7_builtin_is_null(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  218 | `{` |
|     90 |  219 | `	int res = 0; /* Assume false by default */` |
|     90 |  220 | `	if( nArg > 0 ){` |
|     90 |  221 | `		res = ph7_value_is_null(apArg[0]);` |
|     43 |  222 | `	}` |
|      - |  223 | `	/* Query result */` |
|     90 |  224 | `	ph7_result_bool(pCtx,res);` |
|     90 |  225 | `	return PH7_OK;` |
|      4 |  226 | `}` |
|      - |  227 | `/*` |
|      - |  228 | ` * bool is_numeric($var)` |
|      - |  229 | ` *  Find out whether a variable is NULL.` |
|      - |  230 | ` * Parameters` |
|      - |  231 | ` *  $var: The variable being evaluated.` |
|      - |  232 | ` * Return` |
|      - |  233 | ` *  True if var is numeric. False otherwise.` |
|      - |  234 | ` */` |
|     66 |  235 | `static int PH7_builtin_is_numeric(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  236 | `{` |
|     71 |  237 | `	int res = 0; /* Assume false by default */` |
|     71 |  238 | `	if( nArg > 0 ){` |
|     71 |  239 | `		res = ph7_value_is_numeric(apArg[0]);` |
|     33 |  240 | `	}` |
|      - |  241 | `	/* Query result */` |
|     71 |  242 | `	ph7_result_bool(pCtx,res);` |
|     71 |  243 | `	return PH7_OK;` |
|      5 |  244 | `}` |
|      - |  245 | `/*` |
|      - |  246 | ` * bool is_scalar($var)` |
|      - |  247 | ` *  Find out whether a variable is a scalar.` |
|      - |  248 | ` * Parameters` |
|      - |  249 | ` *  $var: The variable being evaluated.` |
|      - |  250 | ` * Return` |
|      - |  251 | ` *  True if var is scalar. False otherwise.` |
|      - |  252 | ` */` |
|     12 |  253 | `static int PH7_builtin_is_scalar(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  254 | `{` |
|     13 |  255 | `	int res = 0; /* Assume false by default */` |
|     13 |  256 | `	if( nArg > 0 ){` |
|     13 |  257 | `		res = ph7_value_is_scalar(apArg[0]);` |
|      6 |  258 | `	}` |
|      - |  259 | `	/* Query result */` |
|     13 |  260 | `	ph7_result_bool(pCtx,res);` |
|     13 |  261 | `	return PH7_OK;` |
|      1 |  262 | `}` |
|      - |  263 | `/*` |
|      - |  264 | ` * bool is_array($var)` |
|      - |  265 | ` *  Find out whether a variable is an array.` |
|      - |  266 | ` * Parameters` |
|      - |  267 | ` *  $var: The variable being evaluated.` |
|      - |  268 | ` * Return` |
|      - |  269 | ` *  True if var is an array. False otherwise.` |
|      - |  270 | ` */` |
|    760 |  271 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  272 | `{` |
|    764 |  273 | `	int res = 0; /* Assume false by default */` |
|    764 |  274 | `	if( nArg > 0 ){` |
|    764 |  275 | `		res = ph7_value_is_array(apArg[0]);` |
|    380 |  276 | `	}` |
|      - |  277 | `	/* Query result */` |
|    764 |  278 | `	ph7_result_bool(pCtx,res);` |
|    764 |  279 | `	return PH7_OK;` |
|      4 |  280 | `}` |
|      - |  281 | `/*` |
|      - |  282 | ` * bool is_object($var)` |
|      - |  283 | ` *  Find out whether a variable is an object.` |
|      - |  284 | ` * Parameters` |
|      - |  285 | ` *  $var: The variable being evaluated.` |
|      - |  286 | ` * Return` |
|      - |  287 | ` *  True if var is an object. False otherwise.` |
|      - |  288 | ` */` |
|    516 |  289 | `static int PH7_builtin_is_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  290 | `{` |
|    519 |  291 | `	int res = 0; /* Assume false by default */` |
|    519 |  292 | `	if( nArg > 0 ){` |
|    519 |  293 | `		res = ph7_value_is_object(apArg[0]);` |
|    258 |  294 | `	}` |
|      - |  295 | `	/* Query result */` |
|    519 |  296 | `	ph7_result_bool(pCtx,res);` |
|    519 |  297 | `	return PH7_OK;` |
|      3 |  298 | `}` |
|      - |  299 | `/*` |
|      - |  300 | ` * bool is_resource($var)` |
|      - |  301 | ` *  Find out whether a variable is a resource.` |
|      - |  302 | ` * Parameters` |
|      - |  303 | ` *  $var: The variable being evaluated.` |
|      - |  304 | ` * Return` |
|      - |  305 | ` *  True if a resource. False otherwise.` |
|      - |  306 | ` */` |
|     62 |  307 | `static int PH7_builtin_is_resource(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  308 | `{` |
|     66 |  309 | `	int res = 0; /* Assume false by default */` |
|     66 |  310 | `	if( nArg > 0 ){` |
|     66 |  311 | `		res = ph7_value_is_resource(apArg[0]);` |
|     31 |  312 | `	}` |
|     66 |  313 | `	ph7_result_bool(pCtx,res);` |
|     66 |  314 | `	return PH7_OK;` |
|      4 |  315 | `}` |
|      - |  316 | `/*` |
|      - |  317 | ` * float floatval($var)` |
|      - |  318 | ` *  Get float value of a variable.` |
|      - |  319 | ` * Parameter` |
|      - |  320 | ` *  $var: The variable being processed.` |
|      - |  321 | ` * Return` |
|      - |  322 | ` *  the float value of a variable.` |
|      - |  323 | ` */` |
|      4 |  324 | `static int PH7_builtin_floatval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  325 | `{` |
|      5 |  326 | `	if( nArg < 1 ){` |
|      - |  327 | `		/* return 0.0 */` |
|    ! 0 |  328 | `		ph7_result_double(pCtx,0);` |
|    ! 0 |  329 | `	}else{` |
|      - |  330 | `		double dval;` |
|      - |  331 | `		/* Perform the cast */` |
|      5 |  332 | `		dval = ph7_value_to_double(apArg[0]);` |
|      5 |  333 | `		ph7_result_double(pCtx,dval);` |
|      - |  334 | `	}` |
|      5 |  335 | `	return PH7_OK;` |
|      1 |  336 | `}` |
|      - |  337 | `/*` |
|      - |  338 | ` * int intval($var)` |
|      - |  339 | ` *  Get integer value of a variable.` |
|      - |  340 | ` * Parameter` |
|      - |  341 | ` *  $var: The variable being processed.` |
|      - |  342 | ` * Return` |
|      - |  343 | ` *  the int value of a variable.` |
|      - |  344 | ` */` |
|     46 |  345 | `static int PH7_builtin_intval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  346 | `{` |
|     47 |  347 | `	if( nArg < 1 ){` |
|      - |  348 | `		/* return 0 */` |
|    ! 0 |  349 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  350 | `	}else{` |
|      - |  351 | `		sxi64 iVal;` |
|      - |  352 | `		/* Perform the cast */` |
|     47 |  353 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|     47 |  354 | `		ph7_result_int64(pCtx,iVal);` |
|      - |  355 | `	}` |
|     47 |  356 | `	return PH7_OK;` |
|      1 |  357 | `}` |
|      - |  358 | `/*` |
|      - |  359 | ` * string strval($var)` |
|      - |  360 | ` *  Get the string representation of a variable.` |
|      - |  361 | ` * Parameter` |
|      - |  362 | ` *  $var: The variable being processed.` |
|      - |  363 | ` * Return` |
|      - |  364 | ` *  the string value of a variable.` |
|      - |  365 | ` */` |
|      2 |  366 | `static int PH7_builtin_strval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  367 | `{` |
|      3 |  368 | `	if( nArg < 1 ){` |
|      - |  369 | `		/* return NULL */` |
|    ! 0 |  370 | `		ph7_result_null(pCtx);` |
|    ! 0 |  371 | `	}else{` |
|      - |  372 | `		const char *zVal;` |
|      3 |  373 | `		int iLen = 0; /* cc -O6 warning */` |
|      - |  374 | `		/* Perform the cast */` |
|      3 |  375 | `		zVal = ph7_value_to_string(apArg[0],&iLen);` |
|      3 |  376 | `		ph7_result_string(pCtx,zVal,iLen);` |
|      - |  377 | `	}` |
|      3 |  378 | `	return PH7_OK;` |
|      1 |  379 | `}` |
|      - |  380 | `/*` |
|      - |  381 | ` * bool boolval($var)` |
|      - |  382 | ` *  Get the boolean value of a variable.` |
|      - |  383 | ` * Parameter` |
|      - |  384 | ` *  $var: The variable being processed.` |
|      - |  385 | ` * Return` |
|      - |  386 | ` *  the bool value of a variable.` |
|      - |  387 | ` */` |
|     14 |  388 | `static int PH7_builtin_boolval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  389 | `{` |
|      - |  390 | `	int bVal;` |
|     15 |  391 | `	if( nArg != 1 ){` |
|    ! 0 |  392 | `		return PH7_VmThrowException(pCtx,` |
|      - |  393 | `			"ArgumentCountError",` |
|      - |  394 | `			"boolval() expects exactly 1 argument, %d given",` |
|    ! 0 |  395 | `			nArg` |
|      - |  396 | `			);` |
|      - |  397 | `	}` |
|      - |  398 | `	/* Perform the cast */` |
|     15 |  399 | `	bVal = ph7_value_to_bool(apArg[0]);` |
|     15 |  400 | `	ph7_result_bool(pCtx,bVal);` |
|     15 |  401 | `	return PH7_OK;` |
|      8 |  402 | `}` |
|      - |  403 | `/*` |
|      - |  404 | ` * bool empty($var)` |
|      - |  405 | ` *  Determine whether a variable is empty.` |
|      - |  406 | ` * Parameters` |
|      - |  407 | ` *   $var: The variable being checked.` |
|      - |  408 | ` * Return` |
|      - |  409 | ` *  0 if var has a non-empty and non-zero value.1 otherwise.` |
|      - |  410 | ` */` |
|  33566 |  411 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  412 | `{` |
|  33571 |  413 | `	int res = 1; /* Assume empty by default */` |
|  33571 |  414 | `	if( nArg > 0 ){` |
|  33569 |  415 | `		res = ph7_value_is_empty(apArg[0]);` |
|  16782 |  416 | `	}` |
|  33571 |  417 | `	ph7_result_bool(pCtx,res);` |
|  33571 |  418 | `	return PH7_OK;` |
|      - |  419 |  |
|      5 |  420 | `}` |
|      - |  421 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |  422 | `#define PH7_NEED_BUILTIN_REG 1` |
|      - |  423 | `#endif` |
|      - |  424 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - |  425 | `#define PH7_NEED_FMT_AND_INI 1` |
|      - |  426 | `#endif` |
|      - |  427 |  |
|      - |  428 | `/* Math functions moved to builtin_math.c */` |
|      - |  429 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - |  430 | `/*` |
|      - |  431 | ` * Section:` |
|      - |  432 | ` *    String handling Functions.` |
|      - |  433 | ` * Status:` |
|      - |  434 | ` *    Stable.` |
|      - |  435 | ` */` |
|      - |  436 | `/*` |
|      - |  437 | ` * string substr(string $string,int $start[, int $length ])` |
|      - |  438 | ` *  Return part of a string.` |
|      - |  439 | ` * Parameters` |
|      - |  440 | ` *  $string` |
|      - |  441 | ` *   The input string. Must be one character or longer.` |
|      - |  442 | ` * $start` |
|      - |  443 | ` *   If start is non-negative, the returned string will start at the start'th position` |
|      - |  444 | ` *   in string, counting from zero. For instance, in the string 'abcdef', the character` |
|      - |  445 | ` *   at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - |  446 | ` *   If start is negative, the returned string will start at the start'th character` |
|      - |  447 | ` *   from the end of string.` |
|      - |  448 | ` *   If string is less than or equal to start characters long, FALSE will be returned.` |
|      - |  449 | ` * $length` |
|      - |  450 | ` *   If length is given and is positive, the string returned will contain at most length` |
|      - |  451 | ` *   characters beginning from start (depending on the length of string).` |
|      - |  452 | ` *   If length is given and is negative, then that many characters will be omitted from` |
|      - |  453 | ` *   the end of string (after the start position has been calculated when a start is negative).` |
|      - |  454 | ` *   If start denotes the position of this truncation or beyond, false will be returned.` |
|      - |  455 | ` *   If length is given and is 0, FALSE or NULL an empty string will be returned.` |
|      - |  456 | ` *   If length is omitted, the substring starting from start until the end of the string` |
|      - |  457 | ` *   will be returned.` |
|      - |  458 | ` * Return` |
|      - |  459 | ` *  Returns the extracted part of string, or FALSE on failure or an empty string.` |
|      - |  460 | ` */` |
| 265886 |  461 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  462 | `{` |
|      - |  463 | `	const char *zSource;` |
|      - |  464 | `	int nSrcLen;` |
|      - |  465 | `	sxi64 iStart,iEnd;` |
| 265891 |  466 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"substr",1,"$string"); }` |
| 265891 |  467 | `	if( nArg < 2 ){` |
|      - |  468 | `		/* Arity is enforced at the call boundary; nothing sensible to return here. */` |
|    ! 0 |  469 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 |  470 | `		return PH7_OK;` |
|      - |  471 | `	}` |
|      - |  472 | `	/* Extract the target string */` |
| 265891 |  473 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|      - |  474 | `	/* Extract the offset */` |
|      - |  475 | `	{` |
| 265891 |  476 | `		sxi64 iTmp = 0;` |
| 265891 |  477 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"substr",2,"$offset","int",&iTmp);` |
| 265891 |  478 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  479 | `			return rcArg;` |
|      - |  480 | `		}` |
| 265891 |  481 | `		iStart = iTmp;` |
|      - |  482 | `	}` |
|      - |  483 | `	/*` |
|      - |  484 | `	 * php 8 never answers substr() with FALSE — every out-of-range window simply` |
|      - |  485 | `	 * clamps to the empty string (substr("",0), substr("abc",5) and` |
|      - |  486 | `	 * substr("abc",1,-5) are all ""). PH7 returned FALSE for each of those, which` |
|      - |  487 | `	 * then flowed on as a bool into string context.` |
|      - |  488 | `	 *` |
|      - |  489 | `	 * A negative offset counts back from the end (clamped to 0); a negative length` |
|      - |  490 | `	 * leaves that many bytes off the end. Computed in sxi64 so an INT64 offset or` |
|      - |  491 | `	 * length cannot overflow the window arithmetic.` |
|      - |  492 | `	 */` |
| 265891 |  493 | `	if( iStart < 0 ){` |
|  32969 |  494 | `		iStart += nSrcLen;` |
|  32969 |  495 | `		if( iStart < 0 ){` |
|      5 |  496 | `			iStart = 0;` |
|      7 |  497 | `		}` |
| 249409 |  498 | `	}else if( iStart > nSrcLen ){` |
|      7 |  499 | `		iStart = nSrcLen;` |
|      3 |  500 | `	}` |
| 265891 |  501 | `	iEnd = nSrcLen;` |
| 265891 |  502 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
| 195215 |  503 | `		sxi64 iLen = 0;` |
| 195215 |  504 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[2],"substr",3,"$length","?int",&iLen);` |
| 195215 |  505 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  506 | `			return rcArg;` |
|      - |  507 | `		}` |
| 195215 |  508 | `		if( iLen < 0 ){` |
|  32901 |  509 | `			iEnd = (sxi64)nSrcLen + iLen;` |
| 178767 |  510 | `		}else if( iLen > (sxi64)nSrcLen - iStart ){` |
|  18767 |  511 | `			iEnd = nSrcLen;` |
|   9386 |  512 | `		}else{` |
| 143557 |  513 | `			iEnd = iStart + iLen;` |
|      - |  514 | `		}` |
|  97605 |  515 | `	}` |
| 265891 |  516 | `	if( iEnd < iStart ){` |
|      3 |  517 | `		iEnd = iStart;` |
|      1 |  518 | `	}` |
| 265891 |  519 | `	ph7_result_string(pCtx,&zSource[iStart],(int)(iEnd - iStart));` |
| 265891 |  520 | `	return PH7_OK;` |
| 132948 |  521 | `}` |
|      - |  522 | `/*` |
|      - |  523 | ` * int substr_compare(string $main_str,string $str ,int $offset[,int $length[,bool $case_insensitivity = false ]])` |
|      - |  524 | ` *  Binary safe comparison of two strings from an offset, up to length characters.` |
|      - |  525 | ` * Parameters` |
|      - |  526 | ` *  $main_str` |
|      - |  527 | ` *  The main string being compared.` |
|      - |  528 | ` *  $str` |
|      - |  529 | ` *   The secondary string being compared.` |
|      - |  530 | ` * $offset` |
|      - |  531 | ` *  The start position for the comparison. If negative, it starts counting from` |
|      - |  532 | ` *  the end of the string.` |
|      - |  533 | ` * $length` |
|      - |  534 | ` *  The length of the comparison. The default value is the largest of the length` |
|      - |  535 | ` *  of the str compared to the length of main_str less the offset.` |
|      - |  536 | ` * $case_insensitivity` |
|      - |  537 | ` *  If case_insensitivity is TRUE, comparison is case insensitive.` |
|      - |  538 | ` * Return` |
|      - |  539 | ` *  Returns < 0 if main_str from position offset is less than str, > 0 if it is greater than` |
|      - |  540 | ` *  str, and 0 if they are equal. If offset is equal to or greater than the length of main_str` |
|      - |  541 | ` *  or length is set and is less than 1, substr_compare() prints a warning and returns FALSE.` |
|      - |  542 | ` */` |
|     20 |  543 | `static int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  544 | `{` |
|      - |  545 | `	const char *zSource,*zSub;` |
|      - |  546 | `	int nSrcLen,nSubLen;` |
|      - |  547 | `	sxi64 iOfft,iLen,l1,l2,nCmp;` |
|     21 |  548 | `	int iCase = 0;` |
|      - |  549 | `	int rc;` |
|     21 |  550 | `	if( nArg < 3 ){` |
|    ! 0 |  551 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  552 | `		return PH7_OK;` |
|      - |  553 | `	}` |
|     21 |  554 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|     21 |  555 | `	zSub    = ph7_value_to_string(apArg[1],&nSubLen);` |
|      - |  556 | `	{` |
|     21 |  557 | `		sxi64 iTmp = 0;` |
|     21 |  558 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[2],"substr_compare",3,"$offset","int",&iTmp);` |
|     21 |  559 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  560 | `			return rcArg;` |
|      - |  561 | `		}` |
|     21 |  562 | `		iOfft = iTmp;` |
|      - |  563 | `	}` |
|     21 |  564 | `	if( iOfft < 0 ){` |
|      5 |  565 | `		iOfft += nSrcLen;` |
|      5 |  566 | `		if( iOfft < 0 ){` |
|      3 |  567 | `			iOfft = 0;` |
|      1 |  568 | `		}` |
|      2 |  569 | `	}` |
|     21 |  570 | `	if( iOfft > nSrcLen ){` |
|      - |  571 | `		/* php rejects an offset past the end of the haystack outright */` |
|    ! 0 |  572 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  573 | `			"substr_compare(): Argument #3 ($offset) must be contained in argument #1 ($haystack)");` |
|      - |  574 | `	}` |
|      - |  575 | `	/* A NULL/absent length compares as far as the longer of the two operands reaches */` |
|     21 |  576 | `	iLen = (sxi64)nSrcLen - iOfft;` |
|     21 |  577 | `	if( iLen < nSubLen ){` |
|      5 |  578 | `		iLen = nSubLen;` |
|      2 |  579 | `	}` |
|     21 |  580 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|     11 |  581 | `		sxi64 iTmp = 0;` |
|     11 |  582 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[3],"substr_compare",4,"$length","?int",&iTmp);` |
|     11 |  583 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  584 | `			return rcArg;` |
|      - |  585 | `		}` |
|     11 |  586 | `		if( iTmp < 0 ){` |
|      3 |  587 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  588 | `				"substr_compare(): Argument #4 ($length) must be greater than or equal to 0");` |
|      - |  589 | `		}` |
|      9 |  590 | `		iLen = iTmp;` |
|      4 |  591 | `	}` |
|     19 |  592 | `	if( nArg > 4 ){` |
|      5 |  593 | `		iCase = ph7_value_to_bool(apArg[4]);` |
|      2 |  594 | `	}` |
|      - |  595 | `	/* Each side contributes at most what it actually has left */` |
|     19 |  596 | `	l1 = (sxi64)nSrcLen - iOfft;` |
|     19 |  597 | `	if( l1 > iLen ){ l1 = iLen; }` |
|     19 |  598 | `	l2 = nSubLen;` |
|     19 |  599 | `	if( l2 > iLen ){ l2 = iLen; }` |
|     19 |  600 | `	nCmp = (l1 < l2) ? l1 : l2;` |
|     19 |  601 | `	if( iCase ){` |
|      3 |  602 | `		rc = SyStrnicmp(&zSource[iOfft],zSub,(sxu32)nCmp);` |
|      2 |  603 | `	}else{` |
|     17 |  604 | `		rc = SyStrncmp(&zSource[iOfft],zSub,(sxu32)nCmp);` |
|      - |  605 | `	}` |
|     19 |  606 | `	if( rc == 0 ){` |
|      - |  607 | `		/* Prefixes equal: php falls back to a THREE-WAY compare of the lengths, so this` |
|      - |  608 | `		 * arm is normalized to -1/0/1 (substr_compare("abc","",0) is 1, not 3). */` |
|      9 |  609 | `		rc = (l1 == l2) ? 0 : (l1 < l2 ? -1 : 1);` |
|      4 |  610 | `	}` |
|      - |  611 | `	/* ...but when the prefixes differ php returns the RAW byte difference, not its sign:` |
|      - |  612 | `	 * substr_compare("abc","def",1,10) is -2 ('b' - 'd'), which is what SyMemcmp gives. */` |
|     19 |  613 | `	ph7_result_int(pCtx,rc);` |
|     19 |  614 | `	return PH7_OK;` |
|     11 |  615 | `}` |
|      - |  616 | `/*` |
|      - |  617 | ` * int substr_count(string $haystack,string $needle[,int $offset = 0 [,int $length ]])` |
|      - |  618 | ` *  Count the number of substring occurrences.` |
|      - |  619 | ` * Parameters` |
|      - |  620 | ` * $haystack` |
|      - |  621 | ` *   The string to search in` |
|      - |  622 | ` * $needle` |
|      - |  623 | ` *   The substring to search for` |
|      - |  624 | ` * $offset` |
|      - |  625 | ` *  The offset where to start counting` |
|      - |  626 | ` * $length (NOT USED)` |
|      - |  627 | ` *  The maximum length after the specified offset to search for the substring.` |
|      - |  628 | ` *  It outputs a warning if the offset plus the length is greater than the haystack length.` |
|      - |  629 | ` * Return` |
|      - |  630 | ` *  Toral number of substring occurrences.` |
|      - |  631 | ` */` |
|     26 |  632 | `static int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  633 | `{` |
|      - |  634 | `	const char *zText,*zPattern,*zEnd;` |
|      - |  635 | `	int nTextlen,nPatlen;` |
|     27 |  636 | `	int iCount = 0;` |
|      - |  637 | `	sxu32 nOfft;` |
|      - |  638 | `	sxi32 rc;` |
|     27 |  639 | `	if( nArg < 2 ){` |
|      - |  640 | `		/* Missing arguments */` |
|    ! 0 |  641 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  642 | `		return PH7_OK;` |
|      - |  643 | `	}` |
|      - |  644 | `	/* Point to the haystack */` |
|     27 |  645 | `	zText = ph7_value_to_string(apArg[0],&nTextlen);` |
|      - |  646 | `	/* Point to the neddle */` |
|     27 |  647 | `	zPattern = ph7_value_to_string(apArg[1],&nPatlen);` |
|     27 |  648 | `	if( nPatlen < 1 ){` |
|      - |  649 | `		/* Empty needle: PHP 8 throws a catchable ValueError. */` |
|      3 |  650 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  651 | `			"substr_count(): Argument #2 ($needle) must not be empty");` |
|      - |  652 | `	}` |
|      - |  653 | `	/* Apply the optional $offset/$length window before searching. PHP 8 validates` |
|      - |  654 | `	 * both against the haystack (a negative value counts from the end) and throws a` |
|      - |  655 | `	 * catchable ValueError when the result falls outside it — this happens before the` |
|      - |  656 | `	 * needle-fits check, so it fires even when the needle is longer than the haystack. */` |
|     25 |  657 | `	if( nArg > 2 ){` |
|     19 |  658 | `		ph7_int64 iOfft = ph7_value_to_int64(apArg[2]);` |
|     19 |  659 | `		if( iOfft < 0 ){` |
|      5 |  660 | `			iOfft += nTextlen;` |
|      2 |  661 | `		}` |
|     19 |  662 | `		if( iOfft < 0 \|\| iOfft > nTextlen ){` |
|      3 |  663 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  664 | `				"substr_count(): Argument #3 ($offset) must be contained in argument #1 ($haystack)");` |
|      - |  665 | `		}` |
|      - |  666 | `		/* Point to the desired offset and shrink the remaining region */` |
|     17 |  667 | `		zText = &zText[iOfft];` |
|     17 |  668 | `		nTextlen -= (int)iOfft;` |
|      8 |  669 | `	}` |
|     23 |  670 | `	if( nArg > 3 ){` |
|     15 |  671 | `		ph7_int64 nLen = ph7_value_to_int64(apArg[3]);` |
|     15 |  672 | `		if( nLen < 0 ){` |
|      - |  673 | `			/* Negative length is relative to the end of the (offset) haystack */` |
|      5 |  674 | `			nLen += nTextlen;` |
|      2 |  675 | `		}` |
|     15 |  676 | `		if( nLen < 0 \|\| nLen > nTextlen ){` |
|      5 |  677 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  678 | `				"substr_count(): Argument #4 ($length) must be contained in argument #1 ($haystack)");` |
|      - |  679 | `		}` |
|     11 |  680 | `		nTextlen = (int)nLen;` |
|      5 |  681 | `	}` |
|     19 |  682 | `	if( nTextlen < 1 \|\| nPatlen > nTextlen ){` |
|      - |  683 | `		/* The windowed haystack can't contain the needle: zero matches */` |
|      3 |  684 | `		ph7_result_int(pCtx,0);` |
|      3 |  685 | `		return PH7_OK;` |
|      - |  686 | `	}` |
|      - |  687 | `	/* Point to the end of the windowed haystack */` |
|     17 |  688 | `	zEnd = &zText[nTextlen];` |
|      - |  689 | `	/* Perform the search */` |
|     17 |  690 | `	for(;;){` |
|     35 |  691 | `		rc = SyBlobSearch((const void *)zText,(sxu32)(zEnd-zText),(const void *)zPattern,nPatlen,&nOfft);` |
|     35 |  692 | `		if( rc != SXRET_OK ){` |
|      - |  693 | `			/* Pattern not found,break immediately */` |
|     13 |  694 | `			break;` |
|      - |  695 | `		}` |
|      - |  696 | `		/* Increment counter and update the offset */` |
|     23 |  697 | `		iCount++;` |
|     23 |  698 | `		zText += nOfft + nPatlen;` |
|     23 |  699 | `		if( zText >= zEnd ){` |
|      5 |  700 | `			break;` |
|      - |  701 | `		}` |
|      1 |  702 | `	}` |
|      - |  703 | `	/* Pattern count */` |
|     17 |  704 | `	ph7_result_int(pCtx,iCount);` |
|     17 |  705 | `	return PH7_OK;` |
|     14 |  706 | `}` |
|      - |  707 | `/* Forward declarations: defined with the trim/addcslashes and str_contains` |
|      - |  708 | ` * families below. */` |
|      - |  709 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256]);` |
|      - |  710 | `/*` |
|      - |  711 | ` * php 8.1 null-to-non-nullable ZPP deprecation, notice-only form for the` |
|      - |  712 | ` * legacy string builtins that still coerce null to "" themselves: emit` |
|      - |  713 | ``  * `f(): Passing null to parameter #N ($name) of type string is deprecated` `` |
|      - |  714 | ` * when the arg is an actual null, leaving the resolution unchanged.` |
|      - |  715 | ` */` |
| 392332 |  716 | `static void StrNullArgNotice(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNum,const char *zParamName)` |
|      5 |  717 | `{` |
| 392337 |  718 | `	if( ph7_value_is_null(pArg) ){` |
|     22 |  719 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |  720 | `			"%s(): Passing null to parameter #%d (%s) of type string is deprecated",` |
|      7 |  721 | `			zFunc,iArgNum,zParamName);` |
|      7 |  722 | `	}` |
| 392337 |  723 | `}` |
|      - |  724 | `static sxi32 StrPredicateResolveArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,` |
|      - |  725 | `	int iArgNum,const char *zParamName,const char *zTypeStr,const char *zNullMsg,` |
|      - |  726 | `	ph7_value *pTmp,const char **pzOut,int *pnOut);` |
|      - |  727 | `/*` |
|      - |  728 | ` * Validate and resolve an int-typed builtin parameter with php-8 ZPP weak-mode` |
|      - |  729 | ` * semantics: ints and bools pass through; null emits the 8.1 deprecation and` |
|      - |  730 | ` * resolves to 0; floats and float-strings convert, with the implicit-conversion` |
|      - |  731 | ` * E_DEPRECATED when lossy and a TypeError when NAN/INF/out of int range;` |
|      - |  732 | ` * integral numeric strings convert exactly; everything else (arrays, resources,` |
|      - |  733 | ` * objects, non-numeric strings) is a TypeError naming zTypeStr (e.g. "int",` |
|      - |  734 | ` * "array\|int"). Returns PH7_OK with *pOut set, or the throw status.` |
|      - |  735 | ` */` |
|      - |  736 | `/*` |
|      - |  737 | ` * Normalize a substr_replace() offset/length pair against a string of nStrLen` |
|      - |  738 | ` * bytes, exactly like PHP: a negative offset counts from the end (clamped to 0),` |
|      - |  739 | ` * an offset past the end clamps to the end; a negative length leaves that many` |
|      - |  740 | ` * bytes off the end of the remaining region (clamped to 0), and the length is` |
|      - |  741 | ` * finally clamped to the remaining region. Written without f+l additions so an` |
|      - |  742 | ` * INT64_MAX length cannot overflow.` |
|      - |  743 | ` */` |
|     60 |  744 | `static void SubstrReplaceWindow(sxi64 *pF,sxi64 *pL,int nStrLen)` |
|      1 |  745 | `{` |
|     61 |  746 | `	sxi64 f = *pF,l = *pL;` |
|     61 |  747 | `	if( f < 0 ){` |
|      9 |  748 | `		f += nStrLen;` |
|      9 |  749 | `		if( f < 0 ){` |
|      5 |  750 | `			f = 0;` |
|      3 |  751 | `		}` |
|     57 |  752 | `	}else if( f > nStrLen ){` |
|      5 |  753 | `		f = nStrLen;` |
|      2 |  754 | `	}` |
|     61 |  755 | `	if( l < 0 ){` |
|      7 |  756 | `		l += nStrLen - f;` |
|      7 |  757 | `		if( l < 0 ){` |
|      5 |  758 | `			l = 0;` |
|      2 |  759 | `		}` |
|      3 |  760 | `	}` |
|     61 |  761 | `	if( l > nStrLen - f ){` |
|     25 |  762 | `		l = nStrLen - f;` |
|     12 |  763 | `	}` |
|     61 |  764 | `	*pF = f;` |
|     61 |  765 | `	*pL = l;` |
|     61 |  766 | `}` |
|      - |  767 | `/* A replacement string collected out of substr_replace()'s $replace array.` |
|      - |  768 | ` * The bytes live in a shared pool blob (walker values are transient), so the` |
|      - |  769 | ` * item stores pool offsets, mirroring the strtr_entry technique. */` |
|      - |  770 | `typedef struct substr_repl_item substr_repl_item;` |
|      - |  771 | `struct substr_repl_item` |
|      - |  772 | `{` |
|      - |  773 | `	sxu32 nOfft; /* Offset of the string inside the pool */` |
|      - |  774 | `	sxu32 nLen;  /* Length of the string */` |
|      - |  775 | `};` |
|      - |  776 | `typedef struct substr_replace_collect substr_replace_collect;` |
|      - |  777 | `struct substr_replace_collect` |
|      - |  778 | `{` |
|      - |  779 | `	SyBlob *pPool;  /* Byte pool for string items (string walker only) */` |
|      - |  780 | `	SySet *pSet;    /* substr_repl_item set (string) or sxi64 set (int) */` |
|      - |  781 | `	sxi32 rc;       /* SXRET_OK or SXERR_MEM on collector failure */` |
|      - |  782 | `};` |
|      - |  783 | `/* ph7_array_walk() callback: append one $replace element to the pool. */` |
|      6 |  784 | `static int SubstrReplaceStrWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  785 | `{` |
|      7 |  786 | `	substr_replace_collect *pCol = (substr_replace_collect *)pUserData;` |
|      - |  787 | `	substr_repl_item sItem;` |
|      - |  788 | `	const char *zStr;` |
|      - |  789 | `	int nLen;` |
|      3 |  790 | `	SXUNUSED(pKey);` |
|      7 |  791 | `	zStr = ph7_value_to_string(pData,&nLen);` |
|      7 |  792 | `	sItem.nOfft = SyBlobLength(pCol->pPool);` |
|      7 |  793 | `	sItem.nLen = (sxu32)nLen;` |
|      7 |  794 | `	if( nLen > 0 && SXRET_OK != SyBlobAppend(pCol->pPool,(const void *)zStr,(sxu32)nLen) ){` |
|    ! 0 |  795 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  796 | `		return SXERR_ABORT;` |
|      - |  797 | `	}` |
|      7 |  798 | `	if( SXRET_OK != SySetPut(pCol->pSet,(const void *)&sItem) ){` |
|    ! 0 |  799 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  800 | `		return SXERR_ABORT;` |
|      - |  801 | `	}` |
|      7 |  802 | `	return PH7_OK;` |
|      4 |  803 | `}` |
|      - |  804 | `/* ph7_array_walk() callback: collect one $offset/$length element as an int. */` |
|     12 |  805 | `static int SubstrReplaceIntWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  806 | `{` |
|     13 |  807 | `	substr_replace_collect *pCol = (substr_replace_collect *)pUserData;` |
|     13 |  808 | `	sxi64 iVal = ph7_value_to_int64(pData);` |
|      6 |  809 | `	SXUNUSED(pKey);` |
|     13 |  810 | `	if( SXRET_OK != SySetPut(pCol->pSet,(const void *)&iVal) ){` |
|    ! 0 |  811 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  812 | `		return SXERR_ABORT;` |
|      - |  813 | `	}` |
|     13 |  814 | `	return PH7_OK;` |
|      7 |  815 | `}` |
|      - |  816 | `/* Per-element state while walking substr_replace()'s array $string. */` |
|      - |  817 | `typedef struct substr_replace_ctx substr_replace_ctx;` |
|      - |  818 | `struct substr_replace_ctx` |
|      - |  819 | `{` |
|      - |  820 | `	ph7_value *pResult;   /* Result array (keys preserved) */` |
|      - |  821 | `	ph7_value *pScratch;  /* Reusable string value for each element */` |
|      - |  822 | `	SyBlob *pReplPool;    /* Pool behind aRepl items */` |
|      - |  823 | `	SySet *pRepl;         /* substr_repl_item set or NULL when $replace is scalar */` |
|      - |  824 | `	SySet *pFrom;         /* sxi64 set or NULL when $offset is scalar */` |
|      - |  825 | `	SySet *pLen;          /* sxi64 set or NULL when $length is scalar/absent */` |
|      - |  826 | `	sxu32 iReplCur;       /* Next-position cursors into the three sets */` |
|      - |  827 | `	sxu32 iFromCur;` |
|      - |  828 | `	sxu32 iLenCur;` |
|      - |  829 | `	const char *zRepl;    /* Scalar $replace */` |
|      - |  830 | `	int nRepl;` |
|      - |  831 | `	sxi64 iFrom;          /* Scalar $offset */` |
|      - |  832 | `	sxi64 iLen;           /* Scalar $length */` |
|      - |  833 | `	int bLenGiven;        /* FALSE: $length absent/null -> element length */` |
|      - |  834 | `	sxi32 rc;             /* SXRET_OK or SXERR_MEM */` |
|      - |  835 | `};` |
|      - |  836 | `/*` |
|      - |  837 | ` * ph7_array_walk() callback over the array $string: replace the window of one` |
|      - |  838 | ` * element and insert the result under the element's original key. Array-form` |
|      - |  839 | ` * $replace/$offset/$length are consumed positionally; when a set runs out PHP` |
|      - |  840 | ` * falls back to ""/0/element-length respectively.` |
|      - |  841 | ` */` |
|     24 |  842 | `static int SubstrReplaceElemWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  843 | `{` |
|     25 |  844 | `	substr_replace_ctx *pRep = (substr_replace_ctx *)pUserData;` |
|      - |  845 | `	const char *zStr,*zRepl;` |
|      - |  846 | `	sxi64 f,l;` |
|      - |  847 | `	int nLen,nRepl;` |
|     25 |  848 | `	zStr = ph7_value_to_string(pData,&nLen);` |
|      - |  849 | `	/* Positional $replace element ("" when exhausted) */` |
|     25 |  850 | `	if( pRep->pRepl ){` |
|     11 |  851 | `		if( pRep->iReplCur < SySetUsed(pRep->pRepl) ){` |
|      7 |  852 | `			substr_repl_item *pItem = (substr_repl_item *)SySetAt(pRep->pRepl,pRep->iReplCur++);` |
|      7 |  853 | `			zRepl = (const char *)SyBlobDataAt(pRep->pReplPool,pItem->nOfft);` |
|      7 |  854 | `			nRepl = (int)pItem->nLen;` |
|      4 |  855 | `		}else{` |
|      5 |  856 | `			zRepl = "";` |
|      5 |  857 | `			nRepl = 0;` |
|      - |  858 | `		}` |
|      6 |  859 | `	}else{` |
|     15 |  860 | `		zRepl = pRep->zRepl;` |
|     15 |  861 | `		nRepl = pRep->nRepl;` |
|      - |  862 | `	}` |
|      - |  863 | `	/* Positional $offset element (0 when exhausted) */` |
|     25 |  864 | `	if( pRep->pFrom ){` |
|     13 |  865 | `		sxi64 *pVal = 0;` |
|     13 |  866 | `		if( pRep->iFromCur < SySetUsed(pRep->pFrom) ){` |
|      9 |  867 | `			pVal = (sxi64 *)SySetAt(pRep->pFrom,pRep->iFromCur++);` |
|      4 |  868 | `		}` |
|     13 |  869 | `		f = pVal ? *pVal : 0;` |
|      7 |  870 | `	}else{` |
|     13 |  871 | `		f = pRep->iFrom;` |
|      - |  872 | `	}` |
|      - |  873 | `	/* Positional $length element (element length when exhausted) */` |
|     25 |  874 | `	if( pRep->pLen ){` |
|      7 |  875 | `		sxi64 *pVal = 0;` |
|      7 |  876 | `		if( pRep->iLenCur < SySetUsed(pRep->pLen) ){` |
|      5 |  877 | `			pVal = (sxi64 *)SySetAt(pRep->pLen,pRep->iLenCur++);` |
|      2 |  878 | `		}` |
|      7 |  879 | `		l = pVal ? *pVal : nLen;` |
|      4 |  880 | `	}else{` |
|     19 |  881 | `		l = pRep->bLenGiven ? pRep->iLen : nLen;` |
|      - |  882 | `	}` |
|     25 |  883 | `	SubstrReplaceWindow(&f,&l,nLen);` |
|      - |  884 | `	/* Assemble prefix + replacement + suffix in the scratch value */` |
|     25 |  885 | `	ph7_value_reset_string_cursor(pRep->pScratch);` |
|     24 |  886 | `	if( (f > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,zStr,(int)f))` |
|     24 |  887 | `	 \|\| (nRepl > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,zRepl,nRepl))` |
|     40 |  888 | `	 \|\| (nLen - (int)(f+l) > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,&zStr[f+l],nLen - (int)(f+l))) ){` |
|     30 |  889 | `		pRep->rc = SXERR_MEM;` |
|     30 |  890 | `		return SXERR_ABORT;` |
|      - |  891 | `	}` |
|     25 |  892 | `	if( SXRET_OK != ph7_array_add_elem(pRep->pResult,pKey,pRep->pScratch) ){` |
|    ! 0 |  893 | `		pRep->rc = SXERR_MEM;` |
|    ! 0 |  894 | `		return SXERR_ABORT;` |
|      - |  895 | `	}` |
|     25 |  896 | `	return PH7_OK;` |
|     43 |  897 | `}` |
|      - |  898 | `/*` |
|      - |  899 | ` * mixed substr_replace(array\|string $string,array\|string $replace,array\|int $offset[,array\|int\|null $length = null])` |
|      - |  900 | ` *  Replace text within a portion of a string.` |
|      - |  901 | ` * Parameters` |
|      - |  902 | ` *  $string` |
|      - |  903 | ` *   The input string or an array of strings (each element is processed with` |
|      - |  904 | ` *   its own positional replace/offset/length when those are arrays too).` |
|      - |  905 | ` *  $replace` |
|      - |  906 | ` *   The replacement string. When $string is scalar and $replace is an array,` |
|      - |  907 | ` *   only its first element is used (PHP quirk).` |
|      - |  908 | ` *  $offset` |
|      - |  909 | ` *   Window start; negative counts from the end of the string.` |
|      - |  910 | ` *  $length` |
|      - |  911 | ` *   Window length; negative leaves that many bytes at the end; null/absent` |
|      - |  912 | ` *   means "to the end of the string".` |
|      - |  913 | ` * Return` |
|      - |  914 | ` *  The processed string, or an array of processed strings (keys preserved).` |
|      - |  915 | ` * Errors` |
|      - |  916 | ` *  ArgumentCountError on fewer than 3 arguments; TypeError when an array` |
|      - |  917 | ` *  $offset/$length is combined with a scalar $string.` |
|      - |  918 | ` */` |
|     58 |  919 | `static int PH7_builtin_substr_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  920 | `{` |
|      - |  921 | `	ph7_value sStrTmp,sReplTmp;` |
|     59 |  922 | `	const char *zStr = 0,*zRepl = 0;` |
|     59 |  923 | `	int nLen = 0,nRepl = 0;` |
|      - |  924 | `	int bLenGiven;` |
|     59 |  925 | `	sxi64 f = 0,l = 0;` |
|      - |  926 | `	sxi32 rc;` |
|     59 |  927 | `	if( nArg < 3 ){` |
|    ! 0 |  928 | `		return PH7_VmThrowException(pCtx,` |
|      - |  929 | `			"ArgumentCountError",` |
|      - |  930 | `			"substr_replace() expects at least 3 arguments, %d given",` |
|    ! 0 |  931 | `			nArg` |
|      - |  932 | `			);` |
|      - |  933 | `	}` |
|      - |  934 | `	/* $length counts as given unless absent or null (php: ?null semantics) */` |
|     59 |  935 | `	bLenGiven = (nArg > 3 && !ph7_value_is_null(apArg[3]));` |
|      - |  936 | `	/* php ZPP validates all four args, in order, before the body runs: the` |
|      - |  937 | `	 * non-array forms resolve here (null deprecation, __toString objects,` |
|      - |  938 | `	 * numeric strings), arrays pass through to the per-mode handling. */` |
|     59 |  939 | `	PH7_MemObjInit(pCtx->pVm,&sStrTmp);` |
|     59 |  940 | `	PH7_MemObjInit(pCtx->pVm,&sReplTmp);` |
|     59 |  941 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     45 |  942 | `		rc = StrPredicateResolveArg(pCtx,apArg[0],"substr_replace",1,"$string","array\|string",` |
|      - |  943 | `			"substr_replace(): Passing null to parameter #1 ($string) "` |
|      - |  944 | `			"of type array\|string is deprecated",` |
|      - |  945 | `			&sStrTmp,&zStr,&nLen);` |
|     45 |  946 | `		if( rc != PH7_OK ) goto out;` |
|     22 |  947 | `	}` |
|     59 |  948 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|     51 |  949 | `		rc = StrPredicateResolveArg(pCtx,apArg[1],"substr_replace",2,"$replace","array\|string",` |
|      - |  950 | `			"substr_replace(): Passing null to parameter #2 ($replace) "` |
|      - |  951 | `			"of type array\|string is deprecated",` |
|      - |  952 | `			&sReplTmp,&zRepl,&nRepl);` |
|     51 |  953 | `		if( rc != PH7_OK ) goto out;` |
|     25 |  954 | `	}` |
|     59 |  955 | `	if( !ph7_value_is_array(apArg[2]) ){` |
|     51 |  956 | `		rc = PH7_IntArgResolve(pCtx,apArg[2],"substr_replace",3,"$offset","array\|int",&f);` |
|     51 |  957 | `		if( rc != PH7_OK ) goto out;` |
|     24 |  958 | `	}` |
|     57 |  959 | `	if( bLenGiven && !ph7_value_is_array(apArg[3]) ){` |
|     31 |  960 | `		rc = PH7_IntArgResolve(pCtx,apArg[3],"substr_replace",4,"$length","array\|int\|null",&l);` |
|     31 |  961 | `		if( rc != PH7_OK ) goto out;` |
|     14 |  962 | `	}` |
|     55 |  963 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - |  964 | `		/* Array form: process each element, preserving keys */` |
|      - |  965 | `		substr_replace_ctx sRep;` |
|      - |  966 | `		substr_replace_collect sCol;` |
|      - |  967 | `		SyBlob sReplPool;` |
|      - |  968 | `		SySet sRepl,sFrom,sLen;` |
|      - |  969 | `		ph7_value *pResult,*pScratch;` |
|     15 |  970 | `		sxi32 rcWalk = SXRET_OK;` |
|     15 |  971 | `		SyBlobInit(&sReplPool,&pCtx->pVm->sAllocator);` |
|     15 |  972 | `		SySetInit(&sRepl,&pCtx->pVm->sAllocator,sizeof(substr_repl_item));` |
|     15 |  973 | `		SySetInit(&sFrom,&pCtx->pVm->sAllocator,sizeof(sxi64));` |
|     15 |  974 | `		SySetInit(&sLen,&pCtx->pVm->sAllocator,sizeof(sxi64));` |
|     15 |  975 | `		SyZero(&sRep,sizeof(substr_replace_ctx));` |
|     15 |  976 | `		sRep.bLenGiven = bLenGiven;` |
|     15 |  977 | `		sCol.rc = SXRET_OK;` |
|      - |  978 | `		/* Collect array-form $replace/$offset/$length positionally; the` |
|      - |  979 | `		 * scalar forms were already resolved above. */` |
|     15 |  980 | `		if( ph7_value_is_array(apArg[1]) ){` |
|      5 |  981 | `			sCol.pPool = &sReplPool;` |
|      5 |  982 | `			sCol.pSet = &sRepl;` |
|      5 |  983 | `			ph7_array_walk(apArg[1],SubstrReplaceStrWalker,&sCol);` |
|      5 |  984 | `			sRep.pRepl = &sRepl;` |
|      5 |  985 | `			sRep.pReplPool = &sReplPool;` |
|      3 |  986 | `		}else{` |
|     11 |  987 | `			sRep.zRepl = zRepl;` |
|     11 |  988 | `			sRep.nRepl = nRepl;` |
|      - |  989 | `		}` |
|     15 |  990 | `		if( sCol.rc == SXRET_OK && ph7_value_is_array(apArg[2]) ){` |
|      7 |  991 | `			sCol.pSet = &sFrom;` |
|      7 |  992 | `			ph7_array_walk(apArg[2],SubstrReplaceIntWalker,&sCol);` |
|      7 |  993 | `			sRep.pFrom = &sFrom;` |
|      4 |  994 | `		}else{` |
|      9 |  995 | `			sRep.iFrom = f;` |
|      - |  996 | `		}` |
|     15 |  997 | `		if( sCol.rc == SXRET_OK && bLenGiven ){` |
|      9 |  998 | `			if( ph7_value_is_array(apArg[3]) ){` |
|      5 |  999 | `				sCol.pSet = &sLen;` |
|      5 | 1000 | `				ph7_array_walk(apArg[3],SubstrReplaceIntWalker,&sCol);` |
|      5 | 1001 | `				sRep.pLen = &sLen;` |
|      3 | 1002 | `			}else{` |
|      5 | 1003 | `				sRep.iLen = l;` |
|      - | 1004 | `			}` |
|      4 | 1005 | `		}` |
|     15 | 1006 | `		pResult = ph7_context_new_array(pCtx);` |
|     15 | 1007 | `		pScratch = ph7_context_new_scalar(pCtx);` |
|     15 | 1008 | `		if( sCol.rc != SXRET_OK \|\| pResult == 0 \|\| pScratch == 0 ){` |
|    ! 0 | 1009 | `			rcWalk = SXERR_MEM;` |
|    ! 0 | 1010 | `		}else{` |
|     15 | 1011 | `			sRep.pResult = pResult;` |
|     15 | 1012 | `			sRep.pScratch = pScratch;` |
|     15 | 1013 | `			ph7_value_string(pScratch,"",0); /* Force string representation */` |
|     15 | 1014 | `			ph7_array_walk(apArg[0],SubstrReplaceElemWalker,&sRep);` |
|     15 | 1015 | `			rcWalk = sRep.rc;` |
|      - | 1016 | `		}` |
|     15 | 1017 | `		SyBlobRelease(&sReplPool);` |
|     15 | 1018 | `		SySetRelease(&sRepl);` |
|     15 | 1019 | `		SySetRelease(&sFrom);` |
|     15 | 1020 | `		SySetRelease(&sLen);` |
|     15 | 1021 | `		if( rcWalk != SXRET_OK ){` |
|    ! 0 | 1022 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1023 | `			goto out;` |
|      - | 1024 | `		}` |
|     15 | 1025 | `		ph7_result_value(pCtx,pResult);` |
|     15 | 1026 | `		rc = PH7_OK;` |
|     15 | 1027 | `		goto out;` |
|      - | 1028 | `	}` |
|      - | 1029 | `	/* Scalar form: array $offset/$length are a TypeError, array $replace` |
|      - | 1030 | `	 * degrades to its first element (php quirk). */` |
|     41 | 1031 | `	if( ph7_value_is_array(apArg[2]) ){` |
|      3 | 1032 | `		rc = PH7_VmThrowException(pCtx,` |
|      - | 1033 | `			"TypeError",` |
|      - | 1034 | `			"substr_replace(): Argument #3 ($offset) cannot be an array when working on a single string"` |
|      - | 1035 | `			);` |
|      3 | 1036 | `		goto out;` |
|      - | 1037 | `	}` |
|     39 | 1038 | `	if( bLenGiven && ph7_value_is_array(apArg[3]) ){` |
|      3 | 1039 | `		rc = PH7_VmThrowException(pCtx,` |
|      - | 1040 | `			"TypeError",` |
|      - | 1041 | `			"substr_replace(): Argument #4 ($length) cannot be an array when working on a single string"` |
|      - | 1042 | `			);` |
|      3 | 1043 | `		goto out;` |
|      - | 1044 | `	}` |
|     37 | 1045 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 1046 | `		/* First element of the replace array, or "" when empty */` |
|      5 | 1047 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      5 | 1048 | `		zRepl = "";` |
|      5 | 1049 | `		nRepl = 0;` |
|      5 | 1050 | `		if( pMap->pFirst ){` |
|      3 | 1051 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pMap->pFirst->nValIdx);` |
|      3 | 1052 | `			if( pVal ){` |
|      3 | 1053 | `				zRepl = ph7_value_to_string(pVal,&nRepl);` |
|      1 | 1054 | `			}` |
|      1 | 1055 | `		}` |
|      2 | 1056 | `	}` |
|     37 | 1057 | `	if( !bLenGiven ){` |
|     15 | 1058 | `		l = nLen;` |
|      7 | 1059 | `	}` |
|     37 | 1060 | `	SubstrReplaceWindow(&f,&l,nLen);` |
|      - | 1061 | `	/* Assemble prefix + replacement + suffix straight into the call result` |
|      - | 1062 | `	 * (ph7_result_string appends), no scratch buffer needed. */` |
|     37 | 1063 | `	rc = SXRET_OK;` |
|     37 | 1064 | `	if( f > 0 ){` |
|     29 | 1065 | `		rc = ph7_result_string(pCtx,zStr,(int)f);` |
|     14 | 1066 | `	}` |
|     37 | 1067 | `	if( rc == SXRET_OK && nRepl > 0 ){` |
|     33 | 1068 | `		rc = ph7_result_string(pCtx,zRepl,nRepl);` |
|     16 | 1069 | `	}` |
|     37 | 1070 | `	if( rc == SXRET_OK && nLen - (int)(f+l) > 0 ){` |
|     17 | 1071 | `		rc = ph7_result_string(pCtx,&zStr[f+l],nLen - (int)(f+l));` |
|      8 | 1072 | `	}` |
|     37 | 1073 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 1074 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1075 | `		goto out;` |
|      - | 1076 | `	}` |
|      - | 1077 | `	/* Force a string result even when all three segments are empty */` |
|     37 | 1078 | `	rc = ph7_result_string(pCtx,"",0);` |
|     37 | 1079 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 1080 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1081 | `		goto out;` |
|      - | 1082 | `	}` |
|     37 | 1083 | `	rc = PH7_OK;` |
|     29 | 1084 | `out:` |
|     59 | 1085 | `	PH7_MemObjRelease(&sStrTmp);` |
|     59 | 1086 | `	PH7_MemObjRelease(&sReplTmp);` |
|     59 | 1087 | `	return rc;` |
|     30 | 1088 | `}` |
|      - | 1089 | `/*` |
|      - | 1090 | ` * int levenshtein(string $string1,string $string2[,int $insertion_cost = 1[,int $replacement_cost = 1[,int $deletion_cost = 1]]])` |
|      - | 1091 | ` *  Calculate the Levenshtein distance between two strings, byte per byte` |
|      - | 1092 | ` *  (case-sensitive), with optional per-operation costs. Mirrors PHP's` |
|      - | 1093 | ` *  reference_levdist(): two rolling rows over string2.` |
|      - | 1094 | ` * Return` |
|      - | 1095 | ` *  The minimal number of weighted edit operations turning $string1 into` |
|      - | 1096 | ` *  $string2.` |
|      - | 1097 | ` */` |
|     34 | 1098 | `static int PH7_builtin_levenshtein(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1099 | `{` |
|      - | 1100 | `	static const char *azParam[] = { "$insertion_cost","$replacement_cost","$deletion_cost" };` |
|      - | 1101 | `	const char *zStr1,*zStr2;` |
|     35 | 1102 | `	sxi64 iCostIns = 1,iCostRep = 1,iCostDel = 1;` |
|      - | 1103 | `	sxi64 *p1,*p2,*pTmp;` |
|      - | 1104 | `	sxi64 c0,c1,c2;` |
|      - | 1105 | `	ph7_value sTmp1,sTmp2;` |
|      - | 1106 | `	int nLen1,nLen2;` |
|      - | 1107 | `	int i1,i2;` |
|      - | 1108 | `	sxi32 rc;` |
|      - | 1109 | `	int i;` |
|     35 | 1110 | `	if( nArg < 2 ){` |
|    ! 0 | 1111 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1112 | `			"ArgumentCountError",` |
|      - | 1113 | `			"levenshtein() expects at least 2 arguments, %d given",` |
|    ! 0 | 1114 | `			nArg` |
|      - | 1115 | `			);` |
|      - | 1116 | `	}` |
|      - | 1117 | `	/* $string1/$string2: null deprecates to "", __toString objects resolve,` |
|      - | 1118 | `	 * everything non-stringish is a TypeError (php ZPP weak mode). */` |
|     35 | 1119 | `	PH7_MemObjInit(pCtx->pVm,&sTmp1);` |
|     35 | 1120 | `	PH7_MemObjInit(pCtx->pVm,&sTmp2);` |
|     35 | 1121 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"levenshtein",1,"$string1","string",` |
|      - | 1122 | `		"levenshtein(): Passing null to parameter #1 ($string1) "` |
|      - | 1123 | `		"of type string is deprecated",` |
|      - | 1124 | `		&sTmp1,&zStr1,&nLen1);` |
|     35 | 1125 | `	if( rc != PH7_OK ) goto out;` |
|     35 | 1126 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"levenshtein",2,"$string2","string",` |
|      - | 1127 | `		"levenshtein(): Passing null to parameter #2 ($string2) "` |
|      - | 1128 | `		"of type string is deprecated",` |
|      - | 1129 | `		&sTmp2,&zStr2,&nLen2);` |
|     35 | 1130 | `	if( rc != PH7_OK ) goto out;` |
|      - | 1131 | `	/* Optional integer costs */` |
|     57 | 1132 | `	for( i = 2 ; i < nArg && i < 5 ; i++ ){` |
|      - | 1133 | `		sxi64 iVal;` |
|     31 | 1134 | `		rc = PH7_IntArgResolve(pCtx,apArg[i],"levenshtein",i+1,azParam[i-2],"int",&iVal);` |
|     31 | 1135 | `		if( rc != PH7_OK ) goto out;` |
|     23 | 1136 | `		if( i == 2 ){` |
|     11 | 1137 | `			iCostIns = iVal;` |
|     18 | 1138 | `		}else if( i == 3 ){` |
|      7 | 1139 | `			iCostRep = iVal;` |
|      4 | 1140 | `		}else{` |
|      7 | 1141 | `			iCostDel = iVal;` |
|      - | 1142 | `		}` |
|     12 | 1143 | `	}` |
|     27 | 1144 | `	if( nLen1 == 0 ){` |
|      3 | 1145 | `		ph7_result_int64(pCtx,(sxi64)nLen2 * iCostIns);` |
|      3 | 1146 | `		rc = PH7_OK;` |
|      3 | 1147 | `		goto out;` |
|      - | 1148 | `	}` |
|     25 | 1149 | `	if( nLen2 == 0 ){` |
|      3 | 1150 | `		ph7_result_int64(pCtx,(sxi64)nLen1 * iCostDel);` |
|      3 | 1151 | `		rc = PH7_OK;` |
|      3 | 1152 | `		goto out;` |
|      - | 1153 | `	}` |
|      - | 1154 | `	/* Two rolling DP rows over string2 (auto-released on return). Reject a` |
|      - | 1155 | `	 * string2 long enough to overflow the 32-bit allocation size. */` |
|     23 | 1156 | `	if( (sxu32)nLen2 >= (SXU32_HIGH / sizeof(sxi64)) - 1 ){` |
|    ! 0 | 1157 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1158 | `		goto out;` |
|      - | 1159 | `	}` |
|     23 | 1160 | `	p1 = (sxi64 *)ph7_context_alloc_chunk(pCtx,(unsigned int)(sizeof(sxi64) * (sxu32)(nLen2 + 1)),FALSE,TRUE);` |
|     23 | 1161 | `	p2 = (sxi64 *)ph7_context_alloc_chunk(pCtx,(unsigned int)(sizeof(sxi64) * (sxu32)(nLen2 + 1)),FALSE,TRUE);` |
|     23 | 1162 | `	if( p1 == 0 \|\| p2 == 0 ){` |
|    ! 0 | 1163 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1164 | `		goto out;` |
|      - | 1165 | `	}` |
|    733 | 1166 | `	for( i2 = 0 ; i2 <= nLen2 ; i2++ ){` |
|    711 | 1167 | `		p1[i2] = (sxi64)i2 * iCostIns;` |
|    356 | 1168 | `	}` |
|    707 | 1169 | `	for( i1 = 0 ; i1 < nLen1 ; i1++ ){` |
|    685 | 1170 | `		p2[0] = p1[0] + iCostDel;` |
| 181111 | 1171 | `		for( i2 = 0 ; i2 < nLen2 ; i2++ ){` |
| 180427 | 1172 | `			c0 = p1[i2] + ((zStr1[i1] == zStr2[i2]) ? 0 : iCostRep);` |
| 180427 | 1173 | `			c1 = p1[i2 + 1] + iCostDel;` |
| 180427 | 1174 | `			if( c1 < c0 ){` |
|  45393 | 1175 | `				c0 = c1;` |
|  22696 | 1176 | `			}` |
| 180427 | 1177 | `			c2 = p2[i2] + iCostIns;` |
| 180427 | 1178 | `			if( c2 < c0 ){` |
|  44809 | 1179 | `				c0 = c2;` |
|  22404 | 1180 | `			}` |
| 180427 | 1181 | `			p2[i2 + 1] = c0;` |
|  90214 | 1182 | `		}` |
|    685 | 1183 | `		pTmp = p1;` |
|    685 | 1184 | `		p1 = p2;` |
|    685 | 1185 | `		p2 = pTmp;` |
|    343 | 1186 | `	}` |
|     23 | 1187 | `	ph7_result_int64(pCtx,p1[nLen2]);` |
|     23 | 1188 | `	rc = PH7_OK;` |
|     17 | 1189 | `out:` |
|     35 | 1190 | `	PH7_MemObjRelease(&sTmp1);` |
|     35 | 1191 | `	PH7_MemObjRelease(&sTmp2);` |
|     35 | 1192 | `	return rc;` |
|     18 | 1193 | `}` |
|      - | 1194 | `/*` |
|      - | 1195 | ` * Longest common substring scan behind similar_text() — a faithful port of` |
|      - | 1196 | ` * PHP's php_similar_str(): O(n*m) scan recording the first longest run.` |
|      - | 1197 | ` */` |
|     26 | 1198 | `static void SimilarStr(const char *zTxt1,int nLen1,const char *zTxt2,int nLen2,` |
|      - | 1199 | `	int *pPos1,int *pPos2,int *pMax,int *pCount)` |
|      1 | 1200 | `{` |
|      - | 1201 | `	const char *p,*q;` |
|     27 | 1202 | `	const char *zEnd1 = &zTxt1[nLen1];` |
|     27 | 1203 | `	const char *zEnd2 = &zTxt2[nLen2];` |
|      - | 1204 | `	int l;` |
|     27 | 1205 | `	*pMax = 0;` |
|     27 | 1206 | `	*pCount = 0;` |
|    143 | 1207 | `	for( p = zTxt1 ; p < zEnd1 ; p++ ){` |
|    843 | 1208 | `		for( q = zTxt2 ; q < zEnd2 ; q++ ){` |
|    999 | 1209 | `			for( l = 0 ; (p+l < zEnd1) && (q+l < zEnd2) && (p[l] == q[l]) ; l++ );` |
|    727 | 1210 | `			if( l > *pMax ){` |
|     25 | 1211 | `				*pMax = l;` |
|     25 | 1212 | `				*pCount += 1;` |
|     25 | 1213 | `				*pPos1 = (int)(p - zTxt1);` |
|     25 | 1214 | `				*pPos2 = (int)(q - zTxt2);` |
|     12 | 1215 | `			}` |
|    364 | 1216 | `		}` |
|     59 | 1217 | `	}` |
|     27 | 1218 | `}` |
|      - | 1219 | `/*` |
|      - | 1220 | ` * Recursive divide-and-conquer behind similar_text() — a faithful port of` |
|      - | 1221 | `` * PHP's php_similar_char(), including its quirky `count > 1` guard on the`` |
|      - | 1222 | ` * left-side recursion.` |
|      - | 1223 | ` */` |
|     26 | 1224 | `static int SimilarChar(const char *zTxt1,int nLen1,const char *zTxt2,int nLen2)` |
|      1 | 1225 | `{` |
|      - | 1226 | `	int nSum;` |
|     27 | 1227 | `	int nPos1 = 0,nPos2 = 0,nMax,nCount;` |
|     27 | 1228 | `	SimilarStr(zTxt1,nLen1,zTxt2,nLen2,&nPos1,&nPos2,&nMax,&nCount);` |
|     27 | 1229 | `	if( (nSum = nMax) != 0 ){` |
|     25 | 1230 | `		if( nPos1 && nPos2 && nCount > 1 ){` |
|    ! 0 | 1231 | `			nSum += SimilarChar(zTxt1,nPos1,zTxt2,nPos2);` |
|    ! 0 | 1232 | `		}` |
|     25 | 1233 | `		if( (nPos1 + nMax < nLen1) && (nPos2 + nMax < nLen2) ){` |
|     13 | 1234 | `			nSum += SimilarChar(&zTxt1[nPos1 + nMax],nLen1 - nPos1 - nMax,` |
|      8 | 1235 | `				&zTxt2[nPos2 + nMax],nLen2 - nPos2 - nMax);` |
|      4 | 1236 | `		}` |
|     12 | 1237 | `	}` |
|     27 | 1238 | `	return nSum;` |
|      1 | 1239 | `}` |
|      - | 1240 | `/*` |
|      - | 1241 | ` * int similar_text(string $string1,string $string2[,float &$percent])` |
|      - | 1242 | ` *  Calculate the similarity between two strings, as the number of matching` |
|      - | 1243 | ` *  characters found by PHP's greedy longest-common-substring recursion.` |
|      - | 1244 | ` *  When $percent is given it receives the similarity in percent:` |
|      - | 1245 | ` *  matching * 200 / (len1 + len2).` |
|      - | 1246 | ` * Return` |
|      - | 1247 | ` *  The number of matching characters in both strings.` |
|      - | 1248 | ` */` |
|     22 | 1249 | `static int PH7_builtin_similar_text(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1250 | `{` |
|      - | 1251 | `	const char *zStr1,*zStr2;` |
|      - | 1252 | `	ph7_value sTmp1,sTmp2;` |
|      - | 1253 | `	int nLen1,nLen2;` |
|      - | 1254 | `	int nSim;` |
|      - | 1255 | `	sxi32 rc;` |
|     23 | 1256 | `	if( nArg < 2 ){` |
|    ! 0 | 1257 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1258 | `			"ArgumentCountError",` |
|      - | 1259 | `			"similar_text() expects at least 2 arguments, %d given",` |
|    ! 0 | 1260 | `			nArg` |
|      - | 1261 | `			);` |
|      - | 1262 | `	}` |
|     23 | 1263 | `	PH7_MemObjInit(pCtx->pVm,&sTmp1);` |
|     23 | 1264 | `	PH7_MemObjInit(pCtx->pVm,&sTmp2);` |
|     23 | 1265 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"similar_text",1,"$string1","string",` |
|      - | 1266 | `		"similar_text(): Passing null to parameter #1 ($string1) "` |
|      - | 1267 | `		"of type string is deprecated",` |
|      - | 1268 | `		&sTmp1,&zStr1,&nLen1);` |
|     23 | 1269 | `	if( rc != PH7_OK ) goto out;` |
|     23 | 1270 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"similar_text",2,"$string2","string",` |
|      - | 1271 | `		"similar_text(): Passing null to parameter #2 ($string2) "` |
|      - | 1272 | `		"of type string is deprecated",` |
|      - | 1273 | `		&sTmp2,&zStr2,&nLen2);` |
|     23 | 1274 | `	if( rc != PH7_OK ) goto out;` |
|     23 | 1275 | `	if( nLen1 + nLen2 == 0 ){` |
|      5 | 1276 | `		nSim = 0;` |
|      3 | 1277 | `	}else{` |
|     19 | 1278 | `		nSim = SimilarChar(zStr1,nLen1,zStr2,nLen2);` |
|      - | 1279 | `	}` |
|     23 | 1280 | `	if( nArg > 2 ){` |
|      - | 1281 | `		/* Write the percentage through the by-ref out-param */` |
|      7 | 1282 | `		ph7_value *pPercent = ph7_context_new_scalar(pCtx);` |
|      7 | 1283 | `		if( pPercent == 0 ){` |
|    ! 0 | 1284 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1285 | `			goto out;` |
|    ! 0 | 1286 | `		}else{` |
|      7 | 1287 | `			double dPct = (nLen1 + nLen2 == 0) ? 0.0 : (double)nSim * 200.0 / (double)(nLen1 + nLen2);` |
|      7 | 1288 | `			ph7_value_double(pPercent,dPct);` |
|      7 | 1289 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],pPercent);` |
|      - | 1290 | `		}` |
|      3 | 1291 | `	}` |
|     23 | 1292 | `	ph7_result_int(pCtx,nSim);` |
|     23 | 1293 | `	rc = PH7_OK;` |
|     11 | 1294 | `out:` |
|     23 | 1295 | `	PH7_MemObjRelease(&sTmp1);` |
|     23 | 1296 | `	PH7_MemObjRelease(&sTmp2);` |
|     23 | 1297 | `	return rc;` |
|     12 | 1298 | `}` |
|      - | 1299 | `/*` |
|      - | 1300 | ` * array\|int str_word_count(string $string[,int $format = 0[,?string $characters = null]])` |
|      - | 1301 | ` *  Count (or return) the words inside a string. A word is a run of alphabetic` |
|      - | 1302 | ` *  characters, which may contain (but not start the string with) "'" and "-";` |
|      - | 1303 | ` *  $characters adds extra bytes to the word set ("a..z" ranges supported, as` |
|      - | 1304 | ` *  in PHP's php_charmask).` |
|      - | 1305 | ` *  $format: 0 -> word count, 1 -> array of words, 2 -> array of words keyed` |
|      - | 1306 | ` *  by their byte position in $string.` |
|      - | 1307 | ` * Errors` |
|      - | 1308 | ` *  ValueError when $format is not 0, 1 or 2.` |
|      - | 1309 | ` */` |
|     44 | 1310 | `static int PH7_builtin_str_word_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1311 | `{` |
|      - | 1312 | `	const char *zIn,*zEnd,*zPtr;` |
|     45 | 1313 | `	ph7_value *pArray = 0,*pValue = 0;` |
|      - | 1314 | `	ph7_value sTmp,sListTmp;` |
|      - | 1315 | `	char aMask[256];` |
|     45 | 1316 | `	int bMask = 0;` |
|     45 | 1317 | `	int iFormat = 0;` |
|     45 | 1318 | `	int nCount = 0;` |
|      - | 1319 | `	int nLen;` |
|      - | 1320 | `	sxi32 rc;` |
|     45 | 1321 | `	if( nArg < 1 ){` |
|    ! 0 | 1322 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1323 | `			"ArgumentCountError",` |
|      - | 1324 | `			"str_word_count() expects at least 1 argument, %d given",` |
|    ! 0 | 1325 | `			nArg` |
|      - | 1326 | `			);` |
|      - | 1327 | `	}` |
|     45 | 1328 | `	PH7_MemObjInit(pCtx->pVm,&sTmp);` |
|     45 | 1329 | `	PH7_MemObjInit(pCtx->pVm,&sListTmp);` |
|     45 | 1330 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_word_count",1,"$string","string",` |
|      - | 1331 | `		"str_word_count(): Passing null to parameter #1 ($string) "` |
|      - | 1332 | `		"of type string is deprecated",` |
|      - | 1333 | `		&sTmp,&zIn,&nLen);` |
|     45 | 1334 | `	if( rc != PH7_OK ) goto out;` |
|     45 | 1335 | `	if( nArg > 1 ){` |
|      - | 1336 | `		sxi64 iVal;` |
|     31 | 1337 | `		rc = PH7_IntArgResolve(pCtx,apArg[1],"str_word_count",2,"$format","int",&iVal);` |
|     33 | 1338 | `		if( rc != PH7_OK ) goto out;` |
|     29 | 1339 | `		if( iVal < 0 \|\| iVal > 2 ){` |
|      5 | 1340 | `			rc = PH7_VmThrowException(pCtx,` |
|      - | 1341 | `				"ValueError",` |
|      - | 1342 | `				"str_word_count(): Argument #2 ($format) must be a valid format value"` |
|      - | 1343 | `				);` |
|      5 | 1344 | `			goto out;` |
|      - | 1345 | `		}` |
|     25 | 1346 | `		iFormat = (int)iVal;` |
|     12 | 1347 | `	}` |
|     39 | 1348 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      - | 1349 | `		/* $characters is ?string: null (skipped above) simply keeps the` |
|      - | 1350 | `		 * default word set, no deprecation. */` |
|      - | 1351 | `		const char *zList;` |
|      - | 1352 | `		int nList;` |
|     13 | 1353 | `		rc = StrPredicateResolveArg(pCtx,apArg[2],"str_word_count",3,"$characters","?string",` |
|      - | 1354 | `			"" /* unreachable: null never gets here */,` |
|      - | 1355 | `			&sListTmp,&zList,&nList);` |
|     13 | 1356 | `		if( rc != PH7_OK ) goto out;` |
|     13 | 1357 | `		PH7_BuildCharMask(pCtx,zList,nList,aMask);` |
|     13 | 1358 | `		bMask = 1;` |
|      6 | 1359 | `	}` |
|     39 | 1360 | `	if( iFormat != 0 ){` |
|     25 | 1361 | `		pArray = ph7_context_new_array(pCtx);` |
|     25 | 1362 | `		pValue = ph7_context_new_scalar(pCtx);` |
|     25 | 1363 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1364 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1365 | `			goto out;` |
|      - | 1366 | `		}` |
|     12 | 1367 | `	}` |
|     39 | 1368 | `	zPtr = zIn;` |
|     39 | 1369 | `	zEnd = &zIn[nLen];` |
|     39 | 1370 | `	if( nLen > 0 ){` |
|      - | 1371 | `		/* php: the string's first byte cannot be ' or -, and its last byte` |
|      - | 1372 | `		 * cannot be -, unless the charlist explicitly allows them. */` |
|     33 | 1373 | `		if( (zPtr[0] == '\'' && (!bMask \|\| !aMask[(unsigned char)'\''])) \|\|` |
|     28 | 1374 | `			(zPtr[0] == '-'  && (!bMask \|\| !aMask[(unsigned char)'-'])) ){` |
|      9 | 1375 | `			zPtr++;` |
|      4 | 1376 | `		}` |
|     33 | 1377 | `		if( zEnd[-1] == '-' && (!bMask \|\| !aMask[(unsigned char)'-']) ){` |
|      9 | 1378 | `			zEnd--;` |
|      4 | 1379 | `		}` |
|     16 | 1380 | `	}` |
|    135 | 1381 | `	while( zPtr < zEnd ){` |
|     91 | 1382 | `		const char *zStart = zPtr;` |
|    477 | 1383 | `		while( zPtr < zEnd && ( SyisAlpha((unsigned char)zPtr[0])` |
|    253 | 1384 | `			\|\| (bMask && aMask[(unsigned char)zPtr[0]])` |
|     98 | 1385 | `			\|\| zPtr[0] == '\'' \|\| zPtr[0] == '-' ) ){` |
|    339 | 1386 | `			zPtr++;` |
|      1 | 1387 | `		}` |
|     97 | 1388 | `		if( zPtr > zStart ){` |
|     91 | 1389 | `			if( iFormat == 0 ){` |
|     19 | 1390 | `				nCount++;` |
|     10 | 1391 | `			}else{` |
|     73 | 1392 | `				ph7_value_reset_string_cursor(pValue);` |
|     73 | 1393 | `				if( SXRET_OK != ph7_value_string(pValue,zStart,(int)(zPtr-zStart)) ){` |
|    ! 0 | 1394 | `					rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1395 | `					goto out;` |
|      - | 1396 | `				}` |
|     73 | 1397 | `				if( iFormat == 1 ){` |
|     59 | 1398 | `					if( SXRET_OK != ph7_array_add_elem(pArray,0,pValue) ){` |
|    ! 0 | 1399 | `						rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1400 | `						goto out;` |
|      - | 1401 | `					}` |
|     30 | 1402 | `				}else{` |
|     15 | 1403 | `					if( SXRET_OK != ph7_array_add_intkey_elem(pArray,(int)(zStart-zIn),pValue) ){` |
|    ! 0 | 1404 | `						rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1405 | `						goto out;` |
|      - | 1406 | `					}` |
|      - | 1407 | `				}` |
|      - | 1408 | `			}` |
|     45 | 1409 | `		}` |
|     97 | 1410 | `		zPtr++;` |
|      1 | 1411 | `	}` |
|     37 | 1412 | `	if( iFormat == 0 ){` |
|     13 | 1413 | `		ph7_result_int(pCtx,nCount);` |
|      7 | 1414 | `	}else{` |
|     25 | 1415 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1416 | `	}` |
|     37 | 1417 | `	rc = PH7_OK;` |
|     21 | 1418 | `out:` |
|     43 | 1419 | `	PH7_MemObjRelease(&sTmp);` |
|     43 | 1420 | `	PH7_MemObjRelease(&sListTmp);` |
|     43 | 1421 | `	return rc;` |
|     22 | 1422 | `}` |
|      - | 1423 | `/*` |
|      - | 1424 | ` * string chunk_split(string $body[,int $chunklen = 76 [, string $end = "\r\n" ]])` |
|      - | 1425 | ` *   Split a string into smaller chunks.` |
|      - | 1426 | ` * Parameters` |
|      - | 1427 | ` *  $body` |
|      - | 1428 | ` *   The string to be chunked.` |
|      - | 1429 | ` * $chunklen` |
|      - | 1430 | ` *   The chunk length.` |
|      - | 1431 | ` * $end` |
|      - | 1432 | ` *   The line ending sequence.` |
|      - | 1433 | ` * Return` |
|      - | 1434 | ` *  The chunked string or NULL on failure.` |
|      - | 1435 | ` */` |
|     14 | 1436 | `static int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1437 | `{` |
|     15 | 1438 | `	const char *zIn,*zEnd,*zSep = "\r\n";` |
|      - | 1439 | `	int nSepLen,nChunkLen,nLen;` |
|      - | 1440 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1441 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     15 | 1442 | `	if( nArg < 1 ){` |
|      - | 1443 | `		/* Nothing to split,return null */` |
|    ! 0 | 1444 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1445 | `		return PH7_OK;` |
|      - | 1446 | `	}` |
|      - | 1447 | `	/* initialize/Extract arguments */` |
|     15 | 1448 | `	nSepLen = (int)sizeof("\r\n") - 1;` |
|     15 | 1449 | `	nChunkLen = 76;` |
|     15 | 1450 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 1451 | `	zEnd = &zIn[nLen];` |
|     15 | 1452 | `	if( nArg > 1 ){` |
|      - | 1453 | `		/* Chunk length */` |
|     13 | 1454 | `		nChunkLen = ph7_value_to_int(apArg[1]);` |
|     13 | 1455 | `		if( nChunkLen < 1 ){` |
|      - | 1456 | `			/* PHP 8 throws a catchable ValueError for a non-positive length. */` |
|      3 | 1457 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1458 | `				"chunk_split(): Argument #2 ($length) must be greater than 0");` |
|      - | 1459 | `		}` |
|     11 | 1460 | `		if( nArg > 2 ){` |
|      - | 1461 | `			/* Separator */` |
|      9 | 1462 | `			zSep = ph7_value_to_string(apArg[2],&nSepLen);` |
|      9 | 1463 | `			if( nSepLen < 1 ){` |
|      - | 1464 | `				/* Switch back to the default separator */` |
|      3 | 1465 | `				zSep = "\r\n";` |
|      3 | 1466 | `				nSepLen = (int)sizeof("\r\n") - 1;` |
|      1 | 1467 | `			}` |
|      4 | 1468 | `		}` |
|      5 | 1469 | `	}` |
|      - | 1470 | `	/* Perform the requested operation */` |
|     13 | 1471 | `	if( nChunkLen > nLen ){` |
|      - | 1472 | `		/* Nothing to split,return the string and the separator */` |
|      9 | 1473 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);` |
|      9 | 1474 | `		return PH7_OK;` |
|      - | 1475 | `	}` |
|     17 | 1476 | `	while( zIn < zEnd ){` |
|     13 | 1477 | `		if( nChunkLen > (int)(zEnd-zIn) ){` |
|      3 | 1478 | `			nChunkLen = (int)(zEnd - zIn);` |
|      1 | 1479 | `		}` |
|      - | 1480 | `		/* Append the chunk and the separator */` |
|     13 | 1481 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);` |
|      - | 1482 | `		/* Point beyond the chunk */` |
|     13 | 1483 | `		zIn += nChunkLen;` |
|      1 | 1484 | `	}` |
|      5 | 1485 | `	return PH7_OK;` |
|      8 | 1486 | `}` |
|      - | 1487 | `/*` |
|      - | 1488 | ` * string addslashes(string $str)` |
|      - | 1489 | ` *  Quote string with slashes.` |
|      - | 1490 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1491 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1492 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1493 | ` * Parameter` |
|      - | 1494 | ` *  str: The string to be escaped.` |
|      - | 1495 | ` * Return` |
|      - | 1496 | ` *  Returns the escaped string` |
|      - | 1497 | ` */` |
|     20 | 1498 | `static int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1499 | `{` |
|      - | 1500 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1501 | `	int nLen;` |
|      - | 1502 | `	/* PHP enforces exactly one argument. */` |
|     22 | 1503 | `	if( nArg != 1 ){` |
|      4 | 1504 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1505 | `			"ArgumentCountError",` |
|      - | 1506 | `			"addslashes() expects exactly 1 argument, %d given",` |
|      1 | 1507 | `			nArg` |
|      - | 1508 | `			);` |
|      - | 1509 | `	}` |
|      - | 1510 | `	/* NULL is deprecated and treated as an empty string; other invalid` |
|      - | 1511 | `	 * types still produce a TypeError. */` |
|     19 | 1512 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 1513 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 1514 | `			E_DEPRECATED,` |
|      - | 1515 | `			"addslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - | 1516 | `			);` |
|      - | 1517 | `		/* fall through so conversion below yields empty string */` |
|      1 | 1518 | `	}` |
|      - | 1519 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     27 | 1520 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     28 | 1521 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     18 | 1522 | `	    ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 1523 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1524 | `			"TypeError",` |
|      - | 1525 | `			"addslashes(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 1526 | `			ph7_type_name(apArg[0])` |
|      - | 1527 | `			);` |
|      - | 1528 | `	}` |
|      - | 1529 | `	/* Convert to string representation first and obtain length. */` |
|     19 | 1530 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 1531 | `	if( nLen < 1 ){` |
|      - | 1532 | `		/* Return the empty string */` |
|      5 | 1533 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1534 | `		return PH7_OK;` |
|      - | 1535 | `	}` |
|     15 | 1536 | `	zEnd = &zIn[nLen];` |
|     15 | 1537 | `	zCur = 0; /* cc warning */` |
|     20 | 1538 | `	for(;;){` |
|     41 | 1539 | `		if( zIn >= zEnd ){` |
|      - | 1540 | `			/* No more input */` |
|     15 | 1541 | `			break;` |
|      - | 1542 | `		}` |
|     27 | 1543 | `		zCur = zIn;` |
|      - | 1544 | `		/* scan until a character that needs escaping (', ", \\, or NUL) */` |
|     89 | 1545 | `		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' && zIn[0] != '\0' ){` |
|     63 | 1546 | `			zIn++;` |
|      1 | 1547 | `		}` |
|     27 | 1548 | `		if( zIn > zCur ){` |
|      - | 1549 | `			/* Append raw contents */` |
|     23 | 1550 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 1551 | `		}` |
|     27 | 1552 | `		if( zIn < zEnd ){` |
|     17 | 1553 | `			int c = zIn[0];` |
|     17 | 1554 | `			if( c == '\0' ){` |
|      - | 1555 | `				/* PHP escapes NUL as "\\0" (two characters) */` |
|      3 | 1556 | `				ph7_result_string(pCtx,"\\0",2);` |
|      2 | 1557 | `			}else{` |
|     15 | 1558 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1559 | `			}` |
|      8 | 1560 | `		}` |
|     27 | 1561 | `		zIn++;` |
|      1 | 1562 | `	}` |
|     15 | 1563 | `	return PH7_OK;` |
|     12 | 1564 | `}` |
|      - | 1565 | `/*` |
|      - | 1566 | ``  * Build a 256-entry membership mask from a PHP charlist, expanding `a..z` `` |
|      - | 1567 | ` * byte ranges exactly like PHP's php_charmask(). On return aMask[c] != 0 iff` |
|      - | 1568 | ` * the byte c belongs to the set. Emits the PHP-exact warnings for the three` |
|      - | 1569 | ` * malformed-range shapes (ph7_context_throw_error_format prepends the active` |
|      - | 1570 | ` * function name, so the messages omit it); on a bad range the surrounding` |
|      - | 1571 | ` * bytes are still added and the scan never aborts. Reads only within` |
|      - | 1572 | ` * [zList, zList+nLen).` |
|      - | 1573 | ` *` |
|      - | 1574 | ` * Use ONLY for the builtins whose charlist expands ranges the way PHP's` |
|      - | 1575 | ` * php_charmask() does: trim/ltrim/rtrim/addcslashes (and quotemeta, whose set` |
|      - | 1576 | ` * is a fixed literal with no ".."). Do NOT route strspn/strcspn/strtok/strpbrk` |
|      - | 1577 | ` * through this — PHP treats their charlists literally, so expanding "a..z" here` |
|      - | 1578 | ` * would be a behavior regression plus spurious "Invalid '..'-range" warnings.` |
|      - | 1579 | ` */` |
|    232 | 1580 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256])` |
|      5 | 1581 | `{` |
|    237 | 1582 | `	const unsigned char *zIn  = (const unsigned char *)zList;` |
|    237 | 1583 | `	const unsigned char *zEnd = zIn + (nLen > 0 ? nLen : 0);` |
|    237 | 1584 | `	SyZero(aMask,256);` |
|    631 | 1585 | `	for( ; zIn < zEnd ; zIn++ ){` |
|    399 | 1586 | `		int c = zIn[0];` |
|    399 | 1587 | `		if( zIn + 3 < zEnd && zIn[1] == '.' && zIn[2] == '.' && zIn[3] >= c ){` |
|      - | 1588 | `			/* Valid incrementing range c..zIn[3] */` |
|     22 | 1589 | `			int hi = zIn[3],k;` |
|    386 | 1590 | `			for( k = c ; k <= hi ; k++ ){` |
|    366 | 1591 | `				aMask[k] = 1;` |
|    184 | 1592 | `			}` |
|     22 | 1593 | `			zIn += 3; /* the loop's ++ then steps past the range end */` |
|    398 | 1594 | `		}else if( zIn + 1 < zEnd && zIn[0] == '.' && zIn[1] == '.' ){` |
|      - | 1595 | `			/* Malformed range: mirror php_charmask's three diagnostics. */` |
|      - | 1596 | `			const char *zMsg;` |
|     20 | 1597 | `			if( (const unsigned char *)zList >= zIn ){` |
|      6 | 1598 | `				zMsg = "no character to the left of '..'";` |
|     18 | 1599 | `			}else if( zIn + 2 >= zEnd ){` |
|      6 | 1600 | `				zMsg = "no character to the right of '..'";` |
|     14 | 1601 | `			}else if( zIn[-1] > zIn[2] ){` |
|     12 | 1602 | `				zMsg = "'..'-range needs to be incrementing";` |
|      7 | 1603 | `			}else{` |
|    ! 0 | 1604 | `				zMsg = 0; /* catch-all (e.g. a..b..c) */` |
|      - | 1605 | `			}` |
|     20 | 1606 | `			if( zMsg ){` |
|     29 | 1607 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      9 | 1608 | `					"Invalid '..'-range, %s",zMsg);` |
|     11 | 1609 | `			}else{` |
|    ! 0 | 1610 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1611 | `					"Invalid '..'-range");` |
|      - | 1612 | `			}` |
|      - | 1613 | `			/* Do not consume the dots: the loop's ++ steps one byte so the` |
|      - | 1614 | `			 * dots are re-scanned as literals, exactly like php_charmask. */` |
|     11 | 1615 | `		}else{` |
|    361 | 1616 | `			aMask[c] = 1;` |
|      - | 1617 | `		}` |
|    202 | 1618 | `	}` |
|    237 | 1619 | `}` |
|      - | 1620 | `/*` |
|      - | 1621 | ` * string addcslashes(string $str,string $charlist)` |
|      - | 1622 | ` *  Quote string with slashes in a C style.` |
|      - | 1623 | ` * Parameter` |
|      - | 1624 | ` *  $str:` |
|      - | 1625 | ` *    The string to be escaped.` |
|      - | 1626 | ` *  $charlist:` |
|      - | 1627 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|      - | 1628 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|      - | 1629 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|      - | 1630 | ` * Return` |
|      - | 1631 | ` *  Returns the escaped string.` |
|      - | 1632 | ` * Note:` |
|      - | 1633 | ` *  Character ranges [i.e: 'A..Z'] are supported (see PH7_BuildCharMask).` |
|      - | 1634 | ` */` |
|     34 | 1635 | `static int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1636 | `{` |
|      - | 1637 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - | 1638 | `	char aMask[256];` |
|      - | 1639 | `	int nLen,nMask;` |
|      - | 1640 | `	/* PHP enforces exactly two arguments. */` |
|     37 | 1641 | `	if( nArg != 2 ){` |
|      4 | 1642 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1643 | `			"ArgumentCountError",` |
|      - | 1644 | `			"addcslashes() expects exactly 2 arguments, %d given",` |
|      1 | 1645 | `			nArg` |
|      - | 1646 | `			);` |
|      - | 1647 | `	}` |
|      - | 1648 | `	/* First argument must be a string-ish value.  NULL is deprecated and` |
|      - | 1649 | `	 * treated as the empty string (PHP 8.1). */` |
|     35 | 1650 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      - | 1651 | `		/* Emit deprecation only once, similar to PHP behaviour. */` |
|      3 | 1652 | `		PH7_VmThrowError(pCtx->pVm,0,/* iErr will be patched to 8192 below */` |
|      - | 1653 | `			E_DEPRECATED,` |
|      - | 1654 | `			"addcslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - | 1655 | `			);` |
|      - | 1656 | `		/* treat as empty string; fall through to conversion logic */` |
|     47 | 1657 | `	} else if( ph7_value_is_array(apArg[0]) \|\|` |
|     48 | 1658 | `	          ph7_value_is_object(apArg[0]) \|\|` |
|     30 | 1659 | `	          ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 1660 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1661 | `			"TypeError",` |
|      - | 1662 | `			"addcslashes(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 1663 | `			ph7_type_name(apArg[0])` |
|      - | 1664 | `			);` |
|      - | 1665 | `	}` |
|      - | 1666 | `	/* Second argument must be a string.  NULL is deprecated and treated as` |
|      - | 1667 | `	 * an empty mask per PHP semantics.  Arrays/objects/resources still` |
|      - | 1668 | `	 * trigger a TypeError. */` |
|     35 | 1669 | `	if( ph7_value_is_null(apArg[1]) ){` |
|      3 | 1670 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 1671 | `			E_DEPRECATED,` |
|      - | 1672 | `			"addcslashes(): Passing null to parameter #2 ($characters) of type string is deprecated"` |
|      - | 1673 | `			);` |
|      - | 1674 | `		/* allow through so it becomes empty string below */` |
|     47 | 1675 | `	} else if( ph7_value_is_array(apArg[1]) \|\|` |
|     48 | 1676 | `	          ph7_value_is_object(apArg[1]) \|\|` |
|     30 | 1677 | `	          ph7_value_is_resource(apArg[1]) ){` |
|    ! 0 | 1678 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1679 | `			"TypeError",` |
|      - | 1680 | `			"addcslashes(): Argument #2 ($characters) must be of type string, %s given",` |
|    ! 0 | 1681 | `			ph7_type_name(apArg[1])` |
|      - | 1682 | `			);` |
|      - | 1683 | `	}` |
|      - | 1684 | `	/* Extract the string to process */` |
|     35 | 1685 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1686 | `	/* NULL would never reach here due to the check above. */` |
|     35 | 1687 | `	if( nLen < 1 ){` |
|      - | 1688 | `		/* Empty string returns itself. */` |
|      5 | 1689 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      5 | 1690 | `		return PH7_OK;` |
|      - | 1691 | `	}` |
|      - | 1692 | ``	/* Extract the desired mask and expand any `a..z` ranges into a lookup. */`` |
|     31 | 1693 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|     31 | 1694 | `	PH7_BuildCharMask(pCtx,zMask,nMask,aMask);` |
|     31 | 1695 | `	zEnd = &zIn[nLen];` |
|     31 | 1696 | `	zCur = 0; /* cc warning */` |
|     37 | 1697 | `	for(;;){` |
|     77 | 1698 | `		if( zIn >= zEnd ){` |
|      - | 1699 | `			/* No more input */` |
|     31 | 1700 | `			break;` |
|      - | 1701 | `		}` |
|     49 | 1702 | `		zCur = zIn;` |
|    125 | 1703 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     79 | 1704 | `			zIn++;` |
|      3 | 1705 | `		}` |
|     49 | 1706 | `		if( zIn > zCur ){` |
|      - | 1707 | `			/* Append raw contents */` |
|     43 | 1708 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     20 | 1709 | `		}` |
|     49 | 1710 | `		if( zIn < zEnd ){` |
|      - | 1711 | `			/* Make sure we treat the byte as unsigned to avoid negative values` |
|      - | 1712 | `			 * on platforms where char is signed. */` |
|     29 | 1713 | `			int c = (unsigned char)zIn[0];` |
|      - | 1714 | `			/* Handle special C-like escapes for common control characters first.` |
|      - | 1715 | `			 * PHP outputs "\n" "\r" "\t" "\v" "\f" when those chars are` |
|      - | 1716 | `			 * in the mask. NUL is left to the octal conversion below. */` |
|     29 | 1717 | `			if( c == '\n' ){` |
|      3 | 1718 | `				ph7_result_string(pCtx,"\\n",2);` |
|     28 | 1719 | `			}else if( c == '\r' ){` |
|      3 | 1720 | `				ph7_result_string(pCtx,"\\r",2);` |
|     26 | 1721 | `			}else if( c == '\t' ){` |
|      3 | 1722 | `				ph7_result_string(pCtx,"\\t",2);` |
|     24 | 1723 | `			}else if( c == '\v' ){` |
|      3 | 1724 | `				ph7_result_string(pCtx,"\\v",2);` |
|     22 | 1725 | `			}else if( c == '\f' ){` |
|      3 | 1726 | `				ph7_result_string(pCtx,"\\f",2);` |
|     20 | 1727 | `			}else if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|      - | 1728 | `				/* Convert to octal.  PHP always emits three-digit zero-padded` |
|      - | 1729 | `				 * octal escapes (\001 not \1). */` |
|      7 | 1730 | `				ph7_result_string_format(pCtx,"\\%03o",c);` |
|      4 | 1731 | `			}else{` |
|     13 | 1732 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1733 | `			}` |
|     13 | 1734 | `		}` |
|     49 | 1735 | `		zIn++;` |
|      3 | 1736 | `	}` |
|     31 | 1737 | `	return PH7_OK;` |
|     20 | 1738 | `}` |
|      - | 1739 | `/*` |
|      - | 1740 | ` * string quotemeta(string $str)` |
|      - | 1741 | ` *  Quote meta characters.` |
|      - | 1742 | ` * Parameter` |
|      - | 1743 | ` *  $str:` |
|      - | 1744 | ` *    The string to be escaped.` |
|      - | 1745 | ` * Return` |
|      - | 1746 | ` *  Returns the escaped string.` |
|      - | 1747 | `*/` |
|     10 | 1748 | `static int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1749 | `{` |
|      - | 1750 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1751 | `	char aMask[256];` |
|      - | 1752 | `	int nLen;` |
|     12 | 1753 | `	if( nArg < 1 ){` |
|      - | 1754 | `		/* Nothing to process,retun NULL */` |
|    ! 0 | 1755 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1756 | `		return PH7_OK;` |
|      - | 1757 | `	}` |
|      - | 1758 | `	/* Extract the string to process */` |
|     12 | 1759 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     12 | 1760 | `	if( nLen < 1 ){` |
|      - | 1761 | `		/* Return the empty string */` |
|      3 | 1762 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1763 | `		return PH7_OK;` |
|      - | 1764 | `	}` |
|      - | 1765 | `	/* Fixed meta-character set (no ranges); build the lookup once. */` |
|     10 | 1766 | `	PH7_BuildCharMask(pCtx,".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1,aMask);` |
|     10 | 1767 | `	zEnd = &zIn[nLen];` |
|     10 | 1768 | `	zCur = 0; /* cc warning */` |
|     22 | 1769 | `	for(;;){` |
|     46 | 1770 | `		if( zIn >= zEnd ){` |
|      - | 1771 | `			/* No more input */` |
|     10 | 1772 | `			break;` |
|      - | 1773 | `		}` |
|     38 | 1774 | `		zCur = zIn;` |
|     76 | 1775 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     40 | 1776 | `			zIn++;` |
|      2 | 1777 | `		}` |
|     38 | 1778 | `		if( zIn > zCur ){` |
|      - | 1779 | `			/* Append raw contents */` |
|     20 | 1780 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      9 | 1781 | `		}` |
|     38 | 1782 | `		if( zIn < zEnd ){` |
|     36 | 1783 | `			int c = zIn[0];` |
|     36 | 1784 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|     17 | 1785 | `		}` |
|     38 | 1786 | `		zIn++;` |
|      2 | 1787 | `	}` |
|     10 | 1788 | `	return PH7_OK;` |
|      7 | 1789 | `}` |
|      - | 1790 | `/*` |
|      - | 1791 | ` * string stripslashes(string $str)` |
|      - | 1792 | ` *  Un-quotes a quoted string.` |
|      - | 1793 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1794 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1795 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1796 | ` * Parameter` |
|      - | 1797 | ` *  $str` |
|      - | 1798 | ` *   The input string.` |
|      - | 1799 | ` * Return` |
|      - | 1800 | ` *  Returns a string with backslashes stripped off.` |
|      - | 1801 | ` */` |
|      6 | 1802 | `static int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1803 | `{` |
|      - | 1804 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1805 | `	int nLen;` |
|      7 | 1806 | `	if( nArg < 1 ){` |
|      - | 1807 | `		/* Nothing to process,retun NULL */` |
|    ! 0 | 1808 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1809 | `		return PH7_OK;` |
|      - | 1810 | `	}` |
|      - | 1811 | `	/* Extract the string to process */` |
|      7 | 1812 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 1813 | `	if( zIn == 0 ){` |
|    ! 0 | 1814 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1815 | `		return PH7_OK;` |
|      - | 1816 | `	}` |
|      7 | 1817 | `	zEnd = &zIn[nLen];` |
|      7 | 1818 | `	zCur = 0; /* cc warning */` |
|      - | 1819 | `	/* Encode the string */` |
|      4 | 1820 | `	for(;;){` |
|      9 | 1821 | `		if( zIn >= zEnd ){` |
|      - | 1822 | `			/* No more input */` |
|      5 | 1823 | `			break;` |
|      - | 1824 | `		}` |
|      5 | 1825 | `		zCur = zIn;` |
|     17 | 1826 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 | 1827 | `			zIn++;` |
|      1 | 1828 | `		}` |
|      5 | 1829 | `		if( zIn > zCur ){` |
|      - | 1830 | `			/* Append raw contents */` |
|      5 | 1831 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 1832 | `		}` |
|      5 | 1833 | `		if( &zIn[1] < zEnd ){` |
|      3 | 1834 | `			int c = zIn[1];` |
|      3 | 1835 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - | 1836 | `				/* Ignore the backslash */` |
|      3 | 1837 | `				zIn++;` |
|      1 | 1838 | `			}` |
|      2 | 1839 | `		}else{` |
|      3 | 1840 | `			break;` |
|      - | 1841 | `		}` |
|      1 | 1842 | `	}` |
|      7 | 1843 | `	return PH7_OK;` |
|      4 | 1844 | `}` |
|      - | 1845 | `/*` |
|      - | 1846 | ` * UTF-8-aware HTML entity machinery, shared by htmlspecialchars/htmlentities/` |
|      - | 1847 | ` * htmlspecialchars_decode/html_entity_decode/get_html_translation_table.` |
|      - | 1848 | ` * The implementations live further down in this file, next to the filter_var` |
|      - | 1849 | ` * FULL_SPECIAL_CHARS machinery they reuse (aHtml401Ent[]/FvHtml401Lookup()/` |
|      - | 1850 | ` * FvUtf8Next()). Semantics are byte-exact vs php 8.5.7; PHL is UTF-8-only` |
|      - | 1851 | ` * so every charset argument other than a UTF-8 alias gets PHP's` |
|      - | 1852 | ` * unsupported-charset warning and is treated as UTF-8.` |
|      - | 1853 | ` *` |
|      - | 1854 | ` * Flag model (the PHP-exact ENT_* values, see constant.c): bit 1 = encode/` |
|      - | 1855 | ` * decode single quotes, bit 2 = double quotes (ENT_QUOTES=3, ENT_COMPAT=2,` |
|      - | 1856 | ` * ENT_NOQUOTES=0); bits 16\|32 select the doctype (0=HTML401, 16=XML1,` |
|      - | 1857 | ` * 32=XHTML, 48=HTML5); ENT_IGNORE=4 drops invalid UTF-8 bytes (wins over` |
|      - | 1858 | ` * ENT_SUBSTITUTE=8, which replaces each with U+FFFD; with neither set the` |
|      - | 1859 | ` * whole result collapses to ""); ENT_DISALLOWED=128 substitutes valid but` |
|      - | 1860 | ` * doctype-disallowed codepoints. The shared default is` |
|      - | 1861 | ` * ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 = 11.` |
|      - | 1862 | ` */` |
|      - | 1863 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bAll,int bDoubleEncode);` |
|      - | 1864 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bFull);` |
|      - | 1865 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx);` |
|      - | 1866 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags);` |
|      - | 1867 | `/*` |
|      - | 1868 | ` * string htmlspecialchars(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1869 | ` *                         [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1870 | ` *  Convert the special characters & < > " ' to HTML entities.` |
|      - | 1871 | ` * Return` |
|      - | 1872 | ` *  The escaped string or NULL on failure.` |
|      - | 1873 | ` */` |
|     42 | 1874 | `static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1875 | `{` |
|     43 | 1876 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1877 | `	const char *zIn;` |
|     43 | 1878 | `	int nLen,bDouble = 1;` |
|      - | 1879 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1880 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     43 | 1881 | `	if( nArg < 1 ){` |
|      - | 1882 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1883 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1884 | `		return PH7_OK;` |
|      - | 1885 | `	}` |
|      - | 1886 | `	/* Extract the target string */` |
|     43 | 1887 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     43 | 1888 | `	if( nArg > 1 ){` |
|     35 | 1889 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     17 | 1890 | `	}` |
|     43 | 1891 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     43 | 1892 | `	if( nArg > 3 ){` |
|      7 | 1893 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      3 | 1894 | `	}` |
|     43 | 1895 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,0,bDouble);` |
|     43 | 1896 | `	return PH7_OK;` |
|     22 | 1897 | `}` |
|      - | 1898 | `/*` |
|      - | 1899 | ` * string htmlspecialchars_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401])` |
|      - | 1900 | ` *  Convert the special HTML entities (&amp; &lt; &gt; &quot; and the` |
|      - | 1901 | ` *  numeric/doctype forms of the two quotes) back to characters.` |
|      - | 1902 | ` * Return` |
|      - | 1903 | ` *  The unescaped string or NULL on failure.` |
|      - | 1904 | ` */` |
|     22 | 1905 | `static int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1906 | `{` |
|     23 | 1907 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1908 | `	const char *zIn;` |
|      - | 1909 | `	int nLen;` |
|      - | 1910 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1911 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     23 | 1912 | `	if( nArg < 1 ){` |
|      - | 1913 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1914 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1915 | `		return PH7_OK;` |
|      - | 1916 | `	}` |
|      - | 1917 | `	/* Extract the target string */` |
|     23 | 1918 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 1919 | `	if( nArg > 1 ){` |
|      9 | 1920 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1921 | `	}` |
|     23 | 1922 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,0);` |
|     23 | 1923 | `	return PH7_OK;` |
|     12 | 1924 | `}` |
|      - | 1925 | `/*` |
|      - | 1926 | ` * array get_html_translation_table(int $table = HTML_SPECIALCHARS` |
|      - | 1927 | ` *      [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 [, string $encoding = "UTF-8"]])` |
|      - | 1928 | ` *  Return the translation table used by htmlspecialchars() (HTML_SPECIALCHARS)` |
|      - | 1929 | ` *  or htmlentities() (HTML_ENTITIES) as character => entity pairs.` |
|      - | 1930 | ` * Return` |
|      - | 1931 | ` *  The translation table as an array or NULL on failure.` |
|      - | 1932 | ` */` |
|     12 | 1933 | `static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1934 | `{` |
|     13 | 1935 | `	int iTable = 0; /* HTML_SPECIALCHARS */` |
|     13 | 1936 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|     13 | 1937 | `	if( nArg > 0 ){` |
|     11 | 1938 | `		iTable = ph7_value_to_int(apArg[0]);` |
|      5 | 1939 | `	}` |
|     13 | 1940 | `	if( nArg > 1 ){` |
|      9 | 1941 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1942 | `	}` |
|     13 | 1943 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     13 | 1944 | `	HtmlTranslationTable(pCtx,iTable,iFlags);` |
|     13 | 1945 | `	return PH7_OK;` |
|      1 | 1946 | `}` |
|      - | 1947 | `/*` |
|      - | 1948 | ` * string htmlentities(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1949 | ` *                     [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1950 | ` *  Convert all applicable characters to HTML entities: the specials plus` |
|      - | 1951 | ` *  every codepoint with an HTML 4.01 named entity (aHtml401Ent[]).` |
|      - | 1952 | ` * Return` |
|      - | 1953 | ` *  The encoded string or NULL on failure.` |
|      - | 1954 | ` */` |
|     30 | 1955 | `static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1956 | `{` |
|     31 | 1957 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1958 | `	const char *zIn;` |
|     31 | 1959 | `	int nLen,bDouble = 1;` |
|      - | 1960 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1961 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     31 | 1962 | `	if( nArg < 1 ){` |
|      - | 1963 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1964 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1965 | `		return PH7_OK;` |
|      - | 1966 | `	}` |
|      - | 1967 | `	/* Extract the target string */` |
|     31 | 1968 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 1969 | `	if( nArg > 1 ){` |
|     19 | 1970 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 | 1971 | `	}` |
|     31 | 1972 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     31 | 1973 | `	if( nArg > 3 ){` |
|      3 | 1974 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      1 | 1975 | `	}` |
|     31 | 1976 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,1,bDouble);` |
|     31 | 1977 | `	return PH7_OK;` |
|     16 | 1978 | `}` |
|      - | 1979 | `/*` |
|      - | 1980 | ` * string html_entity_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1981 | ` *                           [, string $encoding = "UTF-8"]])` |
|      - | 1982 | ` *  Convert HTML entities (named — case-sensitive — and numeric, decimal or` |
|      - | 1983 | ` *  hex) back to their UTF-8 characters. The reverse of htmlentities().` |
|      - | 1984 | ` * Return` |
|      - | 1985 | ` *  The decoded string or NULL on failure.` |
|      - | 1986 | ` */` |
|     58 | 1987 | `static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1988 | `{` |
|     59 | 1989 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1990 | `	const char *zIn;` |
|      - | 1991 | `	int nLen;` |
|      - | 1992 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1993 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     59 | 1994 | `	if( nArg < 1 ){` |
|      - | 1995 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1996 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1997 | `		return PH7_OK;` |
|      - | 1998 | `	}` |
|      - | 1999 | `	/* Extract the target string */` |
|     59 | 2000 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     59 | 2001 | `	if( nArg > 1 ){` |
|     27 | 2002 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     13 | 2003 | `	}` |
|     59 | 2004 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     59 | 2005 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,1);` |
|     59 | 2006 | `	return PH7_OK;` |
|     30 | 2007 | `}` |
|      - | 2008 | `/*` |
|      - | 2009 | ` * int strlen($string)` |
|      - | 2010 | ` *  return the length of the given string.` |
|      - | 2011 | ` * Parameter` |
|      - | 2012 | ` *  string: The string being measured for length.` |
|      - | 2013 | ` * Return` |
|      - | 2014 | ` *  length of the given string.` |
|      - | 2015 | ` */` |
|  75310 | 2016 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2017 | `{` |
|  75315 | 2018 | `	int iLen = 0;` |
|  75315 | 2019 | `	if( nArg > 0 ){` |
|  75315 | 2020 | `		StrNullArgNotice(pCtx,apArg[0],"strlen",1,"$string");` |
|  75315 | 2021 | `		ph7_value_to_string(apArg[0],&iLen);` |
|  37655 | 2022 | `	}` |
|      - | 2023 | `	/* String length */` |
|  75315 | 2024 | `	ph7_result_int(pCtx,iLen);` |
|  75315 | 2025 | `	return PH7_OK;` |
|      5 | 2026 | `}` |
|      - | 2027 | `/*` |
|      - | 2028 | ` * int strcmp(string $str1,string $str2)` |
|      - | 2029 | ` *  Perform a binary safe string comparison.` |
|      - | 2030 | ` * Parameter` |
|      - | 2031 | ` *  str1: The first string` |
|      - | 2032 | ` *  str2: The second string` |
|      - | 2033 | ` * Return` |
|      - | 2034 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2035 | ` *  than str2, and 0 if they are equal.` |
|      - | 2036 | ` */` |
|     72 | 2037 | `static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2038 | `{` |
|      - | 2039 | `	const char *z1,*z2;` |
|      - | 2040 | `	int n1,n2;` |
|      - | 2041 | `	int res;` |
|     73 | 2042 | `	if( nArg < 2 ){` |
|    ! 0 | 2043 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 2044 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 2045 | `		return PH7_OK;` |
|      - | 2046 | `	}` |
|      - | 2047 | `	/* Perform the comparison */` |
|     73 | 2048 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     73 | 2049 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     73 | 2050 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2051 | `	/* Comparison result */` |
|     73 | 2052 | `	ph7_result_int(pCtx,res);` |
|     73 | 2053 | `	return PH7_OK;` |
|     37 | 2054 | `}` |
|      - | 2055 | `/*` |
|      - | 2056 | ` * Natural-order comparison core (Martin Pool's natcompare as adapted by php's` |
|      - | 2057 | ` * ext/standard/strnatcmp.c): digit runs compare numerically — the longer run` |
|      - | 2058 | ` * wins, a leading zero flips to fractional first-difference-wins semantics —` |
|      - | 2059 | ` * everything else compares bytewise with whitespace skipped.` |
|      - | 2060 | ` */` |
|     16 | 2061 | `static int StrNatCompareRight(const char **pa,const char *aEnd,const char **pb,const char *bEnd)` |
|      1 | 2062 | `{` |
|     17 | 2063 | `	int bias = 0;` |
|     30 | 2064 | `	for(;;){` |
|     39 | 2065 | `		int da = (*pa < aEnd) && SyisDigit(**pa);` |
|     39 | 2066 | `		int db = (*pb < bEnd) && SyisDigit(**pb);` |
|     39 | 2067 | `		if( !da && !db ){ return bias; }` |
|     31 | 2068 | `		if( !da ){ return -1; }` |
|     25 | 2069 | `		if( !db ){ return 1; }` |
|     23 | 2070 | `		if( **pa < **pb ){ if( !bias ){ bias = -1; } }` |
|     21 | 2071 | `		else if( **pa > **pb ){ if( !bias ){ bias = 1; } }` |
|     23 | 2072 | `		(*pa)++;` |
|     23 | 2073 | `		(*pb)++;` |
|      1 | 2074 | `	}` |
|      9 | 2075 | `}` |
|      2 | 2076 | `static int StrNatCompareLeft(const char **pa,const char *aEnd,const char **pb,const char *bEnd)` |
|      1 | 2077 | `{` |
|      1 | 2078 | `	for(;;){` |
|      3 | 2079 | `		int da = (*pa < aEnd) && SyisDigit(**pa);` |
|      3 | 2080 | `		int db = (*pb < bEnd) && SyisDigit(**pb);` |
|      3 | 2081 | `		if( !da && !db ){ return 0; }` |
|      3 | 2082 | `		if( !da ){ return -1; }` |
|      3 | 2083 | `		if( !db ){ return 1; }` |
|      3 | 2084 | `		if( **pa < **pb ){ return -1; }` |
|    ! 0 | 2085 | `		if( **pa > **pb ){ return 1; }` |
|    ! 0 | 2086 | `		(*pa)++;` |
|    ! 0 | 2087 | `		(*pb)++;` |
|    ! 0 | 2088 | `	}` |
|      2 | 2089 | `}` |
|     20 | 2090 | `static int StrNatCmpCore(const char *zA,int nA,const char *zB,int nB,int bFold)` |
|      1 | 2091 | `{` |
|     21 | 2092 | `	const char *a = zA,*aEnd = &zA[nA];` |
|     21 | 2093 | `	const char *b = zB,*bEnd = &zB[nB];` |
|     59 | 2094 | `	for(;;){` |
|      - | 2095 | `		int ca,cb;` |
|     73 | 2096 | `		while( a < aEnd && SyisSpace(a[0]) ){ a++; }` |
|     71 | 2097 | `		while( b < bEnd && SyisSpace(b[0]) ){ b++; }` |
|     71 | 2098 | `		ca = (a < aEnd) ? (unsigned char)a[0] : 0;` |
|     71 | 2099 | `		cb = (b < bEnd) ? (unsigned char)b[0] : 0;` |
|     71 | 2100 | `		if( SyisDigit(ca) && SyisDigit(cb) ){` |
|     18 | 2101 | `			int r = (ca == '0' \|\| cb == '0')` |
|      2 | 2102 | `				? StrNatCompareLeft(&a,aEnd,&b,bEnd)` |
|     25 | 2103 | `				: StrNatCompareRight(&a,aEnd,&b,bEnd);` |
|     19 | 2104 | `			if( r ){ return r; }` |
|      3 | 2105 | `			continue;` |
|      - | 2106 | `		}` |
|     53 | 2107 | `		if( ca == 0 && cb == 0 ){ return 0; }` |
|     49 | 2108 | `		if( bFold ){` |
|     49 | 2109 | `			ca = SyToLower(ca);` |
|     49 | 2110 | `			cb = SyToLower(cb);` |
|     24 | 2111 | `		}` |
|     49 | 2112 | `		if( ca < cb ){ return -1; }` |
|     49 | 2113 | `		if( ca > cb ){ return 1; }` |
|     49 | 2114 | `		a++;` |
|     49 | 2115 | `		b++;` |
|      1 | 2116 | `	}` |
|     11 | 2117 | `}` |
|      - | 2118 | `/*` |
|      - | 2119 | ` * int strnatcmp(string $string1, string $string2)` |
|      - | 2120 | ` * int strnatcasecmp(string $string1, string $string2)` |
|      - | 2121 | ` *  Natural-order string comparison ("img2" < "img10"), case folded for the` |
|      - | 2122 | ` *  latter. php 8.2+ normalizes the result to -1/0/1.` |
|      - | 2123 | ` */` |
|     20 | 2124 | `static int PH7_builtin_strnatcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2125 | `{` |
|      - | 2126 | `	const char *z1,*z2,*zFunc;` |
|      - | 2127 | `	int n1,n2,bFold;` |
|     21 | 2128 | `	if( nArg < 2 ){` |
|    ! 0 | 2129 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2130 | `		return PH7_OK;` |
|      - | 2131 | `	}` |
|     21 | 2132 | `	zFunc = ph7_function_name(pCtx);` |
|     21 | 2133 | `	bFold = zFunc[sizeof("strnat")-1] == 'c'; /* strnatCasecmp */` |
|     21 | 2134 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     21 | 2135 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     21 | 2136 | `	ph7_result_int(pCtx,StrNatCmpCore(z1,n1,z2,n2,bFold));` |
|     21 | 2137 | `	return PH7_OK;` |
|     11 | 2138 | `}` |
|      - | 2139 | `/*` |
|      - | 2140 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 2141 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 2142 | ` * Parameter` |
|      - | 2143 | ` *  str1: The first string` |
|      - | 2144 | ` *  str2: The second string` |
|      - | 2145 | ` * Return` |
|      - | 2146 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2147 | ` *  than str2, and 0 if they are equal.` |
|      - | 2148 | ` */` |
|     66 | 2149 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2150 | `{` |
|      - | 2151 | `	const char *z1,*z2;` |
|      - | 2152 | `	int res;` |
|      - | 2153 | `	int n;` |
|     68 | 2154 | `	if( nArg < 3 ){` |
|      - | 2155 | `		/* Perform a standard comparison */` |
|    ! 0 | 2156 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 2157 | `	}` |
|      - | 2158 | `	/* Desired comparison length */` |
|     68 | 2159 | `	n  = ph7_value_to_int(apArg[2]);` |
|     68 | 2160 | `	if( n < 0 ){` |
|      - | 2161 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 2162 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2163 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 2164 | `			ph7_function_name(pCtx));` |
|      - | 2165 | `	}` |
|      - | 2166 | `	/* Perform the comparison */` |
|     66 | 2167 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     66 | 2168 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     66 | 2169 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 2170 | `	/* Comparison result */` |
|     66 | 2171 | `	ph7_result_int(pCtx,res);` |
|     66 | 2172 | `	return PH7_OK;` |
|     35 | 2173 | `}` |
|      - | 2174 | `/*` |
|      - | 2175 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 2176 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 2177 | ` * Parameter` |
|      - | 2178 | ` *  str1: The first string` |
|      - | 2179 | ` *  str2: The second string` |
|      - | 2180 | ` * Return` |
|      - | 2181 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2182 | ` *  than str2, and 0 if they are equal.` |
|      - | 2183 | ` */` |
|    140 | 2184 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2185 | `{` |
|      - | 2186 | `	const char *z1,*z2;` |
|      - | 2187 | `	int n1,n2;` |
|      - | 2188 | `	int res;` |
|    141 | 2189 | `	if( nArg < 2 ){` |
|    ! 0 | 2190 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 2191 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 2192 | `		return PH7_OK;` |
|      - | 2193 | `	}` |
|      - | 2194 | `	/* Perform the comparison */` |
|    141 | 2195 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|    141 | 2196 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|    141 | 2197 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2198 | `	/* Comparison result */` |
|    141 | 2199 | `	ph7_result_int(pCtx,res);` |
|    141 | 2200 | `	return PH7_OK;` |
|     71 | 2201 | `}` |
|      - | 2202 | `/*` |
|      - | 2203 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 2204 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 2205 | ` * Parameter` |
|      - | 2206 | ` *  $str1: The first string` |
|      - | 2207 | ` *  $str2: The second string` |
|      - | 2208 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 2209 | ` * Return` |
|      - | 2210 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2211 | ` *  than str2, and 0 if they are equal.` |
|      - | 2212 | ` */` |
|     40 | 2213 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2214 | `{` |
|      - | 2215 | `	const char *z1,*z2;` |
|      - | 2216 | `	int res;` |
|      - | 2217 | `	int n;` |
|     45 | 2218 | `	if( nArg < 3 ){` |
|      - | 2219 | `		/* Perform a standard comparison */` |
|    ! 0 | 2220 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 2221 | `	}` |
|      - | 2222 | `	/* Desired comparison length */` |
|     45 | 2223 | `	n  = ph7_value_to_int(apArg[2]);` |
|     45 | 2224 | `	if( n < 0 ){` |
|      - | 2225 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 2226 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2227 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 2228 | `			ph7_function_name(pCtx));` |
|      - | 2229 | `	}` |
|      - | 2230 | `	/* Perform the comparison */` |
|     43 | 2231 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     43 | 2232 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     43 | 2233 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 2234 | `	/* Comparison result */` |
|     43 | 2235 | `	ph7_result_int(pCtx,res);` |
|     43 | 2236 | `	return PH7_OK;` |
|     25 | 2237 | `}` |
|      - | 2238 | `/*` |
|      - | 2239 | ` * Implode context [i.e: it's private data].` |
|      - | 2240 | ` * A pointer to the following structure is forwarded` |
|      - | 2241 | ` * verbatim to the array walker callback defined below.` |
|      - | 2242 | ` */` |
|      - | 2243 | `struct implode_data {` |
|      - | 2244 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 2245 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 2246 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 2247 | `	int nSeplen;          /* Separator length */` |
|      - | 2248 | `	int bFirst;           /* TRUE if first call */` |
|      - | 2249 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 2250 | `	sxi32 rc;             /* Captured allocation rc; SXERR_MEM => the builtin raises an OOM fatal */` |
|      - | 2251 | `};` |
|      - | 2252 | `/*` |
|      - | 2253 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 2254 | ` * The following routine is invoked for each array entry passed` |
|      - | 2255 | ` * to the implode() function.` |
|      - | 2256 | ` */` |
| 150622 | 2257 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 2258 | `{` |
|  75311 | 2259 | `	SXUNUSED(pKey);` |
| 150627 | 2260 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 2261 | `	const char *zData;` |
|      - | 2262 | `	int nLen;` |
| 150627 | 2263 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 2264 | `		if( pData->nSeplen > 0 ){` |
|      3 | 2265 | `			if( !pData->bFirst ){` |
|      - | 2266 | `				/* append the separator first */` |
|      3 | 2267 | `				if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 2268 | `					pData->rc = SXERR_MEM;` |
|    ! 0 | 2269 | `					return PH7_ABORT;` |
|      - | 2270 | `				}` |
|      2 | 2271 | `			}else{` |
|    ! 0 | 2272 | `				pData->bFirst = 0;` |
|      - | 2273 | `			}` |
|      1 | 2274 | `		}` |
|      - | 2275 | `		/* Recurse */` |
|      3 | 2276 | `		pData->bFirst = 1;` |
|      3 | 2277 | `		pData->nRecCount++;` |
|      3 | 2278 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 2279 | `		pData->nRecCount--;` |
|      - | 2280 | `		/* Propagate an allocation failure surfaced deeper in the recursion. */` |
|      3 | 2281 | `		if( pData->rc != SXRET_OK ){` |
|    ! 0 | 2282 | `			return PH7_ABORT;` |
|      - | 2283 | `		}` |
|      3 | 2284 | `		return PH7_OK;` |
|      - | 2285 | `	}` |
|      - | 2286 | `	/* Extract the string representation of the entry value */` |
| 150625 | 2287 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 2288 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 150625 | 2289 | `	if( pData->bFirst ){` |
|  33435 | 2290 | `		pData->bFirst = 0;` |
| 133910 | 2291 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 2292 | `		/* append the separator first */` |
| 117115 | 2293 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 2294 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 2295 | `			return PH7_ABORT;` |
|      - | 2296 | `		}` |
|  58555 | 2297 | `	}` |
|      - | 2298 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 150625 | 2299 | `	if( nLen > 0 ){` |
| 137967 | 2300 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2301 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 2302 | `			return PH7_ABORT;` |
|      - | 2303 | `		}` |
|  68981 | 2304 | `	}` |
| 150625 | 2305 | `	return PH7_OK;` |
|  75316 | 2306 | `}` |
|      - | 2307 | `/*` |
|      - | 2308 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 2309 | ` * string implode(array $pieces,...)` |
|      - | 2310 | ` *  Join array elements with a string.` |
|      - | 2311 | ` * $glue` |
|      - | 2312 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 2313 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 2314 | ` * $pieces` |
|      - | 2315 | ` *   The array of strings to implode.` |
|      - | 2316 | ` * Return` |
|      - | 2317 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 2318 | ` *  order, with the glue string between each element.` |
|      - | 2319 | ` */` |
|  33458 | 2320 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2321 | `{` |
|      - | 2322 | `	struct implode_data imp_data;` |
|  33463 | 2323 | `	int i = 1;` |
|  33463 | 2324 | `	if( nArg < 1 ){` |
|      - | 2325 | `		/* Missing argument,return NULL */` |
|    ! 0 | 2326 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2327 | `		return PH7_OK;` |
|      - | 2328 | `	}` |
|      - | 2329 | `	/* Prepare the implode context */` |
|  33463 | 2330 | `	imp_data.pCtx = pCtx;` |
|  33463 | 2331 | `	imp_data.bRecursive = 0;` |
|  33463 | 2332 | `	imp_data.bFirst = 1;` |
|  33463 | 2333 | `	imp_data.nRecCount = 0;` |
|  33463 | 2334 | `	imp_data.rc = SXRET_OK;` |
|  33463 | 2335 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  33461 | 2336 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  33461 | 2337 | `		if( nArg > 1 && !ph7_value_is_array(apArg[1]) && !ph7_value_is_null(apArg[1]) ){` |
|      - | 2338 | `			/* php: implode($glue, $pieces) requires an ARRAY. PH7 stringified whatever it` |
|      - | 2339 | `			 * was handed, so implode(",", 5) quietly returned "5". */` |
|      - | 2340 | `			char zBuf[64];` |
|      4 | 2341 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 2342 | `				"implode(): Argument #2 ($array) must be of type ?array, %s given",` |
|      2 | 2343 | `				VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 2344 | `		}` |
|  16732 | 2345 | `	}else{` |
|      3 | 2346 | `		imp_data.zSep = 0;` |
|      3 | 2347 | `		imp_data.nSeplen = 0;` |
|      3 | 2348 | `		i = 0;` |
|      - | 2349 | `	}` |
|  33461 | 2350 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2351 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2352 | `	}` |
|      - | 2353 | `	/* Start the 'join' process */` |
|  66917 | 2354 | `	while( i < nArg ){` |
|  33461 | 2355 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2356 | `			/* Iterate throw array entries */` |
|  33461 | 2357 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2358 | `			/* Surface a callback allocation failure as a fatal */` |
|  33461 | 2359 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2360 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2361 | `			}` |
|  16733 | 2362 | `		}else{` |
|      - | 2363 | `			const char *zData;` |
|      - | 2364 | `			int nLen;` |
|      - | 2365 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 2366 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2367 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 2368 | `			if( imp_data.bFirst ){` |
|    ! 0 | 2369 | `				imp_data.bFirst = 0;` |
|    ! 0 | 2370 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2371 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 2372 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2373 | `				}` |
|    ! 0 | 2374 | `			}` |
|      - | 2375 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 2376 | `			if( nLen > 0 ){` |
|    ! 0 | 2377 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2378 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2379 | `				}` |
|    ! 0 | 2380 | `			}` |
|      - | 2381 | `		}` |
|  33461 | 2382 | `		i++;` |
|      5 | 2383 | `	}` |
|  33461 | 2384 | `	return PH7_OK;` |
|  16734 | 2385 | `}` |
|      - | 2386 | `/*` |
|      - | 2387 | ` * Symisc eXtension:` |
|      - | 2388 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 2389 | ` * Purpose` |
|      - | 2390 | ` *  Same as implode() but recurse on arrays.` |
|      - | 2391 | ` * Example:` |
|      - | 2392 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 2393 | ` *   echo implode_recursive("/",$a);` |
|      - | 2394 | ` *   Will output` |
|      - | 2395 | ` *     usr/home/dean.` |
|      - | 2396 | ` *   While the standard implode would produce.` |
|      - | 2397 | ` *    usr/Array.` |
|      - | 2398 | ` * Parameter` |
|      - | 2399 | ` *  Refer to implode().` |
|      - | 2400 | ` * Return` |
|      - | 2401 | ` *  Refer to implode().` |
|      - | 2402 | ` */` |
|     12 | 2403 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2404 | `{` |
|      - | 2405 | `	struct implode_data imp_data;` |
|     13 | 2406 | `	int i = 1;` |
|     13 | 2407 | `	if( nArg < 1 ){` |
|      - | 2408 | `		/* Missing argument,return NULL */` |
|      3 | 2409 | `		ph7_result_null(pCtx);` |
|      3 | 2410 | `		return PH7_OK;` |
|      - | 2411 | `	}` |
|      - | 2412 | `	/* Prepare the implode context */` |
|     11 | 2413 | `	imp_data.pCtx = pCtx;` |
|     11 | 2414 | `	imp_data.bRecursive = 1;` |
|     11 | 2415 | `	imp_data.bFirst = 1;` |
|     11 | 2416 | `	imp_data.nRecCount = 0;` |
|     11 | 2417 | `	imp_data.rc = SXRET_OK;` |
|     11 | 2418 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 2419 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 2420 | `	}else{` |
|    ! 0 | 2421 | `		imp_data.zSep = 0;` |
|    ! 0 | 2422 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 2423 | `		i = 0;` |
|      - | 2424 | `	}` |
|     11 | 2425 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2426 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2427 | `	}` |
|      - | 2428 | `	/* Start the 'join' process */` |
|     21 | 2429 | `	while( i < nArg ){` |
|     11 | 2430 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2431 | `			/* Iterate throw array entries */` |
|      3 | 2432 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2433 | `			/* Surface a callback allocation failure as a fatal */` |
|      3 | 2434 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2435 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2436 | `			}` |
|      2 | 2437 | `		}else{` |
|      - | 2438 | `			const char *zData;` |
|      - | 2439 | `			int nLen;` |
|      - | 2440 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 2441 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2442 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 2443 | `			if( imp_data.bFirst ){` |
|      9 | 2444 | `				imp_data.bFirst = 0;` |
|      4 | 2445 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2446 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 2447 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2448 | `				}` |
|    ! 0 | 2449 | `			}` |
|      - | 2450 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 2451 | `			if( nLen > 0 ){` |
|      9 | 2452 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2453 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2454 | `				}` |
|      4 | 2455 | `			}` |
|      - | 2456 | `		}` |
|     11 | 2457 | `		i++;` |
|      1 | 2458 | `	}` |
|     11 | 2459 | `	return PH7_OK;` |
|      7 | 2460 | `}` |
|      - | 2461 | `/*` |
|      - | 2462 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 2463 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 2464 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 2465 | ` * Parameters` |
|      - | 2466 | ` *  $delimiter` |
|      - | 2467 | ` *   The boundary string.` |
|      - | 2468 | ` * $string` |
|      - | 2469 | ` *   The input string.` |
|      - | 2470 | ` * $limit` |
|      - | 2471 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 2472 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 2473 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 2474 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 2475 | ` * Returns` |
|      - | 2476 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 2477 | ` *  on boundaries formed by the delimiter.` |
|      - | 2478 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 2479 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 2480 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 2481 | ` *  will be returned.` |
|      - | 2482 | ` * NOTE:` |
|      - | 2483 | ` *  Negative limit is not supported.` |
|      - | 2484 | ` */` |
|   6644 | 2485 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2486 | `{` |
|      - | 2487 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 2488 | `	int nDelim,nStrlen,iLimit;` |
|      - | 2489 | `	ph7_value *pArray;` |
|      - | 2490 | `	ph7_value *pValue;` |
|      - | 2491 | `	sxu32 nOfft;` |
|      - | 2492 | `	sxi32 rc;` |
|   6649 | 2493 | `	if( nArg < 2 ){` |
|      - | 2494 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2495 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2496 | `		return PH7_OK;` |
|      - | 2497 | `	}` |
|      - | 2498 | `	/* Extract the delimiter */` |
|   6649 | 2499 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   6649 | 2500 | `	if( nDelim < 1 ){` |
|      - | 2501 | `		/* Empty delimiter: PHP 8 throws a catchable ValueError. */` |
|      5 | 2502 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2503 | `			"explode(): Argument #1 ($separator) must not be empty");` |
|      - | 2504 | `	}` |
|      - | 2505 | `	/* Extract the string */` |
|   6645 | 2506 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   6645 | 2507 | `	if( nStrlen < 1 ){` |
|      - | 2508 | `		/* Empty string: normally an array with a single empty element (PHP behavior).` |
|      - | 2509 | `		 * A negative limit drops the last -limit components, so the sole empty` |
|      - | 2510 | `		 * component is dropped and the result is an empty array. */` |
|      7 | 2511 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      7 | 2512 | `		if( pArrayTmp == 0 ){` |
|      - | 2513 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 2514 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2515 | `			return PH7_OK;` |
|      - | 2516 | `		}` |
|      7 | 2517 | `		if( !(nArg > 2 && ph7_value_to_int(apArg[2]) < 0) ){` |
|      5 | 2518 | `			ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      5 | 2519 | `			if( pValueTmp == 0 ){` |
|      - | 2520 | `				/* Out of memory,return FALSE */` |
|    ! 0 | 2521 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 2522 | `				return PH7_OK;` |
|      - | 2523 | `			}` |
|      5 | 2524 | `			ph7_value_string(pValueTmp, "", 0);` |
|      5 | 2525 | `			if( ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp) != SXRET_OK ){` |
|    ! 0 | 2526 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2527 | `			}` |
|      2 | 2528 | `		}` |
|      7 | 2529 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      7 | 2530 | `		return PH7_OK;` |
|      - | 2531 | `	}` |
|      - | 2532 | `	/* Point to the end of the string */` |
|   6639 | 2533 | `	zEnd = &zString[nStrlen];` |
|      - | 2534 | `	/* Create the array */` |
|   6639 | 2535 | `	pArray =  ph7_context_new_array(pCtx);` |
|   6639 | 2536 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   6639 | 2537 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2538 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 2539 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2540 | `		return PH7_OK;` |
|      - | 2541 | `	}` |
|      - | 2542 | `	/* Set a defualt limit */` |
|   6639 | 2543 | `	iLimit = SXI32_HIGH;` |
|   6639 | 2544 | `	if( nArg > 2 ){` |
|     38 | 2545 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     38 | 2546 | `		if( iLimit < 0 ){` |
|      - | 2547 | `			/* Negative limit: keep all components except the last -iLimit (PHP).` |
|      - | 2548 | `			 * Pre-count the components (delimiters + 1), then emit only the first` |
|      - | 2549 | `			 * nKeep CLEAN components — no trailing-remainder merge (the difference` |
|      - | 2550 | `			 * from the positive path). nKeep <= 0 drops everything -> empty array. */` |
|     17 | 2551 | `			int nTotal = 1,nKeep;` |
|     17 | 2552 | `			const char *zScan = zString;` |
|      - | 2553 | `			sxu32 nScanOfft;` |
|     57 | 2554 | `			while( SyBlobSearch(zScan,(sxu32)(zEnd - zScan),zDelim,nDelim,&nScanOfft) == SXRET_OK ){` |
|     41 | 2555 | `				nTotal++;` |
|     41 | 2556 | `				zScan = &zScan[nScanOfft + nDelim];` |
|      1 | 2557 | `			}` |
|     17 | 2558 | `			nKeep = nTotal + iLimit; /* iLimit < 0, so this is nTotal - (-iLimit) */` |
|     49 | 2559 | `			while( nKeep > (int)ph7_array_count(pArray)` |
|     39 | 2560 | `				&& SyBlobSearch(zString,(sxu32)(zEnd - zString),zDelim,nDelim,&nOfft) == SXRET_OK ){` |
|      - | 2561 | `				/* Emit the next clean component */` |
|     23 | 2562 | `				zCur = &zString[nOfft];` |
|     23 | 2563 | `				ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|     23 | 2564 | `				if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2565 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2566 | `				}` |
|     23 | 2567 | `				zString = &zCur[nDelim];` |
|     23 | 2568 | `				ph7_value_reset_string_cursor(pValue);` |
|      1 | 2569 | `			}` |
|     17 | 2570 | `			ph7_result_value(pCtx,pArray);` |
|     17 | 2571 | `			return PH7_OK;` |
|      - | 2572 | `		}` |
|     22 | 2573 | `		if( iLimit == 0 ){` |
|      5 | 2574 | `			iLimit = 1;` |
|      2 | 2575 | `		}` |
|     22 | 2576 | `		iLimit--;` |
|      9 | 2577 | `	}` |
|      - | 2578 | `	/* Start exploding */` |
|  81066 | 2579 | `	for(;;){` |
| 162137 | 2580 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 162137 | 2581 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 2582 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   6623 | 2583 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   6623 | 2584 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2585 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2586 | `			}` |
|   6623 | 2587 | `			break;` |
|      - | 2588 | `		}` |
|      - | 2589 | `		/* Point to the desired offset */` |
| 155519 | 2590 | `		zCur = &zString[nOfft];` |
|      - | 2591 | `		/* Perform the store operation (may be empty) */` |
| 155519 | 2592 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 155519 | 2593 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2594 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2595 | `		}` |
|      - | 2596 | `		/* Point beyond the delimiter */` |
| 155519 | 2597 | `		zString = &zCur[nDelim];` |
|      - | 2598 | `		/* Reset the cursor */` |
| 155519 | 2599 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 2600 | `	}` |
|      - | 2601 | `	/* Return the freshly created array */` |
|   6623 | 2602 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2603 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 2604 | `	 * released as soon we return from this foregin function.` |
|      - | 2605 | `	 */` |
|   6623 | 2606 | `	return PH7_OK;` |
|   3327 | 2607 | `}` |
|      - | 2608 | `/*` |
|      - | 2609 | ` * string trim(string $str[,string $charlist ])` |
|      - | 2610 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2611 | ` * Parameters` |
|      - | 2612 | ` *  $str` |
|      - | 2613 | ` *   The string that will be trimmed.` |
|      - | 2614 | ` * $charlist` |
|      - | 2615 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2616 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2617 | ` *   With .. you can specify a range of characters.` |
|      - | 2618 | ` * Returns.` |
|      - | 2619 | ` *  Thr processed string.` |
|      - | 2620 | ` * NOTE:` |
|      - | 2621 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2622 | ` */` |
|  14410 | 2623 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2624 | `{` |
|  14415 | 2625 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"trim",1,"$string"); }` |
|      - | 2626 | `	const char *zString;` |
|      - | 2627 | `	int nLen;` |
|  14415 | 2628 | `	if( nArg < 1 ){` |
|      - | 2629 | `		/* Missing arguments,return null */` |
|    ! 0 | 2630 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2631 | `		return PH7_OK;` |
|      - | 2632 | `	}` |
|      - | 2633 | `	/* Extract the target string */` |
|  14415 | 2634 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  14415 | 2635 | `	if( nLen < 1 ){` |
|      - | 2636 | `		/* Empty string,return */` |
|    757 | 2637 | `		ph7_result_string(pCtx,"",0);` |
|    757 | 2638 | `		return PH7_OK;` |
|      - | 2639 | `	}` |
|      - | 2640 | `	/* Start the trim process */` |
|  13663 | 2641 | `	if( nArg < 2 ){` |
|      - | 2642 | `		SyString sStr;` |
|      - | 2643 | `		/* Remove white spaces and NUL bytes */` |
|  13633 | 2644 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  34191 | 2645 | `		SyStringFullTrimSafe(&sStr);` |
|  13633 | 2646 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   6819 | 2647 | `	}else{` |
|      - | 2648 | `		/* Char list */` |
|      - | 2649 | `		const char *zList;` |
|      - | 2650 | `		int nListlen;` |
|     33 | 2651 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     33 | 2652 | `		if( nListlen < 1 ){` |
|      - | 2653 | `			/* Return the string unchanged */` |
|      6 | 2654 | `			ph7_result_string(pCtx,zString,nLen);` |
|      4 | 2655 | `		}else{` |
|      - | 2656 | `			char aMask[256];` |
|     29 | 2657 | `			const char *zEnd = &zString[nLen];` |
|     29 | 2658 | `			const char *zCur = zString;` |
|     29 | 2659 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2660 | `			/* Left trim */` |
|     79 | 2661 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     53 | 2662 | `				zCur++;` |
|      3 | 2663 | `			}` |
|      - | 2664 | `			/* Right trim */` |
|     79 | 2665 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     53 | 2666 | `				zEnd--;` |
|      3 | 2667 | `			}` |
|     29 | 2668 | `			if( zCur >= zEnd ){` |
|      - | 2669 | `				/* Return the empty string */` |
|    ! 0 | 2670 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2671 | `			}else{` |
|     29 | 2672 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2673 | `			}` |
|      - | 2674 | `		}` |
|      - | 2675 | `	}` |
|  13663 | 2676 | `	return PH7_OK;` |
|   7210 | 2677 | `}` |
|      - | 2678 | `/*` |
|      - | 2679 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 2680 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 2681 | ` * Parameters` |
|      - | 2682 | ` *  $str` |
|      - | 2683 | ` *   The string that will be trimmed.` |
|      - | 2684 | ` * $charlist` |
|      - | 2685 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2686 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2687 | ` *   With .. you can specify a range of characters.` |
|      - | 2688 | ` * Returns.` |
|      - | 2689 | ` *  Thr processed string.` |
|      - | 2690 | ` * NOTE:` |
|      - | 2691 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2692 | ` */` |
|    164 | 2693 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2694 | `{` |
|    168 | 2695 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"rtrim",1,"$string"); }` |
|      - | 2696 | `	const char *zString;` |
|      - | 2697 | `	int nLen;` |
|    168 | 2698 | `	if( nArg < 1 ){` |
|      - | 2699 | `		/* Missing arguments,return null */` |
|    ! 0 | 2700 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2701 | `		return PH7_OK;` |
|      - | 2702 | `	}` |
|      - | 2703 | `	/* Extract the target string */` |
|    168 | 2704 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    168 | 2705 | `	if( nLen < 1 ){` |
|      - | 2706 | `		/* Empty string,return */` |
|      7 | 2707 | `		ph7_result_string(pCtx,"",0);` |
|      7 | 2708 | `		return PH7_OK;` |
|      - | 2709 | `	}` |
|      - | 2710 | `	/* Start the trim process */` |
|    162 | 2711 | `	if( nArg < 2 ){` |
|      - | 2712 | `		SyString sStr;` |
|      - | 2713 | `		/* Remove white spaces and NUL bytes*/` |
|     19 | 2714 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     48 | 2715 | `		SyStringRightTrimSafe(&sStr);` |
|     19 | 2716 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|     10 | 2717 | `	}else{` |
|      - | 2718 | `		/* Char list */` |
|      - | 2719 | `		const char *zList;` |
|      - | 2720 | `		int nListlen;` |
|    144 | 2721 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|    144 | 2722 | `		if( nListlen < 1 ){` |
|      - | 2723 | `			/* Return the string unchanged */` |
|    ! 0 | 2724 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 2725 | `		}else{` |
|      - | 2726 | `			char aMask[256];` |
|    144 | 2727 | `			const char *zEnd = &zString[nLen];` |
|    144 | 2728 | `			const char *zCur = zString;` |
|    144 | 2729 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2730 | `			/* Right trim */` |
|    162 | 2731 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     21 | 2732 | `				zEnd--;` |
|      3 | 2733 | `			}` |
|    144 | 2734 | `			if( zEnd <= zCur ){` |
|      - | 2735 | `				/* Return the empty string */` |
|    ! 0 | 2736 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2737 | `			}else{` |
|    144 | 2738 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2739 | `			}` |
|      - | 2740 | `		}` |
|      - | 2741 | `	}` |
|    162 | 2742 | `	return PH7_OK;` |
|     86 | 2743 | `}` |
|      - | 2744 | `/*` |
|      - | 2745 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 2746 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2747 | ` * Parameters` |
|      - | 2748 | ` *  $str` |
|      - | 2749 | ` *   The string that will be trimmed.` |
|      - | 2750 | ` * $charlist` |
|      - | 2751 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2752 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2753 | ` *   With .. you can specify a range of characters.` |
|      - | 2754 | ` * Returns.` |
|      - | 2755 | ` *  Thr processed string.` |
|      - | 2756 | ` * NOTE:` |
|      - | 2757 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2758 | ` */` |
|     42 | 2759 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2760 | `{` |
|     47 | 2761 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"ltrim",1,"$string"); }` |
|      - | 2762 | `	const char *zString;` |
|      - | 2763 | `	int nLen;` |
|     47 | 2764 | `	if( nArg < 1 ){` |
|      - | 2765 | `		/* Missing arguments,return null */` |
|    ! 0 | 2766 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2767 | `		return PH7_OK;` |
|      - | 2768 | `	}` |
|      - | 2769 | `	/* Extract the target string */` |
|     47 | 2770 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     47 | 2771 | `	if( nLen < 1 ){` |
|      - | 2772 | `		/* Empty string,return */` |
|     23 | 2773 | `		ph7_result_string(pCtx,"",0);` |
|     23 | 2774 | `		return PH7_OK;` |
|      - | 2775 | `	}` |
|      - | 2776 | `	/* Start the trim process */` |
|     29 | 2777 | `	if( nArg < 2 ){` |
|      - | 2778 | `		SyString sStr;` |
|      - | 2779 | `		/* Remove white spaces and NUL byte */` |
|      5 | 2780 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     13 | 2781 | `		SyStringLeftTrimSafe(&sStr);` |
|      5 | 2782 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      3 | 2783 | `	}else{` |
|      - | 2784 | `		/* Char list */` |
|      - | 2785 | `		const char *zList;` |
|      - | 2786 | `		int nListlen;` |
|     25 | 2787 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     25 | 2788 | `		if( nListlen < 1 ){` |
|      - | 2789 | `			/* Return the string unchanged */` |
|      3 | 2790 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2791 | `		}else{` |
|      - | 2792 | `			char aMask[256];` |
|     23 | 2793 | `			const char *zEnd = &zString[nLen];` |
|     23 | 2794 | `			const char *zCur = zString;` |
|     23 | 2795 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2796 | `			/* Left trim */` |
|     57 | 2797 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     39 | 2798 | `				zCur++;` |
|      5 | 2799 | `			}` |
|     23 | 2800 | `			if( zCur >= zEnd ){` |
|      - | 2801 | `				/* Return the empty string */` |
|    ! 0 | 2802 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2803 | `			}else{` |
|     23 | 2804 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2805 | `			}` |
|      - | 2806 | `		}` |
|      - | 2807 | `	}` |
|     29 | 2808 | `	return PH7_OK;` |
|     26 | 2809 | `}` |
|      - | 2810 | `/*` |
|      - | 2811 | ` * string strtolower(string $str)` |
|      - | 2812 | ` *  Make a string lowercase.` |
|      - | 2813 | ` * Parameters` |
|      - | 2814 | ` *  $str` |
|      - | 2815 | ` *   The input string.` |
|      - | 2816 | ` * Returns.` |
|      - | 2817 | ` *  The lowercased string.` |
|      - | 2818 | ` */` |
|  33366 | 2819 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2820 | `{` |
|  33371 | 2821 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtolower",1,"$string"); }` |
|      - | 2822 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2823 | `	int nLen;` |
|  33371 | 2824 | `	if( nArg < 1 ){` |
|      - | 2825 | `		/* Missing arguments,return null */` |
|    ! 0 | 2826 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2827 | `		return PH7_OK;` |
|      - | 2828 | `	}` |
|      - | 2829 | `	/* Extract the target string */` |
|  33371 | 2830 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  33371 | 2831 | `	if( nLen < 1 ){` |
|      - | 2832 | `		/* Empty string,return */` |
|      5 | 2833 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2834 | `		return PH7_OK;` |
|      - | 2835 | `	}` |
|      - | 2836 | `	/* Perform the requested operation */` |
|  33367 | 2837 | `	zEnd = &zString[nLen];` |
| 105216 | 2838 | `	for(;;){` |
| 210437 | 2839 | `		if( zString >= zEnd ){` |
|      - | 2840 | `			/* No more input,break immediately */` |
|  33367 | 2841 | `			break;` |
|      - | 2842 | `		}` |
| 177075 | 2843 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2844 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2845 | `			zCur = zString;` |
|    ! 0 | 2846 | `			zString++;` |
|    ! 0 | 2847 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2848 | `				zString++;` |
|    ! 0 | 2849 | `			}` |
|      - | 2850 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2851 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2852 | `		}else{` |
| 177075 | 2853 | `			int c = zString[0];` |
| 177075 | 2854 | `			if( SyisUpper(c) ){` |
| 174519 | 2855 | `				c = SyToLower(zString[0]);` |
|  87257 | 2856 | `			}` |
|      - | 2857 | `			/* Append character */` |
| 177075 | 2858 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2859 | `			/* Advance the cursor */` |
| 177075 | 2860 | `			zString++;` |
|      - | 2861 | `		}` |
|      5 | 2862 | `	}` |
|  33367 | 2863 | `	return PH7_OK;` |
|  16688 | 2864 | `}` |
|      - | 2865 | `/*` |
|      - | 2866 | ` * string strtolower(string $str)` |
|      - | 2867 | ` *  Make a string uppercase.` |
|      - | 2868 | ` * Parameters` |
|      - | 2869 | ` *  $str` |
|      - | 2870 | ` *   The input string.` |
|      - | 2871 | ` * Returns.` |
|      - | 2872 | ` *  The uppercased string.` |
|      - | 2873 | ` */` |
|     74 | 2874 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2875 | `{` |
|     79 | 2876 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtoupper",1,"$string"); }` |
|      - | 2877 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2878 | `	int nLen;` |
|     79 | 2879 | `	if( nArg < 1 ){` |
|      - | 2880 | `		/* Missing arguments,return null */` |
|    ! 0 | 2881 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2882 | `		return PH7_OK;` |
|      - | 2883 | `	}` |
|      - | 2884 | `	/* Extract the target string */` |
|     79 | 2885 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     79 | 2886 | `	if( nLen < 1 ){` |
|      - | 2887 | `		/* Empty string,return */` |
|      5 | 2888 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2889 | `		return PH7_OK;` |
|      - | 2890 | `	}` |
|      - | 2891 | `	/* Perform the requested operation */` |
|     75 | 2892 | `	zEnd = &zString[nLen];` |
|    145 | 2893 | `	for(;;){` |
|    295 | 2894 | `		if( zString >= zEnd ){` |
|      - | 2895 | `			/* No more input,break immediately */` |
|     75 | 2896 | `			break;` |
|      - | 2897 | `		}` |
|    225 | 2898 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2899 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2900 | `			zCur = zString;` |
|    ! 0 | 2901 | `			zString++;` |
|    ! 0 | 2902 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2903 | `				zString++;` |
|    ! 0 | 2904 | `			}` |
|      - | 2905 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2906 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2907 | `		}else{` |
|    225 | 2908 | `			int c = zString[0];` |
|    225 | 2909 | `			if( SyisLower(c) ){` |
|    208 | 2910 | `				c = SyToUpper(zString[0]);` |
|    102 | 2911 | `			}` |
|      - | 2912 | `			/* Append character */` |
|    225 | 2913 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2914 | `			/* Advance the cursor */` |
|    225 | 2915 | `			zString++;` |
|      - | 2916 | `		}` |
|      5 | 2917 | `	}` |
|     75 | 2918 | `	return PH7_OK;` |
|     42 | 2919 | `}` |
|      - | 2920 | `/*` |
|      - | 2921 | ` * string ucfirst(string $str)` |
|      - | 2922 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2923 | ` *  character is alphabetic.` |
|      - | 2924 | ` * Parameters` |
|      - | 2925 | ` *  $str` |
|      - | 2926 | ` *   The input string.` |
|      - | 2927 | ` * Returns.` |
|      - | 2928 | ` *  The processed string.` |
|      - | 2929 | ` */` |
|      4 | 2930 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2931 | `{` |
|      - | 2932 | `	const char *zString,*zEnd;` |
|      - | 2933 | `	int nLen,c;` |
|      5 | 2934 | `	if( nArg < 1 ){` |
|      - | 2935 | `		/* Missing arguments,return null */` |
|    ! 0 | 2936 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2937 | `		return PH7_OK;` |
|      - | 2938 | `	}` |
|      - | 2939 | `	/* Extract the target string */` |
|      5 | 2940 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2941 | `	if( nLen < 1 ){` |
|      - | 2942 | `		/* Empty string,return */` |
|      3 | 2943 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2944 | `		return PH7_OK;` |
|      - | 2945 | `	}` |
|      - | 2946 | `	/* Perform the requested operation */` |
|      3 | 2947 | `	zEnd = &zString[nLen];` |
|      3 | 2948 | `	c = zString[0];` |
|      3 | 2949 | `	if( SyisLower(c) ){` |
|      3 | 2950 | `		c = SyToUpper(c);` |
|      1 | 2951 | `	}` |
|      - | 2952 | `	/* Append the first character */` |
|      3 | 2953 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2954 | `	zString++;` |
|      3 | 2955 | `	if( zString < zEnd ){` |
|      - | 2956 | `		/* Append the rest of the input verbatim */` |
|      3 | 2957 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2958 | `	}` |
|      3 | 2959 | `	return PH7_OK;` |
|      3 | 2960 | `}` |
|      - | 2961 | `/*` |
|      - | 2962 | ` * string lcfirst(string $str)` |
|      - | 2963 | ` *  Make a string's first character lowercase.` |
|      - | 2964 | ` * Parameters` |
|      - | 2965 | ` *  $str` |
|      - | 2966 | ` *   The input string.` |
|      - | 2967 | ` * Returns.` |
|      - | 2968 | ` *  The processed string.` |
|      - | 2969 | ` */` |
|      4 | 2970 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2971 | `{` |
|      - | 2972 | `	const char *zString,*zEnd;` |
|      - | 2973 | `	int nLen,c;` |
|      5 | 2974 | `	if( nArg < 1 ){` |
|      - | 2975 | `		/* Missing arguments,return null */` |
|    ! 0 | 2976 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2977 | `		return PH7_OK;` |
|      - | 2978 | `	}` |
|      - | 2979 | `	/* Extract the target string */` |
|      5 | 2980 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2981 | `	if( nLen < 1 ){` |
|      - | 2982 | `		/* Empty string,return */` |
|      3 | 2983 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2984 | `		return PH7_OK;` |
|      - | 2985 | `	}` |
|      - | 2986 | `	/* Perform the requested operation */` |
|      3 | 2987 | `	zEnd = &zString[nLen];` |
|      3 | 2988 | `	c = zString[0];` |
|      3 | 2989 | `	if( SyisUpper(c) ){` |
|      3 | 2990 | `		c = SyToLower(c);` |
|      1 | 2991 | `	}` |
|      - | 2992 | `	/* Append the first character */` |
|      3 | 2993 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2994 | `	zString++;` |
|      3 | 2995 | `	if( zString < zEnd ){` |
|      - | 2996 | `		/* Append the rest of the input verbatim */` |
|      3 | 2997 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2998 | `	}` |
|      3 | 2999 | `	return PH7_OK;` |
|      3 | 3000 | `}` |
|      - | 3001 | `/*` |
|      - | 3002 | ` * int ord(string $string)` |
|      - | 3003 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 3004 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 3005 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 3006 | ` * Parameters` |
|      - | 3007 | ` *  $string` |
|      - | 3008 | ` *   The input string.` |
|      - | 3009 | ` * Returns` |
|      - | 3010 | ` *  The ASCII value as an integer.` |
|      - | 3011 | ` */` |
|    226 | 3012 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3013 | `{` |
|      - | 3014 | `	const char *zString;` |
|      - | 3015 | `	int nLen,c;` |
|      - | 3016 | `	/* PHP requires exactly one argument. */` |
|    229 | 3017 | `	if( nArg != 1 ){` |
|      4 | 3018 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3019 | `			"ArgumentCountError",` |
|      - | 3020 | `			"ord() expects exactly 1 argument, %d given",` |
|      1 | 3021 | `			nArg` |
|      - | 3022 | `			);` |
|      - | 3023 | `	}` |
|      - | 3024 | `	/* Passing null is deprecated (E_DEPRECATED).  PHP emits this before` |
|      - | 3025 | `	 * the empty-string deprecation, so we check null first. */` |
|    226 | 3026 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 3027 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 3028 | `			"ord(): Passing null to parameter #1 ($character) "` |
|      - | 3029 | `			"of type string is deprecated"` |
|      - | 3030 | `			);` |
|      1 | 3031 | `	}` |
|      - | 3032 | `	/* Extract the target string */` |
|    226 | 3033 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    226 | 3034 | `	if( nLen < 1 ){` |
|      - | 3035 | `		/* Empty string is deprecated (E_DEPRECATED). */` |
|      5 | 3036 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 3037 | `			"ord(): Providing an empty string is deprecated"` |
|      - | 3038 | `			);` |
|      5 | 3039 | `		ph7_result_int(pCtx,0);` |
|      5 | 3040 | `		return PH7_OK;` |
|      - | 3041 | `	}` |
|      - | 3042 | `	/* A string longer than one byte is deprecated (E_DEPRECATED). */` |
|    222 | 3043 | `	if( nLen > 1 ){` |
|      7 | 3044 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 3045 | `			"ord(): Providing a string that is not one byte long "` |
|      - | 3046 | `			"is deprecated. Use ord($str[0]) instead"` |
|      - | 3047 | `			);` |
|      3 | 3048 | `	}` |
|      - | 3049 | `	/* Extract the ASCII value of the first character */` |
|    222 | 3050 | `	c = (unsigned char)zString[0];` |
|      - | 3051 | `	/* Return that value */` |
|    222 | 3052 | `	ph7_result_int(pCtx,c);` |
|    222 | 3053 | `	return PH7_OK;` |
|    116 | 3054 | `}` |
|      - | 3055 | `/*` |
|      - | 3056 | ` * string chr(int $codepoint)` |
|      - | 3057 | ` *  Returns a one-character string containing the character specified` |
|      - | 3058 | ` *  by the given codepoint.  Any integer is accepted; values outside` |
|      - | 3059 | ` *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.` |
|      - | 3060 | ` * Parameters` |
|      - | 3061 | ` *  $codepoint` |
|      - | 3062 | ` *   An integer codepoint.  Values outside 0-255 are deprecated and` |
|      - | 3063 | ` *   will be constrained to a single byte.` |
|      - | 3064 | ` * Returns` |
|      - | 3065 | ` *  A single-character string.` |
|      - | 3066 | ` */` |
|   7170 | 3067 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3068 | `{` |
|      - | 3069 | `	int c;` |
|      - | 3070 | `	unsigned char ch;` |
|      - | 3071 | `	/* PHP requires exactly one argument. */` |
|   7173 | 3072 | `	if( nArg != 1 ){` |
|      4 | 3073 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3074 | `			"ArgumentCountError",` |
|      - | 3075 | `			"chr() expects exactly 1 argument, %d given",` |
|      1 | 3076 | `			nArg` |
|      - | 3077 | `			);` |
|      - | 3078 | `	}` |
|      - | 3079 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 3080 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 3081 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 3082 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|   7171 | 3083 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      - | 3084 | `		char zBuf[120];` |
|      4 | 3085 | `		SyBufferFormat(zBuf,sizeof(zBuf),` |
|      - | 3086 | `			"Implicit conversion from float %g to int loses precision",` |
|      1 | 3087 | `			ph7_value_to_double(apArg[0])` |
|      - | 3088 | `			);` |
|      3 | 3089 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zBuf);` |
|      1 | 3090 | `	}` |
|      - | 3091 | `	/* Extract the codepoint. */` |
|   7171 | 3092 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 3093 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 3094 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 3095 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 3096 | `	 * name to avoid the API double-prefixing it. */` |
|   7171 | 3097 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 3098 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 3099 | `			E_DEPRECATED,` |
|      - | 3100 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 3101 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 3102 | `			"The value used will be constrained using % 256"` |
|      - | 3103 | `			);` |
|      2 | 3104 | `	}` |
|      - | 3105 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 3106 | `	 * when taking the address of a wider int. */` |
|   7171 | 3107 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 3108 | `	/* Return the specified character */` |
|   7171 | 3109 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|   7171 | 3110 | `	return PH7_OK;` |
|   3588 | 3111 | `}` |
|      - | 3112 | `/*` |
|      - | 3113 | ` * Binary to hex consumer callback.` |
|      - | 3114 | ` * This callback is the default consumer used by the hash functions` |
|      - | 3115 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 3116 | ` */` |
|   3158 | 3117 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      3 | 3118 | `{` |
|      - | 3119 | `	/* Append hex chunk verbatim */` |
|   3161 | 3120 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   3161 | 3121 | `	return SXRET_OK;` |
|      3 | 3122 | `}` |
|      - | 3123 |  |
|      - | 3124 | `/*` |
|      - | 3125 | ` * string bin2hex(string $str)` |
|      - | 3126 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 3127 | ` * Parameters` |
|      - | 3128 | ` *  $str` |
|      - | 3129 | ` *   The input string.` |
|      - | 3130 | ` * Returns.` |
|      - | 3131 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 3132 | ` */` |
|    144 | 3133 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3134 | `{` |
|      - | 3135 | `	const char *zString;` |
|      - | 3136 | `	int nLen;` |
|      - | 3137 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|    147 | 3138 | `	if( nArg != 1 ){` |
|      4 | 3139 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3140 | `			"ArgumentCountError",` |
|      - | 3141 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|      1 | 3142 | `			nArg` |
|      - | 3143 | `			);` |
|      - | 3144 | `	}` |
|      - | 3145 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 3146 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 3147 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 3148 | `	 */` |
|    216 | 3149 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|     71 | 3150 | `		( ph7_value_is_object(apArg[0]) &&` |
|    ! 0 | 3151 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|    ! 0 | 3152 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|    ! 0 | 3153 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 3154 | `		)` |
|      - | 3155 | `	){` |
|    ! 0 | 3156 | `		const char *zType = ph7_type_name(apArg[0]);` |
|    ! 0 | 3157 | `		if( ph7_value_is_object(apArg[0]) ){` |
|    ! 0 | 3158 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    ! 0 | 3159 | `			if( pInst && pInst->pClass ){` |
|    ! 0 | 3160 | `				zType = SyStringData(&pInst->pClass->sName);` |
|    ! 0 | 3161 | `			}` |
|    ! 0 | 3162 | `		}` |
|    ! 0 | 3163 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3164 | `			"TypeError",` |
|      - | 3165 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 3166 | `			zType` |
|      - | 3167 | `			);` |
|      - | 3168 | `	}` |
|      - | 3169 | `	/* Extract the target string */` |
|    145 | 3170 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    145 | 3171 | `	if( nLen < 1 ){` |
|      - | 3172 | `		/* Empty string,return */` |
|     13 | 3173 | `		ph7_result_string(pCtx,"",0);` |
|     13 | 3174 | `		return PH7_OK;` |
|      - | 3175 | `	}` |
|      - | 3176 | `	/* Perform the requested operation */` |
|    133 | 3177 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|    133 | 3178 | `	return PH7_OK;` |
|     75 | 3179 | `}` |
|      - | 3180 |  |
|      - | 3181 | `/* Search callback signature */` |
|      - | 3182 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 3183 | `/*` |
|      - | 3184 | ` * Case-insensitive pattern match.` |
|      - | 3185 | ` * Brute force is the default search method used here.` |
|      - | 3186 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 3187 | ` * well for short/medium texts on modern hardware.` |
|      - | 3188 | ` */` |
|    298 | 3189 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      2 | 3190 | `{` |
|    300 | 3191 | `	const char *zpIn = (const char *)pPattern;` |
|    300 | 3192 | `	const char *zIn = (const char *)pText;` |
|    300 | 3193 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    300 | 3194 | `	const char *zEnd = &zIn[nLen];` |
|      - | 3195 | `	const char *zPtr,*zPtr2;` |
|      - | 3196 | `	int c,d;` |
|    300 | 3197 | `	if( iPatLen > nLen ){` |
|      - | 3198 | `		/* Don't bother processing */` |
|     67 | 3199 | `		return SXERR_NOTFOUND;` |
|      - | 3200 | `	}` |
|    860 | 3201 | `	for(;;){` |
|   1722 | 3202 | `		if( zIn >= zEnd ){` |
|    194 | 3203 | `			break;` |
|      - | 3204 | `		}` |
|   1530 | 3205 | `		c = SyToLower(zIn[0]);` |
|   1530 | 3206 | `		d = SyToLower(zpIn[0]);` |
|   1530 | 3207 | `		if( c == d ){` |
|    182 | 3208 | `			zPtr   = &zIn[1];` |
|    182 | 3209 | `			zPtr2  = &zpIn[1];` |
|    141 | 3210 | `			for(;;){` |
|    284 | 3211 | `				if( zPtr2 >= zpEnd ){` |
|      - | 3212 | `					/* Pattern found */` |
|     41 | 3213 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 3214 | `					return SXRET_OK;` |
|      - | 3215 | `				}` |
|    244 | 3216 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 3217 | `					break;` |
|      - | 3218 | `				}` |
|    244 | 3219 | `				c = SyToLower(zPtr[0]);` |
|    244 | 3220 | `				d = SyToLower(zPtr2[0]);` |
|    244 | 3221 | `				if( c != d ){` |
|    142 | 3222 | `					break;` |
|      - | 3223 | `				}` |
|    103 | 3224 | `				zPtr++; zPtr2++;` |
|      1 | 3225 | `			}` |
|     70 | 3226 | `		}` |
|   1490 | 3227 | `		zIn++;` |
|      2 | 3228 | `	}` |
|      - | 3229 | `	/* Pattern not found */` |
|    194 | 3230 | `	return SXERR_NOTFOUND;` |
|    151 | 3231 | `}` |
|      - | 3232 | `/*` |
|      - | 3233 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3234 | ` *  Find the first occurrence of a string.` |
|      - | 3235 | ` * Parameters` |
|      - | 3236 | ` *  $haystack` |
|      - | 3237 | ` *   The input string.` |
|      - | 3238 | ` * $needle` |
|      - | 3239 | ` *   Search pattern (must be a string).` |
|      - | 3240 | ` * $before_needle` |
|      - | 3241 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3242 | ` *   of the needle (excluding the needle).` |
|      - | 3243 | ` * Return` |
|      - | 3244 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3245 | ` */` |
|      6 | 3246 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3247 | `{` |
|      7 | 3248 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3249 | `	const char *zBlob,*zPattern;` |
|      - | 3250 | `	int nLen,nPatLen;` |
|      - | 3251 | `	sxu32 nOfft;` |
|      - | 3252 | `	sxi32 rc;` |
|      7 | 3253 | `	if( nArg < 2 ){` |
|      - | 3254 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3255 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3256 | `		return PH7_OK;` |
|      - | 3257 | `	}` |
|      - | 3258 | `	/* Extract the needle and the haystack */` |
|      7 | 3259 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 3260 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 3261 | `	nOfft = 0; /* cc warning */` |
|      9 | 3262 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3263 | `		int before = 0;` |
|      - | 3264 | `		/* Perform the lookup */` |
|      5 | 3265 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3266 | `		if( rc != SXRET_OK ){` |
|      - | 3267 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3268 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3269 | `			return PH7_OK;` |
|      - | 3270 | `		}` |
|      - | 3271 | `		/* Return the portion of the string */` |
|      5 | 3272 | `		if( nArg > 2 ){` |
|      3 | 3273 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3274 | `		}` |
|      5 | 3275 | `		if( before ){` |
|      3 | 3276 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3277 | `		}else{` |
|      3 | 3278 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3279 | `		}` |
|      3 | 3280 | `	}else{` |
|      3 | 3281 | `		ph7_result_bool(pCtx,0);` |
|      - | 3282 | `	}` |
|      7 | 3283 | `	return PH7_OK;` |
|      4 | 3284 | `}` |
|      - | 3285 | `/*` |
|      - | 3286 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3287 | ` *  Case-insensitive strstr().` |
|      - | 3288 | ` * Parameters` |
|      - | 3289 | ` *  $haystack` |
|      - | 3290 | ` *   The input string.` |
|      - | 3291 | ` * $needle` |
|      - | 3292 | ` *   Search pattern (must be a string).` |
|      - | 3293 | ` * $before_needle` |
|      - | 3294 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3295 | ` *   of the needle (excluding the needle).` |
|      - | 3296 | ` * Return` |
|      - | 3297 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3298 | ` */` |
|      4 | 3299 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3300 | `{` |
|      5 | 3301 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3302 | `	const char *zBlob,*zPattern;` |
|      - | 3303 | `	int nLen,nPatLen;` |
|      - | 3304 | `	sxu32 nOfft;` |
|      - | 3305 | `	sxi32 rc;` |
|      5 | 3306 | `	if( nArg < 2 ){` |
|      - | 3307 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3308 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3309 | `		return PH7_OK;` |
|      - | 3310 | `	}` |
|      - | 3311 | `	/* Extract the needle and the haystack */` |
|      5 | 3312 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3313 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 3314 | `	nOfft = 0; /* cc warning */` |
|      7 | 3315 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3316 | `		int before = 0;` |
|      - | 3317 | `		/* Perform the lookup */` |
|      5 | 3318 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3319 | `		if( rc != SXRET_OK ){` |
|      - | 3320 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3321 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3322 | `			return PH7_OK;` |
|      - | 3323 | `		}` |
|      - | 3324 | `		/* Return the portion of the string */` |
|      5 | 3325 | `		if( nArg > 2 ){` |
|      3 | 3326 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3327 | `		}` |
|      5 | 3328 | `		if( before ){` |
|      3 | 3329 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3330 | `		}else{` |
|      3 | 3331 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3332 | `		}` |
|      3 | 3333 | `	}else{` |
|    ! 0 | 3334 | `		ph7_result_bool(pCtx,0);` |
|      - | 3335 | `	}` |
|      5 | 3336 | `	return PH7_OK;` |
|      3 | 3337 | `}` |
|      - | 3338 | `/*` |
|      - | 3339 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3340 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 3341 | ` * Parameters` |
|      - | 3342 | ` *  $haystack` |
|      - | 3343 | ` *   The input string.` |
|      - | 3344 | ` * $needle` |
|      - | 3345 | ` *   Search pattern (must be a string).` |
|      - | 3346 | ` * $offset` |
|      - | 3347 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3348 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3349 | ` *   of haystack.` |
|      - | 3350 | ` * Return` |
|      - | 3351 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3352 | ` */` |
|   1540 | 3353 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3354 | `{` |
|   1545 | 3355 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strpos",1,"$haystack"); }` |
|   1545 | 3356 | `	if( nArg > 1 ){ StrNullArgNotice(pCtx,apArg[1],"strpos",2,"$needle"); }` |
|   1545 | 3357 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3358 | `	const char *zBlob,*zPattern;` |
|      - | 3359 | `	int nLen,nPatLen,nStart;` |
|      - | 3360 | `	sxu32 nOfft;` |
|      - | 3361 | `	sxi32 rc;` |
|   1545 | 3362 | `	if( nArg < 2 ){` |
|      - | 3363 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3364 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3365 | `		return PH7_OK;` |
|      - | 3366 | `	}` |
|      - | 3367 | `	/* Extract the needle and the haystack */` |
|   1545 | 3368 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|   1545 | 3369 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|   1545 | 3370 | `	nOfft = 0; /* cc warning */` |
|   1545 | 3371 | `	nStart = 0;` |
|      - | 3372 | `	/* Peek the starting offset if available */` |
|   1545 | 3373 | `	if( nArg > 2 ){` |
|     15 | 3374 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3375 | `		if( nStart < 0 ){` |
|    ! 0 | 3376 | `			nStart = -nStart;` |
|    ! 0 | 3377 | `		}` |
|     15 | 3378 | `		if( nStart >= nLen ){` |
|      - | 3379 | `			/* Invalid offset */` |
|    ! 0 | 3380 | `			nStart = 0;` |
|    ! 0 | 3381 | `		}else{` |
|     15 | 3382 | `			zBlob += nStart;` |
|     15 | 3383 | `			nLen -= nStart;` |
|      - | 3384 | `		}` |
|      7 | 3385 | `	}` |
|   1545 | 3386 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3387 | `		/* Perform the lookup */` |
|   1543 | 3388 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|   1543 | 3389 | `		if( rc != SXRET_OK ){` |
|      - | 3390 | `			/* Pattern not found,return FALSE */` |
|    799 | 3391 | `			ph7_result_bool(pCtx,0);` |
|    799 | 3392 | `			return PH7_OK;` |
|      - | 3393 | `		}` |
|      - | 3394 | `		/* Return the pattern position */` |
|    748 | 3395 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|    376 | 3396 | `	}else{` |
|      3 | 3397 | `		ph7_result_bool(pCtx,0);` |
|      - | 3398 | `	}` |
|    750 | 3399 | `	return PH7_OK;` |
|    775 | 3400 | `}` |
|      - | 3401 | `/*` |
|      - | 3402 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|      - | 3403 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|      - | 3404 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|      - | 3405 | ` * TypeError for arrays, resources, and objects without __toString.` |
|      - | 3406 | ` *` |
|      - | 3407 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|      - | 3408 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|      - | 3409 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|      - | 3410 | ` *` |
|      - | 3411 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|      - | 3412 | ` * is valid until pTmp is released or pArg is mutated.` |
|      - | 3413 | ` */` |
|    668 | 3414 | `static sxi32 StrPredicateResolveArg(` |
|      - | 3415 | `	ph7_context *pCtx,` |
|      - | 3416 | `	ph7_value *pArg,` |
|      - | 3417 | `	const char *zFunc,` |
|      - | 3418 | `	int iArgNum,` |
|      - | 3419 | `	const char *zParamName,` |
|      - | 3420 | `	const char *zTypeStr, /* Declared type in the TypeError, e.g. "string" / "?string" */` |
|      - | 3421 | `	const char *zNullMsg,` |
|      - | 3422 | `	ph7_value *pTmp,` |
|      - | 3423 | `	const char **pzOut,` |
|      - | 3424 | `	int *pnOut` |
|      2 | 3425 | `){` |
|    670 | 3426 | `	if( ph7_value_is_null(pArg) ){` |
|     13 | 3427 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zNullMsg);` |
|     13 | 3428 | `		*pzOut = "";` |
|     13 | 3429 | `		*pnOut = 0;` |
|     13 | 3430 | `		return PH7_OK;` |
|      - | 3431 | `	}` |
|   1010 | 3432 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|    656 | 3433 | `	    ( ph7_value_is_object(pArg) &&` |
|     72 | 3434 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|     48 | 3435 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|     24 | 3436 | `	        "__toString",sizeof("__toString")-1) == 0` |
|      - | 3437 | `	    )` |
|      - | 3438 | `	){` |
|    ! 0 | 3439 | `		const char *zType = ph7_type_name(pArg);` |
|    ! 0 | 3440 | `		if( ph7_value_is_object(pArg) ){` |
|    ! 0 | 3441 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|    ! 0 | 3442 | `			if( pInst && pInst->pClass ){` |
|    ! 0 | 3443 | `				zType = SyStringData(&pInst->pClass->sName);` |
|    ! 0 | 3444 | `			}` |
|    ! 0 | 3445 | `		}` |
|    ! 0 | 3446 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3447 | `			"TypeError",` |
|      - | 3448 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|    ! 0 | 3449 | `			zFunc, iArgNum, zParamName, zTypeStr, zType` |
|      - | 3450 | `			);` |
|      - | 3451 | `	}` |
|    658 | 3452 | `	if( ph7_value_is_object(pArg) ){` |
|     49 | 3453 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     49 | 3454 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 3455 | `			"__toString",sizeof("__toString")-1);` |
|     49 | 3456 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|     49 | 3457 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|     49 | 3458 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|     49 | 3459 | `		return PH7_OK;` |
|      - | 3460 | `	}` |
|    610 | 3461 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|    610 | 3462 | `	return PH7_OK;` |
|    336 | 3463 | `}` |
|      - | 3464 | `/*` |
|      - | 3465 | ` * bool str_contains(string $haystack, string $needle)` |
|      - | 3466 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|      - | 3467 | ` * Return` |
|      - | 3468 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|      - | 3469 | ` */` |
|     92 | 3470 | `static int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3471 | `{` |
|      - | 3472 | `	const char *zHaystack,*zNeedle;` |
|      - | 3473 | `	int nHayLen,nNeedleLen;` |
|      - | 3474 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3475 | `	sxi32 rc;` |
|     95 | 3476 | `	if( nArg != 2 ){` |
|      8 | 3477 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3478 | `			"ArgumentCountError",` |
|      - | 3479 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|      2 | 3480 | `			nArg` |
|      - | 3481 | `			);` |
|      - | 3482 | `	}` |
|     90 | 3483 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     90 | 3484 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     90 | 3485 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack","string",` |
|      - | 3486 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|      - | 3487 | `		"of type string is deprecated",` |
|      - | 3488 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     90 | 3489 | `	if( rc != PH7_OK ) goto out;` |
|     90 | 3490 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle","string",` |
|      - | 3491 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|      - | 3492 | `		"of type string is deprecated",` |
|      - | 3493 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     90 | 3494 | `	if( rc != PH7_OK ) goto out;` |
|     90 | 3495 | `	if( nNeedleLen < 1 ){` |
|     13 | 3496 | `		ph7_result_bool(pCtx,1);` |
|     84 | 3497 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3498 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3499 | `	}else{` |
|    104 | 3500 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|     34 | 3501 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|     70 | 3502 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|      - | 3503 | `	}` |
|     90 | 3504 | `	rc = PH7_OK;` |
|     44 | 3505 | `out:` |
|     90 | 3506 | `	PH7_MemObjRelease(&sHayTmp);` |
|     90 | 3507 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     90 | 3508 | `	return rc;` |
|     49 | 3509 | `}` |
|      - | 3510 | `/*` |
|      - | 3511 | ` * bool str_starts_with(string $haystack, string $needle)` |
|      - | 3512 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|      - | 3513 | ` * Return` |
|      - | 3514 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|      - | 3515 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3516 | ` */` |
|     62 | 3517 | `static int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3518 | `{` |
|      - | 3519 | `	const char *zHaystack,*zNeedle;` |
|      - | 3520 | `	int nHayLen,nNeedleLen;` |
|      - | 3521 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3522 | `	sxi32 rc;` |
|     64 | 3523 | `	if( nArg != 2 ){` |
|      8 | 3524 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3525 | `			"ArgumentCountError",` |
|      - | 3526 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|      2 | 3527 | `			nArg` |
|      - | 3528 | `			);` |
|      - | 3529 | `	}` |
|     59 | 3530 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     59 | 3531 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     59 | 3532 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack","string",` |
|      - | 3533 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3534 | `		"of type string is deprecated",` |
|      - | 3535 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     59 | 3536 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3537 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle","string",` |
|      - | 3538 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3539 | `		"of type string is deprecated",` |
|      - | 3540 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     59 | 3541 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3542 | `	if( nNeedleLen < 1 ){` |
|     13 | 3543 | `		ph7_result_bool(pCtx,1);` |
|     53 | 3544 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3545 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3546 | `	}else{` |
|     58 | 3547 | `		ph7_result_bool(pCtx,` |
|     38 | 3548 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3549 | `	}` |
|     59 | 3550 | `	rc = PH7_OK;` |
|     29 | 3551 | `out:` |
|     59 | 3552 | `	PH7_MemObjRelease(&sHayTmp);` |
|     59 | 3553 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     59 | 3554 | `	return rc;` |
|     33 | 3555 | `}` |
|      - | 3556 | `/*` |
|      - | 3557 | ` * bool str_ends_with(string $haystack, string $needle)` |
|      - | 3558 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|      - | 3559 | ` * Return` |
|      - | 3560 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|      - | 3561 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3562 | ` */` |
|     62 | 3563 | `static int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3564 | `{` |
|      - | 3565 | `	const char *zHaystack,*zNeedle;` |
|      - | 3566 | `	int nHayLen,nNeedleLen;` |
|      - | 3567 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3568 | `	sxi32 rc;` |
|     64 | 3569 | `	if( nArg != 2 ){` |
|      8 | 3570 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3571 | `			"ArgumentCountError",` |
|      - | 3572 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|      2 | 3573 | `			nArg` |
|      - | 3574 | `			);` |
|      - | 3575 | `	}` |
|     59 | 3576 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     59 | 3577 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     59 | 3578 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack","string",` |
|      - | 3579 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3580 | `		"of type string is deprecated",` |
|      - | 3581 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     59 | 3582 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3583 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle","string",` |
|      - | 3584 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3585 | `		"of type string is deprecated",` |
|      - | 3586 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     59 | 3587 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3588 | `	if( nNeedleLen < 1 ){` |
|     13 | 3589 | `		ph7_result_bool(pCtx,1);` |
|     53 | 3590 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3591 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3592 | `	}else{` |
|     58 | 3593 | `		ph7_result_bool(pCtx,` |
|     38 | 3594 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3595 | `	}` |
|     59 | 3596 | `	rc = PH7_OK;` |
|     29 | 3597 | `out:` |
|     59 | 3598 | `	PH7_MemObjRelease(&sHayTmp);` |
|     59 | 3599 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     59 | 3600 | `	return rc;` |
|     33 | 3601 | `}` |
|      - | 3602 | `/*` |
|      - | 3603 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3604 | ` *  Case-insensitive strpos.` |
|      - | 3605 | ` * Parameters` |
|      - | 3606 | ` *  $haystack` |
|      - | 3607 | ` *   The input string.` |
|      - | 3608 | ` * $needle` |
|      - | 3609 | ` *   Search pattern (must be a string).` |
|      - | 3610 | ` * $offset` |
|      - | 3611 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3612 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3613 | ` *   of haystack.` |
|      - | 3614 | ` * Return` |
|      - | 3615 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3616 | ` */` |
|    196 | 3617 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3618 | `{` |
|    198 | 3619 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3620 | `	const char *zBlob,*zPattern;` |
|      - | 3621 | `	int nLen,nPatLen,nStart;` |
|      - | 3622 | `	sxu32 nOfft;` |
|      - | 3623 | `	sxi32 rc;` |
|    198 | 3624 | `	if( nArg < 2 ){` |
|      - | 3625 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3626 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3627 | `		return PH7_OK;` |
|      - | 3628 | `	}` |
|      - | 3629 | `	/* Extract the needle and the haystack */` |
|    198 | 3630 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    198 | 3631 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    198 | 3632 | `	nOfft = 0; /* cc warning */` |
|    198 | 3633 | `	nStart = 0;` |
|      - | 3634 | `	/* Peek the starting offset if available */` |
|    198 | 3635 | `	if( nArg > 2 ){` |
|      5 | 3636 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 3637 | `		if( nStart < 0 ){` |
|      3 | 3638 | `			nStart = -nStart;` |
|      1 | 3639 | `		}` |
|      5 | 3640 | `		if( nStart >= nLen ){` |
|      - | 3641 | `			/* Invalid offset */` |
|    ! 0 | 3642 | `			nStart = 0;` |
|    ! 0 | 3643 | `		}else{` |
|      5 | 3644 | `			zBlob += nStart;` |
|      5 | 3645 | `			nLen -= nStart;` |
|      - | 3646 | `		}` |
|      2 | 3647 | `	}` |
|    198 | 3648 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3649 | `		/* Perform the lookup */` |
|    198 | 3650 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    198 | 3651 | `		if( rc != SXRET_OK ){` |
|      - | 3652 | `			/* Pattern not found,return FALSE */` |
|    184 | 3653 | `			ph7_result_bool(pCtx,0);` |
|    184 | 3654 | `			return PH7_OK;` |
|      - | 3655 | `		}` |
|      - | 3656 | `		/* Return the pattern position */` |
|     15 | 3657 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 3658 | `	}else{` |
|    ! 0 | 3659 | `		ph7_result_bool(pCtx,0);` |
|      - | 3660 | `	}` |
|     15 | 3661 | `	return PH7_OK;` |
|    100 | 3662 | `}` |
|      - | 3663 | `/*` |
|      - | 3664 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3665 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 3666 | ` * Parameters` |
|      - | 3667 | ` *  $haystack` |
|      - | 3668 | ` *   The input string.` |
|      - | 3669 | ` * $needle` |
|      - | 3670 | ` *   Search pattern (must be a string).` |
|      - | 3671 | ` * $offset` |
|      - | 3672 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3673 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3674 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3675 | ` * Return` |
|      - | 3676 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3677 | ` */` |
|     40 | 3678 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3679 | `{` |
|      - | 3680 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     41 | 3681 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3682 | `	int nLen,nPatLen;` |
|      - | 3683 | `	sxu32 nOfft;` |
|      - | 3684 | `	sxi32 rc;` |
|     41 | 3685 | `	if( nArg < 2 ){` |
|      - | 3686 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3687 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3688 | `		return PH7_OK;` |
|      - | 3689 | `	}` |
|      - | 3690 | `	/* Extract the needle and the haystack */` |
|     41 | 3691 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 3692 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3693 | `	/* Point to the end of the pattern */` |
|     41 | 3694 | `	zPtr = &zBlob[nLen - 1];` |
|     41 | 3695 | `	zEnd = &zBlob[nLen];` |
|      - | 3696 | `	/* Save the starting posistion */` |
|     41 | 3697 | `	zStart = zBlob;` |
|     41 | 3698 | `	nOfft = 0; /* cc warning */` |
|      - | 3699 | `	/* Peek the starting offset if available */` |
|     41 | 3700 | `	if( nArg > 2 ){` |
|      - | 3701 | `		int nStart;` |
|     21 | 3702 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 3703 | `		if( nStart < 0 ){` |
|     11 | 3704 | `			nStart = -nStart;` |
|     11 | 3705 | `			if( nStart >= nLen ){` |
|      - | 3706 | `				/* Invalid offset */` |
|      3 | 3707 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3708 | `				return PH7_OK;` |
|    ! 0 | 3709 | `			}else{` |
|      9 | 3710 | `				nLen -= nStart;` |
|      9 | 3711 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 3712 | `				zEnd = &zBlob[nLen];` |
|      - | 3713 | `			}` |
|      5 | 3714 | `		}else{` |
|     11 | 3715 | `			if( nStart >= nLen ){` |
|      - | 3716 | `				/* Invalid offset */` |
|      5 | 3717 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3718 | `				return PH7_OK;` |
|    ! 0 | 3719 | `			}else{` |
|      7 | 3720 | `				zBlob += nStart;` |
|      7 | 3721 | `				nLen -= nStart;` |
|      - | 3722 | `			}` |
|      - | 3723 | `		}` |
|      7 | 3724 | `	}` |
|     35 | 3725 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3726 | `		/* Perform the lookup */` |
|    121 | 3727 | `		for(;;){` |
|    243 | 3728 | `			if( zBlob >= zPtr ){` |
|     21 | 3729 | `				break;` |
|      - | 3730 | `			}` |
|    223 | 3731 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    223 | 3732 | `			if( rc == SXRET_OK ){` |
|      - | 3733 | `				/* Pattern found,return it's position */` |
|     13 | 3734 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 3735 | `				return PH7_OK;` |
|      - | 3736 | `			}` |
|    211 | 3737 | `			zPtr--;` |
|      1 | 3738 | `		}` |
|      - | 3739 | `		/* Pattern not found,return FALSE */` |
|     21 | 3740 | `		ph7_result_bool(pCtx,0);` |
|     11 | 3741 | `	}else{` |
|      3 | 3742 | `		ph7_result_bool(pCtx,0);` |
|      - | 3743 | `	}` |
|     23 | 3744 | `	return PH7_OK;` |
|     21 | 3745 | `}` |
|      - | 3746 | `/*` |
|      - | 3747 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3748 | ` *  Case-insensitive strrpos.` |
|      - | 3749 | ` * Parameters` |
|      - | 3750 | ` *  $haystack` |
|      - | 3751 | ` *   The input string.` |
|      - | 3752 | ` * $needle` |
|      - | 3753 | ` *   Search pattern (must be a string).` |
|      - | 3754 | ` * $offset` |
|      - | 3755 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3756 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3757 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3758 | ` * Return` |
|      - | 3759 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3760 | ` */` |
|     26 | 3761 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3762 | `{` |
|      - | 3763 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     27 | 3764 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3765 | `	int nLen,nPatLen;` |
|      - | 3766 | `	sxu32 nOfft;` |
|      - | 3767 | `	sxi32 rc;` |
|     27 | 3768 | `	if( nArg < 2 ){` |
|      - | 3769 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3770 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3771 | `		return PH7_OK;` |
|      - | 3772 | `	}` |
|      - | 3773 | `	/* Extract the needle and the haystack */` |
|     27 | 3774 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 3775 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3776 | `	/* Point to the end of the pattern */` |
|     27 | 3777 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 3778 | `	zEnd = &zBlob[nLen];` |
|      - | 3779 | `	/* Save the starting posistion */` |
|     27 | 3780 | `	zStart = zBlob;` |
|     27 | 3781 | `	nOfft = 0; /* cc warning */` |
|      - | 3782 | `	/* Peek the starting offset if available */` |
|     27 | 3783 | `	if( nArg > 2 ){` |
|      - | 3784 | `		int nStart;` |
|     15 | 3785 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3786 | `		if( nStart < 0 ){` |
|      7 | 3787 | `			nStart = -nStart;` |
|      7 | 3788 | `			if( nStart >= nLen ){` |
|      - | 3789 | `				/* Invalid offset */` |
|      3 | 3790 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3791 | `				return PH7_OK;` |
|    ! 0 | 3792 | `			}else{` |
|      5 | 3793 | `				nLen -= nStart;` |
|      5 | 3794 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 3795 | `				zEnd = &zBlob[nLen];` |
|      - | 3796 | `			}` |
|      3 | 3797 | `		}else{` |
|      9 | 3798 | `			if( nStart >= nLen ){` |
|      - | 3799 | `				/* Invalid offset */` |
|      5 | 3800 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3801 | `				return PH7_OK;` |
|    ! 0 | 3802 | `			}else{` |
|      5 | 3803 | `				zBlob += nStart;` |
|      5 | 3804 | `				nLen -= nStart;` |
|      - | 3805 | `			}` |
|      - | 3806 | `		}` |
|      4 | 3807 | `	}` |
|     21 | 3808 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3809 | `		/* Perform the lookup */` |
|     44 | 3810 | `		for(;;){` |
|     89 | 3811 | `			if( zBlob >= zPtr ){` |
|      9 | 3812 | `				break;` |
|      - | 3813 | `			}` |
|     81 | 3814 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 3815 | `			if( rc == SXRET_OK ){` |
|      - | 3816 | `				/* Pattern found,return it's position */` |
|     11 | 3817 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 3818 | `				return PH7_OK;` |
|      - | 3819 | `			}` |
|     71 | 3820 | `			zPtr--;` |
|      1 | 3821 | `		}` |
|      - | 3822 | `		/* Pattern not found,return FALSE */` |
|      9 | 3823 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3824 | `	}else{` |
|      3 | 3825 | `		ph7_result_bool(pCtx,0);` |
|      - | 3826 | `	}` |
|     11 | 3827 | `	return PH7_OK;` |
|     14 | 3828 | `}` |
|      - | 3829 | `/*` |
|      - | 3830 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3831 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3832 | ` * Parameters` |
|      - | 3833 | ` *  $haystack` |
|      - | 3834 | ` *   The input string.` |
|      - | 3835 | ` * $needle` |
|      - | 3836 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3837 | ` *  This behavior is different from that of strstr().` |
|      - | 3838 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3839 | ` *  as the ordinal value of a character.` |
|      - | 3840 | ` * Return` |
|      - | 3841 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3842 | ` */` |
|     22 | 3843 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3844 | `{` |
|      - | 3845 | `	const char *zBlob;` |
|      - | 3846 | `	int nLen,c;` |
|     23 | 3847 | `	if( nArg < 2 ){` |
|      - | 3848 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3849 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3850 | `		return PH7_OK;` |
|      - | 3851 | `	}` |
|      - | 3852 | `	/* Extract the haystack */` |
|     23 | 3853 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3854 | `	c = 0; /* cc warning */` |
|     23 | 3855 | `	if( nLen > 0 ){` |
|      - | 3856 | `		sxu32 nOfft;` |
|      - | 3857 | `		sxi32 rc;` |
|     21 | 3858 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 3859 | `			const char *zPattern;` |
|     11 | 3860 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 3861 | `														 * for NULL pointer.` |
|      - | 3862 | `														 */` |
|     11 | 3863 | `			c = zPattern[0];` |
|      6 | 3864 | `		}else{` |
|      - | 3865 | `			/* Int cast */` |
|     11 | 3866 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 3867 | `		}` |
|      - | 3868 | `		/* Perform the lookup */` |
|     21 | 3869 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3870 | `		if( rc != SXRET_OK ){` |
|      - | 3871 | `			/* No such entry,return FALSE */` |
|      7 | 3872 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3873 | `			return PH7_OK;` |
|      - | 3874 | `		}` |
|      - | 3875 | `		/* Return the string portion */` |
|     15 | 3876 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 3877 | `	}else{` |
|      3 | 3878 | `		ph7_result_bool(pCtx,0);` |
|      - | 3879 | `	}` |
|     17 | 3880 | `	return PH7_OK;` |
|     12 | 3881 | `}` |
|      - | 3882 | `/*` |
|      - | 3883 | ` * string strrev(string $string)` |
|      - | 3884 | ` *  Reverse a string.` |
|      - | 3885 | ` * Parameters` |
|      - | 3886 | ` *  $string` |
|      - | 3887 | ` *   String to be reversed.` |
|      - | 3888 | ` * Return` |
|      - | 3889 | ` *  The reversed string.` |
|      - | 3890 | ` */` |
|      2 | 3891 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3892 | `{` |
|      - | 3893 | `	const char *zIn,*zEnd;` |
|      - | 3894 | `	int nLen,c;` |
|      3 | 3895 | `	if( nArg < 1 ){` |
|      - | 3896 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3897 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3898 | `		return PH7_OK;` |
|      - | 3899 | `	}` |
|      - | 3900 | `	/* Extract the target string */` |
|      3 | 3901 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3902 | `	if( nLen < 1 ){` |
|      - | 3903 | `		/* Empty string Return null */` |
|    ! 0 | 3904 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3905 | `		return PH7_OK;` |
|      - | 3906 | `	}` |
|      - | 3907 | `	/* Perform the requested operation */` |
|      3 | 3908 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3909 | `	for(;;){` |
|      9 | 3910 | `		if( zEnd < zIn ){` |
|      - | 3911 | `			/* No more input to process */` |
|      3 | 3912 | `			break;` |
|      - | 3913 | `		}` |
|      - | 3914 | `		/* Append current character */` |
|      7 | 3915 | `		c = zEnd[0];` |
|      7 | 3916 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3917 | `		zEnd--;` |
|      1 | 3918 | `	}` |
|      3 | 3919 | `	return PH7_OK;` |
|      2 | 3920 | `}` |
|      - | 3921 | `/*` |
|      - | 3922 | ` * string ucwords(string $string [, string $separators = " \t\r\n\f\v"])` |
|      - | 3923 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3924 | ` *  A word begins at the start of the string and after any character present in` |
|      - | 3925 | ` *  $separators. The default separators are the whitespace characters (space,` |
|      - | 3926 | ` *  horizontal tab, carriage return, newline, form-feed and vertical tab); an` |
|      - | 3927 | ` *  explicit $separators argument REPLACES them (an empty string leaves only the` |
|      - | 3928 | ` *  very first character upper-cased). Like PHP, this is byte-based: only ASCII` |
|      - | 3929 | ` *  bytes are upper-cased and a byte is a separator only if it appears in the set.` |
|      - | 3930 | ` * Parameters` |
|      - | 3931 | ` *  $string` |
|      - | 3932 | ` *   The input string.` |
|      - | 3933 | ` *  $separators` |
|      - | 3934 | ` *   The optional word-boundary characters.` |
|      - | 3935 | ` * Return` |
|      - | 3936 | ` *  The modified string.` |
|      - | 3937 | ` */` |
|     22 | 3938 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3939 | `{` |
|      - | 3940 | `	const char *zIn;` |
|      - | 3941 | `	int nLen,i,iStart;` |
|      - | 3942 | `	char aDelim[256];` |
|     23 | 3943 | `	if( nArg < 1 ){` |
|      - | 3944 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3945 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3946 | `		return PH7_OK;` |
|      - | 3947 | `	}` |
|      - | 3948 | `	/* Build the separator membership table: an explicit $separators argument` |
|      - | 3949 | `	 * replaces the default whitespace set (an empty string clears it). */` |
|     23 | 3950 | `	SyZero(aDelim,(sxu32)sizeof(aDelim));` |
|     23 | 3951 | `	if( nArg > 1 ){` |
|      - | 3952 | `		int nDelim;` |
|      9 | 3953 | `		const char *zDelim = ph7_value_to_string(apArg[1],&nDelim);` |
|     17 | 3954 | `		for( i = 0 ; i < nDelim ; i++ ){` |
|      9 | 3955 | `			aDelim[(unsigned char)zDelim[i]] = 1;` |
|      5 | 3956 | `		}` |
|      5 | 3957 | `	}else{` |
|     15 | 3958 | `		aDelim[(unsigned char)' ']  = 1;` |
|     15 | 3959 | `		aDelim[(unsigned char)'\t'] = 1;` |
|     15 | 3960 | `		aDelim[(unsigned char)'\r'] = 1;` |
|     15 | 3961 | `		aDelim[(unsigned char)'\n'] = 1;` |
|     15 | 3962 | `		aDelim[(unsigned char)'\f'] = 1;` |
|     15 | 3963 | `		aDelim[(unsigned char)'\v'] = 1;` |
|      - | 3964 | `	}` |
|      - | 3965 | `	/* Extract the target string */` |
|     23 | 3966 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3967 | `	if( nLen < 1 ){` |
|      - | 3968 | `		/* Empty string – match PHP semantics */` |
|      3 | 3969 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3970 | `		return PH7_OK;` |
|      - | 3971 | `	}` |
|      - | 3972 | `	/* Upper-case the first byte of each word (the leading byte, or any byte that` |
|      - | 3973 | `	 * follows a separator), appending the untouched runs in between verbatim. */` |
|     21 | 3974 | `	iStart = 0;` |
|    309 | 3975 | `	for( i = 0 ; i < nLen ; i++ ){` |
|    289 | 3976 | `		int c = (unsigned char)zIn[i];` |
|    289 | 3977 | `		if( (i == 0 \|\| aDelim[(unsigned char)zIn[i-1]]) && c < 0x80 && SyisLower(c) ){` |
|     53 | 3978 | `			char up = (char)SyToUpper(c);` |
|     53 | 3979 | `			if( i > iStart ){` |
|     35 | 3980 | `				ph7_result_string(pCtx,&zIn[iStart],i - iStart);` |
|     17 | 3981 | `			}` |
|     53 | 3982 | `			ph7_result_string(pCtx,&up,1);` |
|     53 | 3983 | `			iStart = i + 1;` |
|     26 | 3984 | `		}` |
|    145 | 3985 | `	}` |
|     21 | 3986 | `	if( nLen > iStart ){` |
|     21 | 3987 | `		ph7_result_string(pCtx,&zIn[iStart],nLen - iStart);` |
|     10 | 3988 | `	}` |
|     21 | 3989 | `	return PH7_OK;` |
|     12 | 3990 | `}` |
|      - | 3991 | `/*` |
|      - | 3992 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3993 | ` *  Returns input repeated multiplier times.` |
|      - | 3994 | ` * Parameters` |
|      - | 3995 | ` *  $string` |
|      - | 3996 | ` *   String to be repeated.` |
|      - | 3997 | ` * $multiplier` |
|      - | 3998 | ` *  Number of time the input string should be repeated.` |
|      - | 3999 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 4000 | ` *  to 0, the function will return an empty string.` |
|      - | 4001 | ` * Return` |
|      - | 4002 | ` *  The repeated string.` |
|      - | 4003 | ` */` |
|  20434 | 4004 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4005 | `{` |
|      - | 4006 | `	const char *zIn;` |
|      - | 4007 | `	int nLen;` |
|      - | 4008 | `	ph7_int64 nMul;` |
|      - | 4009 | `	int rc;` |
|  20436 | 4010 | `	if( nArg < 2 ){` |
|      - | 4011 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4012 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4013 | `		return PH7_OK;` |
|      - | 4014 | `	}` |
|      - | 4015 | `	/* Extract the target string */` |
|  20436 | 4016 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4017 | `	/* Resolve $times through the shared ZPP helper so a lossy float / float-string` |
|      - | 4018 | `	 * carries php's precision deprecation and NAN/INF/non-numeric fail with php's` |
|      - | 4019 | `	 * TypeError — a bare ph7_value_to_int64() coerced them silently. */` |
|      - | 4020 | `	{` |
|  20436 | 4021 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_repeat",2,"$times","int",&nMul);` |
|  20436 | 4022 | `		if( rcArg != PH7_OK ){` |
|    ! 0 | 4023 | `			return rcArg;` |
|      - | 4024 | `		}` |
|      - | 4025 | `	}` |
|  20436 | 4026 | `	if( nMul < 0 ){` |
|      3 | 4027 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4028 | `			"str_repeat(): Argument #2 ($times) must be greater than or equal to 0");` |
|      - | 4029 | `	}` |
|  20434 | 4030 | `	if( nLen < 1 \|\| nMul < 1 ){` |
|      - | 4031 | `		/* Empty input or a zero multiplier yields the empty string (PHP). */` |
|    ! 0 | 4032 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4033 | `		return PH7_OK;` |
|      - | 4034 | `	}` |
|      - | 4035 | `	/* Perform the requested operation */` |
| 221930 | 4036 | `	for(;;){` |
| 443862 | 4037 | `		if( !nMul ){` |
|  20434 | 4038 | `			break;` |
|      - | 4039 | `		}` |
|      - | 4040 | `		/* Append the copy */` |
| 423430 | 4041 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 423430 | 4042 | `		if( rc != PH7_OK ){` |
|      - | 4043 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 4044 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 4045 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 4046 | `		}` |
| 423430 | 4047 | `		nMul--;` |
|      2 | 4048 | `	}` |
|  20434 | 4049 | `	return PH7_OK;` |
|  10219 | 4050 | `}` |
|      - | 4051 | `/*` |
|      - | 4052 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 4053 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 4054 | ` * Parameters` |
|      - | 4055 | ` *  $string` |
|      - | 4056 | ` *   The input string.` |
|      - | 4057 | ` * $is_xhtml` |
|      - | 4058 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 4059 | ` * Return` |
|      - | 4060 | ` *  The processed string.` |
|      - | 4061 | ` */` |
|      4 | 4062 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4063 | `{` |
|      - | 4064 | `	const char *zIn,*zCur,*zEnd;` |
|      5 | 4065 | `	int is_xhtml = 1; /* Default to XHTML-style '<br />' like PHP */` |
|      - | 4066 | `	int nLen;` |
|      5 | 4067 | `	if( nArg < 1 ){` |
|      - | 4068 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4069 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4070 | `		return PH7_OK;` |
|      - | 4071 | `	}` |
|      - | 4072 | `	/* Extract the target string */` |
|      5 | 4073 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 4074 | `	if( nLen < 1 ){` |
|      - | 4075 | `		/* Empty string,return null */` |
|    ! 0 | 4076 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4077 | `		return PH7_OK;` |
|      - | 4078 | `	}` |
|      5 | 4079 | `	if( nArg > 1 ){` |
|      3 | 4080 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 4081 | `	}` |
|      5 | 4082 | `	zEnd = &zIn[nLen];` |
|      - | 4083 | `	/* Perform the requested operation */` |
|      4 | 4084 | `	for(;;){` |
|      9 | 4085 | `		zCur = zIn;` |
|      - | 4086 | `		/* Delimit the string */` |
|     21 | 4087 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 4088 | `			zIn++;` |
|      1 | 4089 | `		}` |
|      9 | 4090 | `		if( zCur < zIn ){` |
|      - | 4091 | `			/* Output chunk verbatim */` |
|      9 | 4092 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 4093 | `		}` |
|      9 | 4094 | `		if( zIn >= zEnd ){` |
|      - | 4095 | `			/* No more input to process */` |
|      5 | 4096 | `			break;` |
|      - | 4097 | `		}` |
|      - | 4098 | `		/* Output the HTML line break */` |
|      - | 4099 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br />' (space before the slash), otherwise use '<br>' */` |
|      5 | 4100 | `		if( is_xhtml ){` |
|      3 | 4101 | `			ph7_result_string(pCtx,"<br />",(int)sizeof("<br />")-1);` |
|      2 | 4102 | `		}else{` |
|      3 | 4103 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 4104 | `		}` |
|      5 | 4105 | `		zCur = zIn;` |
|      - | 4106 | `		/* Append trailing line */` |
|     11 | 4107 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 4108 | `			zIn++;` |
|      1 | 4109 | `		}` |
|      5 | 4110 | `		if( zCur < zIn ){` |
|      - | 4111 | `			/* Output chunk verbatim */` |
|      5 | 4112 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 4113 | `		}` |
|      1 | 4114 | `	}` |
|      5 | 4115 | `	return PH7_OK;` |
|      3 | 4116 | `}` |
|      - | 4117 | `/*` |
|      - | 4118 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 4119 | ` *  According to the PHP reference manual.` |
|      - | 4120 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 4121 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 4122 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 4123 | ` * This applies to both sprintf() and printf().` |
|      - | 4124 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 4125 | ` * or more of these elements, in order:` |
|      - | 4126 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 4127 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 4128 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 4129 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 4130 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 4131 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 4132 | ` *   it with a single quote ('). See the examples below.` |
|      - | 4133 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 4134 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 4135 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 4136 | ` *   should result in.` |
|      - | 4137 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 4138 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 4139 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 4140 | ` *   limit to the string.` |
|      - | 4141 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 4142 | ` *       % - a literal percent character. No argument is required.` |
|      - | 4143 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 4144 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 4145 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 4146 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 4147 | ` * 	     for the number of digits after the decimal point.` |
|      - | 4148 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 4149 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 4150 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 4151 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 4152 | ` *       g - shorter of %e and %f.` |
|      - | 4153 | ` *       G - shorter of %E and %f.` |
|      - | 4154 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 4155 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 4156 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 4157 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 4158 | ` */` |
|      - | 4159 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4160 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 4161 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 4162 | `/*` |
|      - | 4163 | `** Conversion types fall into various categories as defined by the` |
|      - | 4164 | `** following enumeration.` |
|      - | 4165 | `*/` |
|      - | 4166 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 4167 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 4168 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 4169 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 4170 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 4171 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 4172 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 4173 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 4174 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 4175 |  |
|      - | 4176 | `/*` |
|      - | 4177 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 4178 | `*/` |
|      - | 4179 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 4180 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 4181 | `/*` |
|      - | 4182 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 4183 | `** by an instance of the following structure` |
|      - | 4184 | `*/` |
|      - | 4185 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 4186 | `struct ph7_fmt_info` |
|      - | 4187 | `{` |
|      - | 4188 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 4189 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 4190 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 4191 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 4192 | `  char *charset; /* The character set for conversion */` |
|      - | 4193 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 4194 | `};` |
|      - | 4195 | `/* PH7_PhpFloatShape (php's float-shape post-processing) lives in memobj.c —` |
|      - | 4196 | ` * the default float->string cast needs it even when this whole formatting` |
|      - | 4197 | ` * region is compiled out by PH7_DISABLE_DISK_IO. */` |
|      - | 4198 | `/*` |
|      - | 4199 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 4200 | ` * used conversion types first.` |
|      - | 4201 | ` */` |
|      - | 4202 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 4203 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 4204 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 4205 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 4206 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 4207 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 4208 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 4209 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 4210 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 4211 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4212 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4213 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 4214 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 4215 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 4216 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 4217 | `  /* php's 'h'/'H' are the locale-independent twins of 'g'/'G'; PHL always` |
|      - | 4218 | `   * formats in the C locale, so they behave identically. */` |
|      - | 4219 | `  {  'h',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 4220 | `  {  'H',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 4221 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 4222 | `};` |
|      - | 4223 | `/*` |
|      - | 4224 | ` * PHP 8 raises a catchable ValueError for an unknown conversion specifier` |
|      - | 4225 | ` * (e.g. "%y", or the C-ism "%#x" — '#' is not a php flag). Because printf()` |
|      - | 4226 | ` * and fprintf() stream their output incrementally while sprintf() buffers it,` |
|      - | 4227 | ` * every format builtin calls PH7_FormatValidate (below) to check the whole` |
|      - | 4228 | ` * format string BEFORE formatting so the throw happens with no partial output` |
|      - | 4229 | ` * escaping (php buffers the entire result and only emits it on success). This` |
|      - | 4230 | ` * scan mirrors the specifier-locating logic of the main format loop below.` |
|      - | 4231 | ` * On the first unknown specifier, stores it in *pBad and returns TRUE; returns` |
|      - | 4232 | ` * FALSE when every specifier is known. (A found-flag rather than a sentinel` |
|      - | 4233 | ` * char, so a NUL specifier byte — "%\0" — is still reported, not mistaken for` |
|      - | 4234 | ` * "all valid".)` |
|      - | 4235 | ` */` |
|    454 | 4236 | `static int FormatUnknownSpec(const char *zIn,int nByte,int *pBad)` |
|      3 | 4237 | `{` |
|    457 | 4238 | `	const char *zEnd = &zIn[nByte];` |
|      - | 4239 | `	int c,idx;` |
|   3703 | 4240 | `	while( zIn < zEnd ){` |
|   3269 | 4241 | `		if( zIn[0] != '%' ){` |
|   2389 | 4242 | `			zIn++;` |
|   2389 | 4243 | `			continue;` |
|      - | 4244 | `		}` |
|    881 | 4245 | `		zIn++; /* jump the percent sign */` |
|      - | 4246 | `		/* php-supported flags: '-', '+', ' ', '0' and the "'<pad>'" custom-pad` |
|      - | 4247 | `		 * form. '#' is intentionally NOT treated as a flag so it surfaces as an` |
|      - | 4248 | `		 * unknown specifier, matching php. */` |
|   1085 | 4249 | `		while( zIn < zEnd ){` |
|   1083 | 4250 | `			c = zIn[0];` |
|   1083 | 4251 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|    193 | 4252 | `				zIn++;` |
|    193 | 4253 | `				continue;` |
|      - | 4254 | `			}` |
|    891 | 4255 | `			if( c=='\'' ){` |
|     13 | 4256 | `				zIn++;` |
|     13 | 4257 | `				if( zIn < zEnd ){` |
|     13 | 4258 | `					zIn++; /* the custom pad character */` |
|      6 | 4259 | `				}` |
|     13 | 4260 | `				continue;` |
|      - | 4261 | `			}` |
|    879 | 4262 | `			break;` |
|    ! 0 | 4263 | `		}` |
|      - | 4264 | `		/* field width */` |
|   1149 | 4265 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    269 | 4266 | `			zIn++;` |
|      1 | 4267 | `		}` |
|      - | 4268 | `		/* positional specifier ($) — php parses flags AFTER it (e.g. "%1$-10s"),` |
|      - | 4269 | `		 * so skip the full flag set and width again, mirroring the main loop. */` |
|    881 | 4270 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
|      5 | 4271 | `			zIn++;` |
|      7 | 4272 | `			while( zIn < zEnd ){` |
|      7 | 4273 | `				c = zIn[0];` |
|      7 | 4274 | `				if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|    ! 0 | 4275 | `					zIn++;` |
|    ! 0 | 4276 | `					continue;` |
|      - | 4277 | `				}` |
|      7 | 4278 | `				if( c=='\'' ){` |
|      3 | 4279 | `					zIn++;` |
|      3 | 4280 | `					if( zIn < zEnd ){` |
|      3 | 4281 | `						zIn++;` |
|      1 | 4282 | `					}` |
|      3 | 4283 | `					continue;` |
|      - | 4284 | `				}` |
|      5 | 4285 | `				break;` |
|    ! 0 | 4286 | `			}` |
|     13 | 4287 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|      9 | 4288 | `				zIn++;` |
|      1 | 4289 | `			}` |
|      2 | 4290 | `		}` |
|      - | 4291 | `		/* precision */` |
|    881 | 4292 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|    113 | 4293 | `			zIn++;` |
|    243 | 4294 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    133 | 4295 | `				zIn++;` |
|      3 | 4296 | `			}` |
|     55 | 4297 | `		}` |
|      - | 4298 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|    881 | 4299 | `		if( zIn < zEnd && zIn[0]=='l' ){` |
|     11 | 4300 | `			zIn++;` |
|      5 | 4301 | `		}` |
|    881 | 4302 | `		if( zIn >= zEnd ){` |
|      - | 4303 | `			/* A dangling '%' with no specifier: PHL's legacy path silently` |
|      - | 4304 | `			 * truncates here (recorded residual); nothing to validate. */` |
|      3 | 4305 | `			break;` |
|      - | 4306 | `		}` |
|    879 | 4307 | `		c = zIn[0];` |
|    879 | 4308 | `		zIn++; /* jump the conversion specifier */` |
|   3675 | 4309 | `		for( idx = 0 ; idx < (int)SX_ARRAYSIZE(aFmt) ; idx++ ){` |
|   3657 | 4310 | `			if( c == aFmt[idx].fmttype ){` |
|    861 | 4311 | `				break;` |
|      - | 4312 | `			}` |
|   1401 | 4313 | `		}` |
|    879 | 4314 | `		if( idx >= (int)SX_ARRAYSIZE(aFmt) ){` |
|     19 | 4315 | `			*pBad = c; /* unknown specifier */` |
|     19 | 4316 | `			return TRUE;` |
|      - | 4317 | `		}` |
|      3 | 4318 | `	}` |
|    439 | 4319 | `	return FALSE;` |
|    230 | 4320 | `}` |
|      - | 4321 | `/*` |
|      - | 4322 | ` * Validate a printf-style format string. PHP 8 raises a catchable ValueError for` |
|      - | 4323 | ` * an unknown conversion specifier, thrown before any output is produced. Every` |
|      - | 4324 | ` * format builtin (sprintf/printf/vprintf/vsprintf/fprintf/vfprintf) calls this` |
|      - | 4325 | ` * up-front, then propagates the returned status verbatim (PH7_EXCEPTION when the` |
|      - | 4326 | ` * throw is caught in place, PH7_ABORT when it goes uncaught).` |
|      - | 4327 | ` * Returns PH7_OK when the format is valid.` |
|      - | 4328 | ` */` |
|    454 | 4329 | `PH7_PRIVATE sxi32 PH7_FormatValidate(ph7_context *pCtx,const char *zFormat,int nByte)` |
|      3 | 4330 | `{` |
|    457 | 4331 | `	int badSpec = 0;` |
|    457 | 4332 | `	if( FormatUnknownSpec(zFormat,nByte,&badSpec) ){` |
|     28 | 4333 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      9 | 4334 | `			"Unknown format specifier \"%c\"",badSpec);` |
|      - | 4335 | `	}` |
|    439 | 4336 | `	return PH7_OK;` |
|    230 | 4337 | `}` |
|      - | 4338 | `/*` |
|      - | 4339 | `` * PHP 8: a printf-family `$format` argument is a `string` parameter — scalars`` |
|      - | 4340 | ` * (int/float/bool) and null coerce to a string, but an array/object/resource` |
|      - | 4341 | ` * raises a catchable TypeError. iArg is the 1-based argument position ($format` |
|      - | 4342 | ` * is #1 for sprintf/printf/vprintf/vsprintf, #2 for fprintf/vfprintf). Returns` |
|      - | 4343 | ` * PH7_OK when the value is string-coercible (the caller then uses` |
|      - | 4344 | ` * ph7_value_to_string, which renders scalars/null verbatim).` |
|      - | 4345 | ` */` |
|      - | 4346 | `/*` |
|      - | 4347 | ` * php 8: a stream parameter that is not a resource is a TypeError, not a warning` |
|      - | 4348 | ` * with a 0 return -- the caller never learned its write went nowhere.` |
|      - | 4349 | ` */` |
|     20 | 4350 | `PH7_PRIVATE sxi32 PH7_CheckStreamArg(ph7_context *pCtx,ph7_value *pArg,int iArg,const char *zName)` |
|      1 | 4351 | `{` |
|     21 | 4352 | `	if( !ph7_value_is_resource(pArg) ){` |
|      - | 4353 | `		char zBuf[64];` |
|      4 | 4354 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4355 | `			"%s(): Argument #%d ($%s) must be of type resource, %s given",` |
|      1 | 4356 | `			ph7_function_name(pCtx),iArg,zName,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|      - | 4357 | `	}` |
|     19 | 4358 | `	return PH7_OK;` |
|     11 | 4359 | `}` |
|    466 | 4360 | `PH7_PRIVATE sxi32 PH7_FormatCheckFormatArg(ph7_context *pCtx,ph7_value *pArg,int iArg)` |
|      3 | 4361 | `{` |
|    469 | 4362 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg) \|\| ph7_value_is_resource(pArg) ){` |
|      - | 4363 | `		char zBuf[64];` |
|    ! 0 | 4364 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4365 | `			"%s(): Argument #%d ($format) must be of type string, %s given",` |
|    ! 0 | 4366 | `			ph7_function_name(pCtx),iArg,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|      - | 4367 | `	}` |
|    469 | 4368 | `	return PH7_OK;` |
|    236 | 4369 | `}` |
|      - | 4370 | `/*` |
|      - | 4371 | ` * Format a given string.` |
|      - | 4372 | ` * The root program.  All variations call this core.` |
|      - | 4373 | ` * INPUTS:` |
|      - | 4374 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 4375 | ` *            1. A pointer to the call context.` |
|      - | 4376 | ` *            2. A pointer to the list of characters to be output` |
|      - | 4377 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 4378 | ` *            3. An integer number of characters to be output.` |
|      - | 4379 | ` *               (Note: This number might be zero.)` |
|      - | 4380 | ` *            4. Upper layer private data.` |
|      - | 4381 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 4382 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 4383 | ` */` |
|    436 | 4384 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 4385 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 4386 | `	ph7_context *pCtx,  /* call context */` |
|      - | 4387 | `	const char *zIn,    /* Format string */` |
|      - | 4388 | `	int nByte,          /* Format string length */` |
|      - | 4389 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 4390 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 4391 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 4392 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 4393 | `	)` |
|      3 | 4394 | `{` |
|    439 | 4395 | `	char spaces[] = "                                                  ";` |
|      - | 4396 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    439 | 4397 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 4398 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 4399 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 4400 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 4401 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 4402 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 4403 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 4404 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 4405 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 4406 | `	ph7_int64 iVal;` |
|      - | 4407 | `	int precision;           /* Precision of the current field */` |
|      - | 4408 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 4409 | `	int c,rc,n;` |
|      - | 4410 | `	int length;              /* Length of the field */` |
|      - | 4411 | `	int prefix;` |
|      - | 4412 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 4413 | `	int width;               /* Width of the current field */` |
|      - | 4414 | `	int idx;` |
|    439 | 4415 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 4416 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 4417 | `	/* An unknown conversion specifier is rejected up-front by PH7_FormatValidate()` |
|      - | 4418 | `	 * (called by every format builtin before this routine), so the specifier set` |
|      - | 4419 | `	 * seen here is always valid. */` |
|      - | 4420 | `	/* Start the format process */` |
|    647 | 4421 | `	for(;;){` |
|   1297 | 4422 | `		zCur = zIn;` |
|   3671 | 4423 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|   2375 | 4424 | `			zIn++;` |
|      1 | 4425 | `		}` |
|   1297 | 4426 | `		if( zCur < zIn ){` |
|      - | 4427 | `			/* Consume chunk verbatim */` |
|    769 | 4428 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|    769 | 4429 | `			if( rc != SXRET_OK ){` |
|      - | 4430 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|    ! 0 | 4431 | `				break;` |
|      - | 4432 | `			}` |
|    384 | 4433 | `		}` |
|   1297 | 4434 | `		if( zIn >= zEnd ){` |
|      - | 4435 | `			/* No more input to process,break immediately */` |
|    437 | 4436 | `			break;` |
|      - | 4437 | `		}` |
|      - | 4438 | `		/* Find out what flags are present */` |
|    863 | 4439 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    860 | 4440 | `			flag_alternateform = flag_zeropad = 0;` |
|      - | 4441 | `		/* Reset the pad buffer to spaces: a custom pad char ('X) — or the string` |
|      - | 4442 | `		 * zero-pad below — from a PREVIOUS specifier must not bleed into this one.` |
|      - | 4443 | `		 * php resets the pad character for every specifier. */` |
|  43863 | 4444 | `		for( idx = 0 ; idx < etSPACESIZE ; ++idx ){ spaces[idx] = ' '; }` |
|    863 | 4445 | `		zIn++; /* Jump the precent sign */` |
|    430 | 4446 | `		do{` |
|   1067 | 4447 | `			c = zIn[0];` |
|   1067 | 4448 | `			switch( c ){` |
|     19 | 4449 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      7 | 4450 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 4451 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|    163 | 4452 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|      6 | 4453 | `			case '\'':` |
|     13 | 4454 | `				zIn++;` |
|     13 | 4455 | `				if( zIn < zEnd ){` |
|      - | 4456 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|     13 | 4457 | `					c = zIn[0];` |
|    613 | 4458 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    601 | 4459 | `						spaces[idx] = (char)c;` |
|    301 | 4460 | `					}` |
|     13 | 4461 | `					c = 0;` |
|      6 | 4462 | `				}` |
|     12 | 4463 | `				break;` |
|    860 | 4464 | `			default:                                       break;` |
|      - | 4465 | `			}` |
|   1067 | 4466 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 4467 | `		/* Get the field width */` |
|    863 | 4468 | `		width = 0;` |
|   1561 | 4469 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    269 | 4470 | `			width = width*10 + (zIn[0] - '0');` |
|    269 | 4471 | `			zIn++;` |
|      1 | 4472 | `		}` |
|    863 | 4473 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 4474 | `			/* Position specifer */` |
|      5 | 4475 | `			if( width > 0 ){` |
|      5 | 4476 | `				n = width;` |
|      5 | 4477 | `				if( vf && n > 0 ){` |
|    ! 0 | 4478 | `					n--;` |
|    ! 0 | 4479 | `				}` |
|      2 | 4480 | `			}` |
|      5 | 4481 | `			zIn++;` |
|      5 | 4482 | `			width = 0;` |
|      - | 4483 | `			/* php's grammar is %argnum$<flags><width>: the flags come AFTER the` |
|      - | 4484 | `			 * positional, so re-parse the full flag set here (e.g. "%1$-10s"),` |
|      - | 4485 | `			 * not just zero-padding. */` |
|      2 | 4486 | `			do{` |
|      7 | 4487 | `				c = zIn[0];` |
|      7 | 4488 | `				switch( c ){` |
|    ! 0 | 4489 | `				case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|    ! 0 | 4490 | `				case '+':   flag_plussign = 1;        c = 0;   break;` |
|    ! 0 | 4491 | `				case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|    ! 0 | 4492 | `				case '0':   flag_zeropad = 1;         c = 0;   break;` |
|      1 | 4493 | `				case '\'':` |
|      3 | 4494 | `					zIn++;` |
|      3 | 4495 | `					if( zIn < zEnd ){` |
|      3 | 4496 | `						c = zIn[0];` |
|    103 | 4497 | `						for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    101 | 4498 | `							spaces[idx] = (char)c;` |
|     51 | 4499 | `						}` |
|      3 | 4500 | `						c = 0;` |
|      1 | 4501 | `					}` |
|      2 | 4502 | `					break;` |
|      4 | 4503 | `				default:                                       break;` |
|      - | 4504 | `				}` |
|      7 | 4505 | `			}while( c==0 && (zIn++ < zEnd) );` |
|     15 | 4506 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|      9 | 4507 | `				width = width*10 + (zIn[0] - '0');` |
|      9 | 4508 | `				zIn++;` |
|      1 | 4509 | `			}` |
|      2 | 4510 | `		}` |
|    863 | 4511 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 4512 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 4513 | `		}` |
|      - | 4514 | `		/* Get the precision */` |
|    863 | 4515 | `		precision = -1;` |
|    863 | 4516 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|    113 | 4517 | `			precision = 0;` |
|    113 | 4518 | `			zIn++;` |
|    298 | 4519 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    133 | 4520 | `				precision = precision*10 + (zIn[0] - '0');` |
|    133 | 4521 | `				zIn++;` |
|      3 | 4522 | `			}` |
|     55 | 4523 | `		}` |
|      - | 4524 | `		/* Consume a single 'l' length modifier (a C-ism php accepts and ignores,` |
|      - | 4525 | `		 * e.g. "%ld"); PH7_FormatValidate mirrors this. Exactly one is skipped:` |
|      - | 4526 | `		 * in "%lld" the second 'l' becomes the (unknown) specifier, just like php. */` |
|    863 | 4527 | `		if( zIn < zEnd && zIn[0] == 'l' ){` |
|      9 | 4528 | `			zIn++;` |
|      4 | 4529 | `		}` |
|    863 | 4530 | `		if( zIn >= zEnd ){` |
|      - | 4531 | `			/* No more input */` |
|      3 | 4532 | `			break;` |
|      - | 4533 | `		}` |
|      - | 4534 | `		/* Fetch the info entry for the field */` |
|    861 | 4535 | `		pInfo = 0;` |
|    861 | 4536 | `		xtype = PH7_FMT_ERROR;` |
|    861 | 4537 | `		c = zIn[0];` |
|    861 | 4538 | `		zIn++; /* Jump the format specifer */` |
|   3351 | 4539 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|   3351 | 4540 | `			if( c==aFmt[idx].fmttype ){` |
|    861 | 4541 | `				pInfo = &aFmt[idx];` |
|    861 | 4542 | `				xtype = pInfo->type;` |
|    861 | 4543 | `				break;` |
|      - | 4544 | `			}` |
|   1248 | 4545 | `		}` |
|    861 | 4546 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    861 | 4547 | `		length = 0;` |
|      - | 4548 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 4549 | `		 /*` |
|      - | 4550 | `		  ** At this point, variables are initialized as follows:` |
|      - | 4551 | `		  **` |
|      - | 4552 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 4553 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 4554 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 4555 | `		  **                               field width was negative.` |
|      - | 4556 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 4557 | `		  **                               the conversion character.` |
|      - | 4558 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 4559 | `		  **   width                       The specified field width.  This is` |
|      - | 4560 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 4561 | `		  **   precision                   The specified precision.  The default` |
|      - | 4562 | `		  **                               is -1.` |
|      - | 4563 | `		  */` |
|    861 | 4564 | `		switch(xtype){` |
|      3 | 4565 | `		case PH7_FMT_PERCENT:` |
|      - | 4566 | `			/* A literal percent character */` |
|      7 | 4567 | `			zWorker[0] = '%';` |
|      7 | 4568 | `			length = (int)sizeof(char);` |
|      7 | 4569 | `			break;` |
|      3 | 4570 | `		case PH7_FMT_CHARX:` |
|      - | 4571 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 4572 | `			 * with that ASCII value` |
|      - | 4573 | `			 */` |
|      7 | 4574 | `			pArg = NEXT_ARG;` |
|      7 | 4575 | `			if( pArg == 0 ){` |
|      3 | 4576 | `				c = 0;` |
|      2 | 4577 | `			}else{` |
|      5 | 4578 | `				c = ph7_value_to_int(pArg);` |
|      - | 4579 | `			}` |
|      - | 4580 | `			/* NUL byte is an acceptable value */` |
|      7 | 4581 | `			zWorker[0] = (char)c;` |
|      7 | 4582 | `			length = (int)sizeof(char);` |
|      7 | 4583 | `			break;` |
|    185 | 4584 | `		case PH7_FMT_STRING:` |
|      - | 4585 | `			/* the argument is treated as and presented as a string */` |
|    371 | 4586 | `			pArg = NEXT_ARG;` |
|    371 | 4587 | `			if( pArg == 0 ){` |
|    ! 0 | 4588 | `				length = 0;` |
|    ! 0 | 4589 | `			}else{` |
|    371 | 4590 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 4591 | `			}` |
|    371 | 4592 | `			if( length < 1 ){` |
|      - | 4593 | `				/* An empty %s substitutes NOTHING in php. PH7 substituted a single` |
|      - | 4594 | `				 * SPACE here, so printf("[%s]","") printed "[ ]" and any format with an` |
|      - | 4595 | `				 * absent optional part gained a stray space. */` |
|      9 | 4596 | `				zBuf = "";` |
|      9 | 4597 | `				length = 0;` |
|      4 | 4598 | `			}` |
|    371 | 4599 | `			if( precision>=0 && precision<length ){` |
|      3 | 4600 | `				length = precision;` |
|      1 | 4601 | `			}` |
|    371 | 4602 | `			if( flag_zeropad ){` |
|      - | 4603 | `				/* zero-padding works on strings too */` |
|    103 | 4604 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    101 | 4605 | `					spaces[idx] = '0';` |
|     51 | 4606 | `				}` |
|      1 | 4607 | `			}` |
|    371 | 4608 | `			break;` |
|    138 | 4609 | `		case PH7_FMT_RADIX:` |
|    277 | 4610 | `			pArg = NEXT_ARG;` |
|    277 | 4611 | `			if( pArg == 0 ){` |
|    ! 0 | 4612 | `				iVal = 0;` |
|    ! 0 | 4613 | `			}else{` |
|    277 | 4614 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 4615 | `			}` |
|      - | 4616 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|    277 | 4617 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 4618 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 4619 | `			}` |
|      - | 4620 | `#if 1` |
|      - | 4621 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 4622 | `        ** I think this is stupid.*/` |
|    277 | 4623 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 4624 | `#else` |
|      - | 4625 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 4626 | `        ** but leave the prefix for hex.*/` |
|      - | 4627 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 4628 | `#endif` |
|    277 | 4629 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|    253 | 4630 | `          if( iVal<0 ){` |
|     25 | 4631 | `            iVal = -iVal;` |
|      - | 4632 | `			/* Ticket 1433-003 */` |
|     25 | 4633 | `			if( iVal < 0 ){` |
|      - | 4634 | `				/* Overflow */` |
|    ! 0 | 4635 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4636 | `			}` |
|     25 | 4637 | `            prefix = '-';` |
|    241 | 4638 | `          }else if( flag_plussign )  prefix = '+';` |
|    227 | 4639 | `          else if( flag_blanksign )  prefix = ' ';` |
|    225 | 4640 | `          else                       prefix = 0;` |
|    127 | 4641 | `        }else{` |
|     25 | 4642 | `			if( iVal<0 ){` |
|    ! 0 | 4643 | `				iVal = -iVal;` |
|      - | 4644 | `				/* Ticket 1433-003 */` |
|    ! 0 | 4645 | `				if( iVal < 0 ){` |
|      - | 4646 | `					/* Overflow */` |
|    ! 0 | 4647 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4648 | `				}` |
|    ! 0 | 4649 | `			}` |
|     25 | 4650 | `			prefix = 0;` |
|      - | 4651 | `		}` |
|    277 | 4652 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|    149 | 4653 | `          precision = width-(prefix!=0);` |
|     74 | 4654 | `        }` |
|    277 | 4655 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 4656 | `        {` |
|      - | 4657 | `          register char *cset;      /* Use registers for speed */` |
|      - | 4658 | `          register int base;` |
|    277 | 4659 | `          cset = pInfo->charset;` |
|    277 | 4660 | `          base = pInfo->base;` |
|    138 | 4661 | `          do{                                           /* Convert to ascii */` |
|    353 | 4662 | `            *(--zBuf) = cset[iVal%base];` |
|    353 | 4663 | `            iVal = iVal/base;` |
|    353 | 4664 | `          }while( iVal>0 );` |
|      - | 4665 | `        }` |
|    277 | 4666 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    443 | 4667 | `        for(idx=precision-length; idx>0; idx--){` |
|    167 | 4668 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|     84 | 4669 | `        }` |
|    277 | 4670 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|    277 | 4671 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 4672 | `          char *pre, x;` |
|    ! 0 | 4673 | `          pre = pInfo->prefix;` |
|    ! 0 | 4674 | `          if( *zBuf!=pre[0] ){` |
|    ! 0 | 4675 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|    ! 0 | 4676 | `          }` |
|    ! 0 | 4677 | `        }` |
|    277 | 4678 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    277 | 4679 | `		break;` |
|    100 | 4680 | `		case PH7_FMT_FLOAT:` |
|      - | 4681 | `		case PH7_FMT_EXP:` |
|      - | 4682 | `		case PH7_FMT_GENERIC:{` |
|      - | 4683 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 4684 | `		double realvalue;` |
|      - | 4685 | `		char zFmt[8];` |
|      - | 4686 | `		int nOut, nFmt;` |
|    203 | 4687 | `		pArg = NEXT_ARG;` |
|    203 | 4688 | `		if( pArg == 0 ){` |
|    ! 0 | 4689 | `			realvalue = 0;` |
|    ! 0 | 4690 | `		}else{` |
|    203 | 4691 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 4692 | `		}` |
|      - | 4693 | `		/* php prints the IEEE specials bare — NaN / INF / -INF with no width` |
|      - | 4694 | `		 * padding, precision, or sign flags (php_sprintf_appenddouble). */` |
|    203 | 4695 | `		if( PH7_IS_NAN(realvalue) ){` |
|     21 | 4696 | `			zBuf = "NaN";` |
|     21 | 4697 | `			length = 3;` |
|     21 | 4698 | `			width = 0;` |
|     21 | 4699 | `			break;` |
|      - | 4700 | `		}` |
|    183 | 4701 | `		if( PH7_IS_INF(realvalue) ){` |
|     37 | 4702 | `			if( realvalue < 0.0 ){` |
|     15 | 4703 | `				zBuf = "-INF";` |
|     15 | 4704 | `				length = 4;` |
|      8 | 4705 | `			}else{` |
|     23 | 4706 | `				zBuf = "INF";` |
|     23 | 4707 | `				length = 3;` |
|      - | 4708 | `			}` |
|     37 | 4709 | `			width = 0;` |
|     37 | 4710 | `			break;` |
|      - | 4711 | `		}` |
|    147 | 4712 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|    147 | 4713 | `		if( precision > 53 ){` |
|      - | 4714 | `			/* php's FORMAT_CONV_MAX_PRECISION cap, with the same E_NOTICE` |
|      - | 4715 | `			 * (message prefixed with the active function's name, like` |
|      - | 4716 | `			 * php_error_docref). */` |
|      - | 4717 | `			char zMsg[160];` |
|      4 | 4718 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 4719 | `				"%z(): Requested precision of %d digits was truncated to PHP maximum of %d digits",` |
|      2 | 4720 | `				&pCtx->pFunc->sName,precision,53);` |
|      3 | 4721 | `			PH7_VmThrowError(pCtx->pVm,0,E_NOTICE,zMsg);` |
|      3 | 4722 | `			precision = 53;` |
|      1 | 4723 | `		}` |
|      - | 4724 | ``		/* php's %f/%e extract the sign via `num < 0`, so negative zero prints`` |
|      - | 4725 | `		 * unsigned there — while %g (php_gcvt on the raw value) keeps "-0". */` |
|    147 | 4726 | `		if( xtype!=PH7_FMT_GENERIC && realvalue == 0.0 ){` |
|      9 | 4727 | `			realvalue = 0.0;` |
|      4 | 4728 | `		}` |
|      - | 4729 | `		/* php's float conversions are correctly rounded (zend_dtoa); use libc` |
|      - | 4730 | `		 * snprintf as the digit engine (the byte-exact-floats rule — the old` |
|      - | 4731 | `		 * hand-rolled vxGetdigit loop stopped at 16 significant digits, so` |
|      - | 4732 | `		 * e.g. %f of 1e308 printed zeros where php prints the exact binary64` |
|      - | 4733 | `		 * expansion), then post-process into php's exact shapes below. */` |
|    147 | 4734 | `		nFmt = 0;` |
|    147 | 4735 | `		zFmt[nFmt++] = '%';` |
|    147 | 4736 | `		if( flag_alternateform ) zFmt[nFmt++] = '#';` |
|      - | 4737 | `		/* php's ' ' flag selects space PADDING (its default), not C's` |
|      - | 4738 | `		 * space-for-positive-sign — so flag_blanksign is NOT forwarded. */` |
|    147 | 4739 | `		if( flag_plussign ) zFmt[nFmt++] = '+';` |
|    147 | 4740 | `		zFmt[nFmt++] = '.';` |
|    147 | 4741 | `		zFmt[nFmt++] = '*';` |
|    195 | 4742 | `		zFmt[nFmt++] = (char)(xtype==PH7_FMT_FLOAT ? 'f' :` |
|     32 | 4743 | `			(xtype==PH7_FMT_EXP ? ((pInfo->charset[0]=='E') ? 'E' : 'e')` |
|     32 | 4744 | `			                    : ((pInfo->charset[0]=='E') ? 'G' : 'g')));` |
|    147 | 4745 | `		zFmt[nFmt] = 0;` |
|    147 | 4746 | `		nOut = snprintf(zWorker,sizeof(zWorker),zFmt,precision,realvalue);` |
|    147 | 4747 | `		if( nOut < 0 \|\| nOut >= (int)sizeof(zWorker) ){` |
|      - | 4748 | `			/* Cannot happen with precision capped at 53 (%f of DBL_MAX is` |
|      - | 4749 | `			 * ~365 bytes); keep the truncated output rather than overrun. */` |
|    ! 0 | 4750 | `			nOut = (int)SyStrlen(zWorker);` |
|    ! 0 | 4751 | `		}` |
|    147 | 4752 | `		nOut = (int)PH7_PhpFloatShape(zWorker,(sxi32)nOut,xtype==PH7_FMT_GENERIC);` |
|    147 | 4753 | `		zBuf = zWorker;` |
|    147 | 4754 | `		length = nOut;` |
|      - | 4755 | `		/* Let the zero-pad block below insert zeros between the sign (written` |
|      - | 4756 | `		 * by snprintf) and the first digit, as before. */` |
|    147 | 4757 | `		prefix = (zWorker[0]=='-' \|\| zWorker[0]=='+' \|\| zWorker[0]==' ') ? zWorker[0] : 0;` |
|      - | 4758 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 4759 | `        ** set and we are not left justified */` |
|    147 | 4760 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 4761 | `          int i;` |
|      9 | 4762 | `          int nPad = width - length;` |
|     63 | 4763 | `          for(i=width; i>=nPad; i--){` |
|     55 | 4764 | `            zBuf[i] = zBuf[i-nPad];` |
|     28 | 4765 | `          }` |
|      9 | 4766 | `          i = prefix!=0;` |
|     39 | 4767 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      9 | 4768 | `          length = width;` |
|      4 | 4769 | `        }` |
|      - | 4770 | `#else` |
|      - | 4771 | `         zBuf = " ";` |
|      - | 4772 | `		 length = (int)sizeof(char);` |
|      - | 4773 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|    147 | 4774 | `		 break;` |
|      - | 4775 | `							 }` |
|    ! 0 | 4776 | `		default:` |
|      - | 4777 | `			/* Unreachable: PH7_FormatValidate() rejects unknown specifiers with a` |
|      - | 4778 | `			 * catchable ValueError before formatting begins. Kept as a defensive` |
|      - | 4779 | `			 * no-op that emits nothing. */` |
|    ! 0 | 4780 | `			length = 0;` |
|    ! 0 | 4781 | `			break;` |
|      - | 4782 | `		}` |
|      - | 4783 | `		 /*` |
|      - | 4784 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 4785 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 4786 | `		 ** the output.` |
|      - | 4787 | `		 */` |
|    861 | 4788 | `    if( !flag_leftjustify ){` |
|      - | 4789 | `      register int nspace;` |
|    843 | 4790 | `      nspace = width-length;` |
|    843 | 4791 | `      if( nspace>0 ){` |
|     37 | 4792 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4793 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4794 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4795 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4796 | `			}` |
|    ! 0 | 4797 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4798 | `        }` |
|     37 | 4799 | `        if( nspace>0 ){` |
|     37 | 4800 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|     37 | 4801 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4802 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4803 | `			}` |
|     18 | 4804 | `		}` |
|     18 | 4805 | `      }` |
|    420 | 4806 | `    }` |
|    861 | 4807 | `    if( length>0 ){` |
|    853 | 4808 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    853 | 4809 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4810 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4811 | `		}` |
|    425 | 4812 | `    }` |
|    861 | 4813 | `    if( flag_leftjustify ){` |
|      - | 4814 | `      register int nspace;` |
|     19 | 4815 | `      nspace = width-length;` |
|     19 | 4816 | `      if( nspace>0 ){` |
|     15 | 4817 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4818 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4819 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4820 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4821 | `			}` |
|    ! 0 | 4822 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4823 | `        }` |
|     15 | 4824 | `        if( nspace>0 ){` |
|     15 | 4825 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|     15 | 4826 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4827 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4828 | `			}` |
|      7 | 4829 | `		}` |
|      7 | 4830 | `      }` |
|      9 | 4831 | `    }` |
|      3 | 4832 | ` }/* for(;;) */` |
|    439 | 4833 | `	return SXRET_OK;` |
|    221 | 4834 | `}` |
|      - | 4835 | `/*` |
|      - | 4836 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 4837 | ` */` |
|    464 | 4838 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      3 | 4839 | `{` |
|      - | 4840 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|      - | 4841 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|      - | 4842 | `	 * non-OK rc also stops the format loop. */` |
|    467 | 4843 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|    467 | 4844 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|    467 | 4845 | `	return *pRc;` |
|      3 | 4846 | `}` |
|      - | 4847 | `/*` |
|      - | 4848 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 4849 | ` *  Return a formatted string.` |
|      - | 4850 | ` * Parameters` |
|      - | 4851 | ` *  $format` |
|      - | 4852 | ` *    The format string (see block comment above)` |
|      - | 4853 | ` * Return` |
|      - | 4854 | ` *  A string produced according to the formatting string format.` |
|      - | 4855 | ` */` |
|    222 | 4856 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4857 | `{` |
|      - | 4858 | `	const char *zFormat;` |
|    225 | 4859 | `	sxi32 rc = SXRET_OK;` |
|      - | 4860 | `	int nLen;` |
|    225 | 4861 | `	if( nArg < 1 ){` |
|      - | 4862 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4863 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4864 | `		return PH7_OK;` |
|      - | 4865 | `	}` |
|      - | 4866 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|    225 | 4867 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    225 | 4868 | `	if( rc != PH7_OK ){` |
|    ! 0 | 4869 | `		return rc;` |
|      - | 4870 | `	}` |
|      - | 4871 | `	/* Extract the string format (scalars/null coerce). */` |
|    225 | 4872 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    225 | 4873 | `	if( nLen < 1 ){` |
|      - | 4874 | `		/* Empty string */` |
|    ! 0 | 4875 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4876 | `		return PH7_OK;` |
|      - | 4877 | `	}` |
|      - | 4878 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4879 | `	 * output; propagate the throw status verbatim. */` |
|    225 | 4880 | `	rc = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    225 | 4881 | `	if( rc != PH7_OK ){` |
|     17 | 4882 | `		return rc;` |
|      - | 4883 | `	}` |
|      - | 4884 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|    209 | 4885 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|    209 | 4886 | `	if( rc != SXRET_OK ){` |
|      - | 4887 | `		/* The result append ran out of memory: raise a fatal rather than` |
|      - | 4888 | `		 * returning a silently-truncated string. */` |
|    ! 0 | 4889 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4890 | `	}` |
|    209 | 4891 | `	return PH7_OK;` |
|    114 | 4892 | `}` |
|      - | 4893 | `/*` |
|      - | 4894 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 4895 | ` */` |
|   1174 | 4896 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4897 | `{` |
|   1175 | 4898 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 4899 | `	/* Call the VM output consumer directly */` |
|   1175 | 4900 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 4901 | `	/* Increment counter */` |
|   1175 | 4902 | `	*pCounter += nLen;` |
|   1175 | 4903 | `	return PH7_OK;` |
|      1 | 4904 | `}` |
|      - | 4905 | `/*` |
|      - | 4906 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 4907 | ` *  Output a formatted string.` |
|      - | 4908 | ` * Parameters` |
|      - | 4909 | ` *  $format` |
|      - | 4910 | ` *   See sprintf() for a description of format.` |
|      - | 4911 | ` * Return` |
|      - | 4912 | ` *  The length of the outputted string.` |
|      - | 4913 | ` */` |
|    204 | 4914 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4915 | `{` |
|    205 | 4916 | `	ph7_int64 nCounter = 0;` |
|      - | 4917 | `	const char *zFormat;` |
|      - | 4918 | `	int nLen;` |
|    205 | 4919 | `	if( nArg < 1 ){` |
|      - | 4920 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 4921 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4922 | `		return PH7_OK;` |
|      - | 4923 | `	}` |
|      - | 4924 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|      - | 4925 | `	{` |
|    205 | 4926 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    205 | 4927 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 4928 | `			return rcf;` |
|      - | 4929 | `		}` |
|      - | 4930 | `	}` |
|      - | 4931 | `	/* Extract the string format (scalars/null coerce). */` |
|    205 | 4932 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    205 | 4933 | `	if( nLen < 1 ){` |
|      - | 4934 | `		/* Empty string */` |
|    ! 0 | 4935 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4936 | `		return PH7_OK;` |
|      - | 4937 | `	}` |
|      - | 4938 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4939 | `	 * output; propagate the throw status verbatim. */` |
|      - | 4940 | `	{` |
|    205 | 4941 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    205 | 4942 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 4943 | `			return rcv;` |
|      - | 4944 | `		}` |
|      - | 4945 | `	}` |
|      - | 4946 | `	/* Format the string */` |
|    205 | 4947 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 4948 | `	/* Return the length of the outputted string */` |
|    205 | 4949 | `	ph7_result_int64(pCtx,nCounter);` |
|    205 | 4950 | `	return PH7_OK;` |
|    103 | 4951 | `}` |
|      - | 4952 | `/*` |
|      - | 4953 | ` * int vprintf(string $format,array $args)` |
|      - | 4954 | ` *  Output a formatted string.` |
|      - | 4955 | ` * Parameters` |
|      - | 4956 | ` *  $format` |
|      - | 4957 | ` *   See sprintf() for a description of format.` |
|      - | 4958 | ` * Return` |
|      - | 4959 | ` *  The length of the outputted string.` |
|      - | 4960 | ` */` |
|      4 | 4961 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4962 | `{` |
|      5 | 4963 | `	ph7_int64 nCounter = 0;` |
|      - | 4964 | `	const char *zFormat;` |
|      - | 4965 | `	ph7_hashmap *pMap;` |
|      - | 4966 | `	SySet sArg;` |
|      - | 4967 | `	int nLen,n;` |
|      - | 4968 | `	sxi32 rcFmt;` |
|      5 | 4969 | `	if( nArg < 2 ){` |
|      - | 4970 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 4971 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4972 | `		return PH7_OK;` |
|      - | 4973 | `	}` |
|      - | 4974 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|      5 | 4975 | `	rcFmt = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|      5 | 4976 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4977 | `		return rcFmt;` |
|      - | 4978 | `	}` |
|      5 | 4979 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 4980 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 4981 | `		char zBuf[64];` |
|      4 | 4982 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4983 | `			"vprintf(): Argument #2 ($values) must be of type array, %s given",` |
|      2 | 4984 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 4985 | `	}` |
|      - | 4986 | `	/* Extract the string format (scalars/null coerce). */` |
|      3 | 4987 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4988 | `	if( nLen < 1 ){` |
|      - | 4989 | `		/* Empty string */` |
|    ! 0 | 4990 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4991 | `		return PH7_OK;` |
|      - | 4992 | `	}` |
|      - | 4993 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4994 | `	 * output; propagate the throw status verbatim. */` |
|      3 | 4995 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      3 | 4996 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4997 | `		return rcFmt;` |
|      - | 4998 | `	}` |
|      - | 4999 | `	/* Point to the hashmap */` |
|      3 | 5000 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 5001 | `	/* Extract arguments from the hashmap */` |
|      3 | 5002 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 5003 | `	/* Format the string */` |
|      3 | 5004 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 5005 | `	/* Release the container */` |
|      3 | 5006 | `	SySetRelease(&sArg);` |
|      - | 5007 | `	/* Return the length of the outputted string */` |
|      3 | 5008 | `	ph7_result_int64(pCtx,nCounter);` |
|      3 | 5009 | `	return PH7_OK;` |
|      3 | 5010 | `}` |
|      - | 5011 | `/*` |
|      - | 5012 | ` * int vsprintf(string $format,array $args)` |
|      - | 5013 | ` *  Output a formatted string.` |
|      - | 5014 | ` * Parameters` |
|      - | 5015 | ` *  $format` |
|      - | 5016 | ` *   See sprintf() for a description of format.` |
|      - | 5017 | ` * Return` |
|      - | 5018 | ` *  A string produced according to the formatting string format.` |
|      - | 5019 | ` */` |
|     18 | 5020 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5021 | `{` |
|      - | 5022 | `	const char *zFormat;` |
|      - | 5023 | `	ph7_hashmap *pMap;` |
|      - | 5024 | `	SySet sArg;` |
|     19 | 5025 | `	sxi32 rc = SXRET_OK;` |
|      - | 5026 | `	sxi32 rcFmt;` |
|      - | 5027 | `	int nLen,n;` |
|     19 | 5028 | `	if( nArg < 2 ){` |
|      - | 5029 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5030 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5031 | `		return PH7_OK;` |
|      - | 5032 | `	}` |
|      - | 5033 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|     19 | 5034 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|     19 | 5035 | `	if( rc != PH7_OK ){` |
|    ! 0 | 5036 | `		return rc;` |
|      - | 5037 | `	}` |
|     19 | 5038 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 5039 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 5040 | `		char zBuf[64];` |
|     16 | 5041 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5042 | `			"vsprintf(): Argument #2 ($values) must be of type array, %s given",` |
|     10 | 5043 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 5044 | `	}` |
|      - | 5045 | `	/* Extract the string format (scalars/null coerce). */` |
|      9 | 5046 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 5047 | `	if( nLen < 1 ){` |
|      - | 5048 | `		/* Empty string */` |
|    ! 0 | 5049 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5050 | `		return PH7_OK;` |
|      - | 5051 | `	}` |
|      - | 5052 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 5053 | `	 * output; propagate the throw status verbatim. */` |
|      9 | 5054 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      9 | 5055 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 5056 | `		return rcFmt;` |
|      - | 5057 | `	}` |
|      - | 5058 | `	/* Point to hashmap */` |
|      9 | 5059 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 5060 | `	/* Extract arguments from the hashmap */` |
|      9 | 5061 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 5062 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|      9 | 5063 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|      - | 5064 | `	/* Release the container */` |
|      9 | 5065 | `	SySetRelease(&sArg);` |
|      9 | 5066 | `	if( rc != SXRET_OK ){` |
|      - | 5067 | `		/* The result append ran out of memory: raise a fatal. */` |
|    ! 0 | 5068 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5069 | `	}` |
|      9 | 5070 | `	return PH7_OK;` |
|     10 | 5071 | `}` |
|      - | 5072 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5073 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5074 | `/*` |
|      - | 5075 | ` * Symisc eXtension.` |
|      - | 5076 | ` * string size_format(int64 $size)` |
|      - | 5077 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 5078 | ` *  Example:` |
|      - | 5079 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 5080 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 5081 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 5082 | ` * Parameter` |
|      - | 5083 | ` *  $size` |
|      - | 5084 | ` *    Entity size in bytes.` |
|      - | 5085 | ` * Return` |
|      - | 5086 | ` *   Formatted string representation of the given size.` |
|      - | 5087 | ` */` |
|     24 | 5088 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5089 | `{` |
|      - | 5090 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 5091 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 5092 | `	sxi32 nRest,i_32;` |
|      - | 5093 | `	ph7_int64 iSize;` |
|     25 | 5094 | `	int c = -1; /* index in zUnit[] */` |
|      - | 5095 |  |
|     25 | 5096 | `	if( nArg < 1 ){` |
|      - | 5097 | `		/* Missing argument,return the empty string */` |
|      3 | 5098 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5099 | `		return PH7_OK;` |
|      - | 5100 | `	}` |
|      - | 5101 | `	/* Extract the given size */` |
|     23 | 5102 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 5103 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 5104 | `		/* Don't bother formatting,return immediately */` |
|      5 | 5105 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 5106 | `		return PH7_OK;` |
|      - | 5107 | `	}` |
|     19 | 5108 | `	for(;;){` |
|     39 | 5109 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 5110 | `		iSize >>= 10;` |
|     39 | 5111 | `		c++;` |
|     39 | 5112 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 5113 | `			break;` |
|      - | 5114 | `		}` |
|      1 | 5115 | `	}` |
|     19 | 5116 | `	nRest /= 100;` |
|     19 | 5117 | `	if( nRest > 9 ){` |
|    ! 0 | 5118 | `		nRest = 9;` |
|    ! 0 | 5119 | `	}` |
|     19 | 5120 | `	if( iSize > 999 ){` |
|    ! 0 | 5121 | `		c++;` |
|    ! 0 | 5122 | `		nRest = 9;` |
|    ! 0 | 5123 | `		iSize = 0;` |
|    ! 0 | 5124 | `	}` |
|     19 | 5125 | `	i_32 = (sxi32)iSize;` |
|      - | 5126 | `	/* Format */` |
|     19 | 5127 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 5128 | `	return PH7_OK;` |
|     13 | 5129 | `}` |
|      - | 5130 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 5131 | `/*` |
|      - | 5132 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 5133 | ` *   Calculate the md5 hash of a string.` |
|      - | 5134 | ` * Parameter` |
|      - | 5135 | ` *  $str` |
|      - | 5136 | ` *   Input string` |
|      - | 5137 | ` * $raw_output` |
|      - | 5138 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 5139 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 5140 | ` * Return` |
|      - | 5141 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 5142 | ` */` |
|     12 | 5143 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5144 | `{` |
|      - | 5145 | `	unsigned char zDigest[16];` |
|     13 | 5146 | `	int raw_output = FALSE;` |
|      - | 5147 | `	const void *pIn;` |
|      - | 5148 | `	int nLen;` |
|     13 | 5149 | `	if( nArg < 1 ){` |
|      - | 5150 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5151 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5152 | `		return PH7_OK;` |
|      - | 5153 | `	}` |
|      - | 5154 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 5155 | `	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */` |
|     13 | 5156 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 5157 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 5158 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 5159 | `	}` |
|      - | 5160 | `	/* Compute the MD5 digest */` |
|     13 | 5161 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|     13 | 5162 | `	if( raw_output ){` |
|      - | 5163 | `		/* Output raw digest */` |
|      5 | 5164 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 5165 | `	}else{` |
|      - | 5166 | `		/* Perform a binary to hex conversion */` |
|      9 | 5167 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 5168 | `	}` |
|     13 | 5169 | `	return PH7_OK;` |
|      7 | 5170 | `}` |
|      - | 5171 | `/*` |
|      - | 5172 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 5173 | ` *   Calculate the sha1 hash of a string.` |
|      - | 5174 | ` * Parameter` |
|      - | 5175 | ` *  $str` |
|      - | 5176 | ` *   Input string` |
|      - | 5177 | ` * $raw_output` |
|      - | 5178 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 5179 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 5180 | ` * Return` |
|      - | 5181 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 5182 | ` */` |
|     10 | 5183 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5184 | `{` |
|      - | 5185 | `	unsigned char zDigest[20];` |
|     11 | 5186 | `	int raw_output = FALSE;` |
|      - | 5187 | `	const void *pIn;` |
|      - | 5188 | `	int nLen;` |
|     11 | 5189 | `	if( nArg < 1 ){` |
|      - | 5190 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5191 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5192 | `		return PH7_OK;` |
|      - | 5193 | `	}` |
|      - | 5194 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 5195 | `	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */` |
|     11 | 5196 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5197 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 5198 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 5199 | `	}` |
|      - | 5200 | `	/* Compute the SHA1 digest */` |
|     11 | 5201 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|     11 | 5202 | `	if( raw_output ){` |
|      - | 5203 | `		/* Output raw digest */` |
|      5 | 5204 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 5205 | `	}else{` |
|      - | 5206 | `		/* Perform a binary to hex conversion */` |
|      7 | 5207 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 5208 | `	}` |
|     11 | 5209 | `	return PH7_OK;` |
|      6 | 5210 | `}` |
|      - | 5211 | `/*` |
|      - | 5212 | ` * int64 crc32(string $str)` |
|      - | 5213 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 5214 | ` * Parameter` |
|      - | 5215 | ` *  $str` |
|      - | 5216 | ` *   Input string` |
|      - | 5217 | ` * Return` |
|      - | 5218 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 5219 | ` */` |
|      2 | 5220 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5221 | `{` |
|      - | 5222 | `	const void *pIn;` |
|      - | 5223 | `	sxu32 nCRC;` |
|      - | 5224 | `	int nLen;` |
|      3 | 5225 | `	if( nArg < 1 ){` |
|      - | 5226 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 5227 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5228 | `		return PH7_OK;` |
|      - | 5229 | `	}` |
|      - | 5230 | `	/* Extract the input string */` |
|      3 | 5231 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5232 | `	if( nLen < 1 ){` |
|      - | 5233 | `		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike` |
|      - | 5234 | `		 * md5()/sha1(), whose empty-string digests are non-zero. */` |
|    ! 0 | 5235 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5236 | `		return PH7_OK;` |
|      - | 5237 | `	}` |
|      - | 5238 | `	/* Calculate the sum */` |
|      3 | 5239 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 5240 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 5241 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 5242 | `	return PH7_OK;` |
|      2 | 5243 | `}` |
|      - | 5244 | `/*` |
|      - | 5245 | ` * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is` |
|      - | 5246 | ` * described by a small record so one dispatch (and one generic HMAC) serves them` |
|      - | 5247 | ` * all. Thin adapters normalize the differing context types and the reversed` |
|      - | 5248 | ` * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.` |
|      - | 5249 | ` */` |
|     11 | 5250 | `static void HashMd5Init(HashCtx *c){ MD5Init(&c->md5); }` |
|     15 | 5251 | `static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }` |
|     11 | 5252 | `static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }` |
|     11 | 5253 | `static void HashSha1Init(HashCtx *c){ SHA1Init(&c->sha1); }` |
|     15 | 5254 | `static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }` |
|     11 | 5255 | `static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }` |
|      9 | 5256 | `static void HashSha224Init(HashCtx *c){ SHA224Init(&c->sha256); }` |
|     33 | 5257 | `static void HashSha256Init(HashCtx *c){ SHA256Init(&c->sha256); }` |
|     57 | 5258 | `static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }` |
|     41 | 5259 | `static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }` |
|      9 | 5260 | `static void HashSha384Init(HashCtx *c){ SHA384Init(&c->sha512); }` |
|     15 | 5261 | `static void HashSha512Init(HashCtx *c){ SHA512Init(&c->sha512); }` |
|     27 | 5262 | `static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }` |
|     23 | 5263 | `static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }` |
|      - | 5264 | `typedef struct HashAlgo HashAlgo;` |
|      - | 5265 | `struct HashAlgo {` |
|      - | 5266 | `	const char *zName;   /* lowercase canonical name */` |
|      - | 5267 | `	int nDigestLen;      /* output bytes: 16/20/28/32/48/64 */` |
|      - | 5268 | `	int nBlockLen;       /* internal block bytes (for HMAC): 64 or 128 */` |
|      - | 5269 | `	void (*xInit)(HashCtx *);` |
|      - | 5270 | `	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);` |
|      - | 5271 | `	void (*xFinal)(HashCtx *,unsigned char *);` |
|      - | 5272 | `};` |
|      - | 5273 | `static const HashAlgo aHashAlgo[] = {` |
|      - | 5274 | `	{ "md5",    16, 64,  HashMd5Init,    HashMd5Update,    HashMd5Final    },` |
|      - | 5275 | `	{ "sha1",   20, 64,  HashSha1Init,   HashSha1Update,   HashSha1Final   },` |
|      - | 5276 | `	{ "sha224", 28, 64,  HashSha224Init, HashSha256Update, HashSha256Final },` |
|      - | 5277 | `	{ "sha256", 32, 64,  HashSha256Init, HashSha256Update, HashSha256Final },` |
|      - | 5278 | `	{ "sha384", 48, 128, HashSha384Init, HashSha512Update, HashSha512Final },` |
|      - | 5279 | `	{ "sha512", 64, 128, HashSha512Init, HashSha512Update, HashSha512Final },` |
|      - | 5280 | `};` |
|      - | 5281 | `/* Case-insensitive algorithm lookup (PHP accepts 'SHA256' etc.). */` |
|     73 | 5282 | `static const HashAlgo * HashFindAlgo(const char *zName,int nLen){` |
|      - | 5283 | `	sxu32 i;` |
|    279 | 5284 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|    272 | 5285 | `		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen` |
|    211 | 5286 | `			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){` |
|     67 | 5287 | `			return &aHashAlgo[i];` |
|      - | 5288 | `		}` |
|    106 | 5289 | `	}` |
|      6 | 5290 | `	return 0;` |
|     38 | 5291 | `}` |
|      - | 5292 | `/*` |
|      - | 5293 | ` * string hash(string $algo,string $data[,bool $binary = false])` |
|      - | 5294 | ` *   Generate a hash value (message digest).` |
|      - | 5295 | ` */` |
|     54 | 5296 | `static int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5297 | `{` |
|      - | 5298 | `	const HashAlgo *pAlgo;` |
|      - | 5299 | `	const char *zAlgo,*zData;` |
|     56 | 5300 | `	int nAlgoLen,nDataLen,raw_output = FALSE;` |
|      - | 5301 | `	HashCtx sCtx;` |
|      - | 5302 | `	unsigned char zDigest[64];` |
|     56 | 5303 | `	if( nArg < 2 ){` |
|    ! 0 | 5304 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5305 | `			"hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5306 | `	}` |
|     56 | 5307 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     56 | 5308 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     56 | 5309 | `	if( pAlgo == 0 ){` |
|      3 | 5310 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5311 | `			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - | 5312 | `	}` |
|     53 | 5313 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     53 | 5314 | `	if( nArg > 2 ){` |
|      9 | 5315 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|      4 | 5316 | `	}` |
|     53 | 5317 | `	pAlgo->xInit(&sCtx);` |
|     53 | 5318 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     53 | 5319 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     53 | 5320 | `	if( raw_output ){` |
|      9 | 5321 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|      5 | 5322 | `	}else{` |
|     45 | 5323 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - | 5324 | `	}` |
|     53 | 5325 | `	return PH7_OK;` |
|     29 | 5326 | `}` |
|      - | 5327 | `/*` |
|      - | 5328 | ` * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])` |
|      - | 5329 | ` *   Generate a keyed hash value using the HMAC method (RFC 2104).` |
|      - | 5330 | ` */` |
|     16 | 5331 | `static int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5332 | `{` |
|      - | 5333 | `	const HashAlgo *pAlgo;` |
|      - | 5334 | `	const char *zAlgo,*zData,*zKey;` |
|     18 | 5335 | `	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;` |
|      - | 5336 | `	HashCtx sCtx;` |
|      - | 5337 | `	unsigned char zKeyBlock[128],zIpad[128],zOpad[128],zInner[64],zDigest[64];` |
|      - | 5338 | `	int i,nBlock,nDigest;` |
|     18 | 5339 | `	if( nArg < 3 ){` |
|    ! 0 | 5340 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5341 | `			"hash_hmac() expects at least 3 arguments, %d given",nArg);` |
|      - | 5342 | `	}` |
|     18 | 5343 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     18 | 5344 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     18 | 5345 | `	if( pAlgo == 0 ){` |
|      3 | 5346 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5347 | `			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 5348 | `	}` |
|     15 | 5349 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     15 | 5350 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|     15 | 5351 | `	if( nArg > 3 ){` |
|      3 | 5352 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|      1 | 5353 | `	}` |
|     15 | 5354 | `	nBlock = pAlgo->nBlockLen;` |
|     15 | 5355 | `	nDigest = pAlgo->nDigestLen;` |
|      - | 5356 | `	/* Reduce the key to a single block: hash it if longer than the block, then` |
|      - | 5357 | `	 * zero-pad (a short or empty key is just zero-padded). */` |
|     15 | 5358 | `	SyZero(zKeyBlock,sizeof(zKeyBlock));` |
|     15 | 5359 | `	if( nKeyLen > nBlock ){` |
|      3 | 5360 | `		pAlgo->xInit(&sCtx);` |
|      3 | 5361 | `		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);` |
|      3 | 5362 | `		pAlgo->xFinal(&sCtx,zKeyBlock);` |
|     14 | 5363 | `	}else if( nKeyLen > 0 ){` |
|     11 | 5364 | `		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);` |
|      5 | 5365 | `	}` |
|   1039 | 5366 | `	for( i = 0; i < nBlock; i++ ){` |
|   1025 | 5367 | `		zIpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);` |
|   1025 | 5368 | `		zOpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);` |
|    513 | 5369 | `	}` |
|      - | 5370 | `	/* inner = H((key ^ ipad) \|\| data) */` |
|     15 | 5371 | `	pAlgo->xInit(&sCtx);` |
|     15 | 5372 | `	pAlgo->xUpdate(&sCtx,zIpad,(unsigned int)nBlock);` |
|     15 | 5373 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     15 | 5374 | `	pAlgo->xFinal(&sCtx,zInner);` |
|      - | 5375 | `	/* out = H((key ^ opad) \|\| inner) */` |
|     15 | 5376 | `	pAlgo->xInit(&sCtx);` |
|     15 | 5377 | `	pAlgo->xUpdate(&sCtx,zOpad,(unsigned int)nBlock);` |
|     15 | 5378 | `	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)nDigest);` |
|     15 | 5379 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     15 | 5380 | `	if( raw_output ){` |
|      3 | 5381 | `		ph7_result_string(pCtx,(const char *)zDigest,nDigest);` |
|      2 | 5382 | `	}else{` |
|     13 | 5383 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);` |
|      - | 5384 | `	}` |
|     15 | 5385 | `	return PH7_OK;` |
|     10 | 5386 | `}` |
|      - | 5387 | `/*` |
|      - | 5388 | ` * bool hash_equals(string $known_string,string $user_string)` |
|      - | 5389 | ` *   Timing-attack-safe string comparison.` |
|      - | 5390 | ` */` |
|     12 | 5391 | `static int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5392 | `{` |
|      - | 5393 | `	const char *zKnown,*zUser;` |
|      - | 5394 | `	int nKnown,nUser,i;` |
|     14 | 5395 | `	volatile unsigned char vDiff = 0;` |
|     14 | 5396 | `	if( nArg < 2 ){` |
|    ! 0 | 5397 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5398 | `			"hash_equals() expects exactly 2 arguments, %d given",nArg);` |
|      - | 5399 | `	}` |
|     14 | 5400 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      4 | 5401 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5402 | `			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",` |
|      1 | 5403 | `			ph7_type_name(apArg[0]));` |
|      - | 5404 | `	}` |
|     11 | 5405 | `	if( !ph7_value_is_string(apArg[1]) ){` |
|    ! 0 | 5406 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5407 | `			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",` |
|    ! 0 | 5408 | `			ph7_type_name(apArg[1]));` |
|      - | 5409 | `	}` |
|     11 | 5410 | `	zKnown = ph7_value_to_string(apArg[0],&nKnown);` |
|     11 | 5411 | `	zUser = ph7_value_to_string(apArg[1],&nUser);` |
|     11 | 5412 | `	if( nKnown != nUser ){` |
|      5 | 5413 | `		ph7_result_bool(pCtx,0);` |
|      5 | 5414 | `		return PH7_OK;` |
|      - | 5415 | `	}` |
|      - | 5416 | `	/* Constant-time: read every byte, never short-circuit. */` |
|     19 | 5417 | `	for( i = 0; i < nKnown; i++ ){` |
|     13 | 5418 | `		vDiff \|= (unsigned char)(zKnown[i] ^ zUser[i]);` |
|      7 | 5419 | `	}` |
|      7 | 5420 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|      7 | 5421 | `	return PH7_OK;` |
|      8 | 5422 | `}` |
|      - | 5423 | `/*` |
|      - | 5424 | ` * array hash_algos(void)` |
|      - | 5425 | ` *   Return a list of the registered hashing algorithms.` |
|      - | 5426 | ` */` |
|      2 | 5427 | `static int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5428 | `{` |
|      - | 5429 | `	ph7_value *pArray,*pValue;` |
|      - | 5430 | `	sxu32 i;` |
|      1 | 5431 | `	SXUNUSED(nArg);` |
|      1 | 5432 | `	SXUNUSED(apArg);` |
|      3 | 5433 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5434 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 5435 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 5436 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5437 | `		return PH7_OK;` |
|      - | 5438 | `	}` |
|     15 | 5439 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|     13 | 5440 | `		ph7_value_string(pValue,aHashAlgo[i].zName,-1);` |
|     13 | 5441 | `		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);` |
|     13 | 5442 | `		ph7_value_reset_string_cursor(pValue);` |
|      7 | 5443 | `	}` |
|      3 | 5444 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5445 | `	return PH7_OK;` |
|      2 | 5446 | `}` |
|      - | 5447 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 5448 | `/*` |
|      - | 5449 | ` * password_* (bcrypt). These live in ext/standard in real PHP — outside the` |
|      - | 5450 | ` * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.` |
|      - | 5451 | ` */` |
|      - | 5452 | `/*` |
|      - | 5453 | ` * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a` |
|      - | 5454 | ` * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).` |
|      - | 5455 | ` */` |
|     40 | 5456 | `static int BcryptParseHash(const char *zHash,int nHash,int *piCost)` |
|      1 | 5457 | `{` |
|      - | 5458 | `	int iCost;` |
|     40 | 5459 | `	if( nHash != 60 \|\| zHash[0] != '$' \|\| zHash[1] != '2' \|\| zHash[3] != '$'` |
|     29 | 5460 | `		\|\| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){` |
|     13 | 5461 | `		return FALSE;` |
|      - | 5462 | `	}` |
|     29 | 5463 | `	if( zHash[4] < '0' \|\| zHash[4] > '9' \|\| zHash[5] < '0' \|\| zHash[5] > '9' \|\| zHash[6] != '$' ){` |
|    ! 0 | 5464 | `		return FALSE;` |
|      - | 5465 | `	}` |
|     29 | 5466 | `	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');` |
|     29 | 5467 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      3 | 5468 | `		return FALSE;` |
|      - | 5469 | `	}` |
|     27 | 5470 | `	if( piCost ){ *piCost = iCost; }` |
|     27 | 5471 | `	return TRUE;` |
|     21 | 5472 | `}` |
|      - | 5473 | `/*` |
|      - | 5474 | ` * TRUE if the $algo argument selects bcrypt: null (PASSWORD_DEFAULT) or the` |
|      - | 5475 | ` * "2y" id (PASSWORD_BCRYPT/PASSWORD_DEFAULT). bcrypt is the only supported algo.` |
|      - | 5476 | ` */` |
|     20 | 5477 | `static int BcryptIsBcryptAlgo(ph7_value *pAlgo)` |
|      3 | 5478 | `{` |
|     23 | 5479 | `	if( ph7_value_is_null(pAlgo) ){` |
|    ! 0 | 5480 | `		return TRUE;` |
|      - | 5481 | `	}` |
|     23 | 5482 | `	if( ph7_value_is_string(pAlgo) ){` |
|      - | 5483 | `		int nAlgo;` |
|     23 | 5484 | `		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);` |
|     23 | 5485 | `		return ( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' );` |
|      - | 5486 | `	}` |
|    ! 0 | 5487 | `	return FALSE;` |
|     13 | 5488 | `}` |
|      - | 5489 | `/*` |
|      - | 5490 | ` * bool\|string password_hash(string $password,string\|int\|null $algo[,array $options])` |
|      - | 5491 | ` *  Create a bcrypt hash of the password.` |
|      - | 5492 | ` */` |
|     16 | 5493 | `static int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 5494 | `{` |
|      - | 5495 | `	const char *zPwd;` |
|     19 | 5496 | `	int nPwd,iCost = 12;` |
|      - | 5497 | `	unsigned char aSalt[16];` |
|      - | 5498 | `	char zHash[60];` |
|     19 | 5499 | `	if( nArg < 2 ){` |
|    ! 0 | 5500 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5501 | `			"password_hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5502 | `	}` |
|     19 | 5503 | `	if( !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      3 | 5504 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5505 | `			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");` |
|      - | 5506 | `	}` |
|      - | 5507 | `	/* cost from $options['cost'] (default 12). */` |
|     16 | 5508 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|     14 | 5509 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|     14 | 5510 | `		if( pCost ){ iCost = ph7_value_to_int(pCost); }` |
|      6 | 5511 | `	}` |
|     16 | 5512 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      4 | 5513 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 5514 | `			"Invalid bcrypt cost parameter specified: %d",iCost);` |
|      - | 5515 | `	}` |
|     13 | 5516 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     13 | 5517 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 5518 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - | 5519 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|      - | 5520 | `	}` |
|     13 | 5521 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){` |
|    ! 0 | 5522 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5523 | `		return PH7_OK;` |
|      - | 5524 | `	}` |
|     13 | 5525 | `	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));` |
|     13 | 5526 | `	return PH7_OK;` |
|     11 | 5527 | `}` |
|      - | 5528 | `/*` |
|      - | 5529 | ` * bool password_verify(string $password,string $hash)` |
|      - | 5530 | ` *  Verify a password against a bcrypt hash. Never throws on a malformed hash.` |
|      - | 5531 | ` */` |
|     28 | 5532 | `static int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5533 | `{` |
|      - | 5534 | `	const char *zPwd,*zHash;` |
|      - | 5535 | `	int nPwd,nHash,iCost,i;` |
|      - | 5536 | `	unsigned char aSalt[16];` |
|      - | 5537 | `	char zComputed[60];` |
|     29 | 5538 | `	volatile unsigned char vDiff = 0;` |
|     29 | 5539 | `	if( nArg < 2 ){` |
|    ! 0 | 5540 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5541 | `			"password_verify() expects exactly 2 arguments, %d given",nArg);` |
|      - | 5542 | `	}` |
|     29 | 5543 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     29 | 5544 | `	zHash = ph7_value_to_string(apArg[1],&nHash);` |
|     29 | 5545 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|     11 | 5546 | `		ph7_result_bool(pCtx,0);` |
|     11 | 5547 | `		return PH7_OK;` |
|      - | 5548 | `	}` |
|      - | 5549 | `	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */` |
|     19 | 5550 | `	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 5551 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5552 | `		return PH7_OK;` |
|      - | 5553 | `	}` |
|     19 | 5554 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){` |
|    ! 0 | 5555 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5556 | `		return PH7_OK;` |
|      - | 5557 | `	}` |
|      - | 5558 | `	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps` |
|      - | 5559 | `	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */` |
|    577 | 5560 | `	for( i = 29; i < 60; i++ ){` |
|    559 | 5561 | `		vDiff \|= (unsigned char)(zComputed[i] ^ zHash[i]);` |
|    280 | 5562 | `	}` |
|     19 | 5563 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|     19 | 5564 | `	return PH7_OK;` |
|     15 | 5565 | `}` |
|      - | 5566 | `/*` |
|      - | 5567 | ` * array password_get_info(string $hash)` |
|      - | 5568 | ` *  Return ["algo"=>id\|null, "algoName"=>name, "options"=>[...]].` |
|      - | 5569 | ` */` |
|      6 | 5570 | `static int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5571 | `{` |
|      7 | 5572 | `	const char *zHash = "";` |
|      7 | 5573 | `	int nHash,iCost = 0,bBcrypt = 0;` |
|      - | 5574 | `	ph7_value *pArray,*pOptions,*pVal;` |
|      7 | 5575 | `	if( nArg > 0 ){` |
|      7 | 5576 | `		zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 5577 | `		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);` |
|      3 | 5578 | `	}` |
|      7 | 5579 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 5580 | `	pOptions = ph7_context_new_array(pCtx);` |
|      7 | 5581 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      7 | 5582 | `	if( pArray == 0 \|\| pOptions == 0 \|\| pVal == 0 ){` |
|    ! 0 | 5583 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5584 | `		return PH7_OK;` |
|      - | 5585 | `	}` |
|      7 | 5586 | `	if( bBcrypt ){` |
|      5 | 5587 | `		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */` |
|      5 | 5588 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 5589 | `		ph7_value_reset_string_cursor(pVal);` |
|      5 | 5590 | `		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);` |
|      5 | 5591 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      5 | 5592 | `		ph7_value_int(pVal,iCost);` |
|      5 | 5593 | `		ph7_array_add_strkey_elem(pOptions,"cost",pVal);` |
|      3 | 5594 | `	}else{` |
|      3 | 5595 | `		ph7_value_null(pVal);                          /* algo => null */` |
|      3 | 5596 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      3 | 5597 | `		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);` |
|      3 | 5598 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      - | 5599 | `	}` |
|      7 | 5600 | `	ph7_array_add_strkey_elem(pArray,"options",pOptions);` |
|      7 | 5601 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 5602 | `	return PH7_OK;` |
|      4 | 5603 | `}` |
|      - | 5604 | `/*` |
|      - | 5605 | ` * bool password_needs_rehash(string $hash,string\|int\|null $algo[,array $options])` |
|      - | 5606 | ` *  True if the hash was not made with the given algo/options.` |
|      - | 5607 | ` */` |
|      6 | 5608 | `static int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5609 | `{` |
|      - | 5610 | `	const char *zHash;` |
|      7 | 5611 | `	int nHash,iCost = 0,iWantCost = 12;` |
|      7 | 5612 | `	if( nArg < 2 ){` |
|    ! 0 | 5613 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5614 | `			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5615 | `	}` |
|      7 | 5616 | `	zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 5617 | `	if( !BcryptParseHash(zHash,nHash,&iCost) \|\| !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      - | 5618 | `		/* A non-bcrypt hash, or a request for a different algo → needs rehash. */` |
|      3 | 5619 | `		ph7_result_bool(pCtx,1);` |
|      3 | 5620 | `		return PH7_OK;` |
|      - | 5621 | `	}` |
|      5 | 5622 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|      5 | 5623 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|      5 | 5624 | `		if( pCost ){ iWantCost = ph7_value_to_int(pCost); }` |
|      2 | 5625 | `	}` |
|      5 | 5626 | `	ph7_result_bool(pCtx,iCost != iWantCost);` |
|      5 | 5627 | `	return PH7_OK;` |
|      4 | 5628 | `}` |
|      - | 5629 | `/*` |
|      - | 5630 | ` * filter_var() — input validation and sanitization (the ext/filter API).` |
|      - | 5631 | ` *` |
|      - | 5632 | ` * Filter and flag identifiers (values match PHP 8.5; the constants themselves` |
|      - | 5633 | ` * are registered in constant.c). The validate filters are hand-rolled rather` |
|      - | 5634 | ` * than delegating to SyStrToInt64/SyStrToReal: the former silently skips leading` |
|      - | 5635 | ` * zeros and cannot signal overflow, and the latter treats ',' as a decimal point` |
|      - | 5636 | ` * unconditionally — neither matches PHP's filter semantics.` |
|      - | 5637 | ` */` |
|      - | 5638 | `#define FV_VALIDATE_INT     257` |
|      - | 5639 | `#define FV_VALIDATE_BOOLEAN 258` |
|      - | 5640 | `#define FV_VALIDATE_FLOAT   259` |
|      - | 5641 | `#define FV_VALIDATE_REGEXP  272` |
|      - | 5642 | `#define FV_VALIDATE_URL     273` |
|      - | 5643 | `#define FV_VALIDATE_EMAIL   274` |
|      - | 5644 | `#define FV_VALIDATE_IP      275` |
|      - | 5645 | `#define FV_VALIDATE_MAC     276` |
|      - | 5646 | `#define FV_VALIDATE_DOMAIN  277` |
|      - | 5647 | `#define FV_SANITIZE_SPECIAL_CHARS      515` |
|      - | 5648 | `#define FV_DEFAULT          516 /* == FILTER_UNSAFE_RAW: pass the value through */` |
|      - | 5649 | `#define FV_SANITIZE_EMAIL   517` |
|      - | 5650 | `#define FV_SANITIZE_URL     518` |
|      - | 5651 | `#define FV_SANITIZE_NUMBER_INT   519` |
|      - | 5652 | `#define FV_SANITIZE_NUMBER_FLOAT 520` |
|      - | 5653 | `#define FV_SANITIZE_FULL_SPECIAL_CHARS 522` |
|      - | 5654 | `#define FV_FLAG_ALLOW_OCTAL  1` |
|      - | 5655 | `#define FV_FLAG_ALLOW_HEX    2` |
|      - | 5656 | `#define FV_FLAG_STRIP_LOW    4` |
|      - | 5657 | `#define FV_FLAG_STRIP_HIGH   8` |
|      - | 5658 | `#define FV_FLAG_ENCODE_LOW   16` |
|      - | 5659 | `#define FV_FLAG_ENCODE_HIGH  32` |
|      - | 5660 | `#define FV_FLAG_ENCODE_AMP   64` |
|      - | 5661 | `#define FV_FLAG_NO_ENCODE_QUOTES 128` |
|      - | 5662 | `#define FV_FLAG_STRIP_BACKTICK   512` |
|      - | 5663 | `#define FV_FLAG_ALLOW_FRACTION   4096` |
|      - | 5664 | `#define FV_FLAG_ALLOW_THOUSAND   8192` |
|      - | 5665 | `#define FV_FLAG_ALLOW_SCIENTIFIC 16384` |
|      - | 5666 | `#define FV_FLAG_IPV4  1048576` |
|      - | 5667 | `#define FV_FLAG_IPV6  2097152` |
|      - | 5668 | `#define FV_NULL_ON_FAILURE 134217728` |
|      - | 5669 | `/* The subset of flags the UNSAFE_RAW/DEFAULT string filter (FvSanitizeString)` |
|      - | 5670 | ` * acts on: when none are set the filter is a verbatim pass-through, so FV_DEFAULT` |
|      - | 5671 | ` * can shortcut. Keep this in sync with FvSanitizeString's flag handling. */` |
|      - | 5672 | `#define FV_FLAG_STRING_MASK (FV_FLAG_STRIP_LOW\|FV_FLAG_STRIP_HIGH\|FV_FLAG_STRIP_BACKTICK \` |
|      - | 5673 | `                            \|FV_FLAG_ENCODE_LOW\|FV_FLAG_ENCODE_HIGH\|FV_FLAG_ENCODE_AMP)` |
|      - | 5674 |  |
|      - | 5675 | `/* Trim leading/trailing PHP whitespace, adjusting the (*pz,*pn) view in place.` |
|      - | 5676 | ` * SyisSpace (isspace) matches PHP's filter whitespace set " \t\n\r\v\f". */` |
|    153 | 5677 | `static void FvTrim(const char **pz,int *pn){` |
|    153 | 5678 | `	const char *z = *pz;` |
|    153 | 5679 | `	int n = *pn;` |
|    157 | 5680 | `	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }` |
|    161 | 5681 | `	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }` |
|    153 | 5682 | `	*pz = z; *pn = n;` |
|    153 | 5683 | `}` |
|      - | 5684 | `/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     57 | 5685 | `static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){` |
|     57 | 5686 | `	int neg = 0, i;` |
|     57 | 5687 | `	sxu64 u = 0;` |
|     57 | 5688 | `	FvTrim(&z,&n);` |
|     57 | 5689 | `	if( n==0 ){ return 0; }` |
|     51 | 5690 | `	if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }` |
|     51 | 5691 | `	if( n==0 ){ return 0; }` |
|     49 | 5692 | `	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'\|\|z[1]=='X') ){` |
|      3 | 5693 | `		z += 2; n -= 2;` |
|      3 | 5694 | `		if( n==0 ){ return 0; }` |
|      7 | 5695 | `		for( i=0; i<n; i++ ){` |
|      5 | 5696 | `			int h = SyHexToint((unsigned char)z[i]);` |
|      5 | 5697 | `			if( h<0 ){ return 0; }` |
|      5 | 5698 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }` |
|      5 | 5699 | `			u = u*16 + (sxu64)h;` |
|      3 | 5700 | `		}` |
|     48 | 5701 | `	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){` |
|      9 | 5702 | `		for( i=0; i<n; i++ ){` |
|      7 | 5703 | `			if( z[i]<'0' \|\| z[i]>'7' ){ return 0; }` |
|      7 | 5704 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }` |
|      7 | 5705 | `			u = u*8 + (sxu64)(z[i]-'0');` |
|      4 | 5706 | `		}` |
|      2 | 5707 | `	}else{` |
|     45 | 5708 | `		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */` |
|    201 | 5709 | `		for( i=0; i<n; i++ ){` |
|    173 | 5710 | `			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|    161 | 5711 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }` |
|    161 | 5712 | `			u = u*10 + (sxu64)(z[i]-'0');` |
|     81 | 5713 | `		}` |
|      - | 5714 | `	}` |
|     33 | 5715 | `	if( neg ){` |
|      5 | 5716 | `		if( u > 0x8000000000000000ULL ){ return 0; }` |
|      5 | 5717 | `		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */` |
|      3 | 5718 | `	}else{` |
|     29 | 5719 | `		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }` |
|     27 | 5720 | `		*pOut = (ph7_int64)u;` |
|      - | 5721 | `	}` |
|     31 | 5722 | `	return 1;` |
|     29 | 5723 | `}` |
|      - | 5724 | `/* FILTER_VALIDATE_FLOAT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     69 | 5725 | `static int FvValidateFloat(const char *z,int n,int flags,double *pOut){` |
|      - | 5726 | `	char zBuf[512];` |
|     69 | 5727 | `	int i, m = 0, seenDigit = 0;` |
|     69 | 5728 | `	const char *zv; int nv; double d = 0;` |
|     69 | 5729 | `	FvTrim(&z,&n);` |
|      - | 5730 | `	/* Bound the input: zBuf[512] holds the thousand-separator-stripped copy, and` |
|      - | 5731 | `	 * the cap also rejects the pathological 500+ digit floats PHP refuses. */` |
|     69 | 5732 | `	if( n==0 \|\| n>500 ){ return 0; }` |
|     69 | 5733 | `	if( flags & FV_FLAG_ALLOW_THOUSAND ){` |
|      - | 5734 | `		/* Commas are optional, but when present they must group the integer part` |
|      - | 5735 | `		 * into a leading run of 1..3 digits followed by groups of exactly 3` |
|      - | 5736 | `		 * ("1,000" ok, "1,5"/"1234,567" rejected). Strip them into zBuf and reject` |
|      - | 5737 | `		 * a comma anywhere in the fractional/exponent tail. */` |
|     25 | 5738 | `		int s = 0, intEnd, segStart, segIdx, hasComma = 0;` |
|     25 | 5739 | `		if( s<n && (z[s]=='+'\|\|z[s]=='-') ){ zBuf[m++] = z[s]; s++; }` |
|     25 | 5740 | `		intEnd = s;` |
|    167 | 5741 | `		while( intEnd<n && z[intEnd]!='.' && z[intEnd]!='e' && z[intEnd]!='E' ){` |
|    143 | 5742 | `			if( z[intEnd]==',' ){ hasComma = 1; }` |
|    143 | 5743 | `			intEnd++;` |
|      1 | 5744 | `		}` |
|     25 | 5745 | `		if( hasComma ){` |
|     25 | 5746 | `			segStart = s; segIdx = 0;` |
|    165 | 5747 | `			for( i=s; i<=intEnd; i++ ){` |
|    151 | 5748 | `				if( i==intEnd \|\| z[i]==',' ){` |
|     49 | 5749 | `					int segLen = i - segStart, k;` |
|     49 | 5750 | `					if( segIdx==0 ){ if( segLen<1 \|\| segLen>3 ){ return 0; } }` |
|     25 | 5751 | `					else if( segLen!=3 ){ return 0; }` |
|    119 | 5752 | `					for( k=segStart; k<i; k++ ){` |
|     81 | 5753 | `						if( !SyisDigit((unsigned char)z[k]) ){ return 0; }` |
|     81 | 5754 | `						zBuf[m++] = z[k];` |
|     41 | 5755 | `					}` |
|     39 | 5756 | `					segStart = i+1; segIdx++;` |
|     19 | 5757 | `				}` |
|     71 | 5758 | `			}` |
|      8 | 5759 | `		}else{` |
|    ! 0 | 5760 | `			for( i=s; i<intEnd; i++ ){ zBuf[m++] = z[i]; }` |
|      - | 5761 | `		}` |
|     27 | 5762 | `		for( i=intEnd; i<n; i++ ){` |
|     13 | 5763 | `			if( z[i]==',' ){ return 0; }` |
|     13 | 5764 | `			zBuf[m++] = z[i];` |
|      7 | 5765 | `		}` |
|     15 | 5766 | `		zv = zBuf; nv = m;` |
|      8 | 5767 | `	}else{` |
|     45 | 5768 | `		zv = z; nv = n;` |
|      - | 5769 | `	}` |
|     59 | 5770 | `	i = 0;` |
|     59 | 5771 | `	if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|    167 | 5772 | `	while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     59 | 5773 | `	if( i<nv && zv[i]=='.' ){` |
|     21 | 5774 | `		i++;` |
|     39 | 5775 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     10 | 5776 | `	}` |
|     59 | 5777 | `	if( !seenDigit ){ return 0; }` |
|     57 | 5778 | `	if( i<nv && (zv[i]=='e'\|\|zv[i]=='E') ){` |
|     29 | 5779 | `		i++;` |
|     29 | 5780 | `		if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|     29 | 5781 | `		if( i>=nv \|\| !SyisDigit((unsigned char)zv[i]) ){ return 0; }` |
|    105 | 5782 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; }` |
|     14 | 5783 | `	}` |
|     57 | 5784 | `	if( i!=nv ){ return 0; } /* trailing junk */` |
|      - | 5785 | `	/* The grammar above guarantees zv[0..nv) is a clean ASCII decimal float (no hex /` |
|      - | 5786 | `	 * inf / nan / trailing junk), so it is safe to hand to libc strtod, which — unlike` |
|      - | 5787 | `	 * SyStrToReal (15 sig-digits + exponent clamped to 308, so it silently saturates` |
|      - | 5788 | `	 * overflowing magnitudes to a finite value) — is overflow/underflow-aware and` |
|      - | 5789 | `	 * correctly rounded. strtod needs a NUL-terminated string: the ALLOW_THOUSAND path` |
|      - | 5790 | `	 * already built the span in zBuf (zv==zBuf); the plain path must copy it there (z is` |
|      - | 5791 | `	 * const + not NUL-terminated). nv <= n <= 500 < sizeof(zBuf) by the cap above.` |
|      - | 5792 | `	 * Matches PHP 8.5 byte-for-byte: reject overflow (-> +/-INF) and total underflow` |
|      - | 5793 | `	 * (-> 0.0), keep subnormals (nonzero, errno==ERANGE) and a genuine "0" (errno==0). */` |
|     53 | 5794 | `	if( zv != zBuf ){ SyMemcpy(zv,zBuf,(sxu32)nv); }` |
|     53 | 5795 | `	zBuf[nv] = 0;` |
|     53 | 5796 | `	errno = 0;` |
|     53 | 5797 | `	d = strtod(zBuf,0);` |
|     53 | 5798 | `	if( errno == ERANGE && (d == HUGE_VAL \|\| d == -HUGE_VAL \|\| d == 0.0) ){` |
|     15 | 5799 | `		return 0;` |
|      - | 5800 | `	}` |
|     39 | 5801 | `	*pOut = d;` |
|     39 | 5802 | `	return 1;` |
|     35 | 5803 | `}` |
|      - | 5804 | `/* FILTER_VALIDATE_BOOLEAN. Returns 1 if the string is recognized (sets *pBool),` |
|      - | 5805 | ` * 0 if it is unrecognized (the failure path). "0"/"false"/"" are recognized as` |
|      - | 5806 | ` * false, NOT failures. */` |
|     33 | 5807 | `static int FvValidateBool(const char *z,int n,int *pBool){` |
|     33 | 5808 | `	FvTrim(&z,&n);` |
|     32 | 5809 | `	if( (n==1 && z[0]=='1') \|\| (n==4 && SyStrnicmp(z,"true",4)==0)` |
|     25 | 5810 | `	    \|\| (n==2 && SyStrnicmp(z,"on",2)==0) \|\| (n==3 && SyStrnicmp(z,"yes",3)==0) ){` |
|     11 | 5811 | `		*pBool = 1; return 1;` |
|      - | 5812 | `	}` |
|     22 | 5813 | `	if( n==0 \|\| (n==1 && z[0]=='0') \|\| (n==5 && SyStrnicmp(z,"false",5)==0)` |
|     11 | 5814 | `	    \|\| (n==3 && SyStrnicmp(z,"off",3)==0) \|\| (n==2 && SyStrnicmp(z,"no",2)==0) ){` |
|     11 | 5815 | `		*pBool = 0; return 1;` |
|      - | 5816 | `	}` |
|      9 | 5817 | `	return 0;` |
|     15 | 5818 | `}` |
|      - | 5819 | `/* IPv4 dotted-quad: exactly 4 octets 0..255, no leading zeros. */` |
|     33 | 5820 | `static int FvValidateIp4(const char *z,int n){` |
|     33 | 5821 | `	int i = 0, parts = 0;` |
|     77 | 5822 | `	while( i<n ){` |
|     65 | 5823 | `		int val = 0, digits = 0, start = i;` |
|    143 | 5824 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){` |
|     85 | 5825 | `			val = val*10 + (z[i]-'0');` |
|     85 | 5826 | `			if( val>255 ){ return 0; }` |
|     79 | 5827 | `			digits++; i++;` |
|      1 | 5828 | `		}` |
|     59 | 5829 | `		if( digits==0 \|\| digits>3 ){ return 0; }` |
|     49 | 5830 | `		if( digits>1 && z[start]=='0' ){ return 0; } /* leading zero */` |
|     45 | 5831 | `		parts++;` |
|     45 | 5832 | `		if( parts>4 ){ return 0; }` |
|     45 | 5833 | `		if( i<n ){` |
|     33 | 5834 | `			if( z[i]!='.' ){ return 0; }` |
|     33 | 5835 | `			i++;` |
|     33 | 5836 | `			if( i>=n ){ return 0; } /* trailing dot */` |
|     16 | 5837 | `		}` |
|      1 | 5838 | `	}` |
|     13 | 5839 | `	return parts==4;` |
|     17 | 5840 | `}` |
|      - | 5841 | `/* A colon-separated run of IPv6 hextets with no "::" (n may be 0 -> 0 groups),` |
|      - | 5842 | ` * allowing a trailing embedded IPv4. Returns the 16-bit group count or -1. */` |
|     19 | 5843 | `static int FvIp6Hextets(const char *z,int n){` |
|     19 | 5844 | `	int i = 0, segStart = 0, groups = 0;` |
|     19 | 5845 | `	if( n==0 ){ return 0; }` |
|    145 | 5846 | `	while( i<=n ){` |
|    133 | 5847 | `		if( i==n \|\| z[i]==':' ){` |
|     23 | 5848 | `			int segLen = i - segStart, j, isV4 = 0;` |
|     23 | 5849 | `			if( segLen==0 ){ return -1; } /* an empty hextet (stray ':') */` |
|     77 | 5850 | `			for( j=segStart; j<i; j++ ){ if( z[j]=='.' ){ isV4 = 1; break; } }` |
|     23 | 5851 | `			if( isV4 ){` |
|     11 | 5852 | `				if( i!=n ){ return -1; } /* IPv4 only as the final token */` |
|     11 | 5853 | `				if( !FvValidateIp4(z+segStart,segLen) ){ return -1; }` |
|      5 | 5854 | `				groups += 2;` |
|      3 | 5855 | `			}else{` |
|     13 | 5856 | `				if( segLen>4 ){ return -1; }` |
|     47 | 5857 | `				for( j=segStart; j<i; j++ ){ if( SyHexToint((unsigned char)z[j])<0 ){ return -1; } }` |
|     13 | 5858 | `				groups++;` |
|      - | 5859 | `			}` |
|     17 | 5860 | `			segStart = i+1;` |
|      8 | 5861 | `		}` |
|    127 | 5862 | `		i++;` |
|      1 | 5863 | `	}` |
|     13 | 5864 | `	return groups;` |
|     10 | 5865 | `}` |
|      - | 5866 | `/* IPv6: at most one "::" zero-run; 8 groups exactly, or fewer when "::" present. */` |
|     19 | 5867 | `static int FvValidateIp6(const char *z,int n){` |
|     19 | 5868 | `	const char *zDbl = 0;` |
|      - | 5869 | `	int i, ga, gb;` |
|    139 | 5870 | `	for( i=0; i+1<n; i++ ){` |
|    123 | 5871 | `		if( z[i]==':' && z[i+1]==':' ){` |
|     13 | 5872 | `			if( zDbl ){ return 0; } /* a second "::" is invalid */` |
|     11 | 5873 | `			zDbl = z+i;` |
|      5 | 5874 | `		}` |
|     61 | 5875 | `	}` |
|     17 | 5876 | `	if( zDbl==0 ){` |
|      9 | 5877 | `		return FvIp6Hextets(z,n)==8;` |
|    ! 0 | 5878 | `	}else{` |
|      9 | 5879 | `		int lenA = (int)(zDbl - z);` |
|      9 | 5880 | `		int lenB = n - lenA - 2;` |
|      9 | 5881 | `		ga = (lenA==0) ? 0 : FvIp6Hextets(z,lenA);` |
|      9 | 5882 | `		gb = (lenB==0) ? 0 : FvIp6Hextets(zDbl+2,lenB);` |
|      9 | 5883 | `		if( ga<0 \|\| gb<0 ){ return 0; }` |
|      9 | 5884 | `		return (ga+gb)<=7; /* "::" stands for at least one zero group */` |
|      - | 5885 | `	}` |
|     10 | 5886 | `}` |
|     25 | 5887 | `static int FvValidateIp(const char *z,int n,int flags){` |
|     25 | 5888 | `	int v4 = (flags & FV_FLAG_IPV4), v6 = (flags & FV_FLAG_IPV6);` |
|     25 | 5889 | `	if( !v4 && !v6 ){ v4 = v6 = 1; } /* default accepts either family */` |
|     25 | 5890 | `	if( v4 && FvValidateIp4(z,n) ){ return 1; }` |
|     21 | 5891 | `	if( v6 && FvValidateIp6(z,n) ){ return 1; }` |
|     13 | 5892 | `	return 0;` |
|     13 | 5893 | `}` |
|      - | 5894 | `/* FILTER_VALIDATE_MAC: 17-char colon- or dash-separated hex (XX:XX:..:XX). */` |
|     11 | 5895 | `static int FvValidateMac(const char *z,int n){` |
|      - | 5896 | `	char sep;` |
|      - | 5897 | `	int i;` |
|     11 | 5898 | `	if( n!=17 ){ return 0; }` |
|      7 | 5899 | `	sep = z[2];` |
|      7 | 5900 | `	if( sep!=':' && sep!='-' ){ return 0; }` |
|    105 | 5901 | `	for( i=0; i<17; i++ ){` |
|    101 | 5902 | `		if( (i%3)==2 ){ if( z[i]!=sep ){ return 0; } }` |
|     71 | 5903 | `		else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }` |
|     50 | 5904 | `	}` |
|      5 | 5905 | `	return 1;` |
|      6 | 5906 | `}` |
|      - | 5907 | `/* FILTER_VALIDATE_EMAIL (best-effort: covers the common cases, not quoted local` |
|      - | 5908 | ` * parts or IP-literal domains). */` |
|     28 | 5909 | `static int FvValidateEmail(const char *z,int n){` |
|     28 | 5910 | `	int at = -1, i, localLen, domLen, labelStart, dotCount = 0;` |
|      - | 5911 | `	const char *zDom;` |
|     28 | 5912 | `	if( n==0 \|\| n>320 ){ return 0; }` |
|    201 | 5913 | `	for( i=0; i<n; i++ ){` |
|    181 | 5914 | `		if( z[i]=='@' ){ if( at>=0 ){ return 0; } at = i; }` |
|     91 | 5915 | `	}` |
|     21 | 5916 | `	if( at<=0 \|\| at==n-1 ){ return 0; } /* one '@', non-empty local and domain */` |
|     21 | 5917 | `	localLen = at;` |
|     21 | 5918 | `	zDom = z + at + 1;` |
|     21 | 5919 | `	domLen = n - at - 1;` |
|     21 | 5920 | `	if( z[0]=='.' \|\| z[at-1]=='.' ){ return 0; }` |
|     57 | 5921 | `	for( i=0; i<localLen; i++ ){` |
|     43 | 5922 | `		unsigned char c = (unsigned char)z[i];` |
|     43 | 5923 | `		if( c<=' ' ){ return 0; }` |
|     41 | 5924 | `		if( c=='.' && i+1<localLen && z[i+1]=='.' ){ return 0; }` |
|     20 | 5925 | `	}` |
|     15 | 5926 | `	if( zDom[0]=='.' \|\| zDom[domLen-1]=='.' ){ return 0; }` |
|     13 | 5927 | `	labelStart = 0;` |
|     85 | 5928 | `	for( i=0; i<=domLen; i++ ){` |
|     75 | 5929 | `		if( i==domLen \|\| zDom[i]=='.' ){` |
|     25 | 5930 | `			int ll = i - labelStart;` |
|     25 | 5931 | `			if( ll==0 ){ return 0; } /* consecutive dots */` |
|     23 | 5932 | `			if( zDom[labelStart]=='-' \|\| zDom[i-1]=='-' ){ return 0; }` |
|     23 | 5933 | `			if( i<domLen ){ dotCount++; }` |
|     23 | 5934 | `			labelStart = i+1;` |
|     12 | 5935 | `		}else{` |
|     51 | 5936 | `			unsigned char c = (unsigned char)zDom[i];` |
|     51 | 5937 | `			if( !((c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9')\|\|c=='-') ){ return 0; }` |
|      - | 5938 | `		}` |
|     37 | 5939 | `	}` |
|     11 | 5940 | `	if( dotCount<1 ){ return 0; } /* PHP requires a dot in the domain (any TLD length) */` |
|      9 | 5941 | `	return 1;` |
|     15 | 5942 | `}` |
|      - | 5943 | `/* FILTER_VALIDATE_DOMAIN (lenient, matching PHP without FILTER_FLAG_HOSTNAME). */` |
|     11 | 5944 | `static int FvValidateDomain(const char *z,int n){` |
|      - | 5945 | `	int i;` |
|     11 | 5946 | `	if( n<1 \|\| n>253 \|\| z[0]=='.' ){ return 0; }` |
|     81 | 5947 | `	for( i=0; i<n; i++ ){` |
|     75 | 5948 | `		unsigned char c = (unsigned char)z[i];` |
|     75 | 5949 | `		if( c<=' ' ){ return 0; }` |
|     75 | 5950 | `		if( c=='.' && i+1<n && z[i+1]=='.' ){ return 0; }` |
|     37 | 5951 | `	}` |
|      7 | 5952 | `	return 1;` |
|      6 | 5953 | `}` |
|      - | 5954 | `/* FILTER_VALIDATE_URL: require a scheme and a host (PHP's filter is itself` |
|      - | 5955 | ` * parse_url-based, so PH7_VmHttpSplitURI tracks it closely). */` |
|     15 | 5956 | `static int FvValidateUrl(const char *z,int n){` |
|      - | 5957 | `	SyhttpUri sUri;` |
|     15 | 5958 | `	if( n==0 ){ return 0; }` |
|     15 | 5959 | `	SyZero(&sUri,(sxu32)sizeof(sUri));` |
|     15 | 5960 | `	if( PH7_VmHttpSplitURI(&sUri,z,(sxu32)n)!=SXRET_OK ){ return 0; }` |
|     15 | 5961 | `	return sUri.sScheme.nByte!=0 && sUri.sHost.nByte!=0;` |
|      8 | 5962 | `}` |
|      - | 5963 | `/* The Fv sanitizers build their result by appending directly to the call` |
|      - | 5964 | ` * context (ph7_result_string accumulates, like htmlspecialchars), emitting each` |
|      - | 5965 | ` * kept run in one call and seeding "" so an all-stripped input yields "". */` |
|      - | 5966 | `/* SANITIZE_NUMBER_INT (isFloat=0) / SANITIZE_NUMBER_FLOAT (isFloat=1). */` |
|     37 | 5967 | `static void FvSanitizeNumber(ph7_context *pCtx,const char *z,int n,int isFloat,int flags){` |
|     37 | 5968 | `	int i, runStart = 0;` |
|     37 | 5969 | `	ph7_result_string(pCtx,"",0);` |
|     97 | 5970 | `	for( i=0; i<n; i++ ){` |
|     91 | 5971 | `		char c = z[i];` |
|     91 | 5972 | `		int keep = (c>='0'&&c<='9') \|\| c=='+' \|\| c=='-';` |
|     91 | 5973 | `		if( !keep && isFloat ){` |
|     38 | 5974 | `			keep = (c=='.' && (flags & FV_FLAG_ALLOW_FRACTION))` |
|     23 | 5975 | `			    \|\| (c==',' && (flags & FV_FLAG_ALLOW_THOUSAND))` |
|     36 | 5976 | `			    \|\| ((c=='e'\|\|c=='E') && (flags & FV_FLAG_ALLOW_SCIENTIFIC));` |
|     12 | 5977 | `		}` |
|     61 | 5978 | `		if( !keep ){` |
|     33 | 5979 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     33 | 5980 | `			runStart = i+1;` |
|     16 | 5981 | `		}` |
|     31 | 5982 | `	}` |
|      7 | 5983 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 | 5984 | `}` |
|      - | 5985 | `/* Return non-zero when byte c must be stripped under the STRIP_* flags. Shared` |
|      - | 5986 | ` * by the UNSAFE_RAW string filter and SANITIZE_SPECIAL_CHARS. STRIP_LOW drops` |
|      - | 5987 | `` * bytes <32, STRIP_HIGH drops bytes >=127 (incl. DEL), STRIP_BACKTICK drops '`'.`` |
|      - | 5988 | ` * Matches php_filter_strip(); verified byte-exact vs php 8.5.7. */` |
|    287 | 5989 | `static int FvStripByte(unsigned char c,int flags){` |
|    287 | 5990 | `	if( (flags & FV_FLAG_STRIP_LOW)      && c<32 )    { return 1; }` |
|    281 | 5991 | `	if( (flags & FV_FLAG_STRIP_HIGH)     && c>=127 )  { return 1; }` |
|    269 | 5992 | `	if( (flags & FV_FLAG_STRIP_BACKTICK) && c==0x60 ) { return 1; }` |
|    267 | 5993 | `	return 0;` |
|    144 | 5994 | `}` |
|      - | 5995 | `/* FILTER_UNSAFE_RAW / FILTER_DEFAULT with flags: no default transform, but the` |
|      - | 5996 | ` * STRIP/ENCODE flags apply. Precedence (per php_filter_unsafe_raw, verified` |
|      - | 5997 | ` * vs php 8.5.7): a byte is first tested for stripping; a surviving byte is then` |
|      - | 5998 | ` * encoded as a decimal numeric entity if ENCODE_LOW (<32) / ENCODE_HIGH (>=127)` |
|      - | 5999 | ` * is set, and '&' becomes "&#38;" under ENCODE_AMP. So STRIP_LOW\|ENCODE_LOW` |
|      - | 6000 | ` * strips (nothing left to encode). Bytes are treated individually — ENCODE_HIGH` |
|      - | 6001 | ` * numeric-encodes each byte of a multibyte sequence separately, not the codepoint. */` |
|     25 | 6002 | `static void FvSanitizeString(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 6003 | `	int i, runStart = 0;` |
|     25 | 6004 | `	ph7_result_string(pCtx,"",0);` |
|    193 | 6005 | `	for( i=0; i<n; i++ ){` |
|    179 | 6006 | `		unsigned char c = (unsigned char)z[i];` |
|    179 | 6007 | `		if( FvStripByte(c,flags) ){` |
|     13 | 6008 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     13 | 6009 | `			runStart = i+1;` |
|     13 | 6010 | `			continue;` |
|      - | 6011 | `		}` |
|    167 | 6012 | `		if( c=='&' && (flags & FV_FLAG_ENCODE_AMP) ){` |
|      3 | 6013 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      3 | 6014 | `			ph7_result_string(pCtx,"&#38;",-1);` |
|      3 | 6015 | `			runStart = i+1;` |
|    166 | 6016 | `		}else if( (c<32 && (flags & FV_FLAG_ENCODE_LOW))` |
|    164 | 6017 | `		       \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     37 | 6018 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 6019 | `			ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|      9 | 6020 | `			runStart = i+1;` |
|      4 | 6021 | `		}` |
|     79 | 6022 | `	}` |
|     15 | 6023 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     15 | 6024 | `}` |
|      - | 6025 | `/* FILTER_SANITIZE_SPECIAL_CHARS: encode <>&"' and every control byte <32 as a` |
|      - | 6026 | ` * decimal numeric entity (&#60; &#38; &#34; ...). The STRIP_* flags remove bytes` |
|      - | 6027 | ` * before encoding; ENCODE_HIGH numeric-encodes surviving bytes >=127. Bytes >=128` |
|      - | 6028 | ` * are otherwise passed through verbatim (this filter is NOT UTF-8-aware — only the` |
|      - | 6029 | ` * FULL variant is). Byte-exact vs php 8.5.7. */` |
|     13 | 6030 | `static void FvSanitizeSpecial(ph7_context *pCtx,const char *z,int n,int flags){` |
|     13 | 6031 | `	int i, runStart = 0;` |
|      - | 6032 | `	const char *zEnt;` |
|     13 | 6033 | `	ph7_result_string(pCtx,"",0);` |
|    131 | 6034 | `	for( i=0; i<n; i++ ){` |
|    119 | 6035 | `		unsigned char c = (unsigned char)z[i];` |
|    119 | 6036 | `		if( FvStripByte(c,flags) ){` |
|      9 | 6037 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 6038 | `			runStart = i+1;` |
|      9 | 6039 | `			continue;` |
|      - | 6040 | `		}` |
|    111 | 6041 | `		switch( c ){` |
|      3 | 6042 | `		case '<':  zEnt = "&#60;"; break;` |
|      3 | 6043 | `		case '>':  zEnt = "&#62;"; break;` |
|     11 | 6044 | `		case '&':  zEnt = "&#38;"; break;` |
|      3 | 6045 | `		case '"':  zEnt = "&#34;"; break;` |
|      3 | 6046 | `		case '\'': zEnt = "&#39;"; break;` |
|     46 | 6047 | `		default:` |
|      - | 6048 | `			/* Control bytes <32 are always numeric-encoded; bytes >=127 only when` |
|      - | 6049 | `			 * ENCODE_HIGH is set. Everything else stays in the current run. */` |
|     93 | 6050 | `			if( c<32 \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     17 | 6051 | `				if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     17 | 6052 | `				ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|     17 | 6053 | `				runStart = i+1;` |
|      8 | 6054 | `			}` |
|     93 | 6055 | `			continue; /* keep in the current run */` |
|      - | 6056 | `		}` |
|     19 | 6057 | `		if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     19 | 6058 | `		ph7_result_string(pCtx,zEnt,-1); /* -1: length from strlen */` |
|     19 | 6059 | `		runStart = i+1;` |
|     10 | 6060 | `	}` |
|     13 | 6061 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     13 | 6062 | `}` |
|      - | 6063 | `/* HTML 4.01 named-entity table (codepoint -> "&name;") used by the UTF-8-aware` |
|      - | 6064 | ` * FULL_SPECIAL_CHARS filter, sorted ascending by codepoint for binary search.` |
|      - | 6065 | ` * Generated from php 8.5.7 (the exact set php_escape_html_entities emits for the` |
|      - | 6066 | ` * default document type); the five inline specials <>&"' are handled separately,` |
|      - | 6067 | ` * so every entry here is a codepoint >=0xA0. 248 rows. */` |
|      - | 6068 | `static const struct { sxu32 cp; const char *zEnt; } aHtml401Ent[] = {` |
|      - | 6069 | `	{0x00A0,"&nbsp;"},{0x00A1,"&iexcl;"},{0x00A2,"&cent;"},{0x00A3,"&pound;"},` |
|      - | 6070 | `	{0x00A4,"&curren;"},{0x00A5,"&yen;"},{0x00A6,"&brvbar;"},{0x00A7,"&sect;"},` |
|      - | 6071 | `	{0x00A8,"&uml;"},{0x00A9,"&copy;"},{0x00AA,"&ordf;"},{0x00AB,"&laquo;"},` |
|      - | 6072 | `	{0x00AC,"&not;"},{0x00AD,"&shy;"},{0x00AE,"&reg;"},{0x00AF,"&macr;"},` |
|      - | 6073 | `	{0x00B0,"&deg;"},{0x00B1,"&plusmn;"},{0x00B2,"&sup2;"},{0x00B3,"&sup3;"},` |
|      - | 6074 | `	{0x00B4,"&acute;"},{0x00B5,"&micro;"},{0x00B6,"&para;"},{0x00B7,"&middot;"},` |
|      - | 6075 | `	{0x00B8,"&cedil;"},{0x00B9,"&sup1;"},{0x00BA,"&ordm;"},{0x00BB,"&raquo;"},` |
|      - | 6076 | `	{0x00BC,"&frac14;"},{0x00BD,"&frac12;"},{0x00BE,"&frac34;"},{0x00BF,"&iquest;"},` |
|      - | 6077 | `	{0x00C0,"&Agrave;"},{0x00C1,"&Aacute;"},{0x00C2,"&Acirc;"},{0x00C3,"&Atilde;"},` |
|      - | 6078 | `	{0x00C4,"&Auml;"},{0x00C5,"&Aring;"},{0x00C6,"&AElig;"},{0x00C7,"&Ccedil;"},` |
|      - | 6079 | `	{0x00C8,"&Egrave;"},{0x00C9,"&Eacute;"},{0x00CA,"&Ecirc;"},{0x00CB,"&Euml;"},` |
|      - | 6080 | `	{0x00CC,"&Igrave;"},{0x00CD,"&Iacute;"},{0x00CE,"&Icirc;"},{0x00CF,"&Iuml;"},` |
|      - | 6081 | `	{0x00D0,"&ETH;"},{0x00D1,"&Ntilde;"},{0x00D2,"&Ograve;"},{0x00D3,"&Oacute;"},` |
|      - | 6082 | `	{0x00D4,"&Ocirc;"},{0x00D5,"&Otilde;"},{0x00D6,"&Ouml;"},{0x00D7,"&times;"},` |
|      - | 6083 | `	{0x00D8,"&Oslash;"},{0x00D9,"&Ugrave;"},{0x00DA,"&Uacute;"},{0x00DB,"&Ucirc;"},` |
|      - | 6084 | `	{0x00DC,"&Uuml;"},{0x00DD,"&Yacute;"},{0x00DE,"&THORN;"},{0x00DF,"&szlig;"},` |
|      - | 6085 | `	{0x00E0,"&agrave;"},{0x00E1,"&aacute;"},{0x00E2,"&acirc;"},{0x00E3,"&atilde;"},` |
|      - | 6086 | `	{0x00E4,"&auml;"},{0x00E5,"&aring;"},{0x00E6,"&aelig;"},{0x00E7,"&ccedil;"},` |
|      - | 6087 | `	{0x00E8,"&egrave;"},{0x00E9,"&eacute;"},{0x00EA,"&ecirc;"},{0x00EB,"&euml;"},` |
|      - | 6088 | `	{0x00EC,"&igrave;"},{0x00ED,"&iacute;"},{0x00EE,"&icirc;"},{0x00EF,"&iuml;"},` |
|      - | 6089 | `	{0x00F0,"&eth;"},{0x00F1,"&ntilde;"},{0x00F2,"&ograve;"},{0x00F3,"&oacute;"},` |
|      - | 6090 | `	{0x00F4,"&ocirc;"},{0x00F5,"&otilde;"},{0x00F6,"&ouml;"},{0x00F7,"&divide;"},` |
|      - | 6091 | `	{0x00F8,"&oslash;"},{0x00F9,"&ugrave;"},{0x00FA,"&uacute;"},{0x00FB,"&ucirc;"},` |
|      - | 6092 | `	{0x00FC,"&uuml;"},{0x00FD,"&yacute;"},{0x00FE,"&thorn;"},{0x00FF,"&yuml;"},` |
|      - | 6093 | `	{0x0152,"&OElig;"},{0x0153,"&oelig;"},{0x0160,"&Scaron;"},{0x0161,"&scaron;"},` |
|      - | 6094 | `	{0x0178,"&Yuml;"},{0x0192,"&fnof;"},{0x02C6,"&circ;"},{0x02DC,"&tilde;"},` |
|      - | 6095 | `	{0x0391,"&Alpha;"},{0x0392,"&Beta;"},{0x0393,"&Gamma;"},{0x0394,"&Delta;"},` |
|      - | 6096 | `	{0x0395,"&Epsilon;"},{0x0396,"&Zeta;"},{0x0397,"&Eta;"},{0x0398,"&Theta;"},` |
|      - | 6097 | `	{0x0399,"&Iota;"},{0x039A,"&Kappa;"},{0x039B,"&Lambda;"},{0x039C,"&Mu;"},` |
|      - | 6098 | `	{0x039D,"&Nu;"},{0x039E,"&Xi;"},{0x039F,"&Omicron;"},{0x03A0,"&Pi;"},` |
|      - | 6099 | `	{0x03A1,"&Rho;"},{0x03A3,"&Sigma;"},{0x03A4,"&Tau;"},{0x03A5,"&Upsilon;"},` |
|      - | 6100 | `	{0x03A6,"&Phi;"},{0x03A7,"&Chi;"},{0x03A8,"&Psi;"},{0x03A9,"&Omega;"},` |
|      - | 6101 | `	{0x03B1,"&alpha;"},{0x03B2,"&beta;"},{0x03B3,"&gamma;"},{0x03B4,"&delta;"},` |
|      - | 6102 | `	{0x03B5,"&epsilon;"},{0x03B6,"&zeta;"},{0x03B7,"&eta;"},{0x03B8,"&theta;"},` |
|      - | 6103 | `	{0x03B9,"&iota;"},{0x03BA,"&kappa;"},{0x03BB,"&lambda;"},{0x03BC,"&mu;"},` |
|      - | 6104 | `	{0x03BD,"&nu;"},{0x03BE,"&xi;"},{0x03BF,"&omicron;"},{0x03C0,"&pi;"},` |
|      - | 6105 | `	{0x03C1,"&rho;"},{0x03C2,"&sigmaf;"},{0x03C3,"&sigma;"},{0x03C4,"&tau;"},` |
|      - | 6106 | `	{0x03C5,"&upsilon;"},{0x03C6,"&phi;"},{0x03C7,"&chi;"},{0x03C8,"&psi;"},` |
|      - | 6107 | `	{0x03C9,"&omega;"},{0x03D1,"&thetasym;"},{0x03D2,"&upsih;"},{0x03D6,"&piv;"},` |
|      - | 6108 | `	{0x2002,"&ensp;"},{0x2003,"&emsp;"},{0x2009,"&thinsp;"},{0x200C,"&zwnj;"},` |
|      - | 6109 | `	{0x200D,"&zwj;"},{0x200E,"&lrm;"},{0x200F,"&rlm;"},{0x2013,"&ndash;"},` |
|      - | 6110 | `	{0x2014,"&mdash;"},{0x2018,"&lsquo;"},{0x2019,"&rsquo;"},{0x201A,"&sbquo;"},` |
|      - | 6111 | `	{0x201C,"&ldquo;"},{0x201D,"&rdquo;"},{0x201E,"&bdquo;"},{0x2020,"&dagger;"},` |
|      - | 6112 | `	{0x2021,"&Dagger;"},{0x2022,"&bull;"},{0x2026,"&hellip;"},{0x2030,"&permil;"},` |
|      - | 6113 | `	{0x2032,"&prime;"},{0x2033,"&Prime;"},{0x2039,"&lsaquo;"},{0x203A,"&rsaquo;"},` |
|      - | 6114 | `	{0x203E,"&oline;"},{0x2044,"&frasl;"},{0x20AC,"&euro;"},{0x2111,"&image;"},` |
|      - | 6115 | `	{0x2118,"&weierp;"},{0x211C,"&real;"},{0x2122,"&trade;"},{0x2135,"&alefsym;"},` |
|      - | 6116 | `	{0x2190,"&larr;"},{0x2191,"&uarr;"},{0x2192,"&rarr;"},{0x2193,"&darr;"},` |
|      - | 6117 | `	{0x2194,"&harr;"},{0x21B5,"&crarr;"},{0x21D0,"&lArr;"},{0x21D1,"&uArr;"},` |
|      - | 6118 | `	{0x21D2,"&rArr;"},{0x21D3,"&dArr;"},{0x21D4,"&hArr;"},{0x2200,"&forall;"},` |
|      - | 6119 | `	{0x2202,"&part;"},{0x2203,"&exist;"},{0x2205,"&empty;"},{0x2207,"&nabla;"},` |
|      - | 6120 | `	{0x2208,"&isin;"},{0x2209,"&notin;"},{0x220B,"&ni;"},{0x220F,"&prod;"},` |
|      - | 6121 | `	{0x2211,"&sum;"},{0x2212,"&minus;"},{0x2217,"&lowast;"},{0x221A,"&radic;"},` |
|      - | 6122 | `	{0x221D,"&prop;"},{0x221E,"&infin;"},{0x2220,"&ang;"},{0x2227,"&and;"},` |
|      - | 6123 | `	{0x2228,"&or;"},{0x2229,"&cap;"},{0x222A,"&cup;"},{0x222B,"&int;"},` |
|      - | 6124 | `	{0x2234,"&there4;"},{0x223C,"&sim;"},{0x2245,"&cong;"},{0x2248,"&asymp;"},` |
|      - | 6125 | `	{0x2260,"&ne;"},{0x2261,"&equiv;"},{0x2264,"&le;"},{0x2265,"&ge;"},` |
|      - | 6126 | `	{0x2282,"&sub;"},{0x2283,"&sup;"},{0x2284,"&nsub;"},{0x2286,"&sube;"},` |
|      - | 6127 | `	{0x2287,"&supe;"},{0x2295,"&oplus;"},{0x2297,"&otimes;"},{0x22A5,"&perp;"},` |
|      - | 6128 | `	{0x22C5,"&sdot;"},{0x2308,"&lceil;"},{0x2309,"&rceil;"},{0x230A,"&lfloor;"},` |
|      - | 6129 | `	{0x230B,"&rfloor;"},{0x2329,"&lang;"},{0x232A,"&rang;"},{0x25CA,"&loz;"},` |
|      - | 6130 | `	{0x2660,"&spades;"},{0x2663,"&clubs;"},{0x2665,"&hearts;"},{0x2666,"&diams;"}` |
|      - | 6131 | `};` |
|      - | 6132 | `/* Binary-search aHtml401Ent[] for cp; return its "&name;" entity or 0. */` |
|     41 | 6133 | `static const char *FvHtml401Lookup(sxu32 cp){` |
|     41 | 6134 | `	int lo = 0, hi = (int)SX_ARRAYSIZE(aHtml401Ent) - 1;` |
|    323 | 6135 | `	while( lo <= hi ){` |
|    309 | 6136 | `		int mid = (lo + hi) / 2;` |
|    309 | 6137 | `		sxu32 c = aHtml401Ent[mid].cp;` |
|    309 | 6138 | `		if( c == cp ){ return aHtml401Ent[mid].zEnt; }` |
|    283 | 6139 | `		if( c < cp ){ lo = mid + 1; } else { hi = mid - 1; }` |
|      1 | 6140 | `	}` |
|     15 | 6141 | `	return 0;` |
|     21 | 6142 | `}` |
|      - | 6143 | `/* Decode one strict-UTF-8 sequence at p (< zEnd). On success returns its byte` |
|      - | 6144 | ` * length (1..4) and sets *pCp to the codepoint; on any malformed, overlong,` |
|      - | 6145 | ` * surrogate, truncated or out-of-range (>U+10FFFF) sequence returns 0. Matches` |
|      - | 6146 | ` * PHP's UTF-8 validation used by FULL_SPECIAL_CHARS (verified vs php 8.5.7). */` |
|    101 | 6147 | `static int FvUtf8Next(const unsigned char *p,const unsigned char *zEnd,sxu32 *pCp){` |
|    101 | 6148 | `	unsigned char c = p[0];` |
|    101 | 6149 | `	if( c < 0x80 ){ *pCp = c; return 1; }` |
|    101 | 6150 | `	if( c < 0xC2 ){ return 0; }              /* 0x80-0xBF stray cont / 0xC0-0xC1 overlong */` |
|     99 | 6151 | `	if( c < 0xE0 ){                          /* 2-byte: U+0080..U+07FF */` |
|     47 | 6152 | `		if( zEnd-p < 2 \|\| (p[1]&0xC0)!=0x80 ){ return 0; }` |
|     45 | 6153 | `		*pCp = ((sxu32)(c&0x1F)<<6) \| (p[1]&0x3F);` |
|     45 | 6154 | `		return 2;` |
|      - | 6155 | `	}` |
|     53 | 6156 | `	if( c < 0xF0 ){                          /* 3-byte: U+0800..U+FFFF minus surrogates */` |
|      - | 6157 | `		sxu32 cp;` |
|     47 | 6158 | `		if( zEnd-p < 3 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 ){ return 0; }` |
|     33 | 6159 | `		cp = ((sxu32)(c&0x0F)<<12) \| ((sxu32)(p[1]&0x3F)<<6) \| (p[2]&0x3F);` |
|     33 | 6160 | `		if( cp < 0x800 \|\| (cp>=0xD800 && cp<=0xDFFF) ){ return 0; }` |
|     29 | 6161 | `		*pCp = cp;` |
|     29 | 6162 | `		return 3;` |
|      - | 6163 | `	}` |
|      7 | 6164 | `	if( c < 0xF5 ){                          /* 4-byte: U+10000..U+10FFFF */` |
|      - | 6165 | `		sxu32 cp;` |
|      5 | 6166 | `		if( zEnd-p < 4 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 \|\| (p[3]&0xC0)!=0x80 ){ return 0; }` |
|      5 | 6167 | `		cp = ((sxu32)(c&0x07)<<18) \| ((sxu32)(p[1]&0x3F)<<12) \| ((sxu32)(p[2]&0x3F)<<6) \| (p[3]&0x3F);` |
|      5 | 6168 | `		if( cp < 0x10000 \|\| cp > 0x10FFFF ){ return 0; }` |
|      5 | 6169 | `		*pCp = cp;` |
|      5 | 6170 | `		return 4;` |
|      - | 6171 | `	}` |
|      3 | 6172 | `	return 0;                                /* 0xF5-0xFF */` |
|     51 | 6173 | `}` |
|      - | 6174 | `/* FILTER_SANITIZE_FULL_SPECIAL_CHARS: htmlentities-style, UTF-8-aware. Encodes` |
|      - | 6175 | ` * <>&"' as named entities ("'" -> &#039;; quotes suppressed under NO_ENCODE_QUOTES),` |
|      - | 6176 | ` * and every valid UTF-8 codepoint with an HTML 4.01 named entity as that entity;` |
|      - | 6177 | ` * valid codepoints without a named entity (and low control bytes) pass through` |
|      - | 6178 | ` * verbatim. If the input contains ANY invalid UTF-8 the whole result is "".` |
|      - | 6179 | ` * The STRIP/ENCODE flags do NOT apply to this filter (only NO_ENCODE_QUOTES).` |
|      - | 6180 | ` * php's filter does NOT re-encode valid pre-existing entities ("&amp;" stays,` |
|      - | 6181 | ` * "&bogus;" becomes "&amp;bogus;"), i.e. double_encode=false semantics —` |
|      - | 6182 | ` * exactly htmlentities(ENT_QUOTES\|ENT_HTML401, double_encode: false), so this` |
|      - | 6183 | ` * delegates to the shared encoder. Byte-exact vs php 8.5.7. */` |
|     25 | 6184 | `static void FvSanitizeFull(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 6185 | `	int iEntFlags = (flags & FV_FLAG_NO_ENCODE_QUOTES) ? 0 : PH7_ENT_QUOTES;` |
|     25 | 6186 | `	HtmlEscape(pCtx,z,n,iEntFlags,1/*bAll*/,0/*bDoubleEncode*/);` |
|     25 | 6187 | `}` |
|      - | 6188 | `/* ---------------------------------------------------------------------------` |
|      - | 6189 | ` * UTF-8-aware HTML entity core (htmlspecialchars/htmlentities family).` |
|      - | 6190 | ` * Prototyped next to the five builtins earlier in this file; lives here so it` |
|      - | 6191 | ` * can share aHtml401Ent[]/FvHtml401Lookup()/FvUtf8Next() with the filter_var` |
|      - | 6192 | ` * FULL_SPECIAL_CHARS filter above. Byte-exact vs php 8.5.7 (oracle-swept).` |
|      - | 6193 | ` * ------------------------------------------------------------------------ */` |
|      - | 6194 | `/* Encode cp as UTF-8 into zBuf (>= 4 bytes); return the byte length 1..4.` |
|      - | 6195 | ` * Thin wrapper over the engine-wide SX_WRITE_UTF8 (sxmacros.h). */` |
|    585 | 6196 | `static int HtmlCpUtf8(sxu32 cp,char *zBuf){` |
|    585 | 6197 | `	sxu8 *z = (sxu8 *)zBuf;` |
|    585 | 6198 | `	SX_WRITE_UTF8(z,cp);` |
|    585 | 6199 | `	return (int)(z - (sxu8 *)zBuf);` |
|      1 | 6200 | `}` |
|      - | 6201 | `/* Doctype-allowed codepoint test (php's unicode_cp_is_allowed) — gates what a` |
|      - | 6202 | ` * numeric reference may DECODE to. Oracle-pinned per doctype: HTML401` |
|      - | 6203 | ` * disallows C0 (except TAB/LF/CR) and DEL..U+009F; XML1 and XHTML share the` |
|      - | 6204 | ` * XML rules — DEL..U+009F allowed, U+FFFE/U+FFFF excluded; HTML5 swaps CR` |
|      - | 6205 | ` * for FF (0x0C) and excludes the noncharacters (U+FDD0..U+FDEF and every` |
|      - | 6206 | ` * U+xFFFE/U+xFFFF). Surrogates are disallowed everywhere. */` |
|     91 | 6207 | `static int HtmlCpAllowed(sxu32 cp,int iFlags){` |
|     91 | 6208 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|     91 | 6209 | `	if( cp==0x09 \|\| cp==0x0A ){ return 1; }` |
|     87 | 6210 | `	if( cp==0x0D ){ return iDoc != PH7_ENT_DOC_HTML5; }` |
|     85 | 6211 | `	if( cp==0x0C ){ return iDoc == PH7_ENT_DOC_HTML5; }` |
|     85 | 6212 | `	if( cp < 0x20 \|\| cp > 0x10FFFF ){ return 0; }` |
|     79 | 6213 | `	if( cp>=0xD800 && cp<=0xDFFF ){ return 0; }` |
|     77 | 6214 | `	if( cp>=0x7F && cp<=0x9F ){ return iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML; }` |
|     71 | 6215 | `	if( iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML ){` |
|    ! 0 | 6216 | `		return cp!=0xFFFE && cp!=0xFFFF;` |
|      - | 6217 | `	}` |
|     71 | 6218 | `	if( iDoc == PH7_ENT_DOC_HTML5 ){` |
|      9 | 6219 | `		if( cp>=0xFDD0 && cp<=0xFDEF ){ return 0; }` |
|      9 | 6220 | `		if( (cp & 0xFFFF) >= 0xFFFE ){ return 0; }` |
|      4 | 6221 | `	}` |
|     71 | 6222 | `	return 1;` |
|     46 | 6223 | `}` |
|      - | 6224 | `/* The ENT_DISALLOWED gate for RAW characters on the ENCODE side. Same as the` |
|      - | 6225 | ` * decode gate except CR under HTML5: php's encode-side unicode_cp_is_allowed` |
|      - | 6226 | ` * keeps a literal "\r" verbatim under ENT_HTML5\|ENT_DISALLOWED while the` |
|      - | 6227 | ` * decode side leaves "&#13;" un-decoded (oracle-pinned at flags 176). */` |
|      9 | 6228 | `static int HtmlCpAllowedEncode(sxu32 cp,int iFlags){` |
|      9 | 6229 | `	if( cp==0x0D && (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 ){ return 1; }` |
|      9 | 6230 | `	return HtmlCpAllowed(cp,iFlags);` |
|      5 | 6231 | `}` |
|      - | 6232 | `/* Numeric-reference validity for the double_encode=false "is this already a` |
|      - | 6233 | ` * valid entity" test — a MUCH looser predicate than the decode gate above:` |
|      - | 6234 | ` * any codepoint <= U+10FFFF is valid (controls and surrogates included, every` |
|      - | 6235 | ` * doctype). ENT_DISALLOWED re-tightens non-HTML401 doctypes to the decode` |
|      - | 6236 | ` * gate, except that HTML5 exempts surrogates. All oracle-pinned: &#0; and` |
|      - | 6237 | ` * &#xD800; stay verbatim at flags 11 and 139; flags -1 (HTML5+DISALLOWED)` |
|      - | 6238 | ` * re-encodes &#0; and &#x10FFFF; but still keeps &#xD800;; flags 144` |
|      - | 6239 | ` * (XML1+DISALLOWED) re-encodes &#xD800;. */` |
|      9 | 6240 | `static int HtmlNumericAllowed(sxu32 cp,int iFlags){` |
|      9 | 6241 | `	if( cp > 0x10FFFF ){ return 0; }` |
|      7 | 6242 | `	if( (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML401 ){ return 1; /* never tightened */ }` |
|    ! 0 | 6243 | `	if( (iFlags & PH7_ENT_DISALLOWED)` |
|    ! 0 | 6244 | `	 && !((iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 && cp>=0xD800 && cp<=0xDFFF)` |
|    ! 0 | 6245 | `	 && !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|    ! 0 | 6246 | `	return 1;` |
|      5 | 6247 | `}` |
|      - | 6248 | `/* How many bytes the malformed UTF-8 sequence at p consumes — php's` |
|      - | 6249 | ` * get_next_char failure step (one U+FFFD substitution / one ENT_IGNORE drop` |
|      - | 6250 | ` * per MAXIMAL invalid subpart, not per byte): a prefix-valid sequence eats` |
|      - | 6251 | ` * its continuation bytes ("\xE0\x80\xAF" is ONE unit) while a byte that could` |
|      - | 6252 | ` * start a new sequence is left for the next round. */` |
|      5 | 6253 | `static int HtmlUtf8Trail(unsigned char c){ return c>=0x80 && c<=0xBF; }` |
|     11 | 6254 | `static int HtmlUtf8Lead(unsigned char c){ return c<0x80 \|\| (c>=0xC2 && c<=0xF4); }` |
|     15 | 6255 | `static int HtmlUtf8FailAdvance(const unsigned char *p,const unsigned char *zEnd){` |
|     15 | 6256 | `	unsigned char c = p[0];` |
|     15 | 6257 | `	int nAvail = (int)(zEnd - p);` |
|     15 | 6258 | `	if( c < 0xC2 \|\| c > 0xF4 ){ return 1; } /* stray trail / C0-C1 / F5-FF */` |
|     13 | 6259 | `	if( c < 0xE0 ){` |
|      3 | 6260 | `		if( nAvail < 2 ){ return 1; }` |
|      3 | 6261 | `		return HtmlUtf8Lead(p[1]) ? 1 : 2;` |
|      - | 6262 | `	}` |
|     11 | 6263 | `	if( c < 0xF0 ){` |
|     11 | 6264 | `		if( nAvail >= 3 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) ){` |
|      3 | 6265 | `			return 3; /* complete but overlong/surrogate */` |
|      - | 6266 | `		}` |
|      9 | 6267 | `		if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 6268 | `		if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 6269 | `		return 3;` |
|      - | 6270 | `	}` |
|    ! 0 | 6271 | `	if( nAvail >= 4 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) && HtmlUtf8Trail(p[3]) ){` |
|    ! 0 | 6272 | `		return 4; /* complete but overlong / > U+10FFFF */` |
|      - | 6273 | `	}` |
|    ! 0 | 6274 | `	if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 6275 | `	if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 6276 | `	if( nAvail < 4 \|\| HtmlUtf8Lead(p[3]) ){ return 3; }` |
|    ! 0 | 6277 | `	return 4;` |
|      8 | 6278 | `}` |
|      - | 6279 | `/* The basic special entities, shared by named matching, the hsc_decode` |
|      - | 6280 | ` * numeric whitelist and the translation-table builder so the sets can never` |
|      - | 6281 | ` * drift apart. (&apos; is not an HTML 4.01 entity — doctype-gated below.) */` |
|      - | 6282 | `static const struct { const char *zEnt; int n; sxu32 cp; } aHtmlSpecEnt[] = {` |
|      - | 6283 | `	{"&amp;",5,38},{"&lt;",4,60},{"&gt;",4,62},{"&quot;",6,34},{"&apos;",6,39}` |
|      - | 6284 | `};` |
|      - | 6285 | `/* Does this doctype consult the named-entity table (aHtml401Ent)? XML 1.0 has` |
|      - | 6286 | ` * no named entities beyond the specials; XHTML/HTML5 are approximated by the` |
|      - | 6287 | ` * HTML 4.01 table (documented divergence). */` |
|     63 | 6288 | `static int HtmlDocHasNamedTable(int iDoc){` |
|     63 | 6289 | `	return iDoc != PH7_ENT_DOC_XML1;` |
|      1 | 6290 | `}` |
|      - | 6291 | `/* The single-quote entity per doctype. Oracle-pinned asymmetry: for every` |
|      - | 6292 | ` * non-HTML401 doctype htmlspecialchars emits &apos; while htmlentities` |
|      - | 6293 | ` * (bEntities) keeps &#039; under XHTML too. The translation table mirrors` |
|      - | 6294 | ` * whichever function the requested table belongs to. */` |
|     29 | 6295 | `static const char *HtmlAposEntity(int iDoc,int bEntities){` |
|     29 | 6296 | `	if( iDoc == PH7_ENT_DOC_HTML401 \|\| (bEntities && iDoc == PH7_ENT_DOC_XHTML) ){` |
|     21 | 6297 | `		return "&#039;";` |
|      - | 6298 | `	}` |
|      9 | 6299 | `	return "&apos;";` |
|     15 | 6300 | `}` |
|      - | 6301 | `/* Try to parse one HTML entity at z (z[0]=='&', z < zEnd). bFull selects the` |
|      - | 6302 | ` * html_entity_decode set (doctype named table + any allowed numeric ref) vs` |
|      - | 6303 | ` * the htmlspecialchars_decode set (the basic specials + quote numerics only).` |
|      - | 6304 | ` * Named matching is case-SENSITIVE and the ';' is required (both PHP-exact);` |
|      - | 6305 | ` * numeric refs accept dec/hex (x or X) with any number of leading zeros but` |
|      - | 6306 | ` * reject out-of-range, surrogate and doctype-disallowed codepoints (the` |
|      - | 6307 | ` * caller then leaves the source verbatim). Quote-flag gating is NOT applied` |
|      - | 6308 | ` * here — the same routine doubles as the "is this a valid entity" test for` |
|      - | 6309 | ` * double_encode=false, which ignores the quote bits (oracle-pinned).` |
|      - | 6310 | ` * bEncodeCheck selects the looser HtmlNumericAllowed predicate used by that` |
|      - | 6311 | ` * double_encode test; decode callers pass 0 for the HtmlCpAllowed gate.` |
|      - | 6312 | ` * On success sets *pCp / *pnConsumed and returns 1. */` |
|    172 | 6313 | `static int HtmlParseEntity(const unsigned char *z,const unsigned char *zEnd,` |
|      1 | 6314 | `                           int iFlags,int bFull,int bEncodeCheck,sxu32 *pCp,int *pnConsumed){` |
|    173 | 6315 | `	int nAvail = (int)(zEnd - z);` |
|    173 | 6316 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6317 | `	sxu32 n;` |
|    173 | 6318 | `	if( nAvail < 4 ){ return 0; } /* shortest entities: &lt; &#9; */` |
|    169 | 6319 | `	if( z[1] == '#' ){` |
|      - | 6320 | `		/* Numeric reference */` |
|     89 | 6321 | `		sxu32 cp = 0;` |
|     89 | 6322 | `		int i = 2, bHex = 0, nDig = 0;` |
|     89 | 6323 | `		if( z[i]=='x' \|\| z[i]=='X' ){ bHex = 1; i++; }` |
|    317 | 6324 | `		for( ; i < nAvail && z[i] != ';' ; i++ ){` |
|      - | 6325 | `			int v;` |
|    221 | 6326 | `			unsigned char c = z[i];` |
|    221 | 6327 | `			if( c>='0' && c<='9' ){ v = c - '0'; }` |
|     17 | 6328 | `			else if( bHex && c>='a' && c<='f' ){ v = c - 'a' + 10; }` |
|     17 | 6329 | `			else if( bHex && c>='A' && c<='F' ){ v = c - 'A' + 10; }` |
|    ! 0 | 6330 | `			else { return 0; }` |
|      - | 6331 | `			/* Stop accumulating once out of range (keeps validating the shape;` |
|      - | 6332 | `			 * max intermediate is 0x10FFFF*16+15, no sxu32 overflow). */` |
|    221 | 6333 | `			if( cp <= 0x10FFFF ){ cp = cp * (bHex ? 16 : 10) + (sxu32)v; }` |
|    221 | 6334 | `			nDig++;` |
|    111 | 6335 | `		}` |
|     97 | 6336 | `		if( nDig == 0 \|\| i >= nAvail ){ return 0; } /* no digits / no ';' */` |
|     97 | 6337 | `		if( bEncodeCheck ? !HtmlNumericAllowed(cp,iFlags) : !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|     83 | 6338 | `		if( !bFull ){` |
|      - | 6339 | `			/* hsc_decode: numeric refs to the five specials only. */` |
|     99 | 6340 | `			for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) && aHtmlSpecEnt[n].cp != cp ; n++ ){}` |
|     25 | 6341 | `			if( n >= SX_ARRAYSIZE(aHtmlSpecEnt) ){ return 0; }` |
|     11 | 6342 | `		}` |
|     75 | 6343 | `		*pCp = cp;` |
|     75 | 6344 | `		*pnConsumed = i + 1;` |
|     75 | 6345 | `		return 1;` |
|      - | 6346 | `	}` |
|      - | 6347 | `	/* Named reference — every entity name starts with a letter, so anything` |
|      - | 6348 | `	 * else can bail out before touching the tables. */` |
|     81 | 6349 | `	if( !((z[1]>='a' && z[1]<='z') \|\| (z[1]>='A' && z[1]<='Z')) ){ return 0; }` |
|    287 | 6350 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) ; n++ ){` |
|    265 | 6351 | `		if( aHtmlSpecEnt[n].cp == 39 && iDoc == PH7_ENT_DOC_HTML401 ){ continue; }` |
|    243 | 6352 | `		if( nAvail >= aHtmlSpecEnt[n].n && SyMemcmp(z,aHtmlSpecEnt[n].zEnt,(sxu32)aHtmlSpecEnt[n].n) == 0 ){` |
|     53 | 6353 | `			*pCp = aHtmlSpecEnt[n].cp;` |
|     53 | 6354 | `			*pnConsumed = aHtmlSpecEnt[n].n;` |
|     53 | 6355 | `			return 1;` |
|      - | 6356 | `		}` |
|     96 | 6357 | `	}` |
|     23 | 6358 | `	if( bFull && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 6359 | `		/* Linear scan of the 248-row table: runs only at '&'-then-letter` |
|      - | 6360 | `		 * positions and guarantees the decode set can never drift from the` |
|      - | 6361 | `		 * encode table. The first-letter guard skips the SyStrlen/SyMemcmp` |
|      - | 6362 | `		 * for ~96% of rows. */` |
|   3369 | 6363 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|      - | 6364 | `			sxu32 nEnt;` |
|   3357 | 6365 | `			if( z[1] != (unsigned char)aHtml401Ent[n].zEnt[1] ){ continue; }` |
|    121 | 6366 | `			nEnt = SyStrlen(aHtml401Ent[n].zEnt);` |
|    121 | 6367 | `			if( (sxu32)nAvail >= nEnt && SyMemcmp(z,aHtml401Ent[n].zEnt,nEnt) == 0 ){` |
|      7 | 6368 | `				*pCp = aHtml401Ent[n].cp;` |
|      7 | 6369 | `				*pnConsumed = (int)nEnt;` |
|      7 | 6370 | `				return 1;` |
|      - | 6371 | `			}` |
|     58 | 6372 | `		}` |
|      6 | 6373 | `	}` |
|     17 | 6374 | `	return 0;` |
|     88 | 6375 | `}` |
|      - | 6376 | `/* Shared encoder for htmlspecialchars (bAll=0) and htmlentities (bAll=1).` |
|      - | 6377 | ` * Invalid UTF-8 policy: ENT_IGNORE drops the byte (and wins over SUBSTITUTE),` |
|      - | 6378 | ` * ENT_SUBSTITUTE emits one U+FFFD per invalid byte, neither -> the whole` |
|      - | 6379 | ` * result is "" (pre-validated in a first pass: the accumulating result API` |
|      - | 6380 | ` * cannot roll back — same reason FvSanitizeFull is two-pass). */` |
|     96 | 6381 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 6382 | `                       int iFlags,int bAll,int bDoubleEncode){` |
|     97 | 6383 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     97 | 6384 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|      - | 6385 | `	const unsigned char *runStart;` |
|     97 | 6386 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6387 | `	sxu32 cp;` |
|     97 | 6388 | `	if( (iFlags & (PH7_ENT_IGNORE\|PH7_ENT_SUBSTITUTE)) == 0 ){` |
|      - | 6389 | `		/* Pass 1: any malformed sequence rejects the entire input. ASCII` |
|      - | 6390 | `		 * bytes cannot be malformed, so skip them without the decoder. */` |
|    381 | 6391 | `		while( p < zEnd ){` |
|      - | 6392 | `			int len;` |
|    323 | 6393 | `			if( *p < 0x80 ){ p++; continue; }` |
|     37 | 6394 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     37 | 6395 | `			if( len == 0 ){ ph7_result_string(pCtx,"",0); return; }` |
|     27 | 6396 | `			p += len;` |
|      1 | 6397 | `		}` |
|     59 | 6398 | `		p = (const unsigned char *)zIn;` |
|     29 | 6399 | `	}` |
|     87 | 6400 | `	runStart = p;` |
|     87 | 6401 | `	ph7_result_string(pCtx,"",0);` |
|    463 | 6402 | `	while( p < zEnd ){` |
|    377 | 6403 | `		const char *zEnt = 0;` |
|      - | 6404 | `		int len;` |
|    377 | 6405 | `		if( *p < 0x80 ){` |
|    313 | 6406 | `			len = 1;` |
|    313 | 6407 | `			switch( *p ){` |
|     25 | 6408 | `			case '<': zEnt = "&lt;"; break;` |
|     25 | 6409 | `			case '>': zEnt = "&gt;"; break;` |
|     18 | 6410 | `			case '&':` |
|     37 | 6411 | `				zEnt = "&amp;";` |
|     37 | 6412 | `				if( !bDoubleEncode ){` |
|      - | 6413 | `					sxu32 eCp; int nEat;` |
|     25 | 6414 | `					if( HtmlParseEntity(p,zEnd,iFlags,1,1,&eCp,&nEat) ){` |
|      - | 6415 | `						/* A valid existing entity: keep it verbatim. */` |
|     13 | 6416 | `						zEnt = 0;` |
|     13 | 6417 | `						len = nEat;` |
|      6 | 6418 | `					}` |
|     12 | 6419 | `				}` |
|     37 | 6420 | `				break;` |
|     10 | 6421 | `			case '"':` |
|     21 | 6422 | `				if( iFlags & PH7_ENT_QUOTE_DOUBLE ){ zEnt = "&quot;"; }` |
|     21 | 6423 | `				break;` |
|     12 | 6424 | `			case '\'':` |
|     25 | 6425 | `				if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|     23 | 6426 | `					zEnt = HtmlAposEntity(iDoc,bAll);` |
|     11 | 6427 | `				}` |
|     25 | 6428 | `				break;` |
|     92 | 6429 | `			default:` |
|    185 | 6430 | `				if( (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode((sxu32)*p,iFlags) ){` |
|    ! 0 | 6431 | `					zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 6432 | `				}` |
|    184 | 6433 | `				break;` |
|      - | 6434 | `			}` |
|    157 | 6435 | `		}else{` |
|     65 | 6436 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     65 | 6437 | `			if( len == 0 ){` |
|      - | 6438 | `				/* Malformed subpart (IGNORE or SUBSTITUTE is set, else pass 1` |
|      - | 6439 | `				 * would have rejected): drop it or emit ONE U+FFFD for the` |
|      - | 6440 | `				 * whole unit (php substitutes per maximal invalid subpart). */` |
|     15 | 6441 | `				if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|     15 | 6442 | `				if( (iFlags & PH7_ENT_IGNORE) == 0 ){ ph7_result_string(pCtx,"\xEF\xBF\xBD",3); }` |
|     15 | 6443 | `				p += HtmlUtf8FailAdvance(p,zEnd);` |
|     15 | 6444 | `				runStart = p;` |
|     15 | 6445 | `				continue;` |
|      - | 6446 | `			}` |
|     51 | 6447 | `			if( bAll && HtmlDocHasNamedTable(iDoc) ){` |
|     41 | 6448 | `				zEnt = FvHtml401Lookup(cp);` |
|     20 | 6449 | `			}` |
|     51 | 6450 | `			if( zEnt == 0 && (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode(cp,iFlags) ){` |
|    ! 0 | 6451 | `				zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 6452 | `			}` |
|      - | 6453 | `		}` |
|    363 | 6454 | `		if( zEnt ){` |
|    135 | 6455 | `			if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|    135 | 6456 | `			ph7_result_string(pCtx,zEnt,-1);` |
|    135 | 6457 | `			runStart = p + len;` |
|     67 | 6458 | `		}` |
|    363 | 6459 | `		p += len;` |
|      1 | 6460 | `	}` |
|     87 | 6461 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     49 | 6462 | `}` |
|      - | 6463 | `/* Shared decoder for html_entity_decode (bFull=1) and htmlspecialchars_decode` |
|      - | 6464 | ` * (bFull=0). Quote refs (cp 34/39, named or numeric) are gated by the quote` |
|      - | 6465 | ` * bits and left verbatim when suppressed; an invalid entity leaves its '&'` |
|      - | 6466 | ` * verbatim and rescans right after it, which also yields PHP's no-double-` |
|      - | 6467 | ` * decode behavior ("&amp;lt;" -> "&lt;"). */` |
|     84 | 6468 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 6469 | `                         int iFlags,int bFull){` |
|     85 | 6470 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     85 | 6471 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|     85 | 6472 | `	const unsigned char *runStart = p;` |
|     85 | 6473 | `	ph7_result_string(pCtx,"",0);` |
|    565 | 6474 | `	while( p < zEnd ){` |
|      - | 6475 | `		sxu32 cp;` |
|      - | 6476 | `		int nEat;` |
|    516 | 6477 | `		if( *p != '&' ){ p++; continue; }` |
|    155 | 6478 | `		if( !HtmlParseEntity(p,zEnd,iFlags,bFull,0,&cp,&nEat) ){ p++; continue; }` |
|    124 | 6479 | `		if( (cp == 34 && (iFlags & PH7_ENT_QUOTE_DOUBLE) == 0)` |
|    117 | 6480 | `		 \|\| (cp == 39 && (iFlags & PH7_ENT_QUOTE_SINGLE) == 0) ){` |
|      - | 6481 | `			/* Suppressed quote: leave the entity source verbatim. */` |
|     37 | 6482 | `			p += nEat;` |
|     37 | 6483 | `			continue;` |
|      - | 6484 | `		}` |
|     89 | 6485 | `		if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|      - | 6486 | `		{` |
|      - | 6487 | `			char zBuf[4];` |
|     89 | 6488 | `			int n = HtmlCpUtf8(cp,zBuf);` |
|     89 | 6489 | `			ph7_result_string(pCtx,zBuf,n);` |
|      - | 6490 | `		}` |
|     89 | 6491 | `		p += nEat;` |
|     89 | 6492 | `		runStart = p;` |
|      1 | 6493 | `	}` |
|     81 | 6494 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     81 | 6495 | `}` |
|      - | 6496 | `/* Validate the optional charset argument at apArg[idx]: UTF-8 aliases (and` |
|      - | 6497 | ` * ""/NULL meaning the default) are accepted; anything else — including` |
|      - | 6498 | ` * php-supported single-byte charsets like ISO-8859-1, PHL is UTF-8-only by` |
|      - | 6499 | ` * policy — raises PHP's unsupported-charset warning and is treated as` |
|      - | 6500 | ` * UTF-8 (ph7_context_throw_error_format prepends the function name). */` |
|    143 | 6501 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx){` |
|      - | 6502 | `	const char *zCs;` |
|      - | 6503 | `	int nCs;` |
|    150 | 6504 | `	if( nArg <= idx \|\| ph7_value_is_null(apArg[idx]) ){ return; }` |
|     15 | 6505 | `	zCs = ph7_value_to_string(apArg[idx],&nCs);` |
|     15 | 6506 | `	if( nCs == 0 ){ return; } /* "" selects the default charset (UTF-8) */` |
|     13 | 6507 | `	if( nCs == 5 && SyStrnicmp(zCs,"UTF-8",5) == 0 ){` |
|     13 | 6508 | `		return; /* php accepts only "UTF-8" (any case) silently — "UTF8" warns */` |
|      - | 6509 | `	}` |
|    ! 0 | 6510 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 6511 | `		"Charset \"%.*s\" is not supported, assuming UTF-8",nCs,zCs);` |
|     72 | 6512 | `}` |
|      - | 6513 | `/* get_html_translation_table() worker: character (UTF-8 bytes) => entity.` |
|      - | 6514 | ` * The five specials come first in byte order, then — for HTML_ENTITIES with a` |
|      - | 6515 | ` * named-table doctype — the 248 aHtml401Ent rows ascending (oracle-pinned` |
|      - | 6516 | ` * ordering; 253 entries under the defaults). */` |
|    549 | 6517 | `static void HtmlTableAdd(ph7_value *pArray,ph7_value *pValue,const char *zKey,const char *zEnt){` |
|    549 | 6518 | `	ph7_value_string(pValue,zEnt,-1);` |
|    549 | 6519 | `	ph7_array_add_strkey_elem(pArray,zKey,pValue);` |
|    549 | 6520 | `	ph7_value_reset_string_cursor(pValue);` |
|    549 | 6521 | `}` |
|     13 | 6522 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags){` |
|      - | 6523 | `	ph7_value *pArray,*pValue;` |
|     13 | 6524 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6525 | `	sxu32 n;` |
|     13 | 6526 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 6527 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 6528 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|    ! 0 | 6529 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6530 | `		return;` |
|      - | 6531 | `	}` |
|     13 | 6532 | `	if( iFlags & PH7_ENT_QUOTE_DOUBLE ){` |
|     11 | 6533 | `		HtmlTableAdd(pArray,pValue,"\"","&quot;");` |
|      5 | 6534 | `	}` |
|     13 | 6535 | `	HtmlTableAdd(pArray,pValue,"&","&amp;");` |
|     13 | 6536 | `	if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|      - | 6537 | `		/* The apostrophe row mirrors the function each table belongs to:` |
|      - | 6538 | `		 * SPECIALCHARS follows htmlspecialchars, ENTITIES follows` |
|      - | 6539 | `		 * htmlentities (oracle-pinned at flags 35). */` |
|      7 | 6540 | `		HtmlTableAdd(pArray,pValue,"'",HtmlAposEntity(iDoc,iTable != 0));` |
|      3 | 6541 | `	}` |
|     13 | 6542 | `	HtmlTableAdd(pArray,pValue,"<","&lt;");` |
|     13 | 6543 | `	HtmlTableAdd(pArray,pValue,">","&gt;");` |
|     13 | 6544 | `	if( iTable != 0 /*php: any non-HTML_SPECIALCHARS table => entities*/ && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 6545 | `		char zKey[8];` |
|    499 | 6546 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|    497 | 6547 | `			int nK = HtmlCpUtf8(aHtml401Ent[n].cp,zKey);` |
|    497 | 6548 | `			zKey[nK] = 0;` |
|    497 | 6549 | `			HtmlTableAdd(pArray,pValue,zKey,aHtml401Ent[n].zEnt);` |
|    249 | 6550 | `		}` |
|      1 | 6551 | `	}` |
|     13 | 6552 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 6553 | `}` |
|     25 | 6554 | `static int FvEmailAllowed(unsigned char c){` |
|     25 | 6555 | `	if( (c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9') ){ return 1; }` |
|     16 | 6556 | `	return c=='!'\|\|c=='#'\|\|c=='$'\|\|c=='%'\|\|c=='&'\|\|c=='\''\|\|c=='*'\|\|c=='+'` |
|     10 | 6557 | ``	    \|\| c=='-'\|\|c=='='\|\|c=='?'\|\|c=='^'\|\|c=='_'\|\|c=='`'\|\|c=='{'\|\|c=='\|'`` |
|     15 | 6558 | `	    \|\| c=='}'\|\|c=='~'\|\|c=='@'\|\|c=='.'\|\|c=='['\|\|c==']';` |
|     13 | 6559 | `}` |
|     23 | 6560 | `static int FvUrlAllowed(unsigned char c){` |
|     23 | 6561 | `	return c>=33 && c<=126; /* PHP keeps every printable ASCII byte except space */` |
|      1 | 6562 | `}` |
|      - | 6563 | `/* SANITIZE_EMAIL (isUrl=0) / SANITIZE_URL (isUrl=1): strip disallowed bytes. */` |
|      5 | 6564 | `static void FvSanitizeChars(ph7_context *pCtx,const char *z,int n,int isUrl){` |
|      5 | 6565 | `	int i, runStart = 0;` |
|      5 | 6566 | `	ph7_result_string(pCtx,"",0);` |
|     51 | 6567 | `	for( i=0; i<n; i++ ){` |
|     47 | 6568 | `		unsigned char c = (unsigned char)z[i];` |
|     47 | 6569 | `		if( !(isUrl ? FvUrlAllowed(c) : FvEmailAllowed(c)) ){` |
|     11 | 6570 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     11 | 6571 | `			runStart = i+1;` |
|      5 | 6572 | `		}` |
|     24 | 6573 | `	}` |
|      5 | 6574 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      5 | 6575 | `}` |
|      - | 6576 | `/*` |
|      - | 6577 | ` * Apply the selected filter to one already-resolved input value and write the` |
|      - | 6578 | ` * result into pCtx. Shared by filter_var() and filter_input(): the caller has` |
|      - | 6579 | ` * already parsed $filter/$flags/$options. On validation failure the 'default'` |
|      - | 6580 | ` * option (if any) is returned, else null when FILTER_NULL_ON_FAILURE is set,` |
|      - | 6581 | ` * else false. A validating filter that passes returns the (string) input` |
|      - | 6582 | ` * unchanged; a sanitizer writes its transformed output directly.` |
|      - | 6583 | ` */` |
|    316 | 6584 | `static int FvApplyFilter(ph7_context *pCtx,ph7_value *pInput,` |
|      - | 6585 | `                         int iFilter,int iFlags,ph7_value *pOpts,` |
|      - | 6586 | `                         ph7_value *pDefault)` |
|      3 | 6587 | `{` |
|    319 | 6588 | `	int bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|      - | 6589 | `	const char *zVal; int nVal;` |
|      - | 6590 | `	/* An array/object input fails every scalar filter. */` |
|    319 | 6591 | `	if( ph7_value_is_array(pInput) ){ goto fail; }` |
|    317 | 6592 | `	zVal = ph7_value_to_string(pInput,&nVal);` |
|    317 | 6593 | `	switch( iFilter ){` |
|     28 | 6594 | `	case FV_VALIDATE_INT: {` |
|      - | 6595 | `		ph7_int64 v;` |
|     58 | 6596 | `		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }` |
|     31 | 6597 | `		if( pOpts ){` |
|      7 | 6598 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|      7 | 6599 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|      7 | 6600 | `			if( pMin && v<ph7_value_to_int64(pMin) ){ goto fail; }` |
|      7 | 6601 | `			if( pMax && v>ph7_value_to_int64(pMax) ){ goto fail; }` |
|      2 | 6602 | `		}` |
|     29 | 6603 | `		ph7_result_int64(pCtx,v);` |
|     29 | 6604 | `		return PH7_OK;` |
|      - | 6605 | `	}` |
|     34 | 6606 | `	case FV_VALIDATE_FLOAT: {` |
|      - | 6607 | `		double d;` |
|     69 | 6608 | `		if( !FvValidateFloat(zVal,nVal,iFlags,&d) ){ goto fail; }` |
|     39 | 6609 | `		ph7_result_double(pCtx,d);` |
|     39 | 6610 | `		return PH7_OK;` |
|      - | 6611 | `	}` |
|     14 | 6612 | `	case FV_VALIDATE_BOOLEAN: {` |
|      - | 6613 | `		int b;` |
|     29 | 6614 | `		if( !FvValidateBool(zVal,nVal,&b) ){ goto fail; }` |
|     21 | 6615 | `		ph7_result_bool(pCtx,b);` |
|     21 | 6616 | `		return PH7_OK;` |
|      - | 6617 | `	}` |
|     25 | 6618 | `	case FV_VALIDATE_IP:     if( !FvValidateIp(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|     11 | 6619 | `	case FV_VALIDATE_MAC:    if( !FvValidateMac(zVal,nVal) ){ goto fail; }       goto pass;` |
|     28 | 6620 | `	case FV_VALIDATE_EMAIL:  if( !FvValidateEmail(zVal,nVal) ){ goto fail; }     goto pass;` |
|     11 | 6621 | `	case FV_VALIDATE_DOMAIN: if( !FvValidateDomain(zVal,nVal) ){ goto fail; }    goto pass;` |
|     15 | 6622 | `	case FV_VALIDATE_URL:    if( !FvValidateUrl(zVal,nVal) ){ goto fail; }       goto pass;` |
|      3 | 6623 | `	case FV_VALIDATE_REGEXP: {` |
|      - | 6624 | `#ifdef PH7_ENABLE_PCRE` |
|      8 | 6625 | `		ph7_value *pRe = pOpts ? ph7_array_fetch(pOpts,"regexp",(int)sizeof("regexp")-1) : 0;` |
|      8 | 6626 | `		const char *zRe; int nRe, matched = 0;` |
|      8 | 6627 | `		if( pRe==0 ){` |
|      3 | 6628 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6629 | `				"filter_var(): \"regexp\" option is missing");` |
|      - | 6630 | `		}` |
|      5 | 6631 | `		zRe = ph7_value_to_string(pRe,&nRe);` |
|      5 | 6632 | `		if( PH7_PcreMatchQuiet(pCtx,zRe,nRe,zVal,nVal,&matched)!=SXRET_OK \|\| !matched ){ goto fail; }` |
|      3 | 6633 | `		goto pass;` |
|      - | 6634 | `#else` |
|      - | 6635 | `		goto fail;` |
|      - | 6636 | `#endif` |
|      - | 6637 | `	}` |
|      3 | 6638 | `	case FV_SANITIZE_NUMBER_INT:   FvSanitizeNumber(pCtx,zVal,nVal,0,0);      return PH7_OK;` |
|      5 | 6639 | `	case FV_SANITIZE_NUMBER_FLOAT: FvSanitizeNumber(pCtx,zVal,nVal,1,iFlags); return PH7_OK;` |
|     13 | 6640 | `	case FV_SANITIZE_SPECIAL_CHARS:      FvSanitizeSpecial(pCtx,zVal,nVal,iFlags); return PH7_OK;` |
|     25 | 6641 | `	case FV_SANITIZE_FULL_SPECIAL_CHARS: FvSanitizeFull(pCtx,zVal,nVal,iFlags);    return PH7_OK;` |
|      3 | 6642 | `	case FV_SANITIZE_EMAIL: FvSanitizeChars(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 6643 | `	case FV_SANITIZE_URL:   FvSanitizeChars(pCtx,zVal,nVal,1); return PH7_OK;` |
|     13 | 6644 | `	case FV_DEFAULT:` |
|      - | 6645 | `		/* FILTER_UNSAFE_RAW / FILTER_DEFAULT: pass through unchanged unless a` |
|      - | 6646 | `		 * STRIP/ENCODE flag is set, in which case apply the string filter. */` |
|     28 | 6647 | `		if( iFlags & FV_FLAG_STRING_MASK ){` |
|     15 | 6648 | `			FvSanitizeString(pCtx,zVal,nVal,iFlags);` |
|     15 | 6649 | `			return PH7_OK;` |
|      - | 6650 | `		}` |
|     14 | 6651 | `		goto pass;` |
|    ! 0 | 6652 | `	default:` |
|    ! 0 | 6653 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 6654 | `			"Unknown filter with ID %d",iFilter);` |
|    ! 0 | 6655 | `		break; /* unknown filter id -> fail */` |
|    ! 0 | 6656 | `	}` |
|     58 | 6657 | `fail:` |
|    118 | 6658 | `	if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|    114 | 6659 | `	else if( bNull ){ ph7_result_null(pCtx); }` |
|    108 | 6660 | `	else { ph7_result_bool(pCtx,0); }` |
|    118 | 6661 | `	return PH7_OK;` |
|     26 | 6662 | `pass: /* validation passed: return the (string) input unchanged */` |
|     54 | 6663 | `	ph7_result_string(pCtx,zVal,nVal);` |
|     54 | 6664 | `	return PH7_OK;` |
|    161 | 6665 | `}` |
|      - | 6666 | `/*` |
|      - | 6667 | ` * Parse the ($filter, $options) pair shared by filter_var()/filter_input() out` |
|      - | 6668 | ` * of apArg[iBase] ($filter) and apArg[iBase+1] ($options): $options is either a` |
|      - | 6669 | ` * plain flags int, or an array with 'flags' and an 'options' sub-array (whose` |
|      - | 6670 | ` * 'default' entry is the fallback value). Fills the four output pointers;` |
|      - | 6671 | ` * unset outputs keep the caller-provided defaults.` |
|      - | 6672 | ` */` |
|    328 | 6673 | `static void FvParseFilterArgs(int nArg,ph7_value **apArg,int iBase,` |
|      - | 6674 | `                              int *piFilter,int *piFlags,` |
|      - | 6675 | `                              ph7_value **ppOpts,ph7_value **ppDefault)` |
|      3 | 6676 | `{` |
|    331 | 6677 | `	if( nArg>iBase ){ *piFilter = ph7_value_to_int(apArg[iBase]); }` |
|    331 | 6678 | `	if( nArg>iBase+1 ){` |
|     88 | 6679 | `		if( ph7_value_is_array(apArg[iBase+1]) ){` |
|     42 | 6680 | `			ph7_value *pF = ph7_array_fetch(apArg[iBase+1],"flags",(int)sizeof("flags")-1);` |
|     42 | 6681 | `			if( pF ){ *piFlags = ph7_value_to_int(pF); }` |
|     42 | 6682 | `			*ppOpts = ph7_array_fetch(apArg[iBase+1],"options",(int)sizeof("options")-1);` |
|     42 | 6683 | `			if( *ppOpts && !ph7_value_is_array(*ppOpts) ){ *ppOpts = 0; }` |
|     42 | 6684 | `			if( *ppOpts ){ *ppDefault = ph7_array_fetch(*ppOpts,"default",(int)sizeof("default")-1); }` |
|     22 | 6685 | `		}else{` |
|     48 | 6686 | `			*piFlags = ph7_value_to_int(apArg[iBase+1]);` |
|      - | 6687 | `		}` |
|     43 | 6688 | `	}` |
|    331 | 6689 | `}` |
|      - | 6690 | `/*` |
|      - | 6691 | ` * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 6692 | ` *  Validate or sanitize a value; see FvApplyFilter for the failure semantics.` |
|      - | 6693 | ` */` |
|    306 | 6694 | `static int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6695 | `{` |
|    308 | 6696 | `	int iFilter = FV_DEFAULT, iFlags = 0;` |
|    308 | 6697 | `	ph7_value *pOpts = 0, *pDefault = 0;` |
|    308 | 6698 | `	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    308 | 6699 | `	FvParseFilterArgs(nArg,apArg,1,&iFilter,&iFlags,&pOpts,&pDefault);` |
|    308 | 6700 | `	return FvApplyFilter(pCtx,apArg[0],iFilter,iFlags,pOpts,pDefault);` |
|    155 | 6701 | `}` |
|      - | 6702 | `/*` |
|      - | 6703 | ` * filter_input($type, $var_name, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 6704 | ` *  Look up $var_name in the requested INPUT_* superglobal, then apply the` |
|      - | 6705 | ` *  filter. Semantics verified byte-for-byte against php 8.5:` |
|      - | 6706 | ` *   - variable NOT set: 'default' option wins, else false when` |
|      - | 6707 | ` *     FILTER_NULL_ON_FAILURE is set, else null. (Note the null/false roles are` |
|      - | 6708 | ` *     INVERTED relative to a present value that fails validation, which yields` |
|      - | 6709 | ` *     default > null-if-NULL_ON_FAILURE > false via FvApplyFilter.)` |
|      - | 6710 | ` *   - variable present: delegate to FvApplyFilter.` |
|      - | 6711 | ` *  Divergence: php reads a SAPI snapshot of the original request variables` |
|      - | 6712 | ` *  captured at startup; PHL reads the live superglobal. In CLI they match for` |
|      - | 6713 | ` *  the SAPI-registered keys (SCRIPT_NAME/PHP_SELF/DOCUMENT_ROOT); keys added` |
|      - | 6714 | ` *  only to the live $_SERVER (REQUEST_TIME/PWD/…) are visible here but not in` |
|      - | 6715 | ` *  php's snapshot.` |
|      - | 6716 | ` */` |
|     24 | 6717 | `static int PH7_builtin_filter_input(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6718 | `{` |
|     26 | 6719 | `	int iType, iFilter = FV_DEFAULT, iFlags = 0;` |
|     26 | 6720 | `	ph7_value *pOpts = 0, *pDefault = 0, *pSuper, *pElem;` |
|      - | 6721 | `	const char *zVar, *zSuper; int nVar; sxu32 nSuper;` |
|     26 | 6722 | `	if( nArg<2 ){` |
|    ! 0 | 6723 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 6724 | `			"filter_input() expects at least 2 arguments, %d given",nArg);` |
|      - | 6725 | `	}` |
|     26 | 6726 | `	iType = ph7_value_to_int(apArg[0]);` |
|     26 | 6727 | `	switch( iType ){` |
|      3 | 6728 | `	case 0: zSuper = "_POST";   nSuper = (sxu32)sizeof("_POST")-1;   break; /* INPUT_POST */` |
|      3 | 6729 | `	case 1: zSuper = "_GET";    nSuper = (sxu32)sizeof("_GET")-1;    break; /* INPUT_GET */` |
|    ! 0 | 6730 | `	case 2: zSuper = "_COOKIE"; nSuper = (sxu32)sizeof("_COOKIE")-1; break; /* INPUT_COOKIE */` |
|    ! 0 | 6731 | `	case 4: zSuper = "_ENV";    nSuper = (sxu32)sizeof("_ENV")-1;    break; /* INPUT_ENV */` |
|     19 | 6732 | `	case 5: zSuper = "_SERVER"; nSuper = (sxu32)sizeof("_SERVER")-1; break; /* INPUT_SERVER */` |
|      1 | 6733 | `	default:` |
|      3 | 6734 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6735 | `			"filter_input(): Argument #1 ($type) must be an INPUT_* constant");` |
|      - | 6736 | `	}` |
|     23 | 6737 | `	zVar = ph7_value_to_string(apArg[1],&nVar);` |
|     23 | 6738 | `	FvParseFilterArgs(nArg,apArg,2,&iFilter,&iFlags,&pOpts,&pDefault);` |
|      - | 6739 | `	/* Resolve the variable from the superglobal (missing/non-array -> not set). */` |
|     23 | 6740 | `	pSuper = PH7_VmExtractSuper(pCtx->pVm,zSuper,nSuper);` |
|     23 | 6741 | `	pElem = (pSuper && ph7_value_is_array(pSuper))` |
|     33 | 6742 | `		? ph7_array_fetch(pSuper,zVar,nVar) : 0;` |
|     23 | 6743 | `	if( pElem==0 ){` |
|      - | 6744 | `		/* Variable not set: default > false(if NULL_ON_FAILURE) > null. Note the` |
|      - | 6745 | `		 * false/null roles are inverted vs FvApplyFilter's present-but-fails path. */` |
|     13 | 6746 | `		if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|      9 | 6747 | `		else if( iFlags & FV_NULL_ON_FAILURE ){ ph7_result_bool(pCtx,0); }` |
|      7 | 6748 | `		else { ph7_result_null(pCtx); }` |
|     13 | 6749 | `		return PH7_OK;` |
|      - | 6750 | `	}` |
|     11 | 6751 | `	return FvApplyFilter(pCtx,pElem,iFilter,iFlags,pOpts,pDefault);` |
|     14 | 6752 | `}` |
|      - | 6753 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6754 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6755 | `/*` |
|      - | 6756 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 6757 |  |
|      - | 6758 | ` */` |
|      4 | 6759 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 6760 | `	const char *zInput, /* Raw input */` |
|      - | 6761 | `	int nByte,  /* Input length */` |
|      - | 6762 | `	int delim,  /* Delimiter */` |
|      - | 6763 | `	int encl,   /* Enclosure */` |
|      - | 6764 | `	int escape,  /* Escape character */` |
|      - | 6765 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 6766 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 6767 | `	)` |
|      1 | 6768 | `{` |
|      5 | 6769 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 6770 | `	const char *zIn = zInput;` |
|      - | 6771 | `	const char *zPtr;` |
|      - | 6772 | `	int isEnc;` |
|      - | 6773 | `	/* Start processing */` |
|      8 | 6774 | `	for(;;){` |
|     17 | 6775 | `		if( zIn >= zEnd ){` |
|      - | 6776 | `			/* No more input to process */` |
|      5 | 6777 | `			break;` |
|      - | 6778 | `		}` |
|     13 | 6779 | `		isEnc = 0;` |
|     13 | 6780 | `		zPtr = zIn;` |
|      - | 6781 | `		/* Find the first delimiter */` |
|     27 | 6782 | `		while( zIn < zEnd ){` |
|     23 | 6783 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 6784 | `				/* Delimiter found,break imediately */` |
|      5 | 6785 | `				break;` |
|     15 | 6786 | `			}else if( zIn[0] == encl ){` |
|      - | 6787 | `				/* Inside enclosure? */` |
|    ! 0 | 6788 | `				isEnc = !isEnc;` |
|     15 | 6789 | `			}else if( zIn[0] == escape ){` |
|      - | 6790 | `				/* Escape sequence */` |
|    ! 0 | 6791 | `				zIn++;` |
|    ! 0 | 6792 | `			}` |
|      - | 6793 | `			/* Advance the cursor */` |
|     15 | 6794 | `			zIn++;` |
|      1 | 6795 | `		}` |
|     13 | 6796 | `		if( zIn > zPtr ){` |
|     13 | 6797 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 6798 | `			sxi32 rc;` |
|      - | 6799 | `			/* Invoke the supllied callback */` |
|     13 | 6800 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 6801 | `				zPtr++;` |
|    ! 0 | 6802 | `				nByteChunk-=2;` |
|    ! 0 | 6803 | `			}` |
|     13 | 6804 | `			if( nByteChunk > 0 ){` |
|     13 | 6805 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 6806 | `				if( rc == SXERR_ABORT ){` |
|      - | 6807 | `					/* User callback request an operation abort */` |
|    ! 0 | 6808 | `					break;` |
|      - | 6809 | `				}` |
|      6 | 6810 | `			}` |
|      6 | 6811 | `		}` |
|      - | 6812 | `		/* Ignore trailing delimiter */` |
|     21 | 6813 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 6814 | `			zIn++;` |
|      1 | 6815 | `		}` |
|      1 | 6816 | `	}` |
|      5 | 6817 | `	return SXRET_OK;` |
|      1 | 6818 | `}` |
|      - | 6819 | `/*` |
|      - | 6820 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 6821 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 6822 | ` * argument to this callback.` |
|      - | 6823 | ` */` |
|     12 | 6824 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 6825 | `{` |
|     13 | 6826 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 6827 | `	ph7_value sEntry;` |
|      - | 6828 | `	SyString sToken;` |
|      - | 6829 | `	/* Insert the token in the given array */` |
|     13 | 6830 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 6831 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 6832 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 6833 | `	if( sToken.nByte < 1){` |
|    ! 0 | 6834 | `		return SXRET_OK;` |
|      - | 6835 | `	}` |
|     13 | 6836 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 6837 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 6838 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 6839 | `	return SXRET_OK;` |
|      7 | 6840 | `}` |
|      - | 6841 | `/*` |
|      - | 6842 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 6843 | ` *  Parse a CSV string into an array.` |
|      - | 6844 | ` * Parameters` |
|      - | 6845 | ` *  $input` |
|      - | 6846 | ` *   The string to parse.` |
|      - | 6847 | ` *  $delimiter` |
|      - | 6848 | ` *   Set the field delimiter (one character only).` |
|      - | 6849 | ` *  $enclosure` |
|      - | 6850 | ` *   Set the field enclosure character (one character only).` |
|      - | 6851 | ` *  $escape` |
|      - | 6852 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 6853 | ` * Return` |
|      - | 6854 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 6855 | ` */` |
|      2 | 6856 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6857 | `{` |
|      - | 6858 | `	const char *zInput,*zPtr;` |
|      - | 6859 | `	ph7_value *pArray;` |
|      3 | 6860 | `	int delim  = ',';   /* Delimiter */` |
|      3 | 6861 | `	int encl   = '"' ;  /* Enclosure */` |
|      3 | 6862 | `	int escape = '\\';  /* Escape character */` |
|      - | 6863 | `	int nLen;` |
|      3 | 6864 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6865 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 6866 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6867 | `		return PH7_OK;` |
|      - | 6868 | `	}` |
|      - | 6869 | `	/* Extract the raw input */` |
|      3 | 6870 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 6871 | `	if( nArg > 1 ){` |
|      - | 6872 | `		int i;` |
|      3 | 6873 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 6874 | `			/* Extract the delimiter */` |
|      3 | 6875 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 6876 | `			if( i > 0 ){` |
|      3 | 6877 | `				delim = zPtr[0];` |
|      1 | 6878 | `			}` |
|      1 | 6879 | `		}` |
|      3 | 6880 | `		if( nArg > 2 ){` |
|      3 | 6881 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 6882 | `				/* Extract the enclosure */` |
|      3 | 6883 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 6884 | `				if( i > 0 ){` |
|      3 | 6885 | `					encl = zPtr[0];` |
|      1 | 6886 | `				}` |
|      1 | 6887 | `			}` |
|      3 | 6888 | `			if( nArg > 3 ){` |
|      3 | 6889 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 6890 | `					/* Extract the escape character */` |
|      3 | 6891 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 6892 | `					if( i > 0 ){` |
|      3 | 6893 | `						escape = zPtr[0];` |
|      1 | 6894 | `					}` |
|      1 | 6895 | `				}` |
|      1 | 6896 | `			}` |
|      1 | 6897 | `		}` |
|      1 | 6898 | `	}` |
|      - | 6899 | `	/* Create our array */` |
|      3 | 6900 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 6901 | `	if( pArray == 0 ){` |
|      - | 6902 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|    ! 0 | 6903 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6904 | `	}` |
|      - | 6905 | `	/* Parse the raw input */` |
|      3 | 6906 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 6907 | `	/* Return the freshly created array */` |
|      3 | 6908 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 6909 | `	return PH7_OK;` |
|      2 | 6910 | `}` |
|      - | 6911 | `/*` |
|      - | 6912 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 6913 | ` * container.` |
|      - | 6914 | ` * Refer to [strip_tags()].` |
|      - | 6915 | ` */` |
|     10 | 6916 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 6917 | `{` |
|     11 | 6918 | `	const char *zEnd = &zTag[nByte];` |
|      - | 6919 | `	const char *zPtr;` |
|      - | 6920 | `	SyString sEntry;` |
|      - | 6921 | `	/* Strip tags */` |
|     10 | 6922 | `	for(;;){` |
|     45 | 6923 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 6924 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 6925 | `				zTag++;` |
|      1 | 6926 | `		}` |
|     21 | 6927 | `		if( zTag >= zEnd ){` |
|     11 | 6928 | `			break;` |
|      - | 6929 | `		}` |
|     11 | 6930 | `		zPtr = zTag;` |
|      - | 6931 | `		/* Delimit the tag */` |
|     25 | 6932 | `		while(zTag < zEnd ){` |
|     25 | 6933 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 6934 | `				/* UTF-8 stream */` |
|      3 | 6935 | `				zTag++;` |
|      5 | 6936 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 6937 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 6938 | `				break;` |
|    ! 0 | 6939 | `			}else{` |
|     13 | 6940 | `				zTag++;` |
|      - | 6941 | `			}` |
|      1 | 6942 | `		}` |
|     11 | 6943 | `		if( zTag > zPtr ){` |
|      - | 6944 | `			/* Perform the insertion */` |
|     11 | 6945 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 6946 | `			SyStringFullTrim(&sEntry);` |
|     11 | 6947 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 6948 | `		}` |
|      - | 6949 | `		/* Jump the trailing '>' */` |
|     11 | 6950 | `		zTag++;` |
|      1 | 6951 | `	}` |
|     11 | 6952 | `	return SXRET_OK;` |
|      1 | 6953 | `}` |
|      - | 6954 | `/*` |
|      - | 6955 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 6956 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 6957 | ` * Refer to [strip_tags()].` |
|      - | 6958 | ` */` |
|     36 | 6959 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 6960 | `{` |
|     37 | 6961 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 6962 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 6963 | `		SyString sTag;` |
|     85 | 6964 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 6965 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 6966 | `			zTag++;` |
|      1 | 6967 | `		}` |
|      - | 6968 | `		/* Delimit the tag */` |
|     25 | 6969 | `		zCur = zTag;` |
|     77 | 6970 | `		while(zTag < zEnd ){` |
|     77 | 6971 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 6972 | `				/* UTF-8 stream */` |
|      5 | 6973 | `				zTag++;` |
|      9 | 6974 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 6975 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 6976 | `				break;` |
|    ! 0 | 6977 | `			}else{` |
|     49 | 6978 | `				zTag++;` |
|      - | 6979 | `			}` |
|      1 | 6980 | `		}` |
|     25 | 6981 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 6982 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 6983 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 6984 | `		if( sTag.nByte > 0 ){` |
|      - | 6985 | `			SyString *aEntry,*pEntry;` |
|      - | 6986 | `			sxi32 rc;` |
|      - | 6987 | `			sxu32 n;` |
|      - | 6988 | `			/* Perform the lookup */` |
|     25 | 6989 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 6990 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 6991 | `				pEntry = &aEntry[n];` |
|      - | 6992 | `				/* Do the comparison */` |
|     25 | 6993 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 6994 | `				if( !rc ){` |
|     21 | 6995 | `					return SXRET_OK;` |
|      - | 6996 | `				}` |
|      3 | 6997 | `			}` |
|      2 | 6998 | `		}` |
|      2 | 6999 | `	}` |
|      - | 7000 | `	/* No such tag */` |
|     17 | 7001 | `	return SXERR_NOTFOUND;` |
|     19 | 7002 | `}` |
|      - | 7003 | `/*` |
|      - | 7004 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 7005 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 7006 | ` * Refer to [strip_tags()].` |
|      - | 7007 | ` */` |
|     16 | 7008 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 7009 | `{` |
|     17 | 7010 | `	const char *zEnd = &zIn[nByte];` |
|      - | 7011 | `	const char *zPtr,*zTag;` |
|      - | 7012 | `	SySet sSet;` |
|      - | 7013 | `	/* initialize the set of allowed tags */` |
|     17 | 7014 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 7015 | `	if( nTaglen > 0 ){` |
|      - | 7016 | `		/* Set of allowed tags */` |
|     11 | 7017 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 7018 | `	}` |
|      - | 7019 | `	/* Set the empty string */` |
|     17 | 7020 | `	ph7_result_string(pCtx,"",0);` |
|      - | 7021 | `	/* Start processing */` |
|     26 | 7022 | `	for(;;){` |
|     53 | 7023 | `		if(zIn >= zEnd){` |
|      - | 7024 | `			/* No more input to process */` |
|     15 | 7025 | `			break;` |
|      - | 7026 | `		}` |
|     39 | 7027 | `		zPtr = zIn;` |
|      - | 7028 | `		/* Find a tag */` |
|    133 | 7029 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 7030 | `			zIn++;` |
|      1 | 7031 | `		}` |
|     39 | 7032 | `		if( zIn > zPtr ){` |
|      - | 7033 | `			/* Consume raw input */` |
|     21 | 7034 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 7035 | `		}` |
|      - | 7036 | `		/* Ignore trailing null bytes */` |
|     39 | 7037 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 7038 | `			zIn++;` |
|    ! 0 | 7039 | `		}` |
|     39 | 7040 | `		if(zIn >= zEnd){` |
|      - | 7041 | `			/* No more input to process */` |
|      3 | 7042 | `			break;` |
|      - | 7043 | `		}` |
|     37 | 7044 | `		if( zIn[0] == '<' ){` |
|      - | 7045 | `			sxi32 rc;` |
|     37 | 7046 | `			zTag = zIn++;` |
|      - | 7047 | `			/* Delimit the tag */` |
|    127 | 7048 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 7049 | `				zIn++;` |
|      1 | 7050 | `			}` |
|     37 | 7051 | `			if( zIn < zEnd ){` |
|     37 | 7052 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 7053 | `			}` |
|      - | 7054 | `			/* Query the set */` |
|     37 | 7055 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 7056 | `			if( rc == SXRET_OK ){` |
|      - | 7057 | `				/* Keep the tag */` |
|     21 | 7058 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 7059 | `			}` |
|     18 | 7060 | `		}` |
|      1 | 7061 | `	}` |
|      - | 7062 | `	/* Cleanup */` |
|     17 | 7063 | `	SySetRelease(&sSet);` |
|     17 | 7064 | `	return SXRET_OK;` |
|      1 | 7065 | `}` |
|      - | 7066 | `/*` |
|      - | 7067 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 7068 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 7069 | ` * Parameters` |
|      - | 7070 | ` *  $str` |
|      - | 7071 | ` *  The input string.` |
|      - | 7072 | ` * $allowable_tags` |
|      - | 7073 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 7074 | ` * Return` |
|      - | 7075 | ` *  Returns the stripped string.` |
|      - | 7076 | ` */` |
|     14 | 7077 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7078 | `{` |
|     15 | 7079 | `	const char *zTaglist = 0;` |
|      - | 7080 | `	const char *zString;` |
|     15 | 7081 | `	int nTaglen = 0;` |
|      - | 7082 | `	int nLen;` |
|     15 | 7083 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7084 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 7085 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7086 | `		return PH7_OK;` |
|      - | 7087 | `	}` |
|      - | 7088 | `	/* Point to the raw string */` |
|     15 | 7089 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 7090 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 7091 | `		/* Allowed tag */` |
|     11 | 7092 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 7093 | `	}` |
|      - | 7094 | `	/* Process input */` |
|     15 | 7095 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 7096 | `	return PH7_OK;` |
|      8 | 7097 | `}` |
|      - | 7098 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7099 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7100 | `/*` |
|      - | 7101 | ` * string str_shuffle(string $str)` |
|      - | 7102 |  |
|      - | 7103 | ` *  Randomly shuffles a string.` |
|      - | 7104 | ` * Parameters` |
|      - | 7105 | ` *  $str` |
|      - | 7106 | ` *   The input string.` |
|      - | 7107 | ` * Return` |
|      - | 7108 | ` *  Returns the shuffled string.` |
|      - | 7109 | ` */` |
|     10 | 7110 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7111 | `{` |
|      - | 7112 | `	const char *zString;` |
|      - | 7113 | `	int nLen,i,c;` |
|      - | 7114 | `	sxu32 iR;` |
|     11 | 7115 | `	if( nArg < 1 ){` |
|      - | 7116 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7117 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7118 | `		return PH7_OK;` |
|      - | 7119 | `	}` |
|      - | 7120 | `	/* Extract the target string */` |
|     11 | 7121 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 7122 | `	if( nLen < 1 ){` |
|      - | 7123 | `		/* Nothing to shuffle */` |
|      3 | 7124 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 7125 | `		return PH7_OK;` |
|      - | 7126 | `	}` |
|      - | 7127 | `	/* Shuffle the string */` |
|     43 | 7128 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 7129 | `		/* Generate a random number first */` |
|     35 | 7130 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 7131 | `		/* Extract a random offset */` |
|     35 | 7132 | `		c = zString[iR % nLen];` |
|      - | 7133 | `		/* Append it */` |
|     35 | 7134 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 7135 | `	}` |
|      9 | 7136 | `	return PH7_OK;` |
|      6 | 7137 | `}` |
|      - | 7138 | `/*` |
|      - | 7139 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 7140 | ` *  Convert a string to an array.` |
|      - | 7141 | ` * Parameters` |
|      - | 7142 | ` * $string` |
|      - | 7143 | ` *  The input string.` |
|      - | 7144 | ` * $split_length` |
|      - | 7145 | ` *  Maximum length of the chunk.` |
|      - | 7146 | ` * Return` |
|      - | 7147 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 7148 | ` *  except possibly the last one which may be shorter.` |
|      - | 7149 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 7150 | ` *  as the first (and only) array element.` |
|      - | 7151 | ` *  An empty string returns an empty array.` |
|      - | 7152 | ` * Errors` |
|      - | 7153 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 7154 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 7155 | ` *  ValueError if $split_length is less than 1.` |
|      - | 7156 | ` */` |
|     24 | 7157 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 7158 | `{` |
|      - | 7159 | `	const char *zString,*zEnd;` |
|      - | 7160 | `	ph7_value *pArray,*pValue;` |
|      - | 7161 | `	int split_len;` |
|      - | 7162 | `	int nLen;` |
|     27 | 7163 | `	if( nArg < 1 ){` |
|    ! 0 | 7164 | `		return PH7_VmThrowException(pCtx,` |
|      - | 7165 | `			"ArgumentCountError",` |
|      - | 7166 | `			"str_split() expects at least 1 argument, %d given",` |
|    ! 0 | 7167 | `			nArg` |
|      - | 7168 | `			);` |
|      - | 7169 | `	}` |
|      - | 7170 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     36 | 7171 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     39 | 7172 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 7173 | `	    ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 7174 | `		return PH7_VmThrowException(pCtx,` |
|      - | 7175 | `			"TypeError",` |
|      - | 7176 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 7177 | `			ph7_type_name(apArg[0])` |
|      - | 7178 | `			);` |
|      - | 7179 | `	}` |
|      - | 7180 | `	/* Point to the target string */` |
|     27 | 7181 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 7182 | `	split_len = (int)sizeof(char);` |
|     27 | 7183 | `	if( nArg > 1 ){` |
|      - | 7184 | `		/* Split length */` |
|     17 | 7185 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 7186 | `		if( split_len < 1 ){` |
|      6 | 7187 | `			return PH7_VmThrowException(pCtx,` |
|      - | 7188 | `				"ValueError",` |
|      - | 7189 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 7190 | `				);` |
|      - | 7191 | `		}` |
|     11 | 7192 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 7193 | `			split_len = nLen;` |
|      1 | 7194 | `		}` |
|      5 | 7195 | `	}` |
|      - | 7196 | `	/* Create the array and the scalar value */` |
|     21 | 7197 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 7198 | `	/*Chunk value */` |
|     21 | 7199 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 7200 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 7201 | `		/* Return FALSE */` |
|    ! 0 | 7202 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7203 | `		return PH7_OK;` |
|      - | 7204 | `	}` |
|      - | 7205 | `	/* Point to the end of the string */` |
|     21 | 7206 | `	zEnd = &zString[nLen];` |
|      - | 7207 | `	/* Perform the requested operation */` |
|     48 | 7208 | `	for(;;){` |
|      - | 7209 | `		int nMax;` |
|     59 | 7210 | `		if( zString >= zEnd ){` |
|      - | 7211 | `			/* No more input to process */` |
|     21 | 7212 | `			break;` |
|      - | 7213 | `		}` |
|     39 | 7214 | `		nMax = (int)(zEnd-zString);` |
|     39 | 7215 | `		if( nMax < split_len ){` |
|      3 | 7216 | `			split_len = nMax;` |
|      1 | 7217 | `		}` |
|      - | 7218 | `		/* Copy the current chunk */` |
|     39 | 7219 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 7220 | `		/* Insert it */` |
|     39 | 7221 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|    ! 0 | 7222 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 7223 | `		}` |
|      - | 7224 | `		/* reset the string cursor */` |
|     39 | 7225 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 7226 | `		/* Update position */` |
|     39 | 7227 | `		zString += split_len;` |
|      1 | 7228 | `	}` |
|      - | 7229 | `	/*` |
|      - | 7230 | `	 * Return the array.` |
|      - | 7231 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 7232 | `	 * upon we return from this function.` |
|      - | 7233 | `	 */` |
|     21 | 7234 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 7235 | `	return PH7_OK;` |
|     15 | 7236 | `}` |
|      - | 7237 | `/*` |
|      - | 7238 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 7239 | ` * Refer to [strspn()].` |
|      - | 7240 | ` */` |
|     28 | 7241 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 7242 | `{` |
|     29 | 7243 | `	const char *zIn = *pzIn;` |
|      - | 7244 | `	const char *zPtr;` |
|      - | 7245 | `	/* Ignore leading white spaces */` |
|     29 | 7246 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 7247 | `		zIn++;` |
|    ! 0 | 7248 | `	}` |
|     29 | 7249 | `	if( zIn >= zEnd ){` |
|      - | 7250 | `		/* End of input */` |
|    ! 0 | 7251 | `		return SXERR_EOF;` |
|      - | 7252 | `	}` |
|     29 | 7253 | `	zPtr = zIn;` |
|      - | 7254 | `	/* Extract the token */` |
|    201 | 7255 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 7256 | `		zIn++;` |
|      1 | 7257 | `	}` |
|     29 | 7258 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 7259 | `	/* Synchronize pointers */` |
|     29 | 7260 | `	*pzIn = zIn;` |
|      - | 7261 | `	/* Return to the caller */` |
|     29 | 7262 | `	return SXRET_OK;` |
|     15 | 7263 | `}` |
|      - | 7264 | `/*` |
|      - | 7265 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 7266 | ` * return the longest match.` |
|      - | 7267 | ` * Refer to [strspn()].` |
|      - | 7268 | ` */` |
|     18 | 7269 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 7270 | `{` |
|     19 | 7271 | `	const char *zEnd = &zString[nLen];` |
|     19 | 7272 | `	const char *zIn = zString;` |
|      - | 7273 | `	int i,c;` |
|     45 | 7274 | `	for(;;){` |
|     91 | 7275 | `		if( zString >= zEnd ){` |
|      7 | 7276 | `			break;` |
|      - | 7277 | `		}` |
|      - | 7278 | `		/* Extract current character */` |
|     85 | 7279 | `		c = zString[0];` |
|      - | 7280 | `		/* Perform the lookup */` |
|    383 | 7281 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 7282 | `			if( c == zMask[i] ){` |
|      - | 7283 | `				/* Character found */` |
|     73 | 7284 | `				break;` |
|      - | 7285 | `			}` |
|    150 | 7286 | `		}` |
|     85 | 7287 | `		if( i >= nMaskLen ){` |
|      - | 7288 | `			/* Character not in the current mask,break immediately */` |
|     13 | 7289 | `			break;` |
|      - | 7290 | `		}` |
|      - | 7291 | `		/* Advance cursor */` |
|     73 | 7292 | `		zString++;` |
|      1 | 7293 | `	}` |
|      - | 7294 | `	/* Longest match */` |
|     19 | 7295 | `	return (int)(zString-zIn);` |
|      1 | 7296 | `}` |
|      - | 7297 | `/*` |
|      - | 7298 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 7299 | ` * Refer to [strcspn()].` |
|      - | 7300 | ` */` |
|     10 | 7301 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 7302 | `{` |
|     11 | 7303 | `	const char *zEnd = &zString[nLen];` |
|     11 | 7304 | `	const char *zIn = zString;` |
|      - | 7305 | `	int i,c;` |
|     12 | 7306 | `	for(;;){` |
|     25 | 7307 | `		if( zString >= zEnd ){` |
|      3 | 7308 | `			break;` |
|      - | 7309 | `		}` |
|      - | 7310 | `		/* Extract current character */` |
|     23 | 7311 | `		c = zString[0];` |
|      - | 7312 | `		/* Perform the lookup */` |
|     51 | 7313 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 7314 | `			if( c == zMask[i] ){` |
|      9 | 7315 | `				break;` |
|      - | 7316 | `			}` |
|     15 | 7317 | `		}` |
|     23 | 7318 | `		if( i < nMaskLen ){` |
|      - | 7319 | `			/* Character in the current mask,break immediately */` |
|      9 | 7320 | `			break;` |
|      - | 7321 | `		}` |
|      - | 7322 | `		/* Advance cursor */` |
|     15 | 7323 | `		zString++;` |
|      1 | 7324 | `	}` |
|      - | 7325 | `	/* Longest match */` |
|     11 | 7326 | `	return (int)(zString-zIn);` |
|      1 | 7327 | `}` |
|      - | 7328 | `/*` |
|      - | 7329 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 7330 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 7331 | ` *  of characters contained within a given mask.` |
|      - | 7332 | ` * Parameters` |
|      - | 7333 | ` * $str` |
|      - | 7334 | ` *  The input string.` |
|      - | 7335 | ` * $mask` |
|      - | 7336 | ` *  The list of allowable characters.` |
|      - | 7337 | ` * $start` |
|      - | 7338 | ` *  The position in subject to start searching.` |
|      - | 7339 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 7340 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 7341 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 7342 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 7343 | ` *  start'th position from the end of subject.` |
|      - | 7344 | ` * $length` |
|      - | 7345 | ` *  The length of the segment from subject to examine.` |
|      - | 7346 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 7347 | ` *  characters after the starting position.` |
|      - | 7348 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 7349 | ` *  position up to length characters from the end of subject.` |
|      - | 7350 | ` * Return` |
|      - | 7351 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 7352 | ` * in mask.` |
|      - | 7353 | ` */` |
|     24 | 7354 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7355 | `{` |
|      - | 7356 | `	const char *zString,*zMask,*zEnd;` |
|      - | 7357 | `	int iMasklen,iLen;` |
|      - | 7358 | `	SyString sToken;` |
|     25 | 7359 | `	int iCount = 0;` |
|      - | 7360 | `	int rc;` |
|     25 | 7361 | `	if( nArg < 2 ){` |
|      - | 7362 | `		/* Missing agruments,return zero */` |
|    ! 0 | 7363 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7364 | `		return PH7_OK;` |
|      - | 7365 | `	}` |
|      - | 7366 | `	/* Extract the target string */` |
|     25 | 7367 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7368 | `	/* Extract the mask */` |
|     25 | 7369 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 7370 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 7371 | `		/* Nothing to process,return zero */` |
|      7 | 7372 | `		ph7_result_int(pCtx,0);` |
|      7 | 7373 | `		return PH7_OK;` |
|      - | 7374 | `	}` |
|     19 | 7375 | `	if( nArg > 2 ){` |
|      - | 7376 | `		int nOfft;` |
|      - | 7377 | `		/* Extract the offset */` |
|      9 | 7378 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 7379 | `		if( nOfft < 0 ){` |
|    ! 0 | 7380 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 7381 | `			if( zBase > zString ){` |
|    ! 0 | 7382 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 7383 | `				zString = zBase;` |
|    ! 0 | 7384 | `			}else{` |
|      - | 7385 | `				/* Invalid offset */` |
|    ! 0 | 7386 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7387 | `				return PH7_OK;` |
|      - | 7388 | `			}` |
|    ! 0 | 7389 | `		}else{` |
|      9 | 7390 | `			if( nOfft >= iLen ){` |
|      - | 7391 | `				/* Invalid offset */` |
|    ! 0 | 7392 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7393 | `				return PH7_OK;` |
|    ! 0 | 7394 | `			}else{` |
|      - | 7395 | `				/* Update offset */` |
|      9 | 7396 | `				zString += nOfft;` |
|      9 | 7397 | `				iLen -= nOfft;` |
|      - | 7398 | `			}` |
|      - | 7399 | `		}` |
|      9 | 7400 | `		if( nArg > 3 ){` |
|      - | 7401 | `			int iUserlen;` |
|      - | 7402 | `			/* Extract the desired length */` |
|      9 | 7403 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 7404 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 7405 | `				iLen = iUserlen;` |
|      2 | 7406 | `			}` |
|      4 | 7407 | `		}` |
|      4 | 7408 | `	}` |
|      - | 7409 | `	/* Point to the end of the string */` |
|     19 | 7410 | `	zEnd = &zString[iLen];` |
|      - | 7411 | `	/* Extract the first non-space token */` |
|     19 | 7412 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 7413 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 7414 | `		/* Compare against the current mask */` |
|     19 | 7415 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 7416 | `	}` |
|      - | 7417 | `	/* Longest match */` |
|     19 | 7418 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 7419 | `	return PH7_OK;` |
|     13 | 7420 | `}` |
|      - | 7421 | `/*` |
|      - | 7422 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 7423 | ` *  Find length of initial segment not matching mask.` |
|      - | 7424 | ` * Parameters` |
|      - | 7425 | ` * $str` |
|      - | 7426 | ` *  The input string.` |
|      - | 7427 | ` * $mask` |
|      - | 7428 | ` *  The list of not allowed characters.` |
|      - | 7429 | ` * $start` |
|      - | 7430 | ` *  The position in subject to start searching.` |
|      - | 7431 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 7432 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 7433 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 7434 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 7435 | ` *  start'th position from the end of subject.` |
|      - | 7436 | ` * $length` |
|      - | 7437 | ` *  The length of the segment from subject to examine.` |
|      - | 7438 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 7439 | ` *  characters after the starting position.` |
|      - | 7440 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 7441 | ` *  position up to length characters from the end of subject.` |
|      - | 7442 | ` * Return` |
|      - | 7443 | ` *  Returns the length of the segment as an integer.` |
|      - | 7444 | ` */` |
|     14 | 7445 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7446 | `{` |
|      - | 7447 | `	const char *zString,*zMask,*zEnd;` |
|      - | 7448 | `	int iMasklen,iLen;` |
|      - | 7449 | `	SyString sToken;` |
|     15 | 7450 | `	int iCount = 0;` |
|      - | 7451 | `	int rc;` |
|     15 | 7452 | `	if( nArg < 2 ){` |
|      - | 7453 | `		/* Missing agruments,return zero */` |
|    ! 0 | 7454 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7455 | `		return PH7_OK;` |
|      - | 7456 | `	}` |
|      - | 7457 | `	/* Extract the target string */` |
|     15 | 7458 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7459 | `	/* Extract the mask */` |
|     15 | 7460 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 7461 | `	if( iLen < 1 ){` |
|      - | 7462 | `		/* Nothing to process,return zero */` |
|    ! 0 | 7463 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7464 | `		return PH7_OK;` |
|      - | 7465 | `	}` |
|     15 | 7466 | `	if( iMasklen < 1 ){` |
|      - | 7467 | `		/* No given mask,return the string length */` |
|      3 | 7468 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 7469 | `		return PH7_OK;` |
|      - | 7470 | `	}` |
|     13 | 7471 | `	if( nArg > 2 ){` |
|      - | 7472 | `		int nOfft;` |
|      - | 7473 | `		/* Extract the offset */` |
|     11 | 7474 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 7475 | `		if( nOfft < 0 ){` |
|    ! 0 | 7476 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 7477 | `			if( zBase > zString ){` |
|    ! 0 | 7478 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 7479 | `				zString = zBase;` |
|    ! 0 | 7480 | `			}else{` |
|      - | 7481 | `				/* Invalid offset */` |
|    ! 0 | 7482 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7483 | `				return PH7_OK;` |
|      - | 7484 | `			}` |
|    ! 0 | 7485 | `		}else{` |
|     11 | 7486 | `			if( nOfft >= iLen ){` |
|      - | 7487 | `				/* Invalid offset */` |
|      3 | 7488 | `				ph7_result_int(pCtx,0);` |
|      3 | 7489 | `				return PH7_OK;` |
|    ! 0 | 7490 | `			}else{` |
|      - | 7491 | `				/* Update offset */` |
|      9 | 7492 | `				zString += nOfft;` |
|      9 | 7493 | `				iLen -= nOfft;` |
|      - | 7494 | `			}` |
|      - | 7495 | `		}` |
|      9 | 7496 | `		if( nArg > 3 ){` |
|      - | 7497 | `			int iUserlen;` |
|      - | 7498 | `			/* Extract the desired length */` |
|    ! 0 | 7499 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 7500 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 7501 | `				iLen = iUserlen;` |
|    ! 0 | 7502 | `			}` |
|    ! 0 | 7503 | `		}` |
|      4 | 7504 | `	}` |
|      - | 7505 | `	/* Point to the end of the string */` |
|     11 | 7506 | `	zEnd = &zString[iLen];` |
|      - | 7507 | `	/* Extract the first non-space token */` |
|     11 | 7508 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 7509 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 7510 | `		/* Compare against the current mask */` |
|     11 | 7511 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 7512 | `	}` |
|      - | 7513 | `	/* Longest match */` |
|     11 | 7514 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 7515 | `	return PH7_OK;` |
|      8 | 7516 | `}` |
|      - | 7517 | `/*` |
|      - | 7518 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 7519 | ` *  Search a string for any of a set of characters.` |
|      - | 7520 | ` * Parameters` |
|      - | 7521 | ` *  $haystack` |
|      - | 7522 | ` *   The string where char_list is looked for.` |
|      - | 7523 | ` *  $char_list` |
|      - | 7524 | ` *   This parameter is case sensitive.` |
|      - | 7525 | ` * Return` |
|      - | 7526 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 7527 | ` */` |
|      4 | 7528 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7529 | `{` |
|      - | 7530 | `	const char *zString,*zList,*zEnd;` |
|      - | 7531 | `	int iLen,iListLen,i,c;` |
|      - | 7532 | `	sxu32 nOfft,nMax;` |
|      - | 7533 | `	sxi32 rc;` |
|      5 | 7534 | `	if( nArg < 2 ){` |
|      - | 7535 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7536 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7537 | `		return PH7_OK;` |
|      - | 7538 | `	}` |
|      - | 7539 | `	/* Extract the haystack and the char list */` |
|      5 | 7540 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 7541 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 7542 | `	if( iLen < 1 ){` |
|      - | 7543 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7544 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7545 | `		return PH7_OK;` |
|      - | 7546 | `	}` |
|      - | 7547 | `	/* Point to the end of the string */` |
|      5 | 7548 | `	zEnd = &zString[iLen];` |
|      5 | 7549 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 7550 | `	/* perform the requested operation */` |
|     15 | 7551 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 7552 | `		c = zList[i];` |
|     11 | 7553 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 7554 | `		if( rc == SXRET_OK ){` |
|      5 | 7555 | `			if( nMax < nOfft ){` |
|      3 | 7556 | `				nOfft = nMax;` |
|      1 | 7557 | `			}` |
|      2 | 7558 | `		}` |
|      6 | 7559 | `	}` |
|      5 | 7560 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 7561 | `		/* No such substring,return FALSE */` |
|      3 | 7562 | `		ph7_result_bool(pCtx,0);` |
|      2 | 7563 | `	}else{` |
|      - | 7564 | `		/* Return the substring */` |
|      3 | 7565 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 7566 | `	}` |
|      5 | 7567 | `	return PH7_OK;` |
|      3 | 7568 | `}` |
|      - | 7569 | `/* SPDX-SnippetBegin */` |
|      - | 7570 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 7571 | `/* SPDX-License-Identifier: blessing */` |
|      - | 7572 | `/*` |
|      - | 7573 | ` * string soundex(string $str)` |
|      - | 7574 | ` *  Calculate the soundex key of a string.` |
|      - | 7575 | ` * Parameters` |
|      - | 7576 | ` *  $str` |
|      - | 7577 | ` *   The input string.` |
|      - | 7578 | ` * Return` |
|      - | 7579 | ` *  Returns the soundex key as a string.` |
|      - | 7580 | ` * Note:` |
|      - | 7581 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 7582 | ` * source tree.` |
|      - | 7583 | ` */` |
|     22 | 7584 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7585 | `{` |
|      - | 7586 | `	const unsigned char *zIn;` |
|      - | 7587 | `	char zResult[8];` |
|      - | 7588 | `	int i, j;` |
|      - | 7589 | `	static const unsigned char iCode[] = {` |
|      - | 7590 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7591 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7592 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7593 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7594 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 7595 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 7596 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 7597 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 7598 | `	};` |
|     23 | 7599 | `	if( nArg < 1 ){` |
|      - | 7600 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7601 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7602 | `		return PH7_OK;` |
|      - | 7603 | `	}` |
|     23 | 7604 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     35 | 7605 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     23 | 7606 | `	if( zIn[i] ){` |
|     17 | 7607 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 7608 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 7609 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 7610 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 7611 | `			if( code>0 ){` |
|     45 | 7612 | `				if( code!=prevcode ){` |
|     33 | 7613 | `					prevcode = (unsigned char)code;` |
|     33 | 7614 | `					zResult[j++] = (char)code + '0';` |
|     16 | 7615 | `				}` |
|     23 | 7616 | `			}else{` |
|     49 | 7617 | `				prevcode = 0;` |
|      - | 7618 | `			}` |
|     47 | 7619 | `		}` |
|     33 | 7620 | `		while( j<4 ){` |
|     17 | 7621 | `			zResult[j++] = '0';` |
|      1 | 7622 | `		}` |
|     17 | 7623 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 7624 | `	}else{` |
|      - | 7625 | `	  /* No alphabetic character: PHP returns "0000" (not the SQLite "?000"). */` |
|      7 | 7626 | `	  ph7_result_string(pCtx,"0000",4);` |
|      - | 7627 | `	}` |
|     23 | 7628 | `	return PH7_OK;` |
|     12 | 7629 | `}` |
|      - | 7630 | `/* SPDX-SnippetEnd */` |
|      - | 7631 | `/*` |
|      - | 7632 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 7633 | ` *  Wraps a string to a given number of characters.` |
|      - | 7634 | ` * Parameters` |
|      - | 7635 | ` *  $str` |
|      - | 7636 | ` *   The input string.` |
|      - | 7637 | ` * $width` |
|      - | 7638 | ` *  The column width.` |
|      - | 7639 | ` * $break` |
|      - | 7640 | ` *  The line is broken using the optional break parameter.` |
|      - | 7641 | ` * Return` |
|      - | 7642 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 7643 | ` */` |
|     26 | 7644 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7645 | `{` |
|      - | 7646 | `	const char *zIn,*zBreak;` |
|      - | 7647 | `	SyBlob sWorker;` |
|      - | 7648 | `	int iLen,iBreaklen,iWidth,iCut,iStart,iSpace,iCur;` |
|      - | 7649 | `	sxi32 rc;` |
|     27 | 7650 | `	if( nArg < 1 ){` |
|      - | 7651 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7652 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7653 | `		return PH7_OK;` |
|      - | 7654 | `	}` |
|      - | 7655 | `	/* Extract the input string */` |
|     27 | 7656 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7657 | `	/* Width (default 75; PHP allows 0/negative — break at every space). */` |
|     27 | 7658 | `	iWidth = 75;` |
|     27 | 7659 | `	if( nArg > 1 ){` |
|     27 | 7660 | `		iWidth = ph7_value_to_int(apArg[1]);` |
|     13 | 7661 | `	}` |
|      - | 7662 | `	/* Break string (default "\n"). */` |
|     27 | 7663 | `	zBreak = "\n";` |
|     27 | 7664 | `	iBreaklen = (int)sizeof(char);` |
|     27 | 7665 | `	if( nArg > 2 ){` |
|     13 | 7666 | `		zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      6 | 7667 | `	}` |
|      - | 7668 | `	/* Cut long words? (default false). */` |
|     27 | 7669 | `	iCut = 0;` |
|     27 | 7670 | `	if( nArg > 3 ){` |
|      7 | 7671 | `		iCut = ph7_value_to_bool(apArg[3]);` |
|      3 | 7672 | `	}` |
|     27 | 7673 | `	if( iLen < 1 ){` |
|      - | 7674 | `		/* PHP returns the empty string for empty input before validating the other args. */` |
|      5 | 7675 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 7676 | `		return PH7_OK;` |
|      - | 7677 | `	}` |
|      - | 7678 | `	/* PHP 8 domain errors (catchable ValueError). */` |
|     23 | 7679 | `	if( iBreaklen < 1 ){` |
|      3 | 7680 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7681 | `			"wordwrap(): Argument #3 ($break) must not be empty");` |
|      - | 7682 | `	}` |
|     21 | 7683 | `	if( iWidth == 0 && iCut ){` |
|      3 | 7684 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7685 | `			"wordwrap(): Argument #4 ($cut_long_words) cannot be true when argument #2 ($width) is 0");` |
|      - | 7686 | `	}` |
|      - | 7687 | `	/*` |
|      - | 7688 | `	 * PHP's algorithm: a single left-to-right pass tracking the start of the` |
|      - | 7689 | `	 * current line (iStart) and the position of the last space seen on it` |
|      - | 7690 | `	 * (iSpace). A break is emitted when the line reaches the width, at the last` |
|      - | 7691 | `	 * space if there was one, otherwise (only when cut is enabled) hard at the` |
|      - | 7692 | `	 * boundary. An existing break sequence in the input resets the line.` |
|      - | 7693 | `	 */` |
|     19 | 7694 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     19 | 7695 | `	iStart = iSpace = iCur = 0;` |
|     19 | 7696 | `	rc = SXRET_OK;` |
|    551 | 7697 | `	while( iCur < iLen ){` |
|    533 | 7698 | `		if( iBreaklen <= iLen - iCur && SyMemcmp(&zIn[iCur],zBreak,(sxu32)iBreaklen) == 0 ){` |
|      - | 7699 | `			/* Existing break sequence in the input: copy it verbatim and reset the line. */` |
|    ! 0 | 7700 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart + iBreaklen));` |
|    ! 0 | 7701 | `			if( rc != SXRET_OK ){ goto oom; }` |
|    ! 0 | 7702 | `			iCur += iBreaklen;` |
|    ! 0 | 7703 | `			iStart = iSpace = iCur;` |
|    ! 0 | 7704 | `			continue;` |
|    533 | 7705 | `		}else if( zIn[iCur] == ' ' ){` |
|     67 | 7706 | `			if( iCur - iStart >= iWidth ){` |
|      - | 7707 | `				/* The line already fills the width at this space: break here (the space is consumed). */` |
|     13 | 7708 | `				rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     13 | 7709 | `				if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     13 | 7710 | `				if( rc != SXRET_OK ){ goto oom; }` |
|     13 | 7711 | `				iStart = iCur + 1;` |
|      6 | 7712 | `			}` |
|     67 | 7713 | `			iSpace = iCur;` |
|    500 | 7714 | `		}else if( iCut && iCur - iStart >= iWidth && iStart >= iSpace ){` |
|      - | 7715 | `			/* A word longer than the width with no space to break at: hard-cut at the boundary. */` |
|      7 | 7716 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|      7 | 7717 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|      7 | 7718 | `			if( rc != SXRET_OK ){ goto oom; }` |
|      7 | 7719 | `			iStart = iSpace = iCur;` |
|    464 | 7720 | `		}else if( iCur - iStart >= iWidth && iStart < iSpace ){` |
|      - | 7721 | `			/* Past the width mid-word: wrap back to the last space (which is consumed). */` |
|     17 | 7722 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iSpace - iStart));` |
|     17 | 7723 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     17 | 7724 | `			if( rc != SXRET_OK ){ goto oom; }` |
|     17 | 7725 | `			iStart = iSpace = iSpace + 1;` |
|      8 | 7726 | `		}` |
|    533 | 7727 | `		iCur++;` |
|      1 | 7728 | `	}` |
|      - | 7729 | `	/* Emit the trailing chunk. */` |
|     19 | 7730 | `	if( iStart < iCur ){` |
|     19 | 7731 | `		rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     19 | 7732 | `		if( rc != SXRET_OK ){ goto oom; }` |
|      9 | 7733 | `	}` |
|     19 | 7734 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|     19 | 7735 | `	SyBlobRelease(&sWorker);` |
|     19 | 7736 | `	return PH7_OK;` |
|    ! 0 | 7737 | `oom:` |
|    ! 0 | 7738 | `	SyBlobRelease(&sWorker);` |
|    ! 0 | 7739 | `	return PH7_ContextMemoryError(pCtx);` |
|     14 | 7740 | `}` |
|      - | 7741 | `/*` |
|      - | 7742 | ` * Check if the given character is a member of the given mask.` |
|      - | 7743 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 7744 | ` * Refer to [strtok()].` |
|      - | 7745 | ` */` |
|     30 | 7746 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 7747 | `{` |
|      - | 7748 | `	int i;` |
|     57 | 7749 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 7750 | `		if( c == zMask[i] ){` |
|     13 | 7751 | `			if( pOfft ){` |
|      5 | 7752 | `				*pOfft = i;` |
|      2 | 7753 | `			}` |
|     13 | 7754 | `			return TRUE;` |
|      - | 7755 | `		}` |
|     14 | 7756 | `	}` |
|     19 | 7757 | `	return FALSE;` |
|     16 | 7758 | `}` |
|      - | 7759 | `/*` |
|      - | 7760 | ` * Extract a single token from the input stream.` |
|      - | 7761 | ` * Refer to [strtok()].` |
|      - | 7762 | ` */` |
|      6 | 7763 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 7764 | `{` |
|      7 | 7765 | `	const char *zIn = *pzIn;` |
|      - | 7766 | `	const char *zPtr;` |
|      - | 7767 | `	/* Ignore leading delimiter */` |
|     11 | 7768 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 7769 | `		zIn++;` |
|      1 | 7770 | `	}` |
|      7 | 7771 | `	if( zIn >= zEnd ){` |
|      - | 7772 | `		/* End of input */` |
|    ! 0 | 7773 | `		return SXERR_EOF;` |
|      - | 7774 | `	}` |
|      7 | 7775 | `	zPtr = zIn;` |
|      - | 7776 | `	/* Extract the token */` |
|     13 | 7777 | `	while( zIn < zEnd ){` |
|     11 | 7778 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 7779 | `			/* UTF-8 stream */` |
|    ! 0 | 7780 | `			zIn++;` |
|    ! 0 | 7781 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 7782 | `		}else{` |
|     11 | 7783 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 7784 | `				break;` |
|      - | 7785 | `			}` |
|      7 | 7786 | `			zIn++;` |
|      - | 7787 | `		}` |
|      1 | 7788 | `	}` |
|      7 | 7789 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 7790 | `	/* Update the cursor */` |
|      7 | 7791 | `	*pzIn = zIn;` |
|      - | 7792 | `	/* Return to the caller */` |
|      7 | 7793 | `	return SXRET_OK;` |
|      4 | 7794 | `}` |
|      - | 7795 | `/* strtok auxiliary private data */` |
|      - | 7796 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 7797 | `struct strtok_aux_data` |
|      - | 7798 | `{` |
|      - | 7799 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 7800 | `	const char *zIn;   /* Current input stream */` |
|      - | 7801 | `	const char *zEnd;  /* End of input */` |
|      - | 7802 | `};` |
|      - | 7803 | `/*` |
|      - | 7804 | ` * string strtok(string $str,string $token)` |
|      - | 7805 | ` * string strtok(string $token)` |
|      - | 7806 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 7807 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 7808 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 7809 | ` *  words by using the space character as the token.` |
|      - | 7810 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 7811 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 7812 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 7813 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 7814 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 7815 | ` *  the argument are found.` |
|      - | 7816 | ` * Parameters` |
|      - | 7817 | ` *  $str` |
|      - | 7818 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 7819 | ` * $token` |
|      - | 7820 | ` *  The delimiter used when splitting up str.` |
|      - | 7821 | ` * Return` |
|      - | 7822 | ` *   Current token or FALSE on EOF.` |
|      - | 7823 | ` */` |
|      6 | 7824 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7825 | `{` |
|      - | 7826 | `	strtok_aux_data *pAux;` |
|      - | 7827 | `	const char *zMask;` |
|      - | 7828 | `	SyString sToken;` |
|      - | 7829 | `	int nMasklen;` |
|      - | 7830 | `	sxi32 rc;` |
|      7 | 7831 | `	if( nArg < 2 ){` |
|      - | 7832 | `		/* Extract top aux data */` |
|      5 | 7833 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      5 | 7834 | `		if( pAux == 0 ){` |
|      - | 7835 | `			/* No aux data,return FALSE */` |
|    ! 0 | 7836 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7837 | `			return PH7_OK;` |
|      - | 7838 | `		}` |
|      5 | 7839 | `		nMasklen = 0;` |
|      5 | 7840 | `		zMask = ""; /* cc warning */` |
|      5 | 7841 | `		if( nArg > 0 ){` |
|      - | 7842 | `			/* Extract the mask */` |
|      5 | 7843 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 7844 | `		}` |
|      5 | 7845 | `		if( nMasklen < 1 ){` |
|      - | 7846 | `			/* Invalid mask,return FALSE */` |
|    ! 0 | 7847 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 7848 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7849 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 7850 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7851 | `			return PH7_OK;` |
|      - | 7852 | `		}` |
|      - | 7853 | `		/* Extract the token */` |
|      5 | 7854 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 7855 | `		if( rc != SXRET_OK ){` |
|      - | 7856 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 7857 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 7858 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7859 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 7860 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7861 | `		}else{` |
|      - | 7862 | `			/* Return the extracted token */` |
|      5 | 7863 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 7864 | `		}` |
|      3 | 7865 | `	}else{` |
|      - | 7866 | `		const char *zInput,*zCur;` |
|      - | 7867 | `		char *zDup;` |
|      - | 7868 | `		int nLen;` |
|      - | 7869 | `		/* Extract the raw input */` |
|      3 | 7870 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 7871 | `		if( nLen < 1 ){` |
|      - | 7872 | `			/* Empty input,return FALSE */` |
|    ! 0 | 7873 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7874 | `			return PH7_OK;` |
|      - | 7875 | `		}` |
|      - | 7876 | `		/* Extract the mask */` |
|      3 | 7877 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 7878 | `		if( nMasklen < 1 ){` |
|      - | 7879 | `			/* Set a default mask */` |
|      - | 7880 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 7881 | `			zMask = TOK_MASK;` |
|    ! 0 | 7882 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 7883 | `#undef TOK_MASK` |
|    ! 0 | 7884 | `		}` |
|      - | 7885 | `		/* Extract a single token */` |
|      3 | 7886 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 7887 | `		if( rc != SXRET_OK ){` |
|      - | 7888 | `			/* Empty input */` |
|    ! 0 | 7889 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7890 | `			return PH7_OK;` |
|    ! 0 | 7891 | `		}else{` |
|      - | 7892 | `			/* Return the extracted token */` |
|      3 | 7893 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 7894 | `		}` |
|      - | 7895 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 7896 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 7897 | `		if( pAux ){` |
|      3 | 7898 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 7899 | `			if( nLen < 1 ){` |
|    ! 0 | 7900 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7901 | `				return PH7_OK;` |
|      - | 7902 | `			}` |
|      - | 7903 | `			/* Duplicate input */` |
|      3 | 7904 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 7905 | `			if( zDup  ){` |
|      3 | 7906 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 7907 | `				/* Register the aux data */` |
|      3 | 7908 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 7909 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 7910 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 7911 | `			}` |
|      1 | 7912 | `		}` |
|      - | 7913 | `	}` |
|      7 | 7914 | `	return PH7_OK;` |
|      4 | 7915 | `}` |
|      - | 7916 | `/*` |
|      - | 7917 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 7918 | ` *  Pad a string to a certain length with another string` |
|      - | 7919 | ` * Parameters` |
|      - | 7920 | ` *  $input` |
|      - | 7921 | ` *   The input string.` |
|      - | 7922 | ` * $pad_length` |
|      - | 7923 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 7924 | ` *   string, no padding takes place.` |
|      - | 7925 | ` * $pad_string` |
|      - | 7926 | ` *   Note:` |
|      - | 7927 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 7928 | ` *    divided by the pad_string's length.` |
|      - | 7929 | ` * $pad_type` |
|      - | 7930 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 7931 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 7932 | ` * Return` |
|      - | 7933 | ` *  The padded string.` |
|      - | 7934 | ` */` |
|     10 | 7935 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7936 | `{` |
|      - | 7937 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 7938 | `	const char *zIn,*zPad;` |
|     11 | 7939 | `	if( nArg < 2 ){` |
|      - | 7940 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7941 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7942 | `		return PH7_OK;` |
|      - | 7943 | `	}` |
|      - | 7944 | `	/* Extract the target string */` |
|     11 | 7945 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7946 | `	/* Padding length */` |
|      - | 7947 | `	{` |
|     11 | 7948 | `		sxi64 iTmp = 0;` |
|     11 | 7949 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_pad",2,"$length","int",&iTmp);` |
|     11 | 7950 | `		if( rcArg != PH7_OK ){` |
|    ! 0 | 7951 | `			return rcArg;` |
|      - | 7952 | `		}` |
|     11 | 7953 | `		iRealPad = iPadlen = (int)iTmp;` |
|      - | 7954 | `	}` |
|     11 | 7955 | `	if( iPadlen > 0 ){` |
|      9 | 7956 | `		iPadlen -= iLen;` |
|      4 | 7957 | `	}` |
|     11 | 7958 | `	if( iPadlen < 1  ){` |
|      - | 7959 | `		/* Return the string verbatim */` |
|      5 | 7960 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      5 | 7961 | `		return PH7_OK;` |
|      - | 7962 | `	}` |
|      7 | 7963 | `	zPad = " "; /* Whitespace padding */` |
|      7 | 7964 | `	iStrpad = (int)sizeof(char);` |
|      7 | 7965 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      7 | 7966 | `	if( nArg > 2 ){` |
|      - | 7967 | `		/* Padding string */` |
|      7 | 7968 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      7 | 7969 | `		if( iStrpad < 1 ){` |
|      - | 7970 | `			/* An empty pad string throws a catchable ValueError in PHP 8` |
|      - | 7971 | `			 * (only reached once padding is actually required). */` |
|      3 | 7972 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7973 | `				"str_pad(): Argument #3 ($pad_string) must not be empty");` |
|      - | 7974 | `		}` |
|      5 | 7975 | `		if( nArg > 3 ){` |
|      - | 7976 | `			/* Padd type */` |
|      5 | 7977 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 7978 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 7979 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 7980 | `			}` |
|      2 | 7981 | `		}` |
|      2 | 7982 | `	}` |
|      5 | 7983 | `	iDiv = 1;` |
|      5 | 7984 | `	if( iType == 2 ){` |
|    ! 0 | 7985 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 7986 | `	}` |
|      - | 7987 | `	/* Perform the requested operation */` |
|      5 | 7988 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 7989 | `		jPad = iStrpad;` |
|      5 | 7990 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 7991 | `			/* Padding */` |
|      5 | 7992 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 7993 | `				break;` |
|      - | 7994 | `			}` |
|      3 | 7995 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7996 | `		}` |
|      3 | 7997 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 7998 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 7999 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 8000 | `				if( jPad > iStrpad ){` |
|    ! 0 | 8001 | `					jPad = iStrpad;` |
|    ! 0 | 8002 | `				}` |
|      3 | 8003 | `				if( jPad < 1){` |
|    ! 0 | 8004 | `					break;` |
|      - | 8005 | `				}` |
|      3 | 8006 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 8007 | `			}` |
|      1 | 8008 | `		}` |
|      1 | 8009 | `	}` |
|      5 | 8010 | `	if( iLen > 0 ){` |
|      - | 8011 | `		/* Append the input string */` |
|      5 | 8012 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 8013 | `	}` |
|      5 | 8014 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 8015 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 8016 | `			/* Padding */` |
|      5 | 8017 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 8018 | `				break;` |
|      - | 8019 | `			}` |
|      3 | 8020 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 8021 | `		}` |
|      5 | 8022 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 8023 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 8024 | `			if( jPad > iStrpad ){` |
|    ! 0 | 8025 | `				jPad = iStrpad;` |
|    ! 0 | 8026 | `			}` |
|      3 | 8027 | `			if( jPad < 1){` |
|    ! 0 | 8028 | `				break;` |
|      - | 8029 | `			}` |
|      3 | 8030 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 8031 | `		}` |
|      1 | 8032 | `	}` |
|      5 | 8033 | `	return PH7_OK;` |
|      6 | 8034 | `}` |
|      - | 8035 | `/*` |
|      - | 8036 | ` * String replacement private data.` |
|      - | 8037 | ` */` |
|      - | 8038 | `typedef struct str_replace_data str_replace_data;` |
|      - | 8039 | `struct str_replace_data` |
|      - | 8040 | `{` |
|      - | 8041 | `	/* Used by the str_replace family to collect the search/replace arguments. */` |
|      - | 8042 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 8043 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 8044 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 8045 | `};` |
|      - | 8046 | `/*` |
|      - | 8047 | ` * Remove a substring.` |
|      - | 8048 | ` */` |
|      - | 8049 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 8050 | `	for(;;){\` |
|      - | 8051 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 8052 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 8053 | `		++OFFT;\` |
|      - | 8054 | `	}\` |
|      - | 8055 | `}` |
|      - | 8056 | `/*` |
|      - | 8057 | ` * Shift right and insert algorithm.` |
|      - | 8058 | ` */` |
|      - | 8059 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 8060 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 8061 | `		for(;;){\` |
|      - | 8062 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 8063 | `			if(INLEN < 1 ) { break; }\` |
|      - | 8064 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 8065 | `			--INLEN; \` |
|      - | 8066 | `		}\` |
|      - | 8067 | `		for(;;){\` |
|      - | 8068 | `				if(ELEN < 1) { break; }\` |
|      - | 8069 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 8070 | `				OFFT++;\` |
|      - | 8071 | `				ENTRY++;\` |
|      - | 8072 | `				--ELEN;\` |
|      - | 8073 | `		}\` |
|      - | 8074 | `}` |
|      - | 8075 | `/*` |
|      - | 8076 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 8077 | ` * replacement string [i.e: zReplace].` |
|      - | 8078 | ` */` |
|     52 | 8079 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      5 | 8080 | `{` |
|     57 | 8081 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 8082 | `	sxu32 n,m;` |
|     57 | 8083 | `	n = SyBlobLength(pWorker);` |
|     57 | 8084 | `	m = nOfft;` |
|      - | 8085 | `	/* Delete the old entry */` |
|   6591 | 8086 | `	STRDEL(zInput,n,m,nLen);` |
|     57 | 8087 | `	SyBlobLength(pWorker) -= nLen;` |
|     57 | 8088 | `	if( nReplen > 0 ){` |
|     51 | 8089 | `		sxi32 iRep = nReplen;` |
|      - | 8090 | `		sxi32 rc;` |
|      - | 8091 | `		/*` |
|      - | 8092 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 8093 | `		 * string.` |
|      - | 8094 | `		 */` |
|     51 | 8095 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     51 | 8096 | `		if( rc != SXRET_OK ){` |
|      - | 8097 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 8098 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 8099 | `			return rc;` |
|      - | 8100 | `		}` |
|      - | 8101 | `		/* Perform the insertion now */` |
|     51 | 8102 | `		zInput = (char *)SyBlobData(pWorker);` |
|     51 | 8103 | `		n = SyBlobLength(pWorker);` |
|   6381 | 8104 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     51 | 8105 | `		SyBlobLength(pWorker) += nReplen;` |
|     23 | 8106 | `	}` |
|     57 | 8107 | `	return SXRET_OK;` |
|     31 | 8108 | `}` |
|      - | 8109 | `/*` |
|      - | 8110 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 8111 | ` * to collect search/replace string.` |
|      - | 8112 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 8113 | ` */` |
|    162 | 8114 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      5 | 8115 | `{` |
|    167 | 8116 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 8117 | `	SyString sWorker;` |
|      - | 8118 | `	const char *zIn;` |
|      - | 8119 | `	int nByte;` |
|      - | 8120 | `	/* Extract a string representation of the given argument */` |
|    167 | 8121 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|    167 | 8122 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|    167 | 8123 | `	if( nByte > 0 ){` |
|      - | 8124 | `		char *zDup;` |
|      - | 8125 | `		/* Duplicate the chunk */` |
|    165 | 8126 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 8127 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 8128 | `			);` |
|    165 | 8129 | `		if( zDup == 0 ){` |
|      - | 8130 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 8131 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 8132 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 8133 | `			return SXERR_MEM;` |
|      - | 8134 | `		}` |
|    165 | 8135 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 8136 | `		/* Save the chunk */` |
|    165 | 8137 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     80 | 8138 | `	}` |
|      - | 8139 | `	/* Save for later processing */` |
|    167 | 8140 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 8141 | `	/* All done */` |
|     81 | 8142 | `	SXUNUSED(pKey); /* cc warning */` |
|    167 | 8143 | `	return PH7_OK;` |
|     86 | 8144 | `}` |
|      - | 8145 | `/*` |
|      - | 8146 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 8147 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 8148 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 8149 | ` * Parameters` |
|      - | 8150 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 8151 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 8152 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 8153 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 8154 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 8155 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 8156 | ` * $search` |
|      - | 8157 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 8158 | ` *  to designate multiple needles.` |
|      - | 8159 | ` * $replace` |
|      - | 8160 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 8161 | ` *  to designate multiple replacements.` |
|      - | 8162 | ` * $subject` |
|      - | 8163 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 8164 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 8165 | ` *  of subject, and the return value is an array as well.` |
|      - | 8166 | ` * $count (Not used)` |
|      - | 8167 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 8168 | ` * Return` |
|      - | 8169 | ` * This function returns a string or an array with the replaced values.` |
|      - | 8170 | ` */` |
|  29946 | 8171 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 8172 | `{` |
|      - | 8173 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 8174 | `	ProcStringMatch xMatch;` |
|      - | 8175 | `	const char *zIn,*zFunc;` |
|      - | 8176 | `	str_replace_data sRep;` |
|      - | 8177 | `	SyBlob sWorker;` |
|      - | 8178 | `	SySet sReplace;` |
|      - | 8179 | `	SySet sSearch;` |
|      - | 8180 | `	int rep_str;` |
|      - | 8181 | `	int nByte;` |
|      - | 8182 | `	sxi32 rc;` |
|  29951 | 8183 | `	if( nArg < 3 ){` |
|      - | 8184 | `		/* Missing/Invalid arguments,return null */` |
|    ! 0 | 8185 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8186 | `		return PH7_OK;` |
|      - | 8187 | `	}` |
|      - | 8188 | `	/* Initialize fields */` |
|  29951 | 8189 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  29951 | 8190 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  29951 | 8191 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  29951 | 8192 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  29951 | 8193 | `	sRep.pCtx = pCtx;` |
|  29951 | 8194 | `	sRep.pCollector = &sSearch;` |
|  29951 | 8195 | `	rep_str = 0;` |
|      - | 8196 | `	/* Extract the subject */` |
|  29951 | 8197 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  29951 | 8198 | `	if( nByte < 1 ){` |
|      - | 8199 | `		/* Nothing to replace,return the empty string */` |
|     21 | 8200 | `		ph7_result_string(pCtx,"",0);` |
|     21 | 8201 | `		return PH7_OK;` |
|      - | 8202 | `	}` |
|      - | 8203 | `	/* Copy the subject */` |
|  29931 | 8204 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 8205 | `	/* Search string */` |
|  29931 | 8206 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 8207 | `		/* Collect search string */` |
|     81 | 8208 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|     43 | 8209 | `	}else{` |
|      - | 8210 | `		/* Single pattern */` |
|  29855 | 8211 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  29855 | 8212 | `		if( nByte < 1 ){` |
|      - | 8213 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 8214 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 8215 | `			return PH7_OK;` |
|      - | 8216 | `		}` |
|  29851 | 8217 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 8218 | `		/* Save for later processing */` |
|  29851 | 8219 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 8220 | `	}` |
|      - | 8221 | `	/* Replace string */` |
|  29927 | 8222 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 8223 | `		/* Collect replace string */` |
|      7 | 8224 | `		sRep.pCollector = &sReplace;` |
|      7 | 8225 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 8226 | `	}else{` |
|      - | 8227 | `		/* Single needle */` |
|  29921 | 8228 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  29921 | 8229 | `		rep_str = 1;` |
|  29921 | 8230 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 8231 | `		/* Save for later processing */` |
|  29921 | 8232 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 8233 | `	}` |
|      - | 8234 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  29927 | 8235 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 8236 | `		SySetRelease(&sSearch);` |
|    ! 0 | 8237 | `		SySetRelease(&sReplace);` |
|    ! 0 | 8238 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 8239 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8240 | `	}` |
|      - | 8241 | `	/* Reset loop cursors */` |
|  29927 | 8242 | `	SySetResetCursor(&sSearch);` |
|  29927 | 8243 | `	SySetResetCursor(&sReplace);` |
|  29927 | 8244 | `	pReplace = pSearch = 0; /* cc warning */` |
|  29927 | 8245 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 8246 | `	/* Extract function name */` |
|  29927 | 8247 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 8248 | `	/* Set the default pattern match routine */` |
|  29927 | 8249 | `	xMatch = SyBlobSearch;` |
|  29927 | 8250 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 8251 | `		/* Case insensitive pattern match */` |
|     11 | 8252 | `		xMatch = iPatternMatch;` |
|      5 | 8253 | `	}` |
|      - | 8254 | `	/* Start the replace process */` |
|  59925 | 8255 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 8256 | `		sxu32 nCount,nOfft;` |
|  30003 | 8257 | `		if( pSearch->nByte <  1 ){` |
|      - | 8258 | `			/* Empty string,ignore */` |
|      3 | 8259 | `			continue;` |
|      - | 8260 | `		}` |
|      - | 8261 | `		/* Extract the replace string */` |
|  30001 | 8262 | `		if( rep_str ){` |
|  29991 | 8263 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  14998 | 8264 | `		}else{` |
|     11 | 8265 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 8266 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 8267 | `				 * An empty string is used for the rest of replacement values` |
|      - | 8268 | `				 */` |
|      3 | 8269 | `				pReplace = 0;` |
|      1 | 8270 | `			}` |
|      - | 8271 | `		}` |
|  30001 | 8272 | `		if( pReplace == 0 ){` |
|      - | 8273 | `			/* Use an empty string instead */` |
|      3 | 8274 | `			pReplace = &sTemp;` |
|      1 | 8275 | `		}` |
|  30001 | 8276 | `		nOfft = nCount = 0;` |
|  15024 | 8277 | `		for(;;){` |
|  30053 | 8278 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 8279 | `				break;` |
|      - | 8280 | `			}` |
|      - | 8281 | `			/* Perform a pattern lookup */` |
|  45059 | 8282 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  30036 | 8283 | `				pSearch->nByte,&nOfft);` |
|  30041 | 8284 | `			if( rc != SXRET_OK ){` |
|      - | 8285 | `				/* Pattern not found */` |
|  29989 | 8286 | `				break;` |
|      - | 8287 | `			}` |
|      - | 8288 | `			/* Perform the replace operation */` |
|     57 | 8289 | `			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|     57 | 8290 | `			if( rc != SXRET_OK ){` |
|      - | 8291 | `				/* Allocation failure: surface a fatal instead of a partial result */` |
|    ! 0 | 8292 | `				SySetRelease(&sSearch);` |
|    ! 0 | 8293 | `				SySetRelease(&sReplace);` |
|    ! 0 | 8294 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8295 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8296 | `			}` |
|      - | 8297 | `			/* Increment offset counter */` |
|     57 | 8298 | `			nCount += nOfft + pReplace->nByte;` |
|      5 | 8299 | `		}` |
|      5 | 8300 | `	}` |
|      - | 8301 | `	/* All done,clean-up the mess left behind */` |
|  29927 | 8302 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  29927 | 8303 | `	SySetRelease(&sSearch);` |
|  29927 | 8304 | `	SySetRelease(&sReplace);` |
|  29927 | 8305 | `	SyBlobRelease(&sWorker);` |
|  29927 | 8306 | `	if( rc != PH7_OK ){` |
|    ! 0 | 8307 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8308 | `	}` |
|  29927 | 8309 | `	return PH7_OK;` |
|  14978 | 8310 | `}` |
|      - | 8311 | `/*` |
|      - | 8312 | ` * strtr() array form: a single (key,value) pair copied out of the replace_pairs` |
|      - | 8313 | ` * array. The bytes are owned by a persistent pool (see strtr_collect) rather than` |
|      - | 8314 | ` * the transient walker values, which HashmapWalk releases after each callback, so` |
|      - | 8315 | ` * we store byte offsets into that pool instead of raw pointers.` |
|      - | 8316 | ` */` |
|      - | 8317 | `typedef struct strtr_entry strtr_entry;` |
|      - | 8318 | `struct strtr_entry` |
|      - | 8319 | `{` |
|      - | 8320 | `	sxu32 nKeyOfft; /* Offset of the search key inside the pool */` |
|      - | 8321 | `	sxu32 nKeyLen;  /* Length of the search key */` |
|      - | 8322 | `	sxu32 nValOfft; /* Offset of the replacement inside the pool */` |
|      - | 8323 | `	sxu32 nValLen;  /* Length of the replacement */` |
|      - | 8324 | `};` |
|      - | 8325 | `typedef struct strtr_collect strtr_collect;` |
|      - | 8326 | `struct strtr_collect` |
|      - | 8327 | `{` |
|      - | 8328 | `	SyBlob *pPool;  /* Byte pool holding copied key + value bytes */` |
|      - | 8329 | `	SySet  *pTable; /* Set of strtr_entry (parallel offsets into pPool) */` |
|      - | 8330 | `	sxi32   rc;     /* Carries an allocation failure (SXERR_MEM) out of the walker */` |
|      - | 8331 | `};` |
|      - | 8332 | `/*` |
|      - | 8333 | ` * Collect one replace_pairs entry into the persistent pool/offset table.` |
|      - | 8334 | ` * PHP coerces both the key and the value to string (an integer key becomes its` |
|      - | 8335 | ` * decimal form) and ignores an empty-string key.` |
|      - | 8336 | ` */` |
|     20 | 8337 | `static int StrtrCollectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 8338 | `{` |
|     21 | 8339 | `	strtr_collect *pCol = (strtr_collect *)pUserData;` |
|      - | 8340 | `	const char *zKey,*zVal;` |
|      - | 8341 | `	strtr_entry sEnt;` |
|      - | 8342 | `	int nKey,nVal;` |
|     21 | 8343 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|     21 | 8344 | `	if( nKey < 1 ){` |
|      - | 8345 | `		/* PHP ignores an empty-string key (it also emits a warning we do not replicate). */` |
|      3 | 8346 | `		return PH7_OK;` |
|      - | 8347 | `	}` |
|     19 | 8348 | `	zVal = ph7_value_to_string(pData,&nVal);` |
|     19 | 8349 | `	sEnt.nKeyOfft = SyBlobLength(pCol->pPool);` |
|     19 | 8350 | `	sEnt.nKeyLen  = (sxu32)nKey;` |
|     19 | 8351 | `	if( SyBlobAppend(pCol->pPool,(const void *)zKey,(sxu32)nKey) != SXRET_OK ){` |
|    ! 0 | 8352 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8353 | `		return SXERR_ABORT;` |
|      - | 8354 | `	}` |
|     19 | 8355 | `	sEnt.nValOfft = SyBlobLength(pCol->pPool);` |
|     19 | 8356 | `	sEnt.nValLen  = (sxu32)nVal;` |
|     19 | 8357 | `	if( nVal > 0 && SyBlobAppend(pCol->pPool,(const void *)zVal,(sxu32)nVal) != SXRET_OK ){` |
|    ! 0 | 8358 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8359 | `		return SXERR_ABORT;` |
|      - | 8360 | `	}` |
|     19 | 8361 | `	if( SySetPut(pCol->pTable,(const void *)&sEnt) != SXRET_OK ){` |
|    ! 0 | 8362 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8363 | `		return SXERR_ABORT;` |
|      - | 8364 | `	}` |
|     19 | 8365 | `	return PH7_OK;` |
|     11 | 8366 | `}` |
|      - | 8367 | `/*` |
|      - | 8368 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 8369 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 8370 | ` *  Translate characters or replace substrings.` |
|      - | 8371 | ` * Parameters` |
|      - | 8372 | ` *  $str` |
|      - | 8373 | ` *  The string being translated.` |
|      - | 8374 | ` * $from` |
|      - | 8375 | ` *  The string being translated to to.` |
|      - | 8376 | ` * $to` |
|      - | 8377 | ` *  The string replacing from.` |
|      - | 8378 | ` * $replace_pairs` |
|      - | 8379 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 8380 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 8381 | ` * Return` |
|      - | 8382 | ` *  The translated string.` |
|      - | 8383 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 8384 | ` */` |
|     12 | 8385 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8386 | `{` |
|      - | 8387 | `	const char *zIn;` |
|      - | 8388 | `	int nLen;` |
|     13 | 8389 | `	if( nArg < 1 ){` |
|      - | 8390 | `		/* Nothing to replace,return FALSE */` |
|    ! 0 | 8391 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8392 | `		return PH7_OK;` |
|      - | 8393 | `	}` |
|     13 | 8394 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 8395 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 8396 | `		/* Invalid arguments */` |
|    ! 0 | 8397 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8398 | `		return PH7_OK;` |
|      - | 8399 | `	}` |
|     18 | 8400 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 8401 | `		strtr_collect sCol;` |
|      - | 8402 | `		SyBlob sPool,sWorker;` |
|      - | 8403 | `		SySet sTable;` |
|      - | 8404 | `		const char *zPool;` |
|      - | 8405 | `		strtr_entry *pEnt;` |
|      - | 8406 | `		sxi32 rc;` |
|      - | 8407 | `		int i,iRun;` |
|      - | 8408 | `		/*` |
|      - | 8409 | `		 * PHP's array-form strtr is a single left-to-right pass over the subject:` |
|      - | 8410 | `		 * at every position it substitutes the LONGEST replace_pairs key that` |
|      - | 8411 | `		 * matches there, then advances past the key (replacements are never` |
|      - | 8412 | `		 * rescanned). It is not a sequential per-key global replace. First copy` |
|      - | 8413 | `		 * the pairs into a persistent pool, then run that scan.` |
|      - | 8414 | `		 */` |
|     11 | 8415 | `		SyBlobInit(&sPool,&pCtx->pVm->sAllocator);` |
|     11 | 8416 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     11 | 8417 | `		SySetInit(&sTable,&pCtx->pVm->sAllocator,sizeof(strtr_entry));` |
|     11 | 8418 | `		sCol.pPool  = &sPool;` |
|     11 | 8419 | `		sCol.pTable = &sTable;` |
|     11 | 8420 | `		sCol.rc     = SXRET_OK;` |
|     11 | 8421 | `		ph7_array_walk(apArg[1],StrtrCollectWalker,&sCol);` |
|     11 | 8422 | `		if( sCol.rc != SXRET_OK ){` |
|      - | 8423 | `			/* Allocation failure while collecting the pairs: surface a fatal */` |
|    ! 0 | 8424 | `			SyBlobRelease(&sPool);` |
|    ! 0 | 8425 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 8426 | `			SySetRelease(&sTable);` |
|    ! 0 | 8427 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 8428 | `		}` |
|      - | 8429 | `		/* The pool is now stable, so offsets can be resolved against its base. */` |
|     11 | 8430 | `		zPool = (const char *)SyBlobData(&sPool);` |
|     11 | 8431 | `		rc = SXRET_OK;` |
|     11 | 8432 | `		iRun = 0; /* Start of the pending run of unmatched bytes copied verbatim. */` |
|     43 | 8433 | `		for( i = 0 ; i < nLen ; ){` |
|     33 | 8434 | `			strtr_entry *pBest = 0;` |
|     33 | 8435 | `			sxu32 nBest = 0;` |
|      - | 8436 | `			/* Pick the longest key that matches at the current position. */` |
|     33 | 8437 | `			SySetResetCursor(&sTable);` |
|     87 | 8438 | `			while( SXRET_OK == SySetGetNextEntry(&sTable,(void **)&pEnt) ){` |
|     54 | 8439 | `				if( pEnt->nKeyLen > nBest` |
|     50 | 8440 | `					&& pEnt->nKeyLen <= (sxu32)(nLen - i)` |
|     46 | 8441 | `					&& SyMemcmp(zPool + pEnt->nKeyOfft,zIn + i,pEnt->nKeyLen) == 0 ){` |
|     29 | 8442 | `					nBest = pEnt->nKeyLen;` |
|     29 | 8443 | `					pBest = pEnt;` |
|     14 | 8444 | `				}` |
|      1 | 8445 | `			}` |
|     33 | 8446 | `			if( pBest == 0 ){` |
|      - | 8447 | `				/* No key here: extend the literal run and copy it in one shot later. */` |
|      9 | 8448 | `				i++;` |
|      9 | 8449 | `				continue;` |
|      - | 8450 | `			}` |
|      - | 8451 | `			/* Flush the pending literal run, then the replacement. */` |
|     25 | 8452 | `			if( i > iRun ){` |
|      5 | 8453 | `				rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(i - iRun));` |
|      2 | 8454 | `			}` |
|     25 | 8455 | `			if( rc == SXRET_OK && pBest->nValLen > 0 ){` |
|     25 | 8456 | `				rc = SyBlobAppend(&sWorker,zPool + pBest->nValOfft,pBest->nValLen);` |
|     12 | 8457 | `			}` |
|     25 | 8458 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8459 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 8460 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8461 | `				SySetRelease(&sTable);` |
|    ! 0 | 8462 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8463 | `			}` |
|     25 | 8464 | `			i += (int)pBest->nKeyLen;` |
|     25 | 8465 | `			iRun = i;` |
|      1 | 8466 | `		}` |
|      - | 8467 | `		/* Flush the trailing literal run. */` |
|     11 | 8468 | `		if( nLen > iRun ){` |
|      3 | 8469 | `			rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(nLen - iRun));` |
|      3 | 8470 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8471 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 8472 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8473 | `				SySetRelease(&sTable);` |
|    ! 0 | 8474 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8475 | `			}` |
|      1 | 8476 | `		}` |
|      - | 8477 | `		/* All done, return the result string */` |
|     16 | 8478 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|     10 | 8479 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 8480 | `		/* Clean-up */` |
|     11 | 8481 | `		SyBlobRelease(&sPool);` |
|     11 | 8482 | `		SyBlobRelease(&sWorker);` |
|     11 | 8483 | `		SySetRelease(&sTable);` |
|     11 | 8484 | `		if( rc != PH7_OK ){` |
|    ! 0 | 8485 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 8486 | `		}` |
|      6 | 8487 | `	}else{` |
|      - | 8488 | `		int i,flen,tlen,c,iOfft;` |
|      - | 8489 | `		const char *zFrom,*zTo;` |
|      3 | 8490 | `		if( nArg < 3 ){` |
|      - | 8491 | `			/* Nothing to replace */` |
|    ! 0 | 8492 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8493 | `			return PH7_OK;` |
|      - | 8494 | `		}` |
|      - | 8495 | `		/* Extract given arguments */` |
|      3 | 8496 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 8497 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 8498 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 8499 | `			/* Nothing to replace */` |
|    ! 0 | 8500 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8501 | `			return PH7_OK;` |
|      - | 8502 | `		}` |
|      - | 8503 | `		/* Start the replace process */` |
|     13 | 8504 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 8505 | `			c = zIn[i];` |
|     11 | 8506 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 8507 | `				if ( iOfft < tlen ){` |
|      5 | 8508 | `					c = zTo[iOfft];` |
|      2 | 8509 | `				}` |
|      2 | 8510 | `			}` |
|     11 | 8511 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 8512 |  |
|      6 | 8513 | `		}` |
|      - | 8514 | `	}` |
|     13 | 8515 | `	return PH7_OK;` |
|      7 | 8516 | `}` |
|      - | 8517 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8518 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8519 | `/*` |
|      - | 8520 | ` * Parse an INI string.` |
|      - | 8521 |  |
|      - | 8522 | ` * According to wikipedia` |
|      - | 8523 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 8524 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 8525 | ` *  Format` |
|      - | 8526 | `*    Properties` |
|      - | 8527 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 8528 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 8529 | `*     Example:` |
|      - | 8530 | `*      name=value` |
|      - | 8531 | `*    Sections` |
|      - | 8532 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 8533 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 8534 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 8535 | `*     or the end of the file. Sections may not be nested.` |
|      - | 8536 | `*     Example:` |
|      - | 8537 | `*      [section]` |
|      - | 8538 | `*   Comments` |
|      - | 8539 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 8540 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 8541 | `*/` |
|     12 | 8542 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 8543 | `{` |
|      - | 8544 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 8545 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 8546 | `	SyHashEntry *pEntry;` |
|      - | 8547 | `	SyString sEntry;` |
|      - | 8548 | `	SyHash sHash;` |
|      - | 8549 | `	int c;` |
|      - | 8550 | `	/* Create an empty array and worker variables */` |
|     13 | 8551 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 8552 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 8553 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 8554 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 8555 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|    ! 0 | 8556 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8557 | `	}` |
|     13 | 8558 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 8559 | `	pCur = pArray;` |
|      - | 8560 | `	/* Start the parse process */` |
|     21 | 8561 | `	for(;;){` |
|      - | 8562 | `		/* Ignore leading white spaces */` |
|     69 | 8563 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 8564 | `			zIn++;` |
|      1 | 8565 | `		}` |
|     43 | 8566 | `		if( zIn >= zEnd ){` |
|      - | 8567 | `			/* No more input to process */` |
|     13 | 8568 | `			break;` |
|      - | 8569 | `		}` |
|     31 | 8570 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 8571 | `			/* Comment til the end of line */` |
|    ! 0 | 8572 | `			zIn++;` |
|    ! 0 | 8573 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 8574 | `				zIn++;` |
|    ! 0 | 8575 | `			}` |
|    ! 0 | 8576 | `			continue;` |
|      - | 8577 | `		}` |
|      - | 8578 | `		/* Reset the string cursor of the working variable */` |
|     31 | 8579 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 8580 | `		if( zIn[0] == '[' ){` |
|      - | 8581 | `			/* Section: Extract the section name */` |
|      9 | 8582 | `			zIn++;` |
|      9 | 8583 | `			zCur = zIn;` |
|     73 | 8584 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 8585 | `				zIn++;` |
|      1 | 8586 | `			}` |
|      9 | 8587 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 8588 | `				/* Save the section name */` |
|      5 | 8589 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 8590 | `				SyStringFullTrim(&sEntry);` |
|      5 | 8591 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 8592 | `				if( sEntry.nByte > 0 ){` |
|      - | 8593 | `					/* Associate an array with the section */` |
|      5 | 8594 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 8595 | `					if( pSection ){` |
|      5 | 8596 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 8597 | `						pCur = pSection;` |
|      2 | 8598 | `					}` |
|      2 | 8599 | `				}` |
|      2 | 8600 | `			}` |
|      9 | 8601 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 8602 | `		}else{` |
|      - | 8603 | `			ph7_value *pOldCur;` |
|      - | 8604 | `			int is_array;` |
|      - | 8605 | `			int iLen;` |
|      - | 8606 | `			/* Properties */` |
|     23 | 8607 | `			is_array = 0;` |
|     23 | 8608 | `			zCur = zIn;` |
|     23 | 8609 | `			iLen = 0; /* cc warning */` |
|     23 | 8610 | `			pOldCur = pCur;` |
|    155 | 8611 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 8612 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 8613 | `					/* Array */` |
|    ! 0 | 8614 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 8615 | `					is_array = 1;` |
|    ! 0 | 8616 | `					if( iLen > 0 ){` |
|    ! 0 | 8617 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 8618 | `						/* Query the hashtable */` |
|    ! 0 | 8619 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 8620 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 8621 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 8622 | `						if( pEntry ){` |
|    ! 0 | 8623 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 8624 | `						}else{` |
|      - | 8625 | `							/* Create an empty array */` |
|    ! 0 | 8626 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 8627 | `							if( pvArr ){` |
|      - | 8628 | `								/* Save the entry */` |
|    ! 0 | 8629 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 8630 | `								/* Insert the entry */` |
|    ! 0 | 8631 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 8632 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 8633 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 8634 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 8635 | `							}` |
|      - | 8636 | `						}` |
|    ! 0 | 8637 | `						if( pvArr ){` |
|    ! 0 | 8638 | `							pCur = pvArr;` |
|    ! 0 | 8639 | `						}` |
|    ! 0 | 8640 | `					}` |
|    ! 0 | 8641 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 8642 | `						zIn++;` |
|    ! 0 | 8643 | `					}` |
|    ! 0 | 8644 | `				}` |
|    133 | 8645 | `				zIn++;` |
|      1 | 8646 | `			}` |
|     23 | 8647 | `			if( !is_array ){` |
|     23 | 8648 | `				iLen = (int)(zIn-zCur);` |
|     11 | 8649 | `			}` |
|      - | 8650 | `			/* Trim the key */` |
|     23 | 8651 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 8652 | `			SyStringFullTrim(&sEntry);` |
|     23 | 8653 | `			if( sEntry.nByte > 0 ){` |
|     23 | 8654 | `				if( !is_array ){` |
|      - | 8655 | `					/* Save the key name */` |
|     23 | 8656 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 8657 | `				}` |
|      - | 8658 | `				/* extract key value */` |
|     23 | 8659 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 8660 | `				zIn++; /* '=' */` |
|     39 | 8661 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 8662 | `					zIn++;` |
|      1 | 8663 | `				}` |
|     23 | 8664 | `				if( zIn < zEnd ){` |
|     21 | 8665 | `					zCur = zIn;` |
|     21 | 8666 | `					c = zIn[0];` |
|     21 | 8667 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 8668 | `						zIn++;` |
|      - | 8669 | `						/* Delimit the value */` |
|    ! 0 | 8670 | `						while( zIn < zEnd ){` |
|    ! 0 | 8671 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 8672 | `								break;` |
|      - | 8673 | `							}` |
|    ! 0 | 8674 | `							zIn++;` |
|    ! 0 | 8675 | `						}` |
|    ! 0 | 8676 | `						if( zIn < zEnd ){` |
|    ! 0 | 8677 | `							zIn++;` |
|    ! 0 | 8678 | `						}` |
|    ! 0 | 8679 | `					}else{` |
|    125 | 8680 | `						while( zIn < zEnd ){` |
|    123 | 8681 | `							if( zIn[0] == '\n' ){` |
|     19 | 8682 | `								if( zIn[-1] != '\\' ){` |
|     19 | 8683 | `									break;` |
|    ! 0 | 8684 | `								}` |
|    105 | 8685 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 8686 | `								/* Inline comments */` |
|    ! 0 | 8687 | `								break;` |
|      - | 8688 | `							}` |
|    105 | 8689 | `							zIn++;` |
|      1 | 8690 | `						}` |
|      - | 8691 | `					}` |
|      - | 8692 | `					/* Trim the value */` |
|     21 | 8693 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 8694 | `					SyStringFullTrim(&sEntry);` |
|     21 | 8695 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 8696 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 8697 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 8698 | `					}` |
|     21 | 8699 | `					if( sEntry.nByte > 0 ){` |
|     21 | 8700 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 8701 | `					}` |
|      - | 8702 | `					/* Insert the key and it's value */` |
|     21 | 8703 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 8704 | `				}` |
|     12 | 8705 | `			}else{` |
|    ! 0 | 8706 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 8707 | `					zIn++;` |
|    ! 0 | 8708 | `				}` |
|      - | 8709 | `			}` |
|     23 | 8710 | `			pCur = pOldCur;` |
|      - | 8711 | `		}` |
|      1 | 8712 | `	}` |
|     13 | 8713 | `	SyHashRelease(&sHash);` |
|      - | 8714 | `	/* Return the parse of the INI string */` |
|     13 | 8715 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 8716 | `	return SXRET_OK;` |
|      7 | 8717 | `}` |
|      - | 8718 | `/*` |
|      - | 8719 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 8720 | ` *  Parse a configuration string.` |
|      - | 8721 | ` * Parameters` |
|      - | 8722 | ` *  $ini` |
|      - | 8723 | ` *   The contents of the ini file being parsed.` |
|      - | 8724 | ` *  $process_sections` |
|      - | 8725 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 8726 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 8727 | ` *  $scanner_mode (Not used)` |
|      - | 8728 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 8729 | ` *   then option values will not be parsed.` |
|      - | 8730 | ` * Return` |
|      - | 8731 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 8732 | ` */` |
|     10 | 8733 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8734 | `{` |
|      - | 8735 | `	const char *zIni;` |
|      - | 8736 | `	int nByte;` |
|     11 | 8737 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8738 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 8739 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8740 | `		return PH7_OK;` |
|      - | 8741 | `	}` |
|      - | 8742 | `	/* Extract the raw INI buffer */` |
|     11 | 8743 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 8744 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|     11 | 8745 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      6 | 8746 | `}` |
|      - | 8747 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8748 |  |
|      - | 8749 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8750 |  |
|      - | 8751 | `/*` |
|      - | 8752 | ` * Ctype Functions.` |
|      - | 8753 | ` * Status:` |
|      - | 8754 | ` *    Stable.` |
|      - | 8755 | ` */` |
|      - | 8756 | `/*` |
|      - | 8757 | ` * bool ctype_alnum(string $text)` |
|      - | 8758 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 8759 | ` * Parameters` |
|      - | 8760 | ` *  $text` |
|      - | 8761 | ` *   The tested string.` |
|      - | 8762 | ` * Return` |
|      - | 8763 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 8764 | ` */` |
|     72 | 8765 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8766 | `{` |
|      - | 8767 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8768 | `	int nLen;` |
|     73 | 8769 | `	if( nArg < 1 ){` |
|      - | 8770 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8771 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8772 | `		return PH7_OK;` |
|      - | 8773 | `	}` |
|      - | 8774 | `	/* Extract the target string */` |
|     73 | 8775 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     73 | 8776 | `	zEnd = &zIn[nLen];` |
|     73 | 8777 | `	if( nLen < 1 ){` |
|      - | 8778 | `		/* Empty string,return FALSE */` |
|      3 | 8779 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8780 | `		return PH7_OK;` |
|      - | 8781 | `	}` |
|      - | 8782 | `	/* Perform the requested operation */` |
|    110 | 8783 | `	for(;;){` |
|    221 | 8784 | `		if( zIn >= zEnd ){` |
|      - | 8785 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     65 | 8786 | `			ph7_result_bool(pCtx,1);` |
|     65 | 8787 | `			return PH7_OK;` |
|      - | 8788 | `		}` |
|    157 | 8789 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      7 | 8790 | `			break;` |
|      - | 8791 | `		}` |
|      - | 8792 | `		/* Point to the next character */` |
|    151 | 8793 | `		zIn++;` |
|      1 | 8794 | `	}` |
|      - | 8795 | `	/* The test failed,return FALSE */` |
|      7 | 8796 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8797 | `	return PH7_OK;` |
|     37 | 8798 | `}` |
|      - | 8799 | `/*` |
|      - | 8800 | ` * bool ctype_alpha(string $text)` |
|      - | 8801 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 8802 | ` * Parameters` |
|      - | 8803 | ` *  $text` |
|      - | 8804 | ` *   The tested string.` |
|      - | 8805 | ` * Return` |
|      - | 8806 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 8807 | ` */` |
|     16 | 8808 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8809 | `{` |
|      - | 8810 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8811 | `	int nLen;` |
|     17 | 8812 | `	if( nArg < 1 ){` |
|      - | 8813 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8814 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8815 | `		return PH7_OK;` |
|      - | 8816 | `	}` |
|      - | 8817 | `	/* Extract the target string */` |
|     17 | 8818 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8819 | `	zEnd = &zIn[nLen];` |
|     17 | 8820 | `	if( nLen < 1 ){` |
|      - | 8821 | `		/* Empty string,return FALSE */` |
|      3 | 8822 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8823 | `		return PH7_OK;` |
|      - | 8824 | `	}` |
|      - | 8825 | `	/* Perform the requested operation */` |
|     42 | 8826 | `	for(;;){` |
|     85 | 8827 | `		if( zIn >= zEnd ){` |
|      - | 8828 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8829 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8830 | `			return PH7_OK;` |
|      - | 8831 | `		}` |
|     77 | 8832 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 8833 | `			break;` |
|      - | 8834 | `		}` |
|      - | 8835 | `		/* Point to the next character */` |
|     71 | 8836 | `		zIn++;` |
|      1 | 8837 | `	}` |
|      - | 8838 | `	/* The test failed,return FALSE */` |
|      7 | 8839 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8840 | `	return PH7_OK;` |
|      9 | 8841 | `}` |
|      - | 8842 | `/*` |
|      - | 8843 | ` * bool ctype_cntrl(string $text)` |
|      - | 8844 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 8845 | ` * Parameters` |
|      - | 8846 | ` *  $text` |
|      - | 8847 | ` *   The tested string.` |
|      - | 8848 | ` * Return` |
|      - | 8849 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 8850 | ` */` |
|     16 | 8851 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8852 | `{` |
|      - | 8853 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8854 | `	int nLen;` |
|     17 | 8855 | `	if( nArg < 1 ){` |
|      - | 8856 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8857 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8858 | `		return PH7_OK;` |
|      - | 8859 | `	}` |
|      - | 8860 | `	/* Extract the target string */` |
|     17 | 8861 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8862 | `	zEnd = &zIn[nLen];` |
|     17 | 8863 | `	if( nLen < 1 ){` |
|      - | 8864 | `		/* Empty string,return FALSE */` |
|      3 | 8865 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8866 | `		return PH7_OK;` |
|      - | 8867 | `	}` |
|      - | 8868 | `	/* Perform the requested operation */` |
|     14 | 8869 | `	for(;;){` |
|     29 | 8870 | `		if( zIn >= zEnd ){` |
|      - | 8871 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8872 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8873 | `			return PH7_OK;` |
|      - | 8874 | `		}` |
|     21 | 8875 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8876 | `			/* UTF-8 stream  */` |
|    ! 0 | 8877 | `			break;` |
|      - | 8878 | `		}` |
|     21 | 8879 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 8880 | `			break;` |
|      - | 8881 | `		}` |
|      - | 8882 | `		/* Point to the next character */` |
|     15 | 8883 | `		zIn++;` |
|      1 | 8884 | `	}` |
|      - | 8885 | `	/* The test failed,return FALSE */` |
|      7 | 8886 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8887 | `	return PH7_OK;` |
|      9 | 8888 | `}` |
|      - | 8889 | `/*` |
|      - | 8890 | ` * bool ctype_digit(string $text)` |
|      - | 8891 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 8892 | ` * Parameters` |
|      - | 8893 | ` *  $text` |
|      - | 8894 | ` *   The tested string.` |
|      - | 8895 | ` * Return` |
|      - | 8896 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 8897 | ` */` |
|   2098 | 8898 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 8899 | `{` |
|      - | 8900 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8901 | `	int nLen;` |
|   2103 | 8902 | `	if( nArg < 1 ){` |
|      - | 8903 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8904 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8905 | `		return PH7_OK;` |
|      - | 8906 | `	}` |
|      - | 8907 | `	/* Extract the target string */` |
|   2103 | 8908 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   2103 | 8909 | `	zEnd = &zIn[nLen];` |
|   2103 | 8910 | `	if( nLen < 1 ){` |
|      - | 8911 | `		/* Empty string,return FALSE */` |
|      3 | 8912 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8913 | `		return PH7_OK;` |
|      - | 8914 | `	}` |
|      - | 8915 | `	/* Perform the requested operation */` |
|   1944 | 8916 | `	for(;;){` |
|   3893 | 8917 | `		if( zIn >= zEnd ){` |
|      - | 8918 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1735 | 8919 | `			ph7_result_bool(pCtx,1);` |
|   1735 | 8920 | `			return PH7_OK;` |
|      - | 8921 | `		}` |
|   2163 | 8922 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8923 | `			/* UTF-8 stream  */` |
|    ! 0 | 8924 | `			break;` |
|      - | 8925 | `		}` |
|   2163 | 8926 | `		if( !SyisDigit(zIn[0]) ){` |
|    371 | 8927 | `			break;` |
|      - | 8928 | `		}` |
|      - | 8929 | `		/* Point to the next character */` |
|   1797 | 8930 | `		zIn++;` |
|      5 | 8931 | `	}` |
|      - | 8932 | `	/* The test failed,return FALSE */` |
|    371 | 8933 | `	ph7_result_bool(pCtx,0);` |
|    371 | 8934 | `	return PH7_OK;` |
|   1054 | 8935 | `}` |
|      - | 8936 | `/*` |
|      - | 8937 | ` * bool ctype_xdigit(string $text)` |
|      - | 8938 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 8939 | ` * Parameters` |
|      - | 8940 | ` *  $text` |
|      - | 8941 | ` *   The tested string.` |
|      - | 8942 | ` * Return` |
|      - | 8943 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 8944 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 8945 | ` */` |
|     38 | 8946 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 8947 | `{` |
|      - | 8948 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8949 | `	int nLen;` |
|     40 | 8950 | `	if( nArg < 1 ){` |
|      - | 8951 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8952 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8953 | `		return PH7_OK;` |
|      - | 8954 | `	}` |
|      - | 8955 | `	/* Extract the target string */` |
|     40 | 8956 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     40 | 8957 | `	zEnd = &zIn[nLen];` |
|     40 | 8958 | `	if( nLen < 1 ){` |
|      - | 8959 | `		/* Empty string,return FALSE */` |
|      3 | 8960 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8961 | `		return PH7_OK;` |
|      - | 8962 | `	}` |
|      - | 8963 | `	/* Perform the requested operation */` |
|     76 | 8964 | `	for(;;){` |
|    154 | 8965 | `		if( zIn >= zEnd ){` |
|      - | 8966 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     32 | 8967 | `			ph7_result_bool(pCtx,1);` |
|     32 | 8968 | `			return PH7_OK;` |
|      - | 8969 | `		}` |
|    124 | 8970 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8971 | `			/* UTF-8 stream  */` |
|    ! 0 | 8972 | `			break;` |
|      - | 8973 | `		}` |
|    124 | 8974 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 8975 | `			break;` |
|      - | 8976 | `		}` |
|      - | 8977 | `		/* Point to the next character */` |
|    118 | 8978 | `		zIn++;` |
|      2 | 8979 | `	}` |
|      - | 8980 | `	/* The test failed,return FALSE */` |
|      7 | 8981 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8982 | `	return PH7_OK;` |
|     21 | 8983 | `}` |
|      - | 8984 | `/*` |
|      - | 8985 | ` * bool ctype_graph(string $text)` |
|      - | 8986 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 8987 | ` * Parameters` |
|      - | 8988 | ` *  $text` |
|      - | 8989 | ` *   The tested string.` |
|      - | 8990 | ` * Return` |
|      - | 8991 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 8992 | ` * (no white space), FALSE otherwise.` |
|      - | 8993 | ` */` |
|     16 | 8994 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8995 | `{` |
|      - | 8996 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8997 | `	int nLen;` |
|     17 | 8998 | `	if( nArg < 1 ){` |
|      - | 8999 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9000 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9001 | `		return PH7_OK;` |
|      - | 9002 | `	}` |
|      - | 9003 | `	/* Extract the target string */` |
|     17 | 9004 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9005 | `	zEnd = &zIn[nLen];` |
|     17 | 9006 | `	if( nLen < 1 ){` |
|      - | 9007 | `		/* Empty string,return FALSE */` |
|      3 | 9008 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9009 | `		return PH7_OK;` |
|      - | 9010 | `	}` |
|      - | 9011 | `	/* Perform the requested operation */` |
|     57 | 9012 | `	for(;;){` |
|    115 | 9013 | `		if( zIn >= zEnd ){` |
|      - | 9014 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 9015 | `			ph7_result_bool(pCtx,1);` |
|      9 | 9016 | `			return PH7_OK;` |
|      - | 9017 | `		}` |
|    107 | 9018 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9019 | `			/* UTF-8 stream  */` |
|    ! 0 | 9020 | `			break;` |
|      - | 9021 | `		}` |
|    107 | 9022 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 9023 | `			break;` |
|      - | 9024 | `		}` |
|      - | 9025 | `		/* Point to the next character */` |
|    101 | 9026 | `		zIn++;` |
|      1 | 9027 | `	}` |
|      - | 9028 | `	/* The test failed,return FALSE */` |
|      7 | 9029 | `	ph7_result_bool(pCtx,0);` |
|      7 | 9030 | `	return PH7_OK;` |
|      9 | 9031 | `}` |
|      - | 9032 | `/*` |
|      - | 9033 | ` * bool ctype_print(string $text)` |
|      - | 9034 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 9035 | ` * Parameters` |
|      - | 9036 | ` *  $text` |
|      - | 9037 | ` *   The tested string.` |
|      - | 9038 | ` * Return` |
|      - | 9039 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 9040 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 9041 | ` *  or control function at all.` |
|      - | 9042 | ` */` |
|     16 | 9043 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9044 | `{` |
|      - | 9045 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9046 | `	int nLen;` |
|     17 | 9047 | `	if( nArg < 1 ){` |
|      - | 9048 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9049 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9050 | `		return PH7_OK;` |
|      - | 9051 | `	}` |
|      - | 9052 | `	/* Extract the target string */` |
|     17 | 9053 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9054 | `	zEnd = &zIn[nLen];` |
|     17 | 9055 | `	if( nLen < 1 ){` |
|      - | 9056 | `		/* Empty string,return FALSE */` |
|      3 | 9057 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9058 | `		return PH7_OK;` |
|      - | 9059 | `	}` |
|      - | 9060 | `	/* Perform the requested operation */` |
|     63 | 9061 | `	for(;;){` |
|    127 | 9062 | `		if( zIn >= zEnd ){` |
|      - | 9063 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 9064 | `			ph7_result_bool(pCtx,1);` |
|      9 | 9065 | `			return PH7_OK;` |
|      - | 9066 | `		}` |
|    119 | 9067 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9068 | `			/* UTF-8 stream  */` |
|    ! 0 | 9069 | `			break;` |
|      - | 9070 | `		}` |
|    119 | 9071 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 9072 | `			break;` |
|      - | 9073 | `		}` |
|      - | 9074 | `		/* Point to the next character */` |
|    113 | 9075 | `		zIn++;` |
|      1 | 9076 | `	}` |
|      - | 9077 | `	/* The test failed,return FALSE */` |
|      7 | 9078 | `	ph7_result_bool(pCtx,0);` |
|      7 | 9079 | `	return PH7_OK;` |
|      9 | 9080 | `}` |
|      - | 9081 | `/*` |
|      - | 9082 | ` * bool ctype_punct(string $text)` |
|      - | 9083 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 9084 | ` * Parameters` |
|      - | 9085 | ` *  $text` |
|      - | 9086 | ` *   The tested string.` |
|      - | 9087 | ` * Return` |
|      - | 9088 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 9089 | ` *  digit or blank, FALSE otherwise.` |
|      - | 9090 | ` */` |
|     18 | 9091 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9092 | `{` |
|      - | 9093 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9094 | `	int nLen;` |
|     19 | 9095 | `	if( nArg < 1 ){` |
|      - | 9096 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9097 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9098 | `		return PH7_OK;` |
|      - | 9099 | `	}` |
|      - | 9100 | `	/* Extract the target string */` |
|     19 | 9101 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 9102 | `	zEnd = &zIn[nLen];` |
|     19 | 9103 | `	if( nLen < 1 ){` |
|      - | 9104 | `		/* Empty string,return FALSE */` |
|      3 | 9105 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9106 | `		return PH7_OK;` |
|      - | 9107 | `	}` |
|      - | 9108 | `	/* Perform the requested operation */` |
|     38 | 9109 | `	for(;;){` |
|     77 | 9110 | `		if( zIn >= zEnd ){` |
|      - | 9111 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 9112 | `			ph7_result_bool(pCtx,1);` |
|      9 | 9113 | `			return PH7_OK;` |
|      - | 9114 | `		}` |
|     69 | 9115 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9116 | `			/* UTF-8 stream  */` |
|    ! 0 | 9117 | `			break;` |
|      - | 9118 | `		}` |
|     69 | 9119 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 9120 | `			break;` |
|      - | 9121 | `		}` |
|      - | 9122 | `		/* Point to the next character */` |
|     61 | 9123 | `		zIn++;` |
|      1 | 9124 | `	}` |
|      - | 9125 | `	/* The test failed,return FALSE */` |
|      9 | 9126 | `	ph7_result_bool(pCtx,0);` |
|      9 | 9127 | `	return PH7_OK;` |
|     10 | 9128 | `}` |
|      - | 9129 | `/*` |
|      - | 9130 | ` * bool ctype_space(string $text)` |
|      - | 9131 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 9132 | ` * Parameters` |
|      - | 9133 | ` *  $text` |
|      - | 9134 | ` *   The tested string.` |
|      - | 9135 | ` * Return` |
|      - | 9136 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 9137 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 9138 | ` *  and form feed characters.` |
|      - | 9139 | ` */` |
|  64463 | 9140 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 9141 | `{` |
|      - | 9142 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9143 | `	int nLen;` |
|  64468 | 9144 | `	if( nArg < 1 ){` |
|      - | 9145 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9146 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9147 | `		return PH7_OK;` |
|      - | 9148 | `	}` |
|      - | 9149 | `	/* Extract the target string */` |
|  64468 | 9150 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  64468 | 9151 | `	zEnd = &zIn[nLen];` |
|  64468 | 9152 | `	if( nLen < 1 ){` |
|      - | 9153 | `		/* Empty string,return FALSE */` |
|      3 | 9154 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9155 | `		return PH7_OK;` |
|      - | 9156 | `	}` |
|      - | 9157 | `	/* Perform the requested operation */` |
|  33145 | 9158 | `	for(;;){` |
|  66248 | 9159 | `		if( zIn >= zEnd ){` |
|      - | 9160 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1763 | 9161 | `			ph7_result_bool(pCtx,1);` |
|   1763 | 9162 | `			return PH7_OK;` |
|      - | 9163 | `		}` |
|  64490 | 9164 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9165 | `			/* UTF-8 stream  */` |
|    ! 0 | 9166 | `			break;` |
|      - | 9167 | `		}` |
|  64490 | 9168 | `		if( !SyisSpace(zIn[0]) ){` |
|  62708 | 9169 | `			break;` |
|      - | 9170 | `		}` |
|      - | 9171 | `		/* Point to the next character */` |
|   1787 | 9172 | `		zIn++;` |
|      5 | 9173 | `	}` |
|      - | 9174 | `	/* The test failed,return FALSE */` |
|  62708 | 9175 | `	ph7_result_bool(pCtx,0);` |
|  62708 | 9176 | `	return PH7_OK;` |
|  32260 | 9177 | `}` |
|      - | 9178 | `/*` |
|      - | 9179 | ` * bool ctype_lower(string $text)` |
|      - | 9180 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 9181 | ` * Parameters` |
|      - | 9182 | ` *  $text` |
|      - | 9183 | ` *   The tested string.` |
|      - | 9184 | ` * Return` |
|      - | 9185 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 9186 | ` */` |
|     16 | 9187 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9188 | `{` |
|      - | 9189 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9190 | `	int nLen;` |
|     17 | 9191 | `	if( nArg < 1 ){` |
|      - | 9192 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9193 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9194 | `		return PH7_OK;` |
|      - | 9195 | `	}` |
|      - | 9196 | `	/* Extract the target string */` |
|     17 | 9197 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9198 | `	zEnd = &zIn[nLen];` |
|     17 | 9199 | `	if( nLen < 1 ){` |
|      - | 9200 | `		/* Empty string,return FALSE */` |
|      3 | 9201 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9202 | `		return PH7_OK;` |
|      - | 9203 | `	}` |
|      - | 9204 | `	/* Perform the requested operation */` |
|     27 | 9205 | `	for(;;){` |
|     55 | 9206 | `		if( zIn >= zEnd ){` |
|      - | 9207 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 9208 | `			ph7_result_bool(pCtx,1);` |
|      5 | 9209 | `			return PH7_OK;` |
|      - | 9210 | `		}` |
|     51 | 9211 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 9212 | `			break;` |
|      - | 9213 | `		}` |
|      - | 9214 | `		/* Point to the next character */` |
|     41 | 9215 | `		zIn++;` |
|      1 | 9216 | `	}` |
|      - | 9217 | `	/* The test failed,return FALSE */` |
|     11 | 9218 | `	ph7_result_bool(pCtx,0);` |
|     11 | 9219 | `	return PH7_OK;` |
|      9 | 9220 | `}` |
|      - | 9221 | `/*` |
|      - | 9222 | ` * bool ctype_upper(string $text)` |
|      - | 9223 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 9224 | ` * Parameters` |
|      - | 9225 | ` *  $text` |
|      - | 9226 | ` *   The tested string.` |
|      - | 9227 | ` * Return` |
|      - | 9228 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 9229 | ` */` |
|     16 | 9230 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9231 | `{` |
|      - | 9232 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9233 | `	int nLen;` |
|     17 | 9234 | `	if( nArg < 1 ){` |
|      - | 9235 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9236 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9237 | `		return PH7_OK;` |
|      - | 9238 | `	}` |
|      - | 9239 | `	/* Extract the target string */` |
|     17 | 9240 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9241 | `	zEnd = &zIn[nLen];` |
|     17 | 9242 | `	if( nLen < 1 ){` |
|      - | 9243 | `		/* Empty string,return FALSE */` |
|      3 | 9244 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9245 | `		return PH7_OK;` |
|      - | 9246 | `	}` |
|      - | 9247 | `	/* Perform the requested operation */` |
|     28 | 9248 | `	for(;;){` |
|     57 | 9249 | `		if( zIn >= zEnd ){` |
|      - | 9250 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 9251 | `			ph7_result_bool(pCtx,1);` |
|      5 | 9252 | `			return PH7_OK;` |
|      - | 9253 | `		}` |
|     53 | 9254 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 9255 | `			break;` |
|      - | 9256 | `		}` |
|      - | 9257 | `		/* Point to the next character */` |
|     43 | 9258 | `		zIn++;` |
|      1 | 9259 | `	}` |
|      - | 9260 | `	/* The test failed,return FALSE */` |
|     11 | 9261 | `	ph7_result_bool(pCtx,0);` |
|     11 | 9262 | `	return PH7_OK;` |
|      9 | 9263 | `}` |
|      - | 9264 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 9265 | `/*` |
|      - | 9266 | ` * Section:` |
|      - | 9267 | ` *    URL handling Functions.` |
|      - | 9268 | ` * Status:` |
|      - | 9269 | ` *    Stable.` |
|      - | 9270 | ` */` |
|      - | 9271 | `/*` |
|      - | 9272 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 9273 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 9274 | ` */` |
|   1270 | 9275 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 9276 | `{` |
|      - | 9277 | `	/* Store in the call context result buffer */` |
|   1272 | 9278 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1272 | 9279 | `	return SXRET_OK;` |
|      2 | 9280 | `}` |
|      - | 9281 | `/*` |
|      - | 9282 | ` * string base64_encode(string $data)` |
|      - | 9283 | ` * string convert_uuencode(string $data)` |
|      - | 9284 | ` *  Encodes data with MIME base64` |
|      - | 9285 | ` * Parameter` |
|      - | 9286 | ` *  $data` |
|      - | 9287 | ` *    Data to encode` |
|      - | 9288 | ` * Return` |
|      - | 9289 | ` *  Encoded data or FALSE on failure.` |
|      - | 9290 | ` */` |
|      6 | 9291 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9292 | `{` |
|      - | 9293 | `	const char *zIn;` |
|      - | 9294 | `	int nLen;` |
|      7 | 9295 | `	if( nArg < 1 ){` |
|      - | 9296 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9297 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9298 | `		return PH7_OK;` |
|      - | 9299 | `	}` |
|      - | 9300 | `	/* Extract the input string */` |
|      7 | 9301 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 9302 | `	if( nLen < 1 ){` |
|      - | 9303 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9304 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9305 | `		return PH7_OK;` |
|      - | 9306 | `	}` |
|      - | 9307 | `	/* Perform the BASE64 encoding */` |
|      7 | 9308 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 9309 | `	return PH7_OK;` |
|      4 | 9310 | `}` |
|      - | 9311 | `/*` |
|      - | 9312 | ` * string base64_decode(string $data)` |
|      - | 9313 | ` * string convert_uudecode(string $data)` |
|      - | 9314 | ` *  Decodes data encoded with MIME base64` |
|      - | 9315 | ` * Parameter` |
|      - | 9316 | ` *  $data` |
|      - | 9317 | ` *    Encoded data.` |
|      - | 9318 | ` * Return` |
|      - | 9319 | ` *  Returns the original data or FALSE on failure.` |
|      - | 9320 | ` */` |
|     34 | 9321 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 9322 | `{` |
|      - | 9323 | `	const char *zIn;` |
|      - | 9324 | `	int nLen;` |
|     36 | 9325 | `	if( nArg < 1 ){` |
|      - | 9326 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9327 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9328 | `		return PH7_OK;` |
|      - | 9329 | `	}` |
|      - | 9330 | `	/* Extract the input string */` |
|     36 | 9331 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 9332 | `	if( nLen < 1 ){` |
|      - | 9333 | `		/* php decodes the empty string to the EMPTY STRING, not FALSE (FALSE is reserved` |
|      - | 9334 | `		 * for input that cannot be decoded at all). */` |
|      3 | 9335 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 9336 | `		return PH7_OK;` |
|      - | 9337 | `	}` |
|      - | 9338 | `	/* Perform the BASE64 decoding */` |
|     34 | 9339 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 9340 | `	return PH7_OK;` |
|     19 | 9341 | `}` |
|      - | 9342 | `/*` |
|      - | 9343 | ` * string urlencode(string $str)` |
|      - | 9344 | ` *  URL encoding` |
|      - | 9345 | ` * Parameter` |
|      - | 9346 | ` *  $data` |
|      - | 9347 | ` *   Input string.` |
|      - | 9348 | ` * Return` |
|      - | 9349 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 9350 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 9351 | ` *  encoded as plus (+) signs.` |
|      - | 9352 | ` */` |
|    100 | 9353 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9354 | `{` |
|      - | 9355 | `	const char *zIn;` |
|      - | 9356 | `	int nLen;` |
|    101 | 9357 | `	if( nArg < 1 ){` |
|      - | 9358 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9359 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9360 | `		return PH7_OK;` |
|      - | 9361 | `	}` |
|      - | 9362 | `	/* Extract the input string */` |
|    101 | 9363 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|    101 | 9364 | `	if( nLen < 1 ){` |
|      - | 9365 | `		/* php returns an empty string for empty input, not FALSE */` |
|      3 | 9366 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 9367 | `		return PH7_OK;` |
|      - | 9368 | `	}` |
|      - | 9369 | `	/* Perform the URL encoding */` |
|     99 | 9370 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     99 | 9371 | `	return PH7_OK;` |
|     51 | 9372 | `}` |
|      - | 9373 | `/*` |
|      - | 9374 | ` * string rawurlencode(string $str)` |
|      - | 9375 | ` *  RFC 3986 URL encoding: spaces become %20 (not '+') and '~' is left intact.` |
|      - | 9376 | ` */` |
|     14 | 9377 | `static int PH7_builtin_rawurlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9378 | `{` |
|      - | 9379 | `	const char *zIn;` |
|      - | 9380 | `	int nLen;` |
|     15 | 9381 | `	if( nArg < 1 ){` |
|      - | 9382 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9383 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9384 | `		return PH7_OK;` |
|      - | 9385 | `	}` |
|      - | 9386 | `	/* Extract the input string */` |
|     15 | 9387 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 9388 | `	if( nLen < 1 ){` |
|      - | 9389 | `		/* php returns an empty string for empty input, not FALSE */` |
|      3 | 9390 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 9391 | `		return PH7_OK;` |
|      - | 9392 | `	}` |
|      - | 9393 | `	/* Perform the RFC 3986 URL encoding */` |
|     13 | 9394 | `	SyUriEncodeRaw(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     13 | 9395 | `	return PH7_OK;` |
|      8 | 9396 | `}` |
|      - | 9397 | `/*` |
|      - | 9398 | ` * string urldecode(string $str)` |
|      - | 9399 | ` *  Decodes any %## encoding in the given string.` |
|      - | 9400 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 9401 | ` * Parameter` |
|      - | 9402 | ` *  $data` |
|      - | 9403 | ` *    Input string.` |
|      - | 9404 | ` * Return` |
|      - | 9405 | ` *  Decoded URL or FALSE on failure.` |
|      - | 9406 | ` */` |
|    110 | 9407 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9408 | `{` |
|      - | 9409 | `	const char *zIn;` |
|      - | 9410 | `	int nLen;` |
|    111 | 9411 | `	if( nArg < 1 ){` |
|      - | 9412 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9413 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9414 | `		return PH7_OK;` |
|      - | 9415 | `	}` |
|      - | 9416 | `	/* Extract the input string */` |
|    111 | 9417 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|    111 | 9418 | `	if( nLen < 1 ){` |
|      - | 9419 | `		/* php returns an empty string for empty input, not FALSE */` |
|     17 | 9420 | `		ph7_result_string(pCtx,"",0);` |
|     17 | 9421 | `		return PH7_OK;` |
|      - | 9422 | `	}` |
|      - | 9423 | `	/* Perform the URL decoding */` |
|     95 | 9424 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|     95 | 9425 | `	return PH7_OK;` |
|     56 | 9426 | `}` |
|      - | 9427 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9428 | `/* Table of the built-in functions */` |
|      - | 9429 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 9430 | `	   /* Variable handling functions */` |
|      - | 9431 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 9432 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 9433 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 9434 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 9435 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 9436 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 9437 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 9438 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 9439 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 9440 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 9441 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 9442 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 9443 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 9444 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 9445 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 9446 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 9447 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 9448 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 9449 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 9450 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 9451 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9452 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 9453 | `	   /* Math functions */` |
|      - | 9454 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 9455 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 9456 | `	{ "acosh" ,   PH7_builtin_acosh        },` |
|      - | 9457 | `	{ "asinh" ,   PH7_builtin_asinh        },` |
|      - | 9458 | `	{ "atanh" ,   PH7_builtin_atanh        },` |
|      - | 9459 | `	{ "expm1" ,   PH7_builtin_expm1        },` |
|      - | 9460 | `	{ "log1p" ,   PH7_builtin_log1p        },` |
|      - | 9461 | `	{ "deg2rad" , PH7_builtin_deg2rad      },` |
|      - | 9462 | `	{ "rad2deg" , PH7_builtin_rad2deg      },` |
|      - | 9463 | `	{ "fpow" ,    PH7_builtin_fpow         },` |
|      - | 9464 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 9465 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 9466 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 9467 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 9468 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 9469 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 9470 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 9471 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 9472 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 9473 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 9474 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 9475 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 9476 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 9477 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 9478 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 9479 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 9480 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 9481 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 9482 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 9483 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 9484 | `	{ "round",    PH7_builtin_round        },` |
|      - | 9485 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 9486 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 9487 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 9488 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 9489 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 9490 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 9491 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 9492 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 9493 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 9494 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9495 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9496 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 9497 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9498 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9499 | `	   /* String handling functions */` |
|      - | 9500 |  |
|      - | 9501 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 9502 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 9503 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 9504 | `	{ "substr_replace",  PH7_builtin_substr_replace },` |
|      - | 9505 | `	{ "levenshtein",     PH7_builtin_levenshtein },` |
|      - | 9506 | `	{ "similar_text",    PH7_builtin_similar_text },` |
|      - | 9507 | `	{ "str_word_count",  PH7_builtin_str_word_count },` |
|      - | 9508 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 9509 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 9510 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 9511 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 9512 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 9513 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 9514 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 9515 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 9516 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 9517 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 9518 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 9519 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 9520 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 9521 | `	{ "strnatcmp"  , PH7_builtin_strnatcmp  },` |
|      - | 9522 | `	{ "strnatcasecmp", PH7_builtin_strnatcmp },` |
|      - | 9523 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 9524 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 9525 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 9526 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 9527 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 9528 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 9529 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 9530 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 9531 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 9532 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 9533 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 9534 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 9535 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 9536 | `	{ "mb_strtolower",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */` |
|      - | 9537 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 9538 | `	{ "mb_strtoupper",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */` |
|      - | 9539 | `	{ "mb_strlen",    PH7_builtin_mb_strlen_f },` |
|      - | 9540 | `	{ "mb_substr",    PH7_builtin_mb_substr_f },` |
|      - | 9541 | `	{ "mb_convert_case", PH7_builtin_mb_convert_case_f },` |
|      - | 9542 | `	{ "mb_strpos",    PH7_builtin_mb_strpos_f },` |
|      - | 9543 | `	{ "mb_stripos",   PH7_builtin_mb_strpos_f },` |
|      - | 9544 | `	{ "mb_strrpos",   PH7_builtin_mb_strpos_f },` |
|      - | 9545 | `	{ "mb_str_split", PH7_builtin_mb_str_split_f },` |
|      - | 9546 | `	{ "mb_internal_encoding", PH7_builtin_mb_internal_encoding_f },` |
|      - | 9547 | `	{ "mb_check_encoding",    PH7_builtin_mb_check_encoding_f },` |
|      - | 9548 | `	{ "mb_strwidth",  PH7_builtin_mb_strwidth_f },` |
|      - | 9549 | `	{ "mb_chr",       PH7_builtin_mb_chr_f   },` |
|      - | 9550 | `	{ "mb_ord",       PH7_builtin_mb_ord_f   },` |
|      - | 9551 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 9552 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 9553 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 9554 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 9555 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 9556 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 9557 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 9558 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 9559 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 9560 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 9561 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 9562 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 9563 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 9564 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 9565 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 9566 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 9567 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - | 9568 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - | 9569 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - | 9570 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 9571 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9572 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9573 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 9574 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 9575 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 9576 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 9577 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9578 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9579 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 9580 |  |
|      - | 9581 |  |
|      - | 9582 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 9583 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 9584 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 9585 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 9586 | `	{ "hash",         PH7_builtin_hash      },` |
|      - | 9587 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|      - | 9588 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|      - | 9589 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|      - | 9590 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 9591 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|      - | 9592 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|      - | 9593 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|      - | 9594 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|      - | 9595 | `	{ "filter_var",            PH7_builtin_filter_var },` |
|      - | 9596 | `	{ "filter_input",          PH7_builtin_filter_input },` |
|      - | 9597 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9598 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9599 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 9600 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 9601 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9602 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9603 |  |
|      - | 9604 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 9605 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 9606 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 9607 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 9608 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 9609 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 9610 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 9611 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 9612 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 9613 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 9614 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 9615 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 9616 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9617 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9618 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 9619 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9620 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9621 |  |
|      - | 9622 | `	         /* Ctype functions */` |
|      - | 9623 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 9624 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 9625 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 9626 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 9627 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 9628 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 9629 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 9630 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 9631 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 9632 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 9633 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 9634 | `	         /* Time functions */` |
|      - | 9635 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 9636 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 9637 | `	{ "hrtime",      PH7_builtin_hrtime       },` |
|      - | 9638 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 9639 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 9640 | `	{ "date",        PH7_builtin_date         },` |
|      - | 9641 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 9642 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 9643 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 9644 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 9645 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 9646 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 9647 | `	{ "date_default_timezone_get", PH7_builtin_date_default_timezone_get },` |
|      - | 9648 | `	{ "date_default_timezone_set", PH7_builtin_date_default_timezone_set },` |
|      - | 9649 | `	        /* URL functions */` |
|      - | 9650 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 9651 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 9652 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 9653 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 9654 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 9655 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 9656 | `	{ "rawurlencode", PH7_builtin_rawurlencode },` |
|      - | 9657 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 9658 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9659 | `};` |
|      - | 9660 | `/*` |
|      - | 9661 | ` * Register the built-in functions defined above,the array functions` |
|      - | 9662 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 9663 | ` */` |
|   3356 | 9664 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      5 | 9665 | `{` |
|      - | 9666 | `	sxu32 n;` |
| 661137 | 9667 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 657781 | 9668 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 328893 | 9669 | `	}` |
|      - | 9670 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   3361 | 9671 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 9672 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   3361 | 9673 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   3361 | 9674 | `}` |
|      - | 9675 |  |
