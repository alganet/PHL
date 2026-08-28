# src/ph7/memobj.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 890/1006 lines (88.47%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits | Line | Source |
| -------: | ---: | :--- |
|        - |    1 | `/**` |
|        - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |    5 | ` */` |
|        - |    6 | `#include "ph7int.h" /* This file handle low-level stuff related to indexed memory objects [i.e: ph7_value] */` |
|        - |    7 | `#include <stdio.h>  /* snprintf — the default float->string conversion needs` |
|        - |    8 | `                     * correctly-rounded digits like php (see MemObjStringValue) */` |
|        - |    9 | `#include <stdlib.h> /* strtod — var_dump's shortest-round-trip float shape` |
|        - |   10 | `                     * verifies each candidate by parsing it back */` |
|        - |   11 |  |
|        - |   12 | `/* Portable 64-bit overflow-detecting arithmetic for compilers that lack the` |
|        - |   13 | ` * GCC/Clang __builtin_*_overflow intrinsics (i.e. MSVC). The header exposes` |
|        - |   14 | ` * these through the PH7_{ADD,SUB,MUL}_OVERFLOW64 macros; the intrinsic path` |
|        - |   15 | ` * needs no out-of-line definition, so gate the whole block off there to avoid` |
|        - |   16 | ` * an unused-function warning. Each sets *pR to the two's-complement wrapped` |
|        - |   17 | ` * result and returns non-zero on overflow. The additive checks compute the` |
|        - |   18 | ` * wrapped result via unsigned math (no signed-overflow UB) and test the sign` |
|        - |   19 | ` * bits; the multiplicative check mirrors vm.c's proven bound-check form. */` |
|        - |   20 | `#if !(defined(__GNUC__) \|\| defined(__clang__))` |
|        - |   21 | `PH7_PRIVATE int PH7_AddOverflow64(sxi64 a,sxi64 b,sxi64 *pR)` |
|        5 |   22 | `{` |
|        5 |   23 | `	*pR = (sxi64)((sxu64)a + (sxu64)b);` |
|        - |   24 | `	/* Overflow iff the operands share a sign and the result's sign differs. */` |
|        5 |   25 | `	return ((a ^ *pR) & (b ^ *pR)) < 0;` |
|        5 |   26 | `}` |
|        - |   27 | `PH7_PRIVATE int PH7_SubOverflow64(sxi64 a,sxi64 b,sxi64 *pR)` |
|        5 |   28 | `{` |
|        5 |   29 | `	*pR = (sxi64)((sxu64)a - (sxu64)b);` |
|        - |   30 | `	/* Overflow iff the operands differ in sign and the result's sign differs` |
|        - |   31 | `	 * from the minuend's. */` |
|        5 |   32 | `	return ((a ^ b) & (a ^ *pR)) < 0;` |
|        5 |   33 | `}` |
|        - |   34 | `PH7_PRIVATE int PH7_MulOverflow64(sxi64 a,sxi64 b,sxi64 *pR)` |
|        4 |   35 | `{` |
|        4 |   36 | `	*pR = (sxi64)((sxu64)a * (sxu64)b);` |
|        4 |   37 | `	if( a == 0 \|\| b == 0 \|\| a == 1 \|\| b == 1 ){` |
|        3 |   38 | `		return 0;` |
|        - |   39 | `	}` |
|        4 |   40 | `	if( a == -1 ){` |
|        1 |   41 | `		return b == SMALLEST_INT64;` |
|        - |   42 | `	}` |
|        4 |   43 | `	if( b == -1 ){` |
|      ! 0 |   44 | `		return a == SMALLEST_INT64;` |
|        - |   45 | `	}` |
|        4 |   46 | `	if( a > 0 ){` |
|        4 |   47 | `		if( b > 0 ){` |
|        4 |   48 | `			return a > LARGEST_INT64 / b;` |
|      ! 0 |   49 | `		}else{` |
|        1 |   50 | `			return b < SMALLEST_INT64 / a;` |
|        - |   51 | `		}` |
|      ! 0 |   52 | `	}else{` |
|        1 |   53 | `		if( b > 0 ){` |
|        1 |   54 | `			return a < SMALLEST_INT64 / b;` |
|      ! 0 |   55 | `		}else{` |
|        1 |   56 | `			return b < LARGEST_INT64 / a;` |
|        - |   57 | `		}` |
|        - |   58 | `	}` |
|        4 |   59 | `}` |
|        - |   60 | `#endif` |
|        - |   61 |  |
|        - |   62 | `/* Provide PHP-style type names for values.  This utility may be reused` |
|        - |   63 | ` * by any subsystem that works with ph7_value.` |
|        - |   64 | ` */` |
|      456 |   65 | `PH7_PRIVATE const char *ph7_type_name(ph7_value *pVal)` |
|        5 |   66 | `{` |
|      461 |   67 | `	if( ph7_value_is_null(pVal) ) return "null";` |
|      419 |   68 | `	if( ph7_value_is_bool(pVal) ) return "bool";` |
|      411 |   69 | `	if( ph7_value_is_int(pVal) ) return "int";` |
|      353 |   70 | `	if( ph7_value_is_float(pVal) ) return "float";` |
|      343 |   71 | `	if( ph7_value_is_string(pVal) ) return "string";` |
|      153 |   72 | `	if( ph7_value_is_array(pVal) ) return "array";` |
|       42 |   73 | `	if( ph7_value_is_object(pVal) ) return "object";` |
|        3 |   74 | `	if( ph7_value_is_resource(pVal) ) return "resource";` |
|      ! 0 |   75 | `	return "unknown";` |
|      233 |   76 | `}` |
|        - |   77 |  |
|        - |   78 | `/*` |
|        - |   79 | ` * Notes on memory objects [i.e: ph7_value].` |
|        - |   80 | ` * Internally, the PH7 virtual machine manipulates nearly all PHP values` |
|        - |   81 | ` * [i.e: string,int,float,resource,object,bool,null..] as ph7_values structures.` |
|        - |   82 | ` * Each ph7_values struct may cache multiple representations (string,` |
|        - |   83 | ` * integer etc.) of the same value.` |
|        - |   84 | ` */` |
|        - |   85 | `/*` |
|        - |   86 | ` * Convert a 64-bit IEEE double into a 64-bit signed integer.` |
|        - |   87 | ` * If the double is too large, return 0x8000000000000000.` |
|        - |   88 | ` *` |
|        - |   89 | ` * Most systems appear to do this simply by assigning ariables and without` |
|        - |   90 | ` * the extra range tests.` |
|        - |   91 | ` * But there are reports that windows throws an expection if the floating` |
|        - |   92 | ` * point value is out of range.` |
|        - |   93 | ` */` |
|     3016 |   94 | `static sxi64 MemObjRealToInt(ph7_value *pObj)` |
|        5 |   95 | `{` |
|        - |   96 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |   97 | `	/* Real and 64bit integer are the same when floating point arithmetic` |
|        - |   98 | `	 * is omitted from the build.` |
|        - |   99 | `	 */` |
|        - |  100 | `	return pObj->rVal;` |
|        - |  101 | `#else` |
|        - |  102 | ` /*` |
|        - |  103 | `  ** Many compilers we encounter do not define constants for the` |
|        - |  104 | `  ** minimum and maximum 64-bit integers, or they define them` |
|        - |  105 | `  ** inconsistently.  And many do not understand the "LL" notation.` |
|        - |  106 | `  ** So we define our own static constants here using nothing` |
|        - |  107 | `  ** larger than a 32-bit integer constant.` |
|        - |  108 | `  */` |
|        - |  109 | `  static const sxi64 maxInt = LARGEST_INT64;` |
|        - |  110 | `  static const sxi64 minInt = SMALLEST_INT64;` |
|     3021 |  111 | `  ph7_real r = pObj->rVal;` |
|     3021 |  112 | `  if( r<(ph7_real)minInt ){` |
|        3 |  113 | `    return minInt;` |
|     3019 |  114 | `  }else if( r>(ph7_real)maxInt ){` |
|        - |  115 | `    /* minInt is correct here - not maxInt.  It turns out that assigning` |
|        - |  116 | `    ** a very large positive number to an integer results in a very large` |
|        - |  117 | `    ** negative integer.  This makes no sense, but it is what x86 hardware` |
|        - |  118 | `    ** does so for compatibility we will do the same in software. */` |
|      157 |  119 | `    return minInt;` |
|      ! 0 |  120 | `  }else{` |
|     2863 |  121 | `    return (sxi64)r;` |
|        - |  122 | `  }` |
|        - |  123 | `#endif` |
|     1513 |  124 | `}` |
|        - |  125 | `/*` |
|        - |  126 | ` * Convert a raw token value typically a stream of digit [i.e: hex,octal,binary or decimal]` |
|        - |  127 | ` * to a 64-bit integer.` |
|        - |  128 | ` */` |
|  1292666 |  129 | `PH7_PRIVATE sxi64 PH7_TokenValueToInt64(SyString *pVal)` |
|        5 |  130 | `{` |
|  1292671 |  131 | `	sxi64 iVal = 0;` |
|  1292671 |  132 | `	if( pVal->nByte <= 0 ){` |
|        7 |  133 | `		return 0;` |
|        - |  134 | `	}` |
|  1292665 |  135 | `	if( pVal->zString[0] == '0' ){` |
|        - |  136 | `		sxi32 c;` |
|   358778 |  137 | `		if( pVal->nByte == sizeof(char) ){` |
|   358363 |  138 | `			return 0;` |
|        - |  139 | `		}` |
|      416 |  140 | `		c = pVal->zString[1];` |
|      416 |  141 | `		if( c  == 'x' \|\| c == 'X' ){` |
|        - |  142 | `			/* Hex digit stream */` |
|       71 |  143 | `			SyHexStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      381 |  144 | `		}else if( c == 'b' \|\| c == 'B' ){` |
|        - |  145 | `			/* Binary digit stream */` |
|      279 |  146 | `			SyBinaryStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      140 |  147 | `		}else{` |
|        - |  148 | `			/* Octal digit stream */` |
|       68 |  149 | `			SyOctalStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|        - |  150 | `		}` |
|      208 |  151 | `	}else{` |
|        - |  152 | `		/* Decimal digit stream */` |
|   933892 |  153 | `		SyStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|        - |  154 | `	}` |
|   934307 |  155 | `	return iVal;` |
|   646338 |  156 | `}` |
|        - |  157 | `/*` |
|        - |  158 | ` * Return some kind of 64-bit integer value which is the best we can` |
|        - |  159 | ` * do at representing the value that pObj describes as a string` |
|        - |  160 | ` * representation.` |
|        - |  161 | ` */` |
|      530 |  162 | `static sxi64 MemObjStringToInt(ph7_value *pObj)` |
|        5 |  163 | `{` |
|        - |  164 | `	SyString sVal;` |
|      535 |  165 | `	SyStringInitFromBuf(&sVal,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|      535 |  166 | `	return PH7_TokenValueToInt64(&sVal);` |
|        5 |  167 | `}` |
|        - |  168 | `/*` |
|        - |  169 | ` * Call a magic class method [i.e: __toString(),__toInt(),...]` |
|        - |  170 | ` * Return SXRET_OK if the magic method is available and have been` |
|        - |  171 | ` * successfully called. Any other return value indicates failure.` |
|        - |  172 | ` */` |
|      188 |  173 | `static sxi32 MemObjCallClassCastMethod(` |
|        - |  174 | `	ph7_vm *pVm,               /* VM that trigger the invocation */` |
|        - |  175 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object] */` |
|        - |  176 | `	const char *zMethod,       /* Magic method name [i.e: __toString] */` |
|        - |  177 | `	sxu32 nLen,                /* Method name length */` |
|        - |  178 | `	ph7_value *pResult         /* OUT: Store the return value of the magic method here */` |
|        - |  179 | `	)` |
|        5 |  180 | `{` |
|        - |  181 | `	ph7_class_method *pMethod;` |
|        - |  182 | `	/* Check if the method is available */` |
|      193 |  183 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,zMethod,nLen);` |
|      193 |  184 | `	if( pMethod == 0 ){` |
|        - |  185 | `		/* No such method */` |
|        6 |  186 | `		return SXERR_NOTFOUND;` |
|        - |  187 | `	}` |
|        - |  188 | `	/* Invoke the desired method */` |
|      189 |  189 | `	PH7_VmCallClassMethod(&(*pVm),&(*pThis),pMethod,&(*pResult),0,0);` |
|        - |  190 | `	/* Method successfully called,pResult should hold the return value */` |
|      189 |  191 | `	return SXRET_OK;` |
|       99 |  192 | `}` |
|        - |  193 | `/*` |
|        - |  194 | ` * Return some kind of integer value which is the best we can` |
|        - |  195 | ` * do at representing the value that pObj describes as an integer.` |
|        - |  196 | ` * If pObj is an integer, then the value is exact. If pObj is` |
|        - |  197 | ` * a floating-point then  the value returned is the integer part.` |
|        - |  198 | ` * If pObj is a string, then we make an attempt to convert it into` |
|        - |  199 | ` * a integer and return that.` |
|        - |  200 | ` * If pObj represents a NULL value, return 0.` |
|        - |  201 | ` */` |
|      608 |  202 | `static sxi64 MemObjIntValue(ph7_value *pObj)` |
|        5 |  203 | `{` |
|        - |  204 | `	sxi32 iFlags;` |
|      613 |  205 | `	iFlags = pObj->iFlags;` |
|      613 |  206 | `	if (iFlags & MEMOBJ_REAL ){` |
|       41 |  207 | `		return MemObjRealToInt(&(*pObj));` |
|      573 |  208 | `	}else if( iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      160 |  209 | `		return pObj->x.iVal;` |
|      415 |  210 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|      393 |  211 | `		return MemObjStringToInt(&(*pObj));` |
|       23 |  212 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|       11 |  213 | `		return 0;` |
|       13 |  214 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  215 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        7 |  216 | `		sxu32 n = pMap->nEntry;` |
|        7 |  217 | `		PH7_HashmapUnref(pMap);` |
|        - |  218 | `		/* Return total number of entries in the hashmap */` |
|        7 |  219 | `		return n;` |
|        7 |  220 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|        - |  221 | `		ph7_value sResult;` |
|        5 |  222 | `		sxi64 iVal = 1;` |
|        - |  223 | `		sxi32 rc;` |
|        - |  224 | `		/* Invoke the [__toInt()] magic method if available [note that this is a symisc extension]  */` |
|        5 |  225 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|        5 |  226 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  227 | `			"__toInt",sizeof("__toInt")-1,&sResult);` |
|        5 |  228 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_INT) ){` |
|        - |  229 | `			/* Extract method return value */` |
|        5 |  230 | `			iVal = sResult.x.iVal;` |
|        2 |  231 | `		}` |
|        5 |  232 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|        5 |  233 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  234 | `		return iVal;` |
|        3 |  235 | `	}else if(iFlags & MEMOBJ_RES ){` |
|        3 |  236 | `		return pObj->x.pOther != 0;` |
|        - |  237 | `	}` |
|        - |  238 | `	/* CANT HAPPEN */` |
|      ! 0 |  239 | `	return 0;` |
|      309 |  240 | `}` |
|        - |  241 | `/*` |
|        - |  242 | ` * Return some kind of real value which is the best we can` |
|        - |  243 | ` * do at representing the value that pObj describes as a real.` |
|        - |  244 | ` * If pObj is a real, then the value is exact.If pObj is an` |
|        - |  245 | ` * integer then the integer  is promoted to real and that value` |
|        - |  246 | ` * is returned.` |
|        - |  247 | ` * If pObj is a string, then we make an attempt to convert it` |
|        - |  248 | ` * into a real and return that.` |
|        - |  249 | ` * If pObj represents a NULL value, return 0.0` |
|        - |  250 | ` */` |
|     1790 |  251 | `static ph7_real MemObjRealValue(ph7_value *pObj)` |
|        5 |  252 | `{` |
|        - |  253 | `	sxi32 iFlags;` |
|     1795 |  254 | `	iFlags = pObj->iFlags;` |
|     1795 |  255 | `	if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  256 | `		return pObj->rVal;` |
|     1795 |  257 | `	}else if (iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      772 |  258 | `		return (ph7_real)pObj->x.iVal;` |
|     1025 |  259 | `	}else if (iFlags & MEMOBJ_STRING){` |
|        - |  260 | `		SyString sString;` |
|        - |  261 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  262 | `		ph7_real rVal = 0;` |
|        - |  263 | `#else` |
|     1019 |  264 | `		ph7_real rVal = 0.0;` |
|        - |  265 | `#endif` |
|     1019 |  266 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|     1019 |  267 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        - |  268 | `			/* Convert as much as we can */` |
|        - |  269 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  270 | `			rVal = MemObjStringToInt(&(*pObj));` |
|        - |  271 | `#else` |
|     1019 |  272 | `			SyStrToReal(sString.zString,sString.nByte,(void *)&rVal,0);` |
|        - |  273 | `#endif` |
|      507 |  274 | `		}` |
|     1019 |  275 | `		return rVal;` |
|        7 |  276 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        - |  277 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  278 | `		return 0;` |
|        - |  279 | `#else` |
|      ! 0 |  280 | `		return 0.0;` |
|        - |  281 | `#endif` |
|        7 |  282 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        - |  283 | `		/* Return the total number of entries in the hashmap */` |
|      ! 0 |  284 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      ! 0 |  285 | `		ph7_real n = (ph7_real)pMap->nEntry;` |
|      ! 0 |  286 | `		PH7_HashmapUnref(pMap);` |
|      ! 0 |  287 | `		return n;` |
|        7 |  288 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|        - |  289 | `		ph7_value sResult;` |
|        5 |  290 | `		ph7_real rVal = 1;` |
|        - |  291 | `		sxi32 rc;` |
|        - |  292 | `		/* Invoke the [__toFloat()] magic method if available [note that this is a symisc extension]  */` |
|        5 |  293 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|        5 |  294 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  295 | `			"__toFloat",sizeof("__toFloat")-1,&sResult);` |
|        5 |  296 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_REAL) ){` |
|        - |  297 | `			/* Extract method return value */` |
|        5 |  298 | `			rVal = sResult.rVal;` |
|        2 |  299 | `		}` |
|        5 |  300 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|        5 |  301 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  302 | `		return rVal;` |
|        3 |  303 | `	}else if(iFlags & MEMOBJ_RES ){` |
|        3 |  304 | `		return (ph7_real)(pObj->x.pOther != 0);` |
|        - |  305 | `	}` |
|        - |  306 | `	/* NOT REACHED  */` |
|      ! 0 |  307 | `	return 0;` |
|      900 |  308 | `}` |
|        - |  309 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  310 | `/*` |
|        - |  311 | ` * Post-process a libc-formatted float into php's exact shape (php_gcvt /` |
|        - |  312 | ` * smart_str_append_double semantics): strip the exponent's zero padding` |
|        - |  313 | ` * (libc's 1e+08 becomes php's 1e+8; a zero exponent stays e+0) and, when` |
|        - |  314 | ` * bGeneric is set (%g-style output, including the default float->string` |
|        - |  315 | ` * cast), make an exponent-form mantissa keep a fractional digit` |
|        - |  316 | ` * (1e+20 -> 1.0e+20). zBuf must be NUL-terminated with at least two bytes` |
|        - |  317 | ` * of spare capacity past the NUL. Returns the new length.` |
|        - |  318 | ` * Defined here (not builtin.c) because the float->string cast below needs it` |
|        - |  319 | ` * even when builtin.c's formatting region is compiled out` |
|        - |  320 | ` * (PH7_DISABLE_DISK_IO); the printf family reuses it from PH7_InputFormat.` |
|        - |  321 | ` */` |
|      500 |  322 | `PH7_PRIVATE sxi32 PH7_PhpFloatShape(char *zBuf,sxi32 nLen,int bGeneric)` |
|        3 |  323 | `{` |
|        - |  324 | `	sxi32 iExp,i;` |
|      503 |  325 | `	iExp = nLen - 1;` |
|     4199 |  326 | `	while( iExp > 0 && zBuf[iExp] != 'e' && zBuf[iExp] != 'E' ){` |
|     3699 |  327 | `		iExp--;` |
|        3 |  328 | `	}` |
|      503 |  329 | `	if( iExp <= 0 ){` |
|      457 |  330 | `		return nLen; /* No exponent part (fixed notation) */` |
|        - |  331 | `	}` |
|        - |  332 | `	{` |
|       47 |  333 | `		sxi32 iDig = iExp + 1;` |
|        - |  334 | `		sxi32 iFirst;` |
|       47 |  335 | `		if( zBuf[iDig] == '+' \|\| zBuf[iDig] == '-' ){` |
|       47 |  336 | `			iDig++;` |
|       23 |  337 | `		}` |
|       47 |  338 | `		iFirst = iDig;` |
|       83 |  339 | `		while( zBuf[iFirst] == '0' && iFirst + 1 < nLen` |
|       61 |  340 | `		 && zBuf[iFirst+1] >= '0' && zBuf[iFirst+1] <= '9' ){` |
|       25 |  341 | `			iFirst++;` |
|        1 |  342 | `		}` |
|       47 |  343 | `		if( iFirst > iDig ){` |
|       25 |  344 | `			sxi32 nStrip = iFirst - iDig;` |
|       73 |  345 | `			for( i = iDig ; i + nStrip <= nLen ; i++ ){` |
|       49 |  346 | `				zBuf[i] = zBuf[i+nStrip]; /* moves the NUL too */` |
|       25 |  347 | `			}` |
|       25 |  348 | `			nLen -= nStrip;` |
|       12 |  349 | `		}` |
|        - |  350 | `	}` |
|       47 |  351 | `	if( bGeneric ){` |
|       31 |  352 | `		int bHasDot = 0;` |
|       63 |  353 | `		for( i = 0 ; i < iExp ; i++ ){` |
|       45 |  354 | `			if( zBuf[i] == '.' ){ bHasDot = 1; break; }` |
|       17 |  355 | `		}` |
|       31 |  356 | `		if( !bHasDot ){` |
|      107 |  357 | `			for( i = nLen ; i >= iExp ; i-- ){` |
|       89 |  358 | `				zBuf[i+2] = zBuf[i]; /* moves the NUL too */` |
|       45 |  359 | `			}` |
|       19 |  360 | `			zBuf[iExp] = '.';` |
|       19 |  361 | `			zBuf[iExp+1] = '0';` |
|       19 |  362 | `			nLen += 2;` |
|        9 |  363 | `		}` |
|       15 |  364 | `	}` |
|       47 |  365 | `	return nLen;` |
|      253 |  366 | `}` |
|        - |  367 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|        - |  368 | `/*` |
|        - |  369 | ` * Return the string representation of a given ph7_value.` |
|        - |  370 | ` * This function never fail and always return SXRET_OK.` |
|        - |  371 | ` */` |
|    58052 |  372 | `static sxi32 MemObjStringValue(SyBlob *pOut,ph7_value *pObj,sxu8 bStrictBool)` |
|        5 |  373 | `{` |
|    58057 |  374 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - |  375 | `		/* Handle special floating-point values first */` |
|      383 |  376 | `		if( PH7_IS_NAN(pObj->rVal) ){` |
|      ! 0 |  377 | `			SyBlobAppend(&(*pOut),"NAN",3);` |
|      383 |  378 | `		}else if( PH7_IS_INF(pObj->rVal) ){` |
|        5 |  379 | `			if( pObj->rVal < 0.0 ){` |
|      ! 0 |  380 | `				SyBlobAppend(&(*pOut),"-INF",4);` |
|      ! 0 |  381 | `			}else{` |
|        5 |  382 | `				SyBlobAppend(&(*pOut),"INF",3);` |
|        - |  383 | `			}` |
|        3 |  384 | `		}else{` |
|        - |  385 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  386 | `			/* php's default float->string conversion (echo/concat/cast):` |
|        - |  387 | `			 * zend_gcvt with EG(precision)=14 and an uppercase exponent` |
|        - |  388 | `			 * marker (smart_str_append_double) — 1/3 -> "0.33333333333333",` |
|        - |  389 | `			 * 1e15 -> "1.0E+15", -0.0 -> "-0". libc snprintf supplies` |
|        - |  390 | `			 * correctly-rounded digits; PH7_PhpFloatShape applies php's` |
|        - |  391 | `			 * exponent/fraction quirks. */` |
|        - |  392 | `			char zNum[48]; /* %.14G peaks at ~22 bytes; +2 spare for ".0" */` |
|      379 |  393 | `			sxi32 n = (sxi32)snprintf(zNum,sizeof(zNum),"%.14G",pObj->rVal);` |
|      379 |  394 | `			if( n < 0 \|\| n >= (sxi32)sizeof(zNum) ){` |
|      ! 0 |  395 | `				n = (sxi32)SyStrlen(zNum);` |
|      ! 0 |  396 | `			}` |
|      379 |  397 | `			n = PH7_PhpFloatShape(zNum,n,TRUE);` |
|      379 |  398 | `			SyBlobAppend(&(*pOut),zNum,(sxu32)n);` |
|        - |  399 | `#else` |
|        - |  400 | `			SyBlobFormat(&(*pOut),"%.15g",pObj->rVal);` |
|        - |  401 | `#endif` |
|        3 |  402 | `		}` |
|    57867 |  403 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|    57139 |  404 | `		SyBlobFormat(&(*pOut),"%qd",pObj->x.iVal);` |
|        - |  405 | `		/* %qd (BSD quad) is equivalent to %lld in the libc printf */` |
|    29110 |  406 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|      269 |  407 | `		if( bStrictBool ){` |
|        - |  408 | `			/* Actual string cast: true -> "1", false -> "" (like PHP) */` |
|      269 |  409 | `			if( pObj->x.iVal ){` |
|       28 |  410 | `				SyBlobAppend(&(*pOut),"1",sizeof("1")-1);` |
|       13 |  411 | `			}` |
|        - |  412 | `			/* false produces empty string, nothing to append */` |
|      137 |  413 | `		}else{` |
|        - |  414 | `			/* Display path (var_dump, print_r): show TRUE/FALSE */` |
|      ! 0 |  415 | `			if( pObj->x.iVal ){` |
|      ! 0 |  416 | `				SyBlobAppend(&(*pOut),"TRUE",sizeof("TRUE")-1);` |
|      ! 0 |  417 | `			}else{` |
|      ! 0 |  418 | `				SyBlobAppend(&(*pOut),"FALSE",sizeof("FALSE")-1);` |
|        - |  419 | `			}` |
|        5 |  420 | `		}` |
|      411 |  421 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 |  422 | `		SyBlobAppend(&(*pOut),"Array",sizeof("Array")-1);` |
|      ! 0 |  423 | `		PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|      279 |  424 | `	}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|        - |  425 | `		ph7_value sResult;` |
|        - |  426 | `		sxi32 rc;` |
|        - |  427 | `		/* Invoke the __toString() method if available */` |
|      179 |  428 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|      179 |  429 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  430 | `			"__toString",sizeof("__toString")-1,&sResult);` |
|      179 |  431 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_STRING) && SyBlobLength(&sResult.sBlob) > 0){` |
|        - |  432 | `			/* Expand method return value */` |
|      100 |  433 | `			SyBlobDup(&sResult.sBlob,pOut);` |
|       52 |  434 | `		}else{` |
|        - |  435 | `			/* Expand "Object" as requested by the PHP language reference manual */` |
|       81 |  436 | `			SyBlobAppend(&(*pOut),"Object",sizeof("Object")-1);` |
|        - |  437 | `		}` |
|      179 |  438 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|      179 |  439 | `		PH7_MemObjRelease(&sResult);` |
|      190 |  440 | `	}else if(pObj->iFlags & MEMOBJ_RES ){` |
|        3 |  441 | `		SyBlobFormat(&(*pOut),"ResourceID_%#x",pObj->x.pOther);` |
|        1 |  442 | `	}` |
|    58057 |  443 | `	return SXRET_OK;` |
|        5 |  444 | `}` |
|        - |  445 | `/*` |
|        - |  446 | ` * Return some kind of boolean value which is the best we can do` |
|        - |  447 | ` * at representing the value that pObj describes as a boolean.` |
|        - |  448 | ` * When converting to boolean, the following values are considered FALSE` |
|        - |  449 | ` * (php's exact set):` |
|        - |  450 | ` * NULL` |
|        - |  451 | ` * the boolean FALSE itself.` |
|        - |  452 | ` * the integer 0 (zero).` |
|        - |  453 | ` * the real 0.0 (zero).` |
|        - |  454 | ` * the empty string "" and the string "0" (nothing else: "00", "0.0", " ",` |
|        - |  455 | ` * and "false" are all TRUE in php — the historical PH7 zero-stream and` |
|        - |  456 | ` * "false"/"on"/"yes" special cases changed the meaning of valid PHP source` |
|        - |  457 | ` * and were removed under the §10 PH7-ism policy).` |
|        - |  458 | ` * an array with zero elements.` |
|        - |  459 | ` */` |
|    16384 |  460 | `static sxi32 MemObjBooleanValue(ph7_value *pObj)` |
|        5 |  461 | `{` |
|        - |  462 | `	sxi32 iFlags;` |
|    16389 |  463 | `	iFlags = pObj->iFlags;` |
|    16389 |  464 | `	if (iFlags & MEMOBJ_REAL ){` |
|        - |  465 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  466 | `		return pObj->rVal ? 1 : 0;` |
|        - |  467 | `#else` |
|       14 |  468 | `		return pObj->rVal != 0.0 ? 1 : 0;` |
|        - |  469 | `#endif` |
|    16377 |  470 | `	}else if( iFlags & MEMOBJ_INT ){` |
|      287 |  471 | `		return pObj->x.iVal ? 1 : 0;` |
|    16095 |  472 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|        - |  473 | `		SyString sString;` |
|      111 |  474 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|        - |  475 | `		/* php: a string is FALSE iff it is empty or exactly "0" */` |
|      111 |  476 | `		if( sString.nByte == 0 ){` |
|       19 |  477 | `			return 0;` |
|        - |  478 | `		}` |
|       94 |  479 | `		if( sString.nByte == 1 && sString.zString[0] == '0' ){` |
|        7 |  480 | `			return 0;` |
|        - |  481 | `		}` |
|       88 |  482 | `		return 1;` |
|    15987 |  483 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|    14935 |  484 | `		return 0;` |
|     1057 |  485 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|       22 |  486 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       22 |  487 | `		sxu32 n = pMap->nEntry;` |
|       22 |  488 | `		PH7_HashmapUnref(pMap);` |
|       22 |  489 | `		return n > 0 ? TRUE : FALSE;` |
|     1037 |  490 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|        - |  491 | `		ph7_value sResult;` |
|        7 |  492 | `		sxi32 iVal = 1;` |
|        - |  493 | `		sxi32 rc;` |
|        - |  494 | `		/* Invoke the __toBool() method if available [note that this is a symisc extension]  */` |
|        7 |  495 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|        7 |  496 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  497 | `			"__toBool",sizeof("__toBool")-1,&sResult);` |
|        7 |  498 | `		if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL)) ){` |
|        - |  499 | `			/* Extract method return value */` |
|        5 |  500 | `			iVal = (sxi32)(sResult.x.iVal != 0); /* Stupid cc warning -W -Wall -O6 */` |
|        2 |  501 | `		}` |
|        7 |  502 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|        7 |  503 | `		PH7_MemObjRelease(&sResult);` |
|        7 |  504 | `		return iVal;` |
|     1031 |  505 | `	}else if(iFlags & MEMOBJ_RES ){` |
|     1031 |  506 | `		return pObj->x.pOther != 0;` |
|        - |  507 | `	}` |
|        - |  508 | `	/* NOT REACHED */` |
|      ! 0 |  509 | `	return 0;` |
|     8197 |  510 | `}` |
|        - |  511 | `/*` |
|        - |  512 | ` * If the ph7_value is of type real,try to make it an integer also.` |
|        - |  513 | ` */` |
|     2976 |  514 | `static sxi32 MemObjTryIntger(ph7_value *pObj)` |
|        5 |  515 | `{` |
|     2981 |  516 | `	pObj->x.iVal = MemObjRealToInt(&(*pObj));` |
|        - |  517 | `  /* Only mark the value as an integer if` |
|        - |  518 | `  **` |
|        - |  519 | `  **    (1) the round-trip conversion real->int->real is a no-op, and` |
|        - |  520 | `  **    (2) The integer is neither the largest nor the smallest` |
|        - |  521 | `  **        possible integer` |
|        - |  522 | `  **` |
|        - |  523 | `  ** The second and third terms in the following conditional enforces` |
|        - |  524 | `  ** the second condition under the assumption that addition overflow causes` |
|        - |  525 | `  ** values to wrap around.  On x86 hardware, the third term is always` |
|        - |  526 | `  ** true and could be omitted.  But we leave it in because other` |
|        - |  527 | `  ** architectures might behave differently.` |
|        - |  528 | `  */` |
|     2976 |  529 | `	if( pObj->rVal ==(ph7_real)pObj->x.iVal && pObj->x.iVal>SMALLEST_INT64` |
|     1557 |  530 | `      && pObj->x.iVal<LARGEST_INT64 ){` |
|     1540 |  531 | `		  pObj->iFlags \|= MEMOBJ_INT;` |
|      770 |  532 | `	}` |
|     2981 |  533 | `	return SXRET_OK;` |
|        5 |  534 | `}` |
|        - |  535 | `/*` |
|        - |  536 | ` * Convert a ph7_value to type integer.Invalidate any prior representations.` |
|        - |  537 | ` */` |
|   443810 |  538 | `PH7_PRIVATE sxi32 PH7_MemObjToInteger(ph7_value *pObj)` |
|        5 |  539 | `{` |
|   443815 |  540 | `	if( (pObj->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  541 | `		/* Preform the conversion */` |
|      613 |  542 | `		pObj->x.iVal = MemObjIntValue(&(*pObj));` |
|        - |  543 | `		/* Invalidate any prior representations */` |
|      613 |  544 | `		SyBlobRelease(&pObj->sBlob);` |
|      613 |  545 | `		MemObjSetType(pObj,MEMOBJ_INT);` |
|      304 |  546 | `	}` |
|   443815 |  547 | `	return SXRET_OK;` |
|        5 |  548 | `}` |
|        - |  549 | `/*` |
|        - |  550 | ` * Convert a ph7_value to type real (Try to get an integer representation also).` |
|        - |  551 | ` * Invalidate any prior representations` |
|        - |  552 | ` */` |
|     2704 |  553 | `PH7_PRIVATE sxi32 PH7_MemObjToReal(ph7_value *pObj)` |
|        5 |  554 | `{` |
|     2709 |  555 | `	if((pObj->iFlags & MEMOBJ_REAL) == 0 ){` |
|        - |  556 | `		/* Preform the conversion */` |
|     1795 |  557 | `		pObj->rVal = MemObjRealValue(&(*pObj));` |
|        - |  558 | `		/* Invalidate any prior representations */` |
|     1795 |  559 | `		SyBlobRelease(&pObj->sBlob);` |
|     1795 |  560 | `		MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - |  561 | `		/* Try to get an integer representation */` |
|     1795 |  562 | `		MemObjTryIntger(&(*pObj));` |
|      895 |  563 | `	}` |
|     2709 |  564 | `	return SXRET_OK;` |
|        5 |  565 | `}` |
|        - |  566 | `/*` |
|        - |  567 | ` * Convert a ph7_value to type boolean.Invalidate any prior representations.` |
|        - |  568 | ` */` |
|    18676 |  569 | `PH7_PRIVATE sxi32 PH7_MemObjToBool(ph7_value *pObj)` |
|        5 |  570 | `{` |
|    18681 |  571 | `	if( (pObj->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        - |  572 | `		/* Preform the conversion */` |
|    16389 |  573 | `		pObj->x.iVal = MemObjBooleanValue(&(*pObj));` |
|        - |  574 | `		/* Invalidate any prior representations */` |
|    16389 |  575 | `		SyBlobRelease(&pObj->sBlob);` |
|    16389 |  576 | `		MemObjSetType(pObj,MEMOBJ_BOOL);` |
|     8192 |  577 | `	}` |
|    18681 |  578 | `	return SXRET_OK;` |
|        5 |  579 | `}` |
|        - |  580 | `/*` |
|        - |  581 | ` * Convert a ph7_value to type string.Prior representations are NOT invalidated.` |
|        - |  582 | ` */` |
|   858587 |  583 | `PH7_PRIVATE sxi32 PH7_MemObjToString(ph7_value *pObj)` |
|        5 |  584 | `{` |
|   858592 |  585 | `	sxi32 rc = SXRET_OK;` |
|   858592 |  586 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  587 | `		/* Perform the conversion */` |
|    58041 |  588 | `		SyBlobReset(&pObj->sBlob); /* Reset the internal buffer */` |
|    58041 |  589 | `		rc = MemObjStringValue(&pObj->sBlob,&(*pObj),TRUE);` |
|    58041 |  590 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|    29018 |  591 | `	}` |
|   858592 |  592 | `	return rc;` |
|        5 |  593 | `}` |
|        - |  594 | `/*` |
|        - |  595 | ` * Nullify a ph7_value.In other words invalidate any prior` |
|        - |  596 | ` * representation.` |
|        - |  597 | ` */` |
|      ! 0 |  598 | `PH7_PRIVATE sxi32 PH7_MemObjToNull(ph7_value *pObj)` |
|      ! 0 |  599 | `{` |
|      ! 0 |  600 | `	return PH7_MemObjRelease(pObj);` |
|      ! 0 |  601 | `}` |
|        - |  602 | `/*` |
|        - |  603 | ` * Convert a ph7_value to type array.Invalidate any prior representations.` |
|        - |  604 | `  * According to the PHP language reference manual.` |
|        - |  605 | `  *   For any of the types: integer, float, string, boolean converting a value` |
|        - |  606 | `  *   to an array results in an array with a single element with index zero` |
|        - |  607 | `  *   and the value of the scalar which was converted.` |
|        - |  608 | `  */` |
|      538 |  609 | `PH7_PRIVATE sxi32 PH7_MemObjToHashmap(ph7_value *pObj)` |
|        4 |  610 | `{` |
|      542 |  611 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  612 | `		ph7_hashmap *pMap;` |
|        - |  613 | `		/* Allocate a new hashmap instance */` |
|      350 |  614 | `		pMap = PH7_NewHashmap(pObj->pVm,0,0);` |
|      350 |  615 | `		if( pMap == 0 ){` |
|      ! 0 |  616 | `			return SXERR_MEM;` |
|        - |  617 | `		}` |
|      350 |  618 | `		if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|        - |  619 | `			/*` |
|        - |  620 | `			 * According to the PHP language reference manual.` |
|        - |  621 | `			 *   For any of the types: integer, float, string, boolean converting a value` |
|        - |  622 | `			 *   to an array results in an array with a single element with index zero` |
|        - |  623 | `			 *   and the value of the scalar which was converted.` |
|        - |  624 | `			 */` |
|       27 |  625 | `			if( pObj->iFlags & MEMOBJ_OBJ ){` |
|        - |  626 | `				/* Object cast */` |
|       15 |  627 | `				PH7_ClassInstanceToHashmap((ph7_class_instance *)pObj->x.pOther,pMap);` |
|        8 |  628 | `			}else{` |
|        - |  629 | `				/* Insert a single element */` |
|       13 |  630 | `				PH7_HashmapInsert(pMap,0/* Automatic index assign */,&(*pObj));` |
|        - |  631 | `			}` |
|       27 |  632 | `			SyBlobRelease(&pObj->sBlob);` |
|       13 |  633 | `		}` |
|        - |  634 | `		/* Invalidate any prior representation */` |
|      350 |  635 | `		PH7_MemObjRelease(pObj);` |
|      350 |  636 | `		MemObjSetType(pObj,MEMOBJ_HASHMAP);` |
|      350 |  637 | `		pObj->x.pOther = pMap;` |
|      173 |  638 | `	}` |
|      542 |  639 | `	return SXRET_OK;` |
|      273 |  640 | `}` |
|        - |  641 | `/* Per-entry callback for the array branch of the (object) cast: add one dynamic` |
|        - |  642 | ` * property to the target stdClass, named by the array key (rendered as a string,` |
|        - |  643 | ` * matching PHP) and holding a copy of the value. */` |
|        - |  644 | `struct VmObjCastData { ph7_vm *pVm; ph7_class_instance *pStd; };` |
|       38 |  645 | `static int VmArrayToObjectWalk(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  646 | `{` |
|       39 |  647 | `	struct VmObjCastData *pData = (struct VmObjCastData *)pUserData;` |
|        - |  648 | `	ph7_value *pSlot;` |
|        - |  649 | `	/* pKey and pValue are walk-owned temporaries (PH7_HashmapWalk passes pointers to` |
|        - |  650 | `	 * its own stack-local sKey/sValue, not slots inside pVm->aMemObj), so they survive` |
|        - |  651 | `	 * the slot reservation inside PH7_VmCreateDynamicAttr — no snapshot needed. pKey is` |
|        - |  652 | `	 * safe to coerce in place. */` |
|       39 |  653 | `	PH7_MemObjToString(pKey);` |
|       58 |  654 | `	pSlot = PH7_VmCreateDynamicAttr(pData->pVm,pData->pStd,` |
|       38 |  655 | `		(const char *)SyBlobData(&pKey->sBlob),(sxu32)SyBlobLength(&pKey->sBlob),0);` |
|       39 |  656 | `	if( pSlot ){` |
|       39 |  657 | `		PH7_MemObjStore(pValue,pSlot);` |
|       19 |  658 | `	}` |
|       39 |  659 | `	return SXRET_OK;` |
|        1 |  660 | `}` |
|        - |  661 | `/*` |
|        - |  662 | ` * Convert a ph7_value to type object, invalidating any prior representation.` |
|        - |  663 | ` * The new object is a (PHP-empty) stdClass populated with dynamic properties,` |
|        - |  664 | ` * matching PHP's (object) cast:` |
|        - |  665 | ` *   - array  -> one property per entry (key rendered as a string -> name).` |
|        - |  666 | ` *   - scalar -> a single property named "scalar".` |
|        - |  667 | ` *   - null   -> an empty stdClass (no properties).` |
|        - |  668 | ` *   - object -> returned unchanged (the MEMOBJ_OBJ guard below).` |
|        - |  669 | ` */` |
|       34 |  670 | `PH7_PRIVATE sxi32 PH7_MemObjToObject(ph7_value *pObj)` |
|        1 |  671 | `{` |
|       35 |  672 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  673 | `		ph7_class_instance *pStd;` |
|        - |  674 | `		ph7_class *pClass;` |
|        - |  675 | `		ph7_vm *pVm;` |
|        - |  676 | `		/* Point to the underlying VM + the stdClass */` |
|       35 |  677 | `		pVm = pObj->pVm;` |
|       52 |  678 | `		pClass = pVm->pStdClass ? pVm->pStdClass` |
|       17 |  679 | `			: PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|       35 |  680 | `		if( pClass == 0 ){` |
|        - |  681 | `			/* Can't happen,load null instead */` |
|      ! 0 |  682 | `			PH7_MemObjRelease(pObj);` |
|      ! 0 |  683 | `			return SXRET_OK;` |
|        - |  684 | `		}` |
|        - |  685 | `		/* Instanciate a new (empty) stdClass object */` |
|       35 |  686 | `		pStd = PH7_NewClassInstance(pVm,pClass);` |
|       35 |  687 | `		if( pStd == 0 ){` |
|        - |  688 | `			/* Out of memory */` |
|      ! 0 |  689 | `			PH7_MemObjRelease(pObj);` |
|      ! 0 |  690 | `			return SXRET_OK;` |
|        - |  691 | `		}` |
|       35 |  692 | `		pStd->iRef = 1;` |
|       35 |  693 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  694 | `			/* Array: one dynamic property per entry. */` |
|        - |  695 | `			struct VmObjCastData sData;` |
|       23 |  696 | `			sData.pVm = pVm;` |
|       23 |  697 | `			sData.pStd = pStd;` |
|       23 |  698 | `			ph7_array_walk(pObj,VmArrayToObjectWalk,&sData);` |
|       24 |  699 | `		}else if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  700 | `			/* Scalar (int/float/bool/string): a single "scalar" property. */` |
|       11 |  701 | `			ph7_value *pSlot = PH7_VmCreateDynamicAttr(pVm,pStd,"scalar",sizeof("scalar")-1,0);` |
|       11 |  702 | `			if( pSlot ){` |
|       11 |  703 | `				PH7_MemObjStore(pObj,pSlot);` |
|        5 |  704 | `			}` |
|        5 |  705 | `		}` |
|        - |  706 | `		/* (A NULL source yields an empty stdClass — nothing to populate.) */` |
|        - |  707 | `		/* Invalidate any prior representation */` |
|       35 |  708 | `		PH7_MemObjRelease(pObj);` |
|        - |  709 | `		/* Save the new instance */` |
|       35 |  710 | `		pObj->x.pOther = pStd;` |
|       35 |  711 | `		MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       17 |  712 | `	}` |
|       35 |  713 | `	return SXRET_OK;` |
|       18 |  714 | `}` |
|        - |  715 | `/*` |
|        - |  716 | ` * Return a pointer to the appropriate convertion method associated` |
|        - |  717 | ` * with the given type.` |
|        - |  718 | ` * Note on type juggling.` |
|        - |  719 | ` * Accoding to the PHP language reference manual` |
|        - |  720 | ` *  PHP does not require (or support) explicit type definition in variable` |
|        - |  721 | ` *  declaration; a variable's type is determined by the context in which` |
|        - |  722 | ` *  the variable is used. That is to say, if a string value is assigned` |
|        - |  723 | ` *  to variable $var, $var becomes a string. If an integer value is then` |
|        - |  724 | ` *  assigned to $var, it becomes an integer.` |
|        - |  725 | ` */` |
|       84 |  726 | `PH7_PRIVATE ProcMemObjCast PH7_MemObjCastMethod(sxi32 iFlags)` |
|        5 |  727 | `{` |
|       89 |  728 | `	if( iFlags & MEMOBJ_STRING ){` |
|       16 |  729 | `		return PH7_MemObjToString;` |
|       75 |  730 | `	}else if( iFlags & MEMOBJ_INT ){` |
|       59 |  731 | `		return PH7_MemObjToInteger;` |
|       19 |  732 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|       16 |  733 | `		return PH7_MemObjToReal;` |
|        3 |  734 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  735 | `		return PH7_MemObjToBool;` |
|        3 |  736 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        3 |  737 | `		return PH7_MemObjToHashmap;` |
|      ! 0 |  738 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  739 | `		return PH7_MemObjToObject;` |
|      ! 0 |  740 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        - |  741 | ``		/* `null` is a type, not a weak-coercion target: never silently cast a`` |
|        - |  742 | ``		 * value to null for a standalone `null` type hint. Return/property`` |
|        - |  743 | `		 * enforcement reject a non-null value before reaching here; this guards` |
|        - |  744 | `		 * the parameter default-value path from quietly nulling a non-null` |
|        - |  745 | `		 * default. */` |
|      ! 0 |  746 | `		return 0;` |
|        - |  747 | `	}` |
|        - |  748 | `	/* NULL cast */` |
|      ! 0 |  749 | `	return PH7_MemObjToNull;` |
|       47 |  750 | `}` |
|        - |  751 | `/*` |
|        - |  752 | ` * Return TRUE only if the entire string held by pValue (optionally surrounded` |
|        - |  753 | ` * by whitespace, with an optional sign) is a well-formed PHP numeric string.` |
|        - |  754 | ` * This mirrors PHP's is_numeric_string grammar used for is_numeric() and the` |
|        - |  755 | ` * loose-comparison numeric gate:` |
|        - |  756 | ` *` |
|        - |  757 | ` *   [ws] [sign] ( D+ [.D*] \| .D+ ) [ (e\|E) [sign] D+ ] [ws]   (whole string)` |
|        - |  758 | ` *` |
|        - |  759 | ` * Implemented directly rather than via SyStrIsNumeric — which returns OK on any` |
|        - |  760 | ` * numeric PREFIX (so it wrongly accepts "10abc"/"0x1A"/"0b101") and requires a` |
|        - |  761 | ` * leading digit (so it wrongly rejects ".5"/"-.5", valid in PHP). Unlike a` |
|        - |  762 | ` * strtod-based classifier this needs no NUL-terminated buffer. Returns FALSE for` |
|        - |  763 | ` * a non-string value.` |
|        - |  764 | ` */` |
|   235190 |  765 | `PH7_PRIVATE int PH7_MemObjStringIsNumeric(ph7_value *pValue)` |
|        5 |  766 | `{` |
|        - |  767 | `	const char *z, *zEnd;` |
|        - |  768 | `	sxu32 n;` |
|   235195 |  769 | `	int bDigit = 0;` |
|   235195 |  770 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  771 | `		return 0;` |
|        - |  772 | `	}` |
|   235195 |  773 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|   235195 |  774 | `	n = SyBlobLength(&pValue->sBlob);` |
|   235195 |  775 | `	if( n == 0 ){` |
|       68 |  776 | `		return 0;` |
|        - |  777 | `	}` |
|   235129 |  778 | `	zEnd = z + n;` |
|   235135 |  779 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|        8 |  780 | `		z++;` |
|        2 |  781 | `	}` |
|   235129 |  782 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|       53 |  783 | `		z++;` |
|       24 |  784 | `	}` |
|   235309 |  785 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|      183 |  786 | `		z++; bDigit = 1;` |
|        3 |  787 | `	}` |
|   235129 |  788 | `	if( z < zEnd && z[0] == '.' ){` |
|       43 |  789 | `		z++;` |
|       79 |  790 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|       38 |  791 | `			z++; bDigit = 1;` |
|        2 |  792 | `		}` |
|       19 |  793 | `	}` |
|        - |  794 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|   235129 |  795 | `	if( !bDigit ){` |
|   234983 |  796 | `		return 0;` |
|        - |  797 | `	}` |
|        - |  798 | `	/* Optional exponent — must carry at least one digit (rejects "1e", "1e+"). */` |
|      149 |  799 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|       14 |  800 | `		z++;` |
|       14 |  801 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|      ! 0 |  802 | `			z++;` |
|      ! 0 |  803 | `		}` |
|       14 |  804 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|        6 |  805 | `			return 0;` |
|        - |  806 | `		}` |
|       22 |  807 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|       14 |  808 | `			z++;` |
|        2 |  809 | `		}` |
|        4 |  810 | `	}` |
|        - |  811 | `	/* Trailing whitespace allowed; anything else means not a numeric string. */` |
|      151 |  812 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|        8 |  813 | `		z++;` |
|        2 |  814 | `	}` |
|      145 |  815 | `	return z == zEnd ? 1 : 0;` |
|   117629 |  816 | `}` |
|        - |  817 | `/*` |
|        - |  818 | ` * Check whether the ph7_value is numeric [i.e: int/float/bool] or looks` |
|        - |  819 | ` * like a numeric number [i.e: if the ph7_value is of type string.].` |
|        - |  820 | ` * Return TRUE if numeric.FALSE otherwise.` |
|        - |  821 | ` */` |
|   235940 |  822 | `PH7_PRIVATE sxi32 PH7_MemObjIsNumeric(ph7_value *pObj)` |
|        5 |  823 | `{` |
|   235945 |  824 | `	if( pObj->iFlags & ( MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      535 |  825 | `		return TRUE;` |
|   235415 |  826 | `	}else if( pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      317 |  827 | `		return FALSE;` |
|   235103 |  828 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        - |  829 | `		/* TRUE only if the whole string is a well-formed PHP numeric string. */` |
|   235103 |  830 | `		return PH7_MemObjStringIsNumeric(pObj) ? TRUE : FALSE;` |
|        - |  831 | `	}` |
|        - |  832 | `	/* NOT REACHED */` |
|      ! 0 |  833 | `	return FALSE;` |
|   118004 |  834 | `}` |
|        - |  835 | `/*` |
|        - |  836 | ` * Check whether the ph7_value is empty.Return TRUE if empty.` |
|        - |  837 | ` * FALSE otherwise.` |
|        - |  838 | ` * An ph7_value is considered empty if the following are true:` |
|        - |  839 | ` * NULL value.` |
|        - |  840 | ` * Boolean FALSE.` |
|        - |  841 | ` * Integer/Float with a 0 (zero) value.` |
|        - |  842 | ` * An empty string or a stream of 0 (zero) [i.e: "0","00","000",...].` |
|        - |  843 | ` * An empty array.` |
|        - |  844 | ` * NOTE` |
|        - |  845 | ` *  OBJECT VALUE MUST NOT BE MODIFIED.` |
|        - |  846 | ` */` |
|    34178 |  847 | `PH7_PRIVATE sxi32 PH7_MemObjIsEmpty(ph7_value *pObj)` |
|        5 |  848 | `{` |
|    34183 |  849 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|       19 |  850 | `		return TRUE;` |
|    34167 |  851 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|       22 |  852 | `		return pObj->x.iVal == 0 ? TRUE : FALSE;` |
|    34147 |  853 | `	}else if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  854 | `		return pObj->rVal == (ph7_real)0 ? TRUE : FALSE;` |
|    34147 |  855 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|        5 |  856 | `		return !pObj->x.iVal;` |
|    34143 |  857 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|    27663 |  858 | `		if( SyBlobLength(&pObj->sBlob) <= 0 ){` |
|    22275 |  859 | `			return TRUE;` |
|      ! 0 |  860 | `		}else{` |
|        - |  861 | `			const char *zIn,*zEnd;` |
|     5393 |  862 | `			zIn = (const char *)SyBlobData(&pObj->sBlob);` |
|     5393 |  863 | `			zEnd = &zIn[SyBlobLength(&pObj->sBlob)];` |
|     5399 |  864 | `			while( zIn < zEnd ){` |
|     5399 |  865 | `				if( zIn[0] != '0' ){` |
|     5393 |  866 | `					break;` |
|        - |  867 | `				}` |
|        7 |  868 | `				zIn++;` |
|        1 |  869 | `			}` |
|     5393 |  870 | `			return zIn >= zEnd ? TRUE : FALSE;` |
|      ! 0 |  871 | `		}` |
|     6485 |  872 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|     6485 |  873 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|     6485 |  874 | `		return pMap->nEntry == 0 ? TRUE : FALSE;` |
|      ! 0 |  875 | `	}else if ( pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      ! 0 |  876 | `		return FALSE;` |
|        - |  877 | `	}` |
|        - |  878 | `	/* Assume empty by default */` |
|      ! 0 |  879 | `	return TRUE;` |
|    17094 |  880 | `}` |
|        - |  881 | `/*` |
|        - |  882 | ` * Convert a ph7_value so that it has types MEMOBJ_REAL or MEMOBJ_INT` |
|        - |  883 | ` * or both.` |
|        - |  884 | ` * Invalidate any prior representations. Every effort is made to force` |
|        - |  885 | ` * the conversion, even if the input is a string that does not look` |
|        - |  886 | ` * completely like a number.Convert as much of the string as we can` |
|        - |  887 | ` * and ignore the rest.` |
|        - |  888 | ` */` |
|   468067 |  889 | `PH7_PRIVATE sxi32 PH7_MemObjToNumeric(ph7_value *pObj)` |
|        5 |  890 | `{` |
|   468072 |  891 | `	if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|   467916 |  892 | `		if( pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|        3 |  893 | `			if( pObj->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  894 | `				pObj->x.iVal = 0;` |
|      ! 0 |  895 | `			}` |
|        3 |  896 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|        1 |  897 | `		}` |
|        - |  898 | `		/* Already numeric */` |
|   467916 |  899 | `		return  SXRET_OK;` |
|        - |  900 | `	}` |
|      159 |  901 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|      159 |  902 | `		sxi32 rc = SXERR_INVALID;` |
|      159 |  903 | `		sxu8 bReal = FALSE;` |
|        - |  904 | `		SyString sString;` |
|      159 |  905 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|        - |  906 | `		/* Check if the given string looks like a numeric number */` |
|      159 |  907 | `		if( sString.nByte > 0 ){` |
|      159 |  908 | `			rc = SyStrIsNumeric(sString.zString,sString.nByte,&bReal,0);` |
|      159 |  909 | `			if( rc != SXRET_OK && !bReal ){` |
|        - |  910 | `				/* SyStrIsNumeric requires a leading digit, so it mis-classifies` |
|        - |  911 | `				 * a leading-decimal real such as ".5"/"-.5"/".5e2" (returns` |
|        - |  912 | `				 * non-OK with bReal FALSE) — PHP treats these as float. Detect` |
|        - |  913 | `				 * that shape so it coerces to real (strtod parses it) instead of` |
|        - |  914 | `				 * falling through to the int(0) "not a number" branch below. */` |
|        9 |  915 | `				const char *z = sString.zString;` |
|        9 |  916 | `				const char *zEnd = z + sString.nByte;` |
|        9 |  917 | `				while( z < zEnd && SyisSpace(z[0]) ){ z++; }` |
|        9 |  918 | `				if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){ z++; }` |
|        9 |  919 | `				if( z < zEnd && z[0] == '.' && (z + 1) < zEnd && SyisDigit(z[1]) ){` |
|        9 |  920 | `					bReal = TRUE;` |
|        4 |  921 | `				}` |
|        4 |  922 | `			}` |
|       78 |  923 | `		}` |
|      159 |  924 | `		if( bReal ){` |
|       15 |  925 | `			PH7_MemObjToReal(&(*pObj));` |
|        8 |  926 | `		}else{` |
|      145 |  927 | `			if( rc != SXRET_OK ){` |
|        - |  928 | `				/* The input does not look at all like a number,set the value to 0 */` |
|      ! 0 |  929 | `				pObj->x.iVal = 0;` |
|      ! 0 |  930 | `			}else{` |
|        - |  931 | `				/* Convert as much as we can */` |
|      145 |  932 | `				pObj->x.iVal = MemObjStringToInt(&(*pObj));` |
|        - |  933 | `			}` |
|      145 |  934 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|      145 |  935 | `			SyBlobRelease(&pObj->sBlob);` |
|        3 |  936 | `		}` |
|       78 |  937 | `	}else if(pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)){` |
|      ! 0 |  938 | `		PH7_MemObjToInteger(pObj);` |
|      ! 0 |  939 | `	}else{` |
|        - |  940 | `		/* Perform a blind cast */` |
|      ! 0 |  941 | `		PH7_MemObjToReal(&(*pObj));` |
|        - |  942 | `	}` |
|      159 |  943 | `	return SXRET_OK;` |
|   234081 |  944 | `}` |
|        - |  945 | `/*` |
|        - |  946 | ` * Apply Perl-style increment to a string ph7_value in place.` |
|        - |  947 | ` * Walks the bytes right-to-left: digits 0-8 / letters a-y, A-Y bump in` |
|        - |  948 | ` * place; '9' wraps to '0' with carry; 'z' to 'a'; 'Z' to 'A'. A non-` |
|        - |  949 | ` * alphanumeric byte stops the walk without prepending. If carry survives` |
|        - |  950 | ` * past index 0, prepend '1', 'a', or 'A' depending on the class of the` |
|        - |  951 | ` * last carried character. Empty strings become "1".` |
|        - |  952 | ` *` |
|        - |  953 | ` * Caller must ensure pObj is MEMOBJ_STRING and not a numeric string;` |
|        - |  954 | ` * this routine never reclassifies the type, so a result like "e0" stays` |
|        - |  955 | ` * a string even though it looks numeric.` |
|        - |  956 | ` */` |
|       48 |  957 | `PH7_PRIVATE sxi32 PH7_MemObjStringIncrement(ph7_value *pObj)` |
|        1 |  958 | `{` |
|        - |  959 | `	enum CarryClass { CARRY_NONE = 0, CARRY_LOWER, CARRY_UPPER, CARRY_DIGIT };` |
|       49 |  960 | `	enum CarryClass last_class = CARRY_NONE;` |
|        - |  961 | `	sxu32 nLen, pos;` |
|        - |  962 | `	sxu8 *zStr;` |
|       49 |  963 | `	int carry = 1;` |
|        - |  964 | `	int ch;` |
|        - |  965 | `	/* Force ownership: the blob may be SXBLOB_RDONLY (e.g., from` |
|        - |  966 | `	 * PH7_MemObjLoad), in which case BlobPrepareGrow copies on demand` |
|        - |  967 | `	 * and clears the flag.  On an already-owned blob with spare capacity` |
|        - |  968 | `	 * (the common case under PHL's growth allocator), this is a no-op` |
|        - |  969 | `	 * append; on an exact-fit owned blob it triggers a single realloc. */` |
|       49 |  970 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       45 |  971 | `		SyBlobNullAppend(&pObj->sBlob);` |
|       22 |  972 | `	}` |
|       49 |  973 | `	nLen = SyBlobLength(&pObj->sBlob);` |
|       49 |  974 | `	if( nLen == 0 ){` |
|        5 |  975 | `		SyBlobAppend(&pObj->sBlob,"1",sizeof(char));` |
|        5 |  976 | `		return SXRET_OK;` |
|        - |  977 | `	}` |
|       45 |  978 | `	zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       45 |  979 | `	pos = nLen;` |
|       97 |  980 | `	while( pos > 0 ){` |
|       79 |  981 | `		pos--;` |
|       79 |  982 | `		ch = zStr[pos];` |
|       79 |  983 | `		if( ch >= 'a' && ch <= 'z' ){` |
|       45 |  984 | `			if( ch == 'z' ){` |
|       29 |  985 | `				zStr[pos] = 'a';` |
|       29 |  986 | `				last_class = CARRY_LOWER;` |
|       29 |  987 | `				continue;` |
|        - |  988 | `			}` |
|       17 |  989 | `			zStr[pos]++;` |
|       17 |  990 | `			carry = 0;` |
|       17 |  991 | `			break;` |
|       35 |  992 | `		}else if( ch >= 'A' && ch <= 'Z' ){` |
|       21 |  993 | `			if( ch == 'Z' ){` |
|       19 |  994 | `				zStr[pos] = 'A';` |
|       19 |  995 | `				last_class = CARRY_UPPER;` |
|       19 |  996 | `				continue;` |
|        - |  997 | `			}` |
|        3 |  998 | `			zStr[pos]++;` |
|        3 |  999 | `			carry = 0;` |
|        3 | 1000 | `			break;` |
|       15 | 1001 | `		}else if( ch >= '0' && ch <= '9' ){` |
|        7 | 1002 | `			if( ch == '9' ){` |
|        7 | 1003 | `				zStr[pos] = '0';` |
|        7 | 1004 | `				last_class = CARRY_DIGIT;` |
|        7 | 1005 | `				continue;` |
|        - | 1006 | `			}` |
|      ! 0 | 1007 | `			zStr[pos]++;` |
|      ! 0 | 1008 | `			carry = 0;` |
|      ! 0 | 1009 | `			break;` |
|      ! 0 | 1010 | `		}else{` |
|        - | 1011 | `			/* non-alphanumeric: stop without prepending */` |
|        9 | 1012 | `			carry = 0;` |
|        9 | 1013 | `			break;` |
|        - | 1014 | `		}` |
|      ! 0 | 1015 | `	}` |
|       45 | 1016 | `	if( carry ){` |
|        - | 1017 | `		sxu8 prepend;` |
|        - | 1018 | `		sxu32 i;` |
|       19 | 1019 | `		switch( last_class ){` |
|        9 | 1020 | `			case CARRY_LOWER: prepend = (sxu8)'a'; break;` |
|       11 | 1021 | `			case CARRY_UPPER: prepend = (sxu8)'A'; break;` |
|      ! 0 | 1022 | `			default:          prepend = (sxu8)'1'; break;` |
|        - | 1023 | `		}` |
|        - | 1024 | `		/* Append a sentinel byte to grow nByte by 1 (capacity grows too). */` |
|       19 | 1025 | `		SyBlobAppend(&pObj->sBlob,"\0",sizeof(char));` |
|       19 | 1026 | `		zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       19 | 1027 | `		nLen = SyBlobLength(&pObj->sBlob);` |
|        - | 1028 | `		/* Shift right by 1, walking from the end so overlapping is safe. */` |
|       57 | 1029 | `		for( i = nLen - 1; i > 0; i-- ){` |
|       39 | 1030 | `			zStr[i] = zStr[i - 1];` |
|       20 | 1031 | `		}` |
|       19 | 1032 | `		zStr[0] = prepend;` |
|        9 | 1033 | `	}` |
|       45 | 1034 | `	return SXRET_OK;` |
|       25 | 1035 | `}` |
|        - | 1036 | `/*` |
|        - | 1037 | ` * Try a get an integer representation of the given ph7_value.` |
|        - | 1038 | ` * If the ph7_value is not of type real,this function is a no-op.` |
|        - | 1039 | ` */` |
|     1120 | 1040 | `PH7_PRIVATE sxi32 PH7_MemObjTryInteger(ph7_value *pObj)` |
|        1 | 1041 | `{` |
|     1121 | 1042 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - | 1043 | `		/* Work only with reals */` |
|     1121 | 1044 | `		MemObjTryIntger(&(*pObj));` |
|      560 | 1045 | `	}` |
|     1121 | 1046 | `	return SXRET_OK;` |
|        1 | 1047 | `}` |
|        - | 1048 | `/*` |
|        - | 1049 | ` * Initialize a ph7_value to the null type.` |
|        - | 1050 | ` */` |
| 10907439 | 1051 | `PH7_PRIVATE sxi32 PH7_MemObjInit(ph7_vm *pVm,ph7_value *pObj)` |
|        5 | 1052 | `{` |
|        - | 1053 | `	/* Zero the structure */` |
| 10907444 | 1054 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1055 | `	/* Initialize fields */` |
| 10907444 | 1056 | `	pObj->pVm = pVm;` |
| 10907444 | 1057 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1058 | `	/* Set the NULL type */` |
| 10907444 | 1059 | `	pObj->iFlags = MEMOBJ_NULL;` |
| 10907444 | 1060 | `	return SXRET_OK;` |
|        5 | 1061 | `}` |
|        - | 1062 | `/*` |
|        - | 1063 | ` * Initialize a ph7_value to the integer type.` |
|        - | 1064 | ` */` |
|  3442266 | 1065 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromInt(ph7_vm *pVm,ph7_value *pObj,sxi64 iVal)` |
|        5 | 1066 | `{` |
|        - | 1067 | `	/* Zero the structure */` |
|  3442271 | 1068 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1069 | `	/* Initialize fields */` |
|  3442271 | 1070 | `	pObj->pVm = pVm;` |
|  3442271 | 1071 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1072 | `	/* Set the desired type */` |
|  3442271 | 1073 | `	pObj->x.iVal = iVal;` |
|  3442271 | 1074 | `	pObj->iFlags = MEMOBJ_INT;` |
|  3442271 | 1075 | `	return SXRET_OK;` |
|        5 | 1076 | `}` |
|        - | 1077 | `/*` |
|        - | 1078 | ` * Initialize a ph7_value to the boolean type.` |
|        - | 1079 | ` */` |
|    17458 | 1080 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromBool(ph7_vm *pVm,ph7_value *pObj,sxi32 iVal)` |
|        5 | 1081 | `{` |
|        - | 1082 | `	/* Zero the structure */` |
|    17463 | 1083 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1084 | `	/* Initialize fields */` |
|    17463 | 1085 | `	pObj->pVm = pVm;` |
|    17463 | 1086 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1087 | `	/* Set the desired type */` |
|    17463 | 1088 | `	pObj->x.iVal = iVal ? 1 : 0;` |
|    17463 | 1089 | `	pObj->iFlags = MEMOBJ_BOOL;` |
|    17463 | 1090 | `	return SXRET_OK;` |
|        5 | 1091 | `}` |
|        - | 1092 | `/*` |
|        - | 1093 | ` * Initialize a ph7_value to the real type.` |
|        - | 1094 | ` */` |
|       10 | 1095 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromReal(ph7_vm *pVm,ph7_value *pObj,ph7_real rVal)` |
|        1 | 1096 | `{` |
|        - | 1097 | `	/* Zero the structure */` |
|       11 | 1098 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1099 | `	/* Initialize fields */` |
|       11 | 1100 | `	pObj->pVm = pVm;` |
|       11 | 1101 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1102 | `	/* Set the desired type */` |
|       11 | 1103 | `	pObj->rVal = rVal;` |
|       11 | 1104 | `	pObj->iFlags = MEMOBJ_REAL;` |
|       11 | 1105 | `	return SXRET_OK;` |
|        1 | 1106 | `}` |
|        - | 1107 | `/*` |
|        - | 1108 | ` * Initialize a ph7_value to the array type.` |
|        - | 1109 | ` */` |
|    74426 | 1110 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromArray(ph7_vm *pVm,ph7_value *pObj,ph7_hashmap *pArray)` |
|        5 | 1111 | `{` |
|        - | 1112 | `	/* Zero the structure */` |
|    74431 | 1113 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1114 | `	/* Initialize fields */` |
|    74431 | 1115 | `	pObj->pVm = pVm;` |
|    74431 | 1116 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1117 | `	/* Set the desired type */` |
|    74431 | 1118 | `	pObj->iFlags = MEMOBJ_HASHMAP;` |
|    74431 | 1119 | `	pObj->x.pOther = pArray;` |
|    74431 | 1120 | `	return SXRET_OK;` |
|        5 | 1121 | `}` |
|        - | 1122 | `/*` |
|        - | 1123 | ` * Initialize a ph7_value to the string type.` |
|        - | 1124 | ` */` |
|  2262292 | 1125 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromString(ph7_vm *pVm,ph7_value *pObj,const SyString *pVal)` |
|        5 | 1126 | `{` |
|        - | 1127 | `	/* Zero the structure */` |
|  2262297 | 1128 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1129 | `	/* Initialize fields */` |
|  2262297 | 1130 | `	pObj->pVm = pVm;` |
|  2262297 | 1131 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|  2262297 | 1132 | `	if( pVal ){` |
|        - | 1133 | `		/* Append contents */` |
|   949813 | 1134 | `		SyBlobAppend(&pObj->sBlob,(const void *)pVal->zString,pVal->nByte);` |
|   474904 | 1135 | `	}` |
|        - | 1136 | `	/* Set the desired type */` |
|  2262297 | 1137 | `	pObj->iFlags = MEMOBJ_STRING;` |
|  2262297 | 1138 | `	return SXRET_OK;` |
|        5 | 1139 | `}` |
|        - | 1140 | `/*` |
|        - | 1141 | ` * Append some contents to the internal buffer of a given ph7_value.` |
|        - | 1142 | ` * If the given ph7_value is not of type string,this function` |
|        - | 1143 | ` * invalidate any prior representation and set the string type.` |
|        - | 1144 | ` * Then a simple append operation is performed.` |
|        - | 1145 | ` */` |
|  1505626 | 1146 | `PH7_PRIVATE sxi32 PH7_MemObjStringAppend(ph7_value *pObj,const char *zData,sxu32 nLen)` |
|        5 | 1147 | `{` |
|        - | 1148 | `	sxi32 rc;` |
|  1505631 | 1149 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 1150 | `		/* Invalidate any prior representation */` |
|     2611 | 1151 | `		PH7_MemObjRelease(pObj);` |
|     2611 | 1152 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|     1303 | 1153 | `	}` |
|        - | 1154 | `	/* Append contents */` |
|  1505631 | 1155 | `	rc = SyBlobAppend(&pObj->sBlob,zData,nLen);` |
|  1505631 | 1156 | `	return rc;` |
|        5 | 1157 | `}` |
|        - | 1158 | `#if 0` |
|        - | 1159 | `/*` |
|        - | 1160 | ` * Format and append some contents to the internal buffer of a given ph7_value.` |
|        - | 1161 | ` * If the given ph7_value is not of type string,this function invalidate` |
|        - | 1162 | ` * any prior representation and set the string type.` |
|        - | 1163 | ` * Then a simple format and append operation is performed.` |
|        - | 1164 | ` */` |
|        - | 1165 | `PH7_PRIVATE sxi32 PH7_MemObjStringFormat(ph7_value *pObj,const char *zFormat,va_list ap)` |
|        - | 1166 | `{` |
|        - | 1167 | `	sxi32 rc;` |
|        - | 1168 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 1169 | `		/* Invalidate any prior representation */` |
|        - | 1170 | `		PH7_MemObjRelease(pObj);` |
|        - | 1171 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|        - | 1172 | `	}` |
|        - | 1173 | `	/* Format and append contents */` |
|        - | 1174 | `	rc = SyBlobFormatAp(&pObj->sBlob,zFormat,ap);` |
|        - | 1175 | `	return rc;` |
|        - | 1176 | `}` |
|        - | 1177 | `#endif` |
|        - | 1178 | `/*` |
|        - | 1179 | ` * Duplicate the contents of a ph7_value.` |
|        - | 1180 | ` */` |
|  5065741 | 1181 | `PH7_PRIVATE sxi32 PH7_MemObjStore(ph7_value *pSrc,ph7_value *pDest)` |
|        5 | 1182 | `{` |
|  5065746 | 1183 | `	ph7_class_instance *pObj = 0;` |
|  5065746 | 1184 | `	ph7_hashmap *pMap = 0;` |
|        - | 1185 | `	sxi32 rc;` |
|  5065746 | 1186 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1187 | `		/* Increment reference count */` |
|   214199 | 1188 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  4958649 | 1189 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|        - | 1190 | `		/* Increment reference count */` |
|     5653 | 1191 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|     2824 | 1192 | `	}` |
|  5065746 | 1193 | `	if( pDest->iFlags & MEMOBJ_HASHMAP ){` |
|    62385 | 1194 | `		pMap = (ph7_hashmap *)pDest->x.pOther;` |
|  5034556 | 1195 | `	}else if( pDest->iFlags & MEMOBJ_OBJ ){` |
|     8109 | 1196 | `		pObj = (ph7_class_instance *)pDest->x.pOther;` |
|     4052 | 1197 | `	}` |
|  5065746 | 1198 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  5065746 | 1199 | `	pDest->iFlags &= ~MEMOBJ_AUX;` |
|  5065746 | 1200 | `	rc = SXRET_OK;` |
|  5065746 | 1201 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  4040773 | 1202 | `		SyBlobReset(&pDest->sBlob);` |
|  4040773 | 1203 | `		rc = SyBlobDup(&pSrc->sBlob,&pDest->sBlob);` |
|  2020389 | 1204 | `	}else{` |
|  1024978 | 1205 | `		if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|   290532 | 1206 | `			SyBlobRelease(&pDest->sBlob);` |
|   145306 | 1207 | `		}` |
|        - | 1208 | `	}` |
|  5065746 | 1209 | `	if( pMap ){` |
|    62385 | 1210 | `		PH7_HashmapUnref(pMap);` |
|  5034556 | 1211 | `	}else if( pObj ){` |
|     8109 | 1212 | `		PH7_ClassInstanceUnref(pObj);` |
|     4052 | 1213 | `	}` |
|  5065741 | 1214 | `	if( rc == SXRET_OK && (pDest->iFlags & MEMOBJ_HASHMAP)` |
|  2640010 | 1215 | `	 && pDest->pVm` |
|   214194 | 1216 | `	 && (ph7_hashmap *)pDest->x.pOther == pDest->pVm->pGlobal` |
|        - | 1217 | `	 /* Identity, not nIdx: transient values carry nIdx==0 (SyZero), which` |
|        - | 1218 | `	  * collides with a typical nGlobalIdx of 0 and would skip the snapshot` |
|        - | 1219 | `	  * for closure envs and other non-slot destinations. */` |
|   107106 | 1220 | `	 && pDest != (ph7_value *)SySetAt(&pDest->pVm->aMemObj,pDest->pVm->nGlobalIdx) ){` |
|        - | 1221 | `		/* php 8.1: a COPY of $GLOBALS ($snap = $GLOBALS, $a[] = $GLOBALS,` |
|        - | 1222 | `		 * by-value argument passing, return $GLOBALS, ...) is a by-value` |
|        - | 1223 | `		 * SNAPSHOT of the symbol table with its reference entries` |
|        - | 1224 | `		 * flattened — never a live alias. Materialize it here, the one` |
|        - | 1225 | `		 * store choke point (loads/subscript access keep sharing, so` |
|        - | 1226 | `		 * $GLOBALS[$k] reads and writes stay live). */` |
|        9 | 1227 | `		ph7_hashmap *pSnap = PH7_NewHashmap(pDest->pVm,0,0);` |
|        9 | 1228 | `		if( pSnap && PH7_HashmapDupMaterialized((ph7_hashmap *)pDest->x.pOther,pSnap) == SXRET_OK ){` |
|        9 | 1229 | `			PH7_HashmapUnref((ph7_hashmap *)pDest->x.pOther);` |
|        9 | 1230 | `			pDest->x.pOther = pSnap;` |
|        4 | 1231 | `		}else if( pSnap ){` |
|      ! 0 | 1232 | `			PH7_HashmapUnref(pSnap);` |
|      ! 0 | 1233 | `		}` |
|        4 | 1234 | `	}` |
|  5065746 | 1235 | `	return rc;` |
|        5 | 1236 | `}` |
|        - | 1237 | `/*` |
|        - | 1238 | ` * Duplicate the contents of a ph7_value but do not copy internal` |
|        - | 1239 | ` * buffer contents,simply point to it.` |
|        - | 1240 | ` */` |
|  7043042 | 1241 | `PH7_PRIVATE sxi32 PH7_MemObjLoad(ph7_value *pSrc,ph7_value *pDest)` |
|        5 | 1242 | `{` |
|  7043047 | 1243 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),` |
|        - | 1244 | `		sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  7043047 | 1245 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1246 | `		/* Increment reference count */` |
|   493325 | 1247 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  6796387 | 1248 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|        - | 1249 | `		/* Increment reference count */` |
|    38027 | 1250 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|    19011 | 1251 | `	}` |
|  7043047 | 1252 | `	if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|       48 | 1253 | `		SyBlobRelease(&pDest->sBlob);` |
|       22 | 1254 | `	}` |
|  7043047 | 1255 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  3793547 | 1256 | `		SyBlobReadOnly(&pDest->sBlob,SyBlobData(&pSrc->sBlob),SyBlobLength(&pSrc->sBlob));` |
|  1896914 | 1257 | `	}` |
|  7043047 | 1258 | `	return SXRET_OK;` |
|        5 | 1259 | `}` |
|        - | 1260 | `/*` |
|        - | 1261 | ` * Invalidate any prior representation of a given ph7_value.` |
|        - | 1262 | ` */` |
| 17308655 | 1263 | `PH7_PRIVATE sxi32 PH7_MemObjRelease(ph7_value *pObj)` |
|        5 | 1264 | `{` |
| 17308660 | 1265 | `	if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
| 15000281 | 1266 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|   684875 | 1267 | `			PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
| 14657846 | 1268 | `		}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|    65101 | 1269 | `			PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|    32548 | 1270 | `		}` |
|        - | 1271 | `		/* Release the internal buffer */` |
| 15000281 | 1272 | `		SyBlobRelease(&pObj->sBlob);` |
|        - | 1273 | `		/* Invalidate any prior representation */` |
| 15000281 | 1274 | `		pObj->iFlags = MEMOBJ_NULL;` |
|  7500537 | 1275 | `	}` |
| 17308660 | 1276 | `	return SXRET_OK;` |
|        5 | 1277 | `}` |
|        - | 1278 | `/*` |
|        - | 1279 | ` * Compare two ph7_values.` |
|        - | 1280 | ` * Return 0 if the values are equals, > 0 if pObj1 is greater than pObj2` |
|        - | 1281 | ` * or < 0 if pObj2 is greater than pObj1.` |
|        - | 1282 | ` * Type comparison table taken from the PHP language reference manual.` |
|        - | 1283 | ` * Comparisons of $x with PHP functions Expression` |
|        - | 1284 | ` *              gettype() 	empty() 	is_null() 	isset() 	boolean : if($x)` |
|        - | 1285 | ` * $x = ""; 	string 	    TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1286 | ` * $x = null 	NULL 	    TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1287 | ` * var $x; 	    NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1288 | ` * $x is undefined 	NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1289 | ` *  $x = array(); 	array 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1290 | ` * $x = false; 	boolean 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1291 | ` * $x = true; 	boolean 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1292 | ` * $x = 1; 	    integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1293 | ` * $x = 42; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1294 | ` * $x = 0; 	    integer 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1295 | ` * $x = -1; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1296 | ` * $x = "1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1297 | ` * $x = "0"; 	string 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1298 | ` * $x = "-1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1299 | ` * $x = "php"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1300 | ` * $x = "true"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1301 | ` * $x = "false"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1302 | ` *      Loose comparisons with ==` |
|        - | 1303 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|        - | 1304 | ` * TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1305 | ` * FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|        - | 1306 | ` * 1 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1307 | ` * 0 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE` |
|        - | 1308 | ` * -1 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1309 | ` * "1" 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1310 | ` * "0" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1311 | ` * "-1" 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1312 | ` * NULL 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|        - | 1313 | ` * array() 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1314 | ` * "php" 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1315 | ` * "" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE` |
|        - | 1316 | ` *    Strict comparisons with ===` |
|        - | 1317 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|        - | 1318 | ` * TRUE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1319 | ` * FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1320 | ` * 1 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1321 | ` * 0 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1322 | ` * -1 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1323 | ` * "1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1324 | ` * "0" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1325 | ` * "-1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1326 | ` * NULL 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE` |
|        - | 1327 | ` * array() 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE` |
|        - | 1328 | ` * "php" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1329 | ` * "" 	    FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE` |
|        - | 1330 | ` */` |
|  1332778 | 1331 | `PH7_PRIVATE sxi32 PH7_MemObjCmp(ph7_value *pObj1,ph7_value *pObj2,int bStrict,int iNest)` |
|        5 | 1332 | `{` |
|        - | 1333 | `	sxi32 iComb;` |
|        - | 1334 | `	sxi32 rc;` |
|  1332783 | 1335 | `	if( bStrict ){` |
|        - | 1336 | `		sxi32 iF1,iF2;` |
|        - | 1337 | `		/* Strict comparisons with === */` |
|   699732 | 1338 | `		iF1 = pObj1->iFlags&~MEMOBJ_AUX;` |
|   699732 | 1339 | `		iF2 = pObj2->iFlags&~MEMOBJ_AUX;` |
|   699732 | 1340 | `		if( iF1 != iF2 ){` |
|        - | 1341 | `			/* Not of the same type */` |
|   195805 | 1342 | `			return 1;` |
|        - | 1343 | `		}` |
|   251964 | 1344 | `	}` |
|        - | 1345 | `	/* Combine flag together */` |
|  1136983 | 1346 | `	iComb = pObj1->iFlags\|pObj2->iFlags;` |
|  1136978 | 1347 | `	if( !bStrict` |
|   885015 | 1348 | `	 && (iComb & MEMOBJ_NULL) != 0` |
|   316635 | 1349 | `	 && (iComb & MEMOBJ_STRING) != 0` |
|       65 | 1350 | `	 && (iComb & (MEMOBJ_BOOL\|MEMOBJ_RES\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|        - | 1351 | `		/*` |
|        - | 1352 | `		 * PHP 8 comparison table: null loosely compared with a STRING is` |
|        - | 1353 | `		 * compared as the empty string (a string comparison), not through` |
|        - | 1354 | `		 * bool coercion — so null == "0" is FALSE and null < "0" is TRUE` |
|        - | 1355 | `		 * (php 7 and the historical PH7 behavior coerced both to bool,` |
|        - | 1356 | `		 * making any non-empty non-"0"-insensitive string "equal" to null).` |
|        - | 1357 | `		 * Convert the null side to "" and let the string branch below run.` |
|        - | 1358 | `		 */` |
|       45 | 1359 | `		if( pObj1->iFlags & MEMOBJ_NULL ){` |
|       35 | 1360 | `			PH7_MemObjToString(pObj1);` |
|       18 | 1361 | `		}else{` |
|       11 | 1362 | `			PH7_MemObjToString(pObj2);` |
|        - | 1363 | `		}` |
|       45 | 1364 | `		iComb = pObj1->iFlags\|pObj2->iFlags;` |
|       22 | 1365 | `	}` |
|  1136983 | 1366 | `	if( iComb & (MEMOBJ_NULL\|MEMOBJ_RES\|MEMOBJ_BOOL) ){` |
|        - | 1367 | `		/* Convert to boolean: Keep in mind FALSE < TRUE */` |
|    23039 | 1368 | `		if( (pObj1->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     8459 | 1369 | `			PH7_MemObjToBool(pObj1);` |
|     4227 | 1370 | `		}` |
|    23039 | 1371 | `		if( (pObj2->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     7479 | 1372 | `			PH7_MemObjToBool(pObj2);` |
|     3737 | 1373 | `		}` |
|    23039 | 1374 | `		return (sxi32)((pObj1->x.iVal != 0) - (pObj2->x.iVal != 0));` |
|  1113949 | 1375 | `	}else if ( iComb & MEMOBJ_HASHMAP ){` |
|        - | 1376 | `		/* Hashmap aka 'array' comparison */` |
|       31 | 1377 | `		if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1378 | `			/* Array is always greater */` |
|      ! 0 | 1379 | `			return -1;` |
|        - | 1380 | `		}` |
|       31 | 1381 | `		if( (pObj2->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1382 | `			/* Array is always greater */` |
|      ! 0 | 1383 | `			return 1;` |
|        - | 1384 | `		}` |
|        - | 1385 | `		/* Perform the comparison */` |
|       31 | 1386 | `		rc = PH7_HashmapCmp((ph7_hashmap *)pObj1->x.pOther,(ph7_hashmap *)pObj2->x.pOther,bStrict);` |
|       31 | 1387 | `		return rc;` |
|  1113919 | 1388 | `	}else if(iComb & MEMOBJ_OBJ ){` |
|        - | 1389 | `		/* Object comparison */` |
|      257 | 1390 | `		if( (pObj1->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1391 | `			/* Object is always greater */` |
|      ! 0 | 1392 | `			return -1;` |
|        - | 1393 | `		}` |
|      257 | 1394 | `		if( (pObj2->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1395 | `			/* Object is always greater */` |
|      ! 0 | 1396 | `			return 1;` |
|        - | 1397 | `		}` |
|        - | 1398 | `		/* Perform the comparison */` |
|      257 | 1399 | `		rc = PH7_ClassInstanceCmp((ph7_class_instance *)pObj1->x.pOther,(ph7_class_instance *)pObj2->x.pOther,bStrict,iNest);` |
|      257 | 1400 | `		return rc;` |
|  1113667 | 1401 | `	}else if ( iComb & MEMOBJ_STRING ){` |
|        - | 1402 | `		SyString s1,s2;` |
|   714565 | 1403 | `		if( !bStrict ){` |
|        - | 1404 | `			/*` |
|        - | 1405 | `			 * PHP 8 "saner string to number comparisons" (RFC): a numeric` |
|        - | 1406 | `			 * comparison is performed only when BOTH operands are numbers or` |
|        - | 1407 | `			 * numeric strings. A number compared with a NON-numeric string is` |
|        - | 1408 | `			 * compared as strings, with the number cast to its string form —` |
|        - | 1409 | `			 * so 0 == "abc" is false, "abc" < 10 is false, and max("abc",10)` |
|        - | 1410 | `			 * is "abc". (PHP 7 cast the non-numeric string to 0 and compared` |
|        - | 1411 | `			 * numerically; comparing when EITHER side was numeric is what this` |
|        - | 1412 | `			 * replaces.) Two non-numeric strings, or one numeric and one` |
|        - | 1413 | `			 * non-numeric string, still fall through to the string comparison` |
|        - | 1414 | `			 * below, unchanged.` |
|        - | 1415 | `			 */` |
|   234777 | 1416 | `			if( PH7_MemObjIsNumeric(pObj1) && PH7_MemObjIsNumeric(pObj2) ){` |
|        - | 1417 | `				/* Perform a numeric comparison */` |
|       29 | 1418 | `				goto Numeric;` |
|        - | 1419 | `			}` |
|   117401 | 1420 | `		}` |
|        - | 1421 | `		/* Perform a strict string comparison.*/` |
|   714537 | 1422 | `		if( (pObj1->iFlags&MEMOBJ_STRING) == 0 ){` |
|       19 | 1423 | `			PH7_MemObjToString(pObj1);` |
|        9 | 1424 | `		}` |
|   714537 | 1425 | `		if( (pObj2->iFlags&MEMOBJ_STRING) == 0 ){` |
|        5 | 1426 | `			PH7_MemObjToString(pObj2);` |
|        2 | 1427 | `		}` |
|   714537 | 1428 | `		SyStringInitFromBuf(&s1,SyBlobData(&pObj1->sBlob),SyBlobLength(&pObj1->sBlob));` |
|   714537 | 1429 | `		SyStringInitFromBuf(&s2,SyBlobData(&pObj2->sBlob),SyBlobLength(&pObj2->sBlob));` |
|        - | 1430 | `		/*` |
|        - | 1431 | `		 * Strings are compared using memcmp(). If one value is an exact prefix of the` |
|        - | 1432 | `		 * other, then the shorter value is less than the longer value.` |
|        - | 1433 | `		 */` |
|   714537 | 1434 | `		rc = SyMemcmp((const void *)s1.zString,(const void *)s2.zString,SXMIN(s1.nByte,s2.nByte));` |
|   714537 | 1435 | `		if( rc == 0 ){` |
|   238086 | 1436 | `			if( s1.nByte != s2.nByte ){` |
|     2280 | 1437 | `				rc = s1.nByte < s2.nByte ? -1 : 1;` |
|     1137 | 1438 | `			}` |
|   119040 | 1439 | `		}` |
|   714537 | 1440 | `		return rc;` |
|   399107 | 1441 | `	}else if( iComb & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|   199508 | 1442 | `Numeric:` |
|        - | 1443 | `		/* Perform a numeric comparison if one of the operand is numeric(integer or real) */` |
|   399135 | 1444 | `		if( (pObj1->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|       11 | 1445 | `			PH7_MemObjToNumeric(pObj1);` |
|        5 | 1446 | `		}` |
|   399135 | 1447 | `		if( (pObj2->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|       19 | 1448 | `			PH7_MemObjToNumeric(pObj2);` |
|        9 | 1449 | `		}` |
|   399135 | 1450 | `		if( (pObj1->iFlags & pObj2->iFlags & MEMOBJ_INT) == 0) {` |
|        - | 1451 | `			/*` |
|        - | 1452 | `			 * Symisc eXtension to the PHP language:` |
|        - | 1453 | `			 *  Floating point comparison is introduced and works as expected.` |
|        - | 1454 | `			 */` |
|        - | 1455 | `			ph7_real r1,r2;` |
|        - | 1456 | `			/* Compare as reals */` |
|      273 | 1457 | `			if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       11 | 1458 | `				PH7_MemObjToReal(pObj1);` |
|        5 | 1459 | `			}` |
|      273 | 1460 | `			r1 = pObj1->rVal;` |
|      273 | 1461 | `			if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|       51 | 1462 | `				PH7_MemObjToReal(pObj2);` |
|       25 | 1463 | `			}` |
|      273 | 1464 | `			r2 = pObj2->rVal;` |
|      273 | 1465 | `			if( PH7_IS_NAN(r1) \|\| PH7_IS_NAN(r2) ){` |
|        - | 1466 | `				/*` |
|        - | 1467 | `				 * Keep a strict three-way comparator contract even for NaN values.` |
|        - | 1468 | `				 * For ordering purposes, NaN compares equal to NaN and greater than` |
|        - | 1469 | `				 * any non-NaN numeric value.` |
|        - | 1470 | `				 */` |
|       45 | 1471 | `				if( PH7_IS_NAN(r1) ){` |
|       35 | 1472 | `					return PH7_IS_NAN(r2) ? 0 : 1;` |
|        - | 1473 | `				}` |
|       11 | 1474 | `				return -1;` |
|        - | 1475 | `			}` |
|      229 | 1476 | `			if( r1 > r2 ){` |
|       45 | 1477 | `				return 1;` |
|      185 | 1478 | `			}else if( r1 < r2 ){` |
|      125 | 1479 | `				return -1;` |
|        - | 1480 | `			}` |
|       61 | 1481 | `			return 0;` |
|      ! 0 | 1482 | `		}else{` |
|        - | 1483 | `			/* Integer comparison */` |
|   398863 | 1484 | `			if( pObj1->x.iVal > pObj2->x.iVal ){` |
|     5952 | 1485 | `				return 1;` |
|   392916 | 1486 | `			}else if( pObj1->x.iVal < pObj2->x.iVal ){` |
|   385314 | 1487 | `				return -1;` |
|        - | 1488 | `			}` |
|     7607 | 1489 | `			return 0;` |
|        - | 1490 | `		}` |
|        - | 1491 | `	}` |
|        - | 1492 | `	/* NOT REACHED */` |
|      ! 0 | 1493 | `	return 0;` |
|   666466 | 1494 | `}` |
|        - | 1495 | `/*` |
|        - | 1496 | ` * Perform an addition operation of two ph7_values.` |
|        - | 1497 | ` * The reason this function is implemented here rather than 'vm.c'` |
|        - | 1498 | ` * is that the '+' operator is overloaded.` |
|        - | 1499 | ` * That is,the '+' operator is used for arithmetic operation and also` |
|        - | 1500 | ` * used for operation on arrays [i.e: union]. When used with an array` |
|        - | 1501 | ` * The + operator returns the right-hand array appended to the left-hand array.` |
|        - | 1502 | ` * For keys that exist in both arrays, the elements from the left-hand array` |
|        - | 1503 | ` * will be used, and the matching elements from the right-hand array will` |
|        - | 1504 | ` * be ignored.` |
|        - | 1505 | ` * This function take care of handling all the scenarios.` |
|        - | 1506 | ` */` |
|    10758 | 1507 | `PH7_PRIVATE sxi32 PH7_MemObjAdd(ph7_value *pObj1,ph7_value *pObj2,int bAddStore)` |
|        5 | 1508 | `{` |
|    10763 | 1509 | `	if( ((pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1510 | `			/* Arithemtic operation */` |
|     6925 | 1511 | `			PH7_MemObjToNumeric(pObj1);` |
|     6925 | 1512 | `			PH7_MemObjToNumeric(pObj2);` |
|     6925 | 1513 | `			if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_REAL ){` |
|        - | 1514 | `				/* Floating point arithmetic */` |
|        - | 1515 | `				ph7_real a,b;` |
|       67 | 1516 | `				if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       31 | 1517 | `					PH7_MemObjToReal(pObj1);` |
|       15 | 1518 | `				}` |
|       67 | 1519 | `				if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 | 1520 | `					PH7_MemObjToReal(pObj2);` |
|        2 | 1521 | `				}` |
|       67 | 1522 | `				a = pObj1->rVal;` |
|       67 | 1523 | `				b = pObj2->rVal;` |
|       67 | 1524 | `				pObj1->rVal = a+b;` |
|       67 | 1525 | `				MemObjSetType(pObj1,MEMOBJ_REAL);` |
|        - | 1526 | `				/* Try to get an integer representation also */` |
|       67 | 1527 | `				MemObjTryIntger(&(*pObj1));` |
|       34 | 1528 | `			}else{` |
|        - | 1529 | `				/* Integer arithmetic; PHP promotes an overflowing sum to float.` |
|        - | 1530 | `				 * The integer-only build (PH7_OMIT_FLOATING_POINT) has no float` |
|        - | 1531 | `				 * type, so it wraps like OP_POW's OMIT path. */` |
|        - | 1532 | `				sxi64 a,b,r;` |
|     6859 | 1533 | `				a = pObj1->x.iVal;` |
|     6859 | 1534 | `				b = pObj2->x.iVal;` |
|     6859 | 1535 | `				if( PH7_ADD_OVERFLOW64(a,b,&r) ){` |
|        - | 1536 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        9 | 1537 | `					pObj1->rVal = (ph7_real)a + (ph7_real)b;` |
|        9 | 1538 | `					MemObjSetType(pObj1,MEMOBJ_REAL);` |
|        - | 1539 | `#else` |
|        - | 1540 | `					pObj1->x.iVal = r;` |
|        - | 1541 | `					MemObjSetType(pObj1,MEMOBJ_INT);` |
|        - | 1542 | `#endif` |
|        5 | 1543 | `				}else{` |
|     6851 | 1544 | `					pObj1->x.iVal = r;` |
|     6851 | 1545 | `					MemObjSetType(pObj1,MEMOBJ_INT);` |
|        - | 1546 | `				}` |
|        - | 1547 | `			}` |
|     3465 | 1548 | `	}else{` |
|     3843 | 1549 | `		if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP ){` |
|        - | 1550 | `			ph7_hashmap *pMap;` |
|        - | 1551 | `			sxi32 rc;` |
|     3843 | 1552 | `			if( bAddStore ){` |
|        - | 1553 | `				/* Do not duplicate the hashmap,use the left one since its an add&store operation.` |
|        - | 1554 | `				 */` |
|        3 | 1555 | `				if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1556 | `					/* Force a hashmap cast */` |
|      ! 0 | 1557 | `					rc = PH7_MemObjToHashmap(pObj1);` |
|      ! 0 | 1558 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 1559 | `						PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|      ! 0 | 1560 | `						return rc;` |
|        - | 1561 | `					}` |
|      ! 0 | 1562 | `				}` |
|        - | 1563 | `				/* COW separate before in-place mutation */` |
|        3 | 1564 | `				pMap = PH7_HashmapCowSeparate(pObj1->pVm,pObj1);` |
|        2 | 1565 | `			}else{` |
|        - | 1566 | `				/* Create a new hashmap */` |
|     3841 | 1567 | `				pMap = PH7_NewHashmap(pObj1->pVm,0,0);` |
|     3841 | 1568 | `				if( pMap == 0){` |
|      ! 0 | 1569 | `					PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|      ! 0 | 1570 | `					return SXERR_MEM;` |
|        - | 1571 | `				}` |
|        - | 1572 | `			}` |
|     3843 | 1573 | `			if( !bAddStore ){` |
|     3841 | 1574 | `				if(pObj1->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1575 | `					/* Perform a hashmap duplication */` |
|     3841 | 1576 | `					PH7_HashmapDup((ph7_hashmap *)pObj1->x.pOther,pMap);` |
|     1923 | 1577 | `				}else{` |
|      ! 0 | 1578 | `					if((pObj1->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 1579 | `						/* Simple insertion */` |
|      ! 0 | 1580 | `						PH7_HashmapInsert(pMap,0,pObj1);` |
|      ! 0 | 1581 | `					}` |
|        - | 1582 | `				}` |
|     1918 | 1583 | `			}` |
|        - | 1584 | `			/* Perform the union */` |
|     3843 | 1585 | `			if(pObj2->iFlags & MEMOBJ_HASHMAP ){` |
|     3843 | 1586 | `				PH7_HashmapUnion(pMap,(ph7_hashmap *)pObj2->x.pOther);` |
|     1924 | 1587 | `			}else{` |
|      ! 0 | 1588 | `				if((pObj2->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 1589 | `					/* Simple insertion */` |
|      ! 0 | 1590 | `					PH7_HashmapInsert(pMap,0,pObj2);` |
|      ! 0 | 1591 | `				}` |
|        - | 1592 | `			}` |
|        - | 1593 | `			/* Reflect the change */` |
|     3843 | 1594 | `			if( pObj1->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 1595 | `				SyBlobRelease(&pObj1->sBlob);` |
|      ! 0 | 1596 | `			}` |
|     3843 | 1597 | `			pObj1->x.pOther = pMap;` |
|     3843 | 1598 | `			MemObjSetType(pObj1,MEMOBJ_HASHMAP);` |
|     1919 | 1599 | `		}` |
|        - | 1600 | `	}` |
|    10763 | 1601 | `	return SXRET_OK;` |
|     5384 | 1602 | `}` |
|        - | 1603 | `/*` |
|        - | 1604 | ` * Return a printable representation of the type of a given` |
|        - | 1605 | ` * ph7_value.` |
|        - | 1606 | ` */` |
|       40 | 1607 | `PH7_PRIVATE const char * PH7_MemObjTypeDump(ph7_value *pVal)` |
|        3 | 1608 | `{` |
|       43 | 1609 | `	const char *zType = "";` |
|       43 | 1610 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|        3 | 1611 | `		zType = "null";` |
|       42 | 1612 | `	}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|        - | 1613 | `		/* REAL is authoritative over a cached MEMOBJ_INT: an integer-valued` |
|        - | 1614 | `		 * real (e.g. 1.0) is reported as "double", matching PHP's gettype(). */` |
|        7 | 1615 | `		zType = "double";` |
|       38 | 1616 | `	}else if( pVal->iFlags & MEMOBJ_INT ){` |
|        3 | 1617 | `		zType = "int";` |
|       34 | 1618 | `	}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|       25 | 1619 | `		zType = "string";` |
|       20 | 1620 | `	}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 | 1621 | `		zType = "bool";` |
|        9 | 1622 | `	}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|        3 | 1623 | `		zType = "array";` |
|        8 | 1624 | `	}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|        7 | 1625 | `		zType = "object";` |
|        3 | 1626 | `	}else if( pVal->iFlags & MEMOBJ_RES ){` |
|      ! 0 | 1627 | `		zType = "resource";` |
|      ! 0 | 1628 | `	}` |
|       43 | 1629 | `	return zType;` |
|        3 | 1630 | `}` |
|        - | 1631 | `/*` |
|        - | 1632 | ` * Dump a ph7_value [i.e: get a printable representation of it's type and contents.].` |
|        - | 1633 | ` * Store the dump in the given blob.` |
|        - | 1634 | ` */` |
|        - | 1635 | `/*` |
|        - | 1636 | ` * php's var_dump float shape (serialize_precision = -1): the SHORTEST decimal` |
|        - | 1637 | ` * string that round-trips to the same double — 0.1+0.2 dumps every digit` |
|        - | 1638 | ` * (0.30000000000000004), 1.0 dumps "1" — pushed through the same` |
|        - | 1639 | ` * exponent/fraction normalization as echo (PH7_PhpFloatShape: uppercase E,` |
|        - | 1640 | ` * "1.0E+100"). Distinct from echo/casts, which use EG(precision)=14.` |
|        - | 1641 | ` */` |
|        4 | 1642 | `static void MemObjDumpRealValue(SyBlob *pOut,ph7_real rVal)` |
|        1 | 1643 | `{` |
|        5 | 1644 | `	if( PH7_IS_NAN(rVal) ){` |
|      ! 0 | 1645 | `		SyBlobAppend(&(*pOut),"NAN",3);` |
|      ! 0 | 1646 | `		return;` |
|        - | 1647 | `	}` |
|        5 | 1648 | `	if( PH7_IS_INF(rVal) ){` |
|      ! 0 | 1649 | `		SyBlobAppend(&(*pOut),rVal < 0.0 ? "-INF" : "INF",rVal < 0.0 ? 4 : 3);` |
|      ! 0 | 1650 | `		return;` |
|        - | 1651 | `	}` |
|        - | 1652 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - | 1653 | `	{` |
|        - | 1654 | `		char zNum[48];` |
|        5 | 1655 | `		sxi32 n = 0;` |
|        - | 1656 | `		int p;` |
|        7 | 1657 | `		for( p = 1 ; p <= 17 ; p++ ){` |
|        7 | 1658 | `			n = (sxi32)snprintf(zNum,sizeof(zNum),"%.*G",p,rVal);` |
|        7 | 1659 | `			if( n < 0 \|\| n >= (sxi32)sizeof(zNum) ){` |
|      ! 0 | 1660 | `				n = (sxi32)SyStrlen(zNum);` |
|      ! 0 | 1661 | `			}` |
|        7 | 1662 | `			if( strtod(zNum,0) == rVal ){` |
|        5 | 1663 | `				break; /* shortest round-trip found */` |
|        - | 1664 | `			}` |
|        2 | 1665 | `		}` |
|        5 | 1666 | `		n = PH7_PhpFloatShape(zNum,n,TRUE);` |
|        5 | 1667 | `		SyBlobAppend(&(*pOut),zNum,(sxu32)n);` |
|        - | 1668 | `	}` |
|        - | 1669 | `#else` |
|        - | 1670 | `	SyBlobFormat(&(*pOut),"%.15g",rVal);` |
|        - | 1671 | `#endif` |
|        3 | 1672 | `}` |
|        - | 1673 | `/*` |
|        - | 1674 | ` * Emit a value's print_r INLINE representation (php: the echo conversion,` |
|        - | 1675 | ` * except true -> "1" and false/null -> ""). Containers never come through` |
|        - | 1676 | ` * here — the entry renderers recurse into the container dumpers instead.` |
|        - | 1677 | ` */` |
|       34 | 1678 | `PH7_PRIVATE void PH7_MemObjPrintRInline(SyBlob *pOut,ph7_value *pObj)` |
|        2 | 1679 | `{` |
|       36 | 1680 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|      ! 0 | 1681 | `		return;` |
|        - | 1682 | `	}` |
|       36 | 1683 | `	if( pObj->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 | 1684 | `		if( pObj->x.iVal != 0 ){` |
|      ! 0 | 1685 | `			SyBlobAppend(&(*pOut),"1",sizeof(char));` |
|      ! 0 | 1686 | `		}` |
|      ! 0 | 1687 | `		return;` |
|        - | 1688 | `	}` |
|       36 | 1689 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|        - | 1690 | `		/* Strings already hold their bytes (MemObjStringValue only CONVERTS` |
|        - | 1691 | `		 * non-strings into the output) */` |
|       20 | 1692 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       20 | 1693 | `			SyBlobAppend(&(*pOut),SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|        9 | 1694 | `		}` |
|       20 | 1695 | `		return;` |
|        - | 1696 | `	}` |
|       18 | 1697 | `	MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|       19 | 1698 | `}` |
|      450 | 1699 | `PH7_PRIVATE sxi32 PH7_MemObjDump(` |
|        - | 1700 | `	SyBlob *pOut,      /* Store the dump here */` |
|        - | 1701 | `	ph7_value *pObj,   /* Dump this */` |
|        - | 1702 | `	int ShowType,      /* TRUE for var_dump; FALSE for print_r */` |
|        - | 1703 | `	int nTab,          /* Indent in SPACES: var_dump = this value's own line;` |
|        - | 1704 | `	                    * print_r = the container's parenthesis column */` |
|        - | 1705 | `	int nDepth,        /* Nesting level */` |
|        - | 1706 | `	int isRef          /* TRUE if referenced entry (var_dump prints '&') */` |
|        - | 1707 | `	)` |
|        4 | 1708 | `{` |
|      454 | 1709 | `	sxi32 rc = SXRET_OK;` |
|        - | 1710 | `	int i;` |
|      454 | 1711 | `	if( !ShowType ){` |
|        - | 1712 | `		/* ---- print_r ---- php prints scalars inline with NO newline; only` |
|        - | 1713 | `		 * containers render the Array/Object block (which the container` |
|        - | 1714 | `		 * dumpers terminate with ")\n"). References carry no marker. */` |
|       18 | 1715 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       11 | 1716 | `			return PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,FALSE,nTab,nDepth+1);` |
|        - | 1717 | `		}` |
|        8 | 1718 | `		if( (pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|        6 | 1719 | `			return PH7_ClassInstanceDump(&(*pOut),(ph7_class_instance *)pObj->x.pOther,FALSE,nTab,nDepth+1);` |
|        - | 1720 | `		}` |
|        3 | 1721 | `		PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|        3 | 1722 | `		return SXRET_OK;` |
|        - | 1723 | `	}` |
|        - | 1724 | `	/* ---- var_dump ---- every value renders on its own line at nTab spaces,` |
|        - | 1725 | `	 * php's exact shapes: bool(true), NULL, int(n), float(shortest),` |
|        - | 1726 | `	 * string(N) "s", array(N) { … }, object(C)#id (n) { … }, &-references. */` |
|     4494 | 1727 | `	for( i = 0 ; i < nTab ; i++ ){` |
|     4060 | 1728 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     2032 | 1729 | `	}` |
|      438 | 1730 | `	if( isRef ){` |
|      ! 0 | 1731 | `		SyBlobAppend(&(*pOut),"&",sizeof(char));` |
|      ! 0 | 1732 | `	}` |
|      438 | 1733 | `	if( (pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|      139 | 1734 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|      139 | 1735 | `		if( pInst->pClass->iFlags & PH7_CLASS_ENUM ){` |
|        - | 1736 | ``			/* php 8.1: var_dump of an enum case prints `enum(S::A)` — no body */`` |
|      ! 0 | 1737 | `			ph7_value *pName = PH7_EnumCaseNameValue(pInst);` |
|      ! 0 | 1738 | `			SyBlobFormat(&(*pOut),"enum(%z::",&pInst->pClass->sName);` |
|      ! 0 | 1739 | `			if( pName && SyBlobLength(&pName->sBlob) > 0 ){` |
|      ! 0 | 1740 | `				SyBlobAppend(&(*pOut),SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));` |
|      ! 0 | 1741 | `			}` |
|      ! 0 | 1742 | `			SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|      ! 0 | 1743 | `			return SXRET_OK;` |
|        - | 1744 | `		}` |
|      139 | 1745 | `		rc = PH7_ClassInstanceDump(&(*pOut),pInst,TRUE,nTab,nDepth+1);` |
|      139 | 1746 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      139 | 1747 | `		return rc;` |
|        - | 1748 | `	}` |
|      302 | 1749 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|        3 | 1750 | `		SyBlobAppend(&(*pOut),"NULL\n",sizeof("NULL\n")-1);` |
|        3 | 1751 | `		return SXRET_OK;` |
|        - | 1752 | `	}` |
|      300 | 1753 | `	if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       14 | 1754 | `		rc = PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,TRUE,nTab,nDepth+1);` |
|       14 | 1755 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       14 | 1756 | `		return rc;` |
|        - | 1757 | `	}` |
|      288 | 1758 | `	if( pObj->iFlags & MEMOBJ_BOOL ){` |
|      109 | 1759 | `		if( pObj->x.iVal != 0 ){` |
|       65 | 1760 | `			SyBlobAppend(&(*pOut),"bool(true)\n",sizeof("bool(true)\n")-1);` |
|       34 | 1761 | `		}else{` |
|       46 | 1762 | `			SyBlobAppend(&(*pOut),"bool(false)\n",sizeof("bool(false)\n")-1);` |
|        - | 1763 | `		}` |
|      109 | 1764 | `		return SXRET_OK;` |
|        - | 1765 | `	}` |
|      182 | 1766 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - | 1767 | `		/* Checked BEFORE the int flag: an integer-valued real carries a cached` |
|        - | 1768 | `		 * MEMOBJ_INT view too, and php dumps it as float(1). */` |
|        5 | 1769 | `		SyBlobAppend(&(*pOut),"float(",sizeof("float(")-1);` |
|        5 | 1770 | `		MemObjDumpRealValue(&(*pOut),pObj->rVal);` |
|        5 | 1771 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        5 | 1772 | `		return SXRET_OK;` |
|        - | 1773 | `	}` |
|      178 | 1774 | `	if( pObj->iFlags & MEMOBJ_INT ){` |
|      127 | 1775 | `		SyBlobFormat(&(*pOut),"int(%qd)",pObj->x.iVal);` |
|      127 | 1776 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      127 | 1777 | `		return SXRET_OK;` |
|        - | 1778 | `	}` |
|       54 | 1779 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|       54 | 1780 | `		SyBlobFormat(&(*pOut),"string(%u) \"",SyBlobLength(&pObj->sBlob));` |
|       54 | 1781 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       52 | 1782 | `			SyBlobAppend(&(*pOut),SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|       24 | 1783 | `		}` |
|       54 | 1784 | `		SyBlobAppend(&(*pOut),"\"\n",sizeof("\"\n")-1);` |
|       54 | 1785 | `		return SXRET_OK;` |
|        - | 1786 | `	}` |
|        - | 1787 | ``	/* Resources and anything else: the legacy `type(value)` shape (php's`` |
|        - | 1788 | ``	 * `resource(N) of type (stream)` needs the §8 typed-resource model). */`` |
|        - | 1789 | `	{` |
|      ! 0 | 1790 | `		const char *zType = PH7_MemObjTypeDump(pObj);` |
|      ! 0 | 1791 | `		SyBlobAppend(&(*pOut),zType,SyStrlen(zType));` |
|      ! 0 | 1792 | `		SyBlobAppend(&(*pOut),"(",sizeof(char));` |
|      ! 0 | 1793 | `		MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|      ! 0 | 1794 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        - | 1795 | `	}` |
|      ! 0 | 1796 | `	return rc;` |
|      229 | 1797 | `}` |
|        - | 1798 |  |
