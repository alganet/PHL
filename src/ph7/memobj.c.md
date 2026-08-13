# src/ph7/memobj.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 870/969 lines (89.78%)

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
|        - |    9 |  |
|        - |   10 | `/* Portable 64-bit overflow-detecting arithmetic for compilers that lack the` |
|        - |   11 | ` * GCC/Clang __builtin_*_overflow intrinsics (i.e. MSVC). The header exposes` |
|        - |   12 | ` * these through the PH7_{ADD,SUB,MUL}_OVERFLOW64 macros; the intrinsic path` |
|        - |   13 | ` * needs no out-of-line definition, so gate the whole block off there to avoid` |
|        - |   14 | ` * an unused-function warning. Each sets *pR to the two's-complement wrapped` |
|        - |   15 | ` * result and returns non-zero on overflow. The additive checks compute the` |
|        - |   16 | ` * wrapped result via unsigned math (no signed-overflow UB) and test the sign` |
|        - |   17 | ` * bits; the multiplicative check mirrors vm.c's proven bound-check form. */` |
|        - |   18 | `#if !(defined(__GNUC__) \|\| defined(__clang__))` |
|        - |   19 | `PH7_PRIVATE int PH7_AddOverflow64(sxi64 a,sxi64 b,sxi64 *pR)` |
|        5 |   20 | `{` |
|        5 |   21 | `	*pR = (sxi64)((sxu64)a + (sxu64)b);` |
|        - |   22 | `	/* Overflow iff the operands share a sign and the result's sign differs. */` |
|        5 |   23 | `	return ((a ^ *pR) & (b ^ *pR)) < 0;` |
|        5 |   24 | `}` |
|        - |   25 | `PH7_PRIVATE int PH7_SubOverflow64(sxi64 a,sxi64 b,sxi64 *pR)` |
|        5 |   26 | `{` |
|        5 |   27 | `	*pR = (sxi64)((sxu64)a - (sxu64)b);` |
|        - |   28 | `	/* Overflow iff the operands differ in sign and the result's sign differs` |
|        - |   29 | `	 * from the minuend's. */` |
|        5 |   30 | `	return ((a ^ b) & (a ^ *pR)) < 0;` |
|        5 |   31 | `}` |
|        - |   32 | `PH7_PRIVATE int PH7_MulOverflow64(sxi64 a,sxi64 b,sxi64 *pR)` |
|        5 |   33 | `{` |
|        5 |   34 | `	*pR = (sxi64)((sxu64)a * (sxu64)b);` |
|        5 |   35 | `	if( a == 0 \|\| b == 0 \|\| a == 1 \|\| b == 1 ){` |
|        4 |   36 | `		return 0;` |
|        - |   37 | `	}` |
|        5 |   38 | `	if( a == -1 ){` |
|        1 |   39 | `		return b == SMALLEST_INT64;` |
|        - |   40 | `	}` |
|        5 |   41 | `	if( b == -1 ){` |
|      ! 0 |   42 | `		return a == SMALLEST_INT64;` |
|        - |   43 | `	}` |
|        5 |   44 | `	if( a > 0 ){` |
|        5 |   45 | `		if( b > 0 ){` |
|        5 |   46 | `			return a > LARGEST_INT64 / b;` |
|      ! 0 |   47 | `		}else{` |
|        1 |   48 | `			return b < SMALLEST_INT64 / a;` |
|        - |   49 | `		}` |
|      ! 0 |   50 | `	}else{` |
|        1 |   51 | `		if( b > 0 ){` |
|        1 |   52 | `			return a < SMALLEST_INT64 / b;` |
|      ! 0 |   53 | `		}else{` |
|        1 |   54 | `			return b < LARGEST_INT64 / a;` |
|        - |   55 | `		}` |
|        - |   56 | `	}` |
|        5 |   57 | `}` |
|        - |   58 | `#endif` |
|        - |   59 |  |
|        - |   60 | `/* Provide PHP-style type names for values.  This utility may be reused` |
|        - |   61 | ` * by any subsystem that works with ph7_value.` |
|        - |   62 | ` */` |
|      456 |   63 | `PH7_PRIVATE const char *ph7_type_name(ph7_value *pVal)` |
|        5 |   64 | `{` |
|      461 |   65 | `	if( ph7_value_is_null(pVal) ) return "null";` |
|      419 |   66 | `	if( ph7_value_is_bool(pVal) ) return "bool";` |
|      411 |   67 | `	if( ph7_value_is_int(pVal) ) return "int";` |
|      353 |   68 | `	if( ph7_value_is_float(pVal) ) return "float";` |
|      343 |   69 | `	if( ph7_value_is_string(pVal) ) return "string";` |
|      153 |   70 | `	if( ph7_value_is_array(pVal) ) return "array";` |
|       43 |   71 | `	if( ph7_value_is_object(pVal) ) return "object";` |
|        3 |   72 | `	if( ph7_value_is_resource(pVal) ) return "resource";` |
|      ! 0 |   73 | `	return "unknown";` |
|      233 |   74 | `}` |
|        - |   75 |  |
|        - |   76 | `/*` |
|        - |   77 | ` * Notes on memory objects [i.e: ph7_value].` |
|        - |   78 | ` * Internally, the PH7 virtual machine manipulates nearly all PHP values` |
|        - |   79 | ` * [i.e: string,int,float,resource,object,bool,null..] as ph7_values structures.` |
|        - |   80 | ` * Each ph7_values struct may cache multiple representations (string,` |
|        - |   81 | ` * integer etc.) of the same value.` |
|        - |   82 | ` */` |
|        - |   83 | `/*` |
|        - |   84 | ` * Convert a 64-bit IEEE double into a 64-bit signed integer.` |
|        - |   85 | ` * If the double is too large, return 0x8000000000000000.` |
|        - |   86 | ` *` |
|        - |   87 | ` * Most systems appear to do this simply by assigning ariables and without` |
|        - |   88 | ` * the extra range tests.` |
|        - |   89 | ` * But there are reports that windows throws an expection if the floating` |
|        - |   90 | ` * point value is out of range.` |
|        - |   91 | ` */` |
|     3008 |   92 | `static sxi64 MemObjRealToInt(ph7_value *pObj)` |
|        4 |   93 | `{` |
|        - |   94 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |   95 | `	/* Real and 64bit integer are the same when floating point arithmetic` |
|        - |   96 | `	 * is omitted from the build.` |
|        - |   97 | `	 */` |
|        - |   98 | `	return pObj->rVal;` |
|        - |   99 | `#else` |
|        - |  100 | ` /*` |
|        - |  101 | `  ** Many compilers we encounter do not define constants for the` |
|        - |  102 | `  ** minimum and maximum 64-bit integers, or they define them` |
|        - |  103 | `  ** inconsistently.  And many do not understand the "LL" notation.` |
|        - |  104 | `  ** So we define our own static constants here using nothing` |
|        - |  105 | `  ** larger than a 32-bit integer constant.` |
|        - |  106 | `  */` |
|        - |  107 | `  static const sxi64 maxInt = LARGEST_INT64;` |
|        - |  108 | `  static const sxi64 minInt = SMALLEST_INT64;` |
|     3012 |  109 | `  ph7_real r = pObj->rVal;` |
|     3012 |  110 | `  if( r<(ph7_real)minInt ){` |
|        3 |  111 | `    return minInt;` |
|     3010 |  112 | `  }else if( r>(ph7_real)maxInt ){` |
|        - |  113 | `    /* minInt is correct here - not maxInt.  It turns out that assigning` |
|        - |  114 | `    ** a very large positive number to an integer results in a very large` |
|        - |  115 | `    ** negative integer.  This makes no sense, but it is what x86 hardware` |
|        - |  116 | `    ** does so for compatibility we will do the same in software. */` |
|      157 |  117 | `    return minInt;` |
|      ! 0 |  118 | `  }else{` |
|     2854 |  119 | `    return (sxi64)r;` |
|        - |  120 | `  }` |
|        - |  121 | `#endif` |
|     1508 |  122 | `}` |
|        - |  123 | `/*` |
|        - |  124 | ` * Convert a raw token value typically a stream of digit [i.e: hex,octal,binary or decimal]` |
|        - |  125 | ` * to a 64-bit integer.` |
|        - |  126 | ` */` |
|  1288548 |  127 | `PH7_PRIVATE sxi64 PH7_TokenValueToInt64(SyString *pVal)` |
|        5 |  128 | `{` |
|  1288553 |  129 | `	sxi64 iVal = 0;` |
|  1288553 |  130 | `	if( pVal->nByte <= 0 ){` |
|        7 |  131 | `		return 0;` |
|        - |  132 | `	}` |
|  1288547 |  133 | `	if( pVal->zString[0] == '0' ){` |
|        - |  134 | `		sxi32 c;` |
|   355942 |  135 | `		if( pVal->nByte == sizeof(char) ){` |
|   355529 |  136 | `			return 0;` |
|        - |  137 | `		}` |
|      414 |  138 | `		c = pVal->zString[1];` |
|      414 |  139 | `		if( c  == 'x' \|\| c == 'X' ){` |
|        - |  140 | `			/* Hex digit stream */` |
|       71 |  141 | `			SyHexStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      379 |  142 | `		}else if( c == 'b' \|\| c == 'B' ){` |
|        - |  143 | `			/* Binary digit stream */` |
|      279 |  144 | `			SyBinaryStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      140 |  145 | `		}else{` |
|        - |  146 | `			/* Octal digit stream */` |
|       66 |  147 | `			SyOctalStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|        - |  148 | `		}` |
|      208 |  149 | `	}else{` |
|        - |  150 | `		/* Decimal digit stream */` |
|   932610 |  151 | `		SyStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|        - |  152 | `	}` |
|   933023 |  153 | `	return iVal;` |
|   644279 |  154 | `}` |
|        - |  155 | `/*` |
|        - |  156 | ` * Return some kind of 64-bit integer value which is the best we can` |
|        - |  157 | ` * do at representing the value that pObj describes as a string` |
|        - |  158 | ` * representation.` |
|        - |  159 | ` */` |
|      502 |  160 | `static sxi64 MemObjStringToInt(ph7_value *pObj)` |
|        5 |  161 | `{` |
|        - |  162 | `	SyString sVal;` |
|      507 |  163 | `	SyStringInitFromBuf(&sVal,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|      507 |  164 | `	return PH7_TokenValueToInt64(&sVal);` |
|        5 |  165 | `}` |
|        - |  166 | `/*` |
|        - |  167 | ` * Call a magic class method [i.e: __toString(),__toInt(),...]` |
|        - |  168 | ` * Return SXRET_OK if the magic method is available and have been` |
|        - |  169 | ` * successfully called. Any other return value indicates failure.` |
|        - |  170 | ` */` |
|      180 |  171 | `static sxi32 MemObjCallClassCastMethod(` |
|        - |  172 | `	ph7_vm *pVm,               /* VM that trigger the invocation */` |
|        - |  173 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object] */` |
|        - |  174 | `	const char *zMethod,       /* Magic method name [i.e: __toString] */` |
|        - |  175 | `	sxu32 nLen,                /* Method name length */` |
|        - |  176 | `	ph7_value *pResult         /* OUT: Store the return value of the magic method here */` |
|        - |  177 | `	)` |
|        5 |  178 | `{` |
|        - |  179 | `	ph7_class_method *pMethod;` |
|        - |  180 | `	/* Check if the method is available */` |
|      185 |  181 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,zMethod,nLen);` |
|      185 |  182 | `	if( pMethod == 0 ){` |
|        - |  183 | `		/* No such method */` |
|        6 |  184 | `		return SXERR_NOTFOUND;` |
|        - |  185 | `	}` |
|        - |  186 | `	/* Invoke the desired method */` |
|      181 |  187 | `	PH7_VmCallClassMethod(&(*pVm),&(*pThis),pMethod,&(*pResult),0,0);` |
|        - |  188 | `	/* Method successfully called,pResult should hold the return value */` |
|      181 |  189 | `	return SXRET_OK;` |
|       95 |  190 | `}` |
|        - |  191 | `/*` |
|        - |  192 | ` * Return some kind of integer value which is the best we can` |
|        - |  193 | ` * do at representing the value that pObj describes as an integer.` |
|        - |  194 | ` * If pObj is an integer, then the value is exact. If pObj is` |
|        - |  195 | ` * a floating-point then  the value returned is the integer part.` |
|        - |  196 | ` * If pObj is a string, then we make an attempt to convert it into` |
|        - |  197 | ` * a integer and return that.` |
|        - |  198 | ` * If pObj represents a NULL value, return 0.` |
|        - |  199 | ` */` |
|      594 |  200 | `static sxi64 MemObjIntValue(ph7_value *pObj)` |
|        5 |  201 | `{` |
|        - |  202 | `	sxi32 iFlags;` |
|      599 |  203 | `	iFlags = pObj->iFlags;` |
|      599 |  204 | `	if (iFlags & MEMOBJ_REAL ){` |
|       39 |  205 | `		return MemObjRealToInt(&(*pObj));` |
|      561 |  206 | `	}else if( iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      148 |  207 | `		return pObj->x.iVal;` |
|      415 |  208 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|      393 |  209 | `		return MemObjStringToInt(&(*pObj));` |
|       23 |  210 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|       11 |  211 | `		return 0;` |
|       13 |  212 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  213 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        7 |  214 | `		sxu32 n = pMap->nEntry;` |
|        7 |  215 | `		PH7_HashmapUnref(pMap);` |
|        - |  216 | `		/* Return total number of entries in the hashmap */` |
|        7 |  217 | `		return n;` |
|        7 |  218 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|        - |  219 | `		ph7_value sResult;` |
|        5 |  220 | `		sxi64 iVal = 1;` |
|        - |  221 | `		sxi32 rc;` |
|        - |  222 | `		/* Invoke the [__toInt()] magic method if available [note that this is a symisc extension]  */` |
|        5 |  223 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|        5 |  224 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  225 | `			"__toInt",sizeof("__toInt")-1,&sResult);` |
|        5 |  226 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_INT) ){` |
|        - |  227 | `			/* Extract method return value */` |
|        5 |  228 | `			iVal = sResult.x.iVal;` |
|        2 |  229 | `		}` |
|        5 |  230 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|        5 |  231 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  232 | `		return iVal;` |
|        3 |  233 | `	}else if(iFlags & MEMOBJ_RES ){` |
|        3 |  234 | `		return pObj->x.pOther != 0;` |
|        - |  235 | `	}` |
|        - |  236 | `	/* CANT HAPPEN */` |
|      ! 0 |  237 | `	return 0;` |
|      302 |  238 | `}` |
|        - |  239 | `/*` |
|        - |  240 | ` * Return some kind of real value which is the best we can` |
|        - |  241 | ` * do at representing the value that pObj describes as a real.` |
|        - |  242 | ` * If pObj is a real, then the value is exact.If pObj is an` |
|        - |  243 | ` * integer then the integer  is promoted to real and that value` |
|        - |  244 | ` * is returned.` |
|        - |  245 | ` * If pObj is a string, then we make an attempt to convert it` |
|        - |  246 | ` * into a real and return that.` |
|        - |  247 | ` * If pObj represents a NULL value, return 0.0` |
|        - |  248 | ` */` |
|     1784 |  249 | `static ph7_real MemObjRealValue(ph7_value *pObj)` |
|        4 |  250 | `{` |
|        - |  251 | `	sxi32 iFlags;` |
|     1788 |  252 | `	iFlags = pObj->iFlags;` |
|     1788 |  253 | `	if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  254 | `		return pObj->rVal;` |
|     1788 |  255 | `	}else if (iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      772 |  256 | `		return (ph7_real)pObj->x.iVal;` |
|     1018 |  257 | `	}else if (iFlags & MEMOBJ_STRING){` |
|        - |  258 | `		SyString sString;` |
|        - |  259 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  260 | `		ph7_real rVal = 0;` |
|        - |  261 | `#else` |
|     1012 |  262 | `		ph7_real rVal = 0.0;` |
|        - |  263 | `#endif` |
|     1012 |  264 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|     1012 |  265 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        - |  266 | `			/* Convert as much as we can */` |
|        - |  267 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  268 | `			rVal = MemObjStringToInt(&(*pObj));` |
|        - |  269 | `#else` |
|     1012 |  270 | `			SyStrToReal(sString.zString,sString.nByte,(void *)&rVal,0);` |
|        - |  271 | `#endif` |
|      504 |  272 | `		}` |
|     1012 |  273 | `		return rVal;` |
|        7 |  274 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        - |  275 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  276 | `		return 0;` |
|        - |  277 | `#else` |
|      ! 0 |  278 | `		return 0.0;` |
|        - |  279 | `#endif` |
|        7 |  280 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        - |  281 | `		/* Return the total number of entries in the hashmap */` |
|      ! 0 |  282 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      ! 0 |  283 | `		ph7_real n = (ph7_real)pMap->nEntry;` |
|      ! 0 |  284 | `		PH7_HashmapUnref(pMap);` |
|      ! 0 |  285 | `		return n;` |
|        7 |  286 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|        - |  287 | `		ph7_value sResult;` |
|        5 |  288 | `		ph7_real rVal = 1;` |
|        - |  289 | `		sxi32 rc;` |
|        - |  290 | `		/* Invoke the [__toFloat()] magic method if available [note that this is a symisc extension]  */` |
|        5 |  291 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|        5 |  292 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  293 | `			"__toFloat",sizeof("__toFloat")-1,&sResult);` |
|        5 |  294 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_REAL) ){` |
|        - |  295 | `			/* Extract method return value */` |
|        5 |  296 | `			rVal = sResult.rVal;` |
|        2 |  297 | `		}` |
|        5 |  298 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|        5 |  299 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  300 | `		return rVal;` |
|        3 |  301 | `	}else if(iFlags & MEMOBJ_RES ){` |
|        3 |  302 | `		return (ph7_real)(pObj->x.pOther != 0);` |
|        - |  303 | `	}` |
|        - |  304 | `	/* NOT REACHED  */` |
|      ! 0 |  305 | `	return 0;` |
|      896 |  306 | `}` |
|        - |  307 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  308 | `/*` |
|        - |  309 | ` * Post-process a libc-formatted float into php's exact shape (php_gcvt /` |
|        - |  310 | ` * smart_str_append_double semantics): strip the exponent's zero padding` |
|        - |  311 | ` * (libc's 1e+08 becomes php's 1e+8; a zero exponent stays e+0) and, when` |
|        - |  312 | ` * bGeneric is set (%g-style output, including the default float->string` |
|        - |  313 | ` * cast), make an exponent-form mantissa keep a fractional digit` |
|        - |  314 | ` * (1e+20 -> 1.0e+20). zBuf must be NUL-terminated with at least two bytes` |
|        - |  315 | ` * of spare capacity past the NUL. Returns the new length.` |
|        - |  316 | ` * Defined here (not builtin.c) because the float->string cast below needs it` |
|        - |  317 | ` * even when builtin.c's formatting region is compiled out` |
|        - |  318 | ` * (PH7_DISABLE_DISK_IO); the printf family reuses it from PH7_InputFormat.` |
|        - |  319 | ` */` |
|      498 |  320 | `PH7_PRIVATE sxi32 PH7_PhpFloatShape(char *zBuf,sxi32 nLen,int bGeneric)` |
|        4 |  321 | `{` |
|        - |  322 | `	sxi32 iExp,i;` |
|      502 |  323 | `	iExp = nLen - 1;` |
|     4198 |  324 | `	while( iExp > 0 && zBuf[iExp] != 'e' && zBuf[iExp] != 'E' ){` |
|     3700 |  325 | `		iExp--;` |
|        4 |  326 | `	}` |
|      502 |  327 | `	if( iExp <= 0 ){` |
|      456 |  328 | `		return nLen; /* No exponent part (fixed notation) */` |
|        - |  329 | `	}` |
|        - |  330 | `	{` |
|       47 |  331 | `		sxi32 iDig = iExp + 1;` |
|        - |  332 | `		sxi32 iFirst;` |
|       47 |  333 | `		if( zBuf[iDig] == '+' \|\| zBuf[iDig] == '-' ){` |
|       47 |  334 | `			iDig++;` |
|       23 |  335 | `		}` |
|       47 |  336 | `		iFirst = iDig;` |
|       83 |  337 | `		while( zBuf[iFirst] == '0' && iFirst + 1 < nLen` |
|       61 |  338 | `		 && zBuf[iFirst+1] >= '0' && zBuf[iFirst+1] <= '9' ){` |
|       25 |  339 | `			iFirst++;` |
|        1 |  340 | `		}` |
|       47 |  341 | `		if( iFirst > iDig ){` |
|       25 |  342 | `			sxi32 nStrip = iFirst - iDig;` |
|       73 |  343 | `			for( i = iDig ; i + nStrip <= nLen ; i++ ){` |
|       49 |  344 | `				zBuf[i] = zBuf[i+nStrip]; /* moves the NUL too */` |
|       25 |  345 | `			}` |
|       25 |  346 | `			nLen -= nStrip;` |
|       12 |  347 | `		}` |
|        - |  348 | `	}` |
|       47 |  349 | `	if( bGeneric ){` |
|       31 |  350 | `		int bHasDot = 0;` |
|       63 |  351 | `		for( i = 0 ; i < iExp ; i++ ){` |
|       45 |  352 | `			if( zBuf[i] == '.' ){ bHasDot = 1; break; }` |
|       17 |  353 | `		}` |
|       31 |  354 | `		if( !bHasDot ){` |
|      107 |  355 | `			for( i = nLen ; i >= iExp ; i-- ){` |
|       89 |  356 | `				zBuf[i+2] = zBuf[i]; /* moves the NUL too */` |
|       45 |  357 | `			}` |
|       19 |  358 | `			zBuf[iExp] = '.';` |
|       19 |  359 | `			zBuf[iExp+1] = '0';` |
|       19 |  360 | `			nLen += 2;` |
|        9 |  361 | `		}` |
|       15 |  362 | `	}` |
|       47 |  363 | `	return nLen;` |
|      253 |  364 | `}` |
|        - |  365 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|        - |  366 | `/*` |
|        - |  367 | ` * Return the string representation of a given ph7_value.` |
|        - |  368 | ` * This function never fail and always return SXRET_OK.` |
|        - |  369 | ` */` |
|    58016 |  370 | `static sxi32 MemObjStringValue(SyBlob *pOut,ph7_value *pObj,sxu8 bStrictBool)` |
|        5 |  371 | `{` |
|    58021 |  372 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - |  373 | `		/* Handle special floating-point values first */` |
|      386 |  374 | `		if( PH7_IS_NAN(pObj->rVal) ){` |
|      ! 0 |  375 | `			SyBlobAppend(&(*pOut),"NAN",3);` |
|      386 |  376 | `		}else if( PH7_IS_INF(pObj->rVal) ){` |
|        5 |  377 | `			if( pObj->rVal < 0.0 ){` |
|      ! 0 |  378 | `				SyBlobAppend(&(*pOut),"-INF",4);` |
|      ! 0 |  379 | `			}else{` |
|        5 |  380 | `				SyBlobAppend(&(*pOut),"INF",3);` |
|        - |  381 | `			}` |
|        3 |  382 | `		}else{` |
|        - |  383 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  384 | `			/* php's default float->string conversion (echo/concat/cast):` |
|        - |  385 | `			 * zend_gcvt with EG(precision)=14 and an uppercase exponent` |
|        - |  386 | `			 * marker (smart_str_append_double) — 1/3 -> "0.33333333333333",` |
|        - |  387 | `			 * 1e15 -> "1.0E+15", -0.0 -> "-0". libc snprintf supplies` |
|        - |  388 | `			 * correctly-rounded digits; PH7_PhpFloatShape applies php's` |
|        - |  389 | `			 * exponent/fraction quirks. */` |
|        - |  390 | `			char zNum[48]; /* %.14G peaks at ~22 bytes; +2 spare for ".0" */` |
|      382 |  391 | `			sxi32 n = (sxi32)snprintf(zNum,sizeof(zNum),"%.14G",pObj->rVal);` |
|      382 |  392 | `			if( n < 0 \|\| n >= (sxi32)sizeof(zNum) ){` |
|      ! 0 |  393 | `				n = (sxi32)SyStrlen(zNum);` |
|      ! 0 |  394 | `			}` |
|      382 |  395 | `			n = PH7_PhpFloatShape(zNum,n,TRUE);` |
|      382 |  396 | `			SyBlobAppend(&(*pOut),zNum,(sxu32)n);` |
|        - |  397 | `#else` |
|        - |  398 | `			SyBlobFormat(&(*pOut),"%.15g",pObj->rVal);` |
|        - |  399 | `#endif` |
|        4 |  400 | `		}` |
|    57830 |  401 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|    57023 |  402 | `		SyBlobFormat(&(*pOut),"%qd",pObj->x.iVal);` |
|        - |  403 | `		/* %qd (BSD quad) is equivalent to %lld in the libc printf */` |
|    29130 |  404 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|      369 |  405 | `		if( bStrictBool ){` |
|        - |  406 | `			/* Actual string cast: true -> "1", false -> "" (like PHP) */` |
|      263 |  407 | `			if( pObj->x.iVal ){` |
|       29 |  408 | `				SyBlobAppend(&(*pOut),"1",sizeof("1")-1);` |
|       13 |  409 | `			}` |
|        - |  410 | `			/* false produces empty string, nothing to append */` |
|      134 |  411 | `		}else{` |
|        - |  412 | `			/* Display path (var_dump, print_r): show TRUE/FALSE */` |
|      109 |  413 | `			if( pObj->x.iVal ){` |
|       65 |  414 | `				SyBlobAppend(&(*pOut),"TRUE",sizeof("TRUE")-1);` |
|       34 |  415 | `			}else{` |
|       46 |  416 | `				SyBlobAppend(&(*pOut),"FALSE",sizeof("FALSE")-1);` |
|        - |  417 | `			}` |
|        5 |  418 | `		}` |
|      439 |  419 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 |  420 | `		SyBlobAppend(&(*pOut),"Array",sizeof("Array")-1);` |
|      ! 0 |  421 | `		PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|      257 |  422 | `	}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|        - |  423 | `		ph7_value sResult;` |
|        - |  424 | `		sxi32 rc;` |
|        - |  425 | `		/* Invoke the __toString() method if available */` |
|      171 |  426 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|      171 |  427 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  428 | `			"__toString",sizeof("__toString")-1,&sResult);` |
|      171 |  429 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_STRING) && SyBlobLength(&sResult.sBlob) > 0){` |
|        - |  430 | `			/* Expand method return value */` |
|       94 |  431 | `			SyBlobDup(&sResult.sBlob,pOut);` |
|       49 |  432 | `		}else{` |
|        - |  433 | `			/* Expand "Object" as requested by the PHP language reference manual */` |
|       78 |  434 | `			SyBlobAppend(&(*pOut),"Object",sizeof("Object")-1);` |
|        - |  435 | `		}` |
|      171 |  436 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|      171 |  437 | `		PH7_MemObjRelease(&sResult);` |
|      172 |  438 | `	}else if(pObj->iFlags & MEMOBJ_RES ){` |
|        3 |  439 | `		SyBlobFormat(&(*pOut),"ResourceID_%#x",pObj->x.pOther);` |
|        1 |  440 | `	}` |
|    58021 |  441 | `	return SXRET_OK;` |
|        5 |  442 | `}` |
|        - |  443 | `/*` |
|        - |  444 | ` * Return some kind of boolean value which is the best we can do` |
|        - |  445 | ` * at representing the value that pObj describes as a boolean.` |
|        - |  446 | ` * When converting to boolean, the following values are considered FALSE` |
|        - |  447 | ` * (php's exact set):` |
|        - |  448 | ` * NULL` |
|        - |  449 | ` * the boolean FALSE itself.` |
|        - |  450 | ` * the integer 0 (zero).` |
|        - |  451 | ` * the real 0.0 (zero).` |
|        - |  452 | ` * the empty string "" and the string "0" (nothing else: "00", "0.0", " ",` |
|        - |  453 | ` * and "false" are all TRUE in php — the historical PH7 zero-stream and` |
|        - |  454 | ` * "false"/"on"/"yes" special cases changed the meaning of valid PHP source` |
|        - |  455 | ` * and were removed under the §10 PH7-ism policy).` |
|        - |  456 | ` * an array with zero elements.` |
|        - |  457 | ` */` |
|    16224 |  458 | `static sxi32 MemObjBooleanValue(ph7_value *pObj)` |
|        5 |  459 | `{` |
|        - |  460 | `	sxi32 iFlags;` |
|    16229 |  461 | `	iFlags = pObj->iFlags;` |
|    16229 |  462 | `	if (iFlags & MEMOBJ_REAL ){` |
|        - |  463 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  464 | `		return pObj->rVal ? 1 : 0;` |
|        - |  465 | `#else` |
|       14 |  466 | `		return pObj->rVal != 0.0 ? 1 : 0;` |
|        - |  467 | `#endif` |
|    16217 |  468 | `	}else if( iFlags & MEMOBJ_INT ){` |
|      287 |  469 | `		return pObj->x.iVal ? 1 : 0;` |
|    15935 |  470 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|        - |  471 | `		SyString sString;` |
|      111 |  472 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|        - |  473 | `		/* php: a string is FALSE iff it is empty or exactly "0" */` |
|      111 |  474 | `		if( sString.nByte == 0 ){` |
|       19 |  475 | `			return 0;` |
|        - |  476 | `		}` |
|       94 |  477 | `		if( sString.nByte == 1 && sString.zString[0] == '0' ){` |
|        7 |  478 | `			return 0;` |
|        - |  479 | `		}` |
|       88 |  480 | `		return 1;` |
|    15827 |  481 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|    14775 |  482 | `		return 0;` |
|     1057 |  483 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|       22 |  484 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       22 |  485 | `		sxu32 n = pMap->nEntry;` |
|       22 |  486 | `		PH7_HashmapUnref(pMap);` |
|       22 |  487 | `		return n > 0 ? TRUE : FALSE;` |
|     1037 |  488 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|        - |  489 | `		ph7_value sResult;` |
|        7 |  490 | `		sxi32 iVal = 1;` |
|        - |  491 | `		sxi32 rc;` |
|        - |  492 | `		/* Invoke the __toBool() method if available [note that this is a symisc extension]  */` |
|        7 |  493 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|        7 |  494 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  495 | `			"__toBool",sizeof("__toBool")-1,&sResult);` |
|        7 |  496 | `		if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL)) ){` |
|        - |  497 | `			/* Extract method return value */` |
|        5 |  498 | `			iVal = (sxi32)(sResult.x.iVal != 0); /* Stupid cc warning -W -Wall -O6 */` |
|        2 |  499 | `		}` |
|        7 |  500 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|        7 |  501 | `		PH7_MemObjRelease(&sResult);` |
|        7 |  502 | `		return iVal;` |
|     1031 |  503 | `	}else if(iFlags & MEMOBJ_RES ){` |
|     1031 |  504 | `		return pObj->x.pOther != 0;` |
|        - |  505 | `	}` |
|        - |  506 | `	/* NOT REACHED */` |
|      ! 0 |  507 | `	return 0;` |
|     8117 |  508 | `}` |
|        - |  509 | `/*` |
|        - |  510 | ` * If the ph7_value is of type real,try to make it an integer also.` |
|        - |  511 | ` */` |
|     2970 |  512 | `static sxi32 MemObjTryIntger(ph7_value *pObj)` |
|        4 |  513 | `{` |
|     2974 |  514 | `	pObj->x.iVal = MemObjRealToInt(&(*pObj));` |
|        - |  515 | `  /* Only mark the value as an integer if` |
|        - |  516 | `  **` |
|        - |  517 | `  **    (1) the round-trip conversion real->int->real is a no-op, and` |
|        - |  518 | `  **    (2) The integer is neither the largest nor the smallest` |
|        - |  519 | `  **        possible integer` |
|        - |  520 | `  **` |
|        - |  521 | `  ** The second and third terms in the following conditional enforces` |
|        - |  522 | `  ** the second condition under the assumption that addition overflow causes` |
|        - |  523 | `  ** values to wrap around.  On x86 hardware, the third term is always` |
|        - |  524 | `  ** true and could be omitted.  But we leave it in because other` |
|        - |  525 | `  ** architectures might behave differently.` |
|        - |  526 | `  */` |
|     2970 |  527 | `	if( pObj->rVal ==(ph7_real)pObj->x.iVal && pObj->x.iVal>SMALLEST_INT64` |
|     1554 |  528 | `      && pObj->x.iVal<LARGEST_INT64 ){` |
|     1537 |  529 | `		  pObj->iFlags \|= MEMOBJ_INT;` |
|      769 |  530 | `	}` |
|     2974 |  531 | `	return SXRET_OK;` |
|        4 |  532 | `}` |
|        - |  533 | `/*` |
|        - |  534 | ` * Convert a ph7_value to type integer.Invalidate any prior representations.` |
|        - |  535 | ` */` |
|   439692 |  536 | `PH7_PRIVATE sxi32 PH7_MemObjToInteger(ph7_value *pObj)` |
|        5 |  537 | `{` |
|   439697 |  538 | `	if( (pObj->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  539 | `		/* Preform the conversion */` |
|      599 |  540 | `		pObj->x.iVal = MemObjIntValue(&(*pObj));` |
|        - |  541 | `		/* Invalidate any prior representations */` |
|      599 |  542 | `		SyBlobRelease(&pObj->sBlob);` |
|      599 |  543 | `		MemObjSetType(pObj,MEMOBJ_INT);` |
|      297 |  544 | `	}` |
|   439697 |  545 | `	return SXRET_OK;` |
|        5 |  546 | `}` |
|        - |  547 | `/*` |
|        - |  548 | ` * Convert a ph7_value to type real (Try to get an integer representation also).` |
|        - |  549 | ` * Invalidate any prior representations` |
|        - |  550 | ` */` |
|     2696 |  551 | `PH7_PRIVATE sxi32 PH7_MemObjToReal(ph7_value *pObj)` |
|        4 |  552 | `{` |
|     2700 |  553 | `	if((pObj->iFlags & MEMOBJ_REAL) == 0 ){` |
|        - |  554 | `		/* Preform the conversion */` |
|     1788 |  555 | `		pObj->rVal = MemObjRealValue(&(*pObj));` |
|        - |  556 | `		/* Invalidate any prior representations */` |
|     1788 |  557 | `		SyBlobRelease(&pObj->sBlob);` |
|     1788 |  558 | `		MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - |  559 | `		/* Try to get an integer representation */` |
|     1788 |  560 | `		MemObjTryIntger(&(*pObj));` |
|      892 |  561 | `	}` |
|     2700 |  562 | `	return SXRET_OK;` |
|        4 |  563 | `}` |
|        - |  564 | `/*` |
|        - |  565 | ` * Convert a ph7_value to type boolean.Invalidate any prior representations.` |
|        - |  566 | ` */` |
|    18508 |  567 | `PH7_PRIVATE sxi32 PH7_MemObjToBool(ph7_value *pObj)` |
|        5 |  568 | `{` |
|    18513 |  569 | `	if( (pObj->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        - |  570 | `		/* Preform the conversion */` |
|    16229 |  571 | `		pObj->x.iVal = MemObjBooleanValue(&(*pObj));` |
|        - |  572 | `		/* Invalidate any prior representations */` |
|    16229 |  573 | `		SyBlobRelease(&pObj->sBlob);` |
|    16229 |  574 | `		MemObjSetType(pObj,MEMOBJ_BOOL);` |
|     8112 |  575 | `	}` |
|    18513 |  576 | `	return SXRET_OK;` |
|        5 |  577 | `}` |
|        - |  578 | `/*` |
|        - |  579 | ` * Convert a ph7_value to type string.Prior representations are NOT invalidated.` |
|        - |  580 | ` */` |
|   853269 |  581 | `PH7_PRIVATE sxi32 PH7_MemObjToString(ph7_value *pObj)` |
|        5 |  582 | `{` |
|   853274 |  583 | `	sxi32 rc = SXRET_OK;` |
|   853274 |  584 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  585 | `		/* Perform the conversion */` |
|    57773 |  586 | `		SyBlobReset(&pObj->sBlob); /* Reset the internal buffer */` |
|    57773 |  587 | `		rc = MemObjStringValue(&pObj->sBlob,&(*pObj),TRUE);` |
|    57773 |  588 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|    28884 |  589 | `	}` |
|   853274 |  590 | `	return rc;` |
|        5 |  591 | `}` |
|        - |  592 | `/*` |
|        - |  593 | ` * Nullify a ph7_value.In other words invalidate any prior` |
|        - |  594 | ` * representation.` |
|        - |  595 | ` */` |
|      ! 0 |  596 | `PH7_PRIVATE sxi32 PH7_MemObjToNull(ph7_value *pObj)` |
|      ! 0 |  597 | `{` |
|      ! 0 |  598 | `	return PH7_MemObjRelease(pObj);` |
|      ! 0 |  599 | `}` |
|        - |  600 | `/*` |
|        - |  601 | ` * Convert a ph7_value to type array.Invalidate any prior representations.` |
|        - |  602 | `  * According to the PHP language reference manual.` |
|        - |  603 | `  *   For any of the types: integer, float, string, boolean converting a value` |
|        - |  604 | `  *   to an array results in an array with a single element with index zero` |
|        - |  605 | `  *   and the value of the scalar which was converted.` |
|        - |  606 | `  */` |
|      532 |  607 | `PH7_PRIVATE sxi32 PH7_MemObjToHashmap(ph7_value *pObj)` |
|        5 |  608 | `{` |
|      537 |  609 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  610 | `		ph7_hashmap *pMap;` |
|        - |  611 | `		/* Allocate a new hashmap instance */` |
|      345 |  612 | `		pMap = PH7_NewHashmap(pObj->pVm,0,0);` |
|      345 |  613 | `		if( pMap == 0 ){` |
|      ! 0 |  614 | `			return SXERR_MEM;` |
|        - |  615 | `		}` |
|      345 |  616 | `		if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|        - |  617 | `			/*` |
|        - |  618 | `			 * According to the PHP language reference manual.` |
|        - |  619 | `			 *   For any of the types: integer, float, string, boolean converting a value` |
|        - |  620 | `			 *   to an array results in an array with a single element with index zero` |
|        - |  621 | `			 *   and the value of the scalar which was converted.` |
|        - |  622 | `			 */` |
|       27 |  623 | `			if( pObj->iFlags & MEMOBJ_OBJ ){` |
|        - |  624 | `				/* Object cast */` |
|       13 |  625 | `				PH7_ClassInstanceToHashmap((ph7_class_instance *)pObj->x.pOther,pMap);` |
|        7 |  626 | `			}else{` |
|        - |  627 | `				/* Insert a single element */` |
|       15 |  628 | `				PH7_HashmapInsert(pMap,0/* Automatic index assign */,&(*pObj));` |
|        - |  629 | `			}` |
|       27 |  630 | `			SyBlobRelease(&pObj->sBlob);` |
|       13 |  631 | `		}` |
|        - |  632 | `		/* Invalidate any prior representation */` |
|      345 |  633 | `		PH7_MemObjRelease(pObj);` |
|      345 |  634 | `		MemObjSetType(pObj,MEMOBJ_HASHMAP);` |
|      345 |  635 | `		pObj->x.pOther = pMap;` |
|      170 |  636 | `	}` |
|      537 |  637 | `	return SXRET_OK;` |
|      271 |  638 | `}` |
|        - |  639 | `/* Per-entry callback for the array branch of the (object) cast: add one dynamic` |
|        - |  640 | ` * property to the target stdClass, named by the array key (rendered as a string,` |
|        - |  641 | ` * matching PHP) and holding a copy of the value. */` |
|        - |  642 | `struct VmObjCastData { ph7_vm *pVm; ph7_class_instance *pStd; };` |
|       38 |  643 | `static int VmArrayToObjectWalk(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  644 | `{` |
|       39 |  645 | `	struct VmObjCastData *pData = (struct VmObjCastData *)pUserData;` |
|        - |  646 | `	ph7_value *pSlot;` |
|        - |  647 | `	/* pKey and pValue are walk-owned temporaries (PH7_HashmapWalk passes pointers to` |
|        - |  648 | `	 * its own stack-local sKey/sValue, not slots inside pVm->aMemObj), so they survive` |
|        - |  649 | `	 * the slot reservation inside PH7_VmCreateDynamicAttr — no snapshot needed. pKey is` |
|        - |  650 | `	 * safe to coerce in place. */` |
|       39 |  651 | `	PH7_MemObjToString(pKey);` |
|       58 |  652 | `	pSlot = PH7_VmCreateDynamicAttr(pData->pVm,pData->pStd,` |
|       38 |  653 | `		(const char *)SyBlobData(&pKey->sBlob),(sxu32)SyBlobLength(&pKey->sBlob),0);` |
|       39 |  654 | `	if( pSlot ){` |
|       39 |  655 | `		PH7_MemObjStore(pValue,pSlot);` |
|       19 |  656 | `	}` |
|       39 |  657 | `	return SXRET_OK;` |
|        1 |  658 | `}` |
|        - |  659 | `/*` |
|        - |  660 | ` * Convert a ph7_value to type object, invalidating any prior representation.` |
|        - |  661 | ` * The new object is a (PHP-empty) stdClass populated with dynamic properties,` |
|        - |  662 | ` * matching PHP's (object) cast:` |
|        - |  663 | ` *   - array  -> one property per entry (key rendered as a string -> name).` |
|        - |  664 | ` *   - scalar -> a single property named "scalar".` |
|        - |  665 | ` *   - null   -> an empty stdClass (no properties).` |
|        - |  666 | ` *   - object -> returned unchanged (the MEMOBJ_OBJ guard below).` |
|        - |  667 | ` */` |
|       34 |  668 | `PH7_PRIVATE sxi32 PH7_MemObjToObject(ph7_value *pObj)` |
|        1 |  669 | `{` |
|       35 |  670 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  671 | `		ph7_class_instance *pStd;` |
|        - |  672 | `		ph7_class *pClass;` |
|        - |  673 | `		ph7_vm *pVm;` |
|        - |  674 | `		/* Point to the underlying VM + the stdClass */` |
|       35 |  675 | `		pVm = pObj->pVm;` |
|       52 |  676 | `		pClass = pVm->pStdClass ? pVm->pStdClass` |
|       17 |  677 | `			: PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|       35 |  678 | `		if( pClass == 0 ){` |
|        - |  679 | `			/* Can't happen,load null instead */` |
|      ! 0 |  680 | `			PH7_MemObjRelease(pObj);` |
|      ! 0 |  681 | `			return SXRET_OK;` |
|        - |  682 | `		}` |
|        - |  683 | `		/* Instanciate a new (empty) stdClass object */` |
|       35 |  684 | `		pStd = PH7_NewClassInstance(pVm,pClass);` |
|       35 |  685 | `		if( pStd == 0 ){` |
|        - |  686 | `			/* Out of memory */` |
|      ! 0 |  687 | `			PH7_MemObjRelease(pObj);` |
|      ! 0 |  688 | `			return SXRET_OK;` |
|        - |  689 | `		}` |
|       35 |  690 | `		pStd->iRef = 1;` |
|       35 |  691 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  692 | `			/* Array: one dynamic property per entry. */` |
|        - |  693 | `			struct VmObjCastData sData;` |
|       23 |  694 | `			sData.pVm = pVm;` |
|       23 |  695 | `			sData.pStd = pStd;` |
|       23 |  696 | `			ph7_array_walk(pObj,VmArrayToObjectWalk,&sData);` |
|       24 |  697 | `		}else if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  698 | `			/* Scalar (int/float/bool/string): a single "scalar" property. */` |
|       11 |  699 | `			ph7_value *pSlot = PH7_VmCreateDynamicAttr(pVm,pStd,"scalar",sizeof("scalar")-1,0);` |
|       11 |  700 | `			if( pSlot ){` |
|       11 |  701 | `				PH7_MemObjStore(pObj,pSlot);` |
|        5 |  702 | `			}` |
|        5 |  703 | `		}` |
|        - |  704 | `		/* (A NULL source yields an empty stdClass — nothing to populate.) */` |
|        - |  705 | `		/* Invalidate any prior representation */` |
|       35 |  706 | `		PH7_MemObjRelease(pObj);` |
|        - |  707 | `		/* Save the new instance */` |
|       35 |  708 | `		pObj->x.pOther = pStd;` |
|       35 |  709 | `		MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       17 |  710 | `	}` |
|       35 |  711 | `	return SXRET_OK;` |
|       18 |  712 | `}` |
|        - |  713 | `/*` |
|        - |  714 | ` * Return a pointer to the appropriate convertion method associated` |
|        - |  715 | ` * with the given type.` |
|        - |  716 | ` * Note on type juggling.` |
|        - |  717 | ` * Accoding to the PHP language reference manual` |
|        - |  718 | ` *  PHP does not require (or support) explicit type definition in variable` |
|        - |  719 | ` *  declaration; a variable's type is determined by the context in which` |
|        - |  720 | ` *  the variable is used. That is to say, if a string value is assigned` |
|        - |  721 | ` *  to variable $var, $var becomes a string. If an integer value is then` |
|        - |  722 | ` *  assigned to $var, it becomes an integer.` |
|        - |  723 | ` */` |
|       82 |  724 | `PH7_PRIVATE ProcMemObjCast PH7_MemObjCastMethod(sxi32 iFlags)` |
|        5 |  725 | `{` |
|       87 |  726 | `	if( iFlags & MEMOBJ_STRING ){` |
|       16 |  727 | `		return PH7_MemObjToString;` |
|       73 |  728 | `	}else if( iFlags & MEMOBJ_INT ){` |
|       57 |  729 | `		return PH7_MemObjToInteger;` |
|       19 |  730 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|       16 |  731 | `		return PH7_MemObjToReal;` |
|        3 |  732 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  733 | `		return PH7_MemObjToBool;` |
|        3 |  734 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        3 |  735 | `		return PH7_MemObjToHashmap;` |
|      ! 0 |  736 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  737 | `		return PH7_MemObjToObject;` |
|      ! 0 |  738 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        - |  739 | ``		/* `null` is a type, not a weak-coercion target: never silently cast a`` |
|        - |  740 | ``		 * value to null for a standalone `null` type hint. Return/property`` |
|        - |  741 | `		 * enforcement reject a non-null value before reaching here; this guards` |
|        - |  742 | `		 * the parameter default-value path from quietly nulling a non-null` |
|        - |  743 | `		 * default. */` |
|      ! 0 |  744 | `		return 0;` |
|        - |  745 | `	}` |
|        - |  746 | `	/* NULL cast */` |
|      ! 0 |  747 | `	return PH7_MemObjToNull;` |
|       46 |  748 | `}` |
|        - |  749 | `/*` |
|        - |  750 | ` * Return TRUE only if the entire string held by pValue (optionally surrounded` |
|        - |  751 | ` * by whitespace, with an optional sign) is a well-formed PHP numeric string.` |
|        - |  752 | ` * This mirrors PHP's is_numeric_string grammar used for is_numeric() and the` |
|        - |  753 | ` * loose-comparison numeric gate:` |
|        - |  754 | ` *` |
|        - |  755 | ` *   [ws] [sign] ( D+ [.D*] \| .D+ ) [ (e\|E) [sign] D+ ] [ws]   (whole string)` |
|        - |  756 | ` *` |
|        - |  757 | ` * Implemented directly rather than via SyStrIsNumeric — which returns OK on any` |
|        - |  758 | ` * numeric PREFIX (so it wrongly accepts "10abc"/"0x1A"/"0b101") and requires a` |
|        - |  759 | ` * leading digit (so it wrongly rejects ".5"/"-.5", valid in PHP). Unlike a` |
|        - |  760 | ` * strtod-based classifier this needs no NUL-terminated buffer. Returns FALSE for` |
|        - |  761 | ` * a non-string value.` |
|        - |  762 | ` */` |
|   234838 |  763 | `PH7_PRIVATE int PH7_MemObjStringIsNumeric(ph7_value *pValue)` |
|        5 |  764 | `{` |
|        - |  765 | `	const char *z, *zEnd;` |
|        - |  766 | `	sxu32 n;` |
|   234843 |  767 | `	int bDigit = 0;` |
|   234843 |  768 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  769 | `		return 0;` |
|        - |  770 | `	}` |
|   234843 |  771 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|   234843 |  772 | `	n = SyBlobLength(&pValue->sBlob);` |
|   234843 |  773 | `	if( n == 0 ){` |
|       68 |  774 | `		return 0;` |
|        - |  775 | `	}` |
|   234777 |  776 | `	zEnd = z + n;` |
|   234783 |  777 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|        8 |  778 | `		z++;` |
|        2 |  779 | `	}` |
|   234777 |  780 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|       53 |  781 | `		z++;` |
|       24 |  782 | `	}` |
|   234957 |  783 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|      183 |  784 | `		z++; bDigit = 1;` |
|        3 |  785 | `	}` |
|   234777 |  786 | `	if( z < zEnd && z[0] == '.' ){` |
|       43 |  787 | `		z++;` |
|       79 |  788 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|       38 |  789 | `			z++; bDigit = 1;` |
|        2 |  790 | `		}` |
|       19 |  791 | `	}` |
|        - |  792 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|   234777 |  793 | `	if( !bDigit ){` |
|   234631 |  794 | `		return 0;` |
|        - |  795 | `	}` |
|        - |  796 | `	/* Optional exponent — must carry at least one digit (rejects "1e", "1e+"). */` |
|      149 |  797 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|       14 |  798 | `		z++;` |
|       14 |  799 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|      ! 0 |  800 | `			z++;` |
|      ! 0 |  801 | `		}` |
|       14 |  802 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|        6 |  803 | `			return 0;` |
|        - |  804 | `		}` |
|       22 |  805 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|       14 |  806 | `			z++;` |
|        2 |  807 | `		}` |
|        4 |  808 | `	}` |
|        - |  809 | `	/* Trailing whitespace allowed; anything else means not a numeric string. */` |
|      151 |  810 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|        8 |  811 | `		z++;` |
|        2 |  812 | `	}` |
|      145 |  813 | `	return z == zEnd ? 1 : 0;` |
|   117410 |  814 | `}` |
|        - |  815 | `/*` |
|        - |  816 | ` * Check whether the ph7_value is numeric [i.e: int/float/bool] or looks` |
|        - |  817 | ` * like a numeric number [i.e: if the ph7_value is of type string.].` |
|        - |  818 | ` * Return TRUE if numeric.FALSE otherwise.` |
|        - |  819 | ` */` |
|   235534 |  820 | `PH7_PRIVATE sxi32 PH7_MemObjIsNumeric(ph7_value *pObj)` |
|        5 |  821 | `{` |
|   235539 |  822 | `	if( pObj->iFlags & ( MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      503 |  823 | `		return TRUE;` |
|   235041 |  824 | `	}else if( pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      295 |  825 | `		return FALSE;` |
|   234751 |  826 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        - |  827 | `		/* TRUE only if the whole string is a well-formed PHP numeric string. */` |
|   234751 |  828 | `		return PH7_MemObjStringIsNumeric(pObj) ? TRUE : FALSE;` |
|        - |  829 | `	}` |
|        - |  830 | `	/* NOT REACHED */` |
|      ! 0 |  831 | `	return FALSE;` |
|   117758 |  832 | `}` |
|        - |  833 | `/*` |
|        - |  834 | ` * Check whether the ph7_value is empty.Return TRUE if empty.` |
|        - |  835 | ` * FALSE otherwise.` |
|        - |  836 | ` * An ph7_value is considered empty if the following are true:` |
|        - |  837 | ` * NULL value.` |
|        - |  838 | ` * Boolean FALSE.` |
|        - |  839 | ` * Integer/Float with a 0 (zero) value.` |
|        - |  840 | ` * An empty string or a stream of 0 (zero) [i.e: "0","00","000",...].` |
|        - |  841 | ` * An empty array.` |
|        - |  842 | ` * NOTE` |
|        - |  843 | ` *  OBJECT VALUE MUST NOT BE MODIFIED.` |
|        - |  844 | ` */` |
|    34172 |  845 | `PH7_PRIVATE sxi32 PH7_MemObjIsEmpty(ph7_value *pObj)` |
|        5 |  846 | `{` |
|    34177 |  847 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|       19 |  848 | `		return TRUE;` |
|    34161 |  849 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|       22 |  850 | `		return pObj->x.iVal == 0 ? TRUE : FALSE;` |
|    34141 |  851 | `	}else if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  852 | `		return pObj->rVal == (ph7_real)0 ? TRUE : FALSE;` |
|    34141 |  853 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|        5 |  854 | `		return !pObj->x.iVal;` |
|    34137 |  855 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|    27679 |  856 | `		if( SyBlobLength(&pObj->sBlob) <= 0 ){` |
|    22281 |  857 | `			return TRUE;` |
|      ! 0 |  858 | `		}else{` |
|        - |  859 | `			const char *zIn,*zEnd;` |
|     5403 |  860 | `			zIn = (const char *)SyBlobData(&pObj->sBlob);` |
|     5403 |  861 | `			zEnd = &zIn[SyBlobLength(&pObj->sBlob)];` |
|     5409 |  862 | `			while( zIn < zEnd ){` |
|     5409 |  863 | `				if( zIn[0] != '0' ){` |
|     5403 |  864 | `					break;` |
|        - |  865 | `				}` |
|        7 |  866 | `				zIn++;` |
|        1 |  867 | `			}` |
|     5403 |  868 | `			return zIn >= zEnd ? TRUE : FALSE;` |
|      ! 0 |  869 | `		}` |
|     6463 |  870 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|     6463 |  871 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|     6463 |  872 | `		return pMap->nEntry == 0 ? TRUE : FALSE;` |
|      ! 0 |  873 | `	}else if ( pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      ! 0 |  874 | `		return FALSE;` |
|        - |  875 | `	}` |
|        - |  876 | `	/* Assume empty by default */` |
|      ! 0 |  877 | `	return TRUE;` |
|    17091 |  878 | `}` |
|        - |  879 | `/*` |
|        - |  880 | ` * Convert a ph7_value so that it has types MEMOBJ_REAL or MEMOBJ_INT` |
|        - |  881 | ` * or both.` |
|        - |  882 | ` * Invalidate any prior representations. Every effort is made to force` |
|        - |  883 | ` * the conversion, even if the input is a string that does not look` |
|        - |  884 | ` * completely like a number.Convert as much of the string as we can` |
|        - |  885 | ` * and ignore the rest.` |
|        - |  886 | ` */` |
|   465155 |  887 | `PH7_PRIVATE sxi32 PH7_MemObjToNumeric(ph7_value *pObj)` |
|        5 |  888 | `{` |
|   465160 |  889 | `	if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|   465032 |  890 | `		if( pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|        3 |  891 | `			if( pObj->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  892 | `				pObj->x.iVal = 0;` |
|      ! 0 |  893 | `			}` |
|        3 |  894 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|        1 |  895 | `		}` |
|        - |  896 | `		/* Already numeric */` |
|   465032 |  897 | `		return  SXRET_OK;` |
|        - |  898 | `	}` |
|      129 |  899 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|      129 |  900 | `		sxi32 rc = SXERR_INVALID;` |
|      129 |  901 | `		sxu8 bReal = FALSE;` |
|        - |  902 | `		SyString sString;` |
|      129 |  903 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|        - |  904 | `		/* Check if the given string looks like a numeric number */` |
|      129 |  905 | `		if( sString.nByte > 0 ){` |
|      129 |  906 | `			rc = SyStrIsNumeric(sString.zString,sString.nByte,&bReal,0);` |
|      129 |  907 | `			if( rc != SXRET_OK && !bReal ){` |
|        - |  908 | `				/* SyStrIsNumeric requires a leading digit, so it mis-classifies` |
|        - |  909 | `				 * a leading-decimal real such as ".5"/"-.5"/".5e2" (returns` |
|        - |  910 | `				 * non-OK with bReal FALSE) — PHP treats these as float. Detect` |
|        - |  911 | `				 * that shape so it coerces to real (strtod parses it) instead of` |
|        - |  912 | `				 * falling through to the int(0) "not a number" branch below. */` |
|        9 |  913 | `				const char *z = sString.zString;` |
|        9 |  914 | `				const char *zEnd = z + sString.nByte;` |
|        9 |  915 | `				while( z < zEnd && SyisSpace(z[0]) ){ z++; }` |
|        9 |  916 | `				if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){ z++; }` |
|        9 |  917 | `				if( z < zEnd && z[0] == '.' && (z + 1) < zEnd && SyisDigit(z[1]) ){` |
|        9 |  918 | `					bReal = TRUE;` |
|        4 |  919 | `				}` |
|        4 |  920 | `			}` |
|       64 |  921 | `		}` |
|      129 |  922 | `		if( bReal ){` |
|       15 |  923 | `			PH7_MemObjToReal(&(*pObj));` |
|        8 |  924 | `		}else{` |
|      115 |  925 | `			if( rc != SXRET_OK ){` |
|        - |  926 | `				/* The input does not look at all like a number,set the value to 0 */` |
|      ! 0 |  927 | `				pObj->x.iVal = 0;` |
|      ! 0 |  928 | `			}else{` |
|        - |  929 | `				/* Convert as much as we can */` |
|      115 |  930 | `				pObj->x.iVal = MemObjStringToInt(&(*pObj));` |
|        - |  931 | `			}` |
|      115 |  932 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|      115 |  933 | `			SyBlobRelease(&pObj->sBlob);` |
|        1 |  934 | `		}` |
|       64 |  935 | `	}else if(pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)){` |
|      ! 0 |  936 | `		PH7_MemObjToInteger(pObj);` |
|      ! 0 |  937 | `	}else{` |
|        - |  938 | `		/* Perform a blind cast */` |
|      ! 0 |  939 | `		PH7_MemObjToReal(&(*pObj));` |
|        - |  940 | `	}` |
|      129 |  941 | `	return SXRET_OK;` |
|   232625 |  942 | `}` |
|        - |  943 | `/*` |
|        - |  944 | ` * Apply Perl-style increment to a string ph7_value in place.` |
|        - |  945 | ` * Walks the bytes right-to-left: digits 0-8 / letters a-y, A-Y bump in` |
|        - |  946 | ` * place; '9' wraps to '0' with carry; 'z' to 'a'; 'Z' to 'A'. A non-` |
|        - |  947 | ` * alphanumeric byte stops the walk without prepending. If carry survives` |
|        - |  948 | ` * past index 0, prepend '1', 'a', or 'A' depending on the class of the` |
|        - |  949 | ` * last carried character. Empty strings become "1".` |
|        - |  950 | ` *` |
|        - |  951 | ` * Caller must ensure pObj is MEMOBJ_STRING and not a numeric string;` |
|        - |  952 | ` * this routine never reclassifies the type, so a result like "e0" stays` |
|        - |  953 | ` * a string even though it looks numeric.` |
|        - |  954 | ` */` |
|       48 |  955 | `PH7_PRIVATE sxi32 PH7_MemObjStringIncrement(ph7_value *pObj)` |
|        1 |  956 | `{` |
|        - |  957 | `	enum CarryClass { CARRY_NONE = 0, CARRY_LOWER, CARRY_UPPER, CARRY_DIGIT };` |
|       49 |  958 | `	enum CarryClass last_class = CARRY_NONE;` |
|        - |  959 | `	sxu32 nLen, pos;` |
|        - |  960 | `	sxu8 *zStr;` |
|       49 |  961 | `	int carry = 1;` |
|        - |  962 | `	int ch;` |
|        - |  963 | `	/* Force ownership: the blob may be SXBLOB_RDONLY (e.g., from` |
|        - |  964 | `	 * PH7_MemObjLoad), in which case BlobPrepareGrow copies on demand` |
|        - |  965 | `	 * and clears the flag.  On an already-owned blob with spare capacity` |
|        - |  966 | `	 * (the common case under PHL's growth allocator), this is a no-op` |
|        - |  967 | `	 * append; on an exact-fit owned blob it triggers a single realloc. */` |
|       49 |  968 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       45 |  969 | `		SyBlobNullAppend(&pObj->sBlob);` |
|       22 |  970 | `	}` |
|       49 |  971 | `	nLen = SyBlobLength(&pObj->sBlob);` |
|       49 |  972 | `	if( nLen == 0 ){` |
|        5 |  973 | `		SyBlobAppend(&pObj->sBlob,"1",sizeof(char));` |
|        5 |  974 | `		return SXRET_OK;` |
|        - |  975 | `	}` |
|       45 |  976 | `	zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       45 |  977 | `	pos = nLen;` |
|       97 |  978 | `	while( pos > 0 ){` |
|       79 |  979 | `		pos--;` |
|       79 |  980 | `		ch = zStr[pos];` |
|       79 |  981 | `		if( ch >= 'a' && ch <= 'z' ){` |
|       45 |  982 | `			if( ch == 'z' ){` |
|       29 |  983 | `				zStr[pos] = 'a';` |
|       29 |  984 | `				last_class = CARRY_LOWER;` |
|       29 |  985 | `				continue;` |
|        - |  986 | `			}` |
|       17 |  987 | `			zStr[pos]++;` |
|       17 |  988 | `			carry = 0;` |
|       17 |  989 | `			break;` |
|       35 |  990 | `		}else if( ch >= 'A' && ch <= 'Z' ){` |
|       21 |  991 | `			if( ch == 'Z' ){` |
|       19 |  992 | `				zStr[pos] = 'A';` |
|       19 |  993 | `				last_class = CARRY_UPPER;` |
|       19 |  994 | `				continue;` |
|        - |  995 | `			}` |
|        3 |  996 | `			zStr[pos]++;` |
|        3 |  997 | `			carry = 0;` |
|        3 |  998 | `			break;` |
|       15 |  999 | `		}else if( ch >= '0' && ch <= '9' ){` |
|        7 | 1000 | `			if( ch == '9' ){` |
|        7 | 1001 | `				zStr[pos] = '0';` |
|        7 | 1002 | `				last_class = CARRY_DIGIT;` |
|        7 | 1003 | `				continue;` |
|        - | 1004 | `			}` |
|      ! 0 | 1005 | `			zStr[pos]++;` |
|      ! 0 | 1006 | `			carry = 0;` |
|      ! 0 | 1007 | `			break;` |
|      ! 0 | 1008 | `		}else{` |
|        - | 1009 | `			/* non-alphanumeric: stop without prepending */` |
|        9 | 1010 | `			carry = 0;` |
|        9 | 1011 | `			break;` |
|        - | 1012 | `		}` |
|      ! 0 | 1013 | `	}` |
|       45 | 1014 | `	if( carry ){` |
|        - | 1015 | `		sxu8 prepend;` |
|        - | 1016 | `		sxu32 i;` |
|       19 | 1017 | `		switch( last_class ){` |
|        9 | 1018 | `			case CARRY_LOWER: prepend = (sxu8)'a'; break;` |
|       11 | 1019 | `			case CARRY_UPPER: prepend = (sxu8)'A'; break;` |
|      ! 0 | 1020 | `			default:          prepend = (sxu8)'1'; break;` |
|        - | 1021 | `		}` |
|        - | 1022 | `		/* Append a sentinel byte to grow nByte by 1 (capacity grows too). */` |
|       19 | 1023 | `		SyBlobAppend(&pObj->sBlob,"\0",sizeof(char));` |
|       19 | 1024 | `		zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       19 | 1025 | `		nLen = SyBlobLength(&pObj->sBlob);` |
|        - | 1026 | `		/* Shift right by 1, walking from the end so overlapping is safe. */` |
|       57 | 1027 | `		for( i = nLen - 1; i > 0; i-- ){` |
|       39 | 1028 | `			zStr[i] = zStr[i - 1];` |
|       20 | 1029 | `		}` |
|       19 | 1030 | `		zStr[0] = prepend;` |
|        9 | 1031 | `	}` |
|       45 | 1032 | `	return SXRET_OK;` |
|       25 | 1033 | `}` |
|        - | 1034 | `/*` |
|        - | 1035 | ` * Try a get an integer representation of the given ph7_value.` |
|        - | 1036 | ` * If the ph7_value is not of type real,this function is a no-op.` |
|        - | 1037 | ` */` |
|     1120 | 1038 | `PH7_PRIVATE sxi32 PH7_MemObjTryInteger(ph7_value *pObj)` |
|        1 | 1039 | `{` |
|     1121 | 1040 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - | 1041 | `		/* Work only with reals */` |
|     1121 | 1042 | `		MemObjTryIntger(&(*pObj));` |
|      560 | 1043 | `	}` |
|     1121 | 1044 | `	return SXRET_OK;` |
|        1 | 1045 | `}` |
|        - | 1046 | `/*` |
|        - | 1047 | ` * Initialize a ph7_value to the null type.` |
|        - | 1048 | ` */` |
| 10715251 | 1049 | `PH7_PRIVATE sxi32 PH7_MemObjInit(ph7_vm *pVm,ph7_value *pObj)` |
|        5 | 1050 | `{` |
|        - | 1051 | `	/* Zero the structure */` |
| 10715256 | 1052 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1053 | `	/* Initialize fields */` |
| 10715256 | 1054 | `	pObj->pVm = pVm;` |
| 10715256 | 1055 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1056 | `	/* Set the NULL type */` |
| 10715256 | 1057 | `	pObj->iFlags = MEMOBJ_NULL;` |
| 10715256 | 1058 | `	return SXRET_OK;` |
|        5 | 1059 | `}` |
|        - | 1060 | `/*` |
|        - | 1061 | ` * Initialize a ph7_value to the integer type.` |
|        - | 1062 | ` */` |
|  3438118 | 1063 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromInt(ph7_vm *pVm,ph7_value *pObj,sxi64 iVal)` |
|        5 | 1064 | `{` |
|        - | 1065 | `	/* Zero the structure */` |
|  3438123 | 1066 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1067 | `	/* Initialize fields */` |
|  3438123 | 1068 | `	pObj->pVm = pVm;` |
|  3438123 | 1069 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1070 | `	/* Set the desired type */` |
|  3438123 | 1071 | `	pObj->x.iVal = iVal;` |
|  3438123 | 1072 | `	pObj->iFlags = MEMOBJ_INT;` |
|  3438123 | 1073 | `	return SXRET_OK;` |
|        5 | 1074 | `}` |
|        - | 1075 | `/*` |
|        - | 1076 | ` * Initialize a ph7_value to the boolean type.` |
|        - | 1077 | ` */` |
|    17488 | 1078 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromBool(ph7_vm *pVm,ph7_value *pObj,sxi32 iVal)` |
|        5 | 1079 | `{` |
|        - | 1080 | `	/* Zero the structure */` |
|    17493 | 1081 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1082 | `	/* Initialize fields */` |
|    17493 | 1083 | `	pObj->pVm = pVm;` |
|    17493 | 1084 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1085 | `	/* Set the desired type */` |
|    17493 | 1086 | `	pObj->x.iVal = iVal ? 1 : 0;` |
|    17493 | 1087 | `	pObj->iFlags = MEMOBJ_BOOL;` |
|    17493 | 1088 | `	return SXRET_OK;` |
|        5 | 1089 | `}` |
|        - | 1090 | `/*` |
|        - | 1091 | ` * Initialize a ph7_value to the real type.` |
|        - | 1092 | ` */` |
|       10 | 1093 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromReal(ph7_vm *pVm,ph7_value *pObj,ph7_real rVal)` |
|        1 | 1094 | `{` |
|        - | 1095 | `	/* Zero the structure */` |
|       11 | 1096 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1097 | `	/* Initialize fields */` |
|       11 | 1098 | `	pObj->pVm = pVm;` |
|       11 | 1099 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1100 | `	/* Set the desired type */` |
|       11 | 1101 | `	pObj->rVal = rVal;` |
|       11 | 1102 | `	pObj->iFlags = MEMOBJ_REAL;` |
|       11 | 1103 | `	return SXRET_OK;` |
|        1 | 1104 | `}` |
|        - | 1105 | `/*` |
|        - | 1106 | ` * Initialize a ph7_value to the array type.` |
|        - | 1107 | ` */` |
|    73278 | 1108 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromArray(ph7_vm *pVm,ph7_value *pObj,ph7_hashmap *pArray)` |
|        5 | 1109 | `{` |
|        - | 1110 | `	/* Zero the structure */` |
|    73283 | 1111 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1112 | `	/* Initialize fields */` |
|    73283 | 1113 | `	pObj->pVm = pVm;` |
|    73283 | 1114 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1115 | `	/* Set the desired type */` |
|    73283 | 1116 | `	pObj->iFlags = MEMOBJ_HASHMAP;` |
|    73283 | 1117 | `	pObj->x.pOther = pArray;` |
|    73283 | 1118 | `	return SXRET_OK;` |
|        5 | 1119 | `}` |
|        - | 1120 | `/*` |
|        - | 1121 | ` * Initialize a ph7_value to the string type.` |
|        - | 1122 | ` */` |
|  2193408 | 1123 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromString(ph7_vm *pVm,ph7_value *pObj,const SyString *pVal)` |
|        5 | 1124 | `{` |
|        - | 1125 | `	/* Zero the structure */` |
|  2193413 | 1126 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1127 | `	/* Initialize fields */` |
|  2193413 | 1128 | `	pObj->pVm = pVm;` |
|  2193413 | 1129 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|  2193413 | 1130 | `	if( pVal ){` |
|        - | 1131 | `		/* Append contents */` |
|   932859 | 1132 | `		SyBlobAppend(&pObj->sBlob,(const void *)pVal->zString,pVal->nByte);` |
|   466427 | 1133 | `	}` |
|        - | 1134 | `	/* Set the desired type */` |
|  2193413 | 1135 | `	pObj->iFlags = MEMOBJ_STRING;` |
|  2193413 | 1136 | `	return SXRET_OK;` |
|        5 | 1137 | `}` |
|        - | 1138 | `/*` |
|        - | 1139 | ` * Append some contents to the internal buffer of a given ph7_value.` |
|        - | 1140 | ` * If the given ph7_value is not of type string,this function` |
|        - | 1141 | ` * invalidate any prior representation and set the string type.` |
|        - | 1142 | ` * Then a simple append operation is performed.` |
|        - | 1143 | ` */` |
|  1453080 | 1144 | `PH7_PRIVATE sxi32 PH7_MemObjStringAppend(ph7_value *pObj,const char *zData,sxu32 nLen)` |
|        5 | 1145 | `{` |
|        - | 1146 | `	sxi32 rc;` |
|  1453085 | 1147 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 1148 | `		/* Invalidate any prior representation */` |
|     2539 | 1149 | `		PH7_MemObjRelease(pObj);` |
|     2539 | 1150 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|     1267 | 1151 | `	}` |
|        - | 1152 | `	/* Append contents */` |
|  1453085 | 1153 | `	rc = SyBlobAppend(&pObj->sBlob,zData,nLen);` |
|  1453085 | 1154 | `	return rc;` |
|        5 | 1155 | `}` |
|        - | 1156 | `#if 0` |
|        - | 1157 | `/*` |
|        - | 1158 | ` * Format and append some contents to the internal buffer of a given ph7_value.` |
|        - | 1159 | ` * If the given ph7_value is not of type string,this function invalidate` |
|        - | 1160 | ` * any prior representation and set the string type.` |
|        - | 1161 | ` * Then a simple format and append operation is performed.` |
|        - | 1162 | ` */` |
|        - | 1163 | `PH7_PRIVATE sxi32 PH7_MemObjStringFormat(ph7_value *pObj,const char *zFormat,va_list ap)` |
|        - | 1164 | `{` |
|        - | 1165 | `	sxi32 rc;` |
|        - | 1166 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 1167 | `		/* Invalidate any prior representation */` |
|        - | 1168 | `		PH7_MemObjRelease(pObj);` |
|        - | 1169 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|        - | 1170 | `	}` |
|        - | 1171 | `	/* Format and append contents */` |
|        - | 1172 | `	rc = SyBlobFormatAp(&pObj->sBlob,zFormat,ap);` |
|        - | 1173 | `	return rc;` |
|        - | 1174 | `}` |
|        - | 1175 | `#endif` |
|        - | 1176 | `/*` |
|        - | 1177 | ` * Duplicate the contents of a ph7_value.` |
|        - | 1178 | ` */` |
|  5025039 | 1179 | `PH7_PRIVATE sxi32 PH7_MemObjStore(ph7_value *pSrc,ph7_value *pDest)` |
|        5 | 1180 | `{` |
|  5025044 | 1181 | `	ph7_class_instance *pObj = 0;` |
|  5025044 | 1182 | `	ph7_hashmap *pMap = 0;` |
|        - | 1183 | `	sxi32 rc;` |
|  5025044 | 1184 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1185 | `		/* Increment reference count */` |
|   211901 | 1186 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  4919096 | 1187 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|        - | 1188 | `		/* Increment reference count */` |
|     5391 | 1189 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|     2693 | 1190 | `	}` |
|  5025044 | 1191 | `	if( pDest->iFlags & MEMOBJ_HASHMAP ){` |
|    62099 | 1192 | `		pMap = (ph7_hashmap *)pDest->x.pOther;` |
|  4993997 | 1193 | `	}else if( pDest->iFlags & MEMOBJ_OBJ ){` |
|     8051 | 1194 | `		pObj = (ph7_class_instance *)pDest->x.pOther;` |
|     4023 | 1195 | `	}` |
|  5025044 | 1196 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  5025044 | 1197 | `	pDest->iFlags &= ~MEMOBJ_AUX;` |
|  5025044 | 1198 | `	rc = SXRET_OK;` |
|  5025044 | 1199 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  4029403 | 1200 | `		SyBlobReset(&pDest->sBlob);` |
|  4029403 | 1201 | `		rc = SyBlobDup(&pSrc->sBlob,&pDest->sBlob);` |
|  2014704 | 1202 | `	}else{` |
|   995646 | 1203 | `		if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|   289302 | 1204 | `			SyBlobRelease(&pDest->sBlob);` |
|   144691 | 1205 | `		}` |
|        - | 1206 | `	}` |
|  5025044 | 1207 | `	if( pMap ){` |
|    62099 | 1208 | `		PH7_HashmapUnref(pMap);` |
|  4993997 | 1209 | `	}else if( pObj ){` |
|     8051 | 1210 | `		PH7_ClassInstanceUnref(pObj);` |
|     4023 | 1211 | `	}` |
|  5025039 | 1212 | `	if( rc == SXRET_OK && (pDest->iFlags & MEMOBJ_HASHMAP)` |
|  2618510 | 1213 | `	 && pDest->pVm` |
|   211896 | 1214 | `	 && (ph7_hashmap *)pDest->x.pOther == pDest->pVm->pGlobal` |
|        - | 1215 | `	 /* Identity, not nIdx: transient values carry nIdx==0 (SyZero), which` |
|        - | 1216 | `	  * collides with a typical nGlobalIdx of 0 and would skip the snapshot` |
|        - | 1217 | `	  * for closure envs and other non-slot destinations. */` |
|   105957 | 1218 | `	 && pDest != (ph7_value *)SySetAt(&pDest->pVm->aMemObj,pDest->pVm->nGlobalIdx) ){` |
|        - | 1219 | `		/* php 8.1: a COPY of $GLOBALS ($snap = $GLOBALS, $a[] = $GLOBALS,` |
|        - | 1220 | `		 * by-value argument passing, return $GLOBALS, ...) is a by-value` |
|        - | 1221 | `		 * SNAPSHOT of the symbol table with its reference entries` |
|        - | 1222 | `		 * flattened — never a live alias. Materialize it here, the one` |
|        - | 1223 | `		 * store choke point (loads/subscript access keep sharing, so` |
|        - | 1224 | `		 * $GLOBALS[$k] reads and writes stay live). */` |
|        9 | 1225 | `		ph7_hashmap *pSnap = PH7_NewHashmap(pDest->pVm,0,0);` |
|        9 | 1226 | `		if( pSnap && PH7_HashmapDupMaterialized((ph7_hashmap *)pDest->x.pOther,pSnap) == SXRET_OK ){` |
|        9 | 1227 | `			PH7_HashmapUnref((ph7_hashmap *)pDest->x.pOther);` |
|        9 | 1228 | `			pDest->x.pOther = pSnap;` |
|        4 | 1229 | `		}else if( pSnap ){` |
|      ! 0 | 1230 | `			PH7_HashmapUnref(pSnap);` |
|      ! 0 | 1231 | `		}` |
|        4 | 1232 | `	}` |
|  5025044 | 1233 | `	return rc;` |
|        5 | 1234 | `}` |
|        - | 1235 | `/*` |
|        - | 1236 | ` * Duplicate the contents of a ph7_value but do not copy internal` |
|        - | 1237 | ` * buffer contents,simply point to it.` |
|        - | 1238 | ` */` |
|  6978040 | 1239 | `PH7_PRIVATE sxi32 PH7_MemObjLoad(ph7_value *pSrc,ph7_value *pDest)` |
|        5 | 1240 | `{` |
|  6978045 | 1241 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),` |
|        - | 1242 | `		sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  6978045 | 1243 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1244 | `		/* Increment reference count */` |
|   490175 | 1245 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  6732960 | 1246 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|        - | 1247 | `		/* Increment reference count */` |
|    36455 | 1248 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|    18225 | 1249 | `	}` |
|  6978045 | 1250 | `	if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|       48 | 1251 | `		SyBlobRelease(&pDest->sBlob);` |
|       22 | 1252 | `	}` |
|  6978045 | 1253 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  3764437 | 1254 | `		SyBlobReadOnly(&pDest->sBlob,SyBlobData(&pSrc->sBlob),SyBlobLength(&pSrc->sBlob));` |
|  1882273 | 1255 | `	}` |
|  6978045 | 1256 | `	return SXRET_OK;` |
|        5 | 1257 | `}` |
|        - | 1258 | `/*` |
|        - | 1259 | ` * Invalidate any prior representation of a given ph7_value.` |
|        - | 1260 | ` */` |
| 17072075 | 1261 | `PH7_PRIVATE sxi32 PH7_MemObjRelease(ph7_value *pObj)` |
|        5 | 1262 | `{` |
| 17072080 | 1263 | `	if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
| 14861021 | 1264 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|   678777 | 1265 | `			PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
| 14521635 | 1266 | `		}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|    62261 | 1267 | `			PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|    31128 | 1268 | `		}` |
|        - | 1269 | `		/* Release the internal buffer */` |
| 14861021 | 1270 | `		SyBlobRelease(&pObj->sBlob);` |
|        - | 1271 | `		/* Invalidate any prior representation */` |
| 14861021 | 1272 | `		pObj->iFlags = MEMOBJ_NULL;` |
|  7430820 | 1273 | `	}` |
| 17072080 | 1274 | `	return SXRET_OK;` |
|        5 | 1275 | `}` |
|        - | 1276 | `/*` |
|        - | 1277 | ` * Compare two ph7_values.` |
|        - | 1278 | ` * Return 0 if the values are equals, > 0 if pObj1 is greater than pObj2` |
|        - | 1279 | ` * or < 0 if pObj2 is greater than pObj1.` |
|        - | 1280 | ` * Type comparison table taken from the PHP language reference manual.` |
|        - | 1281 | ` * Comparisons of $x with PHP functions Expression` |
|        - | 1282 | ` *              gettype() 	empty() 	is_null() 	isset() 	boolean : if($x)` |
|        - | 1283 | ` * $x = ""; 	string 	    TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1284 | ` * $x = null 	NULL 	    TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1285 | ` * var $x; 	    NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1286 | ` * $x is undefined 	NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1287 | ` *  $x = array(); 	array 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1288 | ` * $x = false; 	boolean 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1289 | ` * $x = true; 	boolean 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1290 | ` * $x = 1; 	    integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1291 | ` * $x = 42; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1292 | ` * $x = 0; 	    integer 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1293 | ` * $x = -1; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1294 | ` * $x = "1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1295 | ` * $x = "0"; 	string 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1296 | ` * $x = "-1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1297 | ` * $x = "php"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1298 | ` * $x = "true"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1299 | ` * $x = "false"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1300 | ` *      Loose comparisons with ==` |
|        - | 1301 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|        - | 1302 | ` * TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1303 | ` * FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|        - | 1304 | ` * 1 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1305 | ` * 0 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE` |
|        - | 1306 | ` * -1 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1307 | ` * "1" 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1308 | ` * "0" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1309 | ` * "-1" 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1310 | ` * NULL 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|        - | 1311 | ` * array() 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1312 | ` * "php" 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1313 | ` * "" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE` |
|        - | 1314 | ` *    Strict comparisons with ===` |
|        - | 1315 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|        - | 1316 | ` * TRUE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1317 | ` * FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1318 | ` * 1 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1319 | ` * 0 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1320 | ` * -1 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1321 | ` * "1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1322 | ` * "0" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1323 | ` * "-1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1324 | ` * NULL 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE` |
|        - | 1325 | ` * array() 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE` |
|        - | 1326 | ` * "php" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1327 | ` * "" 	    FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE` |
|        - | 1328 | ` */` |
|  1321599 | 1329 | `PH7_PRIVATE sxi32 PH7_MemObjCmp(ph7_value *pObj1,ph7_value *pObj2,int bStrict,int iNest)` |
|        5 | 1330 | `{` |
|        - | 1331 | `	sxi32 iComb;` |
|        - | 1332 | `	sxi32 rc;` |
|  1321604 | 1333 | `	if( bStrict ){` |
|        - | 1334 | `		sxi32 iF1,iF2;` |
|        - | 1335 | `		/* Strict comparisons with === */` |
|   692319 | 1336 | `		iF1 = pObj1->iFlags&~MEMOBJ_AUX;` |
|   692319 | 1337 | `		iF2 = pObj2->iFlags&~MEMOBJ_AUX;` |
|   692319 | 1338 | `		if( iF1 != iF2 ){` |
|        - | 1339 | `			/* Not of the same type */` |
|   194133 | 1340 | `			return 1;` |
|        - | 1341 | `		}` |
|   249093 | 1342 | `	}` |
|        - | 1343 | `	/* Combine flag together */` |
|  1127476 | 1344 | `	iComb = pObj1->iFlags\|pObj2->iFlags;` |
|  1127471 | 1345 | `	if( !bStrict` |
|   878378 | 1346 | `	 && (iComb & MEMOBJ_NULL) != 0` |
|   314709 | 1347 | `	 && (iComb & MEMOBJ_STRING) != 0` |
|       65 | 1348 | `	 && (iComb & (MEMOBJ_BOOL\|MEMOBJ_RES\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|        - | 1349 | `		/*` |
|        - | 1350 | `		 * PHP 8 comparison table: null loosely compared with a STRING is` |
|        - | 1351 | `		 * compared as the empty string (a string comparison), not through` |
|        - | 1352 | `		 * bool coercion — so null == "0" is FALSE and null < "0" is TRUE` |
|        - | 1353 | `		 * (php 7 and the historical PH7 behavior coerced both to bool,` |
|        - | 1354 | `		 * making any non-empty non-"0"-insensitive string "equal" to null).` |
|        - | 1355 | `		 * Convert the null side to "" and let the string branch below run.` |
|        - | 1356 | `		 */` |
|       45 | 1357 | `		if( pObj1->iFlags & MEMOBJ_NULL ){` |
|       35 | 1358 | `			PH7_MemObjToString(pObj1);` |
|       18 | 1359 | `		}else{` |
|       11 | 1360 | `			PH7_MemObjToString(pObj2);` |
|        - | 1361 | `		}` |
|       45 | 1362 | `		iComb = pObj1->iFlags\|pObj2->iFlags;` |
|       22 | 1363 | `	}` |
|  1127476 | 1364 | `	if( iComb & (MEMOBJ_NULL\|MEMOBJ_RES\|MEMOBJ_BOOL) ){` |
|        - | 1365 | `		/* Convert to boolean: Keep in mind FALSE < TRUE */` |
|    22867 | 1366 | `		if( (pObj1->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     8379 | 1367 | `			PH7_MemObjToBool(pObj1);` |
|     4187 | 1368 | `		}` |
|    22867 | 1369 | `		if( (pObj2->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     7399 | 1370 | `			PH7_MemObjToBool(pObj2);` |
|     3697 | 1371 | `		}` |
|    22867 | 1372 | `		return (sxi32)((pObj1->x.iVal != 0) - (pObj2->x.iVal != 0));` |
|  1104614 | 1373 | `	}else if ( iComb & MEMOBJ_HASHMAP ){` |
|        - | 1374 | `		/* Hashmap aka 'array' comparison */` |
|       31 | 1375 | `		if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1376 | `			/* Array is always greater */` |
|      ! 0 | 1377 | `			return -1;` |
|        - | 1378 | `		}` |
|       31 | 1379 | `		if( (pObj2->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1380 | `			/* Array is always greater */` |
|      ! 0 | 1381 | `			return 1;` |
|        - | 1382 | `		}` |
|        - | 1383 | `		/* Perform the comparison */` |
|       31 | 1384 | `		rc = PH7_HashmapCmp((ph7_hashmap *)pObj1->x.pOther,(ph7_hashmap *)pObj2->x.pOther,bStrict);` |
|       31 | 1385 | `		return rc;` |
|  1104584 | 1386 | `	}else if(iComb & MEMOBJ_OBJ ){` |
|        - | 1387 | `		/* Object comparison */` |
|      257 | 1388 | `		if( (pObj1->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1389 | `			/* Object is always greater */` |
|      ! 0 | 1390 | `			return -1;` |
|        - | 1391 | `		}` |
|      257 | 1392 | `		if( (pObj2->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1393 | `			/* Object is always greater */` |
|      ! 0 | 1394 | `			return 1;` |
|        - | 1395 | `		}` |
|        - | 1396 | `		/* Perform the comparison */` |
|      257 | 1397 | `		rc = PH7_ClassInstanceCmp((ph7_class_instance *)pObj1->x.pOther,(ph7_class_instance *)pObj2->x.pOther,bStrict,iNest);` |
|      257 | 1398 | `		return rc;` |
|  1104332 | 1399 | `	}else if ( iComb & MEMOBJ_STRING ){` |
|        - | 1400 | `		SyString s1,s2;` |
|   708699 | 1401 | `		if( !bStrict ){` |
|        - | 1402 | `			/*` |
|        - | 1403 | `			 * PHP 8 "saner string to number comparisons" (RFC): a numeric` |
|        - | 1404 | `			 * comparison is performed only when BOTH operands are numbers or` |
|        - | 1405 | `			 * numeric strings. A number compared with a NON-numeric string is` |
|        - | 1406 | `			 * compared as strings, with the number cast to its string form —` |
|        - | 1407 | `			 * so 0 == "abc" is false, "abc" < 10 is false, and max("abc",10)` |
|        - | 1408 | `			 * is "abc". (PHP 7 cast the non-numeric string to 0 and compared` |
|        - | 1409 | `			 * numerically; comparing when EITHER side was numeric is what this` |
|        - | 1410 | `			 * replaces.) Two non-numeric strings, or one numeric and one` |
|        - | 1411 | `			 * non-numeric string, still fall through to the string comparison` |
|        - | 1412 | `			 * below, unchanged.` |
|        - | 1413 | `			 */` |
|   234431 | 1414 | `			if( PH7_MemObjIsNumeric(pObj1) && PH7_MemObjIsNumeric(pObj2) ){` |
|        - | 1415 | `				/* Perform a numeric comparison */` |
|       29 | 1416 | `				goto Numeric;` |
|        - | 1417 | `			}` |
|   117185 | 1418 | `		}` |
|        - | 1419 | `		/* Perform a strict string comparison.*/` |
|   708671 | 1420 | `		if( (pObj1->iFlags&MEMOBJ_STRING) == 0 ){` |
|       19 | 1421 | `			PH7_MemObjToString(pObj1);` |
|        9 | 1422 | `		}` |
|   708671 | 1423 | `		if( (pObj2->iFlags&MEMOBJ_STRING) == 0 ){` |
|        5 | 1424 | `			PH7_MemObjToString(pObj2);` |
|        2 | 1425 | `		}` |
|   708671 | 1426 | `		SyStringInitFromBuf(&s1,SyBlobData(&pObj1->sBlob),SyBlobLength(&pObj1->sBlob));` |
|   708671 | 1427 | `		SyStringInitFromBuf(&s2,SyBlobData(&pObj2->sBlob),SyBlobLength(&pObj2->sBlob));` |
|        - | 1428 | `		/*` |
|        - | 1429 | `		 * Strings are compared using memcmp(). If one value is an exact prefix of the` |
|        - | 1430 | `		 * other, then the shorter value is less than the longer value.` |
|        - | 1431 | `		 */` |
|   708671 | 1432 | `		rc = SyMemcmp((const void *)s1.zString,(const void *)s2.zString,SXMIN(s1.nByte,s2.nByte));` |
|   708671 | 1433 | `		if( rc == 0 ){` |
|   236875 | 1434 | `			if( s1.nByte != s2.nByte ){` |
|     2199 | 1435 | `				rc = s1.nByte < s2.nByte ? -1 : 1;` |
|     1098 | 1436 | `			}` |
|   118436 | 1437 | `		}` |
|   708671 | 1438 | `		return rc;` |
|   395638 | 1439 | `	}else if( iComb & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|   197774 | 1440 | `Numeric:` |
|        - | 1441 | `		/* Perform a numeric comparison if one of the operand is numeric(integer or real) */` |
|   395666 | 1442 | `		if( (pObj1->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|       11 | 1443 | `			PH7_MemObjToNumeric(pObj1);` |
|        5 | 1444 | `		}` |
|   395666 | 1445 | `		if( (pObj2->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|       19 | 1446 | `			PH7_MemObjToNumeric(pObj2);` |
|        9 | 1447 | `		}` |
|   395666 | 1448 | `		if( (pObj1->iFlags & pObj2->iFlags & MEMOBJ_INT) == 0) {` |
|        - | 1449 | `			/*` |
|        - | 1450 | `			 * Symisc eXtension to the PHP language:` |
|        - | 1451 | `			 *  Floating point comparison is introduced and works as expected.` |
|        - | 1452 | `			 */` |
|        - | 1453 | `			ph7_real r1,r2;` |
|        - | 1454 | `			/* Compare as reals */` |
|      273 | 1455 | `			if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       11 | 1456 | `				PH7_MemObjToReal(pObj1);` |
|        5 | 1457 | `			}` |
|      273 | 1458 | `			r1 = pObj1->rVal;` |
|      273 | 1459 | `			if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|       51 | 1460 | `				PH7_MemObjToReal(pObj2);` |
|       25 | 1461 | `			}` |
|      273 | 1462 | `			r2 = pObj2->rVal;` |
|      273 | 1463 | `			if( PH7_IS_NAN(r1) \|\| PH7_IS_NAN(r2) ){` |
|        - | 1464 | `				/*` |
|        - | 1465 | `				 * Keep a strict three-way comparator contract even for NaN values.` |
|        - | 1466 | `				 * For ordering purposes, NaN compares equal to NaN and greater than` |
|        - | 1467 | `				 * any non-NaN numeric value.` |
|        - | 1468 | `				 */` |
|       45 | 1469 | `				if( PH7_IS_NAN(r1) ){` |
|       35 | 1470 | `					return PH7_IS_NAN(r2) ? 0 : 1;` |
|        - | 1471 | `				}` |
|       11 | 1472 | `				return -1;` |
|        - | 1473 | `			}` |
|      229 | 1474 | `			if( r1 > r2 ){` |
|       45 | 1475 | `				return 1;` |
|      185 | 1476 | `			}else if( r1 < r2 ){` |
|      125 | 1477 | `				return -1;` |
|        - | 1478 | `			}` |
|       61 | 1479 | `			return 0;` |
|      ! 0 | 1480 | `		}else{` |
|        - | 1481 | `			/* Integer comparison */` |
|   395394 | 1482 | `			if( pObj1->x.iVal > pObj2->x.iVal ){` |
|     5960 | 1483 | `				return 1;` |
|   389439 | 1484 | `			}else if( pObj1->x.iVal < pObj2->x.iVal ){` |
|   381927 | 1485 | `				return -1;` |
|        - | 1486 | `			}` |
|     7517 | 1487 | `			return 0;` |
|        - | 1488 | `		}` |
|        - | 1489 | `	}` |
|        - | 1490 | `	/* NOT REACHED */` |
|      ! 0 | 1491 | `	return 0;` |
|   660833 | 1492 | `}` |
|        - | 1493 | `/*` |
|        - | 1494 | ` * Perform an addition operation of two ph7_values.` |
|        - | 1495 | ` * The reason this function is implemented here rather than 'vm.c'` |
|        - | 1496 | ` * is that the '+' operator is overloaded.` |
|        - | 1497 | ` * That is,the '+' operator is used for arithmetic operation and also` |
|        - | 1498 | ` * used for operation on arrays [i.e: union]. When used with an array` |
|        - | 1499 | ` * The + operator returns the right-hand array appended to the left-hand array.` |
|        - | 1500 | ` * For keys that exist in both arrays, the elements from the left-hand array` |
|        - | 1501 | ` * will be used, and the matching elements from the right-hand array will` |
|        - | 1502 | ` * be ignored.` |
|        - | 1503 | ` * This function take care of handling all the scenarios.` |
|        - | 1504 | ` */` |
|    10726 | 1505 | `PH7_PRIVATE sxi32 PH7_MemObjAdd(ph7_value *pObj1,ph7_value *pObj2,int bAddStore)` |
|        5 | 1506 | `{` |
|    10731 | 1507 | `	if( ((pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1508 | `			/* Arithemtic operation */` |
|     6877 | 1509 | `			PH7_MemObjToNumeric(pObj1);` |
|     6877 | 1510 | `			PH7_MemObjToNumeric(pObj2);` |
|     6877 | 1511 | `			if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_REAL ){` |
|        - | 1512 | `				/* Floating point arithmetic */` |
|        - | 1513 | `				ph7_real a,b;` |
|       67 | 1514 | `				if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       31 | 1515 | `					PH7_MemObjToReal(pObj1);` |
|       15 | 1516 | `				}` |
|       67 | 1517 | `				if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 | 1518 | `					PH7_MemObjToReal(pObj2);` |
|        2 | 1519 | `				}` |
|       67 | 1520 | `				a = pObj1->rVal;` |
|       67 | 1521 | `				b = pObj2->rVal;` |
|       67 | 1522 | `				pObj1->rVal = a+b;` |
|       67 | 1523 | `				MemObjSetType(pObj1,MEMOBJ_REAL);` |
|        - | 1524 | `				/* Try to get an integer representation also */` |
|       67 | 1525 | `				MemObjTryIntger(&(*pObj1));` |
|       34 | 1526 | `			}else{` |
|        - | 1527 | `				/* Integer arithmetic; PHP promotes an overflowing sum to float.` |
|        - | 1528 | `				 * The integer-only build (PH7_OMIT_FLOATING_POINT) has no float` |
|        - | 1529 | `				 * type, so it wraps like OP_POW's OMIT path. */` |
|        - | 1530 | `				sxi64 a,b,r;` |
|     6811 | 1531 | `				a = pObj1->x.iVal;` |
|     6811 | 1532 | `				b = pObj2->x.iVal;` |
|     6811 | 1533 | `				if( PH7_ADD_OVERFLOW64(a,b,&r) ){` |
|        - | 1534 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        9 | 1535 | `					pObj1->rVal = (ph7_real)a + (ph7_real)b;` |
|        9 | 1536 | `					MemObjSetType(pObj1,MEMOBJ_REAL);` |
|        - | 1537 | `#else` |
|        - | 1538 | `					pObj1->x.iVal = r;` |
|        - | 1539 | `					MemObjSetType(pObj1,MEMOBJ_INT);` |
|        - | 1540 | `#endif` |
|        5 | 1541 | `				}else{` |
|     6803 | 1542 | `					pObj1->x.iVal = r;` |
|     6803 | 1543 | `					MemObjSetType(pObj1,MEMOBJ_INT);` |
|        - | 1544 | `				}` |
|        - | 1545 | `			}` |
|     3441 | 1546 | `	}else{` |
|     3859 | 1547 | `		if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP ){` |
|        - | 1548 | `			ph7_hashmap *pMap;` |
|        - | 1549 | `			sxi32 rc;` |
|     3859 | 1550 | `			if( bAddStore ){` |
|        - | 1551 | `				/* Do not duplicate the hashmap,use the left one since its an add&store operation.` |
|        - | 1552 | `				 */` |
|        3 | 1553 | `				if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1554 | `					/* Force a hashmap cast */` |
|      ! 0 | 1555 | `					rc = PH7_MemObjToHashmap(pObj1);` |
|      ! 0 | 1556 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 1557 | `						PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|      ! 0 | 1558 | `						return rc;` |
|        - | 1559 | `					}` |
|      ! 0 | 1560 | `				}` |
|        - | 1561 | `				/* COW separate before in-place mutation */` |
|        3 | 1562 | `				pMap = PH7_HashmapCowSeparate(pObj1->pVm,pObj1);` |
|        2 | 1563 | `			}else{` |
|        - | 1564 | `				/* Create a new hashmap */` |
|     3857 | 1565 | `				pMap = PH7_NewHashmap(pObj1->pVm,0,0);` |
|     3857 | 1566 | `				if( pMap == 0){` |
|      ! 0 | 1567 | `					PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|      ! 0 | 1568 | `					return SXERR_MEM;` |
|        - | 1569 | `				}` |
|        - | 1570 | `			}` |
|     3859 | 1571 | `			if( !bAddStore ){` |
|     3857 | 1572 | `				if(pObj1->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1573 | `					/* Perform a hashmap duplication */` |
|     3857 | 1574 | `					PH7_HashmapDup((ph7_hashmap *)pObj1->x.pOther,pMap);` |
|     1931 | 1575 | `				}else{` |
|      ! 0 | 1576 | `					if((pObj1->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 1577 | `						/* Simple insertion */` |
|      ! 0 | 1578 | `						PH7_HashmapInsert(pMap,0,pObj1);` |
|      ! 0 | 1579 | `					}` |
|        - | 1580 | `				}` |
|     1926 | 1581 | `			}` |
|        - | 1582 | `			/* Perform the union */` |
|     3859 | 1583 | `			if(pObj2->iFlags & MEMOBJ_HASHMAP ){` |
|     3859 | 1584 | `				PH7_HashmapUnion(pMap,(ph7_hashmap *)pObj2->x.pOther);` |
|     1932 | 1585 | `			}else{` |
|      ! 0 | 1586 | `				if((pObj2->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 1587 | `					/* Simple insertion */` |
|      ! 0 | 1588 | `					PH7_HashmapInsert(pMap,0,pObj2);` |
|      ! 0 | 1589 | `				}` |
|        - | 1590 | `			}` |
|        - | 1591 | `			/* Reflect the change */` |
|     3859 | 1592 | `			if( pObj1->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 1593 | `				SyBlobRelease(&pObj1->sBlob);` |
|      ! 0 | 1594 | `			}` |
|     3859 | 1595 | `			pObj1->x.pOther = pMap;` |
|     3859 | 1596 | `			MemObjSetType(pObj1,MEMOBJ_HASHMAP);` |
|     1927 | 1597 | `		}` |
|        - | 1598 | `	}` |
|    10731 | 1599 | `	return SXRET_OK;` |
|     5368 | 1600 | `}` |
|        - | 1601 | `/*` |
|        - | 1602 | ` * Return a printable representation of the type of a given` |
|        - | 1603 | ` * ph7_value.` |
|        - | 1604 | ` */` |
|      466 | 1605 | `PH7_PRIVATE const char * PH7_MemObjTypeDump(ph7_value *pVal)` |
|        5 | 1606 | `{` |
|      471 | 1607 | `	const char *zType = "";` |
|      471 | 1608 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|        5 | 1609 | `		zType = "null";` |
|      469 | 1610 | `	}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|        - | 1611 | `		/* REAL is authoritative over a cached MEMOBJ_INT: an integer-valued` |
|        - | 1612 | `		 * real (e.g. 1.0) is reported as "double", matching PHP's gettype(). */` |
|        7 | 1613 | `		zType = "double";` |
|      464 | 1614 | `	}else if( pVal->iFlags & MEMOBJ_INT ){` |
|      127 | 1615 | `		zType = "int";` |
|      399 | 1616 | `	}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|       77 | 1617 | `		zType = "string";` |
|      301 | 1618 | `	}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      109 | 1619 | `		zType = "bool";` |
|      211 | 1620 | `	}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|       16 | 1621 | `		zType = "array";` |
|      150 | 1622 | `	}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      143 | 1623 | `		zType = "object";` |
|       70 | 1624 | `	}else if( pVal->iFlags & MEMOBJ_RES ){` |
|      ! 0 | 1625 | `		zType = "resource";` |
|      ! 0 | 1626 | `	}` |
|      471 | 1627 | `	return zType;` |
|        5 | 1628 | `}` |
|        - | 1629 | `/*` |
|        - | 1630 | ` * Dump a ph7_value [i.e: get a printable representation of it's type and contents.].` |
|        - | 1631 | ` * Store the dump in the given blob.` |
|        - | 1632 | ` */` |
|      478 | 1633 | `PH7_PRIVATE sxi32 PH7_MemObjDump(` |
|        - | 1634 | `	SyBlob *pOut,      /* Store the dump here */` |
|        - | 1635 | `	ph7_value *pObj,   /* Dump this */` |
|        - | 1636 | `	int ShowType,      /* TRUE to output value type */` |
|        - | 1637 | `	int nTab,          /* # of Whitespace to insert */` |
|        - | 1638 | `	int nDepth,        /* Nesting level */` |
|        - | 1639 | `	int isRef          /* TRUE if referenced object */` |
|        - | 1640 | `	)` |
|        5 | 1641 | `{` |
|      483 | 1642 | `	sxi32 rc = SXRET_OK;` |
|        - | 1643 | `	const char *zType;` |
|        - | 1644 | `	int i;` |
|     4599 | 1645 | `	for( i = 0 ; i < nTab ; i++ ){` |
|     4120 | 1646 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     2062 | 1647 | `	}` |
|      483 | 1648 | `	if( ShowType && (pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|        - | 1649 | ``		/* php 8.1: var_dump of an enum case prints `enum(S::A)` — no body */`` |
|      137 | 1650 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|      137 | 1651 | `		if( pInst->pClass->iFlags & PH7_CLASS_ENUM ){` |
|      ! 0 | 1652 | `			ph7_value *pName = PH7_EnumCaseNameValue(pInst);` |
|      ! 0 | 1653 | `			if( isRef ){` |
|      ! 0 | 1654 | `				SyBlobAppend(&(*pOut),"&",sizeof(char));` |
|      ! 0 | 1655 | `			}` |
|      ! 0 | 1656 | `			SyBlobFormat(&(*pOut),"enum(%z::",&pInst->pClass->sName);` |
|      ! 0 | 1657 | `			if( pName && SyBlobLength(&pName->sBlob) > 0 ){` |
|      ! 0 | 1658 | `				SyBlobAppend(&(*pOut),SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));` |
|      ! 0 | 1659 | `			}` |
|      ! 0 | 1660 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|        - | 1661 | `#ifdef __WINNT__` |
|      ! 0 | 1662 | `			SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|        - | 1663 | `#else` |
|      ! 0 | 1664 | `			SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        - | 1665 | `#endif` |
|      ! 0 | 1666 | `			return SXRET_OK;` |
|        - | 1667 | `		}` |
|       67 | 1668 | `	}` |
|      483 | 1669 | `	if( ShowType ){` |
|      435 | 1670 | `		if( isRef ){` |
|      ! 0 | 1671 | `			SyBlobAppend(&(*pOut),"&",sizeof(char));` |
|      ! 0 | 1672 | `		}` |
|        - | 1673 | `		/* Get value type first. var_dump() labels reals "float" (PHP), whereas` |
|        - | 1674 | `		 * gettype()/PH7_MemObjTypeDump use the legacy "double" spelling. */` |
|      435 | 1675 | `		if( (pObj->iFlags & MEMOBJ_REAL) && (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        5 | 1676 | `			zType = "float";` |
|        3 | 1677 | `		}else{` |
|      431 | 1678 | `			zType = PH7_MemObjTypeDump(pObj);` |
|        - | 1679 | `		}` |
|      435 | 1680 | `		SyBlobAppend(&(*pOut),zType,SyStrlen(zType));` |
|      215 | 1681 | `	}` |
|      483 | 1682 | `	if((pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|      481 | 1683 | `		if ( ShowType ){` |
|      433 | 1684 | `			SyBlobAppend(&(*pOut),"(",sizeof(char));` |
|      214 | 1685 | `		}` |
|      481 | 1686 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1687 | `			/* Dump hashmap entries */` |
|       24 | 1688 | `			rc = PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,ShowType,nTab+1,nDepth+1);` |
|      470 | 1689 | `		}else if(pObj->iFlags & MEMOBJ_OBJ ){` |
|        - | 1690 | `			/* Dump class instance attributes */` |
|      141 | 1691 | `			rc = PH7_ClassInstanceDump(&(*pOut),(ph7_class_instance *)pObj->x.pOther,ShowType,nTab+1,nDepth+1);` |
|       72 | 1692 | `		}else{` |
|      321 | 1693 | `			SyBlob *pContents = &pObj->sBlob;` |
|        - | 1694 | `			/* Get a printable representation of the contents */` |
|      321 | 1695 | `			if((pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|      252 | 1696 | `				MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|      128 | 1697 | `			}else{` |
|        - | 1698 | `				/* PHP format: string(N) "content" */` |
|       72 | 1699 | `				if( ShowType ){` |
|       54 | 1700 | `					SyBlobFormat(&(*pOut),"%u) \"",SyBlobLength(&pObj->sBlob));` |
|       25 | 1701 | `				}` |
|       72 | 1702 | `				if( SyBlobLength(pContents) > 0 ){` |
|       70 | 1703 | `					SyBlobAppend(&(*pOut),SyBlobData(pContents),SyBlobLength(pContents));` |
|       33 | 1704 | `				}` |
|       72 | 1705 | `				if( ShowType ){` |
|       54 | 1706 | `					SyBlobAppend(&(*pOut),"\"",sizeof(char));` |
|       25 | 1707 | `				}` |
|        - | 1708 | `			}` |
|        - | 1709 | `		}` |
|      481 | 1710 | `		if( ShowType ){` |
|        - | 1711 | `			/* Strings already emitted their own ')' as part of the` |
|        - | 1712 | `			 * "N) \"content\"" format above. */` |
|      433 | 1713 | `			if( (pObj->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_STRING)) == 0 ){` |
|      236 | 1714 | `				SyBlobAppend(&(*pOut),")",sizeof(char));` |
|      116 | 1715 | `			}` |
|      214 | 1716 | `		}` |
|      238 | 1717 | `	}` |
|        - | 1718 | `#ifdef __WINNT__` |
|        5 | 1719 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|        - | 1720 | `#else` |
|      478 | 1721 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        - | 1722 | `#endif` |
|      483 | 1723 | `	return rc;` |
|      244 | 1724 | `}` |
|        - | 1725 |  |
