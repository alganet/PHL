# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4302/5040 lines (85.36%)

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
| 478440 |   18 | `PH7_PRIVATE sxi32 PH7_IntArgResolve(` |
|      - |   19 | `	ph7_context *pCtx,` |
|      - |   20 | `	ph7_value *pArg,` |
|      - |   21 | `	const char *zFunc,` |
|      - |   22 | `	int iArgNum,` |
|      - |   23 | `	const char *zParamName,` |
|      - |   24 | `	const char *zTypeStr,` |
|      - |   25 | `	sxi64 *pOut` |
|      5 |   26 | `){` |
| 478445 |   27 | `	if( ph7_value_is_null(pArg) ){` |
|    ! 0 |   28 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |   29 | `			"%s(): Passing null to parameter #%d (%s) of type %s is deprecated",` |
|    ! 0 |   30 | `			zFunc,iArgNum,zParamName,zTypeStr` |
|      - |   31 | `			);` |
|    ! 0 |   32 | `		*pOut = 0;` |
|    ! 0 |   33 | `		return PH7_OK;` |
|      - |   34 | `	}` |
| 478445 |   35 | `	if( ph7_value_is_float(pArg) ){` |
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
| 478439 |   56 | `	if( ph7_value_is_string(pArg) ){` |
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
| 478425 |   98 | `	if( !ph7_value_is_int(pArg) && !ph7_value_is_bool(pArg) ){` |
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
| 478425 |  113 | `	*pOut = ph7_value_to_int64(pArg);` |
| 478425 |  114 | `	return PH7_OK;` |
| 239225 |  115 | `}` |
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
|    868 |  178 | `static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  179 | `{` |
|    872 |  180 | `	int res = 0; /* Assume false by default */` |
|    872 |  181 | `	if( nArg > 0 ){` |
|      - |  182 | `		/* Strict PHP identity: a float is never an int, even when it holds an` |
|      - |  183 | `		 * integer value (1.0). An integer-valued real carries both MEMOBJ_INT` |
|      - |  184 | `		 * (cached) and MEMOBJ_REAL, so REAL must be excluded here. */` |
|    872 |  185 | `		res = ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]);` |
|    434 |  186 | `	}` |
|      - |  187 | `	/* Query result */` |
|    872 |  188 | `	ph7_result_bool(pCtx,res);` |
|    872 |  189 | `	return PH7_OK;` |
|      4 |  190 | `}` |
|      - |  191 | `/*` |
|      - |  192 | ` * bool is_string($var)` |
|      - |  193 | ` *  Finds out whether a variable is a string.` |
|      - |  194 | ` * Parameters` |
|      - |  195 | ` *   $var: The variable being evaluated.` |
|      - |  196 | ` * Return` |
|      - |  197 | ` *  TRUE if var is string. False otherwise.` |
|      - |  198 | ` */` |
|    756 |  199 | `static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  200 | `{` |
|    759 |  201 | `	int res = 0; /* Assume false by default */` |
|    759 |  202 | `	if( nArg > 0 ){` |
|    759 |  203 | `		res = ph7_value_is_string(apArg[0]);` |
|    378 |  204 | `	}` |
|      - |  205 | `	/* Query result */` |
|    759 |  206 | `	ph7_result_bool(pCtx,res);` |
|    759 |  207 | `	return PH7_OK;` |
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
|    648 |  271 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  272 | `{` |
|    653 |  273 | `	int res = 0; /* Assume false by default */` |
|    653 |  274 | `	if( nArg > 0 ){` |
|    653 |  275 | `		res = ph7_value_is_array(apArg[0]);` |
|    324 |  276 | `	}` |
|      - |  277 | `	/* Query result */` |
|    653 |  278 | `	ph7_result_bool(pCtx,res);` |
|    653 |  279 | `	return PH7_OK;` |
|      5 |  280 | `}` |
|      - |  281 | `/*` |
|      - |  282 | ` * bool is_object($var)` |
|      - |  283 | ` *  Find out whether a variable is an object.` |
|      - |  284 | ` * Parameters` |
|      - |  285 | ` *  $var: The variable being evaluated.` |
|      - |  286 | ` * Return` |
|      - |  287 | ` *  True if var is an object. False otherwise.` |
|      - |  288 | ` */` |
|    440 |  289 | `static int PH7_builtin_is_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  290 | `{` |
|    442 |  291 | `	int res = 0; /* Assume false by default */` |
|    442 |  292 | `	if( nArg > 0 ){` |
|    442 |  293 | `		res = ph7_value_is_object(apArg[0]);` |
|    220 |  294 | `	}` |
|      - |  295 | `	/* Query result */` |
|    442 |  296 | `	ph7_result_bool(pCtx,res);` |
|    442 |  297 | `	return PH7_OK;` |
|      2 |  298 | `}` |
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
|  33400 |  411 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  412 | `{` |
|  33405 |  413 | `	int res = 1; /* Assume empty by default */` |
|  33405 |  414 | `	if( nArg > 0 ){` |
|  33403 |  415 | `		res = ph7_value_is_empty(apArg[0]);` |
|  16699 |  416 | `	}` |
|  33405 |  417 | `	ph7_result_bool(pCtx,res);` |
|  33405 |  418 | `	return PH7_OK;` |
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
| 263974 |  461 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  462 | `{` |
|      - |  463 | `	const char *zSource;` |
|      - |  464 | `	int nSrcLen;` |
|      - |  465 | `	sxi64 iStart,iEnd;` |
| 263979 |  466 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"substr",1,"$string"); }` |
| 263979 |  467 | `	if( nArg < 2 ){` |
|      - |  468 | `		/* Arity is enforced at the call boundary; nothing sensible to return here. */` |
|    ! 0 |  469 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 |  470 | `		return PH7_OK;` |
|      - |  471 | `	}` |
|      - |  472 | `	/* Extract the target string */` |
| 263979 |  473 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|      - |  474 | `	/* Extract the offset */` |
|      - |  475 | `	{` |
| 263979 |  476 | `		sxi64 iTmp = 0;` |
| 263979 |  477 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"substr",2,"$offset","int",&iTmp);` |
| 263979 |  478 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  479 | `			return rcArg;` |
|      - |  480 | `		}` |
| 263979 |  481 | `		iStart = iTmp;` |
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
| 263979 |  493 | `	if( iStart < 0 ){` |
|  32807 |  494 | `		iStart += nSrcLen;` |
|  32807 |  495 | `		if( iStart < 0 ){` |
|      5 |  496 | `			iStart = 0;` |
|      7 |  497 | `		}` |
| 247578 |  498 | `	}else if( iStart > nSrcLen ){` |
|      7 |  499 | `		iStart = nSrcLen;` |
|      3 |  500 | `	}` |
| 263979 |  501 | `	iEnd = nSrcLen;` |
| 263979 |  502 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
| 193527 |  503 | `		sxi64 iLen = 0;` |
| 193527 |  504 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[2],"substr",3,"$length","?int",&iLen);` |
| 193527 |  505 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  506 | `			return rcArg;` |
|      - |  507 | `		}` |
| 193527 |  508 | `		if( iLen < 0 ){` |
|  32739 |  509 | `			iEnd = (sxi64)nSrcLen + iLen;` |
| 177160 |  510 | `		}else if( iLen > (sxi64)nSrcLen - iStart ){` |
|  18629 |  511 | `			iEnd = nSrcLen;` |
|   9317 |  512 | `		}else{` |
| 142169 |  513 | `			iEnd = iStart + iLen;` |
|      - |  514 | `		}` |
|  96761 |  515 | `	}` |
| 263979 |  516 | `	if( iEnd < iStart ){` |
|      3 |  517 | `		iEnd = iStart;` |
|      1 |  518 | `	}` |
| 263979 |  519 | `	ph7_result_string(pCtx,&zSource[iStart],(int)(iEnd - iStart));` |
| 263979 |  520 | `	return PH7_OK;` |
| 131992 |  521 | `}` |
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
| 389942 |  716 | `static void StrNullArgNotice(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNum,const char *zParamName)` |
|      5 |  717 | `{` |
| 389947 |  718 | `	if( ph7_value_is_null(pArg) ){` |
|     22 |  719 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |  720 | `			"%s(): Passing null to parameter #%d (%s) of type string is deprecated",` |
|      7 |  721 | `			zFunc,iArgNum,zParamName);` |
|      7 |  722 | `	}` |
| 389947 |  723 | `}` |
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
|    230 | 1580 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256])` |
|      5 | 1581 | `{` |
|    235 | 1582 | `	const unsigned char *zIn  = (const unsigned char *)zList;` |
|    235 | 1583 | `	const unsigned char *zEnd = zIn + (nLen > 0 ? nLen : 0);` |
|    235 | 1584 | `	SyZero(aMask,256);` |
|    627 | 1585 | `	for( ; zIn < zEnd ; zIn++ ){` |
|    397 | 1586 | `		int c = zIn[0];` |
|    397 | 1587 | `		if( zIn + 3 < zEnd && zIn[1] == '.' && zIn[2] == '.' && zIn[3] >= c ){` |
|      - | 1588 | `			/* Valid incrementing range c..zIn[3] */` |
|     22 | 1589 | `			int hi = zIn[3],k;` |
|    386 | 1590 | `			for( k = c ; k <= hi ; k++ ){` |
|    366 | 1591 | `				aMask[k] = 1;` |
|    184 | 1592 | `			}` |
|     22 | 1593 | `			zIn += 3; /* the loop's ++ then steps past the range end */` |
|    396 | 1594 | `		}else if( zIn + 1 < zEnd && zIn[0] == '.' && zIn[1] == '.' ){` |
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
|    359 | 1616 | `			aMask[c] = 1;` |
|      - | 1617 | `		}` |
|    201 | 1618 | `	}` |
|    235 | 1619 | `}` |
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
|  75210 | 2016 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2017 | `{` |
|  75215 | 2018 | `	int iLen = 0;` |
|  75215 | 2019 | `	if( nArg > 0 ){` |
|  75215 | 2020 | `		StrNullArgNotice(pCtx,apArg[0],"strlen",1,"$string");` |
|  75215 | 2021 | `		ph7_value_to_string(apArg[0],&iLen);` |
|  37605 | 2022 | `	}` |
|      - | 2023 | `	/* String length */` |
|  75215 | 2024 | `	ph7_result_int(pCtx,iLen);` |
|  75215 | 2025 | `	return PH7_OK;` |
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
| 149086 | 2257 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 2258 | `{` |
|  74543 | 2259 | `	SXUNUSED(pKey);` |
| 149091 | 2260 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 2261 | `	const char *zData;` |
|      - | 2262 | `	int nLen;` |
| 149091 | 2263 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
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
| 149089 | 2287 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 2288 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 149089 | 2289 | `	if( pData->bFirst ){` |
|  33215 | 2290 | `		pData->bFirst = 0;` |
| 132484 | 2291 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 2292 | `		/* append the separator first */` |
| 115863 | 2293 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 2294 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 2295 | `			return PH7_ABORT;` |
|      - | 2296 | `		}` |
|  57929 | 2297 | `	}` |
|      - | 2298 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 149089 | 2299 | `	if( nLen > 0 ){` |
| 136485 | 2300 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2301 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 2302 | `			return PH7_ABORT;` |
|      - | 2303 | `		}` |
|  68240 | 2304 | `	}` |
| 149089 | 2305 | `	return PH7_OK;` |
|  74548 | 2306 | `}` |
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
|  33234 | 2320 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2321 | `{` |
|      - | 2322 | `	struct implode_data imp_data;` |
|  33239 | 2323 | `	int i = 1;` |
|  33239 | 2324 | `	if( nArg < 1 ){` |
|      - | 2325 | `		/* Missing argument,return NULL */` |
|    ! 0 | 2326 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2327 | `		return PH7_OK;` |
|      - | 2328 | `	}` |
|      - | 2329 | `	/* Prepare the implode context */` |
|  33239 | 2330 | `	imp_data.pCtx = pCtx;` |
|  33239 | 2331 | `	imp_data.bRecursive = 0;` |
|  33239 | 2332 | `	imp_data.bFirst = 1;` |
|  33239 | 2333 | `	imp_data.nRecCount = 0;` |
|  33239 | 2334 | `	imp_data.rc = SXRET_OK;` |
|  33239 | 2335 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  33237 | 2336 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  16621 | 2337 | `	}else{` |
|      3 | 2338 | `		imp_data.zSep = 0;` |
|      3 | 2339 | `		imp_data.nSeplen = 0;` |
|      3 | 2340 | `		i = 0;` |
|      - | 2341 | `	}` |
|  33239 | 2342 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2343 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2344 | `	}` |
|      - | 2345 | `	/* Start the 'join' process */` |
|  66473 | 2346 | `	while( i < nArg ){` |
|  33239 | 2347 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2348 | `			/* Iterate throw array entries */` |
|  33239 | 2349 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2350 | `			/* Surface a callback allocation failure as a fatal */` |
|  33239 | 2351 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2352 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2353 | `			}` |
|  16622 | 2354 | `		}else{` |
|      - | 2355 | `			const char *zData;` |
|      - | 2356 | `			int nLen;` |
|      - | 2357 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 2358 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2359 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 2360 | `			if( imp_data.bFirst ){` |
|    ! 0 | 2361 | `				imp_data.bFirst = 0;` |
|    ! 0 | 2362 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2363 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 2364 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2365 | `				}` |
|    ! 0 | 2366 | `			}` |
|      - | 2367 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 2368 | `			if( nLen > 0 ){` |
|    ! 0 | 2369 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2370 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2371 | `				}` |
|    ! 0 | 2372 | `			}` |
|      - | 2373 | `		}` |
|  33239 | 2374 | `		i++;` |
|      5 | 2375 | `	}` |
|  33239 | 2376 | `	return PH7_OK;` |
|  16622 | 2377 | `}` |
|      - | 2378 | `/*` |
|      - | 2379 | ` * Symisc eXtension:` |
|      - | 2380 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 2381 | ` * Purpose` |
|      - | 2382 | ` *  Same as implode() but recurse on arrays.` |
|      - | 2383 | ` * Example:` |
|      - | 2384 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 2385 | ` *   echo implode_recursive("/",$a);` |
|      - | 2386 | ` *   Will output` |
|      - | 2387 | ` *     usr/home/dean.` |
|      - | 2388 | ` *   While the standard implode would produce.` |
|      - | 2389 | ` *    usr/Array.` |
|      - | 2390 | ` * Parameter` |
|      - | 2391 | ` *  Refer to implode().` |
|      - | 2392 | ` * Return` |
|      - | 2393 | ` *  Refer to implode().` |
|      - | 2394 | ` */` |
|     12 | 2395 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2396 | `{` |
|      - | 2397 | `	struct implode_data imp_data;` |
|     13 | 2398 | `	int i = 1;` |
|     13 | 2399 | `	if( nArg < 1 ){` |
|      - | 2400 | `		/* Missing argument,return NULL */` |
|      3 | 2401 | `		ph7_result_null(pCtx);` |
|      3 | 2402 | `		return PH7_OK;` |
|      - | 2403 | `	}` |
|      - | 2404 | `	/* Prepare the implode context */` |
|     11 | 2405 | `	imp_data.pCtx = pCtx;` |
|     11 | 2406 | `	imp_data.bRecursive = 1;` |
|     11 | 2407 | `	imp_data.bFirst = 1;` |
|     11 | 2408 | `	imp_data.nRecCount = 0;` |
|     11 | 2409 | `	imp_data.rc = SXRET_OK;` |
|     11 | 2410 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 2411 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 2412 | `	}else{` |
|    ! 0 | 2413 | `		imp_data.zSep = 0;` |
|    ! 0 | 2414 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 2415 | `		i = 0;` |
|      - | 2416 | `	}` |
|     11 | 2417 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2418 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2419 | `	}` |
|      - | 2420 | `	/* Start the 'join' process */` |
|     21 | 2421 | `	while( i < nArg ){` |
|     11 | 2422 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2423 | `			/* Iterate throw array entries */` |
|      3 | 2424 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2425 | `			/* Surface a callback allocation failure as a fatal */` |
|      3 | 2426 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2427 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2428 | `			}` |
|      2 | 2429 | `		}else{` |
|      - | 2430 | `			const char *zData;` |
|      - | 2431 | `			int nLen;` |
|      - | 2432 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 2433 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2434 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 2435 | `			if( imp_data.bFirst ){` |
|      9 | 2436 | `				imp_data.bFirst = 0;` |
|      4 | 2437 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2438 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 2439 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2440 | `				}` |
|    ! 0 | 2441 | `			}` |
|      - | 2442 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 2443 | `			if( nLen > 0 ){` |
|      9 | 2444 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2445 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2446 | `				}` |
|      4 | 2447 | `			}` |
|      - | 2448 | `		}` |
|     11 | 2449 | `		i++;` |
|      1 | 2450 | `	}` |
|     11 | 2451 | `	return PH7_OK;` |
|      7 | 2452 | `}` |
|      - | 2453 | `/*` |
|      - | 2454 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 2455 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 2456 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 2457 | ` * Parameters` |
|      - | 2458 | ` *  $delimiter` |
|      - | 2459 | ` *   The boundary string.` |
|      - | 2460 | ` * $string` |
|      - | 2461 | ` *   The input string.` |
|      - | 2462 | ` * $limit` |
|      - | 2463 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 2464 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 2465 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 2466 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 2467 | ` * Returns` |
|      - | 2468 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 2469 | ` *  on boundaries formed by the delimiter.` |
|      - | 2470 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 2471 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 2472 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 2473 | ` *  will be returned.` |
|      - | 2474 | ` * NOTE:` |
|      - | 2475 | ` *  Negative limit is not supported.` |
|      - | 2476 | ` */` |
|   6586 | 2477 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2478 | `{` |
|      - | 2479 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 2480 | `	int nDelim,nStrlen,iLimit;` |
|      - | 2481 | `	ph7_value *pArray;` |
|      - | 2482 | `	ph7_value *pValue;` |
|      - | 2483 | `	sxu32 nOfft;` |
|      - | 2484 | `	sxi32 rc;` |
|   6591 | 2485 | `	if( nArg < 2 ){` |
|      - | 2486 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2487 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2488 | `		return PH7_OK;` |
|      - | 2489 | `	}` |
|      - | 2490 | `	/* Extract the delimiter */` |
|   6591 | 2491 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   6591 | 2492 | `	if( nDelim < 1 ){` |
|      - | 2493 | `		/* Empty delimiter: PHP 8 throws a catchable ValueError. */` |
|      5 | 2494 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2495 | `			"explode(): Argument #1 ($separator) must not be empty");` |
|      - | 2496 | `	}` |
|      - | 2497 | `	/* Extract the string */` |
|   6587 | 2498 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   6587 | 2499 | `	if( nStrlen < 1 ){` |
|      - | 2500 | `		/* Empty string: normally an array with a single empty element (PHP behavior).` |
|      - | 2501 | `		 * A negative limit drops the last -limit components, so the sole empty` |
|      - | 2502 | `		 * component is dropped and the result is an empty array. */` |
|      7 | 2503 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      7 | 2504 | `		if( pArrayTmp == 0 ){` |
|      - | 2505 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 2506 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2507 | `			return PH7_OK;` |
|      - | 2508 | `		}` |
|      7 | 2509 | `		if( !(nArg > 2 && ph7_value_to_int(apArg[2]) < 0) ){` |
|      5 | 2510 | `			ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      5 | 2511 | `			if( pValueTmp == 0 ){` |
|      - | 2512 | `				/* Out of memory,return FALSE */` |
|    ! 0 | 2513 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 2514 | `				return PH7_OK;` |
|      - | 2515 | `			}` |
|      5 | 2516 | `			ph7_value_string(pValueTmp, "", 0);` |
|      5 | 2517 | `			if( ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp) != SXRET_OK ){` |
|    ! 0 | 2518 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2519 | `			}` |
|      2 | 2520 | `		}` |
|      7 | 2521 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      7 | 2522 | `		return PH7_OK;` |
|      - | 2523 | `	}` |
|      - | 2524 | `	/* Point to the end of the string */` |
|   6581 | 2525 | `	zEnd = &zString[nStrlen];` |
|      - | 2526 | `	/* Create the array */` |
|   6581 | 2527 | `	pArray =  ph7_context_new_array(pCtx);` |
|   6581 | 2528 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   6581 | 2529 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2530 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 2531 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2532 | `		return PH7_OK;` |
|      - | 2533 | `	}` |
|      - | 2534 | `	/* Set a defualt limit */` |
|   6581 | 2535 | `	iLimit = SXI32_HIGH;` |
|   6581 | 2536 | `	if( nArg > 2 ){` |
|     38 | 2537 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     38 | 2538 | `		if( iLimit < 0 ){` |
|      - | 2539 | `			/* Negative limit: keep all components except the last -iLimit (PHP).` |
|      - | 2540 | `			 * Pre-count the components (delimiters + 1), then emit only the first` |
|      - | 2541 | `			 * nKeep CLEAN components — no trailing-remainder merge (the difference` |
|      - | 2542 | `			 * from the positive path). nKeep <= 0 drops everything -> empty array. */` |
|     17 | 2543 | `			int nTotal = 1,nKeep;` |
|     17 | 2544 | `			const char *zScan = zString;` |
|      - | 2545 | `			sxu32 nScanOfft;` |
|     57 | 2546 | `			while( SyBlobSearch(zScan,(sxu32)(zEnd - zScan),zDelim,nDelim,&nScanOfft) == SXRET_OK ){` |
|     41 | 2547 | `				nTotal++;` |
|     41 | 2548 | `				zScan = &zScan[nScanOfft + nDelim];` |
|      1 | 2549 | `			}` |
|     17 | 2550 | `			nKeep = nTotal + iLimit; /* iLimit < 0, so this is nTotal - (-iLimit) */` |
|     49 | 2551 | `			while( nKeep > (int)ph7_array_count(pArray)` |
|     39 | 2552 | `				&& SyBlobSearch(zString,(sxu32)(zEnd - zString),zDelim,nDelim,&nOfft) == SXRET_OK ){` |
|      - | 2553 | `				/* Emit the next clean component */` |
|     23 | 2554 | `				zCur = &zString[nOfft];` |
|     23 | 2555 | `				ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|     23 | 2556 | `				if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2557 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2558 | `				}` |
|     23 | 2559 | `				zString = &zCur[nDelim];` |
|     23 | 2560 | `				ph7_value_reset_string_cursor(pValue);` |
|      1 | 2561 | `			}` |
|     17 | 2562 | `			ph7_result_value(pCtx,pArray);` |
|     17 | 2563 | `			return PH7_OK;` |
|      - | 2564 | `		}` |
|     22 | 2565 | `		if( iLimit == 0 ){` |
|      5 | 2566 | `			iLimit = 1;` |
|      2 | 2567 | `		}` |
|     22 | 2568 | `		iLimit--;` |
|      9 | 2569 | `	}` |
|      - | 2570 | `	/* Start exploding */` |
|  80309 | 2571 | `	for(;;){` |
| 160623 | 2572 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 160623 | 2573 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 2574 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   6565 | 2575 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   6565 | 2576 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2577 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2578 | `			}` |
|   6565 | 2579 | `			break;` |
|      - | 2580 | `		}` |
|      - | 2581 | `		/* Point to the desired offset */` |
| 154063 | 2582 | `		zCur = &zString[nOfft];` |
|      - | 2583 | `		/* Perform the store operation (may be empty) */` |
| 154063 | 2584 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 154063 | 2585 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2586 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2587 | `		}` |
|      - | 2588 | `		/* Point beyond the delimiter */` |
| 154063 | 2589 | `		zString = &zCur[nDelim];` |
|      - | 2590 | `		/* Reset the cursor */` |
| 154063 | 2591 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 2592 | `	}` |
|      - | 2593 | `	/* Return the freshly created array */` |
|   6565 | 2594 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2595 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 2596 | `	 * released as soon we return from this foregin function.` |
|      - | 2597 | `	 */` |
|   6565 | 2598 | `	return PH7_OK;` |
|   3298 | 2599 | `}` |
|      - | 2600 | `/*` |
|      - | 2601 | ` * string trim(string $str[,string $charlist ])` |
|      - | 2602 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2603 | ` * Parameters` |
|      - | 2604 | ` *  $str` |
|      - | 2605 | ` *   The string that will be trimmed.` |
|      - | 2606 | ` * $charlist` |
|      - | 2607 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2608 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2609 | ` *   With .. you can specify a range of characters.` |
|      - | 2610 | ` * Returns.` |
|      - | 2611 | ` *  Thr processed string.` |
|      - | 2612 | ` * NOTE:` |
|      - | 2613 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2614 | ` */` |
|  14344 | 2615 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2616 | `{` |
|  14349 | 2617 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"trim",1,"$string"); }` |
|      - | 2618 | `	const char *zString;` |
|      - | 2619 | `	int nLen;` |
|  14349 | 2620 | `	if( nArg < 1 ){` |
|      - | 2621 | `		/* Missing arguments,return null */` |
|    ! 0 | 2622 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2623 | `		return PH7_OK;` |
|      - | 2624 | `	}` |
|      - | 2625 | `	/* Extract the target string */` |
|  14349 | 2626 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  14349 | 2627 | `	if( nLen < 1 ){` |
|      - | 2628 | `		/* Empty string,return */` |
|    755 | 2629 | `		ph7_result_string(pCtx,"",0);` |
|    755 | 2630 | `		return PH7_OK;` |
|      - | 2631 | `	}` |
|      - | 2632 | `	/* Start the trim process */` |
|  13599 | 2633 | `	if( nArg < 2 ){` |
|      - | 2634 | `		SyString sStr;` |
|      - | 2635 | `		/* Remove white spaces and NUL bytes */` |
|  13569 | 2636 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  34031 | 2637 | `		SyStringFullTrimSafe(&sStr);` |
|  13569 | 2638 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   6787 | 2639 | `	}else{` |
|      - | 2640 | `		/* Char list */` |
|      - | 2641 | `		const char *zList;` |
|      - | 2642 | `		int nListlen;` |
|     33 | 2643 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     33 | 2644 | `		if( nListlen < 1 ){` |
|      - | 2645 | `			/* Return the string unchanged */` |
|      6 | 2646 | `			ph7_result_string(pCtx,zString,nLen);` |
|      4 | 2647 | `		}else{` |
|      - | 2648 | `			char aMask[256];` |
|     29 | 2649 | `			const char *zEnd = &zString[nLen];` |
|     29 | 2650 | `			const char *zCur = zString;` |
|     29 | 2651 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2652 | `			/* Left trim */` |
|     79 | 2653 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     53 | 2654 | `				zCur++;` |
|      3 | 2655 | `			}` |
|      - | 2656 | `			/* Right trim */` |
|     79 | 2657 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     53 | 2658 | `				zEnd--;` |
|      3 | 2659 | `			}` |
|     29 | 2660 | `			if( zCur >= zEnd ){` |
|      - | 2661 | `				/* Return the empty string */` |
|    ! 0 | 2662 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2663 | `			}else{` |
|     29 | 2664 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2665 | `			}` |
|      - | 2666 | `		}` |
|      - | 2667 | `	}` |
|  13599 | 2668 | `	return PH7_OK;` |
|   7177 | 2669 | `}` |
|      - | 2670 | `/*` |
|      - | 2671 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 2672 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 2673 | ` * Parameters` |
|      - | 2674 | ` *  $str` |
|      - | 2675 | ` *   The string that will be trimmed.` |
|      - | 2676 | ` * $charlist` |
|      - | 2677 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2678 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2679 | ` *   With .. you can specify a range of characters.` |
|      - | 2680 | ` * Returns.` |
|      - | 2681 | ` *  Thr processed string.` |
|      - | 2682 | ` * NOTE:` |
|      - | 2683 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2684 | ` */` |
|    162 | 2685 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2686 | `{` |
|    167 | 2687 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"rtrim",1,"$string"); }` |
|      - | 2688 | `	const char *zString;` |
|      - | 2689 | `	int nLen;` |
|    167 | 2690 | `	if( nArg < 1 ){` |
|      - | 2691 | `		/* Missing arguments,return null */` |
|    ! 0 | 2692 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2693 | `		return PH7_OK;` |
|      - | 2694 | `	}` |
|      - | 2695 | `	/* Extract the target string */` |
|    167 | 2696 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    167 | 2697 | `	if( nLen < 1 ){` |
|      - | 2698 | `		/* Empty string,return */` |
|      7 | 2699 | `		ph7_result_string(pCtx,"",0);` |
|      7 | 2700 | `		return PH7_OK;` |
|      - | 2701 | `	}` |
|      - | 2702 | `	/* Start the trim process */` |
|    161 | 2703 | `	if( nArg < 2 ){` |
|      - | 2704 | `		SyString sStr;` |
|      - | 2705 | `		/* Remove white spaces and NUL bytes*/` |
|     19 | 2706 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     48 | 2707 | `		SyStringRightTrimSafe(&sStr);` |
|     19 | 2708 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|     10 | 2709 | `	}else{` |
|      - | 2710 | `		/* Char list */` |
|      - | 2711 | `		const char *zList;` |
|      - | 2712 | `		int nListlen;` |
|    143 | 2713 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|    143 | 2714 | `		if( nListlen < 1 ){` |
|      - | 2715 | `			/* Return the string unchanged */` |
|    ! 0 | 2716 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 2717 | `		}else{` |
|      - | 2718 | `			char aMask[256];` |
|    143 | 2719 | `			const char *zEnd = &zString[nLen];` |
|    143 | 2720 | `			const char *zCur = zString;` |
|    143 | 2721 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2722 | `			/* Right trim */` |
|    161 | 2723 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     22 | 2724 | `				zEnd--;` |
|      4 | 2725 | `			}` |
|    143 | 2726 | `			if( zEnd <= zCur ){` |
|      - | 2727 | `				/* Return the empty string */` |
|    ! 0 | 2728 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2729 | `			}else{` |
|    143 | 2730 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2731 | `			}` |
|      - | 2732 | `		}` |
|      - | 2733 | `	}` |
|    161 | 2734 | `	return PH7_OK;` |
|     86 | 2735 | `}` |
|      - | 2736 | `/*` |
|      - | 2737 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 2738 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2739 | ` * Parameters` |
|      - | 2740 | ` *  $str` |
|      - | 2741 | ` *   The string that will be trimmed.` |
|      - | 2742 | ` * $charlist` |
|      - | 2743 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2744 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2745 | ` *   With .. you can specify a range of characters.` |
|      - | 2746 | ` * Returns.` |
|      - | 2747 | ` *  Thr processed string.` |
|      - | 2748 | ` * NOTE:` |
|      - | 2749 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2750 | ` */` |
|     42 | 2751 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2752 | `{` |
|     47 | 2753 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"ltrim",1,"$string"); }` |
|      - | 2754 | `	const char *zString;` |
|      - | 2755 | `	int nLen;` |
|     47 | 2756 | `	if( nArg < 1 ){` |
|      - | 2757 | `		/* Missing arguments,return null */` |
|    ! 0 | 2758 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2759 | `		return PH7_OK;` |
|      - | 2760 | `	}` |
|      - | 2761 | `	/* Extract the target string */` |
|     47 | 2762 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     47 | 2763 | `	if( nLen < 1 ){` |
|      - | 2764 | `		/* Empty string,return */` |
|     23 | 2765 | `		ph7_result_string(pCtx,"",0);` |
|     23 | 2766 | `		return PH7_OK;` |
|      - | 2767 | `	}` |
|      - | 2768 | `	/* Start the trim process */` |
|     28 | 2769 | `	if( nArg < 2 ){` |
|      - | 2770 | `		SyString sStr;` |
|      - | 2771 | `		/* Remove white spaces and NUL byte */` |
|      5 | 2772 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     13 | 2773 | `		SyStringLeftTrimSafe(&sStr);` |
|      5 | 2774 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      3 | 2775 | `	}else{` |
|      - | 2776 | `		/* Char list */` |
|      - | 2777 | `		const char *zList;` |
|      - | 2778 | `		int nListlen;` |
|     24 | 2779 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     24 | 2780 | `		if( nListlen < 1 ){` |
|      - | 2781 | `			/* Return the string unchanged */` |
|      3 | 2782 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2783 | `		}else{` |
|      - | 2784 | `			char aMask[256];` |
|     22 | 2785 | `			const char *zEnd = &zString[nLen];` |
|     22 | 2786 | `			const char *zCur = zString;` |
|     22 | 2787 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2788 | `			/* Left trim */` |
|     56 | 2789 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     38 | 2790 | `				zCur++;` |
|      4 | 2791 | `			}` |
|     22 | 2792 | `			if( zCur >= zEnd ){` |
|      - | 2793 | `				/* Return the empty string */` |
|    ! 0 | 2794 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2795 | `			}else{` |
|     22 | 2796 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2797 | `			}` |
|      - | 2798 | `		}` |
|      - | 2799 | `	}` |
|     28 | 2800 | `	return PH7_OK;` |
|     26 | 2801 | `}` |
|      - | 2802 | `/*` |
|      - | 2803 | ` * string strtolower(string $str)` |
|      - | 2804 | ` *  Make a string lowercase.` |
|      - | 2805 | ` * Parameters` |
|      - | 2806 | ` *  $str` |
|      - | 2807 | ` *   The input string.` |
|      - | 2808 | ` * Returns.` |
|      - | 2809 | ` *  The lowercased string.` |
|      - | 2810 | ` */` |
|  33204 | 2811 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2812 | `{` |
|  33209 | 2813 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtolower",1,"$string"); }` |
|      - | 2814 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2815 | `	int nLen;` |
|  33209 | 2816 | `	if( nArg < 1 ){` |
|      - | 2817 | `		/* Missing arguments,return null */` |
|    ! 0 | 2818 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2819 | `		return PH7_OK;` |
|      - | 2820 | `	}` |
|      - | 2821 | `	/* Extract the target string */` |
|  33209 | 2822 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  33209 | 2823 | `	if( nLen < 1 ){` |
|      - | 2824 | `		/* Empty string,return */` |
|      5 | 2825 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2826 | `		return PH7_OK;` |
|      - | 2827 | `	}` |
|      - | 2828 | `	/* Perform the requested operation */` |
|  33205 | 2829 | `	zEnd = &zString[nLen];` |
| 104713 | 2830 | `	for(;;){` |
| 209431 | 2831 | `		if( zString >= zEnd ){` |
|      - | 2832 | `			/* No more input,break immediately */` |
|  33205 | 2833 | `			break;` |
|      - | 2834 | `		}` |
| 176231 | 2835 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2836 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2837 | `			zCur = zString;` |
|    ! 0 | 2838 | `			zString++;` |
|    ! 0 | 2839 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2840 | `				zString++;` |
|    ! 0 | 2841 | `			}` |
|      - | 2842 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2843 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2844 | `		}else{` |
| 176231 | 2845 | `			int c = zString[0];` |
| 176231 | 2846 | `			if( SyisUpper(c) ){` |
| 173675 | 2847 | `				c = SyToLower(zString[0]);` |
|  86835 | 2848 | `			}` |
|      - | 2849 | `			/* Append character */` |
| 176231 | 2850 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2851 | `			/* Advance the cursor */` |
| 176231 | 2852 | `			zString++;` |
|      - | 2853 | `		}` |
|      5 | 2854 | `	}` |
|  33205 | 2855 | `	return PH7_OK;` |
|  16607 | 2856 | `}` |
|      - | 2857 | `/*` |
|      - | 2858 | ` * string strtolower(string $str)` |
|      - | 2859 | ` *  Make a string uppercase.` |
|      - | 2860 | ` * Parameters` |
|      - | 2861 | ` *  $str` |
|      - | 2862 | ` *   The input string.` |
|      - | 2863 | ` * Returns.` |
|      - | 2864 | ` *  The uppercased string.` |
|      - | 2865 | ` */` |
|     70 | 2866 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2867 | `{` |
|     75 | 2868 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtoupper",1,"$string"); }` |
|      - | 2869 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2870 | `	int nLen;` |
|     75 | 2871 | `	if( nArg < 1 ){` |
|      - | 2872 | `		/* Missing arguments,return null */` |
|    ! 0 | 2873 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2874 | `		return PH7_OK;` |
|      - | 2875 | `	}` |
|      - | 2876 | `	/* Extract the target string */` |
|     75 | 2877 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     75 | 2878 | `	if( nLen < 1 ){` |
|      - | 2879 | `		/* Empty string,return */` |
|      5 | 2880 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2881 | `		return PH7_OK;` |
|      - | 2882 | `	}` |
|      - | 2883 | `	/* Perform the requested operation */` |
|     71 | 2884 | `	zEnd = &zString[nLen];` |
|    139 | 2885 | `	for(;;){` |
|    283 | 2886 | `		if( zString >= zEnd ){` |
|      - | 2887 | `			/* No more input,break immediately */` |
|     71 | 2888 | `			break;` |
|      - | 2889 | `		}` |
|    217 | 2890 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2891 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2892 | `			zCur = zString;` |
|    ! 0 | 2893 | `			zString++;` |
|    ! 0 | 2894 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2895 | `				zString++;` |
|    ! 0 | 2896 | `			}` |
|      - | 2897 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2898 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2899 | `		}else{` |
|    217 | 2900 | `			int c = zString[0];` |
|    217 | 2901 | `			if( SyisLower(c) ){` |
|    204 | 2902 | `				c = SyToUpper(zString[0]);` |
|    100 | 2903 | `			}` |
|      - | 2904 | `			/* Append character */` |
|    217 | 2905 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2906 | `			/* Advance the cursor */` |
|    217 | 2907 | `			zString++;` |
|      - | 2908 | `		}` |
|      5 | 2909 | `	}` |
|     71 | 2910 | `	return PH7_OK;` |
|     40 | 2911 | `}` |
|      - | 2912 | `/*` |
|      - | 2913 | ` * string ucfirst(string $str)` |
|      - | 2914 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2915 | ` *  character is alphabetic.` |
|      - | 2916 | ` * Parameters` |
|      - | 2917 | ` *  $str` |
|      - | 2918 | ` *   The input string.` |
|      - | 2919 | ` * Returns.` |
|      - | 2920 | ` *  The processed string.` |
|      - | 2921 | ` */` |
|      4 | 2922 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2923 | `{` |
|      - | 2924 | `	const char *zString,*zEnd;` |
|      - | 2925 | `	int nLen,c;` |
|      5 | 2926 | `	if( nArg < 1 ){` |
|      - | 2927 | `		/* Missing arguments,return null */` |
|    ! 0 | 2928 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2929 | `		return PH7_OK;` |
|      - | 2930 | `	}` |
|      - | 2931 | `	/* Extract the target string */` |
|      5 | 2932 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2933 | `	if( nLen < 1 ){` |
|      - | 2934 | `		/* Empty string,return */` |
|      3 | 2935 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2936 | `		return PH7_OK;` |
|      - | 2937 | `	}` |
|      - | 2938 | `	/* Perform the requested operation */` |
|      3 | 2939 | `	zEnd = &zString[nLen];` |
|      3 | 2940 | `	c = zString[0];` |
|      3 | 2941 | `	if( SyisLower(c) ){` |
|      3 | 2942 | `		c = SyToUpper(c);` |
|      1 | 2943 | `	}` |
|      - | 2944 | `	/* Append the first character */` |
|      3 | 2945 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2946 | `	zString++;` |
|      3 | 2947 | `	if( zString < zEnd ){` |
|      - | 2948 | `		/* Append the rest of the input verbatim */` |
|      3 | 2949 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2950 | `	}` |
|      3 | 2951 | `	return PH7_OK;` |
|      3 | 2952 | `}` |
|      - | 2953 | `/*` |
|      - | 2954 | ` * string lcfirst(string $str)` |
|      - | 2955 | ` *  Make a string's first character lowercase.` |
|      - | 2956 | ` * Parameters` |
|      - | 2957 | ` *  $str` |
|      - | 2958 | ` *   The input string.` |
|      - | 2959 | ` * Returns.` |
|      - | 2960 | ` *  The processed string.` |
|      - | 2961 | ` */` |
|      4 | 2962 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2963 | `{` |
|      - | 2964 | `	const char *zString,*zEnd;` |
|      - | 2965 | `	int nLen,c;` |
|      5 | 2966 | `	if( nArg < 1 ){` |
|      - | 2967 | `		/* Missing arguments,return null */` |
|    ! 0 | 2968 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2969 | `		return PH7_OK;` |
|      - | 2970 | `	}` |
|      - | 2971 | `	/* Extract the target string */` |
|      5 | 2972 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2973 | `	if( nLen < 1 ){` |
|      - | 2974 | `		/* Empty string,return */` |
|      3 | 2975 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2976 | `		return PH7_OK;` |
|      - | 2977 | `	}` |
|      - | 2978 | `	/* Perform the requested operation */` |
|      3 | 2979 | `	zEnd = &zString[nLen];` |
|      3 | 2980 | `	c = zString[0];` |
|      3 | 2981 | `	if( SyisUpper(c) ){` |
|      3 | 2982 | `		c = SyToLower(c);` |
|      1 | 2983 | `	}` |
|      - | 2984 | `	/* Append the first character */` |
|      3 | 2985 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2986 | `	zString++;` |
|      3 | 2987 | `	if( zString < zEnd ){` |
|      - | 2988 | `		/* Append the rest of the input verbatim */` |
|      3 | 2989 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2990 | `	}` |
|      3 | 2991 | `	return PH7_OK;` |
|      3 | 2992 | `}` |
|      - | 2993 | `/*` |
|      - | 2994 | ` * int ord(string $string)` |
|      - | 2995 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 2996 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 2997 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 2998 | ` * Parameters` |
|      - | 2999 | ` *  $string` |
|      - | 3000 | ` *   The input string.` |
|      - | 3001 | ` * Returns` |
|      - | 3002 | ` *  The ASCII value as an integer.` |
|      - | 3003 | ` */` |
|    182 | 3004 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3005 | `{` |
|      - | 3006 | `	const char *zString;` |
|      - | 3007 | `	int nLen,c;` |
|      - | 3008 | `	/* PHP requires exactly one argument. */` |
|    185 | 3009 | `	if( nArg != 1 ){` |
|      4 | 3010 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3011 | `			"ArgumentCountError",` |
|      - | 3012 | `			"ord() expects exactly 1 argument, %d given",` |
|      1 | 3013 | `			nArg` |
|      - | 3014 | `			);` |
|      - | 3015 | `	}` |
|      - | 3016 | `	/* Passing null is deprecated (E_DEPRECATED).  PHP emits this before` |
|      - | 3017 | `	 * the empty-string deprecation, so we check null first. */` |
|    182 | 3018 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 3019 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 3020 | `			"ord(): Passing null to parameter #1 ($character) "` |
|      - | 3021 | `			"of type string is deprecated"` |
|      - | 3022 | `			);` |
|      1 | 3023 | `	}` |
|      - | 3024 | `	/* Extract the target string */` |
|    182 | 3025 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    182 | 3026 | `	if( nLen < 1 ){` |
|      - | 3027 | `		/* Empty string is deprecated (E_DEPRECATED). */` |
|      5 | 3028 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 3029 | `			"ord(): Providing an empty string is deprecated"` |
|      - | 3030 | `			);` |
|      5 | 3031 | `		ph7_result_int(pCtx,0);` |
|      5 | 3032 | `		return PH7_OK;` |
|      - | 3033 | `	}` |
|      - | 3034 | `	/* A string longer than one byte is deprecated (E_DEPRECATED). */` |
|    178 | 3035 | `	if( nLen > 1 ){` |
|      7 | 3036 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 3037 | `			"ord(): Providing a string that is not one byte long "` |
|      - | 3038 | `			"is deprecated. Use ord($str[0]) instead"` |
|      - | 3039 | `			);` |
|      3 | 3040 | `	}` |
|      - | 3041 | `	/* Extract the ASCII value of the first character */` |
|    178 | 3042 | `	c = (unsigned char)zString[0];` |
|      - | 3043 | `	/* Return that value */` |
|    178 | 3044 | `	ph7_result_int(pCtx,c);` |
|    178 | 3045 | `	return PH7_OK;` |
|     94 | 3046 | `}` |
|      - | 3047 | `/*` |
|      - | 3048 | ` * string chr(int $codepoint)` |
|      - | 3049 | ` *  Returns a one-character string containing the character specified` |
|      - | 3050 | ` *  by the given codepoint.  Any integer is accepted; values outside` |
|      - | 3051 | ` *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.` |
|      - | 3052 | ` * Parameters` |
|      - | 3053 | ` *  $codepoint` |
|      - | 3054 | ` *   An integer codepoint.  Values outside 0-255 are deprecated and` |
|      - | 3055 | ` *   will be constrained to a single byte.` |
|      - | 3056 | ` * Returns` |
|      - | 3057 | ` *  A single-character string.` |
|      - | 3058 | ` */` |
|   7114 | 3059 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3060 | `{` |
|      - | 3061 | `	int c;` |
|      - | 3062 | `	unsigned char ch;` |
|      - | 3063 | `	/* PHP requires exactly one argument. */` |
|   7117 | 3064 | `	if( nArg != 1 ){` |
|      4 | 3065 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3066 | `			"ArgumentCountError",` |
|      - | 3067 | `			"chr() expects exactly 1 argument, %d given",` |
|      1 | 3068 | `			nArg` |
|      - | 3069 | `			);` |
|      - | 3070 | `	}` |
|      - | 3071 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 3072 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 3073 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 3074 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|   7114 | 3075 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      - | 3076 | `		char zBuf[120];` |
|      4 | 3077 | `		SyBufferFormat(zBuf,sizeof(zBuf),` |
|      - | 3078 | `			"Implicit conversion from float %g to int loses precision",` |
|      1 | 3079 | `			ph7_value_to_double(apArg[0])` |
|      - | 3080 | `			);` |
|      3 | 3081 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zBuf);` |
|      1 | 3082 | `	}` |
|      - | 3083 | `	/* Extract the codepoint. */` |
|   7114 | 3084 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 3085 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 3086 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 3087 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 3088 | `	 * name to avoid the API double-prefixing it. */` |
|   7114 | 3089 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 3090 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 3091 | `			E_DEPRECATED,` |
|      - | 3092 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 3093 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 3094 | `			"The value used will be constrained using % 256"` |
|      - | 3095 | `			);` |
|      2 | 3096 | `	}` |
|      - | 3097 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 3098 | `	 * when taking the address of a wider int. */` |
|   7114 | 3099 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 3100 | `	/* Return the specified character */` |
|   7114 | 3101 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|   7114 | 3102 | `	return PH7_OK;` |
|   3560 | 3103 | `}` |
|      - | 3104 | `/*` |
|      - | 3105 | ` * Binary to hex consumer callback.` |
|      - | 3106 | ` * This callback is the default consumer used by the hash functions` |
|      - | 3107 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 3108 | ` */` |
|   3118 | 3109 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 3110 | `{` |
|      - | 3111 | `	/* Append hex chunk verbatim */` |
|   3120 | 3112 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   3120 | 3113 | `	return SXRET_OK;` |
|      2 | 3114 | `}` |
|      - | 3115 |  |
|      - | 3116 | `/*` |
|      - | 3117 | ` * string bin2hex(string $str)` |
|      - | 3118 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 3119 | ` * Parameters` |
|      - | 3120 | ` *  $str` |
|      - | 3121 | ` *   The input string.` |
|      - | 3122 | ` * Returns.` |
|      - | 3123 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 3124 | ` */` |
|    130 | 3125 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3126 | `{` |
|      - | 3127 | `	const char *zString;` |
|      - | 3128 | `	int nLen;` |
|      - | 3129 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|    133 | 3130 | `	if( nArg != 1 ){` |
|      4 | 3131 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3132 | `			"ArgumentCountError",` |
|      - | 3133 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|      1 | 3134 | `			nArg` |
|      - | 3135 | `			);` |
|      - | 3136 | `	}` |
|      - | 3137 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 3138 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 3139 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 3140 | `	 */` |
|    194 | 3141 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|     64 | 3142 | `		( ph7_value_is_object(apArg[0]) &&` |
|    ! 0 | 3143 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|    ! 0 | 3144 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|    ! 0 | 3145 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 3146 | `		)` |
|      - | 3147 | `	){` |
|    ! 0 | 3148 | `		const char *zType = ph7_type_name(apArg[0]);` |
|    ! 0 | 3149 | `		if( ph7_value_is_object(apArg[0]) ){` |
|    ! 0 | 3150 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    ! 0 | 3151 | `			if( pInst && pInst->pClass ){` |
|    ! 0 | 3152 | `				zType = SyStringData(&pInst->pClass->sName);` |
|    ! 0 | 3153 | `			}` |
|    ! 0 | 3154 | `		}` |
|    ! 0 | 3155 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3156 | `			"TypeError",` |
|      - | 3157 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 3158 | `			zType` |
|      - | 3159 | `			);` |
|      - | 3160 | `	}` |
|      - | 3161 | `	/* Extract the target string */` |
|    130 | 3162 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    130 | 3163 | `	if( nLen < 1 ){` |
|      - | 3164 | `		/* Empty string,return */` |
|     13 | 3165 | `		ph7_result_string(pCtx,"",0);` |
|     13 | 3166 | `		return PH7_OK;` |
|      - | 3167 | `	}` |
|      - | 3168 | `	/* Perform the requested operation */` |
|    118 | 3169 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|    118 | 3170 | `	return PH7_OK;` |
|     68 | 3171 | `}` |
|      - | 3172 |  |
|      - | 3173 | `/* Search callback signature */` |
|      - | 3174 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 3175 | `/*` |
|      - | 3176 | ` * Case-insensitive pattern match.` |
|      - | 3177 | ` * Brute force is the default search method used here.` |
|      - | 3178 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 3179 | ` * well for short/medium texts on modern hardware.` |
|      - | 3180 | ` */` |
|    298 | 3181 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      2 | 3182 | `{` |
|    300 | 3183 | `	const char *zpIn = (const char *)pPattern;` |
|    300 | 3184 | `	const char *zIn = (const char *)pText;` |
|    300 | 3185 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    300 | 3186 | `	const char *zEnd = &zIn[nLen];` |
|      - | 3187 | `	const char *zPtr,*zPtr2;` |
|      - | 3188 | `	int c,d;` |
|    300 | 3189 | `	if( iPatLen > nLen ){` |
|      - | 3190 | `		/* Don't bother processing */` |
|     67 | 3191 | `		return SXERR_NOTFOUND;` |
|      - | 3192 | `	}` |
|    860 | 3193 | `	for(;;){` |
|   1722 | 3194 | `		if( zIn >= zEnd ){` |
|    194 | 3195 | `			break;` |
|      - | 3196 | `		}` |
|   1530 | 3197 | `		c = SyToLower(zIn[0]);` |
|   1530 | 3198 | `		d = SyToLower(zpIn[0]);` |
|   1530 | 3199 | `		if( c == d ){` |
|    182 | 3200 | `			zPtr   = &zIn[1];` |
|    182 | 3201 | `			zPtr2  = &zpIn[1];` |
|    141 | 3202 | `			for(;;){` |
|    284 | 3203 | `				if( zPtr2 >= zpEnd ){` |
|      - | 3204 | `					/* Pattern found */` |
|     41 | 3205 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 3206 | `					return SXRET_OK;` |
|      - | 3207 | `				}` |
|    244 | 3208 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 3209 | `					break;` |
|      - | 3210 | `				}` |
|    244 | 3211 | `				c = SyToLower(zPtr[0]);` |
|    244 | 3212 | `				d = SyToLower(zPtr2[0]);` |
|    244 | 3213 | `				if( c != d ){` |
|    142 | 3214 | `					break;` |
|      - | 3215 | `				}` |
|    103 | 3216 | `				zPtr++; zPtr2++;` |
|      1 | 3217 | `			}` |
|     70 | 3218 | `		}` |
|   1490 | 3219 | `		zIn++;` |
|      2 | 3220 | `	}` |
|      - | 3221 | `	/* Pattern not found */` |
|    194 | 3222 | `	return SXERR_NOTFOUND;` |
|    151 | 3223 | `}` |
|      - | 3224 | `/*` |
|      - | 3225 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3226 | ` *  Find the first occurrence of a string.` |
|      - | 3227 | ` * Parameters` |
|      - | 3228 | ` *  $haystack` |
|      - | 3229 | ` *   The input string.` |
|      - | 3230 | ` * $needle` |
|      - | 3231 | ` *   Search pattern (must be a string).` |
|      - | 3232 | ` * $before_needle` |
|      - | 3233 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3234 | ` *   of the needle (excluding the needle).` |
|      - | 3235 | ` * Return` |
|      - | 3236 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3237 | ` */` |
|      6 | 3238 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3239 | `{` |
|      7 | 3240 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3241 | `	const char *zBlob,*zPattern;` |
|      - | 3242 | `	int nLen,nPatLen;` |
|      - | 3243 | `	sxu32 nOfft;` |
|      - | 3244 | `	sxi32 rc;` |
|      7 | 3245 | `	if( nArg < 2 ){` |
|      - | 3246 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3247 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3248 | `		return PH7_OK;` |
|      - | 3249 | `	}` |
|      - | 3250 | `	/* Extract the needle and the haystack */` |
|      7 | 3251 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 3252 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 3253 | `	nOfft = 0; /* cc warning */` |
|      9 | 3254 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3255 | `		int before = 0;` |
|      - | 3256 | `		/* Perform the lookup */` |
|      5 | 3257 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3258 | `		if( rc != SXRET_OK ){` |
|      - | 3259 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3260 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3261 | `			return PH7_OK;` |
|      - | 3262 | `		}` |
|      - | 3263 | `		/* Return the portion of the string */` |
|      5 | 3264 | `		if( nArg > 2 ){` |
|      3 | 3265 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3266 | `		}` |
|      5 | 3267 | `		if( before ){` |
|      3 | 3268 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3269 | `		}else{` |
|      3 | 3270 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3271 | `		}` |
|      3 | 3272 | `	}else{` |
|      3 | 3273 | `		ph7_result_bool(pCtx,0);` |
|      - | 3274 | `	}` |
|      7 | 3275 | `	return PH7_OK;` |
|      4 | 3276 | `}` |
|      - | 3277 | `/*` |
|      - | 3278 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3279 | ` *  Case-insensitive strstr().` |
|      - | 3280 | ` * Parameters` |
|      - | 3281 | ` *  $haystack` |
|      - | 3282 | ` *   The input string.` |
|      - | 3283 | ` * $needle` |
|      - | 3284 | ` *   Search pattern (must be a string).` |
|      - | 3285 | ` * $before_needle` |
|      - | 3286 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3287 | ` *   of the needle (excluding the needle).` |
|      - | 3288 | ` * Return` |
|      - | 3289 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3290 | ` */` |
|      4 | 3291 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3292 | `{` |
|      5 | 3293 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3294 | `	const char *zBlob,*zPattern;` |
|      - | 3295 | `	int nLen,nPatLen;` |
|      - | 3296 | `	sxu32 nOfft;` |
|      - | 3297 | `	sxi32 rc;` |
|      5 | 3298 | `	if( nArg < 2 ){` |
|      - | 3299 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3300 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3301 | `		return PH7_OK;` |
|      - | 3302 | `	}` |
|      - | 3303 | `	/* Extract the needle and the haystack */` |
|      5 | 3304 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3305 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 3306 | `	nOfft = 0; /* cc warning */` |
|      7 | 3307 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3308 | `		int before = 0;` |
|      - | 3309 | `		/* Perform the lookup */` |
|      5 | 3310 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3311 | `		if( rc != SXRET_OK ){` |
|      - | 3312 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3313 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3314 | `			return PH7_OK;` |
|      - | 3315 | `		}` |
|      - | 3316 | `		/* Return the portion of the string */` |
|      5 | 3317 | `		if( nArg > 2 ){` |
|      3 | 3318 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3319 | `		}` |
|      5 | 3320 | `		if( before ){` |
|      3 | 3321 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3322 | `		}else{` |
|      3 | 3323 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3324 | `		}` |
|      3 | 3325 | `	}else{` |
|    ! 0 | 3326 | `		ph7_result_bool(pCtx,0);` |
|      - | 3327 | `	}` |
|      5 | 3328 | `	return PH7_OK;` |
|      3 | 3329 | `}` |
|      - | 3330 | `/*` |
|      - | 3331 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3332 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 3333 | ` * Parameters` |
|      - | 3334 | ` *  $haystack` |
|      - | 3335 | ` *   The input string.` |
|      - | 3336 | ` * $needle` |
|      - | 3337 | ` *   Search pattern (must be a string).` |
|      - | 3338 | ` * $offset` |
|      - | 3339 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3340 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3341 | ` *   of haystack.` |
|      - | 3342 | ` * Return` |
|      - | 3343 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3344 | ` */` |
|   1468 | 3345 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3346 | `{` |
|   1473 | 3347 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strpos",1,"$haystack"); }` |
|   1473 | 3348 | `	if( nArg > 1 ){ StrNullArgNotice(pCtx,apArg[1],"strpos",2,"$needle"); }` |
|   1473 | 3349 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3350 | `	const char *zBlob,*zPattern;` |
|      - | 3351 | `	int nLen,nPatLen,nStart;` |
|      - | 3352 | `	sxu32 nOfft;` |
|      - | 3353 | `	sxi32 rc;` |
|   1473 | 3354 | `	if( nArg < 2 ){` |
|      - | 3355 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3356 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3357 | `		return PH7_OK;` |
|      - | 3358 | `	}` |
|      - | 3359 | `	/* Extract the needle and the haystack */` |
|   1473 | 3360 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|   1473 | 3361 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|   1473 | 3362 | `	nOfft = 0; /* cc warning */` |
|   1473 | 3363 | `	nStart = 0;` |
|      - | 3364 | `	/* Peek the starting offset if available */` |
|   1473 | 3365 | `	if( nArg > 2 ){` |
|     15 | 3366 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3367 | `		if( nStart < 0 ){` |
|    ! 0 | 3368 | `			nStart = -nStart;` |
|    ! 0 | 3369 | `		}` |
|     15 | 3370 | `		if( nStart >= nLen ){` |
|      - | 3371 | `			/* Invalid offset */` |
|    ! 0 | 3372 | `			nStart = 0;` |
|    ! 0 | 3373 | `		}else{` |
|     15 | 3374 | `			zBlob += nStart;` |
|     15 | 3375 | `			nLen -= nStart;` |
|      - | 3376 | `		}` |
|      7 | 3377 | `	}` |
|   1473 | 3378 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3379 | `		/* Perform the lookup */` |
|   1471 | 3380 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|   1471 | 3381 | `		if( rc != SXRET_OK ){` |
|      - | 3382 | `			/* Pattern not found,return FALSE */` |
|    779 | 3383 | `			ph7_result_bool(pCtx,0);` |
|    779 | 3384 | `			return PH7_OK;` |
|      - | 3385 | `		}` |
|      - | 3386 | `		/* Return the pattern position */` |
|    696 | 3387 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|    350 | 3388 | `	}else{` |
|      3 | 3389 | `		ph7_result_bool(pCtx,0);` |
|      - | 3390 | `	}` |
|    698 | 3391 | `	return PH7_OK;` |
|    739 | 3392 | `}` |
|      - | 3393 | `/*` |
|      - | 3394 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|      - | 3395 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|      - | 3396 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|      - | 3397 | ` * TypeError for arrays, resources, and objects without __toString.` |
|      - | 3398 | ` *` |
|      - | 3399 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|      - | 3400 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|      - | 3401 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|      - | 3402 | ` *` |
|      - | 3403 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|      - | 3404 | ` * is valid until pTmp is released or pArg is mutated.` |
|      - | 3405 | ` */` |
|    668 | 3406 | `static sxi32 StrPredicateResolveArg(` |
|      - | 3407 | `	ph7_context *pCtx,` |
|      - | 3408 | `	ph7_value *pArg,` |
|      - | 3409 | `	const char *zFunc,` |
|      - | 3410 | `	int iArgNum,` |
|      - | 3411 | `	const char *zParamName,` |
|      - | 3412 | `	const char *zTypeStr, /* Declared type in the TypeError, e.g. "string" / "?string" */` |
|      - | 3413 | `	const char *zNullMsg,` |
|      - | 3414 | `	ph7_value *pTmp,` |
|      - | 3415 | `	const char **pzOut,` |
|      - | 3416 | `	int *pnOut` |
|      2 | 3417 | `){` |
|    670 | 3418 | `	if( ph7_value_is_null(pArg) ){` |
|     13 | 3419 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zNullMsg);` |
|     13 | 3420 | `		*pzOut = "";` |
|     13 | 3421 | `		*pnOut = 0;` |
|     13 | 3422 | `		return PH7_OK;` |
|      - | 3423 | `	}` |
|   1010 | 3424 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|    656 | 3425 | `	    ( ph7_value_is_object(pArg) &&` |
|     72 | 3426 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|     48 | 3427 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|     24 | 3428 | `	        "__toString",sizeof("__toString")-1) == 0` |
|      - | 3429 | `	    )` |
|      - | 3430 | `	){` |
|    ! 0 | 3431 | `		const char *zType = ph7_type_name(pArg);` |
|    ! 0 | 3432 | `		if( ph7_value_is_object(pArg) ){` |
|    ! 0 | 3433 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|    ! 0 | 3434 | `			if( pInst && pInst->pClass ){` |
|    ! 0 | 3435 | `				zType = SyStringData(&pInst->pClass->sName);` |
|    ! 0 | 3436 | `			}` |
|    ! 0 | 3437 | `		}` |
|    ! 0 | 3438 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3439 | `			"TypeError",` |
|      - | 3440 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|    ! 0 | 3441 | `			zFunc, iArgNum, zParamName, zTypeStr, zType` |
|      - | 3442 | `			);` |
|      - | 3443 | `	}` |
|    658 | 3444 | `	if( ph7_value_is_object(pArg) ){` |
|     49 | 3445 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     49 | 3446 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 3447 | `			"__toString",sizeof("__toString")-1);` |
|     49 | 3448 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|     49 | 3449 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|     49 | 3450 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|     49 | 3451 | `		return PH7_OK;` |
|      - | 3452 | `	}` |
|    610 | 3453 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|    610 | 3454 | `	return PH7_OK;` |
|    336 | 3455 | `}` |
|      - | 3456 | `/*` |
|      - | 3457 | ` * bool str_contains(string $haystack, string $needle)` |
|      - | 3458 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|      - | 3459 | ` * Return` |
|      - | 3460 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|      - | 3461 | ` */` |
|     92 | 3462 | `static int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3463 | `{` |
|      - | 3464 | `	const char *zHaystack,*zNeedle;` |
|      - | 3465 | `	int nHayLen,nNeedleLen;` |
|      - | 3466 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3467 | `	sxi32 rc;` |
|     95 | 3468 | `	if( nArg != 2 ){` |
|      8 | 3469 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3470 | `			"ArgumentCountError",` |
|      - | 3471 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|      2 | 3472 | `			nArg` |
|      - | 3473 | `			);` |
|      - | 3474 | `	}` |
|     90 | 3475 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     90 | 3476 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     90 | 3477 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack","string",` |
|      - | 3478 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|      - | 3479 | `		"of type string is deprecated",` |
|      - | 3480 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     90 | 3481 | `	if( rc != PH7_OK ) goto out;` |
|     90 | 3482 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle","string",` |
|      - | 3483 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|      - | 3484 | `		"of type string is deprecated",` |
|      - | 3485 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     90 | 3486 | `	if( rc != PH7_OK ) goto out;` |
|     90 | 3487 | `	if( nNeedleLen < 1 ){` |
|     13 | 3488 | `		ph7_result_bool(pCtx,1);` |
|     84 | 3489 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3490 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3491 | `	}else{` |
|    104 | 3492 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|     34 | 3493 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|     70 | 3494 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|      - | 3495 | `	}` |
|     90 | 3496 | `	rc = PH7_OK;` |
|     44 | 3497 | `out:` |
|     90 | 3498 | `	PH7_MemObjRelease(&sHayTmp);` |
|     90 | 3499 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     90 | 3500 | `	return rc;` |
|     49 | 3501 | `}` |
|      - | 3502 | `/*` |
|      - | 3503 | ` * bool str_starts_with(string $haystack, string $needle)` |
|      - | 3504 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|      - | 3505 | ` * Return` |
|      - | 3506 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|      - | 3507 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3508 | ` */` |
|     62 | 3509 | `static int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3510 | `{` |
|      - | 3511 | `	const char *zHaystack,*zNeedle;` |
|      - | 3512 | `	int nHayLen,nNeedleLen;` |
|      - | 3513 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3514 | `	sxi32 rc;` |
|     64 | 3515 | `	if( nArg != 2 ){` |
|      8 | 3516 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3517 | `			"ArgumentCountError",` |
|      - | 3518 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|      2 | 3519 | `			nArg` |
|      - | 3520 | `			);` |
|      - | 3521 | `	}` |
|     59 | 3522 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     59 | 3523 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     59 | 3524 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack","string",` |
|      - | 3525 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3526 | `		"of type string is deprecated",` |
|      - | 3527 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     59 | 3528 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3529 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle","string",` |
|      - | 3530 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3531 | `		"of type string is deprecated",` |
|      - | 3532 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     59 | 3533 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3534 | `	if( nNeedleLen < 1 ){` |
|     13 | 3535 | `		ph7_result_bool(pCtx,1);` |
|     53 | 3536 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3537 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3538 | `	}else{` |
|     58 | 3539 | `		ph7_result_bool(pCtx,` |
|     38 | 3540 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3541 | `	}` |
|     59 | 3542 | `	rc = PH7_OK;` |
|     29 | 3543 | `out:` |
|     59 | 3544 | `	PH7_MemObjRelease(&sHayTmp);` |
|     59 | 3545 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     59 | 3546 | `	return rc;` |
|     33 | 3547 | `}` |
|      - | 3548 | `/*` |
|      - | 3549 | ` * bool str_ends_with(string $haystack, string $needle)` |
|      - | 3550 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|      - | 3551 | ` * Return` |
|      - | 3552 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|      - | 3553 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3554 | ` */` |
|     62 | 3555 | `static int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3556 | `{` |
|      - | 3557 | `	const char *zHaystack,*zNeedle;` |
|      - | 3558 | `	int nHayLen,nNeedleLen;` |
|      - | 3559 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3560 | `	sxi32 rc;` |
|     64 | 3561 | `	if( nArg != 2 ){` |
|      8 | 3562 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3563 | `			"ArgumentCountError",` |
|      - | 3564 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|      2 | 3565 | `			nArg` |
|      - | 3566 | `			);` |
|      - | 3567 | `	}` |
|     59 | 3568 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     59 | 3569 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     59 | 3570 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack","string",` |
|      - | 3571 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3572 | `		"of type string is deprecated",` |
|      - | 3573 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     59 | 3574 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3575 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle","string",` |
|      - | 3576 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3577 | `		"of type string is deprecated",` |
|      - | 3578 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     59 | 3579 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3580 | `	if( nNeedleLen < 1 ){` |
|     13 | 3581 | `		ph7_result_bool(pCtx,1);` |
|     53 | 3582 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3583 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3584 | `	}else{` |
|     58 | 3585 | `		ph7_result_bool(pCtx,` |
|     38 | 3586 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3587 | `	}` |
|     59 | 3588 | `	rc = PH7_OK;` |
|     29 | 3589 | `out:` |
|     59 | 3590 | `	PH7_MemObjRelease(&sHayTmp);` |
|     59 | 3591 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     59 | 3592 | `	return rc;` |
|     33 | 3593 | `}` |
|      - | 3594 | `/*` |
|      - | 3595 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3596 | ` *  Case-insensitive strpos.` |
|      - | 3597 | ` * Parameters` |
|      - | 3598 | ` *  $haystack` |
|      - | 3599 | ` *   The input string.` |
|      - | 3600 | ` * $needle` |
|      - | 3601 | ` *   Search pattern (must be a string).` |
|      - | 3602 | ` * $offset` |
|      - | 3603 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3604 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3605 | ` *   of haystack.` |
|      - | 3606 | ` * Return` |
|      - | 3607 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3608 | ` */` |
|    196 | 3609 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3610 | `{` |
|    198 | 3611 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3612 | `	const char *zBlob,*zPattern;` |
|      - | 3613 | `	int nLen,nPatLen,nStart;` |
|      - | 3614 | `	sxu32 nOfft;` |
|      - | 3615 | `	sxi32 rc;` |
|    198 | 3616 | `	if( nArg < 2 ){` |
|      - | 3617 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3618 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3619 | `		return PH7_OK;` |
|      - | 3620 | `	}` |
|      - | 3621 | `	/* Extract the needle and the haystack */` |
|    198 | 3622 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    198 | 3623 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    198 | 3624 | `	nOfft = 0; /* cc warning */` |
|    198 | 3625 | `	nStart = 0;` |
|      - | 3626 | `	/* Peek the starting offset if available */` |
|    198 | 3627 | `	if( nArg > 2 ){` |
|      5 | 3628 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 3629 | `		if( nStart < 0 ){` |
|      3 | 3630 | `			nStart = -nStart;` |
|      1 | 3631 | `		}` |
|      5 | 3632 | `		if( nStart >= nLen ){` |
|      - | 3633 | `			/* Invalid offset */` |
|    ! 0 | 3634 | `			nStart = 0;` |
|    ! 0 | 3635 | `		}else{` |
|      5 | 3636 | `			zBlob += nStart;` |
|      5 | 3637 | `			nLen -= nStart;` |
|      - | 3638 | `		}` |
|      2 | 3639 | `	}` |
|    198 | 3640 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3641 | `		/* Perform the lookup */` |
|    198 | 3642 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    198 | 3643 | `		if( rc != SXRET_OK ){` |
|      - | 3644 | `			/* Pattern not found,return FALSE */` |
|    184 | 3645 | `			ph7_result_bool(pCtx,0);` |
|    184 | 3646 | `			return PH7_OK;` |
|      - | 3647 | `		}` |
|      - | 3648 | `		/* Return the pattern position */` |
|     15 | 3649 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 3650 | `	}else{` |
|    ! 0 | 3651 | `		ph7_result_bool(pCtx,0);` |
|      - | 3652 | `	}` |
|     15 | 3653 | `	return PH7_OK;` |
|    100 | 3654 | `}` |
|      - | 3655 | `/*` |
|      - | 3656 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3657 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 3658 | ` * Parameters` |
|      - | 3659 | ` *  $haystack` |
|      - | 3660 | ` *   The input string.` |
|      - | 3661 | ` * $needle` |
|      - | 3662 | ` *   Search pattern (must be a string).` |
|      - | 3663 | ` * $offset` |
|      - | 3664 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3665 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3666 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3667 | ` * Return` |
|      - | 3668 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3669 | ` */` |
|     40 | 3670 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3671 | `{` |
|      - | 3672 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     41 | 3673 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3674 | `	int nLen,nPatLen;` |
|      - | 3675 | `	sxu32 nOfft;` |
|      - | 3676 | `	sxi32 rc;` |
|     41 | 3677 | `	if( nArg < 2 ){` |
|      - | 3678 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3679 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3680 | `		return PH7_OK;` |
|      - | 3681 | `	}` |
|      - | 3682 | `	/* Extract the needle and the haystack */` |
|     41 | 3683 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 3684 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3685 | `	/* Point to the end of the pattern */` |
|     41 | 3686 | `	zPtr = &zBlob[nLen - 1];` |
|     41 | 3687 | `	zEnd = &zBlob[nLen];` |
|      - | 3688 | `	/* Save the starting posistion */` |
|     41 | 3689 | `	zStart = zBlob;` |
|     41 | 3690 | `	nOfft = 0; /* cc warning */` |
|      - | 3691 | `	/* Peek the starting offset if available */` |
|     41 | 3692 | `	if( nArg > 2 ){` |
|      - | 3693 | `		int nStart;` |
|     21 | 3694 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 3695 | `		if( nStart < 0 ){` |
|     11 | 3696 | `			nStart = -nStart;` |
|     11 | 3697 | `			if( nStart >= nLen ){` |
|      - | 3698 | `				/* Invalid offset */` |
|      3 | 3699 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3700 | `				return PH7_OK;` |
|    ! 0 | 3701 | `			}else{` |
|      9 | 3702 | `				nLen -= nStart;` |
|      9 | 3703 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 3704 | `				zEnd = &zBlob[nLen];` |
|      - | 3705 | `			}` |
|      5 | 3706 | `		}else{` |
|     11 | 3707 | `			if( nStart >= nLen ){` |
|      - | 3708 | `				/* Invalid offset */` |
|      5 | 3709 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3710 | `				return PH7_OK;` |
|    ! 0 | 3711 | `			}else{` |
|      7 | 3712 | `				zBlob += nStart;` |
|      7 | 3713 | `				nLen -= nStart;` |
|      - | 3714 | `			}` |
|      - | 3715 | `		}` |
|      7 | 3716 | `	}` |
|     35 | 3717 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3718 | `		/* Perform the lookup */` |
|    121 | 3719 | `		for(;;){` |
|    243 | 3720 | `			if( zBlob >= zPtr ){` |
|     21 | 3721 | `				break;` |
|      - | 3722 | `			}` |
|    223 | 3723 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    223 | 3724 | `			if( rc == SXRET_OK ){` |
|      - | 3725 | `				/* Pattern found,return it's position */` |
|     13 | 3726 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 3727 | `				return PH7_OK;` |
|      - | 3728 | `			}` |
|    211 | 3729 | `			zPtr--;` |
|      1 | 3730 | `		}` |
|      - | 3731 | `		/* Pattern not found,return FALSE */` |
|     21 | 3732 | `		ph7_result_bool(pCtx,0);` |
|     11 | 3733 | `	}else{` |
|      3 | 3734 | `		ph7_result_bool(pCtx,0);` |
|      - | 3735 | `	}` |
|     23 | 3736 | `	return PH7_OK;` |
|     21 | 3737 | `}` |
|      - | 3738 | `/*` |
|      - | 3739 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3740 | ` *  Case-insensitive strrpos.` |
|      - | 3741 | ` * Parameters` |
|      - | 3742 | ` *  $haystack` |
|      - | 3743 | ` *   The input string.` |
|      - | 3744 | ` * $needle` |
|      - | 3745 | ` *   Search pattern (must be a string).` |
|      - | 3746 | ` * $offset` |
|      - | 3747 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3748 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3749 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3750 | ` * Return` |
|      - | 3751 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3752 | ` */` |
|     26 | 3753 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3754 | `{` |
|      - | 3755 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     27 | 3756 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3757 | `	int nLen,nPatLen;` |
|      - | 3758 | `	sxu32 nOfft;` |
|      - | 3759 | `	sxi32 rc;` |
|     27 | 3760 | `	if( nArg < 2 ){` |
|      - | 3761 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3762 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3763 | `		return PH7_OK;` |
|      - | 3764 | `	}` |
|      - | 3765 | `	/* Extract the needle and the haystack */` |
|     27 | 3766 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 3767 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3768 | `	/* Point to the end of the pattern */` |
|     27 | 3769 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 3770 | `	zEnd = &zBlob[nLen];` |
|      - | 3771 | `	/* Save the starting posistion */` |
|     27 | 3772 | `	zStart = zBlob;` |
|     27 | 3773 | `	nOfft = 0; /* cc warning */` |
|      - | 3774 | `	/* Peek the starting offset if available */` |
|     27 | 3775 | `	if( nArg > 2 ){` |
|      - | 3776 | `		int nStart;` |
|     15 | 3777 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3778 | `		if( nStart < 0 ){` |
|      7 | 3779 | `			nStart = -nStart;` |
|      7 | 3780 | `			if( nStart >= nLen ){` |
|      - | 3781 | `				/* Invalid offset */` |
|      3 | 3782 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3783 | `				return PH7_OK;` |
|    ! 0 | 3784 | `			}else{` |
|      5 | 3785 | `				nLen -= nStart;` |
|      5 | 3786 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 3787 | `				zEnd = &zBlob[nLen];` |
|      - | 3788 | `			}` |
|      3 | 3789 | `		}else{` |
|      9 | 3790 | `			if( nStart >= nLen ){` |
|      - | 3791 | `				/* Invalid offset */` |
|      5 | 3792 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3793 | `				return PH7_OK;` |
|    ! 0 | 3794 | `			}else{` |
|      5 | 3795 | `				zBlob += nStart;` |
|      5 | 3796 | `				nLen -= nStart;` |
|      - | 3797 | `			}` |
|      - | 3798 | `		}` |
|      4 | 3799 | `	}` |
|     21 | 3800 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3801 | `		/* Perform the lookup */` |
|     44 | 3802 | `		for(;;){` |
|     89 | 3803 | `			if( zBlob >= zPtr ){` |
|      9 | 3804 | `				break;` |
|      - | 3805 | `			}` |
|     81 | 3806 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 3807 | `			if( rc == SXRET_OK ){` |
|      - | 3808 | `				/* Pattern found,return it's position */` |
|     11 | 3809 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 3810 | `				return PH7_OK;` |
|      - | 3811 | `			}` |
|     71 | 3812 | `			zPtr--;` |
|      1 | 3813 | `		}` |
|      - | 3814 | `		/* Pattern not found,return FALSE */` |
|      9 | 3815 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3816 | `	}else{` |
|      3 | 3817 | `		ph7_result_bool(pCtx,0);` |
|      - | 3818 | `	}` |
|     11 | 3819 | `	return PH7_OK;` |
|     14 | 3820 | `}` |
|      - | 3821 | `/*` |
|      - | 3822 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3823 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3824 | ` * Parameters` |
|      - | 3825 | ` *  $haystack` |
|      - | 3826 | ` *   The input string.` |
|      - | 3827 | ` * $needle` |
|      - | 3828 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3829 | ` *  This behavior is different from that of strstr().` |
|      - | 3830 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3831 | ` *  as the ordinal value of a character.` |
|      - | 3832 | ` * Return` |
|      - | 3833 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3834 | ` */` |
|     22 | 3835 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3836 | `{` |
|      - | 3837 | `	const char *zBlob;` |
|      - | 3838 | `	int nLen,c;` |
|     23 | 3839 | `	if( nArg < 2 ){` |
|      - | 3840 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3841 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3842 | `		return PH7_OK;` |
|      - | 3843 | `	}` |
|      - | 3844 | `	/* Extract the haystack */` |
|     23 | 3845 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3846 | `	c = 0; /* cc warning */` |
|     23 | 3847 | `	if( nLen > 0 ){` |
|      - | 3848 | `		sxu32 nOfft;` |
|      - | 3849 | `		sxi32 rc;` |
|     21 | 3850 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 3851 | `			const char *zPattern;` |
|     11 | 3852 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 3853 | `														 * for NULL pointer.` |
|      - | 3854 | `														 */` |
|     11 | 3855 | `			c = zPattern[0];` |
|      6 | 3856 | `		}else{` |
|      - | 3857 | `			/* Int cast */` |
|     11 | 3858 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 3859 | `		}` |
|      - | 3860 | `		/* Perform the lookup */` |
|     21 | 3861 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3862 | `		if( rc != SXRET_OK ){` |
|      - | 3863 | `			/* No such entry,return FALSE */` |
|      7 | 3864 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3865 | `			return PH7_OK;` |
|      - | 3866 | `		}` |
|      - | 3867 | `		/* Return the string portion */` |
|     15 | 3868 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 3869 | `	}else{` |
|      3 | 3870 | `		ph7_result_bool(pCtx,0);` |
|      - | 3871 | `	}` |
|     17 | 3872 | `	return PH7_OK;` |
|     12 | 3873 | `}` |
|      - | 3874 | `/*` |
|      - | 3875 | ` * string strrev(string $string)` |
|      - | 3876 | ` *  Reverse a string.` |
|      - | 3877 | ` * Parameters` |
|      - | 3878 | ` *  $string` |
|      - | 3879 | ` *   String to be reversed.` |
|      - | 3880 | ` * Return` |
|      - | 3881 | ` *  The reversed string.` |
|      - | 3882 | ` */` |
|      2 | 3883 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3884 | `{` |
|      - | 3885 | `	const char *zIn,*zEnd;` |
|      - | 3886 | `	int nLen,c;` |
|      3 | 3887 | `	if( nArg < 1 ){` |
|      - | 3888 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3889 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3890 | `		return PH7_OK;` |
|      - | 3891 | `	}` |
|      - | 3892 | `	/* Extract the target string */` |
|      3 | 3893 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3894 | `	if( nLen < 1 ){` |
|      - | 3895 | `		/* Empty string Return null */` |
|    ! 0 | 3896 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3897 | `		return PH7_OK;` |
|      - | 3898 | `	}` |
|      - | 3899 | `	/* Perform the requested operation */` |
|      3 | 3900 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3901 | `	for(;;){` |
|      9 | 3902 | `		if( zEnd < zIn ){` |
|      - | 3903 | `			/* No more input to process */` |
|      3 | 3904 | `			break;` |
|      - | 3905 | `		}` |
|      - | 3906 | `		/* Append current character */` |
|      7 | 3907 | `		c = zEnd[0];` |
|      7 | 3908 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3909 | `		zEnd--;` |
|      1 | 3910 | `	}` |
|      3 | 3911 | `	return PH7_OK;` |
|      2 | 3912 | `}` |
|      - | 3913 | `/*` |
|      - | 3914 | ` * string ucwords(string $string [, string $separators = " \t\r\n\f\v"])` |
|      - | 3915 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3916 | ` *  A word begins at the start of the string and after any character present in` |
|      - | 3917 | ` *  $separators. The default separators are the whitespace characters (space,` |
|      - | 3918 | ` *  horizontal tab, carriage return, newline, form-feed and vertical tab); an` |
|      - | 3919 | ` *  explicit $separators argument REPLACES them (an empty string leaves only the` |
|      - | 3920 | ` *  very first character upper-cased). Like PHP, this is byte-based: only ASCII` |
|      - | 3921 | ` *  bytes are upper-cased and a byte is a separator only if it appears in the set.` |
|      - | 3922 | ` * Parameters` |
|      - | 3923 | ` *  $string` |
|      - | 3924 | ` *   The input string.` |
|      - | 3925 | ` *  $separators` |
|      - | 3926 | ` *   The optional word-boundary characters.` |
|      - | 3927 | ` * Return` |
|      - | 3928 | ` *  The modified string.` |
|      - | 3929 | ` */` |
|     22 | 3930 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3931 | `{` |
|      - | 3932 | `	const char *zIn;` |
|      - | 3933 | `	int nLen,i,iStart;` |
|      - | 3934 | `	char aDelim[256];` |
|     23 | 3935 | `	if( nArg < 1 ){` |
|      - | 3936 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3937 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3938 | `		return PH7_OK;` |
|      - | 3939 | `	}` |
|      - | 3940 | `	/* Build the separator membership table: an explicit $separators argument` |
|      - | 3941 | `	 * replaces the default whitespace set (an empty string clears it). */` |
|     23 | 3942 | `	SyZero(aDelim,(sxu32)sizeof(aDelim));` |
|     23 | 3943 | `	if( nArg > 1 ){` |
|      - | 3944 | `		int nDelim;` |
|      9 | 3945 | `		const char *zDelim = ph7_value_to_string(apArg[1],&nDelim);` |
|     17 | 3946 | `		for( i = 0 ; i < nDelim ; i++ ){` |
|      9 | 3947 | `			aDelim[(unsigned char)zDelim[i]] = 1;` |
|      5 | 3948 | `		}` |
|      5 | 3949 | `	}else{` |
|     15 | 3950 | `		aDelim[(unsigned char)' ']  = 1;` |
|     15 | 3951 | `		aDelim[(unsigned char)'\t'] = 1;` |
|     15 | 3952 | `		aDelim[(unsigned char)'\r'] = 1;` |
|     15 | 3953 | `		aDelim[(unsigned char)'\n'] = 1;` |
|     15 | 3954 | `		aDelim[(unsigned char)'\f'] = 1;` |
|     15 | 3955 | `		aDelim[(unsigned char)'\v'] = 1;` |
|      - | 3956 | `	}` |
|      - | 3957 | `	/* Extract the target string */` |
|     23 | 3958 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3959 | `	if( nLen < 1 ){` |
|      - | 3960 | `		/* Empty string – match PHP semantics */` |
|      3 | 3961 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3962 | `		return PH7_OK;` |
|      - | 3963 | `	}` |
|      - | 3964 | `	/* Upper-case the first byte of each word (the leading byte, or any byte that` |
|      - | 3965 | `	 * follows a separator), appending the untouched runs in between verbatim. */` |
|     21 | 3966 | `	iStart = 0;` |
|    309 | 3967 | `	for( i = 0 ; i < nLen ; i++ ){` |
|    289 | 3968 | `		int c = (unsigned char)zIn[i];` |
|    289 | 3969 | `		if( (i == 0 \|\| aDelim[(unsigned char)zIn[i-1]]) && c < 0x80 && SyisLower(c) ){` |
|     53 | 3970 | `			char up = (char)SyToUpper(c);` |
|     53 | 3971 | `			if( i > iStart ){` |
|     35 | 3972 | `				ph7_result_string(pCtx,&zIn[iStart],i - iStart);` |
|     17 | 3973 | `			}` |
|     53 | 3974 | `			ph7_result_string(pCtx,&up,1);` |
|     53 | 3975 | `			iStart = i + 1;` |
|     26 | 3976 | `		}` |
|    145 | 3977 | `	}` |
|     21 | 3978 | `	if( nLen > iStart ){` |
|     21 | 3979 | `		ph7_result_string(pCtx,&zIn[iStart],nLen - iStart);` |
|     10 | 3980 | `	}` |
|     21 | 3981 | `	return PH7_OK;` |
|     12 | 3982 | `}` |
|      - | 3983 | `/*` |
|      - | 3984 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3985 | ` *  Returns input repeated multiplier times.` |
|      - | 3986 | ` * Parameters` |
|      - | 3987 | ` *  $string` |
|      - | 3988 | ` *   String to be repeated.` |
|      - | 3989 | ` * $multiplier` |
|      - | 3990 | ` *  Number of time the input string should be repeated.` |
|      - | 3991 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3992 | ` *  to 0, the function will return an empty string.` |
|      - | 3993 | ` * Return` |
|      - | 3994 | ` *  The repeated string.` |
|      - | 3995 | ` */` |
|  20434 | 3996 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3997 | `{` |
|      - | 3998 | `	const char *zIn;` |
|      - | 3999 | `	int nLen;` |
|      - | 4000 | `	ph7_int64 nMul;` |
|      - | 4001 | `	int rc;` |
|  20436 | 4002 | `	if( nArg < 2 ){` |
|      - | 4003 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4004 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4005 | `		return PH7_OK;` |
|      - | 4006 | `	}` |
|      - | 4007 | `	/* Extract the target string */` |
|  20436 | 4008 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4009 | `	/* Resolve $times through the shared ZPP helper so a lossy float / float-string` |
|      - | 4010 | `	 * carries php's precision deprecation and NAN/INF/non-numeric fail with php's` |
|      - | 4011 | `	 * TypeError — a bare ph7_value_to_int64() coerced them silently. */` |
|      - | 4012 | `	{` |
|  20436 | 4013 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_repeat",2,"$times","int",&nMul);` |
|  20436 | 4014 | `		if( rcArg != PH7_OK ){` |
|    ! 0 | 4015 | `			return rcArg;` |
|      - | 4016 | `		}` |
|      - | 4017 | `	}` |
|  20436 | 4018 | `	if( nMul < 0 ){` |
|      3 | 4019 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4020 | `			"str_repeat(): Argument #2 ($times) must be greater than or equal to 0");` |
|      - | 4021 | `	}` |
|  20434 | 4022 | `	if( nLen < 1 \|\| nMul < 1 ){` |
|      - | 4023 | `		/* Empty input or a zero multiplier yields the empty string (PHP). */` |
|    ! 0 | 4024 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4025 | `		return PH7_OK;` |
|      - | 4026 | `	}` |
|      - | 4027 | `	/* Perform the requested operation */` |
| 221930 | 4028 | `	for(;;){` |
| 443862 | 4029 | `		if( !nMul ){` |
|  20434 | 4030 | `			break;` |
|      - | 4031 | `		}` |
|      - | 4032 | `		/* Append the copy */` |
| 423430 | 4033 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 423430 | 4034 | `		if( rc != PH7_OK ){` |
|      - | 4035 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 4036 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 4037 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 4038 | `		}` |
| 423430 | 4039 | `		nMul--;` |
|      2 | 4040 | `	}` |
|  20434 | 4041 | `	return PH7_OK;` |
|  10219 | 4042 | `}` |
|      - | 4043 | `/*` |
|      - | 4044 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 4045 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 4046 | ` * Parameters` |
|      - | 4047 | ` *  $string` |
|      - | 4048 | ` *   The input string.` |
|      - | 4049 | ` * $is_xhtml` |
|      - | 4050 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 4051 | ` * Return` |
|      - | 4052 | ` *  The processed string.` |
|      - | 4053 | ` */` |
|      4 | 4054 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4055 | `{` |
|      - | 4056 | `	const char *zIn,*zCur,*zEnd;` |
|      5 | 4057 | `	int is_xhtml = 1; /* Default to XHTML-style '<br />' like PHP */` |
|      - | 4058 | `	int nLen;` |
|      5 | 4059 | `	if( nArg < 1 ){` |
|      - | 4060 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4061 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4062 | `		return PH7_OK;` |
|      - | 4063 | `	}` |
|      - | 4064 | `	/* Extract the target string */` |
|      5 | 4065 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 4066 | `	if( nLen < 1 ){` |
|      - | 4067 | `		/* Empty string,return null */` |
|    ! 0 | 4068 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4069 | `		return PH7_OK;` |
|      - | 4070 | `	}` |
|      5 | 4071 | `	if( nArg > 1 ){` |
|      3 | 4072 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 4073 | `	}` |
|      5 | 4074 | `	zEnd = &zIn[nLen];` |
|      - | 4075 | `	/* Perform the requested operation */` |
|      4 | 4076 | `	for(;;){` |
|      9 | 4077 | `		zCur = zIn;` |
|      - | 4078 | `		/* Delimit the string */` |
|     21 | 4079 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 4080 | `			zIn++;` |
|      1 | 4081 | `		}` |
|      9 | 4082 | `		if( zCur < zIn ){` |
|      - | 4083 | `			/* Output chunk verbatim */` |
|      9 | 4084 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 4085 | `		}` |
|      9 | 4086 | `		if( zIn >= zEnd ){` |
|      - | 4087 | `			/* No more input to process */` |
|      5 | 4088 | `			break;` |
|      - | 4089 | `		}` |
|      - | 4090 | `		/* Output the HTML line break */` |
|      - | 4091 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br />' (space before the slash), otherwise use '<br>' */` |
|      5 | 4092 | `		if( is_xhtml ){` |
|      3 | 4093 | `			ph7_result_string(pCtx,"<br />",(int)sizeof("<br />")-1);` |
|      2 | 4094 | `		}else{` |
|      3 | 4095 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 4096 | `		}` |
|      5 | 4097 | `		zCur = zIn;` |
|      - | 4098 | `		/* Append trailing line */` |
|     11 | 4099 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 4100 | `			zIn++;` |
|      1 | 4101 | `		}` |
|      5 | 4102 | `		if( zCur < zIn ){` |
|      - | 4103 | `			/* Output chunk verbatim */` |
|      5 | 4104 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 4105 | `		}` |
|      1 | 4106 | `	}` |
|      5 | 4107 | `	return PH7_OK;` |
|      3 | 4108 | `}` |
|      - | 4109 | `/*` |
|      - | 4110 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 4111 | ` *  According to the PHP reference manual.` |
|      - | 4112 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 4113 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 4114 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 4115 | ` * This applies to both sprintf() and printf().` |
|      - | 4116 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 4117 | ` * or more of these elements, in order:` |
|      - | 4118 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 4119 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 4120 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 4121 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 4122 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 4123 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 4124 | ` *   it with a single quote ('). See the examples below.` |
|      - | 4125 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 4126 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 4127 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 4128 | ` *   should result in.` |
|      - | 4129 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 4130 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 4131 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 4132 | ` *   limit to the string.` |
|      - | 4133 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 4134 | ` *       % - a literal percent character. No argument is required.` |
|      - | 4135 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 4136 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 4137 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 4138 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 4139 | ` * 	     for the number of digits after the decimal point.` |
|      - | 4140 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 4141 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 4142 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 4143 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 4144 | ` *       g - shorter of %e and %f.` |
|      - | 4145 | ` *       G - shorter of %E and %f.` |
|      - | 4146 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 4147 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 4148 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 4149 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 4150 | ` */` |
|      - | 4151 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4152 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 4153 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 4154 | `/*` |
|      - | 4155 | `** Conversion types fall into various categories as defined by the` |
|      - | 4156 | `** following enumeration.` |
|      - | 4157 | `*/` |
|      - | 4158 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 4159 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 4160 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 4161 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 4162 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 4163 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 4164 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 4165 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 4166 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 4167 |  |
|      - | 4168 | `/*` |
|      - | 4169 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 4170 | `*/` |
|      - | 4171 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 4172 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 4173 | `/*` |
|      - | 4174 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 4175 | `** by an instance of the following structure` |
|      - | 4176 | `*/` |
|      - | 4177 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 4178 | `struct ph7_fmt_info` |
|      - | 4179 | `{` |
|      - | 4180 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 4181 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 4182 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 4183 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 4184 | `  char *charset; /* The character set for conversion */` |
|      - | 4185 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 4186 | `};` |
|      - | 4187 | `/* PH7_PhpFloatShape (php's float-shape post-processing) lives in memobj.c —` |
|      - | 4188 | ` * the default float->string cast needs it even when this whole formatting` |
|      - | 4189 | ` * region is compiled out by PH7_DISABLE_DISK_IO. */` |
|      - | 4190 | `/*` |
|      - | 4191 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 4192 | ` * used conversion types first.` |
|      - | 4193 | ` */` |
|      - | 4194 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 4195 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 4196 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 4197 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 4198 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 4199 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 4200 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 4201 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 4202 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 4203 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4204 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4205 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 4206 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 4207 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 4208 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 4209 | `  /* php's 'h'/'H' are the locale-independent twins of 'g'/'G'; PHL always` |
|      - | 4210 | `   * formats in the C locale, so they behave identically. */` |
|      - | 4211 | `  {  'h',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 4212 | `  {  'H',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 4213 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 4214 | `};` |
|      - | 4215 | `/*` |
|      - | 4216 | ` * PHP 8 raises a catchable ValueError for an unknown conversion specifier` |
|      - | 4217 | ` * (e.g. "%y", or the C-ism "%#x" — '#' is not a php flag). Because printf()` |
|      - | 4218 | ` * and fprintf() stream their output incrementally while sprintf() buffers it,` |
|      - | 4219 | ` * every format builtin calls PH7_FormatValidate (below) to check the whole` |
|      - | 4220 | ` * format string BEFORE formatting so the throw happens with no partial output` |
|      - | 4221 | ` * escaping (php buffers the entire result and only emits it on success). This` |
|      - | 4222 | ` * scan mirrors the specifier-locating logic of the main format loop below.` |
|      - | 4223 | ` * On the first unknown specifier, stores it in *pBad and returns TRUE; returns` |
|      - | 4224 | ` * FALSE when every specifier is known. (A found-flag rather than a sentinel` |
|      - | 4225 | ` * char, so a NUL specifier byte — "%\0" — is still reported, not mistaken for` |
|      - | 4226 | ` * "all valid".)` |
|      - | 4227 | ` */` |
|    416 | 4228 | `static int FormatUnknownSpec(const char *zIn,int nByte,int *pBad)` |
|      1 | 4229 | `{` |
|    417 | 4230 | `	const char *zEnd = &zIn[nByte];` |
|      - | 4231 | `	int c,idx;` |
|   3585 | 4232 | `	while( zIn < zEnd ){` |
|   3189 | 4233 | `		if( zIn[0] != '%' ){` |
|   2369 | 4234 | `			zIn++;` |
|   2369 | 4235 | `			continue;` |
|      - | 4236 | `		}` |
|    821 | 4237 | `		zIn++; /* jump the percent sign */` |
|      - | 4238 | `		/* php-supported flags: '-', '+', ' ', '0' and the "'<pad>'" custom-pad` |
|      - | 4239 | `		 * form. '#' is intentionally NOT treated as a flag so it surfaces as an` |
|      - | 4240 | `		 * unknown specifier, matching php. */` |
|   1005 | 4241 | `		while( zIn < zEnd ){` |
|   1003 | 4242 | `			c = zIn[0];` |
|   1003 | 4243 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|    185 | 4244 | `				zIn++;` |
|    185 | 4245 | `				continue;` |
|      - | 4246 | `			}` |
|    819 | 4247 | `			if( c=='\'' ){` |
|    ! 0 | 4248 | `				zIn++;` |
|    ! 0 | 4249 | `				if( zIn < zEnd ){` |
|    ! 0 | 4250 | `					zIn++; /* the custom pad character */` |
|    ! 0 | 4251 | `				}` |
|    ! 0 | 4252 | `				continue;` |
|      - | 4253 | `			}` |
|    819 | 4254 | `			break;` |
|    ! 0 | 4255 | `		}` |
|      - | 4256 | `		/* field width */` |
|   1037 | 4257 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    217 | 4258 | `			zIn++;` |
|      1 | 4259 | `		}` |
|      - | 4260 | `		/* positional specifier ($) — php parses flags AFTER it (e.g. "%1$-10s"),` |
|      - | 4261 | `		 * so skip the full flag set and width again, mirroring the main loop. */` |
|    821 | 4262 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
|    ! 0 | 4263 | `			zIn++;` |
|    ! 0 | 4264 | `			while( zIn < zEnd ){` |
|    ! 0 | 4265 | `				c = zIn[0];` |
|    ! 0 | 4266 | `				if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|    ! 0 | 4267 | `					zIn++;` |
|    ! 0 | 4268 | `					continue;` |
|      - | 4269 | `				}` |
|    ! 0 | 4270 | `				if( c=='\'' ){` |
|    ! 0 | 4271 | `					zIn++;` |
|    ! 0 | 4272 | `					if( zIn < zEnd ){` |
|    ! 0 | 4273 | `						zIn++;` |
|    ! 0 | 4274 | `					}` |
|    ! 0 | 4275 | `					continue;` |
|      - | 4276 | `				}` |
|    ! 0 | 4277 | `				break;` |
|    ! 0 | 4278 | `			}` |
|    ! 0 | 4279 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    ! 0 | 4280 | `				zIn++;` |
|    ! 0 | 4281 | `			}` |
|    ! 0 | 4282 | `		}` |
|      - | 4283 | `		/* precision */` |
|    821 | 4284 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|     87 | 4285 | `			zIn++;` |
|    183 | 4286 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|     97 | 4287 | `				zIn++;` |
|      1 | 4288 | `			}` |
|     43 | 4289 | `		}` |
|      - | 4290 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|    821 | 4291 | `		if( zIn < zEnd && zIn[0]=='l' ){` |
|     11 | 4292 | `			zIn++;` |
|      5 | 4293 | `		}` |
|    821 | 4294 | `		if( zIn >= zEnd ){` |
|      - | 4295 | `			/* A dangling '%' with no specifier: PHL's legacy path silently` |
|      - | 4296 | `			 * truncates here (recorded residual); nothing to validate. */` |
|      3 | 4297 | `			break;` |
|      - | 4298 | `		}` |
|    819 | 4299 | `		c = zIn[0];` |
|    819 | 4300 | `		zIn++; /* jump the conversion specifier */` |
|   3377 | 4301 | `		for( idx = 0 ; idx < (int)SX_ARRAYSIZE(aFmt) ; idx++ ){` |
|   3359 | 4302 | `			if( c == aFmt[idx].fmttype ){` |
|    801 | 4303 | `				break;` |
|      - | 4304 | `			}` |
|   1280 | 4305 | `		}` |
|    819 | 4306 | `		if( idx >= (int)SX_ARRAYSIZE(aFmt) ){` |
|     19 | 4307 | `			*pBad = c; /* unknown specifier */` |
|     19 | 4308 | `			return TRUE;` |
|      - | 4309 | `		}` |
|      1 | 4310 | `	}` |
|    399 | 4311 | `	return FALSE;` |
|    209 | 4312 | `}` |
|      - | 4313 | `/*` |
|      - | 4314 | ` * Validate a printf-style format string. PHP 8 raises a catchable ValueError for` |
|      - | 4315 | ` * an unknown conversion specifier, thrown before any output is produced. Every` |
|      - | 4316 | ` * format builtin (sprintf/printf/vprintf/vsprintf/fprintf/vfprintf) calls this` |
|      - | 4317 | ` * up-front, then propagates the returned status verbatim (PH7_EXCEPTION when the` |
|      - | 4318 | ` * throw is caught in place, PH7_ABORT when it goes uncaught).` |
|      - | 4319 | ` * Returns PH7_OK when the format is valid.` |
|      - | 4320 | ` */` |
|    416 | 4321 | `PH7_PRIVATE sxi32 PH7_FormatValidate(ph7_context *pCtx,const char *zFormat,int nByte)` |
|      1 | 4322 | `{` |
|    417 | 4323 | `	int badSpec = 0;` |
|    417 | 4324 | `	if( FormatUnknownSpec(zFormat,nByte,&badSpec) ){` |
|     28 | 4325 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      9 | 4326 | `			"Unknown format specifier \"%c\"",badSpec);` |
|      - | 4327 | `	}` |
|    399 | 4328 | `	return PH7_OK;` |
|    209 | 4329 | `}` |
|      - | 4330 | `/*` |
|      - | 4331 | `` * PHP 8: a printf-family `$format` argument is a `string` parameter — scalars`` |
|      - | 4332 | ` * (int/float/bool) and null coerce to a string, but an array/object/resource` |
|      - | 4333 | ` * raises a catchable TypeError. iArg is the 1-based argument position ($format` |
|      - | 4334 | ` * is #1 for sprintf/printf/vprintf/vsprintf, #2 for fprintf/vfprintf). Returns` |
|      - | 4335 | ` * PH7_OK when the value is string-coercible (the caller then uses` |
|      - | 4336 | ` * ph7_value_to_string, which renders scalars/null verbatim).` |
|      - | 4337 | ` */` |
|      - | 4338 | `/*` |
|      - | 4339 | ` * php 8: a stream parameter that is not a resource is a TypeError, not a warning` |
|      - | 4340 | ` * with a 0 return -- the caller never learned its write went nowhere.` |
|      - | 4341 | ` */` |
|     20 | 4342 | `PH7_PRIVATE sxi32 PH7_CheckStreamArg(ph7_context *pCtx,ph7_value *pArg,int iArg,const char *zName)` |
|      1 | 4343 | `{` |
|     21 | 4344 | `	if( !ph7_value_is_resource(pArg) ){` |
|      - | 4345 | `		char zBuf[64];` |
|      4 | 4346 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4347 | `			"%s(): Argument #%d ($%s) must be of type resource, %s given",` |
|      1 | 4348 | `			ph7_function_name(pCtx),iArg,zName,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|      - | 4349 | `	}` |
|     19 | 4350 | `	return PH7_OK;` |
|     11 | 4351 | `}` |
|    428 | 4352 | `PH7_PRIVATE sxi32 PH7_FormatCheckFormatArg(ph7_context *pCtx,ph7_value *pArg,int iArg)` |
|      1 | 4353 | `{` |
|    429 | 4354 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg) \|\| ph7_value_is_resource(pArg) ){` |
|      - | 4355 | `		char zBuf[64];` |
|    ! 0 | 4356 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4357 | `			"%s(): Argument #%d ($format) must be of type string, %s given",` |
|    ! 0 | 4358 | `			ph7_function_name(pCtx),iArg,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|      - | 4359 | `	}` |
|    429 | 4360 | `	return PH7_OK;` |
|    215 | 4361 | `}` |
|      - | 4362 | `/*` |
|      - | 4363 | ` * Format a given string.` |
|      - | 4364 | ` * The root program.  All variations call this core.` |
|      - | 4365 | ` * INPUTS:` |
|      - | 4366 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 4367 | ` *            1. A pointer to the call context.` |
|      - | 4368 | ` *            2. A pointer to the list of characters to be output` |
|      - | 4369 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 4370 | ` *            3. An integer number of characters to be output.` |
|      - | 4371 | ` *               (Note: This number might be zero.)` |
|      - | 4372 | ` *            4. Upper layer private data.` |
|      - | 4373 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 4374 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 4375 | ` */` |
|    398 | 4376 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 4377 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 4378 | `	ph7_context *pCtx,  /* call context */` |
|      - | 4379 | `	const char *zIn,    /* Format string */` |
|      - | 4380 | `	int nByte,          /* Format string length */` |
|      - | 4381 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 4382 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 4383 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 4384 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 4385 | `	)` |
|      1 | 4386 | `{` |
|    399 | 4387 | `	char spaces[] = "                                                  ";` |
|      - | 4388 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    399 | 4389 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 4390 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 4391 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 4392 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 4393 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 4394 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 4395 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 4396 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 4397 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 4398 | `	ph7_int64 iVal;` |
|      - | 4399 | `	int precision;           /* Precision of the current field */` |
|      - | 4400 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 4401 | `	int c,rc,n;` |
|      - | 4402 | `	int length;              /* Length of the field */` |
|      - | 4403 | `	int prefix;` |
|      - | 4404 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 4405 | `	int width;               /* Width of the current field */` |
|      - | 4406 | `	int idx;` |
|    399 | 4407 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 4408 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 4409 | `	/* An unknown conversion specifier is rejected up-front by PH7_FormatValidate()` |
|      - | 4410 | `	 * (called by every format builtin before this routine), so the specifier set` |
|      - | 4411 | `	 * seen here is always valid. */` |
|      - | 4412 | `	/* Start the format process */` |
|    599 | 4413 | `	for(;;){` |
|   1199 | 4414 | `		zCur = zIn;` |
|   3553 | 4415 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|   2355 | 4416 | `			zIn++;` |
|      1 | 4417 | `		}` |
|   1199 | 4418 | `		if( zCur < zIn ){` |
|      - | 4419 | `			/* Consume chunk verbatim */` |
|    749 | 4420 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|    749 | 4421 | `			if( rc != SXRET_OK ){` |
|      - | 4422 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|    ! 0 | 4423 | `				break;` |
|      - | 4424 | `			}` |
|    374 | 4425 | `		}` |
|   1199 | 4426 | `		if( zIn >= zEnd ){` |
|      - | 4427 | `			/* No more input to process,break immediately */` |
|    397 | 4428 | `			break;` |
|      - | 4429 | `		}` |
|      - | 4430 | `		/* Find out what flags are present */` |
|    803 | 4431 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    802 | 4432 | `			flag_alternateform = flag_zeropad = 0;` |
|    803 | 4433 | `		zIn++; /* Jump the precent sign */` |
|    401 | 4434 | `		do{` |
|    987 | 4435 | `			c = zIn[0];` |
|    987 | 4436 | `			switch( c ){` |
|     15 | 4437 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      7 | 4438 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 4439 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|    159 | 4440 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 4441 | `			case '\'':` |
|    ! 0 | 4442 | `				zIn++;` |
|    ! 0 | 4443 | `				if( zIn < zEnd ){` |
|      - | 4444 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 4445 | `					c = zIn[0];` |
|    ! 0 | 4446 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4447 | `						spaces[idx] = (char)c;` |
|    ! 0 | 4448 | `					}` |
|    ! 0 | 4449 | `					c = 0;` |
|    ! 0 | 4450 | `				}` |
|    ! 0 | 4451 | `				break;` |
|    802 | 4452 | `			default:                                       break;` |
|      - | 4453 | `			}` |
|    987 | 4454 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 4455 | `		/* Get the field width */` |
|    803 | 4456 | `		width = 0;` |
|   1420 | 4457 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    217 | 4458 | `			width = width*10 + (zIn[0] - '0');` |
|    217 | 4459 | `			zIn++;` |
|      1 | 4460 | `		}` |
|    803 | 4461 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 4462 | `			/* Position specifer */` |
|    ! 0 | 4463 | `			if( width > 0 ){` |
|    ! 0 | 4464 | `				n = width;` |
|    ! 0 | 4465 | `				if( vf && n > 0 ){` |
|    ! 0 | 4466 | `					n--;` |
|    ! 0 | 4467 | `				}` |
|    ! 0 | 4468 | `			}` |
|    ! 0 | 4469 | `			zIn++;` |
|    ! 0 | 4470 | `			width = 0;` |
|      - | 4471 | `			/* php's grammar is %argnum$<flags><width>: the flags come AFTER the` |
|      - | 4472 | `			 * positional, so re-parse the full flag set here (e.g. "%1$-10s"),` |
|      - | 4473 | `			 * not just zero-padding. */` |
|    ! 0 | 4474 | `			do{` |
|    ! 0 | 4475 | `				c = zIn[0];` |
|    ! 0 | 4476 | `				switch( c ){` |
|    ! 0 | 4477 | `				case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|    ! 0 | 4478 | `				case '+':   flag_plussign = 1;        c = 0;   break;` |
|    ! 0 | 4479 | `				case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|    ! 0 | 4480 | `				case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 4481 | `				case '\'':` |
|    ! 0 | 4482 | `					zIn++;` |
|    ! 0 | 4483 | `					if( zIn < zEnd ){` |
|    ! 0 | 4484 | `						c = zIn[0];` |
|    ! 0 | 4485 | `						for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4486 | `							spaces[idx] = (char)c;` |
|    ! 0 | 4487 | `						}` |
|    ! 0 | 4488 | `						c = 0;` |
|    ! 0 | 4489 | `					}` |
|    ! 0 | 4490 | `					break;` |
|    ! 0 | 4491 | `				default:                                       break;` |
|      - | 4492 | `				}` |
|    ! 0 | 4493 | `			}while( c==0 && (zIn++ < zEnd) );` |
|    ! 0 | 4494 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 4495 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 4496 | `				zIn++;` |
|    ! 0 | 4497 | `			}` |
|    ! 0 | 4498 | `		}` |
|    803 | 4499 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 4500 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 4501 | `		}` |
|      - | 4502 | `		/* Get the precision */` |
|    803 | 4503 | `		precision = -1;` |
|    803 | 4504 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     87 | 4505 | `			precision = 0;` |
|     87 | 4506 | `			zIn++;` |
|    226 | 4507 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     97 | 4508 | `				precision = precision*10 + (zIn[0] - '0');` |
|     97 | 4509 | `				zIn++;` |
|      1 | 4510 | `			}` |
|     43 | 4511 | `		}` |
|      - | 4512 | `		/* Consume a single 'l' length modifier (a C-ism php accepts and ignores,` |
|      - | 4513 | `		 * e.g. "%ld"); PH7_FormatValidate mirrors this. Exactly one is skipped:` |
|      - | 4514 | `		 * in "%lld" the second 'l' becomes the (unknown) specifier, just like php. */` |
|    803 | 4515 | `		if( zIn < zEnd && zIn[0] == 'l' ){` |
|      9 | 4516 | `			zIn++;` |
|      4 | 4517 | `		}` |
|    803 | 4518 | `		if( zIn >= zEnd ){` |
|      - | 4519 | `			/* No more input */` |
|      3 | 4520 | `			break;` |
|      - | 4521 | `		}` |
|      - | 4522 | `		/* Fetch the info entry for the field */` |
|    801 | 4523 | `		pInfo = 0;` |
|    801 | 4524 | `		xtype = PH7_FMT_ERROR;` |
|    801 | 4525 | `		c = zIn[0];` |
|    801 | 4526 | `		zIn++; /* Jump the format specifer */` |
|   3053 | 4527 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|   3053 | 4528 | `			if( c==aFmt[idx].fmttype ){` |
|    801 | 4529 | `				pInfo = &aFmt[idx];` |
|    801 | 4530 | `				xtype = pInfo->type;` |
|    801 | 4531 | `				break;` |
|      - | 4532 | `			}` |
|   1127 | 4533 | `		}` |
|    801 | 4534 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    801 | 4535 | `		length = 0;` |
|      - | 4536 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 4537 | `		 /*` |
|      - | 4538 | `		  ** At this point, variables are initialized as follows:` |
|      - | 4539 | `		  **` |
|      - | 4540 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 4541 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 4542 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 4543 | `		  **                               field width was negative.` |
|      - | 4544 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 4545 | `		  **                               the conversion character.` |
|      - | 4546 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 4547 | `		  **   width                       The specified field width.  This is` |
|      - | 4548 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 4549 | `		  **   precision                   The specified precision.  The default` |
|      - | 4550 | `		  **                               is -1.` |
|      - | 4551 | `		  */` |
|    801 | 4552 | `		switch(xtype){` |
|      3 | 4553 | `		case PH7_FMT_PERCENT:` |
|      - | 4554 | `			/* A literal percent character */` |
|      7 | 4555 | `			zWorker[0] = '%';` |
|      7 | 4556 | `			length = (int)sizeof(char);` |
|      7 | 4557 | `			break;` |
|      3 | 4558 | `		case PH7_FMT_CHARX:` |
|      - | 4559 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 4560 | `			 * with that ASCII value` |
|      - | 4561 | `			 */` |
|      7 | 4562 | `			pArg = NEXT_ARG;` |
|      7 | 4563 | `			if( pArg == 0 ){` |
|      3 | 4564 | `				c = 0;` |
|      2 | 4565 | `			}else{` |
|      5 | 4566 | `				c = ph7_value_to_int(pArg);` |
|      - | 4567 | `			}` |
|      - | 4568 | `			/* NUL byte is an acceptable value */` |
|      7 | 4569 | `			zWorker[0] = (char)c;` |
|      7 | 4570 | `			length = (int)sizeof(char);` |
|      7 | 4571 | `			break;` |
|    170 | 4572 | `		case PH7_FMT_STRING:` |
|      - | 4573 | `			/* the argument is treated as and presented as a string */` |
|    341 | 4574 | `			pArg = NEXT_ARG;` |
|    341 | 4575 | `			if( pArg == 0 ){` |
|    ! 0 | 4576 | `				length = 0;` |
|    ! 0 | 4577 | `			}else{` |
|    341 | 4578 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 4579 | `			}` |
|    341 | 4580 | `			if( length < 1 ){` |
|      - | 4581 | `				/* An empty %s substitutes NOTHING in php. PH7 substituted a single` |
|      - | 4582 | `				 * SPACE here, so printf("[%s]","") printed "[ ]" and any format with an` |
|      - | 4583 | `				 * absent optional part gained a stray space. */` |
|      9 | 4584 | `				zBuf = "";` |
|      9 | 4585 | `				length = 0;` |
|      4 | 4586 | `			}` |
|    341 | 4587 | `			if( precision>=0 && precision<length ){` |
|      3 | 4588 | `				length = precision;` |
|      1 | 4589 | `			}` |
|    341 | 4590 | `			if( flag_zeropad ){` |
|      - | 4591 | `				/* zero-padding works on strings too */` |
|    ! 0 | 4592 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4593 | `					spaces[idx] = '0';` |
|    ! 0 | 4594 | `				}` |
|    ! 0 | 4595 | `			}` |
|    341 | 4596 | `			break;` |
|    136 | 4597 | `		case PH7_FMT_RADIX:` |
|    273 | 4598 | `			pArg = NEXT_ARG;` |
|    273 | 4599 | `			if( pArg == 0 ){` |
|    ! 0 | 4600 | `				iVal = 0;` |
|    ! 0 | 4601 | `			}else{` |
|    273 | 4602 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 4603 | `			}` |
|      - | 4604 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|    273 | 4605 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 4606 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 4607 | `			}` |
|      - | 4608 | `#if 1` |
|      - | 4609 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 4610 | `        ** I think this is stupid.*/` |
|    273 | 4611 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 4612 | `#else` |
|      - | 4613 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 4614 | `        ** but leave the prefix for hex.*/` |
|      - | 4615 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 4616 | `#endif` |
|    273 | 4617 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|    249 | 4618 | `          if( iVal<0 ){` |
|     25 | 4619 | `            iVal = -iVal;` |
|      - | 4620 | `			/* Ticket 1433-003 */` |
|     25 | 4621 | `			if( iVal < 0 ){` |
|      - | 4622 | `				/* Overflow */` |
|    ! 0 | 4623 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4624 | `			}` |
|     25 | 4625 | `            prefix = '-';` |
|    237 | 4626 | `          }else if( flag_plussign )  prefix = '+';` |
|    223 | 4627 | `          else if( flag_blanksign )  prefix = ' ';` |
|    221 | 4628 | `          else                       prefix = 0;` |
|    125 | 4629 | `        }else{` |
|     25 | 4630 | `			if( iVal<0 ){` |
|    ! 0 | 4631 | `				iVal = -iVal;` |
|      - | 4632 | `				/* Ticket 1433-003 */` |
|    ! 0 | 4633 | `				if( iVal < 0 ){` |
|      - | 4634 | `					/* Overflow */` |
|    ! 0 | 4635 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4636 | `				}` |
|    ! 0 | 4637 | `			}` |
|     25 | 4638 | `			prefix = 0;` |
|      - | 4639 | `		}` |
|    273 | 4640 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|    149 | 4641 | `          precision = width-(prefix!=0);` |
|     74 | 4642 | `        }` |
|    273 | 4643 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 4644 | `        {` |
|      - | 4645 | `          register char *cset;      /* Use registers for speed */` |
|      - | 4646 | `          register int base;` |
|    273 | 4647 | `          cset = pInfo->charset;` |
|    273 | 4648 | `          base = pInfo->base;` |
|    136 | 4649 | `          do{                                           /* Convert to ascii */` |
|    349 | 4650 | `            *(--zBuf) = cset[iVal%base];` |
|    349 | 4651 | `            iVal = iVal/base;` |
|    349 | 4652 | `          }while( iVal>0 );` |
|      - | 4653 | `        }` |
|    273 | 4654 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    439 | 4655 | `        for(idx=precision-length; idx>0; idx--){` |
|    167 | 4656 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|     84 | 4657 | `        }` |
|    273 | 4658 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|    273 | 4659 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 4660 | `          char *pre, x;` |
|    ! 0 | 4661 | `          pre = pInfo->prefix;` |
|    ! 0 | 4662 | `          if( *zBuf!=pre[0] ){` |
|    ! 0 | 4663 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|    ! 0 | 4664 | `          }` |
|    ! 0 | 4665 | `        }` |
|    273 | 4666 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    273 | 4667 | `		break;` |
|     88 | 4668 | `		case PH7_FMT_FLOAT:` |
|      - | 4669 | `		case PH7_FMT_EXP:` |
|      - | 4670 | `		case PH7_FMT_GENERIC:{` |
|      - | 4671 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 4672 | `		double realvalue;` |
|      - | 4673 | `		char zFmt[8];` |
|      - | 4674 | `		int nOut, nFmt;` |
|    177 | 4675 | `		pArg = NEXT_ARG;` |
|    177 | 4676 | `		if( pArg == 0 ){` |
|    ! 0 | 4677 | `			realvalue = 0;` |
|    ! 0 | 4678 | `		}else{` |
|    177 | 4679 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 4680 | `		}` |
|      - | 4681 | `		/* php prints the IEEE specials bare — NaN / INF / -INF with no width` |
|      - | 4682 | `		 * padding, precision, or sign flags (php_sprintf_appenddouble). */` |
|    177 | 4683 | `		if( PH7_IS_NAN(realvalue) ){` |
|     21 | 4684 | `			zBuf = "NaN";` |
|     21 | 4685 | `			length = 3;` |
|     21 | 4686 | `			width = 0;` |
|     21 | 4687 | `			break;` |
|      - | 4688 | `		}` |
|    157 | 4689 | `		if( PH7_IS_INF(realvalue) ){` |
|     37 | 4690 | `			if( realvalue < 0.0 ){` |
|     15 | 4691 | `				zBuf = "-INF";` |
|     15 | 4692 | `				length = 4;` |
|      8 | 4693 | `			}else{` |
|     23 | 4694 | `				zBuf = "INF";` |
|     23 | 4695 | `				length = 3;` |
|      - | 4696 | `			}` |
|     37 | 4697 | `			width = 0;` |
|     37 | 4698 | `			break;` |
|      - | 4699 | `		}` |
|    121 | 4700 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|    121 | 4701 | `		if( precision > 53 ){` |
|      - | 4702 | `			/* php's FORMAT_CONV_MAX_PRECISION cap, with the same E_NOTICE` |
|      - | 4703 | `			 * (message prefixed with the active function's name, like` |
|      - | 4704 | `			 * php_error_docref). */` |
|      - | 4705 | `			char zMsg[160];` |
|      4 | 4706 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 4707 | `				"%z(): Requested precision of %d digits was truncated to PHP maximum of %d digits",` |
|      2 | 4708 | `				&pCtx->pFunc->sName,precision,53);` |
|      3 | 4709 | `			PH7_VmThrowError(pCtx->pVm,0,E_NOTICE,zMsg);` |
|      3 | 4710 | `			precision = 53;` |
|      1 | 4711 | `		}` |
|      - | 4712 | ``		/* php's %f/%e extract the sign via `num < 0`, so negative zero prints`` |
|      - | 4713 | `		 * unsigned there — while %g (php_gcvt on the raw value) keeps "-0". */` |
|    121 | 4714 | `		if( xtype!=PH7_FMT_GENERIC && realvalue == 0.0 ){` |
|      9 | 4715 | `			realvalue = 0.0;` |
|      4 | 4716 | `		}` |
|      - | 4717 | `		/* php's float conversions are correctly rounded (zend_dtoa); use libc` |
|      - | 4718 | `		 * snprintf as the digit engine (the byte-exact-floats rule — the old` |
|      - | 4719 | `		 * hand-rolled vxGetdigit loop stopped at 16 significant digits, so` |
|      - | 4720 | `		 * e.g. %f of 1e308 printed zeros where php prints the exact binary64` |
|      - | 4721 | `		 * expansion), then post-process into php's exact shapes below. */` |
|    121 | 4722 | `		nFmt = 0;` |
|    121 | 4723 | `		zFmt[nFmt++] = '%';` |
|    121 | 4724 | `		if( flag_alternateform ) zFmt[nFmt++] = '#';` |
|      - | 4725 | `		/* php's ' ' flag selects space PADDING (its default), not C's` |
|      - | 4726 | `		 * space-for-positive-sign — so flag_blanksign is NOT forwarded. */` |
|    121 | 4727 | `		if( flag_plussign ) zFmt[nFmt++] = '+';` |
|    121 | 4728 | `		zFmt[nFmt++] = '.';` |
|    121 | 4729 | `		zFmt[nFmt++] = '*';` |
|    165 | 4730 | `		zFmt[nFmt++] = (char)(xtype==PH7_FMT_FLOAT ? 'f' :` |
|     30 | 4731 | `			(xtype==PH7_FMT_EXP ? ((pInfo->charset[0]=='E') ? 'E' : 'e')` |
|     28 | 4732 | `			                    : ((pInfo->charset[0]=='E') ? 'G' : 'g')));` |
|    121 | 4733 | `		zFmt[nFmt] = 0;` |
|    121 | 4734 | `		nOut = snprintf(zWorker,sizeof(zWorker),zFmt,precision,realvalue);` |
|    121 | 4735 | `		if( nOut < 0 \|\| nOut >= (int)sizeof(zWorker) ){` |
|      - | 4736 | `			/* Cannot happen with precision capped at 53 (%f of DBL_MAX is` |
|      - | 4737 | `			 * ~365 bytes); keep the truncated output rather than overrun. */` |
|    ! 0 | 4738 | `			nOut = (int)SyStrlen(zWorker);` |
|    ! 0 | 4739 | `		}` |
|    121 | 4740 | `		nOut = (int)PH7_PhpFloatShape(zWorker,(sxi32)nOut,xtype==PH7_FMT_GENERIC);` |
|    121 | 4741 | `		zBuf = zWorker;` |
|    121 | 4742 | `		length = nOut;` |
|      - | 4743 | `		/* Let the zero-pad block below insert zeros between the sign (written` |
|      - | 4744 | `		 * by snprintf) and the first digit, as before. */` |
|    121 | 4745 | `		prefix = (zWorker[0]=='-' \|\| zWorker[0]=='+' \|\| zWorker[0]==' ') ? zWorker[0] : 0;` |
|      - | 4746 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 4747 | `        ** set and we are not left justified */` |
|    121 | 4748 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 4749 | `          int i;` |
|      7 | 4750 | `          int nPad = width - length;` |
|     51 | 4751 | `          for(i=width; i>=nPad; i--){` |
|     45 | 4752 | `            zBuf[i] = zBuf[i-nPad];` |
|     23 | 4753 | `          }` |
|      7 | 4754 | `          i = prefix!=0;` |
|     29 | 4755 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      7 | 4756 | `          length = width;` |
|      3 | 4757 | `        }` |
|      - | 4758 | `#else` |
|      - | 4759 | `         zBuf = " ";` |
|      - | 4760 | `		 length = (int)sizeof(char);` |
|      - | 4761 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|    121 | 4762 | `		 break;` |
|      - | 4763 | `							 }` |
|    ! 0 | 4764 | `		default:` |
|      - | 4765 | `			/* Unreachable: PH7_FormatValidate() rejects unknown specifiers with a` |
|      - | 4766 | `			 * catchable ValueError before formatting begins. Kept as a defensive` |
|      - | 4767 | `			 * no-op that emits nothing. */` |
|    ! 0 | 4768 | `			length = 0;` |
|    ! 0 | 4769 | `			break;` |
|      - | 4770 | `		}` |
|      - | 4771 | `		 /*` |
|      - | 4772 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 4773 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 4774 | `		 ** the output.` |
|      - | 4775 | `		 */` |
|    801 | 4776 | `    if( !flag_leftjustify ){` |
|      - | 4777 | `      register int nspace;` |
|    787 | 4778 | `      nspace = width-length;` |
|    787 | 4779 | `      if( nspace>0 ){` |
|      7 | 4780 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4781 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4782 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4783 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4784 | `			}` |
|    ! 0 | 4785 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4786 | `        }` |
|      7 | 4787 | `        if( nspace>0 ){` |
|      7 | 4788 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      7 | 4789 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4790 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4791 | `			}` |
|      3 | 4792 | `		}` |
|      3 | 4793 | `      }` |
|    393 | 4794 | `    }` |
|    801 | 4795 | `    if( length>0 ){` |
|    793 | 4796 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    793 | 4797 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4798 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4799 | `		}` |
|    396 | 4800 | `    }` |
|    801 | 4801 | `    if( flag_leftjustify ){` |
|      - | 4802 | `      register int nspace;` |
|     15 | 4803 | `      nspace = width-length;` |
|     15 | 4804 | `      if( nspace>0 ){` |
|     11 | 4805 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4806 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4807 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4808 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4809 | `			}` |
|    ! 0 | 4810 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4811 | `        }` |
|     11 | 4812 | `        if( nspace>0 ){` |
|     11 | 4813 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|     11 | 4814 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4815 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4816 | `			}` |
|      5 | 4817 | `		}` |
|      5 | 4818 | `      }` |
|      7 | 4819 | `    }` |
|      1 | 4820 | ` }/* for(;;) */` |
|    399 | 4821 | `	return SXRET_OK;` |
|    200 | 4822 | `}` |
|      - | 4823 | `/*` |
|      - | 4824 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 4825 | ` */` |
|    352 | 4826 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4827 | `{` |
|      - | 4828 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|      - | 4829 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|      - | 4830 | `	 * non-OK rc also stops the format loop. */` |
|    353 | 4831 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|    353 | 4832 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|    353 | 4833 | `	return *pRc;` |
|      1 | 4834 | `}` |
|      - | 4835 | `/*` |
|      - | 4836 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 4837 | ` *  Return a formatted string.` |
|      - | 4838 | ` * Parameters` |
|      - | 4839 | ` *  $format` |
|      - | 4840 | ` *    The format string (see block comment above)` |
|      - | 4841 | ` * Return` |
|      - | 4842 | ` *  A string produced according to the formatting string format.` |
|      - | 4843 | ` */` |
|    184 | 4844 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4845 | `{` |
|      - | 4846 | `	const char *zFormat;` |
|    185 | 4847 | `	sxi32 rc = SXRET_OK;` |
|      - | 4848 | `	int nLen;` |
|    185 | 4849 | `	if( nArg < 1 ){` |
|      - | 4850 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4851 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4852 | `		return PH7_OK;` |
|      - | 4853 | `	}` |
|      - | 4854 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|    185 | 4855 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    185 | 4856 | `	if( rc != PH7_OK ){` |
|    ! 0 | 4857 | `		return rc;` |
|      - | 4858 | `	}` |
|      - | 4859 | `	/* Extract the string format (scalars/null coerce). */` |
|    185 | 4860 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    185 | 4861 | `	if( nLen < 1 ){` |
|      - | 4862 | `		/* Empty string */` |
|    ! 0 | 4863 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4864 | `		return PH7_OK;` |
|      - | 4865 | `	}` |
|      - | 4866 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4867 | `	 * output; propagate the throw status verbatim. */` |
|    185 | 4868 | `	rc = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    185 | 4869 | `	if( rc != PH7_OK ){` |
|     17 | 4870 | `		return rc;` |
|      - | 4871 | `	}` |
|      - | 4872 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|    169 | 4873 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|    169 | 4874 | `	if( rc != SXRET_OK ){` |
|      - | 4875 | `		/* The result append ran out of memory: raise a fatal rather than` |
|      - | 4876 | `		 * returning a silently-truncated string. */` |
|    ! 0 | 4877 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4878 | `	}` |
|    169 | 4879 | `	return PH7_OK;` |
|     93 | 4880 | `}` |
|      - | 4881 | `/*` |
|      - | 4882 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 4883 | ` */` |
|   1174 | 4884 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4885 | `{` |
|   1175 | 4886 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 4887 | `	/* Call the VM output consumer directly */` |
|   1175 | 4888 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 4889 | `	/* Increment counter */` |
|   1175 | 4890 | `	*pCounter += nLen;` |
|   1175 | 4891 | `	return PH7_OK;` |
|      1 | 4892 | `}` |
|      - | 4893 | `/*` |
|      - | 4894 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 4895 | ` *  Output a formatted string.` |
|      - | 4896 | ` * Parameters` |
|      - | 4897 | ` *  $format` |
|      - | 4898 | ` *   See sprintf() for a description of format.` |
|      - | 4899 | ` * Return` |
|      - | 4900 | ` *  The length of the outputted string.` |
|      - | 4901 | ` */` |
|    204 | 4902 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4903 | `{` |
|    205 | 4904 | `	ph7_int64 nCounter = 0;` |
|      - | 4905 | `	const char *zFormat;` |
|      - | 4906 | `	int nLen;` |
|    205 | 4907 | `	if( nArg < 1 ){` |
|      - | 4908 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 4909 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4910 | `		return PH7_OK;` |
|      - | 4911 | `	}` |
|      - | 4912 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|      - | 4913 | `	{` |
|    205 | 4914 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    205 | 4915 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 4916 | `			return rcf;` |
|      - | 4917 | `		}` |
|      - | 4918 | `	}` |
|      - | 4919 | `	/* Extract the string format (scalars/null coerce). */` |
|    205 | 4920 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    205 | 4921 | `	if( nLen < 1 ){` |
|      - | 4922 | `		/* Empty string */` |
|    ! 0 | 4923 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4924 | `		return PH7_OK;` |
|      - | 4925 | `	}` |
|      - | 4926 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4927 | `	 * output; propagate the throw status verbatim. */` |
|      - | 4928 | `	{` |
|    205 | 4929 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    205 | 4930 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 4931 | `			return rcv;` |
|      - | 4932 | `		}` |
|      - | 4933 | `	}` |
|      - | 4934 | `	/* Format the string */` |
|    205 | 4935 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 4936 | `	/* Return the length of the outputted string */` |
|    205 | 4937 | `	ph7_result_int64(pCtx,nCounter);` |
|    205 | 4938 | `	return PH7_OK;` |
|    103 | 4939 | `}` |
|      - | 4940 | `/*` |
|      - | 4941 | ` * int vprintf(string $format,array $args)` |
|      - | 4942 | ` *  Output a formatted string.` |
|      - | 4943 | ` * Parameters` |
|      - | 4944 | ` *  $format` |
|      - | 4945 | ` *   See sprintf() for a description of format.` |
|      - | 4946 | ` * Return` |
|      - | 4947 | ` *  The length of the outputted string.` |
|      - | 4948 | ` */` |
|      4 | 4949 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4950 | `{` |
|      5 | 4951 | `	ph7_int64 nCounter = 0;` |
|      - | 4952 | `	const char *zFormat;` |
|      - | 4953 | `	ph7_hashmap *pMap;` |
|      - | 4954 | `	SySet sArg;` |
|      - | 4955 | `	int nLen,n;` |
|      - | 4956 | `	sxi32 rcFmt;` |
|      5 | 4957 | `	if( nArg < 2 ){` |
|      - | 4958 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 4959 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4960 | `		return PH7_OK;` |
|      - | 4961 | `	}` |
|      - | 4962 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|      5 | 4963 | `	rcFmt = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|      5 | 4964 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4965 | `		return rcFmt;` |
|      - | 4966 | `	}` |
|      5 | 4967 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 4968 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 4969 | `		char zBuf[64];` |
|      4 | 4970 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4971 | `			"vprintf(): Argument #2 ($values) must be of type array, %s given",` |
|      2 | 4972 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 4973 | `	}` |
|      - | 4974 | `	/* Extract the string format (scalars/null coerce). */` |
|      3 | 4975 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4976 | `	if( nLen < 1 ){` |
|      - | 4977 | `		/* Empty string */` |
|    ! 0 | 4978 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4979 | `		return PH7_OK;` |
|      - | 4980 | `	}` |
|      - | 4981 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4982 | `	 * output; propagate the throw status verbatim. */` |
|      3 | 4983 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      3 | 4984 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4985 | `		return rcFmt;` |
|      - | 4986 | `	}` |
|      - | 4987 | `	/* Point to the hashmap */` |
|      3 | 4988 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4989 | `	/* Extract arguments from the hashmap */` |
|      3 | 4990 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4991 | `	/* Format the string */` |
|      3 | 4992 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 4993 | `	/* Release the container */` |
|      3 | 4994 | `	SySetRelease(&sArg);` |
|      - | 4995 | `	/* Return the length of the outputted string */` |
|      3 | 4996 | `	ph7_result_int64(pCtx,nCounter);` |
|      3 | 4997 | `	return PH7_OK;` |
|      3 | 4998 | `}` |
|      - | 4999 | `/*` |
|      - | 5000 | ` * int vsprintf(string $format,array $args)` |
|      - | 5001 | ` *  Output a formatted string.` |
|      - | 5002 | ` * Parameters` |
|      - | 5003 | ` *  $format` |
|      - | 5004 | ` *   See sprintf() for a description of format.` |
|      - | 5005 | ` * Return` |
|      - | 5006 | ` *  A string produced according to the formatting string format.` |
|      - | 5007 | ` */` |
|     18 | 5008 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5009 | `{` |
|      - | 5010 | `	const char *zFormat;` |
|      - | 5011 | `	ph7_hashmap *pMap;` |
|      - | 5012 | `	SySet sArg;` |
|     19 | 5013 | `	sxi32 rc = SXRET_OK;` |
|      - | 5014 | `	sxi32 rcFmt;` |
|      - | 5015 | `	int nLen,n;` |
|     19 | 5016 | `	if( nArg < 2 ){` |
|      - | 5017 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5018 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5019 | `		return PH7_OK;` |
|      - | 5020 | `	}` |
|      - | 5021 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|     19 | 5022 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|     19 | 5023 | `	if( rc != PH7_OK ){` |
|    ! 0 | 5024 | `		return rc;` |
|      - | 5025 | `	}` |
|     19 | 5026 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 5027 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 5028 | `		char zBuf[64];` |
|     16 | 5029 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5030 | `			"vsprintf(): Argument #2 ($values) must be of type array, %s given",` |
|     10 | 5031 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 5032 | `	}` |
|      - | 5033 | `	/* Extract the string format (scalars/null coerce). */` |
|      9 | 5034 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 5035 | `	if( nLen < 1 ){` |
|      - | 5036 | `		/* Empty string */` |
|    ! 0 | 5037 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5038 | `		return PH7_OK;` |
|      - | 5039 | `	}` |
|      - | 5040 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 5041 | `	 * output; propagate the throw status verbatim. */` |
|      9 | 5042 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      9 | 5043 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 5044 | `		return rcFmt;` |
|      - | 5045 | `	}` |
|      - | 5046 | `	/* Point to hashmap */` |
|      9 | 5047 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 5048 | `	/* Extract arguments from the hashmap */` |
|      9 | 5049 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 5050 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|      9 | 5051 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|      - | 5052 | `	/* Release the container */` |
|      9 | 5053 | `	SySetRelease(&sArg);` |
|      9 | 5054 | `	if( rc != SXRET_OK ){` |
|      - | 5055 | `		/* The result append ran out of memory: raise a fatal. */` |
|    ! 0 | 5056 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5057 | `	}` |
|      9 | 5058 | `	return PH7_OK;` |
|     10 | 5059 | `}` |
|      - | 5060 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5061 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5062 | `/*` |
|      - | 5063 | ` * Symisc eXtension.` |
|      - | 5064 | ` * string size_format(int64 $size)` |
|      - | 5065 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 5066 | ` *  Example:` |
|      - | 5067 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 5068 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 5069 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 5070 | ` * Parameter` |
|      - | 5071 | ` *  $size` |
|      - | 5072 | ` *    Entity size in bytes.` |
|      - | 5073 | ` * Return` |
|      - | 5074 | ` *   Formatted string representation of the given size.` |
|      - | 5075 | ` */` |
|     24 | 5076 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5077 | `{` |
|      - | 5078 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 5079 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 5080 | `	sxi32 nRest,i_32;` |
|      - | 5081 | `	ph7_int64 iSize;` |
|     25 | 5082 | `	int c = -1; /* index in zUnit[] */` |
|      - | 5083 |  |
|     25 | 5084 | `	if( nArg < 1 ){` |
|      - | 5085 | `		/* Missing argument,return the empty string */` |
|      3 | 5086 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5087 | `		return PH7_OK;` |
|      - | 5088 | `	}` |
|      - | 5089 | `	/* Extract the given size */` |
|     23 | 5090 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 5091 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 5092 | `		/* Don't bother formatting,return immediately */` |
|      5 | 5093 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 5094 | `		return PH7_OK;` |
|      - | 5095 | `	}` |
|     19 | 5096 | `	for(;;){` |
|     39 | 5097 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 5098 | `		iSize >>= 10;` |
|     39 | 5099 | `		c++;` |
|     39 | 5100 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 5101 | `			break;` |
|      - | 5102 | `		}` |
|      1 | 5103 | `	}` |
|     19 | 5104 | `	nRest /= 100;` |
|     19 | 5105 | `	if( nRest > 9 ){` |
|    ! 0 | 5106 | `		nRest = 9;` |
|    ! 0 | 5107 | `	}` |
|     19 | 5108 | `	if( iSize > 999 ){` |
|    ! 0 | 5109 | `		c++;` |
|    ! 0 | 5110 | `		nRest = 9;` |
|    ! 0 | 5111 | `		iSize = 0;` |
|    ! 0 | 5112 | `	}` |
|     19 | 5113 | `	i_32 = (sxi32)iSize;` |
|      - | 5114 | `	/* Format */` |
|     19 | 5115 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 5116 | `	return PH7_OK;` |
|     13 | 5117 | `}` |
|      - | 5118 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 5119 | `/*` |
|      - | 5120 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 5121 | ` *   Calculate the md5 hash of a string.` |
|      - | 5122 | ` * Parameter` |
|      - | 5123 | ` *  $str` |
|      - | 5124 | ` *   Input string` |
|      - | 5125 | ` * $raw_output` |
|      - | 5126 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 5127 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 5128 | ` * Return` |
|      - | 5129 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 5130 | ` */` |
|     12 | 5131 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5132 | `{` |
|      - | 5133 | `	unsigned char zDigest[16];` |
|     13 | 5134 | `	int raw_output = FALSE;` |
|      - | 5135 | `	const void *pIn;` |
|      - | 5136 | `	int nLen;` |
|     13 | 5137 | `	if( nArg < 1 ){` |
|      - | 5138 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5139 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5140 | `		return PH7_OK;` |
|      - | 5141 | `	}` |
|      - | 5142 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 5143 | `	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */` |
|     13 | 5144 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 5145 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 5146 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 5147 | `	}` |
|      - | 5148 | `	/* Compute the MD5 digest */` |
|     13 | 5149 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|     13 | 5150 | `	if( raw_output ){` |
|      - | 5151 | `		/* Output raw digest */` |
|      5 | 5152 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 5153 | `	}else{` |
|      - | 5154 | `		/* Perform a binary to hex conversion */` |
|      9 | 5155 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 5156 | `	}` |
|     13 | 5157 | `	return PH7_OK;` |
|      7 | 5158 | `}` |
|      - | 5159 | `/*` |
|      - | 5160 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 5161 | ` *   Calculate the sha1 hash of a string.` |
|      - | 5162 | ` * Parameter` |
|      - | 5163 | ` *  $str` |
|      - | 5164 | ` *   Input string` |
|      - | 5165 | ` * $raw_output` |
|      - | 5166 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 5167 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 5168 | ` * Return` |
|      - | 5169 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 5170 | ` */` |
|     10 | 5171 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5172 | `{` |
|      - | 5173 | `	unsigned char zDigest[20];` |
|     11 | 5174 | `	int raw_output = FALSE;` |
|      - | 5175 | `	const void *pIn;` |
|      - | 5176 | `	int nLen;` |
|     11 | 5177 | `	if( nArg < 1 ){` |
|      - | 5178 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5179 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5180 | `		return PH7_OK;` |
|      - | 5181 | `	}` |
|      - | 5182 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 5183 | `	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */` |
|     11 | 5184 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5185 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 5186 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 5187 | `	}` |
|      - | 5188 | `	/* Compute the SHA1 digest */` |
|     11 | 5189 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|     11 | 5190 | `	if( raw_output ){` |
|      - | 5191 | `		/* Output raw digest */` |
|      5 | 5192 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 5193 | `	}else{` |
|      - | 5194 | `		/* Perform a binary to hex conversion */` |
|      7 | 5195 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 5196 | `	}` |
|     11 | 5197 | `	return PH7_OK;` |
|      6 | 5198 | `}` |
|      - | 5199 | `/*` |
|      - | 5200 | ` * int64 crc32(string $str)` |
|      - | 5201 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 5202 | ` * Parameter` |
|      - | 5203 | ` *  $str` |
|      - | 5204 | ` *   Input string` |
|      - | 5205 | ` * Return` |
|      - | 5206 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 5207 | ` */` |
|      2 | 5208 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5209 | `{` |
|      - | 5210 | `	const void *pIn;` |
|      - | 5211 | `	sxu32 nCRC;` |
|      - | 5212 | `	int nLen;` |
|      3 | 5213 | `	if( nArg < 1 ){` |
|      - | 5214 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 5215 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5216 | `		return PH7_OK;` |
|      - | 5217 | `	}` |
|      - | 5218 | `	/* Extract the input string */` |
|      3 | 5219 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5220 | `	if( nLen < 1 ){` |
|      - | 5221 | `		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike` |
|      - | 5222 | `		 * md5()/sha1(), whose empty-string digests are non-zero. */` |
|    ! 0 | 5223 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5224 | `		return PH7_OK;` |
|      - | 5225 | `	}` |
|      - | 5226 | `	/* Calculate the sum */` |
|      3 | 5227 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 5228 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 5229 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 5230 | `	return PH7_OK;` |
|      2 | 5231 | `}` |
|      - | 5232 | `/*` |
|      - | 5233 | ` * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is` |
|      - | 5234 | ` * described by a small record so one dispatch (and one generic HMAC) serves them` |
|      - | 5235 | ` * all. Thin adapters normalize the differing context types and the reversed` |
|      - | 5236 | ` * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.` |
|      - | 5237 | ` */` |
|     11 | 5238 | `static void HashMd5Init(HashCtx *c){ MD5Init(&c->md5); }` |
|     15 | 5239 | `static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }` |
|     11 | 5240 | `static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }` |
|     11 | 5241 | `static void HashSha1Init(HashCtx *c){ SHA1Init(&c->sha1); }` |
|     15 | 5242 | `static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }` |
|     11 | 5243 | `static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }` |
|      9 | 5244 | `static void HashSha224Init(HashCtx *c){ SHA224Init(&c->sha256); }` |
|     33 | 5245 | `static void HashSha256Init(HashCtx *c){ SHA256Init(&c->sha256); }` |
|     57 | 5246 | `static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }` |
|     41 | 5247 | `static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }` |
|      9 | 5248 | `static void HashSha384Init(HashCtx *c){ SHA384Init(&c->sha512); }` |
|     15 | 5249 | `static void HashSha512Init(HashCtx *c){ SHA512Init(&c->sha512); }` |
|     27 | 5250 | `static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }` |
|     23 | 5251 | `static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }` |
|      - | 5252 | `typedef struct HashAlgo HashAlgo;` |
|      - | 5253 | `struct HashAlgo {` |
|      - | 5254 | `	const char *zName;   /* lowercase canonical name */` |
|      - | 5255 | `	int nDigestLen;      /* output bytes: 16/20/28/32/48/64 */` |
|      - | 5256 | `	int nBlockLen;       /* internal block bytes (for HMAC): 64 or 128 */` |
|      - | 5257 | `	void (*xInit)(HashCtx *);` |
|      - | 5258 | `	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);` |
|      - | 5259 | `	void (*xFinal)(HashCtx *,unsigned char *);` |
|      - | 5260 | `};` |
|      - | 5261 | `static const HashAlgo aHashAlgo[] = {` |
|      - | 5262 | `	{ "md5",    16, 64,  HashMd5Init,    HashMd5Update,    HashMd5Final    },` |
|      - | 5263 | `	{ "sha1",   20, 64,  HashSha1Init,   HashSha1Update,   HashSha1Final   },` |
|      - | 5264 | `	{ "sha224", 28, 64,  HashSha224Init, HashSha256Update, HashSha256Final },` |
|      - | 5265 | `	{ "sha256", 32, 64,  HashSha256Init, HashSha256Update, HashSha256Final },` |
|      - | 5266 | `	{ "sha384", 48, 128, HashSha384Init, HashSha512Update, HashSha512Final },` |
|      - | 5267 | `	{ "sha512", 64, 128, HashSha512Init, HashSha512Update, HashSha512Final },` |
|      - | 5268 | `};` |
|      - | 5269 | `/* Case-insensitive algorithm lookup (PHP accepts 'SHA256' etc.). */` |
|     73 | 5270 | `static const HashAlgo * HashFindAlgo(const char *zName,int nLen){` |
|      - | 5271 | `	sxu32 i;` |
|    279 | 5272 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|    272 | 5273 | `		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen` |
|    211 | 5274 | `			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){` |
|     67 | 5275 | `			return &aHashAlgo[i];` |
|      - | 5276 | `		}` |
|    106 | 5277 | `	}` |
|      6 | 5278 | `	return 0;` |
|     38 | 5279 | `}` |
|      - | 5280 | `/*` |
|      - | 5281 | ` * string hash(string $algo,string $data[,bool $binary = false])` |
|      - | 5282 | ` *   Generate a hash value (message digest).` |
|      - | 5283 | ` */` |
|     54 | 5284 | `static int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5285 | `{` |
|      - | 5286 | `	const HashAlgo *pAlgo;` |
|      - | 5287 | `	const char *zAlgo,*zData;` |
|     56 | 5288 | `	int nAlgoLen,nDataLen,raw_output = FALSE;` |
|      - | 5289 | `	HashCtx sCtx;` |
|      - | 5290 | `	unsigned char zDigest[64];` |
|     56 | 5291 | `	if( nArg < 2 ){` |
|    ! 0 | 5292 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5293 | `			"hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5294 | `	}` |
|     56 | 5295 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     56 | 5296 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     56 | 5297 | `	if( pAlgo == 0 ){` |
|      3 | 5298 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5299 | `			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - | 5300 | `	}` |
|     53 | 5301 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     53 | 5302 | `	if( nArg > 2 ){` |
|      9 | 5303 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|      4 | 5304 | `	}` |
|     53 | 5305 | `	pAlgo->xInit(&sCtx);` |
|     53 | 5306 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     53 | 5307 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     53 | 5308 | `	if( raw_output ){` |
|      9 | 5309 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|      5 | 5310 | `	}else{` |
|     45 | 5311 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - | 5312 | `	}` |
|     53 | 5313 | `	return PH7_OK;` |
|     29 | 5314 | `}` |
|      - | 5315 | `/*` |
|      - | 5316 | ` * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])` |
|      - | 5317 | ` *   Generate a keyed hash value using the HMAC method (RFC 2104).` |
|      - | 5318 | ` */` |
|     16 | 5319 | `static int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5320 | `{` |
|      - | 5321 | `	const HashAlgo *pAlgo;` |
|      - | 5322 | `	const char *zAlgo,*zData,*zKey;` |
|     18 | 5323 | `	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;` |
|      - | 5324 | `	HashCtx sCtx;` |
|      - | 5325 | `	unsigned char zKeyBlock[128],zIpad[128],zOpad[128],zInner[64],zDigest[64];` |
|      - | 5326 | `	int i,nBlock,nDigest;` |
|     18 | 5327 | `	if( nArg < 3 ){` |
|    ! 0 | 5328 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5329 | `			"hash_hmac() expects at least 3 arguments, %d given",nArg);` |
|      - | 5330 | `	}` |
|     18 | 5331 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     18 | 5332 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     18 | 5333 | `	if( pAlgo == 0 ){` |
|      3 | 5334 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5335 | `			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 5336 | `	}` |
|     15 | 5337 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     15 | 5338 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|     15 | 5339 | `	if( nArg > 3 ){` |
|      3 | 5340 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|      1 | 5341 | `	}` |
|     15 | 5342 | `	nBlock = pAlgo->nBlockLen;` |
|     15 | 5343 | `	nDigest = pAlgo->nDigestLen;` |
|      - | 5344 | `	/* Reduce the key to a single block: hash it if longer than the block, then` |
|      - | 5345 | `	 * zero-pad (a short or empty key is just zero-padded). */` |
|     15 | 5346 | `	SyZero(zKeyBlock,sizeof(zKeyBlock));` |
|     15 | 5347 | `	if( nKeyLen > nBlock ){` |
|      3 | 5348 | `		pAlgo->xInit(&sCtx);` |
|      3 | 5349 | `		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);` |
|      3 | 5350 | `		pAlgo->xFinal(&sCtx,zKeyBlock);` |
|     14 | 5351 | `	}else if( nKeyLen > 0 ){` |
|     11 | 5352 | `		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);` |
|      5 | 5353 | `	}` |
|   1039 | 5354 | `	for( i = 0; i < nBlock; i++ ){` |
|   1025 | 5355 | `		zIpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);` |
|   1025 | 5356 | `		zOpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);` |
|    513 | 5357 | `	}` |
|      - | 5358 | `	/* inner = H((key ^ ipad) \|\| data) */` |
|     15 | 5359 | `	pAlgo->xInit(&sCtx);` |
|     15 | 5360 | `	pAlgo->xUpdate(&sCtx,zIpad,(unsigned int)nBlock);` |
|     15 | 5361 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     15 | 5362 | `	pAlgo->xFinal(&sCtx,zInner);` |
|      - | 5363 | `	/* out = H((key ^ opad) \|\| inner) */` |
|     15 | 5364 | `	pAlgo->xInit(&sCtx);` |
|     15 | 5365 | `	pAlgo->xUpdate(&sCtx,zOpad,(unsigned int)nBlock);` |
|     15 | 5366 | `	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)nDigest);` |
|     15 | 5367 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     15 | 5368 | `	if( raw_output ){` |
|      3 | 5369 | `		ph7_result_string(pCtx,(const char *)zDigest,nDigest);` |
|      2 | 5370 | `	}else{` |
|     13 | 5371 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);` |
|      - | 5372 | `	}` |
|     15 | 5373 | `	return PH7_OK;` |
|     10 | 5374 | `}` |
|      - | 5375 | `/*` |
|      - | 5376 | ` * bool hash_equals(string $known_string,string $user_string)` |
|      - | 5377 | ` *   Timing-attack-safe string comparison.` |
|      - | 5378 | ` */` |
|     12 | 5379 | `static int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5380 | `{` |
|      - | 5381 | `	const char *zKnown,*zUser;` |
|      - | 5382 | `	int nKnown,nUser,i;` |
|     14 | 5383 | `	volatile unsigned char vDiff = 0;` |
|     14 | 5384 | `	if( nArg < 2 ){` |
|    ! 0 | 5385 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5386 | `			"hash_equals() expects exactly 2 arguments, %d given",nArg);` |
|      - | 5387 | `	}` |
|     14 | 5388 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      4 | 5389 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5390 | `			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",` |
|      1 | 5391 | `			ph7_type_name(apArg[0]));` |
|      - | 5392 | `	}` |
|     11 | 5393 | `	if( !ph7_value_is_string(apArg[1]) ){` |
|    ! 0 | 5394 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5395 | `			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",` |
|    ! 0 | 5396 | `			ph7_type_name(apArg[1]));` |
|      - | 5397 | `	}` |
|     11 | 5398 | `	zKnown = ph7_value_to_string(apArg[0],&nKnown);` |
|     11 | 5399 | `	zUser = ph7_value_to_string(apArg[1],&nUser);` |
|     11 | 5400 | `	if( nKnown != nUser ){` |
|      5 | 5401 | `		ph7_result_bool(pCtx,0);` |
|      5 | 5402 | `		return PH7_OK;` |
|      - | 5403 | `	}` |
|      - | 5404 | `	/* Constant-time: read every byte, never short-circuit. */` |
|     19 | 5405 | `	for( i = 0; i < nKnown; i++ ){` |
|     13 | 5406 | `		vDiff \|= (unsigned char)(zKnown[i] ^ zUser[i]);` |
|      7 | 5407 | `	}` |
|      7 | 5408 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|      7 | 5409 | `	return PH7_OK;` |
|      8 | 5410 | `}` |
|      - | 5411 | `/*` |
|      - | 5412 | ` * array hash_algos(void)` |
|      - | 5413 | ` *   Return a list of the registered hashing algorithms.` |
|      - | 5414 | ` */` |
|      2 | 5415 | `static int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5416 | `{` |
|      - | 5417 | `	ph7_value *pArray,*pValue;` |
|      - | 5418 | `	sxu32 i;` |
|      1 | 5419 | `	SXUNUSED(nArg);` |
|      1 | 5420 | `	SXUNUSED(apArg);` |
|      3 | 5421 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5422 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 5423 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 5424 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5425 | `		return PH7_OK;` |
|      - | 5426 | `	}` |
|     15 | 5427 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|     13 | 5428 | `		ph7_value_string(pValue,aHashAlgo[i].zName,-1);` |
|     13 | 5429 | `		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);` |
|     13 | 5430 | `		ph7_value_reset_string_cursor(pValue);` |
|      7 | 5431 | `	}` |
|      3 | 5432 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5433 | `	return PH7_OK;` |
|      2 | 5434 | `}` |
|      - | 5435 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 5436 | `/*` |
|      - | 5437 | ` * password_* (bcrypt). These live in ext/standard in real PHP — outside the` |
|      - | 5438 | ` * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.` |
|      - | 5439 | ` */` |
|      - | 5440 | `/*` |
|      - | 5441 | ` * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a` |
|      - | 5442 | ` * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).` |
|      - | 5443 | ` */` |
|     40 | 5444 | `static int BcryptParseHash(const char *zHash,int nHash,int *piCost)` |
|      1 | 5445 | `{` |
|      - | 5446 | `	int iCost;` |
|     40 | 5447 | `	if( nHash != 60 \|\| zHash[0] != '$' \|\| zHash[1] != '2' \|\| zHash[3] != '$'` |
|     29 | 5448 | `		\|\| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){` |
|     13 | 5449 | `		return FALSE;` |
|      - | 5450 | `	}` |
|     29 | 5451 | `	if( zHash[4] < '0' \|\| zHash[4] > '9' \|\| zHash[5] < '0' \|\| zHash[5] > '9' \|\| zHash[6] != '$' ){` |
|    ! 0 | 5452 | `		return FALSE;` |
|      - | 5453 | `	}` |
|     29 | 5454 | `	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');` |
|     29 | 5455 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      3 | 5456 | `		return FALSE;` |
|      - | 5457 | `	}` |
|     27 | 5458 | `	if( piCost ){ *piCost = iCost; }` |
|     27 | 5459 | `	return TRUE;` |
|     21 | 5460 | `}` |
|      - | 5461 | `/*` |
|      - | 5462 | ` * TRUE if the $algo argument selects bcrypt: null (PASSWORD_DEFAULT) or the` |
|      - | 5463 | ` * "2y" id (PASSWORD_BCRYPT/PASSWORD_DEFAULT). bcrypt is the only supported algo.` |
|      - | 5464 | ` */` |
|     20 | 5465 | `static int BcryptIsBcryptAlgo(ph7_value *pAlgo)` |
|      3 | 5466 | `{` |
|     23 | 5467 | `	if( ph7_value_is_null(pAlgo) ){` |
|    ! 0 | 5468 | `		return TRUE;` |
|      - | 5469 | `	}` |
|     23 | 5470 | `	if( ph7_value_is_string(pAlgo) ){` |
|      - | 5471 | `		int nAlgo;` |
|     23 | 5472 | `		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);` |
|     23 | 5473 | `		return ( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' );` |
|      - | 5474 | `	}` |
|    ! 0 | 5475 | `	return FALSE;` |
|     13 | 5476 | `}` |
|      - | 5477 | `/*` |
|      - | 5478 | ` * bool\|string password_hash(string $password,string\|int\|null $algo[,array $options])` |
|      - | 5479 | ` *  Create a bcrypt hash of the password.` |
|      - | 5480 | ` */` |
|     16 | 5481 | `static int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 5482 | `{` |
|      - | 5483 | `	const char *zPwd;` |
|     19 | 5484 | `	int nPwd,iCost = 12;` |
|      - | 5485 | `	unsigned char aSalt[16];` |
|      - | 5486 | `	char zHash[60];` |
|     19 | 5487 | `	if( nArg < 2 ){` |
|    ! 0 | 5488 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5489 | `			"password_hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5490 | `	}` |
|     19 | 5491 | `	if( !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      3 | 5492 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5493 | `			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");` |
|      - | 5494 | `	}` |
|      - | 5495 | `	/* cost from $options['cost'] (default 12). */` |
|     16 | 5496 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|     14 | 5497 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|     14 | 5498 | `		if( pCost ){ iCost = ph7_value_to_int(pCost); }` |
|      6 | 5499 | `	}` |
|     16 | 5500 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      4 | 5501 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 5502 | `			"Invalid bcrypt cost parameter specified: %d",iCost);` |
|      - | 5503 | `	}` |
|     13 | 5504 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     13 | 5505 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 5506 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - | 5507 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|      - | 5508 | `	}` |
|     13 | 5509 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){` |
|    ! 0 | 5510 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5511 | `		return PH7_OK;` |
|      - | 5512 | `	}` |
|     13 | 5513 | `	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));` |
|     13 | 5514 | `	return PH7_OK;` |
|     11 | 5515 | `}` |
|      - | 5516 | `/*` |
|      - | 5517 | ` * bool password_verify(string $password,string $hash)` |
|      - | 5518 | ` *  Verify a password against a bcrypt hash. Never throws on a malformed hash.` |
|      - | 5519 | ` */` |
|     28 | 5520 | `static int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5521 | `{` |
|      - | 5522 | `	const char *zPwd,*zHash;` |
|      - | 5523 | `	int nPwd,nHash,iCost,i;` |
|      - | 5524 | `	unsigned char aSalt[16];` |
|      - | 5525 | `	char zComputed[60];` |
|     29 | 5526 | `	volatile unsigned char vDiff = 0;` |
|     29 | 5527 | `	if( nArg < 2 ){` |
|    ! 0 | 5528 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5529 | `			"password_verify() expects exactly 2 arguments, %d given",nArg);` |
|      - | 5530 | `	}` |
|     29 | 5531 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     29 | 5532 | `	zHash = ph7_value_to_string(apArg[1],&nHash);` |
|     29 | 5533 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|     11 | 5534 | `		ph7_result_bool(pCtx,0);` |
|     11 | 5535 | `		return PH7_OK;` |
|      - | 5536 | `	}` |
|      - | 5537 | `	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */` |
|     19 | 5538 | `	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 5539 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5540 | `		return PH7_OK;` |
|      - | 5541 | `	}` |
|     19 | 5542 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){` |
|    ! 0 | 5543 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5544 | `		return PH7_OK;` |
|      - | 5545 | `	}` |
|      - | 5546 | `	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps` |
|      - | 5547 | `	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */` |
|    577 | 5548 | `	for( i = 29; i < 60; i++ ){` |
|    559 | 5549 | `		vDiff \|= (unsigned char)(zComputed[i] ^ zHash[i]);` |
|    280 | 5550 | `	}` |
|     19 | 5551 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|     19 | 5552 | `	return PH7_OK;` |
|     15 | 5553 | `}` |
|      - | 5554 | `/*` |
|      - | 5555 | ` * array password_get_info(string $hash)` |
|      - | 5556 | ` *  Return ["algo"=>id\|null, "algoName"=>name, "options"=>[...]].` |
|      - | 5557 | ` */` |
|      6 | 5558 | `static int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5559 | `{` |
|      7 | 5560 | `	const char *zHash = "";` |
|      7 | 5561 | `	int nHash,iCost = 0,bBcrypt = 0;` |
|      - | 5562 | `	ph7_value *pArray,*pOptions,*pVal;` |
|      7 | 5563 | `	if( nArg > 0 ){` |
|      7 | 5564 | `		zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 5565 | `		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);` |
|      3 | 5566 | `	}` |
|      7 | 5567 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 5568 | `	pOptions = ph7_context_new_array(pCtx);` |
|      7 | 5569 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      7 | 5570 | `	if( pArray == 0 \|\| pOptions == 0 \|\| pVal == 0 ){` |
|    ! 0 | 5571 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5572 | `		return PH7_OK;` |
|      - | 5573 | `	}` |
|      7 | 5574 | `	if( bBcrypt ){` |
|      5 | 5575 | `		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */` |
|      5 | 5576 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 5577 | `		ph7_value_reset_string_cursor(pVal);` |
|      5 | 5578 | `		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);` |
|      5 | 5579 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      5 | 5580 | `		ph7_value_int(pVal,iCost);` |
|      5 | 5581 | `		ph7_array_add_strkey_elem(pOptions,"cost",pVal);` |
|      3 | 5582 | `	}else{` |
|      3 | 5583 | `		ph7_value_null(pVal);                          /* algo => null */` |
|      3 | 5584 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      3 | 5585 | `		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);` |
|      3 | 5586 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      - | 5587 | `	}` |
|      7 | 5588 | `	ph7_array_add_strkey_elem(pArray,"options",pOptions);` |
|      7 | 5589 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 5590 | `	return PH7_OK;` |
|      4 | 5591 | `}` |
|      - | 5592 | `/*` |
|      - | 5593 | ` * bool password_needs_rehash(string $hash,string\|int\|null $algo[,array $options])` |
|      - | 5594 | ` *  True if the hash was not made with the given algo/options.` |
|      - | 5595 | ` */` |
|      6 | 5596 | `static int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5597 | `{` |
|      - | 5598 | `	const char *zHash;` |
|      7 | 5599 | `	int nHash,iCost = 0,iWantCost = 12;` |
|      7 | 5600 | `	if( nArg < 2 ){` |
|    ! 0 | 5601 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5602 | `			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5603 | `	}` |
|      7 | 5604 | `	zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 5605 | `	if( !BcryptParseHash(zHash,nHash,&iCost) \|\| !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      - | 5606 | `		/* A non-bcrypt hash, or a request for a different algo → needs rehash. */` |
|      3 | 5607 | `		ph7_result_bool(pCtx,1);` |
|      3 | 5608 | `		return PH7_OK;` |
|      - | 5609 | `	}` |
|      5 | 5610 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|      5 | 5611 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|      5 | 5612 | `		if( pCost ){ iWantCost = ph7_value_to_int(pCost); }` |
|      2 | 5613 | `	}` |
|      5 | 5614 | `	ph7_result_bool(pCtx,iCost != iWantCost);` |
|      5 | 5615 | `	return PH7_OK;` |
|      4 | 5616 | `}` |
|      - | 5617 | `/*` |
|      - | 5618 | ` * filter_var() — input validation and sanitization (the ext/filter API).` |
|      - | 5619 | ` *` |
|      - | 5620 | ` * Filter and flag identifiers (values match PHP 8.5; the constants themselves` |
|      - | 5621 | ` * are registered in constant.c). The validate filters are hand-rolled rather` |
|      - | 5622 | ` * than delegating to SyStrToInt64/SyStrToReal: the former silently skips leading` |
|      - | 5623 | ` * zeros and cannot signal overflow, and the latter treats ',' as a decimal point` |
|      - | 5624 | ` * unconditionally — neither matches PHP's filter semantics.` |
|      - | 5625 | ` */` |
|      - | 5626 | `#define FV_VALIDATE_INT     257` |
|      - | 5627 | `#define FV_VALIDATE_BOOLEAN 258` |
|      - | 5628 | `#define FV_VALIDATE_FLOAT   259` |
|      - | 5629 | `#define FV_VALIDATE_REGEXP  272` |
|      - | 5630 | `#define FV_VALIDATE_URL     273` |
|      - | 5631 | `#define FV_VALIDATE_EMAIL   274` |
|      - | 5632 | `#define FV_VALIDATE_IP      275` |
|      - | 5633 | `#define FV_VALIDATE_MAC     276` |
|      - | 5634 | `#define FV_VALIDATE_DOMAIN  277` |
|      - | 5635 | `#define FV_SANITIZE_SPECIAL_CHARS      515` |
|      - | 5636 | `#define FV_DEFAULT          516 /* == FILTER_UNSAFE_RAW: pass the value through */` |
|      - | 5637 | `#define FV_SANITIZE_EMAIL   517` |
|      - | 5638 | `#define FV_SANITIZE_URL     518` |
|      - | 5639 | `#define FV_SANITIZE_NUMBER_INT   519` |
|      - | 5640 | `#define FV_SANITIZE_NUMBER_FLOAT 520` |
|      - | 5641 | `#define FV_SANITIZE_FULL_SPECIAL_CHARS 522` |
|      - | 5642 | `#define FV_FLAG_ALLOW_OCTAL  1` |
|      - | 5643 | `#define FV_FLAG_ALLOW_HEX    2` |
|      - | 5644 | `#define FV_FLAG_STRIP_LOW    4` |
|      - | 5645 | `#define FV_FLAG_STRIP_HIGH   8` |
|      - | 5646 | `#define FV_FLAG_ENCODE_LOW   16` |
|      - | 5647 | `#define FV_FLAG_ENCODE_HIGH  32` |
|      - | 5648 | `#define FV_FLAG_ENCODE_AMP   64` |
|      - | 5649 | `#define FV_FLAG_NO_ENCODE_QUOTES 128` |
|      - | 5650 | `#define FV_FLAG_STRIP_BACKTICK   512` |
|      - | 5651 | `#define FV_FLAG_ALLOW_FRACTION   4096` |
|      - | 5652 | `#define FV_FLAG_ALLOW_THOUSAND   8192` |
|      - | 5653 | `#define FV_FLAG_ALLOW_SCIENTIFIC 16384` |
|      - | 5654 | `#define FV_FLAG_IPV4  1048576` |
|      - | 5655 | `#define FV_FLAG_IPV6  2097152` |
|      - | 5656 | `#define FV_NULL_ON_FAILURE 134217728` |
|      - | 5657 | `/* The subset of flags the UNSAFE_RAW/DEFAULT string filter (FvSanitizeString)` |
|      - | 5658 | ` * acts on: when none are set the filter is a verbatim pass-through, so FV_DEFAULT` |
|      - | 5659 | ` * can shortcut. Keep this in sync with FvSanitizeString's flag handling. */` |
|      - | 5660 | `#define FV_FLAG_STRING_MASK (FV_FLAG_STRIP_LOW\|FV_FLAG_STRIP_HIGH\|FV_FLAG_STRIP_BACKTICK \` |
|      - | 5661 | `                            \|FV_FLAG_ENCODE_LOW\|FV_FLAG_ENCODE_HIGH\|FV_FLAG_ENCODE_AMP)` |
|      - | 5662 |  |
|      - | 5663 | `/* Trim leading/trailing PHP whitespace, adjusting the (*pz,*pn) view in place.` |
|      - | 5664 | ` * SyisSpace (isspace) matches PHP's filter whitespace set " \t\n\r\v\f". */` |
|    153 | 5665 | `static void FvTrim(const char **pz,int *pn){` |
|    153 | 5666 | `	const char *z = *pz;` |
|    153 | 5667 | `	int n = *pn;` |
|    157 | 5668 | `	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }` |
|    161 | 5669 | `	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }` |
|    153 | 5670 | `	*pz = z; *pn = n;` |
|    153 | 5671 | `}` |
|      - | 5672 | `/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     57 | 5673 | `static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){` |
|     57 | 5674 | `	int neg = 0, i;` |
|     57 | 5675 | `	sxu64 u = 0;` |
|     57 | 5676 | `	FvTrim(&z,&n);` |
|     57 | 5677 | `	if( n==0 ){ return 0; }` |
|     51 | 5678 | `	if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }` |
|     51 | 5679 | `	if( n==0 ){ return 0; }` |
|     49 | 5680 | `	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'\|\|z[1]=='X') ){` |
|      3 | 5681 | `		z += 2; n -= 2;` |
|      3 | 5682 | `		if( n==0 ){ return 0; }` |
|      7 | 5683 | `		for( i=0; i<n; i++ ){` |
|      5 | 5684 | `			int h = SyHexToint((unsigned char)z[i]);` |
|      5 | 5685 | `			if( h<0 ){ return 0; }` |
|      5 | 5686 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }` |
|      5 | 5687 | `			u = u*16 + (sxu64)h;` |
|      3 | 5688 | `		}` |
|     48 | 5689 | `	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){` |
|      9 | 5690 | `		for( i=0; i<n; i++ ){` |
|      7 | 5691 | `			if( z[i]<'0' \|\| z[i]>'7' ){ return 0; }` |
|      7 | 5692 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }` |
|      7 | 5693 | `			u = u*8 + (sxu64)(z[i]-'0');` |
|      4 | 5694 | `		}` |
|      2 | 5695 | `	}else{` |
|     45 | 5696 | `		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */` |
|    201 | 5697 | `		for( i=0; i<n; i++ ){` |
|    173 | 5698 | `			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|    161 | 5699 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }` |
|    161 | 5700 | `			u = u*10 + (sxu64)(z[i]-'0');` |
|     81 | 5701 | `		}` |
|      - | 5702 | `	}` |
|     33 | 5703 | `	if( neg ){` |
|      5 | 5704 | `		if( u > 0x8000000000000000ULL ){ return 0; }` |
|      5 | 5705 | `		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */` |
|      3 | 5706 | `	}else{` |
|     29 | 5707 | `		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }` |
|     27 | 5708 | `		*pOut = (ph7_int64)u;` |
|      - | 5709 | `	}` |
|     31 | 5710 | `	return 1;` |
|     29 | 5711 | `}` |
|      - | 5712 | `/* FILTER_VALIDATE_FLOAT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     69 | 5713 | `static int FvValidateFloat(const char *z,int n,int flags,double *pOut){` |
|      - | 5714 | `	char zBuf[512];` |
|     69 | 5715 | `	int i, m = 0, seenDigit = 0;` |
|     69 | 5716 | `	const char *zv; int nv; double d = 0;` |
|     69 | 5717 | `	FvTrim(&z,&n);` |
|      - | 5718 | `	/* Bound the input: zBuf[512] holds the thousand-separator-stripped copy, and` |
|      - | 5719 | `	 * the cap also rejects the pathological 500+ digit floats PHP refuses. */` |
|     69 | 5720 | `	if( n==0 \|\| n>500 ){ return 0; }` |
|     69 | 5721 | `	if( flags & FV_FLAG_ALLOW_THOUSAND ){` |
|      - | 5722 | `		/* Commas are optional, but when present they must group the integer part` |
|      - | 5723 | `		 * into a leading run of 1..3 digits followed by groups of exactly 3` |
|      - | 5724 | `		 * ("1,000" ok, "1,5"/"1234,567" rejected). Strip them into zBuf and reject` |
|      - | 5725 | `		 * a comma anywhere in the fractional/exponent tail. */` |
|     25 | 5726 | `		int s = 0, intEnd, segStart, segIdx, hasComma = 0;` |
|     25 | 5727 | `		if( s<n && (z[s]=='+'\|\|z[s]=='-') ){ zBuf[m++] = z[s]; s++; }` |
|     25 | 5728 | `		intEnd = s;` |
|    167 | 5729 | `		while( intEnd<n && z[intEnd]!='.' && z[intEnd]!='e' && z[intEnd]!='E' ){` |
|    143 | 5730 | `			if( z[intEnd]==',' ){ hasComma = 1; }` |
|    143 | 5731 | `			intEnd++;` |
|      1 | 5732 | `		}` |
|     25 | 5733 | `		if( hasComma ){` |
|     25 | 5734 | `			segStart = s; segIdx = 0;` |
|    165 | 5735 | `			for( i=s; i<=intEnd; i++ ){` |
|    151 | 5736 | `				if( i==intEnd \|\| z[i]==',' ){` |
|     49 | 5737 | `					int segLen = i - segStart, k;` |
|     49 | 5738 | `					if( segIdx==0 ){ if( segLen<1 \|\| segLen>3 ){ return 0; } }` |
|     25 | 5739 | `					else if( segLen!=3 ){ return 0; }` |
|    119 | 5740 | `					for( k=segStart; k<i; k++ ){` |
|     81 | 5741 | `						if( !SyisDigit((unsigned char)z[k]) ){ return 0; }` |
|     81 | 5742 | `						zBuf[m++] = z[k];` |
|     41 | 5743 | `					}` |
|     39 | 5744 | `					segStart = i+1; segIdx++;` |
|     19 | 5745 | `				}` |
|     71 | 5746 | `			}` |
|      8 | 5747 | `		}else{` |
|    ! 0 | 5748 | `			for( i=s; i<intEnd; i++ ){ zBuf[m++] = z[i]; }` |
|      - | 5749 | `		}` |
|     27 | 5750 | `		for( i=intEnd; i<n; i++ ){` |
|     13 | 5751 | `			if( z[i]==',' ){ return 0; }` |
|     13 | 5752 | `			zBuf[m++] = z[i];` |
|      7 | 5753 | `		}` |
|     15 | 5754 | `		zv = zBuf; nv = m;` |
|      8 | 5755 | `	}else{` |
|     45 | 5756 | `		zv = z; nv = n;` |
|      - | 5757 | `	}` |
|     59 | 5758 | `	i = 0;` |
|     59 | 5759 | `	if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|    167 | 5760 | `	while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     59 | 5761 | `	if( i<nv && zv[i]=='.' ){` |
|     21 | 5762 | `		i++;` |
|     39 | 5763 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     10 | 5764 | `	}` |
|     59 | 5765 | `	if( !seenDigit ){ return 0; }` |
|     57 | 5766 | `	if( i<nv && (zv[i]=='e'\|\|zv[i]=='E') ){` |
|     29 | 5767 | `		i++;` |
|     29 | 5768 | `		if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|     29 | 5769 | `		if( i>=nv \|\| !SyisDigit((unsigned char)zv[i]) ){ return 0; }` |
|    105 | 5770 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; }` |
|     14 | 5771 | `	}` |
|     57 | 5772 | `	if( i!=nv ){ return 0; } /* trailing junk */` |
|      - | 5773 | `	/* The grammar above guarantees zv[0..nv) is a clean ASCII decimal float (no hex /` |
|      - | 5774 | `	 * inf / nan / trailing junk), so it is safe to hand to libc strtod, which — unlike` |
|      - | 5775 | `	 * SyStrToReal (15 sig-digits + exponent clamped to 308, so it silently saturates` |
|      - | 5776 | `	 * overflowing magnitudes to a finite value) — is overflow/underflow-aware and` |
|      - | 5777 | `	 * correctly rounded. strtod needs a NUL-terminated string: the ALLOW_THOUSAND path` |
|      - | 5778 | `	 * already built the span in zBuf (zv==zBuf); the plain path must copy it there (z is` |
|      - | 5779 | `	 * const + not NUL-terminated). nv <= n <= 500 < sizeof(zBuf) by the cap above.` |
|      - | 5780 | `	 * Matches PHP 8.5 byte-for-byte: reject overflow (-> +/-INF) and total underflow` |
|      - | 5781 | `	 * (-> 0.0), keep subnormals (nonzero, errno==ERANGE) and a genuine "0" (errno==0). */` |
|     53 | 5782 | `	if( zv != zBuf ){ SyMemcpy(zv,zBuf,(sxu32)nv); }` |
|     53 | 5783 | `	zBuf[nv] = 0;` |
|     53 | 5784 | `	errno = 0;` |
|     53 | 5785 | `	d = strtod(zBuf,0);` |
|     53 | 5786 | `	if( errno == ERANGE && (d == HUGE_VAL \|\| d == -HUGE_VAL \|\| d == 0.0) ){` |
|     15 | 5787 | `		return 0;` |
|      - | 5788 | `	}` |
|     39 | 5789 | `	*pOut = d;` |
|     39 | 5790 | `	return 1;` |
|     35 | 5791 | `}` |
|      - | 5792 | `/* FILTER_VALIDATE_BOOLEAN. Returns 1 if the string is recognized (sets *pBool),` |
|      - | 5793 | ` * 0 if it is unrecognized (the failure path). "0"/"false"/"" are recognized as` |
|      - | 5794 | ` * false, NOT failures. */` |
|     33 | 5795 | `static int FvValidateBool(const char *z,int n,int *pBool){` |
|     33 | 5796 | `	FvTrim(&z,&n);` |
|     32 | 5797 | `	if( (n==1 && z[0]=='1') \|\| (n==4 && SyStrnicmp(z,"true",4)==0)` |
|     25 | 5798 | `	    \|\| (n==2 && SyStrnicmp(z,"on",2)==0) \|\| (n==3 && SyStrnicmp(z,"yes",3)==0) ){` |
|     11 | 5799 | `		*pBool = 1; return 1;` |
|      - | 5800 | `	}` |
|     22 | 5801 | `	if( n==0 \|\| (n==1 && z[0]=='0') \|\| (n==5 && SyStrnicmp(z,"false",5)==0)` |
|     11 | 5802 | `	    \|\| (n==3 && SyStrnicmp(z,"off",3)==0) \|\| (n==2 && SyStrnicmp(z,"no",2)==0) ){` |
|     11 | 5803 | `		*pBool = 0; return 1;` |
|      - | 5804 | `	}` |
|      9 | 5805 | `	return 0;` |
|     15 | 5806 | `}` |
|      - | 5807 | `/* IPv4 dotted-quad: exactly 4 octets 0..255, no leading zeros. */` |
|     33 | 5808 | `static int FvValidateIp4(const char *z,int n){` |
|     33 | 5809 | `	int i = 0, parts = 0;` |
|     77 | 5810 | `	while( i<n ){` |
|     65 | 5811 | `		int val = 0, digits = 0, start = i;` |
|    143 | 5812 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){` |
|     85 | 5813 | `			val = val*10 + (z[i]-'0');` |
|     85 | 5814 | `			if( val>255 ){ return 0; }` |
|     79 | 5815 | `			digits++; i++;` |
|      1 | 5816 | `		}` |
|     59 | 5817 | `		if( digits==0 \|\| digits>3 ){ return 0; }` |
|     49 | 5818 | `		if( digits>1 && z[start]=='0' ){ return 0; } /* leading zero */` |
|     45 | 5819 | `		parts++;` |
|     45 | 5820 | `		if( parts>4 ){ return 0; }` |
|     45 | 5821 | `		if( i<n ){` |
|     33 | 5822 | `			if( z[i]!='.' ){ return 0; }` |
|     33 | 5823 | `			i++;` |
|     33 | 5824 | `			if( i>=n ){ return 0; } /* trailing dot */` |
|     16 | 5825 | `		}` |
|      1 | 5826 | `	}` |
|     13 | 5827 | `	return parts==4;` |
|     17 | 5828 | `}` |
|      - | 5829 | `/* A colon-separated run of IPv6 hextets with no "::" (n may be 0 -> 0 groups),` |
|      - | 5830 | ` * allowing a trailing embedded IPv4. Returns the 16-bit group count or -1. */` |
|     19 | 5831 | `static int FvIp6Hextets(const char *z,int n){` |
|     19 | 5832 | `	int i = 0, segStart = 0, groups = 0;` |
|     19 | 5833 | `	if( n==0 ){ return 0; }` |
|    145 | 5834 | `	while( i<=n ){` |
|    133 | 5835 | `		if( i==n \|\| z[i]==':' ){` |
|     23 | 5836 | `			int segLen = i - segStart, j, isV4 = 0;` |
|     23 | 5837 | `			if( segLen==0 ){ return -1; } /* an empty hextet (stray ':') */` |
|     77 | 5838 | `			for( j=segStart; j<i; j++ ){ if( z[j]=='.' ){ isV4 = 1; break; } }` |
|     23 | 5839 | `			if( isV4 ){` |
|     11 | 5840 | `				if( i!=n ){ return -1; } /* IPv4 only as the final token */` |
|     11 | 5841 | `				if( !FvValidateIp4(z+segStart,segLen) ){ return -1; }` |
|      5 | 5842 | `				groups += 2;` |
|      3 | 5843 | `			}else{` |
|     13 | 5844 | `				if( segLen>4 ){ return -1; }` |
|     47 | 5845 | `				for( j=segStart; j<i; j++ ){ if( SyHexToint((unsigned char)z[j])<0 ){ return -1; } }` |
|     13 | 5846 | `				groups++;` |
|      - | 5847 | `			}` |
|     17 | 5848 | `			segStart = i+1;` |
|      8 | 5849 | `		}` |
|    127 | 5850 | `		i++;` |
|      1 | 5851 | `	}` |
|     13 | 5852 | `	return groups;` |
|     10 | 5853 | `}` |
|      - | 5854 | `/* IPv6: at most one "::" zero-run; 8 groups exactly, or fewer when "::" present. */` |
|     19 | 5855 | `static int FvValidateIp6(const char *z,int n){` |
|     19 | 5856 | `	const char *zDbl = 0;` |
|      - | 5857 | `	int i, ga, gb;` |
|    139 | 5858 | `	for( i=0; i+1<n; i++ ){` |
|    123 | 5859 | `		if( z[i]==':' && z[i+1]==':' ){` |
|     13 | 5860 | `			if( zDbl ){ return 0; } /* a second "::" is invalid */` |
|     11 | 5861 | `			zDbl = z+i;` |
|      5 | 5862 | `		}` |
|     61 | 5863 | `	}` |
|     17 | 5864 | `	if( zDbl==0 ){` |
|      9 | 5865 | `		return FvIp6Hextets(z,n)==8;` |
|    ! 0 | 5866 | `	}else{` |
|      9 | 5867 | `		int lenA = (int)(zDbl - z);` |
|      9 | 5868 | `		int lenB = n - lenA - 2;` |
|      9 | 5869 | `		ga = (lenA==0) ? 0 : FvIp6Hextets(z,lenA);` |
|      9 | 5870 | `		gb = (lenB==0) ? 0 : FvIp6Hextets(zDbl+2,lenB);` |
|      9 | 5871 | `		if( ga<0 \|\| gb<0 ){ return 0; }` |
|      9 | 5872 | `		return (ga+gb)<=7; /* "::" stands for at least one zero group */` |
|      - | 5873 | `	}` |
|     10 | 5874 | `}` |
|     25 | 5875 | `static int FvValidateIp(const char *z,int n,int flags){` |
|     25 | 5876 | `	int v4 = (flags & FV_FLAG_IPV4), v6 = (flags & FV_FLAG_IPV6);` |
|     25 | 5877 | `	if( !v4 && !v6 ){ v4 = v6 = 1; } /* default accepts either family */` |
|     25 | 5878 | `	if( v4 && FvValidateIp4(z,n) ){ return 1; }` |
|     21 | 5879 | `	if( v6 && FvValidateIp6(z,n) ){ return 1; }` |
|     13 | 5880 | `	return 0;` |
|     13 | 5881 | `}` |
|      - | 5882 | `/* FILTER_VALIDATE_MAC: 17-char colon- or dash-separated hex (XX:XX:..:XX). */` |
|     11 | 5883 | `static int FvValidateMac(const char *z,int n){` |
|      - | 5884 | `	char sep;` |
|      - | 5885 | `	int i;` |
|     11 | 5886 | `	if( n!=17 ){ return 0; }` |
|      7 | 5887 | `	sep = z[2];` |
|      7 | 5888 | `	if( sep!=':' && sep!='-' ){ return 0; }` |
|    105 | 5889 | `	for( i=0; i<17; i++ ){` |
|    101 | 5890 | `		if( (i%3)==2 ){ if( z[i]!=sep ){ return 0; } }` |
|     71 | 5891 | `		else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }` |
|     50 | 5892 | `	}` |
|      5 | 5893 | `	return 1;` |
|      6 | 5894 | `}` |
|      - | 5895 | `/* FILTER_VALIDATE_EMAIL (best-effort: covers the common cases, not quoted local` |
|      - | 5896 | ` * parts or IP-literal domains). */` |
|     28 | 5897 | `static int FvValidateEmail(const char *z,int n){` |
|     28 | 5898 | `	int at = -1, i, localLen, domLen, labelStart, dotCount = 0;` |
|      - | 5899 | `	const char *zDom;` |
|     28 | 5900 | `	if( n==0 \|\| n>320 ){ return 0; }` |
|    201 | 5901 | `	for( i=0; i<n; i++ ){` |
|    181 | 5902 | `		if( z[i]=='@' ){ if( at>=0 ){ return 0; } at = i; }` |
|     91 | 5903 | `	}` |
|     21 | 5904 | `	if( at<=0 \|\| at==n-1 ){ return 0; } /* one '@', non-empty local and domain */` |
|     21 | 5905 | `	localLen = at;` |
|     21 | 5906 | `	zDom = z + at + 1;` |
|     21 | 5907 | `	domLen = n - at - 1;` |
|     21 | 5908 | `	if( z[0]=='.' \|\| z[at-1]=='.' ){ return 0; }` |
|     57 | 5909 | `	for( i=0; i<localLen; i++ ){` |
|     43 | 5910 | `		unsigned char c = (unsigned char)z[i];` |
|     43 | 5911 | `		if( c<=' ' ){ return 0; }` |
|     41 | 5912 | `		if( c=='.' && i+1<localLen && z[i+1]=='.' ){ return 0; }` |
|     20 | 5913 | `	}` |
|     15 | 5914 | `	if( zDom[0]=='.' \|\| zDom[domLen-1]=='.' ){ return 0; }` |
|     13 | 5915 | `	labelStart = 0;` |
|     85 | 5916 | `	for( i=0; i<=domLen; i++ ){` |
|     75 | 5917 | `		if( i==domLen \|\| zDom[i]=='.' ){` |
|     25 | 5918 | `			int ll = i - labelStart;` |
|     25 | 5919 | `			if( ll==0 ){ return 0; } /* consecutive dots */` |
|     23 | 5920 | `			if( zDom[labelStart]=='-' \|\| zDom[i-1]=='-' ){ return 0; }` |
|     23 | 5921 | `			if( i<domLen ){ dotCount++; }` |
|     23 | 5922 | `			labelStart = i+1;` |
|     12 | 5923 | `		}else{` |
|     51 | 5924 | `			unsigned char c = (unsigned char)zDom[i];` |
|     51 | 5925 | `			if( !((c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9')\|\|c=='-') ){ return 0; }` |
|      - | 5926 | `		}` |
|     37 | 5927 | `	}` |
|     11 | 5928 | `	if( dotCount<1 ){ return 0; } /* PHP requires a dot in the domain (any TLD length) */` |
|      9 | 5929 | `	return 1;` |
|     15 | 5930 | `}` |
|      - | 5931 | `/* FILTER_VALIDATE_DOMAIN (lenient, matching PHP without FILTER_FLAG_HOSTNAME). */` |
|     11 | 5932 | `static int FvValidateDomain(const char *z,int n){` |
|      - | 5933 | `	int i;` |
|     11 | 5934 | `	if( n<1 \|\| n>253 \|\| z[0]=='.' ){ return 0; }` |
|     81 | 5935 | `	for( i=0; i<n; i++ ){` |
|     75 | 5936 | `		unsigned char c = (unsigned char)z[i];` |
|     75 | 5937 | `		if( c<=' ' ){ return 0; }` |
|     75 | 5938 | `		if( c=='.' && i+1<n && z[i+1]=='.' ){ return 0; }` |
|     37 | 5939 | `	}` |
|      7 | 5940 | `	return 1;` |
|      6 | 5941 | `}` |
|      - | 5942 | `/* FILTER_VALIDATE_URL: require a scheme and a host (PHP's filter is itself` |
|      - | 5943 | ` * parse_url-based, so PH7_VmHttpSplitURI tracks it closely). */` |
|     15 | 5944 | `static int FvValidateUrl(const char *z,int n){` |
|      - | 5945 | `	SyhttpUri sUri;` |
|     15 | 5946 | `	if( n==0 ){ return 0; }` |
|     15 | 5947 | `	SyZero(&sUri,(sxu32)sizeof(sUri));` |
|     15 | 5948 | `	if( PH7_VmHttpSplitURI(&sUri,z,(sxu32)n)!=SXRET_OK ){ return 0; }` |
|     15 | 5949 | `	return sUri.sScheme.nByte!=0 && sUri.sHost.nByte!=0;` |
|      8 | 5950 | `}` |
|      - | 5951 | `/* The Fv sanitizers build their result by appending directly to the call` |
|      - | 5952 | ` * context (ph7_result_string accumulates, like htmlspecialchars), emitting each` |
|      - | 5953 | ` * kept run in one call and seeding "" so an all-stripped input yields "". */` |
|      - | 5954 | `/* SANITIZE_NUMBER_INT (isFloat=0) / SANITIZE_NUMBER_FLOAT (isFloat=1). */` |
|     37 | 5955 | `static void FvSanitizeNumber(ph7_context *pCtx,const char *z,int n,int isFloat,int flags){` |
|     37 | 5956 | `	int i, runStart = 0;` |
|     37 | 5957 | `	ph7_result_string(pCtx,"",0);` |
|     97 | 5958 | `	for( i=0; i<n; i++ ){` |
|     91 | 5959 | `		char c = z[i];` |
|     91 | 5960 | `		int keep = (c>='0'&&c<='9') \|\| c=='+' \|\| c=='-';` |
|     91 | 5961 | `		if( !keep && isFloat ){` |
|     38 | 5962 | `			keep = (c=='.' && (flags & FV_FLAG_ALLOW_FRACTION))` |
|     23 | 5963 | `			    \|\| (c==',' && (flags & FV_FLAG_ALLOW_THOUSAND))` |
|     36 | 5964 | `			    \|\| ((c=='e'\|\|c=='E') && (flags & FV_FLAG_ALLOW_SCIENTIFIC));` |
|     12 | 5965 | `		}` |
|     61 | 5966 | `		if( !keep ){` |
|     33 | 5967 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     33 | 5968 | `			runStart = i+1;` |
|     16 | 5969 | `		}` |
|     31 | 5970 | `	}` |
|      7 | 5971 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 | 5972 | `}` |
|      - | 5973 | `/* Return non-zero when byte c must be stripped under the STRIP_* flags. Shared` |
|      - | 5974 | ` * by the UNSAFE_RAW string filter and SANITIZE_SPECIAL_CHARS. STRIP_LOW drops` |
|      - | 5975 | `` * bytes <32, STRIP_HIGH drops bytes >=127 (incl. DEL), STRIP_BACKTICK drops '`'.`` |
|      - | 5976 | ` * Matches php_filter_strip(); verified byte-exact vs php 8.5.7. */` |
|    287 | 5977 | `static int FvStripByte(unsigned char c,int flags){` |
|    287 | 5978 | `	if( (flags & FV_FLAG_STRIP_LOW)      && c<32 )    { return 1; }` |
|    281 | 5979 | `	if( (flags & FV_FLAG_STRIP_HIGH)     && c>=127 )  { return 1; }` |
|    269 | 5980 | `	if( (flags & FV_FLAG_STRIP_BACKTICK) && c==0x60 ) { return 1; }` |
|    267 | 5981 | `	return 0;` |
|    144 | 5982 | `}` |
|      - | 5983 | `/* FILTER_UNSAFE_RAW / FILTER_DEFAULT with flags: no default transform, but the` |
|      - | 5984 | ` * STRIP/ENCODE flags apply. Precedence (per php_filter_unsafe_raw, verified` |
|      - | 5985 | ` * vs php 8.5.7): a byte is first tested for stripping; a surviving byte is then` |
|      - | 5986 | ` * encoded as a decimal numeric entity if ENCODE_LOW (<32) / ENCODE_HIGH (>=127)` |
|      - | 5987 | ` * is set, and '&' becomes "&#38;" under ENCODE_AMP. So STRIP_LOW\|ENCODE_LOW` |
|      - | 5988 | ` * strips (nothing left to encode). Bytes are treated individually — ENCODE_HIGH` |
|      - | 5989 | ` * numeric-encodes each byte of a multibyte sequence separately, not the codepoint. */` |
|     25 | 5990 | `static void FvSanitizeString(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 5991 | `	int i, runStart = 0;` |
|     25 | 5992 | `	ph7_result_string(pCtx,"",0);` |
|    193 | 5993 | `	for( i=0; i<n; i++ ){` |
|    179 | 5994 | `		unsigned char c = (unsigned char)z[i];` |
|    179 | 5995 | `		if( FvStripByte(c,flags) ){` |
|     13 | 5996 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     13 | 5997 | `			runStart = i+1;` |
|     13 | 5998 | `			continue;` |
|      - | 5999 | `		}` |
|    167 | 6000 | `		if( c=='&' && (flags & FV_FLAG_ENCODE_AMP) ){` |
|      3 | 6001 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      3 | 6002 | `			ph7_result_string(pCtx,"&#38;",-1);` |
|      3 | 6003 | `			runStart = i+1;` |
|    166 | 6004 | `		}else if( (c<32 && (flags & FV_FLAG_ENCODE_LOW))` |
|    164 | 6005 | `		       \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     37 | 6006 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 6007 | `			ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|      9 | 6008 | `			runStart = i+1;` |
|      4 | 6009 | `		}` |
|     79 | 6010 | `	}` |
|     15 | 6011 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     15 | 6012 | `}` |
|      - | 6013 | `/* FILTER_SANITIZE_SPECIAL_CHARS: encode <>&"' and every control byte <32 as a` |
|      - | 6014 | ` * decimal numeric entity (&#60; &#38; &#34; ...). The STRIP_* flags remove bytes` |
|      - | 6015 | ` * before encoding; ENCODE_HIGH numeric-encodes surviving bytes >=127. Bytes >=128` |
|      - | 6016 | ` * are otherwise passed through verbatim (this filter is NOT UTF-8-aware — only the` |
|      - | 6017 | ` * FULL variant is). Byte-exact vs php 8.5.7. */` |
|     13 | 6018 | `static void FvSanitizeSpecial(ph7_context *pCtx,const char *z,int n,int flags){` |
|     13 | 6019 | `	int i, runStart = 0;` |
|      - | 6020 | `	const char *zEnt;` |
|     13 | 6021 | `	ph7_result_string(pCtx,"",0);` |
|    131 | 6022 | `	for( i=0; i<n; i++ ){` |
|    119 | 6023 | `		unsigned char c = (unsigned char)z[i];` |
|    119 | 6024 | `		if( FvStripByte(c,flags) ){` |
|      9 | 6025 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 6026 | `			runStart = i+1;` |
|      9 | 6027 | `			continue;` |
|      - | 6028 | `		}` |
|    111 | 6029 | `		switch( c ){` |
|      3 | 6030 | `		case '<':  zEnt = "&#60;"; break;` |
|      3 | 6031 | `		case '>':  zEnt = "&#62;"; break;` |
|     11 | 6032 | `		case '&':  zEnt = "&#38;"; break;` |
|      3 | 6033 | `		case '"':  zEnt = "&#34;"; break;` |
|      3 | 6034 | `		case '\'': zEnt = "&#39;"; break;` |
|     46 | 6035 | `		default:` |
|      - | 6036 | `			/* Control bytes <32 are always numeric-encoded; bytes >=127 only when` |
|      - | 6037 | `			 * ENCODE_HIGH is set. Everything else stays in the current run. */` |
|     93 | 6038 | `			if( c<32 \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     17 | 6039 | `				if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     17 | 6040 | `				ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|     17 | 6041 | `				runStart = i+1;` |
|      8 | 6042 | `			}` |
|     93 | 6043 | `			continue; /* keep in the current run */` |
|      - | 6044 | `		}` |
|     19 | 6045 | `		if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     19 | 6046 | `		ph7_result_string(pCtx,zEnt,-1); /* -1: length from strlen */` |
|     19 | 6047 | `		runStart = i+1;` |
|     10 | 6048 | `	}` |
|     13 | 6049 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     13 | 6050 | `}` |
|      - | 6051 | `/* HTML 4.01 named-entity table (codepoint -> "&name;") used by the UTF-8-aware` |
|      - | 6052 | ` * FULL_SPECIAL_CHARS filter, sorted ascending by codepoint for binary search.` |
|      - | 6053 | ` * Generated from php 8.5.7 (the exact set php_escape_html_entities emits for the` |
|      - | 6054 | ` * default document type); the five inline specials <>&"' are handled separately,` |
|      - | 6055 | ` * so every entry here is a codepoint >=0xA0. 248 rows. */` |
|      - | 6056 | `static const struct { sxu32 cp; const char *zEnt; } aHtml401Ent[] = {` |
|      - | 6057 | `	{0x00A0,"&nbsp;"},{0x00A1,"&iexcl;"},{0x00A2,"&cent;"},{0x00A3,"&pound;"},` |
|      - | 6058 | `	{0x00A4,"&curren;"},{0x00A5,"&yen;"},{0x00A6,"&brvbar;"},{0x00A7,"&sect;"},` |
|      - | 6059 | `	{0x00A8,"&uml;"},{0x00A9,"&copy;"},{0x00AA,"&ordf;"},{0x00AB,"&laquo;"},` |
|      - | 6060 | `	{0x00AC,"&not;"},{0x00AD,"&shy;"},{0x00AE,"&reg;"},{0x00AF,"&macr;"},` |
|      - | 6061 | `	{0x00B0,"&deg;"},{0x00B1,"&plusmn;"},{0x00B2,"&sup2;"},{0x00B3,"&sup3;"},` |
|      - | 6062 | `	{0x00B4,"&acute;"},{0x00B5,"&micro;"},{0x00B6,"&para;"},{0x00B7,"&middot;"},` |
|      - | 6063 | `	{0x00B8,"&cedil;"},{0x00B9,"&sup1;"},{0x00BA,"&ordm;"},{0x00BB,"&raquo;"},` |
|      - | 6064 | `	{0x00BC,"&frac14;"},{0x00BD,"&frac12;"},{0x00BE,"&frac34;"},{0x00BF,"&iquest;"},` |
|      - | 6065 | `	{0x00C0,"&Agrave;"},{0x00C1,"&Aacute;"},{0x00C2,"&Acirc;"},{0x00C3,"&Atilde;"},` |
|      - | 6066 | `	{0x00C4,"&Auml;"},{0x00C5,"&Aring;"},{0x00C6,"&AElig;"},{0x00C7,"&Ccedil;"},` |
|      - | 6067 | `	{0x00C8,"&Egrave;"},{0x00C9,"&Eacute;"},{0x00CA,"&Ecirc;"},{0x00CB,"&Euml;"},` |
|      - | 6068 | `	{0x00CC,"&Igrave;"},{0x00CD,"&Iacute;"},{0x00CE,"&Icirc;"},{0x00CF,"&Iuml;"},` |
|      - | 6069 | `	{0x00D0,"&ETH;"},{0x00D1,"&Ntilde;"},{0x00D2,"&Ograve;"},{0x00D3,"&Oacute;"},` |
|      - | 6070 | `	{0x00D4,"&Ocirc;"},{0x00D5,"&Otilde;"},{0x00D6,"&Ouml;"},{0x00D7,"&times;"},` |
|      - | 6071 | `	{0x00D8,"&Oslash;"},{0x00D9,"&Ugrave;"},{0x00DA,"&Uacute;"},{0x00DB,"&Ucirc;"},` |
|      - | 6072 | `	{0x00DC,"&Uuml;"},{0x00DD,"&Yacute;"},{0x00DE,"&THORN;"},{0x00DF,"&szlig;"},` |
|      - | 6073 | `	{0x00E0,"&agrave;"},{0x00E1,"&aacute;"},{0x00E2,"&acirc;"},{0x00E3,"&atilde;"},` |
|      - | 6074 | `	{0x00E4,"&auml;"},{0x00E5,"&aring;"},{0x00E6,"&aelig;"},{0x00E7,"&ccedil;"},` |
|      - | 6075 | `	{0x00E8,"&egrave;"},{0x00E9,"&eacute;"},{0x00EA,"&ecirc;"},{0x00EB,"&euml;"},` |
|      - | 6076 | `	{0x00EC,"&igrave;"},{0x00ED,"&iacute;"},{0x00EE,"&icirc;"},{0x00EF,"&iuml;"},` |
|      - | 6077 | `	{0x00F0,"&eth;"},{0x00F1,"&ntilde;"},{0x00F2,"&ograve;"},{0x00F3,"&oacute;"},` |
|      - | 6078 | `	{0x00F4,"&ocirc;"},{0x00F5,"&otilde;"},{0x00F6,"&ouml;"},{0x00F7,"&divide;"},` |
|      - | 6079 | `	{0x00F8,"&oslash;"},{0x00F9,"&ugrave;"},{0x00FA,"&uacute;"},{0x00FB,"&ucirc;"},` |
|      - | 6080 | `	{0x00FC,"&uuml;"},{0x00FD,"&yacute;"},{0x00FE,"&thorn;"},{0x00FF,"&yuml;"},` |
|      - | 6081 | `	{0x0152,"&OElig;"},{0x0153,"&oelig;"},{0x0160,"&Scaron;"},{0x0161,"&scaron;"},` |
|      - | 6082 | `	{0x0178,"&Yuml;"},{0x0192,"&fnof;"},{0x02C6,"&circ;"},{0x02DC,"&tilde;"},` |
|      - | 6083 | `	{0x0391,"&Alpha;"},{0x0392,"&Beta;"},{0x0393,"&Gamma;"},{0x0394,"&Delta;"},` |
|      - | 6084 | `	{0x0395,"&Epsilon;"},{0x0396,"&Zeta;"},{0x0397,"&Eta;"},{0x0398,"&Theta;"},` |
|      - | 6085 | `	{0x0399,"&Iota;"},{0x039A,"&Kappa;"},{0x039B,"&Lambda;"},{0x039C,"&Mu;"},` |
|      - | 6086 | `	{0x039D,"&Nu;"},{0x039E,"&Xi;"},{0x039F,"&Omicron;"},{0x03A0,"&Pi;"},` |
|      - | 6087 | `	{0x03A1,"&Rho;"},{0x03A3,"&Sigma;"},{0x03A4,"&Tau;"},{0x03A5,"&Upsilon;"},` |
|      - | 6088 | `	{0x03A6,"&Phi;"},{0x03A7,"&Chi;"},{0x03A8,"&Psi;"},{0x03A9,"&Omega;"},` |
|      - | 6089 | `	{0x03B1,"&alpha;"},{0x03B2,"&beta;"},{0x03B3,"&gamma;"},{0x03B4,"&delta;"},` |
|      - | 6090 | `	{0x03B5,"&epsilon;"},{0x03B6,"&zeta;"},{0x03B7,"&eta;"},{0x03B8,"&theta;"},` |
|      - | 6091 | `	{0x03B9,"&iota;"},{0x03BA,"&kappa;"},{0x03BB,"&lambda;"},{0x03BC,"&mu;"},` |
|      - | 6092 | `	{0x03BD,"&nu;"},{0x03BE,"&xi;"},{0x03BF,"&omicron;"},{0x03C0,"&pi;"},` |
|      - | 6093 | `	{0x03C1,"&rho;"},{0x03C2,"&sigmaf;"},{0x03C3,"&sigma;"},{0x03C4,"&tau;"},` |
|      - | 6094 | `	{0x03C5,"&upsilon;"},{0x03C6,"&phi;"},{0x03C7,"&chi;"},{0x03C8,"&psi;"},` |
|      - | 6095 | `	{0x03C9,"&omega;"},{0x03D1,"&thetasym;"},{0x03D2,"&upsih;"},{0x03D6,"&piv;"},` |
|      - | 6096 | `	{0x2002,"&ensp;"},{0x2003,"&emsp;"},{0x2009,"&thinsp;"},{0x200C,"&zwnj;"},` |
|      - | 6097 | `	{0x200D,"&zwj;"},{0x200E,"&lrm;"},{0x200F,"&rlm;"},{0x2013,"&ndash;"},` |
|      - | 6098 | `	{0x2014,"&mdash;"},{0x2018,"&lsquo;"},{0x2019,"&rsquo;"},{0x201A,"&sbquo;"},` |
|      - | 6099 | `	{0x201C,"&ldquo;"},{0x201D,"&rdquo;"},{0x201E,"&bdquo;"},{0x2020,"&dagger;"},` |
|      - | 6100 | `	{0x2021,"&Dagger;"},{0x2022,"&bull;"},{0x2026,"&hellip;"},{0x2030,"&permil;"},` |
|      - | 6101 | `	{0x2032,"&prime;"},{0x2033,"&Prime;"},{0x2039,"&lsaquo;"},{0x203A,"&rsaquo;"},` |
|      - | 6102 | `	{0x203E,"&oline;"},{0x2044,"&frasl;"},{0x20AC,"&euro;"},{0x2111,"&image;"},` |
|      - | 6103 | `	{0x2118,"&weierp;"},{0x211C,"&real;"},{0x2122,"&trade;"},{0x2135,"&alefsym;"},` |
|      - | 6104 | `	{0x2190,"&larr;"},{0x2191,"&uarr;"},{0x2192,"&rarr;"},{0x2193,"&darr;"},` |
|      - | 6105 | `	{0x2194,"&harr;"},{0x21B5,"&crarr;"},{0x21D0,"&lArr;"},{0x21D1,"&uArr;"},` |
|      - | 6106 | `	{0x21D2,"&rArr;"},{0x21D3,"&dArr;"},{0x21D4,"&hArr;"},{0x2200,"&forall;"},` |
|      - | 6107 | `	{0x2202,"&part;"},{0x2203,"&exist;"},{0x2205,"&empty;"},{0x2207,"&nabla;"},` |
|      - | 6108 | `	{0x2208,"&isin;"},{0x2209,"&notin;"},{0x220B,"&ni;"},{0x220F,"&prod;"},` |
|      - | 6109 | `	{0x2211,"&sum;"},{0x2212,"&minus;"},{0x2217,"&lowast;"},{0x221A,"&radic;"},` |
|      - | 6110 | `	{0x221D,"&prop;"},{0x221E,"&infin;"},{0x2220,"&ang;"},{0x2227,"&and;"},` |
|      - | 6111 | `	{0x2228,"&or;"},{0x2229,"&cap;"},{0x222A,"&cup;"},{0x222B,"&int;"},` |
|      - | 6112 | `	{0x2234,"&there4;"},{0x223C,"&sim;"},{0x2245,"&cong;"},{0x2248,"&asymp;"},` |
|      - | 6113 | `	{0x2260,"&ne;"},{0x2261,"&equiv;"},{0x2264,"&le;"},{0x2265,"&ge;"},` |
|      - | 6114 | `	{0x2282,"&sub;"},{0x2283,"&sup;"},{0x2284,"&nsub;"},{0x2286,"&sube;"},` |
|      - | 6115 | `	{0x2287,"&supe;"},{0x2295,"&oplus;"},{0x2297,"&otimes;"},{0x22A5,"&perp;"},` |
|      - | 6116 | `	{0x22C5,"&sdot;"},{0x2308,"&lceil;"},{0x2309,"&rceil;"},{0x230A,"&lfloor;"},` |
|      - | 6117 | `	{0x230B,"&rfloor;"},{0x2329,"&lang;"},{0x232A,"&rang;"},{0x25CA,"&loz;"},` |
|      - | 6118 | `	{0x2660,"&spades;"},{0x2663,"&clubs;"},{0x2665,"&hearts;"},{0x2666,"&diams;"}` |
|      - | 6119 | `};` |
|      - | 6120 | `/* Binary-search aHtml401Ent[] for cp; return its "&name;" entity or 0. */` |
|     41 | 6121 | `static const char *FvHtml401Lookup(sxu32 cp){` |
|     41 | 6122 | `	int lo = 0, hi = (int)SX_ARRAYSIZE(aHtml401Ent) - 1;` |
|    323 | 6123 | `	while( lo <= hi ){` |
|    309 | 6124 | `		int mid = (lo + hi) / 2;` |
|    309 | 6125 | `		sxu32 c = aHtml401Ent[mid].cp;` |
|    309 | 6126 | `		if( c == cp ){ return aHtml401Ent[mid].zEnt; }` |
|    283 | 6127 | `		if( c < cp ){ lo = mid + 1; } else { hi = mid - 1; }` |
|      1 | 6128 | `	}` |
|     15 | 6129 | `	return 0;` |
|     21 | 6130 | `}` |
|      - | 6131 | `/* Decode one strict-UTF-8 sequence at p (< zEnd). On success returns its byte` |
|      - | 6132 | ` * length (1..4) and sets *pCp to the codepoint; on any malformed, overlong,` |
|      - | 6133 | ` * surrogate, truncated or out-of-range (>U+10FFFF) sequence returns 0. Matches` |
|      - | 6134 | ` * PHP's UTF-8 validation used by FULL_SPECIAL_CHARS (verified vs php 8.5.7). */` |
|    101 | 6135 | `static int FvUtf8Next(const unsigned char *p,const unsigned char *zEnd,sxu32 *pCp){` |
|    101 | 6136 | `	unsigned char c = p[0];` |
|    101 | 6137 | `	if( c < 0x80 ){ *pCp = c; return 1; }` |
|    101 | 6138 | `	if( c < 0xC2 ){ return 0; }              /* 0x80-0xBF stray cont / 0xC0-0xC1 overlong */` |
|     99 | 6139 | `	if( c < 0xE0 ){                          /* 2-byte: U+0080..U+07FF */` |
|     47 | 6140 | `		if( zEnd-p < 2 \|\| (p[1]&0xC0)!=0x80 ){ return 0; }` |
|     45 | 6141 | `		*pCp = ((sxu32)(c&0x1F)<<6) \| (p[1]&0x3F);` |
|     45 | 6142 | `		return 2;` |
|      - | 6143 | `	}` |
|     53 | 6144 | `	if( c < 0xF0 ){                          /* 3-byte: U+0800..U+FFFF minus surrogates */` |
|      - | 6145 | `		sxu32 cp;` |
|     47 | 6146 | `		if( zEnd-p < 3 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 ){ return 0; }` |
|     33 | 6147 | `		cp = ((sxu32)(c&0x0F)<<12) \| ((sxu32)(p[1]&0x3F)<<6) \| (p[2]&0x3F);` |
|     33 | 6148 | `		if( cp < 0x800 \|\| (cp>=0xD800 && cp<=0xDFFF) ){ return 0; }` |
|     29 | 6149 | `		*pCp = cp;` |
|     29 | 6150 | `		return 3;` |
|      - | 6151 | `	}` |
|      7 | 6152 | `	if( c < 0xF5 ){                          /* 4-byte: U+10000..U+10FFFF */` |
|      - | 6153 | `		sxu32 cp;` |
|      5 | 6154 | `		if( zEnd-p < 4 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 \|\| (p[3]&0xC0)!=0x80 ){ return 0; }` |
|      5 | 6155 | `		cp = ((sxu32)(c&0x07)<<18) \| ((sxu32)(p[1]&0x3F)<<12) \| ((sxu32)(p[2]&0x3F)<<6) \| (p[3]&0x3F);` |
|      5 | 6156 | `		if( cp < 0x10000 \|\| cp > 0x10FFFF ){ return 0; }` |
|      5 | 6157 | `		*pCp = cp;` |
|      5 | 6158 | `		return 4;` |
|      - | 6159 | `	}` |
|      3 | 6160 | `	return 0;                                /* 0xF5-0xFF */` |
|     51 | 6161 | `}` |
|      - | 6162 | `/* FILTER_SANITIZE_FULL_SPECIAL_CHARS: htmlentities-style, UTF-8-aware. Encodes` |
|      - | 6163 | ` * <>&"' as named entities ("'" -> &#039;; quotes suppressed under NO_ENCODE_QUOTES),` |
|      - | 6164 | ` * and every valid UTF-8 codepoint with an HTML 4.01 named entity as that entity;` |
|      - | 6165 | ` * valid codepoints without a named entity (and low control bytes) pass through` |
|      - | 6166 | ` * verbatim. If the input contains ANY invalid UTF-8 the whole result is "".` |
|      - | 6167 | ` * The STRIP/ENCODE flags do NOT apply to this filter (only NO_ENCODE_QUOTES).` |
|      - | 6168 | ` * php's filter does NOT re-encode valid pre-existing entities ("&amp;" stays,` |
|      - | 6169 | ` * "&bogus;" becomes "&amp;bogus;"), i.e. double_encode=false semantics —` |
|      - | 6170 | ` * exactly htmlentities(ENT_QUOTES\|ENT_HTML401, double_encode: false), so this` |
|      - | 6171 | ` * delegates to the shared encoder. Byte-exact vs php 8.5.7. */` |
|     25 | 6172 | `static void FvSanitizeFull(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 6173 | `	int iEntFlags = (flags & FV_FLAG_NO_ENCODE_QUOTES) ? 0 : PH7_ENT_QUOTES;` |
|     25 | 6174 | `	HtmlEscape(pCtx,z,n,iEntFlags,1/*bAll*/,0/*bDoubleEncode*/);` |
|     25 | 6175 | `}` |
|      - | 6176 | `/* ---------------------------------------------------------------------------` |
|      - | 6177 | ` * UTF-8-aware HTML entity core (htmlspecialchars/htmlentities family).` |
|      - | 6178 | ` * Prototyped next to the five builtins earlier in this file; lives here so it` |
|      - | 6179 | ` * can share aHtml401Ent[]/FvHtml401Lookup()/FvUtf8Next() with the filter_var` |
|      - | 6180 | ` * FULL_SPECIAL_CHARS filter above. Byte-exact vs php 8.5.7 (oracle-swept).` |
|      - | 6181 | ` * ------------------------------------------------------------------------ */` |
|      - | 6182 | `/* Encode cp as UTF-8 into zBuf (>= 4 bytes); return the byte length 1..4.` |
|      - | 6183 | ` * Thin wrapper over the engine-wide SX_WRITE_UTF8 (sxmacros.h). */` |
|    585 | 6184 | `static int HtmlCpUtf8(sxu32 cp,char *zBuf){` |
|    585 | 6185 | `	sxu8 *z = (sxu8 *)zBuf;` |
|    585 | 6186 | `	SX_WRITE_UTF8(z,cp);` |
|    585 | 6187 | `	return (int)(z - (sxu8 *)zBuf);` |
|      1 | 6188 | `}` |
|      - | 6189 | `/* Doctype-allowed codepoint test (php's unicode_cp_is_allowed) — gates what a` |
|      - | 6190 | ` * numeric reference may DECODE to. Oracle-pinned per doctype: HTML401` |
|      - | 6191 | ` * disallows C0 (except TAB/LF/CR) and DEL..U+009F; XML1 and XHTML share the` |
|      - | 6192 | ` * XML rules — DEL..U+009F allowed, U+FFFE/U+FFFF excluded; HTML5 swaps CR` |
|      - | 6193 | ` * for FF (0x0C) and excludes the noncharacters (U+FDD0..U+FDEF and every` |
|      - | 6194 | ` * U+xFFFE/U+xFFFF). Surrogates are disallowed everywhere. */` |
|     91 | 6195 | `static int HtmlCpAllowed(sxu32 cp,int iFlags){` |
|     91 | 6196 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|     91 | 6197 | `	if( cp==0x09 \|\| cp==0x0A ){ return 1; }` |
|     87 | 6198 | `	if( cp==0x0D ){ return iDoc != PH7_ENT_DOC_HTML5; }` |
|     85 | 6199 | `	if( cp==0x0C ){ return iDoc == PH7_ENT_DOC_HTML5; }` |
|     85 | 6200 | `	if( cp < 0x20 \|\| cp > 0x10FFFF ){ return 0; }` |
|     79 | 6201 | `	if( cp>=0xD800 && cp<=0xDFFF ){ return 0; }` |
|     77 | 6202 | `	if( cp>=0x7F && cp<=0x9F ){ return iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML; }` |
|     71 | 6203 | `	if( iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML ){` |
|    ! 0 | 6204 | `		return cp!=0xFFFE && cp!=0xFFFF;` |
|      - | 6205 | `	}` |
|     71 | 6206 | `	if( iDoc == PH7_ENT_DOC_HTML5 ){` |
|      9 | 6207 | `		if( cp>=0xFDD0 && cp<=0xFDEF ){ return 0; }` |
|      9 | 6208 | `		if( (cp & 0xFFFF) >= 0xFFFE ){ return 0; }` |
|      4 | 6209 | `	}` |
|     71 | 6210 | `	return 1;` |
|     46 | 6211 | `}` |
|      - | 6212 | `/* The ENT_DISALLOWED gate for RAW characters on the ENCODE side. Same as the` |
|      - | 6213 | ` * decode gate except CR under HTML5: php's encode-side unicode_cp_is_allowed` |
|      - | 6214 | ` * keeps a literal "\r" verbatim under ENT_HTML5\|ENT_DISALLOWED while the` |
|      - | 6215 | ` * decode side leaves "&#13;" un-decoded (oracle-pinned at flags 176). */` |
|      9 | 6216 | `static int HtmlCpAllowedEncode(sxu32 cp,int iFlags){` |
|      9 | 6217 | `	if( cp==0x0D && (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 ){ return 1; }` |
|      9 | 6218 | `	return HtmlCpAllowed(cp,iFlags);` |
|      5 | 6219 | `}` |
|      - | 6220 | `/* Numeric-reference validity for the double_encode=false "is this already a` |
|      - | 6221 | ` * valid entity" test — a MUCH looser predicate than the decode gate above:` |
|      - | 6222 | ` * any codepoint <= U+10FFFF is valid (controls and surrogates included, every` |
|      - | 6223 | ` * doctype). ENT_DISALLOWED re-tightens non-HTML401 doctypes to the decode` |
|      - | 6224 | ` * gate, except that HTML5 exempts surrogates. All oracle-pinned: &#0; and` |
|      - | 6225 | ` * &#xD800; stay verbatim at flags 11 and 139; flags -1 (HTML5+DISALLOWED)` |
|      - | 6226 | ` * re-encodes &#0; and &#x10FFFF; but still keeps &#xD800;; flags 144` |
|      - | 6227 | ` * (XML1+DISALLOWED) re-encodes &#xD800;. */` |
|      9 | 6228 | `static int HtmlNumericAllowed(sxu32 cp,int iFlags){` |
|      9 | 6229 | `	if( cp > 0x10FFFF ){ return 0; }` |
|      7 | 6230 | `	if( (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML401 ){ return 1; /* never tightened */ }` |
|    ! 0 | 6231 | `	if( (iFlags & PH7_ENT_DISALLOWED)` |
|    ! 0 | 6232 | `	 && !((iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 && cp>=0xD800 && cp<=0xDFFF)` |
|    ! 0 | 6233 | `	 && !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|    ! 0 | 6234 | `	return 1;` |
|      5 | 6235 | `}` |
|      - | 6236 | `/* How many bytes the malformed UTF-8 sequence at p consumes — php's` |
|      - | 6237 | ` * get_next_char failure step (one U+FFFD substitution / one ENT_IGNORE drop` |
|      - | 6238 | ` * per MAXIMAL invalid subpart, not per byte): a prefix-valid sequence eats` |
|      - | 6239 | ` * its continuation bytes ("\xE0\x80\xAF" is ONE unit) while a byte that could` |
|      - | 6240 | ` * start a new sequence is left for the next round. */` |
|      5 | 6241 | `static int HtmlUtf8Trail(unsigned char c){ return c>=0x80 && c<=0xBF; }` |
|     11 | 6242 | `static int HtmlUtf8Lead(unsigned char c){ return c<0x80 \|\| (c>=0xC2 && c<=0xF4); }` |
|     15 | 6243 | `static int HtmlUtf8FailAdvance(const unsigned char *p,const unsigned char *zEnd){` |
|     15 | 6244 | `	unsigned char c = p[0];` |
|     15 | 6245 | `	int nAvail = (int)(zEnd - p);` |
|     15 | 6246 | `	if( c < 0xC2 \|\| c > 0xF4 ){ return 1; } /* stray trail / C0-C1 / F5-FF */` |
|     13 | 6247 | `	if( c < 0xE0 ){` |
|      3 | 6248 | `		if( nAvail < 2 ){ return 1; }` |
|      3 | 6249 | `		return HtmlUtf8Lead(p[1]) ? 1 : 2;` |
|      - | 6250 | `	}` |
|     11 | 6251 | `	if( c < 0xF0 ){` |
|     11 | 6252 | `		if( nAvail >= 3 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) ){` |
|      3 | 6253 | `			return 3; /* complete but overlong/surrogate */` |
|      - | 6254 | `		}` |
|      9 | 6255 | `		if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 6256 | `		if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 6257 | `		return 3;` |
|      - | 6258 | `	}` |
|    ! 0 | 6259 | `	if( nAvail >= 4 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) && HtmlUtf8Trail(p[3]) ){` |
|    ! 0 | 6260 | `		return 4; /* complete but overlong / > U+10FFFF */` |
|      - | 6261 | `	}` |
|    ! 0 | 6262 | `	if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 6263 | `	if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 6264 | `	if( nAvail < 4 \|\| HtmlUtf8Lead(p[3]) ){ return 3; }` |
|    ! 0 | 6265 | `	return 4;` |
|      8 | 6266 | `}` |
|      - | 6267 | `/* The basic special entities, shared by named matching, the hsc_decode` |
|      - | 6268 | ` * numeric whitelist and the translation-table builder so the sets can never` |
|      - | 6269 | ` * drift apart. (&apos; is not an HTML 4.01 entity — doctype-gated below.) */` |
|      - | 6270 | `static const struct { const char *zEnt; int n; sxu32 cp; } aHtmlSpecEnt[] = {` |
|      - | 6271 | `	{"&amp;",5,38},{"&lt;",4,60},{"&gt;",4,62},{"&quot;",6,34},{"&apos;",6,39}` |
|      - | 6272 | `};` |
|      - | 6273 | `/* Does this doctype consult the named-entity table (aHtml401Ent)? XML 1.0 has` |
|      - | 6274 | ` * no named entities beyond the specials; XHTML/HTML5 are approximated by the` |
|      - | 6275 | ` * HTML 4.01 table (documented divergence). */` |
|     63 | 6276 | `static int HtmlDocHasNamedTable(int iDoc){` |
|     63 | 6277 | `	return iDoc != PH7_ENT_DOC_XML1;` |
|      1 | 6278 | `}` |
|      - | 6279 | `/* The single-quote entity per doctype. Oracle-pinned asymmetry: for every` |
|      - | 6280 | ` * non-HTML401 doctype htmlspecialchars emits &apos; while htmlentities` |
|      - | 6281 | ` * (bEntities) keeps &#039; under XHTML too. The translation table mirrors` |
|      - | 6282 | ` * whichever function the requested table belongs to. */` |
|     29 | 6283 | `static const char *HtmlAposEntity(int iDoc,int bEntities){` |
|     29 | 6284 | `	if( iDoc == PH7_ENT_DOC_HTML401 \|\| (bEntities && iDoc == PH7_ENT_DOC_XHTML) ){` |
|     21 | 6285 | `		return "&#039;";` |
|      - | 6286 | `	}` |
|      9 | 6287 | `	return "&apos;";` |
|     15 | 6288 | `}` |
|      - | 6289 | `/* Try to parse one HTML entity at z (z[0]=='&', z < zEnd). bFull selects the` |
|      - | 6290 | ` * html_entity_decode set (doctype named table + any allowed numeric ref) vs` |
|      - | 6291 | ` * the htmlspecialchars_decode set (the basic specials + quote numerics only).` |
|      - | 6292 | ` * Named matching is case-SENSITIVE and the ';' is required (both PHP-exact);` |
|      - | 6293 | ` * numeric refs accept dec/hex (x or X) with any number of leading zeros but` |
|      - | 6294 | ` * reject out-of-range, surrogate and doctype-disallowed codepoints (the` |
|      - | 6295 | ` * caller then leaves the source verbatim). Quote-flag gating is NOT applied` |
|      - | 6296 | ` * here — the same routine doubles as the "is this a valid entity" test for` |
|      - | 6297 | ` * double_encode=false, which ignores the quote bits (oracle-pinned).` |
|      - | 6298 | ` * bEncodeCheck selects the looser HtmlNumericAllowed predicate used by that` |
|      - | 6299 | ` * double_encode test; decode callers pass 0 for the HtmlCpAllowed gate.` |
|      - | 6300 | ` * On success sets *pCp / *pnConsumed and returns 1. */` |
|    172 | 6301 | `static int HtmlParseEntity(const unsigned char *z,const unsigned char *zEnd,` |
|      1 | 6302 | `                           int iFlags,int bFull,int bEncodeCheck,sxu32 *pCp,int *pnConsumed){` |
|    173 | 6303 | `	int nAvail = (int)(zEnd - z);` |
|    173 | 6304 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6305 | `	sxu32 n;` |
|    173 | 6306 | `	if( nAvail < 4 ){ return 0; } /* shortest entities: &lt; &#9; */` |
|    169 | 6307 | `	if( z[1] == '#' ){` |
|      - | 6308 | `		/* Numeric reference */` |
|     89 | 6309 | `		sxu32 cp = 0;` |
|     89 | 6310 | `		int i = 2, bHex = 0, nDig = 0;` |
|     89 | 6311 | `		if( z[i]=='x' \|\| z[i]=='X' ){ bHex = 1; i++; }` |
|    317 | 6312 | `		for( ; i < nAvail && z[i] != ';' ; i++ ){` |
|      - | 6313 | `			int v;` |
|    221 | 6314 | `			unsigned char c = z[i];` |
|    221 | 6315 | `			if( c>='0' && c<='9' ){ v = c - '0'; }` |
|     17 | 6316 | `			else if( bHex && c>='a' && c<='f' ){ v = c - 'a' + 10; }` |
|     17 | 6317 | `			else if( bHex && c>='A' && c<='F' ){ v = c - 'A' + 10; }` |
|    ! 0 | 6318 | `			else { return 0; }` |
|      - | 6319 | `			/* Stop accumulating once out of range (keeps validating the shape;` |
|      - | 6320 | `			 * max intermediate is 0x10FFFF*16+15, no sxu32 overflow). */` |
|    221 | 6321 | `			if( cp <= 0x10FFFF ){ cp = cp * (bHex ? 16 : 10) + (sxu32)v; }` |
|    221 | 6322 | `			nDig++;` |
|    111 | 6323 | `		}` |
|     97 | 6324 | `		if( nDig == 0 \|\| i >= nAvail ){ return 0; } /* no digits / no ';' */` |
|     97 | 6325 | `		if( bEncodeCheck ? !HtmlNumericAllowed(cp,iFlags) : !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|     83 | 6326 | `		if( !bFull ){` |
|      - | 6327 | `			/* hsc_decode: numeric refs to the five specials only. */` |
|     99 | 6328 | `			for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) && aHtmlSpecEnt[n].cp != cp ; n++ ){}` |
|     25 | 6329 | `			if( n >= SX_ARRAYSIZE(aHtmlSpecEnt) ){ return 0; }` |
|     11 | 6330 | `		}` |
|     75 | 6331 | `		*pCp = cp;` |
|     75 | 6332 | `		*pnConsumed = i + 1;` |
|     75 | 6333 | `		return 1;` |
|      - | 6334 | `	}` |
|      - | 6335 | `	/* Named reference — every entity name starts with a letter, so anything` |
|      - | 6336 | `	 * else can bail out before touching the tables. */` |
|     81 | 6337 | `	if( !((z[1]>='a' && z[1]<='z') \|\| (z[1]>='A' && z[1]<='Z')) ){ return 0; }` |
|    287 | 6338 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) ; n++ ){` |
|    265 | 6339 | `		if( aHtmlSpecEnt[n].cp == 39 && iDoc == PH7_ENT_DOC_HTML401 ){ continue; }` |
|    243 | 6340 | `		if( nAvail >= aHtmlSpecEnt[n].n && SyMemcmp(z,aHtmlSpecEnt[n].zEnt,(sxu32)aHtmlSpecEnt[n].n) == 0 ){` |
|     53 | 6341 | `			*pCp = aHtmlSpecEnt[n].cp;` |
|     53 | 6342 | `			*pnConsumed = aHtmlSpecEnt[n].n;` |
|     53 | 6343 | `			return 1;` |
|      - | 6344 | `		}` |
|     96 | 6345 | `	}` |
|     23 | 6346 | `	if( bFull && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 6347 | `		/* Linear scan of the 248-row table: runs only at '&'-then-letter` |
|      - | 6348 | `		 * positions and guarantees the decode set can never drift from the` |
|      - | 6349 | `		 * encode table. The first-letter guard skips the SyStrlen/SyMemcmp` |
|      - | 6350 | `		 * for ~96% of rows. */` |
|   3369 | 6351 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|      - | 6352 | `			sxu32 nEnt;` |
|   3357 | 6353 | `			if( z[1] != (unsigned char)aHtml401Ent[n].zEnt[1] ){ continue; }` |
|    121 | 6354 | `			nEnt = SyStrlen(aHtml401Ent[n].zEnt);` |
|    121 | 6355 | `			if( (sxu32)nAvail >= nEnt && SyMemcmp(z,aHtml401Ent[n].zEnt,nEnt) == 0 ){` |
|      7 | 6356 | `				*pCp = aHtml401Ent[n].cp;` |
|      7 | 6357 | `				*pnConsumed = (int)nEnt;` |
|      7 | 6358 | `				return 1;` |
|      - | 6359 | `			}` |
|     58 | 6360 | `		}` |
|      6 | 6361 | `	}` |
|     17 | 6362 | `	return 0;` |
|     88 | 6363 | `}` |
|      - | 6364 | `/* Shared encoder for htmlspecialchars (bAll=0) and htmlentities (bAll=1).` |
|      - | 6365 | ` * Invalid UTF-8 policy: ENT_IGNORE drops the byte (and wins over SUBSTITUTE),` |
|      - | 6366 | ` * ENT_SUBSTITUTE emits one U+FFFD per invalid byte, neither -> the whole` |
|      - | 6367 | ` * result is "" (pre-validated in a first pass: the accumulating result API` |
|      - | 6368 | ` * cannot roll back — same reason FvSanitizeFull is two-pass). */` |
|     96 | 6369 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 6370 | `                       int iFlags,int bAll,int bDoubleEncode){` |
|     97 | 6371 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     97 | 6372 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|      - | 6373 | `	const unsigned char *runStart;` |
|     97 | 6374 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6375 | `	sxu32 cp;` |
|     97 | 6376 | `	if( (iFlags & (PH7_ENT_IGNORE\|PH7_ENT_SUBSTITUTE)) == 0 ){` |
|      - | 6377 | `		/* Pass 1: any malformed sequence rejects the entire input. ASCII` |
|      - | 6378 | `		 * bytes cannot be malformed, so skip them without the decoder. */` |
|    381 | 6379 | `		while( p < zEnd ){` |
|      - | 6380 | `			int len;` |
|    323 | 6381 | `			if( *p < 0x80 ){ p++; continue; }` |
|     37 | 6382 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     37 | 6383 | `			if( len == 0 ){ ph7_result_string(pCtx,"",0); return; }` |
|     27 | 6384 | `			p += len;` |
|      1 | 6385 | `		}` |
|     59 | 6386 | `		p = (const unsigned char *)zIn;` |
|     29 | 6387 | `	}` |
|     87 | 6388 | `	runStart = p;` |
|     87 | 6389 | `	ph7_result_string(pCtx,"",0);` |
|    463 | 6390 | `	while( p < zEnd ){` |
|    377 | 6391 | `		const char *zEnt = 0;` |
|      - | 6392 | `		int len;` |
|    377 | 6393 | `		if( *p < 0x80 ){` |
|    313 | 6394 | `			len = 1;` |
|    313 | 6395 | `			switch( *p ){` |
|     25 | 6396 | `			case '<': zEnt = "&lt;"; break;` |
|     25 | 6397 | `			case '>': zEnt = "&gt;"; break;` |
|     18 | 6398 | `			case '&':` |
|     37 | 6399 | `				zEnt = "&amp;";` |
|     37 | 6400 | `				if( !bDoubleEncode ){` |
|      - | 6401 | `					sxu32 eCp; int nEat;` |
|     25 | 6402 | `					if( HtmlParseEntity(p,zEnd,iFlags,1,1,&eCp,&nEat) ){` |
|      - | 6403 | `						/* A valid existing entity: keep it verbatim. */` |
|     13 | 6404 | `						zEnt = 0;` |
|     13 | 6405 | `						len = nEat;` |
|      6 | 6406 | `					}` |
|     12 | 6407 | `				}` |
|     37 | 6408 | `				break;` |
|     10 | 6409 | `			case '"':` |
|     21 | 6410 | `				if( iFlags & PH7_ENT_QUOTE_DOUBLE ){ zEnt = "&quot;"; }` |
|     21 | 6411 | `				break;` |
|     12 | 6412 | `			case '\'':` |
|     25 | 6413 | `				if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|     23 | 6414 | `					zEnt = HtmlAposEntity(iDoc,bAll);` |
|     11 | 6415 | `				}` |
|     25 | 6416 | `				break;` |
|     92 | 6417 | `			default:` |
|    185 | 6418 | `				if( (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode((sxu32)*p,iFlags) ){` |
|    ! 0 | 6419 | `					zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 6420 | `				}` |
|    184 | 6421 | `				break;` |
|      - | 6422 | `			}` |
|    157 | 6423 | `		}else{` |
|     65 | 6424 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     65 | 6425 | `			if( len == 0 ){` |
|      - | 6426 | `				/* Malformed subpart (IGNORE or SUBSTITUTE is set, else pass 1` |
|      - | 6427 | `				 * would have rejected): drop it or emit ONE U+FFFD for the` |
|      - | 6428 | `				 * whole unit (php substitutes per maximal invalid subpart). */` |
|     15 | 6429 | `				if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|     15 | 6430 | `				if( (iFlags & PH7_ENT_IGNORE) == 0 ){ ph7_result_string(pCtx,"\xEF\xBF\xBD",3); }` |
|     15 | 6431 | `				p += HtmlUtf8FailAdvance(p,zEnd);` |
|     15 | 6432 | `				runStart = p;` |
|     15 | 6433 | `				continue;` |
|      - | 6434 | `			}` |
|     51 | 6435 | `			if( bAll && HtmlDocHasNamedTable(iDoc) ){` |
|     41 | 6436 | `				zEnt = FvHtml401Lookup(cp);` |
|     20 | 6437 | `			}` |
|     51 | 6438 | `			if( zEnt == 0 && (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode(cp,iFlags) ){` |
|    ! 0 | 6439 | `				zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 6440 | `			}` |
|      - | 6441 | `		}` |
|    363 | 6442 | `		if( zEnt ){` |
|    135 | 6443 | `			if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|    135 | 6444 | `			ph7_result_string(pCtx,zEnt,-1);` |
|    135 | 6445 | `			runStart = p + len;` |
|     67 | 6446 | `		}` |
|    363 | 6447 | `		p += len;` |
|      1 | 6448 | `	}` |
|     87 | 6449 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     49 | 6450 | `}` |
|      - | 6451 | `/* Shared decoder for html_entity_decode (bFull=1) and htmlspecialchars_decode` |
|      - | 6452 | ` * (bFull=0). Quote refs (cp 34/39, named or numeric) are gated by the quote` |
|      - | 6453 | ` * bits and left verbatim when suppressed; an invalid entity leaves its '&'` |
|      - | 6454 | ` * verbatim and rescans right after it, which also yields PHP's no-double-` |
|      - | 6455 | ` * decode behavior ("&amp;lt;" -> "&lt;"). */` |
|     84 | 6456 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 6457 | `                         int iFlags,int bFull){` |
|     85 | 6458 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     85 | 6459 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|     85 | 6460 | `	const unsigned char *runStart = p;` |
|     85 | 6461 | `	ph7_result_string(pCtx,"",0);` |
|    565 | 6462 | `	while( p < zEnd ){` |
|      - | 6463 | `		sxu32 cp;` |
|      - | 6464 | `		int nEat;` |
|    516 | 6465 | `		if( *p != '&' ){ p++; continue; }` |
|    155 | 6466 | `		if( !HtmlParseEntity(p,zEnd,iFlags,bFull,0,&cp,&nEat) ){ p++; continue; }` |
|    124 | 6467 | `		if( (cp == 34 && (iFlags & PH7_ENT_QUOTE_DOUBLE) == 0)` |
|    117 | 6468 | `		 \|\| (cp == 39 && (iFlags & PH7_ENT_QUOTE_SINGLE) == 0) ){` |
|      - | 6469 | `			/* Suppressed quote: leave the entity source verbatim. */` |
|     37 | 6470 | `			p += nEat;` |
|     37 | 6471 | `			continue;` |
|      - | 6472 | `		}` |
|     89 | 6473 | `		if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|      - | 6474 | `		{` |
|      - | 6475 | `			char zBuf[4];` |
|     89 | 6476 | `			int n = HtmlCpUtf8(cp,zBuf);` |
|     89 | 6477 | `			ph7_result_string(pCtx,zBuf,n);` |
|      - | 6478 | `		}` |
|     89 | 6479 | `		p += nEat;` |
|     89 | 6480 | `		runStart = p;` |
|      1 | 6481 | `	}` |
|     81 | 6482 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     81 | 6483 | `}` |
|      - | 6484 | `/* Validate the optional charset argument at apArg[idx]: UTF-8 aliases (and` |
|      - | 6485 | ` * ""/NULL meaning the default) are accepted; anything else — including` |
|      - | 6486 | ` * php-supported single-byte charsets like ISO-8859-1, PHL is UTF-8-only by` |
|      - | 6487 | ` * policy — raises PHP's unsupported-charset warning and is treated as` |
|      - | 6488 | ` * UTF-8 (ph7_context_throw_error_format prepends the function name). */` |
|    143 | 6489 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx){` |
|      - | 6490 | `	const char *zCs;` |
|      - | 6491 | `	int nCs;` |
|    150 | 6492 | `	if( nArg <= idx \|\| ph7_value_is_null(apArg[idx]) ){ return; }` |
|     15 | 6493 | `	zCs = ph7_value_to_string(apArg[idx],&nCs);` |
|     15 | 6494 | `	if( nCs == 0 ){ return; } /* "" selects the default charset (UTF-8) */` |
|     13 | 6495 | `	if( nCs == 5 && SyStrnicmp(zCs,"UTF-8",5) == 0 ){` |
|     13 | 6496 | `		return; /* php accepts only "UTF-8" (any case) silently — "UTF8" warns */` |
|      - | 6497 | `	}` |
|    ! 0 | 6498 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 6499 | `		"Charset \"%.*s\" is not supported, assuming UTF-8",nCs,zCs);` |
|     72 | 6500 | `}` |
|      - | 6501 | `/* get_html_translation_table() worker: character (UTF-8 bytes) => entity.` |
|      - | 6502 | ` * The five specials come first in byte order, then — for HTML_ENTITIES with a` |
|      - | 6503 | ` * named-table doctype — the 248 aHtml401Ent rows ascending (oracle-pinned` |
|      - | 6504 | ` * ordering; 253 entries under the defaults). */` |
|    549 | 6505 | `static void HtmlTableAdd(ph7_value *pArray,ph7_value *pValue,const char *zKey,const char *zEnt){` |
|    549 | 6506 | `	ph7_value_string(pValue,zEnt,-1);` |
|    549 | 6507 | `	ph7_array_add_strkey_elem(pArray,zKey,pValue);` |
|    549 | 6508 | `	ph7_value_reset_string_cursor(pValue);` |
|    549 | 6509 | `}` |
|     13 | 6510 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags){` |
|      - | 6511 | `	ph7_value *pArray,*pValue;` |
|     13 | 6512 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6513 | `	sxu32 n;` |
|     13 | 6514 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 6515 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 6516 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|    ! 0 | 6517 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6518 | `		return;` |
|      - | 6519 | `	}` |
|     13 | 6520 | `	if( iFlags & PH7_ENT_QUOTE_DOUBLE ){` |
|     11 | 6521 | `		HtmlTableAdd(pArray,pValue,"\"","&quot;");` |
|      5 | 6522 | `	}` |
|     13 | 6523 | `	HtmlTableAdd(pArray,pValue,"&","&amp;");` |
|     13 | 6524 | `	if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|      - | 6525 | `		/* The apostrophe row mirrors the function each table belongs to:` |
|      - | 6526 | `		 * SPECIALCHARS follows htmlspecialchars, ENTITIES follows` |
|      - | 6527 | `		 * htmlentities (oracle-pinned at flags 35). */` |
|      7 | 6528 | `		HtmlTableAdd(pArray,pValue,"'",HtmlAposEntity(iDoc,iTable != 0));` |
|      3 | 6529 | `	}` |
|     13 | 6530 | `	HtmlTableAdd(pArray,pValue,"<","&lt;");` |
|     13 | 6531 | `	HtmlTableAdd(pArray,pValue,">","&gt;");` |
|     13 | 6532 | `	if( iTable != 0 /*php: any non-HTML_SPECIALCHARS table => entities*/ && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 6533 | `		char zKey[8];` |
|    499 | 6534 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|    497 | 6535 | `			int nK = HtmlCpUtf8(aHtml401Ent[n].cp,zKey);` |
|    497 | 6536 | `			zKey[nK] = 0;` |
|    497 | 6537 | `			HtmlTableAdd(pArray,pValue,zKey,aHtml401Ent[n].zEnt);` |
|    249 | 6538 | `		}` |
|      1 | 6539 | `	}` |
|     13 | 6540 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 6541 | `}` |
|     25 | 6542 | `static int FvEmailAllowed(unsigned char c){` |
|     25 | 6543 | `	if( (c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9') ){ return 1; }` |
|     16 | 6544 | `	return c=='!'\|\|c=='#'\|\|c=='$'\|\|c=='%'\|\|c=='&'\|\|c=='\''\|\|c=='*'\|\|c=='+'` |
|     10 | 6545 | ``	    \|\| c=='-'\|\|c=='='\|\|c=='?'\|\|c=='^'\|\|c=='_'\|\|c=='`'\|\|c=='{'\|\|c=='\|'`` |
|     15 | 6546 | `	    \|\| c=='}'\|\|c=='~'\|\|c=='@'\|\|c=='.'\|\|c=='['\|\|c==']';` |
|     13 | 6547 | `}` |
|     23 | 6548 | `static int FvUrlAllowed(unsigned char c){` |
|     23 | 6549 | `	return c>=33 && c<=126; /* PHP keeps every printable ASCII byte except space */` |
|      1 | 6550 | `}` |
|      - | 6551 | `/* SANITIZE_EMAIL (isUrl=0) / SANITIZE_URL (isUrl=1): strip disallowed bytes. */` |
|      5 | 6552 | `static void FvSanitizeChars(ph7_context *pCtx,const char *z,int n,int isUrl){` |
|      5 | 6553 | `	int i, runStart = 0;` |
|      5 | 6554 | `	ph7_result_string(pCtx,"",0);` |
|     51 | 6555 | `	for( i=0; i<n; i++ ){` |
|     47 | 6556 | `		unsigned char c = (unsigned char)z[i];` |
|     47 | 6557 | `		if( !(isUrl ? FvUrlAllowed(c) : FvEmailAllowed(c)) ){` |
|     11 | 6558 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     11 | 6559 | `			runStart = i+1;` |
|      5 | 6560 | `		}` |
|     24 | 6561 | `	}` |
|      5 | 6562 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      5 | 6563 | `}` |
|      - | 6564 | `/*` |
|      - | 6565 | ` * Apply the selected filter to one already-resolved input value and write the` |
|      - | 6566 | ` * result into pCtx. Shared by filter_var() and filter_input(): the caller has` |
|      - | 6567 | ` * already parsed $filter/$flags/$options. On validation failure the 'default'` |
|      - | 6568 | ` * option (if any) is returned, else null when FILTER_NULL_ON_FAILURE is set,` |
|      - | 6569 | ` * else false. A validating filter that passes returns the (string) input` |
|      - | 6570 | ` * unchanged; a sanitizer writes its transformed output directly.` |
|      - | 6571 | ` */` |
|    316 | 6572 | `static int FvApplyFilter(ph7_context *pCtx,ph7_value *pInput,` |
|      - | 6573 | `                         int iFilter,int iFlags,ph7_value *pOpts,` |
|      - | 6574 | `                         ph7_value *pDefault)` |
|      3 | 6575 | `{` |
|    319 | 6576 | `	int bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|      - | 6577 | `	const char *zVal; int nVal;` |
|      - | 6578 | `	/* An array/object input fails every scalar filter. */` |
|    319 | 6579 | `	if( ph7_value_is_array(pInput) ){ goto fail; }` |
|    317 | 6580 | `	zVal = ph7_value_to_string(pInput,&nVal);` |
|    317 | 6581 | `	switch( iFilter ){` |
|     28 | 6582 | `	case FV_VALIDATE_INT: {` |
|      - | 6583 | `		ph7_int64 v;` |
|     58 | 6584 | `		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }` |
|     31 | 6585 | `		if( pOpts ){` |
|      7 | 6586 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|      7 | 6587 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|      7 | 6588 | `			if( pMin && v<ph7_value_to_int64(pMin) ){ goto fail; }` |
|      7 | 6589 | `			if( pMax && v>ph7_value_to_int64(pMax) ){ goto fail; }` |
|      2 | 6590 | `		}` |
|     29 | 6591 | `		ph7_result_int64(pCtx,v);` |
|     29 | 6592 | `		return PH7_OK;` |
|      - | 6593 | `	}` |
|     34 | 6594 | `	case FV_VALIDATE_FLOAT: {` |
|      - | 6595 | `		double d;` |
|     69 | 6596 | `		if( !FvValidateFloat(zVal,nVal,iFlags,&d) ){ goto fail; }` |
|     39 | 6597 | `		ph7_result_double(pCtx,d);` |
|     39 | 6598 | `		return PH7_OK;` |
|      - | 6599 | `	}` |
|     14 | 6600 | `	case FV_VALIDATE_BOOLEAN: {` |
|      - | 6601 | `		int b;` |
|     29 | 6602 | `		if( !FvValidateBool(zVal,nVal,&b) ){ goto fail; }` |
|     21 | 6603 | `		ph7_result_bool(pCtx,b);` |
|     21 | 6604 | `		return PH7_OK;` |
|      - | 6605 | `	}` |
|     25 | 6606 | `	case FV_VALIDATE_IP:     if( !FvValidateIp(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|     11 | 6607 | `	case FV_VALIDATE_MAC:    if( !FvValidateMac(zVal,nVal) ){ goto fail; }       goto pass;` |
|     28 | 6608 | `	case FV_VALIDATE_EMAIL:  if( !FvValidateEmail(zVal,nVal) ){ goto fail; }     goto pass;` |
|     11 | 6609 | `	case FV_VALIDATE_DOMAIN: if( !FvValidateDomain(zVal,nVal) ){ goto fail; }    goto pass;` |
|     15 | 6610 | `	case FV_VALIDATE_URL:    if( !FvValidateUrl(zVal,nVal) ){ goto fail; }       goto pass;` |
|      3 | 6611 | `	case FV_VALIDATE_REGEXP: {` |
|      - | 6612 | `#ifdef PH7_ENABLE_PCRE` |
|      8 | 6613 | `		ph7_value *pRe = pOpts ? ph7_array_fetch(pOpts,"regexp",(int)sizeof("regexp")-1) : 0;` |
|      8 | 6614 | `		const char *zRe; int nRe, matched = 0;` |
|      8 | 6615 | `		if( pRe==0 ){` |
|      3 | 6616 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6617 | `				"filter_var(): \"regexp\" option is missing");` |
|      - | 6618 | `		}` |
|      5 | 6619 | `		zRe = ph7_value_to_string(pRe,&nRe);` |
|      5 | 6620 | `		if( PH7_PcreMatchQuiet(pCtx,zRe,nRe,zVal,nVal,&matched)!=SXRET_OK \|\| !matched ){ goto fail; }` |
|      3 | 6621 | `		goto pass;` |
|      - | 6622 | `#else` |
|      - | 6623 | `		goto fail;` |
|      - | 6624 | `#endif` |
|      - | 6625 | `	}` |
|      3 | 6626 | `	case FV_SANITIZE_NUMBER_INT:   FvSanitizeNumber(pCtx,zVal,nVal,0,0);      return PH7_OK;` |
|      5 | 6627 | `	case FV_SANITIZE_NUMBER_FLOAT: FvSanitizeNumber(pCtx,zVal,nVal,1,iFlags); return PH7_OK;` |
|     13 | 6628 | `	case FV_SANITIZE_SPECIAL_CHARS:      FvSanitizeSpecial(pCtx,zVal,nVal,iFlags); return PH7_OK;` |
|     25 | 6629 | `	case FV_SANITIZE_FULL_SPECIAL_CHARS: FvSanitizeFull(pCtx,zVal,nVal,iFlags);    return PH7_OK;` |
|      3 | 6630 | `	case FV_SANITIZE_EMAIL: FvSanitizeChars(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 6631 | `	case FV_SANITIZE_URL:   FvSanitizeChars(pCtx,zVal,nVal,1); return PH7_OK;` |
|     13 | 6632 | `	case FV_DEFAULT:` |
|      - | 6633 | `		/* FILTER_UNSAFE_RAW / FILTER_DEFAULT: pass through unchanged unless a` |
|      - | 6634 | `		 * STRIP/ENCODE flag is set, in which case apply the string filter. */` |
|     28 | 6635 | `		if( iFlags & FV_FLAG_STRING_MASK ){` |
|     15 | 6636 | `			FvSanitizeString(pCtx,zVal,nVal,iFlags);` |
|     15 | 6637 | `			return PH7_OK;` |
|      - | 6638 | `		}` |
|     14 | 6639 | `		goto pass;` |
|    ! 0 | 6640 | `	default:` |
|    ! 0 | 6641 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 6642 | `			"Unknown filter with ID %d",iFilter);` |
|    ! 0 | 6643 | `		break; /* unknown filter id -> fail */` |
|    ! 0 | 6644 | `	}` |
|     58 | 6645 | `fail:` |
|    118 | 6646 | `	if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|    114 | 6647 | `	else if( bNull ){ ph7_result_null(pCtx); }` |
|    108 | 6648 | `	else { ph7_result_bool(pCtx,0); }` |
|    118 | 6649 | `	return PH7_OK;` |
|     26 | 6650 | `pass: /* validation passed: return the (string) input unchanged */` |
|     54 | 6651 | `	ph7_result_string(pCtx,zVal,nVal);` |
|     54 | 6652 | `	return PH7_OK;` |
|    161 | 6653 | `}` |
|      - | 6654 | `/*` |
|      - | 6655 | ` * Parse the ($filter, $options) pair shared by filter_var()/filter_input() out` |
|      - | 6656 | ` * of apArg[iBase] ($filter) and apArg[iBase+1] ($options): $options is either a` |
|      - | 6657 | ` * plain flags int, or an array with 'flags' and an 'options' sub-array (whose` |
|      - | 6658 | ` * 'default' entry is the fallback value). Fills the four output pointers;` |
|      - | 6659 | ` * unset outputs keep the caller-provided defaults.` |
|      - | 6660 | ` */` |
|    328 | 6661 | `static void FvParseFilterArgs(int nArg,ph7_value **apArg,int iBase,` |
|      - | 6662 | `                              int *piFilter,int *piFlags,` |
|      - | 6663 | `                              ph7_value **ppOpts,ph7_value **ppDefault)` |
|      3 | 6664 | `{` |
|    331 | 6665 | `	if( nArg>iBase ){ *piFilter = ph7_value_to_int(apArg[iBase]); }` |
|    331 | 6666 | `	if( nArg>iBase+1 ){` |
|     88 | 6667 | `		if( ph7_value_is_array(apArg[iBase+1]) ){` |
|     42 | 6668 | `			ph7_value *pF = ph7_array_fetch(apArg[iBase+1],"flags",(int)sizeof("flags")-1);` |
|     42 | 6669 | `			if( pF ){ *piFlags = ph7_value_to_int(pF); }` |
|     42 | 6670 | `			*ppOpts = ph7_array_fetch(apArg[iBase+1],"options",(int)sizeof("options")-1);` |
|     42 | 6671 | `			if( *ppOpts && !ph7_value_is_array(*ppOpts) ){ *ppOpts = 0; }` |
|     42 | 6672 | `			if( *ppOpts ){ *ppDefault = ph7_array_fetch(*ppOpts,"default",(int)sizeof("default")-1); }` |
|     22 | 6673 | `		}else{` |
|     48 | 6674 | `			*piFlags = ph7_value_to_int(apArg[iBase+1]);` |
|      - | 6675 | `		}` |
|     43 | 6676 | `	}` |
|    331 | 6677 | `}` |
|      - | 6678 | `/*` |
|      - | 6679 | ` * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 6680 | ` *  Validate or sanitize a value; see FvApplyFilter for the failure semantics.` |
|      - | 6681 | ` */` |
|    306 | 6682 | `static int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6683 | `{` |
|    308 | 6684 | `	int iFilter = FV_DEFAULT, iFlags = 0;` |
|    308 | 6685 | `	ph7_value *pOpts = 0, *pDefault = 0;` |
|    308 | 6686 | `	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    308 | 6687 | `	FvParseFilterArgs(nArg,apArg,1,&iFilter,&iFlags,&pOpts,&pDefault);` |
|    308 | 6688 | `	return FvApplyFilter(pCtx,apArg[0],iFilter,iFlags,pOpts,pDefault);` |
|    155 | 6689 | `}` |
|      - | 6690 | `/*` |
|      - | 6691 | ` * filter_input($type, $var_name, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 6692 | ` *  Look up $var_name in the requested INPUT_* superglobal, then apply the` |
|      - | 6693 | ` *  filter. Semantics verified byte-for-byte against php 8.5:` |
|      - | 6694 | ` *   - variable NOT set: 'default' option wins, else false when` |
|      - | 6695 | ` *     FILTER_NULL_ON_FAILURE is set, else null. (Note the null/false roles are` |
|      - | 6696 | ` *     INVERTED relative to a present value that fails validation, which yields` |
|      - | 6697 | ` *     default > null-if-NULL_ON_FAILURE > false via FvApplyFilter.)` |
|      - | 6698 | ` *   - variable present: delegate to FvApplyFilter.` |
|      - | 6699 | ` *  Divergence: php reads a SAPI snapshot of the original request variables` |
|      - | 6700 | ` *  captured at startup; PHL reads the live superglobal. In CLI they match for` |
|      - | 6701 | ` *  the SAPI-registered keys (SCRIPT_NAME/PHP_SELF/DOCUMENT_ROOT); keys added` |
|      - | 6702 | ` *  only to the live $_SERVER (REQUEST_TIME/PWD/…) are visible here but not in` |
|      - | 6703 | ` *  php's snapshot.` |
|      - | 6704 | ` */` |
|     24 | 6705 | `static int PH7_builtin_filter_input(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6706 | `{` |
|     26 | 6707 | `	int iType, iFilter = FV_DEFAULT, iFlags = 0;` |
|     26 | 6708 | `	ph7_value *pOpts = 0, *pDefault = 0, *pSuper, *pElem;` |
|      - | 6709 | `	const char *zVar, *zSuper; int nVar; sxu32 nSuper;` |
|     26 | 6710 | `	if( nArg<2 ){` |
|    ! 0 | 6711 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 6712 | `			"filter_input() expects at least 2 arguments, %d given",nArg);` |
|      - | 6713 | `	}` |
|     26 | 6714 | `	iType = ph7_value_to_int(apArg[0]);` |
|     26 | 6715 | `	switch( iType ){` |
|      3 | 6716 | `	case 0: zSuper = "_POST";   nSuper = (sxu32)sizeof("_POST")-1;   break; /* INPUT_POST */` |
|      3 | 6717 | `	case 1: zSuper = "_GET";    nSuper = (sxu32)sizeof("_GET")-1;    break; /* INPUT_GET */` |
|    ! 0 | 6718 | `	case 2: zSuper = "_COOKIE"; nSuper = (sxu32)sizeof("_COOKIE")-1; break; /* INPUT_COOKIE */` |
|    ! 0 | 6719 | `	case 4: zSuper = "_ENV";    nSuper = (sxu32)sizeof("_ENV")-1;    break; /* INPUT_ENV */` |
|     19 | 6720 | `	case 5: zSuper = "_SERVER"; nSuper = (sxu32)sizeof("_SERVER")-1; break; /* INPUT_SERVER */` |
|      1 | 6721 | `	default:` |
|      3 | 6722 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6723 | `			"filter_input(): Argument #1 ($type) must be an INPUT_* constant");` |
|      - | 6724 | `	}` |
|     23 | 6725 | `	zVar = ph7_value_to_string(apArg[1],&nVar);` |
|     23 | 6726 | `	FvParseFilterArgs(nArg,apArg,2,&iFilter,&iFlags,&pOpts,&pDefault);` |
|      - | 6727 | `	/* Resolve the variable from the superglobal (missing/non-array -> not set). */` |
|     23 | 6728 | `	pSuper = PH7_VmExtractSuper(pCtx->pVm,zSuper,nSuper);` |
|     23 | 6729 | `	pElem = (pSuper && ph7_value_is_array(pSuper))` |
|     33 | 6730 | `		? ph7_array_fetch(pSuper,zVar,nVar) : 0;` |
|     23 | 6731 | `	if( pElem==0 ){` |
|      - | 6732 | `		/* Variable not set: default > false(if NULL_ON_FAILURE) > null. Note the` |
|      - | 6733 | `		 * false/null roles are inverted vs FvApplyFilter's present-but-fails path. */` |
|     13 | 6734 | `		if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|      9 | 6735 | `		else if( iFlags & FV_NULL_ON_FAILURE ){ ph7_result_bool(pCtx,0); }` |
|      7 | 6736 | `		else { ph7_result_null(pCtx); }` |
|     13 | 6737 | `		return PH7_OK;` |
|      - | 6738 | `	}` |
|     11 | 6739 | `	return FvApplyFilter(pCtx,pElem,iFilter,iFlags,pOpts,pDefault);` |
|     14 | 6740 | `}` |
|      - | 6741 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6742 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6743 | `/*` |
|      - | 6744 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 6745 |  |
|      - | 6746 | ` */` |
|      4 | 6747 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 6748 | `	const char *zInput, /* Raw input */` |
|      - | 6749 | `	int nByte,  /* Input length */` |
|      - | 6750 | `	int delim,  /* Delimiter */` |
|      - | 6751 | `	int encl,   /* Enclosure */` |
|      - | 6752 | `	int escape,  /* Escape character */` |
|      - | 6753 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 6754 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 6755 | `	)` |
|      1 | 6756 | `{` |
|      5 | 6757 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 6758 | `	const char *zIn = zInput;` |
|      - | 6759 | `	const char *zPtr;` |
|      - | 6760 | `	int isEnc;` |
|      - | 6761 | `	/* Start processing */` |
|      8 | 6762 | `	for(;;){` |
|     17 | 6763 | `		if( zIn >= zEnd ){` |
|      - | 6764 | `			/* No more input to process */` |
|      5 | 6765 | `			break;` |
|      - | 6766 | `		}` |
|     13 | 6767 | `		isEnc = 0;` |
|     13 | 6768 | `		zPtr = zIn;` |
|      - | 6769 | `		/* Find the first delimiter */` |
|     27 | 6770 | `		while( zIn < zEnd ){` |
|     23 | 6771 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 6772 | `				/* Delimiter found,break imediately */` |
|      5 | 6773 | `				break;` |
|     15 | 6774 | `			}else if( zIn[0] == encl ){` |
|      - | 6775 | `				/* Inside enclosure? */` |
|    ! 0 | 6776 | `				isEnc = !isEnc;` |
|     15 | 6777 | `			}else if( zIn[0] == escape ){` |
|      - | 6778 | `				/* Escape sequence */` |
|    ! 0 | 6779 | `				zIn++;` |
|    ! 0 | 6780 | `			}` |
|      - | 6781 | `			/* Advance the cursor */` |
|     15 | 6782 | `			zIn++;` |
|      1 | 6783 | `		}` |
|     13 | 6784 | `		if( zIn > zPtr ){` |
|     13 | 6785 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 6786 | `			sxi32 rc;` |
|      - | 6787 | `			/* Invoke the supllied callback */` |
|     13 | 6788 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 6789 | `				zPtr++;` |
|    ! 0 | 6790 | `				nByteChunk-=2;` |
|    ! 0 | 6791 | `			}` |
|     13 | 6792 | `			if( nByteChunk > 0 ){` |
|     13 | 6793 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 6794 | `				if( rc == SXERR_ABORT ){` |
|      - | 6795 | `					/* User callback request an operation abort */` |
|    ! 0 | 6796 | `					break;` |
|      - | 6797 | `				}` |
|      6 | 6798 | `			}` |
|      6 | 6799 | `		}` |
|      - | 6800 | `		/* Ignore trailing delimiter */` |
|     21 | 6801 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 6802 | `			zIn++;` |
|      1 | 6803 | `		}` |
|      1 | 6804 | `	}` |
|      5 | 6805 | `	return SXRET_OK;` |
|      1 | 6806 | `}` |
|      - | 6807 | `/*` |
|      - | 6808 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 6809 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 6810 | ` * argument to this callback.` |
|      - | 6811 | ` */` |
|     12 | 6812 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 6813 | `{` |
|     13 | 6814 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 6815 | `	ph7_value sEntry;` |
|      - | 6816 | `	SyString sToken;` |
|      - | 6817 | `	/* Insert the token in the given array */` |
|     13 | 6818 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 6819 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 6820 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 6821 | `	if( sToken.nByte < 1){` |
|    ! 0 | 6822 | `		return SXRET_OK;` |
|      - | 6823 | `	}` |
|     13 | 6824 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 6825 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 6826 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 6827 | `	return SXRET_OK;` |
|      7 | 6828 | `}` |
|      - | 6829 | `/*` |
|      - | 6830 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 6831 | ` *  Parse a CSV string into an array.` |
|      - | 6832 | ` * Parameters` |
|      - | 6833 | ` *  $input` |
|      - | 6834 | ` *   The string to parse.` |
|      - | 6835 | ` *  $delimiter` |
|      - | 6836 | ` *   Set the field delimiter (one character only).` |
|      - | 6837 | ` *  $enclosure` |
|      - | 6838 | ` *   Set the field enclosure character (one character only).` |
|      - | 6839 | ` *  $escape` |
|      - | 6840 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 6841 | ` * Return` |
|      - | 6842 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 6843 | ` */` |
|      2 | 6844 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6845 | `{` |
|      - | 6846 | `	const char *zInput,*zPtr;` |
|      - | 6847 | `	ph7_value *pArray;` |
|      3 | 6848 | `	int delim  = ',';   /* Delimiter */` |
|      3 | 6849 | `	int encl   = '"' ;  /* Enclosure */` |
|      3 | 6850 | `	int escape = '\\';  /* Escape character */` |
|      - | 6851 | `	int nLen;` |
|      3 | 6852 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6853 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 6854 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6855 | `		return PH7_OK;` |
|      - | 6856 | `	}` |
|      - | 6857 | `	/* Extract the raw input */` |
|      3 | 6858 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 6859 | `	if( nArg > 1 ){` |
|      - | 6860 | `		int i;` |
|      3 | 6861 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 6862 | `			/* Extract the delimiter */` |
|      3 | 6863 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 6864 | `			if( i > 0 ){` |
|      3 | 6865 | `				delim = zPtr[0];` |
|      1 | 6866 | `			}` |
|      1 | 6867 | `		}` |
|      3 | 6868 | `		if( nArg > 2 ){` |
|      3 | 6869 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 6870 | `				/* Extract the enclosure */` |
|      3 | 6871 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 6872 | `				if( i > 0 ){` |
|      3 | 6873 | `					encl = zPtr[0];` |
|      1 | 6874 | `				}` |
|      1 | 6875 | `			}` |
|      3 | 6876 | `			if( nArg > 3 ){` |
|      3 | 6877 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 6878 | `					/* Extract the escape character */` |
|      3 | 6879 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 6880 | `					if( i > 0 ){` |
|      3 | 6881 | `						escape = zPtr[0];` |
|      1 | 6882 | `					}` |
|      1 | 6883 | `				}` |
|      1 | 6884 | `			}` |
|      1 | 6885 | `		}` |
|      1 | 6886 | `	}` |
|      - | 6887 | `	/* Create our array */` |
|      3 | 6888 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 6889 | `	if( pArray == 0 ){` |
|      - | 6890 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|    ! 0 | 6891 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6892 | `	}` |
|      - | 6893 | `	/* Parse the raw input */` |
|      3 | 6894 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 6895 | `	/* Return the freshly created array */` |
|      3 | 6896 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 6897 | `	return PH7_OK;` |
|      2 | 6898 | `}` |
|      - | 6899 | `/*` |
|      - | 6900 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 6901 | ` * container.` |
|      - | 6902 | ` * Refer to [strip_tags()].` |
|      - | 6903 | ` */` |
|     10 | 6904 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 6905 | `{` |
|     11 | 6906 | `	const char *zEnd = &zTag[nByte];` |
|      - | 6907 | `	const char *zPtr;` |
|      - | 6908 | `	SyString sEntry;` |
|      - | 6909 | `	/* Strip tags */` |
|     10 | 6910 | `	for(;;){` |
|     45 | 6911 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 6912 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 6913 | `				zTag++;` |
|      1 | 6914 | `		}` |
|     21 | 6915 | `		if( zTag >= zEnd ){` |
|     11 | 6916 | `			break;` |
|      - | 6917 | `		}` |
|     11 | 6918 | `		zPtr = zTag;` |
|      - | 6919 | `		/* Delimit the tag */` |
|     25 | 6920 | `		while(zTag < zEnd ){` |
|     25 | 6921 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 6922 | `				/* UTF-8 stream */` |
|      3 | 6923 | `				zTag++;` |
|      5 | 6924 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 6925 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 6926 | `				break;` |
|    ! 0 | 6927 | `			}else{` |
|     13 | 6928 | `				zTag++;` |
|      - | 6929 | `			}` |
|      1 | 6930 | `		}` |
|     11 | 6931 | `		if( zTag > zPtr ){` |
|      - | 6932 | `			/* Perform the insertion */` |
|     11 | 6933 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 6934 | `			SyStringFullTrim(&sEntry);` |
|     11 | 6935 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 6936 | `		}` |
|      - | 6937 | `		/* Jump the trailing '>' */` |
|     11 | 6938 | `		zTag++;` |
|      1 | 6939 | `	}` |
|     11 | 6940 | `	return SXRET_OK;` |
|      1 | 6941 | `}` |
|      - | 6942 | `/*` |
|      - | 6943 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 6944 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 6945 | ` * Refer to [strip_tags()].` |
|      - | 6946 | ` */` |
|     36 | 6947 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 6948 | `{` |
|     37 | 6949 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 6950 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 6951 | `		SyString sTag;` |
|     85 | 6952 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 6953 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 6954 | `			zTag++;` |
|      1 | 6955 | `		}` |
|      - | 6956 | `		/* Delimit the tag */` |
|     25 | 6957 | `		zCur = zTag;` |
|     77 | 6958 | `		while(zTag < zEnd ){` |
|     77 | 6959 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 6960 | `				/* UTF-8 stream */` |
|      5 | 6961 | `				zTag++;` |
|      9 | 6962 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 6963 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 6964 | `				break;` |
|    ! 0 | 6965 | `			}else{` |
|     49 | 6966 | `				zTag++;` |
|      - | 6967 | `			}` |
|      1 | 6968 | `		}` |
|     25 | 6969 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 6970 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 6971 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 6972 | `		if( sTag.nByte > 0 ){` |
|      - | 6973 | `			SyString *aEntry,*pEntry;` |
|      - | 6974 | `			sxi32 rc;` |
|      - | 6975 | `			sxu32 n;` |
|      - | 6976 | `			/* Perform the lookup */` |
|     25 | 6977 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 6978 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 6979 | `				pEntry = &aEntry[n];` |
|      - | 6980 | `				/* Do the comparison */` |
|     25 | 6981 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 6982 | `				if( !rc ){` |
|     21 | 6983 | `					return SXRET_OK;` |
|      - | 6984 | `				}` |
|      3 | 6985 | `			}` |
|      2 | 6986 | `		}` |
|      2 | 6987 | `	}` |
|      - | 6988 | `	/* No such tag */` |
|     17 | 6989 | `	return SXERR_NOTFOUND;` |
|     19 | 6990 | `}` |
|      - | 6991 | `/*` |
|      - | 6992 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 6993 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 6994 | ` * Refer to [strip_tags()].` |
|      - | 6995 | ` */` |
|     16 | 6996 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 6997 | `{` |
|     17 | 6998 | `	const char *zEnd = &zIn[nByte];` |
|      - | 6999 | `	const char *zPtr,*zTag;` |
|      - | 7000 | `	SySet sSet;` |
|      - | 7001 | `	/* initialize the set of allowed tags */` |
|     17 | 7002 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 7003 | `	if( nTaglen > 0 ){` |
|      - | 7004 | `		/* Set of allowed tags */` |
|     11 | 7005 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 7006 | `	}` |
|      - | 7007 | `	/* Set the empty string */` |
|     17 | 7008 | `	ph7_result_string(pCtx,"",0);` |
|      - | 7009 | `	/* Start processing */` |
|     26 | 7010 | `	for(;;){` |
|     53 | 7011 | `		if(zIn >= zEnd){` |
|      - | 7012 | `			/* No more input to process */` |
|     15 | 7013 | `			break;` |
|      - | 7014 | `		}` |
|     39 | 7015 | `		zPtr = zIn;` |
|      - | 7016 | `		/* Find a tag */` |
|    133 | 7017 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 7018 | `			zIn++;` |
|      1 | 7019 | `		}` |
|     39 | 7020 | `		if( zIn > zPtr ){` |
|      - | 7021 | `			/* Consume raw input */` |
|     21 | 7022 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 7023 | `		}` |
|      - | 7024 | `		/* Ignore trailing null bytes */` |
|     39 | 7025 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 7026 | `			zIn++;` |
|    ! 0 | 7027 | `		}` |
|     39 | 7028 | `		if(zIn >= zEnd){` |
|      - | 7029 | `			/* No more input to process */` |
|      3 | 7030 | `			break;` |
|      - | 7031 | `		}` |
|     37 | 7032 | `		if( zIn[0] == '<' ){` |
|      - | 7033 | `			sxi32 rc;` |
|     37 | 7034 | `			zTag = zIn++;` |
|      - | 7035 | `			/* Delimit the tag */` |
|    127 | 7036 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 7037 | `				zIn++;` |
|      1 | 7038 | `			}` |
|     37 | 7039 | `			if( zIn < zEnd ){` |
|     37 | 7040 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 7041 | `			}` |
|      - | 7042 | `			/* Query the set */` |
|     37 | 7043 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 7044 | `			if( rc == SXRET_OK ){` |
|      - | 7045 | `				/* Keep the tag */` |
|     21 | 7046 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 7047 | `			}` |
|     18 | 7048 | `		}` |
|      1 | 7049 | `	}` |
|      - | 7050 | `	/* Cleanup */` |
|     17 | 7051 | `	SySetRelease(&sSet);` |
|     17 | 7052 | `	return SXRET_OK;` |
|      1 | 7053 | `}` |
|      - | 7054 | `/*` |
|      - | 7055 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 7056 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 7057 | ` * Parameters` |
|      - | 7058 | ` *  $str` |
|      - | 7059 | ` *  The input string.` |
|      - | 7060 | ` * $allowable_tags` |
|      - | 7061 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 7062 | ` * Return` |
|      - | 7063 | ` *  Returns the stripped string.` |
|      - | 7064 | ` */` |
|     14 | 7065 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7066 | `{` |
|     15 | 7067 | `	const char *zTaglist = 0;` |
|      - | 7068 | `	const char *zString;` |
|     15 | 7069 | `	int nTaglen = 0;` |
|      - | 7070 | `	int nLen;` |
|     15 | 7071 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7072 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 7073 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7074 | `		return PH7_OK;` |
|      - | 7075 | `	}` |
|      - | 7076 | `	/* Point to the raw string */` |
|     15 | 7077 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 7078 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 7079 | `		/* Allowed tag */` |
|     11 | 7080 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 7081 | `	}` |
|      - | 7082 | `	/* Process input */` |
|     15 | 7083 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 7084 | `	return PH7_OK;` |
|      8 | 7085 | `}` |
|      - | 7086 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7087 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7088 | `/*` |
|      - | 7089 | ` * string str_shuffle(string $str)` |
|      - | 7090 |  |
|      - | 7091 | ` *  Randomly shuffles a string.` |
|      - | 7092 | ` * Parameters` |
|      - | 7093 | ` *  $str` |
|      - | 7094 | ` *   The input string.` |
|      - | 7095 | ` * Return` |
|      - | 7096 | ` *  Returns the shuffled string.` |
|      - | 7097 | ` */` |
|     10 | 7098 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7099 | `{` |
|      - | 7100 | `	const char *zString;` |
|      - | 7101 | `	int nLen,i,c;` |
|      - | 7102 | `	sxu32 iR;` |
|     11 | 7103 | `	if( nArg < 1 ){` |
|      - | 7104 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7105 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7106 | `		return PH7_OK;` |
|      - | 7107 | `	}` |
|      - | 7108 | `	/* Extract the target string */` |
|     11 | 7109 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 7110 | `	if( nLen < 1 ){` |
|      - | 7111 | `		/* Nothing to shuffle */` |
|      3 | 7112 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 7113 | `		return PH7_OK;` |
|      - | 7114 | `	}` |
|      - | 7115 | `	/* Shuffle the string */` |
|     43 | 7116 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 7117 | `		/* Generate a random number first */` |
|     35 | 7118 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 7119 | `		/* Extract a random offset */` |
|     35 | 7120 | `		c = zString[iR % nLen];` |
|      - | 7121 | `		/* Append it */` |
|     35 | 7122 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 7123 | `	}` |
|      9 | 7124 | `	return PH7_OK;` |
|      6 | 7125 | `}` |
|      - | 7126 | `/*` |
|      - | 7127 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 7128 | ` *  Convert a string to an array.` |
|      - | 7129 | ` * Parameters` |
|      - | 7130 | ` * $string` |
|      - | 7131 | ` *  The input string.` |
|      - | 7132 | ` * $split_length` |
|      - | 7133 | ` *  Maximum length of the chunk.` |
|      - | 7134 | ` * Return` |
|      - | 7135 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 7136 | ` *  except possibly the last one which may be shorter.` |
|      - | 7137 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 7138 | ` *  as the first (and only) array element.` |
|      - | 7139 | ` *  An empty string returns an empty array.` |
|      - | 7140 | ` * Errors` |
|      - | 7141 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 7142 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 7143 | ` *  ValueError if $split_length is less than 1.` |
|      - | 7144 | ` */` |
|     24 | 7145 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 7146 | `{` |
|      - | 7147 | `	const char *zString,*zEnd;` |
|      - | 7148 | `	ph7_value *pArray,*pValue;` |
|      - | 7149 | `	int split_len;` |
|      - | 7150 | `	int nLen;` |
|     27 | 7151 | `	if( nArg < 1 ){` |
|    ! 0 | 7152 | `		return PH7_VmThrowException(pCtx,` |
|      - | 7153 | `			"ArgumentCountError",` |
|      - | 7154 | `			"str_split() expects at least 1 argument, %d given",` |
|    ! 0 | 7155 | `			nArg` |
|      - | 7156 | `			);` |
|      - | 7157 | `	}` |
|      - | 7158 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     36 | 7159 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     39 | 7160 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 7161 | `	    ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 7162 | `		return PH7_VmThrowException(pCtx,` |
|      - | 7163 | `			"TypeError",` |
|      - | 7164 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 7165 | `			ph7_type_name(apArg[0])` |
|      - | 7166 | `			);` |
|      - | 7167 | `	}` |
|      - | 7168 | `	/* Point to the target string */` |
|     27 | 7169 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 7170 | `	split_len = (int)sizeof(char);` |
|     27 | 7171 | `	if( nArg > 1 ){` |
|      - | 7172 | `		/* Split length */` |
|     17 | 7173 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 7174 | `		if( split_len < 1 ){` |
|      6 | 7175 | `			return PH7_VmThrowException(pCtx,` |
|      - | 7176 | `				"ValueError",` |
|      - | 7177 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 7178 | `				);` |
|      - | 7179 | `		}` |
|     11 | 7180 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 7181 | `			split_len = nLen;` |
|      1 | 7182 | `		}` |
|      5 | 7183 | `	}` |
|      - | 7184 | `	/* Create the array and the scalar value */` |
|     21 | 7185 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 7186 | `	/*Chunk value */` |
|     21 | 7187 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 7188 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 7189 | `		/* Return FALSE */` |
|    ! 0 | 7190 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7191 | `		return PH7_OK;` |
|      - | 7192 | `	}` |
|      - | 7193 | `	/* Point to the end of the string */` |
|     21 | 7194 | `	zEnd = &zString[nLen];` |
|      - | 7195 | `	/* Perform the requested operation */` |
|     48 | 7196 | `	for(;;){` |
|      - | 7197 | `		int nMax;` |
|     59 | 7198 | `		if( zString >= zEnd ){` |
|      - | 7199 | `			/* No more input to process */` |
|     21 | 7200 | `			break;` |
|      - | 7201 | `		}` |
|     39 | 7202 | `		nMax = (int)(zEnd-zString);` |
|     39 | 7203 | `		if( nMax < split_len ){` |
|      3 | 7204 | `			split_len = nMax;` |
|      1 | 7205 | `		}` |
|      - | 7206 | `		/* Copy the current chunk */` |
|     39 | 7207 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 7208 | `		/* Insert it */` |
|     39 | 7209 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|    ! 0 | 7210 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 7211 | `		}` |
|      - | 7212 | `		/* reset the string cursor */` |
|     39 | 7213 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 7214 | `		/* Update position */` |
|     39 | 7215 | `		zString += split_len;` |
|      1 | 7216 | `	}` |
|      - | 7217 | `	/*` |
|      - | 7218 | `	 * Return the array.` |
|      - | 7219 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 7220 | `	 * upon we return from this function.` |
|      - | 7221 | `	 */` |
|     21 | 7222 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 7223 | `	return PH7_OK;` |
|     15 | 7224 | `}` |
|      - | 7225 | `/*` |
|      - | 7226 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 7227 | ` * Refer to [strspn()].` |
|      - | 7228 | ` */` |
|     28 | 7229 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 7230 | `{` |
|     29 | 7231 | `	const char *zIn = *pzIn;` |
|      - | 7232 | `	const char *zPtr;` |
|      - | 7233 | `	/* Ignore leading white spaces */` |
|     29 | 7234 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 7235 | `		zIn++;` |
|    ! 0 | 7236 | `	}` |
|     29 | 7237 | `	if( zIn >= zEnd ){` |
|      - | 7238 | `		/* End of input */` |
|    ! 0 | 7239 | `		return SXERR_EOF;` |
|      - | 7240 | `	}` |
|     29 | 7241 | `	zPtr = zIn;` |
|      - | 7242 | `	/* Extract the token */` |
|    201 | 7243 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 7244 | `		zIn++;` |
|      1 | 7245 | `	}` |
|     29 | 7246 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 7247 | `	/* Synchronize pointers */` |
|     29 | 7248 | `	*pzIn = zIn;` |
|      - | 7249 | `	/* Return to the caller */` |
|     29 | 7250 | `	return SXRET_OK;` |
|     15 | 7251 | `}` |
|      - | 7252 | `/*` |
|      - | 7253 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 7254 | ` * return the longest match.` |
|      - | 7255 | ` * Refer to [strspn()].` |
|      - | 7256 | ` */` |
|     18 | 7257 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 7258 | `{` |
|     19 | 7259 | `	const char *zEnd = &zString[nLen];` |
|     19 | 7260 | `	const char *zIn = zString;` |
|      - | 7261 | `	int i,c;` |
|     45 | 7262 | `	for(;;){` |
|     91 | 7263 | `		if( zString >= zEnd ){` |
|      7 | 7264 | `			break;` |
|      - | 7265 | `		}` |
|      - | 7266 | `		/* Extract current character */` |
|     85 | 7267 | `		c = zString[0];` |
|      - | 7268 | `		/* Perform the lookup */` |
|    383 | 7269 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 7270 | `			if( c == zMask[i] ){` |
|      - | 7271 | `				/* Character found */` |
|     73 | 7272 | `				break;` |
|      - | 7273 | `			}` |
|    150 | 7274 | `		}` |
|     85 | 7275 | `		if( i >= nMaskLen ){` |
|      - | 7276 | `			/* Character not in the current mask,break immediately */` |
|     13 | 7277 | `			break;` |
|      - | 7278 | `		}` |
|      - | 7279 | `		/* Advance cursor */` |
|     73 | 7280 | `		zString++;` |
|      1 | 7281 | `	}` |
|      - | 7282 | `	/* Longest match */` |
|     19 | 7283 | `	return (int)(zString-zIn);` |
|      1 | 7284 | `}` |
|      - | 7285 | `/*` |
|      - | 7286 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 7287 | ` * Refer to [strcspn()].` |
|      - | 7288 | ` */` |
|     10 | 7289 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 7290 | `{` |
|     11 | 7291 | `	const char *zEnd = &zString[nLen];` |
|     11 | 7292 | `	const char *zIn = zString;` |
|      - | 7293 | `	int i,c;` |
|     12 | 7294 | `	for(;;){` |
|     25 | 7295 | `		if( zString >= zEnd ){` |
|      3 | 7296 | `			break;` |
|      - | 7297 | `		}` |
|      - | 7298 | `		/* Extract current character */` |
|     23 | 7299 | `		c = zString[0];` |
|      - | 7300 | `		/* Perform the lookup */` |
|     51 | 7301 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 7302 | `			if( c == zMask[i] ){` |
|      9 | 7303 | `				break;` |
|      - | 7304 | `			}` |
|     15 | 7305 | `		}` |
|     23 | 7306 | `		if( i < nMaskLen ){` |
|      - | 7307 | `			/* Character in the current mask,break immediately */` |
|      9 | 7308 | `			break;` |
|      - | 7309 | `		}` |
|      - | 7310 | `		/* Advance cursor */` |
|     15 | 7311 | `		zString++;` |
|      1 | 7312 | `	}` |
|      - | 7313 | `	/* Longest match */` |
|     11 | 7314 | `	return (int)(zString-zIn);` |
|      1 | 7315 | `}` |
|      - | 7316 | `/*` |
|      - | 7317 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 7318 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 7319 | ` *  of characters contained within a given mask.` |
|      - | 7320 | ` * Parameters` |
|      - | 7321 | ` * $str` |
|      - | 7322 | ` *  The input string.` |
|      - | 7323 | ` * $mask` |
|      - | 7324 | ` *  The list of allowable characters.` |
|      - | 7325 | ` * $start` |
|      - | 7326 | ` *  The position in subject to start searching.` |
|      - | 7327 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 7328 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 7329 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 7330 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 7331 | ` *  start'th position from the end of subject.` |
|      - | 7332 | ` * $length` |
|      - | 7333 | ` *  The length of the segment from subject to examine.` |
|      - | 7334 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 7335 | ` *  characters after the starting position.` |
|      - | 7336 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 7337 | ` *  position up to length characters from the end of subject.` |
|      - | 7338 | ` * Return` |
|      - | 7339 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 7340 | ` * in mask.` |
|      - | 7341 | ` */` |
|     24 | 7342 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7343 | `{` |
|      - | 7344 | `	const char *zString,*zMask,*zEnd;` |
|      - | 7345 | `	int iMasklen,iLen;` |
|      - | 7346 | `	SyString sToken;` |
|     25 | 7347 | `	int iCount = 0;` |
|      - | 7348 | `	int rc;` |
|     25 | 7349 | `	if( nArg < 2 ){` |
|      - | 7350 | `		/* Missing agruments,return zero */` |
|    ! 0 | 7351 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7352 | `		return PH7_OK;` |
|      - | 7353 | `	}` |
|      - | 7354 | `	/* Extract the target string */` |
|     25 | 7355 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7356 | `	/* Extract the mask */` |
|     25 | 7357 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 7358 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 7359 | `		/* Nothing to process,return zero */` |
|      7 | 7360 | `		ph7_result_int(pCtx,0);` |
|      7 | 7361 | `		return PH7_OK;` |
|      - | 7362 | `	}` |
|     19 | 7363 | `	if( nArg > 2 ){` |
|      - | 7364 | `		int nOfft;` |
|      - | 7365 | `		/* Extract the offset */` |
|      9 | 7366 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 7367 | `		if( nOfft < 0 ){` |
|    ! 0 | 7368 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 7369 | `			if( zBase > zString ){` |
|    ! 0 | 7370 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 7371 | `				zString = zBase;` |
|    ! 0 | 7372 | `			}else{` |
|      - | 7373 | `				/* Invalid offset */` |
|    ! 0 | 7374 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7375 | `				return PH7_OK;` |
|      - | 7376 | `			}` |
|    ! 0 | 7377 | `		}else{` |
|      9 | 7378 | `			if( nOfft >= iLen ){` |
|      - | 7379 | `				/* Invalid offset */` |
|    ! 0 | 7380 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7381 | `				return PH7_OK;` |
|    ! 0 | 7382 | `			}else{` |
|      - | 7383 | `				/* Update offset */` |
|      9 | 7384 | `				zString += nOfft;` |
|      9 | 7385 | `				iLen -= nOfft;` |
|      - | 7386 | `			}` |
|      - | 7387 | `		}` |
|      9 | 7388 | `		if( nArg > 3 ){` |
|      - | 7389 | `			int iUserlen;` |
|      - | 7390 | `			/* Extract the desired length */` |
|      9 | 7391 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 7392 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 7393 | `				iLen = iUserlen;` |
|      2 | 7394 | `			}` |
|      4 | 7395 | `		}` |
|      4 | 7396 | `	}` |
|      - | 7397 | `	/* Point to the end of the string */` |
|     19 | 7398 | `	zEnd = &zString[iLen];` |
|      - | 7399 | `	/* Extract the first non-space token */` |
|     19 | 7400 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 7401 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 7402 | `		/* Compare against the current mask */` |
|     19 | 7403 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 7404 | `	}` |
|      - | 7405 | `	/* Longest match */` |
|     19 | 7406 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 7407 | `	return PH7_OK;` |
|     13 | 7408 | `}` |
|      - | 7409 | `/*` |
|      - | 7410 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 7411 | ` *  Find length of initial segment not matching mask.` |
|      - | 7412 | ` * Parameters` |
|      - | 7413 | ` * $str` |
|      - | 7414 | ` *  The input string.` |
|      - | 7415 | ` * $mask` |
|      - | 7416 | ` *  The list of not allowed characters.` |
|      - | 7417 | ` * $start` |
|      - | 7418 | ` *  The position in subject to start searching.` |
|      - | 7419 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 7420 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 7421 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 7422 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 7423 | ` *  start'th position from the end of subject.` |
|      - | 7424 | ` * $length` |
|      - | 7425 | ` *  The length of the segment from subject to examine.` |
|      - | 7426 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 7427 | ` *  characters after the starting position.` |
|      - | 7428 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 7429 | ` *  position up to length characters from the end of subject.` |
|      - | 7430 | ` * Return` |
|      - | 7431 | ` *  Returns the length of the segment as an integer.` |
|      - | 7432 | ` */` |
|     14 | 7433 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7434 | `{` |
|      - | 7435 | `	const char *zString,*zMask,*zEnd;` |
|      - | 7436 | `	int iMasklen,iLen;` |
|      - | 7437 | `	SyString sToken;` |
|     15 | 7438 | `	int iCount = 0;` |
|      - | 7439 | `	int rc;` |
|     15 | 7440 | `	if( nArg < 2 ){` |
|      - | 7441 | `		/* Missing agruments,return zero */` |
|    ! 0 | 7442 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7443 | `		return PH7_OK;` |
|      - | 7444 | `	}` |
|      - | 7445 | `	/* Extract the target string */` |
|     15 | 7446 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7447 | `	/* Extract the mask */` |
|     15 | 7448 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 7449 | `	if( iLen < 1 ){` |
|      - | 7450 | `		/* Nothing to process,return zero */` |
|    ! 0 | 7451 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7452 | `		return PH7_OK;` |
|      - | 7453 | `	}` |
|     15 | 7454 | `	if( iMasklen < 1 ){` |
|      - | 7455 | `		/* No given mask,return the string length */` |
|      3 | 7456 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 7457 | `		return PH7_OK;` |
|      - | 7458 | `	}` |
|     13 | 7459 | `	if( nArg > 2 ){` |
|      - | 7460 | `		int nOfft;` |
|      - | 7461 | `		/* Extract the offset */` |
|     11 | 7462 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 7463 | `		if( nOfft < 0 ){` |
|    ! 0 | 7464 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 7465 | `			if( zBase > zString ){` |
|    ! 0 | 7466 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 7467 | `				zString = zBase;` |
|    ! 0 | 7468 | `			}else{` |
|      - | 7469 | `				/* Invalid offset */` |
|    ! 0 | 7470 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7471 | `				return PH7_OK;` |
|      - | 7472 | `			}` |
|    ! 0 | 7473 | `		}else{` |
|     11 | 7474 | `			if( nOfft >= iLen ){` |
|      - | 7475 | `				/* Invalid offset */` |
|      3 | 7476 | `				ph7_result_int(pCtx,0);` |
|      3 | 7477 | `				return PH7_OK;` |
|    ! 0 | 7478 | `			}else{` |
|      - | 7479 | `				/* Update offset */` |
|      9 | 7480 | `				zString += nOfft;` |
|      9 | 7481 | `				iLen -= nOfft;` |
|      - | 7482 | `			}` |
|      - | 7483 | `		}` |
|      9 | 7484 | `		if( nArg > 3 ){` |
|      - | 7485 | `			int iUserlen;` |
|      - | 7486 | `			/* Extract the desired length */` |
|    ! 0 | 7487 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 7488 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 7489 | `				iLen = iUserlen;` |
|    ! 0 | 7490 | `			}` |
|    ! 0 | 7491 | `		}` |
|      4 | 7492 | `	}` |
|      - | 7493 | `	/* Point to the end of the string */` |
|     11 | 7494 | `	zEnd = &zString[iLen];` |
|      - | 7495 | `	/* Extract the first non-space token */` |
|     11 | 7496 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 7497 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 7498 | `		/* Compare against the current mask */` |
|     11 | 7499 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 7500 | `	}` |
|      - | 7501 | `	/* Longest match */` |
|     11 | 7502 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 7503 | `	return PH7_OK;` |
|      8 | 7504 | `}` |
|      - | 7505 | `/*` |
|      - | 7506 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 7507 | ` *  Search a string for any of a set of characters.` |
|      - | 7508 | ` * Parameters` |
|      - | 7509 | ` *  $haystack` |
|      - | 7510 | ` *   The string where char_list is looked for.` |
|      - | 7511 | ` *  $char_list` |
|      - | 7512 | ` *   This parameter is case sensitive.` |
|      - | 7513 | ` * Return` |
|      - | 7514 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 7515 | ` */` |
|      4 | 7516 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7517 | `{` |
|      - | 7518 | `	const char *zString,*zList,*zEnd;` |
|      - | 7519 | `	int iLen,iListLen,i,c;` |
|      - | 7520 | `	sxu32 nOfft,nMax;` |
|      - | 7521 | `	sxi32 rc;` |
|      5 | 7522 | `	if( nArg < 2 ){` |
|      - | 7523 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7524 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7525 | `		return PH7_OK;` |
|      - | 7526 | `	}` |
|      - | 7527 | `	/* Extract the haystack and the char list */` |
|      5 | 7528 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 7529 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 7530 | `	if( iLen < 1 ){` |
|      - | 7531 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7532 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7533 | `		return PH7_OK;` |
|      - | 7534 | `	}` |
|      - | 7535 | `	/* Point to the end of the string */` |
|      5 | 7536 | `	zEnd = &zString[iLen];` |
|      5 | 7537 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 7538 | `	/* perform the requested operation */` |
|     15 | 7539 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 7540 | `		c = zList[i];` |
|     11 | 7541 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 7542 | `		if( rc == SXRET_OK ){` |
|      5 | 7543 | `			if( nMax < nOfft ){` |
|      3 | 7544 | `				nOfft = nMax;` |
|      1 | 7545 | `			}` |
|      2 | 7546 | `		}` |
|      6 | 7547 | `	}` |
|      5 | 7548 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 7549 | `		/* No such substring,return FALSE */` |
|      3 | 7550 | `		ph7_result_bool(pCtx,0);` |
|      2 | 7551 | `	}else{` |
|      - | 7552 | `		/* Return the substring */` |
|      3 | 7553 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 7554 | `	}` |
|      5 | 7555 | `	return PH7_OK;` |
|      3 | 7556 | `}` |
|      - | 7557 | `/* SPDX-SnippetBegin */` |
|      - | 7558 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 7559 | `/* SPDX-License-Identifier: blessing */` |
|      - | 7560 | `/*` |
|      - | 7561 | ` * string soundex(string $str)` |
|      - | 7562 | ` *  Calculate the soundex key of a string.` |
|      - | 7563 | ` * Parameters` |
|      - | 7564 | ` *  $str` |
|      - | 7565 | ` *   The input string.` |
|      - | 7566 | ` * Return` |
|      - | 7567 | ` *  Returns the soundex key as a string.` |
|      - | 7568 | ` * Note:` |
|      - | 7569 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 7570 | ` * source tree.` |
|      - | 7571 | ` */` |
|     22 | 7572 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7573 | `{` |
|      - | 7574 | `	const unsigned char *zIn;` |
|      - | 7575 | `	char zResult[8];` |
|      - | 7576 | `	int i, j;` |
|      - | 7577 | `	static const unsigned char iCode[] = {` |
|      - | 7578 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7579 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7580 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7581 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7582 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 7583 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 7584 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 7585 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 7586 | `	};` |
|     23 | 7587 | `	if( nArg < 1 ){` |
|      - | 7588 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7589 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7590 | `		return PH7_OK;` |
|      - | 7591 | `	}` |
|     23 | 7592 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     35 | 7593 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     23 | 7594 | `	if( zIn[i] ){` |
|     17 | 7595 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 7596 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 7597 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 7598 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 7599 | `			if( code>0 ){` |
|     45 | 7600 | `				if( code!=prevcode ){` |
|     33 | 7601 | `					prevcode = (unsigned char)code;` |
|     33 | 7602 | `					zResult[j++] = (char)code + '0';` |
|     16 | 7603 | `				}` |
|     23 | 7604 | `			}else{` |
|     49 | 7605 | `				prevcode = 0;` |
|      - | 7606 | `			}` |
|     47 | 7607 | `		}` |
|     33 | 7608 | `		while( j<4 ){` |
|     17 | 7609 | `			zResult[j++] = '0';` |
|      1 | 7610 | `		}` |
|     17 | 7611 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 7612 | `	}else{` |
|      - | 7613 | `	  /* No alphabetic character: PHP returns "0000" (not the SQLite "?000"). */` |
|      7 | 7614 | `	  ph7_result_string(pCtx,"0000",4);` |
|      - | 7615 | `	}` |
|     23 | 7616 | `	return PH7_OK;` |
|     12 | 7617 | `}` |
|      - | 7618 | `/* SPDX-SnippetEnd */` |
|      - | 7619 | `/*` |
|      - | 7620 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 7621 | ` *  Wraps a string to a given number of characters.` |
|      - | 7622 | ` * Parameters` |
|      - | 7623 | ` *  $str` |
|      - | 7624 | ` *   The input string.` |
|      - | 7625 | ` * $width` |
|      - | 7626 | ` *  The column width.` |
|      - | 7627 | ` * $break` |
|      - | 7628 | ` *  The line is broken using the optional break parameter.` |
|      - | 7629 | ` * Return` |
|      - | 7630 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 7631 | ` */` |
|     26 | 7632 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7633 | `{` |
|      - | 7634 | `	const char *zIn,*zBreak;` |
|      - | 7635 | `	SyBlob sWorker;` |
|      - | 7636 | `	int iLen,iBreaklen,iWidth,iCut,iStart,iSpace,iCur;` |
|      - | 7637 | `	sxi32 rc;` |
|     27 | 7638 | `	if( nArg < 1 ){` |
|      - | 7639 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7640 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7641 | `		return PH7_OK;` |
|      - | 7642 | `	}` |
|      - | 7643 | `	/* Extract the input string */` |
|     27 | 7644 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7645 | `	/* Width (default 75; PHP allows 0/negative — break at every space). */` |
|     27 | 7646 | `	iWidth = 75;` |
|     27 | 7647 | `	if( nArg > 1 ){` |
|     27 | 7648 | `		iWidth = ph7_value_to_int(apArg[1]);` |
|     13 | 7649 | `	}` |
|      - | 7650 | `	/* Break string (default "\n"). */` |
|     27 | 7651 | `	zBreak = "\n";` |
|     27 | 7652 | `	iBreaklen = (int)sizeof(char);` |
|     27 | 7653 | `	if( nArg > 2 ){` |
|     13 | 7654 | `		zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      6 | 7655 | `	}` |
|      - | 7656 | `	/* Cut long words? (default false). */` |
|     27 | 7657 | `	iCut = 0;` |
|     27 | 7658 | `	if( nArg > 3 ){` |
|      7 | 7659 | `		iCut = ph7_value_to_bool(apArg[3]);` |
|      3 | 7660 | `	}` |
|     27 | 7661 | `	if( iLen < 1 ){` |
|      - | 7662 | `		/* PHP returns the empty string for empty input before validating the other args. */` |
|      5 | 7663 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 7664 | `		return PH7_OK;` |
|      - | 7665 | `	}` |
|      - | 7666 | `	/* PHP 8 domain errors (catchable ValueError). */` |
|     23 | 7667 | `	if( iBreaklen < 1 ){` |
|      3 | 7668 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7669 | `			"wordwrap(): Argument #3 ($break) must not be empty");` |
|      - | 7670 | `	}` |
|     21 | 7671 | `	if( iWidth == 0 && iCut ){` |
|      3 | 7672 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7673 | `			"wordwrap(): Argument #4 ($cut_long_words) cannot be true when argument #2 ($width) is 0");` |
|      - | 7674 | `	}` |
|      - | 7675 | `	/*` |
|      - | 7676 | `	 * PHP's algorithm: a single left-to-right pass tracking the start of the` |
|      - | 7677 | `	 * current line (iStart) and the position of the last space seen on it` |
|      - | 7678 | `	 * (iSpace). A break is emitted when the line reaches the width, at the last` |
|      - | 7679 | `	 * space if there was one, otherwise (only when cut is enabled) hard at the` |
|      - | 7680 | `	 * boundary. An existing break sequence in the input resets the line.` |
|      - | 7681 | `	 */` |
|     19 | 7682 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     19 | 7683 | `	iStart = iSpace = iCur = 0;` |
|     19 | 7684 | `	rc = SXRET_OK;` |
|    551 | 7685 | `	while( iCur < iLen ){` |
|    533 | 7686 | `		if( iBreaklen <= iLen - iCur && SyMemcmp(&zIn[iCur],zBreak,(sxu32)iBreaklen) == 0 ){` |
|      - | 7687 | `			/* Existing break sequence in the input: copy it verbatim and reset the line. */` |
|    ! 0 | 7688 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart + iBreaklen));` |
|    ! 0 | 7689 | `			if( rc != SXRET_OK ){ goto oom; }` |
|    ! 0 | 7690 | `			iCur += iBreaklen;` |
|    ! 0 | 7691 | `			iStart = iSpace = iCur;` |
|    ! 0 | 7692 | `			continue;` |
|    533 | 7693 | `		}else if( zIn[iCur] == ' ' ){` |
|     67 | 7694 | `			if( iCur - iStart >= iWidth ){` |
|      - | 7695 | `				/* The line already fills the width at this space: break here (the space is consumed). */` |
|     13 | 7696 | `				rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     13 | 7697 | `				if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     13 | 7698 | `				if( rc != SXRET_OK ){ goto oom; }` |
|     13 | 7699 | `				iStart = iCur + 1;` |
|      6 | 7700 | `			}` |
|     67 | 7701 | `			iSpace = iCur;` |
|    500 | 7702 | `		}else if( iCut && iCur - iStart >= iWidth && iStart >= iSpace ){` |
|      - | 7703 | `			/* A word longer than the width with no space to break at: hard-cut at the boundary. */` |
|      7 | 7704 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|      7 | 7705 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|      7 | 7706 | `			if( rc != SXRET_OK ){ goto oom; }` |
|      7 | 7707 | `			iStart = iSpace = iCur;` |
|    464 | 7708 | `		}else if( iCur - iStart >= iWidth && iStart < iSpace ){` |
|      - | 7709 | `			/* Past the width mid-word: wrap back to the last space (which is consumed). */` |
|     17 | 7710 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iSpace - iStart));` |
|     17 | 7711 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     17 | 7712 | `			if( rc != SXRET_OK ){ goto oom; }` |
|     17 | 7713 | `			iStart = iSpace = iSpace + 1;` |
|      8 | 7714 | `		}` |
|    533 | 7715 | `		iCur++;` |
|      1 | 7716 | `	}` |
|      - | 7717 | `	/* Emit the trailing chunk. */` |
|     19 | 7718 | `	if( iStart < iCur ){` |
|     19 | 7719 | `		rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     19 | 7720 | `		if( rc != SXRET_OK ){ goto oom; }` |
|      9 | 7721 | `	}` |
|     19 | 7722 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|     19 | 7723 | `	SyBlobRelease(&sWorker);` |
|     19 | 7724 | `	return PH7_OK;` |
|    ! 0 | 7725 | `oom:` |
|    ! 0 | 7726 | `	SyBlobRelease(&sWorker);` |
|    ! 0 | 7727 | `	return PH7_ContextMemoryError(pCtx);` |
|     14 | 7728 | `}` |
|      - | 7729 | `/*` |
|      - | 7730 | ` * Check if the given character is a member of the given mask.` |
|      - | 7731 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 7732 | ` * Refer to [strtok()].` |
|      - | 7733 | ` */` |
|     30 | 7734 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 7735 | `{` |
|      - | 7736 | `	int i;` |
|     57 | 7737 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 7738 | `		if( c == zMask[i] ){` |
|     13 | 7739 | `			if( pOfft ){` |
|      5 | 7740 | `				*pOfft = i;` |
|      2 | 7741 | `			}` |
|     13 | 7742 | `			return TRUE;` |
|      - | 7743 | `		}` |
|     14 | 7744 | `	}` |
|     19 | 7745 | `	return FALSE;` |
|     16 | 7746 | `}` |
|      - | 7747 | `/*` |
|      - | 7748 | ` * Extract a single token from the input stream.` |
|      - | 7749 | ` * Refer to [strtok()].` |
|      - | 7750 | ` */` |
|      6 | 7751 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 7752 | `{` |
|      7 | 7753 | `	const char *zIn = *pzIn;` |
|      - | 7754 | `	const char *zPtr;` |
|      - | 7755 | `	/* Ignore leading delimiter */` |
|     11 | 7756 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 7757 | `		zIn++;` |
|      1 | 7758 | `	}` |
|      7 | 7759 | `	if( zIn >= zEnd ){` |
|      - | 7760 | `		/* End of input */` |
|    ! 0 | 7761 | `		return SXERR_EOF;` |
|      - | 7762 | `	}` |
|      7 | 7763 | `	zPtr = zIn;` |
|      - | 7764 | `	/* Extract the token */` |
|     13 | 7765 | `	while( zIn < zEnd ){` |
|     11 | 7766 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 7767 | `			/* UTF-8 stream */` |
|    ! 0 | 7768 | `			zIn++;` |
|    ! 0 | 7769 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 7770 | `		}else{` |
|     11 | 7771 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 7772 | `				break;` |
|      - | 7773 | `			}` |
|      7 | 7774 | `			zIn++;` |
|      - | 7775 | `		}` |
|      1 | 7776 | `	}` |
|      7 | 7777 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 7778 | `	/* Update the cursor */` |
|      7 | 7779 | `	*pzIn = zIn;` |
|      - | 7780 | `	/* Return to the caller */` |
|      7 | 7781 | `	return SXRET_OK;` |
|      4 | 7782 | `}` |
|      - | 7783 | `/* strtok auxiliary private data */` |
|      - | 7784 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 7785 | `struct strtok_aux_data` |
|      - | 7786 | `{` |
|      - | 7787 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 7788 | `	const char *zIn;   /* Current input stream */` |
|      - | 7789 | `	const char *zEnd;  /* End of input */` |
|      - | 7790 | `};` |
|      - | 7791 | `/*` |
|      - | 7792 | ` * string strtok(string $str,string $token)` |
|      - | 7793 | ` * string strtok(string $token)` |
|      - | 7794 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 7795 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 7796 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 7797 | ` *  words by using the space character as the token.` |
|      - | 7798 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 7799 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 7800 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 7801 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 7802 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 7803 | ` *  the argument are found.` |
|      - | 7804 | ` * Parameters` |
|      - | 7805 | ` *  $str` |
|      - | 7806 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 7807 | ` * $token` |
|      - | 7808 | ` *  The delimiter used when splitting up str.` |
|      - | 7809 | ` * Return` |
|      - | 7810 | ` *   Current token or FALSE on EOF.` |
|      - | 7811 | ` */` |
|      6 | 7812 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7813 | `{` |
|      - | 7814 | `	strtok_aux_data *pAux;` |
|      - | 7815 | `	const char *zMask;` |
|      - | 7816 | `	SyString sToken;` |
|      - | 7817 | `	int nMasklen;` |
|      - | 7818 | `	sxi32 rc;` |
|      7 | 7819 | `	if( nArg < 2 ){` |
|      - | 7820 | `		/* Extract top aux data */` |
|      5 | 7821 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      5 | 7822 | `		if( pAux == 0 ){` |
|      - | 7823 | `			/* No aux data,return FALSE */` |
|    ! 0 | 7824 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7825 | `			return PH7_OK;` |
|      - | 7826 | `		}` |
|      5 | 7827 | `		nMasklen = 0;` |
|      5 | 7828 | `		zMask = ""; /* cc warning */` |
|      5 | 7829 | `		if( nArg > 0 ){` |
|      - | 7830 | `			/* Extract the mask */` |
|      5 | 7831 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 7832 | `		}` |
|      5 | 7833 | `		if( nMasklen < 1 ){` |
|      - | 7834 | `			/* Invalid mask,return FALSE */` |
|    ! 0 | 7835 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 7836 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7837 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 7838 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7839 | `			return PH7_OK;` |
|      - | 7840 | `		}` |
|      - | 7841 | `		/* Extract the token */` |
|      5 | 7842 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 7843 | `		if( rc != SXRET_OK ){` |
|      - | 7844 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 7845 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 7846 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7847 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 7848 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7849 | `		}else{` |
|      - | 7850 | `			/* Return the extracted token */` |
|      5 | 7851 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 7852 | `		}` |
|      3 | 7853 | `	}else{` |
|      - | 7854 | `		const char *zInput,*zCur;` |
|      - | 7855 | `		char *zDup;` |
|      - | 7856 | `		int nLen;` |
|      - | 7857 | `		/* Extract the raw input */` |
|      3 | 7858 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 7859 | `		if( nLen < 1 ){` |
|      - | 7860 | `			/* Empty input,return FALSE */` |
|    ! 0 | 7861 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7862 | `			return PH7_OK;` |
|      - | 7863 | `		}` |
|      - | 7864 | `		/* Extract the mask */` |
|      3 | 7865 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 7866 | `		if( nMasklen < 1 ){` |
|      - | 7867 | `			/* Set a default mask */` |
|      - | 7868 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 7869 | `			zMask = TOK_MASK;` |
|    ! 0 | 7870 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 7871 | `#undef TOK_MASK` |
|    ! 0 | 7872 | `		}` |
|      - | 7873 | `		/* Extract a single token */` |
|      3 | 7874 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 7875 | `		if( rc != SXRET_OK ){` |
|      - | 7876 | `			/* Empty input */` |
|    ! 0 | 7877 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7878 | `			return PH7_OK;` |
|    ! 0 | 7879 | `		}else{` |
|      - | 7880 | `			/* Return the extracted token */` |
|      3 | 7881 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 7882 | `		}` |
|      - | 7883 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 7884 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 7885 | `		if( pAux ){` |
|      3 | 7886 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 7887 | `			if( nLen < 1 ){` |
|    ! 0 | 7888 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7889 | `				return PH7_OK;` |
|      - | 7890 | `			}` |
|      - | 7891 | `			/* Duplicate input */` |
|      3 | 7892 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 7893 | `			if( zDup  ){` |
|      3 | 7894 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 7895 | `				/* Register the aux data */` |
|      3 | 7896 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 7897 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 7898 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 7899 | `			}` |
|      1 | 7900 | `		}` |
|      - | 7901 | `	}` |
|      7 | 7902 | `	return PH7_OK;` |
|      4 | 7903 | `}` |
|      - | 7904 | `/*` |
|      - | 7905 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 7906 | ` *  Pad a string to a certain length with another string` |
|      - | 7907 | ` * Parameters` |
|      - | 7908 | ` *  $input` |
|      - | 7909 | ` *   The input string.` |
|      - | 7910 | ` * $pad_length` |
|      - | 7911 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 7912 | ` *   string, no padding takes place.` |
|      - | 7913 | ` * $pad_string` |
|      - | 7914 | ` *   Note:` |
|      - | 7915 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 7916 | ` *    divided by the pad_string's length.` |
|      - | 7917 | ` * $pad_type` |
|      - | 7918 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 7919 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 7920 | ` * Return` |
|      - | 7921 | ` *  The padded string.` |
|      - | 7922 | ` */` |
|     10 | 7923 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7924 | `{` |
|      - | 7925 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 7926 | `	const char *zIn,*zPad;` |
|     11 | 7927 | `	if( nArg < 2 ){` |
|      - | 7928 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7929 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7930 | `		return PH7_OK;` |
|      - | 7931 | `	}` |
|      - | 7932 | `	/* Extract the target string */` |
|     11 | 7933 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7934 | `	/* Padding length */` |
|      - | 7935 | `	{` |
|     11 | 7936 | `		sxi64 iTmp = 0;` |
|     11 | 7937 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_pad",2,"$length","int",&iTmp);` |
|     11 | 7938 | `		if( rcArg != PH7_OK ){` |
|    ! 0 | 7939 | `			return rcArg;` |
|      - | 7940 | `		}` |
|     11 | 7941 | `		iRealPad = iPadlen = (int)iTmp;` |
|      - | 7942 | `	}` |
|     11 | 7943 | `	if( iPadlen > 0 ){` |
|      9 | 7944 | `		iPadlen -= iLen;` |
|      4 | 7945 | `	}` |
|     11 | 7946 | `	if( iPadlen < 1  ){` |
|      - | 7947 | `		/* Return the string verbatim */` |
|      5 | 7948 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      5 | 7949 | `		return PH7_OK;` |
|      - | 7950 | `	}` |
|      7 | 7951 | `	zPad = " "; /* Whitespace padding */` |
|      7 | 7952 | `	iStrpad = (int)sizeof(char);` |
|      7 | 7953 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      7 | 7954 | `	if( nArg > 2 ){` |
|      - | 7955 | `		/* Padding string */` |
|      7 | 7956 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      7 | 7957 | `		if( iStrpad < 1 ){` |
|      - | 7958 | `			/* An empty pad string throws a catchable ValueError in PHP 8` |
|      - | 7959 | `			 * (only reached once padding is actually required). */` |
|      3 | 7960 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7961 | `				"str_pad(): Argument #3 ($pad_string) must not be empty");` |
|      - | 7962 | `		}` |
|      5 | 7963 | `		if( nArg > 3 ){` |
|      - | 7964 | `			/* Padd type */` |
|      5 | 7965 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 7966 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 7967 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 7968 | `			}` |
|      2 | 7969 | `		}` |
|      2 | 7970 | `	}` |
|      5 | 7971 | `	iDiv = 1;` |
|      5 | 7972 | `	if( iType == 2 ){` |
|    ! 0 | 7973 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 7974 | `	}` |
|      - | 7975 | `	/* Perform the requested operation */` |
|      5 | 7976 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 7977 | `		jPad = iStrpad;` |
|      5 | 7978 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 7979 | `			/* Padding */` |
|      5 | 7980 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 7981 | `				break;` |
|      - | 7982 | `			}` |
|      3 | 7983 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7984 | `		}` |
|      3 | 7985 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 7986 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 7987 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 7988 | `				if( jPad > iStrpad ){` |
|    ! 0 | 7989 | `					jPad = iStrpad;` |
|    ! 0 | 7990 | `				}` |
|      3 | 7991 | `				if( jPad < 1){` |
|    ! 0 | 7992 | `					break;` |
|      - | 7993 | `				}` |
|      3 | 7994 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 7995 | `			}` |
|      1 | 7996 | `		}` |
|      1 | 7997 | `	}` |
|      5 | 7998 | `	if( iLen > 0 ){` |
|      - | 7999 | `		/* Append the input string */` |
|      5 | 8000 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 8001 | `	}` |
|      5 | 8002 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 8003 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 8004 | `			/* Padding */` |
|      5 | 8005 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 8006 | `				break;` |
|      - | 8007 | `			}` |
|      3 | 8008 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 8009 | `		}` |
|      5 | 8010 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 8011 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 8012 | `			if( jPad > iStrpad ){` |
|    ! 0 | 8013 | `				jPad = iStrpad;` |
|    ! 0 | 8014 | `			}` |
|      3 | 8015 | `			if( jPad < 1){` |
|    ! 0 | 8016 | `				break;` |
|      - | 8017 | `			}` |
|      3 | 8018 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 8019 | `		}` |
|      1 | 8020 | `	}` |
|      5 | 8021 | `	return PH7_OK;` |
|      6 | 8022 | `}` |
|      - | 8023 | `/*` |
|      - | 8024 | ` * String replacement private data.` |
|      - | 8025 | ` */` |
|      - | 8026 | `typedef struct str_replace_data str_replace_data;` |
|      - | 8027 | `struct str_replace_data` |
|      - | 8028 | `{` |
|      - | 8029 | `	/* Used by the str_replace family to collect the search/replace arguments. */` |
|      - | 8030 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 8031 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 8032 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 8033 | `};` |
|      - | 8034 | `/*` |
|      - | 8035 | ` * Remove a substring.` |
|      - | 8036 | ` */` |
|      - | 8037 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 8038 | `	for(;;){\` |
|      - | 8039 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 8040 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 8041 | `		++OFFT;\` |
|      - | 8042 | `	}\` |
|      - | 8043 | `}` |
|      - | 8044 | `/*` |
|      - | 8045 | ` * Shift right and insert algorithm.` |
|      - | 8046 | ` */` |
|      - | 8047 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 8048 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 8049 | `		for(;;){\` |
|      - | 8050 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 8051 | `			if(INLEN < 1 ) { break; }\` |
|      - | 8052 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 8053 | `			--INLEN; \` |
|      - | 8054 | `		}\` |
|      - | 8055 | `		for(;;){\` |
|      - | 8056 | `				if(ELEN < 1) { break; }\` |
|      - | 8057 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 8058 | `				OFFT++;\` |
|      - | 8059 | `				ENTRY++;\` |
|      - | 8060 | `				--ELEN;\` |
|      - | 8061 | `		}\` |
|      - | 8062 | `}` |
|      - | 8063 | `/*` |
|      - | 8064 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 8065 | ` * replacement string [i.e: zReplace].` |
|      - | 8066 | ` */` |
|     48 | 8067 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      5 | 8068 | `{` |
|     53 | 8069 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 8070 | `	sxu32 n,m;` |
|     53 | 8071 | `	n = SyBlobLength(pWorker);` |
|     53 | 8072 | `	m = nOfft;` |
|      - | 8073 | `	/* Delete the old entry */` |
|   6583 | 8074 | `	STRDEL(zInput,n,m,nLen);` |
|     53 | 8075 | `	SyBlobLength(pWorker) -= nLen;` |
|     53 | 8076 | `	if( nReplen > 0 ){` |
|     47 | 8077 | `		sxi32 iRep = nReplen;` |
|      - | 8078 | `		sxi32 rc;` |
|      - | 8079 | `		/*` |
|      - | 8080 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 8081 | `		 * string.` |
|      - | 8082 | `		 */` |
|     47 | 8083 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     47 | 8084 | `		if( rc != SXRET_OK ){` |
|      - | 8085 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 8086 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 8087 | `			return rc;` |
|      - | 8088 | `		}` |
|      - | 8089 | `		/* Perform the insertion now */` |
|     47 | 8090 | `		zInput = (char *)SyBlobData(pWorker);` |
|     47 | 8091 | `		n = SyBlobLength(pWorker);` |
|   6369 | 8092 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     47 | 8093 | `		SyBlobLength(pWorker) += nReplen;` |
|     21 | 8094 | `	}` |
|     53 | 8095 | `	return SXRET_OK;` |
|     29 | 8096 | `}` |
|      - | 8097 | `/*` |
|      - | 8098 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 8099 | ` * to collect search/replace string.` |
|      - | 8100 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 8101 | ` */` |
|     90 | 8102 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      5 | 8103 | `{` |
|     95 | 8104 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 8105 | `	SyString sWorker;` |
|      - | 8106 | `	const char *zIn;` |
|      - | 8107 | `	int nByte;` |
|      - | 8108 | `	/* Extract a string representation of the given argument */` |
|     95 | 8109 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     95 | 8110 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     95 | 8111 | `	if( nByte > 0 ){` |
|      - | 8112 | `		char *zDup;` |
|      - | 8113 | `		/* Duplicate the chunk */` |
|     93 | 8114 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 8115 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 8116 | `			);` |
|     93 | 8117 | `		if( zDup == 0 ){` |
|      - | 8118 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 8119 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 8120 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 8121 | `			return SXERR_MEM;` |
|      - | 8122 | `		}` |
|     93 | 8123 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 8124 | `		/* Save the chunk */` |
|     93 | 8125 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     44 | 8126 | `	}` |
|      - | 8127 | `	/* Save for later processing */` |
|     95 | 8128 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 8129 | `	/* All done */` |
|     45 | 8130 | `	SXUNUSED(pKey); /* cc warning */` |
|     95 | 8131 | `	return PH7_OK;` |
|     50 | 8132 | `}` |
|      - | 8133 | `/*` |
|      - | 8134 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 8135 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 8136 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 8137 | ` * Parameters` |
|      - | 8138 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 8139 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 8140 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 8141 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 8142 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 8143 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 8144 | ` * $search` |
|      - | 8145 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 8146 | ` *  to designate multiple needles.` |
|      - | 8147 | ` * $replace` |
|      - | 8148 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 8149 | ` *  to designate multiple replacements.` |
|      - | 8150 | ` * $subject` |
|      - | 8151 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 8152 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 8153 | ` *  of subject, and the return value is an array as well.` |
|      - | 8154 | ` * $count (Not used)` |
|      - | 8155 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 8156 | ` * Return` |
|      - | 8157 | ` * This function returns a string or an array with the replaced values.` |
|      - | 8158 | ` */` |
|  29770 | 8159 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 8160 | `{` |
|      - | 8161 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 8162 | `	ProcStringMatch xMatch;` |
|      - | 8163 | `	const char *zIn,*zFunc;` |
|      - | 8164 | `	str_replace_data sRep;` |
|      - | 8165 | `	SyBlob sWorker;` |
|      - | 8166 | `	SySet sReplace;` |
|      - | 8167 | `	SySet sSearch;` |
|      - | 8168 | `	int rep_str;` |
|      - | 8169 | `	int nByte;` |
|      - | 8170 | `	sxi32 rc;` |
|  29775 | 8171 | `	if( nArg < 3 ){` |
|      - | 8172 | `		/* Missing/Invalid arguments,return null */` |
|    ! 0 | 8173 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8174 | `		return PH7_OK;` |
|      - | 8175 | `	}` |
|      - | 8176 | `	/* Initialize fields */` |
|  29775 | 8177 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  29775 | 8178 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  29775 | 8179 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  29775 | 8180 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  29775 | 8181 | `	sRep.pCtx = pCtx;` |
|  29775 | 8182 | `	sRep.pCollector = &sSearch;` |
|  29775 | 8183 | `	rep_str = 0;` |
|      - | 8184 | `	/* Extract the subject */` |
|  29775 | 8185 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  29775 | 8186 | `	if( nByte < 1 ){` |
|      - | 8187 | `		/* Nothing to replace,return the empty string */` |
|     21 | 8188 | `		ph7_result_string(pCtx,"",0);` |
|     21 | 8189 | `		return PH7_OK;` |
|      - | 8190 | `	}` |
|      - | 8191 | `	/* Copy the subject */` |
|  29755 | 8192 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 8193 | `	/* Search string */` |
|  29755 | 8194 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 8195 | `		/* Collect search string */` |
|     45 | 8196 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|     25 | 8197 | `	}else{` |
|      - | 8198 | `		/* Single pattern */` |
|  29715 | 8199 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  29715 | 8200 | `		if( nByte < 1 ){` |
|      - | 8201 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 8202 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 8203 | `			return PH7_OK;` |
|      - | 8204 | `		}` |
|  29711 | 8205 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 8206 | `		/* Save for later processing */` |
|  29711 | 8207 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 8208 | `	}` |
|      - | 8209 | `	/* Replace string */` |
|  29751 | 8210 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 8211 | `		/* Collect replace string */` |
|      7 | 8212 | `		sRep.pCollector = &sReplace;` |
|      7 | 8213 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 8214 | `	}else{` |
|      - | 8215 | `		/* Single needle */` |
|  29745 | 8216 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  29745 | 8217 | `		rep_str = 1;` |
|  29745 | 8218 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 8219 | `		/* Save for later processing */` |
|  29745 | 8220 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 8221 | `	}` |
|      - | 8222 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  29751 | 8223 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 8224 | `		SySetRelease(&sSearch);` |
|    ! 0 | 8225 | `		SySetRelease(&sReplace);` |
|    ! 0 | 8226 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 8227 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8228 | `	}` |
|      - | 8229 | `	/* Reset loop cursors */` |
|  29751 | 8230 | `	SySetResetCursor(&sSearch);` |
|  29751 | 8231 | `	SySetResetCursor(&sReplace);` |
|  29751 | 8232 | `	pReplace = pSearch = 0; /* cc warning */` |
|  29751 | 8233 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 8234 | `	/* Extract function name */` |
|  29751 | 8235 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 8236 | `	/* Set the default pattern match routine */` |
|  29751 | 8237 | `	xMatch = SyBlobSearch;` |
|  29751 | 8238 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 8239 | `		/* Case insensitive pattern match */` |
|     11 | 8240 | `		xMatch = iPatternMatch;` |
|      5 | 8241 | `	}` |
|      - | 8242 | `	/* Start the replace process */` |
|  59537 | 8243 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 8244 | `		sxu32 nCount,nOfft;` |
|  29791 | 8245 | `		if( pSearch->nByte <  1 ){` |
|      - | 8246 | `			/* Empty string,ignore */` |
|      3 | 8247 | `			continue;` |
|      - | 8248 | `		}` |
|      - | 8249 | `		/* Extract the replace string */` |
|  29789 | 8250 | `		if( rep_str ){` |
|  29779 | 8251 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  14892 | 8252 | `		}else{` |
|     11 | 8253 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 8254 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 8255 | `				 * An empty string is used for the rest of replacement values` |
|      - | 8256 | `				 */` |
|      3 | 8257 | `				pReplace = 0;` |
|      1 | 8258 | `			}` |
|      - | 8259 | `		}` |
|  29789 | 8260 | `		if( pReplace == 0 ){` |
|      - | 8261 | `			/* Use an empty string instead */` |
|      3 | 8262 | `			pReplace = &sTemp;` |
|      1 | 8263 | `		}` |
|  29789 | 8264 | `		nOfft = nCount = 0;` |
|  14916 | 8265 | `		for(;;){` |
|  29837 | 8266 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 8267 | `				break;` |
|      - | 8268 | `			}` |
|      - | 8269 | `			/* Perform a pattern lookup */` |
|  44735 | 8270 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  29820 | 8271 | `				pSearch->nByte,&nOfft);` |
|  29825 | 8272 | `			if( rc != SXRET_OK ){` |
|      - | 8273 | `				/* Pattern not found */` |
|  29777 | 8274 | `				break;` |
|      - | 8275 | `			}` |
|      - | 8276 | `			/* Perform the replace operation */` |
|     53 | 8277 | `			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|     53 | 8278 | `			if( rc != SXRET_OK ){` |
|      - | 8279 | `				/* Allocation failure: surface a fatal instead of a partial result */` |
|    ! 0 | 8280 | `				SySetRelease(&sSearch);` |
|    ! 0 | 8281 | `				SySetRelease(&sReplace);` |
|    ! 0 | 8282 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8283 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8284 | `			}` |
|      - | 8285 | `			/* Increment offset counter */` |
|     53 | 8286 | `			nCount += nOfft + pReplace->nByte;` |
|      5 | 8287 | `		}` |
|      5 | 8288 | `	}` |
|      - | 8289 | `	/* All done,clean-up the mess left behind */` |
|  29751 | 8290 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  29751 | 8291 | `	SySetRelease(&sSearch);` |
|  29751 | 8292 | `	SySetRelease(&sReplace);` |
|  29751 | 8293 | `	SyBlobRelease(&sWorker);` |
|  29751 | 8294 | `	if( rc != PH7_OK ){` |
|    ! 0 | 8295 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8296 | `	}` |
|  29751 | 8297 | `	return PH7_OK;` |
|  14890 | 8298 | `}` |
|      - | 8299 | `/*` |
|      - | 8300 | ` * strtr() array form: a single (key,value) pair copied out of the replace_pairs` |
|      - | 8301 | ` * array. The bytes are owned by a persistent pool (see strtr_collect) rather than` |
|      - | 8302 | ` * the transient walker values, which HashmapWalk releases after each callback, so` |
|      - | 8303 | ` * we store byte offsets into that pool instead of raw pointers.` |
|      - | 8304 | ` */` |
|      - | 8305 | `typedef struct strtr_entry strtr_entry;` |
|      - | 8306 | `struct strtr_entry` |
|      - | 8307 | `{` |
|      - | 8308 | `	sxu32 nKeyOfft; /* Offset of the search key inside the pool */` |
|      - | 8309 | `	sxu32 nKeyLen;  /* Length of the search key */` |
|      - | 8310 | `	sxu32 nValOfft; /* Offset of the replacement inside the pool */` |
|      - | 8311 | `	sxu32 nValLen;  /* Length of the replacement */` |
|      - | 8312 | `};` |
|      - | 8313 | `typedef struct strtr_collect strtr_collect;` |
|      - | 8314 | `struct strtr_collect` |
|      - | 8315 | `{` |
|      - | 8316 | `	SyBlob *pPool;  /* Byte pool holding copied key + value bytes */` |
|      - | 8317 | `	SySet  *pTable; /* Set of strtr_entry (parallel offsets into pPool) */` |
|      - | 8318 | `	sxi32   rc;     /* Carries an allocation failure (SXERR_MEM) out of the walker */` |
|      - | 8319 | `};` |
|      - | 8320 | `/*` |
|      - | 8321 | ` * Collect one replace_pairs entry into the persistent pool/offset table.` |
|      - | 8322 | ` * PHP coerces both the key and the value to string (an integer key becomes its` |
|      - | 8323 | ` * decimal form) and ignores an empty-string key.` |
|      - | 8324 | ` */` |
|     20 | 8325 | `static int StrtrCollectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 8326 | `{` |
|     21 | 8327 | `	strtr_collect *pCol = (strtr_collect *)pUserData;` |
|      - | 8328 | `	const char *zKey,*zVal;` |
|      - | 8329 | `	strtr_entry sEnt;` |
|      - | 8330 | `	int nKey,nVal;` |
|     21 | 8331 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|     21 | 8332 | `	if( nKey < 1 ){` |
|      - | 8333 | `		/* PHP ignores an empty-string key (it also emits a warning we do not replicate). */` |
|      3 | 8334 | `		return PH7_OK;` |
|      - | 8335 | `	}` |
|     19 | 8336 | `	zVal = ph7_value_to_string(pData,&nVal);` |
|     19 | 8337 | `	sEnt.nKeyOfft = SyBlobLength(pCol->pPool);` |
|     19 | 8338 | `	sEnt.nKeyLen  = (sxu32)nKey;` |
|     19 | 8339 | `	if( SyBlobAppend(pCol->pPool,(const void *)zKey,(sxu32)nKey) != SXRET_OK ){` |
|    ! 0 | 8340 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8341 | `		return SXERR_ABORT;` |
|      - | 8342 | `	}` |
|     19 | 8343 | `	sEnt.nValOfft = SyBlobLength(pCol->pPool);` |
|     19 | 8344 | `	sEnt.nValLen  = (sxu32)nVal;` |
|     19 | 8345 | `	if( nVal > 0 && SyBlobAppend(pCol->pPool,(const void *)zVal,(sxu32)nVal) != SXRET_OK ){` |
|    ! 0 | 8346 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8347 | `		return SXERR_ABORT;` |
|      - | 8348 | `	}` |
|     19 | 8349 | `	if( SySetPut(pCol->pTable,(const void *)&sEnt) != SXRET_OK ){` |
|    ! 0 | 8350 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8351 | `		return SXERR_ABORT;` |
|      - | 8352 | `	}` |
|     19 | 8353 | `	return PH7_OK;` |
|     11 | 8354 | `}` |
|      - | 8355 | `/*` |
|      - | 8356 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 8357 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 8358 | ` *  Translate characters or replace substrings.` |
|      - | 8359 | ` * Parameters` |
|      - | 8360 | ` *  $str` |
|      - | 8361 | ` *  The string being translated.` |
|      - | 8362 | ` * $from` |
|      - | 8363 | ` *  The string being translated to to.` |
|      - | 8364 | ` * $to` |
|      - | 8365 | ` *  The string replacing from.` |
|      - | 8366 | ` * $replace_pairs` |
|      - | 8367 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 8368 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 8369 | ` * Return` |
|      - | 8370 | ` *  The translated string.` |
|      - | 8371 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 8372 | ` */` |
|     12 | 8373 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8374 | `{` |
|      - | 8375 | `	const char *zIn;` |
|      - | 8376 | `	int nLen;` |
|     13 | 8377 | `	if( nArg < 1 ){` |
|      - | 8378 | `		/* Nothing to replace,return FALSE */` |
|    ! 0 | 8379 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8380 | `		return PH7_OK;` |
|      - | 8381 | `	}` |
|     13 | 8382 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 8383 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 8384 | `		/* Invalid arguments */` |
|    ! 0 | 8385 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8386 | `		return PH7_OK;` |
|      - | 8387 | `	}` |
|     18 | 8388 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 8389 | `		strtr_collect sCol;` |
|      - | 8390 | `		SyBlob sPool,sWorker;` |
|      - | 8391 | `		SySet sTable;` |
|      - | 8392 | `		const char *zPool;` |
|      - | 8393 | `		strtr_entry *pEnt;` |
|      - | 8394 | `		sxi32 rc;` |
|      - | 8395 | `		int i,iRun;` |
|      - | 8396 | `		/*` |
|      - | 8397 | `		 * PHP's array-form strtr is a single left-to-right pass over the subject:` |
|      - | 8398 | `		 * at every position it substitutes the LONGEST replace_pairs key that` |
|      - | 8399 | `		 * matches there, then advances past the key (replacements are never` |
|      - | 8400 | `		 * rescanned). It is not a sequential per-key global replace. First copy` |
|      - | 8401 | `		 * the pairs into a persistent pool, then run that scan.` |
|      - | 8402 | `		 */` |
|     11 | 8403 | `		SyBlobInit(&sPool,&pCtx->pVm->sAllocator);` |
|     11 | 8404 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     11 | 8405 | `		SySetInit(&sTable,&pCtx->pVm->sAllocator,sizeof(strtr_entry));` |
|     11 | 8406 | `		sCol.pPool  = &sPool;` |
|     11 | 8407 | `		sCol.pTable = &sTable;` |
|     11 | 8408 | `		sCol.rc     = SXRET_OK;` |
|     11 | 8409 | `		ph7_array_walk(apArg[1],StrtrCollectWalker,&sCol);` |
|     11 | 8410 | `		if( sCol.rc != SXRET_OK ){` |
|      - | 8411 | `			/* Allocation failure while collecting the pairs: surface a fatal */` |
|    ! 0 | 8412 | `			SyBlobRelease(&sPool);` |
|    ! 0 | 8413 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 8414 | `			SySetRelease(&sTable);` |
|    ! 0 | 8415 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 8416 | `		}` |
|      - | 8417 | `		/* The pool is now stable, so offsets can be resolved against its base. */` |
|     11 | 8418 | `		zPool = (const char *)SyBlobData(&sPool);` |
|     11 | 8419 | `		rc = SXRET_OK;` |
|     11 | 8420 | `		iRun = 0; /* Start of the pending run of unmatched bytes copied verbatim. */` |
|     43 | 8421 | `		for( i = 0 ; i < nLen ; ){` |
|     33 | 8422 | `			strtr_entry *pBest = 0;` |
|     33 | 8423 | `			sxu32 nBest = 0;` |
|      - | 8424 | `			/* Pick the longest key that matches at the current position. */` |
|     33 | 8425 | `			SySetResetCursor(&sTable);` |
|     87 | 8426 | `			while( SXRET_OK == SySetGetNextEntry(&sTable,(void **)&pEnt) ){` |
|     54 | 8427 | `				if( pEnt->nKeyLen > nBest` |
|     50 | 8428 | `					&& pEnt->nKeyLen <= (sxu32)(nLen - i)` |
|     46 | 8429 | `					&& SyMemcmp(zPool + pEnt->nKeyOfft,zIn + i,pEnt->nKeyLen) == 0 ){` |
|     29 | 8430 | `					nBest = pEnt->nKeyLen;` |
|     29 | 8431 | `					pBest = pEnt;` |
|     14 | 8432 | `				}` |
|      1 | 8433 | `			}` |
|     33 | 8434 | `			if( pBest == 0 ){` |
|      - | 8435 | `				/* No key here: extend the literal run and copy it in one shot later. */` |
|      9 | 8436 | `				i++;` |
|      9 | 8437 | `				continue;` |
|      - | 8438 | `			}` |
|      - | 8439 | `			/* Flush the pending literal run, then the replacement. */` |
|     25 | 8440 | `			if( i > iRun ){` |
|      5 | 8441 | `				rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(i - iRun));` |
|      2 | 8442 | `			}` |
|     25 | 8443 | `			if( rc == SXRET_OK && pBest->nValLen > 0 ){` |
|     25 | 8444 | `				rc = SyBlobAppend(&sWorker,zPool + pBest->nValOfft,pBest->nValLen);` |
|     12 | 8445 | `			}` |
|     25 | 8446 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8447 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 8448 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8449 | `				SySetRelease(&sTable);` |
|    ! 0 | 8450 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8451 | `			}` |
|     25 | 8452 | `			i += (int)pBest->nKeyLen;` |
|     25 | 8453 | `			iRun = i;` |
|      1 | 8454 | `		}` |
|      - | 8455 | `		/* Flush the trailing literal run. */` |
|     11 | 8456 | `		if( nLen > iRun ){` |
|      3 | 8457 | `			rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(nLen - iRun));` |
|      3 | 8458 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8459 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 8460 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8461 | `				SySetRelease(&sTable);` |
|    ! 0 | 8462 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8463 | `			}` |
|      1 | 8464 | `		}` |
|      - | 8465 | `		/* All done, return the result string */` |
|     16 | 8466 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|     10 | 8467 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 8468 | `		/* Clean-up */` |
|     11 | 8469 | `		SyBlobRelease(&sPool);` |
|     11 | 8470 | `		SyBlobRelease(&sWorker);` |
|     11 | 8471 | `		SySetRelease(&sTable);` |
|     11 | 8472 | `		if( rc != PH7_OK ){` |
|    ! 0 | 8473 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 8474 | `		}` |
|      6 | 8475 | `	}else{` |
|      - | 8476 | `		int i,flen,tlen,c,iOfft;` |
|      - | 8477 | `		const char *zFrom,*zTo;` |
|      3 | 8478 | `		if( nArg < 3 ){` |
|      - | 8479 | `			/* Nothing to replace */` |
|    ! 0 | 8480 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8481 | `			return PH7_OK;` |
|      - | 8482 | `		}` |
|      - | 8483 | `		/* Extract given arguments */` |
|      3 | 8484 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 8485 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 8486 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 8487 | `			/* Nothing to replace */` |
|    ! 0 | 8488 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8489 | `			return PH7_OK;` |
|      - | 8490 | `		}` |
|      - | 8491 | `		/* Start the replace process */` |
|     13 | 8492 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 8493 | `			c = zIn[i];` |
|     11 | 8494 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 8495 | `				if ( iOfft < tlen ){` |
|      5 | 8496 | `					c = zTo[iOfft];` |
|      2 | 8497 | `				}` |
|      2 | 8498 | `			}` |
|     11 | 8499 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 8500 |  |
|      6 | 8501 | `		}` |
|      - | 8502 | `	}` |
|     13 | 8503 | `	return PH7_OK;` |
|      7 | 8504 | `}` |
|      - | 8505 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8506 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8507 | `/*` |
|      - | 8508 | ` * Parse an INI string.` |
|      - | 8509 |  |
|      - | 8510 | ` * According to wikipedia` |
|      - | 8511 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 8512 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 8513 | ` *  Format` |
|      - | 8514 | `*    Properties` |
|      - | 8515 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 8516 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 8517 | `*     Example:` |
|      - | 8518 | `*      name=value` |
|      - | 8519 | `*    Sections` |
|      - | 8520 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 8521 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 8522 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 8523 | `*     or the end of the file. Sections may not be nested.` |
|      - | 8524 | `*     Example:` |
|      - | 8525 | `*      [section]` |
|      - | 8526 | `*   Comments` |
|      - | 8527 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 8528 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 8529 | `*/` |
|     12 | 8530 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 8531 | `{` |
|      - | 8532 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 8533 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 8534 | `	SyHashEntry *pEntry;` |
|      - | 8535 | `	SyString sEntry;` |
|      - | 8536 | `	SyHash sHash;` |
|      - | 8537 | `	int c;` |
|      - | 8538 | `	/* Create an empty array and worker variables */` |
|     13 | 8539 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 8540 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 8541 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 8542 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 8543 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|    ! 0 | 8544 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8545 | `	}` |
|     13 | 8546 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 8547 | `	pCur = pArray;` |
|      - | 8548 | `	/* Start the parse process */` |
|     21 | 8549 | `	for(;;){` |
|      - | 8550 | `		/* Ignore leading white spaces */` |
|     69 | 8551 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 8552 | `			zIn++;` |
|      1 | 8553 | `		}` |
|     43 | 8554 | `		if( zIn >= zEnd ){` |
|      - | 8555 | `			/* No more input to process */` |
|     13 | 8556 | `			break;` |
|      - | 8557 | `		}` |
|     31 | 8558 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 8559 | `			/* Comment til the end of line */` |
|    ! 0 | 8560 | `			zIn++;` |
|    ! 0 | 8561 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 8562 | `				zIn++;` |
|    ! 0 | 8563 | `			}` |
|    ! 0 | 8564 | `			continue;` |
|      - | 8565 | `		}` |
|      - | 8566 | `		/* Reset the string cursor of the working variable */` |
|     31 | 8567 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 8568 | `		if( zIn[0] == '[' ){` |
|      - | 8569 | `			/* Section: Extract the section name */` |
|      9 | 8570 | `			zIn++;` |
|      9 | 8571 | `			zCur = zIn;` |
|     73 | 8572 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 8573 | `				zIn++;` |
|      1 | 8574 | `			}` |
|      9 | 8575 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 8576 | `				/* Save the section name */` |
|      5 | 8577 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 8578 | `				SyStringFullTrim(&sEntry);` |
|      5 | 8579 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 8580 | `				if( sEntry.nByte > 0 ){` |
|      - | 8581 | `					/* Associate an array with the section */` |
|      5 | 8582 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 8583 | `					if( pSection ){` |
|      5 | 8584 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 8585 | `						pCur = pSection;` |
|      2 | 8586 | `					}` |
|      2 | 8587 | `				}` |
|      2 | 8588 | `			}` |
|      9 | 8589 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 8590 | `		}else{` |
|      - | 8591 | `			ph7_value *pOldCur;` |
|      - | 8592 | `			int is_array;` |
|      - | 8593 | `			int iLen;` |
|      - | 8594 | `			/* Properties */` |
|     23 | 8595 | `			is_array = 0;` |
|     23 | 8596 | `			zCur = zIn;` |
|     23 | 8597 | `			iLen = 0; /* cc warning */` |
|     23 | 8598 | `			pOldCur = pCur;` |
|    155 | 8599 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 8600 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 8601 | `					/* Array */` |
|    ! 0 | 8602 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 8603 | `					is_array = 1;` |
|    ! 0 | 8604 | `					if( iLen > 0 ){` |
|    ! 0 | 8605 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 8606 | `						/* Query the hashtable */` |
|    ! 0 | 8607 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 8608 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 8609 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 8610 | `						if( pEntry ){` |
|    ! 0 | 8611 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 8612 | `						}else{` |
|      - | 8613 | `							/* Create an empty array */` |
|    ! 0 | 8614 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 8615 | `							if( pvArr ){` |
|      - | 8616 | `								/* Save the entry */` |
|    ! 0 | 8617 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 8618 | `								/* Insert the entry */` |
|    ! 0 | 8619 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 8620 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 8621 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 8622 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 8623 | `							}` |
|      - | 8624 | `						}` |
|    ! 0 | 8625 | `						if( pvArr ){` |
|    ! 0 | 8626 | `							pCur = pvArr;` |
|    ! 0 | 8627 | `						}` |
|    ! 0 | 8628 | `					}` |
|    ! 0 | 8629 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 8630 | `						zIn++;` |
|    ! 0 | 8631 | `					}` |
|    ! 0 | 8632 | `				}` |
|    133 | 8633 | `				zIn++;` |
|      1 | 8634 | `			}` |
|     23 | 8635 | `			if( !is_array ){` |
|     23 | 8636 | `				iLen = (int)(zIn-zCur);` |
|     11 | 8637 | `			}` |
|      - | 8638 | `			/* Trim the key */` |
|     23 | 8639 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 8640 | `			SyStringFullTrim(&sEntry);` |
|     23 | 8641 | `			if( sEntry.nByte > 0 ){` |
|     23 | 8642 | `				if( !is_array ){` |
|      - | 8643 | `					/* Save the key name */` |
|     23 | 8644 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 8645 | `				}` |
|      - | 8646 | `				/* extract key value */` |
|     23 | 8647 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 8648 | `				zIn++; /* '=' */` |
|     39 | 8649 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 8650 | `					zIn++;` |
|      1 | 8651 | `				}` |
|     23 | 8652 | `				if( zIn < zEnd ){` |
|     21 | 8653 | `					zCur = zIn;` |
|     21 | 8654 | `					c = zIn[0];` |
|     21 | 8655 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 8656 | `						zIn++;` |
|      - | 8657 | `						/* Delimit the value */` |
|    ! 0 | 8658 | `						while( zIn < zEnd ){` |
|    ! 0 | 8659 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 8660 | `								break;` |
|      - | 8661 | `							}` |
|    ! 0 | 8662 | `							zIn++;` |
|    ! 0 | 8663 | `						}` |
|    ! 0 | 8664 | `						if( zIn < zEnd ){` |
|    ! 0 | 8665 | `							zIn++;` |
|    ! 0 | 8666 | `						}` |
|    ! 0 | 8667 | `					}else{` |
|    125 | 8668 | `						while( zIn < zEnd ){` |
|    123 | 8669 | `							if( zIn[0] == '\n' ){` |
|     19 | 8670 | `								if( zIn[-1] != '\\' ){` |
|     19 | 8671 | `									break;` |
|    ! 0 | 8672 | `								}` |
|    105 | 8673 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 8674 | `								/* Inline comments */` |
|    ! 0 | 8675 | `								break;` |
|      - | 8676 | `							}` |
|    105 | 8677 | `							zIn++;` |
|      1 | 8678 | `						}` |
|      - | 8679 | `					}` |
|      - | 8680 | `					/* Trim the value */` |
|     21 | 8681 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 8682 | `					SyStringFullTrim(&sEntry);` |
|     21 | 8683 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 8684 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 8685 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 8686 | `					}` |
|     21 | 8687 | `					if( sEntry.nByte > 0 ){` |
|     21 | 8688 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 8689 | `					}` |
|      - | 8690 | `					/* Insert the key and it's value */` |
|     21 | 8691 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 8692 | `				}` |
|     12 | 8693 | `			}else{` |
|    ! 0 | 8694 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 8695 | `					zIn++;` |
|    ! 0 | 8696 | `				}` |
|      - | 8697 | `			}` |
|     23 | 8698 | `			pCur = pOldCur;` |
|      - | 8699 | `		}` |
|      1 | 8700 | `	}` |
|     13 | 8701 | `	SyHashRelease(&sHash);` |
|      - | 8702 | `	/* Return the parse of the INI string */` |
|     13 | 8703 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 8704 | `	return SXRET_OK;` |
|      7 | 8705 | `}` |
|      - | 8706 | `/*` |
|      - | 8707 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 8708 | ` *  Parse a configuration string.` |
|      - | 8709 | ` * Parameters` |
|      - | 8710 | ` *  $ini` |
|      - | 8711 | ` *   The contents of the ini file being parsed.` |
|      - | 8712 | ` *  $process_sections` |
|      - | 8713 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 8714 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 8715 | ` *  $scanner_mode (Not used)` |
|      - | 8716 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 8717 | ` *   then option values will not be parsed.` |
|      - | 8718 | ` * Return` |
|      - | 8719 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 8720 | ` */` |
|     10 | 8721 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8722 | `{` |
|      - | 8723 | `	const char *zIni;` |
|      - | 8724 | `	int nByte;` |
|     11 | 8725 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8726 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 8727 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8728 | `		return PH7_OK;` |
|      - | 8729 | `	}` |
|      - | 8730 | `	/* Extract the raw INI buffer */` |
|     11 | 8731 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 8732 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|     11 | 8733 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      6 | 8734 | `}` |
|      - | 8735 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8736 |  |
|      - | 8737 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8738 |  |
|      - | 8739 | `/*` |
|      - | 8740 | ` * Ctype Functions.` |
|      - | 8741 | ` * Status:` |
|      - | 8742 | ` *    Stable.` |
|      - | 8743 | ` */` |
|      - | 8744 | `/*` |
|      - | 8745 | ` * bool ctype_alnum(string $text)` |
|      - | 8746 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 8747 | ` * Parameters` |
|      - | 8748 | ` *  $text` |
|      - | 8749 | ` *   The tested string.` |
|      - | 8750 | ` * Return` |
|      - | 8751 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 8752 | ` */` |
|     14 | 8753 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8754 | `{` |
|      - | 8755 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8756 | `	int nLen;` |
|     15 | 8757 | `	if( nArg < 1 ){` |
|      - | 8758 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8759 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8760 | `		return PH7_OK;` |
|      - | 8761 | `	}` |
|      - | 8762 | `	/* Extract the target string */` |
|     15 | 8763 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 8764 | `	zEnd = &zIn[nLen];` |
|     15 | 8765 | `	if( nLen < 1 ){` |
|      - | 8766 | `		/* Empty string,return FALSE */` |
|      3 | 8767 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8768 | `		return PH7_OK;` |
|      - | 8769 | `	}` |
|      - | 8770 | `	/* Perform the requested operation */` |
|     32 | 8771 | `	for(;;){` |
|     65 | 8772 | `		if( zIn >= zEnd ){` |
|      - | 8773 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8774 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8775 | `			return PH7_OK;` |
|      - | 8776 | `		}` |
|     57 | 8777 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 8778 | `			break;` |
|      - | 8779 | `		}` |
|      - | 8780 | `		/* Point to the next character */` |
|     53 | 8781 | `		zIn++;` |
|      1 | 8782 | `	}` |
|      - | 8783 | `	/* The test failed,return FALSE */` |
|      5 | 8784 | `	ph7_result_bool(pCtx,0);` |
|      5 | 8785 | `	return PH7_OK;` |
|      8 | 8786 | `}` |
|      - | 8787 | `/*` |
|      - | 8788 | ` * bool ctype_alpha(string $text)` |
|      - | 8789 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 8790 | ` * Parameters` |
|      - | 8791 | ` *  $text` |
|      - | 8792 | ` *   The tested string.` |
|      - | 8793 | ` * Return` |
|      - | 8794 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 8795 | ` */` |
|     16 | 8796 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8797 | `{` |
|      - | 8798 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8799 | `	int nLen;` |
|     17 | 8800 | `	if( nArg < 1 ){` |
|      - | 8801 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8802 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8803 | `		return PH7_OK;` |
|      - | 8804 | `	}` |
|      - | 8805 | `	/* Extract the target string */` |
|     17 | 8806 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8807 | `	zEnd = &zIn[nLen];` |
|     17 | 8808 | `	if( nLen < 1 ){` |
|      - | 8809 | `		/* Empty string,return FALSE */` |
|      3 | 8810 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8811 | `		return PH7_OK;` |
|      - | 8812 | `	}` |
|      - | 8813 | `	/* Perform the requested operation */` |
|     42 | 8814 | `	for(;;){` |
|     85 | 8815 | `		if( zIn >= zEnd ){` |
|      - | 8816 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8817 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8818 | `			return PH7_OK;` |
|      - | 8819 | `		}` |
|     77 | 8820 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 8821 | `			break;` |
|      - | 8822 | `		}` |
|      - | 8823 | `		/* Point to the next character */` |
|     71 | 8824 | `		zIn++;` |
|      1 | 8825 | `	}` |
|      - | 8826 | `	/* The test failed,return FALSE */` |
|      7 | 8827 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8828 | `	return PH7_OK;` |
|      9 | 8829 | `}` |
|      - | 8830 | `/*` |
|      - | 8831 | ` * bool ctype_cntrl(string $text)` |
|      - | 8832 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 8833 | ` * Parameters` |
|      - | 8834 | ` *  $text` |
|      - | 8835 | ` *   The tested string.` |
|      - | 8836 | ` * Return` |
|      - | 8837 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 8838 | ` */` |
|     16 | 8839 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8840 | `{` |
|      - | 8841 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8842 | `	int nLen;` |
|     17 | 8843 | `	if( nArg < 1 ){` |
|      - | 8844 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8845 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8846 | `		return PH7_OK;` |
|      - | 8847 | `	}` |
|      - | 8848 | `	/* Extract the target string */` |
|     17 | 8849 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8850 | `	zEnd = &zIn[nLen];` |
|     17 | 8851 | `	if( nLen < 1 ){` |
|      - | 8852 | `		/* Empty string,return FALSE */` |
|      3 | 8853 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8854 | `		return PH7_OK;` |
|      - | 8855 | `	}` |
|      - | 8856 | `	/* Perform the requested operation */` |
|     14 | 8857 | `	for(;;){` |
|     29 | 8858 | `		if( zIn >= zEnd ){` |
|      - | 8859 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8860 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8861 | `			return PH7_OK;` |
|      - | 8862 | `		}` |
|     21 | 8863 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8864 | `			/* UTF-8 stream  */` |
|    ! 0 | 8865 | `			break;` |
|      - | 8866 | `		}` |
|     21 | 8867 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 8868 | `			break;` |
|      - | 8869 | `		}` |
|      - | 8870 | `		/* Point to the next character */` |
|     15 | 8871 | `		zIn++;` |
|      1 | 8872 | `	}` |
|      - | 8873 | `	/* The test failed,return FALSE */` |
|      7 | 8874 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8875 | `	return PH7_OK;` |
|      9 | 8876 | `}` |
|      - | 8877 | `/*` |
|      - | 8878 | ` * bool ctype_digit(string $text)` |
|      - | 8879 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 8880 | ` * Parameters` |
|      - | 8881 | ` *  $text` |
|      - | 8882 | ` *   The tested string.` |
|      - | 8883 | ` * Return` |
|      - | 8884 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 8885 | ` */` |
|   2089 | 8886 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 8887 | `{` |
|      - | 8888 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8889 | `	int nLen;` |
|   2094 | 8890 | `	if( nArg < 1 ){` |
|      - | 8891 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8892 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8893 | `		return PH7_OK;` |
|      - | 8894 | `	}` |
|      - | 8895 | `	/* Extract the target string */` |
|   2094 | 8896 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   2094 | 8897 | `	zEnd = &zIn[nLen];` |
|   2094 | 8898 | `	if( nLen < 1 ){` |
|      - | 8899 | `		/* Empty string,return FALSE */` |
|      3 | 8900 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8901 | `		return PH7_OK;` |
|      - | 8902 | `	}` |
|      - | 8903 | `	/* Perform the requested operation */` |
|   1930 | 8904 | `	for(;;){` |
|   3863 | 8905 | `		if( zIn >= zEnd ){` |
|      - | 8906 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1726 | 8907 | `			ph7_result_bool(pCtx,1);` |
|   1726 | 8908 | `			return PH7_OK;` |
|      - | 8909 | `		}` |
|   2142 | 8910 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8911 | `			/* UTF-8 stream  */` |
|    ! 0 | 8912 | `			break;` |
|      - | 8913 | `		}` |
|   2142 | 8914 | `		if( !SyisDigit(zIn[0]) ){` |
|    371 | 8915 | `			break;` |
|      - | 8916 | `		}` |
|      - | 8917 | `		/* Point to the next character */` |
|   1776 | 8918 | `		zIn++;` |
|      5 | 8919 | `	}` |
|      - | 8920 | `	/* The test failed,return FALSE */` |
|    371 | 8921 | `	ph7_result_bool(pCtx,0);` |
|    371 | 8922 | `	return PH7_OK;` |
|   1050 | 8923 | `}` |
|      - | 8924 | `/*` |
|      - | 8925 | ` * bool ctype_xdigit(string $text)` |
|      - | 8926 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 8927 | ` * Parameters` |
|      - | 8928 | ` *  $text` |
|      - | 8929 | ` *   The tested string.` |
|      - | 8930 | ` * Return` |
|      - | 8931 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 8932 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 8933 | ` */` |
|     18 | 8934 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8935 | `{` |
|      - | 8936 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8937 | `	int nLen;` |
|     19 | 8938 | `	if( nArg < 1 ){` |
|      - | 8939 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8940 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8941 | `		return PH7_OK;` |
|      - | 8942 | `	}` |
|      - | 8943 | `	/* Extract the target string */` |
|     19 | 8944 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 8945 | `	zEnd = &zIn[nLen];` |
|     19 | 8946 | `	if( nLen < 1 ){` |
|      - | 8947 | `		/* Empty string,return FALSE */` |
|      3 | 8948 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8949 | `		return PH7_OK;` |
|      - | 8950 | `	}` |
|      - | 8951 | `	/* Perform the requested operation */` |
|     46 | 8952 | `	for(;;){` |
|     93 | 8953 | `		if( zIn >= zEnd ){` |
|      - | 8954 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 8955 | `			ph7_result_bool(pCtx,1);` |
|     11 | 8956 | `			return PH7_OK;` |
|      - | 8957 | `		}` |
|     83 | 8958 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8959 | `			/* UTF-8 stream  */` |
|    ! 0 | 8960 | `			break;` |
|      - | 8961 | `		}` |
|     83 | 8962 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 8963 | `			break;` |
|      - | 8964 | `		}` |
|      - | 8965 | `		/* Point to the next character */` |
|     77 | 8966 | `		zIn++;` |
|      1 | 8967 | `	}` |
|      - | 8968 | `	/* The test failed,return FALSE */` |
|      7 | 8969 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8970 | `	return PH7_OK;` |
|     10 | 8971 | `}` |
|      - | 8972 | `/*` |
|      - | 8973 | ` * bool ctype_graph(string $text)` |
|      - | 8974 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 8975 | ` * Parameters` |
|      - | 8976 | ` *  $text` |
|      - | 8977 | ` *   The tested string.` |
|      - | 8978 | ` * Return` |
|      - | 8979 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 8980 | ` * (no white space), FALSE otherwise.` |
|      - | 8981 | ` */` |
|     16 | 8982 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8983 | `{` |
|      - | 8984 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8985 | `	int nLen;` |
|     17 | 8986 | `	if( nArg < 1 ){` |
|      - | 8987 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8988 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8989 | `		return PH7_OK;` |
|      - | 8990 | `	}` |
|      - | 8991 | `	/* Extract the target string */` |
|     17 | 8992 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8993 | `	zEnd = &zIn[nLen];` |
|     17 | 8994 | `	if( nLen < 1 ){` |
|      - | 8995 | `		/* Empty string,return FALSE */` |
|      3 | 8996 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8997 | `		return PH7_OK;` |
|      - | 8998 | `	}` |
|      - | 8999 | `	/* Perform the requested operation */` |
|     57 | 9000 | `	for(;;){` |
|    115 | 9001 | `		if( zIn >= zEnd ){` |
|      - | 9002 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 9003 | `			ph7_result_bool(pCtx,1);` |
|      9 | 9004 | `			return PH7_OK;` |
|      - | 9005 | `		}` |
|    107 | 9006 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9007 | `			/* UTF-8 stream  */` |
|    ! 0 | 9008 | `			break;` |
|      - | 9009 | `		}` |
|    107 | 9010 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 9011 | `			break;` |
|      - | 9012 | `		}` |
|      - | 9013 | `		/* Point to the next character */` |
|    101 | 9014 | `		zIn++;` |
|      1 | 9015 | `	}` |
|      - | 9016 | `	/* The test failed,return FALSE */` |
|      7 | 9017 | `	ph7_result_bool(pCtx,0);` |
|      7 | 9018 | `	return PH7_OK;` |
|      9 | 9019 | `}` |
|      - | 9020 | `/*` |
|      - | 9021 | ` * bool ctype_print(string $text)` |
|      - | 9022 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 9023 | ` * Parameters` |
|      - | 9024 | ` *  $text` |
|      - | 9025 | ` *   The tested string.` |
|      - | 9026 | ` * Return` |
|      - | 9027 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 9028 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 9029 | ` *  or control function at all.` |
|      - | 9030 | ` */` |
|     16 | 9031 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9032 | `{` |
|      - | 9033 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9034 | `	int nLen;` |
|     17 | 9035 | `	if( nArg < 1 ){` |
|      - | 9036 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9037 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9038 | `		return PH7_OK;` |
|      - | 9039 | `	}` |
|      - | 9040 | `	/* Extract the target string */` |
|     17 | 9041 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9042 | `	zEnd = &zIn[nLen];` |
|     17 | 9043 | `	if( nLen < 1 ){` |
|      - | 9044 | `		/* Empty string,return FALSE */` |
|      3 | 9045 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9046 | `		return PH7_OK;` |
|      - | 9047 | `	}` |
|      - | 9048 | `	/* Perform the requested operation */` |
|     63 | 9049 | `	for(;;){` |
|    127 | 9050 | `		if( zIn >= zEnd ){` |
|      - | 9051 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 9052 | `			ph7_result_bool(pCtx,1);` |
|      9 | 9053 | `			return PH7_OK;` |
|      - | 9054 | `		}` |
|    119 | 9055 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9056 | `			/* UTF-8 stream  */` |
|    ! 0 | 9057 | `			break;` |
|      - | 9058 | `		}` |
|    119 | 9059 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 9060 | `			break;` |
|      - | 9061 | `		}` |
|      - | 9062 | `		/* Point to the next character */` |
|    113 | 9063 | `		zIn++;` |
|      1 | 9064 | `	}` |
|      - | 9065 | `	/* The test failed,return FALSE */` |
|      7 | 9066 | `	ph7_result_bool(pCtx,0);` |
|      7 | 9067 | `	return PH7_OK;` |
|      9 | 9068 | `}` |
|      - | 9069 | `/*` |
|      - | 9070 | ` * bool ctype_punct(string $text)` |
|      - | 9071 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 9072 | ` * Parameters` |
|      - | 9073 | ` *  $text` |
|      - | 9074 | ` *   The tested string.` |
|      - | 9075 | ` * Return` |
|      - | 9076 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 9077 | ` *  digit or blank, FALSE otherwise.` |
|      - | 9078 | ` */` |
|     18 | 9079 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9080 | `{` |
|      - | 9081 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9082 | `	int nLen;` |
|     19 | 9083 | `	if( nArg < 1 ){` |
|      - | 9084 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9085 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9086 | `		return PH7_OK;` |
|      - | 9087 | `	}` |
|      - | 9088 | `	/* Extract the target string */` |
|     19 | 9089 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 9090 | `	zEnd = &zIn[nLen];` |
|     19 | 9091 | `	if( nLen < 1 ){` |
|      - | 9092 | `		/* Empty string,return FALSE */` |
|      3 | 9093 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9094 | `		return PH7_OK;` |
|      - | 9095 | `	}` |
|      - | 9096 | `	/* Perform the requested operation */` |
|     38 | 9097 | `	for(;;){` |
|     77 | 9098 | `		if( zIn >= zEnd ){` |
|      - | 9099 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 9100 | `			ph7_result_bool(pCtx,1);` |
|      9 | 9101 | `			return PH7_OK;` |
|      - | 9102 | `		}` |
|     69 | 9103 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9104 | `			/* UTF-8 stream  */` |
|    ! 0 | 9105 | `			break;` |
|      - | 9106 | `		}` |
|     69 | 9107 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 9108 | `			break;` |
|      - | 9109 | `		}` |
|      - | 9110 | `		/* Point to the next character */` |
|     61 | 9111 | `		zIn++;` |
|      1 | 9112 | `	}` |
|      - | 9113 | `	/* The test failed,return FALSE */` |
|      9 | 9114 | `	ph7_result_bool(pCtx,0);` |
|      9 | 9115 | `	return PH7_OK;` |
|     10 | 9116 | `}` |
|      - | 9117 | `/*` |
|      - | 9118 | ` * bool ctype_space(string $text)` |
|      - | 9119 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 9120 | ` * Parameters` |
|      - | 9121 | ` *  $text` |
|      - | 9122 | ` *   The tested string.` |
|      - | 9123 | ` * Return` |
|      - | 9124 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 9125 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 9126 | ` *  and form feed characters.` |
|      - | 9127 | ` */` |
|  64464 | 9128 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 9129 | `{` |
|      - | 9130 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9131 | `	int nLen;` |
|  64469 | 9132 | `	if( nArg < 1 ){` |
|      - | 9133 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9134 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9135 | `		return PH7_OK;` |
|      - | 9136 | `	}` |
|      - | 9137 | `	/* Extract the target string */` |
|  64469 | 9138 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  64469 | 9139 | `	zEnd = &zIn[nLen];` |
|  64469 | 9140 | `	if( nLen < 1 ){` |
|      - | 9141 | `		/* Empty string,return FALSE */` |
|      3 | 9142 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9143 | `		return PH7_OK;` |
|      - | 9144 | `	}` |
|      - | 9145 | `	/* Perform the requested operation */` |
|  33146 | 9146 | `	for(;;){` |
|  66249 | 9147 | `		if( zIn >= zEnd ){` |
|      - | 9148 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1763 | 9149 | `			ph7_result_bool(pCtx,1);` |
|   1763 | 9150 | `			return PH7_OK;` |
|      - | 9151 | `		}` |
|  64491 | 9152 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9153 | `			/* UTF-8 stream  */` |
|    ! 0 | 9154 | `			break;` |
|      - | 9155 | `		}` |
|  64491 | 9156 | `		if( !SyisSpace(zIn[0]) ){` |
|  62709 | 9157 | `			break;` |
|      - | 9158 | `		}` |
|      - | 9159 | `		/* Point to the next character */` |
|   1787 | 9160 | `		zIn++;` |
|      5 | 9161 | `	}` |
|      - | 9162 | `	/* The test failed,return FALSE */` |
|  62709 | 9163 | `	ph7_result_bool(pCtx,0);` |
|  62709 | 9164 | `	return PH7_OK;` |
|  32261 | 9165 | `}` |
|      - | 9166 | `/*` |
|      - | 9167 | ` * bool ctype_lower(string $text)` |
|      - | 9168 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 9169 | ` * Parameters` |
|      - | 9170 | ` *  $text` |
|      - | 9171 | ` *   The tested string.` |
|      - | 9172 | ` * Return` |
|      - | 9173 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 9174 | ` */` |
|     16 | 9175 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9176 | `{` |
|      - | 9177 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9178 | `	int nLen;` |
|     17 | 9179 | `	if( nArg < 1 ){` |
|      - | 9180 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9181 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9182 | `		return PH7_OK;` |
|      - | 9183 | `	}` |
|      - | 9184 | `	/* Extract the target string */` |
|     17 | 9185 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9186 | `	zEnd = &zIn[nLen];` |
|     17 | 9187 | `	if( nLen < 1 ){` |
|      - | 9188 | `		/* Empty string,return FALSE */` |
|      3 | 9189 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9190 | `		return PH7_OK;` |
|      - | 9191 | `	}` |
|      - | 9192 | `	/* Perform the requested operation */` |
|     27 | 9193 | `	for(;;){` |
|     55 | 9194 | `		if( zIn >= zEnd ){` |
|      - | 9195 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 9196 | `			ph7_result_bool(pCtx,1);` |
|      5 | 9197 | `			return PH7_OK;` |
|      - | 9198 | `		}` |
|     51 | 9199 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 9200 | `			break;` |
|      - | 9201 | `		}` |
|      - | 9202 | `		/* Point to the next character */` |
|     41 | 9203 | `		zIn++;` |
|      1 | 9204 | `	}` |
|      - | 9205 | `	/* The test failed,return FALSE */` |
|     11 | 9206 | `	ph7_result_bool(pCtx,0);` |
|     11 | 9207 | `	return PH7_OK;` |
|      9 | 9208 | `}` |
|      - | 9209 | `/*` |
|      - | 9210 | ` * bool ctype_upper(string $text)` |
|      - | 9211 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 9212 | ` * Parameters` |
|      - | 9213 | ` *  $text` |
|      - | 9214 | ` *   The tested string.` |
|      - | 9215 | ` * Return` |
|      - | 9216 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 9217 | ` */` |
|     16 | 9218 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9219 | `{` |
|      - | 9220 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9221 | `	int nLen;` |
|     17 | 9222 | `	if( nArg < 1 ){` |
|      - | 9223 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9224 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9225 | `		return PH7_OK;` |
|      - | 9226 | `	}` |
|      - | 9227 | `	/* Extract the target string */` |
|     17 | 9228 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9229 | `	zEnd = &zIn[nLen];` |
|     17 | 9230 | `	if( nLen < 1 ){` |
|      - | 9231 | `		/* Empty string,return FALSE */` |
|      3 | 9232 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9233 | `		return PH7_OK;` |
|      - | 9234 | `	}` |
|      - | 9235 | `	/* Perform the requested operation */` |
|     28 | 9236 | `	for(;;){` |
|     57 | 9237 | `		if( zIn >= zEnd ){` |
|      - | 9238 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 9239 | `			ph7_result_bool(pCtx,1);` |
|      5 | 9240 | `			return PH7_OK;` |
|      - | 9241 | `		}` |
|     53 | 9242 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 9243 | `			break;` |
|      - | 9244 | `		}` |
|      - | 9245 | `		/* Point to the next character */` |
|     43 | 9246 | `		zIn++;` |
|      1 | 9247 | `	}` |
|      - | 9248 | `	/* The test failed,return FALSE */` |
|     11 | 9249 | `	ph7_result_bool(pCtx,0);` |
|     11 | 9250 | `	return PH7_OK;` |
|      9 | 9251 | `}` |
|      - | 9252 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 9253 | `/*` |
|      - | 9254 | ` * Section:` |
|      - | 9255 | ` *    URL handling Functions.` |
|      - | 9256 | ` * Status:` |
|      - | 9257 | ` *    Stable.` |
|      - | 9258 | ` */` |
|      - | 9259 | `/*` |
|      - | 9260 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 9261 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 9262 | ` */` |
|   1026 | 9263 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 9264 | `{` |
|      - | 9265 | `	/* Store in the call context result buffer */` |
|   1028 | 9266 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 9267 | `	return SXRET_OK;` |
|      2 | 9268 | `}` |
|      - | 9269 | `/*` |
|      - | 9270 | ` * string base64_encode(string $data)` |
|      - | 9271 | ` * string convert_uuencode(string $data)` |
|      - | 9272 | ` *  Encodes data with MIME base64` |
|      - | 9273 | ` * Parameter` |
|      - | 9274 | ` *  $data` |
|      - | 9275 | ` *    Data to encode` |
|      - | 9276 | ` * Return` |
|      - | 9277 | ` *  Encoded data or FALSE on failure.` |
|      - | 9278 | ` */` |
|      6 | 9279 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9280 | `{` |
|      - | 9281 | `	const char *zIn;` |
|      - | 9282 | `	int nLen;` |
|      7 | 9283 | `	if( nArg < 1 ){` |
|      - | 9284 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9285 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9286 | `		return PH7_OK;` |
|      - | 9287 | `	}` |
|      - | 9288 | `	/* Extract the input string */` |
|      7 | 9289 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 9290 | `	if( nLen < 1 ){` |
|      - | 9291 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9292 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9293 | `		return PH7_OK;` |
|      - | 9294 | `	}` |
|      - | 9295 | `	/* Perform the BASE64 encoding */` |
|      7 | 9296 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 9297 | `	return PH7_OK;` |
|      4 | 9298 | `}` |
|      - | 9299 | `/*` |
|      - | 9300 | ` * string base64_decode(string $data)` |
|      - | 9301 | ` * string convert_uudecode(string $data)` |
|      - | 9302 | ` *  Decodes data encoded with MIME base64` |
|      - | 9303 | ` * Parameter` |
|      - | 9304 | ` *  $data` |
|      - | 9305 | ` *    Encoded data.` |
|      - | 9306 | ` * Return` |
|      - | 9307 | ` *  Returns the original data or FALSE on failure.` |
|      - | 9308 | ` */` |
|     34 | 9309 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 9310 | `{` |
|      - | 9311 | `	const char *zIn;` |
|      - | 9312 | `	int nLen;` |
|     36 | 9313 | `	if( nArg < 1 ){` |
|      - | 9314 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9315 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9316 | `		return PH7_OK;` |
|      - | 9317 | `	}` |
|      - | 9318 | `	/* Extract the input string */` |
|     36 | 9319 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 9320 | `	if( nLen < 1 ){` |
|      - | 9321 | `		/* php decodes the empty string to the EMPTY STRING, not FALSE (FALSE is reserved` |
|      - | 9322 | `		 * for input that cannot be decoded at all). */` |
|      3 | 9323 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 9324 | `		return PH7_OK;` |
|      - | 9325 | `	}` |
|      - | 9326 | `	/* Perform the BASE64 decoding */` |
|     34 | 9327 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 9328 | `	return PH7_OK;` |
|     19 | 9329 | `}` |
|      - | 9330 | `/*` |
|      - | 9331 | ` * string urlencode(string $str)` |
|      - | 9332 | ` *  URL encoding` |
|      - | 9333 | ` * Parameter` |
|      - | 9334 | ` *  $data` |
|      - | 9335 | ` *   Input string.` |
|      - | 9336 | ` * Return` |
|      - | 9337 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 9338 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 9339 | ` *  encoded as plus (+) signs.` |
|      - | 9340 | ` */` |
|      4 | 9341 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9342 | `{` |
|      - | 9343 | `	const char *zIn;` |
|      - | 9344 | `	int nLen;` |
|      5 | 9345 | `	if( nArg < 1 ){` |
|      - | 9346 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9347 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9348 | `		return PH7_OK;` |
|      - | 9349 | `	}` |
|      - | 9350 | `	/* Extract the input string */` |
|      5 | 9351 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 9352 | `	if( nLen < 1 ){` |
|      - | 9353 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9354 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9355 | `		return PH7_OK;` |
|      - | 9356 | `	}` |
|      - | 9357 | `	/* Perform the URL encoding */` |
|      5 | 9358 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 9359 | `	return PH7_OK;` |
|      3 | 9360 | `}` |
|      - | 9361 | `/*` |
|      - | 9362 | ` * string urldecode(string $str)` |
|      - | 9363 | ` *  Decodes any %## encoding in the given string.` |
|      - | 9364 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 9365 | ` * Parameter` |
|      - | 9366 | ` *  $data` |
|      - | 9367 | ` *    Input string.` |
|      - | 9368 | ` * Return` |
|      - | 9369 | ` *  Decoded URL or FALSE on failure.` |
|      - | 9370 | ` */` |
|      6 | 9371 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9372 | `{` |
|      - | 9373 | `	const char *zIn;` |
|      - | 9374 | `	int nLen;` |
|      7 | 9375 | `	if( nArg < 1 ){` |
|      - | 9376 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9377 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9378 | `		return PH7_OK;` |
|      - | 9379 | `	}` |
|      - | 9380 | `	/* Extract the input string */` |
|      7 | 9381 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 9382 | `	if( nLen < 1 ){` |
|      - | 9383 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9384 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9385 | `		return PH7_OK;` |
|      - | 9386 | `	}` |
|      - | 9387 | `	/* Perform the URL decoding */` |
|      7 | 9388 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 9389 | `	return PH7_OK;` |
|      4 | 9390 | `}` |
|      - | 9391 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9392 | `/* Table of the built-in functions */` |
|      - | 9393 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 9394 | `	   /* Variable handling functions */` |
|      - | 9395 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 9396 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 9397 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 9398 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 9399 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 9400 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 9401 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 9402 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 9403 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 9404 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 9405 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 9406 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 9407 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 9408 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 9409 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 9410 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 9411 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 9412 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 9413 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 9414 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 9415 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9416 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 9417 | `	   /* Math functions */` |
|      - | 9418 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 9419 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 9420 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 9421 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 9422 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 9423 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 9424 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 9425 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 9426 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 9427 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 9428 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 9429 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 9430 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 9431 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 9432 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 9433 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 9434 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 9435 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 9436 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 9437 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 9438 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 9439 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 9440 | `	{ "round",    PH7_builtin_round        },` |
|      - | 9441 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 9442 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 9443 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 9444 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 9445 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 9446 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 9447 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 9448 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 9449 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 9450 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9451 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9452 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 9453 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9454 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9455 | `	   /* String handling functions */` |
|      - | 9456 |  |
|      - | 9457 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 9458 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 9459 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 9460 | `	{ "substr_replace",  PH7_builtin_substr_replace },` |
|      - | 9461 | `	{ "levenshtein",     PH7_builtin_levenshtein },` |
|      - | 9462 | `	{ "similar_text",    PH7_builtin_similar_text },` |
|      - | 9463 | `	{ "str_word_count",  PH7_builtin_str_word_count },` |
|      - | 9464 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 9465 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 9466 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 9467 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 9468 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 9469 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 9470 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 9471 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 9472 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 9473 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 9474 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 9475 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 9476 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 9477 | `	{ "strnatcmp"  , PH7_builtin_strnatcmp  },` |
|      - | 9478 | `	{ "strnatcasecmp", PH7_builtin_strnatcmp },` |
|      - | 9479 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 9480 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 9481 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 9482 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 9483 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 9484 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 9485 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 9486 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 9487 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 9488 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 9489 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 9490 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 9491 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 9492 | `	{ "mb_strtolower",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */` |
|      - | 9493 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 9494 | `	{ "mb_strtoupper",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */` |
|      - | 9495 | `	{ "mb_strlen",    PH7_builtin_mb_strlen_f },` |
|      - | 9496 | `	{ "mb_substr",    PH7_builtin_mb_substr_f },` |
|      - | 9497 | `	{ "mb_convert_case", PH7_builtin_mb_convert_case_f },` |
|      - | 9498 | `	{ "mb_strpos",    PH7_builtin_mb_strpos_f },` |
|      - | 9499 | `	{ "mb_stripos",   PH7_builtin_mb_strpos_f },` |
|      - | 9500 | `	{ "mb_strrpos",   PH7_builtin_mb_strpos_f },` |
|      - | 9501 | `	{ "mb_str_split", PH7_builtin_mb_str_split_f },` |
|      - | 9502 | `	{ "mb_internal_encoding", PH7_builtin_mb_internal_encoding_f },` |
|      - | 9503 | `	{ "mb_check_encoding",    PH7_builtin_mb_check_encoding_f },` |
|      - | 9504 | `	{ "mb_strwidth",  PH7_builtin_mb_strwidth_f },` |
|      - | 9505 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 9506 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 9507 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 9508 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 9509 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 9510 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 9511 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 9512 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 9513 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 9514 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 9515 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 9516 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 9517 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 9518 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 9519 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 9520 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 9521 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - | 9522 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - | 9523 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - | 9524 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 9525 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9526 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9527 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 9528 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 9529 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 9530 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 9531 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9532 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9533 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 9534 |  |
|      - | 9535 |  |
|      - | 9536 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 9537 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 9538 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 9539 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 9540 | `	{ "hash",         PH7_builtin_hash      },` |
|      - | 9541 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|      - | 9542 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|      - | 9543 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|      - | 9544 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 9545 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|      - | 9546 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|      - | 9547 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|      - | 9548 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|      - | 9549 | `	{ "filter_var",            PH7_builtin_filter_var },` |
|      - | 9550 | `	{ "filter_input",          PH7_builtin_filter_input },` |
|      - | 9551 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9552 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9553 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 9554 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 9555 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9556 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9557 |  |
|      - | 9558 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 9559 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 9560 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 9561 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 9562 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 9563 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 9564 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 9565 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 9566 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 9567 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 9568 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 9569 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 9570 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9571 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9572 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 9573 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9574 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9575 |  |
|      - | 9576 | `	         /* Ctype functions */` |
|      - | 9577 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 9578 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 9579 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 9580 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 9581 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 9582 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 9583 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 9584 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 9585 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 9586 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 9587 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 9588 | `	         /* Time functions */` |
|      - | 9589 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 9590 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 9591 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 9592 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 9593 | `	{ "date",        PH7_builtin_date         },` |
|      - | 9594 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 9595 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 9596 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 9597 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 9598 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 9599 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 9600 | `	{ "date_default_timezone_get", PH7_builtin_date_default_timezone_get },` |
|      - | 9601 | `	{ "date_default_timezone_set", PH7_builtin_date_default_timezone_set },` |
|      - | 9602 | `	        /* URL functions */` |
|      - | 9603 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 9604 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 9605 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 9606 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 9607 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 9608 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 9609 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 9610 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 9611 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9612 | `};` |
|      - | 9613 | `/*` |
|      - | 9614 | ` * Register the built-in functions defined above,the array functions` |
|      - | 9615 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 9616 | ` */` |
|   3344 | 9617 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      5 | 9618 | `{` |
|      - | 9619 | `	sxu32 n;` |
| 621989 | 9620 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 618645 | 9621 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 309325 | 9622 | `	}` |
|      - | 9623 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   3349 | 9624 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 9625 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   3349 | 9626 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   3349 | 9627 | `}` |
|      - | 9628 |  |
