# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4291/5033 lines (85.26%)

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
| 468644 |   18 | `PH7_PRIVATE sxi32 PH7_IntArgResolve(` |
|      - |   19 | `	ph7_context *pCtx,` |
|      - |   20 | `	ph7_value *pArg,` |
|      - |   21 | `	const char *zFunc,` |
|      - |   22 | `	int iArgNum,` |
|      - |   23 | `	const char *zParamName,` |
|      - |   24 | `	const char *zTypeStr,` |
|      - |   25 | `	sxi64 *pOut` |
|      5 |   26 | `){` |
| 468649 |   27 | `	if( ph7_value_is_null(pArg) ){` |
|    ! 0 |   28 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |   29 | `			"%s(): Passing null to parameter #%d (%s) of type %s is deprecated",` |
|    ! 0 |   30 | `			zFunc,iArgNum,zParamName,zTypeStr` |
|      - |   31 | `			);` |
|    ! 0 |   32 | `		*pOut = 0;` |
|    ! 0 |   33 | `		return PH7_OK;` |
|      - |   34 | `	}` |
| 468649 |   35 | `	if( ph7_value_is_float(pArg) ){` |
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
| 468643 |   56 | `	if( ph7_value_is_string(pArg) ){` |
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
| 468629 |   98 | `	if( !ph7_value_is_int(pArg) && !ph7_value_is_bool(pArg) ){` |
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
| 468629 |  113 | `	*pOut = ph7_value_to_int64(pArg);` |
| 468629 |  114 | `	return PH7_OK;` |
| 234327 |  115 | `}` |
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
|      3 |  179 | `{` |
|    871 |  180 | `	int res = 0; /* Assume false by default */` |
|    871 |  181 | `	if( nArg > 0 ){` |
|      - |  182 | `		/* Strict PHP identity: a float is never an int, even when it holds an` |
|      - |  183 | `		 * integer value (1.0). An integer-valued real carries both MEMOBJ_INT` |
|      - |  184 | `		 * (cached) and MEMOBJ_REAL, so REAL must be excluded here. */` |
|    871 |  185 | `		res = ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]);` |
|    434 |  186 | `	}` |
|      - |  187 | `	/* Query result */` |
|    871 |  188 | `	ph7_result_bool(pCtx,res);` |
|    871 |  189 | `	return PH7_OK;` |
|      3 |  190 | `}` |
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
|  33536 |  411 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  412 | `{` |
|  33541 |  413 | `	int res = 1; /* Assume empty by default */` |
|  33541 |  414 | `	if( nArg > 0 ){` |
|  33539 |  415 | `		res = ph7_value_is_empty(apArg[0]);` |
|  16767 |  416 | `	}` |
|  33541 |  417 | `	ph7_result_bool(pCtx,res);` |
|  33541 |  418 | `	return PH7_OK;` |
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
| 254146 |  461 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  462 | `{` |
|      - |  463 | `	const char *zSource;` |
|      - |  464 | `	int nSrcLen;` |
|      - |  465 | `	sxi64 iStart,iEnd;` |
| 254151 |  466 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"substr",1,"$string"); }` |
| 254151 |  467 | `	if( nArg < 2 ){` |
|      - |  468 | `		/* Arity is enforced at the call boundary; nothing sensible to return here. */` |
|    ! 0 |  469 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 |  470 | `		return PH7_OK;` |
|      - |  471 | `	}` |
|      - |  472 | `	/* Extract the target string */` |
| 254151 |  473 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|      - |  474 | `	/* Extract the offset */` |
|      - |  475 | `	{` |
| 254151 |  476 | `		sxi64 iTmp = 0;` |
| 254151 |  477 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"substr",2,"$offset","int",&iTmp);` |
| 254151 |  478 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  479 | `			return rcArg;` |
|      - |  480 | `		}` |
| 254151 |  481 | `		iStart = iTmp;` |
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
| 254151 |  493 | `	if( iStart < 0 ){` |
|  32857 |  494 | `		iStart += nSrcLen;` |
|  32857 |  495 | `		if( iStart < 0 ){` |
|      5 |  496 | `			iStart = 0;` |
|      7 |  497 | `		}` |
| 237725 |  498 | `	}else if( iStart > nSrcLen ){` |
|      7 |  499 | `		iStart = nSrcLen;` |
|      3 |  500 | `	}` |
| 254151 |  501 | `	iEnd = nSrcLen;` |
| 254151 |  502 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
| 193559 |  503 | `		sxi64 iLen = 0;` |
| 193559 |  504 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[2],"substr",3,"$length","?int",&iLen);` |
| 193559 |  505 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  506 | `			return rcArg;` |
|      - |  507 | `		}` |
| 193559 |  508 | `		if( iLen < 0 ){` |
|  32787 |  509 | `			iEnd = (sxi64)nSrcLen + iLen;` |
| 177168 |  510 | `		}else if( iLen > (sxi64)nSrcLen - iStart ){` |
|  18629 |  511 | `			iEnd = nSrcLen;` |
|   9317 |  512 | `		}else{` |
| 142153 |  513 | `			iEnd = iStart + iLen;` |
|      - |  514 | `		}` |
|  96777 |  515 | `	}` |
| 254151 |  516 | `	if( iEnd < iStart ){` |
|      3 |  517 | `		iEnd = iStart;` |
|      1 |  518 | `	}` |
| 254151 |  519 | `	ph7_result_string(pCtx,&zSource[iStart],(int)(iEnd - iStart));` |
| 254151 |  520 | `	return PH7_OK;` |
| 127078 |  521 | `}` |
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
| 360770 |  716 | `static void StrNullArgNotice(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNum,const char *zParamName)` |
|      5 |  717 | `{` |
| 360775 |  718 | `	if( ph7_value_is_null(pArg) ){` |
|     25 |  719 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |  720 | `			"%s(): Passing null to parameter #%d (%s) of type string is deprecated",` |
|      8 |  721 | `			zFunc,iArgNum,zParamName);` |
|      8 |  722 | `	}` |
| 360775 |  723 | `}` |
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
|    104 | 1580 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256])` |
|      5 | 1581 | `{` |
|    109 | 1582 | `	const unsigned char *zIn  = (const unsigned char *)zList;` |
|    109 | 1583 | `	const unsigned char *zEnd = zIn + (nLen > 0 ? nLen : 0);` |
|    109 | 1584 | `	SyZero(aMask,256);` |
|    369 | 1585 | `	for( ; zIn < zEnd ; zIn++ ){` |
|    265 | 1586 | `		int c = zIn[0];` |
|    265 | 1587 | `		if( zIn + 3 < zEnd && zIn[1] == '.' && zIn[2] == '.' && zIn[3] >= c ){` |
|      - | 1588 | `			/* Valid incrementing range c..zIn[3] */` |
|     22 | 1589 | `			int hi = zIn[3],k;` |
|    386 | 1590 | `			for( k = c ; k <= hi ; k++ ){` |
|    366 | 1591 | `				aMask[k] = 1;` |
|    184 | 1592 | `			}` |
|     22 | 1593 | `			zIn += 3; /* the loop's ++ then steps past the range end */` |
|    264 | 1594 | `		}else if( zIn + 1 < zEnd && zIn[0] == '.' && zIn[1] == '.' ){` |
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
|    227 | 1616 | `			aMask[c] = 1;` |
|      - | 1617 | `		}` |
|    135 | 1618 | `	}` |
|    109 | 1619 | `}` |
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
|      4 | 1636 | `{` |
|      - | 1637 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - | 1638 | `	char aMask[256];` |
|      - | 1639 | `	int nLen,nMask;` |
|      - | 1640 | `	/* PHP enforces exactly two arguments. */` |
|     38 | 1641 | `	if( nArg != 2 ){` |
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
|     21 | 1738 | `}` |
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
|  55874 | 2016 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2017 | `{` |
|  55879 | 2018 | `	int iLen = 0;` |
|  55879 | 2019 | `	if( nArg > 0 ){` |
|  55879 | 2020 | `		StrNullArgNotice(pCtx,apArg[0],"strlen",1,"$string");` |
|  55879 | 2021 | `		ph7_value_to_string(apArg[0],&iLen);` |
|  27937 | 2022 | `	}` |
|      - | 2023 | `	/* String length */` |
|  55879 | 2024 | `	ph7_result_int(pCtx,iLen);` |
|  55879 | 2025 | `	return PH7_OK;` |
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
|     38 | 2213 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2214 | `{` |
|      - | 2215 | `	const char *z1,*z2;` |
|      - | 2216 | `	int res;` |
|      - | 2217 | `	int n;` |
|     43 | 2218 | `	if( nArg < 3 ){` |
|      - | 2219 | `		/* Perform a standard comparison */` |
|    ! 0 | 2220 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 2221 | `	}` |
|      - | 2222 | `	/* Desired comparison length */` |
|     43 | 2223 | `	n  = ph7_value_to_int(apArg[2]);` |
|     43 | 2224 | `	if( n < 0 ){` |
|      - | 2225 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 2226 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2227 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 2228 | `			ph7_function_name(pCtx));` |
|      - | 2229 | `	}` |
|      - | 2230 | `	/* Perform the comparison */` |
|     41 | 2231 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     41 | 2232 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     41 | 2233 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 2234 | `	/* Comparison result */` |
|     41 | 2235 | `	ph7_result_int(pCtx,res);` |
|     41 | 2236 | `	return PH7_OK;` |
|     24 | 2237 | `}` |
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
| 149022 | 2257 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 2258 | `{` |
|  74511 | 2259 | `	SXUNUSED(pKey);` |
| 149027 | 2260 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 2261 | `	const char *zData;` |
|      - | 2262 | `	int nLen;` |
| 149027 | 2263 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
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
| 149025 | 2287 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 2288 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 149025 | 2289 | `	if( pData->bFirst ){` |
|  33263 | 2290 | `		pData->bFirst = 0;` |
| 132396 | 2291 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 2292 | `		/* append the separator first */` |
| 115751 | 2293 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 2294 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 2295 | `			return PH7_ABORT;` |
|      - | 2296 | `		}` |
|  57873 | 2297 | `	}` |
|      - | 2298 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 149025 | 2299 | `	if( nLen > 0 ){` |
| 136437 | 2300 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2301 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 2302 | `			return PH7_ABORT;` |
|      - | 2303 | `		}` |
|  68216 | 2304 | `	}` |
| 149025 | 2305 | `	return PH7_OK;` |
|  74516 | 2306 | `}` |
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
|  33280 | 2320 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2321 | `{` |
|      - | 2322 | `	struct implode_data imp_data;` |
|  33285 | 2323 | `	int i = 1;` |
|  33285 | 2324 | `	if( nArg < 1 ){` |
|      - | 2325 | `		/* Missing argument,return NULL */` |
|    ! 0 | 2326 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2327 | `		return PH7_OK;` |
|      - | 2328 | `	}` |
|      - | 2329 | `	/* Prepare the implode context */` |
|  33285 | 2330 | `	imp_data.pCtx = pCtx;` |
|  33285 | 2331 | `	imp_data.bRecursive = 0;` |
|  33285 | 2332 | `	imp_data.bFirst = 1;` |
|  33285 | 2333 | `	imp_data.nRecCount = 0;` |
|  33285 | 2334 | `	imp_data.rc = SXRET_OK;` |
|  33285 | 2335 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  33283 | 2336 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  16644 | 2337 | `	}else{` |
|      3 | 2338 | `		imp_data.zSep = 0;` |
|      3 | 2339 | `		imp_data.nSeplen = 0;` |
|      3 | 2340 | `		i = 0;` |
|      - | 2341 | `	}` |
|  33285 | 2342 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2343 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2344 | `	}` |
|      - | 2345 | `	/* Start the 'join' process */` |
|  66565 | 2346 | `	while( i < nArg ){` |
|  33285 | 2347 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2348 | `			/* Iterate throw array entries */` |
|  33285 | 2349 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2350 | `			/* Surface a callback allocation failure as a fatal */` |
|  33285 | 2351 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2352 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2353 | `			}` |
|  16645 | 2354 | `		}else{` |
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
|  33285 | 2374 | `		i++;` |
|      5 | 2375 | `	}` |
|  33285 | 2376 | `	return PH7_OK;` |
|  16645 | 2377 | `}` |
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
|   6580 | 2477 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2478 | `{` |
|      - | 2479 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 2480 | `	int nDelim,nStrlen,iLimit;` |
|      - | 2481 | `	ph7_value *pArray;` |
|      - | 2482 | `	ph7_value *pValue;` |
|      - | 2483 | `	sxu32 nOfft;` |
|      - | 2484 | `	sxi32 rc;` |
|   6585 | 2485 | `	if( nArg < 2 ){` |
|      - | 2486 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2487 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2488 | `		return PH7_OK;` |
|      - | 2489 | `	}` |
|      - | 2490 | `	/* Extract the delimiter */` |
|   6585 | 2491 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   6585 | 2492 | `	if( nDelim < 1 ){` |
|      - | 2493 | `		/* Empty delimiter: PHP 8 throws a catchable ValueError. */` |
|      5 | 2494 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2495 | `			"explode(): Argument #1 ($separator) must not be empty");` |
|      - | 2496 | `	}` |
|      - | 2497 | `	/* Extract the string */` |
|   6581 | 2498 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   6581 | 2499 | `	if( nStrlen < 1 ){` |
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
|   6575 | 2525 | `	zEnd = &zString[nStrlen];` |
|      - | 2526 | `	/* Create the array */` |
|   6575 | 2527 | `	pArray =  ph7_context_new_array(pCtx);` |
|   6575 | 2528 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   6575 | 2529 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2530 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 2531 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2532 | `		return PH7_OK;` |
|      - | 2533 | `	}` |
|      - | 2534 | `	/* Set a defualt limit */` |
|   6575 | 2535 | `	iLimit = SXI32_HIGH;` |
|   6575 | 2536 | `	if( nArg > 2 ){` |
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
|  80301 | 2571 | `	for(;;){` |
| 160607 | 2572 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 160607 | 2573 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 2574 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   6559 | 2575 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   6559 | 2576 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2577 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2578 | `			}` |
|   6559 | 2579 | `			break;` |
|      - | 2580 | `		}` |
|      - | 2581 | `		/* Point to the desired offset */` |
| 154053 | 2582 | `		zCur = &zString[nOfft];` |
|      - | 2583 | `		/* Perform the store operation (may be empty) */` |
| 154053 | 2584 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 154053 | 2585 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2586 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2587 | `		}` |
|      - | 2588 | `		/* Point beyond the delimiter */` |
| 154053 | 2589 | `		zString = &zCur[nDelim];` |
|      - | 2590 | `		/* Reset the cursor */` |
| 154053 | 2591 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 2592 | `	}` |
|      - | 2593 | `	/* Return the freshly created array */` |
|   6559 | 2594 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2595 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 2596 | `	 * released as soon we return from this foregin function.` |
|      - | 2597 | `	 */` |
|   6559 | 2598 | `	return PH7_OK;` |
|   3295 | 2599 | `}` |
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
|  14414 | 2615 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2616 | `{` |
|  14419 | 2617 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"trim",1,"$string"); }` |
|      - | 2618 | `	const char *zString;` |
|      - | 2619 | `	int nLen;` |
|  14419 | 2620 | `	if( nArg < 1 ){` |
|      - | 2621 | `		/* Missing arguments,return null */` |
|    ! 0 | 2622 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2623 | `		return PH7_OK;` |
|      - | 2624 | `	}` |
|      - | 2625 | `	/* Extract the target string */` |
|  14419 | 2626 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  14419 | 2627 | `	if( nLen < 1 ){` |
|      - | 2628 | `		/* Empty string,return */` |
|    837 | 2629 | `		ph7_result_string(pCtx,"",0);` |
|    837 | 2630 | `		return PH7_OK;` |
|      - | 2631 | `	}` |
|      - | 2632 | `	/* Start the trim process */` |
|  13587 | 2633 | `	if( nArg < 2 ){` |
|      - | 2634 | `		SyString sStr;` |
|      - | 2635 | `		/* Remove white spaces and NUL bytes */` |
|  13557 | 2636 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  34009 | 2637 | `		SyStringFullTrimSafe(&sStr);` |
|  13557 | 2638 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   6781 | 2639 | `	}else{` |
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
|  13587 | 2668 | `	return PH7_OK;` |
|   7212 | 2669 | `}` |
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
|     38 | 2685 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2686 | `{` |
|     41 | 2687 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"rtrim",1,"$string"); }` |
|      - | 2688 | `	const char *zString;` |
|      - | 2689 | `	int nLen;` |
|     41 | 2690 | `	if( nArg < 1 ){` |
|      - | 2691 | `		/* Missing arguments,return null */` |
|    ! 0 | 2692 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2693 | `		return PH7_OK;` |
|      - | 2694 | `	}` |
|      - | 2695 | `	/* Extract the target string */` |
|     41 | 2696 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 2697 | `	if( nLen < 1 ){` |
|      - | 2698 | `		/* Empty string,return */` |
|      7 | 2699 | `		ph7_result_string(pCtx,"",0);` |
|      7 | 2700 | `		return PH7_OK;` |
|      - | 2701 | `	}` |
|      - | 2702 | `	/* Start the trim process */` |
|     35 | 2703 | `	if( nArg < 2 ){` |
|      - | 2704 | `		SyString sStr;` |
|      - | 2705 | `		/* Remove white spaces and NUL bytes*/` |
|     19 | 2706 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     48 | 2707 | `		SyStringRightTrimSafe(&sStr);` |
|     19 | 2708 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|     10 | 2709 | `	}else{` |
|      - | 2710 | `		/* Char list */` |
|      - | 2711 | `		const char *zList;` |
|      - | 2712 | `		int nListlen;` |
|     17 | 2713 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     17 | 2714 | `		if( nListlen < 1 ){` |
|      - | 2715 | `			/* Return the string unchanged */` |
|    ! 0 | 2716 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 2717 | `		}else{` |
|      - | 2718 | `			char aMask[256];` |
|     17 | 2719 | `			const char *zEnd = &zString[nLen];` |
|     17 | 2720 | `			const char *zCur = zString;` |
|     17 | 2721 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2722 | `			/* Right trim */` |
|     37 | 2723 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     22 | 2724 | `				zEnd--;` |
|      2 | 2725 | `			}` |
|     17 | 2726 | `			if( zEnd <= zCur ){` |
|      - | 2727 | `				/* Return the empty string */` |
|    ! 0 | 2728 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2729 | `			}else{` |
|     17 | 2730 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2731 | `			}` |
|      - | 2732 | `		}` |
|      - | 2733 | `	}` |
|     35 | 2734 | `	return PH7_OK;` |
|     22 | 2735 | `}` |
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
|     40 | 2751 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2752 | `{` |
|     45 | 2753 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"ltrim",1,"$string"); }` |
|      - | 2754 | `	const char *zString;` |
|      - | 2755 | `	int nLen;` |
|     45 | 2756 | `	if( nArg < 1 ){` |
|      - | 2757 | `		/* Missing arguments,return null */` |
|    ! 0 | 2758 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2759 | `		return PH7_OK;` |
|      - | 2760 | `	}` |
|      - | 2761 | `	/* Extract the target string */` |
|     45 | 2762 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     45 | 2763 | `	if( nLen < 1 ){` |
|      - | 2764 | `		/* Empty string,return */` |
|     23 | 2765 | `		ph7_result_string(pCtx,"",0);` |
|     23 | 2766 | `		return PH7_OK;` |
|      - | 2767 | `	}` |
|      - | 2768 | `	/* Start the trim process */` |
|     27 | 2769 | `	if( nArg < 2 ){` |
|      - | 2770 | `		SyString sStr;` |
|      - | 2771 | `		/* Remove white spaces and NUL byte */` |
|      5 | 2772 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     13 | 2773 | `		SyStringLeftTrimSafe(&sStr);` |
|      5 | 2774 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      3 | 2775 | `	}else{` |
|      - | 2776 | `		/* Char list */` |
|      - | 2777 | `		const char *zList;` |
|      - | 2778 | `		int nListlen;` |
|     23 | 2779 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     23 | 2780 | `		if( nListlen < 1 ){` |
|      - | 2781 | `			/* Return the string unchanged */` |
|      3 | 2782 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2783 | `		}else{` |
|      - | 2784 | `			char aMask[256];` |
|     21 | 2785 | `			const char *zEnd = &zString[nLen];` |
|     21 | 2786 | `			const char *zCur = zString;` |
|     21 | 2787 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2788 | `			/* Left trim */` |
|     51 | 2789 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     34 | 2790 | `				zCur++;` |
|      4 | 2791 | `			}` |
|     21 | 2792 | `			if( zCur >= zEnd ){` |
|      - | 2793 | `				/* Return the empty string */` |
|    ! 0 | 2794 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2795 | `			}else{` |
|     21 | 2796 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2797 | `			}` |
|      - | 2798 | `		}` |
|      - | 2799 | `	}` |
|     27 | 2800 | `	return PH7_OK;` |
|     25 | 2801 | `}` |
|      - | 2802 | `/*` |
|      - | 2803 | ` * string strtolower(string $str)` |
|      - | 2804 | ` *  Make a string lowercase.` |
|      - | 2805 | ` * Parameters` |
|      - | 2806 | ` *  $str` |
|      - | 2807 | ` *   The input string.` |
|      - | 2808 | ` * Returns.` |
|      - | 2809 | ` *  The lowercased string.` |
|      - | 2810 | ` */` |
|  33252 | 2811 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2812 | `{` |
|  33257 | 2813 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtolower",1,"$string"); }` |
|      - | 2814 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2815 | `	int nLen;` |
|  33257 | 2816 | `	if( nArg < 1 ){` |
|      - | 2817 | `		/* Missing arguments,return null */` |
|    ! 0 | 2818 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2819 | `		return PH7_OK;` |
|      - | 2820 | `	}` |
|      - | 2821 | `	/* Extract the target string */` |
|  33257 | 2822 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  33257 | 2823 | `	if( nLen < 1 ){` |
|      - | 2824 | `		/* Empty string,return */` |
|      5 | 2825 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2826 | `		return PH7_OK;` |
|      - | 2827 | `	}` |
|      - | 2828 | `	/* Perform the requested operation */` |
|  33253 | 2829 | `	zEnd = &zString[nLen];` |
| 104874 | 2830 | `	for(;;){` |
| 209753 | 2831 | `		if( zString >= zEnd ){` |
|      - | 2832 | `			/* No more input,break immediately */` |
|  33253 | 2833 | `			break;` |
|      - | 2834 | `		}` |
| 176505 | 2835 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2836 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2837 | `			zCur = zString;` |
|    ! 0 | 2838 | `			zString++;` |
|    ! 0 | 2839 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2840 | `				zString++;` |
|    ! 0 | 2841 | `			}` |
|      - | 2842 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2843 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2844 | `		}else{` |
| 176505 | 2845 | `			int c = zString[0];` |
| 176505 | 2846 | `			if( SyisUpper(c) ){` |
| 173949 | 2847 | `				c = SyToLower(zString[0]);` |
|  86972 | 2848 | `			}` |
|      - | 2849 | `			/* Append character */` |
| 176505 | 2850 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2851 | `			/* Advance the cursor */` |
| 176505 | 2852 | `			zString++;` |
|      - | 2853 | `		}` |
|      5 | 2854 | `	}` |
|  33253 | 2855 | `	return PH7_OK;` |
|  16631 | 2856 | `}` |
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
|    697 | 3387 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|    351 | 3388 | `	}else{` |
|      3 | 3389 | `		ph7_result_bool(pCtx,0);` |
|      - | 3390 | `	}` |
|    699 | 3391 | `	return PH7_OK;` |
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
|    412 | 4228 | `static int FormatUnknownSpec(const char *zIn,int nByte,int *pBad)` |
|      1 | 4229 | `{` |
|    413 | 4230 | `	const char *zEnd = &zIn[nByte];` |
|      - | 4231 | `	int c,idx;` |
|   3449 | 4232 | `	while( zIn < zEnd ){` |
|   3057 | 4233 | `		if( zIn[0] != '%' ){` |
|   2265 | 4234 | `			zIn++;` |
|   2265 | 4235 | `			continue;` |
|      - | 4236 | `		}` |
|    793 | 4237 | `		zIn++; /* jump the percent sign */` |
|      - | 4238 | `		/* php-supported flags: '-', '+', ' ', '0' and the "'<pad>'" custom-pad` |
|      - | 4239 | `		 * form. '#' is intentionally NOT treated as a flag so it surfaces as an` |
|      - | 4240 | `		 * unknown specifier, matching php. */` |
|    977 | 4241 | `		while( zIn < zEnd ){` |
|    975 | 4242 | `			c = zIn[0];` |
|    975 | 4243 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|    185 | 4244 | `				zIn++;` |
|    185 | 4245 | `				continue;` |
|      - | 4246 | `			}` |
|    791 | 4247 | `			if( c=='\'' ){` |
|    ! 0 | 4248 | `				zIn++;` |
|    ! 0 | 4249 | `				if( zIn < zEnd ){` |
|    ! 0 | 4250 | `					zIn++; /* the custom pad character */` |
|    ! 0 | 4251 | `				}` |
|    ! 0 | 4252 | `				continue;` |
|      - | 4253 | `			}` |
|    791 | 4254 | `			break;` |
|    ! 0 | 4255 | `		}` |
|      - | 4256 | `		/* field width */` |
|   1009 | 4257 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    217 | 4258 | `			zIn++;` |
|      1 | 4259 | `		}` |
|      - | 4260 | `		/* positional specifier ($) — php parses flags AFTER it (e.g. "%1$-10s"),` |
|      - | 4261 | `		 * so skip the full flag set and width again, mirroring the main loop. */` |
|    793 | 4262 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
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
|    793 | 4284 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|     87 | 4285 | `			zIn++;` |
|    183 | 4286 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|     97 | 4287 | `				zIn++;` |
|      1 | 4288 | `			}` |
|     43 | 4289 | `		}` |
|      - | 4290 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|    793 | 4291 | `		if( zIn < zEnd && zIn[0]=='l' ){` |
|     11 | 4292 | `			zIn++;` |
|      5 | 4293 | `		}` |
|    793 | 4294 | `		if( zIn >= zEnd ){` |
|      - | 4295 | `			/* A dangling '%' with no specifier: PHL's legacy path silently` |
|      - | 4296 | `			 * truncates here (recorded residual); nothing to validate. */` |
|      3 | 4297 | `			break;` |
|      - | 4298 | `		}` |
|    791 | 4299 | `		c = zIn[0];` |
|    791 | 4300 | `		zIn++; /* jump the conversion specifier */` |
|   3333 | 4301 | `		for( idx = 0 ; idx < (int)SX_ARRAYSIZE(aFmt) ; idx++ ){` |
|   3315 | 4302 | `			if( c == aFmt[idx].fmttype ){` |
|    773 | 4303 | `				break;` |
|      - | 4304 | `			}` |
|   1272 | 4305 | `		}` |
|    791 | 4306 | `		if( idx >= (int)SX_ARRAYSIZE(aFmt) ){` |
|     19 | 4307 | `			*pBad = c; /* unknown specifier */` |
|     19 | 4308 | `			return TRUE;` |
|      - | 4309 | `		}` |
|      1 | 4310 | `	}` |
|    395 | 4311 | `	return FALSE;` |
|    207 | 4312 | `}` |
|      - | 4313 | `/*` |
|      - | 4314 | ` * Validate a printf-style format string. PHP 8 raises a catchable ValueError for` |
|      - | 4315 | ` * an unknown conversion specifier, thrown before any output is produced. Every` |
|      - | 4316 | ` * format builtin (sprintf/printf/vprintf/vsprintf/fprintf/vfprintf) calls this` |
|      - | 4317 | ` * up-front, then propagates the returned status verbatim (PH7_EXCEPTION when the` |
|      - | 4318 | ` * throw is caught in place, PH7_ABORT when it goes uncaught).` |
|      - | 4319 | ` * Returns PH7_OK when the format is valid.` |
|      - | 4320 | ` */` |
|    412 | 4321 | `PH7_PRIVATE sxi32 PH7_FormatValidate(ph7_context *pCtx,const char *zFormat,int nByte)` |
|      1 | 4322 | `{` |
|    413 | 4323 | `	int badSpec = 0;` |
|    413 | 4324 | `	if( FormatUnknownSpec(zFormat,nByte,&badSpec) ){` |
|     28 | 4325 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      9 | 4326 | `			"Unknown format specifier \"%c\"",badSpec);` |
|      - | 4327 | `	}` |
|    395 | 4328 | `	return PH7_OK;` |
|    207 | 4329 | `}` |
|      - | 4330 | `/*` |
|      - | 4331 | `` * PHP 8: a printf-family `$format` argument is a `string` parameter — scalars`` |
|      - | 4332 | ` * (int/float/bool) and null coerce to a string, but an array/object/resource` |
|      - | 4333 | ` * raises a catchable TypeError. iArg is the 1-based argument position ($format` |
|      - | 4334 | ` * is #1 for sprintf/printf/vprintf/vsprintf, #2 for fprintf/vfprintf). Returns` |
|      - | 4335 | ` * PH7_OK when the value is string-coercible (the caller then uses` |
|      - | 4336 | ` * ph7_value_to_string, which renders scalars/null verbatim).` |
|      - | 4337 | ` */` |
|    424 | 4338 | `PH7_PRIVATE sxi32 PH7_FormatCheckFormatArg(ph7_context *pCtx,ph7_value *pArg,int iArg)` |
|      1 | 4339 | `{` |
|    425 | 4340 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg) \|\| ph7_value_is_resource(pArg) ){` |
|      - | 4341 | `		char zBuf[64];` |
|    ! 0 | 4342 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4343 | `			"%s(): Argument #%d ($format) must be of type string, %s given",` |
|    ! 0 | 4344 | `			ph7_function_name(pCtx),iArg,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|      - | 4345 | `	}` |
|    425 | 4346 | `	return PH7_OK;` |
|    213 | 4347 | `}` |
|      - | 4348 | `/*` |
|      - | 4349 | ` * Format a given string.` |
|      - | 4350 | ` * The root program.  All variations call this core.` |
|      - | 4351 | ` * INPUTS:` |
|      - | 4352 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 4353 | ` *            1. A pointer to the call context.` |
|      - | 4354 | ` *            2. A pointer to the list of characters to be output` |
|      - | 4355 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 4356 | ` *            3. An integer number of characters to be output.` |
|      - | 4357 | ` *               (Note: This number might be zero.)` |
|      - | 4358 | ` *            4. Upper layer private data.` |
|      - | 4359 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 4360 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 4361 | ` */` |
|    394 | 4362 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 4363 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 4364 | `	ph7_context *pCtx,  /* call context */` |
|      - | 4365 | `	const char *zIn,    /* Format string */` |
|      - | 4366 | `	int nByte,          /* Format string length */` |
|      - | 4367 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 4368 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 4369 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 4370 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 4371 | `	)` |
|      1 | 4372 | `{` |
|    395 | 4373 | `	char spaces[] = "                                                  ";` |
|      - | 4374 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    395 | 4375 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 4376 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 4377 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 4378 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 4379 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 4380 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 4381 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 4382 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 4383 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 4384 | `	ph7_int64 iVal;` |
|      - | 4385 | `	int precision;           /* Precision of the current field */` |
|      - | 4386 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 4387 | `	int c,rc,n;` |
|      - | 4388 | `	int length;              /* Length of the field */` |
|      - | 4389 | `	int prefix;` |
|      - | 4390 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 4391 | `	int width;               /* Width of the current field */` |
|      - | 4392 | `	int idx;` |
|    395 | 4393 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 4394 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 4395 | `	/* An unknown conversion specifier is rejected up-front by PH7_FormatValidate()` |
|      - | 4396 | `	 * (called by every format builtin before this routine), so the specifier set` |
|      - | 4397 | `	 * seen here is always valid. */` |
|      - | 4398 | `	/* Start the format process */` |
|    583 | 4399 | `	for(;;){` |
|   1167 | 4400 | `		zCur = zIn;` |
|   3417 | 4401 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|   2251 | 4402 | `			zIn++;` |
|      1 | 4403 | `		}` |
|   1167 | 4404 | `		if( zCur < zIn ){` |
|      - | 4405 | `			/* Consume chunk verbatim */` |
|    725 | 4406 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|    725 | 4407 | `			if( rc != SXRET_OK ){` |
|      - | 4408 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|    ! 0 | 4409 | `				break;` |
|      - | 4410 | `			}` |
|    362 | 4411 | `		}` |
|   1167 | 4412 | `		if( zIn >= zEnd ){` |
|      - | 4413 | `			/* No more input to process,break immediately */` |
|    393 | 4414 | `			break;` |
|      - | 4415 | `		}` |
|      - | 4416 | `		/* Find out what flags are present */` |
|    775 | 4417 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    774 | 4418 | `			flag_alternateform = flag_zeropad = 0;` |
|    775 | 4419 | `		zIn++; /* Jump the precent sign */` |
|    387 | 4420 | `		do{` |
|    959 | 4421 | `			c = zIn[0];` |
|    959 | 4422 | `			switch( c ){` |
|     15 | 4423 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      7 | 4424 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 4425 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|    159 | 4426 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 4427 | `			case '\'':` |
|    ! 0 | 4428 | `				zIn++;` |
|    ! 0 | 4429 | `				if( zIn < zEnd ){` |
|      - | 4430 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 4431 | `					c = zIn[0];` |
|    ! 0 | 4432 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4433 | `						spaces[idx] = (char)c;` |
|    ! 0 | 4434 | `					}` |
|    ! 0 | 4435 | `					c = 0;` |
|    ! 0 | 4436 | `				}` |
|    ! 0 | 4437 | `				break;` |
|    774 | 4438 | `			default:                                       break;` |
|      - | 4439 | `			}` |
|    959 | 4440 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 4441 | `		/* Get the field width */` |
|    775 | 4442 | `		width = 0;` |
|   1378 | 4443 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    217 | 4444 | `			width = width*10 + (zIn[0] - '0');` |
|    217 | 4445 | `			zIn++;` |
|      1 | 4446 | `		}` |
|    775 | 4447 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 4448 | `			/* Position specifer */` |
|    ! 0 | 4449 | `			if( width > 0 ){` |
|    ! 0 | 4450 | `				n = width;` |
|    ! 0 | 4451 | `				if( vf && n > 0 ){` |
|    ! 0 | 4452 | `					n--;` |
|    ! 0 | 4453 | `				}` |
|    ! 0 | 4454 | `			}` |
|    ! 0 | 4455 | `			zIn++;` |
|    ! 0 | 4456 | `			width = 0;` |
|      - | 4457 | `			/* php's grammar is %argnum$<flags><width>: the flags come AFTER the` |
|      - | 4458 | `			 * positional, so re-parse the full flag set here (e.g. "%1$-10s"),` |
|      - | 4459 | `			 * not just zero-padding. */` |
|    ! 0 | 4460 | `			do{` |
|    ! 0 | 4461 | `				c = zIn[0];` |
|    ! 0 | 4462 | `				switch( c ){` |
|    ! 0 | 4463 | `				case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|    ! 0 | 4464 | `				case '+':   flag_plussign = 1;        c = 0;   break;` |
|    ! 0 | 4465 | `				case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|    ! 0 | 4466 | `				case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 4467 | `				case '\'':` |
|    ! 0 | 4468 | `					zIn++;` |
|    ! 0 | 4469 | `					if( zIn < zEnd ){` |
|    ! 0 | 4470 | `						c = zIn[0];` |
|    ! 0 | 4471 | `						for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4472 | `							spaces[idx] = (char)c;` |
|    ! 0 | 4473 | `						}` |
|    ! 0 | 4474 | `						c = 0;` |
|    ! 0 | 4475 | `					}` |
|    ! 0 | 4476 | `					break;` |
|    ! 0 | 4477 | `				default:                                       break;` |
|      - | 4478 | `				}` |
|    ! 0 | 4479 | `			}while( c==0 && (zIn++ < zEnd) );` |
|    ! 0 | 4480 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 4481 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 4482 | `				zIn++;` |
|    ! 0 | 4483 | `			}` |
|    ! 0 | 4484 | `		}` |
|    775 | 4485 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 4486 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 4487 | `		}` |
|      - | 4488 | `		/* Get the precision */` |
|    775 | 4489 | `		precision = -1;` |
|    775 | 4490 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     87 | 4491 | `			precision = 0;` |
|     87 | 4492 | `			zIn++;` |
|    226 | 4493 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     97 | 4494 | `				precision = precision*10 + (zIn[0] - '0');` |
|     97 | 4495 | `				zIn++;` |
|      1 | 4496 | `			}` |
|     43 | 4497 | `		}` |
|      - | 4498 | `		/* Consume a single 'l' length modifier (a C-ism php accepts and ignores,` |
|      - | 4499 | `		 * e.g. "%ld"); PH7_FormatValidate mirrors this. Exactly one is skipped:` |
|      - | 4500 | `		 * in "%lld" the second 'l' becomes the (unknown) specifier, just like php. */` |
|    775 | 4501 | `		if( zIn < zEnd && zIn[0] == 'l' ){` |
|      9 | 4502 | `			zIn++;` |
|      4 | 4503 | `		}` |
|    775 | 4504 | `		if( zIn >= zEnd ){` |
|      - | 4505 | `			/* No more input */` |
|      3 | 4506 | `			break;` |
|      - | 4507 | `		}` |
|      - | 4508 | `		/* Fetch the info entry for the field */` |
|    773 | 4509 | `		pInfo = 0;` |
|    773 | 4510 | `		xtype = PH7_FMT_ERROR;` |
|    773 | 4511 | `		c = zIn[0];` |
|    773 | 4512 | `		zIn++; /* Jump the format specifer */` |
|   3009 | 4513 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|   3009 | 4514 | `			if( c==aFmt[idx].fmttype ){` |
|    773 | 4515 | `				pInfo = &aFmt[idx];` |
|    773 | 4516 | `				xtype = pInfo->type;` |
|    773 | 4517 | `				break;` |
|      - | 4518 | `			}` |
|   1119 | 4519 | `		}` |
|    773 | 4520 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    773 | 4521 | `		length = 0;` |
|      - | 4522 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 4523 | `		 /*` |
|      - | 4524 | `		  ** At this point, variables are initialized as follows:` |
|      - | 4525 | `		  **` |
|      - | 4526 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 4527 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 4528 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 4529 | `		  **                               field width was negative.` |
|      - | 4530 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 4531 | `		  **                               the conversion character.` |
|      - | 4532 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 4533 | `		  **   width                       The specified field width.  This is` |
|      - | 4534 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 4535 | `		  **   precision                   The specified precision.  The default` |
|      - | 4536 | `		  **                               is -1.` |
|      - | 4537 | `		  */` |
|    773 | 4538 | `		switch(xtype){` |
|      3 | 4539 | `		case PH7_FMT_PERCENT:` |
|      - | 4540 | `			/* A literal percent character */` |
|      7 | 4541 | `			zWorker[0] = '%';` |
|      7 | 4542 | `			length = (int)sizeof(char);` |
|      7 | 4543 | `			break;` |
|      3 | 4544 | `		case PH7_FMT_CHARX:` |
|      - | 4545 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 4546 | `			 * with that ASCII value` |
|      - | 4547 | `			 */` |
|      7 | 4548 | `			pArg = NEXT_ARG;` |
|      7 | 4549 | `			if( pArg == 0 ){` |
|      3 | 4550 | `				c = 0;` |
|      2 | 4551 | `			}else{` |
|      5 | 4552 | `				c = ph7_value_to_int(pArg);` |
|      - | 4553 | `			}` |
|      - | 4554 | `			/* NUL byte is an acceptable value */` |
|      7 | 4555 | `			zWorker[0] = (char)c;` |
|      7 | 4556 | `			length = (int)sizeof(char);` |
|      7 | 4557 | `			break;` |
|    162 | 4558 | `		case PH7_FMT_STRING:` |
|      - | 4559 | `			/* the argument is treated as and presented as a string */` |
|    325 | 4560 | `			pArg = NEXT_ARG;` |
|    325 | 4561 | `			if( pArg == 0 ){` |
|    ! 0 | 4562 | `				length = 0;` |
|    ! 0 | 4563 | `			}else{` |
|    325 | 4564 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 4565 | `			}` |
|    325 | 4566 | `			if( length < 1 ){` |
|    ! 0 | 4567 | `				zBuf = " ";` |
|    ! 0 | 4568 | `				length = (int)sizeof(char);` |
|    ! 0 | 4569 | `			}` |
|    325 | 4570 | `			if( precision>=0 && precision<length ){` |
|      3 | 4571 | `				length = precision;` |
|      1 | 4572 | `			}` |
|    325 | 4573 | `			if( flag_zeropad ){` |
|      - | 4574 | `				/* zero-padding works on strings too */` |
|    ! 0 | 4575 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4576 | `					spaces[idx] = '0';` |
|    ! 0 | 4577 | `				}` |
|    ! 0 | 4578 | `			}` |
|    325 | 4579 | `			break;` |
|    130 | 4580 | `		case PH7_FMT_RADIX:` |
|    261 | 4581 | `			pArg = NEXT_ARG;` |
|    261 | 4582 | `			if( pArg == 0 ){` |
|    ! 0 | 4583 | `				iVal = 0;` |
|    ! 0 | 4584 | `			}else{` |
|    261 | 4585 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 4586 | `			}` |
|      - | 4587 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|    261 | 4588 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 4589 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 4590 | `			}` |
|      - | 4591 | `#if 1` |
|      - | 4592 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 4593 | `        ** I think this is stupid.*/` |
|    261 | 4594 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 4595 | `#else` |
|      - | 4596 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 4597 | `        ** but leave the prefix for hex.*/` |
|      - | 4598 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 4599 | `#endif` |
|    261 | 4600 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|    237 | 4601 | `          if( iVal<0 ){` |
|     25 | 4602 | `            iVal = -iVal;` |
|      - | 4603 | `			/* Ticket 1433-003 */` |
|     25 | 4604 | `			if( iVal < 0 ){` |
|      - | 4605 | `				/* Overflow */` |
|    ! 0 | 4606 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4607 | `			}` |
|     25 | 4608 | `            prefix = '-';` |
|    225 | 4609 | `          }else if( flag_plussign )  prefix = '+';` |
|    211 | 4610 | `          else if( flag_blanksign )  prefix = ' ';` |
|    209 | 4611 | `          else                       prefix = 0;` |
|    119 | 4612 | `        }else{` |
|     25 | 4613 | `			if( iVal<0 ){` |
|    ! 0 | 4614 | `				iVal = -iVal;` |
|      - | 4615 | `				/* Ticket 1433-003 */` |
|    ! 0 | 4616 | `				if( iVal < 0 ){` |
|      - | 4617 | `					/* Overflow */` |
|    ! 0 | 4618 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4619 | `				}` |
|    ! 0 | 4620 | `			}` |
|     25 | 4621 | `			prefix = 0;` |
|      - | 4622 | `		}` |
|    261 | 4623 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|    149 | 4624 | `          precision = width-(prefix!=0);` |
|     74 | 4625 | `        }` |
|    261 | 4626 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 4627 | `        {` |
|      - | 4628 | `          register char *cset;      /* Use registers for speed */` |
|      - | 4629 | `          register int base;` |
|    261 | 4630 | `          cset = pInfo->charset;` |
|    261 | 4631 | `          base = pInfo->base;` |
|    130 | 4632 | `          do{                                           /* Convert to ascii */` |
|    333 | 4633 | `            *(--zBuf) = cset[iVal%base];` |
|    333 | 4634 | `            iVal = iVal/base;` |
|    333 | 4635 | `          }while( iVal>0 );` |
|      - | 4636 | `        }` |
|    261 | 4637 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    427 | 4638 | `        for(idx=precision-length; idx>0; idx--){` |
|    167 | 4639 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|     84 | 4640 | `        }` |
|    261 | 4641 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|    261 | 4642 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 4643 | `          char *pre, x;` |
|    ! 0 | 4644 | `          pre = pInfo->prefix;` |
|    ! 0 | 4645 | `          if( *zBuf!=pre[0] ){` |
|    ! 0 | 4646 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|    ! 0 | 4647 | `          }` |
|    ! 0 | 4648 | `        }` |
|    261 | 4649 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    261 | 4650 | `		break;` |
|     88 | 4651 | `		case PH7_FMT_FLOAT:` |
|      - | 4652 | `		case PH7_FMT_EXP:` |
|      - | 4653 | `		case PH7_FMT_GENERIC:{` |
|      - | 4654 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 4655 | `		double realvalue;` |
|      - | 4656 | `		char zFmt[8];` |
|      - | 4657 | `		int nOut, nFmt;` |
|    177 | 4658 | `		pArg = NEXT_ARG;` |
|    177 | 4659 | `		if( pArg == 0 ){` |
|    ! 0 | 4660 | `			realvalue = 0;` |
|    ! 0 | 4661 | `		}else{` |
|    177 | 4662 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 4663 | `		}` |
|      - | 4664 | `		/* php prints the IEEE specials bare — NaN / INF / -INF with no width` |
|      - | 4665 | `		 * padding, precision, or sign flags (php_sprintf_appenddouble). */` |
|    177 | 4666 | `		if( PH7_IS_NAN(realvalue) ){` |
|     21 | 4667 | `			zBuf = "NaN";` |
|     21 | 4668 | `			length = 3;` |
|     21 | 4669 | `			width = 0;` |
|     21 | 4670 | `			break;` |
|      - | 4671 | `		}` |
|    157 | 4672 | `		if( PH7_IS_INF(realvalue) ){` |
|     37 | 4673 | `			if( realvalue < 0.0 ){` |
|     15 | 4674 | `				zBuf = "-INF";` |
|     15 | 4675 | `				length = 4;` |
|      8 | 4676 | `			}else{` |
|     23 | 4677 | `				zBuf = "INF";` |
|     23 | 4678 | `				length = 3;` |
|      - | 4679 | `			}` |
|     37 | 4680 | `			width = 0;` |
|     37 | 4681 | `			break;` |
|      - | 4682 | `		}` |
|    121 | 4683 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|    121 | 4684 | `		if( precision > 53 ){` |
|      - | 4685 | `			/* php's FORMAT_CONV_MAX_PRECISION cap, with the same E_NOTICE` |
|      - | 4686 | `			 * (message prefixed with the active function's name, like` |
|      - | 4687 | `			 * php_error_docref). */` |
|      - | 4688 | `			char zMsg[160];` |
|      4 | 4689 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 4690 | `				"%z(): Requested precision of %d digits was truncated to PHP maximum of %d digits",` |
|      2 | 4691 | `				&pCtx->pFunc->sName,precision,53);` |
|      3 | 4692 | `			PH7_VmThrowError(pCtx->pVm,0,E_NOTICE,zMsg);` |
|      3 | 4693 | `			precision = 53;` |
|      1 | 4694 | `		}` |
|      - | 4695 | ``		/* php's %f/%e extract the sign via `num < 0`, so negative zero prints`` |
|      - | 4696 | `		 * unsigned there — while %g (php_gcvt on the raw value) keeps "-0". */` |
|    121 | 4697 | `		if( xtype!=PH7_FMT_GENERIC && realvalue == 0.0 ){` |
|      9 | 4698 | `			realvalue = 0.0;` |
|      4 | 4699 | `		}` |
|      - | 4700 | `		/* php's float conversions are correctly rounded (zend_dtoa); use libc` |
|      - | 4701 | `		 * snprintf as the digit engine (the byte-exact-floats rule — the old` |
|      - | 4702 | `		 * hand-rolled vxGetdigit loop stopped at 16 significant digits, so` |
|      - | 4703 | `		 * e.g. %f of 1e308 printed zeros where php prints the exact binary64` |
|      - | 4704 | `		 * expansion), then post-process into php's exact shapes below. */` |
|    121 | 4705 | `		nFmt = 0;` |
|    121 | 4706 | `		zFmt[nFmt++] = '%';` |
|    121 | 4707 | `		if( flag_alternateform ) zFmt[nFmt++] = '#';` |
|      - | 4708 | `		/* php's ' ' flag selects space PADDING (its default), not C's` |
|      - | 4709 | `		 * space-for-positive-sign — so flag_blanksign is NOT forwarded. */` |
|    121 | 4710 | `		if( flag_plussign ) zFmt[nFmt++] = '+';` |
|    121 | 4711 | `		zFmt[nFmt++] = '.';` |
|    121 | 4712 | `		zFmt[nFmt++] = '*';` |
|    165 | 4713 | `		zFmt[nFmt++] = (char)(xtype==PH7_FMT_FLOAT ? 'f' :` |
|     30 | 4714 | `			(xtype==PH7_FMT_EXP ? ((pInfo->charset[0]=='E') ? 'E' : 'e')` |
|     28 | 4715 | `			                    : ((pInfo->charset[0]=='E') ? 'G' : 'g')));` |
|    121 | 4716 | `		zFmt[nFmt] = 0;` |
|    121 | 4717 | `		nOut = snprintf(zWorker,sizeof(zWorker),zFmt,precision,realvalue);` |
|    121 | 4718 | `		if( nOut < 0 \|\| nOut >= (int)sizeof(zWorker) ){` |
|      - | 4719 | `			/* Cannot happen with precision capped at 53 (%f of DBL_MAX is` |
|      - | 4720 | `			 * ~365 bytes); keep the truncated output rather than overrun. */` |
|    ! 0 | 4721 | `			nOut = (int)SyStrlen(zWorker);` |
|    ! 0 | 4722 | `		}` |
|    121 | 4723 | `		nOut = (int)PH7_PhpFloatShape(zWorker,(sxi32)nOut,xtype==PH7_FMT_GENERIC);` |
|    121 | 4724 | `		zBuf = zWorker;` |
|    121 | 4725 | `		length = nOut;` |
|      - | 4726 | `		/* Let the zero-pad block below insert zeros between the sign (written` |
|      - | 4727 | `		 * by snprintf) and the first digit, as before. */` |
|    121 | 4728 | `		prefix = (zWorker[0]=='-' \|\| zWorker[0]=='+' \|\| zWorker[0]==' ') ? zWorker[0] : 0;` |
|      - | 4729 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 4730 | `        ** set and we are not left justified */` |
|    121 | 4731 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 4732 | `          int i;` |
|      7 | 4733 | `          int nPad = width - length;` |
|     51 | 4734 | `          for(i=width; i>=nPad; i--){` |
|     45 | 4735 | `            zBuf[i] = zBuf[i-nPad];` |
|     23 | 4736 | `          }` |
|      7 | 4737 | `          i = prefix!=0;` |
|     29 | 4738 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      7 | 4739 | `          length = width;` |
|      3 | 4740 | `        }` |
|      - | 4741 | `#else` |
|      - | 4742 | `         zBuf = " ";` |
|      - | 4743 | `		 length = (int)sizeof(char);` |
|      - | 4744 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|    121 | 4745 | `		 break;` |
|      - | 4746 | `							 }` |
|    ! 0 | 4747 | `		default:` |
|      - | 4748 | `			/* Unreachable: PH7_FormatValidate() rejects unknown specifiers with a` |
|      - | 4749 | `			 * catchable ValueError before formatting begins. Kept as a defensive` |
|      - | 4750 | `			 * no-op that emits nothing. */` |
|    ! 0 | 4751 | `			length = 0;` |
|    ! 0 | 4752 | `			break;` |
|      - | 4753 | `		}` |
|      - | 4754 | `		 /*` |
|      - | 4755 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 4756 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 4757 | `		 ** the output.` |
|      - | 4758 | `		 */` |
|    773 | 4759 | `    if( !flag_leftjustify ){` |
|      - | 4760 | `      register int nspace;` |
|    759 | 4761 | `      nspace = width-length;` |
|    759 | 4762 | `      if( nspace>0 ){` |
|      7 | 4763 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4764 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4765 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4766 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4767 | `			}` |
|    ! 0 | 4768 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4769 | `        }` |
|      7 | 4770 | `        if( nspace>0 ){` |
|      7 | 4771 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      7 | 4772 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4773 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4774 | `			}` |
|      3 | 4775 | `		}` |
|      3 | 4776 | `      }` |
|    379 | 4777 | `    }` |
|    773 | 4778 | `    if( length>0 ){` |
|    773 | 4779 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    773 | 4780 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4781 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4782 | `		}` |
|    386 | 4783 | `    }` |
|    773 | 4784 | `    if( flag_leftjustify ){` |
|      - | 4785 | `      register int nspace;` |
|     15 | 4786 | `      nspace = width-length;` |
|     15 | 4787 | `      if( nspace>0 ){` |
|     11 | 4788 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4789 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4790 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4791 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4792 | `			}` |
|    ! 0 | 4793 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4794 | `        }` |
|     11 | 4795 | `        if( nspace>0 ){` |
|     11 | 4796 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|     11 | 4797 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4798 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4799 | `			}` |
|      5 | 4800 | `		}` |
|      5 | 4801 | `      }` |
|      7 | 4802 | `    }` |
|      1 | 4803 | ` }/* for(;;) */` |
|    395 | 4804 | `	return SXRET_OK;` |
|    198 | 4805 | `}` |
|      - | 4806 | `/*` |
|      - | 4807 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 4808 | ` */` |
|    352 | 4809 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4810 | `{` |
|      - | 4811 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|      - | 4812 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|      - | 4813 | `	 * non-OK rc also stops the format loop. */` |
|    353 | 4814 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|    353 | 4815 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|    353 | 4816 | `	return *pRc;` |
|      1 | 4817 | `}` |
|      - | 4818 | `/*` |
|      - | 4819 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 4820 | ` *  Return a formatted string.` |
|      - | 4821 | ` * Parameters` |
|      - | 4822 | ` *  $format` |
|      - | 4823 | ` *    The format string (see block comment above)` |
|      - | 4824 | ` * Return` |
|      - | 4825 | ` *  A string produced according to the formatting string format.` |
|      - | 4826 | ` */` |
|    184 | 4827 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4828 | `{` |
|      - | 4829 | `	const char *zFormat;` |
|    185 | 4830 | `	sxi32 rc = SXRET_OK;` |
|      - | 4831 | `	int nLen;` |
|    185 | 4832 | `	if( nArg < 1 ){` |
|      - | 4833 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4834 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4835 | `		return PH7_OK;` |
|      - | 4836 | `	}` |
|      - | 4837 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|    185 | 4838 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    185 | 4839 | `	if( rc != PH7_OK ){` |
|    ! 0 | 4840 | `		return rc;` |
|      - | 4841 | `	}` |
|      - | 4842 | `	/* Extract the string format (scalars/null coerce). */` |
|    185 | 4843 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    185 | 4844 | `	if( nLen < 1 ){` |
|      - | 4845 | `		/* Empty string */` |
|    ! 0 | 4846 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4847 | `		return PH7_OK;` |
|      - | 4848 | `	}` |
|      - | 4849 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4850 | `	 * output; propagate the throw status verbatim. */` |
|    185 | 4851 | `	rc = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    185 | 4852 | `	if( rc != PH7_OK ){` |
|     17 | 4853 | `		return rc;` |
|      - | 4854 | `	}` |
|      - | 4855 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|    169 | 4856 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|    169 | 4857 | `	if( rc != SXRET_OK ){` |
|      - | 4858 | `		/* The result append ran out of memory: raise a fatal rather than` |
|      - | 4859 | `		 * returning a silently-truncated string. */` |
|    ! 0 | 4860 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4861 | `	}` |
|    169 | 4862 | `	return PH7_OK;` |
|     93 | 4863 | `}` |
|      - | 4864 | `/*` |
|      - | 4865 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 4866 | ` */` |
|   1130 | 4867 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4868 | `{` |
|   1131 | 4869 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 4870 | `	/* Call the VM output consumer directly */` |
|   1131 | 4871 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 4872 | `	/* Increment counter */` |
|   1131 | 4873 | `	*pCounter += nLen;` |
|   1131 | 4874 | `	return PH7_OK;` |
|      1 | 4875 | `}` |
|      - | 4876 | `/*` |
|      - | 4877 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 4878 | ` *  Output a formatted string.` |
|      - | 4879 | ` * Parameters` |
|      - | 4880 | ` *  $format` |
|      - | 4881 | ` *   See sprintf() for a description of format.` |
|      - | 4882 | ` * Return` |
|      - | 4883 | ` *  The length of the outputted string.` |
|      - | 4884 | ` */` |
|    200 | 4885 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4886 | `{` |
|    201 | 4887 | `	ph7_int64 nCounter = 0;` |
|      - | 4888 | `	const char *zFormat;` |
|      - | 4889 | `	int nLen;` |
|    201 | 4890 | `	if( nArg < 1 ){` |
|      - | 4891 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 4892 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4893 | `		return PH7_OK;` |
|      - | 4894 | `	}` |
|      - | 4895 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|      - | 4896 | `	{` |
|    201 | 4897 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    201 | 4898 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 4899 | `			return rcf;` |
|      - | 4900 | `		}` |
|      - | 4901 | `	}` |
|      - | 4902 | `	/* Extract the string format (scalars/null coerce). */` |
|    201 | 4903 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    201 | 4904 | `	if( nLen < 1 ){` |
|      - | 4905 | `		/* Empty string */` |
|    ! 0 | 4906 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4907 | `		return PH7_OK;` |
|      - | 4908 | `	}` |
|      - | 4909 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4910 | `	 * output; propagate the throw status verbatim. */` |
|      - | 4911 | `	{` |
|    201 | 4912 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    201 | 4913 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 4914 | `			return rcv;` |
|      - | 4915 | `		}` |
|      - | 4916 | `	}` |
|      - | 4917 | `	/* Format the string */` |
|    201 | 4918 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 4919 | `	/* Return the length of the outputted string */` |
|    201 | 4920 | `	ph7_result_int64(pCtx,nCounter);` |
|    201 | 4921 | `	return PH7_OK;` |
|    101 | 4922 | `}` |
|      - | 4923 | `/*` |
|      - | 4924 | ` * int vprintf(string $format,array $args)` |
|      - | 4925 | ` *  Output a formatted string.` |
|      - | 4926 | ` * Parameters` |
|      - | 4927 | ` *  $format` |
|      - | 4928 | ` *   See sprintf() for a description of format.` |
|      - | 4929 | ` * Return` |
|      - | 4930 | ` *  The length of the outputted string.` |
|      - | 4931 | ` */` |
|      4 | 4932 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4933 | `{` |
|      5 | 4934 | `	ph7_int64 nCounter = 0;` |
|      - | 4935 | `	const char *zFormat;` |
|      - | 4936 | `	ph7_hashmap *pMap;` |
|      - | 4937 | `	SySet sArg;` |
|      - | 4938 | `	int nLen,n;` |
|      - | 4939 | `	sxi32 rcFmt;` |
|      5 | 4940 | `	if( nArg < 2 ){` |
|      - | 4941 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 4942 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4943 | `		return PH7_OK;` |
|      - | 4944 | `	}` |
|      - | 4945 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|      5 | 4946 | `	rcFmt = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|      5 | 4947 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4948 | `		return rcFmt;` |
|      - | 4949 | `	}` |
|      5 | 4950 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 4951 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 4952 | `		char zBuf[64];` |
|      4 | 4953 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4954 | `			"vprintf(): Argument #2 ($values) must be of type array, %s given",` |
|      2 | 4955 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 4956 | `	}` |
|      - | 4957 | `	/* Extract the string format (scalars/null coerce). */` |
|      3 | 4958 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4959 | `	if( nLen < 1 ){` |
|      - | 4960 | `		/* Empty string */` |
|    ! 0 | 4961 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4962 | `		return PH7_OK;` |
|      - | 4963 | `	}` |
|      - | 4964 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4965 | `	 * output; propagate the throw status verbatim. */` |
|      3 | 4966 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      3 | 4967 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4968 | `		return rcFmt;` |
|      - | 4969 | `	}` |
|      - | 4970 | `	/* Point to the hashmap */` |
|      3 | 4971 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4972 | `	/* Extract arguments from the hashmap */` |
|      3 | 4973 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4974 | `	/* Format the string */` |
|      3 | 4975 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 4976 | `	/* Release the container */` |
|      3 | 4977 | `	SySetRelease(&sArg);` |
|      - | 4978 | `	/* Return the length of the outputted string */` |
|      3 | 4979 | `	ph7_result_int64(pCtx,nCounter);` |
|      3 | 4980 | `	return PH7_OK;` |
|      3 | 4981 | `}` |
|      - | 4982 | `/*` |
|      - | 4983 | ` * int vsprintf(string $format,array $args)` |
|      - | 4984 | ` *  Output a formatted string.` |
|      - | 4985 | ` * Parameters` |
|      - | 4986 | ` *  $format` |
|      - | 4987 | ` *   See sprintf() for a description of format.` |
|      - | 4988 | ` * Return` |
|      - | 4989 | ` *  A string produced according to the formatting string format.` |
|      - | 4990 | ` */` |
|     18 | 4991 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4992 | `{` |
|      - | 4993 | `	const char *zFormat;` |
|      - | 4994 | `	ph7_hashmap *pMap;` |
|      - | 4995 | `	SySet sArg;` |
|     19 | 4996 | `	sxi32 rc = SXRET_OK;` |
|      - | 4997 | `	sxi32 rcFmt;` |
|      - | 4998 | `	int nLen,n;` |
|     19 | 4999 | `	if( nArg < 2 ){` |
|      - | 5000 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5001 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5002 | `		return PH7_OK;` |
|      - | 5003 | `	}` |
|      - | 5004 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|     19 | 5005 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|     19 | 5006 | `	if( rc != PH7_OK ){` |
|    ! 0 | 5007 | `		return rc;` |
|      - | 5008 | `	}` |
|     19 | 5009 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 5010 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 5011 | `		char zBuf[64];` |
|     16 | 5012 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5013 | `			"vsprintf(): Argument #2 ($values) must be of type array, %s given",` |
|     10 | 5014 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 5015 | `	}` |
|      - | 5016 | `	/* Extract the string format (scalars/null coerce). */` |
|      9 | 5017 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 5018 | `	if( nLen < 1 ){` |
|      - | 5019 | `		/* Empty string */` |
|    ! 0 | 5020 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5021 | `		return PH7_OK;` |
|      - | 5022 | `	}` |
|      - | 5023 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 5024 | `	 * output; propagate the throw status verbatim. */` |
|      9 | 5025 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      9 | 5026 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 5027 | `		return rcFmt;` |
|      - | 5028 | `	}` |
|      - | 5029 | `	/* Point to hashmap */` |
|      9 | 5030 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 5031 | `	/* Extract arguments from the hashmap */` |
|      9 | 5032 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 5033 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|      9 | 5034 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|      - | 5035 | `	/* Release the container */` |
|      9 | 5036 | `	SySetRelease(&sArg);` |
|      9 | 5037 | `	if( rc != SXRET_OK ){` |
|      - | 5038 | `		/* The result append ran out of memory: raise a fatal. */` |
|    ! 0 | 5039 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5040 | `	}` |
|      9 | 5041 | `	return PH7_OK;` |
|     10 | 5042 | `}` |
|      - | 5043 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5044 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5045 | `/*` |
|      - | 5046 | ` * Symisc eXtension.` |
|      - | 5047 | ` * string size_format(int64 $size)` |
|      - | 5048 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 5049 | ` *  Example:` |
|      - | 5050 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 5051 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 5052 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 5053 | ` * Parameter` |
|      - | 5054 | ` *  $size` |
|      - | 5055 | ` *    Entity size in bytes.` |
|      - | 5056 | ` * Return` |
|      - | 5057 | ` *   Formatted string representation of the given size.` |
|      - | 5058 | ` */` |
|     24 | 5059 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5060 | `{` |
|      - | 5061 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 5062 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 5063 | `	sxi32 nRest,i_32;` |
|      - | 5064 | `	ph7_int64 iSize;` |
|     25 | 5065 | `	int c = -1; /* index in zUnit[] */` |
|      - | 5066 |  |
|     25 | 5067 | `	if( nArg < 1 ){` |
|      - | 5068 | `		/* Missing argument,return the empty string */` |
|      3 | 5069 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5070 | `		return PH7_OK;` |
|      - | 5071 | `	}` |
|      - | 5072 | `	/* Extract the given size */` |
|     23 | 5073 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 5074 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 5075 | `		/* Don't bother formatting,return immediately */` |
|      5 | 5076 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 5077 | `		return PH7_OK;` |
|      - | 5078 | `	}` |
|     19 | 5079 | `	for(;;){` |
|     39 | 5080 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 5081 | `		iSize >>= 10;` |
|     39 | 5082 | `		c++;` |
|     39 | 5083 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 5084 | `			break;` |
|      - | 5085 | `		}` |
|      1 | 5086 | `	}` |
|     19 | 5087 | `	nRest /= 100;` |
|     19 | 5088 | `	if( nRest > 9 ){` |
|    ! 0 | 5089 | `		nRest = 9;` |
|    ! 0 | 5090 | `	}` |
|     19 | 5091 | `	if( iSize > 999 ){` |
|    ! 0 | 5092 | `		c++;` |
|    ! 0 | 5093 | `		nRest = 9;` |
|    ! 0 | 5094 | `		iSize = 0;` |
|    ! 0 | 5095 | `	}` |
|     19 | 5096 | `	i_32 = (sxi32)iSize;` |
|      - | 5097 | `	/* Format */` |
|     19 | 5098 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 5099 | `	return PH7_OK;` |
|     13 | 5100 | `}` |
|      - | 5101 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 5102 | `/*` |
|      - | 5103 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 5104 | ` *   Calculate the md5 hash of a string.` |
|      - | 5105 | ` * Parameter` |
|      - | 5106 | ` *  $str` |
|      - | 5107 | ` *   Input string` |
|      - | 5108 | ` * $raw_output` |
|      - | 5109 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 5110 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 5111 | ` * Return` |
|      - | 5112 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 5113 | ` */` |
|     12 | 5114 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5115 | `{` |
|      - | 5116 | `	unsigned char zDigest[16];` |
|     13 | 5117 | `	int raw_output = FALSE;` |
|      - | 5118 | `	const void *pIn;` |
|      - | 5119 | `	int nLen;` |
|     13 | 5120 | `	if( nArg < 1 ){` |
|      - | 5121 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5122 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5123 | `		return PH7_OK;` |
|      - | 5124 | `	}` |
|      - | 5125 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 5126 | `	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */` |
|     13 | 5127 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 5128 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 5129 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 5130 | `	}` |
|      - | 5131 | `	/* Compute the MD5 digest */` |
|     13 | 5132 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|     13 | 5133 | `	if( raw_output ){` |
|      - | 5134 | `		/* Output raw digest */` |
|      5 | 5135 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 5136 | `	}else{` |
|      - | 5137 | `		/* Perform a binary to hex conversion */` |
|      9 | 5138 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 5139 | `	}` |
|     13 | 5140 | `	return PH7_OK;` |
|      7 | 5141 | `}` |
|      - | 5142 | `/*` |
|      - | 5143 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 5144 | ` *   Calculate the sha1 hash of a string.` |
|      - | 5145 | ` * Parameter` |
|      - | 5146 | ` *  $str` |
|      - | 5147 | ` *   Input string` |
|      - | 5148 | ` * $raw_output` |
|      - | 5149 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 5150 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 5151 | ` * Return` |
|      - | 5152 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 5153 | ` */` |
|     10 | 5154 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5155 | `{` |
|      - | 5156 | `	unsigned char zDigest[20];` |
|     11 | 5157 | `	int raw_output = FALSE;` |
|      - | 5158 | `	const void *pIn;` |
|      - | 5159 | `	int nLen;` |
|     11 | 5160 | `	if( nArg < 1 ){` |
|      - | 5161 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5162 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5163 | `		return PH7_OK;` |
|      - | 5164 | `	}` |
|      - | 5165 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 5166 | `	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */` |
|     11 | 5167 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5168 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 5169 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 5170 | `	}` |
|      - | 5171 | `	/* Compute the SHA1 digest */` |
|     11 | 5172 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|     11 | 5173 | `	if( raw_output ){` |
|      - | 5174 | `		/* Output raw digest */` |
|      5 | 5175 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 5176 | `	}else{` |
|      - | 5177 | `		/* Perform a binary to hex conversion */` |
|      7 | 5178 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 5179 | `	}` |
|     11 | 5180 | `	return PH7_OK;` |
|      6 | 5181 | `}` |
|      - | 5182 | `/*` |
|      - | 5183 | ` * int64 crc32(string $str)` |
|      - | 5184 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 5185 | ` * Parameter` |
|      - | 5186 | ` *  $str` |
|      - | 5187 | ` *   Input string` |
|      - | 5188 | ` * Return` |
|      - | 5189 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 5190 | ` */` |
|      2 | 5191 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5192 | `{` |
|      - | 5193 | `	const void *pIn;` |
|      - | 5194 | `	sxu32 nCRC;` |
|      - | 5195 | `	int nLen;` |
|      3 | 5196 | `	if( nArg < 1 ){` |
|      - | 5197 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 5198 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5199 | `		return PH7_OK;` |
|      - | 5200 | `	}` |
|      - | 5201 | `	/* Extract the input string */` |
|      3 | 5202 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5203 | `	if( nLen < 1 ){` |
|      - | 5204 | `		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike` |
|      - | 5205 | `		 * md5()/sha1(), whose empty-string digests are non-zero. */` |
|    ! 0 | 5206 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5207 | `		return PH7_OK;` |
|      - | 5208 | `	}` |
|      - | 5209 | `	/* Calculate the sum */` |
|      3 | 5210 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 5211 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 5212 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 5213 | `	return PH7_OK;` |
|      2 | 5214 | `}` |
|      - | 5215 | `/*` |
|      - | 5216 | ` * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is` |
|      - | 5217 | ` * described by a small record so one dispatch (and one generic HMAC) serves them` |
|      - | 5218 | ` * all. Thin adapters normalize the differing context types and the reversed` |
|      - | 5219 | ` * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.` |
|      - | 5220 | ` */` |
|     11 | 5221 | `static void HashMd5Init(HashCtx *c){ MD5Init(&c->md5); }` |
|     15 | 5222 | `static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }` |
|     11 | 5223 | `static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }` |
|     11 | 5224 | `static void HashSha1Init(HashCtx *c){ SHA1Init(&c->sha1); }` |
|     15 | 5225 | `static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }` |
|     11 | 5226 | `static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }` |
|      9 | 5227 | `static void HashSha224Init(HashCtx *c){ SHA224Init(&c->sha256); }` |
|     33 | 5228 | `static void HashSha256Init(HashCtx *c){ SHA256Init(&c->sha256); }` |
|     57 | 5229 | `static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }` |
|     41 | 5230 | `static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }` |
|      9 | 5231 | `static void HashSha384Init(HashCtx *c){ SHA384Init(&c->sha512); }` |
|     15 | 5232 | `static void HashSha512Init(HashCtx *c){ SHA512Init(&c->sha512); }` |
|     27 | 5233 | `static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }` |
|     23 | 5234 | `static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }` |
|      - | 5235 | `typedef struct HashAlgo HashAlgo;` |
|      - | 5236 | `struct HashAlgo {` |
|      - | 5237 | `	const char *zName;   /* lowercase canonical name */` |
|      - | 5238 | `	int nDigestLen;      /* output bytes: 16/20/28/32/48/64 */` |
|      - | 5239 | `	int nBlockLen;       /* internal block bytes (for HMAC): 64 or 128 */` |
|      - | 5240 | `	void (*xInit)(HashCtx *);` |
|      - | 5241 | `	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);` |
|      - | 5242 | `	void (*xFinal)(HashCtx *,unsigned char *);` |
|      - | 5243 | `};` |
|      - | 5244 | `static const HashAlgo aHashAlgo[] = {` |
|      - | 5245 | `	{ "md5",    16, 64,  HashMd5Init,    HashMd5Update,    HashMd5Final    },` |
|      - | 5246 | `	{ "sha1",   20, 64,  HashSha1Init,   HashSha1Update,   HashSha1Final   },` |
|      - | 5247 | `	{ "sha224", 28, 64,  HashSha224Init, HashSha256Update, HashSha256Final },` |
|      - | 5248 | `	{ "sha256", 32, 64,  HashSha256Init, HashSha256Update, HashSha256Final },` |
|      - | 5249 | `	{ "sha384", 48, 128, HashSha384Init, HashSha512Update, HashSha512Final },` |
|      - | 5250 | `	{ "sha512", 64, 128, HashSha512Init, HashSha512Update, HashSha512Final },` |
|      - | 5251 | `};` |
|      - | 5252 | `/* Case-insensitive algorithm lookup (PHP accepts 'SHA256' etc.). */` |
|     73 | 5253 | `static const HashAlgo * HashFindAlgo(const char *zName,int nLen){` |
|      - | 5254 | `	sxu32 i;` |
|    279 | 5255 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|    272 | 5256 | `		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen` |
|    211 | 5257 | `			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){` |
|     67 | 5258 | `			return &aHashAlgo[i];` |
|      - | 5259 | `		}` |
|    106 | 5260 | `	}` |
|      6 | 5261 | `	return 0;` |
|     38 | 5262 | `}` |
|      - | 5263 | `/*` |
|      - | 5264 | ` * string hash(string $algo,string $data[,bool $binary = false])` |
|      - | 5265 | ` *   Generate a hash value (message digest).` |
|      - | 5266 | ` */` |
|     54 | 5267 | `static int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5268 | `{` |
|      - | 5269 | `	const HashAlgo *pAlgo;` |
|      - | 5270 | `	const char *zAlgo,*zData;` |
|     56 | 5271 | `	int nAlgoLen,nDataLen,raw_output = FALSE;` |
|      - | 5272 | `	HashCtx sCtx;` |
|      - | 5273 | `	unsigned char zDigest[64];` |
|     56 | 5274 | `	if( nArg < 2 ){` |
|    ! 0 | 5275 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5276 | `			"hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5277 | `	}` |
|     56 | 5278 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     56 | 5279 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     56 | 5280 | `	if( pAlgo == 0 ){` |
|      3 | 5281 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5282 | `			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - | 5283 | `	}` |
|     53 | 5284 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     53 | 5285 | `	if( nArg > 2 ){` |
|      9 | 5286 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|      4 | 5287 | `	}` |
|     53 | 5288 | `	pAlgo->xInit(&sCtx);` |
|     53 | 5289 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     53 | 5290 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     53 | 5291 | `	if( raw_output ){` |
|      9 | 5292 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|      5 | 5293 | `	}else{` |
|     45 | 5294 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - | 5295 | `	}` |
|     53 | 5296 | `	return PH7_OK;` |
|     29 | 5297 | `}` |
|      - | 5298 | `/*` |
|      - | 5299 | ` * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])` |
|      - | 5300 | ` *   Generate a keyed hash value using the HMAC method (RFC 2104).` |
|      - | 5301 | ` */` |
|     16 | 5302 | `static int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5303 | `{` |
|      - | 5304 | `	const HashAlgo *pAlgo;` |
|      - | 5305 | `	const char *zAlgo,*zData,*zKey;` |
|     18 | 5306 | `	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;` |
|      - | 5307 | `	HashCtx sCtx;` |
|      - | 5308 | `	unsigned char zKeyBlock[128],zIpad[128],zOpad[128],zInner[64],zDigest[64];` |
|      - | 5309 | `	int i,nBlock,nDigest;` |
|     18 | 5310 | `	if( nArg < 3 ){` |
|    ! 0 | 5311 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5312 | `			"hash_hmac() expects at least 3 arguments, %d given",nArg);` |
|      - | 5313 | `	}` |
|     18 | 5314 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     18 | 5315 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     18 | 5316 | `	if( pAlgo == 0 ){` |
|      3 | 5317 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5318 | `			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 5319 | `	}` |
|     15 | 5320 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     15 | 5321 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|     15 | 5322 | `	if( nArg > 3 ){` |
|      3 | 5323 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|      1 | 5324 | `	}` |
|     15 | 5325 | `	nBlock = pAlgo->nBlockLen;` |
|     15 | 5326 | `	nDigest = pAlgo->nDigestLen;` |
|      - | 5327 | `	/* Reduce the key to a single block: hash it if longer than the block, then` |
|      - | 5328 | `	 * zero-pad (a short or empty key is just zero-padded). */` |
|     15 | 5329 | `	SyZero(zKeyBlock,sizeof(zKeyBlock));` |
|     15 | 5330 | `	if( nKeyLen > nBlock ){` |
|      3 | 5331 | `		pAlgo->xInit(&sCtx);` |
|      3 | 5332 | `		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);` |
|      3 | 5333 | `		pAlgo->xFinal(&sCtx,zKeyBlock);` |
|     14 | 5334 | `	}else if( nKeyLen > 0 ){` |
|     11 | 5335 | `		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);` |
|      5 | 5336 | `	}` |
|   1039 | 5337 | `	for( i = 0; i < nBlock; i++ ){` |
|   1025 | 5338 | `		zIpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);` |
|   1025 | 5339 | `		zOpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);` |
|    513 | 5340 | `	}` |
|      - | 5341 | `	/* inner = H((key ^ ipad) \|\| data) */` |
|     15 | 5342 | `	pAlgo->xInit(&sCtx);` |
|     15 | 5343 | `	pAlgo->xUpdate(&sCtx,zIpad,(unsigned int)nBlock);` |
|     15 | 5344 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     15 | 5345 | `	pAlgo->xFinal(&sCtx,zInner);` |
|      - | 5346 | `	/* out = H((key ^ opad) \|\| inner) */` |
|     15 | 5347 | `	pAlgo->xInit(&sCtx);` |
|     15 | 5348 | `	pAlgo->xUpdate(&sCtx,zOpad,(unsigned int)nBlock);` |
|     15 | 5349 | `	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)nDigest);` |
|     15 | 5350 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     15 | 5351 | `	if( raw_output ){` |
|      3 | 5352 | `		ph7_result_string(pCtx,(const char *)zDigest,nDigest);` |
|      2 | 5353 | `	}else{` |
|     13 | 5354 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);` |
|      - | 5355 | `	}` |
|     15 | 5356 | `	return PH7_OK;` |
|     10 | 5357 | `}` |
|      - | 5358 | `/*` |
|      - | 5359 | ` * bool hash_equals(string $known_string,string $user_string)` |
|      - | 5360 | ` *   Timing-attack-safe string comparison.` |
|      - | 5361 | ` */` |
|     12 | 5362 | `static int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5363 | `{` |
|      - | 5364 | `	const char *zKnown,*zUser;` |
|      - | 5365 | `	int nKnown,nUser,i;` |
|     14 | 5366 | `	volatile unsigned char vDiff = 0;` |
|     14 | 5367 | `	if( nArg < 2 ){` |
|    ! 0 | 5368 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5369 | `			"hash_equals() expects exactly 2 arguments, %d given",nArg);` |
|      - | 5370 | `	}` |
|     14 | 5371 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      4 | 5372 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5373 | `			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",` |
|      1 | 5374 | `			ph7_type_name(apArg[0]));` |
|      - | 5375 | `	}` |
|     11 | 5376 | `	if( !ph7_value_is_string(apArg[1]) ){` |
|    ! 0 | 5377 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5378 | `			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",` |
|    ! 0 | 5379 | `			ph7_type_name(apArg[1]));` |
|      - | 5380 | `	}` |
|     11 | 5381 | `	zKnown = ph7_value_to_string(apArg[0],&nKnown);` |
|     11 | 5382 | `	zUser = ph7_value_to_string(apArg[1],&nUser);` |
|     11 | 5383 | `	if( nKnown != nUser ){` |
|      5 | 5384 | `		ph7_result_bool(pCtx,0);` |
|      5 | 5385 | `		return PH7_OK;` |
|      - | 5386 | `	}` |
|      - | 5387 | `	/* Constant-time: read every byte, never short-circuit. */` |
|     19 | 5388 | `	for( i = 0; i < nKnown; i++ ){` |
|     13 | 5389 | `		vDiff \|= (unsigned char)(zKnown[i] ^ zUser[i]);` |
|      7 | 5390 | `	}` |
|      7 | 5391 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|      7 | 5392 | `	return PH7_OK;` |
|      8 | 5393 | `}` |
|      - | 5394 | `/*` |
|      - | 5395 | ` * array hash_algos(void)` |
|      - | 5396 | ` *   Return a list of the registered hashing algorithms.` |
|      - | 5397 | ` */` |
|      2 | 5398 | `static int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5399 | `{` |
|      - | 5400 | `	ph7_value *pArray,*pValue;` |
|      - | 5401 | `	sxu32 i;` |
|      1 | 5402 | `	SXUNUSED(nArg);` |
|      1 | 5403 | `	SXUNUSED(apArg);` |
|      3 | 5404 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5405 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 5406 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 5407 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5408 | `		return PH7_OK;` |
|      - | 5409 | `	}` |
|     15 | 5410 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|     13 | 5411 | `		ph7_value_string(pValue,aHashAlgo[i].zName,-1);` |
|     13 | 5412 | `		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);` |
|     13 | 5413 | `		ph7_value_reset_string_cursor(pValue);` |
|      7 | 5414 | `	}` |
|      3 | 5415 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5416 | `	return PH7_OK;` |
|      2 | 5417 | `}` |
|      - | 5418 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 5419 | `/*` |
|      - | 5420 | ` * password_* (bcrypt). These live in ext/standard in real PHP — outside the` |
|      - | 5421 | ` * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.` |
|      - | 5422 | ` */` |
|      - | 5423 | `/*` |
|      - | 5424 | ` * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a` |
|      - | 5425 | ` * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).` |
|      - | 5426 | ` */` |
|     40 | 5427 | `static int BcryptParseHash(const char *zHash,int nHash,int *piCost)` |
|      1 | 5428 | `{` |
|      - | 5429 | `	int iCost;` |
|     40 | 5430 | `	if( nHash != 60 \|\| zHash[0] != '$' \|\| zHash[1] != '2' \|\| zHash[3] != '$'` |
|     29 | 5431 | `		\|\| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){` |
|     13 | 5432 | `		return FALSE;` |
|      - | 5433 | `	}` |
|     29 | 5434 | `	if( zHash[4] < '0' \|\| zHash[4] > '9' \|\| zHash[5] < '0' \|\| zHash[5] > '9' \|\| zHash[6] != '$' ){` |
|    ! 0 | 5435 | `		return FALSE;` |
|      - | 5436 | `	}` |
|     29 | 5437 | `	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');` |
|     29 | 5438 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      3 | 5439 | `		return FALSE;` |
|      - | 5440 | `	}` |
|     27 | 5441 | `	if( piCost ){ *piCost = iCost; }` |
|     27 | 5442 | `	return TRUE;` |
|     21 | 5443 | `}` |
|      - | 5444 | `/*` |
|      - | 5445 | ` * TRUE if the $algo argument selects bcrypt: null (PASSWORD_DEFAULT) or the` |
|      - | 5446 | ` * "2y" id (PASSWORD_BCRYPT/PASSWORD_DEFAULT). bcrypt is the only supported algo.` |
|      - | 5447 | ` */` |
|     20 | 5448 | `static int BcryptIsBcryptAlgo(ph7_value *pAlgo)` |
|      3 | 5449 | `{` |
|     23 | 5450 | `	if( ph7_value_is_null(pAlgo) ){` |
|    ! 0 | 5451 | `		return TRUE;` |
|      - | 5452 | `	}` |
|     23 | 5453 | `	if( ph7_value_is_string(pAlgo) ){` |
|      - | 5454 | `		int nAlgo;` |
|     23 | 5455 | `		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);` |
|     23 | 5456 | `		return ( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' );` |
|      - | 5457 | `	}` |
|    ! 0 | 5458 | `	return FALSE;` |
|     13 | 5459 | `}` |
|      - | 5460 | `/*` |
|      - | 5461 | ` * bool\|string password_hash(string $password,string\|int\|null $algo[,array $options])` |
|      - | 5462 | ` *  Create a bcrypt hash of the password.` |
|      - | 5463 | ` */` |
|     16 | 5464 | `static int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 5465 | `{` |
|      - | 5466 | `	const char *zPwd;` |
|     19 | 5467 | `	int nPwd,iCost = 12;` |
|      - | 5468 | `	unsigned char aSalt[16];` |
|      - | 5469 | `	char zHash[60];` |
|     19 | 5470 | `	if( nArg < 2 ){` |
|    ! 0 | 5471 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5472 | `			"password_hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5473 | `	}` |
|     19 | 5474 | `	if( !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      3 | 5475 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5476 | `			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");` |
|      - | 5477 | `	}` |
|      - | 5478 | `	/* cost from $options['cost'] (default 12). */` |
|     16 | 5479 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|     14 | 5480 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|     14 | 5481 | `		if( pCost ){ iCost = ph7_value_to_int(pCost); }` |
|      6 | 5482 | `	}` |
|     16 | 5483 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      4 | 5484 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 5485 | `			"Invalid bcrypt cost parameter specified: %d",iCost);` |
|      - | 5486 | `	}` |
|     13 | 5487 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     13 | 5488 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 5489 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - | 5490 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|      - | 5491 | `	}` |
|     13 | 5492 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){` |
|    ! 0 | 5493 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5494 | `		return PH7_OK;` |
|      - | 5495 | `	}` |
|     13 | 5496 | `	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));` |
|     13 | 5497 | `	return PH7_OK;` |
|     11 | 5498 | `}` |
|      - | 5499 | `/*` |
|      - | 5500 | ` * bool password_verify(string $password,string $hash)` |
|      - | 5501 | ` *  Verify a password against a bcrypt hash. Never throws on a malformed hash.` |
|      - | 5502 | ` */` |
|     28 | 5503 | `static int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5504 | `{` |
|      - | 5505 | `	const char *zPwd,*zHash;` |
|      - | 5506 | `	int nPwd,nHash,iCost,i;` |
|      - | 5507 | `	unsigned char aSalt[16];` |
|      - | 5508 | `	char zComputed[60];` |
|     29 | 5509 | `	volatile unsigned char vDiff = 0;` |
|     29 | 5510 | `	if( nArg < 2 ){` |
|    ! 0 | 5511 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5512 | `			"password_verify() expects exactly 2 arguments, %d given",nArg);` |
|      - | 5513 | `	}` |
|     29 | 5514 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     29 | 5515 | `	zHash = ph7_value_to_string(apArg[1],&nHash);` |
|     29 | 5516 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|     11 | 5517 | `		ph7_result_bool(pCtx,0);` |
|     11 | 5518 | `		return PH7_OK;` |
|      - | 5519 | `	}` |
|      - | 5520 | `	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */` |
|     19 | 5521 | `	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 5522 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5523 | `		return PH7_OK;` |
|      - | 5524 | `	}` |
|     19 | 5525 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){` |
|    ! 0 | 5526 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5527 | `		return PH7_OK;` |
|      - | 5528 | `	}` |
|      - | 5529 | `	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps` |
|      - | 5530 | `	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */` |
|    577 | 5531 | `	for( i = 29; i < 60; i++ ){` |
|    559 | 5532 | `		vDiff \|= (unsigned char)(zComputed[i] ^ zHash[i]);` |
|    280 | 5533 | `	}` |
|     19 | 5534 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|     19 | 5535 | `	return PH7_OK;` |
|     15 | 5536 | `}` |
|      - | 5537 | `/*` |
|      - | 5538 | ` * array password_get_info(string $hash)` |
|      - | 5539 | ` *  Return ["algo"=>id\|null, "algoName"=>name, "options"=>[...]].` |
|      - | 5540 | ` */` |
|      6 | 5541 | `static int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5542 | `{` |
|      7 | 5543 | `	const char *zHash = "";` |
|      7 | 5544 | `	int nHash,iCost = 0,bBcrypt = 0;` |
|      - | 5545 | `	ph7_value *pArray,*pOptions,*pVal;` |
|      7 | 5546 | `	if( nArg > 0 ){` |
|      7 | 5547 | `		zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 5548 | `		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);` |
|      3 | 5549 | `	}` |
|      7 | 5550 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 5551 | `	pOptions = ph7_context_new_array(pCtx);` |
|      7 | 5552 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      7 | 5553 | `	if( pArray == 0 \|\| pOptions == 0 \|\| pVal == 0 ){` |
|    ! 0 | 5554 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5555 | `		return PH7_OK;` |
|      - | 5556 | `	}` |
|      7 | 5557 | `	if( bBcrypt ){` |
|      5 | 5558 | `		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */` |
|      5 | 5559 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 5560 | `		ph7_value_reset_string_cursor(pVal);` |
|      5 | 5561 | `		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);` |
|      5 | 5562 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      5 | 5563 | `		ph7_value_int(pVal,iCost);` |
|      5 | 5564 | `		ph7_array_add_strkey_elem(pOptions,"cost",pVal);` |
|      3 | 5565 | `	}else{` |
|      3 | 5566 | `		ph7_value_null(pVal);                          /* algo => null */` |
|      3 | 5567 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      3 | 5568 | `		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);` |
|      3 | 5569 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      - | 5570 | `	}` |
|      7 | 5571 | `	ph7_array_add_strkey_elem(pArray,"options",pOptions);` |
|      7 | 5572 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 5573 | `	return PH7_OK;` |
|      4 | 5574 | `}` |
|      - | 5575 | `/*` |
|      - | 5576 | ` * bool password_needs_rehash(string $hash,string\|int\|null $algo[,array $options])` |
|      - | 5577 | ` *  True if the hash was not made with the given algo/options.` |
|      - | 5578 | ` */` |
|      6 | 5579 | `static int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5580 | `{` |
|      - | 5581 | `	const char *zHash;` |
|      7 | 5582 | `	int nHash,iCost = 0,iWantCost = 12;` |
|      7 | 5583 | `	if( nArg < 2 ){` |
|    ! 0 | 5584 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5585 | `			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5586 | `	}` |
|      7 | 5587 | `	zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 5588 | `	if( !BcryptParseHash(zHash,nHash,&iCost) \|\| !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      - | 5589 | `		/* A non-bcrypt hash, or a request for a different algo → needs rehash. */` |
|      3 | 5590 | `		ph7_result_bool(pCtx,1);` |
|      3 | 5591 | `		return PH7_OK;` |
|      - | 5592 | `	}` |
|      5 | 5593 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|      5 | 5594 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|      5 | 5595 | `		if( pCost ){ iWantCost = ph7_value_to_int(pCost); }` |
|      2 | 5596 | `	}` |
|      5 | 5597 | `	ph7_result_bool(pCtx,iCost != iWantCost);` |
|      5 | 5598 | `	return PH7_OK;` |
|      4 | 5599 | `}` |
|      - | 5600 | `/*` |
|      - | 5601 | ` * filter_var() — input validation and sanitization (the ext/filter API).` |
|      - | 5602 | ` *` |
|      - | 5603 | ` * Filter and flag identifiers (values match PHP 8.5; the constants themselves` |
|      - | 5604 | ` * are registered in constant.c). The validate filters are hand-rolled rather` |
|      - | 5605 | ` * than delegating to SyStrToInt64/SyStrToReal: the former silently skips leading` |
|      - | 5606 | ` * zeros and cannot signal overflow, and the latter treats ',' as a decimal point` |
|      - | 5607 | ` * unconditionally — neither matches PHP's filter semantics.` |
|      - | 5608 | ` */` |
|      - | 5609 | `#define FV_VALIDATE_INT     257` |
|      - | 5610 | `#define FV_VALIDATE_BOOLEAN 258` |
|      - | 5611 | `#define FV_VALIDATE_FLOAT   259` |
|      - | 5612 | `#define FV_VALIDATE_REGEXP  272` |
|      - | 5613 | `#define FV_VALIDATE_URL     273` |
|      - | 5614 | `#define FV_VALIDATE_EMAIL   274` |
|      - | 5615 | `#define FV_VALIDATE_IP      275` |
|      - | 5616 | `#define FV_VALIDATE_MAC     276` |
|      - | 5617 | `#define FV_VALIDATE_DOMAIN  277` |
|      - | 5618 | `#define FV_SANITIZE_SPECIAL_CHARS      515` |
|      - | 5619 | `#define FV_DEFAULT          516 /* == FILTER_UNSAFE_RAW: pass the value through */` |
|      - | 5620 | `#define FV_SANITIZE_EMAIL   517` |
|      - | 5621 | `#define FV_SANITIZE_URL     518` |
|      - | 5622 | `#define FV_SANITIZE_NUMBER_INT   519` |
|      - | 5623 | `#define FV_SANITIZE_NUMBER_FLOAT 520` |
|      - | 5624 | `#define FV_SANITIZE_FULL_SPECIAL_CHARS 522` |
|      - | 5625 | `#define FV_FLAG_ALLOW_OCTAL  1` |
|      - | 5626 | `#define FV_FLAG_ALLOW_HEX    2` |
|      - | 5627 | `#define FV_FLAG_STRIP_LOW    4` |
|      - | 5628 | `#define FV_FLAG_STRIP_HIGH   8` |
|      - | 5629 | `#define FV_FLAG_ENCODE_LOW   16` |
|      - | 5630 | `#define FV_FLAG_ENCODE_HIGH  32` |
|      - | 5631 | `#define FV_FLAG_ENCODE_AMP   64` |
|      - | 5632 | `#define FV_FLAG_NO_ENCODE_QUOTES 128` |
|      - | 5633 | `#define FV_FLAG_STRIP_BACKTICK   512` |
|      - | 5634 | `#define FV_FLAG_ALLOW_FRACTION   4096` |
|      - | 5635 | `#define FV_FLAG_ALLOW_THOUSAND   8192` |
|      - | 5636 | `#define FV_FLAG_ALLOW_SCIENTIFIC 16384` |
|      - | 5637 | `#define FV_FLAG_IPV4  1048576` |
|      - | 5638 | `#define FV_FLAG_IPV6  2097152` |
|      - | 5639 | `#define FV_NULL_ON_FAILURE 134217728` |
|      - | 5640 | `/* The subset of flags the UNSAFE_RAW/DEFAULT string filter (FvSanitizeString)` |
|      - | 5641 | ` * acts on: when none are set the filter is a verbatim pass-through, so FV_DEFAULT` |
|      - | 5642 | ` * can shortcut. Keep this in sync with FvSanitizeString's flag handling. */` |
|      - | 5643 | `#define FV_FLAG_STRING_MASK (FV_FLAG_STRIP_LOW\|FV_FLAG_STRIP_HIGH\|FV_FLAG_STRIP_BACKTICK \` |
|      - | 5644 | `                            \|FV_FLAG_ENCODE_LOW\|FV_FLAG_ENCODE_HIGH\|FV_FLAG_ENCODE_AMP)` |
|      - | 5645 |  |
|      - | 5646 | `/* Trim leading/trailing PHP whitespace, adjusting the (*pz,*pn) view in place.` |
|      - | 5647 | ` * SyisSpace (isspace) matches PHP's filter whitespace set " \t\n\r\v\f". */` |
|    153 | 5648 | `static void FvTrim(const char **pz,int *pn){` |
|    153 | 5649 | `	const char *z = *pz;` |
|    153 | 5650 | `	int n = *pn;` |
|    157 | 5651 | `	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }` |
|    161 | 5652 | `	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }` |
|    153 | 5653 | `	*pz = z; *pn = n;` |
|    153 | 5654 | `}` |
|      - | 5655 | `/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     57 | 5656 | `static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){` |
|     57 | 5657 | `	int neg = 0, i;` |
|     57 | 5658 | `	sxu64 u = 0;` |
|     57 | 5659 | `	FvTrim(&z,&n);` |
|     57 | 5660 | `	if( n==0 ){ return 0; }` |
|     51 | 5661 | `	if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }` |
|     51 | 5662 | `	if( n==0 ){ return 0; }` |
|     49 | 5663 | `	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'\|\|z[1]=='X') ){` |
|      3 | 5664 | `		z += 2; n -= 2;` |
|      3 | 5665 | `		if( n==0 ){ return 0; }` |
|      7 | 5666 | `		for( i=0; i<n; i++ ){` |
|      5 | 5667 | `			int h = SyHexToint((unsigned char)z[i]);` |
|      5 | 5668 | `			if( h<0 ){ return 0; }` |
|      5 | 5669 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }` |
|      5 | 5670 | `			u = u*16 + (sxu64)h;` |
|      3 | 5671 | `		}` |
|     48 | 5672 | `	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){` |
|      9 | 5673 | `		for( i=0; i<n; i++ ){` |
|      7 | 5674 | `			if( z[i]<'0' \|\| z[i]>'7' ){ return 0; }` |
|      7 | 5675 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }` |
|      7 | 5676 | `			u = u*8 + (sxu64)(z[i]-'0');` |
|      4 | 5677 | `		}` |
|      2 | 5678 | `	}else{` |
|     45 | 5679 | `		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */` |
|    201 | 5680 | `		for( i=0; i<n; i++ ){` |
|    173 | 5681 | `			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|    161 | 5682 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }` |
|    161 | 5683 | `			u = u*10 + (sxu64)(z[i]-'0');` |
|     81 | 5684 | `		}` |
|      - | 5685 | `	}` |
|     33 | 5686 | `	if( neg ){` |
|      5 | 5687 | `		if( u > 0x8000000000000000ULL ){ return 0; }` |
|      5 | 5688 | `		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */` |
|      3 | 5689 | `	}else{` |
|     29 | 5690 | `		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }` |
|     27 | 5691 | `		*pOut = (ph7_int64)u;` |
|      - | 5692 | `	}` |
|     31 | 5693 | `	return 1;` |
|     29 | 5694 | `}` |
|      - | 5695 | `/* FILTER_VALIDATE_FLOAT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     69 | 5696 | `static int FvValidateFloat(const char *z,int n,int flags,double *pOut){` |
|      - | 5697 | `	char zBuf[512];` |
|     69 | 5698 | `	int i, m = 0, seenDigit = 0;` |
|     69 | 5699 | `	const char *zv; int nv; double d = 0;` |
|     69 | 5700 | `	FvTrim(&z,&n);` |
|      - | 5701 | `	/* Bound the input: zBuf[512] holds the thousand-separator-stripped copy, and` |
|      - | 5702 | `	 * the cap also rejects the pathological 500+ digit floats PHP refuses. */` |
|     69 | 5703 | `	if( n==0 \|\| n>500 ){ return 0; }` |
|     69 | 5704 | `	if( flags & FV_FLAG_ALLOW_THOUSAND ){` |
|      - | 5705 | `		/* Commas are optional, but when present they must group the integer part` |
|      - | 5706 | `		 * into a leading run of 1..3 digits followed by groups of exactly 3` |
|      - | 5707 | `		 * ("1,000" ok, "1,5"/"1234,567" rejected). Strip them into zBuf and reject` |
|      - | 5708 | `		 * a comma anywhere in the fractional/exponent tail. */` |
|     25 | 5709 | `		int s = 0, intEnd, segStart, segIdx, hasComma = 0;` |
|     25 | 5710 | `		if( s<n && (z[s]=='+'\|\|z[s]=='-') ){ zBuf[m++] = z[s]; s++; }` |
|     25 | 5711 | `		intEnd = s;` |
|    167 | 5712 | `		while( intEnd<n && z[intEnd]!='.' && z[intEnd]!='e' && z[intEnd]!='E' ){` |
|    143 | 5713 | `			if( z[intEnd]==',' ){ hasComma = 1; }` |
|    143 | 5714 | `			intEnd++;` |
|      1 | 5715 | `		}` |
|     25 | 5716 | `		if( hasComma ){` |
|     25 | 5717 | `			segStart = s; segIdx = 0;` |
|    165 | 5718 | `			for( i=s; i<=intEnd; i++ ){` |
|    151 | 5719 | `				if( i==intEnd \|\| z[i]==',' ){` |
|     49 | 5720 | `					int segLen = i - segStart, k;` |
|     49 | 5721 | `					if( segIdx==0 ){ if( segLen<1 \|\| segLen>3 ){ return 0; } }` |
|     25 | 5722 | `					else if( segLen!=3 ){ return 0; }` |
|    119 | 5723 | `					for( k=segStart; k<i; k++ ){` |
|     81 | 5724 | `						if( !SyisDigit((unsigned char)z[k]) ){ return 0; }` |
|     81 | 5725 | `						zBuf[m++] = z[k];` |
|     41 | 5726 | `					}` |
|     39 | 5727 | `					segStart = i+1; segIdx++;` |
|     19 | 5728 | `				}` |
|     71 | 5729 | `			}` |
|      8 | 5730 | `		}else{` |
|    ! 0 | 5731 | `			for( i=s; i<intEnd; i++ ){ zBuf[m++] = z[i]; }` |
|      - | 5732 | `		}` |
|     27 | 5733 | `		for( i=intEnd; i<n; i++ ){` |
|     13 | 5734 | `			if( z[i]==',' ){ return 0; }` |
|     13 | 5735 | `			zBuf[m++] = z[i];` |
|      7 | 5736 | `		}` |
|     15 | 5737 | `		zv = zBuf; nv = m;` |
|      8 | 5738 | `	}else{` |
|     45 | 5739 | `		zv = z; nv = n;` |
|      - | 5740 | `	}` |
|     59 | 5741 | `	i = 0;` |
|     59 | 5742 | `	if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|    167 | 5743 | `	while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     59 | 5744 | `	if( i<nv && zv[i]=='.' ){` |
|     21 | 5745 | `		i++;` |
|     39 | 5746 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     10 | 5747 | `	}` |
|     59 | 5748 | `	if( !seenDigit ){ return 0; }` |
|     57 | 5749 | `	if( i<nv && (zv[i]=='e'\|\|zv[i]=='E') ){` |
|     29 | 5750 | `		i++;` |
|     29 | 5751 | `		if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|     29 | 5752 | `		if( i>=nv \|\| !SyisDigit((unsigned char)zv[i]) ){ return 0; }` |
|    105 | 5753 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; }` |
|     14 | 5754 | `	}` |
|     57 | 5755 | `	if( i!=nv ){ return 0; } /* trailing junk */` |
|      - | 5756 | `	/* The grammar above guarantees zv[0..nv) is a clean ASCII decimal float (no hex /` |
|      - | 5757 | `	 * inf / nan / trailing junk), so it is safe to hand to libc strtod, which — unlike` |
|      - | 5758 | `	 * SyStrToReal (15 sig-digits + exponent clamped to 308, so it silently saturates` |
|      - | 5759 | `	 * overflowing magnitudes to a finite value) — is overflow/underflow-aware and` |
|      - | 5760 | `	 * correctly rounded. strtod needs a NUL-terminated string: the ALLOW_THOUSAND path` |
|      - | 5761 | `	 * already built the span in zBuf (zv==zBuf); the plain path must copy it there (z is` |
|      - | 5762 | `	 * const + not NUL-terminated). nv <= n <= 500 < sizeof(zBuf) by the cap above.` |
|      - | 5763 | `	 * Matches PHP 8.5 byte-for-byte: reject overflow (-> +/-INF) and total underflow` |
|      - | 5764 | `	 * (-> 0.0), keep subnormals (nonzero, errno==ERANGE) and a genuine "0" (errno==0). */` |
|     53 | 5765 | `	if( zv != zBuf ){ SyMemcpy(zv,zBuf,(sxu32)nv); }` |
|     53 | 5766 | `	zBuf[nv] = 0;` |
|     53 | 5767 | `	errno = 0;` |
|     53 | 5768 | `	d = strtod(zBuf,0);` |
|     53 | 5769 | `	if( errno == ERANGE && (d == HUGE_VAL \|\| d == -HUGE_VAL \|\| d == 0.0) ){` |
|     15 | 5770 | `		return 0;` |
|      - | 5771 | `	}` |
|     39 | 5772 | `	*pOut = d;` |
|     39 | 5773 | `	return 1;` |
|     35 | 5774 | `}` |
|      - | 5775 | `/* FILTER_VALIDATE_BOOLEAN. Returns 1 if the string is recognized (sets *pBool),` |
|      - | 5776 | ` * 0 if it is unrecognized (the failure path). "0"/"false"/"" are recognized as` |
|      - | 5777 | ` * false, NOT failures. */` |
|     33 | 5778 | `static int FvValidateBool(const char *z,int n,int *pBool){` |
|     33 | 5779 | `	FvTrim(&z,&n);` |
|     32 | 5780 | `	if( (n==1 && z[0]=='1') \|\| (n==4 && SyStrnicmp(z,"true",4)==0)` |
|     25 | 5781 | `	    \|\| (n==2 && SyStrnicmp(z,"on",2)==0) \|\| (n==3 && SyStrnicmp(z,"yes",3)==0) ){` |
|     11 | 5782 | `		*pBool = 1; return 1;` |
|      - | 5783 | `	}` |
|     22 | 5784 | `	if( n==0 \|\| (n==1 && z[0]=='0') \|\| (n==5 && SyStrnicmp(z,"false",5)==0)` |
|     11 | 5785 | `	    \|\| (n==3 && SyStrnicmp(z,"off",3)==0) \|\| (n==2 && SyStrnicmp(z,"no",2)==0) ){` |
|     11 | 5786 | `		*pBool = 0; return 1;` |
|      - | 5787 | `	}` |
|      9 | 5788 | `	return 0;` |
|     15 | 5789 | `}` |
|      - | 5790 | `/* IPv4 dotted-quad: exactly 4 octets 0..255, no leading zeros. */` |
|     33 | 5791 | `static int FvValidateIp4(const char *z,int n){` |
|     33 | 5792 | `	int i = 0, parts = 0;` |
|     77 | 5793 | `	while( i<n ){` |
|     65 | 5794 | `		int val = 0, digits = 0, start = i;` |
|    143 | 5795 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){` |
|     85 | 5796 | `			val = val*10 + (z[i]-'0');` |
|     85 | 5797 | `			if( val>255 ){ return 0; }` |
|     79 | 5798 | `			digits++; i++;` |
|      1 | 5799 | `		}` |
|     59 | 5800 | `		if( digits==0 \|\| digits>3 ){ return 0; }` |
|     49 | 5801 | `		if( digits>1 && z[start]=='0' ){ return 0; } /* leading zero */` |
|     45 | 5802 | `		parts++;` |
|     45 | 5803 | `		if( parts>4 ){ return 0; }` |
|     45 | 5804 | `		if( i<n ){` |
|     33 | 5805 | `			if( z[i]!='.' ){ return 0; }` |
|     33 | 5806 | `			i++;` |
|     33 | 5807 | `			if( i>=n ){ return 0; } /* trailing dot */` |
|     16 | 5808 | `		}` |
|      1 | 5809 | `	}` |
|     13 | 5810 | `	return parts==4;` |
|     17 | 5811 | `}` |
|      - | 5812 | `/* A colon-separated run of IPv6 hextets with no "::" (n may be 0 -> 0 groups),` |
|      - | 5813 | ` * allowing a trailing embedded IPv4. Returns the 16-bit group count or -1. */` |
|     19 | 5814 | `static int FvIp6Hextets(const char *z,int n){` |
|     19 | 5815 | `	int i = 0, segStart = 0, groups = 0;` |
|     19 | 5816 | `	if( n==0 ){ return 0; }` |
|    145 | 5817 | `	while( i<=n ){` |
|    133 | 5818 | `		if( i==n \|\| z[i]==':' ){` |
|     23 | 5819 | `			int segLen = i - segStart, j, isV4 = 0;` |
|     23 | 5820 | `			if( segLen==0 ){ return -1; } /* an empty hextet (stray ':') */` |
|     77 | 5821 | `			for( j=segStart; j<i; j++ ){ if( z[j]=='.' ){ isV4 = 1; break; } }` |
|     23 | 5822 | `			if( isV4 ){` |
|     11 | 5823 | `				if( i!=n ){ return -1; } /* IPv4 only as the final token */` |
|     11 | 5824 | `				if( !FvValidateIp4(z+segStart,segLen) ){ return -1; }` |
|      5 | 5825 | `				groups += 2;` |
|      3 | 5826 | `			}else{` |
|     13 | 5827 | `				if( segLen>4 ){ return -1; }` |
|     47 | 5828 | `				for( j=segStart; j<i; j++ ){ if( SyHexToint((unsigned char)z[j])<0 ){ return -1; } }` |
|     13 | 5829 | `				groups++;` |
|      - | 5830 | `			}` |
|     17 | 5831 | `			segStart = i+1;` |
|      8 | 5832 | `		}` |
|    127 | 5833 | `		i++;` |
|      1 | 5834 | `	}` |
|     13 | 5835 | `	return groups;` |
|     10 | 5836 | `}` |
|      - | 5837 | `/* IPv6: at most one "::" zero-run; 8 groups exactly, or fewer when "::" present. */` |
|     19 | 5838 | `static int FvValidateIp6(const char *z,int n){` |
|     19 | 5839 | `	const char *zDbl = 0;` |
|      - | 5840 | `	int i, ga, gb;` |
|    139 | 5841 | `	for( i=0; i+1<n; i++ ){` |
|    123 | 5842 | `		if( z[i]==':' && z[i+1]==':' ){` |
|     13 | 5843 | `			if( zDbl ){ return 0; } /* a second "::" is invalid */` |
|     11 | 5844 | `			zDbl = z+i;` |
|      5 | 5845 | `		}` |
|     61 | 5846 | `	}` |
|     17 | 5847 | `	if( zDbl==0 ){` |
|      9 | 5848 | `		return FvIp6Hextets(z,n)==8;` |
|    ! 0 | 5849 | `	}else{` |
|      9 | 5850 | `		int lenA = (int)(zDbl - z);` |
|      9 | 5851 | `		int lenB = n - lenA - 2;` |
|      9 | 5852 | `		ga = (lenA==0) ? 0 : FvIp6Hextets(z,lenA);` |
|      9 | 5853 | `		gb = (lenB==0) ? 0 : FvIp6Hextets(zDbl+2,lenB);` |
|      9 | 5854 | `		if( ga<0 \|\| gb<0 ){ return 0; }` |
|      9 | 5855 | `		return (ga+gb)<=7; /* "::" stands for at least one zero group */` |
|      - | 5856 | `	}` |
|     10 | 5857 | `}` |
|     25 | 5858 | `static int FvValidateIp(const char *z,int n,int flags){` |
|     25 | 5859 | `	int v4 = (flags & FV_FLAG_IPV4), v6 = (flags & FV_FLAG_IPV6);` |
|     25 | 5860 | `	if( !v4 && !v6 ){ v4 = v6 = 1; } /* default accepts either family */` |
|     25 | 5861 | `	if( v4 && FvValidateIp4(z,n) ){ return 1; }` |
|     21 | 5862 | `	if( v6 && FvValidateIp6(z,n) ){ return 1; }` |
|     13 | 5863 | `	return 0;` |
|     13 | 5864 | `}` |
|      - | 5865 | `/* FILTER_VALIDATE_MAC: 17-char colon- or dash-separated hex (XX:XX:..:XX). */` |
|     11 | 5866 | `static int FvValidateMac(const char *z,int n){` |
|      - | 5867 | `	char sep;` |
|      - | 5868 | `	int i;` |
|     11 | 5869 | `	if( n!=17 ){ return 0; }` |
|      7 | 5870 | `	sep = z[2];` |
|      7 | 5871 | `	if( sep!=':' && sep!='-' ){ return 0; }` |
|    105 | 5872 | `	for( i=0; i<17; i++ ){` |
|    101 | 5873 | `		if( (i%3)==2 ){ if( z[i]!=sep ){ return 0; } }` |
|     71 | 5874 | `		else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }` |
|     50 | 5875 | `	}` |
|      5 | 5876 | `	return 1;` |
|      6 | 5877 | `}` |
|      - | 5878 | `/* FILTER_VALIDATE_EMAIL (best-effort: covers the common cases, not quoted local` |
|      - | 5879 | ` * parts or IP-literal domains). */` |
|     28 | 5880 | `static int FvValidateEmail(const char *z,int n){` |
|     28 | 5881 | `	int at = -1, i, localLen, domLen, labelStart, dotCount = 0;` |
|      - | 5882 | `	const char *zDom;` |
|     28 | 5883 | `	if( n==0 \|\| n>320 ){ return 0; }` |
|    201 | 5884 | `	for( i=0; i<n; i++ ){` |
|    181 | 5885 | `		if( z[i]=='@' ){ if( at>=0 ){ return 0; } at = i; }` |
|     91 | 5886 | `	}` |
|     21 | 5887 | `	if( at<=0 \|\| at==n-1 ){ return 0; } /* one '@', non-empty local and domain */` |
|     21 | 5888 | `	localLen = at;` |
|     21 | 5889 | `	zDom = z + at + 1;` |
|     21 | 5890 | `	domLen = n - at - 1;` |
|     21 | 5891 | `	if( z[0]=='.' \|\| z[at-1]=='.' ){ return 0; }` |
|     57 | 5892 | `	for( i=0; i<localLen; i++ ){` |
|     43 | 5893 | `		unsigned char c = (unsigned char)z[i];` |
|     43 | 5894 | `		if( c<=' ' ){ return 0; }` |
|     41 | 5895 | `		if( c=='.' && i+1<localLen && z[i+1]=='.' ){ return 0; }` |
|     20 | 5896 | `	}` |
|     15 | 5897 | `	if( zDom[0]=='.' \|\| zDom[domLen-1]=='.' ){ return 0; }` |
|     13 | 5898 | `	labelStart = 0;` |
|     85 | 5899 | `	for( i=0; i<=domLen; i++ ){` |
|     75 | 5900 | `		if( i==domLen \|\| zDom[i]=='.' ){` |
|     25 | 5901 | `			int ll = i - labelStart;` |
|     25 | 5902 | `			if( ll==0 ){ return 0; } /* consecutive dots */` |
|     23 | 5903 | `			if( zDom[labelStart]=='-' \|\| zDom[i-1]=='-' ){ return 0; }` |
|     23 | 5904 | `			if( i<domLen ){ dotCount++; }` |
|     23 | 5905 | `			labelStart = i+1;` |
|     12 | 5906 | `		}else{` |
|     51 | 5907 | `			unsigned char c = (unsigned char)zDom[i];` |
|     51 | 5908 | `			if( !((c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9')\|\|c=='-') ){ return 0; }` |
|      - | 5909 | `		}` |
|     37 | 5910 | `	}` |
|     11 | 5911 | `	if( dotCount<1 ){ return 0; } /* PHP requires a dot in the domain (any TLD length) */` |
|      9 | 5912 | `	return 1;` |
|     15 | 5913 | `}` |
|      - | 5914 | `/* FILTER_VALIDATE_DOMAIN (lenient, matching PHP without FILTER_FLAG_HOSTNAME). */` |
|     11 | 5915 | `static int FvValidateDomain(const char *z,int n){` |
|      - | 5916 | `	int i;` |
|     11 | 5917 | `	if( n<1 \|\| n>253 \|\| z[0]=='.' ){ return 0; }` |
|     81 | 5918 | `	for( i=0; i<n; i++ ){` |
|     75 | 5919 | `		unsigned char c = (unsigned char)z[i];` |
|     75 | 5920 | `		if( c<=' ' ){ return 0; }` |
|     75 | 5921 | `		if( c=='.' && i+1<n && z[i+1]=='.' ){ return 0; }` |
|     37 | 5922 | `	}` |
|      7 | 5923 | `	return 1;` |
|      6 | 5924 | `}` |
|      - | 5925 | `/* FILTER_VALIDATE_URL: require a scheme and a host (PHP's filter is itself` |
|      - | 5926 | ` * parse_url-based, so PH7_VmHttpSplitURI tracks it closely). */` |
|     15 | 5927 | `static int FvValidateUrl(const char *z,int n){` |
|      - | 5928 | `	SyhttpUri sUri;` |
|     15 | 5929 | `	if( n==0 ){ return 0; }` |
|     15 | 5930 | `	SyZero(&sUri,(sxu32)sizeof(sUri));` |
|     15 | 5931 | `	if( PH7_VmHttpSplitURI(&sUri,z,(sxu32)n)!=SXRET_OK ){ return 0; }` |
|     15 | 5932 | `	return sUri.sScheme.nByte!=0 && sUri.sHost.nByte!=0;` |
|      8 | 5933 | `}` |
|      - | 5934 | `/* The Fv sanitizers build their result by appending directly to the call` |
|      - | 5935 | ` * context (ph7_result_string accumulates, like htmlspecialchars), emitting each` |
|      - | 5936 | ` * kept run in one call and seeding "" so an all-stripped input yields "". */` |
|      - | 5937 | `/* SANITIZE_NUMBER_INT (isFloat=0) / SANITIZE_NUMBER_FLOAT (isFloat=1). */` |
|     37 | 5938 | `static void FvSanitizeNumber(ph7_context *pCtx,const char *z,int n,int isFloat,int flags){` |
|     37 | 5939 | `	int i, runStart = 0;` |
|     37 | 5940 | `	ph7_result_string(pCtx,"",0);` |
|     97 | 5941 | `	for( i=0; i<n; i++ ){` |
|     91 | 5942 | `		char c = z[i];` |
|     91 | 5943 | `		int keep = (c>='0'&&c<='9') \|\| c=='+' \|\| c=='-';` |
|     91 | 5944 | `		if( !keep && isFloat ){` |
|     38 | 5945 | `			keep = (c=='.' && (flags & FV_FLAG_ALLOW_FRACTION))` |
|     23 | 5946 | `			    \|\| (c==',' && (flags & FV_FLAG_ALLOW_THOUSAND))` |
|     36 | 5947 | `			    \|\| ((c=='e'\|\|c=='E') && (flags & FV_FLAG_ALLOW_SCIENTIFIC));` |
|     12 | 5948 | `		}` |
|     61 | 5949 | `		if( !keep ){` |
|     33 | 5950 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     33 | 5951 | `			runStart = i+1;` |
|     16 | 5952 | `		}` |
|     31 | 5953 | `	}` |
|      7 | 5954 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 | 5955 | `}` |
|      - | 5956 | `/* Return non-zero when byte c must be stripped under the STRIP_* flags. Shared` |
|      - | 5957 | ` * by the UNSAFE_RAW string filter and SANITIZE_SPECIAL_CHARS. STRIP_LOW drops` |
|      - | 5958 | `` * bytes <32, STRIP_HIGH drops bytes >=127 (incl. DEL), STRIP_BACKTICK drops '`'.`` |
|      - | 5959 | ` * Matches php_filter_strip(); verified byte-exact vs php 8.5.7. */` |
|    287 | 5960 | `static int FvStripByte(unsigned char c,int flags){` |
|    287 | 5961 | `	if( (flags & FV_FLAG_STRIP_LOW)      && c<32 )    { return 1; }` |
|    281 | 5962 | `	if( (flags & FV_FLAG_STRIP_HIGH)     && c>=127 )  { return 1; }` |
|    269 | 5963 | `	if( (flags & FV_FLAG_STRIP_BACKTICK) && c==0x60 ) { return 1; }` |
|    267 | 5964 | `	return 0;` |
|    144 | 5965 | `}` |
|      - | 5966 | `/* FILTER_UNSAFE_RAW / FILTER_DEFAULT with flags: no default transform, but the` |
|      - | 5967 | ` * STRIP/ENCODE flags apply. Precedence (per php_filter_unsafe_raw, verified` |
|      - | 5968 | ` * vs php 8.5.7): a byte is first tested for stripping; a surviving byte is then` |
|      - | 5969 | ` * encoded as a decimal numeric entity if ENCODE_LOW (<32) / ENCODE_HIGH (>=127)` |
|      - | 5970 | ` * is set, and '&' becomes "&#38;" under ENCODE_AMP. So STRIP_LOW\|ENCODE_LOW` |
|      - | 5971 | ` * strips (nothing left to encode). Bytes are treated individually — ENCODE_HIGH` |
|      - | 5972 | ` * numeric-encodes each byte of a multibyte sequence separately, not the codepoint. */` |
|     25 | 5973 | `static void FvSanitizeString(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 5974 | `	int i, runStart = 0;` |
|     25 | 5975 | `	ph7_result_string(pCtx,"",0);` |
|    193 | 5976 | `	for( i=0; i<n; i++ ){` |
|    179 | 5977 | `		unsigned char c = (unsigned char)z[i];` |
|    179 | 5978 | `		if( FvStripByte(c,flags) ){` |
|     13 | 5979 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     13 | 5980 | `			runStart = i+1;` |
|     13 | 5981 | `			continue;` |
|      - | 5982 | `		}` |
|    167 | 5983 | `		if( c=='&' && (flags & FV_FLAG_ENCODE_AMP) ){` |
|      3 | 5984 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      3 | 5985 | `			ph7_result_string(pCtx,"&#38;",-1);` |
|      3 | 5986 | `			runStart = i+1;` |
|    166 | 5987 | `		}else if( (c<32 && (flags & FV_FLAG_ENCODE_LOW))` |
|    164 | 5988 | `		       \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     37 | 5989 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 5990 | `			ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|      9 | 5991 | `			runStart = i+1;` |
|      4 | 5992 | `		}` |
|     79 | 5993 | `	}` |
|     15 | 5994 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     15 | 5995 | `}` |
|      - | 5996 | `/* FILTER_SANITIZE_SPECIAL_CHARS: encode <>&"' and every control byte <32 as a` |
|      - | 5997 | ` * decimal numeric entity (&#60; &#38; &#34; ...). The STRIP_* flags remove bytes` |
|      - | 5998 | ` * before encoding; ENCODE_HIGH numeric-encodes surviving bytes >=127. Bytes >=128` |
|      - | 5999 | ` * are otherwise passed through verbatim (this filter is NOT UTF-8-aware — only the` |
|      - | 6000 | ` * FULL variant is). Byte-exact vs php 8.5.7. */` |
|     13 | 6001 | `static void FvSanitizeSpecial(ph7_context *pCtx,const char *z,int n,int flags){` |
|     13 | 6002 | `	int i, runStart = 0;` |
|      - | 6003 | `	const char *zEnt;` |
|     13 | 6004 | `	ph7_result_string(pCtx,"",0);` |
|    131 | 6005 | `	for( i=0; i<n; i++ ){` |
|    119 | 6006 | `		unsigned char c = (unsigned char)z[i];` |
|    119 | 6007 | `		if( FvStripByte(c,flags) ){` |
|      9 | 6008 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 6009 | `			runStart = i+1;` |
|      9 | 6010 | `			continue;` |
|      - | 6011 | `		}` |
|    111 | 6012 | `		switch( c ){` |
|      3 | 6013 | `		case '<':  zEnt = "&#60;"; break;` |
|      3 | 6014 | `		case '>':  zEnt = "&#62;"; break;` |
|     11 | 6015 | `		case '&':  zEnt = "&#38;"; break;` |
|      3 | 6016 | `		case '"':  zEnt = "&#34;"; break;` |
|      3 | 6017 | `		case '\'': zEnt = "&#39;"; break;` |
|     46 | 6018 | `		default:` |
|      - | 6019 | `			/* Control bytes <32 are always numeric-encoded; bytes >=127 only when` |
|      - | 6020 | `			 * ENCODE_HIGH is set. Everything else stays in the current run. */` |
|     93 | 6021 | `			if( c<32 \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     17 | 6022 | `				if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     17 | 6023 | `				ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|     17 | 6024 | `				runStart = i+1;` |
|      8 | 6025 | `			}` |
|     93 | 6026 | `			continue; /* keep in the current run */` |
|      - | 6027 | `		}` |
|     19 | 6028 | `		if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     19 | 6029 | `		ph7_result_string(pCtx,zEnt,-1); /* -1: length from strlen */` |
|     19 | 6030 | `		runStart = i+1;` |
|     10 | 6031 | `	}` |
|     13 | 6032 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     13 | 6033 | `}` |
|      - | 6034 | `/* HTML 4.01 named-entity table (codepoint -> "&name;") used by the UTF-8-aware` |
|      - | 6035 | ` * FULL_SPECIAL_CHARS filter, sorted ascending by codepoint for binary search.` |
|      - | 6036 | ` * Generated from php 8.5.7 (the exact set php_escape_html_entities emits for the` |
|      - | 6037 | ` * default document type); the five inline specials <>&"' are handled separately,` |
|      - | 6038 | ` * so every entry here is a codepoint >=0xA0. 248 rows. */` |
|      - | 6039 | `static const struct { sxu32 cp; const char *zEnt; } aHtml401Ent[] = {` |
|      - | 6040 | `	{0x00A0,"&nbsp;"},{0x00A1,"&iexcl;"},{0x00A2,"&cent;"},{0x00A3,"&pound;"},` |
|      - | 6041 | `	{0x00A4,"&curren;"},{0x00A5,"&yen;"},{0x00A6,"&brvbar;"},{0x00A7,"&sect;"},` |
|      - | 6042 | `	{0x00A8,"&uml;"},{0x00A9,"&copy;"},{0x00AA,"&ordf;"},{0x00AB,"&laquo;"},` |
|      - | 6043 | `	{0x00AC,"&not;"},{0x00AD,"&shy;"},{0x00AE,"&reg;"},{0x00AF,"&macr;"},` |
|      - | 6044 | `	{0x00B0,"&deg;"},{0x00B1,"&plusmn;"},{0x00B2,"&sup2;"},{0x00B3,"&sup3;"},` |
|      - | 6045 | `	{0x00B4,"&acute;"},{0x00B5,"&micro;"},{0x00B6,"&para;"},{0x00B7,"&middot;"},` |
|      - | 6046 | `	{0x00B8,"&cedil;"},{0x00B9,"&sup1;"},{0x00BA,"&ordm;"},{0x00BB,"&raquo;"},` |
|      - | 6047 | `	{0x00BC,"&frac14;"},{0x00BD,"&frac12;"},{0x00BE,"&frac34;"},{0x00BF,"&iquest;"},` |
|      - | 6048 | `	{0x00C0,"&Agrave;"},{0x00C1,"&Aacute;"},{0x00C2,"&Acirc;"},{0x00C3,"&Atilde;"},` |
|      - | 6049 | `	{0x00C4,"&Auml;"},{0x00C5,"&Aring;"},{0x00C6,"&AElig;"},{0x00C7,"&Ccedil;"},` |
|      - | 6050 | `	{0x00C8,"&Egrave;"},{0x00C9,"&Eacute;"},{0x00CA,"&Ecirc;"},{0x00CB,"&Euml;"},` |
|      - | 6051 | `	{0x00CC,"&Igrave;"},{0x00CD,"&Iacute;"},{0x00CE,"&Icirc;"},{0x00CF,"&Iuml;"},` |
|      - | 6052 | `	{0x00D0,"&ETH;"},{0x00D1,"&Ntilde;"},{0x00D2,"&Ograve;"},{0x00D3,"&Oacute;"},` |
|      - | 6053 | `	{0x00D4,"&Ocirc;"},{0x00D5,"&Otilde;"},{0x00D6,"&Ouml;"},{0x00D7,"&times;"},` |
|      - | 6054 | `	{0x00D8,"&Oslash;"},{0x00D9,"&Ugrave;"},{0x00DA,"&Uacute;"},{0x00DB,"&Ucirc;"},` |
|      - | 6055 | `	{0x00DC,"&Uuml;"},{0x00DD,"&Yacute;"},{0x00DE,"&THORN;"},{0x00DF,"&szlig;"},` |
|      - | 6056 | `	{0x00E0,"&agrave;"},{0x00E1,"&aacute;"},{0x00E2,"&acirc;"},{0x00E3,"&atilde;"},` |
|      - | 6057 | `	{0x00E4,"&auml;"},{0x00E5,"&aring;"},{0x00E6,"&aelig;"},{0x00E7,"&ccedil;"},` |
|      - | 6058 | `	{0x00E8,"&egrave;"},{0x00E9,"&eacute;"},{0x00EA,"&ecirc;"},{0x00EB,"&euml;"},` |
|      - | 6059 | `	{0x00EC,"&igrave;"},{0x00ED,"&iacute;"},{0x00EE,"&icirc;"},{0x00EF,"&iuml;"},` |
|      - | 6060 | `	{0x00F0,"&eth;"},{0x00F1,"&ntilde;"},{0x00F2,"&ograve;"},{0x00F3,"&oacute;"},` |
|      - | 6061 | `	{0x00F4,"&ocirc;"},{0x00F5,"&otilde;"},{0x00F6,"&ouml;"},{0x00F7,"&divide;"},` |
|      - | 6062 | `	{0x00F8,"&oslash;"},{0x00F9,"&ugrave;"},{0x00FA,"&uacute;"},{0x00FB,"&ucirc;"},` |
|      - | 6063 | `	{0x00FC,"&uuml;"},{0x00FD,"&yacute;"},{0x00FE,"&thorn;"},{0x00FF,"&yuml;"},` |
|      - | 6064 | `	{0x0152,"&OElig;"},{0x0153,"&oelig;"},{0x0160,"&Scaron;"},{0x0161,"&scaron;"},` |
|      - | 6065 | `	{0x0178,"&Yuml;"},{0x0192,"&fnof;"},{0x02C6,"&circ;"},{0x02DC,"&tilde;"},` |
|      - | 6066 | `	{0x0391,"&Alpha;"},{0x0392,"&Beta;"},{0x0393,"&Gamma;"},{0x0394,"&Delta;"},` |
|      - | 6067 | `	{0x0395,"&Epsilon;"},{0x0396,"&Zeta;"},{0x0397,"&Eta;"},{0x0398,"&Theta;"},` |
|      - | 6068 | `	{0x0399,"&Iota;"},{0x039A,"&Kappa;"},{0x039B,"&Lambda;"},{0x039C,"&Mu;"},` |
|      - | 6069 | `	{0x039D,"&Nu;"},{0x039E,"&Xi;"},{0x039F,"&Omicron;"},{0x03A0,"&Pi;"},` |
|      - | 6070 | `	{0x03A1,"&Rho;"},{0x03A3,"&Sigma;"},{0x03A4,"&Tau;"},{0x03A5,"&Upsilon;"},` |
|      - | 6071 | `	{0x03A6,"&Phi;"},{0x03A7,"&Chi;"},{0x03A8,"&Psi;"},{0x03A9,"&Omega;"},` |
|      - | 6072 | `	{0x03B1,"&alpha;"},{0x03B2,"&beta;"},{0x03B3,"&gamma;"},{0x03B4,"&delta;"},` |
|      - | 6073 | `	{0x03B5,"&epsilon;"},{0x03B6,"&zeta;"},{0x03B7,"&eta;"},{0x03B8,"&theta;"},` |
|      - | 6074 | `	{0x03B9,"&iota;"},{0x03BA,"&kappa;"},{0x03BB,"&lambda;"},{0x03BC,"&mu;"},` |
|      - | 6075 | `	{0x03BD,"&nu;"},{0x03BE,"&xi;"},{0x03BF,"&omicron;"},{0x03C0,"&pi;"},` |
|      - | 6076 | `	{0x03C1,"&rho;"},{0x03C2,"&sigmaf;"},{0x03C3,"&sigma;"},{0x03C4,"&tau;"},` |
|      - | 6077 | `	{0x03C5,"&upsilon;"},{0x03C6,"&phi;"},{0x03C7,"&chi;"},{0x03C8,"&psi;"},` |
|      - | 6078 | `	{0x03C9,"&omega;"},{0x03D1,"&thetasym;"},{0x03D2,"&upsih;"},{0x03D6,"&piv;"},` |
|      - | 6079 | `	{0x2002,"&ensp;"},{0x2003,"&emsp;"},{0x2009,"&thinsp;"},{0x200C,"&zwnj;"},` |
|      - | 6080 | `	{0x200D,"&zwj;"},{0x200E,"&lrm;"},{0x200F,"&rlm;"},{0x2013,"&ndash;"},` |
|      - | 6081 | `	{0x2014,"&mdash;"},{0x2018,"&lsquo;"},{0x2019,"&rsquo;"},{0x201A,"&sbquo;"},` |
|      - | 6082 | `	{0x201C,"&ldquo;"},{0x201D,"&rdquo;"},{0x201E,"&bdquo;"},{0x2020,"&dagger;"},` |
|      - | 6083 | `	{0x2021,"&Dagger;"},{0x2022,"&bull;"},{0x2026,"&hellip;"},{0x2030,"&permil;"},` |
|      - | 6084 | `	{0x2032,"&prime;"},{0x2033,"&Prime;"},{0x2039,"&lsaquo;"},{0x203A,"&rsaquo;"},` |
|      - | 6085 | `	{0x203E,"&oline;"},{0x2044,"&frasl;"},{0x20AC,"&euro;"},{0x2111,"&image;"},` |
|      - | 6086 | `	{0x2118,"&weierp;"},{0x211C,"&real;"},{0x2122,"&trade;"},{0x2135,"&alefsym;"},` |
|      - | 6087 | `	{0x2190,"&larr;"},{0x2191,"&uarr;"},{0x2192,"&rarr;"},{0x2193,"&darr;"},` |
|      - | 6088 | `	{0x2194,"&harr;"},{0x21B5,"&crarr;"},{0x21D0,"&lArr;"},{0x21D1,"&uArr;"},` |
|      - | 6089 | `	{0x21D2,"&rArr;"},{0x21D3,"&dArr;"},{0x21D4,"&hArr;"},{0x2200,"&forall;"},` |
|      - | 6090 | `	{0x2202,"&part;"},{0x2203,"&exist;"},{0x2205,"&empty;"},{0x2207,"&nabla;"},` |
|      - | 6091 | `	{0x2208,"&isin;"},{0x2209,"&notin;"},{0x220B,"&ni;"},{0x220F,"&prod;"},` |
|      - | 6092 | `	{0x2211,"&sum;"},{0x2212,"&minus;"},{0x2217,"&lowast;"},{0x221A,"&radic;"},` |
|      - | 6093 | `	{0x221D,"&prop;"},{0x221E,"&infin;"},{0x2220,"&ang;"},{0x2227,"&and;"},` |
|      - | 6094 | `	{0x2228,"&or;"},{0x2229,"&cap;"},{0x222A,"&cup;"},{0x222B,"&int;"},` |
|      - | 6095 | `	{0x2234,"&there4;"},{0x223C,"&sim;"},{0x2245,"&cong;"},{0x2248,"&asymp;"},` |
|      - | 6096 | `	{0x2260,"&ne;"},{0x2261,"&equiv;"},{0x2264,"&le;"},{0x2265,"&ge;"},` |
|      - | 6097 | `	{0x2282,"&sub;"},{0x2283,"&sup;"},{0x2284,"&nsub;"},{0x2286,"&sube;"},` |
|      - | 6098 | `	{0x2287,"&supe;"},{0x2295,"&oplus;"},{0x2297,"&otimes;"},{0x22A5,"&perp;"},` |
|      - | 6099 | `	{0x22C5,"&sdot;"},{0x2308,"&lceil;"},{0x2309,"&rceil;"},{0x230A,"&lfloor;"},` |
|      - | 6100 | `	{0x230B,"&rfloor;"},{0x2329,"&lang;"},{0x232A,"&rang;"},{0x25CA,"&loz;"},` |
|      - | 6101 | `	{0x2660,"&spades;"},{0x2663,"&clubs;"},{0x2665,"&hearts;"},{0x2666,"&diams;"}` |
|      - | 6102 | `};` |
|      - | 6103 | `/* Binary-search aHtml401Ent[] for cp; return its "&name;" entity or 0. */` |
|     41 | 6104 | `static const char *FvHtml401Lookup(sxu32 cp){` |
|     41 | 6105 | `	int lo = 0, hi = (int)SX_ARRAYSIZE(aHtml401Ent) - 1;` |
|    323 | 6106 | `	while( lo <= hi ){` |
|    309 | 6107 | `		int mid = (lo + hi) / 2;` |
|    309 | 6108 | `		sxu32 c = aHtml401Ent[mid].cp;` |
|    309 | 6109 | `		if( c == cp ){ return aHtml401Ent[mid].zEnt; }` |
|    283 | 6110 | `		if( c < cp ){ lo = mid + 1; } else { hi = mid - 1; }` |
|      1 | 6111 | `	}` |
|     15 | 6112 | `	return 0;` |
|     21 | 6113 | `}` |
|      - | 6114 | `/* Decode one strict-UTF-8 sequence at p (< zEnd). On success returns its byte` |
|      - | 6115 | ` * length (1..4) and sets *pCp to the codepoint; on any malformed, overlong,` |
|      - | 6116 | ` * surrogate, truncated or out-of-range (>U+10FFFF) sequence returns 0. Matches` |
|      - | 6117 | ` * PHP's UTF-8 validation used by FULL_SPECIAL_CHARS (verified vs php 8.5.7). */` |
|    101 | 6118 | `static int FvUtf8Next(const unsigned char *p,const unsigned char *zEnd,sxu32 *pCp){` |
|    101 | 6119 | `	unsigned char c = p[0];` |
|    101 | 6120 | `	if( c < 0x80 ){ *pCp = c; return 1; }` |
|    101 | 6121 | `	if( c < 0xC2 ){ return 0; }              /* 0x80-0xBF stray cont / 0xC0-0xC1 overlong */` |
|     99 | 6122 | `	if( c < 0xE0 ){                          /* 2-byte: U+0080..U+07FF */` |
|     47 | 6123 | `		if( zEnd-p < 2 \|\| (p[1]&0xC0)!=0x80 ){ return 0; }` |
|     45 | 6124 | `		*pCp = ((sxu32)(c&0x1F)<<6) \| (p[1]&0x3F);` |
|     45 | 6125 | `		return 2;` |
|      - | 6126 | `	}` |
|     53 | 6127 | `	if( c < 0xF0 ){                          /* 3-byte: U+0800..U+FFFF minus surrogates */` |
|      - | 6128 | `		sxu32 cp;` |
|     47 | 6129 | `		if( zEnd-p < 3 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 ){ return 0; }` |
|     33 | 6130 | `		cp = ((sxu32)(c&0x0F)<<12) \| ((sxu32)(p[1]&0x3F)<<6) \| (p[2]&0x3F);` |
|     33 | 6131 | `		if( cp < 0x800 \|\| (cp>=0xD800 && cp<=0xDFFF) ){ return 0; }` |
|     29 | 6132 | `		*pCp = cp;` |
|     29 | 6133 | `		return 3;` |
|      - | 6134 | `	}` |
|      7 | 6135 | `	if( c < 0xF5 ){                          /* 4-byte: U+10000..U+10FFFF */` |
|      - | 6136 | `		sxu32 cp;` |
|      5 | 6137 | `		if( zEnd-p < 4 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 \|\| (p[3]&0xC0)!=0x80 ){ return 0; }` |
|      5 | 6138 | `		cp = ((sxu32)(c&0x07)<<18) \| ((sxu32)(p[1]&0x3F)<<12) \| ((sxu32)(p[2]&0x3F)<<6) \| (p[3]&0x3F);` |
|      5 | 6139 | `		if( cp < 0x10000 \|\| cp > 0x10FFFF ){ return 0; }` |
|      5 | 6140 | `		*pCp = cp;` |
|      5 | 6141 | `		return 4;` |
|      - | 6142 | `	}` |
|      3 | 6143 | `	return 0;                                /* 0xF5-0xFF */` |
|     51 | 6144 | `}` |
|      - | 6145 | `/* FILTER_SANITIZE_FULL_SPECIAL_CHARS: htmlentities-style, UTF-8-aware. Encodes` |
|      - | 6146 | ` * <>&"' as named entities ("'" -> &#039;; quotes suppressed under NO_ENCODE_QUOTES),` |
|      - | 6147 | ` * and every valid UTF-8 codepoint with an HTML 4.01 named entity as that entity;` |
|      - | 6148 | ` * valid codepoints without a named entity (and low control bytes) pass through` |
|      - | 6149 | ` * verbatim. If the input contains ANY invalid UTF-8 the whole result is "".` |
|      - | 6150 | ` * The STRIP/ENCODE flags do NOT apply to this filter (only NO_ENCODE_QUOTES).` |
|      - | 6151 | ` * php's filter does NOT re-encode valid pre-existing entities ("&amp;" stays,` |
|      - | 6152 | ` * "&bogus;" becomes "&amp;bogus;"), i.e. double_encode=false semantics —` |
|      - | 6153 | ` * exactly htmlentities(ENT_QUOTES\|ENT_HTML401, double_encode: false), so this` |
|      - | 6154 | ` * delegates to the shared encoder. Byte-exact vs php 8.5.7. */` |
|     25 | 6155 | `static void FvSanitizeFull(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 6156 | `	int iEntFlags = (flags & FV_FLAG_NO_ENCODE_QUOTES) ? 0 : PH7_ENT_QUOTES;` |
|     25 | 6157 | `	HtmlEscape(pCtx,z,n,iEntFlags,1/*bAll*/,0/*bDoubleEncode*/);` |
|     25 | 6158 | `}` |
|      - | 6159 | `/* ---------------------------------------------------------------------------` |
|      - | 6160 | ` * UTF-8-aware HTML entity core (htmlspecialchars/htmlentities family).` |
|      - | 6161 | ` * Prototyped next to the five builtins earlier in this file; lives here so it` |
|      - | 6162 | ` * can share aHtml401Ent[]/FvHtml401Lookup()/FvUtf8Next() with the filter_var` |
|      - | 6163 | ` * FULL_SPECIAL_CHARS filter above. Byte-exact vs php 8.5.7 (oracle-swept).` |
|      - | 6164 | ` * ------------------------------------------------------------------------ */` |
|      - | 6165 | `/* Encode cp as UTF-8 into zBuf (>= 4 bytes); return the byte length 1..4.` |
|      - | 6166 | ` * Thin wrapper over the engine-wide SX_WRITE_UTF8 (sxmacros.h). */` |
|    585 | 6167 | `static int HtmlCpUtf8(sxu32 cp,char *zBuf){` |
|    585 | 6168 | `	sxu8 *z = (sxu8 *)zBuf;` |
|    585 | 6169 | `	SX_WRITE_UTF8(z,cp);` |
|    585 | 6170 | `	return (int)(z - (sxu8 *)zBuf);` |
|      1 | 6171 | `}` |
|      - | 6172 | `/* Doctype-allowed codepoint test (php's unicode_cp_is_allowed) — gates what a` |
|      - | 6173 | ` * numeric reference may DECODE to. Oracle-pinned per doctype: HTML401` |
|      - | 6174 | ` * disallows C0 (except TAB/LF/CR) and DEL..U+009F; XML1 and XHTML share the` |
|      - | 6175 | ` * XML rules — DEL..U+009F allowed, U+FFFE/U+FFFF excluded; HTML5 swaps CR` |
|      - | 6176 | ` * for FF (0x0C) and excludes the noncharacters (U+FDD0..U+FDEF and every` |
|      - | 6177 | ` * U+xFFFE/U+xFFFF). Surrogates are disallowed everywhere. */` |
|     91 | 6178 | `static int HtmlCpAllowed(sxu32 cp,int iFlags){` |
|     91 | 6179 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|     91 | 6180 | `	if( cp==0x09 \|\| cp==0x0A ){ return 1; }` |
|     87 | 6181 | `	if( cp==0x0D ){ return iDoc != PH7_ENT_DOC_HTML5; }` |
|     85 | 6182 | `	if( cp==0x0C ){ return iDoc == PH7_ENT_DOC_HTML5; }` |
|     85 | 6183 | `	if( cp < 0x20 \|\| cp > 0x10FFFF ){ return 0; }` |
|     79 | 6184 | `	if( cp>=0xD800 && cp<=0xDFFF ){ return 0; }` |
|     77 | 6185 | `	if( cp>=0x7F && cp<=0x9F ){ return iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML; }` |
|     71 | 6186 | `	if( iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML ){` |
|    ! 0 | 6187 | `		return cp!=0xFFFE && cp!=0xFFFF;` |
|      - | 6188 | `	}` |
|     71 | 6189 | `	if( iDoc == PH7_ENT_DOC_HTML5 ){` |
|      9 | 6190 | `		if( cp>=0xFDD0 && cp<=0xFDEF ){ return 0; }` |
|      9 | 6191 | `		if( (cp & 0xFFFF) >= 0xFFFE ){ return 0; }` |
|      4 | 6192 | `	}` |
|     71 | 6193 | `	return 1;` |
|     46 | 6194 | `}` |
|      - | 6195 | `/* The ENT_DISALLOWED gate for RAW characters on the ENCODE side. Same as the` |
|      - | 6196 | ` * decode gate except CR under HTML5: php's encode-side unicode_cp_is_allowed` |
|      - | 6197 | ` * keeps a literal "\r" verbatim under ENT_HTML5\|ENT_DISALLOWED while the` |
|      - | 6198 | ` * decode side leaves "&#13;" un-decoded (oracle-pinned at flags 176). */` |
|      9 | 6199 | `static int HtmlCpAllowedEncode(sxu32 cp,int iFlags){` |
|      9 | 6200 | `	if( cp==0x0D && (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 ){ return 1; }` |
|      9 | 6201 | `	return HtmlCpAllowed(cp,iFlags);` |
|      5 | 6202 | `}` |
|      - | 6203 | `/* Numeric-reference validity for the double_encode=false "is this already a` |
|      - | 6204 | ` * valid entity" test — a MUCH looser predicate than the decode gate above:` |
|      - | 6205 | ` * any codepoint <= U+10FFFF is valid (controls and surrogates included, every` |
|      - | 6206 | ` * doctype). ENT_DISALLOWED re-tightens non-HTML401 doctypes to the decode` |
|      - | 6207 | ` * gate, except that HTML5 exempts surrogates. All oracle-pinned: &#0; and` |
|      - | 6208 | ` * &#xD800; stay verbatim at flags 11 and 139; flags -1 (HTML5+DISALLOWED)` |
|      - | 6209 | ` * re-encodes &#0; and &#x10FFFF; but still keeps &#xD800;; flags 144` |
|      - | 6210 | ` * (XML1+DISALLOWED) re-encodes &#xD800;. */` |
|      9 | 6211 | `static int HtmlNumericAllowed(sxu32 cp,int iFlags){` |
|      9 | 6212 | `	if( cp > 0x10FFFF ){ return 0; }` |
|      7 | 6213 | `	if( (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML401 ){ return 1; /* never tightened */ }` |
|    ! 0 | 6214 | `	if( (iFlags & PH7_ENT_DISALLOWED)` |
|    ! 0 | 6215 | `	 && !((iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 && cp>=0xD800 && cp<=0xDFFF)` |
|    ! 0 | 6216 | `	 && !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|    ! 0 | 6217 | `	return 1;` |
|      5 | 6218 | `}` |
|      - | 6219 | `/* How many bytes the malformed UTF-8 sequence at p consumes — php's` |
|      - | 6220 | ` * get_next_char failure step (one U+FFFD substitution / one ENT_IGNORE drop` |
|      - | 6221 | ` * per MAXIMAL invalid subpart, not per byte): a prefix-valid sequence eats` |
|      - | 6222 | ` * its continuation bytes ("\xE0\x80\xAF" is ONE unit) while a byte that could` |
|      - | 6223 | ` * start a new sequence is left for the next round. */` |
|      5 | 6224 | `static int HtmlUtf8Trail(unsigned char c){ return c>=0x80 && c<=0xBF; }` |
|     11 | 6225 | `static int HtmlUtf8Lead(unsigned char c){ return c<0x80 \|\| (c>=0xC2 && c<=0xF4); }` |
|     15 | 6226 | `static int HtmlUtf8FailAdvance(const unsigned char *p,const unsigned char *zEnd){` |
|     15 | 6227 | `	unsigned char c = p[0];` |
|     15 | 6228 | `	int nAvail = (int)(zEnd - p);` |
|     15 | 6229 | `	if( c < 0xC2 \|\| c > 0xF4 ){ return 1; } /* stray trail / C0-C1 / F5-FF */` |
|     13 | 6230 | `	if( c < 0xE0 ){` |
|      3 | 6231 | `		if( nAvail < 2 ){ return 1; }` |
|      3 | 6232 | `		return HtmlUtf8Lead(p[1]) ? 1 : 2;` |
|      - | 6233 | `	}` |
|     11 | 6234 | `	if( c < 0xF0 ){` |
|     11 | 6235 | `		if( nAvail >= 3 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) ){` |
|      3 | 6236 | `			return 3; /* complete but overlong/surrogate */` |
|      - | 6237 | `		}` |
|      9 | 6238 | `		if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 6239 | `		if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 6240 | `		return 3;` |
|      - | 6241 | `	}` |
|    ! 0 | 6242 | `	if( nAvail >= 4 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) && HtmlUtf8Trail(p[3]) ){` |
|    ! 0 | 6243 | `		return 4; /* complete but overlong / > U+10FFFF */` |
|      - | 6244 | `	}` |
|    ! 0 | 6245 | `	if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 6246 | `	if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 6247 | `	if( nAvail < 4 \|\| HtmlUtf8Lead(p[3]) ){ return 3; }` |
|    ! 0 | 6248 | `	return 4;` |
|      8 | 6249 | `}` |
|      - | 6250 | `/* The basic special entities, shared by named matching, the hsc_decode` |
|      - | 6251 | ` * numeric whitelist and the translation-table builder so the sets can never` |
|      - | 6252 | ` * drift apart. (&apos; is not an HTML 4.01 entity — doctype-gated below.) */` |
|      - | 6253 | `static const struct { const char *zEnt; int n; sxu32 cp; } aHtmlSpecEnt[] = {` |
|      - | 6254 | `	{"&amp;",5,38},{"&lt;",4,60},{"&gt;",4,62},{"&quot;",6,34},{"&apos;",6,39}` |
|      - | 6255 | `};` |
|      - | 6256 | `/* Does this doctype consult the named-entity table (aHtml401Ent)? XML 1.0 has` |
|      - | 6257 | ` * no named entities beyond the specials; XHTML/HTML5 are approximated by the` |
|      - | 6258 | ` * HTML 4.01 table (documented divergence). */` |
|     63 | 6259 | `static int HtmlDocHasNamedTable(int iDoc){` |
|     63 | 6260 | `	return iDoc != PH7_ENT_DOC_XML1;` |
|      1 | 6261 | `}` |
|      - | 6262 | `/* The single-quote entity per doctype. Oracle-pinned asymmetry: for every` |
|      - | 6263 | ` * non-HTML401 doctype htmlspecialchars emits &apos; while htmlentities` |
|      - | 6264 | ` * (bEntities) keeps &#039; under XHTML too. The translation table mirrors` |
|      - | 6265 | ` * whichever function the requested table belongs to. */` |
|     29 | 6266 | `static const char *HtmlAposEntity(int iDoc,int bEntities){` |
|     29 | 6267 | `	if( iDoc == PH7_ENT_DOC_HTML401 \|\| (bEntities && iDoc == PH7_ENT_DOC_XHTML) ){` |
|     21 | 6268 | `		return "&#039;";` |
|      - | 6269 | `	}` |
|      9 | 6270 | `	return "&apos;";` |
|     15 | 6271 | `}` |
|      - | 6272 | `/* Try to parse one HTML entity at z (z[0]=='&', z < zEnd). bFull selects the` |
|      - | 6273 | ` * html_entity_decode set (doctype named table + any allowed numeric ref) vs` |
|      - | 6274 | ` * the htmlspecialchars_decode set (the basic specials + quote numerics only).` |
|      - | 6275 | ` * Named matching is case-SENSITIVE and the ';' is required (both PHP-exact);` |
|      - | 6276 | ` * numeric refs accept dec/hex (x or X) with any number of leading zeros but` |
|      - | 6277 | ` * reject out-of-range, surrogate and doctype-disallowed codepoints (the` |
|      - | 6278 | ` * caller then leaves the source verbatim). Quote-flag gating is NOT applied` |
|      - | 6279 | ` * here — the same routine doubles as the "is this a valid entity" test for` |
|      - | 6280 | ` * double_encode=false, which ignores the quote bits (oracle-pinned).` |
|      - | 6281 | ` * bEncodeCheck selects the looser HtmlNumericAllowed predicate used by that` |
|      - | 6282 | ` * double_encode test; decode callers pass 0 for the HtmlCpAllowed gate.` |
|      - | 6283 | ` * On success sets *pCp / *pnConsumed and returns 1. */` |
|    172 | 6284 | `static int HtmlParseEntity(const unsigned char *z,const unsigned char *zEnd,` |
|      1 | 6285 | `                           int iFlags,int bFull,int bEncodeCheck,sxu32 *pCp,int *pnConsumed){` |
|    173 | 6286 | `	int nAvail = (int)(zEnd - z);` |
|    173 | 6287 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6288 | `	sxu32 n;` |
|    173 | 6289 | `	if( nAvail < 4 ){ return 0; } /* shortest entities: &lt; &#9; */` |
|    169 | 6290 | `	if( z[1] == '#' ){` |
|      - | 6291 | `		/* Numeric reference */` |
|     89 | 6292 | `		sxu32 cp = 0;` |
|     89 | 6293 | `		int i = 2, bHex = 0, nDig = 0;` |
|     89 | 6294 | `		if( z[i]=='x' \|\| z[i]=='X' ){ bHex = 1; i++; }` |
|    317 | 6295 | `		for( ; i < nAvail && z[i] != ';' ; i++ ){` |
|      - | 6296 | `			int v;` |
|    221 | 6297 | `			unsigned char c = z[i];` |
|    221 | 6298 | `			if( c>='0' && c<='9' ){ v = c - '0'; }` |
|     17 | 6299 | `			else if( bHex && c>='a' && c<='f' ){ v = c - 'a' + 10; }` |
|     17 | 6300 | `			else if( bHex && c>='A' && c<='F' ){ v = c - 'A' + 10; }` |
|    ! 0 | 6301 | `			else { return 0; }` |
|      - | 6302 | `			/* Stop accumulating once out of range (keeps validating the shape;` |
|      - | 6303 | `			 * max intermediate is 0x10FFFF*16+15, no sxu32 overflow). */` |
|    221 | 6304 | `			if( cp <= 0x10FFFF ){ cp = cp * (bHex ? 16 : 10) + (sxu32)v; }` |
|    221 | 6305 | `			nDig++;` |
|    111 | 6306 | `		}` |
|     97 | 6307 | `		if( nDig == 0 \|\| i >= nAvail ){ return 0; } /* no digits / no ';' */` |
|     97 | 6308 | `		if( bEncodeCheck ? !HtmlNumericAllowed(cp,iFlags) : !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|     83 | 6309 | `		if( !bFull ){` |
|      - | 6310 | `			/* hsc_decode: numeric refs to the five specials only. */` |
|     99 | 6311 | `			for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) && aHtmlSpecEnt[n].cp != cp ; n++ ){}` |
|     25 | 6312 | `			if( n >= SX_ARRAYSIZE(aHtmlSpecEnt) ){ return 0; }` |
|     11 | 6313 | `		}` |
|     75 | 6314 | `		*pCp = cp;` |
|     75 | 6315 | `		*pnConsumed = i + 1;` |
|     75 | 6316 | `		return 1;` |
|      - | 6317 | `	}` |
|      - | 6318 | `	/* Named reference — every entity name starts with a letter, so anything` |
|      - | 6319 | `	 * else can bail out before touching the tables. */` |
|     81 | 6320 | `	if( !((z[1]>='a' && z[1]<='z') \|\| (z[1]>='A' && z[1]<='Z')) ){ return 0; }` |
|    287 | 6321 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) ; n++ ){` |
|    265 | 6322 | `		if( aHtmlSpecEnt[n].cp == 39 && iDoc == PH7_ENT_DOC_HTML401 ){ continue; }` |
|    243 | 6323 | `		if( nAvail >= aHtmlSpecEnt[n].n && SyMemcmp(z,aHtmlSpecEnt[n].zEnt,(sxu32)aHtmlSpecEnt[n].n) == 0 ){` |
|     53 | 6324 | `			*pCp = aHtmlSpecEnt[n].cp;` |
|     53 | 6325 | `			*pnConsumed = aHtmlSpecEnt[n].n;` |
|     53 | 6326 | `			return 1;` |
|      - | 6327 | `		}` |
|     96 | 6328 | `	}` |
|     23 | 6329 | `	if( bFull && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 6330 | `		/* Linear scan of the 248-row table: runs only at '&'-then-letter` |
|      - | 6331 | `		 * positions and guarantees the decode set can never drift from the` |
|      - | 6332 | `		 * encode table. The first-letter guard skips the SyStrlen/SyMemcmp` |
|      - | 6333 | `		 * for ~96% of rows. */` |
|   3369 | 6334 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|      - | 6335 | `			sxu32 nEnt;` |
|   3357 | 6336 | `			if( z[1] != (unsigned char)aHtml401Ent[n].zEnt[1] ){ continue; }` |
|    121 | 6337 | `			nEnt = SyStrlen(aHtml401Ent[n].zEnt);` |
|    121 | 6338 | `			if( (sxu32)nAvail >= nEnt && SyMemcmp(z,aHtml401Ent[n].zEnt,nEnt) == 0 ){` |
|      7 | 6339 | `				*pCp = aHtml401Ent[n].cp;` |
|      7 | 6340 | `				*pnConsumed = (int)nEnt;` |
|      7 | 6341 | `				return 1;` |
|      - | 6342 | `			}` |
|     58 | 6343 | `		}` |
|      6 | 6344 | `	}` |
|     17 | 6345 | `	return 0;` |
|     88 | 6346 | `}` |
|      - | 6347 | `/* Shared encoder for htmlspecialchars (bAll=0) and htmlentities (bAll=1).` |
|      - | 6348 | ` * Invalid UTF-8 policy: ENT_IGNORE drops the byte (and wins over SUBSTITUTE),` |
|      - | 6349 | ` * ENT_SUBSTITUTE emits one U+FFFD per invalid byte, neither -> the whole` |
|      - | 6350 | ` * result is "" (pre-validated in a first pass: the accumulating result API` |
|      - | 6351 | ` * cannot roll back — same reason FvSanitizeFull is two-pass). */` |
|     96 | 6352 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 6353 | `                       int iFlags,int bAll,int bDoubleEncode){` |
|     97 | 6354 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     97 | 6355 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|      - | 6356 | `	const unsigned char *runStart;` |
|     97 | 6357 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6358 | `	sxu32 cp;` |
|     97 | 6359 | `	if( (iFlags & (PH7_ENT_IGNORE\|PH7_ENT_SUBSTITUTE)) == 0 ){` |
|      - | 6360 | `		/* Pass 1: any malformed sequence rejects the entire input. ASCII` |
|      - | 6361 | `		 * bytes cannot be malformed, so skip them without the decoder. */` |
|    381 | 6362 | `		while( p < zEnd ){` |
|      - | 6363 | `			int len;` |
|    323 | 6364 | `			if( *p < 0x80 ){ p++; continue; }` |
|     37 | 6365 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     37 | 6366 | `			if( len == 0 ){ ph7_result_string(pCtx,"",0); return; }` |
|     27 | 6367 | `			p += len;` |
|      1 | 6368 | `		}` |
|     59 | 6369 | `		p = (const unsigned char *)zIn;` |
|     29 | 6370 | `	}` |
|     87 | 6371 | `	runStart = p;` |
|     87 | 6372 | `	ph7_result_string(pCtx,"",0);` |
|    463 | 6373 | `	while( p < zEnd ){` |
|    377 | 6374 | `		const char *zEnt = 0;` |
|      - | 6375 | `		int len;` |
|    377 | 6376 | `		if( *p < 0x80 ){` |
|    313 | 6377 | `			len = 1;` |
|    313 | 6378 | `			switch( *p ){` |
|     25 | 6379 | `			case '<': zEnt = "&lt;"; break;` |
|     25 | 6380 | `			case '>': zEnt = "&gt;"; break;` |
|     18 | 6381 | `			case '&':` |
|     37 | 6382 | `				zEnt = "&amp;";` |
|     37 | 6383 | `				if( !bDoubleEncode ){` |
|      - | 6384 | `					sxu32 eCp; int nEat;` |
|     25 | 6385 | `					if( HtmlParseEntity(p,zEnd,iFlags,1,1,&eCp,&nEat) ){` |
|      - | 6386 | `						/* A valid existing entity: keep it verbatim. */` |
|     13 | 6387 | `						zEnt = 0;` |
|     13 | 6388 | `						len = nEat;` |
|      6 | 6389 | `					}` |
|     12 | 6390 | `				}` |
|     37 | 6391 | `				break;` |
|     10 | 6392 | `			case '"':` |
|     21 | 6393 | `				if( iFlags & PH7_ENT_QUOTE_DOUBLE ){ zEnt = "&quot;"; }` |
|     21 | 6394 | `				break;` |
|     12 | 6395 | `			case '\'':` |
|     25 | 6396 | `				if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|     23 | 6397 | `					zEnt = HtmlAposEntity(iDoc,bAll);` |
|     11 | 6398 | `				}` |
|     25 | 6399 | `				break;` |
|     92 | 6400 | `			default:` |
|    185 | 6401 | `				if( (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode((sxu32)*p,iFlags) ){` |
|    ! 0 | 6402 | `					zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 6403 | `				}` |
|    184 | 6404 | `				break;` |
|      - | 6405 | `			}` |
|    157 | 6406 | `		}else{` |
|     65 | 6407 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     65 | 6408 | `			if( len == 0 ){` |
|      - | 6409 | `				/* Malformed subpart (IGNORE or SUBSTITUTE is set, else pass 1` |
|      - | 6410 | `				 * would have rejected): drop it or emit ONE U+FFFD for the` |
|      - | 6411 | `				 * whole unit (php substitutes per maximal invalid subpart). */` |
|     15 | 6412 | `				if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|     15 | 6413 | `				if( (iFlags & PH7_ENT_IGNORE) == 0 ){ ph7_result_string(pCtx,"\xEF\xBF\xBD",3); }` |
|     15 | 6414 | `				p += HtmlUtf8FailAdvance(p,zEnd);` |
|     15 | 6415 | `				runStart = p;` |
|     15 | 6416 | `				continue;` |
|      - | 6417 | `			}` |
|     51 | 6418 | `			if( bAll && HtmlDocHasNamedTable(iDoc) ){` |
|     41 | 6419 | `				zEnt = FvHtml401Lookup(cp);` |
|     20 | 6420 | `			}` |
|     51 | 6421 | `			if( zEnt == 0 && (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode(cp,iFlags) ){` |
|    ! 0 | 6422 | `				zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 6423 | `			}` |
|      - | 6424 | `		}` |
|    363 | 6425 | `		if( zEnt ){` |
|    135 | 6426 | `			if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|    135 | 6427 | `			ph7_result_string(pCtx,zEnt,-1);` |
|    135 | 6428 | `			runStart = p + len;` |
|     67 | 6429 | `		}` |
|    363 | 6430 | `		p += len;` |
|      1 | 6431 | `	}` |
|     87 | 6432 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     49 | 6433 | `}` |
|      - | 6434 | `/* Shared decoder for html_entity_decode (bFull=1) and htmlspecialchars_decode` |
|      - | 6435 | ` * (bFull=0). Quote refs (cp 34/39, named or numeric) are gated by the quote` |
|      - | 6436 | ` * bits and left verbatim when suppressed; an invalid entity leaves its '&'` |
|      - | 6437 | ` * verbatim and rescans right after it, which also yields PHP's no-double-` |
|      - | 6438 | ` * decode behavior ("&amp;lt;" -> "&lt;"). */` |
|     84 | 6439 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 6440 | `                         int iFlags,int bFull){` |
|     85 | 6441 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     85 | 6442 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|     85 | 6443 | `	const unsigned char *runStart = p;` |
|     85 | 6444 | `	ph7_result_string(pCtx,"",0);` |
|    565 | 6445 | `	while( p < zEnd ){` |
|      - | 6446 | `		sxu32 cp;` |
|      - | 6447 | `		int nEat;` |
|    516 | 6448 | `		if( *p != '&' ){ p++; continue; }` |
|    155 | 6449 | `		if( !HtmlParseEntity(p,zEnd,iFlags,bFull,0,&cp,&nEat) ){ p++; continue; }` |
|    124 | 6450 | `		if( (cp == 34 && (iFlags & PH7_ENT_QUOTE_DOUBLE) == 0)` |
|    117 | 6451 | `		 \|\| (cp == 39 && (iFlags & PH7_ENT_QUOTE_SINGLE) == 0) ){` |
|      - | 6452 | `			/* Suppressed quote: leave the entity source verbatim. */` |
|     37 | 6453 | `			p += nEat;` |
|     37 | 6454 | `			continue;` |
|      - | 6455 | `		}` |
|     89 | 6456 | `		if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|      - | 6457 | `		{` |
|      - | 6458 | `			char zBuf[4];` |
|     89 | 6459 | `			int n = HtmlCpUtf8(cp,zBuf);` |
|     89 | 6460 | `			ph7_result_string(pCtx,zBuf,n);` |
|      - | 6461 | `		}` |
|     89 | 6462 | `		p += nEat;` |
|     89 | 6463 | `		runStart = p;` |
|      1 | 6464 | `	}` |
|     81 | 6465 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     81 | 6466 | `}` |
|      - | 6467 | `/* Validate the optional charset argument at apArg[idx]: UTF-8 aliases (and` |
|      - | 6468 | ` * ""/NULL meaning the default) are accepted; anything else — including` |
|      - | 6469 | ` * php-supported single-byte charsets like ISO-8859-1, PHL is UTF-8-only by` |
|      - | 6470 | ` * policy — raises PHP's unsupported-charset warning and is treated as` |
|      - | 6471 | ` * UTF-8 (ph7_context_throw_error_format prepends the function name). */` |
|    143 | 6472 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx){` |
|      - | 6473 | `	const char *zCs;` |
|      - | 6474 | `	int nCs;` |
|    150 | 6475 | `	if( nArg <= idx \|\| ph7_value_is_null(apArg[idx]) ){ return; }` |
|     15 | 6476 | `	zCs = ph7_value_to_string(apArg[idx],&nCs);` |
|     15 | 6477 | `	if( nCs == 0 ){ return; } /* "" selects the default charset (UTF-8) */` |
|     13 | 6478 | `	if( nCs == 5 && SyStrnicmp(zCs,"UTF-8",5) == 0 ){` |
|     13 | 6479 | `		return; /* php accepts only "UTF-8" (any case) silently — "UTF8" warns */` |
|      - | 6480 | `	}` |
|    ! 0 | 6481 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 6482 | `		"Charset \"%.*s\" is not supported, assuming UTF-8",nCs,zCs);` |
|     72 | 6483 | `}` |
|      - | 6484 | `/* get_html_translation_table() worker: character (UTF-8 bytes) => entity.` |
|      - | 6485 | ` * The five specials come first in byte order, then — for HTML_ENTITIES with a` |
|      - | 6486 | ` * named-table doctype — the 248 aHtml401Ent rows ascending (oracle-pinned` |
|      - | 6487 | ` * ordering; 253 entries under the defaults). */` |
|    549 | 6488 | `static void HtmlTableAdd(ph7_value *pArray,ph7_value *pValue,const char *zKey,const char *zEnt){` |
|    549 | 6489 | `	ph7_value_string(pValue,zEnt,-1);` |
|    549 | 6490 | `	ph7_array_add_strkey_elem(pArray,zKey,pValue);` |
|    549 | 6491 | `	ph7_value_reset_string_cursor(pValue);` |
|    549 | 6492 | `}` |
|     13 | 6493 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags){` |
|      - | 6494 | `	ph7_value *pArray,*pValue;` |
|     13 | 6495 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6496 | `	sxu32 n;` |
|     13 | 6497 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 6498 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 6499 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|    ! 0 | 6500 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6501 | `		return;` |
|      - | 6502 | `	}` |
|     13 | 6503 | `	if( iFlags & PH7_ENT_QUOTE_DOUBLE ){` |
|     11 | 6504 | `		HtmlTableAdd(pArray,pValue,"\"","&quot;");` |
|      5 | 6505 | `	}` |
|     13 | 6506 | `	HtmlTableAdd(pArray,pValue,"&","&amp;");` |
|     13 | 6507 | `	if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|      - | 6508 | `		/* The apostrophe row mirrors the function each table belongs to:` |
|      - | 6509 | `		 * SPECIALCHARS follows htmlspecialchars, ENTITIES follows` |
|      - | 6510 | `		 * htmlentities (oracle-pinned at flags 35). */` |
|      7 | 6511 | `		HtmlTableAdd(pArray,pValue,"'",HtmlAposEntity(iDoc,iTable != 0));` |
|      3 | 6512 | `	}` |
|     13 | 6513 | `	HtmlTableAdd(pArray,pValue,"<","&lt;");` |
|     13 | 6514 | `	HtmlTableAdd(pArray,pValue,">","&gt;");` |
|     13 | 6515 | `	if( iTable != 0 /*php: any non-HTML_SPECIALCHARS table => entities*/ && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 6516 | `		char zKey[8];` |
|    499 | 6517 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|    497 | 6518 | `			int nK = HtmlCpUtf8(aHtml401Ent[n].cp,zKey);` |
|    497 | 6519 | `			zKey[nK] = 0;` |
|    497 | 6520 | `			HtmlTableAdd(pArray,pValue,zKey,aHtml401Ent[n].zEnt);` |
|    249 | 6521 | `		}` |
|      1 | 6522 | `	}` |
|     13 | 6523 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 6524 | `}` |
|     25 | 6525 | `static int FvEmailAllowed(unsigned char c){` |
|     25 | 6526 | `	if( (c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9') ){ return 1; }` |
|     16 | 6527 | `	return c=='!'\|\|c=='#'\|\|c=='$'\|\|c=='%'\|\|c=='&'\|\|c=='\''\|\|c=='*'\|\|c=='+'` |
|     10 | 6528 | ``	    \|\| c=='-'\|\|c=='='\|\|c=='?'\|\|c=='^'\|\|c=='_'\|\|c=='`'\|\|c=='{'\|\|c=='\|'`` |
|     15 | 6529 | `	    \|\| c=='}'\|\|c=='~'\|\|c=='@'\|\|c=='.'\|\|c=='['\|\|c==']';` |
|     13 | 6530 | `}` |
|     23 | 6531 | `static int FvUrlAllowed(unsigned char c){` |
|     23 | 6532 | `	return c>=33 && c<=126; /* PHP keeps every printable ASCII byte except space */` |
|      1 | 6533 | `}` |
|      - | 6534 | `/* SANITIZE_EMAIL (isUrl=0) / SANITIZE_URL (isUrl=1): strip disallowed bytes. */` |
|      5 | 6535 | `static void FvSanitizeChars(ph7_context *pCtx,const char *z,int n,int isUrl){` |
|      5 | 6536 | `	int i, runStart = 0;` |
|      5 | 6537 | `	ph7_result_string(pCtx,"",0);` |
|     51 | 6538 | `	for( i=0; i<n; i++ ){` |
|     47 | 6539 | `		unsigned char c = (unsigned char)z[i];` |
|     47 | 6540 | `		if( !(isUrl ? FvUrlAllowed(c) : FvEmailAllowed(c)) ){` |
|     11 | 6541 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     11 | 6542 | `			runStart = i+1;` |
|      5 | 6543 | `		}` |
|     24 | 6544 | `	}` |
|      5 | 6545 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      5 | 6546 | `}` |
|      - | 6547 | `/*` |
|      - | 6548 | ` * Apply the selected filter to one already-resolved input value and write the` |
|      - | 6549 | ` * result into pCtx. Shared by filter_var() and filter_input(): the caller has` |
|      - | 6550 | ` * already parsed $filter/$flags/$options. On validation failure the 'default'` |
|      - | 6551 | ` * option (if any) is returned, else null when FILTER_NULL_ON_FAILURE is set,` |
|      - | 6552 | ` * else false. A validating filter that passes returns the (string) input` |
|      - | 6553 | ` * unchanged; a sanitizer writes its transformed output directly.` |
|      - | 6554 | ` */` |
|    316 | 6555 | `static int FvApplyFilter(ph7_context *pCtx,ph7_value *pInput,` |
|      - | 6556 | `                         int iFilter,int iFlags,ph7_value *pOpts,` |
|      - | 6557 | `                         ph7_value *pDefault)` |
|      3 | 6558 | `{` |
|    319 | 6559 | `	int bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|      - | 6560 | `	const char *zVal; int nVal;` |
|      - | 6561 | `	/* An array/object input fails every scalar filter. */` |
|    319 | 6562 | `	if( ph7_value_is_array(pInput) ){ goto fail; }` |
|    317 | 6563 | `	zVal = ph7_value_to_string(pInput,&nVal);` |
|    317 | 6564 | `	switch( iFilter ){` |
|     28 | 6565 | `	case FV_VALIDATE_INT: {` |
|      - | 6566 | `		ph7_int64 v;` |
|     58 | 6567 | `		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }` |
|     31 | 6568 | `		if( pOpts ){` |
|      7 | 6569 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|      7 | 6570 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|      7 | 6571 | `			if( pMin && v<ph7_value_to_int64(pMin) ){ goto fail; }` |
|      7 | 6572 | `			if( pMax && v>ph7_value_to_int64(pMax) ){ goto fail; }` |
|      2 | 6573 | `		}` |
|     29 | 6574 | `		ph7_result_int64(pCtx,v);` |
|     29 | 6575 | `		return PH7_OK;` |
|      - | 6576 | `	}` |
|     34 | 6577 | `	case FV_VALIDATE_FLOAT: {` |
|      - | 6578 | `		double d;` |
|     69 | 6579 | `		if( !FvValidateFloat(zVal,nVal,iFlags,&d) ){ goto fail; }` |
|     39 | 6580 | `		ph7_result_double(pCtx,d);` |
|     39 | 6581 | `		return PH7_OK;` |
|      - | 6582 | `	}` |
|     14 | 6583 | `	case FV_VALIDATE_BOOLEAN: {` |
|      - | 6584 | `		int b;` |
|     29 | 6585 | `		if( !FvValidateBool(zVal,nVal,&b) ){ goto fail; }` |
|     21 | 6586 | `		ph7_result_bool(pCtx,b);` |
|     21 | 6587 | `		return PH7_OK;` |
|      - | 6588 | `	}` |
|     25 | 6589 | `	case FV_VALIDATE_IP:     if( !FvValidateIp(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|     11 | 6590 | `	case FV_VALIDATE_MAC:    if( !FvValidateMac(zVal,nVal) ){ goto fail; }       goto pass;` |
|     28 | 6591 | `	case FV_VALIDATE_EMAIL:  if( !FvValidateEmail(zVal,nVal) ){ goto fail; }     goto pass;` |
|     11 | 6592 | `	case FV_VALIDATE_DOMAIN: if( !FvValidateDomain(zVal,nVal) ){ goto fail; }    goto pass;` |
|     15 | 6593 | `	case FV_VALIDATE_URL:    if( !FvValidateUrl(zVal,nVal) ){ goto fail; }       goto pass;` |
|      3 | 6594 | `	case FV_VALIDATE_REGEXP: {` |
|      - | 6595 | `#ifdef PH7_ENABLE_PCRE` |
|      8 | 6596 | `		ph7_value *pRe = pOpts ? ph7_array_fetch(pOpts,"regexp",(int)sizeof("regexp")-1) : 0;` |
|      8 | 6597 | `		const char *zRe; int nRe, matched = 0;` |
|      8 | 6598 | `		if( pRe==0 ){` |
|      3 | 6599 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6600 | `				"filter_var(): \"regexp\" option is missing");` |
|      - | 6601 | `		}` |
|      5 | 6602 | `		zRe = ph7_value_to_string(pRe,&nRe);` |
|      5 | 6603 | `		if( PH7_PcreMatchQuiet(pCtx,zRe,nRe,zVal,nVal,&matched)!=SXRET_OK \|\| !matched ){ goto fail; }` |
|      3 | 6604 | `		goto pass;` |
|      - | 6605 | `#else` |
|      - | 6606 | `		goto fail;` |
|      - | 6607 | `#endif` |
|      - | 6608 | `	}` |
|      3 | 6609 | `	case FV_SANITIZE_NUMBER_INT:   FvSanitizeNumber(pCtx,zVal,nVal,0,0);      return PH7_OK;` |
|      5 | 6610 | `	case FV_SANITIZE_NUMBER_FLOAT: FvSanitizeNumber(pCtx,zVal,nVal,1,iFlags); return PH7_OK;` |
|     13 | 6611 | `	case FV_SANITIZE_SPECIAL_CHARS:      FvSanitizeSpecial(pCtx,zVal,nVal,iFlags); return PH7_OK;` |
|     25 | 6612 | `	case FV_SANITIZE_FULL_SPECIAL_CHARS: FvSanitizeFull(pCtx,zVal,nVal,iFlags);    return PH7_OK;` |
|      3 | 6613 | `	case FV_SANITIZE_EMAIL: FvSanitizeChars(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 6614 | `	case FV_SANITIZE_URL:   FvSanitizeChars(pCtx,zVal,nVal,1); return PH7_OK;` |
|     13 | 6615 | `	case FV_DEFAULT:` |
|      - | 6616 | `		/* FILTER_UNSAFE_RAW / FILTER_DEFAULT: pass through unchanged unless a` |
|      - | 6617 | `		 * STRIP/ENCODE flag is set, in which case apply the string filter. */` |
|     28 | 6618 | `		if( iFlags & FV_FLAG_STRING_MASK ){` |
|     15 | 6619 | `			FvSanitizeString(pCtx,zVal,nVal,iFlags);` |
|     15 | 6620 | `			return PH7_OK;` |
|      - | 6621 | `		}` |
|     14 | 6622 | `		goto pass;` |
|    ! 0 | 6623 | `	default:` |
|    ! 0 | 6624 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 6625 | `			"Unknown filter with ID %d",iFilter);` |
|    ! 0 | 6626 | `		break; /* unknown filter id -> fail */` |
|    ! 0 | 6627 | `	}` |
|     58 | 6628 | `fail:` |
|    118 | 6629 | `	if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|    114 | 6630 | `	else if( bNull ){ ph7_result_null(pCtx); }` |
|    108 | 6631 | `	else { ph7_result_bool(pCtx,0); }` |
|    118 | 6632 | `	return PH7_OK;` |
|     26 | 6633 | `pass: /* validation passed: return the (string) input unchanged */` |
|     54 | 6634 | `	ph7_result_string(pCtx,zVal,nVal);` |
|     54 | 6635 | `	return PH7_OK;` |
|    161 | 6636 | `}` |
|      - | 6637 | `/*` |
|      - | 6638 | ` * Parse the ($filter, $options) pair shared by filter_var()/filter_input() out` |
|      - | 6639 | ` * of apArg[iBase] ($filter) and apArg[iBase+1] ($options): $options is either a` |
|      - | 6640 | ` * plain flags int, or an array with 'flags' and an 'options' sub-array (whose` |
|      - | 6641 | ` * 'default' entry is the fallback value). Fills the four output pointers;` |
|      - | 6642 | ` * unset outputs keep the caller-provided defaults.` |
|      - | 6643 | ` */` |
|    328 | 6644 | `static void FvParseFilterArgs(int nArg,ph7_value **apArg,int iBase,` |
|      - | 6645 | `                              int *piFilter,int *piFlags,` |
|      - | 6646 | `                              ph7_value **ppOpts,ph7_value **ppDefault)` |
|      3 | 6647 | `{` |
|    331 | 6648 | `	if( nArg>iBase ){ *piFilter = ph7_value_to_int(apArg[iBase]); }` |
|    331 | 6649 | `	if( nArg>iBase+1 ){` |
|     88 | 6650 | `		if( ph7_value_is_array(apArg[iBase+1]) ){` |
|     42 | 6651 | `			ph7_value *pF = ph7_array_fetch(apArg[iBase+1],"flags",(int)sizeof("flags")-1);` |
|     42 | 6652 | `			if( pF ){ *piFlags = ph7_value_to_int(pF); }` |
|     42 | 6653 | `			*ppOpts = ph7_array_fetch(apArg[iBase+1],"options",(int)sizeof("options")-1);` |
|     42 | 6654 | `			if( *ppOpts && !ph7_value_is_array(*ppOpts) ){ *ppOpts = 0; }` |
|     42 | 6655 | `			if( *ppOpts ){ *ppDefault = ph7_array_fetch(*ppOpts,"default",(int)sizeof("default")-1); }` |
|     22 | 6656 | `		}else{` |
|     48 | 6657 | `			*piFlags = ph7_value_to_int(apArg[iBase+1]);` |
|      - | 6658 | `		}` |
|     43 | 6659 | `	}` |
|    331 | 6660 | `}` |
|      - | 6661 | `/*` |
|      - | 6662 | ` * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 6663 | ` *  Validate or sanitize a value; see FvApplyFilter for the failure semantics.` |
|      - | 6664 | ` */` |
|    306 | 6665 | `static int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6666 | `{` |
|    308 | 6667 | `	int iFilter = FV_DEFAULT, iFlags = 0;` |
|    308 | 6668 | `	ph7_value *pOpts = 0, *pDefault = 0;` |
|    308 | 6669 | `	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    308 | 6670 | `	FvParseFilterArgs(nArg,apArg,1,&iFilter,&iFlags,&pOpts,&pDefault);` |
|    308 | 6671 | `	return FvApplyFilter(pCtx,apArg[0],iFilter,iFlags,pOpts,pDefault);` |
|    155 | 6672 | `}` |
|      - | 6673 | `/*` |
|      - | 6674 | ` * filter_input($type, $var_name, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 6675 | ` *  Look up $var_name in the requested INPUT_* superglobal, then apply the` |
|      - | 6676 | ` *  filter. Semantics verified byte-for-byte against php 8.5:` |
|      - | 6677 | ` *   - variable NOT set: 'default' option wins, else false when` |
|      - | 6678 | ` *     FILTER_NULL_ON_FAILURE is set, else null. (Note the null/false roles are` |
|      - | 6679 | ` *     INVERTED relative to a present value that fails validation, which yields` |
|      - | 6680 | ` *     default > null-if-NULL_ON_FAILURE > false via FvApplyFilter.)` |
|      - | 6681 | ` *   - variable present: delegate to FvApplyFilter.` |
|      - | 6682 | ` *  Divergence: php reads a SAPI snapshot of the original request variables` |
|      - | 6683 | ` *  captured at startup; PHL reads the live superglobal. In CLI they match for` |
|      - | 6684 | ` *  the SAPI-registered keys (SCRIPT_NAME/PHP_SELF/DOCUMENT_ROOT); keys added` |
|      - | 6685 | ` *  only to the live $_SERVER (REQUEST_TIME/PWD/…) are visible here but not in` |
|      - | 6686 | ` *  php's snapshot.` |
|      - | 6687 | ` */` |
|     24 | 6688 | `static int PH7_builtin_filter_input(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6689 | `{` |
|     26 | 6690 | `	int iType, iFilter = FV_DEFAULT, iFlags = 0;` |
|     26 | 6691 | `	ph7_value *pOpts = 0, *pDefault = 0, *pSuper, *pElem;` |
|      - | 6692 | `	const char *zVar, *zSuper; int nVar; sxu32 nSuper;` |
|     26 | 6693 | `	if( nArg<2 ){` |
|    ! 0 | 6694 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 6695 | `			"filter_input() expects at least 2 arguments, %d given",nArg);` |
|      - | 6696 | `	}` |
|     26 | 6697 | `	iType = ph7_value_to_int(apArg[0]);` |
|     26 | 6698 | `	switch( iType ){` |
|      3 | 6699 | `	case 0: zSuper = "_POST";   nSuper = (sxu32)sizeof("_POST")-1;   break; /* INPUT_POST */` |
|      3 | 6700 | `	case 1: zSuper = "_GET";    nSuper = (sxu32)sizeof("_GET")-1;    break; /* INPUT_GET */` |
|    ! 0 | 6701 | `	case 2: zSuper = "_COOKIE"; nSuper = (sxu32)sizeof("_COOKIE")-1; break; /* INPUT_COOKIE */` |
|    ! 0 | 6702 | `	case 4: zSuper = "_ENV";    nSuper = (sxu32)sizeof("_ENV")-1;    break; /* INPUT_ENV */` |
|     19 | 6703 | `	case 5: zSuper = "_SERVER"; nSuper = (sxu32)sizeof("_SERVER")-1; break; /* INPUT_SERVER */` |
|      1 | 6704 | `	default:` |
|      3 | 6705 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6706 | `			"filter_input(): Argument #1 ($type) must be an INPUT_* constant");` |
|      - | 6707 | `	}` |
|     23 | 6708 | `	zVar = ph7_value_to_string(apArg[1],&nVar);` |
|     23 | 6709 | `	FvParseFilterArgs(nArg,apArg,2,&iFilter,&iFlags,&pOpts,&pDefault);` |
|      - | 6710 | `	/* Resolve the variable from the superglobal (missing/non-array -> not set). */` |
|     23 | 6711 | `	pSuper = PH7_VmExtractSuper(pCtx->pVm,zSuper,nSuper);` |
|     23 | 6712 | `	pElem = (pSuper && ph7_value_is_array(pSuper))` |
|     33 | 6713 | `		? ph7_array_fetch(pSuper,zVar,nVar) : 0;` |
|     23 | 6714 | `	if( pElem==0 ){` |
|      - | 6715 | `		/* Variable not set: default > false(if NULL_ON_FAILURE) > null. Note the` |
|      - | 6716 | `		 * false/null roles are inverted vs FvApplyFilter's present-but-fails path. */` |
|     13 | 6717 | `		if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|      9 | 6718 | `		else if( iFlags & FV_NULL_ON_FAILURE ){ ph7_result_bool(pCtx,0); }` |
|      7 | 6719 | `		else { ph7_result_null(pCtx); }` |
|     13 | 6720 | `		return PH7_OK;` |
|      - | 6721 | `	}` |
|     11 | 6722 | `	return FvApplyFilter(pCtx,pElem,iFilter,iFlags,pOpts,pDefault);` |
|     14 | 6723 | `}` |
|      - | 6724 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6725 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6726 | `/*` |
|      - | 6727 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 6728 |  |
|      - | 6729 | ` */` |
|      4 | 6730 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 6731 | `	const char *zInput, /* Raw input */` |
|      - | 6732 | `	int nByte,  /* Input length */` |
|      - | 6733 | `	int delim,  /* Delimiter */` |
|      - | 6734 | `	int encl,   /* Enclosure */` |
|      - | 6735 | `	int escape,  /* Escape character */` |
|      - | 6736 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 6737 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 6738 | `	)` |
|      1 | 6739 | `{` |
|      5 | 6740 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 6741 | `	const char *zIn = zInput;` |
|      - | 6742 | `	const char *zPtr;` |
|      - | 6743 | `	int isEnc;` |
|      - | 6744 | `	/* Start processing */` |
|      8 | 6745 | `	for(;;){` |
|     17 | 6746 | `		if( zIn >= zEnd ){` |
|      - | 6747 | `			/* No more input to process */` |
|      5 | 6748 | `			break;` |
|      - | 6749 | `		}` |
|     13 | 6750 | `		isEnc = 0;` |
|     13 | 6751 | `		zPtr = zIn;` |
|      - | 6752 | `		/* Find the first delimiter */` |
|     27 | 6753 | `		while( zIn < zEnd ){` |
|     23 | 6754 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 6755 | `				/* Delimiter found,break imediately */` |
|      5 | 6756 | `				break;` |
|     15 | 6757 | `			}else if( zIn[0] == encl ){` |
|      - | 6758 | `				/* Inside enclosure? */` |
|    ! 0 | 6759 | `				isEnc = !isEnc;` |
|     15 | 6760 | `			}else if( zIn[0] == escape ){` |
|      - | 6761 | `				/* Escape sequence */` |
|    ! 0 | 6762 | `				zIn++;` |
|    ! 0 | 6763 | `			}` |
|      - | 6764 | `			/* Advance the cursor */` |
|     15 | 6765 | `			zIn++;` |
|      1 | 6766 | `		}` |
|     13 | 6767 | `		if( zIn > zPtr ){` |
|     13 | 6768 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 6769 | `			sxi32 rc;` |
|      - | 6770 | `			/* Invoke the supllied callback */` |
|     13 | 6771 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 6772 | `				zPtr++;` |
|    ! 0 | 6773 | `				nByteChunk-=2;` |
|    ! 0 | 6774 | `			}` |
|     13 | 6775 | `			if( nByteChunk > 0 ){` |
|     13 | 6776 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 6777 | `				if( rc == SXERR_ABORT ){` |
|      - | 6778 | `					/* User callback request an operation abort */` |
|    ! 0 | 6779 | `					break;` |
|      - | 6780 | `				}` |
|      6 | 6781 | `			}` |
|      6 | 6782 | `		}` |
|      - | 6783 | `		/* Ignore trailing delimiter */` |
|     21 | 6784 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 6785 | `			zIn++;` |
|      1 | 6786 | `		}` |
|      1 | 6787 | `	}` |
|      5 | 6788 | `	return SXRET_OK;` |
|      1 | 6789 | `}` |
|      - | 6790 | `/*` |
|      - | 6791 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 6792 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 6793 | ` * argument to this callback.` |
|      - | 6794 | ` */` |
|     12 | 6795 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 6796 | `{` |
|     13 | 6797 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 6798 | `	ph7_value sEntry;` |
|      - | 6799 | `	SyString sToken;` |
|      - | 6800 | `	/* Insert the token in the given array */` |
|     13 | 6801 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 6802 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 6803 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 6804 | `	if( sToken.nByte < 1){` |
|    ! 0 | 6805 | `		return SXRET_OK;` |
|      - | 6806 | `	}` |
|     13 | 6807 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 6808 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 6809 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 6810 | `	return SXRET_OK;` |
|      7 | 6811 | `}` |
|      - | 6812 | `/*` |
|      - | 6813 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 6814 | ` *  Parse a CSV string into an array.` |
|      - | 6815 | ` * Parameters` |
|      - | 6816 | ` *  $input` |
|      - | 6817 | ` *   The string to parse.` |
|      - | 6818 | ` *  $delimiter` |
|      - | 6819 | ` *   Set the field delimiter (one character only).` |
|      - | 6820 | ` *  $enclosure` |
|      - | 6821 | ` *   Set the field enclosure character (one character only).` |
|      - | 6822 | ` *  $escape` |
|      - | 6823 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 6824 | ` * Return` |
|      - | 6825 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 6826 | ` */` |
|      2 | 6827 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6828 | `{` |
|      - | 6829 | `	const char *zInput,*zPtr;` |
|      - | 6830 | `	ph7_value *pArray;` |
|      3 | 6831 | `	int delim  = ',';   /* Delimiter */` |
|      3 | 6832 | `	int encl   = '"' ;  /* Enclosure */` |
|      3 | 6833 | `	int escape = '\\';  /* Escape character */` |
|      - | 6834 | `	int nLen;` |
|      3 | 6835 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6836 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 6837 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6838 | `		return PH7_OK;` |
|      - | 6839 | `	}` |
|      - | 6840 | `	/* Extract the raw input */` |
|      3 | 6841 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 6842 | `	if( nArg > 1 ){` |
|      - | 6843 | `		int i;` |
|      3 | 6844 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 6845 | `			/* Extract the delimiter */` |
|      3 | 6846 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 6847 | `			if( i > 0 ){` |
|      3 | 6848 | `				delim = zPtr[0];` |
|      1 | 6849 | `			}` |
|      1 | 6850 | `		}` |
|      3 | 6851 | `		if( nArg > 2 ){` |
|      3 | 6852 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 6853 | `				/* Extract the enclosure */` |
|      3 | 6854 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 6855 | `				if( i > 0 ){` |
|      3 | 6856 | `					encl = zPtr[0];` |
|      1 | 6857 | `				}` |
|      1 | 6858 | `			}` |
|      3 | 6859 | `			if( nArg > 3 ){` |
|      3 | 6860 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 6861 | `					/* Extract the escape character */` |
|      3 | 6862 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 6863 | `					if( i > 0 ){` |
|      3 | 6864 | `						escape = zPtr[0];` |
|      1 | 6865 | `					}` |
|      1 | 6866 | `				}` |
|      1 | 6867 | `			}` |
|      1 | 6868 | `		}` |
|      1 | 6869 | `	}` |
|      - | 6870 | `	/* Create our array */` |
|      3 | 6871 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 6872 | `	if( pArray == 0 ){` |
|      - | 6873 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|    ! 0 | 6874 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6875 | `	}` |
|      - | 6876 | `	/* Parse the raw input */` |
|      3 | 6877 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 6878 | `	/* Return the freshly created array */` |
|      3 | 6879 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 6880 | `	return PH7_OK;` |
|      2 | 6881 | `}` |
|      - | 6882 | `/*` |
|      - | 6883 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 6884 | ` * container.` |
|      - | 6885 | ` * Refer to [strip_tags()].` |
|      - | 6886 | ` */` |
|     10 | 6887 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 6888 | `{` |
|     11 | 6889 | `	const char *zEnd = &zTag[nByte];` |
|      - | 6890 | `	const char *zPtr;` |
|      - | 6891 | `	SyString sEntry;` |
|      - | 6892 | `	/* Strip tags */` |
|     10 | 6893 | `	for(;;){` |
|     45 | 6894 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 6895 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 6896 | `				zTag++;` |
|      1 | 6897 | `		}` |
|     21 | 6898 | `		if( zTag >= zEnd ){` |
|     11 | 6899 | `			break;` |
|      - | 6900 | `		}` |
|     11 | 6901 | `		zPtr = zTag;` |
|      - | 6902 | `		/* Delimit the tag */` |
|     25 | 6903 | `		while(zTag < zEnd ){` |
|     25 | 6904 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 6905 | `				/* UTF-8 stream */` |
|      3 | 6906 | `				zTag++;` |
|      5 | 6907 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 6908 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 6909 | `				break;` |
|    ! 0 | 6910 | `			}else{` |
|     13 | 6911 | `				zTag++;` |
|      - | 6912 | `			}` |
|      1 | 6913 | `		}` |
|     11 | 6914 | `		if( zTag > zPtr ){` |
|      - | 6915 | `			/* Perform the insertion */` |
|     11 | 6916 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 6917 | `			SyStringFullTrim(&sEntry);` |
|     11 | 6918 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 6919 | `		}` |
|      - | 6920 | `		/* Jump the trailing '>' */` |
|     11 | 6921 | `		zTag++;` |
|      1 | 6922 | `	}` |
|     11 | 6923 | `	return SXRET_OK;` |
|      1 | 6924 | `}` |
|      - | 6925 | `/*` |
|      - | 6926 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 6927 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 6928 | ` * Refer to [strip_tags()].` |
|      - | 6929 | ` */` |
|     36 | 6930 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 6931 | `{` |
|     37 | 6932 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 6933 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 6934 | `		SyString sTag;` |
|     85 | 6935 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 6936 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 6937 | `			zTag++;` |
|      1 | 6938 | `		}` |
|      - | 6939 | `		/* Delimit the tag */` |
|     25 | 6940 | `		zCur = zTag;` |
|     77 | 6941 | `		while(zTag < zEnd ){` |
|     77 | 6942 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 6943 | `				/* UTF-8 stream */` |
|      5 | 6944 | `				zTag++;` |
|      9 | 6945 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 6946 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 6947 | `				break;` |
|    ! 0 | 6948 | `			}else{` |
|     49 | 6949 | `				zTag++;` |
|      - | 6950 | `			}` |
|      1 | 6951 | `		}` |
|     25 | 6952 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 6953 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 6954 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 6955 | `		if( sTag.nByte > 0 ){` |
|      - | 6956 | `			SyString *aEntry,*pEntry;` |
|      - | 6957 | `			sxi32 rc;` |
|      - | 6958 | `			sxu32 n;` |
|      - | 6959 | `			/* Perform the lookup */` |
|     25 | 6960 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 6961 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 6962 | `				pEntry = &aEntry[n];` |
|      - | 6963 | `				/* Do the comparison */` |
|     25 | 6964 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 6965 | `				if( !rc ){` |
|     21 | 6966 | `					return SXRET_OK;` |
|      - | 6967 | `				}` |
|      3 | 6968 | `			}` |
|      2 | 6969 | `		}` |
|      2 | 6970 | `	}` |
|      - | 6971 | `	/* No such tag */` |
|     17 | 6972 | `	return SXERR_NOTFOUND;` |
|     19 | 6973 | `}` |
|      - | 6974 | `/*` |
|      - | 6975 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 6976 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 6977 | ` * Refer to [strip_tags()].` |
|      - | 6978 | ` */` |
|     16 | 6979 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 6980 | `{` |
|     17 | 6981 | `	const char *zEnd = &zIn[nByte];` |
|      - | 6982 | `	const char *zPtr,*zTag;` |
|      - | 6983 | `	SySet sSet;` |
|      - | 6984 | `	/* initialize the set of allowed tags */` |
|     17 | 6985 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 6986 | `	if( nTaglen > 0 ){` |
|      - | 6987 | `		/* Set of allowed tags */` |
|     11 | 6988 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 6989 | `	}` |
|      - | 6990 | `	/* Set the empty string */` |
|     17 | 6991 | `	ph7_result_string(pCtx,"",0);` |
|      - | 6992 | `	/* Start processing */` |
|     26 | 6993 | `	for(;;){` |
|     53 | 6994 | `		if(zIn >= zEnd){` |
|      - | 6995 | `			/* No more input to process */` |
|     15 | 6996 | `			break;` |
|      - | 6997 | `		}` |
|     39 | 6998 | `		zPtr = zIn;` |
|      - | 6999 | `		/* Find a tag */` |
|    133 | 7000 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 7001 | `			zIn++;` |
|      1 | 7002 | `		}` |
|     39 | 7003 | `		if( zIn > zPtr ){` |
|      - | 7004 | `			/* Consume raw input */` |
|     21 | 7005 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 7006 | `		}` |
|      - | 7007 | `		/* Ignore trailing null bytes */` |
|     39 | 7008 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 7009 | `			zIn++;` |
|    ! 0 | 7010 | `		}` |
|     39 | 7011 | `		if(zIn >= zEnd){` |
|      - | 7012 | `			/* No more input to process */` |
|      3 | 7013 | `			break;` |
|      - | 7014 | `		}` |
|     37 | 7015 | `		if( zIn[0] == '<' ){` |
|      - | 7016 | `			sxi32 rc;` |
|     37 | 7017 | `			zTag = zIn++;` |
|      - | 7018 | `			/* Delimit the tag */` |
|    127 | 7019 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 7020 | `				zIn++;` |
|      1 | 7021 | `			}` |
|     37 | 7022 | `			if( zIn < zEnd ){` |
|     37 | 7023 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 7024 | `			}` |
|      - | 7025 | `			/* Query the set */` |
|     37 | 7026 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 7027 | `			if( rc == SXRET_OK ){` |
|      - | 7028 | `				/* Keep the tag */` |
|     21 | 7029 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 7030 | `			}` |
|     18 | 7031 | `		}` |
|      1 | 7032 | `	}` |
|      - | 7033 | `	/* Cleanup */` |
|     17 | 7034 | `	SySetRelease(&sSet);` |
|     17 | 7035 | `	return SXRET_OK;` |
|      1 | 7036 | `}` |
|      - | 7037 | `/*` |
|      - | 7038 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 7039 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 7040 | ` * Parameters` |
|      - | 7041 | ` *  $str` |
|      - | 7042 | ` *  The input string.` |
|      - | 7043 | ` * $allowable_tags` |
|      - | 7044 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 7045 | ` * Return` |
|      - | 7046 | ` *  Returns the stripped string.` |
|      - | 7047 | ` */` |
|     14 | 7048 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7049 | `{` |
|     15 | 7050 | `	const char *zTaglist = 0;` |
|      - | 7051 | `	const char *zString;` |
|     15 | 7052 | `	int nTaglen = 0;` |
|      - | 7053 | `	int nLen;` |
|     15 | 7054 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7055 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 7056 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7057 | `		return PH7_OK;` |
|      - | 7058 | `	}` |
|      - | 7059 | `	/* Point to the raw string */` |
|     15 | 7060 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 7061 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 7062 | `		/* Allowed tag */` |
|     11 | 7063 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 7064 | `	}` |
|      - | 7065 | `	/* Process input */` |
|     15 | 7066 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 7067 | `	return PH7_OK;` |
|      8 | 7068 | `}` |
|      - | 7069 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7070 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7071 | `/*` |
|      - | 7072 | ` * string str_shuffle(string $str)` |
|      - | 7073 |  |
|      - | 7074 | ` *  Randomly shuffles a string.` |
|      - | 7075 | ` * Parameters` |
|      - | 7076 | ` *  $str` |
|      - | 7077 | ` *   The input string.` |
|      - | 7078 | ` * Return` |
|      - | 7079 | ` *  Returns the shuffled string.` |
|      - | 7080 | ` */` |
|     10 | 7081 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7082 | `{` |
|      - | 7083 | `	const char *zString;` |
|      - | 7084 | `	int nLen,i,c;` |
|      - | 7085 | `	sxu32 iR;` |
|     11 | 7086 | `	if( nArg < 1 ){` |
|      - | 7087 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7088 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7089 | `		return PH7_OK;` |
|      - | 7090 | `	}` |
|      - | 7091 | `	/* Extract the target string */` |
|     11 | 7092 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 7093 | `	if( nLen < 1 ){` |
|      - | 7094 | `		/* Nothing to shuffle */` |
|      3 | 7095 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 7096 | `		return PH7_OK;` |
|      - | 7097 | `	}` |
|      - | 7098 | `	/* Shuffle the string */` |
|     43 | 7099 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 7100 | `		/* Generate a random number first */` |
|     35 | 7101 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 7102 | `		/* Extract a random offset */` |
|     35 | 7103 | `		c = zString[iR % nLen];` |
|      - | 7104 | `		/* Append it */` |
|     35 | 7105 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 7106 | `	}` |
|      9 | 7107 | `	return PH7_OK;` |
|      6 | 7108 | `}` |
|      - | 7109 | `/*` |
|      - | 7110 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 7111 | ` *  Convert a string to an array.` |
|      - | 7112 | ` * Parameters` |
|      - | 7113 | ` * $string` |
|      - | 7114 | ` *  The input string.` |
|      - | 7115 | ` * $split_length` |
|      - | 7116 | ` *  Maximum length of the chunk.` |
|      - | 7117 | ` * Return` |
|      - | 7118 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 7119 | ` *  except possibly the last one which may be shorter.` |
|      - | 7120 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 7121 | ` *  as the first (and only) array element.` |
|      - | 7122 | ` *  An empty string returns an empty array.` |
|      - | 7123 | ` * Errors` |
|      - | 7124 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 7125 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 7126 | ` *  ValueError if $split_length is less than 1.` |
|      - | 7127 | ` */` |
|     24 | 7128 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 7129 | `{` |
|      - | 7130 | `	const char *zString,*zEnd;` |
|      - | 7131 | `	ph7_value *pArray,*pValue;` |
|      - | 7132 | `	int split_len;` |
|      - | 7133 | `	int nLen;` |
|     27 | 7134 | `	if( nArg < 1 ){` |
|    ! 0 | 7135 | `		return PH7_VmThrowException(pCtx,` |
|      - | 7136 | `			"ArgumentCountError",` |
|      - | 7137 | `			"str_split() expects at least 1 argument, %d given",` |
|    ! 0 | 7138 | `			nArg` |
|      - | 7139 | `			);` |
|      - | 7140 | `	}` |
|      - | 7141 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     36 | 7142 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     39 | 7143 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 7144 | `	    ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 7145 | `		return PH7_VmThrowException(pCtx,` |
|      - | 7146 | `			"TypeError",` |
|      - | 7147 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 7148 | `			ph7_type_name(apArg[0])` |
|      - | 7149 | `			);` |
|      - | 7150 | `	}` |
|      - | 7151 | `	/* Point to the target string */` |
|     27 | 7152 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 7153 | `	split_len = (int)sizeof(char);` |
|     27 | 7154 | `	if( nArg > 1 ){` |
|      - | 7155 | `		/* Split length */` |
|     17 | 7156 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 7157 | `		if( split_len < 1 ){` |
|      6 | 7158 | `			return PH7_VmThrowException(pCtx,` |
|      - | 7159 | `				"ValueError",` |
|      - | 7160 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 7161 | `				);` |
|      - | 7162 | `		}` |
|     11 | 7163 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 7164 | `			split_len = nLen;` |
|      1 | 7165 | `		}` |
|      5 | 7166 | `	}` |
|      - | 7167 | `	/* Create the array and the scalar value */` |
|     21 | 7168 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 7169 | `	/*Chunk value */` |
|     21 | 7170 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 7171 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 7172 | `		/* Return FALSE */` |
|    ! 0 | 7173 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7174 | `		return PH7_OK;` |
|      - | 7175 | `	}` |
|      - | 7176 | `	/* Point to the end of the string */` |
|     21 | 7177 | `	zEnd = &zString[nLen];` |
|      - | 7178 | `	/* Perform the requested operation */` |
|     48 | 7179 | `	for(;;){` |
|      - | 7180 | `		int nMax;` |
|     59 | 7181 | `		if( zString >= zEnd ){` |
|      - | 7182 | `			/* No more input to process */` |
|     21 | 7183 | `			break;` |
|      - | 7184 | `		}` |
|     39 | 7185 | `		nMax = (int)(zEnd-zString);` |
|     39 | 7186 | `		if( nMax < split_len ){` |
|      3 | 7187 | `			split_len = nMax;` |
|      1 | 7188 | `		}` |
|      - | 7189 | `		/* Copy the current chunk */` |
|     39 | 7190 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 7191 | `		/* Insert it */` |
|     39 | 7192 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|    ! 0 | 7193 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 7194 | `		}` |
|      - | 7195 | `		/* reset the string cursor */` |
|     39 | 7196 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 7197 | `		/* Update position */` |
|     39 | 7198 | `		zString += split_len;` |
|      1 | 7199 | `	}` |
|      - | 7200 | `	/*` |
|      - | 7201 | `	 * Return the array.` |
|      - | 7202 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 7203 | `	 * upon we return from this function.` |
|      - | 7204 | `	 */` |
|     21 | 7205 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 7206 | `	return PH7_OK;` |
|     15 | 7207 | `}` |
|      - | 7208 | `/*` |
|      - | 7209 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 7210 | ` * Refer to [strspn()].` |
|      - | 7211 | ` */` |
|     28 | 7212 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 7213 | `{` |
|     29 | 7214 | `	const char *zIn = *pzIn;` |
|      - | 7215 | `	const char *zPtr;` |
|      - | 7216 | `	/* Ignore leading white spaces */` |
|     29 | 7217 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 7218 | `		zIn++;` |
|    ! 0 | 7219 | `	}` |
|     29 | 7220 | `	if( zIn >= zEnd ){` |
|      - | 7221 | `		/* End of input */` |
|    ! 0 | 7222 | `		return SXERR_EOF;` |
|      - | 7223 | `	}` |
|     29 | 7224 | `	zPtr = zIn;` |
|      - | 7225 | `	/* Extract the token */` |
|    201 | 7226 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 7227 | `		zIn++;` |
|      1 | 7228 | `	}` |
|     29 | 7229 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 7230 | `	/* Synchronize pointers */` |
|     29 | 7231 | `	*pzIn = zIn;` |
|      - | 7232 | `	/* Return to the caller */` |
|     29 | 7233 | `	return SXRET_OK;` |
|     15 | 7234 | `}` |
|      - | 7235 | `/*` |
|      - | 7236 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 7237 | ` * return the longest match.` |
|      - | 7238 | ` * Refer to [strspn()].` |
|      - | 7239 | ` */` |
|     18 | 7240 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 7241 | `{` |
|     19 | 7242 | `	const char *zEnd = &zString[nLen];` |
|     19 | 7243 | `	const char *zIn = zString;` |
|      - | 7244 | `	int i,c;` |
|     45 | 7245 | `	for(;;){` |
|     91 | 7246 | `		if( zString >= zEnd ){` |
|      7 | 7247 | `			break;` |
|      - | 7248 | `		}` |
|      - | 7249 | `		/* Extract current character */` |
|     85 | 7250 | `		c = zString[0];` |
|      - | 7251 | `		/* Perform the lookup */` |
|    383 | 7252 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 7253 | `			if( c == zMask[i] ){` |
|      - | 7254 | `				/* Character found */` |
|     73 | 7255 | `				break;` |
|      - | 7256 | `			}` |
|    150 | 7257 | `		}` |
|     85 | 7258 | `		if( i >= nMaskLen ){` |
|      - | 7259 | `			/* Character not in the current mask,break immediately */` |
|     13 | 7260 | `			break;` |
|      - | 7261 | `		}` |
|      - | 7262 | `		/* Advance cursor */` |
|     73 | 7263 | `		zString++;` |
|      1 | 7264 | `	}` |
|      - | 7265 | `	/* Longest match */` |
|     19 | 7266 | `	return (int)(zString-zIn);` |
|      1 | 7267 | `}` |
|      - | 7268 | `/*` |
|      - | 7269 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 7270 | ` * Refer to [strcspn()].` |
|      - | 7271 | ` */` |
|     10 | 7272 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 7273 | `{` |
|     11 | 7274 | `	const char *zEnd = &zString[nLen];` |
|     11 | 7275 | `	const char *zIn = zString;` |
|      - | 7276 | `	int i,c;` |
|     12 | 7277 | `	for(;;){` |
|     25 | 7278 | `		if( zString >= zEnd ){` |
|      3 | 7279 | `			break;` |
|      - | 7280 | `		}` |
|      - | 7281 | `		/* Extract current character */` |
|     23 | 7282 | `		c = zString[0];` |
|      - | 7283 | `		/* Perform the lookup */` |
|     51 | 7284 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 7285 | `			if( c == zMask[i] ){` |
|      9 | 7286 | `				break;` |
|      - | 7287 | `			}` |
|     15 | 7288 | `		}` |
|     23 | 7289 | `		if( i < nMaskLen ){` |
|      - | 7290 | `			/* Character in the current mask,break immediately */` |
|      9 | 7291 | `			break;` |
|      - | 7292 | `		}` |
|      - | 7293 | `		/* Advance cursor */` |
|     15 | 7294 | `		zString++;` |
|      1 | 7295 | `	}` |
|      - | 7296 | `	/* Longest match */` |
|     11 | 7297 | `	return (int)(zString-zIn);` |
|      1 | 7298 | `}` |
|      - | 7299 | `/*` |
|      - | 7300 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 7301 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 7302 | ` *  of characters contained within a given mask.` |
|      - | 7303 | ` * Parameters` |
|      - | 7304 | ` * $str` |
|      - | 7305 | ` *  The input string.` |
|      - | 7306 | ` * $mask` |
|      - | 7307 | ` *  The list of allowable characters.` |
|      - | 7308 | ` * $start` |
|      - | 7309 | ` *  The position in subject to start searching.` |
|      - | 7310 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 7311 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 7312 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 7313 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 7314 | ` *  start'th position from the end of subject.` |
|      - | 7315 | ` * $length` |
|      - | 7316 | ` *  The length of the segment from subject to examine.` |
|      - | 7317 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 7318 | ` *  characters after the starting position.` |
|      - | 7319 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 7320 | ` *  position up to length characters from the end of subject.` |
|      - | 7321 | ` * Return` |
|      - | 7322 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 7323 | ` * in mask.` |
|      - | 7324 | ` */` |
|     24 | 7325 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7326 | `{` |
|      - | 7327 | `	const char *zString,*zMask,*zEnd;` |
|      - | 7328 | `	int iMasklen,iLen;` |
|      - | 7329 | `	SyString sToken;` |
|     25 | 7330 | `	int iCount = 0;` |
|      - | 7331 | `	int rc;` |
|     25 | 7332 | `	if( nArg < 2 ){` |
|      - | 7333 | `		/* Missing agruments,return zero */` |
|    ! 0 | 7334 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7335 | `		return PH7_OK;` |
|      - | 7336 | `	}` |
|      - | 7337 | `	/* Extract the target string */` |
|     25 | 7338 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7339 | `	/* Extract the mask */` |
|     25 | 7340 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 7341 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 7342 | `		/* Nothing to process,return zero */` |
|      7 | 7343 | `		ph7_result_int(pCtx,0);` |
|      7 | 7344 | `		return PH7_OK;` |
|      - | 7345 | `	}` |
|     19 | 7346 | `	if( nArg > 2 ){` |
|      - | 7347 | `		int nOfft;` |
|      - | 7348 | `		/* Extract the offset */` |
|      9 | 7349 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 7350 | `		if( nOfft < 0 ){` |
|    ! 0 | 7351 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 7352 | `			if( zBase > zString ){` |
|    ! 0 | 7353 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 7354 | `				zString = zBase;` |
|    ! 0 | 7355 | `			}else{` |
|      - | 7356 | `				/* Invalid offset */` |
|    ! 0 | 7357 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7358 | `				return PH7_OK;` |
|      - | 7359 | `			}` |
|    ! 0 | 7360 | `		}else{` |
|      9 | 7361 | `			if( nOfft >= iLen ){` |
|      - | 7362 | `				/* Invalid offset */` |
|    ! 0 | 7363 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7364 | `				return PH7_OK;` |
|    ! 0 | 7365 | `			}else{` |
|      - | 7366 | `				/* Update offset */` |
|      9 | 7367 | `				zString += nOfft;` |
|      9 | 7368 | `				iLen -= nOfft;` |
|      - | 7369 | `			}` |
|      - | 7370 | `		}` |
|      9 | 7371 | `		if( nArg > 3 ){` |
|      - | 7372 | `			int iUserlen;` |
|      - | 7373 | `			/* Extract the desired length */` |
|      9 | 7374 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 7375 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 7376 | `				iLen = iUserlen;` |
|      2 | 7377 | `			}` |
|      4 | 7378 | `		}` |
|      4 | 7379 | `	}` |
|      - | 7380 | `	/* Point to the end of the string */` |
|     19 | 7381 | `	zEnd = &zString[iLen];` |
|      - | 7382 | `	/* Extract the first non-space token */` |
|     19 | 7383 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 7384 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 7385 | `		/* Compare against the current mask */` |
|     19 | 7386 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 7387 | `	}` |
|      - | 7388 | `	/* Longest match */` |
|     19 | 7389 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 7390 | `	return PH7_OK;` |
|     13 | 7391 | `}` |
|      - | 7392 | `/*` |
|      - | 7393 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 7394 | ` *  Find length of initial segment not matching mask.` |
|      - | 7395 | ` * Parameters` |
|      - | 7396 | ` * $str` |
|      - | 7397 | ` *  The input string.` |
|      - | 7398 | ` * $mask` |
|      - | 7399 | ` *  The list of not allowed characters.` |
|      - | 7400 | ` * $start` |
|      - | 7401 | ` *  The position in subject to start searching.` |
|      - | 7402 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 7403 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 7404 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 7405 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 7406 | ` *  start'th position from the end of subject.` |
|      - | 7407 | ` * $length` |
|      - | 7408 | ` *  The length of the segment from subject to examine.` |
|      - | 7409 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 7410 | ` *  characters after the starting position.` |
|      - | 7411 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 7412 | ` *  position up to length characters from the end of subject.` |
|      - | 7413 | ` * Return` |
|      - | 7414 | ` *  Returns the length of the segment as an integer.` |
|      - | 7415 | ` */` |
|     14 | 7416 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7417 | `{` |
|      - | 7418 | `	const char *zString,*zMask,*zEnd;` |
|      - | 7419 | `	int iMasklen,iLen;` |
|      - | 7420 | `	SyString sToken;` |
|     15 | 7421 | `	int iCount = 0;` |
|      - | 7422 | `	int rc;` |
|     15 | 7423 | `	if( nArg < 2 ){` |
|      - | 7424 | `		/* Missing agruments,return zero */` |
|    ! 0 | 7425 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7426 | `		return PH7_OK;` |
|      - | 7427 | `	}` |
|      - | 7428 | `	/* Extract the target string */` |
|     15 | 7429 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7430 | `	/* Extract the mask */` |
|     15 | 7431 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 7432 | `	if( iLen < 1 ){` |
|      - | 7433 | `		/* Nothing to process,return zero */` |
|    ! 0 | 7434 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7435 | `		return PH7_OK;` |
|      - | 7436 | `	}` |
|     15 | 7437 | `	if( iMasklen < 1 ){` |
|      - | 7438 | `		/* No given mask,return the string length */` |
|      3 | 7439 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 7440 | `		return PH7_OK;` |
|      - | 7441 | `	}` |
|     13 | 7442 | `	if( nArg > 2 ){` |
|      - | 7443 | `		int nOfft;` |
|      - | 7444 | `		/* Extract the offset */` |
|     11 | 7445 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 7446 | `		if( nOfft < 0 ){` |
|    ! 0 | 7447 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 7448 | `			if( zBase > zString ){` |
|    ! 0 | 7449 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 7450 | `				zString = zBase;` |
|    ! 0 | 7451 | `			}else{` |
|      - | 7452 | `				/* Invalid offset */` |
|    ! 0 | 7453 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7454 | `				return PH7_OK;` |
|      - | 7455 | `			}` |
|    ! 0 | 7456 | `		}else{` |
|     11 | 7457 | `			if( nOfft >= iLen ){` |
|      - | 7458 | `				/* Invalid offset */` |
|      3 | 7459 | `				ph7_result_int(pCtx,0);` |
|      3 | 7460 | `				return PH7_OK;` |
|    ! 0 | 7461 | `			}else{` |
|      - | 7462 | `				/* Update offset */` |
|      9 | 7463 | `				zString += nOfft;` |
|      9 | 7464 | `				iLen -= nOfft;` |
|      - | 7465 | `			}` |
|      - | 7466 | `		}` |
|      9 | 7467 | `		if( nArg > 3 ){` |
|      - | 7468 | `			int iUserlen;` |
|      - | 7469 | `			/* Extract the desired length */` |
|    ! 0 | 7470 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 7471 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 7472 | `				iLen = iUserlen;` |
|    ! 0 | 7473 | `			}` |
|    ! 0 | 7474 | `		}` |
|      4 | 7475 | `	}` |
|      - | 7476 | `	/* Point to the end of the string */` |
|     11 | 7477 | `	zEnd = &zString[iLen];` |
|      - | 7478 | `	/* Extract the first non-space token */` |
|     11 | 7479 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 7480 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 7481 | `		/* Compare against the current mask */` |
|     11 | 7482 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 7483 | `	}` |
|      - | 7484 | `	/* Longest match */` |
|     11 | 7485 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 7486 | `	return PH7_OK;` |
|      8 | 7487 | `}` |
|      - | 7488 | `/*` |
|      - | 7489 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 7490 | ` *  Search a string for any of a set of characters.` |
|      - | 7491 | ` * Parameters` |
|      - | 7492 | ` *  $haystack` |
|      - | 7493 | ` *   The string where char_list is looked for.` |
|      - | 7494 | ` *  $char_list` |
|      - | 7495 | ` *   This parameter is case sensitive.` |
|      - | 7496 | ` * Return` |
|      - | 7497 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 7498 | ` */` |
|      4 | 7499 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7500 | `{` |
|      - | 7501 | `	const char *zString,*zList,*zEnd;` |
|      - | 7502 | `	int iLen,iListLen,i,c;` |
|      - | 7503 | `	sxu32 nOfft,nMax;` |
|      - | 7504 | `	sxi32 rc;` |
|      5 | 7505 | `	if( nArg < 2 ){` |
|      - | 7506 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7507 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7508 | `		return PH7_OK;` |
|      - | 7509 | `	}` |
|      - | 7510 | `	/* Extract the haystack and the char list */` |
|      5 | 7511 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 7512 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 7513 | `	if( iLen < 1 ){` |
|      - | 7514 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7515 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7516 | `		return PH7_OK;` |
|      - | 7517 | `	}` |
|      - | 7518 | `	/* Point to the end of the string */` |
|      5 | 7519 | `	zEnd = &zString[iLen];` |
|      5 | 7520 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 7521 | `	/* perform the requested operation */` |
|     15 | 7522 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 7523 | `		c = zList[i];` |
|     11 | 7524 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 7525 | `		if( rc == SXRET_OK ){` |
|      5 | 7526 | `			if( nMax < nOfft ){` |
|      3 | 7527 | `				nOfft = nMax;` |
|      1 | 7528 | `			}` |
|      2 | 7529 | `		}` |
|      6 | 7530 | `	}` |
|      5 | 7531 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 7532 | `		/* No such substring,return FALSE */` |
|      3 | 7533 | `		ph7_result_bool(pCtx,0);` |
|      2 | 7534 | `	}else{` |
|      - | 7535 | `		/* Return the substring */` |
|      3 | 7536 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 7537 | `	}` |
|      5 | 7538 | `	return PH7_OK;` |
|      3 | 7539 | `}` |
|      - | 7540 | `/* SPDX-SnippetBegin */` |
|      - | 7541 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 7542 | `/* SPDX-License-Identifier: blessing */` |
|      - | 7543 | `/*` |
|      - | 7544 | ` * string soundex(string $str)` |
|      - | 7545 | ` *  Calculate the soundex key of a string.` |
|      - | 7546 | ` * Parameters` |
|      - | 7547 | ` *  $str` |
|      - | 7548 | ` *   The input string.` |
|      - | 7549 | ` * Return` |
|      - | 7550 | ` *  Returns the soundex key as a string.` |
|      - | 7551 | ` * Note:` |
|      - | 7552 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 7553 | ` * source tree.` |
|      - | 7554 | ` */` |
|     22 | 7555 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7556 | `{` |
|      - | 7557 | `	const unsigned char *zIn;` |
|      - | 7558 | `	char zResult[8];` |
|      - | 7559 | `	int i, j;` |
|      - | 7560 | `	static const unsigned char iCode[] = {` |
|      - | 7561 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7562 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7563 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7564 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7565 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 7566 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 7567 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 7568 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 7569 | `	};` |
|     23 | 7570 | `	if( nArg < 1 ){` |
|      - | 7571 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7572 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7573 | `		return PH7_OK;` |
|      - | 7574 | `	}` |
|     23 | 7575 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     35 | 7576 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     23 | 7577 | `	if( zIn[i] ){` |
|     17 | 7578 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 7579 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 7580 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 7581 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 7582 | `			if( code>0 ){` |
|     45 | 7583 | `				if( code!=prevcode ){` |
|     33 | 7584 | `					prevcode = (unsigned char)code;` |
|     33 | 7585 | `					zResult[j++] = (char)code + '0';` |
|     16 | 7586 | `				}` |
|     23 | 7587 | `			}else{` |
|     49 | 7588 | `				prevcode = 0;` |
|      - | 7589 | `			}` |
|     47 | 7590 | `		}` |
|     33 | 7591 | `		while( j<4 ){` |
|     17 | 7592 | `			zResult[j++] = '0';` |
|      1 | 7593 | `		}` |
|     17 | 7594 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 7595 | `	}else{` |
|      - | 7596 | `	  /* No alphabetic character: PHP returns "0000" (not the SQLite "?000"). */` |
|      7 | 7597 | `	  ph7_result_string(pCtx,"0000",4);` |
|      - | 7598 | `	}` |
|     23 | 7599 | `	return PH7_OK;` |
|     12 | 7600 | `}` |
|      - | 7601 | `/* SPDX-SnippetEnd */` |
|      - | 7602 | `/*` |
|      - | 7603 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 7604 | ` *  Wraps a string to a given number of characters.` |
|      - | 7605 | ` * Parameters` |
|      - | 7606 | ` *  $str` |
|      - | 7607 | ` *   The input string.` |
|      - | 7608 | ` * $width` |
|      - | 7609 | ` *  The column width.` |
|      - | 7610 | ` * $break` |
|      - | 7611 | ` *  The line is broken using the optional break parameter.` |
|      - | 7612 | ` * Return` |
|      - | 7613 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 7614 | ` */` |
|     26 | 7615 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7616 | `{` |
|      - | 7617 | `	const char *zIn,*zBreak;` |
|      - | 7618 | `	SyBlob sWorker;` |
|      - | 7619 | `	int iLen,iBreaklen,iWidth,iCut,iStart,iSpace,iCur;` |
|      - | 7620 | `	sxi32 rc;` |
|     27 | 7621 | `	if( nArg < 1 ){` |
|      - | 7622 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7623 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7624 | `		return PH7_OK;` |
|      - | 7625 | `	}` |
|      - | 7626 | `	/* Extract the input string */` |
|     27 | 7627 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7628 | `	/* Width (default 75; PHP allows 0/negative — break at every space). */` |
|     27 | 7629 | `	iWidth = 75;` |
|     27 | 7630 | `	if( nArg > 1 ){` |
|     27 | 7631 | `		iWidth = ph7_value_to_int(apArg[1]);` |
|     13 | 7632 | `	}` |
|      - | 7633 | `	/* Break string (default "\n"). */` |
|     27 | 7634 | `	zBreak = "\n";` |
|     27 | 7635 | `	iBreaklen = (int)sizeof(char);` |
|     27 | 7636 | `	if( nArg > 2 ){` |
|     13 | 7637 | `		zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      6 | 7638 | `	}` |
|      - | 7639 | `	/* Cut long words? (default false). */` |
|     27 | 7640 | `	iCut = 0;` |
|     27 | 7641 | `	if( nArg > 3 ){` |
|      7 | 7642 | `		iCut = ph7_value_to_bool(apArg[3]);` |
|      3 | 7643 | `	}` |
|     27 | 7644 | `	if( iLen < 1 ){` |
|      - | 7645 | `		/* PHP returns the empty string for empty input before validating the other args. */` |
|      5 | 7646 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 7647 | `		return PH7_OK;` |
|      - | 7648 | `	}` |
|      - | 7649 | `	/* PHP 8 domain errors (catchable ValueError). */` |
|     23 | 7650 | `	if( iBreaklen < 1 ){` |
|      3 | 7651 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7652 | `			"wordwrap(): Argument #3 ($break) must not be empty");` |
|      - | 7653 | `	}` |
|     21 | 7654 | `	if( iWidth == 0 && iCut ){` |
|      3 | 7655 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7656 | `			"wordwrap(): Argument #4 ($cut_long_words) cannot be true when argument #2 ($width) is 0");` |
|      - | 7657 | `	}` |
|      - | 7658 | `	/*` |
|      - | 7659 | `	 * PHP's algorithm: a single left-to-right pass tracking the start of the` |
|      - | 7660 | `	 * current line (iStart) and the position of the last space seen on it` |
|      - | 7661 | `	 * (iSpace). A break is emitted when the line reaches the width, at the last` |
|      - | 7662 | `	 * space if there was one, otherwise (only when cut is enabled) hard at the` |
|      - | 7663 | `	 * boundary. An existing break sequence in the input resets the line.` |
|      - | 7664 | `	 */` |
|     19 | 7665 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     19 | 7666 | `	iStart = iSpace = iCur = 0;` |
|     19 | 7667 | `	rc = SXRET_OK;` |
|    551 | 7668 | `	while( iCur < iLen ){` |
|    533 | 7669 | `		if( iBreaklen <= iLen - iCur && SyMemcmp(&zIn[iCur],zBreak,(sxu32)iBreaklen) == 0 ){` |
|      - | 7670 | `			/* Existing break sequence in the input: copy it verbatim and reset the line. */` |
|    ! 0 | 7671 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart + iBreaklen));` |
|    ! 0 | 7672 | `			if( rc != SXRET_OK ){ goto oom; }` |
|    ! 0 | 7673 | `			iCur += iBreaklen;` |
|    ! 0 | 7674 | `			iStart = iSpace = iCur;` |
|    ! 0 | 7675 | `			continue;` |
|    533 | 7676 | `		}else if( zIn[iCur] == ' ' ){` |
|     67 | 7677 | `			if( iCur - iStart >= iWidth ){` |
|      - | 7678 | `				/* The line already fills the width at this space: break here (the space is consumed). */` |
|     13 | 7679 | `				rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     13 | 7680 | `				if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     13 | 7681 | `				if( rc != SXRET_OK ){ goto oom; }` |
|     13 | 7682 | `				iStart = iCur + 1;` |
|      6 | 7683 | `			}` |
|     67 | 7684 | `			iSpace = iCur;` |
|    500 | 7685 | `		}else if( iCut && iCur - iStart >= iWidth && iStart >= iSpace ){` |
|      - | 7686 | `			/* A word longer than the width with no space to break at: hard-cut at the boundary. */` |
|      7 | 7687 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|      7 | 7688 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|      7 | 7689 | `			if( rc != SXRET_OK ){ goto oom; }` |
|      7 | 7690 | `			iStart = iSpace = iCur;` |
|    464 | 7691 | `		}else if( iCur - iStart >= iWidth && iStart < iSpace ){` |
|      - | 7692 | `			/* Past the width mid-word: wrap back to the last space (which is consumed). */` |
|     17 | 7693 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iSpace - iStart));` |
|     17 | 7694 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     17 | 7695 | `			if( rc != SXRET_OK ){ goto oom; }` |
|     17 | 7696 | `			iStart = iSpace = iSpace + 1;` |
|      8 | 7697 | `		}` |
|    533 | 7698 | `		iCur++;` |
|      1 | 7699 | `	}` |
|      - | 7700 | `	/* Emit the trailing chunk. */` |
|     19 | 7701 | `	if( iStart < iCur ){` |
|     19 | 7702 | `		rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     19 | 7703 | `		if( rc != SXRET_OK ){ goto oom; }` |
|      9 | 7704 | `	}` |
|     19 | 7705 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|     19 | 7706 | `	SyBlobRelease(&sWorker);` |
|     19 | 7707 | `	return PH7_OK;` |
|    ! 0 | 7708 | `oom:` |
|    ! 0 | 7709 | `	SyBlobRelease(&sWorker);` |
|    ! 0 | 7710 | `	return PH7_ContextMemoryError(pCtx);` |
|     14 | 7711 | `}` |
|      - | 7712 | `/*` |
|      - | 7713 | ` * Check if the given character is a member of the given mask.` |
|      - | 7714 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 7715 | ` * Refer to [strtok()].` |
|      - | 7716 | ` */` |
|     30 | 7717 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 7718 | `{` |
|      - | 7719 | `	int i;` |
|     57 | 7720 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 7721 | `		if( c == zMask[i] ){` |
|     13 | 7722 | `			if( pOfft ){` |
|      5 | 7723 | `				*pOfft = i;` |
|      2 | 7724 | `			}` |
|     13 | 7725 | `			return TRUE;` |
|      - | 7726 | `		}` |
|     14 | 7727 | `	}` |
|     19 | 7728 | `	return FALSE;` |
|     16 | 7729 | `}` |
|      - | 7730 | `/*` |
|      - | 7731 | ` * Extract a single token from the input stream.` |
|      - | 7732 | ` * Refer to [strtok()].` |
|      - | 7733 | ` */` |
|      6 | 7734 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 7735 | `{` |
|      7 | 7736 | `	const char *zIn = *pzIn;` |
|      - | 7737 | `	const char *zPtr;` |
|      - | 7738 | `	/* Ignore leading delimiter */` |
|     11 | 7739 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 7740 | `		zIn++;` |
|      1 | 7741 | `	}` |
|      7 | 7742 | `	if( zIn >= zEnd ){` |
|      - | 7743 | `		/* End of input */` |
|    ! 0 | 7744 | `		return SXERR_EOF;` |
|      - | 7745 | `	}` |
|      7 | 7746 | `	zPtr = zIn;` |
|      - | 7747 | `	/* Extract the token */` |
|     13 | 7748 | `	while( zIn < zEnd ){` |
|     11 | 7749 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 7750 | `			/* UTF-8 stream */` |
|    ! 0 | 7751 | `			zIn++;` |
|    ! 0 | 7752 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 7753 | `		}else{` |
|     11 | 7754 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 7755 | `				break;` |
|      - | 7756 | `			}` |
|      7 | 7757 | `			zIn++;` |
|      - | 7758 | `		}` |
|      1 | 7759 | `	}` |
|      7 | 7760 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 7761 | `	/* Update the cursor */` |
|      7 | 7762 | `	*pzIn = zIn;` |
|      - | 7763 | `	/* Return to the caller */` |
|      7 | 7764 | `	return SXRET_OK;` |
|      4 | 7765 | `}` |
|      - | 7766 | `/* strtok auxiliary private data */` |
|      - | 7767 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 7768 | `struct strtok_aux_data` |
|      - | 7769 | `{` |
|      - | 7770 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 7771 | `	const char *zIn;   /* Current input stream */` |
|      - | 7772 | `	const char *zEnd;  /* End of input */` |
|      - | 7773 | `};` |
|      - | 7774 | `/*` |
|      - | 7775 | ` * string strtok(string $str,string $token)` |
|      - | 7776 | ` * string strtok(string $token)` |
|      - | 7777 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 7778 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 7779 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 7780 | ` *  words by using the space character as the token.` |
|      - | 7781 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 7782 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 7783 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 7784 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 7785 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 7786 | ` *  the argument are found.` |
|      - | 7787 | ` * Parameters` |
|      - | 7788 | ` *  $str` |
|      - | 7789 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 7790 | ` * $token` |
|      - | 7791 | ` *  The delimiter used when splitting up str.` |
|      - | 7792 | ` * Return` |
|      - | 7793 | ` *   Current token or FALSE on EOF.` |
|      - | 7794 | ` */` |
|      6 | 7795 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7796 | `{` |
|      - | 7797 | `	strtok_aux_data *pAux;` |
|      - | 7798 | `	const char *zMask;` |
|      - | 7799 | `	SyString sToken;` |
|      - | 7800 | `	int nMasklen;` |
|      - | 7801 | `	sxi32 rc;` |
|      7 | 7802 | `	if( nArg < 2 ){` |
|      - | 7803 | `		/* Extract top aux data */` |
|      5 | 7804 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      5 | 7805 | `		if( pAux == 0 ){` |
|      - | 7806 | `			/* No aux data,return FALSE */` |
|    ! 0 | 7807 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7808 | `			return PH7_OK;` |
|      - | 7809 | `		}` |
|      5 | 7810 | `		nMasklen = 0;` |
|      5 | 7811 | `		zMask = ""; /* cc warning */` |
|      5 | 7812 | `		if( nArg > 0 ){` |
|      - | 7813 | `			/* Extract the mask */` |
|      5 | 7814 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 7815 | `		}` |
|      5 | 7816 | `		if( nMasklen < 1 ){` |
|      - | 7817 | `			/* Invalid mask,return FALSE */` |
|    ! 0 | 7818 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 7819 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7820 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 7821 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7822 | `			return PH7_OK;` |
|      - | 7823 | `		}` |
|      - | 7824 | `		/* Extract the token */` |
|      5 | 7825 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 7826 | `		if( rc != SXRET_OK ){` |
|      - | 7827 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 7828 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 7829 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7830 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 7831 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7832 | `		}else{` |
|      - | 7833 | `			/* Return the extracted token */` |
|      5 | 7834 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 7835 | `		}` |
|      3 | 7836 | `	}else{` |
|      - | 7837 | `		const char *zInput,*zCur;` |
|      - | 7838 | `		char *zDup;` |
|      - | 7839 | `		int nLen;` |
|      - | 7840 | `		/* Extract the raw input */` |
|      3 | 7841 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 7842 | `		if( nLen < 1 ){` |
|      - | 7843 | `			/* Empty input,return FALSE */` |
|    ! 0 | 7844 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7845 | `			return PH7_OK;` |
|      - | 7846 | `		}` |
|      - | 7847 | `		/* Extract the mask */` |
|      3 | 7848 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 7849 | `		if( nMasklen < 1 ){` |
|      - | 7850 | `			/* Set a default mask */` |
|      - | 7851 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 7852 | `			zMask = TOK_MASK;` |
|    ! 0 | 7853 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 7854 | `#undef TOK_MASK` |
|    ! 0 | 7855 | `		}` |
|      - | 7856 | `		/* Extract a single token */` |
|      3 | 7857 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 7858 | `		if( rc != SXRET_OK ){` |
|      - | 7859 | `			/* Empty input */` |
|    ! 0 | 7860 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7861 | `			return PH7_OK;` |
|    ! 0 | 7862 | `		}else{` |
|      - | 7863 | `			/* Return the extracted token */` |
|      3 | 7864 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 7865 | `		}` |
|      - | 7866 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 7867 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 7868 | `		if( pAux ){` |
|      3 | 7869 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 7870 | `			if( nLen < 1 ){` |
|    ! 0 | 7871 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7872 | `				return PH7_OK;` |
|      - | 7873 | `			}` |
|      - | 7874 | `			/* Duplicate input */` |
|      3 | 7875 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 7876 | `			if( zDup  ){` |
|      3 | 7877 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 7878 | `				/* Register the aux data */` |
|      3 | 7879 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 7880 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 7881 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 7882 | `			}` |
|      1 | 7883 | `		}` |
|      - | 7884 | `	}` |
|      7 | 7885 | `	return PH7_OK;` |
|      4 | 7886 | `}` |
|      - | 7887 | `/*` |
|      - | 7888 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 7889 | ` *  Pad a string to a certain length with another string` |
|      - | 7890 | ` * Parameters` |
|      - | 7891 | ` *  $input` |
|      - | 7892 | ` *   The input string.` |
|      - | 7893 | ` * $pad_length` |
|      - | 7894 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 7895 | ` *   string, no padding takes place.` |
|      - | 7896 | ` * $pad_string` |
|      - | 7897 | ` *   Note:` |
|      - | 7898 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 7899 | ` *    divided by the pad_string's length.` |
|      - | 7900 | ` * $pad_type` |
|      - | 7901 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 7902 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 7903 | ` * Return` |
|      - | 7904 | ` *  The padded string.` |
|      - | 7905 | ` */` |
|     10 | 7906 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7907 | `{` |
|      - | 7908 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 7909 | `	const char *zIn,*zPad;` |
|     11 | 7910 | `	if( nArg < 2 ){` |
|      - | 7911 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7912 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7913 | `		return PH7_OK;` |
|      - | 7914 | `	}` |
|      - | 7915 | `	/* Extract the target string */` |
|     11 | 7916 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7917 | `	/* Padding length */` |
|      - | 7918 | `	{` |
|     11 | 7919 | `		sxi64 iTmp = 0;` |
|     11 | 7920 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_pad",2,"$length","int",&iTmp);` |
|     11 | 7921 | `		if( rcArg != PH7_OK ){` |
|    ! 0 | 7922 | `			return rcArg;` |
|      - | 7923 | `		}` |
|     11 | 7924 | `		iRealPad = iPadlen = (int)iTmp;` |
|      - | 7925 | `	}` |
|     11 | 7926 | `	if( iPadlen > 0 ){` |
|      9 | 7927 | `		iPadlen -= iLen;` |
|      4 | 7928 | `	}` |
|     11 | 7929 | `	if( iPadlen < 1  ){` |
|      - | 7930 | `		/* Return the string verbatim */` |
|      5 | 7931 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      5 | 7932 | `		return PH7_OK;` |
|      - | 7933 | `	}` |
|      7 | 7934 | `	zPad = " "; /* Whitespace padding */` |
|      7 | 7935 | `	iStrpad = (int)sizeof(char);` |
|      7 | 7936 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      7 | 7937 | `	if( nArg > 2 ){` |
|      - | 7938 | `		/* Padding string */` |
|      7 | 7939 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      7 | 7940 | `		if( iStrpad < 1 ){` |
|      - | 7941 | `			/* An empty pad string throws a catchable ValueError in PHP 8` |
|      - | 7942 | `			 * (only reached once padding is actually required). */` |
|      3 | 7943 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7944 | `				"str_pad(): Argument #3 ($pad_string) must not be empty");` |
|      - | 7945 | `		}` |
|      5 | 7946 | `		if( nArg > 3 ){` |
|      - | 7947 | `			/* Padd type */` |
|      5 | 7948 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 7949 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 7950 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 7951 | `			}` |
|      2 | 7952 | `		}` |
|      2 | 7953 | `	}` |
|      5 | 7954 | `	iDiv = 1;` |
|      5 | 7955 | `	if( iType == 2 ){` |
|    ! 0 | 7956 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 7957 | `	}` |
|      - | 7958 | `	/* Perform the requested operation */` |
|      5 | 7959 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 7960 | `		jPad = iStrpad;` |
|      5 | 7961 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 7962 | `			/* Padding */` |
|      5 | 7963 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 7964 | `				break;` |
|      - | 7965 | `			}` |
|      3 | 7966 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7967 | `		}` |
|      3 | 7968 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 7969 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 7970 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 7971 | `				if( jPad > iStrpad ){` |
|    ! 0 | 7972 | `					jPad = iStrpad;` |
|    ! 0 | 7973 | `				}` |
|      3 | 7974 | `				if( jPad < 1){` |
|    ! 0 | 7975 | `					break;` |
|      - | 7976 | `				}` |
|      3 | 7977 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 7978 | `			}` |
|      1 | 7979 | `		}` |
|      1 | 7980 | `	}` |
|      5 | 7981 | `	if( iLen > 0 ){` |
|      - | 7982 | `		/* Append the input string */` |
|      5 | 7983 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7984 | `	}` |
|      5 | 7985 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 7986 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 7987 | `			/* Padding */` |
|      5 | 7988 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 7989 | `				break;` |
|      - | 7990 | `			}` |
|      3 | 7991 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7992 | `		}` |
|      5 | 7993 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 7994 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 7995 | `			if( jPad > iStrpad ){` |
|    ! 0 | 7996 | `				jPad = iStrpad;` |
|    ! 0 | 7997 | `			}` |
|      3 | 7998 | `			if( jPad < 1){` |
|    ! 0 | 7999 | `				break;` |
|      - | 8000 | `			}` |
|      3 | 8001 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 8002 | `		}` |
|      1 | 8003 | `	}` |
|      5 | 8004 | `	return PH7_OK;` |
|      6 | 8005 | `}` |
|      - | 8006 | `/*` |
|      - | 8007 | ` * String replacement private data.` |
|      - | 8008 | ` */` |
|      - | 8009 | `typedef struct str_replace_data str_replace_data;` |
|      - | 8010 | `struct str_replace_data` |
|      - | 8011 | `{` |
|      - | 8012 | `	/* Used by the str_replace family to collect the search/replace arguments. */` |
|      - | 8013 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 8014 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 8015 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 8016 | `};` |
|      - | 8017 | `/*` |
|      - | 8018 | ` * Remove a substring.` |
|      - | 8019 | ` */` |
|      - | 8020 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 8021 | `	for(;;){\` |
|      - | 8022 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 8023 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 8024 | `		++OFFT;\` |
|      - | 8025 | `	}\` |
|      - | 8026 | `}` |
|      - | 8027 | `/*` |
|      - | 8028 | ` * Shift right and insert algorithm.` |
|      - | 8029 | ` */` |
|      - | 8030 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 8031 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 8032 | `		for(;;){\` |
|      - | 8033 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 8034 | `			if(INLEN < 1 ) { break; }\` |
|      - | 8035 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 8036 | `			--INLEN; \` |
|      - | 8037 | `		}\` |
|      - | 8038 | `		for(;;){\` |
|      - | 8039 | `				if(ELEN < 1) { break; }\` |
|      - | 8040 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 8041 | `				OFFT++;\` |
|      - | 8042 | `				ENTRY++;\` |
|      - | 8043 | `				--ELEN;\` |
|      - | 8044 | `		}\` |
|      - | 8045 | `}` |
|      - | 8046 | `/*` |
|      - | 8047 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 8048 | ` * replacement string [i.e: zReplace].` |
|      - | 8049 | ` */` |
|     46 | 8050 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      5 | 8051 | `{` |
|     51 | 8052 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 8053 | `	sxu32 n,m;` |
|     51 | 8054 | `	n = SyBlobLength(pWorker);` |
|     51 | 8055 | `	m = nOfft;` |
|      - | 8056 | `	/* Delete the old entry */` |
|   6577 | 8057 | `	STRDEL(zInput,n,m,nLen);` |
|     51 | 8058 | `	SyBlobLength(pWorker) -= nLen;` |
|     51 | 8059 | `	if( nReplen > 0 ){` |
|     45 | 8060 | `		sxi32 iRep = nReplen;` |
|      - | 8061 | `		sxi32 rc;` |
|      - | 8062 | `		/*` |
|      - | 8063 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 8064 | `		 * string.` |
|      - | 8065 | `		 */` |
|     45 | 8066 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     45 | 8067 | `		if( rc != SXRET_OK ){` |
|      - | 8068 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 8069 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 8070 | `			return rc;` |
|      - | 8071 | `		}` |
|      - | 8072 | `		/* Perform the insertion now */` |
|     45 | 8073 | `		zInput = (char *)SyBlobData(pWorker);` |
|     45 | 8074 | `		n = SyBlobLength(pWorker);` |
|   6361 | 8075 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     45 | 8076 | `		SyBlobLength(pWorker) += nReplen;` |
|     20 | 8077 | `	}` |
|     51 | 8078 | `	return SXRET_OK;` |
|     28 | 8079 | `}` |
|      - | 8080 | `/*` |
|      - | 8081 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 8082 | ` * to collect search/replace string.` |
|      - | 8083 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 8084 | ` */` |
|     86 | 8085 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      5 | 8086 | `{` |
|     91 | 8087 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 8088 | `	SyString sWorker;` |
|      - | 8089 | `	const char *zIn;` |
|      - | 8090 | `	int nByte;` |
|      - | 8091 | `	/* Extract a string representation of the given argument */` |
|     91 | 8092 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     91 | 8093 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     91 | 8094 | `	if( nByte > 0 ){` |
|      - | 8095 | `		char *zDup;` |
|      - | 8096 | `		/* Duplicate the chunk */` |
|     89 | 8097 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 8098 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 8099 | `			);` |
|     89 | 8100 | `		if( zDup == 0 ){` |
|      - | 8101 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 8102 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 8103 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 8104 | `			return SXERR_MEM;` |
|      - | 8105 | `		}` |
|     89 | 8106 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 8107 | `		/* Save the chunk */` |
|     89 | 8108 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     42 | 8109 | `	}` |
|      - | 8110 | `	/* Save for later processing */` |
|     91 | 8111 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 8112 | `	/* All done */` |
|     43 | 8113 | `	SXUNUSED(pKey); /* cc warning */` |
|     91 | 8114 | `	return PH7_OK;` |
|     48 | 8115 | `}` |
|      - | 8116 | `/*` |
|      - | 8117 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 8118 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 8119 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 8120 | ` * Parameters` |
|      - | 8121 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 8122 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 8123 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 8124 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 8125 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 8126 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 8127 | ` * $search` |
|      - | 8128 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 8129 | ` *  to designate multiple needles.` |
|      - | 8130 | ` * $replace` |
|      - | 8131 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 8132 | ` *  to designate multiple replacements.` |
|      - | 8133 | ` * $subject` |
|      - | 8134 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 8135 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 8136 | ` *  of subject, and the return value is an array as well.` |
|      - | 8137 | ` * $count (Not used)` |
|      - | 8138 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 8139 | ` * Return` |
|      - | 8140 | ` * This function returns a string or an array with the replaced values.` |
|      - | 8141 | ` */` |
|  29744 | 8142 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 8143 | `{` |
|      - | 8144 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 8145 | `	ProcStringMatch xMatch;` |
|      - | 8146 | `	const char *zIn,*zFunc;` |
|      - | 8147 | `	str_replace_data sRep;` |
|      - | 8148 | `	SyBlob sWorker;` |
|      - | 8149 | `	SySet sReplace;` |
|      - | 8150 | `	SySet sSearch;` |
|      - | 8151 | `	int rep_str;` |
|      - | 8152 | `	int nByte;` |
|      - | 8153 | `	sxi32 rc;` |
|  29749 | 8154 | `	if( nArg < 3 ){` |
|      - | 8155 | `		/* Missing/Invalid arguments,return null */` |
|    ! 0 | 8156 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8157 | `		return PH7_OK;` |
|      - | 8158 | `	}` |
|      - | 8159 | `	/* Initialize fields */` |
|  29749 | 8160 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  29749 | 8161 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  29749 | 8162 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  29749 | 8163 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  29749 | 8164 | `	sRep.pCtx = pCtx;` |
|  29749 | 8165 | `	sRep.pCollector = &sSearch;` |
|  29749 | 8166 | `	rep_str = 0;` |
|      - | 8167 | `	/* Extract the subject */` |
|  29749 | 8168 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  29749 | 8169 | `	if( nByte < 1 ){` |
|      - | 8170 | `		/* Nothing to replace,return the empty string */` |
|     21 | 8171 | `		ph7_result_string(pCtx,"",0);` |
|     21 | 8172 | `		return PH7_OK;` |
|      - | 8173 | `	}` |
|      - | 8174 | `	/* Copy the subject */` |
|  29729 | 8175 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 8176 | `	/* Search string */` |
|  29729 | 8177 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 8178 | `		/* Collect search string */` |
|     43 | 8179 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|     24 | 8180 | `	}else{` |
|      - | 8181 | `		/* Single pattern */` |
|  29691 | 8182 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  29691 | 8183 | `		if( nByte < 1 ){` |
|      - | 8184 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 8185 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 8186 | `			return PH7_OK;` |
|      - | 8187 | `		}` |
|  29687 | 8188 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 8189 | `		/* Save for later processing */` |
|  29687 | 8190 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 8191 | `	}` |
|      - | 8192 | `	/* Replace string */` |
|  29725 | 8193 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 8194 | `		/* Collect replace string */` |
|      7 | 8195 | `		sRep.pCollector = &sReplace;` |
|      7 | 8196 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 8197 | `	}else{` |
|      - | 8198 | `		/* Single needle */` |
|  29719 | 8199 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  29719 | 8200 | `		rep_str = 1;` |
|  29719 | 8201 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 8202 | `		/* Save for later processing */` |
|  29719 | 8203 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 8204 | `	}` |
|      - | 8205 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  29725 | 8206 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 8207 | `		SySetRelease(&sSearch);` |
|    ! 0 | 8208 | `		SySetRelease(&sReplace);` |
|    ! 0 | 8209 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 8210 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8211 | `	}` |
|      - | 8212 | `	/* Reset loop cursors */` |
|  29725 | 8213 | `	SySetResetCursor(&sSearch);` |
|  29725 | 8214 | `	SySetResetCursor(&sReplace);` |
|  29725 | 8215 | `	pReplace = pSearch = 0; /* cc warning */` |
|  29725 | 8216 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 8217 | `	/* Extract function name */` |
|  29725 | 8218 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 8219 | `	/* Set the default pattern match routine */` |
|  29725 | 8220 | `	xMatch = SyBlobSearch;` |
|  29725 | 8221 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 8222 | `		/* Case insensitive pattern match */` |
|     11 | 8223 | `		xMatch = iPatternMatch;` |
|      5 | 8224 | `	}` |
|      - | 8225 | `	/* Start the replace process */` |
|  59483 | 8226 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 8227 | `		sxu32 nCount,nOfft;` |
|  29763 | 8228 | `		if( pSearch->nByte <  1 ){` |
|      - | 8229 | `			/* Empty string,ignore */` |
|      3 | 8230 | `			continue;` |
|      - | 8231 | `		}` |
|      - | 8232 | `		/* Extract the replace string */` |
|  29761 | 8233 | `		if( rep_str ){` |
|  29751 | 8234 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  14878 | 8235 | `		}else{` |
|     11 | 8236 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 8237 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 8238 | `				 * An empty string is used for the rest of replacement values` |
|      - | 8239 | `				 */` |
|      3 | 8240 | `				pReplace = 0;` |
|      1 | 8241 | `			}` |
|      - | 8242 | `		}` |
|  29761 | 8243 | `		if( pReplace == 0 ){` |
|      - | 8244 | `			/* Use an empty string instead */` |
|      3 | 8245 | `			pReplace = &sTemp;` |
|      1 | 8246 | `		}` |
|  29761 | 8247 | `		nOfft = nCount = 0;` |
|  14901 | 8248 | `		for(;;){` |
|  29807 | 8249 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 8250 | `				break;` |
|      - | 8251 | `			}` |
|      - | 8252 | `			/* Perform a pattern lookup */` |
|  44690 | 8253 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  29790 | 8254 | `				pSearch->nByte,&nOfft);` |
|  29795 | 8255 | `			if( rc != SXRET_OK ){` |
|      - | 8256 | `				/* Pattern not found */` |
|  29749 | 8257 | `				break;` |
|      - | 8258 | `			}` |
|      - | 8259 | `			/* Perform the replace operation */` |
|     51 | 8260 | `			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|     51 | 8261 | `			if( rc != SXRET_OK ){` |
|      - | 8262 | `				/* Allocation failure: surface a fatal instead of a partial result */` |
|    ! 0 | 8263 | `				SySetRelease(&sSearch);` |
|    ! 0 | 8264 | `				SySetRelease(&sReplace);` |
|    ! 0 | 8265 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8266 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8267 | `			}` |
|      - | 8268 | `			/* Increment offset counter */` |
|     51 | 8269 | `			nCount += nOfft + pReplace->nByte;` |
|      5 | 8270 | `		}` |
|      5 | 8271 | `	}` |
|      - | 8272 | `	/* All done,clean-up the mess left behind */` |
|  29725 | 8273 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  29725 | 8274 | `	SySetRelease(&sSearch);` |
|  29725 | 8275 | `	SySetRelease(&sReplace);` |
|  29725 | 8276 | `	SyBlobRelease(&sWorker);` |
|  29725 | 8277 | `	if( rc != PH7_OK ){` |
|    ! 0 | 8278 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8279 | `	}` |
|  29725 | 8280 | `	return PH7_OK;` |
|  14877 | 8281 | `}` |
|      - | 8282 | `/*` |
|      - | 8283 | ` * strtr() array form: a single (key,value) pair copied out of the replace_pairs` |
|      - | 8284 | ` * array. The bytes are owned by a persistent pool (see strtr_collect) rather than` |
|      - | 8285 | ` * the transient walker values, which HashmapWalk releases after each callback, so` |
|      - | 8286 | ` * we store byte offsets into that pool instead of raw pointers.` |
|      - | 8287 | ` */` |
|      - | 8288 | `typedef struct strtr_entry strtr_entry;` |
|      - | 8289 | `struct strtr_entry` |
|      - | 8290 | `{` |
|      - | 8291 | `	sxu32 nKeyOfft; /* Offset of the search key inside the pool */` |
|      - | 8292 | `	sxu32 nKeyLen;  /* Length of the search key */` |
|      - | 8293 | `	sxu32 nValOfft; /* Offset of the replacement inside the pool */` |
|      - | 8294 | `	sxu32 nValLen;  /* Length of the replacement */` |
|      - | 8295 | `};` |
|      - | 8296 | `typedef struct strtr_collect strtr_collect;` |
|      - | 8297 | `struct strtr_collect` |
|      - | 8298 | `{` |
|      - | 8299 | `	SyBlob *pPool;  /* Byte pool holding copied key + value bytes */` |
|      - | 8300 | `	SySet  *pTable; /* Set of strtr_entry (parallel offsets into pPool) */` |
|      - | 8301 | `	sxi32   rc;     /* Carries an allocation failure (SXERR_MEM) out of the walker */` |
|      - | 8302 | `};` |
|      - | 8303 | `/*` |
|      - | 8304 | ` * Collect one replace_pairs entry into the persistent pool/offset table.` |
|      - | 8305 | ` * PHP coerces both the key and the value to string (an integer key becomes its` |
|      - | 8306 | ` * decimal form) and ignores an empty-string key.` |
|      - | 8307 | ` */` |
|     20 | 8308 | `static int StrtrCollectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 8309 | `{` |
|     21 | 8310 | `	strtr_collect *pCol = (strtr_collect *)pUserData;` |
|      - | 8311 | `	const char *zKey,*zVal;` |
|      - | 8312 | `	strtr_entry sEnt;` |
|      - | 8313 | `	int nKey,nVal;` |
|     21 | 8314 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|     21 | 8315 | `	if( nKey < 1 ){` |
|      - | 8316 | `		/* PHP ignores an empty-string key (it also emits a warning we do not replicate). */` |
|    ! 0 | 8317 | `		return PH7_OK;` |
|      - | 8318 | `	}` |
|     21 | 8319 | `	zVal = ph7_value_to_string(pData,&nVal);` |
|     21 | 8320 | `	sEnt.nKeyOfft = SyBlobLength(pCol->pPool);` |
|     21 | 8321 | `	sEnt.nKeyLen  = (sxu32)nKey;` |
|     21 | 8322 | `	if( SyBlobAppend(pCol->pPool,(const void *)zKey,(sxu32)nKey) != SXRET_OK ){` |
|    ! 0 | 8323 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8324 | `		return SXERR_ABORT;` |
|      - | 8325 | `	}` |
|     21 | 8326 | `	sEnt.nValOfft = SyBlobLength(pCol->pPool);` |
|     21 | 8327 | `	sEnt.nValLen  = (sxu32)nVal;` |
|     21 | 8328 | `	if( nVal > 0 && SyBlobAppend(pCol->pPool,(const void *)zVal,(sxu32)nVal) != SXRET_OK ){` |
|    ! 0 | 8329 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8330 | `		return SXERR_ABORT;` |
|      - | 8331 | `	}` |
|     21 | 8332 | `	if( SySetPut(pCol->pTable,(const void *)&sEnt) != SXRET_OK ){` |
|    ! 0 | 8333 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8334 | `		return SXERR_ABORT;` |
|      - | 8335 | `	}` |
|     21 | 8336 | `	return PH7_OK;` |
|     11 | 8337 | `}` |
|      - | 8338 | `/*` |
|      - | 8339 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 8340 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 8341 | ` *  Translate characters or replace substrings.` |
|      - | 8342 | ` * Parameters` |
|      - | 8343 | ` *  $str` |
|      - | 8344 | ` *  The string being translated.` |
|      - | 8345 | ` * $from` |
|      - | 8346 | ` *  The string being translated to to.` |
|      - | 8347 | ` * $to` |
|      - | 8348 | ` *  The string replacing from.` |
|      - | 8349 | ` * $replace_pairs` |
|      - | 8350 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 8351 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 8352 | ` * Return` |
|      - | 8353 | ` *  The translated string.` |
|      - | 8354 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 8355 | ` */` |
|     12 | 8356 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8357 | `{` |
|      - | 8358 | `	const char *zIn;` |
|      - | 8359 | `	int nLen;` |
|     13 | 8360 | `	if( nArg < 1 ){` |
|      - | 8361 | `		/* Nothing to replace,return FALSE */` |
|    ! 0 | 8362 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8363 | `		return PH7_OK;` |
|      - | 8364 | `	}` |
|     13 | 8365 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 8366 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 8367 | `		/* Invalid arguments */` |
|    ! 0 | 8368 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8369 | `		return PH7_OK;` |
|      - | 8370 | `	}` |
|     18 | 8371 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 8372 | `		strtr_collect sCol;` |
|      - | 8373 | `		SyBlob sPool,sWorker;` |
|      - | 8374 | `		SySet sTable;` |
|      - | 8375 | `		const char *zPool;` |
|      - | 8376 | `		strtr_entry *pEnt;` |
|      - | 8377 | `		sxi32 rc;` |
|      - | 8378 | `		int i,iRun;` |
|      - | 8379 | `		/*` |
|      - | 8380 | `		 * PHP's array-form strtr is a single left-to-right pass over the subject:` |
|      - | 8381 | `		 * at every position it substitutes the LONGEST replace_pairs key that` |
|      - | 8382 | `		 * matches there, then advances past the key (replacements are never` |
|      - | 8383 | `		 * rescanned). It is not a sequential per-key global replace. First copy` |
|      - | 8384 | `		 * the pairs into a persistent pool, then run that scan.` |
|      - | 8385 | `		 */` |
|     11 | 8386 | `		SyBlobInit(&sPool,&pCtx->pVm->sAllocator);` |
|     11 | 8387 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     11 | 8388 | `		SySetInit(&sTable,&pCtx->pVm->sAllocator,sizeof(strtr_entry));` |
|     11 | 8389 | `		sCol.pPool  = &sPool;` |
|     11 | 8390 | `		sCol.pTable = &sTable;` |
|     11 | 8391 | `		sCol.rc     = SXRET_OK;` |
|     11 | 8392 | `		ph7_array_walk(apArg[1],StrtrCollectWalker,&sCol);` |
|     11 | 8393 | `		if( sCol.rc != SXRET_OK ){` |
|      - | 8394 | `			/* Allocation failure while collecting the pairs: surface a fatal */` |
|    ! 0 | 8395 | `			SyBlobRelease(&sPool);` |
|    ! 0 | 8396 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 8397 | `			SySetRelease(&sTable);` |
|    ! 0 | 8398 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 8399 | `		}` |
|      - | 8400 | `		/* The pool is now stable, so offsets can be resolved against its base. */` |
|     11 | 8401 | `		zPool = (const char *)SyBlobData(&sPool);` |
|     11 | 8402 | `		rc = SXRET_OK;` |
|     11 | 8403 | `		iRun = 0; /* Start of the pending run of unmatched bytes copied verbatim. */` |
|     43 | 8404 | `		for( i = 0 ; i < nLen ; ){` |
|     33 | 8405 | `			strtr_entry *pBest = 0;` |
|     33 | 8406 | `			sxu32 nBest = 0;` |
|      - | 8407 | `			/* Pick the longest key that matches at the current position. */` |
|     33 | 8408 | `			SySetResetCursor(&sTable);` |
|     97 | 8409 | `			while( SXRET_OK == SySetGetNextEntry(&sTable,(void **)&pEnt) ){` |
|     64 | 8410 | `				if( pEnt->nKeyLen > nBest` |
|     60 | 8411 | `					&& pEnt->nKeyLen <= (sxu32)(nLen - i)` |
|     56 | 8412 | `					&& SyMemcmp(zPool + pEnt->nKeyOfft,zIn + i,pEnt->nKeyLen) == 0 ){` |
|     29 | 8413 | `					nBest = pEnt->nKeyLen;` |
|     29 | 8414 | `					pBest = pEnt;` |
|     14 | 8415 | `				}` |
|      1 | 8416 | `			}` |
|     33 | 8417 | `			if( pBest == 0 ){` |
|      - | 8418 | `				/* No key here: extend the literal run and copy it in one shot later. */` |
|      9 | 8419 | `				i++;` |
|      9 | 8420 | `				continue;` |
|      - | 8421 | `			}` |
|      - | 8422 | `			/* Flush the pending literal run, then the replacement. */` |
|     25 | 8423 | `			if( i > iRun ){` |
|      5 | 8424 | `				rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(i - iRun));` |
|      2 | 8425 | `			}` |
|     25 | 8426 | `			if( rc == SXRET_OK && pBest->nValLen > 0 ){` |
|     25 | 8427 | `				rc = SyBlobAppend(&sWorker,zPool + pBest->nValOfft,pBest->nValLen);` |
|     12 | 8428 | `			}` |
|     25 | 8429 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8430 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 8431 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8432 | `				SySetRelease(&sTable);` |
|    ! 0 | 8433 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8434 | `			}` |
|     25 | 8435 | `			i += (int)pBest->nKeyLen;` |
|     25 | 8436 | `			iRun = i;` |
|      1 | 8437 | `		}` |
|      - | 8438 | `		/* Flush the trailing literal run. */` |
|     11 | 8439 | `		if( nLen > iRun ){` |
|      3 | 8440 | `			rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(nLen - iRun));` |
|      3 | 8441 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8442 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 8443 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8444 | `				SySetRelease(&sTable);` |
|    ! 0 | 8445 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8446 | `			}` |
|      1 | 8447 | `		}` |
|      - | 8448 | `		/* All done, return the result string */` |
|     16 | 8449 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|     10 | 8450 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 8451 | `		/* Clean-up */` |
|     11 | 8452 | `		SyBlobRelease(&sPool);` |
|     11 | 8453 | `		SyBlobRelease(&sWorker);` |
|     11 | 8454 | `		SySetRelease(&sTable);` |
|     11 | 8455 | `		if( rc != PH7_OK ){` |
|    ! 0 | 8456 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 8457 | `		}` |
|      6 | 8458 | `	}else{` |
|      - | 8459 | `		int i,flen,tlen,c,iOfft;` |
|      - | 8460 | `		const char *zFrom,*zTo;` |
|      3 | 8461 | `		if( nArg < 3 ){` |
|      - | 8462 | `			/* Nothing to replace */` |
|    ! 0 | 8463 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8464 | `			return PH7_OK;` |
|      - | 8465 | `		}` |
|      - | 8466 | `		/* Extract given arguments */` |
|      3 | 8467 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 8468 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 8469 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 8470 | `			/* Nothing to replace */` |
|    ! 0 | 8471 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8472 | `			return PH7_OK;` |
|      - | 8473 | `		}` |
|      - | 8474 | `		/* Start the replace process */` |
|     13 | 8475 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 8476 | `			c = zIn[i];` |
|     11 | 8477 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 8478 | `				if ( iOfft < tlen ){` |
|      5 | 8479 | `					c = zTo[iOfft];` |
|      2 | 8480 | `				}` |
|      2 | 8481 | `			}` |
|     11 | 8482 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 8483 |  |
|      6 | 8484 | `		}` |
|      - | 8485 | `	}` |
|     13 | 8486 | `	return PH7_OK;` |
|      7 | 8487 | `}` |
|      - | 8488 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8489 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8490 | `/*` |
|      - | 8491 | ` * Parse an INI string.` |
|      - | 8492 |  |
|      - | 8493 | ` * According to wikipedia` |
|      - | 8494 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 8495 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 8496 | ` *  Format` |
|      - | 8497 | `*    Properties` |
|      - | 8498 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 8499 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 8500 | `*     Example:` |
|      - | 8501 | `*      name=value` |
|      - | 8502 | `*    Sections` |
|      - | 8503 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 8504 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 8505 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 8506 | `*     or the end of the file. Sections may not be nested.` |
|      - | 8507 | `*     Example:` |
|      - | 8508 | `*      [section]` |
|      - | 8509 | `*   Comments` |
|      - | 8510 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 8511 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 8512 | `*/` |
|     12 | 8513 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 8514 | `{` |
|      - | 8515 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 8516 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 8517 | `	SyHashEntry *pEntry;` |
|      - | 8518 | `	SyString sEntry;` |
|      - | 8519 | `	SyHash sHash;` |
|      - | 8520 | `	int c;` |
|      - | 8521 | `	/* Create an empty array and worker variables */` |
|     13 | 8522 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 8523 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 8524 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 8525 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 8526 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|    ! 0 | 8527 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8528 | `	}` |
|     13 | 8529 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 8530 | `	pCur = pArray;` |
|      - | 8531 | `	/* Start the parse process */` |
|     21 | 8532 | `	for(;;){` |
|      - | 8533 | `		/* Ignore leading white spaces */` |
|     69 | 8534 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 8535 | `			zIn++;` |
|      1 | 8536 | `		}` |
|     43 | 8537 | `		if( zIn >= zEnd ){` |
|      - | 8538 | `			/* No more input to process */` |
|     13 | 8539 | `			break;` |
|      - | 8540 | `		}` |
|     31 | 8541 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 8542 | `			/* Comment til the end of line */` |
|    ! 0 | 8543 | `			zIn++;` |
|    ! 0 | 8544 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 8545 | `				zIn++;` |
|    ! 0 | 8546 | `			}` |
|    ! 0 | 8547 | `			continue;` |
|      - | 8548 | `		}` |
|      - | 8549 | `		/* Reset the string cursor of the working variable */` |
|     31 | 8550 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 8551 | `		if( zIn[0] == '[' ){` |
|      - | 8552 | `			/* Section: Extract the section name */` |
|      9 | 8553 | `			zIn++;` |
|      9 | 8554 | `			zCur = zIn;` |
|     73 | 8555 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 8556 | `				zIn++;` |
|      1 | 8557 | `			}` |
|      9 | 8558 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 8559 | `				/* Save the section name */` |
|      5 | 8560 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 8561 | `				SyStringFullTrim(&sEntry);` |
|      5 | 8562 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 8563 | `				if( sEntry.nByte > 0 ){` |
|      - | 8564 | `					/* Associate an array with the section */` |
|      5 | 8565 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 8566 | `					if( pSection ){` |
|      5 | 8567 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 8568 | `						pCur = pSection;` |
|      2 | 8569 | `					}` |
|      2 | 8570 | `				}` |
|      2 | 8571 | `			}` |
|      9 | 8572 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 8573 | `		}else{` |
|      - | 8574 | `			ph7_value *pOldCur;` |
|      - | 8575 | `			int is_array;` |
|      - | 8576 | `			int iLen;` |
|      - | 8577 | `			/* Properties */` |
|     23 | 8578 | `			is_array = 0;` |
|     23 | 8579 | `			zCur = zIn;` |
|     23 | 8580 | `			iLen = 0; /* cc warning */` |
|     23 | 8581 | `			pOldCur = pCur;` |
|    155 | 8582 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 8583 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 8584 | `					/* Array */` |
|    ! 0 | 8585 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 8586 | `					is_array = 1;` |
|    ! 0 | 8587 | `					if( iLen > 0 ){` |
|    ! 0 | 8588 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 8589 | `						/* Query the hashtable */` |
|    ! 0 | 8590 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 8591 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 8592 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 8593 | `						if( pEntry ){` |
|    ! 0 | 8594 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 8595 | `						}else{` |
|      - | 8596 | `							/* Create an empty array */` |
|    ! 0 | 8597 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 8598 | `							if( pvArr ){` |
|      - | 8599 | `								/* Save the entry */` |
|    ! 0 | 8600 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 8601 | `								/* Insert the entry */` |
|    ! 0 | 8602 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 8603 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 8604 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 8605 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 8606 | `							}` |
|      - | 8607 | `						}` |
|    ! 0 | 8608 | `						if( pvArr ){` |
|    ! 0 | 8609 | `							pCur = pvArr;` |
|    ! 0 | 8610 | `						}` |
|    ! 0 | 8611 | `					}` |
|    ! 0 | 8612 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 8613 | `						zIn++;` |
|    ! 0 | 8614 | `					}` |
|    ! 0 | 8615 | `				}` |
|    133 | 8616 | `				zIn++;` |
|      1 | 8617 | `			}` |
|     23 | 8618 | `			if( !is_array ){` |
|     23 | 8619 | `				iLen = (int)(zIn-zCur);` |
|     11 | 8620 | `			}` |
|      - | 8621 | `			/* Trim the key */` |
|     23 | 8622 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 8623 | `			SyStringFullTrim(&sEntry);` |
|     23 | 8624 | `			if( sEntry.nByte > 0 ){` |
|     23 | 8625 | `				if( !is_array ){` |
|      - | 8626 | `					/* Save the key name */` |
|     23 | 8627 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 8628 | `				}` |
|      - | 8629 | `				/* extract key value */` |
|     23 | 8630 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 8631 | `				zIn++; /* '=' */` |
|     39 | 8632 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 8633 | `					zIn++;` |
|      1 | 8634 | `				}` |
|     23 | 8635 | `				if( zIn < zEnd ){` |
|     21 | 8636 | `					zCur = zIn;` |
|     21 | 8637 | `					c = zIn[0];` |
|     21 | 8638 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 8639 | `						zIn++;` |
|      - | 8640 | `						/* Delimit the value */` |
|    ! 0 | 8641 | `						while( zIn < zEnd ){` |
|    ! 0 | 8642 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 8643 | `								break;` |
|      - | 8644 | `							}` |
|    ! 0 | 8645 | `							zIn++;` |
|    ! 0 | 8646 | `						}` |
|    ! 0 | 8647 | `						if( zIn < zEnd ){` |
|    ! 0 | 8648 | `							zIn++;` |
|    ! 0 | 8649 | `						}` |
|    ! 0 | 8650 | `					}else{` |
|    125 | 8651 | `						while( zIn < zEnd ){` |
|    123 | 8652 | `							if( zIn[0] == '\n' ){` |
|     19 | 8653 | `								if( zIn[-1] != '\\' ){` |
|     19 | 8654 | `									break;` |
|    ! 0 | 8655 | `								}` |
|    105 | 8656 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 8657 | `								/* Inline comments */` |
|    ! 0 | 8658 | `								break;` |
|      - | 8659 | `							}` |
|    105 | 8660 | `							zIn++;` |
|      1 | 8661 | `						}` |
|      - | 8662 | `					}` |
|      - | 8663 | `					/* Trim the value */` |
|     21 | 8664 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 8665 | `					SyStringFullTrim(&sEntry);` |
|     21 | 8666 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 8667 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 8668 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 8669 | `					}` |
|     21 | 8670 | `					if( sEntry.nByte > 0 ){` |
|     21 | 8671 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 8672 | `					}` |
|      - | 8673 | `					/* Insert the key and it's value */` |
|     21 | 8674 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 8675 | `				}` |
|     12 | 8676 | `			}else{` |
|    ! 0 | 8677 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 8678 | `					zIn++;` |
|    ! 0 | 8679 | `				}` |
|      - | 8680 | `			}` |
|     23 | 8681 | `			pCur = pOldCur;` |
|      - | 8682 | `		}` |
|      1 | 8683 | `	}` |
|     13 | 8684 | `	SyHashRelease(&sHash);` |
|      - | 8685 | `	/* Return the parse of the INI string */` |
|     13 | 8686 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 8687 | `	return SXRET_OK;` |
|      7 | 8688 | `}` |
|      - | 8689 | `/*` |
|      - | 8690 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 8691 | ` *  Parse a configuration string.` |
|      - | 8692 | ` * Parameters` |
|      - | 8693 | ` *  $ini` |
|      - | 8694 | ` *   The contents of the ini file being parsed.` |
|      - | 8695 | ` *  $process_sections` |
|      - | 8696 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 8697 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 8698 | ` *  $scanner_mode (Not used)` |
|      - | 8699 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 8700 | ` *   then option values will not be parsed.` |
|      - | 8701 | ` * Return` |
|      - | 8702 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 8703 | ` */` |
|     10 | 8704 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8705 | `{` |
|      - | 8706 | `	const char *zIni;` |
|      - | 8707 | `	int nByte;` |
|     11 | 8708 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8709 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 8710 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8711 | `		return PH7_OK;` |
|      - | 8712 | `	}` |
|      - | 8713 | `	/* Extract the raw INI buffer */` |
|     11 | 8714 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 8715 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|     11 | 8716 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      6 | 8717 | `}` |
|      - | 8718 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8719 |  |
|      - | 8720 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8721 |  |
|      - | 8722 | `/*` |
|      - | 8723 | ` * Ctype Functions.` |
|      - | 8724 | ` * Status:` |
|      - | 8725 | ` *    Stable.` |
|      - | 8726 | ` */` |
|      - | 8727 | `/*` |
|      - | 8728 | ` * bool ctype_alnum(string $text)` |
|      - | 8729 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 8730 | ` * Parameters` |
|      - | 8731 | ` *  $text` |
|      - | 8732 | ` *   The tested string.` |
|      - | 8733 | ` * Return` |
|      - | 8734 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 8735 | ` */` |
|     14 | 8736 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8737 | `{` |
|      - | 8738 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8739 | `	int nLen;` |
|     15 | 8740 | `	if( nArg < 1 ){` |
|      - | 8741 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8742 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8743 | `		return PH7_OK;` |
|      - | 8744 | `	}` |
|      - | 8745 | `	/* Extract the target string */` |
|     15 | 8746 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 8747 | `	zEnd = &zIn[nLen];` |
|     15 | 8748 | `	if( nLen < 1 ){` |
|      - | 8749 | `		/* Empty string,return FALSE */` |
|      3 | 8750 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8751 | `		return PH7_OK;` |
|      - | 8752 | `	}` |
|      - | 8753 | `	/* Perform the requested operation */` |
|     32 | 8754 | `	for(;;){` |
|     65 | 8755 | `		if( zIn >= zEnd ){` |
|      - | 8756 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8757 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8758 | `			return PH7_OK;` |
|      - | 8759 | `		}` |
|     57 | 8760 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 8761 | `			break;` |
|      - | 8762 | `		}` |
|      - | 8763 | `		/* Point to the next character */` |
|     53 | 8764 | `		zIn++;` |
|      1 | 8765 | `	}` |
|      - | 8766 | `	/* The test failed,return FALSE */` |
|      5 | 8767 | `	ph7_result_bool(pCtx,0);` |
|      5 | 8768 | `	return PH7_OK;` |
|      8 | 8769 | `}` |
|      - | 8770 | `/*` |
|      - | 8771 | ` * bool ctype_alpha(string $text)` |
|      - | 8772 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 8773 | ` * Parameters` |
|      - | 8774 | ` *  $text` |
|      - | 8775 | ` *   The tested string.` |
|      - | 8776 | ` * Return` |
|      - | 8777 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 8778 | ` */` |
|     16 | 8779 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8780 | `{` |
|      - | 8781 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8782 | `	int nLen;` |
|     17 | 8783 | `	if( nArg < 1 ){` |
|      - | 8784 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8785 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8786 | `		return PH7_OK;` |
|      - | 8787 | `	}` |
|      - | 8788 | `	/* Extract the target string */` |
|     17 | 8789 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8790 | `	zEnd = &zIn[nLen];` |
|     17 | 8791 | `	if( nLen < 1 ){` |
|      - | 8792 | `		/* Empty string,return FALSE */` |
|      3 | 8793 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8794 | `		return PH7_OK;` |
|      - | 8795 | `	}` |
|      - | 8796 | `	/* Perform the requested operation */` |
|     42 | 8797 | `	for(;;){` |
|     85 | 8798 | `		if( zIn >= zEnd ){` |
|      - | 8799 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8800 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8801 | `			return PH7_OK;` |
|      - | 8802 | `		}` |
|     77 | 8803 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 8804 | `			break;` |
|      - | 8805 | `		}` |
|      - | 8806 | `		/* Point to the next character */` |
|     71 | 8807 | `		zIn++;` |
|      1 | 8808 | `	}` |
|      - | 8809 | `	/* The test failed,return FALSE */` |
|      7 | 8810 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8811 | `	return PH7_OK;` |
|      9 | 8812 | `}` |
|      - | 8813 | `/*` |
|      - | 8814 | ` * bool ctype_cntrl(string $text)` |
|      - | 8815 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 8816 | ` * Parameters` |
|      - | 8817 | ` *  $text` |
|      - | 8818 | ` *   The tested string.` |
|      - | 8819 | ` * Return` |
|      - | 8820 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 8821 | ` */` |
|     16 | 8822 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8823 | `{` |
|      - | 8824 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8825 | `	int nLen;` |
|     17 | 8826 | `	if( nArg < 1 ){` |
|      - | 8827 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8828 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8829 | `		return PH7_OK;` |
|      - | 8830 | `	}` |
|      - | 8831 | `	/* Extract the target string */` |
|     17 | 8832 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8833 | `	zEnd = &zIn[nLen];` |
|     17 | 8834 | `	if( nLen < 1 ){` |
|      - | 8835 | `		/* Empty string,return FALSE */` |
|      3 | 8836 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8837 | `		return PH7_OK;` |
|      - | 8838 | `	}` |
|      - | 8839 | `	/* Perform the requested operation */` |
|     14 | 8840 | `	for(;;){` |
|     29 | 8841 | `		if( zIn >= zEnd ){` |
|      - | 8842 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8843 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8844 | `			return PH7_OK;` |
|      - | 8845 | `		}` |
|     21 | 8846 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8847 | `			/* UTF-8 stream  */` |
|    ! 0 | 8848 | `			break;` |
|      - | 8849 | `		}` |
|     21 | 8850 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 8851 | `			break;` |
|      - | 8852 | `		}` |
|      - | 8853 | `		/* Point to the next character */` |
|     15 | 8854 | `		zIn++;` |
|      1 | 8855 | `	}` |
|      - | 8856 | `	/* The test failed,return FALSE */` |
|      7 | 8857 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8858 | `	return PH7_OK;` |
|      9 | 8859 | `}` |
|      - | 8860 | `/*` |
|      - | 8861 | ` * bool ctype_digit(string $text)` |
|      - | 8862 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 8863 | ` * Parameters` |
|      - | 8864 | ` *  $text` |
|      - | 8865 | ` *   The tested string.` |
|      - | 8866 | ` * Return` |
|      - | 8867 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 8868 | ` */` |
|   2188 | 8869 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 8870 | `{` |
|      - | 8871 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8872 | `	int nLen;` |
|   2193 | 8873 | `	if( nArg < 1 ){` |
|      - | 8874 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8875 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8876 | `		return PH7_OK;` |
|      - | 8877 | `	}` |
|      - | 8878 | `	/* Extract the target string */` |
|   2193 | 8879 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   2193 | 8880 | `	zEnd = &zIn[nLen];` |
|   2193 | 8881 | `	if( nLen < 1 ){` |
|      - | 8882 | `		/* Empty string,return FALSE */` |
|      3 | 8883 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8884 | `		return PH7_OK;` |
|      - | 8885 | `	}` |
|      - | 8886 | `	/* Perform the requested operation */` |
|   2011 | 8887 | `	for(;;){` |
|   4027 | 8888 | `		if( zIn >= zEnd ){` |
|      - | 8889 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1791 | 8890 | `			ph7_result_bool(pCtx,1);` |
|   1791 | 8891 | `			return PH7_OK;` |
|      - | 8892 | `		}` |
|   2241 | 8893 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8894 | `			/* UTF-8 stream  */` |
|    ! 0 | 8895 | `			break;` |
|      - | 8896 | `		}` |
|   2241 | 8897 | `		if( !SyisDigit(zIn[0]) ){` |
|    405 | 8898 | `			break;` |
|      - | 8899 | `		}` |
|      - | 8900 | `		/* Point to the next character */` |
|   1841 | 8901 | `		zIn++;` |
|      5 | 8902 | `	}` |
|      - | 8903 | `	/* The test failed,return FALSE */` |
|    405 | 8904 | `	ph7_result_bool(pCtx,0);` |
|    405 | 8905 | `	return PH7_OK;` |
|   1099 | 8906 | `}` |
|      - | 8907 | `/*` |
|      - | 8908 | ` * bool ctype_xdigit(string $text)` |
|      - | 8909 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 8910 | ` * Parameters` |
|      - | 8911 | ` *  $text` |
|      - | 8912 | ` *   The tested string.` |
|      - | 8913 | ` * Return` |
|      - | 8914 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 8915 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 8916 | ` */` |
|     18 | 8917 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8918 | `{` |
|      - | 8919 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8920 | `	int nLen;` |
|     19 | 8921 | `	if( nArg < 1 ){` |
|      - | 8922 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8923 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8924 | `		return PH7_OK;` |
|      - | 8925 | `	}` |
|      - | 8926 | `	/* Extract the target string */` |
|     19 | 8927 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 8928 | `	zEnd = &zIn[nLen];` |
|     19 | 8929 | `	if( nLen < 1 ){` |
|      - | 8930 | `		/* Empty string,return FALSE */` |
|      3 | 8931 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8932 | `		return PH7_OK;` |
|      - | 8933 | `	}` |
|      - | 8934 | `	/* Perform the requested operation */` |
|     46 | 8935 | `	for(;;){` |
|     93 | 8936 | `		if( zIn >= zEnd ){` |
|      - | 8937 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 8938 | `			ph7_result_bool(pCtx,1);` |
|     11 | 8939 | `			return PH7_OK;` |
|      - | 8940 | `		}` |
|     83 | 8941 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8942 | `			/* UTF-8 stream  */` |
|    ! 0 | 8943 | `			break;` |
|      - | 8944 | `		}` |
|     83 | 8945 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 8946 | `			break;` |
|      - | 8947 | `		}` |
|      - | 8948 | `		/* Point to the next character */` |
|     77 | 8949 | `		zIn++;` |
|      1 | 8950 | `	}` |
|      - | 8951 | `	/* The test failed,return FALSE */` |
|      7 | 8952 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8953 | `	return PH7_OK;` |
|     10 | 8954 | `}` |
|      - | 8955 | `/*` |
|      - | 8956 | ` * bool ctype_graph(string $text)` |
|      - | 8957 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 8958 | ` * Parameters` |
|      - | 8959 | ` *  $text` |
|      - | 8960 | ` *   The tested string.` |
|      - | 8961 | ` * Return` |
|      - | 8962 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 8963 | ` * (no white space), FALSE otherwise.` |
|      - | 8964 | ` */` |
|     16 | 8965 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8966 | `{` |
|      - | 8967 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8968 | `	int nLen;` |
|     17 | 8969 | `	if( nArg < 1 ){` |
|      - | 8970 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8971 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8972 | `		return PH7_OK;` |
|      - | 8973 | `	}` |
|      - | 8974 | `	/* Extract the target string */` |
|     17 | 8975 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8976 | `	zEnd = &zIn[nLen];` |
|     17 | 8977 | `	if( nLen < 1 ){` |
|      - | 8978 | `		/* Empty string,return FALSE */` |
|      3 | 8979 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8980 | `		return PH7_OK;` |
|      - | 8981 | `	}` |
|      - | 8982 | `	/* Perform the requested operation */` |
|     57 | 8983 | `	for(;;){` |
|    115 | 8984 | `		if( zIn >= zEnd ){` |
|      - | 8985 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8986 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8987 | `			return PH7_OK;` |
|      - | 8988 | `		}` |
|    107 | 8989 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8990 | `			/* UTF-8 stream  */` |
|    ! 0 | 8991 | `			break;` |
|      - | 8992 | `		}` |
|    107 | 8993 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 8994 | `			break;` |
|      - | 8995 | `		}` |
|      - | 8996 | `		/* Point to the next character */` |
|    101 | 8997 | `		zIn++;` |
|      1 | 8998 | `	}` |
|      - | 8999 | `	/* The test failed,return FALSE */` |
|      7 | 9000 | `	ph7_result_bool(pCtx,0);` |
|      7 | 9001 | `	return PH7_OK;` |
|      9 | 9002 | `}` |
|      - | 9003 | `/*` |
|      - | 9004 | ` * bool ctype_print(string $text)` |
|      - | 9005 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 9006 | ` * Parameters` |
|      - | 9007 | ` *  $text` |
|      - | 9008 | ` *   The tested string.` |
|      - | 9009 | ` * Return` |
|      - | 9010 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 9011 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 9012 | ` *  or control function at all.` |
|      - | 9013 | ` */` |
|     16 | 9014 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9015 | `{` |
|      - | 9016 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9017 | `	int nLen;` |
|     17 | 9018 | `	if( nArg < 1 ){` |
|      - | 9019 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9020 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9021 | `		return PH7_OK;` |
|      - | 9022 | `	}` |
|      - | 9023 | `	/* Extract the target string */` |
|     17 | 9024 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9025 | `	zEnd = &zIn[nLen];` |
|     17 | 9026 | `	if( nLen < 1 ){` |
|      - | 9027 | `		/* Empty string,return FALSE */` |
|      3 | 9028 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9029 | `		return PH7_OK;` |
|      - | 9030 | `	}` |
|      - | 9031 | `	/* Perform the requested operation */` |
|     63 | 9032 | `	for(;;){` |
|    127 | 9033 | `		if( zIn >= zEnd ){` |
|      - | 9034 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 9035 | `			ph7_result_bool(pCtx,1);` |
|      9 | 9036 | `			return PH7_OK;` |
|      - | 9037 | `		}` |
|    119 | 9038 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9039 | `			/* UTF-8 stream  */` |
|    ! 0 | 9040 | `			break;` |
|      - | 9041 | `		}` |
|    119 | 9042 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 9043 | `			break;` |
|      - | 9044 | `		}` |
|      - | 9045 | `		/* Point to the next character */` |
|    113 | 9046 | `		zIn++;` |
|      1 | 9047 | `	}` |
|      - | 9048 | `	/* The test failed,return FALSE */` |
|      7 | 9049 | `	ph7_result_bool(pCtx,0);` |
|      7 | 9050 | `	return PH7_OK;` |
|      9 | 9051 | `}` |
|      - | 9052 | `/*` |
|      - | 9053 | ` * bool ctype_punct(string $text)` |
|      - | 9054 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 9055 | ` * Parameters` |
|      - | 9056 | ` *  $text` |
|      - | 9057 | ` *   The tested string.` |
|      - | 9058 | ` * Return` |
|      - | 9059 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 9060 | ` *  digit or blank, FALSE otherwise.` |
|      - | 9061 | ` */` |
|     18 | 9062 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9063 | `{` |
|      - | 9064 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9065 | `	int nLen;` |
|     19 | 9066 | `	if( nArg < 1 ){` |
|      - | 9067 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9068 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9069 | `		return PH7_OK;` |
|      - | 9070 | `	}` |
|      - | 9071 | `	/* Extract the target string */` |
|     19 | 9072 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 9073 | `	zEnd = &zIn[nLen];` |
|     19 | 9074 | `	if( nLen < 1 ){` |
|      - | 9075 | `		/* Empty string,return FALSE */` |
|      3 | 9076 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9077 | `		return PH7_OK;` |
|      - | 9078 | `	}` |
|      - | 9079 | `	/* Perform the requested operation */` |
|     38 | 9080 | `	for(;;){` |
|     77 | 9081 | `		if( zIn >= zEnd ){` |
|      - | 9082 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 9083 | `			ph7_result_bool(pCtx,1);` |
|      9 | 9084 | `			return PH7_OK;` |
|      - | 9085 | `		}` |
|     69 | 9086 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9087 | `			/* UTF-8 stream  */` |
|    ! 0 | 9088 | `			break;` |
|      - | 9089 | `		}` |
|     69 | 9090 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 9091 | `			break;` |
|      - | 9092 | `		}` |
|      - | 9093 | `		/* Point to the next character */` |
|     61 | 9094 | `		zIn++;` |
|      1 | 9095 | `	}` |
|      - | 9096 | `	/* The test failed,return FALSE */` |
|      9 | 9097 | `	ph7_result_bool(pCtx,0);` |
|      9 | 9098 | `	return PH7_OK;` |
|     10 | 9099 | `}` |
|      - | 9100 | `/*` |
|      - | 9101 | ` * bool ctype_space(string $text)` |
|      - | 9102 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 9103 | ` * Parameters` |
|      - | 9104 | ` *  $text` |
|      - | 9105 | ` *   The tested string.` |
|      - | 9106 | ` * Return` |
|      - | 9107 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 9108 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 9109 | ` *  and form feed characters.` |
|      - | 9110 | ` */` |
|  67580 | 9111 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 9112 | `{` |
|      - | 9113 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9114 | `	int nLen;` |
|  67585 | 9115 | `	if( nArg < 1 ){` |
|      - | 9116 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9117 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9118 | `		return PH7_OK;` |
|      - | 9119 | `	}` |
|      - | 9120 | `	/* Extract the target string */` |
|  67585 | 9121 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  67585 | 9122 | `	zEnd = &zIn[nLen];` |
|  67585 | 9123 | `	if( nLen < 1 ){` |
|      - | 9124 | `		/* Empty string,return FALSE */` |
|      3 | 9125 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9126 | `		return PH7_OK;` |
|      - | 9127 | `	}` |
|      - | 9128 | `	/* Perform the requested operation */` |
|  34728 | 9129 | `	for(;;){` |
|  69417 | 9130 | `		if( zIn >= zEnd ){` |
|      - | 9131 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1815 | 9132 | `			ph7_result_bool(pCtx,1);` |
|   1815 | 9133 | `			return PH7_OK;` |
|      - | 9134 | `		}` |
|  67607 | 9135 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9136 | `			/* UTF-8 stream  */` |
|    ! 0 | 9137 | `			break;` |
|      - | 9138 | `		}` |
|  67607 | 9139 | `		if( !SyisSpace(zIn[0]) ){` |
|  65773 | 9140 | `			break;` |
|      - | 9141 | `		}` |
|      - | 9142 | `		/* Point to the next character */` |
|   1839 | 9143 | `		zIn++;` |
|      5 | 9144 | `	}` |
|      - | 9145 | `	/* The test failed,return FALSE */` |
|  65773 | 9146 | `	ph7_result_bool(pCtx,0);` |
|  65773 | 9147 | `	return PH7_OK;` |
|  33817 | 9148 | `}` |
|      - | 9149 | `/*` |
|      - | 9150 | ` * bool ctype_lower(string $text)` |
|      - | 9151 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 9152 | ` * Parameters` |
|      - | 9153 | ` *  $text` |
|      - | 9154 | ` *   The tested string.` |
|      - | 9155 | ` * Return` |
|      - | 9156 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 9157 | ` */` |
|     16 | 9158 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9159 | `{` |
|      - | 9160 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9161 | `	int nLen;` |
|     17 | 9162 | `	if( nArg < 1 ){` |
|      - | 9163 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9164 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9165 | `		return PH7_OK;` |
|      - | 9166 | `	}` |
|      - | 9167 | `	/* Extract the target string */` |
|     17 | 9168 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9169 | `	zEnd = &zIn[nLen];` |
|     17 | 9170 | `	if( nLen < 1 ){` |
|      - | 9171 | `		/* Empty string,return FALSE */` |
|      3 | 9172 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9173 | `		return PH7_OK;` |
|      - | 9174 | `	}` |
|      - | 9175 | `	/* Perform the requested operation */` |
|     27 | 9176 | `	for(;;){` |
|     55 | 9177 | `		if( zIn >= zEnd ){` |
|      - | 9178 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 9179 | `			ph7_result_bool(pCtx,1);` |
|      5 | 9180 | `			return PH7_OK;` |
|      - | 9181 | `		}` |
|     51 | 9182 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 9183 | `			break;` |
|      - | 9184 | `		}` |
|      - | 9185 | `		/* Point to the next character */` |
|     41 | 9186 | `		zIn++;` |
|      1 | 9187 | `	}` |
|      - | 9188 | `	/* The test failed,return FALSE */` |
|     11 | 9189 | `	ph7_result_bool(pCtx,0);` |
|     11 | 9190 | `	return PH7_OK;` |
|      9 | 9191 | `}` |
|      - | 9192 | `/*` |
|      - | 9193 | ` * bool ctype_upper(string $text)` |
|      - | 9194 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 9195 | ` * Parameters` |
|      - | 9196 | ` *  $text` |
|      - | 9197 | ` *   The tested string.` |
|      - | 9198 | ` * Return` |
|      - | 9199 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 9200 | ` */` |
|     16 | 9201 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9202 | `{` |
|      - | 9203 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9204 | `	int nLen;` |
|     17 | 9205 | `	if( nArg < 1 ){` |
|      - | 9206 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9207 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9208 | `		return PH7_OK;` |
|      - | 9209 | `	}` |
|      - | 9210 | `	/* Extract the target string */` |
|     17 | 9211 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9212 | `	zEnd = &zIn[nLen];` |
|     17 | 9213 | `	if( nLen < 1 ){` |
|      - | 9214 | `		/* Empty string,return FALSE */` |
|      3 | 9215 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9216 | `		return PH7_OK;` |
|      - | 9217 | `	}` |
|      - | 9218 | `	/* Perform the requested operation */` |
|     28 | 9219 | `	for(;;){` |
|     57 | 9220 | `		if( zIn >= zEnd ){` |
|      - | 9221 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 9222 | `			ph7_result_bool(pCtx,1);` |
|      5 | 9223 | `			return PH7_OK;` |
|      - | 9224 | `		}` |
|     53 | 9225 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 9226 | `			break;` |
|      - | 9227 | `		}` |
|      - | 9228 | `		/* Point to the next character */` |
|     43 | 9229 | `		zIn++;` |
|      1 | 9230 | `	}` |
|      - | 9231 | `	/* The test failed,return FALSE */` |
|     11 | 9232 | `	ph7_result_bool(pCtx,0);` |
|     11 | 9233 | `	return PH7_OK;` |
|      9 | 9234 | `}` |
|      - | 9235 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 9236 | `/*` |
|      - | 9237 | ` * Section:` |
|      - | 9238 | ` *    URL handling Functions.` |
|      - | 9239 | ` * Status:` |
|      - | 9240 | ` *    Stable.` |
|      - | 9241 | ` */` |
|      - | 9242 | `/*` |
|      - | 9243 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 9244 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 9245 | ` */` |
|   1026 | 9246 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 9247 | `{` |
|      - | 9248 | `	/* Store in the call context result buffer */` |
|   1028 | 9249 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 9250 | `	return SXRET_OK;` |
|      2 | 9251 | `}` |
|      - | 9252 | `/*` |
|      - | 9253 | ` * string base64_encode(string $data)` |
|      - | 9254 | ` * string convert_uuencode(string $data)` |
|      - | 9255 | ` *  Encodes data with MIME base64` |
|      - | 9256 | ` * Parameter` |
|      - | 9257 | ` *  $data` |
|      - | 9258 | ` *    Data to encode` |
|      - | 9259 | ` * Return` |
|      - | 9260 | ` *  Encoded data or FALSE on failure.` |
|      - | 9261 | ` */` |
|      6 | 9262 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9263 | `{` |
|      - | 9264 | `	const char *zIn;` |
|      - | 9265 | `	int nLen;` |
|      7 | 9266 | `	if( nArg < 1 ){` |
|      - | 9267 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9268 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9269 | `		return PH7_OK;` |
|      - | 9270 | `	}` |
|      - | 9271 | `	/* Extract the input string */` |
|      7 | 9272 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 9273 | `	if( nLen < 1 ){` |
|      - | 9274 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9275 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9276 | `		return PH7_OK;` |
|      - | 9277 | `	}` |
|      - | 9278 | `	/* Perform the BASE64 encoding */` |
|      7 | 9279 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 9280 | `	return PH7_OK;` |
|      4 | 9281 | `}` |
|      - | 9282 | `/*` |
|      - | 9283 | ` * string base64_decode(string $data)` |
|      - | 9284 | ` * string convert_uudecode(string $data)` |
|      - | 9285 | ` *  Decodes data encoded with MIME base64` |
|      - | 9286 | ` * Parameter` |
|      - | 9287 | ` *  $data` |
|      - | 9288 | ` *    Encoded data.` |
|      - | 9289 | ` * Return` |
|      - | 9290 | ` *  Returns the original data or FALSE on failure.` |
|      - | 9291 | ` */` |
|     34 | 9292 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 9293 | `{` |
|      - | 9294 | `	const char *zIn;` |
|      - | 9295 | `	int nLen;` |
|     36 | 9296 | `	if( nArg < 1 ){` |
|      - | 9297 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9298 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9299 | `		return PH7_OK;` |
|      - | 9300 | `	}` |
|      - | 9301 | `	/* Extract the input string */` |
|     36 | 9302 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 9303 | `	if( nLen < 1 ){` |
|      - | 9304 | `		/* Nothing to process,return FALSE */` |
|      3 | 9305 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9306 | `		return PH7_OK;` |
|      - | 9307 | `	}` |
|      - | 9308 | `	/* Perform the BASE64 decoding */` |
|     34 | 9309 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 9310 | `	return PH7_OK;` |
|     19 | 9311 | `}` |
|      - | 9312 | `/*` |
|      - | 9313 | ` * string urlencode(string $str)` |
|      - | 9314 | ` *  URL encoding` |
|      - | 9315 | ` * Parameter` |
|      - | 9316 | ` *  $data` |
|      - | 9317 | ` *   Input string.` |
|      - | 9318 | ` * Return` |
|      - | 9319 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 9320 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 9321 | ` *  encoded as plus (+) signs.` |
|      - | 9322 | ` */` |
|      4 | 9323 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9324 | `{` |
|      - | 9325 | `	const char *zIn;` |
|      - | 9326 | `	int nLen;` |
|      5 | 9327 | `	if( nArg < 1 ){` |
|      - | 9328 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9329 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9330 | `		return PH7_OK;` |
|      - | 9331 | `	}` |
|      - | 9332 | `	/* Extract the input string */` |
|      5 | 9333 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 9334 | `	if( nLen < 1 ){` |
|      - | 9335 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9336 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9337 | `		return PH7_OK;` |
|      - | 9338 | `	}` |
|      - | 9339 | `	/* Perform the URL encoding */` |
|      5 | 9340 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 9341 | `	return PH7_OK;` |
|      3 | 9342 | `}` |
|      - | 9343 | `/*` |
|      - | 9344 | ` * string urldecode(string $str)` |
|      - | 9345 | ` *  Decodes any %## encoding in the given string.` |
|      - | 9346 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 9347 | ` * Parameter` |
|      - | 9348 | ` *  $data` |
|      - | 9349 | ` *    Input string.` |
|      - | 9350 | ` * Return` |
|      - | 9351 | ` *  Decoded URL or FALSE on failure.` |
|      - | 9352 | ` */` |
|      6 | 9353 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9354 | `{` |
|      - | 9355 | `	const char *zIn;` |
|      - | 9356 | `	int nLen;` |
|      7 | 9357 | `	if( nArg < 1 ){` |
|      - | 9358 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9359 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9360 | `		return PH7_OK;` |
|      - | 9361 | `	}` |
|      - | 9362 | `	/* Extract the input string */` |
|      7 | 9363 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 9364 | `	if( nLen < 1 ){` |
|      - | 9365 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9366 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9367 | `		return PH7_OK;` |
|      - | 9368 | `	}` |
|      - | 9369 | `	/* Perform the URL decoding */` |
|      7 | 9370 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 9371 | `	return PH7_OK;` |
|      4 | 9372 | `}` |
|      - | 9373 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9374 | `/* Table of the built-in functions */` |
|      - | 9375 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 9376 | `	   /* Variable handling functions */` |
|      - | 9377 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 9378 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 9379 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 9380 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 9381 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 9382 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 9383 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 9384 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 9385 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 9386 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 9387 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 9388 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 9389 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 9390 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 9391 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 9392 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 9393 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 9394 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 9395 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 9396 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 9397 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9398 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 9399 | `	   /* Math functions */` |
|      - | 9400 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 9401 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 9402 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 9403 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 9404 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 9405 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 9406 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 9407 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 9408 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 9409 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 9410 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 9411 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 9412 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 9413 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 9414 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 9415 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 9416 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 9417 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 9418 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 9419 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 9420 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 9421 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 9422 | `	{ "round",    PH7_builtin_round        },` |
|      - | 9423 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 9424 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 9425 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 9426 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 9427 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 9428 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 9429 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 9430 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 9431 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 9432 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9433 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9434 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 9435 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9436 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9437 | `	   /* String handling functions */` |
|      - | 9438 |  |
|      - | 9439 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 9440 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 9441 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 9442 | `	{ "substr_replace",  PH7_builtin_substr_replace },` |
|      - | 9443 | `	{ "levenshtein",     PH7_builtin_levenshtein },` |
|      - | 9444 | `	{ "similar_text",    PH7_builtin_similar_text },` |
|      - | 9445 | `	{ "str_word_count",  PH7_builtin_str_word_count },` |
|      - | 9446 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 9447 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 9448 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 9449 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 9450 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 9451 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 9452 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 9453 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 9454 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 9455 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 9456 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 9457 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 9458 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 9459 | `	{ "strnatcmp"  , PH7_builtin_strnatcmp  },` |
|      - | 9460 | `	{ "strnatcasecmp", PH7_builtin_strnatcmp },` |
|      - | 9461 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 9462 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 9463 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 9464 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 9465 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 9466 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 9467 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 9468 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 9469 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 9470 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 9471 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 9472 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 9473 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 9474 | `	{ "mb_strtolower",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */` |
|      - | 9475 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 9476 | `	{ "mb_strtoupper",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */` |
|      - | 9477 | `	{ "mb_strlen",    PH7_builtin_mb_strlen_f },` |
|      - | 9478 | `	{ "mb_substr",    PH7_builtin_mb_substr_f },` |
|      - | 9479 | `	{ "mb_convert_case", PH7_builtin_mb_convert_case_f },` |
|      - | 9480 | `	{ "mb_strpos",    PH7_builtin_mb_strpos_f },` |
|      - | 9481 | `	{ "mb_stripos",   PH7_builtin_mb_strpos_f },` |
|      - | 9482 | `	{ "mb_strrpos",   PH7_builtin_mb_strpos_f },` |
|      - | 9483 | `	{ "mb_str_split", PH7_builtin_mb_str_split_f },` |
|      - | 9484 | `	{ "mb_internal_encoding", PH7_builtin_mb_internal_encoding_f },` |
|      - | 9485 | `	{ "mb_check_encoding",    PH7_builtin_mb_check_encoding_f },` |
|      - | 9486 | `	{ "mb_strwidth",  PH7_builtin_mb_strwidth_f },` |
|      - | 9487 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 9488 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 9489 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 9490 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 9491 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 9492 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 9493 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 9494 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 9495 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 9496 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 9497 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 9498 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 9499 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 9500 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 9501 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 9502 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 9503 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - | 9504 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - | 9505 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - | 9506 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 9507 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9508 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9509 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 9510 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 9511 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 9512 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 9513 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9514 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9515 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 9516 |  |
|      - | 9517 |  |
|      - | 9518 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 9519 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 9520 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 9521 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 9522 | `	{ "hash",         PH7_builtin_hash      },` |
|      - | 9523 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|      - | 9524 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|      - | 9525 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|      - | 9526 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 9527 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|      - | 9528 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|      - | 9529 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|      - | 9530 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|      - | 9531 | `	{ "filter_var",            PH7_builtin_filter_var },` |
|      - | 9532 | `	{ "filter_input",          PH7_builtin_filter_input },` |
|      - | 9533 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9534 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9535 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 9536 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 9537 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9538 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9539 |  |
|      - | 9540 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 9541 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 9542 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 9543 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 9544 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 9545 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 9546 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 9547 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 9548 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 9549 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 9550 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 9551 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 9552 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9553 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9554 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 9555 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9556 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9557 |  |
|      - | 9558 | `	         /* Ctype functions */` |
|      - | 9559 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 9560 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 9561 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 9562 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 9563 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 9564 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 9565 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 9566 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 9567 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 9568 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 9569 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 9570 | `	         /* Time functions */` |
|      - | 9571 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 9572 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 9573 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 9574 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 9575 | `	{ "date",        PH7_builtin_date         },` |
|      - | 9576 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 9577 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 9578 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 9579 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 9580 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 9581 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 9582 | `	{ "date_default_timezone_get", PH7_builtin_date_default_timezone_get },` |
|      - | 9583 | `	{ "date_default_timezone_set", PH7_builtin_date_default_timezone_set },` |
|      - | 9584 | `	        /* URL functions */` |
|      - | 9585 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 9586 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 9587 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 9588 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 9589 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 9590 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 9591 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 9592 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 9593 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9594 | `};` |
|      - | 9595 | `/*` |
|      - | 9596 | ` * Register the built-in functions defined above,the array functions` |
|      - | 9597 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 9598 | ` */` |
|   3342 | 9599 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      5 | 9600 | `{` |
|      - | 9601 | `	sxu32 n;` |
| 621617 | 9602 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 618275 | 9603 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 309140 | 9604 | `	}` |
|      - | 9605 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   3347 | 9606 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 9607 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   3347 | 9608 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   3347 | 9609 | `}` |
|      - | 9610 |  |
