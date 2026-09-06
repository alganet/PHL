# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4359/5037 lines (86.54%)

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
| 242242 |   18 | `PH7_PRIVATE sxi32 PH7_IntArgResolve(` |
|      - |   19 | `	ph7_context *pCtx,` |
|      - |   20 | `	ph7_value *pArg,` |
|      - |   21 | `	const char *zFunc,` |
|      - |   22 | `	int iArgNum,` |
|      - |   23 | `	const char *zParamName,` |
|      - |   24 | `	const char *zTypeStr,` |
|      - |   25 | `	sxi64 *pOut` |
|      5 |   26 | `){` |
| 242247 |   27 | `	if( ph7_value_is_null(pArg) ){` |
|    ! 0 |   28 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |   29 | `			"%s(): Passing null to parameter #%d (%s) of type %s is deprecated",` |
|    ! 0 |   30 | `			zFunc,iArgNum,zParamName,zTypeStr` |
|      - |   31 | `			);` |
|    ! 0 |   32 | `		*pOut = 0;` |
|    ! 0 |   33 | `		return PH7_OK;` |
|      - |   34 | `	}` |
| 242247 |   35 | `	if( ph7_value_is_float(pArg) ){` |
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
| 242241 |   56 | `	if( ph7_value_is_string(pArg) ){` |
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
| 242227 |   98 | `	if( !ph7_value_is_int(pArg) && !ph7_value_is_bool(pArg) ){` |
|      - |   99 | `		/* Arrays, resources and objects: php names the class for objects */` |
|      5 |  100 | `		const char *zType = ph7_type_name(pArg);` |
|      5 |  101 | `		if( ph7_value_is_object(pArg) ){` |
|      3 |  102 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|      3 |  103 | `			if( pInst && pInst->pClass ){` |
|      3 |  104 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      1 |  105 | `			}` |
|      1 |  106 | `		}` |
|      7 |  107 | `		return PH7_VmThrowException(pCtx,` |
|      - |  108 | `			"TypeError",` |
|      - |  109 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|      2 |  110 | `			zFunc,iArgNum,zParamName,zTypeStr,zType` |
|      - |  111 | `			);` |
|      - |  112 | `	}` |
| 242223 |  113 | `	*pOut = ph7_value_to_int64(pArg);` |
| 242223 |  114 | `	return PH7_OK;` |
| 121126 |  115 | `}` |
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
|    650 |  271 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  272 | `{` |
|    654 |  273 | `	int res = 0; /* Assume false by default */` |
|    654 |  274 | `	if( nArg > 0 ){` |
|    654 |  275 | `		res = ph7_value_is_array(apArg[0]);` |
|    325 |  276 | `	}` |
|      - |  277 | `	/* Query result */` |
|    654 |  278 | `	ph7_result_bool(pCtx,res);` |
|    654 |  279 | `	return PH7_OK;` |
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
|     16 |  388 | `static int PH7_builtin_boolval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  389 | `{` |
|      - |  390 | `	int bVal;` |
|     18 |  391 | `	if( nArg != 1 ){` |
|      4 |  392 | `		return PH7_VmThrowException(pCtx,` |
|      - |  393 | `			"ArgumentCountError",` |
|      - |  394 | `			"boolval() expects exactly 1 argument, %d given",` |
|      1 |  395 | `			nArg` |
|      - |  396 | `			);` |
|      - |  397 | `	}` |
|      - |  398 | `	/* Perform the cast */` |
|     15 |  399 | `	bVal = ph7_value_to_bool(apArg[0]);` |
|     15 |  400 | `	ph7_result_bool(pCtx,bVal);` |
|     15 |  401 | `	return PH7_OK;` |
|     10 |  402 | `}` |
|      - |  403 | `/*` |
|      - |  404 | ` * bool empty($var)` |
|      - |  405 | ` *  Determine whether a variable is empty.` |
|      - |  406 | ` * Parameters` |
|      - |  407 | ` *   $var: The variable being checked.` |
|      - |  408 | ` * Return` |
|      - |  409 | ` *  0 if var has a non-empty and non-zero value.1 otherwise.` |
|      - |  410 | ` */` |
|  34046 |  411 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  412 | `{` |
|  34051 |  413 | `	int res = 1; /* Assume empty by default */` |
|  34051 |  414 | `	if( nArg > 0 ){` |
|  34049 |  415 | `		res = ph7_value_is_empty(apArg[0]);` |
|  17022 |  416 | `	}` |
|  34051 |  417 | `	ph7_result_bool(pCtx,res);` |
|  34051 |  418 | `	return PH7_OK;` |
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
| 233938 |  461 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  462 | `{` |
| 233943 |  463 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"substr",1,"$string"); }` |
|      - |  464 | `	const char *zSource,*zOfft;` |
|      - |  465 | `	int nOfft,nLen,nSrcLen;` |
| 233943 |  466 | `	if( nArg < 2 ){` |
|      - |  467 | `		/* return FALSE */` |
|    ! 0 |  468 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  469 | `		return PH7_OK;` |
|      - |  470 | `	}` |
|      - |  471 | `	/* Extract the target string */` |
| 233943 |  472 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 233943 |  473 | `	if( nSrcLen < 1 ){` |
|      - |  474 | `		/* Empty string,return FALSE */` |
|  12625 |  475 | `		ph7_result_bool(pCtx,0);` |
|  12625 |  476 | `		return PH7_OK;` |
|      - |  477 | `	}` |
| 221323 |  478 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  479 | `	/* Extract the offset */` |
|      - |  480 | `	{` |
| 221323 |  481 | `		sxi64 iTmp = 0;` |
| 221323 |  482 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"substr",2,"$offset","int",&iTmp);` |
| 221323 |  483 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  484 | `			return rcArg;` |
|      - |  485 | `		}` |
| 221323 |  486 | `		nOfft = (int)iTmp;` |
|      - |  487 | `	}` |
| 221323 |  488 | `	if( nOfft < 0 ){` |
|  33139 |  489 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  33139 |  490 | `		if( zOfft < zSource ){` |
|      - |  491 | `			/* Invalid offset */` |
|      5 |  492 | `			ph7_result_bool(pCtx,0);` |
|      5 |  493 | `			return PH7_OK;` |
|      - |  494 | `		}` |
|  33135 |  495 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  33135 |  496 | `		nOfft = (int)(zOfft-zSource);` |
| 204754 |  497 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  498 | `		/* Invalid offset */` |
|    247 |  499 | `		ph7_result_bool(pCtx,0);` |
|    247 |  500 | `		return PH7_OK;` |
|    ! 0 |  501 | `	}else{` |
| 187947 |  502 | `		zOfft = &zSource[nOfft];` |
| 187947 |  503 | `		nLen = nSrcLen - nOfft;` |
|      - |  504 | `	}` |
| 221077 |  505 | `	if( nArg > 2 ){` |
|      - |  506 | `		/* Extract the length */` |
| 181973 |  507 | `		nLen = ph7_value_to_int(apArg[2]);` |
| 181973 |  508 | `		if( nLen == 0 ){` |
|      - |  509 | `			/* Invalid length,return an empty string */` |
|      5 |  510 | `			ph7_result_string(pCtx,"",0);` |
|      5 |  511 | `			return PH7_OK;` |
| 181969 |  512 | `		}else if( nLen < 0 ){` |
|  33069 |  513 | `			nLen = nSrcLen + nLen - nOfft;` |
|  33069 |  514 | `			if( nLen < 1 ){` |
|      - |  515 | `				/* Invalid  length */` |
|      3 |  516 | `				nLen = nSrcLen - nOfft;` |
|      1 |  517 | `			}` |
|  16532 |  518 | `		}` |
| 181969 |  519 | `		if( nLen + nOfft > nSrcLen ){` |
|      - |  520 | `			/* Invalid length */` |
|   6073 |  521 | `			nLen = nSrcLen - nOfft;` |
|   3034 |  522 | `		}` |
|  90982 |  523 | `	}` |
|      - |  524 | `	/* Return the substring */` |
| 221073 |  525 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 221073 |  526 | `	return PH7_OK;` |
| 116974 |  527 | `}` |
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
| 300062 |  720 | `static void StrNullArgNotice(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNum,const char *zParamName)` |
|      5 |  721 | `{` |
| 300067 |  722 | `	if( ph7_value_is_null(pArg) ){` |
|     25 |  723 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |  724 | `			"%s(): Passing null to parameter #%d (%s) of type string is deprecated",` |
|      8 |  725 | `			zFunc,iArgNum,zParamName);` |
|      8 |  726 | `	}` |
| 300067 |  727 | `}` |
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
|     68 |  923 | `static int PH7_builtin_substr_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  924 | `{` |
|      - |  925 | `	ph7_value sStrTmp,sReplTmp;` |
|     69 |  926 | `	const char *zStr = 0,*zRepl = 0;` |
|     69 |  927 | `	int nLen = 0,nRepl = 0;` |
|      - |  928 | `	int bLenGiven;` |
|     69 |  929 | `	sxi64 f = 0,l = 0;` |
|      - |  930 | `	sxi32 rc;` |
|     69 |  931 | `	if( nArg < 3 ){` |
|      7 |  932 | `		return PH7_VmThrowException(pCtx,` |
|      - |  933 | `			"ArgumentCountError",` |
|      - |  934 | `			"substr_replace() expects at least 3 arguments, %d given",` |
|      2 |  935 | `			nArg` |
|      - |  936 | `			);` |
|      - |  937 | `	}` |
|      - |  938 | `	/* $length counts as given unless absent or null (php: ?null semantics) */` |
|     65 |  939 | `	bLenGiven = (nArg > 3 && !ph7_value_is_null(apArg[3]));` |
|      - |  940 | `	/* php ZPP validates all four args, in order, before the body runs: the` |
|      - |  941 | `	 * non-array forms resolve here (null deprecation, __toString objects,` |
|      - |  942 | `	 * numeric strings), arrays pass through to the per-mode handling. */` |
|     65 |  943 | `	PH7_MemObjInit(pCtx->pVm,&sStrTmp);` |
|     65 |  944 | `	PH7_MemObjInit(pCtx->pVm,&sReplTmp);` |
|     65 |  945 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     49 |  946 | `		rc = StrPredicateResolveArg(pCtx,apArg[0],"substr_replace",1,"$string","array\|string",` |
|      - |  947 | `			"substr_replace(): Passing null to parameter #1 ($string) "` |
|      - |  948 | `			"of type array\|string is deprecated",` |
|      - |  949 | `			&sStrTmp,&zStr,&nLen);` |
|     49 |  950 | `		if( rc != PH7_OK ) goto out;` |
|     23 |  951 | `	}` |
|     63 |  952 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|     55 |  953 | `		rc = StrPredicateResolveArg(pCtx,apArg[1],"substr_replace",2,"$replace","array\|string",` |
|      - |  954 | `			"substr_replace(): Passing null to parameter #2 ($replace) "` |
|      - |  955 | `			"of type array\|string is deprecated",` |
|      - |  956 | `			&sReplTmp,&zRepl,&nRepl);` |
|     55 |  957 | `		if( rc != PH7_OK ) goto out;` |
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
|     32 | 1088 | `out:` |
|     65 | 1089 | `	PH7_MemObjRelease(&sStrTmp);` |
|     65 | 1090 | `	PH7_MemObjRelease(&sReplTmp);` |
|     65 | 1091 | `	return rc;` |
|     35 | 1092 | `}` |
|      - | 1093 | `/*` |
|      - | 1094 | ` * int levenshtein(string $string1,string $string2[,int $insertion_cost = 1[,int $replacement_cost = 1[,int $deletion_cost = 1]]])` |
|      - | 1095 | ` *  Calculate the Levenshtein distance between two strings, byte per byte` |
|      - | 1096 | ` *  (case-sensitive), with optional per-operation costs. Mirrors PHP's` |
|      - | 1097 | ` *  reference_levdist(): two rolling rows over string2.` |
|      - | 1098 | ` * Return` |
|      - | 1099 | ` *  The minimal number of weighted edit operations turning $string1 into` |
|      - | 1100 | ` *  $string2.` |
|      - | 1101 | ` */` |
|     42 | 1102 | `static int PH7_builtin_levenshtein(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1103 | `{` |
|      - | 1104 | `	static const char *azParam[] = { "$insertion_cost","$replacement_cost","$deletion_cost" };` |
|      - | 1105 | `	const char *zStr1,*zStr2;` |
|     43 | 1106 | `	sxi64 iCostIns = 1,iCostRep = 1,iCostDel = 1;` |
|      - | 1107 | `	sxi64 *p1,*p2,*pTmp;` |
|      - | 1108 | `	sxi64 c0,c1,c2;` |
|      - | 1109 | `	ph7_value sTmp1,sTmp2;` |
|      - | 1110 | `	int nLen1,nLen2;` |
|      - | 1111 | `	int i1,i2;` |
|      - | 1112 | `	sxi32 rc;` |
|      - | 1113 | `	int i;` |
|     43 | 1114 | `	if( nArg < 2 ){` |
|      4 | 1115 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1116 | `			"ArgumentCountError",` |
|      - | 1117 | `			"levenshtein() expects at least 2 arguments, %d given",` |
|      1 | 1118 | `			nArg` |
|      - | 1119 | `			);` |
|      - | 1120 | `	}` |
|      - | 1121 | `	/* $string1/$string2: null deprecates to "", __toString objects resolve,` |
|      - | 1122 | `	 * everything non-stringish is a TypeError (php ZPP weak mode). */` |
|     41 | 1123 | `	PH7_MemObjInit(pCtx->pVm,&sTmp1);` |
|     41 | 1124 | `	PH7_MemObjInit(pCtx->pVm,&sTmp2);` |
|     41 | 1125 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"levenshtein",1,"$string1","string",` |
|      - | 1126 | `		"levenshtein(): Passing null to parameter #1 ($string1) "` |
|      - | 1127 | `		"of type string is deprecated",` |
|      - | 1128 | `		&sTmp1,&zStr1,&nLen1);` |
|     41 | 1129 | `	if( rc != PH7_OK ) goto out;` |
|     39 | 1130 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"levenshtein",2,"$string2","string",` |
|      - | 1131 | `		"levenshtein(): Passing null to parameter #2 ($string2) "` |
|      - | 1132 | `		"of type string is deprecated",` |
|      - | 1133 | `		&sTmp2,&zStr2,&nLen2);` |
|     39 | 1134 | `	if( rc != PH7_OK ) goto out;` |
|      - | 1135 | `	/* Optional integer costs */` |
|     63 | 1136 | `	for( i = 2 ; i < nArg && i < 5 ; i++ ){` |
|      - | 1137 | `		sxi64 iVal;` |
|     37 | 1138 | `		rc = PH7_IntArgResolve(pCtx,apArg[i],"levenshtein",i+1,azParam[i-2],"int",&iVal);` |
|     37 | 1139 | `		if( rc != PH7_OK ) goto out;` |
|     25 | 1140 | `		if( i == 2 ){` |
|     13 | 1141 | `			iCostIns = iVal;` |
|     19 | 1142 | `		}else if( i == 3 ){` |
|      7 | 1143 | `			iCostRep = iVal;` |
|      4 | 1144 | `		}else{` |
|      7 | 1145 | `			iCostDel = iVal;` |
|      - | 1146 | `		}` |
|     13 | 1147 | `	}` |
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
|     20 | 1193 | `out:` |
|     41 | 1194 | `	PH7_MemObjRelease(&sTmp1);` |
|     41 | 1195 | `	PH7_MemObjRelease(&sTmp2);` |
|     41 | 1196 | `	return rc;` |
|     22 | 1197 | `}` |
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
|     28 | 1253 | `static int PH7_builtin_similar_text(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1254 | `{` |
|      - | 1255 | `	const char *zStr1,*zStr2;` |
|      - | 1256 | `	ph7_value sTmp1,sTmp2;` |
|      - | 1257 | `	int nLen1,nLen2;` |
|      - | 1258 | `	int nSim;` |
|      - | 1259 | `	sxi32 rc;` |
|     29 | 1260 | `	if( nArg < 2 ){` |
|      4 | 1261 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1262 | `			"ArgumentCountError",` |
|      - | 1263 | `			"similar_text() expects at least 2 arguments, %d given",` |
|      1 | 1264 | `			nArg` |
|      - | 1265 | `			);` |
|      - | 1266 | `	}` |
|     27 | 1267 | `	PH7_MemObjInit(pCtx->pVm,&sTmp1);` |
|     27 | 1268 | `	PH7_MemObjInit(pCtx->pVm,&sTmp2);` |
|     27 | 1269 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"similar_text",1,"$string1","string",` |
|      - | 1270 | `		"similar_text(): Passing null to parameter #1 ($string1) "` |
|      - | 1271 | `		"of type string is deprecated",` |
|      - | 1272 | `		&sTmp1,&zStr1,&nLen1);` |
|     27 | 1273 | `	if( rc != PH7_OK ) goto out;` |
|     25 | 1274 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"similar_text",2,"$string2","string",` |
|      - | 1275 | `		"similar_text(): Passing null to parameter #2 ($string2) "` |
|      - | 1276 | `		"of type string is deprecated",` |
|      - | 1277 | `		&sTmp2,&zStr2,&nLen2);` |
|     25 | 1278 | `	if( rc != PH7_OK ) goto out;` |
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
|     13 | 1298 | `out:` |
|     27 | 1299 | `	PH7_MemObjRelease(&sTmp1);` |
|     27 | 1300 | `	PH7_MemObjRelease(&sTmp2);` |
|     27 | 1301 | `	return rc;` |
|     15 | 1302 | `}` |
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
|     52 | 1314 | `static int PH7_builtin_str_word_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1315 | `{` |
|      - | 1316 | `	const char *zIn,*zEnd,*zPtr;` |
|     53 | 1317 | `	ph7_value *pArray = 0,*pValue = 0;` |
|      - | 1318 | `	ph7_value sTmp,sListTmp;` |
|      - | 1319 | `	char aMask[256];` |
|     53 | 1320 | `	int bMask = 0;` |
|     53 | 1321 | `	int iFormat = 0;` |
|     53 | 1322 | `	int nCount = 0;` |
|      - | 1323 | `	int nLen;` |
|      - | 1324 | `	sxi32 rc;` |
|     53 | 1325 | `	if( nArg < 1 ){` |
|      4 | 1326 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1327 | `			"ArgumentCountError",` |
|      - | 1328 | `			"str_word_count() expects at least 1 argument, %d given",` |
|      1 | 1329 | `			nArg` |
|      - | 1330 | `			);` |
|      - | 1331 | `	}` |
|     51 | 1332 | `	PH7_MemObjInit(pCtx->pVm,&sTmp);` |
|     51 | 1333 | `	PH7_MemObjInit(pCtx->pVm,&sListTmp);` |
|     51 | 1334 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_word_count",1,"$string","string",` |
|      - | 1335 | `		"str_word_count(): Passing null to parameter #1 ($string) "` |
|      - | 1336 | `		"of type string is deprecated",` |
|      - | 1337 | `		&sTmp,&zIn,&nLen);` |
|     51 | 1338 | `	if( rc != PH7_OK ) goto out;` |
|     49 | 1339 | `	if( nArg > 1 ){` |
|      - | 1340 | `		sxi64 iVal;` |
|     35 | 1341 | `		rc = PH7_IntArgResolve(pCtx,apArg[1],"str_word_count",2,"$format","int",&iVal);` |
|     37 | 1342 | `		if( rc != PH7_OK ) goto out;` |
|     33 | 1343 | `		if( iVal < 0 \|\| iVal > 2 ){` |
|      5 | 1344 | `			rc = PH7_VmThrowException(pCtx,` |
|      - | 1345 | `				"ValueError",` |
|      - | 1346 | `				"str_word_count(): Argument #2 ($format) must be a valid format value"` |
|      - | 1347 | `				);` |
|      5 | 1348 | `			goto out;` |
|      - | 1349 | `		}` |
|     29 | 1350 | `		iFormat = (int)iVal;` |
|     14 | 1351 | `	}` |
|     43 | 1352 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      - | 1353 | `		/* $characters is ?string: null (skipped above) simply keeps the` |
|      - | 1354 | `		 * default word set, no deprecation. */` |
|      - | 1355 | `		const char *zList;` |
|      - | 1356 | `		int nList;` |
|     17 | 1357 | `		rc = StrPredicateResolveArg(pCtx,apArg[2],"str_word_count",3,"$characters","?string",` |
|      - | 1358 | `			"" /* unreachable: null never gets here */,` |
|      - | 1359 | `			&sListTmp,&zList,&nList);` |
|     17 | 1360 | `		if( rc != PH7_OK ) goto out;` |
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
|     24 | 1422 | `out:` |
|     49 | 1423 | `	PH7_MemObjRelease(&sTmp);` |
|     49 | 1424 | `	PH7_MemObjRelease(&sListTmp);` |
|     49 | 1425 | `	return rc;` |
|     26 | 1426 | `}` |
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
|     15 | 1444 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1445 | `		/* Nothing to split,return null */` |
|      3 | 1446 | `		ph7_result_null(pCtx);` |
|      3 | 1447 | `		return PH7_OK;` |
|      - | 1448 | `	}` |
|      - | 1449 | `	/* initialize/Extract arguments */` |
|     13 | 1450 | `	nSepLen = (int)sizeof("\r\n") - 1;` |
|     13 | 1451 | `	nChunkLen = 76;` |
|     13 | 1452 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 1453 | `	zEnd = &zIn[nLen];` |
|     13 | 1454 | `	if( nArg > 1 ){` |
|      - | 1455 | `		/* Chunk length */` |
|     13 | 1456 | `		nChunkLen = ph7_value_to_int(apArg[1]);` |
|     13 | 1457 | `		if( nChunkLen < 1 ){` |
|      - | 1458 | `			/* PHP 8 throws a catchable ValueError for a non-positive length. */` |
|      3 | 1459 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1460 | `				"chunk_split(): Argument #2 ($length) must be greater than 0");` |
|      - | 1461 | `		}` |
|     11 | 1462 | `		if( nArg > 2 ){` |
|      - | 1463 | `			/* Separator */` |
|      9 | 1464 | `			zSep = ph7_value_to_string(apArg[2],&nSepLen);` |
|      9 | 1465 | `			if( nSepLen < 1 ){` |
|      - | 1466 | `				/* Switch back to the default separator */` |
|      3 | 1467 | `				zSep = "\r\n";` |
|      3 | 1468 | `				nSepLen = (int)sizeof("\r\n") - 1;` |
|      1 | 1469 | `			}` |
|      4 | 1470 | `		}` |
|      5 | 1471 | `	}` |
|      - | 1472 | `	/* Perform the requested operation */` |
|     11 | 1473 | `	if( nChunkLen > nLen ){` |
|      - | 1474 | `		/* Nothing to split,return the string and the separator */` |
|      7 | 1475 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);` |
|      7 | 1476 | `		return PH7_OK;` |
|      - | 1477 | `	}` |
|     17 | 1478 | `	while( zIn < zEnd ){` |
|     13 | 1479 | `		if( nChunkLen > (int)(zEnd-zIn) ){` |
|      3 | 1480 | `			nChunkLen = (int)(zEnd - zIn);` |
|      1 | 1481 | `		}` |
|      - | 1482 | `		/* Append the chunk and the separator */` |
|     13 | 1483 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);` |
|      - | 1484 | `		/* Point beyond the chunk */` |
|     13 | 1485 | `		zIn += nChunkLen;` |
|      1 | 1486 | `	}` |
|      5 | 1487 | `	return PH7_OK;` |
|      8 | 1488 | `}` |
|      - | 1489 | `/*` |
|      - | 1490 | ` * string addslashes(string $str)` |
|      - | 1491 | ` *  Quote string with slashes.` |
|      - | 1492 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1493 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1494 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1495 | ` * Parameter` |
|      - | 1496 | ` *  str: The string to be escaped.` |
|      - | 1497 | ` * Return` |
|      - | 1498 | ` *  Returns the escaped string` |
|      - | 1499 | ` */` |
|     24 | 1500 | `static int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1501 | `{` |
|      - | 1502 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1503 | `	int nLen;` |
|      - | 1504 | `	/* PHP enforces exactly one argument. */` |
|     28 | 1505 | `	if( nArg != 1 ){` |
|      8 | 1506 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1507 | `			"ArgumentCountError",` |
|      - | 1508 | `			"addslashes() expects exactly 1 argument, %d given",` |
|      2 | 1509 | `			nArg` |
|      - | 1510 | `			);` |
|      - | 1511 | `	}` |
|      - | 1512 | `	/* NULL is deprecated and treated as an empty string; other invalid` |
|      - | 1513 | `	 * types still produce a TypeError. */` |
|     22 | 1514 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 1515 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 1516 | `			E_DEPRECATED,` |
|      - | 1517 | `			"addslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - | 1518 | `			);` |
|      - | 1519 | `		/* fall through so conversion below yields empty string */` |
|      1 | 1520 | `	}` |
|      - | 1521 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     29 | 1522 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     29 | 1523 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     18 | 1524 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 1525 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1526 | `			"TypeError",` |
|      - | 1527 | `			"addslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 1528 | `			ph7_type_name(apArg[0])` |
|      - | 1529 | `			);` |
|      - | 1530 | `	}` |
|      - | 1531 | `	/* Convert to string representation first and obtain length. */` |
|     19 | 1532 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 1533 | `	if( nLen < 1 ){` |
|      - | 1534 | `		/* Return the empty string */` |
|      5 | 1535 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1536 | `		return PH7_OK;` |
|      - | 1537 | `	}` |
|     15 | 1538 | `	zEnd = &zIn[nLen];` |
|     15 | 1539 | `	zCur = 0; /* cc warning */` |
|     20 | 1540 | `	for(;;){` |
|     41 | 1541 | `		if( zIn >= zEnd ){` |
|      - | 1542 | `			/* No more input */` |
|     15 | 1543 | `			break;` |
|      - | 1544 | `		}` |
|     27 | 1545 | `		zCur = zIn;` |
|      - | 1546 | `		/* scan until a character that needs escaping (', ", \\, or NUL) */` |
|     89 | 1547 | `		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' && zIn[0] != '\0' ){` |
|     63 | 1548 | `			zIn++;` |
|      1 | 1549 | `		}` |
|     27 | 1550 | `		if( zIn > zCur ){` |
|      - | 1551 | `			/* Append raw contents */` |
|     23 | 1552 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 1553 | `		}` |
|     27 | 1554 | `		if( zIn < zEnd ){` |
|     17 | 1555 | `			int c = zIn[0];` |
|     17 | 1556 | `			if( c == '\0' ){` |
|      - | 1557 | `				/* PHP escapes NUL as "\\0" (two characters) */` |
|      3 | 1558 | `				ph7_result_string(pCtx,"\\0",2);` |
|      2 | 1559 | `			}else{` |
|     15 | 1560 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1561 | `			}` |
|      8 | 1562 | `		}` |
|     27 | 1563 | `		zIn++;` |
|      1 | 1564 | `	}` |
|     15 | 1565 | `	return PH7_OK;` |
|     16 | 1566 | `}` |
|      - | 1567 | `/*` |
|      - | 1568 | ``  * Build a 256-entry membership mask from a PHP charlist, expanding `a..z` `` |
|      - | 1569 | ` * byte ranges exactly like PHP's php_charmask(). On return aMask[c] != 0 iff` |
|      - | 1570 | ` * the byte c belongs to the set. Emits the PHP-exact warnings for the three` |
|      - | 1571 | ` * malformed-range shapes (ph7_context_throw_error_format prepends the active` |
|      - | 1572 | ` * function name, so the messages omit it); on a bad range the surrounding` |
|      - | 1573 | ` * bytes are still added and the scan never aborts. Reads only within` |
|      - | 1574 | ` * [zList, zList+nLen).` |
|      - | 1575 | ` *` |
|      - | 1576 | ` * Use ONLY for the builtins whose charlist expands ranges the way PHP's` |
|      - | 1577 | ` * php_charmask() does: trim/ltrim/rtrim/addcslashes (and quotemeta, whose set` |
|      - | 1578 | ` * is a fixed literal with no ".."). Do NOT route strspn/strcspn/strtok/strpbrk` |
|      - | 1579 | ` * through this — PHP treats their charlists literally, so expanding "a..z" here` |
|      - | 1580 | ` * would be a behavior regression plus spurious "Invalid '..'-range" warnings.` |
|      - | 1581 | ` */` |
|    106 | 1582 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256])` |
|      5 | 1583 | `{` |
|    111 | 1584 | `	const unsigned char *zIn  = (const unsigned char *)zList;` |
|    111 | 1585 | `	const unsigned char *zEnd = zIn + (nLen > 0 ? nLen : 0);` |
|    111 | 1586 | `	SyZero(aMask,256);` |
|    379 | 1587 | `	for( ; zIn < zEnd ; zIn++ ){` |
|    273 | 1588 | `		int c = zIn[0];` |
|    273 | 1589 | `		if( zIn + 3 < zEnd && zIn[1] == '.' && zIn[2] == '.' && zIn[3] >= c ){` |
|      - | 1590 | `			/* Valid incrementing range c..zIn[3] */` |
|     22 | 1591 | `			int hi = zIn[3],k;` |
|    386 | 1592 | `			for( k = c ; k <= hi ; k++ ){` |
|    366 | 1593 | `				aMask[k] = 1;` |
|    184 | 1594 | `			}` |
|     22 | 1595 | `			zIn += 3; /* the loop's ++ then steps past the range end */` |
|    272 | 1596 | `		}else if( zIn + 1 < zEnd && zIn[0] == '.' && zIn[1] == '.' ){` |
|      - | 1597 | `			/* Malformed range: mirror php_charmask's three diagnostics. */` |
|      - | 1598 | `			const char *zMsg;` |
|     20 | 1599 | `			if( (const unsigned char *)zList >= zIn ){` |
|      6 | 1600 | `				zMsg = "no character to the left of '..'";` |
|     18 | 1601 | `			}else if( zIn + 2 >= zEnd ){` |
|      6 | 1602 | `				zMsg = "no character to the right of '..'";` |
|     14 | 1603 | `			}else if( zIn[-1] > zIn[2] ){` |
|     12 | 1604 | `				zMsg = "'..'-range needs to be incrementing";` |
|      7 | 1605 | `			}else{` |
|    ! 0 | 1606 | `				zMsg = 0; /* catch-all (e.g. a..b..c) */` |
|      - | 1607 | `			}` |
|     20 | 1608 | `			if( zMsg ){` |
|     29 | 1609 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      9 | 1610 | `					"Invalid '..'-range, %s",zMsg);` |
|     11 | 1611 | `			}else{` |
|    ! 0 | 1612 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1613 | `					"Invalid '..'-range");` |
|      - | 1614 | `			}` |
|      - | 1615 | `			/* Do not consume the dots: the loop's ++ steps one byte so the` |
|      - | 1616 | `			 * dots are re-scanned as literals, exactly like php_charmask. */` |
|     11 | 1617 | `		}else{` |
|    235 | 1618 | `			aMask[c] = 1;` |
|      - | 1619 | `		}` |
|    139 | 1620 | `	}` |
|    111 | 1621 | `}` |
|      - | 1622 | `/*` |
|      - | 1623 | ` * string addcslashes(string $str,string $charlist)` |
|      - | 1624 | ` *  Quote string with slashes in a C style.` |
|      - | 1625 | ` * Parameter` |
|      - | 1626 | ` *  $str:` |
|      - | 1627 | ` *    The string to be escaped.` |
|      - | 1628 | ` *  $charlist:` |
|      - | 1629 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|      - | 1630 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|      - | 1631 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|      - | 1632 | ` * Return` |
|      - | 1633 | ` *  Returns the escaped string.` |
|      - | 1634 | ` * Note:` |
|      - | 1635 | ` *  Character ranges [i.e: 'A..Z'] are supported (see PH7_BuildCharMask).` |
|      - | 1636 | ` */` |
|     40 | 1637 | `static int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1638 | `{` |
|      - | 1639 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - | 1640 | `	char aMask[256];` |
|      - | 1641 | `	int nLen,nMask;` |
|      - | 1642 | `	/* PHP enforces exactly two arguments. */` |
|     45 | 1643 | `	if( nArg != 2 ){` |
|      8 | 1644 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1645 | `			"ArgumentCountError",` |
|      - | 1646 | `			"addcslashes() expects exactly 2 arguments, %d given",` |
|      2 | 1647 | `			nArg` |
|      - | 1648 | `			);` |
|      - | 1649 | `	}` |
|      - | 1650 | `	/* First argument must be a string-ish value.  NULL is deprecated and` |
|      - | 1651 | `	 * treated as the empty string (PHP 8.1). */` |
|     39 | 1652 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      - | 1653 | `		/* Emit deprecation only once, similar to PHP behaviour. */` |
|      3 | 1654 | `		PH7_VmThrowError(pCtx->pVm,0,/* iErr will be patched to 8192 below */` |
|      - | 1655 | `			E_DEPRECATED,` |
|      - | 1656 | `			"addcslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - | 1657 | `			);` |
|      - | 1658 | `		/* treat as empty string; fall through to conversion logic */` |
|     52 | 1659 | `	} else if( ph7_value_is_array(apArg[0]) \|\|` |
|     51 | 1660 | `	          ph7_value_is_object(apArg[0]) \|\|` |
|     32 | 1661 | `	          ph7_value_is_resource(apArg[0]) ){` |
|      4 | 1662 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1663 | `			"TypeError",` |
|      - | 1664 | `			"addcslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 1665 | `			ph7_type_name(apArg[0])` |
|      - | 1666 | `			);` |
|      - | 1667 | `	}` |
|      - | 1668 | `	/* Second argument must be a string.  NULL is deprecated and treated as` |
|      - | 1669 | `	 * an empty mask per PHP semantics.  Arrays/objects/resources still` |
|      - | 1670 | `	 * trigger a TypeError. */` |
|     37 | 1671 | `	if( ph7_value_is_null(apArg[1]) ){` |
|      3 | 1672 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 1673 | `			E_DEPRECATED,` |
|      - | 1674 | `			"addcslashes(): Passing null to parameter #2 ($characters) of type string is deprecated"` |
|      - | 1675 | `			);` |
|      - | 1676 | `		/* allow through so it becomes empty string below */` |
|     49 | 1677 | `	} else if( ph7_value_is_array(apArg[1]) \|\|` |
|     48 | 1678 | `	          ph7_value_is_object(apArg[1]) \|\|` |
|     30 | 1679 | `	          ph7_value_is_resource(apArg[1]) ){` |
|      4 | 1680 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1681 | `			"TypeError",` |
|      - | 1682 | `			"addcslashes(): Argument #2 ($characters) must be of type string, %s given",` |
|      2 | 1683 | `			ph7_type_name(apArg[1])` |
|      - | 1684 | `			);` |
|      - | 1685 | `	}` |
|      - | 1686 | `	/* Extract the string to process */` |
|     35 | 1687 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1688 | `	/* NULL would never reach here due to the check above. */` |
|     35 | 1689 | `	if( nLen < 1 ){` |
|      - | 1690 | `		/* Empty string returns itself. */` |
|      5 | 1691 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      5 | 1692 | `		return PH7_OK;` |
|      - | 1693 | `	}` |
|      - | 1694 | ``	/* Extract the desired mask and expand any `a..z` ranges into a lookup. */`` |
|     31 | 1695 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|     31 | 1696 | `	PH7_BuildCharMask(pCtx,zMask,nMask,aMask);` |
|     31 | 1697 | `	zEnd = &zIn[nLen];` |
|     31 | 1698 | `	zCur = 0; /* cc warning */` |
|     37 | 1699 | `	for(;;){` |
|     77 | 1700 | `		if( zIn >= zEnd ){` |
|      - | 1701 | `			/* No more input */` |
|     31 | 1702 | `			break;` |
|      - | 1703 | `		}` |
|     49 | 1704 | `		zCur = zIn;` |
|    125 | 1705 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     79 | 1706 | `			zIn++;` |
|      3 | 1707 | `		}` |
|     49 | 1708 | `		if( zIn > zCur ){` |
|      - | 1709 | `			/* Append raw contents */` |
|     43 | 1710 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     20 | 1711 | `		}` |
|     49 | 1712 | `		if( zIn < zEnd ){` |
|      - | 1713 | `			/* Make sure we treat the byte as unsigned to avoid negative values` |
|      - | 1714 | `			 * on platforms where char is signed. */` |
|     29 | 1715 | `			int c = (unsigned char)zIn[0];` |
|      - | 1716 | `			/* Handle special C-like escapes for common control characters first.` |
|      - | 1717 | `			 * PHP outputs "\n" "\r" "\t" "\v" "\f" when those chars are` |
|      - | 1718 | `			 * in the mask. NUL is left to the octal conversion below. */` |
|     29 | 1719 | `			if( c == '\n' ){` |
|      3 | 1720 | `				ph7_result_string(pCtx,"\\n",2);` |
|     28 | 1721 | `			}else if( c == '\r' ){` |
|      3 | 1722 | `				ph7_result_string(pCtx,"\\r",2);` |
|     26 | 1723 | `			}else if( c == '\t' ){` |
|      3 | 1724 | `				ph7_result_string(pCtx,"\\t",2);` |
|     24 | 1725 | `			}else if( c == '\v' ){` |
|      3 | 1726 | `				ph7_result_string(pCtx,"\\v",2);` |
|     22 | 1727 | `			}else if( c == '\f' ){` |
|      3 | 1728 | `				ph7_result_string(pCtx,"\\f",2);` |
|     20 | 1729 | `			}else if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|      - | 1730 | `				/* Convert to octal.  PHP always emits three-digit zero-padded` |
|      - | 1731 | `				 * octal escapes (\001 not \1). */` |
|      7 | 1732 | `				ph7_result_string_format(pCtx,"\\%03o",c);` |
|      4 | 1733 | `			}else{` |
|     13 | 1734 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1735 | `			}` |
|     13 | 1736 | `		}` |
|     49 | 1737 | `		zIn++;` |
|      3 | 1738 | `	}` |
|     31 | 1739 | `	return PH7_OK;` |
|     25 | 1740 | `}` |
|      - | 1741 | `/*` |
|      - | 1742 | ` * string quotemeta(string $str)` |
|      - | 1743 | ` *  Quote meta characters.` |
|      - | 1744 | ` * Parameter` |
|      - | 1745 | ` *  $str:` |
|      - | 1746 | ` *    The string to be escaped.` |
|      - | 1747 | ` * Return` |
|      - | 1748 | ` *  Returns the escaped string.` |
|      - | 1749 | `*/` |
|     10 | 1750 | `static int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1751 | `{` |
|      - | 1752 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1753 | `	char aMask[256];` |
|      - | 1754 | `	int nLen;` |
|     12 | 1755 | `	if( nArg < 1 ){` |
|      - | 1756 | `		/* Nothing to process,retun NULL */` |
|    ! 0 | 1757 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1758 | `		return PH7_OK;` |
|      - | 1759 | `	}` |
|      - | 1760 | `	/* Extract the string to process */` |
|     12 | 1761 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     12 | 1762 | `	if( nLen < 1 ){` |
|      - | 1763 | `		/* Return the empty string */` |
|      3 | 1764 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1765 | `		return PH7_OK;` |
|      - | 1766 | `	}` |
|      - | 1767 | `	/* Fixed meta-character set (no ranges); build the lookup once. */` |
|     10 | 1768 | `	PH7_BuildCharMask(pCtx,".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1,aMask);` |
|     10 | 1769 | `	zEnd = &zIn[nLen];` |
|     10 | 1770 | `	zCur = 0; /* cc warning */` |
|     22 | 1771 | `	for(;;){` |
|     46 | 1772 | `		if( zIn >= zEnd ){` |
|      - | 1773 | `			/* No more input */` |
|     10 | 1774 | `			break;` |
|      - | 1775 | `		}` |
|     38 | 1776 | `		zCur = zIn;` |
|     76 | 1777 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     40 | 1778 | `			zIn++;` |
|      2 | 1779 | `		}` |
|     38 | 1780 | `		if( zIn > zCur ){` |
|      - | 1781 | `			/* Append raw contents */` |
|     20 | 1782 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      9 | 1783 | `		}` |
|     38 | 1784 | `		if( zIn < zEnd ){` |
|     36 | 1785 | `			int c = zIn[0];` |
|     36 | 1786 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|     17 | 1787 | `		}` |
|     38 | 1788 | `		zIn++;` |
|      2 | 1789 | `	}` |
|     10 | 1790 | `	return PH7_OK;` |
|      7 | 1791 | `}` |
|      - | 1792 | `/*` |
|      - | 1793 | ` * string stripslashes(string $str)` |
|      - | 1794 | ` *  Un-quotes a quoted string.` |
|      - | 1795 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1796 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1797 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1798 | ` * Parameter` |
|      - | 1799 | ` *  $str` |
|      - | 1800 | ` *   The input string.` |
|      - | 1801 | ` * Return` |
|      - | 1802 | ` *  Returns a string with backslashes stripped off.` |
|      - | 1803 | ` */` |
|      6 | 1804 | `static int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1805 | `{` |
|      - | 1806 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1807 | `	int nLen;` |
|      7 | 1808 | `	if( nArg < 1 ){` |
|      - | 1809 | `		/* Nothing to process,retun NULL */` |
|    ! 0 | 1810 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1811 | `		return PH7_OK;` |
|      - | 1812 | `	}` |
|      - | 1813 | `	/* Extract the string to process */` |
|      7 | 1814 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 1815 | `	if( zIn == 0 ){` |
|    ! 0 | 1816 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1817 | `		return PH7_OK;` |
|      - | 1818 | `	}` |
|      7 | 1819 | `	zEnd = &zIn[nLen];` |
|      7 | 1820 | `	zCur = 0; /* cc warning */` |
|      - | 1821 | `	/* Encode the string */` |
|      4 | 1822 | `	for(;;){` |
|      9 | 1823 | `		if( zIn >= zEnd ){` |
|      - | 1824 | `			/* No more input */` |
|      5 | 1825 | `			break;` |
|      - | 1826 | `		}` |
|      5 | 1827 | `		zCur = zIn;` |
|     17 | 1828 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 | 1829 | `			zIn++;` |
|      1 | 1830 | `		}` |
|      5 | 1831 | `		if( zIn > zCur ){` |
|      - | 1832 | `			/* Append raw contents */` |
|      5 | 1833 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 1834 | `		}` |
|      5 | 1835 | `		if( &zIn[1] < zEnd ){` |
|      3 | 1836 | `			int c = zIn[1];` |
|      3 | 1837 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - | 1838 | `				/* Ignore the backslash */` |
|      3 | 1839 | `				zIn++;` |
|      1 | 1840 | `			}` |
|      2 | 1841 | `		}else{` |
|      3 | 1842 | `			break;` |
|      - | 1843 | `		}` |
|      1 | 1844 | `	}` |
|      7 | 1845 | `	return PH7_OK;` |
|      4 | 1846 | `}` |
|      - | 1847 | `/*` |
|      - | 1848 | ` * UTF-8-aware HTML entity machinery, shared by htmlspecialchars/htmlentities/` |
|      - | 1849 | ` * htmlspecialchars_decode/html_entity_decode/get_html_translation_table.` |
|      - | 1850 | ` * The implementations live further down in this file, next to the filter_var` |
|      - | 1851 | ` * FULL_SPECIAL_CHARS machinery they reuse (aHtml401Ent[]/FvHtml401Lookup()/` |
|      - | 1852 | ` * FvUtf8Next()). Semantics are byte-exact vs php 8.5.7; PHL is UTF-8-only` |
|      - | 1853 | ` * so every charset argument other than a UTF-8 alias gets PHP's` |
|      - | 1854 | ` * unsupported-charset warning and is treated as UTF-8.` |
|      - | 1855 | ` *` |
|      - | 1856 | ` * Flag model (the PHP-exact ENT_* values, see constant.c): bit 1 = encode/` |
|      - | 1857 | ` * decode single quotes, bit 2 = double quotes (ENT_QUOTES=3, ENT_COMPAT=2,` |
|      - | 1858 | ` * ENT_NOQUOTES=0); bits 16\|32 select the doctype (0=HTML401, 16=XML1,` |
|      - | 1859 | ` * 32=XHTML, 48=HTML5); ENT_IGNORE=4 drops invalid UTF-8 bytes (wins over` |
|      - | 1860 | ` * ENT_SUBSTITUTE=8, which replaces each with U+FFFD; with neither set the` |
|      - | 1861 | ` * whole result collapses to ""); ENT_DISALLOWED=128 substitutes valid but` |
|      - | 1862 | ` * doctype-disallowed codepoints. The shared default is` |
|      - | 1863 | ` * ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 = 11.` |
|      - | 1864 | ` */` |
|      - | 1865 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bAll,int bDoubleEncode);` |
|      - | 1866 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bFull);` |
|      - | 1867 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx);` |
|      - | 1868 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags);` |
|      - | 1869 | `/*` |
|      - | 1870 | ` * string htmlspecialchars(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1871 | ` *                         [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1872 | ` *  Convert the special characters & < > " ' to HTML entities.` |
|      - | 1873 | ` * Return` |
|      - | 1874 | ` *  The escaped string or NULL on failure.` |
|      - | 1875 | ` */` |
|     42 | 1876 | `static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1877 | `{` |
|     43 | 1878 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1879 | `	const char *zIn;` |
|     43 | 1880 | `	int nLen,bDouble = 1;` |
|     43 | 1881 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1882 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 1883 | `		ph7_result_null(pCtx);` |
|      3 | 1884 | `		return PH7_OK;` |
|      - | 1885 | `	}` |
|      - | 1886 | `	/* Extract the target string */` |
|     41 | 1887 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 1888 | `	if( nArg > 1 ){` |
|     35 | 1889 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     17 | 1890 | `	}` |
|     41 | 1891 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     41 | 1892 | `	if( nArg > 3 ){` |
|      7 | 1893 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      3 | 1894 | `	}` |
|     41 | 1895 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,0,bDouble);` |
|     41 | 1896 | `	return PH7_OK;` |
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
|     23 | 1910 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1911 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 1912 | `		ph7_result_null(pCtx);` |
|      3 | 1913 | `		return PH7_OK;` |
|      - | 1914 | `	}` |
|      - | 1915 | `	/* Extract the target string */` |
|     21 | 1916 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     21 | 1917 | `	if( nArg > 1 ){` |
|      9 | 1918 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1919 | `	}` |
|     21 | 1920 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,0);` |
|     21 | 1921 | `	return PH7_OK;` |
|     12 | 1922 | `}` |
|      - | 1923 | `/*` |
|      - | 1924 | ` * array get_html_translation_table(int $table = HTML_SPECIALCHARS` |
|      - | 1925 | ` *      [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 [, string $encoding = "UTF-8"]])` |
|      - | 1926 | ` *  Return the translation table used by htmlspecialchars() (HTML_SPECIALCHARS)` |
|      - | 1927 | ` *  or htmlentities() (HTML_ENTITIES) as character => entity pairs.` |
|      - | 1928 | ` * Return` |
|      - | 1929 | ` *  The translation table as an array or NULL on failure.` |
|      - | 1930 | ` */` |
|     12 | 1931 | `static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1932 | `{` |
|     13 | 1933 | `	int iTable = 0; /* HTML_SPECIALCHARS */` |
|     13 | 1934 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|     13 | 1935 | `	if( nArg > 0 ){` |
|     11 | 1936 | `		iTable = ph7_value_to_int(apArg[0]);` |
|      5 | 1937 | `	}` |
|     13 | 1938 | `	if( nArg > 1 ){` |
|      9 | 1939 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1940 | `	}` |
|     13 | 1941 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     13 | 1942 | `	HtmlTranslationTable(pCtx,iTable,iFlags);` |
|     13 | 1943 | `	return PH7_OK;` |
|      1 | 1944 | `}` |
|      - | 1945 | `/*` |
|      - | 1946 | ` * string htmlentities(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1947 | ` *                     [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1948 | ` *  Convert all applicable characters to HTML entities: the specials plus` |
|      - | 1949 | ` *  every codepoint with an HTML 4.01 named entity (aHtml401Ent[]).` |
|      - | 1950 | ` * Return` |
|      - | 1951 | ` *  The encoded string or NULL on failure.` |
|      - | 1952 | ` */` |
|     30 | 1953 | `static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1954 | `{` |
|     31 | 1955 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1956 | `	const char *zIn;` |
|     31 | 1957 | `	int nLen,bDouble = 1;` |
|     31 | 1958 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1959 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1960 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1961 | `		return PH7_OK;` |
|      - | 1962 | `	}` |
|      - | 1963 | `	/* Extract the target string */` |
|     31 | 1964 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 1965 | `	if( nArg > 1 ){` |
|     19 | 1966 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 | 1967 | `	}` |
|     31 | 1968 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     31 | 1969 | `	if( nArg > 3 ){` |
|      3 | 1970 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      1 | 1971 | `	}` |
|     31 | 1972 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,1,bDouble);` |
|     31 | 1973 | `	return PH7_OK;` |
|     16 | 1974 | `}` |
|      - | 1975 | `/*` |
|      - | 1976 | ` * string html_entity_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1977 | ` *                           [, string $encoding = "UTF-8"]])` |
|      - | 1978 | ` *  Convert HTML entities (named — case-sensitive — and numeric, decimal or` |
|      - | 1979 | ` *  hex) back to their UTF-8 characters. The reverse of htmlentities().` |
|      - | 1980 | ` * Return` |
|      - | 1981 | ` *  The decoded string or NULL on failure.` |
|      - | 1982 | ` */` |
|     58 | 1983 | `static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1984 | `{` |
|     59 | 1985 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1986 | `	const char *zIn;` |
|      - | 1987 | `	int nLen;` |
|     59 | 1988 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1989 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1990 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1991 | `		return PH7_OK;` |
|      - | 1992 | `	}` |
|      - | 1993 | `	/* Extract the target string */` |
|     59 | 1994 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     59 | 1995 | `	if( nArg > 1 ){` |
|     27 | 1996 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     13 | 1997 | `	}` |
|     59 | 1998 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     59 | 1999 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,1);` |
|     59 | 2000 | `	return PH7_OK;` |
|     30 | 2001 | `}` |
|      - | 2002 | `/*` |
|      - | 2003 | ` * int strlen($string)` |
|      - | 2004 | ` *  return the length of the given string.` |
|      - | 2005 | ` * Parameter` |
|      - | 2006 | ` *  string: The string being measured for length.` |
|      - | 2007 | ` * Return` |
|      - | 2008 | ` *  length of the given string.` |
|      - | 2009 | ` */` |
|  14830 | 2010 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2011 | `{` |
|  14835 | 2012 | `	int iLen = 0;` |
|  14835 | 2013 | `	if( nArg > 0 ){` |
|  14835 | 2014 | `		StrNullArgNotice(pCtx,apArg[0],"strlen",1,"$string");` |
|  14835 | 2015 | `		ph7_value_to_string(apArg[0],&iLen);` |
|   7415 | 2016 | `	}` |
|      - | 2017 | `	/* String length */` |
|  14835 | 2018 | `	ph7_result_int(pCtx,iLen);` |
|  14835 | 2019 | `	return PH7_OK;` |
|      5 | 2020 | `}` |
|      - | 2021 | `/*` |
|      - | 2022 | ` * int strcmp(string $str1,string $str2)` |
|      - | 2023 | ` *  Perform a binary safe string comparison.` |
|      - | 2024 | ` * Parameter` |
|      - | 2025 | ` *  str1: The first string` |
|      - | 2026 | ` *  str2: The second string` |
|      - | 2027 | ` * Return` |
|      - | 2028 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2029 | ` *  than str2, and 0 if they are equal.` |
|      - | 2030 | ` */` |
|     72 | 2031 | `static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2032 | `{` |
|      - | 2033 | `	const char *z1,*z2;` |
|      - | 2034 | `	int n1,n2;` |
|      - | 2035 | `	int res;` |
|     73 | 2036 | `	if( nArg < 2 ){` |
|    ! 0 | 2037 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 2038 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 2039 | `		return PH7_OK;` |
|      - | 2040 | `	}` |
|      - | 2041 | `	/* Perform the comparison */` |
|     73 | 2042 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     73 | 2043 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     73 | 2044 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2045 | `	/* Comparison result */` |
|     73 | 2046 | `	ph7_result_int(pCtx,res);` |
|     73 | 2047 | `	return PH7_OK;` |
|     37 | 2048 | `}` |
|      - | 2049 | `/*` |
|      - | 2050 | ` * Natural-order comparison core (Martin Pool's natcompare as adapted by php's` |
|      - | 2051 | ` * ext/standard/strnatcmp.c): digit runs compare numerically — the longer run` |
|      - | 2052 | ` * wins, a leading zero flips to fractional first-difference-wins semantics —` |
|      - | 2053 | ` * everything else compares bytewise with whitespace skipped.` |
|      - | 2054 | ` */` |
|     16 | 2055 | `static int StrNatCompareRight(const char **pa,const char *aEnd,const char **pb,const char *bEnd)` |
|      1 | 2056 | `{` |
|     17 | 2057 | `	int bias = 0;` |
|     30 | 2058 | `	for(;;){` |
|     39 | 2059 | `		int da = (*pa < aEnd) && SyisDigit(**pa);` |
|     39 | 2060 | `		int db = (*pb < bEnd) && SyisDigit(**pb);` |
|     39 | 2061 | `		if( !da && !db ){ return bias; }` |
|     31 | 2062 | `		if( !da ){ return -1; }` |
|     25 | 2063 | `		if( !db ){ return 1; }` |
|     23 | 2064 | `		if( **pa < **pb ){ if( !bias ){ bias = -1; } }` |
|     21 | 2065 | `		else if( **pa > **pb ){ if( !bias ){ bias = 1; } }` |
|     23 | 2066 | `		(*pa)++;` |
|     23 | 2067 | `		(*pb)++;` |
|      1 | 2068 | `	}` |
|      9 | 2069 | `}` |
|      2 | 2070 | `static int StrNatCompareLeft(const char **pa,const char *aEnd,const char **pb,const char *bEnd)` |
|      1 | 2071 | `{` |
|      1 | 2072 | `	for(;;){` |
|      3 | 2073 | `		int da = (*pa < aEnd) && SyisDigit(**pa);` |
|      3 | 2074 | `		int db = (*pb < bEnd) && SyisDigit(**pb);` |
|      3 | 2075 | `		if( !da && !db ){ return 0; }` |
|      3 | 2076 | `		if( !da ){ return -1; }` |
|      3 | 2077 | `		if( !db ){ return 1; }` |
|      3 | 2078 | `		if( **pa < **pb ){ return -1; }` |
|    ! 0 | 2079 | `		if( **pa > **pb ){ return 1; }` |
|    ! 0 | 2080 | `		(*pa)++;` |
|    ! 0 | 2081 | `		(*pb)++;` |
|    ! 0 | 2082 | `	}` |
|      2 | 2083 | `}` |
|     20 | 2084 | `static int StrNatCmpCore(const char *zA,int nA,const char *zB,int nB,int bFold)` |
|      1 | 2085 | `{` |
|     21 | 2086 | `	const char *a = zA,*aEnd = &zA[nA];` |
|     21 | 2087 | `	const char *b = zB,*bEnd = &zB[nB];` |
|     59 | 2088 | `	for(;;){` |
|      - | 2089 | `		int ca,cb;` |
|     73 | 2090 | `		while( a < aEnd && SyisSpace(a[0]) ){ a++; }` |
|     71 | 2091 | `		while( b < bEnd && SyisSpace(b[0]) ){ b++; }` |
|     71 | 2092 | `		ca = (a < aEnd) ? (unsigned char)a[0] : 0;` |
|     71 | 2093 | `		cb = (b < bEnd) ? (unsigned char)b[0] : 0;` |
|     71 | 2094 | `		if( SyisDigit(ca) && SyisDigit(cb) ){` |
|     18 | 2095 | `			int r = (ca == '0' \|\| cb == '0')` |
|      2 | 2096 | `				? StrNatCompareLeft(&a,aEnd,&b,bEnd)` |
|     25 | 2097 | `				: StrNatCompareRight(&a,aEnd,&b,bEnd);` |
|     19 | 2098 | `			if( r ){ return r; }` |
|      3 | 2099 | `			continue;` |
|      - | 2100 | `		}` |
|     53 | 2101 | `		if( ca == 0 && cb == 0 ){ return 0; }` |
|     49 | 2102 | `		if( bFold ){` |
|     49 | 2103 | `			ca = SyToLower(ca);` |
|     49 | 2104 | `			cb = SyToLower(cb);` |
|     24 | 2105 | `		}` |
|     49 | 2106 | `		if( ca < cb ){ return -1; }` |
|     49 | 2107 | `		if( ca > cb ){ return 1; }` |
|     49 | 2108 | `		a++;` |
|     49 | 2109 | `		b++;` |
|      1 | 2110 | `	}` |
|     11 | 2111 | `}` |
|      - | 2112 | `/*` |
|      - | 2113 | ` * int strnatcmp(string $string1, string $string2)` |
|      - | 2114 | ` * int strnatcasecmp(string $string1, string $string2)` |
|      - | 2115 | ` *  Natural-order string comparison ("img2" < "img10"), case folded for the` |
|      - | 2116 | ` *  latter. php 8.2+ normalizes the result to -1/0/1.` |
|      - | 2117 | ` */` |
|     20 | 2118 | `static int PH7_builtin_strnatcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2119 | `{` |
|      - | 2120 | `	const char *z1,*z2,*zFunc;` |
|      - | 2121 | `	int n1,n2,bFold;` |
|     21 | 2122 | `	if( nArg < 2 ){` |
|    ! 0 | 2123 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2124 | `		return PH7_OK;` |
|      - | 2125 | `	}` |
|     21 | 2126 | `	zFunc = ph7_function_name(pCtx);` |
|     21 | 2127 | `	bFold = zFunc[sizeof("strnat")-1] == 'c'; /* strnatCasecmp */` |
|     21 | 2128 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     21 | 2129 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     21 | 2130 | `	ph7_result_int(pCtx,StrNatCmpCore(z1,n1,z2,n2,bFold));` |
|     21 | 2131 | `	return PH7_OK;` |
|     11 | 2132 | `}` |
|      - | 2133 | `/*` |
|      - | 2134 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 2135 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 2136 | ` * Parameter` |
|      - | 2137 | ` *  str1: The first string` |
|      - | 2138 | ` *  str2: The second string` |
|      - | 2139 | ` * Return` |
|      - | 2140 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2141 | ` *  than str2, and 0 if they are equal.` |
|      - | 2142 | ` */` |
|     66 | 2143 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2144 | `{` |
|      - | 2145 | `	const char *z1,*z2;` |
|      - | 2146 | `	int res;` |
|      - | 2147 | `	int n;` |
|     68 | 2148 | `	if( nArg < 3 ){` |
|      - | 2149 | `		/* Perform a standard comparison */` |
|    ! 0 | 2150 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 2151 | `	}` |
|      - | 2152 | `	/* Desired comparison length */` |
|     68 | 2153 | `	n  = ph7_value_to_int(apArg[2]);` |
|     68 | 2154 | `	if( n < 0 ){` |
|      - | 2155 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 2156 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2157 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 2158 | `			ph7_function_name(pCtx));` |
|      - | 2159 | `	}` |
|      - | 2160 | `	/* Perform the comparison */` |
|     66 | 2161 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     66 | 2162 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     66 | 2163 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 2164 | `	/* Comparison result */` |
|     66 | 2165 | `	ph7_result_int(pCtx,res);` |
|     66 | 2166 | `	return PH7_OK;` |
|     35 | 2167 | `}` |
|      - | 2168 | `/*` |
|      - | 2169 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 2170 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 2171 | ` * Parameter` |
|      - | 2172 | ` *  str1: The first string` |
|      - | 2173 | ` *  str2: The second string` |
|      - | 2174 | ` * Return` |
|      - | 2175 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2176 | ` *  than str2, and 0 if they are equal.` |
|      - | 2177 | ` */` |
|    140 | 2178 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2179 | `{` |
|      - | 2180 | `	const char *z1,*z2;` |
|      - | 2181 | `	int n1,n2;` |
|      - | 2182 | `	int res;` |
|    141 | 2183 | `	if( nArg < 2 ){` |
|    ! 0 | 2184 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 2185 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 2186 | `		return PH7_OK;` |
|      - | 2187 | `	}` |
|      - | 2188 | `	/* Perform the comparison */` |
|    141 | 2189 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|    141 | 2190 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|    141 | 2191 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2192 | `	/* Comparison result */` |
|    141 | 2193 | `	ph7_result_int(pCtx,res);` |
|    141 | 2194 | `	return PH7_OK;` |
|     71 | 2195 | `}` |
|      - | 2196 | `/*` |
|      - | 2197 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 2198 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 2199 | ` * Parameter` |
|      - | 2200 | ` *  $str1: The first string` |
|      - | 2201 | ` *  $str2: The second string` |
|      - | 2202 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 2203 | ` * Return` |
|      - | 2204 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2205 | ` *  than str2, and 0 if they are equal.` |
|      - | 2206 | ` */` |
|     46 | 2207 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2208 | `{` |
|      - | 2209 | `	const char *z1,*z2;` |
|      - | 2210 | `	int res;` |
|      - | 2211 | `	int n;` |
|     51 | 2212 | `	if( nArg < 3 ){` |
|      - | 2213 | `		/* Perform a standard comparison */` |
|    ! 0 | 2214 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 2215 | `	}` |
|      - | 2216 | `	/* Desired comparison length */` |
|     51 | 2217 | `	n  = ph7_value_to_int(apArg[2]);` |
|     51 | 2218 | `	if( n < 0 ){` |
|      - | 2219 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 2220 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2221 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 2222 | `			ph7_function_name(pCtx));` |
|      - | 2223 | `	}` |
|      - | 2224 | `	/* Perform the comparison */` |
|     49 | 2225 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     49 | 2226 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     49 | 2227 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 2228 | `	/* Comparison result */` |
|     49 | 2229 | `	ph7_result_int(pCtx,res);` |
|     49 | 2230 | `	return PH7_OK;` |
|     28 | 2231 | `}` |
|      - | 2232 | `/*` |
|      - | 2233 | ` * Implode context [i.e: it's private data].` |
|      - | 2234 | ` * A pointer to the following structure is forwarded` |
|      - | 2235 | ` * verbatim to the array walker callback defined below.` |
|      - | 2236 | ` */` |
|      - | 2237 | `struct implode_data {` |
|      - | 2238 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 2239 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 2240 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 2241 | `	int nSeplen;          /* Separator length */` |
|      - | 2242 | `	int bFirst;           /* TRUE if first call */` |
|      - | 2243 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 2244 | `	sxi32 rc;             /* Captured allocation rc; SXERR_MEM => the builtin raises an OOM fatal */` |
|      - | 2245 | `};` |
|      - | 2246 | `/*` |
|      - | 2247 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 2248 | ` * The following routine is invoked for each array entry passed` |
|      - | 2249 | ` * to the implode() function.` |
|      - | 2250 | ` */` |
| 149492 | 2251 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 2252 | `{` |
|  74746 | 2253 | `	SXUNUSED(pKey);` |
| 149497 | 2254 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 2255 | `	const char *zData;` |
|      - | 2256 | `	int nLen;` |
| 149497 | 2257 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 2258 | `		if( pData->nSeplen > 0 ){` |
|      3 | 2259 | `			if( !pData->bFirst ){` |
|      - | 2260 | `				/* append the separator first */` |
|      3 | 2261 | `				if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 2262 | `					pData->rc = SXERR_MEM;` |
|    ! 0 | 2263 | `					return PH7_ABORT;` |
|      - | 2264 | `				}` |
|      2 | 2265 | `			}else{` |
|    ! 0 | 2266 | `				pData->bFirst = 0;` |
|      - | 2267 | `			}` |
|      1 | 2268 | `		}` |
|      - | 2269 | `		/* Recurse */` |
|      3 | 2270 | `		pData->bFirst = 1;` |
|      3 | 2271 | `		pData->nRecCount++;` |
|      3 | 2272 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 2273 | `		pData->nRecCount--;` |
|      - | 2274 | `		/* Propagate an allocation failure surfaced deeper in the recursion. */` |
|      3 | 2275 | `		if( pData->rc != SXRET_OK ){` |
|    ! 0 | 2276 | `			return PH7_ABORT;` |
|      - | 2277 | `		}` |
|      3 | 2278 | `		return PH7_OK;` |
|      - | 2279 | `	}` |
|      - | 2280 | `	/* Extract the string representation of the entry value */` |
| 149495 | 2281 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 2282 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 149495 | 2283 | `	if( pData->bFirst ){` |
|  33545 | 2284 | `		pData->bFirst = 0;` |
| 132725 | 2285 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 2286 | `		/* append the separator first */` |
| 115939 | 2287 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 2288 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 2289 | `			return PH7_ABORT;` |
|      - | 2290 | `		}` |
|  57967 | 2291 | `	}` |
|      - | 2292 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 149495 | 2293 | `	if( nLen > 0 ){` |
| 136877 | 2294 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2295 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 2296 | `			return PH7_ABORT;` |
|      - | 2297 | `		}` |
|  68436 | 2298 | `	}` |
| 149495 | 2299 | `	return PH7_OK;` |
|  74751 | 2300 | `}` |
|      - | 2301 | `/*` |
|      - | 2302 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 2303 | ` * string implode(array $pieces,...)` |
|      - | 2304 | ` *  Join array elements with a string.` |
|      - | 2305 | ` * $glue` |
|      - | 2306 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 2307 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 2308 | ` * $pieces` |
|      - | 2309 | ` *   The array of strings to implode.` |
|      - | 2310 | ` * Return` |
|      - | 2311 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 2312 | ` *  order, with the glue string between each element.` |
|      - | 2313 | ` */` |
|  33562 | 2314 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2315 | `{` |
|      - | 2316 | `	struct implode_data imp_data;` |
|  33567 | 2317 | `	int i = 1;` |
|  33567 | 2318 | `	if( nArg < 1 ){` |
|      - | 2319 | `		/* Missing argument,return NULL */` |
|    ! 0 | 2320 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2321 | `		return PH7_OK;` |
|      - | 2322 | `	}` |
|      - | 2323 | `	/* Prepare the implode context */` |
|  33567 | 2324 | `	imp_data.pCtx = pCtx;` |
|  33567 | 2325 | `	imp_data.bRecursive = 0;` |
|  33567 | 2326 | `	imp_data.bFirst = 1;` |
|  33567 | 2327 | `	imp_data.nRecCount = 0;` |
|  33567 | 2328 | `	imp_data.rc = SXRET_OK;` |
|  33567 | 2329 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  33565 | 2330 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  16785 | 2331 | `	}else{` |
|      3 | 2332 | `		imp_data.zSep = 0;` |
|      3 | 2333 | `		imp_data.nSeplen = 0;` |
|      3 | 2334 | `		i = 0;` |
|      - | 2335 | `	}` |
|  33567 | 2336 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2337 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2338 | `	}` |
|      - | 2339 | `	/* Start the 'join' process */` |
|  67129 | 2340 | `	while( i < nArg ){` |
|  33567 | 2341 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2342 | `			/* Iterate throw array entries */` |
|  33567 | 2343 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2344 | `			/* Surface a callback allocation failure as a fatal */` |
|  33567 | 2345 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2346 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2347 | `			}` |
|  16786 | 2348 | `		}else{` |
|      - | 2349 | `			const char *zData;` |
|      - | 2350 | `			int nLen;` |
|      - | 2351 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 2352 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2353 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 2354 | `			if( imp_data.bFirst ){` |
|    ! 0 | 2355 | `				imp_data.bFirst = 0;` |
|    ! 0 | 2356 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2357 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 2358 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2359 | `				}` |
|    ! 0 | 2360 | `			}` |
|      - | 2361 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 2362 | `			if( nLen > 0 ){` |
|    ! 0 | 2363 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2364 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2365 | `				}` |
|    ! 0 | 2366 | `			}` |
|      - | 2367 | `		}` |
|  33567 | 2368 | `		i++;` |
|      5 | 2369 | `	}` |
|  33567 | 2370 | `	return PH7_OK;` |
|  16786 | 2371 | `}` |
|      - | 2372 | `/*` |
|      - | 2373 | ` * Symisc eXtension:` |
|      - | 2374 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 2375 | ` * Purpose` |
|      - | 2376 | ` *  Same as implode() but recurse on arrays.` |
|      - | 2377 | ` * Example:` |
|      - | 2378 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 2379 | ` *   echo implode_recursive("/",$a);` |
|      - | 2380 | ` *   Will output` |
|      - | 2381 | ` *     usr/home/dean.` |
|      - | 2382 | ` *   While the standard implode would produce.` |
|      - | 2383 | ` *    usr/Array.` |
|      - | 2384 | ` * Parameter` |
|      - | 2385 | ` *  Refer to implode().` |
|      - | 2386 | ` * Return` |
|      - | 2387 | ` *  Refer to implode().` |
|      - | 2388 | ` */` |
|     12 | 2389 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2390 | `{` |
|      - | 2391 | `	struct implode_data imp_data;` |
|     13 | 2392 | `	int i = 1;` |
|     13 | 2393 | `	if( nArg < 1 ){` |
|      - | 2394 | `		/* Missing argument,return NULL */` |
|      3 | 2395 | `		ph7_result_null(pCtx);` |
|      3 | 2396 | `		return PH7_OK;` |
|      - | 2397 | `	}` |
|      - | 2398 | `	/* Prepare the implode context */` |
|     11 | 2399 | `	imp_data.pCtx = pCtx;` |
|     11 | 2400 | `	imp_data.bRecursive = 1;` |
|     11 | 2401 | `	imp_data.bFirst = 1;` |
|     11 | 2402 | `	imp_data.nRecCount = 0;` |
|     11 | 2403 | `	imp_data.rc = SXRET_OK;` |
|     11 | 2404 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 2405 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 2406 | `	}else{` |
|    ! 0 | 2407 | `		imp_data.zSep = 0;` |
|    ! 0 | 2408 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 2409 | `		i = 0;` |
|      - | 2410 | `	}` |
|     11 | 2411 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2412 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2413 | `	}` |
|      - | 2414 | `	/* Start the 'join' process */` |
|     21 | 2415 | `	while( i < nArg ){` |
|     11 | 2416 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2417 | `			/* Iterate throw array entries */` |
|      3 | 2418 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2419 | `			/* Surface a callback allocation failure as a fatal */` |
|      3 | 2420 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2421 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2422 | `			}` |
|      2 | 2423 | `		}else{` |
|      - | 2424 | `			const char *zData;` |
|      - | 2425 | `			int nLen;` |
|      - | 2426 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 2427 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2428 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 2429 | `			if( imp_data.bFirst ){` |
|      9 | 2430 | `				imp_data.bFirst = 0;` |
|      4 | 2431 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2432 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 2433 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2434 | `				}` |
|    ! 0 | 2435 | `			}` |
|      - | 2436 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 2437 | `			if( nLen > 0 ){` |
|      9 | 2438 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2439 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2440 | `				}` |
|      4 | 2441 | `			}` |
|      - | 2442 | `		}` |
|     11 | 2443 | `		i++;` |
|      1 | 2444 | `	}` |
|     11 | 2445 | `	return PH7_OK;` |
|      7 | 2446 | `}` |
|      - | 2447 | `/*` |
|      - | 2448 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 2449 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 2450 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 2451 | ` * Parameters` |
|      - | 2452 | ` *  $delimiter` |
|      - | 2453 | ` *   The boundary string.` |
|      - | 2454 | ` * $string` |
|      - | 2455 | ` *   The input string.` |
|      - | 2456 | ` * $limit` |
|      - | 2457 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 2458 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 2459 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 2460 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 2461 | ` * Returns` |
|      - | 2462 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 2463 | ` *  on boundaries formed by the delimiter.` |
|      - | 2464 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 2465 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 2466 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 2467 | ` *  will be returned.` |
|      - | 2468 | ` * NOTE:` |
|      - | 2469 | ` *  Negative limit is not supported.` |
|      - | 2470 | ` */` |
|   6586 | 2471 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2472 | `{` |
|      - | 2473 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 2474 | `	int nDelim,nStrlen,iLimit;` |
|      - | 2475 | `	ph7_value *pArray;` |
|      - | 2476 | `	ph7_value *pValue;` |
|      - | 2477 | `	sxu32 nOfft;` |
|      - | 2478 | `	sxi32 rc;` |
|   6591 | 2479 | `	if( nArg < 2 ){` |
|      - | 2480 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2481 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2482 | `		return PH7_OK;` |
|      - | 2483 | `	}` |
|      - | 2484 | `	/* Extract the delimiter */` |
|   6591 | 2485 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   6591 | 2486 | `	if( nDelim < 1 ){` |
|      - | 2487 | `		/* Empty delimiter: PHP 8 throws a catchable ValueError. */` |
|      5 | 2488 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2489 | `			"explode(): Argument #1 ($separator) must not be empty");` |
|      - | 2490 | `	}` |
|      - | 2491 | `	/* Extract the string */` |
|   6587 | 2492 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   6587 | 2493 | `	if( nStrlen < 1 ){` |
|      - | 2494 | `		/* Empty string: normally an array with a single empty element (PHP behavior).` |
|      - | 2495 | `		 * A negative limit drops the last -limit components, so the sole empty` |
|      - | 2496 | `		 * component is dropped and the result is an empty array. */` |
|      7 | 2497 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      7 | 2498 | `		if( pArrayTmp == 0 ){` |
|      - | 2499 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 2500 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2501 | `			return PH7_OK;` |
|      - | 2502 | `		}` |
|      7 | 2503 | `		if( !(nArg > 2 && ph7_value_to_int(apArg[2]) < 0) ){` |
|      5 | 2504 | `			ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      5 | 2505 | `			if( pValueTmp == 0 ){` |
|      - | 2506 | `				/* Out of memory,return FALSE */` |
|    ! 0 | 2507 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 2508 | `				return PH7_OK;` |
|      - | 2509 | `			}` |
|      5 | 2510 | `			ph7_value_string(pValueTmp, "", 0);` |
|      5 | 2511 | `			if( ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp) != SXRET_OK ){` |
|    ! 0 | 2512 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2513 | `			}` |
|      2 | 2514 | `		}` |
|      7 | 2515 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      7 | 2516 | `		return PH7_OK;` |
|      - | 2517 | `	}` |
|      - | 2518 | `	/* Point to the end of the string */` |
|   6581 | 2519 | `	zEnd = &zString[nStrlen];` |
|      - | 2520 | `	/* Create the array */` |
|   6581 | 2521 | `	pArray =  ph7_context_new_array(pCtx);` |
|   6581 | 2522 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   6581 | 2523 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2524 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 2525 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2526 | `		return PH7_OK;` |
|      - | 2527 | `	}` |
|      - | 2528 | `	/* Set a defualt limit */` |
|   6581 | 2529 | `	iLimit = SXI32_HIGH;` |
|   6581 | 2530 | `	if( nArg > 2 ){` |
|     38 | 2531 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     38 | 2532 | `		if( iLimit < 0 ){` |
|      - | 2533 | `			/* Negative limit: keep all components except the last -iLimit (PHP).` |
|      - | 2534 | `			 * Pre-count the components (delimiters + 1), then emit only the first` |
|      - | 2535 | `			 * nKeep CLEAN components — no trailing-remainder merge (the difference` |
|      - | 2536 | `			 * from the positive path). nKeep <= 0 drops everything -> empty array. */` |
|     17 | 2537 | `			int nTotal = 1,nKeep;` |
|     17 | 2538 | `			const char *zScan = zString;` |
|      - | 2539 | `			sxu32 nScanOfft;` |
|     57 | 2540 | `			while( SyBlobSearch(zScan,(sxu32)(zEnd - zScan),zDelim,nDelim,&nScanOfft) == SXRET_OK ){` |
|     41 | 2541 | `				nTotal++;` |
|     41 | 2542 | `				zScan = &zScan[nScanOfft + nDelim];` |
|      1 | 2543 | `			}` |
|     17 | 2544 | `			nKeep = nTotal + iLimit; /* iLimit < 0, so this is nTotal - (-iLimit) */` |
|     49 | 2545 | `			while( nKeep > (int)ph7_array_count(pArray)` |
|     39 | 2546 | `				&& SyBlobSearch(zString,(sxu32)(zEnd - zString),zDelim,nDelim,&nOfft) == SXRET_OK ){` |
|      - | 2547 | `				/* Emit the next clean component */` |
|     23 | 2548 | `				zCur = &zString[nOfft];` |
|     23 | 2549 | `				ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|     23 | 2550 | `				if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2551 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2552 | `				}` |
|     23 | 2553 | `				zString = &zCur[nDelim];` |
|     23 | 2554 | `				ph7_value_reset_string_cursor(pValue);` |
|      1 | 2555 | `			}` |
|     17 | 2556 | `			ph7_result_value(pCtx,pArray);` |
|     17 | 2557 | `			return PH7_OK;` |
|      - | 2558 | `		}` |
|     22 | 2559 | `		if( iLimit == 0 ){` |
|      5 | 2560 | `			iLimit = 1;` |
|      2 | 2561 | `		}` |
|     22 | 2562 | `		iLimit--;` |
|      9 | 2563 | `	}` |
|      - | 2564 | `	/* Start exploding */` |
|  80677 | 2565 | `	for(;;){` |
| 161359 | 2566 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 161359 | 2567 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 2568 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   6565 | 2569 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   6565 | 2570 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2571 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2572 | `			}` |
|   6565 | 2573 | `			break;` |
|      - | 2574 | `		}` |
|      - | 2575 | `		/* Point to the desired offset */` |
| 154799 | 2576 | `		zCur = &zString[nOfft];` |
|      - | 2577 | `		/* Perform the store operation (may be empty) */` |
| 154799 | 2578 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 154799 | 2579 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2580 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2581 | `		}` |
|      - | 2582 | `		/* Point beyond the delimiter */` |
| 154799 | 2583 | `		zString = &zCur[nDelim];` |
|      - | 2584 | `		/* Reset the cursor */` |
| 154799 | 2585 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 2586 | `	}` |
|      - | 2587 | `	/* Return the freshly created array */` |
|   6565 | 2588 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2589 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 2590 | `	 * released as soon we return from this foregin function.` |
|      - | 2591 | `	 */` |
|   6565 | 2592 | `	return PH7_OK;` |
|   3298 | 2593 | `}` |
|      - | 2594 | `/*` |
|      - | 2595 | ` * string trim(string $str[,string $charlist ])` |
|      - | 2596 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2597 | ` * Parameters` |
|      - | 2598 | ` *  $str` |
|      - | 2599 | ` *   The string that will be trimmed.` |
|      - | 2600 | ` * $charlist` |
|      - | 2601 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2602 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2603 | ` *   With .. you can specify a range of characters.` |
|      - | 2604 | ` * Returns.` |
|      - | 2605 | ` *  Thr processed string.` |
|      - | 2606 | ` * NOTE:` |
|      - | 2607 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2608 | ` */` |
|  14670 | 2609 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2610 | `{` |
|  14675 | 2611 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"trim",1,"$string"); }` |
|      - | 2612 | `	const char *zString;` |
|      - | 2613 | `	int nLen;` |
|  14675 | 2614 | `	if( nArg < 1 ){` |
|      - | 2615 | `		/* Missing arguments,return null */` |
|    ! 0 | 2616 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2617 | `		return PH7_OK;` |
|      - | 2618 | `	}` |
|      - | 2619 | `	/* Extract the target string */` |
|  14675 | 2620 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  14675 | 2621 | `	if( nLen < 1 ){` |
|      - | 2622 | `		/* Empty string,return */` |
|   1087 | 2623 | `		ph7_result_string(pCtx,"",0);` |
|   1087 | 2624 | `		return PH7_OK;` |
|      - | 2625 | `	}` |
|      - | 2626 | `	/* Start the trim process */` |
|  13593 | 2627 | `	if( nArg < 2 ){` |
|      - | 2628 | `		SyString sStr;` |
|      - | 2629 | `		/* Remove white spaces and NUL bytes */` |
|  13563 | 2630 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  34013 | 2631 | `		SyStringFullTrimSafe(&sStr);` |
|  13563 | 2632 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   6784 | 2633 | `	}else{` |
|      - | 2634 | `		/* Char list */` |
|      - | 2635 | `		const char *zList;` |
|      - | 2636 | `		int nListlen;` |
|     33 | 2637 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     33 | 2638 | `		if( nListlen < 1 ){` |
|      - | 2639 | `			/* Return the string unchanged */` |
|      6 | 2640 | `			ph7_result_string(pCtx,zString,nLen);` |
|      4 | 2641 | `		}else{` |
|      - | 2642 | `			char aMask[256];` |
|     29 | 2643 | `			const char *zEnd = &zString[nLen];` |
|     29 | 2644 | `			const char *zCur = zString;` |
|     29 | 2645 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2646 | `			/* Left trim */` |
|     79 | 2647 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     53 | 2648 | `				zCur++;` |
|      3 | 2649 | `			}` |
|      - | 2650 | `			/* Right trim */` |
|     79 | 2651 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     53 | 2652 | `				zEnd--;` |
|      3 | 2653 | `			}` |
|     29 | 2654 | `			if( zCur >= zEnd ){` |
|      - | 2655 | `				/* Return the empty string */` |
|    ! 0 | 2656 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2657 | `			}else{` |
|     29 | 2658 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2659 | `			}` |
|      - | 2660 | `		}` |
|      - | 2661 | `	}` |
|  13593 | 2662 | `	return PH7_OK;` |
|   7340 | 2663 | `}` |
|      - | 2664 | `/*` |
|      - | 2665 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 2666 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 2667 | ` * Parameters` |
|      - | 2668 | ` *  $str` |
|      - | 2669 | ` *   The string that will be trimmed.` |
|      - | 2670 | ` * $charlist` |
|      - | 2671 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2672 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2673 | ` *   With .. you can specify a range of characters.` |
|      - | 2674 | ` * Returns.` |
|      - | 2675 | ` *  Thr processed string.` |
|      - | 2676 | ` * NOTE:` |
|      - | 2677 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2678 | ` */` |
|     36 | 2679 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2680 | `{` |
|     39 | 2681 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"rtrim",1,"$string"); }` |
|      - | 2682 | `	const char *zString;` |
|      - | 2683 | `	int nLen;` |
|     39 | 2684 | `	if( nArg < 1 ){` |
|      - | 2685 | `		/* Missing arguments,return null */` |
|    ! 0 | 2686 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2687 | `		return PH7_OK;` |
|      - | 2688 | `	}` |
|      - | 2689 | `	/* Extract the target string */` |
|     39 | 2690 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     39 | 2691 | `	if( nLen < 1 ){` |
|      - | 2692 | `		/* Empty string,return */` |
|      7 | 2693 | `		ph7_result_string(pCtx,"",0);` |
|      7 | 2694 | `		return PH7_OK;` |
|      - | 2695 | `	}` |
|      - | 2696 | `	/* Start the trim process */` |
|     33 | 2697 | `	if( nArg < 2 ){` |
|      - | 2698 | `		SyString sStr;` |
|      - | 2699 | `		/* Remove white spaces and NUL bytes*/` |
|     17 | 2700 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     41 | 2701 | `		SyStringRightTrimSafe(&sStr);` |
|     17 | 2702 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      9 | 2703 | `	}else{` |
|      - | 2704 | `		/* Char list */` |
|      - | 2705 | `		const char *zList;` |
|      - | 2706 | `		int nListlen;` |
|     17 | 2707 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     17 | 2708 | `		if( nListlen < 1 ){` |
|      - | 2709 | `			/* Return the string unchanged */` |
|    ! 0 | 2710 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 2711 | `		}else{` |
|      - | 2712 | `			char aMask[256];` |
|     17 | 2713 | `			const char *zEnd = &zString[nLen];` |
|     17 | 2714 | `			const char *zCur = zString;` |
|     17 | 2715 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2716 | `			/* Right trim */` |
|     37 | 2717 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     22 | 2718 | `				zEnd--;` |
|      2 | 2719 | `			}` |
|     17 | 2720 | `			if( zEnd <= zCur ){` |
|      - | 2721 | `				/* Return the empty string */` |
|    ! 0 | 2722 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2723 | `			}else{` |
|     17 | 2724 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2725 | `			}` |
|      - | 2726 | `		}` |
|      - | 2727 | `	}` |
|     33 | 2728 | `	return PH7_OK;` |
|     21 | 2729 | `}` |
|      - | 2730 | `/*` |
|      - | 2731 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 2732 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2733 | ` * Parameters` |
|      - | 2734 | ` *  $str` |
|      - | 2735 | ` *   The string that will be trimmed.` |
|      - | 2736 | ` * $charlist` |
|      - | 2737 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2738 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2739 | ` *   With .. you can specify a range of characters.` |
|      - | 2740 | ` * Returns.` |
|      - | 2741 | ` *  Thr processed string.` |
|      - | 2742 | ` * NOTE:` |
|      - | 2743 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2744 | ` */` |
|     48 | 2745 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2746 | `{` |
|     53 | 2747 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"ltrim",1,"$string"); }` |
|      - | 2748 | `	const char *zString;` |
|      - | 2749 | `	int nLen;` |
|     53 | 2750 | `	if( nArg < 1 ){` |
|      - | 2751 | `		/* Missing arguments,return null */` |
|    ! 0 | 2752 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2753 | `		return PH7_OK;` |
|      - | 2754 | `	}` |
|      - | 2755 | `	/* Extract the target string */` |
|     53 | 2756 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     53 | 2757 | `	if( nLen < 1 ){` |
|      - | 2758 | `		/* Empty string,return */` |
|     29 | 2759 | `		ph7_result_string(pCtx,"",0);` |
|     29 | 2760 | `		return PH7_OK;` |
|      - | 2761 | `	}` |
|      - | 2762 | `	/* Start the trim process */` |
|     29 | 2763 | `	if( nArg < 2 ){` |
|      - | 2764 | `		SyString sStr;` |
|      - | 2765 | `		/* Remove white spaces and NUL byte */` |
|      5 | 2766 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     13 | 2767 | `		SyStringLeftTrimSafe(&sStr);` |
|      5 | 2768 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      3 | 2769 | `	}else{` |
|      - | 2770 | `		/* Char list */` |
|      - | 2771 | `		const char *zList;` |
|      - | 2772 | `		int nListlen;` |
|     25 | 2773 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     25 | 2774 | `		if( nListlen < 1 ){` |
|      - | 2775 | `			/* Return the string unchanged */` |
|      3 | 2776 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2777 | `		}else{` |
|      - | 2778 | `			char aMask[256];` |
|     23 | 2779 | `			const char *zEnd = &zString[nLen];` |
|     23 | 2780 | `			const char *zCur = zString;` |
|     23 | 2781 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2782 | `			/* Left trim */` |
|     55 | 2783 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     37 | 2784 | `				zCur++;` |
|      5 | 2785 | `			}` |
|     23 | 2786 | `			if( zCur >= zEnd ){` |
|      - | 2787 | `				/* Return the empty string */` |
|    ! 0 | 2788 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2789 | `			}else{` |
|     23 | 2790 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2791 | `			}` |
|      - | 2792 | `		}` |
|      - | 2793 | `	}` |
|     29 | 2794 | `	return PH7_OK;` |
|     29 | 2795 | `}` |
|      - | 2796 | `/*` |
|      - | 2797 | ` * string strtolower(string $str)` |
|      - | 2798 | ` *  Make a string lowercase.` |
|      - | 2799 | ` * Parameters` |
|      - | 2800 | ` *  $str` |
|      - | 2801 | ` *   The input string.` |
|      - | 2802 | ` * Returns.` |
|      - | 2803 | ` *  The lowercased string.` |
|      - | 2804 | ` */` |
|  33534 | 2805 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2806 | `{` |
|  33539 | 2807 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtolower",1,"$string"); }` |
|      - | 2808 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2809 | `	int nLen;` |
|  33539 | 2810 | `	if( nArg < 1 ){` |
|      - | 2811 | `		/* Missing arguments,return null */` |
|    ! 0 | 2812 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2813 | `		return PH7_OK;` |
|      - | 2814 | `	}` |
|      - | 2815 | `	/* Extract the target string */` |
|  33539 | 2816 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  33539 | 2817 | `	if( nLen < 1 ){` |
|      - | 2818 | `		/* Empty string,return */` |
|      5 | 2819 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2820 | `		return PH7_OK;` |
|      - | 2821 | `	}` |
|      - | 2822 | `	/* Perform the requested operation */` |
|  33535 | 2823 | `	zEnd = &zString[nLen];` |
| 105776 | 2824 | `	for(;;){` |
| 211557 | 2825 | `		if( zString >= zEnd ){` |
|      - | 2826 | `			/* No more input,break immediately */` |
|  33535 | 2827 | `			break;` |
|      - | 2828 | `		}` |
| 178027 | 2829 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2830 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2831 | `			zCur = zString;` |
|    ! 0 | 2832 | `			zString++;` |
|    ! 0 | 2833 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2834 | `				zString++;` |
|    ! 0 | 2835 | `			}` |
|      - | 2836 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2837 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2838 | `		}else{` |
| 178027 | 2839 | `			int c = zString[0];` |
| 178027 | 2840 | `			if( SyisUpper(c) ){` |
| 175471 | 2841 | `				c = SyToLower(zString[0]);` |
|  87733 | 2842 | `			}` |
|      - | 2843 | `			/* Append character */` |
| 178027 | 2844 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2845 | `			/* Advance the cursor */` |
| 178027 | 2846 | `			zString++;` |
|      - | 2847 | `		}` |
|      5 | 2848 | `	}` |
|  33535 | 2849 | `	return PH7_OK;` |
|  16772 | 2850 | `}` |
|      - | 2851 | `/*` |
|      - | 2852 | ` * string strtolower(string $str)` |
|      - | 2853 | ` *  Make a string uppercase.` |
|      - | 2854 | ` * Parameters` |
|      - | 2855 | ` *  $str` |
|      - | 2856 | ` *   The input string.` |
|      - | 2857 | ` * Returns.` |
|      - | 2858 | ` *  The uppercased string.` |
|      - | 2859 | ` */` |
|     70 | 2860 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2861 | `{` |
|     75 | 2862 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtoupper",1,"$string"); }` |
|      - | 2863 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2864 | `	int nLen;` |
|     75 | 2865 | `	if( nArg < 1 ){` |
|      - | 2866 | `		/* Missing arguments,return null */` |
|    ! 0 | 2867 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2868 | `		return PH7_OK;` |
|      - | 2869 | `	}` |
|      - | 2870 | `	/* Extract the target string */` |
|     75 | 2871 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     75 | 2872 | `	if( nLen < 1 ){` |
|      - | 2873 | `		/* Empty string,return */` |
|      5 | 2874 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2875 | `		return PH7_OK;` |
|      - | 2876 | `	}` |
|      - | 2877 | `	/* Perform the requested operation */` |
|     71 | 2878 | `	zEnd = &zString[nLen];` |
|    139 | 2879 | `	for(;;){` |
|    283 | 2880 | `		if( zString >= zEnd ){` |
|      - | 2881 | `			/* No more input,break immediately */` |
|     71 | 2882 | `			break;` |
|      - | 2883 | `		}` |
|    217 | 2884 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2885 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2886 | `			zCur = zString;` |
|    ! 0 | 2887 | `			zString++;` |
|    ! 0 | 2888 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2889 | `				zString++;` |
|    ! 0 | 2890 | `			}` |
|      - | 2891 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2892 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2893 | `		}else{` |
|    217 | 2894 | `			int c = zString[0];` |
|    217 | 2895 | `			if( SyisLower(c) ){` |
|    204 | 2896 | `				c = SyToUpper(zString[0]);` |
|    100 | 2897 | `			}` |
|      - | 2898 | `			/* Append character */` |
|    217 | 2899 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2900 | `			/* Advance the cursor */` |
|    217 | 2901 | `			zString++;` |
|      - | 2902 | `		}` |
|      5 | 2903 | `	}` |
|     71 | 2904 | `	return PH7_OK;` |
|     40 | 2905 | `}` |
|      - | 2906 | `/*` |
|      - | 2907 | ` * string ucfirst(string $str)` |
|      - | 2908 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2909 | ` *  character is alphabetic.` |
|      - | 2910 | ` * Parameters` |
|      - | 2911 | ` *  $str` |
|      - | 2912 | ` *   The input string.` |
|      - | 2913 | ` * Returns.` |
|      - | 2914 | ` *  The processed string.` |
|      - | 2915 | ` */` |
|      4 | 2916 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2917 | `{` |
|      - | 2918 | `	const char *zString,*zEnd;` |
|      - | 2919 | `	int nLen,c;` |
|      5 | 2920 | `	if( nArg < 1 ){` |
|      - | 2921 | `		/* Missing arguments,return null */` |
|    ! 0 | 2922 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2923 | `		return PH7_OK;` |
|      - | 2924 | `	}` |
|      - | 2925 | `	/* Extract the target string */` |
|      5 | 2926 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2927 | `	if( nLen < 1 ){` |
|      - | 2928 | `		/* Empty string,return */` |
|      3 | 2929 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2930 | `		return PH7_OK;` |
|      - | 2931 | `	}` |
|      - | 2932 | `	/* Perform the requested operation */` |
|      3 | 2933 | `	zEnd = &zString[nLen];` |
|      3 | 2934 | `	c = zString[0];` |
|      3 | 2935 | `	if( SyisLower(c) ){` |
|      3 | 2936 | `		c = SyToUpper(c);` |
|      1 | 2937 | `	}` |
|      - | 2938 | `	/* Append the first character */` |
|      3 | 2939 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2940 | `	zString++;` |
|      3 | 2941 | `	if( zString < zEnd ){` |
|      - | 2942 | `		/* Append the rest of the input verbatim */` |
|      3 | 2943 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2944 | `	}` |
|      3 | 2945 | `	return PH7_OK;` |
|      3 | 2946 | `}` |
|      - | 2947 | `/*` |
|      - | 2948 | ` * string lcfirst(string $str)` |
|      - | 2949 | ` *  Make a string's first character lowercase.` |
|      - | 2950 | ` * Parameters` |
|      - | 2951 | ` *  $str` |
|      - | 2952 | ` *   The input string.` |
|      - | 2953 | ` * Returns.` |
|      - | 2954 | ` *  The processed string.` |
|      - | 2955 | ` */` |
|      4 | 2956 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2957 | `{` |
|      - | 2958 | `	const char *zString,*zEnd;` |
|      - | 2959 | `	int nLen,c;` |
|      5 | 2960 | `	if( nArg < 1 ){` |
|      - | 2961 | `		/* Missing arguments,return null */` |
|    ! 0 | 2962 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2963 | `		return PH7_OK;` |
|      - | 2964 | `	}` |
|      - | 2965 | `	/* Extract the target string */` |
|      5 | 2966 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2967 | `	if( nLen < 1 ){` |
|      - | 2968 | `		/* Empty string,return */` |
|      3 | 2969 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2970 | `		return PH7_OK;` |
|      - | 2971 | `	}` |
|      - | 2972 | `	/* Perform the requested operation */` |
|      3 | 2973 | `	zEnd = &zString[nLen];` |
|      3 | 2974 | `	c = zString[0];` |
|      3 | 2975 | `	if( SyisUpper(c) ){` |
|      3 | 2976 | `		c = SyToLower(c);` |
|      1 | 2977 | `	}` |
|      - | 2978 | `	/* Append the first character */` |
|      3 | 2979 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2980 | `	zString++;` |
|      3 | 2981 | `	if( zString < zEnd ){` |
|      - | 2982 | `		/* Append the rest of the input verbatim */` |
|      3 | 2983 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2984 | `	}` |
|      3 | 2985 | `	return PH7_OK;` |
|      3 | 2986 | `}` |
|      - | 2987 | `/*` |
|      - | 2988 | ` * int ord(string $string)` |
|      - | 2989 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 2990 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 2991 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 2992 | ` * Parameters` |
|      - | 2993 | ` *  $string` |
|      - | 2994 | ` *   The input string.` |
|      - | 2995 | ` * Returns` |
|      - | 2996 | ` *  The ASCII value as an integer.` |
|      - | 2997 | ` */` |
|    184 | 2998 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2999 | `{` |
|      - | 3000 | `	const char *zString;` |
|      - | 3001 | `	int nLen,c;` |
|      - | 3002 | `	/* PHP requires exactly one argument. */` |
|    188 | 3003 | `	if( nArg != 1 ){` |
|      8 | 3004 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3005 | `			"ArgumentCountError",` |
|      - | 3006 | `			"ord() expects exactly 1 argument, %d given",` |
|      2 | 3007 | `			nArg` |
|      - | 3008 | `			);` |
|      - | 3009 | `	}` |
|      - | 3010 | `	/* Passing null is deprecated (E_DEPRECATED).  PHP emits this before` |
|      - | 3011 | `	 * the empty-string deprecation, so we check null first. */` |
|    182 | 3012 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 3013 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 3014 | `			"ord(): Passing null to parameter #1 ($character) "` |
|      - | 3015 | `			"of type string is deprecated"` |
|      - | 3016 | `			);` |
|      1 | 3017 | `	}` |
|      - | 3018 | `	/* Extract the target string */` |
|    182 | 3019 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    182 | 3020 | `	if( nLen < 1 ){` |
|      - | 3021 | `		/* Empty string is deprecated (E_DEPRECATED). */` |
|      5 | 3022 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 3023 | `			"ord(): Providing an empty string is deprecated"` |
|      - | 3024 | `			);` |
|      5 | 3025 | `		ph7_result_int(pCtx,0);` |
|      5 | 3026 | `		return PH7_OK;` |
|      - | 3027 | `	}` |
|      - | 3028 | `	/* A string longer than one byte is deprecated (E_DEPRECATED). */` |
|    178 | 3029 | `	if( nLen > 1 ){` |
|      7 | 3030 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 3031 | `			"ord(): Providing a string that is not one byte long "` |
|      - | 3032 | `			"is deprecated. Use ord($str[0]) instead"` |
|      - | 3033 | `			);` |
|      3 | 3034 | `	}` |
|      - | 3035 | `	/* Extract the ASCII value of the first character */` |
|    178 | 3036 | `	c = (unsigned char)zString[0];` |
|      - | 3037 | `	/* Return that value */` |
|    178 | 3038 | `	ph7_result_int(pCtx,c);` |
|    178 | 3039 | `	return PH7_OK;` |
|     96 | 3040 | `}` |
|      - | 3041 | `/*` |
|      - | 3042 | ` * string chr(int $codepoint)` |
|      - | 3043 | ` *  Returns a one-character string containing the character specified` |
|      - | 3044 | ` *  by the given codepoint.  Any integer is accepted; values outside` |
|      - | 3045 | ` *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.` |
|      - | 3046 | ` * Parameters` |
|      - | 3047 | ` *  $codepoint` |
|      - | 3048 | ` *   An integer codepoint.  Values outside 0-255 are deprecated and` |
|      - | 3049 | ` *   will be constrained to a single byte.` |
|      - | 3050 | ` * Returns` |
|      - | 3051 | ` *  A single-character string.` |
|      - | 3052 | ` */` |
|   7116 | 3053 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3054 | `{` |
|      - | 3055 | `	int c;` |
|      - | 3056 | `	unsigned char ch;` |
|      - | 3057 | `	/* PHP requires exactly one argument. */` |
|   7119 | 3058 | `	if( nArg != 1 ){` |
|      8 | 3059 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3060 | `			"ArgumentCountError",` |
|      - | 3061 | `			"chr() expects exactly 1 argument, %d given",` |
|      2 | 3062 | `			nArg` |
|      - | 3063 | `			);` |
|      - | 3064 | `	}` |
|      - | 3065 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 3066 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 3067 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 3068 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|   7114 | 3069 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      - | 3070 | `		char zBuf[120];` |
|      4 | 3071 | `		SyBufferFormat(zBuf,sizeof(zBuf),` |
|      - | 3072 | `			"Implicit conversion from float %g to int loses precision",` |
|      1 | 3073 | `			ph7_value_to_double(apArg[0])` |
|      - | 3074 | `			);` |
|      3 | 3075 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zBuf);` |
|      1 | 3076 | `	}` |
|      - | 3077 | `	/* Extract the codepoint. */` |
|   7114 | 3078 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 3079 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 3080 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 3081 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 3082 | `	 * name to avoid the API double-prefixing it. */` |
|   7114 | 3083 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 3084 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 3085 | `			E_DEPRECATED,` |
|      - | 3086 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 3087 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 3088 | `			"The value used will be constrained using % 256"` |
|      - | 3089 | `			);` |
|      2 | 3090 | `	}` |
|      - | 3091 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 3092 | `	 * when taking the address of a wider int. */` |
|   7114 | 3093 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 3094 | `	/* Return the specified character */` |
|   7114 | 3095 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|   7114 | 3096 | `	return PH7_OK;` |
|   3561 | 3097 | `}` |
|      - | 3098 | `/*` |
|      - | 3099 | ` * Binary to hex consumer callback.` |
|      - | 3100 | ` * This callback is the default consumer used by the hash functions` |
|      - | 3101 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 3102 | ` */` |
|   3118 | 3103 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 3104 | `{` |
|      - | 3105 | `	/* Append hex chunk verbatim */` |
|   3120 | 3106 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   3120 | 3107 | `	return SXRET_OK;` |
|      2 | 3108 | `}` |
|      - | 3109 |  |
|      - | 3110 | `/*` |
|      - | 3111 | ` * string bin2hex(string $str)` |
|      - | 3112 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 3113 | ` * Parameters` |
|      - | 3114 | ` *  $str` |
|      - | 3115 | ` *   The input string.` |
|      - | 3116 | ` * Returns.` |
|      - | 3117 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 3118 | ` */` |
|    138 | 3119 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3120 | `{` |
|      - | 3121 | `	const char *zString;` |
|      - | 3122 | `	int nLen;` |
|      - | 3123 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|    143 | 3124 | `	if( nArg != 1 ){` |
|      8 | 3125 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3126 | `			"ArgumentCountError",` |
|      - | 3127 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|      2 | 3128 | `			nArg` |
|      - | 3129 | `			);` |
|      - | 3130 | `	}` |
|      - | 3131 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 3132 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 3133 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 3134 | `	 */` |
|    205 | 3135 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|    130 | 3136 | `		( ph7_value_is_object(apArg[0]) &&` |
|      3 | 3137 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|      2 | 3138 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|      1 | 3139 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 3140 | `		)` |
|      - | 3141 | `	){` |
|      9 | 3142 | `		const char *zType = ph7_type_name(apArg[0]);` |
|      9 | 3143 | `		if( ph7_value_is_object(apArg[0]) ){` |
|      3 | 3144 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      3 | 3145 | `			if( pInst && pInst->pClass ){` |
|      3 | 3146 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      1 | 3147 | `			}` |
|      1 | 3148 | `		}` |
|     12 | 3149 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3150 | `			"TypeError",` |
|      - | 3151 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|      3 | 3152 | `			zType` |
|      - | 3153 | `			);` |
|      - | 3154 | `	}` |
|      - | 3155 | `	/* Extract the target string */` |
|    130 | 3156 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    130 | 3157 | `	if( nLen < 1 ){` |
|      - | 3158 | `		/* Empty string,return */` |
|     13 | 3159 | `		ph7_result_string(pCtx,"",0);` |
|     13 | 3160 | `		return PH7_OK;` |
|      - | 3161 | `	}` |
|      - | 3162 | `	/* Perform the requested operation */` |
|    118 | 3163 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|    118 | 3164 | `	return PH7_OK;` |
|     74 | 3165 | `}` |
|      - | 3166 |  |
|      - | 3167 | `/* Search callback signature */` |
|      - | 3168 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 3169 | `/*` |
|      - | 3170 | ` * Case-insensitive pattern match.` |
|      - | 3171 | ` * Brute force is the default search method used here.` |
|      - | 3172 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 3173 | ` * well for short/medium texts on modern hardware.` |
|      - | 3174 | ` */` |
|    298 | 3175 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      2 | 3176 | `{` |
|    300 | 3177 | `	const char *zpIn = (const char *)pPattern;` |
|    300 | 3178 | `	const char *zIn = (const char *)pText;` |
|    300 | 3179 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    300 | 3180 | `	const char *zEnd = &zIn[nLen];` |
|      - | 3181 | `	const char *zPtr,*zPtr2;` |
|      - | 3182 | `	int c,d;` |
|    300 | 3183 | `	if( iPatLen > nLen ){` |
|      - | 3184 | `		/* Don't bother processing */` |
|     67 | 3185 | `		return SXERR_NOTFOUND;` |
|      - | 3186 | `	}` |
|    860 | 3187 | `	for(;;){` |
|   1722 | 3188 | `		if( zIn >= zEnd ){` |
|    194 | 3189 | `			break;` |
|      - | 3190 | `		}` |
|   1530 | 3191 | `		c = SyToLower(zIn[0]);` |
|   1530 | 3192 | `		d = SyToLower(zpIn[0]);` |
|   1530 | 3193 | `		if( c == d ){` |
|    182 | 3194 | `			zPtr   = &zIn[1];` |
|    182 | 3195 | `			zPtr2  = &zpIn[1];` |
|    141 | 3196 | `			for(;;){` |
|    284 | 3197 | `				if( zPtr2 >= zpEnd ){` |
|      - | 3198 | `					/* Pattern found */` |
|     41 | 3199 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 3200 | `					return SXRET_OK;` |
|      - | 3201 | `				}` |
|    244 | 3202 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 3203 | `					break;` |
|      - | 3204 | `				}` |
|    244 | 3205 | `				c = SyToLower(zPtr[0]);` |
|    244 | 3206 | `				d = SyToLower(zPtr2[0]);` |
|    244 | 3207 | `				if( c != d ){` |
|    142 | 3208 | `					break;` |
|      - | 3209 | `				}` |
|    103 | 3210 | `				zPtr++; zPtr2++;` |
|      1 | 3211 | `			}` |
|     70 | 3212 | `		}` |
|   1490 | 3213 | `		zIn++;` |
|      2 | 3214 | `	}` |
|      - | 3215 | `	/* Pattern not found */` |
|    194 | 3216 | `	return SXERR_NOTFOUND;` |
|    151 | 3217 | `}` |
|      - | 3218 | `/*` |
|      - | 3219 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3220 | ` *  Find the first occurrence of a string.` |
|      - | 3221 | ` * Parameters` |
|      - | 3222 | ` *  $haystack` |
|      - | 3223 | ` *   The input string.` |
|      - | 3224 | ` * $needle` |
|      - | 3225 | ` *   Search pattern (must be a string).` |
|      - | 3226 | ` * $before_needle` |
|      - | 3227 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3228 | ` *   of the needle (excluding the needle).` |
|      - | 3229 | ` * Return` |
|      - | 3230 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3231 | ` */` |
|      6 | 3232 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3233 | `{` |
|      7 | 3234 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3235 | `	const char *zBlob,*zPattern;` |
|      - | 3236 | `	int nLen,nPatLen;` |
|      - | 3237 | `	sxu32 nOfft;` |
|      - | 3238 | `	sxi32 rc;` |
|      7 | 3239 | `	if( nArg < 2 ){` |
|      - | 3240 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3241 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3242 | `		return PH7_OK;` |
|      - | 3243 | `	}` |
|      - | 3244 | `	/* Extract the needle and the haystack */` |
|      7 | 3245 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 3246 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 3247 | `	nOfft = 0; /* cc warning */` |
|      9 | 3248 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3249 | `		int before = 0;` |
|      - | 3250 | `		/* Perform the lookup */` |
|      5 | 3251 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3252 | `		if( rc != SXRET_OK ){` |
|      - | 3253 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3254 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3255 | `			return PH7_OK;` |
|      - | 3256 | `		}` |
|      - | 3257 | `		/* Return the portion of the string */` |
|      5 | 3258 | `		if( nArg > 2 ){` |
|      3 | 3259 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3260 | `		}` |
|      5 | 3261 | `		if( before ){` |
|      3 | 3262 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3263 | `		}else{` |
|      3 | 3264 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3265 | `		}` |
|      3 | 3266 | `	}else{` |
|      3 | 3267 | `		ph7_result_bool(pCtx,0);` |
|      - | 3268 | `	}` |
|      7 | 3269 | `	return PH7_OK;` |
|      4 | 3270 | `}` |
|      - | 3271 | `/*` |
|      - | 3272 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3273 | ` *  Case-insensitive strstr().` |
|      - | 3274 | ` * Parameters` |
|      - | 3275 | ` *  $haystack` |
|      - | 3276 | ` *   The input string.` |
|      - | 3277 | ` * $needle` |
|      - | 3278 | ` *   Search pattern (must be a string).` |
|      - | 3279 | ` * $before_needle` |
|      - | 3280 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3281 | ` *   of the needle (excluding the needle).` |
|      - | 3282 | ` * Return` |
|      - | 3283 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3284 | ` */` |
|      4 | 3285 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3286 | `{` |
|      5 | 3287 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3288 | `	const char *zBlob,*zPattern;` |
|      - | 3289 | `	int nLen,nPatLen;` |
|      - | 3290 | `	sxu32 nOfft;` |
|      - | 3291 | `	sxi32 rc;` |
|      5 | 3292 | `	if( nArg < 2 ){` |
|      - | 3293 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3294 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3295 | `		return PH7_OK;` |
|      - | 3296 | `	}` |
|      - | 3297 | `	/* Extract the needle and the haystack */` |
|      5 | 3298 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3299 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 3300 | `	nOfft = 0; /* cc warning */` |
|      7 | 3301 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3302 | `		int before = 0;` |
|      - | 3303 | `		/* Perform the lookup */` |
|      5 | 3304 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3305 | `		if( rc != SXRET_OK ){` |
|      - | 3306 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3307 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3308 | `			return PH7_OK;` |
|      - | 3309 | `		}` |
|      - | 3310 | `		/* Return the portion of the string */` |
|      5 | 3311 | `		if( nArg > 2 ){` |
|      3 | 3312 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3313 | `		}` |
|      5 | 3314 | `		if( before ){` |
|      3 | 3315 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3316 | `		}else{` |
|      3 | 3317 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3318 | `		}` |
|      3 | 3319 | `	}else{` |
|    ! 0 | 3320 | `		ph7_result_bool(pCtx,0);` |
|      - | 3321 | `	}` |
|      5 | 3322 | `	return PH7_OK;` |
|      3 | 3323 | `}` |
|      - | 3324 | `/*` |
|      - | 3325 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3326 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 3327 | ` * Parameters` |
|      - | 3328 | ` *  $haystack` |
|      - | 3329 | ` *   The input string.` |
|      - | 3330 | ` * $needle` |
|      - | 3331 | ` *   Search pattern (must be a string).` |
|      - | 3332 | ` * $offset` |
|      - | 3333 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3334 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3335 | ` *   of haystack.` |
|      - | 3336 | ` * Return` |
|      - | 3337 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3338 | ` */` |
|   1468 | 3339 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3340 | `{` |
|   1473 | 3341 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strpos",1,"$haystack"); }` |
|   1473 | 3342 | `	if( nArg > 1 ){ StrNullArgNotice(pCtx,apArg[1],"strpos",2,"$needle"); }` |
|   1473 | 3343 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3344 | `	const char *zBlob,*zPattern;` |
|      - | 3345 | `	int nLen,nPatLen,nStart;` |
|      - | 3346 | `	sxu32 nOfft;` |
|      - | 3347 | `	sxi32 rc;` |
|   1473 | 3348 | `	if( nArg < 2 ){` |
|      - | 3349 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3350 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3351 | `		return PH7_OK;` |
|      - | 3352 | `	}` |
|      - | 3353 | `	/* Extract the needle and the haystack */` |
|   1473 | 3354 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|   1473 | 3355 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|   1473 | 3356 | `	nOfft = 0; /* cc warning */` |
|   1473 | 3357 | `	nStart = 0;` |
|      - | 3358 | `	/* Peek the starting offset if available */` |
|   1473 | 3359 | `	if( nArg > 2 ){` |
|     15 | 3360 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3361 | `		if( nStart < 0 ){` |
|    ! 0 | 3362 | `			nStart = -nStart;` |
|    ! 0 | 3363 | `		}` |
|     15 | 3364 | `		if( nStart >= nLen ){` |
|      - | 3365 | `			/* Invalid offset */` |
|    ! 0 | 3366 | `			nStart = 0;` |
|    ! 0 | 3367 | `		}else{` |
|     15 | 3368 | `			zBlob += nStart;` |
|     15 | 3369 | `			nLen -= nStart;` |
|      - | 3370 | `		}` |
|      7 | 3371 | `	}` |
|   1473 | 3372 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3373 | `		/* Perform the lookup */` |
|   1471 | 3374 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|   1471 | 3375 | `		if( rc != SXRET_OK ){` |
|      - | 3376 | `			/* Pattern not found,return FALSE */` |
|    779 | 3377 | `			ph7_result_bool(pCtx,0);` |
|    779 | 3378 | `			return PH7_OK;` |
|      - | 3379 | `		}` |
|      - | 3380 | `		/* Return the pattern position */` |
|    696 | 3381 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|    350 | 3382 | `	}else{` |
|      3 | 3383 | `		ph7_result_bool(pCtx,0);` |
|      - | 3384 | `	}` |
|    698 | 3385 | `	return PH7_OK;` |
|    739 | 3386 | `}` |
|      - | 3387 | `/*` |
|      - | 3388 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|      - | 3389 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|      - | 3390 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|      - | 3391 | ` * TypeError for arrays, resources, and objects without __toString.` |
|      - | 3392 | ` *` |
|      - | 3393 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|      - | 3394 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|      - | 3395 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|      - | 3396 | ` *` |
|      - | 3397 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|      - | 3398 | ` * is valid until pTmp is released or pArg is mutated.` |
|      - | 3399 | ` */` |
|    744 | 3400 | `static sxi32 StrPredicateResolveArg(` |
|      - | 3401 | `	ph7_context *pCtx,` |
|      - | 3402 | `	ph7_value *pArg,` |
|      - | 3403 | `	const char *zFunc,` |
|      - | 3404 | `	int iArgNum,` |
|      - | 3405 | `	const char *zParamName,` |
|      - | 3406 | `	const char *zTypeStr, /* Declared type in the TypeError, e.g. "string" / "?string" */` |
|      - | 3407 | `	const char *zNullMsg,` |
|      - | 3408 | `	ph7_value *pTmp,` |
|      - | 3409 | `	const char **pzOut,` |
|      - | 3410 | `	int *pnOut` |
|      4 | 3411 | `){` |
|    748 | 3412 | `	if( ph7_value_is_null(pArg) ){` |
|     13 | 3413 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zNullMsg);` |
|     13 | 3414 | `		*pzOut = "";` |
|     13 | 3415 | `		*pnOut = 0;` |
|     13 | 3416 | `		return PH7_OK;` |
|      - | 3417 | `	}` |
|   1124 | 3418 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|    706 | 3419 | `	    ( ph7_value_is_object(pArg) &&` |
|    105 | 3420 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|     70 | 3421 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|     35 | 3422 | `	        "__toString",sizeof("__toString")-1) == 0` |
|      - | 3423 | `	    )` |
|      - | 3424 | `	){` |
|     52 | 3425 | `		const char *zType = ph7_type_name(pArg);` |
|     52 | 3426 | `		if( ph7_value_is_object(pArg) ){` |
|     23 | 3427 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     23 | 3428 | `			if( pInst && pInst->pClass ){` |
|     23 | 3429 | `				zType = SyStringData(&pInst->pClass->sName);` |
|     11 | 3430 | `			}` |
|     11 | 3431 | `		}` |
|     76 | 3432 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3433 | `			"TypeError",` |
|      - | 3434 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|     24 | 3435 | `			zFunc, iArgNum, zParamName, zTypeStr, zType` |
|      - | 3436 | `			);` |
|      - | 3437 | `	}` |
|    686 | 3438 | `	if( ph7_value_is_object(pArg) ){` |
|     49 | 3439 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     49 | 3440 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 3441 | `			"__toString",sizeof("__toString")-1);` |
|     49 | 3442 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|     49 | 3443 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|     49 | 3444 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|     49 | 3445 | `		return PH7_OK;` |
|      - | 3446 | `	}` |
|    638 | 3447 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|    638 | 3448 | `	return PH7_OK;` |
|    376 | 3449 | `}` |
|      - | 3450 | `/*` |
|      - | 3451 | ` * bool str_contains(string $haystack, string $needle)` |
|      - | 3452 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|      - | 3453 | ` * Return` |
|      - | 3454 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|      - | 3455 | ` */` |
|    108 | 3456 | `static int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 3457 | `{` |
|      - | 3458 | `	const char *zHaystack,*zNeedle;` |
|      - | 3459 | `	int nHayLen,nNeedleLen;` |
|      - | 3460 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3461 | `	sxi32 rc;` |
|    112 | 3462 | `	if( nArg != 2 ){` |
|     18 | 3463 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3464 | `			"ArgumentCountError",` |
|      - | 3465 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|      5 | 3466 | `			nArg` |
|      - | 3467 | `			);` |
|      - | 3468 | `	}` |
|    101 | 3469 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|    101 | 3470 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|    101 | 3471 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack","string",` |
|      - | 3472 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|      - | 3473 | `		"of type string is deprecated",` |
|      - | 3474 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|    101 | 3475 | `	if( rc != PH7_OK ) goto out;` |
|     94 | 3476 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle","string",` |
|      - | 3477 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|      - | 3478 | `		"of type string is deprecated",` |
|      - | 3479 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     94 | 3480 | `	if( rc != PH7_OK ) goto out;` |
|     90 | 3481 | `	if( nNeedleLen < 1 ){` |
|     13 | 3482 | `		ph7_result_bool(pCtx,1);` |
|     84 | 3483 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3484 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3485 | `	}else{` |
|    104 | 3486 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|     34 | 3487 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|     70 | 3488 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|      - | 3489 | `	}` |
|     90 | 3490 | `	rc = PH7_OK;` |
|     49 | 3491 | `out:` |
|    101 | 3492 | `	PH7_MemObjRelease(&sHayTmp);` |
|    101 | 3493 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|    101 | 3494 | `	return rc;` |
|     58 | 3495 | `}` |
|      - | 3496 | `/*` |
|      - | 3497 | ` * bool str_starts_with(string $haystack, string $needle)` |
|      - | 3498 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|      - | 3499 | ` * Return` |
|      - | 3500 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|      - | 3501 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3502 | ` */` |
|     78 | 3503 | `static int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 3504 | `{` |
|      - | 3505 | `	const char *zHaystack,*zNeedle;` |
|      - | 3506 | `	int nHayLen,nNeedleLen;` |
|      - | 3507 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3508 | `	sxi32 rc;` |
|     82 | 3509 | `	if( nArg != 2 ){` |
|     18 | 3510 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3511 | `			"ArgumentCountError",` |
|      - | 3512 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|      5 | 3513 | `			nArg` |
|      - | 3514 | `			);` |
|      - | 3515 | `	}` |
|     70 | 3516 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 3517 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 3518 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack","string",` |
|      - | 3519 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3520 | `		"of type string is deprecated",` |
|      - | 3521 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 3522 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 3523 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle","string",` |
|      - | 3524 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3525 | `		"of type string is deprecated",` |
|      - | 3526 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 3527 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3528 | `	if( nNeedleLen < 1 ){` |
|     13 | 3529 | `		ph7_result_bool(pCtx,1);` |
|     53 | 3530 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3531 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3532 | `	}else{` |
|     58 | 3533 | `		ph7_result_bool(pCtx,` |
|     38 | 3534 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3535 | `	}` |
|     59 | 3536 | `	rc = PH7_OK;` |
|     34 | 3537 | `out:` |
|     70 | 3538 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 3539 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 3540 | `	return rc;` |
|     43 | 3541 | `}` |
|      - | 3542 | `/*` |
|      - | 3543 | ` * bool str_ends_with(string $haystack, string $needle)` |
|      - | 3544 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|      - | 3545 | ` * Return` |
|      - | 3546 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|      - | 3547 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3548 | ` */` |
|     78 | 3549 | `static int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 3550 | `{` |
|      - | 3551 | `	const char *zHaystack,*zNeedle;` |
|      - | 3552 | `	int nHayLen,nNeedleLen;` |
|      - | 3553 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3554 | `	sxi32 rc;` |
|     82 | 3555 | `	if( nArg != 2 ){` |
|     18 | 3556 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3557 | `			"ArgumentCountError",` |
|      - | 3558 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|      5 | 3559 | `			nArg` |
|      - | 3560 | `			);` |
|      - | 3561 | `	}` |
|     70 | 3562 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 3563 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 3564 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack","string",` |
|      - | 3565 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3566 | `		"of type string is deprecated",` |
|      - | 3567 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 3568 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 3569 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle","string",` |
|      - | 3570 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3571 | `		"of type string is deprecated",` |
|      - | 3572 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 3573 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3574 | `	if( nNeedleLen < 1 ){` |
|     13 | 3575 | `		ph7_result_bool(pCtx,1);` |
|     53 | 3576 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3577 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3578 | `	}else{` |
|     58 | 3579 | `		ph7_result_bool(pCtx,` |
|     38 | 3580 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3581 | `	}` |
|     59 | 3582 | `	rc = PH7_OK;` |
|     34 | 3583 | `out:` |
|     70 | 3584 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 3585 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 3586 | `	return rc;` |
|     43 | 3587 | `}` |
|      - | 3588 | `/*` |
|      - | 3589 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3590 | ` *  Case-insensitive strpos.` |
|      - | 3591 | ` * Parameters` |
|      - | 3592 | ` *  $haystack` |
|      - | 3593 | ` *   The input string.` |
|      - | 3594 | ` * $needle` |
|      - | 3595 | ` *   Search pattern (must be a string).` |
|      - | 3596 | ` * $offset` |
|      - | 3597 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3598 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3599 | ` *   of haystack.` |
|      - | 3600 | ` * Return` |
|      - | 3601 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3602 | ` */` |
|    196 | 3603 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3604 | `{` |
|    198 | 3605 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3606 | `	const char *zBlob,*zPattern;` |
|      - | 3607 | `	int nLen,nPatLen,nStart;` |
|      - | 3608 | `	sxu32 nOfft;` |
|      - | 3609 | `	sxi32 rc;` |
|    198 | 3610 | `	if( nArg < 2 ){` |
|      - | 3611 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3612 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3613 | `		return PH7_OK;` |
|      - | 3614 | `	}` |
|      - | 3615 | `	/* Extract the needle and the haystack */` |
|    198 | 3616 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    198 | 3617 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    198 | 3618 | `	nOfft = 0; /* cc warning */` |
|    198 | 3619 | `	nStart = 0;` |
|      - | 3620 | `	/* Peek the starting offset if available */` |
|    198 | 3621 | `	if( nArg > 2 ){` |
|      5 | 3622 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 3623 | `		if( nStart < 0 ){` |
|      3 | 3624 | `			nStart = -nStart;` |
|      1 | 3625 | `		}` |
|      5 | 3626 | `		if( nStart >= nLen ){` |
|      - | 3627 | `			/* Invalid offset */` |
|    ! 0 | 3628 | `			nStart = 0;` |
|    ! 0 | 3629 | `		}else{` |
|      5 | 3630 | `			zBlob += nStart;` |
|      5 | 3631 | `			nLen -= nStart;` |
|      - | 3632 | `		}` |
|      2 | 3633 | `	}` |
|    198 | 3634 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3635 | `		/* Perform the lookup */` |
|    198 | 3636 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    198 | 3637 | `		if( rc != SXRET_OK ){` |
|      - | 3638 | `			/* Pattern not found,return FALSE */` |
|    184 | 3639 | `			ph7_result_bool(pCtx,0);` |
|    184 | 3640 | `			return PH7_OK;` |
|      - | 3641 | `		}` |
|      - | 3642 | `		/* Return the pattern position */` |
|     15 | 3643 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 3644 | `	}else{` |
|    ! 0 | 3645 | `		ph7_result_bool(pCtx,0);` |
|      - | 3646 | `	}` |
|     15 | 3647 | `	return PH7_OK;` |
|    100 | 3648 | `}` |
|      - | 3649 | `/*` |
|      - | 3650 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3651 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 3652 | ` * Parameters` |
|      - | 3653 | ` *  $haystack` |
|      - | 3654 | ` *   The input string.` |
|      - | 3655 | ` * $needle` |
|      - | 3656 | ` *   Search pattern (must be a string).` |
|      - | 3657 | ` * $offset` |
|      - | 3658 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3659 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3660 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3661 | ` * Return` |
|      - | 3662 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3663 | ` */` |
|     40 | 3664 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3665 | `{` |
|      - | 3666 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     41 | 3667 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3668 | `	int nLen,nPatLen;` |
|      - | 3669 | `	sxu32 nOfft;` |
|      - | 3670 | `	sxi32 rc;` |
|     41 | 3671 | `	if( nArg < 2 ){` |
|      - | 3672 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3673 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3674 | `		return PH7_OK;` |
|      - | 3675 | `	}` |
|      - | 3676 | `	/* Extract the needle and the haystack */` |
|     41 | 3677 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 3678 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3679 | `	/* Point to the end of the pattern */` |
|     41 | 3680 | `	zPtr = &zBlob[nLen - 1];` |
|     41 | 3681 | `	zEnd = &zBlob[nLen];` |
|      - | 3682 | `	/* Save the starting posistion */` |
|     41 | 3683 | `	zStart = zBlob;` |
|     41 | 3684 | `	nOfft = 0; /* cc warning */` |
|      - | 3685 | `	/* Peek the starting offset if available */` |
|     41 | 3686 | `	if( nArg > 2 ){` |
|      - | 3687 | `		int nStart;` |
|     21 | 3688 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 3689 | `		if( nStart < 0 ){` |
|     11 | 3690 | `			nStart = -nStart;` |
|     11 | 3691 | `			if( nStart >= nLen ){` |
|      - | 3692 | `				/* Invalid offset */` |
|      3 | 3693 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3694 | `				return PH7_OK;` |
|    ! 0 | 3695 | `			}else{` |
|      9 | 3696 | `				nLen -= nStart;` |
|      9 | 3697 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 3698 | `				zEnd = &zBlob[nLen];` |
|      - | 3699 | `			}` |
|      5 | 3700 | `		}else{` |
|     11 | 3701 | `			if( nStart >= nLen ){` |
|      - | 3702 | `				/* Invalid offset */` |
|      5 | 3703 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3704 | `				return PH7_OK;` |
|    ! 0 | 3705 | `			}else{` |
|      7 | 3706 | `				zBlob += nStart;` |
|      7 | 3707 | `				nLen -= nStart;` |
|      - | 3708 | `			}` |
|      - | 3709 | `		}` |
|      7 | 3710 | `	}` |
|     35 | 3711 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3712 | `		/* Perform the lookup */` |
|    121 | 3713 | `		for(;;){` |
|    243 | 3714 | `			if( zBlob >= zPtr ){` |
|     21 | 3715 | `				break;` |
|      - | 3716 | `			}` |
|    223 | 3717 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    223 | 3718 | `			if( rc == SXRET_OK ){` |
|      - | 3719 | `				/* Pattern found,return it's position */` |
|     13 | 3720 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 3721 | `				return PH7_OK;` |
|      - | 3722 | `			}` |
|    211 | 3723 | `			zPtr--;` |
|      1 | 3724 | `		}` |
|      - | 3725 | `		/* Pattern not found,return FALSE */` |
|     21 | 3726 | `		ph7_result_bool(pCtx,0);` |
|     11 | 3727 | `	}else{` |
|      3 | 3728 | `		ph7_result_bool(pCtx,0);` |
|      - | 3729 | `	}` |
|     23 | 3730 | `	return PH7_OK;` |
|     21 | 3731 | `}` |
|      - | 3732 | `/*` |
|      - | 3733 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3734 | ` *  Case-insensitive strrpos.` |
|      - | 3735 | ` * Parameters` |
|      - | 3736 | ` *  $haystack` |
|      - | 3737 | ` *   The input string.` |
|      - | 3738 | ` * $needle` |
|      - | 3739 | ` *   Search pattern (must be a string).` |
|      - | 3740 | ` * $offset` |
|      - | 3741 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3742 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3743 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3744 | ` * Return` |
|      - | 3745 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3746 | ` */` |
|     26 | 3747 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3748 | `{` |
|      - | 3749 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     27 | 3750 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3751 | `	int nLen,nPatLen;` |
|      - | 3752 | `	sxu32 nOfft;` |
|      - | 3753 | `	sxi32 rc;` |
|     27 | 3754 | `	if( nArg < 2 ){` |
|      - | 3755 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3756 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3757 | `		return PH7_OK;` |
|      - | 3758 | `	}` |
|      - | 3759 | `	/* Extract the needle and the haystack */` |
|     27 | 3760 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 3761 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3762 | `	/* Point to the end of the pattern */` |
|     27 | 3763 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 3764 | `	zEnd = &zBlob[nLen];` |
|      - | 3765 | `	/* Save the starting posistion */` |
|     27 | 3766 | `	zStart = zBlob;` |
|     27 | 3767 | `	nOfft = 0; /* cc warning */` |
|      - | 3768 | `	/* Peek the starting offset if available */` |
|     27 | 3769 | `	if( nArg > 2 ){` |
|      - | 3770 | `		int nStart;` |
|     15 | 3771 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3772 | `		if( nStart < 0 ){` |
|      7 | 3773 | `			nStart = -nStart;` |
|      7 | 3774 | `			if( nStart >= nLen ){` |
|      - | 3775 | `				/* Invalid offset */` |
|      3 | 3776 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3777 | `				return PH7_OK;` |
|    ! 0 | 3778 | `			}else{` |
|      5 | 3779 | `				nLen -= nStart;` |
|      5 | 3780 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 3781 | `				zEnd = &zBlob[nLen];` |
|      - | 3782 | `			}` |
|      3 | 3783 | `		}else{` |
|      9 | 3784 | `			if( nStart >= nLen ){` |
|      - | 3785 | `				/* Invalid offset */` |
|      5 | 3786 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3787 | `				return PH7_OK;` |
|    ! 0 | 3788 | `			}else{` |
|      5 | 3789 | `				zBlob += nStart;` |
|      5 | 3790 | `				nLen -= nStart;` |
|      - | 3791 | `			}` |
|      - | 3792 | `		}` |
|      4 | 3793 | `	}` |
|     21 | 3794 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3795 | `		/* Perform the lookup */` |
|     44 | 3796 | `		for(;;){` |
|     89 | 3797 | `			if( zBlob >= zPtr ){` |
|      9 | 3798 | `				break;` |
|      - | 3799 | `			}` |
|     81 | 3800 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 3801 | `			if( rc == SXRET_OK ){` |
|      - | 3802 | `				/* Pattern found,return it's position */` |
|     11 | 3803 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 3804 | `				return PH7_OK;` |
|      - | 3805 | `			}` |
|     71 | 3806 | `			zPtr--;` |
|      1 | 3807 | `		}` |
|      - | 3808 | `		/* Pattern not found,return FALSE */` |
|      9 | 3809 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3810 | `	}else{` |
|      3 | 3811 | `		ph7_result_bool(pCtx,0);` |
|      - | 3812 | `	}` |
|     11 | 3813 | `	return PH7_OK;` |
|     14 | 3814 | `}` |
|      - | 3815 | `/*` |
|      - | 3816 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3817 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3818 | ` * Parameters` |
|      - | 3819 | ` *  $haystack` |
|      - | 3820 | ` *   The input string.` |
|      - | 3821 | ` * $needle` |
|      - | 3822 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3823 | ` *  This behavior is different from that of strstr().` |
|      - | 3824 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3825 | ` *  as the ordinal value of a character.` |
|      - | 3826 | ` * Return` |
|      - | 3827 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3828 | ` */` |
|     22 | 3829 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3830 | `{` |
|      - | 3831 | `	const char *zBlob;` |
|      - | 3832 | `	int nLen,c;` |
|     23 | 3833 | `	if( nArg < 2 ){` |
|      - | 3834 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3835 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3836 | `		return PH7_OK;` |
|      - | 3837 | `	}` |
|      - | 3838 | `	/* Extract the haystack */` |
|     23 | 3839 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3840 | `	c = 0; /* cc warning */` |
|     23 | 3841 | `	if( nLen > 0 ){` |
|      - | 3842 | `		sxu32 nOfft;` |
|      - | 3843 | `		sxi32 rc;` |
|     21 | 3844 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 3845 | `			const char *zPattern;` |
|     11 | 3846 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 3847 | `														 * for NULL pointer.` |
|      - | 3848 | `														 */` |
|     11 | 3849 | `			c = zPattern[0];` |
|      6 | 3850 | `		}else{` |
|      - | 3851 | `			/* Int cast */` |
|     11 | 3852 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 3853 | `		}` |
|      - | 3854 | `		/* Perform the lookup */` |
|     21 | 3855 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3856 | `		if( rc != SXRET_OK ){` |
|      - | 3857 | `			/* No such entry,return FALSE */` |
|      7 | 3858 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3859 | `			return PH7_OK;` |
|      - | 3860 | `		}` |
|      - | 3861 | `		/* Return the string portion */` |
|     15 | 3862 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 3863 | `	}else{` |
|      3 | 3864 | `		ph7_result_bool(pCtx,0);` |
|      - | 3865 | `	}` |
|     17 | 3866 | `	return PH7_OK;` |
|     12 | 3867 | `}` |
|      - | 3868 | `/*` |
|      - | 3869 | ` * string strrev(string $string)` |
|      - | 3870 | ` *  Reverse a string.` |
|      - | 3871 | ` * Parameters` |
|      - | 3872 | ` *  $string` |
|      - | 3873 | ` *   String to be reversed.` |
|      - | 3874 | ` * Return` |
|      - | 3875 | ` *  The reversed string.` |
|      - | 3876 | ` */` |
|      2 | 3877 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3878 | `{` |
|      - | 3879 | `	const char *zIn,*zEnd;` |
|      - | 3880 | `	int nLen,c;` |
|      3 | 3881 | `	if( nArg < 1 ){` |
|      - | 3882 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3883 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3884 | `		return PH7_OK;` |
|      - | 3885 | `	}` |
|      - | 3886 | `	/* Extract the target string */` |
|      3 | 3887 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3888 | `	if( nLen < 1 ){` |
|      - | 3889 | `		/* Empty string Return null */` |
|    ! 0 | 3890 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3891 | `		return PH7_OK;` |
|      - | 3892 | `	}` |
|      - | 3893 | `	/* Perform the requested operation */` |
|      3 | 3894 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3895 | `	for(;;){` |
|      9 | 3896 | `		if( zEnd < zIn ){` |
|      - | 3897 | `			/* No more input to process */` |
|      3 | 3898 | `			break;` |
|      - | 3899 | `		}` |
|      - | 3900 | `		/* Append current character */` |
|      7 | 3901 | `		c = zEnd[0];` |
|      7 | 3902 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3903 | `		zEnd--;` |
|      1 | 3904 | `	}` |
|      3 | 3905 | `	return PH7_OK;` |
|      2 | 3906 | `}` |
|      - | 3907 | `/*` |
|      - | 3908 | ` * string ucwords(string $string [, string $separators = " \t\r\n\f\v"])` |
|      - | 3909 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3910 | ` *  A word begins at the start of the string and after any character present in` |
|      - | 3911 | ` *  $separators. The default separators are the whitespace characters (space,` |
|      - | 3912 | ` *  horizontal tab, carriage return, newline, form-feed and vertical tab); an` |
|      - | 3913 | ` *  explicit $separators argument REPLACES them (an empty string leaves only the` |
|      - | 3914 | ` *  very first character upper-cased). Like PHP, this is byte-based: only ASCII` |
|      - | 3915 | ` *  bytes are upper-cased and a byte is a separator only if it appears in the set.` |
|      - | 3916 | ` * Parameters` |
|      - | 3917 | ` *  $string` |
|      - | 3918 | ` *   The input string.` |
|      - | 3919 | ` *  $separators` |
|      - | 3920 | ` *   The optional word-boundary characters.` |
|      - | 3921 | ` * Return` |
|      - | 3922 | ` *  The modified string.` |
|      - | 3923 | ` */` |
|     22 | 3924 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3925 | `{` |
|      - | 3926 | `	const char *zIn;` |
|      - | 3927 | `	int nLen,i,iStart;` |
|      - | 3928 | `	char aDelim[256];` |
|     23 | 3929 | `	if( nArg < 1 ){` |
|      - | 3930 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3931 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3932 | `		return PH7_OK;` |
|      - | 3933 | `	}` |
|      - | 3934 | `	/* Build the separator membership table: an explicit $separators argument` |
|      - | 3935 | `	 * replaces the default whitespace set (an empty string clears it). */` |
|     23 | 3936 | `	SyZero(aDelim,(sxu32)sizeof(aDelim));` |
|     23 | 3937 | `	if( nArg > 1 ){` |
|      - | 3938 | `		int nDelim;` |
|      9 | 3939 | `		const char *zDelim = ph7_value_to_string(apArg[1],&nDelim);` |
|     17 | 3940 | `		for( i = 0 ; i < nDelim ; i++ ){` |
|      9 | 3941 | `			aDelim[(unsigned char)zDelim[i]] = 1;` |
|      5 | 3942 | `		}` |
|      5 | 3943 | `	}else{` |
|     15 | 3944 | `		aDelim[(unsigned char)' ']  = 1;` |
|     15 | 3945 | `		aDelim[(unsigned char)'\t'] = 1;` |
|     15 | 3946 | `		aDelim[(unsigned char)'\r'] = 1;` |
|     15 | 3947 | `		aDelim[(unsigned char)'\n'] = 1;` |
|     15 | 3948 | `		aDelim[(unsigned char)'\f'] = 1;` |
|     15 | 3949 | `		aDelim[(unsigned char)'\v'] = 1;` |
|      - | 3950 | `	}` |
|      - | 3951 | `	/* Extract the target string */` |
|     23 | 3952 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3953 | `	if( nLen < 1 ){` |
|      - | 3954 | `		/* Empty string – match PHP semantics */` |
|      3 | 3955 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3956 | `		return PH7_OK;` |
|      - | 3957 | `	}` |
|      - | 3958 | `	/* Upper-case the first byte of each word (the leading byte, or any byte that` |
|      - | 3959 | `	 * follows a separator), appending the untouched runs in between verbatim. */` |
|     21 | 3960 | `	iStart = 0;` |
|    309 | 3961 | `	for( i = 0 ; i < nLen ; i++ ){` |
|    289 | 3962 | `		int c = (unsigned char)zIn[i];` |
|    289 | 3963 | `		if( (i == 0 \|\| aDelim[(unsigned char)zIn[i-1]]) && c < 0x80 && SyisLower(c) ){` |
|     53 | 3964 | `			char up = (char)SyToUpper(c);` |
|     53 | 3965 | `			if( i > iStart ){` |
|     35 | 3966 | `				ph7_result_string(pCtx,&zIn[iStart],i - iStart);` |
|     17 | 3967 | `			}` |
|     53 | 3968 | `			ph7_result_string(pCtx,&up,1);` |
|     53 | 3969 | `			iStart = i + 1;` |
|     26 | 3970 | `		}` |
|    145 | 3971 | `	}` |
|     21 | 3972 | `	if( nLen > iStart ){` |
|     21 | 3973 | `		ph7_result_string(pCtx,&zIn[iStart],nLen - iStart);` |
|     10 | 3974 | `	}` |
|     21 | 3975 | `	return PH7_OK;` |
|     12 | 3976 | `}` |
|      - | 3977 | `/*` |
|      - | 3978 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3979 | ` *  Returns input repeated multiplier times.` |
|      - | 3980 | ` * Parameters` |
|      - | 3981 | ` *  $string` |
|      - | 3982 | ` *   String to be repeated.` |
|      - | 3983 | ` * $multiplier` |
|      - | 3984 | ` *  Number of time the input string should be repeated.` |
|      - | 3985 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3986 | ` *  to 0, the function will return an empty string.` |
|      - | 3987 | ` * Return` |
|      - | 3988 | ` *  The repeated string.` |
|      - | 3989 | ` */` |
|  20434 | 3990 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3991 | `{` |
|      - | 3992 | `	const char *zIn;` |
|      - | 3993 | `	int nLen;` |
|      - | 3994 | `	ph7_int64 nMul;` |
|      - | 3995 | `	int rc;` |
|  20436 | 3996 | `	if( nArg < 2 ){` |
|      - | 3997 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3998 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3999 | `		return PH7_OK;` |
|      - | 4000 | `	}` |
|      - | 4001 | `	/* Extract the target string */` |
|  20436 | 4002 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4003 | `	/* Resolve $times through the shared ZPP helper so a lossy float / float-string` |
|      - | 4004 | `	 * carries php's precision deprecation and NAN/INF/non-numeric fail with php's` |
|      - | 4005 | `	 * TypeError — a bare ph7_value_to_int64() coerced them silently. */` |
|      - | 4006 | `	{` |
|  20436 | 4007 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_repeat",2,"$times","int",&nMul);` |
|  20436 | 4008 | `		if( rcArg != PH7_OK ){` |
|    ! 0 | 4009 | `			return rcArg;` |
|      - | 4010 | `		}` |
|      - | 4011 | `	}` |
|  20436 | 4012 | `	if( nMul < 0 ){` |
|      3 | 4013 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4014 | `			"str_repeat(): Argument #2 ($times) must be greater than or equal to 0");` |
|      - | 4015 | `	}` |
|  20434 | 4016 | `	if( nLen < 1 \|\| nMul < 1 ){` |
|      - | 4017 | `		/* Empty input or a zero multiplier yields the empty string (PHP). */` |
|    ! 0 | 4018 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4019 | `		return PH7_OK;` |
|      - | 4020 | `	}` |
|      - | 4021 | `	/* Perform the requested operation */` |
| 221930 | 4022 | `	for(;;){` |
| 443862 | 4023 | `		if( !nMul ){` |
|  20434 | 4024 | `			break;` |
|      - | 4025 | `		}` |
|      - | 4026 | `		/* Append the copy */` |
| 423430 | 4027 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 423430 | 4028 | `		if( rc != PH7_OK ){` |
|      - | 4029 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 4030 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 4031 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 4032 | `		}` |
| 423430 | 4033 | `		nMul--;` |
|      2 | 4034 | `	}` |
|  20434 | 4035 | `	return PH7_OK;` |
|  10219 | 4036 | `}` |
|      - | 4037 | `/*` |
|      - | 4038 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 4039 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 4040 | ` * Parameters` |
|      - | 4041 | ` *  $string` |
|      - | 4042 | ` *   The input string.` |
|      - | 4043 | ` * $is_xhtml` |
|      - | 4044 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 4045 | ` * Return` |
|      - | 4046 | ` *  The processed string.` |
|      - | 4047 | ` */` |
|      4 | 4048 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4049 | `{` |
|      - | 4050 | `	const char *zIn,*zCur,*zEnd;` |
|      5 | 4051 | `	int is_xhtml = 1; /* Default to XHTML-style '<br />' like PHP */` |
|      - | 4052 | `	int nLen;` |
|      5 | 4053 | `	if( nArg < 1 ){` |
|      - | 4054 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4055 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4056 | `		return PH7_OK;` |
|      - | 4057 | `	}` |
|      - | 4058 | `	/* Extract the target string */` |
|      5 | 4059 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 4060 | `	if( nLen < 1 ){` |
|      - | 4061 | `		/* Empty string,return null */` |
|    ! 0 | 4062 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4063 | `		return PH7_OK;` |
|      - | 4064 | `	}` |
|      5 | 4065 | `	if( nArg > 1 ){` |
|      3 | 4066 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 4067 | `	}` |
|      5 | 4068 | `	zEnd = &zIn[nLen];` |
|      - | 4069 | `	/* Perform the requested operation */` |
|      4 | 4070 | `	for(;;){` |
|      9 | 4071 | `		zCur = zIn;` |
|      - | 4072 | `		/* Delimit the string */` |
|     21 | 4073 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 4074 | `			zIn++;` |
|      1 | 4075 | `		}` |
|      9 | 4076 | `		if( zCur < zIn ){` |
|      - | 4077 | `			/* Output chunk verbatim */` |
|      9 | 4078 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 4079 | `		}` |
|      9 | 4080 | `		if( zIn >= zEnd ){` |
|      - | 4081 | `			/* No more input to process */` |
|      5 | 4082 | `			break;` |
|      - | 4083 | `		}` |
|      - | 4084 | `		/* Output the HTML line break */` |
|      - | 4085 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br />' (space before the slash), otherwise use '<br>' */` |
|      5 | 4086 | `		if( is_xhtml ){` |
|      3 | 4087 | `			ph7_result_string(pCtx,"<br />",(int)sizeof("<br />")-1);` |
|      2 | 4088 | `		}else{` |
|      3 | 4089 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 4090 | `		}` |
|      5 | 4091 | `		zCur = zIn;` |
|      - | 4092 | `		/* Append trailing line */` |
|     11 | 4093 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 4094 | `			zIn++;` |
|      1 | 4095 | `		}` |
|      5 | 4096 | `		if( zCur < zIn ){` |
|      - | 4097 | `			/* Output chunk verbatim */` |
|      5 | 4098 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 4099 | `		}` |
|      1 | 4100 | `	}` |
|      5 | 4101 | `	return PH7_OK;` |
|      3 | 4102 | `}` |
|      - | 4103 | `/*` |
|      - | 4104 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 4105 | ` *  According to the PHP reference manual.` |
|      - | 4106 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 4107 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 4108 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 4109 | ` * This applies to both sprintf() and printf().` |
|      - | 4110 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 4111 | ` * or more of these elements, in order:` |
|      - | 4112 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 4113 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 4114 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 4115 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 4116 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 4117 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 4118 | ` *   it with a single quote ('). See the examples below.` |
|      - | 4119 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 4120 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 4121 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 4122 | ` *   should result in.` |
|      - | 4123 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 4124 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 4125 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 4126 | ` *   limit to the string.` |
|      - | 4127 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 4128 | ` *       % - a literal percent character. No argument is required.` |
|      - | 4129 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 4130 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 4131 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 4132 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 4133 | ` * 	     for the number of digits after the decimal point.` |
|      - | 4134 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 4135 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 4136 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 4137 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 4138 | ` *       g - shorter of %e and %f.` |
|      - | 4139 | ` *       G - shorter of %E and %f.` |
|      - | 4140 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 4141 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 4142 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 4143 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 4144 | ` */` |
|      - | 4145 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4146 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 4147 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 4148 | `/*` |
|      - | 4149 | `** Conversion types fall into various categories as defined by the` |
|      - | 4150 | `** following enumeration.` |
|      - | 4151 | `*/` |
|      - | 4152 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 4153 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 4154 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 4155 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 4156 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 4157 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 4158 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 4159 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 4160 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 4161 |  |
|      - | 4162 | `/*` |
|      - | 4163 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 4164 | `*/` |
|      - | 4165 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 4166 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 4167 | `/*` |
|      - | 4168 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 4169 | `** by an instance of the following structure` |
|      - | 4170 | `*/` |
|      - | 4171 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 4172 | `struct ph7_fmt_info` |
|      - | 4173 | `{` |
|      - | 4174 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 4175 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 4176 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 4177 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 4178 | `  char *charset; /* The character set for conversion */` |
|      - | 4179 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 4180 | `};` |
|      - | 4181 | `/* PH7_PhpFloatShape (php's float-shape post-processing) lives in memobj.c —` |
|      - | 4182 | ` * the default float->string cast needs it even when this whole formatting` |
|      - | 4183 | ` * region is compiled out by PH7_DISABLE_DISK_IO. */` |
|      - | 4184 | `/*` |
|      - | 4185 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 4186 | ` * used conversion types first.` |
|      - | 4187 | ` */` |
|      - | 4188 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 4189 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 4190 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 4191 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 4192 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 4193 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 4194 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 4195 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 4196 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 4197 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4198 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4199 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 4200 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 4201 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 4202 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 4203 | `  /* php's 'h'/'H' are the locale-independent twins of 'g'/'G'; PHL always` |
|      - | 4204 | `   * formats in the C locale, so they behave identically. */` |
|      - | 4205 | `  {  'h',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 4206 | `  {  'H',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 4207 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 4208 | `};` |
|      - | 4209 | `/*` |
|      - | 4210 | ` * PHP 8 raises a catchable ValueError for an unknown conversion specifier` |
|      - | 4211 | ` * (e.g. "%y", or the C-ism "%#x" — '#' is not a php flag). Because printf()` |
|      - | 4212 | ` * and fprintf() stream their output incrementally while sprintf() buffers it,` |
|      - | 4213 | ` * every format builtin calls PH7_FormatValidate (below) to check the whole` |
|      - | 4214 | ` * format string BEFORE formatting so the throw happens with no partial output` |
|      - | 4215 | ` * escaping (php buffers the entire result and only emits it on success). This` |
|      - | 4216 | ` * scan mirrors the specifier-locating logic of the main format loop below.` |
|      - | 4217 | ` * On the first unknown specifier, stores it in *pBad and returns TRUE; returns` |
|      - | 4218 | ` * FALSE when every specifier is known. (A found-flag rather than a sentinel` |
|      - | 4219 | ` * char, so a NUL specifier byte — "%\0" — is still reported, not mistaken for` |
|      - | 4220 | ` * "all valid".)` |
|      - | 4221 | ` */` |
|    412 | 4222 | `static int FormatUnknownSpec(const char *zIn,int nByte,int *pBad)` |
|      1 | 4223 | `{` |
|    413 | 4224 | `	const char *zEnd = &zIn[nByte];` |
|      - | 4225 | `	int c,idx;` |
|   3449 | 4226 | `	while( zIn < zEnd ){` |
|   3057 | 4227 | `		if( zIn[0] != '%' ){` |
|   2265 | 4228 | `			zIn++;` |
|   2265 | 4229 | `			continue;` |
|      - | 4230 | `		}` |
|    793 | 4231 | `		zIn++; /* jump the percent sign */` |
|      - | 4232 | `		/* php-supported flags: '-', '+', ' ', '0' and the "'<pad>'" custom-pad` |
|      - | 4233 | `		 * form. '#' is intentionally NOT treated as a flag so it surfaces as an` |
|      - | 4234 | `		 * unknown specifier, matching php. */` |
|    977 | 4235 | `		while( zIn < zEnd ){` |
|    975 | 4236 | `			c = zIn[0];` |
|    975 | 4237 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|    185 | 4238 | `				zIn++;` |
|    185 | 4239 | `				continue;` |
|      - | 4240 | `			}` |
|    791 | 4241 | `			if( c=='\'' ){` |
|    ! 0 | 4242 | `				zIn++;` |
|    ! 0 | 4243 | `				if( zIn < zEnd ){` |
|    ! 0 | 4244 | `					zIn++; /* the custom pad character */` |
|    ! 0 | 4245 | `				}` |
|    ! 0 | 4246 | `				continue;` |
|      - | 4247 | `			}` |
|    791 | 4248 | `			break;` |
|    ! 0 | 4249 | `		}` |
|      - | 4250 | `		/* field width */` |
|   1009 | 4251 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    217 | 4252 | `			zIn++;` |
|      1 | 4253 | `		}` |
|      - | 4254 | `		/* positional specifier ($) — php parses flags AFTER it (e.g. "%1$-10s"),` |
|      - | 4255 | `		 * so skip the full flag set and width again, mirroring the main loop. */` |
|    793 | 4256 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
|    ! 0 | 4257 | `			zIn++;` |
|    ! 0 | 4258 | `			while( zIn < zEnd ){` |
|    ! 0 | 4259 | `				c = zIn[0];` |
|    ! 0 | 4260 | `				if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|    ! 0 | 4261 | `					zIn++;` |
|    ! 0 | 4262 | `					continue;` |
|      - | 4263 | `				}` |
|    ! 0 | 4264 | `				if( c=='\'' ){` |
|    ! 0 | 4265 | `					zIn++;` |
|    ! 0 | 4266 | `					if( zIn < zEnd ){` |
|    ! 0 | 4267 | `						zIn++;` |
|    ! 0 | 4268 | `					}` |
|    ! 0 | 4269 | `					continue;` |
|      - | 4270 | `				}` |
|    ! 0 | 4271 | `				break;` |
|    ! 0 | 4272 | `			}` |
|    ! 0 | 4273 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    ! 0 | 4274 | `				zIn++;` |
|    ! 0 | 4275 | `			}` |
|    ! 0 | 4276 | `		}` |
|      - | 4277 | `		/* precision */` |
|    793 | 4278 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|     87 | 4279 | `			zIn++;` |
|    183 | 4280 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|     97 | 4281 | `				zIn++;` |
|      1 | 4282 | `			}` |
|     43 | 4283 | `		}` |
|      - | 4284 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|    793 | 4285 | `		if( zIn < zEnd && zIn[0]=='l' ){` |
|     11 | 4286 | `			zIn++;` |
|      5 | 4287 | `		}` |
|    793 | 4288 | `		if( zIn >= zEnd ){` |
|      - | 4289 | `			/* A dangling '%' with no specifier: PHL's legacy path silently` |
|      - | 4290 | `			 * truncates here (recorded residual); nothing to validate. */` |
|      3 | 4291 | `			break;` |
|      - | 4292 | `		}` |
|    791 | 4293 | `		c = zIn[0];` |
|    791 | 4294 | `		zIn++; /* jump the conversion specifier */` |
|   3333 | 4295 | `		for( idx = 0 ; idx < (int)SX_ARRAYSIZE(aFmt) ; idx++ ){` |
|   3315 | 4296 | `			if( c == aFmt[idx].fmttype ){` |
|    773 | 4297 | `				break;` |
|      - | 4298 | `			}` |
|   1272 | 4299 | `		}` |
|    791 | 4300 | `		if( idx >= (int)SX_ARRAYSIZE(aFmt) ){` |
|     19 | 4301 | `			*pBad = c; /* unknown specifier */` |
|     19 | 4302 | `			return TRUE;` |
|      - | 4303 | `		}` |
|      1 | 4304 | `	}` |
|    395 | 4305 | `	return FALSE;` |
|    207 | 4306 | `}` |
|      - | 4307 | `/*` |
|      - | 4308 | ` * Validate a printf-style format string. PHP 8 raises a catchable ValueError for` |
|      - | 4309 | ` * an unknown conversion specifier, thrown before any output is produced. Every` |
|      - | 4310 | ` * format builtin (sprintf/printf/vprintf/vsprintf/fprintf/vfprintf) calls this` |
|      - | 4311 | ` * up-front, then propagates the returned status verbatim (PH7_EXCEPTION when the` |
|      - | 4312 | ` * throw is caught in place, PH7_ABORT when it goes uncaught).` |
|      - | 4313 | ` * Returns PH7_OK when the format is valid.` |
|      - | 4314 | ` */` |
|    412 | 4315 | `PH7_PRIVATE sxi32 PH7_FormatValidate(ph7_context *pCtx,const char *zFormat,int nByte)` |
|      1 | 4316 | `{` |
|    413 | 4317 | `	int badSpec = 0;` |
|    413 | 4318 | `	if( FormatUnknownSpec(zFormat,nByte,&badSpec) ){` |
|     28 | 4319 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      9 | 4320 | `			"Unknown format specifier \"%c\"",badSpec);` |
|      - | 4321 | `	}` |
|    395 | 4322 | `	return PH7_OK;` |
|    207 | 4323 | `}` |
|      - | 4324 | `/*` |
|      - | 4325 | `` * PHP 8: a printf-family `$format` argument is a `string` parameter — scalars`` |
|      - | 4326 | ` * (int/float/bool) and null coerce to a string, but an array/object/resource` |
|      - | 4327 | ` * raises a catchable TypeError. iArg is the 1-based argument position ($format` |
|      - | 4328 | ` * is #1 for sprintf/printf/vprintf/vsprintf, #2 for fprintf/vfprintf). Returns` |
|      - | 4329 | ` * PH7_OK when the value is string-coercible (the caller then uses` |
|      - | 4330 | ` * ph7_value_to_string, which renders scalars/null verbatim).` |
|      - | 4331 | ` */` |
|    432 | 4332 | `PH7_PRIVATE sxi32 PH7_FormatCheckFormatArg(ph7_context *pCtx,ph7_value *pArg,int iArg)` |
|      1 | 4333 | `{` |
|    433 | 4334 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg) \|\| ph7_value_is_resource(pArg) ){` |
|      - | 4335 | `		char zBuf[64];` |
|     13 | 4336 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4337 | `			"%s(): Argument #%d ($format) must be of type string, %s given",` |
|      4 | 4338 | `			ph7_function_name(pCtx),iArg,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|      - | 4339 | `	}` |
|    425 | 4340 | `	return PH7_OK;` |
|    217 | 4341 | `}` |
|      - | 4342 | `/*` |
|      - | 4343 | ` * Format a given string.` |
|      - | 4344 | ` * The root program.  All variations call this core.` |
|      - | 4345 | ` * INPUTS:` |
|      - | 4346 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 4347 | ` *            1. A pointer to the call context.` |
|      - | 4348 | ` *            2. A pointer to the list of characters to be output` |
|      - | 4349 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 4350 | ` *            3. An integer number of characters to be output.` |
|      - | 4351 | ` *               (Note: This number might be zero.)` |
|      - | 4352 | ` *            4. Upper layer private data.` |
|      - | 4353 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 4354 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 4355 | ` */` |
|    394 | 4356 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 4357 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 4358 | `	ph7_context *pCtx,  /* call context */` |
|      - | 4359 | `	const char *zIn,    /* Format string */` |
|      - | 4360 | `	int nByte,          /* Format string length */` |
|      - | 4361 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 4362 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 4363 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 4364 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 4365 | `	)` |
|      1 | 4366 | `{` |
|    395 | 4367 | `	char spaces[] = "                                                  ";` |
|      - | 4368 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    395 | 4369 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 4370 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 4371 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 4372 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 4373 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 4374 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 4375 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 4376 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 4377 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 4378 | `	ph7_int64 iVal;` |
|      - | 4379 | `	int precision;           /* Precision of the current field */` |
|      - | 4380 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 4381 | `	int c,rc,n;` |
|      - | 4382 | `	int length;              /* Length of the field */` |
|      - | 4383 | `	int prefix;` |
|      - | 4384 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 4385 | `	int width;               /* Width of the current field */` |
|      - | 4386 | `	int idx;` |
|    395 | 4387 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 4388 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 4389 | `	/* An unknown conversion specifier is rejected up-front by PH7_FormatValidate()` |
|      - | 4390 | `	 * (called by every format builtin before this routine), so the specifier set` |
|      - | 4391 | `	 * seen here is always valid. */` |
|      - | 4392 | `	/* Start the format process */` |
|    583 | 4393 | `	for(;;){` |
|   1167 | 4394 | `		zCur = zIn;` |
|   3417 | 4395 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|   2251 | 4396 | `			zIn++;` |
|      1 | 4397 | `		}` |
|   1167 | 4398 | `		if( zCur < zIn ){` |
|      - | 4399 | `			/* Consume chunk verbatim */` |
|    725 | 4400 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|    725 | 4401 | `			if( rc != SXRET_OK ){` |
|      - | 4402 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|    ! 0 | 4403 | `				break;` |
|      - | 4404 | `			}` |
|    362 | 4405 | `		}` |
|   1167 | 4406 | `		if( zIn >= zEnd ){` |
|      - | 4407 | `			/* No more input to process,break immediately */` |
|    393 | 4408 | `			break;` |
|      - | 4409 | `		}` |
|      - | 4410 | `		/* Find out what flags are present */` |
|    775 | 4411 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    774 | 4412 | `			flag_alternateform = flag_zeropad = 0;` |
|    775 | 4413 | `		zIn++; /* Jump the precent sign */` |
|    387 | 4414 | `		do{` |
|    959 | 4415 | `			c = zIn[0];` |
|    959 | 4416 | `			switch( c ){` |
|     15 | 4417 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      7 | 4418 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 4419 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|    159 | 4420 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 4421 | `			case '\'':` |
|    ! 0 | 4422 | `				zIn++;` |
|    ! 0 | 4423 | `				if( zIn < zEnd ){` |
|      - | 4424 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 4425 | `					c = zIn[0];` |
|    ! 0 | 4426 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4427 | `						spaces[idx] = (char)c;` |
|    ! 0 | 4428 | `					}` |
|    ! 0 | 4429 | `					c = 0;` |
|    ! 0 | 4430 | `				}` |
|    ! 0 | 4431 | `				break;` |
|    774 | 4432 | `			default:                                       break;` |
|      - | 4433 | `			}` |
|    959 | 4434 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 4435 | `		/* Get the field width */` |
|    775 | 4436 | `		width = 0;` |
|   1378 | 4437 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    217 | 4438 | `			width = width*10 + (zIn[0] - '0');` |
|    217 | 4439 | `			zIn++;` |
|      1 | 4440 | `		}` |
|    775 | 4441 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 4442 | `			/* Position specifer */` |
|    ! 0 | 4443 | `			if( width > 0 ){` |
|    ! 0 | 4444 | `				n = width;` |
|    ! 0 | 4445 | `				if( vf && n > 0 ){` |
|    ! 0 | 4446 | `					n--;` |
|    ! 0 | 4447 | `				}` |
|    ! 0 | 4448 | `			}` |
|    ! 0 | 4449 | `			zIn++;` |
|    ! 0 | 4450 | `			width = 0;` |
|      - | 4451 | `			/* php's grammar is %argnum$<flags><width>: the flags come AFTER the` |
|      - | 4452 | `			 * positional, so re-parse the full flag set here (e.g. "%1$-10s"),` |
|      - | 4453 | `			 * not just zero-padding. */` |
|    ! 0 | 4454 | `			do{` |
|    ! 0 | 4455 | `				c = zIn[0];` |
|    ! 0 | 4456 | `				switch( c ){` |
|    ! 0 | 4457 | `				case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|    ! 0 | 4458 | `				case '+':   flag_plussign = 1;        c = 0;   break;` |
|    ! 0 | 4459 | `				case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|    ! 0 | 4460 | `				case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 4461 | `				case '\'':` |
|    ! 0 | 4462 | `					zIn++;` |
|    ! 0 | 4463 | `					if( zIn < zEnd ){` |
|    ! 0 | 4464 | `						c = zIn[0];` |
|    ! 0 | 4465 | `						for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4466 | `							spaces[idx] = (char)c;` |
|    ! 0 | 4467 | `						}` |
|    ! 0 | 4468 | `						c = 0;` |
|    ! 0 | 4469 | `					}` |
|    ! 0 | 4470 | `					break;` |
|    ! 0 | 4471 | `				default:                                       break;` |
|      - | 4472 | `				}` |
|    ! 0 | 4473 | `			}while( c==0 && (zIn++ < zEnd) );` |
|    ! 0 | 4474 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 4475 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 4476 | `				zIn++;` |
|    ! 0 | 4477 | `			}` |
|    ! 0 | 4478 | `		}` |
|    775 | 4479 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 4480 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 4481 | `		}` |
|      - | 4482 | `		/* Get the precision */` |
|    775 | 4483 | `		precision = -1;` |
|    775 | 4484 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     87 | 4485 | `			precision = 0;` |
|     87 | 4486 | `			zIn++;` |
|    226 | 4487 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     97 | 4488 | `				precision = precision*10 + (zIn[0] - '0');` |
|     97 | 4489 | `				zIn++;` |
|      1 | 4490 | `			}` |
|     43 | 4491 | `		}` |
|      - | 4492 | `		/* Consume a single 'l' length modifier (a C-ism php accepts and ignores,` |
|      - | 4493 | `		 * e.g. "%ld"); PH7_FormatValidate mirrors this. Exactly one is skipped:` |
|      - | 4494 | `		 * in "%lld" the second 'l' becomes the (unknown) specifier, just like php. */` |
|    775 | 4495 | `		if( zIn < zEnd && zIn[0] == 'l' ){` |
|      9 | 4496 | `			zIn++;` |
|      4 | 4497 | `		}` |
|    775 | 4498 | `		if( zIn >= zEnd ){` |
|      - | 4499 | `			/* No more input */` |
|      3 | 4500 | `			break;` |
|      - | 4501 | `		}` |
|      - | 4502 | `		/* Fetch the info entry for the field */` |
|    773 | 4503 | `		pInfo = 0;` |
|    773 | 4504 | `		xtype = PH7_FMT_ERROR;` |
|    773 | 4505 | `		c = zIn[0];` |
|    773 | 4506 | `		zIn++; /* Jump the format specifer */` |
|   3009 | 4507 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|   3009 | 4508 | `			if( c==aFmt[idx].fmttype ){` |
|    773 | 4509 | `				pInfo = &aFmt[idx];` |
|    773 | 4510 | `				xtype = pInfo->type;` |
|    773 | 4511 | `				break;` |
|      - | 4512 | `			}` |
|   1119 | 4513 | `		}` |
|    773 | 4514 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    773 | 4515 | `		length = 0;` |
|      - | 4516 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 4517 | `		 /*` |
|      - | 4518 | `		  ** At this point, variables are initialized as follows:` |
|      - | 4519 | `		  **` |
|      - | 4520 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 4521 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 4522 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 4523 | `		  **                               field width was negative.` |
|      - | 4524 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 4525 | `		  **                               the conversion character.` |
|      - | 4526 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 4527 | `		  **   width                       The specified field width.  This is` |
|      - | 4528 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 4529 | `		  **   precision                   The specified precision.  The default` |
|      - | 4530 | `		  **                               is -1.` |
|      - | 4531 | `		  */` |
|    773 | 4532 | `		switch(xtype){` |
|      3 | 4533 | `		case PH7_FMT_PERCENT:` |
|      - | 4534 | `			/* A literal percent character */` |
|      7 | 4535 | `			zWorker[0] = '%';` |
|      7 | 4536 | `			length = (int)sizeof(char);` |
|      7 | 4537 | `			break;` |
|      3 | 4538 | `		case PH7_FMT_CHARX:` |
|      - | 4539 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 4540 | `			 * with that ASCII value` |
|      - | 4541 | `			 */` |
|      7 | 4542 | `			pArg = NEXT_ARG;` |
|      7 | 4543 | `			if( pArg == 0 ){` |
|      3 | 4544 | `				c = 0;` |
|      2 | 4545 | `			}else{` |
|      5 | 4546 | `				c = ph7_value_to_int(pArg);` |
|      - | 4547 | `			}` |
|      - | 4548 | `			/* NUL byte is an acceptable value */` |
|      7 | 4549 | `			zWorker[0] = (char)c;` |
|      7 | 4550 | `			length = (int)sizeof(char);` |
|      7 | 4551 | `			break;` |
|    162 | 4552 | `		case PH7_FMT_STRING:` |
|      - | 4553 | `			/* the argument is treated as and presented as a string */` |
|    325 | 4554 | `			pArg = NEXT_ARG;` |
|    325 | 4555 | `			if( pArg == 0 ){` |
|    ! 0 | 4556 | `				length = 0;` |
|    ! 0 | 4557 | `			}else{` |
|    325 | 4558 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 4559 | `			}` |
|    325 | 4560 | `			if( length < 1 ){` |
|    ! 0 | 4561 | `				zBuf = " ";` |
|    ! 0 | 4562 | `				length = (int)sizeof(char);` |
|    ! 0 | 4563 | `			}` |
|    325 | 4564 | `			if( precision>=0 && precision<length ){` |
|      3 | 4565 | `				length = precision;` |
|      1 | 4566 | `			}` |
|    325 | 4567 | `			if( flag_zeropad ){` |
|      - | 4568 | `				/* zero-padding works on strings too */` |
|    ! 0 | 4569 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4570 | `					spaces[idx] = '0';` |
|    ! 0 | 4571 | `				}` |
|    ! 0 | 4572 | `			}` |
|    325 | 4573 | `			break;` |
|    130 | 4574 | `		case PH7_FMT_RADIX:` |
|    261 | 4575 | `			pArg = NEXT_ARG;` |
|    261 | 4576 | `			if( pArg == 0 ){` |
|    ! 0 | 4577 | `				iVal = 0;` |
|    ! 0 | 4578 | `			}else{` |
|    261 | 4579 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 4580 | `			}` |
|      - | 4581 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|    261 | 4582 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 4583 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 4584 | `			}` |
|      - | 4585 | `#if 1` |
|      - | 4586 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 4587 | `        ** I think this is stupid.*/` |
|    261 | 4588 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 4589 | `#else` |
|      - | 4590 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 4591 | `        ** but leave the prefix for hex.*/` |
|      - | 4592 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 4593 | `#endif` |
|    261 | 4594 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|    237 | 4595 | `          if( iVal<0 ){` |
|     25 | 4596 | `            iVal = -iVal;` |
|      - | 4597 | `			/* Ticket 1433-003 */` |
|     25 | 4598 | `			if( iVal < 0 ){` |
|      - | 4599 | `				/* Overflow */` |
|    ! 0 | 4600 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4601 | `			}` |
|     25 | 4602 | `            prefix = '-';` |
|    225 | 4603 | `          }else if( flag_plussign )  prefix = '+';` |
|    211 | 4604 | `          else if( flag_blanksign )  prefix = ' ';` |
|    209 | 4605 | `          else                       prefix = 0;` |
|    119 | 4606 | `        }else{` |
|     25 | 4607 | `			if( iVal<0 ){` |
|    ! 0 | 4608 | `				iVal = -iVal;` |
|      - | 4609 | `				/* Ticket 1433-003 */` |
|    ! 0 | 4610 | `				if( iVal < 0 ){` |
|      - | 4611 | `					/* Overflow */` |
|    ! 0 | 4612 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4613 | `				}` |
|    ! 0 | 4614 | `			}` |
|     25 | 4615 | `			prefix = 0;` |
|      - | 4616 | `		}` |
|    261 | 4617 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|    149 | 4618 | `          precision = width-(prefix!=0);` |
|     74 | 4619 | `        }` |
|    261 | 4620 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 4621 | `        {` |
|      - | 4622 | `          register char *cset;      /* Use registers for speed */` |
|      - | 4623 | `          register int base;` |
|    261 | 4624 | `          cset = pInfo->charset;` |
|    261 | 4625 | `          base = pInfo->base;` |
|    130 | 4626 | `          do{                                           /* Convert to ascii */` |
|    333 | 4627 | `            *(--zBuf) = cset[iVal%base];` |
|    333 | 4628 | `            iVal = iVal/base;` |
|    333 | 4629 | `          }while( iVal>0 );` |
|      - | 4630 | `        }` |
|    261 | 4631 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    427 | 4632 | `        for(idx=precision-length; idx>0; idx--){` |
|    167 | 4633 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|     84 | 4634 | `        }` |
|    261 | 4635 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|    261 | 4636 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 4637 | `          char *pre, x;` |
|    ! 0 | 4638 | `          pre = pInfo->prefix;` |
|    ! 0 | 4639 | `          if( *zBuf!=pre[0] ){` |
|    ! 0 | 4640 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|    ! 0 | 4641 | `          }` |
|    ! 0 | 4642 | `        }` |
|    261 | 4643 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    261 | 4644 | `		break;` |
|     88 | 4645 | `		case PH7_FMT_FLOAT:` |
|      - | 4646 | `		case PH7_FMT_EXP:` |
|      - | 4647 | `		case PH7_FMT_GENERIC:{` |
|      - | 4648 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 4649 | `		double realvalue;` |
|      - | 4650 | `		char zFmt[8];` |
|      - | 4651 | `		int nOut, nFmt;` |
|    177 | 4652 | `		pArg = NEXT_ARG;` |
|    177 | 4653 | `		if( pArg == 0 ){` |
|    ! 0 | 4654 | `			realvalue = 0;` |
|    ! 0 | 4655 | `		}else{` |
|    177 | 4656 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 4657 | `		}` |
|      - | 4658 | `		/* php prints the IEEE specials bare — NaN / INF / -INF with no width` |
|      - | 4659 | `		 * padding, precision, or sign flags (php_sprintf_appenddouble). */` |
|    177 | 4660 | `		if( PH7_IS_NAN(realvalue) ){` |
|     21 | 4661 | `			zBuf = "NaN";` |
|     21 | 4662 | `			length = 3;` |
|     21 | 4663 | `			width = 0;` |
|     21 | 4664 | `			break;` |
|      - | 4665 | `		}` |
|    157 | 4666 | `		if( PH7_IS_INF(realvalue) ){` |
|     37 | 4667 | `			if( realvalue < 0.0 ){` |
|     15 | 4668 | `				zBuf = "-INF";` |
|     15 | 4669 | `				length = 4;` |
|      8 | 4670 | `			}else{` |
|     23 | 4671 | `				zBuf = "INF";` |
|     23 | 4672 | `				length = 3;` |
|      - | 4673 | `			}` |
|     37 | 4674 | `			width = 0;` |
|     37 | 4675 | `			break;` |
|      - | 4676 | `		}` |
|    121 | 4677 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|    121 | 4678 | `		if( precision > 53 ){` |
|      - | 4679 | `			/* php's FORMAT_CONV_MAX_PRECISION cap, with the same E_NOTICE` |
|      - | 4680 | `			 * (message prefixed with the active function's name, like` |
|      - | 4681 | `			 * php_error_docref). */` |
|      - | 4682 | `			char zMsg[160];` |
|      4 | 4683 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 4684 | `				"%z(): Requested precision of %d digits was truncated to PHP maximum of %d digits",` |
|      2 | 4685 | `				&pCtx->pFunc->sName,precision,53);` |
|      3 | 4686 | `			PH7_VmThrowError(pCtx->pVm,0,E_NOTICE,zMsg);` |
|      3 | 4687 | `			precision = 53;` |
|      1 | 4688 | `		}` |
|      - | 4689 | ``		/* php's %f/%e extract the sign via `num < 0`, so negative zero prints`` |
|      - | 4690 | `		 * unsigned there — while %g (php_gcvt on the raw value) keeps "-0". */` |
|    121 | 4691 | `		if( xtype!=PH7_FMT_GENERIC && realvalue == 0.0 ){` |
|      9 | 4692 | `			realvalue = 0.0;` |
|      4 | 4693 | `		}` |
|      - | 4694 | `		/* php's float conversions are correctly rounded (zend_dtoa); use libc` |
|      - | 4695 | `		 * snprintf as the digit engine (the byte-exact-floats rule — the old` |
|      - | 4696 | `		 * hand-rolled vxGetdigit loop stopped at 16 significant digits, so` |
|      - | 4697 | `		 * e.g. %f of 1e308 printed zeros where php prints the exact binary64` |
|      - | 4698 | `		 * expansion), then post-process into php's exact shapes below. */` |
|    121 | 4699 | `		nFmt = 0;` |
|    121 | 4700 | `		zFmt[nFmt++] = '%';` |
|    121 | 4701 | `		if( flag_alternateform ) zFmt[nFmt++] = '#';` |
|      - | 4702 | `		/* php's ' ' flag selects space PADDING (its default), not C's` |
|      - | 4703 | `		 * space-for-positive-sign — so flag_blanksign is NOT forwarded. */` |
|    121 | 4704 | `		if( flag_plussign ) zFmt[nFmt++] = '+';` |
|    121 | 4705 | `		zFmt[nFmt++] = '.';` |
|    121 | 4706 | `		zFmt[nFmt++] = '*';` |
|    165 | 4707 | `		zFmt[nFmt++] = (char)(xtype==PH7_FMT_FLOAT ? 'f' :` |
|     30 | 4708 | `			(xtype==PH7_FMT_EXP ? ((pInfo->charset[0]=='E') ? 'E' : 'e')` |
|     28 | 4709 | `			                    : ((pInfo->charset[0]=='E') ? 'G' : 'g')));` |
|    121 | 4710 | `		zFmt[nFmt] = 0;` |
|    121 | 4711 | `		nOut = snprintf(zWorker,sizeof(zWorker),zFmt,precision,realvalue);` |
|    121 | 4712 | `		if( nOut < 0 \|\| nOut >= (int)sizeof(zWorker) ){` |
|      - | 4713 | `			/* Cannot happen with precision capped at 53 (%f of DBL_MAX is` |
|      - | 4714 | `			 * ~365 bytes); keep the truncated output rather than overrun. */` |
|    ! 0 | 4715 | `			nOut = (int)SyStrlen(zWorker);` |
|    ! 0 | 4716 | `		}` |
|    121 | 4717 | `		nOut = (int)PH7_PhpFloatShape(zWorker,(sxi32)nOut,xtype==PH7_FMT_GENERIC);` |
|    121 | 4718 | `		zBuf = zWorker;` |
|    121 | 4719 | `		length = nOut;` |
|      - | 4720 | `		/* Let the zero-pad block below insert zeros between the sign (written` |
|      - | 4721 | `		 * by snprintf) and the first digit, as before. */` |
|    121 | 4722 | `		prefix = (zWorker[0]=='-' \|\| zWorker[0]=='+' \|\| zWorker[0]==' ') ? zWorker[0] : 0;` |
|      - | 4723 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 4724 | `        ** set and we are not left justified */` |
|    121 | 4725 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 4726 | `          int i;` |
|      7 | 4727 | `          int nPad = width - length;` |
|     51 | 4728 | `          for(i=width; i>=nPad; i--){` |
|     45 | 4729 | `            zBuf[i] = zBuf[i-nPad];` |
|     23 | 4730 | `          }` |
|      7 | 4731 | `          i = prefix!=0;` |
|     29 | 4732 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      7 | 4733 | `          length = width;` |
|      3 | 4734 | `        }` |
|      - | 4735 | `#else` |
|      - | 4736 | `         zBuf = " ";` |
|      - | 4737 | `		 length = (int)sizeof(char);` |
|      - | 4738 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|    121 | 4739 | `		 break;` |
|      - | 4740 | `							 }` |
|    ! 0 | 4741 | `		default:` |
|      - | 4742 | `			/* Unreachable: PH7_FormatValidate() rejects unknown specifiers with a` |
|      - | 4743 | `			 * catchable ValueError before formatting begins. Kept as a defensive` |
|      - | 4744 | `			 * no-op that emits nothing. */` |
|    ! 0 | 4745 | `			length = 0;` |
|    ! 0 | 4746 | `			break;` |
|      - | 4747 | `		}` |
|      - | 4748 | `		 /*` |
|      - | 4749 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 4750 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 4751 | `		 ** the output.` |
|      - | 4752 | `		 */` |
|    773 | 4753 | `    if( !flag_leftjustify ){` |
|      - | 4754 | `      register int nspace;` |
|    759 | 4755 | `      nspace = width-length;` |
|    759 | 4756 | `      if( nspace>0 ){` |
|      7 | 4757 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4758 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4759 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4760 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4761 | `			}` |
|    ! 0 | 4762 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4763 | `        }` |
|      7 | 4764 | `        if( nspace>0 ){` |
|      7 | 4765 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      7 | 4766 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4767 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4768 | `			}` |
|      3 | 4769 | `		}` |
|      3 | 4770 | `      }` |
|    379 | 4771 | `    }` |
|    773 | 4772 | `    if( length>0 ){` |
|    773 | 4773 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    773 | 4774 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4775 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4776 | `		}` |
|    386 | 4777 | `    }` |
|    773 | 4778 | `    if( flag_leftjustify ){` |
|      - | 4779 | `      register int nspace;` |
|     15 | 4780 | `      nspace = width-length;` |
|     15 | 4781 | `      if( nspace>0 ){` |
|     11 | 4782 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4783 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4784 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4785 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4786 | `			}` |
|    ! 0 | 4787 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4788 | `        }` |
|     11 | 4789 | `        if( nspace>0 ){` |
|     11 | 4790 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|     11 | 4791 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4792 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4793 | `			}` |
|      5 | 4794 | `		}` |
|      5 | 4795 | `      }` |
|      7 | 4796 | `    }` |
|      1 | 4797 | ` }/* for(;;) */` |
|    395 | 4798 | `	return SXRET_OK;` |
|    198 | 4799 | `}` |
|      - | 4800 | `/*` |
|      - | 4801 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 4802 | ` */` |
|    352 | 4803 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4804 | `{` |
|      - | 4805 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|      - | 4806 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|      - | 4807 | `	 * non-OK rc also stops the format loop. */` |
|    353 | 4808 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|    353 | 4809 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|    353 | 4810 | `	return *pRc;` |
|      1 | 4811 | `}` |
|      - | 4812 | `/*` |
|      - | 4813 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 4814 | ` *  Return a formatted string.` |
|      - | 4815 | ` * Parameters` |
|      - | 4816 | ` *  $format` |
|      - | 4817 | ` *    The format string (see block comment above)` |
|      - | 4818 | ` * Return` |
|      - | 4819 | ` *  A string produced according to the formatting string format.` |
|      - | 4820 | ` */` |
|    188 | 4821 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4822 | `{` |
|      - | 4823 | `	const char *zFormat;` |
|    189 | 4824 | `	sxi32 rc = SXRET_OK;` |
|      - | 4825 | `	int nLen;` |
|    189 | 4826 | `	if( nArg < 1 ){` |
|      - | 4827 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4828 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4829 | `		return PH7_OK;` |
|      - | 4830 | `	}` |
|      - | 4831 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|    189 | 4832 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    189 | 4833 | `	if( rc != PH7_OK ){` |
|      5 | 4834 | `		return rc;` |
|      - | 4835 | `	}` |
|      - | 4836 | `	/* Extract the string format (scalars/null coerce). */` |
|    185 | 4837 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    185 | 4838 | `	if( nLen < 1 ){` |
|      - | 4839 | `		/* Empty string */` |
|    ! 0 | 4840 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4841 | `		return PH7_OK;` |
|      - | 4842 | `	}` |
|      - | 4843 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4844 | `	 * output; propagate the throw status verbatim. */` |
|    185 | 4845 | `	rc = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    185 | 4846 | `	if( rc != PH7_OK ){` |
|     17 | 4847 | `		return rc;` |
|      - | 4848 | `	}` |
|      - | 4849 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|    169 | 4850 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|    169 | 4851 | `	if( rc != SXRET_OK ){` |
|      - | 4852 | `		/* The result append ran out of memory: raise a fatal rather than` |
|      - | 4853 | `		 * returning a silently-truncated string. */` |
|    ! 0 | 4854 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4855 | `	}` |
|    169 | 4856 | `	return PH7_OK;` |
|     95 | 4857 | `}` |
|      - | 4858 | `/*` |
|      - | 4859 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 4860 | ` */` |
|   1130 | 4861 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4862 | `{` |
|   1131 | 4863 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 4864 | `	/* Call the VM output consumer directly */` |
|   1131 | 4865 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 4866 | `	/* Increment counter */` |
|   1131 | 4867 | `	*pCounter += nLen;` |
|   1131 | 4868 | `	return PH7_OK;` |
|      1 | 4869 | `}` |
|      - | 4870 | `/*` |
|      - | 4871 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 4872 | ` *  Output a formatted string.` |
|      - | 4873 | ` * Parameters` |
|      - | 4874 | ` *  $format` |
|      - | 4875 | ` *   See sprintf() for a description of format.` |
|      - | 4876 | ` * Return` |
|      - | 4877 | ` *  The length of the outputted string.` |
|      - | 4878 | ` */` |
|    200 | 4879 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4880 | `{` |
|    201 | 4881 | `	ph7_int64 nCounter = 0;` |
|      - | 4882 | `	const char *zFormat;` |
|      - | 4883 | `	int nLen;` |
|    201 | 4884 | `	if( nArg < 1 ){` |
|      - | 4885 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 4886 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4887 | `		return PH7_OK;` |
|      - | 4888 | `	}` |
|      - | 4889 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|      - | 4890 | `	{` |
|    201 | 4891 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    201 | 4892 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 4893 | `			return rcf;` |
|      - | 4894 | `		}` |
|      - | 4895 | `	}` |
|      - | 4896 | `	/* Extract the string format (scalars/null coerce). */` |
|    201 | 4897 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    201 | 4898 | `	if( nLen < 1 ){` |
|      - | 4899 | `		/* Empty string */` |
|    ! 0 | 4900 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4901 | `		return PH7_OK;` |
|      - | 4902 | `	}` |
|      - | 4903 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4904 | `	 * output; propagate the throw status verbatim. */` |
|      - | 4905 | `	{` |
|    201 | 4906 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    201 | 4907 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 4908 | `			return rcv;` |
|      - | 4909 | `		}` |
|      - | 4910 | `	}` |
|      - | 4911 | `	/* Format the string */` |
|    201 | 4912 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 4913 | `	/* Return the length of the outputted string */` |
|    201 | 4914 | `	ph7_result_int64(pCtx,nCounter);` |
|    201 | 4915 | `	return PH7_OK;` |
|    101 | 4916 | `}` |
|      - | 4917 | `/*` |
|      - | 4918 | ` * int vprintf(string $format,array $args)` |
|      - | 4919 | ` *  Output a formatted string.` |
|      - | 4920 | ` * Parameters` |
|      - | 4921 | ` *  $format` |
|      - | 4922 | ` *   See sprintf() for a description of format.` |
|      - | 4923 | ` * Return` |
|      - | 4924 | ` *  The length of the outputted string.` |
|      - | 4925 | ` */` |
|      4 | 4926 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4927 | `{` |
|      5 | 4928 | `	ph7_int64 nCounter = 0;` |
|      - | 4929 | `	const char *zFormat;` |
|      - | 4930 | `	ph7_hashmap *pMap;` |
|      - | 4931 | `	SySet sArg;` |
|      - | 4932 | `	int nLen,n;` |
|      - | 4933 | `	sxi32 rcFmt;` |
|      5 | 4934 | `	if( nArg < 2 ){` |
|      - | 4935 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 4936 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4937 | `		return PH7_OK;` |
|      - | 4938 | `	}` |
|      - | 4939 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|      5 | 4940 | `	rcFmt = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|      5 | 4941 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4942 | `		return rcFmt;` |
|      - | 4943 | `	}` |
|      5 | 4944 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 4945 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 4946 | `		char zBuf[64];` |
|      4 | 4947 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4948 | `			"vprintf(): Argument #2 ($values) must be of type array, %s given",` |
|      2 | 4949 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 4950 | `	}` |
|      - | 4951 | `	/* Extract the string format (scalars/null coerce). */` |
|      3 | 4952 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4953 | `	if( nLen < 1 ){` |
|      - | 4954 | `		/* Empty string */` |
|    ! 0 | 4955 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4956 | `		return PH7_OK;` |
|      - | 4957 | `	}` |
|      - | 4958 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4959 | `	 * output; propagate the throw status verbatim. */` |
|      3 | 4960 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      3 | 4961 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4962 | `		return rcFmt;` |
|      - | 4963 | `	}` |
|      - | 4964 | `	/* Point to the hashmap */` |
|      3 | 4965 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4966 | `	/* Extract arguments from the hashmap */` |
|      3 | 4967 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4968 | `	/* Format the string */` |
|      3 | 4969 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 4970 | `	/* Release the container */` |
|      3 | 4971 | `	SySetRelease(&sArg);` |
|      - | 4972 | `	/* Return the length of the outputted string */` |
|      3 | 4973 | `	ph7_result_int64(pCtx,nCounter);` |
|      3 | 4974 | `	return PH7_OK;` |
|      3 | 4975 | `}` |
|      - | 4976 | `/*` |
|      - | 4977 | ` * int vsprintf(string $format,array $args)` |
|      - | 4978 | ` *  Output a formatted string.` |
|      - | 4979 | ` * Parameters` |
|      - | 4980 | ` *  $format` |
|      - | 4981 | ` *   See sprintf() for a description of format.` |
|      - | 4982 | ` * Return` |
|      - | 4983 | ` *  A string produced according to the formatting string format.` |
|      - | 4984 | ` */` |
|     22 | 4985 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4986 | `{` |
|      - | 4987 | `	const char *zFormat;` |
|      - | 4988 | `	ph7_hashmap *pMap;` |
|      - | 4989 | `	SySet sArg;` |
|     23 | 4990 | `	sxi32 rc = SXRET_OK;` |
|      - | 4991 | `	sxi32 rcFmt;` |
|      - | 4992 | `	int nLen,n;` |
|     23 | 4993 | `	if( nArg < 2 ){` |
|      - | 4994 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4995 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4996 | `		return PH7_OK;` |
|      - | 4997 | `	}` |
|      - | 4998 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|     23 | 4999 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|     23 | 5000 | `	if( rc != PH7_OK ){` |
|      5 | 5001 | `		return rc;` |
|      - | 5002 | `	}` |
|     19 | 5003 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 5004 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 5005 | `		char zBuf[64];` |
|     16 | 5006 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5007 | `			"vsprintf(): Argument #2 ($values) must be of type array, %s given",` |
|     10 | 5008 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 5009 | `	}` |
|      - | 5010 | `	/* Extract the string format (scalars/null coerce). */` |
|      9 | 5011 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 5012 | `	if( nLen < 1 ){` |
|      - | 5013 | `		/* Empty string */` |
|    ! 0 | 5014 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5015 | `		return PH7_OK;` |
|      - | 5016 | `	}` |
|      - | 5017 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 5018 | `	 * output; propagate the throw status verbatim. */` |
|      9 | 5019 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      9 | 5020 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 5021 | `		return rcFmt;` |
|      - | 5022 | `	}` |
|      - | 5023 | `	/* Point to hashmap */` |
|      9 | 5024 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 5025 | `	/* Extract arguments from the hashmap */` |
|      9 | 5026 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 5027 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|      9 | 5028 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|      - | 5029 | `	/* Release the container */` |
|      9 | 5030 | `	SySetRelease(&sArg);` |
|      9 | 5031 | `	if( rc != SXRET_OK ){` |
|      - | 5032 | `		/* The result append ran out of memory: raise a fatal. */` |
|    ! 0 | 5033 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5034 | `	}` |
|      9 | 5035 | `	return PH7_OK;` |
|     12 | 5036 | `}` |
|      - | 5037 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5038 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5039 | `/*` |
|      - | 5040 | ` * Symisc eXtension.` |
|      - | 5041 | ` * string size_format(int64 $size)` |
|      - | 5042 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 5043 | ` *  Example:` |
|      - | 5044 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 5045 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 5046 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 5047 | ` * Parameter` |
|      - | 5048 | ` *  $size` |
|      - | 5049 | ` *    Entity size in bytes.` |
|      - | 5050 | ` * Return` |
|      - | 5051 | ` *   Formatted string representation of the given size.` |
|      - | 5052 | ` */` |
|     24 | 5053 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5054 | `{` |
|      - | 5055 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 5056 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 5057 | `	sxi32 nRest,i_32;` |
|      - | 5058 | `	ph7_int64 iSize;` |
|     25 | 5059 | `	int c = -1; /* index in zUnit[] */` |
|      - | 5060 |  |
|     25 | 5061 | `	if( nArg < 1 ){` |
|      - | 5062 | `		/* Missing argument,return the empty string */` |
|      3 | 5063 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5064 | `		return PH7_OK;` |
|      - | 5065 | `	}` |
|      - | 5066 | `	/* Extract the given size */` |
|     23 | 5067 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 5068 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 5069 | `		/* Don't bother formatting,return immediately */` |
|      5 | 5070 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 5071 | `		return PH7_OK;` |
|      - | 5072 | `	}` |
|     19 | 5073 | `	for(;;){` |
|     39 | 5074 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 5075 | `		iSize >>= 10;` |
|     39 | 5076 | `		c++;` |
|     39 | 5077 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 5078 | `			break;` |
|      - | 5079 | `		}` |
|      1 | 5080 | `	}` |
|     19 | 5081 | `	nRest /= 100;` |
|     19 | 5082 | `	if( nRest > 9 ){` |
|    ! 0 | 5083 | `		nRest = 9;` |
|    ! 0 | 5084 | `	}` |
|     19 | 5085 | `	if( iSize > 999 ){` |
|    ! 0 | 5086 | `		c++;` |
|    ! 0 | 5087 | `		nRest = 9;` |
|    ! 0 | 5088 | `		iSize = 0;` |
|    ! 0 | 5089 | `	}` |
|     19 | 5090 | `	i_32 = (sxi32)iSize;` |
|      - | 5091 | `	/* Format */` |
|     19 | 5092 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 5093 | `	return PH7_OK;` |
|     13 | 5094 | `}` |
|      - | 5095 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 5096 | `/*` |
|      - | 5097 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 5098 | ` *   Calculate the md5 hash of a string.` |
|      - | 5099 | ` * Parameter` |
|      - | 5100 | ` *  $str` |
|      - | 5101 | ` *   Input string` |
|      - | 5102 | ` * $raw_output` |
|      - | 5103 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 5104 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 5105 | ` * Return` |
|      - | 5106 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 5107 | ` */` |
|     12 | 5108 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5109 | `{` |
|      - | 5110 | `	unsigned char zDigest[16];` |
|     13 | 5111 | `	int raw_output = FALSE;` |
|      - | 5112 | `	const void *pIn;` |
|      - | 5113 | `	int nLen;` |
|     13 | 5114 | `	if( nArg < 1 ){` |
|      - | 5115 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5116 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5117 | `		return PH7_OK;` |
|      - | 5118 | `	}` |
|      - | 5119 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 5120 | `	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */` |
|     13 | 5121 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 5122 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 5123 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 5124 | `	}` |
|      - | 5125 | `	/* Compute the MD5 digest */` |
|     13 | 5126 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|     13 | 5127 | `	if( raw_output ){` |
|      - | 5128 | `		/* Output raw digest */` |
|      5 | 5129 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 5130 | `	}else{` |
|      - | 5131 | `		/* Perform a binary to hex conversion */` |
|      9 | 5132 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 5133 | `	}` |
|     13 | 5134 | `	return PH7_OK;` |
|      7 | 5135 | `}` |
|      - | 5136 | `/*` |
|      - | 5137 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 5138 | ` *   Calculate the sha1 hash of a string.` |
|      - | 5139 | ` * Parameter` |
|      - | 5140 | ` *  $str` |
|      - | 5141 | ` *   Input string` |
|      - | 5142 | ` * $raw_output` |
|      - | 5143 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 5144 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 5145 | ` * Return` |
|      - | 5146 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 5147 | ` */` |
|     10 | 5148 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5149 | `{` |
|      - | 5150 | `	unsigned char zDigest[20];` |
|     11 | 5151 | `	int raw_output = FALSE;` |
|      - | 5152 | `	const void *pIn;` |
|      - | 5153 | `	int nLen;` |
|     11 | 5154 | `	if( nArg < 1 ){` |
|      - | 5155 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5156 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5157 | `		return PH7_OK;` |
|      - | 5158 | `	}` |
|      - | 5159 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 5160 | `	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */` |
|     11 | 5161 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5162 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 5163 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 5164 | `	}` |
|      - | 5165 | `	/* Compute the SHA1 digest */` |
|     11 | 5166 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|     11 | 5167 | `	if( raw_output ){` |
|      - | 5168 | `		/* Output raw digest */` |
|      5 | 5169 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 5170 | `	}else{` |
|      - | 5171 | `		/* Perform a binary to hex conversion */` |
|      7 | 5172 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 5173 | `	}` |
|     11 | 5174 | `	return PH7_OK;` |
|      6 | 5175 | `}` |
|      - | 5176 | `/*` |
|      - | 5177 | ` * int64 crc32(string $str)` |
|      - | 5178 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 5179 | ` * Parameter` |
|      - | 5180 | ` *  $str` |
|      - | 5181 | ` *   Input string` |
|      - | 5182 | ` * Return` |
|      - | 5183 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 5184 | ` */` |
|      2 | 5185 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5186 | `{` |
|      - | 5187 | `	const void *pIn;` |
|      - | 5188 | `	sxu32 nCRC;` |
|      - | 5189 | `	int nLen;` |
|      3 | 5190 | `	if( nArg < 1 ){` |
|      - | 5191 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 5192 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5193 | `		return PH7_OK;` |
|      - | 5194 | `	}` |
|      - | 5195 | `	/* Extract the input string */` |
|      3 | 5196 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5197 | `	if( nLen < 1 ){` |
|      - | 5198 | `		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike` |
|      - | 5199 | `		 * md5()/sha1(), whose empty-string digests are non-zero. */` |
|    ! 0 | 5200 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5201 | `		return PH7_OK;` |
|      - | 5202 | `	}` |
|      - | 5203 | `	/* Calculate the sum */` |
|      3 | 5204 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 5205 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 5206 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 5207 | `	return PH7_OK;` |
|      2 | 5208 | `}` |
|      - | 5209 | `/*` |
|      - | 5210 | ` * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is` |
|      - | 5211 | ` * described by a small record so one dispatch (and one generic HMAC) serves them` |
|      - | 5212 | ` * all. Thin adapters normalize the differing context types and the reversed` |
|      - | 5213 | ` * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.` |
|      - | 5214 | ` */` |
|     11 | 5215 | `static void HashMd5Init(HashCtx *c){ MD5Init(&c->md5); }` |
|     15 | 5216 | `static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }` |
|     11 | 5217 | `static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }` |
|     11 | 5218 | `static void HashSha1Init(HashCtx *c){ SHA1Init(&c->sha1); }` |
|     15 | 5219 | `static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }` |
|     11 | 5220 | `static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }` |
|      9 | 5221 | `static void HashSha224Init(HashCtx *c){ SHA224Init(&c->sha256); }` |
|     33 | 5222 | `static void HashSha256Init(HashCtx *c){ SHA256Init(&c->sha256); }` |
|     57 | 5223 | `static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }` |
|     41 | 5224 | `static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }` |
|      9 | 5225 | `static void HashSha384Init(HashCtx *c){ SHA384Init(&c->sha512); }` |
|     15 | 5226 | `static void HashSha512Init(HashCtx *c){ SHA512Init(&c->sha512); }` |
|     27 | 5227 | `static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }` |
|     23 | 5228 | `static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }` |
|      - | 5229 | `typedef struct HashAlgo HashAlgo;` |
|      - | 5230 | `struct HashAlgo {` |
|      - | 5231 | `	const char *zName;   /* lowercase canonical name */` |
|      - | 5232 | `	int nDigestLen;      /* output bytes: 16/20/28/32/48/64 */` |
|      - | 5233 | `	int nBlockLen;       /* internal block bytes (for HMAC): 64 or 128 */` |
|      - | 5234 | `	void (*xInit)(HashCtx *);` |
|      - | 5235 | `	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);` |
|      - | 5236 | `	void (*xFinal)(HashCtx *,unsigned char *);` |
|      - | 5237 | `};` |
|      - | 5238 | `static const HashAlgo aHashAlgo[] = {` |
|      - | 5239 | `	{ "md5",    16, 64,  HashMd5Init,    HashMd5Update,    HashMd5Final    },` |
|      - | 5240 | `	{ "sha1",   20, 64,  HashSha1Init,   HashSha1Update,   HashSha1Final   },` |
|      - | 5241 | `	{ "sha224", 28, 64,  HashSha224Init, HashSha256Update, HashSha256Final },` |
|      - | 5242 | `	{ "sha256", 32, 64,  HashSha256Init, HashSha256Update, HashSha256Final },` |
|      - | 5243 | `	{ "sha384", 48, 128, HashSha384Init, HashSha512Update, HashSha512Final },` |
|      - | 5244 | `	{ "sha512", 64, 128, HashSha512Init, HashSha512Update, HashSha512Final },` |
|      - | 5245 | `};` |
|      - | 5246 | `/* Case-insensitive algorithm lookup (PHP accepts 'SHA256' etc.). */` |
|     73 | 5247 | `static const HashAlgo * HashFindAlgo(const char *zName,int nLen){` |
|      - | 5248 | `	sxu32 i;` |
|    279 | 5249 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|    272 | 5250 | `		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen` |
|    211 | 5251 | `			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){` |
|     67 | 5252 | `			return &aHashAlgo[i];` |
|      - | 5253 | `		}` |
|    106 | 5254 | `	}` |
|      6 | 5255 | `	return 0;` |
|     38 | 5256 | `}` |
|      - | 5257 | `/*` |
|      - | 5258 | ` * string hash(string $algo,string $data[,bool $binary = false])` |
|      - | 5259 | ` *   Generate a hash value (message digest).` |
|      - | 5260 | ` */` |
|     54 | 5261 | `static int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5262 | `{` |
|      - | 5263 | `	const HashAlgo *pAlgo;` |
|      - | 5264 | `	const char *zAlgo,*zData;` |
|     56 | 5265 | `	int nAlgoLen,nDataLen,raw_output = FALSE;` |
|      - | 5266 | `	HashCtx sCtx;` |
|      - | 5267 | `	unsigned char zDigest[64];` |
|     56 | 5268 | `	if( nArg < 2 ){` |
|    ! 0 | 5269 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5270 | `			"hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5271 | `	}` |
|     56 | 5272 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     56 | 5273 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     56 | 5274 | `	if( pAlgo == 0 ){` |
|      3 | 5275 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5276 | `			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - | 5277 | `	}` |
|     53 | 5278 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     53 | 5279 | `	if( nArg > 2 ){` |
|      9 | 5280 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|      4 | 5281 | `	}` |
|     53 | 5282 | `	pAlgo->xInit(&sCtx);` |
|     53 | 5283 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     53 | 5284 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     53 | 5285 | `	if( raw_output ){` |
|      9 | 5286 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|      5 | 5287 | `	}else{` |
|     45 | 5288 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - | 5289 | `	}` |
|     53 | 5290 | `	return PH7_OK;` |
|     29 | 5291 | `}` |
|      - | 5292 | `/*` |
|      - | 5293 | ` * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])` |
|      - | 5294 | ` *   Generate a keyed hash value using the HMAC method (RFC 2104).` |
|      - | 5295 | ` */` |
|     16 | 5296 | `static int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5297 | `{` |
|      - | 5298 | `	const HashAlgo *pAlgo;` |
|      - | 5299 | `	const char *zAlgo,*zData,*zKey;` |
|     18 | 5300 | `	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;` |
|      - | 5301 | `	HashCtx sCtx;` |
|      - | 5302 | `	unsigned char zKeyBlock[128],zIpad[128],zOpad[128],zInner[64],zDigest[64];` |
|      - | 5303 | `	int i,nBlock,nDigest;` |
|     18 | 5304 | `	if( nArg < 3 ){` |
|    ! 0 | 5305 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5306 | `			"hash_hmac() expects at least 3 arguments, %d given",nArg);` |
|      - | 5307 | `	}` |
|     18 | 5308 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     18 | 5309 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     18 | 5310 | `	if( pAlgo == 0 ){` |
|      3 | 5311 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5312 | `			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 5313 | `	}` |
|     15 | 5314 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     15 | 5315 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|     15 | 5316 | `	if( nArg > 3 ){` |
|      3 | 5317 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|      1 | 5318 | `	}` |
|     15 | 5319 | `	nBlock = pAlgo->nBlockLen;` |
|     15 | 5320 | `	nDigest = pAlgo->nDigestLen;` |
|      - | 5321 | `	/* Reduce the key to a single block: hash it if longer than the block, then` |
|      - | 5322 | `	 * zero-pad (a short or empty key is just zero-padded). */` |
|     15 | 5323 | `	SyZero(zKeyBlock,sizeof(zKeyBlock));` |
|     15 | 5324 | `	if( nKeyLen > nBlock ){` |
|      3 | 5325 | `		pAlgo->xInit(&sCtx);` |
|      3 | 5326 | `		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);` |
|      3 | 5327 | `		pAlgo->xFinal(&sCtx,zKeyBlock);` |
|     14 | 5328 | `	}else if( nKeyLen > 0 ){` |
|     11 | 5329 | `		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);` |
|      5 | 5330 | `	}` |
|   1039 | 5331 | `	for( i = 0; i < nBlock; i++ ){` |
|   1025 | 5332 | `		zIpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);` |
|   1025 | 5333 | `		zOpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);` |
|    513 | 5334 | `	}` |
|      - | 5335 | `	/* inner = H((key ^ ipad) \|\| data) */` |
|     15 | 5336 | `	pAlgo->xInit(&sCtx);` |
|     15 | 5337 | `	pAlgo->xUpdate(&sCtx,zIpad,(unsigned int)nBlock);` |
|     15 | 5338 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     15 | 5339 | `	pAlgo->xFinal(&sCtx,zInner);` |
|      - | 5340 | `	/* out = H((key ^ opad) \|\| inner) */` |
|     15 | 5341 | `	pAlgo->xInit(&sCtx);` |
|     15 | 5342 | `	pAlgo->xUpdate(&sCtx,zOpad,(unsigned int)nBlock);` |
|     15 | 5343 | `	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)nDigest);` |
|     15 | 5344 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     15 | 5345 | `	if( raw_output ){` |
|      3 | 5346 | `		ph7_result_string(pCtx,(const char *)zDigest,nDigest);` |
|      2 | 5347 | `	}else{` |
|     13 | 5348 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);` |
|      - | 5349 | `	}` |
|     15 | 5350 | `	return PH7_OK;` |
|     10 | 5351 | `}` |
|      - | 5352 | `/*` |
|      - | 5353 | ` * bool hash_equals(string $known_string,string $user_string)` |
|      - | 5354 | ` *   Timing-attack-safe string comparison.` |
|      - | 5355 | ` */` |
|     14 | 5356 | `static int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 5357 | `{` |
|      - | 5358 | `	const char *zKnown,*zUser;` |
|      - | 5359 | `	int nKnown,nUser,i;` |
|     17 | 5360 | `	volatile unsigned char vDiff = 0;` |
|     17 | 5361 | `	if( nArg < 2 ){` |
|    ! 0 | 5362 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5363 | `			"hash_equals() expects exactly 2 arguments, %d given",nArg);` |
|      - | 5364 | `	}` |
|     17 | 5365 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      4 | 5366 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5367 | `			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",` |
|      1 | 5368 | `			ph7_type_name(apArg[0]));` |
|      - | 5369 | `	}` |
|     14 | 5370 | `	if( !ph7_value_is_string(apArg[1]) ){` |
|      4 | 5371 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5372 | `			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",` |
|      2 | 5373 | `			ph7_type_name(apArg[1]));` |
|      - | 5374 | `	}` |
|     11 | 5375 | `	zKnown = ph7_value_to_string(apArg[0],&nKnown);` |
|     11 | 5376 | `	zUser = ph7_value_to_string(apArg[1],&nUser);` |
|     11 | 5377 | `	if( nKnown != nUser ){` |
|      5 | 5378 | `		ph7_result_bool(pCtx,0);` |
|      5 | 5379 | `		return PH7_OK;` |
|      - | 5380 | `	}` |
|      - | 5381 | `	/* Constant-time: read every byte, never short-circuit. */` |
|     19 | 5382 | `	for( i = 0; i < nKnown; i++ ){` |
|     13 | 5383 | `		vDiff \|= (unsigned char)(zKnown[i] ^ zUser[i]);` |
|      7 | 5384 | `	}` |
|      7 | 5385 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|      7 | 5386 | `	return PH7_OK;` |
|     10 | 5387 | `}` |
|      - | 5388 | `/*` |
|      - | 5389 | ` * array hash_algos(void)` |
|      - | 5390 | ` *   Return a list of the registered hashing algorithms.` |
|      - | 5391 | ` */` |
|      2 | 5392 | `static int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5393 | `{` |
|      - | 5394 | `	ph7_value *pArray,*pValue;` |
|      - | 5395 | `	sxu32 i;` |
|      1 | 5396 | `	SXUNUSED(nArg);` |
|      1 | 5397 | `	SXUNUSED(apArg);` |
|      3 | 5398 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5399 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 5400 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 5401 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5402 | `		return PH7_OK;` |
|      - | 5403 | `	}` |
|     15 | 5404 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|     13 | 5405 | `		ph7_value_string(pValue,aHashAlgo[i].zName,-1);` |
|     13 | 5406 | `		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);` |
|     13 | 5407 | `		ph7_value_reset_string_cursor(pValue);` |
|      7 | 5408 | `	}` |
|      3 | 5409 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5410 | `	return PH7_OK;` |
|      2 | 5411 | `}` |
|      - | 5412 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 5413 | `/*` |
|      - | 5414 | ` * password_* (bcrypt). These live in ext/standard in real PHP — outside the` |
|      - | 5415 | ` * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.` |
|      - | 5416 | ` */` |
|      - | 5417 | `/*` |
|      - | 5418 | ` * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a` |
|      - | 5419 | ` * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).` |
|      - | 5420 | ` */` |
|     40 | 5421 | `static int BcryptParseHash(const char *zHash,int nHash,int *piCost)` |
|      1 | 5422 | `{` |
|      - | 5423 | `	int iCost;` |
|     40 | 5424 | `	if( nHash != 60 \|\| zHash[0] != '$' \|\| zHash[1] != '2' \|\| zHash[3] != '$'` |
|     29 | 5425 | `		\|\| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){` |
|     13 | 5426 | `		return FALSE;` |
|      - | 5427 | `	}` |
|     29 | 5428 | `	if( zHash[4] < '0' \|\| zHash[4] > '9' \|\| zHash[5] < '0' \|\| zHash[5] > '9' \|\| zHash[6] != '$' ){` |
|    ! 0 | 5429 | `		return FALSE;` |
|      - | 5430 | `	}` |
|     29 | 5431 | `	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');` |
|     29 | 5432 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      3 | 5433 | `		return FALSE;` |
|      - | 5434 | `	}` |
|     27 | 5435 | `	if( piCost ){ *piCost = iCost; }` |
|     27 | 5436 | `	return TRUE;` |
|     21 | 5437 | `}` |
|      - | 5438 | `/*` |
|      - | 5439 | ` * TRUE if the $algo argument selects bcrypt: null (PASSWORD_DEFAULT) or the` |
|      - | 5440 | ` * "2y" id (PASSWORD_BCRYPT/PASSWORD_DEFAULT). bcrypt is the only supported algo.` |
|      - | 5441 | ` */` |
|     20 | 5442 | `static int BcryptIsBcryptAlgo(ph7_value *pAlgo)` |
|      3 | 5443 | `{` |
|     23 | 5444 | `	if( ph7_value_is_null(pAlgo) ){` |
|    ! 0 | 5445 | `		return TRUE;` |
|      - | 5446 | `	}` |
|     23 | 5447 | `	if( ph7_value_is_string(pAlgo) ){` |
|      - | 5448 | `		int nAlgo;` |
|     23 | 5449 | `		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);` |
|     23 | 5450 | `		return ( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' );` |
|      - | 5451 | `	}` |
|    ! 0 | 5452 | `	return FALSE;` |
|     13 | 5453 | `}` |
|      - | 5454 | `/*` |
|      - | 5455 | ` * bool\|string password_hash(string $password,string\|int\|null $algo[,array $options])` |
|      - | 5456 | ` *  Create a bcrypt hash of the password.` |
|      - | 5457 | ` */` |
|     16 | 5458 | `static int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 5459 | `{` |
|      - | 5460 | `	const char *zPwd;` |
|     19 | 5461 | `	int nPwd,iCost = 12;` |
|      - | 5462 | `	unsigned char aSalt[16];` |
|      - | 5463 | `	char zHash[60];` |
|     19 | 5464 | `	if( nArg < 2 ){` |
|    ! 0 | 5465 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5466 | `			"password_hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5467 | `	}` |
|     19 | 5468 | `	if( !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      3 | 5469 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5470 | `			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");` |
|      - | 5471 | `	}` |
|      - | 5472 | `	/* cost from $options['cost'] (default 12). */` |
|     16 | 5473 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|     14 | 5474 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|     14 | 5475 | `		if( pCost ){ iCost = ph7_value_to_int(pCost); }` |
|      6 | 5476 | `	}` |
|     16 | 5477 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      4 | 5478 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 5479 | `			"Invalid bcrypt cost parameter specified: %d",iCost);` |
|      - | 5480 | `	}` |
|     13 | 5481 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     13 | 5482 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 5483 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - | 5484 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|      - | 5485 | `	}` |
|     13 | 5486 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){` |
|    ! 0 | 5487 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5488 | `		return PH7_OK;` |
|      - | 5489 | `	}` |
|     13 | 5490 | `	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));` |
|     13 | 5491 | `	return PH7_OK;` |
|     11 | 5492 | `}` |
|      - | 5493 | `/*` |
|      - | 5494 | ` * bool password_verify(string $password,string $hash)` |
|      - | 5495 | ` *  Verify a password against a bcrypt hash. Never throws on a malformed hash.` |
|      - | 5496 | ` */` |
|     28 | 5497 | `static int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5498 | `{` |
|      - | 5499 | `	const char *zPwd,*zHash;` |
|      - | 5500 | `	int nPwd,nHash,iCost,i;` |
|      - | 5501 | `	unsigned char aSalt[16];` |
|      - | 5502 | `	char zComputed[60];` |
|     29 | 5503 | `	volatile unsigned char vDiff = 0;` |
|     29 | 5504 | `	if( nArg < 2 ){` |
|    ! 0 | 5505 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5506 | `			"password_verify() expects exactly 2 arguments, %d given",nArg);` |
|      - | 5507 | `	}` |
|     29 | 5508 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     29 | 5509 | `	zHash = ph7_value_to_string(apArg[1],&nHash);` |
|     29 | 5510 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|     11 | 5511 | `		ph7_result_bool(pCtx,0);` |
|     11 | 5512 | `		return PH7_OK;` |
|      - | 5513 | `	}` |
|      - | 5514 | `	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */` |
|     19 | 5515 | `	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 5516 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5517 | `		return PH7_OK;` |
|      - | 5518 | `	}` |
|     19 | 5519 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){` |
|    ! 0 | 5520 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5521 | `		return PH7_OK;` |
|      - | 5522 | `	}` |
|      - | 5523 | `	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps` |
|      - | 5524 | `	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */` |
|    577 | 5525 | `	for( i = 29; i < 60; i++ ){` |
|    559 | 5526 | `		vDiff \|= (unsigned char)(zComputed[i] ^ zHash[i]);` |
|    280 | 5527 | `	}` |
|     19 | 5528 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|     19 | 5529 | `	return PH7_OK;` |
|     15 | 5530 | `}` |
|      - | 5531 | `/*` |
|      - | 5532 | ` * array password_get_info(string $hash)` |
|      - | 5533 | ` *  Return ["algo"=>id\|null, "algoName"=>name, "options"=>[...]].` |
|      - | 5534 | ` */` |
|      6 | 5535 | `static int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5536 | `{` |
|      7 | 5537 | `	const char *zHash = "";` |
|      7 | 5538 | `	int nHash,iCost = 0,bBcrypt = 0;` |
|      - | 5539 | `	ph7_value *pArray,*pOptions,*pVal;` |
|      7 | 5540 | `	if( nArg > 0 ){` |
|      7 | 5541 | `		zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 5542 | `		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);` |
|      3 | 5543 | `	}` |
|      7 | 5544 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 5545 | `	pOptions = ph7_context_new_array(pCtx);` |
|      7 | 5546 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      7 | 5547 | `	if( pArray == 0 \|\| pOptions == 0 \|\| pVal == 0 ){` |
|    ! 0 | 5548 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5549 | `		return PH7_OK;` |
|      - | 5550 | `	}` |
|      7 | 5551 | `	if( bBcrypt ){` |
|      5 | 5552 | `		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */` |
|      5 | 5553 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 5554 | `		ph7_value_reset_string_cursor(pVal);` |
|      5 | 5555 | `		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);` |
|      5 | 5556 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      5 | 5557 | `		ph7_value_int(pVal,iCost);` |
|      5 | 5558 | `		ph7_array_add_strkey_elem(pOptions,"cost",pVal);` |
|      3 | 5559 | `	}else{` |
|      3 | 5560 | `		ph7_value_null(pVal);                          /* algo => null */` |
|      3 | 5561 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      3 | 5562 | `		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);` |
|      3 | 5563 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      - | 5564 | `	}` |
|      7 | 5565 | `	ph7_array_add_strkey_elem(pArray,"options",pOptions);` |
|      7 | 5566 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 5567 | `	return PH7_OK;` |
|      4 | 5568 | `}` |
|      - | 5569 | `/*` |
|      - | 5570 | ` * bool password_needs_rehash(string $hash,string\|int\|null $algo[,array $options])` |
|      - | 5571 | ` *  True if the hash was not made with the given algo/options.` |
|      - | 5572 | ` */` |
|      6 | 5573 | `static int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5574 | `{` |
|      - | 5575 | `	const char *zHash;` |
|      7 | 5576 | `	int nHash,iCost = 0,iWantCost = 12;` |
|      7 | 5577 | `	if( nArg < 2 ){` |
|    ! 0 | 5578 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5579 | `			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5580 | `	}` |
|      7 | 5581 | `	zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 5582 | `	if( !BcryptParseHash(zHash,nHash,&iCost) \|\| !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      - | 5583 | `		/* A non-bcrypt hash, or a request for a different algo → needs rehash. */` |
|      3 | 5584 | `		ph7_result_bool(pCtx,1);` |
|      3 | 5585 | `		return PH7_OK;` |
|      - | 5586 | `	}` |
|      5 | 5587 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|      5 | 5588 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|      5 | 5589 | `		if( pCost ){ iWantCost = ph7_value_to_int(pCost); }` |
|      2 | 5590 | `	}` |
|      5 | 5591 | `	ph7_result_bool(pCtx,iCost != iWantCost);` |
|      5 | 5592 | `	return PH7_OK;` |
|      4 | 5593 | `}` |
|      - | 5594 | `/*` |
|      - | 5595 | ` * filter_var() — input validation and sanitization (the ext/filter API).` |
|      - | 5596 | ` *` |
|      - | 5597 | ` * Filter and flag identifiers (values match PHP 8.5; the constants themselves` |
|      - | 5598 | ` * are registered in constant.c). The validate filters are hand-rolled rather` |
|      - | 5599 | ` * than delegating to SyStrToInt64/SyStrToReal: the former silently skips leading` |
|      - | 5600 | ` * zeros and cannot signal overflow, and the latter treats ',' as a decimal point` |
|      - | 5601 | ` * unconditionally — neither matches PHP's filter semantics.` |
|      - | 5602 | ` */` |
|      - | 5603 | `#define FV_VALIDATE_INT     257` |
|      - | 5604 | `#define FV_VALIDATE_BOOLEAN 258` |
|      - | 5605 | `#define FV_VALIDATE_FLOAT   259` |
|      - | 5606 | `#define FV_VALIDATE_REGEXP  272` |
|      - | 5607 | `#define FV_VALIDATE_URL     273` |
|      - | 5608 | `#define FV_VALIDATE_EMAIL   274` |
|      - | 5609 | `#define FV_VALIDATE_IP      275` |
|      - | 5610 | `#define FV_VALIDATE_MAC     276` |
|      - | 5611 | `#define FV_VALIDATE_DOMAIN  277` |
|      - | 5612 | `#define FV_SANITIZE_SPECIAL_CHARS      515` |
|      - | 5613 | `#define FV_DEFAULT          516 /* == FILTER_UNSAFE_RAW: pass the value through */` |
|      - | 5614 | `#define FV_SANITIZE_EMAIL   517` |
|      - | 5615 | `#define FV_SANITIZE_URL     518` |
|      - | 5616 | `#define FV_SANITIZE_NUMBER_INT   519` |
|      - | 5617 | `#define FV_SANITIZE_NUMBER_FLOAT 520` |
|      - | 5618 | `#define FV_SANITIZE_FULL_SPECIAL_CHARS 522` |
|      - | 5619 | `#define FV_FLAG_ALLOW_OCTAL  1` |
|      - | 5620 | `#define FV_FLAG_ALLOW_HEX    2` |
|      - | 5621 | `#define FV_FLAG_STRIP_LOW    4` |
|      - | 5622 | `#define FV_FLAG_STRIP_HIGH   8` |
|      - | 5623 | `#define FV_FLAG_ENCODE_LOW   16` |
|      - | 5624 | `#define FV_FLAG_ENCODE_HIGH  32` |
|      - | 5625 | `#define FV_FLAG_ENCODE_AMP   64` |
|      - | 5626 | `#define FV_FLAG_NO_ENCODE_QUOTES 128` |
|      - | 5627 | `#define FV_FLAG_STRIP_BACKTICK   512` |
|      - | 5628 | `#define FV_FLAG_ALLOW_FRACTION   4096` |
|      - | 5629 | `#define FV_FLAG_ALLOW_THOUSAND   8192` |
|      - | 5630 | `#define FV_FLAG_ALLOW_SCIENTIFIC 16384` |
|      - | 5631 | `#define FV_FLAG_IPV4  1048576` |
|      - | 5632 | `#define FV_FLAG_IPV6  2097152` |
|      - | 5633 | `#define FV_NULL_ON_FAILURE 134217728` |
|      - | 5634 | `/* The subset of flags the UNSAFE_RAW/DEFAULT string filter (FvSanitizeString)` |
|      - | 5635 | ` * acts on: when none are set the filter is a verbatim pass-through, so FV_DEFAULT` |
|      - | 5636 | ` * can shortcut. Keep this in sync with FvSanitizeString's flag handling. */` |
|      - | 5637 | `#define FV_FLAG_STRING_MASK (FV_FLAG_STRIP_LOW\|FV_FLAG_STRIP_HIGH\|FV_FLAG_STRIP_BACKTICK \` |
|      - | 5638 | `                            \|FV_FLAG_ENCODE_LOW\|FV_FLAG_ENCODE_HIGH\|FV_FLAG_ENCODE_AMP)` |
|      - | 5639 |  |
|      - | 5640 | `/* Trim leading/trailing PHP whitespace, adjusting the (*pz,*pn) view in place.` |
|      - | 5641 | ` * SyisSpace (isspace) matches PHP's filter whitespace set " \t\n\r\v\f". */` |
|    153 | 5642 | `static void FvTrim(const char **pz,int *pn){` |
|    153 | 5643 | `	const char *z = *pz;` |
|    153 | 5644 | `	int n = *pn;` |
|    157 | 5645 | `	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }` |
|    161 | 5646 | `	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }` |
|    153 | 5647 | `	*pz = z; *pn = n;` |
|    153 | 5648 | `}` |
|      - | 5649 | `/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     57 | 5650 | `static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){` |
|     57 | 5651 | `	int neg = 0, i;` |
|     57 | 5652 | `	sxu64 u = 0;` |
|     57 | 5653 | `	FvTrim(&z,&n);` |
|     57 | 5654 | `	if( n==0 ){ return 0; }` |
|     51 | 5655 | `	if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }` |
|     51 | 5656 | `	if( n==0 ){ return 0; }` |
|     49 | 5657 | `	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'\|\|z[1]=='X') ){` |
|      3 | 5658 | `		z += 2; n -= 2;` |
|      3 | 5659 | `		if( n==0 ){ return 0; }` |
|      7 | 5660 | `		for( i=0; i<n; i++ ){` |
|      5 | 5661 | `			int h = SyHexToint((unsigned char)z[i]);` |
|      5 | 5662 | `			if( h<0 ){ return 0; }` |
|      5 | 5663 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }` |
|      5 | 5664 | `			u = u*16 + (sxu64)h;` |
|      3 | 5665 | `		}` |
|     48 | 5666 | `	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){` |
|      9 | 5667 | `		for( i=0; i<n; i++ ){` |
|      7 | 5668 | `			if( z[i]<'0' \|\| z[i]>'7' ){ return 0; }` |
|      7 | 5669 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }` |
|      7 | 5670 | `			u = u*8 + (sxu64)(z[i]-'0');` |
|      4 | 5671 | `		}` |
|      2 | 5672 | `	}else{` |
|     45 | 5673 | `		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */` |
|    201 | 5674 | `		for( i=0; i<n; i++ ){` |
|    173 | 5675 | `			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|    161 | 5676 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }` |
|    161 | 5677 | `			u = u*10 + (sxu64)(z[i]-'0');` |
|     81 | 5678 | `		}` |
|      - | 5679 | `	}` |
|     33 | 5680 | `	if( neg ){` |
|      5 | 5681 | `		if( u > 0x8000000000000000ULL ){ return 0; }` |
|      5 | 5682 | `		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */` |
|      3 | 5683 | `	}else{` |
|     29 | 5684 | `		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }` |
|     27 | 5685 | `		*pOut = (ph7_int64)u;` |
|      - | 5686 | `	}` |
|     31 | 5687 | `	return 1;` |
|     29 | 5688 | `}` |
|      - | 5689 | `/* FILTER_VALIDATE_FLOAT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     69 | 5690 | `static int FvValidateFloat(const char *z,int n,int flags,double *pOut){` |
|      - | 5691 | `	char zBuf[512];` |
|     69 | 5692 | `	int i, m = 0, seenDigit = 0;` |
|     69 | 5693 | `	const char *zv; int nv; double d = 0;` |
|     69 | 5694 | `	FvTrim(&z,&n);` |
|      - | 5695 | `	/* Bound the input: zBuf[512] holds the thousand-separator-stripped copy, and` |
|      - | 5696 | `	 * the cap also rejects the pathological 500+ digit floats PHP refuses. */` |
|     69 | 5697 | `	if( n==0 \|\| n>500 ){ return 0; }` |
|     69 | 5698 | `	if( flags & FV_FLAG_ALLOW_THOUSAND ){` |
|      - | 5699 | `		/* Commas are optional, but when present they must group the integer part` |
|      - | 5700 | `		 * into a leading run of 1..3 digits followed by groups of exactly 3` |
|      - | 5701 | `		 * ("1,000" ok, "1,5"/"1234,567" rejected). Strip them into zBuf and reject` |
|      - | 5702 | `		 * a comma anywhere in the fractional/exponent tail. */` |
|     25 | 5703 | `		int s = 0, intEnd, segStart, segIdx, hasComma = 0;` |
|     25 | 5704 | `		if( s<n && (z[s]=='+'\|\|z[s]=='-') ){ zBuf[m++] = z[s]; s++; }` |
|     25 | 5705 | `		intEnd = s;` |
|    167 | 5706 | `		while( intEnd<n && z[intEnd]!='.' && z[intEnd]!='e' && z[intEnd]!='E' ){` |
|    143 | 5707 | `			if( z[intEnd]==',' ){ hasComma = 1; }` |
|    143 | 5708 | `			intEnd++;` |
|      1 | 5709 | `		}` |
|     25 | 5710 | `		if( hasComma ){` |
|     25 | 5711 | `			segStart = s; segIdx = 0;` |
|    165 | 5712 | `			for( i=s; i<=intEnd; i++ ){` |
|    151 | 5713 | `				if( i==intEnd \|\| z[i]==',' ){` |
|     49 | 5714 | `					int segLen = i - segStart, k;` |
|     49 | 5715 | `					if( segIdx==0 ){ if( segLen<1 \|\| segLen>3 ){ return 0; } }` |
|     25 | 5716 | `					else if( segLen!=3 ){ return 0; }` |
|    119 | 5717 | `					for( k=segStart; k<i; k++ ){` |
|     81 | 5718 | `						if( !SyisDigit((unsigned char)z[k]) ){ return 0; }` |
|     81 | 5719 | `						zBuf[m++] = z[k];` |
|     41 | 5720 | `					}` |
|     39 | 5721 | `					segStart = i+1; segIdx++;` |
|     19 | 5722 | `				}` |
|     71 | 5723 | `			}` |
|      8 | 5724 | `		}else{` |
|    ! 0 | 5725 | `			for( i=s; i<intEnd; i++ ){ zBuf[m++] = z[i]; }` |
|      - | 5726 | `		}` |
|     27 | 5727 | `		for( i=intEnd; i<n; i++ ){` |
|     13 | 5728 | `			if( z[i]==',' ){ return 0; }` |
|     13 | 5729 | `			zBuf[m++] = z[i];` |
|      7 | 5730 | `		}` |
|     15 | 5731 | `		zv = zBuf; nv = m;` |
|      8 | 5732 | `	}else{` |
|     45 | 5733 | `		zv = z; nv = n;` |
|      - | 5734 | `	}` |
|     59 | 5735 | `	i = 0;` |
|     59 | 5736 | `	if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|    167 | 5737 | `	while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     59 | 5738 | `	if( i<nv && zv[i]=='.' ){` |
|     21 | 5739 | `		i++;` |
|     39 | 5740 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     10 | 5741 | `	}` |
|     59 | 5742 | `	if( !seenDigit ){ return 0; }` |
|     57 | 5743 | `	if( i<nv && (zv[i]=='e'\|\|zv[i]=='E') ){` |
|     29 | 5744 | `		i++;` |
|     29 | 5745 | `		if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|     29 | 5746 | `		if( i>=nv \|\| !SyisDigit((unsigned char)zv[i]) ){ return 0; }` |
|    105 | 5747 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; }` |
|     14 | 5748 | `	}` |
|     57 | 5749 | `	if( i!=nv ){ return 0; } /* trailing junk */` |
|      - | 5750 | `	/* The grammar above guarantees zv[0..nv) is a clean ASCII decimal float (no hex /` |
|      - | 5751 | `	 * inf / nan / trailing junk), so it is safe to hand to libc strtod, which — unlike` |
|      - | 5752 | `	 * SyStrToReal (15 sig-digits + exponent clamped to 308, so it silently saturates` |
|      - | 5753 | `	 * overflowing magnitudes to a finite value) — is overflow/underflow-aware and` |
|      - | 5754 | `	 * correctly rounded. strtod needs a NUL-terminated string: the ALLOW_THOUSAND path` |
|      - | 5755 | `	 * already built the span in zBuf (zv==zBuf); the plain path must copy it there (z is` |
|      - | 5756 | `	 * const + not NUL-terminated). nv <= n <= 500 < sizeof(zBuf) by the cap above.` |
|      - | 5757 | `	 * Matches PHP 8.5 byte-for-byte: reject overflow (-> +/-INF) and total underflow` |
|      - | 5758 | `	 * (-> 0.0), keep subnormals (nonzero, errno==ERANGE) and a genuine "0" (errno==0). */` |
|     53 | 5759 | `	if( zv != zBuf ){ SyMemcpy(zv,zBuf,(sxu32)nv); }` |
|     53 | 5760 | `	zBuf[nv] = 0;` |
|     53 | 5761 | `	errno = 0;` |
|     53 | 5762 | `	d = strtod(zBuf,0);` |
|     53 | 5763 | `	if( errno == ERANGE && (d == HUGE_VAL \|\| d == -HUGE_VAL \|\| d == 0.0) ){` |
|     15 | 5764 | `		return 0;` |
|      - | 5765 | `	}` |
|     39 | 5766 | `	*pOut = d;` |
|     39 | 5767 | `	return 1;` |
|     35 | 5768 | `}` |
|      - | 5769 | `/* FILTER_VALIDATE_BOOLEAN. Returns 1 if the string is recognized (sets *pBool),` |
|      - | 5770 | ` * 0 if it is unrecognized (the failure path). "0"/"false"/"" are recognized as` |
|      - | 5771 | ` * false, NOT failures. */` |
|     33 | 5772 | `static int FvValidateBool(const char *z,int n,int *pBool){` |
|     33 | 5773 | `	FvTrim(&z,&n);` |
|     32 | 5774 | `	if( (n==1 && z[0]=='1') \|\| (n==4 && SyStrnicmp(z,"true",4)==0)` |
|     25 | 5775 | `	    \|\| (n==2 && SyStrnicmp(z,"on",2)==0) \|\| (n==3 && SyStrnicmp(z,"yes",3)==0) ){` |
|     11 | 5776 | `		*pBool = 1; return 1;` |
|      - | 5777 | `	}` |
|     22 | 5778 | `	if( n==0 \|\| (n==1 && z[0]=='0') \|\| (n==5 && SyStrnicmp(z,"false",5)==0)` |
|     11 | 5779 | `	    \|\| (n==3 && SyStrnicmp(z,"off",3)==0) \|\| (n==2 && SyStrnicmp(z,"no",2)==0) ){` |
|     11 | 5780 | `		*pBool = 0; return 1;` |
|      - | 5781 | `	}` |
|      9 | 5782 | `	return 0;` |
|     15 | 5783 | `}` |
|      - | 5784 | `/* IPv4 dotted-quad: exactly 4 octets 0..255, no leading zeros. */` |
|     33 | 5785 | `static int FvValidateIp4(const char *z,int n){` |
|     33 | 5786 | `	int i = 0, parts = 0;` |
|     77 | 5787 | `	while( i<n ){` |
|     65 | 5788 | `		int val = 0, digits = 0, start = i;` |
|    143 | 5789 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){` |
|     85 | 5790 | `			val = val*10 + (z[i]-'0');` |
|     85 | 5791 | `			if( val>255 ){ return 0; }` |
|     79 | 5792 | `			digits++; i++;` |
|      1 | 5793 | `		}` |
|     59 | 5794 | `		if( digits==0 \|\| digits>3 ){ return 0; }` |
|     49 | 5795 | `		if( digits>1 && z[start]=='0' ){ return 0; } /* leading zero */` |
|     45 | 5796 | `		parts++;` |
|     45 | 5797 | `		if( parts>4 ){ return 0; }` |
|     45 | 5798 | `		if( i<n ){` |
|     33 | 5799 | `			if( z[i]!='.' ){ return 0; }` |
|     33 | 5800 | `			i++;` |
|     33 | 5801 | `			if( i>=n ){ return 0; } /* trailing dot */` |
|     16 | 5802 | `		}` |
|      1 | 5803 | `	}` |
|     13 | 5804 | `	return parts==4;` |
|     17 | 5805 | `}` |
|      - | 5806 | `/* A colon-separated run of IPv6 hextets with no "::" (n may be 0 -> 0 groups),` |
|      - | 5807 | ` * allowing a trailing embedded IPv4. Returns the 16-bit group count or -1. */` |
|     19 | 5808 | `static int FvIp6Hextets(const char *z,int n){` |
|     19 | 5809 | `	int i = 0, segStart = 0, groups = 0;` |
|     19 | 5810 | `	if( n==0 ){ return 0; }` |
|    145 | 5811 | `	while( i<=n ){` |
|    133 | 5812 | `		if( i==n \|\| z[i]==':' ){` |
|     23 | 5813 | `			int segLen = i - segStart, j, isV4 = 0;` |
|     23 | 5814 | `			if( segLen==0 ){ return -1; } /* an empty hextet (stray ':') */` |
|     77 | 5815 | `			for( j=segStart; j<i; j++ ){ if( z[j]=='.' ){ isV4 = 1; break; } }` |
|     23 | 5816 | `			if( isV4 ){` |
|     11 | 5817 | `				if( i!=n ){ return -1; } /* IPv4 only as the final token */` |
|     11 | 5818 | `				if( !FvValidateIp4(z+segStart,segLen) ){ return -1; }` |
|      5 | 5819 | `				groups += 2;` |
|      3 | 5820 | `			}else{` |
|     13 | 5821 | `				if( segLen>4 ){ return -1; }` |
|     47 | 5822 | `				for( j=segStart; j<i; j++ ){ if( SyHexToint((unsigned char)z[j])<0 ){ return -1; } }` |
|     13 | 5823 | `				groups++;` |
|      - | 5824 | `			}` |
|     17 | 5825 | `			segStart = i+1;` |
|      8 | 5826 | `		}` |
|    127 | 5827 | `		i++;` |
|      1 | 5828 | `	}` |
|     13 | 5829 | `	return groups;` |
|     10 | 5830 | `}` |
|      - | 5831 | `/* IPv6: at most one "::" zero-run; 8 groups exactly, or fewer when "::" present. */` |
|     19 | 5832 | `static int FvValidateIp6(const char *z,int n){` |
|     19 | 5833 | `	const char *zDbl = 0;` |
|      - | 5834 | `	int i, ga, gb;` |
|    139 | 5835 | `	for( i=0; i+1<n; i++ ){` |
|    123 | 5836 | `		if( z[i]==':' && z[i+1]==':' ){` |
|     13 | 5837 | `			if( zDbl ){ return 0; } /* a second "::" is invalid */` |
|     11 | 5838 | `			zDbl = z+i;` |
|      5 | 5839 | `		}` |
|     61 | 5840 | `	}` |
|     17 | 5841 | `	if( zDbl==0 ){` |
|      9 | 5842 | `		return FvIp6Hextets(z,n)==8;` |
|    ! 0 | 5843 | `	}else{` |
|      9 | 5844 | `		int lenA = (int)(zDbl - z);` |
|      9 | 5845 | `		int lenB = n - lenA - 2;` |
|      9 | 5846 | `		ga = (lenA==0) ? 0 : FvIp6Hextets(z,lenA);` |
|      9 | 5847 | `		gb = (lenB==0) ? 0 : FvIp6Hextets(zDbl+2,lenB);` |
|      9 | 5848 | `		if( ga<0 \|\| gb<0 ){ return 0; }` |
|      9 | 5849 | `		return (ga+gb)<=7; /* "::" stands for at least one zero group */` |
|      - | 5850 | `	}` |
|     10 | 5851 | `}` |
|     25 | 5852 | `static int FvValidateIp(const char *z,int n,int flags){` |
|     25 | 5853 | `	int v4 = (flags & FV_FLAG_IPV4), v6 = (flags & FV_FLAG_IPV6);` |
|     25 | 5854 | `	if( !v4 && !v6 ){ v4 = v6 = 1; } /* default accepts either family */` |
|     25 | 5855 | `	if( v4 && FvValidateIp4(z,n) ){ return 1; }` |
|     21 | 5856 | `	if( v6 && FvValidateIp6(z,n) ){ return 1; }` |
|     13 | 5857 | `	return 0;` |
|     13 | 5858 | `}` |
|      - | 5859 | `/* FILTER_VALIDATE_MAC: 17-char colon- or dash-separated hex (XX:XX:..:XX). */` |
|     11 | 5860 | `static int FvValidateMac(const char *z,int n){` |
|      - | 5861 | `	char sep;` |
|      - | 5862 | `	int i;` |
|     11 | 5863 | `	if( n!=17 ){ return 0; }` |
|      7 | 5864 | `	sep = z[2];` |
|      7 | 5865 | `	if( sep!=':' && sep!='-' ){ return 0; }` |
|    105 | 5866 | `	for( i=0; i<17; i++ ){` |
|    101 | 5867 | `		if( (i%3)==2 ){ if( z[i]!=sep ){ return 0; } }` |
|     71 | 5868 | `		else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }` |
|     50 | 5869 | `	}` |
|      5 | 5870 | `	return 1;` |
|      6 | 5871 | `}` |
|      - | 5872 | `/* FILTER_VALIDATE_EMAIL (best-effort: covers the common cases, not quoted local` |
|      - | 5873 | ` * parts or IP-literal domains). */` |
|     28 | 5874 | `static int FvValidateEmail(const char *z,int n){` |
|     28 | 5875 | `	int at = -1, i, localLen, domLen, labelStart, dotCount = 0;` |
|      - | 5876 | `	const char *zDom;` |
|     28 | 5877 | `	if( n==0 \|\| n>320 ){ return 0; }` |
|    201 | 5878 | `	for( i=0; i<n; i++ ){` |
|    181 | 5879 | `		if( z[i]=='@' ){ if( at>=0 ){ return 0; } at = i; }` |
|     91 | 5880 | `	}` |
|     21 | 5881 | `	if( at<=0 \|\| at==n-1 ){ return 0; } /* one '@', non-empty local and domain */` |
|     21 | 5882 | `	localLen = at;` |
|     21 | 5883 | `	zDom = z + at + 1;` |
|     21 | 5884 | `	domLen = n - at - 1;` |
|     21 | 5885 | `	if( z[0]=='.' \|\| z[at-1]=='.' ){ return 0; }` |
|     57 | 5886 | `	for( i=0; i<localLen; i++ ){` |
|     43 | 5887 | `		unsigned char c = (unsigned char)z[i];` |
|     43 | 5888 | `		if( c<=' ' ){ return 0; }` |
|     41 | 5889 | `		if( c=='.' && i+1<localLen && z[i+1]=='.' ){ return 0; }` |
|     20 | 5890 | `	}` |
|     15 | 5891 | `	if( zDom[0]=='.' \|\| zDom[domLen-1]=='.' ){ return 0; }` |
|     13 | 5892 | `	labelStart = 0;` |
|     85 | 5893 | `	for( i=0; i<=domLen; i++ ){` |
|     75 | 5894 | `		if( i==domLen \|\| zDom[i]=='.' ){` |
|     25 | 5895 | `			int ll = i - labelStart;` |
|     25 | 5896 | `			if( ll==0 ){ return 0; } /* consecutive dots */` |
|     23 | 5897 | `			if( zDom[labelStart]=='-' \|\| zDom[i-1]=='-' ){ return 0; }` |
|     23 | 5898 | `			if( i<domLen ){ dotCount++; }` |
|     23 | 5899 | `			labelStart = i+1;` |
|     12 | 5900 | `		}else{` |
|     51 | 5901 | `			unsigned char c = (unsigned char)zDom[i];` |
|     51 | 5902 | `			if( !((c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9')\|\|c=='-') ){ return 0; }` |
|      - | 5903 | `		}` |
|     37 | 5904 | `	}` |
|     11 | 5905 | `	if( dotCount<1 ){ return 0; } /* PHP requires a dot in the domain (any TLD length) */` |
|      9 | 5906 | `	return 1;` |
|     15 | 5907 | `}` |
|      - | 5908 | `/* FILTER_VALIDATE_DOMAIN (lenient, matching PHP without FILTER_FLAG_HOSTNAME). */` |
|     11 | 5909 | `static int FvValidateDomain(const char *z,int n){` |
|      - | 5910 | `	int i;` |
|     11 | 5911 | `	if( n<1 \|\| n>253 \|\| z[0]=='.' ){ return 0; }` |
|     81 | 5912 | `	for( i=0; i<n; i++ ){` |
|     75 | 5913 | `		unsigned char c = (unsigned char)z[i];` |
|     75 | 5914 | `		if( c<=' ' ){ return 0; }` |
|     75 | 5915 | `		if( c=='.' && i+1<n && z[i+1]=='.' ){ return 0; }` |
|     37 | 5916 | `	}` |
|      7 | 5917 | `	return 1;` |
|      6 | 5918 | `}` |
|      - | 5919 | `/* FILTER_VALIDATE_URL: require a scheme and a host (PHP's filter is itself` |
|      - | 5920 | ` * parse_url-based, so PH7_VmHttpSplitURI tracks it closely). */` |
|     15 | 5921 | `static int FvValidateUrl(const char *z,int n){` |
|      - | 5922 | `	SyhttpUri sUri;` |
|     15 | 5923 | `	if( n==0 ){ return 0; }` |
|     15 | 5924 | `	SyZero(&sUri,(sxu32)sizeof(sUri));` |
|     15 | 5925 | `	if( PH7_VmHttpSplitURI(&sUri,z,(sxu32)n)!=SXRET_OK ){ return 0; }` |
|     15 | 5926 | `	return sUri.sScheme.nByte!=0 && sUri.sHost.nByte!=0;` |
|      8 | 5927 | `}` |
|      - | 5928 | `/* The Fv sanitizers build their result by appending directly to the call` |
|      - | 5929 | ` * context (ph7_result_string accumulates, like htmlspecialchars), emitting each` |
|      - | 5930 | ` * kept run in one call and seeding "" so an all-stripped input yields "". */` |
|      - | 5931 | `/* SANITIZE_NUMBER_INT (isFloat=0) / SANITIZE_NUMBER_FLOAT (isFloat=1). */` |
|     37 | 5932 | `static void FvSanitizeNumber(ph7_context *pCtx,const char *z,int n,int isFloat,int flags){` |
|     37 | 5933 | `	int i, runStart = 0;` |
|     37 | 5934 | `	ph7_result_string(pCtx,"",0);` |
|     97 | 5935 | `	for( i=0; i<n; i++ ){` |
|     91 | 5936 | `		char c = z[i];` |
|     91 | 5937 | `		int keep = (c>='0'&&c<='9') \|\| c=='+' \|\| c=='-';` |
|     91 | 5938 | `		if( !keep && isFloat ){` |
|     38 | 5939 | `			keep = (c=='.' && (flags & FV_FLAG_ALLOW_FRACTION))` |
|     23 | 5940 | `			    \|\| (c==',' && (flags & FV_FLAG_ALLOW_THOUSAND))` |
|     36 | 5941 | `			    \|\| ((c=='e'\|\|c=='E') && (flags & FV_FLAG_ALLOW_SCIENTIFIC));` |
|     12 | 5942 | `		}` |
|     61 | 5943 | `		if( !keep ){` |
|     33 | 5944 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     33 | 5945 | `			runStart = i+1;` |
|     16 | 5946 | `		}` |
|     31 | 5947 | `	}` |
|      7 | 5948 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 | 5949 | `}` |
|      - | 5950 | `/* Return non-zero when byte c must be stripped under the STRIP_* flags. Shared` |
|      - | 5951 | ` * by the UNSAFE_RAW string filter and SANITIZE_SPECIAL_CHARS. STRIP_LOW drops` |
|      - | 5952 | `` * bytes <32, STRIP_HIGH drops bytes >=127 (incl. DEL), STRIP_BACKTICK drops '`'.`` |
|      - | 5953 | ` * Matches php_filter_strip(); verified byte-exact vs php 8.5.7. */` |
|    287 | 5954 | `static int FvStripByte(unsigned char c,int flags){` |
|    287 | 5955 | `	if( (flags & FV_FLAG_STRIP_LOW)      && c<32 )    { return 1; }` |
|    281 | 5956 | `	if( (flags & FV_FLAG_STRIP_HIGH)     && c>=127 )  { return 1; }` |
|    269 | 5957 | `	if( (flags & FV_FLAG_STRIP_BACKTICK) && c==0x60 ) { return 1; }` |
|    267 | 5958 | `	return 0;` |
|    144 | 5959 | `}` |
|      - | 5960 | `/* FILTER_UNSAFE_RAW / FILTER_DEFAULT with flags: no default transform, but the` |
|      - | 5961 | ` * STRIP/ENCODE flags apply. Precedence (per php_filter_unsafe_raw, verified` |
|      - | 5962 | ` * vs php 8.5.7): a byte is first tested for stripping; a surviving byte is then` |
|      - | 5963 | ` * encoded as a decimal numeric entity if ENCODE_LOW (<32) / ENCODE_HIGH (>=127)` |
|      - | 5964 | ` * is set, and '&' becomes "&#38;" under ENCODE_AMP. So STRIP_LOW\|ENCODE_LOW` |
|      - | 5965 | ` * strips (nothing left to encode). Bytes are treated individually — ENCODE_HIGH` |
|      - | 5966 | ` * numeric-encodes each byte of a multibyte sequence separately, not the codepoint. */` |
|     25 | 5967 | `static void FvSanitizeString(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 5968 | `	int i, runStart = 0;` |
|     25 | 5969 | `	ph7_result_string(pCtx,"",0);` |
|    193 | 5970 | `	for( i=0; i<n; i++ ){` |
|    179 | 5971 | `		unsigned char c = (unsigned char)z[i];` |
|    179 | 5972 | `		if( FvStripByte(c,flags) ){` |
|     13 | 5973 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     13 | 5974 | `			runStart = i+1;` |
|     13 | 5975 | `			continue;` |
|      - | 5976 | `		}` |
|    167 | 5977 | `		if( c=='&' && (flags & FV_FLAG_ENCODE_AMP) ){` |
|      3 | 5978 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      3 | 5979 | `			ph7_result_string(pCtx,"&#38;",-1);` |
|      3 | 5980 | `			runStart = i+1;` |
|    166 | 5981 | `		}else if( (c<32 && (flags & FV_FLAG_ENCODE_LOW))` |
|    164 | 5982 | `		       \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     37 | 5983 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 5984 | `			ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|      9 | 5985 | `			runStart = i+1;` |
|      4 | 5986 | `		}` |
|     79 | 5987 | `	}` |
|     15 | 5988 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     15 | 5989 | `}` |
|      - | 5990 | `/* FILTER_SANITIZE_SPECIAL_CHARS: encode <>&"' and every control byte <32 as a` |
|      - | 5991 | ` * decimal numeric entity (&#60; &#38; &#34; ...). The STRIP_* flags remove bytes` |
|      - | 5992 | ` * before encoding; ENCODE_HIGH numeric-encodes surviving bytes >=127. Bytes >=128` |
|      - | 5993 | ` * are otherwise passed through verbatim (this filter is NOT UTF-8-aware — only the` |
|      - | 5994 | ` * FULL variant is). Byte-exact vs php 8.5.7. */` |
|     13 | 5995 | `static void FvSanitizeSpecial(ph7_context *pCtx,const char *z,int n,int flags){` |
|     13 | 5996 | `	int i, runStart = 0;` |
|      - | 5997 | `	const char *zEnt;` |
|     13 | 5998 | `	ph7_result_string(pCtx,"",0);` |
|    131 | 5999 | `	for( i=0; i<n; i++ ){` |
|    119 | 6000 | `		unsigned char c = (unsigned char)z[i];` |
|    119 | 6001 | `		if( FvStripByte(c,flags) ){` |
|      9 | 6002 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 6003 | `			runStart = i+1;` |
|      9 | 6004 | `			continue;` |
|      - | 6005 | `		}` |
|    111 | 6006 | `		switch( c ){` |
|      3 | 6007 | `		case '<':  zEnt = "&#60;"; break;` |
|      3 | 6008 | `		case '>':  zEnt = "&#62;"; break;` |
|     11 | 6009 | `		case '&':  zEnt = "&#38;"; break;` |
|      3 | 6010 | `		case '"':  zEnt = "&#34;"; break;` |
|      3 | 6011 | `		case '\'': zEnt = "&#39;"; break;` |
|     46 | 6012 | `		default:` |
|      - | 6013 | `			/* Control bytes <32 are always numeric-encoded; bytes >=127 only when` |
|      - | 6014 | `			 * ENCODE_HIGH is set. Everything else stays in the current run. */` |
|     93 | 6015 | `			if( c<32 \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     17 | 6016 | `				if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     17 | 6017 | `				ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|     17 | 6018 | `				runStart = i+1;` |
|      8 | 6019 | `			}` |
|     93 | 6020 | `			continue; /* keep in the current run */` |
|      - | 6021 | `		}` |
|     19 | 6022 | `		if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     19 | 6023 | `		ph7_result_string(pCtx,zEnt,-1); /* -1: length from strlen */` |
|     19 | 6024 | `		runStart = i+1;` |
|     10 | 6025 | `	}` |
|     13 | 6026 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     13 | 6027 | `}` |
|      - | 6028 | `/* HTML 4.01 named-entity table (codepoint -> "&name;") used by the UTF-8-aware` |
|      - | 6029 | ` * FULL_SPECIAL_CHARS filter, sorted ascending by codepoint for binary search.` |
|      - | 6030 | ` * Generated from php 8.5.7 (the exact set php_escape_html_entities emits for the` |
|      - | 6031 | ` * default document type); the five inline specials <>&"' are handled separately,` |
|      - | 6032 | ` * so every entry here is a codepoint >=0xA0. 248 rows. */` |
|      - | 6033 | `static const struct { sxu32 cp; const char *zEnt; } aHtml401Ent[] = {` |
|      - | 6034 | `	{0x00A0,"&nbsp;"},{0x00A1,"&iexcl;"},{0x00A2,"&cent;"},{0x00A3,"&pound;"},` |
|      - | 6035 | `	{0x00A4,"&curren;"},{0x00A5,"&yen;"},{0x00A6,"&brvbar;"},{0x00A7,"&sect;"},` |
|      - | 6036 | `	{0x00A8,"&uml;"},{0x00A9,"&copy;"},{0x00AA,"&ordf;"},{0x00AB,"&laquo;"},` |
|      - | 6037 | `	{0x00AC,"&not;"},{0x00AD,"&shy;"},{0x00AE,"&reg;"},{0x00AF,"&macr;"},` |
|      - | 6038 | `	{0x00B0,"&deg;"},{0x00B1,"&plusmn;"},{0x00B2,"&sup2;"},{0x00B3,"&sup3;"},` |
|      - | 6039 | `	{0x00B4,"&acute;"},{0x00B5,"&micro;"},{0x00B6,"&para;"},{0x00B7,"&middot;"},` |
|      - | 6040 | `	{0x00B8,"&cedil;"},{0x00B9,"&sup1;"},{0x00BA,"&ordm;"},{0x00BB,"&raquo;"},` |
|      - | 6041 | `	{0x00BC,"&frac14;"},{0x00BD,"&frac12;"},{0x00BE,"&frac34;"},{0x00BF,"&iquest;"},` |
|      - | 6042 | `	{0x00C0,"&Agrave;"},{0x00C1,"&Aacute;"},{0x00C2,"&Acirc;"},{0x00C3,"&Atilde;"},` |
|      - | 6043 | `	{0x00C4,"&Auml;"},{0x00C5,"&Aring;"},{0x00C6,"&AElig;"},{0x00C7,"&Ccedil;"},` |
|      - | 6044 | `	{0x00C8,"&Egrave;"},{0x00C9,"&Eacute;"},{0x00CA,"&Ecirc;"},{0x00CB,"&Euml;"},` |
|      - | 6045 | `	{0x00CC,"&Igrave;"},{0x00CD,"&Iacute;"},{0x00CE,"&Icirc;"},{0x00CF,"&Iuml;"},` |
|      - | 6046 | `	{0x00D0,"&ETH;"},{0x00D1,"&Ntilde;"},{0x00D2,"&Ograve;"},{0x00D3,"&Oacute;"},` |
|      - | 6047 | `	{0x00D4,"&Ocirc;"},{0x00D5,"&Otilde;"},{0x00D6,"&Ouml;"},{0x00D7,"&times;"},` |
|      - | 6048 | `	{0x00D8,"&Oslash;"},{0x00D9,"&Ugrave;"},{0x00DA,"&Uacute;"},{0x00DB,"&Ucirc;"},` |
|      - | 6049 | `	{0x00DC,"&Uuml;"},{0x00DD,"&Yacute;"},{0x00DE,"&THORN;"},{0x00DF,"&szlig;"},` |
|      - | 6050 | `	{0x00E0,"&agrave;"},{0x00E1,"&aacute;"},{0x00E2,"&acirc;"},{0x00E3,"&atilde;"},` |
|      - | 6051 | `	{0x00E4,"&auml;"},{0x00E5,"&aring;"},{0x00E6,"&aelig;"},{0x00E7,"&ccedil;"},` |
|      - | 6052 | `	{0x00E8,"&egrave;"},{0x00E9,"&eacute;"},{0x00EA,"&ecirc;"},{0x00EB,"&euml;"},` |
|      - | 6053 | `	{0x00EC,"&igrave;"},{0x00ED,"&iacute;"},{0x00EE,"&icirc;"},{0x00EF,"&iuml;"},` |
|      - | 6054 | `	{0x00F0,"&eth;"},{0x00F1,"&ntilde;"},{0x00F2,"&ograve;"},{0x00F3,"&oacute;"},` |
|      - | 6055 | `	{0x00F4,"&ocirc;"},{0x00F5,"&otilde;"},{0x00F6,"&ouml;"},{0x00F7,"&divide;"},` |
|      - | 6056 | `	{0x00F8,"&oslash;"},{0x00F9,"&ugrave;"},{0x00FA,"&uacute;"},{0x00FB,"&ucirc;"},` |
|      - | 6057 | `	{0x00FC,"&uuml;"},{0x00FD,"&yacute;"},{0x00FE,"&thorn;"},{0x00FF,"&yuml;"},` |
|      - | 6058 | `	{0x0152,"&OElig;"},{0x0153,"&oelig;"},{0x0160,"&Scaron;"},{0x0161,"&scaron;"},` |
|      - | 6059 | `	{0x0178,"&Yuml;"},{0x0192,"&fnof;"},{0x02C6,"&circ;"},{0x02DC,"&tilde;"},` |
|      - | 6060 | `	{0x0391,"&Alpha;"},{0x0392,"&Beta;"},{0x0393,"&Gamma;"},{0x0394,"&Delta;"},` |
|      - | 6061 | `	{0x0395,"&Epsilon;"},{0x0396,"&Zeta;"},{0x0397,"&Eta;"},{0x0398,"&Theta;"},` |
|      - | 6062 | `	{0x0399,"&Iota;"},{0x039A,"&Kappa;"},{0x039B,"&Lambda;"},{0x039C,"&Mu;"},` |
|      - | 6063 | `	{0x039D,"&Nu;"},{0x039E,"&Xi;"},{0x039F,"&Omicron;"},{0x03A0,"&Pi;"},` |
|      - | 6064 | `	{0x03A1,"&Rho;"},{0x03A3,"&Sigma;"},{0x03A4,"&Tau;"},{0x03A5,"&Upsilon;"},` |
|      - | 6065 | `	{0x03A6,"&Phi;"},{0x03A7,"&Chi;"},{0x03A8,"&Psi;"},{0x03A9,"&Omega;"},` |
|      - | 6066 | `	{0x03B1,"&alpha;"},{0x03B2,"&beta;"},{0x03B3,"&gamma;"},{0x03B4,"&delta;"},` |
|      - | 6067 | `	{0x03B5,"&epsilon;"},{0x03B6,"&zeta;"},{0x03B7,"&eta;"},{0x03B8,"&theta;"},` |
|      - | 6068 | `	{0x03B9,"&iota;"},{0x03BA,"&kappa;"},{0x03BB,"&lambda;"},{0x03BC,"&mu;"},` |
|      - | 6069 | `	{0x03BD,"&nu;"},{0x03BE,"&xi;"},{0x03BF,"&omicron;"},{0x03C0,"&pi;"},` |
|      - | 6070 | `	{0x03C1,"&rho;"},{0x03C2,"&sigmaf;"},{0x03C3,"&sigma;"},{0x03C4,"&tau;"},` |
|      - | 6071 | `	{0x03C5,"&upsilon;"},{0x03C6,"&phi;"},{0x03C7,"&chi;"},{0x03C8,"&psi;"},` |
|      - | 6072 | `	{0x03C9,"&omega;"},{0x03D1,"&thetasym;"},{0x03D2,"&upsih;"},{0x03D6,"&piv;"},` |
|      - | 6073 | `	{0x2002,"&ensp;"},{0x2003,"&emsp;"},{0x2009,"&thinsp;"},{0x200C,"&zwnj;"},` |
|      - | 6074 | `	{0x200D,"&zwj;"},{0x200E,"&lrm;"},{0x200F,"&rlm;"},{0x2013,"&ndash;"},` |
|      - | 6075 | `	{0x2014,"&mdash;"},{0x2018,"&lsquo;"},{0x2019,"&rsquo;"},{0x201A,"&sbquo;"},` |
|      - | 6076 | `	{0x201C,"&ldquo;"},{0x201D,"&rdquo;"},{0x201E,"&bdquo;"},{0x2020,"&dagger;"},` |
|      - | 6077 | `	{0x2021,"&Dagger;"},{0x2022,"&bull;"},{0x2026,"&hellip;"},{0x2030,"&permil;"},` |
|      - | 6078 | `	{0x2032,"&prime;"},{0x2033,"&Prime;"},{0x2039,"&lsaquo;"},{0x203A,"&rsaquo;"},` |
|      - | 6079 | `	{0x203E,"&oline;"},{0x2044,"&frasl;"},{0x20AC,"&euro;"},{0x2111,"&image;"},` |
|      - | 6080 | `	{0x2118,"&weierp;"},{0x211C,"&real;"},{0x2122,"&trade;"},{0x2135,"&alefsym;"},` |
|      - | 6081 | `	{0x2190,"&larr;"},{0x2191,"&uarr;"},{0x2192,"&rarr;"},{0x2193,"&darr;"},` |
|      - | 6082 | `	{0x2194,"&harr;"},{0x21B5,"&crarr;"},{0x21D0,"&lArr;"},{0x21D1,"&uArr;"},` |
|      - | 6083 | `	{0x21D2,"&rArr;"},{0x21D3,"&dArr;"},{0x21D4,"&hArr;"},{0x2200,"&forall;"},` |
|      - | 6084 | `	{0x2202,"&part;"},{0x2203,"&exist;"},{0x2205,"&empty;"},{0x2207,"&nabla;"},` |
|      - | 6085 | `	{0x2208,"&isin;"},{0x2209,"&notin;"},{0x220B,"&ni;"},{0x220F,"&prod;"},` |
|      - | 6086 | `	{0x2211,"&sum;"},{0x2212,"&minus;"},{0x2217,"&lowast;"},{0x221A,"&radic;"},` |
|      - | 6087 | `	{0x221D,"&prop;"},{0x221E,"&infin;"},{0x2220,"&ang;"},{0x2227,"&and;"},` |
|      - | 6088 | `	{0x2228,"&or;"},{0x2229,"&cap;"},{0x222A,"&cup;"},{0x222B,"&int;"},` |
|      - | 6089 | `	{0x2234,"&there4;"},{0x223C,"&sim;"},{0x2245,"&cong;"},{0x2248,"&asymp;"},` |
|      - | 6090 | `	{0x2260,"&ne;"},{0x2261,"&equiv;"},{0x2264,"&le;"},{0x2265,"&ge;"},` |
|      - | 6091 | `	{0x2282,"&sub;"},{0x2283,"&sup;"},{0x2284,"&nsub;"},{0x2286,"&sube;"},` |
|      - | 6092 | `	{0x2287,"&supe;"},{0x2295,"&oplus;"},{0x2297,"&otimes;"},{0x22A5,"&perp;"},` |
|      - | 6093 | `	{0x22C5,"&sdot;"},{0x2308,"&lceil;"},{0x2309,"&rceil;"},{0x230A,"&lfloor;"},` |
|      - | 6094 | `	{0x230B,"&rfloor;"},{0x2329,"&lang;"},{0x232A,"&rang;"},{0x25CA,"&loz;"},` |
|      - | 6095 | `	{0x2660,"&spades;"},{0x2663,"&clubs;"},{0x2665,"&hearts;"},{0x2666,"&diams;"}` |
|      - | 6096 | `};` |
|      - | 6097 | `/* Binary-search aHtml401Ent[] for cp; return its "&name;" entity or 0. */` |
|     41 | 6098 | `static const char *FvHtml401Lookup(sxu32 cp){` |
|     41 | 6099 | `	int lo = 0, hi = (int)SX_ARRAYSIZE(aHtml401Ent) - 1;` |
|    323 | 6100 | `	while( lo <= hi ){` |
|    309 | 6101 | `		int mid = (lo + hi) / 2;` |
|    309 | 6102 | `		sxu32 c = aHtml401Ent[mid].cp;` |
|    309 | 6103 | `		if( c == cp ){ return aHtml401Ent[mid].zEnt; }` |
|    283 | 6104 | `		if( c < cp ){ lo = mid + 1; } else { hi = mid - 1; }` |
|      1 | 6105 | `	}` |
|     15 | 6106 | `	return 0;` |
|     21 | 6107 | `}` |
|      - | 6108 | `/* Decode one strict-UTF-8 sequence at p (< zEnd). On success returns its byte` |
|      - | 6109 | ` * length (1..4) and sets *pCp to the codepoint; on any malformed, overlong,` |
|      - | 6110 | ` * surrogate, truncated or out-of-range (>U+10FFFF) sequence returns 0. Matches` |
|      - | 6111 | ` * PHP's UTF-8 validation used by FULL_SPECIAL_CHARS (verified vs php 8.5.7). */` |
|    101 | 6112 | `static int FvUtf8Next(const unsigned char *p,const unsigned char *zEnd,sxu32 *pCp){` |
|    101 | 6113 | `	unsigned char c = p[0];` |
|    101 | 6114 | `	if( c < 0x80 ){ *pCp = c; return 1; }` |
|    101 | 6115 | `	if( c < 0xC2 ){ return 0; }              /* 0x80-0xBF stray cont / 0xC0-0xC1 overlong */` |
|     99 | 6116 | `	if( c < 0xE0 ){                          /* 2-byte: U+0080..U+07FF */` |
|     47 | 6117 | `		if( zEnd-p < 2 \|\| (p[1]&0xC0)!=0x80 ){ return 0; }` |
|     45 | 6118 | `		*pCp = ((sxu32)(c&0x1F)<<6) \| (p[1]&0x3F);` |
|     45 | 6119 | `		return 2;` |
|      - | 6120 | `	}` |
|     53 | 6121 | `	if( c < 0xF0 ){                          /* 3-byte: U+0800..U+FFFF minus surrogates */` |
|      - | 6122 | `		sxu32 cp;` |
|     47 | 6123 | `		if( zEnd-p < 3 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 ){ return 0; }` |
|     33 | 6124 | `		cp = ((sxu32)(c&0x0F)<<12) \| ((sxu32)(p[1]&0x3F)<<6) \| (p[2]&0x3F);` |
|     33 | 6125 | `		if( cp < 0x800 \|\| (cp>=0xD800 && cp<=0xDFFF) ){ return 0; }` |
|     29 | 6126 | `		*pCp = cp;` |
|     29 | 6127 | `		return 3;` |
|      - | 6128 | `	}` |
|      7 | 6129 | `	if( c < 0xF5 ){                          /* 4-byte: U+10000..U+10FFFF */` |
|      - | 6130 | `		sxu32 cp;` |
|      5 | 6131 | `		if( zEnd-p < 4 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 \|\| (p[3]&0xC0)!=0x80 ){ return 0; }` |
|      5 | 6132 | `		cp = ((sxu32)(c&0x07)<<18) \| ((sxu32)(p[1]&0x3F)<<12) \| ((sxu32)(p[2]&0x3F)<<6) \| (p[3]&0x3F);` |
|      5 | 6133 | `		if( cp < 0x10000 \|\| cp > 0x10FFFF ){ return 0; }` |
|      5 | 6134 | `		*pCp = cp;` |
|      5 | 6135 | `		return 4;` |
|      - | 6136 | `	}` |
|      3 | 6137 | `	return 0;                                /* 0xF5-0xFF */` |
|     51 | 6138 | `}` |
|      - | 6139 | `/* FILTER_SANITIZE_FULL_SPECIAL_CHARS: htmlentities-style, UTF-8-aware. Encodes` |
|      - | 6140 | ` * <>&"' as named entities ("'" -> &#039;; quotes suppressed under NO_ENCODE_QUOTES),` |
|      - | 6141 | ` * and every valid UTF-8 codepoint with an HTML 4.01 named entity as that entity;` |
|      - | 6142 | ` * valid codepoints without a named entity (and low control bytes) pass through` |
|      - | 6143 | ` * verbatim. If the input contains ANY invalid UTF-8 the whole result is "".` |
|      - | 6144 | ` * The STRIP/ENCODE flags do NOT apply to this filter (only NO_ENCODE_QUOTES).` |
|      - | 6145 | ` * php's filter does NOT re-encode valid pre-existing entities ("&amp;" stays,` |
|      - | 6146 | ` * "&bogus;" becomes "&amp;bogus;"), i.e. double_encode=false semantics —` |
|      - | 6147 | ` * exactly htmlentities(ENT_QUOTES\|ENT_HTML401, double_encode: false), so this` |
|      - | 6148 | ` * delegates to the shared encoder. Byte-exact vs php 8.5.7. */` |
|     25 | 6149 | `static void FvSanitizeFull(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 6150 | `	int iEntFlags = (flags & FV_FLAG_NO_ENCODE_QUOTES) ? 0 : PH7_ENT_QUOTES;` |
|     25 | 6151 | `	HtmlEscape(pCtx,z,n,iEntFlags,1/*bAll*/,0/*bDoubleEncode*/);` |
|     25 | 6152 | `}` |
|      - | 6153 | `/* ---------------------------------------------------------------------------` |
|      - | 6154 | ` * UTF-8-aware HTML entity core (htmlspecialchars/htmlentities family).` |
|      - | 6155 | ` * Prototyped next to the five builtins earlier in this file; lives here so it` |
|      - | 6156 | ` * can share aHtml401Ent[]/FvHtml401Lookup()/FvUtf8Next() with the filter_var` |
|      - | 6157 | ` * FULL_SPECIAL_CHARS filter above. Byte-exact vs php 8.5.7 (oracle-swept).` |
|      - | 6158 | ` * ------------------------------------------------------------------------ */` |
|      - | 6159 | `/* Encode cp as UTF-8 into zBuf (>= 4 bytes); return the byte length 1..4.` |
|      - | 6160 | ` * Thin wrapper over the engine-wide SX_WRITE_UTF8 (sxmacros.h). */` |
|    585 | 6161 | `static int HtmlCpUtf8(sxu32 cp,char *zBuf){` |
|    585 | 6162 | `	sxu8 *z = (sxu8 *)zBuf;` |
|    585 | 6163 | `	SX_WRITE_UTF8(z,cp);` |
|    585 | 6164 | `	return (int)(z - (sxu8 *)zBuf);` |
|      1 | 6165 | `}` |
|      - | 6166 | `/* Doctype-allowed codepoint test (php's unicode_cp_is_allowed) — gates what a` |
|      - | 6167 | ` * numeric reference may DECODE to. Oracle-pinned per doctype: HTML401` |
|      - | 6168 | ` * disallows C0 (except TAB/LF/CR) and DEL..U+009F; XML1 and XHTML share the` |
|      - | 6169 | ` * XML rules — DEL..U+009F allowed, U+FFFE/U+FFFF excluded; HTML5 swaps CR` |
|      - | 6170 | ` * for FF (0x0C) and excludes the noncharacters (U+FDD0..U+FDEF and every` |
|      - | 6171 | ` * U+xFFFE/U+xFFFF). Surrogates are disallowed everywhere. */` |
|     91 | 6172 | `static int HtmlCpAllowed(sxu32 cp,int iFlags){` |
|     91 | 6173 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|     91 | 6174 | `	if( cp==0x09 \|\| cp==0x0A ){ return 1; }` |
|     87 | 6175 | `	if( cp==0x0D ){ return iDoc != PH7_ENT_DOC_HTML5; }` |
|     85 | 6176 | `	if( cp==0x0C ){ return iDoc == PH7_ENT_DOC_HTML5; }` |
|     85 | 6177 | `	if( cp < 0x20 \|\| cp > 0x10FFFF ){ return 0; }` |
|     79 | 6178 | `	if( cp>=0xD800 && cp<=0xDFFF ){ return 0; }` |
|     77 | 6179 | `	if( cp>=0x7F && cp<=0x9F ){ return iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML; }` |
|     71 | 6180 | `	if( iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML ){` |
|    ! 0 | 6181 | `		return cp!=0xFFFE && cp!=0xFFFF;` |
|      - | 6182 | `	}` |
|     71 | 6183 | `	if( iDoc == PH7_ENT_DOC_HTML5 ){` |
|      9 | 6184 | `		if( cp>=0xFDD0 && cp<=0xFDEF ){ return 0; }` |
|      9 | 6185 | `		if( (cp & 0xFFFF) >= 0xFFFE ){ return 0; }` |
|      4 | 6186 | `	}` |
|     71 | 6187 | `	return 1;` |
|     46 | 6188 | `}` |
|      - | 6189 | `/* The ENT_DISALLOWED gate for RAW characters on the ENCODE side. Same as the` |
|      - | 6190 | ` * decode gate except CR under HTML5: php's encode-side unicode_cp_is_allowed` |
|      - | 6191 | ` * keeps a literal "\r" verbatim under ENT_HTML5\|ENT_DISALLOWED while the` |
|      - | 6192 | ` * decode side leaves "&#13;" un-decoded (oracle-pinned at flags 176). */` |
|      9 | 6193 | `static int HtmlCpAllowedEncode(sxu32 cp,int iFlags){` |
|      9 | 6194 | `	if( cp==0x0D && (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 ){ return 1; }` |
|      9 | 6195 | `	return HtmlCpAllowed(cp,iFlags);` |
|      5 | 6196 | `}` |
|      - | 6197 | `/* Numeric-reference validity for the double_encode=false "is this already a` |
|      - | 6198 | ` * valid entity" test — a MUCH looser predicate than the decode gate above:` |
|      - | 6199 | ` * any codepoint <= U+10FFFF is valid (controls and surrogates included, every` |
|      - | 6200 | ` * doctype). ENT_DISALLOWED re-tightens non-HTML401 doctypes to the decode` |
|      - | 6201 | ` * gate, except that HTML5 exempts surrogates. All oracle-pinned: &#0; and` |
|      - | 6202 | ` * &#xD800; stay verbatim at flags 11 and 139; flags -1 (HTML5+DISALLOWED)` |
|      - | 6203 | ` * re-encodes &#0; and &#x10FFFF; but still keeps &#xD800;; flags 144` |
|      - | 6204 | ` * (XML1+DISALLOWED) re-encodes &#xD800;. */` |
|      9 | 6205 | `static int HtmlNumericAllowed(sxu32 cp,int iFlags){` |
|      9 | 6206 | `	if( cp > 0x10FFFF ){ return 0; }` |
|      7 | 6207 | `	if( (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML401 ){ return 1; /* never tightened */ }` |
|    ! 0 | 6208 | `	if( (iFlags & PH7_ENT_DISALLOWED)` |
|    ! 0 | 6209 | `	 && !((iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 && cp>=0xD800 && cp<=0xDFFF)` |
|    ! 0 | 6210 | `	 && !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|    ! 0 | 6211 | `	return 1;` |
|      5 | 6212 | `}` |
|      - | 6213 | `/* How many bytes the malformed UTF-8 sequence at p consumes — php's` |
|      - | 6214 | ` * get_next_char failure step (one U+FFFD substitution / one ENT_IGNORE drop` |
|      - | 6215 | ` * per MAXIMAL invalid subpart, not per byte): a prefix-valid sequence eats` |
|      - | 6216 | ` * its continuation bytes ("\xE0\x80\xAF" is ONE unit) while a byte that could` |
|      - | 6217 | ` * start a new sequence is left for the next round. */` |
|      5 | 6218 | `static int HtmlUtf8Trail(unsigned char c){ return c>=0x80 && c<=0xBF; }` |
|     11 | 6219 | `static int HtmlUtf8Lead(unsigned char c){ return c<0x80 \|\| (c>=0xC2 && c<=0xF4); }` |
|     15 | 6220 | `static int HtmlUtf8FailAdvance(const unsigned char *p,const unsigned char *zEnd){` |
|     15 | 6221 | `	unsigned char c = p[0];` |
|     15 | 6222 | `	int nAvail = (int)(zEnd - p);` |
|     15 | 6223 | `	if( c < 0xC2 \|\| c > 0xF4 ){ return 1; } /* stray trail / C0-C1 / F5-FF */` |
|     13 | 6224 | `	if( c < 0xE0 ){` |
|      3 | 6225 | `		if( nAvail < 2 ){ return 1; }` |
|      3 | 6226 | `		return HtmlUtf8Lead(p[1]) ? 1 : 2;` |
|      - | 6227 | `	}` |
|     11 | 6228 | `	if( c < 0xF0 ){` |
|     11 | 6229 | `		if( nAvail >= 3 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) ){` |
|      3 | 6230 | `			return 3; /* complete but overlong/surrogate */` |
|      - | 6231 | `		}` |
|      9 | 6232 | `		if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 6233 | `		if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 6234 | `		return 3;` |
|      - | 6235 | `	}` |
|    ! 0 | 6236 | `	if( nAvail >= 4 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) && HtmlUtf8Trail(p[3]) ){` |
|    ! 0 | 6237 | `		return 4; /* complete but overlong / > U+10FFFF */` |
|      - | 6238 | `	}` |
|    ! 0 | 6239 | `	if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 6240 | `	if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 6241 | `	if( nAvail < 4 \|\| HtmlUtf8Lead(p[3]) ){ return 3; }` |
|    ! 0 | 6242 | `	return 4;` |
|      8 | 6243 | `}` |
|      - | 6244 | `/* The basic special entities, shared by named matching, the hsc_decode` |
|      - | 6245 | ` * numeric whitelist and the translation-table builder so the sets can never` |
|      - | 6246 | ` * drift apart. (&apos; is not an HTML 4.01 entity — doctype-gated below.) */` |
|      - | 6247 | `static const struct { const char *zEnt; int n; sxu32 cp; } aHtmlSpecEnt[] = {` |
|      - | 6248 | `	{"&amp;",5,38},{"&lt;",4,60},{"&gt;",4,62},{"&quot;",6,34},{"&apos;",6,39}` |
|      - | 6249 | `};` |
|      - | 6250 | `/* Does this doctype consult the named-entity table (aHtml401Ent)? XML 1.0 has` |
|      - | 6251 | ` * no named entities beyond the specials; XHTML/HTML5 are approximated by the` |
|      - | 6252 | ` * HTML 4.01 table (documented divergence). */` |
|     63 | 6253 | `static int HtmlDocHasNamedTable(int iDoc){` |
|     63 | 6254 | `	return iDoc != PH7_ENT_DOC_XML1;` |
|      1 | 6255 | `}` |
|      - | 6256 | `/* The single-quote entity per doctype. Oracle-pinned asymmetry: for every` |
|      - | 6257 | ` * non-HTML401 doctype htmlspecialchars emits &apos; while htmlentities` |
|      - | 6258 | ` * (bEntities) keeps &#039; under XHTML too. The translation table mirrors` |
|      - | 6259 | ` * whichever function the requested table belongs to. */` |
|     29 | 6260 | `static const char *HtmlAposEntity(int iDoc,int bEntities){` |
|     29 | 6261 | `	if( iDoc == PH7_ENT_DOC_HTML401 \|\| (bEntities && iDoc == PH7_ENT_DOC_XHTML) ){` |
|     21 | 6262 | `		return "&#039;";` |
|      - | 6263 | `	}` |
|      9 | 6264 | `	return "&apos;";` |
|     15 | 6265 | `}` |
|      - | 6266 | `/* Try to parse one HTML entity at z (z[0]=='&', z < zEnd). bFull selects the` |
|      - | 6267 | ` * html_entity_decode set (doctype named table + any allowed numeric ref) vs` |
|      - | 6268 | ` * the htmlspecialchars_decode set (the basic specials + quote numerics only).` |
|      - | 6269 | ` * Named matching is case-SENSITIVE and the ';' is required (both PHP-exact);` |
|      - | 6270 | ` * numeric refs accept dec/hex (x or X) with any number of leading zeros but` |
|      - | 6271 | ` * reject out-of-range, surrogate and doctype-disallowed codepoints (the` |
|      - | 6272 | ` * caller then leaves the source verbatim). Quote-flag gating is NOT applied` |
|      - | 6273 | ` * here — the same routine doubles as the "is this a valid entity" test for` |
|      - | 6274 | ` * double_encode=false, which ignores the quote bits (oracle-pinned).` |
|      - | 6275 | ` * bEncodeCheck selects the looser HtmlNumericAllowed predicate used by that` |
|      - | 6276 | ` * double_encode test; decode callers pass 0 for the HtmlCpAllowed gate.` |
|      - | 6277 | ` * On success sets *pCp / *pnConsumed and returns 1. */` |
|    172 | 6278 | `static int HtmlParseEntity(const unsigned char *z,const unsigned char *zEnd,` |
|      1 | 6279 | `                           int iFlags,int bFull,int bEncodeCheck,sxu32 *pCp,int *pnConsumed){` |
|    173 | 6280 | `	int nAvail = (int)(zEnd - z);` |
|    173 | 6281 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6282 | `	sxu32 n;` |
|    173 | 6283 | `	if( nAvail < 4 ){ return 0; } /* shortest entities: &lt; &#9; */` |
|    169 | 6284 | `	if( z[1] == '#' ){` |
|      - | 6285 | `		/* Numeric reference */` |
|     89 | 6286 | `		sxu32 cp = 0;` |
|     89 | 6287 | `		int i = 2, bHex = 0, nDig = 0;` |
|     89 | 6288 | `		if( z[i]=='x' \|\| z[i]=='X' ){ bHex = 1; i++; }` |
|    317 | 6289 | `		for( ; i < nAvail && z[i] != ';' ; i++ ){` |
|      - | 6290 | `			int v;` |
|    221 | 6291 | `			unsigned char c = z[i];` |
|    221 | 6292 | `			if( c>='0' && c<='9' ){ v = c - '0'; }` |
|     17 | 6293 | `			else if( bHex && c>='a' && c<='f' ){ v = c - 'a' + 10; }` |
|     17 | 6294 | `			else if( bHex && c>='A' && c<='F' ){ v = c - 'A' + 10; }` |
|    ! 0 | 6295 | `			else { return 0; }` |
|      - | 6296 | `			/* Stop accumulating once out of range (keeps validating the shape;` |
|      - | 6297 | `			 * max intermediate is 0x10FFFF*16+15, no sxu32 overflow). */` |
|    221 | 6298 | `			if( cp <= 0x10FFFF ){ cp = cp * (bHex ? 16 : 10) + (sxu32)v; }` |
|    221 | 6299 | `			nDig++;` |
|    111 | 6300 | `		}` |
|     97 | 6301 | `		if( nDig == 0 \|\| i >= nAvail ){ return 0; } /* no digits / no ';' */` |
|     97 | 6302 | `		if( bEncodeCheck ? !HtmlNumericAllowed(cp,iFlags) : !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|     83 | 6303 | `		if( !bFull ){` |
|      - | 6304 | `			/* hsc_decode: numeric refs to the five specials only. */` |
|     99 | 6305 | `			for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) && aHtmlSpecEnt[n].cp != cp ; n++ ){}` |
|     25 | 6306 | `			if( n >= SX_ARRAYSIZE(aHtmlSpecEnt) ){ return 0; }` |
|     11 | 6307 | `		}` |
|     75 | 6308 | `		*pCp = cp;` |
|     75 | 6309 | `		*pnConsumed = i + 1;` |
|     75 | 6310 | `		return 1;` |
|      - | 6311 | `	}` |
|      - | 6312 | `	/* Named reference — every entity name starts with a letter, so anything` |
|      - | 6313 | `	 * else can bail out before touching the tables. */` |
|     81 | 6314 | `	if( !((z[1]>='a' && z[1]<='z') \|\| (z[1]>='A' && z[1]<='Z')) ){ return 0; }` |
|    287 | 6315 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) ; n++ ){` |
|    265 | 6316 | `		if( aHtmlSpecEnt[n].cp == 39 && iDoc == PH7_ENT_DOC_HTML401 ){ continue; }` |
|    243 | 6317 | `		if( nAvail >= aHtmlSpecEnt[n].n && SyMemcmp(z,aHtmlSpecEnt[n].zEnt,(sxu32)aHtmlSpecEnt[n].n) == 0 ){` |
|     53 | 6318 | `			*pCp = aHtmlSpecEnt[n].cp;` |
|     53 | 6319 | `			*pnConsumed = aHtmlSpecEnt[n].n;` |
|     53 | 6320 | `			return 1;` |
|      - | 6321 | `		}` |
|     96 | 6322 | `	}` |
|     23 | 6323 | `	if( bFull && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 6324 | `		/* Linear scan of the 248-row table: runs only at '&'-then-letter` |
|      - | 6325 | `		 * positions and guarantees the decode set can never drift from the` |
|      - | 6326 | `		 * encode table. The first-letter guard skips the SyStrlen/SyMemcmp` |
|      - | 6327 | `		 * for ~96% of rows. */` |
|   3369 | 6328 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|      - | 6329 | `			sxu32 nEnt;` |
|   3357 | 6330 | `			if( z[1] != (unsigned char)aHtml401Ent[n].zEnt[1] ){ continue; }` |
|    121 | 6331 | `			nEnt = SyStrlen(aHtml401Ent[n].zEnt);` |
|    121 | 6332 | `			if( (sxu32)nAvail >= nEnt && SyMemcmp(z,aHtml401Ent[n].zEnt,nEnt) == 0 ){` |
|      7 | 6333 | `				*pCp = aHtml401Ent[n].cp;` |
|      7 | 6334 | `				*pnConsumed = (int)nEnt;` |
|      7 | 6335 | `				return 1;` |
|      - | 6336 | `			}` |
|     58 | 6337 | `		}` |
|      6 | 6338 | `	}` |
|     17 | 6339 | `	return 0;` |
|     88 | 6340 | `}` |
|      - | 6341 | `/* Shared encoder for htmlspecialchars (bAll=0) and htmlentities (bAll=1).` |
|      - | 6342 | ` * Invalid UTF-8 policy: ENT_IGNORE drops the byte (and wins over SUBSTITUTE),` |
|      - | 6343 | ` * ENT_SUBSTITUTE emits one U+FFFD per invalid byte, neither -> the whole` |
|      - | 6344 | ` * result is "" (pre-validated in a first pass: the accumulating result API` |
|      - | 6345 | ` * cannot roll back — same reason FvSanitizeFull is two-pass). */` |
|     94 | 6346 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 6347 | `                       int iFlags,int bAll,int bDoubleEncode){` |
|     95 | 6348 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     95 | 6349 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|      - | 6350 | `	const unsigned char *runStart;` |
|     95 | 6351 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6352 | `	sxu32 cp;` |
|     95 | 6353 | `	if( (iFlags & (PH7_ENT_IGNORE\|PH7_ENT_SUBSTITUTE)) == 0 ){` |
|      - | 6354 | `		/* Pass 1: any malformed sequence rejects the entire input. ASCII` |
|      - | 6355 | `		 * bytes cannot be malformed, so skip them without the decoder. */` |
|    381 | 6356 | `		while( p < zEnd ){` |
|      - | 6357 | `			int len;` |
|    323 | 6358 | `			if( *p < 0x80 ){ p++; continue; }` |
|     37 | 6359 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     37 | 6360 | `			if( len == 0 ){ ph7_result_string(pCtx,"",0); return; }` |
|     27 | 6361 | `			p += len;` |
|      1 | 6362 | `		}` |
|     59 | 6363 | `		p = (const unsigned char *)zIn;` |
|     29 | 6364 | `	}` |
|     85 | 6365 | `	runStart = p;` |
|     85 | 6366 | `	ph7_result_string(pCtx,"",0);` |
|    455 | 6367 | `	while( p < zEnd ){` |
|    371 | 6368 | `		const char *zEnt = 0;` |
|      - | 6369 | `		int len;` |
|    371 | 6370 | `		if( *p < 0x80 ){` |
|    307 | 6371 | `			len = 1;` |
|    307 | 6372 | `			switch( *p ){` |
|     25 | 6373 | `			case '<': zEnt = "&lt;"; break;` |
|     25 | 6374 | `			case '>': zEnt = "&gt;"; break;` |
|     18 | 6375 | `			case '&':` |
|     37 | 6376 | `				zEnt = "&amp;";` |
|     37 | 6377 | `				if( !bDoubleEncode ){` |
|      - | 6378 | `					sxu32 eCp; int nEat;` |
|     25 | 6379 | `					if( HtmlParseEntity(p,zEnd,iFlags,1,1,&eCp,&nEat) ){` |
|      - | 6380 | `						/* A valid existing entity: keep it verbatim. */` |
|     13 | 6381 | `						zEnt = 0;` |
|     13 | 6382 | `						len = nEat;` |
|      6 | 6383 | `					}` |
|     12 | 6384 | `				}` |
|     37 | 6385 | `				break;` |
|     10 | 6386 | `			case '"':` |
|     21 | 6387 | `				if( iFlags & PH7_ENT_QUOTE_DOUBLE ){ zEnt = "&quot;"; }` |
|     21 | 6388 | `				break;` |
|     12 | 6389 | `			case '\'':` |
|     25 | 6390 | `				if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|     23 | 6391 | `					zEnt = HtmlAposEntity(iDoc,bAll);` |
|     11 | 6392 | `				}` |
|     25 | 6393 | `				break;` |
|     89 | 6394 | `			default:` |
|    179 | 6395 | `				if( (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode((sxu32)*p,iFlags) ){` |
|    ! 0 | 6396 | `					zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 6397 | `				}` |
|    178 | 6398 | `				break;` |
|      - | 6399 | `			}` |
|    154 | 6400 | `		}else{` |
|     65 | 6401 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     65 | 6402 | `			if( len == 0 ){` |
|      - | 6403 | `				/* Malformed subpart (IGNORE or SUBSTITUTE is set, else pass 1` |
|      - | 6404 | `				 * would have rejected): drop it or emit ONE U+FFFD for the` |
|      - | 6405 | `				 * whole unit (php substitutes per maximal invalid subpart). */` |
|     15 | 6406 | `				if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|     15 | 6407 | `				if( (iFlags & PH7_ENT_IGNORE) == 0 ){ ph7_result_string(pCtx,"\xEF\xBF\xBD",3); }` |
|     15 | 6408 | `				p += HtmlUtf8FailAdvance(p,zEnd);` |
|     15 | 6409 | `				runStart = p;` |
|     15 | 6410 | `				continue;` |
|      - | 6411 | `			}` |
|     51 | 6412 | `			if( bAll && HtmlDocHasNamedTable(iDoc) ){` |
|     41 | 6413 | `				zEnt = FvHtml401Lookup(cp);` |
|     20 | 6414 | `			}` |
|     51 | 6415 | `			if( zEnt == 0 && (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode(cp,iFlags) ){` |
|    ! 0 | 6416 | `				zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 6417 | `			}` |
|      - | 6418 | `		}` |
|    357 | 6419 | `		if( zEnt ){` |
|    135 | 6420 | `			if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|    135 | 6421 | `			ph7_result_string(pCtx,zEnt,-1);` |
|    135 | 6422 | `			runStart = p + len;` |
|     67 | 6423 | `		}` |
|    357 | 6424 | `		p += len;` |
|      1 | 6425 | `	}` |
|     85 | 6426 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     48 | 6427 | `}` |
|      - | 6428 | `/* Shared decoder for html_entity_decode (bFull=1) and htmlspecialchars_decode` |
|      - | 6429 | ` * (bFull=0). Quote refs (cp 34/39, named or numeric) are gated by the quote` |
|      - | 6430 | ` * bits and left verbatim when suppressed; an invalid entity leaves its '&'` |
|      - | 6431 | ` * verbatim and rescans right after it, which also yields PHP's no-double-` |
|      - | 6432 | ` * decode behavior ("&amp;lt;" -> "&lt;"). */` |
|     82 | 6433 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 6434 | `                         int iFlags,int bFull){` |
|     83 | 6435 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     83 | 6436 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|     83 | 6437 | `	const unsigned char *runStart = p;` |
|     83 | 6438 | `	ph7_result_string(pCtx,"",0);` |
|    557 | 6439 | `	while( p < zEnd ){` |
|      - | 6440 | `		sxu32 cp;` |
|      - | 6441 | `		int nEat;` |
|    510 | 6442 | `		if( *p != '&' ){ p++; continue; }` |
|    155 | 6443 | `		if( !HtmlParseEntity(p,zEnd,iFlags,bFull,0,&cp,&nEat) ){ p++; continue; }` |
|    124 | 6444 | `		if( (cp == 34 && (iFlags & PH7_ENT_QUOTE_DOUBLE) == 0)` |
|    117 | 6445 | `		 \|\| (cp == 39 && (iFlags & PH7_ENT_QUOTE_SINGLE) == 0) ){` |
|      - | 6446 | `			/* Suppressed quote: leave the entity source verbatim. */` |
|     37 | 6447 | `			p += nEat;` |
|     37 | 6448 | `			continue;` |
|      - | 6449 | `		}` |
|     89 | 6450 | `		if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|      - | 6451 | `		{` |
|      - | 6452 | `			char zBuf[4];` |
|     89 | 6453 | `			int n = HtmlCpUtf8(cp,zBuf);` |
|     89 | 6454 | `			ph7_result_string(pCtx,zBuf,n);` |
|      - | 6455 | `		}` |
|     89 | 6456 | `		p += nEat;` |
|     89 | 6457 | `		runStart = p;` |
|      1 | 6458 | `	}` |
|     79 | 6459 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     79 | 6460 | `}` |
|      - | 6461 | `/* Validate the optional charset argument at apArg[idx]: UTF-8 aliases (and` |
|      - | 6462 | ` * ""/NULL meaning the default) are accepted; anything else — including` |
|      - | 6463 | ` * php-supported single-byte charsets like ISO-8859-1, PHL is UTF-8-only by` |
|      - | 6464 | ` * policy — raises PHP's unsupported-charset warning and is treated as` |
|      - | 6465 | ` * UTF-8 (ph7_context_throw_error_format prepends the function name). */` |
|    141 | 6466 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx){` |
|      - | 6467 | `	const char *zCs;` |
|      - | 6468 | `	int nCs;` |
|    148 | 6469 | `	if( nArg <= idx \|\| ph7_value_is_null(apArg[idx]) ){ return; }` |
|     15 | 6470 | `	zCs = ph7_value_to_string(apArg[idx],&nCs);` |
|     15 | 6471 | `	if( nCs == 0 ){ return; } /* "" selects the default charset (UTF-8) */` |
|     13 | 6472 | `	if( nCs == 5 && SyStrnicmp(zCs,"UTF-8",5) == 0 ){` |
|     13 | 6473 | `		return; /* php accepts only "UTF-8" (any case) silently — "UTF8" warns */` |
|      - | 6474 | `	}` |
|    ! 0 | 6475 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 6476 | `		"Charset \"%.*s\" is not supported, assuming UTF-8",nCs,zCs);` |
|     71 | 6477 | `}` |
|      - | 6478 | `/* get_html_translation_table() worker: character (UTF-8 bytes) => entity.` |
|      - | 6479 | ` * The five specials come first in byte order, then — for HTML_ENTITIES with a` |
|      - | 6480 | ` * named-table doctype — the 248 aHtml401Ent rows ascending (oracle-pinned` |
|      - | 6481 | ` * ordering; 253 entries under the defaults). */` |
|    549 | 6482 | `static void HtmlTableAdd(ph7_value *pArray,ph7_value *pValue,const char *zKey,const char *zEnt){` |
|    549 | 6483 | `	ph7_value_string(pValue,zEnt,-1);` |
|    549 | 6484 | `	ph7_array_add_strkey_elem(pArray,zKey,pValue);` |
|    549 | 6485 | `	ph7_value_reset_string_cursor(pValue);` |
|    549 | 6486 | `}` |
|     13 | 6487 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags){` |
|      - | 6488 | `	ph7_value *pArray,*pValue;` |
|     13 | 6489 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6490 | `	sxu32 n;` |
|     13 | 6491 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 6492 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 6493 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|    ! 0 | 6494 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6495 | `		return;` |
|      - | 6496 | `	}` |
|     13 | 6497 | `	if( iFlags & PH7_ENT_QUOTE_DOUBLE ){` |
|     11 | 6498 | `		HtmlTableAdd(pArray,pValue,"\"","&quot;");` |
|      5 | 6499 | `	}` |
|     13 | 6500 | `	HtmlTableAdd(pArray,pValue,"&","&amp;");` |
|     13 | 6501 | `	if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|      - | 6502 | `		/* The apostrophe row mirrors the function each table belongs to:` |
|      - | 6503 | `		 * SPECIALCHARS follows htmlspecialchars, ENTITIES follows` |
|      - | 6504 | `		 * htmlentities (oracle-pinned at flags 35). */` |
|      7 | 6505 | `		HtmlTableAdd(pArray,pValue,"'",HtmlAposEntity(iDoc,iTable != 0));` |
|      3 | 6506 | `	}` |
|     13 | 6507 | `	HtmlTableAdd(pArray,pValue,"<","&lt;");` |
|     13 | 6508 | `	HtmlTableAdd(pArray,pValue,">","&gt;");` |
|     13 | 6509 | `	if( iTable != 0 /*php: any non-HTML_SPECIALCHARS table => entities*/ && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 6510 | `		char zKey[8];` |
|    499 | 6511 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|    497 | 6512 | `			int nK = HtmlCpUtf8(aHtml401Ent[n].cp,zKey);` |
|    497 | 6513 | `			zKey[nK] = 0;` |
|    497 | 6514 | `			HtmlTableAdd(pArray,pValue,zKey,aHtml401Ent[n].zEnt);` |
|    249 | 6515 | `		}` |
|      1 | 6516 | `	}` |
|     13 | 6517 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 6518 | `}` |
|     25 | 6519 | `static int FvEmailAllowed(unsigned char c){` |
|     25 | 6520 | `	if( (c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9') ){ return 1; }` |
|     16 | 6521 | `	return c=='!'\|\|c=='#'\|\|c=='$'\|\|c=='%'\|\|c=='&'\|\|c=='\''\|\|c=='*'\|\|c=='+'` |
|     10 | 6522 | ``	    \|\| c=='-'\|\|c=='='\|\|c=='?'\|\|c=='^'\|\|c=='_'\|\|c=='`'\|\|c=='{'\|\|c=='\|'`` |
|     15 | 6523 | `	    \|\| c=='}'\|\|c=='~'\|\|c=='@'\|\|c=='.'\|\|c=='['\|\|c==']';` |
|     13 | 6524 | `}` |
|     23 | 6525 | `static int FvUrlAllowed(unsigned char c){` |
|     23 | 6526 | `	return c>=33 && c<=126; /* PHP keeps every printable ASCII byte except space */` |
|      1 | 6527 | `}` |
|      - | 6528 | `/* SANITIZE_EMAIL (isUrl=0) / SANITIZE_URL (isUrl=1): strip disallowed bytes. */` |
|      5 | 6529 | `static void FvSanitizeChars(ph7_context *pCtx,const char *z,int n,int isUrl){` |
|      5 | 6530 | `	int i, runStart = 0;` |
|      5 | 6531 | `	ph7_result_string(pCtx,"",0);` |
|     51 | 6532 | `	for( i=0; i<n; i++ ){` |
|     47 | 6533 | `		unsigned char c = (unsigned char)z[i];` |
|     47 | 6534 | `		if( !(isUrl ? FvUrlAllowed(c) : FvEmailAllowed(c)) ){` |
|     11 | 6535 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     11 | 6536 | `			runStart = i+1;` |
|      5 | 6537 | `		}` |
|     24 | 6538 | `	}` |
|      5 | 6539 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      5 | 6540 | `}` |
|      - | 6541 | `/*` |
|      - | 6542 | ` * Apply the selected filter to one already-resolved input value and write the` |
|      - | 6543 | ` * result into pCtx. Shared by filter_var() and filter_input(): the caller has` |
|      - | 6544 | ` * already parsed $filter/$flags/$options. On validation failure the 'default'` |
|      - | 6545 | ` * option (if any) is returned, else null when FILTER_NULL_ON_FAILURE is set,` |
|      - | 6546 | ` * else false. A validating filter that passes returns the (string) input` |
|      - | 6547 | ` * unchanged; a sanitizer writes its transformed output directly.` |
|      - | 6548 | ` */` |
|    316 | 6549 | `static int FvApplyFilter(ph7_context *pCtx,ph7_value *pInput,` |
|      - | 6550 | `                         int iFilter,int iFlags,ph7_value *pOpts,` |
|      - | 6551 | `                         ph7_value *pDefault)` |
|      3 | 6552 | `{` |
|    319 | 6553 | `	int bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|      - | 6554 | `	const char *zVal; int nVal;` |
|      - | 6555 | `	/* An array/object input fails every scalar filter. */` |
|    319 | 6556 | `	if( ph7_value_is_array(pInput) ){ goto fail; }` |
|    317 | 6557 | `	zVal = ph7_value_to_string(pInput,&nVal);` |
|    317 | 6558 | `	switch( iFilter ){` |
|     28 | 6559 | `	case FV_VALIDATE_INT: {` |
|      - | 6560 | `		ph7_int64 v;` |
|     58 | 6561 | `		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }` |
|     31 | 6562 | `		if( pOpts ){` |
|      7 | 6563 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|      7 | 6564 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|      7 | 6565 | `			if( pMin && v<ph7_value_to_int64(pMin) ){ goto fail; }` |
|      7 | 6566 | `			if( pMax && v>ph7_value_to_int64(pMax) ){ goto fail; }` |
|      2 | 6567 | `		}` |
|     29 | 6568 | `		ph7_result_int64(pCtx,v);` |
|     29 | 6569 | `		return PH7_OK;` |
|      - | 6570 | `	}` |
|     34 | 6571 | `	case FV_VALIDATE_FLOAT: {` |
|      - | 6572 | `		double d;` |
|     69 | 6573 | `		if( !FvValidateFloat(zVal,nVal,iFlags,&d) ){ goto fail; }` |
|     39 | 6574 | `		ph7_result_double(pCtx,d);` |
|     39 | 6575 | `		return PH7_OK;` |
|      - | 6576 | `	}` |
|     14 | 6577 | `	case FV_VALIDATE_BOOLEAN: {` |
|      - | 6578 | `		int b;` |
|     29 | 6579 | `		if( !FvValidateBool(zVal,nVal,&b) ){ goto fail; }` |
|     21 | 6580 | `		ph7_result_bool(pCtx,b);` |
|     21 | 6581 | `		return PH7_OK;` |
|      - | 6582 | `	}` |
|     25 | 6583 | `	case FV_VALIDATE_IP:     if( !FvValidateIp(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|     11 | 6584 | `	case FV_VALIDATE_MAC:    if( !FvValidateMac(zVal,nVal) ){ goto fail; }       goto pass;` |
|     28 | 6585 | `	case FV_VALIDATE_EMAIL:  if( !FvValidateEmail(zVal,nVal) ){ goto fail; }     goto pass;` |
|     11 | 6586 | `	case FV_VALIDATE_DOMAIN: if( !FvValidateDomain(zVal,nVal) ){ goto fail; }    goto pass;` |
|     15 | 6587 | `	case FV_VALIDATE_URL:    if( !FvValidateUrl(zVal,nVal) ){ goto fail; }       goto pass;` |
|      3 | 6588 | `	case FV_VALIDATE_REGEXP: {` |
|      - | 6589 | `#ifdef PH7_ENABLE_PCRE` |
|      8 | 6590 | `		ph7_value *pRe = pOpts ? ph7_array_fetch(pOpts,"regexp",(int)sizeof("regexp")-1) : 0;` |
|      8 | 6591 | `		const char *zRe; int nRe, matched = 0;` |
|      8 | 6592 | `		if( pRe==0 ){` |
|      3 | 6593 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6594 | `				"filter_var(): \"regexp\" option is missing");` |
|      - | 6595 | `		}` |
|      5 | 6596 | `		zRe = ph7_value_to_string(pRe,&nRe);` |
|      5 | 6597 | `		if( PH7_PcreMatchQuiet(pCtx,zRe,nRe,zVal,nVal,&matched)!=SXRET_OK \|\| !matched ){ goto fail; }` |
|      3 | 6598 | `		goto pass;` |
|      - | 6599 | `#else` |
|      - | 6600 | `		goto fail;` |
|      - | 6601 | `#endif` |
|      - | 6602 | `	}` |
|      3 | 6603 | `	case FV_SANITIZE_NUMBER_INT:   FvSanitizeNumber(pCtx,zVal,nVal,0,0);      return PH7_OK;` |
|      5 | 6604 | `	case FV_SANITIZE_NUMBER_FLOAT: FvSanitizeNumber(pCtx,zVal,nVal,1,iFlags); return PH7_OK;` |
|     13 | 6605 | `	case FV_SANITIZE_SPECIAL_CHARS:      FvSanitizeSpecial(pCtx,zVal,nVal,iFlags); return PH7_OK;` |
|     25 | 6606 | `	case FV_SANITIZE_FULL_SPECIAL_CHARS: FvSanitizeFull(pCtx,zVal,nVal,iFlags);    return PH7_OK;` |
|      3 | 6607 | `	case FV_SANITIZE_EMAIL: FvSanitizeChars(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 6608 | `	case FV_SANITIZE_URL:   FvSanitizeChars(pCtx,zVal,nVal,1); return PH7_OK;` |
|     13 | 6609 | `	case FV_DEFAULT:` |
|      - | 6610 | `		/* FILTER_UNSAFE_RAW / FILTER_DEFAULT: pass through unchanged unless a` |
|      - | 6611 | `		 * STRIP/ENCODE flag is set, in which case apply the string filter. */` |
|     28 | 6612 | `		if( iFlags & FV_FLAG_STRING_MASK ){` |
|     15 | 6613 | `			FvSanitizeString(pCtx,zVal,nVal,iFlags);` |
|     15 | 6614 | `			return PH7_OK;` |
|      - | 6615 | `		}` |
|     14 | 6616 | `		goto pass;` |
|    ! 0 | 6617 | `	default:` |
|    ! 0 | 6618 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 6619 | `			"Unknown filter with ID %d",iFilter);` |
|    ! 0 | 6620 | `		break; /* unknown filter id -> fail */` |
|    ! 0 | 6621 | `	}` |
|     58 | 6622 | `fail:` |
|    118 | 6623 | `	if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|    114 | 6624 | `	else if( bNull ){ ph7_result_null(pCtx); }` |
|    108 | 6625 | `	else { ph7_result_bool(pCtx,0); }` |
|    118 | 6626 | `	return PH7_OK;` |
|     26 | 6627 | `pass: /* validation passed: return the (string) input unchanged */` |
|     54 | 6628 | `	ph7_result_string(pCtx,zVal,nVal);` |
|     54 | 6629 | `	return PH7_OK;` |
|    161 | 6630 | `}` |
|      - | 6631 | `/*` |
|      - | 6632 | ` * Parse the ($filter, $options) pair shared by filter_var()/filter_input() out` |
|      - | 6633 | ` * of apArg[iBase] ($filter) and apArg[iBase+1] ($options): $options is either a` |
|      - | 6634 | ` * plain flags int, or an array with 'flags' and an 'options' sub-array (whose` |
|      - | 6635 | ` * 'default' entry is the fallback value). Fills the four output pointers;` |
|      - | 6636 | ` * unset outputs keep the caller-provided defaults.` |
|      - | 6637 | ` */` |
|    328 | 6638 | `static void FvParseFilterArgs(int nArg,ph7_value **apArg,int iBase,` |
|      - | 6639 | `                              int *piFilter,int *piFlags,` |
|      - | 6640 | `                              ph7_value **ppOpts,ph7_value **ppDefault)` |
|      3 | 6641 | `{` |
|    331 | 6642 | `	if( nArg>iBase ){ *piFilter = ph7_value_to_int(apArg[iBase]); }` |
|    331 | 6643 | `	if( nArg>iBase+1 ){` |
|     88 | 6644 | `		if( ph7_value_is_array(apArg[iBase+1]) ){` |
|     42 | 6645 | `			ph7_value *pF = ph7_array_fetch(apArg[iBase+1],"flags",(int)sizeof("flags")-1);` |
|     42 | 6646 | `			if( pF ){ *piFlags = ph7_value_to_int(pF); }` |
|     42 | 6647 | `			*ppOpts = ph7_array_fetch(apArg[iBase+1],"options",(int)sizeof("options")-1);` |
|     42 | 6648 | `			if( *ppOpts && !ph7_value_is_array(*ppOpts) ){ *ppOpts = 0; }` |
|     42 | 6649 | `			if( *ppOpts ){ *ppDefault = ph7_array_fetch(*ppOpts,"default",(int)sizeof("default")-1); }` |
|     22 | 6650 | `		}else{` |
|     48 | 6651 | `			*piFlags = ph7_value_to_int(apArg[iBase+1]);` |
|      - | 6652 | `		}` |
|     43 | 6653 | `	}` |
|    331 | 6654 | `}` |
|      - | 6655 | `/*` |
|      - | 6656 | ` * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 6657 | ` *  Validate or sanitize a value; see FvApplyFilter for the failure semantics.` |
|      - | 6658 | ` */` |
|    306 | 6659 | `static int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6660 | `{` |
|    308 | 6661 | `	int iFilter = FV_DEFAULT, iFlags = 0;` |
|    308 | 6662 | `	ph7_value *pOpts = 0, *pDefault = 0;` |
|    308 | 6663 | `	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    308 | 6664 | `	FvParseFilterArgs(nArg,apArg,1,&iFilter,&iFlags,&pOpts,&pDefault);` |
|    308 | 6665 | `	return FvApplyFilter(pCtx,apArg[0],iFilter,iFlags,pOpts,pDefault);` |
|    155 | 6666 | `}` |
|      - | 6667 | `/*` |
|      - | 6668 | ` * filter_input($type, $var_name, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 6669 | ` *  Look up $var_name in the requested INPUT_* superglobal, then apply the` |
|      - | 6670 | ` *  filter. Semantics verified byte-for-byte against php 8.5:` |
|      - | 6671 | ` *   - variable NOT set: 'default' option wins, else false when` |
|      - | 6672 | ` *     FILTER_NULL_ON_FAILURE is set, else null. (Note the null/false roles are` |
|      - | 6673 | ` *     INVERTED relative to a present value that fails validation, which yields` |
|      - | 6674 | ` *     default > null-if-NULL_ON_FAILURE > false via FvApplyFilter.)` |
|      - | 6675 | ` *   - variable present: delegate to FvApplyFilter.` |
|      - | 6676 | ` *  Divergence: php reads a SAPI snapshot of the original request variables` |
|      - | 6677 | ` *  captured at startup; PHL reads the live superglobal. In CLI they match for` |
|      - | 6678 | ` *  the SAPI-registered keys (SCRIPT_NAME/PHP_SELF/DOCUMENT_ROOT); keys added` |
|      - | 6679 | ` *  only to the live $_SERVER (REQUEST_TIME/PWD/…) are visible here but not in` |
|      - | 6680 | ` *  php's snapshot.` |
|      - | 6681 | ` */` |
|     28 | 6682 | `static int PH7_builtin_filter_input(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6683 | `{` |
|     30 | 6684 | `	int iType, iFilter = FV_DEFAULT, iFlags = 0;` |
|     30 | 6685 | `	ph7_value *pOpts = 0, *pDefault = 0, *pSuper, *pElem;` |
|      - | 6686 | `	const char *zVar, *zSuper; int nVar; sxu32 nSuper;` |
|     30 | 6687 | `	if( nArg<2 ){` |
|      7 | 6688 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|      2 | 6689 | `			"filter_input() expects at least 2 arguments, %d given",nArg);` |
|      - | 6690 | `	}` |
|     26 | 6691 | `	iType = ph7_value_to_int(apArg[0]);` |
|     26 | 6692 | `	switch( iType ){` |
|      3 | 6693 | `	case 0: zSuper = "_POST";   nSuper = (sxu32)sizeof("_POST")-1;   break; /* INPUT_POST */` |
|      3 | 6694 | `	case 1: zSuper = "_GET";    nSuper = (sxu32)sizeof("_GET")-1;    break; /* INPUT_GET */` |
|    ! 0 | 6695 | `	case 2: zSuper = "_COOKIE"; nSuper = (sxu32)sizeof("_COOKIE")-1; break; /* INPUT_COOKIE */` |
|    ! 0 | 6696 | `	case 4: zSuper = "_ENV";    nSuper = (sxu32)sizeof("_ENV")-1;    break; /* INPUT_ENV */` |
|     19 | 6697 | `	case 5: zSuper = "_SERVER"; nSuper = (sxu32)sizeof("_SERVER")-1; break; /* INPUT_SERVER */` |
|      1 | 6698 | `	default:` |
|      3 | 6699 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6700 | `			"filter_input(): Argument #1 ($type) must be an INPUT_* constant");` |
|      - | 6701 | `	}` |
|     23 | 6702 | `	zVar = ph7_value_to_string(apArg[1],&nVar);` |
|     23 | 6703 | `	FvParseFilterArgs(nArg,apArg,2,&iFilter,&iFlags,&pOpts,&pDefault);` |
|      - | 6704 | `	/* Resolve the variable from the superglobal (missing/non-array -> not set). */` |
|     23 | 6705 | `	pSuper = PH7_VmExtractSuper(pCtx->pVm,zSuper,nSuper);` |
|     23 | 6706 | `	pElem = (pSuper && ph7_value_is_array(pSuper))` |
|     33 | 6707 | `		? ph7_array_fetch(pSuper,zVar,nVar) : 0;` |
|     23 | 6708 | `	if( pElem==0 ){` |
|      - | 6709 | `		/* Variable not set: default > false(if NULL_ON_FAILURE) > null. Note the` |
|      - | 6710 | `		 * false/null roles are inverted vs FvApplyFilter's present-but-fails path. */` |
|     13 | 6711 | `		if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|      9 | 6712 | `		else if( iFlags & FV_NULL_ON_FAILURE ){ ph7_result_bool(pCtx,0); }` |
|      7 | 6713 | `		else { ph7_result_null(pCtx); }` |
|     13 | 6714 | `		return PH7_OK;` |
|      - | 6715 | `	}` |
|     11 | 6716 | `	return FvApplyFilter(pCtx,pElem,iFilter,iFlags,pOpts,pDefault);` |
|     16 | 6717 | `}` |
|      - | 6718 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6719 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6720 | `/*` |
|      - | 6721 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 6722 |  |
|      - | 6723 | ` */` |
|      4 | 6724 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 6725 | `	const char *zInput, /* Raw input */` |
|      - | 6726 | `	int nByte,  /* Input length */` |
|      - | 6727 | `	int delim,  /* Delimiter */` |
|      - | 6728 | `	int encl,   /* Enclosure */` |
|      - | 6729 | `	int escape,  /* Escape character */` |
|      - | 6730 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 6731 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 6732 | `	)` |
|      1 | 6733 | `{` |
|      5 | 6734 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 6735 | `	const char *zIn = zInput;` |
|      - | 6736 | `	const char *zPtr;` |
|      - | 6737 | `	int isEnc;` |
|      - | 6738 | `	/* Start processing */` |
|      8 | 6739 | `	for(;;){` |
|     17 | 6740 | `		if( zIn >= zEnd ){` |
|      - | 6741 | `			/* No more input to process */` |
|      5 | 6742 | `			break;` |
|      - | 6743 | `		}` |
|     13 | 6744 | `		isEnc = 0;` |
|     13 | 6745 | `		zPtr = zIn;` |
|      - | 6746 | `		/* Find the first delimiter */` |
|     27 | 6747 | `		while( zIn < zEnd ){` |
|     23 | 6748 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 6749 | `				/* Delimiter found,break imediately */` |
|      5 | 6750 | `				break;` |
|     15 | 6751 | `			}else if( zIn[0] == encl ){` |
|      - | 6752 | `				/* Inside enclosure? */` |
|    ! 0 | 6753 | `				isEnc = !isEnc;` |
|     15 | 6754 | `			}else if( zIn[0] == escape ){` |
|      - | 6755 | `				/* Escape sequence */` |
|    ! 0 | 6756 | `				zIn++;` |
|    ! 0 | 6757 | `			}` |
|      - | 6758 | `			/* Advance the cursor */` |
|     15 | 6759 | `			zIn++;` |
|      1 | 6760 | `		}` |
|     13 | 6761 | `		if( zIn > zPtr ){` |
|     13 | 6762 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 6763 | `			sxi32 rc;` |
|      - | 6764 | `			/* Invoke the supllied callback */` |
|     13 | 6765 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 6766 | `				zPtr++;` |
|    ! 0 | 6767 | `				nByteChunk-=2;` |
|    ! 0 | 6768 | `			}` |
|     13 | 6769 | `			if( nByteChunk > 0 ){` |
|     13 | 6770 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 6771 | `				if( rc == SXERR_ABORT ){` |
|      - | 6772 | `					/* User callback request an operation abort */` |
|    ! 0 | 6773 | `					break;` |
|      - | 6774 | `				}` |
|      6 | 6775 | `			}` |
|      6 | 6776 | `		}` |
|      - | 6777 | `		/* Ignore trailing delimiter */` |
|     21 | 6778 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 6779 | `			zIn++;` |
|      1 | 6780 | `		}` |
|      1 | 6781 | `	}` |
|      5 | 6782 | `	return SXRET_OK;` |
|      1 | 6783 | `}` |
|      - | 6784 | `/*` |
|      - | 6785 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 6786 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 6787 | ` * argument to this callback.` |
|      - | 6788 | ` */` |
|     12 | 6789 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 6790 | `{` |
|     13 | 6791 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 6792 | `	ph7_value sEntry;` |
|      - | 6793 | `	SyString sToken;` |
|      - | 6794 | `	/* Insert the token in the given array */` |
|     13 | 6795 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 6796 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 6797 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 6798 | `	if( sToken.nByte < 1){` |
|    ! 0 | 6799 | `		return SXRET_OK;` |
|      - | 6800 | `	}` |
|     13 | 6801 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 6802 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 6803 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 6804 | `	return SXRET_OK;` |
|      7 | 6805 | `}` |
|      - | 6806 | `/*` |
|      - | 6807 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 6808 | ` *  Parse a CSV string into an array.` |
|      - | 6809 | ` * Parameters` |
|      - | 6810 | ` *  $input` |
|      - | 6811 | ` *   The string to parse.` |
|      - | 6812 | ` *  $delimiter` |
|      - | 6813 | ` *   Set the field delimiter (one character only).` |
|      - | 6814 | ` *  $enclosure` |
|      - | 6815 | ` *   Set the field enclosure character (one character only).` |
|      - | 6816 | ` *  $escape` |
|      - | 6817 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 6818 | ` * Return` |
|      - | 6819 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 6820 | ` */` |
|      2 | 6821 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6822 | `{` |
|      - | 6823 | `	const char *zInput,*zPtr;` |
|      - | 6824 | `	ph7_value *pArray;` |
|      3 | 6825 | `	int delim  = ',';   /* Delimiter */` |
|      3 | 6826 | `	int encl   = '"' ;  /* Enclosure */` |
|      3 | 6827 | `	int escape = '\\';  /* Escape character */` |
|      - | 6828 | `	int nLen;` |
|      3 | 6829 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6830 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 6831 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6832 | `		return PH7_OK;` |
|      - | 6833 | `	}` |
|      - | 6834 | `	/* Extract the raw input */` |
|      3 | 6835 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 6836 | `	if( nArg > 1 ){` |
|      - | 6837 | `		int i;` |
|      3 | 6838 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 6839 | `			/* Extract the delimiter */` |
|      3 | 6840 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 6841 | `			if( i > 0 ){` |
|      3 | 6842 | `				delim = zPtr[0];` |
|      1 | 6843 | `			}` |
|      1 | 6844 | `		}` |
|      3 | 6845 | `		if( nArg > 2 ){` |
|      3 | 6846 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 6847 | `				/* Extract the enclosure */` |
|      3 | 6848 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 6849 | `				if( i > 0 ){` |
|      3 | 6850 | `					encl = zPtr[0];` |
|      1 | 6851 | `				}` |
|      1 | 6852 | `			}` |
|      3 | 6853 | `			if( nArg > 3 ){` |
|      3 | 6854 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 6855 | `					/* Extract the escape character */` |
|      3 | 6856 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 6857 | `					if( i > 0 ){` |
|      3 | 6858 | `						escape = zPtr[0];` |
|      1 | 6859 | `					}` |
|      1 | 6860 | `				}` |
|      1 | 6861 | `			}` |
|      1 | 6862 | `		}` |
|      1 | 6863 | `	}` |
|      - | 6864 | `	/* Create our array */` |
|      3 | 6865 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 6866 | `	if( pArray == 0 ){` |
|      - | 6867 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|    ! 0 | 6868 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6869 | `	}` |
|      - | 6870 | `	/* Parse the raw input */` |
|      3 | 6871 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 6872 | `	/* Return the freshly created array */` |
|      3 | 6873 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 6874 | `	return PH7_OK;` |
|      2 | 6875 | `}` |
|      - | 6876 | `/*` |
|      - | 6877 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 6878 | ` * container.` |
|      - | 6879 | ` * Refer to [strip_tags()].` |
|      - | 6880 | ` */` |
|     10 | 6881 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 6882 | `{` |
|     11 | 6883 | `	const char *zEnd = &zTag[nByte];` |
|      - | 6884 | `	const char *zPtr;` |
|      - | 6885 | `	SyString sEntry;` |
|      - | 6886 | `	/* Strip tags */` |
|     10 | 6887 | `	for(;;){` |
|     45 | 6888 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 6889 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 6890 | `				zTag++;` |
|      1 | 6891 | `		}` |
|     21 | 6892 | `		if( zTag >= zEnd ){` |
|     11 | 6893 | `			break;` |
|      - | 6894 | `		}` |
|     11 | 6895 | `		zPtr = zTag;` |
|      - | 6896 | `		/* Delimit the tag */` |
|     25 | 6897 | `		while(zTag < zEnd ){` |
|     25 | 6898 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 6899 | `				/* UTF-8 stream */` |
|      3 | 6900 | `				zTag++;` |
|      5 | 6901 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 6902 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 6903 | `				break;` |
|    ! 0 | 6904 | `			}else{` |
|     13 | 6905 | `				zTag++;` |
|      - | 6906 | `			}` |
|      1 | 6907 | `		}` |
|     11 | 6908 | `		if( zTag > zPtr ){` |
|      - | 6909 | `			/* Perform the insertion */` |
|     11 | 6910 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 6911 | `			SyStringFullTrim(&sEntry);` |
|     11 | 6912 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 6913 | `		}` |
|      - | 6914 | `		/* Jump the trailing '>' */` |
|     11 | 6915 | `		zTag++;` |
|      1 | 6916 | `	}` |
|     11 | 6917 | `	return SXRET_OK;` |
|      1 | 6918 | `}` |
|      - | 6919 | `/*` |
|      - | 6920 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 6921 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 6922 | ` * Refer to [strip_tags()].` |
|      - | 6923 | ` */` |
|     36 | 6924 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 6925 | `{` |
|     37 | 6926 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 6927 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 6928 | `		SyString sTag;` |
|     85 | 6929 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 6930 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 6931 | `			zTag++;` |
|      1 | 6932 | `		}` |
|      - | 6933 | `		/* Delimit the tag */` |
|     25 | 6934 | `		zCur = zTag;` |
|     77 | 6935 | `		while(zTag < zEnd ){` |
|     77 | 6936 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 6937 | `				/* UTF-8 stream */` |
|      5 | 6938 | `				zTag++;` |
|      9 | 6939 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 6940 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 6941 | `				break;` |
|    ! 0 | 6942 | `			}else{` |
|     49 | 6943 | `				zTag++;` |
|      - | 6944 | `			}` |
|      1 | 6945 | `		}` |
|     25 | 6946 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 6947 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 6948 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 6949 | `		if( sTag.nByte > 0 ){` |
|      - | 6950 | `			SyString *aEntry,*pEntry;` |
|      - | 6951 | `			sxi32 rc;` |
|      - | 6952 | `			sxu32 n;` |
|      - | 6953 | `			/* Perform the lookup */` |
|     25 | 6954 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 6955 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 6956 | `				pEntry = &aEntry[n];` |
|      - | 6957 | `				/* Do the comparison */` |
|     25 | 6958 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 6959 | `				if( !rc ){` |
|     21 | 6960 | `					return SXRET_OK;` |
|      - | 6961 | `				}` |
|      3 | 6962 | `			}` |
|      2 | 6963 | `		}` |
|      2 | 6964 | `	}` |
|      - | 6965 | `	/* No such tag */` |
|     17 | 6966 | `	return SXERR_NOTFOUND;` |
|     19 | 6967 | `}` |
|      - | 6968 | `/*` |
|      - | 6969 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 6970 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 6971 | ` * Refer to [strip_tags()].` |
|      - | 6972 | ` */` |
|     16 | 6973 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 6974 | `{` |
|     17 | 6975 | `	const char *zEnd = &zIn[nByte];` |
|      - | 6976 | `	const char *zPtr,*zTag;` |
|      - | 6977 | `	SySet sSet;` |
|      - | 6978 | `	/* initialize the set of allowed tags */` |
|     17 | 6979 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 6980 | `	if( nTaglen > 0 ){` |
|      - | 6981 | `		/* Set of allowed tags */` |
|     11 | 6982 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 6983 | `	}` |
|      - | 6984 | `	/* Set the empty string */` |
|     17 | 6985 | `	ph7_result_string(pCtx,"",0);` |
|      - | 6986 | `	/* Start processing */` |
|     26 | 6987 | `	for(;;){` |
|     53 | 6988 | `		if(zIn >= zEnd){` |
|      - | 6989 | `			/* No more input to process */` |
|     15 | 6990 | `			break;` |
|      - | 6991 | `		}` |
|     39 | 6992 | `		zPtr = zIn;` |
|      - | 6993 | `		/* Find a tag */` |
|    133 | 6994 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 6995 | `			zIn++;` |
|      1 | 6996 | `		}` |
|     39 | 6997 | `		if( zIn > zPtr ){` |
|      - | 6998 | `			/* Consume raw input */` |
|     21 | 6999 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 7000 | `		}` |
|      - | 7001 | `		/* Ignore trailing null bytes */` |
|     39 | 7002 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 7003 | `			zIn++;` |
|    ! 0 | 7004 | `		}` |
|     39 | 7005 | `		if(zIn >= zEnd){` |
|      - | 7006 | `			/* No more input to process */` |
|      3 | 7007 | `			break;` |
|      - | 7008 | `		}` |
|     37 | 7009 | `		if( zIn[0] == '<' ){` |
|      - | 7010 | `			sxi32 rc;` |
|     37 | 7011 | `			zTag = zIn++;` |
|      - | 7012 | `			/* Delimit the tag */` |
|    127 | 7013 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 7014 | `				zIn++;` |
|      1 | 7015 | `			}` |
|     37 | 7016 | `			if( zIn < zEnd ){` |
|     37 | 7017 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 7018 | `			}` |
|      - | 7019 | `			/* Query the set */` |
|     37 | 7020 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 7021 | `			if( rc == SXRET_OK ){` |
|      - | 7022 | `				/* Keep the tag */` |
|     21 | 7023 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 7024 | `			}` |
|     18 | 7025 | `		}` |
|      1 | 7026 | `	}` |
|      - | 7027 | `	/* Cleanup */` |
|     17 | 7028 | `	SySetRelease(&sSet);` |
|     17 | 7029 | `	return SXRET_OK;` |
|      1 | 7030 | `}` |
|      - | 7031 | `/*` |
|      - | 7032 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 7033 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 7034 | ` * Parameters` |
|      - | 7035 | ` *  $str` |
|      - | 7036 | ` *  The input string.` |
|      - | 7037 | ` * $allowable_tags` |
|      - | 7038 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 7039 | ` * Return` |
|      - | 7040 | ` *  Returns the stripped string.` |
|      - | 7041 | ` */` |
|     14 | 7042 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7043 | `{` |
|     15 | 7044 | `	const char *zTaglist = 0;` |
|      - | 7045 | `	const char *zString;` |
|     15 | 7046 | `	int nTaglen = 0;` |
|      - | 7047 | `	int nLen;` |
|     15 | 7048 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7049 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 7050 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7051 | `		return PH7_OK;` |
|      - | 7052 | `	}` |
|      - | 7053 | `	/* Point to the raw string */` |
|     15 | 7054 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 7055 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 7056 | `		/* Allowed tag */` |
|     11 | 7057 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 7058 | `	}` |
|      - | 7059 | `	/* Process input */` |
|     15 | 7060 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 7061 | `	return PH7_OK;` |
|      8 | 7062 | `}` |
|      - | 7063 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7064 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7065 | `/*` |
|      - | 7066 | ` * string str_shuffle(string $str)` |
|      - | 7067 |  |
|      - | 7068 | ` *  Randomly shuffles a string.` |
|      - | 7069 | ` * Parameters` |
|      - | 7070 | ` *  $str` |
|      - | 7071 | ` *   The input string.` |
|      - | 7072 | ` * Return` |
|      - | 7073 | ` *  Returns the shuffled string.` |
|      - | 7074 | ` */` |
|     10 | 7075 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7076 | `{` |
|      - | 7077 | `	const char *zString;` |
|      - | 7078 | `	int nLen,i,c;` |
|      - | 7079 | `	sxu32 iR;` |
|     11 | 7080 | `	if( nArg < 1 ){` |
|      - | 7081 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7082 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7083 | `		return PH7_OK;` |
|      - | 7084 | `	}` |
|      - | 7085 | `	/* Extract the target string */` |
|     11 | 7086 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 7087 | `	if( nLen < 1 ){` |
|      - | 7088 | `		/* Nothing to shuffle */` |
|      3 | 7089 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 7090 | `		return PH7_OK;` |
|      - | 7091 | `	}` |
|      - | 7092 | `	/* Shuffle the string */` |
|     43 | 7093 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 7094 | `		/* Generate a random number first */` |
|     35 | 7095 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 7096 | `		/* Extract a random offset */` |
|     35 | 7097 | `		c = zString[iR % nLen];` |
|      - | 7098 | `		/* Append it */` |
|     35 | 7099 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 7100 | `	}` |
|      9 | 7101 | `	return PH7_OK;` |
|      6 | 7102 | `}` |
|      - | 7103 | `/*` |
|      - | 7104 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 7105 | ` *  Convert a string to an array.` |
|      - | 7106 | ` * Parameters` |
|      - | 7107 | ` * $string` |
|      - | 7108 | ` *  The input string.` |
|      - | 7109 | ` * $split_length` |
|      - | 7110 | ` *  Maximum length of the chunk.` |
|      - | 7111 | ` * Return` |
|      - | 7112 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 7113 | ` *  except possibly the last one which may be shorter.` |
|      - | 7114 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 7115 | ` *  as the first (and only) array element.` |
|      - | 7116 | ` *  An empty string returns an empty array.` |
|      - | 7117 | ` * Errors` |
|      - | 7118 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 7119 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 7120 | ` *  ValueError if $split_length is less than 1.` |
|      - | 7121 | ` */` |
|     28 | 7122 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7123 | `{` |
|      - | 7124 | `	const char *zString,*zEnd;` |
|      - | 7125 | `	ph7_value *pArray,*pValue;` |
|      - | 7126 | `	int split_len;` |
|      - | 7127 | `	int nLen;` |
|     33 | 7128 | `	if( nArg < 1 ){` |
|      4 | 7129 | `		return PH7_VmThrowException(pCtx,` |
|      - | 7130 | `			"ArgumentCountError",` |
|      - | 7131 | `			"str_split() expects at least 1 argument, %d given",` |
|      1 | 7132 | `			nArg` |
|      - | 7133 | `			);` |
|      - | 7134 | `	}` |
|      - | 7135 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     38 | 7136 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     40 | 7137 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 7138 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 7139 | `		return PH7_VmThrowException(pCtx,` |
|      - | 7140 | `			"TypeError",` |
|      - | 7141 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 7142 | `			ph7_type_name(apArg[0])` |
|      - | 7143 | `			);` |
|      - | 7144 | `	}` |
|      - | 7145 | `	/* Point to the target string */` |
|     27 | 7146 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 7147 | `	split_len = (int)sizeof(char);` |
|     27 | 7148 | `	if( nArg > 1 ){` |
|      - | 7149 | `		/* Split length */` |
|     17 | 7150 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 7151 | `		if( split_len < 1 ){` |
|      6 | 7152 | `			return PH7_VmThrowException(pCtx,` |
|      - | 7153 | `				"ValueError",` |
|      - | 7154 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 7155 | `				);` |
|      - | 7156 | `		}` |
|     11 | 7157 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 7158 | `			split_len = nLen;` |
|      1 | 7159 | `		}` |
|      5 | 7160 | `	}` |
|      - | 7161 | `	/* Create the array and the scalar value */` |
|     21 | 7162 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 7163 | `	/*Chunk value */` |
|     21 | 7164 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 7165 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 7166 | `		/* Return FALSE */` |
|    ! 0 | 7167 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7168 | `		return PH7_OK;` |
|      - | 7169 | `	}` |
|      - | 7170 | `	/* Point to the end of the string */` |
|     21 | 7171 | `	zEnd = &zString[nLen];` |
|      - | 7172 | `	/* Perform the requested operation */` |
|     48 | 7173 | `	for(;;){` |
|      - | 7174 | `		int nMax;` |
|     59 | 7175 | `		if( zString >= zEnd ){` |
|      - | 7176 | `			/* No more input to process */` |
|     21 | 7177 | `			break;` |
|      - | 7178 | `		}` |
|     39 | 7179 | `		nMax = (int)(zEnd-zString);` |
|     39 | 7180 | `		if( nMax < split_len ){` |
|      3 | 7181 | `			split_len = nMax;` |
|      1 | 7182 | `		}` |
|      - | 7183 | `		/* Copy the current chunk */` |
|     39 | 7184 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 7185 | `		/* Insert it */` |
|     39 | 7186 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|    ! 0 | 7187 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 7188 | `		}` |
|      - | 7189 | `		/* reset the string cursor */` |
|     39 | 7190 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 7191 | `		/* Update position */` |
|     39 | 7192 | `		zString += split_len;` |
|      1 | 7193 | `	}` |
|      - | 7194 | `	/*` |
|      - | 7195 | `	 * Return the array.` |
|      - | 7196 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 7197 | `	 * upon we return from this function.` |
|      - | 7198 | `	 */` |
|     21 | 7199 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 7200 | `	return PH7_OK;` |
|     19 | 7201 | `}` |
|      - | 7202 | `/*` |
|      - | 7203 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 7204 | ` * Refer to [strspn()].` |
|      - | 7205 | ` */` |
|     28 | 7206 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 7207 | `{` |
|     29 | 7208 | `	const char *zIn = *pzIn;` |
|      - | 7209 | `	const char *zPtr;` |
|      - | 7210 | `	/* Ignore leading white spaces */` |
|     29 | 7211 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 7212 | `		zIn++;` |
|    ! 0 | 7213 | `	}` |
|     29 | 7214 | `	if( zIn >= zEnd ){` |
|      - | 7215 | `		/* End of input */` |
|    ! 0 | 7216 | `		return SXERR_EOF;` |
|      - | 7217 | `	}` |
|     29 | 7218 | `	zPtr = zIn;` |
|      - | 7219 | `	/* Extract the token */` |
|    201 | 7220 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 7221 | `		zIn++;` |
|      1 | 7222 | `	}` |
|     29 | 7223 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 7224 | `	/* Synchronize pointers */` |
|     29 | 7225 | `	*pzIn = zIn;` |
|      - | 7226 | `	/* Return to the caller */` |
|     29 | 7227 | `	return SXRET_OK;` |
|     15 | 7228 | `}` |
|      - | 7229 | `/*` |
|      - | 7230 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 7231 | ` * return the longest match.` |
|      - | 7232 | ` * Refer to [strspn()].` |
|      - | 7233 | ` */` |
|     18 | 7234 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 7235 | `{` |
|     19 | 7236 | `	const char *zEnd = &zString[nLen];` |
|     19 | 7237 | `	const char *zIn = zString;` |
|      - | 7238 | `	int i,c;` |
|     45 | 7239 | `	for(;;){` |
|     91 | 7240 | `		if( zString >= zEnd ){` |
|      7 | 7241 | `			break;` |
|      - | 7242 | `		}` |
|      - | 7243 | `		/* Extract current character */` |
|     85 | 7244 | `		c = zString[0];` |
|      - | 7245 | `		/* Perform the lookup */` |
|    383 | 7246 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 7247 | `			if( c == zMask[i] ){` |
|      - | 7248 | `				/* Character found */` |
|     73 | 7249 | `				break;` |
|      - | 7250 | `			}` |
|    150 | 7251 | `		}` |
|     85 | 7252 | `		if( i >= nMaskLen ){` |
|      - | 7253 | `			/* Character not in the current mask,break immediately */` |
|     13 | 7254 | `			break;` |
|      - | 7255 | `		}` |
|      - | 7256 | `		/* Advance cursor */` |
|     73 | 7257 | `		zString++;` |
|      1 | 7258 | `	}` |
|      - | 7259 | `	/* Longest match */` |
|     19 | 7260 | `	return (int)(zString-zIn);` |
|      1 | 7261 | `}` |
|      - | 7262 | `/*` |
|      - | 7263 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 7264 | ` * Refer to [strcspn()].` |
|      - | 7265 | ` */` |
|     10 | 7266 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 7267 | `{` |
|     11 | 7268 | `	const char *zEnd = &zString[nLen];` |
|     11 | 7269 | `	const char *zIn = zString;` |
|      - | 7270 | `	int i,c;` |
|     12 | 7271 | `	for(;;){` |
|     25 | 7272 | `		if( zString >= zEnd ){` |
|      3 | 7273 | `			break;` |
|      - | 7274 | `		}` |
|      - | 7275 | `		/* Extract current character */` |
|     23 | 7276 | `		c = zString[0];` |
|      - | 7277 | `		/* Perform the lookup */` |
|     51 | 7278 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 7279 | `			if( c == zMask[i] ){` |
|      9 | 7280 | `				break;` |
|      - | 7281 | `			}` |
|     15 | 7282 | `		}` |
|     23 | 7283 | `		if( i < nMaskLen ){` |
|      - | 7284 | `			/* Character in the current mask,break immediately */` |
|      9 | 7285 | `			break;` |
|      - | 7286 | `		}` |
|      - | 7287 | `		/* Advance cursor */` |
|     15 | 7288 | `		zString++;` |
|      1 | 7289 | `	}` |
|      - | 7290 | `	/* Longest match */` |
|     11 | 7291 | `	return (int)(zString-zIn);` |
|      1 | 7292 | `}` |
|      - | 7293 | `/*` |
|      - | 7294 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 7295 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 7296 | ` *  of characters contained within a given mask.` |
|      - | 7297 | ` * Parameters` |
|      - | 7298 | ` * $str` |
|      - | 7299 | ` *  The input string.` |
|      - | 7300 | ` * $mask` |
|      - | 7301 | ` *  The list of allowable characters.` |
|      - | 7302 | ` * $start` |
|      - | 7303 | ` *  The position in subject to start searching.` |
|      - | 7304 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 7305 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 7306 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 7307 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 7308 | ` *  start'th position from the end of subject.` |
|      - | 7309 | ` * $length` |
|      - | 7310 | ` *  The length of the segment from subject to examine.` |
|      - | 7311 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 7312 | ` *  characters after the starting position.` |
|      - | 7313 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 7314 | ` *  position up to length characters from the end of subject.` |
|      - | 7315 | ` * Return` |
|      - | 7316 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 7317 | ` * in mask.` |
|      - | 7318 | ` */` |
|     24 | 7319 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7320 | `{` |
|      - | 7321 | `	const char *zString,*zMask,*zEnd;` |
|      - | 7322 | `	int iMasklen,iLen;` |
|      - | 7323 | `	SyString sToken;` |
|     25 | 7324 | `	int iCount = 0;` |
|      - | 7325 | `	int rc;` |
|     25 | 7326 | `	if( nArg < 2 ){` |
|      - | 7327 | `		/* Missing agruments,return zero */` |
|    ! 0 | 7328 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7329 | `		return PH7_OK;` |
|      - | 7330 | `	}` |
|      - | 7331 | `	/* Extract the target string */` |
|     25 | 7332 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7333 | `	/* Extract the mask */` |
|     25 | 7334 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 7335 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 7336 | `		/* Nothing to process,return zero */` |
|      7 | 7337 | `		ph7_result_int(pCtx,0);` |
|      7 | 7338 | `		return PH7_OK;` |
|      - | 7339 | `	}` |
|     19 | 7340 | `	if( nArg > 2 ){` |
|      - | 7341 | `		int nOfft;` |
|      - | 7342 | `		/* Extract the offset */` |
|      9 | 7343 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 7344 | `		if( nOfft < 0 ){` |
|    ! 0 | 7345 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 7346 | `			if( zBase > zString ){` |
|    ! 0 | 7347 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 7348 | `				zString = zBase;` |
|    ! 0 | 7349 | `			}else{` |
|      - | 7350 | `				/* Invalid offset */` |
|    ! 0 | 7351 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7352 | `				return PH7_OK;` |
|      - | 7353 | `			}` |
|    ! 0 | 7354 | `		}else{` |
|      9 | 7355 | `			if( nOfft >= iLen ){` |
|      - | 7356 | `				/* Invalid offset */` |
|    ! 0 | 7357 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7358 | `				return PH7_OK;` |
|    ! 0 | 7359 | `			}else{` |
|      - | 7360 | `				/* Update offset */` |
|      9 | 7361 | `				zString += nOfft;` |
|      9 | 7362 | `				iLen -= nOfft;` |
|      - | 7363 | `			}` |
|      - | 7364 | `		}` |
|      9 | 7365 | `		if( nArg > 3 ){` |
|      - | 7366 | `			int iUserlen;` |
|      - | 7367 | `			/* Extract the desired length */` |
|      9 | 7368 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 7369 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 7370 | `				iLen = iUserlen;` |
|      2 | 7371 | `			}` |
|      4 | 7372 | `		}` |
|      4 | 7373 | `	}` |
|      - | 7374 | `	/* Point to the end of the string */` |
|     19 | 7375 | `	zEnd = &zString[iLen];` |
|      - | 7376 | `	/* Extract the first non-space token */` |
|     19 | 7377 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 7378 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 7379 | `		/* Compare against the current mask */` |
|     19 | 7380 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 7381 | `	}` |
|      - | 7382 | `	/* Longest match */` |
|     19 | 7383 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 7384 | `	return PH7_OK;` |
|     13 | 7385 | `}` |
|      - | 7386 | `/*` |
|      - | 7387 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 7388 | ` *  Find length of initial segment not matching mask.` |
|      - | 7389 | ` * Parameters` |
|      - | 7390 | ` * $str` |
|      - | 7391 | ` *  The input string.` |
|      - | 7392 | ` * $mask` |
|      - | 7393 | ` *  The list of not allowed characters.` |
|      - | 7394 | ` * $start` |
|      - | 7395 | ` *  The position in subject to start searching.` |
|      - | 7396 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 7397 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 7398 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 7399 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 7400 | ` *  start'th position from the end of subject.` |
|      - | 7401 | ` * $length` |
|      - | 7402 | ` *  The length of the segment from subject to examine.` |
|      - | 7403 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 7404 | ` *  characters after the starting position.` |
|      - | 7405 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 7406 | ` *  position up to length characters from the end of subject.` |
|      - | 7407 | ` * Return` |
|      - | 7408 | ` *  Returns the length of the segment as an integer.` |
|      - | 7409 | ` */` |
|     14 | 7410 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7411 | `{` |
|      - | 7412 | `	const char *zString,*zMask,*zEnd;` |
|      - | 7413 | `	int iMasklen,iLen;` |
|      - | 7414 | `	SyString sToken;` |
|     15 | 7415 | `	int iCount = 0;` |
|      - | 7416 | `	int rc;` |
|     15 | 7417 | `	if( nArg < 2 ){` |
|      - | 7418 | `		/* Missing agruments,return zero */` |
|    ! 0 | 7419 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7420 | `		return PH7_OK;` |
|      - | 7421 | `	}` |
|      - | 7422 | `	/* Extract the target string */` |
|     15 | 7423 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7424 | `	/* Extract the mask */` |
|     15 | 7425 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 7426 | `	if( iLen < 1 ){` |
|      - | 7427 | `		/* Nothing to process,return zero */` |
|    ! 0 | 7428 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7429 | `		return PH7_OK;` |
|      - | 7430 | `	}` |
|     15 | 7431 | `	if( iMasklen < 1 ){` |
|      - | 7432 | `		/* No given mask,return the string length */` |
|      3 | 7433 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 7434 | `		return PH7_OK;` |
|      - | 7435 | `	}` |
|     13 | 7436 | `	if( nArg > 2 ){` |
|      - | 7437 | `		int nOfft;` |
|      - | 7438 | `		/* Extract the offset */` |
|     11 | 7439 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 7440 | `		if( nOfft < 0 ){` |
|    ! 0 | 7441 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 7442 | `			if( zBase > zString ){` |
|    ! 0 | 7443 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 7444 | `				zString = zBase;` |
|    ! 0 | 7445 | `			}else{` |
|      - | 7446 | `				/* Invalid offset */` |
|    ! 0 | 7447 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7448 | `				return PH7_OK;` |
|      - | 7449 | `			}` |
|    ! 0 | 7450 | `		}else{` |
|     11 | 7451 | `			if( nOfft >= iLen ){` |
|      - | 7452 | `				/* Invalid offset */` |
|      3 | 7453 | `				ph7_result_int(pCtx,0);` |
|      3 | 7454 | `				return PH7_OK;` |
|    ! 0 | 7455 | `			}else{` |
|      - | 7456 | `				/* Update offset */` |
|      9 | 7457 | `				zString += nOfft;` |
|      9 | 7458 | `				iLen -= nOfft;` |
|      - | 7459 | `			}` |
|      - | 7460 | `		}` |
|      9 | 7461 | `		if( nArg > 3 ){` |
|      - | 7462 | `			int iUserlen;` |
|      - | 7463 | `			/* Extract the desired length */` |
|    ! 0 | 7464 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 7465 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 7466 | `				iLen = iUserlen;` |
|    ! 0 | 7467 | `			}` |
|    ! 0 | 7468 | `		}` |
|      4 | 7469 | `	}` |
|      - | 7470 | `	/* Point to the end of the string */` |
|     11 | 7471 | `	zEnd = &zString[iLen];` |
|      - | 7472 | `	/* Extract the first non-space token */` |
|     11 | 7473 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 7474 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 7475 | `		/* Compare against the current mask */` |
|     11 | 7476 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 7477 | `	}` |
|      - | 7478 | `	/* Longest match */` |
|     11 | 7479 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 7480 | `	return PH7_OK;` |
|      8 | 7481 | `}` |
|      - | 7482 | `/*` |
|      - | 7483 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 7484 | ` *  Search a string for any of a set of characters.` |
|      - | 7485 | ` * Parameters` |
|      - | 7486 | ` *  $haystack` |
|      - | 7487 | ` *   The string where char_list is looked for.` |
|      - | 7488 | ` *  $char_list` |
|      - | 7489 | ` *   This parameter is case sensitive.` |
|      - | 7490 | ` * Return` |
|      - | 7491 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 7492 | ` */` |
|      4 | 7493 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7494 | `{` |
|      - | 7495 | `	const char *zString,*zList,*zEnd;` |
|      - | 7496 | `	int iLen,iListLen,i,c;` |
|      - | 7497 | `	sxu32 nOfft,nMax;` |
|      - | 7498 | `	sxi32 rc;` |
|      5 | 7499 | `	if( nArg < 2 ){` |
|      - | 7500 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7501 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7502 | `		return PH7_OK;` |
|      - | 7503 | `	}` |
|      - | 7504 | `	/* Extract the haystack and the char list */` |
|      5 | 7505 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 7506 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 7507 | `	if( iLen < 1 ){` |
|      - | 7508 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7509 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7510 | `		return PH7_OK;` |
|      - | 7511 | `	}` |
|      - | 7512 | `	/* Point to the end of the string */` |
|      5 | 7513 | `	zEnd = &zString[iLen];` |
|      5 | 7514 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 7515 | `	/* perform the requested operation */` |
|     15 | 7516 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 7517 | `		c = zList[i];` |
|     11 | 7518 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 7519 | `		if( rc == SXRET_OK ){` |
|      5 | 7520 | `			if( nMax < nOfft ){` |
|      3 | 7521 | `				nOfft = nMax;` |
|      1 | 7522 | `			}` |
|      2 | 7523 | `		}` |
|      6 | 7524 | `	}` |
|      5 | 7525 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 7526 | `		/* No such substring,return FALSE */` |
|      3 | 7527 | `		ph7_result_bool(pCtx,0);` |
|      2 | 7528 | `	}else{` |
|      - | 7529 | `		/* Return the substring */` |
|      3 | 7530 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 7531 | `	}` |
|      5 | 7532 | `	return PH7_OK;` |
|      3 | 7533 | `}` |
|      - | 7534 | `/* SPDX-SnippetBegin */` |
|      - | 7535 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 7536 | `/* SPDX-License-Identifier: blessing */` |
|      - | 7537 | `/*` |
|      - | 7538 | ` * string soundex(string $str)` |
|      - | 7539 | ` *  Calculate the soundex key of a string.` |
|      - | 7540 | ` * Parameters` |
|      - | 7541 | ` *  $str` |
|      - | 7542 | ` *   The input string.` |
|      - | 7543 | ` * Return` |
|      - | 7544 | ` *  Returns the soundex key as a string.` |
|      - | 7545 | ` * Note:` |
|      - | 7546 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 7547 | ` * source tree.` |
|      - | 7548 | ` */` |
|     22 | 7549 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7550 | `{` |
|      - | 7551 | `	const unsigned char *zIn;` |
|      - | 7552 | `	char zResult[8];` |
|      - | 7553 | `	int i, j;` |
|      - | 7554 | `	static const unsigned char iCode[] = {` |
|      - | 7555 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7556 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7557 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7558 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7559 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 7560 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 7561 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 7562 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 7563 | `	};` |
|     23 | 7564 | `	if( nArg < 1 ){` |
|      - | 7565 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7566 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7567 | `		return PH7_OK;` |
|      - | 7568 | `	}` |
|     23 | 7569 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     35 | 7570 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     23 | 7571 | `	if( zIn[i] ){` |
|     17 | 7572 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 7573 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 7574 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 7575 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 7576 | `			if( code>0 ){` |
|     45 | 7577 | `				if( code!=prevcode ){` |
|     33 | 7578 | `					prevcode = (unsigned char)code;` |
|     33 | 7579 | `					zResult[j++] = (char)code + '0';` |
|     16 | 7580 | `				}` |
|     23 | 7581 | `			}else{` |
|     49 | 7582 | `				prevcode = 0;` |
|      - | 7583 | `			}` |
|     47 | 7584 | `		}` |
|     33 | 7585 | `		while( j<4 ){` |
|     17 | 7586 | `			zResult[j++] = '0';` |
|      1 | 7587 | `		}` |
|     17 | 7588 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 7589 | `	}else{` |
|      - | 7590 | `	  /* No alphabetic character: PHP returns "0000" (not the SQLite "?000"). */` |
|      7 | 7591 | `	  ph7_result_string(pCtx,"0000",4);` |
|      - | 7592 | `	}` |
|     23 | 7593 | `	return PH7_OK;` |
|     12 | 7594 | `}` |
|      - | 7595 | `/* SPDX-SnippetEnd */` |
|      - | 7596 | `/*` |
|      - | 7597 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 7598 | ` *  Wraps a string to a given number of characters.` |
|      - | 7599 | ` * Parameters` |
|      - | 7600 | ` *  $str` |
|      - | 7601 | ` *   The input string.` |
|      - | 7602 | ` * $width` |
|      - | 7603 | ` *  The column width.` |
|      - | 7604 | ` * $break` |
|      - | 7605 | ` *  The line is broken using the optional break parameter.` |
|      - | 7606 | ` * Return` |
|      - | 7607 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 7608 | ` */` |
|     26 | 7609 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7610 | `{` |
|      - | 7611 | `	const char *zIn,*zBreak;` |
|      - | 7612 | `	SyBlob sWorker;` |
|      - | 7613 | `	int iLen,iBreaklen,iWidth,iCut,iStart,iSpace,iCur;` |
|      - | 7614 | `	sxi32 rc;` |
|     27 | 7615 | `	if( nArg < 1 ){` |
|      - | 7616 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7617 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7618 | `		return PH7_OK;` |
|      - | 7619 | `	}` |
|      - | 7620 | `	/* Extract the input string */` |
|     27 | 7621 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7622 | `	/* Width (default 75; PHP allows 0/negative — break at every space). */` |
|     27 | 7623 | `	iWidth = 75;` |
|     27 | 7624 | `	if( nArg > 1 ){` |
|     27 | 7625 | `		iWidth = ph7_value_to_int(apArg[1]);` |
|     13 | 7626 | `	}` |
|      - | 7627 | `	/* Break string (default "\n"). */` |
|     27 | 7628 | `	zBreak = "\n";` |
|     27 | 7629 | `	iBreaklen = (int)sizeof(char);` |
|     27 | 7630 | `	if( nArg > 2 ){` |
|     13 | 7631 | `		zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      6 | 7632 | `	}` |
|      - | 7633 | `	/* Cut long words? (default false). */` |
|     27 | 7634 | `	iCut = 0;` |
|     27 | 7635 | `	if( nArg > 3 ){` |
|      7 | 7636 | `		iCut = ph7_value_to_bool(apArg[3]);` |
|      3 | 7637 | `	}` |
|     27 | 7638 | `	if( iLen < 1 ){` |
|      - | 7639 | `		/* PHP returns the empty string for empty input before validating the other args. */` |
|      5 | 7640 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 7641 | `		return PH7_OK;` |
|      - | 7642 | `	}` |
|      - | 7643 | `	/* PHP 8 domain errors (catchable ValueError). */` |
|     23 | 7644 | `	if( iBreaklen < 1 ){` |
|      3 | 7645 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7646 | `			"wordwrap(): Argument #3 ($break) must not be empty");` |
|      - | 7647 | `	}` |
|     21 | 7648 | `	if( iWidth == 0 && iCut ){` |
|      3 | 7649 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7650 | `			"wordwrap(): Argument #4 ($cut_long_words) cannot be true when argument #2 ($width) is 0");` |
|      - | 7651 | `	}` |
|      - | 7652 | `	/*` |
|      - | 7653 | `	 * PHP's algorithm: a single left-to-right pass tracking the start of the` |
|      - | 7654 | `	 * current line (iStart) and the position of the last space seen on it` |
|      - | 7655 | `	 * (iSpace). A break is emitted when the line reaches the width, at the last` |
|      - | 7656 | `	 * space if there was one, otherwise (only when cut is enabled) hard at the` |
|      - | 7657 | `	 * boundary. An existing break sequence in the input resets the line.` |
|      - | 7658 | `	 */` |
|     19 | 7659 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     19 | 7660 | `	iStart = iSpace = iCur = 0;` |
|     19 | 7661 | `	rc = SXRET_OK;` |
|    551 | 7662 | `	while( iCur < iLen ){` |
|    533 | 7663 | `		if( iBreaklen <= iLen - iCur && SyMemcmp(&zIn[iCur],zBreak,(sxu32)iBreaklen) == 0 ){` |
|      - | 7664 | `			/* Existing break sequence in the input: copy it verbatim and reset the line. */` |
|    ! 0 | 7665 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart + iBreaklen));` |
|    ! 0 | 7666 | `			if( rc != SXRET_OK ){ goto oom; }` |
|    ! 0 | 7667 | `			iCur += iBreaklen;` |
|    ! 0 | 7668 | `			iStart = iSpace = iCur;` |
|    ! 0 | 7669 | `			continue;` |
|    533 | 7670 | `		}else if( zIn[iCur] == ' ' ){` |
|     67 | 7671 | `			if( iCur - iStart >= iWidth ){` |
|      - | 7672 | `				/* The line already fills the width at this space: break here (the space is consumed). */` |
|     13 | 7673 | `				rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     13 | 7674 | `				if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     13 | 7675 | `				if( rc != SXRET_OK ){ goto oom; }` |
|     13 | 7676 | `				iStart = iCur + 1;` |
|      6 | 7677 | `			}` |
|     67 | 7678 | `			iSpace = iCur;` |
|    500 | 7679 | `		}else if( iCut && iCur - iStart >= iWidth && iStart >= iSpace ){` |
|      - | 7680 | `			/* A word longer than the width with no space to break at: hard-cut at the boundary. */` |
|      7 | 7681 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|      7 | 7682 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|      7 | 7683 | `			if( rc != SXRET_OK ){ goto oom; }` |
|      7 | 7684 | `			iStart = iSpace = iCur;` |
|    464 | 7685 | `		}else if( iCur - iStart >= iWidth && iStart < iSpace ){` |
|      - | 7686 | `			/* Past the width mid-word: wrap back to the last space (which is consumed). */` |
|     17 | 7687 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iSpace - iStart));` |
|     17 | 7688 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     17 | 7689 | `			if( rc != SXRET_OK ){ goto oom; }` |
|     17 | 7690 | `			iStart = iSpace = iSpace + 1;` |
|      8 | 7691 | `		}` |
|    533 | 7692 | `		iCur++;` |
|      1 | 7693 | `	}` |
|      - | 7694 | `	/* Emit the trailing chunk. */` |
|     19 | 7695 | `	if( iStart < iCur ){` |
|     19 | 7696 | `		rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     19 | 7697 | `		if( rc != SXRET_OK ){ goto oom; }` |
|      9 | 7698 | `	}` |
|     19 | 7699 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|     19 | 7700 | `	SyBlobRelease(&sWorker);` |
|     19 | 7701 | `	return PH7_OK;` |
|    ! 0 | 7702 | `oom:` |
|    ! 0 | 7703 | `	SyBlobRelease(&sWorker);` |
|    ! 0 | 7704 | `	return PH7_ContextMemoryError(pCtx);` |
|     14 | 7705 | `}` |
|      - | 7706 | `/*` |
|      - | 7707 | ` * Check if the given character is a member of the given mask.` |
|      - | 7708 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 7709 | ` * Refer to [strtok()].` |
|      - | 7710 | ` */` |
|     30 | 7711 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 7712 | `{` |
|      - | 7713 | `	int i;` |
|     57 | 7714 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 7715 | `		if( c == zMask[i] ){` |
|     13 | 7716 | `			if( pOfft ){` |
|      5 | 7717 | `				*pOfft = i;` |
|      2 | 7718 | `			}` |
|     13 | 7719 | `			return TRUE;` |
|      - | 7720 | `		}` |
|     14 | 7721 | `	}` |
|     19 | 7722 | `	return FALSE;` |
|     16 | 7723 | `}` |
|      - | 7724 | `/*` |
|      - | 7725 | ` * Extract a single token from the input stream.` |
|      - | 7726 | ` * Refer to [strtok()].` |
|      - | 7727 | ` */` |
|      6 | 7728 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 7729 | `{` |
|      7 | 7730 | `	const char *zIn = *pzIn;` |
|      - | 7731 | `	const char *zPtr;` |
|      - | 7732 | `	/* Ignore leading delimiter */` |
|     11 | 7733 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 7734 | `		zIn++;` |
|      1 | 7735 | `	}` |
|      7 | 7736 | `	if( zIn >= zEnd ){` |
|      - | 7737 | `		/* End of input */` |
|    ! 0 | 7738 | `		return SXERR_EOF;` |
|      - | 7739 | `	}` |
|      7 | 7740 | `	zPtr = zIn;` |
|      - | 7741 | `	/* Extract the token */` |
|     13 | 7742 | `	while( zIn < zEnd ){` |
|     11 | 7743 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 7744 | `			/* UTF-8 stream */` |
|    ! 0 | 7745 | `			zIn++;` |
|    ! 0 | 7746 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 7747 | `		}else{` |
|     11 | 7748 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 7749 | `				break;` |
|      - | 7750 | `			}` |
|      7 | 7751 | `			zIn++;` |
|      - | 7752 | `		}` |
|      1 | 7753 | `	}` |
|      7 | 7754 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 7755 | `	/* Update the cursor */` |
|      7 | 7756 | `	*pzIn = zIn;` |
|      - | 7757 | `	/* Return to the caller */` |
|      7 | 7758 | `	return SXRET_OK;` |
|      4 | 7759 | `}` |
|      - | 7760 | `/* strtok auxiliary private data */` |
|      - | 7761 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 7762 | `struct strtok_aux_data` |
|      - | 7763 | `{` |
|      - | 7764 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 7765 | `	const char *zIn;   /* Current input stream */` |
|      - | 7766 | `	const char *zEnd;  /* End of input */` |
|      - | 7767 | `};` |
|      - | 7768 | `/*` |
|      - | 7769 | ` * string strtok(string $str,string $token)` |
|      - | 7770 | ` * string strtok(string $token)` |
|      - | 7771 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 7772 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 7773 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 7774 | ` *  words by using the space character as the token.` |
|      - | 7775 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 7776 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 7777 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 7778 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 7779 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 7780 | ` *  the argument are found.` |
|      - | 7781 | ` * Parameters` |
|      - | 7782 | ` *  $str` |
|      - | 7783 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 7784 | ` * $token` |
|      - | 7785 | ` *  The delimiter used when splitting up str.` |
|      - | 7786 | ` * Return` |
|      - | 7787 | ` *   Current token or FALSE on EOF.` |
|      - | 7788 | ` */` |
|      6 | 7789 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7790 | `{` |
|      - | 7791 | `	strtok_aux_data *pAux;` |
|      - | 7792 | `	const char *zMask;` |
|      - | 7793 | `	SyString sToken;` |
|      - | 7794 | `	int nMasklen;` |
|      - | 7795 | `	sxi32 rc;` |
|      7 | 7796 | `	if( nArg < 2 ){` |
|      - | 7797 | `		/* Extract top aux data */` |
|      5 | 7798 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      5 | 7799 | `		if( pAux == 0 ){` |
|      - | 7800 | `			/* No aux data,return FALSE */` |
|    ! 0 | 7801 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7802 | `			return PH7_OK;` |
|      - | 7803 | `		}` |
|      5 | 7804 | `		nMasklen = 0;` |
|      5 | 7805 | `		zMask = ""; /* cc warning */` |
|      5 | 7806 | `		if( nArg > 0 ){` |
|      - | 7807 | `			/* Extract the mask */` |
|      5 | 7808 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 7809 | `		}` |
|      5 | 7810 | `		if( nMasklen < 1 ){` |
|      - | 7811 | `			/* Invalid mask,return FALSE */` |
|    ! 0 | 7812 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 7813 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7814 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 7815 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7816 | `			return PH7_OK;` |
|      - | 7817 | `		}` |
|      - | 7818 | `		/* Extract the token */` |
|      5 | 7819 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 7820 | `		if( rc != SXRET_OK ){` |
|      - | 7821 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 7822 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 7823 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7824 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 7825 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7826 | `		}else{` |
|      - | 7827 | `			/* Return the extracted token */` |
|      5 | 7828 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 7829 | `		}` |
|      3 | 7830 | `	}else{` |
|      - | 7831 | `		const char *zInput,*zCur;` |
|      - | 7832 | `		char *zDup;` |
|      - | 7833 | `		int nLen;` |
|      - | 7834 | `		/* Extract the raw input */` |
|      3 | 7835 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 7836 | `		if( nLen < 1 ){` |
|      - | 7837 | `			/* Empty input,return FALSE */` |
|    ! 0 | 7838 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7839 | `			return PH7_OK;` |
|      - | 7840 | `		}` |
|      - | 7841 | `		/* Extract the mask */` |
|      3 | 7842 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 7843 | `		if( nMasklen < 1 ){` |
|      - | 7844 | `			/* Set a default mask */` |
|      - | 7845 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 7846 | `			zMask = TOK_MASK;` |
|    ! 0 | 7847 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 7848 | `#undef TOK_MASK` |
|    ! 0 | 7849 | `		}` |
|      - | 7850 | `		/* Extract a single token */` |
|      3 | 7851 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 7852 | `		if( rc != SXRET_OK ){` |
|      - | 7853 | `			/* Empty input */` |
|    ! 0 | 7854 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7855 | `			return PH7_OK;` |
|    ! 0 | 7856 | `		}else{` |
|      - | 7857 | `			/* Return the extracted token */` |
|      3 | 7858 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 7859 | `		}` |
|      - | 7860 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 7861 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 7862 | `		if( pAux ){` |
|      3 | 7863 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 7864 | `			if( nLen < 1 ){` |
|    ! 0 | 7865 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7866 | `				return PH7_OK;` |
|      - | 7867 | `			}` |
|      - | 7868 | `			/* Duplicate input */` |
|      3 | 7869 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 7870 | `			if( zDup  ){` |
|      3 | 7871 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 7872 | `				/* Register the aux data */` |
|      3 | 7873 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 7874 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 7875 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 7876 | `			}` |
|      1 | 7877 | `		}` |
|      - | 7878 | `	}` |
|      7 | 7879 | `	return PH7_OK;` |
|      4 | 7880 | `}` |
|      - | 7881 | `/*` |
|      - | 7882 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 7883 | ` *  Pad a string to a certain length with another string` |
|      - | 7884 | ` * Parameters` |
|      - | 7885 | ` *  $input` |
|      - | 7886 | ` *   The input string.` |
|      - | 7887 | ` * $pad_length` |
|      - | 7888 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 7889 | ` *   string, no padding takes place.` |
|      - | 7890 | ` * $pad_string` |
|      - | 7891 | ` *   Note:` |
|      - | 7892 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 7893 | ` *    divided by the pad_string's length.` |
|      - | 7894 | ` * $pad_type` |
|      - | 7895 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 7896 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 7897 | ` * Return` |
|      - | 7898 | ` *  The padded string.` |
|      - | 7899 | ` */` |
|     10 | 7900 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7901 | `{` |
|      - | 7902 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 7903 | `	const char *zIn,*zPad;` |
|     11 | 7904 | `	if( nArg < 2 ){` |
|      - | 7905 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7906 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7907 | `		return PH7_OK;` |
|      - | 7908 | `	}` |
|      - | 7909 | `	/* Extract the target string */` |
|     11 | 7910 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7911 | `	/* Padding length */` |
|      - | 7912 | `	{` |
|     11 | 7913 | `		sxi64 iTmp = 0;` |
|     11 | 7914 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_pad",2,"$length","int",&iTmp);` |
|     11 | 7915 | `		if( rcArg != PH7_OK ){` |
|    ! 0 | 7916 | `			return rcArg;` |
|      - | 7917 | `		}` |
|     11 | 7918 | `		iRealPad = iPadlen = (int)iTmp;` |
|      - | 7919 | `	}` |
|     11 | 7920 | `	if( iPadlen > 0 ){` |
|      9 | 7921 | `		iPadlen -= iLen;` |
|      4 | 7922 | `	}` |
|     11 | 7923 | `	if( iPadlen < 1  ){` |
|      - | 7924 | `		/* Return the string verbatim */` |
|      5 | 7925 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      5 | 7926 | `		return PH7_OK;` |
|      - | 7927 | `	}` |
|      7 | 7928 | `	zPad = " "; /* Whitespace padding */` |
|      7 | 7929 | `	iStrpad = (int)sizeof(char);` |
|      7 | 7930 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      7 | 7931 | `	if( nArg > 2 ){` |
|      - | 7932 | `		/* Padding string */` |
|      7 | 7933 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      7 | 7934 | `		if( iStrpad < 1 ){` |
|      - | 7935 | `			/* An empty pad string throws a catchable ValueError in PHP 8` |
|      - | 7936 | `			 * (only reached once padding is actually required). */` |
|      3 | 7937 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7938 | `				"str_pad(): Argument #3 ($pad_string) must not be empty");` |
|      - | 7939 | `		}` |
|      5 | 7940 | `		if( nArg > 3 ){` |
|      - | 7941 | `			/* Padd type */` |
|      5 | 7942 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 7943 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 7944 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 7945 | `			}` |
|      2 | 7946 | `		}` |
|      2 | 7947 | `	}` |
|      5 | 7948 | `	iDiv = 1;` |
|      5 | 7949 | `	if( iType == 2 ){` |
|    ! 0 | 7950 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 7951 | `	}` |
|      - | 7952 | `	/* Perform the requested operation */` |
|      5 | 7953 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 7954 | `		jPad = iStrpad;` |
|      5 | 7955 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 7956 | `			/* Padding */` |
|      5 | 7957 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 7958 | `				break;` |
|      - | 7959 | `			}` |
|      3 | 7960 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7961 | `		}` |
|      3 | 7962 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 7963 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 7964 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 7965 | `				if( jPad > iStrpad ){` |
|    ! 0 | 7966 | `					jPad = iStrpad;` |
|    ! 0 | 7967 | `				}` |
|      3 | 7968 | `				if( jPad < 1){` |
|    ! 0 | 7969 | `					break;` |
|      - | 7970 | `				}` |
|      3 | 7971 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 7972 | `			}` |
|      1 | 7973 | `		}` |
|      1 | 7974 | `	}` |
|      5 | 7975 | `	if( iLen > 0 ){` |
|      - | 7976 | `		/* Append the input string */` |
|      5 | 7977 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7978 | `	}` |
|      5 | 7979 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 7980 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 7981 | `			/* Padding */` |
|      5 | 7982 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 7983 | `				break;` |
|      - | 7984 | `			}` |
|      3 | 7985 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7986 | `		}` |
|      5 | 7987 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 7988 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 7989 | `			if( jPad > iStrpad ){` |
|    ! 0 | 7990 | `				jPad = iStrpad;` |
|    ! 0 | 7991 | `			}` |
|      3 | 7992 | `			if( jPad < 1){` |
|    ! 0 | 7993 | `				break;` |
|      - | 7994 | `			}` |
|      3 | 7995 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 7996 | `		}` |
|      1 | 7997 | `	}` |
|      5 | 7998 | `	return PH7_OK;` |
|      6 | 7999 | `}` |
|      - | 8000 | `/*` |
|      - | 8001 | ` * String replacement private data.` |
|      - | 8002 | ` */` |
|      - | 8003 | `typedef struct str_replace_data str_replace_data;` |
|      - | 8004 | `struct str_replace_data` |
|      - | 8005 | `{` |
|      - | 8006 | `	/* Used by the str_replace family to collect the search/replace arguments. */` |
|      - | 8007 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 8008 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 8009 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 8010 | `};` |
|      - | 8011 | `/*` |
|      - | 8012 | ` * Remove a substring.` |
|      - | 8013 | ` */` |
|      - | 8014 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 8015 | `	for(;;){\` |
|      - | 8016 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 8017 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 8018 | `		++OFFT;\` |
|      - | 8019 | `	}\` |
|      - | 8020 | `}` |
|      - | 8021 | `/*` |
|      - | 8022 | ` * Shift right and insert algorithm.` |
|      - | 8023 | ` */` |
|      - | 8024 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 8025 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 8026 | `		for(;;){\` |
|      - | 8027 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 8028 | `			if(INLEN < 1 ) { break; }\` |
|      - | 8029 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 8030 | `			--INLEN; \` |
|      - | 8031 | `		}\` |
|      - | 8032 | `		for(;;){\` |
|      - | 8033 | `				if(ELEN < 1) { break; }\` |
|      - | 8034 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 8035 | `				OFFT++;\` |
|      - | 8036 | `				ENTRY++;\` |
|      - | 8037 | `				--ELEN;\` |
|      - | 8038 | `		}\` |
|      - | 8039 | `}` |
|      - | 8040 | `/*` |
|      - | 8041 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 8042 | ` * replacement string [i.e: zReplace].` |
|      - | 8043 | ` */` |
|     54 | 8044 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      5 | 8045 | `{` |
|     59 | 8046 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 8047 | `	sxu32 n,m;` |
|     59 | 8048 | `	n = SyBlobLength(pWorker);` |
|     59 | 8049 | `	m = nOfft;` |
|      - | 8050 | `	/* Delete the old entry */` |
|   6689 | 8051 | `	STRDEL(zInput,n,m,nLen);` |
|     59 | 8052 | `	SyBlobLength(pWorker) -= nLen;` |
|     59 | 8053 | `	if( nReplen > 0 ){` |
|     53 | 8054 | `		sxi32 iRep = nReplen;` |
|      - | 8055 | `		sxi32 rc;` |
|      - | 8056 | `		/*` |
|      - | 8057 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 8058 | `		 * string.` |
|      - | 8059 | `		 */` |
|     53 | 8060 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     53 | 8061 | `		if( rc != SXRET_OK ){` |
|      - | 8062 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 8063 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 8064 | `			return rc;` |
|      - | 8065 | `		}` |
|      - | 8066 | `		/* Perform the insertion now */` |
|     53 | 8067 | `		zInput = (char *)SyBlobData(pWorker);` |
|     53 | 8068 | `		n = SyBlobLength(pWorker);` |
|   6481 | 8069 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     53 | 8070 | `		SyBlobLength(pWorker) += nReplen;` |
|     24 | 8071 | `	}` |
|     59 | 8072 | `	return SXRET_OK;` |
|     32 | 8073 | `}` |
|      - | 8074 | `/*` |
|      - | 8075 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 8076 | ` * to collect search/replace string.` |
|      - | 8077 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 8078 | ` */` |
|    102 | 8079 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      5 | 8080 | `{` |
|    107 | 8081 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 8082 | `	SyString sWorker;` |
|      - | 8083 | `	const char *zIn;` |
|      - | 8084 | `	int nByte;` |
|      - | 8085 | `	/* Extract a string representation of the given argument */` |
|    107 | 8086 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|    107 | 8087 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|    107 | 8088 | `	if( nByte > 0 ){` |
|      - | 8089 | `		char *zDup;` |
|      - | 8090 | `		/* Duplicate the chunk */` |
|    105 | 8091 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 8092 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 8093 | `			);` |
|    105 | 8094 | `		if( zDup == 0 ){` |
|      - | 8095 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 8096 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 8097 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 8098 | `			return SXERR_MEM;` |
|      - | 8099 | `		}` |
|    105 | 8100 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 8101 | `		/* Save the chunk */` |
|    105 | 8102 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     50 | 8103 | `	}` |
|      - | 8104 | `	/* Save for later processing */` |
|    107 | 8105 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 8106 | `	/* All done */` |
|     51 | 8107 | `	SXUNUSED(pKey); /* cc warning */` |
|    107 | 8108 | `	return PH7_OK;` |
|     56 | 8109 | `}` |
|      - | 8110 | `/*` |
|      - | 8111 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 8112 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 8113 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 8114 | ` * Parameters` |
|      - | 8115 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 8116 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 8117 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 8118 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 8119 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 8120 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 8121 | ` * $search` |
|      - | 8122 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 8123 | ` *  to designate multiple needles.` |
|      - | 8124 | ` * $replace` |
|      - | 8125 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 8126 | ` *  to designate multiple replacements.` |
|      - | 8127 | ` * $subject` |
|      - | 8128 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 8129 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 8130 | ` *  of subject, and the return value is an array as well.` |
|      - | 8131 | ` * $count (Not used)` |
|      - | 8132 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 8133 | ` * Return` |
|      - | 8134 | ` * This function returns a string or an array with the replaced values.` |
|      - | 8135 | ` */` |
|  29912 | 8136 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 8137 | `{` |
|      - | 8138 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 8139 | `	ProcStringMatch xMatch;` |
|      - | 8140 | `	const char *zIn,*zFunc;` |
|      - | 8141 | `	str_replace_data sRep;` |
|      - | 8142 | `	SyBlob sWorker;` |
|      - | 8143 | `	SySet sReplace;` |
|      - | 8144 | `	SySet sSearch;` |
|      - | 8145 | `	int rep_str;` |
|      - | 8146 | `	int nByte;` |
|      - | 8147 | `	sxi32 rc;` |
|  29917 | 8148 | `	if( nArg < 3 ){` |
|      - | 8149 | `		/* Missing/Invalid arguments,return null */` |
|    ! 0 | 8150 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8151 | `		return PH7_OK;` |
|      - | 8152 | `	}` |
|      - | 8153 | `	/* Initialize fields */` |
|  29917 | 8154 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  29917 | 8155 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  29917 | 8156 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  29917 | 8157 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  29917 | 8158 | `	sRep.pCtx = pCtx;` |
|  29917 | 8159 | `	sRep.pCollector = &sSearch;` |
|  29917 | 8160 | `	rep_str = 0;` |
|      - | 8161 | `	/* Extract the subject */` |
|  29917 | 8162 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  29917 | 8163 | `	if( nByte < 1 ){` |
|      - | 8164 | `		/* Nothing to replace,return the empty string */` |
|     21 | 8165 | `		ph7_result_string(pCtx,"",0);` |
|     21 | 8166 | `		return PH7_OK;` |
|      - | 8167 | `	}` |
|      - | 8168 | `	/* Copy the subject */` |
|  29897 | 8169 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 8170 | `	/* Search string */` |
|  29897 | 8171 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 8172 | `		/* Collect search string */` |
|     51 | 8173 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|     28 | 8174 | `	}else{` |
|      - | 8175 | `		/* Single pattern */` |
|  29851 | 8176 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  29851 | 8177 | `		if( nByte < 1 ){` |
|      - | 8178 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 8179 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 8180 | `			return PH7_OK;` |
|      - | 8181 | `		}` |
|  29847 | 8182 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 8183 | `		/* Save for later processing */` |
|  29847 | 8184 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 8185 | `	}` |
|      - | 8186 | `	/* Replace string */` |
|  29893 | 8187 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 8188 | `		/* Collect replace string */` |
|      7 | 8189 | `		sRep.pCollector = &sReplace;` |
|      7 | 8190 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 8191 | `	}else{` |
|      - | 8192 | `		/* Single needle */` |
|  29887 | 8193 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  29887 | 8194 | `		rep_str = 1;` |
|  29887 | 8195 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 8196 | `		/* Save for later processing */` |
|  29887 | 8197 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 8198 | `	}` |
|      - | 8199 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  29893 | 8200 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 8201 | `		SySetRelease(&sSearch);` |
|    ! 0 | 8202 | `		SySetRelease(&sReplace);` |
|    ! 0 | 8203 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 8204 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8205 | `	}` |
|      - | 8206 | `	/* Reset loop cursors */` |
|  29893 | 8207 | `	SySetResetCursor(&sSearch);` |
|  29893 | 8208 | `	SySetResetCursor(&sReplace);` |
|  29893 | 8209 | `	pReplace = pSearch = 0; /* cc warning */` |
|  29893 | 8210 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 8211 | `	/* Extract function name */` |
|  29893 | 8212 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 8213 | `	/* Set the default pattern match routine */` |
|  29893 | 8214 | `	xMatch = SyBlobSearch;` |
|  29893 | 8215 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 8216 | `		/* Case insensitive pattern match */` |
|     11 | 8217 | `		xMatch = iPatternMatch;` |
|      5 | 8218 | `	}` |
|      - | 8219 | `	/* Start the replace process */` |
|  59827 | 8220 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 8221 | `		sxu32 nCount,nOfft;` |
|  29939 | 8222 | `		if( pSearch->nByte <  1 ){` |
|      - | 8223 | `			/* Empty string,ignore */` |
|      3 | 8224 | `			continue;` |
|      - | 8225 | `		}` |
|      - | 8226 | `		/* Extract the replace string */` |
|  29937 | 8227 | `		if( rep_str ){` |
|  29927 | 8228 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  14966 | 8229 | `		}else{` |
|     11 | 8230 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 8231 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 8232 | `				 * An empty string is used for the rest of replacement values` |
|      - | 8233 | `				 */` |
|      3 | 8234 | `				pReplace = 0;` |
|      1 | 8235 | `			}` |
|      - | 8236 | `		}` |
|  29937 | 8237 | `		if( pReplace == 0 ){` |
|      - | 8238 | `			/* Use an empty string instead */` |
|      3 | 8239 | `			pReplace = &sTemp;` |
|      1 | 8240 | `		}` |
|  29937 | 8241 | `		nOfft = nCount = 0;` |
|  14993 | 8242 | `		for(;;){` |
|  29991 | 8243 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 8244 | `				break;` |
|      - | 8245 | `			}` |
|      - | 8246 | `			/* Perform a pattern lookup */` |
|  44966 | 8247 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  29974 | 8248 | `				pSearch->nByte,&nOfft);` |
|  29979 | 8249 | `			if( rc != SXRET_OK ){` |
|      - | 8250 | `				/* Pattern not found */` |
|  29925 | 8251 | `				break;` |
|      - | 8252 | `			}` |
|      - | 8253 | `			/* Perform the replace operation */` |
|     59 | 8254 | `			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|     59 | 8255 | `			if( rc != SXRET_OK ){` |
|      - | 8256 | `				/* Allocation failure: surface a fatal instead of a partial result */` |
|    ! 0 | 8257 | `				SySetRelease(&sSearch);` |
|    ! 0 | 8258 | `				SySetRelease(&sReplace);` |
|    ! 0 | 8259 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8260 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8261 | `			}` |
|      - | 8262 | `			/* Increment offset counter */` |
|     59 | 8263 | `			nCount += nOfft + pReplace->nByte;` |
|      5 | 8264 | `		}` |
|      5 | 8265 | `	}` |
|      - | 8266 | `	/* All done,clean-up the mess left behind */` |
|  29893 | 8267 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  29893 | 8268 | `	SySetRelease(&sSearch);` |
|  29893 | 8269 | `	SySetRelease(&sReplace);` |
|  29893 | 8270 | `	SyBlobRelease(&sWorker);` |
|  29893 | 8271 | `	if( rc != PH7_OK ){` |
|    ! 0 | 8272 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8273 | `	}` |
|  29893 | 8274 | `	return PH7_OK;` |
|  14961 | 8275 | `}` |
|      - | 8276 | `/*` |
|      - | 8277 | ` * strtr() array form: a single (key,value) pair copied out of the replace_pairs` |
|      - | 8278 | ` * array. The bytes are owned by a persistent pool (see strtr_collect) rather than` |
|      - | 8279 | ` * the transient walker values, which HashmapWalk releases after each callback, so` |
|      - | 8280 | ` * we store byte offsets into that pool instead of raw pointers.` |
|      - | 8281 | ` */` |
|      - | 8282 | `typedef struct strtr_entry strtr_entry;` |
|      - | 8283 | `struct strtr_entry` |
|      - | 8284 | `{` |
|      - | 8285 | `	sxu32 nKeyOfft; /* Offset of the search key inside the pool */` |
|      - | 8286 | `	sxu32 nKeyLen;  /* Length of the search key */` |
|      - | 8287 | `	sxu32 nValOfft; /* Offset of the replacement inside the pool */` |
|      - | 8288 | `	sxu32 nValLen;  /* Length of the replacement */` |
|      - | 8289 | `};` |
|      - | 8290 | `typedef struct strtr_collect strtr_collect;` |
|      - | 8291 | `struct strtr_collect` |
|      - | 8292 | `{` |
|      - | 8293 | `	SyBlob *pPool;  /* Byte pool holding copied key + value bytes */` |
|      - | 8294 | `	SySet  *pTable; /* Set of strtr_entry (parallel offsets into pPool) */` |
|      - | 8295 | `	sxi32   rc;     /* Carries an allocation failure (SXERR_MEM) out of the walker */` |
|      - | 8296 | `};` |
|      - | 8297 | `/*` |
|      - | 8298 | ` * Collect one replace_pairs entry into the persistent pool/offset table.` |
|      - | 8299 | ` * PHP coerces both the key and the value to string (an integer key becomes its` |
|      - | 8300 | ` * decimal form) and ignores an empty-string key.` |
|      - | 8301 | ` */` |
|     20 | 8302 | `static int StrtrCollectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 8303 | `{` |
|     21 | 8304 | `	strtr_collect *pCol = (strtr_collect *)pUserData;` |
|      - | 8305 | `	const char *zKey,*zVal;` |
|      - | 8306 | `	strtr_entry sEnt;` |
|      - | 8307 | `	int nKey,nVal;` |
|     21 | 8308 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|     21 | 8309 | `	if( nKey < 1 ){` |
|      - | 8310 | `		/* PHP ignores an empty-string key (it also emits a warning we do not replicate). */` |
|    ! 0 | 8311 | `		return PH7_OK;` |
|      - | 8312 | `	}` |
|     21 | 8313 | `	zVal = ph7_value_to_string(pData,&nVal);` |
|     21 | 8314 | `	sEnt.nKeyOfft = SyBlobLength(pCol->pPool);` |
|     21 | 8315 | `	sEnt.nKeyLen  = (sxu32)nKey;` |
|     21 | 8316 | `	if( SyBlobAppend(pCol->pPool,(const void *)zKey,(sxu32)nKey) != SXRET_OK ){` |
|    ! 0 | 8317 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8318 | `		return SXERR_ABORT;` |
|      - | 8319 | `	}` |
|     21 | 8320 | `	sEnt.nValOfft = SyBlobLength(pCol->pPool);` |
|     21 | 8321 | `	sEnt.nValLen  = (sxu32)nVal;` |
|     21 | 8322 | `	if( nVal > 0 && SyBlobAppend(pCol->pPool,(const void *)zVal,(sxu32)nVal) != SXRET_OK ){` |
|    ! 0 | 8323 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8324 | `		return SXERR_ABORT;` |
|      - | 8325 | `	}` |
|     21 | 8326 | `	if( SySetPut(pCol->pTable,(const void *)&sEnt) != SXRET_OK ){` |
|    ! 0 | 8327 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8328 | `		return SXERR_ABORT;` |
|      - | 8329 | `	}` |
|     21 | 8330 | `	return PH7_OK;` |
|     11 | 8331 | `}` |
|      - | 8332 | `/*` |
|      - | 8333 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 8334 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 8335 | ` *  Translate characters or replace substrings.` |
|      - | 8336 | ` * Parameters` |
|      - | 8337 | ` *  $str` |
|      - | 8338 | ` *  The string being translated.` |
|      - | 8339 | ` * $from` |
|      - | 8340 | ` *  The string being translated to to.` |
|      - | 8341 | ` * $to` |
|      - | 8342 | ` *  The string replacing from.` |
|      - | 8343 | ` * $replace_pairs` |
|      - | 8344 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 8345 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 8346 | ` * Return` |
|      - | 8347 | ` *  The translated string.` |
|      - | 8348 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 8349 | ` */` |
|     12 | 8350 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8351 | `{` |
|      - | 8352 | `	const char *zIn;` |
|      - | 8353 | `	int nLen;` |
|     13 | 8354 | `	if( nArg < 1 ){` |
|      - | 8355 | `		/* Nothing to replace,return FALSE */` |
|    ! 0 | 8356 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8357 | `		return PH7_OK;` |
|      - | 8358 | `	}` |
|     13 | 8359 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 8360 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 8361 | `		/* Invalid arguments */` |
|    ! 0 | 8362 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8363 | `		return PH7_OK;` |
|      - | 8364 | `	}` |
|     18 | 8365 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 8366 | `		strtr_collect sCol;` |
|      - | 8367 | `		SyBlob sPool,sWorker;` |
|      - | 8368 | `		SySet sTable;` |
|      - | 8369 | `		const char *zPool;` |
|      - | 8370 | `		strtr_entry *pEnt;` |
|      - | 8371 | `		sxi32 rc;` |
|      - | 8372 | `		int i,iRun;` |
|      - | 8373 | `		/*` |
|      - | 8374 | `		 * PHP's array-form strtr is a single left-to-right pass over the subject:` |
|      - | 8375 | `		 * at every position it substitutes the LONGEST replace_pairs key that` |
|      - | 8376 | `		 * matches there, then advances past the key (replacements are never` |
|      - | 8377 | `		 * rescanned). It is not a sequential per-key global replace. First copy` |
|      - | 8378 | `		 * the pairs into a persistent pool, then run that scan.` |
|      - | 8379 | `		 */` |
|     11 | 8380 | `		SyBlobInit(&sPool,&pCtx->pVm->sAllocator);` |
|     11 | 8381 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     11 | 8382 | `		SySetInit(&sTable,&pCtx->pVm->sAllocator,sizeof(strtr_entry));` |
|     11 | 8383 | `		sCol.pPool  = &sPool;` |
|     11 | 8384 | `		sCol.pTable = &sTable;` |
|     11 | 8385 | `		sCol.rc     = SXRET_OK;` |
|     11 | 8386 | `		ph7_array_walk(apArg[1],StrtrCollectWalker,&sCol);` |
|     11 | 8387 | `		if( sCol.rc != SXRET_OK ){` |
|      - | 8388 | `			/* Allocation failure while collecting the pairs: surface a fatal */` |
|    ! 0 | 8389 | `			SyBlobRelease(&sPool);` |
|    ! 0 | 8390 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 8391 | `			SySetRelease(&sTable);` |
|    ! 0 | 8392 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 8393 | `		}` |
|      - | 8394 | `		/* The pool is now stable, so offsets can be resolved against its base. */` |
|     11 | 8395 | `		zPool = (const char *)SyBlobData(&sPool);` |
|     11 | 8396 | `		rc = SXRET_OK;` |
|     11 | 8397 | `		iRun = 0; /* Start of the pending run of unmatched bytes copied verbatim. */` |
|     43 | 8398 | `		for( i = 0 ; i < nLen ; ){` |
|     33 | 8399 | `			strtr_entry *pBest = 0;` |
|     33 | 8400 | `			sxu32 nBest = 0;` |
|      - | 8401 | `			/* Pick the longest key that matches at the current position. */` |
|     33 | 8402 | `			SySetResetCursor(&sTable);` |
|     97 | 8403 | `			while( SXRET_OK == SySetGetNextEntry(&sTable,(void **)&pEnt) ){` |
|     64 | 8404 | `				if( pEnt->nKeyLen > nBest` |
|     60 | 8405 | `					&& pEnt->nKeyLen <= (sxu32)(nLen - i)` |
|     56 | 8406 | `					&& SyMemcmp(zPool + pEnt->nKeyOfft,zIn + i,pEnt->nKeyLen) == 0 ){` |
|     29 | 8407 | `					nBest = pEnt->nKeyLen;` |
|     29 | 8408 | `					pBest = pEnt;` |
|     14 | 8409 | `				}` |
|      1 | 8410 | `			}` |
|     33 | 8411 | `			if( pBest == 0 ){` |
|      - | 8412 | `				/* No key here: extend the literal run and copy it in one shot later. */` |
|      9 | 8413 | `				i++;` |
|      9 | 8414 | `				continue;` |
|      - | 8415 | `			}` |
|      - | 8416 | `			/* Flush the pending literal run, then the replacement. */` |
|     25 | 8417 | `			if( i > iRun ){` |
|      5 | 8418 | `				rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(i - iRun));` |
|      2 | 8419 | `			}` |
|     25 | 8420 | `			if( rc == SXRET_OK && pBest->nValLen > 0 ){` |
|     25 | 8421 | `				rc = SyBlobAppend(&sWorker,zPool + pBest->nValOfft,pBest->nValLen);` |
|     12 | 8422 | `			}` |
|     25 | 8423 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8424 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 8425 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8426 | `				SySetRelease(&sTable);` |
|    ! 0 | 8427 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8428 | `			}` |
|     25 | 8429 | `			i += (int)pBest->nKeyLen;` |
|     25 | 8430 | `			iRun = i;` |
|      1 | 8431 | `		}` |
|      - | 8432 | `		/* Flush the trailing literal run. */` |
|     11 | 8433 | `		if( nLen > iRun ){` |
|      3 | 8434 | `			rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(nLen - iRun));` |
|      3 | 8435 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8436 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 8437 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8438 | `				SySetRelease(&sTable);` |
|    ! 0 | 8439 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8440 | `			}` |
|      1 | 8441 | `		}` |
|      - | 8442 | `		/* All done, return the result string */` |
|     16 | 8443 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|     10 | 8444 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 8445 | `		/* Clean-up */` |
|     11 | 8446 | `		SyBlobRelease(&sPool);` |
|     11 | 8447 | `		SyBlobRelease(&sWorker);` |
|     11 | 8448 | `		SySetRelease(&sTable);` |
|     11 | 8449 | `		if( rc != PH7_OK ){` |
|    ! 0 | 8450 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 8451 | `		}` |
|      6 | 8452 | `	}else{` |
|      - | 8453 | `		int i,flen,tlen,c,iOfft;` |
|      - | 8454 | `		const char *zFrom,*zTo;` |
|      3 | 8455 | `		if( nArg < 3 ){` |
|      - | 8456 | `			/* Nothing to replace */` |
|    ! 0 | 8457 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8458 | `			return PH7_OK;` |
|      - | 8459 | `		}` |
|      - | 8460 | `		/* Extract given arguments */` |
|      3 | 8461 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 8462 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 8463 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 8464 | `			/* Nothing to replace */` |
|    ! 0 | 8465 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8466 | `			return PH7_OK;` |
|      - | 8467 | `		}` |
|      - | 8468 | `		/* Start the replace process */` |
|     13 | 8469 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 8470 | `			c = zIn[i];` |
|     11 | 8471 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 8472 | `				if ( iOfft < tlen ){` |
|      5 | 8473 | `					c = zTo[iOfft];` |
|      2 | 8474 | `				}` |
|      2 | 8475 | `			}` |
|     11 | 8476 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 8477 |  |
|      6 | 8478 | `		}` |
|      - | 8479 | `	}` |
|     13 | 8480 | `	return PH7_OK;` |
|      7 | 8481 | `}` |
|      - | 8482 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8483 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8484 | `/*` |
|      - | 8485 | ` * Parse an INI string.` |
|      - | 8486 |  |
|      - | 8487 | ` * According to wikipedia` |
|      - | 8488 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 8489 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 8490 | ` *  Format` |
|      - | 8491 | `*    Properties` |
|      - | 8492 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 8493 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 8494 | `*     Example:` |
|      - | 8495 | `*      name=value` |
|      - | 8496 | `*    Sections` |
|      - | 8497 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 8498 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 8499 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 8500 | `*     or the end of the file. Sections may not be nested.` |
|      - | 8501 | `*     Example:` |
|      - | 8502 | `*      [section]` |
|      - | 8503 | `*   Comments` |
|      - | 8504 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 8505 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 8506 | `*/` |
|     12 | 8507 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 8508 | `{` |
|      - | 8509 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 8510 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 8511 | `	SyHashEntry *pEntry;` |
|      - | 8512 | `	SyString sEntry;` |
|      - | 8513 | `	SyHash sHash;` |
|      - | 8514 | `	int c;` |
|      - | 8515 | `	/* Create an empty array and worker variables */` |
|     13 | 8516 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 8517 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 8518 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 8519 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 8520 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|    ! 0 | 8521 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8522 | `	}` |
|     13 | 8523 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 8524 | `	pCur = pArray;` |
|      - | 8525 | `	/* Start the parse process */` |
|     21 | 8526 | `	for(;;){` |
|      - | 8527 | `		/* Ignore leading white spaces */` |
|     69 | 8528 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 8529 | `			zIn++;` |
|      1 | 8530 | `		}` |
|     43 | 8531 | `		if( zIn >= zEnd ){` |
|      - | 8532 | `			/* No more input to process */` |
|     13 | 8533 | `			break;` |
|      - | 8534 | `		}` |
|     31 | 8535 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 8536 | `			/* Comment til the end of line */` |
|    ! 0 | 8537 | `			zIn++;` |
|    ! 0 | 8538 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 8539 | `				zIn++;` |
|    ! 0 | 8540 | `			}` |
|    ! 0 | 8541 | `			continue;` |
|      - | 8542 | `		}` |
|      - | 8543 | `		/* Reset the string cursor of the working variable */` |
|     31 | 8544 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 8545 | `		if( zIn[0] == '[' ){` |
|      - | 8546 | `			/* Section: Extract the section name */` |
|      9 | 8547 | `			zIn++;` |
|      9 | 8548 | `			zCur = zIn;` |
|     73 | 8549 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 8550 | `				zIn++;` |
|      1 | 8551 | `			}` |
|      9 | 8552 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 8553 | `				/* Save the section name */` |
|      5 | 8554 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 8555 | `				SyStringFullTrim(&sEntry);` |
|      5 | 8556 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 8557 | `				if( sEntry.nByte > 0 ){` |
|      - | 8558 | `					/* Associate an array with the section */` |
|      5 | 8559 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 8560 | `					if( pSection ){` |
|      5 | 8561 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 8562 | `						pCur = pSection;` |
|      2 | 8563 | `					}` |
|      2 | 8564 | `				}` |
|      2 | 8565 | `			}` |
|      9 | 8566 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 8567 | `		}else{` |
|      - | 8568 | `			ph7_value *pOldCur;` |
|      - | 8569 | `			int is_array;` |
|      - | 8570 | `			int iLen;` |
|      - | 8571 | `			/* Properties */` |
|     23 | 8572 | `			is_array = 0;` |
|     23 | 8573 | `			zCur = zIn;` |
|     23 | 8574 | `			iLen = 0; /* cc warning */` |
|     23 | 8575 | `			pOldCur = pCur;` |
|    155 | 8576 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 8577 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 8578 | `					/* Array */` |
|    ! 0 | 8579 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 8580 | `					is_array = 1;` |
|    ! 0 | 8581 | `					if( iLen > 0 ){` |
|    ! 0 | 8582 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 8583 | `						/* Query the hashtable */` |
|    ! 0 | 8584 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 8585 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 8586 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 8587 | `						if( pEntry ){` |
|    ! 0 | 8588 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 8589 | `						}else{` |
|      - | 8590 | `							/* Create an empty array */` |
|    ! 0 | 8591 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 8592 | `							if( pvArr ){` |
|      - | 8593 | `								/* Save the entry */` |
|    ! 0 | 8594 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 8595 | `								/* Insert the entry */` |
|    ! 0 | 8596 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 8597 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 8598 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 8599 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 8600 | `							}` |
|      - | 8601 | `						}` |
|    ! 0 | 8602 | `						if( pvArr ){` |
|    ! 0 | 8603 | `							pCur = pvArr;` |
|    ! 0 | 8604 | `						}` |
|    ! 0 | 8605 | `					}` |
|    ! 0 | 8606 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 8607 | `						zIn++;` |
|    ! 0 | 8608 | `					}` |
|    ! 0 | 8609 | `				}` |
|    133 | 8610 | `				zIn++;` |
|      1 | 8611 | `			}` |
|     23 | 8612 | `			if( !is_array ){` |
|     23 | 8613 | `				iLen = (int)(zIn-zCur);` |
|     11 | 8614 | `			}` |
|      - | 8615 | `			/* Trim the key */` |
|     23 | 8616 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 8617 | `			SyStringFullTrim(&sEntry);` |
|     23 | 8618 | `			if( sEntry.nByte > 0 ){` |
|     23 | 8619 | `				if( !is_array ){` |
|      - | 8620 | `					/* Save the key name */` |
|     23 | 8621 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 8622 | `				}` |
|      - | 8623 | `				/* extract key value */` |
|     23 | 8624 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 8625 | `				zIn++; /* '=' */` |
|     39 | 8626 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 8627 | `					zIn++;` |
|      1 | 8628 | `				}` |
|     23 | 8629 | `				if( zIn < zEnd ){` |
|     21 | 8630 | `					zCur = zIn;` |
|     21 | 8631 | `					c = zIn[0];` |
|     21 | 8632 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 8633 | `						zIn++;` |
|      - | 8634 | `						/* Delimit the value */` |
|    ! 0 | 8635 | `						while( zIn < zEnd ){` |
|    ! 0 | 8636 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 8637 | `								break;` |
|      - | 8638 | `							}` |
|    ! 0 | 8639 | `							zIn++;` |
|    ! 0 | 8640 | `						}` |
|    ! 0 | 8641 | `						if( zIn < zEnd ){` |
|    ! 0 | 8642 | `							zIn++;` |
|    ! 0 | 8643 | `						}` |
|    ! 0 | 8644 | `					}else{` |
|    125 | 8645 | `						while( zIn < zEnd ){` |
|    123 | 8646 | `							if( zIn[0] == '\n' ){` |
|     19 | 8647 | `								if( zIn[-1] != '\\' ){` |
|     19 | 8648 | `									break;` |
|    ! 0 | 8649 | `								}` |
|    105 | 8650 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 8651 | `								/* Inline comments */` |
|    ! 0 | 8652 | `								break;` |
|      - | 8653 | `							}` |
|    105 | 8654 | `							zIn++;` |
|      1 | 8655 | `						}` |
|      - | 8656 | `					}` |
|      - | 8657 | `					/* Trim the value */` |
|     21 | 8658 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 8659 | `					SyStringFullTrim(&sEntry);` |
|     21 | 8660 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 8661 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 8662 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 8663 | `					}` |
|     21 | 8664 | `					if( sEntry.nByte > 0 ){` |
|     21 | 8665 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 8666 | `					}` |
|      - | 8667 | `					/* Insert the key and it's value */` |
|     21 | 8668 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 8669 | `				}` |
|     12 | 8670 | `			}else{` |
|    ! 0 | 8671 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 8672 | `					zIn++;` |
|    ! 0 | 8673 | `				}` |
|      - | 8674 | `			}` |
|     23 | 8675 | `			pCur = pOldCur;` |
|      - | 8676 | `		}` |
|      1 | 8677 | `	}` |
|     13 | 8678 | `	SyHashRelease(&sHash);` |
|      - | 8679 | `	/* Return the parse of the INI string */` |
|     13 | 8680 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 8681 | `	return SXRET_OK;` |
|      7 | 8682 | `}` |
|      - | 8683 | `/*` |
|      - | 8684 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 8685 | ` *  Parse a configuration string.` |
|      - | 8686 | ` * Parameters` |
|      - | 8687 | ` *  $ini` |
|      - | 8688 | ` *   The contents of the ini file being parsed.` |
|      - | 8689 | ` *  $process_sections` |
|      - | 8690 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 8691 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 8692 | ` *  $scanner_mode (Not used)` |
|      - | 8693 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 8694 | ` *   then option values will not be parsed.` |
|      - | 8695 | ` * Return` |
|      - | 8696 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 8697 | ` */` |
|     10 | 8698 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8699 | `{` |
|      - | 8700 | `	const char *zIni;` |
|      - | 8701 | `	int nByte;` |
|     11 | 8702 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8703 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 8704 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8705 | `		return PH7_OK;` |
|      - | 8706 | `	}` |
|      - | 8707 | `	/* Extract the raw INI buffer */` |
|     11 | 8708 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 8709 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|     11 | 8710 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      6 | 8711 | `}` |
|      - | 8712 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8713 |  |
|      - | 8714 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8715 |  |
|      - | 8716 | `/*` |
|      - | 8717 | ` * Ctype Functions.` |
|      - | 8718 | ` * Status:` |
|      - | 8719 | ` *    Stable.` |
|      - | 8720 | ` */` |
|      - | 8721 | `/*` |
|      - | 8722 | ` * bool ctype_alnum(string $text)` |
|      - | 8723 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 8724 | ` * Parameters` |
|      - | 8725 | ` *  $text` |
|      - | 8726 | ` *   The tested string.` |
|      - | 8727 | ` * Return` |
|      - | 8728 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 8729 | ` */` |
|     14 | 8730 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8731 | `{` |
|      - | 8732 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8733 | `	int nLen;` |
|     15 | 8734 | `	if( nArg < 1 ){` |
|      - | 8735 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8736 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8737 | `		return PH7_OK;` |
|      - | 8738 | `	}` |
|      - | 8739 | `	/* Extract the target string */` |
|     15 | 8740 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 8741 | `	zEnd = &zIn[nLen];` |
|     15 | 8742 | `	if( nLen < 1 ){` |
|      - | 8743 | `		/* Empty string,return FALSE */` |
|      3 | 8744 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8745 | `		return PH7_OK;` |
|      - | 8746 | `	}` |
|      - | 8747 | `	/* Perform the requested operation */` |
|     32 | 8748 | `	for(;;){` |
|     65 | 8749 | `		if( zIn >= zEnd ){` |
|      - | 8750 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8751 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8752 | `			return PH7_OK;` |
|      - | 8753 | `		}` |
|     57 | 8754 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 8755 | `			break;` |
|      - | 8756 | `		}` |
|      - | 8757 | `		/* Point to the next character */` |
|     53 | 8758 | `		zIn++;` |
|      1 | 8759 | `	}` |
|      - | 8760 | `	/* The test failed,return FALSE */` |
|      5 | 8761 | `	ph7_result_bool(pCtx,0);` |
|      5 | 8762 | `	return PH7_OK;` |
|      8 | 8763 | `}` |
|      - | 8764 | `/*` |
|      - | 8765 | ` * bool ctype_alpha(string $text)` |
|      - | 8766 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 8767 | ` * Parameters` |
|      - | 8768 | ` *  $text` |
|      - | 8769 | ` *   The tested string.` |
|      - | 8770 | ` * Return` |
|      - | 8771 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 8772 | ` */` |
|     16 | 8773 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8774 | `{` |
|      - | 8775 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8776 | `	int nLen;` |
|     17 | 8777 | `	if( nArg < 1 ){` |
|      - | 8778 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8779 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8780 | `		return PH7_OK;` |
|      - | 8781 | `	}` |
|      - | 8782 | `	/* Extract the target string */` |
|     17 | 8783 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8784 | `	zEnd = &zIn[nLen];` |
|     17 | 8785 | `	if( nLen < 1 ){` |
|      - | 8786 | `		/* Empty string,return FALSE */` |
|      3 | 8787 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8788 | `		return PH7_OK;` |
|      - | 8789 | `	}` |
|      - | 8790 | `	/* Perform the requested operation */` |
|     42 | 8791 | `	for(;;){` |
|     85 | 8792 | `		if( zIn >= zEnd ){` |
|      - | 8793 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8794 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8795 | `			return PH7_OK;` |
|      - | 8796 | `		}` |
|     77 | 8797 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 8798 | `			break;` |
|      - | 8799 | `		}` |
|      - | 8800 | `		/* Point to the next character */` |
|     71 | 8801 | `		zIn++;` |
|      1 | 8802 | `	}` |
|      - | 8803 | `	/* The test failed,return FALSE */` |
|      7 | 8804 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8805 | `	return PH7_OK;` |
|      9 | 8806 | `}` |
|      - | 8807 | `/*` |
|      - | 8808 | ` * bool ctype_cntrl(string $text)` |
|      - | 8809 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 8810 | ` * Parameters` |
|      - | 8811 | ` *  $text` |
|      - | 8812 | ` *   The tested string.` |
|      - | 8813 | ` * Return` |
|      - | 8814 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 8815 | ` */` |
|     16 | 8816 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8817 | `{` |
|      - | 8818 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8819 | `	int nLen;` |
|     17 | 8820 | `	if( nArg < 1 ){` |
|      - | 8821 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8822 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8823 | `		return PH7_OK;` |
|      - | 8824 | `	}` |
|      - | 8825 | `	/* Extract the target string */` |
|     17 | 8826 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8827 | `	zEnd = &zIn[nLen];` |
|     17 | 8828 | `	if( nLen < 1 ){` |
|      - | 8829 | `		/* Empty string,return FALSE */` |
|      3 | 8830 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8831 | `		return PH7_OK;` |
|      - | 8832 | `	}` |
|      - | 8833 | `	/* Perform the requested operation */` |
|     14 | 8834 | `	for(;;){` |
|     29 | 8835 | `		if( zIn >= zEnd ){` |
|      - | 8836 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8837 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8838 | `			return PH7_OK;` |
|      - | 8839 | `		}` |
|     21 | 8840 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8841 | `			/* UTF-8 stream  */` |
|    ! 0 | 8842 | `			break;` |
|      - | 8843 | `		}` |
|     21 | 8844 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 8845 | `			break;` |
|      - | 8846 | `		}` |
|      - | 8847 | `		/* Point to the next character */` |
|     15 | 8848 | `		zIn++;` |
|      1 | 8849 | `	}` |
|      - | 8850 | `	/* The test failed,return FALSE */` |
|      7 | 8851 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8852 | `	return PH7_OK;` |
|      9 | 8853 | `}` |
|      - | 8854 | `/*` |
|      - | 8855 | ` * bool ctype_digit(string $text)` |
|      - | 8856 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 8857 | ` * Parameters` |
|      - | 8858 | ` *  $text` |
|      - | 8859 | ` *   The tested string.` |
|      - | 8860 | ` * Return` |
|      - | 8861 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 8862 | ` */` |
|   1890 | 8863 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 8864 | `{` |
|      - | 8865 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8866 | `	int nLen;` |
|   1895 | 8867 | `	if( nArg < 1 ){` |
|      - | 8868 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8869 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8870 | `		return PH7_OK;` |
|      - | 8871 | `	}` |
|      - | 8872 | `	/* Extract the target string */` |
|   1895 | 8873 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1895 | 8874 | `	zEnd = &zIn[nLen];` |
|   1895 | 8875 | `	if( nLen < 1 ){` |
|      - | 8876 | `		/* Empty string,return FALSE */` |
|      3 | 8877 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8878 | `		return PH7_OK;` |
|      - | 8879 | `	}` |
|      - | 8880 | `	/* Perform the requested operation */` |
|   1750 | 8881 | `	for(;;){` |
|   3505 | 8882 | `		if( zIn >= zEnd ){` |
|      - | 8883 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1567 | 8884 | `			ph7_result_bool(pCtx,1);` |
|   1567 | 8885 | `			return PH7_OK;` |
|      - | 8886 | `		}` |
|   1943 | 8887 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8888 | `			/* UTF-8 stream  */` |
|    ! 0 | 8889 | `			break;` |
|      - | 8890 | `		}` |
|   1943 | 8891 | `		if( !SyisDigit(zIn[0]) ){` |
|    331 | 8892 | `			break;` |
|      - | 8893 | `		}` |
|      - | 8894 | `		/* Point to the next character */` |
|   1617 | 8895 | `		zIn++;` |
|      5 | 8896 | `	}` |
|      - | 8897 | `	/* The test failed,return FALSE */` |
|    331 | 8898 | `	ph7_result_bool(pCtx,0);` |
|    331 | 8899 | `	return PH7_OK;` |
|    950 | 8900 | `}` |
|      - | 8901 | `/*` |
|      - | 8902 | ` * bool ctype_xdigit(string $text)` |
|      - | 8903 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 8904 | ` * Parameters` |
|      - | 8905 | ` *  $text` |
|      - | 8906 | ` *   The tested string.` |
|      - | 8907 | ` * Return` |
|      - | 8908 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 8909 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 8910 | ` */` |
|     18 | 8911 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8912 | `{` |
|      - | 8913 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8914 | `	int nLen;` |
|     19 | 8915 | `	if( nArg < 1 ){` |
|      - | 8916 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8917 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8918 | `		return PH7_OK;` |
|      - | 8919 | `	}` |
|      - | 8920 | `	/* Extract the target string */` |
|     19 | 8921 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 8922 | `	zEnd = &zIn[nLen];` |
|     19 | 8923 | `	if( nLen < 1 ){` |
|      - | 8924 | `		/* Empty string,return FALSE */` |
|      3 | 8925 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8926 | `		return PH7_OK;` |
|      - | 8927 | `	}` |
|      - | 8928 | `	/* Perform the requested operation */` |
|     46 | 8929 | `	for(;;){` |
|     93 | 8930 | `		if( zIn >= zEnd ){` |
|      - | 8931 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 8932 | `			ph7_result_bool(pCtx,1);` |
|     11 | 8933 | `			return PH7_OK;` |
|      - | 8934 | `		}` |
|     83 | 8935 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8936 | `			/* UTF-8 stream  */` |
|    ! 0 | 8937 | `			break;` |
|      - | 8938 | `		}` |
|     83 | 8939 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 8940 | `			break;` |
|      - | 8941 | `		}` |
|      - | 8942 | `		/* Point to the next character */` |
|     77 | 8943 | `		zIn++;` |
|      1 | 8944 | `	}` |
|      - | 8945 | `	/* The test failed,return FALSE */` |
|      7 | 8946 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8947 | `	return PH7_OK;` |
|     10 | 8948 | `}` |
|      - | 8949 | `/*` |
|      - | 8950 | ` * bool ctype_graph(string $text)` |
|      - | 8951 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 8952 | ` * Parameters` |
|      - | 8953 | ` *  $text` |
|      - | 8954 | ` *   The tested string.` |
|      - | 8955 | ` * Return` |
|      - | 8956 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 8957 | ` * (no white space), FALSE otherwise.` |
|      - | 8958 | ` */` |
|     16 | 8959 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8960 | `{` |
|      - | 8961 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8962 | `	int nLen;` |
|     17 | 8963 | `	if( nArg < 1 ){` |
|      - | 8964 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8965 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8966 | `		return PH7_OK;` |
|      - | 8967 | `	}` |
|      - | 8968 | `	/* Extract the target string */` |
|     17 | 8969 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8970 | `	zEnd = &zIn[nLen];` |
|     17 | 8971 | `	if( nLen < 1 ){` |
|      - | 8972 | `		/* Empty string,return FALSE */` |
|      3 | 8973 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8974 | `		return PH7_OK;` |
|      - | 8975 | `	}` |
|      - | 8976 | `	/* Perform the requested operation */` |
|     57 | 8977 | `	for(;;){` |
|    115 | 8978 | `		if( zIn >= zEnd ){` |
|      - | 8979 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8980 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8981 | `			return PH7_OK;` |
|      - | 8982 | `		}` |
|    107 | 8983 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8984 | `			/* UTF-8 stream  */` |
|    ! 0 | 8985 | `			break;` |
|      - | 8986 | `		}` |
|    107 | 8987 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 8988 | `			break;` |
|      - | 8989 | `		}` |
|      - | 8990 | `		/* Point to the next character */` |
|    101 | 8991 | `		zIn++;` |
|      1 | 8992 | `	}` |
|      - | 8993 | `	/* The test failed,return FALSE */` |
|      7 | 8994 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8995 | `	return PH7_OK;` |
|      9 | 8996 | `}` |
|      - | 8997 | `/*` |
|      - | 8998 | ` * bool ctype_print(string $text)` |
|      - | 8999 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 9000 | ` * Parameters` |
|      - | 9001 | ` *  $text` |
|      - | 9002 | ` *   The tested string.` |
|      - | 9003 | ` * Return` |
|      - | 9004 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 9005 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 9006 | ` *  or control function at all.` |
|      - | 9007 | ` */` |
|     16 | 9008 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9009 | `{` |
|      - | 9010 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9011 | `	int nLen;` |
|     17 | 9012 | `	if( nArg < 1 ){` |
|      - | 9013 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9014 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9015 | `		return PH7_OK;` |
|      - | 9016 | `	}` |
|      - | 9017 | `	/* Extract the target string */` |
|     17 | 9018 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9019 | `	zEnd = &zIn[nLen];` |
|     17 | 9020 | `	if( nLen < 1 ){` |
|      - | 9021 | `		/* Empty string,return FALSE */` |
|      3 | 9022 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9023 | `		return PH7_OK;` |
|      - | 9024 | `	}` |
|      - | 9025 | `	/* Perform the requested operation */` |
|     63 | 9026 | `	for(;;){` |
|    127 | 9027 | `		if( zIn >= zEnd ){` |
|      - | 9028 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 9029 | `			ph7_result_bool(pCtx,1);` |
|      9 | 9030 | `			return PH7_OK;` |
|      - | 9031 | `		}` |
|    119 | 9032 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9033 | `			/* UTF-8 stream  */` |
|    ! 0 | 9034 | `			break;` |
|      - | 9035 | `		}` |
|    119 | 9036 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 9037 | `			break;` |
|      - | 9038 | `		}` |
|      - | 9039 | `		/* Point to the next character */` |
|    113 | 9040 | `		zIn++;` |
|      1 | 9041 | `	}` |
|      - | 9042 | `	/* The test failed,return FALSE */` |
|      7 | 9043 | `	ph7_result_bool(pCtx,0);` |
|      7 | 9044 | `	return PH7_OK;` |
|      9 | 9045 | `}` |
|      - | 9046 | `/*` |
|      - | 9047 | ` * bool ctype_punct(string $text)` |
|      - | 9048 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 9049 | ` * Parameters` |
|      - | 9050 | ` *  $text` |
|      - | 9051 | ` *   The tested string.` |
|      - | 9052 | ` * Return` |
|      - | 9053 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 9054 | ` *  digit or blank, FALSE otherwise.` |
|      - | 9055 | ` */` |
|     18 | 9056 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9057 | `{` |
|      - | 9058 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9059 | `	int nLen;` |
|     19 | 9060 | `	if( nArg < 1 ){` |
|      - | 9061 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9062 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9063 | `		return PH7_OK;` |
|      - | 9064 | `	}` |
|      - | 9065 | `	/* Extract the target string */` |
|     19 | 9066 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 9067 | `	zEnd = &zIn[nLen];` |
|     19 | 9068 | `	if( nLen < 1 ){` |
|      - | 9069 | `		/* Empty string,return FALSE */` |
|      3 | 9070 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9071 | `		return PH7_OK;` |
|      - | 9072 | `	}` |
|      - | 9073 | `	/* Perform the requested operation */` |
|     38 | 9074 | `	for(;;){` |
|     77 | 9075 | `		if( zIn >= zEnd ){` |
|      - | 9076 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 9077 | `			ph7_result_bool(pCtx,1);` |
|      9 | 9078 | `			return PH7_OK;` |
|      - | 9079 | `		}` |
|     69 | 9080 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9081 | `			/* UTF-8 stream  */` |
|    ! 0 | 9082 | `			break;` |
|      - | 9083 | `		}` |
|     69 | 9084 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 9085 | `			break;` |
|      - | 9086 | `		}` |
|      - | 9087 | `		/* Point to the next character */` |
|     61 | 9088 | `		zIn++;` |
|      1 | 9089 | `	}` |
|      - | 9090 | `	/* The test failed,return FALSE */` |
|      9 | 9091 | `	ph7_result_bool(pCtx,0);` |
|      9 | 9092 | `	return PH7_OK;` |
|     10 | 9093 | `}` |
|      - | 9094 | `/*` |
|      - | 9095 | ` * bool ctype_space(string $text)` |
|      - | 9096 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 9097 | ` * Parameters` |
|      - | 9098 | ` *  $text` |
|      - | 9099 | ` *   The tested string.` |
|      - | 9100 | ` * Return` |
|      - | 9101 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 9102 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 9103 | ` *  and form feed characters.` |
|      - | 9104 | ` */` |
|  63773 | 9105 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 9106 | `{` |
|      - | 9107 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9108 | `	int nLen;` |
|  63778 | 9109 | `	if( nArg < 1 ){` |
|      - | 9110 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9111 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9112 | `		return PH7_OK;` |
|      - | 9113 | `	}` |
|      - | 9114 | `	/* Extract the target string */` |
|  63778 | 9115 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  63778 | 9116 | `	zEnd = &zIn[nLen];` |
|  63778 | 9117 | `	if( nLen < 1 ){` |
|      - | 9118 | `		/* Empty string,return FALSE */` |
|      3 | 9119 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9120 | `		return PH7_OK;` |
|      - | 9121 | `	}` |
|      - | 9122 | `	/* Perform the requested operation */` |
|  33012 | 9123 | `	for(;;){` |
|  65944 | 9124 | `		if( zIn >= zEnd ){` |
|      - | 9125 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   2149 | 9126 | `			ph7_result_bool(pCtx,1);` |
|   2149 | 9127 | `			return PH7_OK;` |
|      - | 9128 | `		}` |
|  63800 | 9129 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9130 | `			/* UTF-8 stream  */` |
|    ! 0 | 9131 | `			break;` |
|      - | 9132 | `		}` |
|  63800 | 9133 | `		if( !SyisSpace(zIn[0]) ){` |
|  61632 | 9134 | `			break;` |
|      - | 9135 | `		}` |
|      - | 9136 | `		/* Point to the next character */` |
|   2173 | 9137 | `		zIn++;` |
|      5 | 9138 | `	}` |
|      - | 9139 | `	/* The test failed,return FALSE */` |
|  61632 | 9140 | `	ph7_result_bool(pCtx,0);` |
|  61632 | 9141 | `	return PH7_OK;` |
|  31934 | 9142 | `}` |
|      - | 9143 | `/*` |
|      - | 9144 | ` * bool ctype_lower(string $text)` |
|      - | 9145 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 9146 | ` * Parameters` |
|      - | 9147 | ` *  $text` |
|      - | 9148 | ` *   The tested string.` |
|      - | 9149 | ` * Return` |
|      - | 9150 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 9151 | ` */` |
|     16 | 9152 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9153 | `{` |
|      - | 9154 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9155 | `	int nLen;` |
|     17 | 9156 | `	if( nArg < 1 ){` |
|      - | 9157 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9158 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9159 | `		return PH7_OK;` |
|      - | 9160 | `	}` |
|      - | 9161 | `	/* Extract the target string */` |
|     17 | 9162 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9163 | `	zEnd = &zIn[nLen];` |
|     17 | 9164 | `	if( nLen < 1 ){` |
|      - | 9165 | `		/* Empty string,return FALSE */` |
|      3 | 9166 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9167 | `		return PH7_OK;` |
|      - | 9168 | `	}` |
|      - | 9169 | `	/* Perform the requested operation */` |
|     27 | 9170 | `	for(;;){` |
|     55 | 9171 | `		if( zIn >= zEnd ){` |
|      - | 9172 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 9173 | `			ph7_result_bool(pCtx,1);` |
|      5 | 9174 | `			return PH7_OK;` |
|      - | 9175 | `		}` |
|     51 | 9176 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 9177 | `			break;` |
|      - | 9178 | `		}` |
|      - | 9179 | `		/* Point to the next character */` |
|     41 | 9180 | `		zIn++;` |
|      1 | 9181 | `	}` |
|      - | 9182 | `	/* The test failed,return FALSE */` |
|     11 | 9183 | `	ph7_result_bool(pCtx,0);` |
|     11 | 9184 | `	return PH7_OK;` |
|      9 | 9185 | `}` |
|      - | 9186 | `/*` |
|      - | 9187 | ` * bool ctype_upper(string $text)` |
|      - | 9188 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 9189 | ` * Parameters` |
|      - | 9190 | ` *  $text` |
|      - | 9191 | ` *   The tested string.` |
|      - | 9192 | ` * Return` |
|      - | 9193 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 9194 | ` */` |
|     16 | 9195 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9196 | `{` |
|      - | 9197 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9198 | `	int nLen;` |
|     17 | 9199 | `	if( nArg < 1 ){` |
|      - | 9200 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9201 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9202 | `		return PH7_OK;` |
|      - | 9203 | `	}` |
|      - | 9204 | `	/* Extract the target string */` |
|     17 | 9205 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9206 | `	zEnd = &zIn[nLen];` |
|     17 | 9207 | `	if( nLen < 1 ){` |
|      - | 9208 | `		/* Empty string,return FALSE */` |
|      3 | 9209 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9210 | `		return PH7_OK;` |
|      - | 9211 | `	}` |
|      - | 9212 | `	/* Perform the requested operation */` |
|     28 | 9213 | `	for(;;){` |
|     57 | 9214 | `		if( zIn >= zEnd ){` |
|      - | 9215 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 9216 | `			ph7_result_bool(pCtx,1);` |
|      5 | 9217 | `			return PH7_OK;` |
|      - | 9218 | `		}` |
|     53 | 9219 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 9220 | `			break;` |
|      - | 9221 | `		}` |
|      - | 9222 | `		/* Point to the next character */` |
|     43 | 9223 | `		zIn++;` |
|      1 | 9224 | `	}` |
|      - | 9225 | `	/* The test failed,return FALSE */` |
|     11 | 9226 | `	ph7_result_bool(pCtx,0);` |
|     11 | 9227 | `	return PH7_OK;` |
|      9 | 9228 | `}` |
|      - | 9229 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 9230 | `/*` |
|      - | 9231 | ` * Section:` |
|      - | 9232 | ` *    URL handling Functions.` |
|      - | 9233 | ` * Status:` |
|      - | 9234 | ` *    Stable.` |
|      - | 9235 | ` */` |
|      - | 9236 | `/*` |
|      - | 9237 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 9238 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 9239 | ` */` |
|   1026 | 9240 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 9241 | `{` |
|      - | 9242 | `	/* Store in the call context result buffer */` |
|   1028 | 9243 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 9244 | `	return SXRET_OK;` |
|      2 | 9245 | `}` |
|      - | 9246 | `/*` |
|      - | 9247 | ` * string base64_encode(string $data)` |
|      - | 9248 | ` * string convert_uuencode(string $data)` |
|      - | 9249 | ` *  Encodes data with MIME base64` |
|      - | 9250 | ` * Parameter` |
|      - | 9251 | ` *  $data` |
|      - | 9252 | ` *    Data to encode` |
|      - | 9253 | ` * Return` |
|      - | 9254 | ` *  Encoded data or FALSE on failure.` |
|      - | 9255 | ` */` |
|      6 | 9256 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9257 | `{` |
|      - | 9258 | `	const char *zIn;` |
|      - | 9259 | `	int nLen;` |
|      7 | 9260 | `	if( nArg < 1 ){` |
|      - | 9261 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9262 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9263 | `		return PH7_OK;` |
|      - | 9264 | `	}` |
|      - | 9265 | `	/* Extract the input string */` |
|      7 | 9266 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 9267 | `	if( nLen < 1 ){` |
|      - | 9268 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9269 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9270 | `		return PH7_OK;` |
|      - | 9271 | `	}` |
|      - | 9272 | `	/* Perform the BASE64 encoding */` |
|      7 | 9273 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 9274 | `	return PH7_OK;` |
|      4 | 9275 | `}` |
|      - | 9276 | `/*` |
|      - | 9277 | ` * string base64_decode(string $data)` |
|      - | 9278 | ` * string convert_uudecode(string $data)` |
|      - | 9279 | ` *  Decodes data encoded with MIME base64` |
|      - | 9280 | ` * Parameter` |
|      - | 9281 | ` *  $data` |
|      - | 9282 | ` *    Encoded data.` |
|      - | 9283 | ` * Return` |
|      - | 9284 | ` *  Returns the original data or FALSE on failure.` |
|      - | 9285 | ` */` |
|     34 | 9286 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 9287 | `{` |
|      - | 9288 | `	const char *zIn;` |
|      - | 9289 | `	int nLen;` |
|     36 | 9290 | `	if( nArg < 1 ){` |
|      - | 9291 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9292 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9293 | `		return PH7_OK;` |
|      - | 9294 | `	}` |
|      - | 9295 | `	/* Extract the input string */` |
|     36 | 9296 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 9297 | `	if( nLen < 1 ){` |
|      - | 9298 | `		/* Nothing to process,return FALSE */` |
|      3 | 9299 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9300 | `		return PH7_OK;` |
|      - | 9301 | `	}` |
|      - | 9302 | `	/* Perform the BASE64 decoding */` |
|     34 | 9303 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 9304 | `	return PH7_OK;` |
|     19 | 9305 | `}` |
|      - | 9306 | `/*` |
|      - | 9307 | ` * string urlencode(string $str)` |
|      - | 9308 | ` *  URL encoding` |
|      - | 9309 | ` * Parameter` |
|      - | 9310 | ` *  $data` |
|      - | 9311 | ` *   Input string.` |
|      - | 9312 | ` * Return` |
|      - | 9313 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 9314 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 9315 | ` *  encoded as plus (+) signs.` |
|      - | 9316 | ` */` |
|      4 | 9317 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9318 | `{` |
|      - | 9319 | `	const char *zIn;` |
|      - | 9320 | `	int nLen;` |
|      5 | 9321 | `	if( nArg < 1 ){` |
|      - | 9322 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9323 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9324 | `		return PH7_OK;` |
|      - | 9325 | `	}` |
|      - | 9326 | `	/* Extract the input string */` |
|      5 | 9327 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 9328 | `	if( nLen < 1 ){` |
|      - | 9329 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9330 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9331 | `		return PH7_OK;` |
|      - | 9332 | `	}` |
|      - | 9333 | `	/* Perform the URL encoding */` |
|      5 | 9334 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 9335 | `	return PH7_OK;` |
|      3 | 9336 | `}` |
|      - | 9337 | `/*` |
|      - | 9338 | ` * string urldecode(string $str)` |
|      - | 9339 | ` *  Decodes any %## encoding in the given string.` |
|      - | 9340 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 9341 | ` * Parameter` |
|      - | 9342 | ` *  $data` |
|      - | 9343 | ` *    Input string.` |
|      - | 9344 | ` * Return` |
|      - | 9345 | ` *  Decoded URL or FALSE on failure.` |
|      - | 9346 | ` */` |
|      6 | 9347 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9348 | `{` |
|      - | 9349 | `	const char *zIn;` |
|      - | 9350 | `	int nLen;` |
|      7 | 9351 | `	if( nArg < 1 ){` |
|      - | 9352 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9353 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9354 | `		return PH7_OK;` |
|      - | 9355 | `	}` |
|      - | 9356 | `	/* Extract the input string */` |
|      7 | 9357 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 9358 | `	if( nLen < 1 ){` |
|      - | 9359 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9360 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9361 | `		return PH7_OK;` |
|      - | 9362 | `	}` |
|      - | 9363 | `	/* Perform the URL decoding */` |
|      7 | 9364 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 9365 | `	return PH7_OK;` |
|      4 | 9366 | `}` |
|      - | 9367 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9368 | `/* Table of the built-in functions */` |
|      - | 9369 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 9370 | `	   /* Variable handling functions */` |
|      - | 9371 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 9372 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 9373 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 9374 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 9375 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 9376 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 9377 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 9378 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 9379 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 9380 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 9381 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 9382 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 9383 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 9384 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 9385 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 9386 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 9387 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 9388 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 9389 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 9390 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 9391 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9392 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 9393 | `	   /* Math functions */` |
|      - | 9394 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 9395 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 9396 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 9397 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 9398 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 9399 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 9400 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 9401 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 9402 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 9403 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 9404 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 9405 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 9406 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 9407 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 9408 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 9409 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 9410 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 9411 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 9412 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 9413 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 9414 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 9415 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 9416 | `	{ "round",    PH7_builtin_round        },` |
|      - | 9417 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 9418 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 9419 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 9420 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 9421 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 9422 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 9423 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 9424 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 9425 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 9426 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9427 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9428 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 9429 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9430 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9431 | `	   /* String handling functions */` |
|      - | 9432 |  |
|      - | 9433 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 9434 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 9435 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 9436 | `	{ "substr_replace",  PH7_builtin_substr_replace },` |
|      - | 9437 | `	{ "levenshtein",     PH7_builtin_levenshtein },` |
|      - | 9438 | `	{ "similar_text",    PH7_builtin_similar_text },` |
|      - | 9439 | `	{ "str_word_count",  PH7_builtin_str_word_count },` |
|      - | 9440 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 9441 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 9442 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 9443 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 9444 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 9445 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 9446 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 9447 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 9448 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 9449 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 9450 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 9451 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 9452 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 9453 | `	{ "strnatcmp"  , PH7_builtin_strnatcmp  },` |
|      - | 9454 | `	{ "strnatcasecmp", PH7_builtin_strnatcmp },` |
|      - | 9455 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 9456 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 9457 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 9458 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 9459 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 9460 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 9461 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 9462 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 9463 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 9464 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 9465 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 9466 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 9467 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 9468 | `	{ "mb_strtolower",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */` |
|      - | 9469 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 9470 | `	{ "mb_strtoupper",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */` |
|      - | 9471 | `	{ "mb_strlen",    PH7_builtin_mb_strlen_f },` |
|      - | 9472 | `	{ "mb_substr",    PH7_builtin_mb_substr_f },` |
|      - | 9473 | `	{ "mb_convert_case", PH7_builtin_mb_convert_case_f },` |
|      - | 9474 | `	{ "mb_strpos",    PH7_builtin_mb_strpos_f },` |
|      - | 9475 | `	{ "mb_stripos",   PH7_builtin_mb_strpos_f },` |
|      - | 9476 | `	{ "mb_strrpos",   PH7_builtin_mb_strpos_f },` |
|      - | 9477 | `	{ "mb_str_split", PH7_builtin_mb_str_split_f },` |
|      - | 9478 | `	{ "mb_internal_encoding", PH7_builtin_mb_internal_encoding_f },` |
|      - | 9479 | `	{ "mb_check_encoding",    PH7_builtin_mb_check_encoding_f },` |
|      - | 9480 | `	{ "mb_strwidth",  PH7_builtin_mb_strwidth_f },` |
|      - | 9481 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 9482 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 9483 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 9484 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 9485 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 9486 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 9487 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 9488 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 9489 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 9490 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 9491 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 9492 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 9493 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 9494 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 9495 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 9496 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 9497 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - | 9498 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - | 9499 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - | 9500 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 9501 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9502 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9503 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 9504 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 9505 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 9506 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 9507 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9508 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9509 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 9510 |  |
|      - | 9511 |  |
|      - | 9512 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 9513 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 9514 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 9515 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 9516 | `	{ "hash",         PH7_builtin_hash      },` |
|      - | 9517 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|      - | 9518 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|      - | 9519 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|      - | 9520 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 9521 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|      - | 9522 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|      - | 9523 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|      - | 9524 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|      - | 9525 | `	{ "filter_var",            PH7_builtin_filter_var },` |
|      - | 9526 | `	{ "filter_input",          PH7_builtin_filter_input },` |
|      - | 9527 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9528 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9529 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 9530 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 9531 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9532 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9533 |  |
|      - | 9534 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 9535 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 9536 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 9537 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 9538 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 9539 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 9540 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 9541 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 9542 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 9543 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 9544 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 9545 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 9546 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9547 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9548 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 9549 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9550 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9551 |  |
|      - | 9552 | `	         /* Ctype functions */` |
|      - | 9553 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 9554 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 9555 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 9556 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 9557 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 9558 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 9559 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 9560 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 9561 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 9562 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 9563 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 9564 | `	         /* Time functions */` |
|      - | 9565 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 9566 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 9567 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 9568 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 9569 | `	{ "date",        PH7_builtin_date         },` |
|      - | 9570 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 9571 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 9572 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 9573 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 9574 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 9575 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 9576 | `	{ "date_default_timezone_get", PH7_builtin_date_default_timezone_get },` |
|      - | 9577 | `	{ "date_default_timezone_set", PH7_builtin_date_default_timezone_set },` |
|      - | 9578 | `	        /* URL functions */` |
|      - | 9579 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 9580 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 9581 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 9582 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 9583 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 9584 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 9585 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 9586 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 9587 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9588 | `};` |
|      - | 9589 | `/*` |
|      - | 9590 | ` * Register the built-in functions defined above,the array functions` |
|      - | 9591 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 9592 | ` */` |
|   3560 | 9593 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      5 | 9594 | `{` |
|      - | 9595 | `	sxu32 n;` |
| 662165 | 9596 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 658605 | 9597 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 329305 | 9598 | `	}` |
|      - | 9599 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   3565 | 9600 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 9601 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   3565 | 9602 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   3565 | 9603 | `}` |
|      - | 9604 |  |
