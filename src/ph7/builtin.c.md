# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4295/5037 lines (85.27%)

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
| 243730 |   18 | `PH7_PRIVATE sxi32 PH7_IntArgResolve(` |
|      - |   19 | `	ph7_context *pCtx,` |
|      - |   20 | `	ph7_value *pArg,` |
|      - |   21 | `	const char *zFunc,` |
|      - |   22 | `	int iArgNum,` |
|      - |   23 | `	const char *zParamName,` |
|      - |   24 | `	const char *zTypeStr,` |
|      - |   25 | `	sxi64 *pOut` |
|      5 |   26 | `){` |
| 243735 |   27 | `	if( ph7_value_is_null(pArg) ){` |
|    ! 0 |   28 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |   29 | `			"%s(): Passing null to parameter #%d (%s) of type %s is deprecated",` |
|    ! 0 |   30 | `			zFunc,iArgNum,zParamName,zTypeStr` |
|      - |   31 | `			);` |
|    ! 0 |   32 | `		*pOut = 0;` |
|    ! 0 |   33 | `		return PH7_OK;` |
|      - |   34 | `	}` |
| 243735 |   35 | `	if( ph7_value_is_float(pArg) ){` |
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
| 243729 |   56 | `	if( ph7_value_is_string(pArg) ){` |
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
| 243715 |   98 | `	if( !ph7_value_is_int(pArg) && !ph7_value_is_bool(pArg) ){` |
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
| 243715 |  113 | `	*pOut = ph7_value_to_int64(pArg);` |
| 243715 |  114 | `	return PH7_OK;` |
| 121870 |  115 | `}` |
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
|      3 |  218 | `{` |
|     89 |  219 | `	int res = 0; /* Assume false by default */` |
|     89 |  220 | `	if( nArg > 0 ){` |
|     89 |  221 | `		res = ph7_value_is_null(apArg[0]);` |
|     43 |  222 | `	}` |
|      - |  223 | `	/* Query result */` |
|     89 |  224 | `	ph7_result_bool(pCtx,res);` |
|     89 |  225 | `	return PH7_OK;` |
|      3 |  226 | `}` |
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
|      4 |  272 | `{` |
|    652 |  273 | `	int res = 0; /* Assume false by default */` |
|    652 |  274 | `	if( nArg > 0 ){` |
|    652 |  275 | `		res = ph7_value_is_array(apArg[0]);` |
|    324 |  276 | `	}` |
|      - |  277 | `	/* Query result */` |
|    652 |  278 | `	ph7_result_bool(pCtx,res);` |
|    652 |  279 | `	return PH7_OK;` |
|      4 |  280 | `}` |
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
|      3 |  308 | `{` |
|     65 |  309 | `	int res = 0; /* Assume false by default */` |
|     65 |  310 | `	if( nArg > 0 ){` |
|     65 |  311 | `		res = ph7_value_is_resource(apArg[0]);` |
|     31 |  312 | `	}` |
|     65 |  313 | `	ph7_result_bool(pCtx,res);` |
|     65 |  314 | `	return PH7_OK;` |
|      3 |  315 | `}` |
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
|  33932 |  411 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  412 | `{` |
|  33937 |  413 | `	int res = 1; /* Assume empty by default */` |
|  33937 |  414 | `	if( nArg > 0 ){` |
|  33935 |  415 | `		res = ph7_value_is_empty(apArg[0]);` |
|  16965 |  416 | `	}` |
|  33937 |  417 | `	ph7_result_bool(pCtx,res);` |
|  33937 |  418 | `	return PH7_OK;` |
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
| 235416 |  461 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  462 | `{` |
| 235421 |  463 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"substr",1,"$string"); }` |
|      - |  464 | `	const char *zSource,*zOfft;` |
|      - |  465 | `	int nOfft,nLen,nSrcLen;` |
| 235421 |  466 | `	if( nArg < 2 ){` |
|      - |  467 | `		/* return FALSE */` |
|    ! 0 |  468 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  469 | `		return PH7_OK;` |
|      - |  470 | `	}` |
|      - |  471 | `	/* Extract the target string */` |
| 235421 |  472 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 235421 |  473 | `	if( nSrcLen < 1 ){` |
|      - |  474 | `		/* Empty string,return FALSE */` |
|  12605 |  475 | `		ph7_result_bool(pCtx,0);` |
|  12605 |  476 | `		return PH7_OK;` |
|      - |  477 | `	}` |
| 222821 |  478 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  479 | `	/* Extract the offset */` |
|      - |  480 | `	{` |
| 222821 |  481 | `		sxi64 iTmp = 0;` |
| 222821 |  482 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"substr",2,"$offset","int",&iTmp);` |
| 222821 |  483 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  484 | `			return rcArg;` |
|      - |  485 | `		}` |
| 222821 |  486 | `		nOfft = (int)iTmp;` |
|      - |  487 | `	}` |
| 222821 |  488 | `	if( nOfft < 0 ){` |
|  33069 |  489 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  33069 |  490 | `		if( zOfft < zSource ){` |
|      - |  491 | `			/* Invalid offset */` |
|      5 |  492 | `			ph7_result_bool(pCtx,0);` |
|      5 |  493 | `			return PH7_OK;` |
|      - |  494 | `		}` |
|  33065 |  495 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  33065 |  496 | `		nOfft = (int)(zOfft-zSource);` |
| 206287 |  497 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  498 | `		/* Invalid offset */` |
|    245 |  499 | `		ph7_result_bool(pCtx,0);` |
|    245 |  500 | `		return PH7_OK;` |
|    ! 0 |  501 | `	}else{` |
| 189517 |  502 | `		zOfft = &zSource[nOfft];` |
| 189517 |  503 | `		nLen = nSrcLen - nOfft;` |
|      - |  504 | `	}` |
| 222577 |  505 | `	if( nArg > 2 ){` |
|      - |  506 | `		/* Extract the length */` |
| 181909 |  507 | `		nLen = ph7_value_to_int(apArg[2]);` |
| 181909 |  508 | `		if( nLen == 0 ){` |
|      - |  509 | `			/* Invalid length,return an empty string */` |
|      5 |  510 | `			ph7_result_string(pCtx,"",0);` |
|      5 |  511 | `			return PH7_OK;` |
| 181905 |  512 | `		}else if( nLen < 0 ){` |
|  32999 |  513 | `			nLen = nSrcLen + nLen - nOfft;` |
|  32999 |  514 | `			if( nLen < 1 ){` |
|      - |  515 | `				/* Invalid  length */` |
|      3 |  516 | `				nLen = nSrcLen - nOfft;` |
|      1 |  517 | `			}` |
|  16497 |  518 | `		}` |
| 181905 |  519 | `		if( nLen + nOfft > nSrcLen ){` |
|      - |  520 | `			/* Invalid length */` |
|   6075 |  521 | `			nLen = nSrcLen - nOfft;` |
|   3035 |  522 | `		}` |
|  90950 |  523 | `	}` |
|      - |  524 | `	/* Return the substring */` |
| 222573 |  525 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 222573 |  526 | `	return PH7_OK;` |
| 117713 |  527 | `}` |
|      - |  528 | `/*` |
|      - |  529 | ` * int substr_compare(string $main_str,string $str ,int $offset[,int $length[,bool $case_insensitivity = false ]])` |
|      - |  530 | ` *  Binary safe comparison of two strings from an offset, up to length characters.` |
|      - |  531 | ` * Parameters` |
|      - |  532 | ` *  $main_str` |
|      - |  533 | ` *  The main string being compared.` |
|      - |  534 | ` *  $str` |
|      - |  535 | ` *   The secondary string being compared.` |
|      - |  536 | ` * $offset` |
|      - |  537 | ` *  The start position for the comparison. If negative, it starts counting from` |
|      - |  538 | ` *  the end of the string.` |
|      - |  539 | ` * $length` |
|      - |  540 | ` *  The length of the comparison. The default value is the largest of the length` |
|      - |  541 | ` *  of the str compared to the length of main_str less the offset.` |
|      - |  542 | ` * $case_insensitivity` |
|      - |  543 | ` *  If case_insensitivity is TRUE, comparison is case insensitive.` |
|      - |  544 | ` * Return` |
|      - |  545 | ` *  Returns < 0 if main_str from position offset is less than str, > 0 if it is greater than` |
|      - |  546 | ` *  str, and 0 if they are equal. If offset is equal to or greater than the length of main_str` |
|      - |  547 | ` *  or length is set and is less than 1, substr_compare() prints a warning and returns FALSE.` |
|      - |  548 | ` */` |
|     20 |  549 | `static int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  550 | `{` |
|      - |  551 | `	const char *zSource,*zOfft,*zSub;` |
|      - |  552 | `	int nOfft,nLen,nSrcLen,nSublen;` |
|     21 |  553 | `	int iCase = 0;` |
|      - |  554 | `	int rc;` |
|     21 |  555 | `	if( nArg < 3 ){` |
|      - |  556 | `		/* Missing arguments,return FALSE */` |
|    ! 0 |  557 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  558 | `		return PH7_OK;` |
|      - |  559 | `	}` |
|      - |  560 | `	/* Extract the target string */` |
|     21 |  561 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|     21 |  562 | `	if( nSrcLen < 1 ){` |
|      - |  563 | `		/* Empty string,return FALSE */` |
|      3 |  564 | `		ph7_result_bool(pCtx,0);` |
|      3 |  565 | `		return PH7_OK;` |
|      - |  566 | `	}` |
|     19 |  567 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  568 | `	/* Extract the substring */` |
|     19 |  569 | `	zSub = ph7_value_to_string(apArg[1],&nSublen);` |
|     19 |  570 | `	if( nSublen < 1 \|\| nSublen > nSrcLen){` |
|      - |  571 | `		/* Empty string,return FALSE */` |
|      3 |  572 | `		ph7_result_bool(pCtx,0);` |
|      3 |  573 | `		return PH7_OK;` |
|      - |  574 | `	}` |
|      - |  575 | `	/* Extract the offset */` |
|     17 |  576 | `	nOfft = ph7_value_to_int(apArg[2]);` |
|     17 |  577 | `	if( nOfft < 0 ){` |
|      5 |  578 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|      5 |  579 | `		if( zOfft < zSource ){` |
|      - |  580 | `			/* Invalid offset */` |
|      3 |  581 | `			ph7_result_bool(pCtx,0);` |
|      3 |  582 | `			return PH7_OK;` |
|      - |  583 | `		}` |
|      3 |  584 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|      3 |  585 | `		nOfft = (int)(zOfft-zSource);` |
|     14 |  586 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  587 | `		/* Invalid offset */` |
|      3 |  588 | `		ph7_result_bool(pCtx,0);` |
|      3 |  589 | `		return PH7_OK;` |
|    ! 0 |  590 | `	}else{` |
|     11 |  591 | `		zOfft = &zSource[nOfft];` |
|     11 |  592 | `		nLen = nSrcLen - nOfft;` |
|      - |  593 | `	}` |
|     13 |  594 | `	if( nArg > 3 ){` |
|      - |  595 | `		/* Extract the length */` |
|     11 |  596 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     11 |  597 | `		if( nLen < 1 ){` |
|      - |  598 | `			/* Invalid  length */` |
|      5 |  599 | `			ph7_result_int(pCtx,1);` |
|      5 |  600 | `			return PH7_OK;` |
|      7 |  601 | `		}else if( nLen + nOfft > nSrcLen ){` |
|      - |  602 | `			/* Invalid length */` |
|    ! 0 |  603 | `			nLen = nSrcLen - nOfft;` |
|    ! 0 |  604 | `		}` |
|      7 |  605 | `		if( nArg > 4 ){` |
|      - |  606 | `			/* Case-sensitive or not */` |
|      5 |  607 | `			iCase = ph7_value_to_bool(apArg[4]);` |
|      2 |  608 | `		}` |
|      3 |  609 | `	}` |
|      - |  610 | `	/* Perform the comparison */` |
|      9 |  611 | `	if( iCase ){` |
|      3 |  612 | `		rc = SyStrnicmp(zOfft,zSub,(sxu32)nLen);` |
|      2 |  613 | `	}else{` |
|      7 |  614 | `		rc = SyStrncmp(zOfft,zSub,(sxu32)nLen);` |
|      - |  615 | `	}` |
|      - |  616 | `	/* Comparison result */` |
|      9 |  617 | `	ph7_result_int(pCtx,rc);` |
|      9 |  618 | `	return PH7_OK;` |
|     11 |  619 | `}` |
|      - |  620 | `/*` |
|      - |  621 | ` * int substr_count(string $haystack,string $needle[,int $offset = 0 [,int $length ]])` |
|      - |  622 | ` *  Count the number of substring occurrences.` |
|      - |  623 | ` * Parameters` |
|      - |  624 | ` * $haystack` |
|      - |  625 | ` *   The string to search in` |
|      - |  626 | ` * $needle` |
|      - |  627 | ` *   The substring to search for` |
|      - |  628 | ` * $offset` |
|      - |  629 | ` *  The offset where to start counting` |
|      - |  630 | ` * $length (NOT USED)` |
|      - |  631 | ` *  The maximum length after the specified offset to search for the substring.` |
|      - |  632 | ` *  It outputs a warning if the offset plus the length is greater than the haystack length.` |
|      - |  633 | ` * Return` |
|      - |  634 | ` *  Toral number of substring occurrences.` |
|      - |  635 | ` */` |
|     26 |  636 | `static int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  637 | `{` |
|      - |  638 | `	const char *zText,*zPattern,*zEnd;` |
|      - |  639 | `	int nTextlen,nPatlen;` |
|     27 |  640 | `	int iCount = 0;` |
|      - |  641 | `	sxu32 nOfft;` |
|      - |  642 | `	sxi32 rc;` |
|     27 |  643 | `	if( nArg < 2 ){` |
|      - |  644 | `		/* Missing arguments */` |
|    ! 0 |  645 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  646 | `		return PH7_OK;` |
|      - |  647 | `	}` |
|      - |  648 | `	/* Point to the haystack */` |
|     27 |  649 | `	zText = ph7_value_to_string(apArg[0],&nTextlen);` |
|      - |  650 | `	/* Point to the neddle */` |
|     27 |  651 | `	zPattern = ph7_value_to_string(apArg[1],&nPatlen);` |
|     27 |  652 | `	if( nPatlen < 1 ){` |
|      - |  653 | `		/* Empty needle: PHP 8 throws a catchable ValueError. */` |
|      3 |  654 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  655 | `			"substr_count(): Argument #2 ($needle) must not be empty");` |
|      - |  656 | `	}` |
|      - |  657 | `	/* Apply the optional $offset/$length window before searching. PHP 8 validates` |
|      - |  658 | `	 * both against the haystack (a negative value counts from the end) and throws a` |
|      - |  659 | `	 * catchable ValueError when the result falls outside it — this happens before the` |
|      - |  660 | `	 * needle-fits check, so it fires even when the needle is longer than the haystack. */` |
|     25 |  661 | `	if( nArg > 2 ){` |
|     19 |  662 | `		ph7_int64 iOfft = ph7_value_to_int64(apArg[2]);` |
|     19 |  663 | `		if( iOfft < 0 ){` |
|      5 |  664 | `			iOfft += nTextlen;` |
|      2 |  665 | `		}` |
|     19 |  666 | `		if( iOfft < 0 \|\| iOfft > nTextlen ){` |
|      3 |  667 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  668 | `				"substr_count(): Argument #3 ($offset) must be contained in argument #1 ($haystack)");` |
|      - |  669 | `		}` |
|      - |  670 | `		/* Point to the desired offset and shrink the remaining region */` |
|     17 |  671 | `		zText = &zText[iOfft];` |
|     17 |  672 | `		nTextlen -= (int)iOfft;` |
|      8 |  673 | `	}` |
|     23 |  674 | `	if( nArg > 3 ){` |
|     15 |  675 | `		ph7_int64 nLen = ph7_value_to_int64(apArg[3]);` |
|     15 |  676 | `		if( nLen < 0 ){` |
|      - |  677 | `			/* Negative length is relative to the end of the (offset) haystack */` |
|      5 |  678 | `			nLen += nTextlen;` |
|      2 |  679 | `		}` |
|     15 |  680 | `		if( nLen < 0 \|\| nLen > nTextlen ){` |
|      5 |  681 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  682 | `				"substr_count(): Argument #4 ($length) must be contained in argument #1 ($haystack)");` |
|      - |  683 | `		}` |
|     11 |  684 | `		nTextlen = (int)nLen;` |
|      5 |  685 | `	}` |
|     19 |  686 | `	if( nTextlen < 1 \|\| nPatlen > nTextlen ){` |
|      - |  687 | `		/* The windowed haystack can't contain the needle: zero matches */` |
|      3 |  688 | `		ph7_result_int(pCtx,0);` |
|      3 |  689 | `		return PH7_OK;` |
|      - |  690 | `	}` |
|      - |  691 | `	/* Point to the end of the windowed haystack */` |
|     17 |  692 | `	zEnd = &zText[nTextlen];` |
|      - |  693 | `	/* Perform the search */` |
|     17 |  694 | `	for(;;){` |
|     35 |  695 | `		rc = SyBlobSearch((const void *)zText,(sxu32)(zEnd-zText),(const void *)zPattern,nPatlen,&nOfft);` |
|     35 |  696 | `		if( rc != SXRET_OK ){` |
|      - |  697 | `			/* Pattern not found,break immediately */` |
|     13 |  698 | `			break;` |
|      - |  699 | `		}` |
|      - |  700 | `		/* Increment counter and update the offset */` |
|     23 |  701 | `		iCount++;` |
|     23 |  702 | `		zText += nOfft + nPatlen;` |
|     23 |  703 | `		if( zText >= zEnd ){` |
|      5 |  704 | `			break;` |
|      - |  705 | `		}` |
|      1 |  706 | `	}` |
|      - |  707 | `	/* Pattern count */` |
|     17 |  708 | `	ph7_result_int(pCtx,iCount);` |
|     17 |  709 | `	return PH7_OK;` |
|     14 |  710 | `}` |
|      - |  711 | `/* Forward declarations: defined with the trim/addcslashes and str_contains` |
|      - |  712 | ` * families below. */` |
|      - |  713 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256]);` |
|      - |  714 | `/*` |
|      - |  715 | ` * php 8.1 null-to-non-nullable ZPP deprecation, notice-only form for the` |
|      - |  716 | ` * legacy string builtins that still coerce null to "" themselves: emit` |
|      - |  717 | ``  * `f(): Passing null to parameter #N ($name) of type string is deprecated` `` |
|      - |  718 | ` * when the arg is an actual null, leaving the resolution unchanged.` |
|      - |  719 | ` */` |
| 304822 |  720 | `static void StrNullArgNotice(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNum,const char *zParamName)` |
|      5 |  721 | `{` |
| 304827 |  722 | `	if( ph7_value_is_null(pArg) ){` |
|     25 |  723 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |  724 | `			"%s(): Passing null to parameter #%d (%s) of type string is deprecated",` |
|      8 |  725 | `			zFunc,iArgNum,zParamName);` |
|      8 |  726 | `	}` |
| 304827 |  727 | `}` |
|      - |  728 | `static sxi32 StrPredicateResolveArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,` |
|      - |  729 | `	int iArgNum,const char *zParamName,const char *zTypeStr,const char *zNullMsg,` |
|      - |  730 | `	ph7_value *pTmp,const char **pzOut,int *pnOut);` |
|      - |  731 | `/*` |
|      - |  732 | ` * Validate and resolve an int-typed builtin parameter with php-8 ZPP weak-mode` |
|      - |  733 | ` * semantics: ints and bools pass through; null emits the 8.1 deprecation and` |
|      - |  734 | ` * resolves to 0; floats and float-strings convert, with the implicit-conversion` |
|      - |  735 | ` * E_DEPRECATED when lossy and a TypeError when NAN/INF/out of int range;` |
|      - |  736 | ` * integral numeric strings convert exactly; everything else (arrays, resources,` |
|      - |  737 | ` * objects, non-numeric strings) is a TypeError naming zTypeStr (e.g. "int",` |
|      - |  738 | ` * "array\|int"). Returns PH7_OK with *pOut set, or the throw status.` |
|      - |  739 | ` */` |
|      - |  740 | `/*` |
|      - |  741 | ` * Normalize a substr_replace() offset/length pair against a string of nStrLen` |
|      - |  742 | ` * bytes, exactly like PHP: a negative offset counts from the end (clamped to 0),` |
|      - |  743 | ` * an offset past the end clamps to the end; a negative length leaves that many` |
|      - |  744 | ` * bytes off the end of the remaining region (clamped to 0), and the length is` |
|      - |  745 | ` * finally clamped to the remaining region. Written without f+l additions so an` |
|      - |  746 | ` * INT64_MAX length cannot overflow.` |
|      - |  747 | ` */` |
|     60 |  748 | `static void SubstrReplaceWindow(sxi64 *pF,sxi64 *pL,int nStrLen)` |
|      1 |  749 | `{` |
|     61 |  750 | `	sxi64 f = *pF,l = *pL;` |
|     61 |  751 | `	if( f < 0 ){` |
|      9 |  752 | `		f += nStrLen;` |
|      9 |  753 | `		if( f < 0 ){` |
|      5 |  754 | `			f = 0;` |
|      3 |  755 | `		}` |
|     57 |  756 | `	}else if( f > nStrLen ){` |
|      5 |  757 | `		f = nStrLen;` |
|      2 |  758 | `	}` |
|     61 |  759 | `	if( l < 0 ){` |
|      7 |  760 | `		l += nStrLen - f;` |
|      7 |  761 | `		if( l < 0 ){` |
|      5 |  762 | `			l = 0;` |
|      2 |  763 | `		}` |
|      3 |  764 | `	}` |
|     61 |  765 | `	if( l > nStrLen - f ){` |
|     25 |  766 | `		l = nStrLen - f;` |
|     12 |  767 | `	}` |
|     61 |  768 | `	*pF = f;` |
|     61 |  769 | `	*pL = l;` |
|     61 |  770 | `}` |
|      - |  771 | `/* A replacement string collected out of substr_replace()'s $replace array.` |
|      - |  772 | ` * The bytes live in a shared pool blob (walker values are transient), so the` |
|      - |  773 | ` * item stores pool offsets, mirroring the strtr_entry technique. */` |
|      - |  774 | `typedef struct substr_repl_item substr_repl_item;` |
|      - |  775 | `struct substr_repl_item` |
|      - |  776 | `{` |
|      - |  777 | `	sxu32 nOfft; /* Offset of the string inside the pool */` |
|      - |  778 | `	sxu32 nLen;  /* Length of the string */` |
|      - |  779 | `};` |
|      - |  780 | `typedef struct substr_replace_collect substr_replace_collect;` |
|      - |  781 | `struct substr_replace_collect` |
|      - |  782 | `{` |
|      - |  783 | `	SyBlob *pPool;  /* Byte pool for string items (string walker only) */` |
|      - |  784 | `	SySet *pSet;    /* substr_repl_item set (string) or sxi64 set (int) */` |
|      - |  785 | `	sxi32 rc;       /* SXRET_OK or SXERR_MEM on collector failure */` |
|      - |  786 | `};` |
|      - |  787 | `/* ph7_array_walk() callback: append one $replace element to the pool. */` |
|      6 |  788 | `static int SubstrReplaceStrWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  789 | `{` |
|      7 |  790 | `	substr_replace_collect *pCol = (substr_replace_collect *)pUserData;` |
|      - |  791 | `	substr_repl_item sItem;` |
|      - |  792 | `	const char *zStr;` |
|      - |  793 | `	int nLen;` |
|      3 |  794 | `	SXUNUSED(pKey);` |
|      7 |  795 | `	zStr = ph7_value_to_string(pData,&nLen);` |
|      7 |  796 | `	sItem.nOfft = SyBlobLength(pCol->pPool);` |
|      7 |  797 | `	sItem.nLen = (sxu32)nLen;` |
|      7 |  798 | `	if( nLen > 0 && SXRET_OK != SyBlobAppend(pCol->pPool,(const void *)zStr,(sxu32)nLen) ){` |
|    ! 0 |  799 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  800 | `		return SXERR_ABORT;` |
|      - |  801 | `	}` |
|      7 |  802 | `	if( SXRET_OK != SySetPut(pCol->pSet,(const void *)&sItem) ){` |
|    ! 0 |  803 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  804 | `		return SXERR_ABORT;` |
|      - |  805 | `	}` |
|      7 |  806 | `	return PH7_OK;` |
|      4 |  807 | `}` |
|      - |  808 | `/* ph7_array_walk() callback: collect one $offset/$length element as an int. */` |
|     12 |  809 | `static int SubstrReplaceIntWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  810 | `{` |
|     13 |  811 | `	substr_replace_collect *pCol = (substr_replace_collect *)pUserData;` |
|     13 |  812 | `	sxi64 iVal = ph7_value_to_int64(pData);` |
|      6 |  813 | `	SXUNUSED(pKey);` |
|     13 |  814 | `	if( SXRET_OK != SySetPut(pCol->pSet,(const void *)&iVal) ){` |
|    ! 0 |  815 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  816 | `		return SXERR_ABORT;` |
|      - |  817 | `	}` |
|     13 |  818 | `	return PH7_OK;` |
|      7 |  819 | `}` |
|      - |  820 | `/* Per-element state while walking substr_replace()'s array $string. */` |
|      - |  821 | `typedef struct substr_replace_ctx substr_replace_ctx;` |
|      - |  822 | `struct substr_replace_ctx` |
|      - |  823 | `{` |
|      - |  824 | `	ph7_value *pResult;   /* Result array (keys preserved) */` |
|      - |  825 | `	ph7_value *pScratch;  /* Reusable string value for each element */` |
|      - |  826 | `	SyBlob *pReplPool;    /* Pool behind aRepl items */` |
|      - |  827 | `	SySet *pRepl;         /* substr_repl_item set or NULL when $replace is scalar */` |
|      - |  828 | `	SySet *pFrom;         /* sxi64 set or NULL when $offset is scalar */` |
|      - |  829 | `	SySet *pLen;          /* sxi64 set or NULL when $length is scalar/absent */` |
|      - |  830 | `	sxu32 iReplCur;       /* Next-position cursors into the three sets */` |
|      - |  831 | `	sxu32 iFromCur;` |
|      - |  832 | `	sxu32 iLenCur;` |
|      - |  833 | `	const char *zRepl;    /* Scalar $replace */` |
|      - |  834 | `	int nRepl;` |
|      - |  835 | `	sxi64 iFrom;          /* Scalar $offset */` |
|      - |  836 | `	sxi64 iLen;           /* Scalar $length */` |
|      - |  837 | `	int bLenGiven;        /* FALSE: $length absent/null -> element length */` |
|      - |  838 | `	sxi32 rc;             /* SXRET_OK or SXERR_MEM */` |
|      - |  839 | `};` |
|      - |  840 | `/*` |
|      - |  841 | ` * ph7_array_walk() callback over the array $string: replace the window of one` |
|      - |  842 | ` * element and insert the result under the element's original key. Array-form` |
|      - |  843 | ` * $replace/$offset/$length are consumed positionally; when a set runs out PHP` |
|      - |  844 | ` * falls back to ""/0/element-length respectively.` |
|      - |  845 | ` */` |
|     24 |  846 | `static int SubstrReplaceElemWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  847 | `{` |
|     25 |  848 | `	substr_replace_ctx *pRep = (substr_replace_ctx *)pUserData;` |
|      - |  849 | `	const char *zStr,*zRepl;` |
|      - |  850 | `	sxi64 f,l;` |
|      - |  851 | `	int nLen,nRepl;` |
|     25 |  852 | `	zStr = ph7_value_to_string(pData,&nLen);` |
|      - |  853 | `	/* Positional $replace element ("" when exhausted) */` |
|     25 |  854 | `	if( pRep->pRepl ){` |
|     11 |  855 | `		if( pRep->iReplCur < SySetUsed(pRep->pRepl) ){` |
|      7 |  856 | `			substr_repl_item *pItem = (substr_repl_item *)SySetAt(pRep->pRepl,pRep->iReplCur++);` |
|      7 |  857 | `			zRepl = (const char *)SyBlobDataAt(pRep->pReplPool,pItem->nOfft);` |
|      7 |  858 | `			nRepl = (int)pItem->nLen;` |
|      4 |  859 | `		}else{` |
|      5 |  860 | `			zRepl = "";` |
|      5 |  861 | `			nRepl = 0;` |
|      - |  862 | `		}` |
|      6 |  863 | `	}else{` |
|     15 |  864 | `		zRepl = pRep->zRepl;` |
|     15 |  865 | `		nRepl = pRep->nRepl;` |
|      - |  866 | `	}` |
|      - |  867 | `	/* Positional $offset element (0 when exhausted) */` |
|     25 |  868 | `	if( pRep->pFrom ){` |
|     13 |  869 | `		sxi64 *pVal = 0;` |
|     13 |  870 | `		if( pRep->iFromCur < SySetUsed(pRep->pFrom) ){` |
|      9 |  871 | `			pVal = (sxi64 *)SySetAt(pRep->pFrom,pRep->iFromCur++);` |
|      4 |  872 | `		}` |
|     13 |  873 | `		f = pVal ? *pVal : 0;` |
|      7 |  874 | `	}else{` |
|     13 |  875 | `		f = pRep->iFrom;` |
|      - |  876 | `	}` |
|      - |  877 | `	/* Positional $length element (element length when exhausted) */` |
|     25 |  878 | `	if( pRep->pLen ){` |
|      7 |  879 | `		sxi64 *pVal = 0;` |
|      7 |  880 | `		if( pRep->iLenCur < SySetUsed(pRep->pLen) ){` |
|      5 |  881 | `			pVal = (sxi64 *)SySetAt(pRep->pLen,pRep->iLenCur++);` |
|      2 |  882 | `		}` |
|      7 |  883 | `		l = pVal ? *pVal : nLen;` |
|      4 |  884 | `	}else{` |
|     19 |  885 | `		l = pRep->bLenGiven ? pRep->iLen : nLen;` |
|      - |  886 | `	}` |
|     25 |  887 | `	SubstrReplaceWindow(&f,&l,nLen);` |
|      - |  888 | `	/* Assemble prefix + replacement + suffix in the scratch value */` |
|     25 |  889 | `	ph7_value_reset_string_cursor(pRep->pScratch);` |
|     24 |  890 | `	if( (f > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,zStr,(int)f))` |
|     24 |  891 | `	 \|\| (nRepl > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,zRepl,nRepl))` |
|     40 |  892 | `	 \|\| (nLen - (int)(f+l) > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,&zStr[f+l],nLen - (int)(f+l))) ){` |
|     30 |  893 | `		pRep->rc = SXERR_MEM;` |
|     30 |  894 | `		return SXERR_ABORT;` |
|      - |  895 | `	}` |
|     25 |  896 | `	if( SXRET_OK != ph7_array_add_elem(pRep->pResult,pKey,pRep->pScratch) ){` |
|    ! 0 |  897 | `		pRep->rc = SXERR_MEM;` |
|    ! 0 |  898 | `		return SXERR_ABORT;` |
|      - |  899 | `	}` |
|     25 |  900 | `	return PH7_OK;` |
|     43 |  901 | `}` |
|      - |  902 | `/*` |
|      - |  903 | ` * mixed substr_replace(array\|string $string,array\|string $replace,array\|int $offset[,array\|int\|null $length = null])` |
|      - |  904 | ` *  Replace text within a portion of a string.` |
|      - |  905 | ` * Parameters` |
|      - |  906 | ` *  $string` |
|      - |  907 | ` *   The input string or an array of strings (each element is processed with` |
|      - |  908 | ` *   its own positional replace/offset/length when those are arrays too).` |
|      - |  909 | ` *  $replace` |
|      - |  910 | ` *   The replacement string. When $string is scalar and $replace is an array,` |
|      - |  911 | ` *   only its first element is used (PHP quirk).` |
|      - |  912 | ` *  $offset` |
|      - |  913 | ` *   Window start; negative counts from the end of the string.` |
|      - |  914 | ` *  $length` |
|      - |  915 | ` *   Window length; negative leaves that many bytes at the end; null/absent` |
|      - |  916 | ` *   means "to the end of the string".` |
|      - |  917 | ` * Return` |
|      - |  918 | ` *  The processed string, or an array of processed strings (keys preserved).` |
|      - |  919 | ` * Errors` |
|      - |  920 | ` *  ArgumentCountError on fewer than 3 arguments; TypeError when an array` |
|      - |  921 | ` *  $offset/$length is combined with a scalar $string.` |
|      - |  922 | ` */` |
|     58 |  923 | `static int PH7_builtin_substr_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  924 | `{` |
|      - |  925 | `	ph7_value sStrTmp,sReplTmp;` |
|     59 |  926 | `	const char *zStr = 0,*zRepl = 0;` |
|     59 |  927 | `	int nLen = 0,nRepl = 0;` |
|      - |  928 | `	int bLenGiven;` |
|     59 |  929 | `	sxi64 f = 0,l = 0;` |
|      - |  930 | `	sxi32 rc;` |
|     59 |  931 | `	if( nArg < 3 ){` |
|    ! 0 |  932 | `		return PH7_VmThrowException(pCtx,` |
|      - |  933 | `			"ArgumentCountError",` |
|      - |  934 | `			"substr_replace() expects at least 3 arguments, %d given",` |
|    ! 0 |  935 | `			nArg` |
|      - |  936 | `			);` |
|      - |  937 | `	}` |
|      - |  938 | `	/* $length counts as given unless absent or null (php: ?null semantics) */` |
|     59 |  939 | `	bLenGiven = (nArg > 3 && !ph7_value_is_null(apArg[3]));` |
|      - |  940 | `	/* php ZPP validates all four args, in order, before the body runs: the` |
|      - |  941 | `	 * non-array forms resolve here (null deprecation, __toString objects,` |
|      - |  942 | `	 * numeric strings), arrays pass through to the per-mode handling. */` |
|     59 |  943 | `	PH7_MemObjInit(pCtx->pVm,&sStrTmp);` |
|     59 |  944 | `	PH7_MemObjInit(pCtx->pVm,&sReplTmp);` |
|     59 |  945 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     45 |  946 | `		rc = StrPredicateResolveArg(pCtx,apArg[0],"substr_replace",1,"$string","array\|string",` |
|      - |  947 | `			"substr_replace(): Passing null to parameter #1 ($string) "` |
|      - |  948 | `			"of type array\|string is deprecated",` |
|      - |  949 | `			&sStrTmp,&zStr,&nLen);` |
|     45 |  950 | `		if( rc != PH7_OK ) goto out;` |
|     22 |  951 | `	}` |
|     59 |  952 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|     51 |  953 | `		rc = StrPredicateResolveArg(pCtx,apArg[1],"substr_replace",2,"$replace","array\|string",` |
|      - |  954 | `			"substr_replace(): Passing null to parameter #2 ($replace) "` |
|      - |  955 | `			"of type array\|string is deprecated",` |
|      - |  956 | `			&sReplTmp,&zRepl,&nRepl);` |
|     51 |  957 | `		if( rc != PH7_OK ) goto out;` |
|     25 |  958 | `	}` |
|     59 |  959 | `	if( !ph7_value_is_array(apArg[2]) ){` |
|     51 |  960 | `		rc = PH7_IntArgResolve(pCtx,apArg[2],"substr_replace",3,"$offset","array\|int",&f);` |
|     51 |  961 | `		if( rc != PH7_OK ) goto out;` |
|     24 |  962 | `	}` |
|     57 |  963 | `	if( bLenGiven && !ph7_value_is_array(apArg[3]) ){` |
|     31 |  964 | `		rc = PH7_IntArgResolve(pCtx,apArg[3],"substr_replace",4,"$length","array\|int\|null",&l);` |
|     31 |  965 | `		if( rc != PH7_OK ) goto out;` |
|     14 |  966 | `	}` |
|     55 |  967 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - |  968 | `		/* Array form: process each element, preserving keys */` |
|      - |  969 | `		substr_replace_ctx sRep;` |
|      - |  970 | `		substr_replace_collect sCol;` |
|      - |  971 | `		SyBlob sReplPool;` |
|      - |  972 | `		SySet sRepl,sFrom,sLen;` |
|      - |  973 | `		ph7_value *pResult,*pScratch;` |
|     15 |  974 | `		sxi32 rcWalk = SXRET_OK;` |
|     15 |  975 | `		SyBlobInit(&sReplPool,&pCtx->pVm->sAllocator);` |
|     15 |  976 | `		SySetInit(&sRepl,&pCtx->pVm->sAllocator,sizeof(substr_repl_item));` |
|     15 |  977 | `		SySetInit(&sFrom,&pCtx->pVm->sAllocator,sizeof(sxi64));` |
|     15 |  978 | `		SySetInit(&sLen,&pCtx->pVm->sAllocator,sizeof(sxi64));` |
|     15 |  979 | `		SyZero(&sRep,sizeof(substr_replace_ctx));` |
|     15 |  980 | `		sRep.bLenGiven = bLenGiven;` |
|     15 |  981 | `		sCol.rc = SXRET_OK;` |
|      - |  982 | `		/* Collect array-form $replace/$offset/$length positionally; the` |
|      - |  983 | `		 * scalar forms were already resolved above. */` |
|     15 |  984 | `		if( ph7_value_is_array(apArg[1]) ){` |
|      5 |  985 | `			sCol.pPool = &sReplPool;` |
|      5 |  986 | `			sCol.pSet = &sRepl;` |
|      5 |  987 | `			ph7_array_walk(apArg[1],SubstrReplaceStrWalker,&sCol);` |
|      5 |  988 | `			sRep.pRepl = &sRepl;` |
|      5 |  989 | `			sRep.pReplPool = &sReplPool;` |
|      3 |  990 | `		}else{` |
|     11 |  991 | `			sRep.zRepl = zRepl;` |
|     11 |  992 | `			sRep.nRepl = nRepl;` |
|      - |  993 | `		}` |
|     15 |  994 | `		if( sCol.rc == SXRET_OK && ph7_value_is_array(apArg[2]) ){` |
|      7 |  995 | `			sCol.pSet = &sFrom;` |
|      7 |  996 | `			ph7_array_walk(apArg[2],SubstrReplaceIntWalker,&sCol);` |
|      7 |  997 | `			sRep.pFrom = &sFrom;` |
|      4 |  998 | `		}else{` |
|      9 |  999 | `			sRep.iFrom = f;` |
|      - | 1000 | `		}` |
|     15 | 1001 | `		if( sCol.rc == SXRET_OK && bLenGiven ){` |
|      9 | 1002 | `			if( ph7_value_is_array(apArg[3]) ){` |
|      5 | 1003 | `				sCol.pSet = &sLen;` |
|      5 | 1004 | `				ph7_array_walk(apArg[3],SubstrReplaceIntWalker,&sCol);` |
|      5 | 1005 | `				sRep.pLen = &sLen;` |
|      3 | 1006 | `			}else{` |
|      5 | 1007 | `				sRep.iLen = l;` |
|      - | 1008 | `			}` |
|      4 | 1009 | `		}` |
|     15 | 1010 | `		pResult = ph7_context_new_array(pCtx);` |
|     15 | 1011 | `		pScratch = ph7_context_new_scalar(pCtx);` |
|     15 | 1012 | `		if( sCol.rc != SXRET_OK \|\| pResult == 0 \|\| pScratch == 0 ){` |
|    ! 0 | 1013 | `			rcWalk = SXERR_MEM;` |
|    ! 0 | 1014 | `		}else{` |
|     15 | 1015 | `			sRep.pResult = pResult;` |
|     15 | 1016 | `			sRep.pScratch = pScratch;` |
|     15 | 1017 | `			ph7_value_string(pScratch,"",0); /* Force string representation */` |
|     15 | 1018 | `			ph7_array_walk(apArg[0],SubstrReplaceElemWalker,&sRep);` |
|     15 | 1019 | `			rcWalk = sRep.rc;` |
|      - | 1020 | `		}` |
|     15 | 1021 | `		SyBlobRelease(&sReplPool);` |
|     15 | 1022 | `		SySetRelease(&sRepl);` |
|     15 | 1023 | `		SySetRelease(&sFrom);` |
|     15 | 1024 | `		SySetRelease(&sLen);` |
|     15 | 1025 | `		if( rcWalk != SXRET_OK ){` |
|    ! 0 | 1026 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1027 | `			goto out;` |
|      - | 1028 | `		}` |
|     15 | 1029 | `		ph7_result_value(pCtx,pResult);` |
|     15 | 1030 | `		rc = PH7_OK;` |
|     15 | 1031 | `		goto out;` |
|      - | 1032 | `	}` |
|      - | 1033 | `	/* Scalar form: array $offset/$length are a TypeError, array $replace` |
|      - | 1034 | `	 * degrades to its first element (php quirk). */` |
|     41 | 1035 | `	if( ph7_value_is_array(apArg[2]) ){` |
|      3 | 1036 | `		rc = PH7_VmThrowException(pCtx,` |
|      - | 1037 | `			"TypeError",` |
|      - | 1038 | `			"substr_replace(): Argument #3 ($offset) cannot be an array when working on a single string"` |
|      - | 1039 | `			);` |
|      3 | 1040 | `		goto out;` |
|      - | 1041 | `	}` |
|     39 | 1042 | `	if( bLenGiven && ph7_value_is_array(apArg[3]) ){` |
|      3 | 1043 | `		rc = PH7_VmThrowException(pCtx,` |
|      - | 1044 | `			"TypeError",` |
|      - | 1045 | `			"substr_replace(): Argument #4 ($length) cannot be an array when working on a single string"` |
|      - | 1046 | `			);` |
|      3 | 1047 | `		goto out;` |
|      - | 1048 | `	}` |
|     37 | 1049 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 1050 | `		/* First element of the replace array, or "" when empty */` |
|      5 | 1051 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      5 | 1052 | `		zRepl = "";` |
|      5 | 1053 | `		nRepl = 0;` |
|      5 | 1054 | `		if( pMap->pFirst ){` |
|      3 | 1055 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pMap->pFirst->nValIdx);` |
|      3 | 1056 | `			if( pVal ){` |
|      3 | 1057 | `				zRepl = ph7_value_to_string(pVal,&nRepl);` |
|      1 | 1058 | `			}` |
|      1 | 1059 | `		}` |
|      2 | 1060 | `	}` |
|     37 | 1061 | `	if( !bLenGiven ){` |
|     15 | 1062 | `		l = nLen;` |
|      7 | 1063 | `	}` |
|     37 | 1064 | `	SubstrReplaceWindow(&f,&l,nLen);` |
|      - | 1065 | `	/* Assemble prefix + replacement + suffix straight into the call result` |
|      - | 1066 | `	 * (ph7_result_string appends), no scratch buffer needed. */` |
|     37 | 1067 | `	rc = SXRET_OK;` |
|     37 | 1068 | `	if( f > 0 ){` |
|     29 | 1069 | `		rc = ph7_result_string(pCtx,zStr,(int)f);` |
|     14 | 1070 | `	}` |
|     37 | 1071 | `	if( rc == SXRET_OK && nRepl > 0 ){` |
|     33 | 1072 | `		rc = ph7_result_string(pCtx,zRepl,nRepl);` |
|     16 | 1073 | `	}` |
|     37 | 1074 | `	if( rc == SXRET_OK && nLen - (int)(f+l) > 0 ){` |
|     17 | 1075 | `		rc = ph7_result_string(pCtx,&zStr[f+l],nLen - (int)(f+l));` |
|      8 | 1076 | `	}` |
|     37 | 1077 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 1078 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1079 | `		goto out;` |
|      - | 1080 | `	}` |
|      - | 1081 | `	/* Force a string result even when all three segments are empty */` |
|     37 | 1082 | `	rc = ph7_result_string(pCtx,"",0);` |
|     37 | 1083 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 1084 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1085 | `		goto out;` |
|      - | 1086 | `	}` |
|     37 | 1087 | `	rc = PH7_OK;` |
|     29 | 1088 | `out:` |
|     59 | 1089 | `	PH7_MemObjRelease(&sStrTmp);` |
|     59 | 1090 | `	PH7_MemObjRelease(&sReplTmp);` |
|     59 | 1091 | `	return rc;` |
|     30 | 1092 | `}` |
|      - | 1093 | `/*` |
|      - | 1094 | ` * int levenshtein(string $string1,string $string2[,int $insertion_cost = 1[,int $replacement_cost = 1[,int $deletion_cost = 1]]])` |
|      - | 1095 | ` *  Calculate the Levenshtein distance between two strings, byte per byte` |
|      - | 1096 | ` *  (case-sensitive), with optional per-operation costs. Mirrors PHP's` |
|      - | 1097 | ` *  reference_levdist(): two rolling rows over string2.` |
|      - | 1098 | ` * Return` |
|      - | 1099 | ` *  The minimal number of weighted edit operations turning $string1 into` |
|      - | 1100 | ` *  $string2.` |
|      - | 1101 | ` */` |
|     34 | 1102 | `static int PH7_builtin_levenshtein(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1103 | `{` |
|      - | 1104 | `	static const char *azParam[] = { "$insertion_cost","$replacement_cost","$deletion_cost" };` |
|      - | 1105 | `	const char *zStr1,*zStr2;` |
|     35 | 1106 | `	sxi64 iCostIns = 1,iCostRep = 1,iCostDel = 1;` |
|      - | 1107 | `	sxi64 *p1,*p2,*pTmp;` |
|      - | 1108 | `	sxi64 c0,c1,c2;` |
|      - | 1109 | `	ph7_value sTmp1,sTmp2;` |
|      - | 1110 | `	int nLen1,nLen2;` |
|      - | 1111 | `	int i1,i2;` |
|      - | 1112 | `	sxi32 rc;` |
|      - | 1113 | `	int i;` |
|     35 | 1114 | `	if( nArg < 2 ){` |
|    ! 0 | 1115 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1116 | `			"ArgumentCountError",` |
|      - | 1117 | `			"levenshtein() expects at least 2 arguments, %d given",` |
|    ! 0 | 1118 | `			nArg` |
|      - | 1119 | `			);` |
|      - | 1120 | `	}` |
|      - | 1121 | `	/* $string1/$string2: null deprecates to "", __toString objects resolve,` |
|      - | 1122 | `	 * everything non-stringish is a TypeError (php ZPP weak mode). */` |
|     35 | 1123 | `	PH7_MemObjInit(pCtx->pVm,&sTmp1);` |
|     35 | 1124 | `	PH7_MemObjInit(pCtx->pVm,&sTmp2);` |
|     35 | 1125 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"levenshtein",1,"$string1","string",` |
|      - | 1126 | `		"levenshtein(): Passing null to parameter #1 ($string1) "` |
|      - | 1127 | `		"of type string is deprecated",` |
|      - | 1128 | `		&sTmp1,&zStr1,&nLen1);` |
|     35 | 1129 | `	if( rc != PH7_OK ) goto out;` |
|     35 | 1130 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"levenshtein",2,"$string2","string",` |
|      - | 1131 | `		"levenshtein(): Passing null to parameter #2 ($string2) "` |
|      - | 1132 | `		"of type string is deprecated",` |
|      - | 1133 | `		&sTmp2,&zStr2,&nLen2);` |
|     35 | 1134 | `	if( rc != PH7_OK ) goto out;` |
|      - | 1135 | `	/* Optional integer costs */` |
|     57 | 1136 | `	for( i = 2 ; i < nArg && i < 5 ; i++ ){` |
|      - | 1137 | `		sxi64 iVal;` |
|     31 | 1138 | `		rc = PH7_IntArgResolve(pCtx,apArg[i],"levenshtein",i+1,azParam[i-2],"int",&iVal);` |
|     31 | 1139 | `		if( rc != PH7_OK ) goto out;` |
|     23 | 1140 | `		if( i == 2 ){` |
|     11 | 1141 | `			iCostIns = iVal;` |
|     18 | 1142 | `		}else if( i == 3 ){` |
|      7 | 1143 | `			iCostRep = iVal;` |
|      4 | 1144 | `		}else{` |
|      7 | 1145 | `			iCostDel = iVal;` |
|      - | 1146 | `		}` |
|     12 | 1147 | `	}` |
|     27 | 1148 | `	if( nLen1 == 0 ){` |
|      3 | 1149 | `		ph7_result_int64(pCtx,(sxi64)nLen2 * iCostIns);` |
|      3 | 1150 | `		rc = PH7_OK;` |
|      3 | 1151 | `		goto out;` |
|      - | 1152 | `	}` |
|     25 | 1153 | `	if( nLen2 == 0 ){` |
|      3 | 1154 | `		ph7_result_int64(pCtx,(sxi64)nLen1 * iCostDel);` |
|      3 | 1155 | `		rc = PH7_OK;` |
|      3 | 1156 | `		goto out;` |
|      - | 1157 | `	}` |
|      - | 1158 | `	/* Two rolling DP rows over string2 (auto-released on return). Reject a` |
|      - | 1159 | `	 * string2 long enough to overflow the 32-bit allocation size. */` |
|     23 | 1160 | `	if( (sxu32)nLen2 >= (SXU32_HIGH / sizeof(sxi64)) - 1 ){` |
|    ! 0 | 1161 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1162 | `		goto out;` |
|      - | 1163 | `	}` |
|     23 | 1164 | `	p1 = (sxi64 *)ph7_context_alloc_chunk(pCtx,(unsigned int)(sizeof(sxi64) * (sxu32)(nLen2 + 1)),FALSE,TRUE);` |
|     23 | 1165 | `	p2 = (sxi64 *)ph7_context_alloc_chunk(pCtx,(unsigned int)(sizeof(sxi64) * (sxu32)(nLen2 + 1)),FALSE,TRUE);` |
|     23 | 1166 | `	if( p1 == 0 \|\| p2 == 0 ){` |
|    ! 0 | 1167 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1168 | `		goto out;` |
|      - | 1169 | `	}` |
|    733 | 1170 | `	for( i2 = 0 ; i2 <= nLen2 ; i2++ ){` |
|    711 | 1171 | `		p1[i2] = (sxi64)i2 * iCostIns;` |
|    356 | 1172 | `	}` |
|    707 | 1173 | `	for( i1 = 0 ; i1 < nLen1 ; i1++ ){` |
|    685 | 1174 | `		p2[0] = p1[0] + iCostDel;` |
| 181111 | 1175 | `		for( i2 = 0 ; i2 < nLen2 ; i2++ ){` |
| 180427 | 1176 | `			c0 = p1[i2] + ((zStr1[i1] == zStr2[i2]) ? 0 : iCostRep);` |
| 180427 | 1177 | `			c1 = p1[i2 + 1] + iCostDel;` |
| 180427 | 1178 | `			if( c1 < c0 ){` |
|  45393 | 1179 | `				c0 = c1;` |
|  22696 | 1180 | `			}` |
| 180427 | 1181 | `			c2 = p2[i2] + iCostIns;` |
| 180427 | 1182 | `			if( c2 < c0 ){` |
|  44809 | 1183 | `				c0 = c2;` |
|  22404 | 1184 | `			}` |
| 180427 | 1185 | `			p2[i2 + 1] = c0;` |
|  90214 | 1186 | `		}` |
|    685 | 1187 | `		pTmp = p1;` |
|    685 | 1188 | `		p1 = p2;` |
|    685 | 1189 | `		p2 = pTmp;` |
|    343 | 1190 | `	}` |
|     23 | 1191 | `	ph7_result_int64(pCtx,p1[nLen2]);` |
|     23 | 1192 | `	rc = PH7_OK;` |
|     17 | 1193 | `out:` |
|     35 | 1194 | `	PH7_MemObjRelease(&sTmp1);` |
|     35 | 1195 | `	PH7_MemObjRelease(&sTmp2);` |
|     35 | 1196 | `	return rc;` |
|     18 | 1197 | `}` |
|      - | 1198 | `/*` |
|      - | 1199 | ` * Longest common substring scan behind similar_text() — a faithful port of` |
|      - | 1200 | ` * PHP's php_similar_str(): O(n*m) scan recording the first longest run.` |
|      - | 1201 | ` */` |
|     26 | 1202 | `static void SimilarStr(const char *zTxt1,int nLen1,const char *zTxt2,int nLen2,` |
|      - | 1203 | `	int *pPos1,int *pPos2,int *pMax,int *pCount)` |
|      1 | 1204 | `{` |
|      - | 1205 | `	const char *p,*q;` |
|     27 | 1206 | `	const char *zEnd1 = &zTxt1[nLen1];` |
|     27 | 1207 | `	const char *zEnd2 = &zTxt2[nLen2];` |
|      - | 1208 | `	int l;` |
|     27 | 1209 | `	*pMax = 0;` |
|     27 | 1210 | `	*pCount = 0;` |
|    143 | 1211 | `	for( p = zTxt1 ; p < zEnd1 ; p++ ){` |
|    843 | 1212 | `		for( q = zTxt2 ; q < zEnd2 ; q++ ){` |
|    999 | 1213 | `			for( l = 0 ; (p+l < zEnd1) && (q+l < zEnd2) && (p[l] == q[l]) ; l++ );` |
|    727 | 1214 | `			if( l > *pMax ){` |
|     25 | 1215 | `				*pMax = l;` |
|     25 | 1216 | `				*pCount += 1;` |
|     25 | 1217 | `				*pPos1 = (int)(p - zTxt1);` |
|     25 | 1218 | `				*pPos2 = (int)(q - zTxt2);` |
|     12 | 1219 | `			}` |
|    364 | 1220 | `		}` |
|     59 | 1221 | `	}` |
|     27 | 1222 | `}` |
|      - | 1223 | `/*` |
|      - | 1224 | ` * Recursive divide-and-conquer behind similar_text() — a faithful port of` |
|      - | 1225 | `` * PHP's php_similar_char(), including its quirky `count > 1` guard on the`` |
|      - | 1226 | ` * left-side recursion.` |
|      - | 1227 | ` */` |
|     26 | 1228 | `static int SimilarChar(const char *zTxt1,int nLen1,const char *zTxt2,int nLen2)` |
|      1 | 1229 | `{` |
|      - | 1230 | `	int nSum;` |
|     27 | 1231 | `	int nPos1 = 0,nPos2 = 0,nMax,nCount;` |
|     27 | 1232 | `	SimilarStr(zTxt1,nLen1,zTxt2,nLen2,&nPos1,&nPos2,&nMax,&nCount);` |
|     27 | 1233 | `	if( (nSum = nMax) != 0 ){` |
|     25 | 1234 | `		if( nPos1 && nPos2 && nCount > 1 ){` |
|    ! 0 | 1235 | `			nSum += SimilarChar(zTxt1,nPos1,zTxt2,nPos2);` |
|    ! 0 | 1236 | `		}` |
|     25 | 1237 | `		if( (nPos1 + nMax < nLen1) && (nPos2 + nMax < nLen2) ){` |
|     13 | 1238 | `			nSum += SimilarChar(&zTxt1[nPos1 + nMax],nLen1 - nPos1 - nMax,` |
|      8 | 1239 | `				&zTxt2[nPos2 + nMax],nLen2 - nPos2 - nMax);` |
|      4 | 1240 | `		}` |
|     12 | 1241 | `	}` |
|     27 | 1242 | `	return nSum;` |
|      1 | 1243 | `}` |
|      - | 1244 | `/*` |
|      - | 1245 | ` * int similar_text(string $string1,string $string2[,float &$percent])` |
|      - | 1246 | ` *  Calculate the similarity between two strings, as the number of matching` |
|      - | 1247 | ` *  characters found by PHP's greedy longest-common-substring recursion.` |
|      - | 1248 | ` *  When $percent is given it receives the similarity in percent:` |
|      - | 1249 | ` *  matching * 200 / (len1 + len2).` |
|      - | 1250 | ` * Return` |
|      - | 1251 | ` *  The number of matching characters in both strings.` |
|      - | 1252 | ` */` |
|     22 | 1253 | `static int PH7_builtin_similar_text(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1254 | `{` |
|      - | 1255 | `	const char *zStr1,*zStr2;` |
|      - | 1256 | `	ph7_value sTmp1,sTmp2;` |
|      - | 1257 | `	int nLen1,nLen2;` |
|      - | 1258 | `	int nSim;` |
|      - | 1259 | `	sxi32 rc;` |
|     23 | 1260 | `	if( nArg < 2 ){` |
|    ! 0 | 1261 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1262 | `			"ArgumentCountError",` |
|      - | 1263 | `			"similar_text() expects at least 2 arguments, %d given",` |
|    ! 0 | 1264 | `			nArg` |
|      - | 1265 | `			);` |
|      - | 1266 | `	}` |
|     23 | 1267 | `	PH7_MemObjInit(pCtx->pVm,&sTmp1);` |
|     23 | 1268 | `	PH7_MemObjInit(pCtx->pVm,&sTmp2);` |
|     23 | 1269 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"similar_text",1,"$string1","string",` |
|      - | 1270 | `		"similar_text(): Passing null to parameter #1 ($string1) "` |
|      - | 1271 | `		"of type string is deprecated",` |
|      - | 1272 | `		&sTmp1,&zStr1,&nLen1);` |
|     23 | 1273 | `	if( rc != PH7_OK ) goto out;` |
|     23 | 1274 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"similar_text",2,"$string2","string",` |
|      - | 1275 | `		"similar_text(): Passing null to parameter #2 ($string2) "` |
|      - | 1276 | `		"of type string is deprecated",` |
|      - | 1277 | `		&sTmp2,&zStr2,&nLen2);` |
|     23 | 1278 | `	if( rc != PH7_OK ) goto out;` |
|     23 | 1279 | `	if( nLen1 + nLen2 == 0 ){` |
|      5 | 1280 | `		nSim = 0;` |
|      3 | 1281 | `	}else{` |
|     19 | 1282 | `		nSim = SimilarChar(zStr1,nLen1,zStr2,nLen2);` |
|      - | 1283 | `	}` |
|     23 | 1284 | `	if( nArg > 2 ){` |
|      - | 1285 | `		/* Write the percentage through the by-ref out-param */` |
|      7 | 1286 | `		ph7_value *pPercent = ph7_context_new_scalar(pCtx);` |
|      7 | 1287 | `		if( pPercent == 0 ){` |
|    ! 0 | 1288 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1289 | `			goto out;` |
|    ! 0 | 1290 | `		}else{` |
|      7 | 1291 | `			double dPct = (nLen1 + nLen2 == 0) ? 0.0 : (double)nSim * 200.0 / (double)(nLen1 + nLen2);` |
|      7 | 1292 | `			ph7_value_double(pPercent,dPct);` |
|      7 | 1293 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],pPercent);` |
|      - | 1294 | `		}` |
|      3 | 1295 | `	}` |
|     23 | 1296 | `	ph7_result_int(pCtx,nSim);` |
|     23 | 1297 | `	rc = PH7_OK;` |
|     11 | 1298 | `out:` |
|     23 | 1299 | `	PH7_MemObjRelease(&sTmp1);` |
|     23 | 1300 | `	PH7_MemObjRelease(&sTmp2);` |
|     23 | 1301 | `	return rc;` |
|     12 | 1302 | `}` |
|      - | 1303 | `/*` |
|      - | 1304 | ` * array\|int str_word_count(string $string[,int $format = 0[,?string $characters = null]])` |
|      - | 1305 | ` *  Count (or return) the words inside a string. A word is a run of alphabetic` |
|      - | 1306 | ` *  characters, which may contain (but not start the string with) "'" and "-";` |
|      - | 1307 | ` *  $characters adds extra bytes to the word set ("a..z" ranges supported, as` |
|      - | 1308 | ` *  in PHP's php_charmask).` |
|      - | 1309 | ` *  $format: 0 -> word count, 1 -> array of words, 2 -> array of words keyed` |
|      - | 1310 | ` *  by their byte position in $string.` |
|      - | 1311 | ` * Errors` |
|      - | 1312 | ` *  ValueError when $format is not 0, 1 or 2.` |
|      - | 1313 | ` */` |
|     44 | 1314 | `static int PH7_builtin_str_word_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1315 | `{` |
|      - | 1316 | `	const char *zIn,*zEnd,*zPtr;` |
|     45 | 1317 | `	ph7_value *pArray = 0,*pValue = 0;` |
|      - | 1318 | `	ph7_value sTmp,sListTmp;` |
|      - | 1319 | `	char aMask[256];` |
|     45 | 1320 | `	int bMask = 0;` |
|     45 | 1321 | `	int iFormat = 0;` |
|     45 | 1322 | `	int nCount = 0;` |
|      - | 1323 | `	int nLen;` |
|      - | 1324 | `	sxi32 rc;` |
|     45 | 1325 | `	if( nArg < 1 ){` |
|    ! 0 | 1326 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1327 | `			"ArgumentCountError",` |
|      - | 1328 | `			"str_word_count() expects at least 1 argument, %d given",` |
|    ! 0 | 1329 | `			nArg` |
|      - | 1330 | `			);` |
|      - | 1331 | `	}` |
|     45 | 1332 | `	PH7_MemObjInit(pCtx->pVm,&sTmp);` |
|     45 | 1333 | `	PH7_MemObjInit(pCtx->pVm,&sListTmp);` |
|     45 | 1334 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_word_count",1,"$string","string",` |
|      - | 1335 | `		"str_word_count(): Passing null to parameter #1 ($string) "` |
|      - | 1336 | `		"of type string is deprecated",` |
|      - | 1337 | `		&sTmp,&zIn,&nLen);` |
|     45 | 1338 | `	if( rc != PH7_OK ) goto out;` |
|     45 | 1339 | `	if( nArg > 1 ){` |
|      - | 1340 | `		sxi64 iVal;` |
|     31 | 1341 | `		rc = PH7_IntArgResolve(pCtx,apArg[1],"str_word_count",2,"$format","int",&iVal);` |
|     33 | 1342 | `		if( rc != PH7_OK ) goto out;` |
|     29 | 1343 | `		if( iVal < 0 \|\| iVal > 2 ){` |
|      5 | 1344 | `			rc = PH7_VmThrowException(pCtx,` |
|      - | 1345 | `				"ValueError",` |
|      - | 1346 | `				"str_word_count(): Argument #2 ($format) must be a valid format value"` |
|      - | 1347 | `				);` |
|      5 | 1348 | `			goto out;` |
|      - | 1349 | `		}` |
|     25 | 1350 | `		iFormat = (int)iVal;` |
|     12 | 1351 | `	}` |
|     39 | 1352 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      - | 1353 | `		/* $characters is ?string: null (skipped above) simply keeps the` |
|      - | 1354 | `		 * default word set, no deprecation. */` |
|      - | 1355 | `		const char *zList;` |
|      - | 1356 | `		int nList;` |
|     13 | 1357 | `		rc = StrPredicateResolveArg(pCtx,apArg[2],"str_word_count",3,"$characters","?string",` |
|      - | 1358 | `			"" /* unreachable: null never gets here */,` |
|      - | 1359 | `			&sListTmp,&zList,&nList);` |
|     13 | 1360 | `		if( rc != PH7_OK ) goto out;` |
|     13 | 1361 | `		PH7_BuildCharMask(pCtx,zList,nList,aMask);` |
|     13 | 1362 | `		bMask = 1;` |
|      6 | 1363 | `	}` |
|     39 | 1364 | `	if( iFormat != 0 ){` |
|     25 | 1365 | `		pArray = ph7_context_new_array(pCtx);` |
|     25 | 1366 | `		pValue = ph7_context_new_scalar(pCtx);` |
|     25 | 1367 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1368 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1369 | `			goto out;` |
|      - | 1370 | `		}` |
|     12 | 1371 | `	}` |
|     39 | 1372 | `	zPtr = zIn;` |
|     39 | 1373 | `	zEnd = &zIn[nLen];` |
|     39 | 1374 | `	if( nLen > 0 ){` |
|      - | 1375 | `		/* php: the string's first byte cannot be ' or -, and its last byte` |
|      - | 1376 | `		 * cannot be -, unless the charlist explicitly allows them. */` |
|     33 | 1377 | `		if( (zPtr[0] == '\'' && (!bMask \|\| !aMask[(unsigned char)'\''])) \|\|` |
|     28 | 1378 | `			(zPtr[0] == '-'  && (!bMask \|\| !aMask[(unsigned char)'-'])) ){` |
|      9 | 1379 | `			zPtr++;` |
|      4 | 1380 | `		}` |
|     33 | 1381 | `		if( zEnd[-1] == '-' && (!bMask \|\| !aMask[(unsigned char)'-']) ){` |
|      9 | 1382 | `			zEnd--;` |
|      4 | 1383 | `		}` |
|     16 | 1384 | `	}` |
|    135 | 1385 | `	while( zPtr < zEnd ){` |
|     91 | 1386 | `		const char *zStart = zPtr;` |
|    477 | 1387 | `		while( zPtr < zEnd && ( SyisAlpha((unsigned char)zPtr[0])` |
|    253 | 1388 | `			\|\| (bMask && aMask[(unsigned char)zPtr[0]])` |
|     98 | 1389 | `			\|\| zPtr[0] == '\'' \|\| zPtr[0] == '-' ) ){` |
|    339 | 1390 | `			zPtr++;` |
|      1 | 1391 | `		}` |
|     97 | 1392 | `		if( zPtr > zStart ){` |
|     91 | 1393 | `			if( iFormat == 0 ){` |
|     19 | 1394 | `				nCount++;` |
|     10 | 1395 | `			}else{` |
|     73 | 1396 | `				ph7_value_reset_string_cursor(pValue);` |
|     73 | 1397 | `				if( SXRET_OK != ph7_value_string(pValue,zStart,(int)(zPtr-zStart)) ){` |
|    ! 0 | 1398 | `					rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1399 | `					goto out;` |
|      - | 1400 | `				}` |
|     73 | 1401 | `				if( iFormat == 1 ){` |
|     59 | 1402 | `					if( SXRET_OK != ph7_array_add_elem(pArray,0,pValue) ){` |
|    ! 0 | 1403 | `						rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1404 | `						goto out;` |
|      - | 1405 | `					}` |
|     30 | 1406 | `				}else{` |
|     15 | 1407 | `					if( SXRET_OK != ph7_array_add_intkey_elem(pArray,(int)(zStart-zIn),pValue) ){` |
|    ! 0 | 1408 | `						rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1409 | `						goto out;` |
|      - | 1410 | `					}` |
|      - | 1411 | `				}` |
|      - | 1412 | `			}` |
|     45 | 1413 | `		}` |
|     97 | 1414 | `		zPtr++;` |
|      1 | 1415 | `	}` |
|     37 | 1416 | `	if( iFormat == 0 ){` |
|     13 | 1417 | `		ph7_result_int(pCtx,nCount);` |
|      7 | 1418 | `	}else{` |
|     25 | 1419 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1420 | `	}` |
|     37 | 1421 | `	rc = PH7_OK;` |
|     21 | 1422 | `out:` |
|     43 | 1423 | `	PH7_MemObjRelease(&sTmp);` |
|     43 | 1424 | `	PH7_MemObjRelease(&sListTmp);` |
|     43 | 1425 | `	return rc;` |
|     22 | 1426 | `}` |
|      - | 1427 | `/*` |
|      - | 1428 | ` * string chunk_split(string $body[,int $chunklen = 76 [, string $end = "\r\n" ]])` |
|      - | 1429 | ` *   Split a string into smaller chunks.` |
|      - | 1430 | ` * Parameters` |
|      - | 1431 | ` *  $body` |
|      - | 1432 | ` *   The string to be chunked.` |
|      - | 1433 | ` * $chunklen` |
|      - | 1434 | ` *   The chunk length.` |
|      - | 1435 | ` * $end` |
|      - | 1436 | ` *   The line ending sequence.` |
|      - | 1437 | ` * Return` |
|      - | 1438 | ` *  The chunked string or NULL on failure.` |
|      - | 1439 | ` */` |
|     14 | 1440 | `static int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1441 | `{` |
|     15 | 1442 | `	const char *zIn,*zEnd,*zSep = "\r\n";` |
|      - | 1443 | `	int nSepLen,nChunkLen,nLen;` |
|      - | 1444 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1445 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     15 | 1446 | `	if( nArg < 1 ){` |
|      - | 1447 | `		/* Nothing to split,return null */` |
|    ! 0 | 1448 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1449 | `		return PH7_OK;` |
|      - | 1450 | `	}` |
|      - | 1451 | `	/* initialize/Extract arguments */` |
|     15 | 1452 | `	nSepLen = (int)sizeof("\r\n") - 1;` |
|     15 | 1453 | `	nChunkLen = 76;` |
|     15 | 1454 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 1455 | `	zEnd = &zIn[nLen];` |
|     15 | 1456 | `	if( nArg > 1 ){` |
|      - | 1457 | `		/* Chunk length */` |
|     13 | 1458 | `		nChunkLen = ph7_value_to_int(apArg[1]);` |
|     13 | 1459 | `		if( nChunkLen < 1 ){` |
|      - | 1460 | `			/* PHP 8 throws a catchable ValueError for a non-positive length. */` |
|      3 | 1461 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1462 | `				"chunk_split(): Argument #2 ($length) must be greater than 0");` |
|      - | 1463 | `		}` |
|     11 | 1464 | `		if( nArg > 2 ){` |
|      - | 1465 | `			/* Separator */` |
|      9 | 1466 | `			zSep = ph7_value_to_string(apArg[2],&nSepLen);` |
|      9 | 1467 | `			if( nSepLen < 1 ){` |
|      - | 1468 | `				/* Switch back to the default separator */` |
|      3 | 1469 | `				zSep = "\r\n";` |
|      3 | 1470 | `				nSepLen = (int)sizeof("\r\n") - 1;` |
|      1 | 1471 | `			}` |
|      4 | 1472 | `		}` |
|      5 | 1473 | `	}` |
|      - | 1474 | `	/* Perform the requested operation */` |
|     13 | 1475 | `	if( nChunkLen > nLen ){` |
|      - | 1476 | `		/* Nothing to split,return the string and the separator */` |
|      9 | 1477 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);` |
|      9 | 1478 | `		return PH7_OK;` |
|      - | 1479 | `	}` |
|     17 | 1480 | `	while( zIn < zEnd ){` |
|     13 | 1481 | `		if( nChunkLen > (int)(zEnd-zIn) ){` |
|      3 | 1482 | `			nChunkLen = (int)(zEnd - zIn);` |
|      1 | 1483 | `		}` |
|      - | 1484 | `		/* Append the chunk and the separator */` |
|     13 | 1485 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);` |
|      - | 1486 | `		/* Point beyond the chunk */` |
|     13 | 1487 | `		zIn += nChunkLen;` |
|      1 | 1488 | `	}` |
|      5 | 1489 | `	return PH7_OK;` |
|      8 | 1490 | `}` |
|      - | 1491 | `/*` |
|      - | 1492 | ` * string addslashes(string $str)` |
|      - | 1493 | ` *  Quote string with slashes.` |
|      - | 1494 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1495 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1496 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1497 | ` * Parameter` |
|      - | 1498 | ` *  str: The string to be escaped.` |
|      - | 1499 | ` * Return` |
|      - | 1500 | ` *  Returns the escaped string` |
|      - | 1501 | ` */` |
|     20 | 1502 | `static int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1503 | `{` |
|      - | 1504 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1505 | `	int nLen;` |
|      - | 1506 | `	/* PHP enforces exactly one argument. */` |
|     22 | 1507 | `	if( nArg != 1 ){` |
|      4 | 1508 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1509 | `			"ArgumentCountError",` |
|      - | 1510 | `			"addslashes() expects exactly 1 argument, %d given",` |
|      1 | 1511 | `			nArg` |
|      - | 1512 | `			);` |
|      - | 1513 | `	}` |
|      - | 1514 | `	/* NULL is deprecated and treated as an empty string; other invalid` |
|      - | 1515 | `	 * types still produce a TypeError. */` |
|     19 | 1516 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 1517 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 1518 | `			E_DEPRECATED,` |
|      - | 1519 | `			"addslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - | 1520 | `			);` |
|      - | 1521 | `		/* fall through so conversion below yields empty string */` |
|      1 | 1522 | `	}` |
|      - | 1523 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     27 | 1524 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     28 | 1525 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     18 | 1526 | `	    ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 1527 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1528 | `			"TypeError",` |
|      - | 1529 | `			"addslashes(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 1530 | `			ph7_type_name(apArg[0])` |
|      - | 1531 | `			);` |
|      - | 1532 | `	}` |
|      - | 1533 | `	/* Convert to string representation first and obtain length. */` |
|     19 | 1534 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 1535 | `	if( nLen < 1 ){` |
|      - | 1536 | `		/* Return the empty string */` |
|      5 | 1537 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1538 | `		return PH7_OK;` |
|      - | 1539 | `	}` |
|     15 | 1540 | `	zEnd = &zIn[nLen];` |
|     15 | 1541 | `	zCur = 0; /* cc warning */` |
|     20 | 1542 | `	for(;;){` |
|     41 | 1543 | `		if( zIn >= zEnd ){` |
|      - | 1544 | `			/* No more input */` |
|     15 | 1545 | `			break;` |
|      - | 1546 | `		}` |
|     27 | 1547 | `		zCur = zIn;` |
|      - | 1548 | `		/* scan until a character that needs escaping (', ", \\, or NUL) */` |
|     89 | 1549 | `		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' && zIn[0] != '\0' ){` |
|     63 | 1550 | `			zIn++;` |
|      1 | 1551 | `		}` |
|     27 | 1552 | `		if( zIn > zCur ){` |
|      - | 1553 | `			/* Append raw contents */` |
|     23 | 1554 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 1555 | `		}` |
|     27 | 1556 | `		if( zIn < zEnd ){` |
|     17 | 1557 | `			int c = zIn[0];` |
|     17 | 1558 | `			if( c == '\0' ){` |
|      - | 1559 | `				/* PHP escapes NUL as "\\0" (two characters) */` |
|      3 | 1560 | `				ph7_result_string(pCtx,"\\0",2);` |
|      2 | 1561 | `			}else{` |
|     15 | 1562 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1563 | `			}` |
|      8 | 1564 | `		}` |
|     27 | 1565 | `		zIn++;` |
|      1 | 1566 | `	}` |
|     15 | 1567 | `	return PH7_OK;` |
|     12 | 1568 | `}` |
|      - | 1569 | `/*` |
|      - | 1570 | ``  * Build a 256-entry membership mask from a PHP charlist, expanding `a..z` `` |
|      - | 1571 | ` * byte ranges exactly like PHP's php_charmask(). On return aMask[c] != 0 iff` |
|      - | 1572 | ` * the byte c belongs to the set. Emits the PHP-exact warnings for the three` |
|      - | 1573 | ` * malformed-range shapes (ph7_context_throw_error_format prepends the active` |
|      - | 1574 | ` * function name, so the messages omit it); on a bad range the surrounding` |
|      - | 1575 | ` * bytes are still added and the scan never aborts. Reads only within` |
|      - | 1576 | ` * [zList, zList+nLen).` |
|      - | 1577 | ` *` |
|      - | 1578 | ` * Use ONLY for the builtins whose charlist expands ranges the way PHP's` |
|      - | 1579 | ` * php_charmask() does: trim/ltrim/rtrim/addcslashes (and quotemeta, whose set` |
|      - | 1580 | ` * is a fixed literal with no ".."). Do NOT route strspn/strcspn/strtok/strpbrk` |
|      - | 1581 | ` * through this — PHP treats their charlists literally, so expanding "a..z" here` |
|      - | 1582 | ` * would be a behavior regression plus spurious "Invalid '..'-range" warnings.` |
|      - | 1583 | ` */` |
|    106 | 1584 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256])` |
|      4 | 1585 | `{` |
|    110 | 1586 | `	const unsigned char *zIn  = (const unsigned char *)zList;` |
|    110 | 1587 | `	const unsigned char *zEnd = zIn + (nLen > 0 ? nLen : 0);` |
|    110 | 1588 | `	SyZero(aMask,256);` |
|    378 | 1589 | `	for( ; zIn < zEnd ; zIn++ ){` |
|    272 | 1590 | `		int c = zIn[0];` |
|    272 | 1591 | `		if( zIn + 3 < zEnd && zIn[1] == '.' && zIn[2] == '.' && zIn[3] >= c ){` |
|      - | 1592 | `			/* Valid incrementing range c..zIn[3] */` |
|     22 | 1593 | `			int hi = zIn[3],k;` |
|    386 | 1594 | `			for( k = c ; k <= hi ; k++ ){` |
|    366 | 1595 | `				aMask[k] = 1;` |
|    184 | 1596 | `			}` |
|     22 | 1597 | `			zIn += 3; /* the loop's ++ then steps past the range end */` |
|    271 | 1598 | `		}else if( zIn + 1 < zEnd && zIn[0] == '.' && zIn[1] == '.' ){` |
|      - | 1599 | `			/* Malformed range: mirror php_charmask's three diagnostics. */` |
|      - | 1600 | `			const char *zMsg;` |
|     20 | 1601 | `			if( (const unsigned char *)zList >= zIn ){` |
|      6 | 1602 | `				zMsg = "no character to the left of '..'";` |
|     18 | 1603 | `			}else if( zIn + 2 >= zEnd ){` |
|      6 | 1604 | `				zMsg = "no character to the right of '..'";` |
|     14 | 1605 | `			}else if( zIn[-1] > zIn[2] ){` |
|     12 | 1606 | `				zMsg = "'..'-range needs to be incrementing";` |
|      7 | 1607 | `			}else{` |
|    ! 0 | 1608 | `				zMsg = 0; /* catch-all (e.g. a..b..c) */` |
|      - | 1609 | `			}` |
|     20 | 1610 | `			if( zMsg ){` |
|     29 | 1611 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      9 | 1612 | `					"Invalid '..'-range, %s",zMsg);` |
|     11 | 1613 | `			}else{` |
|    ! 0 | 1614 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1615 | `					"Invalid '..'-range");` |
|      - | 1616 | `			}` |
|      - | 1617 | `			/* Do not consume the dots: the loop's ++ steps one byte so the` |
|      - | 1618 | `			 * dots are re-scanned as literals, exactly like php_charmask. */` |
|     11 | 1619 | `		}else{` |
|    234 | 1620 | `			aMask[c] = 1;` |
|      - | 1621 | `		}` |
|    138 | 1622 | `	}` |
|    110 | 1623 | `}` |
|      - | 1624 | `/*` |
|      - | 1625 | ` * string addcslashes(string $str,string $charlist)` |
|      - | 1626 | ` *  Quote string with slashes in a C style.` |
|      - | 1627 | ` * Parameter` |
|      - | 1628 | ` *  $str:` |
|      - | 1629 | ` *    The string to be escaped.` |
|      - | 1630 | ` *  $charlist:` |
|      - | 1631 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|      - | 1632 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|      - | 1633 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|      - | 1634 | ` * Return` |
|      - | 1635 | ` *  Returns the escaped string.` |
|      - | 1636 | ` * Note:` |
|      - | 1637 | ` *  Character ranges [i.e: 'A..Z'] are supported (see PH7_BuildCharMask).` |
|      - | 1638 | ` */` |
|     34 | 1639 | `static int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1640 | `{` |
|      - | 1641 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - | 1642 | `	char aMask[256];` |
|      - | 1643 | `	int nLen,nMask;` |
|      - | 1644 | `	/* PHP enforces exactly two arguments. */` |
|     38 | 1645 | `	if( nArg != 2 ){` |
|      4 | 1646 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1647 | `			"ArgumentCountError",` |
|      - | 1648 | `			"addcslashes() expects exactly 2 arguments, %d given",` |
|      1 | 1649 | `			nArg` |
|      - | 1650 | `			);` |
|      - | 1651 | `	}` |
|      - | 1652 | `	/* First argument must be a string-ish value.  NULL is deprecated and` |
|      - | 1653 | `	 * treated as the empty string (PHP 8.1). */` |
|     35 | 1654 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      - | 1655 | `		/* Emit deprecation only once, similar to PHP behaviour. */` |
|      3 | 1656 | `		PH7_VmThrowError(pCtx->pVm,0,/* iErr will be patched to 8192 below */` |
|      - | 1657 | `			E_DEPRECATED,` |
|      - | 1658 | `			"addcslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - | 1659 | `			);` |
|      - | 1660 | `		/* treat as empty string; fall through to conversion logic */` |
|     47 | 1661 | `	} else if( ph7_value_is_array(apArg[0]) \|\|` |
|     48 | 1662 | `	          ph7_value_is_object(apArg[0]) \|\|` |
|     30 | 1663 | `	          ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 1664 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1665 | `			"TypeError",` |
|      - | 1666 | `			"addcslashes(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 1667 | `			ph7_type_name(apArg[0])` |
|      - | 1668 | `			);` |
|      - | 1669 | `	}` |
|      - | 1670 | `	/* Second argument must be a string.  NULL is deprecated and treated as` |
|      - | 1671 | `	 * an empty mask per PHP semantics.  Arrays/objects/resources still` |
|      - | 1672 | `	 * trigger a TypeError. */` |
|     35 | 1673 | `	if( ph7_value_is_null(apArg[1]) ){` |
|      3 | 1674 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 1675 | `			E_DEPRECATED,` |
|      - | 1676 | `			"addcslashes(): Passing null to parameter #2 ($characters) of type string is deprecated"` |
|      - | 1677 | `			);` |
|      - | 1678 | `		/* allow through so it becomes empty string below */` |
|     47 | 1679 | `	} else if( ph7_value_is_array(apArg[1]) \|\|` |
|     48 | 1680 | `	          ph7_value_is_object(apArg[1]) \|\|` |
|     30 | 1681 | `	          ph7_value_is_resource(apArg[1]) ){` |
|    ! 0 | 1682 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1683 | `			"TypeError",` |
|      - | 1684 | `			"addcslashes(): Argument #2 ($characters) must be of type string, %s given",` |
|    ! 0 | 1685 | `			ph7_type_name(apArg[1])` |
|      - | 1686 | `			);` |
|      - | 1687 | `	}` |
|      - | 1688 | `	/* Extract the string to process */` |
|     35 | 1689 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1690 | `	/* NULL would never reach here due to the check above. */` |
|     35 | 1691 | `	if( nLen < 1 ){` |
|      - | 1692 | `		/* Empty string returns itself. */` |
|      5 | 1693 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      5 | 1694 | `		return PH7_OK;` |
|      - | 1695 | `	}` |
|      - | 1696 | ``	/* Extract the desired mask and expand any `a..z` ranges into a lookup. */`` |
|     31 | 1697 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|     31 | 1698 | `	PH7_BuildCharMask(pCtx,zMask,nMask,aMask);` |
|     31 | 1699 | `	zEnd = &zIn[nLen];` |
|     31 | 1700 | `	zCur = 0; /* cc warning */` |
|     37 | 1701 | `	for(;;){` |
|     77 | 1702 | `		if( zIn >= zEnd ){` |
|      - | 1703 | `			/* No more input */` |
|     31 | 1704 | `			break;` |
|      - | 1705 | `		}` |
|     49 | 1706 | `		zCur = zIn;` |
|    125 | 1707 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     79 | 1708 | `			zIn++;` |
|      3 | 1709 | `		}` |
|     49 | 1710 | `		if( zIn > zCur ){` |
|      - | 1711 | `			/* Append raw contents */` |
|     43 | 1712 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     20 | 1713 | `		}` |
|     49 | 1714 | `		if( zIn < zEnd ){` |
|      - | 1715 | `			/* Make sure we treat the byte as unsigned to avoid negative values` |
|      - | 1716 | `			 * on platforms where char is signed. */` |
|     29 | 1717 | `			int c = (unsigned char)zIn[0];` |
|      - | 1718 | `			/* Handle special C-like escapes for common control characters first.` |
|      - | 1719 | `			 * PHP outputs "\n" "\r" "\t" "\v" "\f" when those chars are` |
|      - | 1720 | `			 * in the mask. NUL is left to the octal conversion below. */` |
|     29 | 1721 | `			if( c == '\n' ){` |
|      3 | 1722 | `				ph7_result_string(pCtx,"\\n",2);` |
|     28 | 1723 | `			}else if( c == '\r' ){` |
|      3 | 1724 | `				ph7_result_string(pCtx,"\\r",2);` |
|     26 | 1725 | `			}else if( c == '\t' ){` |
|      3 | 1726 | `				ph7_result_string(pCtx,"\\t",2);` |
|     24 | 1727 | `			}else if( c == '\v' ){` |
|      3 | 1728 | `				ph7_result_string(pCtx,"\\v",2);` |
|     22 | 1729 | `			}else if( c == '\f' ){` |
|      3 | 1730 | `				ph7_result_string(pCtx,"\\f",2);` |
|     20 | 1731 | `			}else if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|      - | 1732 | `				/* Convert to octal.  PHP always emits three-digit zero-padded` |
|      - | 1733 | `				 * octal escapes (\001 not \1). */` |
|      7 | 1734 | `				ph7_result_string_format(pCtx,"\\%03o",c);` |
|      4 | 1735 | `			}else{` |
|     13 | 1736 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1737 | `			}` |
|     13 | 1738 | `		}` |
|     49 | 1739 | `		zIn++;` |
|      3 | 1740 | `	}` |
|     31 | 1741 | `	return PH7_OK;` |
|     21 | 1742 | `}` |
|      - | 1743 | `/*` |
|      - | 1744 | ` * string quotemeta(string $str)` |
|      - | 1745 | ` *  Quote meta characters.` |
|      - | 1746 | ` * Parameter` |
|      - | 1747 | ` *  $str:` |
|      - | 1748 | ` *    The string to be escaped.` |
|      - | 1749 | ` * Return` |
|      - | 1750 | ` *  Returns the escaped string.` |
|      - | 1751 | `*/` |
|     10 | 1752 | `static int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1753 | `{` |
|      - | 1754 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1755 | `	char aMask[256];` |
|      - | 1756 | `	int nLen;` |
|     12 | 1757 | `	if( nArg < 1 ){` |
|      - | 1758 | `		/* Nothing to process,retun NULL */` |
|    ! 0 | 1759 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1760 | `		return PH7_OK;` |
|      - | 1761 | `	}` |
|      - | 1762 | `	/* Extract the string to process */` |
|     12 | 1763 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     12 | 1764 | `	if( nLen < 1 ){` |
|      - | 1765 | `		/* Return the empty string */` |
|      3 | 1766 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1767 | `		return PH7_OK;` |
|      - | 1768 | `	}` |
|      - | 1769 | `	/* Fixed meta-character set (no ranges); build the lookup once. */` |
|     10 | 1770 | `	PH7_BuildCharMask(pCtx,".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1,aMask);` |
|     10 | 1771 | `	zEnd = &zIn[nLen];` |
|     10 | 1772 | `	zCur = 0; /* cc warning */` |
|     22 | 1773 | `	for(;;){` |
|     46 | 1774 | `		if( zIn >= zEnd ){` |
|      - | 1775 | `			/* No more input */` |
|     10 | 1776 | `			break;` |
|      - | 1777 | `		}` |
|     38 | 1778 | `		zCur = zIn;` |
|     76 | 1779 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     40 | 1780 | `			zIn++;` |
|      2 | 1781 | `		}` |
|     38 | 1782 | `		if( zIn > zCur ){` |
|      - | 1783 | `			/* Append raw contents */` |
|     20 | 1784 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      9 | 1785 | `		}` |
|     38 | 1786 | `		if( zIn < zEnd ){` |
|     36 | 1787 | `			int c = zIn[0];` |
|     36 | 1788 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|     17 | 1789 | `		}` |
|     38 | 1790 | `		zIn++;` |
|      2 | 1791 | `	}` |
|     10 | 1792 | `	return PH7_OK;` |
|      7 | 1793 | `}` |
|      - | 1794 | `/*` |
|      - | 1795 | ` * string stripslashes(string $str)` |
|      - | 1796 | ` *  Un-quotes a quoted string.` |
|      - | 1797 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1798 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1799 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1800 | ` * Parameter` |
|      - | 1801 | ` *  $str` |
|      - | 1802 | ` *   The input string.` |
|      - | 1803 | ` * Return` |
|      - | 1804 | ` *  Returns a string with backslashes stripped off.` |
|      - | 1805 | ` */` |
|      6 | 1806 | `static int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1807 | `{` |
|      - | 1808 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1809 | `	int nLen;` |
|      7 | 1810 | `	if( nArg < 1 ){` |
|      - | 1811 | `		/* Nothing to process,retun NULL */` |
|    ! 0 | 1812 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1813 | `		return PH7_OK;` |
|      - | 1814 | `	}` |
|      - | 1815 | `	/* Extract the string to process */` |
|      7 | 1816 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 1817 | `	if( zIn == 0 ){` |
|    ! 0 | 1818 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1819 | `		return PH7_OK;` |
|      - | 1820 | `	}` |
|      7 | 1821 | `	zEnd = &zIn[nLen];` |
|      7 | 1822 | `	zCur = 0; /* cc warning */` |
|      - | 1823 | `	/* Encode the string */` |
|      4 | 1824 | `	for(;;){` |
|      9 | 1825 | `		if( zIn >= zEnd ){` |
|      - | 1826 | `			/* No more input */` |
|      5 | 1827 | `			break;` |
|      - | 1828 | `		}` |
|      5 | 1829 | `		zCur = zIn;` |
|     17 | 1830 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 | 1831 | `			zIn++;` |
|      1 | 1832 | `		}` |
|      5 | 1833 | `		if( zIn > zCur ){` |
|      - | 1834 | `			/* Append raw contents */` |
|      5 | 1835 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 1836 | `		}` |
|      5 | 1837 | `		if( &zIn[1] < zEnd ){` |
|      3 | 1838 | `			int c = zIn[1];` |
|      3 | 1839 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - | 1840 | `				/* Ignore the backslash */` |
|      3 | 1841 | `				zIn++;` |
|      1 | 1842 | `			}` |
|      2 | 1843 | `		}else{` |
|      3 | 1844 | `			break;` |
|      - | 1845 | `		}` |
|      1 | 1846 | `	}` |
|      7 | 1847 | `	return PH7_OK;` |
|      4 | 1848 | `}` |
|      - | 1849 | `/*` |
|      - | 1850 | ` * UTF-8-aware HTML entity machinery, shared by htmlspecialchars/htmlentities/` |
|      - | 1851 | ` * htmlspecialchars_decode/html_entity_decode/get_html_translation_table.` |
|      - | 1852 | ` * The implementations live further down in this file, next to the filter_var` |
|      - | 1853 | ` * FULL_SPECIAL_CHARS machinery they reuse (aHtml401Ent[]/FvHtml401Lookup()/` |
|      - | 1854 | ` * FvUtf8Next()). Semantics are byte-exact vs php 8.5.7; PHL is UTF-8-only` |
|      - | 1855 | ` * so every charset argument other than a UTF-8 alias gets PHP's` |
|      - | 1856 | ` * unsupported-charset warning and is treated as UTF-8.` |
|      - | 1857 | ` *` |
|      - | 1858 | ` * Flag model (the PHP-exact ENT_* values, see constant.c): bit 1 = encode/` |
|      - | 1859 | ` * decode single quotes, bit 2 = double quotes (ENT_QUOTES=3, ENT_COMPAT=2,` |
|      - | 1860 | ` * ENT_NOQUOTES=0); bits 16\|32 select the doctype (0=HTML401, 16=XML1,` |
|      - | 1861 | ` * 32=XHTML, 48=HTML5); ENT_IGNORE=4 drops invalid UTF-8 bytes (wins over` |
|      - | 1862 | ` * ENT_SUBSTITUTE=8, which replaces each with U+FFFD; with neither set the` |
|      - | 1863 | ` * whole result collapses to ""); ENT_DISALLOWED=128 substitutes valid but` |
|      - | 1864 | ` * doctype-disallowed codepoints. The shared default is` |
|      - | 1865 | ` * ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 = 11.` |
|      - | 1866 | ` */` |
|      - | 1867 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bAll,int bDoubleEncode);` |
|      - | 1868 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bFull);` |
|      - | 1869 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx);` |
|      - | 1870 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags);` |
|      - | 1871 | `/*` |
|      - | 1872 | ` * string htmlspecialchars(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1873 | ` *                         [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1874 | ` *  Convert the special characters & < > " ' to HTML entities.` |
|      - | 1875 | ` * Return` |
|      - | 1876 | ` *  The escaped string or NULL on failure.` |
|      - | 1877 | ` */` |
|     42 | 1878 | `static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1879 | `{` |
|     43 | 1880 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1881 | `	const char *zIn;` |
|     43 | 1882 | `	int nLen,bDouble = 1;` |
|      - | 1883 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1884 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     43 | 1885 | `	if( nArg < 1 ){` |
|      - | 1886 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1887 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1888 | `		return PH7_OK;` |
|      - | 1889 | `	}` |
|      - | 1890 | `	/* Extract the target string */` |
|     43 | 1891 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     43 | 1892 | `	if( nArg > 1 ){` |
|     35 | 1893 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     17 | 1894 | `	}` |
|     43 | 1895 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     43 | 1896 | `	if( nArg > 3 ){` |
|      7 | 1897 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      3 | 1898 | `	}` |
|     43 | 1899 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,0,bDouble);` |
|     43 | 1900 | `	return PH7_OK;` |
|     22 | 1901 | `}` |
|      - | 1902 | `/*` |
|      - | 1903 | ` * string htmlspecialchars_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401])` |
|      - | 1904 | ` *  Convert the special HTML entities (&amp; &lt; &gt; &quot; and the` |
|      - | 1905 | ` *  numeric/doctype forms of the two quotes) back to characters.` |
|      - | 1906 | ` * Return` |
|      - | 1907 | ` *  The unescaped string or NULL on failure.` |
|      - | 1908 | ` */` |
|     22 | 1909 | `static int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1910 | `{` |
|     23 | 1911 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1912 | `	const char *zIn;` |
|      - | 1913 | `	int nLen;` |
|      - | 1914 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1915 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     23 | 1916 | `	if( nArg < 1 ){` |
|      - | 1917 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1918 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1919 | `		return PH7_OK;` |
|      - | 1920 | `	}` |
|      - | 1921 | `	/* Extract the target string */` |
|     23 | 1922 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 1923 | `	if( nArg > 1 ){` |
|      9 | 1924 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1925 | `	}` |
|     23 | 1926 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,0);` |
|     23 | 1927 | `	return PH7_OK;` |
|     12 | 1928 | `}` |
|      - | 1929 | `/*` |
|      - | 1930 | ` * array get_html_translation_table(int $table = HTML_SPECIALCHARS` |
|      - | 1931 | ` *      [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 [, string $encoding = "UTF-8"]])` |
|      - | 1932 | ` *  Return the translation table used by htmlspecialchars() (HTML_SPECIALCHARS)` |
|      - | 1933 | ` *  or htmlentities() (HTML_ENTITIES) as character => entity pairs.` |
|      - | 1934 | ` * Return` |
|      - | 1935 | ` *  The translation table as an array or NULL on failure.` |
|      - | 1936 | ` */` |
|     12 | 1937 | `static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1938 | `{` |
|     13 | 1939 | `	int iTable = 0; /* HTML_SPECIALCHARS */` |
|     13 | 1940 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|     13 | 1941 | `	if( nArg > 0 ){` |
|     11 | 1942 | `		iTable = ph7_value_to_int(apArg[0]);` |
|      5 | 1943 | `	}` |
|     13 | 1944 | `	if( nArg > 1 ){` |
|      9 | 1945 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1946 | `	}` |
|     13 | 1947 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     13 | 1948 | `	HtmlTranslationTable(pCtx,iTable,iFlags);` |
|     13 | 1949 | `	return PH7_OK;` |
|      1 | 1950 | `}` |
|      - | 1951 | `/*` |
|      - | 1952 | ` * string htmlentities(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1953 | ` *                     [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1954 | ` *  Convert all applicable characters to HTML entities: the specials plus` |
|      - | 1955 | ` *  every codepoint with an HTML 4.01 named entity (aHtml401Ent[]).` |
|      - | 1956 | ` * Return` |
|      - | 1957 | ` *  The encoded string or NULL on failure.` |
|      - | 1958 | ` */` |
|     30 | 1959 | `static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1960 | `{` |
|     31 | 1961 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1962 | `	const char *zIn;` |
|     31 | 1963 | `	int nLen,bDouble = 1;` |
|      - | 1964 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1965 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     31 | 1966 | `	if( nArg < 1 ){` |
|      - | 1967 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1968 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1969 | `		return PH7_OK;` |
|      - | 1970 | `	}` |
|      - | 1971 | `	/* Extract the target string */` |
|     31 | 1972 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 1973 | `	if( nArg > 1 ){` |
|     19 | 1974 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 | 1975 | `	}` |
|     31 | 1976 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     31 | 1977 | `	if( nArg > 3 ){` |
|      3 | 1978 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      1 | 1979 | `	}` |
|     31 | 1980 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,1,bDouble);` |
|     31 | 1981 | `	return PH7_OK;` |
|     16 | 1982 | `}` |
|      - | 1983 | `/*` |
|      - | 1984 | ` * string html_entity_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1985 | ` *                           [, string $encoding = "UTF-8"]])` |
|      - | 1986 | ` *  Convert HTML entities (named — case-sensitive — and numeric, decimal or` |
|      - | 1987 | ` *  hex) back to their UTF-8 characters. The reverse of htmlentities().` |
|      - | 1988 | ` * Return` |
|      - | 1989 | ` *  The decoded string or NULL on failure.` |
|      - | 1990 | ` */` |
|     58 | 1991 | `static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1992 | `{` |
|     59 | 1993 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1994 | `	const char *zIn;` |
|      - | 1995 | `	int nLen;` |
|      - | 1996 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1997 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     59 | 1998 | `	if( nArg < 1 ){` |
|      - | 1999 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 2000 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2001 | `		return PH7_OK;` |
|      - | 2002 | `	}` |
|      - | 2003 | `	/* Extract the target string */` |
|     59 | 2004 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     59 | 2005 | `	if( nArg > 1 ){` |
|     27 | 2006 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     13 | 2007 | `	}` |
|     59 | 2008 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     59 | 2009 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,1);` |
|     59 | 2010 | `	return PH7_OK;` |
|     30 | 2011 | `}` |
|      - | 2012 | `/*` |
|      - | 2013 | ` * int strlen($string)` |
|      - | 2014 | ` *  return the length of the given string.` |
|      - | 2015 | ` * Parameter` |
|      - | 2016 | ` *  string: The string being measured for length.` |
|      - | 2017 | ` * Return` |
|      - | 2018 | ` *  length of the given string.` |
|      - | 2019 | ` */` |
|  18238 | 2020 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2021 | `{` |
|  18243 | 2022 | `	int iLen = 0;` |
|  18243 | 2023 | `	if( nArg > 0 ){` |
|  18243 | 2024 | `		StrNullArgNotice(pCtx,apArg[0],"strlen",1,"$string");` |
|  18243 | 2025 | `		ph7_value_to_string(apArg[0],&iLen);` |
|   9119 | 2026 | `	}` |
|      - | 2027 | `	/* String length */` |
|  18243 | 2028 | `	ph7_result_int(pCtx,iLen);` |
|  18243 | 2029 | `	return PH7_OK;` |
|      5 | 2030 | `}` |
|      - | 2031 | `/*` |
|      - | 2032 | ` * int strcmp(string $str1,string $str2)` |
|      - | 2033 | ` *  Perform a binary safe string comparison.` |
|      - | 2034 | ` * Parameter` |
|      - | 2035 | ` *  str1: The first string` |
|      - | 2036 | ` *  str2: The second string` |
|      - | 2037 | ` * Return` |
|      - | 2038 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2039 | ` *  than str2, and 0 if they are equal.` |
|      - | 2040 | ` */` |
|     72 | 2041 | `static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2042 | `{` |
|      - | 2043 | `	const char *z1,*z2;` |
|      - | 2044 | `	int n1,n2;` |
|      - | 2045 | `	int res;` |
|     73 | 2046 | `	if( nArg < 2 ){` |
|    ! 0 | 2047 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 2048 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 2049 | `		return PH7_OK;` |
|      - | 2050 | `	}` |
|      - | 2051 | `	/* Perform the comparison */` |
|     73 | 2052 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     73 | 2053 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     73 | 2054 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2055 | `	/* Comparison result */` |
|     73 | 2056 | `	ph7_result_int(pCtx,res);` |
|     73 | 2057 | `	return PH7_OK;` |
|     37 | 2058 | `}` |
|      - | 2059 | `/*` |
|      - | 2060 | ` * Natural-order comparison core (Martin Pool's natcompare as adapted by php's` |
|      - | 2061 | ` * ext/standard/strnatcmp.c): digit runs compare numerically — the longer run` |
|      - | 2062 | ` * wins, a leading zero flips to fractional first-difference-wins semantics —` |
|      - | 2063 | ` * everything else compares bytewise with whitespace skipped.` |
|      - | 2064 | ` */` |
|     16 | 2065 | `static int StrNatCompareRight(const char **pa,const char *aEnd,const char **pb,const char *bEnd)` |
|      1 | 2066 | `{` |
|     17 | 2067 | `	int bias = 0;` |
|     30 | 2068 | `	for(;;){` |
|     39 | 2069 | `		int da = (*pa < aEnd) && SyisDigit(**pa);` |
|     39 | 2070 | `		int db = (*pb < bEnd) && SyisDigit(**pb);` |
|     39 | 2071 | `		if( !da && !db ){ return bias; }` |
|     31 | 2072 | `		if( !da ){ return -1; }` |
|     25 | 2073 | `		if( !db ){ return 1; }` |
|     23 | 2074 | `		if( **pa < **pb ){ if( !bias ){ bias = -1; } }` |
|     21 | 2075 | `		else if( **pa > **pb ){ if( !bias ){ bias = 1; } }` |
|     23 | 2076 | `		(*pa)++;` |
|     23 | 2077 | `		(*pb)++;` |
|      1 | 2078 | `	}` |
|      9 | 2079 | `}` |
|      2 | 2080 | `static int StrNatCompareLeft(const char **pa,const char *aEnd,const char **pb,const char *bEnd)` |
|      1 | 2081 | `{` |
|      1 | 2082 | `	for(;;){` |
|      3 | 2083 | `		int da = (*pa < aEnd) && SyisDigit(**pa);` |
|      3 | 2084 | `		int db = (*pb < bEnd) && SyisDigit(**pb);` |
|      3 | 2085 | `		if( !da && !db ){ return 0; }` |
|      3 | 2086 | `		if( !da ){ return -1; }` |
|      3 | 2087 | `		if( !db ){ return 1; }` |
|      3 | 2088 | `		if( **pa < **pb ){ return -1; }` |
|    ! 0 | 2089 | `		if( **pa > **pb ){ return 1; }` |
|    ! 0 | 2090 | `		(*pa)++;` |
|    ! 0 | 2091 | `		(*pb)++;` |
|    ! 0 | 2092 | `	}` |
|      2 | 2093 | `}` |
|     20 | 2094 | `static int StrNatCmpCore(const char *zA,int nA,const char *zB,int nB,int bFold)` |
|      1 | 2095 | `{` |
|     21 | 2096 | `	const char *a = zA,*aEnd = &zA[nA];` |
|     21 | 2097 | `	const char *b = zB,*bEnd = &zB[nB];` |
|     59 | 2098 | `	for(;;){` |
|      - | 2099 | `		int ca,cb;` |
|     73 | 2100 | `		while( a < aEnd && SyisSpace(a[0]) ){ a++; }` |
|     71 | 2101 | `		while( b < bEnd && SyisSpace(b[0]) ){ b++; }` |
|     71 | 2102 | `		ca = (a < aEnd) ? (unsigned char)a[0] : 0;` |
|     71 | 2103 | `		cb = (b < bEnd) ? (unsigned char)b[0] : 0;` |
|     71 | 2104 | `		if( SyisDigit(ca) && SyisDigit(cb) ){` |
|     18 | 2105 | `			int r = (ca == '0' \|\| cb == '0')` |
|      2 | 2106 | `				? StrNatCompareLeft(&a,aEnd,&b,bEnd)` |
|     25 | 2107 | `				: StrNatCompareRight(&a,aEnd,&b,bEnd);` |
|     19 | 2108 | `			if( r ){ return r; }` |
|      3 | 2109 | `			continue;` |
|      - | 2110 | `		}` |
|     53 | 2111 | `		if( ca == 0 && cb == 0 ){ return 0; }` |
|     49 | 2112 | `		if( bFold ){` |
|     49 | 2113 | `			ca = SyToLower(ca);` |
|     49 | 2114 | `			cb = SyToLower(cb);` |
|     24 | 2115 | `		}` |
|     49 | 2116 | `		if( ca < cb ){ return -1; }` |
|     49 | 2117 | `		if( ca > cb ){ return 1; }` |
|     49 | 2118 | `		a++;` |
|     49 | 2119 | `		b++;` |
|      1 | 2120 | `	}` |
|     11 | 2121 | `}` |
|      - | 2122 | `/*` |
|      - | 2123 | ` * int strnatcmp(string $string1, string $string2)` |
|      - | 2124 | ` * int strnatcasecmp(string $string1, string $string2)` |
|      - | 2125 | ` *  Natural-order string comparison ("img2" < "img10"), case folded for the` |
|      - | 2126 | ` *  latter. php 8.2+ normalizes the result to -1/0/1.` |
|      - | 2127 | ` */` |
|     20 | 2128 | `static int PH7_builtin_strnatcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2129 | `{` |
|      - | 2130 | `	const char *z1,*z2,*zFunc;` |
|      - | 2131 | `	int n1,n2,bFold;` |
|     21 | 2132 | `	if( nArg < 2 ){` |
|    ! 0 | 2133 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2134 | `		return PH7_OK;` |
|      - | 2135 | `	}` |
|     21 | 2136 | `	zFunc = ph7_function_name(pCtx);` |
|     21 | 2137 | `	bFold = zFunc[sizeof("strnat")-1] == 'c'; /* strnatCasecmp */` |
|     21 | 2138 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     21 | 2139 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     21 | 2140 | `	ph7_result_int(pCtx,StrNatCmpCore(z1,n1,z2,n2,bFold));` |
|     21 | 2141 | `	return PH7_OK;` |
|     11 | 2142 | `}` |
|      - | 2143 | `/*` |
|      - | 2144 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 2145 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 2146 | ` * Parameter` |
|      - | 2147 | ` *  str1: The first string` |
|      - | 2148 | ` *  str2: The second string` |
|      - | 2149 | ` * Return` |
|      - | 2150 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2151 | ` *  than str2, and 0 if they are equal.` |
|      - | 2152 | ` */` |
|     66 | 2153 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2154 | `{` |
|      - | 2155 | `	const char *z1,*z2;` |
|      - | 2156 | `	int res;` |
|      - | 2157 | `	int n;` |
|     68 | 2158 | `	if( nArg < 3 ){` |
|      - | 2159 | `		/* Perform a standard comparison */` |
|    ! 0 | 2160 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 2161 | `	}` |
|      - | 2162 | `	/* Desired comparison length */` |
|     68 | 2163 | `	n  = ph7_value_to_int(apArg[2]);` |
|     68 | 2164 | `	if( n < 0 ){` |
|      - | 2165 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 2166 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2167 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 2168 | `			ph7_function_name(pCtx));` |
|      - | 2169 | `	}` |
|      - | 2170 | `	/* Perform the comparison */` |
|     66 | 2171 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     66 | 2172 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     66 | 2173 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 2174 | `	/* Comparison result */` |
|     66 | 2175 | `	ph7_result_int(pCtx,res);` |
|     66 | 2176 | `	return PH7_OK;` |
|     35 | 2177 | `}` |
|      - | 2178 | `/*` |
|      - | 2179 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 2180 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 2181 | ` * Parameter` |
|      - | 2182 | ` *  str1: The first string` |
|      - | 2183 | ` *  str2: The second string` |
|      - | 2184 | ` * Return` |
|      - | 2185 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2186 | ` *  than str2, and 0 if they are equal.` |
|      - | 2187 | ` */` |
|    140 | 2188 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2189 | `{` |
|      - | 2190 | `	const char *z1,*z2;` |
|      - | 2191 | `	int n1,n2;` |
|      - | 2192 | `	int res;` |
|    141 | 2193 | `	if( nArg < 2 ){` |
|    ! 0 | 2194 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 2195 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 2196 | `		return PH7_OK;` |
|      - | 2197 | `	}` |
|      - | 2198 | `	/* Perform the comparison */` |
|    141 | 2199 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|    141 | 2200 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|    141 | 2201 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2202 | `	/* Comparison result */` |
|    141 | 2203 | `	ph7_result_int(pCtx,res);` |
|    141 | 2204 | `	return PH7_OK;` |
|     71 | 2205 | `}` |
|      - | 2206 | `/*` |
|      - | 2207 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 2208 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 2209 | ` * Parameter` |
|      - | 2210 | ` *  $str1: The first string` |
|      - | 2211 | ` *  $str2: The second string` |
|      - | 2212 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 2213 | ` * Return` |
|      - | 2214 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2215 | ` *  than str2, and 0 if they are equal.` |
|      - | 2216 | ` */` |
|     44 | 2217 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2218 | `{` |
|      - | 2219 | `	const char *z1,*z2;` |
|      - | 2220 | `	int res;` |
|      - | 2221 | `	int n;` |
|     49 | 2222 | `	if( nArg < 3 ){` |
|      - | 2223 | `		/* Perform a standard comparison */` |
|    ! 0 | 2224 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 2225 | `	}` |
|      - | 2226 | `	/* Desired comparison length */` |
|     49 | 2227 | `	n  = ph7_value_to_int(apArg[2]);` |
|     49 | 2228 | `	if( n < 0 ){` |
|      - | 2229 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 2230 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2231 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 2232 | `			ph7_function_name(pCtx));` |
|      - | 2233 | `	}` |
|      - | 2234 | `	/* Perform the comparison */` |
|     47 | 2235 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     47 | 2236 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     47 | 2237 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 2238 | `	/* Comparison result */` |
|     47 | 2239 | `	ph7_result_int(pCtx,res);` |
|     47 | 2240 | `	return PH7_OK;` |
|     27 | 2241 | `}` |
|      - | 2242 | `/*` |
|      - | 2243 | ` * Implode context [i.e: it's private data].` |
|      - | 2244 | ` * A pointer to the following structure is forwarded` |
|      - | 2245 | ` * verbatim to the array walker callback defined below.` |
|      - | 2246 | ` */` |
|      - | 2247 | `struct implode_data {` |
|      - | 2248 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 2249 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 2250 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 2251 | `	int nSeplen;          /* Separator length */` |
|      - | 2252 | `	int bFirst;           /* TRUE if first call */` |
|      - | 2253 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 2254 | `	sxi32 rc;             /* Captured allocation rc; SXERR_MEM => the builtin raises an OOM fatal */` |
|      - | 2255 | `};` |
|      - | 2256 | `/*` |
|      - | 2257 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 2258 | ` * The following routine is invoked for each array entry passed` |
|      - | 2259 | ` * to the implode() function.` |
|      - | 2260 | ` */` |
| 149548 | 2261 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 2262 | `{` |
|  74774 | 2263 | `	SXUNUSED(pKey);` |
| 149553 | 2264 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 2265 | `	const char *zData;` |
|      - | 2266 | `	int nLen;` |
| 149553 | 2267 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 2268 | `		if( pData->nSeplen > 0 ){` |
|      3 | 2269 | `			if( !pData->bFirst ){` |
|      - | 2270 | `				/* append the separator first */` |
|      3 | 2271 | `				if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 2272 | `					pData->rc = SXERR_MEM;` |
|    ! 0 | 2273 | `					return PH7_ABORT;` |
|      - | 2274 | `				}` |
|      2 | 2275 | `			}else{` |
|    ! 0 | 2276 | `				pData->bFirst = 0;` |
|      - | 2277 | `			}` |
|      1 | 2278 | `		}` |
|      - | 2279 | `		/* Recurse */` |
|      3 | 2280 | `		pData->bFirst = 1;` |
|      3 | 2281 | `		pData->nRecCount++;` |
|      3 | 2282 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 2283 | `		pData->nRecCount--;` |
|      - | 2284 | `		/* Propagate an allocation failure surfaced deeper in the recursion. */` |
|      3 | 2285 | `		if( pData->rc != SXRET_OK ){` |
|    ! 0 | 2286 | `			return PH7_ABORT;` |
|      - | 2287 | `		}` |
|      3 | 2288 | `		return PH7_OK;` |
|      - | 2289 | `	}` |
|      - | 2290 | `	/* Extract the string representation of the entry value */` |
| 149551 | 2291 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 2292 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 149551 | 2293 | `	if( pData->bFirst ){` |
|  33475 | 2294 | `		pData->bFirst = 0;` |
| 132816 | 2295 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 2296 | `		/* append the separator first */` |
| 116065 | 2297 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 2298 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 2299 | `			return PH7_ABORT;` |
|      - | 2300 | `		}` |
|  58030 | 2301 | `	}` |
|      - | 2302 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 149551 | 2303 | `	if( nLen > 0 ){` |
| 136953 | 2304 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2305 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 2306 | `			return PH7_ABORT;` |
|      - | 2307 | `		}` |
|  68474 | 2308 | `	}` |
| 149551 | 2309 | `	return PH7_OK;` |
|  74779 | 2310 | `}` |
|      - | 2311 | `/*` |
|      - | 2312 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 2313 | ` * string implode(array $pieces,...)` |
|      - | 2314 | ` *  Join array elements with a string.` |
|      - | 2315 | ` * $glue` |
|      - | 2316 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 2317 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 2318 | ` * $pieces` |
|      - | 2319 | ` *   The array of strings to implode.` |
|      - | 2320 | ` * Return` |
|      - | 2321 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 2322 | ` *  order, with the glue string between each element.` |
|      - | 2323 | ` */` |
|  33492 | 2324 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2325 | `{` |
|      - | 2326 | `	struct implode_data imp_data;` |
|  33497 | 2327 | `	int i = 1;` |
|  33497 | 2328 | `	if( nArg < 1 ){` |
|      - | 2329 | `		/* Missing argument,return NULL */` |
|    ! 0 | 2330 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2331 | `		return PH7_OK;` |
|      - | 2332 | `	}` |
|      - | 2333 | `	/* Prepare the implode context */` |
|  33497 | 2334 | `	imp_data.pCtx = pCtx;` |
|  33497 | 2335 | `	imp_data.bRecursive = 0;` |
|  33497 | 2336 | `	imp_data.bFirst = 1;` |
|  33497 | 2337 | `	imp_data.nRecCount = 0;` |
|  33497 | 2338 | `	imp_data.rc = SXRET_OK;` |
|  33497 | 2339 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  33495 | 2340 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  16750 | 2341 | `	}else{` |
|      3 | 2342 | `		imp_data.zSep = 0;` |
|      3 | 2343 | `		imp_data.nSeplen = 0;` |
|      3 | 2344 | `		i = 0;` |
|      - | 2345 | `	}` |
|  33497 | 2346 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2347 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2348 | `	}` |
|      - | 2349 | `	/* Start the 'join' process */` |
|  66989 | 2350 | `	while( i < nArg ){` |
|  33497 | 2351 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2352 | `			/* Iterate throw array entries */` |
|  33497 | 2353 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2354 | `			/* Surface a callback allocation failure as a fatal */` |
|  33497 | 2355 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2356 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2357 | `			}` |
|  16751 | 2358 | `		}else{` |
|      - | 2359 | `			const char *zData;` |
|      - | 2360 | `			int nLen;` |
|      - | 2361 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 2362 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2363 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 2364 | `			if( imp_data.bFirst ){` |
|    ! 0 | 2365 | `				imp_data.bFirst = 0;` |
|    ! 0 | 2366 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2367 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 2368 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2369 | `				}` |
|    ! 0 | 2370 | `			}` |
|      - | 2371 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 2372 | `			if( nLen > 0 ){` |
|    ! 0 | 2373 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2374 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2375 | `				}` |
|    ! 0 | 2376 | `			}` |
|      - | 2377 | `		}` |
|  33497 | 2378 | `		i++;` |
|      5 | 2379 | `	}` |
|  33497 | 2380 | `	return PH7_OK;` |
|  16751 | 2381 | `}` |
|      - | 2382 | `/*` |
|      - | 2383 | ` * Symisc eXtension:` |
|      - | 2384 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 2385 | ` * Purpose` |
|      - | 2386 | ` *  Same as implode() but recurse on arrays.` |
|      - | 2387 | ` * Example:` |
|      - | 2388 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 2389 | ` *   echo implode_recursive("/",$a);` |
|      - | 2390 | ` *   Will output` |
|      - | 2391 | ` *     usr/home/dean.` |
|      - | 2392 | ` *   While the standard implode would produce.` |
|      - | 2393 | ` *    usr/Array.` |
|      - | 2394 | ` * Parameter` |
|      - | 2395 | ` *  Refer to implode().` |
|      - | 2396 | ` * Return` |
|      - | 2397 | ` *  Refer to implode().` |
|      - | 2398 | ` */` |
|     12 | 2399 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2400 | `{` |
|      - | 2401 | `	struct implode_data imp_data;` |
|     13 | 2402 | `	int i = 1;` |
|     13 | 2403 | `	if( nArg < 1 ){` |
|      - | 2404 | `		/* Missing argument,return NULL */` |
|      3 | 2405 | `		ph7_result_null(pCtx);` |
|      3 | 2406 | `		return PH7_OK;` |
|      - | 2407 | `	}` |
|      - | 2408 | `	/* Prepare the implode context */` |
|     11 | 2409 | `	imp_data.pCtx = pCtx;` |
|     11 | 2410 | `	imp_data.bRecursive = 1;` |
|     11 | 2411 | `	imp_data.bFirst = 1;` |
|     11 | 2412 | `	imp_data.nRecCount = 0;` |
|     11 | 2413 | `	imp_data.rc = SXRET_OK;` |
|     11 | 2414 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 2415 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 2416 | `	}else{` |
|    ! 0 | 2417 | `		imp_data.zSep = 0;` |
|    ! 0 | 2418 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 2419 | `		i = 0;` |
|      - | 2420 | `	}` |
|     11 | 2421 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2422 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2423 | `	}` |
|      - | 2424 | `	/* Start the 'join' process */` |
|     21 | 2425 | `	while( i < nArg ){` |
|     11 | 2426 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2427 | `			/* Iterate throw array entries */` |
|      3 | 2428 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2429 | `			/* Surface a callback allocation failure as a fatal */` |
|      3 | 2430 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2431 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2432 | `			}` |
|      2 | 2433 | `		}else{` |
|      - | 2434 | `			const char *zData;` |
|      - | 2435 | `			int nLen;` |
|      - | 2436 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 2437 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2438 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 2439 | `			if( imp_data.bFirst ){` |
|      9 | 2440 | `				imp_data.bFirst = 0;` |
|      4 | 2441 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2442 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 2443 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2444 | `				}` |
|    ! 0 | 2445 | `			}` |
|      - | 2446 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 2447 | `			if( nLen > 0 ){` |
|      9 | 2448 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2449 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2450 | `				}` |
|      4 | 2451 | `			}` |
|      - | 2452 | `		}` |
|     11 | 2453 | `		i++;` |
|      1 | 2454 | `	}` |
|     11 | 2455 | `	return PH7_OK;` |
|      7 | 2456 | `}` |
|      - | 2457 | `/*` |
|      - | 2458 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 2459 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 2460 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 2461 | ` * Parameters` |
|      - | 2462 | ` *  $delimiter` |
|      - | 2463 | ` *   The boundary string.` |
|      - | 2464 | ` * $string` |
|      - | 2465 | ` *   The input string.` |
|      - | 2466 | ` * $limit` |
|      - | 2467 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 2468 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 2469 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 2470 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 2471 | ` * Returns` |
|      - | 2472 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 2473 | ` *  on boundaries formed by the delimiter.` |
|      - | 2474 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 2475 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 2476 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 2477 | ` *  will be returned.` |
|      - | 2478 | ` * NOTE:` |
|      - | 2479 | ` *  Negative limit is not supported.` |
|      - | 2480 | ` */` |
|   6582 | 2481 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2482 | `{` |
|      - | 2483 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 2484 | `	int nDelim,nStrlen,iLimit;` |
|      - | 2485 | `	ph7_value *pArray;` |
|      - | 2486 | `	ph7_value *pValue;` |
|      - | 2487 | `	sxu32 nOfft;` |
|      - | 2488 | `	sxi32 rc;` |
|   6587 | 2489 | `	if( nArg < 2 ){` |
|      - | 2490 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2491 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2492 | `		return PH7_OK;` |
|      - | 2493 | `	}` |
|      - | 2494 | `	/* Extract the delimiter */` |
|   6587 | 2495 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   6587 | 2496 | `	if( nDelim < 1 ){` |
|      - | 2497 | `		/* Empty delimiter: PHP 8 throws a catchable ValueError. */` |
|      5 | 2498 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2499 | `			"explode(): Argument #1 ($separator) must not be empty");` |
|      - | 2500 | `	}` |
|      - | 2501 | `	/* Extract the string */` |
|   6583 | 2502 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   6583 | 2503 | `	if( nStrlen < 1 ){` |
|      - | 2504 | `		/* Empty string: normally an array with a single empty element (PHP behavior).` |
|      - | 2505 | `		 * A negative limit drops the last -limit components, so the sole empty` |
|      - | 2506 | `		 * component is dropped and the result is an empty array. */` |
|      7 | 2507 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      7 | 2508 | `		if( pArrayTmp == 0 ){` |
|      - | 2509 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 2510 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2511 | `			return PH7_OK;` |
|      - | 2512 | `		}` |
|      7 | 2513 | `		if( !(nArg > 2 && ph7_value_to_int(apArg[2]) < 0) ){` |
|      5 | 2514 | `			ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      5 | 2515 | `			if( pValueTmp == 0 ){` |
|      - | 2516 | `				/* Out of memory,return FALSE */` |
|    ! 0 | 2517 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 2518 | `				return PH7_OK;` |
|      - | 2519 | `			}` |
|      5 | 2520 | `			ph7_value_string(pValueTmp, "", 0);` |
|      5 | 2521 | `			if( ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp) != SXRET_OK ){` |
|    ! 0 | 2522 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2523 | `			}` |
|      2 | 2524 | `		}` |
|      7 | 2525 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      7 | 2526 | `		return PH7_OK;` |
|      - | 2527 | `	}` |
|      - | 2528 | `	/* Point to the end of the string */` |
|   6577 | 2529 | `	zEnd = &zString[nStrlen];` |
|      - | 2530 | `	/* Create the array */` |
|   6577 | 2531 | `	pArray =  ph7_context_new_array(pCtx);` |
|   6577 | 2532 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   6577 | 2533 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2534 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 2535 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2536 | `		return PH7_OK;` |
|      - | 2537 | `	}` |
|      - | 2538 | `	/* Set a defualt limit */` |
|   6577 | 2539 | `	iLimit = SXI32_HIGH;` |
|   6577 | 2540 | `	if( nArg > 2 ){` |
|     38 | 2541 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     38 | 2542 | `		if( iLimit < 0 ){` |
|      - | 2543 | `			/* Negative limit: keep all components except the last -iLimit (PHP).` |
|      - | 2544 | `			 * Pre-count the components (delimiters + 1), then emit only the first` |
|      - | 2545 | `			 * nKeep CLEAN components — no trailing-remainder merge (the difference` |
|      - | 2546 | `			 * from the positive path). nKeep <= 0 drops everything -> empty array. */` |
|     17 | 2547 | `			int nTotal = 1,nKeep;` |
|     17 | 2548 | `			const char *zScan = zString;` |
|      - | 2549 | `			sxu32 nScanOfft;` |
|     57 | 2550 | `			while( SyBlobSearch(zScan,(sxu32)(zEnd - zScan),zDelim,nDelim,&nScanOfft) == SXRET_OK ){` |
|     41 | 2551 | `				nTotal++;` |
|     41 | 2552 | `				zScan = &zScan[nScanOfft + nDelim];` |
|      1 | 2553 | `			}` |
|     17 | 2554 | `			nKeep = nTotal + iLimit; /* iLimit < 0, so this is nTotal - (-iLimit) */` |
|     49 | 2555 | `			while( nKeep > (int)ph7_array_count(pArray)` |
|     39 | 2556 | `				&& SyBlobSearch(zString,(sxu32)(zEnd - zString),zDelim,nDelim,&nOfft) == SXRET_OK ){` |
|      - | 2557 | `				/* Emit the next clean component */` |
|     23 | 2558 | `				zCur = &zString[nOfft];` |
|     23 | 2559 | `				ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|     23 | 2560 | `				if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2561 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2562 | `				}` |
|     23 | 2563 | `				zString = &zCur[nDelim];` |
|     23 | 2564 | `				ph7_value_reset_string_cursor(pValue);` |
|      1 | 2565 | `			}` |
|     17 | 2566 | `			ph7_result_value(pCtx,pArray);` |
|     17 | 2567 | `			return PH7_OK;` |
|      - | 2568 | `		}` |
|     22 | 2569 | `		if( iLimit == 0 ){` |
|      5 | 2570 | `			iLimit = 1;` |
|      2 | 2571 | `		}` |
|     22 | 2572 | `		iLimit--;` |
|      9 | 2573 | `	}` |
|      - | 2574 | `	/* Start exploding */` |
|  80670 | 2575 | `	for(;;){` |
| 161345 | 2576 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 161345 | 2577 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 2578 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   6561 | 2579 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   6561 | 2580 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2581 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2582 | `			}` |
|   6561 | 2583 | `			break;` |
|      - | 2584 | `		}` |
|      - | 2585 | `		/* Point to the desired offset */` |
| 154789 | 2586 | `		zCur = &zString[nOfft];` |
|      - | 2587 | `		/* Perform the store operation (may be empty) */` |
| 154789 | 2588 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 154789 | 2589 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2590 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2591 | `		}` |
|      - | 2592 | `		/* Point beyond the delimiter */` |
| 154789 | 2593 | `		zString = &zCur[nDelim];` |
|      - | 2594 | `		/* Reset the cursor */` |
| 154789 | 2595 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 2596 | `	}` |
|      - | 2597 | `	/* Return the freshly created array */` |
|   6561 | 2598 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2599 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 2600 | `	 * released as soon we return from this foregin function.` |
|      - | 2601 | `	 */` |
|   6561 | 2602 | `	return PH7_OK;` |
|   3296 | 2603 | `}` |
|      - | 2604 | `/*` |
|      - | 2605 | ` * string trim(string $str[,string $charlist ])` |
|      - | 2606 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2607 | ` * Parameters` |
|      - | 2608 | ` *  $str` |
|      - | 2609 | ` *   The string that will be trimmed.` |
|      - | 2610 | ` * $charlist` |
|      - | 2611 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2612 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2613 | ` *   With .. you can specify a range of characters.` |
|      - | 2614 | ` * Returns.` |
|      - | 2615 | ` *  Thr processed string.` |
|      - | 2616 | ` * NOTE:` |
|      - | 2617 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2618 | ` */` |
|  14614 | 2619 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2620 | `{` |
|  14619 | 2621 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"trim",1,"$string"); }` |
|      - | 2622 | `	const char *zString;` |
|      - | 2623 | `	int nLen;` |
|  14619 | 2624 | `	if( nArg < 1 ){` |
|      - | 2625 | `		/* Missing arguments,return null */` |
|    ! 0 | 2626 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2627 | `		return PH7_OK;` |
|      - | 2628 | `	}` |
|      - | 2629 | `	/* Extract the target string */` |
|  14619 | 2630 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  14619 | 2631 | `	if( nLen < 1 ){` |
|      - | 2632 | `		/* Empty string,return */` |
|   1037 | 2633 | `		ph7_result_string(pCtx,"",0);` |
|   1037 | 2634 | `		return PH7_OK;` |
|      - | 2635 | `	}` |
|      - | 2636 | `	/* Start the trim process */` |
|  13587 | 2637 | `	if( nArg < 2 ){` |
|      - | 2638 | `		SyString sStr;` |
|      - | 2639 | `		/* Remove white spaces and NUL bytes */` |
|  13557 | 2640 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  34009 | 2641 | `		SyStringFullTrimSafe(&sStr);` |
|  13557 | 2642 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   6781 | 2643 | `	}else{` |
|      - | 2644 | `		/* Char list */` |
|      - | 2645 | `		const char *zList;` |
|      - | 2646 | `		int nListlen;` |
|     33 | 2647 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     33 | 2648 | `		if( nListlen < 1 ){` |
|      - | 2649 | `			/* Return the string unchanged */` |
|      6 | 2650 | `			ph7_result_string(pCtx,zString,nLen);` |
|      4 | 2651 | `		}else{` |
|      - | 2652 | `			char aMask[256];` |
|     29 | 2653 | `			const char *zEnd = &zString[nLen];` |
|     29 | 2654 | `			const char *zCur = zString;` |
|     29 | 2655 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2656 | `			/* Left trim */` |
|     79 | 2657 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     53 | 2658 | `				zCur++;` |
|      3 | 2659 | `			}` |
|      - | 2660 | `			/* Right trim */` |
|     79 | 2661 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     53 | 2662 | `				zEnd--;` |
|      3 | 2663 | `			}` |
|     29 | 2664 | `			if( zCur >= zEnd ){` |
|      - | 2665 | `				/* Return the empty string */` |
|    ! 0 | 2666 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2667 | `			}else{` |
|     29 | 2668 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2669 | `			}` |
|      - | 2670 | `		}` |
|      - | 2671 | `	}` |
|  13587 | 2672 | `	return PH7_OK;` |
|   7312 | 2673 | `}` |
|      - | 2674 | `/*` |
|      - | 2675 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 2676 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 2677 | ` * Parameters` |
|      - | 2678 | ` *  $str` |
|      - | 2679 | ` *   The string that will be trimmed.` |
|      - | 2680 | ` * $charlist` |
|      - | 2681 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2682 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2683 | ` *   With .. you can specify a range of characters.` |
|      - | 2684 | ` * Returns.` |
|      - | 2685 | ` *  Thr processed string.` |
|      - | 2686 | ` * NOTE:` |
|      - | 2687 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2688 | ` */` |
|     38 | 2689 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2690 | `{` |
|     41 | 2691 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"rtrim",1,"$string"); }` |
|      - | 2692 | `	const char *zString;` |
|      - | 2693 | `	int nLen;` |
|     41 | 2694 | `	if( nArg < 1 ){` |
|      - | 2695 | `		/* Missing arguments,return null */` |
|    ! 0 | 2696 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2697 | `		return PH7_OK;` |
|      - | 2698 | `	}` |
|      - | 2699 | `	/* Extract the target string */` |
|     41 | 2700 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 2701 | `	if( nLen < 1 ){` |
|      - | 2702 | `		/* Empty string,return */` |
|      7 | 2703 | `		ph7_result_string(pCtx,"",0);` |
|      7 | 2704 | `		return PH7_OK;` |
|      - | 2705 | `	}` |
|      - | 2706 | `	/* Start the trim process */` |
|     35 | 2707 | `	if( nArg < 2 ){` |
|      - | 2708 | `		SyString sStr;` |
|      - | 2709 | `		/* Remove white spaces and NUL bytes*/` |
|     19 | 2710 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     48 | 2711 | `		SyStringRightTrimSafe(&sStr);` |
|     19 | 2712 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|     10 | 2713 | `	}else{` |
|      - | 2714 | `		/* Char list */` |
|      - | 2715 | `		const char *zList;` |
|      - | 2716 | `		int nListlen;` |
|     17 | 2717 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     17 | 2718 | `		if( nListlen < 1 ){` |
|      - | 2719 | `			/* Return the string unchanged */` |
|    ! 0 | 2720 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 2721 | `		}else{` |
|      - | 2722 | `			char aMask[256];` |
|     17 | 2723 | `			const char *zEnd = &zString[nLen];` |
|     17 | 2724 | `			const char *zCur = zString;` |
|     17 | 2725 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2726 | `			/* Right trim */` |
|     37 | 2727 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     22 | 2728 | `				zEnd--;` |
|      2 | 2729 | `			}` |
|     17 | 2730 | `			if( zEnd <= zCur ){` |
|      - | 2731 | `				/* Return the empty string */` |
|    ! 0 | 2732 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2733 | `			}else{` |
|     17 | 2734 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2735 | `			}` |
|      - | 2736 | `		}` |
|      - | 2737 | `	}` |
|     35 | 2738 | `	return PH7_OK;` |
|     22 | 2739 | `}` |
|      - | 2740 | `/*` |
|      - | 2741 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 2742 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2743 | ` * Parameters` |
|      - | 2744 | ` *  $str` |
|      - | 2745 | ` *   The string that will be trimmed.` |
|      - | 2746 | ` * $charlist` |
|      - | 2747 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2748 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2749 | ` *   With .. you can specify a range of characters.` |
|      - | 2750 | ` * Returns.` |
|      - | 2751 | ` *  Thr processed string.` |
|      - | 2752 | ` * NOTE:` |
|      - | 2753 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2754 | ` */` |
|     46 | 2755 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2756 | `{` |
|     51 | 2757 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"ltrim",1,"$string"); }` |
|      - | 2758 | `	const char *zString;` |
|      - | 2759 | `	int nLen;` |
|     51 | 2760 | `	if( nArg < 1 ){` |
|      - | 2761 | `		/* Missing arguments,return null */` |
|    ! 0 | 2762 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2763 | `		return PH7_OK;` |
|      - | 2764 | `	}` |
|      - | 2765 | `	/* Extract the target string */` |
|     51 | 2766 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     51 | 2767 | `	if( nLen < 1 ){` |
|      - | 2768 | `		/* Empty string,return */` |
|     27 | 2769 | `		ph7_result_string(pCtx,"",0);` |
|     27 | 2770 | `		return PH7_OK;` |
|      - | 2771 | `	}` |
|      - | 2772 | `	/* Start the trim process */` |
|     28 | 2773 | `	if( nArg < 2 ){` |
|      - | 2774 | `		SyString sStr;` |
|      - | 2775 | `		/* Remove white spaces and NUL byte */` |
|      5 | 2776 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     13 | 2777 | `		SyStringLeftTrimSafe(&sStr);` |
|      5 | 2778 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      3 | 2779 | `	}else{` |
|      - | 2780 | `		/* Char list */` |
|      - | 2781 | `		const char *zList;` |
|      - | 2782 | `		int nListlen;` |
|     24 | 2783 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     24 | 2784 | `		if( nListlen < 1 ){` |
|      - | 2785 | `			/* Return the string unchanged */` |
|      3 | 2786 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2787 | `		}else{` |
|      - | 2788 | `			char aMask[256];` |
|     22 | 2789 | `			const char *zEnd = &zString[nLen];` |
|     22 | 2790 | `			const char *zCur = zString;` |
|     22 | 2791 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2792 | `			/* Left trim */` |
|     54 | 2793 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     36 | 2794 | `				zCur++;` |
|      4 | 2795 | `			}` |
|     22 | 2796 | `			if( zCur >= zEnd ){` |
|      - | 2797 | `				/* Return the empty string */` |
|    ! 0 | 2798 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2799 | `			}else{` |
|     22 | 2800 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2801 | `			}` |
|      - | 2802 | `		}` |
|      - | 2803 | `	}` |
|     28 | 2804 | `	return PH7_OK;` |
|     28 | 2805 | `}` |
|      - | 2806 | `/*` |
|      - | 2807 | ` * string strtolower(string $str)` |
|      - | 2808 | ` *  Make a string lowercase.` |
|      - | 2809 | ` * Parameters` |
|      - | 2810 | ` *  $str` |
|      - | 2811 | ` *   The input string.` |
|      - | 2812 | ` * Returns.` |
|      - | 2813 | ` *  The lowercased string.` |
|      - | 2814 | ` */` |
|  33464 | 2815 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2816 | `{` |
|  33469 | 2817 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtolower",1,"$string"); }` |
|      - | 2818 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2819 | `	int nLen;` |
|  33469 | 2820 | `	if( nArg < 1 ){` |
|      - | 2821 | `		/* Missing arguments,return null */` |
|    ! 0 | 2822 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2823 | `		return PH7_OK;` |
|      - | 2824 | `	}` |
|      - | 2825 | `	/* Extract the target string */` |
|  33469 | 2826 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  33469 | 2827 | `	if( nLen < 1 ){` |
|      - | 2828 | `		/* Empty string,return */` |
|      5 | 2829 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2830 | `		return PH7_OK;` |
|      - | 2831 | `	}` |
|      - | 2832 | `	/* Perform the requested operation */` |
|  33465 | 2833 | `	zEnd = &zString[nLen];` |
| 105590 | 2834 | `	for(;;){` |
| 211185 | 2835 | `		if( zString >= zEnd ){` |
|      - | 2836 | `			/* No more input,break immediately */` |
|  33465 | 2837 | `			break;` |
|      - | 2838 | `		}` |
| 177725 | 2839 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2840 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2841 | `			zCur = zString;` |
|    ! 0 | 2842 | `			zString++;` |
|    ! 0 | 2843 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2844 | `				zString++;` |
|    ! 0 | 2845 | `			}` |
|      - | 2846 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2847 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2848 | `		}else{` |
| 177725 | 2849 | `			int c = zString[0];` |
| 177725 | 2850 | `			if( SyisUpper(c) ){` |
| 175169 | 2851 | `				c = SyToLower(zString[0]);` |
|  87582 | 2852 | `			}` |
|      - | 2853 | `			/* Append character */` |
| 177725 | 2854 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2855 | `			/* Advance the cursor */` |
| 177725 | 2856 | `			zString++;` |
|      - | 2857 | `		}` |
|      5 | 2858 | `	}` |
|  33465 | 2859 | `	return PH7_OK;` |
|  16737 | 2860 | `}` |
|      - | 2861 | `/*` |
|      - | 2862 | ` * string strtolower(string $str)` |
|      - | 2863 | ` *  Make a string uppercase.` |
|      - | 2864 | ` * Parameters` |
|      - | 2865 | ` *  $str` |
|      - | 2866 | ` *   The input string.` |
|      - | 2867 | ` * Returns.` |
|      - | 2868 | ` *  The uppercased string.` |
|      - | 2869 | ` */` |
|     70 | 2870 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2871 | `{` |
|     75 | 2872 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtoupper",1,"$string"); }` |
|      - | 2873 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2874 | `	int nLen;` |
|     75 | 2875 | `	if( nArg < 1 ){` |
|      - | 2876 | `		/* Missing arguments,return null */` |
|    ! 0 | 2877 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2878 | `		return PH7_OK;` |
|      - | 2879 | `	}` |
|      - | 2880 | `	/* Extract the target string */` |
|     75 | 2881 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     75 | 2882 | `	if( nLen < 1 ){` |
|      - | 2883 | `		/* Empty string,return */` |
|      5 | 2884 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2885 | `		return PH7_OK;` |
|      - | 2886 | `	}` |
|      - | 2887 | `	/* Perform the requested operation */` |
|     71 | 2888 | `	zEnd = &zString[nLen];` |
|    139 | 2889 | `	for(;;){` |
|    283 | 2890 | `		if( zString >= zEnd ){` |
|      - | 2891 | `			/* No more input,break immediately */` |
|     71 | 2892 | `			break;` |
|      - | 2893 | `		}` |
|    217 | 2894 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2895 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2896 | `			zCur = zString;` |
|    ! 0 | 2897 | `			zString++;` |
|    ! 0 | 2898 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2899 | `				zString++;` |
|    ! 0 | 2900 | `			}` |
|      - | 2901 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2902 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2903 | `		}else{` |
|    217 | 2904 | `			int c = zString[0];` |
|    217 | 2905 | `			if( SyisLower(c) ){` |
|    204 | 2906 | `				c = SyToUpper(zString[0]);` |
|    100 | 2907 | `			}` |
|      - | 2908 | `			/* Append character */` |
|    217 | 2909 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2910 | `			/* Advance the cursor */` |
|    217 | 2911 | `			zString++;` |
|      - | 2912 | `		}` |
|      5 | 2913 | `	}` |
|     71 | 2914 | `	return PH7_OK;` |
|     40 | 2915 | `}` |
|      - | 2916 | `/*` |
|      - | 2917 | ` * string ucfirst(string $str)` |
|      - | 2918 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2919 | ` *  character is alphabetic.` |
|      - | 2920 | ` * Parameters` |
|      - | 2921 | ` *  $str` |
|      - | 2922 | ` *   The input string.` |
|      - | 2923 | ` * Returns.` |
|      - | 2924 | ` *  The processed string.` |
|      - | 2925 | ` */` |
|      4 | 2926 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2927 | `{` |
|      - | 2928 | `	const char *zString,*zEnd;` |
|      - | 2929 | `	int nLen,c;` |
|      5 | 2930 | `	if( nArg < 1 ){` |
|      - | 2931 | `		/* Missing arguments,return null */` |
|    ! 0 | 2932 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2933 | `		return PH7_OK;` |
|      - | 2934 | `	}` |
|      - | 2935 | `	/* Extract the target string */` |
|      5 | 2936 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2937 | `	if( nLen < 1 ){` |
|      - | 2938 | `		/* Empty string,return */` |
|      3 | 2939 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2940 | `		return PH7_OK;` |
|      - | 2941 | `	}` |
|      - | 2942 | `	/* Perform the requested operation */` |
|      3 | 2943 | `	zEnd = &zString[nLen];` |
|      3 | 2944 | `	c = zString[0];` |
|      3 | 2945 | `	if( SyisLower(c) ){` |
|      3 | 2946 | `		c = SyToUpper(c);` |
|      1 | 2947 | `	}` |
|      - | 2948 | `	/* Append the first character */` |
|      3 | 2949 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2950 | `	zString++;` |
|      3 | 2951 | `	if( zString < zEnd ){` |
|      - | 2952 | `		/* Append the rest of the input verbatim */` |
|      3 | 2953 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2954 | `	}` |
|      3 | 2955 | `	return PH7_OK;` |
|      3 | 2956 | `}` |
|      - | 2957 | `/*` |
|      - | 2958 | ` * string lcfirst(string $str)` |
|      - | 2959 | ` *  Make a string's first character lowercase.` |
|      - | 2960 | ` * Parameters` |
|      - | 2961 | ` *  $str` |
|      - | 2962 | ` *   The input string.` |
|      - | 2963 | ` * Returns.` |
|      - | 2964 | ` *  The processed string.` |
|      - | 2965 | ` */` |
|      4 | 2966 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2967 | `{` |
|      - | 2968 | `	const char *zString,*zEnd;` |
|      - | 2969 | `	int nLen,c;` |
|      5 | 2970 | `	if( nArg < 1 ){` |
|      - | 2971 | `		/* Missing arguments,return null */` |
|    ! 0 | 2972 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2973 | `		return PH7_OK;` |
|      - | 2974 | `	}` |
|      - | 2975 | `	/* Extract the target string */` |
|      5 | 2976 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2977 | `	if( nLen < 1 ){` |
|      - | 2978 | `		/* Empty string,return */` |
|      3 | 2979 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2980 | `		return PH7_OK;` |
|      - | 2981 | `	}` |
|      - | 2982 | `	/* Perform the requested operation */` |
|      3 | 2983 | `	zEnd = &zString[nLen];` |
|      3 | 2984 | `	c = zString[0];` |
|      3 | 2985 | `	if( SyisUpper(c) ){` |
|      3 | 2986 | `		c = SyToLower(c);` |
|      1 | 2987 | `	}` |
|      - | 2988 | `	/* Append the first character */` |
|      3 | 2989 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2990 | `	zString++;` |
|      3 | 2991 | `	if( zString < zEnd ){` |
|      - | 2992 | `		/* Append the rest of the input verbatim */` |
|      3 | 2993 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2994 | `	}` |
|      3 | 2995 | `	return PH7_OK;` |
|      3 | 2996 | `}` |
|      - | 2997 | `/*` |
|      - | 2998 | ` * int ord(string $string)` |
|      - | 2999 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 3000 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 3001 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 3002 | ` * Parameters` |
|      - | 3003 | ` *  $string` |
|      - | 3004 | ` *   The input string.` |
|      - | 3005 | ` * Returns` |
|      - | 3006 | ` *  The ASCII value as an integer.` |
|      - | 3007 | ` */` |
|    182 | 3008 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3009 | `{` |
|      - | 3010 | `	const char *zString;` |
|      - | 3011 | `	int nLen,c;` |
|      - | 3012 | `	/* PHP requires exactly one argument. */` |
|    185 | 3013 | `	if( nArg != 1 ){` |
|      4 | 3014 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3015 | `			"ArgumentCountError",` |
|      - | 3016 | `			"ord() expects exactly 1 argument, %d given",` |
|      1 | 3017 | `			nArg` |
|      - | 3018 | `			);` |
|      - | 3019 | `	}` |
|      - | 3020 | `	/* Passing null is deprecated (E_DEPRECATED).  PHP emits this before` |
|      - | 3021 | `	 * the empty-string deprecation, so we check null first. */` |
|    182 | 3022 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 3023 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 3024 | `			"ord(): Passing null to parameter #1 ($character) "` |
|      - | 3025 | `			"of type string is deprecated"` |
|      - | 3026 | `			);` |
|      1 | 3027 | `	}` |
|      - | 3028 | `	/* Extract the target string */` |
|    182 | 3029 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    182 | 3030 | `	if( nLen < 1 ){` |
|      - | 3031 | `		/* Empty string is deprecated (E_DEPRECATED). */` |
|      5 | 3032 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 3033 | `			"ord(): Providing an empty string is deprecated"` |
|      - | 3034 | `			);` |
|      5 | 3035 | `		ph7_result_int(pCtx,0);` |
|      5 | 3036 | `		return PH7_OK;` |
|      - | 3037 | `	}` |
|      - | 3038 | `	/* A string longer than one byte is deprecated (E_DEPRECATED). */` |
|    178 | 3039 | `	if( nLen > 1 ){` |
|      7 | 3040 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 3041 | `			"ord(): Providing a string that is not one byte long "` |
|      - | 3042 | `			"is deprecated. Use ord($str[0]) instead"` |
|      - | 3043 | `			);` |
|      3 | 3044 | `	}` |
|      - | 3045 | `	/* Extract the ASCII value of the first character */` |
|    178 | 3046 | `	c = (unsigned char)zString[0];` |
|      - | 3047 | `	/* Return that value */` |
|    178 | 3048 | `	ph7_result_int(pCtx,c);` |
|    178 | 3049 | `	return PH7_OK;` |
|     94 | 3050 | `}` |
|      - | 3051 | `/*` |
|      - | 3052 | ` * string chr(int $codepoint)` |
|      - | 3053 | ` *  Returns a one-character string containing the character specified` |
|      - | 3054 | ` *  by the given codepoint.  Any integer is accepted; values outside` |
|      - | 3055 | ` *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.` |
|      - | 3056 | ` * Parameters` |
|      - | 3057 | ` *  $codepoint` |
|      - | 3058 | ` *   An integer codepoint.  Values outside 0-255 are deprecated and` |
|      - | 3059 | ` *   will be constrained to a single byte.` |
|      - | 3060 | ` * Returns` |
|      - | 3061 | ` *  A single-character string.` |
|      - | 3062 | ` */` |
|   7114 | 3063 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3064 | `{` |
|      - | 3065 | `	int c;` |
|      - | 3066 | `	unsigned char ch;` |
|      - | 3067 | `	/* PHP requires exactly one argument. */` |
|   7117 | 3068 | `	if( nArg != 1 ){` |
|      4 | 3069 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3070 | `			"ArgumentCountError",` |
|      - | 3071 | `			"chr() expects exactly 1 argument, %d given",` |
|      1 | 3072 | `			nArg` |
|      - | 3073 | `			);` |
|      - | 3074 | `	}` |
|      - | 3075 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 3076 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 3077 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 3078 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|   7114 | 3079 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      - | 3080 | `		char zBuf[120];` |
|      4 | 3081 | `		SyBufferFormat(zBuf,sizeof(zBuf),` |
|      - | 3082 | `			"Implicit conversion from float %g to int loses precision",` |
|      1 | 3083 | `			ph7_value_to_double(apArg[0])` |
|      - | 3084 | `			);` |
|      3 | 3085 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zBuf);` |
|      1 | 3086 | `	}` |
|      - | 3087 | `	/* Extract the codepoint. */` |
|   7114 | 3088 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 3089 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 3090 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 3091 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 3092 | `	 * name to avoid the API double-prefixing it. */` |
|   7114 | 3093 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 3094 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 3095 | `			E_DEPRECATED,` |
|      - | 3096 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 3097 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 3098 | `			"The value used will be constrained using % 256"` |
|      - | 3099 | `			);` |
|      2 | 3100 | `	}` |
|      - | 3101 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 3102 | `	 * when taking the address of a wider int. */` |
|   7114 | 3103 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 3104 | `	/* Return the specified character */` |
|   7114 | 3105 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|   7114 | 3106 | `	return PH7_OK;` |
|   3560 | 3107 | `}` |
|      - | 3108 | `/*` |
|      - | 3109 | ` * Binary to hex consumer callback.` |
|      - | 3110 | ` * This callback is the default consumer used by the hash functions` |
|      - | 3111 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 3112 | ` */` |
|   3118 | 3113 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 3114 | `{` |
|      - | 3115 | `	/* Append hex chunk verbatim */` |
|   3120 | 3116 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   3120 | 3117 | `	return SXRET_OK;` |
|      2 | 3118 | `}` |
|      - | 3119 |  |
|      - | 3120 | `/*` |
|      - | 3121 | ` * string bin2hex(string $str)` |
|      - | 3122 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 3123 | ` * Parameters` |
|      - | 3124 | ` *  $str` |
|      - | 3125 | ` *   The input string.` |
|      - | 3126 | ` * Returns.` |
|      - | 3127 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 3128 | ` */` |
|    130 | 3129 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3130 | `{` |
|      - | 3131 | `	const char *zString;` |
|      - | 3132 | `	int nLen;` |
|      - | 3133 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|    133 | 3134 | `	if( nArg != 1 ){` |
|      4 | 3135 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3136 | `			"ArgumentCountError",` |
|      - | 3137 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|      1 | 3138 | `			nArg` |
|      - | 3139 | `			);` |
|      - | 3140 | `	}` |
|      - | 3141 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 3142 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 3143 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 3144 | `	 */` |
|    194 | 3145 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|     64 | 3146 | `		( ph7_value_is_object(apArg[0]) &&` |
|    ! 0 | 3147 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|    ! 0 | 3148 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|    ! 0 | 3149 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 3150 | `		)` |
|      - | 3151 | `	){` |
|    ! 0 | 3152 | `		const char *zType = ph7_type_name(apArg[0]);` |
|    ! 0 | 3153 | `		if( ph7_value_is_object(apArg[0]) ){` |
|    ! 0 | 3154 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    ! 0 | 3155 | `			if( pInst && pInst->pClass ){` |
|    ! 0 | 3156 | `				zType = SyStringData(&pInst->pClass->sName);` |
|    ! 0 | 3157 | `			}` |
|    ! 0 | 3158 | `		}` |
|    ! 0 | 3159 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3160 | `			"TypeError",` |
|      - | 3161 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 3162 | `			zType` |
|      - | 3163 | `			);` |
|      - | 3164 | `	}` |
|      - | 3165 | `	/* Extract the target string */` |
|    130 | 3166 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    130 | 3167 | `	if( nLen < 1 ){` |
|      - | 3168 | `		/* Empty string,return */` |
|     13 | 3169 | `		ph7_result_string(pCtx,"",0);` |
|     13 | 3170 | `		return PH7_OK;` |
|      - | 3171 | `	}` |
|      - | 3172 | `	/* Perform the requested operation */` |
|    118 | 3173 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|    118 | 3174 | `	return PH7_OK;` |
|     68 | 3175 | `}` |
|      - | 3176 |  |
|      - | 3177 | `/* Search callback signature */` |
|      - | 3178 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 3179 | `/*` |
|      - | 3180 | ` * Case-insensitive pattern match.` |
|      - | 3181 | ` * Brute force is the default search method used here.` |
|      - | 3182 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 3183 | ` * well for short/medium texts on modern hardware.` |
|      - | 3184 | ` */` |
|    298 | 3185 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      2 | 3186 | `{` |
|    300 | 3187 | `	const char *zpIn = (const char *)pPattern;` |
|    300 | 3188 | `	const char *zIn = (const char *)pText;` |
|    300 | 3189 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    300 | 3190 | `	const char *zEnd = &zIn[nLen];` |
|      - | 3191 | `	const char *zPtr,*zPtr2;` |
|      - | 3192 | `	int c,d;` |
|    300 | 3193 | `	if( iPatLen > nLen ){` |
|      - | 3194 | `		/* Don't bother processing */` |
|     67 | 3195 | `		return SXERR_NOTFOUND;` |
|      - | 3196 | `	}` |
|    860 | 3197 | `	for(;;){` |
|   1722 | 3198 | `		if( zIn >= zEnd ){` |
|    194 | 3199 | `			break;` |
|      - | 3200 | `		}` |
|   1530 | 3201 | `		c = SyToLower(zIn[0]);` |
|   1530 | 3202 | `		d = SyToLower(zpIn[0]);` |
|   1530 | 3203 | `		if( c == d ){` |
|    182 | 3204 | `			zPtr   = &zIn[1];` |
|    182 | 3205 | `			zPtr2  = &zpIn[1];` |
|    141 | 3206 | `			for(;;){` |
|    284 | 3207 | `				if( zPtr2 >= zpEnd ){` |
|      - | 3208 | `					/* Pattern found */` |
|     41 | 3209 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 3210 | `					return SXRET_OK;` |
|      - | 3211 | `				}` |
|    244 | 3212 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 3213 | `					break;` |
|      - | 3214 | `				}` |
|    244 | 3215 | `				c = SyToLower(zPtr[0]);` |
|    244 | 3216 | `				d = SyToLower(zPtr2[0]);` |
|    244 | 3217 | `				if( c != d ){` |
|    142 | 3218 | `					break;` |
|      - | 3219 | `				}` |
|    103 | 3220 | `				zPtr++; zPtr2++;` |
|      1 | 3221 | `			}` |
|     70 | 3222 | `		}` |
|   1490 | 3223 | `		zIn++;` |
|      2 | 3224 | `	}` |
|      - | 3225 | `	/* Pattern not found */` |
|    194 | 3226 | `	return SXERR_NOTFOUND;` |
|    151 | 3227 | `}` |
|      - | 3228 | `/*` |
|      - | 3229 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3230 | ` *  Find the first occurrence of a string.` |
|      - | 3231 | ` * Parameters` |
|      - | 3232 | ` *  $haystack` |
|      - | 3233 | ` *   The input string.` |
|      - | 3234 | ` * $needle` |
|      - | 3235 | ` *   Search pattern (must be a string).` |
|      - | 3236 | ` * $before_needle` |
|      - | 3237 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3238 | ` *   of the needle (excluding the needle).` |
|      - | 3239 | ` * Return` |
|      - | 3240 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3241 | ` */` |
|      6 | 3242 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3243 | `{` |
|      7 | 3244 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3245 | `	const char *zBlob,*zPattern;` |
|      - | 3246 | `	int nLen,nPatLen;` |
|      - | 3247 | `	sxu32 nOfft;` |
|      - | 3248 | `	sxi32 rc;` |
|      7 | 3249 | `	if( nArg < 2 ){` |
|      - | 3250 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3251 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3252 | `		return PH7_OK;` |
|      - | 3253 | `	}` |
|      - | 3254 | `	/* Extract the needle and the haystack */` |
|      7 | 3255 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 3256 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 3257 | `	nOfft = 0; /* cc warning */` |
|      9 | 3258 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3259 | `		int before = 0;` |
|      - | 3260 | `		/* Perform the lookup */` |
|      5 | 3261 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3262 | `		if( rc != SXRET_OK ){` |
|      - | 3263 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3264 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3265 | `			return PH7_OK;` |
|      - | 3266 | `		}` |
|      - | 3267 | `		/* Return the portion of the string */` |
|      5 | 3268 | `		if( nArg > 2 ){` |
|      3 | 3269 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3270 | `		}` |
|      5 | 3271 | `		if( before ){` |
|      3 | 3272 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3273 | `		}else{` |
|      3 | 3274 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3275 | `		}` |
|      3 | 3276 | `	}else{` |
|      3 | 3277 | `		ph7_result_bool(pCtx,0);` |
|      - | 3278 | `	}` |
|      7 | 3279 | `	return PH7_OK;` |
|      4 | 3280 | `}` |
|      - | 3281 | `/*` |
|      - | 3282 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3283 | ` *  Case-insensitive strstr().` |
|      - | 3284 | ` * Parameters` |
|      - | 3285 | ` *  $haystack` |
|      - | 3286 | ` *   The input string.` |
|      - | 3287 | ` * $needle` |
|      - | 3288 | ` *   Search pattern (must be a string).` |
|      - | 3289 | ` * $before_needle` |
|      - | 3290 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3291 | ` *   of the needle (excluding the needle).` |
|      - | 3292 | ` * Return` |
|      - | 3293 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3294 | ` */` |
|      4 | 3295 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3296 | `{` |
|      5 | 3297 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3298 | `	const char *zBlob,*zPattern;` |
|      - | 3299 | `	int nLen,nPatLen;` |
|      - | 3300 | `	sxu32 nOfft;` |
|      - | 3301 | `	sxi32 rc;` |
|      5 | 3302 | `	if( nArg < 2 ){` |
|      - | 3303 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3304 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3305 | `		return PH7_OK;` |
|      - | 3306 | `	}` |
|      - | 3307 | `	/* Extract the needle and the haystack */` |
|      5 | 3308 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3309 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 3310 | `	nOfft = 0; /* cc warning */` |
|      7 | 3311 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3312 | `		int before = 0;` |
|      - | 3313 | `		/* Perform the lookup */` |
|      5 | 3314 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3315 | `		if( rc != SXRET_OK ){` |
|      - | 3316 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3317 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3318 | `			return PH7_OK;` |
|      - | 3319 | `		}` |
|      - | 3320 | `		/* Return the portion of the string */` |
|      5 | 3321 | `		if( nArg > 2 ){` |
|      3 | 3322 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3323 | `		}` |
|      5 | 3324 | `		if( before ){` |
|      3 | 3325 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3326 | `		}else{` |
|      3 | 3327 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3328 | `		}` |
|      3 | 3329 | `	}else{` |
|    ! 0 | 3330 | `		ph7_result_bool(pCtx,0);` |
|      - | 3331 | `	}` |
|      5 | 3332 | `	return PH7_OK;` |
|      3 | 3333 | `}` |
|      - | 3334 | `/*` |
|      - | 3335 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3336 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 3337 | ` * Parameters` |
|      - | 3338 | ` *  $haystack` |
|      - | 3339 | ` *   The input string.` |
|      - | 3340 | ` * $needle` |
|      - | 3341 | ` *   Search pattern (must be a string).` |
|      - | 3342 | ` * $offset` |
|      - | 3343 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3344 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3345 | ` *   of haystack.` |
|      - | 3346 | ` * Return` |
|      - | 3347 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3348 | ` */` |
|   1468 | 3349 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3350 | `{` |
|   1473 | 3351 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strpos",1,"$haystack"); }` |
|   1473 | 3352 | `	if( nArg > 1 ){ StrNullArgNotice(pCtx,apArg[1],"strpos",2,"$needle"); }` |
|   1473 | 3353 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3354 | `	const char *zBlob,*zPattern;` |
|      - | 3355 | `	int nLen,nPatLen,nStart;` |
|      - | 3356 | `	sxu32 nOfft;` |
|      - | 3357 | `	sxi32 rc;` |
|   1473 | 3358 | `	if( nArg < 2 ){` |
|      - | 3359 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3360 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3361 | `		return PH7_OK;` |
|      - | 3362 | `	}` |
|      - | 3363 | `	/* Extract the needle and the haystack */` |
|   1473 | 3364 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|   1473 | 3365 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|   1473 | 3366 | `	nOfft = 0; /* cc warning */` |
|   1473 | 3367 | `	nStart = 0;` |
|      - | 3368 | `	/* Peek the starting offset if available */` |
|   1473 | 3369 | `	if( nArg > 2 ){` |
|     15 | 3370 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3371 | `		if( nStart < 0 ){` |
|    ! 0 | 3372 | `			nStart = -nStart;` |
|    ! 0 | 3373 | `		}` |
|     15 | 3374 | `		if( nStart >= nLen ){` |
|      - | 3375 | `			/* Invalid offset */` |
|    ! 0 | 3376 | `			nStart = 0;` |
|    ! 0 | 3377 | `		}else{` |
|     15 | 3378 | `			zBlob += nStart;` |
|     15 | 3379 | `			nLen -= nStart;` |
|      - | 3380 | `		}` |
|      7 | 3381 | `	}` |
|   1473 | 3382 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3383 | `		/* Perform the lookup */` |
|   1471 | 3384 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|   1471 | 3385 | `		if( rc != SXRET_OK ){` |
|      - | 3386 | `			/* Pattern not found,return FALSE */` |
|    779 | 3387 | `			ph7_result_bool(pCtx,0);` |
|    779 | 3388 | `			return PH7_OK;` |
|      - | 3389 | `		}` |
|      - | 3390 | `		/* Return the pattern position */` |
|    697 | 3391 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|    351 | 3392 | `	}else{` |
|      3 | 3393 | `		ph7_result_bool(pCtx,0);` |
|      - | 3394 | `	}` |
|    699 | 3395 | `	return PH7_OK;` |
|    739 | 3396 | `}` |
|      - | 3397 | `/*` |
|      - | 3398 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|      - | 3399 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|      - | 3400 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|      - | 3401 | ` * TypeError for arrays, resources, and objects without __toString.` |
|      - | 3402 | ` *` |
|      - | 3403 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|      - | 3404 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|      - | 3405 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|      - | 3406 | ` *` |
|      - | 3407 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|      - | 3408 | ` * is valid until pTmp is released or pArg is mutated.` |
|      - | 3409 | ` */` |
|    668 | 3410 | `static sxi32 StrPredicateResolveArg(` |
|      - | 3411 | `	ph7_context *pCtx,` |
|      - | 3412 | `	ph7_value *pArg,` |
|      - | 3413 | `	const char *zFunc,` |
|      - | 3414 | `	int iArgNum,` |
|      - | 3415 | `	const char *zParamName,` |
|      - | 3416 | `	const char *zTypeStr, /* Declared type in the TypeError, e.g. "string" / "?string" */` |
|      - | 3417 | `	const char *zNullMsg,` |
|      - | 3418 | `	ph7_value *pTmp,` |
|      - | 3419 | `	const char **pzOut,` |
|      - | 3420 | `	int *pnOut` |
|      2 | 3421 | `){` |
|    670 | 3422 | `	if( ph7_value_is_null(pArg) ){` |
|     13 | 3423 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zNullMsg);` |
|     13 | 3424 | `		*pzOut = "";` |
|     13 | 3425 | `		*pnOut = 0;` |
|     13 | 3426 | `		return PH7_OK;` |
|      - | 3427 | `	}` |
|   1010 | 3428 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|    656 | 3429 | `	    ( ph7_value_is_object(pArg) &&` |
|     72 | 3430 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|     48 | 3431 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|     24 | 3432 | `	        "__toString",sizeof("__toString")-1) == 0` |
|      - | 3433 | `	    )` |
|      - | 3434 | `	){` |
|    ! 0 | 3435 | `		const char *zType = ph7_type_name(pArg);` |
|    ! 0 | 3436 | `		if( ph7_value_is_object(pArg) ){` |
|    ! 0 | 3437 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|    ! 0 | 3438 | `			if( pInst && pInst->pClass ){` |
|    ! 0 | 3439 | `				zType = SyStringData(&pInst->pClass->sName);` |
|    ! 0 | 3440 | `			}` |
|    ! 0 | 3441 | `		}` |
|    ! 0 | 3442 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3443 | `			"TypeError",` |
|      - | 3444 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|    ! 0 | 3445 | `			zFunc, iArgNum, zParamName, zTypeStr, zType` |
|      - | 3446 | `			);` |
|      - | 3447 | `	}` |
|    658 | 3448 | `	if( ph7_value_is_object(pArg) ){` |
|     49 | 3449 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     49 | 3450 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 3451 | `			"__toString",sizeof("__toString")-1);` |
|     49 | 3452 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|     49 | 3453 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|     49 | 3454 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|     49 | 3455 | `		return PH7_OK;` |
|      - | 3456 | `	}` |
|    610 | 3457 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|    610 | 3458 | `	return PH7_OK;` |
|    336 | 3459 | `}` |
|      - | 3460 | `/*` |
|      - | 3461 | ` * bool str_contains(string $haystack, string $needle)` |
|      - | 3462 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|      - | 3463 | ` * Return` |
|      - | 3464 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|      - | 3465 | ` */` |
|     92 | 3466 | `static int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3467 | `{` |
|      - | 3468 | `	const char *zHaystack,*zNeedle;` |
|      - | 3469 | `	int nHayLen,nNeedleLen;` |
|      - | 3470 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3471 | `	sxi32 rc;` |
|     95 | 3472 | `	if( nArg != 2 ){` |
|      8 | 3473 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3474 | `			"ArgumentCountError",` |
|      - | 3475 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|      2 | 3476 | `			nArg` |
|      - | 3477 | `			);` |
|      - | 3478 | `	}` |
|     90 | 3479 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     90 | 3480 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     90 | 3481 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack","string",` |
|      - | 3482 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|      - | 3483 | `		"of type string is deprecated",` |
|      - | 3484 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     90 | 3485 | `	if( rc != PH7_OK ) goto out;` |
|     90 | 3486 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle","string",` |
|      - | 3487 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|      - | 3488 | `		"of type string is deprecated",` |
|      - | 3489 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     90 | 3490 | `	if( rc != PH7_OK ) goto out;` |
|     90 | 3491 | `	if( nNeedleLen < 1 ){` |
|     13 | 3492 | `		ph7_result_bool(pCtx,1);` |
|     84 | 3493 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3494 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3495 | `	}else{` |
|    104 | 3496 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|     34 | 3497 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|     70 | 3498 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|      - | 3499 | `	}` |
|     90 | 3500 | `	rc = PH7_OK;` |
|     44 | 3501 | `out:` |
|     90 | 3502 | `	PH7_MemObjRelease(&sHayTmp);` |
|     90 | 3503 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     90 | 3504 | `	return rc;` |
|     49 | 3505 | `}` |
|      - | 3506 | `/*` |
|      - | 3507 | ` * bool str_starts_with(string $haystack, string $needle)` |
|      - | 3508 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|      - | 3509 | ` * Return` |
|      - | 3510 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|      - | 3511 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3512 | ` */` |
|     62 | 3513 | `static int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3514 | `{` |
|      - | 3515 | `	const char *zHaystack,*zNeedle;` |
|      - | 3516 | `	int nHayLen,nNeedleLen;` |
|      - | 3517 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3518 | `	sxi32 rc;` |
|     64 | 3519 | `	if( nArg != 2 ){` |
|      8 | 3520 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3521 | `			"ArgumentCountError",` |
|      - | 3522 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|      2 | 3523 | `			nArg` |
|      - | 3524 | `			);` |
|      - | 3525 | `	}` |
|     59 | 3526 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     59 | 3527 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     59 | 3528 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack","string",` |
|      - | 3529 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3530 | `		"of type string is deprecated",` |
|      - | 3531 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     59 | 3532 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3533 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle","string",` |
|      - | 3534 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3535 | `		"of type string is deprecated",` |
|      - | 3536 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     59 | 3537 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3538 | `	if( nNeedleLen < 1 ){` |
|     13 | 3539 | `		ph7_result_bool(pCtx,1);` |
|     53 | 3540 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3541 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3542 | `	}else{` |
|     58 | 3543 | `		ph7_result_bool(pCtx,` |
|     38 | 3544 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3545 | `	}` |
|     59 | 3546 | `	rc = PH7_OK;` |
|     29 | 3547 | `out:` |
|     59 | 3548 | `	PH7_MemObjRelease(&sHayTmp);` |
|     59 | 3549 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     59 | 3550 | `	return rc;` |
|     33 | 3551 | `}` |
|      - | 3552 | `/*` |
|      - | 3553 | ` * bool str_ends_with(string $haystack, string $needle)` |
|      - | 3554 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|      - | 3555 | ` * Return` |
|      - | 3556 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|      - | 3557 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3558 | ` */` |
|     62 | 3559 | `static int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3560 | `{` |
|      - | 3561 | `	const char *zHaystack,*zNeedle;` |
|      - | 3562 | `	int nHayLen,nNeedleLen;` |
|      - | 3563 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3564 | `	sxi32 rc;` |
|     64 | 3565 | `	if( nArg != 2 ){` |
|      8 | 3566 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3567 | `			"ArgumentCountError",` |
|      - | 3568 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|      2 | 3569 | `			nArg` |
|      - | 3570 | `			);` |
|      - | 3571 | `	}` |
|     59 | 3572 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     59 | 3573 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     59 | 3574 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack","string",` |
|      - | 3575 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3576 | `		"of type string is deprecated",` |
|      - | 3577 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     59 | 3578 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3579 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle","string",` |
|      - | 3580 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3581 | `		"of type string is deprecated",` |
|      - | 3582 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     59 | 3583 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3584 | `	if( nNeedleLen < 1 ){` |
|     13 | 3585 | `		ph7_result_bool(pCtx,1);` |
|     53 | 3586 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3587 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3588 | `	}else{` |
|     58 | 3589 | `		ph7_result_bool(pCtx,` |
|     38 | 3590 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3591 | `	}` |
|     59 | 3592 | `	rc = PH7_OK;` |
|     29 | 3593 | `out:` |
|     59 | 3594 | `	PH7_MemObjRelease(&sHayTmp);` |
|     59 | 3595 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     59 | 3596 | `	return rc;` |
|     33 | 3597 | `}` |
|      - | 3598 | `/*` |
|      - | 3599 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3600 | ` *  Case-insensitive strpos.` |
|      - | 3601 | ` * Parameters` |
|      - | 3602 | ` *  $haystack` |
|      - | 3603 | ` *   The input string.` |
|      - | 3604 | ` * $needle` |
|      - | 3605 | ` *   Search pattern (must be a string).` |
|      - | 3606 | ` * $offset` |
|      - | 3607 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3608 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3609 | ` *   of haystack.` |
|      - | 3610 | ` * Return` |
|      - | 3611 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3612 | ` */` |
|    196 | 3613 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3614 | `{` |
|    198 | 3615 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3616 | `	const char *zBlob,*zPattern;` |
|      - | 3617 | `	int nLen,nPatLen,nStart;` |
|      - | 3618 | `	sxu32 nOfft;` |
|      - | 3619 | `	sxi32 rc;` |
|    198 | 3620 | `	if( nArg < 2 ){` |
|      - | 3621 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3622 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3623 | `		return PH7_OK;` |
|      - | 3624 | `	}` |
|      - | 3625 | `	/* Extract the needle and the haystack */` |
|    198 | 3626 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    198 | 3627 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    198 | 3628 | `	nOfft = 0; /* cc warning */` |
|    198 | 3629 | `	nStart = 0;` |
|      - | 3630 | `	/* Peek the starting offset if available */` |
|    198 | 3631 | `	if( nArg > 2 ){` |
|      5 | 3632 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 3633 | `		if( nStart < 0 ){` |
|      3 | 3634 | `			nStart = -nStart;` |
|      1 | 3635 | `		}` |
|      5 | 3636 | `		if( nStart >= nLen ){` |
|      - | 3637 | `			/* Invalid offset */` |
|    ! 0 | 3638 | `			nStart = 0;` |
|    ! 0 | 3639 | `		}else{` |
|      5 | 3640 | `			zBlob += nStart;` |
|      5 | 3641 | `			nLen -= nStart;` |
|      - | 3642 | `		}` |
|      2 | 3643 | `	}` |
|    198 | 3644 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3645 | `		/* Perform the lookup */` |
|    198 | 3646 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    198 | 3647 | `		if( rc != SXRET_OK ){` |
|      - | 3648 | `			/* Pattern not found,return FALSE */` |
|    184 | 3649 | `			ph7_result_bool(pCtx,0);` |
|    184 | 3650 | `			return PH7_OK;` |
|      - | 3651 | `		}` |
|      - | 3652 | `		/* Return the pattern position */` |
|     15 | 3653 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 3654 | `	}else{` |
|    ! 0 | 3655 | `		ph7_result_bool(pCtx,0);` |
|      - | 3656 | `	}` |
|     15 | 3657 | `	return PH7_OK;` |
|    100 | 3658 | `}` |
|      - | 3659 | `/*` |
|      - | 3660 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3661 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 3662 | ` * Parameters` |
|      - | 3663 | ` *  $haystack` |
|      - | 3664 | ` *   The input string.` |
|      - | 3665 | ` * $needle` |
|      - | 3666 | ` *   Search pattern (must be a string).` |
|      - | 3667 | ` * $offset` |
|      - | 3668 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3669 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3670 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3671 | ` * Return` |
|      - | 3672 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3673 | ` */` |
|     40 | 3674 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3675 | `{` |
|      - | 3676 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     41 | 3677 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3678 | `	int nLen,nPatLen;` |
|      - | 3679 | `	sxu32 nOfft;` |
|      - | 3680 | `	sxi32 rc;` |
|     41 | 3681 | `	if( nArg < 2 ){` |
|      - | 3682 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3683 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3684 | `		return PH7_OK;` |
|      - | 3685 | `	}` |
|      - | 3686 | `	/* Extract the needle and the haystack */` |
|     41 | 3687 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 3688 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3689 | `	/* Point to the end of the pattern */` |
|     41 | 3690 | `	zPtr = &zBlob[nLen - 1];` |
|     41 | 3691 | `	zEnd = &zBlob[nLen];` |
|      - | 3692 | `	/* Save the starting posistion */` |
|     41 | 3693 | `	zStart = zBlob;` |
|     41 | 3694 | `	nOfft = 0; /* cc warning */` |
|      - | 3695 | `	/* Peek the starting offset if available */` |
|     41 | 3696 | `	if( nArg > 2 ){` |
|      - | 3697 | `		int nStart;` |
|     21 | 3698 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 3699 | `		if( nStart < 0 ){` |
|     11 | 3700 | `			nStart = -nStart;` |
|     11 | 3701 | `			if( nStart >= nLen ){` |
|      - | 3702 | `				/* Invalid offset */` |
|      3 | 3703 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3704 | `				return PH7_OK;` |
|    ! 0 | 3705 | `			}else{` |
|      9 | 3706 | `				nLen -= nStart;` |
|      9 | 3707 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 3708 | `				zEnd = &zBlob[nLen];` |
|      - | 3709 | `			}` |
|      5 | 3710 | `		}else{` |
|     11 | 3711 | `			if( nStart >= nLen ){` |
|      - | 3712 | `				/* Invalid offset */` |
|      5 | 3713 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3714 | `				return PH7_OK;` |
|    ! 0 | 3715 | `			}else{` |
|      7 | 3716 | `				zBlob += nStart;` |
|      7 | 3717 | `				nLen -= nStart;` |
|      - | 3718 | `			}` |
|      - | 3719 | `		}` |
|      7 | 3720 | `	}` |
|     35 | 3721 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3722 | `		/* Perform the lookup */` |
|    121 | 3723 | `		for(;;){` |
|    243 | 3724 | `			if( zBlob >= zPtr ){` |
|     21 | 3725 | `				break;` |
|      - | 3726 | `			}` |
|    223 | 3727 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    223 | 3728 | `			if( rc == SXRET_OK ){` |
|      - | 3729 | `				/* Pattern found,return it's position */` |
|     13 | 3730 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 3731 | `				return PH7_OK;` |
|      - | 3732 | `			}` |
|    211 | 3733 | `			zPtr--;` |
|      1 | 3734 | `		}` |
|      - | 3735 | `		/* Pattern not found,return FALSE */` |
|     21 | 3736 | `		ph7_result_bool(pCtx,0);` |
|     11 | 3737 | `	}else{` |
|      3 | 3738 | `		ph7_result_bool(pCtx,0);` |
|      - | 3739 | `	}` |
|     23 | 3740 | `	return PH7_OK;` |
|     21 | 3741 | `}` |
|      - | 3742 | `/*` |
|      - | 3743 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3744 | ` *  Case-insensitive strrpos.` |
|      - | 3745 | ` * Parameters` |
|      - | 3746 | ` *  $haystack` |
|      - | 3747 | ` *   The input string.` |
|      - | 3748 | ` * $needle` |
|      - | 3749 | ` *   Search pattern (must be a string).` |
|      - | 3750 | ` * $offset` |
|      - | 3751 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3752 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3753 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3754 | ` * Return` |
|      - | 3755 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3756 | ` */` |
|     26 | 3757 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3758 | `{` |
|      - | 3759 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     27 | 3760 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3761 | `	int nLen,nPatLen;` |
|      - | 3762 | `	sxu32 nOfft;` |
|      - | 3763 | `	sxi32 rc;` |
|     27 | 3764 | `	if( nArg < 2 ){` |
|      - | 3765 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3766 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3767 | `		return PH7_OK;` |
|      - | 3768 | `	}` |
|      - | 3769 | `	/* Extract the needle and the haystack */` |
|     27 | 3770 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 3771 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3772 | `	/* Point to the end of the pattern */` |
|     27 | 3773 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 3774 | `	zEnd = &zBlob[nLen];` |
|      - | 3775 | `	/* Save the starting posistion */` |
|     27 | 3776 | `	zStart = zBlob;` |
|     27 | 3777 | `	nOfft = 0; /* cc warning */` |
|      - | 3778 | `	/* Peek the starting offset if available */` |
|     27 | 3779 | `	if( nArg > 2 ){` |
|      - | 3780 | `		int nStart;` |
|     15 | 3781 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3782 | `		if( nStart < 0 ){` |
|      7 | 3783 | `			nStart = -nStart;` |
|      7 | 3784 | `			if( nStart >= nLen ){` |
|      - | 3785 | `				/* Invalid offset */` |
|      3 | 3786 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3787 | `				return PH7_OK;` |
|    ! 0 | 3788 | `			}else{` |
|      5 | 3789 | `				nLen -= nStart;` |
|      5 | 3790 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 3791 | `				zEnd = &zBlob[nLen];` |
|      - | 3792 | `			}` |
|      3 | 3793 | `		}else{` |
|      9 | 3794 | `			if( nStart >= nLen ){` |
|      - | 3795 | `				/* Invalid offset */` |
|      5 | 3796 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3797 | `				return PH7_OK;` |
|    ! 0 | 3798 | `			}else{` |
|      5 | 3799 | `				zBlob += nStart;` |
|      5 | 3800 | `				nLen -= nStart;` |
|      - | 3801 | `			}` |
|      - | 3802 | `		}` |
|      4 | 3803 | `	}` |
|     21 | 3804 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3805 | `		/* Perform the lookup */` |
|     44 | 3806 | `		for(;;){` |
|     89 | 3807 | `			if( zBlob >= zPtr ){` |
|      9 | 3808 | `				break;` |
|      - | 3809 | `			}` |
|     81 | 3810 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 3811 | `			if( rc == SXRET_OK ){` |
|      - | 3812 | `				/* Pattern found,return it's position */` |
|     11 | 3813 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 3814 | `				return PH7_OK;` |
|      - | 3815 | `			}` |
|     71 | 3816 | `			zPtr--;` |
|      1 | 3817 | `		}` |
|      - | 3818 | `		/* Pattern not found,return FALSE */` |
|      9 | 3819 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3820 | `	}else{` |
|      3 | 3821 | `		ph7_result_bool(pCtx,0);` |
|      - | 3822 | `	}` |
|     11 | 3823 | `	return PH7_OK;` |
|     14 | 3824 | `}` |
|      - | 3825 | `/*` |
|      - | 3826 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3827 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3828 | ` * Parameters` |
|      - | 3829 | ` *  $haystack` |
|      - | 3830 | ` *   The input string.` |
|      - | 3831 | ` * $needle` |
|      - | 3832 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3833 | ` *  This behavior is different from that of strstr().` |
|      - | 3834 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3835 | ` *  as the ordinal value of a character.` |
|      - | 3836 | ` * Return` |
|      - | 3837 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3838 | ` */` |
|     22 | 3839 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3840 | `{` |
|      - | 3841 | `	const char *zBlob;` |
|      - | 3842 | `	int nLen,c;` |
|     23 | 3843 | `	if( nArg < 2 ){` |
|      - | 3844 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3845 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3846 | `		return PH7_OK;` |
|      - | 3847 | `	}` |
|      - | 3848 | `	/* Extract the haystack */` |
|     23 | 3849 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3850 | `	c = 0; /* cc warning */` |
|     23 | 3851 | `	if( nLen > 0 ){` |
|      - | 3852 | `		sxu32 nOfft;` |
|      - | 3853 | `		sxi32 rc;` |
|     21 | 3854 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 3855 | `			const char *zPattern;` |
|     11 | 3856 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 3857 | `														 * for NULL pointer.` |
|      - | 3858 | `														 */` |
|     11 | 3859 | `			c = zPattern[0];` |
|      6 | 3860 | `		}else{` |
|      - | 3861 | `			/* Int cast */` |
|     11 | 3862 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 3863 | `		}` |
|      - | 3864 | `		/* Perform the lookup */` |
|     21 | 3865 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3866 | `		if( rc != SXRET_OK ){` |
|      - | 3867 | `			/* No such entry,return FALSE */` |
|      7 | 3868 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3869 | `			return PH7_OK;` |
|      - | 3870 | `		}` |
|      - | 3871 | `		/* Return the string portion */` |
|     15 | 3872 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 3873 | `	}else{` |
|      3 | 3874 | `		ph7_result_bool(pCtx,0);` |
|      - | 3875 | `	}` |
|     17 | 3876 | `	return PH7_OK;` |
|     12 | 3877 | `}` |
|      - | 3878 | `/*` |
|      - | 3879 | ` * string strrev(string $string)` |
|      - | 3880 | ` *  Reverse a string.` |
|      - | 3881 | ` * Parameters` |
|      - | 3882 | ` *  $string` |
|      - | 3883 | ` *   String to be reversed.` |
|      - | 3884 | ` * Return` |
|      - | 3885 | ` *  The reversed string.` |
|      - | 3886 | ` */` |
|      2 | 3887 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3888 | `{` |
|      - | 3889 | `	const char *zIn,*zEnd;` |
|      - | 3890 | `	int nLen,c;` |
|      3 | 3891 | `	if( nArg < 1 ){` |
|      - | 3892 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3893 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3894 | `		return PH7_OK;` |
|      - | 3895 | `	}` |
|      - | 3896 | `	/* Extract the target string */` |
|      3 | 3897 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3898 | `	if( nLen < 1 ){` |
|      - | 3899 | `		/* Empty string Return null */` |
|    ! 0 | 3900 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3901 | `		return PH7_OK;` |
|      - | 3902 | `	}` |
|      - | 3903 | `	/* Perform the requested operation */` |
|      3 | 3904 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3905 | `	for(;;){` |
|      9 | 3906 | `		if( zEnd < zIn ){` |
|      - | 3907 | `			/* No more input to process */` |
|      3 | 3908 | `			break;` |
|      - | 3909 | `		}` |
|      - | 3910 | `		/* Append current character */` |
|      7 | 3911 | `		c = zEnd[0];` |
|      7 | 3912 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3913 | `		zEnd--;` |
|      1 | 3914 | `	}` |
|      3 | 3915 | `	return PH7_OK;` |
|      2 | 3916 | `}` |
|      - | 3917 | `/*` |
|      - | 3918 | ` * string ucwords(string $string [, string $separators = " \t\r\n\f\v"])` |
|      - | 3919 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3920 | ` *  A word begins at the start of the string and after any character present in` |
|      - | 3921 | ` *  $separators. The default separators are the whitespace characters (space,` |
|      - | 3922 | ` *  horizontal tab, carriage return, newline, form-feed and vertical tab); an` |
|      - | 3923 | ` *  explicit $separators argument REPLACES them (an empty string leaves only the` |
|      - | 3924 | ` *  very first character upper-cased). Like PHP, this is byte-based: only ASCII` |
|      - | 3925 | ` *  bytes are upper-cased and a byte is a separator only if it appears in the set.` |
|      - | 3926 | ` * Parameters` |
|      - | 3927 | ` *  $string` |
|      - | 3928 | ` *   The input string.` |
|      - | 3929 | ` *  $separators` |
|      - | 3930 | ` *   The optional word-boundary characters.` |
|      - | 3931 | ` * Return` |
|      - | 3932 | ` *  The modified string.` |
|      - | 3933 | ` */` |
|     22 | 3934 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3935 | `{` |
|      - | 3936 | `	const char *zIn;` |
|      - | 3937 | `	int nLen,i,iStart;` |
|      - | 3938 | `	char aDelim[256];` |
|     23 | 3939 | `	if( nArg < 1 ){` |
|      - | 3940 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3941 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3942 | `		return PH7_OK;` |
|      - | 3943 | `	}` |
|      - | 3944 | `	/* Build the separator membership table: an explicit $separators argument` |
|      - | 3945 | `	 * replaces the default whitespace set (an empty string clears it). */` |
|     23 | 3946 | `	SyZero(aDelim,(sxu32)sizeof(aDelim));` |
|     23 | 3947 | `	if( nArg > 1 ){` |
|      - | 3948 | `		int nDelim;` |
|      9 | 3949 | `		const char *zDelim = ph7_value_to_string(apArg[1],&nDelim);` |
|     17 | 3950 | `		for( i = 0 ; i < nDelim ; i++ ){` |
|      9 | 3951 | `			aDelim[(unsigned char)zDelim[i]] = 1;` |
|      5 | 3952 | `		}` |
|      5 | 3953 | `	}else{` |
|     15 | 3954 | `		aDelim[(unsigned char)' ']  = 1;` |
|     15 | 3955 | `		aDelim[(unsigned char)'\t'] = 1;` |
|     15 | 3956 | `		aDelim[(unsigned char)'\r'] = 1;` |
|     15 | 3957 | `		aDelim[(unsigned char)'\n'] = 1;` |
|     15 | 3958 | `		aDelim[(unsigned char)'\f'] = 1;` |
|     15 | 3959 | `		aDelim[(unsigned char)'\v'] = 1;` |
|      - | 3960 | `	}` |
|      - | 3961 | `	/* Extract the target string */` |
|     23 | 3962 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3963 | `	if( nLen < 1 ){` |
|      - | 3964 | `		/* Empty string – match PHP semantics */` |
|      3 | 3965 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3966 | `		return PH7_OK;` |
|      - | 3967 | `	}` |
|      - | 3968 | `	/* Upper-case the first byte of each word (the leading byte, or any byte that` |
|      - | 3969 | `	 * follows a separator), appending the untouched runs in between verbatim. */` |
|     21 | 3970 | `	iStart = 0;` |
|    309 | 3971 | `	for( i = 0 ; i < nLen ; i++ ){` |
|    289 | 3972 | `		int c = (unsigned char)zIn[i];` |
|    289 | 3973 | `		if( (i == 0 \|\| aDelim[(unsigned char)zIn[i-1]]) && c < 0x80 && SyisLower(c) ){` |
|     53 | 3974 | `			char up = (char)SyToUpper(c);` |
|     53 | 3975 | `			if( i > iStart ){` |
|     35 | 3976 | `				ph7_result_string(pCtx,&zIn[iStart],i - iStart);` |
|     17 | 3977 | `			}` |
|     53 | 3978 | `			ph7_result_string(pCtx,&up,1);` |
|     53 | 3979 | `			iStart = i + 1;` |
|     26 | 3980 | `		}` |
|    145 | 3981 | `	}` |
|     21 | 3982 | `	if( nLen > iStart ){` |
|     21 | 3983 | `		ph7_result_string(pCtx,&zIn[iStart],nLen - iStart);` |
|     10 | 3984 | `	}` |
|     21 | 3985 | `	return PH7_OK;` |
|     12 | 3986 | `}` |
|      - | 3987 | `/*` |
|      - | 3988 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3989 | ` *  Returns input repeated multiplier times.` |
|      - | 3990 | ` * Parameters` |
|      - | 3991 | ` *  $string` |
|      - | 3992 | ` *   String to be repeated.` |
|      - | 3993 | ` * $multiplier` |
|      - | 3994 | ` *  Number of time the input string should be repeated.` |
|      - | 3995 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3996 | ` *  to 0, the function will return an empty string.` |
|      - | 3997 | ` * Return` |
|      - | 3998 | ` *  The repeated string.` |
|      - | 3999 | ` */` |
|  20434 | 4000 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4001 | `{` |
|      - | 4002 | `	const char *zIn;` |
|      - | 4003 | `	int nLen;` |
|      - | 4004 | `	ph7_int64 nMul;` |
|      - | 4005 | `	int rc;` |
|  20436 | 4006 | `	if( nArg < 2 ){` |
|      - | 4007 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4008 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4009 | `		return PH7_OK;` |
|      - | 4010 | `	}` |
|      - | 4011 | `	/* Extract the target string */` |
|  20436 | 4012 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4013 | `	/* Resolve $times through the shared ZPP helper so a lossy float / float-string` |
|      - | 4014 | `	 * carries php's precision deprecation and NAN/INF/non-numeric fail with php's` |
|      - | 4015 | `	 * TypeError — a bare ph7_value_to_int64() coerced them silently. */` |
|      - | 4016 | `	{` |
|  20436 | 4017 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_repeat",2,"$times","int",&nMul);` |
|  20436 | 4018 | `		if( rcArg != PH7_OK ){` |
|    ! 0 | 4019 | `			return rcArg;` |
|      - | 4020 | `		}` |
|      - | 4021 | `	}` |
|  20436 | 4022 | `	if( nMul < 0 ){` |
|      3 | 4023 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4024 | `			"str_repeat(): Argument #2 ($times) must be greater than or equal to 0");` |
|      - | 4025 | `	}` |
|  20434 | 4026 | `	if( nLen < 1 \|\| nMul < 1 ){` |
|      - | 4027 | `		/* Empty input or a zero multiplier yields the empty string (PHP). */` |
|    ! 0 | 4028 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4029 | `		return PH7_OK;` |
|      - | 4030 | `	}` |
|      - | 4031 | `	/* Perform the requested operation */` |
| 221930 | 4032 | `	for(;;){` |
| 443862 | 4033 | `		if( !nMul ){` |
|  20434 | 4034 | `			break;` |
|      - | 4035 | `		}` |
|      - | 4036 | `		/* Append the copy */` |
| 423430 | 4037 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 423430 | 4038 | `		if( rc != PH7_OK ){` |
|      - | 4039 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 4040 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 4041 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 4042 | `		}` |
| 423430 | 4043 | `		nMul--;` |
|      2 | 4044 | `	}` |
|  20434 | 4045 | `	return PH7_OK;` |
|  10219 | 4046 | `}` |
|      - | 4047 | `/*` |
|      - | 4048 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 4049 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 4050 | ` * Parameters` |
|      - | 4051 | ` *  $string` |
|      - | 4052 | ` *   The input string.` |
|      - | 4053 | ` * $is_xhtml` |
|      - | 4054 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 4055 | ` * Return` |
|      - | 4056 | ` *  The processed string.` |
|      - | 4057 | ` */` |
|      4 | 4058 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4059 | `{` |
|      - | 4060 | `	const char *zIn,*zCur,*zEnd;` |
|      5 | 4061 | `	int is_xhtml = 1; /* Default to XHTML-style '<br />' like PHP */` |
|      - | 4062 | `	int nLen;` |
|      5 | 4063 | `	if( nArg < 1 ){` |
|      - | 4064 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4065 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4066 | `		return PH7_OK;` |
|      - | 4067 | `	}` |
|      - | 4068 | `	/* Extract the target string */` |
|      5 | 4069 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 4070 | `	if( nLen < 1 ){` |
|      - | 4071 | `		/* Empty string,return null */` |
|    ! 0 | 4072 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4073 | `		return PH7_OK;` |
|      - | 4074 | `	}` |
|      5 | 4075 | `	if( nArg > 1 ){` |
|      3 | 4076 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 4077 | `	}` |
|      5 | 4078 | `	zEnd = &zIn[nLen];` |
|      - | 4079 | `	/* Perform the requested operation */` |
|      4 | 4080 | `	for(;;){` |
|      9 | 4081 | `		zCur = zIn;` |
|      - | 4082 | `		/* Delimit the string */` |
|     21 | 4083 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 4084 | `			zIn++;` |
|      1 | 4085 | `		}` |
|      9 | 4086 | `		if( zCur < zIn ){` |
|      - | 4087 | `			/* Output chunk verbatim */` |
|      9 | 4088 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 4089 | `		}` |
|      9 | 4090 | `		if( zIn >= zEnd ){` |
|      - | 4091 | `			/* No more input to process */` |
|      5 | 4092 | `			break;` |
|      - | 4093 | `		}` |
|      - | 4094 | `		/* Output the HTML line break */` |
|      - | 4095 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br />' (space before the slash), otherwise use '<br>' */` |
|      5 | 4096 | `		if( is_xhtml ){` |
|      3 | 4097 | `			ph7_result_string(pCtx,"<br />",(int)sizeof("<br />")-1);` |
|      2 | 4098 | `		}else{` |
|      3 | 4099 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 4100 | `		}` |
|      5 | 4101 | `		zCur = zIn;` |
|      - | 4102 | `		/* Append trailing line */` |
|     11 | 4103 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 4104 | `			zIn++;` |
|      1 | 4105 | `		}` |
|      5 | 4106 | `		if( zCur < zIn ){` |
|      - | 4107 | `			/* Output chunk verbatim */` |
|      5 | 4108 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 4109 | `		}` |
|      1 | 4110 | `	}` |
|      5 | 4111 | `	return PH7_OK;` |
|      3 | 4112 | `}` |
|      - | 4113 | `/*` |
|      - | 4114 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 4115 | ` *  According to the PHP reference manual.` |
|      - | 4116 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 4117 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 4118 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 4119 | ` * This applies to both sprintf() and printf().` |
|      - | 4120 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 4121 | ` * or more of these elements, in order:` |
|      - | 4122 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 4123 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 4124 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 4125 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 4126 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 4127 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 4128 | ` *   it with a single quote ('). See the examples below.` |
|      - | 4129 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 4130 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 4131 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 4132 | ` *   should result in.` |
|      - | 4133 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 4134 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 4135 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 4136 | ` *   limit to the string.` |
|      - | 4137 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 4138 | ` *       % - a literal percent character. No argument is required.` |
|      - | 4139 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 4140 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 4141 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 4142 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 4143 | ` * 	     for the number of digits after the decimal point.` |
|      - | 4144 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 4145 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 4146 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 4147 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 4148 | ` *       g - shorter of %e and %f.` |
|      - | 4149 | ` *       G - shorter of %E and %f.` |
|      - | 4150 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 4151 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 4152 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 4153 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 4154 | ` */` |
|      - | 4155 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4156 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 4157 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 4158 | `/*` |
|      - | 4159 | `** Conversion types fall into various categories as defined by the` |
|      - | 4160 | `** following enumeration.` |
|      - | 4161 | `*/` |
|      - | 4162 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 4163 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 4164 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 4165 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 4166 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 4167 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 4168 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 4169 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 4170 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 4171 |  |
|      - | 4172 | `/*` |
|      - | 4173 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 4174 | `*/` |
|      - | 4175 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 4176 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 4177 | `/*` |
|      - | 4178 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 4179 | `** by an instance of the following structure` |
|      - | 4180 | `*/` |
|      - | 4181 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 4182 | `struct ph7_fmt_info` |
|      - | 4183 | `{` |
|      - | 4184 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 4185 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 4186 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 4187 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 4188 | `  char *charset; /* The character set for conversion */` |
|      - | 4189 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 4190 | `};` |
|      - | 4191 | `/* PH7_PhpFloatShape (php's float-shape post-processing) lives in memobj.c —` |
|      - | 4192 | ` * the default float->string cast needs it even when this whole formatting` |
|      - | 4193 | ` * region is compiled out by PH7_DISABLE_DISK_IO. */` |
|      - | 4194 | `/*` |
|      - | 4195 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 4196 | ` * used conversion types first.` |
|      - | 4197 | ` */` |
|      - | 4198 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 4199 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 4200 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 4201 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 4202 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 4203 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 4204 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 4205 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 4206 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 4207 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4208 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4209 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 4210 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 4211 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 4212 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 4213 | `  /* php's 'h'/'H' are the locale-independent twins of 'g'/'G'; PHL always` |
|      - | 4214 | `   * formats in the C locale, so they behave identically. */` |
|      - | 4215 | `  {  'h',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 4216 | `  {  'H',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 4217 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 4218 | `};` |
|      - | 4219 | `/*` |
|      - | 4220 | ` * PHP 8 raises a catchable ValueError for an unknown conversion specifier` |
|      - | 4221 | ` * (e.g. "%y", or the C-ism "%#x" — '#' is not a php flag). Because printf()` |
|      - | 4222 | ` * and fprintf() stream their output incrementally while sprintf() buffers it,` |
|      - | 4223 | ` * every format builtin calls PH7_FormatValidate (below) to check the whole` |
|      - | 4224 | ` * format string BEFORE formatting so the throw happens with no partial output` |
|      - | 4225 | ` * escaping (php buffers the entire result and only emits it on success). This` |
|      - | 4226 | ` * scan mirrors the specifier-locating logic of the main format loop below.` |
|      - | 4227 | ` * On the first unknown specifier, stores it in *pBad and returns TRUE; returns` |
|      - | 4228 | ` * FALSE when every specifier is known. (A found-flag rather than a sentinel` |
|      - | 4229 | ` * char, so a NUL specifier byte — "%\0" — is still reported, not mistaken for` |
|      - | 4230 | ` * "all valid".)` |
|      - | 4231 | ` */` |
|    412 | 4232 | `static int FormatUnknownSpec(const char *zIn,int nByte,int *pBad)` |
|      1 | 4233 | `{` |
|    413 | 4234 | `	const char *zEnd = &zIn[nByte];` |
|      - | 4235 | `	int c,idx;` |
|   3449 | 4236 | `	while( zIn < zEnd ){` |
|   3057 | 4237 | `		if( zIn[0] != '%' ){` |
|   2265 | 4238 | `			zIn++;` |
|   2265 | 4239 | `			continue;` |
|      - | 4240 | `		}` |
|    793 | 4241 | `		zIn++; /* jump the percent sign */` |
|      - | 4242 | `		/* php-supported flags: '-', '+', ' ', '0' and the "'<pad>'" custom-pad` |
|      - | 4243 | `		 * form. '#' is intentionally NOT treated as a flag so it surfaces as an` |
|      - | 4244 | `		 * unknown specifier, matching php. */` |
|    977 | 4245 | `		while( zIn < zEnd ){` |
|    975 | 4246 | `			c = zIn[0];` |
|    975 | 4247 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|    185 | 4248 | `				zIn++;` |
|    185 | 4249 | `				continue;` |
|      - | 4250 | `			}` |
|    791 | 4251 | `			if( c=='\'' ){` |
|    ! 0 | 4252 | `				zIn++;` |
|    ! 0 | 4253 | `				if( zIn < zEnd ){` |
|    ! 0 | 4254 | `					zIn++; /* the custom pad character */` |
|    ! 0 | 4255 | `				}` |
|    ! 0 | 4256 | `				continue;` |
|      - | 4257 | `			}` |
|    791 | 4258 | `			break;` |
|    ! 0 | 4259 | `		}` |
|      - | 4260 | `		/* field width */` |
|   1009 | 4261 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    217 | 4262 | `			zIn++;` |
|      1 | 4263 | `		}` |
|      - | 4264 | `		/* positional specifier ($) — php parses flags AFTER it (e.g. "%1$-10s"),` |
|      - | 4265 | `		 * so skip the full flag set and width again, mirroring the main loop. */` |
|    793 | 4266 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
|    ! 0 | 4267 | `			zIn++;` |
|    ! 0 | 4268 | `			while( zIn < zEnd ){` |
|    ! 0 | 4269 | `				c = zIn[0];` |
|    ! 0 | 4270 | `				if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|    ! 0 | 4271 | `					zIn++;` |
|    ! 0 | 4272 | `					continue;` |
|      - | 4273 | `				}` |
|    ! 0 | 4274 | `				if( c=='\'' ){` |
|    ! 0 | 4275 | `					zIn++;` |
|    ! 0 | 4276 | `					if( zIn < zEnd ){` |
|    ! 0 | 4277 | `						zIn++;` |
|    ! 0 | 4278 | `					}` |
|    ! 0 | 4279 | `					continue;` |
|      - | 4280 | `				}` |
|    ! 0 | 4281 | `				break;` |
|    ! 0 | 4282 | `			}` |
|    ! 0 | 4283 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    ! 0 | 4284 | `				zIn++;` |
|    ! 0 | 4285 | `			}` |
|    ! 0 | 4286 | `		}` |
|      - | 4287 | `		/* precision */` |
|    793 | 4288 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|     87 | 4289 | `			zIn++;` |
|    183 | 4290 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|     97 | 4291 | `				zIn++;` |
|      1 | 4292 | `			}` |
|     43 | 4293 | `		}` |
|      - | 4294 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|    793 | 4295 | `		if( zIn < zEnd && zIn[0]=='l' ){` |
|     11 | 4296 | `			zIn++;` |
|      5 | 4297 | `		}` |
|    793 | 4298 | `		if( zIn >= zEnd ){` |
|      - | 4299 | `			/* A dangling '%' with no specifier: PHL's legacy path silently` |
|      - | 4300 | `			 * truncates here (recorded residual); nothing to validate. */` |
|      3 | 4301 | `			break;` |
|      - | 4302 | `		}` |
|    791 | 4303 | `		c = zIn[0];` |
|    791 | 4304 | `		zIn++; /* jump the conversion specifier */` |
|   3333 | 4305 | `		for( idx = 0 ; idx < (int)SX_ARRAYSIZE(aFmt) ; idx++ ){` |
|   3315 | 4306 | `			if( c == aFmt[idx].fmttype ){` |
|    773 | 4307 | `				break;` |
|      - | 4308 | `			}` |
|   1272 | 4309 | `		}` |
|    791 | 4310 | `		if( idx >= (int)SX_ARRAYSIZE(aFmt) ){` |
|     19 | 4311 | `			*pBad = c; /* unknown specifier */` |
|     19 | 4312 | `			return TRUE;` |
|      - | 4313 | `		}` |
|      1 | 4314 | `	}` |
|    395 | 4315 | `	return FALSE;` |
|    207 | 4316 | `}` |
|      - | 4317 | `/*` |
|      - | 4318 | ` * Validate a printf-style format string. PHP 8 raises a catchable ValueError for` |
|      - | 4319 | ` * an unknown conversion specifier, thrown before any output is produced. Every` |
|      - | 4320 | ` * format builtin (sprintf/printf/vprintf/vsprintf/fprintf/vfprintf) calls this` |
|      - | 4321 | ` * up-front, then propagates the returned status verbatim (PH7_EXCEPTION when the` |
|      - | 4322 | ` * throw is caught in place, PH7_ABORT when it goes uncaught).` |
|      - | 4323 | ` * Returns PH7_OK when the format is valid.` |
|      - | 4324 | ` */` |
|    412 | 4325 | `PH7_PRIVATE sxi32 PH7_FormatValidate(ph7_context *pCtx,const char *zFormat,int nByte)` |
|      1 | 4326 | `{` |
|    413 | 4327 | `	int badSpec = 0;` |
|    413 | 4328 | `	if( FormatUnknownSpec(zFormat,nByte,&badSpec) ){` |
|     28 | 4329 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      9 | 4330 | `			"Unknown format specifier \"%c\"",badSpec);` |
|      - | 4331 | `	}` |
|    395 | 4332 | `	return PH7_OK;` |
|    207 | 4333 | `}` |
|      - | 4334 | `/*` |
|      - | 4335 | `` * PHP 8: a printf-family `$format` argument is a `string` parameter — scalars`` |
|      - | 4336 | ` * (int/float/bool) and null coerce to a string, but an array/object/resource` |
|      - | 4337 | ` * raises a catchable TypeError. iArg is the 1-based argument position ($format` |
|      - | 4338 | ` * is #1 for sprintf/printf/vprintf/vsprintf, #2 for fprintf/vfprintf). Returns` |
|      - | 4339 | ` * PH7_OK when the value is string-coercible (the caller then uses` |
|      - | 4340 | ` * ph7_value_to_string, which renders scalars/null verbatim).` |
|      - | 4341 | ` */` |
|    424 | 4342 | `PH7_PRIVATE sxi32 PH7_FormatCheckFormatArg(ph7_context *pCtx,ph7_value *pArg,int iArg)` |
|      1 | 4343 | `{` |
|    425 | 4344 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg) \|\| ph7_value_is_resource(pArg) ){` |
|      - | 4345 | `		char zBuf[64];` |
|    ! 0 | 4346 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4347 | `			"%s(): Argument #%d ($format) must be of type string, %s given",` |
|    ! 0 | 4348 | `			ph7_function_name(pCtx),iArg,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|      - | 4349 | `	}` |
|    425 | 4350 | `	return PH7_OK;` |
|    213 | 4351 | `}` |
|      - | 4352 | `/*` |
|      - | 4353 | ` * Format a given string.` |
|      - | 4354 | ` * The root program.  All variations call this core.` |
|      - | 4355 | ` * INPUTS:` |
|      - | 4356 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 4357 | ` *            1. A pointer to the call context.` |
|      - | 4358 | ` *            2. A pointer to the list of characters to be output` |
|      - | 4359 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 4360 | ` *            3. An integer number of characters to be output.` |
|      - | 4361 | ` *               (Note: This number might be zero.)` |
|      - | 4362 | ` *            4. Upper layer private data.` |
|      - | 4363 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 4364 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 4365 | ` */` |
|    394 | 4366 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 4367 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 4368 | `	ph7_context *pCtx,  /* call context */` |
|      - | 4369 | `	const char *zIn,    /* Format string */` |
|      - | 4370 | `	int nByte,          /* Format string length */` |
|      - | 4371 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 4372 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 4373 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 4374 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 4375 | `	)` |
|      1 | 4376 | `{` |
|    395 | 4377 | `	char spaces[] = "                                                  ";` |
|      - | 4378 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    395 | 4379 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 4380 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 4381 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 4382 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 4383 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 4384 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 4385 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 4386 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 4387 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 4388 | `	ph7_int64 iVal;` |
|      - | 4389 | `	int precision;           /* Precision of the current field */` |
|      - | 4390 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 4391 | `	int c,rc,n;` |
|      - | 4392 | `	int length;              /* Length of the field */` |
|      - | 4393 | `	int prefix;` |
|      - | 4394 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 4395 | `	int width;               /* Width of the current field */` |
|      - | 4396 | `	int idx;` |
|    395 | 4397 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 4398 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 4399 | `	/* An unknown conversion specifier is rejected up-front by PH7_FormatValidate()` |
|      - | 4400 | `	 * (called by every format builtin before this routine), so the specifier set` |
|      - | 4401 | `	 * seen here is always valid. */` |
|      - | 4402 | `	/* Start the format process */` |
|    583 | 4403 | `	for(;;){` |
|   1167 | 4404 | `		zCur = zIn;` |
|   3417 | 4405 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|   2251 | 4406 | `			zIn++;` |
|      1 | 4407 | `		}` |
|   1167 | 4408 | `		if( zCur < zIn ){` |
|      - | 4409 | `			/* Consume chunk verbatim */` |
|    725 | 4410 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|    725 | 4411 | `			if( rc != SXRET_OK ){` |
|      - | 4412 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|    ! 0 | 4413 | `				break;` |
|      - | 4414 | `			}` |
|    362 | 4415 | `		}` |
|   1167 | 4416 | `		if( zIn >= zEnd ){` |
|      - | 4417 | `			/* No more input to process,break immediately */` |
|    393 | 4418 | `			break;` |
|      - | 4419 | `		}` |
|      - | 4420 | `		/* Find out what flags are present */` |
|    775 | 4421 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    774 | 4422 | `			flag_alternateform = flag_zeropad = 0;` |
|    775 | 4423 | `		zIn++; /* Jump the precent sign */` |
|    387 | 4424 | `		do{` |
|    959 | 4425 | `			c = zIn[0];` |
|    959 | 4426 | `			switch( c ){` |
|     15 | 4427 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      7 | 4428 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 4429 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|    159 | 4430 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 4431 | `			case '\'':` |
|    ! 0 | 4432 | `				zIn++;` |
|    ! 0 | 4433 | `				if( zIn < zEnd ){` |
|      - | 4434 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 4435 | `					c = zIn[0];` |
|    ! 0 | 4436 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4437 | `						spaces[idx] = (char)c;` |
|    ! 0 | 4438 | `					}` |
|    ! 0 | 4439 | `					c = 0;` |
|    ! 0 | 4440 | `				}` |
|    ! 0 | 4441 | `				break;` |
|    774 | 4442 | `			default:                                       break;` |
|      - | 4443 | `			}` |
|    959 | 4444 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 4445 | `		/* Get the field width */` |
|    775 | 4446 | `		width = 0;` |
|   1378 | 4447 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    217 | 4448 | `			width = width*10 + (zIn[0] - '0');` |
|    217 | 4449 | `			zIn++;` |
|      1 | 4450 | `		}` |
|    775 | 4451 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 4452 | `			/* Position specifer */` |
|    ! 0 | 4453 | `			if( width > 0 ){` |
|    ! 0 | 4454 | `				n = width;` |
|    ! 0 | 4455 | `				if( vf && n > 0 ){` |
|    ! 0 | 4456 | `					n--;` |
|    ! 0 | 4457 | `				}` |
|    ! 0 | 4458 | `			}` |
|    ! 0 | 4459 | `			zIn++;` |
|    ! 0 | 4460 | `			width = 0;` |
|      - | 4461 | `			/* php's grammar is %argnum$<flags><width>: the flags come AFTER the` |
|      - | 4462 | `			 * positional, so re-parse the full flag set here (e.g. "%1$-10s"),` |
|      - | 4463 | `			 * not just zero-padding. */` |
|    ! 0 | 4464 | `			do{` |
|    ! 0 | 4465 | `				c = zIn[0];` |
|    ! 0 | 4466 | `				switch( c ){` |
|    ! 0 | 4467 | `				case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|    ! 0 | 4468 | `				case '+':   flag_plussign = 1;        c = 0;   break;` |
|    ! 0 | 4469 | `				case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|    ! 0 | 4470 | `				case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 4471 | `				case '\'':` |
|    ! 0 | 4472 | `					zIn++;` |
|    ! 0 | 4473 | `					if( zIn < zEnd ){` |
|    ! 0 | 4474 | `						c = zIn[0];` |
|    ! 0 | 4475 | `						for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4476 | `							spaces[idx] = (char)c;` |
|    ! 0 | 4477 | `						}` |
|    ! 0 | 4478 | `						c = 0;` |
|    ! 0 | 4479 | `					}` |
|    ! 0 | 4480 | `					break;` |
|    ! 0 | 4481 | `				default:                                       break;` |
|      - | 4482 | `				}` |
|    ! 0 | 4483 | `			}while( c==0 && (zIn++ < zEnd) );` |
|    ! 0 | 4484 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 4485 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 4486 | `				zIn++;` |
|    ! 0 | 4487 | `			}` |
|    ! 0 | 4488 | `		}` |
|    775 | 4489 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 4490 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 4491 | `		}` |
|      - | 4492 | `		/* Get the precision */` |
|    775 | 4493 | `		precision = -1;` |
|    775 | 4494 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     87 | 4495 | `			precision = 0;` |
|     87 | 4496 | `			zIn++;` |
|    226 | 4497 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     97 | 4498 | `				precision = precision*10 + (zIn[0] - '0');` |
|     97 | 4499 | `				zIn++;` |
|      1 | 4500 | `			}` |
|     43 | 4501 | `		}` |
|      - | 4502 | `		/* Consume a single 'l' length modifier (a C-ism php accepts and ignores,` |
|      - | 4503 | `		 * e.g. "%ld"); PH7_FormatValidate mirrors this. Exactly one is skipped:` |
|      - | 4504 | `		 * in "%lld" the second 'l' becomes the (unknown) specifier, just like php. */` |
|    775 | 4505 | `		if( zIn < zEnd && zIn[0] == 'l' ){` |
|      9 | 4506 | `			zIn++;` |
|      4 | 4507 | `		}` |
|    775 | 4508 | `		if( zIn >= zEnd ){` |
|      - | 4509 | `			/* No more input */` |
|      3 | 4510 | `			break;` |
|      - | 4511 | `		}` |
|      - | 4512 | `		/* Fetch the info entry for the field */` |
|    773 | 4513 | `		pInfo = 0;` |
|    773 | 4514 | `		xtype = PH7_FMT_ERROR;` |
|    773 | 4515 | `		c = zIn[0];` |
|    773 | 4516 | `		zIn++; /* Jump the format specifer */` |
|   3009 | 4517 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|   3009 | 4518 | `			if( c==aFmt[idx].fmttype ){` |
|    773 | 4519 | `				pInfo = &aFmt[idx];` |
|    773 | 4520 | `				xtype = pInfo->type;` |
|    773 | 4521 | `				break;` |
|      - | 4522 | `			}` |
|   1119 | 4523 | `		}` |
|    773 | 4524 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    773 | 4525 | `		length = 0;` |
|      - | 4526 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 4527 | `		 /*` |
|      - | 4528 | `		  ** At this point, variables are initialized as follows:` |
|      - | 4529 | `		  **` |
|      - | 4530 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 4531 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 4532 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 4533 | `		  **                               field width was negative.` |
|      - | 4534 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 4535 | `		  **                               the conversion character.` |
|      - | 4536 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 4537 | `		  **   width                       The specified field width.  This is` |
|      - | 4538 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 4539 | `		  **   precision                   The specified precision.  The default` |
|      - | 4540 | `		  **                               is -1.` |
|      - | 4541 | `		  */` |
|    773 | 4542 | `		switch(xtype){` |
|      3 | 4543 | `		case PH7_FMT_PERCENT:` |
|      - | 4544 | `			/* A literal percent character */` |
|      7 | 4545 | `			zWorker[0] = '%';` |
|      7 | 4546 | `			length = (int)sizeof(char);` |
|      7 | 4547 | `			break;` |
|      3 | 4548 | `		case PH7_FMT_CHARX:` |
|      - | 4549 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 4550 | `			 * with that ASCII value` |
|      - | 4551 | `			 */` |
|      7 | 4552 | `			pArg = NEXT_ARG;` |
|      7 | 4553 | `			if( pArg == 0 ){` |
|      3 | 4554 | `				c = 0;` |
|      2 | 4555 | `			}else{` |
|      5 | 4556 | `				c = ph7_value_to_int(pArg);` |
|      - | 4557 | `			}` |
|      - | 4558 | `			/* NUL byte is an acceptable value */` |
|      7 | 4559 | `			zWorker[0] = (char)c;` |
|      7 | 4560 | `			length = (int)sizeof(char);` |
|      7 | 4561 | `			break;` |
|    162 | 4562 | `		case PH7_FMT_STRING:` |
|      - | 4563 | `			/* the argument is treated as and presented as a string */` |
|    325 | 4564 | `			pArg = NEXT_ARG;` |
|    325 | 4565 | `			if( pArg == 0 ){` |
|    ! 0 | 4566 | `				length = 0;` |
|    ! 0 | 4567 | `			}else{` |
|    325 | 4568 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 4569 | `			}` |
|    325 | 4570 | `			if( length < 1 ){` |
|    ! 0 | 4571 | `				zBuf = " ";` |
|    ! 0 | 4572 | `				length = (int)sizeof(char);` |
|    ! 0 | 4573 | `			}` |
|    325 | 4574 | `			if( precision>=0 && precision<length ){` |
|      3 | 4575 | `				length = precision;` |
|      1 | 4576 | `			}` |
|    325 | 4577 | `			if( flag_zeropad ){` |
|      - | 4578 | `				/* zero-padding works on strings too */` |
|    ! 0 | 4579 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4580 | `					spaces[idx] = '0';` |
|    ! 0 | 4581 | `				}` |
|    ! 0 | 4582 | `			}` |
|    325 | 4583 | `			break;` |
|    130 | 4584 | `		case PH7_FMT_RADIX:` |
|    261 | 4585 | `			pArg = NEXT_ARG;` |
|    261 | 4586 | `			if( pArg == 0 ){` |
|    ! 0 | 4587 | `				iVal = 0;` |
|    ! 0 | 4588 | `			}else{` |
|    261 | 4589 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 4590 | `			}` |
|      - | 4591 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|    261 | 4592 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 4593 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 4594 | `			}` |
|      - | 4595 | `#if 1` |
|      - | 4596 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 4597 | `        ** I think this is stupid.*/` |
|    261 | 4598 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 4599 | `#else` |
|      - | 4600 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 4601 | `        ** but leave the prefix for hex.*/` |
|      - | 4602 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 4603 | `#endif` |
|    261 | 4604 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|    237 | 4605 | `          if( iVal<0 ){` |
|     25 | 4606 | `            iVal = -iVal;` |
|      - | 4607 | `			/* Ticket 1433-003 */` |
|     25 | 4608 | `			if( iVal < 0 ){` |
|      - | 4609 | `				/* Overflow */` |
|    ! 0 | 4610 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4611 | `			}` |
|     25 | 4612 | `            prefix = '-';` |
|    225 | 4613 | `          }else if( flag_plussign )  prefix = '+';` |
|    211 | 4614 | `          else if( flag_blanksign )  prefix = ' ';` |
|    209 | 4615 | `          else                       prefix = 0;` |
|    119 | 4616 | `        }else{` |
|     25 | 4617 | `			if( iVal<0 ){` |
|    ! 0 | 4618 | `				iVal = -iVal;` |
|      - | 4619 | `				/* Ticket 1433-003 */` |
|    ! 0 | 4620 | `				if( iVal < 0 ){` |
|      - | 4621 | `					/* Overflow */` |
|    ! 0 | 4622 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4623 | `				}` |
|    ! 0 | 4624 | `			}` |
|     25 | 4625 | `			prefix = 0;` |
|      - | 4626 | `		}` |
|    261 | 4627 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|    149 | 4628 | `          precision = width-(prefix!=0);` |
|     74 | 4629 | `        }` |
|    261 | 4630 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 4631 | `        {` |
|      - | 4632 | `          register char *cset;      /* Use registers for speed */` |
|      - | 4633 | `          register int base;` |
|    261 | 4634 | `          cset = pInfo->charset;` |
|    261 | 4635 | `          base = pInfo->base;` |
|    130 | 4636 | `          do{                                           /* Convert to ascii */` |
|    333 | 4637 | `            *(--zBuf) = cset[iVal%base];` |
|    333 | 4638 | `            iVal = iVal/base;` |
|    333 | 4639 | `          }while( iVal>0 );` |
|      - | 4640 | `        }` |
|    261 | 4641 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    427 | 4642 | `        for(idx=precision-length; idx>0; idx--){` |
|    167 | 4643 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|     84 | 4644 | `        }` |
|    261 | 4645 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|    261 | 4646 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 4647 | `          char *pre, x;` |
|    ! 0 | 4648 | `          pre = pInfo->prefix;` |
|    ! 0 | 4649 | `          if( *zBuf!=pre[0] ){` |
|    ! 0 | 4650 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|    ! 0 | 4651 | `          }` |
|    ! 0 | 4652 | `        }` |
|    261 | 4653 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    261 | 4654 | `		break;` |
|     88 | 4655 | `		case PH7_FMT_FLOAT:` |
|      - | 4656 | `		case PH7_FMT_EXP:` |
|      - | 4657 | `		case PH7_FMT_GENERIC:{` |
|      - | 4658 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 4659 | `		double realvalue;` |
|      - | 4660 | `		char zFmt[8];` |
|      - | 4661 | `		int nOut, nFmt;` |
|    177 | 4662 | `		pArg = NEXT_ARG;` |
|    177 | 4663 | `		if( pArg == 0 ){` |
|    ! 0 | 4664 | `			realvalue = 0;` |
|    ! 0 | 4665 | `		}else{` |
|    177 | 4666 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 4667 | `		}` |
|      - | 4668 | `		/* php prints the IEEE specials bare — NaN / INF / -INF with no width` |
|      - | 4669 | `		 * padding, precision, or sign flags (php_sprintf_appenddouble). */` |
|    177 | 4670 | `		if( PH7_IS_NAN(realvalue) ){` |
|     21 | 4671 | `			zBuf = "NaN";` |
|     21 | 4672 | `			length = 3;` |
|     21 | 4673 | `			width = 0;` |
|     21 | 4674 | `			break;` |
|      - | 4675 | `		}` |
|    157 | 4676 | `		if( PH7_IS_INF(realvalue) ){` |
|     37 | 4677 | `			if( realvalue < 0.0 ){` |
|     15 | 4678 | `				zBuf = "-INF";` |
|     15 | 4679 | `				length = 4;` |
|      8 | 4680 | `			}else{` |
|     23 | 4681 | `				zBuf = "INF";` |
|     23 | 4682 | `				length = 3;` |
|      - | 4683 | `			}` |
|     37 | 4684 | `			width = 0;` |
|     37 | 4685 | `			break;` |
|      - | 4686 | `		}` |
|    121 | 4687 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|    121 | 4688 | `		if( precision > 53 ){` |
|      - | 4689 | `			/* php's FORMAT_CONV_MAX_PRECISION cap, with the same E_NOTICE` |
|      - | 4690 | `			 * (message prefixed with the active function's name, like` |
|      - | 4691 | `			 * php_error_docref). */` |
|      - | 4692 | `			char zMsg[160];` |
|      4 | 4693 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 4694 | `				"%z(): Requested precision of %d digits was truncated to PHP maximum of %d digits",` |
|      2 | 4695 | `				&pCtx->pFunc->sName,precision,53);` |
|      3 | 4696 | `			PH7_VmThrowError(pCtx->pVm,0,E_NOTICE,zMsg);` |
|      3 | 4697 | `			precision = 53;` |
|      1 | 4698 | `		}` |
|      - | 4699 | ``		/* php's %f/%e extract the sign via `num < 0`, so negative zero prints`` |
|      - | 4700 | `		 * unsigned there — while %g (php_gcvt on the raw value) keeps "-0". */` |
|    121 | 4701 | `		if( xtype!=PH7_FMT_GENERIC && realvalue == 0.0 ){` |
|      9 | 4702 | `			realvalue = 0.0;` |
|      4 | 4703 | `		}` |
|      - | 4704 | `		/* php's float conversions are correctly rounded (zend_dtoa); use libc` |
|      - | 4705 | `		 * snprintf as the digit engine (the byte-exact-floats rule — the old` |
|      - | 4706 | `		 * hand-rolled vxGetdigit loop stopped at 16 significant digits, so` |
|      - | 4707 | `		 * e.g. %f of 1e308 printed zeros where php prints the exact binary64` |
|      - | 4708 | `		 * expansion), then post-process into php's exact shapes below. */` |
|    121 | 4709 | `		nFmt = 0;` |
|    121 | 4710 | `		zFmt[nFmt++] = '%';` |
|    121 | 4711 | `		if( flag_alternateform ) zFmt[nFmt++] = '#';` |
|      - | 4712 | `		/* php's ' ' flag selects space PADDING (its default), not C's` |
|      - | 4713 | `		 * space-for-positive-sign — so flag_blanksign is NOT forwarded. */` |
|    121 | 4714 | `		if( flag_plussign ) zFmt[nFmt++] = '+';` |
|    121 | 4715 | `		zFmt[nFmt++] = '.';` |
|    121 | 4716 | `		zFmt[nFmt++] = '*';` |
|    165 | 4717 | `		zFmt[nFmt++] = (char)(xtype==PH7_FMT_FLOAT ? 'f' :` |
|     30 | 4718 | `			(xtype==PH7_FMT_EXP ? ((pInfo->charset[0]=='E') ? 'E' : 'e')` |
|     28 | 4719 | `			                    : ((pInfo->charset[0]=='E') ? 'G' : 'g')));` |
|    121 | 4720 | `		zFmt[nFmt] = 0;` |
|    121 | 4721 | `		nOut = snprintf(zWorker,sizeof(zWorker),zFmt,precision,realvalue);` |
|    121 | 4722 | `		if( nOut < 0 \|\| nOut >= (int)sizeof(zWorker) ){` |
|      - | 4723 | `			/* Cannot happen with precision capped at 53 (%f of DBL_MAX is` |
|      - | 4724 | `			 * ~365 bytes); keep the truncated output rather than overrun. */` |
|    ! 0 | 4725 | `			nOut = (int)SyStrlen(zWorker);` |
|    ! 0 | 4726 | `		}` |
|    121 | 4727 | `		nOut = (int)PH7_PhpFloatShape(zWorker,(sxi32)nOut,xtype==PH7_FMT_GENERIC);` |
|    121 | 4728 | `		zBuf = zWorker;` |
|    121 | 4729 | `		length = nOut;` |
|      - | 4730 | `		/* Let the zero-pad block below insert zeros between the sign (written` |
|      - | 4731 | `		 * by snprintf) and the first digit, as before. */` |
|    121 | 4732 | `		prefix = (zWorker[0]=='-' \|\| zWorker[0]=='+' \|\| zWorker[0]==' ') ? zWorker[0] : 0;` |
|      - | 4733 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 4734 | `        ** set and we are not left justified */` |
|    121 | 4735 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 4736 | `          int i;` |
|      7 | 4737 | `          int nPad = width - length;` |
|     51 | 4738 | `          for(i=width; i>=nPad; i--){` |
|     45 | 4739 | `            zBuf[i] = zBuf[i-nPad];` |
|     23 | 4740 | `          }` |
|      7 | 4741 | `          i = prefix!=0;` |
|     29 | 4742 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      7 | 4743 | `          length = width;` |
|      3 | 4744 | `        }` |
|      - | 4745 | `#else` |
|      - | 4746 | `         zBuf = " ";` |
|      - | 4747 | `		 length = (int)sizeof(char);` |
|      - | 4748 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|    121 | 4749 | `		 break;` |
|      - | 4750 | `							 }` |
|    ! 0 | 4751 | `		default:` |
|      - | 4752 | `			/* Unreachable: PH7_FormatValidate() rejects unknown specifiers with a` |
|      - | 4753 | `			 * catchable ValueError before formatting begins. Kept as a defensive` |
|      - | 4754 | `			 * no-op that emits nothing. */` |
|    ! 0 | 4755 | `			length = 0;` |
|    ! 0 | 4756 | `			break;` |
|      - | 4757 | `		}` |
|      - | 4758 | `		 /*` |
|      - | 4759 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 4760 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 4761 | `		 ** the output.` |
|      - | 4762 | `		 */` |
|    773 | 4763 | `    if( !flag_leftjustify ){` |
|      - | 4764 | `      register int nspace;` |
|    759 | 4765 | `      nspace = width-length;` |
|    759 | 4766 | `      if( nspace>0 ){` |
|      7 | 4767 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4768 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4769 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4770 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4771 | `			}` |
|    ! 0 | 4772 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4773 | `        }` |
|      7 | 4774 | `        if( nspace>0 ){` |
|      7 | 4775 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      7 | 4776 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4777 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4778 | `			}` |
|      3 | 4779 | `		}` |
|      3 | 4780 | `      }` |
|    379 | 4781 | `    }` |
|    773 | 4782 | `    if( length>0 ){` |
|    773 | 4783 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    773 | 4784 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4785 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4786 | `		}` |
|    386 | 4787 | `    }` |
|    773 | 4788 | `    if( flag_leftjustify ){` |
|      - | 4789 | `      register int nspace;` |
|     15 | 4790 | `      nspace = width-length;` |
|     15 | 4791 | `      if( nspace>0 ){` |
|     11 | 4792 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4793 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4794 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4795 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4796 | `			}` |
|    ! 0 | 4797 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4798 | `        }` |
|     11 | 4799 | `        if( nspace>0 ){` |
|     11 | 4800 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|     11 | 4801 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4802 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4803 | `			}` |
|      5 | 4804 | `		}` |
|      5 | 4805 | `      }` |
|      7 | 4806 | `    }` |
|      1 | 4807 | ` }/* for(;;) */` |
|    395 | 4808 | `	return SXRET_OK;` |
|    198 | 4809 | `}` |
|      - | 4810 | `/*` |
|      - | 4811 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 4812 | ` */` |
|    352 | 4813 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4814 | `{` |
|      - | 4815 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|      - | 4816 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|      - | 4817 | `	 * non-OK rc also stops the format loop. */` |
|    353 | 4818 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|    353 | 4819 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|    353 | 4820 | `	return *pRc;` |
|      1 | 4821 | `}` |
|      - | 4822 | `/*` |
|      - | 4823 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 4824 | ` *  Return a formatted string.` |
|      - | 4825 | ` * Parameters` |
|      - | 4826 | ` *  $format` |
|      - | 4827 | ` *    The format string (see block comment above)` |
|      - | 4828 | ` * Return` |
|      - | 4829 | ` *  A string produced according to the formatting string format.` |
|      - | 4830 | ` */` |
|    184 | 4831 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4832 | `{` |
|      - | 4833 | `	const char *zFormat;` |
|    185 | 4834 | `	sxi32 rc = SXRET_OK;` |
|      - | 4835 | `	int nLen;` |
|    185 | 4836 | `	if( nArg < 1 ){` |
|      - | 4837 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4838 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4839 | `		return PH7_OK;` |
|      - | 4840 | `	}` |
|      - | 4841 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|    185 | 4842 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    185 | 4843 | `	if( rc != PH7_OK ){` |
|    ! 0 | 4844 | `		return rc;` |
|      - | 4845 | `	}` |
|      - | 4846 | `	/* Extract the string format (scalars/null coerce). */` |
|    185 | 4847 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    185 | 4848 | `	if( nLen < 1 ){` |
|      - | 4849 | `		/* Empty string */` |
|    ! 0 | 4850 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4851 | `		return PH7_OK;` |
|      - | 4852 | `	}` |
|      - | 4853 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4854 | `	 * output; propagate the throw status verbatim. */` |
|    185 | 4855 | `	rc = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    185 | 4856 | `	if( rc != PH7_OK ){` |
|     17 | 4857 | `		return rc;` |
|      - | 4858 | `	}` |
|      - | 4859 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|    169 | 4860 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|    169 | 4861 | `	if( rc != SXRET_OK ){` |
|      - | 4862 | `		/* The result append ran out of memory: raise a fatal rather than` |
|      - | 4863 | `		 * returning a silently-truncated string. */` |
|    ! 0 | 4864 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4865 | `	}` |
|    169 | 4866 | `	return PH7_OK;` |
|     93 | 4867 | `}` |
|      - | 4868 | `/*` |
|      - | 4869 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 4870 | ` */` |
|   1130 | 4871 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4872 | `{` |
|   1131 | 4873 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 4874 | `	/* Call the VM output consumer directly */` |
|   1131 | 4875 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 4876 | `	/* Increment counter */` |
|   1131 | 4877 | `	*pCounter += nLen;` |
|   1131 | 4878 | `	return PH7_OK;` |
|      1 | 4879 | `}` |
|      - | 4880 | `/*` |
|      - | 4881 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 4882 | ` *  Output a formatted string.` |
|      - | 4883 | ` * Parameters` |
|      - | 4884 | ` *  $format` |
|      - | 4885 | ` *   See sprintf() for a description of format.` |
|      - | 4886 | ` * Return` |
|      - | 4887 | ` *  The length of the outputted string.` |
|      - | 4888 | ` */` |
|    200 | 4889 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4890 | `{` |
|    201 | 4891 | `	ph7_int64 nCounter = 0;` |
|      - | 4892 | `	const char *zFormat;` |
|      - | 4893 | `	int nLen;` |
|    201 | 4894 | `	if( nArg < 1 ){` |
|      - | 4895 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 4896 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4897 | `		return PH7_OK;` |
|      - | 4898 | `	}` |
|      - | 4899 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|      - | 4900 | `	{` |
|    201 | 4901 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    201 | 4902 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 4903 | `			return rcf;` |
|      - | 4904 | `		}` |
|      - | 4905 | `	}` |
|      - | 4906 | `	/* Extract the string format (scalars/null coerce). */` |
|    201 | 4907 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    201 | 4908 | `	if( nLen < 1 ){` |
|      - | 4909 | `		/* Empty string */` |
|    ! 0 | 4910 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4911 | `		return PH7_OK;` |
|      - | 4912 | `	}` |
|      - | 4913 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4914 | `	 * output; propagate the throw status verbatim. */` |
|      - | 4915 | `	{` |
|    201 | 4916 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    201 | 4917 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 4918 | `			return rcv;` |
|      - | 4919 | `		}` |
|      - | 4920 | `	}` |
|      - | 4921 | `	/* Format the string */` |
|    201 | 4922 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 4923 | `	/* Return the length of the outputted string */` |
|    201 | 4924 | `	ph7_result_int64(pCtx,nCounter);` |
|    201 | 4925 | `	return PH7_OK;` |
|    101 | 4926 | `}` |
|      - | 4927 | `/*` |
|      - | 4928 | ` * int vprintf(string $format,array $args)` |
|      - | 4929 | ` *  Output a formatted string.` |
|      - | 4930 | ` * Parameters` |
|      - | 4931 | ` *  $format` |
|      - | 4932 | ` *   See sprintf() for a description of format.` |
|      - | 4933 | ` * Return` |
|      - | 4934 | ` *  The length of the outputted string.` |
|      - | 4935 | ` */` |
|      4 | 4936 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4937 | `{` |
|      5 | 4938 | `	ph7_int64 nCounter = 0;` |
|      - | 4939 | `	const char *zFormat;` |
|      - | 4940 | `	ph7_hashmap *pMap;` |
|      - | 4941 | `	SySet sArg;` |
|      - | 4942 | `	int nLen,n;` |
|      - | 4943 | `	sxi32 rcFmt;` |
|      5 | 4944 | `	if( nArg < 2 ){` |
|      - | 4945 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 4946 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4947 | `		return PH7_OK;` |
|      - | 4948 | `	}` |
|      - | 4949 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|      5 | 4950 | `	rcFmt = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|      5 | 4951 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4952 | `		return rcFmt;` |
|      - | 4953 | `	}` |
|      5 | 4954 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 4955 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 4956 | `		char zBuf[64];` |
|      4 | 4957 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4958 | `			"vprintf(): Argument #2 ($values) must be of type array, %s given",` |
|      2 | 4959 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 4960 | `	}` |
|      - | 4961 | `	/* Extract the string format (scalars/null coerce). */` |
|      3 | 4962 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4963 | `	if( nLen < 1 ){` |
|      - | 4964 | `		/* Empty string */` |
|    ! 0 | 4965 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4966 | `		return PH7_OK;` |
|      - | 4967 | `	}` |
|      - | 4968 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4969 | `	 * output; propagate the throw status verbatim. */` |
|      3 | 4970 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      3 | 4971 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4972 | `		return rcFmt;` |
|      - | 4973 | `	}` |
|      - | 4974 | `	/* Point to the hashmap */` |
|      3 | 4975 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4976 | `	/* Extract arguments from the hashmap */` |
|      3 | 4977 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4978 | `	/* Format the string */` |
|      3 | 4979 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 4980 | `	/* Release the container */` |
|      3 | 4981 | `	SySetRelease(&sArg);` |
|      - | 4982 | `	/* Return the length of the outputted string */` |
|      3 | 4983 | `	ph7_result_int64(pCtx,nCounter);` |
|      3 | 4984 | `	return PH7_OK;` |
|      3 | 4985 | `}` |
|      - | 4986 | `/*` |
|      - | 4987 | ` * int vsprintf(string $format,array $args)` |
|      - | 4988 | ` *  Output a formatted string.` |
|      - | 4989 | ` * Parameters` |
|      - | 4990 | ` *  $format` |
|      - | 4991 | ` *   See sprintf() for a description of format.` |
|      - | 4992 | ` * Return` |
|      - | 4993 | ` *  A string produced according to the formatting string format.` |
|      - | 4994 | ` */` |
|     18 | 4995 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4996 | `{` |
|      - | 4997 | `	const char *zFormat;` |
|      - | 4998 | `	ph7_hashmap *pMap;` |
|      - | 4999 | `	SySet sArg;` |
|     19 | 5000 | `	sxi32 rc = SXRET_OK;` |
|      - | 5001 | `	sxi32 rcFmt;` |
|      - | 5002 | `	int nLen,n;` |
|     19 | 5003 | `	if( nArg < 2 ){` |
|      - | 5004 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5005 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5006 | `		return PH7_OK;` |
|      - | 5007 | `	}` |
|      - | 5008 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|     19 | 5009 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|     19 | 5010 | `	if( rc != PH7_OK ){` |
|    ! 0 | 5011 | `		return rc;` |
|      - | 5012 | `	}` |
|     19 | 5013 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 5014 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 5015 | `		char zBuf[64];` |
|     16 | 5016 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5017 | `			"vsprintf(): Argument #2 ($values) must be of type array, %s given",` |
|     10 | 5018 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 5019 | `	}` |
|      - | 5020 | `	/* Extract the string format (scalars/null coerce). */` |
|      9 | 5021 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 5022 | `	if( nLen < 1 ){` |
|      - | 5023 | `		/* Empty string */` |
|    ! 0 | 5024 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5025 | `		return PH7_OK;` |
|      - | 5026 | `	}` |
|      - | 5027 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 5028 | `	 * output; propagate the throw status verbatim. */` |
|      9 | 5029 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      9 | 5030 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 5031 | `		return rcFmt;` |
|      - | 5032 | `	}` |
|      - | 5033 | `	/* Point to hashmap */` |
|      9 | 5034 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 5035 | `	/* Extract arguments from the hashmap */` |
|      9 | 5036 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 5037 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|      9 | 5038 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|      - | 5039 | `	/* Release the container */` |
|      9 | 5040 | `	SySetRelease(&sArg);` |
|      9 | 5041 | `	if( rc != SXRET_OK ){` |
|      - | 5042 | `		/* The result append ran out of memory: raise a fatal. */` |
|    ! 0 | 5043 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5044 | `	}` |
|      9 | 5045 | `	return PH7_OK;` |
|     10 | 5046 | `}` |
|      - | 5047 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5048 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5049 | `/*` |
|      - | 5050 | ` * Symisc eXtension.` |
|      - | 5051 | ` * string size_format(int64 $size)` |
|      - | 5052 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 5053 | ` *  Example:` |
|      - | 5054 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 5055 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 5056 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 5057 | ` * Parameter` |
|      - | 5058 | ` *  $size` |
|      - | 5059 | ` *    Entity size in bytes.` |
|      - | 5060 | ` * Return` |
|      - | 5061 | ` *   Formatted string representation of the given size.` |
|      - | 5062 | ` */` |
|     24 | 5063 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5064 | `{` |
|      - | 5065 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 5066 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 5067 | `	sxi32 nRest,i_32;` |
|      - | 5068 | `	ph7_int64 iSize;` |
|     25 | 5069 | `	int c = -1; /* index in zUnit[] */` |
|      - | 5070 |  |
|     25 | 5071 | `	if( nArg < 1 ){` |
|      - | 5072 | `		/* Missing argument,return the empty string */` |
|      3 | 5073 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5074 | `		return PH7_OK;` |
|      - | 5075 | `	}` |
|      - | 5076 | `	/* Extract the given size */` |
|     23 | 5077 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 5078 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 5079 | `		/* Don't bother formatting,return immediately */` |
|      5 | 5080 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 5081 | `		return PH7_OK;` |
|      - | 5082 | `	}` |
|     19 | 5083 | `	for(;;){` |
|     39 | 5084 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 5085 | `		iSize >>= 10;` |
|     39 | 5086 | `		c++;` |
|     39 | 5087 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 5088 | `			break;` |
|      - | 5089 | `		}` |
|      1 | 5090 | `	}` |
|     19 | 5091 | `	nRest /= 100;` |
|     19 | 5092 | `	if( nRest > 9 ){` |
|    ! 0 | 5093 | `		nRest = 9;` |
|    ! 0 | 5094 | `	}` |
|     19 | 5095 | `	if( iSize > 999 ){` |
|    ! 0 | 5096 | `		c++;` |
|    ! 0 | 5097 | `		nRest = 9;` |
|    ! 0 | 5098 | `		iSize = 0;` |
|    ! 0 | 5099 | `	}` |
|     19 | 5100 | `	i_32 = (sxi32)iSize;` |
|      - | 5101 | `	/* Format */` |
|     19 | 5102 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 5103 | `	return PH7_OK;` |
|     13 | 5104 | `}` |
|      - | 5105 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 5106 | `/*` |
|      - | 5107 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 5108 | ` *   Calculate the md5 hash of a string.` |
|      - | 5109 | ` * Parameter` |
|      - | 5110 | ` *  $str` |
|      - | 5111 | ` *   Input string` |
|      - | 5112 | ` * $raw_output` |
|      - | 5113 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 5114 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 5115 | ` * Return` |
|      - | 5116 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 5117 | ` */` |
|     12 | 5118 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5119 | `{` |
|      - | 5120 | `	unsigned char zDigest[16];` |
|     13 | 5121 | `	int raw_output = FALSE;` |
|      - | 5122 | `	const void *pIn;` |
|      - | 5123 | `	int nLen;` |
|     13 | 5124 | `	if( nArg < 1 ){` |
|      - | 5125 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5126 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5127 | `		return PH7_OK;` |
|      - | 5128 | `	}` |
|      - | 5129 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 5130 | `	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */` |
|     13 | 5131 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 5132 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 5133 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 5134 | `	}` |
|      - | 5135 | `	/* Compute the MD5 digest */` |
|     13 | 5136 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|     13 | 5137 | `	if( raw_output ){` |
|      - | 5138 | `		/* Output raw digest */` |
|      5 | 5139 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 5140 | `	}else{` |
|      - | 5141 | `		/* Perform a binary to hex conversion */` |
|      9 | 5142 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 5143 | `	}` |
|     13 | 5144 | `	return PH7_OK;` |
|      7 | 5145 | `}` |
|      - | 5146 | `/*` |
|      - | 5147 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 5148 | ` *   Calculate the sha1 hash of a string.` |
|      - | 5149 | ` * Parameter` |
|      - | 5150 | ` *  $str` |
|      - | 5151 | ` *   Input string` |
|      - | 5152 | ` * $raw_output` |
|      - | 5153 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 5154 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 5155 | ` * Return` |
|      - | 5156 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 5157 | ` */` |
|     10 | 5158 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5159 | `{` |
|      - | 5160 | `	unsigned char zDigest[20];` |
|     11 | 5161 | `	int raw_output = FALSE;` |
|      - | 5162 | `	const void *pIn;` |
|      - | 5163 | `	int nLen;` |
|     11 | 5164 | `	if( nArg < 1 ){` |
|      - | 5165 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5166 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5167 | `		return PH7_OK;` |
|      - | 5168 | `	}` |
|      - | 5169 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 5170 | `	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */` |
|     11 | 5171 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5172 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 5173 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 5174 | `	}` |
|      - | 5175 | `	/* Compute the SHA1 digest */` |
|     11 | 5176 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|     11 | 5177 | `	if( raw_output ){` |
|      - | 5178 | `		/* Output raw digest */` |
|      5 | 5179 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 5180 | `	}else{` |
|      - | 5181 | `		/* Perform a binary to hex conversion */` |
|      7 | 5182 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 5183 | `	}` |
|     11 | 5184 | `	return PH7_OK;` |
|      6 | 5185 | `}` |
|      - | 5186 | `/*` |
|      - | 5187 | ` * int64 crc32(string $str)` |
|      - | 5188 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 5189 | ` * Parameter` |
|      - | 5190 | ` *  $str` |
|      - | 5191 | ` *   Input string` |
|      - | 5192 | ` * Return` |
|      - | 5193 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 5194 | ` */` |
|      2 | 5195 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5196 | `{` |
|      - | 5197 | `	const void *pIn;` |
|      - | 5198 | `	sxu32 nCRC;` |
|      - | 5199 | `	int nLen;` |
|      3 | 5200 | `	if( nArg < 1 ){` |
|      - | 5201 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 5202 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5203 | `		return PH7_OK;` |
|      - | 5204 | `	}` |
|      - | 5205 | `	/* Extract the input string */` |
|      3 | 5206 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5207 | `	if( nLen < 1 ){` |
|      - | 5208 | `		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike` |
|      - | 5209 | `		 * md5()/sha1(), whose empty-string digests are non-zero. */` |
|    ! 0 | 5210 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5211 | `		return PH7_OK;` |
|      - | 5212 | `	}` |
|      - | 5213 | `	/* Calculate the sum */` |
|      3 | 5214 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 5215 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 5216 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 5217 | `	return PH7_OK;` |
|      2 | 5218 | `}` |
|      - | 5219 | `/*` |
|      - | 5220 | ` * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is` |
|      - | 5221 | ` * described by a small record so one dispatch (and one generic HMAC) serves them` |
|      - | 5222 | ` * all. Thin adapters normalize the differing context types and the reversed` |
|      - | 5223 | ` * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.` |
|      - | 5224 | ` */` |
|     11 | 5225 | `static void HashMd5Init(HashCtx *c){ MD5Init(&c->md5); }` |
|     15 | 5226 | `static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }` |
|     11 | 5227 | `static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }` |
|     11 | 5228 | `static void HashSha1Init(HashCtx *c){ SHA1Init(&c->sha1); }` |
|     15 | 5229 | `static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }` |
|     11 | 5230 | `static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }` |
|      9 | 5231 | `static void HashSha224Init(HashCtx *c){ SHA224Init(&c->sha256); }` |
|     33 | 5232 | `static void HashSha256Init(HashCtx *c){ SHA256Init(&c->sha256); }` |
|     57 | 5233 | `static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }` |
|     41 | 5234 | `static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }` |
|      9 | 5235 | `static void HashSha384Init(HashCtx *c){ SHA384Init(&c->sha512); }` |
|     15 | 5236 | `static void HashSha512Init(HashCtx *c){ SHA512Init(&c->sha512); }` |
|     27 | 5237 | `static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }` |
|     23 | 5238 | `static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }` |
|      - | 5239 | `typedef struct HashAlgo HashAlgo;` |
|      - | 5240 | `struct HashAlgo {` |
|      - | 5241 | `	const char *zName;   /* lowercase canonical name */` |
|      - | 5242 | `	int nDigestLen;      /* output bytes: 16/20/28/32/48/64 */` |
|      - | 5243 | `	int nBlockLen;       /* internal block bytes (for HMAC): 64 or 128 */` |
|      - | 5244 | `	void (*xInit)(HashCtx *);` |
|      - | 5245 | `	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);` |
|      - | 5246 | `	void (*xFinal)(HashCtx *,unsigned char *);` |
|      - | 5247 | `};` |
|      - | 5248 | `static const HashAlgo aHashAlgo[] = {` |
|      - | 5249 | `	{ "md5",    16, 64,  HashMd5Init,    HashMd5Update,    HashMd5Final    },` |
|      - | 5250 | `	{ "sha1",   20, 64,  HashSha1Init,   HashSha1Update,   HashSha1Final   },` |
|      - | 5251 | `	{ "sha224", 28, 64,  HashSha224Init, HashSha256Update, HashSha256Final },` |
|      - | 5252 | `	{ "sha256", 32, 64,  HashSha256Init, HashSha256Update, HashSha256Final },` |
|      - | 5253 | `	{ "sha384", 48, 128, HashSha384Init, HashSha512Update, HashSha512Final },` |
|      - | 5254 | `	{ "sha512", 64, 128, HashSha512Init, HashSha512Update, HashSha512Final },` |
|      - | 5255 | `};` |
|      - | 5256 | `/* Case-insensitive algorithm lookup (PHP accepts 'SHA256' etc.). */` |
|     73 | 5257 | `static const HashAlgo * HashFindAlgo(const char *zName,int nLen){` |
|      - | 5258 | `	sxu32 i;` |
|    279 | 5259 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|    272 | 5260 | `		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen` |
|    211 | 5261 | `			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){` |
|     67 | 5262 | `			return &aHashAlgo[i];` |
|      - | 5263 | `		}` |
|    106 | 5264 | `	}` |
|      6 | 5265 | `	return 0;` |
|     38 | 5266 | `}` |
|      - | 5267 | `/*` |
|      - | 5268 | ` * string hash(string $algo,string $data[,bool $binary = false])` |
|      - | 5269 | ` *   Generate a hash value (message digest).` |
|      - | 5270 | ` */` |
|     54 | 5271 | `static int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5272 | `{` |
|      - | 5273 | `	const HashAlgo *pAlgo;` |
|      - | 5274 | `	const char *zAlgo,*zData;` |
|     56 | 5275 | `	int nAlgoLen,nDataLen,raw_output = FALSE;` |
|      - | 5276 | `	HashCtx sCtx;` |
|      - | 5277 | `	unsigned char zDigest[64];` |
|     56 | 5278 | `	if( nArg < 2 ){` |
|    ! 0 | 5279 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5280 | `			"hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5281 | `	}` |
|     56 | 5282 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     56 | 5283 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     56 | 5284 | `	if( pAlgo == 0 ){` |
|      3 | 5285 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5286 | `			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - | 5287 | `	}` |
|     53 | 5288 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     53 | 5289 | `	if( nArg > 2 ){` |
|      9 | 5290 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|      4 | 5291 | `	}` |
|     53 | 5292 | `	pAlgo->xInit(&sCtx);` |
|     53 | 5293 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     53 | 5294 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     53 | 5295 | `	if( raw_output ){` |
|      9 | 5296 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|      5 | 5297 | `	}else{` |
|     45 | 5298 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - | 5299 | `	}` |
|     53 | 5300 | `	return PH7_OK;` |
|     29 | 5301 | `}` |
|      - | 5302 | `/*` |
|      - | 5303 | ` * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])` |
|      - | 5304 | ` *   Generate a keyed hash value using the HMAC method (RFC 2104).` |
|      - | 5305 | ` */` |
|     16 | 5306 | `static int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5307 | `{` |
|      - | 5308 | `	const HashAlgo *pAlgo;` |
|      - | 5309 | `	const char *zAlgo,*zData,*zKey;` |
|     18 | 5310 | `	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;` |
|      - | 5311 | `	HashCtx sCtx;` |
|      - | 5312 | `	unsigned char zKeyBlock[128],zIpad[128],zOpad[128],zInner[64],zDigest[64];` |
|      - | 5313 | `	int i,nBlock,nDigest;` |
|     18 | 5314 | `	if( nArg < 3 ){` |
|    ! 0 | 5315 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5316 | `			"hash_hmac() expects at least 3 arguments, %d given",nArg);` |
|      - | 5317 | `	}` |
|     18 | 5318 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     18 | 5319 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     18 | 5320 | `	if( pAlgo == 0 ){` |
|      3 | 5321 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5322 | `			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 5323 | `	}` |
|     15 | 5324 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     15 | 5325 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|     15 | 5326 | `	if( nArg > 3 ){` |
|      3 | 5327 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|      1 | 5328 | `	}` |
|     15 | 5329 | `	nBlock = pAlgo->nBlockLen;` |
|     15 | 5330 | `	nDigest = pAlgo->nDigestLen;` |
|      - | 5331 | `	/* Reduce the key to a single block: hash it if longer than the block, then` |
|      - | 5332 | `	 * zero-pad (a short or empty key is just zero-padded). */` |
|     15 | 5333 | `	SyZero(zKeyBlock,sizeof(zKeyBlock));` |
|     15 | 5334 | `	if( nKeyLen > nBlock ){` |
|      3 | 5335 | `		pAlgo->xInit(&sCtx);` |
|      3 | 5336 | `		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);` |
|      3 | 5337 | `		pAlgo->xFinal(&sCtx,zKeyBlock);` |
|     14 | 5338 | `	}else if( nKeyLen > 0 ){` |
|     11 | 5339 | `		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);` |
|      5 | 5340 | `	}` |
|   1039 | 5341 | `	for( i = 0; i < nBlock; i++ ){` |
|   1025 | 5342 | `		zIpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);` |
|   1025 | 5343 | `		zOpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);` |
|    513 | 5344 | `	}` |
|      - | 5345 | `	/* inner = H((key ^ ipad) \|\| data) */` |
|     15 | 5346 | `	pAlgo->xInit(&sCtx);` |
|     15 | 5347 | `	pAlgo->xUpdate(&sCtx,zIpad,(unsigned int)nBlock);` |
|     15 | 5348 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     15 | 5349 | `	pAlgo->xFinal(&sCtx,zInner);` |
|      - | 5350 | `	/* out = H((key ^ opad) \|\| inner) */` |
|     15 | 5351 | `	pAlgo->xInit(&sCtx);` |
|     15 | 5352 | `	pAlgo->xUpdate(&sCtx,zOpad,(unsigned int)nBlock);` |
|     15 | 5353 | `	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)nDigest);` |
|     15 | 5354 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     15 | 5355 | `	if( raw_output ){` |
|      3 | 5356 | `		ph7_result_string(pCtx,(const char *)zDigest,nDigest);` |
|      2 | 5357 | `	}else{` |
|     13 | 5358 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);` |
|      - | 5359 | `	}` |
|     15 | 5360 | `	return PH7_OK;` |
|     10 | 5361 | `}` |
|      - | 5362 | `/*` |
|      - | 5363 | ` * bool hash_equals(string $known_string,string $user_string)` |
|      - | 5364 | ` *   Timing-attack-safe string comparison.` |
|      - | 5365 | ` */` |
|     12 | 5366 | `static int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5367 | `{` |
|      - | 5368 | `	const char *zKnown,*zUser;` |
|      - | 5369 | `	int nKnown,nUser,i;` |
|     14 | 5370 | `	volatile unsigned char vDiff = 0;` |
|     14 | 5371 | `	if( nArg < 2 ){` |
|    ! 0 | 5372 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5373 | `			"hash_equals() expects exactly 2 arguments, %d given",nArg);` |
|      - | 5374 | `	}` |
|     14 | 5375 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      4 | 5376 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5377 | `			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",` |
|      1 | 5378 | `			ph7_type_name(apArg[0]));` |
|      - | 5379 | `	}` |
|     11 | 5380 | `	if( !ph7_value_is_string(apArg[1]) ){` |
|    ! 0 | 5381 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5382 | `			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",` |
|    ! 0 | 5383 | `			ph7_type_name(apArg[1]));` |
|      - | 5384 | `	}` |
|     11 | 5385 | `	zKnown = ph7_value_to_string(apArg[0],&nKnown);` |
|     11 | 5386 | `	zUser = ph7_value_to_string(apArg[1],&nUser);` |
|     11 | 5387 | `	if( nKnown != nUser ){` |
|      5 | 5388 | `		ph7_result_bool(pCtx,0);` |
|      5 | 5389 | `		return PH7_OK;` |
|      - | 5390 | `	}` |
|      - | 5391 | `	/* Constant-time: read every byte, never short-circuit. */` |
|     19 | 5392 | `	for( i = 0; i < nKnown; i++ ){` |
|     13 | 5393 | `		vDiff \|= (unsigned char)(zKnown[i] ^ zUser[i]);` |
|      7 | 5394 | `	}` |
|      7 | 5395 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|      7 | 5396 | `	return PH7_OK;` |
|      8 | 5397 | `}` |
|      - | 5398 | `/*` |
|      - | 5399 | ` * array hash_algos(void)` |
|      - | 5400 | ` *   Return a list of the registered hashing algorithms.` |
|      - | 5401 | ` */` |
|      2 | 5402 | `static int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5403 | `{` |
|      - | 5404 | `	ph7_value *pArray,*pValue;` |
|      - | 5405 | `	sxu32 i;` |
|      1 | 5406 | `	SXUNUSED(nArg);` |
|      1 | 5407 | `	SXUNUSED(apArg);` |
|      3 | 5408 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5409 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 5410 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 5411 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5412 | `		return PH7_OK;` |
|      - | 5413 | `	}` |
|     15 | 5414 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|     13 | 5415 | `		ph7_value_string(pValue,aHashAlgo[i].zName,-1);` |
|     13 | 5416 | `		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);` |
|     13 | 5417 | `		ph7_value_reset_string_cursor(pValue);` |
|      7 | 5418 | `	}` |
|      3 | 5419 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5420 | `	return PH7_OK;` |
|      2 | 5421 | `}` |
|      - | 5422 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 5423 | `/*` |
|      - | 5424 | ` * password_* (bcrypt). These live in ext/standard in real PHP — outside the` |
|      - | 5425 | ` * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.` |
|      - | 5426 | ` */` |
|      - | 5427 | `/*` |
|      - | 5428 | ` * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a` |
|      - | 5429 | ` * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).` |
|      - | 5430 | ` */` |
|     40 | 5431 | `static int BcryptParseHash(const char *zHash,int nHash,int *piCost)` |
|      1 | 5432 | `{` |
|      - | 5433 | `	int iCost;` |
|     40 | 5434 | `	if( nHash != 60 \|\| zHash[0] != '$' \|\| zHash[1] != '2' \|\| zHash[3] != '$'` |
|     29 | 5435 | `		\|\| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){` |
|     13 | 5436 | `		return FALSE;` |
|      - | 5437 | `	}` |
|     29 | 5438 | `	if( zHash[4] < '0' \|\| zHash[4] > '9' \|\| zHash[5] < '0' \|\| zHash[5] > '9' \|\| zHash[6] != '$' ){` |
|    ! 0 | 5439 | `		return FALSE;` |
|      - | 5440 | `	}` |
|     29 | 5441 | `	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');` |
|     29 | 5442 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      3 | 5443 | `		return FALSE;` |
|      - | 5444 | `	}` |
|     27 | 5445 | `	if( piCost ){ *piCost = iCost; }` |
|     27 | 5446 | `	return TRUE;` |
|     21 | 5447 | `}` |
|      - | 5448 | `/*` |
|      - | 5449 | ` * TRUE if the $algo argument selects bcrypt: null (PASSWORD_DEFAULT) or the` |
|      - | 5450 | ` * "2y" id (PASSWORD_BCRYPT/PASSWORD_DEFAULT). bcrypt is the only supported algo.` |
|      - | 5451 | ` */` |
|     20 | 5452 | `static int BcryptIsBcryptAlgo(ph7_value *pAlgo)` |
|      3 | 5453 | `{` |
|     23 | 5454 | `	if( ph7_value_is_null(pAlgo) ){` |
|    ! 0 | 5455 | `		return TRUE;` |
|      - | 5456 | `	}` |
|     23 | 5457 | `	if( ph7_value_is_string(pAlgo) ){` |
|      - | 5458 | `		int nAlgo;` |
|     23 | 5459 | `		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);` |
|     23 | 5460 | `		return ( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' );` |
|      - | 5461 | `	}` |
|    ! 0 | 5462 | `	return FALSE;` |
|     13 | 5463 | `}` |
|      - | 5464 | `/*` |
|      - | 5465 | ` * bool\|string password_hash(string $password,string\|int\|null $algo[,array $options])` |
|      - | 5466 | ` *  Create a bcrypt hash of the password.` |
|      - | 5467 | ` */` |
|     16 | 5468 | `static int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 5469 | `{` |
|      - | 5470 | `	const char *zPwd;` |
|     19 | 5471 | `	int nPwd,iCost = 12;` |
|      - | 5472 | `	unsigned char aSalt[16];` |
|      - | 5473 | `	char zHash[60];` |
|     19 | 5474 | `	if( nArg < 2 ){` |
|    ! 0 | 5475 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5476 | `			"password_hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5477 | `	}` |
|     19 | 5478 | `	if( !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      3 | 5479 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5480 | `			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");` |
|      - | 5481 | `	}` |
|      - | 5482 | `	/* cost from $options['cost'] (default 12). */` |
|     16 | 5483 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|     14 | 5484 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|     14 | 5485 | `		if( pCost ){ iCost = ph7_value_to_int(pCost); }` |
|      6 | 5486 | `	}` |
|     16 | 5487 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      4 | 5488 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 5489 | `			"Invalid bcrypt cost parameter specified: %d",iCost);` |
|      - | 5490 | `	}` |
|     13 | 5491 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     13 | 5492 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 5493 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - | 5494 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|      - | 5495 | `	}` |
|     13 | 5496 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){` |
|    ! 0 | 5497 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5498 | `		return PH7_OK;` |
|      - | 5499 | `	}` |
|     13 | 5500 | `	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));` |
|     13 | 5501 | `	return PH7_OK;` |
|     11 | 5502 | `}` |
|      - | 5503 | `/*` |
|      - | 5504 | ` * bool password_verify(string $password,string $hash)` |
|      - | 5505 | ` *  Verify a password against a bcrypt hash. Never throws on a malformed hash.` |
|      - | 5506 | ` */` |
|     28 | 5507 | `static int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5508 | `{` |
|      - | 5509 | `	const char *zPwd,*zHash;` |
|      - | 5510 | `	int nPwd,nHash,iCost,i;` |
|      - | 5511 | `	unsigned char aSalt[16];` |
|      - | 5512 | `	char zComputed[60];` |
|     29 | 5513 | `	volatile unsigned char vDiff = 0;` |
|     29 | 5514 | `	if( nArg < 2 ){` |
|    ! 0 | 5515 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5516 | `			"password_verify() expects exactly 2 arguments, %d given",nArg);` |
|      - | 5517 | `	}` |
|     29 | 5518 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     29 | 5519 | `	zHash = ph7_value_to_string(apArg[1],&nHash);` |
|     29 | 5520 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|     11 | 5521 | `		ph7_result_bool(pCtx,0);` |
|     11 | 5522 | `		return PH7_OK;` |
|      - | 5523 | `	}` |
|      - | 5524 | `	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */` |
|     19 | 5525 | `	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 5526 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5527 | `		return PH7_OK;` |
|      - | 5528 | `	}` |
|     19 | 5529 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){` |
|    ! 0 | 5530 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5531 | `		return PH7_OK;` |
|      - | 5532 | `	}` |
|      - | 5533 | `	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps` |
|      - | 5534 | `	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */` |
|    577 | 5535 | `	for( i = 29; i < 60; i++ ){` |
|    559 | 5536 | `		vDiff \|= (unsigned char)(zComputed[i] ^ zHash[i]);` |
|    280 | 5537 | `	}` |
|     19 | 5538 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|     19 | 5539 | `	return PH7_OK;` |
|     15 | 5540 | `}` |
|      - | 5541 | `/*` |
|      - | 5542 | ` * array password_get_info(string $hash)` |
|      - | 5543 | ` *  Return ["algo"=>id\|null, "algoName"=>name, "options"=>[...]].` |
|      - | 5544 | ` */` |
|      6 | 5545 | `static int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5546 | `{` |
|      7 | 5547 | `	const char *zHash = "";` |
|      7 | 5548 | `	int nHash,iCost = 0,bBcrypt = 0;` |
|      - | 5549 | `	ph7_value *pArray,*pOptions,*pVal;` |
|      7 | 5550 | `	if( nArg > 0 ){` |
|      7 | 5551 | `		zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 5552 | `		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);` |
|      3 | 5553 | `	}` |
|      7 | 5554 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 5555 | `	pOptions = ph7_context_new_array(pCtx);` |
|      7 | 5556 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      7 | 5557 | `	if( pArray == 0 \|\| pOptions == 0 \|\| pVal == 0 ){` |
|    ! 0 | 5558 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5559 | `		return PH7_OK;` |
|      - | 5560 | `	}` |
|      7 | 5561 | `	if( bBcrypt ){` |
|      5 | 5562 | `		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */` |
|      5 | 5563 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 5564 | `		ph7_value_reset_string_cursor(pVal);` |
|      5 | 5565 | `		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);` |
|      5 | 5566 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      5 | 5567 | `		ph7_value_int(pVal,iCost);` |
|      5 | 5568 | `		ph7_array_add_strkey_elem(pOptions,"cost",pVal);` |
|      3 | 5569 | `	}else{` |
|      3 | 5570 | `		ph7_value_null(pVal);                          /* algo => null */` |
|      3 | 5571 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      3 | 5572 | `		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);` |
|      3 | 5573 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      - | 5574 | `	}` |
|      7 | 5575 | `	ph7_array_add_strkey_elem(pArray,"options",pOptions);` |
|      7 | 5576 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 5577 | `	return PH7_OK;` |
|      4 | 5578 | `}` |
|      - | 5579 | `/*` |
|      - | 5580 | ` * bool password_needs_rehash(string $hash,string\|int\|null $algo[,array $options])` |
|      - | 5581 | ` *  True if the hash was not made with the given algo/options.` |
|      - | 5582 | ` */` |
|      6 | 5583 | `static int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5584 | `{` |
|      - | 5585 | `	const char *zHash;` |
|      7 | 5586 | `	int nHash,iCost = 0,iWantCost = 12;` |
|      7 | 5587 | `	if( nArg < 2 ){` |
|    ! 0 | 5588 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5589 | `			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5590 | `	}` |
|      7 | 5591 | `	zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 5592 | `	if( !BcryptParseHash(zHash,nHash,&iCost) \|\| !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      - | 5593 | `		/* A non-bcrypt hash, or a request for a different algo → needs rehash. */` |
|      3 | 5594 | `		ph7_result_bool(pCtx,1);` |
|      3 | 5595 | `		return PH7_OK;` |
|      - | 5596 | `	}` |
|      5 | 5597 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|      5 | 5598 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|      5 | 5599 | `		if( pCost ){ iWantCost = ph7_value_to_int(pCost); }` |
|      2 | 5600 | `	}` |
|      5 | 5601 | `	ph7_result_bool(pCtx,iCost != iWantCost);` |
|      5 | 5602 | `	return PH7_OK;` |
|      4 | 5603 | `}` |
|      - | 5604 | `/*` |
|      - | 5605 | ` * filter_var() — input validation and sanitization (the ext/filter API).` |
|      - | 5606 | ` *` |
|      - | 5607 | ` * Filter and flag identifiers (values match PHP 8.5; the constants themselves` |
|      - | 5608 | ` * are registered in constant.c). The validate filters are hand-rolled rather` |
|      - | 5609 | ` * than delegating to SyStrToInt64/SyStrToReal: the former silently skips leading` |
|      - | 5610 | ` * zeros and cannot signal overflow, and the latter treats ',' as a decimal point` |
|      - | 5611 | ` * unconditionally — neither matches PHP's filter semantics.` |
|      - | 5612 | ` */` |
|      - | 5613 | `#define FV_VALIDATE_INT     257` |
|      - | 5614 | `#define FV_VALIDATE_BOOLEAN 258` |
|      - | 5615 | `#define FV_VALIDATE_FLOAT   259` |
|      - | 5616 | `#define FV_VALIDATE_REGEXP  272` |
|      - | 5617 | `#define FV_VALIDATE_URL     273` |
|      - | 5618 | `#define FV_VALIDATE_EMAIL   274` |
|      - | 5619 | `#define FV_VALIDATE_IP      275` |
|      - | 5620 | `#define FV_VALIDATE_MAC     276` |
|      - | 5621 | `#define FV_VALIDATE_DOMAIN  277` |
|      - | 5622 | `#define FV_SANITIZE_SPECIAL_CHARS      515` |
|      - | 5623 | `#define FV_DEFAULT          516 /* == FILTER_UNSAFE_RAW: pass the value through */` |
|      - | 5624 | `#define FV_SANITIZE_EMAIL   517` |
|      - | 5625 | `#define FV_SANITIZE_URL     518` |
|      - | 5626 | `#define FV_SANITIZE_NUMBER_INT   519` |
|      - | 5627 | `#define FV_SANITIZE_NUMBER_FLOAT 520` |
|      - | 5628 | `#define FV_SANITIZE_FULL_SPECIAL_CHARS 522` |
|      - | 5629 | `#define FV_FLAG_ALLOW_OCTAL  1` |
|      - | 5630 | `#define FV_FLAG_ALLOW_HEX    2` |
|      - | 5631 | `#define FV_FLAG_STRIP_LOW    4` |
|      - | 5632 | `#define FV_FLAG_STRIP_HIGH   8` |
|      - | 5633 | `#define FV_FLAG_ENCODE_LOW   16` |
|      - | 5634 | `#define FV_FLAG_ENCODE_HIGH  32` |
|      - | 5635 | `#define FV_FLAG_ENCODE_AMP   64` |
|      - | 5636 | `#define FV_FLAG_NO_ENCODE_QUOTES 128` |
|      - | 5637 | `#define FV_FLAG_STRIP_BACKTICK   512` |
|      - | 5638 | `#define FV_FLAG_ALLOW_FRACTION   4096` |
|      - | 5639 | `#define FV_FLAG_ALLOW_THOUSAND   8192` |
|      - | 5640 | `#define FV_FLAG_ALLOW_SCIENTIFIC 16384` |
|      - | 5641 | `#define FV_FLAG_IPV4  1048576` |
|      - | 5642 | `#define FV_FLAG_IPV6  2097152` |
|      - | 5643 | `#define FV_NULL_ON_FAILURE 134217728` |
|      - | 5644 | `/* The subset of flags the UNSAFE_RAW/DEFAULT string filter (FvSanitizeString)` |
|      - | 5645 | ` * acts on: when none are set the filter is a verbatim pass-through, so FV_DEFAULT` |
|      - | 5646 | ` * can shortcut. Keep this in sync with FvSanitizeString's flag handling. */` |
|      - | 5647 | `#define FV_FLAG_STRING_MASK (FV_FLAG_STRIP_LOW\|FV_FLAG_STRIP_HIGH\|FV_FLAG_STRIP_BACKTICK \` |
|      - | 5648 | `                            \|FV_FLAG_ENCODE_LOW\|FV_FLAG_ENCODE_HIGH\|FV_FLAG_ENCODE_AMP)` |
|      - | 5649 |  |
|      - | 5650 | `/* Trim leading/trailing PHP whitespace, adjusting the (*pz,*pn) view in place.` |
|      - | 5651 | ` * SyisSpace (isspace) matches PHP's filter whitespace set " \t\n\r\v\f". */` |
|    153 | 5652 | `static void FvTrim(const char **pz,int *pn){` |
|    153 | 5653 | `	const char *z = *pz;` |
|    153 | 5654 | `	int n = *pn;` |
|    157 | 5655 | `	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }` |
|    161 | 5656 | `	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }` |
|    153 | 5657 | `	*pz = z; *pn = n;` |
|    153 | 5658 | `}` |
|      - | 5659 | `/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     57 | 5660 | `static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){` |
|     57 | 5661 | `	int neg = 0, i;` |
|     57 | 5662 | `	sxu64 u = 0;` |
|     57 | 5663 | `	FvTrim(&z,&n);` |
|     57 | 5664 | `	if( n==0 ){ return 0; }` |
|     51 | 5665 | `	if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }` |
|     51 | 5666 | `	if( n==0 ){ return 0; }` |
|     49 | 5667 | `	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'\|\|z[1]=='X') ){` |
|      3 | 5668 | `		z += 2; n -= 2;` |
|      3 | 5669 | `		if( n==0 ){ return 0; }` |
|      7 | 5670 | `		for( i=0; i<n; i++ ){` |
|      5 | 5671 | `			int h = SyHexToint((unsigned char)z[i]);` |
|      5 | 5672 | `			if( h<0 ){ return 0; }` |
|      5 | 5673 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }` |
|      5 | 5674 | `			u = u*16 + (sxu64)h;` |
|      3 | 5675 | `		}` |
|     48 | 5676 | `	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){` |
|      9 | 5677 | `		for( i=0; i<n; i++ ){` |
|      7 | 5678 | `			if( z[i]<'0' \|\| z[i]>'7' ){ return 0; }` |
|      7 | 5679 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }` |
|      7 | 5680 | `			u = u*8 + (sxu64)(z[i]-'0');` |
|      4 | 5681 | `		}` |
|      2 | 5682 | `	}else{` |
|     45 | 5683 | `		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */` |
|    201 | 5684 | `		for( i=0; i<n; i++ ){` |
|    173 | 5685 | `			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|    161 | 5686 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }` |
|    161 | 5687 | `			u = u*10 + (sxu64)(z[i]-'0');` |
|     81 | 5688 | `		}` |
|      - | 5689 | `	}` |
|     33 | 5690 | `	if( neg ){` |
|      5 | 5691 | `		if( u > 0x8000000000000000ULL ){ return 0; }` |
|      5 | 5692 | `		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */` |
|      3 | 5693 | `	}else{` |
|     29 | 5694 | `		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }` |
|     27 | 5695 | `		*pOut = (ph7_int64)u;` |
|      - | 5696 | `	}` |
|     31 | 5697 | `	return 1;` |
|     29 | 5698 | `}` |
|      - | 5699 | `/* FILTER_VALIDATE_FLOAT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     69 | 5700 | `static int FvValidateFloat(const char *z,int n,int flags,double *pOut){` |
|      - | 5701 | `	char zBuf[512];` |
|     69 | 5702 | `	int i, m = 0, seenDigit = 0;` |
|     69 | 5703 | `	const char *zv; int nv; double d = 0;` |
|     69 | 5704 | `	FvTrim(&z,&n);` |
|      - | 5705 | `	/* Bound the input: zBuf[512] holds the thousand-separator-stripped copy, and` |
|      - | 5706 | `	 * the cap also rejects the pathological 500+ digit floats PHP refuses. */` |
|     69 | 5707 | `	if( n==0 \|\| n>500 ){ return 0; }` |
|     69 | 5708 | `	if( flags & FV_FLAG_ALLOW_THOUSAND ){` |
|      - | 5709 | `		/* Commas are optional, but when present they must group the integer part` |
|      - | 5710 | `		 * into a leading run of 1..3 digits followed by groups of exactly 3` |
|      - | 5711 | `		 * ("1,000" ok, "1,5"/"1234,567" rejected). Strip them into zBuf and reject` |
|      - | 5712 | `		 * a comma anywhere in the fractional/exponent tail. */` |
|     25 | 5713 | `		int s = 0, intEnd, segStart, segIdx, hasComma = 0;` |
|     25 | 5714 | `		if( s<n && (z[s]=='+'\|\|z[s]=='-') ){ zBuf[m++] = z[s]; s++; }` |
|     25 | 5715 | `		intEnd = s;` |
|    167 | 5716 | `		while( intEnd<n && z[intEnd]!='.' && z[intEnd]!='e' && z[intEnd]!='E' ){` |
|    143 | 5717 | `			if( z[intEnd]==',' ){ hasComma = 1; }` |
|    143 | 5718 | `			intEnd++;` |
|      1 | 5719 | `		}` |
|     25 | 5720 | `		if( hasComma ){` |
|     25 | 5721 | `			segStart = s; segIdx = 0;` |
|    165 | 5722 | `			for( i=s; i<=intEnd; i++ ){` |
|    151 | 5723 | `				if( i==intEnd \|\| z[i]==',' ){` |
|     49 | 5724 | `					int segLen = i - segStart, k;` |
|     49 | 5725 | `					if( segIdx==0 ){ if( segLen<1 \|\| segLen>3 ){ return 0; } }` |
|     25 | 5726 | `					else if( segLen!=3 ){ return 0; }` |
|    119 | 5727 | `					for( k=segStart; k<i; k++ ){` |
|     81 | 5728 | `						if( !SyisDigit((unsigned char)z[k]) ){ return 0; }` |
|     81 | 5729 | `						zBuf[m++] = z[k];` |
|     41 | 5730 | `					}` |
|     39 | 5731 | `					segStart = i+1; segIdx++;` |
|     19 | 5732 | `				}` |
|     71 | 5733 | `			}` |
|      8 | 5734 | `		}else{` |
|    ! 0 | 5735 | `			for( i=s; i<intEnd; i++ ){ zBuf[m++] = z[i]; }` |
|      - | 5736 | `		}` |
|     27 | 5737 | `		for( i=intEnd; i<n; i++ ){` |
|     13 | 5738 | `			if( z[i]==',' ){ return 0; }` |
|     13 | 5739 | `			zBuf[m++] = z[i];` |
|      7 | 5740 | `		}` |
|     15 | 5741 | `		zv = zBuf; nv = m;` |
|      8 | 5742 | `	}else{` |
|     45 | 5743 | `		zv = z; nv = n;` |
|      - | 5744 | `	}` |
|     59 | 5745 | `	i = 0;` |
|     59 | 5746 | `	if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|    167 | 5747 | `	while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     59 | 5748 | `	if( i<nv && zv[i]=='.' ){` |
|     21 | 5749 | `		i++;` |
|     39 | 5750 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     10 | 5751 | `	}` |
|     59 | 5752 | `	if( !seenDigit ){ return 0; }` |
|     57 | 5753 | `	if( i<nv && (zv[i]=='e'\|\|zv[i]=='E') ){` |
|     29 | 5754 | `		i++;` |
|     29 | 5755 | `		if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|     29 | 5756 | `		if( i>=nv \|\| !SyisDigit((unsigned char)zv[i]) ){ return 0; }` |
|    105 | 5757 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; }` |
|     14 | 5758 | `	}` |
|     57 | 5759 | `	if( i!=nv ){ return 0; } /* trailing junk */` |
|      - | 5760 | `	/* The grammar above guarantees zv[0..nv) is a clean ASCII decimal float (no hex /` |
|      - | 5761 | `	 * inf / nan / trailing junk), so it is safe to hand to libc strtod, which — unlike` |
|      - | 5762 | `	 * SyStrToReal (15 sig-digits + exponent clamped to 308, so it silently saturates` |
|      - | 5763 | `	 * overflowing magnitudes to a finite value) — is overflow/underflow-aware and` |
|      - | 5764 | `	 * correctly rounded. strtod needs a NUL-terminated string: the ALLOW_THOUSAND path` |
|      - | 5765 | `	 * already built the span in zBuf (zv==zBuf); the plain path must copy it there (z is` |
|      - | 5766 | `	 * const + not NUL-terminated). nv <= n <= 500 < sizeof(zBuf) by the cap above.` |
|      - | 5767 | `	 * Matches PHP 8.5 byte-for-byte: reject overflow (-> +/-INF) and total underflow` |
|      - | 5768 | `	 * (-> 0.0), keep subnormals (nonzero, errno==ERANGE) and a genuine "0" (errno==0). */` |
|     53 | 5769 | `	if( zv != zBuf ){ SyMemcpy(zv,zBuf,(sxu32)nv); }` |
|     53 | 5770 | `	zBuf[nv] = 0;` |
|     53 | 5771 | `	errno = 0;` |
|     53 | 5772 | `	d = strtod(zBuf,0);` |
|     53 | 5773 | `	if( errno == ERANGE && (d == HUGE_VAL \|\| d == -HUGE_VAL \|\| d == 0.0) ){` |
|     15 | 5774 | `		return 0;` |
|      - | 5775 | `	}` |
|     39 | 5776 | `	*pOut = d;` |
|     39 | 5777 | `	return 1;` |
|     35 | 5778 | `}` |
|      - | 5779 | `/* FILTER_VALIDATE_BOOLEAN. Returns 1 if the string is recognized (sets *pBool),` |
|      - | 5780 | ` * 0 if it is unrecognized (the failure path). "0"/"false"/"" are recognized as` |
|      - | 5781 | ` * false, NOT failures. */` |
|     33 | 5782 | `static int FvValidateBool(const char *z,int n,int *pBool){` |
|     33 | 5783 | `	FvTrim(&z,&n);` |
|     32 | 5784 | `	if( (n==1 && z[0]=='1') \|\| (n==4 && SyStrnicmp(z,"true",4)==0)` |
|     25 | 5785 | `	    \|\| (n==2 && SyStrnicmp(z,"on",2)==0) \|\| (n==3 && SyStrnicmp(z,"yes",3)==0) ){` |
|     11 | 5786 | `		*pBool = 1; return 1;` |
|      - | 5787 | `	}` |
|     22 | 5788 | `	if( n==0 \|\| (n==1 && z[0]=='0') \|\| (n==5 && SyStrnicmp(z,"false",5)==0)` |
|     11 | 5789 | `	    \|\| (n==3 && SyStrnicmp(z,"off",3)==0) \|\| (n==2 && SyStrnicmp(z,"no",2)==0) ){` |
|     11 | 5790 | `		*pBool = 0; return 1;` |
|      - | 5791 | `	}` |
|      9 | 5792 | `	return 0;` |
|     15 | 5793 | `}` |
|      - | 5794 | `/* IPv4 dotted-quad: exactly 4 octets 0..255, no leading zeros. */` |
|     33 | 5795 | `static int FvValidateIp4(const char *z,int n){` |
|     33 | 5796 | `	int i = 0, parts = 0;` |
|     77 | 5797 | `	while( i<n ){` |
|     65 | 5798 | `		int val = 0, digits = 0, start = i;` |
|    143 | 5799 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){` |
|     85 | 5800 | `			val = val*10 + (z[i]-'0');` |
|     85 | 5801 | `			if( val>255 ){ return 0; }` |
|     79 | 5802 | `			digits++; i++;` |
|      1 | 5803 | `		}` |
|     59 | 5804 | `		if( digits==0 \|\| digits>3 ){ return 0; }` |
|     49 | 5805 | `		if( digits>1 && z[start]=='0' ){ return 0; } /* leading zero */` |
|     45 | 5806 | `		parts++;` |
|     45 | 5807 | `		if( parts>4 ){ return 0; }` |
|     45 | 5808 | `		if( i<n ){` |
|     33 | 5809 | `			if( z[i]!='.' ){ return 0; }` |
|     33 | 5810 | `			i++;` |
|     33 | 5811 | `			if( i>=n ){ return 0; } /* trailing dot */` |
|     16 | 5812 | `		}` |
|      1 | 5813 | `	}` |
|     13 | 5814 | `	return parts==4;` |
|     17 | 5815 | `}` |
|      - | 5816 | `/* A colon-separated run of IPv6 hextets with no "::" (n may be 0 -> 0 groups),` |
|      - | 5817 | ` * allowing a trailing embedded IPv4. Returns the 16-bit group count or -1. */` |
|     19 | 5818 | `static int FvIp6Hextets(const char *z,int n){` |
|     19 | 5819 | `	int i = 0, segStart = 0, groups = 0;` |
|     19 | 5820 | `	if( n==0 ){ return 0; }` |
|    145 | 5821 | `	while( i<=n ){` |
|    133 | 5822 | `		if( i==n \|\| z[i]==':' ){` |
|     23 | 5823 | `			int segLen = i - segStart, j, isV4 = 0;` |
|     23 | 5824 | `			if( segLen==0 ){ return -1; } /* an empty hextet (stray ':') */` |
|     77 | 5825 | `			for( j=segStart; j<i; j++ ){ if( z[j]=='.' ){ isV4 = 1; break; } }` |
|     23 | 5826 | `			if( isV4 ){` |
|     11 | 5827 | `				if( i!=n ){ return -1; } /* IPv4 only as the final token */` |
|     11 | 5828 | `				if( !FvValidateIp4(z+segStart,segLen) ){ return -1; }` |
|      5 | 5829 | `				groups += 2;` |
|      3 | 5830 | `			}else{` |
|     13 | 5831 | `				if( segLen>4 ){ return -1; }` |
|     47 | 5832 | `				for( j=segStart; j<i; j++ ){ if( SyHexToint((unsigned char)z[j])<0 ){ return -1; } }` |
|     13 | 5833 | `				groups++;` |
|      - | 5834 | `			}` |
|     17 | 5835 | `			segStart = i+1;` |
|      8 | 5836 | `		}` |
|    127 | 5837 | `		i++;` |
|      1 | 5838 | `	}` |
|     13 | 5839 | `	return groups;` |
|     10 | 5840 | `}` |
|      - | 5841 | `/* IPv6: at most one "::" zero-run; 8 groups exactly, or fewer when "::" present. */` |
|     19 | 5842 | `static int FvValidateIp6(const char *z,int n){` |
|     19 | 5843 | `	const char *zDbl = 0;` |
|      - | 5844 | `	int i, ga, gb;` |
|    139 | 5845 | `	for( i=0; i+1<n; i++ ){` |
|    123 | 5846 | `		if( z[i]==':' && z[i+1]==':' ){` |
|     13 | 5847 | `			if( zDbl ){ return 0; } /* a second "::" is invalid */` |
|     11 | 5848 | `			zDbl = z+i;` |
|      5 | 5849 | `		}` |
|     61 | 5850 | `	}` |
|     17 | 5851 | `	if( zDbl==0 ){` |
|      9 | 5852 | `		return FvIp6Hextets(z,n)==8;` |
|    ! 0 | 5853 | `	}else{` |
|      9 | 5854 | `		int lenA = (int)(zDbl - z);` |
|      9 | 5855 | `		int lenB = n - lenA - 2;` |
|      9 | 5856 | `		ga = (lenA==0) ? 0 : FvIp6Hextets(z,lenA);` |
|      9 | 5857 | `		gb = (lenB==0) ? 0 : FvIp6Hextets(zDbl+2,lenB);` |
|      9 | 5858 | `		if( ga<0 \|\| gb<0 ){ return 0; }` |
|      9 | 5859 | `		return (ga+gb)<=7; /* "::" stands for at least one zero group */` |
|      - | 5860 | `	}` |
|     10 | 5861 | `}` |
|     25 | 5862 | `static int FvValidateIp(const char *z,int n,int flags){` |
|     25 | 5863 | `	int v4 = (flags & FV_FLAG_IPV4), v6 = (flags & FV_FLAG_IPV6);` |
|     25 | 5864 | `	if( !v4 && !v6 ){ v4 = v6 = 1; } /* default accepts either family */` |
|     25 | 5865 | `	if( v4 && FvValidateIp4(z,n) ){ return 1; }` |
|     21 | 5866 | `	if( v6 && FvValidateIp6(z,n) ){ return 1; }` |
|     13 | 5867 | `	return 0;` |
|     13 | 5868 | `}` |
|      - | 5869 | `/* FILTER_VALIDATE_MAC: 17-char colon- or dash-separated hex (XX:XX:..:XX). */` |
|     11 | 5870 | `static int FvValidateMac(const char *z,int n){` |
|      - | 5871 | `	char sep;` |
|      - | 5872 | `	int i;` |
|     11 | 5873 | `	if( n!=17 ){ return 0; }` |
|      7 | 5874 | `	sep = z[2];` |
|      7 | 5875 | `	if( sep!=':' && sep!='-' ){ return 0; }` |
|    105 | 5876 | `	for( i=0; i<17; i++ ){` |
|    101 | 5877 | `		if( (i%3)==2 ){ if( z[i]!=sep ){ return 0; } }` |
|     71 | 5878 | `		else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }` |
|     50 | 5879 | `	}` |
|      5 | 5880 | `	return 1;` |
|      6 | 5881 | `}` |
|      - | 5882 | `/* FILTER_VALIDATE_EMAIL (best-effort: covers the common cases, not quoted local` |
|      - | 5883 | ` * parts or IP-literal domains). */` |
|     28 | 5884 | `static int FvValidateEmail(const char *z,int n){` |
|     28 | 5885 | `	int at = -1, i, localLen, domLen, labelStart, dotCount = 0;` |
|      - | 5886 | `	const char *zDom;` |
|     28 | 5887 | `	if( n==0 \|\| n>320 ){ return 0; }` |
|    201 | 5888 | `	for( i=0; i<n; i++ ){` |
|    181 | 5889 | `		if( z[i]=='@' ){ if( at>=0 ){ return 0; } at = i; }` |
|     91 | 5890 | `	}` |
|     21 | 5891 | `	if( at<=0 \|\| at==n-1 ){ return 0; } /* one '@', non-empty local and domain */` |
|     21 | 5892 | `	localLen = at;` |
|     21 | 5893 | `	zDom = z + at + 1;` |
|     21 | 5894 | `	domLen = n - at - 1;` |
|     21 | 5895 | `	if( z[0]=='.' \|\| z[at-1]=='.' ){ return 0; }` |
|     57 | 5896 | `	for( i=0; i<localLen; i++ ){` |
|     43 | 5897 | `		unsigned char c = (unsigned char)z[i];` |
|     43 | 5898 | `		if( c<=' ' ){ return 0; }` |
|     41 | 5899 | `		if( c=='.' && i+1<localLen && z[i+1]=='.' ){ return 0; }` |
|     20 | 5900 | `	}` |
|     15 | 5901 | `	if( zDom[0]=='.' \|\| zDom[domLen-1]=='.' ){ return 0; }` |
|     13 | 5902 | `	labelStart = 0;` |
|     85 | 5903 | `	for( i=0; i<=domLen; i++ ){` |
|     75 | 5904 | `		if( i==domLen \|\| zDom[i]=='.' ){` |
|     25 | 5905 | `			int ll = i - labelStart;` |
|     25 | 5906 | `			if( ll==0 ){ return 0; } /* consecutive dots */` |
|     23 | 5907 | `			if( zDom[labelStart]=='-' \|\| zDom[i-1]=='-' ){ return 0; }` |
|     23 | 5908 | `			if( i<domLen ){ dotCount++; }` |
|     23 | 5909 | `			labelStart = i+1;` |
|     12 | 5910 | `		}else{` |
|     51 | 5911 | `			unsigned char c = (unsigned char)zDom[i];` |
|     51 | 5912 | `			if( !((c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9')\|\|c=='-') ){ return 0; }` |
|      - | 5913 | `		}` |
|     37 | 5914 | `	}` |
|     11 | 5915 | `	if( dotCount<1 ){ return 0; } /* PHP requires a dot in the domain (any TLD length) */` |
|      9 | 5916 | `	return 1;` |
|     15 | 5917 | `}` |
|      - | 5918 | `/* FILTER_VALIDATE_DOMAIN (lenient, matching PHP without FILTER_FLAG_HOSTNAME). */` |
|     11 | 5919 | `static int FvValidateDomain(const char *z,int n){` |
|      - | 5920 | `	int i;` |
|     11 | 5921 | `	if( n<1 \|\| n>253 \|\| z[0]=='.' ){ return 0; }` |
|     81 | 5922 | `	for( i=0; i<n; i++ ){` |
|     75 | 5923 | `		unsigned char c = (unsigned char)z[i];` |
|     75 | 5924 | `		if( c<=' ' ){ return 0; }` |
|     75 | 5925 | `		if( c=='.' && i+1<n && z[i+1]=='.' ){ return 0; }` |
|     37 | 5926 | `	}` |
|      7 | 5927 | `	return 1;` |
|      6 | 5928 | `}` |
|      - | 5929 | `/* FILTER_VALIDATE_URL: require a scheme and a host (PHP's filter is itself` |
|      - | 5930 | ` * parse_url-based, so PH7_VmHttpSplitURI tracks it closely). */` |
|     15 | 5931 | `static int FvValidateUrl(const char *z,int n){` |
|      - | 5932 | `	SyhttpUri sUri;` |
|     15 | 5933 | `	if( n==0 ){ return 0; }` |
|     15 | 5934 | `	SyZero(&sUri,(sxu32)sizeof(sUri));` |
|     15 | 5935 | `	if( PH7_VmHttpSplitURI(&sUri,z,(sxu32)n)!=SXRET_OK ){ return 0; }` |
|     15 | 5936 | `	return sUri.sScheme.nByte!=0 && sUri.sHost.nByte!=0;` |
|      8 | 5937 | `}` |
|      - | 5938 | `/* The Fv sanitizers build their result by appending directly to the call` |
|      - | 5939 | ` * context (ph7_result_string accumulates, like htmlspecialchars), emitting each` |
|      - | 5940 | ` * kept run in one call and seeding "" so an all-stripped input yields "". */` |
|      - | 5941 | `/* SANITIZE_NUMBER_INT (isFloat=0) / SANITIZE_NUMBER_FLOAT (isFloat=1). */` |
|     37 | 5942 | `static void FvSanitizeNumber(ph7_context *pCtx,const char *z,int n,int isFloat,int flags){` |
|     37 | 5943 | `	int i, runStart = 0;` |
|     37 | 5944 | `	ph7_result_string(pCtx,"",0);` |
|     97 | 5945 | `	for( i=0; i<n; i++ ){` |
|     91 | 5946 | `		char c = z[i];` |
|     91 | 5947 | `		int keep = (c>='0'&&c<='9') \|\| c=='+' \|\| c=='-';` |
|     91 | 5948 | `		if( !keep && isFloat ){` |
|     38 | 5949 | `			keep = (c=='.' && (flags & FV_FLAG_ALLOW_FRACTION))` |
|     23 | 5950 | `			    \|\| (c==',' && (flags & FV_FLAG_ALLOW_THOUSAND))` |
|     36 | 5951 | `			    \|\| ((c=='e'\|\|c=='E') && (flags & FV_FLAG_ALLOW_SCIENTIFIC));` |
|     12 | 5952 | `		}` |
|     61 | 5953 | `		if( !keep ){` |
|     33 | 5954 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     33 | 5955 | `			runStart = i+1;` |
|     16 | 5956 | `		}` |
|     31 | 5957 | `	}` |
|      7 | 5958 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 | 5959 | `}` |
|      - | 5960 | `/* Return non-zero when byte c must be stripped under the STRIP_* flags. Shared` |
|      - | 5961 | ` * by the UNSAFE_RAW string filter and SANITIZE_SPECIAL_CHARS. STRIP_LOW drops` |
|      - | 5962 | `` * bytes <32, STRIP_HIGH drops bytes >=127 (incl. DEL), STRIP_BACKTICK drops '`'.`` |
|      - | 5963 | ` * Matches php_filter_strip(); verified byte-exact vs php 8.5.7. */` |
|    287 | 5964 | `static int FvStripByte(unsigned char c,int flags){` |
|    287 | 5965 | `	if( (flags & FV_FLAG_STRIP_LOW)      && c<32 )    { return 1; }` |
|    281 | 5966 | `	if( (flags & FV_FLAG_STRIP_HIGH)     && c>=127 )  { return 1; }` |
|    269 | 5967 | `	if( (flags & FV_FLAG_STRIP_BACKTICK) && c==0x60 ) { return 1; }` |
|    267 | 5968 | `	return 0;` |
|    144 | 5969 | `}` |
|      - | 5970 | `/* FILTER_UNSAFE_RAW / FILTER_DEFAULT with flags: no default transform, but the` |
|      - | 5971 | ` * STRIP/ENCODE flags apply. Precedence (per php_filter_unsafe_raw, verified` |
|      - | 5972 | ` * vs php 8.5.7): a byte is first tested for stripping; a surviving byte is then` |
|      - | 5973 | ` * encoded as a decimal numeric entity if ENCODE_LOW (<32) / ENCODE_HIGH (>=127)` |
|      - | 5974 | ` * is set, and '&' becomes "&#38;" under ENCODE_AMP. So STRIP_LOW\|ENCODE_LOW` |
|      - | 5975 | ` * strips (nothing left to encode). Bytes are treated individually — ENCODE_HIGH` |
|      - | 5976 | ` * numeric-encodes each byte of a multibyte sequence separately, not the codepoint. */` |
|     25 | 5977 | `static void FvSanitizeString(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 5978 | `	int i, runStart = 0;` |
|     25 | 5979 | `	ph7_result_string(pCtx,"",0);` |
|    193 | 5980 | `	for( i=0; i<n; i++ ){` |
|    179 | 5981 | `		unsigned char c = (unsigned char)z[i];` |
|    179 | 5982 | `		if( FvStripByte(c,flags) ){` |
|     13 | 5983 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     13 | 5984 | `			runStart = i+1;` |
|     13 | 5985 | `			continue;` |
|      - | 5986 | `		}` |
|    167 | 5987 | `		if( c=='&' && (flags & FV_FLAG_ENCODE_AMP) ){` |
|      3 | 5988 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      3 | 5989 | `			ph7_result_string(pCtx,"&#38;",-1);` |
|      3 | 5990 | `			runStart = i+1;` |
|    166 | 5991 | `		}else if( (c<32 && (flags & FV_FLAG_ENCODE_LOW))` |
|    164 | 5992 | `		       \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     37 | 5993 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 5994 | `			ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|      9 | 5995 | `			runStart = i+1;` |
|      4 | 5996 | `		}` |
|     79 | 5997 | `	}` |
|     15 | 5998 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     15 | 5999 | `}` |
|      - | 6000 | `/* FILTER_SANITIZE_SPECIAL_CHARS: encode <>&"' and every control byte <32 as a` |
|      - | 6001 | ` * decimal numeric entity (&#60; &#38; &#34; ...). The STRIP_* flags remove bytes` |
|      - | 6002 | ` * before encoding; ENCODE_HIGH numeric-encodes surviving bytes >=127. Bytes >=128` |
|      - | 6003 | ` * are otherwise passed through verbatim (this filter is NOT UTF-8-aware — only the` |
|      - | 6004 | ` * FULL variant is). Byte-exact vs php 8.5.7. */` |
|     13 | 6005 | `static void FvSanitizeSpecial(ph7_context *pCtx,const char *z,int n,int flags){` |
|     13 | 6006 | `	int i, runStart = 0;` |
|      - | 6007 | `	const char *zEnt;` |
|     13 | 6008 | `	ph7_result_string(pCtx,"",0);` |
|    131 | 6009 | `	for( i=0; i<n; i++ ){` |
|    119 | 6010 | `		unsigned char c = (unsigned char)z[i];` |
|    119 | 6011 | `		if( FvStripByte(c,flags) ){` |
|      9 | 6012 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 6013 | `			runStart = i+1;` |
|      9 | 6014 | `			continue;` |
|      - | 6015 | `		}` |
|    111 | 6016 | `		switch( c ){` |
|      3 | 6017 | `		case '<':  zEnt = "&#60;"; break;` |
|      3 | 6018 | `		case '>':  zEnt = "&#62;"; break;` |
|     11 | 6019 | `		case '&':  zEnt = "&#38;"; break;` |
|      3 | 6020 | `		case '"':  zEnt = "&#34;"; break;` |
|      3 | 6021 | `		case '\'': zEnt = "&#39;"; break;` |
|     46 | 6022 | `		default:` |
|      - | 6023 | `			/* Control bytes <32 are always numeric-encoded; bytes >=127 only when` |
|      - | 6024 | `			 * ENCODE_HIGH is set. Everything else stays in the current run. */` |
|     93 | 6025 | `			if( c<32 \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     17 | 6026 | `				if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     17 | 6027 | `				ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|     17 | 6028 | `				runStart = i+1;` |
|      8 | 6029 | `			}` |
|     93 | 6030 | `			continue; /* keep in the current run */` |
|      - | 6031 | `		}` |
|     19 | 6032 | `		if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     19 | 6033 | `		ph7_result_string(pCtx,zEnt,-1); /* -1: length from strlen */` |
|     19 | 6034 | `		runStart = i+1;` |
|     10 | 6035 | `	}` |
|     13 | 6036 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     13 | 6037 | `}` |
|      - | 6038 | `/* HTML 4.01 named-entity table (codepoint -> "&name;") used by the UTF-8-aware` |
|      - | 6039 | ` * FULL_SPECIAL_CHARS filter, sorted ascending by codepoint for binary search.` |
|      - | 6040 | ` * Generated from php 8.5.7 (the exact set php_escape_html_entities emits for the` |
|      - | 6041 | ` * default document type); the five inline specials <>&"' are handled separately,` |
|      - | 6042 | ` * so every entry here is a codepoint >=0xA0. 248 rows. */` |
|      - | 6043 | `static const struct { sxu32 cp; const char *zEnt; } aHtml401Ent[] = {` |
|      - | 6044 | `	{0x00A0,"&nbsp;"},{0x00A1,"&iexcl;"},{0x00A2,"&cent;"},{0x00A3,"&pound;"},` |
|      - | 6045 | `	{0x00A4,"&curren;"},{0x00A5,"&yen;"},{0x00A6,"&brvbar;"},{0x00A7,"&sect;"},` |
|      - | 6046 | `	{0x00A8,"&uml;"},{0x00A9,"&copy;"},{0x00AA,"&ordf;"},{0x00AB,"&laquo;"},` |
|      - | 6047 | `	{0x00AC,"&not;"},{0x00AD,"&shy;"},{0x00AE,"&reg;"},{0x00AF,"&macr;"},` |
|      - | 6048 | `	{0x00B0,"&deg;"},{0x00B1,"&plusmn;"},{0x00B2,"&sup2;"},{0x00B3,"&sup3;"},` |
|      - | 6049 | `	{0x00B4,"&acute;"},{0x00B5,"&micro;"},{0x00B6,"&para;"},{0x00B7,"&middot;"},` |
|      - | 6050 | `	{0x00B8,"&cedil;"},{0x00B9,"&sup1;"},{0x00BA,"&ordm;"},{0x00BB,"&raquo;"},` |
|      - | 6051 | `	{0x00BC,"&frac14;"},{0x00BD,"&frac12;"},{0x00BE,"&frac34;"},{0x00BF,"&iquest;"},` |
|      - | 6052 | `	{0x00C0,"&Agrave;"},{0x00C1,"&Aacute;"},{0x00C2,"&Acirc;"},{0x00C3,"&Atilde;"},` |
|      - | 6053 | `	{0x00C4,"&Auml;"},{0x00C5,"&Aring;"},{0x00C6,"&AElig;"},{0x00C7,"&Ccedil;"},` |
|      - | 6054 | `	{0x00C8,"&Egrave;"},{0x00C9,"&Eacute;"},{0x00CA,"&Ecirc;"},{0x00CB,"&Euml;"},` |
|      - | 6055 | `	{0x00CC,"&Igrave;"},{0x00CD,"&Iacute;"},{0x00CE,"&Icirc;"},{0x00CF,"&Iuml;"},` |
|      - | 6056 | `	{0x00D0,"&ETH;"},{0x00D1,"&Ntilde;"},{0x00D2,"&Ograve;"},{0x00D3,"&Oacute;"},` |
|      - | 6057 | `	{0x00D4,"&Ocirc;"},{0x00D5,"&Otilde;"},{0x00D6,"&Ouml;"},{0x00D7,"&times;"},` |
|      - | 6058 | `	{0x00D8,"&Oslash;"},{0x00D9,"&Ugrave;"},{0x00DA,"&Uacute;"},{0x00DB,"&Ucirc;"},` |
|      - | 6059 | `	{0x00DC,"&Uuml;"},{0x00DD,"&Yacute;"},{0x00DE,"&THORN;"},{0x00DF,"&szlig;"},` |
|      - | 6060 | `	{0x00E0,"&agrave;"},{0x00E1,"&aacute;"},{0x00E2,"&acirc;"},{0x00E3,"&atilde;"},` |
|      - | 6061 | `	{0x00E4,"&auml;"},{0x00E5,"&aring;"},{0x00E6,"&aelig;"},{0x00E7,"&ccedil;"},` |
|      - | 6062 | `	{0x00E8,"&egrave;"},{0x00E9,"&eacute;"},{0x00EA,"&ecirc;"},{0x00EB,"&euml;"},` |
|      - | 6063 | `	{0x00EC,"&igrave;"},{0x00ED,"&iacute;"},{0x00EE,"&icirc;"},{0x00EF,"&iuml;"},` |
|      - | 6064 | `	{0x00F0,"&eth;"},{0x00F1,"&ntilde;"},{0x00F2,"&ograve;"},{0x00F3,"&oacute;"},` |
|      - | 6065 | `	{0x00F4,"&ocirc;"},{0x00F5,"&otilde;"},{0x00F6,"&ouml;"},{0x00F7,"&divide;"},` |
|      - | 6066 | `	{0x00F8,"&oslash;"},{0x00F9,"&ugrave;"},{0x00FA,"&uacute;"},{0x00FB,"&ucirc;"},` |
|      - | 6067 | `	{0x00FC,"&uuml;"},{0x00FD,"&yacute;"},{0x00FE,"&thorn;"},{0x00FF,"&yuml;"},` |
|      - | 6068 | `	{0x0152,"&OElig;"},{0x0153,"&oelig;"},{0x0160,"&Scaron;"},{0x0161,"&scaron;"},` |
|      - | 6069 | `	{0x0178,"&Yuml;"},{0x0192,"&fnof;"},{0x02C6,"&circ;"},{0x02DC,"&tilde;"},` |
|      - | 6070 | `	{0x0391,"&Alpha;"},{0x0392,"&Beta;"},{0x0393,"&Gamma;"},{0x0394,"&Delta;"},` |
|      - | 6071 | `	{0x0395,"&Epsilon;"},{0x0396,"&Zeta;"},{0x0397,"&Eta;"},{0x0398,"&Theta;"},` |
|      - | 6072 | `	{0x0399,"&Iota;"},{0x039A,"&Kappa;"},{0x039B,"&Lambda;"},{0x039C,"&Mu;"},` |
|      - | 6073 | `	{0x039D,"&Nu;"},{0x039E,"&Xi;"},{0x039F,"&Omicron;"},{0x03A0,"&Pi;"},` |
|      - | 6074 | `	{0x03A1,"&Rho;"},{0x03A3,"&Sigma;"},{0x03A4,"&Tau;"},{0x03A5,"&Upsilon;"},` |
|      - | 6075 | `	{0x03A6,"&Phi;"},{0x03A7,"&Chi;"},{0x03A8,"&Psi;"},{0x03A9,"&Omega;"},` |
|      - | 6076 | `	{0x03B1,"&alpha;"},{0x03B2,"&beta;"},{0x03B3,"&gamma;"},{0x03B4,"&delta;"},` |
|      - | 6077 | `	{0x03B5,"&epsilon;"},{0x03B6,"&zeta;"},{0x03B7,"&eta;"},{0x03B8,"&theta;"},` |
|      - | 6078 | `	{0x03B9,"&iota;"},{0x03BA,"&kappa;"},{0x03BB,"&lambda;"},{0x03BC,"&mu;"},` |
|      - | 6079 | `	{0x03BD,"&nu;"},{0x03BE,"&xi;"},{0x03BF,"&omicron;"},{0x03C0,"&pi;"},` |
|      - | 6080 | `	{0x03C1,"&rho;"},{0x03C2,"&sigmaf;"},{0x03C3,"&sigma;"},{0x03C4,"&tau;"},` |
|      - | 6081 | `	{0x03C5,"&upsilon;"},{0x03C6,"&phi;"},{0x03C7,"&chi;"},{0x03C8,"&psi;"},` |
|      - | 6082 | `	{0x03C9,"&omega;"},{0x03D1,"&thetasym;"},{0x03D2,"&upsih;"},{0x03D6,"&piv;"},` |
|      - | 6083 | `	{0x2002,"&ensp;"},{0x2003,"&emsp;"},{0x2009,"&thinsp;"},{0x200C,"&zwnj;"},` |
|      - | 6084 | `	{0x200D,"&zwj;"},{0x200E,"&lrm;"},{0x200F,"&rlm;"},{0x2013,"&ndash;"},` |
|      - | 6085 | `	{0x2014,"&mdash;"},{0x2018,"&lsquo;"},{0x2019,"&rsquo;"},{0x201A,"&sbquo;"},` |
|      - | 6086 | `	{0x201C,"&ldquo;"},{0x201D,"&rdquo;"},{0x201E,"&bdquo;"},{0x2020,"&dagger;"},` |
|      - | 6087 | `	{0x2021,"&Dagger;"},{0x2022,"&bull;"},{0x2026,"&hellip;"},{0x2030,"&permil;"},` |
|      - | 6088 | `	{0x2032,"&prime;"},{0x2033,"&Prime;"},{0x2039,"&lsaquo;"},{0x203A,"&rsaquo;"},` |
|      - | 6089 | `	{0x203E,"&oline;"},{0x2044,"&frasl;"},{0x20AC,"&euro;"},{0x2111,"&image;"},` |
|      - | 6090 | `	{0x2118,"&weierp;"},{0x211C,"&real;"},{0x2122,"&trade;"},{0x2135,"&alefsym;"},` |
|      - | 6091 | `	{0x2190,"&larr;"},{0x2191,"&uarr;"},{0x2192,"&rarr;"},{0x2193,"&darr;"},` |
|      - | 6092 | `	{0x2194,"&harr;"},{0x21B5,"&crarr;"},{0x21D0,"&lArr;"},{0x21D1,"&uArr;"},` |
|      - | 6093 | `	{0x21D2,"&rArr;"},{0x21D3,"&dArr;"},{0x21D4,"&hArr;"},{0x2200,"&forall;"},` |
|      - | 6094 | `	{0x2202,"&part;"},{0x2203,"&exist;"},{0x2205,"&empty;"},{0x2207,"&nabla;"},` |
|      - | 6095 | `	{0x2208,"&isin;"},{0x2209,"&notin;"},{0x220B,"&ni;"},{0x220F,"&prod;"},` |
|      - | 6096 | `	{0x2211,"&sum;"},{0x2212,"&minus;"},{0x2217,"&lowast;"},{0x221A,"&radic;"},` |
|      - | 6097 | `	{0x221D,"&prop;"},{0x221E,"&infin;"},{0x2220,"&ang;"},{0x2227,"&and;"},` |
|      - | 6098 | `	{0x2228,"&or;"},{0x2229,"&cap;"},{0x222A,"&cup;"},{0x222B,"&int;"},` |
|      - | 6099 | `	{0x2234,"&there4;"},{0x223C,"&sim;"},{0x2245,"&cong;"},{0x2248,"&asymp;"},` |
|      - | 6100 | `	{0x2260,"&ne;"},{0x2261,"&equiv;"},{0x2264,"&le;"},{0x2265,"&ge;"},` |
|      - | 6101 | `	{0x2282,"&sub;"},{0x2283,"&sup;"},{0x2284,"&nsub;"},{0x2286,"&sube;"},` |
|      - | 6102 | `	{0x2287,"&supe;"},{0x2295,"&oplus;"},{0x2297,"&otimes;"},{0x22A5,"&perp;"},` |
|      - | 6103 | `	{0x22C5,"&sdot;"},{0x2308,"&lceil;"},{0x2309,"&rceil;"},{0x230A,"&lfloor;"},` |
|      - | 6104 | `	{0x230B,"&rfloor;"},{0x2329,"&lang;"},{0x232A,"&rang;"},{0x25CA,"&loz;"},` |
|      - | 6105 | `	{0x2660,"&spades;"},{0x2663,"&clubs;"},{0x2665,"&hearts;"},{0x2666,"&diams;"}` |
|      - | 6106 | `};` |
|      - | 6107 | `/* Binary-search aHtml401Ent[] for cp; return its "&name;" entity or 0. */` |
|     41 | 6108 | `static const char *FvHtml401Lookup(sxu32 cp){` |
|     41 | 6109 | `	int lo = 0, hi = (int)SX_ARRAYSIZE(aHtml401Ent) - 1;` |
|    323 | 6110 | `	while( lo <= hi ){` |
|    309 | 6111 | `		int mid = (lo + hi) / 2;` |
|    309 | 6112 | `		sxu32 c = aHtml401Ent[mid].cp;` |
|    309 | 6113 | `		if( c == cp ){ return aHtml401Ent[mid].zEnt; }` |
|    283 | 6114 | `		if( c < cp ){ lo = mid + 1; } else { hi = mid - 1; }` |
|      1 | 6115 | `	}` |
|     15 | 6116 | `	return 0;` |
|     21 | 6117 | `}` |
|      - | 6118 | `/* Decode one strict-UTF-8 sequence at p (< zEnd). On success returns its byte` |
|      - | 6119 | ` * length (1..4) and sets *pCp to the codepoint; on any malformed, overlong,` |
|      - | 6120 | ` * surrogate, truncated or out-of-range (>U+10FFFF) sequence returns 0. Matches` |
|      - | 6121 | ` * PHP's UTF-8 validation used by FULL_SPECIAL_CHARS (verified vs php 8.5.7). */` |
|    101 | 6122 | `static int FvUtf8Next(const unsigned char *p,const unsigned char *zEnd,sxu32 *pCp){` |
|    101 | 6123 | `	unsigned char c = p[0];` |
|    101 | 6124 | `	if( c < 0x80 ){ *pCp = c; return 1; }` |
|    101 | 6125 | `	if( c < 0xC2 ){ return 0; }              /* 0x80-0xBF stray cont / 0xC0-0xC1 overlong */` |
|     99 | 6126 | `	if( c < 0xE0 ){                          /* 2-byte: U+0080..U+07FF */` |
|     47 | 6127 | `		if( zEnd-p < 2 \|\| (p[1]&0xC0)!=0x80 ){ return 0; }` |
|     45 | 6128 | `		*pCp = ((sxu32)(c&0x1F)<<6) \| (p[1]&0x3F);` |
|     45 | 6129 | `		return 2;` |
|      - | 6130 | `	}` |
|     53 | 6131 | `	if( c < 0xF0 ){                          /* 3-byte: U+0800..U+FFFF minus surrogates */` |
|      - | 6132 | `		sxu32 cp;` |
|     47 | 6133 | `		if( zEnd-p < 3 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 ){ return 0; }` |
|     33 | 6134 | `		cp = ((sxu32)(c&0x0F)<<12) \| ((sxu32)(p[1]&0x3F)<<6) \| (p[2]&0x3F);` |
|     33 | 6135 | `		if( cp < 0x800 \|\| (cp>=0xD800 && cp<=0xDFFF) ){ return 0; }` |
|     29 | 6136 | `		*pCp = cp;` |
|     29 | 6137 | `		return 3;` |
|      - | 6138 | `	}` |
|      7 | 6139 | `	if( c < 0xF5 ){                          /* 4-byte: U+10000..U+10FFFF */` |
|      - | 6140 | `		sxu32 cp;` |
|      5 | 6141 | `		if( zEnd-p < 4 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 \|\| (p[3]&0xC0)!=0x80 ){ return 0; }` |
|      5 | 6142 | `		cp = ((sxu32)(c&0x07)<<18) \| ((sxu32)(p[1]&0x3F)<<12) \| ((sxu32)(p[2]&0x3F)<<6) \| (p[3]&0x3F);` |
|      5 | 6143 | `		if( cp < 0x10000 \|\| cp > 0x10FFFF ){ return 0; }` |
|      5 | 6144 | `		*pCp = cp;` |
|      5 | 6145 | `		return 4;` |
|      - | 6146 | `	}` |
|      3 | 6147 | `	return 0;                                /* 0xF5-0xFF */` |
|     51 | 6148 | `}` |
|      - | 6149 | `/* FILTER_SANITIZE_FULL_SPECIAL_CHARS: htmlentities-style, UTF-8-aware. Encodes` |
|      - | 6150 | ` * <>&"' as named entities ("'" -> &#039;; quotes suppressed under NO_ENCODE_QUOTES),` |
|      - | 6151 | ` * and every valid UTF-8 codepoint with an HTML 4.01 named entity as that entity;` |
|      - | 6152 | ` * valid codepoints without a named entity (and low control bytes) pass through` |
|      - | 6153 | ` * verbatim. If the input contains ANY invalid UTF-8 the whole result is "".` |
|      - | 6154 | ` * The STRIP/ENCODE flags do NOT apply to this filter (only NO_ENCODE_QUOTES).` |
|      - | 6155 | ` * php's filter does NOT re-encode valid pre-existing entities ("&amp;" stays,` |
|      - | 6156 | ` * "&bogus;" becomes "&amp;bogus;"), i.e. double_encode=false semantics —` |
|      - | 6157 | ` * exactly htmlentities(ENT_QUOTES\|ENT_HTML401, double_encode: false), so this` |
|      - | 6158 | ` * delegates to the shared encoder. Byte-exact vs php 8.5.7. */` |
|     25 | 6159 | `static void FvSanitizeFull(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 6160 | `	int iEntFlags = (flags & FV_FLAG_NO_ENCODE_QUOTES) ? 0 : PH7_ENT_QUOTES;` |
|     25 | 6161 | `	HtmlEscape(pCtx,z,n,iEntFlags,1/*bAll*/,0/*bDoubleEncode*/);` |
|     25 | 6162 | `}` |
|      - | 6163 | `/* ---------------------------------------------------------------------------` |
|      - | 6164 | ` * UTF-8-aware HTML entity core (htmlspecialchars/htmlentities family).` |
|      - | 6165 | ` * Prototyped next to the five builtins earlier in this file; lives here so it` |
|      - | 6166 | ` * can share aHtml401Ent[]/FvHtml401Lookup()/FvUtf8Next() with the filter_var` |
|      - | 6167 | ` * FULL_SPECIAL_CHARS filter above. Byte-exact vs php 8.5.7 (oracle-swept).` |
|      - | 6168 | ` * ------------------------------------------------------------------------ */` |
|      - | 6169 | `/* Encode cp as UTF-8 into zBuf (>= 4 bytes); return the byte length 1..4.` |
|      - | 6170 | ` * Thin wrapper over the engine-wide SX_WRITE_UTF8 (sxmacros.h). */` |
|    585 | 6171 | `static int HtmlCpUtf8(sxu32 cp,char *zBuf){` |
|    585 | 6172 | `	sxu8 *z = (sxu8 *)zBuf;` |
|    585 | 6173 | `	SX_WRITE_UTF8(z,cp);` |
|    585 | 6174 | `	return (int)(z - (sxu8 *)zBuf);` |
|      1 | 6175 | `}` |
|      - | 6176 | `/* Doctype-allowed codepoint test (php's unicode_cp_is_allowed) — gates what a` |
|      - | 6177 | ` * numeric reference may DECODE to. Oracle-pinned per doctype: HTML401` |
|      - | 6178 | ` * disallows C0 (except TAB/LF/CR) and DEL..U+009F; XML1 and XHTML share the` |
|      - | 6179 | ` * XML rules — DEL..U+009F allowed, U+FFFE/U+FFFF excluded; HTML5 swaps CR` |
|      - | 6180 | ` * for FF (0x0C) and excludes the noncharacters (U+FDD0..U+FDEF and every` |
|      - | 6181 | ` * U+xFFFE/U+xFFFF). Surrogates are disallowed everywhere. */` |
|     91 | 6182 | `static int HtmlCpAllowed(sxu32 cp,int iFlags){` |
|     91 | 6183 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|     91 | 6184 | `	if( cp==0x09 \|\| cp==0x0A ){ return 1; }` |
|     87 | 6185 | `	if( cp==0x0D ){ return iDoc != PH7_ENT_DOC_HTML5; }` |
|     85 | 6186 | `	if( cp==0x0C ){ return iDoc == PH7_ENT_DOC_HTML5; }` |
|     85 | 6187 | `	if( cp < 0x20 \|\| cp > 0x10FFFF ){ return 0; }` |
|     79 | 6188 | `	if( cp>=0xD800 && cp<=0xDFFF ){ return 0; }` |
|     77 | 6189 | `	if( cp>=0x7F && cp<=0x9F ){ return iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML; }` |
|     71 | 6190 | `	if( iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML ){` |
|    ! 0 | 6191 | `		return cp!=0xFFFE && cp!=0xFFFF;` |
|      - | 6192 | `	}` |
|     71 | 6193 | `	if( iDoc == PH7_ENT_DOC_HTML5 ){` |
|      9 | 6194 | `		if( cp>=0xFDD0 && cp<=0xFDEF ){ return 0; }` |
|      9 | 6195 | `		if( (cp & 0xFFFF) >= 0xFFFE ){ return 0; }` |
|      4 | 6196 | `	}` |
|     71 | 6197 | `	return 1;` |
|     46 | 6198 | `}` |
|      - | 6199 | `/* The ENT_DISALLOWED gate for RAW characters on the ENCODE side. Same as the` |
|      - | 6200 | ` * decode gate except CR under HTML5: php's encode-side unicode_cp_is_allowed` |
|      - | 6201 | ` * keeps a literal "\r" verbatim under ENT_HTML5\|ENT_DISALLOWED while the` |
|      - | 6202 | ` * decode side leaves "&#13;" un-decoded (oracle-pinned at flags 176). */` |
|      9 | 6203 | `static int HtmlCpAllowedEncode(sxu32 cp,int iFlags){` |
|      9 | 6204 | `	if( cp==0x0D && (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 ){ return 1; }` |
|      9 | 6205 | `	return HtmlCpAllowed(cp,iFlags);` |
|      5 | 6206 | `}` |
|      - | 6207 | `/* Numeric-reference validity for the double_encode=false "is this already a` |
|      - | 6208 | ` * valid entity" test — a MUCH looser predicate than the decode gate above:` |
|      - | 6209 | ` * any codepoint <= U+10FFFF is valid (controls and surrogates included, every` |
|      - | 6210 | ` * doctype). ENT_DISALLOWED re-tightens non-HTML401 doctypes to the decode` |
|      - | 6211 | ` * gate, except that HTML5 exempts surrogates. All oracle-pinned: &#0; and` |
|      - | 6212 | ` * &#xD800; stay verbatim at flags 11 and 139; flags -1 (HTML5+DISALLOWED)` |
|      - | 6213 | ` * re-encodes &#0; and &#x10FFFF; but still keeps &#xD800;; flags 144` |
|      - | 6214 | ` * (XML1+DISALLOWED) re-encodes &#xD800;. */` |
|      9 | 6215 | `static int HtmlNumericAllowed(sxu32 cp,int iFlags){` |
|      9 | 6216 | `	if( cp > 0x10FFFF ){ return 0; }` |
|      7 | 6217 | `	if( (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML401 ){ return 1; /* never tightened */ }` |
|    ! 0 | 6218 | `	if( (iFlags & PH7_ENT_DISALLOWED)` |
|    ! 0 | 6219 | `	 && !((iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 && cp>=0xD800 && cp<=0xDFFF)` |
|    ! 0 | 6220 | `	 && !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|    ! 0 | 6221 | `	return 1;` |
|      5 | 6222 | `}` |
|      - | 6223 | `/* How many bytes the malformed UTF-8 sequence at p consumes — php's` |
|      - | 6224 | ` * get_next_char failure step (one U+FFFD substitution / one ENT_IGNORE drop` |
|      - | 6225 | ` * per MAXIMAL invalid subpart, not per byte): a prefix-valid sequence eats` |
|      - | 6226 | ` * its continuation bytes ("\xE0\x80\xAF" is ONE unit) while a byte that could` |
|      - | 6227 | ` * start a new sequence is left for the next round. */` |
|      5 | 6228 | `static int HtmlUtf8Trail(unsigned char c){ return c>=0x80 && c<=0xBF; }` |
|     11 | 6229 | `static int HtmlUtf8Lead(unsigned char c){ return c<0x80 \|\| (c>=0xC2 && c<=0xF4); }` |
|     15 | 6230 | `static int HtmlUtf8FailAdvance(const unsigned char *p,const unsigned char *zEnd){` |
|     15 | 6231 | `	unsigned char c = p[0];` |
|     15 | 6232 | `	int nAvail = (int)(zEnd - p);` |
|     15 | 6233 | `	if( c < 0xC2 \|\| c > 0xF4 ){ return 1; } /* stray trail / C0-C1 / F5-FF */` |
|     13 | 6234 | `	if( c < 0xE0 ){` |
|      3 | 6235 | `		if( nAvail < 2 ){ return 1; }` |
|      3 | 6236 | `		return HtmlUtf8Lead(p[1]) ? 1 : 2;` |
|      - | 6237 | `	}` |
|     11 | 6238 | `	if( c < 0xF0 ){` |
|     11 | 6239 | `		if( nAvail >= 3 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) ){` |
|      3 | 6240 | `			return 3; /* complete but overlong/surrogate */` |
|      - | 6241 | `		}` |
|      9 | 6242 | `		if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 6243 | `		if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 6244 | `		return 3;` |
|      - | 6245 | `	}` |
|    ! 0 | 6246 | `	if( nAvail >= 4 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) && HtmlUtf8Trail(p[3]) ){` |
|    ! 0 | 6247 | `		return 4; /* complete but overlong / > U+10FFFF */` |
|      - | 6248 | `	}` |
|    ! 0 | 6249 | `	if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 6250 | `	if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 6251 | `	if( nAvail < 4 \|\| HtmlUtf8Lead(p[3]) ){ return 3; }` |
|    ! 0 | 6252 | `	return 4;` |
|      8 | 6253 | `}` |
|      - | 6254 | `/* The basic special entities, shared by named matching, the hsc_decode` |
|      - | 6255 | ` * numeric whitelist and the translation-table builder so the sets can never` |
|      - | 6256 | ` * drift apart. (&apos; is not an HTML 4.01 entity — doctype-gated below.) */` |
|      - | 6257 | `static const struct { const char *zEnt; int n; sxu32 cp; } aHtmlSpecEnt[] = {` |
|      - | 6258 | `	{"&amp;",5,38},{"&lt;",4,60},{"&gt;",4,62},{"&quot;",6,34},{"&apos;",6,39}` |
|      - | 6259 | `};` |
|      - | 6260 | `/* Does this doctype consult the named-entity table (aHtml401Ent)? XML 1.0 has` |
|      - | 6261 | ` * no named entities beyond the specials; XHTML/HTML5 are approximated by the` |
|      - | 6262 | ` * HTML 4.01 table (documented divergence). */` |
|     63 | 6263 | `static int HtmlDocHasNamedTable(int iDoc){` |
|     63 | 6264 | `	return iDoc != PH7_ENT_DOC_XML1;` |
|      1 | 6265 | `}` |
|      - | 6266 | `/* The single-quote entity per doctype. Oracle-pinned asymmetry: for every` |
|      - | 6267 | ` * non-HTML401 doctype htmlspecialchars emits &apos; while htmlentities` |
|      - | 6268 | ` * (bEntities) keeps &#039; under XHTML too. The translation table mirrors` |
|      - | 6269 | ` * whichever function the requested table belongs to. */` |
|     29 | 6270 | `static const char *HtmlAposEntity(int iDoc,int bEntities){` |
|     29 | 6271 | `	if( iDoc == PH7_ENT_DOC_HTML401 \|\| (bEntities && iDoc == PH7_ENT_DOC_XHTML) ){` |
|     21 | 6272 | `		return "&#039;";` |
|      - | 6273 | `	}` |
|      9 | 6274 | `	return "&apos;";` |
|     15 | 6275 | `}` |
|      - | 6276 | `/* Try to parse one HTML entity at z (z[0]=='&', z < zEnd). bFull selects the` |
|      - | 6277 | ` * html_entity_decode set (doctype named table + any allowed numeric ref) vs` |
|      - | 6278 | ` * the htmlspecialchars_decode set (the basic specials + quote numerics only).` |
|      - | 6279 | ` * Named matching is case-SENSITIVE and the ';' is required (both PHP-exact);` |
|      - | 6280 | ` * numeric refs accept dec/hex (x or X) with any number of leading zeros but` |
|      - | 6281 | ` * reject out-of-range, surrogate and doctype-disallowed codepoints (the` |
|      - | 6282 | ` * caller then leaves the source verbatim). Quote-flag gating is NOT applied` |
|      - | 6283 | ` * here — the same routine doubles as the "is this a valid entity" test for` |
|      - | 6284 | ` * double_encode=false, which ignores the quote bits (oracle-pinned).` |
|      - | 6285 | ` * bEncodeCheck selects the looser HtmlNumericAllowed predicate used by that` |
|      - | 6286 | ` * double_encode test; decode callers pass 0 for the HtmlCpAllowed gate.` |
|      - | 6287 | ` * On success sets *pCp / *pnConsumed and returns 1. */` |
|    172 | 6288 | `static int HtmlParseEntity(const unsigned char *z,const unsigned char *zEnd,` |
|      1 | 6289 | `                           int iFlags,int bFull,int bEncodeCheck,sxu32 *pCp,int *pnConsumed){` |
|    173 | 6290 | `	int nAvail = (int)(zEnd - z);` |
|    173 | 6291 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6292 | `	sxu32 n;` |
|    173 | 6293 | `	if( nAvail < 4 ){ return 0; } /* shortest entities: &lt; &#9; */` |
|    169 | 6294 | `	if( z[1] == '#' ){` |
|      - | 6295 | `		/* Numeric reference */` |
|     89 | 6296 | `		sxu32 cp = 0;` |
|     89 | 6297 | `		int i = 2, bHex = 0, nDig = 0;` |
|     89 | 6298 | `		if( z[i]=='x' \|\| z[i]=='X' ){ bHex = 1; i++; }` |
|    317 | 6299 | `		for( ; i < nAvail && z[i] != ';' ; i++ ){` |
|      - | 6300 | `			int v;` |
|    221 | 6301 | `			unsigned char c = z[i];` |
|    221 | 6302 | `			if( c>='0' && c<='9' ){ v = c - '0'; }` |
|     17 | 6303 | `			else if( bHex && c>='a' && c<='f' ){ v = c - 'a' + 10; }` |
|     17 | 6304 | `			else if( bHex && c>='A' && c<='F' ){ v = c - 'A' + 10; }` |
|    ! 0 | 6305 | `			else { return 0; }` |
|      - | 6306 | `			/* Stop accumulating once out of range (keeps validating the shape;` |
|      - | 6307 | `			 * max intermediate is 0x10FFFF*16+15, no sxu32 overflow). */` |
|    221 | 6308 | `			if( cp <= 0x10FFFF ){ cp = cp * (bHex ? 16 : 10) + (sxu32)v; }` |
|    221 | 6309 | `			nDig++;` |
|    111 | 6310 | `		}` |
|     97 | 6311 | `		if( nDig == 0 \|\| i >= nAvail ){ return 0; } /* no digits / no ';' */` |
|     97 | 6312 | `		if( bEncodeCheck ? !HtmlNumericAllowed(cp,iFlags) : !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|     83 | 6313 | `		if( !bFull ){` |
|      - | 6314 | `			/* hsc_decode: numeric refs to the five specials only. */` |
|     99 | 6315 | `			for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) && aHtmlSpecEnt[n].cp != cp ; n++ ){}` |
|     25 | 6316 | `			if( n >= SX_ARRAYSIZE(aHtmlSpecEnt) ){ return 0; }` |
|     11 | 6317 | `		}` |
|     75 | 6318 | `		*pCp = cp;` |
|     75 | 6319 | `		*pnConsumed = i + 1;` |
|     75 | 6320 | `		return 1;` |
|      - | 6321 | `	}` |
|      - | 6322 | `	/* Named reference — every entity name starts with a letter, so anything` |
|      - | 6323 | `	 * else can bail out before touching the tables. */` |
|     81 | 6324 | `	if( !((z[1]>='a' && z[1]<='z') \|\| (z[1]>='A' && z[1]<='Z')) ){ return 0; }` |
|    287 | 6325 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) ; n++ ){` |
|    265 | 6326 | `		if( aHtmlSpecEnt[n].cp == 39 && iDoc == PH7_ENT_DOC_HTML401 ){ continue; }` |
|    243 | 6327 | `		if( nAvail >= aHtmlSpecEnt[n].n && SyMemcmp(z,aHtmlSpecEnt[n].zEnt,(sxu32)aHtmlSpecEnt[n].n) == 0 ){` |
|     53 | 6328 | `			*pCp = aHtmlSpecEnt[n].cp;` |
|     53 | 6329 | `			*pnConsumed = aHtmlSpecEnt[n].n;` |
|     53 | 6330 | `			return 1;` |
|      - | 6331 | `		}` |
|     96 | 6332 | `	}` |
|     23 | 6333 | `	if( bFull && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 6334 | `		/* Linear scan of the 248-row table: runs only at '&'-then-letter` |
|      - | 6335 | `		 * positions and guarantees the decode set can never drift from the` |
|      - | 6336 | `		 * encode table. The first-letter guard skips the SyStrlen/SyMemcmp` |
|      - | 6337 | `		 * for ~96% of rows. */` |
|   3369 | 6338 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|      - | 6339 | `			sxu32 nEnt;` |
|   3357 | 6340 | `			if( z[1] != (unsigned char)aHtml401Ent[n].zEnt[1] ){ continue; }` |
|    121 | 6341 | `			nEnt = SyStrlen(aHtml401Ent[n].zEnt);` |
|    121 | 6342 | `			if( (sxu32)nAvail >= nEnt && SyMemcmp(z,aHtml401Ent[n].zEnt,nEnt) == 0 ){` |
|      7 | 6343 | `				*pCp = aHtml401Ent[n].cp;` |
|      7 | 6344 | `				*pnConsumed = (int)nEnt;` |
|      7 | 6345 | `				return 1;` |
|      - | 6346 | `			}` |
|     58 | 6347 | `		}` |
|      6 | 6348 | `	}` |
|     17 | 6349 | `	return 0;` |
|     88 | 6350 | `}` |
|      - | 6351 | `/* Shared encoder for htmlspecialchars (bAll=0) and htmlentities (bAll=1).` |
|      - | 6352 | ` * Invalid UTF-8 policy: ENT_IGNORE drops the byte (and wins over SUBSTITUTE),` |
|      - | 6353 | ` * ENT_SUBSTITUTE emits one U+FFFD per invalid byte, neither -> the whole` |
|      - | 6354 | ` * result is "" (pre-validated in a first pass: the accumulating result API` |
|      - | 6355 | ` * cannot roll back — same reason FvSanitizeFull is two-pass). */` |
|     96 | 6356 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 6357 | `                       int iFlags,int bAll,int bDoubleEncode){` |
|     97 | 6358 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     97 | 6359 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|      - | 6360 | `	const unsigned char *runStart;` |
|     97 | 6361 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6362 | `	sxu32 cp;` |
|     97 | 6363 | `	if( (iFlags & (PH7_ENT_IGNORE\|PH7_ENT_SUBSTITUTE)) == 0 ){` |
|      - | 6364 | `		/* Pass 1: any malformed sequence rejects the entire input. ASCII` |
|      - | 6365 | `		 * bytes cannot be malformed, so skip them without the decoder. */` |
|    381 | 6366 | `		while( p < zEnd ){` |
|      - | 6367 | `			int len;` |
|    323 | 6368 | `			if( *p < 0x80 ){ p++; continue; }` |
|     37 | 6369 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     37 | 6370 | `			if( len == 0 ){ ph7_result_string(pCtx,"",0); return; }` |
|     27 | 6371 | `			p += len;` |
|      1 | 6372 | `		}` |
|     59 | 6373 | `		p = (const unsigned char *)zIn;` |
|     29 | 6374 | `	}` |
|     87 | 6375 | `	runStart = p;` |
|     87 | 6376 | `	ph7_result_string(pCtx,"",0);` |
|    463 | 6377 | `	while( p < zEnd ){` |
|    377 | 6378 | `		const char *zEnt = 0;` |
|      - | 6379 | `		int len;` |
|    377 | 6380 | `		if( *p < 0x80 ){` |
|    313 | 6381 | `			len = 1;` |
|    313 | 6382 | `			switch( *p ){` |
|     25 | 6383 | `			case '<': zEnt = "&lt;"; break;` |
|     25 | 6384 | `			case '>': zEnt = "&gt;"; break;` |
|     18 | 6385 | `			case '&':` |
|     37 | 6386 | `				zEnt = "&amp;";` |
|     37 | 6387 | `				if( !bDoubleEncode ){` |
|      - | 6388 | `					sxu32 eCp; int nEat;` |
|     25 | 6389 | `					if( HtmlParseEntity(p,zEnd,iFlags,1,1,&eCp,&nEat) ){` |
|      - | 6390 | `						/* A valid existing entity: keep it verbatim. */` |
|     13 | 6391 | `						zEnt = 0;` |
|     13 | 6392 | `						len = nEat;` |
|      6 | 6393 | `					}` |
|     12 | 6394 | `				}` |
|     37 | 6395 | `				break;` |
|     10 | 6396 | `			case '"':` |
|     21 | 6397 | `				if( iFlags & PH7_ENT_QUOTE_DOUBLE ){ zEnt = "&quot;"; }` |
|     21 | 6398 | `				break;` |
|     12 | 6399 | `			case '\'':` |
|     25 | 6400 | `				if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|     23 | 6401 | `					zEnt = HtmlAposEntity(iDoc,bAll);` |
|     11 | 6402 | `				}` |
|     25 | 6403 | `				break;` |
|     92 | 6404 | `			default:` |
|    185 | 6405 | `				if( (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode((sxu32)*p,iFlags) ){` |
|    ! 0 | 6406 | `					zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 6407 | `				}` |
|    184 | 6408 | `				break;` |
|      - | 6409 | `			}` |
|    157 | 6410 | `		}else{` |
|     65 | 6411 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     65 | 6412 | `			if( len == 0 ){` |
|      - | 6413 | `				/* Malformed subpart (IGNORE or SUBSTITUTE is set, else pass 1` |
|      - | 6414 | `				 * would have rejected): drop it or emit ONE U+FFFD for the` |
|      - | 6415 | `				 * whole unit (php substitutes per maximal invalid subpart). */` |
|     15 | 6416 | `				if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|     15 | 6417 | `				if( (iFlags & PH7_ENT_IGNORE) == 0 ){ ph7_result_string(pCtx,"\xEF\xBF\xBD",3); }` |
|     15 | 6418 | `				p += HtmlUtf8FailAdvance(p,zEnd);` |
|     15 | 6419 | `				runStart = p;` |
|     15 | 6420 | `				continue;` |
|      - | 6421 | `			}` |
|     51 | 6422 | `			if( bAll && HtmlDocHasNamedTable(iDoc) ){` |
|     41 | 6423 | `				zEnt = FvHtml401Lookup(cp);` |
|     20 | 6424 | `			}` |
|     51 | 6425 | `			if( zEnt == 0 && (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode(cp,iFlags) ){` |
|    ! 0 | 6426 | `				zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 6427 | `			}` |
|      - | 6428 | `		}` |
|    363 | 6429 | `		if( zEnt ){` |
|    135 | 6430 | `			if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|    135 | 6431 | `			ph7_result_string(pCtx,zEnt,-1);` |
|    135 | 6432 | `			runStart = p + len;` |
|     67 | 6433 | `		}` |
|    363 | 6434 | `		p += len;` |
|      1 | 6435 | `	}` |
|     87 | 6436 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     49 | 6437 | `}` |
|      - | 6438 | `/* Shared decoder for html_entity_decode (bFull=1) and htmlspecialchars_decode` |
|      - | 6439 | ` * (bFull=0). Quote refs (cp 34/39, named or numeric) are gated by the quote` |
|      - | 6440 | ` * bits and left verbatim when suppressed; an invalid entity leaves its '&'` |
|      - | 6441 | ` * verbatim and rescans right after it, which also yields PHP's no-double-` |
|      - | 6442 | ` * decode behavior ("&amp;lt;" -> "&lt;"). */` |
|     84 | 6443 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 6444 | `                         int iFlags,int bFull){` |
|     85 | 6445 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     85 | 6446 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|     85 | 6447 | `	const unsigned char *runStart = p;` |
|     85 | 6448 | `	ph7_result_string(pCtx,"",0);` |
|    565 | 6449 | `	while( p < zEnd ){` |
|      - | 6450 | `		sxu32 cp;` |
|      - | 6451 | `		int nEat;` |
|    516 | 6452 | `		if( *p != '&' ){ p++; continue; }` |
|    155 | 6453 | `		if( !HtmlParseEntity(p,zEnd,iFlags,bFull,0,&cp,&nEat) ){ p++; continue; }` |
|    124 | 6454 | `		if( (cp == 34 && (iFlags & PH7_ENT_QUOTE_DOUBLE) == 0)` |
|    117 | 6455 | `		 \|\| (cp == 39 && (iFlags & PH7_ENT_QUOTE_SINGLE) == 0) ){` |
|      - | 6456 | `			/* Suppressed quote: leave the entity source verbatim. */` |
|     37 | 6457 | `			p += nEat;` |
|     37 | 6458 | `			continue;` |
|      - | 6459 | `		}` |
|     89 | 6460 | `		if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|      - | 6461 | `		{` |
|      - | 6462 | `			char zBuf[4];` |
|     89 | 6463 | `			int n = HtmlCpUtf8(cp,zBuf);` |
|     89 | 6464 | `			ph7_result_string(pCtx,zBuf,n);` |
|      - | 6465 | `		}` |
|     89 | 6466 | `		p += nEat;` |
|     89 | 6467 | `		runStart = p;` |
|      1 | 6468 | `	}` |
|     81 | 6469 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     81 | 6470 | `}` |
|      - | 6471 | `/* Validate the optional charset argument at apArg[idx]: UTF-8 aliases (and` |
|      - | 6472 | ` * ""/NULL meaning the default) are accepted; anything else — including` |
|      - | 6473 | ` * php-supported single-byte charsets like ISO-8859-1, PHL is UTF-8-only by` |
|      - | 6474 | ` * policy — raises PHP's unsupported-charset warning and is treated as` |
|      - | 6475 | ` * UTF-8 (ph7_context_throw_error_format prepends the function name). */` |
|    143 | 6476 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx){` |
|      - | 6477 | `	const char *zCs;` |
|      - | 6478 | `	int nCs;` |
|    150 | 6479 | `	if( nArg <= idx \|\| ph7_value_is_null(apArg[idx]) ){ return; }` |
|     15 | 6480 | `	zCs = ph7_value_to_string(apArg[idx],&nCs);` |
|     15 | 6481 | `	if( nCs == 0 ){ return; } /* "" selects the default charset (UTF-8) */` |
|     13 | 6482 | `	if( nCs == 5 && SyStrnicmp(zCs,"UTF-8",5) == 0 ){` |
|     13 | 6483 | `		return; /* php accepts only "UTF-8" (any case) silently — "UTF8" warns */` |
|      - | 6484 | `	}` |
|    ! 0 | 6485 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 6486 | `		"Charset \"%.*s\" is not supported, assuming UTF-8",nCs,zCs);` |
|     72 | 6487 | `}` |
|      - | 6488 | `/* get_html_translation_table() worker: character (UTF-8 bytes) => entity.` |
|      - | 6489 | ` * The five specials come first in byte order, then — for HTML_ENTITIES with a` |
|      - | 6490 | ` * named-table doctype — the 248 aHtml401Ent rows ascending (oracle-pinned` |
|      - | 6491 | ` * ordering; 253 entries under the defaults). */` |
|    549 | 6492 | `static void HtmlTableAdd(ph7_value *pArray,ph7_value *pValue,const char *zKey,const char *zEnt){` |
|    549 | 6493 | `	ph7_value_string(pValue,zEnt,-1);` |
|    549 | 6494 | `	ph7_array_add_strkey_elem(pArray,zKey,pValue);` |
|    549 | 6495 | `	ph7_value_reset_string_cursor(pValue);` |
|    549 | 6496 | `}` |
|     13 | 6497 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags){` |
|      - | 6498 | `	ph7_value *pArray,*pValue;` |
|     13 | 6499 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6500 | `	sxu32 n;` |
|     13 | 6501 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 6502 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 6503 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|    ! 0 | 6504 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6505 | `		return;` |
|      - | 6506 | `	}` |
|     13 | 6507 | `	if( iFlags & PH7_ENT_QUOTE_DOUBLE ){` |
|     11 | 6508 | `		HtmlTableAdd(pArray,pValue,"\"","&quot;");` |
|      5 | 6509 | `	}` |
|     13 | 6510 | `	HtmlTableAdd(pArray,pValue,"&","&amp;");` |
|     13 | 6511 | `	if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|      - | 6512 | `		/* The apostrophe row mirrors the function each table belongs to:` |
|      - | 6513 | `		 * SPECIALCHARS follows htmlspecialchars, ENTITIES follows` |
|      - | 6514 | `		 * htmlentities (oracle-pinned at flags 35). */` |
|      7 | 6515 | `		HtmlTableAdd(pArray,pValue,"'",HtmlAposEntity(iDoc,iTable != 0));` |
|      3 | 6516 | `	}` |
|     13 | 6517 | `	HtmlTableAdd(pArray,pValue,"<","&lt;");` |
|     13 | 6518 | `	HtmlTableAdd(pArray,pValue,">","&gt;");` |
|     13 | 6519 | `	if( iTable != 0 /*php: any non-HTML_SPECIALCHARS table => entities*/ && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 6520 | `		char zKey[8];` |
|    499 | 6521 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|    497 | 6522 | `			int nK = HtmlCpUtf8(aHtml401Ent[n].cp,zKey);` |
|    497 | 6523 | `			zKey[nK] = 0;` |
|    497 | 6524 | `			HtmlTableAdd(pArray,pValue,zKey,aHtml401Ent[n].zEnt);` |
|    249 | 6525 | `		}` |
|      1 | 6526 | `	}` |
|     13 | 6527 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 6528 | `}` |
|     25 | 6529 | `static int FvEmailAllowed(unsigned char c){` |
|     25 | 6530 | `	if( (c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9') ){ return 1; }` |
|     16 | 6531 | `	return c=='!'\|\|c=='#'\|\|c=='$'\|\|c=='%'\|\|c=='&'\|\|c=='\''\|\|c=='*'\|\|c=='+'` |
|     10 | 6532 | ``	    \|\| c=='-'\|\|c=='='\|\|c=='?'\|\|c=='^'\|\|c=='_'\|\|c=='`'\|\|c=='{'\|\|c=='\|'`` |
|     15 | 6533 | `	    \|\| c=='}'\|\|c=='~'\|\|c=='@'\|\|c=='.'\|\|c=='['\|\|c==']';` |
|     13 | 6534 | `}` |
|     23 | 6535 | `static int FvUrlAllowed(unsigned char c){` |
|     23 | 6536 | `	return c>=33 && c<=126; /* PHP keeps every printable ASCII byte except space */` |
|      1 | 6537 | `}` |
|      - | 6538 | `/* SANITIZE_EMAIL (isUrl=0) / SANITIZE_URL (isUrl=1): strip disallowed bytes. */` |
|      5 | 6539 | `static void FvSanitizeChars(ph7_context *pCtx,const char *z,int n,int isUrl){` |
|      5 | 6540 | `	int i, runStart = 0;` |
|      5 | 6541 | `	ph7_result_string(pCtx,"",0);` |
|     51 | 6542 | `	for( i=0; i<n; i++ ){` |
|     47 | 6543 | `		unsigned char c = (unsigned char)z[i];` |
|     47 | 6544 | `		if( !(isUrl ? FvUrlAllowed(c) : FvEmailAllowed(c)) ){` |
|     11 | 6545 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     11 | 6546 | `			runStart = i+1;` |
|      5 | 6547 | `		}` |
|     24 | 6548 | `	}` |
|      5 | 6549 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      5 | 6550 | `}` |
|      - | 6551 | `/*` |
|      - | 6552 | ` * Apply the selected filter to one already-resolved input value and write the` |
|      - | 6553 | ` * result into pCtx. Shared by filter_var() and filter_input(): the caller has` |
|      - | 6554 | ` * already parsed $filter/$flags/$options. On validation failure the 'default'` |
|      - | 6555 | ` * option (if any) is returned, else null when FILTER_NULL_ON_FAILURE is set,` |
|      - | 6556 | ` * else false. A validating filter that passes returns the (string) input` |
|      - | 6557 | ` * unchanged; a sanitizer writes its transformed output directly.` |
|      - | 6558 | ` */` |
|    316 | 6559 | `static int FvApplyFilter(ph7_context *pCtx,ph7_value *pInput,` |
|      - | 6560 | `                         int iFilter,int iFlags,ph7_value *pOpts,` |
|      - | 6561 | `                         ph7_value *pDefault)` |
|      3 | 6562 | `{` |
|    319 | 6563 | `	int bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|      - | 6564 | `	const char *zVal; int nVal;` |
|      - | 6565 | `	/* An array/object input fails every scalar filter. */` |
|    319 | 6566 | `	if( ph7_value_is_array(pInput) ){ goto fail; }` |
|    317 | 6567 | `	zVal = ph7_value_to_string(pInput,&nVal);` |
|    317 | 6568 | `	switch( iFilter ){` |
|     28 | 6569 | `	case FV_VALIDATE_INT: {` |
|      - | 6570 | `		ph7_int64 v;` |
|     58 | 6571 | `		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }` |
|     31 | 6572 | `		if( pOpts ){` |
|      7 | 6573 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|      7 | 6574 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|      7 | 6575 | `			if( pMin && v<ph7_value_to_int64(pMin) ){ goto fail; }` |
|      7 | 6576 | `			if( pMax && v>ph7_value_to_int64(pMax) ){ goto fail; }` |
|      2 | 6577 | `		}` |
|     29 | 6578 | `		ph7_result_int64(pCtx,v);` |
|     29 | 6579 | `		return PH7_OK;` |
|      - | 6580 | `	}` |
|     34 | 6581 | `	case FV_VALIDATE_FLOAT: {` |
|      - | 6582 | `		double d;` |
|     69 | 6583 | `		if( !FvValidateFloat(zVal,nVal,iFlags,&d) ){ goto fail; }` |
|     39 | 6584 | `		ph7_result_double(pCtx,d);` |
|     39 | 6585 | `		return PH7_OK;` |
|      - | 6586 | `	}` |
|     14 | 6587 | `	case FV_VALIDATE_BOOLEAN: {` |
|      - | 6588 | `		int b;` |
|     29 | 6589 | `		if( !FvValidateBool(zVal,nVal,&b) ){ goto fail; }` |
|     21 | 6590 | `		ph7_result_bool(pCtx,b);` |
|     21 | 6591 | `		return PH7_OK;` |
|      - | 6592 | `	}` |
|     25 | 6593 | `	case FV_VALIDATE_IP:     if( !FvValidateIp(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|     11 | 6594 | `	case FV_VALIDATE_MAC:    if( !FvValidateMac(zVal,nVal) ){ goto fail; }       goto pass;` |
|     28 | 6595 | `	case FV_VALIDATE_EMAIL:  if( !FvValidateEmail(zVal,nVal) ){ goto fail; }     goto pass;` |
|     11 | 6596 | `	case FV_VALIDATE_DOMAIN: if( !FvValidateDomain(zVal,nVal) ){ goto fail; }    goto pass;` |
|     15 | 6597 | `	case FV_VALIDATE_URL:    if( !FvValidateUrl(zVal,nVal) ){ goto fail; }       goto pass;` |
|      3 | 6598 | `	case FV_VALIDATE_REGEXP: {` |
|      - | 6599 | `#ifdef PH7_ENABLE_PCRE` |
|      8 | 6600 | `		ph7_value *pRe = pOpts ? ph7_array_fetch(pOpts,"regexp",(int)sizeof("regexp")-1) : 0;` |
|      8 | 6601 | `		const char *zRe; int nRe, matched = 0;` |
|      8 | 6602 | `		if( pRe==0 ){` |
|      3 | 6603 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6604 | `				"filter_var(): \"regexp\" option is missing");` |
|      - | 6605 | `		}` |
|      5 | 6606 | `		zRe = ph7_value_to_string(pRe,&nRe);` |
|      5 | 6607 | `		if( PH7_PcreMatchQuiet(pCtx,zRe,nRe,zVal,nVal,&matched)!=SXRET_OK \|\| !matched ){ goto fail; }` |
|      3 | 6608 | `		goto pass;` |
|      - | 6609 | `#else` |
|      - | 6610 | `		goto fail;` |
|      - | 6611 | `#endif` |
|      - | 6612 | `	}` |
|      3 | 6613 | `	case FV_SANITIZE_NUMBER_INT:   FvSanitizeNumber(pCtx,zVal,nVal,0,0);      return PH7_OK;` |
|      5 | 6614 | `	case FV_SANITIZE_NUMBER_FLOAT: FvSanitizeNumber(pCtx,zVal,nVal,1,iFlags); return PH7_OK;` |
|     13 | 6615 | `	case FV_SANITIZE_SPECIAL_CHARS:      FvSanitizeSpecial(pCtx,zVal,nVal,iFlags); return PH7_OK;` |
|     25 | 6616 | `	case FV_SANITIZE_FULL_SPECIAL_CHARS: FvSanitizeFull(pCtx,zVal,nVal,iFlags);    return PH7_OK;` |
|      3 | 6617 | `	case FV_SANITIZE_EMAIL: FvSanitizeChars(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 6618 | `	case FV_SANITIZE_URL:   FvSanitizeChars(pCtx,zVal,nVal,1); return PH7_OK;` |
|     13 | 6619 | `	case FV_DEFAULT:` |
|      - | 6620 | `		/* FILTER_UNSAFE_RAW / FILTER_DEFAULT: pass through unchanged unless a` |
|      - | 6621 | `		 * STRIP/ENCODE flag is set, in which case apply the string filter. */` |
|     28 | 6622 | `		if( iFlags & FV_FLAG_STRING_MASK ){` |
|     15 | 6623 | `			FvSanitizeString(pCtx,zVal,nVal,iFlags);` |
|     15 | 6624 | `			return PH7_OK;` |
|      - | 6625 | `		}` |
|     14 | 6626 | `		goto pass;` |
|    ! 0 | 6627 | `	default:` |
|    ! 0 | 6628 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 6629 | `			"Unknown filter with ID %d",iFilter);` |
|    ! 0 | 6630 | `		break; /* unknown filter id -> fail */` |
|    ! 0 | 6631 | `	}` |
|     58 | 6632 | `fail:` |
|    118 | 6633 | `	if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|    114 | 6634 | `	else if( bNull ){ ph7_result_null(pCtx); }` |
|    108 | 6635 | `	else { ph7_result_bool(pCtx,0); }` |
|    118 | 6636 | `	return PH7_OK;` |
|     26 | 6637 | `pass: /* validation passed: return the (string) input unchanged */` |
|     54 | 6638 | `	ph7_result_string(pCtx,zVal,nVal);` |
|     54 | 6639 | `	return PH7_OK;` |
|    161 | 6640 | `}` |
|      - | 6641 | `/*` |
|      - | 6642 | ` * Parse the ($filter, $options) pair shared by filter_var()/filter_input() out` |
|      - | 6643 | ` * of apArg[iBase] ($filter) and apArg[iBase+1] ($options): $options is either a` |
|      - | 6644 | ` * plain flags int, or an array with 'flags' and an 'options' sub-array (whose` |
|      - | 6645 | ` * 'default' entry is the fallback value). Fills the four output pointers;` |
|      - | 6646 | ` * unset outputs keep the caller-provided defaults.` |
|      - | 6647 | ` */` |
|    328 | 6648 | `static void FvParseFilterArgs(int nArg,ph7_value **apArg,int iBase,` |
|      - | 6649 | `                              int *piFilter,int *piFlags,` |
|      - | 6650 | `                              ph7_value **ppOpts,ph7_value **ppDefault)` |
|      3 | 6651 | `{` |
|    331 | 6652 | `	if( nArg>iBase ){ *piFilter = ph7_value_to_int(apArg[iBase]); }` |
|    331 | 6653 | `	if( nArg>iBase+1 ){` |
|     88 | 6654 | `		if( ph7_value_is_array(apArg[iBase+1]) ){` |
|     42 | 6655 | `			ph7_value *pF = ph7_array_fetch(apArg[iBase+1],"flags",(int)sizeof("flags")-1);` |
|     42 | 6656 | `			if( pF ){ *piFlags = ph7_value_to_int(pF); }` |
|     42 | 6657 | `			*ppOpts = ph7_array_fetch(apArg[iBase+1],"options",(int)sizeof("options")-1);` |
|     42 | 6658 | `			if( *ppOpts && !ph7_value_is_array(*ppOpts) ){ *ppOpts = 0; }` |
|     42 | 6659 | `			if( *ppOpts ){ *ppDefault = ph7_array_fetch(*ppOpts,"default",(int)sizeof("default")-1); }` |
|     22 | 6660 | `		}else{` |
|     48 | 6661 | `			*piFlags = ph7_value_to_int(apArg[iBase+1]);` |
|      - | 6662 | `		}` |
|     43 | 6663 | `	}` |
|    331 | 6664 | `}` |
|      - | 6665 | `/*` |
|      - | 6666 | ` * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 6667 | ` *  Validate or sanitize a value; see FvApplyFilter for the failure semantics.` |
|      - | 6668 | ` */` |
|    306 | 6669 | `static int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6670 | `{` |
|    308 | 6671 | `	int iFilter = FV_DEFAULT, iFlags = 0;` |
|    308 | 6672 | `	ph7_value *pOpts = 0, *pDefault = 0;` |
|    308 | 6673 | `	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    308 | 6674 | `	FvParseFilterArgs(nArg,apArg,1,&iFilter,&iFlags,&pOpts,&pDefault);` |
|    308 | 6675 | `	return FvApplyFilter(pCtx,apArg[0],iFilter,iFlags,pOpts,pDefault);` |
|    155 | 6676 | `}` |
|      - | 6677 | `/*` |
|      - | 6678 | ` * filter_input($type, $var_name, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 6679 | ` *  Look up $var_name in the requested INPUT_* superglobal, then apply the` |
|      - | 6680 | ` *  filter. Semantics verified byte-for-byte against php 8.5:` |
|      - | 6681 | ` *   - variable NOT set: 'default' option wins, else false when` |
|      - | 6682 | ` *     FILTER_NULL_ON_FAILURE is set, else null. (Note the null/false roles are` |
|      - | 6683 | ` *     INVERTED relative to a present value that fails validation, which yields` |
|      - | 6684 | ` *     default > null-if-NULL_ON_FAILURE > false via FvApplyFilter.)` |
|      - | 6685 | ` *   - variable present: delegate to FvApplyFilter.` |
|      - | 6686 | ` *  Divergence: php reads a SAPI snapshot of the original request variables` |
|      - | 6687 | ` *  captured at startup; PHL reads the live superglobal. In CLI they match for` |
|      - | 6688 | ` *  the SAPI-registered keys (SCRIPT_NAME/PHP_SELF/DOCUMENT_ROOT); keys added` |
|      - | 6689 | ` *  only to the live $_SERVER (REQUEST_TIME/PWD/…) are visible here but not in` |
|      - | 6690 | ` *  php's snapshot.` |
|      - | 6691 | ` */` |
|     24 | 6692 | `static int PH7_builtin_filter_input(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6693 | `{` |
|     26 | 6694 | `	int iType, iFilter = FV_DEFAULT, iFlags = 0;` |
|     26 | 6695 | `	ph7_value *pOpts = 0, *pDefault = 0, *pSuper, *pElem;` |
|      - | 6696 | `	const char *zVar, *zSuper; int nVar; sxu32 nSuper;` |
|     26 | 6697 | `	if( nArg<2 ){` |
|    ! 0 | 6698 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 6699 | `			"filter_input() expects at least 2 arguments, %d given",nArg);` |
|      - | 6700 | `	}` |
|     26 | 6701 | `	iType = ph7_value_to_int(apArg[0]);` |
|     26 | 6702 | `	switch( iType ){` |
|      3 | 6703 | `	case 0: zSuper = "_POST";   nSuper = (sxu32)sizeof("_POST")-1;   break; /* INPUT_POST */` |
|      3 | 6704 | `	case 1: zSuper = "_GET";    nSuper = (sxu32)sizeof("_GET")-1;    break; /* INPUT_GET */` |
|    ! 0 | 6705 | `	case 2: zSuper = "_COOKIE"; nSuper = (sxu32)sizeof("_COOKIE")-1; break; /* INPUT_COOKIE */` |
|    ! 0 | 6706 | `	case 4: zSuper = "_ENV";    nSuper = (sxu32)sizeof("_ENV")-1;    break; /* INPUT_ENV */` |
|     19 | 6707 | `	case 5: zSuper = "_SERVER"; nSuper = (sxu32)sizeof("_SERVER")-1; break; /* INPUT_SERVER */` |
|      1 | 6708 | `	default:` |
|      3 | 6709 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6710 | `			"filter_input(): Argument #1 ($type) must be an INPUT_* constant");` |
|      - | 6711 | `	}` |
|     23 | 6712 | `	zVar = ph7_value_to_string(apArg[1],&nVar);` |
|     23 | 6713 | `	FvParseFilterArgs(nArg,apArg,2,&iFilter,&iFlags,&pOpts,&pDefault);` |
|      - | 6714 | `	/* Resolve the variable from the superglobal (missing/non-array -> not set). */` |
|     23 | 6715 | `	pSuper = PH7_VmExtractSuper(pCtx->pVm,zSuper,nSuper);` |
|     23 | 6716 | `	pElem = (pSuper && ph7_value_is_array(pSuper))` |
|     33 | 6717 | `		? ph7_array_fetch(pSuper,zVar,nVar) : 0;` |
|     23 | 6718 | `	if( pElem==0 ){` |
|      - | 6719 | `		/* Variable not set: default > false(if NULL_ON_FAILURE) > null. Note the` |
|      - | 6720 | `		 * false/null roles are inverted vs FvApplyFilter's present-but-fails path. */` |
|     13 | 6721 | `		if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|      9 | 6722 | `		else if( iFlags & FV_NULL_ON_FAILURE ){ ph7_result_bool(pCtx,0); }` |
|      7 | 6723 | `		else { ph7_result_null(pCtx); }` |
|     13 | 6724 | `		return PH7_OK;` |
|      - | 6725 | `	}` |
|     11 | 6726 | `	return FvApplyFilter(pCtx,pElem,iFilter,iFlags,pOpts,pDefault);` |
|     14 | 6727 | `}` |
|      - | 6728 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6729 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6730 | `/*` |
|      - | 6731 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 6732 |  |
|      - | 6733 | ` */` |
|      4 | 6734 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 6735 | `	const char *zInput, /* Raw input */` |
|      - | 6736 | `	int nByte,  /* Input length */` |
|      - | 6737 | `	int delim,  /* Delimiter */` |
|      - | 6738 | `	int encl,   /* Enclosure */` |
|      - | 6739 | `	int escape,  /* Escape character */` |
|      - | 6740 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 6741 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 6742 | `	)` |
|      1 | 6743 | `{` |
|      5 | 6744 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 6745 | `	const char *zIn = zInput;` |
|      - | 6746 | `	const char *zPtr;` |
|      - | 6747 | `	int isEnc;` |
|      - | 6748 | `	/* Start processing */` |
|      8 | 6749 | `	for(;;){` |
|     17 | 6750 | `		if( zIn >= zEnd ){` |
|      - | 6751 | `			/* No more input to process */` |
|      5 | 6752 | `			break;` |
|      - | 6753 | `		}` |
|     13 | 6754 | `		isEnc = 0;` |
|     13 | 6755 | `		zPtr = zIn;` |
|      - | 6756 | `		/* Find the first delimiter */` |
|     27 | 6757 | `		while( zIn < zEnd ){` |
|     23 | 6758 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 6759 | `				/* Delimiter found,break imediately */` |
|      5 | 6760 | `				break;` |
|     15 | 6761 | `			}else if( zIn[0] == encl ){` |
|      - | 6762 | `				/* Inside enclosure? */` |
|    ! 0 | 6763 | `				isEnc = !isEnc;` |
|     15 | 6764 | `			}else if( zIn[0] == escape ){` |
|      - | 6765 | `				/* Escape sequence */` |
|    ! 0 | 6766 | `				zIn++;` |
|    ! 0 | 6767 | `			}` |
|      - | 6768 | `			/* Advance the cursor */` |
|     15 | 6769 | `			zIn++;` |
|      1 | 6770 | `		}` |
|     13 | 6771 | `		if( zIn > zPtr ){` |
|     13 | 6772 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 6773 | `			sxi32 rc;` |
|      - | 6774 | `			/* Invoke the supllied callback */` |
|     13 | 6775 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 6776 | `				zPtr++;` |
|    ! 0 | 6777 | `				nByteChunk-=2;` |
|    ! 0 | 6778 | `			}` |
|     13 | 6779 | `			if( nByteChunk > 0 ){` |
|     13 | 6780 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 6781 | `				if( rc == SXERR_ABORT ){` |
|      - | 6782 | `					/* User callback request an operation abort */` |
|    ! 0 | 6783 | `					break;` |
|      - | 6784 | `				}` |
|      6 | 6785 | `			}` |
|      6 | 6786 | `		}` |
|      - | 6787 | `		/* Ignore trailing delimiter */` |
|     21 | 6788 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 6789 | `			zIn++;` |
|      1 | 6790 | `		}` |
|      1 | 6791 | `	}` |
|      5 | 6792 | `	return SXRET_OK;` |
|      1 | 6793 | `}` |
|      - | 6794 | `/*` |
|      - | 6795 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 6796 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 6797 | ` * argument to this callback.` |
|      - | 6798 | ` */` |
|     12 | 6799 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 6800 | `{` |
|     13 | 6801 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 6802 | `	ph7_value sEntry;` |
|      - | 6803 | `	SyString sToken;` |
|      - | 6804 | `	/* Insert the token in the given array */` |
|     13 | 6805 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 6806 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 6807 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 6808 | `	if( sToken.nByte < 1){` |
|    ! 0 | 6809 | `		return SXRET_OK;` |
|      - | 6810 | `	}` |
|     13 | 6811 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 6812 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 6813 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 6814 | `	return SXRET_OK;` |
|      7 | 6815 | `}` |
|      - | 6816 | `/*` |
|      - | 6817 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 6818 | ` *  Parse a CSV string into an array.` |
|      - | 6819 | ` * Parameters` |
|      - | 6820 | ` *  $input` |
|      - | 6821 | ` *   The string to parse.` |
|      - | 6822 | ` *  $delimiter` |
|      - | 6823 | ` *   Set the field delimiter (one character only).` |
|      - | 6824 | ` *  $enclosure` |
|      - | 6825 | ` *   Set the field enclosure character (one character only).` |
|      - | 6826 | ` *  $escape` |
|      - | 6827 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 6828 | ` * Return` |
|      - | 6829 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 6830 | ` */` |
|      2 | 6831 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6832 | `{` |
|      - | 6833 | `	const char *zInput,*zPtr;` |
|      - | 6834 | `	ph7_value *pArray;` |
|      3 | 6835 | `	int delim  = ',';   /* Delimiter */` |
|      3 | 6836 | `	int encl   = '"' ;  /* Enclosure */` |
|      3 | 6837 | `	int escape = '\\';  /* Escape character */` |
|      - | 6838 | `	int nLen;` |
|      3 | 6839 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6840 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 6841 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6842 | `		return PH7_OK;` |
|      - | 6843 | `	}` |
|      - | 6844 | `	/* Extract the raw input */` |
|      3 | 6845 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 6846 | `	if( nArg > 1 ){` |
|      - | 6847 | `		int i;` |
|      3 | 6848 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 6849 | `			/* Extract the delimiter */` |
|      3 | 6850 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 6851 | `			if( i > 0 ){` |
|      3 | 6852 | `				delim = zPtr[0];` |
|      1 | 6853 | `			}` |
|      1 | 6854 | `		}` |
|      3 | 6855 | `		if( nArg > 2 ){` |
|      3 | 6856 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 6857 | `				/* Extract the enclosure */` |
|      3 | 6858 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 6859 | `				if( i > 0 ){` |
|      3 | 6860 | `					encl = zPtr[0];` |
|      1 | 6861 | `				}` |
|      1 | 6862 | `			}` |
|      3 | 6863 | `			if( nArg > 3 ){` |
|      3 | 6864 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 6865 | `					/* Extract the escape character */` |
|      3 | 6866 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 6867 | `					if( i > 0 ){` |
|      3 | 6868 | `						escape = zPtr[0];` |
|      1 | 6869 | `					}` |
|      1 | 6870 | `				}` |
|      1 | 6871 | `			}` |
|      1 | 6872 | `		}` |
|      1 | 6873 | `	}` |
|      - | 6874 | `	/* Create our array */` |
|      3 | 6875 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 6876 | `	if( pArray == 0 ){` |
|      - | 6877 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|    ! 0 | 6878 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6879 | `	}` |
|      - | 6880 | `	/* Parse the raw input */` |
|      3 | 6881 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 6882 | `	/* Return the freshly created array */` |
|      3 | 6883 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 6884 | `	return PH7_OK;` |
|      2 | 6885 | `}` |
|      - | 6886 | `/*` |
|      - | 6887 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 6888 | ` * container.` |
|      - | 6889 | ` * Refer to [strip_tags()].` |
|      - | 6890 | ` */` |
|     10 | 6891 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 6892 | `{` |
|     11 | 6893 | `	const char *zEnd = &zTag[nByte];` |
|      - | 6894 | `	const char *zPtr;` |
|      - | 6895 | `	SyString sEntry;` |
|      - | 6896 | `	/* Strip tags */` |
|     10 | 6897 | `	for(;;){` |
|     45 | 6898 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 6899 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 6900 | `				zTag++;` |
|      1 | 6901 | `		}` |
|     21 | 6902 | `		if( zTag >= zEnd ){` |
|     11 | 6903 | `			break;` |
|      - | 6904 | `		}` |
|     11 | 6905 | `		zPtr = zTag;` |
|      - | 6906 | `		/* Delimit the tag */` |
|     25 | 6907 | `		while(zTag < zEnd ){` |
|     25 | 6908 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 6909 | `				/* UTF-8 stream */` |
|      3 | 6910 | `				zTag++;` |
|      5 | 6911 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 6912 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 6913 | `				break;` |
|    ! 0 | 6914 | `			}else{` |
|     13 | 6915 | `				zTag++;` |
|      - | 6916 | `			}` |
|      1 | 6917 | `		}` |
|     11 | 6918 | `		if( zTag > zPtr ){` |
|      - | 6919 | `			/* Perform the insertion */` |
|     11 | 6920 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 6921 | `			SyStringFullTrim(&sEntry);` |
|     11 | 6922 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 6923 | `		}` |
|      - | 6924 | `		/* Jump the trailing '>' */` |
|     11 | 6925 | `		zTag++;` |
|      1 | 6926 | `	}` |
|     11 | 6927 | `	return SXRET_OK;` |
|      1 | 6928 | `}` |
|      - | 6929 | `/*` |
|      - | 6930 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 6931 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 6932 | ` * Refer to [strip_tags()].` |
|      - | 6933 | ` */` |
|     36 | 6934 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 6935 | `{` |
|     37 | 6936 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 6937 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 6938 | `		SyString sTag;` |
|     85 | 6939 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 6940 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 6941 | `			zTag++;` |
|      1 | 6942 | `		}` |
|      - | 6943 | `		/* Delimit the tag */` |
|     25 | 6944 | `		zCur = zTag;` |
|     77 | 6945 | `		while(zTag < zEnd ){` |
|     77 | 6946 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 6947 | `				/* UTF-8 stream */` |
|      5 | 6948 | `				zTag++;` |
|      9 | 6949 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 6950 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 6951 | `				break;` |
|    ! 0 | 6952 | `			}else{` |
|     49 | 6953 | `				zTag++;` |
|      - | 6954 | `			}` |
|      1 | 6955 | `		}` |
|     25 | 6956 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 6957 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 6958 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 6959 | `		if( sTag.nByte > 0 ){` |
|      - | 6960 | `			SyString *aEntry,*pEntry;` |
|      - | 6961 | `			sxi32 rc;` |
|      - | 6962 | `			sxu32 n;` |
|      - | 6963 | `			/* Perform the lookup */` |
|     25 | 6964 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 6965 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 6966 | `				pEntry = &aEntry[n];` |
|      - | 6967 | `				/* Do the comparison */` |
|     25 | 6968 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 6969 | `				if( !rc ){` |
|     21 | 6970 | `					return SXRET_OK;` |
|      - | 6971 | `				}` |
|      3 | 6972 | `			}` |
|      2 | 6973 | `		}` |
|      2 | 6974 | `	}` |
|      - | 6975 | `	/* No such tag */` |
|     17 | 6976 | `	return SXERR_NOTFOUND;` |
|     19 | 6977 | `}` |
|      - | 6978 | `/*` |
|      - | 6979 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 6980 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 6981 | ` * Refer to [strip_tags()].` |
|      - | 6982 | ` */` |
|     16 | 6983 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 6984 | `{` |
|     17 | 6985 | `	const char *zEnd = &zIn[nByte];` |
|      - | 6986 | `	const char *zPtr,*zTag;` |
|      - | 6987 | `	SySet sSet;` |
|      - | 6988 | `	/* initialize the set of allowed tags */` |
|     17 | 6989 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 6990 | `	if( nTaglen > 0 ){` |
|      - | 6991 | `		/* Set of allowed tags */` |
|     11 | 6992 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 6993 | `	}` |
|      - | 6994 | `	/* Set the empty string */` |
|     17 | 6995 | `	ph7_result_string(pCtx,"",0);` |
|      - | 6996 | `	/* Start processing */` |
|     26 | 6997 | `	for(;;){` |
|     53 | 6998 | `		if(zIn >= zEnd){` |
|      - | 6999 | `			/* No more input to process */` |
|     15 | 7000 | `			break;` |
|      - | 7001 | `		}` |
|     39 | 7002 | `		zPtr = zIn;` |
|      - | 7003 | `		/* Find a tag */` |
|    133 | 7004 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 7005 | `			zIn++;` |
|      1 | 7006 | `		}` |
|     39 | 7007 | `		if( zIn > zPtr ){` |
|      - | 7008 | `			/* Consume raw input */` |
|     21 | 7009 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 7010 | `		}` |
|      - | 7011 | `		/* Ignore trailing null bytes */` |
|     39 | 7012 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 7013 | `			zIn++;` |
|    ! 0 | 7014 | `		}` |
|     39 | 7015 | `		if(zIn >= zEnd){` |
|      - | 7016 | `			/* No more input to process */` |
|      3 | 7017 | `			break;` |
|      - | 7018 | `		}` |
|     37 | 7019 | `		if( zIn[0] == '<' ){` |
|      - | 7020 | `			sxi32 rc;` |
|     37 | 7021 | `			zTag = zIn++;` |
|      - | 7022 | `			/* Delimit the tag */` |
|    127 | 7023 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 7024 | `				zIn++;` |
|      1 | 7025 | `			}` |
|     37 | 7026 | `			if( zIn < zEnd ){` |
|     37 | 7027 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 7028 | `			}` |
|      - | 7029 | `			/* Query the set */` |
|     37 | 7030 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 7031 | `			if( rc == SXRET_OK ){` |
|      - | 7032 | `				/* Keep the tag */` |
|     21 | 7033 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 7034 | `			}` |
|     18 | 7035 | `		}` |
|      1 | 7036 | `	}` |
|      - | 7037 | `	/* Cleanup */` |
|     17 | 7038 | `	SySetRelease(&sSet);` |
|     17 | 7039 | `	return SXRET_OK;` |
|      1 | 7040 | `}` |
|      - | 7041 | `/*` |
|      - | 7042 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 7043 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 7044 | ` * Parameters` |
|      - | 7045 | ` *  $str` |
|      - | 7046 | ` *  The input string.` |
|      - | 7047 | ` * $allowable_tags` |
|      - | 7048 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 7049 | ` * Return` |
|      - | 7050 | ` *  Returns the stripped string.` |
|      - | 7051 | ` */` |
|     14 | 7052 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7053 | `{` |
|     15 | 7054 | `	const char *zTaglist = 0;` |
|      - | 7055 | `	const char *zString;` |
|     15 | 7056 | `	int nTaglen = 0;` |
|      - | 7057 | `	int nLen;` |
|     15 | 7058 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7059 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 7060 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7061 | `		return PH7_OK;` |
|      - | 7062 | `	}` |
|      - | 7063 | `	/* Point to the raw string */` |
|     15 | 7064 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 7065 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 7066 | `		/* Allowed tag */` |
|     11 | 7067 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 7068 | `	}` |
|      - | 7069 | `	/* Process input */` |
|     15 | 7070 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 7071 | `	return PH7_OK;` |
|      8 | 7072 | `}` |
|      - | 7073 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7074 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7075 | `/*` |
|      - | 7076 | ` * string str_shuffle(string $str)` |
|      - | 7077 |  |
|      - | 7078 | ` *  Randomly shuffles a string.` |
|      - | 7079 | ` * Parameters` |
|      - | 7080 | ` *  $str` |
|      - | 7081 | ` *   The input string.` |
|      - | 7082 | ` * Return` |
|      - | 7083 | ` *  Returns the shuffled string.` |
|      - | 7084 | ` */` |
|     10 | 7085 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7086 | `{` |
|      - | 7087 | `	const char *zString;` |
|      - | 7088 | `	int nLen,i,c;` |
|      - | 7089 | `	sxu32 iR;` |
|     11 | 7090 | `	if( nArg < 1 ){` |
|      - | 7091 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7092 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7093 | `		return PH7_OK;` |
|      - | 7094 | `	}` |
|      - | 7095 | `	/* Extract the target string */` |
|     11 | 7096 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 7097 | `	if( nLen < 1 ){` |
|      - | 7098 | `		/* Nothing to shuffle */` |
|      3 | 7099 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 7100 | `		return PH7_OK;` |
|      - | 7101 | `	}` |
|      - | 7102 | `	/* Shuffle the string */` |
|     43 | 7103 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 7104 | `		/* Generate a random number first */` |
|     35 | 7105 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 7106 | `		/* Extract a random offset */` |
|     35 | 7107 | `		c = zString[iR % nLen];` |
|      - | 7108 | `		/* Append it */` |
|     35 | 7109 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 7110 | `	}` |
|      9 | 7111 | `	return PH7_OK;` |
|      6 | 7112 | `}` |
|      - | 7113 | `/*` |
|      - | 7114 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 7115 | ` *  Convert a string to an array.` |
|      - | 7116 | ` * Parameters` |
|      - | 7117 | ` * $string` |
|      - | 7118 | ` *  The input string.` |
|      - | 7119 | ` * $split_length` |
|      - | 7120 | ` *  Maximum length of the chunk.` |
|      - | 7121 | ` * Return` |
|      - | 7122 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 7123 | ` *  except possibly the last one which may be shorter.` |
|      - | 7124 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 7125 | ` *  as the first (and only) array element.` |
|      - | 7126 | ` *  An empty string returns an empty array.` |
|      - | 7127 | ` * Errors` |
|      - | 7128 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 7129 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 7130 | ` *  ValueError if $split_length is less than 1.` |
|      - | 7131 | ` */` |
|     24 | 7132 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 7133 | `{` |
|      - | 7134 | `	const char *zString,*zEnd;` |
|      - | 7135 | `	ph7_value *pArray,*pValue;` |
|      - | 7136 | `	int split_len;` |
|      - | 7137 | `	int nLen;` |
|     27 | 7138 | `	if( nArg < 1 ){` |
|    ! 0 | 7139 | `		return PH7_VmThrowException(pCtx,` |
|      - | 7140 | `			"ArgumentCountError",` |
|      - | 7141 | `			"str_split() expects at least 1 argument, %d given",` |
|    ! 0 | 7142 | `			nArg` |
|      - | 7143 | `			);` |
|      - | 7144 | `	}` |
|      - | 7145 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     36 | 7146 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     39 | 7147 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 7148 | `	    ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 7149 | `		return PH7_VmThrowException(pCtx,` |
|      - | 7150 | `			"TypeError",` |
|      - | 7151 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 7152 | `			ph7_type_name(apArg[0])` |
|      - | 7153 | `			);` |
|      - | 7154 | `	}` |
|      - | 7155 | `	/* Point to the target string */` |
|     27 | 7156 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 7157 | `	split_len = (int)sizeof(char);` |
|     27 | 7158 | `	if( nArg > 1 ){` |
|      - | 7159 | `		/* Split length */` |
|     17 | 7160 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 7161 | `		if( split_len < 1 ){` |
|      6 | 7162 | `			return PH7_VmThrowException(pCtx,` |
|      - | 7163 | `				"ValueError",` |
|      - | 7164 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 7165 | `				);` |
|      - | 7166 | `		}` |
|     11 | 7167 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 7168 | `			split_len = nLen;` |
|      1 | 7169 | `		}` |
|      5 | 7170 | `	}` |
|      - | 7171 | `	/* Create the array and the scalar value */` |
|     21 | 7172 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 7173 | `	/*Chunk value */` |
|     21 | 7174 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 7175 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 7176 | `		/* Return FALSE */` |
|    ! 0 | 7177 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7178 | `		return PH7_OK;` |
|      - | 7179 | `	}` |
|      - | 7180 | `	/* Point to the end of the string */` |
|     21 | 7181 | `	zEnd = &zString[nLen];` |
|      - | 7182 | `	/* Perform the requested operation */` |
|     48 | 7183 | `	for(;;){` |
|      - | 7184 | `		int nMax;` |
|     59 | 7185 | `		if( zString >= zEnd ){` |
|      - | 7186 | `			/* No more input to process */` |
|     21 | 7187 | `			break;` |
|      - | 7188 | `		}` |
|     39 | 7189 | `		nMax = (int)(zEnd-zString);` |
|     39 | 7190 | `		if( nMax < split_len ){` |
|      3 | 7191 | `			split_len = nMax;` |
|      1 | 7192 | `		}` |
|      - | 7193 | `		/* Copy the current chunk */` |
|     39 | 7194 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 7195 | `		/* Insert it */` |
|     39 | 7196 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|    ! 0 | 7197 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 7198 | `		}` |
|      - | 7199 | `		/* reset the string cursor */` |
|     39 | 7200 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 7201 | `		/* Update position */` |
|     39 | 7202 | `		zString += split_len;` |
|      1 | 7203 | `	}` |
|      - | 7204 | `	/*` |
|      - | 7205 | `	 * Return the array.` |
|      - | 7206 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 7207 | `	 * upon we return from this function.` |
|      - | 7208 | `	 */` |
|     21 | 7209 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 7210 | `	return PH7_OK;` |
|     15 | 7211 | `}` |
|      - | 7212 | `/*` |
|      - | 7213 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 7214 | ` * Refer to [strspn()].` |
|      - | 7215 | ` */` |
|     28 | 7216 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 7217 | `{` |
|     29 | 7218 | `	const char *zIn = *pzIn;` |
|      - | 7219 | `	const char *zPtr;` |
|      - | 7220 | `	/* Ignore leading white spaces */` |
|     29 | 7221 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 7222 | `		zIn++;` |
|    ! 0 | 7223 | `	}` |
|     29 | 7224 | `	if( zIn >= zEnd ){` |
|      - | 7225 | `		/* End of input */` |
|    ! 0 | 7226 | `		return SXERR_EOF;` |
|      - | 7227 | `	}` |
|     29 | 7228 | `	zPtr = zIn;` |
|      - | 7229 | `	/* Extract the token */` |
|    201 | 7230 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 7231 | `		zIn++;` |
|      1 | 7232 | `	}` |
|     29 | 7233 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 7234 | `	/* Synchronize pointers */` |
|     29 | 7235 | `	*pzIn = zIn;` |
|      - | 7236 | `	/* Return to the caller */` |
|     29 | 7237 | `	return SXRET_OK;` |
|     15 | 7238 | `}` |
|      - | 7239 | `/*` |
|      - | 7240 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 7241 | ` * return the longest match.` |
|      - | 7242 | ` * Refer to [strspn()].` |
|      - | 7243 | ` */` |
|     18 | 7244 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 7245 | `{` |
|     19 | 7246 | `	const char *zEnd = &zString[nLen];` |
|     19 | 7247 | `	const char *zIn = zString;` |
|      - | 7248 | `	int i,c;` |
|     45 | 7249 | `	for(;;){` |
|     91 | 7250 | `		if( zString >= zEnd ){` |
|      7 | 7251 | `			break;` |
|      - | 7252 | `		}` |
|      - | 7253 | `		/* Extract current character */` |
|     85 | 7254 | `		c = zString[0];` |
|      - | 7255 | `		/* Perform the lookup */` |
|    383 | 7256 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 7257 | `			if( c == zMask[i] ){` |
|      - | 7258 | `				/* Character found */` |
|     73 | 7259 | `				break;` |
|      - | 7260 | `			}` |
|    150 | 7261 | `		}` |
|     85 | 7262 | `		if( i >= nMaskLen ){` |
|      - | 7263 | `			/* Character not in the current mask,break immediately */` |
|     13 | 7264 | `			break;` |
|      - | 7265 | `		}` |
|      - | 7266 | `		/* Advance cursor */` |
|     73 | 7267 | `		zString++;` |
|      1 | 7268 | `	}` |
|      - | 7269 | `	/* Longest match */` |
|     19 | 7270 | `	return (int)(zString-zIn);` |
|      1 | 7271 | `}` |
|      - | 7272 | `/*` |
|      - | 7273 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 7274 | ` * Refer to [strcspn()].` |
|      - | 7275 | ` */` |
|     10 | 7276 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 7277 | `{` |
|     11 | 7278 | `	const char *zEnd = &zString[nLen];` |
|     11 | 7279 | `	const char *zIn = zString;` |
|      - | 7280 | `	int i,c;` |
|     12 | 7281 | `	for(;;){` |
|     25 | 7282 | `		if( zString >= zEnd ){` |
|      3 | 7283 | `			break;` |
|      - | 7284 | `		}` |
|      - | 7285 | `		/* Extract current character */` |
|     23 | 7286 | `		c = zString[0];` |
|      - | 7287 | `		/* Perform the lookup */` |
|     51 | 7288 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 7289 | `			if( c == zMask[i] ){` |
|      9 | 7290 | `				break;` |
|      - | 7291 | `			}` |
|     15 | 7292 | `		}` |
|     23 | 7293 | `		if( i < nMaskLen ){` |
|      - | 7294 | `			/* Character in the current mask,break immediately */` |
|      9 | 7295 | `			break;` |
|      - | 7296 | `		}` |
|      - | 7297 | `		/* Advance cursor */` |
|     15 | 7298 | `		zString++;` |
|      1 | 7299 | `	}` |
|      - | 7300 | `	/* Longest match */` |
|     11 | 7301 | `	return (int)(zString-zIn);` |
|      1 | 7302 | `}` |
|      - | 7303 | `/*` |
|      - | 7304 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 7305 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 7306 | ` *  of characters contained within a given mask.` |
|      - | 7307 | ` * Parameters` |
|      - | 7308 | ` * $str` |
|      - | 7309 | ` *  The input string.` |
|      - | 7310 | ` * $mask` |
|      - | 7311 | ` *  The list of allowable characters.` |
|      - | 7312 | ` * $start` |
|      - | 7313 | ` *  The position in subject to start searching.` |
|      - | 7314 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 7315 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 7316 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 7317 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 7318 | ` *  start'th position from the end of subject.` |
|      - | 7319 | ` * $length` |
|      - | 7320 | ` *  The length of the segment from subject to examine.` |
|      - | 7321 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 7322 | ` *  characters after the starting position.` |
|      - | 7323 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 7324 | ` *  position up to length characters from the end of subject.` |
|      - | 7325 | ` * Return` |
|      - | 7326 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 7327 | ` * in mask.` |
|      - | 7328 | ` */` |
|     24 | 7329 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7330 | `{` |
|      - | 7331 | `	const char *zString,*zMask,*zEnd;` |
|      - | 7332 | `	int iMasklen,iLen;` |
|      - | 7333 | `	SyString sToken;` |
|     25 | 7334 | `	int iCount = 0;` |
|      - | 7335 | `	int rc;` |
|     25 | 7336 | `	if( nArg < 2 ){` |
|      - | 7337 | `		/* Missing agruments,return zero */` |
|    ! 0 | 7338 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7339 | `		return PH7_OK;` |
|      - | 7340 | `	}` |
|      - | 7341 | `	/* Extract the target string */` |
|     25 | 7342 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7343 | `	/* Extract the mask */` |
|     25 | 7344 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 7345 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 7346 | `		/* Nothing to process,return zero */` |
|      7 | 7347 | `		ph7_result_int(pCtx,0);` |
|      7 | 7348 | `		return PH7_OK;` |
|      - | 7349 | `	}` |
|     19 | 7350 | `	if( nArg > 2 ){` |
|      - | 7351 | `		int nOfft;` |
|      - | 7352 | `		/* Extract the offset */` |
|      9 | 7353 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 7354 | `		if( nOfft < 0 ){` |
|    ! 0 | 7355 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 7356 | `			if( zBase > zString ){` |
|    ! 0 | 7357 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 7358 | `				zString = zBase;` |
|    ! 0 | 7359 | `			}else{` |
|      - | 7360 | `				/* Invalid offset */` |
|    ! 0 | 7361 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7362 | `				return PH7_OK;` |
|      - | 7363 | `			}` |
|    ! 0 | 7364 | `		}else{` |
|      9 | 7365 | `			if( nOfft >= iLen ){` |
|      - | 7366 | `				/* Invalid offset */` |
|    ! 0 | 7367 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7368 | `				return PH7_OK;` |
|    ! 0 | 7369 | `			}else{` |
|      - | 7370 | `				/* Update offset */` |
|      9 | 7371 | `				zString += nOfft;` |
|      9 | 7372 | `				iLen -= nOfft;` |
|      - | 7373 | `			}` |
|      - | 7374 | `		}` |
|      9 | 7375 | `		if( nArg > 3 ){` |
|      - | 7376 | `			int iUserlen;` |
|      - | 7377 | `			/* Extract the desired length */` |
|      9 | 7378 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 7379 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 7380 | `				iLen = iUserlen;` |
|      2 | 7381 | `			}` |
|      4 | 7382 | `		}` |
|      4 | 7383 | `	}` |
|      - | 7384 | `	/* Point to the end of the string */` |
|     19 | 7385 | `	zEnd = &zString[iLen];` |
|      - | 7386 | `	/* Extract the first non-space token */` |
|     19 | 7387 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 7388 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 7389 | `		/* Compare against the current mask */` |
|     19 | 7390 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 7391 | `	}` |
|      - | 7392 | `	/* Longest match */` |
|     19 | 7393 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 7394 | `	return PH7_OK;` |
|     13 | 7395 | `}` |
|      - | 7396 | `/*` |
|      - | 7397 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 7398 | ` *  Find length of initial segment not matching mask.` |
|      - | 7399 | ` * Parameters` |
|      - | 7400 | ` * $str` |
|      - | 7401 | ` *  The input string.` |
|      - | 7402 | ` * $mask` |
|      - | 7403 | ` *  The list of not allowed characters.` |
|      - | 7404 | ` * $start` |
|      - | 7405 | ` *  The position in subject to start searching.` |
|      - | 7406 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 7407 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 7408 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 7409 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 7410 | ` *  start'th position from the end of subject.` |
|      - | 7411 | ` * $length` |
|      - | 7412 | ` *  The length of the segment from subject to examine.` |
|      - | 7413 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 7414 | ` *  characters after the starting position.` |
|      - | 7415 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 7416 | ` *  position up to length characters from the end of subject.` |
|      - | 7417 | ` * Return` |
|      - | 7418 | ` *  Returns the length of the segment as an integer.` |
|      - | 7419 | ` */` |
|     14 | 7420 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7421 | `{` |
|      - | 7422 | `	const char *zString,*zMask,*zEnd;` |
|      - | 7423 | `	int iMasklen,iLen;` |
|      - | 7424 | `	SyString sToken;` |
|     15 | 7425 | `	int iCount = 0;` |
|      - | 7426 | `	int rc;` |
|     15 | 7427 | `	if( nArg < 2 ){` |
|      - | 7428 | `		/* Missing agruments,return zero */` |
|    ! 0 | 7429 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7430 | `		return PH7_OK;` |
|      - | 7431 | `	}` |
|      - | 7432 | `	/* Extract the target string */` |
|     15 | 7433 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7434 | `	/* Extract the mask */` |
|     15 | 7435 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 7436 | `	if( iLen < 1 ){` |
|      - | 7437 | `		/* Nothing to process,return zero */` |
|    ! 0 | 7438 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7439 | `		return PH7_OK;` |
|      - | 7440 | `	}` |
|     15 | 7441 | `	if( iMasklen < 1 ){` |
|      - | 7442 | `		/* No given mask,return the string length */` |
|      3 | 7443 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 7444 | `		return PH7_OK;` |
|      - | 7445 | `	}` |
|     13 | 7446 | `	if( nArg > 2 ){` |
|      - | 7447 | `		int nOfft;` |
|      - | 7448 | `		/* Extract the offset */` |
|     11 | 7449 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 7450 | `		if( nOfft < 0 ){` |
|    ! 0 | 7451 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 7452 | `			if( zBase > zString ){` |
|    ! 0 | 7453 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 7454 | `				zString = zBase;` |
|    ! 0 | 7455 | `			}else{` |
|      - | 7456 | `				/* Invalid offset */` |
|    ! 0 | 7457 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7458 | `				return PH7_OK;` |
|      - | 7459 | `			}` |
|    ! 0 | 7460 | `		}else{` |
|     11 | 7461 | `			if( nOfft >= iLen ){` |
|      - | 7462 | `				/* Invalid offset */` |
|      3 | 7463 | `				ph7_result_int(pCtx,0);` |
|      3 | 7464 | `				return PH7_OK;` |
|    ! 0 | 7465 | `			}else{` |
|      - | 7466 | `				/* Update offset */` |
|      9 | 7467 | `				zString += nOfft;` |
|      9 | 7468 | `				iLen -= nOfft;` |
|      - | 7469 | `			}` |
|      - | 7470 | `		}` |
|      9 | 7471 | `		if( nArg > 3 ){` |
|      - | 7472 | `			int iUserlen;` |
|      - | 7473 | `			/* Extract the desired length */` |
|    ! 0 | 7474 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 7475 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 7476 | `				iLen = iUserlen;` |
|    ! 0 | 7477 | `			}` |
|    ! 0 | 7478 | `		}` |
|      4 | 7479 | `	}` |
|      - | 7480 | `	/* Point to the end of the string */` |
|     11 | 7481 | `	zEnd = &zString[iLen];` |
|      - | 7482 | `	/* Extract the first non-space token */` |
|     11 | 7483 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 7484 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 7485 | `		/* Compare against the current mask */` |
|     11 | 7486 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 7487 | `	}` |
|      - | 7488 | `	/* Longest match */` |
|     11 | 7489 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 7490 | `	return PH7_OK;` |
|      8 | 7491 | `}` |
|      - | 7492 | `/*` |
|      - | 7493 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 7494 | ` *  Search a string for any of a set of characters.` |
|      - | 7495 | ` * Parameters` |
|      - | 7496 | ` *  $haystack` |
|      - | 7497 | ` *   The string where char_list is looked for.` |
|      - | 7498 | ` *  $char_list` |
|      - | 7499 | ` *   This parameter is case sensitive.` |
|      - | 7500 | ` * Return` |
|      - | 7501 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 7502 | ` */` |
|      4 | 7503 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7504 | `{` |
|      - | 7505 | `	const char *zString,*zList,*zEnd;` |
|      - | 7506 | `	int iLen,iListLen,i,c;` |
|      - | 7507 | `	sxu32 nOfft,nMax;` |
|      - | 7508 | `	sxi32 rc;` |
|      5 | 7509 | `	if( nArg < 2 ){` |
|      - | 7510 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7511 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7512 | `		return PH7_OK;` |
|      - | 7513 | `	}` |
|      - | 7514 | `	/* Extract the haystack and the char list */` |
|      5 | 7515 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 7516 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 7517 | `	if( iLen < 1 ){` |
|      - | 7518 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7519 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7520 | `		return PH7_OK;` |
|      - | 7521 | `	}` |
|      - | 7522 | `	/* Point to the end of the string */` |
|      5 | 7523 | `	zEnd = &zString[iLen];` |
|      5 | 7524 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 7525 | `	/* perform the requested operation */` |
|     15 | 7526 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 7527 | `		c = zList[i];` |
|     11 | 7528 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 7529 | `		if( rc == SXRET_OK ){` |
|      5 | 7530 | `			if( nMax < nOfft ){` |
|      3 | 7531 | `				nOfft = nMax;` |
|      1 | 7532 | `			}` |
|      2 | 7533 | `		}` |
|      6 | 7534 | `	}` |
|      5 | 7535 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 7536 | `		/* No such substring,return FALSE */` |
|      3 | 7537 | `		ph7_result_bool(pCtx,0);` |
|      2 | 7538 | `	}else{` |
|      - | 7539 | `		/* Return the substring */` |
|      3 | 7540 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 7541 | `	}` |
|      5 | 7542 | `	return PH7_OK;` |
|      3 | 7543 | `}` |
|      - | 7544 | `/* SPDX-SnippetBegin */` |
|      - | 7545 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 7546 | `/* SPDX-License-Identifier: blessing */` |
|      - | 7547 | `/*` |
|      - | 7548 | ` * string soundex(string $str)` |
|      - | 7549 | ` *  Calculate the soundex key of a string.` |
|      - | 7550 | ` * Parameters` |
|      - | 7551 | ` *  $str` |
|      - | 7552 | ` *   The input string.` |
|      - | 7553 | ` * Return` |
|      - | 7554 | ` *  Returns the soundex key as a string.` |
|      - | 7555 | ` * Note:` |
|      - | 7556 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 7557 | ` * source tree.` |
|      - | 7558 | ` */` |
|     22 | 7559 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7560 | `{` |
|      - | 7561 | `	const unsigned char *zIn;` |
|      - | 7562 | `	char zResult[8];` |
|      - | 7563 | `	int i, j;` |
|      - | 7564 | `	static const unsigned char iCode[] = {` |
|      - | 7565 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7566 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7567 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7568 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7569 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 7570 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 7571 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 7572 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 7573 | `	};` |
|     23 | 7574 | `	if( nArg < 1 ){` |
|      - | 7575 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7576 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7577 | `		return PH7_OK;` |
|      - | 7578 | `	}` |
|     23 | 7579 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     35 | 7580 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     23 | 7581 | `	if( zIn[i] ){` |
|     17 | 7582 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 7583 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 7584 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 7585 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 7586 | `			if( code>0 ){` |
|     45 | 7587 | `				if( code!=prevcode ){` |
|     33 | 7588 | `					prevcode = (unsigned char)code;` |
|     33 | 7589 | `					zResult[j++] = (char)code + '0';` |
|     16 | 7590 | `				}` |
|     23 | 7591 | `			}else{` |
|     49 | 7592 | `				prevcode = 0;` |
|      - | 7593 | `			}` |
|     47 | 7594 | `		}` |
|     33 | 7595 | `		while( j<4 ){` |
|     17 | 7596 | `			zResult[j++] = '0';` |
|      1 | 7597 | `		}` |
|     17 | 7598 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 7599 | `	}else{` |
|      - | 7600 | `	  /* No alphabetic character: PHP returns "0000" (not the SQLite "?000"). */` |
|      7 | 7601 | `	  ph7_result_string(pCtx,"0000",4);` |
|      - | 7602 | `	}` |
|     23 | 7603 | `	return PH7_OK;` |
|     12 | 7604 | `}` |
|      - | 7605 | `/* SPDX-SnippetEnd */` |
|      - | 7606 | `/*` |
|      - | 7607 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 7608 | ` *  Wraps a string to a given number of characters.` |
|      - | 7609 | ` * Parameters` |
|      - | 7610 | ` *  $str` |
|      - | 7611 | ` *   The input string.` |
|      - | 7612 | ` * $width` |
|      - | 7613 | ` *  The column width.` |
|      - | 7614 | ` * $break` |
|      - | 7615 | ` *  The line is broken using the optional break parameter.` |
|      - | 7616 | ` * Return` |
|      - | 7617 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 7618 | ` */` |
|     26 | 7619 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7620 | `{` |
|      - | 7621 | `	const char *zIn,*zBreak;` |
|      - | 7622 | `	SyBlob sWorker;` |
|      - | 7623 | `	int iLen,iBreaklen,iWidth,iCut,iStart,iSpace,iCur;` |
|      - | 7624 | `	sxi32 rc;` |
|     27 | 7625 | `	if( nArg < 1 ){` |
|      - | 7626 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7627 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7628 | `		return PH7_OK;` |
|      - | 7629 | `	}` |
|      - | 7630 | `	/* Extract the input string */` |
|     27 | 7631 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7632 | `	/* Width (default 75; PHP allows 0/negative — break at every space). */` |
|     27 | 7633 | `	iWidth = 75;` |
|     27 | 7634 | `	if( nArg > 1 ){` |
|     27 | 7635 | `		iWidth = ph7_value_to_int(apArg[1]);` |
|     13 | 7636 | `	}` |
|      - | 7637 | `	/* Break string (default "\n"). */` |
|     27 | 7638 | `	zBreak = "\n";` |
|     27 | 7639 | `	iBreaklen = (int)sizeof(char);` |
|     27 | 7640 | `	if( nArg > 2 ){` |
|     13 | 7641 | `		zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      6 | 7642 | `	}` |
|      - | 7643 | `	/* Cut long words? (default false). */` |
|     27 | 7644 | `	iCut = 0;` |
|     27 | 7645 | `	if( nArg > 3 ){` |
|      7 | 7646 | `		iCut = ph7_value_to_bool(apArg[3]);` |
|      3 | 7647 | `	}` |
|     27 | 7648 | `	if( iLen < 1 ){` |
|      - | 7649 | `		/* PHP returns the empty string for empty input before validating the other args. */` |
|      5 | 7650 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 7651 | `		return PH7_OK;` |
|      - | 7652 | `	}` |
|      - | 7653 | `	/* PHP 8 domain errors (catchable ValueError). */` |
|     23 | 7654 | `	if( iBreaklen < 1 ){` |
|      3 | 7655 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7656 | `			"wordwrap(): Argument #3 ($break) must not be empty");` |
|      - | 7657 | `	}` |
|     21 | 7658 | `	if( iWidth == 0 && iCut ){` |
|      3 | 7659 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7660 | `			"wordwrap(): Argument #4 ($cut_long_words) cannot be true when argument #2 ($width) is 0");` |
|      - | 7661 | `	}` |
|      - | 7662 | `	/*` |
|      - | 7663 | `	 * PHP's algorithm: a single left-to-right pass tracking the start of the` |
|      - | 7664 | `	 * current line (iStart) and the position of the last space seen on it` |
|      - | 7665 | `	 * (iSpace). A break is emitted when the line reaches the width, at the last` |
|      - | 7666 | `	 * space if there was one, otherwise (only when cut is enabled) hard at the` |
|      - | 7667 | `	 * boundary. An existing break sequence in the input resets the line.` |
|      - | 7668 | `	 */` |
|     19 | 7669 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     19 | 7670 | `	iStart = iSpace = iCur = 0;` |
|     19 | 7671 | `	rc = SXRET_OK;` |
|    551 | 7672 | `	while( iCur < iLen ){` |
|    533 | 7673 | `		if( iBreaklen <= iLen - iCur && SyMemcmp(&zIn[iCur],zBreak,(sxu32)iBreaklen) == 0 ){` |
|      - | 7674 | `			/* Existing break sequence in the input: copy it verbatim and reset the line. */` |
|    ! 0 | 7675 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart + iBreaklen));` |
|    ! 0 | 7676 | `			if( rc != SXRET_OK ){ goto oom; }` |
|    ! 0 | 7677 | `			iCur += iBreaklen;` |
|    ! 0 | 7678 | `			iStart = iSpace = iCur;` |
|    ! 0 | 7679 | `			continue;` |
|    533 | 7680 | `		}else if( zIn[iCur] == ' ' ){` |
|     67 | 7681 | `			if( iCur - iStart >= iWidth ){` |
|      - | 7682 | `				/* The line already fills the width at this space: break here (the space is consumed). */` |
|     13 | 7683 | `				rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     13 | 7684 | `				if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     13 | 7685 | `				if( rc != SXRET_OK ){ goto oom; }` |
|     13 | 7686 | `				iStart = iCur + 1;` |
|      6 | 7687 | `			}` |
|     67 | 7688 | `			iSpace = iCur;` |
|    500 | 7689 | `		}else if( iCut && iCur - iStart >= iWidth && iStart >= iSpace ){` |
|      - | 7690 | `			/* A word longer than the width with no space to break at: hard-cut at the boundary. */` |
|      7 | 7691 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|      7 | 7692 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|      7 | 7693 | `			if( rc != SXRET_OK ){ goto oom; }` |
|      7 | 7694 | `			iStart = iSpace = iCur;` |
|    464 | 7695 | `		}else if( iCur - iStart >= iWidth && iStart < iSpace ){` |
|      - | 7696 | `			/* Past the width mid-word: wrap back to the last space (which is consumed). */` |
|     17 | 7697 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iSpace - iStart));` |
|     17 | 7698 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     17 | 7699 | `			if( rc != SXRET_OK ){ goto oom; }` |
|     17 | 7700 | `			iStart = iSpace = iSpace + 1;` |
|      8 | 7701 | `		}` |
|    533 | 7702 | `		iCur++;` |
|      1 | 7703 | `	}` |
|      - | 7704 | `	/* Emit the trailing chunk. */` |
|     19 | 7705 | `	if( iStart < iCur ){` |
|     19 | 7706 | `		rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     19 | 7707 | `		if( rc != SXRET_OK ){ goto oom; }` |
|      9 | 7708 | `	}` |
|     19 | 7709 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|     19 | 7710 | `	SyBlobRelease(&sWorker);` |
|     19 | 7711 | `	return PH7_OK;` |
|    ! 0 | 7712 | `oom:` |
|    ! 0 | 7713 | `	SyBlobRelease(&sWorker);` |
|    ! 0 | 7714 | `	return PH7_ContextMemoryError(pCtx);` |
|     14 | 7715 | `}` |
|      - | 7716 | `/*` |
|      - | 7717 | ` * Check if the given character is a member of the given mask.` |
|      - | 7718 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 7719 | ` * Refer to [strtok()].` |
|      - | 7720 | ` */` |
|     30 | 7721 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 7722 | `{` |
|      - | 7723 | `	int i;` |
|     57 | 7724 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 7725 | `		if( c == zMask[i] ){` |
|     13 | 7726 | `			if( pOfft ){` |
|      5 | 7727 | `				*pOfft = i;` |
|      2 | 7728 | `			}` |
|     13 | 7729 | `			return TRUE;` |
|      - | 7730 | `		}` |
|     14 | 7731 | `	}` |
|     19 | 7732 | `	return FALSE;` |
|     16 | 7733 | `}` |
|      - | 7734 | `/*` |
|      - | 7735 | ` * Extract a single token from the input stream.` |
|      - | 7736 | ` * Refer to [strtok()].` |
|      - | 7737 | ` */` |
|      6 | 7738 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 7739 | `{` |
|      7 | 7740 | `	const char *zIn = *pzIn;` |
|      - | 7741 | `	const char *zPtr;` |
|      - | 7742 | `	/* Ignore leading delimiter */` |
|     11 | 7743 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 7744 | `		zIn++;` |
|      1 | 7745 | `	}` |
|      7 | 7746 | `	if( zIn >= zEnd ){` |
|      - | 7747 | `		/* End of input */` |
|    ! 0 | 7748 | `		return SXERR_EOF;` |
|      - | 7749 | `	}` |
|      7 | 7750 | `	zPtr = zIn;` |
|      - | 7751 | `	/* Extract the token */` |
|     13 | 7752 | `	while( zIn < zEnd ){` |
|     11 | 7753 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 7754 | `			/* UTF-8 stream */` |
|    ! 0 | 7755 | `			zIn++;` |
|    ! 0 | 7756 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 7757 | `		}else{` |
|     11 | 7758 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 7759 | `				break;` |
|      - | 7760 | `			}` |
|      7 | 7761 | `			zIn++;` |
|      - | 7762 | `		}` |
|      1 | 7763 | `	}` |
|      7 | 7764 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 7765 | `	/* Update the cursor */` |
|      7 | 7766 | `	*pzIn = zIn;` |
|      - | 7767 | `	/* Return to the caller */` |
|      7 | 7768 | `	return SXRET_OK;` |
|      4 | 7769 | `}` |
|      - | 7770 | `/* strtok auxiliary private data */` |
|      - | 7771 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 7772 | `struct strtok_aux_data` |
|      - | 7773 | `{` |
|      - | 7774 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 7775 | `	const char *zIn;   /* Current input stream */` |
|      - | 7776 | `	const char *zEnd;  /* End of input */` |
|      - | 7777 | `};` |
|      - | 7778 | `/*` |
|      - | 7779 | ` * string strtok(string $str,string $token)` |
|      - | 7780 | ` * string strtok(string $token)` |
|      - | 7781 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 7782 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 7783 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 7784 | ` *  words by using the space character as the token.` |
|      - | 7785 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 7786 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 7787 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 7788 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 7789 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 7790 | ` *  the argument are found.` |
|      - | 7791 | ` * Parameters` |
|      - | 7792 | ` *  $str` |
|      - | 7793 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 7794 | ` * $token` |
|      - | 7795 | ` *  The delimiter used when splitting up str.` |
|      - | 7796 | ` * Return` |
|      - | 7797 | ` *   Current token or FALSE on EOF.` |
|      - | 7798 | ` */` |
|      6 | 7799 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7800 | `{` |
|      - | 7801 | `	strtok_aux_data *pAux;` |
|      - | 7802 | `	const char *zMask;` |
|      - | 7803 | `	SyString sToken;` |
|      - | 7804 | `	int nMasklen;` |
|      - | 7805 | `	sxi32 rc;` |
|      7 | 7806 | `	if( nArg < 2 ){` |
|      - | 7807 | `		/* Extract top aux data */` |
|      5 | 7808 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      5 | 7809 | `		if( pAux == 0 ){` |
|      - | 7810 | `			/* No aux data,return FALSE */` |
|    ! 0 | 7811 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7812 | `			return PH7_OK;` |
|      - | 7813 | `		}` |
|      5 | 7814 | `		nMasklen = 0;` |
|      5 | 7815 | `		zMask = ""; /* cc warning */` |
|      5 | 7816 | `		if( nArg > 0 ){` |
|      - | 7817 | `			/* Extract the mask */` |
|      5 | 7818 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 7819 | `		}` |
|      5 | 7820 | `		if( nMasklen < 1 ){` |
|      - | 7821 | `			/* Invalid mask,return FALSE */` |
|    ! 0 | 7822 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 7823 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7824 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 7825 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7826 | `			return PH7_OK;` |
|      - | 7827 | `		}` |
|      - | 7828 | `		/* Extract the token */` |
|      5 | 7829 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 7830 | `		if( rc != SXRET_OK ){` |
|      - | 7831 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 7832 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 7833 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7834 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 7835 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7836 | `		}else{` |
|      - | 7837 | `			/* Return the extracted token */` |
|      5 | 7838 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 7839 | `		}` |
|      3 | 7840 | `	}else{` |
|      - | 7841 | `		const char *zInput,*zCur;` |
|      - | 7842 | `		char *zDup;` |
|      - | 7843 | `		int nLen;` |
|      - | 7844 | `		/* Extract the raw input */` |
|      3 | 7845 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 7846 | `		if( nLen < 1 ){` |
|      - | 7847 | `			/* Empty input,return FALSE */` |
|    ! 0 | 7848 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7849 | `			return PH7_OK;` |
|      - | 7850 | `		}` |
|      - | 7851 | `		/* Extract the mask */` |
|      3 | 7852 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 7853 | `		if( nMasklen < 1 ){` |
|      - | 7854 | `			/* Set a default mask */` |
|      - | 7855 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 7856 | `			zMask = TOK_MASK;` |
|    ! 0 | 7857 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 7858 | `#undef TOK_MASK` |
|    ! 0 | 7859 | `		}` |
|      - | 7860 | `		/* Extract a single token */` |
|      3 | 7861 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 7862 | `		if( rc != SXRET_OK ){` |
|      - | 7863 | `			/* Empty input */` |
|    ! 0 | 7864 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7865 | `			return PH7_OK;` |
|    ! 0 | 7866 | `		}else{` |
|      - | 7867 | `			/* Return the extracted token */` |
|      3 | 7868 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 7869 | `		}` |
|      - | 7870 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 7871 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 7872 | `		if( pAux ){` |
|      3 | 7873 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 7874 | `			if( nLen < 1 ){` |
|    ! 0 | 7875 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7876 | `				return PH7_OK;` |
|      - | 7877 | `			}` |
|      - | 7878 | `			/* Duplicate input */` |
|      3 | 7879 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 7880 | `			if( zDup  ){` |
|      3 | 7881 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 7882 | `				/* Register the aux data */` |
|      3 | 7883 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 7884 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 7885 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 7886 | `			}` |
|      1 | 7887 | `		}` |
|      - | 7888 | `	}` |
|      7 | 7889 | `	return PH7_OK;` |
|      4 | 7890 | `}` |
|      - | 7891 | `/*` |
|      - | 7892 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 7893 | ` *  Pad a string to a certain length with another string` |
|      - | 7894 | ` * Parameters` |
|      - | 7895 | ` *  $input` |
|      - | 7896 | ` *   The input string.` |
|      - | 7897 | ` * $pad_length` |
|      - | 7898 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 7899 | ` *   string, no padding takes place.` |
|      - | 7900 | ` * $pad_string` |
|      - | 7901 | ` *   Note:` |
|      - | 7902 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 7903 | ` *    divided by the pad_string's length.` |
|      - | 7904 | ` * $pad_type` |
|      - | 7905 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 7906 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 7907 | ` * Return` |
|      - | 7908 | ` *  The padded string.` |
|      - | 7909 | ` */` |
|     10 | 7910 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7911 | `{` |
|      - | 7912 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 7913 | `	const char *zIn,*zPad;` |
|     11 | 7914 | `	if( nArg < 2 ){` |
|      - | 7915 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7916 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7917 | `		return PH7_OK;` |
|      - | 7918 | `	}` |
|      - | 7919 | `	/* Extract the target string */` |
|     11 | 7920 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7921 | `	/* Padding length */` |
|      - | 7922 | `	{` |
|     11 | 7923 | `		sxi64 iTmp = 0;` |
|     11 | 7924 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_pad",2,"$length","int",&iTmp);` |
|     11 | 7925 | `		if( rcArg != PH7_OK ){` |
|    ! 0 | 7926 | `			return rcArg;` |
|      - | 7927 | `		}` |
|     11 | 7928 | `		iRealPad = iPadlen = (int)iTmp;` |
|      - | 7929 | `	}` |
|     11 | 7930 | `	if( iPadlen > 0 ){` |
|      9 | 7931 | `		iPadlen -= iLen;` |
|      4 | 7932 | `	}` |
|     11 | 7933 | `	if( iPadlen < 1  ){` |
|      - | 7934 | `		/* Return the string verbatim */` |
|      5 | 7935 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      5 | 7936 | `		return PH7_OK;` |
|      - | 7937 | `	}` |
|      7 | 7938 | `	zPad = " "; /* Whitespace padding */` |
|      7 | 7939 | `	iStrpad = (int)sizeof(char);` |
|      7 | 7940 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      7 | 7941 | `	if( nArg > 2 ){` |
|      - | 7942 | `		/* Padding string */` |
|      7 | 7943 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      7 | 7944 | `		if( iStrpad < 1 ){` |
|      - | 7945 | `			/* An empty pad string throws a catchable ValueError in PHP 8` |
|      - | 7946 | `			 * (only reached once padding is actually required). */` |
|      3 | 7947 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7948 | `				"str_pad(): Argument #3 ($pad_string) must not be empty");` |
|      - | 7949 | `		}` |
|      5 | 7950 | `		if( nArg > 3 ){` |
|      - | 7951 | `			/* Padd type */` |
|      5 | 7952 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 7953 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 7954 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 7955 | `			}` |
|      2 | 7956 | `		}` |
|      2 | 7957 | `	}` |
|      5 | 7958 | `	iDiv = 1;` |
|      5 | 7959 | `	if( iType == 2 ){` |
|    ! 0 | 7960 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 7961 | `	}` |
|      - | 7962 | `	/* Perform the requested operation */` |
|      5 | 7963 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 7964 | `		jPad = iStrpad;` |
|      5 | 7965 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 7966 | `			/* Padding */` |
|      5 | 7967 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 7968 | `				break;` |
|      - | 7969 | `			}` |
|      3 | 7970 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7971 | `		}` |
|      3 | 7972 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 7973 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 7974 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 7975 | `				if( jPad > iStrpad ){` |
|    ! 0 | 7976 | `					jPad = iStrpad;` |
|    ! 0 | 7977 | `				}` |
|      3 | 7978 | `				if( jPad < 1){` |
|    ! 0 | 7979 | `					break;` |
|      - | 7980 | `				}` |
|      3 | 7981 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 7982 | `			}` |
|      1 | 7983 | `		}` |
|      1 | 7984 | `	}` |
|      5 | 7985 | `	if( iLen > 0 ){` |
|      - | 7986 | `		/* Append the input string */` |
|      5 | 7987 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7988 | `	}` |
|      5 | 7989 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 7990 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 7991 | `			/* Padding */` |
|      5 | 7992 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 7993 | `				break;` |
|      - | 7994 | `			}` |
|      3 | 7995 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7996 | `		}` |
|      5 | 7997 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 7998 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 7999 | `			if( jPad > iStrpad ){` |
|    ! 0 | 8000 | `				jPad = iStrpad;` |
|    ! 0 | 8001 | `			}` |
|      3 | 8002 | `			if( jPad < 1){` |
|    ! 0 | 8003 | `				break;` |
|      - | 8004 | `			}` |
|      3 | 8005 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 8006 | `		}` |
|      1 | 8007 | `	}` |
|      5 | 8008 | `	return PH7_OK;` |
|      6 | 8009 | `}` |
|      - | 8010 | `/*` |
|      - | 8011 | ` * String replacement private data.` |
|      - | 8012 | ` */` |
|      - | 8013 | `typedef struct str_replace_data str_replace_data;` |
|      - | 8014 | `struct str_replace_data` |
|      - | 8015 | `{` |
|      - | 8016 | `	/* Used by the str_replace family to collect the search/replace arguments. */` |
|      - | 8017 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 8018 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 8019 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 8020 | `};` |
|      - | 8021 | `/*` |
|      - | 8022 | ` * Remove a substring.` |
|      - | 8023 | ` */` |
|      - | 8024 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 8025 | `	for(;;){\` |
|      - | 8026 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 8027 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 8028 | `		++OFFT;\` |
|      - | 8029 | `	}\` |
|      - | 8030 | `}` |
|      - | 8031 | `/*` |
|      - | 8032 | ` * Shift right and insert algorithm.` |
|      - | 8033 | ` */` |
|      - | 8034 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 8035 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 8036 | `		for(;;){\` |
|      - | 8037 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 8038 | `			if(INLEN < 1 ) { break; }\` |
|      - | 8039 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 8040 | `			--INLEN; \` |
|      - | 8041 | `		}\` |
|      - | 8042 | `		for(;;){\` |
|      - | 8043 | `				if(ELEN < 1) { break; }\` |
|      - | 8044 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 8045 | `				OFFT++;\` |
|      - | 8046 | `				ENTRY++;\` |
|      - | 8047 | `				--ELEN;\` |
|      - | 8048 | `		}\` |
|      - | 8049 | `}` |
|      - | 8050 | `/*` |
|      - | 8051 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 8052 | ` * replacement string [i.e: zReplace].` |
|      - | 8053 | ` */` |
|     54 | 8054 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      5 | 8055 | `{` |
|     59 | 8056 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 8057 | `	sxu32 n,m;` |
|     59 | 8058 | `	n = SyBlobLength(pWorker);` |
|     59 | 8059 | `	m = nOfft;` |
|      - | 8060 | `	/* Delete the old entry */` |
|   6689 | 8061 | `	STRDEL(zInput,n,m,nLen);` |
|     59 | 8062 | `	SyBlobLength(pWorker) -= nLen;` |
|     59 | 8063 | `	if( nReplen > 0 ){` |
|     53 | 8064 | `		sxi32 iRep = nReplen;` |
|      - | 8065 | `		sxi32 rc;` |
|      - | 8066 | `		/*` |
|      - | 8067 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 8068 | `		 * string.` |
|      - | 8069 | `		 */` |
|     53 | 8070 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     53 | 8071 | `		if( rc != SXRET_OK ){` |
|      - | 8072 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 8073 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 8074 | `			return rc;` |
|      - | 8075 | `		}` |
|      - | 8076 | `		/* Perform the insertion now */` |
|     53 | 8077 | `		zInput = (char *)SyBlobData(pWorker);` |
|     53 | 8078 | `		n = SyBlobLength(pWorker);` |
|   6481 | 8079 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     53 | 8080 | `		SyBlobLength(pWorker) += nReplen;` |
|     24 | 8081 | `	}` |
|     59 | 8082 | `	return SXRET_OK;` |
|     32 | 8083 | `}` |
|      - | 8084 | `/*` |
|      - | 8085 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 8086 | ` * to collect search/replace string.` |
|      - | 8087 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 8088 | ` */` |
|     98 | 8089 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      5 | 8090 | `{` |
|    103 | 8091 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 8092 | `	SyString sWorker;` |
|      - | 8093 | `	const char *zIn;` |
|      - | 8094 | `	int nByte;` |
|      - | 8095 | `	/* Extract a string representation of the given argument */` |
|    103 | 8096 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|    103 | 8097 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|    103 | 8098 | `	if( nByte > 0 ){` |
|      - | 8099 | `		char *zDup;` |
|      - | 8100 | `		/* Duplicate the chunk */` |
|    101 | 8101 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 8102 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 8103 | `			);` |
|    101 | 8104 | `		if( zDup == 0 ){` |
|      - | 8105 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 8106 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 8107 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 8108 | `			return SXERR_MEM;` |
|      - | 8109 | `		}` |
|    101 | 8110 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 8111 | `		/* Save the chunk */` |
|    101 | 8112 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     48 | 8113 | `	}` |
|      - | 8114 | `	/* Save for later processing */` |
|    103 | 8115 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 8116 | `	/* All done */` |
|     49 | 8117 | `	SXUNUSED(pKey); /* cc warning */` |
|    103 | 8118 | `	return PH7_OK;` |
|     54 | 8119 | `}` |
|      - | 8120 | `/*` |
|      - | 8121 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 8122 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 8123 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 8124 | ` * Parameters` |
|      - | 8125 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 8126 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 8127 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 8128 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 8129 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 8130 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 8131 | ` * $search` |
|      - | 8132 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 8133 | ` *  to designate multiple needles.` |
|      - | 8134 | ` * $replace` |
|      - | 8135 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 8136 | ` *  to designate multiple replacements.` |
|      - | 8137 | ` * $subject` |
|      - | 8138 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 8139 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 8140 | ` *  of subject, and the return value is an array as well.` |
|      - | 8141 | ` * $count (Not used)` |
|      - | 8142 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 8143 | ` * Return` |
|      - | 8144 | ` * This function returns a string or an array with the replaced values.` |
|      - | 8145 | ` */` |
|  29890 | 8146 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 8147 | `{` |
|      - | 8148 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 8149 | `	ProcStringMatch xMatch;` |
|      - | 8150 | `	const char *zIn,*zFunc;` |
|      - | 8151 | `	str_replace_data sRep;` |
|      - | 8152 | `	SyBlob sWorker;` |
|      - | 8153 | `	SySet sReplace;` |
|      - | 8154 | `	SySet sSearch;` |
|      - | 8155 | `	int rep_str;` |
|      - | 8156 | `	int nByte;` |
|      - | 8157 | `	sxi32 rc;` |
|  29895 | 8158 | `	if( nArg < 3 ){` |
|      - | 8159 | `		/* Missing/Invalid arguments,return null */` |
|    ! 0 | 8160 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8161 | `		return PH7_OK;` |
|      - | 8162 | `	}` |
|      - | 8163 | `	/* Initialize fields */` |
|  29895 | 8164 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  29895 | 8165 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  29895 | 8166 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  29895 | 8167 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  29895 | 8168 | `	sRep.pCtx = pCtx;` |
|  29895 | 8169 | `	sRep.pCollector = &sSearch;` |
|  29895 | 8170 | `	rep_str = 0;` |
|      - | 8171 | `	/* Extract the subject */` |
|  29895 | 8172 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  29895 | 8173 | `	if( nByte < 1 ){` |
|      - | 8174 | `		/* Nothing to replace,return the empty string */` |
|     21 | 8175 | `		ph7_result_string(pCtx,"",0);` |
|     21 | 8176 | `		return PH7_OK;` |
|      - | 8177 | `	}` |
|      - | 8178 | `	/* Copy the subject */` |
|  29875 | 8179 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 8180 | `	/* Search string */` |
|  29875 | 8181 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 8182 | `		/* Collect search string */` |
|     49 | 8183 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|     27 | 8184 | `	}else{` |
|      - | 8185 | `		/* Single pattern */` |
|  29831 | 8186 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  29831 | 8187 | `		if( nByte < 1 ){` |
|      - | 8188 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 8189 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 8190 | `			return PH7_OK;` |
|      - | 8191 | `		}` |
|  29827 | 8192 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 8193 | `		/* Save for later processing */` |
|  29827 | 8194 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 8195 | `	}` |
|      - | 8196 | `	/* Replace string */` |
|  29871 | 8197 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 8198 | `		/* Collect replace string */` |
|      7 | 8199 | `		sRep.pCollector = &sReplace;` |
|      7 | 8200 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 8201 | `	}else{` |
|      - | 8202 | `		/* Single needle */` |
|  29865 | 8203 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  29865 | 8204 | `		rep_str = 1;` |
|  29865 | 8205 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 8206 | `		/* Save for later processing */` |
|  29865 | 8207 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 8208 | `	}` |
|      - | 8209 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  29871 | 8210 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 8211 | `		SySetRelease(&sSearch);` |
|    ! 0 | 8212 | `		SySetRelease(&sReplace);` |
|    ! 0 | 8213 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 8214 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8215 | `	}` |
|      - | 8216 | `	/* Reset loop cursors */` |
|  29871 | 8217 | `	SySetResetCursor(&sSearch);` |
|  29871 | 8218 | `	SySetResetCursor(&sReplace);` |
|  29871 | 8219 | `	pReplace = pSearch = 0; /* cc warning */` |
|  29871 | 8220 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 8221 | `	/* Extract function name */` |
|  29871 | 8222 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 8223 | `	/* Set the default pattern match routine */` |
|  29871 | 8224 | `	xMatch = SyBlobSearch;` |
|  29871 | 8225 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 8226 | `		/* Case insensitive pattern match */` |
|     11 | 8227 | `		xMatch = iPatternMatch;` |
|      5 | 8228 | `	}` |
|      - | 8229 | `	/* Start the replace process */` |
|  59781 | 8230 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 8231 | `		sxu32 nCount,nOfft;` |
|  29915 | 8232 | `		if( pSearch->nByte <  1 ){` |
|      - | 8233 | `			/* Empty string,ignore */` |
|      3 | 8234 | `			continue;` |
|      - | 8235 | `		}` |
|      - | 8236 | `		/* Extract the replace string */` |
|  29913 | 8237 | `		if( rep_str ){` |
|  29903 | 8238 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  14954 | 8239 | `		}else{` |
|     11 | 8240 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 8241 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 8242 | `				 * An empty string is used for the rest of replacement values` |
|      - | 8243 | `				 */` |
|      3 | 8244 | `				pReplace = 0;` |
|      1 | 8245 | `			}` |
|      - | 8246 | `		}` |
|  29913 | 8247 | `		if( pReplace == 0 ){` |
|      - | 8248 | `			/* Use an empty string instead */` |
|      3 | 8249 | `			pReplace = &sTemp;` |
|      1 | 8250 | `		}` |
|  29913 | 8251 | `		nOfft = nCount = 0;` |
|  14981 | 8252 | `		for(;;){` |
|  29967 | 8253 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 8254 | `				break;` |
|      - | 8255 | `			}` |
|      - | 8256 | `			/* Perform a pattern lookup */` |
|  44930 | 8257 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  29950 | 8258 | `				pSearch->nByte,&nOfft);` |
|  29955 | 8259 | `			if( rc != SXRET_OK ){` |
|      - | 8260 | `				/* Pattern not found */` |
|  29901 | 8261 | `				break;` |
|      - | 8262 | `			}` |
|      - | 8263 | `			/* Perform the replace operation */` |
|     59 | 8264 | `			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|     59 | 8265 | `			if( rc != SXRET_OK ){` |
|      - | 8266 | `				/* Allocation failure: surface a fatal instead of a partial result */` |
|    ! 0 | 8267 | `				SySetRelease(&sSearch);` |
|    ! 0 | 8268 | `				SySetRelease(&sReplace);` |
|    ! 0 | 8269 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8270 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8271 | `			}` |
|      - | 8272 | `			/* Increment offset counter */` |
|     59 | 8273 | `			nCount += nOfft + pReplace->nByte;` |
|      5 | 8274 | `		}` |
|      5 | 8275 | `	}` |
|      - | 8276 | `	/* All done,clean-up the mess left behind */` |
|  29871 | 8277 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  29871 | 8278 | `	SySetRelease(&sSearch);` |
|  29871 | 8279 | `	SySetRelease(&sReplace);` |
|  29871 | 8280 | `	SyBlobRelease(&sWorker);` |
|  29871 | 8281 | `	if( rc != PH7_OK ){` |
|    ! 0 | 8282 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8283 | `	}` |
|  29871 | 8284 | `	return PH7_OK;` |
|  14950 | 8285 | `}` |
|      - | 8286 | `/*` |
|      - | 8287 | ` * strtr() array form: a single (key,value) pair copied out of the replace_pairs` |
|      - | 8288 | ` * array. The bytes are owned by a persistent pool (see strtr_collect) rather than` |
|      - | 8289 | ` * the transient walker values, which HashmapWalk releases after each callback, so` |
|      - | 8290 | ` * we store byte offsets into that pool instead of raw pointers.` |
|      - | 8291 | ` */` |
|      - | 8292 | `typedef struct strtr_entry strtr_entry;` |
|      - | 8293 | `struct strtr_entry` |
|      - | 8294 | `{` |
|      - | 8295 | `	sxu32 nKeyOfft; /* Offset of the search key inside the pool */` |
|      - | 8296 | `	sxu32 nKeyLen;  /* Length of the search key */` |
|      - | 8297 | `	sxu32 nValOfft; /* Offset of the replacement inside the pool */` |
|      - | 8298 | `	sxu32 nValLen;  /* Length of the replacement */` |
|      - | 8299 | `};` |
|      - | 8300 | `typedef struct strtr_collect strtr_collect;` |
|      - | 8301 | `struct strtr_collect` |
|      - | 8302 | `{` |
|      - | 8303 | `	SyBlob *pPool;  /* Byte pool holding copied key + value bytes */` |
|      - | 8304 | `	SySet  *pTable; /* Set of strtr_entry (parallel offsets into pPool) */` |
|      - | 8305 | `	sxi32   rc;     /* Carries an allocation failure (SXERR_MEM) out of the walker */` |
|      - | 8306 | `};` |
|      - | 8307 | `/*` |
|      - | 8308 | ` * Collect one replace_pairs entry into the persistent pool/offset table.` |
|      - | 8309 | ` * PHP coerces both the key and the value to string (an integer key becomes its` |
|      - | 8310 | ` * decimal form) and ignores an empty-string key.` |
|      - | 8311 | ` */` |
|     20 | 8312 | `static int StrtrCollectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 8313 | `{` |
|     21 | 8314 | `	strtr_collect *pCol = (strtr_collect *)pUserData;` |
|      - | 8315 | `	const char *zKey,*zVal;` |
|      - | 8316 | `	strtr_entry sEnt;` |
|      - | 8317 | `	int nKey,nVal;` |
|     21 | 8318 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|     21 | 8319 | `	if( nKey < 1 ){` |
|      - | 8320 | `		/* PHP ignores an empty-string key (it also emits a warning we do not replicate). */` |
|    ! 0 | 8321 | `		return PH7_OK;` |
|      - | 8322 | `	}` |
|     21 | 8323 | `	zVal = ph7_value_to_string(pData,&nVal);` |
|     21 | 8324 | `	sEnt.nKeyOfft = SyBlobLength(pCol->pPool);` |
|     21 | 8325 | `	sEnt.nKeyLen  = (sxu32)nKey;` |
|     21 | 8326 | `	if( SyBlobAppend(pCol->pPool,(const void *)zKey,(sxu32)nKey) != SXRET_OK ){` |
|    ! 0 | 8327 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8328 | `		return SXERR_ABORT;` |
|      - | 8329 | `	}` |
|     21 | 8330 | `	sEnt.nValOfft = SyBlobLength(pCol->pPool);` |
|     21 | 8331 | `	sEnt.nValLen  = (sxu32)nVal;` |
|     21 | 8332 | `	if( nVal > 0 && SyBlobAppend(pCol->pPool,(const void *)zVal,(sxu32)nVal) != SXRET_OK ){` |
|    ! 0 | 8333 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8334 | `		return SXERR_ABORT;` |
|      - | 8335 | `	}` |
|     21 | 8336 | `	if( SySetPut(pCol->pTable,(const void *)&sEnt) != SXRET_OK ){` |
|    ! 0 | 8337 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8338 | `		return SXERR_ABORT;` |
|      - | 8339 | `	}` |
|     21 | 8340 | `	return PH7_OK;` |
|     11 | 8341 | `}` |
|      - | 8342 | `/*` |
|      - | 8343 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 8344 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 8345 | ` *  Translate characters or replace substrings.` |
|      - | 8346 | ` * Parameters` |
|      - | 8347 | ` *  $str` |
|      - | 8348 | ` *  The string being translated.` |
|      - | 8349 | ` * $from` |
|      - | 8350 | ` *  The string being translated to to.` |
|      - | 8351 | ` * $to` |
|      - | 8352 | ` *  The string replacing from.` |
|      - | 8353 | ` * $replace_pairs` |
|      - | 8354 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 8355 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 8356 | ` * Return` |
|      - | 8357 | ` *  The translated string.` |
|      - | 8358 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 8359 | ` */` |
|     12 | 8360 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8361 | `{` |
|      - | 8362 | `	const char *zIn;` |
|      - | 8363 | `	int nLen;` |
|     13 | 8364 | `	if( nArg < 1 ){` |
|      - | 8365 | `		/* Nothing to replace,return FALSE */` |
|    ! 0 | 8366 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8367 | `		return PH7_OK;` |
|      - | 8368 | `	}` |
|     13 | 8369 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 8370 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 8371 | `		/* Invalid arguments */` |
|    ! 0 | 8372 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8373 | `		return PH7_OK;` |
|      - | 8374 | `	}` |
|     18 | 8375 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 8376 | `		strtr_collect sCol;` |
|      - | 8377 | `		SyBlob sPool,sWorker;` |
|      - | 8378 | `		SySet sTable;` |
|      - | 8379 | `		const char *zPool;` |
|      - | 8380 | `		strtr_entry *pEnt;` |
|      - | 8381 | `		sxi32 rc;` |
|      - | 8382 | `		int i,iRun;` |
|      - | 8383 | `		/*` |
|      - | 8384 | `		 * PHP's array-form strtr is a single left-to-right pass over the subject:` |
|      - | 8385 | `		 * at every position it substitutes the LONGEST replace_pairs key that` |
|      - | 8386 | `		 * matches there, then advances past the key (replacements are never` |
|      - | 8387 | `		 * rescanned). It is not a sequential per-key global replace. First copy` |
|      - | 8388 | `		 * the pairs into a persistent pool, then run that scan.` |
|      - | 8389 | `		 */` |
|     11 | 8390 | `		SyBlobInit(&sPool,&pCtx->pVm->sAllocator);` |
|     11 | 8391 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     11 | 8392 | `		SySetInit(&sTable,&pCtx->pVm->sAllocator,sizeof(strtr_entry));` |
|     11 | 8393 | `		sCol.pPool  = &sPool;` |
|     11 | 8394 | `		sCol.pTable = &sTable;` |
|     11 | 8395 | `		sCol.rc     = SXRET_OK;` |
|     11 | 8396 | `		ph7_array_walk(apArg[1],StrtrCollectWalker,&sCol);` |
|     11 | 8397 | `		if( sCol.rc != SXRET_OK ){` |
|      - | 8398 | `			/* Allocation failure while collecting the pairs: surface a fatal */` |
|    ! 0 | 8399 | `			SyBlobRelease(&sPool);` |
|    ! 0 | 8400 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 8401 | `			SySetRelease(&sTable);` |
|    ! 0 | 8402 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 8403 | `		}` |
|      - | 8404 | `		/* The pool is now stable, so offsets can be resolved against its base. */` |
|     11 | 8405 | `		zPool = (const char *)SyBlobData(&sPool);` |
|     11 | 8406 | `		rc = SXRET_OK;` |
|     11 | 8407 | `		iRun = 0; /* Start of the pending run of unmatched bytes copied verbatim. */` |
|     43 | 8408 | `		for( i = 0 ; i < nLen ; ){` |
|     33 | 8409 | `			strtr_entry *pBest = 0;` |
|     33 | 8410 | `			sxu32 nBest = 0;` |
|      - | 8411 | `			/* Pick the longest key that matches at the current position. */` |
|     33 | 8412 | `			SySetResetCursor(&sTable);` |
|     97 | 8413 | `			while( SXRET_OK == SySetGetNextEntry(&sTable,(void **)&pEnt) ){` |
|     64 | 8414 | `				if( pEnt->nKeyLen > nBest` |
|     60 | 8415 | `					&& pEnt->nKeyLen <= (sxu32)(nLen - i)` |
|     56 | 8416 | `					&& SyMemcmp(zPool + pEnt->nKeyOfft,zIn + i,pEnt->nKeyLen) == 0 ){` |
|     29 | 8417 | `					nBest = pEnt->nKeyLen;` |
|     29 | 8418 | `					pBest = pEnt;` |
|     14 | 8419 | `				}` |
|      1 | 8420 | `			}` |
|     33 | 8421 | `			if( pBest == 0 ){` |
|      - | 8422 | `				/* No key here: extend the literal run and copy it in one shot later. */` |
|      9 | 8423 | `				i++;` |
|      9 | 8424 | `				continue;` |
|      - | 8425 | `			}` |
|      - | 8426 | `			/* Flush the pending literal run, then the replacement. */` |
|     25 | 8427 | `			if( i > iRun ){` |
|      5 | 8428 | `				rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(i - iRun));` |
|      2 | 8429 | `			}` |
|     25 | 8430 | `			if( rc == SXRET_OK && pBest->nValLen > 0 ){` |
|     25 | 8431 | `				rc = SyBlobAppend(&sWorker,zPool + pBest->nValOfft,pBest->nValLen);` |
|     12 | 8432 | `			}` |
|     25 | 8433 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8434 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 8435 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8436 | `				SySetRelease(&sTable);` |
|    ! 0 | 8437 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8438 | `			}` |
|     25 | 8439 | `			i += (int)pBest->nKeyLen;` |
|     25 | 8440 | `			iRun = i;` |
|      1 | 8441 | `		}` |
|      - | 8442 | `		/* Flush the trailing literal run. */` |
|     11 | 8443 | `		if( nLen > iRun ){` |
|      3 | 8444 | `			rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(nLen - iRun));` |
|      3 | 8445 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8446 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 8447 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8448 | `				SySetRelease(&sTable);` |
|    ! 0 | 8449 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8450 | `			}` |
|      1 | 8451 | `		}` |
|      - | 8452 | `		/* All done, return the result string */` |
|     16 | 8453 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|     10 | 8454 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 8455 | `		/* Clean-up */` |
|     11 | 8456 | `		SyBlobRelease(&sPool);` |
|     11 | 8457 | `		SyBlobRelease(&sWorker);` |
|     11 | 8458 | `		SySetRelease(&sTable);` |
|     11 | 8459 | `		if( rc != PH7_OK ){` |
|    ! 0 | 8460 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 8461 | `		}` |
|      6 | 8462 | `	}else{` |
|      - | 8463 | `		int i,flen,tlen,c,iOfft;` |
|      - | 8464 | `		const char *zFrom,*zTo;` |
|      3 | 8465 | `		if( nArg < 3 ){` |
|      - | 8466 | `			/* Nothing to replace */` |
|    ! 0 | 8467 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8468 | `			return PH7_OK;` |
|      - | 8469 | `		}` |
|      - | 8470 | `		/* Extract given arguments */` |
|      3 | 8471 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 8472 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 8473 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 8474 | `			/* Nothing to replace */` |
|    ! 0 | 8475 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8476 | `			return PH7_OK;` |
|      - | 8477 | `		}` |
|      - | 8478 | `		/* Start the replace process */` |
|     13 | 8479 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 8480 | `			c = zIn[i];` |
|     11 | 8481 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 8482 | `				if ( iOfft < tlen ){` |
|      5 | 8483 | `					c = zTo[iOfft];` |
|      2 | 8484 | `				}` |
|      2 | 8485 | `			}` |
|     11 | 8486 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 8487 |  |
|      6 | 8488 | `		}` |
|      - | 8489 | `	}` |
|     13 | 8490 | `	return PH7_OK;` |
|      7 | 8491 | `}` |
|      - | 8492 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8493 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8494 | `/*` |
|      - | 8495 | ` * Parse an INI string.` |
|      - | 8496 |  |
|      - | 8497 | ` * According to wikipedia` |
|      - | 8498 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 8499 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 8500 | ` *  Format` |
|      - | 8501 | `*    Properties` |
|      - | 8502 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 8503 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 8504 | `*     Example:` |
|      - | 8505 | `*      name=value` |
|      - | 8506 | `*    Sections` |
|      - | 8507 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 8508 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 8509 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 8510 | `*     or the end of the file. Sections may not be nested.` |
|      - | 8511 | `*     Example:` |
|      - | 8512 | `*      [section]` |
|      - | 8513 | `*   Comments` |
|      - | 8514 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 8515 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 8516 | `*/` |
|     12 | 8517 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 8518 | `{` |
|      - | 8519 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 8520 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 8521 | `	SyHashEntry *pEntry;` |
|      - | 8522 | `	SyString sEntry;` |
|      - | 8523 | `	SyHash sHash;` |
|      - | 8524 | `	int c;` |
|      - | 8525 | `	/* Create an empty array and worker variables */` |
|     13 | 8526 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 8527 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 8528 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 8529 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 8530 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|    ! 0 | 8531 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8532 | `	}` |
|     13 | 8533 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 8534 | `	pCur = pArray;` |
|      - | 8535 | `	/* Start the parse process */` |
|     21 | 8536 | `	for(;;){` |
|      - | 8537 | `		/* Ignore leading white spaces */` |
|     69 | 8538 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 8539 | `			zIn++;` |
|      1 | 8540 | `		}` |
|     43 | 8541 | `		if( zIn >= zEnd ){` |
|      - | 8542 | `			/* No more input to process */` |
|     13 | 8543 | `			break;` |
|      - | 8544 | `		}` |
|     31 | 8545 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 8546 | `			/* Comment til the end of line */` |
|    ! 0 | 8547 | `			zIn++;` |
|    ! 0 | 8548 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 8549 | `				zIn++;` |
|    ! 0 | 8550 | `			}` |
|    ! 0 | 8551 | `			continue;` |
|      - | 8552 | `		}` |
|      - | 8553 | `		/* Reset the string cursor of the working variable */` |
|     31 | 8554 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 8555 | `		if( zIn[0] == '[' ){` |
|      - | 8556 | `			/* Section: Extract the section name */` |
|      9 | 8557 | `			zIn++;` |
|      9 | 8558 | `			zCur = zIn;` |
|     73 | 8559 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 8560 | `				zIn++;` |
|      1 | 8561 | `			}` |
|      9 | 8562 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 8563 | `				/* Save the section name */` |
|      5 | 8564 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 8565 | `				SyStringFullTrim(&sEntry);` |
|      5 | 8566 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 8567 | `				if( sEntry.nByte > 0 ){` |
|      - | 8568 | `					/* Associate an array with the section */` |
|      5 | 8569 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 8570 | `					if( pSection ){` |
|      5 | 8571 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 8572 | `						pCur = pSection;` |
|      2 | 8573 | `					}` |
|      2 | 8574 | `				}` |
|      2 | 8575 | `			}` |
|      9 | 8576 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 8577 | `		}else{` |
|      - | 8578 | `			ph7_value *pOldCur;` |
|      - | 8579 | `			int is_array;` |
|      - | 8580 | `			int iLen;` |
|      - | 8581 | `			/* Properties */` |
|     23 | 8582 | `			is_array = 0;` |
|     23 | 8583 | `			zCur = zIn;` |
|     23 | 8584 | `			iLen = 0; /* cc warning */` |
|     23 | 8585 | `			pOldCur = pCur;` |
|    155 | 8586 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 8587 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 8588 | `					/* Array */` |
|    ! 0 | 8589 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 8590 | `					is_array = 1;` |
|    ! 0 | 8591 | `					if( iLen > 0 ){` |
|    ! 0 | 8592 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 8593 | `						/* Query the hashtable */` |
|    ! 0 | 8594 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 8595 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 8596 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 8597 | `						if( pEntry ){` |
|    ! 0 | 8598 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 8599 | `						}else{` |
|      - | 8600 | `							/* Create an empty array */` |
|    ! 0 | 8601 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 8602 | `							if( pvArr ){` |
|      - | 8603 | `								/* Save the entry */` |
|    ! 0 | 8604 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 8605 | `								/* Insert the entry */` |
|    ! 0 | 8606 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 8607 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 8608 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 8609 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 8610 | `							}` |
|      - | 8611 | `						}` |
|    ! 0 | 8612 | `						if( pvArr ){` |
|    ! 0 | 8613 | `							pCur = pvArr;` |
|    ! 0 | 8614 | `						}` |
|    ! 0 | 8615 | `					}` |
|    ! 0 | 8616 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 8617 | `						zIn++;` |
|    ! 0 | 8618 | `					}` |
|    ! 0 | 8619 | `				}` |
|    133 | 8620 | `				zIn++;` |
|      1 | 8621 | `			}` |
|     23 | 8622 | `			if( !is_array ){` |
|     23 | 8623 | `				iLen = (int)(zIn-zCur);` |
|     11 | 8624 | `			}` |
|      - | 8625 | `			/* Trim the key */` |
|     23 | 8626 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 8627 | `			SyStringFullTrim(&sEntry);` |
|     23 | 8628 | `			if( sEntry.nByte > 0 ){` |
|     23 | 8629 | `				if( !is_array ){` |
|      - | 8630 | `					/* Save the key name */` |
|     23 | 8631 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 8632 | `				}` |
|      - | 8633 | `				/* extract key value */` |
|     23 | 8634 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 8635 | `				zIn++; /* '=' */` |
|     39 | 8636 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 8637 | `					zIn++;` |
|      1 | 8638 | `				}` |
|     23 | 8639 | `				if( zIn < zEnd ){` |
|     21 | 8640 | `					zCur = zIn;` |
|     21 | 8641 | `					c = zIn[0];` |
|     21 | 8642 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 8643 | `						zIn++;` |
|      - | 8644 | `						/* Delimit the value */` |
|    ! 0 | 8645 | `						while( zIn < zEnd ){` |
|    ! 0 | 8646 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 8647 | `								break;` |
|      - | 8648 | `							}` |
|    ! 0 | 8649 | `							zIn++;` |
|    ! 0 | 8650 | `						}` |
|    ! 0 | 8651 | `						if( zIn < zEnd ){` |
|    ! 0 | 8652 | `							zIn++;` |
|    ! 0 | 8653 | `						}` |
|    ! 0 | 8654 | `					}else{` |
|    125 | 8655 | `						while( zIn < zEnd ){` |
|    123 | 8656 | `							if( zIn[0] == '\n' ){` |
|     19 | 8657 | `								if( zIn[-1] != '\\' ){` |
|     19 | 8658 | `									break;` |
|    ! 0 | 8659 | `								}` |
|    105 | 8660 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 8661 | `								/* Inline comments */` |
|    ! 0 | 8662 | `								break;` |
|      - | 8663 | `							}` |
|    105 | 8664 | `							zIn++;` |
|      1 | 8665 | `						}` |
|      - | 8666 | `					}` |
|      - | 8667 | `					/* Trim the value */` |
|     21 | 8668 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 8669 | `					SyStringFullTrim(&sEntry);` |
|     21 | 8670 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 8671 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 8672 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 8673 | `					}` |
|     21 | 8674 | `					if( sEntry.nByte > 0 ){` |
|     21 | 8675 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 8676 | `					}` |
|      - | 8677 | `					/* Insert the key and it's value */` |
|     21 | 8678 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 8679 | `				}` |
|     12 | 8680 | `			}else{` |
|    ! 0 | 8681 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 8682 | `					zIn++;` |
|    ! 0 | 8683 | `				}` |
|      - | 8684 | `			}` |
|     23 | 8685 | `			pCur = pOldCur;` |
|      - | 8686 | `		}` |
|      1 | 8687 | `	}` |
|     13 | 8688 | `	SyHashRelease(&sHash);` |
|      - | 8689 | `	/* Return the parse of the INI string */` |
|     13 | 8690 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 8691 | `	return SXRET_OK;` |
|      7 | 8692 | `}` |
|      - | 8693 | `/*` |
|      - | 8694 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 8695 | ` *  Parse a configuration string.` |
|      - | 8696 | ` * Parameters` |
|      - | 8697 | ` *  $ini` |
|      - | 8698 | ` *   The contents of the ini file being parsed.` |
|      - | 8699 | ` *  $process_sections` |
|      - | 8700 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 8701 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 8702 | ` *  $scanner_mode (Not used)` |
|      - | 8703 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 8704 | ` *   then option values will not be parsed.` |
|      - | 8705 | ` * Return` |
|      - | 8706 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 8707 | ` */` |
|     10 | 8708 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8709 | `{` |
|      - | 8710 | `	const char *zIni;` |
|      - | 8711 | `	int nByte;` |
|     11 | 8712 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8713 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 8714 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8715 | `		return PH7_OK;` |
|      - | 8716 | `	}` |
|      - | 8717 | `	/* Extract the raw INI buffer */` |
|     11 | 8718 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 8719 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|     11 | 8720 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      6 | 8721 | `}` |
|      - | 8722 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8723 |  |
|      - | 8724 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8725 |  |
|      - | 8726 | `/*` |
|      - | 8727 | ` * Ctype Functions.` |
|      - | 8728 | ` * Status:` |
|      - | 8729 | ` *    Stable.` |
|      - | 8730 | ` */` |
|      - | 8731 | `/*` |
|      - | 8732 | ` * bool ctype_alnum(string $text)` |
|      - | 8733 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 8734 | ` * Parameters` |
|      - | 8735 | ` *  $text` |
|      - | 8736 | ` *   The tested string.` |
|      - | 8737 | ` * Return` |
|      - | 8738 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 8739 | ` */` |
|     14 | 8740 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8741 | `{` |
|      - | 8742 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8743 | `	int nLen;` |
|     15 | 8744 | `	if( nArg < 1 ){` |
|      - | 8745 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8746 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8747 | `		return PH7_OK;` |
|      - | 8748 | `	}` |
|      - | 8749 | `	/* Extract the target string */` |
|     15 | 8750 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 8751 | `	zEnd = &zIn[nLen];` |
|     15 | 8752 | `	if( nLen < 1 ){` |
|      - | 8753 | `		/* Empty string,return FALSE */` |
|      3 | 8754 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8755 | `		return PH7_OK;` |
|      - | 8756 | `	}` |
|      - | 8757 | `	/* Perform the requested operation */` |
|     32 | 8758 | `	for(;;){` |
|     65 | 8759 | `		if( zIn >= zEnd ){` |
|      - | 8760 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8761 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8762 | `			return PH7_OK;` |
|      - | 8763 | `		}` |
|     57 | 8764 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 8765 | `			break;` |
|      - | 8766 | `		}` |
|      - | 8767 | `		/* Point to the next character */` |
|     53 | 8768 | `		zIn++;` |
|      1 | 8769 | `	}` |
|      - | 8770 | `	/* The test failed,return FALSE */` |
|      5 | 8771 | `	ph7_result_bool(pCtx,0);` |
|      5 | 8772 | `	return PH7_OK;` |
|      8 | 8773 | `}` |
|      - | 8774 | `/*` |
|      - | 8775 | ` * bool ctype_alpha(string $text)` |
|      - | 8776 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 8777 | ` * Parameters` |
|      - | 8778 | ` *  $text` |
|      - | 8779 | ` *   The tested string.` |
|      - | 8780 | ` * Return` |
|      - | 8781 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 8782 | ` */` |
|     16 | 8783 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8784 | `{` |
|      - | 8785 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8786 | `	int nLen;` |
|     17 | 8787 | `	if( nArg < 1 ){` |
|      - | 8788 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8789 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8790 | `		return PH7_OK;` |
|      - | 8791 | `	}` |
|      - | 8792 | `	/* Extract the target string */` |
|     17 | 8793 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8794 | `	zEnd = &zIn[nLen];` |
|     17 | 8795 | `	if( nLen < 1 ){` |
|      - | 8796 | `		/* Empty string,return FALSE */` |
|      3 | 8797 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8798 | `		return PH7_OK;` |
|      - | 8799 | `	}` |
|      - | 8800 | `	/* Perform the requested operation */` |
|     42 | 8801 | `	for(;;){` |
|     85 | 8802 | `		if( zIn >= zEnd ){` |
|      - | 8803 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8804 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8805 | `			return PH7_OK;` |
|      - | 8806 | `		}` |
|     77 | 8807 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 8808 | `			break;` |
|      - | 8809 | `		}` |
|      - | 8810 | `		/* Point to the next character */` |
|     71 | 8811 | `		zIn++;` |
|      1 | 8812 | `	}` |
|      - | 8813 | `	/* The test failed,return FALSE */` |
|      7 | 8814 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8815 | `	return PH7_OK;` |
|      9 | 8816 | `}` |
|      - | 8817 | `/*` |
|      - | 8818 | ` * bool ctype_cntrl(string $text)` |
|      - | 8819 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 8820 | ` * Parameters` |
|      - | 8821 | ` *  $text` |
|      - | 8822 | ` *   The tested string.` |
|      - | 8823 | ` * Return` |
|      - | 8824 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 8825 | ` */` |
|     16 | 8826 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8827 | `{` |
|      - | 8828 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8829 | `	int nLen;` |
|     17 | 8830 | `	if( nArg < 1 ){` |
|      - | 8831 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8832 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8833 | `		return PH7_OK;` |
|      - | 8834 | `	}` |
|      - | 8835 | `	/* Extract the target string */` |
|     17 | 8836 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8837 | `	zEnd = &zIn[nLen];` |
|     17 | 8838 | `	if( nLen < 1 ){` |
|      - | 8839 | `		/* Empty string,return FALSE */` |
|      3 | 8840 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8841 | `		return PH7_OK;` |
|      - | 8842 | `	}` |
|      - | 8843 | `	/* Perform the requested operation */` |
|     14 | 8844 | `	for(;;){` |
|     29 | 8845 | `		if( zIn >= zEnd ){` |
|      - | 8846 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8847 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8848 | `			return PH7_OK;` |
|      - | 8849 | `		}` |
|     21 | 8850 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8851 | `			/* UTF-8 stream  */` |
|    ! 0 | 8852 | `			break;` |
|      - | 8853 | `		}` |
|     21 | 8854 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 8855 | `			break;` |
|      - | 8856 | `		}` |
|      - | 8857 | `		/* Point to the next character */` |
|     15 | 8858 | `		zIn++;` |
|      1 | 8859 | `	}` |
|      - | 8860 | `	/* The test failed,return FALSE */` |
|      7 | 8861 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8862 | `	return PH7_OK;` |
|      9 | 8863 | `}` |
|      - | 8864 | `/*` |
|      - | 8865 | ` * bool ctype_digit(string $text)` |
|      - | 8866 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 8867 | ` * Parameters` |
|      - | 8868 | ` *  $text` |
|      - | 8869 | ` *   The tested string.` |
|      - | 8870 | ` * Return` |
|      - | 8871 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 8872 | ` */` |
|   2284 | 8873 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 8874 | `{` |
|      - | 8875 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8876 | `	int nLen;` |
|   2289 | 8877 | `	if( nArg < 1 ){` |
|      - | 8878 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8879 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8880 | `		return PH7_OK;` |
|      - | 8881 | `	}` |
|      - | 8882 | `	/* Extract the target string */` |
|   2289 | 8883 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   2289 | 8884 | `	zEnd = &zIn[nLen];` |
|   2289 | 8885 | `	if( nLen < 1 ){` |
|      - | 8886 | `		/* Empty string,return FALSE */` |
|      3 | 8887 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8888 | `		return PH7_OK;` |
|      - | 8889 | `	}` |
|      - | 8890 | `	/* Perform the requested operation */` |
|   2087 | 8891 | `	for(;;){` |
|   4179 | 8892 | `		if( zIn >= zEnd ){` |
|      - | 8893 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1847 | 8894 | `			ph7_result_bool(pCtx,1);` |
|   1847 | 8895 | `			return PH7_OK;` |
|      - | 8896 | `		}` |
|   2337 | 8897 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8898 | `			/* UTF-8 stream  */` |
|    ! 0 | 8899 | `			break;` |
|      - | 8900 | `		}` |
|   2337 | 8901 | `		if( !SyisDigit(zIn[0]) ){` |
|    445 | 8902 | `			break;` |
|      - | 8903 | `		}` |
|      - | 8904 | `		/* Point to the next character */` |
|   1897 | 8905 | `		zIn++;` |
|      5 | 8906 | `	}` |
|      - | 8907 | `	/* The test failed,return FALSE */` |
|    445 | 8908 | `	ph7_result_bool(pCtx,0);` |
|    445 | 8909 | `	return PH7_OK;` |
|   1147 | 8910 | `}` |
|      - | 8911 | `/*` |
|      - | 8912 | ` * bool ctype_xdigit(string $text)` |
|      - | 8913 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 8914 | ` * Parameters` |
|      - | 8915 | ` *  $text` |
|      - | 8916 | ` *   The tested string.` |
|      - | 8917 | ` * Return` |
|      - | 8918 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 8919 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 8920 | ` */` |
|     18 | 8921 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8922 | `{` |
|      - | 8923 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8924 | `	int nLen;` |
|     19 | 8925 | `	if( nArg < 1 ){` |
|      - | 8926 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8927 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8928 | `		return PH7_OK;` |
|      - | 8929 | `	}` |
|      - | 8930 | `	/* Extract the target string */` |
|     19 | 8931 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 8932 | `	zEnd = &zIn[nLen];` |
|     19 | 8933 | `	if( nLen < 1 ){` |
|      - | 8934 | `		/* Empty string,return FALSE */` |
|      3 | 8935 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8936 | `		return PH7_OK;` |
|      - | 8937 | `	}` |
|      - | 8938 | `	/* Perform the requested operation */` |
|     46 | 8939 | `	for(;;){` |
|     93 | 8940 | `		if( zIn >= zEnd ){` |
|      - | 8941 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 8942 | `			ph7_result_bool(pCtx,1);` |
|     11 | 8943 | `			return PH7_OK;` |
|      - | 8944 | `		}` |
|     83 | 8945 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8946 | `			/* UTF-8 stream  */` |
|    ! 0 | 8947 | `			break;` |
|      - | 8948 | `		}` |
|     83 | 8949 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 8950 | `			break;` |
|      - | 8951 | `		}` |
|      - | 8952 | `		/* Point to the next character */` |
|     77 | 8953 | `		zIn++;` |
|      1 | 8954 | `	}` |
|      - | 8955 | `	/* The test failed,return FALSE */` |
|      7 | 8956 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8957 | `	return PH7_OK;` |
|     10 | 8958 | `}` |
|      - | 8959 | `/*` |
|      - | 8960 | ` * bool ctype_graph(string $text)` |
|      - | 8961 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 8962 | ` * Parameters` |
|      - | 8963 | ` *  $text` |
|      - | 8964 | ` *   The tested string.` |
|      - | 8965 | ` * Return` |
|      - | 8966 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 8967 | ` * (no white space), FALSE otherwise.` |
|      - | 8968 | ` */` |
|     16 | 8969 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8970 | `{` |
|      - | 8971 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8972 | `	int nLen;` |
|     17 | 8973 | `	if( nArg < 1 ){` |
|      - | 8974 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8975 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8976 | `		return PH7_OK;` |
|      - | 8977 | `	}` |
|      - | 8978 | `	/* Extract the target string */` |
|     17 | 8979 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8980 | `	zEnd = &zIn[nLen];` |
|     17 | 8981 | `	if( nLen < 1 ){` |
|      - | 8982 | `		/* Empty string,return FALSE */` |
|      3 | 8983 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8984 | `		return PH7_OK;` |
|      - | 8985 | `	}` |
|      - | 8986 | `	/* Perform the requested operation */` |
|     57 | 8987 | `	for(;;){` |
|    115 | 8988 | `		if( zIn >= zEnd ){` |
|      - | 8989 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8990 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8991 | `			return PH7_OK;` |
|      - | 8992 | `		}` |
|    107 | 8993 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8994 | `			/* UTF-8 stream  */` |
|    ! 0 | 8995 | `			break;` |
|      - | 8996 | `		}` |
|    107 | 8997 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 8998 | `			break;` |
|      - | 8999 | `		}` |
|      - | 9000 | `		/* Point to the next character */` |
|    101 | 9001 | `		zIn++;` |
|      1 | 9002 | `	}` |
|      - | 9003 | `	/* The test failed,return FALSE */` |
|      7 | 9004 | `	ph7_result_bool(pCtx,0);` |
|      7 | 9005 | `	return PH7_OK;` |
|      9 | 9006 | `}` |
|      - | 9007 | `/*` |
|      - | 9008 | ` * bool ctype_print(string $text)` |
|      - | 9009 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 9010 | ` * Parameters` |
|      - | 9011 | ` *  $text` |
|      - | 9012 | ` *   The tested string.` |
|      - | 9013 | ` * Return` |
|      - | 9014 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 9015 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 9016 | ` *  or control function at all.` |
|      - | 9017 | ` */` |
|     16 | 9018 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9019 | `{` |
|      - | 9020 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9021 | `	int nLen;` |
|     17 | 9022 | `	if( nArg < 1 ){` |
|      - | 9023 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9024 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9025 | `		return PH7_OK;` |
|      - | 9026 | `	}` |
|      - | 9027 | `	/* Extract the target string */` |
|     17 | 9028 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9029 | `	zEnd = &zIn[nLen];` |
|     17 | 9030 | `	if( nLen < 1 ){` |
|      - | 9031 | `		/* Empty string,return FALSE */` |
|      3 | 9032 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9033 | `		return PH7_OK;` |
|      - | 9034 | `	}` |
|      - | 9035 | `	/* Perform the requested operation */` |
|     63 | 9036 | `	for(;;){` |
|    127 | 9037 | `		if( zIn >= zEnd ){` |
|      - | 9038 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 9039 | `			ph7_result_bool(pCtx,1);` |
|      9 | 9040 | `			return PH7_OK;` |
|      - | 9041 | `		}` |
|    119 | 9042 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9043 | `			/* UTF-8 stream  */` |
|    ! 0 | 9044 | `			break;` |
|      - | 9045 | `		}` |
|    119 | 9046 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 9047 | `			break;` |
|      - | 9048 | `		}` |
|      - | 9049 | `		/* Point to the next character */` |
|    113 | 9050 | `		zIn++;` |
|      1 | 9051 | `	}` |
|      - | 9052 | `	/* The test failed,return FALSE */` |
|      7 | 9053 | `	ph7_result_bool(pCtx,0);` |
|      7 | 9054 | `	return PH7_OK;` |
|      9 | 9055 | `}` |
|      - | 9056 | `/*` |
|      - | 9057 | ` * bool ctype_punct(string $text)` |
|      - | 9058 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 9059 | ` * Parameters` |
|      - | 9060 | ` *  $text` |
|      - | 9061 | ` *   The tested string.` |
|      - | 9062 | ` * Return` |
|      - | 9063 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 9064 | ` *  digit or blank, FALSE otherwise.` |
|      - | 9065 | ` */` |
|     18 | 9066 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9067 | `{` |
|      - | 9068 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9069 | `	int nLen;` |
|     19 | 9070 | `	if( nArg < 1 ){` |
|      - | 9071 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9072 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9073 | `		return PH7_OK;` |
|      - | 9074 | `	}` |
|      - | 9075 | `	/* Extract the target string */` |
|     19 | 9076 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 9077 | `	zEnd = &zIn[nLen];` |
|     19 | 9078 | `	if( nLen < 1 ){` |
|      - | 9079 | `		/* Empty string,return FALSE */` |
|      3 | 9080 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9081 | `		return PH7_OK;` |
|      - | 9082 | `	}` |
|      - | 9083 | `	/* Perform the requested operation */` |
|     38 | 9084 | `	for(;;){` |
|     77 | 9085 | `		if( zIn >= zEnd ){` |
|      - | 9086 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 9087 | `			ph7_result_bool(pCtx,1);` |
|      9 | 9088 | `			return PH7_OK;` |
|      - | 9089 | `		}` |
|     69 | 9090 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9091 | `			/* UTF-8 stream  */` |
|    ! 0 | 9092 | `			break;` |
|      - | 9093 | `		}` |
|     69 | 9094 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 9095 | `			break;` |
|      - | 9096 | `		}` |
|      - | 9097 | `		/* Point to the next character */` |
|     61 | 9098 | `		zIn++;` |
|      1 | 9099 | `	}` |
|      - | 9100 | `	/* The test failed,return FALSE */` |
|      9 | 9101 | `	ph7_result_bool(pCtx,0);` |
|      9 | 9102 | `	return PH7_OK;` |
|     10 | 9103 | `}` |
|      - | 9104 | `/*` |
|      - | 9105 | ` * bool ctype_space(string $text)` |
|      - | 9106 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 9107 | ` * Parameters` |
|      - | 9108 | ` *  $text` |
|      - | 9109 | ` *   The tested string.` |
|      - | 9110 | ` * Return` |
|      - | 9111 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 9112 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 9113 | ` *  and form feed characters.` |
|      - | 9114 | ` */` |
|  71604 | 9115 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 9116 | `{` |
|      - | 9117 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9118 | `	int nLen;` |
|  71609 | 9119 | `	if( nArg < 1 ){` |
|      - | 9120 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9121 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9122 | `		return PH7_OK;` |
|      - | 9123 | `	}` |
|      - | 9124 | `	/* Extract the target string */` |
|  71609 | 9125 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  71609 | 9126 | `	zEnd = &zIn[nLen];` |
|  71609 | 9127 | `	if( nLen < 1 ){` |
|      - | 9128 | `		/* Empty string,return FALSE */` |
|      3 | 9129 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9130 | `		return PH7_OK;` |
|      - | 9131 | `	}` |
|      - | 9132 | `	/* Perform the requested operation */` |
|  36972 | 9133 | `	for(;;){` |
|  73905 | 9134 | `		if( zIn >= zEnd ){` |
|      - | 9135 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   2279 | 9136 | `			ph7_result_bool(pCtx,1);` |
|   2279 | 9137 | `			return PH7_OK;` |
|      - | 9138 | `		}` |
|  71631 | 9139 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9140 | `			/* UTF-8 stream  */` |
|    ! 0 | 9141 | `			break;` |
|      - | 9142 | `		}` |
|  71631 | 9143 | `		if( !SyisSpace(zIn[0]) ){` |
|  69333 | 9144 | `			break;` |
|      - | 9145 | `		}` |
|      - | 9146 | `		/* Point to the next character */` |
|   2303 | 9147 | `		zIn++;` |
|      5 | 9148 | `	}` |
|      - | 9149 | `	/* The test failed,return FALSE */` |
|  69333 | 9150 | `	ph7_result_bool(pCtx,0);` |
|  69333 | 9151 | `	return PH7_OK;` |
|  35829 | 9152 | `}` |
|      - | 9153 | `/*` |
|      - | 9154 | ` * bool ctype_lower(string $text)` |
|      - | 9155 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 9156 | ` * Parameters` |
|      - | 9157 | ` *  $text` |
|      - | 9158 | ` *   The tested string.` |
|      - | 9159 | ` * Return` |
|      - | 9160 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 9161 | ` */` |
|     16 | 9162 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9163 | `{` |
|      - | 9164 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9165 | `	int nLen;` |
|     17 | 9166 | `	if( nArg < 1 ){` |
|      - | 9167 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9168 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9169 | `		return PH7_OK;` |
|      - | 9170 | `	}` |
|      - | 9171 | `	/* Extract the target string */` |
|     17 | 9172 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9173 | `	zEnd = &zIn[nLen];` |
|     17 | 9174 | `	if( nLen < 1 ){` |
|      - | 9175 | `		/* Empty string,return FALSE */` |
|      3 | 9176 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9177 | `		return PH7_OK;` |
|      - | 9178 | `	}` |
|      - | 9179 | `	/* Perform the requested operation */` |
|     27 | 9180 | `	for(;;){` |
|     55 | 9181 | `		if( zIn >= zEnd ){` |
|      - | 9182 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 9183 | `			ph7_result_bool(pCtx,1);` |
|      5 | 9184 | `			return PH7_OK;` |
|      - | 9185 | `		}` |
|     51 | 9186 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 9187 | `			break;` |
|      - | 9188 | `		}` |
|      - | 9189 | `		/* Point to the next character */` |
|     41 | 9190 | `		zIn++;` |
|      1 | 9191 | `	}` |
|      - | 9192 | `	/* The test failed,return FALSE */` |
|     11 | 9193 | `	ph7_result_bool(pCtx,0);` |
|     11 | 9194 | `	return PH7_OK;` |
|      9 | 9195 | `}` |
|      - | 9196 | `/*` |
|      - | 9197 | ` * bool ctype_upper(string $text)` |
|      - | 9198 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 9199 | ` * Parameters` |
|      - | 9200 | ` *  $text` |
|      - | 9201 | ` *   The tested string.` |
|      - | 9202 | ` * Return` |
|      - | 9203 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 9204 | ` */` |
|     16 | 9205 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9206 | `{` |
|      - | 9207 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9208 | `	int nLen;` |
|     17 | 9209 | `	if( nArg < 1 ){` |
|      - | 9210 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9211 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9212 | `		return PH7_OK;` |
|      - | 9213 | `	}` |
|      - | 9214 | `	/* Extract the target string */` |
|     17 | 9215 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9216 | `	zEnd = &zIn[nLen];` |
|     17 | 9217 | `	if( nLen < 1 ){` |
|      - | 9218 | `		/* Empty string,return FALSE */` |
|      3 | 9219 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9220 | `		return PH7_OK;` |
|      - | 9221 | `	}` |
|      - | 9222 | `	/* Perform the requested operation */` |
|     28 | 9223 | `	for(;;){` |
|     57 | 9224 | `		if( zIn >= zEnd ){` |
|      - | 9225 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 9226 | `			ph7_result_bool(pCtx,1);` |
|      5 | 9227 | `			return PH7_OK;` |
|      - | 9228 | `		}` |
|     53 | 9229 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 9230 | `			break;` |
|      - | 9231 | `		}` |
|      - | 9232 | `		/* Point to the next character */` |
|     43 | 9233 | `		zIn++;` |
|      1 | 9234 | `	}` |
|      - | 9235 | `	/* The test failed,return FALSE */` |
|     11 | 9236 | `	ph7_result_bool(pCtx,0);` |
|     11 | 9237 | `	return PH7_OK;` |
|      9 | 9238 | `}` |
|      - | 9239 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 9240 | `/*` |
|      - | 9241 | ` * Section:` |
|      - | 9242 | ` *    URL handling Functions.` |
|      - | 9243 | ` * Status:` |
|      - | 9244 | ` *    Stable.` |
|      - | 9245 | ` */` |
|      - | 9246 | `/*` |
|      - | 9247 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 9248 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 9249 | ` */` |
|   1026 | 9250 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 9251 | `{` |
|      - | 9252 | `	/* Store in the call context result buffer */` |
|   1028 | 9253 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 9254 | `	return SXRET_OK;` |
|      2 | 9255 | `}` |
|      - | 9256 | `/*` |
|      - | 9257 | ` * string base64_encode(string $data)` |
|      - | 9258 | ` * string convert_uuencode(string $data)` |
|      - | 9259 | ` *  Encodes data with MIME base64` |
|      - | 9260 | ` * Parameter` |
|      - | 9261 | ` *  $data` |
|      - | 9262 | ` *    Data to encode` |
|      - | 9263 | ` * Return` |
|      - | 9264 | ` *  Encoded data or FALSE on failure.` |
|      - | 9265 | ` */` |
|      6 | 9266 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9267 | `{` |
|      - | 9268 | `	const char *zIn;` |
|      - | 9269 | `	int nLen;` |
|      7 | 9270 | `	if( nArg < 1 ){` |
|      - | 9271 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9272 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9273 | `		return PH7_OK;` |
|      - | 9274 | `	}` |
|      - | 9275 | `	/* Extract the input string */` |
|      7 | 9276 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 9277 | `	if( nLen < 1 ){` |
|      - | 9278 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9279 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9280 | `		return PH7_OK;` |
|      - | 9281 | `	}` |
|      - | 9282 | `	/* Perform the BASE64 encoding */` |
|      7 | 9283 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 9284 | `	return PH7_OK;` |
|      4 | 9285 | `}` |
|      - | 9286 | `/*` |
|      - | 9287 | ` * string base64_decode(string $data)` |
|      - | 9288 | ` * string convert_uudecode(string $data)` |
|      - | 9289 | ` *  Decodes data encoded with MIME base64` |
|      - | 9290 | ` * Parameter` |
|      - | 9291 | ` *  $data` |
|      - | 9292 | ` *    Encoded data.` |
|      - | 9293 | ` * Return` |
|      - | 9294 | ` *  Returns the original data or FALSE on failure.` |
|      - | 9295 | ` */` |
|     34 | 9296 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 9297 | `{` |
|      - | 9298 | `	const char *zIn;` |
|      - | 9299 | `	int nLen;` |
|     36 | 9300 | `	if( nArg < 1 ){` |
|      - | 9301 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9302 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9303 | `		return PH7_OK;` |
|      - | 9304 | `	}` |
|      - | 9305 | `	/* Extract the input string */` |
|     36 | 9306 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 9307 | `	if( nLen < 1 ){` |
|      - | 9308 | `		/* Nothing to process,return FALSE */` |
|      3 | 9309 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9310 | `		return PH7_OK;` |
|      - | 9311 | `	}` |
|      - | 9312 | `	/* Perform the BASE64 decoding */` |
|     34 | 9313 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 9314 | `	return PH7_OK;` |
|     19 | 9315 | `}` |
|      - | 9316 | `/*` |
|      - | 9317 | ` * string urlencode(string $str)` |
|      - | 9318 | ` *  URL encoding` |
|      - | 9319 | ` * Parameter` |
|      - | 9320 | ` *  $data` |
|      - | 9321 | ` *   Input string.` |
|      - | 9322 | ` * Return` |
|      - | 9323 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 9324 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 9325 | ` *  encoded as plus (+) signs.` |
|      - | 9326 | ` */` |
|      4 | 9327 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9328 | `{` |
|      - | 9329 | `	const char *zIn;` |
|      - | 9330 | `	int nLen;` |
|      5 | 9331 | `	if( nArg < 1 ){` |
|      - | 9332 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9333 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9334 | `		return PH7_OK;` |
|      - | 9335 | `	}` |
|      - | 9336 | `	/* Extract the input string */` |
|      5 | 9337 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 9338 | `	if( nLen < 1 ){` |
|      - | 9339 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9340 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9341 | `		return PH7_OK;` |
|      - | 9342 | `	}` |
|      - | 9343 | `	/* Perform the URL encoding */` |
|      5 | 9344 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 9345 | `	return PH7_OK;` |
|      3 | 9346 | `}` |
|      - | 9347 | `/*` |
|      - | 9348 | ` * string urldecode(string $str)` |
|      - | 9349 | ` *  Decodes any %## encoding in the given string.` |
|      - | 9350 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 9351 | ` * Parameter` |
|      - | 9352 | ` *  $data` |
|      - | 9353 | ` *    Input string.` |
|      - | 9354 | ` * Return` |
|      - | 9355 | ` *  Decoded URL or FALSE on failure.` |
|      - | 9356 | ` */` |
|      6 | 9357 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9358 | `{` |
|      - | 9359 | `	const char *zIn;` |
|      - | 9360 | `	int nLen;` |
|      7 | 9361 | `	if( nArg < 1 ){` |
|      - | 9362 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9363 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9364 | `		return PH7_OK;` |
|      - | 9365 | `	}` |
|      - | 9366 | `	/* Extract the input string */` |
|      7 | 9367 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 9368 | `	if( nLen < 1 ){` |
|      - | 9369 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9370 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9371 | `		return PH7_OK;` |
|      - | 9372 | `	}` |
|      - | 9373 | `	/* Perform the URL decoding */` |
|      7 | 9374 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 9375 | `	return PH7_OK;` |
|      4 | 9376 | `}` |
|      - | 9377 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9378 | `/* Table of the built-in functions */` |
|      - | 9379 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 9380 | `	   /* Variable handling functions */` |
|      - | 9381 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 9382 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 9383 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 9384 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 9385 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 9386 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 9387 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 9388 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 9389 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 9390 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 9391 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 9392 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 9393 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 9394 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 9395 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 9396 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 9397 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 9398 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 9399 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 9400 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 9401 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9402 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 9403 | `	   /* Math functions */` |
|      - | 9404 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 9405 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 9406 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 9407 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 9408 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 9409 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 9410 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 9411 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 9412 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 9413 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 9414 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 9415 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 9416 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 9417 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 9418 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 9419 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 9420 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 9421 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 9422 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 9423 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 9424 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 9425 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 9426 | `	{ "round",    PH7_builtin_round        },` |
|      - | 9427 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 9428 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 9429 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 9430 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 9431 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 9432 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 9433 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 9434 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 9435 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 9436 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9437 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9438 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 9439 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9440 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9441 | `	   /* String handling functions */` |
|      - | 9442 |  |
|      - | 9443 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 9444 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 9445 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 9446 | `	{ "substr_replace",  PH7_builtin_substr_replace },` |
|      - | 9447 | `	{ "levenshtein",     PH7_builtin_levenshtein },` |
|      - | 9448 | `	{ "similar_text",    PH7_builtin_similar_text },` |
|      - | 9449 | `	{ "str_word_count",  PH7_builtin_str_word_count },` |
|      - | 9450 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 9451 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 9452 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 9453 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 9454 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 9455 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 9456 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 9457 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 9458 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 9459 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 9460 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 9461 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 9462 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 9463 | `	{ "strnatcmp"  , PH7_builtin_strnatcmp  },` |
|      - | 9464 | `	{ "strnatcasecmp", PH7_builtin_strnatcmp },` |
|      - | 9465 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 9466 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 9467 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 9468 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 9469 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 9470 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 9471 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 9472 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 9473 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 9474 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 9475 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 9476 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 9477 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 9478 | `	{ "mb_strtolower",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */` |
|      - | 9479 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 9480 | `	{ "mb_strtoupper",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */` |
|      - | 9481 | `	{ "mb_strlen",    PH7_builtin_mb_strlen_f },` |
|      - | 9482 | `	{ "mb_substr",    PH7_builtin_mb_substr_f },` |
|      - | 9483 | `	{ "mb_convert_case", PH7_builtin_mb_convert_case_f },` |
|      - | 9484 | `	{ "mb_strpos",    PH7_builtin_mb_strpos_f },` |
|      - | 9485 | `	{ "mb_stripos",   PH7_builtin_mb_strpos_f },` |
|      - | 9486 | `	{ "mb_strrpos",   PH7_builtin_mb_strpos_f },` |
|      - | 9487 | `	{ "mb_str_split", PH7_builtin_mb_str_split_f },` |
|      - | 9488 | `	{ "mb_internal_encoding", PH7_builtin_mb_internal_encoding_f },` |
|      - | 9489 | `	{ "mb_check_encoding",    PH7_builtin_mb_check_encoding_f },` |
|      - | 9490 | `	{ "mb_strwidth",  PH7_builtin_mb_strwidth_f },` |
|      - | 9491 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 9492 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 9493 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 9494 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 9495 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 9496 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 9497 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 9498 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 9499 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 9500 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 9501 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 9502 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 9503 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 9504 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 9505 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 9506 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 9507 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - | 9508 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - | 9509 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - | 9510 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 9511 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9512 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9513 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 9514 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 9515 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 9516 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 9517 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9518 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9519 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 9520 |  |
|      - | 9521 |  |
|      - | 9522 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 9523 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 9524 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 9525 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 9526 | `	{ "hash",         PH7_builtin_hash      },` |
|      - | 9527 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|      - | 9528 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|      - | 9529 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|      - | 9530 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 9531 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|      - | 9532 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|      - | 9533 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|      - | 9534 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|      - | 9535 | `	{ "filter_var",            PH7_builtin_filter_var },` |
|      - | 9536 | `	{ "filter_input",          PH7_builtin_filter_input },` |
|      - | 9537 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9538 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9539 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 9540 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 9541 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9542 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9543 |  |
|      - | 9544 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 9545 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 9546 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 9547 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 9548 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 9549 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 9550 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 9551 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 9552 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 9553 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 9554 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 9555 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 9556 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9557 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9558 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 9559 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9560 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9561 |  |
|      - | 9562 | `	         /* Ctype functions */` |
|      - | 9563 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 9564 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 9565 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 9566 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 9567 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 9568 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 9569 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 9570 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 9571 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 9572 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 9573 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 9574 | `	         /* Time functions */` |
|      - | 9575 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 9576 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 9577 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 9578 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 9579 | `	{ "date",        PH7_builtin_date         },` |
|      - | 9580 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 9581 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 9582 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 9583 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 9584 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 9585 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 9586 | `	{ "date_default_timezone_get", PH7_builtin_date_default_timezone_get },` |
|      - | 9587 | `	{ "date_default_timezone_set", PH7_builtin_date_default_timezone_set },` |
|      - | 9588 | `	        /* URL functions */` |
|      - | 9589 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 9590 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 9591 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 9592 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 9593 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 9594 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 9595 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 9596 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 9597 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9598 | `};` |
|      - | 9599 | `/*` |
|      - | 9600 | ` * Register the built-in functions defined above,the array functions` |
|      - | 9601 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 9602 | ` */` |
|   3552 | 9603 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      5 | 9604 | `{` |
|      - | 9605 | `	sxu32 n;` |
| 660677 | 9606 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 657125 | 9607 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 328565 | 9608 | `	}` |
|      - | 9609 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   3557 | 9610 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 9611 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   3557 | 9612 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   3557 | 9613 | `}` |
|      - | 9614 |  |
