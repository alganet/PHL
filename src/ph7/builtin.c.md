# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4445/5126 lines (86.71%)

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
| 482236 |   18 | `PH7_PRIVATE sxi32 PH7_IntArgResolve(` |
|      - |   19 | `	ph7_context *pCtx,` |
|      - |   20 | `	ph7_value *pArg,` |
|      - |   21 | `	const char *zFunc,` |
|      - |   22 | `	int iArgNum,` |
|      - |   23 | `	const char *zParamName,` |
|      - |   24 | `	const char *zTypeStr,` |
|      - |   25 | `	sxi64 *pOut` |
|      5 |   26 | `){` |
| 482241 |   27 | `	if( ph7_value_is_null(pArg) ){` |
|    ! 0 |   28 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |   29 | `			"%s(): Passing null to parameter #%d (%s) of type %s is deprecated",` |
|    ! 0 |   30 | `			zFunc,iArgNum,zParamName,zTypeStr` |
|      - |   31 | `			);` |
|    ! 0 |   32 | `		*pOut = 0;` |
|    ! 0 |   33 | `		return PH7_OK;` |
|      - |   34 | `	}` |
| 482241 |   35 | `	if( ph7_value_is_float(pArg) ){` |
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
| 482235 |   56 | `	if( ph7_value_is_string(pArg) ){` |
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
| 482221 |   98 | `	if( !ph7_value_is_int(pArg) && !ph7_value_is_bool(pArg) ){` |
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
| 482221 |  113 | `	*pOut = ph7_value_to_int64(pArg);` |
| 482221 |  114 | `	return PH7_OK;` |
| 241123 |  115 | `}` |
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
|  33572 |  411 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  412 | `{` |
|  33577 |  413 | `	int res = 1; /* Assume empty by default */` |
|  33577 |  414 | `	if( nArg > 0 ){` |
|  33575 |  415 | `		res = ph7_value_is_empty(apArg[0]);` |
|  16785 |  416 | `	}` |
|  33577 |  417 | `	ph7_result_bool(pCtx,res);` |
|  33577 |  418 | `	return PH7_OK;` |
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
| 265978 |  461 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  462 | `{` |
|      - |  463 | `	const char *zSource;` |
|      - |  464 | `	int nSrcLen;` |
|      - |  465 | `	sxi64 iStart,iEnd;` |
| 265983 |  466 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"substr",1,"$string"); }` |
| 265983 |  467 | `	if( nArg < 2 ){` |
|      - |  468 | `		/* Arity is enforced at the call boundary; nothing sensible to return here. */` |
|    ! 0 |  469 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 |  470 | `		return PH7_OK;` |
|      - |  471 | `	}` |
|      - |  472 | `	/* Extract the target string */` |
| 265983 |  473 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|      - |  474 | `	/* Extract the offset */` |
|      - |  475 | `	{` |
| 265983 |  476 | `		sxi64 iTmp = 0;` |
| 265983 |  477 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"substr",2,"$offset","int",&iTmp);` |
| 265983 |  478 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  479 | `			return rcArg;` |
|      - |  480 | `		}` |
| 265983 |  481 | `		iStart = iTmp;` |
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
| 265983 |  493 | `	if( iStart < 0 ){` |
|  32977 |  494 | `		iStart += nSrcLen;` |
|  32977 |  495 | `		if( iStart < 0 ){` |
|      5 |  496 | `			iStart = 0;` |
|      7 |  497 | `		}` |
| 249497 |  498 | `	}else if( iStart > nSrcLen ){` |
|      7 |  499 | `		iStart = nSrcLen;` |
|      3 |  500 | `	}` |
| 265983 |  501 | `	iEnd = nSrcLen;` |
| 265983 |  502 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
| 195299 |  503 | `		sxi64 iLen = 0;` |
| 195299 |  504 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[2],"substr",3,"$length","?int",&iLen);` |
| 195299 |  505 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  506 | `			return rcArg;` |
|      - |  507 | `		}` |
| 195299 |  508 | `		if( iLen < 0 ){` |
|  32909 |  509 | `			iEnd = (sxi64)nSrcLen + iLen;` |
| 178847 |  510 | `		}else if( iLen > (sxi64)nSrcLen - iStart ){` |
|  18769 |  511 | `			iEnd = nSrcLen;` |
|   9387 |  512 | `		}else{` |
| 143631 |  513 | `			iEnd = iStart + iLen;` |
|      - |  514 | `		}` |
|  97647 |  515 | `	}` |
| 265983 |  516 | `	if( iEnd < iStart ){` |
|      3 |  517 | `		iEnd = iStart;` |
|      1 |  518 | `	}` |
| 265983 |  519 | `	ph7_result_string(pCtx,&zSource[iStart],(int)(iEnd - iStart));` |
| 265983 |  520 | `	return PH7_OK;` |
| 132994 |  521 | `}` |
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
| 392434 |  716 | `static void StrNullArgNotice(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNum,const char *zParamName)` |
|      5 |  717 | `{` |
| 392439 |  718 | `	if( ph7_value_is_null(pArg) ){` |
|     22 |  719 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |  720 | `			"%s(): Passing null to parameter #%d (%s) of type string is deprecated",` |
|      7 |  721 | `			zFunc,iArgNum,zParamName);` |
|      7 |  722 | `	}` |
| 392439 |  723 | `}` |
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
| 150716 | 2257 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 2258 | `{` |
|  75358 | 2259 | `	SXUNUSED(pKey);` |
| 150721 | 2260 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 2261 | `	const char *zData;` |
|      - | 2262 | `	int nLen;` |
| 150721 | 2263 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
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
| 150719 | 2287 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 2288 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 150719 | 2289 | `	if( pData->bFirst ){` |
|  33445 | 2290 | `		pData->bFirst = 0;` |
| 133999 | 2291 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 2292 | `		/* append the separator first */` |
| 117199 | 2293 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 2294 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 2295 | `			return PH7_ABORT;` |
|      - | 2296 | `		}` |
|  58597 | 2297 | `	}` |
|      - | 2298 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 150719 | 2299 | `	if( nLen > 0 ){` |
| 138061 | 2300 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2301 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 2302 | `			return PH7_ABORT;` |
|      - | 2303 | `		}` |
|  69028 | 2304 | `	}` |
| 150719 | 2305 | `	return PH7_OK;` |
|  75363 | 2306 | `}` |
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
|  33468 | 2320 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2321 | `{` |
|      - | 2322 | `	struct implode_data imp_data;` |
|  33473 | 2323 | `	int i = 1;` |
|  33473 | 2324 | `	if( nArg < 1 ){` |
|      - | 2325 | `		/* Missing argument,return NULL */` |
|    ! 0 | 2326 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2327 | `		return PH7_OK;` |
|      - | 2328 | `	}` |
|      - | 2329 | `	/* Prepare the implode context */` |
|  33473 | 2330 | `	imp_data.pCtx = pCtx;` |
|  33473 | 2331 | `	imp_data.bRecursive = 0;` |
|  33473 | 2332 | `	imp_data.bFirst = 1;` |
|  33473 | 2333 | `	imp_data.nRecCount = 0;` |
|  33473 | 2334 | `	imp_data.rc = SXRET_OK;` |
|  33473 | 2335 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  33471 | 2336 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  33471 | 2337 | `		if( nArg > 1 && !ph7_value_is_array(apArg[1]) && !ph7_value_is_null(apArg[1]) ){` |
|      - | 2338 | `			/* php: implode($glue, $pieces) requires an ARRAY. PH7 stringified whatever it` |
|      - | 2339 | `			 * was handed, so implode(",", 5) quietly returned "5". */` |
|      - | 2340 | `			char zBuf[64];` |
|      4 | 2341 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 2342 | `				"implode(): Argument #2 ($array) must be of type ?array, %s given",` |
|      2 | 2343 | `				VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 2344 | `		}` |
|  16737 | 2345 | `	}else{` |
|      3 | 2346 | `		imp_data.zSep = 0;` |
|      3 | 2347 | `		imp_data.nSeplen = 0;` |
|      3 | 2348 | `		i = 0;` |
|      - | 2349 | `	}` |
|  33471 | 2350 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2351 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2352 | `	}` |
|      - | 2353 | `	/* Start the 'join' process */` |
|  66937 | 2354 | `	while( i < nArg ){` |
|  33471 | 2355 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2356 | `			/* Iterate throw array entries */` |
|  33471 | 2357 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2358 | `			/* Surface a callback allocation failure as a fatal */` |
|  33471 | 2359 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2360 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2361 | `			}` |
|  16738 | 2362 | `		}else{` |
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
|  33471 | 2382 | `		i++;` |
|      5 | 2383 | `	}` |
|  33471 | 2384 | `	return PH7_OK;` |
|  16739 | 2385 | `}` |
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
|   6646 | 2485 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2486 | `{` |
|      - | 2487 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 2488 | `	int nDelim,nStrlen,iLimit;` |
|      - | 2489 | `	ph7_value *pArray;` |
|      - | 2490 | `	ph7_value *pValue;` |
|      - | 2491 | `	sxu32 nOfft;` |
|      - | 2492 | `	sxi32 rc;` |
|   6651 | 2493 | `	if( nArg < 2 ){` |
|      - | 2494 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2495 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2496 | `		return PH7_OK;` |
|      - | 2497 | `	}` |
|      - | 2498 | `	/* Extract the delimiter */` |
|   6651 | 2499 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   6651 | 2500 | `	if( nDelim < 1 ){` |
|      - | 2501 | `		/* Empty delimiter: PHP 8 throws a catchable ValueError. */` |
|      5 | 2502 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2503 | `			"explode(): Argument #1 ($separator) must not be empty");` |
|      - | 2504 | `	}` |
|      - | 2505 | `	/* Extract the string */` |
|   6647 | 2506 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   6647 | 2507 | `	if( nStrlen < 1 ){` |
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
|   6641 | 2533 | `	zEnd = &zString[nStrlen];` |
|      - | 2534 | `	/* Create the array */` |
|   6641 | 2535 | `	pArray =  ph7_context_new_array(pCtx);` |
|   6641 | 2536 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   6641 | 2537 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2538 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 2539 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2540 | `		return PH7_OK;` |
|      - | 2541 | `	}` |
|      - | 2542 | `	/* Set a defualt limit */` |
|   6641 | 2543 | `	iLimit = SXI32_HIGH;` |
|   6641 | 2544 | `	if( nArg > 2 ){` |
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
|  81104 | 2579 | `	for(;;){` |
| 162213 | 2580 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 162213 | 2581 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 2582 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   6625 | 2583 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   6625 | 2584 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2585 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2586 | `			}` |
|   6625 | 2587 | `			break;` |
|      - | 2588 | `		}` |
|      - | 2589 | `		/* Point to the desired offset */` |
| 155593 | 2590 | `		zCur = &zString[nOfft];` |
|      - | 2591 | `		/* Perform the store operation (may be empty) */` |
| 155593 | 2592 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 155593 | 2593 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2594 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2595 | `		}` |
|      - | 2596 | `		/* Point beyond the delimiter */` |
| 155593 | 2597 | `		zString = &zCur[nDelim];` |
|      - | 2598 | `		/* Reset the cursor */` |
| 155593 | 2599 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 2600 | `	}` |
|      - | 2601 | `	/* Return the freshly created array */` |
|   6625 | 2602 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2603 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 2604 | `	 * released as soon we return from this foregin function.` |
|      - | 2605 | `	 */` |
|   6625 | 2606 | `	return PH7_OK;` |
|   3328 | 2607 | `}` |
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
|  14412 | 2623 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2624 | `{` |
|  14417 | 2625 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"trim",1,"$string"); }` |
|      - | 2626 | `	const char *zString;` |
|      - | 2627 | `	int nLen;` |
|  14417 | 2628 | `	if( nArg < 1 ){` |
|      - | 2629 | `		/* Missing arguments,return null */` |
|    ! 0 | 2630 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2631 | `		return PH7_OK;` |
|      - | 2632 | `	}` |
|      - | 2633 | `	/* Extract the target string */` |
|  14417 | 2634 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  14417 | 2635 | `	if( nLen < 1 ){` |
|      - | 2636 | `		/* Empty string,return */` |
|    755 | 2637 | `		ph7_result_string(pCtx,"",0);` |
|    755 | 2638 | `		return PH7_OK;` |
|      - | 2639 | `	}` |
|      - | 2640 | `	/* Start the trim process */` |
|  13667 | 2641 | `	if( nArg < 2 ){` |
|      - | 2642 | `		SyString sStr;` |
|      - | 2643 | `		/* Remove white spaces and NUL bytes */` |
|  13637 | 2644 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  34201 | 2645 | `		SyStringFullTrimSafe(&sStr);` |
|  13637 | 2646 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   6821 | 2647 | `	}else{` |
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
|  13667 | 2676 | `	return PH7_OK;` |
|   7211 | 2677 | `}` |
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
|  33374 | 2819 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2820 | `{` |
|  33379 | 2821 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtolower",1,"$string"); }` |
|      - | 2822 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2823 | `	int nLen;` |
|  33379 | 2824 | `	if( nArg < 1 ){` |
|      - | 2825 | `		/* Missing arguments,return null */` |
|    ! 0 | 2826 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2827 | `		return PH7_OK;` |
|      - | 2828 | `	}` |
|      - | 2829 | `	/* Extract the target string */` |
|  33379 | 2830 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  33379 | 2831 | `	if( nLen < 1 ){` |
|      - | 2832 | `		/* Empty string,return */` |
|      5 | 2833 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2834 | `		return PH7_OK;` |
|      - | 2835 | `	}` |
|      - | 2836 | `	/* Perform the requested operation */` |
|  33375 | 2837 | `	zEnd = &zString[nLen];` |
| 105240 | 2838 | `	for(;;){` |
| 210485 | 2839 | `		if( zString >= zEnd ){` |
|      - | 2840 | `			/* No more input,break immediately */` |
|  33375 | 2841 | `			break;` |
|      - | 2842 | `		}` |
| 177115 | 2843 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2844 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2845 | `			zCur = zString;` |
|    ! 0 | 2846 | `			zString++;` |
|    ! 0 | 2847 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2848 | `				zString++;` |
|    ! 0 | 2849 | `			}` |
|      - | 2850 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2851 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2852 | `		}else{` |
| 177115 | 2853 | `			int c = zString[0];` |
| 177115 | 2854 | `			if( SyisUpper(c) ){` |
| 174559 | 2855 | `				c = SyToLower(zString[0]);` |
|  87277 | 2856 | `			}` |
|      - | 2857 | `			/* Append character */` |
| 177115 | 2858 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2859 | `			/* Advance the cursor */` |
| 177115 | 2860 | `			zString++;` |
|      - | 2861 | `		}` |
|      5 | 2862 | `	}` |
|  33375 | 2863 | `	return PH7_OK;` |
|  16692 | 2864 | `}` |
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
|    480 | 4236 | `static int FormatUnknownSpec(const char *zIn,int nByte,int *pBad)` |
|      3 | 4237 | `{` |
|    483 | 4238 | `	const char *zEnd = &zIn[nByte];` |
|      - | 4239 | `	int c,idx;` |
|   3793 | 4240 | `	while( zIn < zEnd ){` |
|   3333 | 4241 | `		if( zIn[0] != '%' ){` |
|   2411 | 4242 | `			zIn++;` |
|   2411 | 4243 | `			continue;` |
|      - | 4244 | `		}` |
|    923 | 4245 | `		zIn++; /* jump the percent sign */` |
|      - | 4246 | `		/* php-supported flags: '-', '+', ' ', '0' and the "'<pad>'" custom-pad` |
|      - | 4247 | `		 * form. '#' is intentionally NOT treated as a flag so it surfaces as an` |
|      - | 4248 | `		 * unknown specifier, matching php. */` |
|   1127 | 4249 | `		while( zIn < zEnd ){` |
|   1125 | 4250 | `			c = zIn[0];` |
|   1125 | 4251 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|    193 | 4252 | `				zIn++;` |
|    193 | 4253 | `				continue;` |
|      - | 4254 | `			}` |
|    933 | 4255 | `			if( c=='\'' ){` |
|     13 | 4256 | `				zIn++;` |
|     13 | 4257 | `				if( zIn < zEnd ){` |
|     13 | 4258 | `					zIn++; /* the custom pad character */` |
|      6 | 4259 | `				}` |
|     13 | 4260 | `				continue;` |
|      - | 4261 | `			}` |
|    921 | 4262 | `			break;` |
|    ! 0 | 4263 | `		}` |
|      - | 4264 | `		/* field width */` |
|   1201 | 4265 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    279 | 4266 | `			zIn++;` |
|      1 | 4267 | `		}` |
|      - | 4268 | `		/* positional specifier ($) — php parses flags AFTER it (e.g. "%1$-10s"),` |
|      - | 4269 | `		 * so skip the full flag set and width again, mirroring the main loop. */` |
|    923 | 4270 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
|     15 | 4271 | `			zIn++;` |
|     17 | 4272 | `			while( zIn < zEnd ){` |
|     17 | 4273 | `				c = zIn[0];` |
|     17 | 4274 | `				if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|    ! 0 | 4275 | `					zIn++;` |
|    ! 0 | 4276 | `					continue;` |
|      - | 4277 | `				}` |
|     17 | 4278 | `				if( c=='\'' ){` |
|      3 | 4279 | `					zIn++;` |
|      3 | 4280 | `					if( zIn < zEnd ){` |
|      3 | 4281 | `						zIn++;` |
|      1 | 4282 | `					}` |
|      3 | 4283 | `					continue;` |
|      - | 4284 | `				}` |
|     15 | 4285 | `				break;` |
|    ! 0 | 4286 | `			}` |
|     23 | 4287 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|      9 | 4288 | `				zIn++;` |
|      1 | 4289 | `			}` |
|      7 | 4290 | `		}` |
|      - | 4291 | `		/* precision */` |
|    923 | 4292 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|    113 | 4293 | `			zIn++;` |
|    243 | 4294 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    133 | 4295 | `				zIn++;` |
|      3 | 4296 | `			}` |
|     55 | 4297 | `		}` |
|      - | 4298 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|    923 | 4299 | `		if( zIn < zEnd && zIn[0]=='l' ){` |
|     11 | 4300 | `			zIn++;` |
|      5 | 4301 | `		}` |
|    923 | 4302 | `		if( zIn >= zEnd ){` |
|      - | 4303 | `			/* A dangling '%' with no specifier: PHL's legacy path silently` |
|      - | 4304 | `			 * truncates here (recorded residual); nothing to validate. */` |
|      3 | 4305 | `			break;` |
|      - | 4306 | `		}` |
|    921 | 4307 | `		c = zIn[0];` |
|    921 | 4308 | `		zIn++; /* jump the conversion specifier */` |
|   3765 | 4309 | `		for( idx = 0 ; idx < (int)SX_ARRAYSIZE(aFmt) ; idx++ ){` |
|   3747 | 4310 | `			if( c == aFmt[idx].fmttype ){` |
|    903 | 4311 | `				break;` |
|      - | 4312 | `			}` |
|   1425 | 4313 | `		}` |
|    921 | 4314 | `		if( idx >= (int)SX_ARRAYSIZE(aFmt) ){` |
|     19 | 4315 | `			*pBad = c; /* unknown specifier */` |
|     19 | 4316 | `			return TRUE;` |
|      - | 4317 | `		}` |
|      3 | 4318 | `	}` |
|    465 | 4319 | `	return FALSE;` |
|    243 | 4320 | `}` |
|      - | 4321 | `/*` |
|      - | 4322 | ` * Validate a printf-style format string. PHP 8 raises a catchable ValueError for` |
|      - | 4323 | ` * an unknown conversion specifier, thrown before any output is produced. Every` |
|      - | 4324 | ` * format builtin (sprintf/printf/vprintf/vsprintf/fprintf/vfprintf) calls this` |
|      - | 4325 | ` * up-front, then propagates the returned status verbatim (PH7_EXCEPTION when the` |
|      - | 4326 | ` * throw is caught in place, PH7_ABORT when it goes uncaught).` |
|      - | 4327 | ` * Returns PH7_OK when the format is valid.` |
|      - | 4328 | ` */` |
|    480 | 4329 | `PH7_PRIVATE sxi32 PH7_FormatValidate(ph7_context *pCtx,const char *zFormat,int nByte)` |
|      3 | 4330 | `{` |
|    483 | 4331 | `	int badSpec = 0;` |
|    483 | 4332 | `	if( FormatUnknownSpec(zFormat,nByte,&badSpec) ){` |
|     28 | 4333 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      9 | 4334 | `			"Unknown format specifier \"%c\"",badSpec);` |
|      - | 4335 | `	}` |
|    465 | 4336 | `	return PH7_OK;` |
|    243 | 4337 | `}` |
|      - | 4338 | `/*` |
|      - | 4339 | ` * Count the number of VALUE arguments a format string needs: the greater of the` |
|      - | 4340 | ` * sequential (non-positional) conversion count and the highest positional index` |
|      - | 4341 | `` * (`%N$`). `%%` consumes nothing. Mirrors FormatUnknownSpec's specifier walk.`` |
|      - | 4342 | ` */` |
|    462 | 4343 | `static int FormatRequiredArgs(const char *zIn,int nByte)` |
|      3 | 4344 | `{` |
|    465 | 4345 | `	const char *zEnd = &zIn[nByte];` |
|    465 | 4346 | `	int c,seq = 0,maxpos = 0;` |
|   3761 | 4347 | `	while( zIn < zEnd ){` |
|   3301 | 4348 | `		int numVal = 0,pos = 0;` |
|   3301 | 4349 | `		if( zIn[0] != '%' ){` |
|   2397 | 4350 | `			zIn++;` |
|   2397 | 4351 | `			continue;` |
|      - | 4352 | `		}` |
|    905 | 4353 | `		zIn++; /* jump the percent sign */` |
|      - | 4354 | `		/* leading flags (incl. the "'<pad>'" custom-pad form) */` |
|   1109 | 4355 | `		while( zIn < zEnd ){` |
|   1107 | 4356 | `			c = zIn[0];` |
|   1107 | 4357 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){ zIn++; continue; }` |
|    915 | 4358 | `			if( c=='\'' ){ zIn++; if( zIn < zEnd ){ zIn++; } continue; }` |
|    903 | 4359 | `			break;` |
|    ! 0 | 4360 | `		}` |
|      - | 4361 | `		/* leading number: a positional index when a '$' follows, else the width */` |
|   1183 | 4362 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    279 | 4363 | `			numVal = numVal*10 + (zIn[0]-'0');` |
|    279 | 4364 | `			zIn++;` |
|      1 | 4365 | `		}` |
|    905 | 4366 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
|     15 | 4367 | `			pos = numVal;` |
|     15 | 4368 | `			zIn++;` |
|      - | 4369 | `			/* flags then width may follow the positional marker */` |
|     17 | 4370 | `			while( zIn < zEnd ){` |
|     17 | 4371 | `				c = zIn[0];` |
|     17 | 4372 | `				if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){ zIn++; continue; }` |
|     17 | 4373 | `				if( c=='\'' ){ zIn++; if( zIn < zEnd ){ zIn++; } continue; }` |
|     15 | 4374 | `				break;` |
|    ! 0 | 4375 | `			}` |
|     23 | 4376 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){ zIn++; }` |
|      7 | 4377 | `		}` |
|      - | 4378 | `		/* precision */` |
|    905 | 4379 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|    113 | 4380 | `			zIn++;` |
|    243 | 4381 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){ zIn++; }` |
|     55 | 4382 | `		}` |
|      - | 4383 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|    905 | 4384 | `		if( zIn < zEnd && zIn[0]=='l' ){ zIn++; }` |
|    905 | 4385 | `		if( zIn >= zEnd ){ break; }` |
|    903 | 4386 | `		c = zIn[0];` |
|    903 | 4387 | `		zIn++; /* jump the conversion specifier */` |
|    903 | 4388 | `		if( c == '%' ){ continue; } /* %% consumes no argument */` |
|    895 | 4389 | `		if( pos > 0 ){` |
|     15 | 4390 | `			if( pos > maxpos ){ maxpos = pos; }` |
|      8 | 4391 | `		}else{` |
|    881 | 4392 | `			seq++;` |
|      - | 4393 | `		}` |
|      3 | 4394 | `	}` |
|    465 | 4395 | `	return seq > maxpos ? seq : maxpos;` |
|      3 | 4396 | `}` |
|      - | 4397 | `/*` |
|      - | 4398 | ` * PHP 8: a printf-family call with fewer VALUE arguments than the format needs` |
|      - | 4399 | ` * throws BEFORE any output. The non-vararg family (sprintf/printf/fprintf) raises` |
|      - | 4400 | ` * ArgumentCountError counting the format itself ("N arguments are required, M` |
|      - | 4401 | ` * given"); the vararg family (vsprintf/vprintf/vfprintf) raises a ValueError over` |
|      - | 4402 | ` * the values array ("The arguments array must contain N items, M given"). nValues` |
|      - | 4403 | ` * is the count of value arguments actually supplied; nFixed is the number of` |
|      - | 4404 | ` * fixed leading parameters counted in the ArgumentCountError totals (1 for the` |
|      - | 4405 | ` * $format of sprintf/printf, 2 for fprintf's $stream + $format — the vararg` |
|      - | 4406 | ` * ValueError counts only the array, so nFixed is ignored there). Returns PH7_OK` |
|      - | 4407 | ` * when enough.` |
|      - | 4408 | ` */` |
|    462 | 4409 | `PH7_PRIVATE sxi32 PH7_FormatCheckArgCount(ph7_context *pCtx,const char *zFormat,int nByte,int nValues,int nFixed,int bVararg)` |
|      3 | 4410 | `{` |
|    465 | 4411 | `	int required = FormatRequiredArgs(zFormat,nByte);` |
|    465 | 4412 | `	if( nValues < required ){` |
|     21 | 4413 | `		if( bVararg ){` |
|     10 | 4414 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      3 | 4415 | `				"The arguments array must contain %d items, %d given",required,nValues);` |
|      - | 4416 | `		}` |
|     22 | 4417 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|      7 | 4418 | `			"%d arguments are required, %d given",required+nFixed,nValues+nFixed);` |
|      - | 4419 | `	}` |
|    445 | 4420 | `	return PH7_OK;` |
|    234 | 4421 | `}` |
|      - | 4422 | `/*` |
|      - | 4423 | `` * PHP 8: a printf-family `$format` argument is a `string` parameter — scalars`` |
|      - | 4424 | ` * (int/float/bool) and null coerce to a string, but an array/object/resource` |
|      - | 4425 | ` * raises a catchable TypeError. iArg is the 1-based argument position ($format` |
|      - | 4426 | ` * is #1 for sprintf/printf/vprintf/vsprintf, #2 for fprintf/vfprintf). Returns` |
|      - | 4427 | ` * PH7_OK when the value is string-coercible (the caller then uses` |
|      - | 4428 | ` * ph7_value_to_string, which renders scalars/null verbatim).` |
|      - | 4429 | ` */` |
|      - | 4430 | `/*` |
|      - | 4431 | ` * php 8: a stream parameter that is not a resource is a TypeError, not a warning` |
|      - | 4432 | ` * with a 0 return -- the caller never learned its write went nowhere.` |
|      - | 4433 | ` */` |
|     24 | 4434 | `PH7_PRIVATE sxi32 PH7_CheckStreamArg(ph7_context *pCtx,ph7_value *pArg,int iArg,const char *zName)` |
|      1 | 4435 | `{` |
|     25 | 4436 | `	if( !ph7_value_is_resource(pArg) ){` |
|      - | 4437 | `		char zBuf[64];` |
|      4 | 4438 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4439 | `			"%s(): Argument #%d ($%s) must be of type resource, %s given",` |
|      1 | 4440 | `			ph7_function_name(pCtx),iArg,zName,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|      - | 4441 | `	}` |
|     23 | 4442 | `	return PH7_OK;` |
|     13 | 4443 | `}` |
|    492 | 4444 | `PH7_PRIVATE sxi32 PH7_FormatCheckFormatArg(ph7_context *pCtx,ph7_value *pArg,int iArg)` |
|      3 | 4445 | `{` |
|    495 | 4446 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg) \|\| ph7_value_is_resource(pArg) ){` |
|      - | 4447 | `		char zBuf[64];` |
|    ! 0 | 4448 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4449 | `			"%s(): Argument #%d ($format) must be of type string, %s given",` |
|    ! 0 | 4450 | `			ph7_function_name(pCtx),iArg,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|      - | 4451 | `	}` |
|    495 | 4452 | `	return PH7_OK;` |
|    249 | 4453 | `}` |
|      - | 4454 | `/*` |
|      - | 4455 | ` * Format a given string.` |
|      - | 4456 | ` * The root program.  All variations call this core.` |
|      - | 4457 | ` * INPUTS:` |
|      - | 4458 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 4459 | ` *            1. A pointer to the call context.` |
|      - | 4460 | ` *            2. A pointer to the list of characters to be output` |
|      - | 4461 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 4462 | ` *            3. An integer number of characters to be output.` |
|      - | 4463 | ` *               (Note: This number might be zero.)` |
|      - | 4464 | ` *            4. Upper layer private data.` |
|      - | 4465 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 4466 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 4467 | ` */` |
|    442 | 4468 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 4469 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 4470 | `	ph7_context *pCtx,  /* call context */` |
|      - | 4471 | `	const char *zIn,    /* Format string */` |
|      - | 4472 | `	int nByte,          /* Format string length */` |
|      - | 4473 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 4474 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 4475 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 4476 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 4477 | `	)` |
|      3 | 4478 | `{` |
|    445 | 4479 | `	char spaces[] = "                                                  ";` |
|      - | 4480 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    445 | 4481 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 4482 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 4483 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 4484 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 4485 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 4486 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 4487 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 4488 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 4489 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 4490 | `	ph7_int64 iVal;` |
|      - | 4491 | `	int precision;           /* Precision of the current field */` |
|      - | 4492 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 4493 | `	int c,rc,n;` |
|      - | 4494 | `	int length;              /* Length of the field */` |
|      - | 4495 | `	int prefix;` |
|      - | 4496 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 4497 | `	int width;               /* Width of the current field */` |
|      - | 4498 | `	int idx;` |
|    445 | 4499 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 4500 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 4501 | `	/* An unknown conversion specifier is rejected up-front by PH7_FormatValidate()` |
|      - | 4502 | `	 * (called by every format builtin before this routine), so the specifier set` |
|      - | 4503 | `	 * seen here is always valid. */` |
|      - | 4504 | `	/* Start the format process */` |
|    655 | 4505 | `	for(;;){` |
|   1313 | 4506 | `		zCur = zIn;` |
|   3701 | 4507 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|   2389 | 4508 | `			zIn++;` |
|      1 | 4509 | `		}` |
|   1313 | 4510 | `		if( zCur < zIn ){` |
|      - | 4511 | `			/* Consume chunk verbatim */` |
|    775 | 4512 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|    775 | 4513 | `			if( rc != SXRET_OK ){` |
|      - | 4514 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|    ! 0 | 4515 | `				break;` |
|      - | 4516 | `			}` |
|    387 | 4517 | `		}` |
|   1313 | 4518 | `		if( zIn >= zEnd ){` |
|      - | 4519 | `			/* No more input to process,break immediately */` |
|    443 | 4520 | `			break;` |
|      - | 4521 | `		}` |
|      - | 4522 | `		/* Find out what flags are present */` |
|    873 | 4523 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    870 | 4524 | `			flag_alternateform = flag_zeropad = 0;` |
|      - | 4525 | `		/* Reset the pad buffer to spaces: a custom pad char ('X) — or the string` |
|      - | 4526 | `		 * zero-pad below — from a PREVIOUS specifier must not bleed into this one.` |
|      - | 4527 | `		 * php resets the pad character for every specifier. */` |
|  44373 | 4528 | `		for( idx = 0 ; idx < etSPACESIZE ; ++idx ){ spaces[idx] = ' '; }` |
|    873 | 4529 | `		zIn++; /* Jump the precent sign */` |
|    435 | 4530 | `		do{` |
|   1077 | 4531 | `			c = zIn[0];` |
|   1077 | 4532 | `			switch( c ){` |
|     19 | 4533 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      7 | 4534 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 4535 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|    163 | 4536 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|      6 | 4537 | `			case '\'':` |
|     13 | 4538 | `				zIn++;` |
|     13 | 4539 | `				if( zIn < zEnd ){` |
|      - | 4540 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|     13 | 4541 | `					c = zIn[0];` |
|    613 | 4542 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    601 | 4543 | `						spaces[idx] = (char)c;` |
|    301 | 4544 | `					}` |
|     13 | 4545 | `					c = 0;` |
|      6 | 4546 | `				}` |
|     12 | 4547 | `				break;` |
|    870 | 4548 | `			default:                                       break;` |
|      - | 4549 | `			}` |
|   1077 | 4550 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 4551 | `		/* Get the field width */` |
|    873 | 4552 | `		width = 0;` |
|   1580 | 4553 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    273 | 4554 | `			width = width*10 + (zIn[0] - '0');` |
|    273 | 4555 | `			zIn++;` |
|      1 | 4556 | `		}` |
|    873 | 4557 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 4558 | `			/* Position specifer */` |
|      9 | 4559 | `			if( width > 0 ){` |
|      9 | 4560 | `				n = width;` |
|      9 | 4561 | `				if( vf && n > 0 ){` |
|    ! 0 | 4562 | `					n--;` |
|    ! 0 | 4563 | `				}` |
|      4 | 4564 | `			}` |
|      9 | 4565 | `			zIn++;` |
|      9 | 4566 | `			width = 0;` |
|      - | 4567 | `			/* php's grammar is %argnum$<flags><width>: the flags come AFTER the` |
|      - | 4568 | `			 * positional, so re-parse the full flag set here (e.g. "%1$-10s"),` |
|      - | 4569 | `			 * not just zero-padding. */` |
|      4 | 4570 | `			do{` |
|     11 | 4571 | `				c = zIn[0];` |
|     11 | 4572 | `				switch( c ){` |
|    ! 0 | 4573 | `				case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|    ! 0 | 4574 | `				case '+':   flag_plussign = 1;        c = 0;   break;` |
|    ! 0 | 4575 | `				case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|    ! 0 | 4576 | `				case '0':   flag_zeropad = 1;         c = 0;   break;` |
|      1 | 4577 | `				case '\'':` |
|      3 | 4578 | `					zIn++;` |
|      3 | 4579 | `					if( zIn < zEnd ){` |
|      3 | 4580 | `						c = zIn[0];` |
|    103 | 4581 | `						for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    101 | 4582 | `							spaces[idx] = (char)c;` |
|     51 | 4583 | `						}` |
|      3 | 4584 | `						c = 0;` |
|      1 | 4585 | `					}` |
|      2 | 4586 | `					break;` |
|      8 | 4587 | `				default:                                       break;` |
|      - | 4588 | `				}` |
|     11 | 4589 | `			}while( c==0 && (zIn++ < zEnd) );` |
|     21 | 4590 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|      9 | 4591 | `				width = width*10 + (zIn[0] - '0');` |
|      9 | 4592 | `				zIn++;` |
|      1 | 4593 | `			}` |
|      4 | 4594 | `		}` |
|    873 | 4595 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 4596 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 4597 | `		}` |
|      - | 4598 | `		/* Get the precision */` |
|    873 | 4599 | `		precision = -1;` |
|    873 | 4600 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|    113 | 4601 | `			precision = 0;` |
|    113 | 4602 | `			zIn++;` |
|    298 | 4603 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    133 | 4604 | `				precision = precision*10 + (zIn[0] - '0');` |
|    133 | 4605 | `				zIn++;` |
|      3 | 4606 | `			}` |
|     55 | 4607 | `		}` |
|      - | 4608 | `		/* Consume a single 'l' length modifier (a C-ism php accepts and ignores,` |
|      - | 4609 | `		 * e.g. "%ld"); PH7_FormatValidate mirrors this. Exactly one is skipped:` |
|      - | 4610 | `		 * in "%lld" the second 'l' becomes the (unknown) specifier, just like php. */` |
|    873 | 4611 | `		if( zIn < zEnd && zIn[0] == 'l' ){` |
|      9 | 4612 | `			zIn++;` |
|      4 | 4613 | `		}` |
|    873 | 4614 | `		if( zIn >= zEnd ){` |
|      - | 4615 | `			/* No more input */` |
|      3 | 4616 | `			break;` |
|      - | 4617 | `		}` |
|      - | 4618 | `		/* Fetch the info entry for the field */` |
|    871 | 4619 | `		pInfo = 0;` |
|    871 | 4620 | `		xtype = PH7_FMT_ERROR;` |
|    871 | 4621 | `		c = zIn[0];` |
|    871 | 4622 | `		zIn++; /* Jump the format specifer */` |
|   3395 | 4623 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|   3395 | 4624 | `			if( c==aFmt[idx].fmttype ){` |
|    871 | 4625 | `				pInfo = &aFmt[idx];` |
|    871 | 4626 | `				xtype = pInfo->type;` |
|    871 | 4627 | `				break;` |
|      - | 4628 | `			}` |
|   1265 | 4629 | `		}` |
|    871 | 4630 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    871 | 4631 | `		length = 0;` |
|      - | 4632 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 4633 | `		 /*` |
|      - | 4634 | `		  ** At this point, variables are initialized as follows:` |
|      - | 4635 | `		  **` |
|      - | 4636 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 4637 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 4638 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 4639 | `		  **                               field width was negative.` |
|      - | 4640 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 4641 | `		  **                               the conversion character.` |
|      - | 4642 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 4643 | `		  **   width                       The specified field width.  This is` |
|      - | 4644 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 4645 | `		  **   precision                   The specified precision.  The default` |
|      - | 4646 | `		  **                               is -1.` |
|      - | 4647 | `		  */` |
|    871 | 4648 | `		switch(xtype){` |
|      4 | 4649 | `		case PH7_FMT_PERCENT:` |
|      - | 4650 | `			/* A literal percent character */` |
|      9 | 4651 | `			zWorker[0] = '%';` |
|      9 | 4652 | `			length = (int)sizeof(char);` |
|      9 | 4653 | `			break;` |
|      2 | 4654 | `		case PH7_FMT_CHARX:` |
|      - | 4655 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 4656 | `			 * with that ASCII value` |
|      - | 4657 | `			 */` |
|      5 | 4658 | `			pArg = NEXT_ARG;` |
|      5 | 4659 | `			if( pArg == 0 ){` |
|    ! 0 | 4660 | `				c = 0;` |
|    ! 0 | 4661 | `			}else{` |
|      5 | 4662 | `				c = ph7_value_to_int(pArg);` |
|      - | 4663 | `			}` |
|      - | 4664 | `			/* NUL byte is an acceptable value */` |
|      5 | 4665 | `			zWorker[0] = (char)c;` |
|      5 | 4666 | `			length = (int)sizeof(char);` |
|      5 | 4667 | `			break;` |
|    188 | 4668 | `		case PH7_FMT_STRING:` |
|      - | 4669 | `			/* the argument is treated as and presented as a string */` |
|    377 | 4670 | `			pArg = NEXT_ARG;` |
|    377 | 4671 | `			if( pArg == 0 ){` |
|    ! 0 | 4672 | `				length = 0;` |
|    ! 0 | 4673 | `			}else{` |
|    377 | 4674 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 4675 | `			}` |
|    377 | 4676 | `			if( length < 1 ){` |
|      - | 4677 | `				/* An empty %s substitutes NOTHING in php. PH7 substituted a single` |
|      - | 4678 | `				 * SPACE here, so printf("[%s]","") printed "[ ]" and any format with an` |
|      - | 4679 | `				 * absent optional part gained a stray space. */` |
|      9 | 4680 | `				zBuf = "";` |
|      9 | 4681 | `				length = 0;` |
|      4 | 4682 | `			}` |
|    377 | 4683 | `			if( precision>=0 && precision<length ){` |
|      3 | 4684 | `				length = precision;` |
|      1 | 4685 | `			}` |
|    377 | 4686 | `			if( flag_zeropad ){` |
|      - | 4687 | `				/* zero-padding works on strings too */` |
|    103 | 4688 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    101 | 4689 | `					spaces[idx] = '0';` |
|     51 | 4690 | `				}` |
|      1 | 4691 | `			}` |
|    377 | 4692 | `			break;` |
|    140 | 4693 | `		case PH7_FMT_RADIX:` |
|    281 | 4694 | `			pArg = NEXT_ARG;` |
|    281 | 4695 | `			if( pArg == 0 ){` |
|    ! 0 | 4696 | `				iVal = 0;` |
|    ! 0 | 4697 | `			}else{` |
|    281 | 4698 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 4699 | `			}` |
|      - | 4700 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|    281 | 4701 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 4702 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 4703 | `			}` |
|      - | 4704 | `#if 1` |
|      - | 4705 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 4706 | `        ** I think this is stupid.*/` |
|    281 | 4707 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 4708 | `#else` |
|      - | 4709 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 4710 | `        ** but leave the prefix for hex.*/` |
|      - | 4711 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 4712 | `#endif` |
|    281 | 4713 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|    257 | 4714 | `          if( iVal<0 ){` |
|     25 | 4715 | `            iVal = -iVal;` |
|      - | 4716 | `			/* Ticket 1433-003 */` |
|     25 | 4717 | `			if( iVal < 0 ){` |
|      - | 4718 | `				/* Overflow */` |
|    ! 0 | 4719 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4720 | `			}` |
|     25 | 4721 | `            prefix = '-';` |
|    245 | 4722 | `          }else if( flag_plussign )  prefix = '+';` |
|    231 | 4723 | `          else if( flag_blanksign )  prefix = ' ';` |
|    229 | 4724 | `          else                       prefix = 0;` |
|    129 | 4725 | `        }else{` |
|     25 | 4726 | `			if( iVal<0 ){` |
|    ! 0 | 4727 | `				iVal = -iVal;` |
|      - | 4728 | `				/* Ticket 1433-003 */` |
|    ! 0 | 4729 | `				if( iVal < 0 ){` |
|      - | 4730 | `					/* Overflow */` |
|    ! 0 | 4731 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4732 | `				}` |
|    ! 0 | 4733 | `			}` |
|     25 | 4734 | `			prefix = 0;` |
|      - | 4735 | `		}` |
|    281 | 4736 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|    149 | 4737 | `          precision = width-(prefix!=0);` |
|     74 | 4738 | `        }` |
|    281 | 4739 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 4740 | `        {` |
|      - | 4741 | `          register char *cset;      /* Use registers for speed */` |
|      - | 4742 | `          register int base;` |
|    281 | 4743 | `          cset = pInfo->charset;` |
|    281 | 4744 | `          base = pInfo->base;` |
|    140 | 4745 | `          do{                                           /* Convert to ascii */` |
|    357 | 4746 | `            *(--zBuf) = cset[iVal%base];` |
|    357 | 4747 | `            iVal = iVal/base;` |
|    357 | 4748 | `          }while( iVal>0 );` |
|      - | 4749 | `        }` |
|    281 | 4750 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    447 | 4751 | `        for(idx=precision-length; idx>0; idx--){` |
|    167 | 4752 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|     84 | 4753 | `        }` |
|    281 | 4754 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|    281 | 4755 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 4756 | `          char *pre, x;` |
|    ! 0 | 4757 | `          pre = pInfo->prefix;` |
|    ! 0 | 4758 | `          if( *zBuf!=pre[0] ){` |
|    ! 0 | 4759 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|    ! 0 | 4760 | `          }` |
|    ! 0 | 4761 | `        }` |
|    281 | 4762 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    281 | 4763 | `		break;` |
|    100 | 4764 | `		case PH7_FMT_FLOAT:` |
|      - | 4765 | `		case PH7_FMT_EXP:` |
|      - | 4766 | `		case PH7_FMT_GENERIC:{` |
|      - | 4767 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 4768 | `		double realvalue;` |
|      - | 4769 | `		char zFmt[8];` |
|      - | 4770 | `		int nOut, nFmt;` |
|    203 | 4771 | `		pArg = NEXT_ARG;` |
|    203 | 4772 | `		if( pArg == 0 ){` |
|    ! 0 | 4773 | `			realvalue = 0;` |
|    ! 0 | 4774 | `		}else{` |
|    203 | 4775 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 4776 | `		}` |
|      - | 4777 | `		/* php prints the IEEE specials bare — NaN / INF / -INF with no width` |
|      - | 4778 | `		 * padding, precision, or sign flags (php_sprintf_appenddouble). */` |
|    203 | 4779 | `		if( PH7_IS_NAN(realvalue) ){` |
|     21 | 4780 | `			zBuf = "NaN";` |
|     21 | 4781 | `			length = 3;` |
|     21 | 4782 | `			width = 0;` |
|     21 | 4783 | `			break;` |
|      - | 4784 | `		}` |
|    183 | 4785 | `		if( PH7_IS_INF(realvalue) ){` |
|     37 | 4786 | `			if( realvalue < 0.0 ){` |
|     15 | 4787 | `				zBuf = "-INF";` |
|     15 | 4788 | `				length = 4;` |
|      8 | 4789 | `			}else{` |
|     23 | 4790 | `				zBuf = "INF";` |
|     23 | 4791 | `				length = 3;` |
|      - | 4792 | `			}` |
|     37 | 4793 | `			width = 0;` |
|     37 | 4794 | `			break;` |
|      - | 4795 | `		}` |
|    147 | 4796 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|    147 | 4797 | `		if( precision > 53 ){` |
|      - | 4798 | `			/* php's FORMAT_CONV_MAX_PRECISION cap, with the same E_NOTICE` |
|      - | 4799 | `			 * (message prefixed with the active function's name, like` |
|      - | 4800 | `			 * php_error_docref). */` |
|      - | 4801 | `			char zMsg[160];` |
|      4 | 4802 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 4803 | `				"%z(): Requested precision of %d digits was truncated to PHP maximum of %d digits",` |
|      2 | 4804 | `				&pCtx->pFunc->sName,precision,53);` |
|      3 | 4805 | `			PH7_VmThrowError(pCtx->pVm,0,E_NOTICE,zMsg);` |
|      3 | 4806 | `			precision = 53;` |
|      1 | 4807 | `		}` |
|      - | 4808 | ``		/* php's %f/%e extract the sign via `num < 0`, so negative zero prints`` |
|      - | 4809 | `		 * unsigned there — while %g (php_gcvt on the raw value) keeps "-0". */` |
|    147 | 4810 | `		if( xtype!=PH7_FMT_GENERIC && realvalue == 0.0 ){` |
|      9 | 4811 | `			realvalue = 0.0;` |
|      4 | 4812 | `		}` |
|      - | 4813 | `		/* php's float conversions are correctly rounded (zend_dtoa); use libc` |
|      - | 4814 | `		 * snprintf as the digit engine (the byte-exact-floats rule — the old` |
|      - | 4815 | `		 * hand-rolled vxGetdigit loop stopped at 16 significant digits, so` |
|      - | 4816 | `		 * e.g. %f of 1e308 printed zeros where php prints the exact binary64` |
|      - | 4817 | `		 * expansion), then post-process into php's exact shapes below. */` |
|    147 | 4818 | `		nFmt = 0;` |
|    147 | 4819 | `		zFmt[nFmt++] = '%';` |
|    147 | 4820 | `		if( flag_alternateform ) zFmt[nFmt++] = '#';` |
|      - | 4821 | `		/* php's ' ' flag selects space PADDING (its default), not C's` |
|      - | 4822 | `		 * space-for-positive-sign — so flag_blanksign is NOT forwarded. */` |
|    147 | 4823 | `		if( flag_plussign ) zFmt[nFmt++] = '+';` |
|    147 | 4824 | `		zFmt[nFmt++] = '.';` |
|    147 | 4825 | `		zFmt[nFmt++] = '*';` |
|    195 | 4826 | `		zFmt[nFmt++] = (char)(xtype==PH7_FMT_FLOAT ? 'f' :` |
|     32 | 4827 | `			(xtype==PH7_FMT_EXP ? ((pInfo->charset[0]=='E') ? 'E' : 'e')` |
|     32 | 4828 | `			                    : ((pInfo->charset[0]=='E') ? 'G' : 'g')));` |
|    147 | 4829 | `		zFmt[nFmt] = 0;` |
|    147 | 4830 | `		nOut = snprintf(zWorker,sizeof(zWorker),zFmt,precision,realvalue);` |
|    147 | 4831 | `		if( nOut < 0 \|\| nOut >= (int)sizeof(zWorker) ){` |
|      - | 4832 | `			/* Cannot happen with precision capped at 53 (%f of DBL_MAX is` |
|      - | 4833 | `			 * ~365 bytes); keep the truncated output rather than overrun. */` |
|    ! 0 | 4834 | `			nOut = (int)SyStrlen(zWorker);` |
|    ! 0 | 4835 | `		}` |
|    147 | 4836 | `		nOut = (int)PH7_PhpFloatShape(zWorker,(sxi32)nOut,xtype==PH7_FMT_GENERIC);` |
|    147 | 4837 | `		zBuf = zWorker;` |
|    147 | 4838 | `		length = nOut;` |
|      - | 4839 | `		/* Let the zero-pad block below insert zeros between the sign (written` |
|      - | 4840 | `		 * by snprintf) and the first digit, as before. */` |
|    147 | 4841 | `		prefix = (zWorker[0]=='-' \|\| zWorker[0]=='+' \|\| zWorker[0]==' ') ? zWorker[0] : 0;` |
|      - | 4842 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 4843 | `        ** set and we are not left justified */` |
|    147 | 4844 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 4845 | `          int i;` |
|      9 | 4846 | `          int nPad = width - length;` |
|     63 | 4847 | `          for(i=width; i>=nPad; i--){` |
|     55 | 4848 | `            zBuf[i] = zBuf[i-nPad];` |
|     28 | 4849 | `          }` |
|      9 | 4850 | `          i = prefix!=0;` |
|     39 | 4851 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      9 | 4852 | `          length = width;` |
|      4 | 4853 | `        }` |
|      - | 4854 | `#else` |
|      - | 4855 | `         zBuf = " ";` |
|      - | 4856 | `		 length = (int)sizeof(char);` |
|      - | 4857 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|    147 | 4858 | `		 break;` |
|      - | 4859 | `							 }` |
|    ! 0 | 4860 | `		default:` |
|      - | 4861 | `			/* Unreachable: PH7_FormatValidate() rejects unknown specifiers with a` |
|      - | 4862 | `			 * catchable ValueError before formatting begins. Kept as a defensive` |
|      - | 4863 | `			 * no-op that emits nothing. */` |
|    ! 0 | 4864 | `			length = 0;` |
|    ! 0 | 4865 | `			break;` |
|      - | 4866 | `		}` |
|      - | 4867 | `		 /*` |
|      - | 4868 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 4869 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 4870 | `		 ** the output.` |
|      - | 4871 | `		 */` |
|    871 | 4872 | `    if( !flag_leftjustify ){` |
|      - | 4873 | `      register int nspace;` |
|    853 | 4874 | `      nspace = width-length;` |
|    853 | 4875 | `      if( nspace>0 ){` |
|     37 | 4876 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4877 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4878 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4879 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4880 | `			}` |
|    ! 0 | 4881 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4882 | `        }` |
|     37 | 4883 | `        if( nspace>0 ){` |
|     37 | 4884 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|     37 | 4885 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4886 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4887 | `			}` |
|     18 | 4888 | `		}` |
|     18 | 4889 | `      }` |
|    425 | 4890 | `    }` |
|    871 | 4891 | `    if( length>0 ){` |
|    863 | 4892 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    863 | 4893 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4894 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4895 | `		}` |
|    430 | 4896 | `    }` |
|    871 | 4897 | `    if( flag_leftjustify ){` |
|      - | 4898 | `      register int nspace;` |
|     19 | 4899 | `      nspace = width-length;` |
|     19 | 4900 | `      if( nspace>0 ){` |
|     15 | 4901 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4902 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4903 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4904 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4905 | `			}` |
|    ! 0 | 4906 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4907 | `        }` |
|     15 | 4908 | `        if( nspace>0 ){` |
|     15 | 4909 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|     15 | 4910 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4911 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4912 | `			}` |
|      7 | 4913 | `		}` |
|      7 | 4914 | `      }` |
|      9 | 4915 | `    }` |
|      3 | 4916 | ` }/* for(;;) */` |
|    445 | 4917 | `	return SXRET_OK;` |
|    224 | 4918 | `}` |
|      - | 4919 | `/*` |
|      - | 4920 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 4921 | ` */` |
|    480 | 4922 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      3 | 4923 | `{` |
|      - | 4924 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|      - | 4925 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|      - | 4926 | `	 * non-OK rc also stops the format loop. */` |
|    483 | 4927 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|    483 | 4928 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|    483 | 4929 | `	return *pRc;` |
|      3 | 4930 | `}` |
|      - | 4931 | `/*` |
|      - | 4932 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 4933 | ` *  Return a formatted string.` |
|      - | 4934 | ` * Parameters` |
|      - | 4935 | ` *  $format` |
|      - | 4936 | ` *    The format string (see block comment above)` |
|      - | 4937 | ` * Return` |
|      - | 4938 | ` *  A string produced according to the formatting string format.` |
|      - | 4939 | ` */` |
|    236 | 4940 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4941 | `{` |
|      - | 4942 | `	const char *zFormat;` |
|    239 | 4943 | `	sxi32 rc = SXRET_OK;` |
|      - | 4944 | `	int nLen;` |
|    239 | 4945 | `	if( nArg < 1 ){` |
|      - | 4946 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4947 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4948 | `		return PH7_OK;` |
|      - | 4949 | `	}` |
|      - | 4950 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|    239 | 4951 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    239 | 4952 | `	if( rc != PH7_OK ){` |
|    ! 0 | 4953 | `		return rc;` |
|      - | 4954 | `	}` |
|      - | 4955 | `	/* Extract the string format (scalars/null coerce). */` |
|    239 | 4956 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    239 | 4957 | `	if( nLen < 1 ){` |
|      - | 4958 | `		/* Empty string */` |
|    ! 0 | 4959 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4960 | `		return PH7_OK;` |
|      - | 4961 | `	}` |
|      - | 4962 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4963 | `	 * output; propagate the throw status verbatim. */` |
|    239 | 4964 | `	rc = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    239 | 4965 | `	if( rc != PH7_OK ){` |
|     17 | 4966 | `		return rc;` |
|      - | 4967 | `	}` |
|      - | 4968 | `	/* PHP 8: too few value arguments is a catchable ArgumentCountError before output. */` |
|    223 | 4969 | `	rc = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg-1,1,FALSE);` |
|    223 | 4970 | `	if( rc != PH7_OK ){` |
|     11 | 4971 | `		return rc;` |
|      - | 4972 | `	}` |
|      - | 4973 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|    213 | 4974 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|    213 | 4975 | `	if( rc != SXRET_OK ){` |
|      - | 4976 | `		/* The result append ran out of memory: raise a fatal rather than` |
|      - | 4977 | `		 * returning a silently-truncated string. */` |
|    ! 0 | 4978 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4979 | `	}` |
|    213 | 4980 | `	return PH7_OK;` |
|    121 | 4981 | `}` |
|      - | 4982 | `/*` |
|      - | 4983 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 4984 | ` */` |
|   1174 | 4985 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4986 | `{` |
|   1175 | 4987 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 4988 | `	/* Call the VM output consumer directly */` |
|   1175 | 4989 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 4990 | `	/* Increment counter */` |
|   1175 | 4991 | `	*pCounter += nLen;` |
|   1175 | 4992 | `	return PH7_OK;` |
|      1 | 4993 | `}` |
|      - | 4994 | `/*` |
|      - | 4995 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 4996 | ` *  Output a formatted string.` |
|      - | 4997 | ` * Parameters` |
|      - | 4998 | ` *  $format` |
|      - | 4999 | ` *   See sprintf() for a description of format.` |
|      - | 5000 | ` * Return` |
|      - | 5001 | ` *  The length of the outputted string.` |
|      - | 5002 | ` */` |
|    206 | 5003 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5004 | `{` |
|    207 | 5005 | `	ph7_int64 nCounter = 0;` |
|      - | 5006 | `	const char *zFormat;` |
|      - | 5007 | `	int nLen;` |
|    207 | 5008 | `	if( nArg < 1 ){` |
|      - | 5009 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 5010 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5011 | `		return PH7_OK;` |
|      - | 5012 | `	}` |
|      - | 5013 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|      - | 5014 | `	{` |
|    207 | 5015 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    207 | 5016 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 5017 | `			return rcf;` |
|      - | 5018 | `		}` |
|      - | 5019 | `	}` |
|      - | 5020 | `	/* Extract the string format (scalars/null coerce). */` |
|    207 | 5021 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    207 | 5022 | `	if( nLen < 1 ){` |
|      - | 5023 | `		/* Empty string */` |
|    ! 0 | 5024 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5025 | `		return PH7_OK;` |
|      - | 5026 | `	}` |
|      - | 5027 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 5028 | `	 * output; propagate the throw status verbatim. */` |
|      - | 5029 | `	{` |
|    207 | 5030 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    207 | 5031 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 5032 | `			return rcv;` |
|      - | 5033 | `		}` |
|      - | 5034 | `		/* PHP 8: too few value arguments is a catchable ArgumentCountError before output. */` |
|    207 | 5035 | `		rcv = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg-1,1,FALSE);` |
|    207 | 5036 | `		if( rcv != PH7_OK ){` |
|      3 | 5037 | `			return rcv;` |
|      - | 5038 | `		}` |
|      - | 5039 | `	}` |
|      - | 5040 | `	/* Format the string */` |
|    205 | 5041 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 5042 | `	/* Return the length of the outputted string */` |
|    205 | 5043 | `	ph7_result_int64(pCtx,nCounter);` |
|    205 | 5044 | `	return PH7_OK;` |
|    104 | 5045 | `}` |
|      - | 5046 | `/*` |
|      - | 5047 | ` * int vprintf(string $format,array $args)` |
|      - | 5048 | ` *  Output a formatted string.` |
|      - | 5049 | ` * Parameters` |
|      - | 5050 | ` *  $format` |
|      - | 5051 | ` *   See sprintf() for a description of format.` |
|      - | 5052 | ` * Return` |
|      - | 5053 | ` *  The length of the outputted string.` |
|      - | 5054 | ` */` |
|      4 | 5055 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5056 | `{` |
|      5 | 5057 | `	ph7_int64 nCounter = 0;` |
|      - | 5058 | `	const char *zFormat;` |
|      - | 5059 | `	ph7_hashmap *pMap;` |
|      - | 5060 | `	SySet sArg;` |
|      - | 5061 | `	int nLen,n;` |
|      - | 5062 | `	sxi32 rcFmt;` |
|      5 | 5063 | `	if( nArg < 2 ){` |
|      - | 5064 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 5065 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5066 | `		return PH7_OK;` |
|      - | 5067 | `	}` |
|      - | 5068 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|      5 | 5069 | `	rcFmt = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|      5 | 5070 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 5071 | `		return rcFmt;` |
|      - | 5072 | `	}` |
|      5 | 5073 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 5074 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 5075 | `		char zBuf[64];` |
|      4 | 5076 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5077 | `			"vprintf(): Argument #2 ($values) must be of type array, %s given",` |
|      2 | 5078 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 5079 | `	}` |
|      - | 5080 | `	/* Extract the string format (scalars/null coerce). */` |
|      3 | 5081 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5082 | `	if( nLen < 1 ){` |
|      - | 5083 | `		/* Empty string */` |
|    ! 0 | 5084 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5085 | `		return PH7_OK;` |
|      - | 5086 | `	}` |
|      - | 5087 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 5088 | `	 * output; propagate the throw status verbatim. */` |
|      3 | 5089 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      3 | 5090 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 5091 | `		return rcFmt;` |
|      - | 5092 | `	}` |
|      - | 5093 | `	/* Point to the hashmap */` |
|      3 | 5094 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 5095 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output.` |
|      - | 5096 | `	 * Checked on the entry count before materialising the value set. */` |
|      3 | 5097 | `	rcFmt = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|      3 | 5098 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 5099 | `		return rcFmt;` |
|      - | 5100 | `	}` |
|      - | 5101 | `	/* Extract arguments from the hashmap */` |
|      3 | 5102 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 5103 | `	/* Format the string */` |
|      3 | 5104 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 5105 | `	/* Release the container */` |
|      3 | 5106 | `	SySetRelease(&sArg);` |
|      - | 5107 | `	/* Return the length of the outputted string */` |
|      3 | 5108 | `	ph7_result_int64(pCtx,nCounter);` |
|      3 | 5109 | `	return PH7_OK;` |
|      3 | 5110 | `}` |
|      - | 5111 | `/*` |
|      - | 5112 | ` * int vsprintf(string $format,array $args)` |
|      - | 5113 | ` *  Output a formatted string.` |
|      - | 5114 | ` * Parameters` |
|      - | 5115 | ` *  $format` |
|      - | 5116 | ` *   See sprintf() for a description of format.` |
|      - | 5117 | ` * Return` |
|      - | 5118 | ` *  A string produced according to the formatting string format.` |
|      - | 5119 | ` */` |
|     24 | 5120 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5121 | `{` |
|      - | 5122 | `	const char *zFormat;` |
|      - | 5123 | `	ph7_hashmap *pMap;` |
|      - | 5124 | `	SySet sArg;` |
|     25 | 5125 | `	sxi32 rc = SXRET_OK;` |
|      - | 5126 | `	sxi32 rcFmt;` |
|      - | 5127 | `	int nLen,n;` |
|     25 | 5128 | `	if( nArg < 2 ){` |
|      - | 5129 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5130 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5131 | `		return PH7_OK;` |
|      - | 5132 | `	}` |
|      - | 5133 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|     25 | 5134 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|     25 | 5135 | `	if( rc != PH7_OK ){` |
|    ! 0 | 5136 | `		return rc;` |
|      - | 5137 | `	}` |
|     25 | 5138 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 5139 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 5140 | `		char zBuf[64];` |
|     16 | 5141 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5142 | `			"vsprintf(): Argument #2 ($values) must be of type array, %s given",` |
|     10 | 5143 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 5144 | `	}` |
|      - | 5145 | `	/* Extract the string format (scalars/null coerce). */` |
|     15 | 5146 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 5147 | `	if( nLen < 1 ){` |
|      - | 5148 | `		/* Empty string */` |
|    ! 0 | 5149 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5150 | `		return PH7_OK;` |
|      - | 5151 | `	}` |
|      - | 5152 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 5153 | `	 * output; propagate the throw status verbatim. */` |
|     15 | 5154 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|     15 | 5155 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 5156 | `		return rcFmt;` |
|      - | 5157 | `	}` |
|      - | 5158 | `	/* Point to hashmap */` |
|     15 | 5159 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 5160 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output. */` |
|     15 | 5161 | `	rcFmt = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|     15 | 5162 | `	if( rcFmt != PH7_OK ){` |
|      5 | 5163 | `		return rcFmt;` |
|      - | 5164 | `	}` |
|      - | 5165 | `	/* Extract arguments from the hashmap */` |
|     11 | 5166 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 5167 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|     11 | 5168 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|      - | 5169 | `	/* Release the container */` |
|     11 | 5170 | `	SySetRelease(&sArg);` |
|     11 | 5171 | `	if( rc != SXRET_OK ){` |
|      - | 5172 | `		/* The result append ran out of memory: raise a fatal. */` |
|    ! 0 | 5173 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5174 | `	}` |
|     11 | 5175 | `	return PH7_OK;` |
|     13 | 5176 | `}` |
|      - | 5177 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5178 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5179 | `/*` |
|      - | 5180 | ` * Symisc eXtension.` |
|      - | 5181 | ` * string size_format(int64 $size)` |
|      - | 5182 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 5183 | ` *  Example:` |
|      - | 5184 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 5185 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 5186 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 5187 | ` * Parameter` |
|      - | 5188 | ` *  $size` |
|      - | 5189 | ` *    Entity size in bytes.` |
|      - | 5190 | ` * Return` |
|      - | 5191 | ` *   Formatted string representation of the given size.` |
|      - | 5192 | ` */` |
|     24 | 5193 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5194 | `{` |
|      - | 5195 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 5196 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 5197 | `	sxi32 nRest,i_32;` |
|      - | 5198 | `	ph7_int64 iSize;` |
|     25 | 5199 | `	int c = -1; /* index in zUnit[] */` |
|      - | 5200 |  |
|     25 | 5201 | `	if( nArg < 1 ){` |
|      - | 5202 | `		/* Missing argument,return the empty string */` |
|      3 | 5203 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5204 | `		return PH7_OK;` |
|      - | 5205 | `	}` |
|      - | 5206 | `	/* Extract the given size */` |
|     23 | 5207 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 5208 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 5209 | `		/* Don't bother formatting,return immediately */` |
|      5 | 5210 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 5211 | `		return PH7_OK;` |
|      - | 5212 | `	}` |
|     19 | 5213 | `	for(;;){` |
|     39 | 5214 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 5215 | `		iSize >>= 10;` |
|     39 | 5216 | `		c++;` |
|     39 | 5217 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 5218 | `			break;` |
|      - | 5219 | `		}` |
|      1 | 5220 | `	}` |
|     19 | 5221 | `	nRest /= 100;` |
|     19 | 5222 | `	if( nRest > 9 ){` |
|    ! 0 | 5223 | `		nRest = 9;` |
|    ! 0 | 5224 | `	}` |
|     19 | 5225 | `	if( iSize > 999 ){` |
|    ! 0 | 5226 | `		c++;` |
|    ! 0 | 5227 | `		nRest = 9;` |
|    ! 0 | 5228 | `		iSize = 0;` |
|    ! 0 | 5229 | `	}` |
|     19 | 5230 | `	i_32 = (sxi32)iSize;` |
|      - | 5231 | `	/* Format */` |
|     19 | 5232 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 5233 | `	return PH7_OK;` |
|     13 | 5234 | `}` |
|      - | 5235 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 5236 | `/*` |
|      - | 5237 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 5238 | ` *   Calculate the md5 hash of a string.` |
|      - | 5239 | ` * Parameter` |
|      - | 5240 | ` *  $str` |
|      - | 5241 | ` *   Input string` |
|      - | 5242 | ` * $raw_output` |
|      - | 5243 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 5244 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 5245 | ` * Return` |
|      - | 5246 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 5247 | ` */` |
|     12 | 5248 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5249 | `{` |
|      - | 5250 | `	unsigned char zDigest[16];` |
|     13 | 5251 | `	int raw_output = FALSE;` |
|      - | 5252 | `	const void *pIn;` |
|      - | 5253 | `	int nLen;` |
|     13 | 5254 | `	if( nArg < 1 ){` |
|      - | 5255 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5256 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5257 | `		return PH7_OK;` |
|      - | 5258 | `	}` |
|      - | 5259 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 5260 | `	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */` |
|     13 | 5261 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 5262 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 5263 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 5264 | `	}` |
|      - | 5265 | `	/* Compute the MD5 digest */` |
|     13 | 5266 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|     13 | 5267 | `	if( raw_output ){` |
|      - | 5268 | `		/* Output raw digest */` |
|      5 | 5269 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 5270 | `	}else{` |
|      - | 5271 | `		/* Perform a binary to hex conversion */` |
|      9 | 5272 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 5273 | `	}` |
|     13 | 5274 | `	return PH7_OK;` |
|      7 | 5275 | `}` |
|      - | 5276 | `/*` |
|      - | 5277 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 5278 | ` *   Calculate the sha1 hash of a string.` |
|      - | 5279 | ` * Parameter` |
|      - | 5280 | ` *  $str` |
|      - | 5281 | ` *   Input string` |
|      - | 5282 | ` * $raw_output` |
|      - | 5283 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 5284 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 5285 | ` * Return` |
|      - | 5286 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 5287 | ` */` |
|     10 | 5288 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5289 | `{` |
|      - | 5290 | `	unsigned char zDigest[20];` |
|     11 | 5291 | `	int raw_output = FALSE;` |
|      - | 5292 | `	const void *pIn;` |
|      - | 5293 | `	int nLen;` |
|     11 | 5294 | `	if( nArg < 1 ){` |
|      - | 5295 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5296 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5297 | `		return PH7_OK;` |
|      - | 5298 | `	}` |
|      - | 5299 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 5300 | `	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */` |
|     11 | 5301 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5302 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 5303 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 5304 | `	}` |
|      - | 5305 | `	/* Compute the SHA1 digest */` |
|     11 | 5306 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|     11 | 5307 | `	if( raw_output ){` |
|      - | 5308 | `		/* Output raw digest */` |
|      5 | 5309 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 5310 | `	}else{` |
|      - | 5311 | `		/* Perform a binary to hex conversion */` |
|      7 | 5312 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 5313 | `	}` |
|     11 | 5314 | `	return PH7_OK;` |
|      6 | 5315 | `}` |
|      - | 5316 | `/*` |
|      - | 5317 | ` * int64 crc32(string $str)` |
|      - | 5318 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 5319 | ` * Parameter` |
|      - | 5320 | ` *  $str` |
|      - | 5321 | ` *   Input string` |
|      - | 5322 | ` * Return` |
|      - | 5323 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 5324 | ` */` |
|      2 | 5325 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5326 | `{` |
|      - | 5327 | `	const void *pIn;` |
|      - | 5328 | `	sxu32 nCRC;` |
|      - | 5329 | `	int nLen;` |
|      3 | 5330 | `	if( nArg < 1 ){` |
|      - | 5331 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 5332 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5333 | `		return PH7_OK;` |
|      - | 5334 | `	}` |
|      - | 5335 | `	/* Extract the input string */` |
|      3 | 5336 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5337 | `	if( nLen < 1 ){` |
|      - | 5338 | `		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike` |
|      - | 5339 | `		 * md5()/sha1(), whose empty-string digests are non-zero. */` |
|    ! 0 | 5340 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5341 | `		return PH7_OK;` |
|      - | 5342 | `	}` |
|      - | 5343 | `	/* Calculate the sum */` |
|      3 | 5344 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 5345 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 5346 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 5347 | `	return PH7_OK;` |
|      2 | 5348 | `}` |
|      - | 5349 | `/*` |
|      - | 5350 | ` * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is` |
|      - | 5351 | ` * described by a small record so one dispatch (and one generic HMAC) serves them` |
|      - | 5352 | ` * all. Thin adapters normalize the differing context types and the reversed` |
|      - | 5353 | ` * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.` |
|      - | 5354 | ` */` |
|     11 | 5355 | `static void HashMd5Init(HashCtx *c){ MD5Init(&c->md5); }` |
|     15 | 5356 | `static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }` |
|     11 | 5357 | `static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }` |
|     11 | 5358 | `static void HashSha1Init(HashCtx *c){ SHA1Init(&c->sha1); }` |
|     15 | 5359 | `static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }` |
|     11 | 5360 | `static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }` |
|      9 | 5361 | `static void HashSha224Init(HashCtx *c){ SHA224Init(&c->sha256); }` |
|     33 | 5362 | `static void HashSha256Init(HashCtx *c){ SHA256Init(&c->sha256); }` |
|     57 | 5363 | `static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }` |
|     41 | 5364 | `static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }` |
|      9 | 5365 | `static void HashSha384Init(HashCtx *c){ SHA384Init(&c->sha512); }` |
|     15 | 5366 | `static void HashSha512Init(HashCtx *c){ SHA512Init(&c->sha512); }` |
|     27 | 5367 | `static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }` |
|     23 | 5368 | `static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }` |
|      - | 5369 | `typedef struct HashAlgo HashAlgo;` |
|      - | 5370 | `struct HashAlgo {` |
|      - | 5371 | `	const char *zName;   /* lowercase canonical name */` |
|      - | 5372 | `	int nDigestLen;      /* output bytes: 16/20/28/32/48/64 */` |
|      - | 5373 | `	int nBlockLen;       /* internal block bytes (for HMAC): 64 or 128 */` |
|      - | 5374 | `	void (*xInit)(HashCtx *);` |
|      - | 5375 | `	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);` |
|      - | 5376 | `	void (*xFinal)(HashCtx *,unsigned char *);` |
|      - | 5377 | `};` |
|      - | 5378 | `static const HashAlgo aHashAlgo[] = {` |
|      - | 5379 | `	{ "md5",    16, 64,  HashMd5Init,    HashMd5Update,    HashMd5Final    },` |
|      - | 5380 | `	{ "sha1",   20, 64,  HashSha1Init,   HashSha1Update,   HashSha1Final   },` |
|      - | 5381 | `	{ "sha224", 28, 64,  HashSha224Init, HashSha256Update, HashSha256Final },` |
|      - | 5382 | `	{ "sha256", 32, 64,  HashSha256Init, HashSha256Update, HashSha256Final },` |
|      - | 5383 | `	{ "sha384", 48, 128, HashSha384Init, HashSha512Update, HashSha512Final },` |
|      - | 5384 | `	{ "sha512", 64, 128, HashSha512Init, HashSha512Update, HashSha512Final },` |
|      - | 5385 | `};` |
|      - | 5386 | `/* Case-insensitive algorithm lookup (PHP accepts 'SHA256' etc.). */` |
|     73 | 5387 | `static const HashAlgo * HashFindAlgo(const char *zName,int nLen){` |
|      - | 5388 | `	sxu32 i;` |
|    279 | 5389 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|    272 | 5390 | `		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen` |
|    211 | 5391 | `			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){` |
|     67 | 5392 | `			return &aHashAlgo[i];` |
|      - | 5393 | `		}` |
|    106 | 5394 | `	}` |
|      6 | 5395 | `	return 0;` |
|     38 | 5396 | `}` |
|      - | 5397 | `/*` |
|      - | 5398 | ` * string hash(string $algo,string $data[,bool $binary = false])` |
|      - | 5399 | ` *   Generate a hash value (message digest).` |
|      - | 5400 | ` */` |
|     54 | 5401 | `static int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5402 | `{` |
|      - | 5403 | `	const HashAlgo *pAlgo;` |
|      - | 5404 | `	const char *zAlgo,*zData;` |
|     56 | 5405 | `	int nAlgoLen,nDataLen,raw_output = FALSE;` |
|      - | 5406 | `	HashCtx sCtx;` |
|      - | 5407 | `	unsigned char zDigest[64];` |
|     56 | 5408 | `	if( nArg < 2 ){` |
|    ! 0 | 5409 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5410 | `			"hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5411 | `	}` |
|     56 | 5412 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     56 | 5413 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     56 | 5414 | `	if( pAlgo == 0 ){` |
|      3 | 5415 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5416 | `			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - | 5417 | `	}` |
|     53 | 5418 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     53 | 5419 | `	if( nArg > 2 ){` |
|      9 | 5420 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|      4 | 5421 | `	}` |
|     53 | 5422 | `	pAlgo->xInit(&sCtx);` |
|     53 | 5423 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     53 | 5424 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     53 | 5425 | `	if( raw_output ){` |
|      9 | 5426 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|      5 | 5427 | `	}else{` |
|     45 | 5428 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - | 5429 | `	}` |
|     53 | 5430 | `	return PH7_OK;` |
|     29 | 5431 | `}` |
|      - | 5432 | `/*` |
|      - | 5433 | ` * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])` |
|      - | 5434 | ` *   Generate a keyed hash value using the HMAC method (RFC 2104).` |
|      - | 5435 | ` */` |
|     16 | 5436 | `static int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5437 | `{` |
|      - | 5438 | `	const HashAlgo *pAlgo;` |
|      - | 5439 | `	const char *zAlgo,*zData,*zKey;` |
|     18 | 5440 | `	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;` |
|      - | 5441 | `	HashCtx sCtx;` |
|      - | 5442 | `	unsigned char zKeyBlock[128],zIpad[128],zOpad[128],zInner[64],zDigest[64];` |
|      - | 5443 | `	int i,nBlock,nDigest;` |
|     18 | 5444 | `	if( nArg < 3 ){` |
|    ! 0 | 5445 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5446 | `			"hash_hmac() expects at least 3 arguments, %d given",nArg);` |
|      - | 5447 | `	}` |
|     18 | 5448 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     18 | 5449 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     18 | 5450 | `	if( pAlgo == 0 ){` |
|      3 | 5451 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5452 | `			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 5453 | `	}` |
|     15 | 5454 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     15 | 5455 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|     15 | 5456 | `	if( nArg > 3 ){` |
|      3 | 5457 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|      1 | 5458 | `	}` |
|     15 | 5459 | `	nBlock = pAlgo->nBlockLen;` |
|     15 | 5460 | `	nDigest = pAlgo->nDigestLen;` |
|      - | 5461 | `	/* Reduce the key to a single block: hash it if longer than the block, then` |
|      - | 5462 | `	 * zero-pad (a short or empty key is just zero-padded). */` |
|     15 | 5463 | `	SyZero(zKeyBlock,sizeof(zKeyBlock));` |
|     15 | 5464 | `	if( nKeyLen > nBlock ){` |
|      3 | 5465 | `		pAlgo->xInit(&sCtx);` |
|      3 | 5466 | `		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);` |
|      3 | 5467 | `		pAlgo->xFinal(&sCtx,zKeyBlock);` |
|     14 | 5468 | `	}else if( nKeyLen > 0 ){` |
|     11 | 5469 | `		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);` |
|      5 | 5470 | `	}` |
|   1039 | 5471 | `	for( i = 0; i < nBlock; i++ ){` |
|   1025 | 5472 | `		zIpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);` |
|   1025 | 5473 | `		zOpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);` |
|    513 | 5474 | `	}` |
|      - | 5475 | `	/* inner = H((key ^ ipad) \|\| data) */` |
|     15 | 5476 | `	pAlgo->xInit(&sCtx);` |
|     15 | 5477 | `	pAlgo->xUpdate(&sCtx,zIpad,(unsigned int)nBlock);` |
|     15 | 5478 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     15 | 5479 | `	pAlgo->xFinal(&sCtx,zInner);` |
|      - | 5480 | `	/* out = H((key ^ opad) \|\| inner) */` |
|     15 | 5481 | `	pAlgo->xInit(&sCtx);` |
|     15 | 5482 | `	pAlgo->xUpdate(&sCtx,zOpad,(unsigned int)nBlock);` |
|     15 | 5483 | `	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)nDigest);` |
|     15 | 5484 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     15 | 5485 | `	if( raw_output ){` |
|      3 | 5486 | `		ph7_result_string(pCtx,(const char *)zDigest,nDigest);` |
|      2 | 5487 | `	}else{` |
|     13 | 5488 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);` |
|      - | 5489 | `	}` |
|     15 | 5490 | `	return PH7_OK;` |
|     10 | 5491 | `}` |
|      - | 5492 | `/*` |
|      - | 5493 | ` * bool hash_equals(string $known_string,string $user_string)` |
|      - | 5494 | ` *   Timing-attack-safe string comparison.` |
|      - | 5495 | ` */` |
|     12 | 5496 | `static int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5497 | `{` |
|      - | 5498 | `	const char *zKnown,*zUser;` |
|      - | 5499 | `	int nKnown,nUser,i;` |
|     14 | 5500 | `	volatile unsigned char vDiff = 0;` |
|     14 | 5501 | `	if( nArg < 2 ){` |
|    ! 0 | 5502 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5503 | `			"hash_equals() expects exactly 2 arguments, %d given",nArg);` |
|      - | 5504 | `	}` |
|     14 | 5505 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      4 | 5506 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5507 | `			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",` |
|      1 | 5508 | `			ph7_type_name(apArg[0]));` |
|      - | 5509 | `	}` |
|     11 | 5510 | `	if( !ph7_value_is_string(apArg[1]) ){` |
|    ! 0 | 5511 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5512 | `			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",` |
|    ! 0 | 5513 | `			ph7_type_name(apArg[1]));` |
|      - | 5514 | `	}` |
|     11 | 5515 | `	zKnown = ph7_value_to_string(apArg[0],&nKnown);` |
|     11 | 5516 | `	zUser = ph7_value_to_string(apArg[1],&nUser);` |
|     11 | 5517 | `	if( nKnown != nUser ){` |
|      5 | 5518 | `		ph7_result_bool(pCtx,0);` |
|      5 | 5519 | `		return PH7_OK;` |
|      - | 5520 | `	}` |
|      - | 5521 | `	/* Constant-time: read every byte, never short-circuit. */` |
|     19 | 5522 | `	for( i = 0; i < nKnown; i++ ){` |
|     13 | 5523 | `		vDiff \|= (unsigned char)(zKnown[i] ^ zUser[i]);` |
|      7 | 5524 | `	}` |
|      7 | 5525 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|      7 | 5526 | `	return PH7_OK;` |
|      8 | 5527 | `}` |
|      - | 5528 | `/*` |
|      - | 5529 | ` * array hash_algos(void)` |
|      - | 5530 | ` *   Return a list of the registered hashing algorithms.` |
|      - | 5531 | ` */` |
|      2 | 5532 | `static int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5533 | `{` |
|      - | 5534 | `	ph7_value *pArray,*pValue;` |
|      - | 5535 | `	sxu32 i;` |
|      1 | 5536 | `	SXUNUSED(nArg);` |
|      1 | 5537 | `	SXUNUSED(apArg);` |
|      3 | 5538 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5539 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 5540 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 5541 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5542 | `		return PH7_OK;` |
|      - | 5543 | `	}` |
|     15 | 5544 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|     13 | 5545 | `		ph7_value_string(pValue,aHashAlgo[i].zName,-1);` |
|     13 | 5546 | `		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);` |
|     13 | 5547 | `		ph7_value_reset_string_cursor(pValue);` |
|      7 | 5548 | `	}` |
|      3 | 5549 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5550 | `	return PH7_OK;` |
|      2 | 5551 | `}` |
|      - | 5552 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 5553 | `/*` |
|      - | 5554 | ` * password_* (bcrypt). These live in ext/standard in real PHP — outside the` |
|      - | 5555 | ` * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.` |
|      - | 5556 | ` */` |
|      - | 5557 | `/*` |
|      - | 5558 | ` * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a` |
|      - | 5559 | ` * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).` |
|      - | 5560 | ` */` |
|     40 | 5561 | `static int BcryptParseHash(const char *zHash,int nHash,int *piCost)` |
|      1 | 5562 | `{` |
|      - | 5563 | `	int iCost;` |
|     40 | 5564 | `	if( nHash != 60 \|\| zHash[0] != '$' \|\| zHash[1] != '2' \|\| zHash[3] != '$'` |
|     29 | 5565 | `		\|\| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){` |
|     13 | 5566 | `		return FALSE;` |
|      - | 5567 | `	}` |
|     29 | 5568 | `	if( zHash[4] < '0' \|\| zHash[4] > '9' \|\| zHash[5] < '0' \|\| zHash[5] > '9' \|\| zHash[6] != '$' ){` |
|    ! 0 | 5569 | `		return FALSE;` |
|      - | 5570 | `	}` |
|     29 | 5571 | `	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');` |
|     29 | 5572 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      3 | 5573 | `		return FALSE;` |
|      - | 5574 | `	}` |
|     27 | 5575 | `	if( piCost ){ *piCost = iCost; }` |
|     27 | 5576 | `	return TRUE;` |
|     21 | 5577 | `}` |
|      - | 5578 | `/*` |
|      - | 5579 | ` * TRUE if the $algo argument selects bcrypt: null (PASSWORD_DEFAULT) or the` |
|      - | 5580 | ` * "2y" id (PASSWORD_BCRYPT/PASSWORD_DEFAULT). bcrypt is the only supported algo.` |
|      - | 5581 | ` */` |
|     20 | 5582 | `static int BcryptIsBcryptAlgo(ph7_value *pAlgo)` |
|      3 | 5583 | `{` |
|     23 | 5584 | `	if( ph7_value_is_null(pAlgo) ){` |
|    ! 0 | 5585 | `		return TRUE;` |
|      - | 5586 | `	}` |
|     23 | 5587 | `	if( ph7_value_is_string(pAlgo) ){` |
|      - | 5588 | `		int nAlgo;` |
|     23 | 5589 | `		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);` |
|     23 | 5590 | `		return ( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' );` |
|      - | 5591 | `	}` |
|    ! 0 | 5592 | `	return FALSE;` |
|     13 | 5593 | `}` |
|      - | 5594 | `/*` |
|      - | 5595 | ` * bool\|string password_hash(string $password,string\|int\|null $algo[,array $options])` |
|      - | 5596 | ` *  Create a bcrypt hash of the password.` |
|      - | 5597 | ` */` |
|     16 | 5598 | `static int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 5599 | `{` |
|      - | 5600 | `	const char *zPwd;` |
|     19 | 5601 | `	int nPwd,iCost = 12;` |
|      - | 5602 | `	unsigned char aSalt[16];` |
|      - | 5603 | `	char zHash[60];` |
|     19 | 5604 | `	if( nArg < 2 ){` |
|    ! 0 | 5605 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5606 | `			"password_hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5607 | `	}` |
|     19 | 5608 | `	if( !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      3 | 5609 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5610 | `			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");` |
|      - | 5611 | `	}` |
|      - | 5612 | `	/* cost from $options['cost'] (default 12). */` |
|     16 | 5613 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|     14 | 5614 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|     14 | 5615 | `		if( pCost ){ iCost = ph7_value_to_int(pCost); }` |
|      6 | 5616 | `	}` |
|     16 | 5617 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      4 | 5618 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 5619 | `			"Invalid bcrypt cost parameter specified: %d",iCost);` |
|      - | 5620 | `	}` |
|     13 | 5621 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     13 | 5622 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 5623 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - | 5624 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|      - | 5625 | `	}` |
|     13 | 5626 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){` |
|    ! 0 | 5627 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5628 | `		return PH7_OK;` |
|      - | 5629 | `	}` |
|     13 | 5630 | `	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));` |
|     13 | 5631 | `	return PH7_OK;` |
|     11 | 5632 | `}` |
|      - | 5633 | `/*` |
|      - | 5634 | ` * bool password_verify(string $password,string $hash)` |
|      - | 5635 | ` *  Verify a password against a bcrypt hash. Never throws on a malformed hash.` |
|      - | 5636 | ` */` |
|     28 | 5637 | `static int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5638 | `{` |
|      - | 5639 | `	const char *zPwd,*zHash;` |
|      - | 5640 | `	int nPwd,nHash,iCost,i;` |
|      - | 5641 | `	unsigned char aSalt[16];` |
|      - | 5642 | `	char zComputed[60];` |
|     29 | 5643 | `	volatile unsigned char vDiff = 0;` |
|     29 | 5644 | `	if( nArg < 2 ){` |
|    ! 0 | 5645 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5646 | `			"password_verify() expects exactly 2 arguments, %d given",nArg);` |
|      - | 5647 | `	}` |
|     29 | 5648 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     29 | 5649 | `	zHash = ph7_value_to_string(apArg[1],&nHash);` |
|     29 | 5650 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|     11 | 5651 | `		ph7_result_bool(pCtx,0);` |
|     11 | 5652 | `		return PH7_OK;` |
|      - | 5653 | `	}` |
|      - | 5654 | `	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */` |
|     19 | 5655 | `	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 5656 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5657 | `		return PH7_OK;` |
|      - | 5658 | `	}` |
|     19 | 5659 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){` |
|    ! 0 | 5660 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5661 | `		return PH7_OK;` |
|      - | 5662 | `	}` |
|      - | 5663 | `	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps` |
|      - | 5664 | `	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */` |
|    577 | 5665 | `	for( i = 29; i < 60; i++ ){` |
|    559 | 5666 | `		vDiff \|= (unsigned char)(zComputed[i] ^ zHash[i]);` |
|    280 | 5667 | `	}` |
|     19 | 5668 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|     19 | 5669 | `	return PH7_OK;` |
|     15 | 5670 | `}` |
|      - | 5671 | `/*` |
|      - | 5672 | ` * array password_get_info(string $hash)` |
|      - | 5673 | ` *  Return ["algo"=>id\|null, "algoName"=>name, "options"=>[...]].` |
|      - | 5674 | ` */` |
|      6 | 5675 | `static int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5676 | `{` |
|      7 | 5677 | `	const char *zHash = "";` |
|      7 | 5678 | `	int nHash,iCost = 0,bBcrypt = 0;` |
|      - | 5679 | `	ph7_value *pArray,*pOptions,*pVal;` |
|      7 | 5680 | `	if( nArg > 0 ){` |
|      7 | 5681 | `		zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 5682 | `		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);` |
|      3 | 5683 | `	}` |
|      7 | 5684 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 5685 | `	pOptions = ph7_context_new_array(pCtx);` |
|      7 | 5686 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      7 | 5687 | `	if( pArray == 0 \|\| pOptions == 0 \|\| pVal == 0 ){` |
|    ! 0 | 5688 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5689 | `		return PH7_OK;` |
|      - | 5690 | `	}` |
|      7 | 5691 | `	if( bBcrypt ){` |
|      5 | 5692 | `		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */` |
|      5 | 5693 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 5694 | `		ph7_value_reset_string_cursor(pVal);` |
|      5 | 5695 | `		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);` |
|      5 | 5696 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      5 | 5697 | `		ph7_value_int(pVal,iCost);` |
|      5 | 5698 | `		ph7_array_add_strkey_elem(pOptions,"cost",pVal);` |
|      3 | 5699 | `	}else{` |
|      3 | 5700 | `		ph7_value_null(pVal);                          /* algo => null */` |
|      3 | 5701 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      3 | 5702 | `		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);` |
|      3 | 5703 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      - | 5704 | `	}` |
|      7 | 5705 | `	ph7_array_add_strkey_elem(pArray,"options",pOptions);` |
|      7 | 5706 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 5707 | `	return PH7_OK;` |
|      4 | 5708 | `}` |
|      - | 5709 | `/*` |
|      - | 5710 | ` * bool password_needs_rehash(string $hash,string\|int\|null $algo[,array $options])` |
|      - | 5711 | ` *  True if the hash was not made with the given algo/options.` |
|      - | 5712 | ` */` |
|      6 | 5713 | `static int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5714 | `{` |
|      - | 5715 | `	const char *zHash;` |
|      7 | 5716 | `	int nHash,iCost = 0,iWantCost = 12;` |
|      7 | 5717 | `	if( nArg < 2 ){` |
|    ! 0 | 5718 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5719 | `			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5720 | `	}` |
|      7 | 5721 | `	zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 5722 | `	if( !BcryptParseHash(zHash,nHash,&iCost) \|\| !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      - | 5723 | `		/* A non-bcrypt hash, or a request for a different algo → needs rehash. */` |
|      3 | 5724 | `		ph7_result_bool(pCtx,1);` |
|      3 | 5725 | `		return PH7_OK;` |
|      - | 5726 | `	}` |
|      5 | 5727 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|      5 | 5728 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|      5 | 5729 | `		if( pCost ){ iWantCost = ph7_value_to_int(pCost); }` |
|      2 | 5730 | `	}` |
|      5 | 5731 | `	ph7_result_bool(pCtx,iCost != iWantCost);` |
|      5 | 5732 | `	return PH7_OK;` |
|      4 | 5733 | `}` |
|      - | 5734 | `/*` |
|      - | 5735 | ` * filter_var() — input validation and sanitization (the ext/filter API).` |
|      - | 5736 | ` *` |
|      - | 5737 | ` * Filter and flag identifiers (values match PHP 8.5; the constants themselves` |
|      - | 5738 | ` * are registered in constant.c). The validate filters are hand-rolled rather` |
|      - | 5739 | ` * than delegating to SyStrToInt64/SyStrToReal: the former silently skips leading` |
|      - | 5740 | ` * zeros and cannot signal overflow, and the latter treats ',' as a decimal point` |
|      - | 5741 | ` * unconditionally — neither matches PHP's filter semantics.` |
|      - | 5742 | ` */` |
|      - | 5743 | `#define FV_VALIDATE_INT     257` |
|      - | 5744 | `#define FV_VALIDATE_BOOLEAN 258` |
|      - | 5745 | `#define FV_VALIDATE_FLOAT   259` |
|      - | 5746 | `#define FV_VALIDATE_REGEXP  272` |
|      - | 5747 | `#define FV_VALIDATE_URL     273` |
|      - | 5748 | `#define FV_VALIDATE_EMAIL   274` |
|      - | 5749 | `#define FV_VALIDATE_IP      275` |
|      - | 5750 | `#define FV_VALIDATE_MAC     276` |
|      - | 5751 | `#define FV_VALIDATE_DOMAIN  277` |
|      - | 5752 | `#define FV_SANITIZE_SPECIAL_CHARS      515` |
|      - | 5753 | `#define FV_DEFAULT          516 /* == FILTER_UNSAFE_RAW: pass the value through */` |
|      - | 5754 | `#define FV_SANITIZE_EMAIL   517` |
|      - | 5755 | `#define FV_SANITIZE_URL     518` |
|      - | 5756 | `#define FV_SANITIZE_NUMBER_INT   519` |
|      - | 5757 | `#define FV_SANITIZE_NUMBER_FLOAT 520` |
|      - | 5758 | `#define FV_SANITIZE_FULL_SPECIAL_CHARS 522` |
|      - | 5759 | `#define FV_FLAG_ALLOW_OCTAL  1` |
|      - | 5760 | `#define FV_FLAG_ALLOW_HEX    2` |
|      - | 5761 | `#define FV_FLAG_STRIP_LOW    4` |
|      - | 5762 | `#define FV_FLAG_STRIP_HIGH   8` |
|      - | 5763 | `#define FV_FLAG_ENCODE_LOW   16` |
|      - | 5764 | `#define FV_FLAG_ENCODE_HIGH  32` |
|      - | 5765 | `#define FV_FLAG_ENCODE_AMP   64` |
|      - | 5766 | `#define FV_FLAG_NO_ENCODE_QUOTES 128` |
|      - | 5767 | `#define FV_FLAG_STRIP_BACKTICK   512` |
|      - | 5768 | `#define FV_FLAG_ALLOW_FRACTION   4096` |
|      - | 5769 | `#define FV_FLAG_ALLOW_THOUSAND   8192` |
|      - | 5770 | `#define FV_FLAG_ALLOW_SCIENTIFIC 16384` |
|      - | 5771 | `#define FV_FLAG_IPV4  1048576` |
|      - | 5772 | `#define FV_FLAG_IPV6  2097152` |
|      - | 5773 | `#define FV_NULL_ON_FAILURE 134217728` |
|      - | 5774 | `/* The subset of flags the UNSAFE_RAW/DEFAULT string filter (FvSanitizeString)` |
|      - | 5775 | ` * acts on: when none are set the filter is a verbatim pass-through, so FV_DEFAULT` |
|      - | 5776 | ` * can shortcut. Keep this in sync with FvSanitizeString's flag handling. */` |
|      - | 5777 | `#define FV_FLAG_STRING_MASK (FV_FLAG_STRIP_LOW\|FV_FLAG_STRIP_HIGH\|FV_FLAG_STRIP_BACKTICK \` |
|      - | 5778 | `                            \|FV_FLAG_ENCODE_LOW\|FV_FLAG_ENCODE_HIGH\|FV_FLAG_ENCODE_AMP)` |
|      - | 5779 |  |
|      - | 5780 | `/* Trim leading/trailing PHP whitespace, adjusting the (*pz,*pn) view in place.` |
|      - | 5781 | ` * SyisSpace (isspace) matches PHP's filter whitespace set " \t\n\r\v\f". */` |
|    153 | 5782 | `static void FvTrim(const char **pz,int *pn){` |
|    153 | 5783 | `	const char *z = *pz;` |
|    153 | 5784 | `	int n = *pn;` |
|    157 | 5785 | `	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }` |
|    161 | 5786 | `	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }` |
|    153 | 5787 | `	*pz = z; *pn = n;` |
|    153 | 5788 | `}` |
|      - | 5789 | `/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     57 | 5790 | `static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){` |
|     57 | 5791 | `	int neg = 0, i;` |
|     57 | 5792 | `	sxu64 u = 0;` |
|     57 | 5793 | `	FvTrim(&z,&n);` |
|     57 | 5794 | `	if( n==0 ){ return 0; }` |
|     51 | 5795 | `	if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }` |
|     51 | 5796 | `	if( n==0 ){ return 0; }` |
|     49 | 5797 | `	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'\|\|z[1]=='X') ){` |
|      3 | 5798 | `		z += 2; n -= 2;` |
|      3 | 5799 | `		if( n==0 ){ return 0; }` |
|      7 | 5800 | `		for( i=0; i<n; i++ ){` |
|      5 | 5801 | `			int h = SyHexToint((unsigned char)z[i]);` |
|      5 | 5802 | `			if( h<0 ){ return 0; }` |
|      5 | 5803 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }` |
|      5 | 5804 | `			u = u*16 + (sxu64)h;` |
|      3 | 5805 | `		}` |
|     48 | 5806 | `	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){` |
|      9 | 5807 | `		for( i=0; i<n; i++ ){` |
|      7 | 5808 | `			if( z[i]<'0' \|\| z[i]>'7' ){ return 0; }` |
|      7 | 5809 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }` |
|      7 | 5810 | `			u = u*8 + (sxu64)(z[i]-'0');` |
|      4 | 5811 | `		}` |
|      2 | 5812 | `	}else{` |
|     45 | 5813 | `		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */` |
|    201 | 5814 | `		for( i=0; i<n; i++ ){` |
|    173 | 5815 | `			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|    161 | 5816 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }` |
|    161 | 5817 | `			u = u*10 + (sxu64)(z[i]-'0');` |
|     81 | 5818 | `		}` |
|      - | 5819 | `	}` |
|     33 | 5820 | `	if( neg ){` |
|      5 | 5821 | `		if( u > 0x8000000000000000ULL ){ return 0; }` |
|      5 | 5822 | `		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */` |
|      3 | 5823 | `	}else{` |
|     29 | 5824 | `		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }` |
|     27 | 5825 | `		*pOut = (ph7_int64)u;` |
|      - | 5826 | `	}` |
|     31 | 5827 | `	return 1;` |
|     29 | 5828 | `}` |
|      - | 5829 | `/* FILTER_VALIDATE_FLOAT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     69 | 5830 | `static int FvValidateFloat(const char *z,int n,int flags,double *pOut){` |
|      - | 5831 | `	char zBuf[512];` |
|     69 | 5832 | `	int i, m = 0, seenDigit = 0;` |
|     69 | 5833 | `	const char *zv; int nv; double d = 0;` |
|     69 | 5834 | `	FvTrim(&z,&n);` |
|      - | 5835 | `	/* Bound the input: zBuf[512] holds the thousand-separator-stripped copy, and` |
|      - | 5836 | `	 * the cap also rejects the pathological 500+ digit floats PHP refuses. */` |
|     69 | 5837 | `	if( n==0 \|\| n>500 ){ return 0; }` |
|     69 | 5838 | `	if( flags & FV_FLAG_ALLOW_THOUSAND ){` |
|      - | 5839 | `		/* Commas are optional, but when present they must group the integer part` |
|      - | 5840 | `		 * into a leading run of 1..3 digits followed by groups of exactly 3` |
|      - | 5841 | `		 * ("1,000" ok, "1,5"/"1234,567" rejected). Strip them into zBuf and reject` |
|      - | 5842 | `		 * a comma anywhere in the fractional/exponent tail. */` |
|     25 | 5843 | `		int s = 0, intEnd, segStart, segIdx, hasComma = 0;` |
|     25 | 5844 | `		if( s<n && (z[s]=='+'\|\|z[s]=='-') ){ zBuf[m++] = z[s]; s++; }` |
|     25 | 5845 | `		intEnd = s;` |
|    167 | 5846 | `		while( intEnd<n && z[intEnd]!='.' && z[intEnd]!='e' && z[intEnd]!='E' ){` |
|    143 | 5847 | `			if( z[intEnd]==',' ){ hasComma = 1; }` |
|    143 | 5848 | `			intEnd++;` |
|      1 | 5849 | `		}` |
|     25 | 5850 | `		if( hasComma ){` |
|     25 | 5851 | `			segStart = s; segIdx = 0;` |
|    165 | 5852 | `			for( i=s; i<=intEnd; i++ ){` |
|    151 | 5853 | `				if( i==intEnd \|\| z[i]==',' ){` |
|     49 | 5854 | `					int segLen = i - segStart, k;` |
|     49 | 5855 | `					if( segIdx==0 ){ if( segLen<1 \|\| segLen>3 ){ return 0; } }` |
|     25 | 5856 | `					else if( segLen!=3 ){ return 0; }` |
|    119 | 5857 | `					for( k=segStart; k<i; k++ ){` |
|     81 | 5858 | `						if( !SyisDigit((unsigned char)z[k]) ){ return 0; }` |
|     81 | 5859 | `						zBuf[m++] = z[k];` |
|     41 | 5860 | `					}` |
|     39 | 5861 | `					segStart = i+1; segIdx++;` |
|     19 | 5862 | `				}` |
|     71 | 5863 | `			}` |
|      8 | 5864 | `		}else{` |
|    ! 0 | 5865 | `			for( i=s; i<intEnd; i++ ){ zBuf[m++] = z[i]; }` |
|      - | 5866 | `		}` |
|     27 | 5867 | `		for( i=intEnd; i<n; i++ ){` |
|     13 | 5868 | `			if( z[i]==',' ){ return 0; }` |
|     13 | 5869 | `			zBuf[m++] = z[i];` |
|      7 | 5870 | `		}` |
|     15 | 5871 | `		zv = zBuf; nv = m;` |
|      8 | 5872 | `	}else{` |
|     45 | 5873 | `		zv = z; nv = n;` |
|      - | 5874 | `	}` |
|     59 | 5875 | `	i = 0;` |
|     59 | 5876 | `	if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|    167 | 5877 | `	while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     59 | 5878 | `	if( i<nv && zv[i]=='.' ){` |
|     21 | 5879 | `		i++;` |
|     39 | 5880 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     10 | 5881 | `	}` |
|     59 | 5882 | `	if( !seenDigit ){ return 0; }` |
|     57 | 5883 | `	if( i<nv && (zv[i]=='e'\|\|zv[i]=='E') ){` |
|     29 | 5884 | `		i++;` |
|     29 | 5885 | `		if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|     29 | 5886 | `		if( i>=nv \|\| !SyisDigit((unsigned char)zv[i]) ){ return 0; }` |
|    105 | 5887 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; }` |
|     14 | 5888 | `	}` |
|     57 | 5889 | `	if( i!=nv ){ return 0; } /* trailing junk */` |
|      - | 5890 | `	/* The grammar above guarantees zv[0..nv) is a clean ASCII decimal float (no hex /` |
|      - | 5891 | `	 * inf / nan / trailing junk), so it is safe to hand to libc strtod, which — unlike` |
|      - | 5892 | `	 * SyStrToReal (15 sig-digits + exponent clamped to 308, so it silently saturates` |
|      - | 5893 | `	 * overflowing magnitudes to a finite value) — is overflow/underflow-aware and` |
|      - | 5894 | `	 * correctly rounded. strtod needs a NUL-terminated string: the ALLOW_THOUSAND path` |
|      - | 5895 | `	 * already built the span in zBuf (zv==zBuf); the plain path must copy it there (z is` |
|      - | 5896 | `	 * const + not NUL-terminated). nv <= n <= 500 < sizeof(zBuf) by the cap above.` |
|      - | 5897 | `	 * Matches PHP 8.5 byte-for-byte: reject overflow (-> +/-INF) and total underflow` |
|      - | 5898 | `	 * (-> 0.0), keep subnormals (nonzero, errno==ERANGE) and a genuine "0" (errno==0). */` |
|     53 | 5899 | `	if( zv != zBuf ){ SyMemcpy(zv,zBuf,(sxu32)nv); }` |
|     53 | 5900 | `	zBuf[nv] = 0;` |
|     53 | 5901 | `	errno = 0;` |
|     53 | 5902 | `	d = strtod(zBuf,0);` |
|     53 | 5903 | `	if( errno == ERANGE && (d == HUGE_VAL \|\| d == -HUGE_VAL \|\| d == 0.0) ){` |
|     15 | 5904 | `		return 0;` |
|      - | 5905 | `	}` |
|     39 | 5906 | `	*pOut = d;` |
|     39 | 5907 | `	return 1;` |
|     35 | 5908 | `}` |
|      - | 5909 | `/* FILTER_VALIDATE_BOOLEAN. Returns 1 if the string is recognized (sets *pBool),` |
|      - | 5910 | ` * 0 if it is unrecognized (the failure path). "0"/"false"/"" are recognized as` |
|      - | 5911 | ` * false, NOT failures. */` |
|     33 | 5912 | `static int FvValidateBool(const char *z,int n,int *pBool){` |
|     33 | 5913 | `	FvTrim(&z,&n);` |
|     32 | 5914 | `	if( (n==1 && z[0]=='1') \|\| (n==4 && SyStrnicmp(z,"true",4)==0)` |
|     25 | 5915 | `	    \|\| (n==2 && SyStrnicmp(z,"on",2)==0) \|\| (n==3 && SyStrnicmp(z,"yes",3)==0) ){` |
|     11 | 5916 | `		*pBool = 1; return 1;` |
|      - | 5917 | `	}` |
|     22 | 5918 | `	if( n==0 \|\| (n==1 && z[0]=='0') \|\| (n==5 && SyStrnicmp(z,"false",5)==0)` |
|     11 | 5919 | `	    \|\| (n==3 && SyStrnicmp(z,"off",3)==0) \|\| (n==2 && SyStrnicmp(z,"no",2)==0) ){` |
|     11 | 5920 | `		*pBool = 0; return 1;` |
|      - | 5921 | `	}` |
|      9 | 5922 | `	return 0;` |
|     15 | 5923 | `}` |
|      - | 5924 | `/* IPv4 dotted-quad: exactly 4 octets 0..255, no leading zeros. */` |
|     33 | 5925 | `static int FvValidateIp4(const char *z,int n){` |
|     33 | 5926 | `	int i = 0, parts = 0;` |
|     77 | 5927 | `	while( i<n ){` |
|     65 | 5928 | `		int val = 0, digits = 0, start = i;` |
|    143 | 5929 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){` |
|     85 | 5930 | `			val = val*10 + (z[i]-'0');` |
|     85 | 5931 | `			if( val>255 ){ return 0; }` |
|     79 | 5932 | `			digits++; i++;` |
|      1 | 5933 | `		}` |
|     59 | 5934 | `		if( digits==0 \|\| digits>3 ){ return 0; }` |
|     49 | 5935 | `		if( digits>1 && z[start]=='0' ){ return 0; } /* leading zero */` |
|     45 | 5936 | `		parts++;` |
|     45 | 5937 | `		if( parts>4 ){ return 0; }` |
|     45 | 5938 | `		if( i<n ){` |
|     33 | 5939 | `			if( z[i]!='.' ){ return 0; }` |
|     33 | 5940 | `			i++;` |
|     33 | 5941 | `			if( i>=n ){ return 0; } /* trailing dot */` |
|     16 | 5942 | `		}` |
|      1 | 5943 | `	}` |
|     13 | 5944 | `	return parts==4;` |
|     17 | 5945 | `}` |
|      - | 5946 | `/* A colon-separated run of IPv6 hextets with no "::" (n may be 0 -> 0 groups),` |
|      - | 5947 | ` * allowing a trailing embedded IPv4. Returns the 16-bit group count or -1. */` |
|     19 | 5948 | `static int FvIp6Hextets(const char *z,int n){` |
|     19 | 5949 | `	int i = 0, segStart = 0, groups = 0;` |
|     19 | 5950 | `	if( n==0 ){ return 0; }` |
|    145 | 5951 | `	while( i<=n ){` |
|    133 | 5952 | `		if( i==n \|\| z[i]==':' ){` |
|     23 | 5953 | `			int segLen = i - segStart, j, isV4 = 0;` |
|     23 | 5954 | `			if( segLen==0 ){ return -1; } /* an empty hextet (stray ':') */` |
|     77 | 5955 | `			for( j=segStart; j<i; j++ ){ if( z[j]=='.' ){ isV4 = 1; break; } }` |
|     23 | 5956 | `			if( isV4 ){` |
|     11 | 5957 | `				if( i!=n ){ return -1; } /* IPv4 only as the final token */` |
|     11 | 5958 | `				if( !FvValidateIp4(z+segStart,segLen) ){ return -1; }` |
|      5 | 5959 | `				groups += 2;` |
|      3 | 5960 | `			}else{` |
|     13 | 5961 | `				if( segLen>4 ){ return -1; }` |
|     47 | 5962 | `				for( j=segStart; j<i; j++ ){ if( SyHexToint((unsigned char)z[j])<0 ){ return -1; } }` |
|     13 | 5963 | `				groups++;` |
|      - | 5964 | `			}` |
|     17 | 5965 | `			segStart = i+1;` |
|      8 | 5966 | `		}` |
|    127 | 5967 | `		i++;` |
|      1 | 5968 | `	}` |
|     13 | 5969 | `	return groups;` |
|     10 | 5970 | `}` |
|      - | 5971 | `/* IPv6: at most one "::" zero-run; 8 groups exactly, or fewer when "::" present. */` |
|     19 | 5972 | `static int FvValidateIp6(const char *z,int n){` |
|     19 | 5973 | `	const char *zDbl = 0;` |
|      - | 5974 | `	int i, ga, gb;` |
|    139 | 5975 | `	for( i=0; i+1<n; i++ ){` |
|    123 | 5976 | `		if( z[i]==':' && z[i+1]==':' ){` |
|     13 | 5977 | `			if( zDbl ){ return 0; } /* a second "::" is invalid */` |
|     11 | 5978 | `			zDbl = z+i;` |
|      5 | 5979 | `		}` |
|     61 | 5980 | `	}` |
|     17 | 5981 | `	if( zDbl==0 ){` |
|      9 | 5982 | `		return FvIp6Hextets(z,n)==8;` |
|    ! 0 | 5983 | `	}else{` |
|      9 | 5984 | `		int lenA = (int)(zDbl - z);` |
|      9 | 5985 | `		int lenB = n - lenA - 2;` |
|      9 | 5986 | `		ga = (lenA==0) ? 0 : FvIp6Hextets(z,lenA);` |
|      9 | 5987 | `		gb = (lenB==0) ? 0 : FvIp6Hextets(zDbl+2,lenB);` |
|      9 | 5988 | `		if( ga<0 \|\| gb<0 ){ return 0; }` |
|      9 | 5989 | `		return (ga+gb)<=7; /* "::" stands for at least one zero group */` |
|      - | 5990 | `	}` |
|     10 | 5991 | `}` |
|     25 | 5992 | `static int FvValidateIp(const char *z,int n,int flags){` |
|     25 | 5993 | `	int v4 = (flags & FV_FLAG_IPV4), v6 = (flags & FV_FLAG_IPV6);` |
|     25 | 5994 | `	if( !v4 && !v6 ){ v4 = v6 = 1; } /* default accepts either family */` |
|     25 | 5995 | `	if( v4 && FvValidateIp4(z,n) ){ return 1; }` |
|     21 | 5996 | `	if( v6 && FvValidateIp6(z,n) ){ return 1; }` |
|     13 | 5997 | `	return 0;` |
|     13 | 5998 | `}` |
|      - | 5999 | `/* FILTER_VALIDATE_MAC: 17-char colon- or dash-separated hex (XX:XX:..:XX). */` |
|     11 | 6000 | `static int FvValidateMac(const char *z,int n){` |
|      - | 6001 | `	char sep;` |
|      - | 6002 | `	int i;` |
|     11 | 6003 | `	if( n!=17 ){ return 0; }` |
|      7 | 6004 | `	sep = z[2];` |
|      7 | 6005 | `	if( sep!=':' && sep!='-' ){ return 0; }` |
|    105 | 6006 | `	for( i=0; i<17; i++ ){` |
|    101 | 6007 | `		if( (i%3)==2 ){ if( z[i]!=sep ){ return 0; } }` |
|     71 | 6008 | `		else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }` |
|     50 | 6009 | `	}` |
|      5 | 6010 | `	return 1;` |
|      6 | 6011 | `}` |
|      - | 6012 | `/* FILTER_VALIDATE_EMAIL (best-effort: covers the common cases, not quoted local` |
|      - | 6013 | ` * parts or IP-literal domains). */` |
|     28 | 6014 | `static int FvValidateEmail(const char *z,int n){` |
|     28 | 6015 | `	int at = -1, i, localLen, domLen, labelStart, dotCount = 0;` |
|      - | 6016 | `	const char *zDom;` |
|     28 | 6017 | `	if( n==0 \|\| n>320 ){ return 0; }` |
|    201 | 6018 | `	for( i=0; i<n; i++ ){` |
|    181 | 6019 | `		if( z[i]=='@' ){ if( at>=0 ){ return 0; } at = i; }` |
|     91 | 6020 | `	}` |
|     21 | 6021 | `	if( at<=0 \|\| at==n-1 ){ return 0; } /* one '@', non-empty local and domain */` |
|     21 | 6022 | `	localLen = at;` |
|     21 | 6023 | `	zDom = z + at + 1;` |
|     21 | 6024 | `	domLen = n - at - 1;` |
|     21 | 6025 | `	if( z[0]=='.' \|\| z[at-1]=='.' ){ return 0; }` |
|     57 | 6026 | `	for( i=0; i<localLen; i++ ){` |
|     43 | 6027 | `		unsigned char c = (unsigned char)z[i];` |
|     43 | 6028 | `		if( c<=' ' ){ return 0; }` |
|     41 | 6029 | `		if( c=='.' && i+1<localLen && z[i+1]=='.' ){ return 0; }` |
|     20 | 6030 | `	}` |
|     15 | 6031 | `	if( zDom[0]=='.' \|\| zDom[domLen-1]=='.' ){ return 0; }` |
|     13 | 6032 | `	labelStart = 0;` |
|     85 | 6033 | `	for( i=0; i<=domLen; i++ ){` |
|     75 | 6034 | `		if( i==domLen \|\| zDom[i]=='.' ){` |
|     25 | 6035 | `			int ll = i - labelStart;` |
|     25 | 6036 | `			if( ll==0 ){ return 0; } /* consecutive dots */` |
|     23 | 6037 | `			if( zDom[labelStart]=='-' \|\| zDom[i-1]=='-' ){ return 0; }` |
|     23 | 6038 | `			if( i<domLen ){ dotCount++; }` |
|     23 | 6039 | `			labelStart = i+1;` |
|     12 | 6040 | `		}else{` |
|     51 | 6041 | `			unsigned char c = (unsigned char)zDom[i];` |
|     51 | 6042 | `			if( !((c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9')\|\|c=='-') ){ return 0; }` |
|      - | 6043 | `		}` |
|     37 | 6044 | `	}` |
|     11 | 6045 | `	if( dotCount<1 ){ return 0; } /* PHP requires a dot in the domain (any TLD length) */` |
|      9 | 6046 | `	return 1;` |
|     15 | 6047 | `}` |
|      - | 6048 | `/* FILTER_VALIDATE_DOMAIN (lenient, matching PHP without FILTER_FLAG_HOSTNAME). */` |
|     11 | 6049 | `static int FvValidateDomain(const char *z,int n){` |
|      - | 6050 | `	int i;` |
|     11 | 6051 | `	if( n<1 \|\| n>253 \|\| z[0]=='.' ){ return 0; }` |
|     81 | 6052 | `	for( i=0; i<n; i++ ){` |
|     75 | 6053 | `		unsigned char c = (unsigned char)z[i];` |
|     75 | 6054 | `		if( c<=' ' ){ return 0; }` |
|     75 | 6055 | `		if( c=='.' && i+1<n && z[i+1]=='.' ){ return 0; }` |
|     37 | 6056 | `	}` |
|      7 | 6057 | `	return 1;` |
|      6 | 6058 | `}` |
|      - | 6059 | `/* FILTER_VALIDATE_URL: require a scheme and a host (PHP's filter is itself` |
|      - | 6060 | ` * parse_url-based, so PH7_VmHttpSplitURI tracks it closely). */` |
|     15 | 6061 | `static int FvValidateUrl(const char *z,int n){` |
|      - | 6062 | `	SyhttpUri sUri;` |
|     15 | 6063 | `	if( n==0 ){ return 0; }` |
|     15 | 6064 | `	SyZero(&sUri,(sxu32)sizeof(sUri));` |
|     15 | 6065 | `	if( PH7_VmHttpSplitURI(&sUri,z,(sxu32)n)!=SXRET_OK ){ return 0; }` |
|     15 | 6066 | `	return sUri.sScheme.nByte!=0 && sUri.sHost.nByte!=0;` |
|      8 | 6067 | `}` |
|      - | 6068 | `/* The Fv sanitizers build their result by appending directly to the call` |
|      - | 6069 | ` * context (ph7_result_string accumulates, like htmlspecialchars), emitting each` |
|      - | 6070 | ` * kept run in one call and seeding "" so an all-stripped input yields "". */` |
|      - | 6071 | `/* SANITIZE_NUMBER_INT (isFloat=0) / SANITIZE_NUMBER_FLOAT (isFloat=1). */` |
|     37 | 6072 | `static void FvSanitizeNumber(ph7_context *pCtx,const char *z,int n,int isFloat,int flags){` |
|     37 | 6073 | `	int i, runStart = 0;` |
|     37 | 6074 | `	ph7_result_string(pCtx,"",0);` |
|     97 | 6075 | `	for( i=0; i<n; i++ ){` |
|     91 | 6076 | `		char c = z[i];` |
|     91 | 6077 | `		int keep = (c>='0'&&c<='9') \|\| c=='+' \|\| c=='-';` |
|     91 | 6078 | `		if( !keep && isFloat ){` |
|     38 | 6079 | `			keep = (c=='.' && (flags & FV_FLAG_ALLOW_FRACTION))` |
|     23 | 6080 | `			    \|\| (c==',' && (flags & FV_FLAG_ALLOW_THOUSAND))` |
|     36 | 6081 | `			    \|\| ((c=='e'\|\|c=='E') && (flags & FV_FLAG_ALLOW_SCIENTIFIC));` |
|     12 | 6082 | `		}` |
|     61 | 6083 | `		if( !keep ){` |
|     33 | 6084 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     33 | 6085 | `			runStart = i+1;` |
|     16 | 6086 | `		}` |
|     31 | 6087 | `	}` |
|      7 | 6088 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 | 6089 | `}` |
|      - | 6090 | `/* Return non-zero when byte c must be stripped under the STRIP_* flags. Shared` |
|      - | 6091 | ` * by the UNSAFE_RAW string filter and SANITIZE_SPECIAL_CHARS. STRIP_LOW drops` |
|      - | 6092 | `` * bytes <32, STRIP_HIGH drops bytes >=127 (incl. DEL), STRIP_BACKTICK drops '`'.`` |
|      - | 6093 | ` * Matches php_filter_strip(); verified byte-exact vs php 8.5.7. */` |
|    287 | 6094 | `static int FvStripByte(unsigned char c,int flags){` |
|    287 | 6095 | `	if( (flags & FV_FLAG_STRIP_LOW)      && c<32 )    { return 1; }` |
|    281 | 6096 | `	if( (flags & FV_FLAG_STRIP_HIGH)     && c>=127 )  { return 1; }` |
|    269 | 6097 | `	if( (flags & FV_FLAG_STRIP_BACKTICK) && c==0x60 ) { return 1; }` |
|    267 | 6098 | `	return 0;` |
|    144 | 6099 | `}` |
|      - | 6100 | `/* FILTER_UNSAFE_RAW / FILTER_DEFAULT with flags: no default transform, but the` |
|      - | 6101 | ` * STRIP/ENCODE flags apply. Precedence (per php_filter_unsafe_raw, verified` |
|      - | 6102 | ` * vs php 8.5.7): a byte is first tested for stripping; a surviving byte is then` |
|      - | 6103 | ` * encoded as a decimal numeric entity if ENCODE_LOW (<32) / ENCODE_HIGH (>=127)` |
|      - | 6104 | ` * is set, and '&' becomes "&#38;" under ENCODE_AMP. So STRIP_LOW\|ENCODE_LOW` |
|      - | 6105 | ` * strips (nothing left to encode). Bytes are treated individually — ENCODE_HIGH` |
|      - | 6106 | ` * numeric-encodes each byte of a multibyte sequence separately, not the codepoint. */` |
|     25 | 6107 | `static void FvSanitizeString(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 6108 | `	int i, runStart = 0;` |
|     25 | 6109 | `	ph7_result_string(pCtx,"",0);` |
|    193 | 6110 | `	for( i=0; i<n; i++ ){` |
|    179 | 6111 | `		unsigned char c = (unsigned char)z[i];` |
|    179 | 6112 | `		if( FvStripByte(c,flags) ){` |
|     13 | 6113 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     13 | 6114 | `			runStart = i+1;` |
|     13 | 6115 | `			continue;` |
|      - | 6116 | `		}` |
|    167 | 6117 | `		if( c=='&' && (flags & FV_FLAG_ENCODE_AMP) ){` |
|      3 | 6118 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      3 | 6119 | `			ph7_result_string(pCtx,"&#38;",-1);` |
|      3 | 6120 | `			runStart = i+1;` |
|    166 | 6121 | `		}else if( (c<32 && (flags & FV_FLAG_ENCODE_LOW))` |
|    164 | 6122 | `		       \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     37 | 6123 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 6124 | `			ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|      9 | 6125 | `			runStart = i+1;` |
|      4 | 6126 | `		}` |
|     79 | 6127 | `	}` |
|     15 | 6128 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     15 | 6129 | `}` |
|      - | 6130 | `/* FILTER_SANITIZE_SPECIAL_CHARS: encode <>&"' and every control byte <32 as a` |
|      - | 6131 | ` * decimal numeric entity (&#60; &#38; &#34; ...). The STRIP_* flags remove bytes` |
|      - | 6132 | ` * before encoding; ENCODE_HIGH numeric-encodes surviving bytes >=127. Bytes >=128` |
|      - | 6133 | ` * are otherwise passed through verbatim (this filter is NOT UTF-8-aware — only the` |
|      - | 6134 | ` * FULL variant is). Byte-exact vs php 8.5.7. */` |
|     13 | 6135 | `static void FvSanitizeSpecial(ph7_context *pCtx,const char *z,int n,int flags){` |
|     13 | 6136 | `	int i, runStart = 0;` |
|      - | 6137 | `	const char *zEnt;` |
|     13 | 6138 | `	ph7_result_string(pCtx,"",0);` |
|    131 | 6139 | `	for( i=0; i<n; i++ ){` |
|    119 | 6140 | `		unsigned char c = (unsigned char)z[i];` |
|    119 | 6141 | `		if( FvStripByte(c,flags) ){` |
|      9 | 6142 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 6143 | `			runStart = i+1;` |
|      9 | 6144 | `			continue;` |
|      - | 6145 | `		}` |
|    111 | 6146 | `		switch( c ){` |
|      3 | 6147 | `		case '<':  zEnt = "&#60;"; break;` |
|      3 | 6148 | `		case '>':  zEnt = "&#62;"; break;` |
|     11 | 6149 | `		case '&':  zEnt = "&#38;"; break;` |
|      3 | 6150 | `		case '"':  zEnt = "&#34;"; break;` |
|      3 | 6151 | `		case '\'': zEnt = "&#39;"; break;` |
|     46 | 6152 | `		default:` |
|      - | 6153 | `			/* Control bytes <32 are always numeric-encoded; bytes >=127 only when` |
|      - | 6154 | `			 * ENCODE_HIGH is set. Everything else stays in the current run. */` |
|     93 | 6155 | `			if( c<32 \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     17 | 6156 | `				if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     17 | 6157 | `				ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|     17 | 6158 | `				runStart = i+1;` |
|      8 | 6159 | `			}` |
|     93 | 6160 | `			continue; /* keep in the current run */` |
|      - | 6161 | `		}` |
|     19 | 6162 | `		if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     19 | 6163 | `		ph7_result_string(pCtx,zEnt,-1); /* -1: length from strlen */` |
|     19 | 6164 | `		runStart = i+1;` |
|     10 | 6165 | `	}` |
|     13 | 6166 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     13 | 6167 | `}` |
|      - | 6168 | `/* HTML 4.01 named-entity table (codepoint -> "&name;") used by the UTF-8-aware` |
|      - | 6169 | ` * FULL_SPECIAL_CHARS filter, sorted ascending by codepoint for binary search.` |
|      - | 6170 | ` * Generated from php 8.5.7 (the exact set php_escape_html_entities emits for the` |
|      - | 6171 | ` * default document type); the five inline specials <>&"' are handled separately,` |
|      - | 6172 | ` * so every entry here is a codepoint >=0xA0. 248 rows. */` |
|      - | 6173 | `static const struct { sxu32 cp; const char *zEnt; } aHtml401Ent[] = {` |
|      - | 6174 | `	{0x00A0,"&nbsp;"},{0x00A1,"&iexcl;"},{0x00A2,"&cent;"},{0x00A3,"&pound;"},` |
|      - | 6175 | `	{0x00A4,"&curren;"},{0x00A5,"&yen;"},{0x00A6,"&brvbar;"},{0x00A7,"&sect;"},` |
|      - | 6176 | `	{0x00A8,"&uml;"},{0x00A9,"&copy;"},{0x00AA,"&ordf;"},{0x00AB,"&laquo;"},` |
|      - | 6177 | `	{0x00AC,"&not;"},{0x00AD,"&shy;"},{0x00AE,"&reg;"},{0x00AF,"&macr;"},` |
|      - | 6178 | `	{0x00B0,"&deg;"},{0x00B1,"&plusmn;"},{0x00B2,"&sup2;"},{0x00B3,"&sup3;"},` |
|      - | 6179 | `	{0x00B4,"&acute;"},{0x00B5,"&micro;"},{0x00B6,"&para;"},{0x00B7,"&middot;"},` |
|      - | 6180 | `	{0x00B8,"&cedil;"},{0x00B9,"&sup1;"},{0x00BA,"&ordm;"},{0x00BB,"&raquo;"},` |
|      - | 6181 | `	{0x00BC,"&frac14;"},{0x00BD,"&frac12;"},{0x00BE,"&frac34;"},{0x00BF,"&iquest;"},` |
|      - | 6182 | `	{0x00C0,"&Agrave;"},{0x00C1,"&Aacute;"},{0x00C2,"&Acirc;"},{0x00C3,"&Atilde;"},` |
|      - | 6183 | `	{0x00C4,"&Auml;"},{0x00C5,"&Aring;"},{0x00C6,"&AElig;"},{0x00C7,"&Ccedil;"},` |
|      - | 6184 | `	{0x00C8,"&Egrave;"},{0x00C9,"&Eacute;"},{0x00CA,"&Ecirc;"},{0x00CB,"&Euml;"},` |
|      - | 6185 | `	{0x00CC,"&Igrave;"},{0x00CD,"&Iacute;"},{0x00CE,"&Icirc;"},{0x00CF,"&Iuml;"},` |
|      - | 6186 | `	{0x00D0,"&ETH;"},{0x00D1,"&Ntilde;"},{0x00D2,"&Ograve;"},{0x00D3,"&Oacute;"},` |
|      - | 6187 | `	{0x00D4,"&Ocirc;"},{0x00D5,"&Otilde;"},{0x00D6,"&Ouml;"},{0x00D7,"&times;"},` |
|      - | 6188 | `	{0x00D8,"&Oslash;"},{0x00D9,"&Ugrave;"},{0x00DA,"&Uacute;"},{0x00DB,"&Ucirc;"},` |
|      - | 6189 | `	{0x00DC,"&Uuml;"},{0x00DD,"&Yacute;"},{0x00DE,"&THORN;"},{0x00DF,"&szlig;"},` |
|      - | 6190 | `	{0x00E0,"&agrave;"},{0x00E1,"&aacute;"},{0x00E2,"&acirc;"},{0x00E3,"&atilde;"},` |
|      - | 6191 | `	{0x00E4,"&auml;"},{0x00E5,"&aring;"},{0x00E6,"&aelig;"},{0x00E7,"&ccedil;"},` |
|      - | 6192 | `	{0x00E8,"&egrave;"},{0x00E9,"&eacute;"},{0x00EA,"&ecirc;"},{0x00EB,"&euml;"},` |
|      - | 6193 | `	{0x00EC,"&igrave;"},{0x00ED,"&iacute;"},{0x00EE,"&icirc;"},{0x00EF,"&iuml;"},` |
|      - | 6194 | `	{0x00F0,"&eth;"},{0x00F1,"&ntilde;"},{0x00F2,"&ograve;"},{0x00F3,"&oacute;"},` |
|      - | 6195 | `	{0x00F4,"&ocirc;"},{0x00F5,"&otilde;"},{0x00F6,"&ouml;"},{0x00F7,"&divide;"},` |
|      - | 6196 | `	{0x00F8,"&oslash;"},{0x00F9,"&ugrave;"},{0x00FA,"&uacute;"},{0x00FB,"&ucirc;"},` |
|      - | 6197 | `	{0x00FC,"&uuml;"},{0x00FD,"&yacute;"},{0x00FE,"&thorn;"},{0x00FF,"&yuml;"},` |
|      - | 6198 | `	{0x0152,"&OElig;"},{0x0153,"&oelig;"},{0x0160,"&Scaron;"},{0x0161,"&scaron;"},` |
|      - | 6199 | `	{0x0178,"&Yuml;"},{0x0192,"&fnof;"},{0x02C6,"&circ;"},{0x02DC,"&tilde;"},` |
|      - | 6200 | `	{0x0391,"&Alpha;"},{0x0392,"&Beta;"},{0x0393,"&Gamma;"},{0x0394,"&Delta;"},` |
|      - | 6201 | `	{0x0395,"&Epsilon;"},{0x0396,"&Zeta;"},{0x0397,"&Eta;"},{0x0398,"&Theta;"},` |
|      - | 6202 | `	{0x0399,"&Iota;"},{0x039A,"&Kappa;"},{0x039B,"&Lambda;"},{0x039C,"&Mu;"},` |
|      - | 6203 | `	{0x039D,"&Nu;"},{0x039E,"&Xi;"},{0x039F,"&Omicron;"},{0x03A0,"&Pi;"},` |
|      - | 6204 | `	{0x03A1,"&Rho;"},{0x03A3,"&Sigma;"},{0x03A4,"&Tau;"},{0x03A5,"&Upsilon;"},` |
|      - | 6205 | `	{0x03A6,"&Phi;"},{0x03A7,"&Chi;"},{0x03A8,"&Psi;"},{0x03A9,"&Omega;"},` |
|      - | 6206 | `	{0x03B1,"&alpha;"},{0x03B2,"&beta;"},{0x03B3,"&gamma;"},{0x03B4,"&delta;"},` |
|      - | 6207 | `	{0x03B5,"&epsilon;"},{0x03B6,"&zeta;"},{0x03B7,"&eta;"},{0x03B8,"&theta;"},` |
|      - | 6208 | `	{0x03B9,"&iota;"},{0x03BA,"&kappa;"},{0x03BB,"&lambda;"},{0x03BC,"&mu;"},` |
|      - | 6209 | `	{0x03BD,"&nu;"},{0x03BE,"&xi;"},{0x03BF,"&omicron;"},{0x03C0,"&pi;"},` |
|      - | 6210 | `	{0x03C1,"&rho;"},{0x03C2,"&sigmaf;"},{0x03C3,"&sigma;"},{0x03C4,"&tau;"},` |
|      - | 6211 | `	{0x03C5,"&upsilon;"},{0x03C6,"&phi;"},{0x03C7,"&chi;"},{0x03C8,"&psi;"},` |
|      - | 6212 | `	{0x03C9,"&omega;"},{0x03D1,"&thetasym;"},{0x03D2,"&upsih;"},{0x03D6,"&piv;"},` |
|      - | 6213 | `	{0x2002,"&ensp;"},{0x2003,"&emsp;"},{0x2009,"&thinsp;"},{0x200C,"&zwnj;"},` |
|      - | 6214 | `	{0x200D,"&zwj;"},{0x200E,"&lrm;"},{0x200F,"&rlm;"},{0x2013,"&ndash;"},` |
|      - | 6215 | `	{0x2014,"&mdash;"},{0x2018,"&lsquo;"},{0x2019,"&rsquo;"},{0x201A,"&sbquo;"},` |
|      - | 6216 | `	{0x201C,"&ldquo;"},{0x201D,"&rdquo;"},{0x201E,"&bdquo;"},{0x2020,"&dagger;"},` |
|      - | 6217 | `	{0x2021,"&Dagger;"},{0x2022,"&bull;"},{0x2026,"&hellip;"},{0x2030,"&permil;"},` |
|      - | 6218 | `	{0x2032,"&prime;"},{0x2033,"&Prime;"},{0x2039,"&lsaquo;"},{0x203A,"&rsaquo;"},` |
|      - | 6219 | `	{0x203E,"&oline;"},{0x2044,"&frasl;"},{0x20AC,"&euro;"},{0x2111,"&image;"},` |
|      - | 6220 | `	{0x2118,"&weierp;"},{0x211C,"&real;"},{0x2122,"&trade;"},{0x2135,"&alefsym;"},` |
|      - | 6221 | `	{0x2190,"&larr;"},{0x2191,"&uarr;"},{0x2192,"&rarr;"},{0x2193,"&darr;"},` |
|      - | 6222 | `	{0x2194,"&harr;"},{0x21B5,"&crarr;"},{0x21D0,"&lArr;"},{0x21D1,"&uArr;"},` |
|      - | 6223 | `	{0x21D2,"&rArr;"},{0x21D3,"&dArr;"},{0x21D4,"&hArr;"},{0x2200,"&forall;"},` |
|      - | 6224 | `	{0x2202,"&part;"},{0x2203,"&exist;"},{0x2205,"&empty;"},{0x2207,"&nabla;"},` |
|      - | 6225 | `	{0x2208,"&isin;"},{0x2209,"&notin;"},{0x220B,"&ni;"},{0x220F,"&prod;"},` |
|      - | 6226 | `	{0x2211,"&sum;"},{0x2212,"&minus;"},{0x2217,"&lowast;"},{0x221A,"&radic;"},` |
|      - | 6227 | `	{0x221D,"&prop;"},{0x221E,"&infin;"},{0x2220,"&ang;"},{0x2227,"&and;"},` |
|      - | 6228 | `	{0x2228,"&or;"},{0x2229,"&cap;"},{0x222A,"&cup;"},{0x222B,"&int;"},` |
|      - | 6229 | `	{0x2234,"&there4;"},{0x223C,"&sim;"},{0x2245,"&cong;"},{0x2248,"&asymp;"},` |
|      - | 6230 | `	{0x2260,"&ne;"},{0x2261,"&equiv;"},{0x2264,"&le;"},{0x2265,"&ge;"},` |
|      - | 6231 | `	{0x2282,"&sub;"},{0x2283,"&sup;"},{0x2284,"&nsub;"},{0x2286,"&sube;"},` |
|      - | 6232 | `	{0x2287,"&supe;"},{0x2295,"&oplus;"},{0x2297,"&otimes;"},{0x22A5,"&perp;"},` |
|      - | 6233 | `	{0x22C5,"&sdot;"},{0x2308,"&lceil;"},{0x2309,"&rceil;"},{0x230A,"&lfloor;"},` |
|      - | 6234 | `	{0x230B,"&rfloor;"},{0x2329,"&lang;"},{0x232A,"&rang;"},{0x25CA,"&loz;"},` |
|      - | 6235 | `	{0x2660,"&spades;"},{0x2663,"&clubs;"},{0x2665,"&hearts;"},{0x2666,"&diams;"}` |
|      - | 6236 | `};` |
|      - | 6237 | `/* Binary-search aHtml401Ent[] for cp; return its "&name;" entity or 0. */` |
|     41 | 6238 | `static const char *FvHtml401Lookup(sxu32 cp){` |
|     41 | 6239 | `	int lo = 0, hi = (int)SX_ARRAYSIZE(aHtml401Ent) - 1;` |
|    323 | 6240 | `	while( lo <= hi ){` |
|    309 | 6241 | `		int mid = (lo + hi) / 2;` |
|    309 | 6242 | `		sxu32 c = aHtml401Ent[mid].cp;` |
|    309 | 6243 | `		if( c == cp ){ return aHtml401Ent[mid].zEnt; }` |
|    283 | 6244 | `		if( c < cp ){ lo = mid + 1; } else { hi = mid - 1; }` |
|      1 | 6245 | `	}` |
|     15 | 6246 | `	return 0;` |
|     21 | 6247 | `}` |
|      - | 6248 | `/* Decode one strict-UTF-8 sequence at p (< zEnd). On success returns its byte` |
|      - | 6249 | ` * length (1..4) and sets *pCp to the codepoint; on any malformed, overlong,` |
|      - | 6250 | ` * surrogate, truncated or out-of-range (>U+10FFFF) sequence returns 0. Matches` |
|      - | 6251 | ` * PHP's UTF-8 validation used by FULL_SPECIAL_CHARS (verified vs php 8.5.7). */` |
|    101 | 6252 | `static int FvUtf8Next(const unsigned char *p,const unsigned char *zEnd,sxu32 *pCp){` |
|    101 | 6253 | `	unsigned char c = p[0];` |
|    101 | 6254 | `	if( c < 0x80 ){ *pCp = c; return 1; }` |
|    101 | 6255 | `	if( c < 0xC2 ){ return 0; }              /* 0x80-0xBF stray cont / 0xC0-0xC1 overlong */` |
|     99 | 6256 | `	if( c < 0xE0 ){                          /* 2-byte: U+0080..U+07FF */` |
|     47 | 6257 | `		if( zEnd-p < 2 \|\| (p[1]&0xC0)!=0x80 ){ return 0; }` |
|     45 | 6258 | `		*pCp = ((sxu32)(c&0x1F)<<6) \| (p[1]&0x3F);` |
|     45 | 6259 | `		return 2;` |
|      - | 6260 | `	}` |
|     53 | 6261 | `	if( c < 0xF0 ){                          /* 3-byte: U+0800..U+FFFF minus surrogates */` |
|      - | 6262 | `		sxu32 cp;` |
|     47 | 6263 | `		if( zEnd-p < 3 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 ){ return 0; }` |
|     33 | 6264 | `		cp = ((sxu32)(c&0x0F)<<12) \| ((sxu32)(p[1]&0x3F)<<6) \| (p[2]&0x3F);` |
|     33 | 6265 | `		if( cp < 0x800 \|\| (cp>=0xD800 && cp<=0xDFFF) ){ return 0; }` |
|     29 | 6266 | `		*pCp = cp;` |
|     29 | 6267 | `		return 3;` |
|      - | 6268 | `	}` |
|      7 | 6269 | `	if( c < 0xF5 ){                          /* 4-byte: U+10000..U+10FFFF */` |
|      - | 6270 | `		sxu32 cp;` |
|      5 | 6271 | `		if( zEnd-p < 4 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 \|\| (p[3]&0xC0)!=0x80 ){ return 0; }` |
|      5 | 6272 | `		cp = ((sxu32)(c&0x07)<<18) \| ((sxu32)(p[1]&0x3F)<<12) \| ((sxu32)(p[2]&0x3F)<<6) \| (p[3]&0x3F);` |
|      5 | 6273 | `		if( cp < 0x10000 \|\| cp > 0x10FFFF ){ return 0; }` |
|      5 | 6274 | `		*pCp = cp;` |
|      5 | 6275 | `		return 4;` |
|      - | 6276 | `	}` |
|      3 | 6277 | `	return 0;                                /* 0xF5-0xFF */` |
|     51 | 6278 | `}` |
|      - | 6279 | `/* FILTER_SANITIZE_FULL_SPECIAL_CHARS: htmlentities-style, UTF-8-aware. Encodes` |
|      - | 6280 | ` * <>&"' as named entities ("'" -> &#039;; quotes suppressed under NO_ENCODE_QUOTES),` |
|      - | 6281 | ` * and every valid UTF-8 codepoint with an HTML 4.01 named entity as that entity;` |
|      - | 6282 | ` * valid codepoints without a named entity (and low control bytes) pass through` |
|      - | 6283 | ` * verbatim. If the input contains ANY invalid UTF-8 the whole result is "".` |
|      - | 6284 | ` * The STRIP/ENCODE flags do NOT apply to this filter (only NO_ENCODE_QUOTES).` |
|      - | 6285 | ` * php's filter does NOT re-encode valid pre-existing entities ("&amp;" stays,` |
|      - | 6286 | ` * "&bogus;" becomes "&amp;bogus;"), i.e. double_encode=false semantics —` |
|      - | 6287 | ` * exactly htmlentities(ENT_QUOTES\|ENT_HTML401, double_encode: false), so this` |
|      - | 6288 | ` * delegates to the shared encoder. Byte-exact vs php 8.5.7. */` |
|     25 | 6289 | `static void FvSanitizeFull(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 6290 | `	int iEntFlags = (flags & FV_FLAG_NO_ENCODE_QUOTES) ? 0 : PH7_ENT_QUOTES;` |
|     25 | 6291 | `	HtmlEscape(pCtx,z,n,iEntFlags,1/*bAll*/,0/*bDoubleEncode*/);` |
|     25 | 6292 | `}` |
|      - | 6293 | `/* ---------------------------------------------------------------------------` |
|      - | 6294 | ` * UTF-8-aware HTML entity core (htmlspecialchars/htmlentities family).` |
|      - | 6295 | ` * Prototyped next to the five builtins earlier in this file; lives here so it` |
|      - | 6296 | ` * can share aHtml401Ent[]/FvHtml401Lookup()/FvUtf8Next() with the filter_var` |
|      - | 6297 | ` * FULL_SPECIAL_CHARS filter above. Byte-exact vs php 8.5.7 (oracle-swept).` |
|      - | 6298 | ` * ------------------------------------------------------------------------ */` |
|      - | 6299 | `/* Encode cp as UTF-8 into zBuf (>= 4 bytes); return the byte length 1..4.` |
|      - | 6300 | ` * Thin wrapper over the engine-wide SX_WRITE_UTF8 (sxmacros.h). */` |
|    585 | 6301 | `static int HtmlCpUtf8(sxu32 cp,char *zBuf){` |
|    585 | 6302 | `	sxu8 *z = (sxu8 *)zBuf;` |
|    585 | 6303 | `	SX_WRITE_UTF8(z,cp);` |
|    585 | 6304 | `	return (int)(z - (sxu8 *)zBuf);` |
|      1 | 6305 | `}` |
|      - | 6306 | `/* Doctype-allowed codepoint test (php's unicode_cp_is_allowed) — gates what a` |
|      - | 6307 | ` * numeric reference may DECODE to. Oracle-pinned per doctype: HTML401` |
|      - | 6308 | ` * disallows C0 (except TAB/LF/CR) and DEL..U+009F; XML1 and XHTML share the` |
|      - | 6309 | ` * XML rules — DEL..U+009F allowed, U+FFFE/U+FFFF excluded; HTML5 swaps CR` |
|      - | 6310 | ` * for FF (0x0C) and excludes the noncharacters (U+FDD0..U+FDEF and every` |
|      - | 6311 | ` * U+xFFFE/U+xFFFF). Surrogates are disallowed everywhere. */` |
|     91 | 6312 | `static int HtmlCpAllowed(sxu32 cp,int iFlags){` |
|     91 | 6313 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|     91 | 6314 | `	if( cp==0x09 \|\| cp==0x0A ){ return 1; }` |
|     87 | 6315 | `	if( cp==0x0D ){ return iDoc != PH7_ENT_DOC_HTML5; }` |
|     85 | 6316 | `	if( cp==0x0C ){ return iDoc == PH7_ENT_DOC_HTML5; }` |
|     85 | 6317 | `	if( cp < 0x20 \|\| cp > 0x10FFFF ){ return 0; }` |
|     79 | 6318 | `	if( cp>=0xD800 && cp<=0xDFFF ){ return 0; }` |
|     77 | 6319 | `	if( cp>=0x7F && cp<=0x9F ){ return iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML; }` |
|     71 | 6320 | `	if( iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML ){` |
|    ! 0 | 6321 | `		return cp!=0xFFFE && cp!=0xFFFF;` |
|      - | 6322 | `	}` |
|     71 | 6323 | `	if( iDoc == PH7_ENT_DOC_HTML5 ){` |
|      9 | 6324 | `		if( cp>=0xFDD0 && cp<=0xFDEF ){ return 0; }` |
|      9 | 6325 | `		if( (cp & 0xFFFF) >= 0xFFFE ){ return 0; }` |
|      4 | 6326 | `	}` |
|     71 | 6327 | `	return 1;` |
|     46 | 6328 | `}` |
|      - | 6329 | `/* The ENT_DISALLOWED gate for RAW characters on the ENCODE side. Same as the` |
|      - | 6330 | ` * decode gate except CR under HTML5: php's encode-side unicode_cp_is_allowed` |
|      - | 6331 | ` * keeps a literal "\r" verbatim under ENT_HTML5\|ENT_DISALLOWED while the` |
|      - | 6332 | ` * decode side leaves "&#13;" un-decoded (oracle-pinned at flags 176). */` |
|      9 | 6333 | `static int HtmlCpAllowedEncode(sxu32 cp,int iFlags){` |
|      9 | 6334 | `	if( cp==0x0D && (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 ){ return 1; }` |
|      9 | 6335 | `	return HtmlCpAllowed(cp,iFlags);` |
|      5 | 6336 | `}` |
|      - | 6337 | `/* Numeric-reference validity for the double_encode=false "is this already a` |
|      - | 6338 | ` * valid entity" test — a MUCH looser predicate than the decode gate above:` |
|      - | 6339 | ` * any codepoint <= U+10FFFF is valid (controls and surrogates included, every` |
|      - | 6340 | ` * doctype). ENT_DISALLOWED re-tightens non-HTML401 doctypes to the decode` |
|      - | 6341 | ` * gate, except that HTML5 exempts surrogates. All oracle-pinned: &#0; and` |
|      - | 6342 | ` * &#xD800; stay verbatim at flags 11 and 139; flags -1 (HTML5+DISALLOWED)` |
|      - | 6343 | ` * re-encodes &#0; and &#x10FFFF; but still keeps &#xD800;; flags 144` |
|      - | 6344 | ` * (XML1+DISALLOWED) re-encodes &#xD800;. */` |
|      9 | 6345 | `static int HtmlNumericAllowed(sxu32 cp,int iFlags){` |
|      9 | 6346 | `	if( cp > 0x10FFFF ){ return 0; }` |
|      7 | 6347 | `	if( (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML401 ){ return 1; /* never tightened */ }` |
|    ! 0 | 6348 | `	if( (iFlags & PH7_ENT_DISALLOWED)` |
|    ! 0 | 6349 | `	 && !((iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 && cp>=0xD800 && cp<=0xDFFF)` |
|    ! 0 | 6350 | `	 && !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|    ! 0 | 6351 | `	return 1;` |
|      5 | 6352 | `}` |
|      - | 6353 | `/* How many bytes the malformed UTF-8 sequence at p consumes — php's` |
|      - | 6354 | ` * get_next_char failure step (one U+FFFD substitution / one ENT_IGNORE drop` |
|      - | 6355 | ` * per MAXIMAL invalid subpart, not per byte): a prefix-valid sequence eats` |
|      - | 6356 | ` * its continuation bytes ("\xE0\x80\xAF" is ONE unit) while a byte that could` |
|      - | 6357 | ` * start a new sequence is left for the next round. */` |
|      5 | 6358 | `static int HtmlUtf8Trail(unsigned char c){ return c>=0x80 && c<=0xBF; }` |
|     11 | 6359 | `static int HtmlUtf8Lead(unsigned char c){ return c<0x80 \|\| (c>=0xC2 && c<=0xF4); }` |
|     15 | 6360 | `static int HtmlUtf8FailAdvance(const unsigned char *p,const unsigned char *zEnd){` |
|     15 | 6361 | `	unsigned char c = p[0];` |
|     15 | 6362 | `	int nAvail = (int)(zEnd - p);` |
|     15 | 6363 | `	if( c < 0xC2 \|\| c > 0xF4 ){ return 1; } /* stray trail / C0-C1 / F5-FF */` |
|     13 | 6364 | `	if( c < 0xE0 ){` |
|      3 | 6365 | `		if( nAvail < 2 ){ return 1; }` |
|      3 | 6366 | `		return HtmlUtf8Lead(p[1]) ? 1 : 2;` |
|      - | 6367 | `	}` |
|     11 | 6368 | `	if( c < 0xF0 ){` |
|     11 | 6369 | `		if( nAvail >= 3 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) ){` |
|      3 | 6370 | `			return 3; /* complete but overlong/surrogate */` |
|      - | 6371 | `		}` |
|      9 | 6372 | `		if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 6373 | `		if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 6374 | `		return 3;` |
|      - | 6375 | `	}` |
|    ! 0 | 6376 | `	if( nAvail >= 4 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) && HtmlUtf8Trail(p[3]) ){` |
|    ! 0 | 6377 | `		return 4; /* complete but overlong / > U+10FFFF */` |
|      - | 6378 | `	}` |
|    ! 0 | 6379 | `	if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 6380 | `	if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 6381 | `	if( nAvail < 4 \|\| HtmlUtf8Lead(p[3]) ){ return 3; }` |
|    ! 0 | 6382 | `	return 4;` |
|      8 | 6383 | `}` |
|      - | 6384 | `/* The basic special entities, shared by named matching, the hsc_decode` |
|      - | 6385 | ` * numeric whitelist and the translation-table builder so the sets can never` |
|      - | 6386 | ` * drift apart. (&apos; is not an HTML 4.01 entity — doctype-gated below.) */` |
|      - | 6387 | `static const struct { const char *zEnt; int n; sxu32 cp; } aHtmlSpecEnt[] = {` |
|      - | 6388 | `	{"&amp;",5,38},{"&lt;",4,60},{"&gt;",4,62},{"&quot;",6,34},{"&apos;",6,39}` |
|      - | 6389 | `};` |
|      - | 6390 | `/* Does this doctype consult the named-entity table (aHtml401Ent)? XML 1.0 has` |
|      - | 6391 | ` * no named entities beyond the specials; XHTML/HTML5 are approximated by the` |
|      - | 6392 | ` * HTML 4.01 table (documented divergence). */` |
|     63 | 6393 | `static int HtmlDocHasNamedTable(int iDoc){` |
|     63 | 6394 | `	return iDoc != PH7_ENT_DOC_XML1;` |
|      1 | 6395 | `}` |
|      - | 6396 | `/* The single-quote entity per doctype. Oracle-pinned asymmetry: for every` |
|      - | 6397 | ` * non-HTML401 doctype htmlspecialchars emits &apos; while htmlentities` |
|      - | 6398 | ` * (bEntities) keeps &#039; under XHTML too. The translation table mirrors` |
|      - | 6399 | ` * whichever function the requested table belongs to. */` |
|     29 | 6400 | `static const char *HtmlAposEntity(int iDoc,int bEntities){` |
|     29 | 6401 | `	if( iDoc == PH7_ENT_DOC_HTML401 \|\| (bEntities && iDoc == PH7_ENT_DOC_XHTML) ){` |
|     21 | 6402 | `		return "&#039;";` |
|      - | 6403 | `	}` |
|      9 | 6404 | `	return "&apos;";` |
|     15 | 6405 | `}` |
|      - | 6406 | `/* Try to parse one HTML entity at z (z[0]=='&', z < zEnd). bFull selects the` |
|      - | 6407 | ` * html_entity_decode set (doctype named table + any allowed numeric ref) vs` |
|      - | 6408 | ` * the htmlspecialchars_decode set (the basic specials + quote numerics only).` |
|      - | 6409 | ` * Named matching is case-SENSITIVE and the ';' is required (both PHP-exact);` |
|      - | 6410 | ` * numeric refs accept dec/hex (x or X) with any number of leading zeros but` |
|      - | 6411 | ` * reject out-of-range, surrogate and doctype-disallowed codepoints (the` |
|      - | 6412 | ` * caller then leaves the source verbatim). Quote-flag gating is NOT applied` |
|      - | 6413 | ` * here — the same routine doubles as the "is this a valid entity" test for` |
|      - | 6414 | ` * double_encode=false, which ignores the quote bits (oracle-pinned).` |
|      - | 6415 | ` * bEncodeCheck selects the looser HtmlNumericAllowed predicate used by that` |
|      - | 6416 | ` * double_encode test; decode callers pass 0 for the HtmlCpAllowed gate.` |
|      - | 6417 | ` * On success sets *pCp / *pnConsumed and returns 1. */` |
|    172 | 6418 | `static int HtmlParseEntity(const unsigned char *z,const unsigned char *zEnd,` |
|      1 | 6419 | `                           int iFlags,int bFull,int bEncodeCheck,sxu32 *pCp,int *pnConsumed){` |
|    173 | 6420 | `	int nAvail = (int)(zEnd - z);` |
|    173 | 6421 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6422 | `	sxu32 n;` |
|    173 | 6423 | `	if( nAvail < 4 ){ return 0; } /* shortest entities: &lt; &#9; */` |
|    169 | 6424 | `	if( z[1] == '#' ){` |
|      - | 6425 | `		/* Numeric reference */` |
|     89 | 6426 | `		sxu32 cp = 0;` |
|     89 | 6427 | `		int i = 2, bHex = 0, nDig = 0;` |
|     89 | 6428 | `		if( z[i]=='x' \|\| z[i]=='X' ){ bHex = 1; i++; }` |
|    317 | 6429 | `		for( ; i < nAvail && z[i] != ';' ; i++ ){` |
|      - | 6430 | `			int v;` |
|    221 | 6431 | `			unsigned char c = z[i];` |
|    221 | 6432 | `			if( c>='0' && c<='9' ){ v = c - '0'; }` |
|     17 | 6433 | `			else if( bHex && c>='a' && c<='f' ){ v = c - 'a' + 10; }` |
|     17 | 6434 | `			else if( bHex && c>='A' && c<='F' ){ v = c - 'A' + 10; }` |
|    ! 0 | 6435 | `			else { return 0; }` |
|      - | 6436 | `			/* Stop accumulating once out of range (keeps validating the shape;` |
|      - | 6437 | `			 * max intermediate is 0x10FFFF*16+15, no sxu32 overflow). */` |
|    221 | 6438 | `			if( cp <= 0x10FFFF ){ cp = cp * (bHex ? 16 : 10) + (sxu32)v; }` |
|    221 | 6439 | `			nDig++;` |
|    111 | 6440 | `		}` |
|     97 | 6441 | `		if( nDig == 0 \|\| i >= nAvail ){ return 0; } /* no digits / no ';' */` |
|     97 | 6442 | `		if( bEncodeCheck ? !HtmlNumericAllowed(cp,iFlags) : !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|     83 | 6443 | `		if( !bFull ){` |
|      - | 6444 | `			/* hsc_decode: numeric refs to the five specials only. */` |
|     99 | 6445 | `			for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) && aHtmlSpecEnt[n].cp != cp ; n++ ){}` |
|     25 | 6446 | `			if( n >= SX_ARRAYSIZE(aHtmlSpecEnt) ){ return 0; }` |
|     11 | 6447 | `		}` |
|     75 | 6448 | `		*pCp = cp;` |
|     75 | 6449 | `		*pnConsumed = i + 1;` |
|     75 | 6450 | `		return 1;` |
|      - | 6451 | `	}` |
|      - | 6452 | `	/* Named reference — every entity name starts with a letter, so anything` |
|      - | 6453 | `	 * else can bail out before touching the tables. */` |
|     81 | 6454 | `	if( !((z[1]>='a' && z[1]<='z') \|\| (z[1]>='A' && z[1]<='Z')) ){ return 0; }` |
|    287 | 6455 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) ; n++ ){` |
|    265 | 6456 | `		if( aHtmlSpecEnt[n].cp == 39 && iDoc == PH7_ENT_DOC_HTML401 ){ continue; }` |
|    243 | 6457 | `		if( nAvail >= aHtmlSpecEnt[n].n && SyMemcmp(z,aHtmlSpecEnt[n].zEnt,(sxu32)aHtmlSpecEnt[n].n) == 0 ){` |
|     53 | 6458 | `			*pCp = aHtmlSpecEnt[n].cp;` |
|     53 | 6459 | `			*pnConsumed = aHtmlSpecEnt[n].n;` |
|     53 | 6460 | `			return 1;` |
|      - | 6461 | `		}` |
|     96 | 6462 | `	}` |
|     23 | 6463 | `	if( bFull && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 6464 | `		/* Linear scan of the 248-row table: runs only at '&'-then-letter` |
|      - | 6465 | `		 * positions and guarantees the decode set can never drift from the` |
|      - | 6466 | `		 * encode table. The first-letter guard skips the SyStrlen/SyMemcmp` |
|      - | 6467 | `		 * for ~96% of rows. */` |
|   3369 | 6468 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|      - | 6469 | `			sxu32 nEnt;` |
|   3357 | 6470 | `			if( z[1] != (unsigned char)aHtml401Ent[n].zEnt[1] ){ continue; }` |
|    121 | 6471 | `			nEnt = SyStrlen(aHtml401Ent[n].zEnt);` |
|    121 | 6472 | `			if( (sxu32)nAvail >= nEnt && SyMemcmp(z,aHtml401Ent[n].zEnt,nEnt) == 0 ){` |
|      7 | 6473 | `				*pCp = aHtml401Ent[n].cp;` |
|      7 | 6474 | `				*pnConsumed = (int)nEnt;` |
|      7 | 6475 | `				return 1;` |
|      - | 6476 | `			}` |
|     58 | 6477 | `		}` |
|      6 | 6478 | `	}` |
|     17 | 6479 | `	return 0;` |
|     88 | 6480 | `}` |
|      - | 6481 | `/* Shared encoder for htmlspecialchars (bAll=0) and htmlentities (bAll=1).` |
|      - | 6482 | ` * Invalid UTF-8 policy: ENT_IGNORE drops the byte (and wins over SUBSTITUTE),` |
|      - | 6483 | ` * ENT_SUBSTITUTE emits one U+FFFD per invalid byte, neither -> the whole` |
|      - | 6484 | ` * result is "" (pre-validated in a first pass: the accumulating result API` |
|      - | 6485 | ` * cannot roll back — same reason FvSanitizeFull is two-pass). */` |
|     96 | 6486 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 6487 | `                       int iFlags,int bAll,int bDoubleEncode){` |
|     97 | 6488 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     97 | 6489 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|      - | 6490 | `	const unsigned char *runStart;` |
|     97 | 6491 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6492 | `	sxu32 cp;` |
|     97 | 6493 | `	if( (iFlags & (PH7_ENT_IGNORE\|PH7_ENT_SUBSTITUTE)) == 0 ){` |
|      - | 6494 | `		/* Pass 1: any malformed sequence rejects the entire input. ASCII` |
|      - | 6495 | `		 * bytes cannot be malformed, so skip them without the decoder. */` |
|    381 | 6496 | `		while( p < zEnd ){` |
|      - | 6497 | `			int len;` |
|    323 | 6498 | `			if( *p < 0x80 ){ p++; continue; }` |
|     37 | 6499 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     37 | 6500 | `			if( len == 0 ){ ph7_result_string(pCtx,"",0); return; }` |
|     27 | 6501 | `			p += len;` |
|      1 | 6502 | `		}` |
|     59 | 6503 | `		p = (const unsigned char *)zIn;` |
|     29 | 6504 | `	}` |
|     87 | 6505 | `	runStart = p;` |
|     87 | 6506 | `	ph7_result_string(pCtx,"",0);` |
|    463 | 6507 | `	while( p < zEnd ){` |
|    377 | 6508 | `		const char *zEnt = 0;` |
|      - | 6509 | `		int len;` |
|    377 | 6510 | `		if( *p < 0x80 ){` |
|    313 | 6511 | `			len = 1;` |
|    313 | 6512 | `			switch( *p ){` |
|     25 | 6513 | `			case '<': zEnt = "&lt;"; break;` |
|     25 | 6514 | `			case '>': zEnt = "&gt;"; break;` |
|     18 | 6515 | `			case '&':` |
|     37 | 6516 | `				zEnt = "&amp;";` |
|     37 | 6517 | `				if( !bDoubleEncode ){` |
|      - | 6518 | `					sxu32 eCp; int nEat;` |
|     25 | 6519 | `					if( HtmlParseEntity(p,zEnd,iFlags,1,1,&eCp,&nEat) ){` |
|      - | 6520 | `						/* A valid existing entity: keep it verbatim. */` |
|     13 | 6521 | `						zEnt = 0;` |
|     13 | 6522 | `						len = nEat;` |
|      6 | 6523 | `					}` |
|     12 | 6524 | `				}` |
|     37 | 6525 | `				break;` |
|     10 | 6526 | `			case '"':` |
|     21 | 6527 | `				if( iFlags & PH7_ENT_QUOTE_DOUBLE ){ zEnt = "&quot;"; }` |
|     21 | 6528 | `				break;` |
|     12 | 6529 | `			case '\'':` |
|     25 | 6530 | `				if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|     23 | 6531 | `					zEnt = HtmlAposEntity(iDoc,bAll);` |
|     11 | 6532 | `				}` |
|     25 | 6533 | `				break;` |
|     92 | 6534 | `			default:` |
|    185 | 6535 | `				if( (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode((sxu32)*p,iFlags) ){` |
|    ! 0 | 6536 | `					zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 6537 | `				}` |
|    184 | 6538 | `				break;` |
|      - | 6539 | `			}` |
|    157 | 6540 | `		}else{` |
|     65 | 6541 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     65 | 6542 | `			if( len == 0 ){` |
|      - | 6543 | `				/* Malformed subpart (IGNORE or SUBSTITUTE is set, else pass 1` |
|      - | 6544 | `				 * would have rejected): drop it or emit ONE U+FFFD for the` |
|      - | 6545 | `				 * whole unit (php substitutes per maximal invalid subpart). */` |
|     15 | 6546 | `				if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|     15 | 6547 | `				if( (iFlags & PH7_ENT_IGNORE) == 0 ){ ph7_result_string(pCtx,"\xEF\xBF\xBD",3); }` |
|     15 | 6548 | `				p += HtmlUtf8FailAdvance(p,zEnd);` |
|     15 | 6549 | `				runStart = p;` |
|     15 | 6550 | `				continue;` |
|      - | 6551 | `			}` |
|     51 | 6552 | `			if( bAll && HtmlDocHasNamedTable(iDoc) ){` |
|     41 | 6553 | `				zEnt = FvHtml401Lookup(cp);` |
|     20 | 6554 | `			}` |
|     51 | 6555 | `			if( zEnt == 0 && (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode(cp,iFlags) ){` |
|    ! 0 | 6556 | `				zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 6557 | `			}` |
|      - | 6558 | `		}` |
|    363 | 6559 | `		if( zEnt ){` |
|    135 | 6560 | `			if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|    135 | 6561 | `			ph7_result_string(pCtx,zEnt,-1);` |
|    135 | 6562 | `			runStart = p + len;` |
|     67 | 6563 | `		}` |
|    363 | 6564 | `		p += len;` |
|      1 | 6565 | `	}` |
|     87 | 6566 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     49 | 6567 | `}` |
|      - | 6568 | `/* Shared decoder for html_entity_decode (bFull=1) and htmlspecialchars_decode` |
|      - | 6569 | ` * (bFull=0). Quote refs (cp 34/39, named or numeric) are gated by the quote` |
|      - | 6570 | ` * bits and left verbatim when suppressed; an invalid entity leaves its '&'` |
|      - | 6571 | ` * verbatim and rescans right after it, which also yields PHP's no-double-` |
|      - | 6572 | ` * decode behavior ("&amp;lt;" -> "&lt;"). */` |
|     84 | 6573 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 6574 | `                         int iFlags,int bFull){` |
|     85 | 6575 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     85 | 6576 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|     85 | 6577 | `	const unsigned char *runStart = p;` |
|     85 | 6578 | `	ph7_result_string(pCtx,"",0);` |
|    565 | 6579 | `	while( p < zEnd ){` |
|      - | 6580 | `		sxu32 cp;` |
|      - | 6581 | `		int nEat;` |
|    516 | 6582 | `		if( *p != '&' ){ p++; continue; }` |
|    155 | 6583 | `		if( !HtmlParseEntity(p,zEnd,iFlags,bFull,0,&cp,&nEat) ){ p++; continue; }` |
|    124 | 6584 | `		if( (cp == 34 && (iFlags & PH7_ENT_QUOTE_DOUBLE) == 0)` |
|    117 | 6585 | `		 \|\| (cp == 39 && (iFlags & PH7_ENT_QUOTE_SINGLE) == 0) ){` |
|      - | 6586 | `			/* Suppressed quote: leave the entity source verbatim. */` |
|     37 | 6587 | `			p += nEat;` |
|     37 | 6588 | `			continue;` |
|      - | 6589 | `		}` |
|     89 | 6590 | `		if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|      - | 6591 | `		{` |
|      - | 6592 | `			char zBuf[4];` |
|     89 | 6593 | `			int n = HtmlCpUtf8(cp,zBuf);` |
|     89 | 6594 | `			ph7_result_string(pCtx,zBuf,n);` |
|      - | 6595 | `		}` |
|     89 | 6596 | `		p += nEat;` |
|     89 | 6597 | `		runStart = p;` |
|      1 | 6598 | `	}` |
|     81 | 6599 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     81 | 6600 | `}` |
|      - | 6601 | `/* Validate the optional charset argument at apArg[idx]: UTF-8 aliases (and` |
|      - | 6602 | ` * ""/NULL meaning the default) are accepted; anything else — including` |
|      - | 6603 | ` * php-supported single-byte charsets like ISO-8859-1, PHL is UTF-8-only by` |
|      - | 6604 | ` * policy — raises PHP's unsupported-charset warning and is treated as` |
|      - | 6605 | ` * UTF-8 (ph7_context_throw_error_format prepends the function name). */` |
|    143 | 6606 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx){` |
|      - | 6607 | `	const char *zCs;` |
|      - | 6608 | `	int nCs;` |
|    150 | 6609 | `	if( nArg <= idx \|\| ph7_value_is_null(apArg[idx]) ){ return; }` |
|     15 | 6610 | `	zCs = ph7_value_to_string(apArg[idx],&nCs);` |
|     15 | 6611 | `	if( nCs == 0 ){ return; } /* "" selects the default charset (UTF-8) */` |
|     13 | 6612 | `	if( nCs == 5 && SyStrnicmp(zCs,"UTF-8",5) == 0 ){` |
|     13 | 6613 | `		return; /* php accepts only "UTF-8" (any case) silently — "UTF8" warns */` |
|      - | 6614 | `	}` |
|    ! 0 | 6615 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 6616 | `		"Charset \"%.*s\" is not supported, assuming UTF-8",nCs,zCs);` |
|     72 | 6617 | `}` |
|      - | 6618 | `/* get_html_translation_table() worker: character (UTF-8 bytes) => entity.` |
|      - | 6619 | ` * The five specials come first in byte order, then — for HTML_ENTITIES with a` |
|      - | 6620 | ` * named-table doctype — the 248 aHtml401Ent rows ascending (oracle-pinned` |
|      - | 6621 | ` * ordering; 253 entries under the defaults). */` |
|    549 | 6622 | `static void HtmlTableAdd(ph7_value *pArray,ph7_value *pValue,const char *zKey,const char *zEnt){` |
|    549 | 6623 | `	ph7_value_string(pValue,zEnt,-1);` |
|    549 | 6624 | `	ph7_array_add_strkey_elem(pArray,zKey,pValue);` |
|    549 | 6625 | `	ph7_value_reset_string_cursor(pValue);` |
|    549 | 6626 | `}` |
|     13 | 6627 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags){` |
|      - | 6628 | `	ph7_value *pArray,*pValue;` |
|     13 | 6629 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6630 | `	sxu32 n;` |
|     13 | 6631 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 6632 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 6633 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|    ! 0 | 6634 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6635 | `		return;` |
|      - | 6636 | `	}` |
|     13 | 6637 | `	if( iFlags & PH7_ENT_QUOTE_DOUBLE ){` |
|     11 | 6638 | `		HtmlTableAdd(pArray,pValue,"\"","&quot;");` |
|      5 | 6639 | `	}` |
|     13 | 6640 | `	HtmlTableAdd(pArray,pValue,"&","&amp;");` |
|     13 | 6641 | `	if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|      - | 6642 | `		/* The apostrophe row mirrors the function each table belongs to:` |
|      - | 6643 | `		 * SPECIALCHARS follows htmlspecialchars, ENTITIES follows` |
|      - | 6644 | `		 * htmlentities (oracle-pinned at flags 35). */` |
|      7 | 6645 | `		HtmlTableAdd(pArray,pValue,"'",HtmlAposEntity(iDoc,iTable != 0));` |
|      3 | 6646 | `	}` |
|     13 | 6647 | `	HtmlTableAdd(pArray,pValue,"<","&lt;");` |
|     13 | 6648 | `	HtmlTableAdd(pArray,pValue,">","&gt;");` |
|     13 | 6649 | `	if( iTable != 0 /*php: any non-HTML_SPECIALCHARS table => entities*/ && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 6650 | `		char zKey[8];` |
|    499 | 6651 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|    497 | 6652 | `			int nK = HtmlCpUtf8(aHtml401Ent[n].cp,zKey);` |
|    497 | 6653 | `			zKey[nK] = 0;` |
|    497 | 6654 | `			HtmlTableAdd(pArray,pValue,zKey,aHtml401Ent[n].zEnt);` |
|    249 | 6655 | `		}` |
|      1 | 6656 | `	}` |
|     13 | 6657 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 6658 | `}` |
|     25 | 6659 | `static int FvEmailAllowed(unsigned char c){` |
|     25 | 6660 | `	if( (c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9') ){ return 1; }` |
|     16 | 6661 | `	return c=='!'\|\|c=='#'\|\|c=='$'\|\|c=='%'\|\|c=='&'\|\|c=='\''\|\|c=='*'\|\|c=='+'` |
|     10 | 6662 | ``	    \|\| c=='-'\|\|c=='='\|\|c=='?'\|\|c=='^'\|\|c=='_'\|\|c=='`'\|\|c=='{'\|\|c=='\|'`` |
|     15 | 6663 | `	    \|\| c=='}'\|\|c=='~'\|\|c=='@'\|\|c=='.'\|\|c=='['\|\|c==']';` |
|     13 | 6664 | `}` |
|     23 | 6665 | `static int FvUrlAllowed(unsigned char c){` |
|     23 | 6666 | `	return c>=33 && c<=126; /* PHP keeps every printable ASCII byte except space */` |
|      1 | 6667 | `}` |
|      - | 6668 | `/* SANITIZE_EMAIL (isUrl=0) / SANITIZE_URL (isUrl=1): strip disallowed bytes. */` |
|      5 | 6669 | `static void FvSanitizeChars(ph7_context *pCtx,const char *z,int n,int isUrl){` |
|      5 | 6670 | `	int i, runStart = 0;` |
|      5 | 6671 | `	ph7_result_string(pCtx,"",0);` |
|     51 | 6672 | `	for( i=0; i<n; i++ ){` |
|     47 | 6673 | `		unsigned char c = (unsigned char)z[i];` |
|     47 | 6674 | `		if( !(isUrl ? FvUrlAllowed(c) : FvEmailAllowed(c)) ){` |
|     11 | 6675 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     11 | 6676 | `			runStart = i+1;` |
|      5 | 6677 | `		}` |
|     24 | 6678 | `	}` |
|      5 | 6679 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      5 | 6680 | `}` |
|      - | 6681 | `/*` |
|      - | 6682 | ` * Apply the selected filter to one already-resolved input value and write the` |
|      - | 6683 | ` * result into pCtx. Shared by filter_var() and filter_input(): the caller has` |
|      - | 6684 | ` * already parsed $filter/$flags/$options. On validation failure the 'default'` |
|      - | 6685 | ` * option (if any) is returned, else null when FILTER_NULL_ON_FAILURE is set,` |
|      - | 6686 | ` * else false. A validating filter that passes returns the (string) input` |
|      - | 6687 | ` * unchanged; a sanitizer writes its transformed output directly.` |
|      - | 6688 | ` */` |
|    316 | 6689 | `static int FvApplyFilter(ph7_context *pCtx,ph7_value *pInput,` |
|      - | 6690 | `                         int iFilter,int iFlags,ph7_value *pOpts,` |
|      - | 6691 | `                         ph7_value *pDefault)` |
|      3 | 6692 | `{` |
|    319 | 6693 | `	int bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|      - | 6694 | `	const char *zVal; int nVal;` |
|      - | 6695 | `	/* An array/object input fails every scalar filter. */` |
|    319 | 6696 | `	if( ph7_value_is_array(pInput) ){ goto fail; }` |
|    317 | 6697 | `	zVal = ph7_value_to_string(pInput,&nVal);` |
|    317 | 6698 | `	switch( iFilter ){` |
|     28 | 6699 | `	case FV_VALIDATE_INT: {` |
|      - | 6700 | `		ph7_int64 v;` |
|     58 | 6701 | `		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }` |
|     31 | 6702 | `		if( pOpts ){` |
|      7 | 6703 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|      7 | 6704 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|      7 | 6705 | `			if( pMin && v<ph7_value_to_int64(pMin) ){ goto fail; }` |
|      7 | 6706 | `			if( pMax && v>ph7_value_to_int64(pMax) ){ goto fail; }` |
|      2 | 6707 | `		}` |
|     29 | 6708 | `		ph7_result_int64(pCtx,v);` |
|     29 | 6709 | `		return PH7_OK;` |
|      - | 6710 | `	}` |
|     34 | 6711 | `	case FV_VALIDATE_FLOAT: {` |
|      - | 6712 | `		double d;` |
|     69 | 6713 | `		if( !FvValidateFloat(zVal,nVal,iFlags,&d) ){ goto fail; }` |
|     39 | 6714 | `		ph7_result_double(pCtx,d);` |
|     39 | 6715 | `		return PH7_OK;` |
|      - | 6716 | `	}` |
|     14 | 6717 | `	case FV_VALIDATE_BOOLEAN: {` |
|      - | 6718 | `		int b;` |
|     29 | 6719 | `		if( !FvValidateBool(zVal,nVal,&b) ){ goto fail; }` |
|     21 | 6720 | `		ph7_result_bool(pCtx,b);` |
|     21 | 6721 | `		return PH7_OK;` |
|      - | 6722 | `	}` |
|     25 | 6723 | `	case FV_VALIDATE_IP:     if( !FvValidateIp(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|     11 | 6724 | `	case FV_VALIDATE_MAC:    if( !FvValidateMac(zVal,nVal) ){ goto fail; }       goto pass;` |
|     28 | 6725 | `	case FV_VALIDATE_EMAIL:  if( !FvValidateEmail(zVal,nVal) ){ goto fail; }     goto pass;` |
|     11 | 6726 | `	case FV_VALIDATE_DOMAIN: if( !FvValidateDomain(zVal,nVal) ){ goto fail; }    goto pass;` |
|     15 | 6727 | `	case FV_VALIDATE_URL:    if( !FvValidateUrl(zVal,nVal) ){ goto fail; }       goto pass;` |
|      3 | 6728 | `	case FV_VALIDATE_REGEXP: {` |
|      - | 6729 | `#ifdef PH7_ENABLE_PCRE` |
|      8 | 6730 | `		ph7_value *pRe = pOpts ? ph7_array_fetch(pOpts,"regexp",(int)sizeof("regexp")-1) : 0;` |
|      8 | 6731 | `		const char *zRe; int nRe, matched = 0;` |
|      8 | 6732 | `		if( pRe==0 ){` |
|      3 | 6733 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6734 | `				"filter_var(): \"regexp\" option is missing");` |
|      - | 6735 | `		}` |
|      5 | 6736 | `		zRe = ph7_value_to_string(pRe,&nRe);` |
|      5 | 6737 | `		if( PH7_PcreMatchQuiet(pCtx,zRe,nRe,zVal,nVal,&matched)!=SXRET_OK \|\| !matched ){ goto fail; }` |
|      3 | 6738 | `		goto pass;` |
|      - | 6739 | `#else` |
|      - | 6740 | `		goto fail;` |
|      - | 6741 | `#endif` |
|      - | 6742 | `	}` |
|      3 | 6743 | `	case FV_SANITIZE_NUMBER_INT:   FvSanitizeNumber(pCtx,zVal,nVal,0,0);      return PH7_OK;` |
|      5 | 6744 | `	case FV_SANITIZE_NUMBER_FLOAT: FvSanitizeNumber(pCtx,zVal,nVal,1,iFlags); return PH7_OK;` |
|     13 | 6745 | `	case FV_SANITIZE_SPECIAL_CHARS:      FvSanitizeSpecial(pCtx,zVal,nVal,iFlags); return PH7_OK;` |
|     25 | 6746 | `	case FV_SANITIZE_FULL_SPECIAL_CHARS: FvSanitizeFull(pCtx,zVal,nVal,iFlags);    return PH7_OK;` |
|      3 | 6747 | `	case FV_SANITIZE_EMAIL: FvSanitizeChars(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 6748 | `	case FV_SANITIZE_URL:   FvSanitizeChars(pCtx,zVal,nVal,1); return PH7_OK;` |
|     13 | 6749 | `	case FV_DEFAULT:` |
|      - | 6750 | `		/* FILTER_UNSAFE_RAW / FILTER_DEFAULT: pass through unchanged unless a` |
|      - | 6751 | `		 * STRIP/ENCODE flag is set, in which case apply the string filter. */` |
|     28 | 6752 | `		if( iFlags & FV_FLAG_STRING_MASK ){` |
|     15 | 6753 | `			FvSanitizeString(pCtx,zVal,nVal,iFlags);` |
|     15 | 6754 | `			return PH7_OK;` |
|      - | 6755 | `		}` |
|     14 | 6756 | `		goto pass;` |
|    ! 0 | 6757 | `	default:` |
|    ! 0 | 6758 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 6759 | `			"Unknown filter with ID %d",iFilter);` |
|    ! 0 | 6760 | `		break; /* unknown filter id -> fail */` |
|    ! 0 | 6761 | `	}` |
|     58 | 6762 | `fail:` |
|    118 | 6763 | `	if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|    114 | 6764 | `	else if( bNull ){ ph7_result_null(pCtx); }` |
|    108 | 6765 | `	else { ph7_result_bool(pCtx,0); }` |
|    118 | 6766 | `	return PH7_OK;` |
|     26 | 6767 | `pass: /* validation passed: return the (string) input unchanged */` |
|     54 | 6768 | `	ph7_result_string(pCtx,zVal,nVal);` |
|     54 | 6769 | `	return PH7_OK;` |
|    161 | 6770 | `}` |
|      - | 6771 | `/*` |
|      - | 6772 | ` * Parse the ($filter, $options) pair shared by filter_var()/filter_input() out` |
|      - | 6773 | ` * of apArg[iBase] ($filter) and apArg[iBase+1] ($options): $options is either a` |
|      - | 6774 | ` * plain flags int, or an array with 'flags' and an 'options' sub-array (whose` |
|      - | 6775 | ` * 'default' entry is the fallback value). Fills the four output pointers;` |
|      - | 6776 | ` * unset outputs keep the caller-provided defaults.` |
|      - | 6777 | ` */` |
|    328 | 6778 | `static void FvParseFilterArgs(int nArg,ph7_value **apArg,int iBase,` |
|      - | 6779 | `                              int *piFilter,int *piFlags,` |
|      - | 6780 | `                              ph7_value **ppOpts,ph7_value **ppDefault)` |
|      3 | 6781 | `{` |
|    331 | 6782 | `	if( nArg>iBase ){ *piFilter = ph7_value_to_int(apArg[iBase]); }` |
|    331 | 6783 | `	if( nArg>iBase+1 ){` |
|     88 | 6784 | `		if( ph7_value_is_array(apArg[iBase+1]) ){` |
|     42 | 6785 | `			ph7_value *pF = ph7_array_fetch(apArg[iBase+1],"flags",(int)sizeof("flags")-1);` |
|     42 | 6786 | `			if( pF ){ *piFlags = ph7_value_to_int(pF); }` |
|     42 | 6787 | `			*ppOpts = ph7_array_fetch(apArg[iBase+1],"options",(int)sizeof("options")-1);` |
|     42 | 6788 | `			if( *ppOpts && !ph7_value_is_array(*ppOpts) ){ *ppOpts = 0; }` |
|     42 | 6789 | `			if( *ppOpts ){ *ppDefault = ph7_array_fetch(*ppOpts,"default",(int)sizeof("default")-1); }` |
|     22 | 6790 | `		}else{` |
|     48 | 6791 | `			*piFlags = ph7_value_to_int(apArg[iBase+1]);` |
|      - | 6792 | `		}` |
|     43 | 6793 | `	}` |
|    331 | 6794 | `}` |
|      - | 6795 | `/*` |
|      - | 6796 | ` * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 6797 | ` *  Validate or sanitize a value; see FvApplyFilter for the failure semantics.` |
|      - | 6798 | ` */` |
|    306 | 6799 | `static int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6800 | `{` |
|    308 | 6801 | `	int iFilter = FV_DEFAULT, iFlags = 0;` |
|    308 | 6802 | `	ph7_value *pOpts = 0, *pDefault = 0;` |
|    308 | 6803 | `	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    308 | 6804 | `	FvParseFilterArgs(nArg,apArg,1,&iFilter,&iFlags,&pOpts,&pDefault);` |
|    308 | 6805 | `	return FvApplyFilter(pCtx,apArg[0],iFilter,iFlags,pOpts,pDefault);` |
|    155 | 6806 | `}` |
|      - | 6807 | `/*` |
|      - | 6808 | ` * filter_input($type, $var_name, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 6809 | ` *  Look up $var_name in the requested INPUT_* superglobal, then apply the` |
|      - | 6810 | ` *  filter. Semantics verified byte-for-byte against php 8.5:` |
|      - | 6811 | ` *   - variable NOT set: 'default' option wins, else false when` |
|      - | 6812 | ` *     FILTER_NULL_ON_FAILURE is set, else null. (Note the null/false roles are` |
|      - | 6813 | ` *     INVERTED relative to a present value that fails validation, which yields` |
|      - | 6814 | ` *     default > null-if-NULL_ON_FAILURE > false via FvApplyFilter.)` |
|      - | 6815 | ` *   - variable present: delegate to FvApplyFilter.` |
|      - | 6816 | ` *  Divergence: php reads a SAPI snapshot of the original request variables` |
|      - | 6817 | ` *  captured at startup; PHL reads the live superglobal. In CLI they match for` |
|      - | 6818 | ` *  the SAPI-registered keys (SCRIPT_NAME/PHP_SELF/DOCUMENT_ROOT); keys added` |
|      - | 6819 | ` *  only to the live $_SERVER (REQUEST_TIME/PWD/…) are visible here but not in` |
|      - | 6820 | ` *  php's snapshot.` |
|      - | 6821 | ` */` |
|     24 | 6822 | `static int PH7_builtin_filter_input(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6823 | `{` |
|     26 | 6824 | `	int iType, iFilter = FV_DEFAULT, iFlags = 0;` |
|     26 | 6825 | `	ph7_value *pOpts = 0, *pDefault = 0, *pSuper, *pElem;` |
|      - | 6826 | `	const char *zVar, *zSuper; int nVar; sxu32 nSuper;` |
|     26 | 6827 | `	if( nArg<2 ){` |
|    ! 0 | 6828 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 6829 | `			"filter_input() expects at least 2 arguments, %d given",nArg);` |
|      - | 6830 | `	}` |
|     26 | 6831 | `	iType = ph7_value_to_int(apArg[0]);` |
|     26 | 6832 | `	switch( iType ){` |
|      3 | 6833 | `	case 0: zSuper = "_POST";   nSuper = (sxu32)sizeof("_POST")-1;   break; /* INPUT_POST */` |
|      3 | 6834 | `	case 1: zSuper = "_GET";    nSuper = (sxu32)sizeof("_GET")-1;    break; /* INPUT_GET */` |
|    ! 0 | 6835 | `	case 2: zSuper = "_COOKIE"; nSuper = (sxu32)sizeof("_COOKIE")-1; break; /* INPUT_COOKIE */` |
|    ! 0 | 6836 | `	case 4: zSuper = "_ENV";    nSuper = (sxu32)sizeof("_ENV")-1;    break; /* INPUT_ENV */` |
|     19 | 6837 | `	case 5: zSuper = "_SERVER"; nSuper = (sxu32)sizeof("_SERVER")-1; break; /* INPUT_SERVER */` |
|      1 | 6838 | `	default:` |
|      3 | 6839 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6840 | `			"filter_input(): Argument #1 ($type) must be an INPUT_* constant");` |
|      - | 6841 | `	}` |
|     23 | 6842 | `	zVar = ph7_value_to_string(apArg[1],&nVar);` |
|     23 | 6843 | `	FvParseFilterArgs(nArg,apArg,2,&iFilter,&iFlags,&pOpts,&pDefault);` |
|      - | 6844 | `	/* Resolve the variable from the superglobal (missing/non-array -> not set). */` |
|     23 | 6845 | `	pSuper = PH7_VmExtractSuper(pCtx->pVm,zSuper,nSuper);` |
|     23 | 6846 | `	pElem = (pSuper && ph7_value_is_array(pSuper))` |
|     33 | 6847 | `		? ph7_array_fetch(pSuper,zVar,nVar) : 0;` |
|     23 | 6848 | `	if( pElem==0 ){` |
|      - | 6849 | `		/* Variable not set: default > false(if NULL_ON_FAILURE) > null. Note the` |
|      - | 6850 | `		 * false/null roles are inverted vs FvApplyFilter's present-but-fails path. */` |
|     13 | 6851 | `		if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|      9 | 6852 | `		else if( iFlags & FV_NULL_ON_FAILURE ){ ph7_result_bool(pCtx,0); }` |
|      7 | 6853 | `		else { ph7_result_null(pCtx); }` |
|     13 | 6854 | `		return PH7_OK;` |
|      - | 6855 | `	}` |
|     11 | 6856 | `	return FvApplyFilter(pCtx,pElem,iFilter,iFlags,pOpts,pDefault);` |
|     14 | 6857 | `}` |
|      - | 6858 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6859 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6860 | `/*` |
|      - | 6861 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 6862 |  |
|      - | 6863 | ` */` |
|      4 | 6864 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 6865 | `	const char *zInput, /* Raw input */` |
|      - | 6866 | `	int nByte,  /* Input length */` |
|      - | 6867 | `	int delim,  /* Delimiter */` |
|      - | 6868 | `	int encl,   /* Enclosure */` |
|      - | 6869 | `	int escape,  /* Escape character */` |
|      - | 6870 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 6871 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 6872 | `	)` |
|      1 | 6873 | `{` |
|      5 | 6874 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 6875 | `	const char *zIn = zInput;` |
|      - | 6876 | `	const char *zPtr;` |
|      - | 6877 | `	int isEnc;` |
|      - | 6878 | `	/* Start processing */` |
|      8 | 6879 | `	for(;;){` |
|     17 | 6880 | `		if( zIn >= zEnd ){` |
|      - | 6881 | `			/* No more input to process */` |
|      5 | 6882 | `			break;` |
|      - | 6883 | `		}` |
|     13 | 6884 | `		isEnc = 0;` |
|     13 | 6885 | `		zPtr = zIn;` |
|      - | 6886 | `		/* Find the first delimiter */` |
|     27 | 6887 | `		while( zIn < zEnd ){` |
|     23 | 6888 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 6889 | `				/* Delimiter found,break imediately */` |
|      5 | 6890 | `				break;` |
|     15 | 6891 | `			}else if( zIn[0] == encl ){` |
|      - | 6892 | `				/* Inside enclosure? */` |
|    ! 0 | 6893 | `				isEnc = !isEnc;` |
|     15 | 6894 | `			}else if( zIn[0] == escape ){` |
|      - | 6895 | `				/* Escape sequence */` |
|    ! 0 | 6896 | `				zIn++;` |
|    ! 0 | 6897 | `			}` |
|      - | 6898 | `			/* Advance the cursor */` |
|     15 | 6899 | `			zIn++;` |
|      1 | 6900 | `		}` |
|     13 | 6901 | `		if( zIn > zPtr ){` |
|     13 | 6902 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 6903 | `			sxi32 rc;` |
|      - | 6904 | `			/* Invoke the supllied callback */` |
|     13 | 6905 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 6906 | `				zPtr++;` |
|    ! 0 | 6907 | `				nByteChunk-=2;` |
|    ! 0 | 6908 | `			}` |
|     13 | 6909 | `			if( nByteChunk > 0 ){` |
|     13 | 6910 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 6911 | `				if( rc == SXERR_ABORT ){` |
|      - | 6912 | `					/* User callback request an operation abort */` |
|    ! 0 | 6913 | `					break;` |
|      - | 6914 | `				}` |
|      6 | 6915 | `			}` |
|      6 | 6916 | `		}` |
|      - | 6917 | `		/* Ignore trailing delimiter */` |
|     21 | 6918 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 6919 | `			zIn++;` |
|      1 | 6920 | `		}` |
|      1 | 6921 | `	}` |
|      5 | 6922 | `	return SXRET_OK;` |
|      1 | 6923 | `}` |
|      - | 6924 | `/*` |
|      - | 6925 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 6926 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 6927 | ` * argument to this callback.` |
|      - | 6928 | ` */` |
|     12 | 6929 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 6930 | `{` |
|     13 | 6931 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 6932 | `	ph7_value sEntry;` |
|      - | 6933 | `	SyString sToken;` |
|      - | 6934 | `	/* Insert the token in the given array */` |
|     13 | 6935 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 6936 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 6937 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 6938 | `	if( sToken.nByte < 1){` |
|    ! 0 | 6939 | `		return SXRET_OK;` |
|      - | 6940 | `	}` |
|     13 | 6941 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 6942 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 6943 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 6944 | `	return SXRET_OK;` |
|      7 | 6945 | `}` |
|      - | 6946 | `/*` |
|      - | 6947 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 6948 | ` *  Parse a CSV string into an array.` |
|      - | 6949 | ` * Parameters` |
|      - | 6950 | ` *  $input` |
|      - | 6951 | ` *   The string to parse.` |
|      - | 6952 | ` *  $delimiter` |
|      - | 6953 | ` *   Set the field delimiter (one character only).` |
|      - | 6954 | ` *  $enclosure` |
|      - | 6955 | ` *   Set the field enclosure character (one character only).` |
|      - | 6956 | ` *  $escape` |
|      - | 6957 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 6958 | ` * Return` |
|      - | 6959 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 6960 | ` */` |
|      2 | 6961 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6962 | `{` |
|      - | 6963 | `	const char *zInput,*zPtr;` |
|      - | 6964 | `	ph7_value *pArray;` |
|      3 | 6965 | `	int delim  = ',';   /* Delimiter */` |
|      3 | 6966 | `	int encl   = '"' ;  /* Enclosure */` |
|      3 | 6967 | `	int escape = '\\';  /* Escape character */` |
|      - | 6968 | `	int nLen;` |
|      3 | 6969 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6970 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 6971 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6972 | `		return PH7_OK;` |
|      - | 6973 | `	}` |
|      - | 6974 | `	/* Extract the raw input */` |
|      3 | 6975 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 6976 | `	if( nArg > 1 ){` |
|      - | 6977 | `		int i;` |
|      3 | 6978 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 6979 | `			/* Extract the delimiter */` |
|      3 | 6980 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 6981 | `			if( i > 0 ){` |
|      3 | 6982 | `				delim = zPtr[0];` |
|      1 | 6983 | `			}` |
|      1 | 6984 | `		}` |
|      3 | 6985 | `		if( nArg > 2 ){` |
|      3 | 6986 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 6987 | `				/* Extract the enclosure */` |
|      3 | 6988 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 6989 | `				if( i > 0 ){` |
|      3 | 6990 | `					encl = zPtr[0];` |
|      1 | 6991 | `				}` |
|      1 | 6992 | `			}` |
|      3 | 6993 | `			if( nArg > 3 ){` |
|      3 | 6994 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 6995 | `					/* Extract the escape character */` |
|      3 | 6996 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 6997 | `					if( i > 0 ){` |
|      3 | 6998 | `						escape = zPtr[0];` |
|      1 | 6999 | `					}` |
|      1 | 7000 | `				}` |
|      1 | 7001 | `			}` |
|      1 | 7002 | `		}` |
|      1 | 7003 | `	}` |
|      - | 7004 | `	/* Create our array */` |
|      3 | 7005 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 7006 | `	if( pArray == 0 ){` |
|      - | 7007 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|    ! 0 | 7008 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7009 | `	}` |
|      - | 7010 | `	/* Parse the raw input */` |
|      3 | 7011 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 7012 | `	/* Return the freshly created array */` |
|      3 | 7013 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 7014 | `	return PH7_OK;` |
|      2 | 7015 | `}` |
|      - | 7016 | `/*` |
|      - | 7017 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 7018 | ` * container.` |
|      - | 7019 | ` * Refer to [strip_tags()].` |
|      - | 7020 | ` */` |
|     10 | 7021 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 7022 | `{` |
|     11 | 7023 | `	const char *zEnd = &zTag[nByte];` |
|      - | 7024 | `	const char *zPtr;` |
|      - | 7025 | `	SyString sEntry;` |
|      - | 7026 | `	/* Strip tags */` |
|     10 | 7027 | `	for(;;){` |
|     45 | 7028 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 7029 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 7030 | `				zTag++;` |
|      1 | 7031 | `		}` |
|     21 | 7032 | `		if( zTag >= zEnd ){` |
|     11 | 7033 | `			break;` |
|      - | 7034 | `		}` |
|     11 | 7035 | `		zPtr = zTag;` |
|      - | 7036 | `		/* Delimit the tag */` |
|     25 | 7037 | `		while(zTag < zEnd ){` |
|     25 | 7038 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 7039 | `				/* UTF-8 stream */` |
|      3 | 7040 | `				zTag++;` |
|      5 | 7041 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 7042 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 7043 | `				break;` |
|    ! 0 | 7044 | `			}else{` |
|     13 | 7045 | `				zTag++;` |
|      - | 7046 | `			}` |
|      1 | 7047 | `		}` |
|     11 | 7048 | `		if( zTag > zPtr ){` |
|      - | 7049 | `			/* Perform the insertion */` |
|     11 | 7050 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 7051 | `			SyStringFullTrim(&sEntry);` |
|     11 | 7052 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 7053 | `		}` |
|      - | 7054 | `		/* Jump the trailing '>' */` |
|     11 | 7055 | `		zTag++;` |
|      1 | 7056 | `	}` |
|     11 | 7057 | `	return SXRET_OK;` |
|      1 | 7058 | `}` |
|      - | 7059 | `/*` |
|      - | 7060 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 7061 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 7062 | ` * Refer to [strip_tags()].` |
|      - | 7063 | ` */` |
|     36 | 7064 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 7065 | `{` |
|     37 | 7066 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 7067 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 7068 | `		SyString sTag;` |
|     85 | 7069 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 7070 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 7071 | `			zTag++;` |
|      1 | 7072 | `		}` |
|      - | 7073 | `		/* Delimit the tag */` |
|     25 | 7074 | `		zCur = zTag;` |
|     77 | 7075 | `		while(zTag < zEnd ){` |
|     77 | 7076 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 7077 | `				/* UTF-8 stream */` |
|      5 | 7078 | `				zTag++;` |
|      9 | 7079 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 7080 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 7081 | `				break;` |
|    ! 0 | 7082 | `			}else{` |
|     49 | 7083 | `				zTag++;` |
|      - | 7084 | `			}` |
|      1 | 7085 | `		}` |
|     25 | 7086 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 7087 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 7088 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 7089 | `		if( sTag.nByte > 0 ){` |
|      - | 7090 | `			SyString *aEntry,*pEntry;` |
|      - | 7091 | `			sxi32 rc;` |
|      - | 7092 | `			sxu32 n;` |
|      - | 7093 | `			/* Perform the lookup */` |
|     25 | 7094 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 7095 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 7096 | `				pEntry = &aEntry[n];` |
|      - | 7097 | `				/* Do the comparison */` |
|     25 | 7098 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 7099 | `				if( !rc ){` |
|     21 | 7100 | `					return SXRET_OK;` |
|      - | 7101 | `				}` |
|      3 | 7102 | `			}` |
|      2 | 7103 | `		}` |
|      2 | 7104 | `	}` |
|      - | 7105 | `	/* No such tag */` |
|     17 | 7106 | `	return SXERR_NOTFOUND;` |
|     19 | 7107 | `}` |
|      - | 7108 | `/*` |
|      - | 7109 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 7110 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 7111 | ` * Refer to [strip_tags()].` |
|      - | 7112 | ` */` |
|     16 | 7113 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 7114 | `{` |
|     17 | 7115 | `	const char *zEnd = &zIn[nByte];` |
|      - | 7116 | `	const char *zPtr,*zTag;` |
|      - | 7117 | `	SySet sSet;` |
|      - | 7118 | `	/* initialize the set of allowed tags */` |
|     17 | 7119 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 7120 | `	if( nTaglen > 0 ){` |
|      - | 7121 | `		/* Set of allowed tags */` |
|     11 | 7122 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 7123 | `	}` |
|      - | 7124 | `	/* Set the empty string */` |
|     17 | 7125 | `	ph7_result_string(pCtx,"",0);` |
|      - | 7126 | `	/* Start processing */` |
|     26 | 7127 | `	for(;;){` |
|     53 | 7128 | `		if(zIn >= zEnd){` |
|      - | 7129 | `			/* No more input to process */` |
|     15 | 7130 | `			break;` |
|      - | 7131 | `		}` |
|     39 | 7132 | `		zPtr = zIn;` |
|      - | 7133 | `		/* Find a tag */` |
|    133 | 7134 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 7135 | `			zIn++;` |
|      1 | 7136 | `		}` |
|     39 | 7137 | `		if( zIn > zPtr ){` |
|      - | 7138 | `			/* Consume raw input */` |
|     21 | 7139 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 7140 | `		}` |
|      - | 7141 | `		/* Ignore trailing null bytes */` |
|     39 | 7142 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 7143 | `			zIn++;` |
|    ! 0 | 7144 | `		}` |
|     39 | 7145 | `		if(zIn >= zEnd){` |
|      - | 7146 | `			/* No more input to process */` |
|      3 | 7147 | `			break;` |
|      - | 7148 | `		}` |
|     37 | 7149 | `		if( zIn[0] == '<' ){` |
|      - | 7150 | `			sxi32 rc;` |
|     37 | 7151 | `			zTag = zIn++;` |
|      - | 7152 | `			/* Delimit the tag */` |
|    127 | 7153 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 7154 | `				zIn++;` |
|      1 | 7155 | `			}` |
|     37 | 7156 | `			if( zIn < zEnd ){` |
|     37 | 7157 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 7158 | `			}` |
|      - | 7159 | `			/* Query the set */` |
|     37 | 7160 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 7161 | `			if( rc == SXRET_OK ){` |
|      - | 7162 | `				/* Keep the tag */` |
|     21 | 7163 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 7164 | `			}` |
|     18 | 7165 | `		}` |
|      1 | 7166 | `	}` |
|      - | 7167 | `	/* Cleanup */` |
|     17 | 7168 | `	SySetRelease(&sSet);` |
|     17 | 7169 | `	return SXRET_OK;` |
|      1 | 7170 | `}` |
|      - | 7171 | `/*` |
|      - | 7172 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 7173 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 7174 | ` * Parameters` |
|      - | 7175 | ` *  $str` |
|      - | 7176 | ` *  The input string.` |
|      - | 7177 | ` * $allowable_tags` |
|      - | 7178 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 7179 | ` * Return` |
|      - | 7180 | ` *  Returns the stripped string.` |
|      - | 7181 | ` */` |
|     14 | 7182 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7183 | `{` |
|     15 | 7184 | `	const char *zTaglist = 0;` |
|      - | 7185 | `	const char *zString;` |
|     15 | 7186 | `	int nTaglen = 0;` |
|      - | 7187 | `	int nLen;` |
|     15 | 7188 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7189 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 7190 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7191 | `		return PH7_OK;` |
|      - | 7192 | `	}` |
|      - | 7193 | `	/* Point to the raw string */` |
|     15 | 7194 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 7195 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 7196 | `		/* Allowed tag */` |
|     11 | 7197 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 7198 | `	}` |
|      - | 7199 | `	/* Process input */` |
|     15 | 7200 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 7201 | `	return PH7_OK;` |
|      8 | 7202 | `}` |
|      - | 7203 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7204 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7205 | `/*` |
|      - | 7206 | ` * string str_shuffle(string $str)` |
|      - | 7207 |  |
|      - | 7208 | ` *  Randomly shuffles a string.` |
|      - | 7209 | ` * Parameters` |
|      - | 7210 | ` *  $str` |
|      - | 7211 | ` *   The input string.` |
|      - | 7212 | ` * Return` |
|      - | 7213 | ` *  Returns the shuffled string.` |
|      - | 7214 | ` */` |
|     10 | 7215 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7216 | `{` |
|      - | 7217 | `	const char *zString;` |
|      - | 7218 | `	int nLen,i,c;` |
|      - | 7219 | `	sxu32 iR;` |
|     11 | 7220 | `	if( nArg < 1 ){` |
|      - | 7221 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7222 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7223 | `		return PH7_OK;` |
|      - | 7224 | `	}` |
|      - | 7225 | `	/* Extract the target string */` |
|     11 | 7226 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 7227 | `	if( nLen < 1 ){` |
|      - | 7228 | `		/* Nothing to shuffle */` |
|      3 | 7229 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 7230 | `		return PH7_OK;` |
|      - | 7231 | `	}` |
|      - | 7232 | `	/* Shuffle the string */` |
|     43 | 7233 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 7234 | `		/* Generate a random number first */` |
|     35 | 7235 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 7236 | `		/* Extract a random offset */` |
|     35 | 7237 | `		c = zString[iR % nLen];` |
|      - | 7238 | `		/* Append it */` |
|     35 | 7239 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 7240 | `	}` |
|      9 | 7241 | `	return PH7_OK;` |
|      6 | 7242 | `}` |
|      - | 7243 | `/*` |
|      - | 7244 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 7245 | ` *  Convert a string to an array.` |
|      - | 7246 | ` * Parameters` |
|      - | 7247 | ` * $string` |
|      - | 7248 | ` *  The input string.` |
|      - | 7249 | ` * $split_length` |
|      - | 7250 | ` *  Maximum length of the chunk.` |
|      - | 7251 | ` * Return` |
|      - | 7252 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 7253 | ` *  except possibly the last one which may be shorter.` |
|      - | 7254 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 7255 | ` *  as the first (and only) array element.` |
|      - | 7256 | ` *  An empty string returns an empty array.` |
|      - | 7257 | ` * Errors` |
|      - | 7258 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 7259 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 7260 | ` *  ValueError if $split_length is less than 1.` |
|      - | 7261 | ` */` |
|     24 | 7262 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 7263 | `{` |
|      - | 7264 | `	const char *zString,*zEnd;` |
|      - | 7265 | `	ph7_value *pArray,*pValue;` |
|      - | 7266 | `	int split_len;` |
|      - | 7267 | `	int nLen;` |
|     27 | 7268 | `	if( nArg < 1 ){` |
|    ! 0 | 7269 | `		return PH7_VmThrowException(pCtx,` |
|      - | 7270 | `			"ArgumentCountError",` |
|      - | 7271 | `			"str_split() expects at least 1 argument, %d given",` |
|    ! 0 | 7272 | `			nArg` |
|      - | 7273 | `			);` |
|      - | 7274 | `	}` |
|      - | 7275 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     36 | 7276 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     39 | 7277 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 7278 | `	    ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 7279 | `		return PH7_VmThrowException(pCtx,` |
|      - | 7280 | `			"TypeError",` |
|      - | 7281 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 7282 | `			ph7_type_name(apArg[0])` |
|      - | 7283 | `			);` |
|      - | 7284 | `	}` |
|      - | 7285 | `	/* Point to the target string */` |
|     27 | 7286 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 7287 | `	split_len = (int)sizeof(char);` |
|     27 | 7288 | `	if( nArg > 1 ){` |
|      - | 7289 | `		/* Split length */` |
|     17 | 7290 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 7291 | `		if( split_len < 1 ){` |
|      6 | 7292 | `			return PH7_VmThrowException(pCtx,` |
|      - | 7293 | `				"ValueError",` |
|      - | 7294 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 7295 | `				);` |
|      - | 7296 | `		}` |
|     11 | 7297 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 7298 | `			split_len = nLen;` |
|      1 | 7299 | `		}` |
|      5 | 7300 | `	}` |
|      - | 7301 | `	/* Create the array and the scalar value */` |
|     21 | 7302 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 7303 | `	/*Chunk value */` |
|     21 | 7304 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 7305 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 7306 | `		/* Return FALSE */` |
|    ! 0 | 7307 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7308 | `		return PH7_OK;` |
|      - | 7309 | `	}` |
|      - | 7310 | `	/* Point to the end of the string */` |
|     21 | 7311 | `	zEnd = &zString[nLen];` |
|      - | 7312 | `	/* Perform the requested operation */` |
|     48 | 7313 | `	for(;;){` |
|      - | 7314 | `		int nMax;` |
|     59 | 7315 | `		if( zString >= zEnd ){` |
|      - | 7316 | `			/* No more input to process */` |
|     21 | 7317 | `			break;` |
|      - | 7318 | `		}` |
|     39 | 7319 | `		nMax = (int)(zEnd-zString);` |
|     39 | 7320 | `		if( nMax < split_len ){` |
|      3 | 7321 | `			split_len = nMax;` |
|      1 | 7322 | `		}` |
|      - | 7323 | `		/* Copy the current chunk */` |
|     39 | 7324 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 7325 | `		/* Insert it */` |
|     39 | 7326 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|    ! 0 | 7327 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 7328 | `		}` |
|      - | 7329 | `		/* reset the string cursor */` |
|     39 | 7330 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 7331 | `		/* Update position */` |
|     39 | 7332 | `		zString += split_len;` |
|      1 | 7333 | `	}` |
|      - | 7334 | `	/*` |
|      - | 7335 | `	 * Return the array.` |
|      - | 7336 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 7337 | `	 * upon we return from this function.` |
|      - | 7338 | `	 */` |
|     21 | 7339 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 7340 | `	return PH7_OK;` |
|     15 | 7341 | `}` |
|      - | 7342 | `/*` |
|      - | 7343 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 7344 | ` * Refer to [strspn()].` |
|      - | 7345 | ` */` |
|     28 | 7346 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 7347 | `{` |
|     29 | 7348 | `	const char *zIn = *pzIn;` |
|      - | 7349 | `	const char *zPtr;` |
|      - | 7350 | `	/* Ignore leading white spaces */` |
|     29 | 7351 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 7352 | `		zIn++;` |
|    ! 0 | 7353 | `	}` |
|     29 | 7354 | `	if( zIn >= zEnd ){` |
|      - | 7355 | `		/* End of input */` |
|    ! 0 | 7356 | `		return SXERR_EOF;` |
|      - | 7357 | `	}` |
|     29 | 7358 | `	zPtr = zIn;` |
|      - | 7359 | `	/* Extract the token */` |
|    201 | 7360 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 7361 | `		zIn++;` |
|      1 | 7362 | `	}` |
|     29 | 7363 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 7364 | `	/* Synchronize pointers */` |
|     29 | 7365 | `	*pzIn = zIn;` |
|      - | 7366 | `	/* Return to the caller */` |
|     29 | 7367 | `	return SXRET_OK;` |
|     15 | 7368 | `}` |
|      - | 7369 | `/*` |
|      - | 7370 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 7371 | ` * return the longest match.` |
|      - | 7372 | ` * Refer to [strspn()].` |
|      - | 7373 | ` */` |
|     18 | 7374 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 7375 | `{` |
|     19 | 7376 | `	const char *zEnd = &zString[nLen];` |
|     19 | 7377 | `	const char *zIn = zString;` |
|      - | 7378 | `	int i,c;` |
|     45 | 7379 | `	for(;;){` |
|     91 | 7380 | `		if( zString >= zEnd ){` |
|      7 | 7381 | `			break;` |
|      - | 7382 | `		}` |
|      - | 7383 | `		/* Extract current character */` |
|     85 | 7384 | `		c = zString[0];` |
|      - | 7385 | `		/* Perform the lookup */` |
|    383 | 7386 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 7387 | `			if( c == zMask[i] ){` |
|      - | 7388 | `				/* Character found */` |
|     73 | 7389 | `				break;` |
|      - | 7390 | `			}` |
|    150 | 7391 | `		}` |
|     85 | 7392 | `		if( i >= nMaskLen ){` |
|      - | 7393 | `			/* Character not in the current mask,break immediately */` |
|     13 | 7394 | `			break;` |
|      - | 7395 | `		}` |
|      - | 7396 | `		/* Advance cursor */` |
|     73 | 7397 | `		zString++;` |
|      1 | 7398 | `	}` |
|      - | 7399 | `	/* Longest match */` |
|     19 | 7400 | `	return (int)(zString-zIn);` |
|      1 | 7401 | `}` |
|      - | 7402 | `/*` |
|      - | 7403 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 7404 | ` * Refer to [strcspn()].` |
|      - | 7405 | ` */` |
|     10 | 7406 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 7407 | `{` |
|     11 | 7408 | `	const char *zEnd = &zString[nLen];` |
|     11 | 7409 | `	const char *zIn = zString;` |
|      - | 7410 | `	int i,c;` |
|     12 | 7411 | `	for(;;){` |
|     25 | 7412 | `		if( zString >= zEnd ){` |
|      3 | 7413 | `			break;` |
|      - | 7414 | `		}` |
|      - | 7415 | `		/* Extract current character */` |
|     23 | 7416 | `		c = zString[0];` |
|      - | 7417 | `		/* Perform the lookup */` |
|     51 | 7418 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 7419 | `			if( c == zMask[i] ){` |
|      9 | 7420 | `				break;` |
|      - | 7421 | `			}` |
|     15 | 7422 | `		}` |
|     23 | 7423 | `		if( i < nMaskLen ){` |
|      - | 7424 | `			/* Character in the current mask,break immediately */` |
|      9 | 7425 | `			break;` |
|      - | 7426 | `		}` |
|      - | 7427 | `		/* Advance cursor */` |
|     15 | 7428 | `		zString++;` |
|      1 | 7429 | `	}` |
|      - | 7430 | `	/* Longest match */` |
|     11 | 7431 | `	return (int)(zString-zIn);` |
|      1 | 7432 | `}` |
|      - | 7433 | `/*` |
|      - | 7434 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 7435 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 7436 | ` *  of characters contained within a given mask.` |
|      - | 7437 | ` * Parameters` |
|      - | 7438 | ` * $str` |
|      - | 7439 | ` *  The input string.` |
|      - | 7440 | ` * $mask` |
|      - | 7441 | ` *  The list of allowable characters.` |
|      - | 7442 | ` * $start` |
|      - | 7443 | ` *  The position in subject to start searching.` |
|      - | 7444 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 7445 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 7446 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 7447 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 7448 | ` *  start'th position from the end of subject.` |
|      - | 7449 | ` * $length` |
|      - | 7450 | ` *  The length of the segment from subject to examine.` |
|      - | 7451 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 7452 | ` *  characters after the starting position.` |
|      - | 7453 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 7454 | ` *  position up to length characters from the end of subject.` |
|      - | 7455 | ` * Return` |
|      - | 7456 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 7457 | ` * in mask.` |
|      - | 7458 | ` */` |
|     24 | 7459 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7460 | `{` |
|      - | 7461 | `	const char *zString,*zMask,*zEnd;` |
|      - | 7462 | `	int iMasklen,iLen;` |
|      - | 7463 | `	SyString sToken;` |
|     25 | 7464 | `	int iCount = 0;` |
|      - | 7465 | `	int rc;` |
|     25 | 7466 | `	if( nArg < 2 ){` |
|      - | 7467 | `		/* Missing agruments,return zero */` |
|    ! 0 | 7468 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7469 | `		return PH7_OK;` |
|      - | 7470 | `	}` |
|      - | 7471 | `	/* Extract the target string */` |
|     25 | 7472 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7473 | `	/* Extract the mask */` |
|     25 | 7474 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 7475 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 7476 | `		/* Nothing to process,return zero */` |
|      7 | 7477 | `		ph7_result_int(pCtx,0);` |
|      7 | 7478 | `		return PH7_OK;` |
|      - | 7479 | `	}` |
|     19 | 7480 | `	if( nArg > 2 ){` |
|      - | 7481 | `		int nOfft;` |
|      - | 7482 | `		/* Extract the offset */` |
|      9 | 7483 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 7484 | `		if( nOfft < 0 ){` |
|    ! 0 | 7485 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 7486 | `			if( zBase > zString ){` |
|    ! 0 | 7487 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 7488 | `				zString = zBase;` |
|    ! 0 | 7489 | `			}else{` |
|      - | 7490 | `				/* Invalid offset */` |
|    ! 0 | 7491 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7492 | `				return PH7_OK;` |
|      - | 7493 | `			}` |
|    ! 0 | 7494 | `		}else{` |
|      9 | 7495 | `			if( nOfft >= iLen ){` |
|      - | 7496 | `				/* Invalid offset */` |
|    ! 0 | 7497 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7498 | `				return PH7_OK;` |
|    ! 0 | 7499 | `			}else{` |
|      - | 7500 | `				/* Update offset */` |
|      9 | 7501 | `				zString += nOfft;` |
|      9 | 7502 | `				iLen -= nOfft;` |
|      - | 7503 | `			}` |
|      - | 7504 | `		}` |
|      9 | 7505 | `		if( nArg > 3 ){` |
|      - | 7506 | `			int iUserlen;` |
|      - | 7507 | `			/* Extract the desired length */` |
|      9 | 7508 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 7509 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 7510 | `				iLen = iUserlen;` |
|      2 | 7511 | `			}` |
|      4 | 7512 | `		}` |
|      4 | 7513 | `	}` |
|      - | 7514 | `	/* Point to the end of the string */` |
|     19 | 7515 | `	zEnd = &zString[iLen];` |
|      - | 7516 | `	/* Extract the first non-space token */` |
|     19 | 7517 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 7518 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 7519 | `		/* Compare against the current mask */` |
|     19 | 7520 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 7521 | `	}` |
|      - | 7522 | `	/* Longest match */` |
|     19 | 7523 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 7524 | `	return PH7_OK;` |
|     13 | 7525 | `}` |
|      - | 7526 | `/*` |
|      - | 7527 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 7528 | ` *  Find length of initial segment not matching mask.` |
|      - | 7529 | ` * Parameters` |
|      - | 7530 | ` * $str` |
|      - | 7531 | ` *  The input string.` |
|      - | 7532 | ` * $mask` |
|      - | 7533 | ` *  The list of not allowed characters.` |
|      - | 7534 | ` * $start` |
|      - | 7535 | ` *  The position in subject to start searching.` |
|      - | 7536 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 7537 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 7538 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 7539 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 7540 | ` *  start'th position from the end of subject.` |
|      - | 7541 | ` * $length` |
|      - | 7542 | ` *  The length of the segment from subject to examine.` |
|      - | 7543 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 7544 | ` *  characters after the starting position.` |
|      - | 7545 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 7546 | ` *  position up to length characters from the end of subject.` |
|      - | 7547 | ` * Return` |
|      - | 7548 | ` *  Returns the length of the segment as an integer.` |
|      - | 7549 | ` */` |
|     14 | 7550 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7551 | `{` |
|      - | 7552 | `	const char *zString,*zMask,*zEnd;` |
|      - | 7553 | `	int iMasklen,iLen;` |
|      - | 7554 | `	SyString sToken;` |
|     15 | 7555 | `	int iCount = 0;` |
|      - | 7556 | `	int rc;` |
|     15 | 7557 | `	if( nArg < 2 ){` |
|      - | 7558 | `		/* Missing agruments,return zero */` |
|    ! 0 | 7559 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7560 | `		return PH7_OK;` |
|      - | 7561 | `	}` |
|      - | 7562 | `	/* Extract the target string */` |
|     15 | 7563 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7564 | `	/* Extract the mask */` |
|     15 | 7565 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 7566 | `	if( iLen < 1 ){` |
|      - | 7567 | `		/* Nothing to process,return zero */` |
|    ! 0 | 7568 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7569 | `		return PH7_OK;` |
|      - | 7570 | `	}` |
|     15 | 7571 | `	if( iMasklen < 1 ){` |
|      - | 7572 | `		/* No given mask,return the string length */` |
|      3 | 7573 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 7574 | `		return PH7_OK;` |
|      - | 7575 | `	}` |
|     13 | 7576 | `	if( nArg > 2 ){` |
|      - | 7577 | `		int nOfft;` |
|      - | 7578 | `		/* Extract the offset */` |
|     11 | 7579 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 7580 | `		if( nOfft < 0 ){` |
|    ! 0 | 7581 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 7582 | `			if( zBase > zString ){` |
|    ! 0 | 7583 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 7584 | `				zString = zBase;` |
|    ! 0 | 7585 | `			}else{` |
|      - | 7586 | `				/* Invalid offset */` |
|    ! 0 | 7587 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7588 | `				return PH7_OK;` |
|      - | 7589 | `			}` |
|    ! 0 | 7590 | `		}else{` |
|     11 | 7591 | `			if( nOfft >= iLen ){` |
|      - | 7592 | `				/* Invalid offset */` |
|      3 | 7593 | `				ph7_result_int(pCtx,0);` |
|      3 | 7594 | `				return PH7_OK;` |
|    ! 0 | 7595 | `			}else{` |
|      - | 7596 | `				/* Update offset */` |
|      9 | 7597 | `				zString += nOfft;` |
|      9 | 7598 | `				iLen -= nOfft;` |
|      - | 7599 | `			}` |
|      - | 7600 | `		}` |
|      9 | 7601 | `		if( nArg > 3 ){` |
|      - | 7602 | `			int iUserlen;` |
|      - | 7603 | `			/* Extract the desired length */` |
|    ! 0 | 7604 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 7605 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 7606 | `				iLen = iUserlen;` |
|    ! 0 | 7607 | `			}` |
|    ! 0 | 7608 | `		}` |
|      4 | 7609 | `	}` |
|      - | 7610 | `	/* Point to the end of the string */` |
|     11 | 7611 | `	zEnd = &zString[iLen];` |
|      - | 7612 | `	/* Extract the first non-space token */` |
|     11 | 7613 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 7614 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 7615 | `		/* Compare against the current mask */` |
|     11 | 7616 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 7617 | `	}` |
|      - | 7618 | `	/* Longest match */` |
|     11 | 7619 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 7620 | `	return PH7_OK;` |
|      8 | 7621 | `}` |
|      - | 7622 | `/*` |
|      - | 7623 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 7624 | ` *  Search a string for any of a set of characters.` |
|      - | 7625 | ` * Parameters` |
|      - | 7626 | ` *  $haystack` |
|      - | 7627 | ` *   The string where char_list is looked for.` |
|      - | 7628 | ` *  $char_list` |
|      - | 7629 | ` *   This parameter is case sensitive.` |
|      - | 7630 | ` * Return` |
|      - | 7631 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 7632 | ` */` |
|      4 | 7633 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7634 | `{` |
|      - | 7635 | `	const char *zString,*zList,*zEnd;` |
|      - | 7636 | `	int iLen,iListLen,i,c;` |
|      - | 7637 | `	sxu32 nOfft,nMax;` |
|      - | 7638 | `	sxi32 rc;` |
|      5 | 7639 | `	if( nArg < 2 ){` |
|      - | 7640 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7641 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7642 | `		return PH7_OK;` |
|      - | 7643 | `	}` |
|      - | 7644 | `	/* Extract the haystack and the char list */` |
|      5 | 7645 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 7646 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 7647 | `	if( iLen < 1 ){` |
|      - | 7648 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7649 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7650 | `		return PH7_OK;` |
|      - | 7651 | `	}` |
|      - | 7652 | `	/* Point to the end of the string */` |
|      5 | 7653 | `	zEnd = &zString[iLen];` |
|      5 | 7654 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 7655 | `	/* perform the requested operation */` |
|     15 | 7656 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 7657 | `		c = zList[i];` |
|     11 | 7658 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 7659 | `		if( rc == SXRET_OK ){` |
|      5 | 7660 | `			if( nMax < nOfft ){` |
|      3 | 7661 | `				nOfft = nMax;` |
|      1 | 7662 | `			}` |
|      2 | 7663 | `		}` |
|      6 | 7664 | `	}` |
|      5 | 7665 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 7666 | `		/* No such substring,return FALSE */` |
|      3 | 7667 | `		ph7_result_bool(pCtx,0);` |
|      2 | 7668 | `	}else{` |
|      - | 7669 | `		/* Return the substring */` |
|      3 | 7670 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 7671 | `	}` |
|      5 | 7672 | `	return PH7_OK;` |
|      3 | 7673 | `}` |
|      - | 7674 | `/* SPDX-SnippetBegin */` |
|      - | 7675 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 7676 | `/* SPDX-License-Identifier: blessing */` |
|      - | 7677 | `/*` |
|      - | 7678 | ` * string soundex(string $str)` |
|      - | 7679 | ` *  Calculate the soundex key of a string.` |
|      - | 7680 | ` * Parameters` |
|      - | 7681 | ` *  $str` |
|      - | 7682 | ` *   The input string.` |
|      - | 7683 | ` * Return` |
|      - | 7684 | ` *  Returns the soundex key as a string.` |
|      - | 7685 | ` * Note:` |
|      - | 7686 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 7687 | ` * source tree.` |
|      - | 7688 | ` */` |
|     22 | 7689 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7690 | `{` |
|      - | 7691 | `	const unsigned char *zIn;` |
|      - | 7692 | `	char zResult[8];` |
|      - | 7693 | `	int i, j;` |
|      - | 7694 | `	static const unsigned char iCode[] = {` |
|      - | 7695 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7696 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7697 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7698 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7699 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 7700 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 7701 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 7702 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 7703 | `	};` |
|     23 | 7704 | `	if( nArg < 1 ){` |
|      - | 7705 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7706 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7707 | `		return PH7_OK;` |
|      - | 7708 | `	}` |
|     23 | 7709 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     35 | 7710 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     23 | 7711 | `	if( zIn[i] ){` |
|     17 | 7712 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 7713 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 7714 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 7715 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 7716 | `			if( code>0 ){` |
|     45 | 7717 | `				if( code!=prevcode ){` |
|     33 | 7718 | `					prevcode = (unsigned char)code;` |
|     33 | 7719 | `					zResult[j++] = (char)code + '0';` |
|     16 | 7720 | `				}` |
|     23 | 7721 | `			}else{` |
|     49 | 7722 | `				prevcode = 0;` |
|      - | 7723 | `			}` |
|     47 | 7724 | `		}` |
|     33 | 7725 | `		while( j<4 ){` |
|     17 | 7726 | `			zResult[j++] = '0';` |
|      1 | 7727 | `		}` |
|     17 | 7728 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 7729 | `	}else{` |
|      - | 7730 | `	  /* No alphabetic character: PHP returns "0000" (not the SQLite "?000"). */` |
|      7 | 7731 | `	  ph7_result_string(pCtx,"0000",4);` |
|      - | 7732 | `	}` |
|     23 | 7733 | `	return PH7_OK;` |
|     12 | 7734 | `}` |
|      - | 7735 | `/* SPDX-SnippetEnd */` |
|      - | 7736 | `/*` |
|      - | 7737 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 7738 | ` *  Wraps a string to a given number of characters.` |
|      - | 7739 | ` * Parameters` |
|      - | 7740 | ` *  $str` |
|      - | 7741 | ` *   The input string.` |
|      - | 7742 | ` * $width` |
|      - | 7743 | ` *  The column width.` |
|      - | 7744 | ` * $break` |
|      - | 7745 | ` *  The line is broken using the optional break parameter.` |
|      - | 7746 | ` * Return` |
|      - | 7747 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 7748 | ` */` |
|     26 | 7749 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7750 | `{` |
|      - | 7751 | `	const char *zIn,*zBreak;` |
|      - | 7752 | `	SyBlob sWorker;` |
|      - | 7753 | `	int iLen,iBreaklen,iWidth,iCut,iStart,iSpace,iCur;` |
|      - | 7754 | `	sxi32 rc;` |
|     27 | 7755 | `	if( nArg < 1 ){` |
|      - | 7756 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7757 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7758 | `		return PH7_OK;` |
|      - | 7759 | `	}` |
|      - | 7760 | `	/* Extract the input string */` |
|     27 | 7761 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7762 | `	/* Width (default 75; PHP allows 0/negative — break at every space). */` |
|     27 | 7763 | `	iWidth = 75;` |
|     27 | 7764 | `	if( nArg > 1 ){` |
|     27 | 7765 | `		iWidth = ph7_value_to_int(apArg[1]);` |
|     13 | 7766 | `	}` |
|      - | 7767 | `	/* Break string (default "\n"). */` |
|     27 | 7768 | `	zBreak = "\n";` |
|     27 | 7769 | `	iBreaklen = (int)sizeof(char);` |
|     27 | 7770 | `	if( nArg > 2 ){` |
|     13 | 7771 | `		zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      6 | 7772 | `	}` |
|      - | 7773 | `	/* Cut long words? (default false). */` |
|     27 | 7774 | `	iCut = 0;` |
|     27 | 7775 | `	if( nArg > 3 ){` |
|      7 | 7776 | `		iCut = ph7_value_to_bool(apArg[3]);` |
|      3 | 7777 | `	}` |
|     27 | 7778 | `	if( iLen < 1 ){` |
|      - | 7779 | `		/* PHP returns the empty string for empty input before validating the other args. */` |
|      5 | 7780 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 7781 | `		return PH7_OK;` |
|      - | 7782 | `	}` |
|      - | 7783 | `	/* PHP 8 domain errors (catchable ValueError). */` |
|     23 | 7784 | `	if( iBreaklen < 1 ){` |
|      3 | 7785 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7786 | `			"wordwrap(): Argument #3 ($break) must not be empty");` |
|      - | 7787 | `	}` |
|     21 | 7788 | `	if( iWidth == 0 && iCut ){` |
|      3 | 7789 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7790 | `			"wordwrap(): Argument #4 ($cut_long_words) cannot be true when argument #2 ($width) is 0");` |
|      - | 7791 | `	}` |
|      - | 7792 | `	/*` |
|      - | 7793 | `	 * PHP's algorithm: a single left-to-right pass tracking the start of the` |
|      - | 7794 | `	 * current line (iStart) and the position of the last space seen on it` |
|      - | 7795 | `	 * (iSpace). A break is emitted when the line reaches the width, at the last` |
|      - | 7796 | `	 * space if there was one, otherwise (only when cut is enabled) hard at the` |
|      - | 7797 | `	 * boundary. An existing break sequence in the input resets the line.` |
|      - | 7798 | `	 */` |
|     19 | 7799 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     19 | 7800 | `	iStart = iSpace = iCur = 0;` |
|     19 | 7801 | `	rc = SXRET_OK;` |
|    551 | 7802 | `	while( iCur < iLen ){` |
|    533 | 7803 | `		if( iBreaklen <= iLen - iCur && SyMemcmp(&zIn[iCur],zBreak,(sxu32)iBreaklen) == 0 ){` |
|      - | 7804 | `			/* Existing break sequence in the input: copy it verbatim and reset the line. */` |
|    ! 0 | 7805 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart + iBreaklen));` |
|    ! 0 | 7806 | `			if( rc != SXRET_OK ){ goto oom; }` |
|    ! 0 | 7807 | `			iCur += iBreaklen;` |
|    ! 0 | 7808 | `			iStart = iSpace = iCur;` |
|    ! 0 | 7809 | `			continue;` |
|    533 | 7810 | `		}else if( zIn[iCur] == ' ' ){` |
|     67 | 7811 | `			if( iCur - iStart >= iWidth ){` |
|      - | 7812 | `				/* The line already fills the width at this space: break here (the space is consumed). */` |
|     13 | 7813 | `				rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     13 | 7814 | `				if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     13 | 7815 | `				if( rc != SXRET_OK ){ goto oom; }` |
|     13 | 7816 | `				iStart = iCur + 1;` |
|      6 | 7817 | `			}` |
|     67 | 7818 | `			iSpace = iCur;` |
|    500 | 7819 | `		}else if( iCut && iCur - iStart >= iWidth && iStart >= iSpace ){` |
|      - | 7820 | `			/* A word longer than the width with no space to break at: hard-cut at the boundary. */` |
|      7 | 7821 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|      7 | 7822 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|      7 | 7823 | `			if( rc != SXRET_OK ){ goto oom; }` |
|      7 | 7824 | `			iStart = iSpace = iCur;` |
|    464 | 7825 | `		}else if( iCur - iStart >= iWidth && iStart < iSpace ){` |
|      - | 7826 | `			/* Past the width mid-word: wrap back to the last space (which is consumed). */` |
|     17 | 7827 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iSpace - iStart));` |
|     17 | 7828 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     17 | 7829 | `			if( rc != SXRET_OK ){ goto oom; }` |
|     17 | 7830 | `			iStart = iSpace = iSpace + 1;` |
|      8 | 7831 | `		}` |
|    533 | 7832 | `		iCur++;` |
|      1 | 7833 | `	}` |
|      - | 7834 | `	/* Emit the trailing chunk. */` |
|     19 | 7835 | `	if( iStart < iCur ){` |
|     19 | 7836 | `		rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     19 | 7837 | `		if( rc != SXRET_OK ){ goto oom; }` |
|      9 | 7838 | `	}` |
|     19 | 7839 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|     19 | 7840 | `	SyBlobRelease(&sWorker);` |
|     19 | 7841 | `	return PH7_OK;` |
|    ! 0 | 7842 | `oom:` |
|    ! 0 | 7843 | `	SyBlobRelease(&sWorker);` |
|    ! 0 | 7844 | `	return PH7_ContextMemoryError(pCtx);` |
|     14 | 7845 | `}` |
|      - | 7846 | `/*` |
|      - | 7847 | ` * Check if the given character is a member of the given mask.` |
|      - | 7848 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 7849 | ` * Refer to [strtok()].` |
|      - | 7850 | ` */` |
|     30 | 7851 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 7852 | `{` |
|      - | 7853 | `	int i;` |
|     57 | 7854 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 7855 | `		if( c == zMask[i] ){` |
|     13 | 7856 | `			if( pOfft ){` |
|      5 | 7857 | `				*pOfft = i;` |
|      2 | 7858 | `			}` |
|     13 | 7859 | `			return TRUE;` |
|      - | 7860 | `		}` |
|     14 | 7861 | `	}` |
|     19 | 7862 | `	return FALSE;` |
|     16 | 7863 | `}` |
|      - | 7864 | `/*` |
|      - | 7865 | ` * Extract a single token from the input stream.` |
|      - | 7866 | ` * Refer to [strtok()].` |
|      - | 7867 | ` */` |
|      6 | 7868 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 7869 | `{` |
|      7 | 7870 | `	const char *zIn = *pzIn;` |
|      - | 7871 | `	const char *zPtr;` |
|      - | 7872 | `	/* Ignore leading delimiter */` |
|     11 | 7873 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 7874 | `		zIn++;` |
|      1 | 7875 | `	}` |
|      7 | 7876 | `	if( zIn >= zEnd ){` |
|      - | 7877 | `		/* End of input */` |
|    ! 0 | 7878 | `		return SXERR_EOF;` |
|      - | 7879 | `	}` |
|      7 | 7880 | `	zPtr = zIn;` |
|      - | 7881 | `	/* Extract the token */` |
|     13 | 7882 | `	while( zIn < zEnd ){` |
|     11 | 7883 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 7884 | `			/* UTF-8 stream */` |
|    ! 0 | 7885 | `			zIn++;` |
|    ! 0 | 7886 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 7887 | `		}else{` |
|     11 | 7888 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 7889 | `				break;` |
|      - | 7890 | `			}` |
|      7 | 7891 | `			zIn++;` |
|      - | 7892 | `		}` |
|      1 | 7893 | `	}` |
|      7 | 7894 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 7895 | `	/* Update the cursor */` |
|      7 | 7896 | `	*pzIn = zIn;` |
|      - | 7897 | `	/* Return to the caller */` |
|      7 | 7898 | `	return SXRET_OK;` |
|      4 | 7899 | `}` |
|      - | 7900 | `/* strtok auxiliary private data */` |
|      - | 7901 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 7902 | `struct strtok_aux_data` |
|      - | 7903 | `{` |
|      - | 7904 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 7905 | `	const char *zIn;   /* Current input stream */` |
|      - | 7906 | `	const char *zEnd;  /* End of input */` |
|      - | 7907 | `};` |
|      - | 7908 | `/*` |
|      - | 7909 | ` * string strtok(string $str,string $token)` |
|      - | 7910 | ` * string strtok(string $token)` |
|      - | 7911 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 7912 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 7913 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 7914 | ` *  words by using the space character as the token.` |
|      - | 7915 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 7916 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 7917 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 7918 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 7919 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 7920 | ` *  the argument are found.` |
|      - | 7921 | ` * Parameters` |
|      - | 7922 | ` *  $str` |
|      - | 7923 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 7924 | ` * $token` |
|      - | 7925 | ` *  The delimiter used when splitting up str.` |
|      - | 7926 | ` * Return` |
|      - | 7927 | ` *   Current token or FALSE on EOF.` |
|      - | 7928 | ` */` |
|      6 | 7929 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7930 | `{` |
|      - | 7931 | `	strtok_aux_data *pAux;` |
|      - | 7932 | `	const char *zMask;` |
|      - | 7933 | `	SyString sToken;` |
|      - | 7934 | `	int nMasklen;` |
|      - | 7935 | `	sxi32 rc;` |
|      7 | 7936 | `	if( nArg < 2 ){` |
|      - | 7937 | `		/* Extract top aux data */` |
|      5 | 7938 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      5 | 7939 | `		if( pAux == 0 ){` |
|      - | 7940 | `			/* No aux data,return FALSE */` |
|    ! 0 | 7941 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7942 | `			return PH7_OK;` |
|      - | 7943 | `		}` |
|      5 | 7944 | `		nMasklen = 0;` |
|      5 | 7945 | `		zMask = ""; /* cc warning */` |
|      5 | 7946 | `		if( nArg > 0 ){` |
|      - | 7947 | `			/* Extract the mask */` |
|      5 | 7948 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 7949 | `		}` |
|      5 | 7950 | `		if( nMasklen < 1 ){` |
|      - | 7951 | `			/* Invalid mask,return FALSE */` |
|    ! 0 | 7952 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 7953 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7954 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 7955 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7956 | `			return PH7_OK;` |
|      - | 7957 | `		}` |
|      - | 7958 | `		/* Extract the token */` |
|      5 | 7959 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 7960 | `		if( rc != SXRET_OK ){` |
|      - | 7961 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 7962 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 7963 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7964 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 7965 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7966 | `		}else{` |
|      - | 7967 | `			/* Return the extracted token */` |
|      5 | 7968 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 7969 | `		}` |
|      3 | 7970 | `	}else{` |
|      - | 7971 | `		const char *zInput,*zCur;` |
|      - | 7972 | `		char *zDup;` |
|      - | 7973 | `		int nLen;` |
|      - | 7974 | `		/* Extract the raw input */` |
|      3 | 7975 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 7976 | `		if( nLen < 1 ){` |
|      - | 7977 | `			/* Empty input,return FALSE */` |
|    ! 0 | 7978 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7979 | `			return PH7_OK;` |
|      - | 7980 | `		}` |
|      - | 7981 | `		/* Extract the mask */` |
|      3 | 7982 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 7983 | `		if( nMasklen < 1 ){` |
|      - | 7984 | `			/* Set a default mask */` |
|      - | 7985 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 7986 | `			zMask = TOK_MASK;` |
|    ! 0 | 7987 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 7988 | `#undef TOK_MASK` |
|    ! 0 | 7989 | `		}` |
|      - | 7990 | `		/* Extract a single token */` |
|      3 | 7991 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 7992 | `		if( rc != SXRET_OK ){` |
|      - | 7993 | `			/* Empty input */` |
|    ! 0 | 7994 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7995 | `			return PH7_OK;` |
|    ! 0 | 7996 | `		}else{` |
|      - | 7997 | `			/* Return the extracted token */` |
|      3 | 7998 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 7999 | `		}` |
|      - | 8000 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 8001 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 8002 | `		if( pAux ){` |
|      3 | 8003 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 8004 | `			if( nLen < 1 ){` |
|    ! 0 | 8005 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 8006 | `				return PH7_OK;` |
|      - | 8007 | `			}` |
|      - | 8008 | `			/* Duplicate input */` |
|      3 | 8009 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 8010 | `			if( zDup  ){` |
|      3 | 8011 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 8012 | `				/* Register the aux data */` |
|      3 | 8013 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 8014 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 8015 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 8016 | `			}` |
|      1 | 8017 | `		}` |
|      - | 8018 | `	}` |
|      7 | 8019 | `	return PH7_OK;` |
|      4 | 8020 | `}` |
|      - | 8021 | `/*` |
|      - | 8022 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 8023 | ` *  Pad a string to a certain length with another string` |
|      - | 8024 | ` * Parameters` |
|      - | 8025 | ` *  $input` |
|      - | 8026 | ` *   The input string.` |
|      - | 8027 | ` * $pad_length` |
|      - | 8028 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 8029 | ` *   string, no padding takes place.` |
|      - | 8030 | ` * $pad_string` |
|      - | 8031 | ` *   Note:` |
|      - | 8032 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 8033 | ` *    divided by the pad_string's length.` |
|      - | 8034 | ` * $pad_type` |
|      - | 8035 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 8036 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 8037 | ` * Return` |
|      - | 8038 | ` *  The padded string.` |
|      - | 8039 | ` */` |
|     10 | 8040 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8041 | `{` |
|      - | 8042 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 8043 | `	const char *zIn,*zPad;` |
|     11 | 8044 | `	if( nArg < 2 ){` |
|      - | 8045 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 8046 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 8047 | `		return PH7_OK;` |
|      - | 8048 | `	}` |
|      - | 8049 | `	/* Extract the target string */` |
|     11 | 8050 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 8051 | `	/* Padding length */` |
|      - | 8052 | `	{` |
|     11 | 8053 | `		sxi64 iTmp = 0;` |
|     11 | 8054 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_pad",2,"$length","int",&iTmp);` |
|     11 | 8055 | `		if( rcArg != PH7_OK ){` |
|    ! 0 | 8056 | `			return rcArg;` |
|      - | 8057 | `		}` |
|     11 | 8058 | `		iRealPad = iPadlen = (int)iTmp;` |
|      - | 8059 | `	}` |
|     11 | 8060 | `	if( iPadlen > 0 ){` |
|      9 | 8061 | `		iPadlen -= iLen;` |
|      4 | 8062 | `	}` |
|     11 | 8063 | `	if( iPadlen < 1  ){` |
|      - | 8064 | `		/* Return the string verbatim */` |
|      5 | 8065 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      5 | 8066 | `		return PH7_OK;` |
|      - | 8067 | `	}` |
|      7 | 8068 | `	zPad = " "; /* Whitespace padding */` |
|      7 | 8069 | `	iStrpad = (int)sizeof(char);` |
|      7 | 8070 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      7 | 8071 | `	if( nArg > 2 ){` |
|      - | 8072 | `		/* Padding string */` |
|      7 | 8073 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      7 | 8074 | `		if( iStrpad < 1 ){` |
|      - | 8075 | `			/* An empty pad string throws a catchable ValueError in PHP 8` |
|      - | 8076 | `			 * (only reached once padding is actually required). */` |
|      3 | 8077 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 8078 | `				"str_pad(): Argument #3 ($pad_string) must not be empty");` |
|      - | 8079 | `		}` |
|      5 | 8080 | `		if( nArg > 3 ){` |
|      - | 8081 | `			/* Padd type */` |
|      5 | 8082 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 8083 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 8084 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 8085 | `			}` |
|      2 | 8086 | `		}` |
|      2 | 8087 | `	}` |
|      5 | 8088 | `	iDiv = 1;` |
|      5 | 8089 | `	if( iType == 2 ){` |
|    ! 0 | 8090 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 8091 | `	}` |
|      - | 8092 | `	/* Perform the requested operation */` |
|      5 | 8093 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 8094 | `		jPad = iStrpad;` |
|      5 | 8095 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 8096 | `			/* Padding */` |
|      5 | 8097 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 8098 | `				break;` |
|      - | 8099 | `			}` |
|      3 | 8100 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 8101 | `		}` |
|      3 | 8102 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 8103 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 8104 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 8105 | `				if( jPad > iStrpad ){` |
|    ! 0 | 8106 | `					jPad = iStrpad;` |
|    ! 0 | 8107 | `				}` |
|      3 | 8108 | `				if( jPad < 1){` |
|    ! 0 | 8109 | `					break;` |
|      - | 8110 | `				}` |
|      3 | 8111 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 8112 | `			}` |
|      1 | 8113 | `		}` |
|      1 | 8114 | `	}` |
|      5 | 8115 | `	if( iLen > 0 ){` |
|      - | 8116 | `		/* Append the input string */` |
|      5 | 8117 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 8118 | `	}` |
|      5 | 8119 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 8120 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 8121 | `			/* Padding */` |
|      5 | 8122 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 8123 | `				break;` |
|      - | 8124 | `			}` |
|      3 | 8125 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 8126 | `		}` |
|      5 | 8127 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 8128 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 8129 | `			if( jPad > iStrpad ){` |
|    ! 0 | 8130 | `				jPad = iStrpad;` |
|    ! 0 | 8131 | `			}` |
|      3 | 8132 | `			if( jPad < 1){` |
|    ! 0 | 8133 | `				break;` |
|      - | 8134 | `			}` |
|      3 | 8135 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 8136 | `		}` |
|      1 | 8137 | `	}` |
|      5 | 8138 | `	return PH7_OK;` |
|      6 | 8139 | `}` |
|      - | 8140 | `/*` |
|      - | 8141 | ` * String replacement private data.` |
|      - | 8142 | ` */` |
|      - | 8143 | `typedef struct str_replace_data str_replace_data;` |
|      - | 8144 | `struct str_replace_data` |
|      - | 8145 | `{` |
|      - | 8146 | `	/* Used by the str_replace family to collect the search/replace arguments. */` |
|      - | 8147 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 8148 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 8149 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 8150 | `};` |
|      - | 8151 | `/*` |
|      - | 8152 | ` * Remove a substring.` |
|      - | 8153 | ` */` |
|      - | 8154 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 8155 | `	for(;;){\` |
|      - | 8156 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 8157 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 8158 | `		++OFFT;\` |
|      - | 8159 | `	}\` |
|      - | 8160 | `}` |
|      - | 8161 | `/*` |
|      - | 8162 | ` * Shift right and insert algorithm.` |
|      - | 8163 | ` */` |
|      - | 8164 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 8165 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 8166 | `		for(;;){\` |
|      - | 8167 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 8168 | `			if(INLEN < 1 ) { break; }\` |
|      - | 8169 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 8170 | `			--INLEN; \` |
|      - | 8171 | `		}\` |
|      - | 8172 | `		for(;;){\` |
|      - | 8173 | `				if(ELEN < 1) { break; }\` |
|      - | 8174 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 8175 | `				OFFT++;\` |
|      - | 8176 | `				ENTRY++;\` |
|      - | 8177 | `				--ELEN;\` |
|      - | 8178 | `		}\` |
|      - | 8179 | `}` |
|      - | 8180 | `/*` |
|      - | 8181 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 8182 | ` * replacement string [i.e: zReplace].` |
|      - | 8183 | ` */` |
|     52 | 8184 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      5 | 8185 | `{` |
|     57 | 8186 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 8187 | `	sxu32 n,m;` |
|     57 | 8188 | `	n = SyBlobLength(pWorker);` |
|     57 | 8189 | `	m = nOfft;` |
|      - | 8190 | `	/* Delete the old entry */` |
|   6591 | 8191 | `	STRDEL(zInput,n,m,nLen);` |
|     57 | 8192 | `	SyBlobLength(pWorker) -= nLen;` |
|     57 | 8193 | `	if( nReplen > 0 ){` |
|     51 | 8194 | `		sxi32 iRep = nReplen;` |
|      - | 8195 | `		sxi32 rc;` |
|      - | 8196 | `		/*` |
|      - | 8197 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 8198 | `		 * string.` |
|      - | 8199 | `		 */` |
|     51 | 8200 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     51 | 8201 | `		if( rc != SXRET_OK ){` |
|      - | 8202 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 8203 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 8204 | `			return rc;` |
|      - | 8205 | `		}` |
|      - | 8206 | `		/* Perform the insertion now */` |
|     51 | 8207 | `		zInput = (char *)SyBlobData(pWorker);` |
|     51 | 8208 | `		n = SyBlobLength(pWorker);` |
|   6381 | 8209 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     51 | 8210 | `		SyBlobLength(pWorker) += nReplen;` |
|     23 | 8211 | `	}` |
|     57 | 8212 | `	return SXRET_OK;` |
|     31 | 8213 | `}` |
|      - | 8214 | `/*` |
|      - | 8215 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 8216 | ` * to collect search/replace string.` |
|      - | 8217 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 8218 | ` */` |
|    162 | 8219 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      5 | 8220 | `{` |
|    167 | 8221 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 8222 | `	SyString sWorker;` |
|      - | 8223 | `	const char *zIn;` |
|      - | 8224 | `	int nByte;` |
|      - | 8225 | `	/* Extract a string representation of the given argument */` |
|    167 | 8226 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|    167 | 8227 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|    167 | 8228 | `	if( nByte > 0 ){` |
|      - | 8229 | `		char *zDup;` |
|      - | 8230 | `		/* Duplicate the chunk */` |
|    165 | 8231 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 8232 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 8233 | `			);` |
|    165 | 8234 | `		if( zDup == 0 ){` |
|      - | 8235 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 8236 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 8237 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 8238 | `			return SXERR_MEM;` |
|      - | 8239 | `		}` |
|    165 | 8240 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 8241 | `		/* Save the chunk */` |
|    165 | 8242 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     80 | 8243 | `	}` |
|      - | 8244 | `	/* Save for later processing */` |
|    167 | 8245 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 8246 | `	/* All done */` |
|     81 | 8247 | `	SXUNUSED(pKey); /* cc warning */` |
|    167 | 8248 | `	return PH7_OK;` |
|     86 | 8249 | `}` |
|      - | 8250 | `/*` |
|      - | 8251 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 8252 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 8253 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 8254 | ` * Parameters` |
|      - | 8255 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 8256 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 8257 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 8258 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 8259 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 8260 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 8261 | ` * $search` |
|      - | 8262 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 8263 | ` *  to designate multiple needles.` |
|      - | 8264 | ` * $replace` |
|      - | 8265 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 8266 | ` *  to designate multiple replacements.` |
|      - | 8267 | ` * $subject` |
|      - | 8268 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 8269 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 8270 | ` *  of subject, and the return value is an array as well.` |
|      - | 8271 | ` * $count (Not used)` |
|      - | 8272 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 8273 | ` * Return` |
|      - | 8274 | ` * This function returns a string or an array with the replaced values.` |
|      - | 8275 | ` */` |
|  29954 | 8276 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 8277 | `{` |
|      - | 8278 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 8279 | `	ProcStringMatch xMatch;` |
|      - | 8280 | `	const char *zIn,*zFunc;` |
|      - | 8281 | `	str_replace_data sRep;` |
|      - | 8282 | `	SyBlob sWorker;` |
|      - | 8283 | `	SySet sReplace;` |
|      - | 8284 | `	SySet sSearch;` |
|      - | 8285 | `	int rep_str;` |
|      - | 8286 | `	int nByte;` |
|      - | 8287 | `	sxi32 rc;` |
|  29959 | 8288 | `	if( nArg < 3 ){` |
|      - | 8289 | `		/* Missing/Invalid arguments,return null */` |
|    ! 0 | 8290 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8291 | `		return PH7_OK;` |
|      - | 8292 | `	}` |
|      - | 8293 | `	/* Initialize fields */` |
|  29959 | 8294 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  29959 | 8295 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  29959 | 8296 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  29959 | 8297 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  29959 | 8298 | `	sRep.pCtx = pCtx;` |
|  29959 | 8299 | `	sRep.pCollector = &sSearch;` |
|  29959 | 8300 | `	rep_str = 0;` |
|      - | 8301 | `	/* Extract the subject */` |
|  29959 | 8302 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  29959 | 8303 | `	if( nByte < 1 ){` |
|      - | 8304 | `		/* Nothing to replace,return the empty string */` |
|     21 | 8305 | `		ph7_result_string(pCtx,"",0);` |
|     21 | 8306 | `		return PH7_OK;` |
|      - | 8307 | `	}` |
|      - | 8308 | `	/* Copy the subject */` |
|  29939 | 8309 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 8310 | `	/* Search string */` |
|  29939 | 8311 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 8312 | `		/* Collect search string */` |
|     81 | 8313 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|     43 | 8314 | `	}else{` |
|      - | 8315 | `		/* Single pattern */` |
|  29863 | 8316 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  29863 | 8317 | `		if( nByte < 1 ){` |
|      - | 8318 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 8319 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 8320 | `			return PH7_OK;` |
|      - | 8321 | `		}` |
|  29859 | 8322 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 8323 | `		/* Save for later processing */` |
|  29859 | 8324 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 8325 | `	}` |
|      - | 8326 | `	/* Replace string */` |
|  29935 | 8327 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 8328 | `		/* Collect replace string */` |
|      7 | 8329 | `		sRep.pCollector = &sReplace;` |
|      7 | 8330 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 8331 | `	}else{` |
|      - | 8332 | `		/* Single needle */` |
|  29929 | 8333 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  29929 | 8334 | `		rep_str = 1;` |
|  29929 | 8335 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 8336 | `		/* Save for later processing */` |
|  29929 | 8337 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 8338 | `	}` |
|      - | 8339 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  29935 | 8340 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 8341 | `		SySetRelease(&sSearch);` |
|    ! 0 | 8342 | `		SySetRelease(&sReplace);` |
|    ! 0 | 8343 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 8344 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8345 | `	}` |
|      - | 8346 | `	/* Reset loop cursors */` |
|  29935 | 8347 | `	SySetResetCursor(&sSearch);` |
|  29935 | 8348 | `	SySetResetCursor(&sReplace);` |
|  29935 | 8349 | `	pReplace = pSearch = 0; /* cc warning */` |
|  29935 | 8350 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 8351 | `	/* Extract function name */` |
|  29935 | 8352 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 8353 | `	/* Set the default pattern match routine */` |
|  29935 | 8354 | `	xMatch = SyBlobSearch;` |
|  29935 | 8355 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 8356 | `		/* Case insensitive pattern match */` |
|     11 | 8357 | `		xMatch = iPatternMatch;` |
|      5 | 8358 | `	}` |
|      - | 8359 | `	/* Start the replace process */` |
|  59941 | 8360 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 8361 | `		sxu32 nCount,nOfft;` |
|  30011 | 8362 | `		if( pSearch->nByte <  1 ){` |
|      - | 8363 | `			/* Empty string,ignore */` |
|      3 | 8364 | `			continue;` |
|      - | 8365 | `		}` |
|      - | 8366 | `		/* Extract the replace string */` |
|  30009 | 8367 | `		if( rep_str ){` |
|  29999 | 8368 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  15002 | 8369 | `		}else{` |
|     11 | 8370 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 8371 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 8372 | `				 * An empty string is used for the rest of replacement values` |
|      - | 8373 | `				 */` |
|      3 | 8374 | `				pReplace = 0;` |
|      1 | 8375 | `			}` |
|      - | 8376 | `		}` |
|  30009 | 8377 | `		if( pReplace == 0 ){` |
|      - | 8378 | `			/* Use an empty string instead */` |
|      3 | 8379 | `			pReplace = &sTemp;` |
|      1 | 8380 | `		}` |
|  30009 | 8381 | `		nOfft = nCount = 0;` |
|  15028 | 8382 | `		for(;;){` |
|  30061 | 8383 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 8384 | `				break;` |
|      - | 8385 | `			}` |
|      - | 8386 | `			/* Perform a pattern lookup */` |
|  45071 | 8387 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  30044 | 8388 | `				pSearch->nByte,&nOfft);` |
|  30049 | 8389 | `			if( rc != SXRET_OK ){` |
|      - | 8390 | `				/* Pattern not found */` |
|  29997 | 8391 | `				break;` |
|      - | 8392 | `			}` |
|      - | 8393 | `			/* Perform the replace operation */` |
|     57 | 8394 | `			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|     57 | 8395 | `			if( rc != SXRET_OK ){` |
|      - | 8396 | `				/* Allocation failure: surface a fatal instead of a partial result */` |
|    ! 0 | 8397 | `				SySetRelease(&sSearch);` |
|    ! 0 | 8398 | `				SySetRelease(&sReplace);` |
|    ! 0 | 8399 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8400 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8401 | `			}` |
|      - | 8402 | `			/* Increment offset counter */` |
|     57 | 8403 | `			nCount += nOfft + pReplace->nByte;` |
|      5 | 8404 | `		}` |
|      5 | 8405 | `	}` |
|      - | 8406 | `	/* All done,clean-up the mess left behind */` |
|  29935 | 8407 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  29935 | 8408 | `	SySetRelease(&sSearch);` |
|  29935 | 8409 | `	SySetRelease(&sReplace);` |
|  29935 | 8410 | `	SyBlobRelease(&sWorker);` |
|  29935 | 8411 | `	if( rc != PH7_OK ){` |
|    ! 0 | 8412 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8413 | `	}` |
|  29935 | 8414 | `	return PH7_OK;` |
|  14982 | 8415 | `}` |
|      - | 8416 | `/*` |
|      - | 8417 | ` * strtr() array form: a single (key,value) pair copied out of the replace_pairs` |
|      - | 8418 | ` * array. The bytes are owned by a persistent pool (see strtr_collect) rather than` |
|      - | 8419 | ` * the transient walker values, which HashmapWalk releases after each callback, so` |
|      - | 8420 | ` * we store byte offsets into that pool instead of raw pointers.` |
|      - | 8421 | ` */` |
|      - | 8422 | `typedef struct strtr_entry strtr_entry;` |
|      - | 8423 | `struct strtr_entry` |
|      - | 8424 | `{` |
|      - | 8425 | `	sxu32 nKeyOfft; /* Offset of the search key inside the pool */` |
|      - | 8426 | `	sxu32 nKeyLen;  /* Length of the search key */` |
|      - | 8427 | `	sxu32 nValOfft; /* Offset of the replacement inside the pool */` |
|      - | 8428 | `	sxu32 nValLen;  /* Length of the replacement */` |
|      - | 8429 | `};` |
|      - | 8430 | `typedef struct strtr_collect strtr_collect;` |
|      - | 8431 | `struct strtr_collect` |
|      - | 8432 | `{` |
|      - | 8433 | `	SyBlob *pPool;  /* Byte pool holding copied key + value bytes */` |
|      - | 8434 | `	SySet  *pTable; /* Set of strtr_entry (parallel offsets into pPool) */` |
|      - | 8435 | `	sxi32   rc;     /* Carries an allocation failure (SXERR_MEM) out of the walker */` |
|      - | 8436 | `};` |
|      - | 8437 | `/*` |
|      - | 8438 | ` * Collect one replace_pairs entry into the persistent pool/offset table.` |
|      - | 8439 | ` * PHP coerces both the key and the value to string (an integer key becomes its` |
|      - | 8440 | ` * decimal form) and ignores an empty-string key.` |
|      - | 8441 | ` */` |
|     20 | 8442 | `static int StrtrCollectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 8443 | `{` |
|     21 | 8444 | `	strtr_collect *pCol = (strtr_collect *)pUserData;` |
|      - | 8445 | `	const char *zKey,*zVal;` |
|      - | 8446 | `	strtr_entry sEnt;` |
|      - | 8447 | `	int nKey,nVal;` |
|     21 | 8448 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|     21 | 8449 | `	if( nKey < 1 ){` |
|      - | 8450 | `		/* PHP ignores an empty-string key (it also emits a warning we do not replicate). */` |
|      3 | 8451 | `		return PH7_OK;` |
|      - | 8452 | `	}` |
|     19 | 8453 | `	zVal = ph7_value_to_string(pData,&nVal);` |
|     19 | 8454 | `	sEnt.nKeyOfft = SyBlobLength(pCol->pPool);` |
|     19 | 8455 | `	sEnt.nKeyLen  = (sxu32)nKey;` |
|     19 | 8456 | `	if( SyBlobAppend(pCol->pPool,(const void *)zKey,(sxu32)nKey) != SXRET_OK ){` |
|    ! 0 | 8457 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8458 | `		return SXERR_ABORT;` |
|      - | 8459 | `	}` |
|     19 | 8460 | `	sEnt.nValOfft = SyBlobLength(pCol->pPool);` |
|     19 | 8461 | `	sEnt.nValLen  = (sxu32)nVal;` |
|     19 | 8462 | `	if( nVal > 0 && SyBlobAppend(pCol->pPool,(const void *)zVal,(sxu32)nVal) != SXRET_OK ){` |
|    ! 0 | 8463 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8464 | `		return SXERR_ABORT;` |
|      - | 8465 | `	}` |
|     19 | 8466 | `	if( SySetPut(pCol->pTable,(const void *)&sEnt) != SXRET_OK ){` |
|    ! 0 | 8467 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8468 | `		return SXERR_ABORT;` |
|      - | 8469 | `	}` |
|     19 | 8470 | `	return PH7_OK;` |
|     11 | 8471 | `}` |
|      - | 8472 | `/*` |
|      - | 8473 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 8474 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 8475 | ` *  Translate characters or replace substrings.` |
|      - | 8476 | ` * Parameters` |
|      - | 8477 | ` *  $str` |
|      - | 8478 | ` *  The string being translated.` |
|      - | 8479 | ` * $from` |
|      - | 8480 | ` *  The string being translated to to.` |
|      - | 8481 | ` * $to` |
|      - | 8482 | ` *  The string replacing from.` |
|      - | 8483 | ` * $replace_pairs` |
|      - | 8484 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 8485 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 8486 | ` * Return` |
|      - | 8487 | ` *  The translated string.` |
|      - | 8488 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 8489 | ` */` |
|     12 | 8490 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8491 | `{` |
|      - | 8492 | `	const char *zIn;` |
|      - | 8493 | `	int nLen;` |
|     13 | 8494 | `	if( nArg < 1 ){` |
|      - | 8495 | `		/* Nothing to replace,return FALSE */` |
|    ! 0 | 8496 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8497 | `		return PH7_OK;` |
|      - | 8498 | `	}` |
|     13 | 8499 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 8500 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 8501 | `		/* Invalid arguments */` |
|    ! 0 | 8502 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8503 | `		return PH7_OK;` |
|      - | 8504 | `	}` |
|     18 | 8505 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 8506 | `		strtr_collect sCol;` |
|      - | 8507 | `		SyBlob sPool,sWorker;` |
|      - | 8508 | `		SySet sTable;` |
|      - | 8509 | `		const char *zPool;` |
|      - | 8510 | `		strtr_entry *pEnt;` |
|      - | 8511 | `		sxi32 rc;` |
|      - | 8512 | `		int i,iRun;` |
|      - | 8513 | `		/*` |
|      - | 8514 | `		 * PHP's array-form strtr is a single left-to-right pass over the subject:` |
|      - | 8515 | `		 * at every position it substitutes the LONGEST replace_pairs key that` |
|      - | 8516 | `		 * matches there, then advances past the key (replacements are never` |
|      - | 8517 | `		 * rescanned). It is not a sequential per-key global replace. First copy` |
|      - | 8518 | `		 * the pairs into a persistent pool, then run that scan.` |
|      - | 8519 | `		 */` |
|     11 | 8520 | `		SyBlobInit(&sPool,&pCtx->pVm->sAllocator);` |
|     11 | 8521 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     11 | 8522 | `		SySetInit(&sTable,&pCtx->pVm->sAllocator,sizeof(strtr_entry));` |
|     11 | 8523 | `		sCol.pPool  = &sPool;` |
|     11 | 8524 | `		sCol.pTable = &sTable;` |
|     11 | 8525 | `		sCol.rc     = SXRET_OK;` |
|     11 | 8526 | `		ph7_array_walk(apArg[1],StrtrCollectWalker,&sCol);` |
|     11 | 8527 | `		if( sCol.rc != SXRET_OK ){` |
|      - | 8528 | `			/* Allocation failure while collecting the pairs: surface a fatal */` |
|    ! 0 | 8529 | `			SyBlobRelease(&sPool);` |
|    ! 0 | 8530 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 8531 | `			SySetRelease(&sTable);` |
|    ! 0 | 8532 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 8533 | `		}` |
|      - | 8534 | `		/* The pool is now stable, so offsets can be resolved against its base. */` |
|     11 | 8535 | `		zPool = (const char *)SyBlobData(&sPool);` |
|     11 | 8536 | `		rc = SXRET_OK;` |
|     11 | 8537 | `		iRun = 0; /* Start of the pending run of unmatched bytes copied verbatim. */` |
|     43 | 8538 | `		for( i = 0 ; i < nLen ; ){` |
|     33 | 8539 | `			strtr_entry *pBest = 0;` |
|     33 | 8540 | `			sxu32 nBest = 0;` |
|      - | 8541 | `			/* Pick the longest key that matches at the current position. */` |
|     33 | 8542 | `			SySetResetCursor(&sTable);` |
|     87 | 8543 | `			while( SXRET_OK == SySetGetNextEntry(&sTable,(void **)&pEnt) ){` |
|     54 | 8544 | `				if( pEnt->nKeyLen > nBest` |
|     50 | 8545 | `					&& pEnt->nKeyLen <= (sxu32)(nLen - i)` |
|     46 | 8546 | `					&& SyMemcmp(zPool + pEnt->nKeyOfft,zIn + i,pEnt->nKeyLen) == 0 ){` |
|     29 | 8547 | `					nBest = pEnt->nKeyLen;` |
|     29 | 8548 | `					pBest = pEnt;` |
|     14 | 8549 | `				}` |
|      1 | 8550 | `			}` |
|     33 | 8551 | `			if( pBest == 0 ){` |
|      - | 8552 | `				/* No key here: extend the literal run and copy it in one shot later. */` |
|      9 | 8553 | `				i++;` |
|      9 | 8554 | `				continue;` |
|      - | 8555 | `			}` |
|      - | 8556 | `			/* Flush the pending literal run, then the replacement. */` |
|     25 | 8557 | `			if( i > iRun ){` |
|      5 | 8558 | `				rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(i - iRun));` |
|      2 | 8559 | `			}` |
|     25 | 8560 | `			if( rc == SXRET_OK && pBest->nValLen > 0 ){` |
|     25 | 8561 | `				rc = SyBlobAppend(&sWorker,zPool + pBest->nValOfft,pBest->nValLen);` |
|     12 | 8562 | `			}` |
|     25 | 8563 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8564 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 8565 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8566 | `				SySetRelease(&sTable);` |
|    ! 0 | 8567 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8568 | `			}` |
|     25 | 8569 | `			i += (int)pBest->nKeyLen;` |
|     25 | 8570 | `			iRun = i;` |
|      1 | 8571 | `		}` |
|      - | 8572 | `		/* Flush the trailing literal run. */` |
|     11 | 8573 | `		if( nLen > iRun ){` |
|      3 | 8574 | `			rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(nLen - iRun));` |
|      3 | 8575 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8576 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 8577 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8578 | `				SySetRelease(&sTable);` |
|    ! 0 | 8579 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8580 | `			}` |
|      1 | 8581 | `		}` |
|      - | 8582 | `		/* All done, return the result string */` |
|     16 | 8583 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|     10 | 8584 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 8585 | `		/* Clean-up */` |
|     11 | 8586 | `		SyBlobRelease(&sPool);` |
|     11 | 8587 | `		SyBlobRelease(&sWorker);` |
|     11 | 8588 | `		SySetRelease(&sTable);` |
|     11 | 8589 | `		if( rc != PH7_OK ){` |
|    ! 0 | 8590 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 8591 | `		}` |
|      6 | 8592 | `	}else{` |
|      - | 8593 | `		int i,flen,tlen,c,iOfft;` |
|      - | 8594 | `		const char *zFrom,*zTo;` |
|      3 | 8595 | `		if( nArg < 3 ){` |
|      - | 8596 | `			/* Nothing to replace */` |
|    ! 0 | 8597 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8598 | `			return PH7_OK;` |
|      - | 8599 | `		}` |
|      - | 8600 | `		/* Extract given arguments */` |
|      3 | 8601 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 8602 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 8603 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 8604 | `			/* Nothing to replace */` |
|    ! 0 | 8605 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8606 | `			return PH7_OK;` |
|      - | 8607 | `		}` |
|      - | 8608 | `		/* Start the replace process */` |
|     13 | 8609 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 8610 | `			c = zIn[i];` |
|     11 | 8611 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 8612 | `				if ( iOfft < tlen ){` |
|      5 | 8613 | `					c = zTo[iOfft];` |
|      2 | 8614 | `				}` |
|      2 | 8615 | `			}` |
|     11 | 8616 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 8617 |  |
|      6 | 8618 | `		}` |
|      - | 8619 | `	}` |
|     13 | 8620 | `	return PH7_OK;` |
|      7 | 8621 | `}` |
|      - | 8622 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8623 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8624 | `/*` |
|      - | 8625 | ` * Parse an INI string.` |
|      - | 8626 |  |
|      - | 8627 | ` * According to wikipedia` |
|      - | 8628 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 8629 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 8630 | ` *  Format` |
|      - | 8631 | `*    Properties` |
|      - | 8632 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 8633 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 8634 | `*     Example:` |
|      - | 8635 | `*      name=value` |
|      - | 8636 | `*    Sections` |
|      - | 8637 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 8638 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 8639 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 8640 | `*     or the end of the file. Sections may not be nested.` |
|      - | 8641 | `*     Example:` |
|      - | 8642 | `*      [section]` |
|      - | 8643 | `*   Comments` |
|      - | 8644 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 8645 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 8646 | `*/` |
|     12 | 8647 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 8648 | `{` |
|      - | 8649 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 8650 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 8651 | `	SyHashEntry *pEntry;` |
|      - | 8652 | `	SyString sEntry;` |
|      - | 8653 | `	SyHash sHash;` |
|      - | 8654 | `	int c;` |
|      - | 8655 | `	/* Create an empty array and worker variables */` |
|     13 | 8656 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 8657 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 8658 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 8659 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 8660 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|    ! 0 | 8661 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8662 | `	}` |
|     13 | 8663 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 8664 | `	pCur = pArray;` |
|      - | 8665 | `	/* Start the parse process */` |
|     21 | 8666 | `	for(;;){` |
|      - | 8667 | `		/* Ignore leading white spaces */` |
|     69 | 8668 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 8669 | `			zIn++;` |
|      1 | 8670 | `		}` |
|     43 | 8671 | `		if( zIn >= zEnd ){` |
|      - | 8672 | `			/* No more input to process */` |
|     13 | 8673 | `			break;` |
|      - | 8674 | `		}` |
|     31 | 8675 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 8676 | `			/* Comment til the end of line */` |
|    ! 0 | 8677 | `			zIn++;` |
|    ! 0 | 8678 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 8679 | `				zIn++;` |
|    ! 0 | 8680 | `			}` |
|    ! 0 | 8681 | `			continue;` |
|      - | 8682 | `		}` |
|      - | 8683 | `		/* Reset the string cursor of the working variable */` |
|     31 | 8684 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 8685 | `		if( zIn[0] == '[' ){` |
|      - | 8686 | `			/* Section: Extract the section name */` |
|      9 | 8687 | `			zIn++;` |
|      9 | 8688 | `			zCur = zIn;` |
|     73 | 8689 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 8690 | `				zIn++;` |
|      1 | 8691 | `			}` |
|      9 | 8692 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 8693 | `				/* Save the section name */` |
|      5 | 8694 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 8695 | `				SyStringFullTrim(&sEntry);` |
|      5 | 8696 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 8697 | `				if( sEntry.nByte > 0 ){` |
|      - | 8698 | `					/* Associate an array with the section */` |
|      5 | 8699 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 8700 | `					if( pSection ){` |
|      5 | 8701 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 8702 | `						pCur = pSection;` |
|      2 | 8703 | `					}` |
|      2 | 8704 | `				}` |
|      2 | 8705 | `			}` |
|      9 | 8706 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 8707 | `		}else{` |
|      - | 8708 | `			ph7_value *pOldCur;` |
|      - | 8709 | `			int is_array;` |
|      - | 8710 | `			int iLen;` |
|      - | 8711 | `			/* Properties */` |
|     23 | 8712 | `			is_array = 0;` |
|     23 | 8713 | `			zCur = zIn;` |
|     23 | 8714 | `			iLen = 0; /* cc warning */` |
|     23 | 8715 | `			pOldCur = pCur;` |
|    155 | 8716 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 8717 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 8718 | `					/* Array */` |
|    ! 0 | 8719 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 8720 | `					is_array = 1;` |
|    ! 0 | 8721 | `					if( iLen > 0 ){` |
|    ! 0 | 8722 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 8723 | `						/* Query the hashtable */` |
|    ! 0 | 8724 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 8725 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 8726 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 8727 | `						if( pEntry ){` |
|    ! 0 | 8728 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 8729 | `						}else{` |
|      - | 8730 | `							/* Create an empty array */` |
|    ! 0 | 8731 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 8732 | `							if( pvArr ){` |
|      - | 8733 | `								/* Save the entry */` |
|    ! 0 | 8734 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 8735 | `								/* Insert the entry */` |
|    ! 0 | 8736 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 8737 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 8738 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 8739 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 8740 | `							}` |
|      - | 8741 | `						}` |
|    ! 0 | 8742 | `						if( pvArr ){` |
|    ! 0 | 8743 | `							pCur = pvArr;` |
|    ! 0 | 8744 | `						}` |
|    ! 0 | 8745 | `					}` |
|    ! 0 | 8746 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 8747 | `						zIn++;` |
|    ! 0 | 8748 | `					}` |
|    ! 0 | 8749 | `				}` |
|    133 | 8750 | `				zIn++;` |
|      1 | 8751 | `			}` |
|     23 | 8752 | `			if( !is_array ){` |
|     23 | 8753 | `				iLen = (int)(zIn-zCur);` |
|     11 | 8754 | `			}` |
|      - | 8755 | `			/* Trim the key */` |
|     23 | 8756 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 8757 | `			SyStringFullTrim(&sEntry);` |
|     23 | 8758 | `			if( sEntry.nByte > 0 ){` |
|     23 | 8759 | `				if( !is_array ){` |
|      - | 8760 | `					/* Save the key name */` |
|     23 | 8761 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 8762 | `				}` |
|      - | 8763 | `				/* extract key value */` |
|     23 | 8764 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 8765 | `				zIn++; /* '=' */` |
|     39 | 8766 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 8767 | `					zIn++;` |
|      1 | 8768 | `				}` |
|     23 | 8769 | `				if( zIn < zEnd ){` |
|     21 | 8770 | `					zCur = zIn;` |
|     21 | 8771 | `					c = zIn[0];` |
|     21 | 8772 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 8773 | `						zIn++;` |
|      - | 8774 | `						/* Delimit the value */` |
|    ! 0 | 8775 | `						while( zIn < zEnd ){` |
|    ! 0 | 8776 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 8777 | `								break;` |
|      - | 8778 | `							}` |
|    ! 0 | 8779 | `							zIn++;` |
|    ! 0 | 8780 | `						}` |
|    ! 0 | 8781 | `						if( zIn < zEnd ){` |
|    ! 0 | 8782 | `							zIn++;` |
|    ! 0 | 8783 | `						}` |
|    ! 0 | 8784 | `					}else{` |
|    125 | 8785 | `						while( zIn < zEnd ){` |
|    123 | 8786 | `							if( zIn[0] == '\n' ){` |
|     19 | 8787 | `								if( zIn[-1] != '\\' ){` |
|     19 | 8788 | `									break;` |
|    ! 0 | 8789 | `								}` |
|    105 | 8790 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 8791 | `								/* Inline comments */` |
|    ! 0 | 8792 | `								break;` |
|      - | 8793 | `							}` |
|    105 | 8794 | `							zIn++;` |
|      1 | 8795 | `						}` |
|      - | 8796 | `					}` |
|      - | 8797 | `					/* Trim the value */` |
|     21 | 8798 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 8799 | `					SyStringFullTrim(&sEntry);` |
|     21 | 8800 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 8801 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 8802 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 8803 | `					}` |
|     21 | 8804 | `					if( sEntry.nByte > 0 ){` |
|     21 | 8805 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 8806 | `					}` |
|      - | 8807 | `					/* Insert the key and it's value */` |
|     21 | 8808 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 8809 | `				}` |
|     12 | 8810 | `			}else{` |
|    ! 0 | 8811 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 8812 | `					zIn++;` |
|    ! 0 | 8813 | `				}` |
|      - | 8814 | `			}` |
|     23 | 8815 | `			pCur = pOldCur;` |
|      - | 8816 | `		}` |
|      1 | 8817 | `	}` |
|     13 | 8818 | `	SyHashRelease(&sHash);` |
|      - | 8819 | `	/* Return the parse of the INI string */` |
|     13 | 8820 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 8821 | `	return SXRET_OK;` |
|      7 | 8822 | `}` |
|      - | 8823 | `/*` |
|      - | 8824 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 8825 | ` *  Parse a configuration string.` |
|      - | 8826 | ` * Parameters` |
|      - | 8827 | ` *  $ini` |
|      - | 8828 | ` *   The contents of the ini file being parsed.` |
|      - | 8829 | ` *  $process_sections` |
|      - | 8830 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 8831 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 8832 | ` *  $scanner_mode (Not used)` |
|      - | 8833 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 8834 | ` *   then option values will not be parsed.` |
|      - | 8835 | ` * Return` |
|      - | 8836 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 8837 | ` */` |
|     10 | 8838 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8839 | `{` |
|      - | 8840 | `	const char *zIni;` |
|      - | 8841 | `	int nByte;` |
|     11 | 8842 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8843 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 8844 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8845 | `		return PH7_OK;` |
|      - | 8846 | `	}` |
|      - | 8847 | `	/* Extract the raw INI buffer */` |
|     11 | 8848 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 8849 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|     11 | 8850 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      6 | 8851 | `}` |
|      - | 8852 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8853 |  |
|      - | 8854 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8855 |  |
|      - | 8856 | `/*` |
|      - | 8857 | ` * Ctype Functions.` |
|      - | 8858 | ` * Status:` |
|      - | 8859 | ` *    Stable.` |
|      - | 8860 | ` */` |
|      - | 8861 | `/*` |
|      - | 8862 | ` * bool ctype_alnum(string $text)` |
|      - | 8863 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 8864 | ` * Parameters` |
|      - | 8865 | ` *  $text` |
|      - | 8866 | ` *   The tested string.` |
|      - | 8867 | ` * Return` |
|      - | 8868 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 8869 | ` */` |
|     72 | 8870 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8871 | `{` |
|      - | 8872 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8873 | `	int nLen;` |
|     73 | 8874 | `	if( nArg < 1 ){` |
|      - | 8875 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8876 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8877 | `		return PH7_OK;` |
|      - | 8878 | `	}` |
|      - | 8879 | `	/* Extract the target string */` |
|     73 | 8880 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     73 | 8881 | `	zEnd = &zIn[nLen];` |
|     73 | 8882 | `	if( nLen < 1 ){` |
|      - | 8883 | `		/* Empty string,return FALSE */` |
|      3 | 8884 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8885 | `		return PH7_OK;` |
|      - | 8886 | `	}` |
|      - | 8887 | `	/* Perform the requested operation */` |
|    110 | 8888 | `	for(;;){` |
|    221 | 8889 | `		if( zIn >= zEnd ){` |
|      - | 8890 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     65 | 8891 | `			ph7_result_bool(pCtx,1);` |
|     65 | 8892 | `			return PH7_OK;` |
|      - | 8893 | `		}` |
|    157 | 8894 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      7 | 8895 | `			break;` |
|      - | 8896 | `		}` |
|      - | 8897 | `		/* Point to the next character */` |
|    151 | 8898 | `		zIn++;` |
|      1 | 8899 | `	}` |
|      - | 8900 | `	/* The test failed,return FALSE */` |
|      7 | 8901 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8902 | `	return PH7_OK;` |
|     37 | 8903 | `}` |
|      - | 8904 | `/*` |
|      - | 8905 | ` * bool ctype_alpha(string $text)` |
|      - | 8906 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 8907 | ` * Parameters` |
|      - | 8908 | ` *  $text` |
|      - | 8909 | ` *   The tested string.` |
|      - | 8910 | ` * Return` |
|      - | 8911 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 8912 | ` */` |
|     16 | 8913 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8914 | `{` |
|      - | 8915 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8916 | `	int nLen;` |
|     17 | 8917 | `	if( nArg < 1 ){` |
|      - | 8918 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8919 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8920 | `		return PH7_OK;` |
|      - | 8921 | `	}` |
|      - | 8922 | `	/* Extract the target string */` |
|     17 | 8923 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8924 | `	zEnd = &zIn[nLen];` |
|     17 | 8925 | `	if( nLen < 1 ){` |
|      - | 8926 | `		/* Empty string,return FALSE */` |
|      3 | 8927 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8928 | `		return PH7_OK;` |
|      - | 8929 | `	}` |
|      - | 8930 | `	/* Perform the requested operation */` |
|     42 | 8931 | `	for(;;){` |
|     85 | 8932 | `		if( zIn >= zEnd ){` |
|      - | 8933 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8934 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8935 | `			return PH7_OK;` |
|      - | 8936 | `		}` |
|     77 | 8937 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 8938 | `			break;` |
|      - | 8939 | `		}` |
|      - | 8940 | `		/* Point to the next character */` |
|     71 | 8941 | `		zIn++;` |
|      1 | 8942 | `	}` |
|      - | 8943 | `	/* The test failed,return FALSE */` |
|      7 | 8944 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8945 | `	return PH7_OK;` |
|      9 | 8946 | `}` |
|      - | 8947 | `/*` |
|      - | 8948 | ` * bool ctype_cntrl(string $text)` |
|      - | 8949 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 8950 | ` * Parameters` |
|      - | 8951 | ` *  $text` |
|      - | 8952 | ` *   The tested string.` |
|      - | 8953 | ` * Return` |
|      - | 8954 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 8955 | ` */` |
|     16 | 8956 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8957 | `{` |
|      - | 8958 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8959 | `	int nLen;` |
|     17 | 8960 | `	if( nArg < 1 ){` |
|      - | 8961 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8962 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8963 | `		return PH7_OK;` |
|      - | 8964 | `	}` |
|      - | 8965 | `	/* Extract the target string */` |
|     17 | 8966 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8967 | `	zEnd = &zIn[nLen];` |
|     17 | 8968 | `	if( nLen < 1 ){` |
|      - | 8969 | `		/* Empty string,return FALSE */` |
|      3 | 8970 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8971 | `		return PH7_OK;` |
|      - | 8972 | `	}` |
|      - | 8973 | `	/* Perform the requested operation */` |
|     14 | 8974 | `	for(;;){` |
|     29 | 8975 | `		if( zIn >= zEnd ){` |
|      - | 8976 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8977 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8978 | `			return PH7_OK;` |
|      - | 8979 | `		}` |
|     21 | 8980 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8981 | `			/* UTF-8 stream  */` |
|    ! 0 | 8982 | `			break;` |
|      - | 8983 | `		}` |
|     21 | 8984 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 8985 | `			break;` |
|      - | 8986 | `		}` |
|      - | 8987 | `		/* Point to the next character */` |
|     15 | 8988 | `		zIn++;` |
|      1 | 8989 | `	}` |
|      - | 8990 | `	/* The test failed,return FALSE */` |
|      7 | 8991 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8992 | `	return PH7_OK;` |
|      9 | 8993 | `}` |
|      - | 8994 | `/*` |
|      - | 8995 | ` * bool ctype_digit(string $text)` |
|      - | 8996 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 8997 | ` * Parameters` |
|      - | 8998 | ` *  $text` |
|      - | 8999 | ` *   The tested string.` |
|      - | 9000 | ` * Return` |
|      - | 9001 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 9002 | ` */` |
|   2098 | 9003 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 9004 | `{` |
|      - | 9005 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9006 | `	int nLen;` |
|   2103 | 9007 | `	if( nArg < 1 ){` |
|      - | 9008 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9009 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9010 | `		return PH7_OK;` |
|      - | 9011 | `	}` |
|      - | 9012 | `	/* Extract the target string */` |
|   2103 | 9013 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   2103 | 9014 | `	zEnd = &zIn[nLen];` |
|   2103 | 9015 | `	if( nLen < 1 ){` |
|      - | 9016 | `		/* Empty string,return FALSE */` |
|      3 | 9017 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9018 | `		return PH7_OK;` |
|      - | 9019 | `	}` |
|      - | 9020 | `	/* Perform the requested operation */` |
|   1944 | 9021 | `	for(;;){` |
|   3893 | 9022 | `		if( zIn >= zEnd ){` |
|      - | 9023 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1735 | 9024 | `			ph7_result_bool(pCtx,1);` |
|   1735 | 9025 | `			return PH7_OK;` |
|      - | 9026 | `		}` |
|   2163 | 9027 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9028 | `			/* UTF-8 stream  */` |
|    ! 0 | 9029 | `			break;` |
|      - | 9030 | `		}` |
|   2163 | 9031 | `		if( !SyisDigit(zIn[0]) ){` |
|    371 | 9032 | `			break;` |
|      - | 9033 | `		}` |
|      - | 9034 | `		/* Point to the next character */` |
|   1797 | 9035 | `		zIn++;` |
|      5 | 9036 | `	}` |
|      - | 9037 | `	/* The test failed,return FALSE */` |
|    371 | 9038 | `	ph7_result_bool(pCtx,0);` |
|    371 | 9039 | `	return PH7_OK;` |
|   1054 | 9040 | `}` |
|      - | 9041 | `/*` |
|      - | 9042 | ` * bool ctype_xdigit(string $text)` |
|      - | 9043 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 9044 | ` * Parameters` |
|      - | 9045 | ` *  $text` |
|      - | 9046 | ` *   The tested string.` |
|      - | 9047 | ` * Return` |
|      - | 9048 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 9049 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 9050 | ` */` |
|     38 | 9051 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 9052 | `{` |
|      - | 9053 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9054 | `	int nLen;` |
|     40 | 9055 | `	if( nArg < 1 ){` |
|      - | 9056 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9057 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9058 | `		return PH7_OK;` |
|      - | 9059 | `	}` |
|      - | 9060 | `	/* Extract the target string */` |
|     40 | 9061 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     40 | 9062 | `	zEnd = &zIn[nLen];` |
|     40 | 9063 | `	if( nLen < 1 ){` |
|      - | 9064 | `		/* Empty string,return FALSE */` |
|      3 | 9065 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9066 | `		return PH7_OK;` |
|      - | 9067 | `	}` |
|      - | 9068 | `	/* Perform the requested operation */` |
|     76 | 9069 | `	for(;;){` |
|    154 | 9070 | `		if( zIn >= zEnd ){` |
|      - | 9071 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     32 | 9072 | `			ph7_result_bool(pCtx,1);` |
|     32 | 9073 | `			return PH7_OK;` |
|      - | 9074 | `		}` |
|    124 | 9075 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9076 | `			/* UTF-8 stream  */` |
|    ! 0 | 9077 | `			break;` |
|      - | 9078 | `		}` |
|    124 | 9079 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 9080 | `			break;` |
|      - | 9081 | `		}` |
|      - | 9082 | `		/* Point to the next character */` |
|    118 | 9083 | `		zIn++;` |
|      2 | 9084 | `	}` |
|      - | 9085 | `	/* The test failed,return FALSE */` |
|      7 | 9086 | `	ph7_result_bool(pCtx,0);` |
|      7 | 9087 | `	return PH7_OK;` |
|     21 | 9088 | `}` |
|      - | 9089 | `/*` |
|      - | 9090 | ` * bool ctype_graph(string $text)` |
|      - | 9091 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 9092 | ` * Parameters` |
|      - | 9093 | ` *  $text` |
|      - | 9094 | ` *   The tested string.` |
|      - | 9095 | ` * Return` |
|      - | 9096 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 9097 | ` * (no white space), FALSE otherwise.` |
|      - | 9098 | ` */` |
|     16 | 9099 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9100 | `{` |
|      - | 9101 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9102 | `	int nLen;` |
|     17 | 9103 | `	if( nArg < 1 ){` |
|      - | 9104 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9105 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9106 | `		return PH7_OK;` |
|      - | 9107 | `	}` |
|      - | 9108 | `	/* Extract the target string */` |
|     17 | 9109 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9110 | `	zEnd = &zIn[nLen];` |
|     17 | 9111 | `	if( nLen < 1 ){` |
|      - | 9112 | `		/* Empty string,return FALSE */` |
|      3 | 9113 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9114 | `		return PH7_OK;` |
|      - | 9115 | `	}` |
|      - | 9116 | `	/* Perform the requested operation */` |
|     57 | 9117 | `	for(;;){` |
|    115 | 9118 | `		if( zIn >= zEnd ){` |
|      - | 9119 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 9120 | `			ph7_result_bool(pCtx,1);` |
|      9 | 9121 | `			return PH7_OK;` |
|      - | 9122 | `		}` |
|    107 | 9123 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9124 | `			/* UTF-8 stream  */` |
|    ! 0 | 9125 | `			break;` |
|      - | 9126 | `		}` |
|    107 | 9127 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 9128 | `			break;` |
|      - | 9129 | `		}` |
|      - | 9130 | `		/* Point to the next character */` |
|    101 | 9131 | `		zIn++;` |
|      1 | 9132 | `	}` |
|      - | 9133 | `	/* The test failed,return FALSE */` |
|      7 | 9134 | `	ph7_result_bool(pCtx,0);` |
|      7 | 9135 | `	return PH7_OK;` |
|      9 | 9136 | `}` |
|      - | 9137 | `/*` |
|      - | 9138 | ` * bool ctype_print(string $text)` |
|      - | 9139 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 9140 | ` * Parameters` |
|      - | 9141 | ` *  $text` |
|      - | 9142 | ` *   The tested string.` |
|      - | 9143 | ` * Return` |
|      - | 9144 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 9145 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 9146 | ` *  or control function at all.` |
|      - | 9147 | ` */` |
|     16 | 9148 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9149 | `{` |
|      - | 9150 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9151 | `	int nLen;` |
|     17 | 9152 | `	if( nArg < 1 ){` |
|      - | 9153 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9154 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9155 | `		return PH7_OK;` |
|      - | 9156 | `	}` |
|      - | 9157 | `	/* Extract the target string */` |
|     17 | 9158 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9159 | `	zEnd = &zIn[nLen];` |
|     17 | 9160 | `	if( nLen < 1 ){` |
|      - | 9161 | `		/* Empty string,return FALSE */` |
|      3 | 9162 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9163 | `		return PH7_OK;` |
|      - | 9164 | `	}` |
|      - | 9165 | `	/* Perform the requested operation */` |
|     63 | 9166 | `	for(;;){` |
|    127 | 9167 | `		if( zIn >= zEnd ){` |
|      - | 9168 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 9169 | `			ph7_result_bool(pCtx,1);` |
|      9 | 9170 | `			return PH7_OK;` |
|      - | 9171 | `		}` |
|    119 | 9172 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9173 | `			/* UTF-8 stream  */` |
|    ! 0 | 9174 | `			break;` |
|      - | 9175 | `		}` |
|    119 | 9176 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 9177 | `			break;` |
|      - | 9178 | `		}` |
|      - | 9179 | `		/* Point to the next character */` |
|    113 | 9180 | `		zIn++;` |
|      1 | 9181 | `	}` |
|      - | 9182 | `	/* The test failed,return FALSE */` |
|      7 | 9183 | `	ph7_result_bool(pCtx,0);` |
|      7 | 9184 | `	return PH7_OK;` |
|      9 | 9185 | `}` |
|      - | 9186 | `/*` |
|      - | 9187 | ` * bool ctype_punct(string $text)` |
|      - | 9188 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 9189 | ` * Parameters` |
|      - | 9190 | ` *  $text` |
|      - | 9191 | ` *   The tested string.` |
|      - | 9192 | ` * Return` |
|      - | 9193 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 9194 | ` *  digit or blank, FALSE otherwise.` |
|      - | 9195 | ` */` |
|     18 | 9196 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9197 | `{` |
|      - | 9198 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9199 | `	int nLen;` |
|     19 | 9200 | `	if( nArg < 1 ){` |
|      - | 9201 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9202 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9203 | `		return PH7_OK;` |
|      - | 9204 | `	}` |
|      - | 9205 | `	/* Extract the target string */` |
|     19 | 9206 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 9207 | `	zEnd = &zIn[nLen];` |
|     19 | 9208 | `	if( nLen < 1 ){` |
|      - | 9209 | `		/* Empty string,return FALSE */` |
|      3 | 9210 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9211 | `		return PH7_OK;` |
|      - | 9212 | `	}` |
|      - | 9213 | `	/* Perform the requested operation */` |
|     38 | 9214 | `	for(;;){` |
|     77 | 9215 | `		if( zIn >= zEnd ){` |
|      - | 9216 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 9217 | `			ph7_result_bool(pCtx,1);` |
|      9 | 9218 | `			return PH7_OK;` |
|      - | 9219 | `		}` |
|     69 | 9220 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9221 | `			/* UTF-8 stream  */` |
|    ! 0 | 9222 | `			break;` |
|      - | 9223 | `		}` |
|     69 | 9224 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 9225 | `			break;` |
|      - | 9226 | `		}` |
|      - | 9227 | `		/* Point to the next character */` |
|     61 | 9228 | `		zIn++;` |
|      1 | 9229 | `	}` |
|      - | 9230 | `	/* The test failed,return FALSE */` |
|      9 | 9231 | `	ph7_result_bool(pCtx,0);` |
|      9 | 9232 | `	return PH7_OK;` |
|     10 | 9233 | `}` |
|      - | 9234 | `/*` |
|      - | 9235 | ` * bool ctype_space(string $text)` |
|      - | 9236 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 9237 | ` * Parameters` |
|      - | 9238 | ` *  $text` |
|      - | 9239 | ` *   The tested string.` |
|      - | 9240 | ` * Return` |
|      - | 9241 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 9242 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 9243 | ` *  and form feed characters.` |
|      - | 9244 | ` */` |
|  64463 | 9245 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 9246 | `{` |
|      - | 9247 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9248 | `	int nLen;` |
|  64468 | 9249 | `	if( nArg < 1 ){` |
|      - | 9250 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9251 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9252 | `		return PH7_OK;` |
|      - | 9253 | `	}` |
|      - | 9254 | `	/* Extract the target string */` |
|  64468 | 9255 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  64468 | 9256 | `	zEnd = &zIn[nLen];` |
|  64468 | 9257 | `	if( nLen < 1 ){` |
|      - | 9258 | `		/* Empty string,return FALSE */` |
|      3 | 9259 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9260 | `		return PH7_OK;` |
|      - | 9261 | `	}` |
|      - | 9262 | `	/* Perform the requested operation */` |
|  33145 | 9263 | `	for(;;){` |
|  66248 | 9264 | `		if( zIn >= zEnd ){` |
|      - | 9265 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1763 | 9266 | `			ph7_result_bool(pCtx,1);` |
|   1763 | 9267 | `			return PH7_OK;` |
|      - | 9268 | `		}` |
|  64490 | 9269 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9270 | `			/* UTF-8 stream  */` |
|    ! 0 | 9271 | `			break;` |
|      - | 9272 | `		}` |
|  64490 | 9273 | `		if( !SyisSpace(zIn[0]) ){` |
|  62708 | 9274 | `			break;` |
|      - | 9275 | `		}` |
|      - | 9276 | `		/* Point to the next character */` |
|   1787 | 9277 | `		zIn++;` |
|      5 | 9278 | `	}` |
|      - | 9279 | `	/* The test failed,return FALSE */` |
|  62708 | 9280 | `	ph7_result_bool(pCtx,0);` |
|  62708 | 9281 | `	return PH7_OK;` |
|  32260 | 9282 | `}` |
|      - | 9283 | `/*` |
|      - | 9284 | ` * bool ctype_lower(string $text)` |
|      - | 9285 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 9286 | ` * Parameters` |
|      - | 9287 | ` *  $text` |
|      - | 9288 | ` *   The tested string.` |
|      - | 9289 | ` * Return` |
|      - | 9290 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 9291 | ` */` |
|     16 | 9292 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9293 | `{` |
|      - | 9294 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9295 | `	int nLen;` |
|     17 | 9296 | `	if( nArg < 1 ){` |
|      - | 9297 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9298 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9299 | `		return PH7_OK;` |
|      - | 9300 | `	}` |
|      - | 9301 | `	/* Extract the target string */` |
|     17 | 9302 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9303 | `	zEnd = &zIn[nLen];` |
|     17 | 9304 | `	if( nLen < 1 ){` |
|      - | 9305 | `		/* Empty string,return FALSE */` |
|      3 | 9306 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9307 | `		return PH7_OK;` |
|      - | 9308 | `	}` |
|      - | 9309 | `	/* Perform the requested operation */` |
|     27 | 9310 | `	for(;;){` |
|     55 | 9311 | `		if( zIn >= zEnd ){` |
|      - | 9312 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 9313 | `			ph7_result_bool(pCtx,1);` |
|      5 | 9314 | `			return PH7_OK;` |
|      - | 9315 | `		}` |
|     51 | 9316 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 9317 | `			break;` |
|      - | 9318 | `		}` |
|      - | 9319 | `		/* Point to the next character */` |
|     41 | 9320 | `		zIn++;` |
|      1 | 9321 | `	}` |
|      - | 9322 | `	/* The test failed,return FALSE */` |
|     11 | 9323 | `	ph7_result_bool(pCtx,0);` |
|     11 | 9324 | `	return PH7_OK;` |
|      9 | 9325 | `}` |
|      - | 9326 | `/*` |
|      - | 9327 | ` * bool ctype_upper(string $text)` |
|      - | 9328 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 9329 | ` * Parameters` |
|      - | 9330 | ` *  $text` |
|      - | 9331 | ` *   The tested string.` |
|      - | 9332 | ` * Return` |
|      - | 9333 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 9334 | ` */` |
|     16 | 9335 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9336 | `{` |
|      - | 9337 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9338 | `	int nLen;` |
|     17 | 9339 | `	if( nArg < 1 ){` |
|      - | 9340 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9341 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9342 | `		return PH7_OK;` |
|      - | 9343 | `	}` |
|      - | 9344 | `	/* Extract the target string */` |
|     17 | 9345 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9346 | `	zEnd = &zIn[nLen];` |
|     17 | 9347 | `	if( nLen < 1 ){` |
|      - | 9348 | `		/* Empty string,return FALSE */` |
|      3 | 9349 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9350 | `		return PH7_OK;` |
|      - | 9351 | `	}` |
|      - | 9352 | `	/* Perform the requested operation */` |
|     28 | 9353 | `	for(;;){` |
|     57 | 9354 | `		if( zIn >= zEnd ){` |
|      - | 9355 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 9356 | `			ph7_result_bool(pCtx,1);` |
|      5 | 9357 | `			return PH7_OK;` |
|      - | 9358 | `		}` |
|     53 | 9359 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 9360 | `			break;` |
|      - | 9361 | `		}` |
|      - | 9362 | `		/* Point to the next character */` |
|     43 | 9363 | `		zIn++;` |
|      1 | 9364 | `	}` |
|      - | 9365 | `	/* The test failed,return FALSE */` |
|     11 | 9366 | `	ph7_result_bool(pCtx,0);` |
|     11 | 9367 | `	return PH7_OK;` |
|      9 | 9368 | `}` |
|      - | 9369 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 9370 | `/*` |
|      - | 9371 | ` * Section:` |
|      - | 9372 | ` *    URL handling Functions.` |
|      - | 9373 | ` * Status:` |
|      - | 9374 | ` *    Stable.` |
|      - | 9375 | ` */` |
|      - | 9376 | `/*` |
|      - | 9377 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 9378 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 9379 | ` */` |
|   1270 | 9380 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 9381 | `{` |
|      - | 9382 | `	/* Store in the call context result buffer */` |
|   1272 | 9383 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1272 | 9384 | `	return SXRET_OK;` |
|      2 | 9385 | `}` |
|      - | 9386 | `/*` |
|      - | 9387 | ` * string base64_encode(string $data)` |
|      - | 9388 | ` * string convert_uuencode(string $data)` |
|      - | 9389 | ` *  Encodes data with MIME base64` |
|      - | 9390 | ` * Parameter` |
|      - | 9391 | ` *  $data` |
|      - | 9392 | ` *    Data to encode` |
|      - | 9393 | ` * Return` |
|      - | 9394 | ` *  Encoded data or FALSE on failure.` |
|      - | 9395 | ` */` |
|      6 | 9396 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9397 | `{` |
|      - | 9398 | `	const char *zIn;` |
|      - | 9399 | `	int nLen;` |
|      7 | 9400 | `	if( nArg < 1 ){` |
|      - | 9401 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9402 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9403 | `		return PH7_OK;` |
|      - | 9404 | `	}` |
|      - | 9405 | `	/* Extract the input string */` |
|      7 | 9406 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 9407 | `	if( nLen < 1 ){` |
|      - | 9408 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9409 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9410 | `		return PH7_OK;` |
|      - | 9411 | `	}` |
|      - | 9412 | `	/* Perform the BASE64 encoding */` |
|      7 | 9413 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 9414 | `	return PH7_OK;` |
|      4 | 9415 | `}` |
|      - | 9416 | `/*` |
|      - | 9417 | ` * string base64_decode(string $data)` |
|      - | 9418 | ` * string convert_uudecode(string $data)` |
|      - | 9419 | ` *  Decodes data encoded with MIME base64` |
|      - | 9420 | ` * Parameter` |
|      - | 9421 | ` *  $data` |
|      - | 9422 | ` *    Encoded data.` |
|      - | 9423 | ` * Return` |
|      - | 9424 | ` *  Returns the original data or FALSE on failure.` |
|      - | 9425 | ` */` |
|     34 | 9426 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 9427 | `{` |
|      - | 9428 | `	const char *zIn;` |
|      - | 9429 | `	int nLen;` |
|     36 | 9430 | `	if( nArg < 1 ){` |
|      - | 9431 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9432 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9433 | `		return PH7_OK;` |
|      - | 9434 | `	}` |
|      - | 9435 | `	/* Extract the input string */` |
|     36 | 9436 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 9437 | `	if( nLen < 1 ){` |
|      - | 9438 | `		/* php decodes the empty string to the EMPTY STRING, not FALSE (FALSE is reserved` |
|      - | 9439 | `		 * for input that cannot be decoded at all). */` |
|      3 | 9440 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 9441 | `		return PH7_OK;` |
|      - | 9442 | `	}` |
|      - | 9443 | `	/* Perform the BASE64 decoding */` |
|     34 | 9444 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 9445 | `	return PH7_OK;` |
|     19 | 9446 | `}` |
|      - | 9447 | `/*` |
|      - | 9448 | ` * string urlencode(string $str)` |
|      - | 9449 | ` *  URL encoding` |
|      - | 9450 | ` * Parameter` |
|      - | 9451 | ` *  $data` |
|      - | 9452 | ` *   Input string.` |
|      - | 9453 | ` * Return` |
|      - | 9454 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 9455 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 9456 | ` *  encoded as plus (+) signs.` |
|      - | 9457 | ` */` |
|    100 | 9458 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9459 | `{` |
|      - | 9460 | `	const char *zIn;` |
|      - | 9461 | `	int nLen;` |
|    101 | 9462 | `	if( nArg < 1 ){` |
|      - | 9463 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9464 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9465 | `		return PH7_OK;` |
|      - | 9466 | `	}` |
|      - | 9467 | `	/* Extract the input string */` |
|    101 | 9468 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|    101 | 9469 | `	if( nLen < 1 ){` |
|      - | 9470 | `		/* php returns an empty string for empty input, not FALSE */` |
|      3 | 9471 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 9472 | `		return PH7_OK;` |
|      - | 9473 | `	}` |
|      - | 9474 | `	/* Perform the URL encoding */` |
|     99 | 9475 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     99 | 9476 | `	return PH7_OK;` |
|     51 | 9477 | `}` |
|      - | 9478 | `/*` |
|      - | 9479 | ` * string rawurlencode(string $str)` |
|      - | 9480 | ` *  RFC 3986 URL encoding: spaces become %20 (not '+') and '~' is left intact.` |
|      - | 9481 | ` */` |
|     14 | 9482 | `static int PH7_builtin_rawurlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9483 | `{` |
|      - | 9484 | `	const char *zIn;` |
|      - | 9485 | `	int nLen;` |
|     15 | 9486 | `	if( nArg < 1 ){` |
|      - | 9487 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9488 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9489 | `		return PH7_OK;` |
|      - | 9490 | `	}` |
|      - | 9491 | `	/* Extract the input string */` |
|     15 | 9492 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 9493 | `	if( nLen < 1 ){` |
|      - | 9494 | `		/* php returns an empty string for empty input, not FALSE */` |
|      3 | 9495 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 9496 | `		return PH7_OK;` |
|      - | 9497 | `	}` |
|      - | 9498 | `	/* Perform the RFC 3986 URL encoding */` |
|     13 | 9499 | `	SyUriEncodeRaw(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     13 | 9500 | `	return PH7_OK;` |
|      8 | 9501 | `}` |
|      - | 9502 | `/*` |
|      - | 9503 | ` * string urldecode(string $str)` |
|      - | 9504 | ` *  Decodes any %## encoding in the given string.` |
|      - | 9505 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 9506 | ` * Parameter` |
|      - | 9507 | ` *  $data` |
|      - | 9508 | ` *    Input string.` |
|      - | 9509 | ` * Return` |
|      - | 9510 | ` *  Decoded URL or FALSE on failure.` |
|      - | 9511 | ` */` |
|    110 | 9512 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9513 | `{` |
|      - | 9514 | `	const char *zIn;` |
|      - | 9515 | `	int nLen;` |
|    111 | 9516 | `	if( nArg < 1 ){` |
|      - | 9517 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9518 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9519 | `		return PH7_OK;` |
|      - | 9520 | `	}` |
|      - | 9521 | `	/* Extract the input string */` |
|    111 | 9522 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|    111 | 9523 | `	if( nLen < 1 ){` |
|      - | 9524 | `		/* php returns an empty string for empty input, not FALSE */` |
|     17 | 9525 | `		ph7_result_string(pCtx,"",0);` |
|     17 | 9526 | `		return PH7_OK;` |
|      - | 9527 | `	}` |
|      - | 9528 | `	/* Perform the URL decoding */` |
|     95 | 9529 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|     95 | 9530 | `	return PH7_OK;` |
|     56 | 9531 | `}` |
|      - | 9532 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9533 | `/* Table of the built-in functions */` |
|      - | 9534 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 9535 | `	   /* Variable handling functions */` |
|      - | 9536 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 9537 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 9538 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 9539 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 9540 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 9541 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 9542 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 9543 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 9544 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 9545 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 9546 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 9547 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 9548 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 9549 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 9550 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 9551 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 9552 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 9553 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 9554 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 9555 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 9556 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9557 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 9558 | `	   /* Math functions */` |
|      - | 9559 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 9560 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 9561 | `	{ "acosh" ,   PH7_builtin_acosh        },` |
|      - | 9562 | `	{ "asinh" ,   PH7_builtin_asinh        },` |
|      - | 9563 | `	{ "atanh" ,   PH7_builtin_atanh        },` |
|      - | 9564 | `	{ "expm1" ,   PH7_builtin_expm1        },` |
|      - | 9565 | `	{ "log1p" ,   PH7_builtin_log1p        },` |
|      - | 9566 | `	{ "deg2rad" , PH7_builtin_deg2rad      },` |
|      - | 9567 | `	{ "rad2deg" , PH7_builtin_rad2deg      },` |
|      - | 9568 | `	{ "fpow" ,    PH7_builtin_fpow         },` |
|      - | 9569 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 9570 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 9571 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 9572 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 9573 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 9574 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 9575 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 9576 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 9577 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 9578 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 9579 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 9580 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 9581 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 9582 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 9583 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 9584 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 9585 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 9586 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 9587 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 9588 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 9589 | `	{ "round",    PH7_builtin_round        },` |
|      - | 9590 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 9591 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 9592 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 9593 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 9594 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 9595 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 9596 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 9597 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 9598 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 9599 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9600 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9601 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 9602 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9603 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9604 | `	   /* String handling functions */` |
|      - | 9605 |  |
|      - | 9606 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 9607 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 9608 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 9609 | `	{ "substr_replace",  PH7_builtin_substr_replace },` |
|      - | 9610 | `	{ "levenshtein",     PH7_builtin_levenshtein },` |
|      - | 9611 | `	{ "similar_text",    PH7_builtin_similar_text },` |
|      - | 9612 | `	{ "str_word_count",  PH7_builtin_str_word_count },` |
|      - | 9613 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 9614 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 9615 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 9616 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 9617 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 9618 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 9619 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 9620 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 9621 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 9622 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 9623 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 9624 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 9625 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 9626 | `	{ "strnatcmp"  , PH7_builtin_strnatcmp  },` |
|      - | 9627 | `	{ "strnatcasecmp", PH7_builtin_strnatcmp },` |
|      - | 9628 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 9629 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 9630 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 9631 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 9632 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 9633 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 9634 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 9635 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 9636 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 9637 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 9638 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 9639 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 9640 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 9641 | `	{ "mb_strtolower",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */` |
|      - | 9642 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 9643 | `	{ "mb_strtoupper",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */` |
|      - | 9644 | `	{ "mb_strlen",    PH7_builtin_mb_strlen_f },` |
|      - | 9645 | `	{ "mb_substr",    PH7_builtin_mb_substr_f },` |
|      - | 9646 | `	{ "mb_convert_case", PH7_builtin_mb_convert_case_f },` |
|      - | 9647 | `	{ "mb_strpos",    PH7_builtin_mb_strpos_f },` |
|      - | 9648 | `	{ "mb_stripos",   PH7_builtin_mb_strpos_f },` |
|      - | 9649 | `	{ "mb_strrpos",   PH7_builtin_mb_strpos_f },` |
|      - | 9650 | `	{ "mb_str_split", PH7_builtin_mb_str_split_f },` |
|      - | 9651 | `	{ "mb_internal_encoding", PH7_builtin_mb_internal_encoding_f },` |
|      - | 9652 | `	{ "mb_check_encoding",    PH7_builtin_mb_check_encoding_f },` |
|      - | 9653 | `	{ "mb_strwidth",  PH7_builtin_mb_strwidth_f },` |
|      - | 9654 | `	{ "mb_chr",       PH7_builtin_mb_chr_f   },` |
|      - | 9655 | `	{ "mb_ord",       PH7_builtin_mb_ord_f   },` |
|      - | 9656 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 9657 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 9658 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 9659 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 9660 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 9661 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 9662 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 9663 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 9664 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 9665 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 9666 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 9667 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 9668 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 9669 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 9670 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 9671 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 9672 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - | 9673 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - | 9674 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - | 9675 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 9676 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9677 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9678 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 9679 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 9680 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 9681 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 9682 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9683 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9684 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 9685 |  |
|      - | 9686 |  |
|      - | 9687 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 9688 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 9689 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 9690 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 9691 | `	{ "hash",         PH7_builtin_hash      },` |
|      - | 9692 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|      - | 9693 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|      - | 9694 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|      - | 9695 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 9696 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|      - | 9697 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|      - | 9698 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|      - | 9699 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|      - | 9700 | `	{ "filter_var",            PH7_builtin_filter_var },` |
|      - | 9701 | `	{ "filter_input",          PH7_builtin_filter_input },` |
|      - | 9702 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9703 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9704 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 9705 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 9706 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9707 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9708 |  |
|      - | 9709 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 9710 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 9711 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 9712 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 9713 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 9714 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 9715 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 9716 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 9717 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 9718 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 9719 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 9720 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 9721 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9722 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9723 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 9724 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9725 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9726 |  |
|      - | 9727 | `	         /* Ctype functions */` |
|      - | 9728 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 9729 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 9730 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 9731 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 9732 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 9733 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 9734 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 9735 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 9736 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 9737 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 9738 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 9739 | `	         /* Time functions */` |
|      - | 9740 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 9741 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 9742 | `	{ "hrtime",      PH7_builtin_hrtime       },` |
|      - | 9743 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 9744 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 9745 | `	{ "date",        PH7_builtin_date         },` |
|      - | 9746 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 9747 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 9748 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 9749 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 9750 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 9751 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 9752 | `	{ "date_default_timezone_get", PH7_builtin_date_default_timezone_get },` |
|      - | 9753 | `	{ "date_default_timezone_set", PH7_builtin_date_default_timezone_set },` |
|      - | 9754 | `	        /* URL functions */` |
|      - | 9755 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 9756 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 9757 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 9758 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 9759 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 9760 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 9761 | `	{ "rawurlencode", PH7_builtin_rawurlencode },` |
|      - | 9762 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 9763 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9764 | `};` |
|      - | 9765 | `/*` |
|      - | 9766 | ` * Register the built-in functions defined above,the array functions` |
|      - | 9767 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 9768 | ` */` |
|   3356 | 9769 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      5 | 9770 | `{` |
|      - | 9771 | `	sxu32 n;` |
| 661137 | 9772 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 657781 | 9773 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 328893 | 9774 | `	}` |
|      - | 9775 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   3361 | 9776 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 9777 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   3361 | 9778 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   3361 | 9779 | `}` |
|      - | 9780 |  |
